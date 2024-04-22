/*=============================================================================
ShaderComplexityRendering.cpp: Contains definitions for rendering the shader complexity viewmode.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"

/**
 * Gets the maximum shader complexity count from the ini settings.
 */
FLOAT GetMaxShaderComplexityCount()
{
	return GEngine->MaxPixelShaderAdditiveComplexityCount;
}

void FShaderComplexityAccumulatePixelShader::SetParameters(
	UINT NumVertexInstructions, 
	UINT NumPixelInstructions)
{
	//normalize the complexity so we can fit it in a low precision scene color which is necessary on some platforms
	const FLOAT NormalizedComplexity = FLOAT(NumPixelInstructions) / GetMaxShaderComplexityCount();

	SetPixelShaderValue( GetPixelShader(), NormalizedComplexityParameter, NormalizedComplexity);
}

IMPLEMENT_SHADER_TYPE(,FShaderComplexityAccumulatePixelShader,TEXT("ShaderComplexityAccumulatePixelShader"),TEXT("Main"),SF_Pixel,0,0);

/**
* The number of shader complexity colors from the engine ini that will be passed to the shader. 
* Changing this requires a recompile of the FShaderComplexityApplyPixelShader.
*/
const UINT NumShaderComplexityColors = 9;

/**
* Pixel shader that is used to map shader complexity stored in scene color into color.
*/
class FShaderComplexityApplyPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FShaderComplexityApplyPixelShader,Global);
public:

	/** 
	* Constructor - binds all shader params
	* @param Initializer - init data from shader compiler
	*/
	FShaderComplexityApplyPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		SceneTextureParams.Bind(Initializer.ParameterMap);
		ShaderComplexityColorsParameter.Bind(Initializer.ParameterMap,TEXT("ShaderComplexityColors"), TRUE);
	}

	FShaderComplexityApplyPixelShader() 
	{
	}

	/**
	* Sets the current pixel shader params
	* @param View - current view
	* @param ShadowInfo - projected shadow info for a single light
	*/
	virtual void SetParameters(
		const FSceneView* View
		)
	{
		SceneTextureParams.Set(View,this);

		//Make sure there are at least NumShaderComplexityColors colors specified in the ini.
		//If there are more than NumShaderComplexityColors they will be ignored.
		check(GEngine->ShaderComplexityColors.Num() >= NumShaderComplexityColors);

		//pass the complexity -> color mapping into the pixel shader
		for(INT ColorIndex = 0; ColorIndex < NumShaderComplexityColors; ColorIndex ++)
		{
			FLinearColor CurrentColor = FLinearColor(GEngine->ShaderComplexityColors(ColorIndex));
			SetPixelShaderValue(
				GetPixelShader(),
				ShaderComplexityColorsParameter,
				CurrentColor,
				ColorIndex
				);
		}

	}

	/**
	* @param Platform - hardware platform
	* @return TRUE if this shader should be cached
	*/
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("NUM_COMPLEXITY_COLORS"),*FString::Printf(TEXT("%u"), NumShaderComplexityColors));
	}

	/**
	* Serialize shader parameters for this shader
	* @param Ar - archive to serialize with
	*/
	UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SceneTextureParams;
		Ar << ShaderComplexityColorsParameter;
		return bShaderHasOutdatedParameters;
	}

private:

	FSceneTextureShaderParameters SceneTextureParams;
	FShaderParameter ShaderComplexityColorsParameter;
};

IMPLEMENT_SHADER_TYPE(,FShaderComplexityApplyPixelShader,TEXT("ShaderComplexityApplyPixelShader"),TEXT("Main"),SF_Pixel,0,0);

//reuse the generic filter vertex declaration
extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

FGlobalBoundShaderState FSceneRenderer::ShaderComplexityBoundShaderState;

/**
* Renders shader complexity colors to the ViewFamily's rendertarget. 
*/
void FSceneRenderer::RenderShaderComplexity(const FViewInfo* View)
{
	// Set the view family's render target/viewport.
	RHISetRenderTarget(ViewFamily.RenderTarget->GetRenderTargetSurface(),FSurfaceRHIRef());	

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
	TShaderMapRef<FShaderComplexityApplyPixelShader> PixelShader(GetGlobalShaderMap());

	SetGlobalBoundShaderState( ShaderComplexityBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));

	PixelShader->SetParameters( View);
	
	// Draw a quad mapping the blurred pixels in the filter buffer to the scene color buffer.
	DrawDenormalizedQuad(
		View->X,View->Y,
		View->SizeX,View->SizeY,
		View->RenderTargetX,View->RenderTargetY,
		View->RenderTargetSizeX,View->RenderTargetSizeY,
		ViewFamily.RenderTarget->GetSizeX(),ViewFamily.RenderTarget->GetSizeY(),
		GSceneRenderTargets.GetBufferSizeX(),GSceneRenderTargets.GetBufferSizeY()
		);
}

