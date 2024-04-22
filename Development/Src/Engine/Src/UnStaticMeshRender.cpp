/*=============================================================================
	UnStaticMeshRender.cpp: Static mesh rendering code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineMeshClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineDecalClasses.h"
#include "LevelUtils.h"

#include "ScenePrivate.h"

#if XBOX
// Contains the optimized plane dots function
#include "UnStaticMeshRenderXe.h"
#endif

void UStaticMeshComponent::GenerateDecalRenderData(FDecalState* Decal, TArray< FDecalRenderData* >& OutDecalRenderDatas) const
{
	SCOPE_CYCLE_COUNTER(STAT_DecalStaticMeshAttachTime);

	OutDecalRenderDatas.Reset();

	// Do nothing if the specified decal doesn't project on static meshes.
	if ( !Decal->bProjectOnStaticMeshes )
	{
		return;
	}


	const FStaticMeshRenderData& StaticMeshRenderData = StaticMesh->LODModels(0);

	// Is the decal lit?
	const UBOOL bLitDecal = !Decal->bDecalMaterialHasUnlitLightingModel;

	// Static mesh light map info.
	INT LightMapWidth	= 0;
	INT LightMapHeight	= 0;
	if ( bLitDecal )
	{
		GetLightMapResolution( LightMapWidth, LightMapHeight );
	}

	// Should the decal use texture lightmapping?  FALSE if the decal is unlit.
	const UBOOL bHasLightMap =
		bLitDecal
		&& LODData.Num() > 0
		&& LODData(0).LightMap;

	// Should the decal use texture lightmapping?  FALSE if the decal has no lightmap.
	const UBOOL bUsingTextureLightmapping =
		bHasLightMap
		&& (LightMapWidth > 0)
		&& (LightMapHeight > 0) 
		&& (StaticMesh->LightMapCoordinateIndex >= 0) 
		&& ((UINT)StaticMesh->LightMapCoordinateIndex < StaticMeshRenderData.VertexBuffer.GetNumTexCoords());

	// Should the decal use vertex lightmapping?  FALSE if the decal has no lightmap.
	const UBOOL bUsingVertexLightmapping =
		bHasLightMap
		&& !bUsingTextureLightmapping;

	// Should the decal use software clipping?  FALSE if not wanted by the decal or if vertex lightmapping is used.
	// Vertex lightmapped decals MUST NOT use software clipping so that the vertex lightmap index remapping works.
	const UBOOL bUseSoftwareClipping = 
		!bUsingVertexLightmapping
		&& !Decal->bNoClip &&
		Decal->bStaticDecal &&
		!Decal->bMovableDecal;

	// We modify the incoming decal state to set whether or not we want to actually use software clipping
	Decal->bUseSoftwareClip = bUseSoftwareClipping;


	// Iterate over each mesh instance looking for hits.  Note that for non-instanced meshes
	// we'll only perform a single iteration here (InstanceCount will be 1.)
	const INT InstanceCount = IsPrimitiveInstanced() ? GetInstanceCount() : 1;
	for( INT CurInstanceIndex = 0; CurInstanceIndex < InstanceCount; ++CurInstanceIndex )
	{
		const FMatrix InstanceLocalToWorld = GetInstanceLocalToWorld( CurInstanceIndex );
		const FLOAT InstanceLocalToWorldDeterminant = InstanceLocalToWorld.Determinant();

		// Perform a kDOP query to retrieve intersecting leavesl.
		const FStaticMeshCollisionDataProvider MeshData( this, InstanceLocalToWorld, InstanceLocalToWorldDeterminant );
		TArray<TkDOPFrustumQuery<FStaticMeshCollisionDataProvider,WORD,UStaticMesh::kDOPTreeType>::FTriangleRun> Runs;
		TkDOPFrustumQuery<FStaticMeshCollisionDataProvider,WORD,UStaticMesh::kDOPTreeType> kDOPQuery( Decal->Planes.GetData(),
																			Decal->Planes.Num(),
																			Runs,
																			MeshData );
		const UStaticMesh::kDOPTreeType& kDOPTree = StaticMesh->kDOPTree;
		const UBOOL bHit = kDOPTree.FrustumQuery( kDOPQuery );
		// Early out if there is no overlap.
		if ( !bHit )
		{
			// Move on to the next instance
			continue;
		}

		FDecalRenderData* DecalRenderData = NULL;
	
		// Transform decal properties into local space.
		const FDecalLocalSpaceInfoClip DecalInfo( Decal, InstanceLocalToWorld, InstanceLocalToWorld.Inverse() );
		

		// Only use index buffer if we're clipping or need to remap indices.
		const UBOOL bUseIndexBuffer = bUseSoftwareClipping || bUsingVertexLightmapping || !Decal->bStaticDecal || Decal->bMovableDecal;

		// dynamic decals never clip and only create an index buffer
		if( !Decal->bStaticDecal || Decal->bMovableDecal )
		{
			// Allocate a FDecalRenderData object.  Use vertex factory from receiver static mesh
			DecalRenderData = new FDecalRenderData( NULL, FALSE, TRUE, &StaticMeshRenderData.VertexFactory );
			DecalRenderData->InstanceIndex = CurInstanceIndex;

			TArray<WORD>& VertexIndices = DecalRenderData->IndexBuffer.Indices;
			for( INT RunIndex = 0 ; RunIndex < Runs.Num() ; ++RunIndex )
			{
				const TkDOPFrustumQuery<FStaticMeshCollisionDataProvider,WORD,UStaticMesh::kDOPTreeType>::FTriangleRun& Run = Runs(RunIndex);
				const WORD FirstTriangle						= Run.FirstTriangle;
				const WORD LastTriangle							= FirstTriangle + Run.NumTriangles;

				for( WORD TriangleIndex=FirstTriangle; TriangleIndex<LastTriangle; ++TriangleIndex )
				{
					const FkDOPCollisionTriangle<WORD>& Triangle = kDOPTree.Triangles(TriangleIndex);

					// Calculate face direction, used for backface culling.
					const FVector& V1 = StaticMeshRenderData.PositionVertexBuffer.VertexPosition(Triangle.v1);
					const FVector& V2 = StaticMeshRenderData.PositionVertexBuffer.VertexPosition(Triangle.v2);
					const FVector& V3 = StaticMeshRenderData.PositionVertexBuffer.VertexPosition(Triangle.v3);
					FVector FaceNormal = (V2 - V1 ^ V3 - V1);

					// Normalize direction, if not possible skip triangle as it's zero surface.
					if( FaceNormal.Normalize(KINDA_SMALL_NUMBER) )
					{
						// Calculate angle between look vector and decal face normal.
						const FLOAT Dot = DecalInfo.LocalLookVector | FaceNormal;
						// Determine whether decal is front facing.
						const UBOOL bIsFrontFacing = Decal->bFlipBackfaceDirection ? -Dot > Decal->DecalComponent->BackfaceAngle : Dot > Decal->DecalComponent->BackfaceAngle;

						// Even if backface culling is disabled, reject triangles that view the decal at grazing angles.
						if( bIsFrontFacing || ( Decal->bProjectOnBackfaces && Abs( Dot ) > Decal->DecalComponent->BackfaceAngle ) )
						{
							VertexIndices.AddItem( Triangle.v1 );
							VertexIndices.AddItem( Triangle.v2 );
							VertexIndices.AddItem( Triangle.v3 );
						}
					}
				}
			}
			// Set triangle count.
			DecalRenderData->NumTriangles = VertexIndices.Num()/3;
			// set the blending interval
			DecalRenderData->DecalBlendRange = Decal->DecalComponent->CalcDecalDotProductBlendRange();

			OutDecalRenderDatas.AddItem( DecalRenderData );
		}
		else 
		{
			TArray<WORD> VertexIndices;
			VertexIndices.Empty(2000);
			for( INT RunIndex = 0 ; RunIndex < Runs.Num() ; ++RunIndex )
			{
				const TkDOPFrustumQuery<FStaticMeshCollisionDataProvider,WORD,UStaticMesh::kDOPTreeType>::FTriangleRun& Run = Runs(RunIndex);
				const WORD FirstTriangle						= Run.FirstTriangle;
				const WORD LastTriangle							= FirstTriangle + Run.NumTriangles;

				for( WORD TriangleIndex=FirstTriangle; TriangleIndex<LastTriangle; ++TriangleIndex )
				{
					const FkDOPCollisionTriangle<WORD>& Triangle = kDOPTree.Triangles(TriangleIndex);

					// Calculate face direction, used for backface culling.
					const FVector& V1 = StaticMeshRenderData.PositionVertexBuffer.VertexPosition(Triangle.v1);
					const FVector& V2 = StaticMeshRenderData.PositionVertexBuffer.VertexPosition(Triangle.v2);
					const FVector& V3 = StaticMeshRenderData.PositionVertexBuffer.VertexPosition(Triangle.v3);
					FVector FaceNormal = (V2 - V1 ^ V3 - V1);

					// Normalize direction, if not possible skip triangle as it's zero surface.
					if( FaceNormal.Normalize(KINDA_SMALL_NUMBER) )
					{
						// Calculate angle between look vector and decal face normal.
						const FLOAT Dot = DecalInfo.LocalLookVector | FaceNormal;
						// Determine whether decal is front facing.
						const UBOOL bIsFrontFacing = Decal->bFlipBackfaceDirection ? -Dot > Decal->DecalComponent->BackfaceAngle : Dot > Decal->DecalComponent->BackfaceAngle;

						// Even if backface culling is disabled, reject triangles that view the decal at grazing angles.
						if( bIsFrontFacing || ( Decal->bProjectOnBackfaces && Abs( Dot ) > Decal->DecalComponent->BackfaceAngle ) )
						{
							VertexIndices.AddItem( Triangle.v1 );
							VertexIndices.AddItem( Triangle.v2 );
							VertexIndices.AddItem( Triangle.v3 );
						}
					}
				}
			}

			if( VertexIndices.Num() > 0 )
			{
				// Allocate a FDecalRenderData object.
				DecalRenderData = new FDecalRenderData( NULL, TRUE, bUseIndexBuffer );
				DecalRenderData->InstanceIndex = CurInstanceIndex;

				// Create temporary structures.
				FDecalPoly Poly;
				FVector2D TempTexCoords;

				// Presize buffers.
				DecalRenderData->Vertices.Empty(VertexIndices.Num());
				if( bUseIndexBuffer )
				{
					DecalRenderData->IndexBuffer.Indices.Empty(VertexIndices.Num());
				}

				// Iterate over all collected triangle indices and process them.
				for( INT VertexIndexIndex=0; VertexIndexIndex<VertexIndices.Num(); VertexIndexIndex+=3 )
				{
					INT V1 = VertexIndices(VertexIndexIndex+0);
					INT V2 = VertexIndices(VertexIndexIndex+1);
					INT V3 = VertexIndices(VertexIndexIndex+2);

					// Set up FDecalPoly used for clipping.
					Poly.Init();
					new(Poly.Vertices) FVector(StaticMeshRenderData.PositionVertexBuffer.VertexPosition(V1));
					new(Poly.Vertices) FVector(StaticMeshRenderData.PositionVertexBuffer.VertexPosition(V2));
					new(Poly.Vertices) FVector(StaticMeshRenderData.PositionVertexBuffer.VertexPosition(V3));
					Poly.Indices.AddItem(V1);
					Poly.Indices.AddItem(V2);
					Poly.Indices.AddItem(V3);

					if ( bUsingTextureLightmapping )
					{
						new(Poly.ShadowTexCoords) FVector2D(StaticMeshRenderData.VertexBuffer.GetVertexUV(V1,StaticMesh->LightMapCoordinateIndex));
						new(Poly.ShadowTexCoords) FVector2D(StaticMeshRenderData.VertexBuffer.GetVertexUV(V2,StaticMesh->LightMapCoordinateIndex));
						new(Poly.ShadowTexCoords) FVector2D(StaticMeshRenderData.VertexBuffer.GetVertexUV(V3,StaticMesh->LightMapCoordinateIndex));
					}
					else
					{
						new(Poly.ShadowTexCoords) FVector2D(0.f, 0.f);
						new(Poly.ShadowTexCoords) FVector2D(0.f, 0.f);
						new(Poly.ShadowTexCoords) FVector2D(0.f, 0.f);
					}

					const UBOOL bClipPassed = bUseSoftwareClipping ? Poly.ClipAgainstConvex( DecalInfo.Convex ) : TRUE;
					if( bClipPassed )
					{
						if ( bUsingVertexLightmapping )
						{
							DecalRenderData->SampleRemapping.AddItem(V1);
							DecalRenderData->SampleRemapping.AddItem(V2);
							DecalRenderData->SampleRemapping.AddItem(V3);
						}

						const INT FirstVertexIndex = DecalRenderData->GetNumVertices(); 
						for ( INT i = 0 ; i < Poly.Vertices.Num() ; ++i )
						{
							// Create decal vertex tangent basis by projecting decal tangents onto the plane defined by the vertex plane.
							const INT VertexIndex = Poly.Indices(i);
							const FPackedNormal& DecalPackedTangentX = StaticMeshRenderData.VertexBuffer.VertexTangentX(VertexIndex);
							const FPackedNormal& DecalPackedTangentZ = StaticMeshRenderData.VertexBuffer.VertexTangentZ(VertexIndex);					

							// Store the decal vertex.
							new(DecalRenderData->Vertices) FDecalVertex(Poly.Vertices( i ),
								DecalPackedTangentX,
								DecalPackedTangentZ,
								Poly.ShadowTexCoords( i ));
						}

						if( bUseIndexBuffer )
						{
							// Triangulate the polygon and add indices to the index buffer
							const INT FirstIndex = DecalRenderData->GetNumIndices();
							for ( INT i = 0 ; i < Poly.Vertices.Num() - 2 ; ++i )
							{
								DecalRenderData->AddIndex( FirstVertexIndex+0 );
								DecalRenderData->AddIndex( FirstVertexIndex+i+1 );
								DecalRenderData->AddIndex( FirstVertexIndex+i+2 );
							}
						}
					}
				}
				// Set triangle count.
				DecalRenderData->NumTriangles = bUseIndexBuffer ? DecalRenderData->GetNumIndices()/3 : VertexIndices.Num()/3;
				// set the blending interval
				DecalRenderData->DecalBlendRange = Decal->DecalComponent->CalcDecalDotProductBlendRange();

				// Copy the vertex lightmap/shadowmap from the base mesh to the decal with remapping
				if (bUsingVertexLightmapping && DecalRenderData->SampleRemapping.Num() > 0 && LODData.Num() > 0)
				{
					// copy 1D lightmap from base mesh with remapping of indices
					const FLightMap* SourceLightMap = LODData(0).LightMap;
					if (SourceLightMap != NULL)
					{
						FLightMap1D* SourceLightMap1D = const_cast<FLightMap1D*>(SourceLightMap->GetLightMap1D());
						if (SourceLightMap1D != NULL)
						{
							// Create the vertex light map data.
							DecalRenderData->LightMap1D = SourceLightMap1D->DuplicateWithRemappedVerts( DecalRenderData->SampleRemapping );
						}
					}

					// copy 1D shadowmap from base mesh with remapping of indices
			 		if( LODData(0).ShadowVertexBuffers.Num() > 0 )
					{
						const TArray<UShadowMap1D*>& VertexShadowMaps = LODData(0).ShadowVertexBuffers;
						DecalRenderData->ShadowMap1D.Empty(VertexShadowMaps.Num());
						for( INT ShadowMapIndex = 0; ShadowMapIndex < VertexShadowMaps.Num(); ++ShadowMapIndex )
						{
							UShadowMap1D* SourceShadowMap1D = const_cast<UShadowMap1D*>(VertexShadowMaps(ShadowMapIndex));
							if (SourceShadowMap1D != NULL)
							{ 
								UObject* ShadowMapOuter = Decal->DecalComponent->GetOuter();
								// Create the vertex light map data.
								DecalRenderData->ShadowMap1D.AddItem(SourceShadowMap1D->DuplicateWithRemappedVerts( DecalRenderData->SampleRemapping, ShadowMapOuter ));
							}
						}
					}
				}

				OutDecalRenderDatas.AddItem( DecalRenderData );
			}
		}
	}
}

IMPLEMENT_COMPARE_CONSTPOINTER( FDecalInteraction, UnStaticMeshRender,
{
	return (A->DecalState.SortOrder <= B->DecalState.SortOrder) ? -1 : 1;
} );

/** Creates a light cache for the decal if it has a lit material. */
void FStaticMeshSceneProxy::CreateDecalLightCache(const FDecalInteraction& DecalInteraction)
{
	if ( DecalInteraction.DecalState.MaterialViewRelevance.bLit )
	{
		new(DecalLightCaches) FDecalLightCache( DecalInteraction, *this );
	}
}

/** Initialization constructor. */
FStaticMeshSceneProxy::FStaticMeshSceneProxy(const UStaticMeshComponent* Component):
	FPrimitiveSceneProxy(Component, Component->StaticMesh->GetFName()),
	Owner(Component->GetOwner()),
	StaticMesh(Component->StaticMesh),
	StaticMeshComponent(Component),
	ForcedLodModel(Component->ForcedLodModel),
	LODMaxRange(Component->OverriddenLODMaxRange > 0 ? Component->OverriddenLODMaxRange : Component->StaticMesh->LODMaxRange),
	LevelColor(1,1,1),
	PropertyColor(1,1,1),
	bCastShadow(Component->CastShadow),
	bShouldCollide(Component->ShouldCollide()),
	bBlockZeroExtent(Component->BlockZeroExtent),
	bBlockNonZeroExtent(Component->BlockNonZeroExtent),
	bBlockRigidBody(Component->BlockRigidBody),
	bForceStaticDecal(Component->bForceStaticDecals),
	MaterialViewRelevance(Component->GetMaterialViewRelevance()),
	WireframeColor(Component->WireframeColor)
{
	// Build the proxy's LOD data.
	LODs.Empty(StaticMesh->LODModels.Num());
	for(INT LODIndex = 0;LODIndex < StaticMesh->LODModels.Num();LODIndex++)
	{
		FLODInfo* NewLODInfo = new(LODs) FLODInfo(Component,LODIndex);

		// Under certain error conditions an LOD's material will be set to DefaultMaterial. Ensure our material view relevance is set properly.
		const INT ElementCount = NewLODInfo->Elements.Num();
		for ( INT ElementIndex = 0; ElementIndex < ElementCount; ++ElementIndex )
		{
			const FLODInfo::FElementInfo& ElementInfo = NewLODInfo->Elements( ElementIndex );
			if ( ElementInfo.Material == GEngine->DefaultMaterial )
			{
				MaterialViewRelevance |= GEngine->DefaultMaterial->GetViewRelevance();
			}
		}
	}

	// If the static mesh can accept decals, copy off statically irrelevant lights and light map guids.
	if( Component->bAcceptsStaticDecals ||
		Component->bAcceptsDynamicDecals )
	{
		for (INT DecalType = 0; DecalType < NUM_DECAL_TYPES; ++DecalType)
		{
			for( INT DecalIndex = 0 ; DecalIndex < Decals[DecalType].Num() ; ++DecalIndex )
			{
				// Create light cache information for any decals that were attached in the FPrimitiveSceneProxy ctor.
				// This needs to be executed on the rendering thread since the rendering thread may already be executing CreateDecalLightCache()
				ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
					CreateDecalLightCacheCommand,
					FStaticMeshSceneProxy*,StaticMeshSceneProxy,this,
					FDecalInteraction,DecalInteraction,*Decals[DecalType](DecalIndex),
				{
					StaticMeshSceneProxy->CreateDecalLightCache(DecalInteraction);
				});

				// Transform the decal frustum verts into local space.  These will be transformed
				// each frame into world space for the the scissor test.
				Decals[DecalType](DecalIndex)->DecalState.TransformFrustumVerts( GetInstanceLocalToWorld( Decals[DecalType](DecalIndex)->RenderData->InstanceIndex ) );
			}
		}
	}

#if !FINAL_RELEASE
	if( GIsEditor )
	{
		// Try to find a color for level coloration.
		if ( Owner )
		{
			ULevel* Level = Owner->GetLevel();
			ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
			if ( LevelStreaming )
			{
				LevelColor = LevelStreaming->DrawColor;
			}
		}

		// Get a color for property coloration.
		FColor TempPropertyColor;
		if (GEngine->GetPropertyColorationColor( (UObject*)Component, TempPropertyColor ))
		{
			PropertyColor = TempPropertyColor;
		}
	}
#endif
}

/** Add or remove elements to have the size in the specified range. Reconstructs elements if MaxSize<MinSize */
void UStaticMeshComponent::SetLODDataCount( const UINT MinSize, const UINT MaxSize )
{
	if (MaxSize < (UINT)LODData.Num())
	{
		// call destructors
		LODData.Remove(MaxSize, LODData.Num() - MaxSize);
	}
	
	if(MinSize > (UINT)LODData.Num())
	{
		// call constructors
		LODData.Reserve(MinSize);

		// TArray doesn't have a function for constructing n items
		UINT ItemCountToAdd = MinSize - LODData.Num();
		for(UINT i = 0; i < ItemCountToAdd; ++i)
		{
			// call constructor
			new (LODData)FStaticMeshComponentLODInfo();
		}
	}
}

/** Sets up a FMeshBatch for a specific LOD and element. */
UBOOL FStaticMeshSceneProxy::GetMeshElement(INT LODIndex,INT ElementIndex,INT FragmentIndex,BYTE InDepthPriorityGroup,const FMatrix& WorldToLocal,FMeshBatch& OutMeshElement, const UBOOL bUseSelectedMaterial, const UBOOL bUseHoveredMaterial) const
{
	const FStaticMeshRenderData& LODModel = StaticMesh->LODModels(LODIndex);
	const FStaticMeshElement& Element = LODModel.Elements(ElementIndex);

	const FLODInfo& ProxyLODInfo = LODs( LODIndex );
	UMaterialInterface* Material = ProxyLODInfo.Elements(ElementIndex).Material;
	OutMeshElement.MaterialRenderProxy = Material->GetRenderProxy(bUseSelectedMaterial,bUseHoveredMaterial);
	OutMeshElement.VertexFactory = &LODModel.VertexFactory;

	// Has the mesh component overridden the vertex color stream for this mesh LOD?
	if( ProxyLODInfo.OverrideColorVertexBuffer != NULL )
	{
		check( ProxyLODInfo.OverrideColorVertexFactory != NULL );

		// Make sure the indices are accessing data within the vertex buffer's
		if(Element.MaxVertexIndex < ProxyLODInfo.OverrideColorVertexBuffer->GetNumVertices())
		{
			// Switch out the stock mesh vertex factory with own own vertex factory that points to
			// our overridden color data
			OutMeshElement.VertexFactory = ProxyLODInfo.OverrideColorVertexFactory.GetOwnedPointer();
		}
	}

	const UBOOL bWireframe = FALSE;
#if WITH_D3D11_TESSELLATION
	const UBOOL bRequiresAdjacencyInformation = FTessellationMaterialPolicy::RequiresAdjacencyInformation( Material, OutMeshElement.VertexFactory->GetType() );
#else
	const UBOOL bRequiresAdjacencyInformation = FALSE;
#endif

	SetIndexSource(LODIndex, ElementIndex, FragmentIndex, OutMeshElement, bWireframe, bRequiresAdjacencyInformation );

	FMeshBatchElement& OutBatchElement = OutMeshElement.Elements(0);

	if(OutBatchElement.NumPrimitives > 0)
	{
		OutMeshElement.DynamicVertexData = NULL;
		OutMeshElement.LCI = &ProxyLODInfo;
		OutBatchElement.LocalToWorld = LocalToWorld;
		OutBatchElement.WorldToLocal = WorldToLocal;
		OutBatchElement.MinVertexIndex = Element.MinVertexIndex;
		OutBatchElement.MaxVertexIndex = Element.MaxVertexIndex;
		OutMeshElement.LODIndex = FMeshBatch::QuantizeLODIndex(LODs.Num() > 1 ? LODIndex : INDEX_NONE);
		OutMeshElement.UseDynamicData = FALSE;
		OutMeshElement.ReverseCulling = LocalToWorldDeterminant < 0.0f ? TRUE : FALSE;
		OutMeshElement.CastShadow = bCastShadow && Element.bEnableShadowCasting;
		OutMeshElement.DepthPriorityGroup = (ESceneDepthPriorityGroup)InDepthPriorityGroup;
		OutMeshElement.bUsePreVertexShaderCulling = TRUE;
		OutMeshElement.PlatformMeshData = Element.PlatformData;

		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/** Sets up a wireframe FMeshBatch for a specific LOD. */
UBOOL FStaticMeshSceneProxy::GetWireframeMeshElement(INT LODIndex, const FMaterialRenderProxy* WireframeRenderProxy, BYTE InDepthPriorityGroup, const FMatrix& WorldToLocal, FMeshBatch& OutMeshElement) const
{
	const FStaticMeshRenderData& LODModel = StaticMesh->LODModels(LODIndex);

	FMeshBatchElement& OutBatchElement = OutMeshElement.Elements(0);

	OutMeshElement.VertexFactory = &LODModel.VertexFactory;
	OutMeshElement.MaterialRenderProxy = WireframeRenderProxy;
	OutBatchElement.LocalToWorld = LocalToWorld;
	OutBatchElement.WorldToLocal = LocalToWorld.Inverse();
	OutBatchElement.MinVertexIndex = 0;
	OutBatchElement.MaxVertexIndex = LODModel.NumVertices - 1;
	OutMeshElement.ReverseCulling = LocalToWorldDeterminant < 0.0f ? TRUE : FALSE;
	OutMeshElement.CastShadow = bCastShadow;
	OutMeshElement.DepthPriorityGroup = (ESceneDepthPriorityGroup)InDepthPriorityGroup;
	OutMeshElement.bUsePreVertexShaderCulling = FALSE;
	OutMeshElement.PlatformMeshData = NULL;

	const UBOOL bWireframe = TRUE;
	const UBOOL bRequiresAdjacencyInformation = FALSE;

	SetIndexSource(LODIndex, 0, 0, OutMeshElement, bWireframe, bRequiresAdjacencyInformation);

	return TRUE;
}

/**
 * Sets IndexBuffer, FirstIndex and NumPrimitives of OutMeshElement.
 */
void FStaticMeshSceneProxy::SetIndexSource(INT LODIndex, INT ElementIndex, INT FragmentIndex, FMeshBatch& OutMeshElement, UBOOL bWireframe, UBOOL bRequiresAdjacencyInformation) const
{
	FMeshBatchElement& OutElement = OutMeshElement.Elements(0);
	const FStaticMeshRenderData& LODModel = StaticMesh->LODModels(LODIndex);
	if (bWireframe)
	{
		if( LODModel.WireframeIndexBuffer.IsInitialized()
#if WITH_D3D11_TESSELLATION
			&& !(GRHIShaderPlatform == SP_PCD3D_SM5 && OutMeshElement.VertexFactory->GetType()->SupportsTessellationShaders())
#endif
			)
		{
			OutMeshElement.Type = PT_LineList;
			OutElement.FirstIndex = 0;
			OutElement.IndexBuffer = &LODModel.WireframeIndexBuffer;
			OutElement.NumPrimitives = LODModel.WireframeIndexBuffer.Indices.Num() / 2;
		}
		else
		{
			OutMeshElement.Type = PT_TriangleList;
			OutElement.FirstIndex = 0;
			OutElement.IndexBuffer = &LODModel.IndexBuffer;
			OutElement.NumPrimitives = LODModel.IndexBuffer.Indices.Num() / 3;
			OutMeshElement.bWireframe = TRUE;
			OutMeshElement.bDisableBackfaceCulling = TRUE;
		}
	}
	else
	{
		const FStaticMeshElement& Element = LODModel.Elements(ElementIndex);
		OutMeshElement.Type = PT_TriangleList;
		OutElement.IndexBuffer = &LODModel.IndexBuffer;
		OutElement.FirstIndex = Element.FirstIndex;
		OutElement.NumPrimitives = Element.NumTriangles;
	}

#if WITH_D3D11_TESSELLATION
	if ( bRequiresAdjacencyInformation )
	{
		check( LODModel.AdjacencyIndexBuffer.Indices.Num() > 0 );
		OutElement.IndexBuffer = &LODModel.AdjacencyIndexBuffer;
		OutMeshElement.Type = PT_12_ControlPointPatchList;
		OutElement.FirstIndex *= 4;
	}
#endif // #if WITH_D3D11_TESSELLATION
}

// FPrimitiveSceneProxy interface.
void FStaticMeshSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	if(!HasViewDependentDPG() && !IsMovable())
	{
		// Determine the DPG the primitive should be drawn in.
		BYTE PrimitiveDPG = GetStaticDepthPriorityGroup();
		INT NumLODs = StaticMesh->LODModels.Num();
		//Never use the dynamic path in this path, because only unselected elements will use DrawStaticElements
		UBOOL bUseSelectedMaterial = FALSE;
		const UBOOL bUseHoveredMaterial = FALSE;
		

		//check if a LOD is being forced
		if (ForcedLodModel > 0) 
		{
			INT LODIndex = ::Clamp(ForcedLodModel, 1, NumLODs) - 1;
			const FStaticMeshRenderData& LODModel = StaticMesh->LODModels(LODIndex);
			// Draw the static mesh elements.
			const FMatrix WorldToLocal = LocalToWorld.Inverse();
			for(INT ElementIndex = 0;ElementIndex < LODModel.Elements.Num();ElementIndex++)
			{
#if WITH_EDITOR
				if( GIsEditor )
				{
					if( LODIndex < StaticMesh->LODInfo.Num() )
					{
						const FStaticMeshLODInfo& LODInfo = StaticMesh->LODInfo( LODIndex );
						if( ElementIndex < LODInfo.Elements.Num() )
						{
							const FStaticMeshLODElement& LODElement = LODInfo.Elements( ElementIndex );
							bUseSelectedMaterial = ( LODElement.bSelected && StaticMeshComponent->bCanHighlightSelectedSections );
						}
					}
				}
#endif // WITH_EDITOR
				for(INT FragmentIndex = 0;FragmentIndex < LODs(LODIndex).Elements(ElementIndex).NumFragments;FragmentIndex++)
				{
					FMeshBatch MeshElement;
					if(GetMeshElement(LODIndex,ElementIndex,FragmentIndex,PrimitiveDPG,WorldToLocal,MeshElement, bUseSelectedMaterial, bUseHoveredMaterial))
					{
						PDI->DrawMesh(MeshElement, 0, FLT_MAX);
					}
				}
			}
		} 
		else //no LOD is being forced, submit them all with appropriate cull distances
		{
			for(INT LODIndex = 0;LODIndex < NumLODs;LODIndex++)
			{
				const FStaticMeshRenderData& LODModel = StaticMesh->LODModels(LODIndex);
				const FMatrix WorldToLocal = LocalToWorld.Inverse();

				FLOAT MinDist = GetMinLODDist(LODIndex);
				FLOAT MaxDist = GetMaxLODDist(LODIndex);

				// Draw the static mesh elements.
				for(INT ElementIndex = 0;ElementIndex < LODModel.Elements.Num();ElementIndex++)
				{
#if WITH_EDITOR
					if( GIsEditor )
					{
						if( LODIndex < StaticMesh->LODInfo.Num() )
						{
							const FStaticMeshLODInfo& LODInfo = StaticMesh->LODInfo( LODIndex );
							if( ElementIndex < LODInfo.Elements.Num() )
							{
								const FStaticMeshLODElement& LODElement = LODInfo.Elements( ElementIndex );
								bUseSelectedMaterial = ( LODElement.bSelected && StaticMeshComponent->bCanHighlightSelectedSections );
							}
						}
					}
#endif // WITH_EDITOR
					for(INT FragmentIndex = 0;FragmentIndex < LODs(LODIndex).Elements(ElementIndex).NumFragments;FragmentIndex++)
					{
						FMeshBatch MeshElement;
						if(GetMeshElement(LODIndex,ElementIndex,FragmentIndex,PrimitiveDPG,WorldToLocal,MeshElement, bUseSelectedMaterial, bUseHoveredMaterial))
						{
							PDI->DrawMesh(MeshElement, MinDist, MaxDist);
						}
					}
				}
			}
		}
	}
}

/** Determines if any collision should be drawn for this mesh. */
UBOOL FStaticMeshSceneProxy::ShouldDrawCollision(const FSceneView* View)
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

/** Determines if the simple or complex collision should be drawn for a particular static mesh. */
UBOOL FStaticMeshSceneProxy::ShouldDrawSimpleCollision(const FSceneView* View, const UStaticMesh* Mesh)
{
	if(Mesh->UseSimpleBoxCollision && (View->Family->ShowFlags & SHOW_CollisionNonZeroExtent))
	{
		return TRUE;
	}	

	if(Mesh->UseSimpleLineCollision && (View->Family->ShowFlags & SHOW_CollisionZeroExtent))
	{
		return TRUE;
	}	

	if(Mesh->UseSimpleRigidBodyCollision && (View->Family->ShowFlags & SHOW_CollisionRigidBody))
	{
		return TRUE;
	}

	return FALSE;
}

/**
 * @return		The index into DecalLightCaches of the specified component, or INDEX_NONE if not found.
 */
INT FStaticMeshSceneProxy::FindDecalLightCacheIndex(const UDecalComponent* DecalComponent) const
{
	for( INT DecalIndex = 0 ; DecalIndex < DecalLightCaches.Num() ; ++DecalIndex )
	{
		const FDecalLightCache& DecalLightCache = DecalLightCaches(DecalIndex);
		if( DecalLightCache.GetDecalComponent() == DecalComponent )
		{
			return DecalIndex;
		}
	}
	return INDEX_NONE;
}

/**
* Draws the primitive's dynamic decal elements.  This is called from the rendering thread for each frame of each view.
* The dynamic elements will only be rendered if GetViewRelevance declares dynamic relevance.
* Called in the rendering thread.
*
* @param	PDI						The interface which receives the primitive elements.
* @param	View					The view which is being rendered.
* @param	InDepthPriorityGroup	The DPG which is being rendered.
* @param	bDynamicLightingPass	TRUE if drawing dynamic lights, FALSE if drawing static lights.
* @param	bDrawOpaqueDecals		TRUE if we want to draw opaque decals
* @param	bDrawTransparentDecals	TRUE if we want to draw transparent decals
* @param	bTranslucentReceiverPass	TRUE during the decal pass for translucent receivers, FALSE for opaque receivers.
*/
void FStaticMeshSceneProxy::DrawDynamicDecalElements(
									  FPrimitiveDrawInterface* PDI,
									  const FSceneView* View,
									  UINT InDepthPriorityGroup,
									  UBOOL bDynamicLightingPass,
									  UBOOL bDrawOpaqueDecals,
									  UBOOL bDrawTransparentDecals,
									  UBOOL bTranslucentReceiverPass
									  )
{
	SCOPE_CYCLE_COUNTER(STAT_DecalRenderDynamicSMTime);

	checkSlow( View->Family->ShowFlags & SHOW_Decals );

#if !FINAL_RELEASE
	UBOOL bRichView = IsRichView(View);
#else
	UBOOL bRichView = FALSE;
#endif
	// only render decals that haven't been added to a static batch
	INT StartDecalType = !bRichView ? DYNAMIC_DECALS : STATIC_DECALS;

	// Compute the set of decals in this DPG.
	FMemMark MemStackMark(GRenderingThreadMemStack);
 	TArray<FDecalInteraction*,TMemStackAllocator<GRenderingThreadMemStack> > DPGDecals;
	for (INT DecalType = StartDecalType; DecalType < NUM_DECAL_TYPES; ++DecalType)
	{
		for ( INT DecalIndex = 0 ; DecalIndex < Decals[DecalType].Num() ; ++DecalIndex )
		{
			FDecalInteraction* Interaction = Decals[DecalType](DecalIndex);
			if(
				// match current DPG
				InDepthPriorityGroup == Interaction->DecalState.DepthPriorityGroup &&
				// only render transparent or opaque decals as they are requested
				((Interaction->DecalState.MaterialViewRelevance.bTranslucency && bDrawTransparentDecals) || (Interaction->DecalState.MaterialViewRelevance.bOpaque && bDrawOpaqueDecals)) &&
				// only render lit decals during dynamic lighting pass
				((Interaction->DecalState.MaterialViewRelevance.bLit && bDynamicLightingPass) || !bDynamicLightingPass) )
			{
				DPGDecals.AddItem( Interaction );
			}
		}
	}
	// Sort decals for the translucent receiver pass
	if( bTranslucentReceiverPass )
	{
		Sort<USE_COMPARE_CONSTPOINTER(FDecalInteraction,UnStaticMeshRender)>( DPGDecals.GetTypedData(), DPGDecals.Num() );
	}
	for ( INT DecalIndex = 0 ; DecalIndex < DPGDecals.Num() ; ++DecalIndex )
	{
		FDecalInteraction* Decal	= DPGDecals(DecalIndex);
		const FDecalState& DecalState	= Decal->DecalState;
		FDecalRenderData* RenderData = Decal->RenderData;

		if( RenderData->DecalVertexFactory &&
			RenderData->NumTriangles > 0 )
		{
			UBOOL bIsDecalVisible = TRUE;

//@todo decal - decal bounds are not getting updated, so this culling won't work
#if 0
			const FBox& DecalBoundingBox = Decal->DecalState.Bounds;

			// Distance cull using decal's CullDistance (perspective views only)
			if( bIsDecalVisible && View->ViewOrigin.W > 0.0f )
			{
				// Compute the distance between the view and the decal
				FLOAT SquaredDistance = ( DecalBoundingBox.GetCenter() - View->ViewOrigin ).SizeSquared();
			    const FLOAT SquaredCullDistance = Decal->DecalState.SquaredCullDistance;
			    if( SquaredCullDistance > 0.0f && SquaredDistance > SquaredCullDistance )
			    {
				    // Too far away to render
				    bIsDecalVisible = FALSE;
			    }
			}

			if( bIsDecalVisible )
			{
				// Make sure the decal's frustum bounds are in view
				if( !View->ViewFrustum.IntersectBox( DecalBoundingBox.GetCenter(), DecalBoundingBox.GetExtent() ) )
				{
					bIsDecalVisible = FALSE;
				}
			}
#endif

			if( bIsDecalVisible )
			{
				const UDecalComponent* DecalComponent = Decal->Decal;
				const INT DecalLightCacheIndex = FindDecalLightCacheIndex( DecalComponent );				

				FMeshBatch MeshElement;
				FMeshBatchElement& BatchElement = MeshElement.Elements(0);
				BatchElement.IndexBuffer = RenderData->bUsesIndexResources ? &RenderData->IndexBuffer : NULL;
				MeshElement.VertexFactory = RenderData->DecalVertexFactory->CastToFVertexFactory();

				MeshElement.MaterialRenderProxy = DecalState.DecalMaterial->GetRenderProxy(FALSE);
				if( DecalState.bDecalMaterialHasStaticLightingUsage )
				{
					if( bTranslucentReceiverPass )
					{
						MeshElement.LCI = RenderData->LCI;
					}
					else
					{
						MeshElement.LCI = ( DecalLightCacheIndex != INDEX_NONE ) ? &DecalLightCaches(DecalLightCacheIndex) : NULL;
					}
				}
				else
				{
					MeshElement.LCI = NULL;
				}


				// Grab the local -> world matrix for this mesh.  For instanced components, we'll also take into
				// account the instance -> local transform by using the instancing API.
				checkSlow( !IsPrimitiveInstanced() || RenderData->InstanceIndex != INDEX_NONE );
				const FMatrix& InstanceLocalToWorld = GetInstanceLocalToWorld( RenderData->InstanceIndex );
				const FLOAT InstanceLocalToWorldDeterminant = InstanceLocalToWorld.Determinant();

				BatchElement.LocalToWorld = InstanceLocalToWorld;
				BatchElement.WorldToLocal = InstanceLocalToWorld.Inverse();
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = RenderData->NumTriangles;

				const FStaticMeshRenderData& LODModel = StaticMesh->LODModels(0);
				BatchElement.MinVertexIndex = 0;
				if( RenderData->ReceiverVertexFactory )
				{
					BatchElement.MaxVertexIndex = LODModel.NumVertices-1;
				}
				else
				{
					BatchElement.MaxVertexIndex = RenderData->DecalVertexBuffer.GetNumVertices()-1;
				}
				MeshElement.ReverseCulling = InstanceLocalToWorldDeterminant < 0.0f ? TRUE : FALSE;
				MeshElement.CastShadow = FALSE;
				MeshElement.DepthBias = DecalState.DepthBias;
				MeshElement.SlopeScaleDepthBias = DecalState.SlopeScaleDepthBias;
				MeshElement.Type = PT_TriangleList;
				MeshElement.DepthPriorityGroup = InDepthPriorityGroup;
				MeshElement.bIsDecal = TRUE;
				MeshElement.DecalState = &Decal->DecalState;
				MeshElement.bUsePreVertexShaderCulling = FALSE;
				MeshElement.PlatformMeshData = NULL;

				// set decal vertex factory parameters (in local space)
				const FDecalLocalSpaceInfo DecalLocal(&DecalState,DecalState.AttachmentLocalToWorld,DecalState.AttachmentLocalToWorld.Inverse());
				RenderData->DecalVertexFactory->SetDecalMatrix(DecalLocal.TextureTransform);
				RenderData->DecalVertexFactory->SetDecalLocation(DecalLocal.LocalLocation);
				RenderData->DecalVertexFactory->SetDecalOffset(FVector2D(DecalState.OffsetX, DecalState.OffsetY));
				RenderData->DecalVertexFactory->SetDecalLocalBinormal(DecalLocal.LocalBinormal);
				RenderData->DecalVertexFactory->SetDecalLocalTangent(DecalLocal.LocalTangent);
				RenderData->DecalVertexFactory->SetDecalLocalNormal(DecalLocal.LocalNormal);

				static const FLinearColor WireColor(0.5f,1.0f,0.5f);
				const INT NumPasses = DrawRichMesh(PDI,MeshElement,WireColor,LevelColor,PropertyColor,PrimitiveSceneInfo,FALSE);

				INC_DWORD_STAT_BY(STAT_DecalTriangles,MeshElement.GetNumPrimitives()*NumPasses);
				INC_DWORD_STAT(STAT_DecalDrawCalls);

#if 0
				if( RenderData )
				{
					RenderData->DebugDraw(PDI,DecalState,InstanceLocalToWorld,SDPG_World);
				}
#endif
			}
		}
	}
}

/**
* Draws the primitive's static decal elements.  This is called from the game thread whenever this primitive is attached
* as a receiver for a decal.
*
* The static elements will only be rendered if GetViewRelevance declares both static and decal relevance.
* Called in the game thread.
*
* @param PDI - The interface which receives the primitive elements.
*/
void FStaticMeshSceneProxy::DrawStaticDecalElements(FStaticPrimitiveDrawInterface* PDI,const FDecalInteraction& DecalInteraction)
{
	if( !HasViewDependentDPG() &&
		// decals should render dynamically on movable meshes 
		UseStaticDecal() &&
		// only add static decal batches for decals projecting on *strictly* opaque receivers or if the decal is opaque itself
		((MaterialViewRelevance.bOpaque && !MaterialViewRelevance.bTranslucency) || DecalInteraction.DecalState.MaterialViewRelevance.bOpaque) &&
		DecalInteraction.RenderData->DecalVertexFactory &&
		DecalInteraction.RenderData->NumTriangles > 0 )
	{
		const FDecalState& DecalState	= DecalInteraction.DecalState;
		FDecalRenderData* RenderData = DecalInteraction.RenderData;

		FMeshBatch MeshElement;
		FMeshBatchElement& BatchElement = MeshElement.Elements(0);
		BatchElement.IndexBuffer = RenderData->bUsesIndexResources ? &RenderData->IndexBuffer : NULL;		
		MeshElement.VertexFactory = RenderData->DecalVertexFactory->CastToFVertexFactory();
		MeshElement.MaterialRenderProxy = DecalState.DecalMaterial->GetRenderProxy(FALSE);

		// This makes the decal render using a scissor rect (for performance reasons).
		MeshElement.DecalState = &DecalState;

		// Grab the local -> world matrix for this mesh.  For instanced components, we'll also take into
		// account the instance -> local transform by using the instancing API.
		checkSlow( !IsPrimitiveInstanced() || RenderData->InstanceIndex != INDEX_NONE );
		const FMatrix& InstanceLocalToWorld = GetInstanceLocalToWorld( RenderData->InstanceIndex );
		const FLOAT InstanceLocalToWorldDeterminant = InstanceLocalToWorld.Determinant();

		BatchElement.LocalToWorld = InstanceLocalToWorld;
		BatchElement.WorldToLocal = InstanceLocalToWorld.Inverse();
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = RenderData->NumTriangles;
		const FStaticMeshRenderData& LODModel = StaticMesh->LODModels(0);
		BatchElement.MinVertexIndex = 0;
		if( RenderData->ReceiverVertexFactory )
		{
			BatchElement.MaxVertexIndex = LODModel.NumVertices-1;
		}
		else
		{
			BatchElement.MaxVertexIndex = RenderData->DecalVertexBuffer.GetNumVertices()-1;
		}		
		MeshElement.ReverseCulling = InstanceLocalToWorldDeterminant < 0.0f ? TRUE : FALSE;
		MeshElement.CastShadow = FALSE;
		MeshElement.DepthBias = DecalState.DepthBias;
		MeshElement.SlopeScaleDepthBias = DecalState.SlopeScaleDepthBias;
		MeshElement.Type = PT_TriangleList;
		MeshElement.DepthPriorityGroup = GetStaticDepthPriorityGroup();
		MeshElement.bIsDecal = TRUE;
		MeshElement.bUsePreVertexShaderCulling = FALSE;
		MeshElement.PlatformMeshData = NULL;

		// set decal vertex factory parameters (in local space)
		const FDecalLocalSpaceInfo DecalLocal(&DecalState,DecalState.AttachmentLocalToWorld,DecalState.AttachmentLocalToWorld.Inverse());
		RenderData->DecalVertexFactory->SetDecalMatrix(DecalLocal.TextureTransform);
		RenderData->DecalVertexFactory->SetDecalLocation(DecalLocal.LocalLocation);
		RenderData->DecalVertexFactory->SetDecalOffset(FVector2D(DecalState.OffsetX, DecalState.OffsetY));
		RenderData->DecalVertexFactory->SetDecalLocalBinormal(DecalLocal.LocalBinormal);
		RenderData->DecalVertexFactory->SetDecalLocalTangent(DecalLocal.LocalTangent);
		RenderData->DecalVertexFactory->SetDecalLocalNormal(DecalLocal.LocalNormal);
		
		MeshElement.LCI = NULL;
		if( DecalState.bDecalMaterialHasStaticLightingUsage )
		{
			// The decal is lit and so should have an entry in the DecalLightCaches list.
			const INT DecalLightCacheIndex = FindDecalLightCacheIndex( DecalInteraction.Decal );
			if( DecalLightCaches.IsValidIndex(DecalLightCacheIndex) )
			{
				MeshElement.LCI = &DecalLightCaches(DecalLightCacheIndex);
			}
			else
			{
				debugfSuppressed( TEXT("Missing Decal LCI Receiver=%s Decal=%s"), 
					Owner ? *Owner->GetName() : TEXT("None"),
					DecalState.DecalComponent && DecalState.DecalComponent->GetOwner() 
					? *DecalState.DecalComponent->GetOwner()->GetName() : TEXT("None") );
			}
		}

		PDI->DrawMesh(MeshElement,0,FLT_MAX);
	}	
}

/**
 * Adds a decal interaction to the primitive.  This is called in the rendering thread by AddDecalInteraction_GameThread.
 */
void FStaticMeshSceneProxy::AddDecalInteraction_RenderingThread(const FDecalInteraction& DecalInteraction)
{
	INT DecalType;
	//make a copy from the template that will be owned by the proxy
	FDecalInteraction* NewInteraction = new FDecalInteraction(DecalInteraction);
	// Cache any lighting information for the decal.
	CreateDecalLightCache( *NewInteraction );
	FPrimitiveSceneProxy::AddDecalInteraction_Internal_RenderingThread( NewInteraction, DecalType );

	// Transform the decal frustum verts into local space.  These will be transformed
	// each frame into world space for the the scissor test.
	const FMatrix& InstanceLocalToWorld = GetInstanceLocalToWorld( NewInteraction->RenderData->InstanceIndex );
	NewInteraction->DecalState.TransformFrustumVerts( InstanceLocalToWorld.Inverse() );

}

/**
 * Removes a decal interaction from the primitive.  This is called in the rendering thread by RemoveDecalInteraction_GameThread.
 */
void FStaticMeshSceneProxy::RemoveDecalInteraction_RenderingThread(UDecalComponent* DecalComponent)
{
	FPrimitiveSceneProxy::RemoveDecalInteraction_RenderingThread( DecalComponent );

	// Find the decal interaction representing the given decal component, and remove it from the interaction list.
	const INT DecalLightCacheIndex = FindDecalLightCacheIndex( DecalComponent );
	if ( DecalLightCacheIndex != INDEX_NONE )
	{
		DecalLightCaches.Remove( DecalLightCacheIndex );
	}
}

/** 
* Draw the scene proxy as a dynamic element
*
* @param	PDI - draw interface to render to
* @param	View - current view
* @param	DPGIndex - current depth priority 
* @param	Flags - optional set of flags from EDrawDynamicElementFlags
*/
void FStaticMeshSceneProxy::DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
{
	// Bool that defines how we should draw the collision for this mesh.
	const UBOOL bIsCollisionView = IsCollisionView(View);
	const UBOOL bDrawCollision = bIsCollisionView && ShouldDrawCollision(View);
	const UBOOL bDrawSimple = ShouldDrawSimpleCollision(View, StaticMesh);
	const UBOOL bDrawComplexCollision = (bDrawCollision && !bDrawSimple);
	const UBOOL bDrawSimpleCollision = (bDrawCollision && bDrawSimple);

	UBOOL bIsSelected = bSelected;

	// Determine the DPG the primitive should be drawn in for this view.
	if (GetDepthPriorityGroup(View) == DPGIndex)
	{
		// Check to see if this mesh is currently fading in or out.  If it's fading then it's been temporarily moved
		// from static relevance to dynamic relevance for the masked fade and we we'll definitely want to draw it 
		const FSceneViewState* SceneViewState = static_cast<const FSceneViewState*>( View->State );
		INT FadingInLODIndex = INDEX_NONE;
		INT FadingOutLODIndex = INDEX_NONE;
		const UBOOL bIsMaskedForScreenDoorFade =
			SceneViewState != NULL && PrimitiveSceneInfo != NULL &&
			SceneViewState->IsPrimitiveFading( PrimitiveSceneInfo->Component, FadingInLODIndex, FadingOutLODIndex );

		const UBOOL bDrawMesh = bIsCollisionView ?
			bDrawComplexCollision :
			IsRichView(View) || HasViewDependentDPG() || IsMovable() || (View->Family->ShowFlags & (SHOW_Bounds | SHOW_Collision)) || bIsMaskedForScreenDoorFade || bSelected || bHovered;

		// Draw the fading in and fading out LODs if they are valid instead of the actual LOD based on distance from the view.
		// This prevents popping from transitioning LODs when a transition fade is already active.
		const INT LODsToDraw[] = {FadingInLODIndex == INDEX_NONE ? GetLOD(View) : FadingInLODIndex, FadingOutLODIndex};
		for (INT LODLoopIndex = 0; LODLoopIndex < ARRAY_COUNT(LODsToDraw) && LODsToDraw[LODLoopIndex] != INDEX_NONE && LODsToDraw[LODLoopIndex] < StaticMesh->LODModels.Num(); LODLoopIndex++)
		{
			const INT LODIndex = LODsToDraw[LODLoopIndex];
			// Draw polygon mesh if we are either not in a collision view, or are drawing it as collision.
			if((View->Family->ShowFlags & SHOW_StaticMeshes) && bDrawMesh )
			{
				const UBOOL bLevelColorationEnabled = (View->Family->ShowFlags & SHOW_LevelColoration) ? TRUE : FALSE;
				const UBOOL bPropertyColorationEnabled = (View->Family->ShowFlags & SHOW_PropertyColoration) ? TRUE : FALSE;

				const FStaticMeshRenderData& LODModel = StaticMesh->LODModels(LODIndex);
				const FLODInfo& ProxyLODInfo = LODs( LODIndex );

				if (AllowDebugViewmodes() && (View->Family->ShowFlags & SHOW_Wireframe) && !(View->Family->ShowFlags & SHOW_Materials)
					// If any of the materials are mesh-modifying, we can't use the single merged mesh element of GetWireframeMeshElement()
					&& !ProxyLODInfo.UsesMeshModifyingMaterials())
				{
					FLinearColor ViewWireframeColor( bLevelColorationEnabled ? LevelColor : WireframeColor );
					if ( bPropertyColorationEnabled )
					{
						ViewWireframeColor = PropertyColor;
					}
					// Use collision color if we are drawing this as collision
					else if(bDrawComplexCollision)
					{
						ViewWireframeColor = FLinearColor(GEngine->C_ScaleBoxHi);
					}

					FColoredMaterialRenderProxy WireframeMaterialInstance(
						GEngine->WireframeMaterial->GetRenderProxy(FALSE),
						ConditionalAdjustForMobileEmulation(View, GetSelectionColor(ViewWireframeColor,!(GIsEditor && (View->Family->ShowFlags & SHOW_Selection)) || bSelected, bHovered))
						);

					FMeshBatch Mesh;

					if (GetWireframeMeshElement(LODIndex, &WireframeMaterialInstance, DPGIndex, LocalToWorld.Inverse(), Mesh))
					{
						const INT NumPasses = PDI->DrawMesh(Mesh);

						INC_DWORD_STAT_BY(STAT_StaticMeshTriangles,Mesh.GetNumPrimitives() * NumPasses);
					}
				}
				else
				{
					const FLinearColor UtilColor( IsCollisionView(View) ? FLinearColor(GEngine->C_ScaleBoxHi) : LevelColor );
					const FMatrix WorldToLocal = LocalToWorld.Inverse();

					// Draw the static mesh elements.
					for(INT ElementIndex = 0;ElementIndex < LODModel.Elements.Num();ElementIndex++)
					{
#if WITH_EDITOR
						if( GIsEditor )
						{
							if( LODIndex < StaticMesh->LODInfo.Num() )
							{
								const FStaticMeshLODInfo& LODInfo = StaticMesh->LODInfo( LODIndex );
								if( ElementIndex < LODInfo.Elements.Num() )
								{
									const FStaticMeshLODElement& LODElement = LODInfo.Elements( ElementIndex );
									bIsSelected = bSelected || ( LODElement.bSelected && StaticMeshComponent->bCanHighlightSelectedSections );
								}
							}
						}
#endif // WITH_EDITOR
						for(INT FragmentIndex = 0;FragmentIndex < LODs(LODIndex).Elements(ElementIndex).NumFragments;FragmentIndex++)
						{
							FMeshBatch MeshElement;
							if(GetMeshElement(LODIndex,ElementIndex,FragmentIndex,DPGIndex,WorldToLocal,MeshElement, bIsSelected, bHovered))
							{
#if !FINAL_RELEASE
								if( View->Family->ShowFlags & SHOW_VertexColors && AllowDebugViewmodes() )
								{
									// Temporarily disable Emulate Mobile Rendering
									GForceDisableEmulateMobileRendering = TRUE;

									// Override the mesh's material with our material that draws the vertex colors
									UMaterial* VertexColorVisualizationMaterial = NULL;
									switch( GVertexColorViewMode )
									{
									case EVertexColorViewMode::Color:
										VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_ColorOnly;
										break;

									case EVertexColorViewMode::Alpha:
										VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_AlphaAsColor;
										break;

									case EVertexColorViewMode::Red:
										VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_RedOnly;
										break;

									case EVertexColorViewMode::Green:
										VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_GreenOnly;
										break;

									case EVertexColorViewMode::Blue:
										VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_BlueOnly;
										break;
									}
									check( VertexColorVisualizationMaterial != NULL );

									const FColoredMaterialRenderProxy VertexColorVisualizationMaterialInstance(
										VertexColorVisualizationMaterial->GetRenderProxy( MeshElement.MaterialRenderProxy->IsSelected(),MeshElement.MaterialRenderProxy->IsHovered() ),
										ConditionalAdjustForMobileEmulation(View, GetSelectionColor( FLinearColor::White, bIsSelected, bHovered )) );
									FMeshBatch ModifiedMeshElement = MeshElement;
									ModifiedMeshElement.MaterialRenderProxy = &VertexColorVisualizationMaterialInstance;

									const INT NumPasses = DrawRichMesh(
										PDI,
										ModifiedMeshElement,
										WireframeColor,
										UtilColor,
										PropertyColor,
										PrimitiveSceneInfo,
										bIsSelected
										);
									INC_DWORD_STAT_BY(STAT_StaticMeshTriangles,MeshElement.GetNumPrimitives() * NumPasses);
									
									// Restore Emulate Mobile Rendering
									GForceDisableEmulateMobileRendering = FALSE;
								}
								else
#endif
								{
									const INT NumPasses = DrawRichMesh(
										PDI,
										MeshElement,
										WireframeColor,
										UtilColor,
										PropertyColor,
										PrimitiveSceneInfo,
										bIsSelected
										);
									INC_DWORD_STAT_BY(STAT_StaticMeshTriangles,MeshElement.GetNumPrimitives() * NumPasses);
								}

							}
						}
					}
				}
			}
		}
	}

	if(DPGIndex == SDPG_World 
		&& ((View->Family->ShowFlags & SHOW_Collision) && bShouldCollide || bDrawSimpleCollision)
		&& AllowDebugViewmodes())
	{
		if(StaticMesh->BodySetup)
		{
			// Make a material for drawing solid collision stuff
			const UMaterial* LevelColorationMaterial = (View->Family->ShowFlags & SHOW_Lighting) ? GEngine->ShadedLevelColorationLitMaterial : GEngine->ShadedLevelColorationUnlitMaterial;
			const FColoredMaterialRenderProxy CollisionMaterialInstance(
				LevelColorationMaterial->GetRenderProxy(bSelected, bHovered),
				ConditionalAdjustForMobileEmulation(View, WireframeColor)
				);

			// Draw the static mesh's body setup.

			// Get transform without scaling.
			FMatrix GeomMatrix = LocalToWorld;
			FVector RecipScale( 1.f/TotalScale3D.X, 1.f/TotalScale3D.Y, 1.f/TotalScale3D.Z );

			GeomMatrix.M[0][0] *= RecipScale.X;
			GeomMatrix.M[0][1] *= RecipScale.X;
			GeomMatrix.M[0][2] *= RecipScale.X;

			GeomMatrix.M[1][0] *= RecipScale.Y;
			GeomMatrix.M[1][1] *= RecipScale.Y;
			GeomMatrix.M[1][2] *= RecipScale.Y;

			GeomMatrix.M[2][0] *= RecipScale.Z;
			GeomMatrix.M[2][1] *= RecipScale.Z;
			GeomMatrix.M[2][2] *= RecipScale.Z;

			// Slight hack here - draw each hull in a different color if no Owner (usually in a tool like StaticMeshEditor).
			UBOOL bDrawSimpleSolid = (bDrawSimpleCollision && !(View->Family->ShowFlags & SHOW_Wireframe));

			// In old wireframe collision mode, always draw the wireframe highlighted (selected or not).
			UBOOL bDrawWireSelected = bSelected;
			if(View->Family->ShowFlags & SHOW_Collision)
			{
				bDrawWireSelected = TRUE;
			}
 
			// Differentiate the color based on bBlockNonZeroExtent.  Helps greatly with skimming a level for optimization opportunities.
			FColor collisionColor = bBlockNonZeroExtent ? FColor(223,149,157,255) : FColor(157,149,223,255);

			StaticMesh->BodySetup->AggGeom.DrawAggGeom(PDI, GeomMatrix, TotalScale3D, GetSelectionColor(collisionColor, bDrawWireSelected, bHovered), &CollisionMaterialInstance, (Owner == NULL), bDrawSimpleSolid);
		}
	}

	if (View->Family->ShowFlags & SHOW_StaticMeshes)
	{
		RenderBounds(PDI, DPGIndex, View->Family->ShowFlags, PrimitiveSceneInfo->Bounds, !Owner || bSelected);
	}
}

void FStaticMeshSceneProxy::OnTransformChanged()
{
	// Update the cached scaling.
	TotalScale3D.X = FVector(LocalToWorld.TransformNormal(FVector(1,0,0))).Size();
	TotalScale3D.Y = FVector(LocalToWorld.TransformNormal(FVector(0,1,0))).Size();
	TotalScale3D.Z = FVector(LocalToWorld.TransformNormal(FVector(0,0,1))).Size();
}

FPrimitiveViewRelevance FStaticMeshSceneProxy::GetViewRelevance(const FSceneView* View)
{   
	UBOOL bRenderStaticMeshes = (View->Family->ShowFlags & SHOW_StaticMeshes) != 0;

	if(!TEST_PROFILEEXSTATE(0x400, View->Family->CurrentRealTime))
	{
		bRenderStaticMeshes = FALSE;
	}

	FPrimitiveViewRelevance Result;
	if(bRenderStaticMeshes)
	{
		if(IsShown(View))
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
				IsMovable()
#if WITH_EDITOR
				//only check these in the editor
				|| bSelected || bHovered
#endif
				)
			{
				SetRelevanceForShowBounds(View->Family->ShowFlags, Result);
				Result.bDynamicRelevance = TRUE;
			}
			else
			{
				// Is the mesh currently fading in or out?
				const FSceneViewState* SceneViewState = static_cast<const FSceneViewState*>( View->State );
				const UBOOL bIsMaskedForScreenDoorFade =
					GAllowScreenDoorFade && SceneViewState != NULL && PrimitiveSceneInfo != NULL &&
					SceneViewState->IsPrimitiveFading( PrimitiveSceneInfo->Component );
				if( bIsMaskedForScreenDoorFade )
				{
					// Screen-door fading primitives are implicitly masked and always need depth rendered with
					// a shader, so we'll bucket them with dynamic meshes for this
					Result.bDynamicRelevance = TRUE;
					Result.bStaticButFadingRelevance = TRUE;
				}
				else
				{
					Result.bStaticRelevance = TRUE;
				}
			}
			Result.SetDPG(GetDepthPriorityGroup(View),TRUE);
		}

		if (IsShadowCast(View))
		{
			Result.bShadowRelevance = TRUE;
		}

		MaterialViewRelevance.SetPrimitiveViewRelevance(Result);

		if (!(View->Family->ShowFlags & SHOW_Materials) || (View->Family->ShowFlags & SHOW_VertexColors))
		{
			Result.bOpaqueRelevance = TRUE;
		}

		Result.bDecalStaticRelevance = HasRelevantStaticDecals(View);
		Result.bDecalDynamicRelevance = HasRelevantDynamicDecals(View);
	}
	return Result;
}


/**
 *	Determines the relevance of this primitive's elements to the given light.
 *	@param	LightSceneInfo			The light to determine relevance for
 *	@param	bDynamic (output)		The light is dynamic for this primitive
 *	@param	bRelevant (output)		The light is relevant for this primitive
 *	@param	bLightMapped (output)	The light is light mapped for this primitive
 */
void FStaticMeshSceneProxy::GetLightRelevance(const FLightSceneInfo* LightSceneInfo, UBOOL& bDynamic, UBOOL& bRelevant, UBOOL& bLightMapped) const
{
	// Attach the light to the primitive's static meshes.
	bDynamic = TRUE;
	bRelevant = FALSE;
	bLightMapped = TRUE;

	if (LODs.Num() > 0)
	{
		for(INT LODIndex = 0; LODIndex < LODs.Num(); LODIndex++)
		{
			const FLODInfo* LCI = &LODs(LODIndex);
			if (LCI)
			{
				ELightInteractionType InteractionType = LCI->GetInteraction(LightSceneInfo).GetType();
				if(InteractionType != LIT_CachedIrrelevant)
				{
					bRelevant = TRUE;
				}
				if(InteractionType != LIT_CachedLightMap && InteractionType != LIT_CachedIrrelevant)
				{
					bLightMapped = FALSE;
				}
				if(InteractionType != LIT_Uncached)
				{
					bDynamic = FALSE;
				}
			}
		}
	}
	else
	{
		bRelevant = TRUE;
		bLightMapped = FALSE;
	}
}

/** Initialization constructor. */
FStaticMeshSceneProxy::FLODInfo::FLODInfo(const UStaticMeshComponent* InComponent,INT InLODIndex):
	OverrideColorVertexBuffer(0),
	Component(InComponent),
	LODIndex(InLODIndex),
	bUsesMeshModifyingMaterials(FALSE)
{
	UBOOL bHasStaticLighting = FALSE;
	if(LODIndex < Component->LODData.Num())
	{
		const FStaticMeshComponentLODInfo& ComponentLODInfo = Component->LODData(LODIndex);

		// Determine if the LOD has static lighting.
		bHasStaticLighting = ComponentLODInfo.LightMap != NULL || ComponentLODInfo.ShadowMaps.Num() || ComponentLODInfo.ShadowVertexBuffers.Num();

		// Initialize this LOD's overridden vertex colors, if it has any
		if( ComponentLODInfo.OverrideVertexColors )
		{
			FStaticMeshRenderData& LODRenderData = Component->StaticMesh->LODModels( LODIndex );
			
			// the instance should point to the loaded data to avoid copy and memory waste
			OverrideColorVertexBuffer = ComponentLODInfo.OverrideVertexColors;

			// Setup our vertex factory that points to our overridden color vertex stream.  We'll use this
			// vertex factory when rendering the static mesh instead of it's stock factory
			OverrideColorVertexFactory.Reset( new FLocalVertexFactory() );
			LODRenderData.SetupVertexFactory( *OverrideColorVertexFactory.GetOwnedPointer(), Component->StaticMesh, OverrideColorVertexBuffer );

			// @todo MeshPaint: Make sure this is the best place to do this; also make sure cleanup is called!
			BeginInitResource( OverrideColorVertexFactory.GetOwnedPointer() );
		}
	}

	// Gather the materials applied to the LOD.
	Elements.Empty(Component->StaticMesh->LODModels(LODIndex).Elements.Num());
	const FStaticMeshRenderData& LODModel = Component->StaticMesh->LODModels(LODIndex);
	for(INT ElementIndex = 0;ElementIndex < LODModel.Elements.Num();ElementIndex++)
	{
		const FStaticMeshElement& Element = LODModel.Elements(ElementIndex);
		FElementInfo ElementInfo;

		// Determine the material applied to this element of the LOD.
		ElementInfo.Material = Component->GetMaterial(Element.MaterialIndex,LODIndex);

		// If there isn't an applied material, or if we need static lighting and it doesn't support it, fall back to the default material.
		if(!ElementInfo.Material || (bHasStaticLighting && !ElementInfo.Material->CheckMaterialUsage(MATUSAGE_StaticLighting)))
		{
			ElementInfo.Material = GEngine->DefaultMaterial;
		}

#if WITH_D3D11_TESSELLATION
		const UBOOL bRequiresAdjacencyInformation = FTessellationMaterialPolicy::RequiresAdjacencyInformation( ElementInfo.Material, LODModel.VertexFactory.GetType() );
		if ( bRequiresAdjacencyInformation && LODModel.AdjacencyIndexBuffer.Indices.Num() == 0 )
		{
			warnf( TEXT("Adjacency information not built for static mesh with a material that requires it. Using default material instead.\n\tMaterial: %s\n\tStaticMesh: %s"),
				*ElementInfo.Material->GetPathName(), 
				*Component->StaticMesh->GetPathName() );
			ElementInfo.Material = GEngine->DefaultMaterial;
		}
#endif // #if WITH_D3D11_TESSELLATION

		// Store the element info.
		Elements.AddItem(ElementInfo);

		// Flag the entire LOD if any material modifies its mesh
		FMaterialResource* MaterialResource = ElementInfo.Material->GetMaterial()->GetMaterialResource();
		if(MaterialResource)
		{
			if(MaterialResource->MaterialModifiesMeshPosition())
			{
				bUsesMeshModifyingMaterials = TRUE;
			}
		}
	}

}



/** Destructor */
FStaticMeshSceneProxy::FLODInfo::~FLODInfo()
{
	if( OverrideColorVertexFactory.IsValid() )
	{
		OverrideColorVertexFactory->ReleaseResource();
		OverrideColorVertexFactory.Reset();
	}

	// delete for OverrideColorVertexBuffer is not required , FStaticMeshComponentLODInfo handle the release of the memory
}

// FLightCacheInterface.
FLightInteraction FStaticMeshSceneProxy::FLODInfo::GetInteraction(const FLightSceneInfo* LightSceneInfo) const
{
	// Check if the light has static lighting or shadowing.
	// This directly accesses the component's static lighting with the assumption that it won't be changed without synchronizing with the rendering thread.
	if(LightSceneInfo->bStaticShadowing)
	{
		if(LODIndex < Component->LODData.Num())
		{
			const FStaticMeshComponentLODInfo& LODInstanceData = Component->LODData(LODIndex);
			if(LODInstanceData.LightMap)
			{
				if(LODInstanceData.LightMap->ContainsLight(LightSceneInfo->LightmapGuid))
				{
					return FLightInteraction::LightMap();
				}
			}
			for(INT LightIndex = 0;LightIndex < LODInstanceData.ShadowVertexBuffers.Num();LightIndex++)
			{
				const UShadowMap1D* const ShadowVertexBuffer = LODInstanceData.ShadowVertexBuffers(LightIndex);
				if(ShadowVertexBuffer && ShadowVertexBuffer->GetLightGuid() == LightSceneInfo->LightGuid)
				{
#if MOBILE || ( !CONSOLE && !FINAL_RELEASE )
					// On mobile platforms, shadow map lights are baked into simple lightmaps
					if( GUsingMobileRHI || GEmulateMobileRendering )
					{
						return FLightInteraction::LightMap();
					}
#endif
					return FLightInteraction::ShadowMap1D(ShadowVertexBuffer);
				}
			}
			for(INT LightIndex = 0;LightIndex < LODInstanceData.ShadowMaps.Num();LightIndex++)
			{
				const UShadowMap2D* const ShadowMap = LODInstanceData.ShadowMaps(LightIndex);
				if(ShadowMap && ShadowMap->IsValid() && ShadowMap->GetLightGuid() == LightSceneInfo->LightGuid)
				{
#if MOBILE || ( !CONSOLE && !FINAL_RELEASE )
					// On mobile platforms, shadow map lights are baked into simple lightmaps
					if( GUsingMobileRHI || GEmulateMobileRendering )
					{
						return FLightInteraction::LightMap();
					}
#endif
					return FLightInteraction::ShadowMap2D(
						LODInstanceData.ShadowMaps(LightIndex)->GetTexture(),
						LODInstanceData.ShadowMaps(LightIndex)->GetCoordinateScale(),
						LODInstanceData.ShadowMaps(LightIndex)->GetCoordinateBias(),
						ShadowMap->IsShadowFactorTexture()
						);
				}
			}
		}
		
		if(Component->IrrelevantLights.ContainsItem(LightSceneInfo->LightGuid))
		{
			return FLightInteraction::Irrelevant();
		}
	}

	// Use dynamic lighting if the light doesn't have static lighting.
	return FLightInteraction::Uncached();
}

FStaticMeshSceneProxy::FDecalLightCache::FDecalLightCache(const FDecalInteraction& DecalInteraction, const FStaticMeshSceneProxy& Proxy)
	: DecalComponent(DecalInteraction.Decal)
{
	// Build the static light interaction map.
	for (INT LightIndex = 0; LightIndex < Proxy.StaticMeshComponent->IrrelevantLights.Num(); ++LightIndex)
	{
		StaticLightInteractionMap.Set( Proxy.StaticMeshComponent->IrrelevantLights(LightIndex), 
			FLightInteraction::Irrelevant() );
	}

	// Toss the sample remapping now that a lightmap has been created.
	DecalInteraction.RenderData->SampleRemapping.Empty();

	// If a custom vertex lightmap was specified with the decal, use it.
	// Otherwise, use the mesh's lightmap texture.
	if (DecalInteraction.RenderData->LightMap1D != NULL
		&& DecalInteraction.RenderData->LightMap1D->GetLightMap1D() != NULL
		// Only use the custom vertex lightmap if it contains samples!
		&& DecalInteraction.RenderData->LightMap1D->GetLightMap1D()->NumSamples() > 0)
	{
		// 1D vertex lightmap from decal
		LightMap = DecalInteraction.RenderData->LightMap1D;

		// 1D vertex shadowmap from decal
		for (INT ShadowMapIndex = 0; ShadowMapIndex < DecalInteraction.RenderData->ShadowMap1D.Num(); ++ShadowMapIndex)
		{
			const UShadowMap1D* ShadowMap = DecalInteraction.RenderData->ShadowMap1D(ShadowMapIndex);
			if (ShadowMap != NULL)
			{
				StaticLightInteractionMap.Set(ShadowMap->GetLightGuid(), 
					FLightInteraction::ShadowMap1D(ShadowMap));
			}
		}
	}
	else
	{
		// 1D,2D lightmap buffer,texture from the underlying mesh.
		LightMap = Proxy.LODs(0).GetLightMap();

		// 2D shadowmap texture from the underlying mesh.
		const TArray<UShadowMap2D*>* TextureShadowMaps = Proxy.LODs(0).GetTextureShadowMaps();
		if (TextureShadowMaps != NULL && 
			TextureShadowMaps->Num() > 0)
		{
			for (INT ShadowMapIndex = 0; ShadowMapIndex < TextureShadowMaps->Num(); ++ShadowMapIndex)
			{
				UShadowMap2D* ShadowMap = (*TextureShadowMaps)(ShadowMapIndex);
				if (ShadowMap != NULL && ShadowMap->IsValid())
				{
					StaticLightInteractionMap.Set(ShadowMap->GetLightGuid(), 
						FLightInteraction::ShadowMap2D(
						ShadowMap->GetTexture(),
						ShadowMap->GetCoordinateScale(),
						ShadowMap->GetCoordinateBias(),
						ShadowMap->IsShadowFactorTexture()
						));
				}
			}
		}
		else
		{
			// 1D shadowmap buffer from the underlying mesh.
			const TArray<UShadowMap1D*>* VertexShadowMaps = Proxy.LODs(0).GetVertexShadowMaps();
			if (VertexShadowMaps != NULL)
			{
				for (INT ShadowMapIndex = 0; ShadowMapIndex < VertexShadowMaps->Num(); ++ShadowMapIndex)
				{
					const UShadowMap1D* ShadowMap = (*VertexShadowMaps)(ShadowMapIndex);
					if (ShadowMap != NULL)
					{
						StaticLightInteractionMap.Set(ShadowMap->GetLightGuid(), 
							FLightInteraction::ShadowMap1D(ShadowMap));
					}
				}
			}
		}
	}

	if (LightMap != NULL)
	{
		for (INT LightIndex = 0; LightIndex < LightMap->LightGuids.Num(); ++LightIndex)
		{
			StaticLightInteractionMap.Set( LightMap->LightGuids(LightIndex), 
				FLightInteraction::LightMap() );
		}
	}
}

// FLightCacheInterface.
FLightInteraction FStaticMeshSceneProxy::FDecalLightCache::GetInteraction(const FLightSceneInfo* LightSceneInfo) const
{
	// Check for a static light interaction.
	const FLightInteraction* Interaction = StaticLightInteractionMap.Find(LightSceneInfo->LightmapGuid);
	if(!Interaction)
	{
		Interaction = StaticLightInteractionMap.Find(LightSceneInfo->LightGuid);
	}
	return Interaction ? *Interaction : FLightInteraction::Uncached();
}

/**
 * Returns the minimum distance that the given LOD should be displayed at
 *
 * @param CurrentLevel - the LOD to find the min distance for
 */
FLOAT FStaticMeshSceneProxy::GetMinLODDist(INT CurrentLevel) const 
{
	//Scale LODMaxRange by LODDistanceRatio and then split this range up by the number of LOD's
	FLOAT MinDist = CurrentLevel * LODMaxRange * StaticMesh->LODDistanceRatio / StaticMesh->LODModels.Num();
	return MinDist;
}

/**
 * Returns the maximum distance that the given LOD should be displayed at
 * If the given LOD is the lowest detail LOD, then its maxDist will be FLT_MAX
 *
 * @param CurrentLevel - the LOD to find the max distance for
 */
FLOAT FStaticMeshSceneProxy::GetMaxLODDist(INT CurrentLevel) const 
{
	//This level's MaxDist is the next level's MinDist
	FLOAT MaxDist = GetMinLODDist(CurrentLevel + 1);

	//If the lowest detail LOD was passed in, set MaxDist to FLT_MAX so that it doesn't get culled
	if (CurrentLevel + 1 == StaticMesh->LODModels.Num()) 
	{
		MaxDist = FLT_MAX;
	} 
	return MaxDist;
}

/**
 * Returns the LOD that the primitive will render at for this view. 
 *
 * @param Distance - distance from the current view to the component's bound origin
 */
INT FStaticMeshSceneProxy::GetLOD(const FSceneView* View) const 
{
	//If an LOD is being forced, use that one
	if (ForcedLodModel > 0)
	{
		return ::Clamp(ForcedLodModel, 1, StaticMesh->LODModels.Num()) - 1;
	}
	// Note: These distance calculations must match up with the main renderer!
#if CONSOLE
	const FVector4 ViewOriginForDistance = View->ViewOrigin;
#else
	const FVector4 ViewOriginForDistance = View->ViewOrigin.W > 0.0f ? View->ViewOrigin : View->OverrideLODViewOrigin;
#endif

	const FLOAT DistanceSquared = CalculateDistanceSquaredForLOD(PrimitiveSceneInfo->Bounds, ViewOriginForDistance);

	for(INT LODIndex = LODs.Num() - 1; LODIndex >= 0; LODIndex--)
	{
		// Use the same distances as FStaticMeshSceneProxy::DrawStaticElements 
		// To ensure that LODs change the same way when drawn in a static draw list or when rendered through DrawDynamicElements
		const FLOAT MinDist = GetMinLODDist(LODIndex);
		const FLOAT MaxDist = GetMaxLODDist(LODIndex);
		
		const FLOAT LODFactorDistanceSquared = DistanceSquared * Square(View->LODDistanceFactor);
		if (LODFactorDistanceSquared >= Square(MinDist) && LODFactorDistanceSquared < Square(MaxDist))
		{
			return LODIndex;
		}
	}
	return INDEX_NONE;
}

FPrimitiveSceneProxy* UStaticMeshComponent::CreateSceneProxy()
{
#if !CONSOLE
	FPrimitiveSceneProxy* Proxy = ::new FStaticMeshSceneProxy(this);
	if (GIsEditor && Proxy)
	{
		SetupLightmapResolutionViewInfo(*Proxy);
	}
	return Proxy;
#else
	//@todo: figure out why i need a ::new (gcc3-specific)
	return ::new FStaticMeshSceneProxy(this);
#endif
}

UBOOL UStaticMeshComponent::ShouldRecreateProxyOnUpdateTransform() const
{
	// If the primitive is movable during gameplay, it won't use static mesh elements, and the proxy doesn't need to be recreated on UpdateTransform.
	// If the primitive isn't movable during gameplay, it will use static mesh elements, and the proxy must be recreated on UpdateTransform.
	UBOOL	bMovable = FALSE;
	if( GetOwner() )
	{
		bMovable = !GetOwner()->IsStatic() && GetOwner()->bMovable;
	}

	return bMovable==FALSE;
}
