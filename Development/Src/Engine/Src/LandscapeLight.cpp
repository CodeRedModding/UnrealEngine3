/*=============================================================================
LandscapeLight.cpp: Static lighting for LandscapeComponents
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnTerrain.h"
#include "LandscapeLight.h"
#include "LandscapeRender.h"
#include "LandscapeDataAccess.h"
#include "ScenePrivate.h"

#if WITH_EDITOR

#define LANDSCAPE_LIGHTMAP_UV_INDEX 1

/** A texture mapping for landscapes */
/** Initialization constructor. */
FLandscapeStaticLightingTextureMapping::FLandscapeStaticLightingTextureMapping(
	ULandscapeComponent* InComponent,FStaticLightingMesh* InMesh,INT InLightMapWidth,INT InLightMapHeight,UBOOL bPerformFullQualityRebuild) :
FStaticLightingTextureMapping(
							  InMesh,
							  InComponent,
							  InLightMapWidth,
							  InLightMapHeight,
							  LANDSCAPE_LIGHTMAP_UV_INDEX,
							  InComponent->bForceDirectLightMap
							  ),
							  LandscapeComponent(InComponent)
{
}

void FLandscapeStaticLightingTextureMapping::Apply(FLightMapData2D* LightMapData,const TMap<ULightComponent*,FShadowMapData2D*>& ShadowMapData, FQuantizedLightmapData* QuantizedData)
{
	// Determine the material to use for grouping the light-maps and shadow-maps.
	UMaterialInterface* const Material = LandscapeComponent->MaterialInstance;

	if (QuantizedData)
	{
		LandscapeComponent->PreviewEnvironmentShadowing = QuantizedData->PreviewEnvironmentShadowing;
	}

	//ELightMapPaddingType PaddingType = GAllowLightmapPadding ? LMPT_NormalPadding : LMPT_NoPadding;
	ELightMapPaddingType PaddingType = LMPT_NoPadding;

	const UBOOL bHasNonZeroData = LightMapData != NULL || QuantizedData != NULL && QuantizedData->HasNonZeroData();
	if (bHasNonZeroData)
	{
		// Create a light-map for the primitive.
		LandscapeComponent->LightMap = FLightMap2D::AllocateLightMap(
			LandscapeComponent,
			LightMapData,
			QuantizedData,
			Material,
			LandscapeComponent->Bounds,
			LMPT_NoPadding,
			LMF_Streamed
			);
	}
	else
	{
		LandscapeComponent->LightMap = NULL;
	}

	delete LightMapData;

	// Create the shadow-maps for the primitive.
	LandscapeComponent->ShadowMaps.Empty(ShadowMapData.Num());
	for(TMap<ULightComponent*,FShadowMapData2D*>::TConstIterator ShadowMapDataIt(ShadowMapData);ShadowMapDataIt;++ShadowMapDataIt)
	{
		ShadowMapDataIt.Value()->Shrink();
		LandscapeComponent->ShadowMaps.AddItem(
			new(Owner) UShadowMap2D(
			LandscapeComponent,
			*ShadowMapDataIt.Value(),
			ShadowMapDataIt.Key()->LightGuid,
			Material,
			LandscapeComponent->Bounds,
			LMPT_NoPadding,
			SMF_Streamed
			)
			);
		delete ShadowMapDataIt.Value();
	}

	// Build the list of statically irrelevant lights.
	// TODO: This should be stored per LOD.
	LandscapeComponent->IrrelevantLights.Empty();
	for(INT LightIndex = 0;LightIndex < Mesh->RelevantLights.Num();LightIndex++)
	{
		const ULightComponent* Light = Mesh->RelevantLights(LightIndex);

		// Check if the light is stored in the light-map.
		const UBOOL bIsInLightMap = LandscapeComponent->LightMap && LandscapeComponent->LightMap->LightGuids.ContainsItem(Light->LightmapGuid);

		// Check if the light is stored in the shadow-map.
		UBOOL bIsInShadowMap = FALSE;
		for(INT ShadowMapIndex = 0;ShadowMapIndex < LandscapeComponent->ShadowMaps.Num();ShadowMapIndex++)
		{
			if(LandscapeComponent->ShadowMaps(ShadowMapIndex)->GetLightGuid() == Light->LightGuid)
			{
				bIsInShadowMap = TRUE;
				break;
			}
		}

		// Add the light to the statically irrelevant light list if it is in the potentially relevant light list, but didn't contribute to the light-map or a shadow-map.
		if(!bIsInLightMap && !bIsInShadowMap)
		{	
			LandscapeComponent->IrrelevantLights.AddUniqueItem(Light->LightGuid);
		}
	}

	// Mark the primitive's package as dirty.
	LandscapeComponent->MarkPackageDirty();
}

/** Initialization constructor. */
FLandscapeStaticLightingMesh::FLandscapeStaticLightingMesh(ULandscapeComponent* InComponent, const TArray<ULightComponent*>& InRelevantLights, INT InExpandQuadsX, INT InExpandQuadsY, INT InSizeX, INT InSizeY, FLOAT InLIghtMapRatio)
:	FStaticLightingMesh(
					Square(InComponent->ComponentSizeQuads + 2*InExpandQuadsX) * 2,
					Square(InComponent->ComponentSizeQuads + 2*InExpandQuadsX) * 2,
					Square(InComponent->ComponentSizeQuads + 2*InExpandQuadsX + 1),
					Square(InComponent->ComponentSizeQuads + 2*InExpandQuadsX + 1),
					0,
					InComponent->CastShadow | InComponent->bCastHiddenShadow,
					InComponent->bSelfShadowOnly,
					FALSE,
					InRelevantLights,
					InComponent,
					InComponent->CachedBoxSphereBounds.GetBox(),
					InComponent->GetLightingGuid()
					)
,	LandscapeComponent(InComponent)
,	StaticLightingResolution(InComponent->GetLandscapeProxy()->StaticLightingResolution)
{
	DataInterface = new FLandscapeComponentDataInterface(InComponent);
	DataInterface->ExpandQuadsX = InExpandQuadsX;
	DataInterface->ExpandQuadsY = InExpandQuadsY;
	DataInterface->SizeX = InSizeX;
	DataInterface->SizeY = InSizeY;
	DataInterface->LightMapRatio = InLIghtMapRatio;

	// init HeightData...
	DataInterface->GetRawHeightmapData(HeightData);
	DataInterface->UnlockRawHeightData();
}

FLandscapeStaticLightingMesh::~FLandscapeStaticLightingMesh()
{
	check(DataInterface);
	delete DataInterface;
}

/** Fills in the static lighting vertex data for the Landscape vertex. */
void FLandscapeStaticLightingMesh::GetStaticLightingVertex(INT VertexIndex, FStaticLightingVertex& OutVertex) const
{
	INT X, Y;
	DataInterface->VertexIndexToXYExpanded(VertexIndex, X, Y);
	INT LocalX = X-DataInterface->ExpandQuadsX;
	INT LocalY = Y-DataInterface->ExpandQuadsY;
	INT NumVertices = LandscapeComponent->ComponentSizeQuads + 2*DataInterface->ExpandQuadsX + 1;

	const FColor* Data = &HeightData(X + Y * NumVertices);

	OutVertex.WorldTangentZ.X = 2.f / 255.f * (FLOAT)Data->B - 1.f;
	OutVertex.WorldTangentZ.Y = 2.f / 255.f * (FLOAT)Data->A - 1.f;
	OutVertex.WorldTangentZ.Z = appSqrt(1.f - (Square(OutVertex.WorldTangentZ.X)+Square(OutVertex.WorldTangentZ.Y)));
	OutVertex.WorldTangentX = FVector4(OutVertex.WorldTangentZ.Z, 0.f, -OutVertex.WorldTangentZ.X);
	OutVertex.WorldTangentY = OutVertex.WorldTangentZ ^ OutVertex.WorldTangentX;

	// Assume there is no rotation, so we don't need to do any LocalToWorld.
	WORD Height = (Data->R << 8) + Data->G;

	OutVertex.WorldPosition = LandscapeComponent->LocalToWorld.TransformFVector( FVector4( LocalX, LocalY, LandscapeDataAccess::GetLocalHeight(Height)) );

	FLOAT UVFactor = DataInterface->LightMapRatio / (FLOAT)(NumVertices * StaticLightingResolution);

	OutVertex.TextureCoordinates[0] = FVector2D((FLOAT)X, (FLOAT)Y); 
	OutVertex.TextureCoordinates[LANDSCAPE_LIGHTMAP_UV_INDEX].X = (X * StaticLightingResolution) * UVFactor;
	OutVertex.TextureCoordinates[LANDSCAPE_LIGHTMAP_UV_INDEX].Y = (Y * StaticLightingResolution) * UVFactor;
}

void FLandscapeStaticLightingMesh::GetTriangle(INT TriangleIndex,FStaticLightingVertex& OutV0,FStaticLightingVertex& OutV1,FStaticLightingVertex& OutV2) const
{
	INT I0, I1, I2;
	GetTriangleIndices(TriangleIndex,I0, I1, I2);
	GetStaticLightingVertex(I0,OutV0);
	GetStaticLightingVertex(I1,OutV1);
	GetStaticLightingVertex(I2,OutV2);
}

void FLandscapeStaticLightingMesh::GetTriangleIndices(INT TriangleIndex,INT& OutI0,INT& OutI1,INT& OutI2) const
{
	INT QuadIndex = TriangleIndex >> 1;
	INT QuadTriIndex = TriangleIndex & 1;

	INT NumVertices = LandscapeComponent->ComponentSizeQuads + 2*DataInterface->ExpandQuadsX + 1;
	INT QuadX = QuadIndex % (NumVertices - 1);
	INT QuadY = QuadIndex / (NumVertices - 1);

	switch(QuadTriIndex)
	{
	case 0:
		OutI0 = (QuadX + 0) + (QuadY + 0) * NumVertices;
		OutI1 = (QuadX + 1) + (QuadY + 1) * NumVertices;
		OutI2 = (QuadX + 1) + (QuadY + 0) * NumVertices;
		break;
	case 1:
		OutI0 = (QuadX + 0) + (QuadY + 0) * NumVertices;
		OutI1 = (QuadX + 0) + (QuadY + 1) * NumVertices;
		OutI2 = (QuadX + 1) + (QuadY + 1) * NumVertices;
		break;
	}
}

FLightRayIntersection FLandscapeStaticLightingMesh::IntersectLightRay(const FVector& Start,const FVector& End,UBOOL bFindNearestIntersection) const
{
	// Intersect the light ray with the terrain component.
	FCheckResult Result(1.0f);
	const UBOOL bIntersects = !LandscapeComponent->LineCheck(Result,End,Start,FVector(0,0,0),TRACE_ShadowCast | (!bFindNearestIntersection ? TRACE_StopAtAnyHit : 0));

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

//
// ULandscapeComponent Primitive lighting interface
//
void ULandscapeComponent::GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options)
{
	if (Options.bOnlyBuildSelected)
	{
		ULandscapeInfo* Info = GetLandscapeInfo(FALSE);
		if ( !(Info && (Info->SelectedComponents.Num() == 0 || Info->SelectedComponents.Contains(this))) )
		{
			return; // don't submit to Lightmass
		}
	}

	if( HasStaticShadowing() && bAcceptsLights )
	{
		FLOAT LightMapRes = GetLandscapeProxy()->StaticLightingResolution;
		INT PatchExpandCountX = 1;
		INT PatchExpandCountY = 1;
		INT DesiredSize = 1;

		FLOAT LightMapRatio = ::GetTerrainExpandPatchCount(LightMapRes, PatchExpandCountX, PatchExpandCountY, ComponentSizeQuads, (NumSubsections * (SubsectionSizeQuads+1)), DesiredSize);

		INT SizeX = DesiredSize; 
		INT SizeY = DesiredSize; 

		if (SizeX > 0 && SizeY > 0)
		{
			FLandscapeStaticLightingMesh* StaticLightingMesh = new FLandscapeStaticLightingMesh(this,InRelevantLights, PatchExpandCountX, PatchExpandCountY, SizeX, SizeY, LightMapRatio);
			OutPrimitiveInfo.Meshes.AddItem(StaticLightingMesh);
			// Create a static lighting texture mapping
			OutPrimitiveInfo.Mappings.AddItem(new FLandscapeStaticLightingTextureMapping(
				this,StaticLightingMesh,SizeX,SizeY,TRUE));
		}
	}
}

UBOOL ULandscapeComponent::GetLightMapResolution( INT& Width, INT& Height ) const
{
	// Assuming DXT_1 compression at the moment...
	FLOAT LightMapRes = GetLandscapeProxy()->StaticLightingResolution;
	INT PatchExpandCountX = 1;
	INT PatchExpandCountY = 1;
	INT DesiredSize = 1;

	FLOAT LightMapRatio = ::GetTerrainExpandPatchCount(LightMapRes, PatchExpandCountX, PatchExpandCountY, ComponentSizeQuads, (NumSubsections * (SubsectionSizeQuads+1)), DesiredSize);

	Width = DesiredSize;
	Height = DesiredSize;

	return FALSE;
}

void ULandscapeComponent::GetStaticTriangles(FPrimitiveTriangleDefinitionInterface* PTDI) const
{
	FLandscapeComponentDataInterface DataInterface((ULandscapeComponent*)this);
	for(INT QuadY = 0;QuadY < ComponentSizeQuads;QuadY++)
	{
		for(INT QuadX = 0;QuadX < ComponentSizeQuads;QuadX++)
		{
			const INT GlobalQuadX = SectionBaseX + QuadX;
			const INT GlobalQuadY = SectionBaseY + QuadY;

			// Setup the quad's vertices.
			FPrimitiveTriangleVertex Vertices[2][2];
			for(INT SubY = 0;SubY < 2;SubY++)
			{
				for(INT SubX = 0;SubX < 2;SubX++)
				{
					FPrimitiveTriangleVertex& DestVertex = Vertices[SubX][SubY];
					DataInterface.GetWorldPositionTangents(QuadX + SubX, QuadY + SubY, DestVertex.WorldPosition, DestVertex.WorldTangentX, DestVertex.WorldTangentY, DestVertex.WorldTangentZ);
				}
			}

			PTDI->DefineTriangle(Vertices[0][0],Vertices[1][1],Vertices[1][0]);
			PTDI->DefineTriangle(Vertices[0][0],Vertices[0][1],Vertices[1][1]);
		}
	}
}
#endif

void ULandscapeComponent::InvalidateLightingCache()
{
	const UBOOL bHasStaticLightingData = LightMap.GetReference() || ShadowMaps.Num() > 0;
	if (bHasStaticLightingData)
	{
		Modify();

		// Mark lighting as requiring a rebuilt.
		MarkLightingRequiringRebuild();

		// Detach the component from the scene for the duration of this function.
		FComponentReattachContext ReattachContext(this);
		FlushRenderingCommands();
		Super::InvalidateLightingCache();

		// Discard all cached lighting.
		IrrelevantLights.Empty();
		LightMap = NULL;
		ShadowMaps.Empty();
	}
}
