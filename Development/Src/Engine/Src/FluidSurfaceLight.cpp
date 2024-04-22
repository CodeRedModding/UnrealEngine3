/*=============================================================================
	FluidSurfaceLight.cpp: Fluid surface lighting code
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "EngineFluidClasses.h"
#include "FluidSurface.h"
#include "FluidSurfaceLight.h"

/** A texture mapping for fluid surfaces */
	/** Initialization constructor. */
FFluidSurfaceStaticLightingTextureMapping::FFluidSurfaceStaticLightingTextureMapping(
	UFluidSurfaceComponent* InPrimitive,FStaticLightingMesh* InMesh,INT InSizeX,INT InSizeY,INT InTextureCoordinateIndex,UBOOL bPerformFullQualityRebuild) :
		FStaticLightingTextureMapping(
			InMesh,
			InPrimitive,
			bPerformFullQualityRebuild ? InSizeX : InSizeX / 2,
			bPerformFullQualityRebuild ? InSizeY : InSizeY / 2,
			InTextureCoordinateIndex,
			InPrimitive->bForceDirectLightMap
		),
		Primitive(InPrimitive)
{
}

	  // FStaticLightingTextureMapping interface
void FFluidSurfaceStaticLightingTextureMapping::Apply(FLightMapData2D* LightMapData,const TMap<ULightComponent*,FShadowMapData2D*>& ShadowMapData, FQuantizedLightmapData* QuantizedData)
{
	// Determine the material to use for grouping the light-maps and shadow-maps.
	UMaterialInterface* const Material = Primitive->GetMaterial();

	if (QuantizedData)
	{
		Primitive->PreviewEnvironmentShadowing = QuantizedData->PreviewEnvironmentShadowing;
	}

	ELightMapPaddingType PaddingType = GAllowLightmapPadding ? LMPT_NormalPadding : LMPT_NoPadding;

	const UBOOL bHasNonZeroData = LightMapData != NULL || QuantizedData != NULL && QuantizedData->HasNonZeroData();
	if (bHasNonZeroData)
	{
		// Create a light-map for the primitive.
		Primitive->LightMap = FLightMap2D::AllocateLightMap(
			Primitive,
			LightMapData,
			QuantizedData,
			Material,
			Primitive->Bounds,
			PaddingType,
			LMF_None
			);
	}
	else
	{
		Primitive->LightMap = NULL;
	}

	// Create the shadow-maps for the primitive.
	Primitive->ShadowMaps.Empty(ShadowMapData.Num());
	for(TMap<ULightComponent*,FShadowMapData2D*>::TConstIterator ShadowMapDataIt(ShadowMapData);ShadowMapDataIt;++ShadowMapDataIt)
	{
		Primitive->ShadowMaps.AddItem(
			new(Owner) UShadowMap2D(
			Primitive,
			*ShadowMapDataIt.Value(),
			ShadowMapDataIt.Key()->LightGuid,
			Material,
			Primitive->Bounds,
			LMPT_NormalPadding,
			SMF_None
			)
			);
		delete ShadowMapDataIt.Value();
	}

	// Mark the primitive's package as dirty.
	Primitive->MarkPackageDirty();
}

/** Represents the fluid surface mesh to the static lighting system. */
static void GetStaticLightingVertex(
	const FVector* QuadCorners,
	const FVector2D* QuadUVCorners,
	UINT VertexIndex,
	const FMatrix& LocalToWorld,
	const FMatrix& LocalToWorldInverseTranspose,
	FStaticLightingVertex& OutVertex
	)
{
	OutVertex.WorldPosition = LocalToWorld.TransformFVector(QuadCorners[VertexIndex]);
	OutVertex.WorldTangentX = LocalToWorld.TransformNormal(FVector(1, 0, 0)).SafeNormal();
	OutVertex.WorldTangentY = LocalToWorld.TransformNormal(FVector(0, 1, 0)).SafeNormal();
	OutVertex.WorldTangentZ = LocalToWorldInverseTranspose.TransformNormal(FVector(0, 0, 1)).SafeNormal();

	for(UINT LightmapTextureCoordinateIndex = 0;LightmapTextureCoordinateIndex < 1;LightmapTextureCoordinateIndex++)
	{
		OutVertex.TextureCoordinates[LightmapTextureCoordinateIndex] = QuadUVCorners[VertexIndex];
	}
}

/** Initialization constructor. */
FFluidSurfaceStaticLightingMesh::FFluidSurfaceStaticLightingMesh(const UFluidSurfaceComponent* InComponent,const TArray<ULightComponent*>& InRelevantLights):
	FStaticLightingMesh(
		2,
		2,
		4,
		4,
		0,
		InComponent->CastShadow | InComponent->bCastHiddenShadow,
		InComponent->bSelfShadowOnly,
		FALSE,
		InRelevantLights,
		InComponent,
		InComponent->Bounds.GetBox(),
		FGuid(0, 0, 0, 0) // there is no source mesh, all per instance
		),
	Component(InComponent),
	LocalToWorldInverseTranspose(Component->LocalToWorld.Inverse().Transpose())
{
	QuadCorners[0] = FVector(-Component->FluidWidth * 0.5f, -Component->FluidHeight * 0.5f, 0.0f);
	QuadCorners[1] = FVector(Component->FluidWidth * 0.5f, -Component->FluidHeight * 0.5f, 0.0f);
	QuadCorners[2] = FVector(-Component->FluidWidth * 0.5f, Component->FluidHeight * 0.5f, 0.0f);
	QuadCorners[3] = FVector(Component->FluidWidth * 0.5f, Component->FluidHeight * 0.5f, 0.0f);

	QuadUVCorners[0] = FVector2D(0, 0);
	QuadUVCorners[1] = FVector2D(1, 0);
	QuadUVCorners[2] = FVector2D(0, 1);
	QuadUVCorners[3] = FVector2D(1, 1);

	QuadIndices[0] = 1;
	QuadIndices[1] = 0;
	QuadIndices[2] = 2;
	QuadIndices[3] = 1;
	QuadIndices[4] = 2;
	QuadIndices[5] = 3;
}

// FStaticLightingMesh interface.

void FFluidSurfaceStaticLightingMesh::GetTriangle(INT TriangleIndex,FStaticLightingVertex& OutV0,FStaticLightingVertex& OutV1,FStaticLightingVertex& OutV2) const
{
	GetStaticLightingVertex(QuadCorners,QuadUVCorners,QuadIndices[TriangleIndex * 3 + 0],Component->LocalToWorld,LocalToWorldInverseTranspose,OutV0);
	GetStaticLightingVertex(QuadCorners,QuadUVCorners,QuadIndices[TriangleIndex * 3 + 1],Component->LocalToWorld,LocalToWorldInverseTranspose,OutV1);
	GetStaticLightingVertex(QuadCorners,QuadUVCorners,QuadIndices[TriangleIndex * 3 + 2],Component->LocalToWorld,LocalToWorldInverseTranspose,OutV2);
}

void FFluidSurfaceStaticLightingMesh::GetTriangleIndices(INT TriangleIndex,INT& OutI0,INT& OutI1,INT& OutI2) const
{
	OutI0 = QuadIndices[TriangleIndex * 3 + 0];
	OutI1 = QuadIndices[TriangleIndex * 3 + 1];
	OutI2 = QuadIndices[TriangleIndex * 3 + 2];
}

FLightRayIntersection FFluidSurfaceStaticLightingMesh::IntersectLightRay(const FVector& Start,const FVector& End,UBOOL bFindNearestIntersection) const
{
	return FLightRayIntersection::None();
}

void UFluidSurfaceComponent::GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options)
{
	if( HasStaticShadowing() && bAcceptsLights )
	{
		INT		LightMapWidth	= 0;
		INT		LightMapHeight	= 0;
		GetLightMapResolution( LightMapWidth, LightMapHeight );
		if (LightMapWidth > 0 && LightMapHeight > 0)
		{
			FFluidSurfaceStaticLightingMesh* StaticLightingMesh = new FFluidSurfaceStaticLightingMesh(this, InRelevantLights);
			OutPrimitiveInfo.Meshes.AddItem(StaticLightingMesh);
			// Create a static lighting texture mapping
			OutPrimitiveInfo.Mappings.AddItem(new FFluidSurfaceStaticLightingTextureMapping(
				this,StaticLightingMesh,LightMapWidth,LightMapHeight,0,TRUE));
		}
	}
}

/** Gets the emissive boost for the primitive component. */
FLOAT UFluidSurfaceComponent::GetEmissiveBoost(INT ElementIndex) const
{
	return LightmassSettings.EmissiveBoost;
}

/** Gets the diffuse boost for the primitive component. */
FLOAT UFluidSurfaceComponent::GetDiffuseBoost(INT ElementIndex) const
{
	return LightmassSettings.DiffuseBoost;
}

/** Gets the specular boost for the primitive component. */
FLOAT UFluidSurfaceComponent::GetSpecularBoost(INT ElementIndex) const
{
	return LightmassSettings.SpecularBoost;
}
