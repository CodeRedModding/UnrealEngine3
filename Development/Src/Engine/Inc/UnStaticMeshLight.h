/*=============================================================================
	UnStaticMeshLight.h: Static mesh lighting code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNSTATICMESHLIGHT_H__
#define __UNSTATICMESHLIGHT_H__

/** Represents the triangles of one LOD of a static mesh primitive to the static lighting system. */
class FStaticMeshStaticLightingMesh : public FStaticLightingMesh
{
public:

	/** The meshes representing other LODs of this primitive. */
	TArray<FStaticLightingMesh*> OtherLODs;

	/** Initialization constructor. */
	FStaticMeshStaticLightingMesh(const UStaticMeshComponent* InPrimitive,INT InLODIndex,const TArray<ULightComponent*>& InRelevantLights);

	// FStaticLightingMesh interface.

	virtual void GetTriangle(INT TriangleIndex,FStaticLightingVertex& OutV0,FStaticLightingVertex& OutV1,FStaticLightingVertex& OutV2) const;

	virtual void GetTriangleIndices(INT TriangleIndex,INT& OutI0,INT& OutI1,INT& OutI2) const;

	virtual UBOOL ShouldCastShadow(ULightComponent* Light,const FStaticLightingMapping* Receiver) const;

	/** @return		TRUE if the specified triangle casts a shadow. */
	virtual UBOOL IsTriangleCastingShadow(UINT TriangleIndex) const;

	/** @return		TRUE if the mesh wants to control shadow casting per element rather than per mesh. */
	virtual UBOOL IsControllingShadowPerElement() const;

	virtual UBOOL IsUniformShadowCaster() const;

	virtual FLightRayIntersection IntersectLightRay(const FVector& Start,const FVector& End,UBOOL bFindNearestIntersection) const;

#if WITH_EDITOR
	/** 
	* Export static lighting mesh instance data to an exporter 
	* @param Exporter - export interface to process static lighting data
	**/
	virtual void ExportMeshInstance(class FLightmassExporter* Exporter) const;

	virtual const struct FSplineMeshParams* GetSplineParameters() const { return NULL; }

#endif	//WITH_EDITOR

protected:

	/** The LOD this mesh represents. */
	const INT LODIndex;

	/** 
	 * Sets the local to world matrix for this mesh, will also update LocalToWorldInverseTranspose
	 *
	 * @param InLocalToWorld Local to world matrix to apply
	 */
	void SetLocalToWorld(const FMatrix& InLocalToWorld);

private:

	/** The static mesh this mesh represents. */
	const UStaticMesh* StaticMesh;

	/** The primitive this mesh represents. */
	const UStaticMeshComponent* const Primitive;

	/** Cached local to world matrix to transform all the verts by */
	FMatrix LocalToWorld;
	
	/** The inverse transpose of the primitive's local to world transform. */
	FMatrix LocalToWorldInverseTranspose;

	/** Cached determinant for the local to world */
	FLOAT LocalToWorldDeterminant;

	/** TRUE if the primitive has a transform which reverses the winding of its triangles. */
	const BITFIELD bReverseWinding : 1;
	
	friend class FLightmassExporter;
};

/** Represents a static mesh primitive with texture mapped static lighting. */
class FStaticMeshStaticLightingTextureMapping : public FStaticLightingTextureMapping
{
public:

	/** Initialization constructor. */
	FStaticMeshStaticLightingTextureMapping(UStaticMeshComponent* InPrimitive,INT InLODIndex,FStaticLightingMesh* InMesh,INT InSizeX,INT InSizeY,INT InTextureCoordinateIndex,UBOOL bPerformFullQualityRebuild);

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
		return FString(TEXT("SMTextureMapping"));
	}

protected:

	/** The primitive this mapping represents. */
	UStaticMeshComponent* const Primitive;

	/** The LOD this mapping represents. */
	const INT LODIndex;
};

/** Represents a static mesh primitive with vertex mapped static lighting. */
class FStaticMeshStaticLightingVertexMapping : public FStaticLightingVertexMapping
{
public:

	/** Initialization constructor. */
	FStaticMeshStaticLightingVertexMapping(UStaticMeshComponent* InPrimitive,INT InLODIndex,FStaticLightingMesh* InMesh,UBOOL bPerformFullQualityBuild);

	// FStaticLightingTextureMapping interface
	virtual void Apply(FLightMapData1D* LightMapData,const TMap<ULightComponent*,FShadowMapData1D*>& ShadowMapData, FQuantizedLightmapData* QuantizedData);	

#if WITH_EDITOR
	/** 
	 * Export static lighting mapping instance data to an exporter 
	 * @param Exporter - export interface to process static lighting data
	 */
	virtual void ExportMapping(class FLightmassExporter* Exporter);
#endif	//WITH_EDITOR

	virtual FString GetDescription() const
	{
		return FString(TEXT("SMVertexMapping"));
	}
private:

	/** The primitive this mapping represents. */
	UStaticMeshComponent* const Primitive;

	/** The LOD this mapping represents. */
	const INT LODIndex;
};

#endif //__UNSTATICMESHLIGHT_H__
