/*=============================================================================
	UnTex.h: Unreal texture related classes.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

//
//	UTexture
//

// This needs to be mirrored in UnTex.h, Texture.uc and UnEdFact.cpp.
enum ETextureCompressionSettings
{
	TC_Default			= 0,
	TC_Normalmap		= 1,
	TC_Displacementmap	= 2,
	TC_NormalmapAlpha	= 3,
	TC_Grayscale		= 4,
	TC_HighDynamicRange = 5,
};

class UTexture : public UObject
{
	DECLARE_ABSTRACT_CLASS(UTexture,UObject,0,Engine);

	// Texture settings.

	BITFIELD			SRGB:1 GCC_PACK(PROPERTY_ALIGNMENT);
	BITFIELD			RGBE:1;
	FPlane				UnpackMin GCC_PACK(PROPERTY_ALIGNMENT);
	FPlane				UnpackMax;

	TLazyArray<BYTE>	SourceArt;

	BITFIELD			CompressionNoAlpha:1 GCC_PACK(PROPERTY_ALIGNMENT);
	BITFIELD			CompressionNone:1;
	BITFIELD			CompressionNoMipmaps:1;
	BITFIELD			CompressionFullDynamicRange:1;
	/** Indicates whether this texture should be considered for streaming (default is true) */
	BITFIELD			NeverStream:1;

	BYTE				CompressionSettings GCC_PACK(PROPERTY_ALIGNMENT);

	// UObject interface.

	virtual void PostEditChange(UProperty* PropertyThatChanged);
	virtual void Serialize(FArchive& Ar);
	/**
	 * @todo: PostLoad currently ensures that RGBE is correctly set to work around a bug in older versions of the engine.
	 */
	virtual void PostLoad();

	// UTexture interface.

	virtual FTextureBase* GetTexture() = 0;
	
	/**
	 * Compresses the texture based on the compression settings. Make sure to update UTexture::PostEditChange
	 * if you add any variables that might require recompression.
	 */
	virtual void Compress();
};

//
//	FPixelFormatInfo
//

struct FPixelFormatInfo
{
	const TCHAR*	Name;
	UINT			BlockSizeX,
					BlockSizeY,
					BlockSizeZ,
					BlockBytes,
					NumComponents;
	/** Platform specific token, e.g. D3DFORMAT with D3DDrv										*/
	DWORD			PlatformFormat;
	/** Platform specific internal flags, e.g. whether SRGB is supported with this format		*/
	DWORD			PlatformFlags;
	/** Whether the texture format is supported on the current platform/ rendering combination	*/
	UBOOL			Supported;
};

extern FPixelFormatInfo GPixelFormats[];		// Maps members of EPixelFormat to a FPixelFormatInfo describing the format.

//
//	CalculateImageBytes
//

extern SIZE_T CalculateImageBytes(UINT SizeX,UINT SizeY,UINT SizeZ,EPixelFormat Format);

//
//	FStaticMipMap2D
//

struct FStaticMipMap2D
{
	TLazyArray<BYTE>	Data;
	UINT				SizeX,
						SizeY;

	// Constructors.

	FStaticMipMap2D() {}
	FStaticMipMap2D(UINT InSizeX,UINT InSizeY,UINT NumBytes):
		SizeX(InSizeX),
		SizeY(InSizeY),
		Data(NumBytes)
	{
	}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FStaticMipMap2D& M)
	{
		Ar << M.Data << M.SizeX << M.SizeY;
		return Ar;
	}
};

//
//	FStaticTexture2D
//

struct FStaticTexture2D: FTexture2D
{
	DECLARE_RESOURCE_TYPE(FStaticTexture2D,FTexture2D);

	TArray<FStaticMipMap2D>	Mips;

	// Constructor.

	FStaticTexture2D();

	// FTexture2D interface.

	virtual void GetData(void* Buffer,UINT MipIndex,UINT StrideY);

	// FStaticTexture2D interface.

	void Serialize(FArchive& Ar);
};



/** 
 * UTexture implementing movie features, including real time updates.
 */
class UTextureMovie : public UTexture, public FTexture2D, public FTickableObject
{
	DECLARE_CLASS(UTextureMovie,UTexture,CLASS_SafeReplace,Engine)

	/** Whether to clamp the texture horizontally. Note that non power of two textures can't be clamped.					*/
	BITFIELD							ClampX:1 GCC_PACK(PROPERTY_ALIGNMENT);
	/** Whether to clamp the texture vertically. Note that non power of two textures can't be clamped.						*/
	BITFIELD							ClampY:1;

	/** Class type of Decoder that will be used to decode RawData.															*/
	UClass*								DecoderClass GCC_PACK(PROPERTY_ALIGNMENT);
	/** Instance of decoder.																								*/
	class UCodecMovie*					Decoder;
	/** Time into the movie in seconds.																						*/
	FLOAT								TimeIntoMovie;
	/** Time that has passed since the last frame. Will be adjusted by decoder to combat drift.								*/
	FLOAT								TimeSinceLastFrame;

	/** Whether the movie is currently paused.																				*/
	BITFIELD							Paused:1 GCC_PACK(PROPERTY_ALIGNMENT);
	/** Whether the movie is currently stopped.																				*/
	BITFIELD							Stopped:1;
	/** Whether the movie should loop when it reaches the end.																*/
	BITFIELD							Looping:1;
	/** Whether the movie should automatically start playing when it is loaded.												*/
	BITFIELD							AutoPlay:1;

	/** Used to hold the currently decoded frame. Requires syncronization as both the decoder and this object access it.	*/
	TArray<BYTE>						CurrentFrame GCC_PACK(PROPERTY_ALIGNMENT);
	/** Raw data containing raw compressed data as imported. Never call unload on this lazy array!							*/
	TLazyArray<BYTE>					RawData;
	/** Size of raw data, needed for streaming directly from disk															*/
	INT									RawSize;


	// Native script functions.

	/** Plays the movie and also unpauses.																					*/
	DECLARE_FUNCTION(execPlay);
	/** Pauses the movie.																									*/	
	DECLARE_FUNCTION(execPause);
	/** Stops movie playback.																								*/
	DECLARE_FUNCTION(execStop);

	
	// FTickableObject interface

	/**
	 * Updates the movie texture if necessary by requesting a new frame from the decoder taking into account both
	 * game and movie framerate.
	 *
	 * @param DeltaTime		Time (in seconds) that has passed since the last time this function has been called.
	 */
	virtual void Tick( FLOAT DeltaTime );

	
	// UTexture interface.

	/**
	 * Returns the object implementing the FTextureBase interface which in the case of movie textures is "this".
	 *
	 * @return Returns a FTextureBase interface used for resource caching.
	 */
	virtual FTextureBase* GetTexture() { return this; }


	// UObject interface.

	void StaticConstructor();
	/**
	 * Serializes the compressed movie data.
	 *
	 * @param Ar	FArchive to serialize RawData with.
	 */
	virtual void Serialize(FArchive& Ar);
	
	/**
	 * Postload initialization of movie texture. Creates decoder object and retriever first frame.
	 */
	virtual void PostLoad();
	/**
	 * Destroy - gets called by ConditionalDestroy from within destructor. We need to ensure that the decoder 
	 * doesn't have any references to RawData before destructing it.
	 */
	virtual void Destroy();
	/**
	 * PostEditChange - gets called whenever a property is either edited via the Editor or the "set" console command.
	 * In this particular case we just make sure that non power of two textures have ClampX/Y set to true as wrapping
	 * isn't supported on them.
	 *
	 * @param PropertyThatChanged	Property that changed
	 */
	virtual void PostEditChange(UProperty* /*PropertyThatChanged*/);


	// FTexture2D interface.

	/**
	 * Returns whether the texture should be clamped horizontally.
	 *
	 * @return TRUE if the texture should be clamped horizontally, FALSE if it should be wrapped
	 */
	virtual UBOOL GetClampX() { return ClampX; }
	/**
	 * Returns whether the texture should be clamped vertically.
	 *
	 * @return TRUE if the texture should be clamped vertically, FALSE if it should be wrapped
	 */
	virtual UBOOL GetClampY() { return ClampY; }
	/**
	 * Copies the current uncompressed movie frame to the passed in Buffer.
	 *
	 * @param Buffer	Pointer to destination data to upload uncompressed movie frame
	 * @param MipIndex	unused
	 * @param StrideY	StrideY for destination data.
	 */
	virtual void GetData(void* Buffer,UINT /*MipIndex*/,UINT StrideY);


	// FTextureBase interface.

	/**
	 * Returns the lower bound of the expansion range.
	 *
	 * @return lower bound of the expansion range
	 */
	virtual FPlane GetUnpackMin() { return UnpackMin; }
	/**
	 * Returns the upper bound of the expansion range.
	 *
	 * @return upper bound of the expansion range
	 */
	virtual FPlane GetUnpackMax() { return UnpackMax; }
	/**
	 * Returns whether the texture is already gamma corrected.
	 *
	 * @return TRUE if gamma corrected, FALSE otherwise.
	 */
	virtual UBOOL IsGammaCorrected() { return SRGB; }
	/**
	 * Returns whether the texture use Gregory Ward's high dynamic range RGBE format.
	 *
	 * @return TRUE if RGBE is used, FALSE otherwise.
	 */
	virtual UBOOL IsRGBE() { return RGBE; }
	
	/**
	 * Returns the UTexture interface associated, aka this as we inherit from UTexture.
	 */
	virtual class UTexture* GetUTexture() { return this; }


	// FResource interface.

	/**
	 * Returns a human readable description of the resource, in this case
	 * the full name.
	 *
	 * *return Fully qualified object name. 
	 */
	virtual FString DescribeResource() { return GetPathName(); }

	// Thumbnail interface.

	virtual FString GetDesc();
	virtual void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	virtual FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	virtual INT GetThumbnailLabels( TArray<FString>* InLabels );

	// Helper.

	/**
	 * Updates texture format from decoder and re- allocates memory for the intermediate buffer to hold one frame in the decoders texture format.
	 */
	void UpdateTextureFormat();
};


//
//	UTexture2D
//

class UTexture2D : public UTexture, public FStaticTexture2D
{
	DECLARE_CLASS(UTexture2D,UTexture,CLASS_SafeReplace,Engine)

	BITFIELD							ClampX:1 GCC_PACK(PROPERTY_ALIGNMENT);
	BITFIELD							ClampY:1;
	
	// FTextureBase interface.

	virtual FPlane GetUnpackMin() { return UnpackMin; }
	virtual FPlane GetUnpackMax() { return UnpackMax; }
	virtual UBOOL IsGammaCorrected() { return SRGB; }
	virtual UBOOL IsRGBE() { return RGBE; }
	
	/**
	 * Returns the UTexture interface associated, aka this as we inherit from UTexture.
	 */
	virtual class UTexture* GetUTexture() { return this; }

	// FTexture2D interface.

	virtual UBOOL GetClampX() { return ClampX; }
	virtual UBOOL GetClampY() { return ClampY; }

	// FResource interface.

	virtual FString DescribeResource() { return GetPathName(); }

	/**
	 *	Returns whether this UTexture2D can be streamed in. The default behaviour is to allow streaming and
	 *	UTexture exposes a NeverStream override to artists which can be used to diallow streaming for e.g.
	 *	HUD and menu textures.
	 *
	 *	@return TRUE if resource can be streamed (default case), FALSE if artist explictely disabled streaming
	 */
	virtual UBOOL CanBeStreamed() { return !NeverStream; }

	// UObject interface.

	void StaticConstructor();
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();

	// UTexture interface.

	virtual FTextureBase* GetTexture() { return this; }
	virtual void Compress();

	// UTexture2D interface.

	void Init(UINT InSizeX,UINT InSizeY,EPixelFormat InFormat);
	void CreateMips(UINT NumMips,UBOOL Generate);
	void Clear(FColor Color);

	virtual FString GetDesc();
	virtual void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	virtual FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	virtual INT GetThumbnailLabels( TArray<FString>* InLabels );
};

//
//	FStaticMipMap3D
//

struct FStaticMipMap3D
{
	TLazyArray<BYTE>	Data;
	UINT				SizeX,
						SizeY,
						SizeZ;

	// Constructors.

	FStaticMipMap3D() {}
	FStaticMipMap3D(UINT InSizeX,UINT InSizeY,UINT InSizeZ,UINT NumBytes):
		SizeX(InSizeX),
		SizeY(InSizeY),
		SizeZ(InSizeZ),
		Data(NumBytes)
	{
	}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FStaticMipMap3D& M)
	{
		Ar << M.Data << M.SizeX << M.SizeY << M.SizeZ;
		return Ar;
	}
};

//
//	FStaticTexture2D
//

struct FStaticTexture3D: FTexture3D
{
	DECLARE_RESOURCE_TYPE(FStaticTexture3D,FTexture3D);

	TArray<FStaticMipMap3D>	Mips;

	// Constructor.

	FStaticTexture3D();

	// FTexture3D interface.

	virtual void GetData(void* Buffer,UINT MipIndex,UINT StrideY,UINT StrideZ);

	// FStaticTexture3D interface.

	void Serialize(FArchive& Ar);
};

//
//	UTexture2D
//

class UTexture3D : public UTexture, public FStaticTexture3D
{
	DECLARE_CLASS(UTexture3D,UTexture,CLASS_SafeReplace,Engine)

	BITFIELD							ClampX:1 GCC_PACK(PROPERTY_ALIGNMENT);
	BITFIELD							ClampY:1;
	BITFIELD							ClampZ:1;

	// FTextureBase interface.

	virtual FPlane GetUnpackMin() { return UnpackMin; }
	virtual FPlane GetUnpackMax() { return UnpackMax; }
	virtual UBOOL IsGammaCorrected() { return SRGB; }
	virtual UBOOL IsRGBE() { return RGBE; }
	
	/**
	 * Returns the UTexture interface associated, aka this as we inherit from UTexture.
	 */
	virtual class UTexture* GetUTexture() { return this; }

	// FTexture3D interface.

	virtual UBOOL GetClampX() { return ClampX; }
	virtual UBOOL GetClampY() { return ClampY; }
	virtual UBOOL GetClampZ() { return ClampZ; }

	// FResource interface.

	virtual FString DescribeResource() { return GetPathName(); }

	// UObject interface.

	void StaticConstructor();
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();

	// UTexture interface.

	virtual FTextureBase* GetTexture() { return this; }
	virtual void Compress();

	// UTexture3D interface.

	void Init(UINT InSizeX,UINT InSizeY,UINT InSizeZ,EPixelFormat InFormat);

	virtual FString GetDesc();
	virtual void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	virtual FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	virtual INT GetThumbnailLabels( TArray<FString>* InLabels );
};

//
//	UTextureCube
//

class UTextureCube : public UTexture, public FTextureCube
{
	DECLARE_CLASS(UTextureCube,UTexture,CLASS_SafeReplace,Engine)

	UTexture2D* Faces[6];
	UBOOL		Valid;

	// FTextureBase interface.

	virtual FPlane GetUnpackMin() { return UnpackMin; }
	virtual FPlane GetUnpackMax() { return UnpackMax; }
	virtual UBOOL IsGammaCorrected() { return SRGB; }
	virtual UBOOL IsRGBE() { return RGBE; }

	// FTextureCube interface.

	virtual void GetData(void* Buffer,UINT FaceIndex,UINT MipIndex,UINT StrideY);

	// UObject interface.

	void StaticConstructor();
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();
	virtual void PostEditChange(UProperty* PropertyThatChanged);

	// UTexture interface.

	virtual FTextureBase* GetTexture();

	virtual FString GetDesc();
	virtual void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	virtual FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	virtual INT GetThumbnailLabels( TArray<FString>* InLabels );
};



/**
 * A 2D texture object storing shadow map data in PF_G8 format. Includes helper functions to generate
 * mipmaps based on coverage information.
 */
class UShadowMap : public UTexture, public FStaticTexture2D
{
	DECLARE_CLASS(UShadowMap,UTexture,CLASS_SafeReplace,Engine)
	DECLARE_RESOURCE_TYPE(UShadowMap,FStaticTexture2D);

	/** Light component this texture is associated with */
	class ULightComponent* Light;

	// UShadowMap interface.

	/**
	 * Initializes the texture and allocates room in SourceArt array for visibility and coverage data. Please
	 * note that the texture is not ready for rendering at this point as there aren't ANY miplevels yet.
	 *
	 * @param	Size	Width/ height of lightmap
	 * @param	Light	Light component to associate with texture
	 * @return			Offset into SourceArt where coverage information is expected.
	 */
	UINT Init( UINT Size, class ULightComponent* Light );

	/**
	 * Creates one or more miplevels based on visibility and potentially coverage information stored in the
	 * SourceArt array and cleans out SourceArt afterwards as it doesn't make sense to store three times the
	 * data necessary for something that can easily be recalculated by rebuilding lighting.
	 *
	 * @param	CreateMips	Whether to create miplevels or not
	 */
	void CreateFromSourceArt( UBOOL CreateMips );

	/**
	 * Downsample the passed in source coverage and visibility information. The filter used is a simple box
	 * filter albeit only taking into account covered samples.
	 *
	 * @param	SrcCoverage		Pointer to the source coverage data
	 * @param	SrcVisibility	Pointer to the source visibility data
	 * @param	DstCoverage		Pointer to the destination coverage data
	 * @param	DstVisibility	Pointer to the destination visibility data
	 * @param	DstDimension	Width and height of the destination image
	 */
	void DownSample( BYTE* SrcCoverage, BYTE* SrcVisibility, BYTE* DstCoverage, BYTE* DstVisibility, UINT DstDimension );

	/**
	 * Upsample the passed in source coverage and visibility information and fill in uncovered areas in the
	 * destination image. The source image has full coverage and we fill in uncovered areas in the destination
	 * by looking at the source. Please note that this function does NOT change any already covered pixels.
	 *
	 * @param	SrcCoverage		Pointer to the source coverage data
	 * @param	SrcVisibility	Pointer to the source visibility data
	 * @param	DstCoverage		Pointer to the destination coverage data
	 * @param	DstVisibility	Pointer to the destination visibility data
	 * @param	DstDimension	Width and height of the destination image
	 */
	void UpSample( BYTE* SrcCoverage, BYTE* SrcVisibility, BYTE* DstCoverage, BYTE* DstVisibility, UINT DstDimension );

	// UObject interface.

	/**
	 * Serializes the lightmap data.
	 *
	 * @param Ar	FArchive to serialize object with.
	 */
	virtual void Serialize( FArchive& Ar );

	// UTexture interface.

	/**
	 * Returns the FTextureBase interface of this texture.
	 *
	 * @return	"this" as we directly inherit from FTextureBase
	 */
	virtual FTextureBase* GetTexture() { return this; }

	// Editor thumbnail interface.
	virtual FString GetDesc();
	virtual void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	virtual FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	virtual INT GetThumbnailLabels( TArray<FString>* InLabels );
};

/*-----------------------------------------------------------------------------
	UFont.
-----------------------------------------------------------------------------*/

struct FFontCharacter
{
	// Variables.
	INT StartU, StartV;
	INT USize, VSize;
	BYTE TextureIndex;

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FFontCharacter& Ch )
	{
		return Ar << Ch.StartU << Ch.StartV << Ch.USize << Ch.VSize << Ch.TextureIndex;
	}
};

//
// A font object, containing information about a set of glyphs.
// The glyph bitmaps are stored in the contained textures, while
// the font database only contains the coordinates of the individual
// glyph.
//
class UFont : public UObject
{
	DECLARE_CLASS(UFont,UObject,0,Engine)

	// Variables.

	TArray<FFontCharacter>	Characters;
	TArray<UTexture2D*>		Textures;
	
	TMap<TCHAR,TCHAR> CharRemap;
	UBOOL IsRemapped;
    INT Kerning;

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UFont interface
	TCHAR RemapChar(TCHAR ch)
	{
		TCHAR *p;
		if( !IsRemapped )
			return ch;
		p = CharRemap.Find(ch);
		return p ? *p : 32; // return space if not found.
	}

	void GetCharSize(TCHAR InCh, INT& Width, INT& Height);
};

//
//	DXTCompress
//

void DXTCompress(FColor* SrcData,INT Width,INT Height,EPixelFormat DestFormat,TArray<BYTE>& DestData);

