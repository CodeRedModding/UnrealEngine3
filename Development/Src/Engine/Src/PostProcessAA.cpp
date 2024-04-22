/*=============================================================================
	PostprocessAA.cpp: For post process anti aliasing.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessAA.h"

extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

const FPostProcessAA* FPostProcessAA::DeferredObject = 0;

/*-----------------------------------------------------------------------------
	FMLAAVertexShader
-----------------------------------------------------------------------------*/
class FMLAAVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMLAAVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return IsPCPlatform(Platform);
	}

	/** Default constructor. */
	FMLAAVertexShader() {}

	/** Initialization constructor. */
	FMLAAVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		return FShader::Serialize(Ar);
	}
};

/*-----------------------------------------------------------------------------
	FSRGBMLAAEdgeDetectionPixelShader
-----------------------------------------------------------------------------*/
class FSRGBMLAAEdgeDetectionPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSRGBMLAAEdgeDetectionPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return IsPCPlatform(Platform);
	}

	/** Default constructor. */
	FSRGBMLAAEdgeDetectionPixelShader() {}

	/** Initialization constructor. */
	FSRGBMLAAEdgeDetectionPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		MLAAParameter.Bind(Initializer.ParameterMap,TEXT("gParam"),TRUE);		
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << MLAAParameter;
		return bShaderHasOutdatedParameters;
	}

	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderParameter MLAAParameter;	
private:
};

/*-----------------------------------------------------------------------------
	FMLAAComputeLineLengthPixelShader
-----------------------------------------------------------------------------*/
class FMLAAComputeLineLengthPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMLAAComputeLineLengthPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return IsPCPlatform(Platform);
	}

	/** Default constructor. */
	FMLAAComputeLineLengthPixelShader() {}

	/** Initialization constructor. */
	FMLAAComputeLineLengthPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		EdgeMaskTextureParameter.Bind(Initializer.ParameterMap, TEXT("EdgeMaskTexture"), TRUE);
		MLAAParameter.Bind(Initializer.ParameterMap,TEXT("gParam"),TRUE);		
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << EdgeMaskTextureParameter;
		Ar << MLAAParameter;
		return bShaderHasOutdatedParameters;
	}

	FShaderResourceParameter EdgeMaskTextureParameter;
	FShaderParameter MLAAParameter;
private:
};

/*-----------------------------------------------------------------------------
	FSRGBMLAABlendPixelShader
-----------------------------------------------------------------------------*/
class FSRGBMLAABlendPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSRGBMLAABlendPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return IsPCPlatform(Platform);
	}

	/** Default constructor. */
	FSRGBMLAABlendPixelShader() {}

	/** Initialization constructor. */
	FSRGBMLAABlendPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);		
		EdgeCountTextureParameter.Bind(Initializer.ParameterMap, TEXT("EdgeCountTexture"), TRUE);
		MLAAParameter.Bind(Initializer.ParameterMap,TEXT("gParam"),TRUE);	
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << EdgeCountTextureParameter;
		Ar << MLAAParameter;
		return bShaderHasOutdatedParameters;
	}

	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderResourceParameter EdgeCountTextureParameter;
	FShaderParameter MLAAParameter;	
private:
};

/*-----------------------------------------------------------------------------
	FFXAAVertexShader
-----------------------------------------------------------------------------*/
class FFXAAVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FFXAAVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	/** Default constructor. */
	FFXAAVertexShader() {}

	/** Initialization constructor. */
	FFXAAVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		fxaaQualityRcpFrameParameter.Bind(Initializer.ParameterMap, TEXT("fxaaQualityRcpFrame"), TRUE);
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << fxaaQualityRcpFrameParameter;
		return bShaderHasOutdatedParameters;
	}

	FShaderParameter fxaaQualityRcpFrameParameter;
};

/*-----------------------------------------------------------------------------
	FFXAABlendPixelShader
-----------------------------------------------------------------------------*/
template <UINT FXAAPreset>
class FFXAABlendPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FFXAABlendPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		// textureGrad (tex2Dgrad in D3D) requires a newer OpenGL version so we don't support this shader
		return Platform != SP_PCOGL;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("FXAA_PRESET"), *FString::Printf(TEXT("%u"), FXAAPreset));
	}

	/** Default constructor. */
	FFXAABlendPixelShader() {}

	/** Initialization constructor. */
	FFXAABlendPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		fxaaConsole360TexExpBiasNegOneParameter.Bind(Initializer.ParameterMap,TEXT("fxaaConsole360TexExpBiasNegOne"), TRUE);
		fxaaConsole360TexExpBiasNegTwoParameter.Bind(Initializer.ParameterMap,TEXT("fxaaConsole360TexExpBiasNegTwo"), TRUE);
		fxaaQualityRcpFrameParameter.Bind(Initializer.ParameterMap, TEXT("fxaaQualityRcpFrame"), TRUE);
		fxaaConsoleRcpFrameOptParameter.Bind(Initializer.ParameterMap, TEXT("fxaaConsoleRcpFrameOpt"), TRUE);
		fxaaConsoleRcpFrameOpt2Parameter.Bind(Initializer.ParameterMap, TEXT("fxaaConsoleRcpFrameOpt2"), TRUE);
		fxaaConsole360RcpFrameOpt2Parameter.Bind(Initializer.ParameterMap, TEXT("fxaaConsole360RcpFrameOpt2"), TRUE);
		fxaaQualitySubpixParameter.Bind(Initializer.ParameterMap, TEXT("fxaaQualitySubpix"), TRUE);
		fxaaQualityEdgeThresholdParameter.Bind(Initializer.ParameterMap, TEXT("fxaaQualityEdgeThreshold"), TRUE);
		fxaaQualityEdgeThresholdMinParameter.Bind(Initializer.ParameterMap, TEXT("fxaaQualityEdgeThresholdMin"), TRUE);
		fxaaConsoleEdgeSharpnessParameter.Bind(Initializer.ParameterMap, TEXT("fxaaConsoleEdgeSharpness"), TRUE);
		fxaaConsoleEdgeThresholdParameter.Bind(Initializer.ParameterMap, TEXT("fxaaConsoleEdgeThreshold"), TRUE);
		fxaaConsoleEdgeThresholdMinParameter.Bind(Initializer.ParameterMap, TEXT("fxaaConsoleEdgeThresholdMin"), TRUE);
		fxaaConsole360ConstDirParameter.Bind(Initializer.ParameterMap, TEXT("fxaaConsole360ConstDir"), TRUE);
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SceneTextureParameters 
			<< fxaaConsole360TexExpBiasNegOneParameter << fxaaConsole360TexExpBiasNegTwoParameter
			<< fxaaQualityRcpFrameParameter	<< fxaaConsoleRcpFrameOptParameter << fxaaConsoleRcpFrameOpt2Parameter << fxaaConsole360RcpFrameOpt2Parameter
			<< fxaaQualitySubpixParameter << fxaaQualityEdgeThresholdParameter << fxaaQualityEdgeThresholdMinParameter
			<< fxaaConsoleEdgeSharpnessParameter << fxaaConsoleEdgeThresholdParameter << fxaaConsoleEdgeThresholdMinParameter
			<< fxaaConsole360ConstDirParameter;
		return bShaderHasOutdatedParameters;
	}

	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderResourceParameter fxaaConsole360TexExpBiasNegOneParameter;
	FShaderResourceParameter fxaaConsole360TexExpBiasNegTwoParameter;
	FShaderParameter fxaaQualityRcpFrameParameter;
	FShaderParameter fxaaConsoleRcpFrameOptParameter;
	FShaderParameter fxaaConsoleRcpFrameOpt2Parameter;
	FShaderParameter fxaaConsole360RcpFrameOpt2Parameter;
	FShaderParameter fxaaQualitySubpixParameter;
	FShaderParameter fxaaQualityEdgeThresholdParameter;
	FShaderParameter fxaaQualityEdgeThresholdMinParameter;
	FShaderParameter fxaaConsoleEdgeSharpnessParameter;
	FShaderParameter fxaaConsoleEdgeThresholdParameter;
	FShaderParameter fxaaConsoleEdgeThresholdMinParameter;
	FShaderParameter fxaaConsole360ConstDirParameter;
};

IMPLEMENT_SHADER_TYPE(,FMLAAVertexShader,TEXT("MLAAShader"),TEXT("MLAA_VertexMain"),SF_Vertex,0,0);
IMPLEMENT_SHADER_TYPE(,FMLAAComputeLineLengthPixelShader,TEXT("MLAAShader"),TEXT("MLAA_ComputeLineLength_PS"),SF_Pixel,0,0);
IMPLEMENT_SHADER_TYPE(,FSRGBMLAAEdgeDetectionPixelShader,TEXT("MLAAShader"),TEXT("MLAA_SeperatingLines_PS"),SF_Pixel,0,0);
IMPLEMENT_SHADER_TYPE(,FSRGBMLAABlendPixelShader,TEXT("MLAAShader"),TEXT("MLAA_BlendColor_PS"),SF_Pixel,0,0);

IMPLEMENT_SHADER_TYPE(,FFXAAVertexShader,TEXT("FXAAShader"),TEXT("FxaaVS"),SF_Vertex,0,0);
// #define avoids a lot of code duplication
#define VARIATION(FXAAPreset) typedef FFXAABlendPixelShader<FXAAPreset> FFXAABlendPixelShader##FXAAPreset; \
	IMPLEMENT_SHADER_TYPE(template<>,FFXAABlendPixelShader##FXAAPreset,TEXT("FXAAShader"),TEXT("FxaaPS"),SF_Pixel,0,0);
VARIATION(0) VARIATION(1) VARIATION(2) VARIATION(3) VARIATION(4) VARIATION(5)
//
#undef VARIATION

/*-----------------------------------------------------------------------------
FPostProcessAA
-----------------------------------------------------------------------------*/
/** 
* Initialization constructor. 
* @param InEffect - PostProcessEffect where the techniques properties are stored in
*/
FPostProcessAA::FPostProcessAA(const UUberPostProcessEffect* InEffect, const FPostProcessSettings* WorldSettings)
{		
	EdgeDetectionThreshold = InEffect->EdgeDetectionThreshold;
	
	// set PostProcessAAType
	{
		PostProcessAAType = (EPostProcessAAType)InEffect->PostProcessAAType;

#if !FINAL_RELEASE
		// console variable override
		{
			static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("PostprocessAAType")); 
			INT Value = CVar ? CVar->GetInt() : -1;

			if(Value >= 0)
			{
				PostProcessAAType = (EPostProcessAAType)Value;
			}
		}
#endif

		UBOOL bAllowMLAA = IsValidRef(GSceneRenderTargets.GetRenderTargetSurface(MLAAEdgeMask));

		if(PostProcessAAType == PostProcessAA_MLAA && !bAllowMLAA)
		{
			// MLAA requires extra render targets
			PostProcessAAType = PostProcessAA_Off;
		}

		if(PostProcessAAType == PostProcessAA_MLAA && !IsPCPlatform(GRHIShaderPlatform))
		{
			// Shader doesn't compile on Xbox360 and the extra memory doesn't make it a good option currently
			PostProcessAAType = PostProcessAA_Off;
		}

		if(GRHIShaderPlatform == SP_PCOGL && PostProcessAAType != PostProcessAA_MLAA)
		{
			// textureGrad (tex2Dgrad in D3D) requires a newer OpenGL version so we don't support this shader
			PostProcessAAType = PostProcessAA_Off;
		}
	}
}

/**
* The first pass of MLAA. It renders edge detection pass which detect the aliasing edge and write the mask value
* to a render target.
*/
void FPostProcessAA::RenderEdgeDetectingPass(FViewInfo& View) const
{	
	// Render to the MLAA edge mask render target.
	RHISetRenderTarget(GSceneRenderTargets.GetRenderTargetSurface(MLAAEdgeMask), FSurfaceRHIRef());
	
	// Set the filter vertex shader.
	TShaderMapRef<FMLAAVertexShader> EdgeDetectionVertexShader(GetGlobalShaderMap());

	// Set the edge detection pixel shader.
	{
		TShaderMapRef<FSRGBMLAAEdgeDetectionPixelShader> EdgeDetectionPixelShader(GetGlobalShaderMap());	

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState( BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *EdgeDetectionVertexShader, *EdgeDetectionPixelShader, sizeof(FFilterVertex));

		FVector4 Value(GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY(), 1.0f / EdgeDetectionThreshold, 0.0f);
		SetPixelShaderValues(
			EdgeDetectionPixelShader->GetPixelShader(),
			EdgeDetectionPixelShader->MLAAParameter,
			&Value,
			1);
		EdgeDetectionPixelShader->SceneTextureParameters.Set(&View, *EdgeDetectionPixelShader, SF_Point);
	}

	RHISetColorWriteMask(CW_RED);
	DrawDenormalizedQuad(
		View.RenderTargetX,View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
		View.RenderTargetX,View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
		GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY(),
		GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());

	// Resolve the edge mask render target.
	RHICopyToResolveTarget(GSceneRenderTargets.GetRenderTargetSurface(MLAAEdgeMask), TRUE, FResolveParams());
}

/**
* The second pass of MLAA. It renders edge length computing pass which compute the edge length according 
* to data from the first pass.
*/
void FPostProcessAA::RenderComputeEdgeLengthPass(FViewInfo& View) const
{
	// Render to the edge count render target.
	RHISetRenderTarget(GSceneRenderTargets.GetRenderTargetSurface(MLAAEdgeCount), FSurfaceRHIRef());
	
	// Set the gather vertex shader.
	TShaderMapRef<FMLAAVertexShader> VertexShader(GetGlobalShaderMap());

	// Set the edge length computing pixel shader.
	TShaderMapRef<FMLAAComputeLineLengthPixelShader> PixelShader(GetGlobalShaderMap());	

	static FGlobalBoundShaderState BoundShaderState;
	SetGlobalBoundShaderState( BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));

	FVector4 Value(GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY(), 1.0f / EdgeDetectionThreshold, 0.0f);
	SetPixelShaderValues(
		PixelShader->GetPixelShader(),
		PixelShader->MLAAParameter,
		&Value,
		1);
	SetTextureParameter(
		PixelShader->GetPixelShader(),
		PixelShader->EdgeMaskTextureParameter,
		TStaticSamplerState<SF_Point>::GetRHI(),
		GSceneRenderTargets.GetRenderTargetTexture(MLAAEdgeMask)
		);		

	RHISetColorWriteMask(CW_RED | CW_GREEN);
	DrawDenormalizedQuad(
		View.RenderTargetX,View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
		View.RenderTargetX,View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
		GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY(),
		GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());

	RHICopyToResolveTarget(GSceneRenderTargets.GetRenderTargetSurface(MLAAEdgeCount), FALSE, FResolveParams());
}

/**
* The third and final pass of MLAA. This pass reads data from previous pass and perform anti-aliasing of edges 
* by blending colors as required.
*/
void FPostProcessAA::RenderBlendColorPass(FViewInfo& View) const
{
	UINT TargetSizeX = GSceneRenderTargets.GetBufferSizeX();
	UINT TargetSizeY = GSceneRenderTargets.GetBufferSizeY();

	{
		// If this is the final effect in chain, render to the view's output render target
		// unless an upscale is needed, in which case render to LDR scene color.
		if(!GSystemSettings.NeedsUpscale()) 
		{
			// Render to back buffer
			GSceneRenderTargets.BeginRenderingBackBuffer();
			TargetSizeX = View.Family->RenderTarget->GetSizeX();
			TargetSizeY = View.Family->RenderTarget->GetSizeY();
		}
		else
		{
			// Render to LDR scene surface
			GSceneRenderTargets.BeginRenderingSceneColorLDR();
		}
	}

	// Set the filter vertex shader.
	TShaderMapRef<FMLAAVertexShader> VertexShader(GetGlobalShaderMap());

	// Set the color blending pixel shader.
	{
		TShaderMapRef<FSRGBMLAABlendPixelShader> PixelShader(GetGlobalShaderMap());

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState( BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));

		FVector4 Value(GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY(), 1.0f / EdgeDetectionThreshold, 0.0f);
		SetPixelShaderValues(
			PixelShader->GetPixelShader(),
			PixelShader->MLAAParameter,
			&Value,
			1);
		SetTextureParameter(
			PixelShader->GetPixelShader(),
			PixelShader->EdgeCountTextureParameter,
			TStaticSamplerState<SF_Point>::GetRHI(),
			GSceneRenderTargets.GetRenderTargetTexture(MLAAEdgeCount)
			);		
		PixelShader->SceneTextureParameters.Set(&View, *PixelShader, SF_Point);
	}

	RHISetColorWriteMask(CW_RGB);

	if( View.bUseLDRSceneColor &&
		(View.X > 0 || View.Y > 0 || View.SizeX < TargetSizeX || View.SizeY < TargetSizeY) )
	{
		// Draw the quad.
		DrawDenormalizedQuad(
			View.X, View.Y, View.SizeX, View.SizeY,
			View.RenderTargetX, View.RenderTargetY, View.RenderTargetSizeX, View.RenderTargetSizeY,
			TargetSizeX, TargetSizeY,
			GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());
	}
	else
	{
		// Draw the quad.
		DrawDenormalizedQuad(
			View.RenderTargetX,View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
			View.RenderTargetX,View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
			TargetSizeX, TargetSizeY,
			GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());
	}

	if(GSystemSettings.NeedsUpscale()) 
	{
		GSceneRenderTargets.FinishRenderingSceneColorLDR();
	}
}

template <UINT FXAAPreset>
void SetFXAAShader(FViewInfo& View)
{
	// Set the filter vertex shader.
	TShaderMapRef<FFXAAVertexShader> VertexShader(GetGlobalShaderMap());

	// Set the color blending pixel shader.
	TShaderMapRef<FFXAABlendPixelShader<FXAAPreset> > PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	SetGlobalBoundShaderState( BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));

	const FPixelShaderRHIParamRef PixelShaderRHI = PixelShader->GetPixelShader();

	PixelShader->SceneTextureParameters.Set(&View, *PixelShader, SF_Bilinear);

	// Todo: 
	// We don't use the shaderpath yet that makes use of those samplers.
	// We would have to setup sampler to scale the color on lookup (expbias),
	// the following commented code is wrong as it does a mip bias.
/*	SetTextureParameterDirectly(
		PixelShaderRHI,
		PixelShader->fxaaConsole360TexExpBiasNegOneParameter,
		TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp, MIPBIAS_LowerResolution_1>::GetRHI(),
		GSceneRenderTargets.GetSceneColorTexture()
		);
	SetTextureParameterDirectly(
		PixelShaderRHI,
		PixelShader->fxaaConsole360TexExpBiasNegTwoParameter,
		TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp, MIPBIAS_LowerResolution_2>::GetRHI(),
		GSceneRenderTargets.GetSceneColorTexture()
		);
*/
	FVector2D InvExtent = FVector2D(1.0f / GSceneRenderTargets.GetBufferSizeX(), 1.0f / GSceneRenderTargets.GetBufferSizeY());

	{
		SetPixelShaderValues(PixelShaderRHI, PixelShader->fxaaQualityRcpFrameParameter, &InvExtent, 1);
		SetVertexShaderValues(VertexShader->GetVertexShader(), VertexShader->fxaaQualityRcpFrameParameter, &InvExtent, 1);
	}

	{
		FLOAT N = 0.5f;
		FVector4 Value(-N * InvExtent.X, -N * InvExtent.Y, N * InvExtent.X, N * InvExtent.Y);
		SetPixelShaderValues(PixelShaderRHI, PixelShader->fxaaConsoleRcpFrameOptParameter, &Value, 1);
	}

	{
		FLOAT N = 2.0f;
		FVector4 Value(-N * InvExtent.X, -N * InvExtent.Y, N * InvExtent.X, N * InvExtent.Y);
		SetPixelShaderValues(PixelShaderRHI, PixelShader->fxaaConsoleRcpFrameOpt2Parameter, &Value, 1);
	}

	{
		FVector4 Value(8.0f * InvExtent.X, 8.0f * InvExtent.Y, -4 * InvExtent.X, -4.0f * InvExtent.Y);
		SetPixelShaderValues(PixelShaderRHI, PixelShader->fxaaConsole360RcpFrameOpt2Parameter, &Value, 1);
	}

	{
		FLOAT Value = 0.75f;
		SetPixelShaderValues(PixelShaderRHI, PixelShader->fxaaQualitySubpixParameter, &Value, 1);
	}

	{
		FLOAT Value = 0.166f;
		SetPixelShaderValues(PixelShaderRHI, PixelShader->fxaaQualityEdgeThresholdParameter, &Value, 1);
	}

	{
		FLOAT Value = 0.0833f;
		SetPixelShaderValues(PixelShaderRHI, PixelShader->fxaaQualityEdgeThresholdMinParameter, &Value, 1);
	}

	{
		FLOAT Value = 8.0f;
		SetPixelShaderValues(PixelShaderRHI, PixelShader->fxaaConsoleEdgeSharpnessParameter, &Value, 1);
	}

	{
		FLOAT Value = 0.125f;
		SetPixelShaderValues(PixelShaderRHI, PixelShader->fxaaConsoleEdgeThresholdParameter, &Value, 1);
	}

	{
		FLOAT Value = 0.05f;
		SetPixelShaderValues(PixelShaderRHI, PixelShader->fxaaConsoleEdgeThresholdMinParameter, &Value, 1);
	}

	{
		FVector4 Value(1.0, -1.0, 0.25, -0.25);
		SetPixelShaderValues(PixelShaderRHI, PixelShader->fxaaConsole360ConstDirParameter, &Value, 1);
	}
}

/**
* The single pass of FXAA (alternative to MLAA).
*/
void FPostProcessAA::RenderFXAA(FViewInfo& View) const
{
	UINT TargetSizeX = GSceneRenderTargets.GetBufferSizeX();
	UINT TargetSizeY = GSceneRenderTargets.GetBufferSizeY();

	// If this is the final effect in chain, render to the view's output render target
	// unless an upscale is needed, in which case render to LDR scene color.
	if(!GSystemSettings.NeedsUpscale()) 
	{
		// Render to back buffer
		GSceneRenderTargets.BeginRenderingBackBuffer();
		TargetSizeX = View.Family->RenderTarget->GetSizeX();
		TargetSizeY = View.Family->RenderTarget->GetSizeY();
	}
	else
	{
		// Render to LDR scene surface
		GSceneRenderTargets.BeginRenderingSceneColorLDR();
	}

	switch(PostProcessAAType)
	{
		case PostProcessAA_FXAA0:	SetFXAAShader<0>(View);	break;
		case PostProcessAA_FXAA1:	SetFXAAShader<1>(View);	break;
		case PostProcessAA_FXAA2:	SetFXAAShader<2>(View);	break;
		case PostProcessAA_FXAA3:	SetFXAAShader<3>(View);	break;
		case PostProcessAA_FXAA4:	SetFXAAShader<4>(View);	break;
		case PostProcessAA_FXAA5:	SetFXAAShader<5>(View);	break;
	}

	if( View.bUseLDRSceneColor &&
		(View.X > 0 || View.Y > 0 || View.SizeX < TargetSizeX || View.SizeY < TargetSizeY) )
	{
		// Draw the quad.
		DrawDenormalizedQuad(
			View.X, View.Y, View.SizeX, View.SizeY,
			View.RenderTargetX, View.RenderTargetY, View.RenderTargetSizeX, View.RenderTargetSizeY,
			TargetSizeX, TargetSizeY,
			GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());
	}
	else
	{
		// Draw the quad.
		DrawDenormalizedQuad(
			View.RenderTargetX,View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
			View.RenderTargetX,View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
			TargetSizeX, TargetSizeY,
			GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());
	}

	if(GSystemSettings.NeedsUpscale()) 
	{
		GSceneRenderTargets.FinishRenderingSceneColorLDR();
	}
}

UBOOL FPostProcessAA::IsEnabled(const FViewInfo& View) const
{
	return PostProcessAAType != PostProcessAA_Off && (View.Family->ShowFlags & SHOW_PostProcessAA);
}

/** set static DeferredObject */
void FPostProcessAA::SetDeferredObject() const
{
	DeferredObject = this;
}

/** get static DeferredObject */
const FPostProcessAA* FPostProcessAA::GetDeferredObject()
{
	const FPostProcessAA* Ret = DeferredObject;

	DeferredObject = 0;

	return Ret;
}

/**
* Render the post process effect
* Called by the rendering thread during scene rendering
* @param View - current view
*/
void FPostProcessAA::Render(FViewInfo& View) const
{
	if (!IsEnabled(View))
	{
		return;
	}

	// No depth tests, no backface culling.		
	RHISetDepthState(TStaticDepthState<FALSE, CF_Always>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHISetBlendState(TStaticBlendState<>::GetRHI());

	if(PostProcessAAType == PostProcessAA_MLAA)
	{
		// MLAA
		SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("MLAA"));		

		RenderEdgeDetectingPass(View);
		RenderComputeEdgeLengthPass(View);
		RenderBlendColorPass(View);

		RHISetColorWriteMask(CW_RGBA);
	}
	else
	{
		// FXAA
		SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("FXAA"));		

		RenderFXAA(View);
	}
}
