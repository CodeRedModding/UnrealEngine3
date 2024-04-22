/*=============================================================================
	FluidSurfaceLight.h: Fluid surface lighting code
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef __FLUIDSURFACELIGHT_H__
#define __FLUIDSURFACELIGHT_H__

/** A texture mapping for fluid surfaces */
class FFluidSurfaceStaticLightingTextureMapping : public FStaticLightingTextureMapping
{
public:

	/** Initialization constructor. */
	FFluidSurfaceStaticLightingTextureMapping(UFluidSurfaceComponent* InPrimitive,FStaticLightingMesh* InMesh,INT InSizeX,INT InSizeY,INT InTextureCoordinateIndex,UBOOL bPerformFullQualityRebuild);

	// FStaticLightingTextureMapping interface
	virtual void Apply(FLightMapData2D* LightMapData,const TMap<ULightComponent*,FShadowMapData2D*>& ShadowMapData, FQuantizedLightmapData* QuantizedData);

#if WITH_EDITOR
	/** 
	 * Export static lighting mapping instance data to an exporter 
	 * @param Exporter - export interface to process static lighting data
	 **/
	virtual void ExportMapping(class FLightmassExporter* Exporter);
#endif	//WITH_EDITOR

	virtual FString GetDescription() const
	{
		return FString(TEXT("FluidMapping"));
	}
private:

	/** The primitive this mapping represents. */
	UFluidSurfaceComponent* const Primitive;
};

/** Represents the fluid surface mesh to the static lighting system. */
class FFluidSurfaceStaticLightingMesh : public FStaticLightingMesh
{
public:

	/** Initialization constructor. */
	FFluidSurfaceStaticLightingMesh(const UFluidSurfaceComponent* InComponent,const TArray<ULightComponent*>& InRelevantLights);

	// FStaticLightingMesh interface.

	virtual void GetTriangle(INT TriangleIndex,FStaticLightingVertex& OutV0,FStaticLightingVertex& OutV1,FStaticLightingVertex& OutV2) const;

	virtual void GetTriangleIndices(INT TriangleIndex,INT& OutI0,INT& OutI1,INT& OutI2) const;

	virtual FLightRayIntersection IntersectLightRay(const FVector& Start,const FVector& End,UBOOL bFindNearestIntersection) const;

#if WITH_EDITOR
	/** 
	 * Export static lighting mesh instance data to an exporter 
	 * @param Exporter - export interface to process static lighting data
	 **/
	virtual void ExportMeshInstance(class FLightmassExporter* Exporter) const;
#endif	//WITH_EDITOR

	/** The primitive component this mesh represents. */
	const UFluidSurfaceComponent* GetFluidSurfaceComponent() const	{	return Component;	}

private:

	/** The primitive component this mesh represents. */
	const UFluidSurfaceComponent* const Component;

	/** The inverse transpose of the primitive's local to world transform. */
	const FMatrix LocalToWorldInverseTranspose;

	/** The mesh data of the fluid surface, which is represented as a quad. */
	FVector QuadCorners[4];
	FVector2D QuadUVCorners[4];
	INT QuadIndices[6];

	friend class FLightmassExporter;
};

#endif	//__FLUIDSURFACELIGHT_H__
