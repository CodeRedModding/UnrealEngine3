/*=============================================================================
UnStaticMeshLight.cpp: Static mesh lighting code.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnStaticMeshLight.h"

/**
 * Creates a static lighting vertex to represent the given static mesh vertex.
 * @param VertexBuffer - The static mesh's vertex buffer.
 * @param VertexIndex - The index of the static mesh vertex to access.
 * @param OutVertex - Upon return, contains a static lighting vertex representing the specified static mesh vertex.
 */
static void GetStaticLightingVertex(
	const FPositionVertexBuffer& PositionVertexBuffer,
	const FStaticMeshVertexBuffer& VertexBuffer,
	UINT VertexIndex,
	const FMatrix& LocalToWorld,
	const FMatrix& LocalToWorldInverseTranspose,
	FStaticLightingVertex& OutVertex
	)
{
	OutVertex.WorldPosition = LocalToWorld.TransformFVector(PositionVertexBuffer.VertexPosition(VertexIndex));
	OutVertex.WorldTangentX = LocalToWorld.TransformNormal(VertexBuffer.VertexTangentX(VertexIndex)).SafeNormal();
	OutVertex.WorldTangentY = LocalToWorld.TransformNormal(VertexBuffer.VertexTangentY(VertexIndex)).SafeNormal();
	OutVertex.WorldTangentZ = LocalToWorldInverseTranspose.TransformNormal(VertexBuffer.VertexTangentZ(VertexIndex)).SafeNormal();

	checkSlow(VertexBuffer.GetNumTexCoords() <= ARRAY_COUNT(OutVertex.TextureCoordinates));
	for(UINT LightmapTextureCoordinateIndex = 0;LightmapTextureCoordinateIndex < VertexBuffer.GetNumTexCoords();LightmapTextureCoordinateIndex++)
	{
		OutVertex.TextureCoordinates[LightmapTextureCoordinateIndex] = VertexBuffer.GetVertexUV(VertexIndex,LightmapTextureCoordinateIndex);
	}
}


/** Initialization constructor. */
FStaticMeshStaticLightingMesh::FStaticMeshStaticLightingMesh(const UStaticMeshComponent* InPrimitive,INT InLODIndex,const TArray<ULightComponent*>& InRelevantLights):
	FStaticLightingMesh(
		InPrimitive->StaticMesh->LODModels(InLODIndex).GetTriangleCount(),
		InPrimitive->StaticMesh->LODModels(InLODIndex).GetTriangleCount(),
		InPrimitive->StaticMesh->LODModels(InLODIndex).NumVertices,
		InPrimitive->StaticMesh->LODModels(InLODIndex).NumVertices,
		0,
		InPrimitive->CastShadow | InPrimitive->bCastHiddenShadow,
		InPrimitive->bSelfShadowOnly,
		FALSE,
		InRelevantLights,
		InPrimitive,
		InPrimitive->Bounds.GetBox(),
		InPrimitive->StaticMesh->GetLightingGuid()
		),
	LODIndex(InLODIndex),
	StaticMesh(InPrimitive->StaticMesh),
	Primitive(InPrimitive),
	bReverseWinding(InPrimitive->LocalToWorldDeterminant < 0.0f)
{
	// use the primitive's local to world
	SetLocalToWorld(InPrimitive->LocalToWorld);
}

/** 
 * Sets the local to world matrix for this mesh, will also update LocalToWorldInverseTranspose and determinant
 *
 * @param InLocalToWorld Local to world matrix to apply
 */
void FStaticMeshStaticLightingMesh::SetLocalToWorld(const FMatrix& InLocalToWorld)
{
	LocalToWorld = InLocalToWorld;
	LocalToWorldInverseTranspose = LocalToWorld.Inverse().Transpose();
	LocalToWorldDeterminant = LocalToWorld.Determinant();
}



// FStaticLightingMesh interface.

void FStaticMeshStaticLightingMesh::GetTriangle(INT TriangleIndex,FStaticLightingVertex& OutV0,FStaticLightingVertex& OutV1,FStaticLightingVertex& OutV2) const
{
	const FStaticMeshRenderData& LODRenderData = StaticMesh->LODModels(LODIndex);

	// Lookup the triangle's vertex indices.
	const WORD I0 = LODRenderData.IndexBuffer.Indices(TriangleIndex * 3 + 0);
	const WORD I1 = LODRenderData.IndexBuffer.Indices(TriangleIndex * 3 + (bReverseWinding ? 2 : 1));
	const WORD I2 = LODRenderData.IndexBuffer.Indices(TriangleIndex * 3 + (bReverseWinding ? 1 : 2));

	// Translate the triangle's static mesh vertices to static lighting vertices.
	GetStaticLightingVertex(LODRenderData.PositionVertexBuffer,LODRenderData.VertexBuffer,I0,LocalToWorld,LocalToWorldInverseTranspose,OutV0);
	GetStaticLightingVertex(LODRenderData.PositionVertexBuffer,LODRenderData.VertexBuffer,I1,LocalToWorld,LocalToWorldInverseTranspose,OutV1);
	GetStaticLightingVertex(LODRenderData.PositionVertexBuffer,LODRenderData.VertexBuffer,I2,LocalToWorld,LocalToWorldInverseTranspose,OutV2);
}

void FStaticMeshStaticLightingMesh::GetTriangleIndices(INT TriangleIndex,INT& OutI0,INT& OutI1,INT& OutI2) const
{
	const FStaticMeshRenderData& LODRenderData = StaticMesh->LODModels(LODIndex);

	// Lookup the triangle's vertex indices.
	OutI0 = LODRenderData.IndexBuffer.Indices(TriangleIndex * 3 + 0);
	OutI1 = LODRenderData.IndexBuffer.Indices(TriangleIndex * 3 + (bReverseWinding ? 2 : 1));
	OutI2 = LODRenderData.IndexBuffer.Indices(TriangleIndex * 3 + (bReverseWinding ? 1 : 2));
}

UBOOL FStaticMeshStaticLightingMesh::ShouldCastShadow(ULightComponent* Light,const FStaticLightingMapping* Receiver) const
{
	// If the receiver is the same primitive but a different LOD, don't cast shadows on it.
	if(OtherLODs.ContainsItem(Receiver->Mesh))
	{
		return FALSE;
	}
	else
	{
		return FStaticLightingMesh::ShouldCastShadow(Light,Receiver);
	}
}

/** @return		TRUE if the specified triangle casts a shadow. */
UBOOL FStaticMeshStaticLightingMesh::IsTriangleCastingShadow(UINT TriangleIndex) const
{
	// Find the mesh element containing the specified triangle.
	const FStaticMeshRenderData& LODRenderData = StaticMesh->LODModels(LODIndex);
	for ( INT ElementIndex = 0 ; ElementIndex < LODRenderData.Elements.Num() ; ++ElementIndex )
	{
		const FStaticMeshElement& Element = LODRenderData.Elements( ElementIndex );
		if ( ( TriangleIndex >= Element.FirstIndex / 3 ) && ( TriangleIndex < Element.FirstIndex / 3 + Element.NumTriangles ) )
		{
			return Element.bEnableShadowCasting;
		}
	}

	return TRUE;
}

/** @return		TRUE if the mesh wants to control shadow casting per element rather than per mesh. */
UBOOL FStaticMeshStaticLightingMesh::IsControllingShadowPerElement() const
{
	const FStaticMeshRenderData& LODRenderData = StaticMesh->LODModels(LODIndex);
	for ( INT ElementIndex = 0 ; ElementIndex < LODRenderData.Elements.Num() ; ++ElementIndex )
	{
		if ( !LODRenderData.Elements( ElementIndex ).bEnableShadowCasting )
		{
			return TRUE;
		}
	}
	return FALSE;
}

UBOOL FStaticMeshStaticLightingMesh::IsUniformShadowCaster() const
{
	// If this mesh is one of multiple LODs, it won't uniformly shadow all of them.
	return OtherLODs.Num() == 0 && FStaticLightingMesh::IsUniformShadowCaster();
}

FLightRayIntersection FStaticMeshStaticLightingMesh::IntersectLightRay(const FVector& Start,const FVector& End,UBOOL bFindNearestIntersection) const
{
	// Create the object that knows how to extract information from the component/mesh
	FStaticMeshCollisionDataProvider Provider(Primitive, LocalToWorld, LocalToWorldDeterminant);

	// Create the check structure with all the local space fun
	FCheckResult Result(1.0f);
	TkDOPLineCollisionCheck<FStaticMeshCollisionDataProvider,WORD,UStaticMesh::kDOPTreeType> kDOPCheck(Start,End,!bFindNearestIntersection ? TRACE_StopAtAnyHit : 0,Provider,&Result);

	// Do the line check
	const UBOOL bIntersects = StaticMesh->kDOPTree.LineCheck(kDOPCheck);

	// Setup a vertex to represent the intersection.
	FStaticLightingVertex IntersectionVertex;
	if(bIntersects)
	{
		IntersectionVertex.WorldPosition = Start + (End - Start) * Result.Time;
		IntersectionVertex.WorldTangentZ = kDOPCheck.GetHitNormal();
	}
	else
	{
		IntersectionVertex.WorldPosition.Set(0,0,0);
		IntersectionVertex.WorldTangentZ.Set(0,0,1);
	}
	return FLightRayIntersection(bIntersects,IntersectionVertex);
}

/** Initialization constructor. */
FStaticMeshStaticLightingTextureMapping::FStaticMeshStaticLightingTextureMapping(
	UStaticMeshComponent* InPrimitive,
	INT InLODIndex,
	FStaticLightingMesh* InMesh,
	INT InSizeX,
	INT InSizeY,
	INT InLightmapTextureCoordinateIndex,
	UBOOL bPerformFullQualityRebuild
	):
	FStaticLightingTextureMapping(
		InMesh,
		InPrimitive,
		InSizeX,
		InSizeY,
		InLightmapTextureCoordinateIndex,
		InPrimitive->bForceDirectLightMap
		),
	Primitive(InPrimitive),
	LODIndex(InLODIndex)
{}

// FStaticLightingTextureMapping interface
void FStaticMeshStaticLightingTextureMapping::Apply(FLightMapData2D* LightMapData,const TMap<ULightComponent*,FShadowMapData2D*>& ShadowMapData, FQuantizedLightmapData* QuantizedData)
{
	// Determine the material to use for grouping the light-maps and shadow-maps.
	UMaterialInterface* const Material = Primitive->GetNumElements() == 1 ? Primitive->GetMaterial(0) : NULL;

	// Ensure LODData has enough entries in it, free not required.
	Primitive->SetLODDataCount(LODIndex + 1, Primitive->StaticMesh->LODModels.Num());

	if (LODIndex == 0 && QuantizedData)
	{
		Primitive->PreviewEnvironmentShadowing = QuantizedData->PreviewEnvironmentShadowing;
	}
	FStaticMeshComponentLODInfo& ComponentLODInfo = Primitive->LODData(LODIndex);

	ELightMapPaddingType PaddingType = GAllowLightmapPadding ? LMPT_NormalPadding : LMPT_NoPadding;

	const UBOOL bHasNonZeroData = LightMapData != NULL || QuantizedData != NULL && QuantizedData->HasNonZeroData();
	if (bHasNonZeroData)
	{
		// Create a light-map for the primitive.
		ComponentLODInfo.LightMap = FLightMap2D::AllocateLightMap(
			Primitive,
			LightMapData,
			QuantizedData,
			Material,
			Primitive->Bounds,
			PaddingType,
			LMF_Streamed
			);
	}
	else
	{
		ComponentLODInfo.LightMap = NULL;
	}

	// Create the shadow-maps for the primitive.
	ComponentLODInfo.ShadowVertexBuffers.Empty();
	ComponentLODInfo.ShadowMaps.Empty(ShadowMapData.Num());
	for(TMap<ULightComponent*,FShadowMapData2D*>::TConstIterator ShadowMapDataIt(ShadowMapData);ShadowMapDataIt;++ShadowMapDataIt)
	{
		ELightMapPaddingType PaddingType = GAllowLightmapPadding ? LMPT_NormalPadding : LMPT_NoPadding;
		ComponentLODInfo.ShadowMaps.AddItem(
			new(Owner) UShadowMap2D(
			Primitive,
			*ShadowMapDataIt.Value(),
			ShadowMapDataIt.Key()->LightGuid,
			Material,
			Primitive->Bounds,
			PaddingType,
			SMF_Streamed
			)
			);
		delete ShadowMapDataIt.Value();
	}

	// Build the list of statically irrelevant lights.
	// TODO: This should be stored per LOD.
	Primitive->IrrelevantLights.Empty();
	for(INT LightIndex = 0;LightIndex < Mesh->RelevantLights.Num();LightIndex++)
	{
		const ULightComponent* Light = Mesh->RelevantLights(LightIndex);

		// Check if the light is stored in the light-map.
		const UBOOL bIsInLightMap = ComponentLODInfo.LightMap && ComponentLODInfo.LightMap->LightGuids.ContainsItem(Light->LightmapGuid);

		// Check if the light is stored in the shadow-map.
		UBOOL bIsInShadowMap = FALSE;
		for(INT ShadowMapIndex = 0;ShadowMapIndex < ComponentLODInfo.ShadowMaps.Num();ShadowMapIndex++)
		{
			if(ComponentLODInfo.ShadowMaps(ShadowMapIndex)->GetLightGuid() == Light->LightGuid)
			{
				bIsInShadowMap = TRUE;
				break;
			}
		}

		// Add the light to the statically irrelevant light list if it is in the potentially relevant light list, but didn't contribute to the light-map or a shadow-map.
		if(!bIsInLightMap && !bIsInShadowMap)
		{	
			Primitive->IrrelevantLights.AddUniqueItem(Light->LightGuid);
		}
	}

	// Mark the primitive's package as dirty.
	Primitive->MarkPackageDirty();
}

/** Initialization constructor. */
FStaticMeshStaticLightingVertexMapping::FStaticMeshStaticLightingVertexMapping(
	UStaticMeshComponent* InPrimitive,
	INT InLODIndex,
	FStaticLightingMesh* InMesh,
	UBOOL bPerformFullQualityBuild
	):
		FStaticLightingVertexMapping(
			InMesh,
			InPrimitive,
			InPrimitive->bForceDirectLightMap,
			1.0f / Square((FLOAT)InPrimitive->SubDivisionStepSize),
			!(bPerformFullQualityBuild && InPrimitive->bUseSubDivisions)
		),
		Primitive(InPrimitive),
		LODIndex(InLODIndex)
	{
	}

// FStaticLightingTextureMapping interface
void FStaticMeshStaticLightingVertexMapping::Apply(FLightMapData1D* LightMapData,const TMap<ULightComponent*,FShadowMapData1D*>& ShadowMapData, FQuantizedLightmapData* QuantizedData)
{
	if (QuantizedData)
	{
		Primitive->PreviewEnvironmentShadowing = QuantizedData->PreviewEnvironmentShadowing;
	}
	// Ensure LODData has enough entries in it, free not required.
	Primitive->SetLODDataCount(LODIndex + 1, Primitive->StaticMesh->LODModels.Num());

	FStaticMeshComponentLODInfo& ComponentLODInfo = Primitive->LODData(LODIndex);

	const UBOOL bHasNonZeroData = LightMapData != NULL || QuantizedData != NULL && QuantizedData->HasNonZeroData();
	if (bHasNonZeroData)
	{
		ComponentLODInfo.LightMap = new FLightMap1D(Primitive, LightMapData, QuantizedData);
	}
	else
	{
		ComponentLODInfo.LightMap = NULL;
	}

	// Create the shadow-maps for the primitive.
	ComponentLODInfo.ShadowVertexBuffers.Empty(ShadowMapData.Num());
	ComponentLODInfo.ShadowMaps.Empty();
	for(TMap<ULightComponent*,FShadowMapData1D*>::TConstIterator ShadowMapDataIt(ShadowMapData);ShadowMapDataIt;++ShadowMapDataIt)
	{
		ComponentLODInfo.ShadowVertexBuffers.AddItem(new(Owner) UShadowMap1D(ShadowMapDataIt.Key()->LightGuid,*ShadowMapDataIt.Value()));
		delete ShadowMapDataIt.Value();
	}

	// Build the list of statically irrelevant lights.
	// TODO: This should be stored per LOD.
	Primitive->IrrelevantLights.Empty();
	for(INT LightIndex = 0;LightIndex < Mesh->RelevantLights.Num();LightIndex++)
	{
		const ULightComponent* Light = Mesh->RelevantLights(LightIndex);

		// Check if the light is stored in the light-map.
		const UBOOL bIsInLightMap = ComponentLODInfo.LightMap && ComponentLODInfo.LightMap->LightGuids.ContainsItem(Light->LightmapGuid);

		// Check if the light is stored in the shadow-map.
		UBOOL bIsInShadowMap = FALSE;
		for(INT ShadowMapIndex = 0;ShadowMapIndex < ComponentLODInfo.ShadowVertexBuffers.Num();ShadowMapIndex++)
		{
			if(ComponentLODInfo.ShadowVertexBuffers(ShadowMapIndex)->GetLightGuid() == Light->LightGuid)
			{
				bIsInShadowMap = TRUE;
				break;
			}
		}

		// Add the light to the statically irrelevant light list if it is in the potentially relevant light list, but didn't contribute to the light-map or a shadow-map.
		if(!bIsInLightMap && !bIsInShadowMap)
		{	
			Primitive->IrrelevantLights.AddUniqueItem(Light->LightGuid);
		}
	}

	// Mark the primitive's package as dirty.
	Primitive->MarkPackageDirty();
}

void UStaticMeshComponent::GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options)
{
	if( StaticMesh && HasStaticShadowing() && bAcceptsLights )
	{
		INT		BaseLightMapWidth	= 0;
		INT		BaseLightMapHeight	= 0;
		GetLightMapResolution( BaseLightMapWidth, BaseLightMapHeight );

		// Process each LOD separately.
		TArray<FStaticMeshStaticLightingMesh*> StaticLightingMeshes;
		for(INT LODIndex = 0;LODIndex < StaticMesh->LODModels.Num();LODIndex++)
		{
			const FStaticMeshRenderData& LODRenderData = StaticMesh->LODModels(LODIndex);
			// Figure out whether we are storing the lighting/ shadowing information in a texture or vertex buffer.
			UBOOL bUseTextureMap;
			if( (BaseLightMapWidth > 0) && (BaseLightMapHeight > 0) 
				&& StaticMesh->LightMapCoordinateIndex >= 0 
				&& (UINT)StaticMesh->LightMapCoordinateIndex < LODRenderData.VertexBuffer.GetNumTexCoords())
			{
				bUseTextureMap = TRUE;
			}
			else
			{
				bUseTextureMap = FALSE;
			}

			// Create a static lighting mesh for the LOD.
			FStaticMeshStaticLightingMesh* StaticLightingMesh = AllocateStaticLightingMesh(LODIndex,InRelevantLights);
			OutPrimitiveInfo.Meshes.AddItem(StaticLightingMesh);
			StaticLightingMeshes.AddItem(StaticLightingMesh);

			if(bUseTextureMap)
			{
				// Shrink LOD texture lightmaps by half for each LOD level
				const INT LightMapWidth = LODIndex > 0 ? Max(BaseLightMapWidth / (2 << (LODIndex - 1)), 32) : BaseLightMapWidth;
				const INT LightMapHeight = LODIndex > 0 ? Max(BaseLightMapHeight / (2 << (LODIndex - 1)), 32) : BaseLightMapHeight;
				// Create a static lighting texture mapping for the LOD.
				OutPrimitiveInfo.Mappings.AddItem(new FStaticMeshStaticLightingTextureMapping(
					this,LODIndex,StaticLightingMesh,LightMapWidth,LightMapHeight,StaticMesh->LightMapCoordinateIndex,TRUE));
			}
			else
			{
				// Create a static lighting vertex mapping for the LOD.
				OutPrimitiveInfo.Mappings.AddItem(new FStaticMeshStaticLightingVertexMapping(
					this,LODIndex,StaticLightingMesh,TRUE));
			}
		}

		// Give each LOD's static lighting mesh a list of the other LODs of this primitive, so they can disallow shadow casting between LODs.
		for(INT MeshIndex = 0;MeshIndex < StaticLightingMeshes.Num();MeshIndex++)
		{
			for(INT OtherMeshIndex = 0;OtherMeshIndex < StaticLightingMeshes.Num();OtherMeshIndex++)
			{
				if(MeshIndex != OtherMeshIndex)
				{
					StaticLightingMeshes(MeshIndex)->OtherLODs.AddItem(StaticLightingMeshes(OtherMeshIndex));
				}
			}
		}
	}
}

/**
 *	Requests whether the component will use texture, vertex or no lightmaps.
 *
 *	@return	ELightMapInteractionType		The type of lightmap interaction the component will use.
 */
ELightMapInteractionType UStaticMeshComponent::GetStaticLightingType() const
{
	UBOOL bUseTextureMap = FALSE;
	if( StaticMesh && HasStaticShadowing() )
	{
		// Process each LOD separately.
		TArray<FStaticMeshStaticLightingMesh*> StaticLightingMeshes;
		for(INT LODIndex = 0;LODIndex < StaticMesh->LODModels.Num();LODIndex++)
		{
			const FStaticMeshRenderData& LODRenderData = StaticMesh->LODModels(LODIndex);

			// Figure out whether we are storing the lighting/ shadowing information in a texture or vertex buffer.
			INT		LightMapWidth	= 0;
			INT		LightMapHeight	= 0;
			GetLightMapResolution( LightMapWidth, LightMapHeight );

			if ((LightMapWidth > 0) && (LightMapHeight > 0) &&	
				(StaticMesh->LightMapCoordinateIndex >= 0) &&
				((UINT)StaticMesh->LightMapCoordinateIndex < LODRenderData.VertexBuffer.GetNumTexCoords())
				)
			{
				bUseTextureMap = TRUE;
				break;
			}
		}
	}

	return (bUseTextureMap == TRUE) ? LMIT_Texture : LMIT_Vertex;
}

/** Gets the emissive boost for the primitive component. */
FLOAT UStaticMeshComponent::GetEmissiveBoost(INT ElementIndex) const
{
	return LightmassSettings.EmissiveBoost;
}

/** Gets the diffuse boost for the primitive component. */
FLOAT UStaticMeshComponent::GetDiffuseBoost(INT ElementIndex) const
{
	return LightmassSettings.DiffuseBoost;
}

/** Gets the specular boost for the primitive component. */
FLOAT UStaticMeshComponent::GetSpecularBoost(INT ElementIndex) const
{
	return LightmassSettings.SpecularBoost;
}

/** Allocates an implementation of FStaticLightingMesh that will handle static lighting for this component */
FStaticMeshStaticLightingMesh* UStaticMeshComponent::AllocateStaticLightingMesh(INT LODIndex, const TArray<ULightComponent*>& InRelevantLights)
{
	return new FStaticMeshStaticLightingMesh(this,LODIndex,InRelevantLights);
}

void UStaticMeshComponent::InvalidateLightingCache()
{
	UBOOL bHasStaticLightingData = IrrelevantLights.Num() > 0;
	for(INT i = 0; i < LODData.Num() && !bHasStaticLightingData; i++)
	{
		bHasStaticLightingData = bHasStaticLightingData || LODData(i).ShadowMaps.Num() > 0;
		bHasStaticLightingData = bHasStaticLightingData || LODData(i).ShadowVertexBuffers.Num() > 0;
		bHasStaticLightingData = bHasStaticLightingData || LODData(i).LightMap != NULL;
	}
	if(bHasStaticLightingData)
	{
		// Save the static mesh state for transactions, force it to be marked dirty if we are going to discard any static lighting data.
		Modify(bHasStaticLightingData);

		// Mark lighting as requiring a rebuilt.
		MarkLightingRequiringRebuild();

		// Detach the component from the scene for the duration of this function.
		FComponentReattachContext ReattachContext(this);

		Super::InvalidateLightingCache();

		// Discard all cached lighting.
		IrrelevantLights.Empty();
		for(INT i = 0; i < LODData.Num(); i++)
		{
			FStaticMeshComponentLODInfo& LODDataElement = LODData(i);
			LODDataElement.ShadowMaps.Empty();
			LODDataElement.ShadowVertexBuffers.Empty();
			LODDataElement.LightMap = NULL;
		}
	}
}

/**
 *	Switches the static mesh component to use either Texture or Vertex static lighting.
 *
 *	@param	bTextureMapping		If TRUE, set the component to use texture light mapping.
 *								If FALSE, set it to use vertex light mapping.
 *	@param	ResolutionToUse		If != 0, set the resolution to the given value. 
 *
 *	@return	UBOOL				TRUE if successfully set; FALSE if not
 */
UBOOL UStaticMeshComponent::SetStaticLightingMapping(UBOOL bTextureMapping, INT ResolutionToUse)
{
	UBOOL bSuccessful = FALSE;
	if (StaticMesh)
	{
		if (bTextureMapping == TRUE)
		{
			// Set it to texture mapping!
			if (ResolutionToUse == 0)
			{
				if (bOverrideLightMapRes == TRUE)
				{
					// If overriding the static mesh setting, check to set if set to 0
					// which will force the component to use vertex mapping
					if (OverriddenLightMapRes == 0)
					{
						// See if the static mesh has a valid setting
						if (StaticMesh->LightMapResolution != 0)
						{
							// Simply uncheck the override...
							bOverrideLightMapRes = FALSE;
							bSuccessful = TRUE;
						}
						else
						{
							// Set it to the default value from the ini
							INT TempInt = 0;
							verify(GConfig->GetInt(TEXT("DevOptions.StaticLighting"), TEXT("DefaultStaticMeshLightingRes"), TempInt, GLightmassIni));
							OverriddenLightMapRes = TempInt;
							bSuccessful = TRUE;
						}
					}
					else
					{
						// We should be texture mapped already...
					}
				}
				else
				{
					// See if the static mesh has a valid setting
					if (StaticMesh->LightMapResolution == 0)
					{
						// See if the static mesh has a valid setting
						if (OverriddenLightMapRes != 0)
						{
							// Simply check the override...
							bOverrideLightMapRes = TRUE;
							bSuccessful = TRUE;
						}
						else
						{
							// Set it to the default value from the ini
							INT TempInt = 0;
							verify(GConfig->GetInt(TEXT("DevOptions.StaticLighting"), TEXT("DefaultStaticMeshLightingRes"), TempInt, GLightmassIni));
							OverriddenLightMapRes = TempInt;
							bOverrideLightMapRes = TRUE;
							bSuccessful = TRUE;
						}
					}
					else
					{
						// We should be texture mapped already...
					}
				}
			}
			else
			{
				// Use the override - even if it was already set to override at a different value
				OverriddenLightMapRes = ResolutionToUse;
				bOverrideLightMapRes = TRUE;
				bSuccessful = TRUE;
			}
		}
		else
		{
			// Set it to vertex mapping...
			if (bOverrideLightMapRes == TRUE)
			{
				if (OverriddenLightMapRes != 0)
				{
					// See if the static mesh has a valid setting
					if (StaticMesh->LightMapResolution == 0)
					{
						// Simply uncheck the override...
						bOverrideLightMapRes = FALSE;
						bSuccessful = TRUE;
					}
					else
					{
						// Set it to 0 to force vertex mapping
						OverriddenLightMapRes = 0;
						bSuccessful = TRUE;
					}
				}
				else
				{
					// We should be vertex mapped already...
				}
			}
			else
			{
				// See if the static mesh has a valid setting
				if (StaticMesh->LightMapResolution != 0)
				{
					// Set it to the default value from the ini
					OverriddenLightMapRes = 0;
					bOverrideLightMapRes = TRUE;
					bSuccessful = TRUE;
				}
				else
				{
					// We should be vertex mapped already...
				}
			}
		}
	}

	if (bSuccessful == TRUE)
	{
		MarkPackageDirty();
	}

	return bSuccessful;
}