/*=============================================================================
BokehDOF.cpp: High quality Depth of Field post process
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#if WITH_D3D11_TESSELLATION

#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "BokehDOF.h"

TGlobalResource<FBokehVertexBuffer> FBokehDOFRenderer::VertexBuffer;

/** 
* Initialize the RHI for this rendering resource 
*/
void FBokehVertexBuffer::InitRHI()
{
	DWORD Size = 1 * sizeof(FFilterVertex);
	// create vertex buffer
	VertexBufferRHI = RHICreateVertexBuffer(Size, NULL, RUF_Static);
	// lock it
	FFilterVertex* DestVertex = (FFilterVertex*)RHILockVertexBuffer(VertexBufferRHI, 0, Size, FALSE);

	FFilterVertex& ref = *DestVertex;
	// add one vertex
	ref.Position = FVector4(0, 0, 0, 0);
	ref.UV = FVector2D(0, 0);

	// Unlock the buffer.
	RHIUnlockVertexBuffer(VertexBufferRHI);        
}


/** Encapsulates the BokehDOF (Bokeh shapes rendered as quad) geometry shader. */
template <UBOOL bSeparateTranslucency>
class FBokehDOFGeometryShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FBokehDOFGeometryShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		// BokehDOF is SM5 only
		return Platform == SP_PCD3D_SM5;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("USE_SEPARATE_TRANSLUCENCY"), bSeparateTranslucency ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	FBokehDOFGeometryShader() {}

public:

	/** Initialization constructor. */
	FBokehDOFGeometryShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:FGlobalShader(Initializer)
		,DOFParameters(Initializer.ParameterMap)
		,PostProcessParameters(Initializer.ParameterMap)
	{
		BokehInputTexture0Parameter.Bind(Initializer.ParameterMap, TEXT("BokehInputTexture0"), TRUE);
		BokehInputTexture1Parameter.Bind(Initializer.ParameterMap, TEXT("BokehInputTexture1"), TRUE);
		BokehInputTexture2Parameter.Bind(Initializer.ParameterMap, TEXT("BokehInputTexture2"), TRUE);
		SceneCoordinate1ScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("SceneCoordinate1ScaleBias"),TRUE);
		SceneCoordinate2ScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("SceneCoordinate2ScaleBias"),TRUE);
		ArraySettingsParameter.Bind(Initializer.ParameterMap,TEXT("ArraySettings"),TRUE);
	}

	void SetParameters(const FViewInfo& View, const FDepthOfFieldParams& DepthOfFieldParams, FLOAT BlurKernelSize, UINT NumPrimitivesInX)
	{
		// full resolution (this can be much bigger than the viewport needed size, half and quarter is computed from that size)
		const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
		const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();
		// half resolution
		const UINT HalfSizeX = BufferSizeX / 2;
		const UINT HalfSizeY = BufferSizeY / 2;

		const UINT HalfTargetSizeX = View.RenderTargetSizeX / 2;
		const UINT HalfTargetSizeY = View.RenderTargetSizeY / 2;

		const FGeometryShaderRHIRef& ShaderRHI = GetGeometryShader();

		DOFParameters.SetGS(this, DepthOfFieldParams);
		// DOFOcclusionTweak = 0.0f as not needed for GS 
		PostProcessParameters.SetGS(this, BlurKernelSize, 0.0f);

		// half res scene samples
		SetTextureParameter(
			ShaderRHI,
			BokehInputTexture0Parameter,
			TStaticSamplerState<SF_Point,AM_Border,AM_Border,AM_Border>::GetRHI(),
			GSceneRenderTargets.GetTranslucencyBufferTexture());

		// translucent scene samples
		SetTextureParameter(
			ShaderRHI,
			BokehInputTexture1Parameter,
			TStaticSamplerState<SF_Bilinear,AM_Border,AM_Border,AM_Border>::GetRHI(),
			GSceneRenderTargets.GetRenderTargetTexture(SeparateTranslucency));

		// compute pixel position from screen pos, unify D3D9 half texel shift (done like this for cleaner code), flip y
		SetShaderValue(
			ShaderRHI,
			SceneCoordinate1ScaleBiasParameter,
			FVector4(
			2.0f / HalfTargetSizeX,
			-2.0f / HalfTargetSizeY,
			-GPixelCenterOffset / HalfSizeX - 1.0f,
			GPixelCenterOffset / HalfSizeY + 1.0f));

		// compute pixel position from screen pos, shifted by half texel to pick reliable the texel center (stable point filter)
		SetShaderValue(
			ShaderRHI,
			SceneCoordinate2ScaleBiasParameter,
			FVector4(
			1.0f / HalfSizeX,
			1.0f / HalfSizeY,
			0.5f / HalfSizeX,
			0.5f / HalfSizeY));

		// compute pixel position from screen pos, unify D3D9 half texel shift (done like this for cleaner code), flip y
		SetShaderValue(
			ShaderRHI,
			ArraySettingsParameter,
			FVector4(
			NumPrimitivesInX,
			0.0f,
			0.0f,
			0.0f));
	}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DOFParameters << PostProcessParameters << BokehInputTexture0Parameter << BokehInputTexture1Parameter
			<< BokehInputTexture2Parameter << SceneCoordinate1ScaleBiasParameter
			<< SceneCoordinate2ScaleBiasParameter << ArraySettingsParameter;

		return bShaderHasOutdatedParameters;
	}

private: // ---------------------------------------------------

	FDOFShaderParameters			DOFParameters;
	FPostProcessParameters			PostProcessParameters;
	FShaderResourceParameter		BokehInputTexture0Parameter;
	FShaderResourceParameter		BokehInputTexture1Parameter;
	FShaderResourceParameter		BokehInputTexture2Parameter;
	FShaderParameter				SceneCoordinate1ScaleBiasParameter;
	FShaderParameter				SceneCoordinate2ScaleBiasParameter;
	FShaderParameter				ArraySettingsParameter;
};
IMPLEMENT_SHADER_TYPE(template<>,FBokehDOFGeometryShader<TRUE>,TEXT("BokehDOF"),TEXT("MainGeometryShader"),SF_Geometry,0,0);
IMPLEMENT_SHADER_TYPE(template<>,FBokehDOFGeometryShader<FALSE>,TEXT("BokehDOF"),TEXT("MainGeometryShader"),SF_Geometry,0,0);


/**
* A vertex shader for rendering BokehDOF
*/
class FBokehDOFVertexShader : public FShader
{
	DECLARE_SHADER_TYPE(FBokehDOFVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		// BokehDOF is SM5 only
		return Platform == SP_PCD3D_SM5;
	}

	FBokehDOFVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
	}
	FBokehDOFVertexShader() {}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		return FShader::Serialize(Ar);
	}
};
IMPLEMENT_SHADER_TYPE(,FBokehDOFVertexShader,TEXT("BokehDOF"),TEXT("MainVS"),SF_Vertex,0,0);



/**
* A pixel shader for rendering BokehDOF
*/
class FBokehDOFPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FBokehDOFPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		// BokehDOF is SM5 only
		return Platform == SP_PCD3D_SM5;
	}

	FBokehDOFPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FShader(Initializer)
	{
		BokehInputTexture0Parameter.Bind(Initializer.ParameterMap, TEXT("BokehInputTexture0"), TRUE);
		BokehTextureParameter.Bind(Initializer.ParameterMap, TEXT("BokehTexture"), TRUE);
	}

	FBokehDOFPixelShader() {}
	
	// @param BokehTexture can be 0
	void SetParameters(FViewInfo& View, UTexture2D* BokehTexture, EDOFQuality QualityLevel)
	{
		// half res scene point samples
		SetTextureParameter(
			GetPixelShader(),
			BokehInputTexture0Parameter, 
			TStaticSamplerState<SF_Bilinear,AM_Border,AM_Border,AM_Border>::GetRHI(),
			GSceneRenderTargets.GetTranslucencyBufferTexture() );

		// Bokeh texture
		if(BokehTextureParameter.IsBound())
		{
			// use default white texture if the Bokeh texture was not specified yet
			FTextureRHIRef BokehTextureRHI = GWhiteTexture->TextureRHI;

			if(BokehTexture)
			{
				BokehTextureRHI = BokehTexture->Resource->TextureRHI;
			}

			FSamplerStateRHIParamRef SamplerRHI;
			if(QualityLevel == DOFQuality_High)
			{
				// smoother but can be slower
				SamplerRHI = TStaticSamplerState<SF_Trilinear,AM_Border,AM_Border,AM_Border>::GetRHI();
			}
			else
			{
				// steps might become noticeable
				SamplerRHI = TStaticSamplerState<SF_Bilinear,AM_Border,AM_Border,AM_Border>::GetRHI();
			}

			SetTextureParameter(GetPixelShader(), BokehTextureParameter, SamplerRHI, BokehTextureRHI );
		}
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << BokehInputTexture0Parameter << BokehTextureParameter;
		return bShaderHasOutdatedParameters;
	}

private: // ---------------------------------------------------

	FShaderResourceParameter		BokehInputTexture0Parameter;
	FShaderResourceParameter		BokehTextureParameter;
};
IMPLEMENT_SHADER_TYPE(,FBokehDOFPixelShader,TEXT("BokehDOF"),TEXT("MainPS"),SF_Pixel,0,0);

void FBokehDOFRenderer::RenderBokehDOF(
	FViewInfo& View,
	const FDepthOfFieldParams& DepthOfFieldParams,
	FLOAT BlurKernelSize,
	EDOFQuality QualityLevel,
	UTexture2D* BokehTexture,
	UBOOL bSeparateTranslucency)
{
	if(GRHIShaderPlatform != SP_PCD3D_SM5)
	{
		// currently only supported on SM5
		return;
	}

	// full resolution (this can be much bigger than the viewport needed size, half and quarter is computed from that size)
	const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
	const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();
	// half resolution
	const UINT HalfSizeX = BufferSizeX / 2;
	const UINT HalfSizeY = BufferSizeY / 2;

	const UINT HalfTargetSizeX = View.RenderTargetSizeX / 2;
	const UINT HalfTargetSizeY = View.RenderTargetSizeY / 2;

	RHISetRenderTarget(GSceneRenderTargets.GetBokehDOFSurface(),FSurfaceRHIRef());
	RHIClear(TRUE,FLinearColor(0,0,0,0),FALSE,0,FALSE,0);

	FViewPortBounds ViewPortBounds[2] =
	{
		FViewPortBounds(0, 0,						HalfTargetSizeX, HalfTargetSizeY),		// background half res
		FViewPortBounds(HalfSizeX, 0,				HalfTargetSizeX, HalfTargetSizeY),		// foreground half res
	};

	RHISetMultipleViewports(2, ViewPortBounds);

	RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_One,BF_One>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_CCW>::GetRHI());

	// this function assumes RHISetRenderTarget(GSceneRenderTargets.GetHalfResPostProcessSurface(), 0); was called before
	// and RHICopyToResolveTarget is called afterwards

	if(bSeparateTranslucency)
	{
		RenderBokehDOFQualityTempl<TRUE>(View, DepthOfFieldParams, BlurKernelSize, BokehTexture, QualityLevel);
	}
	else
	{
		RenderBokehDOFQualityTempl<FALSE>(View, DepthOfFieldParams, BlurKernelSize, BokehTexture, QualityLevel);
	}

	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
	RHISetBlendState(TStaticBlendState<>::GetRHI());

	RHICopyToResolveTarget(GSceneRenderTargets.GetBokehDOFSurface(),FALSE,FResolveParams());
}



// @param BokehTexture can be 0
template <UBOOL bSeparateTranslucency>
void FBokehDOFRenderer::RenderBokehDOFQualityTempl(
	FViewInfo& View,
	const FDepthOfFieldParams& DepthOfFieldParams,
	FLOAT BlurKernelSize,
	UTexture2D* BokehTexture,
	EDOFQuality QualityLevel)
{
	// half resolution
	const UINT HalfTargetSizeX = View.RenderTargetSizeX / 2;
	const UINT HalfTargetSizeY = View.RenderTargetSizeY / 2;

	TShaderMapRef<FBokehDOFGeometryShader<bSeparateTranslucency> > GeometryShader(GetGlobalShaderMap());
	TShaderMapRef<FBokehDOFVertexShader> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FBokehDOFPixelShader > PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex), *GeometryShader);

	// /2 as we amplify 1 input triangle to 4 quads
	UINT NumPrimitivesInX = HalfTargetSizeX / 2;
	UINT NumPrimitivesInY = HalfTargetSizeY / 2;

	// Set the DOF and bloom parameters
	GeometryShader->SetParameters(View, DepthOfFieldParams, BlurKernelSize, NumPrimitivesInX);

	PixelShader->SetParameters(View, BokehTexture, QualityLevel);

	UINT Stride = 0;
	UINT NumVerticesPerInstance = 3;

	// Stride is 0 so I can use one vertex to specify all 3 vertices of the triangle
	RHISetStreamSource(0, VertexBuffer.VertexBufferRHI, Stride, 0, FALSE, NumVerticesPerInstance, 1);

	UINT BatchVertexIndex = 0;
	UINT NumPrimitivesInBatch = NumPrimitivesInX * NumPrimitivesInY;

	RHIDrawPrimitive(PT_TriangleList, BatchVertexIndex, NumPrimitivesInBatch);
}

#endif // WITH_D3D11_TESSELLATION
