/*=============================================================================
	UnModelLight.h: Unreal model lighting.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNMODELLIGHT_H__
#define __UNMODELLIGHT_H__

/** Represents a BSP surface to the static lighting system. */
class FBSPSurfaceStaticLighting : public FStaticLightingTextureMapping, public FStaticLightingMesh
{
public:

	/** The surface's static lighting node group mapping info. */
	const FNodeGroup* NodeGroup;

	/** TRUE if the surface has complete static lighting. */
	UBOOL bComplete;

	FLightMapData2D* LightMapData;
	TMap<ULightComponent*,FShadowMapData2D*> ShadowMapData;

	/** Quantized light map data */
	FQuantizedLightmapData* QuantizedData;

	/** Minimum rectangle that encompasses all mapped texels. */
	FIntRect MappedRect;

	/** Initialization constructor. */
	FBSPSurfaceStaticLighting(
		const FNodeGroup* InNodeGroup,
		UModel* Model,
		UModelComponent* Component,
		UBOOL bForceDirectLightmap
		);

	/** Destructor. */
	~FBSPSurfaceStaticLighting();

	/** Resets the surface's static lighting data. */
	void ResetStaticLightingData();

	// FStaticLightingMesh interface.
	virtual void GetTriangle(INT TriangleIndex,FStaticLightingVertex& OutV0,FStaticLightingVertex& OutV1,FStaticLightingVertex& OutV2) const;
	virtual void GetTriangleIndices(INT TriangleIndex,INT& OutI0,INT& OutI1,INT& OutI2) const;
	virtual FLightRayIntersection IntersectLightRay(const FVector& Start,const FVector& End,UBOOL bFindNearestIntersection) const;

	//FStaticLightingTextureMapping interface.
	virtual void Apply(FLightMapData2D* InLightMapData,const TMap<ULightComponent*,FShadowMapData2D*>& InShadowMapData, FQuantizedLightmapData* QuantizedData);

#if WITH_EDITOR
	virtual UBOOL DebugThisMapping() const;

	/** 
	* Export static lighting mapping instance data to an exporter 
	* @param Exporter - export interface to process static lighting data
	**/
	virtual void ExportMapping(class FLightmassExporter* Exporter);
#endif	//WITH_EDITOR

	/**
	 * Returns the Guid used for static lighting.
	 * @return FGuid that identifies the mapping
	 **/
	virtual const FGuid& GetLightingGuid() const
	{
		return Guid;
	}

	virtual FString GetDescription() const
	{
		return FString(TEXT("BSPMapping"));
	}

	const UModel* GetModel() const
	{
		return Model;
	}

#if WITH_EDITOR
	/**
	 *	@return	UOject*		The object that is mapped by this mapping
	 */
	virtual UObject* GetMappedObject() const;
#endif	//WITH_EDITOR

private:

	/** The model this lighting data is for */
	UModel* Model;
};

#endif //__UNMODELLIGHT_H__
