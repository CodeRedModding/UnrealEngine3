/*=============================================================================
	StaticLighting.h: Static lighting definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Forward declarations.
class FStaticLightingTextureMapping;
class FStaticLightingVertexMapping;
class FStaticLightingMapping;
class FLightMapData1D;
class FLightMapData2D;
class FShadowMapData1D;
class FShadowMapData2D;

/** The vertex data used to build static lighting. */
struct FStaticLightingVertex
{
	FVector WorldPosition;
	FVector WorldTangentX;
	FVector WorldTangentY;
	FVector WorldTangentZ;
	FVector2D TextureCoordinates[MAX_TEXCOORDS];

	// Operators used for linear combinations of static lighting vertices.

	friend FStaticLightingVertex operator+(const FStaticLightingVertex& A,const FStaticLightingVertex& B)
	{
		FStaticLightingVertex Result;
		Result.WorldPosition =	A.WorldPosition + B.WorldPosition;
		Result.WorldTangentX =	A.WorldTangentX + B.WorldTangentX;
		Result.WorldTangentY =	A.WorldTangentY + B.WorldTangentY;
		Result.WorldTangentZ =	A.WorldTangentZ + B.WorldTangentZ;
		for(INT CoordinateIndex = 0;CoordinateIndex < MAX_TEXCOORDS;CoordinateIndex++)
		{
			Result.TextureCoordinates[CoordinateIndex] = A.TextureCoordinates[CoordinateIndex] + B.TextureCoordinates[CoordinateIndex];
		}
		return Result;
	}

	friend FStaticLightingVertex operator-(const FStaticLightingVertex& A,const FStaticLightingVertex& B)
	{
		FStaticLightingVertex Result;
		Result.WorldPosition =	A.WorldPosition - B.WorldPosition;
		Result.WorldTangentX =	A.WorldTangentX - B.WorldTangentX;
		Result.WorldTangentY =	A.WorldTangentY - B.WorldTangentY;
		Result.WorldTangentZ =	A.WorldTangentZ - B.WorldTangentZ;
		for(INT CoordinateIndex = 0;CoordinateIndex < MAX_TEXCOORDS;CoordinateIndex++)
		{
			Result.TextureCoordinates[CoordinateIndex] = A.TextureCoordinates[CoordinateIndex] - B.TextureCoordinates[CoordinateIndex];
		}
		return Result;
	}

	friend FStaticLightingVertex operator*(const FStaticLightingVertex& A,FLOAT B)
	{
		FStaticLightingVertex Result;
		Result.WorldPosition =	A.WorldPosition * B;
		Result.WorldTangentX =	A.WorldTangentX * B;
		Result.WorldTangentY =	A.WorldTangentY * B;
		Result.WorldTangentZ =	A.WorldTangentZ * B;
		for(INT CoordinateIndex = 0;CoordinateIndex < MAX_TEXCOORDS;CoordinateIndex++)
		{
			Result.TextureCoordinates[CoordinateIndex] = A.TextureCoordinates[CoordinateIndex] * B;
		}
		return Result;
	}

	friend FStaticLightingVertex operator/(const FStaticLightingVertex& A,FLOAT B)
	{
		const FLOAT InvB = 1.0f / B;

		FStaticLightingVertex Result;
		Result.WorldPosition =	A.WorldPosition * InvB;
		Result.WorldTangentX =	A.WorldTangentX * InvB;
		Result.WorldTangentY =	A.WorldTangentY * InvB;
		Result.WorldTangentZ =	A.WorldTangentZ * InvB;
		for(INT CoordinateIndex = 0;CoordinateIndex < MAX_TEXCOORDS;CoordinateIndex++)
		{
			Result.TextureCoordinates[CoordinateIndex] = A.TextureCoordinates[CoordinateIndex] * InvB;
		}
		return Result;
	}
};

/** The result of an intersection between a light ray and the scene. */
class FLightRayIntersection
{
public:

	/** TRUE if the light ray intersected scene geometry. */
	BITFIELD bIntersects : 1;

	/** The differential geometry which the light ray intersected with. */
	FStaticLightingVertex IntersectionVertex;

	/** Initialization constructor. */
	FLightRayIntersection(UBOOL bInIntersects,const FStaticLightingVertex& InIntersectionVertex):
		bIntersects(bInIntersects),
		IntersectionVertex(InIntersectionVertex)
	{}

	/** No intersection constructor. */
	static FLightRayIntersection None() { return FLightRayIntersection(FALSE,FStaticLightingVertex()); }
};

/** A mesh which is used for computing static lighting. */
class FStaticLightingMesh : public virtual FRefCountedObject
{
public:

	/** The number of triangles in the mesh that will be used for visibility tests. */
	const INT NumTriangles;

	/** The number of shading triangles in the mesh. */
	const INT NumShadingTriangles;

	/** The number of vertices in the mesh that will be used for visibility tests. */
	const INT NumVertices;

	/** The number of shading vertices in the mesh. */
	const INT NumShadingVertices;

	/** The texture coordinate index which is used to parametrize materials. */
	const INT TextureCoordinateIndex;

	/** Used for precomputed visibility */
	TArray<INT> VisibilityIds;

	/** Whether the mesh casts a shadow. */
	const BITFIELD bCastShadow : 1;

	/** Whether the mesh only casts a shadow on itself. */
	const BITFIELD bSelfShadowOnly : 1;

	/** Whether the mesh uses a two-sided material. */
	const BITFIELD bTwoSidedMaterial : 1;

	/** The lights which affect the mesh's primitive. */
	const TArray<ULightComponent*> RelevantLights;

	/** The primitive component this mesh was created by. */
	const UPrimitiveComponent* const Component;

	/** The bounding box of the mesh. */
	FBox BoundingBox;

	/** Unique ID for tracking this lighting mesh during distributed lighting */
	FGuid Guid;

	/** Cached guid for the source mesh */
	FGuid SourceMeshGuid;

	/** Other FStaticLightingMesh's that should be considered the same mesh object (just different LOD), and should not shadow this LOD. */
	TArray<TRefCountPtr<FStaticLightingMesh> > OtherMeshLODs;

	/** Initialization constructor. */
	FStaticLightingMesh(
		INT InNumTriangles,
		INT InNumShadingTriangles,
		INT InNumVertices,
		INT InNumShadingVertices,
		INT InTextureCoordinateIndex,
		UBOOL bInCastShadow,
		UBOOL bInSelfShadowOnly,
		UBOOL bInTwoSidedMaterial,
		const TArray<ULightComponent*>& InRelevantLights,
		const UPrimitiveComponent* const InComponent,
		const FBox& InBoundingBox,
		const FGuid& InGuid
		);

	/** Virtual destructor. */
	virtual ~FStaticLightingMesh() {}

	/**
	 * Accesses a triangle.
	 * @param TriangleIndex - The triangle to access.
	 * @param OutV0 - Upon return, should contain the first vertex of the triangle.
	 * @param OutV1 - Upon return, should contain the second vertex of the triangle.
	 * @param OutV2 - Upon return, should contain the third vertex of the triangle.
     */
	virtual void GetTriangle(INT TriangleIndex,FStaticLightingVertex& OutV0,FStaticLightingVertex& OutV1,FStaticLightingVertex& OutV2) const = 0;

	/**
	 * Accesses a triangle for shading.
	 * @param TriangleIndex - The triangle to access, valid range is [0, NumShadingTriangles).
	 * @param OutV0 - Upon return, should contain the first vertex of the triangle.
	 * @param OutV1 - Upon return, should contain the second vertex of the triangle.
	 * @param OutV2 - Upon return, should contain the third vertex of the triangle.
	 * @param ElementIndex - Indicates the element index of the triangle.
     */
	virtual void GetShadingTriangle(INT TriangleIndex,FStaticLightingVertex& OutV0,FStaticLightingVertex& OutV1,FStaticLightingVertex& OutV2) const
	{
		checkSlow(NumTriangles == NumShadingTriangles);
		// By default the geometry used for shading is the same as the geometry used for visibility testing.
		GetTriangle(TriangleIndex, OutV0, OutV1, OutV2);
	}

	/**
	 * Accesses a triangle's vertex indices.
	 * @param TriangleIndex - The triangle to access.
	 * @param OutI0 - Upon return, should contain the first vertex index of the triangle.
	 * @param OutI1 - Upon return, should contain the second vertex index of the triangle.
	 * @param OutI2 - Upon return, should contain the third vertex index of the triangle.
	 */
	virtual void GetTriangleIndices(INT TriangleIndex,INT& OutI0,INT& OutI1,INT& OutI2) const = 0;

	/**
	 * Accesses a triangle's vertex indices for shading.
	 * @param TriangleIndex - The triangle to access, valid range is [0, NumShadingTriangles).
	 * @param OutI0 - Upon return, should contain the first vertex index of the triangle.
	 * @param OutI1 - Upon return, should contain the second vertex index of the triangle.
	 * @param OutI2 - Upon return, should contain the third vertex index of the triangle.
	 */
	virtual void GetShadingTriangleIndices(INT TriangleIndex,INT& OutI0,INT& OutI1,INT& OutI2) const
	{ 
		checkSlow(NumTriangles == NumShadingTriangles);
		// By default the geometry used for shading is the same as the geometry used for visibility testing.
		GetTriangleIndices(TriangleIndex, OutI0, OutI1, OutI2);
	}

	/**
	 * Determines whether the mesh should cast a shadow from a specific light on a specific mapping.
	 * This doesn't determine if the mesh actually shadows the receiver, just whether it should be allowed to.
	 * @param Light - The light source.
	 * @param Receiver - The mapping which is receiving the light.
	 * @return TRUE if the mesh should shadow the receiver from the light.
	 */
	virtual UBOOL ShouldCastShadow(ULightComponent* Light,const FStaticLightingMapping* Receiver) const;

	/** @return		TRUE if the specified triangle casts a shadow. */
	virtual UBOOL IsTriangleCastingShadow(UINT TriangleIndex) const
	{
		return TRUE;
	}

	/** @return		TRUE if the mesh wants to control shadow casting per element rather than per mesh. */
	virtual UBOOL IsControllingShadowPerElement() const
	{
		return FALSE;
	}

	/**
	 * Checks whether ShouldCastShadow will return TRUE always.
	 * @return TRUE if ShouldCastShadow will return TRUE always. 
	 */
	virtual UBOOL IsUniformShadowCaster() const
	{
		return bCastShadow && !bSelfShadowOnly;
	}

	/**
	 * Checks if a line segment intersects the mesh.
	 * @param Start - The start point of the line segment.
	 * @param End - The end point of the line segment.
	 * @param bFindNearestIntersection - Whether the nearest intersection is needed, or any intersection.
	 * @return The intersection of the light-ray with the mesh.
	 */
	virtual FLightRayIntersection IntersectLightRay(const FVector& Start,const FVector& End,UBOOL bFindNearestIntersection) const = 0;

#if !CONSOLE
	/** 
	 * Export static lighting mesh instance data to an exporter 
	 * @param Exporter - export interface to process static lighting data
	 **/
	virtual void ExportMeshInstance(class FLightmassExporter* Exporter) const {}
#endif	//#if !CONSOLE

	/**
	 * Returns the Guid used for static lighting.
	 * @return FGuid that identifies the mapping
	 **/
	virtual const FGuid&	GetLightingGuid() const
	{
		return Guid;
	}
};

/** A mapping between world-space surfaces and a static lighting cache. */
class FStaticLightingMapping : public virtual FRefCountedObject
{
public:

	/** The mesh associated with the mapping. */
	class FStaticLightingMesh* Mesh;

	/** The object which owns the mapping. */
	UObject* const Owner;

	/** TRUE if light-maps to be used for the object's direct lighting. */
	const BITFIELD bForceDirectLightMap : 1;

	/** TRUE if the mapping should be processed by Lightmass. */
	BITFIELD bProcessMapping : 1;

	/** Initialization constructor. */
	FStaticLightingMapping(FStaticLightingMesh* InMesh,UObject* InOwner,UBOOL bInForceDirectLightMap):
		Mesh(InMesh),
		Owner(InOwner),
		bForceDirectLightMap(bInForceDirectLightMap),
		bProcessMapping(FALSE)
	{}

	/** Virtual destructor. */
	virtual ~FStaticLightingMapping() {}

	/** @return If the mapping is a texture mapping, returns a pointer to this mapping as a texture mapping.  Otherwise, returns NULL. */
	virtual FStaticLightingTextureMapping* GetTextureMapping()
	{
		return NULL;
	}

	/** @return TRUE if the mapping is a texture mapping.  Otherwise, returns FALSE. */
	virtual UBOOL IsTextureMapping() const
	{
		return FALSE;
	}

	/** @return If the mapping is a vertex mapping, returns a pointer to this mapping as a vertex mapping.  Otherwise, returns NULL. */
	virtual FStaticLightingVertexMapping* GetVertexMapping()
	{
		return NULL;
	}

	/** @return TRUE if the mapping is a vertex mapping.  Otherwise, returns FALSE. */
	virtual UBOOL IsVertexMapping() const
	{
		return FALSE;
	}

#if WITH_EDITOR
	virtual UBOOL DebugThisMapping() const = 0;

	/** 
	 * Export static lighting mapping instance data to an exporter 
	 * @param Exporter - export interface to process static lighting data
	 */
	virtual void ExportMapping(class FLightmassExporter* Exporter) {};
#endif	//WITH_EDITOR

	/**
	 * Returns the Guid used for static lighting.
	 * @return FGuid that identifies the mapping
	 */
	virtual const FGuid& GetLightingGuid() const
	{
		return Mesh->Guid;
	}

	virtual FString GetDescription() const
	{
		return FString(TEXT("Mapping"));
	}

	virtual INT GetTexelCount() const
	{
		return 0;
	}

#if WITH_EDITOR
	/**
	 *	@return	UOject*		The object that is mapped by this mapping
	 */
	virtual UObject* GetMappedObject() const
	{
		return Owner;
	}
#endif	//WITH_EDITOR
};

/**
 * Determines whether the mesh should cast a shadow from a specific light on a specific mapping.
 * This doesn't determine if the mesh actually shadows the receiver, just whether it should be allowed to.
 * @param Light - The light source.
 * @param Receiver - The mapping which is receiving the light.
 * @return TRUE if the mesh should shadow the receiver from the light.
 */
FORCEINLINE UBOOL FStaticLightingMesh::ShouldCastShadow(ULightComponent* Light,const FStaticLightingMapping* Receiver) const
{
	// If this is a shadow casting mesh, then it is allowed to cast a shadow on the receiver from this light.
	return bCastShadow && (!bSelfShadowOnly || Receiver->Mesh == this);
}

/** A mapping between world-space surfaces and static lighting cache textures. */
class FStaticLightingTextureMapping : public FStaticLightingMapping
{
public:

	/** The width of the static lighting textures used by the mapping. */
	const INT SizeX;

	/** The height of the static lighting textures used by the mapping. */
	const INT SizeY;

	/** The lightmap texture coordinate index which is used for the mapping. */
	const INT LightmapTextureCoordinateIndex;

	/** Whether to apply a bilinear filter to the sample or not. */
	const UBOOL bBilinearFilter;

	/** Initialization constructor. */
	FStaticLightingTextureMapping(FStaticLightingMesh* InMesh,UObject* InOwner,INT InSizeX,INT InSizeY,INT InLightmapTextureCoordinateIndex,UBOOL bInForceDirectLightMap,UBOOL bInBilinearFilter=TRUE);

	/**
	 * Called when the static lighting has been computed to apply it to the mapping's owner.
	 * This function is responsible for deleting LightMapData, ShadowMapData and QuantizedData.
	 * @param LightMapData - The light-map data which has been computed for the mapping.
	 * @param ShadowMapData - The shadow-map data which have been computed for the mapping.
	 */
	virtual void Apply(FLightMapData2D* LightMapData,const TMap<ULightComponent*,FShadowMapData2D*>& ShadowMapData, struct FQuantizedLightmapData* QuantizedData) = 0;

	// FStaticLightingMapping interface.
	virtual FStaticLightingTextureMapping* GetTextureMapping()
	{
		return this;
	}

	/** @return TRUE if the mapping is a texture mapping.  Otherwise, returns FALSE. */
	virtual UBOOL IsTextureMapping() const
	{
		return TRUE;
	}

#if WITH_EDITOR
	virtual UBOOL DebugThisMapping() const;
#endif	//WITH_EDITOR

	virtual FString GetDescription() const
	{
		return FString(TEXT("TextureMapping"));
	}

	virtual INT GetTexelCount() const
	{
		return (SizeX * SizeY);
	}
};

/** A mapping between world-space surfaces and static lighting cache vertex buffers. */
class FStaticLightingVertexMapping : public FStaticLightingMapping
{
public:

	/** Lighting will be sampled at a random number of samples/surface area proportional to this factor. */
	const FLOAT SampleToAreaRatio;

	/** TRUE to sample at vertices instead of on the surfaces. */
	const BITFIELD bSampleVertices : 1;

	/** Initialization constructor. */
	FStaticLightingVertexMapping(FStaticLightingMesh* InMesh,UObject* InOwner,UBOOL bInForceDirectLightMap,FLOAT InSampleToAreaRatio,UBOOL bInSampleVertices):
		FStaticLightingMapping(InMesh,InOwner,bInForceDirectLightMap),
		SampleToAreaRatio(InSampleToAreaRatio),
		bSampleVertices(bInSampleVertices)
	{}

	/**
	 * Called when the static lighting has been computed to apply it to the mapping's owner.
	 * This function is responsible for deleting LightMapData, ShadowMapData and QuantizedData.
	 * @param LightMapData - The light-map data which has been computed for the mapping.
	 * @param ShadowMapData - The shadow-map data which have been computed for the mapping.
	 */
	virtual void Apply(FLightMapData1D* LightMapData,const TMap<ULightComponent*,FShadowMapData1D*>& ShadowMapData, struct FQuantizedLightmapData* QuantizedData) = 0;

	// FStaticLightingMapping interface.
	virtual FStaticLightingVertexMapping* GetVertexMapping()
	{
		return this;
	}

	/** @return TRUE if the mapping is a vertex mapping.  Otherwise, returns FALSE. */
	virtual UBOOL IsVertexMapping() const
	{
		return TRUE;
	}

#if WITH_EDITOR
	virtual UBOOL DebugThisMapping() const;
#endif	//WITH_EDITOR

	virtual FString GetDescription() const
	{
		return FString(TEXT("VertexMapping"));
	}

	virtual INT GetTexelCount() const
	{
		return Mesh ? Mesh->NumVertices : 0;
	}
};

/** The info about an actor component which the static lighting system needs. */
struct FStaticLightingPrimitiveInfo
{
	FStaticLightingPrimitiveInfo() :
		VisibilityId(INDEX_NONE)
	{}
	INT VisibilityId;
	/** The primitive's meshes. */
	TArray< TRefCountPtr<FStaticLightingMesh> > Meshes;

	/** The primitive's static lighting mappings. */
	TArray< TRefCountPtr<FStaticLightingMapping> > Mappings;
};
