/*=============================================================================
DepthDependentHaloRendering.cpp: Contains definitions for rendering the depth dependent halos (for wireframe).
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"

FDepthDependentHaloSettings GDepthDependentHaloSettings_RenderThread;


/**
* Pixel shader that is used to map shader complexity stored in scene color into color.
*/
class FDepthDependentHaloApplyPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDepthDependentHaloApplyPixelShader,Global);
public:

	/** 
	* Constructor - binds all shader params
	* @param Initializer - init data from shader compiler
	*/
	FDepthDependentHaloApplyPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		SceneTextureParams.Bind(Initializer.ParameterMap);
		HaloDepthConstants.Bind(Initializer.ParameterMap,TEXT("HaloDepthConstants"), TRUE);
		InverseSceneTextureSize.Bind(Initializer.ParameterMap,TEXT("InverseSceneTextureSize"), TRUE);
		BackgroundColor.Bind(Initializer.ParameterMap,TEXT("BackgroundColor"), TRUE);
	}

	FDepthDependentHaloApplyPixelShader() 
	{
	}

	/**
	* Sets the current pixel shader params
	* @param View - current view
	* @param ShadowInfo - projected shadow info for a single light
	*/
	virtual void SetParameters(const FSceneView* View)
	{
		//send down scene texture information
		SceneTextureParams.Set(View,this);

		//send down background color
		SetPixelShaderValue(GetPixelShader(),BackgroundColor,View->BackgroundColor);

		//send down depth constants for comparison
		SetPixelShaderValue(GetPixelShader(),HaloDepthConstants,FVector4(GDepthDependentHaloSettings_RenderThread.FadeStartDistance,
																		 1.0f/GDepthDependentHaloSettings_RenderThread.FadeGradientDistance, 
																		 GDepthDependentHaloSettings_RenderThread.DepthAcceptanceFactor, 
																		 0.0f));

		FLOAT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
		FLOAT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();

		//send down buffer size information
		SetPixelShaderValue(GetPixelShader(),InverseSceneTextureSize,FVector4(1.0f/BufferSizeX,1.0f/BufferSizeY, BufferSizeX/View->RenderTargetSizeX,BufferSizeY/View->RenderTargetSizeY));
	}

	/**
	* @param Platform - hardware platform
	* @return TRUE if this shader should be cached
	*/
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		//turn off for xbox, ps3
		return AllowDebugViewmodes();
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	/**
	* Serialize shader parameters for this shader
	* @param Ar - archive to serialize with
	*/
	UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SceneTextureParams;
		Ar << InverseSceneTextureSize;
		Ar << HaloDepthConstants;
		Ar << BackgroundColor;

		return bShaderHasOutdatedParameters;
	}

private:

	FSceneTextureShaderParameters SceneTextureParams;

	FShaderParameter InverseSceneTextureSize;
	FShaderParameter HaloDepthConstants;

	FShaderParameter BackgroundColor;
};

IMPLEMENT_SHADER_TYPE(,FDepthDependentHaloApplyPixelShader,TEXT("DepthDependentHaloApplyPixelShader"),TEXT("Main"),SF_Pixel,0,0);

//reuse the generic filter vertex declaration
extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

FGlobalBoundShaderState FSceneRenderer::DepthDependentHaloBoundShaderState;

/**
 * Finish rendering a view, by rendering depth dependent halos around large changes in depth
 * @param View - The view to process
 */
UBOOL FSceneRenderer::RenderDepthDependentHalo(const FViewInfo* View)
{
	//not enabled. do not use this post shader
	if (!GDepthDependentHaloSettings_RenderThread.bEnablePostEffect)
	{
		return FALSE;
	}

	if (!AllowDebugViewmodes())
	{
		return FALSE;
	}

	// Render to an off screen color buffer for further post (brightness)
	GSceneRenderTargets.BeginRenderingSceneColor();

	// Deferred the clear until here so the garbage left in the non rendered regions by the post process effects do not show up
	if( ViewFamily.bDeferClear )
	{
		RHIClear(  TRUE, FLinearColor::Black, FALSE, 0.0f, FALSE, 0 );
		ViewFamily.bDeferClear = FALSE;
	}

	// turn off culling and blending
	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
	RHISetBlendState(TStaticBlendState<>::GetRHI());

	// turn off depth reads/writes
	RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());

	//reuse this generic vertex shader
	TShaderMapRef<FLDRExtractVertexShader> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FDepthDependentHaloApplyPixelShader> PixelShader(GetGlobalShaderMap());

	SetGlobalBoundShaderState( DepthDependentHaloBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));

	PixelShader->SetParameters(View);

	// Draw a quad mapping the blurred pixels in the filter buffer to the scene color buffer.
	DrawDenormalizedQuad(
		View->RenderTargetX,View->RenderTargetY, View->RenderTargetSizeX,View->RenderTargetSizeY,
		View->RenderTargetX,View->RenderTargetY, View->RenderTargetSizeX,View->RenderTargetSizeY,
		GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY(),
		GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());

	// Resolve the scene color buffer.  Note that nothing needs to be done if we are writing directly to the view's render target
	GSceneRenderTargets.FinishRenderingSceneColor();

	return TRUE;
}

