/*=============================================================================
	SpeedTreeStaticLighting.cpp: SpeedTreeComponent static lighting implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#if WITH_SPEEDTREE
#include "SpeedTreeStaticLighting.h"

void FStaticLightingSpeedTreeRayTracer::Init()
{
	// Build a kDOP tree for the tree's triangles.
	TArray<FkDOPBuildCollisionTriangle<WORD> > kDopTriangles;
	for(UINT TriangleIndex = 0;TriangleIndex < MeshElement.NumPrimitives;TriangleIndex++)
	{
		// Read the triangle from the mesh.
		const INT I0 = Indices(MeshElement.FirstIndex + TriangleIndex * 3 + 0);
		const INT I1 = Indices(MeshElement.FirstIndex + TriangleIndex * 3 + 1);
		const INT I2 = Indices(MeshElement.FirstIndex + TriangleIndex * 3 + 2);
		const FStaticLightingVertex& V0 = Vertices(I0);
		const FStaticLightingVertex& V1 = Vertices(I1);
		const FStaticLightingVertex& V2 = Vertices(I2);

		// Compute the triangle's normal.
		const FVector TriangleNormal = (V2.WorldPosition - V0.WorldPosition) ^ (V1.WorldPosition - V0.WorldPosition);

		// Compute the triangle area.
		const FLOAT TriangleArea = TriangleNormal.Size() * 0.5f;

		// Add the triangle to the list of kDOP triangles.
		new(kDopTriangles) FkDOPBuildCollisionTriangle<WORD>(I0,I1,I2,0,V0.WorldPosition,V1.WorldPosition,V2.WorldPosition);
	}
	kDopTree.Build(kDopTriangles);
}

FLightRayIntersection FStaticLightingSpeedTreeRayTracer::IntersectLightRay(const FVector& Start,const FVector& End) const
{
	// Check the line against the triangles.
	FVector CurrentStart = Start;
	while(TRUE)
	{
		FCheckResult CheckResult(1.0f);
		TkDOPLineCollisionCheck<const FStaticLightingSpeedTreeRayTracer,WORD> kDOPCheck(
			CurrentStart,
			End,
			0,
			*this,
			&CheckResult
			);
		if(kDopTree.LineCheck(kDOPCheck))
		{
			FLOAT Fraction = RandomStream.GetFraction();
			//FLOAT Fraction = 0.0f;	// fp:precise mode!
			// Treat the opacity as a probability that the surface will block any particular ray.
			if(Fraction < Opacity)
			{
				// Setup a vertex to represent the intersection.
				FStaticLightingVertex IntersectionVertex;
				IntersectionVertex.WorldPosition = CurrentStart + (End - CurrentStart) * CheckResult.Time;
				IntersectionVertex.WorldTangentZ = kDOPCheck.GetHitNormal();
				return FLightRayIntersection(TRUE,IntersectionVertex);
			}
			else
			{
				// If the ray fails the opacity probability check on this triangle, start the ray again behind the surface.
				const FVector Direction = (End - CurrentStart);
				const FLOAT NextSegmentStartTime = (CheckResult.Time + 16.0f / Direction.Size());
				if(NextSegmentStartTime < 1.0f)
				{
					CurrentStart = CurrentStart + Direction * NextSegmentStartTime;
				}
				else
				{
					return FLightRayIntersection::None();
				}
			}
		}
		else
		{
			// If the ray didn't intersect any triangles, return no intersection.
			return FLightRayIntersection::None();
		}
	}
}

FSpeedTreeStaticLightingMesh::FSpeedTreeStaticLightingMesh(
	FSpeedTreeComponentStaticLighting* InComponentStaticLighting,
	INT InLODIndex,
	const FMeshBatchElement& InMeshElement,
	ESpeedTreeMeshType InMeshType,
	FLOAT InShadowOpacity,
	UBOOL bInTwoSidedMaterial,
	const TArray<ULightComponent*>& InRelevantLights,
	const FLightingBuildOptions& Options
	):
	FStaticLightingMesh(
		InMeshElement.NumPrimitives,
		InMeshElement.NumPrimitives,
		InMeshElement.MaxVertexIndex - InMeshElement.MinVertexIndex + 1,
		InMeshElement.MaxVertexIndex - InMeshElement.MinVertexIndex + 1,
		0,
		InShadowOpacity > 0.0f,
		InComponentStaticLighting->GetComponent()->bSelfShadowOnly,
		bInTwoSidedMaterial,
		InRelevantLights,
		InComponentStaticLighting->GetComponent(),
		InComponentStaticLighting->GetComponent()->Bounds.GetBox(),
		InComponentStaticLighting->GetComponent()->SpeedTree->GetLightingGuid()
		),
	ComponentStaticLighting(InComponentStaticLighting),
	LODIndex(InLODIndex),
	MeshElement(InMeshElement),
	MeshType(InMeshType),
	RayTracer(InComponentStaticLighting->GetVertices(InMeshType),InComponentStaticLighting->GetIndices(),InMeshElement,InShadowOpacity)
{
	// If the mesh casts shadows, build the kDOP tree.
	if(InShadowOpacity > 0.0f)
	{
		RayTracer.Init();
	}
}

void FSpeedTreeStaticLightingMesh::GetTriangle(INT TriangleIndex,FStaticLightingVertex& OutV0,FStaticLightingVertex& OutV1,FStaticLightingVertex& OutV2) const
{
	// Read the triangle's vertex indices.
	const INT I0 = ComponentStaticLighting->GetIndices()(MeshElement.FirstIndex + TriangleIndex * 3 + 0);
	const INT I1 = ComponentStaticLighting->GetIndices()(MeshElement.FirstIndex + TriangleIndex * 3 + 1);
	const INT I2 = ComponentStaticLighting->GetIndices()(MeshElement.FirstIndex + TriangleIndex * 3 + 2);

	// Read the triangle's vertices.
	OutV0 = ComponentStaticLighting->GetVertices(MeshType)(I0);
	OutV1 = ComponentStaticLighting->GetVertices(MeshType)(I1);
	OutV2 = ComponentStaticLighting->GetVertices(MeshType)(I2);
}

void FSpeedTreeStaticLightingMesh::GetTriangleIndices(INT TriangleIndex,INT& OutI0,INT& OutI1,INT& OutI2) const
{
	// Read the triangle's vertex indices.
	OutI0 = ComponentStaticLighting->GetIndices()(MeshElement.FirstIndex + TriangleIndex * 3 + 0) - MeshElement.MinVertexIndex;
	OutI1 = ComponentStaticLighting->GetIndices()(MeshElement.FirstIndex + TriangleIndex * 3 + 1) - MeshElement.MinVertexIndex;
	OutI2 = ComponentStaticLighting->GetIndices()(MeshElement.FirstIndex + TriangleIndex * 3 + 2) - MeshElement.MinVertexIndex;
}

UBOOL FSpeedTreeStaticLightingMesh::ShouldCastShadow(ULightComponent* Light,const FStaticLightingMapping* Receiver) const
{
	// Check if the receiver is in the same SpeedTreeComponent and LOD as this mesh.
	UBOOL bReceiverIsSameLOD = FALSE;
	UBOOL bReceiverIsSameComponent = FALSE;
	if(ComponentStaticLighting->IsMappingFromThisComponent(Receiver))
	{
		FSpeedTreeStaticLightingMapping* ReceiverSpeedTreeMapping = (FSpeedTreeStaticLightingMapping*)Receiver;
		bReceiverIsSameComponent = TRUE;
		bReceiverIsSameLOD = (ReceiverSpeedTreeMapping->GetLODIndex() == LODIndex);
	}

	const UBOOL bThisIsHighestLOD = (LODIndex == 0);
	if(bThisIsHighestLOD)
	{
		// The highest LOD doesn't shadow the other LODs.
		if(bReceiverIsSameComponent && !bReceiverIsSameLOD)
		{
			return FALSE;
		}
	}
	else
	{
		// The lower LODs only shadow themselves.
		if(!(bReceiverIsSameComponent && bReceiverIsSameLOD))
		{
			return FALSE;
		}
	}

	return FStaticLightingMesh::ShouldCastShadow(Light,Receiver);
}

UBOOL FSpeedTreeStaticLightingMesh::IsUniformShadowCaster() const
{
	return FALSE;
}

FLightRayIntersection FSpeedTreeStaticLightingMesh::IntersectLightRay(const FVector& Start,const FVector& End,UBOOL bFindNearestIntersection) const
{
	return RayTracer.IntersectLightRay(Start,End);
}

FSpeedTreeStaticLightingMapping::FSpeedTreeStaticLightingMapping(FStaticLightingMesh* InMesh,FSpeedTreeComponentStaticLighting* InComponentStaticLighting,INT InLODIndex,const FMeshBatchElement& InMeshElement,ESpeedTreeMeshType InMeshType):
	FStaticLightingVertexMapping(
		InMesh,
		InComponentStaticLighting->GetComponent(),
		InComponentStaticLighting->GetComponent()->bForceDirectLightMap,
		1.0f / Square(16.0f),
		FALSE
	),
	ComponentStaticLighting(InComponentStaticLighting),
	LODIndex(InLODIndex),
	MeshElement(InMeshElement),
	MeshType(InMeshType)
{}

void FSpeedTreeStaticLightingMapping::Apply(FLightMapData1D* LightMapData,const TMap<ULightComponent*,FShadowMapData1D*>& ShadowMapData, FQuantizedLightmapData* QuantizedData)
{
	// Pass the static lighting data on to the component's static lighting manager.
	ComponentStaticLighting->ApplyCompletedMapping(this,LightMapData,ShadowMapData, QuantizedData);
}

/**
 * Computes the local-space vertex position for a leaf card vertex.
 * @param PivotPoint - The leaf card vertex's pivot point.
 * @param VertexData - The leaf card vertex's data.
 * @return the local-space vertex position.
 */
static FVector GetLeafCardVertexPosition(const FVector& PivotPoint,const FSpeedTreeVertexDataLeafCard& VertexData)
{
	return PivotPoint + VertexData.CornerOffset;
}

FSpeedTreeComponentStaticLighting::FSpeedTreeComponentStaticLighting(USpeedTreeComponent* InComponent,const TArray<ULightComponent*>& InRelevantLights):
	Component(InComponent),
	RelevantLights(InRelevantLights),
	Indices(InComponent->SpeedTree->SRH->IndexBuffer.Indices),
	BranchLightMapData(NULL),
	FrondLightMapData(NULL),
	LeafMeshLightMapData(NULL),
	LeafCardLightMapData(NULL),
	BillboardLightMapData(NULL),
	bHasBranchStaticLighting(FALSE),
	bHasFrondStaticLighting(FALSE),
	bHasLeafMeshStaticLighting(FALSE),
	bHasLeafCardStaticLighting(FALSE),
	bHasBillboardStaticLighting(FALSE),
	ComponentGuid(appCreateGuid()),
	NumCompleteMappings(0)
{
	// Compute the combined local to world transform, including rotation.
	const FMatrix LocalToWorld = Component->RotationOnlyMatrix.Inverse() * Component->LocalToWorld;

	// Compute the inverse transpose of the local to world transform for transforming normals.
	const FMatrix LocalToWorldInverseTranspose = LocalToWorld.Inverse().Transpose();

	// Set up the lighting vertices.
	for (INT MeshType = STMT_MinMinusOne + 1; MeshType < STMT_Max; MeshType++)
	{
		if (MeshType == STMT_Branches2)
			continue;

		// Find the vertex arrays for this current mesh type.
		const TArray<FSpeedTreeVertexPosition>& SourceVertexPositionArray =
			ChooseByMeshType< TArray<FSpeedTreeVertexPosition> >(
				MeshType,
				Component->SpeedTree->SRH->BranchPositionBuffer.Vertices,
				Component->SpeedTree->SRH->BranchPositionBuffer.Vertices,
				Component->SpeedTree->SRH->FrondPositionBuffer.Vertices,
				Component->SpeedTree->SRH->LeafCardPositionBuffer.Vertices,
				Component->SpeedTree->SRH->LeafMeshPositionBuffer.Vertices,
				Component->SpeedTree->SRH->BillboardPositionBuffer.Vertices
				);
		const INT NumVertices =
			ChooseByMeshType<INT>(
					MeshType,
					Component->SpeedTree->SRH->BranchDataBuffer.Vertices.Num(),
					Component->SpeedTree->SRH->BranchDataBuffer.Vertices.Num(),
					Component->SpeedTree->SRH->FrondDataBuffer.Vertices.Num(),
					Component->SpeedTree->SRH->LeafCardDataBuffer.Vertices.Num(),
					Component->SpeedTree->SRH->LeafMeshDataBuffer.Vertices.Num(),
					Component->SpeedTree->SRH->BillboardDataBuffer.Vertices.Num()
					);
		TArray<FStaticLightingVertex>& DestVertices =
			*ChooseByMeshType< TArray<FStaticLightingVertex>* >(
				MeshType,
				&BranchVertices,
				&BranchVertices,
				&FrondVertices,
				&LeafCardVertices,
				&LeafMeshVertices,
				&BillboardVertices
				);

		// Copy and convert the mesh's vertices to the static lighting vertex arrays.
		DestVertices.Empty(NumVertices);
		for(INT VertexIndex = 0;VertexIndex < NumVertices;VertexIndex++)
		{
			FVector VertexPosition = SourceVertexPositionArray(VertexIndex).Position;

			if (MeshType == STMT_LeafCards)
			{	
				VertexPosition = GetLeafCardVertexPosition(
					VertexPosition,
					Component->SpeedTree->SRH->LeafCardDataBuffer.Vertices(VertexIndex)
					);
			}

			const FSpeedTreeVertexData* SourceVertexData = GetSpeedTreeVertexData(Component->SpeedTree->SRH,MeshType,VertexIndex);

			FStaticLightingVertex* DestVertex = new(DestVertices) FStaticLightingVertex;
			DestVertex->TextureCoordinates[0] = SourceVertexData->TexCoord;
			for(UINT i = 1;i < MAX_TEXCOORDS;i++)
			{
				DestVertex->TextureCoordinates[i] = FVector2D(0,0);
			}
			DestVertex->WorldPosition = LocalToWorld.TransformFVector(VertexPosition);
			DestVertex->WorldTangentX = LocalToWorld.TransformNormal(SourceVertexData->TangentX).SafeNormal();
			DestVertex->WorldTangentY = LocalToWorld.TransformNormal(SourceVertexData->TangentY).SafeNormal();
			DestVertex->WorldTangentZ = LocalToWorldInverseTranspose.TransformNormal(SourceVertexData->TangentZ).SafeNormal();
		}
	}
}

void FSpeedTreeComponentStaticLighting::CreateMapping(
	FStaticLightingPrimitiveInfo& OutPrimitiveInfo,
	INT LODIndex,
	const FMeshBatchElement& BatchElement,
	ESpeedTreeMeshType MeshType,
	FLOAT ShadowOpacity,
	UBOOL bInTwoSidedMaterial,
	const FLightingBuildOptions& Options
	)
{
	if(BatchElement.NumPrimitives > 0)
	{
		// Create the static lighting mesh.
		FSpeedTreeStaticLightingMesh* const Mesh = new FSpeedTreeStaticLightingMesh(
			this,
			LODIndex,
			BatchElement,
			MeshType,
			ShadowOpacity,
			bInTwoSidedMaterial,
			RelevantLights,
			Options
			);

		// Create the static lighting mapping.
		FSpeedTreeStaticLightingMapping* const Mapping = new FSpeedTreeStaticLightingMapping(Mesh,this,LODIndex,BatchElement,MeshType);

		// Add the mapping to the mappings list.
		Mappings.AddItem(Mapping);

		// Add the mapping and mesh to the static lighting system's list of meshes and mappings for the primitive.
		OutPrimitiveInfo.Meshes.AddItem(Mesh);
		OutPrimitiveInfo.Mappings.AddItem(Mapping);

		// Flag the component as having static lighting for this mesh type.
		UBOOL& bComponentHasStaticLightingForThisMeshType =
			*ChooseByMeshType<UBOOL*>(MeshType,&bHasBranchStaticLighting,&bHasBranchStaticLighting,&bHasFrondStaticLighting,&bHasLeafCardStaticLighting,&bHasLeafMeshStaticLighting,&bHasBillboardStaticLighting);
		bComponentHasStaticLightingForThisMeshType = TRUE;
	}
}

/**
 * Given a set of pre-quantized speed tree lightmap data, merge them into one
 *
 * @param QuantizedMap A map of 0 or more pre-quantized buffers to their MinVertexIndex
 *
 * @return A quantized data buffer with all inputs merged together, or NULL if none. The caller takes ownership of this pointer
 */
FQuantizedLightmapData* MergeQuantizedSpeedTreeBuffers(TMap<FQuantizedLightmapData*, INT>& QuantizedMap)
{
	// if there are no entries, we have no quantized data, so return NULL
	if (QuantizedMap.Num() == 0)
	{
		return NULL;
	}
	// if there is just one, return it directly, no need to copy it
	else if (QuantizedMap.Num() == 1)
	{
		// return the key of the only entry
		return TMap<FQuantizedLightmapData*, INT>::TIterator(QuantizedMap).Key();
	}
	else
	{
		// make a new one to hold all the others
		FQuantizedLightmapData* NewQuantizedData = new FQuantizedLightmapData;

		// set scale to 0
		appMemzero(NewQuantizedData->Scale, sizeof(NewQuantizedData->Scale));

		// go over the inputs, calculating the max scale, and max vertex index needed
		UINT MaxVertexIndex = 0;
		for (TMap<FQuantizedLightmapData*, INT>::TIterator It(QuantizedMap); It; ++It)
		{
			FQuantizedLightmapData* QuantizedData = It.Key();

			for(UINT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
			{
				for(UINT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
				{
					NewQuantizedData->Scale[CoefficientIndex][ColorIndex] = 
						Max(QuantizedData->Scale[CoefficientIndex][ColorIndex], NewQuantizedData->Scale[CoefficientIndex][ColorIndex]);
				}
			}

			// the MinVertexIndex for the mapping is in the value, so the min + num = max
			MaxVertexIndex = Max(MaxVertexIndex, It.Value() + QuantizedData->SizeX);
		}

		// make space for the new data, using the max vertex index
		NewQuantizedData->SizeX = MaxVertexIndex;
		NewQuantizedData->Data.Empty(MaxVertexIndex);
		NewQuantizedData->Data.Add(MaxVertexIndex);

		// Calculate inverse scale for the quantized coefficients.
		FLOAT InvCoefficientScale[NUM_STORED_LIGHTMAP_COEF][3];
		for(UINT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
		{
			for(UINT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
			{
				InvCoefficientScale[CoefficientIndex][ColorIndex] = 1.0f / Max(NewQuantizedData->Scale[CoefficientIndex][ColorIndex],DELTA);
			}
		}

		// requantize samples
		for (TMap<FQuantizedLightmapData*, INT>::TIterator It(QuantizedMap); It; ++It)
		{
			FQuantizedLightmapData* QuantizedData = It.Key();
			INT MinVertexIndex = It.Value();

			for (INT LightIndex = 0; LightIndex < QuantizedData->LightGuids.Num(); LightIndex++)
			{
				// Copy light Guids over
				NewQuantizedData->LightGuids.AddUniqueItem(QuantizedData->LightGuids(LightIndex));
			}

			for (UINT SampleIndex = 0; SampleIndex < QuantizedData->SizeX; SampleIndex++)
			{
				// get source from input, dest from the rectangular offset in the group
				FLightMapCoefficients& SourceSample = QuantizedData->Data(SampleIndex);
				FLightMapCoefficients& DestSample = NewQuantizedData->Data(MinVertexIndex + SampleIndex);

				// coverage doesn't change
				DestSample.Coverage = SourceSample.Coverage;

				// go over each coefficient and dequantize and requantize with new Scale
				for(INT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
				{
					for(INT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
					{
						// dequantize it
						FLOAT Dequantized = (FLOAT)SourceSample.Coefficients[CoefficientIndex][ColorIndex];
						Dequantized = appPow(Dequantized / 255.0f, 2.2f) * QuantizedData->Scale[CoefficientIndex][ColorIndex];

						// requantize it
						DestSample.Coefficients[CoefficientIndex][ColorIndex] = (BYTE)Clamp<INT>(
							appTrunc(
								appPow(
									Dequantized * InvCoefficientScale[CoefficientIndex][ColorIndex],
									1.0f / 2.2f
									) * 255.0f
								),
							0,
							255);
					}
				}
			}

			// we are now done with the source QuantizedData
			delete QuantizedData;
		}

		return NewQuantizedData;
	}
}

void FSpeedTreeComponentStaticLighting::ApplyCompletedMapping(
	FSpeedTreeStaticLightingMapping* Mapping,
	FLightMapData1D* MappingLightMapData,
	const TMap<ULightComponent*,FShadowMapData1D*>& MappingShadowMapData,
	FQuantizedLightmapData* QuantizedData
	)
{
	const INT MinVertexIndex = Mapping->GetMeshElement().MinVertexIndex;

	if (QuantizedData)
	{
		// set the quantized data into the proper TMap for later joining
		TMap<FQuantizedLightmapData*, INT>& DestQuantizedMap = 
			ChooseByMeshType< TMap<FQuantizedLightmapData*, INT>& >(
				Mapping->GetMeshType(),
				BranchQuantizedMap,
				BranchQuantizedMap,
				FrondQuantizedMap,
				LeafCardQuantizedMap,
				LeafMeshQuantizedMap,
				BillboardQuantizedMap
				);

		DestQuantizedMap.Set(QuantizedData, MinVertexIndex);
	}
	else if(MappingLightMapData)
	{
		// Copy the mapping's light-map data into the component's light-map data.
		FLightMapData1D*& DestLightMapData = 
			ChooseByMeshType<FLightMapData1D*&>(
				Mapping->GetMeshType(),
				BranchLightMapData,
				BranchLightMapData,
				FrondLightMapData,
				LeafCardLightMapData,
				LeafMeshLightMapData,
				BillboardLightMapData
				);

		if (!DestLightMapData)
		{
			const INT NumLightMapVertices = 
				ChooseByMeshType<INT>(
					Mapping->GetMeshType(),
					Component->SpeedTree->SRH->BranchDataBuffer.Vertices.Num(),
					Component->SpeedTree->SRH->BranchDataBuffer.Vertices.Num(),
					Component->SpeedTree->SRH->FrondDataBuffer.Vertices.Num(),
					Component->SpeedTree->SRH->LeafCardDataBuffer.Vertices.Num(),
					Component->SpeedTree->SRH->LeafMeshDataBuffer.Vertices.Num(),
					Component->SpeedTree->SRH->BillboardDataBuffer.Vertices.Num()
					);

			DestLightMapData = new FLightMapData1D(NumLightMapVertices);
		}

		for(INT VertexIndex = 0;VertexIndex < MappingLightMapData->GetSize();VertexIndex++)
		{
			(*DestLightMapData)(MinVertexIndex + VertexIndex) = (*MappingLightMapData)(VertexIndex);
		}

		// Merge the light-map's light list into the light list for all the component's light-maps.
		for(INT LightIndex = 0;LightIndex < MappingLightMapData->LightGuids.Num();LightIndex++)
		{
			FGuid LightGuid = MappingLightMapData->LightGuids(LightIndex);
			DestLightMapData->LightGuids.AddUniqueItem(LightGuid);
		}

		// Delete the mapping's temporary light-map data.
		delete MappingLightMapData;
		MappingLightMapData = NULL;
	}

	// Merge the shadow-map data into the component's shadow-map data.
	for(TMap<ULightComponent*,FShadowMapData1D*>::TConstIterator ShadowMapIt(MappingShadowMapData);ShadowMapIt;++ShadowMapIt)
	{
		// Find the existing shadow-maps for this light.
		TRefCountPtr<FLightShadowMaps>* LightShadowMaps = ComponentShadowMaps.Find(ShadowMapIt.Key());
		if(!LightShadowMaps)
		{
			// Create the shadow-maps for the light if they don't exist yet.
			LightShadowMaps = &ComponentShadowMaps.Set(
				ShadowMapIt.Key(),
				new FLightShadowMaps(
					Component->SpeedTree->SRH->BranchDataBuffer.Vertices.Num(),
					Component->SpeedTree->SRH->FrondDataBuffer.Vertices.Num(),
					Component->SpeedTree->SRH->LeafMeshDataBuffer.Vertices.Num(),
					Component->SpeedTree->SRH->LeafCardDataBuffer.Vertices.Num(),
					Component->SpeedTree->SRH->BillboardDataBuffer.Vertices.Num()
					)
				);
		}

		// Copy the mapping's shadow-map data into the component's shadow-map data.
		FShadowMapData1D& DestShadowMapData = 
			*ChooseByMeshType<FShadowMapData1D*>(
				Mapping->GetMeshType(),
				&(*LightShadowMaps)->BranchShadowMapData,
				&(*LightShadowMaps)->BranchShadowMapData,
				&(*LightShadowMaps)->FrondShadowMapData,
				&(*LightShadowMaps)->LeafCardShadowMapData,
				&(*LightShadowMaps)->LeafMeshShadowMapData,
				&(*LightShadowMaps)->BillboardShadowMapData
				);
		for(INT VertexIndex = 0;VertexIndex < ShadowMapIt.Value()->GetSize();VertexIndex++)
		{
			DestShadowMapData(MinVertexIndex + VertexIndex) = (*ShadowMapIt.Value())(VertexIndex);
		}

		// Delete the mapping's temporary shadow-map data.
		delete ShadowMapIt.Value();
	}

	// Check if static lighting has been built for all the component's mappings.
	if(++NumCompleteMappings == Mappings.Num())
	{
		// merge any and all quantized sub-buffers into one quantized buffer (will return NULL if there were no quantized buffers
		FQuantizedLightmapData* BranchQuantizedData = MergeQuantizedSpeedTreeBuffers(BranchQuantizedMap);
		FQuantizedLightmapData* FrondQuantizedData = MergeQuantizedSpeedTreeBuffers(FrondQuantizedMap);
		FQuantizedLightmapData* LeafMeshQuantizedData = MergeQuantizedSpeedTreeBuffers(LeafMeshQuantizedMap);
		FQuantizedLightmapData* LeafCardQuantizedData = MergeQuantizedSpeedTreeBuffers(LeafCardQuantizedMap);
		FQuantizedLightmapData* BillboardQuantizedData = MergeQuantizedSpeedTreeBuffers(BillboardQuantizedMap);

		// Create the component's shadow-maps.
		Component->StaticLights.Empty();
		// Add any relevant lights which we don't have shadow-maps to the static light list.
		const TArray<FGuid>* BranchLightGuids = BranchQuantizedData ? 
			&BranchQuantizedData->LightGuids : 
			BranchLightMapData ? &BranchLightMapData->LightGuids : NULL;
		const TArray<FGuid>* FrondLightGuids = FrondQuantizedData ? 
			&FrondQuantizedData->LightGuids : 
			FrondLightMapData ? &FrondLightMapData->LightGuids : NULL;
		const TArray<FGuid>* LeafMeshLightGuids = LeafMeshQuantizedData ? 
			&LeafMeshQuantizedData->LightGuids : 
			LeafMeshLightMapData ? &LeafMeshLightMapData->LightGuids : NULL;
		const TArray<FGuid>* LeafCardLightGuids = LeafCardQuantizedData ? 
			&LeafCardQuantizedData->LightGuids : 
			LeafCardLightMapData ? &LeafCardLightMapData->LightGuids : NULL;
		const TArray<FGuid>* BillboardLightGuids = BillboardQuantizedData ? 
			&BillboardQuantizedData->LightGuids : 
			BillboardLightMapData ? &BillboardLightMapData->LightGuids : NULL;

		for(INT LightIndex = 0;LightIndex < RelevantLights.Num();LightIndex++)
		{
			ULightComponent* Light = RelevantLights(LightIndex);

			if(!(BranchLightGuids && BranchLightGuids->ContainsItem(Light->LightmapGuid) 
				|| FrondLightGuids && FrondLightGuids->ContainsItem(Light->LightmapGuid) 
				|| LeafMeshLightGuids && LeafMeshLightGuids->ContainsItem(Light->LightmapGuid) 
				|| LeafCardLightGuids && LeafCardLightGuids->ContainsItem(Light->LightmapGuid) 
				|| BillboardLightGuids && BillboardLightGuids->ContainsItem(Light->LightmapGuid)) 
				&& !ComponentShadowMaps.Find(Light))
			{
				FSpeedTreeStaticLight* StaticLight = new(Component->StaticLights) FSpeedTreeStaticLight;
				StaticLight->Guid = Light->LightGuid;
				StaticLight->BranchShadowMap = NULL;
				StaticLight->FrondShadowMap = NULL;
				StaticLight->LeafMeshShadowMap = NULL;
				StaticLight->LeafCardShadowMap = NULL;
				StaticLight->BillboardShadowMap = NULL;
			}
		}

		//@lmtodo verify: This code used to check for the number of lights that were applied to the mapping, but that doesn't work with distributed builds. Is there ever a need to do it? Is it solely an optimization?
		// Create the component's branch and frond light-map.
		Component->BranchLightMap = bHasBranchStaticLighting ? new FLightMap1D(Component, BranchLightMapData, BranchQuantizedData) : NULL;
		// Verify that everything has been cleaned up
		check(BranchLightMapData == NULL && BranchQuantizedData == NULL);
		// Create the component's frond light-map.
		Component->FrondLightMap = bHasFrondStaticLighting ? new FLightMap1D(Component, FrondLightMapData, FrondQuantizedData) : NULL;
		// Verify that everything has been cleaned up
		check(FrondLightMapData == NULL && FrondQuantizedData == NULL);
		// Create the component's leaf mesh light-map.
		Component->LeafMeshLightMap = bHasLeafMeshStaticLighting ? new FLightMap1D(Component, LeafMeshLightMapData, LeafMeshQuantizedData) : NULL;
		// Verify that everything has been cleaned up
		check(LeafMeshLightMapData == NULL && LeafMeshQuantizedData == NULL);

		// Create the component's leaf card light-map.
		Component->LeafCardLightMap = bHasLeafCardStaticLighting ? new FLightMap1D(Component, LeafCardLightMapData, LeafCardQuantizedData) : NULL;
		// Verify that everything has been cleaned up
		check(LeafCardLightMapData == NULL && LeafCardQuantizedData == NULL);

		// Create the component's billboard light-map.
		Component->BillboardLightMap = bHasBillboardStaticLighting ? new FLightMap1D(Component, BillboardLightMapData, BillboardQuantizedData) : NULL;
		// Verify that everything has been cleaned up
		check(BillboardLightMapData == NULL && BillboardQuantizedData == NULL);

		for(TMap<ULightComponent*,TRefCountPtr<FLightShadowMaps> >::TConstIterator ShadowMapIt(ComponentShadowMaps);ShadowMapIt;++ShadowMapIt)
		{
			const ULightComponent* Light = ShadowMapIt.Key();
			FSpeedTreeStaticLight* StaticLight = new(Component->StaticLights) FSpeedTreeStaticLight;
			StaticLight->Guid = Light->LightGuid;
			StaticLight->BranchShadowMap = bHasBranchStaticLighting ? new(Component) UShadowMap1D(Light->LightGuid,ShadowMapIt.Value()->BranchShadowMapData) : NULL;
			StaticLight->FrondShadowMap = bHasFrondStaticLighting ? new(Component) UShadowMap1D(Light->LightGuid,ShadowMapIt.Value()->FrondShadowMapData) : NULL;
			StaticLight->LeafMeshShadowMap = bHasLeafMeshStaticLighting ? new(Component) UShadowMap1D(Light->LightGuid,ShadowMapIt.Value()->LeafMeshShadowMapData) : NULL;
			StaticLight->LeafCardShadowMap = bHasLeafCardStaticLighting ? new(Component) UShadowMap1D(Light->LightGuid,ShadowMapIt.Value()->LeafCardShadowMapData) : NULL;
			StaticLight->BillboardShadowMap = bHasBillboardStaticLighting ? new(Component) UShadowMap1D(Light->LightGuid,ShadowMapIt.Value()->BillboardShadowMapData) : NULL;
		}
	}
}

void USpeedTreeComponent::GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options)
{
	if(IsValidComponent() && HasStaticShadowing() && bAcceptsLights)
	{
		// Count the number of LODs.
		INT NumLODs = Max(
			Max(
				SpeedTree->SRH->FrondElements.Num(),
				SpeedTree->SRH->Branch1Elements.Num()
				),
			Max(
				SpeedTree->SRH->LeafMeshElements.Num(),
				SpeedTree->SRH->LeafCardElements.Num()
				)
			);

		// Create the component static lighting object which aggregates all the static lighting mappings used by the component.
		FSpeedTreeComponentStaticLighting* ComponentStaticLighting = new FSpeedTreeComponentStaticLighting(this,InRelevantLights);

		// Create static lighting mappings and meshes for each LOD.
		for(INT LODIndex = 0;LODIndex < NumLODs;LODIndex++)
		{
			// Create the static lighting mesh and mapping for this LOD of the tree's branch triangles.
			if(bUseBranches && SpeedTree->SRH->bHasBranches && LODIndex < SpeedTree->SRH->Branch1Elements.Num())
			{
				ComponentStaticLighting->CreateMapping(OutPrimitiveInfo,LODIndex,SpeedTree->SRH->Branch1Elements(LODIndex).Elements(0),STMT_Branches1,CastShadow ? 1.0f : 0.0f,FALSE,Options);
				ComponentStaticLighting->CreateMapping(OutPrimitiveInfo,LODIndex,SpeedTree->SRH->Branch2Elements(LODIndex).Elements(0),STMT_Branches2,CastShadow ? 1.0f : 0.0f,FALSE,Options);
			}

			// Create the static lighting mesh and mapping for this LOD of the tree's frond triangles.
			if(bUseFronds && SpeedTree->SRH->bHasFronds && LODIndex < SpeedTree->SRH->FrondElements.Num())
			{
				ComponentStaticLighting->CreateMapping(OutPrimitiveInfo,LODIndex,SpeedTree->SRH->FrondElements(LODIndex).Elements(0),STMT_Fronds,0.0f,TRUE,Options);
			}

			// Create the static lighting mesh and mapping for this LOD of the tree's leaf triangles.
			if(bUseLeafCards && SpeedTree->SRH->bHasLeafCards && LODIndex < SpeedTree->SRH->LeafCardElements.Num())
			{
				ComponentStaticLighting->CreateMapping(OutPrimitiveInfo,LODIndex,SpeedTree->SRH->LeafCardElements(LODIndex).Elements(0),STMT_LeafCards,CastShadow ? SpeedTree->LeafStaticShadowOpacity : 0.0f,TRUE,Options);
			}
			if(bUseLeafMeshes && SpeedTree->SRH->bHasLeafMeshes && LODIndex < SpeedTree->SRH->LeafMeshElements.Num())
			{
				ComponentStaticLighting->CreateMapping(OutPrimitiveInfo,LODIndex,SpeedTree->SRH->LeafMeshElements(LODIndex).Elements(0),STMT_LeafMeshes,CastShadow ? SpeedTree->LeafStaticShadowOpacity : 0.0f,TRUE,Options);
			}
		}
		
		// Create the static lighting mesh and mapping for the tree's billboards.
		if(bUseBillboards && SpeedTree->SRH->bHasBillboards)
		{
			ComponentStaticLighting->CreateMapping(OutPrimitiveInfo,0,SpeedTree->SRH->BillboardElement.Elements(0),STMT_Billboards,0.0f,TRUE,Options);
		}
	}
}

/**
 * Returns the light and shadow map memory for this primite in its out variables.
 *
 * Shadow map memory usage is per light whereof lightmap data is independent of number of lights, assuming at least one.
 *
 * @param [out] LightMapMemoryUsage		Memory usage in bytes for light map (either texel or vertex) data
 * @param [out]	ShadowMapMemoryUsage	Memory usage in bytes for shadow map (either texel or vertex) data
 */
void USpeedTreeComponent::GetLightAndShadowMapMemoryUsage( INT& LightMapMemoryUsage, INT& ShadowMapMemoryUsage ) const
{
	// Zero initialize.
	ShadowMapMemoryUsage	= 0;
	LightMapMemoryUsage		= 0;

	if( HasStaticShadowing() && SpeedTree && SpeedTree->SRH )
	{
		INT NumVertices = 0;
		if(bUseBranches && SpeedTree->SRH->bHasBranches)
		{
			NumVertices += SpeedTree->SRH->BranchPositionBuffer.Vertices.Num();
		}

		if(bUseFronds && SpeedTree->SRH->bHasFronds)
		{
			NumVertices += SpeedTree->SRH->FrondPositionBuffer.Vertices.Num();
		}

		if(bUseLeafCards && SpeedTree->SRH->bHasLeafCards)
		{
			NumVertices += SpeedTree->SRH->LeafCardPositionBuffer.Vertices.Num();
		}
		if(bUseLeafMeshes && SpeedTree->SRH->bHasLeafMeshes)
		{
			NumVertices += SpeedTree->SRH->LeafMeshDataBuffer.Vertices.Num();
		}

		// Create the static lighting mesh and mapping for the tree's billboards.
		if(bUseBillboards && SpeedTree->SRH->bHasBillboards)
		{
			NumVertices += SpeedTree->SRH->BillboardPositionBuffer.Vertices.Num();
		}

		// Stored in vertex buffer.
		ShadowMapMemoryUsage = sizeof(FLOAT) * NumVertices;
		const UINT LightMapSampleSize = GSystemSettings.bAllowDirectionalLightMaps ? sizeof(FQuantizedDirectionalLightSample) : sizeof(FQuantizedSimpleLightSample);
		LightMapMemoryUsage	= LightMapSampleSize * NumVertices;
	}
}

void USpeedTreeComponent::InvalidateLightingCache()
{
	// Save the static mesh state for transactions.
	Modify();

	// Mark lighting as requiring a rebuilt.
	MarkLightingRequiringRebuild();

	// Detach the component from the scene for the duration of this function.
	FComponentReattachContext ReattachContext(this);
	FlushRenderingCommands();
	Super::InvalidateLightingCache();

	// Discard all cached lighting.
	StaticLights.Empty( );
	BranchLightMap = NULL;
	FrondLightMap = NULL;
	LeafCardLightMap = NULL;
	LeafMeshLightMap = NULL;
	BillboardLightMap = NULL;
}

#endif
