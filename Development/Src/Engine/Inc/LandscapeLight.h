/*=============================================================================
LandscapeLight.h: Static lighting for LandscapeComponents
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _LANDSCAPELIGHT_H
#define _LANDSCAPELIGHT_H

/** A texture mapping for landscapes */
class FLandscapeStaticLightingTextureMapping : public FStaticLightingTextureMapping
{
public:
	/** Initialization constructor. */
	FLandscapeStaticLightingTextureMapping(ULandscapeComponent* InPrimitive,FStaticLightingMesh* InMesh,INT InLightMapWidth,INT InLightMapHeight,UBOOL bPerformFullQualityRebuild);

	// FStaticLightingTextureMapping interface
	virtual void Apply(FLightMapData2D* LightMapData,const TMap<ULightComponent*,FShadowMapData2D*>& ShadowMapData, FQuantizedLightmapData* QuantizedData);

#if WITH_EDITOR
	/** 
	* Export static lighting mapping instance data to an exporter 
	* @param Exporter - export interface to process static lighting data
	*/
	virtual void ExportMapping(class FLightmassExporter* Exporter);
#endif	//WITH_EDITOR

	virtual FString GetDescription() const
	{
		return FString(TEXT("LandscapeMapping"));
	}
private:

	/** The primitive this mapping represents. */
	ULandscapeComponent* const LandscapeComponent;
};



/** Represents the triangles of a Landscape component to the static lighting system. */
class FLandscapeStaticLightingMesh : public FStaticLightingMesh
{
public:
	// tors
	FLandscapeStaticLightingMesh(ULandscapeComponent* InComponent,const TArray<ULightComponent*>& InRelevantLights, INT InExpandQuadsX, INT InExpandQuadsY, INT InSizeX, INT InSizeY, FLOAT LightMapRatio);
	virtual ~FLandscapeStaticLightingMesh();

	// FStaticLightingMesh interface
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

	/** Fills in the static lighting vertex data for the Landscape vertex. */
	void GetStaticLightingVertex(INT VertexIndex, FStaticLightingVertex& OutVertex) const;

	ULandscapeComponent* LandscapeComponent;
	struct FLandscapeComponentDataInterface* DataInterface;
	FLOAT StaticLightingResolution;

	TArray<FColor> HeightData; // only for non-lightmass build case
};


#endif // _LANDSCAPELIGHT_H
