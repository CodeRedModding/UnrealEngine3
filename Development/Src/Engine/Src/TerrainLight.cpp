/*=============================================================================
TerrainLight.cpp: Terrain static lighting
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnTerrain.h"
#include "UnTerrainRender.h"
#include "TerrainLight.h"

/** Represents terrain to the static lighting system. */
/** Initialization constructor. */
FTerrainComponentStaticLighting::FTerrainComponentStaticLighting(
	UTerrainComponent* InPrimitive,
	const TArray<FIntPoint>& InQuadIndexToCoordinatesMap,
	const TArray<ULightComponent*>& InRelevantLights,
	UBOOL bPerformFullQualityBuild,
	INT InPatchExpandX, INT InPatchExpandY,
	INT InSizeX, INT InSizeY
	):
FStaticLightingMesh(
					InQuadIndexToCoordinatesMap.Num() * 2,
					InQuadIndexToCoordinatesMap.Num() * 2,
					InQuadIndexToCoordinatesMap.Num() * 4,
					InQuadIndexToCoordinatesMap.Num() * 4,
					0,
					InPrimitive->CastShadow | InPrimitive->bCastHiddenShadow,
					InPrimitive->bSelfShadowOnly,
					FALSE,
					InRelevantLights,
					InPrimitive,
					InPrimitive->Bounds.GetBox(),
					InPrimitive->GetTerrain()->GetLightingGuid()
					),
					FStaticLightingTextureMapping(
					this,
					InPrimitive,
					InSizeX,
					InSizeY,
					1,
					InPrimitive->bForceDirectLightMap,
					InPrimitive->GetTerrain()->bBilinearFilterLightmapGeneration
					),
					Terrain(InPrimitive->GetTerrain()),
					Primitive(InPrimitive),
					NumQuadsX(InPrimitive->TrueSectionSizeX),
					NumQuadsY(InPrimitive->TrueSectionSizeY),
					ExpandQuadsX(InPatchExpandX), 
					ExpandQuadsY(InPatchExpandY),
					QuadIndexToCoordinatesMap(InQuadIndexToCoordinatesMap),
					bNeedsNormalFlip(Primitive->LocalToWorld.Determinant() < 0.f)
{
	//Dump();
}

FStaticLightingVertex FTerrainComponentStaticLighting::GetVertex(INT X,INT Y) const
{
	FStaticLightingVertex Result;

	const INT PatchX = Primitive->SectionBaseX + X;
	const INT PatchY = Primitive->SectionBaseY + Y;

	FVector WorldTangentX;
	FVector WorldTangentY;
	FVector WorldTangentZ;

	FLOAT Z = (FLOAT)Terrain->Height(PatchX, PatchY);
	const FLOAT HeightNegX = (FLOAT)Terrain->Height(PatchX - 1,PatchY);
	const FLOAT HeightPosX = (FLOAT)Terrain->Height(PatchX + 1,PatchY);
	const FLOAT HeightNegY = (FLOAT)Terrain->Height(PatchX,PatchY - 1);
	const FLOAT HeightPosY = (FLOAT)Terrain->Height(PatchX,PatchY + 1);
	const FLOAT SampleDerivX = (HeightPosX - HeightNegX) / (FLOAT)1.0f / 2.0f;
	const FLOAT SampleDerivY = (HeightPosY - HeightNegY) / (FLOAT)1.0f / 2.0f;
	FVector TempWorldTangentX = FVector(1,0,(SampleDerivX * TERRAIN_ZSCALE));
	FVector TempWorldTangentY = FVector(0,1,(SampleDerivY * TERRAIN_ZSCALE));

	WorldTangentX = Primitive->LocalToWorld.TransformNormal(TempWorldTangentX).SafeNormal();
	WorldTangentY = Primitive->LocalToWorld.TransformNormal(TempWorldTangentY).SafeNormal();
	WorldTangentZ = (WorldTangentX ^ WorldTangentY).SafeNormal();
	if( bNeedsNormalFlip )
	{
		WorldTangentZ *= -1.f;
	}

	Result.WorldPosition = Primitive->LocalToWorld.TransformFVector(FVector(X,Y,(-32768.0f + Z) * TERRAIN_ZSCALE));

	Result.WorldTangentX = WorldTangentX;
	Result.WorldTangentY = WorldTangentY;
	Result.WorldTangentZ = WorldTangentZ;

	check((X + ExpandQuadsX) >= 0);
	check((Y + ExpandQuadsY) >= 0);

	Result.TextureCoordinates[0] = FVector2D(PatchX,PatchY);
	Result.TextureCoordinates[1].X = ((X + ExpandQuadsX) * Terrain->StaticLightingResolution + 0.5f) / (FLOAT)SizeX;
	Result.TextureCoordinates[1].Y = ((Y + ExpandQuadsY) * Terrain->StaticLightingResolution + 0.5f) / (FLOAT)SizeY;

	return Result;
}

// FStaticLightingMesh interface.
void FTerrainComponentStaticLighting::GetTriangle(INT TriangleIndex,FStaticLightingVertex& OutV0,FStaticLightingVertex& OutV1,FStaticLightingVertex& OutV2) const
{
	const INT QuadIndex = TriangleIndex / 2;
	const FIntPoint& QuadCoordinates = QuadIndexToCoordinatesMap(QuadIndex);

	if (Terrain->IsTerrainQuadFlipped(QuadCoordinates.X, QuadCoordinates.Y) == FALSE)
	{
		if(TriangleIndex & 1)
		{
			OutV0 = GetVertex(QuadCoordinates.X + 0,QuadCoordinates.Y + 0);
			OutV1 = GetVertex(QuadCoordinates.X + 0,QuadCoordinates.Y + 1);
			OutV2 = GetVertex(QuadCoordinates.X + 1,QuadCoordinates.Y + 1);
		}
		else
		{
			OutV0 = GetVertex(QuadCoordinates.X + 0,QuadCoordinates.Y + 0);
			OutV1 = GetVertex(QuadCoordinates.X + 1,QuadCoordinates.Y + 1);
			OutV2 = GetVertex(QuadCoordinates.X + 1,QuadCoordinates.Y + 0);
		}
	}
	else
	{
		if(TriangleIndex & 1)
		{
			OutV0 = GetVertex(QuadCoordinates.X + 0,QuadCoordinates.Y + 0);
			OutV1 = GetVertex(QuadCoordinates.X + 0,QuadCoordinates.Y + 1);
			OutV2 = GetVertex(QuadCoordinates.X + 1,QuadCoordinates.Y + 0);
		}
		else
		{
			OutV0 = GetVertex(QuadCoordinates.X + 1,QuadCoordinates.Y + 0);
			OutV1 = GetVertex(QuadCoordinates.X + 0,QuadCoordinates.Y + 1);
			OutV2 = GetVertex(QuadCoordinates.X + 1,QuadCoordinates.Y + 1);
		}
	}
}
void FTerrainComponentStaticLighting::GetTriangleIndices(INT TriangleIndex,INT& OutI0,INT& OutI1,INT& OutI2) const
{
	const INT QuadIndex = TriangleIndex / 2;
	const FIntPoint& QuadCoordinates = QuadIndexToCoordinatesMap(QuadIndex);

	if (Terrain->IsTerrainQuadFlipped(QuadCoordinates.X, QuadCoordinates.Y) == FALSE)
	{
		if(TriangleIndex & 1)
		{
			OutI0 = QuadIndex * 4 + 0;
			OutI1 = QuadIndex * 4 + 2;
			OutI2 = QuadIndex * 4 + 3;
		}
		else
		{
			OutI0 = QuadIndex * 4 + 0;
			OutI1 = QuadIndex * 4 + 3;
			OutI2 = QuadIndex * 4 + 1;
		}
	}
	else
	{
		if(TriangleIndex & 1)
		{
			OutI0 = QuadIndex * 4 + 0;
			OutI1 = QuadIndex * 4 + 2;
			OutI2 = QuadIndex * 4 + 1;
		}
		else
		{
			OutI0 = QuadIndex * 4 + 1;
			OutI1 = QuadIndex * 4 + 2;
			OutI2 = QuadIndex * 4 + 3;
		}
	}
}
FLightRayIntersection FTerrainComponentStaticLighting::IntersectLightRay(const FVector& Start,const FVector& End,UBOOL bFindNearestIntersection) const
{
	// Intersect the light ray with the terrain component.
	FCheckResult Result(1.0f);
	const UBOOL bIntersects = !Primitive->LineCheck(Result,End,Start,FVector(0,0,0),TRACE_ShadowCast | (!bFindNearestIntersection ? TRACE_StopAtAnyHit : 0));

	// Setup a vertex to represent the intersection.
	FStaticLightingVertex IntersectionVertex;
	if(bIntersects)
	{
		IntersectionVertex.WorldPosition = Result.Location;
		IntersectionVertex.WorldTangentZ = Result.Normal;
	}
	else
	{
		IntersectionVertex.WorldPosition.Set(0,0,0);
		IntersectionVertex.WorldTangentZ.Set(0,0,1);
	}
	return FLightRayIntersection(bIntersects,IntersectionVertex);
}

// FStaticLightingTextureMapping interface.
void FTerrainComponentStaticLighting::Apply(FLightMapData2D* LightMapData,const TMap<ULightComponent*,FShadowMapData2D*>& ShadowMapData, FQuantizedLightmapData* QuantizedData)
{
	if (QuantizedData)
	{
		Primitive->PreviewEnvironmentShadowing = QuantizedData->PreviewEnvironmentShadowing;
	}

	const UBOOL bHasNonZeroData = LightMapData != NULL || QuantizedData != NULL && QuantizedData->HasNonZeroData();
	if (bHasNonZeroData)
	{
		// Create a light-map for the primitive.
		Primitive->LightMap = FLightMap2D::AllocateLightMap(
			Primitive,
			LightMapData,
			QuantizedData,
			NULL,
			Primitive->Bounds,
			LMPT_NoPadding,
			LMF_None
			);
	}
	else
	{
		Primitive->LightMap = NULL;
	}
	delete LightMapData;

	// Create the shadow-maps for the primitive.
	Primitive->ShadowMaps.Empty(ShadowMapData.Num());
	for(TMap<ULightComponent*,FShadowMapData2D*>::TConstIterator ShadowMapDataIt(ShadowMapData);ShadowMapDataIt;++ShadowMapDataIt)
	{
		Primitive->ShadowMaps.AddItem(
			new(Owner) UShadowMap2D(
			Primitive,
			*ShadowMapDataIt.Value(),
			ShadowMapDataIt.Key()->LightGuid,
			NULL,
			Primitive->Bounds,
			LMPT_NoPadding,
			SMF_None
			)
			);
		delete ShadowMapDataIt.Value();
	}

	// Build the list of statically irrelevant lights.
	// TODO: This should be stored per LOD.
	Primitive->IrrelevantLights.Empty();
	for(INT LightIndex = 0;LightIndex < RelevantLights.Num();LightIndex++)
	{
		const ULightComponent* Light = RelevantLights(LightIndex);

		// Check if the light is stored in the light-map.
		const UBOOL bIsInLightMap = Primitive->LightMap && Primitive->LightMap->LightGuids.ContainsItem(Light->LightmapGuid);

		// Check if the light is stored in the shadow-map.
		UBOOL bIsInShadowMap = FALSE;
		for(INT ShadowMapIndex = 0;ShadowMapIndex < Primitive->ShadowMaps.Num();ShadowMapIndex++)
		{
			if(Primitive->ShadowMaps(ShadowMapIndex)->GetLightGuid() == Light->LightGuid)
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

void FTerrainComponentStaticLighting::Dump() const
{
	debugf(TEXT("TERRAIN MAPPING DUMP:"));
	debugf(TEXT("\tTerrain     : %s"), Terrain ? *(Terrain->GetLightingGuid().String()) : TEXT("???"));
	//FStaticLightingTextureMapping TextureMapping;
	//
	debugf(TEXT("\tLocalToWorld: "));
	FMatrix LocalToWorld = Terrain->LocalToWorld();
	debugf(TEXT("\t\t%-8.5f,%-8.5f,%-8.5f,%-8.5f"), LocalToWorld.M[0][0], LocalToWorld.M[0][1], LocalToWorld.M[0][2], LocalToWorld.M[0][3]);
	debugf(TEXT("\t\t%-8.5f,%-8.5f,%-8.5f,%-8.5f"), LocalToWorld.M[1][0], LocalToWorld.M[1][1], LocalToWorld.M[1][2], LocalToWorld.M[1][3]);
	debugf(TEXT("\t\t%-8.5f,%-8.5f,%-8.5f,%-8.5f"), LocalToWorld.M[2][0], LocalToWorld.M[2][1], LocalToWorld.M[2][2], LocalToWorld.M[2][3]);
	debugf(TEXT("\t\t%-8.5f,%-8.5f,%-8.5f,%-8.5f"), LocalToWorld.M[3][0], LocalToWorld.M[3][1], LocalToWorld.M[3][2], LocalToWorld.M[3][3]);
	debugf(TEXT("\tBounds      : "));
	debugf(TEXT("\t\tOrigin      : %-8.5f,%-8.5f,%-8.5f"), Primitive->Bounds.Origin.X, Primitive->Bounds.Origin.Y, Primitive->Bounds.Origin.Z);
	debugf(TEXT("\t\tBoxExtent   : %-8.5f,%-8.5f,%-8.5f"), Primitive->Bounds.BoxExtent.X, Primitive->Bounds.BoxExtent.Y, Primitive->Bounds.BoxExtent.Z);
	debugf(TEXT("\t\tSphereRadius: %-8.5f"), Primitive->Bounds.SphereRadius);
	debugf(TEXT("\tPatchBounds   : %d"), Primitive->PatchBounds.Num());
	for (INT PatchIndex = 0; PatchIndex < Primitive->PatchBounds.Num(); PatchIndex++)
	{
		debugf(TEXT("\t\t%5d - %-8.5f,%-8.5f,%-8.5f"), 
			PatchIndex,
			Primitive->PatchBounds(PatchIndex).MinHeight,
			Primitive->PatchBounds(PatchIndex).MaxHeight,
			Primitive->PatchBounds(PatchIndex).MaxDisplacement
			);
	}
	//TArray<FIntPoint> QuadIndexToCoordinatesMap;
}

void UTerrainComponent::GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options)
{
	if(HasStaticShadowing() && bAcceptsLights)
	{
		// Determine the number of quads we need to 'expand' by to ensure smooth lighting across seams...
		ATerrain* Terrain = GetTerrain();
		check(Terrain);

		// Assuming DXT_1 compression at the moment...
		INT PixelPaddingX = GPixelFormats[PF_DXT1].BlockSizeX;
		INT PixelPaddingY = GPixelFormats[PF_DXT1].BlockSizeY;
		if (GAllowLightmapCompression == FALSE)
		{
			PixelPaddingX = GPixelFormats[PF_A8R8G8B8].BlockSizeX;
			PixelPaddingY = GPixelFormats[PF_A8R8G8B8].BlockSizeY;
		}

		INT PatchExpandCountX = (TERRAIN_PATCH_EXPAND_SCALAR * PixelPaddingX) / Terrain->StaticLightingResolution;
		INT PatchExpandCountY = (TERRAIN_PATCH_EXPAND_SCALAR * PixelPaddingY) / Terrain->StaticLightingResolution;

		PatchExpandCountX = Max<INT>(1, PatchExpandCountX);
		PatchExpandCountY = Max<INT>(1, PatchExpandCountY);

		// Index the quads in the component.
		TArray<FIntPoint> QuadIndexToCoordinateMap;
		for (INT QuadY = -PatchExpandCountY; QuadY < TrueSectionSizeY + PatchExpandCountY; QuadY++)
		{
			for (INT QuadX = -PatchExpandCountX; QuadX < TrueSectionSizeX + PatchExpandCountX; QuadX++)
			{
				if(Terrain->IsTerrainQuadVisible(SectionBaseX + QuadX,SectionBaseY + QuadY))
				{
					QuadIndexToCoordinateMap.AddItem(FIntPoint(QuadX,QuadY));
				}
			}
		}

		if ( QuadIndexToCoordinateMap.Num() > 0 )
		{
			const INT SizeX = (2 * PatchExpandCountX + TrueSectionSizeX) * GetTerrain()->StaticLightingResolution + 1;
			const INT SizeY = (2 * PatchExpandCountY + TrueSectionSizeY) * GetTerrain()->StaticLightingResolution + 1;
			// Create the object which represents the component to the static lighting system.
			FTerrainComponentStaticLighting* PrimitiveStaticLighting = new FTerrainComponentStaticLighting(
				this,QuadIndexToCoordinateMap,InRelevantLights,TRUE,
				PatchExpandCountX, PatchExpandCountY, SizeX, SizeY);
			OutPrimitiveInfo.Mappings.AddItem(PrimitiveStaticLighting);
			OutPrimitiveInfo.Meshes.AddItem(PrimitiveStaticLighting);
		}
	}
}
