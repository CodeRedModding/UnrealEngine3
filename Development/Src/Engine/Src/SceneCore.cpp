/*=============================================================================
	SceneCore.cpp: Core scene implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "AllocatorFixedSizeFreeList.h"
#include "LightRendering.h"
#include "UnTerrain.h"
#include "EngineFogVolumeClasses.h"

/**
 * Fixed Size pool allocator for FLightPrimitiveInteractions
 */
#define FREE_LIST_GROW_SIZE ( 16384 / sizeof(FLightPrimitiveInteraction) )
TAllocatorFixedSizeFreeList<sizeof(FLightPrimitiveInteraction), FREE_LIST_GROW_SIZE> GLightPrimitiveInteractionAllocator;

/*-----------------------------------------------------------------------------
	FLightPrimitiveInteraction
-----------------------------------------------------------------------------*/

/**
 * Custom new
 */
void* FLightPrimitiveInteraction::operator new(size_t Size)
{
	// doesn't support derived classes with a different size
	checkSlow(Size == sizeof(FLightPrimitiveInteraction));
	return GLightPrimitiveInteractionAllocator.Allocate();
	//return appMalloc(Size);
}

/**
 * Custom delete
 */
void FLightPrimitiveInteraction::operator delete(void *RawMemory)
{
	GLightPrimitiveInteractionAllocator.Free(RawMemory);
	//appFree(RawMemory);
}	

/**
 * Initialize the memory pool with a default size from the ini file.
 * Called at render thread startup. Since the render thread is potentially
 * created/destroyed multiple times, must make sure we only do it once.
 */
void FLightPrimitiveInteraction::InitializeMemoryPool()
{
	static UBOOL bAlreadyInitialized = FALSE;
	if (!bAlreadyInitialized)
	{
		bAlreadyInitialized = TRUE;
		INT InitialBlockSize = 0;
		GConfig->GetInt(TEXT("MemoryPools"), TEXT("FLightPrimitiveInteractionInitialBlockSize"), InitialBlockSize, GEngineIni);
		GLightPrimitiveInteractionAllocator.Grow(InitialBlockSize);
	}
}

/**
* Returns current size of memory pool
*/
DWORD FLightPrimitiveInteraction::GetMemoryPoolSize()
{
	return GLightPrimitiveInteractionAllocator.GetAllocatedSize();
}

void FLightPrimitiveInteraction::Create(FLightSceneInfo* LightSceneInfo,FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	// Attach the light to the primitive's static meshes.
	UBOOL bDynamic = TRUE;
	UBOOL bRelevant = FALSE;
	UBOOL bLightMapped = TRUE;

	// Determine the light's relevance to the primitive.
	check(PrimitiveSceneInfo->Proxy);
	PrimitiveSceneInfo->Proxy->GetLightRelevance(LightSceneInfo, bDynamic, bRelevant, bLightMapped);

	// Special case handling for movable actors with precomputed lighting interacting with static lights that are active
	// during gameplay, like e.g. toggleable ones. The intention for precomputing lighting on moving objects is that
	// other 'static' lights won't affect them so we remove those interactions.
	//
	// Only look at interactions considered dynamic and relative.
	if( bDynamic 
	&&	bRelevant
	// Primitive needs to support static shadowing, be movable and the light needs to have static shadowing
	&&	PrimitiveSceneInfo->bStaticShadowing 
	&&	PrimitiveSceneInfo->Proxy->IsMovable()
	&&	LightSceneInfo->bStaticShadowing )
	{
		// If all that is the case, don't create the interaction.
		bRelevant = FALSE;
	}

	if( bRelevant )
	{
		// Create the light interaction.
		FLightPrimitiveInteraction* Interaction = new FLightPrimitiveInteraction(LightSceneInfo,PrimitiveSceneInfo,bDynamic,bLightMapped);

#if !CONSOLE
		if (GIsEditor)
		{
			// Treat the light as completely unbuilt if it has more unbuilt interactions than the threshold.
			// This will result in the light using whole scene shadows instead of many per-object shadows, 
			// Which prevents poor performance when many per-object shadows are created for previewing unbuilt lighting.
			if (LightSceneInfo->NumUnbuiltInteractions >= GSystemSettings.WholeSceneShadowUnbuiltInteractionThreshold)
			{
				LightSceneInfo->bPrecomputedLightingIsValid = FALSE;
			}
		}

		if (!PrimitiveSceneInfo->Proxy->IsHiddenGame() && IsDominantLightType(LightSceneInfo->LightType))
		{
			if (PrimitiveSceneInfo->NumAffectingDominantLights == 1)
			{
				appInterlockedIncrement(&PrimitiveSceneInfo->Scene->NumMultipleDominantLightInteractions);
			}
			PrimitiveSceneInfo->NumAffectingDominantLights++;
		}
#endif

		// Attach the light to the primitive.
		LightSceneInfo->AttachPrimitive(*Interaction);

		if (Interaction->ShouldAddStaticMeshesToLightingDrawLists())
		{
			// Attach the light to the primitive's static meshes.
			for(INT ElementIndex = 0;ElementIndex < PrimitiveSceneInfo->StaticMeshes.Num();ElementIndex++)
			{
				FMeshLightingDrawingPolicyFactory::AddStaticMesh(
					PrimitiveSceneInfo->Scene,
					&PrimitiveSceneInfo->StaticMeshes(ElementIndex),
					LightSceneInfo
					);
			}

			// Also handle static meshes for decal interactions
			for( INT DecalIdx = 0; DecalIdx < PrimitiveSceneInfo->Proxy->Decals[FPrimitiveSceneProxy::STATIC_DECALS].Num(); DecalIdx++ )
			{
				FDecalInteraction* Decal = PrimitiveSceneInfo->Proxy->Decals[FPrimitiveSceneProxy::STATIC_DECALS](DecalIdx);
				if( Decal )
				{
					FMeshLightingDrawingPolicyFactory::AddStaticMesh(
						PrimitiveSceneInfo->Scene,
						Decal->DecalStaticMesh,
						LightSceneInfo
						);				
				}
			}
		}
	}

	// Add the mesh to the shadow depth draw list if any whole scene shadows are active,
	// Or if we are in the editor, because a whole scene shadow may be activated past this point for previewing unbuilt lighting.
	// Moved outside of the bRelevant check to match the functionality of AddToDrawLists, which disregards light relevance for whole scene shadow lights
	if ((PrimitiveSceneInfo->Scene->NumWholeSceneShadowLights > 0 || GIsEditor) && (IsDominantLightType(LightSceneInfo->LightType)))
	{
		for(INT ElementIndex = 0;ElementIndex < PrimitiveSceneInfo->StaticMeshes.Num();ElementIndex++)
		{
			FStaticMesh* Mesh = &PrimitiveSceneInfo->StaticMeshes(ElementIndex);
			// Add each mesh to the shadow depth draw list if they aren't already in it
			if (!Mesh->IsLinkedToDrawList(&PrimitiveSceneInfo->Scene->DPGs[SDPG_World].WholeSceneShadowDepthDrawList))
			{
				FShadowDepthDrawingPolicyFactory::AddStaticMesh(PrimitiveSceneInfo->Scene,Mesh);
			}
		}
	}
}

void FLightPrimitiveInteraction::Destroy(FLightPrimitiveInteraction* LightPrimitiveInteraction)
{
	delete LightPrimitiveInteraction;
}

/** Returns TRUE if the primitive's static meshes should be added to the light's static draw lists. */
UBOOL FLightPrimitiveInteraction::ShouldAddStaticMeshesToLightingDrawLists() const
{
	const UBOOL bIsDominantLight = IsDominantLightType(LightSceneInfo->LightType);
	return bIsDominantLight 
		// Don't add the primitive to the light's static draw lists if the dominant light will be applied in the base pass
		&& !GOnePassDominantLight
		// On PC, only render the brightest dominant light affecting a primitive to be consistent with consoles
		&& (!PrimitiveSceneInfo->BrightestDominantLightSceneInfo || !bIsDominantLight || PrimitiveSceneInfo->BrightestDominantLightSceneInfo == LightSceneInfo)
		// Don't render a separate pass for non-dominant lights that will be merged into the base pass
		// This happens if a light environment is applied to a static mesh, because the DLE directional light is set as DynamicLightSceneInfo
		|| !bIsDominantLight && PrimitiveSceneInfo->DynamicLightSceneInfo != LightSceneInfo;
}

#if !FINAL_RELEASE
static TArray<FString> UnbuiltInteractionsLightLog;
static TArray<FString> UnbuiltInteractionsPrimitiveLog;
void ListUncachedStaticLightingInteractions(FOutputDevice& Ar)
{
	for( BYTE Index = 0; Index < UnbuiltInteractionsLightLog.Num(); Index++ )
	{
		Ar.Logf( TEXT("Uncached static lighting interaction detected") );
		Ar.Logf( *(FString(TEXT("    Light: ")) + UnbuiltInteractionsLightLog(Index)) );
		Ar.Logf( *(FString(TEXT("    Primitive: ")) + UnbuiltInteractionsPrimitiveLog(Index)) );
	}
	Ar.Logf( TEXT("A total of %d uncached static lighting interactions were detected"), UnbuiltInteractionsLightLog.Num() );
}
#endif

FLightPrimitiveInteraction::FLightPrimitiveInteraction(
	FLightSceneInfo* InLightSceneInfo,
	FPrimitiveSceneInfo* InPrimitiveSceneInfo,
	UBOOL bInIsDynamic,
	UBOOL bInLightMapped
	):
	ModShadowStartFadeOutPercent(1.0f),
	// Disable the fade in, as there is currently a one frame pop and moving the shadow between DPG's is not handled.
	ModShadowStartFadeInPercent(1.0f),
	LightId(InLightSceneInfo->Id),
	LightSceneInfo(InLightSceneInfo),
	PrimitiveSceneInfo(InPrimitiveSceneInfo),
	bLightMapped(bInLightMapped),
	bIsDynamic(bInIsDynamic),
	bUncachedStaticLighting(FALSE),
	bNeedsLightRenderingPass(FALSE)
{
	// Determine whether this light-primitive interaction produces a shadow.
	if(PrimitiveSceneInfo->bStaticShadowing)
	{
		const UBOOL bHasStaticShadow =
			LightSceneInfo->bStaticShadowing &&
			LightSceneInfo->bCastStaticShadow &&
			PrimitiveSceneInfo->bCastStaticShadow;
		const UBOOL bHasDynamicShadow =
			!LightSceneInfo->bStaticShadowing &&
			LightSceneInfo->bCastDynamicShadow &&
			PrimitiveSceneInfo->bCastDynamicShadow;
		bCastShadow = bHasStaticShadow || bHasDynamicShadow;
	}
	else
	{
		if(PrimitiveSceneInfo->LightEnvironment != NULL 
			&& LightSceneInfo->LightEnvironment == NULL 
			&& LightSceneInfo->bCastCompositeShadow
			&& !IsDominantLightType(LightSceneInfo->LightType)
			)
		{
			// If the light will affect the primitive's light environment's composite shadow, only use composite shadow casting, and don't cast a separate shadow for this light.
			bCastShadow = FALSE;
		}
		else
		{
			bCastShadow = LightSceneInfo->bCastDynamicShadow && PrimitiveSceneInfo->bCastDynamicShadow;
		}
	}

#if !FINAL_RELEASE
	if(bCastShadow && bIsDynamic)
	{
		// Determine the type of dynamic shadow produced by this light.
		if(PrimitiveSceneInfo->bStaticShadowing)
		{
			if(LightSceneInfo->bStaticShadowing && PrimitiveSceneInfo->bCastStaticShadow)
			{
				UBOOL bMarkAsBad = TRUE;
				if (GIsEditor)
				{
					UTerrainComponent* TerrainComponent = Cast<UTerrainComponent>(PrimitiveSceneInfo->Component);
					if (TerrainComponent)
					{
						if (TerrainComponent->GetTriangleCount() == 0)
						{
							bMarkAsBad = FALSE;
						}
					}
				}

				if (bMarkAsBad == TRUE)
				{
					// Update the game thread's counter of number of uncached static lighting interactions.
					bUncachedStaticLighting = TRUE;
					LightSceneInfo->NumUnbuiltInteractions++;

					// Reset the tracking log each time the NumUncachedStaticLightingInteractions is reset
					if( PrimitiveSceneInfo->Scene->NumUncachedStaticLightingInteractions == 0 )
					{
						UnbuiltInteractionsLightLog.Empty();
						UnbuiltInteractionsPrimitiveLog.Empty();
					}
					// Keep track of uncached interactions for logging out later
					UnbuiltInteractionsLightLog.Push( LightSceneInfo->LightComponentName.ToString() );
					UnbuiltInteractionsPrimitiveLog.Push( PrimitiveSceneInfo->Component->GetFullName() );

					appInterlockedIncrement(&PrimitiveSceneInfo->Scene->NumUncachedStaticLightingInteractions);
					if( GIsGame )
					{
						// Disable shadow casting for unbuilt lighting in the game. We'd rather be fast and incorrect in this case.
						bCastShadow = FALSE;
						// debugf(TEXT("Lighting marked dirty due to component 0x%08x (%s)"), (DWORD)(PrimitiveSceneInfo->Component), *(PrimitiveSceneInfo->Component->GetPathName()));
					}
				}
			}
		}
	}
#endif

#if USE_MASSIVE_LOD

	// initialize it
	ParentPrimitive = NULL;

	// add any orphaned (pending) children of our component to us (reading right into our ChildInteractions array)
	LightSceneInfo->OrphanedPrimitiveMap.MultiFind(PrimitiveSceneInfo->Component, ChildInteractions);
	LightSceneInfo->OrphanedPrimitiveMap.RemoveKey(PrimitiveSceneInfo->Component);

	// this is an LOD parent if it's static and has any dynamic (or bIsLODParentOnly children)
	bIsLODParentOnly = FALSE;

	// loop over the children, looking for dynamic children of statics, as well as fixing up parent pointer, 
	for (INT ChildIndex = 0; ChildIndex < ChildInteractions.Num(); ChildIndex++)
	{
		FLightPrimitiveInteraction* Child = ChildInteractions(ChildIndex);
		
		// a dynamic child, or a static child with dynamic children, means this one must be put in the dynamic list
		if (!bIsDynamic && (Child->bIsDynamic || Child->bIsLODParentOnly))
		{
			bIsLODParentOnly = TRUE;
		}

		// we need the child to know about the parent so it can remove itself
		Child->ParentPrimitive = this;
		Child->PrevPrimitiveLink = NULL;
	}

	// figure out where to put this (under a parent or right into the light scene info)
	// @todo since only dynamic lights are used at runtime, only dynamic lights get parented
	// the rest go into the static list as normal (linearly)
	if (bIsDynamic && PrimitiveSceneInfo->ReplacementPrimitiveMapKey)
	{
		// keep track of our ancestors
		FLightPrimitiveInteraction* ParentInteraction = NULL;
		FLightPrimitiveInteraction* ParentmostInteraction = NULL;

		// find the primitive's parent scene info, since it has a list of it's lights
		FPathToCompact* PathToParent = FPrimitiveSceneInfo::PrimitiveToCompactMap.Find(PrimitiveSceneInfo->ReplacementPrimitiveMapKey);
		if (PathToParent)
		{
			// if it does exist, add us as a child
			FPrimitiveSceneInfoCompact& CompactParent = PathToParent->GetCompact(PrimitiveSceneInfo->Scene->PrimitiveOctree);
			FPrimitiveSceneInfo* ParentSceneInfo = CompactParent.PrimitiveSceneInfo;

			// look for this light's interaction with our parent, we can then just add us to it
			for (FLightPrimitiveInteraction* LightList = CompactParent.PrimitiveSceneInfo->LightList; LightList; LightList = LightList->GetNextLight())
			{
				// do the light's match?
				if (LightList->LightId == LightId)
				{
					ParentInteraction = LightList;

					// now go up the hierarchy to get the parentmost
					ParentmostInteraction = ParentInteraction;
					while (ParentmostInteraction->GetParentPrimitive())
					{
						// go up to the parent
						ParentmostInteraction = ParentmostInteraction->GetParentPrimitive();
					}

					break;
				}
			}
		}	

		// did we find a parent?
		if (ParentInteraction)
		{
			// if the parent is static, then we need to move it to the dynamic list and mark it as a fake-dynamic
			if (!ParentmostInteraction->bIsDynamic && !ParentmostInteraction->bIsLODParentOnly)
			{
				// pull the parentMOST interaction out of the static list
				if(ParentmostInteraction->NextPrimitive)
				{
					ParentmostInteraction->NextPrimitive->PrevPrimitiveLink = ParentmostInteraction->PrevPrimitiveLink;
				}
				*ParentmostInteraction->PrevPrimitiveLink = ParentmostInteraction->NextPrimitive;

				// add it to the dynamic list, 
				ParentmostInteraction->PrevPrimitiveLink = &LightSceneInfo->DynamicPrimitiveList;
				ParentmostInteraction->NextPrimitive = *ParentmostInteraction->PrevPrimitiveLink;
				if (*ParentmostInteraction->PrevPrimitiveLink)
				{
					(*ParentmostInteraction->PrevPrimitiveLink)->PrevPrimitiveLink = &ParentmostInteraction->NextPrimitive;
				}
				*ParentmostInteraction->PrevPrimitiveLink = ParentmostInteraction;

				// but mark it as parent only, don't ever use it as a dynamic interaction
				ParentmostInteraction->bIsLODParentOnly = TRUE;
			}

			// if so, add us as a child instead of to the light scene info
			ParentInteraction->ChildInteractions.AddItem(this);

			// we need to know about the parent so it can remove itself
			ParentPrimitive = ParentInteraction;

			PrevPrimitiveLink = NULL;			
		}
		else
		{
			// add it to the orphaned map, waiting for someone to rescue us! (it will be our parent - the ReplacementPrimitiveMapKey)
			LightSceneInfo->OrphanedPrimitiveMap.Add(PrimitiveSceneInfo->ReplacementPrimitiveMapKey, this);

			PrevPrimitiveLink = NULL;
		}
	}
	else
#endif
	{
		// Add the interaction to the light's interaction list.
		if(bIsDynamic
#if USE_MASSIVE_LOD
			|| bIsLODParentOnly
#endif
			)
		{
			PrevPrimitiveLink = &LightSceneInfo->DynamicPrimitiveList;
		}
		else
		{
			PrevPrimitiveLink = &LightSceneInfo->StaticPrimitiveList;
		}
		NextPrimitive = *PrevPrimitiveLink;
		if(*PrevPrimitiveLink)
		{
			(*PrevPrimitiveLink)->PrevPrimitiveLink = &NextPrimitive;
		}
		*PrevPrimitiveLink = this;
	}

	// Add the interaction to the primitive's interaction list.
	PrevLightLink = &PrimitiveSceneInfo->LightList;
	NextLight = *PrevLightLink;
	if(*PrevLightLink)
	{
		(*PrevLightLink)->PrevLightLink = &NextLight;
	}
	*PrevLightLink = this;
}

FLightPrimitiveInteraction::~FLightPrimitiveInteraction()
{
	check(IsInRenderingThread());

#if !FINAL_RELEASE
	// Update the game thread's counter of number of uncached static lighting interactions.
	if(bUncachedStaticLighting)
	{
		LightSceneInfo->NumUnbuiltInteractions--;
		appInterlockedDecrement(&PrimitiveSceneInfo->Scene->NumUncachedStaticLightingInteractions);
	}
#endif

#if !CONSOLE
	if (!PrimitiveSceneInfo->Proxy->IsHiddenGame() && IsDominantLightType(LightSceneInfo->LightType))
	{
		checkSlow(PrimitiveSceneInfo->NumAffectingDominantLights > 0);
		if (PrimitiveSceneInfo->NumAffectingDominantLights == 2)
		{
			appInterlockedDecrement(&PrimitiveSceneInfo->Scene->NumMultipleDominantLightInteractions);
		}
		PrimitiveSceneInfo->NumAffectingDominantLights--;
	}
#endif

	// Detach the light from the primitive.
	LightSceneInfo->DetachPrimitive(*this);

#if USE_MASSIVE_LOD
	// toss all my children
	for (INT ChildIndex = 0; ChildIndex < ChildInteractions.Num(); ChildIndex++)
	{
		// make sure the children don't try to remove themselves from me
		ChildInteractions(ChildIndex)->ParentPrimitive = NULL;

		// hold any of our children in the orphaned (when parent is reattached, it will pull them out). my component is the 
		// children's key to lookup
		LightSceneInfo->OrphanedPrimitiveMap.Add(PrimitiveSceneInfo->Component, ChildInteractions(ChildIndex));
	}

	// if this is a child light info in the dynamic list, then we need to remove ourself from the parent, not from the linked list
	// we check PrevPrimitiveLink because if that is set, that means it was put directly into a linked list, even tho
	// it had a parent (which can happen with statics)
	if (PrimitiveSceneInfo->ReplacementPrimitiveMapKey && PrevPrimitiveLink == NULL)
	{
		// make sure it has a parent to remove from (otherwise, we should be in the orphaned map)
		if (ParentPrimitive)
		{
			// in this child case, the ParentPrimitive points to it's parent
			ParentPrimitive->ChildInteractions.RemoveSingleItem(this);
		}
		else
		{
			// the ReplacementPrimitiveMapKey is the key we were put in the orphaned map with
			LightSceneInfo->OrphanedPrimitiveMap.RemovePair(PrimitiveSceneInfo->ReplacementPrimitiveMapKey, this);
		}
	}
	else
#endif
	{
		// Remove the interaction from the light's interaction list.
		if(NextPrimitive)
		{
			NextPrimitive->PrevPrimitiveLink = PrevPrimitiveLink;
		}
		*PrevPrimitiveLink = NextPrimitive;
	}

	// Remove the interaction from the primitive's interaction list.
	if(NextLight)
	{
		NextLight->PrevLightLink = PrevLightLink;
	}
	*PrevLightLink = NextLight;
}

/*-----------------------------------------------------------------------------
	FCaptureSceneInfo
-----------------------------------------------------------------------------*/

/** 
* Constructor 
* @param InComponent - mirrored scene capture component requesting the capture
* @param InSceneCaptureProbe - new probe for capturing the scene
*/
FCaptureSceneInfo::FCaptureSceneInfo(USceneCaptureComponent* InComponent,FSceneCaptureProbe* InSceneCaptureProbe)
:	SceneCaptureProbe(InSceneCaptureProbe)
,	Component(InComponent)
,	RenderThreadId(INDEX_NONE)
,	GameThreadId(INDEX_NONE)
,	OwnerName(InComponent->GetOwner() ? InComponent->GetOwner()->GetFName() : InComponent->GetFName())
,	Scene(NULL)
{
	check(Component);
	check(SceneCaptureProbe);
    check(InComponent->CaptureInfo == NULL);
	InComponent->CaptureInfo = this;	
}

/** 
* Destructor
*/
FCaptureSceneInfo::~FCaptureSceneInfo()
{
	RemoveFromScene(Scene);
	delete SceneCaptureProbe;
}

/**
* Capture the scene
* @param SceneRenderer - original scene renderer so that we can match certain view settings
*/
void FCaptureSceneInfo::CaptureScene(class FSceneRenderer* SceneRenderer)
{
	if( SceneCaptureProbe )
	{
        SceneCaptureProbe->CaptureScene(SceneRenderer);
	}
}

/**
* Add this capture scene info to a scene 
* @param InScene - scene to add to
*/
void FCaptureSceneInfo::AddToScene(class FScene* InScene)
{
	check(InScene);

	// can only be active in a single scene
	RemoveFromScene(Scene);
	
	// add it to the scene and keep track of Id
	Scene = InScene;
	RenderThreadId = Scene->SceneCapturesRenderThread.AddItem(this);

}

/**
* Remove this capture scene info from a scene 
* @param InScene - scene to remove from
*/
void FCaptureSceneInfo::RemoveFromScene(class FScene* /*InScene*/)
{
	if( Scene &&
		RenderThreadId != INDEX_NONE )
	{
		Scene->SceneCapturesRenderThread.Remove(RenderThreadId);
		Scene = NULL;
	}
}

/*-----------------------------------------------------------------------------
	FStaticMesh
-----------------------------------------------------------------------------*/

void FStaticMesh::LinkDrawList(FStaticMesh::FDrawListElementLink* Link)
{
	check(IsInRenderingThread());
	check(!DrawListLinks.ContainsItem(Link));
	DrawListLinks.AddItem(Link);
}

void FStaticMesh::UnlinkDrawList(FStaticMesh::FDrawListElementLink* Link)
{
	check(IsInRenderingThread());
	verify(DrawListLinks.RemoveSingleItemSwap(Link) == 1);
}

void FStaticMesh::AddToDrawLists(FScene* Scene)
{
	if ( !GIsRHIInitialized )
	{
		return;
	}

	if( IsDecal() )
	{
		// Add the decal static mesh to the DPG's base pass draw list.
		FBasePassOpaqueDrawingPolicyFactory::AddStaticMesh(Scene,this);

		// Add the decal static mesh to the light draw lists for the primitive's light interactions.
		for(FLightPrimitiveInteraction* LightInteraction = PrimitiveSceneInfo->LightList;LightInteraction;LightInteraction = LightInteraction->GetNextLight())
		{
			if (LightInteraction->ShouldAddStaticMeshesToLightingDrawLists())
			{
				// separate draw lists are maintained for decals
				FMeshLightingDrawingPolicyFactory::AddStaticMesh(
					Scene,
					this,
					LightInteraction->GetLight()
					);
			}
		}
	}
	else
	{
		// not all platforms need this
		const UBOOL bRequiresHitProxies = Scene->RequiresHitProxies();
		if ( bRequiresHitProxies && PrimitiveSceneInfo->bSelectable )
		{
			// Add the static mesh to the DPG's hit proxy draw list.
			FHitProxyDrawingPolicyFactory::AddStaticMesh(Scene,this);
		}

		if(!IsTranslucent())
		{
			if(DepthPriorityGroup == SDPG_World)
			{
				// Render non-masked materials in the depth only pass, unless we are doing a one pass shadowed directional light,
				// In which case scene depth needs to be up to date after the depth only pass.
				if (PrimitiveSceneInfo->bUseAsOccluder && (GOnePassDominantLight || !IsMasked()))
				{
					FDepthDrawingPolicyFactory::AddStaticMesh(Scene,this);
				}

				if ( !PrimitiveSceneInfo->bStaticShadowing )
				{
					FVelocityDrawingPolicyFactory::AddStaticMesh(Scene,this);
				}

				// Add the mesh to the shadow depth draw list if any whole scene shadows are active,
				// Or if we are in the editor, because a whole scene shadow may be activated past this point for previewing unbuilt lighting.
				if ((Scene->NumWholeSceneShadowLights > 0 || GIsEditor) && !IsLinkedToDrawList(&Scene->DPGs[SDPG_World].WholeSceneShadowDepthDrawList))
				{
					FShadowDepthDrawingPolicyFactory::AddStaticMesh(Scene,this);
				}
			}

			// Add the static mesh to the DPG's base pass draw list.
			FBasePassOpaqueDrawingPolicyFactory::AddStaticMesh(Scene,this);
		}

		// Add the static mesh to the light draw lists for the primitive's light interactions.
		for(FLightPrimitiveInteraction* LightInteraction = PrimitiveSceneInfo->LightList;LightInteraction;LightInteraction = LightInteraction->GetNextLight())
		{
			if (LightInteraction->ShouldAddStaticMeshesToLightingDrawLists())
			{
				FMeshLightingDrawingPolicyFactory::AddStaticMesh(
					Scene,
					this,
					LightInteraction->GetLight()
					);
			}
		}
	}
}

void FStaticMesh::RemoveFromDrawLists()
{
	// Remove the mesh from all draw lists.
	while(DrawListLinks.Num())
	{
		FStaticMesh::FDrawListElementLink* Link = DrawListLinks(0);
		const INT OriginalNumLinks = DrawListLinks.Num();
		// This will call UnlinkDrawList.
		Link->Remove();
		check(DrawListLinks.Num() == OriginalNumLinks - 1);
		if(DrawListLinks.Num())
		{
			check(DrawListLinks(0) != Link);
		}
	}
}

/** Returns TRUE if the mesh is linked to the given draw list. */
UBOOL FStaticMesh::IsLinkedToDrawList(const FStaticMeshDrawListBase* DrawList) const
{
	for (INT i = 0; i < DrawListLinks.Num(); i++)
	{
		if (DrawListLinks(i)->IsInDrawList(DrawList))
		{
			return TRUE;
		}
	}
	return FALSE;
}

FStaticMesh::~FStaticMesh()
{
	if( IsDecal() )
	{
		// Remove this decal static mesh from the scene's list.
		PrimitiveSceneInfo->Scene->DecalStaticMeshes.Remove(Id);
	}
	else
	{
		// Remove this static mesh from the scene's list.
		PrimitiveSceneInfo->Scene->StaticMeshes.Remove(Id);
	}
	RemoveFromDrawLists();
}

/** Initialization constructor. */
FHeightFogSceneInfo::FHeightFogSceneInfo(const UHeightFogComponent* InComponent):
	Component(InComponent),
	Height(InComponent->Height),
	Density(InComponent->Density),
	LightColor(FLinearColor(InComponent->LightColor) * InComponent->LightBrightness),
	ExtinctionDistance(InComponent->ExtinctionDistance),
	StartDistance(InComponent->StartDistance)
{
}

/** Initialization constructor. */
FExponentialHeightFogSceneInfo::FExponentialHeightFogSceneInfo(const UExponentialHeightFogComponent* InComponent):
	Component(InComponent),
	FogHeight(InComponent->FogHeight),
	// Scale the densities back down to their real scale
	// Artists edit the densities scaled up so they aren't entering in minuscule floating point numbers
	FogDensity(InComponent->FogDensity / 1000.0f),
	FogHeightFalloff(InComponent->FogHeightFalloff / 1000.0f),
	FogMaxOpacity(InComponent->FogMaxOpacity),
	StartDistance(InComponent->StartDistance),
	LightTerminatorAngle(InComponent->LightTerminatorAngle),
	OppositeLightColor(FLinearColor(InComponent->OppositeLightColor) * InComponent->OppositeLightBrightness),
	LightInscatteringColor(FLinearColor(InComponent->LightInscatteringColor) * InComponent->LightInscatteringBrightness)
{
}
