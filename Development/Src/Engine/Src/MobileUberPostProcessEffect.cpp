/*=============================================================================
	MobileUberPostProcessEffect.cpp: Mobile-specific uber post process effect
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"

#if WITH_MOBILE_RHI

FVector GMobileBloomTint( 1.0f, 0.75f, 0.0f );
FLOAT GOverrideFocusDistance = -1.0f;

struct FMobileUberPostProcessParameters
{
	FMobileUberPostProcessParameters()
	:	bIsInitialized(FALSE)
	{
	}

	UBOOL	IsInitialized() const
	{
		return bIsInitialized;
	}

	void	InitializeMobileShaderParameters()
	{
		TiltShiftParameters.SetShaderParamName(TEXT("TiltShiftParameters"));
		DOFParameters.SetShaderParamName(TEXT("DOFPackedParameters"));
		DOFFactor.SetShaderParamName(TEXT("DOFFactor"));

		BloomScaleAndThreshold.SetShaderParamName(TEXT("BloomScaleAndThreshold"));

		MobileColorGradingBlend.SetShaderParamName(TEXT("MobileColorGradingBlend"));
		MobileColorGradingDesaturation.SetShaderParamName(TEXT("MobileColorGradingDesaturation"));
		MobileColorGradingHighlightsMinusShadows.SetShaderParamName(TEXT("MobileColorGradingHighlightsMinusShadows"));
		MobileColorGradingMidTones.SetShaderParamName(TEXT("MobileColorGradingMidTones"));
		MobileColorGradingShadows.SetShaderParamName(TEXT("MobileColorGradingShadows"));

		// The gather pass uses a filter buffer for the SceneColorTexture
		InputTexture.SetBaseIndex(0, TRUE);
		SceneTextureParamsGatherDOF.SceneDepthTextureParameter.SetBaseIndex( 1, TRUE );
		SceneTextureParamsGatherDOF.SceneDepthCalcParameter.SetShaderParamName(TEXT("MinZ_MaxZRatio"));

		// The apply pass uses all the default textures
		SceneTextureParamsApplyDOF.SceneColorTextureParameter.SetBaseIndex( 0, TRUE );
		SceneTextureParamsApplyDOF.SceneDepthTextureParameter.SetBaseIndex( 1, TRUE );
		SceneTextureParamsApplyDOF.SceneDepthCalcParameter.SetShaderParamName(TEXT("MinZ_MaxZRatio"));
		InputDOFTexture.SetBaseIndex(2, TRUE);

		// FXAA
		FXAAQualityRcpFrame.SetShaderParamName(TEXT("fxaaQualityRcpFrame"));
		FXAAConsoleRcpFrameOpt.SetShaderParamName(TEXT("fxaaConsoleRcpFrameOpt"));
		FXAAConsoleRcpFrameOpt2.SetShaderParamName(TEXT("fxaaConsoleRcpFrameOpt2"));

		bIsInitialized = TRUE;
	}

	void	SetDOFParameters( FPixelShaderRHIParamRef PixelShader, FViewInfo& View, const FPostProcessSettings& Settings )
	{
		FLOAT FocusDistance = Settings.MobilePostProcess.Mobile_DOF_Distance;
		FLOAT FocusMinRadius = Settings.MobilePostProcess.Mobile_DOF_MinRange;
		FLOAT FocusMaxRadius = Max(Settings.MobilePostProcess.Mobile_DOF_MinRange + 1, Settings.MobilePostProcess.Mobile_DOF_MaxRange);
		FLOAT FocusTransitionRange = FocusMaxRadius - FocusMinRadius;

		// "Hide" the DOF effect if it shouldn't be visible
		if ( !(View.Family->ShowFlags & SHOW_DepthOfField) || !(View.Family->ShowFlags & SHOW_PostProcess) || !Settings.bEnableDOF )
		{
			FocusMinRadius = 65000.0f;
			FocusMaxRadius = 65000.0f;
		}

		if ( GOverrideFocusDistance > 0.0f )
		{
			FocusDistance = GOverrideFocusDistance;
		}

		FVector4 DOFParameterValues( FocusDistance, FocusMinRadius, FocusMaxRadius, Clamp(1.0f/FocusTransitionRange, 0.0f, 1.0f) );
		SetPixelShaderValue( PixelShader, DOFParameters, DOFParameterValues );
		FLOAT AdjustedDOFFactor = Settings.bEnableDOF ? Settings.MobilePostProcess.Mobile_DOF_FarBlurFactor : 0.f;
		SetPixelShaderValue( PixelShader, DOFFactor, AdjustedDOFFactor );
	}

	void	SetBloomParameters( FPixelShaderRHIParamRef PixelShader, FViewInfo& View, const FPostProcessSettings& Settings )
	{
		FVector BloomColorScale( Settings.MobilePostProcess.Mobile_Bloom_Tint * Settings.MobilePostProcess.Mobile_Bloom_Scale );
		FLOAT BloomThreshold = Settings.MobilePostProcess.Mobile_Bloom_Threshold;

		// "Hide" the Bloom effect if it shouldn't be visible
		if ( !(View.Family->ShowFlags & SHOW_PostProcess) || !Settings.bEnableBloom )
		{
			BloomColorScale.Set( 0.0f, 0.0f, 0.0f );
		}

		FVector4 BloomScaleAndThresholdValue( BloomColorScale, BloomThreshold );
		SetPixelShaderValue( PixelShader, BloomScaleAndThreshold, BloomScaleAndThresholdValue );
	}

	// @return TRUE if color grading has effect and needs to be applied
	UBOOL SetColorGradingParameters( FPixelShaderRHIParamRef PixelShader, FViewInfo& View, const FPostProcessSettings& Settings, UBOOL bForceSetParams )
	{
		FLOAT MobileColorGradingBlendValue = Settings.MobileColorGrading.Blend;

		// "Hide" the color grading effect if it shouldn't be visible
		if ( !(View.Family->ShowFlags & SHOW_PostProcess) || !GSystemSettings.bAllowMobileColorGrading )
		{
			MobileColorGradingBlendValue = 0.0f;
		}

		FLOAT Desaturation = Settings.MobileColorGrading.Desaturation;

		// the 3 points we use to shape our colors (separate for the color channel)
		FLinearColor MobileColorGradingHighlightsMinusShadowsValue = Settings.MobileColorGrading.HighLights - Settings.MobileColorGrading.Shadows;
		FLinearColor MobileColorGradingMidTonesValue = Settings.MobileColorGrading.MidTones;
		FLinearColor MobileColorGradingShadowsValue = Settings.MobileColorGrading.Shadows;

		// fade out the color grading effect
		Desaturation = Lerp(0.0f, Desaturation, MobileColorGradingBlendValue);
		MobileColorGradingHighlightsMinusShadowsValue = Lerp(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f), MobileColorGradingHighlightsMinusShadowsValue, MobileColorGradingBlendValue);
		MobileColorGradingMidTonesValue = Lerp(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), MobileColorGradingMidTonesValue, MobileColorGradingBlendValue);
		MobileColorGradingShadowsValue = Lerp(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), MobileColorGradingShadowsValue, MobileColorGradingBlendValue);

		const FLOAT Nunace = 1.0f / 255.0f;
		if(Desaturation <= Nunace
		&& MobileColorGradingHighlightsMinusShadowsValue.Equals(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f), Nunace)
		&& MobileColorGradingMidTonesValue.Equals(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), Nunace)
		&& MobileColorGradingShadowsValue.Equals(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), Nunace)
		&& !bForceSetParams)
		{
			return FALSE;
		}

		// to fade out color where we want to have the grey scale
		SetPixelShaderValue( PixelShader, MobileColorGradingBlend, 1.0f - Desaturation );
		// to fade in grey scale,  /3.0 because we compute the grey scale with r+g+b
		SetPixelShaderValue( PixelShader, MobileColorGradingDesaturation, Desaturation / 3.0f );
		SetPixelShaderValue( PixelShader, MobileColorGradingHighlightsMinusShadows, MobileColorGradingHighlightsMinusShadowsValue );
		// *2 needed as mask for midtones is created with this: x *(1 - x)
		SetPixelShaderValue( PixelShader, MobileColorGradingMidTones, MobileColorGradingMidTonesValue * 2 );
		SetPixelShaderValue( PixelShader, MobileColorGradingShadows, MobileColorGradingShadowsValue );

		return TRUE;
	}

	void	SetFXAAParameters( FViewInfo& View )
	{
		FPixelShaderRHIRef PixelShader(NULL);	// not used for mobile
		FVertexShaderRHIRef VertexShader(NULL);

		FLOAT OOTargetSizeX = 1.f/static_cast<FLOAT>(View.RenderTargetSizeX);
		FLOAT OOTargetSizeY = 1.f/static_cast<FLOAT>(View.RenderTargetSizeY);

		FVector2D QualityRcpFrame(OOTargetSizeX, OOTargetSizeY);

		// From FXAA comments:
		// This effects sub-pixel AA quality and inversely sharpness.
		//   Where N ranges between,
		//     N = 0.50 (default)
		//     N = 0.33 (sharper)
		// {x___} = -N/screenWidthInPixels  
		// {_y__} = -N/screenHeightInPixels
		// {__z_} =  N/screenWidthInPixels  
		// {___w} =  N/screenHeightInPixels 
		const float N = 0.5f;
		FVector4 RcpFrameOpt( -N*OOTargetSizeX, -N*OOTargetSizeY, N*OOTargetSizeX, N*OOTargetSizeY);

		// From FXAA comments:
		// {x___} = -2.0/screenWidthInPixels  
		// {_y__} = -2.0/screenHeightInPixels
		// {__z_} =  2.0/screenWidthInPixels  
		// {___w} =  2.0/screenHeightInPixels 
		FVector4 RcpFrameOpt2( -2.0f*OOTargetSizeX, -2.0f*OOTargetSizeY, 2.0f*OOTargetSizeX, 2.0f*OOTargetSizeY);

		SetVertexShaderValue(VertexShader, FXAAQualityRcpFrame, QualityRcpFrame);

		SetPixelShaderValue(PixelShader, FXAAQualityRcpFrame, QualityRcpFrame);
		SetPixelShaderValue(PixelShader, FXAAConsoleRcpFrameOpt, RcpFrameOpt);
		SetPixelShaderValue(PixelShader, FXAAConsoleRcpFrameOpt2, RcpFrameOpt2);
	}

	UBOOL				bIsInitialized;
	FShaderParameter	TiltShiftParameters;
	FShaderParameter	DOFParameters;
	FShaderParameter	DOFFactor;
	FShaderParameter	BloomScaleAndThreshold;
	FShaderParameter	MobileColorGradingBlend;
	FShaderParameter	MobileColorGradingDesaturation;
	FShaderParameter	MobileColorGradingHighlightsMinusShadows;
	FShaderParameter	MobileColorGradingMidTones;
	FShaderParameter	MobileColorGradingShadows;
	FSceneTextureShaderParameters SceneTextureParamsGatherDOF;
	FSceneTextureShaderParameters SceneTextureParamsApplyDOF;
	FShaderResourceParameter InputTexture;	
	FShaderResourceParameter InputDOFTexture;
	FShaderParameter	FXAAQualityRcpFrame;
	FShaderParameter	FXAAConsoleRcpFrameOpt;
	FShaderParameter	FXAAConsoleRcpFrameOpt2;

};

FMobileUberPostProcessParameters GMobileUberPostProcessParameters;

/** Mobile Emulation versions of the shaders */
class FMobileUberPPEmulationVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMobileUberPPEmulationVertexShader,Global);

private:
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	/** Default constructor. */
	FMobileUberPPEmulationVertexShader() {}

	FMobileUberPPEmulationVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{}
};

IMPLEMENT_SHADER_TYPE(,FMobileUberPPEmulationVertexShader,TEXT("MobileUberPostProcessEmulationVertexShader"),TEXT("Main"),SF_Vertex,0,0);

class FMobileGatherDOFPixelShaderBase : public FGlobalShader
{

public:
	/** Initialization constructor. */
	FMobileGatherDOFPixelShaderBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
	FGlobalShader(Initializer)
	{
		Parameters.DOFParameters.Bind(Initializer.ParameterMap, TEXT("DOFPackedParameters"));
		Parameters.DOFFactor.Bind(Initializer.ParameterMap, TEXT("DOFFactor"));

		Parameters.BloomScaleAndThreshold.Bind(Initializer.ParameterMap, TEXT("BloomScaleAndThreshold"));

		// The gather pass uses a filter buffer for the SceneColorTexture
		Parameters.InputTexture.Bind(Initializer.ParameterMap, TEXT("InputTexture"));
		Parameters.SceneTextureParamsGatherDOF.Bind(Initializer.ParameterMap);
	}

	FMobileUberPostProcessParameters& GetPostProcessParameters() { return Parameters; }

protected:
	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << Parameters.DOFParameters << Parameters.DOFFactor << Parameters.BloomScaleAndThreshold << Parameters.InputTexture <<
			Parameters.SceneTextureParamsGatherDOF << Parameters.SceneTextureParamsApplyDOF;
		return bShaderHasOutdatedParameters;
	}

	FMobileGatherDOFPixelShaderBase() {}

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

private:
	FMobileUberPostProcessParameters Parameters;

};

class FMobileGatherDOFPixelShader : public FMobileGatherDOFPixelShaderBase
{
	DECLARE_SHADER_TYPE(FMobileGatherDOFPixelShader,Global);
public:
	FMobileGatherDOFPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
	  FMobileGatherDOFPixelShaderBase(Initializer) {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("INVERT_GAMMA_CORRECTION"), TEXT("0"));
	}
private:
	FMobileGatherDOFPixelShader() {}
};

IMPLEMENT_SHADER_TYPE(,FMobileGatherDOFPixelShader,TEXT("MobileUberPostProcessEmulationPixelShader"),TEXT("GatherMobileDOF"),SF_Pixel,0,0);

class FMobileGatherDOFPixelShaderInvertGamma : public FMobileGatherDOFPixelShaderBase
{
	DECLARE_SHADER_TYPE(FMobileGatherDOFPixelShaderInvertGamma,Global);
public:
	FMobileGatherDOFPixelShaderInvertGamma(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
	  FMobileGatherDOFPixelShaderBase(Initializer) {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("INVERT_GAMMA_CORRECTION"), TEXT("1"));
	}
private:
	FMobileGatherDOFPixelShaderInvertGamma() {}

};

IMPLEMENT_SHADER_TYPE(,FMobileGatherDOFPixelShaderInvertGamma,TEXT("MobileUberPostProcessEmulationPixelShader"),TEXT("GatherMobileDOF"),SF_Pixel,0,0);


class FMobileUberPPEmulationPixelShaderBase : public FGlobalShader
{

public:

	/** Initialization constructor. */
	FMobileUberPPEmulationPixelShaderBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
	  FGlobalShader(Initializer)
	{
		Parameters.DOFParameters.Bind(Initializer.ParameterMap, TEXT("DOFPackedParameters"));
		Parameters.DOFFactor.Bind(Initializer.ParameterMap, TEXT("DOFFactor"));

		Parameters.MobileColorGradingBlend.Bind(Initializer.ParameterMap, TEXT("MobileColorGradingBlend"));
		Parameters.MobileColorGradingDesaturation.Bind(Initializer.ParameterMap, TEXT("MobileColorGradingDesaturation"));
		Parameters.MobileColorGradingHighlightsMinusShadows.Bind(Initializer.ParameterMap, TEXT("MobileColorGradingHighlightsMinusShadows"));
		Parameters.MobileColorGradingMidTones.Bind(Initializer.ParameterMap, TEXT("MobileColorGradingMidTones"));
		Parameters.MobileColorGradingShadows.Bind(Initializer.ParameterMap, TEXT("MobileColorGradingShadows"));

		Parameters.InputDOFTexture.Bind(Initializer.ParameterMap, TEXT("InputTexture"));
		Parameters.SceneTextureParamsApplyDOF.Bind(Initializer.ParameterMap);
	}

	FMobileUberPostProcessParameters& GetPostProcessParameters() { return Parameters; }

protected:
	/** Default constructor. */
	FMobileUberPPEmulationPixelShaderBase() {}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << /*Parameters.TiltShiftParameters <<*/ Parameters.DOFParameters << Parameters.DOFFactor << Parameters.MobileColorGradingBlend <<
			Parameters.MobileColorGradingDesaturation << Parameters.MobileColorGradingHighlightsMinusShadows << Parameters.MobileColorGradingMidTones << Parameters.MobileColorGradingShadows <<
			Parameters.InputDOFTexture << Parameters.SceneTextureParamsGatherDOF << Parameters.SceneTextureParamsApplyDOF;
		return bShaderHasOutdatedParameters;
	}

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

private:
	FMobileUberPostProcessParameters Parameters;

};

class FMobileUberPPEmulationPixelShader : public FMobileUberPPEmulationPixelShaderBase
{
	DECLARE_SHADER_TYPE(FMobileUberPPEmulationPixelShader,Global);
public:
	FMobileUberPPEmulationPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
	  FMobileUberPPEmulationPixelShaderBase(Initializer) {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("INVERT_GAMMA_CORRECTION"), TEXT("0"));
	}
private:
	FMobileUberPPEmulationPixelShader() {}
};

IMPLEMENT_SHADER_TYPE(,FMobileUberPPEmulationPixelShader,TEXT("MobileUberPostProcessEmulationPixelShader"),TEXT("Main"),SF_Pixel,0,0);


class FMobileUberPPEmulationPixelShaderInvertGamma : public FMobileUberPPEmulationPixelShaderBase
{
	DECLARE_SHADER_TYPE(FMobileUberPPEmulationPixelShaderInvertGamma,Global);
public:
	FMobileUberPPEmulationPixelShaderInvertGamma(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
	  FMobileUberPPEmulationPixelShaderBase(Initializer) {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("INVERT_GAMMA_CORRECTION"), TEXT("1"));
	}
private:
	FMobileUberPPEmulationPixelShaderInvertGamma() {}
};

IMPLEMENT_SHADER_TYPE(,FMobileUberPPEmulationPixelShaderInvertGamma,TEXT("MobileUberPostProcessEmulationPixelShader"),TEXT("Main"),SF_Pixel,0,0);

/** 
 * Determines whether to gamma correction is currently enabled
 */
UBOOL ShouldHandleGammaInPost(void)
{
#if WITH_EDITOR
	if (GEmulateMobileRendering)
	{
		return FMobileEmulationMaterialManager::GetManager()->GetGammaCorrectionEnabled();
	}
#endif

	return FALSE;
}

/**
 * Gathers the mobile DOF effect into the specified filter buffer.
 *
 * @param DestSurface	Destination surface
 * @param SrcTexture	Source texture
 * @param DstRect		Destination rectangle, in pixels within the destination surface
 * @param SrcRect		Source rectangle, in pixels within the source texture
 */
void GatherMobileDOF(
	FViewInfo& View,
	const FPostProcessSettings& Settings,
	FSceneRenderTargetIndex DestTarget,
	const FTexture2DRHIParamRef& SrcTexture,
	FIntRect DstRect,
	FIntRect SrcRect
	)
{
	SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("GatherMobileDOF"));

	RHISetRenderTarget(GSceneRenderTargets.GetFilterColorSurface(DestTarget), FSurfaceRHIRef());

	// On a tiled renderer, it's important for performance to clear a surface
	// after binding, if the intension is to overwrite all the pixels
	if (GMobileTiledRenderer)
	{
		RHIClear(TRUE,FLinearColor(0,0,0,0),FALSE,0,FALSE,0);
	}

	RHISetViewport(DstRect.Min.X, DstRect.Min.Y, 0.0f, DstRect.Max.X, DstRect.Max.Y, 1.0f);

	FShader* VertexShader = NULL;
	FShader* PixelShader = NULL;
	FPixelShaderRHIParamRef PixelShaderRef(NULL);

	FMobileUberPostProcessParameters* PostProcessParameters = &GMobileUberPostProcessParameters;
	checkSlow(PostProcessParameters);

	enum GlobalShaderPostOptions
	{
		GSPO_BloomOnly,
		GSPO_BloomAndDOF,

		MAX_GSPO
	};

	static FGlobalBoundShaderState MobileBoundShaderState[MAX_GSPO];
	static FGlobalBoundShaderState EmulationBoundShaderState[MAX_GSPO];
	static FGlobalBoundShaderState EmulationGammaBoundShaderState[MAX_GSPO];


	FGlobalBoundShaderState* ShaderState = MobileBoundShaderState;

	if (GEmulateMobileRendering) 
	{
		TShaderMapRef<FMobileUberPPEmulationVertexShader> EmulationVertexShader(GetGlobalShaderMap());
		VertexShader = *EmulationVertexShader;
		TShaderMapRef<FMobileGatherDOFPixelShader> EmulationPixelShader(GetGlobalShaderMap());
		TShaderMapRef<FMobileGatherDOFPixelShaderInvertGamma> EmulationPixelShaderInvertGamma(GetGlobalShaderMap());

		if (ShouldHandleGammaInPost())
		{
			PixelShader = *EmulationPixelShaderInvertGamma;
			PostProcessParameters = &EmulationPixelShaderInvertGamma->GetPostProcessParameters();
			ShaderState = EmulationGammaBoundShaderState;
		}
		else
		{
			PixelShader = *EmulationPixelShader;
			PostProcessParameters = &EmulationPixelShader->GetPostProcessParameters();
			ShaderState = EmulationBoundShaderState;
		}

		PixelShaderRef = PixelShader->GetPixelShader();		
		check(PostProcessParameters);
	}

	// Set up textures and MinZ_MaxZRatio
	PostProcessParameters->SceneTextureParamsGatherDOF.Set(&View, PixelShader, SF_Point);

	// Set DOF parameters
	PostProcessParameters->SetDOFParameters( PixelShaderRef, View, Settings );

	// Set Bloom parameters
	PostProcessParameters->SetBloomParameters( PixelShaderRef, View, Settings );

	SetTextureParameterDirectly(PixelShaderRef, PostProcessParameters->InputTexture, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), SrcTexture);

	// Select the appropriate gather shader depending on whether DOF is enabled
	GlobalShaderPostOptions ShaderOption = GSPO_BloomAndDOF;
	EMobileGlobalShaderType ShaderType = EGST_DOFAndBloomGather;
	if (!Settings.bEnableDOF)
	{
		ShaderOption = GSPO_BloomOnly;
		ShaderType = EGST_BloomGather;
	}
	SetGlobalBoundShaderState( ShaderState[ShaderOption], GFilterVertexDeclaration.VertexDeclarationRHI, VertexShader, PixelShader, sizeof(FFilterVertex), NULL, ShaderType );

	const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
	const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();

	DrawDenormalizedQuad(
		0, 0, GSceneRenderTargets.GetFilterBufferSizeX(), GSceneRenderTargets.GetFilterBufferSizeY(),
		0, 0, GSceneRenderTargets.GetFilterBufferSizeX(), GSceneRenderTargets.GetFilterBufferSizeY(),
		DstRect.Width(), DstRect.Height(),
		SrcRect.Width(), SrcRect.Height());

	if (GEmulateMobileRendering)
	{
		RHICopyToResolveTarget(GSceneRenderTargets.GetFilterColorSurface(DestTarget), FALSE, FResolveParams(FResolveRect(),CubeFace_PosX,GSceneRenderTargets.GetFilterColorTexture(DestTarget)));
	}

}

/**
 * Applies the mobile DOF effect to the back buffer.
 *
 * @param View				Current view
 * @param SrcFilterBuffer	Source filter buffer, containing a blurred version of SceneColor
 */
void ApplyMobileDOF( FViewInfo& View, const FPostProcessSettings& Settings, FSceneRenderTargetIndex SrcFilterBuffer, UBOOL bRenderToBackBuffer )
{
	SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("ApplyPostProcess"));

	// Render to the final render target
	if (bRenderToBackBuffer)
	{
		GSceneRenderTargets.BeginRenderingBackBuffer( RTUsage_FullOverwrite );
	}
	else
	{
		GSceneRenderTargets.BeginRenderingSceneColor(RTUsage_FullOverwrite);
	}

	// On a tiled renderer, it's important for performance to clear a surface
	// after binding, if the intension is to overwrite all the pixels
	if (GMobileTiledRenderer || View.bForceClear)
	{
		RHIClear(TRUE,FLinearColor(0,0,0,0),FALSE,0,FALSE,0);
	}
	
	const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
	const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();

	RHISetViewport(0, 0, 0.0f, BufferSizeX, BufferSizeY, 1.0f);

	FShader* VertexShader = NULL;
	FShader* PixelShader = NULL;
	FPixelShaderRHIParamRef PixelShaderRef(NULL);

	FMobileUberPostProcessParameters* PostProcessParameters = &GMobileUberPostProcessParameters;

	static FGlobalBoundShaderState EmulationBoundShaderState;
	static FGlobalBoundShaderState EmulationGammaBoundShaderState;

	FGlobalBoundShaderState* ShaderState = &EmulationBoundShaderState;

	if (GEmulateMobileRendering) 
	{
		TShaderMapRef<FMobileUberPPEmulationVertexShader> EmulationVertexShader(GetGlobalShaderMap());
		VertexShader = *EmulationVertexShader;
		TShaderMapRef<FMobileUberPPEmulationPixelShader> EmulationPixelShader(GetGlobalShaderMap());
		TShaderMapRef<FMobileUberPPEmulationPixelShaderInvertGamma> EmulationPixelShaderInvertGamma(GetGlobalShaderMap());

		if (ShouldHandleGammaInPost())
		{
			PixelShader = *EmulationPixelShaderInvertGamma;
			PostProcessParameters = &EmulationPixelShaderInvertGamma->GetPostProcessParameters();
			ShaderState = &EmulationGammaBoundShaderState;
		}
		else
		{
			PixelShader = *EmulationPixelShader;
			PostProcessParameters = &EmulationPixelShader->GetPostProcessParameters();
			ShaderState = &EmulationBoundShaderState;
		}

		PixelShaderRef = PixelShader->GetPixelShader();
	}
	checkSlow(PostProcessParameters);

	// Set up textures and MinZ_MaxZRatio
	PostProcessParameters->SceneTextureParamsApplyDOF.Set(&View, PixelShader, SF_Point);

	// Manually set the post-process input texture
	const FTexture2DRHIRef& SourceTextureRHI = GSceneRenderTargets.GetFilterColorTexture( SrcFilterBuffer );  

	// Set DOF parameters
	PostProcessParameters->SetDOFParameters( PixelShaderRef, View, Settings );

	SetTextureParameterDirectly(PixelShaderRef, PostProcessParameters->InputDOFTexture, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), SourceTextureRHI);

	// Set Color Grading parameters
	UBOOL bForceSetParameters = GEmulateMobileRendering ? TRUE : FALSE;
	UBOOL bDoColorGrading = PostProcessParameters->SetColorGradingParameters( PixelShaderRef, View, Settings, bForceSetParameters );

	// Different shaders depending on combinations of features.
	if(GEmulateMobileRendering)
	{
		check(ShaderState);
		SetGlobalBoundShaderState( *ShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, VertexShader, PixelShader, sizeof(FFilterVertex), NULL, EGST_MobileUberPostProcess1 );
	}
	else if (!bDoColorGrading && !Settings.bEnableDOF && Settings.bEnableBloom)
	{
		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState( BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, VertexShader, PixelShader, sizeof(FFilterVertex), NULL, EGST_MobileUberPostProcess1 );
	}
	else if(!bDoColorGrading && Settings.bEnableDOF)
	{
		//     !bDoColorGrading && Settings.bEnableDOF && Settings.bEnableBloom
		// and !bDoColorGrading && Settings.bEnableDOF && !Settings.bEnableBloom
		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState( BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, VertexShader, PixelShader, sizeof(FFilterVertex), NULL, EGST_MobileUberPostProcess3 );
	}
	else if(bDoColorGrading && !Settings.bEnableDOF && !Settings.bEnableBloom)
	{
		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState( BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, VertexShader, PixelShader, sizeof(FFilterVertex), NULL, EGST_MobileUberPostProcess4 );
	}
	else if(bDoColorGrading && !Settings.bEnableDOF && Settings.bEnableBloom)
	{
		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState( BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, VertexShader, PixelShader, sizeof(FFilterVertex), NULL, EGST_MobileUberPostProcess5 );
	}
	else if(bDoColorGrading && Settings.bEnableDOF)
	{
		//     bDoColorGrading && Settings.bEnableDOF && Settings.bEnableBloom
		// and bDoColorGrading && Settings.bEnableDOF && !Settings.bEnableBloom
		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState( BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, VertexShader, PixelShader, sizeof(FFilterVertex), NULL, EGST_MobileUberPostProcess7 );
	}

	INT UnscaledViewX = View.X;
	INT UnscaledViewY = View.Y;
	UINT UnscaledViewSizeX = View.SizeX;
	UINT UnscaledViewSizeY = View.SizeY;

	if (bRenderToBackBuffer)
	{
		// Set the source texture to be bilinear filtering if upscaling
		if (GSystemSettings.NeedsUpscale())
		{
			SetTextureParameterDirectly(
				PixelShaderRef,
				PostProcessParameters->SceneTextureParamsApplyDOF.SceneColorTextureParameter,
				//use linear filtering to get a smoother upsample
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				GSceneRenderTargets.GetSceneColorLDRTexture()
				);
		}	

		// convert view constraints to their unscaled form for the final render to the backbuffer
		GSystemSettings.UnScaleScreenCoords(
			UnscaledViewX, UnscaledViewY, 
			UnscaledViewSizeX, UnscaledViewSizeY, 
			View.X, View.Y, 
			View.SizeX, View.SizeY);
	}

	DrawDenormalizedQuad(
		UnscaledViewX, UnscaledViewY, 
		UnscaledViewSizeX, UnscaledViewSizeY,
		View.RenderTargetX, View.RenderTargetY, 
		View.RenderTargetSizeX, View.RenderTargetSizeY,
		BufferSizeX, BufferSizeY,
		BufferSizeX, BufferSizeY);
}

#endif

/*-----------------------------------------------------------------------------
	FMobilePostProcessSceneProxy
-----------------------------------------------------------------------------*/

#if WITH_MOBILE_RHI
FMobilePostProcessSceneProxy::FMobilePostProcessSceneProxy(const FViewInfo &View)
	: FPostProcessSceneProxy(0), Settings(*View.PostProcessSettings)
{
	if((View.Family->ShowFlags & SHOW_PostProcess)
	&& View.Family->ShouldPostProcess() )
	{
		Settings.bEnableDOF = Settings.bEnableDOF && GSystemSettings.bAllowDepthOfField;
		Settings.bEnableBloom = Settings.bEnableBloom && GSystemSettings.bAllowBloom;

		if(!TEST_PROFILEEXSTATE(0x2, View.Family->CurrentRealTime))
		{
			// we want to profile with the feature off
			Settings.bEnableDOF = FALSE;
		}

		if(!TEST_PROFILEEXSTATE(0x4, View.Family->CurrentRealTime))
		{
			// we want to profile with the feature off
			Settings.bEnableBloom = FALSE;
		}
	}
	else
	{
		Settings.bEnableDOF = FALSE;
		Settings.bEnableBloom = FALSE;
	}

	// "Hide" the color grading effect if it shouldn't be visible
	if(!(View.Family->ShowFlags & SHOW_PostProcess)
	|| !GSystemSettings.bAllowMobileColorGrading)
	{
		Settings.MobileColorGrading.Blend = 0.0f;
	}

	if(!Settings.bEnableBloom)
	{
		// We don't have the shader permutation for DepthOfField without Bloom
		Settings.Bloom_Scale = 0.0f;
	}
}

UBOOL FMobilePostProcessSceneProxy::IsColorGradingNeeded() const
{
	const FLOAT Nunace = 1.0f / 255.0f;

	if(Settings.MobileColorGrading.Blend < Nunace)
	{
		return FALSE;
	}

	FLinearColor MobileColorGradingHighlights = Settings.MobileColorGrading.HighLights;
	FLinearColor MobileColorGradingMidTonesValue = Settings.MobileColorGrading.MidTones;
	FLinearColor MobileColorGradingShadowsValue = Settings.MobileColorGrading.Shadows;

	MobileColorGradingHighlights.A = 0.0f;
	MobileColorGradingMidTonesValue.A = 0.0f;
	MobileColorGradingShadowsValue.A = 0.0f;

	if(Settings.MobileColorGrading.Desaturation <= Nunace
	&& MobileColorGradingHighlights.Equals(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f), Nunace)
	&& MobileColorGradingMidTonesValue.Equals(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), Nunace)
	&& MobileColorGradingShadowsValue.Equals(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), Nunace))
	{
		return FALSE;
	}

	return TRUE;
}
/**
* Render the post process effect
* Called by the rendering thread during scene rendering
* @param InDepthPriorityGroup - scene DPG currently being rendered
* @param View - current view
* @param CanvasTransform - same canvas transform used to render the scene
* @param LDRInfo - helper information about SceneColorLDR
* @return TRUE if anything was rendered
*/
UBOOL FMobilePostProcessSceneProxy::Render(const FScene* Scene, UINT InDepthPriorityGroup,FViewInfo& View,const FMatrix& CanvasTransform,FSceneColorLDRInfo& LDRInfo)
{
	UBOOL bDirty = FALSE;

	UBOOL bEnableColorGrading = IsColorGradingNeeded();
	UBOOL bUsePostProcessing = (Settings.bEnableDOF || Settings.bEnableBloom || bEnableColorGrading) && (GMobileAllowPostProcess || GEmulateMobileRendering);

	// TODO: support FXAA in editor emulation
	UBOOL bUseFXAA = !GEmulateMobileRendering && (GSystemSettings.MobileFXAAQuality > 0);

	if ( GMobileUberPostProcessParameters.IsInitialized() == FALSE && GUsingMobileRHI )
	{
		GMobileUberPostProcessParameters.InitializeMobileShaderParameters();
	}

	if ( bUsePostProcessing )
	{
		SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("MobilePostProcess"));

		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
		RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
		RHISetBlendState(TStaticBlendState<>::GetRHI());

		const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
		const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();
		const UINT FilterBufferSizeX = GSceneRenderTargets.GetFilterBufferSizeX();
		const UINT FilterBufferSizeY = GSceneRenderTargets.GetFilterBufferSizeY();
		const UINT FilterDownsampleFactor = GSceneRenderTargets.GetFilterDownsampleFactor();

		const UINT DownsampledSizeX = View.RenderTargetSizeX / FilterDownsampleFactor;
		const UINT DownsampledSizeY = View.RenderTargetSizeY / FilterDownsampleFactor;

		if (Settings.bEnableDOF || Settings.bEnableBloom)
		{
			FSceneView* SceneView = NULL; 
			UBOOL bUseBilinearOptimization = TRUE; 

			// single pass 4X down sample TranslucencyBuffer to FilterColor, resolve.
			DrawDownsampledTexture(
				GSceneRenderTargets.GetFilterColorSurface(SRTI_FilterColor0),
				GSceneRenderTargets.GetFilterColorTexture(SRTI_FilterColor0),
				GSceneRenderTargets.GetSceneColorTexture(),
				FIntPoint(0, 0),
				FIntRect(FIntPoint(0, 0), FIntPoint(View.RenderTargetSizeX, View.RenderTargetSizeY)),
				FIntPoint(FilterBufferSizeX, FilterBufferSizeY),
				FIntPoint(View.RenderTargetSizeX, View.RenderTargetSizeY),
				SceneView,	
				4,		// 4 -> 1
				bUseBilinearOptimization,	// bilinear filtering allows 4x less samples
				TRUE	// clear
				);

			// Gather, resolve.
			GatherMobileDOF(
				View,
				Settings,
				SRTI_FilterColor1,
				GSceneRenderTargets.GetFilterColorTexture(SRTI_FilterColor0),
				FIntRect(0, 0, FilterBufferSizeX, FilterBufferSizeY),
				FIntRect(0, 0, FilterBufferSizeX, FilterBufferSizeY)
				);

			// Blur, resolve.
			if(TEST_PROFILEEXSTATE(0x20, View.Family->CurrentRealTime))
			{	
				GaussianBlurFilterBuffer(
					View,
					View.SizeX,
					FilterBufferSizeX,
					FilterBufferSizeY,
					Settings.MobilePostProcess.Mobile_BlurAmount,
					1.0f,
					SRTI_FilterColor1,
					FVector2D(0, 0),
					FVector2D(1, 1)
					);
			}
		}

		// Apply mobile DOF, opaque full-screen quad, blending in shader.
		ApplyMobileDOF(
			View,
			Settings,
			SRTI_FilterColor1,
			!bUseFXAA
			);

		if( View.Family->bResolveScene )
		{
			// We gamma correct ourselves when needed, so tell other passes we don't need it
			View.bUseLDRSceneColor = TRUE; 
		}

		bDirty = TRUE;
	}

	if (bUseFXAA)
	{
		SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("FXAA"));
		if (bUsePostProcessing)
		{
			GSceneRenderTargets.FinishRenderingSceneColor(TRUE);
		}
		GSceneRenderTargets.BeginRenderingBackBuffer( RTUsage_FullOverwrite );
		if (GMobileTiledRenderer)
		{
			RHIClear(TRUE,FLinearColor(0,0,0,0),FALSE,0,FALSE,0);
		}

		INT UnscaledRenderTargetX = 0;
		INT UnscaledRenderTargetY = 0;
		UINT UnscaledRenderTargetSizeX = 0;
		UINT UnscaledRenderTargetSizeY = 0;

		const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
		const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();

		RHISetViewport(0, 0, 0.0f, BufferSizeX, BufferSizeY, 1.0f);

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState( BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, NULL, NULL, sizeof(FFilterVertex), NULL, EGST_FXAA );

		INT UnscaledViewX = 0;
		INT UnscaledViewY = 0;
		UINT UnscaledViewSizeX = 0;
		UINT UnscaledViewSizeY = 0;

		// convert view constraints to their unscaled form for the final render to the backbuffer
		GSystemSettings.UnScaleScreenCoords(
			UnscaledViewX, UnscaledViewY, 
			UnscaledViewSizeX, UnscaledViewSizeY, 
			View.X, View.Y, 
			View.SizeX, View.SizeY);

		GMobileUberPostProcessParameters.SetFXAAParameters(View);

		// Manually set the post-process input texture
		FPixelShaderRHIRef PixelShader(NULL);	// not used for mobile
		SetTextureParameterDirectly(PixelShader, GMobileUberPostProcessParameters.InputTexture, 
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), GSceneRenderTargets.GetSceneColorTexture());
			
		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
		RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
		RHISetBlendState(TStaticBlendState<>::GetRHI());

		DrawDenormalizedQuad(
			UnscaledViewX, UnscaledViewY, 
			UnscaledViewSizeX, UnscaledViewSizeY,
			View.RenderTargetX, View.RenderTargetY, 
			View.RenderTargetSizeX, View.RenderTargetSizeY,
			BufferSizeX, BufferSizeY,
			BufferSizeX, BufferSizeY);

		if( View.Family->bResolveScene && !View.bUseLDRSceneColor)
		{
			// We gamma correct ourselves when needed, so tell other passes we don't need it
			View.bUseLDRSceneColor = TRUE; 
		}

		bDirty = TRUE;
	}

	return bDirty;
}
#endif // WITH_MOBILE_RHI


