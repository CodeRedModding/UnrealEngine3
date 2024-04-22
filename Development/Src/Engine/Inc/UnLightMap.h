/*=============================================================================
	UnLightMap.h: Light-map definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/** Whether to compress lightmaps */
extern UBOOL GAllowLightmapCompression;

/** Whether to encode lightmaps as we process (if FALSE, we wait until all mappings are processed before packing/encoding) */
extern UBOOL GAllowEagerLightmapEncode;

/** Whether to use bilinear filtering on lightmaps */
extern UBOOL GUseBilinearLightmaps;

/** Whether to allow padding around mappings. Old-style UE3 lighting doesn't use this. */
extern UBOOL GAllowLightmapPadding;

/** The quality level of DXT encoding for lightmaps (values come from nvtt::Quality enum) */
extern INT GLightmapEncodeQualityLevel;

/** Whether to attempt to optimize light and shadow map size by repacking textures at lower resolutions */
extern UBOOL GRepackLightAndShadowMapTextures;

/** The quality level of the current lighting build */
extern ELightingBuildQuality GLightingBuildQuality;

#if !CONSOLE
/** Debug options for Lightmass */
extern FLightmassDebugOptions GLightmassDebugOptions;
#endif

/** 
 * Set to 1 to allow selecting lightmap texels by holding down T and left clicking in the editor,
 * And having debug information about that texel tracked during subsequent lighting rebuilds.
 * Be sure to set the define with the same name in Lightmass!
 */
#define ALLOW_LIGHTMAP_SAMPLE_DEBUGGING 0

/**
 * The abstract base class of 1D and 2D light-maps.
 */
class FLightMap : private FDeferredCleanupInterface
{
public:
	enum
	{
		LMT_None = 0,
		LMT_1D = 1,
		LMT_2D = 2,
	};

	/** The GUIDs of lights which this light-map stores. */
	TArray<FGuid> LightGuids;

	/** Default constructor. */
	FLightMap(UBOOL InbAllowDirectionalLightMaps=GSystemSettings.bAllowDirectionalLightMaps) : 
		bAllowDirectionalLightMaps(InbAllowDirectionalLightMaps),
		NumRefs(0) 
	{
		if(!GUsingMobileRHI)
		{
#if CONSOLE 
			checkf(bAllowDirectionalLightMaps == TRUE, TEXT("Simple lightmaps are not currently supported on consoles. Make sure Engine.ini : [SystemSettings] : DirectionalLightmaps is TRUE for this platform"));
#endif
		}
	}

	/** Destructor. */
	virtual ~FLightMap() { check(NumRefs == 0); }

	/**
	 * Checks if a light is stored in this light-map.
	 * @param	LightGuid - The GUID of the light to check for.
	 * @return	True if the light is stored in the light-map.
	 */
	UBOOL ContainsLight(const FGuid& LightGuid) const
	{
		return LightGuids.FindItemIndex(LightGuid) != INDEX_NONE;
	}

	// FLightMap interface.
	virtual void AddReferencedObjects( TArray<UObject*>& ObjectArray ) {}
	virtual void Serialize(FArchive& Ar);
	virtual void InitResources() {}
	virtual FLightMapInteraction GetInteraction() const = 0;

	// Runtime type casting.
	virtual class FLightMap1D* GetLightMap1D() { return NULL; }
	virtual const FLightMap1D* GetLightMap1D() const { return NULL; }
	virtual class FLightMap2D* GetLightMap2D() { return NULL; }
	virtual const FLightMap2D* GetLightMap2D() const { return NULL; }

	// Reference counting.
	void AddRef()
	{
		NumRefs++;
	}
	void Release()
	{
		checkSlow(NumRefs > 0);
		if(--NumRefs == 0)
		{
			Cleanup();
		}
	}

	/**
	* @return TRUE if non-simple lightmaps are allowed
	*/
	FORCEINLINE UBOOL AllowsDirectionalLightmaps() const
	{
		return bAllowDirectionalLightMaps;
	}

protected:

	/** Indicates whether the lightmap is being used for directional or simple lighting. */
	const UBOOL bAllowDirectionalLightMaps;

	/**
	 * Called when the light-map is no longer referenced.  Should release the lightmap's resources.
	 */
	virtual void Cleanup()
	{
		BeginCleanup(this);
	}

private:
	INT NumRefs;
	
	// FDeferredCleanupInterface
	virtual void FinishCleanup();
};

/** A reference to a light-map. */
typedef TRefCountPtr<FLightMap> FLightMapRef;

/** Lightmap reference serializer */
extern FArchive& operator<<(FArchive& Ar,FLightMap*& R);

/** Helper class to allow for forcing simple lighting during serialization */
class FLightMapSerializeHelper
{
public:
	FLightMapSerializeHelper(UBOOL InbAllowDirectionalLightMaps,FLightMapRef& InLightMapRef) 
		:	bAllowDirectionalLightMaps(InbAllowDirectionalLightMaps)
		,	LightMapRef(InLightMapRef)
	{}
	const UBOOL bAllowDirectionalLightMaps;
	FLightMapRef& LightMapRef;
};
extern FArchive& operator<<(FArchive& Ar,FLightMapSerializeHelper& R);

/** 
 * Incident lighting for a single sample, as produced by a lighting build. 
 * FGatheredLightSample is used for gathering lighting instead of this format as FLightSample is not additive.
 */
struct FLightSample
{
	/** 
	 * Coefficients[0] stores the normalized average color, 
	 * Coefficients[1] stores the maximum color component in each lightmap basis direction,
	 * and Coefficients[2] stores the simple lightmap which is colored incident lighting along the vertex normal.
	 */
	FLOAT Coefficients[NUM_STORED_LIGHTMAP_COEF][3];

	/** True if this sample maps to a valid point on a triangle.  This is only meaningful for texture lightmaps. */
	UBOOL bIsMapped;

	/** Initialization constructor. */
	FLightSample():
		bIsMapped(FALSE)
	{
		appMemzero(Coefficients,sizeof(Coefficients));
	}
};

/**
 * The raw data which is used to construct a 2D light-map.
 */
class FLightMapData2D
{
public:

	/** The GUIDs of lights which this light-map stores. */
	TArray<FGuid> LightGuids;

	/**
	 * Minimal initialization constructor.
	 */
	FLightMapData2D(UINT InSizeX,UINT InSizeY):
		Data(InSizeX * InSizeY),
		SizeX(InSizeX),
		SizeY(InSizeY)
	{}

	// Accessors.
	const FLightSample& operator()(UINT X,UINT Y) const { return Data(SizeX * Y + X); }
	FLightSample& operator()(UINT X,UINT Y) { return Data(SizeX * Y + X); }
	UINT GetSizeX() const { return SizeX; }
	UINT GetSizeY() const { return SizeY; }

private:

	/** The incident light samples for a 2D array of points on the surface. */
	TChunkedArray<FLightSample> Data;

	/** The width of the light-map. */
	UINT SizeX;

	/** The height of the light-map. */
	UINT SizeY;
};

enum ELightMapPaddingType
{
	LMPT_NormalPadding,
	LMPT_PrePadding,
	LMPT_NoPadding
};

/**
 * Bit-field flags that affects storage (e.g. packing, streaming) and other info about a lightmap.
 */
enum ELightMapFlags
{
	LMF_None			= 0,			// No flags
	LMF_Streamed		= 0x00000001,	// Lightmap should be placed in a streaming texture
	LMF_SimpleLightmap	= 0x00000002,	// Whether this is a simple lightmap or not (directional coefficient)
};

/**
 * A 2D texture containing lightmap coefficients.
 */
class ULightMapTexture2D: public UTexture2D
{
	DECLARE_CLASS_INTRINSIC(ULightMapTexture2D,UTexture2D,0,Engine)

	// UObject interface.
	virtual void InitializeIntrinsicPropertyValues();
	virtual void Serialize( FArchive& Ar );

	/** 
	 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
	 */
	virtual FString GetDesc();

	/** 
	 * Returns detailed info to populate listview columns
	 */
	virtual FString GetDetailedDescription( INT InIndex );

	/** Returns whether this is a simple lightmap or not (directional coefficient). */
	UBOOL IsSimpleLightmap() const
	{
		return (LightmapFlags & LMF_SimpleLightmap) ? TRUE : FALSE;
	}

	/**
	 * Simple light maps can have source art so we can compress during cook for various mobile devices.
	 * Other types do not have source art.
	 */
	virtual UBOOL HasSourceArt() const { 
		if (IsSimpleLightmap())
		{
			return UTexture2D::HasSourceArt();
		}
		return FALSE; 
	}

	/**
	 * Only compress if this is a simple light map.
	 */
	virtual void CompressSourceArt() 
	{
		if (IsSimpleLightmap())
		{
			UTexture2D::CompressSourceArt();
		}
	}

	/**
	 * Return uncompressed source art if this is a simple light map.
	 *
	 * @param	OutSourceArt	[out]A buffer containing uncompressed source art.
	 */
	virtual void GetUncompressedSourceArt( TArray<BYTE>& OutSourceArt ) 
	{
		if (IsSimpleLightmap())
		{
			UTexture2D::GetUncompressedSourceArt(OutSourceArt);
		}
	}

	/**
	 * Set uncompressed source art if this is a simple light map.
	 *
	 * @param	UncompressedData	Uncompressed source art data. 
	 * @param	DataSize			Size of the UncompressedData.
	 */
	virtual void SetUncompressedSourceArt( const void* UncompressedData, INT DataSize ) 
	{
		if (IsSimpleLightmap())
		{
			UTexture2D::SetUncompressedSourceArt(UncompressedData, DataSize);
		}
	}

	/**
	* Set compressed source art if this is a simple light map.
	 *
	 * @param	CompressedData		Compressed source art data. 
	 * @param	DataSize			Size of the CompressedData.
	 */
	virtual void SetCompressedSourceArt( const void* CompressedData, INT DataSize ) 
	{
		if (IsSimpleLightmap())
		{
			UTexture2D::SetCompressedSourceArt(CompressedData, DataSize);
		}
	}


	/** Bit-field with lightmap flags. */
	ELightMapFlags LightmapFlags;
};

/**
 * A 2D array of incident lighting data.
 */
class FLightMap2D : public FLightMap
{
public:

	FLightMap2D(UBOOL InbAllowDirectionalLightMaps=GSystemSettings.bAllowDirectionalLightMaps);

	// FLightMap2D interface.

	/**
	 * Returns the texture containing the RGB coefficients for a specific basis.
	 * @param	BasisIndex - The basis index.
	 * @return	The RGB coefficient texture.
	 */
	const UTexture2D* GetTexture(UINT BasisIndex) const;
	UTexture2D* GetTexture(UINT BasisIndex);

	/**
	 * Returns whether the specified basis has a valid lightmap texture or not.
	 * @param	BasisIndex - The basis index.
	 * @return	TRUE if the specified basis has a valid lightmap texture, otherwise FALSE
	 */
	UBOOL IsValid(UINT BasisIndex) const;

	const FVector2D& GetCoordinateScale() const { return CoordinateScale; }
	const FVector2D& GetCoordinateBias() const { return CoordinateBias; }

	/**
	 * Finalizes the lightmap after encoding, including setting the UV scale/bias for this lightmap 
	 * inside the larger UTexture2D that this lightmap is in
	 * 
	 * @param Scale UV scale (size)
	 * @param Bias UV Bias (offset)
	 * @param ALightmapTexture One of the lightmap textures that this lightmap was put into
	 */
	virtual void FinalizeEncoding(const FVector2D& Scale, const FVector2D& Bias, UTexture2D* ALightmapTexture);

	// FLightMap interface.
	virtual void AddReferencedObjects( TArray<UObject*>& ObjectArray );

	virtual void Serialize(FArchive& Ar);
	virtual FLightMapInteraction GetInteraction() const;

	// Runtime type casting.
	virtual const FLightMap2D* GetLightMap2D() const { return this; }
	virtual FLightMap2D* GetLightMap2D() { return this; }

	/**
	 * Allocates texture space for the light-map and stores the light-map's raw data for deferred encoding.
	 * If the light-map has no lights in it, it will return NULL.
	 * RawData and SourceQuantizedData will be deleted by this function.
	 * @param	LightMapOuter - The package to create the light-map and textures in.
	 * @param	RawData - The raw light-map data to fill the texture with.  
	 * @param	SourceQuantizedData - If the data is already quantized, the values will be in here, and not in RawData.  
	 * @param	Material - The material which the light-map will be rendered with.  Used as a hint to pack light-maps for the same material in the same texture.  Can be NULL.
	 * @param	Bounds - The bounds of the primitive the light-map will be rendered on.  Used as a hint to pack light-maps on nearby primitives in the same texture.
	 * @param	InPaddingType - the method for padding the lightmap.
	 * @param	LightmapFlags - flags that determine how the lightmap is stored (e.g. streamed or not)
	 */
	static class FLightMap2D* AllocateLightMap(UObject* LightMapOuter,FLightMapData2D*& RawData, struct FQuantizedLightmapData*& SourceQuantizedData, 
		UMaterialInterface* Material, const FBoxSphereBounds& Bounds, ELightMapPaddingType InPaddingType, ELightMapFlags InLightmapFlags );

	/**
	 * Executes all pending light-map encoding requests.
	 * @param	bLightingSuccessful	Whether the lighting build was successful or not.
	 * @param	bForceCompletion	Force all encoding to be fully completed (they may be asynchronous).
	 */
	static void EncodeTextures( UBOOL bLightingSuccessful, UBOOL bForceCompletion );

	/**
	 * Static: Repacks light map textures to optimize texture memory usage
	 *
	 * @param	PendingTexture							(In, Out) Array of packed light map textures
	 * @param	InPackedLightAndShadowMapTextureSize	Target texture size for light and shadow maps
	 */
	static void RepackLightMapTextures( TArray<struct FLightMapPendingTexture*>& PendingTextures, const INT InPackedLightAndShadowMapTextureSize );

	/** Call to enable/disable status update of LightMap encoding */
	static void SetStatusUpdate(UBOOL bInEnable)
	{
		bUpdateStatus = bInEnable;
	}

	static UBOOL GetStatusUpdate()
	{
		return bUpdateStatus;
	}

protected:

	friend struct FLightMapPendingTexture;

	FLightMap2D(const TArray<FGuid>& InLightGuids);
	
	/** The textures containing the light-map data. */
	ULightMapTexture2D* Textures[NUM_STORED_LIGHTMAP_COEF];

	/** A scale to apply to the coefficients. */
	FVector4 ScaleVectors[NUM_STORED_LIGHTMAP_COEF];

	/** The scale which is applied to the light-map coordinates before sampling the light-map textures. */
	FVector2D CoordinateScale;

	/** The bias which is applied to the light-map coordinates before sampling the light-map textures. */
	FVector2D CoordinateBias;

	/** If TRUE, update the status when encoding light maps */
	static UBOOL bUpdateStatus;
};


/*-----------------------------------------------------------------------------
	FInstancedLightMap2D
-----------------------------------------------------------------------------*/

class FInstancedLightMap2D : public FLightMap2D
{
public:

	/**
	 * Allocates texture space for the light-map and stores the light-map's raw data for deferred encoding.
	 * If the light-map has no lights in it, it will return NULL.
	 * RawData and SourceQuantizedData will be deleted by this function.
	 * @param	Component - The component that owns this lightmap
	 * @param	InstanceIndex - Which instance in the component this lightmap is for
	 * @param	RawData - The raw light-map data to fill the texture with.  
	 * @param	SourceQuantizedData - If the data is already quantized, the values will be in here, and not in RawData.  
	 * @param	Material - The material which the light-map will be rendered with.  Used as a hint to pack light-maps for the same material in the same texture.  Can be NULL.
	 * @param	Bounds - The bounds of the primitive the light-map will be rendered on.  Used as a hint to pack light-maps on nearby primitives in the same texture.
	 * @param	InPaddingType - the method for padding the lightmap.
	 * @param	LightmapFlags - flags that determine how the lightmap is stored (e.g. streamed or not)
	 * @param	QuantizationScale - The scale to use when quantizing the data
	 */
	static class FInstancedLightMap2D* AllocateLightMap(class UInstancedStaticMeshComponent* Component, INT InstanceIndex, FLightMapData2D*& RawData, struct FQuantizedLightmapData*& SourceQuantizedData, 
		UMaterialInterface* Material, const FBoxSphereBounds& Bounds, ELightMapPaddingType InPaddingType, ELightMapFlags LightmapFlags,
		FLOAT QuantizationScale[NUM_STORED_LIGHTMAP_COEF][3]);

	/**
	 * Finalizes the lightmap after encodeing, including setting the UV scale/bias for this lightmap 
	 * inside the larger UTexture2D that this lightmap is in
	 * 
	 * @param Scale UV scale (size)
	 * @param Bias UV Bias (offset)
	 * @param ALightmapTexture One of the lightmap textures that this lightmap was put into
	 */
	virtual void FinalizeEncoding(const FVector2D& Scale, const FVector2D& Bias, UTexture2D* ALightmapTexture);

private:

	// hide default constructor
	FInstancedLightMap2D()
	{

	}

	FInstancedLightMap2D(class UInstancedStaticMeshComponent* InComponent, INT InInstancedIndex, const TArray<FGuid>& InLightGuids);

	~FInstancedLightMap2D();


	/** Instanced mesh component this lightmap is used with*/
	class UInstancedStaticMeshComponent* Component;

	/** The instance of the component this lightmap is used with. If this is non-zero, this lightmap object is temporary */
	INT InstanceIndex;

	friend class UInstancedStaticMeshComponent;
};

/**
 * The raw data which is used to construct a 1D light-map.
 */
class FLightMapData1D
{
public:

	/** The GUIDs of lights which this light-map stores. */
	TArray<FGuid> LightGuids;

	/**
	 * Minimal initialization constructor.
	 */
	FLightMapData1D(INT Size)
	{
		Data.Empty(Size);
		Data.AddZeroed(Size);
	}

	// Accessors.
	const FLightSample& operator()(UINT Index) const { return Data(Index); }
	FLightSample& operator()(UINT Index) { return Data(Index); }
	INT GetSize() const { return Data.Num(); }

private:

	/** The incident light samples for a 1D array of points. */
	TArray<FLightSample> Data;
};

/**
 * The light incident for a point on a surface in three directions, stored as bytes representing values from 0-1.
 *
 * @warning BulkSerialize: FQuantizedDirectionalLightSample is serialized as memory dump
 * See TArray::BulkSerialize for detailed description of implied limitations.
 */
struct FQuantizedDirectionalLightSample
{
	/** The lighting coefficients, colored. */
	FColor	Coefficients[NUM_DIRECTIONAL_LIGHTMAP_COEF];
};

/**
* The light incident for a point on a surface along the surface normal, stored as bytes representing values from 0-1.
*
* @warning BulkSerialize: FQuantizedSimpleLightSample is serialized as memory dump
* See TArray::BulkSerialize for detailed description of implied limitations.
*/
struct FQuantizedSimpleLightSample
{
	/** The lighting coefficients, colored. */
	FColor	Coefficients[NUM_SIMPLE_LIGHTMAP_COEF];
};

/**
 * Bulk data array of FQuantizedLightSamples
 */
template<class QuantizedLightSampleType>
struct TQuantizedLightSampleBulkData : public FUntypedBulkData
{
	typedef QuantizedLightSampleType SampleType;
	/**
	 * Returns whether single element serialization is required given an archive. This e.g.
	 * can be the case if the serialization for an element changes and the single element
	 * serialization code handles backward compatibility.
	 */
	virtual UBOOL RequiresSingleElementSerialization( FArchive& Ar );

	/**
	 * Returns size in bytes of single element.
	 *
	 * @return Size in bytes of single element
	 */
	virtual INT GetElementSize() const;

	/**
	 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
	 * 
	 * @param Ar			Archive to serialize with
	 * @param Data			Base pointer to data
	 * @param ElementIndex	Element index to serialize
	 */
	virtual void SerializeElement( FArchive& Ar, void* Data, INT ElementIndex );
};

/** A 1D array of incident lighting data. */
class FLightMap1D : public FLightMap, public FVertexBuffer
{
public:

	FLightMap1D(UBOOL InbAllowDirectionalLightMaps=GSystemSettings.bAllowDirectionalLightMaps)
		:	FLightMap(InbAllowDirectionalLightMaps) 
		,	Owner(NULL)
		,	CachedSampleDataSize(0)
		,	CachedSampleData(NULL) 
	{}

	/**
	 * Uses the raw light-map data to construct a vertex buffer.
	 * Data and QuantizedData will be deleted and NULLed out by this function.
	 * @param	Owner - The object which owns the light-map.
	 * @param	Data - The raw light-map data.
	 */
	FLightMap1D(UObject* InOwner, FLightMapData1D*& Data, FQuantizedLightmapData*& QuantizedData);

	/** Destructor. */
	virtual ~FLightMap1D();

	// FLightMap interface.
	virtual void Serialize(FArchive& Ar);
	virtual void InitResources();
	virtual void Cleanup();
	virtual FLightMapInteraction GetInteraction() const
	{
		return FLightMapInteraction::Vertex(this,ScaleVectors,bAllowDirectionalLightMaps);
	}
	TQuantizedLightSampleBulkData<FQuantizedDirectionalLightSample>& GetDirectionalSamples() { return DirectionalSamples; }
	TQuantizedLightSampleBulkData<FQuantizedSimpleLightSample>& GetSimpleSamples() { return SimpleSamples; }

	// Runtime type casting.
	virtual FLightMap1D* GetLightMap1D() { return this; }
	virtual const FLightMap1D* GetLightMap1D() const { return this; }

	// Accessors.
	INT NumSamples() const 
	{ 
		return bAllowDirectionalLightMaps ? DirectionalSamples.GetElementCount() : SimpleSamples.GetElementCount(); 
	}

	/**
	 * Creates a new lightmap that is a copy of this light map, but where the sample data
	 * has been remapped according to the specified sample index remapping.
	 *
	 * @param		SampleRemapping		Sample remapping: Dst[i] = Src[RemappedIndices[i]].
	 * @return							The new lightmap.
	 */
	FLightMap1D* DuplicateWithRemappedVerts(const TArray<INT>& SampleRemapping);

	/**
	 * Gains access to the bulk data for the simple lightmap data
	 * MUST be followed by an EndAccessToSimpleLightSamples() call to unload the data
	 *
	 * @return A buffer full of simple lightmap data
	 */
	FQuantizedSimpleLightSample* BeginAccessToSimpleLightSamples() const;

	/**
	 * Finishes access to the simple light samples and frees up any temp memory
	 *
	 * @param Data The data previously returned by BeginAccessToSimpleLightSamples()
	 */
	void EndAccessToSimpleLightSamples(FQuantizedSimpleLightSample* Data) const;

	/**
	 * @return The scaler to apply to a quantized simple light sample to get to real value
	 */
	const FVector4& GetSimpleLightmapQuantizationScale() const
	{
		return ScaleVectors[SIMPLE_LIGHTMAP_COEF_INDEX];
	}

private:

	/**
	 * Scales, gamma corrects and quantizes the passed in samples and then stores them in BulkData.
	 *
	 * @param BulkData - The bulk data where the quantized samples are stored.
	 * @param Data - The samples to process.
	 * @param InvScale - 1 / max sample value.
	 * @param NumCoefficients - Number of coefficients in the BulkData samples.
	 * @param RelativeCoefficientOffset - Coefficient offset in the Data samples.
	 */
	template<class BulkDataType>
	void QuantizeBulkSamples(
		BulkDataType& BulkData, 
		FLightMapData1D& Data, 
		const FLOAT InvScale[][3], 
		UINT NumCoefficients, 
		UINT RelativeCoefficientOffset);

	/**
	 * Copies already quantized data into the given bulk data
	 *
	 * @param BulkData - The bulk data where the quantized samples are stored.
	 * @param QuantizedData - The pre-quantized samples
	 * @param NumCoefficients - Number of coefficients in the BulkData samples.
	 * @param RelativeCoefficientOffset - Coefficient offset in the Data samples.
	 */
	template<class BulkDataType>
	void CopyQuantizedData(
		BulkDataType& BulkData, 
		FQuantizedLightmapData* QuantizedData, 
		UINT NumCoefficients, 
		UINT RelativeCoefficientOffset);

	/**
	 * Copies samples using the given remapping.
	 *
	 * @param SourceBulkData - The source samples
	 * @param DestBulkData - The destination samples
	 * @param SampleRemapping - Dst[i] = Src[SampleRemapping[i]].
	 * @param NumCoefficients - Number of coefficients in the sample type.
	 */
	template<class BulkDataType>
	void CopyBulkSamples(
		BulkDataType& SourceBulkData, 
		BulkDataType& DestBulkData, 
		const TArray<INT>& SampleRemapping,
		UINT NumCoefficients);

	/** The object which owns the light-map. */
	UObject* Owner;

	/** The incident light samples for a 1D array of points from three directions. */
	TQuantizedLightSampleBulkData<FQuantizedDirectionalLightSample> DirectionalSamples;

	/** The incident light samples for a 1D array of points along the surface normal. */
	TQuantizedLightSampleBulkData<FQuantizedSimpleLightSample> SimpleSamples;

	/** Size of CachedSampleData */
	UINT CachedSampleDataSize;

	/** 
	 * Cached copy of bulk data that is freed by rendering thread and valid between BeginInitResource
	 * and InitRHI.
	 */
	void* CachedSampleData;

	/** A scale to apply to the coefficients. */
	FVector4 ScaleVectors[NUM_STORED_LIGHTMAP_COEF];

	// FRenderResource interface.
	virtual void InitRHI();
};

/** Stores debug information for a lightmap sample. */
struct FSelectedLightmapSample
{
	UPrimitiveComponent* Component;
	INT NodeIndex;
	FLightMapRef Lightmap;
	FVector Position;
	/** Position in the texture mapping */
	INT LocalX;
	INT LocalY;
	INT MappingSizeX;
	INT MappingSizeY;
	/** Position in the lightmap atlas */
	INT LightmapX;
	INT LightmapY;
	INT VertexIndex;
	FColor OriginalColor;
	
	/** Default ctor */
	FSelectedLightmapSample() :
		Component(NULL),
		NodeIndex(INDEX_NONE),
		Lightmap(NULL),
		Position(FVector(0,0,0)),
		LocalX(-1),
		LocalY(-1),
		MappingSizeX(-1),
		MappingSizeY(-1),
		LightmapX(-1),
		LightmapY(-1),
		VertexIndex(INDEX_NONE),
		OriginalColor(FColor(0,0,0))
	{}

	/** Constructor used for a vertex lightmap sample */
	FSelectedLightmapSample(UPrimitiveComponent* InComponent, FLightMapRef& InLightmap, FVector InPosition, INT InVertexIndex) :
		Component(InComponent),
		NodeIndex(INDEX_NONE),
		Lightmap(InLightmap),
		Position(InPosition),
		LocalX(-1),
		LocalY(-1),
		MappingSizeX(-1),
		MappingSizeY(-1),
		LightmapX(-1),
		LightmapY(-1),
		VertexIndex(InVertexIndex),
		OriginalColor(FColor(0,0,0))
	{}

	/** Constructor used for a texture lightmap sample */
	FSelectedLightmapSample(
		UPrimitiveComponent* InComponent, 
		INT InNodeIndex,
		FLightMapRef& InLightmap, 
		FVector InPosition, 
		INT InLocalX,
		INT InLocalY,
		INT InMappingSizeX,
		INT InMappingSizeY) 
		:
		Component(InComponent),
		NodeIndex(InNodeIndex),
		Lightmap(InLightmap),
		Position(InPosition),
		LocalX(InLocalX),
		LocalY(InLocalY),
		MappingSizeX(InMappingSizeX),
		MappingSizeY(InMappingSizeY),
		LightmapX(-1),
		LightmapY(-1),
		VertexIndex(INDEX_NONE),
		OriginalColor(FColor(0,0,0))
	{}
};

class FDebugShadowRay
{
public:
	FVector Start;
	FVector End;
	UBOOL bHit;

	FDebugShadowRay(const FVector& InStart, const FVector& InEnd, UBOOL bInHit) :
		Start(InStart),
		End(InEnd),
		bHit(bInHit)
	{}
};

/**
 * The quantized coefficients for a single light-map texel.
 */
struct FLightMapCoefficients
{
	BYTE Coverage;
	BYTE Coefficients[NUM_STORED_LIGHTMAP_COEF][3];

	/** Equality operator */
	UBOOL operator==( const FLightMapCoefficients& RHS ) const
	{
		return Coverage == RHS.Coverage &&
			   Coefficients == RHS.Coefficients;
	}
};

struct FQuantizedLightmapData
{
	/** Width or a 2D lightmap, or number of samples for a 1D lightmap */
	UINT SizeX;

	/** Height of a 2D lightmap */
	UINT SizeY;

	/** The quantized coefficients */
	TArray<FLightMapCoefficients> Data;

	/** The scale to apply to the quantized coefficients when expanding */
	FLOAT Scale[NUM_STORED_LIGHTMAP_COEF][3];

	/** The GUIDs of lights which this light-map stores. */
	TArray<FGuid> LightGuids;

	/** Environment shadow factor used when previewing unbuilt lighting. */
	BYTE PreviewEnvironmentShadowing;

	UBOOL HasNonZeroData() const;
};

/**
 * Checks if a lightmap texel is mapped or not.
 *
 * @param MappingData	Array of lightmap texels
 * @param X				X-coordinate for the texel to check
 * @param Y				Y-coordinate for the texel to check
 * @param Pitch			Number of texels per row
 * @return				TRUE if the texel is mapped
 */
FORCEINLINE UBOOL IsTexelMapped( const TArray<FLightMapCoefficients>& MappingData, INT X, INT Y, INT Pitch )
{
	const FLightMapCoefficients& Sample = MappingData(Y * Pitch + X);
	UBOOL bIsMapped = (Sample.Coverage > 0);
	return bIsMapped;
}

/**
 * Calculates the minimum rectangle that encompasses all mapped texels.
 *
 * @param MappingData	Array of lightmap/shadowmap texels
 * @param SizeX			Number of texels along the X-axis
 * @param SizeY			Number of texels along the Y-axis
 * @param CroppedRect	[out] Upon return, contains the minimum rectangle that encompasses all mapped texels
 */
template <class TMappingData>
void CropUnmappedTexels( const TMappingData& MappingData, INT SizeX, INT SizeY, FIntRect& CroppedRect )
{
	INT StartX = 0;
	INT StartY = 0;
	INT EndX = SizeX;
	INT EndY = SizeY;

	CroppedRect.Min.X = EndX;
	CroppedRect.Min.Y = EndY;
	CroppedRect.Max.X = StartX - 1;
	CroppedRect.Max.Y = StartY - 1;

	for ( INT Y = StartY; Y < EndY; ++Y )
	{
		UBOOL bIsMappedRow = FALSE;

		// Scan for first mapped texel in this row (also checks whether the row contains a mapped texel at all).
		for ( INT X = StartX; X < EndX; ++X )
		{
			if ( IsTexelMapped( MappingData, X, Y, SizeX ) )
			{
				CroppedRect.Min.X = Min<INT>(CroppedRect.Min.X, X);
				bIsMappedRow = TRUE;
				break;
			}
		}

		// Scan for the last mapped texel in this row.
		for ( INT X = EndX-1; X > CroppedRect.Max.X; --X )
		{
			if ( IsTexelMapped( MappingData, X, Y, SizeX ) )
			{
				CroppedRect.Max.X = X;
				break;
			}
		}

		if ( bIsMappedRow )
		{
			CroppedRect.Min.Y = Min<INT>(CroppedRect.Min.Y, Y);
			CroppedRect.Max.Y = Max<INT>(CroppedRect.Max.Y, Y);
		}
	}

	CroppedRect.Max.X = CroppedRect.Max.X + 1;
	CroppedRect.Max.Y = CroppedRect.Max.Y + 1;
	CroppedRect.Min.X = Min<INT>(CroppedRect.Min.X, CroppedRect.Max.X);
	CroppedRect.Min.Y = Min<INT>(CroppedRect.Min.Y, CroppedRect.Max.Y);
}
