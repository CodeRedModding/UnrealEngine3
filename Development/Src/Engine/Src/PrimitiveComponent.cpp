/*=============================================================================
	PrimitiveComponent.cpp: Primitive component implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "EngineDecalClasses.h"
#include "EngineFogVolumeClasses.h"
#include "ScenePrivate.h"

IMPLEMENT_CLASS(UPrimitiveComponent);
IMPLEMENT_CLASS(UMeshComponent);
IMPLEMENT_CLASS(UStaticMeshComponent);
IMPLEMENT_CLASS(UCylinderComponent);
IMPLEMENT_CLASS(UArrowComponent);
IMPLEMENT_CLASS(UDrawSphereComponent);
IMPLEMENT_CLASS(UDrawConeComponent);
IMPLEMENT_CLASS(UDrawLightConeComponent);
IMPLEMENT_CLASS(UCameraConeComponent);
IMPLEMENT_CLASS(UDrawQuadComponent);
IMPLEMENT_CLASS(UDrawCylinderComponent);
IMPLEMENT_CLASS(UDrawBoxComponent);
IMPLEMENT_CLASS(UDrawCapsuleComponent);

FPrimitiveSceneProxy::FPrimitiveSceneProxy(const UPrimitiveComponent* InComponent, FName InResourceName)
	: PrimitiveSceneInfo(NULL)
	, ResourceName(InResourceName)
	, bHiddenGame(InComponent->HiddenGame)
	, bHiddenEditor(InComponent->HiddenEditor)
	, bIsNavigationPoint(FALSE)
	, bOnlyOwnerSee(InComponent->bOnlyOwnerSee)
	, bOwnerNoSee(InComponent->bOwnerNoSee)
	, bMovable(FALSE)
	, bSelected(InComponent->ShouldRenderSelected())
	, bHovered(FALSE)
	, bUseViewOwnerDepthPriorityGroup(InComponent->bUseViewOwnerDepthPriorityGroup)
	, bHasMotionBlurVelocityMeshes(InComponent->HasMotionBlurVelocityMeshes())
	, StaticDepthPriorityGroup(InComponent->GetStaticDepthPriorityGroup())
	, ViewOwnerDepthPriorityGroup(InComponent->ViewOwnerDepthPriorityGroup)
	, bRequiresOcclusionForCorrectness(FALSE)
	, MaxDrawDistanceSquared(Square(InComponent->CachedMaxDrawDistance > 0 ? InComponent->CachedMaxDrawDistance : FLT_MAX))
#if !CONSOLE
	// by default we are always drawn
	, HiddenEditorViews(0)
#endif
{
	// If the primitive is in an invalid DPG, move it to the world DPG.
	StaticDepthPriorityGroup = StaticDepthPriorityGroup >= SDPG_MAX_SceneRender ?
		SDPG_World :
		StaticDepthPriorityGroup;
	ViewOwnerDepthPriorityGroup = ViewOwnerDepthPriorityGroup >= SDPG_MAX_SceneRender ?
		SDPG_World :
		ViewOwnerDepthPriorityGroup;

	if(InComponent->GetOwner())
	{
		if(!InComponent->bIgnoreOwnerHidden)
		{
			bHiddenGame |= InComponent->GetOwner()->bHidden;
		}

		bHiddenEditor |= InComponent->GetOwner()->IsHiddenEd();
		bIsNavigationPoint = InComponent->GetOwner()->ShouldBeHiddenBySHOW_NavigationNodes();
		bOnlyOwnerSee |= InComponent->GetOwner()->bOnlyOwnerSee;
		bMovable = !InComponent->GetOwner()->IsStatic() && InComponent->GetOwner()->bMovable;

		if(bOnlyOwnerSee || bOwnerNoSee || bUseViewOwnerDepthPriorityGroup)
		{
			// Make a list of the actors which directly or indirectly own the component.
			for(const AActor* Owner = InComponent->GetOwner();Owner;Owner = Owner->Owner)
			{
				Owners.AddItem(Owner);
			}
		}

#if !CONSOLE
		// cache the actor's group membership
		HiddenEditorViews = InComponent->GetOwner()->HiddenEditorViews;
#endif
	}


	// Copy the primitive's initial decal interactions.
	if( InComponent->bAcceptsStaticDecals || 
		InComponent->bAcceptsDynamicDecals )
	{
		Decals[STATIC_DECALS].Empty();
		Decals[DYNAMIC_DECALS].Empty();
		for(INT DecalIndex = 0;DecalIndex < InComponent->DecalList.Num();DecalIndex++)
		{
			FDecalInteraction* NewInteraction = new FDecalInteraction(*InComponent->DecalList(DecalIndex));
			INT DecalType = (NewInteraction->DecalStaticMesh != NULL) ? STATIC_DECALS : DYNAMIC_DECALS;
			Decals[DecalType].AddItem(NewInteraction);
		}
	}

#if !CONSOLE
	if (GIsEditor && InComponent)
	{
		LightMapResolutionScale = FVector2D(0.0f, 0.0f);
		LightMapType = LMIT_None;
		bLightMapResolutionPadded = FALSE;
	}
#endif
}

FPrimitiveSceneProxy::~FPrimitiveSceneProxy()
{
	for (INT DecalType = 0; DecalType < NUM_DECAL_TYPES; ++DecalType)
	{
		// Free the static decal interactions.
		for(INT DecalIndex = 0;DecalIndex < Decals[DecalType].Num();DecalIndex++)
		{
			delete Decals[DecalType](DecalIndex);
		}
		Decals[DecalType].Empty();
	}
}

HHitProxy* FPrimitiveSceneProxy::CreateHitProxies(const UPrimitiveComponent* Component,TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
{
	if(Component->GetOwner())
	{
		HHitProxy* ActorHitProxy;
		// Create volume brush-component hit proxies with a higher priority than the rest of the world
		if (Component->GetOwner()->IsA(ABrush::StaticClass()) && Component->IsA(UBrushComponent::StaticClass()))
		{
			ActorHitProxy = new HActor(Component->GetOwner(), HPP_Wireframe);
		}
		else
		{
			ActorHitProxy = new HActor(Component->GetOwner());
		}
		OutHitProxies.AddItem(ActorHitProxy);
		return ActorHitProxy;
	}
	else
	{
		return NULL;
	}
}

FPrimitiveViewRelevance FPrimitiveSceneProxy::GetViewRelevance(const FSceneView* View)
{
	return FPrimitiveViewRelevance();
}

void FPrimitiveSceneProxy::SetTransform(const FMatrix& InLocalToWorld,FLOAT InLocalToWorldDeterminant)
{
	// Update the cached transforms.
	LocalToWorld = InLocalToWorld;
	LocalToWorldDeterminant = InLocalToWorldDeterminant;

	// Notify the proxy's implementation of the change.
	OnTransformChanged();
}

FBoxSphereBounds FPrimitiveSceneProxy::GetBounds() const
{
	return PrimitiveSceneInfo->Bounds;
}

/**
 * Adds a decal interaction to the primitive.  This is called in the rendering thread by AddDecalInteraction_GameThread.
 */
void FPrimitiveSceneProxy::AddDecalInteraction_RenderingThread(const FDecalInteraction& DecalInteraction)
{
	INT DecalType;
	//make a copy from the template that will be owned by the proxy
	FDecalInteraction* NewInteraction = new FDecalInteraction(DecalInteraction);
	AddDecalInteraction_Internal_RenderingThread(NewInteraction, DecalType);
}

/**
 * Adds a decal interaction to the primitive.  This is called in the rendering thread by AddDecalInteraction_GameThread.
 * @param NewDecalInteraction - New interaction created from the template
 * @param DecalType - returns if the decal was added to dynamic or static lists
 */
void FPrimitiveSceneProxy::AddDecalInteraction_Internal_RenderingThread(FDecalInteraction* NewDecalInteraction, INT& DecalType)
{
	check(IsInRenderingThread());

	// add the static mesh element for this decal interaction
	NewDecalInteraction->CreateDecalStaticMesh(PrimitiveSceneInfo);

	DecalType = NewDecalInteraction->DecalStaticMesh ? FPrimitiveSceneProxy::STATIC_DECALS : FPrimitiveSceneProxy::DYNAMIC_DECALS;

	// Add the specified interaction to the proxy's decal interaction list.
	Decals[DecalType].AddItem(NewDecalInteraction);
}

/**
 * Adds a decal interaction to the primitive.  This simply sends a message to the rendering thread to call AddDecalInteraction_RenderingThread.
 * This is called in the game thread as new decal interactions are created.
 */
void FPrimitiveSceneProxy::AddDecalInteraction_GameThread(const FDecalInteraction& DecalInteraction)
{
	check(IsInGameThread());

	// Enqueue a message to the rendering thread containing the interaction to add.
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		AddDecalInteraction,
		FPrimitiveSceneProxy*,PrimitiveSceneProxy,this,
		FDecalInteraction,DecalInteraction,DecalInteraction,
	{
		PrimitiveSceneProxy->AddDecalInteraction_RenderingThread(DecalInteraction);
	});

	// remove GT copy of vertex lightmap remapping indices, RT copy deleted when used by the proxy
	if( GIsGame && !GIsEditor )
	{
		DecalInteraction.RenderData->SampleRemapping.Empty();
	}
}

/**
 * Removes a decal interaction from the primitive.  This is called in the rendering thread by RemoveDecalInteraction_GameThread.
 */
void FPrimitiveSceneProxy::RemoveDecalInteraction_RenderingThread(UDecalComponent* DecalComponent)
{
	check(IsInRenderingThread());

	// Find the decal interaction representing the given decal component, and remove it from the interaction list.
	FDecalInteraction* DecalInteraction = NULL;
	for (INT DecalType = 0; DecalType < NUM_DECAL_TYPES; ++DecalType)
	{
		for(INT DecalIndex = 0;DecalIndex < Decals[DecalType].Num();DecalIndex++)
		{
			if(Decals[DecalType](DecalIndex)->Decal == DecalComponent)
			{
				DecalInteraction = Decals[DecalType](DecalIndex);			
				Decals[DecalType].RemoveSwap(DecalIndex);

				delete DecalInteraction;
				DecalIndex--;
			}
		}
	}
}

/**
 * Removes a decal interaction from the primitive.  This simply sends a message to the rendering thread to call RemoveDecalInteraction_RenderingThread.
 * This is called in the game thread when a decal is detached from a primitive which has been added to the scene.
 */
void FPrimitiveSceneProxy::RemoveDecalInteraction_GameThread(UDecalComponent* DecalComponent)
{
	check(IsInGameThread());

	// Enqueue a message to the rendering thread containing the interaction to add.
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		RemoveDecalInteraction,
		FPrimitiveSceneProxy*,PrimitiveSceneProxy,this,
		UDecalComponent*,DecalComponent,DecalComponent,
	{
		PrimitiveSceneProxy->RemoveDecalInteraction_RenderingThread(DecalComponent);
	});

	// Use a fence to keep track of when the rendering thread executes this scene detachment.
	// prevent decals from getting destroyed if this primitive is still accessing the decal component 
	// for updating visibility times in InitViews()
	DecalComponent->DetachFence.BeginFence();
}


/**
 * Updates selection for the primitive proxy. This is called in the rendering thread by SetSelection_GameThread.
 * @param bInSelected - TRUE if the parent actor is selected in the editor
 */
void FPrimitiveSceneProxy::SetSelection_RenderThread(const UBOOL bInSelected)
{
	check(IsInRenderingThread());
	bSelected = bInSelected && PrimitiveSceneInfo->bSelectable;
}

/**
 * Updates selection for the primitive proxy. This simply sends a message to the rendering thread to call SetSelection_RenderThread.
 * This is called in the game thread as selection is toggled.
 * @param bInSelected - TRUE if the parent actor is selected in the editor
 */
void FPrimitiveSceneProxy::SetSelection_GameThread(const UBOOL bInSelected)
{
	check(IsInGameThread());

	// Enqueue a message to the rendering thread containing the interaction to add.
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		SetNewSelection,
		FPrimitiveSceneProxy*,PrimitiveSceneProxy,this,
		const UBOOL,bNewSelection,bInSelected,
	{
		PrimitiveSceneProxy->SetSelection_RenderThread(bNewSelection);
	});
}


/**
 * Updates hover state for the primitive proxy. This is called in the rendering thread by SetHovered_GameThread.
 * @param bInHovered - TRUE if the parent actor is hovered
 */
void FPrimitiveSceneProxy::SetHovered_RenderThread(const UBOOL bInHovered)
{
	check(IsInRenderingThread());
	bHovered = bInHovered;
}

/**
 * Updates hover state for the primitive proxy. This simply sends a message to the rendering thread to call SetHovered_RenderThread.
 * This is called in the game thread as hover state changes
 * @param bInHovered - TRUE if the parent actor is hovered
 */
void FPrimitiveSceneProxy::SetHovered_GameThread(const UBOOL bInHovered)
{
	check(IsInGameThread());

	// Enqueue a message to the rendering thread containing the interaction to add.
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		SetNewHovered,
		FPrimitiveSceneProxy*,PrimitiveSceneProxy,this,
		const UBOOL,bNewHovered,bInHovered,
	{
		PrimitiveSceneProxy->SetHovered_RenderThread(bNewHovered);
	});
}

/**
 * Updates the hidden editor view visibility map on the game thread which just enqueues a command on the render thread
 */
void FPrimitiveSceneProxy::SetHiddenEdViews_GameThread( QWORD InHiddenEditorViews )
{
	check(IsInGameThread());

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		SetEditorVisibility,
		FPrimitiveSceneProxy*,PrimitiveSceneProxy,this,
		const QWORD,NewHiddenEditorViews,InHiddenEditorViews,
	{
		PrimitiveSceneProxy->SetHiddenEdViews_RenderThread(NewHiddenEditorViews);
	});
}

/**
 * Updates the hidden editor view visibility map on the render thread 
 */
void FPrimitiveSceneProxy::SetHiddenEdViews_RenderThread( QWORD InHiddenEditorViews )
{
#if !CONSOLE
	check(IsInRenderingThread());
	HiddenEditorViews = InHiddenEditorViews;
#endif
}

/** 
* Rebuilds the static mesh elements for decals that have a missing FStaticMesh* entry for their interactions
* only called on the game thread
*/
void FPrimitiveSceneProxy::BuildMissingDecalStaticMeshElements_GameThread()
{
	check(IsInGameThread());

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		DecalRegenerateStaticMeshElements,
		FPrimitiveSceneProxy*,PrimitiveSceneProxy,this,
	{
		PrimitiveSceneProxy->BuildMissingDecalStaticMeshElements_RenderThread();
	});
}

/**
* Rebuilds the static mesh elements for decals that have a missing FStaticMesh* entry for their interactions
* enqued by BuildMissingDecalStaticMeshElements_GameThread on the render thread
*/
void FPrimitiveSceneProxy::BuildMissingDecalStaticMeshElements_RenderThread()
{

}

/** @return True if the primitive is visible in the given View. */
UBOOL FPrimitiveSceneProxy::IsShown(const FSceneView* View) const
{
#if !CONSOLE
	if((View->Family->ShowFlags & SHOW_Editor) != 0)
	{
		if(bIsNavigationPoint && !(View->Family->ShowFlags&SHOW_NavigationNodes))
		{
			return FALSE;
		}
		if(bHiddenEditor)
		{
			return FALSE;
		}

		// If the layer is disabled for this viewport, do not render the primitive unless we are in G mode.
		if (!(View->Family->ShowFlags & SHOW_Game) && (HiddenEditorViews & View->EditorViewBitflag))
		{
			return FALSE;
		}
	}
	else
#endif
	{
		if(bHiddenGame)
		{
			return FALSE;
		}

		UBOOL bContainsViewActor = Owners.ContainsItem(View->ViewActor);
		if((bOnlyOwnerSee && !bContainsViewActor) || (bOwnerNoSee && bContainsViewActor))
		{
			return FALSE;
		}
	}

	return TRUE;
}

/** @return True if the primitive is casting a shadow. */
UBOOL FPrimitiveSceneProxy::IsShadowCast(const FSceneView* View) const
{
#if !CONSOLE
	if(View->Family->ShowFlags & SHOW_Editor)
	{
		if(bIsNavigationPoint && !(View->Family->ShowFlags&SHOW_NavigationNodes))
		{
			return FALSE;
		}
		if ((PrimitiveSceneInfo->bCastStaticShadow == FALSE) && 
			(PrimitiveSceneInfo->bCastDynamicShadow == FALSE))
		{
			return FALSE;
		}
		if(bHiddenEditor)
		{
			return PrimitiveSceneInfo->bCastHiddenShadow;
		}
		// if all of it's groups are hidden in this view, don't draw
		if ((HiddenEditorViews & View->EditorViewBitflag) != 0)
		{
			return PrimitiveSceneInfo->bCastHiddenShadow;
		}
	}
	else
#endif
	{
		check(PrimitiveSceneInfo);

		if ((PrimitiveSceneInfo->bCastStaticShadow == FALSE) && 
			(PrimitiveSceneInfo->bCastDynamicShadow == FALSE))
		{
			return FALSE;
		}

		CONSOLE_PREFETCH(&MaxDrawDistanceSquared);
		UBOOL bCastShadow = PrimitiveSceneInfo->bCastHiddenShadow;

		if (bHiddenGame == TRUE)
		{
			return bCastShadow;
		}

		UBOOL bContainsViewActor = Owners.ContainsItem(View->ViewActor);

		// In the OwnerSee cases, we still want to respect hidden shadows...
		// This assumes that bCastHiddenShadow trumps the owner see flags.
		if((bOnlyOwnerSee && !bContainsViewActor) || (bOwnerNoSee && bContainsViewActor))
		{
			return bCastShadow;
		}
	}

	// Compute the distance between the view and the primitive.
	FLOAT DistanceSquared = 0.0f;
	// No ortho viewports on console
#if CONSOLE
#else
	if(View->ViewOrigin.W > 0.0f)
#endif
	{
		DistanceSquared = (PrimitiveSceneInfo->Bounds.Origin - View->ViewOrigin).SizeSquared();
		// Cull the primitive if the view is farther than its cull distance.
		// @todo: Take into account fade opacity here which drives object visibility?
		if(DistanceSquared * View->LODDistanceFactorSquared > MaxDrawDistanceSquared)
		{
			return FALSE;
		} 
	}

	return TRUE;
}

/** @return True if the primitive has decals with static relevance which should be rendered in the given view. */
UBOOL FPrimitiveSceneProxy::HasRelevantStaticDecals(const FSceneView* View) const
{
	if( View->Family->ShowFlags & SHOW_Decals )
	{
#if !FINAL_RELEASE
		if( IsRichView(View) )
		{
			// always force dynamic relevance for decals when in rich rendering modes
			return FALSE;
		}
#endif
		return (Decals[STATIC_DECALS].Num() > 0);
	}
	return FALSE;
}

/** @return True if the primitive has decals with dynamic relevance which should be rendered in the given view. */
UBOOL FPrimitiveSceneProxy::HasRelevantDynamicDecals(const FSceneView* View) const
{
	if( View->Family->ShowFlags & SHOW_Decals &&
		GSystemSettings.bAllowUnbatchedDecals )
	{
#if !FINAL_RELEASE
		if( IsRichView(View) )
		{
			// always force dynamic relevance for decals when in rich rendering modes
			return (Decals[STATIC_DECALS].Num() + Decals[DYNAMIC_DECALS].Num()) > 0;
		}
#endif

		return (Decals[DYNAMIC_DECALS].Num() > 0);
	}
	return FALSE;
}

/** @return True if the primitive has decals which should be rendered in the given view and that use a lit material. */
UBOOL FPrimitiveSceneProxy::HasLitDecals(const FSceneView* View) const
{
	if( View->Family->ShowFlags & SHOW_Decals )
	{	
		for (INT DecalType = 0; DecalType < NUM_DECAL_TYPES; ++DecalType)
		{
			for( INT DecalIndex = 0 ; DecalIndex < Decals[DecalType].Num() ; ++DecalIndex )
			{
				const FDecalInteraction* Interaction = Decals[DecalType](DecalIndex);
				if( Interaction->DecalState.MaterialViewRelevance.bLit )
				{
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

void FPrimitiveSceneProxy::SetRelevanceForShowBounds(EShowFlags ShowFlags, FPrimitiveViewRelevance& ViewRelevance) const
{
	if (ShowFlags & SHOW_Bounds)
	{
		ViewRelevance.SetDPG((ShowFlags & SHOW_Game) ? SDPG_World : SDPG_Foreground,TRUE);
	}
}

void FPrimitiveSceneProxy::RenderBounds(
	FPrimitiveDrawInterface* PDI, 
	UINT DPGIndex, 
	EShowFlags ShowFlags, 
	const FBoxSphereBounds& Bounds, 
	UBOOL bRenderInEditor) const
{
	const ESceneDepthPriorityGroup DrawBoundsDPG = (ShowFlags & SHOW_Game) ? SDPG_World : SDPG_Foreground;
	if (DPGIndex == DrawBoundsDPG
		&& (ShowFlags & SHOW_Bounds) 
		&& ((ShowFlags & SHOW_Game) || bRenderInEditor))
	{
		// Draw the static mesh's bounding box and sphere.
		DrawWireBox(PDI,Bounds.GetBox(), FColor(72,72,255),DrawBoundsDPG);
		DrawCircle(PDI,Bounds.Origin,FVector(1,0,0),FVector(0,1,0),FColor(255,255,0),Bounds.SphereRadius,32,DrawBoundsDPG);
		DrawCircle(PDI,Bounds.Origin,FVector(1,0,0),FVector(0,0,1),FColor(255,255,0),Bounds.SphereRadius,32,DrawBoundsDPG);
		DrawCircle(PDI,Bounds.Origin,FVector(0,1,0),FVector(0,0,1),FColor(255,255,0),Bounds.SphereRadius,32,DrawBoundsDPG);
	}
}

///////////////////////////////////////////////////////////////////////////////
// PRIMITIVE COMPONENT
///////////////////////////////////////////////////////////////////////////////

INT UPrimitiveComponent::CurrentTag = 2147483647 / 4;

/**
 * Returns whether this primitive only uses unlit materials.
 *
 * @return TRUE if only unlit materials are used for rendering, false otherwise.
 */
UBOOL UPrimitiveComponent::UsesOnlyUnlitMaterials() const
{
	return FALSE;
}

/**
 * Returns the lightmap resolution used for this primivite instnace in the case of it supporting texture light/ shadow maps.
 * 0 if not supported or no static shadowing.
 *
 * @param	Width	[out]	Width of light/shadow map
 * @param	Height	[out]	Height of light/shadow map
 *
 * @return	UBOOL			TRUE if LightMap values are padded, FALSE if not
 */
UBOOL UPrimitiveComponent::GetLightMapResolution( INT& Width, INT& Height ) const
{
	Width	= 0;
	Height	= 0;
	return FALSE;
}

/**
 * Returns the light and shadow map memory for this primite in its out variables.
 *
 * Shadow map memory usage is per light whereof lightmap data is independent of number of lights, assuming at least one.
 *
 * @param [out] LightMapMemoryUsage		Memory usage in bytes for light map (either texel or vertex) data
 * @param [out]	ShadowMapMemoryUsage	Memory usage in bytes for shadow map (either texel or vertex) data
 */
void UPrimitiveComponent::GetLightAndShadowMapMemoryUsage( INT& LightMapMemoryUsage, INT& ShadowMapMemoryUsage ) const
{
	LightMapMemoryUsage		= 0;
	ShadowMapMemoryUsage	= 0;
	return;
}

/** 
 *	Indicates whether this PrimitiveComponent should be considered for collision, inserted into Octree for collision purposes etc.
 *	Basically looks at CollideActors, and Owner's bCollideActors (if present).
 */
UBOOL UPrimitiveComponent::ShouldCollide() const
{
	return CollideActors && (!Owner || Owner->bCollideActors); 
}

void UPrimitiveComponent::GenerateDecalRenderData(FDecalState* Decal, TArray< FDecalRenderData* >& OutDecalRenderDatas ) const
{
	OutDecalRenderDatas.Reset();
}

/** Modifies the scale factor of this matrix by the recipricol of Vec. */
static inline void ScaleRotByVecRecip(FMatrix& InMatrix, const FVector& Vec)
{
	FVector RecipVec( 1.f/Vec.X, 1.f/Vec.Y, 1.f/Vec.Z );

	InMatrix.M[0][0] *= RecipVec.X;
	InMatrix.M[0][1] *= RecipVec.X;
	InMatrix.M[0][2] *= RecipVec.X;

	InMatrix.M[1][0] *= RecipVec.Y;
	InMatrix.M[1][1] *= RecipVec.Y;
	InMatrix.M[1][2] *= RecipVec.Y;

	InMatrix.M[2][0] *= RecipVec.Z;
	InMatrix.M[2][1] *= RecipVec.Z;
	InMatrix.M[2][2] *= RecipVec.Z;
}

/** Removes any scaling from the LocalToWorld matrix and returns it, along with the overall scaling. */
void UPrimitiveComponent::GetTransformAndScale(FMatrix& OutTransform, FVector& OutScale)
{
	OutScale = Scale * Scale3D;
	// When using AbsoluteScale, the scale of the owner is removed from the LocalToWorld even though it wasn't applied in the first place.
	if ((Owner != NULL) && !AbsoluteScale)
	{
		OutScale *= Owner->DrawScale * Owner->DrawScale3D;
	}

	if(OutScale.IsNearlyZero())
	{
		// Only complain about the scale if the object is not hidden
		if( Owner && !Owner->bHidden )
		{
			debugf(TEXT("GetTransformAndScale : Zero scale! (%s)"), *GetPathName());
		}
		OutTransform = FMatrix::Identity;
		return;
	}
	else
	{
		OutTransform = LocalToWorld;
		ScaleRotByVecRecip(OutTransform, OutScale);
	}
}

/**
 * Returns the material textures used to render this primitive for the given quality level
 * Internally calls GetUsedMaterials() and GetUsedTextures() for each material.
 *
 * @param OutTextures	[out] The list of used textures.
 * @param Quality		The platform to get material textures for. If unspecified, it will get textures for current SystemSetting
 * @param bAllQualities	Whether to iterate for all platforms. The Platform parameter is ignored if this is set to TRUE.
*/
void UPrimitiveComponent::GetUsedTextures(TArray<UTexture*> &OutTextures, EMaterialShaderQuality Quality, UBOOL bAllQualities)
{
	// Get the used materials so we can get their textures
	TArray<UMaterialInterface*> UsedMaterials;
	GetUsedMaterials( UsedMaterials );

	TArray<UTexture*> UsedTextures;
	for( INT MatIndex = 0; MatIndex < UsedMaterials.Num(); ++MatIndex )
	{
		// Ensure we don't have any NULL elements.
		if( UsedMaterials( MatIndex ) )
		{
			UsedTextures.Reset();
			UsedMaterials( MatIndex )->GetUsedTextures( UsedTextures, Quality, bAllQualities );

			for( INT TextureIndex=0; TextureIndex<UsedTextures.Num(); TextureIndex++ )
			{
				OutTextures.AddUniqueItem( UsedTextures(TextureIndex) );
			}
		}
	}
}

//
//	UPrimitiveComponent::UpdateBounds
//

void UPrimitiveComponent::UpdateBounds()
{
	Bounds.Origin = FVector(0,0,0);
	Bounds.BoxExtent = FVector(HALF_WORLD_MAX,HALF_WORLD_MAX,HALF_WORLD_MAX);
	Bounds.SphereRadius = appSqrt(3.0f * Square(HALF_WORLD_MAX));
}

void UPrimitiveComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	CachedParentToWorld = ParentToWorld;
}

void UPrimitiveComponent::AttachDecal(UDecalComponent* Decal, FDecalRenderData* RenderData, const FDecalState* DecalState)
{
#if !FINAL_RELEASE
	// Unless the decal is applied to an instanced static mesh, then we'll make sure that the
	// decal is only attached to this component once!
	// @todo: We should also check to be sure a decal is never bound to the same instance more than once
	if( RenderData->InstanceIndex == INDEX_NONE )
	{
		for(INT DecalIndex = 0;DecalIndex < DecalList.Num();DecalIndex++)
		{
			if( Decal && 
				DecalList(DecalIndex)->Decal == Decal)
			{
				debugf(TEXT("Duplicate decal interaction! decal=%s"), *Decal->GetPathName());
			}
		}
	}
#endif

	FDecalInteraction* NewDecalInteraction = new FDecalInteraction( Decal, RenderData );
	if ( DecalState )
	{
		NewDecalInteraction->DecalState = *DecalState;
	}
	else
	{
		Decal->CaptureDecalState( &(NewDecalInteraction->DecalState) );
	}


	// Grab the local -> world matrix for this mesh.  For instanced components, we'll also take into
	// account the instance -> local transform by using the instancing API.
	checkSlow( !IsPrimitiveInstanced() || RenderData->InstanceIndex != INDEX_NONE );
	const FMatrix& InstanceLocalToWorld = GetInstanceLocalToWorld( RenderData->InstanceIndex );

	// keep track of the transform used during attachment
	NewDecalInteraction->DecalState.UpdateAttachmentLocalToWorld(InstanceLocalToWorld);

	// each primitive component keeps track of its attached decals
	DecalList.AddItem( NewDecalInteraction );

	// If the primitive has been added to the scene, add the decal interaction to its proxy.
	if(SceneInfo)
	{
		SceneInfo->Proxy->AddDecalInteraction_GameThread(*NewDecalInteraction);
	}
	INC_DWORD_STAT_BY( STAT_DecalInteractionMemory, NewDecalInteraction->DecalState.GetMemoryFootprint()*2 /** x2 for RT copy */ );
}

void UPrimitiveComponent::DetachDecal(UDecalComponent* Decal)
{
	for ( INT i = 0 ; i < DecalList.Num() ; ++i )
	{
		FDecalInteraction* DecalInteraction = DecalList(i);

		if ( DecalInteraction && DecalInteraction->Decal == Decal )
		{
			DEC_DWORD_STAT_BY( STAT_DecalInteractionMemory, DecalInteraction->DecalState.GetMemoryFootprint()*2 /** x2 for RT copy */ );
			// Remove the interaction element from the decal list.  RenderData will be cleared by the decal.
			delete DecalInteraction;
			DecalList.Remove(i);
			i--;
		}
	}

	// If the primitive has been added to the scene, and we found a decal interaction for the decal,
	// remove the decal interaction from the primitive's rendering thread scene proxy.
	if(SceneInfo)
	{
		SceneInfo->Proxy->RemoveDecalInteraction_GameThread(Decal);
	}
}

void UPrimitiveComponent::Attach()
{
	FLightingChannelContainer AllChannels;
	AllChannels.SetAllChannels();
	if( !LightingChannels.bInitialized || bAcceptsLights && !LightingChannels.OverlapsWith(AllChannels) )
	{
		UBOOL bHasStaticShadowing		= HasStaticShadowing();
		LightingChannels.Static			= bHasStaticShadowing;
		LightingChannels.Dynamic		= !bHasStaticShadowing;
		LightingChannels.CompositeDynamic = FALSE;
		LightingChannels.bInitialized	= TRUE;
	}

	// Make sure cached cull distance is up-to-date if its zero and we have an LD cull distance
	if( CachedMaxDrawDistance == 0 && LDMaxDrawDistance > 0 )
	{
		CachedMaxDrawDistance = LDMaxDrawDistance;
	}

	Super::Attach();

	// build the crazy matrix
	SetTransformedToWorld();

	UpdateBounds();

	// If there primitive collides(or it's the editor) and the scene is associated with a world, add it to the world's hash.
	UWorld* World = Scene->GetWorld();
	if(ShouldCollide() && World)
	{
		World->Hash->AddPrimitive(this);
	}

	// Notify the light environment that we are using it for rendering
	if (LightEnvironment)
	{
		LightEnvironment->AddAffectedComponent(this);
	}
	
	//add the fog volume component if one has been set
	if (FogVolumeComponent)
	{
		Scene->AddFogVolume(FogVolumeComponent, this);
	}

	// Setup ShadowParent if appropriate
	// Don't overwrite an explicit shadow parent 
	if (!bHasExplicitShadowParent
		&& Owner 
		&& Owner->bShadowParented 
		&& CastShadow 
		&& bCastDynamicShadow)
	{
		if (Owner->BaseSkelComponent)
		{
			// Use BaseSkelComponent as the shadow parent for this actor if requested.
			ShadowParent = Owner->BaseSkelComponent;
		}
		else if (Owner->Base)
		{
			AActor* ParentActor = Owner->Base;
			while (ParentActor->Base)
			{
				if( ParentActor->Base == ParentActor )
				{
					// recursion detected...  we can just break out here as this warning is dealt with in other code
					break;
				}
				// Walk up the attachment chain and find the parent
				ParentActor = ParentActor->Base;
			}

			// Search for a shadow casting primitive component to use as the shadow parent
			UPrimitiveComponent* ShadowCastingPrimComponent = NULL;
			for (INT BaseComponentIndex = 0; BaseComponentIndex < ParentActor->Components.Num(); BaseComponentIndex++)
			{
				UPrimitiveComponent* CurrentComponent = Cast<UPrimitiveComponent>(ParentActor->Components(BaseComponentIndex));
				if (CurrentComponent && CurrentComponent->CastShadow && CurrentComponent->bCastDynamicShadow)
				{
					ShadowCastingPrimComponent = CurrentComponent;
					break;
				}
			}
			ShadowParent = ShadowCastingPrimComponent;
		}
	}

	// If the primitive isn't hidden and the detail mode setting allows it, add it to the scene.
	if (ShouldComponentAddToScene())
	{
		Scene->AddPrimitive(this);
	}

	// reattach any decals which were interacting with this receiver primitive
	if( DecalsToReattach.Num() > 0 )
	{	
		for( INT Idx=0; Idx < DecalsToReattach.Num(); ++Idx )
		{
			UDecalComponent* ReattachDecal = DecalsToReattach(Idx);
			if( ReattachDecal )
			{
				ReattachDecal->AttachReceiver(this);
			}
		}
		DecalsToReattach.Empty();
	}
}

void UPrimitiveComponent::UpdateTransform()
{
	Super::UpdateTransform();

	SetTransformedToWorld();

	UpdateBounds();

	// If there primitive collides(or it's the editor) and the scene is associated with a world, update the primitive in the world's hash.
	UWorld* World = Scene->GetWorld();
	if(ShouldCollide() && World)
	{
		World->Hash->RemovePrimitive(this);
		World->Hash->AddPrimitive(this);
	}

	// If the primitive isn't hidden update its transform.
	const UBOOL bShowInEditor = !HiddenEditor && (!Owner || !Owner->IsHiddenEd());
	const UBOOL bShowInGame = !HiddenGame && (!Owner || !Owner->bHidden || bIgnoreOwnerHidden);
	const UBOOL bDetailModeAllowsRendering	= DetailMode <= GSystemSettings.DetailMode;
	if( bDetailModeAllowsRendering && ((GIsGame && bShowInGame) || (!GIsGame && bShowInEditor) || bCastHiddenShadow))
	{
		// Update the scene info's transform for this primitive.
		Scene->UpdatePrimitiveTransform(this);
	}

	UpdateRBKinematicData();
}

void UPrimitiveComponent::Detach( UBOOL bWillReattach )
{
	// Clear the actor's shadow parent if it's the BaseSkelComponent.
	if( Owner && Owner->bShadowParented && !bHasExplicitShadowParent )
	{
		ShadowParent = NULL;
	}

	// detach decals which are interacting with this receiver primitive
	if( DecalList.Num() > 0 &&
		AllowDecalRemovalOnDetach() )
	{	
		TArray<UDecalComponent*> DecalsToDetach;
		for( INT DecalIdx=0; DecalIdx < DecalList.Num(); ++DecalIdx )
		{
			FDecalInteraction* DecalInteraction = DecalList(DecalIdx);
			if( DecalInteraction && 
				DecalInteraction->Decal )
			{
				DecalsToDetach.AddUniqueItem(DecalInteraction->Decal);
			}
		}
		for( INT DetachIdx=0; DetachIdx < DecalsToDetach.Num(); ++DetachIdx )
		{
			UDecalComponent* DecalToDetach = DecalsToDetach(DetachIdx);
			DecalToDetach->DetachFromReceiver(this);
		}
		if( bWillReattach &&
			AllowDecalAutomaticReAttach() )
		{
			// reattach these decals to this primitive during Attach()
			DecalsToReattach = DecalsToDetach;
		}
	}

	// If there primitive collides(or it's the editor) and the scene is associated with a world, remove the primitive from the world's hash.
	UWorld* World = Scene->GetWorld();
	if(World)
	{
		World->Hash->RemovePrimitive(this);
	}

	//remove the fog volume component
	if (FogVolumeComponent)
	{
		Scene->RemoveFogVolume(this);
	}

	// Remove the primitive from the scene.
	Scene->RemovePrimitive(this, bWillReattach);

	// Use a fence to keep track of when the rendering thread executes this scene detachment.
	DetachFence.BeginFence();
	if(Owner)
	{
		Owner->DetachFence.BeginFence();
	}

	// If PreviousLightEnvironment is non-null then it is the light environment that was used while attached
	if( PreviousLightEnvironment )
	{
		// Notify the light environment that we are no longer using it for rendering
		PreviousLightEnvironment->RemoveAffectedComponent(this);
		PreviousLightEnvironment = NULL;
	}
	else if( LightEnvironment )
	{
		// Notify the light environment that we are no longer using it for rendering
		LightEnvironment->RemoveAffectedComponent(this);
	}

	for( INT DecalIdx=0; DecalIdx < DecalList.Num(); DecalIdx++ )
	{
		FDecalInteraction* DecalInteraction = DecalList(DecalIdx);
		if( DecalInteraction->Decal )
		{ 
			DecalInteraction->Decal->DetachFence.BeginFence();
		}
	}	

	Super::Detach( bWillReattach );
}

//
//	UPrimitiveComponent::Serialize
//

void UPrimitiveComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(!Ar.IsSaving() && !Ar.IsLoading())
	{
		Ar << BodyInstance;
	}

	if( Ar.Ver() < VER_UPDATED_DECAL_USAGE_FLAGS )
	{
		bAcceptsStaticDecals = bAcceptsDecals;
		bAcceptsDynamicDecals = bAcceptsDecalsDuringGameplay;
	}

	if (Ar.Ver() < VER_RENAMED_CULLDISTANCE)
	{
		LDMaxDrawDistance = LDCullDistance;
		CachedMaxDrawDistance = CachedCullDistance_DEPRECATED;
	}
}

//
//	UPrimitiveComponent::PostEditChange
//

void UPrimitiveComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Keep track of old cached cull distance to see whether we need to re-attach component.
	const FLOAT OldCachedMaxDrawDistance = CachedMaxDrawDistance;

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if(PropertyThatChanged)
	{
		const FName PropertyName = PropertyThatChanged->GetFName();

		// Detect property changes which affect lighting, and discard cached lighting.
		if(PropertyName == TEXT("bAcceptsLights") || PropertyName == TEXT("bUsePrecomputedShadows"))
		{
			InvalidateLightingCache();
		}

		if (bUsePrecomputedShadows
			&& LightEnvironment 
			&& LightEnvironment->IsEnabled())
		{
			// Disable an associated light environment when enabling precomputed shadows
			LightEnvironment->SetEnabled(FALSE);
		}

		// We disregard cull distance volumes in this case as we have no way of handling cull 
		// distance changes to without refreshing all cull distance volumes. Saving or updating 
		// any cull distance volume will set the proper value again.
		if( PropertyName == TEXT("MaxDrawDistance") || PropertyName == TEXT("bAllowCullDistanceVolume") )
		{
			CachedMaxDrawDistance = LDMaxDrawDistance;
		}

		// we need to reattach the primitive if the min draw distnace changed to propagate the change to the rendering thread
		if (PropertyThatChanged->GetName() == TEXT("MinDrawDistance"))
		{
			FPrimitiveSceneAttachmentContext ReattachDueToMinDrawDistanceChange(this);
		}
	}

	ValidateLightingChannels();
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (Owner != NULL && Owner->CollisionComponent == this)
	{
		Owner->BlockRigidBody = BlockRigidBody;
	}

	// Make sure cached cull distance is up-to-date.
	if( LDMaxDrawDistance > 0 )
	{
		CachedMaxDrawDistance = Min( LDMaxDrawDistance, CachedMaxDrawDistance );
	}
	// Directly use LD cull distance if cull distance volumes are disabled.
	if( !bAllowCullDistanceVolume )
	{
		CachedMaxDrawDistance = LDMaxDrawDistance;
	}

	// Reattach to propagate cull distance change.
	if( CachedMaxDrawDistance != OldCachedMaxDrawDistance )
	{
		FPrimitiveSceneAttachmentContext ReattachDueToCullDistanceChange( this );
	}
}

void UPrimitiveComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if( GIsGame )
	{
		for( FEditPropertyChain::TIterator It(PropertyChangedEvent.PropertyChain.GetHead()); It; ++It )
		{
			FName N = *It->GetName();
			if( appStricmp( *It->GetName(), TEXT("Scale3D") )		== 0 || 
				appStricmp( *It->GetName(), TEXT("Scale") )			== 0 || 
				appStricmp( *It->GetName(), TEXT("Translation") )	== 0 || 
				appStricmp( *It->GetName(), TEXT("Rotation") )		== 0 )
			{
				BeginDeferredUpdateTransform();
			}
		}
	}

	Super::PostEditChangeChainProperty( PropertyChangedEvent );
}

/**
 * Validates the lighting channels and makes adjustments as appropriate.
 */
void UPrimitiveComponent::ValidateLightingChannels()
{
	// Don't allow dynamic objects to be in the static groups so we can discard entirely static lights.
	if( !HasStaticShadowing() )
	{
		LightingChannels.BSP	= FALSE;
		LightingChannels.Static = FALSE;
		LightingChannels.CompositeDynamic = FALSE;
	}
}

/**
 * Function that gets called from within Map_Check to allow this actor to check itself
 * for any potential errors and register them with map check dialog.
 */
#if WITH_EDITOR
void UPrimitiveComponent::CheckForErrors()
{
	ValidateLightingChannels();

	FLightingChannelContainer AllChannels;
	AllChannels.SetAllChannels();
	if( Owner && IsValidComponent() && !LightingChannels.OverlapsWith( AllChannels ) && bAcceptsLights )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, Owner, *FString( LocalizeUnrealEd( "MapCheck_Message_NoLightingChannels" ) ), TEXT( "NoLightingChannels" ) );
	}

	if( DepthPriorityGroup == SDPG_UnrealEdBackground || DepthPriorityGroup == SDPG_UnrealEdForeground )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, Owner, *FString( LocalizeUnrealEd( "MapCheck_Message_BadDepthPriorityGroup" ) ), TEXT( "BadDepthPriorityGroup" ) );
	}

	if (bAcceptsLights && CastShadow && bCastDynamicShadow && !bUsePrecomputedShadows && BoundsScale > 1.0f)
	{
		GWarn->MapCheck_Add( MCTYPE_PERFORMANCEWARNING, Owner, *FString( LocalizeUnrealEd( "MapCheck_Message_ShadowCasterUsingBoundsScale" ) ), TEXT( "ShadowCasterUsingBoundsScale" ) );
	}
}
#endif

//
//	UPrimitiveComponent::PostLoad
//
void UPrimitiveComponent::PostLoad()
{
	Super::PostLoad();

	if (bUsePrecomputedShadows
		&& LightEnvironment 
		&& LightEnvironment->IsEnabled())
	{
		// Disable an associated light environment when using precomputed shadows
		LightEnvironment->SetEnabled(FALSE);
	}

	// Perform some postload fixups/ optimizations if we're running the game.
	if( GIsGame && !IsTemplate(RF_ClassDefaultObject) )
	{
		// This primitive only uses unlit materials so there is no benefit to accepting any lights.
		if( UsesOnlyUnlitMaterials() )
		{
			bAcceptsLights = FALSE;
		}
	}

	// Call the update ValidateLightingChannels on previously saved PrimitiveComponents.
	ValidateLightingChannels();

	// Make sure cached cull distance is up-to-date.
	if( LDMaxDrawDistance > 0 )
	{
		// Directly use LD cull distance if cached one is not set.
		if( CachedMaxDrawDistance == 0 )
		{
			CachedMaxDrawDistance = LDMaxDrawDistance;
		}
		// Use min of both if neither is 0. Need to check as 0 has special meaning.
		else
		{
			CachedMaxDrawDistance = Min( LDMaxDrawDistance, CachedMaxDrawDistance );
		}
	}
}

UBOOL UPrimitiveComponent::IsReadyForFinishDestroy()
{
	// Don't allow the primitive component to the purged until its pending scene detachments have completed.
	return Super::IsReadyForFinishDestroy() && DetachFence.GetNumPendingFences() == 0;
}

UBOOL UPrimitiveComponent::NeedsLoadForClient() const
{
	if(HiddenGame && !ShouldCollide() && !AlwaysLoadOnClient)
	{
		return 0;
	}
	else
	{
		return Super::NeedsLoadForClient();
	}
}

UBOOL UPrimitiveComponent::NeedsLoadForServer() const
{
	if(!ShouldCollide() && !AlwaysLoadOnServer)
	{
		return 0;
	}
	else
	{
		return Super::NeedsLoadForServer();
	}
}

void UPrimitiveComponent::PostCrossLevelFixup()
{
	// reattach to the scene because the replacement primitive just changed
	if (IsAttached())
	{
		BeginDeferredReattach();
	}
}

void UPrimitiveComponent::execSetTraceBlocking(FFrame& Stack,RESULT_DECL)
{
	P_GET_UBOOL(NewBlockZeroExtent);
	P_GET_UBOOL(NewBlockNonZeroExtent);
	P_FINISH;

	BlockZeroExtent = NewBlockZeroExtent;
	BlockNonZeroExtent = NewBlockNonZeroExtent;
}

void UPrimitiveComponent::execSetActorCollision(FFrame& Stack,RESULT_DECL)
{
	P_GET_UBOOL(NewCollideActors);
	P_GET_UBOOL(NewBlockActors);
	P_GET_UBOOL_OPTX(NewAlwaysCheckCollision,FALSE);
	P_FINISH;

	AlwaysCheckCollision = NewAlwaysCheckCollision;
	if (NewCollideActors != CollideActors)
	{
		CollideActors = NewCollideActors;
		BeginDeferredReattach();

		if(CollideActors && AlwaysCheckCollision)
		{
			if(Owner != NULL)
			{
				Owner->FindTouchingActors();
			}
		}
	}
	BlockActors = NewBlockActors;
	
}

void UPrimitiveComponent::execSetOwnerNoSee(FFrame& Stack,RESULT_DECL)
{
	P_GET_UBOOL(bNewOwnerNoSee);
	P_FINISH;

	SetOwnerNoSee(bNewOwnerNoSee);
}

void UPrimitiveComponent::SetOwnerNoSee(UBOOL bNewOwnerNoSee)
{
	if(bOwnerNoSee != bNewOwnerNoSee)
	{
		bOwnerNoSee = bNewOwnerNoSee;
		BeginDeferredReattach();
	}
}

void UPrimitiveComponent::execSetOnlyOwnerSee(FFrame& Stack,RESULT_DECL)
{
	P_GET_UBOOL(bNewOnlyOwnerSee);
	P_FINISH;

	SetOnlyOwnerSee(bNewOnlyOwnerSee);
}

// Ignore Owner Hidden
void UPrimitiveComponent::SetIgnoreOwnerHidden(UBOOL bNewIgnoreOwnerHidden)
{
	bIgnoreOwnerHidden = bNewIgnoreOwnerHidden;
	BeginDeferredReattach();
}

void UPrimitiveComponent::execSetIgnoreOwnerHidden(FFrame& Stack,RESULT_DECL)
{
	P_GET_UBOOL(bNewIgnoreOwnerHidden);
	P_FINISH;

	SetIgnoreOwnerHidden(bNewIgnoreOwnerHidden);
}

//

void UPrimitiveComponent::SetOnlyOwnerSee(UBOOL bNewOnlyOwnerSee)
{
	if(bOnlyOwnerSee != bNewOnlyOwnerSee)
	{
		bOnlyOwnerSee = bNewOnlyOwnerSee;
		BeginDeferredReattach();
	}
}

/** 
 *  Looking at various values of the component, determines if this
 *  component should be added to the scene
 * @return TRUE if the component is visible and should be added to the scene, FALSE otherwise
 */
UBOOL UPrimitiveComponent::ShouldComponentAddToScene() const
{
	// If the primitive isn't hidden and the detail mode setting and mobile features allow it, add it to the scene.
	const UBOOL bShowInEditor				= !HiddenEditor && (!Owner || !Owner->IsHiddenEd());
	const UBOOL bShowInGame					= !HiddenGame && (!Owner || !Owner->bHidden || bIgnoreOwnerHidden);
	const UBOOL bDetailModeAllowsRendering	= DetailMode <= GSystemSettings.DetailMode;
	const UBOOL bCheckMobile				= !(GUsingMobileRHI || GEmulateMobileRendering) || bSupportedOnMobile;
	return bDetailModeAllowsRendering && bCheckMobile && ((GIsGame && bShowInGame) || (!GIsGame && bShowInEditor) || bCastHiddenShadow);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execShouldComponentAddToScene);

void UPrimitiveComponent::execSetHidden(FFrame& Stack,RESULT_DECL)
{
	P_GET_UBOOL(NewHidden);
	P_FINISH;

	SetHiddenGame(NewHidden);
}

void UPrimitiveComponent::SetHiddenGame(UBOOL NewHidden)
{
	if( NewHidden != HiddenGame )
	{
		HiddenGame = NewHidden;
		BeginDeferredReattach();
	}
}

/**
 * Pushes new selection state to the render thread primitive proxy
 * @param bInSelected - TRUE if the proxy should display as if selected
 */
void UPrimitiveComponent::PushSelectionToProxy(const UBOOL bInSelected)
{
	//although this should only be called for attached components, some sprite component can get in without valid proxies
	if (SceneInfo && SceneInfo->Proxy)
	{
		SceneInfo->Proxy->SetSelection_GameThread(bInSelected);
	}
}

/**
 * Sends editor visibility updates to the render thread
 */
void UPrimitiveComponent::PushEditorVisibilityToProxy( QWORD InVisibility )
{
	//although this should only be called for attached components, some sprite components can get in without valid proxies
	if (SceneInfo && SceneInfo->Proxy)
	{
		SceneInfo->Proxy->SetHiddenEdViews_GameThread( InVisibility );
	}
}

/**
 * Pushes new hover state to the render thread primitive proxy
 * @param bInHovered - TRUE if the proxy should display as if hovered
 */
void UPrimitiveComponent::PushHoveredToProxy(const UBOOL bInHovered)
{
	//although this should only be called for attached components, some sprite component can get in without valid proxies
	if (SceneInfo && SceneInfo->Proxy)
	{
		SceneInfo->Proxy->SetHovered_GameThread(bInHovered);
	}
}


/**
 *	Sets the HiddenEditor flag and reattaches the component as necessary.
 *
 *	@param	NewHidden		New Value for the HiddenEditor flag.
 */
void UPrimitiveComponent::SetHiddenEditor(UBOOL NewHidden)
{
	if( NewHidden != HiddenEditor )
	{
		HiddenEditor = NewHidden;
		BeginDeferredReattach();
	}
}

void UPrimitiveComponent::execSetShadowParent(FFrame& Stack,RESULT_DECL)
{
	P_GET_OBJECT(UPrimitiveComponent,NewShadowParent);
	P_FINISH;
	SetShadowParent(NewShadowParent);
}

void UPrimitiveComponent::SetShadowParent(UPrimitiveComponent* NewShadowParent)
{
	if (ShadowParent != NewShadowParent)
	{
		ShadowParent = NewShadowParent;
		bHasExplicitShadowParent = NewShadowParent != NULL;
		if (IsAttached())
		{
			BeginDeferredReattach();
		}
	}
}

void UPrimitiveComponent::execSetLightEnvironment(FFrame& Stack,RESULT_DECL)
{
	P_GET_OBJECT(ULightEnvironmentComponent,NewLightEnvironment);
	P_FINISH;
	SetLightEnvironment(NewLightEnvironment);
}

void UPrimitiveComponent::SetLightEnvironment(ULightEnvironmentComponent* NewLightEnvironment)
{
	if (NewLightEnvironment != LightEnvironment)
	{
		if (IsAttached())
		{
			// Maintain a reference to the previous light environment so that we can notify it when we stop using it (detach)
			PreviousLightEnvironment = LightEnvironment;
		}
		LightEnvironment = NewLightEnvironment;

		if (IsAttached())
		{
			// Enqueue a reattach so the new property will be propagated to the rendering thread.
			BeginDeferredReattach();
		}
	}
}

void UPrimitiveComponent::execSetCullDistance(FFrame& Stack,RESULT_DECL)
{
	P_GET_FLOAT(NewCullDistance);
	P_FINISH;
	SetCullDistance(NewCullDistance);
}

void UPrimitiveComponent::SetCullDistance(FLOAT NewCullDistance)
{
	if( CachedMaxDrawDistance != NewCullDistance )
	{
	    CachedMaxDrawDistance = NewCullDistance;
	    BeginDeferredReattach();
	}
}

void UPrimitiveComponent::execSetLightingChannels(FFrame& Stack,RESULT_DECL)
{
	P_GET_STRUCT(FLightingChannelContainer,NewLightingChannels);
	P_FINISH;
	SetLightingChannels(NewLightingChannels);
}

void UPrimitiveComponent::SetLightingChannels(FLightingChannelContainer NewLightingChannels)
{
	LightingChannels = NewLightingChannels;
	BeginDeferredReattach();
}

void UPrimitiveComponent::execSetDepthPriorityGroup(FFrame& Stack,RESULT_DECL)
{
	P_GET_BYTE(NewDepthPriorityGroup);
	P_FINISH;
	SetDepthPriorityGroup((ESceneDepthPriorityGroup)NewDepthPriorityGroup);
}

void UPrimitiveComponent::SetDepthPriorityGroup(ESceneDepthPriorityGroup NewDepthPriorityGroup)
{
	if (DepthPriorityGroup != NewDepthPriorityGroup)
	{
		DepthPriorityGroup = NewDepthPriorityGroup;
		BeginDeferredReattach();
	}
}

void UPrimitiveComponent::execSetViewOwnerDepthPriorityGroup(FFrame& Stack,RESULT_DECL)
{
	P_GET_UBOOL(bNewUseViewOwnerDepthPriorityGroup);
	P_GET_BYTE(NewViewOwnerDepthPriorityGroup);
	P_FINISH;
	SetViewOwnerDepthPriorityGroup(
		bNewUseViewOwnerDepthPriorityGroup,
		(ESceneDepthPriorityGroup)NewViewOwnerDepthPriorityGroup
		);
}

void UPrimitiveComponent::SetViewOwnerDepthPriorityGroup(
	UBOOL bNewUseViewOwnerDepthPriorityGroup,
	ESceneDepthPriorityGroup NewViewOwnerDepthPriorityGroup
	)
{
	bUseViewOwnerDepthPriorityGroup = bNewUseViewOwnerDepthPriorityGroup;
	ViewOwnerDepthPriorityGroup = NewViewOwnerDepthPriorityGroup;
	BeginDeferredReattach();
}

/**
* Calculates the closest point on this primitive to a point given
* @param POI - Point in world space to determine closest point to
* @param Extent - Convex primitive 
* @param OutPointA - The point closest on the extent box
* @param OutPointB - Point on this primitive closest to the extent box
* 
* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
*/
/*GJKResult*/ BYTE UPrimitiveComponent::ClosestPointOnComponentToPoint(const FVector& POI, const FVector& Extent, FVector& OutPointA, FVector& OutPointB)
{
	if( Extent.IsZero() )
	{
		GJKHelperPoint PointHelper(POI);
		return ClosestPointOnComponentInternal(&PointHelper, OutPointA, OutPointB);
	}
	else
	{
		// Construct representation of trace box in world space (AABB like LineCheck())
		FOrientedBox ExtentBox;
		ExtentBox.Center = POI;
		ExtentBox.AxisX = FVector(1.f,0.f,0.f);
		ExtentBox.AxisY = FVector(0.f,1.f,0.f);
		ExtentBox.AxisZ = FVector(0.f,0.f,1.f);
		ExtentBox.ExtentX = Extent.X;
		ExtentBox.ExtentY = Extent.Y;
		ExtentBox.ExtentZ = Extent.Z;

		GJKHelperBox BoxHelper(ExtentBox);
		return ClosestPointOnComponentInternal(&BoxHelper, OutPointA, OutPointB);
	}
}

/**
* Calculates the closest point this component to another component
* @param PrimitiveComponent - Another Primitive Component
* @param PointOnComponentA - Point on this primitive closest to other primitive
* @param PointOnComponentB - Point on other primitive closest to this primitive
* 
* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
*/
/*GJKResult*/ BYTE UPrimitiveComponent::ClosestPointOnComponentToComponent(class UPrimitiveComponent*& OtherComponent,FVector& PointOnComponentA,FVector& PointOnComponentB)
{
	/* NOT IMPLEMENTED YET */
	return GJK_Fail;
}

///////////////////////////////////////////////////////////////////////////////
// Primitive COmponent functions copied from TransformComponent
///////////////////////////////////////////////////////////////////////////////

void UPrimitiveComponent::SetTransformedToWorld()
{
	LocalToWorld = CachedParentToWorld;

	if(AbsoluteTranslation)
	{
		LocalToWorld.M[3][0] = LocalToWorld.M[3][1] = LocalToWorld.M[3][2] = 0.0f;
	}

	if(AbsoluteRotation || AbsoluteScale)
	{
		FVector	X(LocalToWorld.M[0][0],LocalToWorld.M[0][1],LocalToWorld.M[0][2]),
				Y(LocalToWorld.M[1][0],LocalToWorld.M[1][1],LocalToWorld.M[1][2]),
				Z(LocalToWorld.M[2][0],LocalToWorld.M[2][1],LocalToWorld.M[2][2]);

		if(AbsoluteScale)
		{
			X.Normalize();
			Y.Normalize();
			Z.Normalize();
		}

		if(AbsoluteRotation)
		{
			X = FVector(X.Size(),0,0);
			Y = FVector(0,Y.Size(),0);
			Z = FVector(0,0,Z.Size());
		}

		LocalToWorld.M[0][0] = X.X;
		LocalToWorld.M[0][1] = X.Y;
		LocalToWorld.M[0][2] = X.Z;
		LocalToWorld.M[1][0] = Y.X;
		LocalToWorld.M[1][1] = Y.Y;
		LocalToWorld.M[1][2] = Y.Z;
		LocalToWorld.M[2][0] = Z.X;
		LocalToWorld.M[2][1] = Z.Y;
		LocalToWorld.M[2][2] = Z.Z;
	}

	LocalToWorld = FScaleRotationTranslationMatrix( Scale * Scale3D , Rotation, Translation) * LocalToWorld; 
	LocalToWorldDeterminant = LocalToWorld.Determinant();
}

//
//	UPrimitiveComponent::execSetTranslation
//

void UPrimitiveComponent::execSetTranslation(FFrame& Stack,RESULT_DECL)
{
	P_GET_VECTOR(NewTranslation);
	P_FINISH;

	if( NewTranslation != Translation )
	{
		Translation = NewTranslation;
		BeginDeferredUpdateTransform();
	}
}

//
//	UPrimitiveComponent::execSetRotation
//

void UPrimitiveComponent::execSetRotation(FFrame& Stack,RESULT_DECL)
{
	P_GET_ROTATOR(NewRotation);
	P_FINISH;

	if( NewRotation != Rotation )
	{
		Rotation = NewRotation;
		BeginDeferredUpdateTransform();
	}
}

//
//	UPrimitiveComponent::execSetScale
//

void UPrimitiveComponent::execSetScale(FFrame& Stack,RESULT_DECL)
{
	P_GET_FLOAT(NewScale);
	P_FINISH;

	if( NewScale != Scale )
	{
		Scale = NewScale;
		BeginDeferredUpdateTransform();
	}

}

PRAGMA_DISABLE_OPTIMIZATION
//
//	UPrimitiveComponent::execSetScale3D
//
void UPrimitiveComponent::execSetScale3D(FFrame& Stack,RESULT_DECL)
{
	P_GET_VECTOR(NewScale3D);
	P_FINISH;

	if( NewScale3D != Scale3D )
	{
		Scale3D = NewScale3D;
		BeginDeferredUpdateTransform();
	}

}

PRAGMA_ENABLE_OPTIMIZATION

//
//	UPrimitiveComponent::execSetAbsolute
//

void UPrimitiveComponent::execSetAbsolute(FFrame& Stack,RESULT_DECL)
{
	P_GET_UBOOL_OPTX(NewAbsoluteTranslation,AbsoluteTranslation);
	P_GET_UBOOL_OPTX(NewAbsoluteRotation,AbsoluteRotation);
	P_GET_UBOOL_OPTX(NewAbsoluteScale,AbsoluteScale);
	P_FINISH;

	AbsoluteTranslation = NewAbsoluteTranslation;
	AbsoluteRotation = NewAbsoluteRotation;
	AbsoluteScale = NewAbsoluteScale;
	BeginDeferredUpdateTransform();
}

IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetHidden);
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetTranslation);
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetRotation);
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetScale);
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetScale3D);
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetAbsolute);

UPrimitiveComponent::UPrimitiveComponent()
{
	MotionBlurInfoIndex = INDEX_NONE;
	CachedParentToWorld.SetIdentity();
}


void UPrimitiveComponent::execGetPosition(FFrame& Stack,RESULT_DECL)
{
	P_FINISH;
	*(FVector*)Result = LocalToWorld.GetOrigin();
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execGetPosition);

void UPrimitiveComponent::execGetRotation(FFrame& Stack,RESULT_DECL)
{
	P_FINISH;
	*(FRotator*)Result = LocalToWorld.Rotator();
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execGetRotation);

/**
 *	Setup the information required for rendering LightMap Density mode
 *	for this component.
 *
 *	@param	Proxy		The scene proxy for the component (information is set on it)
 *
 *	@return	UBOOL		TRUE if successful, FALSE if not.
 */
UBOOL UPrimitiveComponent::SetupLightmapResolutionViewInfo(FPrimitiveSceneProxy& Proxy) const
{
#if !CONSOLE
	if (GIsEditor)
	{
		INT Width, Height;
		Proxy.SetIsLightMapResolutionPadded(GetLightMapResolution(Width, Height));
		FVector2D TempVector(Width, Height);
		Proxy.SetLightMapResolutionScale(TempVector);
		Proxy.SetLightMapType(GetStaticLightingType());
		return TRUE;
	}
#endif
	return FALSE;
}



///////////////////////////////////////////////////////////////////////////////
// MESH COMPONENT
///////////////////////////////////////////////////////////////////////////////

/**
 * Called before destroying the object.  This is called immediately upon deciding to destroy the object, to allow the object to begin an
 * asynchronous cleanup process.
 */
void UMeshComponent::BeginDestroy()
{
	// Notify the streaming system.
	GStreamingManager->NotifyPrimitiveDetached( this );

	Super::BeginDestroy();
}

UMaterialInterface* UMeshComponent::GetMaterial(INT ElementIndex) const
{
	if(ElementIndex < Materials.Num() && Materials(ElementIndex))
	{
		return Materials(ElementIndex);
	}
	else
	{
		return NULL;
	}
}

void UMeshComponent::SetMaterial(INT ElementIndex,UMaterialInterface* Material)
{
	if (ElementIndex >= 0 && (Materials.Num() <= ElementIndex || Materials(ElementIndex) != Material))
	{
		if (Materials.Num() <= ElementIndex)
		{
			Materials.AddZeroed(ElementIndex + 1 - Materials.Num());
		}
		Materials(ElementIndex) = Material;

		// If the new material has a physical material, be sure to update the meshes body instance
#if WITH_NOVODEX
		if( Material )
		{
			UPhysicalMaterial* NewPhysicsMat = Material->GetPhysicalMaterial();
			URB_BodyInstance* BodyInstance = GetRootBodyInstance();
			if( NewPhysicsMat && BodyInstance )
			{
				BodyInstance->UpdatePhysMaterialOverride();
			}
		}
#endif
		BeginDeferredReattach();
	}
}

FMaterialViewRelevance UMeshComponent::GetMaterialViewRelevance() const
{
	// Combine the material relevance for all materials.
	FMaterialViewRelevance Result;
	for(INT ElementIndex = 0;ElementIndex < GetNumElements();ElementIndex++)
	{
		UMaterialInterface* MaterialInterface = GetMaterial(ElementIndex);
		if(!MaterialInterface)
		{
			MaterialInterface = GEngine->DefaultMaterial;
		}
		Result |= MaterialInterface->GetViewRelevance();
	}
	return Result;
}

/**
 *	Tell the streaming system to start loading all textures with all mip-levels.
 *	@param Seconds							Number of seconds to force all mip-levels to be resident
 *	@param bPrioritizeCharacterTextures		Whether character textures should be prioritized for a while by the streaming system
 *	@param CinematicTextureGroups			Bitfield indicating which texture groups that use extra high-resolution mips
 */
void UMeshComponent::PrestreamTextures( FLOAT Seconds, UBOOL bPrioritizeCharacterTextures, INT CinematicTextureGroups )
{
	// If requested, tell the streaming system to only process character textures for 30 frames.
	if ( bPrioritizeCharacterTextures )
	{
		GStreamingManager->SetDisregardWorldResourcesForFrames( 30 );
	}

	INT NumElements = GetNumElements();
	for ( INT ElementIndex=0; ElementIndex < NumElements; ++ElementIndex )
	{
		UMaterialInterface* Material = GetMaterial( ElementIndex );
		if ( Material )
		{
			Material->SetForceMipLevelsToBeResident( FALSE, FALSE, Seconds, CinematicTextureGroups );
		}
	}
}

/**
 *	Tell the streaming system whether or not all mip levels of all textures used by this component should be loaded and remain loaded.
 *	@param bForceMiplevelsToBeResident		Whether textures should be forced to be resident or not.
 */
void UMeshComponent::SetTextureForceResidentFlag( UBOOL bForceMiplevelsToBeResident )
{
	const INT CinematicTextureGroups = 0;

	INT NumElements = GetNumElements();
	for ( INT ElementIndex=0; ElementIndex < NumElements; ++ElementIndex )
	{
		UMaterialInterface* Material = GetMaterial( ElementIndex );
		if ( Material )
		{
			Material->SetForceMipLevelsToBeResident( TRUE, bForceMiplevelsToBeResident, -1.0f, CinematicTextureGroups );
		}
	}
}

void UMeshComponent::execGetMaterial(FFrame& Stack,RESULT_DECL)
{
	P_GET_INT(SkinIndex);
	P_FINISH;

	*(UMaterialInterface**)Result = GetMaterial(SkinIndex);
}

IMPLEMENT_FUNCTION(UMeshComponent,INDEX_NONE,execGetMaterial);

void UMeshComponent::execSetMaterial(FFrame& Stack,RESULT_DECL)
{
	P_GET_INT(SkinIndex);
	P_GET_OBJECT(UMaterialInterface,Material);
	P_FINISH;
	SetMaterial(SkinIndex,Material);
}

IMPLEMENT_FUNCTION(UMeshComponent,INDEX_NONE,execSetMaterial);

void UMeshComponent::execGetNumElements(FFrame& Stack,RESULT_DECL)
{
	P_FINISH;
	*(INT*)Result = GetNumElements();
}

IMPLEMENT_FUNCTION(UMeshComponent,INDEX_NONE,execGetNumElements);

void UMeshComponent::execPrestreamTextures(FFrame& Stack, RESULT_DECL)
{
	P_GET_FLOAT(Seconds);
	P_GET_UBOOL(bPrioritizeCharacterTextures)
	P_GET_INT(CinematicTextureGroups)
	P_FINISH;
	PrestreamTextures(Seconds,bPrioritizeCharacterTextures,CinematicTextureGroups);
}

///////////////////////////////////////////////////////////////////////////////
// CYLINDER COMPONENT
///////////////////////////////////////////////////////////////////////////////

/**
* Creates a proxy to represent the primitive to the scene manager in the rendering thread.
* @return The proxy object.
*/
FPrimitiveSceneProxy* UCylinderComponent::CreateSceneProxy()
{
	/** Represents a UCylinderComponent to the scene manager. */
	class FDrawCylinderSceneProxy : public FPrimitiveSceneProxy
	{
	public:
		FDrawCylinderSceneProxy(const UCylinderComponent* InComponent)
			:	FPrimitiveSceneProxy(InComponent)
			,	bShouldCollide( InComponent->ShouldCollide() )
			,	bDrawNonColliding( InComponent->bDrawNonColliding )
			,	bAlwaysRenderIfSelected( InComponent->bAlwaysRenderIfSelected )
			,	bDrawBounds( InComponent->bDrawBoundingBox )
			,	BoundingBox( InComponent->Bounds.Origin - InComponent->Bounds.BoxExtent, InComponent->Bounds.Origin + InComponent->Bounds.BoxExtent )
			,	Origin( InComponent->GetOrigin() )
			,	CollisionRadius( InComponent->CollisionRadius )
			,	CollisionHeight( InComponent->CollisionHeight )
			,	CylinderColor( InComponent->CylinderColor )
		{}

		virtual void OnTransformChanged()
		{
			// update origin and move the bounding box accordingly
			FVector OldOrigin = Origin;
			Origin = LocalToWorld.GetOrigin();
			FVector Diff = Origin - OldOrigin;
			BoundingBox.Min += Diff;
			BoundingBox.Max += Diff;
		}

		/** 
		* Draw the scene proxy as a dynamic element
		*
		* @param	PDI - draw interface to render to
		* @param	View - current view
		* @param	DPGIndex - current depth priority 
		* @param	Flags - optional set of flags from EDrawDynamicElementFlags
		*/
		virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
		{
			if(bDrawBounds)
			{
				DrawWireBox( PDI, BoundingBox, FColor(255, 0, 0), SDPG_World );
			}
			FLinearColor NewCylinderColor = CylinderColor;
			if(bSelected)
			{
				NewCylinderColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
			}
			DrawWireCylinder( PDI, Origin, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), GetSelectionColor(NewCylinderColor, bSelected, bHovered), CollisionRadius, CollisionHeight, 16, SDPG_World );
		}

        virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
		{
			const UBOOL bVisible = (bSelected && bAlwaysRenderIfSelected) || ((View->Family->ShowFlags & SHOW_Collision) && (bShouldCollide || bDrawNonColliding));
			FPrimitiveViewRelevance Result;
			Result.bDynamicRelevance = IsShown(View) && bVisible;
			Result.SetDPG(SDPG_World,TRUE);
			if (IsShadowCast(View))
			{
				Result.bShadowRelevance = TRUE;
			}
			return Result;
		}
		virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
		DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

	private:
		const BITFIELD	bShouldCollide:1;
		const BITFIELD	bDrawNonColliding:1;
		const BITFIELD	bAlwaysRenderIfSelected:1;
		const BITFIELD	bDrawBounds:1;
		FBox BoundingBox;
		FVector Origin;
		const FLOAT		CollisionRadius;
		const FLOAT		CollisionHeight;
		const FColor	CylinderColor;
	};

	return new FDrawCylinderSceneProxy( this );
}

//
//	UCylinderComponent::UpdateBounds
//

void UCylinderComponent::UpdateBounds()
{
	FVector BoxPoint = FVector(CollisionRadius,CollisionRadius,CollisionHeight);
	Bounds = FBoxSphereBounds(GetOrigin(), BoxPoint, BoxPoint.Size());
}

//
//	UCylinderComponent::PointCheck
//

/** 
 *	Util for 2D box/circle test on XY plane. Using Arvo's algorithm 
 *	@return	Normal			Distance to move circle to clear the box
 *	@return	Penetration		Min distance required to move circle along Normal to clear box
 */
static UBOOL BoxCircleTest(const FVector& CircleCenter, FLOAT CircleRadius, const FVector& BoxCenter, const FVector& BoxExtent, FVector2D& Normal, FLOAT& Penetration)
{
	FLOAT S = 0.f;
	FVector2D ToCircleOrigin = FVector2D(0.f, 0.f);

	const FVector BoxMin = BoxCenter - BoxExtent;
	const FVector BoxMax = BoxCenter + BoxExtent;

	// Keep track of whether circle center is 'inside' the box
	UBOOL bInsideX = FALSE;
	UBOOL bInsideY = FALSE;

	// Test X
	if( CircleCenter.X < BoxMin.X )
	{
		ToCircleOrigin.X = CircleCenter.X - BoxMin.X;
	}
	else if( CircleCenter.X > BoxMax.X )
	{
		ToCircleOrigin.X = CircleCenter.X - BoxMax.X;
	}
	else
	{
		bInsideX = TRUE;
	}

	// Test Y
	if( CircleCenter.Y < BoxMin.Y )
	{
		ToCircleOrigin.Y = CircleCenter.Y - BoxMin.Y;
	}
	else if( CircleCenter.Y > BoxMax.Y )
	{
		ToCircleOrigin.Y = CircleCenter.Y - BoxMax.Y;
	}
	else
	{
		bInsideY = TRUE;
	}

	// Handle case where circle center is completely inside box
	if(bInsideX && bInsideY)
	{
		FLOAT MinDepth = BIG_NUMBER;

		// Look simply for surface that we are closest to - and return surface normal as contact normal
		FLOAT MaxXDepth = BoxMax.X - CircleCenter.X; 
		if(MaxXDepth < MinDepth)
		{
			MinDepth = MaxXDepth;
			Normal = FVector2D(1,0);
		}

		FLOAT MinXDepth = CircleCenter.X - BoxMin.X; 
		if(MinXDepth < MinDepth)
		{
			MinDepth = MinXDepth;
			Normal = FVector2D(-1,0);
		}

		FLOAT MaxYDepth = BoxMax.Y - CircleCenter.Y; 
		if(MaxYDepth < MinDepth)
		{
			MinDepth = MaxYDepth;
			Normal = FVector2D(0,1);
		}

		FLOAT MinYDepth = CircleCenter.Y - BoxMin.Y; 
		if(MinYDepth < MinDepth)
		{
			MinDepth = MinYDepth;
			Normal = FVector2D(0,-1);
		}

		// Penetration is MinDepth (to get origin to box surface) and then CircleRadius on top of that.
		Penetration = (CircleRadius + MinDepth);

		return TRUE;
	}
	// Circle origin is outside box
	else
	{
		// Test against circle radius
		FLOAT Dist = ToCircleOrigin.Size();

		if(Dist <= CircleRadius)
		{
			// Circle overlaps box
			Penetration = (CircleRadius - Dist); // How far to move circle so it doesn't overlap any more
			Normal = ToCircleOrigin/Dist; // Normalize vector as normal
			return TRUE;
		}
		else
		{
			return FALSE;
		}	
	}
}

UBOOL UCylinderComponent::PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags)
{
	const FVector& CylOrigin = GetOrigin();
	FVector2D Normal(0,0);
	FLOAT Penetration = 0.f;
	if (	Owner
		&&	Square(CylOrigin.Z - Location.Z) < Square(CollisionHeight + Extent.Z)
		&&	BoxCircleTest(CylOrigin, CollisionRadius, Location, Extent, Normal, Penetration) )
	{
		// Hit.
		Result.Actor = Owner;
		Result.Component = this;

		// If box is mostly above or below the cylinder, move it along Z to clear it
		FVector CylToPoint = (Location - CylOrigin).SafeNormal();
		FLOAT ZDiff = (Location - CylOrigin).Z;
		if (CylToPoint.Z < -0.5f)
		{
			Result.Normal = FVector(0,0,-1);
			Result.Location = Location + FVector(0.f, 0.f, -ZDiff - (CollisionHeight + Extent.Z));
		}
		else if (CylToPoint.Z > +0.5f)
		{
			Result.Normal = FVector(0,0,1);
			Result.Location = Location - FVector(0.f, 0.f, ZDiff - (CollisionHeight + Extent.Z));
		}
		// We are more to the side, move along X and Y to clear cylinder.
		else
		{
			Result.Normal = FVector(Normal.X, Normal.Y, 0.f);
			Result.Location = Location - (Penetration * Result.Normal);
		}

		return 0;
	}
	else
	{
		return 1;
	}
}

//
//	UCylinderComponent::LineCheck
//

UBOOL UCylinderComponent::LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags)
{
	Result.Time = 1.f;

	// Ensure always a valid normal.
	Result.Normal = FVector(0,0,1);

	if( !Owner )
		return 1;

	// Treat this actor as a cylinder.
	FVector CylExtent( CollisionRadius, CollisionRadius, CollisionHeight );
	FVector NetExtent = Extent + CylExtent;
	const FVector& CylOrigin = GetOrigin();

	// Quick X reject.
	FLOAT MaxX = CylOrigin.X + NetExtent.X;
	if( Start.X>MaxX && End.X>MaxX )
		return 1;
	FLOAT MinX = CylOrigin.X - NetExtent.X;
	if( Start.X<MinX && End.X<MinX )
		return 1;

	// Quick Y reject.
	FLOAT MaxY = CylOrigin.Y + NetExtent.Y;
	if( Start.Y>MaxY && End.Y>MaxY )
		return 1;
	FLOAT MinY = CylOrigin.Y - NetExtent.Y;
	if( Start.Y<MinY && End.Y<MinY )
		return 1;

	// Quick Z reject.
	FLOAT TopZ = CylOrigin.Z + NetExtent.Z;
	if( Start.Z>TopZ && End.Z>TopZ )
		return 1;
	FLOAT BotZ = CylOrigin.Z - NetExtent.Z;
	if( Start.Z<BotZ && End.Z<BotZ )
		return 1;

	// Clip to top of cylinder.
	FLOAT T0=0.f, T1=1.f;
	if( Start.Z>TopZ && End.Z<TopZ )
	{
		FLOAT T = (TopZ - Start.Z)/(End.Z - Start.Z);
		if( T > T0 )
		{
			T0 = ::Max(T0,T);
			Result.Normal = FVector(0,0,1);
		}
	}
	else if( Start.Z<TopZ && End.Z>TopZ )
		T1 = ::Min( T1, (TopZ - Start.Z)/(End.Z - Start.Z) );

	// Clip to bottom of cylinder.
	if( Start.Z<BotZ && End.Z>BotZ )
	{
		FLOAT T = (BotZ - Start.Z)/(End.Z - Start.Z);
		if( T > T0 )
		{
			T0 = ::Max(T0,T);
			Result.Normal = FVector(0,0,-1);
		}
	}
	else if( Start.Z>BotZ && End.Z<BotZ )
		T1 = ::Min( T1, (BotZ - Start.Z)/(End.Z - Start.Z) );

	// Reject.
	if( T0 >= T1 )
		return 1;

	// Test setup.
	FLOAT   Kx        = Start.X - CylOrigin.X;
	FLOAT   Ky        = Start.Y - CylOrigin.Y;

	// 2D circle clip about origin.
	FLOAT   Vx        = End.X - Start.X;
	FLOAT   Vy        = End.Y - Start.Y;
	FLOAT   A         = Vx*Vx + Vy*Vy;
	FLOAT   B         = 2.f * (Kx*Vx + Ky*Vy);
	FLOAT   C         = Kx*Kx + Ky*Ky - Square(NetExtent.X);
	FLOAT   Discrim   = B*B - 4.f*A*C;

	// If already inside sphere, oppose further movement inward.
	if( C<Square(1.f) && Start.Z>BotZ && Start.Z<TopZ )
	{
		FLOAT Dir = ((End-Start)*FVector(1,1,0)) | (Start - CylOrigin);
		if( Dir < -0.1f )
		{
			Result.Time      = 0.f;
			Result.Location  = Start;
			Result.Normal    = ((Start - CylOrigin) * FVector(1,1,0)).SafeNormal();
			Result.Actor     = Owner;
			Result.Component = this;
			Result.Material = NULL;
			return 0;
		}
		else return 1;
	}

	// No intersection if discriminant is negative.
	if( Discrim < 0 )
		return 1;

	// Unstable intersection if velocity is tiny.
	if( A < Square(0.0001f) )
	{
		// Outside.
		if( C > 0 )
			return 1;
	}
	else
	{
		// Compute intersection times.
		Discrim   = appSqrt(Discrim);
		FLOAT R2A = 0.5/A;
		T1        = ::Min( T1, +(Discrim-B) * R2A );
		FLOAT T   = -(Discrim+B) * R2A;
		if( T > T0 )
		{
			T0 = T;
			Result.Normal   = (Start + (End-Start)*T0 - CylOrigin);
			Result.Normal.Z = 0;
			Result.Normal.Normalize();
		}
		if( T0 >= T1 )
			return 1;
	}

	if (TraceFlags & TRACE_Accurate)
	{
		Result.Time = Clamp(T0,0.0f,1.0f);
	}
	else
	{
		Result.Time = Clamp(T0-0.001f,0.f,1.f);
	}
	Result.Location  = Start + (End-Start) * Result.Time;
	Result.Actor     = Owner;
	Result.Component = this;
	return 0;

}

/**		**INTERNAL USE ONLY**
* Implementation required by a primitive component in order to properly work with the closest points algorithms below
* Given an interface to some other primitive, return the points on each object closest to each other
* @param ExtentHelper - Interface class returning the supporting points on some other primitive type
* @param OutPointA - The point closest on the 'other' primitive
* @param OutPointB - The point closest on this primitive
* 
* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
*/
BYTE UCylinderComponent::ClosestPointOnComponentInternal(IGJKHelper* ExtentHelper, FVector& OutPointA, FVector& OutPointB)
{
	GJKResult Result;
	GJKHelperCylinder CylHelper(CollisionHeight, CollisionRadius, LocalToWorld);

	//FVector Origin = LocalToWorld.GetOrigin();
	//Origin.Z += CylHelper.Height * 0.5f;
	//DrawWireCylinder(GWorld->LineBatcher, Origin, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FLinearColor(1,1,1,1), CylHelper.Radius, CylHelper.Height * 0.5f, 25, SDPG_World);

	Result = ClosestPointsBetweenConvexPrimitives(ExtentHelper, &CylHelper, OutPointA, OutPointB);
	//if (Result != GJK_Intersect)
	//{
	//	GWorld->LineBatcher->DrawLine(OutPointA, OutPointB, FLinearColor(0,1,0,1), SDPG_World);
	//}

	return Result;
}

/**
* Calculates the closest point this component to another component
* @param PrimitiveComponent - Another Primitive Component
* @param PointOnComponentA - Point on this primitive closest to other primitive
* @param PointOnComponentB - Point on other primitive closest to this primitive
* 
* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
*/
/*GJKResult*/ BYTE UCylinderComponent::ClosestPointOnComponentToComponent(class UPrimitiveComponent*& OtherComponent,FVector& PointOnComponentA,FVector& PointOnComponentB)
{
	GJKResult Result;
	GJKHelperCylinder CylHelper(CollisionHeight, CollisionRadius, LocalToWorld);

	//FVector Origin = LocalToWorld.GetOrigin();
	//Origin.Z += CylHelper.Height * 0.5f;
	//DrawWireCylinder(GWorld->LineBatcher, Origin, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FLinearColor(1,1,1,1), CylHelper.Radius, CylHelper.Height * 0.5f, 25, SDPG_World);

	Result = (GJKResult)OtherComponent->ClosestPointOnComponentInternal(&CylHelper, PointOnComponentA, PointOnComponentB);
	//if (Result != GJK_Intersect)
	//{
	//	GWorld->LineBatcher->DrawLine(PointOnComponentA, PointOnComponentB, FLinearColor(0,1,0,1), SDPG_World);
	//}

	return Result;
}

//
//	UCylinderComponent::SetCylinderSize
//

void UCylinderComponent::SetCylinderSize(FLOAT NewRadius, FLOAT NewHeight)
{
	CollisionHeight = NewHeight;
	CollisionRadius = NewRadius;
	BeginDeferredReattach();
}

//
//	UCylinderComponent::execSetSize
//

void UCylinderComponent::execSetCylinderSize( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(NewRadius);
	P_GET_FLOAT(NewHeight);
	P_FINISH;

	SetCylinderSize(NewRadius, NewHeight);

}
IMPLEMENT_FUNCTION(UCylinderComponent,INDEX_NONE,execSetCylinderSize);

///////////////////////////////////////////////////////////////////////////////
// ARROW COMPONENT
///////////////////////////////////////////////////////////////////////////////

#define ARROW_SCALE	16.0f

/** Represents a UArrowComponent to the scene manager. */
class FArrowSceneProxy : public FPrimitiveSceneProxy
{
public:

	FArrowSceneProxy(UArrowComponent* Component):
		FPrimitiveSceneProxy(Component),
		ArrowColor(Component->ArrowColor),
		ArrowSize(Component->ArrowSize),
		bTreatAsASprite(Component->bTreatAsASprite)
	{
#if WITH_EDITORONLY_DATA
		// If in the editor, extract the sprite category from the component
		if ( GIsEditor )
		{
			SpriteCategoryIndex = GEngine->GetSpriteCategoryIndex( Component->SpriteCategoryName );
		}
#endif // if !CONSOLE
	}

	// FPrimitiveSceneProxy interface.
	
	/** 
	* Draw the scene proxy as a dynamic element
	*
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param	DPGIndex - current depth priority 
	* @param	Flags - optional set of flags from EDrawDynamicElementFlags
	*/
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
	{
		// Determine the DPG the primitive should be drawn in for this view.
		if (GetDepthPriorityGroup(View) == DPGIndex)
		{
			DrawDirectionalArrow(PDI,LocalToWorld,ArrowColor,ArrowSize * 3.0f,1.0f,DPGIndex);
		}
	}
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		FPrimitiveViewRelevance Result;
		Result.bDynamicRelevance = IsShown(View);
		if (bTreatAsASprite)
		{
			if ((View->Family->ShowFlags & SHOW_Sprites) == 0)
			{
				Result.bDynamicRelevance = FALSE;
			}
#if !CONSOLE
			else if ( GIsEditor && SpriteCategoryIndex != INDEX_NONE && SpriteCategoryIndex < View->SpriteCategoryVisibility.Num() && !View->SpriteCategoryVisibility( SpriteCategoryIndex ) )
			{
				Result.bDynamicRelevance = FALSE;
			}
#endif
		}
		Result.SetDPG(GetDepthPriorityGroup(View),TRUE);
		if (IsShadowCast(View))
		{
			Result.bShadowRelevance = TRUE;
		}
		return Result;
	}
	virtual void OnTransformChanged()
	{
		LocalToWorld = FScaleMatrix(FVector(ARROW_SCALE,ARROW_SCALE,ARROW_SCALE)) * LocalToWorld;
	}
	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:
	const FColor	ArrowColor;
	const FLOAT		ArrowSize;
	const UBOOL		bTreatAsASprite;
#if !CONSOLE
	INT				SpriteCategoryIndex;
#endif // #if !CONSOLE
};

FPrimitiveSceneProxy* UArrowComponent::CreateSceneProxy()
{
	return new FArrowSceneProxy(this);
}

//
//	UArrowComponent::UpdateBounds
//

void UArrowComponent::UpdateBounds()
{
	Bounds = FBoxSphereBounds(FBox(FVector(0,-ARROW_SCALE,-ARROW_SCALE),FVector(ArrowSize * ARROW_SCALE * 3.0f,ARROW_SCALE,ARROW_SCALE))).TransformBy(LocalToWorld);
}

///////////////////////////////////////////////////////////////////////////////
// SPHERE COMPONENT
///////////////////////////////////////////////////////////////////////////////

/**
 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
 * @return The proxy object.
 */
FPrimitiveSceneProxy* UDrawSphereComponent::CreateSceneProxy()
{
	/** Represents a DrawLightRadiusComponent to the scene manager. */
	class FDrawSphereSceneProxy : public FPrimitiveSceneProxy
	{
	public:

		/** Initialization constructor. */
		FDrawSphereSceneProxy(const UDrawSphereComponent* InComponent)
			: FPrimitiveSceneProxy(InComponent),
			  SphereColor(InComponent->SphereColor),
			  SphereMaterial(InComponent->SphereMaterial),
			  SphereRadius(InComponent->SphereRadius),
			  SphereSides(InComponent->SphereSides),
			  bDrawWireSphere(InComponent->bDrawWireSphere),
			  bDrawLitSphere(InComponent->bDrawLitSphere),
			  bDrawOnlyIfSelected(InComponent->bDrawOnlyIfSelected)
		{}

		  // FPrimitiveSceneProxy interface.
		  
		/** 
		* Draw the scene proxy as a dynamic element
		*
		* @param	PDI - draw interface to render to
		* @param	View - current view
		* @param	DPGIndex - current depth priority 
		* @param	Flags - optional set of flags from EDrawDynamicElementFlags
		*/
		virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
		{
			if( bDrawWireSphere )
			{
				DrawCircle( PDI, LocalToWorld.GetOrigin(), LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(1), SphereColor, SphereRadius, SphereSides, SDPG_World );
				DrawCircle( PDI, LocalToWorld.GetOrigin(), LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(2), SphereColor, SphereRadius, SphereSides, SDPG_World );
				DrawCircle( PDI, LocalToWorld.GetOrigin(), LocalToWorld.GetAxis(1), LocalToWorld.GetAxis(2), SphereColor, SphereRadius, SphereSides, SDPG_World );
			}

			if(bDrawLitSphere && SphereMaterial && !(View->Family->ShowFlags & SHOW_Wireframe))
			{
				  DrawSphere(PDI,LocalToWorld.GetOrigin(), FVector(SphereRadius), SphereSides, SphereSides/2, SphereMaterial->GetRenderProxy(FALSE), SDPG_World);
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
		{
			const UBOOL bVisible = (bDrawWireSphere || bDrawLitSphere) && (bSelected || !bDrawOnlyIfSelected);
			FPrimitiveViewRelevance Result;
			Result.bDynamicRelevance = IsShown(View) && bVisible;
			Result.SetDPG( SDPG_World,TRUE );
			if (IsShadowCast(View))
			{
				Result.bShadowRelevance = TRUE;
			}
			return Result;
		}

		virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
		DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

	private:
		const FColor	SphereColor;
		const UMaterialInterface*	SphereMaterial;
		const FLOAT		SphereRadius;
		const INT		SphereSides;
		const BITFIELD				bDrawWireSphere:1;
		const BITFIELD				bDrawLitSphere:1;
		const BITFIELD				bDrawOnlyIfSelected:1;
	};

	return new FDrawSphereSceneProxy( this );
}

void UDrawSphereComponent::UpdateBounds()
{
	Bounds = FBoxSphereBounds( FVector(0,0,0), FVector(SphereRadius), SphereRadius ).TransformBy(LocalToWorld);
}

/** 
 * Retrieves the materials used in this component 
 * 
 * @param OutMaterials	The list of used materials.
 */
void UDrawSphereComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
	OutMaterials.AddItem( SphereMaterial );
}

///////////////////////////////////////////////////////////////////////////////
// CYLINDER COMPONENT
///////////////////////////////////////////////////////////////////////////////

/**
* Creates a proxy to represent the primitive to the scene manager in the rendering thread.
* @return The proxy object.
*/
FPrimitiveSceneProxy* UDrawCylinderComponent::CreateSceneProxy()
{
	/** Represents a DrawCylinderComponent to the scene manager. */
	class FDrawCylinderSceneProxy : public FPrimitiveSceneProxy
	{
	public:

		/** Initialization constructor. */
		FDrawCylinderSceneProxy(const UDrawCylinderComponent* InComponent)
			: FPrimitiveSceneProxy(InComponent),
			CylinderColor(InComponent->CylinderColor),
			CylinderMaterial(InComponent->CylinderMaterial),
			CylinderRadius(InComponent->CylinderRadius),
			CylinderTopRadius(InComponent->CylinderTopRadius),
			CylinderHeight(InComponent->CylinderHeight),
			CylinderHeightOffset(InComponent->CylinderHeightOffset),
			CylinderSides(InComponent->CylinderSides),
			bDrawWireCylinder(InComponent->bDrawWireCylinder),
			bDrawLitCylinder(InComponent->bDrawLitCylinder),
			bDrawOnlyIfSelected(InComponent->bDrawOnlyIfSelected)
		{}

		// FPrimitiveSceneProxy interface.
		virtual void DrawDynamicElements(
			FPrimitiveDrawInterface* PDI,
			const FSceneView* View,
			UINT InDepthPriorityGroup,
			DWORD Flags
			)
		{
			if(InDepthPriorityGroup == SDPG_World)
			{
				FLOAT HalfHeight = CylinderHeight / 2.0f;
				FVector Base = LocalToWorld.GetOrigin() + LocalToWorld.GetAxis(2) * CylinderHeightOffset;

				if( bDrawWireCylinder )
				{
					DrawWireChoppedCone(PDI,Base, LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(1),
						LocalToWorld.GetAxis(2), CylinderColor, CylinderRadius, CylinderTopRadius, HalfHeight, CylinderSides, SDPG_World);

					
				}

				if(bDrawLitCylinder && CylinderMaterial && !(View->Family->ShowFlags & SHOW_Wireframe))
				{
					/*DrawChoppedCone(PDI,Base, LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(1),
						LocalToWorld.GetAxis(2), CylinderRadius, HalfHeight, CylinderSides, CylinderMaterial->GetInstanceInterface(FALSE), SDPG_World);*/

					//TODO
				}
			}

			RenderBounds(PDI, InDepthPriorityGroup, View->Family->ShowFlags, PrimitiveSceneInfo->Bounds, bSelected);
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
		{
			const UBOOL bVisible = (bDrawWireCylinder || bDrawLitCylinder) && (bSelected || !bDrawOnlyIfSelected);
			FPrimitiveViewRelevance Result;
			Result.bDynamicRelevance = IsShown(View) && bVisible;
			Result.SetDPG( SDPG_World,TRUE );

			SetRelevanceForShowBounds(View->Family->ShowFlags, Result);
			if(View->Family->ShowFlags & SHOW_Bounds)
			{
				Result.bDynamicRelevance = TRUE;
			}

			return Result;
		}

		virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
		DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

	private:
		const FColor				CylinderColor;
		const UMaterialInstance*	CylinderMaterial;
		const FLOAT					CylinderRadius;
		const FLOAT					CylinderTopRadius;
		const FLOAT					CylinderHeight;
		const FLOAT					CylinderHeightOffset;
		const INT					CylinderSides;
		const BITFIELD				bDrawWireCylinder:1;
		const BITFIELD				bDrawLitCylinder:1;
		const BITFIELD				bDrawOnlyIfSelected:1;
	};

	return new FDrawCylinderSceneProxy( this );
}

void UDrawCylinderComponent::UpdateBounds()
{
	FLOAT MaxRadius = Max(CylinderRadius, CylinderTopRadius);

	FVector Extent = FVector(MaxRadius, MaxRadius, 0.5f * CylinderHeight);
	FVector OffsetVec = FVector(0.0f, 0.0f, CylinderHeightOffset);

	Bounds = FBoxSphereBounds( FBox( (-Extent) + OffsetVec, Extent + OffsetVec ) ).TransformBy(LocalToWorld);
}

/** 
 * Retrieves the materials used in this component 
 * 
 * @param OutMaterials	The list of used materials.
 */
void UDrawCylinderComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
	OutMaterials.AddItem( CylinderMaterial );
}

///////////////////////////////////////////////////////////////////////////////
// BOX COMPONENT
///////////////////////////////////////////////////////////////////////////////

/**
* Creates a proxy to represent the primitive to the scene manager in the rendering thread.
* @return The proxy object.
*/
FPrimitiveSceneProxy* UDrawBoxComponent::CreateSceneProxy()
{
	/** Represents a DrawBoxComponent to the scene manager. */
	class FDrawBoxSceneProxy : public FPrimitiveSceneProxy
	{
	public:

		/** Initialization constructor. */
		FDrawBoxSceneProxy(const UDrawBoxComponent* InComponent)
			: FPrimitiveSceneProxy(InComponent),
			BoxColor(InComponent->BoxColor),
			BoxMaterial(InComponent->BoxMaterial),
			BoxExtent(InComponent->BoxExtent),
			bDrawWireBox(InComponent->bDrawWireBox),
			bDrawLitBox(InComponent->bDrawLitBox),
			bDrawOnlyIfSelected(InComponent->bDrawOnlyIfSelected)
		{}

		// FPrimitiveSceneProxy interface.
		virtual void DrawDynamicElements(
			FPrimitiveDrawInterface* PDI,
			const FSceneView* View,
			UINT InDepthPriorityGroup,
			DWORD Flags
			)
		{
			if(InDepthPriorityGroup == SDPG_World)
			{
				FVector Base = LocalToWorld.GetOrigin();

				if( bDrawWireBox )
				{
					FBox Box(Base - BoxExtent, Base + BoxExtent);
					//DrawWireBox(PDI, Box, BoxColor, SDPG_World);
					DrawOrientedWireBox(PDI, Base, LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(1),
						LocalToWorld.GetAxis(2), BoxExtent, BoxColor, SDPG_World);
				}

				if(bDrawLitBox && BoxMaterial && !(View->Family->ShowFlags & SHOW_Wireframe))
				{
					//TODO
				}
			}

			RenderBounds(PDI, InDepthPriorityGroup, View->Family->ShowFlags, PrimitiveSceneInfo->Bounds, bSelected);
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
		{
			const UBOOL bVisible = (bDrawWireBox || bDrawLitBox) && (bSelected || !bDrawOnlyIfSelected);
			FPrimitiveViewRelevance Result;
			Result.bDynamicRelevance = IsShown(View) && bVisible;
			Result.SetDPG( SDPG_World,TRUE );

			SetRelevanceForShowBounds(View->Family->ShowFlags, Result);
			if(View->Family->ShowFlags & SHOW_Bounds)
			{
				Result.bDynamicRelevance = TRUE;
			}
			
			return Result;
		}

		virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
		DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

	private:
		const FColor				BoxColor;
		const UMaterialInstance*	BoxMaterial;
		const FVector				BoxExtent;
		const BITFIELD				bDrawWireBox:1;
		const BITFIELD				bDrawLitBox:1;
		const BITFIELD				bDrawOnlyIfSelected:1;
	};

	return new FDrawBoxSceneProxy( this );
}

void UDrawBoxComponent::UpdateBounds()
{
	Bounds = FBoxSphereBounds( FBox( (-BoxExtent), BoxExtent ) ).TransformBy(LocalToWorld);
}

/** 
 * Retrieves the materials used in this component 
 * 
 * @param OutMaterials	The list of used materials.
 */
void UDrawBoxComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
	OutMaterials.AddItem( BoxMaterial );
}

///////////////////////////////////////////////////////////////////////////////
// CAPSULE COMPONENT
///////////////////////////////////////////////////////////////////////////////
//TODO adapt, it's copied from box now
/**
* Creates a proxy to represent the primitive to the scene manager in the rendering thread.
* @return The proxy object.
*/
FPrimitiveSceneProxy* UDrawCapsuleComponent::CreateSceneProxy()
{
	/** Represents a DrawCapsuleComponent to the scene manager. */
	class FDrawCapsuleSceneProxy : public FPrimitiveSceneProxy
	{
	public:

		/** Initialization constructor. */
		FDrawCapsuleSceneProxy(const UDrawCapsuleComponent* InComponent)
			: FPrimitiveSceneProxy(InComponent),
			CapsuleColor(InComponent->CapsuleColor),
			CapsuleMaterial(InComponent->CapsuleMaterial),
			CapsuleRadius(InComponent->CapsuleRadius),
			CapsuleHeight(InComponent->CapsuleHeight),
			bDrawWireCapsule(InComponent->bDrawWireCapsule),
			bDrawLitCapsule(InComponent->bDrawLitCapsule),
			bDrawOnlyIfSelected(InComponent->bDrawOnlyIfSelected)
		{}

		// FPrimitiveSceneProxy interface.
		virtual void DrawDynamicElements(
			FPrimitiveDrawInterface* PDI,
			const FSceneView* View,
			UINT InDepthPriorityGroup,
			DWORD Flags
			)
		{
			if(InDepthPriorityGroup == SDPG_World)
			{
				FVector Base = LocalToWorld.GetOrigin();
				FVector Offset = FVector(0,CapsuleHeight/2.0f,0);
				FMatrix rot(LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(1), LocalToWorld.GetAxis(2), FVector(0,0,0));
				Offset = rot.TransformFVector(Offset);

				if( bDrawWireCapsule )
				{
					DrawCircle(PDI, Base-Offset,LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(1),CapsuleColor,CapsuleRadius,32,SDPG_World);	
					DrawCircle(PDI, Base-Offset,LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(2),CapsuleColor,CapsuleRadius,32,SDPG_World);	
					DrawCircle(PDI, Base-Offset,LocalToWorld.GetAxis(1), LocalToWorld.GetAxis(2),CapsuleColor,CapsuleRadius,32,SDPG_World);	
					DrawCircle(PDI, Base+Offset,LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(1),CapsuleColor,CapsuleRadius,32,SDPG_World);	
					DrawCircle(PDI, Base+Offset,LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(2),CapsuleColor,CapsuleRadius,32,SDPG_World);	
					DrawCircle(PDI, Base+Offset,LocalToWorld.GetAxis(1), LocalToWorld.GetAxis(2),CapsuleColor,CapsuleRadius,32,SDPG_World);	
					DrawWireCylinder( PDI, Base, LocalToWorld.GetAxis(0), -LocalToWorld.GetAxis(2), LocalToWorld.GetAxis(1), CapsuleColor, CapsuleRadius, CapsuleHeight/2.0, 16, SDPG_World );	
				}

				if(bDrawLitCapsule && CapsuleMaterial && !(View->Family->ShowFlags & SHOW_Wireframe))
				{
					//TODO
				}
			}

			RenderBounds(PDI, InDepthPriorityGroup, View->Family->ShowFlags, PrimitiveSceneInfo->Bounds, bSelected);
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
		{
			const UBOOL bVisible = (bDrawWireCapsule || bDrawLitCapsule) && (bSelected || !bDrawOnlyIfSelected);
			FPrimitiveViewRelevance Result;
			Result.bDynamicRelevance = IsShown(View) && bVisible;
			Result.SetDPG( SDPG_World,TRUE );

			SetRelevanceForShowBounds(View->Family->ShowFlags, Result);
			if(View->Family->ShowFlags & SHOW_Bounds)
			{
				Result.bDynamicRelevance = TRUE;
			}
			
			return Result;
		}

		virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
		DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

	private:
		const FColor				CapsuleColor;
		const UMaterialInstance*	CapsuleMaterial;
		const FLOAT					CapsuleRadius;
		const FLOAT					CapsuleHeight;
		const BITFIELD				bDrawWireCapsule:1;
		const BITFIELD				bDrawLitCapsule:1;
		const BITFIELD				bDrawOnlyIfSelected:1;
	};

	return new FDrawCapsuleSceneProxy( this );
}

void UDrawCapsuleComponent::UpdateBounds()
{
	FVector Extent = FVector(CapsuleRadius, CapsuleRadius, CapsuleHeight/2.0f + CapsuleRadius);
	Bounds = FBoxSphereBounds( FBox( -Extent, Extent ) ).TransformBy(LocalToWorld);
	
}

/** 
 * Retrieves the materials used in this component 
 * 
 * @param OutMaterials	The list of used materials.
 */
void UDrawCapsuleComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
	OutMaterials.AddItem( CapsuleMaterial );
}

///////////////////////////////////////////////////////////////////////////////
// CONE COMPONENTS
///////////////////////////////////////////////////////////////////////////////
/** Represents a DrawConeComponent to the scene manager. */
class FDrawConeSceneProxy : public FPrimitiveSceneProxy
{
public:

	/** Initialization constructor. */
	FDrawConeSceneProxy(const UDrawConeComponent* InComponent):
	  FPrimitiveSceneProxy(InComponent),
	  ConeColor(InComponent->ConeColor),
	  ConeSides(InComponent->ConeSides),
	  ConeRadius(InComponent->ConeRadius),
	  ConeAngle(InComponent->ConeAngle)
	{}

	/** 
	* Draw the scene proxy as a dynamic element
	*
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param	DPGIndex - current depth priority 
	* @param	Flags - optional set of flags from EDrawDynamicElementFlags
	*/
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
	{
		//early out if not selected at all
		if (!bSelected)
		{
			return;
		}

		TArray<FVector> Verts;

		DrawWireCone(PDI, LocalToWorld, ConeRadius, ConeAngle, ConeSides, ConeColor, SDPG_World, Verts);
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		const UBOOL bVisible = (View->Family->ShowFlags & SHOW_LightRadius) != 0;
		FPrimitiveViewRelevance Result;
		Result.bDynamicRelevance = IsShown(View) && bVisible;
		Result.SetDPG(SDPG_World,TRUE);
		if (IsShadowCast(View))
		{
			Result.bShadowRelevance = TRUE;
		}
		return Result;
	}

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:
	
	/** Color of the cone. */
	const FLinearColor ConeColor;

	/** Number of sides in the cone. */
	const INT ConeSides;

	/** Radius of the cone. */
	const FLOAT ConeRadius;

	/** Angle of the cone. */
	const FLOAT ConeAngle;
};

///////////////////////////////////////////////////////////////////////////////
// DRAW CONE COMPONENT
///////////////////////////////////////////////////////////////////////////////

FPrimitiveSceneProxy* UDrawConeComponent::CreateSceneProxy()
{
	return new FDrawConeSceneProxy(this);
}

void UDrawConeComponent::UpdateBounds()
{
	Bounds = FBoxSphereBounds( FVector(0,0,0), FVector(ConeRadius), ConeRadius ).TransformBy(LocalToWorld);
}

///////////////////////////////////////////////////////////////////////////////
// LIGHT CONE COMPONENT
///////////////////////////////////////////////////////////////////////////////

FPrimitiveSceneProxy* UDrawLightConeComponent::CreateSceneProxy()
{
	return new FDrawConeSceneProxy(this);
}

///////////////////////////////////////////////////////////////////////////////
// CAMERA CONE COMPONENT
///////////////////////////////////////////////////////////////////////////////

/**
 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
 * @return The proxy object.
 */
FPrimitiveSceneProxy* UCameraConeComponent::CreateSceneProxy()
{
	/** Represents a UCameraConeComponent to the scene manager. */
	class FCameraConeSceneProxy : public FPrimitiveSceneProxy
	{
	public:
		FCameraConeSceneProxy(const UCameraConeComponent* InComponent)
			:	FPrimitiveSceneProxy( InComponent )
		{}

		/** 
		* Draw the scene proxy as a dynamic element
		*
		* @param	PDI - draw interface to render to
		* @param	View - current view
		* @param	DPGIndex - current depth priority 
		* @param	Flags - optional set of flags from EDrawDynamicElementFlags
		*/
		virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
		{
			// Camera View Cone
			const FVector Direction(1,0,0);
			const FVector UpVector(0,1,0);
			const FVector LeftVector(0,0,1);

			FVector Verts[8];

			Verts[0] = (Direction * 24) + (32 * (UpVector + LeftVector).SafeNormal());
			Verts[1] = (Direction * 24) + (32 * (UpVector - LeftVector).SafeNormal());
			Verts[2] = (Direction * 24) + (32 * (-UpVector - LeftVector).SafeNormal());
			Verts[3] = (Direction * 24) + (32 * (-UpVector + LeftVector).SafeNormal());

			Verts[4] = (Direction * 128) + (64 * (UpVector + LeftVector).SafeNormal());
			Verts[5] = (Direction * 128) + (64 * (UpVector - LeftVector).SafeNormal());
			Verts[6] = (Direction * 128) + (64 * (-UpVector - LeftVector).SafeNormal());
			Verts[7] = (Direction * 128) + (64 * (-UpVector + LeftVector).SafeNormal());

			for( INT x = 0 ; x < 8 ; ++x )
			{
				Verts[x] = LocalToWorld.TransformFVector( Verts[x] );
			}

			const FColor ConeColor( 150, 200, 255 );
			PDI->DrawLine( Verts[0], Verts[1], ConeColor, SDPG_World );
			PDI->DrawLine( Verts[1], Verts[2], ConeColor, SDPG_World );
			PDI->DrawLine( Verts[2], Verts[3], ConeColor, SDPG_World );
			PDI->DrawLine( Verts[3], Verts[0], ConeColor, SDPG_World );

			PDI->DrawLine( Verts[4], Verts[5], ConeColor, SDPG_World );
			PDI->DrawLine( Verts[5], Verts[6], ConeColor, SDPG_World );
			PDI->DrawLine( Verts[6], Verts[7], ConeColor, SDPG_World );
			PDI->DrawLine( Verts[7], Verts[4], ConeColor, SDPG_World );

			PDI->DrawLine( Verts[0], Verts[4], ConeColor, SDPG_World );
			PDI->DrawLine( Verts[1], Verts[5], ConeColor, SDPG_World );
			PDI->DrawLine( Verts[2], Verts[6], ConeColor, SDPG_World );
			PDI->DrawLine( Verts[3], Verts[7], ConeColor, SDPG_World );
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
		{
			FPrimitiveViewRelevance Result;
			Result.bDynamicRelevance = IsShown( View );
			Result.SetDPG( SDPG_World, TRUE );
			if (IsShadowCast(View))
			{
				Result.bShadowRelevance = TRUE;
			}
			return Result;
		}
		virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
		DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }
	};

	return IsOwnerSelected() ? new FCameraConeSceneProxy(this) : NULL;
}

//
//	UCameraConeComponent::UpdateBounds
//

void UCameraConeComponent::UpdateBounds()
{
	Bounds = FBoxSphereBounds(LocalToWorld.GetOrigin(),FVector(128,128,128),128);
}

///////////////////////////////////////////////////////////////////////////////
// QUAD COMPONENT
///////////////////////////////////////////////////////////////////////////////

/** 
 * Render this quad face primitive component
 */
void UDrawQuadComponent::Render( const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	FVector Positions[4];

	FVector UpVector(0,1,0);
	FVector LeftVector(0,0,1);
	Positions[0] = (UpVector * Height) + (LeftVector * Width);
	Positions[1] = (UpVector * Height) + (LeftVector * -Width);
	Positions[2] = (UpVector * -Height) + (LeftVector * -Width);
	Positions[3] = (UpVector * -Height) + (LeftVector * Width);

	for( INT x = 0 ; x < 4 ; ++x )
		Positions[x] = LocalToWorld.TransformFVector( Positions[x] );

	FColor LineColor(255,0,0);
	PDI->DrawLine( Positions[0], Positions[1], LineColor, SDPG_World );
	PDI->DrawLine( Positions[1], Positions[2], LineColor, SDPG_World );
	PDI->DrawLine( Positions[2], Positions[3], LineColor, SDPG_World );
	PDI->DrawLine( Positions[3], Positions[0], LineColor, SDPG_World );

	if( Texture )
	{
#if GEMINI_TODO
		FTriangleRenderInterface* TRI = PDI->GetTRI(FMatrix::Identity,new(GMainThreadMemStack) FTextureMaterialInstance(Texture->GetTexture()),(ESceneDepthPriorityGroup)DepthPriorityGroup);
		TRI->DrawQuad(
			FRawTriangleVertex(Positions[0],FVector(0,0,0),FVector(0,0,0),FVector(0,0,0),FVector2D(0,0)),
			FRawTriangleVertex(Positions[1],FVector(0,0,0),FVector(0,0,0),FVector(0,0,0),FVector2D(1,0)),
			FRawTriangleVertex(Positions[2],FVector(0,0,0),FVector(0,0,0),FVector(0,0,0),FVector2D(1,1)),
			FRawTriangleVertex(Positions[3],FVector(0,0,0),FVector(0,0,0),FVector(0,0,0),FVector2D(0,1))
			);
		TRI->Finish();
#endif
	}    
}


