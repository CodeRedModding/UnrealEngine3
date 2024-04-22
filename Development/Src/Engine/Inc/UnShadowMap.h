/*=============================================================================
	UnShadowMap.h: Shadow-map definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * The raw data which is used to construct a 2D shadowmap.
 */
class FShadowMapData2D
{
public:
	virtual ~FShadowMapData2D() {}

	// Accessors.
	UINT GetSizeX() const { return SizeX; }
	UINT GetSizeY() const { return SizeY; }

	// USurface interface
	virtual FLOAT GetSurfaceWidth() const { return SizeX; }
	virtual FLOAT GetSurfaceHeight() const { return SizeY; }

	enum ShadowMapDataType {
		UNKNOWN,
		SHADOW_FACTOR_DATA,
		SHADOW_FACTOR_DATA_QUANTIZED,
		SHADOW_SIGNED_DISTANCE_FIELD_DATA,
		SHADOW_SIGNED_DISTANCE_FIELD_DATA_QUANTIZED,
	};
	virtual ShadowMapDataType GetType() const { return UNKNOWN; }

	virtual void Quantize( TArray<struct FQuantizedShadowSample>& OutData ) const {}
	virtual void Quantize( TArray<struct FQuantizedSignedDistanceFieldShadowSample>& OutData ) const {}

	virtual void Serialize( FArchive* OutShadowMap ) {}

	// Shrink Uniform Shadow maps
	virtual void Shrink() {}

protected:

	FShadowMapData2D(UINT InSizeX,UINT InSizeY):
		SizeX(InSizeX),
		SizeY(InSizeY)
	{
	}

	/** The width of the shadow-map. */
	UINT SizeX;

	/** The height of the shadow-map. */
	UINT SizeY;
};

/**
 * A quantized sample of the visibility factor between a light and a single point.
 */
struct FShadowSample
{
	/** The fraction of light that reaches this point from the light, between 0 and 1. */
	FLOAT Visibility;

	/** True if this sample maps to a valid point on a surface. */
	UBOOL IsMapped;

	/** Equality operator */
	UBOOL operator==( const FShadowSample& RHS ) const
	{
		return Visibility == RHS.Visibility &&
			   IsMapped == RHS.IsMapped;
	}
};

struct FQuantizedShadowSample
{
	enum {NumFilterableComponents = 1};
	BYTE Visibility;
	BYTE Coverage;

	FLOAT GetFilterableComponent(INT Index) const
	{
		checkSlow(Index == 0);
		// Visibility is encoded in gamma space, so convert back to linear before filtering
		return appPow(Visibility / 255.0f, 2.2f);
	}

	void SetFilterableComponent(FLOAT InComponent, INT Index)
	{
		checkSlow(Index == 0);
		// Convert linear value to gamma space for quantization
		Visibility = (BYTE)Clamp<INT>(appTrunc(appPow(InComponent,1.0f / 2.2f) * 255.0f),0,255);
	}

	/** Equality operator */
	UBOOL operator==( const FQuantizedShadowSample& RHS ) const
	{
		return Visibility == RHS.Visibility &&
			   Coverage == RHS.Coverage;
	}

	FQuantizedShadowSample() {}
	FQuantizedShadowSample(const FShadowSample& InSample)
	{
		// Convert linear input values to gamma space before quantizing, which preserves more detail in the darks where banding would be noticeable otherwise
		Visibility = (BYTE)Clamp<INT>(appTrunc(appPow(InSample.Visibility,1.0f / 2.2f) * 255.0f),0,255);
		Coverage = InSample.IsMapped ? 255 : 0;
	}
};

/**
 * A 2D shadow factor map, which consists of FShadowSample's.
 */
class FShadowFactorData2D : public FShadowMapData2D
{
public:

	FShadowFactorData2D(UINT InSizeX,UINT InSizeY)
	:	FShadowMapData2D(InSizeX, InSizeY)
	{
		Data.Empty(SizeX * SizeY);
		Data.AddZeroed(SizeX * SizeY);
	}

	const FShadowSample& operator()(UINT X,UINT Y) const { return Data(SizeX * Y + X); }
	FShadowSample& operator()(UINT X,UINT Y) { return Data(SizeX * Y + X); }

	virtual ShadowMapDataType GetType() const { return SHADOW_FACTOR_DATA; }
	virtual void Quantize( TArray<struct FQuantizedShadowSample>& OutData ) const
	{
		OutData.Empty( Data.Num() );
		OutData.Add( Data.Num() );
		for( INT Index = 0; Index < Data.Num(); Index++ )
		{
			OutData(Index) = Data(Index);
		}
	}

	virtual void Serialize( FArchive* OutShadowMap )
	{
		for( INT Index = 0; Index < Data.Num(); Index++ )
		{
			OutShadowMap->Serialize(&Data(Index), sizeof(FShadowSample));
		}
	}

private:
	TArray<FShadowSample> Data;
};

/**
* A 2D shadow factor map, which consists of FQuantizedShadowSample's.
*/
class FQuantizedShadowFactorData2D : public FShadowMapData2D
{
public:

	FQuantizedShadowFactorData2D(UINT InSizeX,UINT InSizeY)
	:	FShadowMapData2D(InSizeX, InSizeY)
	{
		Data.Empty(SizeX * SizeY);
		Data.AddZeroed(SizeX * SizeY);
	}

	const FQuantizedShadowSample& operator()(UINT X,UINT Y) const { return Data(SizeX * Y + X); }
	FQuantizedShadowSample& operator()(UINT X,UINT Y) { return Data(SizeX * Y + X); }

	virtual ShadowMapDataType GetType() const { return SHADOW_FACTOR_DATA_QUANTIZED; }
	virtual void Quantize( TArray<struct FQuantizedShadowSample>& OutData ) const
	{
		OutData.Empty( Data.Num() );
		OutData.Add( Data.Num() );
		for( INT Index = 0; Index < Data.Num(); Index++ )
		{
			OutData(Index) = Data(Index);
		}
	}

	virtual void Serialize( FArchive* OutShadowMap )
	{
		for( INT Index = 0; Index < Data.Num(); Index++ )
		{
			OutShadowMap->Serialize(&Data(Index), sizeof(FQuantizedShadowSample));
		}
	}

	virtual void Shrink()
	{
		FQuantizedShadowSample FirstData = Data(0);
		UBOOL bConstant = TRUE;
		for (UINT Y = 0; Y < SizeY; Y++)
		{
			for (UINT X = 0; X < SizeX; X++)
			{
				if (Data(X + Y*SizeY ).Coverage <= 0)
				{
					continue;
				}
				else if (!(Data(X + Y*SizeY ) == FirstData) )
				{
					bConstant = FALSE;
					break;
				}
			}
		}
		if (bConstant)
		{
			SizeX = GPixelFormats[PF_DXT1].BlockSizeX;
			SizeY = GPixelFormats[PF_DXT1].BlockSizeY;
			UINT Size = SizeX * SizeY;
			Data.Empty(Size);
			for (UINT Qidx = 0; Qidx < Size; Qidx++)
			{
				Data.AddItem(FirstData);
			}
		}
	}

private:
	TArray<FQuantizedShadowSample> Data;
};

struct FSignedDistanceFieldShadowSample
{
	/** Normalized and encoded distance to the nearest shadow transition, in the range [0, 1], where .5 is at the transition. */
	FLOAT Distance;
	/** Normalized penumbra size, in the range [0,1]. */
	FLOAT PenumbraSize;

	/** True if this sample maps to a valid point on a surface. */
	UBOOL IsMapped;
};

struct FQuantizedSignedDistanceFieldShadowSample
{
	enum {NumFilterableComponents = 2};
	BYTE Distance;
	BYTE PenumbraSize;
	BYTE Coverage;

	FLOAT GetFilterableComponent(INT Index) const
	{
		if (Index == 0)
		{
			return Distance / 255.0f;
		}
		else
		{
			checkSlow(Index == 1);
			return PenumbraSize / 255.0f;
		}
	}

	void SetFilterableComponent(FLOAT InComponent, INT Index)
	{
		if (Index == 0)
		{
			Distance = (BYTE)Clamp<INT>(appTrunc(InComponent * 255.0f),0,255);
		}
		else
		{
			checkSlow(Index == 1);
			PenumbraSize = (BYTE)Clamp<INT>(appTrunc(InComponent * 255.0f),0,255);
		}
	}

	/** Equality operator */
	UBOOL operator==( const FQuantizedSignedDistanceFieldShadowSample& RHS ) const
	{
		return Distance == RHS.Distance &&
			   PenumbraSize == RHS.PenumbraSize &&
			   Coverage == RHS.Coverage;
	}

	FQuantizedSignedDistanceFieldShadowSample() {}
	FQuantizedSignedDistanceFieldShadowSample(const FSignedDistanceFieldShadowSample& InSample)
	{
		Distance = (BYTE)Clamp<INT>(appTrunc(InSample.Distance * 255.0f),0,255);
		PenumbraSize = (BYTE)Clamp<INT>(appTrunc(InSample.PenumbraSize * 255.0f),0,255);
		Coverage = InSample.IsMapped ? 255 : 0;
	}
};

/**
 * A 2D signed distance field map, which consists of FSignedDistanceFieldShadowSample's.
 */
class FShadowSignedDistanceFieldData2D : public FShadowMapData2D
{
public:

	FShadowSignedDistanceFieldData2D(UINT InSizeX,UINT InSizeY)
	:	FShadowMapData2D(InSizeX, InSizeY)
	{
		Data.Empty(SizeX * SizeY);
		Data.AddZeroed(SizeX * SizeY);
	}

	const FSignedDistanceFieldShadowSample& operator()(UINT X,UINT Y) const { return Data(SizeX * Y + X); }
	FSignedDistanceFieldShadowSample& operator()(UINT X,UINT Y) { return Data(SizeX * Y + X); }

	virtual ShadowMapDataType GetType() const { return SHADOW_SIGNED_DISTANCE_FIELD_DATA; }
	virtual void Quantize( TArray<struct FQuantizedSignedDistanceFieldShadowSample>& OutData ) const
	{
		OutData.Empty( Data.Num() );
		OutData.Add( Data.Num() );
		for( INT Index = 0; Index < Data.Num(); Index++ )
		{
			OutData(Index) = Data(Index);
		}
	}

	virtual void Serialize( FArchive* OutShadowMap )
	{
		for( INT Index = 0; Index < Data.Num(); Index++ )
		{
			OutShadowMap->Serialize(&Data(Index), sizeof(FSignedDistanceFieldShadowSample));
		}
	}

private:
	TArray<FSignedDistanceFieldShadowSample> Data;
};

/**
 * A 2D signed distance field map, which consists of FQuantizedSignedDistanceFieldShadowSample's.
 */
class FQuantizedShadowSignedDistanceFieldData2D : public FShadowMapData2D
{
public:

	FQuantizedShadowSignedDistanceFieldData2D(UINT InSizeX,UINT InSizeY)
	:	FShadowMapData2D(InSizeX, InSizeY)
	{
		Data.Empty(SizeX * SizeY);
		Data.AddZeroed(SizeX * SizeY);
	}

	const FQuantizedSignedDistanceFieldShadowSample& operator()(UINT X,UINT Y) const { return Data(SizeX * Y + X); }
	FQuantizedSignedDistanceFieldShadowSample& operator()(UINT X,UINT Y) { return Data(SizeX * Y + X); }

	virtual ShadowMapDataType GetType() const { return SHADOW_SIGNED_DISTANCE_FIELD_DATA_QUANTIZED; }
	virtual void Quantize( TArray<struct FQuantizedSignedDistanceFieldShadowSample>& OutData ) const
	{
		OutData.Empty( Data.Num() );
		OutData.Add( Data.Num() );
		for( INT Index = 0; Index < Data.Num(); Index++ )
		{
			OutData(Index) = Data(Index);
		}
	}

	virtual void Serialize( FArchive* OutShadowMap )
	{
		for( INT Index = 0; Index < Data.Num(); Index++ )
		{
			OutShadowMap->Serialize(&Data(Index), sizeof(FQuantizedSignedDistanceFieldShadowSample));
		}
	}

	virtual void Shrink()
	{
		FQuantizedSignedDistanceFieldShadowSample FirstData = Data(0);
		UBOOL bConstant = TRUE;
		for (UINT Y = 0; Y < SizeY; Y++)
		{
			for (UINT X = 0; X < SizeX; X++)
			{
				if (Data(X + Y*SizeY ).Coverage <= 0)
				{
					continue;
				}
				else if (!(Data(X + Y*SizeY ) == FirstData) )
				{
					bConstant = FALSE;
					break;
				}
			}
		}
		if (bConstant)
		{
			SizeX = GPixelFormats[PF_DXT1].BlockSizeX;
			SizeY = GPixelFormats[PF_DXT1].BlockSizeY;
			UINT Size = SizeX * SizeY;
			Data.Empty(Size);
			for (UINT Qidx = 0; Qidx < Size; Qidx++)
			{
				Data.AddItem(FirstData);
			}
		}
	}

private:
	TArray<FQuantizedSignedDistanceFieldShadowSample> Data;
};

/**
 * Bit-field flags that affects storage (e.g. packing, streaming) and other info about a shadowmap.
 */
enum EShadowMapFlags
{
	SMF_None			= 0,			// No flags
	SMF_Streamed		= 0x00000001,	// Shadowmap should be placed in a streaming texture
	
	SMF_Compressed		= 0x10000000,	// Shadowmap should be compressed with DXT1, this option should not be overlapped

	SMF_DWORD			= 0x7fffffff
};

/**
 * A 2D texture used to store shadow-map data.
 */
class UShadowMapTexture2D : public UTexture2D
{
	DECLARE_CLASS_NOEXPORT(UShadowMapTexture2D,UTexture2D,0,Engine)

	// UObject interface.
	virtual void Serialize( FArchive& Ar );

	/** 
	 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
	 */
	virtual FString GetDesc();

	/** 
	 * Returns detailed info to populate listview columns
	 */
	virtual FString GetDetailedDescription( INT InIndex );

	/**
	 * Overridden to return FALSE because shadow maps don't have source art.
	 *
	 * @return	FALSE always because shadow maps don't have source art.
	 */
	virtual UBOOL HasSourceArt() const { return FALSE; }

	/**
	 * Overridden to not compress source art because shadow maps don't have source art.
	 */
	virtual void CompressSourceArt() {}

	/**
	 * Overridden to not provide uncompressed source art because shadow maps don't have source art.
	 *
	 * @param	OutSourceArt	[out]A buffer containing uncompressed source art. This parameter will be unmodified.
	 */
	virtual void GetUncompressedSourceArt( TArray<BYTE>& OutSourceArt ) {}

	/**
	 * Overridden to not set uncompressed source art because shadow maps don't have source art.
	 *
	 * @param	UncompressedData	Uncompressed source art data. 
	 * @param	DataSize			Size of the UncompressedData.
	 */
	virtual void SetUncompressedSourceArt( const void* UncompressedData, INT DataSize ) {}

	/**
	* Overridden to not set compressed source art because shadow maps don't have source art.
	 *
	 * @param	CompressedData		Compressed source art data. 
	 * @param	DataSize			Size of the CompressedData.
	 */
	virtual void SetCompressedSourceArt( const void* CompressedData, INT DataSize ) {}


	/** Bit-field with shadowmap flags. */
	EShadowMapFlags ShadowmapFlags;
};

struct FShadowMapPendingTexture;

/**
 * A 2D array of shadow-map data.
 */
class UShadowMap2D : public UObject
{
	DECLARE_CLASS_NOEXPORT(UShadowMap2D,UObject,0,Engine);
public:

	// Accessors.
	UShadowMapTexture2D* GetTexture() const { check(IsValid()); return Texture; }
	const FVector2D& GetCoordinateScale() const { check(IsValid()); return CoordinateScale; }
	const FVector2D& GetCoordinateBias() const { check(IsValid()); return CoordinateBias; }
	const FGuid& GetLightGuid() const { return LightGuid; }
	UBOOL IsValid() const { return Texture != NULL; }
	UBOOL IsShadowFactorTexture() const { return bIsShadowFactorTexture; }

	/**
	 * Allocates texture space for the shadow-map and stores the shadow-map's raw data for deferred encoding.
	 * @param	Primitive - The primitive (e.g. component/model) that owns this shadow map
	 * @param	RawData - The raw shadow-map data to fill the texture with.
	 * @param	InLightGuid - The GUID of the light the shadow-map is for.
	 * @param	Material - The material which the shadow-map will be rendered with.  Used as a hint to pack shadow-maps for the same material in the same texture.  Can be NULL.
	 * @param	Bounds - The bounds of the primitive the shadow-map will be rendered on.  Used as a hint to pack shadow-maps on nearby primitives in the same texture.
	 * @param	ShadowmapFlags - Bit-field of EShadowMapFlags for this shadowmap
	 * @param	InstanceIndex - For instanced static mesh shadow maps, which instance in the component this shadow map is for
	 */
	UShadowMap2D( UObject* Primitive, const FShadowMapData2D& RawData,const FGuid& InLightGuid,UMaterialInterface* Material,const FBoxSphereBounds& Bounds, ELightMapPaddingType InPaddingType, EShadowMapFlags InShadowmapFlags, INT InInstanceIndex = 0 );

	/**
	 * Minimal initialization constructor.
	 */
	UShadowMap2D() {}

	/**
	 * Executes all pending shadow-map encoding requests.
	 * @param	bLightingSuccessful	Whether the lighting build was successful or not.
	 */
	static void EncodeTextures( UBOOL bLightingSuccessful );

	/**
	 * Static: Repacks shadow map textures to optimize texture memory usage
	 *
	 * @param	PendingTextures							(In, Out) Array of packed shadow map textures
	 * @param	InPackedLightAndShadowMapTextureSize	Target texture size for light and shadow maps
	 */
	static void RepackShadowMapTextures( TIndirectArray<FShadowMapPendingTexture>& PendingTextures, const INT InPackedLightAndShadowMapTextureSize );

	/**
	 * Constructs mip maps for a single shadowmap texture.
	 */
	template<class ShadowSampleType>
	static void EncodeSingleTexture(FShadowMapPendingTexture& PendingTexture, UShadowMapTexture2D* Texture, TArray< TArray<ShadowSampleType> >& MipData);

	/** Call to enable/disable status update of LightMap encoding */
	static void SetStatusUpdate(UBOOL bInEnable)
	{
		bUpdateStatus = bInEnable;
	}

	static UBOOL GetStatusUpdate()
	{
		return bUpdateStatus;
	}
private:

	/** The texture which contains the shadow-map data. */
	UShadowMapTexture2D* Texture;

	/** The scale which is applied to the shadow-map coordinates before sampling the shadow-map textures. */
	FVector2D CoordinateScale;

	/** The bias which is applied to the shadow-map coordinates before sampling the shadow-map textures. */
	FVector2D CoordinateBias;

	/** The GUID of the light which this shadow-map is for. */
	FGuid LightGuid;

	/** Indicates whether the texture contains shadow factors (0 for shadowed, 1 for unshadowed) or signed distance field values. */
	UBOOL bIsShadowFactorTexture;

	/** Optional instanced mesh component this shadowmap is used with */
	UInstancedStaticMeshComponent* Component;

	/** Optional instance index this shadowmap is used with. If this is non-zero, this shadowmap object is temporary */
	INT InstanceIndex;

	/** If TRUE, update the status when encoding light maps */
	static UBOOL bUpdateStatus;
};

/**
 * The raw data which is used to construct a 1D shadow-map.
 */
class FShadowMapData1D
{
public:

	/**
	 * Minimal initialization constructor.
	 */
	FShadowMapData1D(INT Size)
	{
		Data.Empty(Size);
		Data.AddZeroed(Size);
	}

	// Accessors.
	FLOAT operator()(UINT Index) const { return Data(Index); }
	FLOAT& operator()(UINT Index) { return Data(Index); }
	INT GetSize() const { return Data.Num(); }

private:

	/** The occlusion samples for a 1D array of points. */
	TArray<FLOAT> Data;
};

/**
 * A 1D array of shadow occlusion data.
 */
class UShadowMap1D : public UObject, public FVertexBuffer
{
	DECLARE_CLASS_INTRINSIC(UShadowMap1D,UObject,0,Engine)
public:

	UShadowMap1D()
		:	Samples(FALSE) // samples are not CPU accessible
	{}

	/**
	 * Uses the raw light-map data to construct a vertex buffer.
	 * @param	Data - The raw light-map data.
	 */
	UShadowMap1D(const FGuid& InLightGuid,const FShadowMapData1D& Data);

	// UObject interface.
	virtual void PostLoad();
	virtual void BeginDestroy();
	virtual UBOOL IsReadyForFinishDestroy();

	// Accessors.
	INT NumSamples() const { return Samples.Num(); }
	const FGuid& GetLightGuid() const { return LightGuid; }

	/**
	 * Reorders the samples based on the given reordering map
	 *
	 * @param SampleRemapping The mapping of new sample index to old sample index
	 */
	void ReorderSamples(const TArray<INT>& SampleRemapping);

	/**
	 * Creates a new shadowmap that is a copy of this shadow map, but where the sample data
	 * has been remapped according to the specified sample index remapping.
	 *
	 * @param SampleRemapping	Sample remapping: Dst[i] = Src[RemappedIndices[i]].
	 * @param NewOuter Outer to use for the newly constructed copy
	 * @return The new shadowmap.
	 */
	UShadowMap1D* DuplicateWithRemappedVerts(const TArray<INT>& SampleRemapping, UObject* NewOuter);

	// UObject interface.
	virtual void Serialize(FArchive& Ar);

private:
	/** The incident light samples for a 1D array of points. */
	TResourceArray<FLOAT,VERTEXBUFFER_ALIGNMENT> Samples;

	/** The GUID of the light which this shadow-map is for. */
	FGuid LightGuid;

	/** A fence used to track when the light-map resource has been released. */
	FRenderCommandFence ReleaseFence;

	// FRenderResource interface.
	virtual void InitRHI();
};

struct FMeshEdge
{
	INT	Vertices[2];
	INT	Faces[2];

	friend FArchive& operator<<(FArchive& Ar,FMeshEdge& E)
	{
		return Ar << E.Vertices[0] << E.Vertices[1] << E.Faces[0] << E.Faces[1];
	}
};

