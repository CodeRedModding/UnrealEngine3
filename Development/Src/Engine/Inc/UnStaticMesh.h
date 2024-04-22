/*=============================================================================
	UnStaticMesh.h: Static mesh class definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _STATICMESH_H
#define _STATICMESH_H

/** 
 * Static mesh build version, forces any versions older than this to be rebuilt on load.  
 * Warning: Incrementing this effectively requires a content resave!
 * Do not use this version for native serialization backwards compatibility, instead use VER_LATEST_ENGINE/VER_LATEST_ENGINE_LICENSEE.
 */
#define STATICMESH_VERSION 18

/**
 * FStaticMeshTriangle
 *
 * @warning BulkSerialize: FStaticMeshTriangle is serialized as memory dump
 * See TArray::BulkSerialize for detailed description of implied limitations.
 */
struct FStaticMeshTriangle
{
	FVector		Vertices[3];
	FVector2D	UVs[3][8];
	FColor		Colors[3];
	INT			MaterialIndex;
	INT			FragmentIndex;
	DWORD		SmoothingMask;
	INT			NumUVs;

	FVector		TangentX[3]; // Tangent, U-direction
	FVector		TangentY[3]; // Binormal, V-direction
	FVector		TangentZ[3]; // Normal
	UBOOL		bOverrideTangentBasis;
	UBOOL		bExplicitNormals;

	/** IMPORTANT: If you add a new member to this structure, remember to update the many places throughout
	       the source tree that FStaticMeshTriangles are filled with data and copied around!  Because
		   triangle arrays are often allocated with appRealloc and blindly casted, there's no point adding
		   a constructor here for initializing members. */
};

/**
 * Bulk data array of FStaticMeshTriangle
 */
struct FStaticMeshTriangleBulkData : public FUntypedBulkData
{
	/**
	 * Returns size in bytes of single element.
	 *
	 * @return Size in bytes of single element
	 */
	virtual INT GetElementSize() const;

	/**
	 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
	 * 
	 * @warning BulkSerialize: FStaticMeshTriangle is serialized as memory dump
	 * See TArray::BulkSerialize for detailed description of implied limitations.
	 *
	 * @param Ar			Archive to serialize with
	 * @param Data			Base pointer to data
	 * @param ElementIndex	Element index to serialize
	 */
	virtual void SerializeElement( FArchive& Ar, void* Data, INT ElementIndex );

	/**
	 * Returns whether single element serialization is required given an archive. This e.g.
	 * can be the case if the serialization for an element changes and the single element
	 * serialization code handles backward compatibility.
	 */
	virtual UBOOL RequiresSingleElementSerialization( FArchive& Ar );
};



#ifdef VER_MESH_PAINT_SYSTEM
/** 
 * Legacy data structure used for mesh backwards compatibility only
 */
struct FLegacyStaticMeshFullVertex
{
	FPackedNormal TangentX;
	FPackedNormal TangentZ;
	FColor Color;	

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	*/
	void Serialize(FArchive& Ar)
	{
		Ar << TangentX;
		Ar << TangentZ;
		Ar << Color;
	}
};

/** 
* 16 bit UV version of static mesh vertex
*/
template<UINT NumTexCoords>
struct TLegacyStaticMeshFullVertexFloat16UVs : public FLegacyStaticMeshFullVertex
{
	FVector2DHalf UVs[NumTexCoords];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,TLegacyStaticMeshFullVertexFloat16UVs& Vertex)
	{
		Vertex.Serialize(Ar);
		for(UINT UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			Ar << Vertex.UVs[UVIndex];
		}
		return Ar;
	}
};

/** 
* 32 bit UV version of static mesh vertex
*/
template<UINT NumTexCoords>
struct TLegacyStaticMeshFullVertexFloat32UVs : public FLegacyStaticMeshFullVertex
{
	FVector2D UVs[NumTexCoords];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,TLegacyStaticMeshFullVertexFloat32UVs& Vertex)
	{
		Vertex.Serialize(Ar);
		for(UINT UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			Ar << Vertex.UVs[UVIndex];
		}
		return Ar;
	}
};

#else
	#error "VER_MESH_PAINT_SYSTEM is no longer defined so the code above should be deleted"
#endif


/** 
 * All information about a static-mesh vertex with a variable number of texture coordinates.
 * Position information is stored separately to reduce vertex fetch bandwidth in passes that only need position. (z prepass)
 */
struct FStaticMeshFullVertex
{
	FPackedNormal TangentX;
	FPackedNormal TangentZ;

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	*/
	void Serialize(FArchive& Ar)
	{
		Ar << TangentX;
		Ar << TangentZ;
	}
};

/** 
* 16 bit UV version of static mesh vertex
*/
template<UINT NumTexCoords>
struct TStaticMeshFullVertexFloat16UVs : public FStaticMeshFullVertex
{
	FVector2DHalf UVs[NumTexCoords];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,TStaticMeshFullVertexFloat16UVs& Vertex)
	{
		Vertex.Serialize(Ar);
		for(UINT UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			Ar << Vertex.UVs[UVIndex];
		}
		return Ar;
	}
};

/** 
* 32 bit UV version of static mesh vertex
*/
template<UINT NumTexCoords>
struct TStaticMeshFullVertexFloat32UVs : public FStaticMeshFullVertex
{
	FVector2D UVs[NumTexCoords];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,TStaticMeshFullVertexFloat32UVs& Vertex)
	{
		Vertex.Serialize(Ar);
		for(UINT UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			Ar << Vertex.UVs[UVIndex];
		}
		return Ar;
	}
};

/** The information used to build a static-mesh vertex. */
struct FStaticMeshBuildVertex
{
	FVector Position;
	FPackedNormal TangentX;
	FPackedNormal TangentY;
	FPackedNormal TangentZ;
	FVector2D UVs[MAX_TEXCOORDS];
	FColor Color;
	WORD FragmentIndex;
};

/**
* Identifies a single chunk of an index buffer
*/
struct FFragmentRange
{
	INT BaseIndex;
	INT NumPrimitives;

	friend FArchive& operator<<(FArchive& Ar,FFragmentRange& FragmentRange)
	{
		Ar << FragmentRange.BaseIndex << FragmentRange.NumPrimitives;
		return Ar;
	}
};

/**
 * Platform-specific mesh data
 */
class FPlatformStaticMeshData
{
public:
	virtual ~FPlatformStaticMeshData() = 0;
};

class FPS3StaticMeshData : public FPlatformStaticMeshData
{
public:
	~FPS3StaticMeshData()
	{}

	// All of these arrays must have the same number of elements -- one entry per partition/job.
	TArray<UINT> IoBufferSize;          // The SPURS I/O buffer size to use for each partition's job.
	TArray<UINT> ScratchBufferSize;     // The SPURS scratch buffer size to use for each partition's job.
	TArray<WORD> CommandBufferHoleSize; // The SPURS scratch buffer size to use for each partition's job.
	TArray<SWORD> IndexBias;            // A negative number for each partition, equal to the minimum value referenced in the partition's index buffer.
	TArray<WORD> VertexCount;           // Number of vertices included in each partition.
	TArray<WORD> TriangleCount;         // Number of triangles included in each partition.
	TArray<WORD> FirstVertex;           // The index within this MeshElement's vertex range where each partition begins. This is relative to the element's MinVertexIndex -- not an absolute index!
	TArray<WORD> FirstTriangle;         // The index within this MeshElement's primitive range where each partition begins. This is relative to the element's StartIndex -- not an absolute index!

	friend FArchive& operator<<(FArchive& Ar,FPS3StaticMeshData& MD)
	{
		Ar << MD.IoBufferSize
		   << MD.ScratchBufferSize
		   << MD.CommandBufferHoleSize
		   << MD.IndexBias
		   << MD.VertexCount
		   << MD.TriangleCount
		   << MD.FirstVertex
		   << MD.FirstTriangle;
		return Ar;
	}
};

/**
 * A set of static mesh triangles which are rendered with the same material.
 */
class FStaticMeshElement
{
public:

	UMaterialInterface*		Material;
	/** A work area to hold the imported name during ASE importing (transient, should not be serialized) */
	FString					Name;			

	UBOOL					EnableCollision,
							OldEnableCollision,
							bEnableShadowCasting;
	
	/** necessary data for the draw call */
	UINT					FirstIndex,
							NumTriangles,
							MinVertexIndex,
							MaxVertexIndex;

	/**
	 * The index used by a StaticMeshComponent to override this element's material.  This will be the index of the element
	 * in uncooked content, but after cooking may be different from the element index due to splitting elements for platform
	 * constraints.
	 */
	INT MaterialIndex;

	/** only required for fractured meshes */
	TArray<FFragmentRange> Fragments;

	FPlatformStaticMeshData*		PlatformData; // platform-specific data

	/** Constructor. */
	FStaticMeshElement():
		Material(NULL),
		EnableCollision(FALSE),
		OldEnableCollision(FALSE),
		bEnableShadowCasting(TRUE),
		FirstIndex(0),
		NumTriangles(0),
		MinVertexIndex(0),
		MaxVertexIndex(0),
		MaterialIndex(0),
		PlatformData(NULL)
	{}

	FStaticMeshElement(UMaterialInterface* InMaterial,UINT InMaterialIndex):
		Material(InMaterial),
		EnableCollision(TRUE),
		OldEnableCollision(TRUE),
		bEnableShadowCasting(TRUE),
		FirstIndex(0),
		NumTriangles(0),
		MinVertexIndex(0),
		MaxVertexIndex(0),
		MaterialIndex(InMaterialIndex),
		PlatformData(NULL)
	{
		EnableCollision = OldEnableCollision = 1;
	}

	~FStaticMeshElement()
	{
		delete PlatformData;
	}

	UBOOL operator==( const FStaticMeshElement& In ) const
	{
		return Material==In.Material;
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FStaticMeshElement& E)
	{
		Ar	<< E.Material
			<< E.EnableCollision
			<< E.OldEnableCollision
			<< E.bEnableShadowCasting
			<< E.FirstIndex
			<< E.NumTriangles
			<< E.MinVertexIndex
			<< E.MaxVertexIndex
			<< E.MaterialIndex;

		if (Ar.Ver() >= VER_STATICMESH_FRAGMENTINDEX)
		{
			Ar << E.Fragments;
		}

		if (Ar.Ver() >= VER_ADDED_PLATFORMMESHDATA)
		{
			if (Ar.IsLoading())
			{
				// Load a flag to indicate whether the platform-specific data should be allocated.
				BYTE LoadPlatformData = 0;
				Ar << LoadPlatformData;
				if (LoadPlatformData)
				{
					// this should only exist in cooked packages, since the cooker creates the mesh data object
					FPS3StaticMeshData *PlatformMeshData = new FPS3StaticMeshData;
					Ar << *PlatformMeshData;
					E.PlatformData = PlatformMeshData;
				}
			}
			else
			{
				BYTE LoadPlatformData = (E.PlatformData != NULL);
				Ar << LoadPlatformData;
				if (LoadPlatformData)
				{
					FPS3StaticMeshData *PlatformMeshData = (FPS3StaticMeshData*)E.PlatformData;
					Ar << *PlatformMeshData;
				}
			}
		}

		return Ar;
	}
};

/** An interface to the static-mesh vertex data storage type. */
class FStaticMeshVertexDataInterface
{
public:

	/** Virtual destructor. */
	virtual ~FStaticMeshVertexDataInterface() {}

	/**
	* Resizes the vertex data buffer, discarding any data which no longer fits.
	* @param NumVertices - The number of vertices to allocate the buffer for.
	*/
	virtual void ResizeBuffer(UINT NumVertices) = 0;

	/** @return The stride of the vertex data in the buffer. */
	virtual UINT GetStride() const = 0;

	/** @return A pointer to the data in the buffer. */
	virtual BYTE* GetDataPointer() = 0;

	/** @return A pointer to the FResourceArrayInterface for the vertex data. */
	virtual FResourceArrayInterface* GetResourceArray() = 0;

	/** Serializer. */
	virtual void Serialize(FArchive& Ar) = 0;
};

/** A vertex that stores just position. */
struct FPositionVertex
{
	FVector	Position;

	friend FArchive& operator<<(FArchive& Ar,FPositionVertex& V)
	{
		Ar << V.Position;
		return Ar;
	}
};

/** A vertex buffer of positions. */
class FPositionVertexBuffer : public FVertexBuffer
{
public:

	/** Default constructor. */
	FPositionVertexBuffer();

	/** Destructor. */
	~FPositionVertexBuffer();

	/** Delete existing resources */
	void CleanUp();

	/**
	* Initializes the buffer with the given vertices, used to convert legacy layouts.
	* @param InVertices - The vertices to initialize the buffer with.
	*/
	void Init(const TArray<FStaticMeshBuildVertex>& InVertices);

	/**
	 * Initializes this vertex buffer with the contents of the given vertex buffer.
	 * @param InVertexBuffer - The vertex buffer to initialize from.
	 */
	void Init(const FPositionVertexBuffer& InVertexBuffer);

	/**
	* Removes the cloned vertices used for extruding shadow volumes.
	* @param NumVertices - The real number of static mesh vertices which should remain in the buffer upon return.
	*/
	void RemoveLegacyShadowVolumeVertices(UINT InNumVertices);

	/**
	* Serializer
	*
	* @param	Ar				Archive to serialize with
	* @param	bNeedsCPUAccess	Whether the elements need to be accessed by the CPU
	*/
	void Serialize( FArchive& Ar, UBOOL bNeedsCPUAccess );

	/**
	* Specialized assignment operator, only used when importing LOD's. 
	*/
	void operator=(const FPositionVertexBuffer &Other);

	// Vertex data accessors.
	FORCEINLINE FVector& VertexPosition(UINT VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FPositionVertex*)(Data + VertexIndex * Stride))->Position;
	}
	FORCEINLINE const FVector& VertexPosition(UINT VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FPositionVertex*)(Data + VertexIndex * Stride))->Position;
	}
	// Other accessors.
	FORCEINLINE UINT GetStride() const
	{
		return Stride;
	}
	FORCEINLINE UINT GetNumVertices() const
	{
		return NumVertices;
	}

	// FRenderResource interface.
	virtual void InitRHI();
	virtual FString GetFriendlyName() const { return TEXT("PositionOnly Static-mesh vertices"); }

private:

	/** The vertex data storage type */
	class FPositionVertexData* VertexData;

	/** The cached vertex data pointer. */
	BYTE* Data;

	/** The cached vertex stride. */
	UINT Stride;

	/** The cached number of vertices. */
	UINT NumVertices;

	/** Allocates the vertex data storage type. */
	void AllocateData( UBOOL bNeedsCPUAccess = TRUE );
};

/**
* A vertex buffer of colors. 
* needs to match FColorVertexBuffer_Mirror in unreal script
*/
class FColorVertexBuffer : public FVertexBuffer
{
public:

	/** Default constructor. */
	FColorVertexBuffer();

	/** Destructor. */
	~FColorVertexBuffer();

	/** Delete existing resources */
	void CleanUp();

	/**
	* Initializes the buffer with the given vertices, used to convert legacy layouts.
	* @param InVertices - The vertices to initialize the buffer with.
	*/
	void Init(const TArray<FStaticMeshBuildVertex>& InVertices);

	/**
	 * Initializes this vertex buffer with the contents of the given vertex buffer.
	 * @param InVertexBuffer - The vertex buffer to initialize from.
	 */
	void Init(const FColorVertexBuffer& InVertexBuffer);

	/**
	* Removes the cloned vertices used for extruding shadow volumes.
	* @param NumVertices - The real number of static mesh vertices which should remain in the buffer upon return.
	*/
	void RemoveLegacyShadowVolumeVertices(UINT InNumVertices);

	/**
	* Serializer
	*
	* @param	Ar					Archive to serialize with
	* @param	bInNeedsCPUAccess	Whether the elements need to be accessed by the CPU
	*/
	void Serialize( FArchive& Ar, UBOOL bNeedsCPUAccess );
	
	/** 
	* Export the data to a string, used for editor Copy&Paste.
	* The method must not be called if there is no data.
	*/
	void ExportText(FString &ValueStr) const;
	
	/**
	* Export the data from a string, used for editor Copy&Paste.
	* @param SourceText - must not be 0
	*/
	void ImportText(const TCHAR* SourceText);

	/**
	* Specialized assignment operator, only used when importing LOD's. 
	*/
	void operator=(const FColorVertexBuffer &Other);

	FORCEINLINE FColor& VertexColor(UINT VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return *(FColor*)(Data + VertexIndex * Stride);
	}

	FORCEINLINE const FColor& VertexColor(UINT VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return *(FColor*)(Data + VertexIndex * Stride);
	}

	// Other accessors.
	FORCEINLINE UINT GetStride() const
	{
		return Stride;
	}
	FORCEINLINE UINT GetNumVertices() const
	{
		return NumVertices;
	}

	/** Useful for memory profiling. */
	UINT GetAllocatedSize() const;

	/**
	* Load from a array of colors
	* @param InColors - must not be 0
	* @param Count - must be > 0
	* @param Stride - in bytes, usually sizeof(FColor) but can be 0 to use a single input color or larger.
	*/
	void InitFromColorArray( const FColor *InColors, UINT Count, UINT Stride = sizeof(FColor) );

	/**
	* Load from raw color array.
	* @param InColors - InColors must not be empty
	*/
	void InitFromColorArray( const TArray<FColor> &InColors )
	{
		InitFromColorArray(InColors.GetData(), InColors.Num());
	}

	/**
	* Load from single color.
	* @param Count - must be > 0
	*/
	void InitFromSingleColor( const FColor &InColor, UINT Count )
	{
		InitFromColorArray(&InColor, Count, 0);
	}

	/** Load from legacy vertex data */
	void InitFromLegacyData( const class FLegacyStaticMeshVertexBuffer& InLegacyData );

	// FRenderResource interface.
	virtual void InitRHI();
	virtual FString GetFriendlyName() const { return TEXT("ColorOnly Static-mesh vertices"); }

private:

	/** The vertex data storage type */
	class FColorVertexData* VertexData;

	/** The cached vertex data pointer. */
	BYTE* Data;

	/** The cached vertex stride. */
	UINT Stride;

	/** The cached number of vertices. */
	UINT NumVertices;

	/** Allocates the vertex data storage type. */
	void AllocateData( UBOOL bNeedsCPUAccess = TRUE );

	/** Purposely hidden */
	FColorVertexBuffer( const FColorVertexBuffer &rhs );
};



/** Legacy vertex buffer for a static mesh LOD */
class FLegacyStaticMeshVertexBuffer : public FVertexBuffer
{
public:

	/** Default constructor. */
	FLegacyStaticMeshVertexBuffer();

	/** Destructor. */
	~FLegacyStaticMeshVertexBuffer();

	/** Delete existing resources */
	void CleanUp();

	/**
	* Serializer
	*
	* @param	Ar					Archive to serialize with
	* @param	bInNeedsCPUAccess	Whether the elements need to be accessed by the CPU
	*/
	void Serialize( FArchive& Ar, UBOOL bNeedsCPUAccess );

	FORCEINLINE const FPackedNormal& VertexTangentX(UINT VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FLegacyStaticMeshFullVertex*)(Data + VertexIndex * Stride))->TangentX;
	}

	FORCEINLINE const FPackedNormal& VertexTangentZ(UINT VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FLegacyStaticMeshFullVertex*)(Data + VertexIndex * Stride))->TangentZ;
	}

	FORCEINLINE const FColor& VertexColor(UINT VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FLegacyStaticMeshFullVertex*)(Data + VertexIndex * Stride))->Color;
	}

	/**
	* Get the vertex UV values at the given index in the vertex buffer
	*
	* @param VertexIndex - index into the vertex buffer
	* @param UVIndex - [0,MAX_TEXCOORDS] value to index into UVs array
	* @param 2D UV values
	*/
	FORCEINLINE FVector2D GetVertexUV(UINT VertexIndex,UINT UVIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		if( !bUseFullPrecisionUVs )
		{
			return ((TLegacyStaticMeshFullVertexFloat16UVs<MAX_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex];
		}
		else
		{
			return ((TLegacyStaticMeshFullVertexFloat32UVs<MAX_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex];
		}		
	}

	// Other accessors.
	FORCEINLINE UINT GetStride() const
	{
		return Stride;
	}
	FORCEINLINE UINT GetNumVertices() const
	{
		return NumVertices;
	}
	FORCEINLINE UINT GetNumTexCoords() const
	{
		return NumTexCoords;
	}
	FORCEINLINE UBOOL GetUseFullPrecisionUVs() const
	{
		return bUseFullPrecisionUVs;
	}
	FORCEINLINE void SetUseFullPrecisionUVs(UBOOL UseFull)
	{
		bUseFullPrecisionUVs = UseFull;
	}
	const BYTE* GetRawVertexData() const
	{
		check( Data != NULL );
		return Data;
	}

private:

	/** The vertex data storage type */
	FStaticMeshVertexDataInterface* VertexData;

	/** The number of texcoords/vertex in the buffer. */
	UINT NumTexCoords;

	/** The cached vertex data pointer. */
	BYTE* Data;

	/** The cached vertex stride. */
	UINT Stride;

	/** The cached number of vertices. */
	UINT NumVertices;

	/** Corresponds to UStaticMesh::UseFullPrecisionUVs. if TRUE then 32 bit UVs are used */
	UBOOL bUseFullPrecisionUVs;

	/** Allocates the vertex data storage type. */
	void AllocateData( UBOOL bNeedsCPUAccess = TRUE );
};

/** Vertex buffer for a static mesh LOD */
class FStaticMeshVertexBuffer : public FVertexBuffer
{
public:

	/** Default constructor. */
	FStaticMeshVertexBuffer();

	/** Destructor. */
	~FStaticMeshVertexBuffer();

	/** Delete existing resources */
	void CleanUp();

	/**
	 * Initializes the buffer with the given vertices.
	 * @param InVertices - The vertices to initialize the buffer with.
	 * @param InNumTexCoords - The number of texture coordinate to store in the buffer.
	 */
	void Init(const TArray<FStaticMeshBuildVertex>& InVertices,UINT InNumTexCoords);

	/**
	 * Initializes this vertex buffer with the contents of the given vertex buffer.
	 * @param InVertexBuffer - The vertex buffer to initialize from.
	 */
	void Init(const FStaticMeshVertexBuffer& InVertexBuffer);

	/**
	 * Removes the cloned vertices used for extruding shadow volumes.
	 * @param NumVertices - The real number of static mesh vertices which should remain in the buffer upon return.
	 */
	void RemoveLegacyShadowVolumeVertices(UINT NumVertices);

	/**
	* Serializer
	*
	* @param	Ar				Archive to serialize with
	* @param	bNeedsCPUAccess	Whether the elements need to be accessed by the CPU
	*/
	void Serialize( FArchive& Ar, UBOOL bNeedsCPUAccess );

	/**
	* Specialized assignment operator, only used when importing LOD's. 
	*/
	void operator=(const FStaticMeshVertexBuffer &Other);

	FORCEINLINE FPackedNormal& VertexTangentX(UINT VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FStaticMeshFullVertex*)(Data + VertexIndex * Stride))->TangentX;
	}
	FORCEINLINE const FPackedNormal& VertexTangentX(UINT VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FStaticMeshFullVertex*)(Data + VertexIndex * Stride))->TangentX;
	}

	/**
	* Calculate the binormal (TangentY) vector using the normal,tangent vectors
	*
	* @param VertexIndex - index into the vertex buffer
	* @return binormal (TangentY) vector
	*/
	FORCEINLINE FVector VertexTangentY(UINT VertexIndex) const
	{
		const FPackedNormal& TangentX = VertexTangentX(VertexIndex);
		const FPackedNormal& TangentZ = VertexTangentZ(VertexIndex);
		return (FVector(TangentZ) ^ FVector(TangentX)) * ((FLOAT)TangentZ.Vector.W  / 127.5f - 1.0f);
	}

	FORCEINLINE FPackedNormal& VertexTangentZ(UINT VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FStaticMeshFullVertex*)(Data + VertexIndex * Stride))->TangentZ;
	}
	FORCEINLINE const FPackedNormal& VertexTangentZ(UINT VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FStaticMeshFullVertex*)(Data + VertexIndex * Stride))->TangentZ;
	}

	/**
	* Set the vertex UV values at the given index in the vertex buffer
	*
	* @param VertexIndex - index into the vertex buffer
	* @param UVIndex - [0,MAX_TEXCOORDS] value to index into UVs array
	* @param Vec2D - UV values to set
	*/
	FORCEINLINE void SetVertexUV(UINT VertexIndex,UINT UVIndex,const FVector2D& Vec2D)
	{
		checkSlow(VertexIndex < GetNumVertices());
		if( !bUseFullPrecisionUVs )
		{
			((TStaticMeshFullVertexFloat16UVs<MAX_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex] = Vec2D;
		}
		else
		{
			((TStaticMeshFullVertexFloat32UVs<MAX_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex] = Vec2D;
		}		
	}

	/**
	* Fet the vertex UV values at the given index in the vertex buffer
	*
	* @param VertexIndex - index into the vertex buffer
	* @param UVIndex - [0,MAX_TEXCOORDS] value to index into UVs array
	* @param 2D UV values
	*/
	FORCEINLINE FVector2D GetVertexUV(UINT VertexIndex,UINT UVIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		if( !bUseFullPrecisionUVs )
		{
			return ((TStaticMeshFullVertexFloat16UVs<MAX_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex];
		}
		else
		{
			return ((TStaticMeshFullVertexFloat32UVs<MAX_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex];
		}		
	}

	// Other accessors.
	FORCEINLINE UINT GetStride() const
	{
		return Stride;
	}
	FORCEINLINE UINT GetNumVertices() const
	{
		return NumVertices;
	}
	FORCEINLINE UINT GetNumTexCoords() const
	{
		return NumTexCoords;
	}
	FORCEINLINE UBOOL GetUseFullPrecisionUVs() const
	{
		return bUseFullPrecisionUVs;
	}
	FORCEINLINE void SetUseFullPrecisionUVs(UBOOL UseFull)
	{
		bUseFullPrecisionUVs = UseFull;
	}
	const BYTE* GetRawVertexData() const
	{
		check( Data != NULL );
		return Data;
	}

	/**
	* Convert the existing data in this mesh from 16 bit to 32 bit UVs.
	* Without rebuilding the mesh (loss of precision)
	*/
	template<INT NumTexCoords>
	void ConvertToFullPrecisionUVs();

	/** Load from legacy vertex data */
	void InitFromLegacyData( const FLegacyStaticMeshVertexBuffer& InLegacyData );

	// FRenderResource interface.
	virtual void InitRHI();
	virtual FString GetFriendlyName() const { return TEXT("Static-mesh vertices"); }

private:

	/** The vertex data storage type */
	FStaticMeshVertexDataInterface* VertexData;

	/** The number of texcoords/vertex in the buffer. */
	UINT NumTexCoords;

	/** The cached vertex data pointer. */
	BYTE* Data;

	/** The cached vertex stride. */
	UINT Stride;

	/** The cached number of vertices. */
	UINT NumVertices;

	/** Corresponds to UStaticMesh::UseFullPrecisionUVs. if TRUE then 32 bit UVs are used */
	UBOOL bUseFullPrecisionUVs;

	/** Allocates the vertex data storage type. */
	void AllocateData( UBOOL bNeedsCPUAccess = TRUE );
};

/** 
* A vertex that stores a shadow volume extrusion info. 
*/
struct FLegacyShadowExtrusionVertex
{
	FLOAT ShadowExtrusionPredicate;

	friend FArchive& operator<<(FArchive& Ar,FLegacyShadowExtrusionVertex& V)
	{
		Ar << V.ShadowExtrusionPredicate;
		return Ar;
	}
};

class FLegacyExtrusionVertexBuffer
{
public:

	friend FArchive& operator<<(FArchive& Ar,FLegacyExtrusionVertexBuffer& VertexBuffer);
};

#include "UnkDOP.h"

/**
* FStaticMeshRenderData - All data to define rendering properties for a certain LOD model for a mesh.
*/
class FStaticMeshRenderData
{
public:

	/** The buffer containing vertex data. */
	FStaticMeshVertexBuffer VertexBuffer;
	/** The buffer containing the position vertex data. */
	FPositionVertexBuffer PositionVertexBuffer;
	/** The buffer containing the vertex color data. */
	FColorVertexBuffer ColorVertexBuffer;

	/** The number of vertices in the LOD. */
	UINT NumVertices;

	/** If True, data of a static mesh is stored in video memory. Only usable on Playstation 3. */
	UBOOL bNeedsCPUAccess;

	/** Index buffer resource for rendering */
	FRawStaticIndexBuffer					IndexBuffer;
	/** Index buffer resource for rendering wireframe mode */
	FRawIndexBuffer							WireframeIndexBuffer;
	/** The collection of sub-meshes to render (needed for multi-material support) */
	TArray<FStaticMeshElement>				Elements;
	/** Source data for mesh */
	FStaticMeshTriangleBulkData				RawTriangles;

	/** Resources neede to render the model with PN-AEN */
	FRawStaticIndexBuffer					AdjacencyIndexBuffer;

	/**
	 * Special serialize function passing the owning UObject along as required by FUntypedBulkData
	 * serialization.
	 *
	 * @param	Ar		Archive to serialize with
	 * @param	Owner	UObject this structure is serialized within
	 * @param	Idx		Index of current array entry being serialized
	 */
	void Serialize( FArchive& Ar, UObject* Owner, INT Idx );

	/** Constructor */
	FStaticMeshRenderData();

	/** @return The triangle count of this LOD. */
	INT GetTriangleCount() const;

	/**
	* Fill an array with triangles which will be used to build a KDOP tree
	* @param kDOPBuildTriangles - the array to fill
	*/
	void GetKDOPTriangles(TArray<FkDOPBuildCollisionTriangle<WORD> >& kDOPBuildTriangles);

	/**
	* Build rendering data from a raw triangle stream
	* @param kDOPBuildTriangles output collision tree. A dummy can be passed if you do not specify BuildKDop as TRUE
	* @param Whether to build and return a kdop tree from the mesh data
	* @param Parent Parent mesh
	* @param bRemoveDegenerates	Whether to remove degenerate triangles or keep them
	*/
	void Build(TArray<FkDOPBuildCollisionTriangle<WORD> >& kDOPBuildTriangles, UBOOL BuildKDop, class UStaticMesh* Parent, UBOOL bSingleTangentSetOverride, UBOOL bRemoveDegenerates, UBOOL bSilent);

	/**
	 * Initialize the LOD's render resources.
	 * @param Parent Parent mesh
	 */
	void InitResources(class UStaticMesh* Parent);

	/** Releases the LOD's render resources. */
	void ReleaseResources();


	/**
	 * Configures a vertex factory for rendering this static mesh
	 *
	 * @param	InOutVertexFactory				The vertex factory to configure
	 * @param	InParentMesh					Parent static mesh
	 * @param	InOverrideColorVertexBuffer		Optional color vertex buffer to use *instead* of the color vertex stream associated with this static mesh
	 */
	void SetupVertexFactory( FLocalVertexFactory& InOutVertexFactory, UStaticMesh* InParentMesh, FColorVertexBuffer* InOverrideColorVertexBuffer );

	// Rendering data.
	FLocalVertexFactory VertexFactory;
};

/** Used to expose information in the editor for one section or a particular LOD. */
struct FStaticMeshLODElement
{
	/** Material to use for this section of this LOD. */
	UMaterialInterface*	Material;

	/** Whether to enable shadow casting for this section of this LOD. */
	UBOOL bEnableShadowCasting;

	/** Whether or not this element is selected. */
	UBOOL bSelected;

	/** Whether to enable collision for this section of this LOD */
	BITFIELD bEnableCollision:1;

	friend FArchive& operator<<(FArchive& Ar,FStaticMeshLODElement& LODElement)
	{
		// For GC - serialise pointer to material
		if( !Ar.IsLoading() && !Ar.IsSaving() )
		{
			Ar << LODElement.Material;
		}
		return Ar;
	}
};

/**
 * FStaticMeshLODInfo - Editor-exposed properties for a specific LOD
 */
struct FStaticMeshLODInfo
{
	/** Used to expose properties for each */
	TArray<FStaticMeshLODElement>			Elements;

	friend FArchive& operator<<(FArchive& Ar,FStaticMeshLODInfo& LODInfo)
	{
		// For GC - serialise pointer to materials
		if( !Ar.IsLoading() && !Ar.IsSaving() )
		{
			Ar << LODInfo.Elements;
		}
		return Ar;
	}
};

/**
 * FStaticMeshSourceData - Source triangles and render data, editor-only.
 */
class FStaticMeshSourceData
{
public:
	FStaticMeshSourceData();
	~FStaticMeshSourceData();

	/** Initialize from static mesh render data. */
	void Init( FStaticMeshRenderData& InRenderData );

	/** Free source data. */
	void Clear();

	/** Clear any material references held by the source data. */
	void ClearMaterialReferences();

	/** Returns TRUE if the source data has been initialized. */
	UBOOL IsInitialized() const { return RenderData != NULL; }

	/** Retrieve render data. */
	FORCEINLINE FStaticMeshRenderData* GetRenderData() { return RenderData; }

	/** Serialization. */
	friend FArchive& operator<<( FArchive& Ar, FStaticMeshSourceData& SourceData );

private:
	FStaticMeshRenderData* RenderData;
};

/**
 * FStaticMeshOptimizationSettings - The settings used to optimize a static mesh LOD.
 */
struct FStaticMeshOptimizationSettings
{
	enum ENormalMode
	{
		NM_PreserveSmoothingGroups,
		NM_RecalculateNormals,
		NM_RecalculateNormalsSmooth,
		NM_RecalculateNormalsHard,
		NM_Max
	};

	enum EImportanceLevel
	{
		IL_Off,
		IL_Lowest,
		IL_Low,
		IL_Normal,
		IL_High,
		IL_Highest,
		IL_Max
	};

	/** Enum specifying the reduction type to use when simplifying static meshes. */
	enum EOptimizationType
	{
		OT_NumOfTriangles,
		OT_MaxDeviation,
		OT_MAX,
	};

	/** The method to use when optimizing the skeletal mesh LOD */
	BYTE ReductionMethod;
	/** If ReductionMethod equals SMOT_NumOfTriangles this value is the ratio of triangles [0-1] to remove from the mesh */
	FLOAT NumOfTrianglesPercentage;
	/**If ReductionMethod equals SMOT_MaxDeviation this value is the maximum deviation from the base mesh as a percentage of the bounding sphere. */
	FLOAT MaxDeviationPercentage;
	/** The welding threshold distance. Vertices under this distance will be welded. */
	FLOAT WeldingThreshold; 
	/** Whether Normal smoothing groups should be preserved. If false then NormalsThreshold is used **/
	UBOOL bRecalcNormals;
	/** If the angle between two triangles are above this value, the normals will not be
	smooth over the edge between those two triangles. Set in degrees. This is only used when PreserveNormals is set to false*/
	FLOAT NormalsThreshold;
	/** How important the shape of the geometry is (EImportanceLevel). */
	BYTE SilhouetteImportance;
	/** How important texture density is (EImportanceLevel). */
	BYTE TextureImportance;
	/** How important shading quality is. */
	BYTE ShadingImportance;

	FStaticMeshOptimizationSettings()
		: ReductionMethod( OT_MaxDeviation )
		, MaxDeviationPercentage( 0.0f )
		, NumOfTrianglesPercentage( 1.0f )
		, WeldingThreshold( 0.1f )
		, bRecalcNormals( TRUE )
		, NormalsThreshold( 60.0f )
		, SilhouetteImportance( IL_Normal )
		, TextureImportance( IL_Normal )
		, ShadingImportance( IL_Normal )
	{
	}
};

/** Serialization for FStaticMeshOptimizationSettings. */
inline FArchive& operator<<( FArchive& Ar, FStaticMeshOptimizationSettings& Settings )
{
	if(Ar.Ver() < VER_ADDED_EXTRA_MESH_OPTIMIZATION_SETTINGS)
	{
		Ar << Settings.MaxDeviationPercentage;

		//Remap Importance Settings
		Ar << Settings.SilhouetteImportance;
		Ar << Settings.TextureImportance;

		//IL_Normal was previously the first enum value. We add the new index of IL_Normal to correctly offset the old values. 
		Settings.SilhouetteImportance += FStaticMeshOptimizationSettings::IL_Normal;
		Settings.TextureImportance += FStaticMeshOptimizationSettings::IL_Normal;

		//Carry over old welding threshold values.
		Settings.WeldingThreshold = THRESH_POINTS_ARE_SAME * 4.0f;

		//Remap NormalMode enum value to new threshold variable.
		BYTE NormalMode;
		Ar << NormalMode;

		const FLOAT NormalThresholdTable[] =
		{
			60.0f, // Recompute
			80.0f, // Recompute (Smooth)
			45.0f  // Recompute (Hard)
		};

		if( NormalMode == FStaticMeshOptimizationSettings::NM_PreserveSmoothingGroups)
		{
			Settings.bRecalcNormals = FALSE;
		} 
		else
		{
			Settings.bRecalcNormals = TRUE;
			Settings.NormalsThreshold = NormalThresholdTable[NormalMode];
		}
	} 
	else
	{
		Ar << Settings.ReductionMethod;
		Ar << Settings.MaxDeviationPercentage;
		Ar << Settings.NumOfTrianglesPercentage;
		Ar << Settings.SilhouetteImportance;
		Ar << Settings.TextureImportance;
		Ar << Settings.ShadingImportance;
		Ar << Settings.bRecalcNormals;
		Ar << Settings.NormalsThreshold;
		Ar << Settings.WeldingThreshold;
	}

	return Ar;
}

//
//	UStaticMesh
//

class UStaticMesh : public UObject
{
	DECLARE_CLASS_INTRINSIC(UStaticMesh,UObject,CLASS_SafeReplace|CLASS_CollapseCategories|0,Engine);
public:
	/** Array of LODs, holding their associated rendering and collision data */
	TIndirectArray<FStaticMeshRenderData>	LODModels;
	/** Per-LOD information exposed to the editor */
	TArray<FStaticMeshLODInfo>				LODInfo;
	/** LOD distance ratio for this mesh */
	FLOAT									LODDistanceRatio;
	/** Range at which only the lowest detail LOD can be displayed */
	FLOAT									LODMaxRange;
	FRotator								ThumbnailAngle;
	FLOAT									ThumbnailDistance;

	INT										LightMapResolution;
	INT										LightMapCoordinateIndex;

	/** TRUE if this mesh been simplified. */
	UBOOL									bHasBeenSimplified;
	/** TRUE if the mesh is a proxy. */
	UBOOL									bIsMeshProxy;

	/** Incremented any time a change in the static mesh causes vertices to change position, such as a reimport */
	INT										VertexPositionVersionNumber;

	// Collision data.

//	typedef TkDOPTree<class FStaticMeshCollisionDataProvider,WORD>	kDOPTreeType;
	typedef TkDOPTreeCompact<class FStaticMeshCollisionDataProvider,WORD>	kDOPTreeType;
	typedef TkDOPTree<class FStaticMeshCollisionDataProvider,WORD>			LegacykDOPTreeType;
	kDOPTreeType							kDOPTree;
	LegacykDOPTreeType*						LegacykDOPTree;  // this is only used temporarily during the loading process, usually NULL

	URB_BodySetup*							BodySetup;
	FBoxSphereBounds						Bounds;

	/** Array of physics-engine shapes that can be used by multiple StaticMeshComponents. */
	TArray<void*>							PhysMesh;

	/** Scale of each PhysMesh entry. Arrays should be same size. */
	TArray<FVector>							PhysMeshScale3D;

	// Artist-accessible options.

	UBOOL									UseSimpleLineCollision,
											UseSimpleBoxCollision,
											UseSimpleRigidBodyCollision,
											UseFullPrecisionUVs;
	UBOOL									bUsedForInstancing;
	
	/** Hint of the expected instance count for consoles to preallocate the duplicated index buffer */
	INT										ConsolePreallocateInstanceCount;

	/** True if mesh should use a less-conservative method of mip LOD texture factor computation.
	    requires mesh to be resaved to take effect as algorithm is applied on save. */
	UBOOL									bUseMaximumStreamingTexelRatio;

	/** If true, the cooker will partition this mesh for use with Edge Geometry on the Playstation 3.
	 *  This will increase the size of the resulting mesh data by a fraction of a percent,
	 *  so don't enable it unless you've enabled Edge in the runtime code.
	 */
	UBOOL									bPartitionForEdgeGeometry;

	/** 
	 * Whether this mesh should be able to become dynamic when placed as a static mesh 
	 * Meshes with bCanBecomeDynamic=true will temporarily turn into KActors and react using PhysX physics when shot or pushed, giving the environment a more interactive feel.
     *
	 * The advantages of this implementation are:
	 * -	Consistent behavior (all objects using that mesh will be interactive, instead of it depending on level designer set up).
	 * -	Transparent to meshing (L.D.s don't have to do anything special, but they can prevent specific objects from being able to become dynamic by setting the bNeverBecomeDynamic property in the StaticMeshComponent's collision properties).
	 * -	These meshes are lit like any other static meshes until they move, so there is no visual impact to using this system.
	 * -	Except during the short period when a mesh is being physically simulated, there's no additional performance or memory cost to these meshes.
     *
	 * This system is intended for use with small decorative meshes that don't have any gameplay implications.  The interactivity is client-side and is not replicated.
	 **/
	UBOOL									bCanBecomeDynamic;

	/** If true during a rebuild, we will remove degenerate triangles.  Otherwise they will be kept */
	UBOOL									bRemoveDegenerates;

	/** If true, strips unwanted complex collision data aka kDOP tree when cooking for consoles. 
		On the Playstation 3 data of this mesh will be stored in video memory. */
	UBOOL									bStripkDOPForConsole;
	
	/** If true, InstancedStaticMeshComponents will build static lighting for each LOD rather than all LODs sharing the top level LOD's lightmaps */
	UBOOL									bPerLODStaticLightingForInstancing;


	/**
	 * Allows artists to adjust the distance where textures using UV 0 are streamed in/out.
	 * 1.0 is the default, whereas a higher value increases the streamed-in resolution.
	 */
	FLOAT									StreamingDistanceMultiplier;

	INT										InternalVersion;

	/** The cached streaming texture factors.  If the array doesn't have MAX_TEXCOORDS entries in it, the cache is outdated. */
	TArray<FLOAT> CachedStreamingTextureFactors;

	/** A fence which is used to keep track of the rendering thread releasing the static mesh resources. */
	FRenderCommandFence ReleaseResourcesFence;


	/**
	 * For simplified meshes, this is the fully qualified path and name of the static mesh object we were
	 * originally duplicated from.  This is serialized to disk, but is discarded when cooking for consoles.
	 */
	FString HighResSourceMeshName;

#if WITH_EDITORONLY_DATA
	/** Default settings when using this mesh for instanced foliage */
	class UInstancedFoliageSettings* FoliageDefaultSettings;

	/** Path to the resource used to construct this static mesh */
	FString SourceFilePath;

	/** Date/Time-stamp of the file from the last import */
	FString SourceFileTimestamp;
#endif // WITH_EDITORONLY_DATA

	/** For simplified meshes, this is the CRC of the high res mesh we were originally duplicated from. */
	DWORD HighResSourceMeshCRC;

	/** Unique ID for tracking/caching this mesh during distributed lighting */
	FGuid LightingGuid;

	// UObject interface.

	void StaticConstructor();
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	virtual void PreEditChange(UProperty* PropertyAboutToChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/**
	 * Called by the editor to query whether a property of this object is allowed to be modified.
	 * The property editor uses this to disable controls for properties that should not be changed.
	 * When overriding this function you should always call the parent implementation first.
	 *
	 * @param	InProperty	The property to query
	 *
	 * @return	TRUE if the property can be modified in the editor, otherwise FALSE
	 */
	virtual UBOOL CanEditChange( const UProperty* InProperty ) const;

	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();
	virtual void BeginDestroy();
	virtual UBOOL IsReadyForFinishDestroy();
	virtual UBOOL Rename( const TCHAR* NewName=NULL, UObject* NewOuter=NULL, ERenameFlags Flags=REN_None );

	/**
	* Used by the cooker to pre-cache the convex data for a given static mesh.  
	* This data is stored with the level.
	* @param Level - the level the data is cooked for
	* @param TotalScale3D - the scale to cook the data at
	* @param Owner - owner of this mesh for debug purposes (can be NULL)
	* @param HullByteCount - running total of memory usage for hull cache
	* @param HullCount - running count of hull cache
	*/
	void CookPhysConvexDataForScale(ULevel* Level, const FVector& TotalScale3D, const AActor* Owner, INT& TriByteCount, INT& TriMeshCount, INT& HullByteCount, INT& HullCount);

	/** Set the physics triangle mesh representations owned by this StaticMesh to be destroyed. */
	void ClearPhysMeshCache();

	/**
	 * Used by various commandlets to purge editor only and platform-specific data from various objects
	 * 
	 * @param PlatformsToKeep Platforms for which to keep platform-specific data
	 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
	 */
	virtual void StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData);

	/**
	 * Called after duplication & serialization and before PostLoad. Used to e.g. make sure UStaticMesh's UModel
	 * gets copied as well.
	 */
	virtual void PostDuplicate();

	/**
	 * Callback used to allow object register its direct object references that are not already covered by
	 * the token stream.
	 *
	 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
	 */
	virtual void AddReferencedObjects( TArray<UObject*>& ObjectArray );

	// UStaticMesh interface.

	void Build(UBOOL bSingleTangentSetOverride = FALSE, UBOOL bSilent = FALSE);

	/**
	 * Initialize the static mesh's render resources.
	 */
	virtual void InitResources();

	/**
	 * Releases the static mesh's render resources.
	 */
	virtual void ReleaseResources();

	/**
	 * Returns the scale dependent texture factor used by the texture streaming code.	
	 *
	 * @param RequestedUVIndex UVIndex to look at
	 * @return scale dependent texture factor
	 */
	FLOAT GetStreamingTextureFactor( INT RequestedUVIndex );

	/** 
	 * Returns a one line description of an object for viewing in the generic browser
	 */
	virtual FString GetDesc();

	/** 
	 * Returns detailed info to populate listview columns
	 */
	virtual FString GetDetailedDescription( INT InIndex );

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize();

	/** Returns the size of renderer resource ie. vertex and index buffers. */
	INT GetRendererResourceSize() const;

	/** Returns the size of kDOP tree. */
	INT GetkDOPTreeSize() const;

	/**
	 * Attempts to load this mesh's high res source static mesh
	 *
	 * @return The high res source mesh, or NULL if it failed to load
	 */
	UStaticMesh* LoadHighResSourceMesh() const;

	/**
	 * Computes a CRC for this mesh to be used with mesh simplification tests.
	 *
	 * @return Simplified CRC for this mesh
	 */
	DWORD ComputeSimplifiedCRCForMesh() const;

	/**
	 * Ensures the original mesh data is saved and returns a pointer to it. This method should
	 * always be called before making changes to the mesh's triangle data. Note that we can't
	 * return a const reference to the data else the calling code can't lock it for reading.
	 *
	 * @return The source mesh data.
	 */
	FStaticMeshRenderData& PreModifyMesh();

	/**
	 * Returns the source data imported for LOD0 of this mesh.
	 */
	FStaticMeshRenderData& GetSourceData();

	/**
	 * Returns the render data to use for exporting the specified LOD. This method should always
	 * be called when exporting a static mesh.
	 */
	FStaticMeshRenderData* GetLODForExport( INT LODIndex );

	/**
	 * Returns TRUE if the mesh has optimizations stored for the specified LOD.
	 * @param LODIndex - LOD index for which to look for optimization settings.
	 * @returns TRUE if the mesh has optimizations stored for the specified LOD.
	 */
	UBOOL HasOptimizationSettings( INT LODIndex ) const
	{
		return LODIndex >= 0 && LODIndex < OptimizationSettings.Num();
	}

	/**
	 * Retrieves the settings with which the LOD was optimized.
	 * @param LODIndex - LOD index for which to look up the optimization settings.
	 * @returns the optimization settings for the specified LOD.
	 */
	const FStaticMeshOptimizationSettings& GetOptimizationSettings( INT LODIndex ) const
	{
		check( LODIndex >= 0 && LODIndex < OptimizationSettings.Num() );
		return OptimizationSettings( LODIndex );
	}

	/**
	 * Stores the settings with which the LOD was optimized.
	 * @param LODIndex - LOD index for which to store optimization settings.
	 * @param Settings - Optimization settings for the specified LOD index.
	 */
	void SetOptimizationSettings( INT LODIndex, const FStaticMeshOptimizationSettings& Settings )
	{
		if ( LODIndex >= OptimizationSettings.Num() )
		{
			FStaticMeshOptimizationSettings DefaultSettings;
			const FStaticMeshOptimizationSettings& SettingsToCopy = OptimizationSettings.Num() ? OptimizationSettings.Last() : DefaultSettings;
			while ( LODIndex >= OptimizationSettings.Num() )
			{
				OptimizationSettings.AddItem( SettingsToCopy );
			}
		}
		check( LODIndex < OptimizationSettings.Num() );
		OptimizationSettings( LODIndex ) = Settings;
	}

	/**
	 * Removes an entry from the list of optimization settings.
	 * @param LODIndex - LOD index for which to remove optimization settings.
	 */
	void RemoveOptimizationSettings( INT LODIndex )
	{
		if ( LODIndex < OptimizationSettings.Num() )
		{
			OptimizationSettings.Remove( LODIndex );
		}
	}

	/**
	 * Clears max deviations.
	 */
	void ClearOptimizationSettings()
	{
		OptimizationSettings.Empty();
	}

	/**
	 * Static: Processes the specified static mesh for light map UV problems
	 *
	 * @param	InStaticMesh					Static mesh to process
	 * @param	InOutAssetsWithMissingUVSets	Array of assets that we found with missing UV sets
	 * @param	InOutAssetsWithBadUVSets		Array of assets that we found with bad UV sets
	 * @param	InOutAssetsWithValidUVSets		Array of assets that we found with valid UV sets
	 * @param	bInVerbose						If TRUE, log the items as they are found
	 */
	static void CheckLightMapUVs( UStaticMesh* InStaticMesh, TArray< FString >& InOutAssetsWithMissingUVSets, TArray< FString >& InOutAssetsWithBadUVSets, TArray< FString >& InOutAssetsWithValidUVSets, UBOOL bInVerbose = TRUE );

	/**
	 * Checks if the specified static mesh has any elements with no triangles and warns the user if that is the case
	 *
	 *	@param	InStaticMesh	The mesh to check
	 *	@param	bUserPrompt		If TRUE, prompt the user for verification
	 *
	 *	@return	UBOOL			TRUE if the mesh was modified, FALSE if not
	 */
	static UBOOL RemoveZeroTriangleElements(UStaticMesh* InStaticMesh, UBOOL bUserPrompt);

	virtual const FGuid& GetLightingGuid() const
	{
#if WITH_EDITORONLY_DATA
		return LightingGuid;
#else
		static const FGuid NullGuid( 0, 0, 0, 0 );
		return NullGuid; 
#endif // WITH_EDITORONLY_DATA
	}

	virtual void SetLightingGuid()
	{
#if WITH_EDITORONLY_DATA
		LightingGuid = appCreateGuid();
#endif // WITH_EDITORONLY_DATA
	}

	/**
	 * Returns vertex color data by position.
	 * For matching to reimported meshes that may have changed or copying vertex paint data from mesh to mesh.
	 *
	 *	@param	VertexColorData		(out)A map of vertex position data and its color. The method fills this map.
	 */
	void GetVertexColorData(TMap<FVector, FColor>& VertexColorData);

	/**
	 * Sets vertex color data by position.
	 * Map of vertex color data by position is matched to the vertex position in the mesh
	 * and nearest matching vertex color is used.
	 *
	 *	@param	VertexColorData		A map of vertex position data and color.
	 */
	void SetVertexColorData(const TMap<FVector, FColor>& VertexColorData);

protected:

	/** 
	 * Index of an element to ignore while gathering streaming texture factors.
	 * This is useful to disregard automatically generated vertex data which breaks texture factor heuristics.
	 */
	INT	ElementToIgnoreForTexFactor;

	/**
	 * Mapping of properties' names to their tooltips
	 */
	static TMap<FString, FString> PropertyToolTipMap;

private:

	/** The original raw triangles and generated render data. */
	FStaticMeshSourceData SourceData;

	/** Optimization settings used to simplify mesh LODs. */
	TArray<FStaticMeshOptimizationSettings> OptimizationSettings;
};

/** Cached vertex information at the time the mesh was painted. */
struct FPaintedVertex
{
	FVector			Position;
	FPackedNormal	Normal;
	FColor			Color;
};

FORCEINLINE FArchive& operator<<( FArchive& Ar, FPaintedVertex& PaintedVertex )
{
	Ar << PaintedVertex.Position;
	Ar << PaintedVertex.Normal;
	Ar << PaintedVertex.Color;
	return Ar;
}

struct FStaticMeshComponentLODInfo
{
	TArray<UShadowMap2D*> ShadowMaps;
	TArray<UShadowMap1D*> ShadowVertexBuffers;
	FLightMapRef LightMap;
	/**
	 * 0 if not used, here we take care of the object creation and destruction, OverrideColorVertexBuffer might point to the same FColorVertexBuffer object. 
	 * The data the pointer points to is accessed by the rendering thread but the RT is not accessing the pointer stored here.
	 */
	FColorVertexBuffer* OverrideVertexColors;
	/** Vertex data cached at the time this LOD was painted, if any */
	TArray<FPaintedVertex> PaintedVertices;

	/** Default constructor */
	FStaticMeshComponentLODInfo();
	/** Copy constructor */
	FStaticMeshComponentLODInfo( const FStaticMeshComponentLODInfo &rhs );
	/** Destructor */
	~FStaticMeshComponentLODInfo();

	/** Delete existing resources */
	void CleanUp();

	/** 
	 * Enqueues a rendering command to release the vertex colors.  
	 * The game thread must block until the rendering thread has processed the command before deleting OverrideVertexColors. 
	 */
	void BeginReleaseOverrideVertexColors();

	void ReleaseOverrideVertexColorsAndBlock();

	void ReleaseResources();

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FStaticMeshComponentLODInfo& I)
	{
		Ar << I.ShadowMaps;
		Ar << I.ShadowVertexBuffers;
		Ar << I.LightMap;
		if( Ar.Ver() >= VER_MESH_PAINT_SYSTEM_ENUM )
		{
			if( Ar.Ver() < VER_OVERWRITE_VERTEX_COLORS_MEM_OPTIMIZED )
			{
				// TArray serialization (old method)
				check(Ar.IsLoading());
				check(!I.OverrideVertexColors);

				TArray<FColor> Temp;

				Ar << Temp;

				if(Temp.Num())
				{
					I.OverrideVertexColors = new FColorVertexBuffer;
					I.OverrideVertexColors->InitFromColorArray(Temp);
				}
			}
			else
			{
				// Bulk serialization (new method)
				BYTE bLoadVertexColorData = (I.OverrideVertexColors != NULL);
				Ar << bLoadVertexColorData;

				if(bLoadVertexColorData)
				{
					if(Ar.IsLoading())
					{
						check(!I.OverrideVertexColors);
						I.OverrideVertexColors = new FColorVertexBuffer;
					}

					I.OverrideVertexColors->Serialize( Ar, TRUE );
				}
			}

		}

		// Legacy serialization for vertex color positions.
		if ( Ar.Ver() >= VER_PRESERVE_SMC_VERT_COLORS && Ar.Ver() < VER_STATIC_MESH_SOURCE_DATA_COPY )
		{
			TArray<FVector> VertexColorPositions;
			Ar << VertexColorPositions;
		}

		// Serialize out cached vertex information if necessary.
		if ( Ar.Ver() >= VER_STATIC_MESH_SOURCE_DATA_COPY )
		{
			Ar << I.PaintedVertices;
		}

		// Fix components affected by the copy + paste bug.
		if ( Ar.Ver() < VER_FIX_OVERRIDEVERTEXCOLORS_COPYPASTE )
		{
			// Components affected by this bug have a single painted vertex.
			// Clear it and it will be rebuilt in PostLoad.
			if ( I.PaintedVertices.Num() == 1 )
			{
				I.PaintedVertices.Empty();
			}
		}

		// Empty when loading and we don't care about saving it again, like e.g. a client.
		if( Ar.IsLoading() && ( !GIsEditor && !GIsUCC ) )
		{
			I.PaintedVertices.Empty();
		}

		return Ar;
	}

private:

	/** Purposely hidden */
	FStaticMeshComponentLODInfo &operator=( const FStaticMeshComponentLODInfo &rhs ) { check(0); return *this; }
};

//
//	UStaticMeshComponent
//

class UStaticMeshComponent : public UMeshComponent
{
	DECLARE_CLASS_NOEXPORT(UStaticMeshComponent,UMeshComponent,0,Engine);
public:
	UStaticMeshComponent();

	/** Force drawing of a specific lodmodel-1. 0 is automatic selection */
	INT									ForcedLodModel;
	/** LOD that was desired for rendering this StaticMeshComponent last frame. */
	INT									PreviousLODLevel;

	UStaticMesh* StaticMesh;
	FColor WireframeColor;

	/** 
	 *	Ignore this instance of this static mesh when calculating streaming information. 
	 *	This can be useful when doing things like applying character textures to static geometry, 
	 *	to avoid them using distance-based streaming.
	 */
	BITFIELD bIgnoreInstanceForTextureStreaming:1;

	/** Deprecated. Replaced by 'bOverrideLightMapRes'. */
	BITFIELD bOverrideLightMapResolution_DEPRECATED:1;

	/** Whether to override the lightmap resolution defined in the static mesh. */
	BITFIELD bOverrideLightMapRes:1;
	/** Deprecated. Replaced by 'OverriddenLightMapRes'. */
	INT OverriddenLightMapResolution_DEPRECATED;

	/** Light map resolution used if bOverrideLightMapRes is TRUE */
	INT OverriddenLightMapRes;

	FLOAT OverriddenLODMaxRange;

	/**
	 * Allows adjusting the desired streaming distance of streaming textures that uses UV 0.
	 * 1.0 is the default, whereas a higher value makes the textures stream in sooner from far away.
	 * A lower value (0.0-1.0) makes the textures stream in later (you have to be closer).
	 */
	FLOAT StreamingDistanceMultiplier;

	/** Subdivision step size for static vertex lighting.				*/
	INT	SubDivisionStepSize;
	/** Whether to use subdivisions or just the triangle's vertices.	*/
	BITFIELD bUseSubDivisions:1;
	/** if True then decals will always use the fast path and will be treated as static wrt this mesh */
	BITFIELD bForceStaticDecals:1;
	/** Whether or not we can highlight selected sections - this should really only be done in the editor */
	BITFIELD bCanHighlightSelectedSections:1;

	/** Whether or not to use the optional simple lightmap modification texture */
	BITFIELD bUseSimpleLightmapModifications:1;

#if WITH_EDITORONLY_DATA
	/** The texture to use when modifying the simple lightmap texture */
	UTexture* SimpleLightmapModificationTexture;
#endif // WITH_EDITORONLY_DATA

	enum ELightmapModificationFunction
	{
		/** Lightmap.RGB * Modification.RGB */
		MLMF_Modulate,
		/** Lightmap.RGB * (Modification.RGB * Modification.A) */
		MLMF_ModulateAlpha,
	};
	/** The function to use when modifying the simple lightmap texture */
	ELightmapModificationFunction SimpleLightmapModificationFunction;

	/** Whether this static mesh component can become dynamic (and be affected by physics) */
	BITFIELD bNeverBecomeDynamic:1;

	TArray<FGuid> IrrelevantLights;	// Statically irrelevant lights.

	/** Per-LOD instance information */
	TArray<FStaticMeshComponentLODInfo> LODData;

	/** Incremented any time the position of vertices from the source mesh change, used to determine if an update from the source static mesh is required */
	INT VertexPositionVersionNumber;

	/** The Lightmass settings for this object. */
	FLightmassPrimitiveSettings	LightmassSettings;

	// UStaticMeshComponent interface

	virtual UBOOL SetStaticMesh(UStaticMesh* NewMesh, UBOOL bForce=FALSE);

	/**
	 * Changes the value of bForceStaticDecals.
	 * @param bInForceStaticDecals - The value to assign to bForceStaticDecals.
	 */
	virtual void SetForceStaticDecals(UBOOL bInForceStaticDecals);

	/**
	 * @RETURNS true if this mesh can become dynamic
	 */
	virtual UBOOL CanBecomeDynamic();

	/**
	 * Returns whether this primitive only uses unlit materials.
	 *
	 * @return TRUE if only unlit materials are used for rendering, false otherwise.
	 */
	virtual UBOOL UsesOnlyUnlitMaterials() const;

	/**
	 *	Returns TRUE if the component uses texture lightmaps
	 *	
	 *	@param	InWidth		[in]	The width of the light/shadow map
	 *	@param	InHeight	[in]	The width of the light/shadow map
	 *
	 *	@return	UBOOL				TRUE if texture lightmaps are used, FALSE if not
	 */
	virtual UBOOL UsesTextureLightmaps(INT InWidth, INT InHeight) const;

	/**
	 *	Returns TRUE if the static mesh the component uses has valid lightmap texture coordinates
	 */
	virtual UBOOL HasLightmapTextureCoordinates() const;

	/**
	 *	Get the memory used for texture-based light and shadow maps of the given width and height
	 *
	 *	@param	InWidth						The desired width of the light/shadow map
	 *	@param	InHeight					The desired height of the light/shadow map
	 *	@param	OutLightMapMemoryUsage		The resulting lightmap memory used
	 *	@param	OutShadowMapMemoryUsage		The resulting shadowmap memory used
	 */
	virtual void GetTextureLightAndShadowMapMemoryUsage(INT InWidth, INT InHeight, INT& OutLightMapMemoryUsage, INT& OutShadowMapMemoryUsage) const;

	/**
	 *	Get the memory used for vertex-based light and shadow maps
	 *
	 *	@param	OutLightMapMemoryUsage		The resulting lightmap memory used
	 *	@param	OutShadowMapMemoryUsage		The resulting shadowmap memory used
	 */
	virtual void GetVertexLightAndShadowMapMemoryUsage(INT& OutLightMapMemoryUsage, INT& OutShadowMapMemoryUsage) const;

	/**
	 * Returns the lightmap resolution used for this primitive instance in the case of it supporting texture light/ shadow maps.
	 * 0 if not supported or no static shadowing.
	 *
	 * @param	Width	[out]	Width of light/shadow map
	 * @param	Height	[out]	Height of light/shadow map
	 *
	 * @return	UBOOL			TRUE if LightMap values are padded, FALSE if not
	 */
	virtual UBOOL GetLightMapResolution( INT& Width, INT& Height ) const;

	/**
	 *	Returns the lightmap resolution used for this primitive instance in the case of it supporting texture light/ shadow maps.
	 *	This will return the value assuming the primitive will be automatically switched to use texture mapping.
	 *
	 *	@param Width	[out]	Width of light/shadow map
	 *	@param Height	[out]	Height of light/shadow map
	 */
	virtual void GetEstimatedLightMapResolution(INT& Width, INT& Height) const;

	/**
	 *	Returns the static lightmap resolution used for this primitive.
	 *	0 if not supported or no static shadowing.
	 *
	 * @return	INT		The StaticLightmapResolution for the component
	 */
	virtual INT GetStaticLightMapResolution() const;

	/**
	 * Returns the light and shadow map memory for this primite in its out variables.
	 *
	 * Shadow map memory usage is per light whereof lightmap data is independent of number of lights, assuming at least one.
	 *
	 * @param [out] LightMapMemoryUsage		Memory usage in bytes for light map (either texel or vertex) data
	 * @param [out]	ShadowMapMemoryUsage	Memory usage in bytes for shadow map (either texel or vertex) data
	 */
	virtual void GetLightAndShadowMapMemoryUsage( INT& LightMapMemoryUsage, INT& ShadowMapMemoryUsage ) const;

	/**
	 * Returns the light and shadow map memory for this primite in its out variables.
	 *
	 * Shadow map memory usage is per light whereof lightmap data is independent of number of lights, assuming at least one.
	 *
	 *	@param [out]	TextureLightMapMemoryUsage		Estimated memory usage in bytes for light map texel data
	 *	@param [out]	TextureShadowMapMemoryUsage		Estimated memory usage in bytes for shadow map texel data
	 *	@param [out]	VertexLightMapMemoryUsage		Estimated memory usage in bytes for light map vertex data
	 *	@param [out]	VertexShadowMapMemoryUsage		Estimated memory usage in bytes for shadow map vertex data
	 *	@param [out]	StaticLightingResolution		The StaticLightingResolution used for Texture estimates
	 *	@param [out]	bIsUsingTextureMapping			Set to TRUE if the mesh is using texture mapping currently; FALSE if vertex
	 *	@param [out]	bHasLightmapTexCoords			Set to TRUE if the mesh has the proper UV channels
	 *
	 *	@return			UBOOL							TRUE if the mesh has static lighting; FALSE if not
	 */
	virtual UBOOL GetEstimatedLightAndShadowMapMemoryUsage( 
		INT& TextureLightMapMemoryUsage, INT& TextureShadowMapMemoryUsage,
		INT& VertexLightMapMemoryUsage, INT& VertexShadowMapMemoryUsage,
		INT& StaticLightingResolution, UBOOL& bIsUsingTextureMapping, UBOOL& bHasLightmapTexCoords) const;

	/**
	 *	UStaticMeshComponent::GetMaterial
	 * @param MaterialIndex Index of material
	 * @param LOD Lod level to query from
	 * @return Material instance for this component at index
	 */
	virtual UMaterialInterface* GetMaterial(INT MaterialIndex, INT LOD) const;

	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const;

	/**
	 * Intersects a line with this component
	 *
	 * @param	LODIndex				LOD to check against
	 */
	virtual UBOOL LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags, UINT LODIndex);

	/**
	 * Determines whether any of the component's LODs require override vertex color fixups
	 *
	 * @param	OutLODIndices	Indices of the LODs requiring fixup, if any
	 *
	 * @return	TRUE if any LODs require override vertex color fixups
	 */
	UBOOL RequiresOverrideVertexColorsFixup( TArray<INT>& OutLODIndices );

	/** 
	 * Update the vertex override colors if necessary (i.e. vertices from source mesh have changed from override colors)
	 * @returns TRUE if any fixup was performed.
	 */
	UBOOL FixupOverrideColorsIfNecessary();

	/** Save off the data painted on to this mesh per LOD if necessary */
	void CachePaintedDataIfNecessary();

private: 
	/** Initializes the resources used by the static mesh component. */
	void InitResources(); 

public:

	void ReleaseResources();

	// UObject interface.
	virtual void BeginDestroy();
	virtual void ExportCustomProperties(FOutputDevice& Out, UINT Indent);
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn);

	// UMeshComponent interface.
	virtual INT GetNumElements() const;
	virtual UMaterialInterface* GetMaterial(INT MaterialIndex) const;

	// UPrimitiveComponent interface.
	virtual void GenerateDecalRenderData(class FDecalState* Decal, TArray< FDecalRenderData* >& OutDecalRenderDatas) const;
	virtual void GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options);
	/**
	 *	Requests whether the component will use texture, vertex or no lightmaps.
	 *
	 *	@return	ELightMapInteractionType		The type of lightmap interaction the component will use.
	 */
	virtual ELightMapInteractionType GetStaticLightingType() const;
	/** Gets the emissive boost for the primitive component. */
	virtual FLOAT GetEmissiveBoost(INT ElementIndex) const;
	/** Gets the diffuse boost for the primitive component. */
	virtual FLOAT GetDiffuseBoost(INT ElementIndex) const;
	/** Gets the specular boost for the primitive component. */
	virtual FLOAT GetSpecularBoost(INT ElementIndex) const;
	virtual UBOOL GetShadowIndirectOnly() const 
	{ 
		return LightmassSettings.bShadowIndirectOnly;
	}

	/** Allocates an implementation of FStaticLightingMesh that will handle static lighting for this component */
	virtual class FStaticMeshStaticLightingMesh* AllocateStaticLightingMesh(INT LODIndex, const TArray<ULightComponent*>& InRelevantLights);
	virtual void GetStaticTriangles(FPrimitiveTriangleDefinitionInterface* PTDI) const;
	virtual void GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const;

	virtual UBOOL PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags);
	virtual UBOOL LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags);

	/**
	 * Calculates the closest point this component to another component
	 * @param PrimitiveComponent - Another Primitive Component
	 * @param PointOnComponentA - Point on this primitive closest to other primitive
	 * @param PointOnComponentB - Point on other primitive closest to this primitive
	 * 
	 * @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
	 */
	/*GJKResult*/ BYTE ClosestPointOnComponentToComponent(class UPrimitiveComponent*& OtherComponent,FVector& PointOnComponentA,FVector& PointOnComponentB);

	/**		**INTERNAL USE ONLY**
	 * Implementation required by a primitive component in order to properly work with the closest points algorithms below
	 * Given an interface to some other primitive, return the points on each object closest to each other
	 * @param ExtentHelper - Interface class returning the supporting points on some other primitive type
	 * @param OutPointA - The point closest on the 'other' primitive
	 * @param OutPointB - The point closest on this primitive
	 * 
	 * @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
	 */
	virtual /*GJKResult*/ BYTE ClosestPointOnComponentInternal(IGJKHelper* ExtentHelper, FVector& OutPointA, FVector& OutPointB);

	virtual void UpdateBounds();

	/** Returns true if this component is always static*/
	virtual UBOOL IsAlwaysStatic();

	virtual void InitComponentRBPhys(UBOOL bFixed);
	virtual void TermComponentRBPhys(FRBPhysScene* InScene);
	virtual class URB_BodySetup* GetRBBodySetup();
	virtual void CookPhysConvexDataForScale(ULevel* Level, const FVector& TotalScale3D, INT& TriByteCount, INT& TriMeshCount, INT& HullByteCount, INT& HullCount);
	virtual FKCachedConvexData* GetCachedPhysConvexData(const FVector& InScale3D);

	/** Disables physics collision between a specific pair of meshes. */
	void DisableRBCollisionWithSMC( UPrimitiveComponent* OtherSMC, UBOOL bDisabled );

	/** Implement support for making meshes that can become dynamic interactive when an impulse or force is applied */
	virtual void AddImpulse(FVector Impulse, FVector Position = FVector(0,0,0), FName BoneName = NAME_None, UBOOL bVelChange=false);
	virtual void AddRadialImpulse(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff, UBOOL bVelChange=false);
	virtual void AddRadialForce(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff);

	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual UBOOL ShouldRecreateProxyOnUpdateTransform() const;

	// UActorComponent interface.
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

	protected: virtual void Attach(); public:
	virtual UBOOL IsValidComponent() const;

	virtual void InvalidateLightingCache();

	// UObject interface.
	virtual void AddReferencedObjects( TArray<UObject*>& ObjectArray );
	virtual void Serialize(FArchive& Ar);
	virtual void PostEditUndo();
	virtual void PreEditUndo();
	virtual void PreSave();

	/** Add or remove elements to have the size in the specified range. Reconstructs elements if MaxSize<MinSize */
	void SetLODDataCount( const UINT MinSize, const UINT MaxSize );

	/**
	 * Used by various commandlets to purge editor only and platform-specific data from various objects
	 * 
	 * @param PlatformsToKeep Platforms for which to keep platform-specific data
	 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
	 */
	virtual void StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData);

	/**
	 * Called after all objects referenced by this object have been serialized. Order of PostLoad routed to 
	 * multiple objects loaded in one set is not deterministic though ConditionalPostLoad can be forced to
	 * ensure an object has been "PostLoad"ed.
	 */
	virtual void PostLoad();

	/** 
	 * Called when any property in this object is modified in UnrealEd
	 *
	 * @param	PropertyThatChanged		changed property
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/**
	 * Returns whether native properties are identical to the one of the passed in component.
	 *
	 * @param	Other	Other component to compare against
	 *
	 * @return TRUE if native properties are identical, FALSE otherwise
	 */
	virtual UBOOL AreNativePropertiesIdenticalTo( UComponent* Other ) const;

#if USE_GAMEPLAY_PROFILER
    /** 
     * This function actually does the work for the GetProfilerAssetObject and is virtual.  
     * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
     **/
	virtual UObject* GetProfilerAssetObjectInternal() const;
#endif

	/**
	 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
	 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
	 * you have a component of interest but what you really want is some characteristic that you can use to track
	 * down where it came from.  
	 *
	 */
	virtual FString GetDetailedInfoInternal() const;

	/**
	 *	Switches the static mesh component to use either Texture or Vertex static lighting.
	 *
	 *	@param	bTextureMapping		If TRUE, set the component to use texture light mapping.
	 *	@param	ResolutionToUse		If != 0, set the resolution to the given value. 
	 *
	 *	@return	UBOOL				TRUE if successfully set; FALSE if not
	 *								If FALSE, set it to use vertex light mapping.
	 */
	virtual UBOOL SetStaticLightingMapping(UBOOL bTextureMapping, INT ResolutionToUse);

	/**
	 *  Retrieve various actor metrics depending on the provided type.  All of
	 *  these will total the values for this component.
	 *
	 *  @param MetricsType The type of metric to calculate.
	 *
	 *  METRICS_VERTS    - Get the number of vertices.
	 *  METRICS_TRIS     - Get the number of triangles.
	 *  METRICS_SECTIONS - Get the number of sections.
	 *
	 *  @return INT The total of the given type for this component.
	 */
	virtual INT GetActorMetrics(EActorMetricsType MetricsType);

	// Script functions

	DECLARE_FUNCTION(execSetStaticMesh);
	DECLARE_FUNCTION(execSetForceStaticDecals);
	DECLARE_FUNCTION(execCanBecomeDynamic);
	DECLARE_FUNCTION(execDisableRBCollisionWithSMC);
};

/**
 * This struct provides the interface into the static mesh collision data
 */
class FStaticMeshCollisionDataProvider
{
	/**
	 * The component this mesh is attached to
	 */
	const UStaticMeshComponent* Component;
	/**
	 * The mesh that is being collided against
	 */
	UStaticMesh* Mesh;

	/**
	 * The LOD that is being collided against
	 */
	UINT CurrentLOD;
	/**
	 * Pointer to vertex buffer containing position data.
	 */
	FPositionVertexBuffer* PositionVertexBuffer;

	/** The transform to map to world space */
	const FMatrix& LocalToWorld;

	/** The determinant of the local to world */
	FLOAT LocalToWorldDeterminant;

	/** Hide default ctor */
	FStaticMeshCollisionDataProvider(void) : LocalToWorld(FMatrix::Identity)
	{
	}


public:
	/**
	 * Sets the component and mesh members
	 */
	FORCEINLINE FStaticMeshCollisionDataProvider(const UStaticMeshComponent* InComponent, const FMatrix& InLocalToWorld, FLOAT InLocalToWorldDeterminant, UINT InCurrentLOD = 0) :
		Component(InComponent),
		Mesh(InComponent->StaticMesh),
		CurrentLOD(InCurrentLOD),
		PositionVertexBuffer(&InComponent->StaticMesh->LODModels(InCurrentLOD).PositionVertexBuffer),
		LocalToWorld(InLocalToWorld),
		LocalToWorldDeterminant(InLocalToWorldDeterminant)
	{
	}
	/**
	 * Given an index, returns the position of the vertex
	 *
	 * @param Index the index into the vertices array
	 */
	FORCEINLINE const FVector& GetVertex(WORD Index) const
	{
		return PositionVertexBuffer->VertexPosition(Index);
	}

	/**
	 * Given an index and a uv channel, returns the uv at the index in the uv channel
	 *
	 * @param Index		The index into the vertex buffer
	 * @param UVChannel	The uv channel to retrieve uv's from
	 */
	FORCEINLINE const FVector2D GetVertexUV( WORD Index, UINT UVChannel ) const
	{
		return Mesh->LODModels(CurrentLOD).VertexBuffer.GetVertexUV( Index, UVChannel );
	}

	/**
	 * Returns the material for a triangle based upon material index
	 *
	 * @param MaterialIndex the index into the materials array
	 */
	FORCEINLINE UMaterialInterface* GetMaterial(WORD MaterialIndex) const
	{
		return Component->GetMaterial(MaterialIndex);
	}

	/** Returns additional information. */
	FORCEINLINE INT GetItemIndex(WORD MaterialIndex) const
	{
		return 0;
	}

	/** If we should test against this triangle. */ 
	FORCEINLINE UBOOL ShouldCheckMaterial(INT MaterialIndex) const
	{
		return TRUE;
	}

	/**
	 * Returns the kDOPTree for this mesh
	 */
	FORCEINLINE const UStaticMesh::kDOPTreeType& GetkDOPTree(void) const
	{
		return Mesh->kDOPTree;
	}

	/**
	 * Returns the local to world for the component
	 */
	FORCEINLINE const FMatrix& GetLocalToWorld(void) const
	{
		return LocalToWorld;
	}

	/**
	 * Returns the world to local for the component
	 */
	FORCEINLINE const FMatrix GetWorldToLocal(void) const
	{
		return LocalToWorld.Inverse();
	}

	/**
	 * Returns the local to world transpose adjoint for the component
	 */
	FORCEINLINE FMatrix GetLocalToWorldTransposeAdjoint(void) const
	{
		return LocalToWorld.TransposeAdjoint();
	}

	/**
	 * Returns the determinant for the component
	 */
	FORCEINLINE FLOAT GetDeterminant(void) const
	{
		return LocalToWorldDeterminant;
	}
};


// Specialized physical material collision check for Static Meshes
// Calculates the physical material that was hit from the physical material mask on the hit material (if it exists)
template<typename KDOP_IDX_TYPE> 
struct TkDOPPhysicalMaterialCheck<class FStaticMeshCollisionDataProvider, KDOP_IDX_TYPE>
{
	static UPhysicalMaterial* DetermineMaskedPhysicalMaterial(	const class FStaticMeshCollisionDataProvider& CollDataProvider, 
		const FVector& Intersection,
		const FkDOPCollisionTriangle<KDOP_IDX_TYPE>& CollTri,
		KDOP_IDX_TYPE MaterialIndex )
	{
		// The hit physical material
		UPhysicalMaterial* ReturnMat = NULL;

#if !DEDICATED_SERVER
		// Get the hit material interface
		UMaterialInterface* HitMaterialInterface = CollDataProvider.GetMaterial(MaterialIndex);
		// Only call from the game thread and if we are in a game session and
		// don't bother with all this if the material doesn't have a valid mask
		if( IsInGameThread() && GIsGame && HitMaterialInterface && HitMaterialInterface->HasValidPhysicalMaterialMask() )
		{
			// Get the UV channel for the physical material mask
			const INT MaskUVChannel = HitMaterialInterface->GetPhysMaterialMaskUVChannel();
			
			// The UV channel could be invalid if this material type does not support phys mat masks. 
			if( MaskUVChannel != -1 )
			{
				// Get the vertices on the collided triangle
				const FVector& v1 = CollDataProvider.GetVertex(CollTri.v1);
				const FVector& v2 = CollDataProvider.GetVertex(CollTri.v2);
				const FVector& v3 = CollDataProvider.GetVertex(CollTri.v3);

				// Get the UV's from the hit triangle on the masked UV channel
				const FVector2D& UV1 = CollDataProvider.GetVertexUV( CollTri.v1, MaskUVChannel );
				const FVector2D& UV2 = CollDataProvider.GetVertexUV( CollTri.v2, MaskUVChannel ); 
				const FVector2D& UV3 = CollDataProvider.GetVertexUV( CollTri.v3, MaskUVChannel );

				// Get the barycentric coordinates for the hit point on the triangle to determine the uv at the hit point.
				const FVector Barycentric = ComputeBaryCentric2D( Intersection, v1, v2, v3 );

				// Interpolate between each UV of the triangle using the barycentric coordinates as weights
				FVector2D HitUV = Barycentric.X * UV1 + Barycentric.Y * UV2 + Barycentric.Z * UV3;

				// Get the physical material hit
				ReturnMat = HitMaterialInterface->DetermineMaskedPhysicalMaterialFromUV( HitUV );

			}
		}
#endif

		return ReturnMat;
	}
};

/**
 * FStaticMeshComponentReattachContext - Destroys StaticMeshComponents using a given StaticMesh and recreates them when destructed.
 * Used to ensure stale rendering data isn't kept around in the components when importing over or rebuilding an existing static mesh.
 */
class FStaticMeshComponentReattachContext
{
public:

	/** Initialization constructor. */
	FStaticMeshComponentReattachContext(UStaticMesh* InStaticMesh, UBOOL bUnbuildLighting = TRUE):
		StaticMesh(InStaticMesh)
	{
		for(TObjectIterator<UStaticMeshComponent> It;It;++It)
		{
			if(It->StaticMesh == StaticMesh)
			{
				new(ReattachContexts) FComponentReattachContext(*It);

				if (bUnbuildLighting)
				{
					// Invalidate the component's static lighting.
					It->InvalidateLightingCache();
				}
			}
		}

		// Flush the rendering commands generated by the detachments.
		// The static mesh scene proxies reference the UStaticMesh, and this ensures that they are cleaned up before the UStaticMesh changes.
		FlushRenderingCommands();
	}

private:

	UStaticMesh* StaticMesh;
	TIndirectArray<FComponentReattachContext> ReattachContexts;
};

/**
 * A static mesh component scene proxy.
 */
class FStaticMeshSceneProxy : public FPrimitiveSceneProxy
{
protected:
	/** Creates a light cache for the decal if it has a lit material. */
	void CreateDecalLightCache(const FDecalInteraction& DecalInteraction);

public:

	/** Initialization constructor. */
	FStaticMeshSceneProxy(const UStaticMeshComponent* Component);

	virtual ~FStaticMeshSceneProxy() {}

	/** Sets up a FMeshBatch for a specific LOD and element. */
	virtual UBOOL GetMeshElement(INT LODIndex,INT ElementIndex,INT FragmentIndex,BYTE InDepthPriorityGroup,const FMatrix& WorldToLocal,FMeshBatch& OutMeshElement, const UBOOL bUseSelectedMaterial, const UBOOL bUseHoveredMaterial) const;

	/** Sets up a wireframe FMeshBatch for a specific LOD. */
	virtual UBOOL GetWireframeMeshElement(INT LODIndex, const FMaterialRenderProxy* WireframeRenderProxy, BYTE InDepthPriorityGroup, const FMatrix& WorldToLocal, FMeshBatch& OutMeshElement) const;

	// FPrimitiveSceneProxy interface.
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI);

	/** Determines if any collision should be drawn for this mesh. */
	UBOOL ShouldDrawCollision(const FSceneView* View);

	/** Determines if the simple or complex collision should be drawn for a particular static mesh. */
	UBOOL ShouldDrawSimpleCollision(const FSceneView* View, const UStaticMesh* Mesh);

protected:
	/**
	 * @return		The index into DecalLightCaches of the specified component, or INDEX_NONE if not found.
	 */
	INT FindDecalLightCacheIndex(const UDecalComponent* DecalComponent) const;

	/**
	 * Sets IndexBuffer, FirstIndex and NumPrimitives of OutMeshElement.
	 */
	virtual void SetIndexSource(INT LODIndex, INT ElementIndex, INT FragmentIndex, FMeshBatch& OutMeshElement, UBOOL bWireframe, UBOOL bRequiresAdjacencyInformation ) const;

public:

	/**
	* Draws the primitive's dynamic decal elements.  This is called from the rendering thread for each frame of each view.
	* The dynamic elements will only be rendered if GetViewRelevance declares dynamic relevance.
	* Called in the rendering thread.
	*
	* @param	PDI						The interface which receives the primitive elements.
	* @param	View					The view which is being rendered.
	* @param	InDepthPriorityGroup	The DPG which is being rendered.
	* @param	bDynamicLightingPass	TRUE if drawing dynamic lights, FALSE if drawing static lights.
	* @param	bDrawOpaqueDecals		TRUE if we want to draw opaque decals
	* @param	bDrawTransparentDecals	TRUE if we want to draw transparent decals
	* @param	bTranslucentReceiverPass	TRUE during the decal pass for translucent receivers, FALSE for opaque receivers.
	*/
	virtual void DrawDynamicDecalElements(
		FPrimitiveDrawInterface* PDI,
		const FSceneView* View,
		UINT InDepthPriorityGroup,
		UBOOL bDynamicLightingPass,
		UBOOL bDrawOpaqueDecals,
		UBOOL bDrawTransparentDecals,
		UBOOL bTranslucentReceiverPass
		);

	/**
	* Draws the primitive's static decal elements.  This is called from the game thread whenever this primitive is attached
	* as a receiver for a decal.
	*
	* The static elements will only be rendered if GetViewRelevance declares both static and decal relevance.
	* Called in the game thread.
	*
	* @param PDI - The interface which receives the primitive elements.
	*/
	virtual void DrawStaticDecalElements(FStaticPrimitiveDrawInterface* PDI,const FDecalInteraction& DecalInteraction);

	/**
	 * Adds a decal interaction to the primitive.  This is called in the rendering thread by AddDecalInteraction_GameThread.
	 */
	virtual void AddDecalInteraction_RenderingThread(const FDecalInteraction& DecalInteraction);

	/**
	 * Removes a decal interaction from the primitive.  This is called in the rendering thread by RemoveDecalInteraction_GameThread.
	 */
	virtual void RemoveDecalInteraction_RenderingThread(UDecalComponent* DecalComponent);

	/** 
	* Draw the scene proxy as a dynamic element
	*
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param	DPGIndex - current depth priority 
	* @param	Flags - optional set of flags from EDrawDynamicElementFlags
	*/
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags);

	virtual void OnTransformChanged();

	/**
	 * Returns the LOD that should be used for this view.
	 *
	 * @param Distance - distance from the current view to the component's bound origin
	 */
	virtual INT GetLOD(const FSceneView* View) const;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View);

	/**
	 *	Determines the relevance of this primitive's elements to the given light.
	 *	@param	LightSceneInfo			The light to determine relevance for
	 *	@param	bDynamic (output)		The light is dynamic for this primitive
	 *	@param	bRelevant (output)		The light is relevant for this primitive
	 *	@param	bLightMapped (output)	The light is light mapped for this primitive
	 */
	virtual void GetLightRelevance(const FLightSceneInfo* LightSceneInfo, UBOOL& bDynamic, UBOOL& bRelevant, UBOOL& bLightMapped) const;

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() + LODs.GetAllocatedSize() ); }

protected:

	/** Information used by the proxy about a single LOD of the mesh. */
	class FLODInfo : public FLightCacheInterface
	{
	public:

		/** Information about an element of a LOD. */
		struct FElementInfo
		{
			/** Number of index ranges in this element, always 1 for static meshes */
			INT NumFragments;
			UMaterialInterface* Material;

			FElementInfo() :
				NumFragments(1),
				Material(NULL)
			{}
		};
		TArray<FElementInfo> Elements;

		/** Vertex color data for this LOD (or NULL when not overridden), FStaticMeshComponentLODInfo handle the release of the memory */
		FColorVertexBuffer* OverrideColorVertexBuffer;

		/** When the mesh component has overridden the LOD's vertex colors, this vertex factory will be created
		    and passed along to the renderer instead of the mesh's stock vertex factory */
		TScopedPointer< FLocalVertexFactory > OverrideColorVertexFactory;


		/** Initialization constructor. */
		FLODInfo(const UStaticMeshComponent* InComponent,INT InLODIndex);

		/** Destructor */
		virtual ~FLODInfo();

		// Accessors.
		const FLightMap* GetLightMap() const
		{
			return LODIndex < Component->LODData.Num() ?
				Component->LODData(LODIndex).LightMap :
				NULL;
		}

		const TArray<UShadowMap2D*>* GetTextureShadowMaps() const
		{
			return LODIndex < Component->LODData.Num() ?
				&Component->LODData(LODIndex).ShadowMaps :
				NULL;
		}

		const TArray<UShadowMap1D*>* GetVertexShadowMaps() const
		{
			return LODIndex < Component->LODData.Num() ?
				&Component->LODData(LODIndex).ShadowVertexBuffers :
				NULL;
		}

		UBOOL UsesMeshModifyingMaterials() const { return bUsesMeshModifyingMaterials; }

		// FLightCacheInterface.
		virtual FLightInteraction GetInteraction(const FLightSceneInfo* LightSceneInfo) const;

		virtual FLightMapInteraction GetLightMapInteraction() const
		{
			const FLightMap* LightMap = 
				LODIndex < Component->LODData.Num() ?
					Component->LODData(LODIndex).LightMap :
					NULL;
			return LightMap ?
				LightMap->GetInteraction() :
				FLightMapInteraction();
		}

	private:

		/** The static mesh component. */
		const UStaticMeshComponent* const Component;

		/** The LOD index. */
		const INT LODIndex;

		/** True if any elements in this LOD use mesh-modifying materials **/
		UBOOL bUsesMeshModifyingMaterials;
	};

	/** Information about lights affecting a decal. */
	class FDecalLightCache : public FLightCacheInterface
	{
	public:
		FDecalLightCache()
			: DecalComponent( NULL )
			, LightMap( NULL )
		{}

		FDecalLightCache(const FDecalInteraction& DecalInteraction, const FStaticMeshSceneProxy& Proxy);

		/**
		 * @return		The decal component associated with the decal interaction that uses this lighting information.
		 */
		const UDecalComponent* GetDecalComponent() const
		{
			return DecalComponent;
		}

		// FLightCacheInterface.
		virtual FLightInteraction GetInteraction(const FLightSceneInfo* LightSceneInfo) const;

		virtual FLightMapInteraction GetLightMapInteraction() const
		{
			return LightMap ? LightMap->GetInteraction() : FLightMapInteraction();
		}

	private:
		/** The decal component associated with the decal interaction that uses this lighting information. */
		const UDecalComponent* DecalComponent;

		/** A map from persistent light IDs to information about the light's interaction with the primitive. */
		TMap<FGuid,FLightInteraction> StaticLightInteractionMap;

		/** The light-map used by the decal. */
		const FLightMap* LightMap;
	};

	TIndirectArray<FDecalLightCache> DecalLightCaches;

	AActor* Owner;
	const UStaticMesh* StaticMesh;
	const UStaticMeshComponent* StaticMeshComponent;

	TIndirectArray<FLODInfo> LODs;

	/**
	 * The forcedLOD set in the static mesh editor, copied from the mesh component
	 */
	INT ForcedLodModel;

	FLOAT LODMaxRange;

	FVector TotalScale3D;

	FLinearColor LevelColor;
	FLinearColor PropertyColor;

	const BITFIELD bCastShadow : 1;
	const BITFIELD bShouldCollide : 1;
	const BITFIELD bBlockZeroExtent : 1;
	const BITFIELD bBlockNonZeroExtent : 1;
	const BITFIELD bBlockRigidBody : 1;
	const BITFIELD bForceStaticDecal : 1;

	/** The view relevance for all the static mesh's materials. */
	FMaterialViewRelevance MaterialViewRelevance;

	const FLinearColor WireframeColor;

	/**
	 * Returns the minimum distance that the given LOD should be displayed at
	 *
	 * @param CurrentLevel - the LOD to find the min distance for
	 */
	FLOAT GetMinLODDist(INT CurrentLevel) const;

	/**
	 * Returns the maximum distance that the given LOD should be displayed at
	 * If the given LOD is the lowest detail LOD, then its maxDist will be WORLD_MAX
	 *
	 * @param CurrentLevel - the LOD to find the max distance for
	 */
	FLOAT GetMaxLODDist(INT CurrentLevel) const;

	/**
	* @return TRUE if decals can be batched as static elements on this primitive
	*/
	UBOOL UseStaticDecal() const
	{
		// decals should render dynamically on movable meshes 
		return !IsMovable() || bForceStaticDecal;
	}
};

#if WITH_EDITOR
UBOOL StaticMesh_MergeComponents( UObject* Outer, const TArray<UStaticMeshComponent*>& ComponentsToMerge, UStaticMesh** OutStaticMesh, FVector* OutMergeOrigin );
#endif // #if WITH_EDITOR

#endif
