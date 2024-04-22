/*=============================================================================
	SpeedTreeComponent.cpp: SpeedTreeComponent implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "EngineMaterialClasses.h"
#include "ScenePrivate.h"
#include "LevelUtils.h"
#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif
#include "SpeedTree.h"

IMPLEMENT_CLASS(USpeedTreeComponent);

#if WITH_SPEEDTREE

/**
 * Chooses the material to render on the tree.  The InstanceMaterial is preferred, then the ArchetypeMaterial, then the default material.
 * If a material doesn't have the necessary shaders, it falls back to the next less preferred material.
 * @return The most preferred material that has the necessary shaders for rendering a SpeedTree.
 */
static UMaterialInterface* GetSpeedTreeMaterial(UMaterialInterface* InstanceMaterial,UMaterialInterface* ArchetypeMaterial,UBOOL bHasStaticLighting,FMaterialViewRelevance& MaterialViewRelevance)
{
	UMaterialInterface* Result = NULL;

	// Try the instance's material first.
	if(InstanceMaterial && InstanceMaterial->CheckMaterialUsage(MATUSAGE_SpeedTree) && (!bHasStaticLighting || InstanceMaterial->CheckMaterialUsage(MATUSAGE_StaticLighting)))
	{
		Result = InstanceMaterial;
	}

	// If that failed, try the archetype's material.
	if(!Result && ArchetypeMaterial && ArchetypeMaterial->CheckMaterialUsage(MATUSAGE_SpeedTree) && (!bHasStaticLighting || ArchetypeMaterial->CheckMaterialUsage(MATUSAGE_StaticLighting)))
	{
		Result = ArchetypeMaterial;
	}

	// If both failed, use the default material.
	if(!Result)
	{
		Result = GEngine->DefaultMaterial;
	}

	// Update the material relevance information from the resulting material.
	MaterialViewRelevance |= Result->GetViewRelevance();

	return Result;
}

/** Represents the static lighting of a SpeedTreeComponent's mesh to the scene manager. */
class FSpeedTreeLCI : public FLightCacheInterface
{
public:

	/** Initialization constructor. */
	FSpeedTreeLCI(const USpeedTreeComponent* InComponent,ESpeedTreeMeshType InMeshType):
		Component(InComponent),
		MeshType(InMeshType)
	{}

	// FLightCacheInterface
	virtual FLightInteraction GetInteraction(const class FLightSceneInfo* LightSceneInfo) const
	{
		// Check if the light is in the light-map.
		const FLightMap* LightMap = 
			ChooseByMeshType<FLightMap*>(
				MeshType,
				Component->BranchLightMap,
				Component->BranchLightMap,
				Component->FrondLightMap,
				Component->LeafCardLightMap,
				Component->LeafMeshLightMap,
				Component->BillboardLightMap
				);
		if(LightMap)
		{
			if(LightMap->LightGuids.ContainsItem(LightSceneInfo->LightmapGuid))
			{
				return FLightInteraction::LightMap();
			}
		}

		// Check whether we have static lighting for the light.
		for(INT LightIndex = 0;LightIndex < Component->StaticLights.Num();LightIndex++)
		{
			const FSpeedTreeStaticLight& StaticLight = Component->StaticLights(LightIndex);

			if(StaticLight.Guid == LightSceneInfo->LightGuid)
			{
				const UShadowMap1D* ShadowMap = 
					ChooseByMeshType<UShadowMap1D*>(
						MeshType,
						StaticLight.BranchShadowMap,
						StaticLight.BranchShadowMap,
						StaticLight.FrondShadowMap,
						StaticLight.LeafCardShadowMap,
						StaticLight.LeafMeshShadowMap,
						StaticLight.BillboardShadowMap
						);

				if(ShadowMap)
				{
					if (ShadowMap->GetLightGuid() == LightSceneInfo->LightGuid)
					{
						return FLightInteraction::ShadowMap1D(ShadowMap);
					}
					else
					{
						return FLightInteraction::Uncached();
					}
				}
				else
				{
					return FLightInteraction::Irrelevant();
				}
			}
		}

		return FLightInteraction::Uncached();
	}
	virtual FLightMapInteraction GetLightMapInteraction() const
	{
		const FLightMap* LightMap =
			ChooseByMeshType<FLightMap*>(
				MeshType,
				Component->BranchLightMap,
				Component->BranchLightMap,
				Component->FrondLightMap,
				Component->LeafCardLightMap,
				Component->LeafMeshLightMap,
				Component->BillboardLightMap
				);
		if(LightMap)
		{
			return LightMap->GetInteraction();
		}
		else
		{
			return FLightMapInteraction();
		}
	}

private:

	const USpeedTreeComponent* const Component;
	ESpeedTreeMeshType MeshType;
};

/** Represents the SpeedTree to the scene manager in the rendering thread. */
class FSpeedTreeSceneProxy : public FPrimitiveSceneProxy
{
public:

	FSpeedTreeSceneProxy( USpeedTreeComponent* InComponent ) 
	:	FPrimitiveSceneProxy( InComponent, InComponent->SpeedTree->GetFName() )
	,	SpeedTree( InComponent->SpeedTree )
	,	Component( InComponent )
	,	Bounds( InComponent->Bounds )
	,	RotatedLocalToWorld( InComponent->RotationOnlyMatrix.Inverse() * InComponent->LocalToWorld )
	,	RotationOnlyMatrix( InComponent->RotationOnlyMatrix )
	,	LevelColor(255,255,255)
	,	PropertyColor(255,255,255)
	,	Lod3DStart(InComponent->Lod3DStart)
	,	Lod3DEnd(InComponent->Lod3DEnd)
	,	LodBillboardStart(InComponent->LodBillboardStart)
	,	LodBillboardEnd(InComponent->LodBillboardEnd)
	,	LodLevelOverride(InComponent->LodLevelOverride)
	,	LodBillboardFadeScalar(0.4f)
	,	bUseLeafCards(InComponent->bUseLeafCards)
	,	bUseLeafMeshes(InComponent->bUseLeafMeshes)
	,	bUseBranches(InComponent->bUseBranches)
	,	bUseFronds(InComponent->bUseFronds)
	,	bUseBillboards(InComponent->bUseBillboards && SpeedTree->SRH->bHasBillboards)
	,	bCastShadow(InComponent->CastShadow)
	,	bShouldCollide(InComponent->ShouldCollide())
	,	bBlockZeroExtent(InComponent->BlockZeroExtent)
	,	bBlockNonZeroExtent(InComponent->BlockNonZeroExtent)
	,	bBlockRigidBody(InComponent->BlockRigidBody)
	,	BranchLCI(InComponent,STMT_Branches1)
	,	FrondLCI(InComponent,STMT_Fronds)
	,	LeafMeshLCI(InComponent,STMT_LeafMeshes)
	,	LeafCardLCI(InComponent,STMT_LeafCards)
	,	BillboardLCI(InComponent,STMT_Billboards)
	{
		// Make sure applied materials have been compiled with the speed tree vertex factory.
		Branch1Material = GetSpeedTreeMaterial(Component->Branch1Material,SpeedTree->Branch1Material,BranchLCI.GetLightMapInteraction().GetType() != LMIT_None,MaterialViewRelevance);
		Branch2Material = GetSpeedTreeMaterial(Component->Branch2Material,SpeedTree->Branch2Material,BranchLCI.GetLightMapInteraction().GetType() != LMIT_None,MaterialViewRelevance);
		FrondMaterial = GetSpeedTreeMaterial(Component->FrondMaterial,SpeedTree->FrondMaterial,FrondLCI.GetLightMapInteraction().GetType() != LMIT_None,MaterialViewRelevance);
		LeafCardMaterial = GetSpeedTreeMaterial(Component->LeafCardMaterial,SpeedTree->LeafCardMaterial,LeafCardLCI.GetLightMapInteraction().GetType() != LMIT_None,MaterialViewRelevance);
		LeafMeshMaterial = GetSpeedTreeMaterial(Component->LeafMeshMaterial,SpeedTree->LeafMeshMaterial,LeafMeshLCI.GetLightMapInteraction().GetType() != LMIT_None,MaterialViewRelevance);
		BillboardMaterial = GetSpeedTreeMaterial(Component->BillboardMaterial,SpeedTree->BillboardMaterial,BillboardLCI.GetLightMapInteraction().GetType() != LMIT_None,MaterialViewRelevance);
	
		// Try to find a color for level coloration.
		AActor* Owner = InComponent->GetOwner();
		if( Owner )
		{
			ULevel* Level = Owner->GetLevel();
			if ( Level )
			{
				ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
				if ( LevelStreaming )
				{
					LevelColor = LevelStreaming->DrawColor;
				}
			}

			// Get a color for property coloration.
			GEngine->GetPropertyColorationColor( InComponent, PropertyColor );
		}
	}

	/** Determines if any collision should be drawn for this mesh. */
	UBOOL ShouldDrawCollision(const FSceneView* View)
	{
		if((View->Family->ShowFlags & SHOW_CollisionNonZeroExtent) && bBlockNonZeroExtent && bShouldCollide)
		{
			return TRUE;
		}

		if((View->Family->ShowFlags & SHOW_CollisionZeroExtent) && bBlockZeroExtent && bShouldCollide)
		{
			return TRUE;
		}	

		if((View->Family->ShowFlags & SHOW_CollisionRigidBody) && bBlockRigidBody)
		{
			return TRUE;
		}

		return FALSE;
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		FPrimitiveViewRelevance Result;
		if(View->Family->ShowFlags & SHOW_SpeedTrees)
		{
			if( IsShown(View) )
			{
#if !FINAL_RELEASE
				if(IsCollisionView(View))
				{
					Result.bDynamicRelevance = TRUE;
					Result.bForceDirectionalLightsDynamic = TRUE;
				}
				else 
#endif
				if(
#if !FINAL_RELEASE
					IsRichView(View) || 
					(View->Family->ShowFlags & (SHOW_Bounds|SHOW_Collision)) ||
#endif
					HasViewDependentDPG() ||
					IsMovable() ||
					bSelected)
				{
					SetRelevanceForShowBounds(View->Family->ShowFlags, Result);
					Result.bDynamicRelevance = TRUE;
				}
				else
				{
					// Is the mesh currently fading in or out?
					const FSceneViewState* SceneViewState = static_cast<const FSceneViewState*>( View->State );
					const UBOOL bIsMaskedForScreenDoorFade =
						SceneViewState != NULL && PrimitiveSceneInfo != NULL &&
						SceneViewState->IsPrimitiveFading( PrimitiveSceneInfo->Component );
					if( bIsMaskedForScreenDoorFade )
					{
						// Screen-door fading primitives are implicitly masked and always need depth rendered with
						// a shader, so we'll bucket them with dynamic meshes for this
						Result.bDynamicRelevance = TRUE;
					}
					else
					{
						Result.bStaticRelevance = TRUE;
					}
				}

				Result.SetDPG( GetDepthPriorityGroup(View), TRUE );
			}
			if (IsShadowCast(View))
			{
				Result.bShadowRelevance = TRUE;
			}
			
			// Replicate the material relevance flags into the resulting primitive view relevance's material flags.
			MaterialViewRelevance.SetPrimitiveViewRelevance(Result);
		}
		return Result;
	}

	virtual void GetLightRelevance(const FLightSceneInfo* LightSceneInfo, UBOOL& bDynamic, UBOOL& bRelevant, UBOOL& bLightMapped) const
	{
		// Use the FSpeedTreeLCI to find the light's interaction type.
		// Assume that light relevance is the same for all mesh types.
		FSpeedTreeLCI SpeedTreeLCI(Component,STMT_Branches1);
		const ELightInteractionType InteractionType = SpeedTreeLCI.GetInteraction(LightSceneInfo).GetType();

		// Attach the light to the primitive's static meshes.
		bDynamic = (InteractionType == LIT_Uncached);
		bRelevant = (InteractionType != LIT_CachedIrrelevant);
		bLightMapped = (InteractionType == LIT_CachedLightMap || InteractionType == LIT_CachedIrrelevant);
	}

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* SPDI)
	{
		// Draw the tree's mesh elements.
		const UBOOL bUseSelectedMaterial = FALSE;
		DrawTreeMesh(NULL,SPDI,NULL,GetStaticDepthPriorityGroup(), bUseSelectedMaterial);
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
		const UBOOL bIsCollisionView = IsCollisionView(View);
		const UBOOL bDrawCollision =
			((bIsCollisionView && ShouldDrawCollision(View)) ||
			((View->Family->ShowFlags & SHOW_Collision) && bShouldCollide)) 
			&& AllowDebugViewmodes();

		// Check to see if this mesh is currently fading in or out.  If it's fading then it's been temporarily moved
		// from static relevance to dynamic relevance for the masked fade and we we'll definitely want to draw it 
		const FSceneViewState* SceneViewState = static_cast<const FSceneViewState*>( View->State );
		const UBOOL bIsMaskedForScreenDoorFade =
			SceneViewState != NULL && PrimitiveSceneInfo != NULL &&
			SceneViewState->IsPrimitiveFading( PrimitiveSceneInfo->Component );

		const UBOOL bDrawMesh =
			!bIsCollisionView &&
			(IsRichView(View) || HasViewDependentDPG() || IsMovable() || (View->Family->ShowFlags & (SHOW_Bounds | SHOW_Collision)) || bIsMaskedForScreenDoorFade || bSelected);

		if(bDrawMesh && GetDepthPriorityGroup(View) == DPGIndex)
		{
			// Draw the tree's mesh elements.
			DrawTreeMesh(PDI,NULL,View,DPGIndex, bSelected);
		}

		if(bDrawCollision && GetDepthPriorityGroup(View) == DPGIndex)
		{
			// Draw the speedtree's collision model
			INT		NumCollision = SpeedTree->SRH->GetNumCollisionPrimitives();
			FLOAT	UniformScale = LocalToWorld.GetAxis(0).Size();

			const UMaterialInterface* CollisionMaterialParent = GEngine->ShadedLevelColorationUnlitMaterial;
			const FColoredMaterialRenderProxy CollisionMaterial(
				CollisionMaterialParent->GetRenderProxy(bSelected, bHovered),
				ConditionalAdjustForMobileEmulation(View, GEngine->C_ScaleBoxHi)
				);
			const UBOOL bDrawWireframeCollision = (View->Family->ShowFlags & (SHOW_Wireframe|SHOW_Collision)) != 0;

			for( INT i=0; i<NumCollision; i++ )
			{
				const SpeedTree::SCollisionObject* Object = SpeedTree->SRH->GetCollisionPrimitive( i );
				const FLOAT Radius = Object->m_fRadius * UniformScale;
				const FVector Pos1 = RotatedLocalToWorld.TransformFVector(FVector(Object->m_vCenter1.x, Object->m_vCenter1.y, Object->m_vCenter1.z));

				if (Object->m_vCenter1 == Object->m_vCenter2)
				{
					// sphere
					static const UINT NumSidesPerHemicircle = 6;
					if(bDrawWireframeCollision)
					{
						DrawWireSphere(
							PDI,
							Pos1,
							GetSelectionColor(GEngine->C_ScaleBoxHi,bSelected,bHovered),
							Radius,
							NumSidesPerHemicircle*2,
							DPGIndex
							);
					}
					else
					{
						DrawSphere(
							PDI,
							Pos1,
							FVector(Radius, Radius, Radius),
							NumSidesPerHemicircle,
							NumSidesPerHemicircle,
							&CollisionMaterial,
							DPGIndex
							);
					}
				}
				else
				{
					// capsule
					const FVector Pos2 = RotatedLocalToWorld.TransformFVector(FVector(Object->m_vCenter2.x, Object->m_vCenter2.y, Object->m_vCenter2.z));
					FVector UpDir = Pos2 - Pos1;
					FLOAT Height = UpDir.Size( );
					if (Height != 0.0f)
						UpDir /= Height;
					Height *= 0.5f;
					FVector RightDir = (UpDir ^ FVector(1, 0, 0)).SafeNormal();
					FVector OutDir = (UpDir ^ RightDir).SafeNormal();
						
					if(bDrawWireframeCollision)
					{
						DrawWireCylinder(
							PDI, 
							Pos1 + UpDir * Height,
							RightDir, 
							OutDir, 
							UpDir,
							GetSelectionColor(GEngine->C_ScaleBoxHi,bSelected,bHovered),
							Radius,
							Height,
							24,
							DPGIndex
							);
					}
					else
					{
						DrawCylinder(
							PDI, 
							Pos1 + UpDir * Height, 
							RightDir, 
							OutDir, 
							UpDir,
							Radius,
							Height,
							24,
							&CollisionMaterial,
							DPGIndex
							);
					}
				}
			}
		}
	
		RenderBounds(PDI, DPGIndex, View->Family->ShowFlags, PrimitiveSceneInfo->Bounds, bSelected);
	}

	virtual DWORD GetMemoryFootprint() const
	{ 
		return (sizeof(*this) + GetAllocatedSize()); 
	}
	DWORD GetAllocatedSize() const
	{ 
		return FPrimitiveSceneProxy::GetAllocatedSize(); 
	}

private:

	USpeedTree*				SpeedTree;
	USpeedTreeComponent*	Component;
	FBoxSphereBounds		Bounds;

	FMatrix RotatedLocalToWorld;
	FMatrix RotationOnlyMatrix;

	const UMaterialInterface* Branch1Material;
	const UMaterialInterface* Branch2Material;
	const UMaterialInterface* FrondMaterial;
	const UMaterialInterface* LeafCardMaterial;
	const UMaterialInterface* LeafMeshMaterial;
	const UMaterialInterface* BillboardMaterial;

	FColor LevelColor;
	FColor PropertyColor;

	const FLOAT Lod3DStart;
	const FLOAT Lod3DEnd;
	const FLOAT LodBillboardStart;
	const FLOAT LodBillboardEnd;
	const FLOAT LodLevelOverride;
	const FLOAT	LodBillboardFadeScalar;

	const BITFIELD bUseLeafCards : 1;
	const BITFIELD bUseLeafMeshes : 1;
	const BITFIELD bUseBranches : 1;
	const BITFIELD bUseFronds : 1;
	const BITFIELD bUseBillboards : 1;

	const BITFIELD bCastShadow : 1;
	const BITFIELD bShouldCollide : 1;
	const BITFIELD bBlockZeroExtent : 1;
	const BITFIELD bBlockNonZeroExtent : 1;
	const BITFIELD bBlockRigidBody : 1;

	/** The view relevance for all the primitive's materials. */
	FMaterialViewRelevance MaterialViewRelevance;

	/** The component's billboard mesh user data. */
	TIndirectArray<FSpeedTreeVertexFactory::MeshUserDataType> MeshUserDatas;
	
	/** The static lighting cache interface for the primitive's branches. */
	FSpeedTreeLCI BranchLCI;

	/** The static lighting cache interface for the primitive's fronds. */
	FSpeedTreeLCI FrondLCI;

	/** The static lighting cache interface for the primitive's leaf meshes. */
	FSpeedTreeLCI LeafMeshLCI;

	/** The static lighting cache interface for the primitive's leaf cards. */
	FSpeedTreeLCI LeafCardLCI;

	/** The static lighting cache interface for the primitive's billboards. */
	FSpeedTreeLCI BillboardLCI;
	


	/**
	 * Draws a single LOD of a mesh.
	 * @param PDI - An optional pointer to an interface to pass the dynamic mesh elements to.
	 * @param SPDI - An optional pointer to an interface to pass the static mesh elements to.  Either PDI or SPDI must be non-NULL.
	 * @param View - An optional pointer to the current view.  If provided, only the LODs relevant in that view will be drawn, otherwise all will be.
	 * @param MeshElement - The mesh to draw.
	 * @param ActualDistances - The distance range for a particular element to compute lod
	 * @param AdjustedDistances - The adjusted distance range for a particular element for drawing
	 * @param Material - The material to apply to the mesh elements.
	 * @param LCI - The static lighting cache to use for the mesh elements.
	 * @param bUseAsOccluder - TRUE if the mesh elements may be effective occluders.
	 * @param DPGIndex - The depth priority group to draw the mesh elements in.
	 * @param MeshType - The type of SpeedTree geometry this mesh is. 
	 */
	void ConditionalDrawElement(
		FPrimitiveDrawInterface* PDI,
		FStaticPrimitiveDrawInterface* SPDI,
		const FSceneView* View,
		FMeshBatch& MeshElement, 
		FVector2D ActualDistances,
		FVector2D AdjustedDistances,
		FVector2D BillboardDistances,
		const UMaterialInterface* Material,
		const FLightCacheInterface& LCI,
		UBOOL bUseAsOccluder,
		UINT DPGIndex,
		ESpeedTreeMeshType MeshType,
		const UBOOL bUseSelectedMaterial
		)
	{
		if(MeshElement.GetNumPrimitives() > 0)
		{
			FMeshBatchElement& BatchElement = MeshElement.Elements(0);
			// Determine whether to draw the LOD.
			UBOOL bDrawElement = FALSE;
			if(View && View->ViewOrigin.W > 0.0f)
			{
				const FLOAT DistanceSquared = CalculateDistanceSquaredForLOD(PrimitiveSceneInfo->Bounds, View->ViewOrigin);

				const FLOAT LODFactorDistanceSquared = DistanceSquared * Square(View->LODDistanceFactor);
				if (LODFactorDistanceSquared >= Square(AdjustedDistances.X) && LODFactorDistanceSquared < Square(AdjustedDistances.Y))
				{
					bDrawElement = TRUE;
				}
			}
			else
			{
				// If no view is provided, always draw this LOD.
				bDrawElement = TRUE;
			}

			if(bDrawElement)
			{
				// Initialize the mesh element with this primitive's parameters.
				MeshElement.LCI = &LCI;
				MeshElement.MaterialRenderProxy = Material ? Material->GetRenderProxy(bUseSelectedMaterial) : GEngine->DefaultMaterial->GetRenderProxy(bUseSelectedMaterial);
				BatchElement.LocalToWorld = LocalToWorld;
				BatchElement.WorldToLocal = LocalToWorld.Inverse();
				MeshElement.LODIndex = INDEX_NONE;
				MeshElement.DepthPriorityGroup	= DPGIndex;
				MeshElement.bUseAsOccluder = bUseAsOccluder && PrimitiveSceneInfo->bUseAsOccluder;
				MeshElement.CastShadow = bCastShadow;
				MeshElement.ReverseCulling = (MeshType != STMT_LeafCards && LocalToWorldDeterminant < 0.0f) ? TRUE : FALSE;

				// Set up the mesh user data.
				FSpeedTreeVertexFactory::MeshUserDataType MeshUserDataTemp;
				MeshUserDataTemp.Bounds = Bounds;
				MeshUserDataTemp.RotationOnlyMatrix = RotationOnlyMatrix;
				MeshUserDataTemp.LODDistances = ActualDistances;
				MeshUserDataTemp.BillboardDistances = BillboardDistances;

				// Draw the mesh element.
				if(PDI)
				{
					// Dynamic mesh elements are drawn immediately before returning, so we can just use the user data on the stack.
					BatchElement.ElementUserData = &MeshUserDataTemp;

					static const FLinearColor WireframeColor(0.3f,1.0f,0.3f);
					DrawRichMesh( PDI, MeshElement, WireframeColor, LevelColor, PropertyColor, PrimitiveSceneInfo, bUseSelectedMaterial );
				}
				else if(SPDI)
				{
					// Static mesh elements aren't drawn until after this function has returned, so add the user data to a persistent array.
					BatchElement.ElementUserData = new(MeshUserDatas) FSpeedTreeVertexFactory::MeshUserDataType(MeshUserDataTemp);

					SPDI->DrawMesh(MeshElement,AdjustedDistances.X,AdjustedDistances.Y);
				}
			}
		}
	}

	/**
	 * Draws a set of mesh elements corresponding to separate LODs for a specific component of the tree.
	 * @param PDI - An optional pointer to an interface to pass the dynamic mesh elements to.
	 * @param SPDI - An optional pointer to an interface to pass the static mesh elements to.  Either PDI or SPDI must be non-NULL.
	 * @param View - An optional pointer to the current view.  If provided, only the LODs relevant in that view will be drawn, otherwise all will be.
	 * @param LODMeshElements - The LOD mesh elements, ordered from highest detail to lowest.
	 * @param Material - The material to apply to the mesh elements.
	 * @param LCI - The static lighting cache to use for the mesh elements.
	 * @param bUseAsOccluder - TRUE if the mesh elements may be effective occluders.
	 * @param DPGIndex - The depth priority group to draw the mesh elements in.
	 */
	void DrawMeshElementLODs(
		FPrimitiveDrawInterface* PDI,
		FStaticPrimitiveDrawInterface* SPDI,
		const FSceneView* View,
		TArray<FMeshBatch>& LODMeshElements,
		const UMaterialInterface* Material,
		const FLightCacheInterface& LCI,
		UBOOL bUseAsOccluder,
		UINT DPGIndex,
		ESpeedTreeMeshType MeshType,
		const UBOOL bUseSelectedMaterial
		)
	{
		if(LodLevelOverride > 0.0f)
		{
			if (LODMeshElements.Num() == 0)
			{
				return;
			}

			ConditionalDrawElement(
				PDI,
				SPDI,
				View,
				LODMeshElements(appRound((FLOAT)(LODMeshElements.Num() - 1) * (1.0f - LodLevelOverride))),
				FVector2D(0.0f, BIG_NUMBER),
				FVector2D(0.0f, BIG_NUMBER),
				FVector2D(BIG_NUMBER, BIG_NUMBER),
				Material,
				LCI,
				bUseAsOccluder,
				DPGIndex,
				MeshType,
				bUseSelectedMaterial
				);
		}
		else
		{
			for(INT LODIndex = 0;LODIndex < LODMeshElements.Num();LODIndex++)
			{
				// the actual lod range for inter-lod scaling
				FVector2D ActualDistances(LODIndex, LODIndex + 1);
				ActualDistances *= (Lod3DEnd - Lod3DStart) / FLOAT(LODMeshElements.Num());
				ActualDistances.X += Lod3DStart;
				ActualDistances.Y += Lod3DStart;
				if (appIsNearlyEqual(ActualDistances.X, ActualDistances.Y))
				{
					ActualDistances.Y += 1.0f;
				}
			
				// the draw distances for ue3 static geometry, adjusted for the various cases
				FVector2D AdjustedDistances = ActualDistances;
				if (LODIndex == 0)
				{
					AdjustedDistances.X = 0.0f;
				}
				if (LODIndex == LODMeshElements.Num() - 1)
				{
					AdjustedDistances.Y = LodBillboardEnd;
				}
				AdjustedDistances.Y = Min(AdjustedDistances.Y, LodBillboardEnd);

				// billboard distances. for geometry, the near is pushed out for better crossfade
				FVector2D BillboardDistances(LodBillboardStart + LodBillboardFadeScalar * (LodBillboardEnd - LodBillboardStart), LodBillboardEnd);
				if (appIsNearlyEqual(BillboardDistances.X, BillboardDistances.Y))
				{
					BillboardDistances.Y += 1.0f;
				}

				ConditionalDrawElement(
					PDI,
					SPDI,
					View,
					LODMeshElements(LODIndex),
					ActualDistances,
					AdjustedDistances,
					BillboardDistances,
					Material,
					LCI,
					bUseAsOccluder,
					DPGIndex,
					MeshType,
					bUseSelectedMaterial
					);
			}
		}
	}

	/**
	 * Draws the tree's mesh elements, either through the static or dynamic primitive draw interface.
	 * @param PDI - An optional pointer to an interface to pass the dynamic mesh elements to.
	 * @param SPDI - An optional pointer to an interface to pass the static mesh elements to.  Either PDI or SPDI must be non-NULL.
	 * @param View - An optional pointer to the current view.  If provided, only the LODs relevant in that view will be drawn, otherwise all will be.
	 * @param DPGIndex - The depth priority group to draw the tree in.
	 */
	void DrawTreeMesh(
		FPrimitiveDrawInterface* PDI,
		FStaticPrimitiveDrawInterface* SPDI,
		const FSceneView* View,
		UINT DPGIndex,
		const UBOOL bUseSelectedMaterial
		)
	{
		check(PDI || SPDI);

		if( bUseBillboards )
		{
			// Draw the billboard mesh element.
			FVector2D ActualDistances(LodBillboardStart, LodBillboardEnd);
			FVector2D AdjustedDistances(LodBillboardStart, BIG_NUMBER);

			// the far is pulled in for better crossfade
			FVector2D BillboardDistances(LodBillboardStart, LodBillboardEnd - LodBillboardFadeScalar * (LodBillboardEnd - LodBillboardStart));
			if (appIsNearlyEqual(BillboardDistances.X, BillboardDistances.Y))
			{
				BillboardDistances.Y += 1.0f;
			}

			if (LodLevelOverride == 0.0f)
			{
				ActualDistances.Set(0.0f, 0.0f);
				AdjustedDistances.Set(0.0f, BIG_NUMBER);
				BillboardDistances.Set(BIG_NUMBER - 1.0f, BIG_NUMBER);
			}

			ConditionalDrawElement(
				PDI,
				SPDI,
				View,
				SpeedTree->SRH->BillboardElement,
				ActualDistances,
				AdjustedDistances,
				BillboardDistances,
				BillboardMaterial,
				BillboardLCI,
				TRUE,
				GetStaticDepthPriorityGroup(),
				STMT_Billboards,
				bUseSelectedMaterial
				);
		}

		if (LodLevelOverride == 0.0f)
		{
			return;
		}

		if(bUseLeafCards && GSystemSettings.bAllowSpeedTreeLeaves)
		{
			// Draw the leaf card mesh elements.
			DrawMeshElementLODs(PDI,SPDI,View,SpeedTree->SRH->LeafCardElements,LeafCardMaterial,LeafCardLCI,TRUE,DPGIndex,STMT_LeafCards, bUseSelectedMaterial);
		}

		if(bUseLeafMeshes && GSystemSettings.bAllowSpeedTreeLeaves)
		{
			// Draw the leaf mesh elements.
			DrawMeshElementLODs(PDI,SPDI,View,SpeedTree->SRH->LeafMeshElements,LeafMeshMaterial,LeafMeshLCI,TRUE,DPGIndex,STMT_LeafMeshes, bUseSelectedMaterial);
		}

		if(bUseFronds && SpeedTree->SRH->bHasFronds && GSystemSettings.bAllowSpeedTreeFronds)
		{
			// Draw the frond mesh elements.
			DrawMeshElementLODs(PDI,SPDI,View,SpeedTree->SRH->FrondElements,FrondMaterial,FrondLCI,FALSE,DPGIndex,STMT_Fronds, bUseSelectedMaterial);
		}

		if(bUseBranches && SpeedTree->SRH->bHasBranches)
		{
			// Draw the branch mesh elements.
			DrawMeshElementLODs(PDI,SPDI,View,SpeedTree->SRH->Branch1Elements,Branch1Material,BranchLCI,TRUE,DPGIndex,STMT_Branches1, bUseSelectedMaterial);
			DrawMeshElementLODs(PDI,SPDI,View,SpeedTree->SRH->Branch2Elements,Branch2Material,BranchLCI,TRUE,DPGIndex,STMT_Branches2, bUseSelectedMaterial);
		}
	}
};

FPrimitiveSceneProxy* USpeedTreeComponent::CreateSceneProxy(void)
{
	return ::new FSpeedTreeSceneProxy(this);
}

void USpeedTreeComponent::UpdateBounds( )
{
	// bounds for tree
	if( SpeedTree == NULL || !SpeedTree->IsInitialized() )
	{
#if WITH_EDITORONLY_DATA
		if( SpeedTreeIcon != NULL )
		{
			// bounds for icon
			const FLOAT IconScale = (Owner ? Owner->DrawScale : 1.0f) * (SpeedTreeIcon ? (FLOAT)Max(SpeedTreeIcon->SizeX, SpeedTreeIcon->SizeY) : 1.0f);
			Bounds = FBoxSphereBounds(LocalToWorld.GetOrigin( ), FVector(IconScale, IconScale, IconScale), appSqrt(3.0f * Square(IconScale)));
		}
		else
#endif // WITH_EDITORONLY_DATA
		{
			Super::UpdateBounds( );
		}
	}
	else
	{
		// speedtree bounds
		Bounds = SpeedTree->SRH->Bounds.TransformBy(RotationOnlyMatrix.Inverse() * LocalToWorld);
		Bounds.BoxExtent += FVector(1.0f, 1.0f, 1.0f);
		Bounds.SphereRadius += 1.0f;
	}
}

/** 
 * Retrieves the materials used in this component 
 * 
 * @param OutMaterials	The list of used materials.
 */
void USpeedTreeComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
	// Note: If the ESpeedTreeMeshType enum changes, this function should be updated.
	OutMaterials.AddItem( GetMaterial( STMT_Branches1 ) );
	OutMaterials.AddItem( GetMaterial( STMT_Branches2 ) );
	OutMaterials.AddItem( GetMaterial( STMT_Fronds ) );
	OutMaterials.AddItem( GetMaterial( STMT_LeafCards ) );
	OutMaterials.AddItem( GetMaterial( STMT_LeafMeshes ) );
	OutMaterials.AddItem( GetMaterial( STMT_Billboards ) );
}	

void FSpeedTreeResourceHelper::BuildTexelFactors()
{
	// If the SpeedTree doesn't have cached texel factors, compute them now.
	if(!bHasValidTexelFactors)
	{
		bHasValidTexelFactors = TRUE;

		// Process each mesh type.
		for(INT MeshType = STMT_MinMinusOne + 1; MeshType < STMT_Max; MeshType++)
		{
			FLOAT& TexelFactor = TexelFactors[MeshType];
			TexelFactor = 0.0f;

			const UBOOL bTreeHasThisMeshType = ChooseByMeshType<UBOOL>(
				MeshType,
				bHasBranches && Branch1Elements.Num(),
				bHasBranches && Branch2Elements.Num(),
				bHasFronds && FrondElements.Num(),
				bHasLeafCards && LeafCardElements.Num(),
				bHasLeafMeshes && LeafMeshElements.Num(),
				bHasBillboards
				);
			if(bTreeHasThisMeshType)
			{
				const TArray<WORD>& Indices = IndexBuffer.Indices;
				const TArray<FSpeedTreeVertexPosition>& VertexPositions = ChooseByMeshType<TArray<FSpeedTreeVertexPosition> >(
					MeshType,
					BranchPositionBuffer.Vertices,
					BranchPositionBuffer.Vertices,
					FrondPositionBuffer.Vertices,
					LeafCardPositionBuffer.Vertices,
					LeafMeshPositionBuffer.Vertices,
					BillboardPositionBuffer.Vertices
					);
				const FMeshBatch& MeshElement = *ChooseByMeshType<FMeshBatch*>(
					MeshType,
					&Branch1Elements(0),
					&Branch2Elements(0),
					&FrondElements(0),
					&LeafCardElements(0),
					&LeafMeshElements(0),
					&BillboardElement
					);

				// Compute the maximum texel ratio of the mesh's triangles.
				for( INT BatchElementIndex=0;BatchElementIndex < MeshElement.Elements.Num();BatchElementIndex++ )
				{
					const FMeshBatchElement& BatchElement = MeshElement.Elements(BatchElementIndex);
					for(UINT TriangleIndex = 0;TriangleIndex < BatchElement.NumPrimitives;TriangleIndex++)
					{
						const WORD I0 = Indices(BatchElement.FirstIndex + TriangleIndex * 3 + 0);
						const WORD I1 = Indices(BatchElement.FirstIndex + TriangleIndex * 3 + 1);
						const WORD I2 = Indices(BatchElement.FirstIndex + TriangleIndex * 3 + 2);
						const FLOAT L1 = (VertexPositions(I0).Position - VertexPositions(I1).Position).Size();
						const FLOAT L2 = (VertexPositions(I0).Position - VertexPositions(I2).Position).Size();
						const FLOAT T1 = (GetSpeedTreeVertexData(this,MeshType,I0)->TexCoord - GetSpeedTreeVertexData(this,MeshType,I1)->TexCoord).Size();
						const FLOAT T2 = (GetSpeedTreeVertexData(this,MeshType,I0)->TexCoord - GetSpeedTreeVertexData(this,MeshType,I2)->TexCoord).Size();
						TexelFactor = Max(TexelFactor,Max(L1 / T1,L2 / T2));
					}
				}
			}
		}
	}
}

void USpeedTreeComponent::GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
{
	if( SpeedTree && SpeedTree->SRH )
	{
		FSpeedTreeResourceHelper* const SRH = SpeedTree->SRH;

		// If the SpeedTree doesn't have cached texel factors, compute them now.
		SRH->BuildTexelFactors();

		for(INT MeshType = STMT_MinMinusOne + 1;MeshType < STMT_Max;MeshType++)
		{
			const UBOOL bComponentUsesThisMeshType = ChooseByMeshType<UBOOL>(
				MeshType,
				bUseBranches,
				bUseBranches,
				bUseFronds,
				bUseLeafCards,
				bUseLeafMeshes,
				bUseBillboards
				);
			if(bComponentUsesThisMeshType)
			{
				// Determine the texel factor this mesh type in world space.
				const FLOAT WorldTexelFactor = LocalToWorld.GetMaximumAxisScale() * SRH->TexelFactors[MeshType];

				// Determine the material used by this mesh.
				FMaterialViewRelevance UnusedMaterialViewRelevance;
				UMaterialInterface* Material = GetSpeedTreeMaterial(
					ChooseByMeshType<UMaterialInterface*>(
						MeshType,
						Branch1Material,
						Branch2Material,
						FrondMaterial,
						LeafCardMaterial,
						LeafMeshMaterial,
						BillboardMaterial
						),
					ChooseByMeshType<UMaterialInterface*>(
						MeshType,
						SpeedTree->Branch1Material,
						SpeedTree->Branch2Material,
						SpeedTree->FrondMaterial,
						SpeedTree->LeafCardMaterial,
						SpeedTree->LeafMeshMaterial,
						SpeedTree->BillboardMaterial
						),
					FALSE,
					UnusedMaterialViewRelevance
					);

				// Enumerate the textures used by the material.
				TArray<UTexture*> Textures;
				
				Material->GetUsedTextures(Textures, MSQ_UNSPECIFIED, TRUE);

				// Add each texture to the output with the appropriate parameters.
				for(INT TextureIndex = 0;TextureIndex < Textures.Num();TextureIndex++)
				{
					FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
					StreamingTexture.Bounds = Bounds.GetSphere();
					StreamingTexture.TexelFactor = WorldTexelFactor;
					StreamingTexture.Texture = Textures(TextureIndex);
				}
			}
		}
	}
}

UBOOL USpeedTreeComponent::PointCheck(FCheckResult& Result, const FVector& Location, const FVector& Extent, DWORD TraceFlags )
{
	if( SpeedTree == NULL || !SpeedTree->IsInitialized() )
	{
		return Super::PointCheck( Result, Location, Extent, TraceFlags );
	}

	UBOOL bReturn = FALSE;
	const FMatrix RotatedLocalToWorld = RotationOnlyMatrix.Inverse() * LocalToWorld;
	INT		NumCollision = SpeedTree->SRH->GetNumCollisionPrimitives();
	FLOAT	UniformScale = LocalToWorld.GetAxis(0).Size();

	for( INT i=0; i<NumCollision && !bReturn; i++ )
	{
		// get the collision object
		const SpeedTree::SCollisionObject* Object = SpeedTree->SRH->GetCollisionPrimitive( i );
		const FVector Pos = RotatedLocalToWorld.TransformFVector(FVector(Object->m_vCenter1.x, Object->m_vCenter1.y, Object->m_vCenter1.z));
		const FLOAT Radius = Object->m_fRadius * UniformScale;
				
		if (Object->m_vCenter1 == Object->m_vCenter2)
		{
			// sphere
			if( (Location - Pos).SizeSquared( ) < Radius * Radius)
			{
				Result.Normal	= (Location - Pos).SafeNormal();
				Result.Location = Result.Normal * Radius;
				bReturn = true;
			}
		}
		else
		{
			// capsule	
			const FVector Pos2 = RotatedLocalToWorld.TransformFVector(FVector(Object->m_vCenter2.x, Object->m_vCenter2.y, Object->m_vCenter2.z));
			
			FVector UpDir = Pos2 - Pos;
			const FVector Center = Pos + UpDir * 0.5f;
			FLOAT Height = UpDir.Size( );
			if (Height != 0.0f)
            {
				UpDir /= Height;
            }
			Height *= 0.5f;
			const FVector RightDir = (UpDir ^ FVector(1, 0, 0)).SafeNormal();
			const FVector OutDir = (UpDir ^ RightDir).SafeNormal();
			const FBasisVectorMatrix MatRotate(OutDir, RightDir, UpDir, FVector(0,0,0));
					
			// rotate cLocation into collision object's space
			const FVector NewLocation = MatRotate.TransformFVector(Location - Center) + Center;

			// *** portions taken from UCylinderComponent::LineCheck in UnActorComponent.cpp ***
			if( Square(Center.Z - NewLocation.Z) < Square(Height + Extent.Z)
				&& Square(Center.X - NewLocation.X) + Square(Center.Y - NewLocation.Y) < Square(Radius + Extent.X) )
			{
				Result.Normal = (NewLocation - Center).SafeNormal();

				if( Result.Normal.Z < -0.5 )
				{
					Result.Location = FVector(NewLocation.X, NewLocation.Y, Center.Z - Extent.Z);
				}
				else if( Result.Normal.Z > 0.5 )
				{
					Result.Location = FVector(NewLocation.X, NewLocation.Y, Center.Z - Extent.Z);
				}
				else
				{
					Result.Location = (NewLocation - Extent.X * (Result.Normal * FVector(1, 1, 0)).SafeNormal( )) + FVector(0, 0, NewLocation.Z);
				}

				bReturn = TRUE;
			}

			// transform back into world coordinates if needed
			if( bReturn )
			{
				Result.Location = MatRotate.InverseTransformFVectorNoScale( Result.Location - Center ) + Center;
				Result.Normal	= MatRotate.InverseTransformFVectorNoScale( Result.Normal);
			}

		}
	}

	// other fcheckresult stuff
	if( bReturn )
	{
		Result.Material		= NULL;
		Result.Actor		= Owner;
		Result.Component	= this;
	}

	return !bReturn;
}

UBOOL USpeedTreeComponent::LineCheck(FCheckResult& Result, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags)
{
	if( SpeedTree == NULL || !SpeedTree->IsInitialized() )
	{
		return Super::LineCheck( Result, End, Start, Extent, TraceFlags );
	}

	UBOOL bReturn = FALSE;

	const FMatrix RotatedLocalToWorld = RotationOnlyMatrix.Inverse() * LocalToWorld;
	INT		NumCollision = SpeedTree->SRH->GetNumCollisionPrimitives();
	FLOAT	UniformScale = LocalToWorld.GetAxis(0).Size();

	for( INT i=0; i<NumCollision && !bReturn; i++ )
	{
		// get the collision object
		const SpeedTree::SCollisionObject* Object = SpeedTree->SRH->GetCollisionPrimitive( i );
		const FVector Pos = RotatedLocalToWorld.TransformFVector(FVector(Object->m_vCenter1.x, Object->m_vCenter1.y, Object->m_vCenter1.z));
		const FLOAT Radius = Object->m_fRadius * UniformScale;
				
		if (Object->m_vCenter1 == Object->m_vCenter2)
		{
			// sphere
			// *** portions taken from FLineSphereIntersection in UnMath.h ***

			// Check if the start is inside the sphere.
			const FVector RelativeStart = Start - Pos;
			const FLOAT StartDistanceSquared = (RelativeStart | RelativeStart) - Square(Radius);
			if( StartDistanceSquared < 0.0f )
			{
				Result.Time		= 0.0f;
				Result.Location = Start;
				Result.Normal	= (Start - Pos).SafeNormal();
				Result			= TRUE;
				break;
			}
			
			// Calculate the discriminant for the line-sphere intersection quadratic formula.
			const FVector Dir = End - Start;
			const FLOAT DirSizeSquared = Dir.SizeSquared();

			// If the line's length is very small, the intersection position will be unstable;
			// in this case we rely on the above "start is inside sphere" check.
			if(DirSizeSquared >= DELTA)
			{
				const FLOAT B = 2.0f * (RelativeStart | Dir);
				const FLOAT Discriminant = Square(B) - 4.0f * DirSizeSquared * StartDistanceSquared;

				// If the discriminant is non-negative, then the line intersects the sphere.
				if( Discriminant >= 0 )
				{
					Result.Time = (-B - appSqrt(Discriminant)) / (2.0f * DirSizeSquared);
					if( Result.Time >= 0.0f && Result.Time <= 1.0f )
					{
						Result.Location = Start + Dir * Result.Time;
						Result.Normal	= (Result.Location - Pos).SafeNormal();
						bReturn			= TRUE;
					}
				}
			}
		}
		else
		{
			// capsule
			const FVector Pos2 = RotatedLocalToWorld.TransformFVector(FVector(Object->m_vCenter2.x, Object->m_vCenter2.y, Object->m_vCenter2.z));
			
			FVector UpDir = Pos2 - Pos;
			const FVector Center = Pos + UpDir * 0.5f;
			FLOAT Height = UpDir.Size( );
			if (Height != 0.0f)
				UpDir /= Height;
			Height *= 0.5f;
			const FVector RightDir = (UpDir ^ FVector(1, 0, 0)).SafeNormal();
			const FVector OutDir = (UpDir ^ RightDir).SafeNormal();
			
			const FBasisVectorMatrix MatRotate(OutDir, RightDir, UpDir, FVector(0,0,0));

			// rotate start/end into collision object's space
			const FVector NewStart = MatRotate.InverseTransformFVectorNoScale(Start - Center);
			const FVector NewEnd = MatRotate.InverseTransformFVectorNoScale(End - Center);

			// *** portions taken from UCylinderComponent::LineCheck in UnActorComponent.cpp ***
			Result.Time = 1.0f;

			// Treat this actor as a cylinder.
			const FVector CylExtent(Radius, Radius, Height);
			const FVector NetExtent = Extent + CylExtent;

			// Quick X reject.
			const FLOAT MaxX = +NetExtent.X;
			if( NewStart.X > MaxX && NewEnd.X > MaxX )
			{
				continue;
			}

			const FLOAT MinX = -NetExtent.X;
			if( NewStart.X < MinX && NewEnd.X < MinX )
			{
				continue;
			}

			// Quick Y reject.
			const FLOAT MaxY = +NetExtent.Y;
			if( NewStart.Y > MaxY && NewEnd.Y > MaxY )
			{
				continue;
			}

			const FLOAT MinY = -NetExtent.Y;
			if( NewStart.Y < MinY && NewEnd.Y < MinY )
			{
				continue;
			}

			// Quick Z reject.
			const FLOAT TopZ = +NetExtent.Z;
			if( NewStart.Z > TopZ && NewEnd.Z > TopZ )
			{
				continue;
			}

			const FLOAT BotZ = -NetExtent.Z;
			if( NewStart.Z < BotZ && NewEnd.Z < BotZ )
			{
				continue;
			}

			// Clip to top of cylinder.
			FLOAT T0 = 0.0f;
			FLOAT T1 = 1.0f;
			if( NewStart.Z > TopZ && NewEnd.Z < TopZ )
			{
				FLOAT T = (TopZ - NewStart.Z) / (NewEnd.Z - NewStart.Z);
				if( T > T0 )
				{
					T0 = ::Max(T0, T);
					Result.Normal = FVector(0, 0, 1);
				}
			}
			else if( NewStart.Z < TopZ && NewEnd.Z > TopZ )
			{
				T1 = ::Min(T1, (TopZ - NewStart.Z) / (NewEnd.Z - NewStart.Z));
			}

			// Clip to bottom of cylinder.
			if( NewStart.Z < BotZ && NewEnd.Z > BotZ )
			{
				FLOAT T = (BotZ - NewStart.Z) / (NewEnd.Z - NewStart.Z);
				if( T > T0 )
				{
					T0 = ::Max(T0, T);
					Result.Normal = FVector(0, 0, -1);
				}
			}
			else if( NewStart.Z > BotZ && NewEnd.Z < BotZ )
			{
				T1 = ::Min(T1, (BotZ - NewStart.Z) / (NewEnd.Z - NewStart.Z));
			}

			// Reject.
			if (T0 >= T1)
			{
				continue;
			}

			// Test setup.
			FLOAT   Kx        = NewStart.X;
			FLOAT   Ky        = NewStart.Y;

			// 2D circle clip about origin.
			FLOAT   Vx        = NewEnd.X - NewStart.X;
			FLOAT   Vy        = NewEnd.Y - NewStart.Y;
			FLOAT   A         = Vx * Vx + Vy * Vy;
			FLOAT   B         = 2.0f * (Kx * Vx + Ky * Vy);
			FLOAT   C         = Kx * Kx + Ky * Ky - Square(NetExtent.X);
			FLOAT   Discrim   = B * B - 4.0f * A * C;

			// If already inside sphere, oppose further movement inward.
			FVector LocalHitLocation;
			FVector LocalHitNormal(0,0,1);
			if( C < Square(1.0f) && NewStart.Z > BotZ && NewStart.Z < TopZ )
			{
				const FVector DirXY(
					NewEnd.X - NewStart.X,
					NewEnd.Y - NewStart.Y,
					0
					);
				FLOAT Dir = DirXY | NewStart;
				if( Dir < -0.01f )
				{
					Result.Time		= 0.0f;

					LocalHitLocation = NewStart;
					LocalHitNormal = NewStart * FVector(1, 1, 0);
						
					// transform back into world coordinates
					Result.Location = Center + MatRotate.TransformFVector(LocalHitLocation);
					Result.Normal = MatRotate.TransformNormal(LocalHitNormal).SafeNormal();

					bReturn = TRUE;

					continue;
				}
				else
				{
					continue;
				}
			}

			// No intersection if discriminant is negative.
			if( Discrim < 0 )
			{
				continue;
			}

			// Unstable intersection if velocity is tiny.
			if( A < Square(0.0001f) )
			{
				// Outside.
				if( C > 0 )
				{
					continue;
				}
				else
				{
					LocalHitNormal = NewStart;
					LocalHitNormal.Z = 0;
				}
			}
			else
			{
				// Compute intersection times.
				Discrim = appSqrt(Discrim);
				FLOAT R2A = 0.5 / A;
				T1 = ::Min(T1, +(Discrim - B) * R2A);
				FLOAT T = -(Discrim + B) * R2A;
				if (T > T0)
				{
					T0 = T;
					LocalHitNormal = NewStart + (NewEnd - NewStart) * T0;
					LocalHitNormal.Z = 0;
				}
				if( T0 >= T1 )
				{
					continue;
				}
			}
			if (TraceFlags & TRACE_Accurate)
			{
				Result.Time = Clamp(T0,0.0f,1.0f);
			}
			else
			{
				Result.Time = Clamp(T0 - 0.001f, 0.0f, 1.0f);
			}
			LocalHitLocation = NewStart + (NewEnd - NewStart) * Result.Time;

			// transform back into world coordinates
			Result.Location = Center + MatRotate.TransformFVector(LocalHitLocation);
			Result.Normal = MatRotate.TransformNormal(LocalHitNormal).SafeNormal();

			bReturn = TRUE;
		}
	}

	// other fcheckresult stuff
	if( bReturn )
	{
		Result.Material		= NULL;
		Result.Actor		= Owner;
		Result.Component	= this;
	}

	return !bReturn;
}


#if WITH_NOVODEX
void USpeedTreeComponent::InitComponentRBPhys(UBOOL /*bFixed*/)
{
	// Don't create physics body at all if no collision (makes assumption it can't change at run time).
	if( !BlockRigidBody)
	{
		return;
	}

	if( GWorld->RBPhysScene && SpeedTree && SpeedTree->IsInitialized() )
	{
		INT NumPrimitives = SpeedTree->SRH->GetNumCollisionPrimitives();
		if (NumPrimitives == 0)
		{
			return;
		}

		// make novodex info
		NxActorDesc nxActorDesc;
		nxActorDesc.setToDefault( );

		NxMat33 MatIdentity;
		MatIdentity.id();

		const FMatrix RotatedLocalToWorld = RotationOnlyMatrix.Inverse() * LocalToWorld;
		FLOAT UniformScale = LocalToWorld.GetAxis(0).Size();

		for( INT i=0; i<NumPrimitives; i++ )
		{
			// get the collision object
			const SpeedTree::SCollisionObject* Object = SpeedTree->SRH->GetCollisionPrimitive( i );
			const FVector Pos = RotatedLocalToWorld.TransformFVector(FVector(Object->m_vCenter1.x, Object->m_vCenter1.y, Object->m_vCenter1.z));
			const FLOAT Radius = Object->m_fRadius * UniformScale;

			if (Object->m_vCenter1 == Object->m_vCenter2)
			{
				// sphere
				NxSphereShapeDesc* SphereDesc = new NxSphereShapeDesc;
				SphereDesc->setToDefault( );
				SphereDesc->radius = Radius * U2PScale;
				SphereDesc->localPose = NxMat34(MatIdentity, U2NPosition(Pos));
				nxActorDesc.shapes.pushBack(SphereDesc);
			}
			else
			{
				// capsule
				const FVector Pos2 = RotatedLocalToWorld.TransformFVector(FVector(Object->m_vCenter2.x, Object->m_vCenter2.y, Object->m_vCenter2.z));
			
				FVector UpDir = Pos2 - Pos;
				const FVector Center = Pos + UpDir * 0.5f;
				const FLOAT Height = UpDir.Size( );
				if (Height != 0.0f)
					UpDir /= Height;
				const FVector RightDir = (UpDir ^ FVector(1, 0, 0)).SafeNormal();
				const FVector OutDir = (UpDir ^ RightDir).SafeNormal();

				FMatrix MatTransform(RightDir, OutDir, UpDir, FVector(0,0,0));

				FMatrix MatUnscaledL2W(RotatedLocalToWorld);
				MatUnscaledL2W.RemoveScaling( );
				MatUnscaledL2W.SetOrigin(FVector(0.0f, 0.0f, 0.0f));
				MatTransform *= MatUnscaledL2W;

				FRotationMatrix MatPostRotate(FRotator(0, 0, 16384));
				MatTransform = MatPostRotate * MatTransform;
				MatTransform.SetOrigin(Center);

				NxCapsuleShapeDesc* CapsuleShape = new NxCapsuleShapeDesc;
				CapsuleShape->setToDefault( );
				CapsuleShape->radius = Radius * U2PScale;
				CapsuleShape->height = Height * U2PScale;
				CapsuleShape->localPose = U2NTransform(MatTransform);
				nxActorDesc.shapes.pushBack(CapsuleShape);
			}
		}
		
		if( nxActorDesc.isValid() && nxActorDesc.shapes.size( ) > 0 && GWorld->RBPhysScene )
		{
			NxScene* NovodexScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();
			check(NovodexScene);
			NxActor* nxActor = NovodexScene->createActor(nxActorDesc);
			
			if( nxActor )
			{
				BodyInstance = GWorld->InstanceRBBody(NULL);
				BodyInstance->BodyData			= (FPointer)nxActor;
				BodyInstance->OwnerComponent	= this;
				nxActor->userData				= BodyInstance;
				BodyInstance->SceneIndex		= GWorld->RBPhysScene->NovodexSceneIndex;
			}
		}
			
		while(!nxActorDesc.shapes.isEmpty())
		{
			NxShapeDesc* Shape = nxActorDesc.shapes.back();
			nxActorDesc.shapes.popBack();
			delete Shape;
		};
	}

	UpdatePhysicsToRBChannels();
}
#endif // WITH_NOVODEX

#endif // #if WITH_SPEEDTREE

/** Returns the component's material corresponding to MeshType if it is set, otherwise returns the USpeedTree's material. */
UMaterialInterface* USpeedTreeComponent::GetMaterial(BYTE MeshType) const
{
#if WITH_SPEEDTREE
	if (MeshType > STMT_MinMinusOne && MeshType < STMT_Max)
	{
		UMaterialInterface* FoundMaterial = ChooseByMeshType<UMaterialInterface*>(
			MeshType,
			Branch1Material,
			Branch2Material,
			FrondMaterial,
			LeafCardMaterial,
			LeafMeshMaterial,
			BillboardMaterial);

		if ( !FoundMaterial && SpeedTree )
		{
			FoundMaterial = ChooseByMeshType<UMaterialInterface*>(
				MeshType,
				SpeedTree->Branch1Material,
				SpeedTree->Branch2Material,
				SpeedTree->FrondMaterial,
				SpeedTree->LeafCardMaterial,
				SpeedTree->LeafMeshMaterial,
				SpeedTree->BillboardMaterial);
		}

		return FoundMaterial;
	}
#endif
	return NULL;
}

/** Sets the component's material override, and reattaches if necessary. */
void USpeedTreeComponent::SetMaterial(BYTE MeshType,class UMaterialInterface* Material)
{
#if WITH_SPEEDTREE
	if (MeshType > STMT_MinMinusOne && MeshType < STMT_Max)
	{
		UMaterialInterface*& MaterialToModify = ChooseByMeshType<UMaterialInterface*&>(
			MeshType,
			Branch1Material,
			Branch2Material,
			FrondMaterial,
			LeafCardMaterial,
			LeafMeshMaterial,
			BillboardMaterial);

		if (MaterialToModify != Material)
		{
			MaterialToModify = Material;
			if (IsAttached())
			{
				BeginDeferredReattach();
			}
		}
	}
#endif
}

void USpeedTreeComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
#if WITH_SPEEDTREE
	// Don't allow the tree to be rotated if it has billboard leaves.
	if(IsValidComponent())
	{
		// Compute a rotation-less parent to world matrix.
		RotationOnlyMatrix = ParentToWorld;
		RotationOnlyMatrix.RemoveScaling();
		RotationOnlyMatrix.SetOrigin(FVector(0,0,0));
		RotationOnlyMatrix = RotationOnlyMatrix.Inverse();
		const FMatrix RotationlessParentToWorld = RotationOnlyMatrix * ParentToWorld;

		// Pass the rotation-less matrix to UPrimitiveComponent::SetParentToWorld.
		Super::SetParentToWorld(RotationlessParentToWorld);
	}
	else
#endif
	{
		Super::SetParentToWorld(ParentToWorld);
	}
}

UBOOL USpeedTreeComponent::IsValidComponent() const
{
#if WITH_SPEEDTREE
	// Only allow the component to be attached if it has a valid SpeedTree reference.
	return SpeedTree != NULL && SpeedTree->IsInitialized() && SpeedTree->SRH != NULL && SpeedTree->SRH->bIsInitialized && Super::IsValidComponent();
#else
	return FALSE;
#endif
}

UBOOL USpeedTreeComponent::AreNativePropertiesIdenticalTo(UComponent* Other) const
{
	UBOOL bNativePropertiesAreIdentical = Super::AreNativePropertiesIdenticalTo( Other );
	USpeedTreeComponent* OtherSpeedTreeComponent = CastChecked<USpeedTreeComponent>(Other);

	if( bNativePropertiesAreIdentical )
	{
		// Components are not identical if they have lighting information.
		if( StaticLights.Num() ||
			BranchLightMap ||
			FrondLightMap ||
			LeafCardLightMap ||
			LeafMeshLightMap || 
			BillboardLightMap ||
			OtherSpeedTreeComponent->StaticLights.Num() ||
			OtherSpeedTreeComponent->BranchLightMap ||
			OtherSpeedTreeComponent->FrondLightMap ||
			OtherSpeedTreeComponent->LeafCardLightMap ||
			OtherSpeedTreeComponent->LeafMeshLightMap || 
			OtherSpeedTreeComponent->BillboardLightMap)
		{
			bNativePropertiesAreIdentical = FALSE;
		}
	}

	return bNativePropertiesAreIdentical;
}

void USpeedTreeComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.Ver() >= VER_SPEEDTREE_5_INTEGRATION)
	{
		// Serialize the component's static lighting.
		Ar << BranchLightMap << FrondLightMap << LeafCardLightMap << BillboardLightMap;
		Ar << LeafMeshLightMap;
	}
	else
	{
		FLightMapRef DummyLightmap0;
		Ar << DummyLightmap0;
		FLightMapRef DummyLightmap1;
		Ar << DummyLightmap1;
		FLightMapRef DummyLightmap2;
		Ar << DummyLightmap2;
		FLightMapRef DummyLightmap3;
		Ar << DummyLightmap3;
	}
}

void USpeedTreeComponent::PostLoad()
{
	Super::PostLoad();

	// Initialize the light-map resources.
	if(BranchLightMap)
	{
		BranchLightMap->InitResources();
	}
	if(FrondLightMap)
	{
		FrondLightMap->InitResources();
	}
	if(LeafMeshLightMap)
	{
		LeafMeshLightMap->InitResources();
	}
	if(LeafCardLightMap)
	{
		LeafCardLightMap->InitResources();
	}
	if(BillboardLightMap)
	{
		BillboardLightMap->InitResources();
	}
}

void USpeedTreeComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
#if WITH_SPEEDTREE
	// make sure Lod level is valid
	if (LodLevelOverride > 1.0f)
	{
		LodLevelOverride = 1.0f;
	}

	if (LodLevelOverride < 0.0f && LodLevelOverride != -1.0f)
	{
		LodLevelOverride = -1.0f;
	}

#endif
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void USpeedTreeComponent::PostEditUndo()
{
	// Initialize the light-map resources.
	if(BranchLightMap)
	{
		BranchLightMap->InitResources();
	}
	if(FrondLightMap)
	{
		FrondLightMap->InitResources();
	}
	if(LeafMeshLightMap)
	{
		LeafMeshLightMap->InitResources();
	}
	if(LeafCardLightMap)
	{
		LeafCardLightMap->InitResources();
	}
	if(BillboardLightMap)
	{
		BillboardLightMap->InitResources();
	}

	Super::PostEditUndo();
}
