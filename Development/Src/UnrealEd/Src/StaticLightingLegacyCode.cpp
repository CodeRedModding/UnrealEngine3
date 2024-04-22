/*=============================================================================
	StaticLightingLegacyCode.cpp: Static lighting code for UE3 lighting.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "UnrealEd.h"
#include "StaticLightingPrivate.h"
#include "UnRaster.h"

extern UBOOL IsLightBehindSurface(const FVector& TrianglePoint,const FVector& TriangleNormal,const ULightComponent* Light);
extern TBitArray<> CullBackfacingLights(UBOOL bTwoSidedMaterial,const FVector& TrianglePoint,const FVector& TriangleNormal,const TArray<ULightComponent*>& Lights);

///////////////////////////////////////////////////////////////////////////////
//	StaticLightingVertexMapping
// Don't compile the static lighting system on consoles.
#if !CONSOLE

/** The maximum number of shadow samples per triangle. */
#define MAX_SHADOW_SAMPLES_PER_TRIANGLE	32

class FStaticLightingVertexMappingProcessor
{
public:

	/** The shadow-maps for the vertex mapping. */
	TMap<ULightComponent*,FShadowMapData1D*> ShadowMapData;

	/** The light-map for the vertex mapping. */
	FGatheredLightMapData1D LightMapData;

	/** Initialization constructor. */
	FStaticLightingVertexMappingProcessor(FStaticLightingVertexMapping* InVertexMapping,FStaticLightingSystem* InSystem)
	:	LightMapData(InVertexMapping->Mesh->NumShadingVertices)
	,	VertexMapping(InVertexMapping)
	,	Mesh(InVertexMapping->Mesh)
	,	System(InSystem)
	,	CoherentRayCache(InVertexMapping->Mesh)
	{}

	/** Processses the vertex mapping. */
	FLightMapData1D* Process();

private:

	struct FAdjacentVertexInfo
	{
		INT VertexIndex;
		FLOAT Weight;
	};

	/** A sample for static vertex lighting. */
	struct FVertexLightingSample
	{
		FStaticLightingVertex SampleVertex;
		TBitArray<> RelevantLightMask;
	};

	FStaticLightingVertexMapping* const VertexMapping;
	const FStaticLightingMesh* const Mesh;
	FStaticLightingSystem* const System;
	FCoherentRayCache CoherentRayCache;

	TArray<FVertexLightingSample> Samples;
	TMultiMap<INT,FAdjacentVertexInfo> SampleToAdjacentVertexMap;

	TArray<FStaticLightingVertex> Vertices;
	TMultiMap<FVector,INT> VertexPositionMap;
	TArray<FLOAT> VertexSampleWeightTotals;

	TArray<FShadowMapData1D*> ShadowMapDataByLightIndex;

	/**
	 * Caches the vertices of a mesh.
	 * @param Mesh - The mesh to cache vertices from.
	 * @param OutVertices - Upon return, contains the meshes vertices.
	 */
	void CacheVertices();

	/** Creates a list of samples for the mapping. */
	void CreateSamples();

	/** Calculates area lighting for the vertex lighting samples. */
	void CalculateAreaLighting();

	/** Calculates direct lighting for the vertex lighting samples. */
	void CalculateDirectLighting();
};

FLightMapData1D* FStaticLightingVertexMappingProcessor::Process()
{
	const FStaticLightingMesh* const Mesh = VertexMapping->Mesh;

	UBOOL bDebugThisMapping = FALSE;
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
	bDebugThisMapping = VertexMapping->DebugThisMapping();
#endif

	// Cache the mesh's vertex data, and build a map from position to indices of vertices at that position.
	CacheVertices();

	// Allocate shadow-map data.
	ShadowMapDataByLightIndex.Empty(Mesh->RelevantLights.Num());
	ShadowMapDataByLightIndex.AddZeroed(Mesh->RelevantLights.Num());

	for (INT LightIdx = 0; LightIdx < VertexMapping->Mesh->RelevantLights.Num(); LightIdx++)
	{
		ULightComponent* LightComp = VertexMapping->Mesh->RelevantLights(LightIdx);
		const UBOOL bUseStaticLighting = LightComp->UseStaticLighting(VertexMapping->bForceDirectLightMap);
		if (bUseStaticLighting)
		{
			LightMapData.LightGuids.AddUniqueItem(LightComp->LightmapGuid);
		}
	}

	// Create the samples for the mesh.
	CreateSamples();

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
	if (bDebugThisMapping)
	{
		GDebugStaticLightingInfo.bValid = TRUE;

		for(INT SampleIndex = 0;SampleIndex < Samples.Num();SampleIndex++)
		{
			const FVertexLightingSample& Sample = Samples(SampleIndex);
			const FLOAT DistanceToDebugVertexSq = (Sample.SampleVertex.WorldPosition - GCurrentSelectedLightmapSample.Position).SizeSquared();
			UBOOL bSampleOfDebugVertex = FALSE;
			for(TMultiMap<INT,FAdjacentVertexInfo>::TConstKeyIterator AdjacentVertexIt(SampleToAdjacentVertexMap,SampleIndex);
				AdjacentVertexIt;
				++AdjacentVertexIt
				)
			{
				const FAdjacentVertexInfo& AdjacentVertexInfo = AdjacentVertexIt.Value();
				if (AdjacentVertexInfo.VertexIndex == GCurrentSelectedLightmapSample.VertexIndex)
				{
					bSampleOfDebugVertex = TRUE;
					break;
				}
			}

			if (DistanceToDebugVertexSq < 40000 || bSampleOfDebugVertex)
			{
				FDebugStaticLightingVertex DebugVertex;
				DebugVertex.VertexNormal = FVector(Sample.SampleVertex.WorldTangentZ);
				DebugVertex.VertexPosition = Sample.SampleVertex.WorldPosition;
				GDebugStaticLightingInfo.Vertices.AddItem(DebugVertex);
				if (bSampleOfDebugVertex)
				{
					GDebugStaticLightingInfo.SelectedVertexIndices.AddItem(GDebugStaticLightingInfo.Vertices.Num() - 1);
					GDebugStaticLightingInfo.SampleRadius = 0;
				}
			}
		}
	}
#endif

	// Calculate area lighting.
	CalculateAreaLighting();

	// Calculate direct lighting.
	CalculateDirectLighting();

	return LightMapData.ConvertToLightmap1D();
}

void FStaticLightingVertexMappingProcessor::CacheVertices()
{
	Vertices.Empty(Mesh->NumShadingVertices);
	Vertices.AddZeroed(Mesh->NumShadingVertices);

	for(INT TriangleIndex = 0;TriangleIndex < Mesh->NumShadingTriangles;TriangleIndex++)
	{
		// Query the mesh for the triangle's vertices.
		FStaticLightingVertex V0;
		FStaticLightingVertex V1;
		FStaticLightingVertex V2;
		Mesh->GetShadingTriangle(TriangleIndex,V0,V1,V2);
		INT I0 = 0;
		INT I1 = 0;
		INT I2 = 0;
		Mesh->GetShadingTriangleIndices(TriangleIndex,I0,I1,I2);

		// Cache the vertices by vertex index.
		Vertices(I0) = V0;
		Vertices(I1) = V1;
		Vertices(I2) = V2;

		// Also map the vertices by position.
		VertexPositionMap.AddUnique(V0.WorldPosition,I0);
		VertexPositionMap.AddUnique(V1.WorldPosition,I1);
		VertexPositionMap.AddUnique(V2.WorldPosition,I2);
	}
}

void FStaticLightingVertexMappingProcessor::CreateSamples()
{
	// Initialize the directly sampled vertex map.
	TBitArray<> DirectlySampledVertexMap;
	DirectlySampledVertexMap.Init(TRUE,Mesh->NumShadingVertices);
	
	// Allocate the vertex sample weight total map.
	VertexSampleWeightTotals.Empty(Mesh->NumShadingVertices);
	VertexSampleWeightTotals.AddZeroed(Mesh->NumShadingVertices);

	// Generate random samples on the faces of larger triangles.
	if(!VertexMapping->bSampleVertices)
	{
		// Setup a thread-safe random stream with a fixed seed, so the sample points are deterministic.
		FRandomStream RandomStream(0);

		// Calculate light visibility for each triangle.
		for(INT TriangleIndex = 0;TriangleIndex < Mesh->NumShadingTriangles;TriangleIndex++)
		{
			// Query the mesh for the triangle's vertex indices.
			INT TriangleVertexIndices[3];
			Mesh->GetShadingTriangleIndices(
				TriangleIndex,
				TriangleVertexIndices[0],
				TriangleVertexIndices[1],
				TriangleVertexIndices[2]
				);

			// Lookup the triangle's vertices.
			FStaticLightingVertex TriangleVertices[3];
			for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			{
				TriangleVertices[VertexIndex] = Vertices(TriangleVertexIndices[VertexIndex]);
			}

			// Compute the triangle's normal.
			const FVector TriangleNormal = (TriangleVertices[2].WorldPosition - TriangleVertices[0].WorldPosition) ^ (TriangleVertices[1].WorldPosition - TriangleVertices[0].WorldPosition);

			// Find the lights which are in front of this triangle.
			const TBitArray<> TriangleRelevantLightMask = CullBackfacingLights(
				Mesh->bTwoSidedMaterial,
				TriangleVertices[0].WorldPosition,
				TriangleNormal,
				Mesh->RelevantLights
				);

			// Compute the triangle's area.
			const FLOAT TriangleArea = 0.5f * TriangleNormal.Size();

			// Compute the number of samples to use for the triangle, proportional to the triangle area.
			const INT NumSamples = Clamp(appTrunc(TriangleArea * VertexMapping->SampleToAreaRatio),0,MAX_SHADOW_SAMPLES_PER_TRIANGLE);
			if(NumSamples)
			{
				// Look up the vertices adjacent to this triangle at each vertex.
				TArray<INT,TInlineAllocator<8> > AdjacentVertexIndices[3];
				for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
				{
					for(TMultiMap<FVector,INT>::TConstKeyIterator VertexPositionMapIt(VertexPositionMap,TriangleVertices[VertexIndex].WorldPosition);
						VertexPositionMapIt;
						++VertexPositionMapIt)
					{
						AdjacentVertexIndices[VertexIndex].AddItem(VertexPositionMapIt.Value());
					}
				}

				// Weight the triangle's samples proportional to the triangle size, but independently of the number of samples.
				const FLOAT TriangleWeight = TriangleArea / NumSamples;

				// Sample the triangle's lighting.
				for(INT TriangleSampleIndex = 0;TriangleSampleIndex < NumSamples;TriangleSampleIndex++)
				{
					// Choose a uniformly distributed random point on the triangle.
					const FLOAT S = 1.0f - appSqrt(RandomStream.GetFraction());
					const FLOAT T = RandomStream.GetFraction() * (1.0f - S);
					const FLOAT U = 1 - S - T;

					// Index the sample's vertex indices and weights.
					const FLOAT TriangleVertexWeights[3] =
					{
						S * TriangleWeight,
						T * TriangleWeight,
						U * TriangleWeight
					};

					// Interpolate the triangle's vertex attributes at the sample point.
					const FStaticLightingVertex SampleVertex =
						TriangleVertices[0] * S +
						TriangleVertices[1] * T +
						TriangleVertices[2] * U;

					// Create the sample.
					const INT SampleIndex = Samples.Num();
					FVertexLightingSample* Sample = new(Samples) FVertexLightingSample;
					Sample->SampleVertex = SampleVertex;
					Sample->RelevantLightMask = TriangleRelevantLightMask;

					// Build a list of vertices whose light-map values will affect this sample point.
					for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
					{
						for(INT AdjacentVertexIndex = 0;AdjacentVertexIndex < AdjacentVertexIndices[VertexIndex].Num();AdjacentVertexIndex++)
						{
							// Copy the adjacent vertex with its weight into the sample.
							FAdjacentVertexInfo AdjacentVertexInfo;
							AdjacentVertexInfo.VertexIndex = AdjacentVertexIndices[VertexIndex](AdjacentVertexIndex);
							AdjacentVertexInfo.Weight = TriangleVertexWeights[VertexIndex];
							SampleToAdjacentVertexMap.Add(SampleIndex,AdjacentVertexInfo);

							// Accumulate the vertex's sum of light-map sample weights.
							VertexSampleWeightTotals(AdjacentVertexInfo.VertexIndex) += TriangleVertexWeights[VertexIndex];

							// Indicate that the vertex doesn't need a direct sample.
							DirectlySampledVertexMap(AdjacentVertexInfo.VertexIndex) = FALSE;
						}
					}
				}
			}
		}
	}

	// Generate samples for vertices of small triangles.
	for(TConstSetBitIterator<> VertexIt(DirectlySampledVertexMap);VertexIt;++VertexIt)
	{
		const INT VertexIndex = VertexIt.GetIndex();
		const FStaticLightingVertex& SampleVertex = Vertices(VertexIndex);

		// Create the sample.
		const INT SampleIndex = Samples.Num();
		FVertexLightingSample* Sample = new(Samples) FVertexLightingSample;
		Sample->SampleVertex = SampleVertex;
		Sample->RelevantLightMask = CullBackfacingLights(
			Mesh->bTwoSidedMaterial,
			SampleVertex.WorldPosition,
			SampleVertex.WorldTangentZ,
			Mesh->RelevantLights
			);
		FAdjacentVertexInfo AdjacentVertexInfo;
		AdjacentVertexInfo.VertexIndex = VertexIndex;
		AdjacentVertexInfo.Weight = 1.0f;
		SampleToAdjacentVertexMap.Add(SampleIndex,AdjacentVertexInfo);

		// Set the vertex sample weight.
		VertexSampleWeightTotals(VertexIndex) = 1.0f;
	}
}

void FStaticLightingVertexMappingProcessor::CalculateAreaLighting()
{
	// Add the sky lights to the light-map's light list.
	UBOOL bHasAreaLights = FALSE;
	UBOOL bHasShadowedAreaLights = FALSE;
	for(INT LightIndex = 0;LightIndex < Mesh->RelevantLights.Num();LightIndex++)
	{
		ULightComponent* Light = Mesh->RelevantLights(LightIndex);
		if(Light->IsA(USkyLightComponent::StaticClass()))
		{
			LightMapData.LightGuids.AddUniqueItem(Light->LightmapGuid);
			bHasAreaLights = TRUE;
			if(Light->CastShadows && Light->CastStaticShadows)
			{
				bHasShadowedAreaLights = TRUE;
			}
		}
	}

	FRandomStream SampleGenerator(0);
	
	// Populate the area lighting cache for the mesh.
	if(bHasShadowedAreaLights)
	{
		for(INT SampleIndex = 0;SampleIndex < Samples.Num();SampleIndex++)
		{
			const FVertexLightingSample& Sample = Samples(SampleIndex);
			System->CalculatePointAreaLighting(VertexMapping,Sample.SampleVertex,CoherentRayCache,SampleGenerator,FALSE);
		}
	}

	// Map the fully populated area lighting cache onto the mesh.
	if(bHasAreaLights)
	{
		for(INT SampleIndex = 0;SampleIndex < Samples.Num();SampleIndex++)
		{
			const FVertexLightingSample& Sample = Samples(SampleIndex);
			const FGatheredLightSample AreaLightingSample = System->CalculatePointAreaLighting(VertexMapping,Sample.SampleVertex,CoherentRayCache,SampleGenerator,FALSE);
			for(TMultiMap<INT,FAdjacentVertexInfo>::TConstKeyIterator AdjacentVertexIt(SampleToAdjacentVertexMap,SampleIndex);
				AdjacentVertexIt;
				++AdjacentVertexIt
				)
			{
				const FAdjacentVertexInfo& AdjacentVertexInfo = AdjacentVertexIt.Value();
				const FLOAT NormalizedWeight = AdjacentVertexInfo.Weight / VertexSampleWeightTotals(AdjacentVertexInfo.VertexIndex);
				LightMapData(AdjacentVertexInfo.VertexIndex).AddWeighted(AreaLightingSample,NormalizedWeight);
			}
		}
	}
}

void FStaticLightingVertexMappingProcessor::CalculateDirectLighting()
{
	// Calculate direct lighting at the generated sample points.
	for(INT SampleIndex = 0;SampleIndex < Samples.Num();SampleIndex++)
	{
		const FVertexLightingSample& Sample = Samples(SampleIndex);
		const FStaticLightingVertex& SampleVertex = Sample.SampleVertex;

		// Add the sample's contribution to the vertex'x shadow-map values.
		for(INT LightIndex = 0;LightIndex < Mesh->RelevantLights.Num();LightIndex++)
		{
			if(Sample.RelevantLightMask(LightIndex))
			{
				ULightComponent* Light = Mesh->RelevantLights(LightIndex);

				// Skip sky lights, since their static lighting is computed separately.
				if(Light->IsA(USkyLightComponent::StaticClass()))
				{
					continue;
				}

				// Compute the shadowing of this sample point from the light.
				const UBOOL bIsShadowed = System->CalculatePointShadowing(
					VertexMapping,
					SampleVertex.WorldPosition,
					Light,
					CoherentRayCache
					);
				if(!bIsShadowed)
				{
					// Accumulate the sample lighting and shadowing at the adjacent vertices.
					for(TMultiMap<INT,FAdjacentVertexInfo>::TConstKeyIterator AdjacentVertexIt(SampleToAdjacentVertexMap,SampleIndex);
						AdjacentVertexIt;
						++AdjacentVertexIt
						)
					{
						const FAdjacentVertexInfo& AdjacentVertexInfo = AdjacentVertexIt.Value();
						const FStaticLightingVertex& AdjacentVertex = Vertices(AdjacentVertexInfo.VertexIndex);
						const FLOAT NormalizedWeight = AdjacentVertexInfo.Weight / VertexSampleWeightTotals(AdjacentVertexInfo.VertexIndex);

						// Determine whether to use a shadow-map or the light-map for this light.
						const UBOOL bUseStaticLighting = Light->UseStaticLighting(VertexMapping->bForceDirectLightMap);
						if(bUseStaticLighting)
						{
							// Use the adjacent vertex's tangent basis to calculate this sample's contribution to its light-map value.
							FStaticLightingVertex AdjacentSampleVertex = SampleVertex;
							AdjacentSampleVertex.WorldTangentX = AdjacentVertex.WorldTangentX;
							AdjacentSampleVertex.WorldTangentY = AdjacentVertex.WorldTangentY;
							AdjacentSampleVertex.WorldTangentZ = AdjacentVertex.WorldTangentZ;

							// Calculate the sample's direct lighting from this light.
							const FGatheredLightSample DirectLighting = System->CalculatePointLighting(
								VertexMapping,
								AdjacentSampleVertex,
								Light);

							// Add the sampled direct lighting to the vertex's light-map value.
							LightMapData(AdjacentVertexInfo.VertexIndex).AddWeighted(DirectLighting,NormalizedWeight);

							// Add the light to the light-map's light list.
							LightMapData.LightGuids.AddUniqueItem(Light->LightmapGuid);
						}
						else
						{
							// Lookup the shadow-map used by this light.
							FShadowMapData1D* CurrentLightShadowMapData = ShadowMapDataByLightIndex(LightIndex);
							if(!CurrentLightShadowMapData)
							{
								// If this the first sample unshadowed from this light, create a shadow-map for it.
								CurrentLightShadowMapData = new FShadowMapData1D(Mesh->NumShadingVertices);
								ShadowMapDataByLightIndex(LightIndex) = CurrentLightShadowMapData;
								ShadowMapData.Set(Light,CurrentLightShadowMapData);
							}
								
							// Accumulate the sample shadowing.
							(*CurrentLightShadowMapData)(AdjacentVertexInfo.VertexIndex) += NormalizedWeight;
						}
					}
				}
			}
		}
	}
}

#endif	//!CONSOLE

///////////////////////////////////////////////////////////////////////////////
//	StaticLightingTextureMapping
// Don't compile the static lighting system on consoles.
#if !CONSOLE

/** A map from light-map texels to the world-space surface points which map the texels. */
class FTexelToVertexMap
{
public:

	/** A map from a texel to the world-space surface point which maps the texel. */
	struct FTexelToVertex
	{
		FVector WorldPosition;
		FPackedNormal WorldTangentX;
		FPackedNormal WorldTangentY;
		FPackedNormal WorldTangentZ;

		FVector WorldSurfaceNormal;
		FLOAT TotalSampleWeight;

		/** Create a static lighting vertex to represent the texel. */
		FStaticLightingVertex GetVertex() const
		{
			FStaticLightingVertex Vertex;
			Vertex.WorldPosition = WorldPosition;
			Vertex.WorldTangentX = WorldTangentX;
			Vertex.WorldTangentY = WorldTangentY;
			Vertex.WorldTangentZ = WorldTangentZ;
			return Vertex;
		}
	};

	/** Initialization constructor. */
	FTexelToVertexMap(INT InSizeX,INT InSizeY):
		Data(InSizeX * InSizeY),
		SizeX(InSizeX),
		SizeY(InSizeY)
	{
		// Clear the map to zero.
		for(INT Y = 0;Y < SizeY;Y++)
		{
			for(INT X = 0;X < SizeX;X++)
			{
				appMemzero(&(*this)(X,Y),sizeof(FTexelToVertex));
			}
		}
	}

	// Accessors.
	FTexelToVertex& operator()(INT X,INT Y)
	{
		const UINT TexelIndex = Y * SizeX + X;
		return Data(TexelIndex);
	}
	const FTexelToVertex& operator()(INT X,INT Y) const
	{
		const INT TexelIndex = Y * SizeX + X;
		return Data(TexelIndex);
	}

private:

	/** The mapping data. */
	TChunkedArray<FTexelToVertex> Data;

	/** The width of the mapping data. */
	INT SizeX;

	/** The height of the mapping data. */
	INT SizeY;
};

/** Used to map static lighting texels to vertices. */
class FStaticLightingRasterPolicy
{
public:

	typedef FStaticLightingVertex InterpolantType;

    /** Initialization constructor. */
	FStaticLightingRasterPolicy(
		const FStaticLightingTextureMapping* InMapping,
		FTexelToVertexMap& InTexelToVertexMap,
		const FVector& InNormal,
		FLOAT InSampleWeight
		):
		Mapping(InMapping),
		TexelToVertexMap(InTexelToVertexMap),
		Normal(InNormal),
		SampleWeight(InSampleWeight)
	{
	}

protected:

	// FTriangleRasterizer policy interface.

	INT GetMinX() const { return 0; }
	INT GetMaxX() const { return Mapping->SizeX - 1; }
	INT GetMinY() const { return 0; }
	INT GetMaxY() const { return Mapping->SizeY - 1; }

	void ProcessPixel(INT X,INT Y,const InterpolantType& Interpolant,UBOOL BackFacing);

private:

	/** The mapping which is being rasterized. */
    const FStaticLightingTextureMapping* Mapping;
	
	/** The texel to vertex map which is being rasterized to. */
	FTexelToVertexMap& TexelToVertexMap;

	/** The normal of the triangle being rasterized. */
	const FVector Normal;

	/** The weight of the current sample. */
	const FLOAT SampleWeight;
};

void FStaticLightingRasterPolicy::ProcessPixel(INT X,INT Y,const InterpolantType& Vertex,UBOOL BackFacing)
{
	FTexelToVertexMap::FTexelToVertex& TexelToVertex = TexelToVertexMap(X,Y);

	// Update the sample weight, and compute the scales used to update the sample's averages.
	const FLOAT NewTotalSampleWeight = TexelToVertex.TotalSampleWeight + SampleWeight;
	const FLOAT OldSampleWeight = TexelToVertex.TotalSampleWeight / NewTotalSampleWeight;
	const FLOAT NewSampleWeight = SampleWeight / NewTotalSampleWeight;
	TexelToVertex.TotalSampleWeight = NewTotalSampleWeight;

	// Add this sample to the mapping.
	TexelToVertex.WorldPosition = TexelToVertex.WorldPosition * OldSampleWeight + Vertex.WorldPosition * NewSampleWeight;
	TexelToVertex.WorldTangentX = FVector(Vertex.WorldTangentX) * OldSampleWeight + Vertex.WorldTangentX * NewSampleWeight;
	TexelToVertex.WorldTangentY = FVector(Vertex.WorldTangentY) * OldSampleWeight + Vertex.WorldTangentY * NewSampleWeight;
	TexelToVertex.WorldTangentZ = FVector(Vertex.WorldTangentZ) * OldSampleWeight + Vertex.WorldTangentZ * NewSampleWeight;
	TexelToVertex.WorldSurfaceNormal = TexelToVertex.WorldSurfaceNormal * OldSampleWeight + Normal * SampleWeight;
}

void FStaticLightingSystem::ProcessTextureMapping(FStaticLightingTextureMapping* TextureMapping)
{
	UBOOL bDebugThisMapping = FALSE;
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
	bDebugThisMapping = TextureMapping->DebugThisMapping();
#endif

	TMap<ULightComponent*,FShadowMapData2D*> ShadowMaps;
	FCoherentRayCache CoherentRayCache(TextureMapping->Mesh);

	// Allocate light-map data.
	FGatheredLightMapData2D LightMapData(TextureMapping->SizeX,TextureMapping->SizeY);

	// Allocate the texel to vertex map.
	FTexelToVertexMap TexelToVertexMap(TextureMapping->SizeX,TextureMapping->SizeY);

	// Rasterize the triangles into the texel to vertex map.
	if (bAllowBilinearTexelRasterization && TextureMapping->bBilinearFilter == TRUE)
	{
		// Use multiple samples to approximate convolving the triangle by the bilinear filter used to sample the light-map texture.
		const UINT NumBilinearFilterSamples = 16;
		FRandomStream SampleGenerator(0);
		for(INT SampleIndex = 0;SampleIndex < NumBilinearFilterSamples;SampleIndex++)
		{
			// Randomly sample the bilinear filter function.
			const FLOAT SampleX = -1.0f + 2.0f * SampleGenerator.GetFraction();
			const FLOAT SampleY = -1.0f + 2.0f * SampleGenerator.GetFraction();
			const FLOAT SampleWeight = (1.0f - Abs(SampleX)) * (1.0f - Abs(SampleY));
			// Rasterize the triangles offset by the random sample location.
			for(INT TriangleIndex = 0;TriangleIndex < TextureMapping->Mesh->NumTriangles;TriangleIndex++)
			{
				// Query the mesh for the triangle's vertices.
				FStaticLightingVertex V0;
				FStaticLightingVertex V1;
				FStaticLightingVertex V2;
				TextureMapping->Mesh->GetTriangle(TriangleIndex,V0,V1,V2);

				// Rasterize the triangle using its the mapping's texture coordinate channel.
				FTriangleRasterizer<FStaticLightingRasterPolicy> TexelMappingRasterizer(FStaticLightingRasterPolicy(
						TextureMapping,
						TexelToVertexMap,
						((V2.WorldPosition - V0.WorldPosition) ^ (V1.WorldPosition - V0.WorldPosition)).SafeNormal(),
						SampleWeight
						));
				TexelMappingRasterizer.DrawTriangle(
					V0,
					V1,
					V2,
					V0.TextureCoordinates[TextureMapping->LightmapTextureCoordinateIndex] * FVector2D(TextureMapping->SizeX,TextureMapping->SizeY) + FVector2D(-0.5f + SampleX,-0.5f + SampleY),
					V1.TextureCoordinates[TextureMapping->LightmapTextureCoordinateIndex] * FVector2D(TextureMapping->SizeX,TextureMapping->SizeY) + FVector2D(-0.5f + SampleX,-0.5f + SampleY),
					V2.TextureCoordinates[TextureMapping->LightmapTextureCoordinateIndex] * FVector2D(TextureMapping->SizeX,TextureMapping->SizeY) + FVector2D(-0.5f + SampleX,-0.5f + SampleY),
					FALSE
					);
			}
		}
	}
	else
	{
		const FLOAT SampleWeight = 1.0f;
		// Rasterize the triangles offset by the random sample location.
		for(INT TriangleIndex = 0;TriangleIndex < TextureMapping->Mesh->NumTriangles;TriangleIndex++)
		{
			// Query the mesh for the triangle's vertices.
			FStaticLightingVertex V0;
			FStaticLightingVertex V1;
			FStaticLightingVertex V2;
			TextureMapping->Mesh->GetTriangle(TriangleIndex,V0,V1,V2);

			// Rasterize the triangle using its the mapping's texture coordinate channel.
			FTriangleRasterizer<FStaticLightingRasterPolicy> TexelMappingRasterizer(FStaticLightingRasterPolicy(
					TextureMapping,
					TexelToVertexMap,
					((V2.WorldPosition - V0.WorldPosition) ^ (V1.WorldPosition - V0.WorldPosition)).SafeNormal(),
					SampleWeight
					));
			TexelMappingRasterizer.DrawTriangle(
				V0,
				V1,
				V2,
				V0.TextureCoordinates[TextureMapping->LightmapTextureCoordinateIndex] * FVector2D(TextureMapping->SizeX,TextureMapping->SizeY) + FVector2D(-0.5f,-0.5f),
				V1.TextureCoordinates[TextureMapping->LightmapTextureCoordinateIndex] * FVector2D(TextureMapping->SizeX,TextureMapping->SizeY) + FVector2D(-0.5f,-0.5f),
				V2.TextureCoordinates[TextureMapping->LightmapTextureCoordinateIndex] * FVector2D(TextureMapping->SizeX,TextureMapping->SizeY) + FVector2D(-0.5f,-0.5f),
				FALSE
				);
		}
	}

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
	if (bDebugThisMapping)
	{
		GDebugStaticLightingInfo.bValid = TRUE;
		GDebugStaticLightingInfo.Vertices.Empty(TextureMapping->SizeY * TextureMapping->SizeX);
		for(INT Y = 0;Y < TextureMapping->SizeY;Y++)
		{
			for(INT X = 0;X < TextureMapping->SizeX;X++)
			{
				const FTexelToVertexMap::FTexelToVertex& TexelToVertex = TexelToVertexMap(X,Y);
				if (TexelToVertex.TotalSampleWeight > DELTA)
				{
					const FLOAT DistanceToDebugTexelSq = (TexelToVertex.WorldPosition - GCurrentSelectedLightmapSample.Position).SizeSquared();
					if (DistanceToDebugTexelSq < 40000
						|| X == GCurrentSelectedLightmapSample.LocalX && Y == GCurrentSelectedLightmapSample.LocalY)
					{
						FDebugStaticLightingVertex DebugVertex;
						DebugVertex.VertexNormal = FVector(TexelToVertex.WorldTangentZ);
						DebugVertex.VertexPosition = TexelToVertex.WorldPosition;
						GDebugStaticLightingInfo.Vertices.AddItem(DebugVertex);
						if (X == GCurrentSelectedLightmapSample.LocalX && Y == GCurrentSelectedLightmapSample.LocalY)
						{
							GDebugStaticLightingInfo.SelectedVertexIndices.AddItem(GDebugStaticLightingInfo.Vertices.Num() - 1);
							GDebugStaticLightingInfo.SampleRadius = 0;
						}
					}
				}
			}
		}
	}
#endif

	for(INT LightIndex = 0;LightIndex < TextureMapping->Mesh->RelevantLights.Num();LightIndex++)
	{
		ULightComponent* Light = TextureMapping->Mesh->RelevantLights(LightIndex);
		const FVector4 LightPosition = Light->GetPosition();

		if(Light->IsA(USkyLightComponent::StaticClass()))
		{
			continue;
		}

		// Raytrace the texels of the shadow-map that map to vertices on a world-space surface.
		FShadowFactorData2D ShadowMapData(TextureMapping->SizeX,TextureMapping->SizeY);
		for(INT Y = 0;Y < TextureMapping->SizeY;Y++)
		{
			for(INT X = 0;X < TextureMapping->SizeX;X++)
			{
				UBOOL bDebugCurrentTexel = FALSE;
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
				if (bDebugThisMapping
					&& X == GCurrentSelectedLightmapSample.LocalX 
					&& Y == GCurrentSelectedLightmapSample.LocalY)
				{
					bDebugCurrentTexel = TRUE;
				}
#endif
				const FTexelToVertexMap::FTexelToVertex& TexelToVertex = TexelToVertexMap(X,Y);
				if(TexelToVertex.TotalSampleWeight > DELTA)
				{
					const FVector& SurfaceNormal = TexelToVertex.WorldSurfaceNormal;

					FShadowSample& ShadowSample = ShadowMapData(X,Y);
					ShadowSample.IsMapped = TRUE;

					// Check if the light is in front of the surface.
					const UBOOL bLightIsInFrontOfTriangle = !IsLightBehindSurface(TexelToVertex.WorldPosition,FVector(TexelToVertex.WorldTangentZ),Light);
					if(/**TextureMapping->Mesh->bTwoSidedMaterial ||*/ bLightIsInFrontOfTriangle)
					{
						// Compute the shadow factors for this sample from the shadow-mapped lights.
						ShadowSample.Visibility = CalculatePointShadowing(TextureMapping,TexelToVertex.WorldPosition,Light,CoherentRayCache) 
							? 0.0f : 1.0f;
					}
				}
			}
		}

		// Filter the shadow-map, and detect completely occluded lights.
		FShadowFactorData2D* FilteredShadowMapData = new FShadowFactorData2D(TextureMapping->SizeX,TextureMapping->SizeY);
		UBOOL bIsCompletelyOccluded = TRUE;
		for(INT Y = 0;Y < TextureMapping->SizeY;Y++)
		{
			for(INT X = 0;X < TextureMapping->SizeX;X++)
			{
				if(ShadowMapData(X,Y).IsMapped)
				{
					// The shadow-map filter.
					static const UINT FilterSizeX = 5;
					static const UINT FilterSizeY = 5;
					static const UINT FilterMiddleX = (FilterSizeX - 1) / 2;
					static const UINT FilterMiddleY = (FilterSizeY - 1) / 2;
					static const UINT Filter[5][5] =
					{
						{ 58,  85,  96,  85, 58 },
						{ 85, 123, 140, 123, 85 },
						{ 96, 140, 159, 140, 96 },
						{ 85, 123, 140, 123, 85 },
						{ 58,  85,  96,  85, 58 }
					};

					// Gather the filtered samples for this texel.
					UINT Visibility = 0;
					UINT Coverage = 0;
					for(UINT FilterY = 0;FilterY < FilterSizeX;FilterY++)
					{
						for(UINT FilterX = 0;FilterX < FilterSizeY;FilterX++)
						{
							INT	SubX = (INT)X - FilterMiddleX + FilterX,
								SubY = (INT)Y - FilterMiddleY + FilterY;
							if(SubX >= 0 && SubX < (INT)TextureMapping->SizeX && SubY >= 0 && SubY < (INT)TextureMapping->SizeY)
							{
								if(ShadowMapData(SubX,SubY).IsMapped)
								{
									Visibility += appTrunc(Filter[FilterX][FilterY] * ShadowMapData(SubX,SubY).Visibility);
									Coverage += Filter[FilterX][FilterY];
								}
							}
						}
					}

					// Keep track of whether any texels have an unoccluded view of the light.
					if(Visibility > 0)
					{
						bIsCompletelyOccluded = FALSE;
					}

					// Write the filtered shadow-map texel.
					(*FilteredShadowMapData)(X,Y).Visibility = (FLOAT)Visibility / (FLOAT)Coverage;
					(*FilteredShadowMapData)(X,Y).IsMapped = TRUE;
				}
				else
				{
					(*FilteredShadowMapData)(X,Y).IsMapped = FALSE;
				}
			}
		}

		if(bIsCompletelyOccluded)
		{
			// If the light is completely occluded, discard the shadow-map.
			delete FilteredShadowMapData;
			FilteredShadowMapData = NULL;
		}
		else
		{
			// Check whether the light should use a light-map or shadow-map.
			const UBOOL bUseStaticLighting = Light->UseStaticLighting(TextureMapping->bForceDirectLightMap);
			if(bUseStaticLighting)
			{
				// Convert the shadow-map into a light-map.
				for(INT Y = 0;Y < TextureMapping->SizeY;Y++)
				{
					for(INT X = 0;X < TextureMapping->SizeX;X++)
					{
						if((*FilteredShadowMapData)(X,Y).IsMapped)
						{
							LightMapData(X,Y).bIsMapped = TRUE;

							// Compute the light sample for this texel based on the corresponding vertex and its shadow factor.
							FLOAT ShadowFactor = (*FilteredShadowMapData)(X,Y).Visibility;
							if(ShadowFactor > 0.0f)
							{
								const FTexelToVertexMap::FTexelToVertex& TexelToVertex = TexelToVertexMap(X,Y);

								// Calculate the lighting for the texel.
								check(TexelToVertex.TotalSampleWeight > DELTA);
								LightMapData(X,Y).AddWeighted(CalculatePointLighting(TextureMapping,TexelToVertex.GetVertex(),Light),ShadowFactor);
							}
						}
					}
				}

				// Add the light to the light-map's light list.
				LightMapData.LightGuids.AddItem(Light->LightmapGuid);

				// Free the shadow-map.
				delete FilteredShadowMapData;
			}
			// only allow for shadow maps if shadow casting is enabled
			else if( Light->CastShadows && Light->CastStaticShadows )
			{
				ShadowMaps.Set(Light,FilteredShadowMapData);
			}
			else
			{
				delete FilteredShadowMapData;
				FilteredShadowMapData = NULL;
			}
		}
	}

	// Add the area lights to the light-map's light list.
	UBOOL bHasAreaLights = FALSE;
	UBOOL bHasShadowedAreaLights = FALSE;
	for(INT LightIndex = 0;LightIndex < TextureMapping->Mesh->RelevantLights.Num();LightIndex++)
	{
		ULightComponent* Light = TextureMapping->Mesh->RelevantLights(LightIndex);
		if(Light->IsA(USkyLightComponent::StaticClass()))
		{
			LightMapData.LightGuids.AddUniqueItem(Light->LightmapGuid);
			bHasAreaLights = TRUE;
			if(Light->CastShadows && Light->CastStaticShadows)
			{
				bHasShadowedAreaLights = TRUE;
			}
		}
	}

	// Compute the area lighting for each of the mapping's texels.
	// Do it twice to allow the first pass to prime the lighting cache.
	if(bHasAreaLights)
	{
		FRandomStream SampleGenerator(0);
		// Skip the priming pass if radiance caching is disabled
		for(INT Pass = 0;Pass < 2;Pass++)
		{
			const UBOOL bApplyAreaLighting = (Pass == 1);
			if(bApplyAreaLighting || bHasShadowedAreaLights)
			{
				for(INT Y = 0;Y < TextureMapping->SizeY;Y++)
				{
					for(INT X = 0;X < TextureMapping->SizeX;X++)
					{
						UBOOL bDebugCurrentTexel = FALSE;
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
						if (bDebugThisMapping
							&& X == GCurrentSelectedLightmapSample.LocalX 
							&& Y == GCurrentSelectedLightmapSample.LocalY)
						{
							bDebugCurrentTexel = TRUE;
						}
#endif

						const FTexelToVertexMap::FTexelToVertex& TexelToVertex = TexelToVertexMap(X,Y);
						if(TexelToVertex.TotalSampleWeight > DELTA)
						{
							// Compute the sample's sky lighting.
							const FGatheredLightSample AreaLightSample = CalculatePointAreaLighting(TextureMapping,TexelToVertex.GetVertex(),CoherentRayCache,SampleGenerator,bDebugCurrentTexel);

							// Don't apply the area lighting in the first pass, since the area lighting cache hasn't been fully populated et.
							if(bApplyAreaLighting)
							{
								LightMapData(X,Y).AddWeighted(AreaLightSample,1.0f);
								LightMapData(X,Y).bIsMapped = TRUE;
							}
						}
					}
				}
			}
		}
	}

	FLightMapData2D* FinalLightMapData = LightMapData.ConvertToLightmap2D();

	// Enqueue the static lighting for application in the main thread.
	TList<FTextureMappingStaticLightingData>* StaticLightingLink = new TList<FTextureMappingStaticLightingData>(FTextureMappingStaticLightingData(),NULL);
	StaticLightingLink->Element.Mapping = TextureMapping;
	StaticLightingLink->Element.LightMapData = FinalLightMapData;
	StaticLightingLink->Element.ShadowMaps = ShadowMaps;
	CompleteTextureMappingList.AddElement(StaticLightingLink);
}

#endif	//!CONSOLE

///////////////////////////////////////////////////////////////////////////////
//	StaticLightingAggregateMesh.cpp
void FStaticLightingAggregateMesh::PrepareForRaytracing()
{
	// Log information about the aggregate mesh.
	debugf(TEXT("Static lighting kd-tree: %u vertices, %u triangles"),Vertices.Num(),kDOPTriangles.Num());

	// Build the kd-tree for simple meshes.
	kDopTree.Build(kDOPTriangles);
	kDOPTriangles.Empty();
}

///////////////////////////////////////////////////////////////////////////////
//	UnShadow.cpp
/** The distance to pull back the shadow visibility traces to avoid the surface shadowing itself. */
#define SHADOW_VISIBILITY_DISTANCE_BIAS	4.0f

/** The cosine of the angle threshold beyond which to ignore lighting records. */
#define LIGHTING_CACHE_NORMAL_THRESHOLD	0.9f

/** 
 * The three orthonormal bases used with directional static lighting, and then the surface normal in tangent space.
 */
static FVector LightMapBasis[NUM_GATHERED_LIGHTMAP_COEF] =
{
	FVector(	0.0f,						appSqrt(6.0f) / 3.0f,			1.0f / appSqrt(3.0f)),
	FVector(	-1.0f / appSqrt(2.0f),		-1.0f / appSqrt(6.0f),			1.0f / appSqrt(3.0f)),
	FVector(	+1.0f / appSqrt(2.0f),		-1.0f / appSqrt(6.0f),			1.0f / appSqrt(3.0f)),
	FVector(	0.0f,						0.0f,							1.0f)
};

/** Fudge factor to roughly match up lightmaps and regular lighting. */
const FLOAT LIGHTMAP_COLOR_FUDGE_FACTOR = 1.5f;

FGatheredLightSample FGatheredLightSample::PointLight(const FLinearColor& Color,const FVector& Direction)
{
	FGatheredLightSample Result;
	FLinearColor FudgedColor = Color * LIGHTMAP_COLOR_FUDGE_FACTOR;
	for(INT CoefficientIndex = 0;CoefficientIndex < NUM_GATHERED_LIGHTMAP_COEF;CoefficientIndex++)
	{
		const FLOAT CoefficientScale = Max(0.0f,Direction | LightMapBasis[CoefficientIndex]);
		Result.Coefficients[CoefficientIndex][0] = FudgedColor.R * CoefficientScale;
		Result.Coefficients[CoefficientIndex][1] = FudgedColor.G * CoefficientScale;
		Result.Coefficients[CoefficientIndex][2] = FudgedColor.B * CoefficientScale;
	}
	return Result;
}

FGatheredLightSample FGatheredLightSample::SkyLight(const FLinearColor& UpperColor,const FLinearColor& LowerColor,const FVector& WorldZ)
{
	FGatheredLightSample Result;
	for(INT CoefficientIndex = 0;CoefficientIndex < NUM_GATHERED_LIGHTMAP_COEF;CoefficientIndex++)
	{
		const FLOAT Dot = WorldZ | LightMapBasis[CoefficientIndex];
		const FLOAT UpperCoefficientScale = Square(0.5f + 0.5f * +Dot);
		const FLOAT LowerCoefficientScale = Square(0.5f + 0.5f * -Dot);
		Result.Coefficients[CoefficientIndex][0] = UpperColor.R * UpperCoefficientScale + LowerColor.R * LowerCoefficientScale;
		Result.Coefficients[CoefficientIndex][1] = UpperColor.G * UpperCoefficientScale + LowerColor.G * LowerCoefficientScale;
		Result.Coefficients[CoefficientIndex][2] = UpperColor.B * UpperCoefficientScale + LowerColor.B * LowerCoefficientScale;
	}
	return Result;
}

void FGatheredLightSample::AddWeighted(const FGatheredLightSample& OtherSample,FLOAT Weight)
{
	for(INT CoefficientIndex = 0;CoefficientIndex < NUM_GATHERED_LIGHTMAP_COEF;CoefficientIndex++)
	{
		for(INT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
		{
			Coefficients[CoefficientIndex][ColorIndex] = Coefficients[CoefficientIndex][ColorIndex] + OtherSample.Coefficients[CoefficientIndex][ColorIndex] * Weight;
		}
	}
}

/** Converts an FGatheredLightSample into a FLightSample. */
FLightSample FGatheredLightSample::ConvertToLightSample() const
{
	checkAtCompileTime(NUM_DIRECTIONAL_LIGHTMAP_COEF == 2, CodeAssumesTwoStoredDirectionalCoefficients);
	checkAtCompileTime(NUM_GATHERED_DIRECTIONAL_LIGHTMAP_COEF == 3, CodeAssumesThreeGatheredDirectionalCoefficients);
	FLightSample NewSample;
	NewSample.bIsMapped = bIsMapped;

	FLinearColor AverageColor(FLinearColor::Black);
	for (INT i = 0; i < NUM_GATHERED_DIRECTIONAL_LIGHTMAP_COEF; i++)
	{
		const FLinearColor CurrentCoefficient(Coefficients[i][0], Coefficients[i][1], Coefficients[i][2]);
		AverageColor += CurrentCoefficient / (FLOAT)NUM_GATHERED_DIRECTIONAL_LIGHTMAP_COEF;
	}

	const FLOAT MaxAverageComponent = Max(AverageColor.R, Max(AverageColor.G, AverageColor.B));
	if (MaxAverageComponent > 0.0f)
	{
		NewSample.Coefficients[0][0] = AverageColor.R / MaxAverageComponent;
		NewSample.Coefficients[0][1] = AverageColor.G / MaxAverageComponent;
		NewSample.Coefficients[0][2] = AverageColor.B / MaxAverageComponent;
	}

	FVector4 DirectionalMaxComponents(0,0,0);
	for (INT i = 0; i < NUM_GATHERED_DIRECTIONAL_LIGHTMAP_COEF; i++)
	{
		const FLOAT MaxComponent = Max(Coefficients[i][0], Max(Coefficients[i][1], Coefficients[i][2]));
		DirectionalMaxComponents.Component(i) = MaxComponent;
	}

	NewSample.Coefficients[1][0] = DirectionalMaxComponents.X;
	NewSample.Coefficients[1][1] = DirectionalMaxComponents.Y;
	NewSample.Coefficients[1][2] = DirectionalMaxComponents.Z;

	NewSample.Coefficients[SIMPLE_LIGHTMAP_COEF_INDEX][0] = Coefficients[SIMPLE_GATHERED_LIGHTMAP_COEF_INDEX][0];
	NewSample.Coefficients[SIMPLE_LIGHTMAP_COEF_INDEX][1] = Coefficients[SIMPLE_GATHERED_LIGHTMAP_COEF_INDEX][1];
	NewSample.Coefficients[SIMPLE_LIGHTMAP_COEF_INDEX][2] = Coefficients[SIMPLE_GATHERED_LIGHTMAP_COEF_INDEX][2];
	return NewSample;
}

FLightMapData1D* FGatheredLightMapData1D::ConvertToLightmap1D() const
{
	FLightMapData1D* ConvertedLightMap = new FLightMapData1D(Data.Num());
	ConvertedLightMap->LightGuids = LightGuids;
	for (INT SampleIndex = 0; SampleIndex < Data.Num(); SampleIndex++)
	{
		(*ConvertedLightMap)(SampleIndex) = Data(SampleIndex).ConvertToLightSample();
	}
	return ConvertedLightMap;
}

FLightMapData2D* FGatheredLightMapData2D::ConvertToLightmap2D() const
{
	FLightMapData2D* ConvertedLightMap = new FLightMapData2D(SizeX, SizeY);
	ConvertedLightMap->LightGuids = LightGuids;
	for (INT SampleIndex = 0; SampleIndex < Data.Num(); SampleIndex++)
	{
		(*ConvertedLightMap)(SampleIndex, 0) = Data(SampleIndex).ConvertToLightSample();
	}
	return ConvertedLightMap;
}

UBOOL FStaticLightingSystem::CalculatePointShadowing(
	const FStaticLightingMapping* Mapping,
	const FVector& WorldSurfacePoint,
	ULightComponent* Light,
	FCoherentRayCache& CoherentRayCache
	) const
{
	const UBOOL bIsSkyLight = Light->IsA(USkyLightComponent::StaticClass());
	if(bIsSkyLight)
	{
		// Sky lighting is computed in CalculatePointAreaLighting, so we simply treat sky lights as shadowed here.
		return TRUE;
	}
	else
	{
		// Treat points which the light doesn't affect as shadowed to avoid the costly ray check.
		if(!Light->AffectsBounds(FBoxSphereBounds(WorldSurfacePoint,FVector(0,0,0),0)))
		{
			return TRUE;
		}

		// Check for visibility between the point and the light.
		UBOOL bIsShadowed = FALSE;
		if(Light->CastShadows && Light->CastStaticShadows)
		{
			const FVector4 LightPosition = Light->GetPosition();

			// Construct a line segment between the light and the surface point.
			const FVector LightVector = (FVector)LightPosition - WorldSurfacePoint * LightPosition.W;
			const FLightRay LightRay(
				WorldSurfacePoint + LightVector.SafeNormal() * SHADOW_VISIBILITY_DISTANCE_BIAS,
				WorldSurfacePoint + LightVector,
				Mapping,
				Light
				);

			// Check the line segment for intersection with the static lighting meshes.
			bIsShadowed = AggregateMesh.IntersectLightRay(LightRay,FALSE,CoherentRayCache).bIntersects;
		}

		return bIsShadowed;
	}
}

FGatheredLightSample FStaticLightingSystem::CalculatePointLighting(
	const FStaticLightingMapping* Mapping,
	const FStaticLightingVertex& Vertex,
	ULightComponent* Light
	) const
{
	const UBOOL bIsSkyLight = Light->IsA(USkyLightComponent::StaticClass());
	if(!bIsSkyLight)
	{
	    // Calculate the direction from the vertex to the light.
	    const FVector4 LightPosition = Light->GetPosition();
	    const FVector WorldLightVector = (FVector)LightPosition - Vertex.WorldPosition * LightPosition.W;
    
	    // Transform the light vector to tangent space.
	    const FVector TangentLightVector = 
		    FVector(
			    WorldLightVector | Vertex.WorldTangentX,
			    WorldLightVector | Vertex.WorldTangentY,
			    WorldLightVector | Vertex.WorldTangentZ
			    ).SafeNormal();

		// Compute the incident lighting of the light on the vertex.
		const FLinearColor LightIntensity = Light->GetDirectIntensity(Vertex.WorldPosition);

		// Compute the light-map sample for the front-face of the vertex.
		const FGatheredLightSample FrontFaceSample = FGatheredLightSample::PointLight(LightIntensity,TangentLightVector);

		if(FALSE)//(Mapping->Mesh->bTwoSidedMaterial)
		{
			// Compute the light-map sample for the back-face of the vertex.
			const FGatheredLightSample BackFaceSample = FGatheredLightSample::PointLight(LightIntensity,-TangentLightVector);

			// If the vertex has a two-sided material, add both front-face and back-face samples.
			FGatheredLightSample TwoSidedSample;
			TwoSidedSample.AddWeighted(FrontFaceSample,1.0f);
			TwoSidedSample.AddWeighted(BackFaceSample,1.0f);

			return TwoSidedSample;
		}
		else
		{
			return FrontFaceSample;
		}
	}

	return FGatheredLightSample();
}

void FLightingCache::AddRecord(const FRecord& Record)
{
	Octree.AddElement(Record);
}

UBOOL FLightingCache::InterpolateLighting(const FStaticLightingVertex& Vertex,FGatheredLightSample& OutLighting) const
{
	FGatheredLightSample AccumulatedLighting;
	FLOAT TotalWeight = 0.0f;

	// Iterate over the octree nodes containing the query point.
	for(LightingOctreeType::TConstElementBoxIterator<> OctreeIt(
			Octree,
			FBoxCenterAndExtent(Vertex.WorldPosition,FVector(0,0,0))
			);
		OctreeIt.HasPendingElements();
		OctreeIt.Advance())
	{
		const FRecord& LightingRecord = OctreeIt.GetCurrentElement();

		// Check whether the query point is farther than the record's intersection distance for the direction to the query point.
		const FLOAT DistanceSquared = (LightingRecord.Vertex.WorldPosition - Vertex.WorldPosition).SizeSquared();
		if(DistanceSquared > Square(LightingRecord.Radius))
		{
			continue;
		}

		// Don't use an lighting record which was computed for a significantly different normal.
		const FLOAT NormalDot = (LightingRecord.Vertex.WorldTangentZ | Vertex.WorldTangentZ);
		if(NormalDot < LIGHTING_CACHE_NORMAL_THRESHOLD)
		{
			continue;
		}

		// TODO: Rotate the record's lighting into this vertex's tangent basis.
		if(	(LightingRecord.Vertex.WorldTangentX | Vertex.WorldTangentX) < LIGHTING_CACHE_NORMAL_THRESHOLD ||
			(LightingRecord.Vertex.WorldTangentY | Vertex.WorldTangentY) < LIGHTING_CACHE_NORMAL_THRESHOLD)
		{
			continue;
		}

		// Don't use an lighting record if it's in front of the query point.
		const FVector RecordToVertexVector = Vertex.WorldPosition - LightingRecord.Vertex.WorldPosition;
		const FLOAT PlaneDistance = LightingRecord.Vertex.WorldTangentZ | RecordToVertexVector.SafeNormal();
		if(PlaneDistance < -0.05f)
		{
			continue;
		}

		// Compute the lighting record's weight.
		const FLOAT RecordError = appSqrt(DistanceSquared) / (LightingRecord.Radius * NormalDot);

		// Don't use a record if it would have an inconsequential weight.
		if(RecordError >= 1.0f)
		{
			continue;
		}

		// Compute the record's weight, and add it to the accumulated result.
		const FLOAT RecordWeight = Square(1.0f - RecordError);
		AccumulatedLighting.AddWeighted(LightingRecord.Lighting,RecordWeight);
		TotalWeight += RecordWeight;
	}

	if(TotalWeight > DELTA)
	{
		// Normalize the accumulated lighting and return success.
		OutLighting.AddWeighted(AccumulatedLighting,1.0f / TotalWeight);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

FLightingCache::FLightingCache(const FBox& InBoundingBox):
	Octree(InBoundingBox.GetCenter(),InBoundingBox.GetExtent().GetMax())
{}

FGatheredLightSample FStaticLightingSystem::CalculatePointAreaLighting(
	const FStaticLightingMapping* Mapping,
	const FStaticLightingVertex& Vertex,
	FCoherentRayCache& CoherentRayCache,
	FRandomStream& RandomStream,
	UBOOL bDebugCurrentTexel
	)
{
	FGatheredLightSample AreaLighting;

	// Transform the up vector to tangent space.
	const FVector TangentUpVector(
		Vertex.WorldTangentX.Z,
		Vertex.WorldTangentY.Z,
		Vertex.WorldTangentZ.Z
		);

	// Compute the total unshadowed upper and lower hemisphere sky light.
	FLinearColor UpperHemisphereIntensity = FLinearColor::Black;
	FLinearColor LowerHemisphereIntensity = FLinearColor::Black;
	FLinearColor ShadowedUpperHemisphereIntensity = FLinearColor::Black;
	FLinearColor ShadowedLowerHemisphereIntensity = FLinearColor::Black;
	for(INT LightIndex = 0;LightIndex < Mapping->Mesh->RelevantLights.Num();LightIndex++)
	{
		const USkyLightComponent* SkyLight = Cast<USkyLightComponent>(Mapping->Mesh->RelevantLights(LightIndex));
		if(SkyLight)
		{
			if(SkyLight->CastShadows && SkyLight->CastStaticShadows)
			{
				ShadowedUpperHemisphereIntensity += FLinearColor(SkyLight->LightColor) * SkyLight->Brightness;
				ShadowedLowerHemisphereIntensity += FLinearColor(SkyLight->LowerColor) * SkyLight->LowerBrightness;
			}
			else
			{
				UpperHemisphereIntensity += FLinearColor(SkyLight->LightColor) * SkyLight->Brightness;
				LowerHemisphereIntensity += FLinearColor(SkyLight->LowerColor) * SkyLight->LowerBrightness;
			}
		}
	}

	// Determine whether there is shadowed sky light in each hemisphere.
	const UBOOL bHasShadowedUpperSkyLight = (ShadowedUpperHemisphereIntensity != FLinearColor::Black);
	const UBOOL bHasShadowedLowerSkyLight = (ShadowedLowerHemisphereIntensity != FLinearColor::Black);
	if(bHasShadowedUpperSkyLight || bHasShadowedLowerSkyLight)
	{
		// If there are enough relevant samples in the area lighting cache, interpolate them to estimate this vertex's area lighting.
		if(!CoherentRayCache.AreaLightingCache.InterpolateLighting(Vertex,AreaLighting))
		{
			// Create a table of random directions to sample sky shadowing for.
			static const INT NumRandomDirections = 128;
			static UBOOL bGeneratedDirections = FALSE;
			static FVector RandomDirections[NumRandomDirections];
			if(!bGeneratedDirections)
			{
				for(INT DirectionIndex = 0;DirectionIndex < NumRandomDirections;DirectionIndex++)
				{
					RandomDirections[DirectionIndex] = RandomStream.GetUnitVector();
				}

				bGeneratedDirections = TRUE;
			}

			// Use monte carlo integration to compute the shadowed sky lighting.
			FLOAT InverseDistanceSum = 0.0f;
			UINT NumRays = 0;
			for(INT SampleIndex = 0;SampleIndex < NumRandomDirections;SampleIndex++)
			{
				const FVector WorldDirection = RandomDirections[SampleIndex];
				const FVector TangentDirection(
					Vertex.WorldTangentX | WorldDirection,
					Vertex.WorldTangentY | WorldDirection,
					Vertex.WorldTangentZ | WorldDirection
					);

				if(TangentDirection.Z > 0.0f)
				{
					const UBOOL bRayIsInUpperHemisphere = WorldDirection.Z > 0.0f;
					if( (bRayIsInUpperHemisphere && bHasShadowedUpperSkyLight) ||
						(!bRayIsInUpperHemisphere && bHasShadowedLowerSkyLight))
					{
						// Construct a line segment between the light and the surface point.
						const FLightRay LightRay(
							Vertex.WorldPosition + WorldDirection * SHADOW_VISIBILITY_DISTANCE_BIAS,
							Vertex.WorldPosition + WorldDirection * HALF_WORLD_MAX,
							Mapping,
							NULL
							);

						// Check the line segment for intersection with the static lighting meshes.
						const FLightRayIntersection RayIntersection = AggregateMesh.IntersectLightRay(LightRay,TRUE,CoherentRayCache);
						if(!RayIntersection.bIntersects)
						{
							// Add this sample's contribution to the vertex's sky lighting.
							const FLinearColor LightIntensity = bRayIsInUpperHemisphere ? ShadowedUpperHemisphereIntensity : ShadowedLowerHemisphereIntensity;
							AreaLighting.AddWeighted(FGatheredLightSample::PointLight(LightIntensity,TangentDirection),2.0f / NumRandomDirections);
						}
						else
						{
							InverseDistanceSum += 1.0f / (Vertex.WorldPosition - RayIntersection.IntersectionVertex.WorldPosition).Size();
						}

						NumRays++;
					}
				}
			}

			// Compute this lighting record's radius based on the harmonic mean of the intersection distance.
			const FLOAT IntersectionDistanceHarmonicMean = (FLOAT)NumRays / InverseDistanceSum;
			const FLOAT RecordRadius = Clamp(IntersectionDistanceHarmonicMean * 0.02f,64.0f,512.0f);

			// Add the vertex's area lighting to the area lighting cache.
			CoherentRayCache.AreaLightingCache.AddRecord(FLightingCache::FRecord(
				Vertex,
				RecordRadius,
				AreaLighting
				));
		}
	}

	// Add the unshadowed sky lighting.
	AreaLighting.AddWeighted(FGatheredLightSample::SkyLight(UpperHemisphereIntensity,LowerHemisphereIntensity,TangentUpVector),1.0f);

	return AreaLighting;
}

/**
 * Checks if a light is behind a triangle.
 * @param TrianglePoint - Any point on the triangle.
 * @param TriangleNormal - The (not necessarily normalized) triangle surface normal.
 * @param Light - The light to classify.
 * @return TRUE if the light is behind the triangle.
 */
UBOOL IsLightBehindSurface(const FVector& TrianglePoint,const FVector& TriangleNormal,const ULightComponent* Light)
{
	const UBOOL bIsSkyLight = Light->IsA(USkyLightComponent::StaticClass());
	if(!bIsSkyLight)
	{
		// Calculate the direction from the triangle to the light.
		const FVector4 LightPosition = Light->GetPosition();
		const FVector WorldLightVector = (FVector)LightPosition - TrianglePoint * LightPosition.W;

		// Check if the light is in front of the triangle.
		const FLOAT Dot = WorldLightVector | TriangleNormal;
		return Dot < 0.0f;
	}
	else
	{
		// Sky lights are always in front of a surface.
		return FALSE;
	}
}

/**
 * Culls lights that are behind a triangle.
 * @param bTwoSidedMaterial - TRUE if the triangle has a two-sided material.  If so, lights behind the surface are not culled.
 * @param TrianglePoint - Any point on the triangle.
 * @param TriangleNormal - The (not necessarily normalized) triangle surface normal.
 * @param Lights - The lights to cull.
 * @return A map from Lights index to a boolean which is TRUE if the light is in front of the triangle.
 */
TBitArray<> CullBackfacingLights(UBOOL bTwoSidedMaterial,const FVector& TrianglePoint,const FVector& TriangleNormal,const TArray<ULightComponent*>& Lights)
{
	if(!bTwoSidedMaterial)
	{
		TBitArray<> Result(FALSE,Lights.Num());
		for(INT LightIndex = 0;LightIndex < Lights.Num();LightIndex++)
		{
			Result(LightIndex) = !IsLightBehindSurface(TrianglePoint,TriangleNormal,Lights(LightIndex));
		}
		return Result;
	}
	else
	{
		return TBitArray<>(TRUE,Lights.Num());
	}
}

void FStaticLightingSystem::FStaticLightingThreadRunnable::CheckHealth() const
{
	if(bTerminatedByError)
	{
#if !CONSOLE
		GErrorHist[0] = 0;
#endif
		GIsCriticalError = FALSE;
		GError->Logf(TEXT("Static lighting thread exception:\r\n%s"),*ErrorMessage);
	}
}

DWORD FStaticLightingSystem::FStaticLightingThreadRunnable::Run()
{
#if _MSC_VER && !XBOX
	extern INT CreateMiniDump( LPEXCEPTION_POINTERS ExceptionInfo );
	if(!appIsDebuggerPresent())
	{
		__try
		{
			System->ThreadLoop(FALSE);
		}
		__except( CreateMiniDump( GetExceptionInformation() ) )
		{
			ErrorMessage = GErrorHist;

			// Use a memory barrier to ensure that the main thread sees the write to ErrorMessage before
			// the write to bTerminatedByError.
			appMemoryBarrier();

			bTerminatedByError = FALSE;
		}
	}
	else
#endif
	{
		System->ThreadLoop(FALSE);
	}

	return 0;
}

void FStaticLightingSystem::ThreadLoop(UBOOL bIsMainThread)
{
	UBOOL bIsDone = FALSE;
	while(!bIsDone && !bBuildCanceled)
	{
		// Atomically read and increment the next mapping index to process.
		INT MappingIndex = NextMappingToProcess.Increment() - 1;

		if(MappingIndex < Mappings.Num())
		{
			// Continue only if we need to process this mapping
			if(Mappings(MappingIndex)->bProcessMapping)
			{
				// If this is the main thread, update progress and apply completed static lighting.
				if(bIsMainThread)
				{
					// Update the progress bar.
					GWarn->StatusUpdatef(MappingIndex,Mappings.Num(),TEXT("Building static lighting"));

					// Apply completed static lighting.
					if (!GLightmassDebugOptions.bUseDeterministicLighting)
					{
						CompleteVertexMappingList.ApplyAndClear(Options.bUseLightmass, Options.bDumpBinaryResults);
						CompleteTextureMappingList.ApplyAndClear(Options.bUseLightmass, Options.bDumpBinaryResults);
					}

					// Check the health of all static lighting threads.
					for(INT ThreadIndex = 0;ThreadIndex < Threads.Num();ThreadIndex++)
					{
						Threads(ThreadIndex).CheckHealth();
					}

					// Check the for build cancellation.
					if(GEditor->GetMapBuildCancelled())
					{
						bBuildCanceled = TRUE;
					}
				}

				// Build the mapping's static lighting.
				if(Mappings(MappingIndex)->GetVertexMapping())
				{
					ProcessVertexMapping(Mappings(MappingIndex)->GetVertexMapping());
				}
				else if(Mappings(MappingIndex)->GetTextureMapping())
				{
					ProcessTextureMapping(Mappings(MappingIndex)->GetTextureMapping());
				}
			}
		}
		else
		{
			// Processing has begun for all mappings.
			bIsDone = TRUE;
		}
	}
}

void FStaticLightingSystem::ProcessVertexMapping(FStaticLightingVertexMapping* VertexMapping)
{
	// Process the vertex mapping.
	FStaticLightingVertexMappingProcessor Processor(VertexMapping,this);
	FLightMapData1D* LightMapData = Processor.Process();

	// Enqueue the static lighting for application in the main thread.
	TList<FVertexMappingStaticLightingData>* StaticLightingLink = new TList<FVertexMappingStaticLightingData>(FVertexMappingStaticLightingData(),NULL);
	StaticLightingLink->Element.Mapping = VertexMapping;
	StaticLightingLink->Element.LightMapData = LightMapData;
	StaticLightingLink->Element.ShadowMaps = Processor.ShadowMapData;
	CompleteVertexMappingList.AddElement(StaticLightingLink);
}
