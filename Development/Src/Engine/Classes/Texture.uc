/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class Texture extends Surface
	native(Texture)
	abstract;

// This needs to be mirrored in UnEdFact.cpp.
enum TextureCompressionSettings
{
	TC_Default,
	TC_Normalmap,
	TC_Displacementmap,
	TC_NormalmapAlpha,
	TC_Grayscale,
	TC_HighDynamicRange,
	TC_OneBitAlpha,
	TC_NormalmapUncompressed,
	TC_NormalmapBC5,
	TC_OneBitMonochrome,
	TC_SimpleLightmapModification,
	TC_VectorDisplacementmap
};

// This enum is split out into a separate file so that the platform-specific Tools DLLs can use it as well
`include(Engine\Classes\PixelFormatEnum.uci);

enum TextureFilter
{
	TF_Nearest<DisplayName=Nearest>,
	TF_Linear<DisplayName=Linear>
};

enum TextureAddress
{
	TA_Wrap<DisplayName=Wrap>,
	TA_Clamp<DisplayName=Clamp>,
	TA_Mirror<DisplayName=Mirror>
};

// @warning: if this is changed
//     update BaseEngine.ini SystemSettings
//     update BaseCompat.ini AppCompatBucket 1 2 3
//     update Game's DefaultEngine.ini SystemSettings
//     update Game's BaseCompat.ini AppCompatBucket 1 2 3
//	   update Texture.uc TextureGroupContainer
//     order and actual name can never change (order is important!)
//
// TEXTUREGROUP_Cinematic:  should be used for Cinematics which will be baked out and want to have the highest settings
enum TextureGroup
{
	TEXTUREGROUP_World<DisplayName=World>,
	TEXTUREGROUP_WorldNormalMap<DisplayName=WorldNormalMap>,
	TEXTUREGROUP_WorldSpecular<DisplayName=WorldSpecular>,
	TEXTUREGROUP_Character<DisplayName=Character>,
	TEXTUREGROUP_CharacterNormalMap<DisplayName=CharacterNormalMap>,
	TEXTUREGROUP_CharacterSpecular<DisplayName=CharacterSpecular>,
	TEXTUREGROUP_Weapon<DisplayName=Weapon>,
	TEXTUREGROUP_WeaponNormalMap<DisplayName=WeaponNormalMap>,
	TEXTUREGROUP_WeaponSpecular<DisplayName=WeaponSpecular>,
	TEXTUREGROUP_Vehicle<DisplayName=Vehicle>,
	TEXTUREGROUP_VehicleNormalMap<DisplayName=VehicleNormalMap>,
	TEXTUREGROUP_VehicleSpecular<DisplayName=VehicleSpecular>,
	TEXTUREGROUP_Cinematic<DisplayName=Cinematic>,
	TEXTUREGROUP_Effects<DisplayName=Effects>,
	TEXTUREGROUP_EffectsNotFiltered<DisplayName=EffectsNotFiltered>,
	TEXTUREGROUP_Skybox<DisplayName=Skybox>,
	TEXTUREGROUP_UI<DisplayName=UI>,
	TEXTUREGROUP_Lightmap<DisplayName=Lightmap>,
	TEXTUREGROUP_RenderTarget<DisplayName=RenderTarget>,
	TEXTUREGROUP_MobileFlattened<DisplayName=MobileFlattened>,
	TEXTUREGROUP_ProcBuilding_Face<DisplayName=ProcBuilding_Face>,
	TEXTUREGROUP_ProcBuilding_LightMap<DisplayName=ProcBuilding_LightMap>,
	TEXTUREGROUP_Shadowmap<DisplayName=Shadowmap>,
	TEXTUREGROUP_ColorLookupTable<DisplayName=ColorLookupTable>,
	TEXTUREGROUP_Terrain_Heightmap<DisplayName=Terrain_Heightmap>,
	TEXTUREGROUP_Terrain_Weightmap<DisplayName=Terrain_Weightmap>,
	TEXTUREGROUP_ImageBasedReflection<DisplayName=ImageBasedReflection>,
	TEXTUREGROUP_Bokeh<DisplayName=Bokeh>,
};

// Helper struct to be able to select multiple texture groups in the UI.
// 
// @warning: Must match the TextureGroup enum and must fit 32 bits.
/**
 * Select texture group(s)
 */
struct native TextureGroupContainer
{
	var()	const bool	TEXTUREGROUP_World;
	var()	const bool	TEXTUREGROUP_WorldNormalMap;
	var()	const bool	TEXTUREGROUP_WorldSpecular;
	var()	const bool	TEXTUREGROUP_Character;
	var()	const bool	TEXTUREGROUP_CharacterNormalMap;
	var()	const bool	TEXTUREGROUP_CharacterSpecular;
	var()	const bool	TEXTUREGROUP_Weapon;
	var()	const bool	TEXTUREGROUP_WeaponNormalMap;
	var()	const bool	TEXTUREGROUP_WeaponSpecular;
	var()	const bool	TEXTUREGROUP_Vehicle;
	var()	const bool	TEXTUREGROUP_VehicleNormalMap;
	var()	const bool	TEXTUREGROUP_VehicleSpecular;
	var()	const bool	TEXTUREGROUP_Cinematic;
	var()	const bool	TEXTUREGROUP_Effects;
	var()	const bool	TEXTUREGROUP_EffectsNotFiltered;
	var()	const bool	TEXTUREGROUP_Skybox;
	var()	const bool	TEXTUREGROUP_UI;
	var()	const bool	TEXTUREGROUP_Lightmap;
	var()	const bool	TEXTUREGROUP_RenderTarget;
	var()	const bool	TEXTUREGROUP_MobileFlattened;
	var()	const bool	TEXTUREGROUP_ProcBuilding_Face;
	var()	const bool	TEXTUREGROUP_ProcBuilding_LightMap;
	var()	const bool	TEXTUREGROUP_Shadowmap;
	var()	const bool	TEXTUREGROUP_ColorLookupTable;
	var()	const bool	TEXTUREGROUP_Terrain_Heightmap;
	var()	const bool	TEXTUREGROUP_Terrain_Weightmap;
	var()	const bool	TEXTUREGROUP_ImageBasedReflection;
	var()	const bool	TEXTUREGROUP_Bokeh;
};

enum TextureMipGenSettings
{
	// default for the "texture"
	TMGS_FromTextureGroup<DisplayName=FromTextureGroup>,
	// 2x2 average, default for the "texture group"
	TMGS_SimpleAverage<DisplayName=SimpleAverage>,
	// 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1..little, 5=medium, 10=extreme
	TMGS_Sharpen0<DisplayName=Sharpen0>,
	TMGS_Sharpen1<DisplayName=Sharpen1>,
	TMGS_Sharpen2<DisplayName=Sharpen2>,
	TMGS_Sharpen3<DisplayName=Sharpen3>,
	TMGS_Sharpen4<DisplayName=Sharpen4>,
	TMGS_Sharpen5<DisplayName=Sharpen5>,
	TMGS_Sharpen6<DisplayName=Sharpen6>,
	TMGS_Sharpen7<DisplayName=Sharpen7>,
	TMGS_Sharpen8<DisplayName=Sharpen8>,
	TMGS_Sharpen9<DisplayName=Sharpen9>,
	TMGS_Sharpen10<DisplayName=Sharpen10>,
	TMGS_NoMipmaps<DisplayName=NoMipmaps>,
	// Do not touch existing mip chain as it contains generated data
	TMGS_LeaveExistingMips<DisplayName=LeaveExistingMips>,
	// blur further (useful for image based reflections)
	TMGS_Blur1<DisplayName=Blur1>,
	TMGS_Blur2<DisplayName=Blur2>,
	TMGS_Blur3<DisplayName=Blur3>,
	TMGS_Blur4<DisplayName=Blur4>,
	TMGS_Blur5<DisplayName=Blur5>,
};

enum ETextureMipCount
{
	TMC_ResidentMips,
	TMC_AllMips,
	TMC_AllMipsBiased,
};

//@warning: make sure to update UTexture::PostEditChange if you add an option that might require recompression.

// Texture settings.

var()	bool							SRGB;
var		bool							RGBE;

var()	float							UnpackMin[4],
										UnpackMax[4];

var native const UntypedBulkData_Mirror	SourceArt{FByteBulkData};

/** Has uncompressed source art? */
var		bool							bIsSourceArtUncompressed;

var()	bool							CompressionNoAlpha;
var		bool							CompressionNone;
var		deprecated bool					CompressionNoMipmaps;
var()	bool							CompressionFullDynamicRange;
var()	bool							DeferCompression;

var		bool							NeverStream;

/** When TRUE, the alpha channel of mip-maps and the base image are dithered for smooth LOD transitions. */
var()	bool							bDitherMipMapAlpha;

/** If TRUE, the color border pixels are preserved by mipmap generation.  One flag per color channel. */
var()	bool							bPreserveBorderR;
var()	bool							bPreserveBorderG;
var()	bool							bPreserveBorderB;
var()	bool							bPreserveBorderA;
/** If TRUE, the RHI texture will be created using TexCreate_NoTiling */
var		const bool						bNoTiling;

/** For DXT1 textures, setting this will cause the texture to be twice the size, but better looking, on iPhone */
var(Mobile) bool						bForcePVRTC4;

/** Whether the async resource release process has already been kicked off or not */
var		transient const private bool	bAsyncResourceReleaseHasBeenStarted;

var()	TextureCompressionSettings		CompressionSettings;

/** The texture filtering mode to use when sampling this texture. */
var()	TextureFilter					Filter;

/** Texture group this texture belongs to for LOD bias */
var()	TextureGroup					LODGroup;

/** Cached texture group this texture belongs to for LOD bias*/
var		transient private TextureGroup	CachedLODGroup;

/** A bias to the index of the top mip level to use. */
var()	int								LODBias;

/** Cached combined group and texture LOD bias to use.	*/
var		transient int					CachedCombinedLODBias;

/** Number of mip-levels to use for cinematic quality. */
var()	int								NumCinematicMipLevels;

/** Whether to use the extra cinematic quality mip-levels, when we're forcing mip-levels to be resident. */
var private transient const bool		bUseCinematicMipLevels;

var()	editoronly string				SourceFilePath;         // Path to the resource used to construct this texture
var()	editconst editoronly string				SourceFileTimestamp;    // Date/Time-stamp of the file from the last import

/** The texture's resource. */
var native const pointer				Resource{FTextureResource};

/** Unique ID for this material, used for caching during distributed lighting */
var private const editoronly Guid		LightingGuid;

/** Static texture brightness adjustment (scales HSV value.)  (Non-destructive; Requires texture source art to be available.) */
var() float AdjustBrightness;

/** Static texture curve adjustment (raises HSV value to the specified power.)  (Non-destructive; Requires texture source art to be available.)  */
var() float AdjustBrightnessCurve;

/** Static texture "vibrance" adjustment (0 - 1) (HSV saturation algorithm adjustment.)  (Non-destructive; Requires texture source art to be available.)  */
var() float AdjustVibrance;

/** Static texture saturation adjustment (scales HSV saturation.)  (Non-destructive; Requires texture source art to be available.)  */
var() float AdjustSaturation;

/** Static texture RGB curve adjustment (raises linear-space RGB color to the specified power.)  (Non-destructive; Requires texture source art to be available.)  */
var() float AdjustRGBCurve;

/** Static texture hue adjustment (0 - 360) (offsets HSV hue by value in degrees.)  (Non-destructive; Requires texture source art to be available.)  */
var() float AdjustHue;

/** Internal LOD bias already applied by the texture format (eg TC_NormalMapUncompressed). Used to adjust MinLODMipCount and MaxLODMipCount in CalculateLODBias */
var const int InternalFormatLODBias;

/** Per asset specific setting to define the mip-map generation properties like sharpening and kernel size. */
var() TextureMipGenSettings MipGenSettings;

cpptext
{
	/**
	 * Resets the resource for the texture.
	 */
	void ReleaseResource();

	/**
	 * Creates a new resource for the texture, and updates any cached references to the resource.
	 */
	virtual void UpdateResource();

	/**
	 * Implemented by subclasses to create a new resource for the texture.
	 */
	virtual FTextureResource* CreateResource() PURE_VIRTUAL(UTexture::CreateResource,return NULL;);

	/**
	 * Returns the cached combined LOD bias based on texture LOD group and LOD bias.
	 *
	 * @return	LOD bias
	 */
	INT GetCachedLODBias() const;

	/**
	 * Compresses the texture based on the compression settings. Make sure to update UTexture::PostEditChange
	 * if you add any variables that might require recompression.
	 */
	virtual void Compress();

	/**
	 * Returns whether or not the texture has source art at all
	 *
	 * @return	TRUE if the texture has source art. FALSE, otherwise.
	 */
	virtual UBOOL HasSourceArt() const { return FALSE; }

	/**
	 * Compresses the source art, if needed
	 */
	virtual void CompressSourceArt() {}

	/**
	 * Returns uncompressed source art.
	 *
	 * @param	OutSourceArt	[out]A buffer containing uncompressed source art.
	 */
	virtual void GetUncompressedSourceArt( TArray<BYTE>& OutSourceArt ) {}

	/**
	 * Sets the given buffer as the uncompressed source art. 
	 *
	 * @param	UncompressedData	Uncompressed source art data. 
	 * @param	DataSize			Size of the UncompressedData.
	 */
	virtual void SetUncompressedSourceArt( const void* UncompressedData, INT DataSize ) {}

	/**
	 * Sets the given buffer as the compressed source art.
	 *
	 * @param	CompressedData		Compressed source art data. 
	 * @param	DataSize			Size of the CompressedData.
	 */
	virtual void SetCompressedSourceArt( const void* CompressedData, INT DataSize ) {}

	/**
	 * @return The material value type of this texture.
	 */
	virtual EMaterialValueType GetMaterialType() PURE_VIRTUAL(UTexture::GetMaterialType,return MCT_Texture;);

	/**
	 * Waits until all streaming requests for this texture has been fully processed.
	 */
	virtual void WaitForStreaming()
	{
	}
	
	/**
	 * Updates the streaming status of the texture and performs finalization when appropriate. The function returns
	 * TRUE while there are pending requests in flight and updating needs to continue.
	 *
	 * @param bWaitForMipFading	Whether to wait for Mip Fading to complete before finalizing.
	 * @return					TRUE if there are requests in flight, FALSE otherwise
	 */
	virtual UBOOL UpdateStreamingStatus( UBOOL bWaitForMipFading = FALSE )
	{
		return FALSE;
	}

	// UObject interface.
	virtual void PreEditChange(UProperty* PropertyThatChanged);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();
	virtual void PreSave();
	virtual void BeginDestroy();
	virtual UBOOL IsReadyForFinishDestroy();
	virtual void FinishDestroy();

	/**
	 * Used by various commandlets to purge editor only and platform-specific data from various objects
	 * 
	 * @param PlatformsToKeep Platforms for which to keep platform-specific data
	 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
	 */
	virtual void StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData);

	/**
	 *	Gets the average brightness of the texture (in linear space)
	 *
	 *	@param	bIgnoreTrueBlack		If TRUE, then pixels w/ 0,0,0 rgb values do not contribute.
	 *	@param	bUseGrayscale			If TRUE, use gray scale else use the max color component.
	 *
	 *	@return	FLOAT					The average brightness of the texture
	 */
	virtual FLOAT GetAverageBrightness(UBOOL bIgnoreTrueBlack, UBOOL bUseGrayscale);
	
	/** Helper functions for text output of texture properties... */
	static const TCHAR* GetCompressionSettingsString(TextureCompressionSettings InCompressionSettings);
	static TextureCompressionSettings GetCompressionSettingsFromString(const TCHAR* InCompressionSettingsStr);
	static const TCHAR* GetPixelFormatString(EPixelFormat InPixelFormat);
	static EPixelFormat GetPixelFormatFromString(const TCHAR* InPixelFormatStr);
	static const TCHAR* GetTextureFilterString(TextureFilter InFilter);
	static TextureFilter GetTextureFilterFromString(const TCHAR* InFilterStr);
	static const TCHAR* GetTextureAddressString(TextureAddress InAddress);
	static TextureAddress GetTextureAddressFromString(const TCHAR* InAddressStr);
	static const TCHAR* GetTextureGroupString(TextureGroup InGroup);
	static TextureGroup GetTextureGroupFromString(const TCHAR* InGroupStr);
	static const TCHAR* GetMipGenSettingsString(TextureMipGenSettings InEnum);
	// @param	bTextureGroup	TRUE=TexturGroup, FALSE=Texture otherwise
	static TextureMipGenSettings GetMipGenSettingsFromString(const TCHAR* InStr, UBOOL bTextureGroup);

	static DWORD GetTextureGroupBitfield( const FTextureGroupContainer& TextureGroups );

	virtual void GetTextureOffset_RenderThread(FLinearColor& UVOffset) const {}

	/**
	 * @return TRUE if the compression type is a normal map compression type
	 */
	bool IsNormalMap()
	{
		return	(CompressionSettings == TC_Normalmap) || (CompressionSettings == TC_NormalmapAlpha) || 
				(CompressionSettings == TC_NormalmapBC5) || (CompressionSettings == TC_NormalmapUncompressed);
	}

	/**
	 * Calculates the size of this texture if it had MipCount miplevels streamed in.
	 *
	 * @param	MipCount	Which mips to calculate size for.
	 * @return	Total size of all specified mips, in bytes
	 */
	virtual INT CalcTextureMemorySize( ETextureMipCount MipCount ) const
	{
		return 0;
	}

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
}

defaultproperties
{
	SRGB=True
	UnpackMax(0)=1.0
	UnpackMax(1)=1.0
	UnpackMax(2)=1.0
	UnpackMax(3)=1.0
	Filter=TF_Linear
	AdjustBrightness=1.0
	AdjustBrightnessCurve=1.0
	AdjustVibrance=0.0
	AdjustSaturation=1.0
	AdjustRGBCurve=1.0
	AdjustHue=0.0
	MipGenSettings=TMGS_FromTextureGroup
}
