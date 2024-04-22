/*=============================================================================
	SceneFilterRendering.h: Filter rendering definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_SCENEFILTERRENDERING
#define _INC_SCENEFILTERRENDERING

#define MAX_FILTER_SAMPLES	16

#include "SceneRenderTargets.h"

/** A pixel shader which filters a texture. */
template<UINT NumSamples>
class TFilterPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TFilterPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("NUM_SAMPLES"),*FString::Printf(TEXT("%u"),NumSamples));
	}

	/** Default constructor. */
	TFilterPixelShader() {}

	/** Initialization constructor. */
	TFilterPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FGlobalShader(Initializer)
	{
		FilterTextureParameter.Bind(Initializer.ParameterMap,TEXT("FilterTexture"),TRUE);
		SampleWeightsParameter.Bind(Initializer.ParameterMap,TEXT("SampleWeights"),TRUE);
		SampleMaskRectParameter.Bind(Initializer.ParameterMap,TEXT("SampleMaskRect"),TRUE);
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << FilterTextureParameter;
		Ar << SampleWeightsParameter;
		Ar << SampleMaskRectParameter;

		if(NumSamples <= 4)
		{
			SampleWeightsParameter.SetShaderParamName(TEXT("SampleWeights4"));
		}
		else
		{
			SampleWeightsParameter.SetShaderParamName(TEXT("SampleWeights16"));
		}

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			FilterTextureParameter.SetBaseIndex( 0, TRUE );
		}
#endif

		return bShaderHasOutdatedParameters;
	}

	/** Sets shader parameter values */
	void SetParameters(FSamplerStateRHIParamRef SamplerStateRHI,FTextureRHIParamRef TextureRHI,const FLinearColor* SampleWeights, FVector2D SampleMaskMin, FVector2D SampleMaskMax)
	{
		FVector4 SampleMaskRect(SampleMaskMin.X, SampleMaskMin.Y, SampleMaskMax.X, SampleMaskMax.Y);

		SetTextureParameterDirectly(GetPixelShader(),FilterTextureParameter,SamplerStateRHI,TextureRHI);
		SetPixelShaderValues(GetPixelShader(),SampleWeightsParameter,SampleWeights,NumSamples);
		SetPixelShaderValues(GetPixelShader(),SampleMaskRectParameter,&SampleMaskRect,1);
	}

protected:
	FShaderResourceParameter FilterTextureParameter;
	FShaderParameter SampleWeightsParameter;
	FShaderParameter SampleMaskRectParameter;
};

/** A pixel shader which filters a texture and puts the depth channel in alpha channel */
class FDownsampleScene : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDownsampleScene,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	/** Default constructor. */
	FDownsampleScene() {}

	/** Initialization constructor. */
	FDownsampleScene(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		FilterTextureParameter.Bind(Initializer.ParameterMap,TEXT("FilterTexture"),TRUE);
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		VelocityTextureParameter.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"), TRUE );
	}

	/** @param View needed for depth in alpha */
	void SetParameters(const FSceneView* View, FSamplerStateRHIParamRef SamplerStateRHI,FTextureRHIParamRef TextureRHI)
	{
		SetTextureParameterDirectly(GetPixelShader(),FilterTextureParameter,SamplerStateRHI,TextureRHI);
#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			FilterTextureParameter.SetBaseIndex( 0, TRUE );
		}
#endif
		SceneTextureParameters.Set(View, this);

		// can be invalid, e.g. if MotionBlur is disabled in BaseEngine.ini
		if(IsValidRef(GSceneRenderTargets.GetVelocityTexture())) 
		{
			// on Xbox360 the velocity texture is half res, using bilinear filtering and 4 samples expands the mask a bit to get rif of artifacts
			// on other platforms the bilinear filtering doesn't matter but the 4 samples still improve quality
			SetTextureParameterDirectly(this->GetPixelShader(), VelocityTextureParameter, TStaticSamplerState<SF_Bilinear>::GetRHI(), GSceneRenderTargets.GetVelocityTexture());
		}
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << FilterTextureParameter << SceneTextureParameters << VelocityTextureParameter;

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			SceneTextureParameters.SceneColorTextureParameter.Unbind();
		}
#endif
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter FilterTextureParameter;
	FShaderParameter SampleMaskRectParameter;
	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderResourceParameter VelocityTextureParameter;
};

/** A pixel shader which filters a texture. */
class VisualizeTexturePixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(VisualizeTexturePixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("NUM_SAMPLES"),TEXT("1"));
	}

	/** Default constructor. */
	VisualizeTexturePixelShader() {}

	/** Initialization constructor. */
	VisualizeTexturePixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FGlobalShader(Initializer)
	{
		VisualizeTextureParameter.Bind(Initializer.ParameterMap,TEXT("VisualizeTexture"),TRUE);
		VisualizeParamParameter.Bind(Initializer.ParameterMap,TEXT("VisualizeParam"),TRUE);
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << VisualizeTextureParameter;
		Ar << VisualizeParamParameter;

		VisualizeParamParameter.SetShaderParamName(TEXT("VisualizeParam"));
		return bShaderHasOutdatedParameters;
	}

	/** Sets shader parameter values */
	void SetParameters(FTextureRHIParamRef TextureRHI, FLOAT RGBMul, FLOAT AMul, FLOAT Add, FLOAT FracScale, UBOOL SaturateInsteadOfFrac);

protected:
	FShaderResourceParameter VisualizeTextureParameter;
	FShaderParameter VisualizeParamParameter;
};

/** A vertex shader which filters a texture. */
template<UINT NumSamples>
class TFilterVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TFilterVertexShader,Global);
public:

	/** The number of 4D constant registers used to hold the packed 2D sample offsets. */
	enum { NumSampleChunks = (NumSamples + 1) / 2 };

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("NUM_SAMPLES"),*FString::Printf(TEXT("%u"),NumSamples));
	}

	/** Default constructor. */
	TFilterVertexShader() {}

	/** Initialization constructor. */
	TFilterVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FGlobalShader(Initializer)
	{
		SampleOffsetsParameter.Bind(Initializer.ParameterMap,TEXT("SampleOffsets"));
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SampleOffsetsParameter;

		if(NumSamples <= 4)
		{
			SampleOffsetsParameter.SetShaderParamName(TEXT("SampleOffsets4"));
		}
		else
		{
			SampleOffsetsParameter.SetShaderParamName(TEXT("SampleOffsets16"));
		}

		return bShaderHasOutdatedParameters;
	}

	/** Sets shader parameter values */
	void SetParameters(const FVector2D* SampleOffsets)
	{
		FVector4 PackedSampleOffsets[NumSampleChunks];
		for(INT SampleIndex = 0;SampleIndex < NumSamples;SampleIndex += 2)
		{
			PackedSampleOffsets[SampleIndex / 2].X = SampleOffsets[SampleIndex + 0].X;
			PackedSampleOffsets[SampleIndex / 2].Y = SampleOffsets[SampleIndex + 0].Y;
			if(SampleIndex + 1 < NumSamples)
			{
				PackedSampleOffsets[SampleIndex / 2].W = SampleOffsets[SampleIndex + 1].X;
				PackedSampleOffsets[SampleIndex / 2].Z = SampleOffsets[SampleIndex + 1].Y;
			}
		}
		SetVertexShaderValues(GetVertexShader(),SampleOffsetsParameter,PackedSampleOffsets,NumSampleChunks);
	}

private:
	FShaderParameter SampleOffsetsParameter;
};

/** The vertex data used to filter a texture. */
struct FFilterVertex
{
	FVector4 Position;
	FVector2D UV;
};

/** The filter vertex declaration resource type. */
class FFilterVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/** Destructor. */
	virtual ~FFilterVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		Elements.AddItem(FVertexElement(0,STRUCT_OFFSET(FFilterVertex,Position),VET_Float4,VEU_Position,0));
		Elements.AddItem(FVertexElement(0,STRUCT_OFFSET(FFilterVertex,UV),VET_Float2,VEU_TextureCoordinate,0));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/**
 * Draws a quad with the given vertex positions and UVs in denormalized pixel/texel coordinates.
 * The platform-dependent mapping from pixels to texels is done automatically.
 * Note that the positions are affected by the current viewport.
 * X, Y							Position in screen pixels of the top left corner of the quad
 * SizeX, SizeY					Size in screen pixels of the quad
 * U, V							Position in texels of the top left corner of the quad's UV's
 * SizeU, SizeV					Size in texels of the quad's UV's
 * TargetSizeX, TargetSizeY		Size in screen pixels of the target surface
 * TextureSizeX, TextureSizeY	Size in texels of the source texture
 */
extern void DrawDenormalizedQuad(
	FLOAT X,
	FLOAT Y,
	FLOAT SizeX,
	FLOAT SizeY,
	FLOAT U,
	FLOAT V,
	FLOAT SizeU,
	FLOAT SizeV,
	UINT TargetSizeX,
	UINT TargetSizeY,
	UINT TextureSizeX,
	UINT TextureSizeY,
	FLOAT ClipSpaceQuadZ = 0.0f
	);

/**
 * Downsamples a texture to a given surface.
 * @param View !=0 means we want the depth in the alpha channel
 * @param DownsampleFactor 2:2x2->1x1 4:4x4->1:1
 * @param bBilinearOptimization allows 4x less samples but it cannot be used in all cases (e.g. input is floating point and GSupportsFPFiltering==FALSE)
 * @param bFillDestination stretch target rect to occupy the full target (e.g. when we are restricted to power of two RT, can break split screen)
 */
extern void DrawDownsampledTexture(
	const FSurfaceRHIRef &Dest,
	const FTexture2DRHIRef& DestTexture,
	const FTexture2DRHIRef& Src,
	FIntPoint DstLeftTop,
	FIntRect SrcRect,
	FIntPoint DstTextureSize,
	FIntPoint SrcTextureSize,
	const FSceneView* View = 0,
	const UINT DownsampleFactor = 2,
	UBOOL bBilinearOptimization = FALSE,
	UBOOL bFillDestination = FALSE);

/**
 * Renders the given texture to a specific rectangle on the backbuffer.
 *
 * @param DstRect - The rectangle in screen coordinates (pixels) that will be displayed
 * @param SrcRect - The rectangle in the texture (pixels)
 * @param DstScreenSize - The total dimensions of the backbuffer (pixels) used to normalize the rectangle
 * @parma SrcTextureSize - The total size of the texture (pixels) used to normalize the texture coordinates
 */
void DrawTexture(
	const FTexture2DRHIRef& Src,
	FIntRect DstRect = FIntRect(0, 0, 1, 1),
	FIntRect SrcRect = FIntRect(0, 0, 1, 1),
	FIntPoint DstScreenSize = FIntPoint(1, 1),
	FIntPoint SrcTextureSize = FIntPoint(1, 1));

/**
 * Renders the given texture to the backbuffer or HDR scene buffer, good for debugging purpose
 * @param Tex00 left top corner in the input, 0..1 range
 * @param Tex11 right bottom corner in the input, 0..1 range
 */
extern void VisualizeTexture(
	const FTexture2DRHIRef& Src,
	UBOOL RenderToBackbuffer,
	FIntPoint ViewExtent, 
	FIntPoint RTExtent,
	FLOAT RGBMul, 
	FLOAT AMul, 
	FLOAT Add, 
	FVector2D Tex00 = FVector2D(0, 0), 
	FVector2D Tex11 = FVector2D(1, 1),
	FLOAT FracScale = 1.0f,
	UBOOL SaturateInsteadOfFrac = FALSE);

/**
 * Uses a Gaussian blur to filter a subregion of the filter buffer.
 * @param View - The view that is being used in rendering
 * @param ViewSizeX - The view width in pixels to scale the radius.
 * @param SizeX - The width of the subregion to blur.
 * @param SizeY - The height of the subregion to blur.
 * @param KernelRadius - The radius of the Gaussian kernel.
 * @param KernelScale - Extra scale on the filter kernel (does not influence the filter weights)
 * @param FilterColorIndex - Index of the render target to apply the Gaussian blur to
 * @param SampleMaskMin - Minimum values used to clamp the filter samples.  For no clamping use FVector2D(-1.f, -1.f)
 * @param SampleMaskMax - Maximum values used to clamp the filter samples.  For no clamping use FVector2D(2.f, 2.f)
 */
extern void GaussianBlurFilterBuffer(
	const FViewInfo& View,
	FLOAT ViewSizeX,
	UINT SizeX,
	UINT SizeY,
	FLOAT KernelRadius,
	FLOAT KernelScale,
	FSceneRenderTargetIndex FilterColorIndex,
	FVector2D SampleMaskMin,
	FVector2D SampleMaskMax);

/**
 * @param FilterColorIndexFullRes - source and destination
 */
extern void CombineFilterBuffer(
	 UINT SizeX,
	 UINT SizeY,
	 FSceneRenderTargetIndex FilterColorIndexFullRes, FLOAT FullResScale = 1.0f,
	 FSceneRenderTargetIndex FilterColorIndexHalfRes = SRTI_None, FLOAT HalfResScale = 0.0f,
	 FSceneRenderTargetIndex FilterColorIndexQuarterRes = SRTI_None, FLOAT QuarterResScale = 0.0f);

extern void SetupSceneColorGaussianBlurStep(FVector2D Direction,FLOAT KernelRadius, UBOOL bRenderingToLDR);

extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;




/*-----------------------------------------------------------------------------
FMGammaShaderParameters
-----------------------------------------------------------------------------*/

/** Encapsulates the gamma correction parameters. */
class FGammaShaderParameters
{
public:

	/** Default constructor. */
	FGammaShaderParameters() {}

	/** Initialization constructor. */
	FGammaShaderParameters(const FShaderParameterMap& ParameterMap)
	{
		RenderTargetExtent.Bind(ParameterMap,TEXT("RenderTargetExtent"),TRUE);
		GammaColorScaleAndInverse.Bind(ParameterMap,TEXT("GammaColorScaleAndInverse"),TRUE);
		GammaOverlayColor.Bind(ParameterMap,TEXT("GammaOverlayColor"),TRUE);
	}

	/** Set the material shader parameter values. */
	void Set(FShader* PixelShader, FLOAT DisplayGamma, FLinearColor const& ColorScale, FLinearColor const& ColorOverlay)
	{
		// GammaColorScaleAndInverse

		FLOAT InvDisplayGamma = 1.f / Max<FLOAT>(DisplayGamma,KINDA_SMALL_NUMBER);
		FLOAT OneMinusOverlayBlend = 1.f - ColorOverlay.A;

		FVector4 ColorScaleAndInverse;

		ColorScaleAndInverse.X = ColorScale.R * OneMinusOverlayBlend;
		ColorScaleAndInverse.Y = ColorScale.G * OneMinusOverlayBlend;
		ColorScaleAndInverse.Z = ColorScale.B * OneMinusOverlayBlend;
		ColorScaleAndInverse.W = InvDisplayGamma;

		SetPixelShaderValue(
			PixelShader->GetPixelShader(),
			GammaColorScaleAndInverse,
			ColorScaleAndInverse
			);

		// GammaOverlayColor

		FVector4 OverlayColor;

		OverlayColor.X = ColorOverlay.R * ColorOverlay.A;
		OverlayColor.Y = ColorOverlay.G * ColorOverlay.A;
		OverlayColor.Z = ColorOverlay.B * ColorOverlay.A; 
		OverlayColor.W = 0.f; // Unused

		SetPixelShaderValue(
			PixelShader->GetPixelShader(),
			GammaOverlayColor,
			OverlayColor
			);

		const FVector4 vRenderTargetExtent(
			GSceneRenderTargets.GetBufferSizeX(),
			GSceneRenderTargets.GetBufferSizeY(),
			1.0f / GSceneRenderTargets.GetBufferSizeX(),
			1.0f / GSceneRenderTargets.GetBufferSizeY());

		SetPixelShaderValue(
			PixelShader->GetPixelShader(),
			RenderTargetExtent, 
			vRenderTargetExtent);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FGammaShaderParameters& P)
	{
		Ar << P.GammaColorScaleAndInverse;
		Ar << P.GammaOverlayColor;
		Ar << P.RenderTargetExtent;

		P.GammaColorScaleAndInverse.SetShaderParamName(TEXT("GammaColorScaleAndInverse"));
		P.GammaOverlayColor.SetShaderParamName(TEXT("GammaOverlayColor"));

		return Ar;
	}


private:
	FShaderParameter				GammaColorScaleAndInverse;
	FShaderParameter				GammaOverlayColor;
	FShaderParameter				RenderTargetExtent;
};

struct ColorTransformMaterialProperties
{
	/** mirrored material properties (see UberPostProcessEffect.uc) */
	FVector			Shadows;
	FVector			HighLights;
	FVector			MidTones;
	FLOAT			Desaturation;
	FVector			Colorize;
	/** Mirrored properties. See DOFBloomMotionBlurEffect.uc for descriptions */

	void Reset()
	{
		Shadows = FVector(0.f, 0.f, 0.f);
		HighLights = FVector(1.f, 1.f, 1.f);
		MidTones = FVector(1.f, 1.f, 1.f);
		Desaturation = 0.f;
		Colorize = FVector(1.f, 1.f, 1.f);
	}
};

/*-----------------------------------------------------------------------------
FColorRemapShaderParameters
-----------------------------------------------------------------------------*/

/** Encapsulates the Material parameters. */
class FColorRemapShaderParameters
{
public:

	/** Default constructor. */
	FColorRemapShaderParameters() {}

	/** Initialization constructor. */
	FColorRemapShaderParameters(const FShaderParameterMap& ParameterMap)
	{
		ShadowsAndDesaturationParameter.Bind(ParameterMap,TEXT("SceneShadowsAndDesaturation"),TRUE);
		InverseHighLightsParameter.Bind(ParameterMap,TEXT("SceneInverseHighLights"),TRUE);
		MidTonesParameter.Bind(ParameterMap,TEXT("SceneMidTones"),TRUE);
		ScaledLuminanceWeightsParameter.Bind(ParameterMap,TEXT("SceneScaledLuminanceWeights"),TRUE);
		ColorizeParameter.Bind(ParameterMap,TEXT("SceneColorize"),TRUE);
	}

	/** Set the material shader parameter values. */
	void Set(FShader* PixelShader, const struct ColorTransformMaterialProperties& ColorTransform)
	{
		// SceneInverseHighlights

		FVector4 InvHighLights;

		InvHighLights.X = 1.f / ColorTransform.HighLights.X;
		InvHighLights.Y = 1.f / ColorTransform.HighLights.Y;
		InvHighLights.Z = 1.f / ColorTransform.HighLights.Z;
		InvHighLights.W = 0.f; // Unused

		SetPixelShaderValue(
			PixelShader->GetPixelShader(),
			InverseHighLightsParameter,
			InvHighLights
			);

		// SceneShadowsAndDesaturation

		FVector4 ShadowsAndDesaturation;

		ShadowsAndDesaturation.X = ColorTransform.Shadows.X;
		ShadowsAndDesaturation.Y = ColorTransform.Shadows.Y;
		ShadowsAndDesaturation.Z = ColorTransform.Shadows.Z;
		ShadowsAndDesaturation.W = (1.f - ColorTransform.Desaturation);

		SetPixelShaderValue(
			PixelShader->GetPixelShader(),
			ShadowsAndDesaturationParameter,
			ShadowsAndDesaturation
			);

		// MaterialPower

		SetPixelShaderValue(
			PixelShader->GetPixelShader(),
			MidTonesParameter,
			ColorTransform.MidTones
			);

		// SceneScaledLuminanceWeights

		FVector4 ScaledLuminanceWeights;

		ScaledLuminanceWeights.X = 0.30000001f * ColorTransform.Desaturation;
		ScaledLuminanceWeights.Y = 0.58999997f * ColorTransform.Desaturation;
		ScaledLuminanceWeights.Z = 0.11000000f * ColorTransform.Desaturation;
		ScaledLuminanceWeights.W = 0.f; // Unused

		SetPixelShaderValue(
			PixelShader->GetPixelShader(),
			ScaledLuminanceWeightsParameter,
			ScaledLuminanceWeights
			);

		SetPixelShaderValue(
			PixelShader->GetPixelShader(),
			ColorizeParameter,
			ColorTransform.Colorize
			);

	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FColorRemapShaderParameters& P)
	{
		Ar << P.ShadowsAndDesaturationParameter;
		Ar << P.InverseHighLightsParameter;
		Ar << P.MidTonesParameter;
		Ar << P.ScaledLuminanceWeightsParameter;
		Ar << P.ColorizeParameter;

		P.ShadowsAndDesaturationParameter.SetShaderParamName(TEXT("SceneShadowsAndDesaturation"));
		P.InverseHighLightsParameter.SetShaderParamName(TEXT("SceneInverseHighLights"));
		P.MidTonesParameter.SetShaderParamName(TEXT("SceneMidTones"));
		P.ScaledLuminanceWeightsParameter.SetShaderParamName(TEXT("SceneScaledLuminanceWeights"));
		P.ColorizeParameter.SetShaderParamName(TEXT("SceneColorize"));

		return Ar;
	}

private:
	FShaderParameter ShadowsAndDesaturationParameter;
	FShaderParameter InverseHighLightsParameter;
	FShaderParameter MidTonesParameter;
	FShaderParameter ScaledLuminanceWeightsParameter;
	FShaderParameter ColorizeParameter;
};


#endif
