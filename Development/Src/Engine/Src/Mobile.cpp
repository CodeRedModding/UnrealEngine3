/*=============================================================================
	MobileSupport.cpp: Mobile related enums and functions needed by the editor/cooker/mobile platforms

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "RHI.h"

#if !WITH_MOBILE_RHI
/**
 * Allows the engine to tell the this RHI what global shader to use next
 * Implemented on mobile platforms, and just using a stub here for other
 * platforms, so it doesn't have to be #ifdef:ed out.
 * 
 * @param GlobalShaderType An enum value for the global shader type
 */
void MobileSetNextDrawGlobalShader( EMobileGlobalShaderType GlobalShaderType )
{
}
#endif

/**
 * An array that holds the number of bits required to store each component in the program key
 */
ES2ShaderProgramKeyField FProgramKeyData::ES2ShaderProgramKeyFields0[] = 
{
	//Reserved for primitive types && platform features
	{2, EPF_MAX},					// [62-63]PKDT_PlatformFeatures
	{3, EPT_MAX},					// [59-61]PKDT_PrimitiveType

	//World/Misc
	{1, 2},							// [58-58]PKDT_IsDepthOnlyRendering
	{1, 2},							// [57-57]PKDT_IsGradientFogEnabled
	{2, SSA_Max},					// [55-56]PKDT_ParticleScreenAlignment
	{1, 2},							// [54-54]PKDT_UseGammaCorrection

	//Vertex Factory Flags
	{1, 2},							// [53-53]PKDT_IsLightmap
	{1, 2},							// [52-52]PKDT_IsSkinned
	{1, 2},							// [51-51]PKDT_IsDecal
	{1, 2},							// [50-50]PKDT_IsSubUV

	//Material Provided
	{3, BLEND_MAX},					// [47-49]PKDT_BlendMode
	{2, MTCS_MAX},					// [45-47]PKDT_BaseTextureTexCoordsSource
	{2, MTCS_MAX},					// [43-44]PKDT_DetailTextureTexCoordsSource
	{2, MTCS_MAX},					// [41-42]PKDT_MaskTextureTexCoordsSource

	{1, 2},							// [40-40]PKDT_IsBaseTextureTransformed

	{1, 2},							// [39-39]PKDT_IsEmissiveTextureTransformed
	{1, 2},							// [38-38]PKDT_IsNormalTextureTransformed
	{1, 2},							// [37-37]PKDT_IsMaskTextureTransformed
	{1, 2},							// [36-36]PKDT_IsDetailTextureTransformed

	{1, 2},							// [35-35]PKDT_IsEnvironmentMappingEnabled
	{1, 2},							// [34-34]PKDT_MobileEnvironmentBlendMode,
	{1, 2},							// [33-33]PKDT_IsUsingThreeDetailTexture,
	{1, 2},							// [32-32]PKDT_IsUsingTwoDetailTexture

	{1, 2},							// [31-31]PKDT_IsUsingOneDetailTexture
	{1, 2},							// [30-30]PKDT_TextureBlendFactorSource
	{4, MSM_MAX},					// [26-29]PKDT_SpecularMask
	{3, MAOS_MAX},					// [23-25]PKDT_AmbientOcclusionSource
	{1, 2},							// [22-22]PKDT_UseUniformColorMultiply
	{1, 2},							// [21-21]PKDT_UseVertexColorMultiply
	{1, 2},							// [20-20]PKDT_IsRimLightingEnabled

	{5, MVS_MAX},					// [15-19]PKDT_RimLightingMaskSource
	{5, MVS_MAX},					// [10-14]PKDT_EnvironmentMaskSource
	{1, 2},							// [09-09]PKDT_IsEmissiveEnabled
	{2, MECS_MAX},					// [07-08]PKDT_EmissiveColorSource
	{5, MVS_MAX},					// [02-06]PKDT_EmissiveMaskSource
	{2, MAVS_MAX}					// [00-01]PKDT_AlphaValueSource
};
// Assure that the array above is always up to date - if it fires, some entries may need to be added or removed
// Please update /Development/Tools/ShaderKeyTool/MainWindow.xaml.cs if the above enumeration changes!
checkAtCompileTime(ARRAY_COUNT(FProgramKeyData::ES2ShaderProgramKeyFields0) == FProgramKeyData::PKDT0_MAX, ES2ShaderProgramKeyFieldBitsSizeMismatch);

ES2ShaderProgramKeyField FProgramKeyData::ES2ShaderProgramKeyFields1[] = 
{
	{10, EGST_MAX},					// [28-37]PKDT_GlobalShaderType
	{0, 0},							// [28-28]PKDT_GlobalShaderType2
	{2, MobileDepthShader_MAX},		// [26-27]PKDT_DepthShaderType
	
	{1, 2},							// [25-25]PKDT_ForwardShadowProjectionShaderType
	{1, 2},							// [24-24]PKDT_IsDirectionalLightmap
	{1, 2},							// [23-23]PKDT_IsLightingEnabled
	{1, 2},							// [22-22]PKDT_IsSpecularEnabled
	
	{1, 2},							// [21-21]PKDT_IsPixelSpecularEnabled
	{1, 2},							// [20-20]PKDT_IsNormalMappingEnabled
	{1, 2},							// [19-19]PKDT_IsHeightFogEnabled
	{1, 2},							// [18-18]PKDT_TwoSided
	
	{1, 2},							// [17-17]PKDT_IsWaveVertexMovementEnabled
	{1, 2},							// [16-16]PKDT_IsDetailNormalEnabled
	{1, 2},							// [15-15]PKDT_IsMobileEnvironmentFresnelEnabled
	{1, 2},							// [14-14]PKDT_IsBumpOffsetEnabled
	{1, 2},							// [13-13]PKDT_IsMobileColorGradingEnabled
	{5, EGFXBM_MAX},				// [08-12]PKDT_GfxBlendMode
	{4, MCMS_MAX},					// [04-07]PKDT_ColorMultiplySource
	{1, 2},							// [03-03]PKDT_UseFallbackStreamColor
	{1, 2},							// [02-02]PKDT_IsLandscape
	{1, 2},							// [01-01]PKDT_UseLandscapeMonochromeLayerBlending
	{1, 1},							// [00-00]PKDT_GfxGamma
};
// Assure that the array above is always up to date - if it fires, some entries may need to be added or removed
// Please update /Development/Tools/ShaderKeyTool/MainWindow.xaml.cs if the above enumeration changes!  Also GetProgramKeyText() in UnContentCookers.cpp
checkAtCompileTime(ARRAY_COUNT(FProgramKeyData::ES2ShaderProgramKeyFields1) == (FProgramKeyData::PKDT1_MAX - FProgramKeyData::PKDT0_MAX), ES2ShaderProgramKeyFieldBitsSizeMismatch);

#if FLASH
// optimization for a super hot spot in the Flash byte code
union uu {
	quad_t	q;		
	quad_t	uq;		
	long	sl[2];	
	u_long	ul[2];	
};
#define	LONG_BITS (sizeof(long) * CHAR_BIT)
#define	QUAD_BITS	(sizeof(quad_t) * CHAR_BIT)
#define	H		_QUAD_HIGHWORD
#define	L		_QUAD_LOWWORD
#define	QUAD_BITS	(sizeof(quad_t) * CHAR_BIT)
#define	LONG_BITS	(sizeof(long) * CHAR_BIT)
#define	HALF_BITS	(sizeof(long) * CHAR_BIT / 2)
#define	HHALF(x)	((x) >> HALF_BITS)
#define	LHALF(x)	((x) & ((1 << HALF_BITS) - 1))
#define	LHUP(x)		((x) << HALF_BITS)

QWORD QWORDSHIFT(QWORD a, int shift)
{
	union uu aa;
    
	aa.q = a;
	if (shift >= LONG_BITS) {
		aa.ul[H] = shift >= QUAD_BITS ? 0 :
        aa.ul[L] << (shift - LONG_BITS);
		aa.ul[L] = 0;
	} else if (shift > 0) {
		aa.ul[H] = (aa.ul[H] << shift) |
        (aa.ul[L] >> (LONG_BITS - shift));
		aa.ul[L] <<= shift;
	}
	return (aa.q);
}
#endif

/**
 * Converts a string with hexadecimal digits into a 64-bit QWORD.
 *
 * @param In	String to convert
 * @return		64-bit QWORD
 */
QWORD HexStringToQWord(const TCHAR *In)
{
	QWORD Result = 0;
	while (1)
	{
		if ( *In>=TEXT('0') && *In<=TEXT('9') )
		{
			Result = (Result << 4) + *In - TEXT('0');
		}
		else if ( *In>=TEXT('A') && *In<=TEXT('F') )
		{
			Result = (Result << 4) + 10 + *In - TEXT('A');
		}
		else if ( *In>=TEXT('a') && *In<=TEXT('f') )
		{
			Result = (Result << 4) + 10 + *In - TEXT('a');
		}
		else if ( (*In==TEXT('x') || *In==TEXT('X')) && Result == 0)
		{
		}
		else
		{
			break;
		}
		In++;
	}
	return Result;
}

/**
 *	FProgramKeyData structure
 */
/** Starts the sampling, tracks if all the parameters were set */
void FProgramKeyData::Start(void)
{
	bStarted = TRUE;
}

/** Stops allowing input and verifies all input has been set */
void FProgramKeyData::Stop(void)
{
	BYTE PrimitiveTypeValue = GetFieldValue(PKDT_PrimitiveType);

	// Special key generation for "global shaders" (only GammaCorrection so far)
	if (PrimitiveTypeValue == EPT_GlobalShader)
	{
		// Clear out most of the fields, making a clean key.
		for (INT FieldIndex = 0; FieldIndex < PKDT1_MAX; ++ FieldIndex)
		{
			if ((FieldIndex != PKDT_GlobalShaderType) &&
				(FieldIndex != PKDT_GlobalShaderType2) &&
				(FieldIndex != PKDT_PrimitiveType) &&
#if WITH_GFx && NGP
                (FieldIndex != PKDT_GFxBlendMode) &&
#endif
				(FieldIndex != PKDT_BlendMode))
			{
				OverrideProgramKeyValue(FieldIndex, 0);
			}
		}
		// It wouldn't be in here if this wasn't already set...
//		AssignProgramKeyValue(PKDT_PrimitiveType, EPT_GlobalShader);
	}
	else
	{
		//STAGE 0 - conditionally remove settings that shouldn't be on because they are irrelevant
		//if not a standard mesh
		if (PrimitiveTypeValue != EPT_Default)
		{
			OverrideProgramKeyValue(PKDT_IsDepthOnlyRendering, 0);
			OverrideProgramKeyValue(PKDT_IsSkinned, 0);
			OverrideProgramKeyValue(PKDT_IsDecal, 0);
			OverrideProgramKeyValue(PKDT_IsLightingEnabled, 0);
			OverrideProgramKeyValue(PKDT_ForwardShadowProjectionShaderType, 0);
			OverrideProgramKeyValue(PKDT_IsLandscape, 0);
		}
		//NOT PARTICLE
		if (PrimitiveTypeValue != EPT_Particle)
		{
			OverrideProgramKeyValue(PKDT_ParticleScreenAlignment, 0);
			OverrideProgramKeyValue(PKDT_IsSubUV, 0);
		}
		//SIMPLE
		if ((PrimitiveTypeValue == EPT_Simple) || (PrimitiveTypeValue == EPT_DistanceFieldFont))
		{
			//@todo. This makes assumptions based on the order... should we make them explicit?
			// If we refactor and break them into smarter groupings, this wouldn't look so scary.
			for (INT KeyIndex = PKDT_IsDepthOnlyRendering; KeyIndex < PKDT1_MAX; ++KeyIndex)
			{
				if ((KeyIndex != PKDT_BlendMode) && 
					(KeyIndex != PKDT_IsBaseTextureTransformed) && 
					(KeyIndex != PKDT_PrimitiveType) &&
					(KeyIndex != PKDT_PlatformFeatures))
				{
					OverrideProgramKeyValue(KeyIndex, 0);
				}
			}

			OverrideProgramKeyValue(PKDT_ForwardShadowProjectionShaderType, 0);
		}

		//STAGE 1 - conditionally force enable/disable based on system settings
#if MINIMIZE_ES2_SHADERS
		BYTE PlatformFeaturesValue = GetFieldValue(PKDT_PlatformFeatures);
		if (PlatformFeaturesValue == EPF_LowEndFeatures)
		{
			//Never fog
			OverrideProgramKeyValue(PKDT_IsGradientFogEnabled, FALSE);
			OverrideProgramKeyValue(PKDT_IsHeightFogEnabled, FALSE);
			//Never bump offset
			OverrideProgramKeyValue(PKDT_IsBumpOffsetEnabled, FALSE);
			//Never environment map
			OverrideProgramKeyValue(PKDT_IsEnvironmentMappingEnabled, FALSE);
			//Never rim light
			OverrideProgramKeyValue(PKDT_IsRimLightingEnabled, FALSE);
			//No Specular
			OverrideProgramKeyValue(PKDT_IsSpecularEnabled, FALSE);
			OverrideProgramKeyValue(PKDT_IsPixelSpecularEnabled, FALSE);
			//No Detail Normal
			OverrideProgramKeyValue(PKDT_IsDetailNormalEnabled, FALSE);

			if( !GetFieldValue(PKDT_IsLandscape) )
			{
				//No color blending
				OverrideProgramKeyValue(PKDT_IsUsingOneDetailTexture, FALSE);
				OverrideProgramKeyValue(PKDT_IsUsingTwoDetailTexture, FALSE);
				OverrideProgramKeyValue(PKDT_IsUsingThreeDetailTexture, FALSE);
			}

			//No normal mapping
			OverrideProgramKeyValue(PKDT_IsNormalMappingEnabled, FALSE);
		}
		else
		{
			//No color grading for high end features as it happens in Post Process
			OverrideProgramKeyValue(PKDT_IsMobileColorGradingEnabled, FALSE);
		}
#endif

		//turn on depth only for depth shader types
		if (GetFieldValue(PKDT_DepthShaderType) != 0)
		{
			OverrideProgramKeyValue(PKDT_IsDepthOnlyRendering, TRUE);
		}

		
		//Depth rendering (mutually exclusive with emissive)
		if (GetFieldValue(PKDT_IsDepthOnlyRendering) != 0 || GetFieldValue(PKDT_ForwardShadowProjectionShaderType) != 0)
		{
			//ensure this is unlocked for shadowing
			LockProgramKeyValue(PKDT_IsUsingOneDetailTexture, FALSE);
			LockProgramKeyValue(PKDT_IsUsingTwoDetailTexture, FALSE);
			LockProgramKeyValue(PKDT_IsUsingThreeDetailTexture, FALSE);
			
			OverrideProgramKeyValue(PKDT_IsUsingOneDetailTexture, FALSE);
			OverrideProgramKeyValue(PKDT_IsUsingTwoDetailTexture, FALSE);
			OverrideProgramKeyValue(PKDT_IsUsingThreeDetailTexture, FALSE);
			
			OverrideProgramKeyValue(PKDT_IsDecal, FALSE);

			OverrideProgramKeyValue(PKDT_IsGradientFogEnabled, FALSE);
			OverrideProgramKeyValue(PKDT_IsHeightFogEnabled, FALSE);
			OverrideProgramKeyValue(PKDT_IsLightmap, FALSE);
			OverrideProgramKeyValue(PKDT_IsDirectionalLightmap, FALSE);
			OverrideProgramKeyValue(PKDT_IsLightingEnabled, FALSE);
			OverrideProgramKeyValue(PKDT_BaseTextureTexCoordsSource, FALSE);
			OverrideProgramKeyValue(PKDT_DetailTextureTexCoordsSource, FALSE);
			OverrideProgramKeyValue(PKDT_MaskTextureTexCoordsSource, FALSE);
			OverrideProgramKeyValue(PKDT_IsMobileColorGradingEnabled, FALSE);

			OverrideProgramKeyValue(PKDT_IsDetailTextureTransformed, FALSE);
			OverrideProgramKeyValue(PKDT_IsBaseTextureTransformed, FALSE);
			OverrideProgramKeyValue(PKDT_IsEmissiveTextureTransformed, FALSE);
			OverrideProgramKeyValue(PKDT_IsNormalTextureTransformed, FALSE);
			OverrideProgramKeyValue(PKDT_IsMaskTextureTransformed, FALSE);

			//all can use the same blend mode since it doesn't matter for the shader
			OverrideProgramKeyValue(PKDT_BlendMode, FALSE);
			//disable alpha source since it doesn't matter
			OverrideProgramKeyValue(PKDT_AlphaValueSource, FALSE);

			//no need during shadows
			OverrideProgramKeyValue(PKDT_IsSubUV, FALSE);
			OverrideProgramKeyValue(PKDT_TextureBlendFactorSource, FALSE);			
			OverrideProgramKeyValue(PKDT_UseVertexColorMultiply, FALSE);
			OverrideProgramKeyValue(PKDT_ColorMultiplySource, FALSE);
			OverrideProgramKeyValue(PKDT_UseFallbackStreamColor, FALSE);
			OverrideProgramKeyValue(PKDT_UseLandscapeMonochromeLayerBlending, FALSE);

			OverrideProgramKeyValue(PKDT_UseUniformColorMultiply, FALSE);
			OverrideProgramKeyValue(PKDT_AmbientOcclusionSource, FALSE);


			OverrideProgramKeyValue(PKDT_IsSpecularEnabled, FALSE);
			OverrideProgramKeyValue(PKDT_IsDetailNormalEnabled, FALSE);
			OverrideProgramKeyValue(PKDT_IsNormalMappingEnabled, FALSE);
			OverrideProgramKeyValue(PKDT_IsEnvironmentMappingEnabled, FALSE);
			OverrideProgramKeyValue(PKDT_MobileEnvironmentBlendMode, FALSE);
			OverrideProgramKeyValue(PKDT_IsBumpOffsetEnabled, FALSE);
			OverrideProgramKeyValue(PKDT_IsRimLightingEnabled, FALSE);
			OverrideProgramKeyValue(PKDT_IsEmissiveEnabled, FALSE);
			OverrideProgramKeyValue(PKDT_EmissiveColorSource, FALSE);
			OverrideProgramKeyValue(PKDT_TwoSided, FALSE);

			// turn off channel overrides
			OverrideProgramKeyValue(PKDT_EmissiveMaskSource, 0);
			OverrideProgramKeyValue(PKDT_EnvironmentMaskSource, 0);
		}
		// Emissive
		else if (GetFieldValue(PKDT_IsEmissiveEnabled) == 0)
		{
			OverrideProgramKeyValue(PKDT_EmissiveColorSource, 0);
			OverrideProgramKeyValue(PKDT_EmissiveMaskSource, 0);
			OverrideProgramKeyValue(PKDT_IsEmissiveTextureTransformed, FALSE);
		}

#if 0
		const UBOOL bUseLighting = (GetFieldValue(PKDT_IsLightingEnabled) != 0);
		const UBOOL bUseLightmap = bUseLighting && (GetFieldValue(PKDT_IsLightmap) != 0);//( BaseFeatures & EShaderBaseFeatures::Lightmap ) != 0;
		const UBOOL bUseDirectionalLightmap = bUseLightmap && (GetFieldValue(PKDT_IsDirectionalLightmap) != 0);
		const UBOOL bUseVertexLightmap = FALSE;//bUseLighting && ( BaseFeatures & EShaderBaseFeatures::VertexLightmap ) != 0;
		const UBOOL bUseDynamicDirectionalLight = bUseLighting && !bUseLightmap && !bUseVertexLightmap;
		if (bUseDynamicDirectionalLight)
		{
			OverrideProgramKeyValue(PKDT_IsNormalMappingEnabled, FALSE);
		}
#endif
		//ENVIRONMENT MAPPING
		if (GetFieldValue(PKDT_IsNormalMappingEnabled) == 0)
		{
			OverrideProgramKeyValue(PKDT_IsNormalTextureTransformed, FALSE);
		}

		//ENVIRONMENT MAPPING
		if (GetFieldValue(PKDT_IsEnvironmentMappingEnabled) == 0)
		{
			OverrideProgramKeyValue(PKDT_MobileEnvironmentBlendMode, FALSE);
			OverrideProgramKeyValue(PKDT_IsMobileEnvironmentFresnelEnabled, FALSE);
			OverrideProgramKeyValue(PKDT_EnvironmentMaskSource, FALSE);
		}

		//LIGHTING
		if (GetFieldValue(PKDT_IsLightingEnabled) == 0)
		{
			OverrideProgramKeyValue(PKDT_IsLightmap, 0);
		}
		if (GetFieldValue(PKDT_IsLightmap) == 0)
		{
			OverrideProgramKeyValue(PKDT_IsDirectionalLightmap, 0);
		}
		//SPECULAR
		if (GetFieldValue(PKDT_IsSpecularEnabled) == 0)
		{
			OverrideProgramKeyValue(PKDT_IsPixelSpecularEnabled, FALSE);
			OverrideProgramKeyValue(PKDT_SpecularMask, FALSE);
		}
		//SPECULAR
		if (GetFieldValue(PKDT_IsRimLightingEnabled) == 0)
		{
			OverrideProgramKeyValue(PKDT_RimLightingMaskSource, FALSE);
		}
		//Color texture blend
		if (GetFieldValue(PKDT_IsUsingOneDetailTexture) == 0)
		{
			OverrideProgramKeyValue(PKDT_IsDetailTextureTransformed, 0);
			OverrideProgramKeyValue(PKDT_TextureBlendFactorSource, 0);
			OverrideProgramKeyValue(PKDT_DetailTextureTexCoordsSource, 0);
		}

		//if not using color multiply, completely turn this off
		if ((GetFieldValue(PKDT_UseVertexColorMultiply) == 0) && (GetFieldValue(PKDT_UseUniformColorMultiply) == 0))
		{
			OverrideProgramKeyValue(PKDT_ColorMultiplySource, FALSE);
		}

		if (GetFieldValue(PKDT_IsDepthOnlyRendering) == 0)
		{
			OverrideProgramKeyValue(PKDT_DepthShaderType, MobileDepthShader_None);
		}

		// Landscape settings
		if ((GetFieldValue(PKDT_IsLandscape) == 0))
		{
			// Disable when not using Landscape
			OverrideProgramKeyValue(PKDT_UseLandscapeMonochromeLayerBlending, FALSE);
		}
		else
		{
			// Landscape ignores this value.			
			OverrideProgramKeyValue(PKDT_MaskTextureTexCoordsSource, 0);
		}

	}

	//now fully stop
	bStopped = TRUE;
	checkf(
		(FieldData0.NumberFieldsSet == PKDT0_MAX) &&
		(FieldData1.NumberFieldsSet == (PKDT1_MAX - PKDT0_MAX))
		);
}

/** Returns the 'packed' program key in a string */
void FProgramKeyData::GetPackedProgramKey( FProgramKey& OutProgramKey )
{
	//ensure that all values have been set before we try and use program key data
	checkf(IsValid());

	OutProgramKey.Reset();
	INT MaxIndexValue;
	for (INT DataSetIdx = 0; DataSetIdx < 2; DataSetIdx++)
	{
		INT FakeKey;
		// Get the max index value for the current field set
		if (DataSetIdx == 0)
		{
			MaxIndexValue = PKDT0_MAX;
			// Get a key value that we know is in the correct range
			FakeKey = PKDT0_MAX - 1;
		}
		else
		{
			MaxIndexValue = PKDT1_MAX - PKDT0_MAX;
			// Get a key value that we know is in the correct range
			FakeKey = PKDT1_MAX - 1;
		}
		
		INT AdjustedKey;
		ES2ShaderProgramKeyField* ShaderProgramKeyFields = NULL;
		FProgramKeyFieldData* FieldData = NULL;
		GetFieldDataSet(FakeKey, AdjustedKey, ShaderProgramKeyFields, FieldData);

		QWORD CurrentKey = 0;
		QWORD RunningMax = 0;

		for (INT Index = 0; Index < MaxIndexValue; Index++)
		{
			const BYTE NextFieldValue = FieldData->FieldValue[Index];
			const BYTE NextFieldBits = ShaderProgramKeyFields[Index].NumBits;

#if _DEBUG
			// Safety check to make sure we're not running out of room for the new key
			QWORD MaxQWORD = ~0;
			if( (MaxQWORD >> NextFieldBits) < RunningMax )
			{
				warnf(NAME_Warning, TEXT("ES2 Program Key has exceeded key capacity"));
			}
#endif

#if FLASH
			CurrentKey = QWORDSHIFT(CurrentKey, NextFieldBits);
#else
			CurrentKey <<= NextFieldBits;
#endif
			CurrentKey += NextFieldValue & ((1 << ShaderProgramKeyFields[Index].NumBits) - 1);
			if (NextFieldBits > 8)
			{
				CurrentKey += ((INT)FieldData->FieldValue[Index+1]) << 8;
			}

#if _DEBUG
			RunningMax <<= NextFieldBits;
			RunningMax += ((1 << NextFieldBits) - 1);
#endif
		}

		OutProgramKey.Data[DataSetIdx] = CurrentKey;
	}
}

/**
 * Unpacks the provided key into the provided ProgramKeyData structure
 *
 * @param ProgramKey	The key to unpack
 */
void FProgramKeyData::UnpackProgramKeyData(const FProgramKey& InPackedKey)
{
	QWORD KeyValue0 = InPackedKey.Data[0];
	QWORD KeyValue1 = InPackedKey.Data[1];

	for (INT Value0Idx = PKDT0_MAX - 1; Value0Idx >= 0; Value0Idx--)
	{
		const BYTE NextFieldBits = ES2ShaderProgramKeyFields0[Value0Idx].NumBits;
		FieldData0.FieldValue[Value0Idx] = KeyValue0 & ((1 << NextFieldBits) - 1);
		KeyValue0 >>= NextFieldBits;
	}

	for (INT Value1Idx = PKDT1_MAX - PKDT0_MAX - 1; Value1Idx >= 0; Value1Idx--)
	{
		const BYTE NextFieldBits = ES2ShaderProgramKeyFields1[Value1Idx].NumBits;
	if (NextFieldBits > 8)
	{
    		FieldData1.FieldValue[Value1Idx]   = KeyValue1 & ((1 << 8) - 1);
    		FieldData1.FieldValue[Value1Idx+1] = (KeyValue1 >> 8) & ((1 << (NextFieldBits-8)) - 1);
	}
	else
	{
    		FieldData1.FieldValue[Value1Idx] = KeyValue1 & ((1 << NextFieldBits) - 1);
	}
		KeyValue1 >>= NextFieldBits;
	}
}

/**
 *	Clears all program key data to 0
 */
void FProgramKeyData::ClearProgramKeyData()
{
	check(bStarted && !bStopped);
	INT FieldIdx0;
	for (FieldIdx0 = 0; FieldIdx0 < FProgramKeyData::PKDT0_MAX; ++FieldIdx0)
	{
		FieldData0.bFieldSet[FieldIdx0] = TRUE;
		FieldData0.NumberFieldsSet++;
		FieldData0.FieldValue[FieldIdx0] = 0;
	}

	for (INT FieldIdx1 = 0; FieldIdx1 < (FProgramKeyData::PKDT1_MAX - FProgramKeyData::PKDT0_MAX); ++FieldIdx1)
	{
		FieldData1.bFieldSet[FieldIdx1] = TRUE;
		FieldData1.NumberFieldsSet++;
		FieldData1.FieldValue[FieldIdx1] = 0;
	}
}

/** Function used by both ES2RHI routines and in MaterialShared code for assigning ProgramKeyData values */
void FProgramKeyData::AssignProgramKeyValue(INT InKey, INT InValue)
{
	INT AdjustedKey;
	ES2ShaderProgramKeyField* ShaderProgramKeyFields = NULL;
	FProgramKeyFieldData* FieldData = NULL;
	GetFieldDataSet(InKey, AdjustedKey, ShaderProgramKeyFields, FieldData);

	checkfSlow(((INT)(1 << (ShaderProgramKeyFields[AdjustedKey].NumBits+1)) >= (ShaderProgramKeyFields[AdjustedKey].MaxValue)), 
		TEXT("Program key value has a max value that is too large for the number of bits!"));
	checkfSlow((ShaderProgramKeyFields[AdjustedKey].MaxValue > InValue), TEXT("Program key value is greater than maximum value!"));
	checkfSlow(IsStarted() && !IsStopped(), TEXT("Key Value protection was not used properly.  Please call Start on the ProgramKeyData"));
	checkfSlow(FieldData->bFieldSet[AdjustedKey] == FALSE, TEXT("Key is being clobbered.  Use RESET_PROGRAM_KEY_VALUE to do this on purpose, as it corrects number of sets"));
	FieldData->bFieldSet[AdjustedKey] = TRUE;
	FieldData->NumberFieldsSet++;
	FieldData->FieldValue[AdjustedKey] = InValue & 0xff;
	if (ShaderProgramKeyFields[AdjustedKey].NumBits > 8)
	{
		FieldData->bFieldSet[AdjustedKey+1] = TRUE;
		FieldData->NumberFieldsSet++;
		FieldData->FieldValue[AdjustedKey+1] = InValue >> 8;
	}
}

/** Function used by both ES2RHI routines and in MaterialShared code for overwriting values when a condition is met */
void FProgramKeyData::OverrideProgramKeyValue(INT InKey, INT InValue)
{
	INT AdjustedKey;
	ES2ShaderProgramKeyField* ShaderProgramKeyFields = NULL;
	FProgramKeyFieldData* FieldData = NULL;
	GetFieldDataSet(InKey, AdjustedKey, ShaderProgramKeyFields, FieldData);

	if (ShaderProgramKeyFields[AdjustedKey].NumBits == 0)
	{
		return;
	}

	checkfSlow(((INT)(1 << (ShaderProgramKeyFields[AdjustedKey].NumBits+1)) >= (ShaderProgramKeyFields[AdjustedKey].MaxValue)), 
		TEXT("Program key value has a max value that is too large for the number of bits!"));
	checkfSlow((ShaderProgramKeyFields[AdjustedKey].MaxValue > InValue), TEXT("Program key value is greater than maximum value!"));
	checkfSlow(IsStarted() && !IsStopped(), TEXT("Key Value protection was not used properly.  Please call Start on the ProgramKeyData"));
	checkfSlow(FieldData->bFieldSet[AdjustedKey] == TRUE, 
		TEXT("Key has not been set yet.  Override is incorrect at this point as the ref count hasn't been incremented"));
	if (FieldData->bFieldLocked[AdjustedKey] == FALSE)
	{
		FieldData->FieldValue[AdjustedKey] = InValue;
	    if (ShaderProgramKeyFields[AdjustedKey].NumBits > 8)
    	{
	    	FieldData->bFieldSet[AdjustedKey+1] = TRUE;
		    FieldData->FieldValue[AdjustedKey+1] = InValue >> 8;
	    }
	}
}

/** Function used to reset a particular field so we can set it again */
void FProgramKeyData::ResetProgramKeyValue(INT InKey)
{
	INT AdjustedKey;
	ES2ShaderProgramKeyField* ShaderProgramKeyFields = NULL;
	FProgramKeyFieldData* FieldData = NULL;
	GetFieldDataSet(InKey, AdjustedKey, ShaderProgramKeyFields, FieldData);

	checkfSlow(((INT)(1 << (ShaderProgramKeyFields[AdjustedKey].NumBits+1)) >= (ShaderProgramKeyFields[AdjustedKey].MaxValue)), 
		TEXT("Program key value has a max value that is too large for the number of bits!"));
	checkfSlow(IsStarted() && !IsStopped(), TEXT("Key Value protection was not used properly.  Please call Start on the ProgramKeyData"));
	checkfSlow(FieldData->bFieldSet[AdjustedKey] == TRUE, TEXT("No need to reset key, as it has never been set"));
	if (FieldData->bFieldLocked[AdjustedKey] == FALSE)
	{
		FieldData->NumberFieldsSet--;
		FieldData->bFieldSet[AdjustedKey] = FALSE;
		FieldData->FieldValue[AdjustedKey] = 0;

	    if (ShaderProgramKeyFields[AdjustedKey].NumBits > 8)
	    {
		    FieldData->bFieldSet[AdjustedKey+1] = FALSE;
		    FieldData->FieldValue[AdjustedKey+1] = 0;
	    }
	}
}

/** Function used to reset a particular field so we can set it again */
void FProgramKeyData::LockProgramKeyValue(INT InKey, UBOOL bInLocked)
{
	INT AdjustedKey;
	ES2ShaderProgramKeyField* ShaderProgramKeyFields = NULL;
	FProgramKeyFieldData* FieldData = NULL;
	GetFieldDataSet(InKey, AdjustedKey, ShaderProgramKeyFields, FieldData);

	checkfSlow(IsStarted() && !IsStopped(), 
		TEXT("Key Value protection was not used properly.  Please call Start on the ProgramKeyData"));
	checkfSlow(FieldData->bFieldSet[InKey] == TRUE, TEXT("No need to reset key, as it has never been set"));
	FieldData->bFieldLocked[InKey] = bInLocked;
}

/**
 * Calls the visual C preprocessor, if available
 *
 * @param SourceCode		Code to preprocess
 * @param DestFile			Fully qualified (!) path to where the preprocessed source should go
 * @return					string containing the preprocessed source
 */
FString RunCPreprocessor(const FString &SourceCode,const TCHAR *DestFile, UBOOL CleanWhiteSPace)
{
	#define EXTENSION_ID TEXT("EXT_______")

	FFilename PreprocessedFileName = FString(DestFile) + TEXT(".parsed");
	FFilename UnPreprocessedFileName = FString(DestFile) + TEXT(".unparsed");

	// we want to make sure this is not stale
	GFileManager->Delete( *PreprocessedFileName );

	// replace the #extension directive with a stand-in string because the pre-processor can not deal with undefined directives.  After pre-processing
	// is complete the stand-in string will be replaced with #extension again.

	FString SourceCodePlusNewline = SourceCode.Replace(TEXT("#extension"), EXTENSION_ID);
	SourceCodePlusNewline += TEXT("\r\n\r\n");

	appSaveStringToFile( SourceCodePlusNewline, *UnPreprocessedFileName );
	const FFilename ExecutableFileName(FString( appBaseDir() ) + FString(TEXT( "..\\UnrealCommand.exe" )));
	const FString CmdLineParams = FString::Printf( TEXT( "PreprocessShader %sGame Shipping %s %s -CleanWhitespace" ),GGameName,*UnPreprocessedFileName, *PreprocessedFileName);

	const UBOOL bLaunchDetached = TRUE;
	const UBOOL bLaunchHidden = TRUE;
	void* ProcHandle = appCreateProc( *ExecutableFileName, *CmdLineParams, bLaunchDetached, bLaunchHidden );
	if( ProcHandle != NULL )
	{
		UBOOL bProfilingComplete = FALSE;
		INT ReturnCode = 1;
		while( !bProfilingComplete )
		{
			bProfilingComplete = appGetProcReturnCode( ProcHandle, &ReturnCode );
			if( !bProfilingComplete )
			{
				appSleep( 0.01f );
			}
		}
	}

	FString FinalCode;

	const UBOOL bLoadedSuccessfully = appLoadFileToString( FinalCode, *PreprocessedFileName );
	if( !bLoadedSuccessfully )
	{
		FinalCode.Empty();  // make double sure we return an empty string as failure
	}
	else
	{
		FinalCode = FinalCode.Replace(EXTENSION_ID, TEXT("#extension"));

		FString PreviousCode;
		//Load the version that was already there to compare the contents (so we don't update the timestamp when nothing has changed)
		const UBOOL bLoadedPreviousVersionSuccessfully = appLoadFileToString( PreviousCode, DestFile );
		if (PreviousCode != FinalCode)
		{
			appSaveStringToFile(FinalCode, DestFile);
		}
	}

	GFileManager->Delete( *UnPreprocessedFileName ); 
	GFileManager->Delete( *PreprocessedFileName ); 

	return FinalCode;

	#undef EXTENSION_ID
}

void RunShaderConverter(const TCHAR *SourceFile, UBOOL VertexShader, const TCHAR *DestFile)
{
	FFilename ExecutableFileName(FString( appBaseDir() ) + FString(TEXT( "..\\..\\Development\\Tools\\Flash\\ShaderConverter\\glsl2agalJSON.exe" )));
	FString ShaderCommand = VertexShader? TEXT("-v") : TEXT("-f");
	FString FileEnd = VertexShader?TEXT("VertJSON.msf"):TEXT("FragJSON.msf");
	FString CmdLineParams = FString::Printf( TEXT( "%s %s %s%s" ),*ShaderCommand,SourceFile, DestFile, *FileEnd);
	warnf(NAME_Log, *(FString::Printf( TEXT( "compiling GLSLShader %s into AGAL %s" ),SourceFile, DestFile)));

	const UBOOL bLaunchDetached = TRUE;
	const UBOOL bLaunchHidden = FALSE;
	void* ProcHandle = appCreateProc( *ExecutableFileName, *CmdLineParams, bLaunchDetached, bLaunchHidden );
	if( ProcHandle != NULL )
	{
		UBOOL bProfilingComplete = FALSE;
		INT ReturnCode = 1;
		while( !bProfilingComplete )
		{
			bProfilingComplete = appGetProcReturnCode( ProcHandle, &ReturnCode );
			if( !bProfilingComplete )
			{
				appSleep( 0.01f );
			}
		}

		if(ReturnCode == 1)
		{
			warnf(NAME_Log, TEXT( "There were errors in the source glsl check the output file for details" ));
		}
		else if(ReturnCode != 0)
		{
			warnf(NAME_Log, TEXT( "Could not initialize glsl2agal converter" ));
		}
	}

	ExecutableFileName = FString( appBaseDir() ) + FString(TEXT( "..\\..\\Development\\Tools\\Flash\\ShaderConverter\\glsl2agal.exe" ));
	ShaderCommand = VertexShader? TEXT("-v") : TEXT("-f");
	FileEnd = VertexShader?TEXT("Vert.msf"):TEXT("Frag.msf");
	CmdLineParams = FString::Printf( TEXT( "%s %s %s%s" ),*ShaderCommand,SourceFile, DestFile, *FileEnd);

	ProcHandle = appCreateProc( *ExecutableFileName, *CmdLineParams, bLaunchDetached, bLaunchHidden );
	if( ProcHandle != NULL )
	{
		UBOOL bProfilingComplete = FALSE;
		INT ReturnCode = 1;
		while( !bProfilingComplete )
		{
			bProfilingComplete = appGetProcReturnCode( ProcHandle, &ReturnCode );
			if( !bProfilingComplete )
			{
				appSleep( 0.01f );
			}
		}
		if(ReturnCode == 1)
		{
			warnf(NAME_Log, TEXT( "There were errors in the source glsl check the JSON output file for details" ));
		}
		else if(ReturnCode != 0)
		{
			warnf(NAME_Log, TEXT( "Could not initialize glsl2agal converter" ));
		}
	}
}

UBOOL MobileGlobalShaderExists(EMobileGlobalShaderType GlobalShaderType)
{
	if (GlobalShaderType == EGST_None)
		return FALSE;
#if WITH_GFx
	if (GlobalShaderType >= EGST_GFxBegin && GlobalShaderType <= EGST_GFxEnd)
	{
		using namespace Scaleform::Render::RHI;
		INT GFxShaderType = (GlobalShaderType-EGST_GFxBegin) >> 1;
		INT GFxVertexOption = ((GlobalShaderType-EGST_GFxBegin) & 1) ? VertexShaderDesc::VS_base_Position3d : 0;
		INT GFxVertexShader = FragShaderDesc::VShaderForFShader[GFxShaderType]+GFxVertexOption;

		// Video not supported on mobile
		if ( GFxShaderType >= FragShaderDesc::FS_FYUVEAlpha && GFxShaderType < FragShaderDesc::FS_FText )
		{
			return FALSE;
		}
		return FragShaderDesc::Descs[GFxShaderType] != NULL
			&& GFxVertexShader < VertexShaderDesc::VS_Count && VertexShaderDesc::Descs[GFxVertexShader] != NULL;
	}
#endif
	return TRUE;
}

/**
 * Returns the base filename of a ES2 shader
 *
 * @param PrimitiveType		Primitive type to get name for
 * @param GlobalShaderType	Global shader type (if PrimitiveType is EPT_GlobalShader)
 * @param Kind				SF_VERTEX or SF_PIXEL
 * @return					string containing the shader name, including path
 */
FString GetES2ShaderFilename( EMobilePrimitiveType PrimitiveType, EMobileGlobalShaderType GlobalShaderType, EShaderFrequency Kind )
{
	FString Filename;
	switch (PrimitiveType)
	{
		case EPT_Default:
			Filename += TEXT("Default");
			break;
		case EPT_Particle:
			Filename += TEXT("ParticleSprite");
			break;
		case EPT_BeamTrailParticle:
			if (Kind == SF_Pixel)
			{
				Filename += TEXT("ParticleSprite");
			}
			else
			{
				Filename += TEXT("BeamTrail");
			}
			break;
		case EPT_LensFlare:
			if (Kind == SF_Pixel)
			{
				Filename += TEXT("ParticleSprite");
			}
			else
			{
				Filename += TEXT("LensFlare");
			}
			break;
		case EPT_Simple:
			Filename += TEXT("Simple");
			break;
		case EPT_DistanceFieldFont:
			if (Kind == SF_Pixel)
			{
				Filename += TEXT("DistanceFieldFont");
			}
			else
			{
				Filename += TEXT("Simple");
			}
			break;
		case EPT_GlobalShader:
			switch ( GlobalShaderType )
			{
				case EGST_GammaCorrection:
					Filename += TEXT("GammaCorrection");
					break;
				case EGST_Filter1:
					Filename += TEXT("Filter1");
					break;
				case EGST_Filter4:
					Filename += TEXT("Filter4");
					break;
				case EGST_Filter16:
					Filename += TEXT("Filter16");
					break;
				case EGST_LUTBlender:
					Filename += TEXT("LUTBlender");
					break;
				case EGST_UberPostProcess:
					Filename += TEXT("UberPostProcess");
					break;
				case EGST_LightShaftDownSample:
					Filename += TEXT("LightShaftDownSample");
					break;
				case EGST_LightShaftDownSample_NoDepth:
					if (Kind == SF_Pixel)
					{
						Filename += TEXT("LightShaftDownSample_NoDepth_");
					}
					else
					{
						Filename += TEXT("LightShaftDownSample");
					}
					break;
				case EGST_LightShaftBlur:
					Filename += TEXT("LightShaftBlur");
					break;
				case EGST_LightShaftApply:
					Filename += TEXT("LightShaftApply");
					break;
				case EGST_SimpleF32:
					Filename += TEXT("SimpleF32");
					break;
				case EGST_PositionOnly:
					Filename += TEXT("PositionOnly");
					break;
				case EGST_ShadowProjection:
					Filename += TEXT("ShadowProjection");
					break;
				case EGST_BloomGather:
				case EGST_DOFAndBloomGather:
					Filename += TEXT("DOFGather");
					break;
				case EGST_MobileUberPostProcess1:
				case EGST_MobileUberPostProcess3:
				case EGST_MobileUberPostProcess4:
				case EGST_MobileUberPostProcess5:
				case EGST_MobileUberPostProcess7:
					Filename += TEXT("MobileUberPostProcess");
					break;
				case EGST_VisualizeTexture:
					Filename += TEXT("VisualizeTexture");
					break;
				case EGST_RadialBlur:
					Filename += TEXT("RadialBlur");
					break;
				case EGST_FXAA:
					Filename += TEXT("MobileFXAA");
					break;

                default:
#if WITH_GFx
					if (GlobalShaderType >= EGST_GFxBegin && GlobalShaderType <= EGST_GFxEnd)
					{
						using namespace Scaleform::Render::RHI;

						Filename += TEXT("GFx_");
						INT GFxShaderType = (GlobalShaderType-EGST_GFxBegin) >> 1;
						INT GFxVertexOption = ((GlobalShaderType-EGST_GFxBegin) & 1) ? VertexShaderDesc::VS_base_Position3d : 0;

						if (Kind == SF_Pixel)
						{
							Filename += FANSIToTCHAR(FragShaderDesc::Descs[GFxShaderType]->name);
						}
						else
						{
							Filename += FANSIToTCHAR(VertexShaderDesc::Descs[FragShaderDesc::VShaderForFShader[GFxShaderType]+GFxVertexOption]->name);
						}
						break;
					}
#endif
                    checkf(0, TEXT("Unknown global shader type: %d"), GlobalShaderType);
            }
            break;
		default:
			appErrorf( TEXT("Unknown mobile primitive type: %d"), (INT)PrimitiveType );
	}
	if (Kind == SF_Pixel)
	{
		Filename += TEXT("PixelShader");
	}
	else
	{
		Filename += TEXT("VertexShader");
	}
	return Filename;
}

/**
 * Generates a define block for texture sources with a prefix
 *
 * @param DefinesString			Return value, adds to the current string
 * @param Prefix				Prefix to add to each define
 * @param InValue				Value for define with only prefix
 */
static void CreateValueSourceDefines( FString& DefinesString, const FString& InPrefix, const INT InValue )
{
	DefinesString += FString::Printf(TEXT("#define %s_CONSTANT %d\r\n"), *InPrefix, (INT)MVS_Constant );
	DefinesString += FString::Printf(TEXT("#define %s_VERTEX_COLOR_RED %d\r\n"), *InPrefix, (INT)MVS_VertexColorRed );
	DefinesString += FString::Printf(TEXT("#define %s_VERTEX_COLOR_GREEN %d\r\n"), *InPrefix, (INT)MVS_VertexColorGreen );
	DefinesString += FString::Printf(TEXT("#define %s_VERTEX_COLOR_BLUE %d\r\n"), *InPrefix, (INT)MVS_VertexColorBlue );
	DefinesString += FString::Printf(TEXT("#define %s_VERTEX_COLOR_ALPHA %d\r\n"), *InPrefix, (INT)MVS_VertexColorAlpha );
	DefinesString += FString::Printf(TEXT("#define %s_BASE_TEXTURE_RED %d\r\n"), *InPrefix, (INT)MVS_BaseTextureRed );
	DefinesString += FString::Printf(TEXT("#define %s_BASE_TEXTURE_GREEN %d\r\n"), *InPrefix, (INT)MVS_BaseTextureGreen );
	DefinesString += FString::Printf(TEXT("#define %s_BASE_TEXTURE_BLUE %d\r\n"), *InPrefix, (INT)MVS_BaseTextureBlue );
	DefinesString += FString::Printf(TEXT("#define %s_BASE_TEXTURE_ALPHA %d\r\n"), *InPrefix, (INT)MVS_BaseTextureAlpha );
	DefinesString += FString::Printf(TEXT("#define %s_MASK_TEXTURE_RED %d\r\n"), *InPrefix, (INT)MVS_MaskTextureRed );
	DefinesString += FString::Printf(TEXT("#define %s_MASK_TEXTURE_GREEN %d\r\n"), *InPrefix, (INT)MVS_MaskTextureGreen );
	DefinesString += FString::Printf(TEXT("#define %s_MASK_TEXTURE_BLUE %d\r\n"), *InPrefix, (INT)MVS_MaskTextureBlue );
	DefinesString += FString::Printf(TEXT("#define %s_MASK_TEXTURE_ALPHA %d\r\n"), *InPrefix, (INT)MVS_MaskTextureAlpha );
	DefinesString += FString::Printf(TEXT("#define %s_NORMAL_TEXTURE_ALPHA %d\r\n"), *InPrefix, (INT)MVS_NormalTextureAlpha );
	DefinesString += FString::Printf(TEXT("#define %s_EMISSIVE_TEXTURE_RED %d\r\n"), *InPrefix, (INT)MVS_EmissiveTextureRed );
	DefinesString += FString::Printf(TEXT("#define %s_EMISSIVE_TEXTURE_GREEN %d\r\n"), *InPrefix, (INT)MVS_EmissiveTextureGreen );
	DefinesString += FString::Printf(TEXT("#define %s_EMISSIVE_TEXTURE_BLUE %d\r\n"), *InPrefix, (INT)MVS_EmissiveTextureBlue );
	DefinesString += FString::Printf(TEXT("#define %s_EMISSIVE_TEXTURE_ALPHA %d\r\n"), *InPrefix, (INT)MVS_EmissiveTextureAlpha );
	DefinesString += FString::Printf(TEXT("#define %s %d\r\n"), *InPrefix, InValue );
}


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
 * @param MobileSettingsMobileLODBias,   from settings
 * @param MobileSettingsMobileBoneCount,	from settings
 * @param MobileSettingsMobileBoneWeightCount,	from settings
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
	)
{
	const UBOOL bUseLighting = (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsLightingEnabled) != 0);
	const UBOOL bUseLightmap = bUseLighting && (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsLightmap) != 0);//( BaseFeatures & EShaderBaseFeatures::Lightmap ) != 0;
	const UBOOL bUseDirectionalLightmap = bUseLightmap && (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsDirectionalLightmap) != 0);
	const UBOOL bUseVertexLightmap = FALSE;//bUseLighting && ( BaseFeatures & EShaderBaseFeatures::VertexLightmap ) != 0;
	const UBOOL bUseGPUSkinning = (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsSkinned) != 0);//( BaseFeatures & EShaderBaseFeatures::GPUSkinning ) != 0;
	const UBOOL bIsDecal = (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsDecal) != 0);//( BaseFeatures & EShaderBaseFeatures::Decal ) != 0;
	const UBOOL bUseSubUVParticles = (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsSubUV) != 0);//( BaseFeatures & EShaderBaseFeatures::SubUVParticles ) != 0;
	const UBOOL bIsLandscape = (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsLandscape) != 0);
	const UBOOL bUseDynamicDirectionalLight = bUseLighting && !bUseLightmap && !bUseVertexLightmap;
	const UBOOL bUseDynamicSkyLight = bUseLighting && !bUseLightmap && !bUseVertexLightmap;
	const EMobileGlobalShaderType GlobalShaderType = (EMobileGlobalShaderType)KeyData.GetFieldValue(FProgramKeyData::PKDT_GlobalShaderType);
	const EMobileDepthShaderType DepthShaderType = (EMobileDepthShaderType)KeyData.GetFieldValue(FProgramKeyData::PKDT_DepthShaderType);
	const UBOOL bShadowProjection = KeyData.GetFieldValue(FProgramKeyData::PKDT_ForwardShadowProjectionShaderType) != 0;

	// Make a string of vertex shader extensions
	FString VertexShaderExtensions;

	// Make a string of pixel shader extensions
	FString PixelShaderExtensions;

	if (Platform != UE3::PLATFORM_Android && Platform != UE3::PLATFORM_Flash)
	{
		PixelShaderExtensions += FString::Printf(TEXT("#extension GL_OES_standard_derivatives : enable\r\n"));
	}

	// Make a string of defines
	FString DefinesString;

	// Platform
	DefinesString += FString::Printf(TEXT("#define NGP      %d\r\n"), (Platform & UE3::PLATFORM_NGP)     ? 1: 0 );
	DefinesString += FString::Printf(TEXT("#define IPHONE   %d\r\n"), (Platform & UE3::PLATFORM_IPhone)  ? 1: 0 );
	DefinesString += FString::Printf(TEXT("#define WINDOWS  %d\r\n"), (Platform & UE3::PLATFORM_Windows) ? 1: 0 );
	DefinesString += FString::Printf(TEXT("#define ANDROID  %d\r\n"), (Platform & UE3::PLATFORM_Android) ? 1: 0 );
	DefinesString += FString::Printf(TEXT("#define FLASH    %d\r\n"), (Platform & UE3::PLATFORM_Flash)   ? 1: 0 );

	// Defines for extensions
	UBOOL bUseLodShaderExtension = FALSE;
	DefinesString += FString::Printf(TEXT("#define USE_GL_EXT_shader_texture_lod   %d\r\n"), bUseLodShaderExtension ? 1: 0 );

	// Pass features
	UBOOL bIsDepthOnly = (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsDepthOnlyRendering) != 0) ? TRUE : FALSE;
	UBOOL bNormalDepth = FALSE;
	UBOOL bShadowDepth = FALSE;
	if ( bIsDepthOnly )
	{
		switch ( DepthShaderType )
		{
			case MobileDepthShader_Normal:
				bNormalDepth = TRUE;
				break;
			case MobileDepthShader_Shadow:
				bShadowDepth = TRUE;
				break;
		}
	}
	DefinesString += FString::Printf(TEXT("#define DEPTH_ONLY %d\r\n"), bNormalDepth ? 1 : 0);
	DefinesString += FString::Printf(TEXT("#define SHADOW_DEPTH_ONLY %d\r\n"), bShadowDepth ? 1 : 0);
	DefinesString += FString::Printf(TEXT("#define SHADOW_PROJECTION %d\r\n"), bShadowProjection ? 1 : 0);

	// SRGB gamma
	const UBOOL bUseSRGBReads = KeyData.GetFieldValue(FProgramKeyData::PKDT_UseGammaCorrection) != 0;
	const UBOOL bUseSRGBWrites = KeyData.GetFieldValue(FProgramKeyData::PKDT_UseGammaCorrection) != 0;
	DefinesString += FString::Printf(TEXT("#define USE_SRGB_READS %d\r\n"), bUseSRGBReads ? 1 : 0);
	DefinesString += FString::Printf(TEXT("#define USE_SRGB_WRITES %d\r\n"), bUseSRGBWrites ? 1 : 0);

	// Base features
	DefinesString += FString::Printf(TEXT("#define USE_LIGHTMAP %d\r\n"), bUseLightmap ? 1 : 0);
	DefinesString += FString::Printf(TEXT("#define USE_DIRECTIONAL_LIGHTMAP %d\r\n"), bUseDirectionalLightmap ? 1 : 0);
	DefinesString += FString::Printf(TEXT("#define USE_VERTEX_LIGHTMAP %d\r\n"), bUseVertexLightmap ? 1 : 0);
	DefinesString += FString::Printf(TEXT("#define USE_GPU_SKINNING %d\r\n"), bUseGPUSkinning ? 1 : 0);
	DefinesString += FString::Printf(TEXT("#define SUBUV_PARTICLES %d\r\n"), bUseSubUVParticles ? 1 : 0);

	// Landscape
	DefinesString += FString::Printf(TEXT("#define IS_LANDSCAPE %d\r\n"), bIsLandscape ? 1 : 0);

	if( bIsLandscape )
	{
		DefinesString += FString::Printf(TEXT("#define LANDSCAPE_USE_MONOCHROME_LAYER_BLENDING %d\r\n"), KeyData.GetFieldValue(FProgramKeyData::PKDT_UseLandscapeMonochromeLayerBlending) ? 1 : 0);
	}

	// Decals
	DefinesString += FString::Printf(TEXT("#define IS_DECAL %d\r\n"), bIsDecal ? 1 : 0);

	// Pixel shader: Default texture sampling LOD bias to use when sampling most textures
	DefinesString += FString::Printf(TEXT("#define DEFAULT_LOD_BIAS %0.2f\r\n"), MobileSettingsMobileLODBias );

	if ( bUseGPUSkinning )
	{
		DefinesString += FString::Printf(TEXT("#define MAX_BONES %d\r\n"), MobileSettingsMobileBoneCount ? MobileSettingsMobileBoneCount : 75);
		DefinesString += FString::Printf(TEXT("#define MAX_BONE_WEIGHTS %d\r\n"), 
			bNormalDepth ? 1 : MobileSettingsMobileBoneWeightCount);
	}


	// Instance features
	// Lens flares use the particle pixel shader that has USE_FOG, but the LF vertex shader won't output the fog,
	// so we disable fog when using a lens flare (we could add dummy fog output to LF vertex shader, but that is wasted GPU time)
	DefinesString += FString::Printf(TEXT("#define USE_GRADIENT_FOG %d\r\n"), 
		(KeyData.GetFieldValue(FProgramKeyData::PKDT_IsGradientFogEnabled) && (PrimitiveType != EPT_LensFlare)) ? 1 : 0);
	DefinesString += FString::Printf(TEXT("#define USE_HEIGHT_FOG %d\r\n"), 
		(KeyData.GetFieldValue(FProgramKeyData::PKDT_IsHeightFogEnabled) && (PrimitiveType != EPT_LensFlare)) ? 1 : 0);

#if ANDROID && WITH_ES2_RHI // Android needs to handle alpha-test differently at run-time than cook time
	DefinesString += FString::Printf(TEXT("#define USE_ALPHAKILL %d\r\n"), 
		(((KeyData.GetFieldValue(FProgramKeyData::PKDT_BlendMode) == BLEND_Masked) || (PrimitiveType == EPT_DistanceFieldFont)) && GMobileAllowShaderDiscard)? 1 : 0);

	DefinesString += FString::Printf(TEXT("#define PREPROCESS_ALPHAKILL %d\r\n"), 1);
	DefinesString += FString::Printf(TEXT("#define SUPPORTS_DISCARD %d\r\n"), GMobileAllowShaderDiscard ? 1 : 0);
#else
	DefinesString += FString::Printf(TEXT("#define USE_ALPHAKILL %d\r\n"), 
		((KeyData.GetFieldValue(FProgramKeyData::PKDT_BlendMode) == BLEND_Masked) || (PrimitiveType == EPT_DistanceFieldFont))? 1 : 0);

	// Don't preprocess ALPHAKILL on Android due to some GPU's not supporting discard
	DefinesString += FString::Printf(TEXT("#define PREPROCESS_ALPHAKILL %d\r\n"), 
		(Platform != UE3::PLATFORM_Android)? 1 : 0);
	DefinesString += FString::Printf(TEXT("#define SUPPORTS_DISCARD %d\r\n"), GMobileAllowShaderDiscard ? 1 : 0);
#endif

	// Handle the derivative instruction.  Right now we disable the derivative instructions on the Android platform though we should
	// really define this at runtime (just like the ALPHAKILL function)
	DefinesString += FString::Printf(TEXT("#define PREPROCESS_DERIVATIVES %d\r\n"), 1);
	DefinesString += FString::Printf(TEXT("#define USE_DERIVATIVES %d\r\n"),
		(Platform != UE3::PLATFORM_Android)? 1 : 0);
	
	// Distance field fonts
	DefinesString += FString::Printf(TEXT("#define USE_DISTANCE_FIELD_FONTS %d\r\n"), 
		(PrimitiveType == EPT_DistanceFieldFont) ? 1 : 0);

	// Texture coordinates mapping
	{
		DefinesString += FString::Printf(TEXT("#define TEXCOORD_SOURCE_TEXCOORDS0 %d\r\n"), 0 );
		DefinesString += FString::Printf(TEXT("#define TEXCOORD_SOURCE_TEXCOORDS1 %d\r\n"), 1 );
		DefinesString += FString::Printf(TEXT("#define TEXCOORD_SOURCE_TEXCOORDS2 %d\r\n"), 2 );
		DefinesString += FString::Printf(TEXT("#define TEXCOORD_SOURCE_TEXCOORDS3 %d\r\n"), 3 );

		// Base texture texture coordinate source
		DefinesString += FString::Printf(TEXT("#define BASE_TEXTURE_TEXCOORDS_SOURCE %d\r\n"), 
			KeyData.GetFieldValue(FProgramKeyData::PKDT_BaseTextureTexCoordsSource));

		// Detail texture texture coordinate source
		DefinesString += FString::Printf(TEXT("#define DETAIL_TEXTURE_TEXCOORDS_SOURCE %d\r\n"), 
			KeyData.GetFieldValue(FProgramKeyData::PKDT_DetailTextureTexCoordsSource));

		// Mask texture texture coordinate source
		DefinesString += FString::Printf(TEXT("#define MASK_TEXTURE_TEXCOORDS_SOURCE %d\r\n"), 
			KeyData.GetFieldValue(FProgramKeyData::PKDT_MaskTextureTexCoordsSource));
	}

	DefinesString += FString::Printf(TEXT("#define USE_BASE_TEX_COORD_XFORM %d\r\n"), KeyData.GetFieldValue(FProgramKeyData::PKDT_IsBaseTextureTransformed) != 0 ? 1 : 0);
	DefinesString += FString::Printf(TEXT("#define USE_EMISSIVE_TEX_COORD_XFORM %d\r\n"), KeyData.GetFieldValue(FProgramKeyData::PKDT_IsEmissiveTextureTransformed) != 0 ? 1 : 0);
	DefinesString += FString::Printf(TEXT("#define USE_NORMAL_TEX_COORD_XFORM %d\r\n"), KeyData.GetFieldValue(FProgramKeyData::PKDT_IsNormalTextureTransformed) != 0 ? 1 : 0);
	DefinesString += FString::Printf(TEXT("#define USE_MASK_TEX_COORD_XFORM %d\r\n"), KeyData.GetFieldValue(FProgramKeyData::PKDT_IsMaskTextureTransformed) != 0 ? 1 : 0);
	DefinesString += FString::Printf(TEXT("#define USE_DETAIL_TEX_COORD_XFORM %d\r\n"), KeyData.GetFieldValue(FProgramKeyData::PKDT_IsDetailTextureTransformed) != 0 ? 1 : 0);

	const UBOOL bIsNormalMappingEnabled = KeyData.GetFieldValue(FProgramKeyData::PKDT_IsNormalMappingEnabled) != 0;
	const UBOOL bIsEnvironmentMappingEnabled = KeyData.GetFieldValue(FProgramKeyData::PKDT_IsEnvironmentMappingEnabled) != 0;

	// Specular
	{
		const UBOOL bIsLightmapSpecularEnabled = bUseDirectionalLightmap && (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsSpecularEnabled) != 0);
		const UBOOL bIsPixelSpecularEnabled = 
			(KeyData.GetFieldValue(FProgramKeyData::PKDT_IsSpecularEnabled) != 0) && 
			(KeyData.GetFieldValue(FProgramKeyData::PKDT_IsPixelSpecularEnabled) != 0) && 
			bIsNormalMappingEnabled &&
			!bIsLightmapSpecularEnabled;
		const UBOOL bIsVertexSpecularEnabled = 
			(KeyData.GetFieldValue(FProgramKeyData::PKDT_IsSpecularEnabled) != 0) && !bIsPixelSpecularEnabled && !bIsLightmapSpecularEnabled;
		const UBOOL bIsSpecularEnabled = bIsPixelSpecularEnabled || bIsVertexSpecularEnabled || bIsLightmapSpecularEnabled;
		DefinesString += FString::Printf(TEXT("#define USE_SPECULAR %d\r\n"), bIsSpecularEnabled ? 1 : 0);
		DefinesString += FString::Printf(TEXT("#define USE_VERTEX_SPECULAR %d\r\n"), bIsVertexSpecularEnabled ? 1 : 0);
		DefinesString += FString::Printf(TEXT("#define USE_PIXEL_SPECULAR %d\r\n"), bIsPixelSpecularEnabled ? 1 : 0);
		DefinesString += FString::Printf(TEXT("#define USE_LIGHTMAP_SPECULAR %d\r\n"), bIsLightmapSpecularEnabled ? 1 : 0);

		const UBOOL bIsDetailNormalEnabled = (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsDetailNormalEnabled) != 0);
		DefinesString += FString::Printf(TEXT("#define USE_DETAIL_NORMAL %d\r\n"), bIsDetailNormalEnabled ? 1 : 0);

		// We allow specular mask for both specular and character lighting map ('fake spec') features
		const UBOOL bIsSpecularMaskSupported = bIsSpecularEnabled;
		{
			// This string maps EMobileSpecularMask to #define values that we can switch off of in our shader
			FString SpecularMaskEnumDefineString;
			SpecularMaskEnumDefineString += FString::Printf( TEXT( "#define SPECMASK_CONSTANT %d\r\n" ), (INT)MSM_Constant );
			SpecularMaskEnumDefineString += FString::Printf( TEXT( "#define SPECMASK_LUMINANCE %d\r\n" ), (INT)MSM_Luminance );
			SpecularMaskEnumDefineString += FString::Printf( TEXT( "#define SPECMASK_DIFFUSE_RED %d\r\n" ), (INT)MSM_DiffuseRed );
			SpecularMaskEnumDefineString += FString::Printf( TEXT( "#define SPECMASK_DIFFUSE_GREEN %d\r\n" ), (INT)MSM_DiffuseGreen );
			SpecularMaskEnumDefineString += FString::Printf( TEXT( "#define SPECMASK_DIFFUSE_BLUE %d\r\n" ), (INT)MSM_DiffuseBlue );
			SpecularMaskEnumDefineString += FString::Printf( TEXT( "#define SPECMASK_DIFFUSE_ALPHA %d\r\n" ), (INT)MSM_DiffuseAlpha );
			SpecularMaskEnumDefineString += FString::Printf( TEXT( "#define SPECMASK_MASK_TEXTURE_RGB %d\r\n" ), (INT)MSM_MaskTextureRGB );
			SpecularMaskEnumDefineString += FString::Printf( TEXT( "#define SPECMASK_MASK_TEXTURE_RED %d\r\n" ), (INT)MSM_MaskTextureRed );
			SpecularMaskEnumDefineString += FString::Printf( TEXT( "#define SPECMASK_MASK_TEXTURE_GREEN %d\r\n" ), (INT)MSM_MaskTextureGreen );
			SpecularMaskEnumDefineString += FString::Printf( TEXT( "#define SPECMASK_MASK_TEXTURE_BLUE %d\r\n" ), (INT)MSM_MaskTextureBlue );
			SpecularMaskEnumDefineString += FString::Printf( TEXT( "#define SPECMASK_MASK_TEXTURE_ALPHA %d\r\n" ), (INT)MSM_MaskTextureAlpha );

			DefinesString += SpecularMaskEnumDefineString;
			DefinesString += FString::Printf(TEXT("#define SPECULAR_MASK %d\r\n"), KeyData.GetFieldValue(FProgramKeyData::PKDT_SpecularMask));
		}
	}

	DefinesString += FString::Printf(TEXT("#define ALPHA_VALUE_SOURCE %d\r\n"), KeyData.GetFieldValue(FProgramKeyData::PKDT_AlphaValueSource));
	DefinesString += FString::Printf( TEXT( "#define ALPHA_VALUE_SOURCE_DIFFUSE_ALPHA %d\r\n" ), (INT)MAVS_DiffuseTextureAlpha );
	DefinesString += FString::Printf( TEXT( "#define ALPHA_VALUE_SOURCE_MASK_RED %d\r\n" ), (INT)MAVS_MaskTextureRed);
	DefinesString += FString::Printf( TEXT( "#define ALPHA_VALUE_SOURCE_MASK_GREEN %d\r\n" ), (INT)MAVS_MaskTextureGreen);
	DefinesString += FString::Printf( TEXT( "#define ALPHA_VALUE_SOURCE_MASK_BLUE %d\r\n" ), (INT)MAVS_MaskTextureBlue);

	DefinesString += FString::Printf(TEXT("#define USE_NORMAL_MAPPING %d\r\n"), bIsNormalMappingEnabled ? 1 : 0);
	DefinesString += FString::Printf(TEXT("#define USE_ENVIRONMENT_MAPPING %d\r\n"), bIsEnvironmentMappingEnabled ? 1 : 0);
	{
		CreateValueSourceDefines(DefinesString, TEXT("ENVIRONMENT_MASK_SOURCE"), (INT)KeyData.GetFieldValue(FProgramKeyData::PKDT_EnvironmentMaskSource));

		DefinesString += FString::Printf(TEXT("#define ENVIRONMENT_BLEND_ADD %d\r\n"), (INT)MEBM_Add );
		DefinesString += FString::Printf(TEXT("#define ENVIRONMENT_BLEND_LERP %d\r\n"), (INT)MEBM_Lerp );
		DefinesString += FString::Printf(TEXT("#define ENVIRONMENT_BLEND_MODE %d\r\n"), KeyData.GetFieldValue(FProgramKeyData::PKDT_MobileEnvironmentBlendMode));

		const UBOOL bUseEnvironmentFresnel = bIsEnvironmentMappingEnabled && KeyData.GetFieldValue(FProgramKeyData::PKDT_IsMobileEnvironmentFresnelEnabled);
		DefinesString += FString::Printf(TEXT("#define USE_ENVIRONMENT_FRESNEL %d\r\n"), bUseEnvironmentFresnel ? 1 : 0);
	}


	const UBOOL bIsEmissiveEnabled = (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsEmissiveEnabled) != 0);
	DefinesString += FString::Printf(TEXT("#define USE_EMISSIVE %d\r\n"), bIsEmissiveEnabled ? 1 : 0);
	{
		CreateValueSourceDefines(DefinesString, TEXT("EMISSIVE_MASK_SOURCE"), (INT)KeyData.GetFieldValue(FProgramKeyData::PKDT_EmissiveMaskSource));

		DefinesString += FString::Printf(TEXT("#define EMISSIVE_COLOR_SOURCE_EMISSIVE_TEXTURE %d\r\n"), (INT)MECS_EmissiveTexture );
		DefinesString += FString::Printf(TEXT("#define EMISSIVE_COLOR_SOURCE_BASE_TEXTURE %d\r\n"), (INT)MECS_BaseTexture );
		DefinesString += FString::Printf(TEXT("#define EMISSIVE_COLOR_SOURCE_CONSTANT %d\r\n"), (INT)MECS_Constant );
		DefinesString += FString::Printf(TEXT("#define EMISSIVE_COLOR_SOURCE %d\r\n"), KeyData.GetFieldValue(FProgramKeyData::PKDT_EmissiveColorSource) );
	}


	const UBOOL bIsRimLightingEnabled = (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsRimLightingEnabled) != 0);
	DefinesString += FString::Printf(TEXT("#define USE_RIM_LIGHTING %d\r\n"), bIsRimLightingEnabled ? 1 : 0);
	if( bIsRimLightingEnabled )
	{
		CreateValueSourceDefines( DefinesString, TEXT("RIM_LIGHTING_MASK_SOURCE"), (INT)KeyData.GetFieldValue(FProgramKeyData::PKDT_RimLightingMaskSource) );
	}


	DefinesString += FString::Printf(TEXT("#define USE_BUMP_OFFSET %d\r\n"), (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsBumpOffsetEnabled) != 0) ? 1 : 0);

	UBOOL bIsUsingOneDetailTexture = (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsUsingOneDetailTexture) != 0);
	DefinesString += FString::Printf(TEXT("#define NEED_ONE_DETAIL_TEXTURES %d\r\n"), bIsUsingOneDetailTexture ? 1 : 0);
	if( bIsUsingOneDetailTexture )
	{
		DefinesString += FString::Printf(TEXT("#define NEED_TWO_DETAIL_TEXTURES %d\r\n"), (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsUsingTwoDetailTexture) != 0) ? 1 : 0);
		DefinesString += FString::Printf(TEXT("#define NEED_THREE_DETAIL_TEXTURES %d\r\n"), (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsUsingThreeDetailTexture) != 0) ? 1 : 0);

		DefinesString += FString::Printf(TEXT("#define TEXTURE_BLEND_FACTOR_SOURCE_VERTEX_COLOR %d\r\n"), (INT)MTBFS_VertexColor );
		DefinesString += FString::Printf(TEXT("#define TEXTURE_BLEND_FACTOR_SOURCE_MASK_TEXTURE %d\r\n"), (INT)MTBFS_MaskTexture );
		DefinesString += FString::Printf(TEXT("#define TEXTURE_BLEND_FACTOR_SOURCE %d\r\n"), KeyData.GetFieldValue(FProgramKeyData::PKDT_TextureBlendFactorSource));
	}
	{
		EBlendMode BlendMode = (EBlendMode)KeyData.GetFieldValue(FProgramKeyData::PKDT_BlendMode);

		DefinesString += FString::Printf(TEXT("#define USE_OPACITY_MULTIPLIER %d\r\n"), BlendMode != BLEND_Opaque ? 1 : 0);
		DefinesString += FString::Printf(TEXT("#define USE_PREMULTIPLIED_OPACITY %d\r\n"), (BlendMode == BLEND_Additive) ? 1 : 0);
	}
	DefinesString += FString::Printf(TEXT("#define USE_VERTEX_MOVEMENT %d\r\n"), (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsWaveVertexMovementEnabled) != 0) ? 1 : 0);
	if (bUseLightmap || bUseVertexLightmap)
	{
		// Only use a fixed lightmap scale if gamma correction is turned off.  Without gamma correction
		// we can't properly scale lightmaps without severely tinting the hue.
		const UBOOL bUseFixedLightmapScale = !bUseDirectionalLightmap && (KeyData.GetFieldValue(FProgramKeyData::PKDT_UseGammaCorrection) == 0);
		DefinesString += FString::Printf(TEXT("#define USE_LIGHTMAP_FIXED_SCALE %d\r\n"), bUseFixedLightmapScale ? 1 : 0);
	}
	DefinesString += FString::Printf(TEXT("#define USE_DYNAMIC_DIRECTIONAL_LIGHT %d\r\n"), bUseDynamicDirectionalLight ? 1 : 0);
	DefinesString += FString::Printf(TEXT("#define USE_DYNAMIC_SKY_LIGHT %d\r\n"), bUseDynamicSkyLight ? 1 : 0);

	// Ambient occlusion
	const UBOOL bUseAmbientOcclusion = bUseLighting && ( (EMobileAmbientOcclusionSource)KeyData.GetFieldValue(FProgramKeyData::PKDT_AmbientOcclusionSource) != MAOS_Disabled );
	DefinesString += FString::Printf(TEXT("#define USE_AMBIENT_OCCLUSION %d\r\n"), bUseAmbientOcclusion ? 1 : 0);
	{
		// This string maps EMobileAmbientOcclusionSource to #define values that we can switch off of in our shader
		FString AOSourceEnumDefineString;
		AOSourceEnumDefineString += FString::Printf( TEXT( "#define AOSOURCE_VERTEX_COLOR_RED %d\r\n" ), (INT)MAOS_VertexColorRed );
		AOSourceEnumDefineString += FString::Printf( TEXT( "#define AOSOURCE_VERTEX_COLOR_GREEN %d\r\n" ), (INT)MAOS_VertexColorGreen );
		AOSourceEnumDefineString += FString::Printf( TEXT( "#define AOSOURCE_VERTEX_COLOR_BLUE %d\r\n" ), (INT)MAOS_VertexColorBlue );
		AOSourceEnumDefineString += FString::Printf( TEXT( "#define AOSOURCE_VERTEX_COLOR_ALPHA %d\r\n" ), (INT)MAOS_VertexColorAlpha );

		DefinesString += AOSourceEnumDefineString;
		DefinesString += FString::Printf(TEXT("#define AMBIENT_OCCLUSION_SOURCE %d\r\n"), KeyData.GetFieldValue(FProgramKeyData::PKDT_AmbientOcclusionSource));
	}

	//Color multiply
	{
		DefinesString += FString::Printf(TEXT("#define USE_UNIFORM_COLOR_MULTIPLY %d\r\n"), (KeyData.GetFieldValue(FProgramKeyData::PKDT_UseUniformColorMultiply) != 0) ? 1 : 0);
		DefinesString += FString::Printf(TEXT("#define USE_VERTEX_COLOR_MULTIPLY %d\r\n"), (KeyData.GetFieldValue(FProgramKeyData::PKDT_UseVertexColorMultiply) != 0) ? 1 : 0);

		UBOOL bUseColorMultiply = (KeyData.GetFieldValue(FProgramKeyData::PKDT_UseUniformColorMultiply) != 0) || (KeyData.GetFieldValue(FProgramKeyData::PKDT_UseVertexColorMultiply) != 0);
		
		if (bUseColorMultiply)
		{
			FString CMSourceDefineString;

			// This allows us to select what channel we require for color multiply in the shader
			CMSourceDefineString += FString::Printf( TEXT( "#define COLOR_MULTIPLY_SOURCE_NONE %d\r\n" ), (INT)MCMS_None );
			CMSourceDefineString += FString::Printf( TEXT( "#define COLOR_MULTIPLY_SOURCE_BASE_TEXTURE_RED %d\r\n" ), (INT)MCMS_BaseTextureRed );
			CMSourceDefineString += FString::Printf( TEXT( "#define COLOR_MULTIPLY_SOURCE_BASE_TEXTURE_GREEN %d\r\n" ), (INT)MCMS_BaseTextureGreen );
			CMSourceDefineString += FString::Printf( TEXT( "#define COLOR_MULTIPLY_SOURCE_BASE_TEXTURE_BLUE %d\r\n" ), (INT)MCMS_BaseTextureBlue );
			CMSourceDefineString += FString::Printf( TEXT( "#define COLOR_MULTIPLY_SOURCE_BASE_TEXTURE_ALPHA %d\r\n" ), (INT)MCMS_BaseTextureAlpha );
			CMSourceDefineString += FString::Printf( TEXT( "#define COLOR_MULTIPLY_SOURCE_MASK_TEXTURE_RED %d\r\n" ), (INT)MCMS_MaskTextureRed );
			CMSourceDefineString += FString::Printf( TEXT( "#define COLOR_MULTIPLY_SOURCE_MASK_TEXTURE_GREEN %d\r\n" ), (INT)MCMS_MaskTextureGreen );
			CMSourceDefineString += FString::Printf( TEXT( "#define COLOR_MULTIPLY_SOURCE_MASK_TEXTURE_BLUE %d\r\n" ), (INT)MCMS_MaskTextureBlue );
			CMSourceDefineString += FString::Printf( TEXT( "#define COLOR_MULTIPLY_SOURCE_MASK_TEXTURE_ALPHA %d\r\n" ), (INT)MCMS_MaskTextureAlpha );

			DefinesString += CMSourceDefineString;
			DefinesString += FString::Printf( TEXT( "#define COLOR_MULTIPLY_SOURCE %d\r\n" ), (INT)KeyData.GetFieldValue(FProgramKeyData::PKDT_ColorMultiplySource) );
		}

		UBOOL bUseFallbackStreamColor = KeyData.GetFieldValue(FProgramKeyData::PKDT_UseFallbackStreamColor) != 0;
		DefinesString += FString::Printf(TEXT("#define USE_FALLBACK_STREAM_COLOR %d\r\n"), bUseFallbackStreamColor ? 1 : 0);
	}

	// Screen alignment
	{
		// This string maps EMobileParticleScreenAlignment to #define values that we can switch off of in our shader
		FString ParticleScreenAlignmentEnumDefineString;
		ParticleScreenAlignmentEnumDefineString += FString::Printf( TEXT( "#define PARTICLESCREENALIGN_CAMERAFACING %d\r\n" ), (INT)SSA_CameraFacing );
		ParticleScreenAlignmentEnumDefineString += FString::Printf( TEXT( "#define PARTICLESCREENALIGN_VELOCITY %d\r\n" ), (INT)SSA_Velocity );
		ParticleScreenAlignmentEnumDefineString += FString::Printf( TEXT( "#define PARTICLESCREENALIGN_LOCKEDAXIS %d\r\n" ), (INT)SSA_LockedAxis );
		DefinesString += ParticleScreenAlignmentEnumDefineString;
		if (KeyData.GetFieldValue(FProgramKeyData::PKDT_ParticleScreenAlignment) == (INT)SSA_LockedAxis)
		{
			warnf(NAME_Warning, TEXT("ParticleShader w/ LOCKED AXIS!!!!"));
		}
		DefinesString += FString::Printf(TEXT("#define PARTICLE_SCREEN_ALIGNMENT %d\r\n"), KeyData.GetFieldValue(FProgramKeyData::PKDT_ParticleScreenAlignment));
	}

	// Mobile settings
	{
		DefinesString += FString::Printf(TEXT("#define USE_MOBILE_COLOR_GRADING %d\r\n"), KeyData.GetFieldValue(FProgramKeyData::PKDT_IsMobileColorGradingEnabled));
	}

	// Scaleform settings
	{
		DefinesString += FString::Printf(TEXT("#define USE_GFX_GAMMA_CORRECTION %d\r\n"), KeyData.GetFieldValue(FProgramKeyData::PKDT_IsGfxGammaCorrectionEnabled));
	}

	// Global shader-specific settings
	if( PrimitiveType == EPT_GlobalShader )
	{
		// 0 means not used
		UINT UberPostProcessPermutation = 0;

		switch(GlobalShaderType)
		{
			case EGST_MobileUberPostProcess1: UberPostProcessPermutation = 1; break;
			case EGST_MobileUberPostProcess3: UberPostProcessPermutation = 3; break;
			case EGST_MobileUberPostProcess4: UberPostProcessPermutation = 4; break;
			case EGST_MobileUberPostProcess5: UberPostProcessPermutation = 5; break;
			case EGST_MobileUberPostProcess7: UberPostProcessPermutation = 7; break;

			case EGST_BloomGather:            UberPostProcessPermutation = 1; break;
			case EGST_DOFAndBloomGather:      UberPostProcessPermutation = 3; break;
		}

		if(UberPostProcessPermutation)
		{
			DefinesString += FString::Printf(TEXT("#define USE_POSTPROCESS_MOBILE_COLOR %d\r\n"),	UberPostProcessPermutation & 0x4 );
			DefinesString += FString::Printf(TEXT("#define USE_POSTPROCESS_MOBILE_DOF %d\r\n"),		UberPostProcessPermutation & 0x2 );
			DefinesString += FString::Printf(TEXT("#define USE_POSTPROCESS_MOBILE_BLOOM %d\r\n"),	UberPostProcessPermutation & 0x1 );
		}
	}

	// Windows OpenGL ES2 implementations do not support overriding the floating point precision, so we only set
	// these when compiling shaders for console targets and for profiling.
	FString VertexShaderDefs, PixelShaderDefs;

#if !FLASH
	if(!bIsCompilingForPC && !(Platform & UE3::PLATFORM_NGP))
	{
		// is defined when we are on tha target platform or for profiling
		VertexShaderDefs = TEXT("#define DEFINE_DEFAULT_PRECISION 1\r\n");
		PixelShaderDefs = TEXT("#define DEFINE_DEFAULT_PRECISION 1\r\n");
	}

	VertexShaderDefs += TEXT( "#ifdef DEFINE_DEFAULT_PRECISION\r\nprecision highp float;\r\n#endif\r\n" );
	PixelShaderDefs += TEXT( "#ifdef DEFINE_DEFAULT_PRECISION\r\nprecision mediump float;\r\n#endif\r\n" );
#endif

	FString VertexShaderText;
	FString PixelShaderText;

	if (PreloadedVertexBody == NULL)
	{
		// load the actual shader files
		FString ShaderFilename;
#if FLASH
		ShaderFilename = appEngineDir() + TEXT("Shaders\\Flash\\") + GetES2ShaderFilename(PrimitiveType,GlobalShaderType,SF_Vertex) + TEXT(".msf");
#else
		ShaderFilename = appEngineDir() + TEXT("Shaders\\Mobile\\") + GetES2ShaderFilename(PrimitiveType,GlobalShaderType,SF_Vertex) + TEXT(".msf");
#endif

		if (!appLoadFileToString(VertexShaderText, *ShaderFilename))
		{
			appErrorf( TEXT("Failed to load shader file '%s'"),*ShaderFilename);
			return FALSE;
		}

		PreloadedVertexBody = *VertexShaderText;
	}

	if (PreloadedPixelBody == NULL)
	{
		// load the actual shader files
		FString ShaderFilename;
#if FLASH
		ShaderFilename = appEngineDir() + TEXT("Shaders\\Flash\\") + GetES2ShaderFilename(PrimitiveType,GlobalShaderType,SF_Pixel) + TEXT(".msf");
#else
		ShaderFilename = appEngineDir() + TEXT("Shaders\\Mobile\\") + GetES2ShaderFilename(PrimitiveType,GlobalShaderType,SF_Pixel) + TEXT(".msf");
#endif

		if (!appLoadFileToString(PixelShaderText, *ShaderFilename))
		{
			appErrorf( TEXT("Failed to load shader file '%s'"),*ShaderFilename);
			return FALSE;
		}
		PreloadedPixelBody = *PixelShaderText;
	}

	OutVertexShader =
		   VertexShaderExtensions + TEXT("\r\n")
		 + DefinesString + TEXT("\r\n")
		 + VertexShaderDefs + TEXT("\r\n")
#if !FLASH
		 + TEXT("#line 1000\r\n")
#endif
		 + CommonShaderPrefixFile + TEXT("\r\n")
#if !FLASH
		 + TEXT("#line 2000\r\n")
#endif
		 + VertexShaderPrefixFile + TEXT("\r\n")
#if !FLASH
		 + TEXT("#line 10000\r\n")
#endif
		 + PreloadedVertexBody;

	OutPixelShader =
		  PixelShaderExtensions + TEXT("\r\n")
		+ DefinesString + TEXT("\r\n")
		+ PixelShaderDefs + TEXT("\r\n")
#if !FLASH
		+ TEXT("#line 1000\r\n")
#endif
		+ CommonShaderPrefixFile + TEXT("\r\n")
#if !FLASH
		+ TEXT("#line 2000\r\n")
#endif
		+ PixelShaderPrefixFile + TEXT("\r\n")
#if !FLASH
		+ TEXT("#line 10000\r\n")
#endif
		+ PreloadedPixelBody;

	return TRUE;
}

/**
 * Starts compiling shaders on the rendering thread.
 * Starts the rendering thread if it's not already running, even on single-core CPUs.
 *
 * @param ShaderGroupName						The name of the ShaderGroup to start compiling
 * @param bPauseGameRenderingWhileCompiling		Whether or not to pause rendering while comiling
 */
void FMobileShaderInitialization::StartCompilingShaderGroup( FName ShaderGroupName, UBOOL bPauseGameRenderingWhileCompiling )
{
#if WITH_ES2_RHI && MOBILESHADER_THREADED_INIT
	if ( GUsingES2RHI )
	{
#if IPHONE
		void RecompileES2GlobalShaders();
		RecompileES2GlobalShaders();
#else

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			RecompileES2GlobalShadersCommand,
			{
				void RecompileES2GlobalShaders();
				RecompileES2GlobalShaders();
			});
#endif

		LoadCachedShaderKeys();

		debugf(TEXT("StartCompilingShaderGroup: %s"), *ShaderGroupName.ToString());

		UBOOL bOneThread = ParseParam(appCmdLine(),TEXT("ONETHREAD")); 

		CurrentState = MobileShaderInit_Started;
		if ( !GUseThreadedRendering && !bOneThread)
		{
			bTemporarilyEnableRenderthread = TRUE;
			GUseThreadedRendering = TRUE;
			StartRenderingThread();
		}

		UINT NumShadersCompiledInGroup = 0;

		if ( PendingShaderGroups.HasKey(ShaderGroupName) )
		{
			bPauseGameRendering = bPauseGameRendering || bPauseGameRenderingWhileCompiling;

			// Pause game rendering so that it doesn't get blocked by the rendering thread compiling shaders.
			if (bPauseGameRendering)
			{
				FViewport::SetGameRenderingEnabled( FALSE );
			}

			TArray<FProgramKey> * ShaderKeys = PendingShaderGroups.Find(ShaderGroupName);

			for (INT ShaderKeyIndex = 0; ShaderKeyIndex < ShaderKeys->Num(); ShaderKeyIndex++)
			{
				FProgramKey ProgramKey = ShaderKeys->operator ()(ShaderKeyIndex);

				ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
					RecompileES2ShaderCommand,
					FProgramKey,ProgramKey,ProgramKey,
				{
					extern void RecompileES2Shader(FProgramKey ProgramKey);
					RecompileES2Shader(ProgramKey);
				});

				++NumShadersCompiledInGroup;
			}

			// remove the key the group that we just issued the compile commands for
			PendingShaderGroups.RemoveKey(ShaderGroupName);
		}

		debugf(TEXT("Compiled %d Shaders in group %s"), NumShadersCompiledInGroup, *ShaderGroupName.ToString());


#if IPHONE
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			WarmES2ShaderCacheCommand,
		{
			void WarmES2ShaderCache();
			WarmES2ShaderCache();
		});
#endif

		if (!CompletionFence)
		{
			CompletionFence = new FRenderCommandFence();
		}
		CompletionFence->BeginFence();
	}

#endif
}


void FMobileShaderInitialization::StartCompilingShaderGroupByMapName(FString MapName, UBOOL bPauseGameRenderingWhileCompiling)
{
#if WITH_ES2_RHI && MOBILESHADER_THREADED_INIT
	FName ShaderGroupName = GetShaderGroupNameFromMapName(MapName);
	if (ShaderGroupName != FName("None"))
	{
		StartCompilingShaderGroup(ShaderGroupName, bPauseGameRenderingWhileCompiling);
	}
#endif
}

FName FMobileShaderInitialization::GetShaderGroupNameFromMapName( FString MapName )
{
	for ( TMap<FName, TArray<FName> >::TConstIterator It(ShaderGroupPackages); It; ++It )
	{
		FName ShaderGroupName = It.Key();
		TArray<FName> * GroupPackages = ShaderGroupPackages.Find(It.Key());
		for (int j = 0; j < GroupPackages->Num(); j++)
		{
			if (GroupPackages->operator ()(j).ToString() == MapName)
			{
				return ShaderGroupName;
			}
		}
	}

	return FName("None");
}

UBOOL FMobileShaderInitialization::IsProgramKeyInGroup( FProgramKey ProgramKey )
{
	for (TMap<FName, TArray<FProgramKey> >::TConstIterator It(PendingShaderGroups); It; ++It)
	{
		FName ShaderGroupName = It.Key();
		
		TArray<FProgramKey> * ProgramKeys = PendingShaderGroups.Find(It.Key());
		for (int j = 0; j < ProgramKeys->Num(); ++j)
		{
			if ((*ProgramKeys)(j) == ProgramKey)
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

UBOOL FMobileShaderInitialization::LoadShaderGroup( const FString& FilePath, TArray<FProgramKey>& ShaderGroup )
{
	// Load the shader group as a strings
	FString FileContents;
	if ( appLoadFileToString(FileContents, *FilePath) == FALSE )
	{
		debugf(TEXT("Failed to Load Shader Group %s"), *FilePath);
		return FALSE;
	}
	TArray<FString> Keys;
	FileContents.ParseIntoArray(&Keys, TEXT("\r\n"), TRUE);

	for ( INT KeyIndex=0; KeyIndex < Keys.Num(); ++KeyIndex )
	{
		FProgramKey ShaderKey( Keys(KeyIndex) );
		ShaderGroup.AddItem( ShaderKey );
	}
	return TRUE;
}

void FMobileShaderInitialization::LoadAllShaderGroups()
{
	if (bAllShaderGroupsLoaded)
	{
		return;
	}

	bAllShaderGroupsLoaded = TRUE;

	ShaderGroupPackages.Empty();
	PendingShaderGroups.Empty();

	FString CookedDirectory;
	appGetCookedContentPath(appGetPlatformType(), CookedDirectory);

	// Read shader group settings from the ini file
	GConfig->Parse1ToNSectionOfNames(TEXT("Engine.MobileShaderGroups"), TEXT("ShaderGroup"), TEXT("Package"), ShaderGroupPackages, GEngineIni);

	FString ShaderGroupBinFile = CookedDirectory + TEXT("ShaderGroups.bin");
	FArchive * ShaderBinFile = GFileManager->CreateFileReader(*ShaderGroupBinFile);

	if (ShaderBinFile)
	{
		INT FileSize = ShaderBinFile->TotalSize();
		void * Contents = appMalloc(FileSize);
		ShaderBinFile->Serialize(Contents, FileSize);
		delete ShaderBinFile;
		FBufferReader Ar(Contents, FileSize, TRUE, FALSE);


		do
		{
			FString GroupName;
			Ar <<  GroupName;
			INT NumShaders;
			Ar <<  NumShaders;
			TArray<FProgramKey> ShaderProgramKeysArray;

			for ( INT KeyIndex=0; KeyIndex < NumShaders; ++KeyIndex )
			{
				FProgramKey Key;
				Ar << Key;
				ShaderProgramKeysArray.AddItem(Key);
			}

			PendingShaderGroups.Set(*GroupName, ShaderProgramKeysArray);

			if (!ShaderGroupPackages.Find(*GroupName))
			{
				TArray<FName> PackageList;
				PackageList.AddItem(*GroupName);
				ShaderGroupPackages.Set(*GroupName, PackageList);
			}

		} while (!Ar.AtEnd() && !Ar.GetError());
	}
	else
	{
		warnf(TEXT("Failed to load ShaderGroups.bin"));
	}

}

void FMobileShaderInitialization::LoadCachedShaderKeys()
{
#if WITH_MOBILE_RHI
	if (bCachedProgramKeysLoaded)
	{
		return;
	}

	bCachedProgramKeysLoaded = TRUE;
	// the shaders are in the cooked directory
	FString CookedDirectory;
	appGetCookedContentPath(appGetPlatformType(), CookedDirectory);

#if _WINDOWS
	// If we're caching program keys during this run, clean out the directory
	if( GSystemSettings.bCachePreprocessedShaders )
	{
		TArray<FString> FoundDirectoryNames;
		FString CachedProgramsDirectoryName = CookedDirectory + TEXT("Shaders\\");
		GFileManager->FindFiles(FoundDirectoryNames, *(CachedProgramsDirectoryName + TEXT("*")), FALSE, TRUE );
		for( INT DirIndex = 0; DirIndex < FoundDirectoryNames.Num(); DirIndex++ )
		{
			GFileManager->DeleteDirectory(*(CachedProgramsDirectoryName + FoundDirectoryNames(DirIndex)), TRUE, TRUE);
		}
	}
#endif
	
	//Create temp lookup map of used FProgramKeys
	LoadAllShaderGroups();
	TArray<FProgramKey> GroupedProgramKeys;

	// Add every unique key used by a predefined group to GroupedProgramKeys
	for ( TMap<FName, TArray<FProgramKey> >::TConstIterator It(PendingShaderGroups); It; ++It )
	{
		TArray<FProgramKey> * ShaderKeys = PendingShaderGroups.Find(It.Key());
		if (ShaderKeys)
		{
			for (INT ShaderKeyIndex = 0; ShaderKeyIndex < ShaderKeys->Num(); ShaderKeyIndex++)
			{
				FProgramKey ProgramKey = ShaderKeys->operator()(ShaderKeyIndex);
				GroupedProgramKeys.AddUniqueItem(ProgramKey);
			}
		}

	}
	
	//Create "Other" ShaderGroup, in the normal TMap
	TArray<FProgramKey> UngroupedProgramKeys;

	// If we want to use cached program keys, see if the program key cache exists, then open it and create the programs
	if( GSystemSettings.bUsePreprocessedShaders )
	{
		FString ProgramKeysFileName = CookedDirectory + TEXT("CachedProgramKeys.txt");
		FString ProgramKeysFileText;
		if( appLoadFileToString(ProgramKeysFileText, *ProgramKeysFileName) )
		{
			TArray<FString> Keys;

			// Extract the key set from the cache
			ProgramKeysFileText.ParseIntoArray(&Keys, TEXT("\r\n"), TRUE);
			if( Keys.Num() > 0 )
			{
				FString& VersionString = Keys(0);

				const FString VersionToken(TEXT("version:"));
				if( VersionString.StartsWith(VersionToken) )
				{
					INT ManifestVersion = appAtoi(*VersionString.Mid(VersionToken.Len()));
					if( ManifestVersion != SHADER_MANIFEST_VERSION )
					{
						warnf(NAME_Warning, TEXT("Shader manifest is an old version, all shaders will be compiled on-demand (causing hitches)."));
					}
					else
					{
						LoadShaderSource(Keys, GroupedProgramKeys, UngroupedProgramKeys);

						//Add NonGrouped Shaders into the "Ungrouped" ShaderGroup
						FName Ungrouped("Ungrouped");
						PendingShaderGroups.Set( Ungrouped, UngroupedProgramKeys );
					}

				}
			}
		}
	}
#endif
}


void FMobileShaderInitialization::LoadShaderSource(TArray<FString> & Keys, TArray<FProgramKey> & GroupedProgramKeys, TArray<FProgramKey> & UngroupedProgramKeys)
{
#if WITH_ES2_RHI
	// init the preprocessed shader loading
	void ES2StartLoadingPreprocessedShaderInfos();
	ES2StartLoadingPreprocessedShaderInfos();

	// Shader manifest is valid, proceed to load and cache the shader programs

	// Parse the program keys, looking for equivalence classes as we go
	const FString vse(TEXT("vse:"));
	const FString pse(TEXT("pse:"));
	for( INT ProgramKeyIndex = 1; ProgramKeyIndex < Keys.Num(); ProgramKeyIndex++ )
	{
		FString& ProgramKeyName = Keys(ProgramKeyIndex);

		UBOOL bIsVertexShaderEquivalence = ProgramKeyName.StartsWith(vse);
		UBOOL bIsPixelShaderEquivalence = ProgramKeyName.StartsWith(pse);

		// Check for an equivalence group
		if( bIsVertexShaderEquivalence || bIsPixelShaderEquivalence )
		{
			// We found a group, see if we have that sharing enabled
			if( !GSystemSettings.bShareShaderPrograms )
			{
				if( (bIsVertexShaderEquivalence && !GSystemSettings.bShareVertexShaders) ||
					(bIsPixelShaderEquivalence && !GSystemSettings.bSharePixelShaders) )
				{
					// Ignore because sharing isn't enabled
					continue;
				}
			}
#if _WINDOWS
			if( GSystemSettings.bCachePreprocessedShaders ||
				GSystemSettings.bProfilePreprocessedShaders )
			{
				// We want to force it skip any cache so it fully regenerates the equivalences
				continue;
			}
#endif
			// Decide which equivalence group to work with
			TMap<FProgramKey,FProgramKey>* KeyMap = bIsVertexShaderEquivalence ? &VertexKeyToEquivalentMasterKey : &PixelKeyToEquivalentMasterKey;

			// Extract the equivalence keys
			TArray<FString> EquivalentKeys;
			ProgramKeyName.Mid(4).ParseIntoArray(&EquivalentKeys, TEXT(","), TRUE);
			check(EquivalentKeys.Num() > 1);

			// The master key is used as the key for the entire equivalence class, arbitrarily the first one in the list
			FProgramKey MasterKey( EquivalentKeys(0) );

			// Ensure the master key is new, and set up the initial entry
			check(!KeyMap->Find(MasterKey));
			KeyMap->Set(MasterKey, MasterKey);

			// Walk through the equivalence list and add entries to the key map
			for( INT EquivalentKeyIndex = 1; EquivalentKeyIndex < EquivalentKeys.Num(); EquivalentKeyIndex++ )
			{
				FProgramKey EquivalentKey( EquivalentKeys(EquivalentKeyIndex) );

				// Ensure the key is new, and set up the equivalence entry
				check(!KeyMap->Find(EquivalentKey));
				KeyMap->Set(EquivalentKey, MasterKey);
			}
		}
		else
		{
			// This is a new program key that needs to be compiled
			FProgramKey ProgramKey( ProgramKeyName );
			
			// Check if it's part of an existing shader group, adding it to the UngroupedProgramKeys if it isn't
			if (!GroupedProgramKeys.ContainsItem(ProgramKey))
			{
				UngroupedProgramKeys.AddUniqueItem(ProgramKey);
			}
		}
	}
#endif
}

/**
 * Pauses game rendering while we're compiling mobile shaders on the rendering thread.
 * Stops the rendering thread when compiling finished, if it was temporarily turned on during compiling.
 */
void FMobileShaderInitialization::Tick()
{
#if WITH_ES2_RHI && MOBILESHADER_THREADED_INIT
	// Keep pausing game rendering while we're compiling mobile shaders on the rendering thread.
	if ( bPauseGameRendering )
	{
		FViewport::SetGameRenderingEnabled(FALSE);
	}

	// Is the rendering thread done compiling shaders?
	if ( CompletionFence && CompletionFence->GetNumPendingFences() == 0 )
	{
		if ( bTemporarilyEnableRenderthread )
		{
			StopRenderingThread();
			GUseThreadedRendering = FALSE;
			bTemporarilyEnableRenderthread = FALSE;
		}

		UBOOL bSimMobile = ParseParam(appCmdLine(),TEXT("SIMMOBILE")); 
		if (bSimMobile)
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND(
				WarmES2ShaderCacheCommand,
			{
				void WarmES2ShaderCache();
				WarmES2ShaderCache();
			});
		}

		delete CompletionFence;
		CompletionFence = NULL;
		bPauseGameRendering = FALSE;

		// Resume game rendering
		FViewport::SetGameRenderingEnabled(TRUE);

		CurrentState = MobileShaderInit_Finished;
	}

#endif
}

/**
 * Maps the GL shader to the pixel master key.  Allows an additional mapping for texture usage flags in Flash.
 */
GLuint* FMobileShaderInitialization::GetPixelShaderFromPixelMasterKey( const FProgramKey& ProgramKey, DWORD TextureUsageFlags)
{
#if FLASH
	// look up based on texture usage
	TMap<DWORD, GLuint>* TextureMap = PixelMasterKeyToGLShader.Find(ProgramKey);
	return TextureMap ? TextureMap->Find(TextureUsageFlags) : NULL;
#else
	return PixelMasterKeyToGLShader.Find(ProgramKey);
#endif
}

/**
 * Maps the GL shader to the pixel master key.  Allows an additional mapping for texture usage flags in Flash.
 */
void FMobileShaderInitialization::SetPixelShaderForPixelMasterKey(const FProgramKey& ProgramKey, DWORD TextureUsageFlags, GLuint Shader)
{
#if FLASH
	// set based on texture usage
	TMap<DWORD, GLuint>* TextureMap = PixelMasterKeyToGLShader.Find(ProgramKey);
	if (TextureMap == NULL)
	{
		TextureMap = &PixelMasterKeyToGLShader.Set(ProgramKey, TMap<DWORD, GLuint>());
	}
	TextureMap->Set(TextureUsageFlags, Shader);
#else
	PixelMasterKeyToGLShader.Set(ProgramKey, Shader);
#endif
}

/** Destructor. */
FMobileShaderInitialization::~FMobileShaderInitialization()
{
	if ( CompletionFence )
	{
		delete CompletionFence;
	}
}

/** Helper container for all information about ES2 shader startup initialization. */
FMobileShaderInitialization GMobileShaderInitialization;

