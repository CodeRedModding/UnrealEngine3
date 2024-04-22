/*=============================================================================
	MobileSupport.h: Mobile related enums and functions needed by the editor/cooker/mobile platforms

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if WITH_GFx
#include "../../GFxUI/Src/Render/RHI_Shaders.h"
#endif

#define MINIMIZE_ES2_SHADERS 1

// Version of the cooked CachedProgramKeys.txt file
#define SHADER_MANIFEST_VERSION (1)

////////////////////////////////////
//
// Enums
// 
////////////////////////////////////

//Mobile platforms
enum EMobilePlatformFeatures
{
	EPF_HighEndFeatures,
	EPF_LowEndFeatures,

	EPF_MAX,
};


/**
 * The scene rendering stats.
 */
enum EMobileTextureUnit
{
	Base_MobileTexture,
	LandscapeLayer0_MobileTexture = Base_MobileTexture,
	Detail_MobileTexture,
	LandscapeLayer1_MobileTexture = Detail_MobileTexture,
	Lightmap_MobileTexture,
	Normal_MobileTexture,
	Environment_MobileTexture,
	LandscapeLayer2_MobileTexture = Environment_MobileTexture,
	Mask_MobileTexture,
	Emissive_MobileTexture,
	LandscapeLayer3_MobileTexture = Emissive_MobileTexture,
	Lightmap2_MobileTexture,

	MAX_Mapped_MobileTexture,
	//have to re-use the first 8 slots so mobile devices don't cause silent errors/stalls	
	Detail_MobileTexture2 = MAX_Mapped_MobileTexture,
	Detail_MobileTexture3,
	
	MAX_MobileTexture
};

enum EMobilePrimitiveType
{
	EPT_Default,	//EPT_DefaultTextureLit, EPT_DefaultUnlit, EPT_SkeletalMesh
	EPT_Particle,	//EPT_ParticleSubUV
	EPT_BeamTrailParticle,
	EPT_LensFlare,
	EPT_Simple,
	EPT_DistanceFieldFont,
	EPT_GlobalShader,	// Note: All other bits in the key changes meaning - they now specify which type of global shader it is.
	EPT_MAX
};

/**
 * For global shaders.
 */
enum EMobileGlobalShaderType
{
	EGST_None,			// (Meaning it's not a global shader.)
	EGST_GammaCorrection,
	EGST_Filter1,
	EGST_Filter4,
	EGST_Filter16,
	EGST_LUTBlender,
	EGST_UberPostProcess,
	EGST_LightShaftDownSample,
	EGST_LightShaftDownSample_NoDepth,
	EGST_LightShaftBlur,
	EGST_LightShaftApply,
	EGST_SimpleF32,
	EGST_PositionOnly,
	EGST_ShadowProjection,
	EGST_BloomGather,
	EGST_DOFAndBloomGather,
	// permutations: bit2:ColorGrading bit1:DOF bit0:Bloom
	EGST_MobileUberPostProcess1,	// Bloom
	EGST_MobileUberPostProcess3,	// Bloom, DOF
	EGST_MobileUberPostProcess4,	// ColorGrading
	EGST_MobileUberPostProcess5,	// ColorGrading, Bloom
	EGST_MobileUberPostProcess7,	// ColorGrading, Bloom, DOF
	EGST_VisualizeTexture,
	EGST_RadialBlur,
	EGST_FXAA,
#if WITH_GFx
	EGST_GFxBegin,
	EGST_GFxEnd = EGST_GFxBegin + Scaleform::Render::RHI::FragShaderDesc::FS_Count * 2 - 1,
#endif
	EGST_MAX,
};

enum EMobileGFxBlendMode
{
    EGFXBM_Disabled, // Use material blend mode
    EGFXBM_Normal,
    EGFXBM_Add,
    EGFXBM_Subtract,
    EGFXBM_Multiply,
    EGFXBM_Darken,
    EGFXBM_Lighten,
	EGFXBM_None,
    EGFXBM_SourceAc = 8,
    EGFXBM_DestAc = 16,
	EGFXBM_NoColorWrite = EGFXBM_None | EGFXBM_SourceAc | EGFXBM_DestAc,
    EGFXBM_MAX
};

/**
 * For depth shaders.
 * These can be skinned or non-skinned (using PKDT_IsSkinned). 
 * PKDT_DepthShaderType is used instead of PKDT_EmissiveColorSource.
 */
enum EMobileDepthShaderType
{
	MobileDepthShader_None,
	MobileDepthShader_Normal,	// Render depth normally
	MobileDepthShader_Shadow,	// Render linear shadow depth

	MobileDepthShader_MAX
};

/** Struct for defining shader program key fields */
struct ES2ShaderProgramKeyField
{
	INT NumBits;
	INT MaxValue;
};

#define MAX_PROGRAM_FIELD_DATA_SIZE		64
/** Program key field data */
struct FProgramKeyFieldData
{
	BYTE FieldValue[MAX_PROGRAM_FIELD_DATA_SIZE];
	BYTE bFieldSet[MAX_PROGRAM_FIELD_DATA_SIZE];
	BYTE bFieldLocked[MAX_PROGRAM_FIELD_DATA_SIZE];
	BYTE NumberFieldsSet;
};

/**
 * Convert mobile texture enum to a "useable" value that doesn't exceed the number of texture slots
 * @param TextureUnit - The texture enum that is attempting to be set
 */
FORCEINLINE UINT GetDeviceValidMobileTextureSlot (const UINT TextureUnit)
{
	INT FinalTextureUnit = TextureUnit;
	switch (TextureUnit)
	{
		case Detail_MobileTexture2:
			FinalTextureUnit = Environment_MobileTexture;
			break;
		case Detail_MobileTexture3:
			FinalTextureUnit = Mask_MobileTexture;
			break;
	}
	return FinalTextureUnit;
}

/**
 * Converts a string with hexadecimal digits into a 64-bit QWORD.
 *
 * @param In	String to convert
 * @return		64-bit QWORD
 */
extern QWORD HexStringToQWord(const TCHAR *In);

/** Binary program key */
struct FProgramKey
{
	/** Constructor. Empties the key. */
	FProgramKey()
	{
		Reset();
	}

	/** Explicit conversion constructor. Converts from string representation to binary key. */
	explicit FProgramKey( const FString& KeyString )
	{
		FromString( KeyString );
	}

	/** Empties the key. */
	void	Reset()
	{
		Data[0] = Data[1] = 0;
	}

	/** Converts from hexadecimal string representation to binary key. */
	void FromString( const FString& KeyString )
	{
		// Parse the string into the two QWORDS
		INT UnderscoreIdx = KeyString.InStr(TEXT("_"));
		checkf((UnderscoreIdx != INDEX_NONE), TEXT("UnpackageProgramKeyData given invalid PackedKey - %s"), *KeyString);

		FString KeyValue1Str = KeyString.Left(UnderscoreIdx);
		FString KeyValue0Str = KeyString.Right(KeyString.Len() - UnderscoreIdx - 1);

		Data[1] = HexStringToQWord(*KeyValue1Str);
		Data[0] = HexStringToQWord(*KeyValue0Str);
	}

	/** Converts the binary key to a hexadecimal string representation. */
	FString	ToString() const
	{
		return FString::Printf(TEXT("0x%llx_0x%llx"), Data[1], Data[0]);
	}

	/**
	 * Compares this key with another key, using ascending sort.
	 *
	 * @param	Other	The other key to compare against
	 * @return	< 0 is this < Other, 0 if this == Other, > 0 if this > Other
	 */
	FORCEINLINE INT Compare( const FProgramKey& Other ) const
	{
		for ( INT Index=0; Index < ARRAY_COUNT(Data); ++Index )
		{
			if ( Data[Index] > Other.Data[Index] )
			{
				return 1;
			}
			if ( Data[Index] < Other.Data[Index] )
			{
				return -1;
			}
		}
		return 0;
	}

	FORCEINLINE UBOOL operator==( const FProgramKey& Other ) const
	{
		for ( INT Index=0; Index < ARRAY_COUNT(Data); ++Index )
		{
			if ( Data[Index] != Other.Data[Index] )
			{
				return FALSE;
			}
		}
		return TRUE;
	}

	FORCEINLINE UBOOL operator!=( const FProgramKey& Other ) const
	{
		return !(*this == Other);
	}

	FORCEINLINE UBOOL operator<( const FProgramKey& Other ) const
	{
		for ( INT Index=0; Index < ARRAY_COUNT(Data); ++Index )
		{
			if ( Data[Index] >= Other.Data[Index] )
			{
				return FALSE;
			}
		}
		return TRUE;
	}

	FORCEINLINE UBOOL operator>( const FProgramKey& Other ) const
	{
		for ( INT Index=0; Index < ARRAY_COUNT(Data); ++Index )
		{
			if ( Data[Index] <= Other.Data[Index] )
			{
				return FALSE;
			}
		}
		return TRUE;
	}

	FORCEINLINE UBOOL operator<=( const FProgramKey& Other ) const
	{
		return !(*this > Other);
	}

	FORCEINLINE UBOOL operator>=( const FProgramKey& Other ) const
	{
		return !(*this < Other);
	}

	/**
	 * Serialization
	 */
	friend FArchive& operator<<( FArchive& Ar, FProgramKey& Key )
	{
		for ( INT Index=0; Index < ARRAY_COUNT(Key.Data); ++Index )
		{
			Ar << Key.Data[Index];
		}
		return Ar;
	}

	QWORD	Data[2];
};

/**
 * Computes a hash value from a binary program key. Used by TMap, etc.
 */
FORCEINLINE DWORD GetTypeHash( const FProgramKey& Key )
{
	DWORD Hash = GetTypeHash( Key.Data[0] );
	for ( INT Index=1; Index < ARRAY_COUNT(Key.Data); ++Index )
	{
		Hash ^= GetTypeHash( Key.Data[Index] );
	}
	return Hash;
}

/**
 * A simple structure used to pass around program key data values
 */
struct FProgramKeyData
{
public:
/**
 * An enum that defines all of the components used to generate program keys
 */
enum EProgramKeyDataTypes
{
		//Reserved for primitive type && platform features
		PKDT_PlatformFeatures,						//0
	PKDT_PrimitiveType,

	//World/Misc provided
		PKDT_IsDepthOnlyRendering,					// See EMobileDepthShaderType to determine the type
	PKDT_IsGradientFogEnabled,
	PKDT_ParticleScreenAlignment,
		PKDT_UseGammaCorrection,					//5

	//Vertex Factory Flags
	PKDT_IsLightmap,
	PKDT_IsSkinned,
	PKDT_IsDecal,
	PKDT_IsSubUV,

	//Material provided
		PKDT_BlendMode,								//10
	PKDT_BaseTextureTexCoordsSource,
	PKDT_DetailTextureTexCoordsSource,
	PKDT_MaskTextureTexCoordsSource,
		PKDT_IsBaseTextureTransformed,
		PKDT_IsEmissiveTextureTransformed,			//15
		PKDT_IsNormalTextureTransformed,
		PKDT_IsMaskTextureTransformed,
		PKDT_IsDetailTextureTransformed,
	PKDT_IsEnvironmentMappingEnabled,
		PKDT_MobileEnvironmentBlendMode,			//20
		PKDT_IsUsingThreeDetailTexture,
		PKDT_IsUsingTwoDetailTexture,
		PKDT_IsUsingOneDetailTexture,
	PKDT_TextureBlendFactorSource,
		PKDT_SpecularMask,							//25
	PKDT_AmbientOcclusionSource,
	PKDT_UseUniformColorMultiply,
	PKDT_UseVertexColorMultiply,
	PKDT_IsRimLightingEnabled,
		PKDT_RimLightingMaskSource,					//30
	PKDT_EnvironmentMaskSource,
	PKDT_IsEmissiveEnabled,
	PKDT_EmissiveColorSource,
	PKDT_EmissiveMaskSource,
		PKDT_AlphaValueSource,						//35
		// Set 0 is LOCKED!
		// DO NOT ADD MORE KEYS ABOVE HERE! Add them below.
		// Max Counter for the first 64-bits
		PKDT0_MAX,

		// Reserved for global shader types 
		PKDT_GlobalShaderType = PKDT0_MAX,			//36
		PKDT_GlobalShaderType2,
		// Reserved for depth shader types
		PKDT_DepthShaderType,
		PKDT_ForwardShadowProjectionShaderType,
		PKDT_IsDirectionalLightmap,					//40
		PKDT_IsLightingEnabled,
		PKDT_IsSpecularEnabled,
		PKDT_IsPixelSpecularEnabled,
		PKDT_IsNormalMappingEnabled,
		PKDT_IsHeightFogEnabled,					//45
		PKDT_TwoSided,								//Not used at run-time, but used for emulate mobile features
		PKDT_IsWaveVertexMovementEnabled,
		PKDT_IsDetailNormalEnabled,
		PKDT_IsMobileEnvironmentFresnelEnabled,
		PKDT_IsBumpOffsetEnabled,					//50
		PKDT_IsMobileColorGradingEnabled,
        PKDT_GFxBlendMode,
		PKDT_ColorMultiplySource,
		PKDT_UseFallbackStreamColor,
		PKDT_IsLandscape,							//55
		PKDT_UseLandscapeMonochromeLayerBlending,
		PKDT_IsGfxGammaCorrectionEnabled,

		// *** Add new keys here!
		// Please update /Development/Tools/ShaderKeyTool/MainWindow.xaml.cs if the above enumeration changes!  Also GetProgramKeyText() in UnContentCookers.cpp

		//Max Counter
		PKDT1_MAX,									//58
};

	FProgramKeyData()
{
		appMemzero(&FieldData0, sizeof(FProgramKeyFieldData));
		appMemzero(&FieldData1, sizeof(FProgramKeyFieldData));

		bStarted = 0;
		bStopped = 0;
	}

	/** Starts the sampling, tracks if all the parameters were set */
	void Start (void);

	/** Stops allowing input and verifies all input has been set */
	void Stop (void);

	/** Whether we've started filling in the data */
	UBOOL IsStarted (void) const
	{
		return bStarted;
	}

	/** Whether we've started filling in the data */
	UBOOL IsStopped (void) const
	{
		return bStopped;
	}

	/** Returns if the key data was properly filled in */
	UBOOL IsValid (void) const
	{
		return (bStarted && bStopped && 
			(FieldData0.NumberFieldsSet == PKDT0_MAX) && 
			(FieldData1.NumberFieldsSet == (PKDT1_MAX - PKDT0_MAX)));
	}

	/** Returns the 'packed' program key in a string */
	void GetPackedProgramKey( FProgramKey& OutProgramKey );

/**
	 * Unpacks the provided key into the provided ProgramKeyData structure
 *
	 * @param ProgramKey	The key to unpack
	 */
	void UnpackProgramKeyData(const FProgramKey& InPackedKey);

	/**
	 *	Clears all program key data to 0
 */
	void ClearProgramKeyData();

	/** Function used by both ES2RHI routines and in MaterialShared code for assigning ProgramKeyData values */
	void AssignProgramKeyValue(INT InKey, INT InValue);

	/** Function used by both ES2RHI routines and in MaterialShared code for overwriting values when a condition is met */
	void OverrideProgramKeyValue(INT InKey, INT InValue);

	/** Function used to reset a particular field so we can set it again */
	void ResetProgramKeyValue(INT InKey);

	/** Function used to reset a particular field so we can set it again */
	void LockProgramKeyValue(INT InKey, UBOOL bInLocked);

/**
	 *	Get the proper data sets for the given key
 *
	 *	@param	InKey						The key of interest
	 *	@param	OutKey						The key adjusted to its data set
	 *	@param	OutShaderProgramKeyFields	The ES2ShaderProgramKeyField set for this key
	 *	@param	OutFieldData				The FProgramKeyFieldData set for this key
	 */
	inline void GetFieldDataSet(INT InKey, INT& OutKey, ES2ShaderProgramKeyField*& OutShaderProgramKeyFields, FProgramKeyFieldData*& OutFieldData)
	{
		checkf((InKey < PKDT1_MAX), TEXT("Invalid InKey value!"));
		if (InKey < PKDT0_MAX)
		{
			OutKey = InKey;
			OutShaderProgramKeyFields = ES2ShaderProgramKeyFields0;
			OutFieldData = &FieldData0;
		}
		else if (InKey < PKDT1_MAX)
		{
			OutKey = InKey - PKDT0_MAX;
			OutShaderProgramKeyFields = ES2ShaderProgramKeyFields1;
			OutFieldData = &FieldData1;
		}
	}

	inline INT GetFieldValue(INT InKey)
	{
		INT AdjustedKey;
		ES2ShaderProgramKeyField* ShaderProgramKeyFields = NULL;
		FProgramKeyFieldData* FieldData = NULL;
		GetFieldDataSet(InKey, AdjustedKey, ShaderProgramKeyFields, FieldData);
		INT OutValue = FieldData->FieldValue[AdjustedKey];
		if (ShaderProgramKeyFields[AdjustedKey].NumBits > 8)
		{
            		OutValue |= ((INT)FieldData->FieldValue[AdjustedKey+1]) << 8;
		}
		return OutValue;
	}

	/**
	 *	Get the ShaderProgramKeyField array for the given key
	 *
	 *	@param	InKey						The key of interest
	 *	@param	OutKey						The key adjusted to its data set
	 *	@param	OutShaderProgramKeyFields	The ES2ShaderProgramKeyField set for this key
 */
	static void GetShaderProgramKeyField(INT InKey, INT& OutKey, ES2ShaderProgramKeyField*& OutShaderProgramKeyFields)
	{
		checkf((InKey < PKDT1_MAX), TEXT("Invalid InKey value!"));
		if (InKey < PKDT0_MAX)
		{
			OutKey = InKey;
			OutShaderProgramKeyFields = ES2ShaderProgramKeyFields0;
		}
		else if (InKey < PKDT1_MAX)
		{
			OutKey = InKey - PKDT0_MAX;
			OutShaderProgramKeyFields = ES2ShaderProgramKeyFields1;
		}
	}

	static INT GetFieldMaxValue(INT InKey)
	{
		INT AdjustedKey;
		ES2ShaderProgramKeyField* ShaderProgramKeyFields = NULL;
		GetShaderProgramKeyField(InKey, AdjustedKey, ShaderProgramKeyFields);
		return ShaderProgramKeyFields[AdjustedKey].MaxValue;
	}

    static QWORD GetFieldMask(INT InKey)
    {
        INT AdjustedKey;
        ES2ShaderProgramKeyField* ShaderProgramKeyFields = NULL;
        GetShaderProgramKeyField(InKey, AdjustedKey, ShaderProgramKeyFields);
        INT MaxIndex = InKey == AdjustedKey ? PKDT0_MAX : PKDT1_MAX - PKDT0_MAX;
        INT Shift = 0;
        for (INT Index = MaxIndex-1; Index > AdjustedKey; Index--)
        {
            Shift += ShaderProgramKeyFields[Index].NumBits;
        }
        QWORD Mask = ((1 << ShaderProgramKeyFields[AdjustedKey].NumBits) - 1) << Shift;
        return Mask;
    }
    static INT GetFieldShift(INT InKey)
    {
        INT AdjustedKey;
        ES2ShaderProgramKeyField* ShaderProgramKeyFields = NULL;
        GetShaderProgramKeyField(InKey, AdjustedKey, ShaderProgramKeyFields);
        INT MaxIndex = InKey == AdjustedKey ? PKDT0_MAX : PKDT1_MAX - PKDT0_MAX;
        INT Shift = 0;
        for (INT Index = MaxIndex-1; Index > AdjustedKey; Index--)
        {
            Shift += ShaderProgramKeyFields[Index].NumBits;
        }
        return Shift;
    }

	static ES2ShaderProgramKeyField ES2ShaderProgramKeyFields0[];
	static ES2ShaderProgramKeyField ES2ShaderProgramKeyFields1[];

	FProgramKeyFieldData FieldData0;
	FProgramKeyFieldData FieldData1;

private:
	UBOOL operator==(const FProgramKeyData& Src) const;

	BITFIELD bStarted : 1;
	BITFIELD bStopped : 1;
};

extern QWORD HexStringToQWord(const TCHAR *In);

/**
 * Calls the visual C preprocessor, if available
 *
 * @param SourceCode		Code to preprocess
 * @param DestFile			Fully qualified (!) path to where the preprocessed source should go
 * @return					string containing the preprocessed source
 */
FString RunCPreprocessor(const FString &SourceCode,const TCHAR *DestFile, UBOOL CleanWhiteSPace = TRUE);

/**
 * calls the glsl2agal converter on the specified file
 *
 * @param SourceFile      Fully qualified path to the file to be converted
 *  
 */
void RunShaderConverter(const TCHAR *SourceFile, UBOOL VertexShader, const TCHAR *DestFile);


/**
 * Returns the base filename of a ES2 shader
 *
 * @param PrimitiveType		Primitive type to get name for
 * @param GlobalShaderType	Global shader type (if PrimitiveType is EPT_GlobalShader)
 * @param Kind				SF_VERTEX or SF_PIXEL
 * @return					string containing the shader name, including path
 */
FString GetES2ShaderFilename( EMobilePrimitiveType PrimitiveType, EMobileGlobalShaderType GlobalShaderType, EShaderFrequency Kind );

/**
 * Generates the shader source for an ES2 shader based on the relevant keys
 *
 * @param Platform					Target platform
 * @param KeyData					The initialized ProgramKeyData structure
 * @param ProgramKey				The key for the current texture format/alphatest settings
 * @param PrimitiveType				Primitive type to build shaders for
 * @param bIsCompilingForPC			True if we're compiling this shader for use on PC ES2
 * @param CommonShaderPrefixFile	preloaded Shaders\\ES2\\Prefix_Common.glsl
 * @param VertexShaderPrefixFile	preloaded Shaders\\ES2\\VertexShader_Common.glsl
 * @param PixelShaderPrefixFile		preloaded Shaders\\ES2\\PixelShader_Common.glsl
 * @param MobileSettings			settings to base the shader on
 * @param PreloadedVertexBody		if the vertex shader body was already loaded, pass it in here, otherwise this function will loade from Engine\Shaders
 * @param PreloadedPixelBody		if the pixel shader body was already loaded, pass it in here, otherwise this function will loade from Engine\Shaders
 * @param OutVertexShader			returned vertex shader, modified only if return == TRUE
 * @param OutPixelShader			returned pixel shader, modified only if return == TRUE
 * @return							TRUE if valid shader source was returned
 */
UBOOL ES2ShaderSourceFromKeys( 
	UE3::EPlatformType			Platform,
	const FProgramKey&			ProgramKey, 
	FProgramKeyData&			KeyData, 
	EMobilePrimitiveType		PrimitiveType, 
	const UBOOL					bIsCompilingForPC,
	FString						CommonShaderPrefixFile,
	FString						VertexShaderPrefixFile,
	FString						PixelShaderPrefixFile,
	FLOAT						MobileSettingsMobileLODBias,
	INT							MobileSettingsMobileBoneCount,
	INT							MobileSettingsMobileBoneWeightCount,
	const TCHAR*				PreloadedVertexBody,
	const TCHAR*				PreloadedPixelBody,
	FString&					OutVertexShader,
	FString&					OutPixelShader
	);

/**
 * Returns whether the global shader exists (i.e, the EGST_ value is valid)
 *
 * @param GlobalShaderType	Global shader type
 */
UBOOL MobileGlobalShaderExists( EMobileGlobalShaderType GlobalShaderType );

#if WITH_MOBILE_RHI
/**
 * Allows the engine to tell the this RHI what global shader to use next
 * 
 * @param GlobalShaderType An enum value for the global shader type
 */
void MobileSetNextDrawGlobalShader( EMobileGlobalShaderType GlobalShaderType );
#endif

/**
 * Mobile Material Vertex Shader Params
 */
class FMobileMaterialVertexParams
{
public:

	/** True if lighting is enabled */
	UBOOL bUseLighting;

	UBOOL bBaseTextureTransformed;
	UBOOL bEmissiveTextureTransformed;
	UBOOL bNormalTextureTransformed;
	UBOOL bMaskTextureTransformed;
	UBOOL bDetailTextureTransformed;

	TMatrix<3,3> TextureTransform;

	/** Whether texture blending between base texture and detail textures is enabled */
	UBOOL bIsUsingOneDetailTexture;
	UBOOL bIsUsingTwoDetailTexture;
	UBOOL bIsUsingThreeDetailTexture;
	/** Whether to lock on or off texture blending regardless of platform settings */
	UBOOL bLockTextureBlend;

	/** When texture blending is enabled, what to use as the blend factor */
	EMobileTextureBlendFactorSource TextureBlendFactorSource;

	/** Whether emissive is enabled (and has an appropriate texture set) */
	UBOOL bUseEmissive;

	/** Emissive color source */
	EMobileEmissiveColorSource EmissiveColorSource;

	/** Emissive mask source */
	EMobileValueSource EmissiveMaskSource;

	/** Emissive constant color */
	FLinearColor EmissiveColor;

	/** Whether normal mapping should be used */
	UBOOL bUseNormalMapping;

	/** Whether environment mapping should be used */
	UBOOL bUseEnvironmentMapping;

	/** Source for environment map amount */
	EMobileValueSource EnvironmentMaskSource;

	/** Environment map strength */
	FLOAT EnvironmentAmount;

	/** Environment map fresnel amount (0.0 = disabled) */
	FLOAT EnvironmentFresnelAmount;

	/** Environment map fresnel exponent */
	FLOAT EnvironmentFresnelExponent;

	/** Whether we should enable specular features when rendering */
	UBOOL bUseSpecular;

	/** Whether we should enable per-pixel specular features when rendering */
	UBOOL bUsePixelSpecular;

	/** Whether to detail normal blending or not */
	UBOOL bUseDetailNormal;

	/** Material specular color */
	FLinearColor SpecularColor;

	/** Material specular power */
	FLOAT SpecularPower;

	/** Rim lighting strength */
	FLOAT RimLightingStrength;

	/** Rim lighting exponent */
	FLOAT RimLightingExponent;

	/** Rim lighting amount source */
	EMobileValueSource RimLightingMaskSource;

	/** Rim lighting color */
	FLinearColor RimLightingColor;

	/**Wave vertex movement*/
	UBOOL bWaveVertexMovementEnabled;
	FLOAT VertexMovementTangentFrequencyMultiplier;
	FLOAT VertexMovementVerticalFrequencyMultiplier;
	FLOAT MaxVertexMovementAmplitude;

	//Sway constants
	FLOAT SwayFrequencyMultiplier;
	FLOAT SwayMaxAngle;

	UBOOL bAllowFog;

	//Use Vertex Color
	UBOOL bVertexColor;


	/** Blending mode used by the material */
	EBlendMode MaterialBlendMode;

	/** Base texture UV coordinate source */
	EMobileTexCoordsSource BaseTextureTexCoordsSource;

	/** Detail texture UV coordinate source */
	EMobileTexCoordsSource DetailTextureTexCoordsSource;

	/** Mask texture UV coordinate source */
	EMobileTexCoordsSource MaskTextureTexCoordsSource;

	/** Ambient occlusion data source */
	EMobileAmbientOcclusionSource AmbientOcclusionSource;

	/** Whether to useuniform color scaling (mesh particles) or not */
	UBOOL bUseUniformColorMultiply;

	/** Default color to modulate each vertex by */
	FLinearColor UniformMultiplyColor;

	/** Whether to use per vertex color scaling */
	UBOOL bUseVertexColorMultiply;

	/**Whether to use monochrome layer blending for landscape */
	UBOOL bUseLandscapeMonochromeLayerBlending;

#if !FINAL_RELEASE || WITH_EDITOR
	//for tracking what material is requesting particular shaders
	FString MaterialName;
#endif
};


/**
 * Mobile Material Pixel Shader Params
 */
class FMobileMaterialPixelParams
{
public:
	/** Whether we should apply bump offset when rendering and the reference values to use */
	UBOOL bBumpOffsetEnabled;
	EMobileAlphaValueSource AlphaValueSource;
	FLOAT BumpReferencePlane;
	FLOAT BumpHeightRatio;

	/** Source to use for Color Multiplication */
	EMobileColorMultiplySource ColorMultiplySource;

	/** Source of specular mask for per-vertex specular on mobile */
	EMobileSpecularMask SpecularMask;

	/** Environment blend mode */
	EMobileEnvironmentBlendMode EnvironmentBlendMode;

	/** Environment map color scale */
	FLinearColor EnvironmentColorScale;

	/** Final opacity multiplier for modifying the final alpha value */
	FLOAT OpacityMultiplier;

	/** TRUE if using landscape monochrome layer blending */
	UBOOL bUseLandscapeMonochromeLayerBlending;

	/** The RBG colors to colorize each monochrome layer when using monochrome layer blending */
	FVector LandscapeMonochomeLayerColors[4];
};


/**
 * Mobile Mesh Vertex Shader Params
 */
class FMobileMeshVertexParams
{
public:

	/** Brightest light source direction */
	FVector BrightestLightDirection;

	/** Brightest light color */
	FLinearColor BrightestLightColor;

	/** Position of key objects in world space */
	FVector CameraPosition;
	FVector ObjectPosition;
	FBoxSphereBounds ObjectBounds;
	const FMatrix* LocalToWorld;

	/** The particle screen alignment */
	ESpriteScreenAlignment ParticleScreenAlignment;
};


/**
 * Mobile Mesh Pixel Shader Params
 */
class FMobileMeshPixelParams
{
public:

	/** True if sky lighting is enabled on this mesh */
	UBOOL bEnableSkyLight;
};
