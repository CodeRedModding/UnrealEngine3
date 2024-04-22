/*=============================================================================
	StaticLightingPrivate.h: Private static lighting system definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __STATICLIGHTINGPRIVATE_H__
#define __STATICLIGHTINGPRIVATE_H__

// Don't compile the static lighting system on consoles.
#if !CONSOLE

// Includes.
#include "GenericOctree.h"

/** The number of coefficients that are gathered for each FGatheredLightSample. */ 
static const INT NUM_GATHERED_LIGHTMAP_COEF = 4;

/** The number of directional coefficients gathered for each FGatheredLightSample. */ 
static const INT NUM_GATHERED_DIRECTIONAL_LIGHTMAP_COEF = 3;

/** The number of simple coefficients gathered for each FGatheredLightSample. */ 
static const INT NUM_GATHERED_SIMPLE_LIGHTMAP_COEF = 1;

/** The index at which simple coefficients are stored in any array containing all NUM_GATHERED_LIGHTMAP_COEF coefficients. */ 
static const INT SIMPLE_GATHERED_LIGHTMAP_COEF_INDEX = 3;

/** 
 * The light incident for a point on a surface, in the representation used when gathering lighting. 
 * This representation is additive, and allows for accumulating lighting contributions in-place. 
 */
struct FGatheredLightSample
{
	/** 
	 * The lighting coefficients, colored. 
	 * The first NUM_GATHERED_DIRECTIONAL_LIGHTMAP_COEF coefficients store the colored incident lighting along each lightmap basis axis.
     * The last coefficient, SIMPLE_GATHERED_LIGHTMAP_COEF_INDEX, stores the simple lighting, which is incident lighting along the vertex normal.
	 */
	FLOAT Coefficients[NUM_GATHERED_LIGHTMAP_COEF][3];

	/** True if this sample maps to a valid point on a triangle.  This is only meaningful for texture lightmaps. */
	UBOOL bIsMapped;

	/** Initialization constructor. */
	FGatheredLightSample():
		bIsMapped(FALSE)
	{
		appMemzero(Coefficients,sizeof(Coefficients));
	}

	/**
	 * Constructs a light sample representing a point light.
	 * @param Color - The color/intensity of the light at the sample point.
	 * @param Direction - The direction toward the light at the sample point.
	 */
	static FGatheredLightSample PointLight(const FLinearColor& Color,const FVector& Direction);

	/**
	 * Constructs a light sample representing a sky light.
	 * @param UpperColor - The color/intensity of the sky light's upper hemisphere.
	 * @param LowerColor - The color/intensity of the sky light's lower hemisphere.
	 * @param WorldZ - The world's +Z axis in tangent space.
	 */
	static FGatheredLightSample SkyLight(const FLinearColor& UpperColor,const FLinearColor& LowerColor,const FVector& WorldZ);

	/**
	 * Adds a weighted light sample to this light sample.
	 * @param OtherSample - The sample to add.
	 * @param Weight - The weight to multiply the other sample by before addition.
	 */
	void AddWeighted(const FGatheredLightSample& OtherSample,FLOAT Weight);

	/** Converts an FGatheredLightSample into a FLightSample. */
	FLightSample ConvertToLightSample() const;
};

/**
 * The raw data which is used to construct a 2D light-map.
 */
class FGatheredLightMapData2D
{
public:

	/** The GUIDs of lights which this light-map stores. */
	TArray<FGuid> LightGuids;

	/**
	 * Minimal initialization constructor.
	 */
	FGatheredLightMapData2D(UINT InSizeX,UINT InSizeY):
		Data(InSizeX * InSizeY),
		SizeX(InSizeX),
		SizeY(InSizeY)
	{}

	// Accessors.
	const FGatheredLightSample& operator()(UINT X,UINT Y) const { return Data(SizeX * Y + X); }
	FGatheredLightSample& operator()(UINT X,UINT Y) { return Data(SizeX * Y + X); }
	UINT GetSizeX() const { return SizeX; }
	UINT GetSizeY() const { return SizeY; }

	FLightMapData2D* ConvertToLightmap2D() const;

private:

	/** The incident light samples for a 2D array of points on the surface. */
	TChunkedArray<FGatheredLightSample> Data;

	/** The width of the light-map. */
	UINT SizeX;

	/** The height of the light-map. */
	UINT SizeY;
};

/**
 * The raw data which is used to construct a 1D light-map.
 */
class FGatheredLightMapData1D
{
public:

	/** The GUIDs of lights which this light-map stores. */
	TArray<FGuid> LightGuids;

	/**
	 * Minimal initialization constructor.
	 */
	FGatheredLightMapData1D(INT Size)
	{
		Data.Empty(Size);
		Data.AddZeroed(Size);
	}

	// Accessors.
	const FGatheredLightSample& operator()(UINT Index) const { return Data(Index); }
	FGatheredLightSample& operator()(UINT Index) { return Data(Index); }
	INT GetSize() const { return Data.Num(); }

	FLightMapData1D* ConvertToLightmap1D() const;

private:

	/** The incident light samples for a 1D array of points. */
	TArray<FGatheredLightSample> Data;
};

/** A lighting cache. */
class FLightingCache
{
public:

	/** The irradiance for a single static lighting vertex. */
	class FRecord
	{
	public:

		/** The static lighting vertex the irradiance record was computed for. */
		FStaticLightingVertex Vertex;

		/** The radius around the vertex that the record ir relevant to. */
		FLOAT Radius;

		/** The lighting incident on an infinitely small surface at WorldPosition facing along WorldNormal. */
		FGatheredLightSample Lighting;

		/** Initialization constructor. */
		FRecord(const FStaticLightingVertex& InVertex,FLOAT InRadius,const FGatheredLightSample& InLighting):
			Vertex(InVertex),
			Radius(InRadius),
			Lighting(InLighting)
		{}
	};

	/** Adds a lighting record to the cache. */
	void AddRecord(const FRecord& Record);

	/**
	 * Interpolates nearby lighting records for a vertex.
	 * @param Vertex - The vertex to interpolate the lighting for.
	 * @param OutLighting - If TRUE is returned, contains the blended lighting records that were found near the point.
	 * @return TRUE if nearby records were found with enough relevance to interpolate this point's lighting.
	 */
	UBOOL InterpolateLighting(const FStaticLightingVertex& Vertex,FGatheredLightSample& OutLighting) const;

	/** Initialization constructor. */
	FLightingCache(const FBox& InBoundingBox);

private:

	struct FRecordOctreeSemantics;

	/** The type of lighting cache octree nodes. */
	typedef TOctree<FRecord,FRecordOctreeSemantics> LightingOctreeType;

	/** The octree semantics for irradiance records. */
	struct FRecordOctreeSemantics
	{
		enum { MaxElementsPerLeaf = 4 };
		enum { MaxNodeDepth = 12 };

		typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

		static FBoxCenterAndExtent GetBoundingBox(const FRecord& LightingRecord)
		{
			return FBoxCenterAndExtent(
				LightingRecord.Vertex.WorldPosition,
				FVector(LightingRecord.Radius,LightingRecord.Radius,LightingRecord.Radius)
				);
		}

		static void SetElementId(const FRecord& Element,FOctreeElementId Id)
		{
		}
	};

	/** The lighting cache octree. */
	LightingOctreeType Octree;
};

/** A line segment representing a direct light path through the scene. */
class FLightRay
{
public:

	FVector Start;
	FVector End;
	FVector Direction;
	FVector OneOverDirection;
	FLOAT Length;

	const FStaticLightingMapping* const Mapping;
	ULightComponent* const Light;

	/** Initialization constructor. */
	FLightRay(const FVector& InStart,const FVector& InEnd,const FStaticLightingMapping* InMapping,ULightComponent* InLight):
		Start(InStart),
		End(InEnd),
		Direction(InEnd - InStart),
		Mapping(InMapping),
		Light(InLight)
	{
		OneOverDirection.X = Square(Direction.X) > DELTA ? 1.0f / Direction.X : 0.0f;
		OneOverDirection.Y = Square(Direction.Y) > DELTA ? 1.0f / Direction.Y : 0.0f;
		OneOverDirection.Z = Square(Direction.Z) > DELTA ? 1.0f / Direction.Z : 0.0f;
		Length = 1.0f;
	}

	/** Clips the light ray to an intersection point. */
	void ClipAgainstIntersection(const FVector& IntersectionPoint)
	{
		End = IntersectionPoint;
		Length = (IntersectionPoint - Start) | Direction;
	}
};

/** A mesh and its bounding box. */
struct FMeshAndBounds
{
	const FStaticLightingMesh* Mesh;
	FBox BoundingBox;

	/** Initialization constructor. */
	FMeshAndBounds(const FStaticLightingMesh* InMesh):
		Mesh(InMesh)
	{
		if(Mesh)
		{
			BoundingBox = Mesh->BoundingBox;
		}
	}

	/**
	 * Checks a line segment for intersection with this mesh.
	 * @param LightRay - The line segment to check.
	 * @return The intersection of between the light ray and the mesh.
	 */
	FLightRayIntersection IntersectLightRay(const FLightRay& LightRay,UBOOL bFindClosestIntersection) const
	{
		if(	Mesh &&
			FLineBoxIntersection(BoundingBox,LightRay.Start,LightRay.End,LightRay.Direction,LightRay.OneOverDirection) &&
			Mesh->ShouldCastShadow(LightRay.Light,LightRay.Mapping))
		{
			return Mesh->IntersectLightRay(LightRay.Start,LightRay.End,bFindClosestIntersection);
		}
		else
		{
			return FLightRayIntersection::None();
		}
	}

};

typedef TOctree<FMeshAndBounds,struct FStaticLightingMeshOctreeSemantics> FStaticLightingMeshOctree;

/** Octree semantic definitions. */
struct FStaticLightingMeshOctreeSemantics
{
	enum { MaxElementsPerLeaf = 4 };
	enum { MaxNodeDepth = 9 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	static FBoxCenterAndExtent GetBoundingBox(const FMeshAndBounds& MeshAndBounds)
	{
		return FBoxCenterAndExtent(MeshAndBounds.BoundingBox);
	}

	static void SetElementId(const FMeshAndBounds& Element,FOctreeElementId Id)
	{
	}
};

class FStaticLightingAggregateMeshDataProvider;  /* needs predeclaration... */

/** The static lighting mesh. */
class FStaticLightingAggregateMesh
{
public:

	/**
	 * Merges a mesh into the shadow mesh.
	 * @param Mesh - The mesh the triangle comes from.
	 */
	void AddMesh(const FStaticLightingMesh* Mesh);

	/** Prepares the mesh for raytracing. */
	void PrepareForRaytracing();

	/** Gets the bounding box of everything in the aggregate mesh. */
	FBox GetBounds() const;

	/**
	 * Checks a light ray for intersection with the shadow mesh.
	 * @param LightRay - The line segment to check for intersection.
	 * @param CoherentRayCache - The calling thread's collision cache.
	 * @param bFindClosestIntersection - TRUE if the intersection must return the closest intersection.  FALSE if it may return any intersection.
	 * @return The intersection of between the light ray and the mesh.
	 */
	FLightRayIntersection IntersectLightRay(const FLightRay& LightRay,UBOOL bFindClosestIntersection,class FCoherentRayCache& CoherentRayCache) const;

	/** Initialization constructor. */
	FStaticLightingAggregateMesh();

private:

	/** Theoctree used to cull ray-mesh intersections. */
	FStaticLightingMeshOctree MeshOctree;

	friend class FStaticLightingAggregateMeshDataProvider;

	/** The world-space kd-tree which is used by the simple meshes in the world. */
	TkDOPTree<const FStaticLightingAggregateMeshDataProvider,DWORD> kDopTree;

	/** The triangles used to build the kd-tree, valid until PrepareForRaytracing is called. */
	TArray<FkDOPBuildCollisionTriangle<DWORD> > kDOPTriangles;

	/** The meshes used to build the kd-tree. */
	TArray<const FStaticLightingMesh*> kDopMeshes;

	/** The vertices used by the kd-tree. */
	TArray<FVector> Vertices;

	/** The bounding box of everything in the aggregate mesh. */
	FBox SceneBounds;
};

/** Information which is cached while processing a group of coherent rays. */
class FCoherentRayCache
{
public:

	/** The mesh that was last hit by a ray in this thread. */
	FMeshAndBounds LastHitMesh;

	/** The thread's local area lighting cache. */
	FLightingCache AreaLightingCache;

	/** Initialization constructor. */
	FCoherentRayCache(const FStaticLightingMesh* InSubjectMesh):
		LastHitMesh(NULL),
		AreaLightingCache(InSubjectMesh->BoundingBox)
	{}
};

/** Encapsulation of all Lightmass statistics */
struct FLightmassStatistics
{
	/** Constructor that clears all statistics */
	FLightmassStatistics()
	{
		ClearAll();
	}
	/** Clears all statistics */
	void ClearAll()
	{
		StartupTime						= 0.0;
		CollectTime						= 0.0;
		PrepareLightsTime				= 0.0;
		GatherLightingInfoTime			= 0.0;
		ProcessingTime					= 0.0;
		CollectLightmassSceneTime		= 0.0;
		ExportTime						= 0.0;
		LightmassTime					= 0.0;
		SwarmStartupTime				= 0.0;
		SwarmCallbackTime				= 0.0;
		SwarmJobTime					= 0.0;
		ImportTime						= 0.0;
		ImportTimeInProcessing			= 0.0;
		ApplyTime						= 0.0;
		ApplyTimeInProcessing			= 0.0;
		EncodingTime					= 0.0;
		FinishingTime					= 0.0;
		TotalTime						= 0.0;
		Scratch0						= 0.0;
		Scratch1						= 0.0;
		Scratch2						= 0.0;
		Scratch3						= 0.0;
	}
	/** Adds timing measurements from another FLightmassStatistics. */
	FLightmassStatistics& operator+=( const FLightmassStatistics& Other )
	{
		StartupTime						+= Other.StartupTime;
		CollectTime						+= Other.CollectTime;
		PrepareLightsTime				+= Other.PrepareLightsTime;
		GatherLightingInfoTime			+= Other.GatherLightingInfoTime;
		ProcessingTime					+= Other.ProcessingTime;
		CollectLightmassSceneTime		+= Other.CollectLightmassSceneTime;
		ExportTime						+= Other.ExportTime;
		LightmassTime					+= Other.LightmassTime;
		SwarmStartupTime				+= Other.SwarmStartupTime;
		SwarmCallbackTime				+= Other.SwarmCallbackTime;
		SwarmJobTime					+= Other.SwarmJobTime;
		ImportTime						+= Other.ImportTime;
		ImportTimeInProcessing			+= Other.ImportTimeInProcessing;
		ApplyTime						+= Other.ApplyTime;
		ApplyTimeInProcessing			+= Other.ApplyTimeInProcessing;
		EncodingTime					+= Other.EncodingTime;
		FinishingTime					+= Other.FinishingTime;
		TotalTime						+= Other.TotalTime;
		Scratch0						+= Other.Scratch0;
		Scratch1						+= Other.Scratch1;
		Scratch2						+= Other.Scratch2;
		Scratch3						+= Other.Scratch3;
		return *this;
	}

	/** Time spent starting up, in seconds. */
	DOUBLE	StartupTime;
	/** Time spent preparing and collecting the scene, in seconds. */
	DOUBLE	CollectTime;
	DOUBLE	PrepareLightsTime;
	DOUBLE	GatherLightingInfoTime;
	/** Time spent in the actual lighting path, in seconds */
	DOUBLE	ProcessingTime;
	/** Time spent collecting the scene and assets for Lightmass, in seconds. */
	DOUBLE	CollectLightmassSceneTime;
	/** Time spent exporting, in seconds. */
	DOUBLE	ExportTime;
	/** Time spent running Lightmass. */
	DOUBLE	LightmassTime;
	/** Time spent in various Swarm APIs, in seconds. */
	DOUBLE	SwarmStartupTime;
	DOUBLE	SwarmCallbackTime;
	DOUBLE	SwarmJobTime;
	/** Time spent importing and applying results, in seconds. */
	DOUBLE	ImportTime;
	DOUBLE	ImportTimeInProcessing;
	/** Time spent just applying results, in seconds. */
	DOUBLE	ApplyTime;
	DOUBLE	ApplyTimeInProcessing;
	/** Time spent encoding textures, in seconds. */
	DOUBLE	EncodingTime;
	/** Time spent finishing up, in seconds. */
	DOUBLE	FinishingTime;
	/** Total time spent for the lighting build. */
	DOUBLE	TotalTime;

	/** Reusable temporary statistics */
	DOUBLE	Scratch0;
	DOUBLE	Scratch1;
	DOUBLE	Scratch2;
	DOUBLE	Scratch3;
};

/** StaticLighting sorting helper */
struct FStaticLightingMappingSortHelper
{
	INT NumTexels;
	TRefCountPtr<FStaticLightingMapping> Mapping;
};

/** The state of the static lighting system. */
class FStaticLightingSystem
{
public:

	/**
	 * Initializes this static lighting system, and builds static lighting based on the provided options.
	 * @param InOptions - The static lighting build options.
	 */
	FStaticLightingSystem(const FLightingBuildOptions& InOptions);

	void CompleteDeterministicMappings(class FLightmassProcessor* LightmassProcessor);

	/**
	 * Calculates shadowing for a given mapping surface point and light.
	 * @param Mapping - The mapping the point comes from.
	 * @param WorldSurfacePoint - The point to check shadowing at.
	 * @param Light - The light to check shadowing from.
	 * @param CoherentRayCache - The calling thread's collision cache.
	 * @return TRUE if the surface point is shadowed from the light.
	 */
	UBOOL CalculatePointShadowing(const FStaticLightingMapping* Mapping,const FVector& WorldSurfacePoint,ULightComponent* Light,FCoherentRayCache& CoherentRayCache) const;

	/**
	 * Calculates the lighting contribution of a light to a mapping vertex.
	 * @param Mapping - The mapping the vertex comes from.
	 * @param Vertex - The vertex to calculate the lighting contribution at.
	 * @param Light - The light to calculate the lighting contribution from.
	 * @return The incident lighting on the vertex.
	 */
	FGatheredLightSample CalculatePointLighting(const FStaticLightingMapping* Mapping,const FStaticLightingVertex& Vertex,ULightComponent* Light) const;

	/**
	 * Calculates the lighting contribution of all area lights to a mapping vertex.
	 * @param Mapping - The mapping the vertex comes from.
	 * @param Vertex - The vertex to calculate the lighting contribution at.
	 * @param CoherentRayCache - The calling thread's collision cache.
	 * @return The incident area lighting on the vertex.
	 */
	FGatheredLightSample CalculatePointAreaLighting(const FStaticLightingMapping* Mapping,const FStaticLightingVertex& Vertex,FCoherentRayCache& CoherentRayCache,FRandomStream& RandomStream,UBOOL bDebugCurrentTexel);

	/**
	 * Clear out all the binary dump log files, so the next run will have just the needed files
	 * @param bUseLightmass If TRUE, this lighting was done using offline rendering
	 */
	static void ClearBinaryDumps(UBOOL bUseLightmass);

	/**
	 * Dump texture map data to a series of output binary files
	 *
	 * @param MappingGuid Guid of the mapping this set of texture maps belongs to
	 * @param Description The description of the mapping type that generated them
	 * @param LightMapData Light map information
	 * @param ShadowMaps Collection of shadow maps to dump
	 * @param bUseLightmass If TRUE, this lighting was done using offline rendering
	 */
	static void DumpLightMapsToDisk(const FGuid& MappingGuid, const FString& Description, FLightMapData2D* LightMapData, TMap<ULightComponent*,FShadowMapData2D*>& ShadowMaps, UBOOL bUseLightmass);

	/**
	 * Dump vertex map data to a series of output binary files
	 *
	 * @param MappingGuid Guid of the mapping this set of vertex maps belongs to
	 * @param Description The description of the mapping type that generated them
	 * @param LightMapData Light map information
	 * @param ShadowMaps Collection of shadow maps to dump
	 * @param bUseLightmass If TRUE, this lighting was done using offline rendering
	 */
	static void DumpLightMapsToDisk(const FGuid& MappingGuid, const FString& Description, FLightMapData1D* LightMapData, TMap<ULightComponent*,FShadowMapData1D*>& ShadowMaps, UBOOL bUseLightmass);

	/** Marks all lights used in the calculated lightmap as used in a lightmap, and calls Apply on the texture mapping. */
	void ApplyMapping(
		FStaticLightingTextureMapping* TextureMapping,
		FLightMapData2D* LightMapData, 
		const TMap<ULightComponent*,FShadowMapData2D*>& ShadowMapData, 
		FQuantizedLightmapData* QuantizedData) const;

	/** Marks all lights used in the calculated lightmap as used in a lightmap, and calls Apply on the vertex mapping. */
	void ApplyMapping(
		FStaticLightingVertexMapping* VertexMapping,
		FLightMapData1D* LightMapData, 
		const TMap<ULightComponent*,FShadowMapData1D*>& ShadowMapData, 
		FQuantizedLightmapData* QuantizedData) const;

private:

	/** A thread which processes static lighting mappings. */
	class FStaticLightingThreadRunnable : public FRunnable
	{
	public:

		FRunnableThread* Thread;

		/** Initialization constructor. */
		FStaticLightingThreadRunnable(FStaticLightingSystem* InSystem):
			System(InSystem),
			bTerminatedByError(FALSE)
		{}

		/** Checks the thread's health, and passes on any errors that have occured.  Called by the main thread. */
		void CheckHealth() const;

		// FRunnable interface.
		virtual UBOOL Init(void) { return TRUE; }
		virtual void Exit(void) {}
		virtual void Stop(void) {}
		virtual DWORD Run(void);

	private:
		FStaticLightingSystem* System;

		/** If the thread has been terminated by an unhandled exception, this contains the error message. */
		FString ErrorMessage;

		/** TRUE if the thread has been terminated by an unhandled exception. */
		UBOOL bTerminatedByError;
	};

	/** The static lighting data for a vertex mapping. */
	struct FVertexMappingStaticLightingData
	{
		FStaticLightingVertexMapping* Mapping;
		FLightMapData1D* LightMapData;
		TMap<ULightComponent*,FShadowMapData1D*> ShadowMaps;
	};

	/** The static lighting data for a texture mapping. */
	struct FTextureMappingStaticLightingData
	{
		FStaticLightingTextureMapping* Mapping;
		FLightMapData2D* LightMapData;
		TMap<ULightComponent*,FShadowMapData2D*> ShadowMaps;
	};

	/** Encapsulates a list of mappings which static lighting has been computed for, but not yet applied. */
	template<typename StaticLightingDataType>
	class TCompleteStaticLightingList
	{	public:

		/** Initialization constructor. */
		TCompleteStaticLightingList(const FStaticLightingSystem& InSystem):
			System(InSystem),
			FirstElement(NULL)
		{}

		/** Adds an element to the list. */
		void AddElement(TList<StaticLightingDataType>* Element)
		{
			// Link the element at the beginning of the list.
			TList<StaticLightingDataType>* LocalFirstElement;
			do 
			{
				LocalFirstElement = FirstElement;
				Element->Next = LocalFirstElement;
			}
			while(appInterlockedCompareExchangePointer((void**)&FirstElement,Element,LocalFirstElement) != LocalFirstElement);
		}

		/** 
		 *	Grab the head of the list.
		 *	NOTE: This is not thread-safe, but when it will be used should be fine...
		 */
		TList<StaticLightingDataType>* GetFirstElement()
		{
			// Link the element at the beginning of the list.
			TList<StaticLightingDataType>* LocalFirstElement;
			do 
			{
				LocalFirstElement = FirstElement;
			}
			while(appInterlockedCompareExchangePointer((void**)&FirstElement,LocalFirstElement,LocalFirstElement) != LocalFirstElement);
			return LocalFirstElement;
		}

		/**
		 * Applies the static lighting to the mappings in the list, and clears the list. 
		 *
		 * @param bUseLightmass If TRUE, this lighting was done using offline rendering
		 * @param bDumpBinaryResults If TRUE, lightmap/shadowmap data will be dumped out to the Logs\Lighting directory
		 */
		void ApplyAndClear(UBOOL bUseLightmass, UBOOL bDumpBinaryResults);

		/**
		 *	Empty out the list...
		 */
		void Clear();

	private:

		const FStaticLightingSystem& System;
		TList<StaticLightingDataType>* FirstElement;
	};

public:
	/**
	 *	Retrieve the mapping lighting data for the given mapping...
	 */
	FVertexMappingStaticLightingData* FStaticLightingSystem::GetVertexMappingElement(FStaticLightingVertexMapping* VertexMapping);
	FTextureMappingStaticLightingData* FStaticLightingSystem::GetTextureMappingElement(FStaticLightingTextureMapping* TextureMapping);

private:

	/** The lights in the world which the system is building. */
	TArray<ULightComponent*> Lights;

	/** Map from Light's LightmapGuid to the associated Light. */
	TMap<FGuid,ULightComponent*> GuidToLightMap;

	/** The options the system is building lighting with. */
	const FLightingBuildOptions Options;

	/** TRUE if the static lighting build has been canceled.  Written by the main thread, read by all static lighting threads. */
	UBOOL bBuildCanceled;

	/** Whether to allow multiple threads for static lighting. */
	UBOOL bAllowMultiThreadedStaticLighting;

	/** Whether to allow multiple samples approximating convolving the triangle by the bilinear filter used to sample the light-map texture. */
	UBOOL bAllowBilinearTexelRasterization;

	/** The aggregate mesh used for raytracing. Only needed for UE3 lighting builds. */
	FStaticLightingAggregateMesh AggregateMesh;

	/** A bound of all meshes being lit - used to check the ImportanceVolume when building with Lightmass */
	FBox LightingMeshBounds;

	/** All meshes in the system. */
	TArray< TRefCountPtr<FStaticLightingMesh> > Meshes;

	/** All mappings in the system. */
	TArray< TRefCountPtr<FStaticLightingMapping> > Mappings;

	TArray<FStaticLightingMappingSortHelper> UnSortedMappings;

	/** The next index into Mappings which processing hasn't started for yet. */
	FThreadSafeCounter NextMappingToProcess;

	/** A list of the vertex mappings which static lighting has been computed for, but not yet applied.  This is accessed by multiple threads and should be written to using interlocked functions. */
	TCompleteStaticLightingList<FVertexMappingStaticLightingData> CompleteVertexMappingList;

	/** A list of the texture mappings which static lighting has been computed for, but not yet applied.  This is accessed by multiple threads and should be written to using interlocked functions. */
	TCompleteStaticLightingList<FTextureMappingStaticLightingData> CompleteTextureMappingList;

	/** The threads spawned by the static lighting system. */
	TIndirectArray<FStaticLightingThreadRunnable> Threads;

	/** Lightmass statistics */
	FLightmassStatistics LightmassStatistics;

	/** The current index for deterministic lighting */
	INT DeterministicIndex;

	INT NextVisibilityId;

	/** The 'custom' importance bounds when building selected objects/levels. */
	FBox CustomImportanceBoundingBox;

	/** 
	 *	If TRUE, then pass the CustomImportanceVolume to Lightmass.
	 *	Will initialize to FALSE, but get set to TRUE if any mapping is
	 *	skipped during the setup phase of the scene.
	 */
	UBOOL bUseCustomImportanceVolume;

	/**
	 * Generates mappings/meshes for all BSP in the given level
	 *
	 * @param Level Level to build BSP lighting info for
	 * @param bBuildLightingForBSP If TRUE, we need BSP mappings generated as well as the meshes
	 */
	void AddBSPStaticLightingInfo(ULevel* Level, UBOOL bBuildLightingForBSP);

	/**
	 * Generates mappings/meshes for the given NodeGroups
	 *
	 * @param Level					Level to build BSP lighting info for
	 * @param NodeGroupsToBuild		The node groups to build the BSP lighting info for
	 */
	void AddBSPStaticLightingInfo(ULevel* Level, TArray<FNodeGroup*>& NodeGroupsToBuild);

	/** Queries a primitive for its static lighting info, and adds it to the system. */
	void AddPrimitiveStaticLightingInfo(FStaticLightingPrimitiveInfo& PrimitiveInfo, UBOOL bBuildActorLighting, UBOOL bAcceptsLights);

	/**
	 * Builds lighting for a vertex mapping.
	 * @param VertexMapping - The mapping to build lighting for.
	 */
	void ProcessVertexMapping(FStaticLightingVertexMapping* VertexMapping);

	/**
	* Builds lighting for a texture mapping.
	 * @param TextureMapping - The mapping to build lighting for.
	 */
	void ProcessTextureMapping(FStaticLightingTextureMapping* TextureMapping);

	/**
	 * The processing loop for a static lighting thread.
	 * @param bIsMainThread - TRUE if this is running in the main thread.
	 */
	void ThreadLoop(UBOOL bIsMainThread);

	/**
	 * Creates multiple worker threads and process locally multi-threaded.
	 **/
	void MultithreadProcess( UINT NumStaticLightingThreads );

	/**
	 * Exports the scene to Lightmass and process distributed.
	 * @return	TRUE if lighting was successful, otherwise FALSE
	 **/
	UBOOL LightmassProcess();

	/**
	 * Reports lighting build statistics to the log.
	 * @param NumStaticLightingThreads	Number of threads used for static lighting
	 */
	void ReportStatistics( INT NumStaticLightingThreads );
};

/** 
 * Types used for debugging static lighting.  
 * NOTE: These must remain binary compatible with the ones in Lightmass.
 */

/** Stores debug information about a static lighting ray. */
struct FDebugStaticLightingRay
{
	FVector4 Start;
	FVector4 End;
	UBOOL bHit;
	UBOOL bPositive;
};

struct FDebugStaticLightingVertex
{
	FDebugStaticLightingVertex() {}

	FDebugStaticLightingVertex(const FStaticLightingVertex& InVertex) :
		VertexNormal(InVertex.WorldTangentZ),
		VertexPosition(InVertex.WorldPosition)
	{}

	FVector4 VertexNormal;
	FVector4 VertexPosition;
};

struct FDebugLightingCacheRecord
{
	UBOOL bNearSelectedTexel;
	UBOOL bAffectsSelectedTexel;
	INT RecordId;
	FDebugStaticLightingVertex Vertex;
	FLOAT Radius;
};

struct FDebugPhoton
{
	INT Id;
	FVector4 Position;
	FVector4 Direction;
	FVector4 Normal;
};

struct FDebugOctreeNode
{
	FVector4 Center;
	FVector4 Extent;
};

struct FDebugVolumeLightingSample
{
	FVector4 Position;
	FLinearColor AverageIncidentRadiance;
};

static const INT NumTexelCorners = 4;

/** 
 * Debug output from the static lighting build.  
 * See Lightmass::FDebugLightingOutput for documentation.
 */
struct FDebugLightingOutput
{
	UBOOL bValid;
	TArray<FDebugStaticLightingRay> PathRays;
	TArray<FDebugStaticLightingRay> ShadowRays;
	TArray<FDebugStaticLightingRay> IndirectPhotonPaths;
	TArray<INT> SelectedVertexIndices;
	TArray<FDebugStaticLightingVertex> Vertices;
	TArray<FDebugLightingCacheRecord> CacheRecords;
	TArray<FDebugPhoton> DirectPhotons;
	TArray<FDebugPhoton> IndirectPhotons;
	TArray<FDebugPhoton> IrradiancePhotons;
	TArray<FDebugPhoton> GatheredCausticPhotons;
	TArray<FDebugPhoton> GatheredPhotons;
	TArray<FDebugPhoton> GatheredImportancePhotons;
	TArray<FDebugOctreeNode> GatheredPhotonNodes;
	TArray<FDebugVolumeLightingSample> VolumeLightingSamples;
	TArray<FDebugStaticLightingRay> PrecomputedVisibilityRays;
	UBOOL bDirectPhotonValid;
	FDebugPhoton GatheredDirectPhoton;
	FVector4 TexelCorners[NumTexelCorners];
	UBOOL bCornerValid[NumTexelCorners];
	FLOAT SampleRadius;

	FDebugLightingOutput() :
		bValid(FALSE),
		bDirectPhotonValid(FALSE)
	{}
};

/** Information about the lightmap sample that is selected */
extern FSelectedLightmapSample GCurrentSelectedLightmapSample;

/** Information about the last static lighting build */
extern FDebugLightingOutput GDebugStaticLightingInfo;

/** Updates GCurrentSelectedLightmapSample given a selected actor's components and the location of the click. */
extern void SetDebugLightmapSample(TArrayNoInit<UActorComponent*>* Components, UModel* Model, INT iSurf, FVector ClickLocation);

/** Renders debug elements for visualizing static lighting info */
extern void DrawStaticLightingDebugInfo(const FSceneView* View,FPrimitiveDrawInterface* PDI);

/** Renders debug elements for visualizing static lighting info */
extern void DrawStaticLightingDebugInfo(const FSceneView* View, FCanvas* Canvas);

#endif

#endif
