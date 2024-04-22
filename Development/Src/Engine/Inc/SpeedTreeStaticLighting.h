/*=============================================================================
	SpeedTreeStaticLighting.h: SpeedTreeComponent static lighting definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SPEEDTREESTATICLIGHTING_H__
#define __SPEEDTREESTATICLIGHTING_H__
#if WITH_SPEEDTREE

#include "SpeedTree.h"
#include "EngineMaterialClasses.h"

// Forward declarations.
class FSpeedTreeComponentStaticLighting;
class FSpeedTreeStaticLightingMesh;
class FSpeedTreeStaticLightingMapping;

/** Encapsulates ray tracing of a SpeedTreeComponent. */
class FStaticLightingSpeedTreeRayTracer
{
public:

	/** Initialization constructor. */
	FStaticLightingSpeedTreeRayTracer(
		const TArray<FStaticLightingVertex>& InVertices,
		const TArray<WORD>& InIndices,
		const FMeshBatchElement& InMeshElement,
		FLOAT InOpacity
		):
		Vertices(InVertices),
		Indices(InIndices),
		MeshElement(InMeshElement),
		Opacity(InOpacity),
		RandomStream(0)
	{
	}

	/** Initializes the ray tracer. */
	void Init();

	/** @return TRUE if a line segment intersects the SpeedTree geometry. */
	FLightRayIntersection IntersectLightRay(const FVector& Start,const FVector& End) const;

	// kDOP data provider interface.
	FORCEINLINE const FVector& GetVertex(WORD Index) const
	{
		return Vertices(Index).WorldPosition;
	}
	FORCEINLINE UMaterialInstance* GetMaterial(WORD MaterialIndex) const
	{
		return NULL;
	}
	FORCEINLINE INT GetItemIndex(WORD MaterialIndex) const
	{
		return 0;
	}
	FORCEINLINE UBOOL ShouldCheckMaterial(INT MaterialIndex) const
	{
		return TRUE;
	}
	FORCEINLINE const TkDOPTree<const FStaticLightingSpeedTreeRayTracer,WORD>& GetkDOPTree(void) const
	{
		return kDopTree;
	}
	FORCEINLINE const FMatrix& GetLocalToWorld(void) const
	{
		return FMatrix::Identity;
	}
	FORCEINLINE const FMatrix& GetWorldToLocal(void) const
	{
		return FMatrix::Identity;
	}
	FORCEINLINE FMatrix GetLocalToWorldTransposeAdjoint(void) const
	{
		return FMatrix::Identity;
	}
	FORCEINLINE FLOAT GetDeterminant(void) const
	{
		return 1.0f;
	}

	// accessors
	FLOAT GetOpacity() const
	{
		return Opacity;
	}

private:

	TkDOPTree<const FStaticLightingSpeedTreeRayTracer,WORD> kDopTree;
	const TArray<FStaticLightingVertex>& Vertices;
	const TArray<WORD>& Indices;
	const FMeshBatchElement MeshElement;
	FLOAT Opacity;

	/** Random number stream so we have determinism (esp between UE3/lightmass lighting) (mutable so we can use it in the const tracing functions) */
	mutable FRandomStream RandomStream;
};

/** Represents a single LOD of a SpeedTreeComponent's mesh element to the static lighting system. */
class FSpeedTreeStaticLightingMesh : public FStaticLightingMesh
{
public:

	/** Initialization constructor. */
	FSpeedTreeStaticLightingMesh(
		FSpeedTreeComponentStaticLighting* InComponentStaticLighting,
		INT InLODIndex,
		const FMeshBatchElement& InMeshElement,
		ESpeedTreeMeshType InMeshType,
		FLOAT InShadowOpacity,
		UBOOL bInTwoSidedMaterial,
		const TArray<ULightComponent*>& InRelevantLights,
		const FLightingBuildOptions& Options
		);

	// FStaticLightingMesh interface.
	virtual void GetTriangle(INT TriangleIndex,FStaticLightingVertex& OutV0,FStaticLightingVertex& OutV1,FStaticLightingVertex& OutV2) const;
	virtual void GetTriangleIndices(INT TriangleIndex,INT& OutI0,INT& OutI1,INT& OutI2) const;
	virtual UBOOL ShouldCastShadow(ULightComponent* Light,const FStaticLightingMapping* Receiver) const;
	virtual UBOOL IsUniformShadowCaster() const;
	virtual FLightRayIntersection IntersectLightRay(const FVector& Start,const FVector& End,UBOOL bFindClosestIntersection) const;

#if WITH_EDITOR
	/** 
	 * Export static lighting mesh instance data to an exporter 
	 * @param Exporter - export interface to process static lighting data
	 **/
	virtual void ExportMeshInstance(class FLightmassExporter* Exporter) const;
#endif	//WITH_EDITOR

	/** 
	 * @return The static lighting component mirror object
	 */
	const TRefCountPtr<FSpeedTreeComponentStaticLighting> GetComponentStaticLighting() const 
	{ 
		return ComponentStaticLighting; 
	}

	/** 
	 * @return LODIndex of this mesh instance 
	 */
	INT GetLODIndex() const
	{
		return LODIndex;
	}

	/** 
	 * @return the MeshType of this mesh instance 
	 */
	ESpeedTreeMeshType GetMeshType() const
	{
		return MeshType;
	}

	/** 
	 * @return the MeshElement for this instance
	 */
	const FMeshBatchElement& GetMeshElement() const
	{
		return MeshElement;
	}

	/**
	 * @return the shadow casting opacity of the mesh
	 */
	FLOAT GetShadowOpacity() const
	{
		return RayTracer.GetOpacity();
	}

private:

	/** The static lighting for the component this mesh is part of. */
	TRefCountPtr<FSpeedTreeComponentStaticLighting> ComponentStaticLighting;

	/** The LOD this mesh is part of. */
	INT LODIndex;

	/** The mesh element which this object represents to the static lighting system. */
	FMeshBatchElement MeshElement;

	/** The type of this mesh. */
	ESpeedTreeMeshType MeshType;

	/** A helper object used to raytrace the mesh's triangles. */
	FStaticLightingSpeedTreeRayTracer RayTracer;
};

/** Represents the per-vertex static lighting of a single LOD of a SpeedTreeComponent's mesh element to the static lighting system. */
class FSpeedTreeStaticLightingMapping : public FStaticLightingVertexMapping
{
public:

	/** Initialization constructor. */
	FSpeedTreeStaticLightingMapping(FStaticLightingMesh* InMesh,FSpeedTreeComponentStaticLighting* InComponentStaticLighting,INT InLODIndex,const FMeshBatchElement& InMeshElement,ESpeedTreeMeshType InMeshType);

	// FStaticLightingVertexMapping interface.
	virtual void Apply(FLightMapData1D* LightMapData,const TMap<ULightComponent*,FShadowMapData1D*>& ShadowMapData, FQuantizedLightmapData* QuantizedData);

	// Accessors.
	INT GetLODIndex() const { return LODIndex; }
	const FMeshBatchElement& GetMeshElement() const { return MeshElement; }
	ESpeedTreeMeshType GetMeshType() const { return MeshType; }

#if WITH_EDITOR
	/** 
	 * Export static lighting mapping instance data to an exporter 
	 * @param Exporter - export interface to process static lighting data
	 **/
	virtual void ExportMapping(class FLightmassExporter* Exporter);
#endif	//WITH_EDITOR

	virtual FString GetDescription() const
	{
		return FString(TEXT("SpeedTreeMapping"));
	}

private:

	/** The static lighting for the component this mapping is part of. */
	TRefCountPtr<FSpeedTreeComponentStaticLighting> ComponentStaticLighting;

	/** The LOD this mapping is part of. */
	INT LODIndex;

	/** The mesh element which this object represents to the static lighting system. */
	FMeshBatchElement MeshElement;

	/** The type of this mesh. */
	ESpeedTreeMeshType MeshType;
};

/** Manages the static lighting mappings for a single SpeedTreeComponent. */
class FSpeedTreeComponentStaticLighting : public FRefCountedObject
{
public:

	/** Initialization constructor. */
	FSpeedTreeComponentStaticLighting(USpeedTreeComponent* InComponent,const TArray<ULightComponent*>& InRelevantLights);

	/** Creates the mesh and mapping for a mesh of the SpeedTreeComponent. */
	void CreateMapping(
		FStaticLightingPrimitiveInfo& OutPrimitiveInfo,
		INT LODIndex,
		const FMeshBatchElement& MeshElement,
		ESpeedTreeMeshType MeshType,
		FLOAT ShadowOpacity,
		UBOOL bInTwoSidedMaterial,
		const FLightingBuildOptions& Options
		);

	/**
	 * Applies a mapping's static lighting data.
	 * @param Mapping - The mapping which the static lighting data is for.
	 * @param MappingLightMapData - The mapping's light-map data.
	 * @param MappingShadowMapData - The mapping's shadow-map data.
	 * @param QuantizedData - The quantized lightmap data, if already quantized
	 */
	void ApplyCompletedMapping(
		FSpeedTreeStaticLightingMapping* Mapping,
		FLightMapData1D* MappingLightMapData,
		const TMap<ULightComponent*,FShadowMapData1D*>& MappingShadowMapData,
		FQuantizedLightmapData* QuantizedData
		);

	// Accessors.
	USpeedTreeComponent* GetComponent() const
	{
		return Component;
	}
	const TArray<FStaticLightingVertex>& GetVertices(ESpeedTreeMeshType MeshType) const
	{
		return ChooseByMeshType<TArray<FStaticLightingVertex> >(MeshType,BranchVertices,BranchVertices,FrondVertices,LeafCardVertices,LeafMeshVertices,BillboardVertices);
	}
	const TArray<WORD>& GetIndices() const
	{
		return Indices;
	}
	UBOOL IsMappingFromThisComponent(const FStaticLightingMapping* Mapping) const
	{
		return Mappings.ContainsItem((FSpeedTreeStaticLightingMapping*)Mapping);
	}

	/**
	 * @return a unique ID for this component to tie sub mappings together
	 */
	const FGuid& GetComponentGuid()
	{
		return ComponentGuid;
	}

private:

	/** The component that mappings are being managed for. */
	USpeedTreeComponent* const Component;

	/** The component's relevant lights. */
	TArray<ULightComponent*> RelevantLights;

	/** The static lighting vertices of the component's branches. */
	TArray<FStaticLightingVertex> BranchVertices;

    /** The static lighting vertices of the component's fronds. */
	TArray<FStaticLightingVertex> FrondVertices;

	/** The static lighting vertices of the component's leaf meshes. */
	TArray<FStaticLightingVertex> LeafMeshVertices;

	/** The static lighting vertices of the component's leaf cards. */
	TArray<FStaticLightingVertex> LeafCardVertices;

	/** The static lighting vertices of the component's billboard meshes. */
	TArray<FStaticLightingVertex> BillboardVertices;

	/** The component's triangle vertex indices. */
	const TArray<WORD>& Indices;

	/** The light-map data for the branches. */
	FLightMapData1D* BranchLightMapData;

	/** The light-map data for the fronds. */
	FLightMapData1D* FrondLightMapData;
	
	/** The light-map data for the leaf meshes. */
	FLightMapData1D* LeafMeshLightMapData;

	/** The light-map data for the leaf card. */
	FLightMapData1D* LeafCardLightMapData;

	/** The light-map data for the billboard meshes. */
	FLightMapData1D* BillboardLightMapData;

	/** The gathered quantized light-map data for the branches. */
	TMap<FQuantizedLightmapData*, INT> BranchQuantizedMap;

	/** The gathered quantized light-map data for the fronds. */
	TMap<FQuantizedLightmapData*, INT> FrondQuantizedMap;

	/** The gathered quantized light-map data for the leaf meshes. */
	TMap<FQuantizedLightmapData*, INT> LeafMeshQuantizedMap;
	
	/** The gathered quantized light-map data for the leaf card. */
	TMap<FQuantizedLightmapData*, INT> LeafCardQuantizedMap;
	
	/** The gathered quantized light-map data for the billboard meshes. */
	TMap<FQuantizedLightmapData*, INT> BillboardQuantizedMap;

	/** TRUE if the component has branch static lighting. */
	UBOOL bHasBranchStaticLighting;

	/** TRUE if the component has frond static lighting. */
	UBOOL bHasFrondStaticLighting;

	/** TRUE if the component has leaf mesh static lighting. */
	UBOOL bHasLeafMeshStaticLighting;

	/** TRUE if the component has leaf card static lighting. */
	UBOOL bHasLeafCardStaticLighting;

	/** TRUE if the component has billboard static lighting. */
	UBOOL bHasBillboardStaticLighting;

	/** Unique ID to tie the mappings together in distributed lighting */
	FGuid ComponentGuid;

	/** The shadow-map data for a single light. */
	class FLightShadowMaps : public FRefCountedObject
	{
	public:

		FShadowMapData1D BranchShadowMapData;
		FShadowMapData1D FrondShadowMapData;
		FShadowMapData1D LeafMeshShadowMapData;
		FShadowMapData1D LeafCardShadowMapData;
		FShadowMapData1D BillboardShadowMapData;

		/** Initialization constructor. */
		FLightShadowMaps(INT NumBranchVertices,INT NumFrondVertices,INT NumLeafMeshVertices,INT NumLeafCardVertices,INT NumBillboardVertices):
			BranchShadowMapData(NumBranchVertices),
			FrondShadowMapData(NumFrondVertices),
			LeafMeshShadowMapData(NumLeafMeshVertices),
			LeafCardShadowMapData(NumLeafCardVertices),
			BillboardShadowMapData(NumBillboardVertices)
		{}
	};

	/** The shadow-map data for the component. */
	TMap<ULightComponent*,TRefCountPtr<FLightShadowMaps> > ComponentShadowMaps;

	/** All mappings for the component. */
	TArray<FSpeedTreeStaticLightingMapping*> Mappings;

	/** The number of mappings which have complete static lighting. */
	INT NumCompleteMappings;
};

#endif
#endif