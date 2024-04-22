/*=============================================================================
UberPostProcessEffect.cpp: combines DOF, bloom and material coloring.

Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "UnMotionBlurEffect.h"
#include "BokehDOF.h"
#include "PostProcessAA.h"

IMPLEMENT_CLASS(UUberPostProcessEffect);

/**
* Called when properties change.
*/
void UUberPostProcessEffect::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// clamp to 0..1
	SceneDesaturation = Clamp(SceneDesaturation, 0.0f, 1.0f);

	// UberPostProcessEffect should only ever exists in the SDPG_PostProcess scene
	SceneDPG=SDPG_PostProcess;

	Super::PostEditChangeProperty(PropertyChangedEvent);
}	

/**
* Called after this instance has been serialized.
*/
void UUberPostProcessEffect::PostLoad()
{
	Super::PostLoad();

	// UberPostProcessEffect should only ever exists in the SDPG_PostProcess scene
	SceneDPG=SDPG_PostProcess;

	// clamp desaturation to 0..1 (fixup for old data)
	SceneDesaturation = Clamp(SceneDesaturation, 0.f, 1.f);

	ULinkerLoad* LFLinkerLoad = GetLinker();
	if(LFLinkerLoad && (LFLinkerLoad->Ver() < VER_TONEMAPPER_ENUM))
	{
		TonemapperType = bEnableHDRTonemapper_DEPRECATED ? Tonemapper_Filmic : Tonemapper_Off;
		TonemapperScale = SceneHDRTonemapperScale_DEPRECATED;
	}
}




/*-----------------------------------------------------------------------------
FUberPostProcessBlendPixelShader
-----------------------------------------------------------------------------*/

/** 
 * Encapsulates the blend pixel shader.
 */
class FUberPostProcessBlendPixelShaderBase : public FDOFAndBloomBlendPixelShader
{
protected:
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment, UINT DepthOfFieldFullResMode, UINT TemplTonemapperType, UBOOL bUseImageGrain, UBOOL bSeparateTranslucency, UBOOL bTemporalAA)
	{
		OutEnvironment.Definitions.Set(TEXT("DOF_FULLRES_MODE"),*FString::Printf(TEXT("%u"), DepthOfFieldFullResMode));
		OutEnvironment.Definitions.Set(TEXT("USE_IMAGEGRAIN"),bUseImageGrain ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("USE_TONEMAPPERTYPE"),*FString::Printf(TEXT("%u"), TemplTonemapperType));
		OutEnvironment.Definitions.Set(TEXT("USE_SEPARATE_TRANSLUCENCY"), bSeparateTranslucency ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("USE_TEMPORAL_AA"),bTemporalAA ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	FUberPostProcessBlendPixelShaderBase() {}

public:
	FShaderResourceParameter		LowResPostProcessBuffer;
	FShaderParameter				ImageAdjustments1Parameter;
	FShaderParameter				ImageAdjustments2Parameter;
	FShaderParameter				ImageAdjustments3Parameter;
	FShaderResourceParameter		ColorGradingLUTTextureParameter;
	FShaderParameter				HalfResMaskRectParameter;
	FShaderResourceParameter		BokehDOFLayerTextureParameter;
	FMotionBlurShaderParameters		MotionBlurShaderParameters;
	FShaderResourceParameter		NoiseTextureParameter;

	/** Initialization constructor. */
	FUberPostProcessBlendPixelShaderBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FDOFAndBloomBlendPixelShader(Initializer)
		,	MotionBlurShaderParameters(Initializer.ParameterMap)
	{
		LowResPostProcessBuffer.Bind(Initializer.ParameterMap, TEXT("LowResPostProcessBuffer"), TRUE );
		ImageAdjustments1Parameter.Bind(Initializer.ParameterMap,TEXT("ImageAdjustments1"),TRUE);
		ImageAdjustments2Parameter.Bind(Initializer.ParameterMap,TEXT("ImageAdjustments2"),TRUE);
		ImageAdjustments3Parameter.Bind(Initializer.ParameterMap,TEXT("ImageAdjustments3"),TRUE);
		ColorGradingLUTTextureParameter.Bind(Initializer.ParameterMap,TEXT("ColorGradingLUT"), TRUE);
		HalfResMaskRectParameter.Bind(Initializer.ParameterMap,TEXT("HalfResMaskRect"),TRUE);
		BokehDOFLayerTextureParameter.Bind(Initializer.ParameterMap, TEXT("BokehDOFLayerTexture"), TRUE);
		NoiseTextureParameter.Bind(Initializer.ParameterMap,TEXT("NoiseTexture"),TRUE);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("UberPostProcessBlendPixelShader");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("Main");
	}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FDOFAndBloomBlendPixelShader::Serialize(Ar);
		Ar	<< LowResPostProcessBuffer << ImageAdjustments1Parameter << ImageAdjustments2Parameter
			<< ImageAdjustments3Parameter << ColorGradingLUTTextureParameter << HalfResMaskRectParameter
			<< BokehDOFLayerTextureParameter << MotionBlurShaderParameters
			<< NoiseTextureParameter;

		ImageAdjustments1Parameter.SetShaderParamName(TEXT("ImageAdjustments1"));
		ImageAdjustments2Parameter.SetShaderParamName(TEXT("ImageAdjustments2"));
		ImageAdjustments3Parameter.SetShaderParamName(TEXT("ImageAdjustments3"));
		HalfResMaskRectParameter.SetShaderParamName(TEXT("HalfResMaskRect"));

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			// Hard-coded to match the shader (it's serialized based on the PC shader, not the NGP shader).
			SceneTextureParameters.SceneColorTextureParameter.SetBaseIndex(0, TRUE);
			ColorGradingLUTTextureParameter.SetBaseIndex( 2 );
		}
#endif

		return bShaderHasOutdatedParameters;
	}
};

/** 
 * Permutations of the blend pixel shader.
 * @param DepthOfFieldFullResMode 0:off 1:ReferenceDOF 2:BokehDOF
 * @param TemplTonemapperType see "TonemapperType" console command, is never -1
 * @param bUseImageGrain whether to enable image grain
 * @param bSeparateTranslucency whether to enable separate translucency
 * @param bTemporalAA whether to enable temporal AA
 */
template<UINT DepthOfFieldFullResMode, UINT TemplTonemapperType, UBOOL bUseImageGrain, UBOOL bSeparateTranslucency, UBOOL bTemporalAA>
class TUberPostProcessBlendPixelShader : public FUberPostProcessBlendPixelShaderBase
{
	DECLARE_SHADER_TYPE( TUberPostProcessBlendPixelShader, Global );

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FUberPostProcessBlendPixelShaderBase::ModifyCompilationEnvironment( Platform, OutEnvironment, DepthOfFieldFullResMode, TemplTonemapperType, bUseImageGrain, bSeparateTranslucency, bTemporalAA );
	}

protected:
	/** Default constructor. */
	TUberPostProcessBlendPixelShader() {}

public:
	/** Initialization constructor. */
	TUberPostProcessBlendPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FUberPostProcessBlendPixelShaderBase(Initializer)
	{
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A)		VARIATION2(A,0)			VARIATION2(A,1)			VARIATION2(A,2)
#define VARIATION2(A,B)		VARIATION3(A,B,0)		VARIATION3(A,B,1)
#define VARIATION3(A,B,C)	VARIATION4(A,B,C,0)		VARIATION4(A,B,C,1)
#define VARIATION4(A,B,C,D)	VARIATION5(A,B,C,D,0)	VARIATION5(A,B,C,D,1)
#define VARIATION5(A, B, C, D, E) typedef TUberPostProcessBlendPixelShader<A, B, C, D, E> FUberPostProcessBlendPixelShader##A##B##C##D##E; \
	IMPLEMENT_SHADER_TYPE2(template<>, FUberPostProcessBlendPixelShader##A##B##C##D##E, SF_Pixel, VER_BLOOM_AFTER_MOTIONBLUR, 0);

	VARIATION1(0) VARIATION1(1) VARIATION1(2)

#undef VARIATION1
#undef VARIATION2
#undef VARIATION3
#undef VARIATION4
#undef VARIATION5



/** Encapsulates the motion blur pixel shader for the uber postprocess. */
class FUberHalfResPixelShaderBase : public FDOFAndBloomBlendPixelShader
{
protected:
	static UBOOL ShouldCache(EShaderPlatform Platform, UBOOL bTMotionBlur, UBOOL bUseDoFBlurBuffer, UBOOL bUseSoftEdgeMotionBlur)
	{
		// Blur buffer currently only supported on 360.
		if(bUseDoFBlurBuffer)
		{
			return Platform == SP_XBOXD3D;
		}

		if(!bTMotionBlur && bUseSoftEdgeMotionBlur)
		{
			// bUseSoftEdgeMotionBlur without bMotionBlur is useless
			return FALSE;
		}

		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment, UBOOL bTMotionBlur, UBOOL bUseDoFBlurBuffer, UINT DepthOfFieldHalfResMode, UBOOL bUseSoftEdgeMotionBlur)
	{
		OutEnvironment.Definitions.Set(TEXT("MOTION_BLUR"),bTMotionBlur ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("USE_DOF_BLUR_BUFFER"),bUseDoFBlurBuffer ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("DOF_HALFRES_MODE"), *FString::Printf(TEXT("%u"), DepthOfFieldHalfResMode));
		OutEnvironment.Definitions.Set(TEXT("USE_SOFTEDGE_MOTIONBLUR"),bUseSoftEdgeMotionBlur ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	FUberHalfResPixelShaderBase() {}

public:
	FMotionBlurShaderParameters		MotionBlurParameters;
	FShaderResourceParameter		LowResPostProcessBufferPointParameter;
	FShaderResourceParameter		BokehDOFLayerTextureParameter;

	/** Initialization constructor. */
	FUberHalfResPixelShaderBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FDOFAndBloomBlendPixelShader(Initializer)
		,	MotionBlurParameters(Initializer.ParameterMap)
	{
		LowResPostProcessBufferPointParameter.Bind(Initializer.ParameterMap, TEXT("LowResPostProcessBufferPoint"), TRUE );
		BokehDOFLayerTextureParameter.Bind(Initializer.ParameterMap, TEXT("BokehDOFLayerTexture"), TRUE);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("UberPostProcessBlendPixelShader");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("UberHalfResMain");
	}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FDOFAndBloomBlendPixelShader::Serialize(Ar);
		Ar << MotionBlurParameters << LowResPostProcessBufferPointParameter << BokehDOFLayerTextureParameter;
		return bShaderHasOutdatedParameters;
	}
};

/** Permutations of the motion blur pixel shader for the uber postprocess. */
template<UBOOL bTMotionBlur, UBOOL bUseDoFBlurBuffer, UINT DepthOfFieldHalfResMode, UBOOL bUseSoftEdgeMotionBlur>
class TUberHalfResPixelShader : public FUberHalfResPixelShaderBase
{
	DECLARE_SHADER_TYPE( TUberHalfResPixelShader, Global );

	static UBOOL ShouldCache( EShaderPlatform Platform )
	{
		return FUberHalfResPixelShaderBase::ShouldCache( Platform, bTMotionBlur, bUseDoFBlurBuffer, bUseSoftEdgeMotionBlur );
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment ) 
	{
		FUberHalfResPixelShaderBase::ModifyCompilationEnvironment( Platform, OutEnvironment, bTMotionBlur, bUseDoFBlurBuffer, DepthOfFieldHalfResMode, bUseSoftEdgeMotionBlur );
	}

protected:
	/** Default constructor. */
	TUberHalfResPixelShader() {}

public:
	/** Initialization constructor. */
	TUberHalfResPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FUberHalfResPixelShaderBase(Initializer)
	{
	}
};

// #define avoids a lot of code duplication
#define VARIATION(bA, bB, C, bD) typedef TUberHalfResPixelShader<bA, bB, C, bD> FUberHalfResPixelShader##bA##bB##C##bD; \
	IMPLEMENT_SHADER_TYPE2(template<>, FUberHalfResPixelShader##bA##bB##C##bD, SF_Pixel, VER_DWORD_SKELETAL_MESH_INDICES_FIXUP, 0);
VARIATION(0,0,0,0) VARIATION(0,0,0,1) VARIATION(0,0,1,0) VARIATION(0,0,1,1)
VARIATION(0,0,2,0) VARIATION(0,0,2,1)
VARIATION(0,1,0,0) VARIATION(0,1,0,1) VARIATION(0,1,1,0) VARIATION(0,1,1,1)
VARIATION(0,1,2,0) VARIATION(0,1,2,1)
VARIATION(1,0,0,0) VARIATION(1,0,0,1) VARIATION(1,0,1,0) VARIATION(1,0,1,1)
VARIATION(1,0,2,0) VARIATION(1,0,2,1)
VARIATION(1,1,0,0) VARIATION(1,1,0,1) VARIATION(1,1,1,0) VARIATION(1,1,1,1)
VARIATION(1,1,2,0) VARIATION(1,1,2,1)
//
#undef VARIATION

/*-----------------------------------------------------------------------------
FUberPostProcessVertexShader
-----------------------------------------------------------------------------*/

/** Encapsulates the UberPostProcess vertex shader. */
class FUberPostProcessVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FUberPostProcessVertexShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	/** Default constructor. */
	FUberPostProcessVertexShader() {}
	
	FUberPostProcessVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		SceneCoordinate1ScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("SceneCoordinate1ScaleBias"),TRUE);
		SceneCoordinate2ScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("SceneCoordinate2ScaleBias"),TRUE);
		SceneCoordinate3ScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("SceneCoordinate3ScaleBias"),TRUE);
	}

	UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneCoordinate1ScaleBiasParameter << SceneCoordinate2ScaleBiasParameter << SceneCoordinate3ScaleBiasParameter;

		SceneCoordinate1ScaleBiasParameter.SetShaderParamName(TEXT("SceneCoordinate1ScaleBias"));
		SceneCoordinate2ScaleBiasParameter.SetShaderParamName(TEXT("SceneCoordinate2ScaleBias"));
		SceneCoordinate3ScaleBiasParameter.SetShaderParamName(TEXT("SceneCoordinate3ScaleBias"));
		return bShaderHasOutdatedParameters;
	}

public:
	FShaderParameter SceneCoordinate1ScaleBiasParameter;
	FShaderParameter SceneCoordinate2ScaleBiasParameter;
	FShaderParameter SceneCoordinate3ScaleBiasParameter;
};

IMPLEMENT_SHADER_TYPE(,FUberPostProcessVertexShader,TEXT("UberPostProcessVertexShader"),TEXT("Main"),SF_Vertex,VER_SOFTEDGEMOTIONBLUR,0);


// y = TonemapperAFunc(x)
// computes y for a given x
static FLOAT TonemapperAFunc(FLOAT x, FLOAT A, FLOAT B)
{
	return x / (x + A) * B;
}
// y = TonemapperAFunc(x)
// computes x for a given y
//static FLOAT InverseTonemapperAFunc(FLOAT y, FLOAT A, FLOAT B)
//{
//	return y * A / (B - y);
//}
// dy = TonemapperFunc(x) derived
// computes dy for a given x
static FLOAT TonemapperADerivedFunc(FLOAT x, FLOAT A, FLOAT B)
{
	return A / Square(x + A) * B;
}
// y = TonemapperFunc(x) derived
// computes x for a given dy
static FLOAT InverseTonemapperADerivedFunc(FLOAT dy, FLOAT A, FLOAT B)
{
	return appSqrt(A * B / dy) - A;
}
 
/*-----------------------------------------------------------------------------
FUberPostProcessSceneProxy
-----------------------------------------------------------------------------*/

extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

class FUberPostProcessSceneProxy : public FDOFAndBloomPostProcessSceneProxy
{
public:
	/** 
	* Initialization constructor. 
	* @param InEffect - Uber post process effect to mirror in this proxy, must not be 0
	* @param WorldSettings can be 0
	* @param InTonemapperType see "TonemapperType" console command, is never -1
	*/
	FUberPostProcessSceneProxy(const UUberPostProcessEffect* InEffect,const FPostProcessSettings* WorldSettings, UINT InColorGradingCVar, UINT InTonemapperType, UBOOL bInMotionBlur, UBOOL bInImageGrain)
		:	FDOFAndBloomPostProcessSceneProxy(InEffect, WorldSettings)
		,	MotionBlurSoftEdgeKernelSize(InEffect->MotionBlurSoftEdgeKernelSize)
		,	TonemapperToeFactor(InEffect->TonemapperToeFactor)
		,	TonemapperRange(InEffect->TonemapperRange)
		,	ColorGradingCVar(InColorGradingCVar)
		,	TonemapperType(InTonemapperType)
		,	bMotionBlur(bInMotionBlur)
		,	bScaleEffectsWithViewSize(InEffect->bScaleEffectsWithViewSize)
		,	bEnableSceneEffect( TRUE )
		,	PostProcessAA(InEffect, WorldSettings)
	{
		checkSlow(IsInGameThread());
		check(InEffect);

		SET_POSTPROCESS_PROPERTY1(Scene, TonemapperScale);
		SET_POSTPROCESS_PROPERTY2(Scene, ImageGrainScale);

		ColorTransform.Shadows = GET_POSTPROCESS_PROPERTY2(Scene, Shadows);
		ColorTransform.HighLights = GET_POSTPROCESS_PROPERTY2(Scene, HighLights);
		ColorTransform.MidTones = GET_POSTPROCESS_PROPERTY2(Scene, MidTones);
		ColorTransform.Desaturation = GET_POSTPROCESS_PROPERTY2(Scene, Desaturation);
		ColorTransform.Colorize = GET_POSTPROCESS_PROPERTY2(Scene, Colorize);

		MotionBlurParams.RotationThreshold =	GET_POSTPROCESS_PROPERTY1(MotionBlur, CameraRotationThreshold);
		MotionBlurParams.TranslationThreshold = GET_POSTPROCESS_PROPERTY1(MotionBlur, CameraTranslationThreshold);
		MotionBlurParams.MaxVelocity =			GET_POSTPROCESS_PROPERTY1(MotionBlur, MaxVelocity);
		MotionBlurParams.MotionBlurAmount =		GET_POSTPROCESS_PROPERTY2(MotionBlur, Amount);

		MotionBlurParams.bFullMotionBlur = InEffect->FullMotionBlur;

		if(WorldSettings)
		{
			if(WorldSettings->bOverride_MotionBlur_FullMotionBlur)
			{
				MotionBlurParams.bFullMotionBlur = WorldSettings->MotionBlur_FullMotionBlur;
			}

			if( WorldSettings->bOverride_EnableSceneEffect )
			{
				bEnableSceneEffect = WorldSettings->bEnableSceneEffect;
			}
		}

		// Compute weights from user settings
		{
			FLOAT Small = Max(InEffect->BloomWeightSmall, 0.0f);
			FLOAT Medium = Max(InEffect->BloomWeightMedium, 0.01f);
			FLOAT Large = Max(InEffect->BloomWeightLarge, 0.0f);

			// normalize weights
			{
				FLOAT Inv = 1.0f / (Small + Medium + Large);

				BloomWeightSmall = Small * Inv;
				BloomWeightLarge = Large * Inv;
			}

			checkSlow(BloomWeightSmall >= 0.0f && BloomWeightLarge <= 1.0f);
			checkSlow(BloomWeightLarge >= 0.0f && BloomWeightLarge <= 1.0f);
		}

		// console command override
		if(GBloomWeightSmall >= 0)
		{
			BloomWeightSmall = GBloomWeightSmall;
		}

		// console command override
		if(GBloomWeightLarge >= 0)
		{
			BloomWeightLarge = GBloomWeightLarge;
		}

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			// currently, mobile only works with bloom type 3
			// @todo, investigate why only 3 works
			BloomType = 3;
		}
		else
#endif
		// find best BloomType 
		{
			// 1 Gaussian bell curve unless we change it in the following code
			BloomType = 3;

			// we pick the method depending on the specified weights
			if(BloomWeightLarge > 0.01f)
			{
				// 3 Gaussian bell curves combined
				BloomType = 2;
			}
			else
			{
				if(BloomWeightSmall > 0.01f)
				{
					// 2 Gaussian bell curves combined, inner radius reduced
					BloomType = 1;
				}
			}

			// console variable override
			{
				static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("BloomType")); 
				INT Value = CVar ? CVar->GetInt() : -1;

				if(Value >= 0)
				{
					BloomType = Value;
				}
			}
		}

		checkSlow(BloomType >= 0);

		BloomQuality = 1;

		if(!bInImageGrain)
		{
			SceneImageGrainScale = 0.0f;
		}

#if !FINAL_RELEASE
		{
			// console variable override
			static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("BloomSize")); 
			FLOAT Value = CVar->GetFloat();

			if(Value >= 0.0f)
			{
				BlurBloomKernelSize = Value;
			}
		}

		{
			// console variable override
			static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("BloomThreshold")); 
			FLOAT Value = CVar->GetFloat();

			if(Value >= 0.0f)
			{
				BloomThreshold = Value;
			}
		}

		{
			// console variable override
			static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("BloomScale")); 
			FLOAT Value = CVar->GetFloat();

			if(Value >= 0.0f)
			{
				BloomScale = Value;
			}
		}

		{
			// console variable override
			static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("MotionBlurSoftEdge")); 
			FLOAT Value = CVar->GetFloat();

			if(Value >= 0.0f)
			{
				MotionBlurSoftEdgeKernelSize = Value;
			}
		}

		{
			// console variable override
			static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("MotionBlurMaxVelocity")); 
			FLOAT Value = CVar->GetFloat();

			if(Value >= 0.0f)
			{
				MotionBlurParams.MaxVelocity = Value;
			}
		}

		{
			// console variable override
			static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("MotionBlurAmount")); 
			FLOAT Value = CVar->GetFloat();

			if(Value >= 0.0f)
			{
				MotionBlurParams.MotionBlurAmount = Value;
			}
		}

		{
			// console variable override
			static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("BloomQuality")); 
			INT Value = CVar->GetInt();

			if(Value >= 0)
			{
				BloomQuality = Value;
			}
		}

		{
			// console variable override
			static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("ImageGrain")); 
			FLOAT Value = CVar->GetFloat();

			if(Value >= 0.0f)
			{
				SceneImageGrainScale = Value;
			}
		}
#endif // !FINAL_RELEASE

		// ensure that desat values out of range get clamped (this can happen in the editor currently)
		ColorTransform.Desaturation = Clamp(ColorTransform.Desaturation, 0.f, 1.f);

		// blend multiple LUT to produce final LUT (3D color lookup texture) for color grading
		if(WorldSettings && ColorGradingCVar != 0)
		{
			if(!WorldSettings->ColorGradingLUT.IsLUTEmpty())
			{
				// game filled out the table (soft transitions)
				WorldSettings->ColorGradingLUT.CopyToRenderThread(ColorGradingBlenderRTCopy);
			}
			else
			{
				// game wasn't filling the table so we set the editor LUT (hard switch)
				FLUTBlender LocalColorGradingLUT;

				LocalColorGradingLUT.Reset();

				if(ColorGrading_LookupTable)
				{
					LocalColorGradingLUT.LerpTo(ColorGrading_LookupTable, 1.0f);
				}

				LocalColorGradingLUT.CopyToRenderThread(ColorGradingBlenderRTCopy);
			}
		}

		extern INT GMotionBlurFullMotionBlur;
		MotionBlurParams.bFullMotionBlur = GMotionBlurFullMotionBlur < 0 ? MotionBlurParams.bFullMotionBlur : (GMotionBlurFullMotionBlur > 0);

		const FLOAT MinRotationThreshold=5.0f;
		MotionBlurParams.RotationThreshold = Max<FLOAT>(MinRotationThreshold, MotionBlurParams.RotationThreshold);
		const FLOAT MinTranslationThreshold=10.0f;
		MotionBlurParams.TranslationThreshold = Max<FLOAT>(MinTranslationThreshold, MotionBlurParams.TranslationThreshold);

		if(WorldSettings && !WorldSettings->bEnableSceneEffect)
		{
			ColorTransform.Reset();
		}

		// compute bloom radius within reasonable ranges
		// is done in full filter buffer resolution (supports up to 0..64 kernel size)
		BloomKernelSizeSmall = BlurBloomKernelSize * Clamp(InEffect->BloomSizeScaleSmall, 0.0f, 1.0f);
		// is done in half filter buffer resolution (scale is limited by 2 so it still support 64 kernel size)
		BloomKernelSizeMedium = BlurBloomKernelSize * Clamp(InEffect->BloomSizeScaleMedium, 0.25f, 2.0f);
		// is done in quarter filter buffer resolution (scale should be limited by 4 so it still support 64 kernel size,
		// however if feels better to have this range not as tightly clamped so we allow 8 and hope the clamped kernel
		// size not an issue.
		BloomKernelSizeLarge = BlurBloomKernelSize * Clamp(InEffect->BloomSizeScaleLarge, 1.0f, 8.0f);
	}

	/**
	 * Selects the correct shaders and renders the half-res variation of the uber postprocess.
	 * @param View - current view.
	 * @param LocalDepthOfFieldType - the type of depth-of-field.
	 * @param EffectSizeScale - amount by which to scale the radius of the effect.
	 */
	template<UBOOL bUseDoFBlurBuffer, UBOOL bUseSoftEdgeMotionBlur, UINT DepthOfFieldHalfResMode, UBOOL bTMotionBlur>
	void RenderVariationHalfRes(FViewInfo& View, EDOFType LocalDepthOfFieldType, FLOAT EffectSizeScale, UBOOL bSeparateTranslucency)
	{
		RenderVariationHalfRes_DoFAndMotionBlur( View, LocalDepthOfFieldType, EffectSizeScale, bUseSoftEdgeMotionBlur, bSeparateTranslucency );

		static FGlobalBoundShaderState BoundShaderState;
		TShaderMapRef<FUberPostProcessVertexShader> BlendVertexShader(GetGlobalShaderMap());
		TShaderMapRef<TUberHalfResPixelShader<bTMotionBlur, bUseDoFBlurBuffer, DepthOfFieldHalfResMode, bUseSoftEdgeMotionBlur> > BlendPixelShader(GetGlobalShaderMap());
		SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *BlendVertexShader, *BlendPixelShader, sizeof(FFilterVertex));
		RenderVariationHalfRes( View, LocalDepthOfFieldType, bUseDoFBlurBuffer, *BlendVertexShader, *BlendPixelShader );
	}

	/**
	 * Renders the half-res depth-of-field and motion blur passes.
	 * @param View - current view.
	 * @param LocalDepthOfFieldType - the type of depth-of-field.
	 * @param EffectSizeScale - amount by which to scale the radius of the effect.
	 * @param bUseSoftEdgeMotionBlur - whether to enable soft edge motion blur.
	 * @param bSeparateTranslucency - ???
	 */
	void RenderVariationHalfRes_DoFAndMotionBlur(
		FViewInfo& View,
		EDOFType LocalDepthOfFieldType,
		FLOAT EffectSizeScale,
		UBOOL bUseSoftEdgeMotionBlur,
		UBOOL bSeparateTranslucency)
	{
		// full resolution (this can be much bigger than the viewport needed size, half and quarter is computed from that size)
		const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
		const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();
		const UINT FilterDownsampleFactor = GSceneRenderTargets.GetFilterDownsampleFactor();
		// quarter resolution of the view without border
		const UINT DownsampledSizeX = View.RenderTargetSizeX / FilterDownsampleFactor;
		const UINT DownsampledSizeY = View.RenderTargetSizeY / FilterDownsampleFactor;

		FVector2D SampleMaskMin(0 / (FLOAT)BufferSizeX, 0 / (FLOAT)BufferSizeY);
		FVector2D SampleMaskMaxA((0 + View.SizeX - 1) / (FLOAT)BufferSizeX, (0 + View.SizeY - 1) / (FLOAT)BufferSizeY);

		// Depth Of Field
		if(LocalDepthOfFieldType == DOFType_SimpleDOF)
		{
			SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("DOF"));

			//Use default quality param so we can add the depth of field quality param
			UINT Quality = 0;

			//if high quality, only render foreground.  If other quality, render both background and foreground
			//foreground should not clamp the min depth
			MinDepth = -MAX_FLT;
			MaxDepth = MAX_FLT;
			// render quarter res DepthOfField
			RenderGatherPass(View, EGD_DepthOfField, SRTI_FilterColor0, 1.0f, Quality);
			GaussianBlurFilterBuffer(View,View.SizeX, DownsampledSizeX + AntiLeakBorder, DownsampledSizeY + AntiLeakBorder, BlurKernelSize * EffectSizeScale, 1.0f, SRTI_FilterColor0, SampleMaskMin, SampleMaskMaxA);
			// SRTI_FilterColor0 now has the full scene/foreground DOF scene content
		}

		// SoftEdge MotionBlur
		if(bUseSoftEdgeMotionBlur)
		{
			SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("SoftEdgeMotionBlur"));

			// render quarter res MotionBlur vector
			RenderGatherPass(View, EGD_MotionBlur, SRTI_FilterColor2, 1.0f);
			GaussianBlurFilterBuffer(View,View.SizeX, DownsampledSizeX + AntiLeakBorder, DownsampledSizeY + AntiLeakBorder, MotionBlurSoftEdgeKernelSize, 1.0f, SRTI_FilterColor2, SampleMaskMin, SampleMaskMaxA);

			// SRTI_FilterColor2 now has the blurred velocity vector content
		}

		// on some hardware TranslucencyBuffer and HalfResPostProcess overlaps so if we debug TranslucencyBuffer we don't draw that pass
#if WITH_D3D11_TESSELLATION
		extern INT GVisualizeTexture;
		if((GVisualizeTexture - 1) != (INT)TranslucencyBuffer)
		{
			if(LocalDepthOfFieldType == DOFType_BokehDOF)
			{
				SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("BokehDOF"));

				FBokehDOFRenderer BokehDOFRenderer;

				BokehDOFRenderer.RenderBokehDOF(View,
					View.DepthOfFieldParams,
					BlurKernelSize * EffectSizeScale,
					DepthOfFieldQuality,
					BokehTexture,
					bSeparateTranslucency);
			}
		}
#endif // WITH_D3D11_TESSELLATION
	}

	/**
	* Render the half-res variation of the post process effect.
	* @param View - current view.
	* @param LocalDepthOfFieldType - the type of depth-of-field.
	* @param bUseDoFBlurBuffer - whether to use the depth-of-field blur buffer.
	* @param BlendVertexShader - the vertex shader with which to render.
	* @param BlendPixelShader - the pixel shader with which to render.
	*/
	void RenderVariationHalfRes(
		FViewInfo& View,
		EDOFType LocalDepthOfFieldType,
		UBOOL bUseDoFBlurBuffer,
		FUberPostProcessVertexShader* BlendVertexShader,
		FUberHalfResPixelShaderBase* BlendPixelShader )
	{
		// full resolution (this can be much bigger than the viewport needed size, half and quarter is computed from that size)
		const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
		const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();
		// quarter resolution +2 pixel border
		const UINT FilterBufferSizeX = GSceneRenderTargets.GetFilterBufferSizeX();
		const UINT FilterBufferSizeY = GSceneRenderTargets.GetFilterBufferSizeY();
		const UINT FilterDownsampleFactor = GSceneRenderTargets.GetFilterDownsampleFactor();
		// half resolution
		const UINT HalfSizeX = BufferSizeX / 2;
		const UINT HalfSizeY = BufferSizeY / 2;
		// quarter resolution
		const UINT QuarterSizeX = BufferSizeX / FilterDownsampleFactor;
		const UINT QuarterSizeY = BufferSizeY / FilterDownsampleFactor;

		// on some hardware TranslucencyBuffer and HalfResPostProcess overlaps so if we debug TranslucencyBuffer we don't draw that pass
		extern INT GVisualizeTexture;
		if((GVisualizeTexture - 1) != (INT)TranslucencyBuffer)
		{
			SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("UberHalfRes"));

			RHISetRenderTarget(GSceneRenderTargets.GetHalfResPostProcessSurface(), 0);

			// Set the DOF and bloom parameters
			BlendPixelShader->DOFParameters.SetPS(BlendPixelShader, View.DepthOfFieldParams);

			BlendPixelShader->PostProcessParameters.SetPS(BlendPixelShader, BlurKernelSize, DOFOcclusionTweak);

			BlendPixelShader->MotionBlurParameters.Set(
				BlendPixelShader->GetPixelShader(),
				View,
				MotionBlurParams);

			// Setup DoF blur buffer
			if(bUseDoFBlurBuffer)
			{
				SetTextureParameterDirectly(
					BlendPixelShader->GetPixelShader(),
					BlendPixelShader->DoFBlurBufferParameter,
					TStaticSamplerState<SF_Bilinear>::GetRHI(),
					GSceneRenderTargets.GetDoFBlurBufferTexture()
					);
			}

			// Setup the scene texture
			BlendPixelShader->SceneTextureParameters.Set(&View, BlendPixelShader, SF_Point);

			// half res scene point samples
			SetTextureParameterDirectly(
				BlendPixelShader->GetPixelShader(),
				BlendPixelShader->LowResPostProcessBufferPointParameter, 
				TStaticSamplerState<SF_Point>::GetRHI(),
				GSceneRenderTargets.GetTranslucencyBufferTexture() );

			if(LocalDepthOfFieldType == DOFType_BokehDOF)
			{
				SetTextureParameter(
					BlendPixelShader->GetPixelShader(),
					BlendPixelShader->BokehDOFLayerTextureParameter, 
					TStaticSamplerState<SF_Point>::GetRHI(),
					GSceneRenderTargets.GetBokehDOFTexture() );
			}

			FVector4 SceneCoordinate1ScaleBias(
				0.5f * HalfSizeX,
				-0.5f * HalfSizeY,
				0.5f * HalfSizeY + GPixelCenterOffset - View.RenderTargetY / 2,
				0.5f * HalfSizeX + GPixelCenterOffset - View.RenderTargetX / 2);

			// compute pixel position from screen pos, unify D3D9 half texel shift (done like this for cleaner code), flip y
			SetVertexShaderValue(BlendVertexShader->GetVertexShader(), BlendVertexShader->SceneCoordinate1ScaleBiasParameter, SceneCoordinate1ScaleBias);

			// used to scale to UV half res
			SetVertexShaderValue(BlendVertexShader->GetVertexShader(), BlendVertexShader->SceneCoordinate2ScaleBiasParameter,
				FVector4(
					1.0f / HalfSizeX,
					1.0f / HalfSizeY,
					0.5f + GPixelCenterOffset / HalfSizeY + SceneCoordinate1ScaleBias.Z / SceneCoordinate1ScaleBias.Y * 0.5f,
					0.5f + GPixelCenterOffset / HalfSizeX - SceneCoordinate1ScaleBias.W / SceneCoordinate1ScaleBias.X * 0.5f ));

			// used to scale to quarter res (which is left top screen +1 border texel)
			{
				FLOAT BorderScaleX = QuarterSizeX / (FLOAT)FilterBufferSizeX;
				FLOAT BorderScaleY = QuarterSizeY / (FLOAT)FilterBufferSizeY;
				SetVertexShaderValue(
					BlendVertexShader->GetVertexShader(),
					BlendVertexShader->SceneCoordinate3ScaleBiasParameter,
					FVector4(
						BorderScaleX / HalfSizeX,
						BorderScaleY / HalfSizeY,
						0.5f + (GPixelCenterOffset / HalfSizeY - View.RenderTargetY / (FLOAT)BufferSizeY) * BorderScaleY 
						+ SceneCoordinate1ScaleBias.Z / SceneCoordinate1ScaleBias.Y * 0.5f * BorderScaleY,
						0.5f + (GPixelCenterOffset / HalfSizeX - View.RenderTargetX / (FLOAT)BufferSizeX) * BorderScaleX 
						- SceneCoordinate1ScaleBias.W / SceneCoordinate1ScaleBias.X * 0.5f * BorderScaleX)
					);
			}

			// Draw a quad to do the half resolution post processing pass before the final uber post processing pass
			// UV is setup to map to the full resolution
			DrawDenormalizedQuad(
				View.RenderTargetX / 2, View.RenderTargetY / 2, View.RenderTargetSizeX / 2, View.RenderTargetSizeY / 2,
				View.RenderTargetX, View.RenderTargetY, View.RenderTargetSizeX, View.RenderTargetSizeY,
				HalfSizeX, HalfSizeY,
				BufferSizeX, BufferSizeY);

			RHICopyToResolveTarget(GSceneRenderTargets.GetHalfResPostProcessSurface(), FALSE, FResolveParams());
		}
	}

	// generate quarter resolution bloom content
	void RenderBloom(FViewInfo& View, FLOAT EffectSizeScale, UBOOL bSeparateTranslucency)
	{
		SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Bloom"));

		// full resolution (this can be much bigger than the viewport needed size, half and quarter is computed from that size)
		const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
		const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();
		// quarter resolution +2 pixel border
		const UINT FilterBufferSizeX = GSceneRenderTargets.GetFilterBufferSizeX();
		const UINT FilterBufferSizeY = GSceneRenderTargets.GetFilterBufferSizeY();
		const UINT FilterDownsampleFactor = GSceneRenderTargets.GetFilterDownsampleFactor();
		// quarter resolution of the view without border
		const UINT DownsampledSizeX = View.RenderTargetSizeX / FilterDownsampleFactor;
		const UINT DownsampledSizeY = View.RenderTargetSizeY / FilterDownsampleFactor;
		FVector2D SampleMaskMin(0 / (FLOAT)BufferSizeX, 0 / (FLOAT)BufferSizeY);
		FVector2D SampleMaskMaxA((0 + View.SizeX - 1) / (FLOAT)BufferSizeX, (0 + View.SizeY - 1) / (FLOAT)BufferSizeY);
		FVector2D SampleMaskMaxB((0 + View.SizeX / 2 - 1) / (FLOAT)BufferSizeX, (0 + View.SizeY / 2 - 1) / (FLOAT)BufferSizeY);
		FVector2D SampleMaskMaxC((0 + View.SizeX / 4 - 1) / (FLOAT)BufferSizeX, (0 + View.SizeY / 4 - 1) / (FLOAT)BufferSizeY);

		// the color was range compressed to fit in the range so we need to decompress it
		const FLOAT MAX_SCENE_COLOR = 4.0f;

		// create source for bloom in quarter resolution in #1
		RenderGatherPass(View, EGD_Bloom, SRTI_FilterColor1, MAX_SCENE_COLOR, BloomQuality, bSeparateTranslucency);

		FIntPoint Origin(1, 1);

		if(BloomType == 0)
		{
			// one Gaussian bell curve (limited radius but linear performance with radius and look)
			SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Blur 1 Gaussian 1 resolution"));

			// blur #1
			GaussianBlurFilterBuffer(View,View.SizeX, DownsampledSizeX + AntiLeakBorder, DownsampledSizeY + AntiLeakBorder, BloomKernelSizeMedium * EffectSizeScale, 1.0f, SRTI_FilterColor1, SampleMaskMin, SampleMaskMaxA);
		}
		else if(BloomType == 1)
		{
			// 2 Gaussian bell curves combined
			SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Blur 2 Gaussians 2 resolutions"));

			// downsample #1 to #2
			{	
				SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Downsample to low res"));

				DrawDownsampledTexture(
					GSceneRenderTargets.GetFilterColorSurface(SRTI_FilterColor2),
					GSceneRenderTargets.GetFilterColorTexture(SRTI_FilterColor2),
					GSceneRenderTargets.GetFilterColorTexture(SRTI_FilterColor1),
					Origin,
					FIntRect(Origin, Origin + FIntPoint(DownsampledSizeX, DownsampledSizeY)),
					FIntPoint(FilterBufferSizeX, FilterBufferSizeY),
					FIntPoint(FilterBufferSizeX, FilterBufferSizeY));
			}

			// now:
			//   #1: 1:1 res 
			//   #2: 1:2 res

			// multiple times Gaussian blur is like adding radius so we blur less as be blur again
			FLOAT Pass1KernelSize = BloomKernelSizeMedium - BloomKernelSizeSmall;

			// blur #2
			// adjust SampleMaskMin, SampleMaskMax ?				
			// * 0.5f on kernel size as this is 1/2 resolution
			{
				SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Blur low res"));

				GaussianBlurFilterBuffer(View,View.SizeX, DownsampledSizeX / 2 + AntiLeakBorder, DownsampledSizeY / 2 + AntiLeakBorder, Pass1KernelSize * EffectSizeScale * 0.5f, 1.0f, SRTI_FilterColor2, SampleMaskMin, SampleMaskMaxB);
			}

			FLOAT BloomWeightMedium = 1.0f - BloomWeightSmall;

			checkSlow(BloomWeightMedium >= 0.0f && BloomWeightMedium <= 1.0f);

			// #1 = #1 * .. + #2 * ..
			{
				SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Combine"));

				CombineFilterBuffer(DownsampledSizeX + AntiLeakBorder, DownsampledSizeY + AntiLeakBorder, SRTI_FilterColor1, BloomWeightSmall, SRTI_FilterColor2, BloomWeightMedium);
			}

			// blur #1
			{
				SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Blur result"));

				GaussianBlurFilterBuffer(View,View.SizeX, DownsampledSizeX + AntiLeakBorder, DownsampledSizeY + AntiLeakBorder, BloomKernelSizeSmall * EffectSizeScale, 1.0f, SRTI_FilterColor1, SampleMaskMin, SampleMaskMaxA);
			}
		}
		else if(BloomType == 2)
		{				
			// 3 Gaussian bell curves combined
			SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Blur 3 Gaussians 3 resolutions"));

			{	
				SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Downsample to medium res"));

				// downsample #1 to #2
				DrawDownsampledTexture(
					GSceneRenderTargets.GetFilterColorSurface(SRTI_FilterColor2),
					GSceneRenderTargets.GetFilterColorTexture(SRTI_FilterColor2),
					GSceneRenderTargets.GetFilterColorTexture(SRTI_FilterColor1),
					Origin,
					FIntRect(Origin, Origin + FIntPoint(DownsampledSizeX, DownsampledSizeY)),
					FIntPoint(FilterBufferSizeX, FilterBufferSizeY),
					FIntPoint(FilterBufferSizeX, FilterBufferSizeY));
			}

			// now:
			//   #0: 1:4 res
			//   #1: 1:1 res 
			//   #2: 1:2 res

			// multiple times Gaussian blur is like adding radius so we blur less as be blur again
			FLOAT Pass1KernelSize = BloomKernelSizeMedium - BloomKernelSizeSmall;

			{
				SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Blur medium res"));

				// blur #2
				// adjust SampleMaskMin, SampleMaskMax ?				
				// * 0.5f on kernel size as this is 1/2 resolution
				GaussianBlurFilterBuffer(View,View.SizeX, DownsampledSizeX / 2 + AntiLeakBorder, DownsampledSizeX / 2 + AntiLeakBorder, Pass1KernelSize * EffectSizeScale * 0.5f, 1.0f, SRTI_FilterColor2, SampleMaskMin, SampleMaskMaxB);
			}

			// downsample #2 to #0
			{
				SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Downsample to low res"));

				DrawDownsampledTexture(
					GSceneRenderTargets.GetFilterColorSurface(SRTI_FilterColor0),
					GSceneRenderTargets.GetFilterColorTexture(SRTI_FilterColor0),
					GSceneRenderTargets.GetFilterColorTexture(SRTI_FilterColor2),
					Origin,
					FIntRect(Origin, Origin + FIntPoint(DownsampledSizeX / 2, DownsampledSizeY / 2)),
					FIntPoint(FilterBufferSizeX, FilterBufferSizeY),
					FIntPoint(FilterBufferSizeX, FilterBufferSizeY));
			}

			// multiple times Gaussian blur is like adding radius so we blur less as be blur again
			FLOAT Pass2KernelSize = BloomKernelSizeLarge - BloomKernelSizeSmall;

			// blur #0
			// adjust SampleMaskMin, SampleMaskMax ?				
			// * 0.25f on kernel size as this is 1/4 resolution
			{
				SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Blur high res"));

				GaussianBlurFilterBuffer(View,View.SizeX, DownsampledSizeX / 4 + AntiLeakBorder, DownsampledSizeY / 4 + AntiLeakBorder, Pass2KernelSize * EffectSizeScale * 0.25f, 1.0f, SRTI_FilterColor0, SampleMaskMin, SampleMaskMaxC);
			}

			FLOAT BloomWeightMedium = 1.0f - BloomWeightSmall - BloomWeightLarge;

			checkSlow(BloomWeightMedium >= 0.0f && BloomWeightMedium <= 1.0f);

			// #1 = #1 * .. + #2 *.. + #0 * ..
			{
				SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Combine"));

				CombineFilterBuffer(DownsampledSizeX + AntiLeakBorder, DownsampledSizeY + AntiLeakBorder, SRTI_FilterColor1, BloomWeightSmall, SRTI_FilterColor2, BloomWeightMedium, SRTI_FilterColor0, BloomWeightLarge);
			}

			// blur #1
			{
				SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Blur result"));

				GaussianBlurFilterBuffer(View,View.SizeX, DownsampledSizeX + AntiLeakBorder, DownsampledSizeY + AntiLeakBorder, BloomKernelSizeSmall * EffectSizeScale, 1.0f, SRTI_FilterColor1, SampleMaskMin, SampleMaskMaxA);
			}
		}
		else if(BloomType == 3)
		{
			// one Gaussian bell curve, multi pass (unlimited radius, but performance can suffer quickly, for backwards compatibility)
			// radius can only be chosen divisible by 4 (e.g. 0,4,8,12,..)
			SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Blur 1 Gaussian multipass if needed"));

			// blur #1
			const INT BloomKernelTexels = appTrunc(BloomKernelSizeMedium / FilterDownsampleFactor);
			// Determine how many blur passes will be needed based on the blur kernels size, where each blur pass doubles the effective blur size
			const INT NumPasses = appCeilLogTwo((BloomKernelTexels + MAX_FILTER_SAMPLES - 1) / MAX_FILTER_SAMPLES) + 1;

			for (INT BlurPassIndex = 0; BlurPassIndex < NumPasses; BlurPassIndex++)
			{
				//@todo - when doing multiple passes, allow blurring on increments less than (1 << NumPasses) * MAX_FILTER_SAMPLES
				INT PassBlurKernel = NumPasses == 1 ? BloomKernelTexels : MAX_FILTER_SAMPLES;
				// Scale up by the next power of two
				FLOAT PassBlurScale = 1 << BlurPassIndex;
				GaussianBlurFilterBuffer(View,View.SizeX, DownsampledSizeX + AntiLeakBorder, DownsampledSizeY + AntiLeakBorder, PassBlurKernel * EffectSizeScale * FilterDownsampleFactor, PassBlurScale, SRTI_FilterColor1, SampleMaskMin, SampleMaskMaxA);
			}
		}

		// #1 now has the bloomed scene content
	}

	/**
	 * Selects the proper pixel shader and renders the full-res variation.
	 * @param View - current view.
	 * @param ColorGradingRHI - the color grading LUT.
	 */
	template<UINT DepthOfFieldFullResMode, UINT TemplTonemapperType, UBOOL bSeparateTranslucency, UBOOL bImageGrain, UBOOL bTemporalAA>
	void RenderVariationFullResTempl(FViewInfo& View, const FTextureRHIRef& ColorGradingRHI)
	{
		static FGlobalBoundShaderState UberBlendBoundShaderState;
		TShaderMapRef<FUberPostProcessVertexShader> BlendVertexShader(GetGlobalShaderMap());
		TShaderMapRef<TUberPostProcessBlendPixelShader<DepthOfFieldFullResMode,TemplTonemapperType,bImageGrain,bSeparateTranslucency,bTemporalAA> > BlendPixelShader(GetGlobalShaderMap());
		SetGlobalBoundShaderState(UberBlendBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *BlendVertexShader, *BlendPixelShader, sizeof(FFilterVertex), 0, EGST_UberPostProcess);
		RenderVariationFullRes(View, ColorGradingRHI, *BlendVertexShader, *BlendPixelShader);
	}

	/**
	 * Render the full-res variation.
	 * @param View - current view.
	 * @param ColorGradingRHI - the color grading LUT.
	 * @param BlendVertexShader - the vertex shader with which to render.
	 * @param BlendPixelShader - the pixel shader with which to render.
	 */
	void RenderVariationFullRes(
		FViewInfo& View,
		const FTextureRHIRef& ColorGradingRHI,
		FUberPostProcessVertexShader* BlendVertexShader,
		FUberPostProcessBlendPixelShaderBase* BlendPixelShader)
	{
		// full resolution (this can be much bigger than the viewport needed size, half and quarter is computed from that size)
		const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
		const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();
		// quarter resolution +2 pixel border
		const UINT FilterBufferSizeX = GSceneRenderTargets.GetFilterBufferSizeX();
		const UINT FilterBufferSizeY = GSceneRenderTargets.GetFilterBufferSizeY();
		const UINT FilterDownsampleFactor = GSceneRenderTargets.GetFilterDownsampleFactor();
		// half resolution
		const UINT HalfSizeX = BufferSizeX / 2;
		const UINT HalfSizeY = BufferSizeY / 2;
		// quarter resolution
		const UINT QuarterSizeX = BufferSizeX / FilterDownsampleFactor;
		const UINT QuarterSizeY = BufferSizeY / FilterDownsampleFactor;

		// The combined (uber) post-processing shader does depth of field, bloom, motion blur, material colorization and gamma correction
		// all together.
		//
		// It takes an HDR (64-bit) input and produces a low dynamic range (32-bit) output.  If it is the final post processing
		// shader in the post processing chain then it can render directly into the view's render target.  Otherwise it has to render into
		// a 32-bit render target.
		//
		// Note: Any post-processing shader that follows the uber shader needs to be able to handle an LDR input.  The shader can can check
		//       for this by looking at the bUseLDRSceneColor flag in the FViewInfo structure.  Also the final shader following the uber
		//       shader needs to write into the view's render target (or needs to write the result out to a 64-bit render-target).

		// Set the DOF and bloom parameters
		BlendPixelShader->DOFParameters.SetPS(BlendPixelShader, View.DepthOfFieldParams);
		BlendPixelShader->PostProcessParameters.SetPS(BlendPixelShader, BlurKernelSize, DOFOcclusionTweak);

		// Set the color LUT texture parameter
		SetTextureParameterDirectly(
			BlendPixelShader->GetPixelShader(),
			BlendPixelShader->ColorGradingLUTTextureParameter,
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
			ColorGradingRHI);

		{
			// we clamp the half res lookup in the view rectangle to fix Matinee cinema border and split screen border artifacts

			// contract view by half a texel in half res (1 texel in full res)
			FLOAT Border = 1.0f; 
			FVector2D SampleMaskMin(View.RenderTargetX + Border, View.RenderTargetY + Border);
			FVector2D SampleMaskMax(View.RenderTargetX + View.RenderTargetSizeX - 1 - Border, View.RenderTargetY + View.RenderTargetSizeY - 1 - Border);

			FVector4 MaskRect(
				SampleMaskMin.X / BufferSizeX,
				SampleMaskMin.Y / BufferSizeY,
				SampleMaskMax.X / BufferSizeX,
				SampleMaskMax.Y / BufferSizeY);

			SetPixelShaderValues(
				BlendPixelShader->GetPixelShader(),
				BlendPixelShader->HalfResMaskRectParameter,
				&MaskRect,
				1);
		}

		// Setup the scene texture
		BlendPixelShader->SceneTextureParameters.Set(&View, BlendPixelShader, SF_Point);

		SetPixelShaderValue(
			BlendPixelShader->GetPixelShader(), 
			BlendPixelShader->BloomTintAndScreenBlendThresholdParameter, 
			// Bloom scale is applied after the blurring to avoid clamping
			FVector4(BloomTint.R * BloomScale, BloomTint.G * BloomScale, BloomTint.B * BloomScale, BloomScreenBlendThreshold));

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
//			SetTextureParameterDirectly(BlendPixelShader->GetPixelShader(), BlendPixelShader->LowResPostProcessBuffer, TStaticSamplerState<SF_Bilinear>::GetRHI(), GSceneRenderTargets.GetHalfResPostProcessTexture() );
		}
		else
#endif
		{
			SetTextureParameterDirectly(BlendPixelShader->GetPixelShader(), BlendPixelShader->LowResPostProcessBuffer, TStaticSamplerState<SF_Bilinear>::GetRHI(), GSceneRenderTargets.GetHalfResPostProcessTexture() );
		}

		// noise texture
		if(BlendPixelShader->NoiseTextureParameter.IsBound())
		{
			UTexture2D* NoiseTexture = GEngine->ImageGrainNoiseTexture;
			
			if(!NoiseTexture)
			{
				// in case there is no noise texture (ini setting points to not existing texture)
				SceneImageGrainScale = 0;
				NoiseTexture = GEngine->ScreenDoorNoiseTexture;
			}

			SetTextureParameter(
				BlendPixelShader->GetPixelShader(),
				BlendPixelShader->NoiseTextureParameter,
				TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
				NoiseTexture->Resource->TextureRHI );
		}

		// set ImageAdjustments1Parameter
		{
			const bool AnimateNoise = true;

			// chose to work well with the 64x64 texture and 
			// to give pleasant change over time
			const char RandomValues[] = 
			{
				8,59,
				43,48,
				20,63,
				43,19,
				6,26,
				8,46,
				29,31,
				30,63,
				44,34,
				62,36,
				49,63,
				36,56,
				35,30,
				41,63,
				10,54,
				58,4
			};

			static UINT NewRand[2] = {0, 0};

			if(AnimateNoise)
			{
				static UINT State = 0;

				State = (State + 1) % 16;

				// tweaked with the 6x64 texture to not have obvious movement in the noise
				NewRand[0] = RandomValues[State * 2 + 0];
				NewRand[1] = RandomValues[State * 2 + 1];
			}

			SetPixelShaderValue(
				BlendPixelShader->GetPixelShader(), 
				BlendPixelShader->ImageAdjustments1Parameter,
				FVector4(NewRand[0], NewRand[1], 0, SceneImageGrainScale));
		}

		// set ImageAdjustments2Parameter and ImageAdjustments3Parameter
		{
			// Adjusted by some constant slightly bigger than 1 to allow the values to reach 1
			// Max to avoid division by zero
			const FLOAT TonemapperScaleClamped = Max(0.000001f, TonemapperScale);

			// 0.22f is the value where the sigmoid curve has the slope of 1 where it meets the linear curve
			FLOAT A = 0.22f / TonemapperScaleClamped;
			FLOAT B = 1.0f;

			// adjust scale to get y=1 at x=TonemapperRange
			{
				// compute y for x=TonemapperRange
				FLOAT y = TonemapperAFunc(TonemapperRange, A, B);

				B /= y;
			}

			FLOAT LinearSteepness = 1.0f * TonemapperScaleClamped;

			FLOAT FunctionSplitPos = InverseTonemapperADerivedFunc(LinearSteepness, A, B);

			SetPixelShaderValue(
				BlendPixelShader->GetPixelShader(), 
				BlendPixelShader->ImageAdjustments2Parameter,
				FVector4(A, B, FunctionSplitPos, LinearSteepness));

			SetPixelShaderValue(
				BlendPixelShader->GetPixelShader(), 
				BlendPixelShader->ImageAdjustments3Parameter,
				FVector4(Clamp(TonemapperToeFactor, 0.0f, 1.0f), 0, 0, 0));
		}

		if(DepthOfFieldType == DOFType_BokehDOF)
		{
			SetTextureParameter(
				BlendPixelShader->GetPixelShader(),
				BlendPixelShader->BokehDOFLayerTextureParameter, 
				TStaticSamplerState<SF_Bilinear>::GetRHI(),
				GSceneRenderTargets.GetBokehDOFTexture() );
		}

		// Set the motion blur parameters.
		BlendPixelShader->MotionBlurShaderParameters.Set(BlendPixelShader->GetPixelShader(),View,MotionBlurParams);

		// We need to adjust the UV coordinate calculation for the scene color texture when rendering directly to
		// the view's render target (to support the editor).

		// without div by 8 adjustment
		UINT TargetSizeX = View.Family->RenderTarget->GetSizeX();
		UINT TargetSizeY = View.Family->RenderTarget->GetSizeY();
		FLOAT ScaleX = TargetSizeX / static_cast<FLOAT>(BufferSizeX);
		FLOAT ScaleY = TargetSizeY / static_cast<FLOAT>(BufferSizeY);

		// For split-screen and 4:3 views we also need to take into account the viewport correctly; however, the
		// DOFAndBloomBlendVertex shader computes the UV coordinates for the SceneColor texture directly from the
		// screen coordinates that are used to render and since the view-port may not be located at 0,0 we need
		// to adjust for that by modifying the UV offset and scale.

		// used to scale to half res, shift to view in left top corner
		SetVertexShaderValue(
			BlendVertexShader->GetVertexShader(),
			BlendVertexShader->SceneCoordinate2ScaleBiasParameter,
			FVector4(
			0.5f / HalfSizeX,
			0.5f / HalfSizeY,
			View.RenderTargetY / (FLOAT)BufferSizeY,
			View.RenderTargetX / (FLOAT)BufferSizeX)
			);

		// used to scale to quarter res (which is left top screen +1 border texel)
		{
			SetVertexShaderValue(
				BlendVertexShader->GetVertexShader(),
				BlendVertexShader->SceneCoordinate3ScaleBiasParameter,
				FVector4(
				0.25f / FilterBufferSizeX,
				0.25f / FilterBufferSizeY,
				1.0f / FilterBufferSizeY,
				1.0f / FilterBufferSizeX)
				);
		}

		if( FinalEffectInGroup
			&& View.Family->bResolveScene 
			&& !GSystemSettings.NeedsUpscale() )
		{	
			// compute pixel position from screen pos, unify D3D9 half texel shift (done like this for cleaner code), flip y
			SetVertexShaderValue(
				BlendVertexShader->GetVertexShader(),
				BlendVertexShader->SceneCoordinate1ScaleBiasParameter,
				FVector4(
				0.5f * TargetSizeX,
				-0.5f * TargetSizeY,
				0.5f * TargetSizeY + GPixelCenterOffset - View.Y,
				0.5f * TargetSizeX + GPixelCenterOffset - View.X));

			// Draw a quad mapping the blurred pixels in the filter buffer to the scene color buffer.
			DrawDenormalizedQuad(
				View.X, View.Y, View.SizeX, View.SizeY,
				View.RenderTargetX, View.RenderTargetY, View.RenderTargetSizeX, View.RenderTargetSizeY,
				TargetSizeX, TargetSizeY,
				BufferSizeX, BufferSizeY);
		}
		else
		{
			// compute pixel position from screen pos, unify D3D9 half texel shift (done like this for cleaner code), flip y
			SetVertexShaderValue(
				BlendVertexShader->GetVertexShader(),
				BlendVertexShader->SceneCoordinate1ScaleBiasParameter,
				FVector4(
				0.5f * BufferSizeX,
				-0.5f * BufferSizeY,
				0.5f * BufferSizeY + GPixelCenterOffset - View.RenderTargetY,
				0.5f * BufferSizeX + GPixelCenterOffset - View.RenderTargetX));

			// Draw a quad mapping the blurred pixels in the filter buffer to the scene color buffer.
			DrawDenormalizedQuad(
				View.RenderTargetX, View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
				View.RenderTargetX, View.RenderTargetY, View.RenderTargetSizeX, View.RenderTargetSizeY,
				BufferSizeX, BufferSizeY,
				BufferSizeX, BufferSizeY);

			FResolveRect ResolveRect;
			ResolveRect.X1 = View.RenderTargetX;
			ResolveRect.Y1 = View.RenderTargetY;
			ResolveRect.X2 = View.RenderTargetX + View.RenderTargetSizeX;
			ResolveRect.Y2 = View.RenderTargetY + View.RenderTargetSizeY;

			if( View.Family->bResolveScene )
			{
				// Resolve the scene color LDR buffer.
				GSceneRenderTargets.FinishRenderingSceneColorLDR(TRUE,ResolveRect);
			}
			else
			{
				// Resolve the scene color HDR buffer.
				GSceneRenderTargets.FinishRenderingSceneColor(TRUE);
			}
		}

		if( View.Family->bResolveScene )
		{
			// Indicate that from now on the scene color is in an LDR surface.
			View.bUseLDRSceneColor = TRUE; 
		}
	}

	UBOOL Render(const FScene* Scene, UINT InDepthPriorityGroup, FViewInfo& View, const FMatrix& CanvasTransform, FSceneColorLDRInfo& LDRInfo)
	{
		SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("UberPostProcessing"));

		check(SDPG_PostProcess == InDepthPriorityGroup);

		if(PostProcessAA.IsEnabled(View))
		{
			// is rendered after all other post processing
			PostProcessAA.SetDeferredObject();
		}

		GSceneRenderTargets.ResolveFullResTransluceny();

		FLOAT EffectSizeScale = 1.0f;
		if(bScaleEffectsWithViewSize)
		{
			// we scale by with because FOV is defined horizontally
			// 1280x720 is the reference resolution
			EffectSizeScale = View.SizeX / 1280.0f;
		}

		DownSampleSceneAndDepth(View);

		check(FALSE==View.bUseLDRSceneColor);

		// blend final LUT (3D color lookup texture) for color grading
		FTextureRHIRef ColorGradingRHI = ColorGradingBlenderRTCopy.ResolveLUT(View, ColorTransform);

		// setup flags -------------------------------------------

		UBOOL bUseDoFBlurBuffer = FALSE;

#if XBOX
		if( View.bRenderedToDoFBlurBuffer && (View.Family->ShowFlags & SHOW_TranslucencyDoF) )
		{
			bUseDoFBlurBuffer = TRUE;
		}
#endif
		// Should we do MotionBlur for this view?
		UBOOL bLocalMotionBlur = bMotionBlur;

		// it depends on view show flags
		// Disable motion blur when not using realtime update, since it requires continuous frames.
		if(
			( ( View.Family->ShowFlags & SHOW_MotionBlur ) == 0 )
			|| ( !View.Family->bRealtimeUpdate )
			|| ( !View.Family->ShouldPostProcess() )
		)
		{
			bLocalMotionBlur = FALSE;
		}

		UBOOL bUseSoftEdgeMotionBlur = MotionBlurSoftEdgeKernelSize > 0;

		if(!bLocalMotionBlur)
		{
			// soft edge motion blur without motion blur is useless
			bUseSoftEdgeMotionBlur = FALSE;
		}

		EDOFType LocalDepthOfFieldType = DepthOfFieldType;

		// depending on view flags or if the radius is too small (optimization) we don't render depth of field
		if(
			( ( View.Family->ShowFlags & SHOW_DepthOfField ) == 0 )
			|| ( !View.Family->ShouldPostProcess() )
			|| ( BlurKernelSize < 0.1f )
		)
		{
			// switch Depth of Field off
			LocalDepthOfFieldType = DOFType_MAX;
		}

		// DepthOfFieldHalfResMode: 0:no DOF (e.g. ReferenceDOF in full res), 1:SimpleDOf, 2:ReferenceDOF in half res
		UINT DepthOfFieldHalfResMode = 0;
		{
			if(LocalDepthOfFieldType == DOFType_SimpleDOF)
			{
				DepthOfFieldHalfResMode = 1;
			}
			// if the showflag is on we override the DepthOfField mode
			if(
				( View.Family->ShowFlags & SHOW_VisualizeDOFLayers )
				&& ( View.Family->ShouldPostProcess() )
			)
			{
				DepthOfFieldHalfResMode = 2;
			}
		}

		checkSlow(IsInRenderingThread());
		UBOOL bSeparateTranslucency = GSystemSettings.bAllowSeparateTranslucency;

		// chose the right shader depending on flags -------------

		// half resolution pass -----------------

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			// don't support DoF on mobile yet
		}
		else
#endif
		{
#define VARIATION(A, B, C, D) \
			if(A == bUseDoFBlurBuffer && B == bUseSoftEdgeMotionBlur && C == DepthOfFieldHalfResMode && D == bLocalMotionBlur) \
			{ \
				RenderVariationHalfRes<A, B, C, D>(View, LocalDepthOfFieldType, EffectSizeScale, bSeparateTranslucency); \
			} else
			//
			VARIATION(0,0,0,0) VARIATION(0,0,0,1) VARIATION(0,0,1,0) VARIATION(0,0,1,1)
			VARIATION(0,0,2,0) VARIATION(0,0,2,1)
			VARIATION(0,1,0,0) VARIATION(0,1,0,1) VARIATION(0,1,1,0) VARIATION(0,1,1,1)
			VARIATION(0,1,2,0) VARIATION(0,1,2,1)
			VARIATION(1,0,0,0) VARIATION(1,0,0,1) VARIATION(1,0,1,0) VARIATION(1,0,1,1)
			VARIATION(1,0,2,0) VARIATION(1,0,2,1)
			VARIATION(1,1,0,0) VARIATION(1,1,0,1) VARIATION(1,1,1,0) VARIATION(1,1,1,1)
			VARIATION(1,1,2,0) VARIATION(1,1,2,1) 
			// terminator, should never be reached
			check(0);
			//
#undef VARIATION
		}

		if (GSystemSettings.bAllowBloom)
		{
			RenderBloom(View, EffectSizeScale, bSeparateTranslucency);
		}

		//

		// if the scene wasn't meant to be resolved to LDR then continue rendering to HDR
		if( !View.Family->bResolveScene )			
		{
			// Using 64-bit (HDR) surface
			GSceneRenderTargets.BeginRenderingSceneColor();
		}
		else
		{
			// Normally, we're going to specify RTUsage_FullOverwrite and ping-pong the RT between the surface and its resolve-target.
			// But if we're drawing to secondary views (e.g. splitscreen), we must make sure that the final result is drawn to the same
			// memory as view0, so that it will contain the final fullscreen image after all postprocess.
			// To do this, we need to detect if there are an odd number of effects that draw to LDR (on the secondary view), since an
			// even number of ping-pongs will place the final image in the same buffer as view0.
			// On PS3 SceneColorLDR, LightAttenuation and BackBuffer refers to the same memory.
			DWORD UsageFlags = RTUsage_FullOverwrite;
			if ( LDRInfo.bAdjustPingPong && (LDRInfo.NumPingPongsRemaining & 1) )
			{
				UsageFlags |= RTUsage_DontSwapBuffer;
			}

			//if this is the final effect then render directly to the view's render target
			//unless an upscale is needed, in which case render to LDR scene color
			if (FinalEffectInGroup
				&& !GSystemSettings.NeedsUpscale())
			{
				// Render to the final render target
				GSceneRenderTargets.BeginRenderingBackBuffer( UsageFlags );
			}
			else
			{
				GSceneRenderTargets.BeginRenderingSceneColorLDR( UsageFlags );
			}
		}

		//

		UBOOL DepthOfFieldFullResMode = 0;
		
		if(LocalDepthOfFieldType == DOFType_ReferenceDOF)
		{
			DepthOfFieldFullResMode = 1;
		}
		else if(LocalDepthOfFieldType == DOFType_BokehDOF)
		{
			DepthOfFieldFullResMode = 2;
		}

		// allow console command to override
		UBOOL bLocalImageGrain = bEnableSceneEffect && SceneImageGrainScale > 0.0f;
		
		// Doing image grain for this view depends on show flags
		if(
			( ( View.Family->ShowFlags & SHOW_ImageGrain ) == 0 )
			|| ( !View.Family->ShouldPostProcess() )
		)
		{
			bLocalImageGrain = FALSE;
		}

		const UBOOL bTemporalAA = 
		   GSystemSettings.bAllowTemporalAA
		&& (View.Family->ShowFlags & SHOW_TemporalAA)
		&& View.RenderingOverrides.bAllowTemporalAA;

		// full resolution pass -----------------
#define VARIATION1(A)		VARIATION2(A,0)			VARIATION2(A,1)			VARIATION2(A,2)
#define VARIATION2(A,B)		VARIATION3(A,B,0)		VARIATION3(A,B,1)
#define VARIATION3(A,B,C)	VARIATION4(A,B,C,0)		VARIATION4(A,B,C,1)
#define VARIATION4(A,B,C,D)	VARIATION5(A,B,C,D,0)	VARIATION5(A,B,C,D,1)
#define VARIATION5(A, B, C, D, E) \
		if(A == DepthOfFieldFullResMode && B == TonemapperType && C == bSeparateTranslucency && D == bLocalImageGrain && E == bTemporalAA) \
		{ \
			RenderVariationFullResTempl<A, B, C, D, E>(View, ColorGradingRHI); \
		} else
		VARIATION1(0) VARIATION1(1) VARIATION1(2)

		// terminator, should never be reached
		check(0);
		//
#undef VARIATION1
#undef VARIATION2
#undef VARIATION3
#undef VARIATION4
#undef VARIATION5

		return TRUE;
	}


	/**
	 * Informs FSceneRenderer what to do during pre-pass.
	 * @param MotionBlurParameters	- The parameters for the motion blur effect are returned in this struct.
	 * @return Motion blur needs to have velocities written during pre-pass.
	 */
	virtual UBOOL RequiresVelocities( FMotionBlurParams& OutMotionBlurParams ) const
	{
		// when used for SceneCapture actor we get a call from the game thread
		// when used for normal post processing the call comes from the render thread

		if ( bMotionBlur )
		{
			OutMotionBlurParams = MotionBlurParams;

			const AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

			OutMotionBlurParams.bPlayersOnly = WorldInfo->bPlayersOnly;
		}
		return bMotionBlur;
	}

	/**
	 * Tells FSceneRenderer whether to store the previous frame's transforms.
	 */
	virtual UBOOL RequiresPreviousTransforms(const FViewInfo& View) const
	{
		return bMotionBlur;
	}

	/**
	 * Whether the effect may potentially render to SceneColorLDR or not.
	 * @return TRUE if the effect may potentially render to SceneColorLDR, otherwise FALSE.
	 */
	virtual UBOOL MayRenderSceneColorLDR() const
	{
		return TRUE;
	}

	/**
	 * Whether the effect will setup a deferred post-process anti-aliasing effect.
	 */
	virtual UBOOL HasPostProcessAA(const FViewInfo& View) const
	{
		return PostProcessAA.IsEnabled(View);
	}

	void CheckForChanges( const FLUTBlender& PreviousLUTBlender )
	{
		ColorGradingBlenderRTCopy.CheckForChanges( PreviousLUTBlender );
	}

	const FLUTBlender& GetLUTBlender() const
	{
		return ColorGradingBlenderRTCopy;
	}

protected:
	ColorTransformMaterialProperties ColorTransform;
	FMotionBlurParams MotionBlurParams;
	FLUTBlender ColorGradingBlenderRTCopy;
	FLOAT TonemapperScale;
	FLOAT SceneImageGrainScale;
	FLOAT MotionBlurSoftEdgeKernelSize;
	FLOAT TonemapperToeFactor;
	FLOAT TonemapperRange;
	UINT ColorGradingCVar;
	UINT TonemapperType;
	INT BloomType;
	INT BloomQuality;
	FLOAT BloomWeightSmall;
	FLOAT BloomWeightLarge; 
	FLOAT BloomKernelSizeSmall;
	FLOAT BloomKernelSizeMedium;
	FLOAT BloomKernelSizeLarge;
	UBOOL bMotionBlur;
	UBOOL bScaleEffectsWithViewSize;
	UBOOL bEnableSceneEffect;
	FPostProcessAA PostProcessAA;

	// to prevent bad colors content to leak in (in split screen or when not using the full render target)
	const static UINT AntiLeakBorder = 2;
};

/**
 * Creates a proxy to represent the render info for a post process effect
 * @param WorldSettings - The world's post process settings for the view.
 * @return The proxy object.
 */
FPostProcessSceneProxy* UUberPostProcessEffect::CreateSceneProxy(const FPostProcessSettings* WorldSettings)
{
	if ( GUsingMobileRHI && !GMobileAllowPostProcess )
	{
		return NULL;
	}

	UBOOL bLocalMotionBlur = FALSE;

	// Disable motion blur for tiled screenshots; the way motion blur is handled is
	// incompatible with tiled rendering and a single view.
	// Probably we could workaround with multiple views if it is necessary.
	extern UBOOL GIsTiledScreenshot;
	extern INT GGameScreenshotCounter;

	if ( (WorldSettings == NULL || WorldSettings->bEnableMotionBlur) && GSystemSettings.bAllowMotionBlur && !GIsTiledScreenshot && (GGameScreenshotCounter == 0) )
	{
		bLocalMotionBlur = TRUE;
	}

	UINT LocalTonemapperType = TonemapperType;

#if !FINAL_RELEASE
	{
		// console command can overwrite
		static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("TonemapperType")); 

		INT Value = CVar->GetInt();

		if(Value == 0
		|| Value == 1
		|| Value == 2)
		{
			LocalTonemapperType = Value;
		}
	}
#endif // !FINAL_RELEASE

	return new FUberPostProcessSceneProxy(this, WorldSettings, GColorGrading, LocalTonemapperType, bLocalMotionBlur, bEnableImageGrain);
}
