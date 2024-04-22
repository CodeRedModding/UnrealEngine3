/*=============================================================================
	TerrainLight.h: Terrain lighting code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __TERRAINLIGHT_H__
#define __TERRAINLIGHT_H__

/** Represents terrain to the static lighting system. */
class FTerrainComponentStaticLighting : public FStaticLightingMesh, public FStaticLightingTextureMapping
{
public:
	/** Initialization constructor. */
	FTerrainComponentStaticLighting(
		UTerrainComponent* InPrimitive,
		const TArray<FIntPoint>& InQuadIndexToCoordinatesMap,
		const TArray<ULightComponent*>& InRelevantLights,
		UBOOL bPerformFullQualityBuild,
		INT InPatchExpandX, INT InPatchExpandY,
		INT InSizeX, INT InSizeY
		);

	FStaticLightingVertex GetVertex(INT X,INT Y) const;

	// FStaticLightingMesh interface.
	virtual void GetTriangle(INT TriangleIndex,FStaticLightingVertex& OutV0,FStaticLightingVertex& OutV1,FStaticLightingVertex& OutV2) const;
	virtual void GetTriangleIndices(INT TriangleIndex,INT& OutI0,INT& OutI1,INT& OutI2) const;
	virtual FLightRayIntersection IntersectLightRay(const FVector& Start,const FVector& End,UBOOL bFindNearestIntersection) const;

	// FStaticLightingTextureMapping interface.
	virtual void Apply(FLightMapData2D* LightMapData,const TMap<ULightComponent*,FShadowMapData2D*>& ShadowMapData, FQuantizedLightmapData* QuantizedData);

	/**
	 * Returns the Guid used for static lighting.
	 * @return FGuid that identifies the mapping
	 **/
	virtual const FGuid& GetLightingGuid() const
	{
#if WITH_EDITORONLY_DATA
		return Guid;
#else
		static const FGuid NullGuid( 0, 0, 0, 0 );
		return NullGuid; 
#endif // WITH_EDITORONLY_DATA
	}

#if WITH_EDITOR
	/** 
	* Export static lighting mapping instance data to an exporter 
	* @param Exporter - export interface to process static lighting data
	**/
	virtual void ExportMapping(class FLightmassExporter* Exporter);
#endif	//WITH_EDITOR

	virtual FString GetDescription() const
	{
		return FString(TEXT("TerrainMapping"));
	}

	/** The terrain this object represents. */
	const ATerrain* GetTerrain() const
	{
		return Terrain;
	}

	void Dump() const;

private:

	/** The terrain this object represents. */
	ATerrain* const Terrain;

	/** The primitive this object represents. */
	UTerrainComponent* const Primitive;

	/** The inverse transpose of the primitive's local to world transform. */
	const FMatrix LocalToWorldInverseTranspose;

	/** The number of quads in the component along the X axis. */
	const INT NumQuadsX;

	/** The number of quads in the component along the Y axis. */
	const INT NumQuadsY;

	/** The number of quads we are expanding to eliminate seams. */
	INT ExpandQuadsX;
	INT ExpandQuadsY;

	/** A map from quad index to the quad's coordinates. */
	TArray<FIntPoint> QuadIndexToCoordinatesMap;

	/** If the normal needs to be flipped due to negative drawscale */
	UBOOL bNeedsNormalFlip;
};

#endif	//__TERRAINLIGHT_H__
