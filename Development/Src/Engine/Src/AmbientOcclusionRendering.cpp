/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "AmbientOcclusionRendering.h"

/** 
 * If TRUE, ambient occlusion and fog are simultaneously rendered as an optimization
 */
#if XBOX
// Disabled as this causes a lot of precision problems with fog
UBOOL GAOCombineWithHeightFog = FALSE;
#else
UBOOL GAOCombineWithHeightFog = FALSE;
#endif

/** The minimum projected screen radius for a primitive to be drawn in the ambient occlusion masking passes, as a fraction of half the horizontal screen width. */
static const FLOAT MinScreenRadiusForAOMasking = 0.10f;
static const FLOAT MinScreenRadiusForAOMaskingSquared = Square(MinScreenRadiusForAOMasking);

/** Whether AO passes should take advantage of a depth buffer or do depth testing manually */
static UBOOL UseDepthBufferForAO(const FDownsampleDimensions& DownsampleDimensions)
{
	return GSceneRenderTargets.IsDownsizedDepthSupported() && DownsampleDimensions.Factor == GSceneRenderTargets.GetSmallColorDepthDownsampleFactor();
}

/*-----------------------------------------------------------------------------
	FDownsampleDimensions
-----------------------------------------------------------------------------*/

FDownsampleDimensions::FDownsampleDimensions(const FViewInfo& View)
{
	Factor = GSceneRenderTargets.GetAODownsampleFactor();
	TargetX = View.RenderTargetX / Factor;
	TargetY = View.RenderTargetY / Factor;
	TargetSizeX = View.RenderTargetSizeX / Factor;
	TargetSizeY = View.RenderTargetSizeY / Factor;
	// Round off odd view sizes
	ViewSizeX = appFloor(View.SizeX / Factor);
	ViewSizeY = appFloor(View.SizeY / Factor);
}

/*-----------------------------------------------------------------------------
	FAmbientOcclusionParams
-----------------------------------------------------------------------------*/

void FAmbientOcclusionParams::Bind(const FShaderParameterMap& ParameterMap)
{
	AmbientOcclusionTextureParameter.Bind(ParameterMap,TEXT("AmbientOcclusionTexture"), TRUE);
	AOHistoryTextureParameter.Bind(ParameterMap,TEXT("AOHistoryTexture"), TRUE);
	AOScreenPositionScaleBiasParameter.Bind(ParameterMap,TEXT("AOScreenPositionScaleBias"), TRUE);
	ScreenEdgeLimitsParameter.Bind(ParameterMap,TEXT("ScreenEdgeLimits"), TRUE);
}

void FAmbientOcclusionParams::Set(const FDownsampleDimensions& DownsampleDimensions, FShader* PixelShader, ESamplerFilter AOFilter, const FTexture2DRHIRef& Input)
{
	// Transform from NDC [-1, 1] to screen space so that positions can be used as texture coordinates to lookup into ambient occlusion buffers.
	// This handles the view size being smaller than the buffer size, and applies a half pixel offset on required platforms.
	// Scale in xy, Bias in zw.
	AOScreenPositionScaleBias = FVector4(
		DownsampleDimensions.ViewSizeX / GSceneRenderTargets.GetAOBufferSizeX() / +2.0f,
		DownsampleDimensions.ViewSizeY / GSceneRenderTargets.GetAOBufferSizeY() / -2.0f,
		(DownsampleDimensions.ViewSizeX / 2.0f + GPixelCenterOffset + DownsampleDimensions.TargetX) / GSceneRenderTargets.GetAOBufferSizeX(),
		(DownsampleDimensions.ViewSizeY / 2.0f + GPixelCenterOffset + DownsampleDimensions.TargetY) / GSceneRenderTargets.GetAOBufferSizeY());

	SetPixelShaderValue(PixelShader->GetPixelShader(), AOScreenPositionScaleBiasParameter, AOScreenPositionScaleBias);

	{
		// Find the edges of the viewport in screenspace, used to identify new pixels along the edges of the viewport so their history can be discarded.
		const FVector2D ClipToScreenScale = FVector2D(AOScreenPositionScaleBias.X, AOScreenPositionScaleBias.Y);
		const FVector2D ClipToScreenBias = FVector2D(AOScreenPositionScaleBias.Z, AOScreenPositionScaleBias.W);
		const FVector2D ScreenSpaceMin = FVector2D(-1.0f, 1.0f) * ClipToScreenScale + ClipToScreenBias;
		const FVector2D ScreenSpaceMax = FVector2D(1.0f, -1.0f) * ClipToScreenScale + ClipToScreenBias;

		SetPixelShaderValue(PixelShader->GetPixelShader(), ScreenEdgeLimitsParameter, FVector4(ScreenSpaceMin.X, ScreenSpaceMin.Y, ScreenSpaceMax.X, ScreenSpaceMax.Y));
	}

	{
		FSamplerStateRHIRef Filter;
		if (AOFilter == SF_Bilinear)
		{
			Filter = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		}
		else
		{
			Filter = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		}
		SetTextureParameter(
			PixelShader->GetPixelShader(),
			AmbientOcclusionTextureParameter,
			Filter,
			Input
			);
	}

	SetTextureParameter(
		PixelShader->GetPixelShader(),
		AOHistoryTextureParameter,
		TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
		GSceneRenderTargets.GetAOHistoryTexture()
		);
}

FArchive& operator<<(FArchive& Ar,FAmbientOcclusionParams& Parameters)
{
	Ar << Parameters.AmbientOcclusionTextureParameter;
	Ar << Parameters.AOHistoryTextureParameter;
	Ar << Parameters.AOScreenPositionScaleBiasParameter;
	Ar << Parameters.ScreenEdgeLimitsParameter;
	return Ar;
}

/*-----------------------------------------------------------------------------
	FDownsampleDepthVertexShader
-----------------------------------------------------------------------------*/

class FDownsampleDepthVertexShader : public FShader
{
	DECLARE_SHADER_TYPE(FDownsampleDepthVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform) 
	{ 
		return TRUE;
	}

	FDownsampleDepthVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		HalfSceneColorTexelSizeParameter.Bind(Initializer.ParameterMap,TEXT("HalfSceneColorTexelSize"), TRUE);
	}

	void SetParameters(const FViewInfo& View)
	{
		const FVector2D HalfSceneColorTexelSize = FVector2D(
			0.5f / (FLOAT)GSceneRenderTargets.GetBufferSizeX(), 
			0.5f / (FLOAT)GSceneRenderTargets.GetBufferSizeY());

		SetVertexShaderValue(GetVertexShader(), HalfSceneColorTexelSizeParameter, HalfSceneColorTexelSize);
	}

	FDownsampleDepthVertexShader() {}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << HalfSceneColorTexelSizeParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter HalfSceneColorTexelSizeParameter;
};

IMPLEMENT_SHADER_TYPE(,FDownsampleDepthVertexShader,TEXT("AmbientOcclusionShader"),TEXT("DownsampleDepthVertexMain"),SF_Vertex,0,0);

/*-----------------------------------------------------------------------------
	TDownsampleDepthPixelShader
-----------------------------------------------------------------------------*/

class TDownsampleDepthPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(TDownsampleDepthPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform) 
	{ 
		return TRUE;
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	TDownsampleDepthPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
	}

	TDownsampleDepthPixelShader() {}

	void SetParameters(const FViewInfo& View)
	{
		SceneTextureParameters.Set(&View, this);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FSceneTextureShaderParameters SceneTextureParameters;
};

IMPLEMENT_SHADER_TYPE(, TDownsampleDepthPixelShader, TEXT("AmbientOcclusionShader"),  TEXT("DownsampleDepthPixelMain"),  SF_Pixel, 0, 0 );

FGlobalBoundShaderState DepthDownsampleBoundShaderState;
FGlobalBoundShaderState DistanceMaskBoundShaderState;

extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

/** Downsamples the scene's fogging and ambient occlusion depth. */
extern UBOOL RenderQuarterDownsampledDepthAndFog(const FScene* Scene, const FViewInfo& View, UINT DPGIndex, const FDownsampleDimensions& DownsampleDimensions);

/**
 * Down-samples depth to the occlusion buffer.
 */
UBOOL DownsampleDepth(
	const FScene* Scene, 
	UINT InDepthPriorityGroup,
	const FViewInfo& View,
	const FDownsampleDimensions& DownsampleDimensions,
	UBOOL bUsingHistoryBuffer,
	FLOAT MaxOcclusionDepth )
{
	UBOOL bFogRendered = FALSE;

	// Clear the render target storing depth for ambient occlusion calculations when we're in the editor and rendering to a subset of the render target.
	// This avoids a band of occlusion on editor viewports which are reading depth values outside of the view.
	//@todo: Need to handle this in game, both when the resolution has changed to be smaller (in which case render targets are not re-allocated)
	// and in split screen, since each view will read depth values from the neighboring view.
	if ( GIsEditor 
		&& (DownsampleDimensions.TargetSizeX < (INT)GSceneRenderTargets.GetAOBufferSizeX()
		|| DownsampleDimensions.TargetSizeY < (INT)GSceneRenderTargets.GetAOBufferSizeY()))
	{
		GSceneRenderTargets.BeginRenderingAOInput();
		// Depth is stored in the g channel, clear to max half so that no occlusion can be added from samples landing outside the viewport.
		const FLinearColor ClearDepthColor(0,65504.0f,0,0);

		// Set the viewport to the current view in the occlusion buffer.
		RHISetViewport(DownsampleDimensions.TargetX, DownsampleDimensions.TargetY, 0.0f, 
			DownsampleDimensions.TargetX + DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetY + DownsampleDimensions.TargetSizeY, 1.0f);				

		FBatchedElements BatchedElements;
		INT V00 = BatchedElements.AddVertex(FVector4(-1,-1,0,1),FVector2D(0,0),ClearDepthColor,FHitProxyId());
		INT V10 = BatchedElements.AddVertex(FVector4(1,-1,0,1),FVector2D(1,0),ClearDepthColor,FHitProxyId());
		INT V01 = BatchedElements.AddVertex(FVector4(-1,1,0,1),FVector2D(0,1),ClearDepthColor,FHitProxyId());
		INT V11 = BatchedElements.AddVertex(FVector4(1,1,0,1),FVector2D(1,1),ClearDepthColor,FHitProxyId());

		// No alpha blending, no depth tests or writes, no backface culling.
		RHISetBlendState(TStaticBlendState<>::GetRHI());
		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
		RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());

		// Draw a quad using the generated vertices.
		BatchedElements.AddTriangle(V00,V10,V11,GWhiteTexture,BLEND_Opaque);
		BatchedElements.AddTriangle(V00,V11,V01,GWhiteTexture,BLEND_Opaque);
		BatchedElements.Draw(
			FMatrix::Identity,
			GSceneRenderTargets.GetAOBufferSizeX(),
			GSceneRenderTargets.GetAOBufferSizeY(),
			FALSE
			);

		GSceneRenderTargets.FinishRenderingAOInput(
			FResolveRect(
				DownsampleDimensions.TargetX,
				DownsampleDimensions.TargetY, 
				DownsampleDimensions.TargetX + DownsampleDimensions.TargetSizeX,
				DownsampleDimensions.TargetY + DownsampleDimensions.TargetSizeY
				)
			);
	}

	// Downsample depths
	// Bind the occlusion buffer as a render target.
	GSceneRenderTargets.BeginRenderingAOInput();

	const UBOOL bUseDepthBufferForAO = UseDepthBufferForAO(DownsampleDimensions);

	UBOOL bVisualizeSSAO = ( ( View.Family->ShowFlags & SHOW_VisualizeSSAO ) != 0 ) && ( View.Family->ShouldPostProcess() );

	// try to downsample depth and fog together if possible
	if(	GSceneRenderTargets.GetAODownsampleFactor() == 2
		&& ShouldRenderFog(View.Family->ShowFlags)
		&& GAOCombineWithHeightFog 
		&& !bVisualizeSSAO
		&& RenderQuarterDownsampledDepthAndFog(Scene, View, InDepthPriorityGroup, DownsampleDimensions))
	{
		bFogRendered = TRUE;
	}
	else
	{
		SCOPED_DRAW_EVENT(OcclusionEventView)(DEC_SCENE_ITEMS,TEXT("DownsampleDepth"));
		// we have to downsample depth ourselves as it was not done in the fog pass
		RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
		// Disable depth test and writes
		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
		RHISetBlendState(TStaticBlendState<>::GetRHI());

		// Set the viewport to the current view in the occlusion buffer.
		RHISetViewport(DownsampleDimensions.TargetX, DownsampleDimensions.TargetY, 0.0f, 
			DownsampleDimensions.TargetX + DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetY + DownsampleDimensions.TargetSizeY, 1.0f);				

		TShaderMapRef<FDownsampleDepthVertexShader> VertexShader(GetGlobalShaderMap());
		TShaderMapRef<TDownsampleDepthPixelShader> PixelShader(GetGlobalShaderMap());

		SetGlobalBoundShaderState(DepthDownsampleBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));
		PixelShader->SetParameters(View);

		VertexShader->SetParameters(View);

		// Draw a full-view quad whose texture coordinates are setup to read from the current view in scene color,
		// and whose positions are setup to render to the entire viewport in the occlusion buffer.
		DrawDenormalizedQuad(
			0, 0,
			DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetSizeY,
			View.RenderTargetX, View.RenderTargetY,
			View.RenderTargetSizeX, View.RenderTargetSizeY,
			DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetSizeY,
			GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY()
			);
	}

	FLOAT DepthBias = 0.0f;
	// Sets up a mask in the R channel of the AO buffer for any primitives that don't allow generating ambient occlusion.
	// If a depth buffer is supported for this downsample factor, setup a stencil mask so these pixels will not be operated on later.
	{
		SCOPED_DRAW_EVENT(RemoveOcclusionEventView)(DEC_SCENE_ITEMS,TEXT("MaskDistantPixels"));
		// Only affect the R channel, G has depth from the previous pass that we don't want to alter.
		RHISetColorWriteMask(CW_RED);
		if (bUseDepthBufferForAO)
		{
			GSceneRenderTargets.BeginRenderingAOInput(TRUE);
			// Using the quarter sized depth buffer
			// Z writes off, depth tests on
			RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());
			// Enable Hi Stencil writes with a cull condition of stencil == 1
			RHIBeginHiStencilRecord(TRUE, 1);
			// Clear stencil in case it is dirty
			RHIClear(FALSE,FLinearColor(0,0,0,0),FALSE,0,TRUE,0);
			// Set stencil to one.
			RHISetStencilState(TStaticStencilState<
				TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,
				FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
				0xff,0xff,1
			>::GetRHI());
			// Use a depth bias as the positions generated from this pass will not exactly match up with the depths in the downsampled depth buffer.
			DepthBias = -0.0001f;
		}
		else
		{
			RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
		}
		RHISetBlendState(TStaticBlendState<>::GetRHI());

		if (bUseDepthBufferForAO)
		{		
			// No occlusion will be visible past MaxOcclusionDepth so we setup a stencil mask to avoid operating on those pixels.
			RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
			RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());
			RHISetBlendState(TStaticBlendState<>::GetRHI());

			TShaderMapRef<FOneColorVertexShader> VertexShader(GetGlobalShaderMap());
			TShaderMapRef<FOneColorPixelShader> PixelShader(GetGlobalShaderMap());

			SetGlobalBoundShaderState(DistanceMaskBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));
			SetPixelShaderValue(PixelShader->GetPixelShader(),PixelShader->ColorParameter,FLinearColor(1.0f, 0.0f, 0.0f, 0.0f));

			FVector ViewSpaceMaxDistance(0.0f, 0.0f, MaxOcclusionDepth);
			FVector4 ClipSpaceMaxDistance = View.ProjectionMatrix.TransformFVector(ViewSpaceMaxDistance);
			// Draw a full-view quad whose Z is at MaxOcclusionDepth
			// Any pixels of the quad that pass the z-test will write a mask to stencil and Hi stencil
			DrawDenormalizedQuad(
				0, 0,
				DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetSizeY,
				DownsampleDimensions.TargetX, DownsampleDimensions.TargetY,
				DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetSizeY,
				DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetSizeY,
				GSceneRenderTargets.GetAOBufferSizeX(), GSceneRenderTargets.GetAOBufferSizeY(),
				ClipSpaceMaxDistance.Z / ClipSpaceMaxDistance.W
				);
	
			// Disable Hi Stencil
			RHIEndHiStencil();
		}
		// Restore color writes to all channels
		RHISetColorWriteMask(CW_RGBA);
	}

	GSceneRenderTargets.FinishRenderingAOInput(
		FResolveRect(
			DownsampleDimensions.TargetX,
			DownsampleDimensions.TargetY, 
			DownsampleDimensions.TargetX + DownsampleDimensions.TargetSizeX,
			DownsampleDimensions.TargetY + DownsampleDimensions.TargetSizeY
			)
		);

	return bFogRendered;
}

/*-----------------------------------------------------------------------------
	FAmbientOcclusionVertexShader
-----------------------------------------------------------------------------*/

class FAmbientOcclusionVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FAmbientOcclusionVertexShader,Global);
public:
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	/** Default constructor. */
	FAmbientOcclusionVertexShader() {}

	/** Initialization constructor. */
	FAmbientOcclusionVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ScreenToViewParameter.Bind(Initializer.ParameterMap,TEXT("ScreenToView"), TRUE);
	}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << ScreenToViewParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FViewInfo& View)
	{
		FMatrix ScreenToView = FMatrix(
			FPlane(1,0,0,0),
			FPlane(0,1,0,0),
			FPlane(0,0,(1.0f - Z_PRECISION),1),
			FPlane(0,0,-View.NearClippingDistance * (1.0f - Z_PRECISION),0)
			) *
			View.ProjectionMatrix.Inverse() *
			FTranslationMatrix(-(FVector)View.ViewOrigin);

		SetVertexShaderValue(GetVertexShader(),ScreenToViewParameter,ScreenToView);
	}

private:
	FShaderParameter ScreenToViewParameter;
};

/** Policy for rendering all qualities */
class FDefaultQualityAO
{
public:
	static const UINT QualityIndex = 1;
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}
};

class FLowQualityAO
{
public:
	static const UINT QualityIndex = 0;
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}
};


/*-----------------------------------------------------------------------------
	TAmbientOcclusionPixelShader
-----------------------------------------------------------------------------*/

template<class QualityPolicy, UBOOL bSupportArbitraryProjection, UBOOL bAngleBased>
class TAmbientOcclusionPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TAmbientOcclusionPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return QualityPolicy::ShouldCache(Platform);
	}

	/** Default constructor. */
	TAmbientOcclusionPixelShader() 
	{
	}

	/** Initialization constructor. */
	TAmbientOcclusionPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		RandomNormalTextureParameter.Bind(Initializer.ParameterMap,TEXT("RandomNormalTexture"), TRUE);
		ProjectionScaleParameter.Bind(Initializer.ParameterMap,TEXT("ProjectionScale"), TRUE);
		ProjectionMatrixParameter.Bind(Initializer.ParameterMap,TEXT("ProjectionMatrix"), TRUE);
		NoiseScaleParameter.Bind(Initializer.ParameterMap,TEXT("NoiseScale"), TRUE);
		AOParams.Bind(Initializer.ParameterMap);
		OcclusionCalcParameters.Bind(Initializer.ParameterMap,TEXT("OcclusionCalcParameters"), TRUE);
		HaloDistanceScaleParameter.Bind(Initializer.ParameterMap,TEXT("HaloDistanceScale"), TRUE);
		OcclusionRemapParameters.Bind(Initializer.ParameterMap,TEXT("OcclusionRemapParameters"), TRUE);
		OcclusionFadeoutParameters.Bind(Initializer.ParameterMap,TEXT("OcclusionFadeoutParameters"), TRUE);
		MaxRadiusTransformParameter.Bind(Initializer.ParameterMap,TEXT("MaxRadiusTransform"), TRUE);
	}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << RandomNormalTextureParameter;
		Ar << ProjectionScaleParameter;
		Ar << ProjectionMatrixParameter;
		Ar << NoiseScaleParameter;
		Ar << AOParams;
		Ar << OcclusionCalcParameters;
		Ar << HaloDistanceScaleParameter;
		Ar << OcclusionRemapParameters;
		Ar << OcclusionFadeoutParameters;
		Ar << MaxRadiusTransformParameter;
		return bShaderHasOutdatedParameters;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("AO_QUALITY"), *FString::Printf(TEXT("%u"), QualityPolicy::QualityIndex));
		OutEnvironment.Definitions.Set(TEXT("ARBITRARY_PROJECTION"), bSupportArbitraryProjection ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("AO_ANGLEBASED"), bAngleBased ? TEXT("1") : TEXT("0"));
	}

	void SetParameters(const FViewInfo& View, const FDownsampleDimensions& DownsampleDimensions, const FAmbientOcclusionSettings& AOSettings)
	{
		TShaderMapRef<FAmbientOcclusionVertexShader> VertexShader(GetGlobalShaderMap());
		VertexShader->SetParameters(View);

		AOParams.Set(DownsampleDimensions, this, SF_Point, GSceneRenderTargets.GetAOInputTexture());

		UTexture2D* RandomNormalTexture = GEngine->RandomNormalTexture;

		if(bAngleBased && GEngine->RandomMirrorDiscTexture)
		{
			// texture aligned with the 4 samples used for AngleBasedSSAO
			RandomNormalTexture = GEngine->RandomMirrorDiscTexture;
		}

		SetTextureParameter(
			GetPixelShader(),
			RandomNormalTextureParameter,
			TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
			RandomNormalTexture->Resource->TextureRHI
			);

		if (bSupportArbitraryProjection)
		{
			// Arbitrary projections are a slow path, but must be supported for tiled screenshots
			SetPixelShaderValue(GetPixelShader(), ProjectionMatrixParameter, View.ProjectionMatrix);
		}
		else
		{
			// If bSupportArbitraryProjection=FALSE the projection matrix cannot be off-center and must only scale x and y.
			checkSlow(Abs(View.ProjectionMatrix.M[3][0]) < KINDA_SMALL_NUMBER 
				&& Abs(View.ProjectionMatrix.M[3][1]) < KINDA_SMALL_NUMBER
				&& Abs(View.ProjectionMatrix.M[2][0]) < KINDA_SMALL_NUMBER
				&& Abs(View.ProjectionMatrix.M[2][1]) < KINDA_SMALL_NUMBER);

			// Combining two scales into one parameter, Projection matrix scaling of x and y and scaling from clip to screen space.
			const FVector2D ProjectionScale = FVector2D(View.ProjectionMatrix.M[0][0], View.ProjectionMatrix.M[1][1])
				* FVector2D(AOParams.AOScreenPositionScaleBias.X, AOParams.AOScreenPositionScaleBias.Y);
			SetPixelShaderValue(GetPixelShader(), ProjectionScaleParameter, ProjectionScale);
		}

		// Maps one pixel of the occlusion buffer to one texel of the random normal texture.
		const FVector4 NoiseScale = FVector4(
			GSceneRenderTargets.GetAOBufferSizeX() / (FLOAT)RandomNormalTexture->SizeX, 
			GSceneRenderTargets.GetAOBufferSizeY() / (FLOAT)RandomNormalTexture->SizeY,
			0.0f,
			0.0f);

		SetPixelShaderValue(GetPixelShader(), NoiseScaleParameter, NoiseScale);

		// Set occlusion heuristic tweakables
		SetPixelShaderValue(
			GetPixelShader(), 
			OcclusionCalcParameters, 
			FVector4(AOSettings.OcclusionRadius, 0, AOSettings.HaloDistanceThreshold, AOSettings.HaloOcclusion));

		// Set HaloDistanceScale
		SetPixelShaderValue(
			GetPixelShader(), 
			HaloDistanceScaleParameter, 
			AOSettings.HaloDistanceScale);

		// Set contrast and brightness tweakables
		SetPixelShaderValue(
			GetPixelShader(), 
			OcclusionRemapParameters, 
			FVector4(AOSettings.OcclusionPower, AOSettings.OcclusionScale, AOSettings.OcclusionBias, AOSettings.MinOcclusion));

		const FLOAT FadeoutRange = 1.0f / (AOSettings.OcclusionFadeoutMaxDistance - AOSettings.OcclusionFadeoutMinDistance);
		SetPixelShaderValue(
			GetPixelShader(), 
			OcclusionFadeoutParameters, 
			FVector4(AOSettings.OcclusionFadeoutMinDistance, FadeoutRange, 0.0f, 0.0f));

		// Maximum screenspace radius allowed, used to limit texture cache thrashing.
		const FLOAT MaxScreenSpaceRadius = 70.0f / GSceneRenderTargets.GetBufferSizeX();
		// Transform screenspace radius into a viewspace horizontal vector.  Multiplication by view space z is done in the pixel shader.
		const FLOAT MaxRadiusTransform = MaxScreenSpaceRadius / (View.ProjectionMatrix.M[0][0] * AOParams.AOScreenPositionScaleBias.X);
		SetPixelShaderValue(GetPixelShader(), MaxRadiusTransformParameter, MaxRadiusTransform);

		SetGlobalBoundShaderState(AOBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, this, sizeof(FFilterVertex));
	}

private:
	FShaderResourceParameter RandomNormalTextureParameter;
	FShaderParameter ProjectionScaleParameter;
	FShaderParameter ProjectionMatrixParameter;
	FShaderParameter NoiseScaleParameter;
	FAmbientOcclusionParams AOParams;
	FShaderParameter OcclusionCalcParameters;
	FShaderParameter HaloDistanceScaleParameter;
	FShaderParameter OcclusionRemapParameters;
	FShaderParameter OcclusionFadeoutParameters;
	FShaderParameter MaxRadiusTransformParameter;

	/** bound shader state for the blend pass */
	static FGlobalBoundShaderState AOBoundShaderState;
};

template<class QualityPolicy, UBOOL bSupportArbitraryProjection, UBOOL bAngleBased>
	FGlobalBoundShaderState TAmbientOcclusionPixelShader<QualityPolicy, bSupportArbitraryProjection, bAngleBased>::AOBoundShaderState;


#define IMPLEMENT_AMBIENTOCCLUSION_SHADER_TYPE(QualityPolicy,bSupportArbitraryProjection,bAngleBased) \
	typedef TAmbientOcclusionPixelShader<QualityPolicy,bSupportArbitraryProjection,bAngleBased> TAmbientOcclusionPixelShader##QualityPolicy##bSupportArbitraryProjection##bAngleBased; \
	IMPLEMENT_SHADER_TYPE( \
	template<>, \
	TAmbientOcclusionPixelShader##QualityPolicy##bSupportArbitraryProjection##bAngleBased, \
	TEXT("AmbientOcclusionShader"), \
	TEXT("OcclusionPixelMain"), \
	SF_Pixel, \
	VER_IMPROVED_ANGLEBASEDSSAO2, \
	0 \
	);

IMPLEMENT_AMBIENTOCCLUSION_SHADER_TYPE(FDefaultQualityAO,TRUE,FALSE);
IMPLEMENT_AMBIENTOCCLUSION_SHADER_TYPE(FDefaultQualityAO,FALSE,FALSE);
IMPLEMENT_AMBIENTOCCLUSION_SHADER_TYPE(FDefaultQualityAO,TRUE,TRUE);
IMPLEMENT_AMBIENTOCCLUSION_SHADER_TYPE(FDefaultQualityAO,FALSE,TRUE);
IMPLEMENT_AMBIENTOCCLUSION_SHADER_TYPE(FLowQualityAO,TRUE,FALSE);
IMPLEMENT_AMBIENTOCCLUSION_SHADER_TYPE(FLowQualityAO,FALSE,FALSE);
IMPLEMENT_AMBIENTOCCLUSION_SHADER_TYPE(FLowQualityAO,TRUE,TRUE);
IMPLEMENT_AMBIENTOCCLUSION_SHADER_TYPE(FLowQualityAO,FALSE,TRUE);

IMPLEMENT_SHADER_TYPE(,FAmbientOcclusionVertexShader,TEXT("AmbientOcclusionShader"),TEXT("OcclusionVertexMain"),SF_Vertex,0,0);

/** 
 * Calculates an occlusion value for each pixel dependent only on scene depth.
 */
void RenderOcclusion(const FViewInfo& View, const FDownsampleDimensions& DownsampleDimensions, const FAmbientOcclusionSettings& AOSettings)
{
	SCOPED_DRAW_EVENT(OcclusionEventView)(DEC_SCENE_ITEMS,TEXT("Occlusion"));

	const UBOOL bUseDepthBufferForAO = UseDepthBufferForAO(DownsampleDimensions);

	GSceneRenderTargets.BeginRenderingAOOutput(bUseDepthBufferForAO);

	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
	RHISetBlendState(TStaticBlendState<>::GetRHI());
	// Disable depth test and writes
	RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());

	if (bUseDepthBufferForAO)
	{
		// Pass if 0
		RHISetStencilState(TStaticStencilState<
			TRUE,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
			FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
			0xff,0xff,0
		>::GetRHI());

		// Use Hi Stencil from masking passes
		RHIBeginHiStencilPlayback(TRUE);
	}

	RHISetViewport(DownsampleDimensions.TargetX, DownsampleDimensions.TargetY, 0.0f, 
		DownsampleDimensions.TargetX + DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetY + DownsampleDimensions.TargetSizeY, 1.0f);				

	if(AOSettings.bAngleBasedSSAO)
	{
		if(AOSettings.OcclusionQuality != AO_Low)
		{
			if(GIsTiledScreenshot)
			{
				// Handle arbitrary projection matrices with tiled screenshots, since they need to zoom and translate within the same view to get different tiles
				TShaderMapRef<TAmbientOcclusionPixelShader<FDefaultQualityAO,TRUE,TRUE> > PixelShader(GetGlobalShaderMap());
				PixelShader->SetParameters(View, DownsampleDimensions, AOSettings);
			}
			else
			{
				// Assume the projection matrix only scales x and y in the general case
				TShaderMapRef<TAmbientOcclusionPixelShader<FDefaultQualityAO,FALSE,TRUE> > PixelShader(GetGlobalShaderMap());
				PixelShader->SetParameters(View, DownsampleDimensions, AOSettings);
			}
		}
		else
		{
			if(GIsTiledScreenshot)
			{
				// Handle arbitrary projection matrices with tiled screenshots, since they need to zoom and translate within the same view to get different tiles
				TShaderMapRef<TAmbientOcclusionPixelShader<FLowQualityAO,TRUE,TRUE> > PixelShader(GetGlobalShaderMap());
				PixelShader->SetParameters(View, DownsampleDimensions, AOSettings);
			}
			else
			{
				// Assume the projection matrix only scales x and y in the general case
				TShaderMapRef<TAmbientOcclusionPixelShader<FLowQualityAO,FALSE,TRUE> > PixelShader(GetGlobalShaderMap());
				PixelShader->SetParameters(View, DownsampleDimensions, AOSettings);
			}
		}
	}
	else
	{
		if (GIsTiledScreenshot)
		{
			// Handle arbitrary projection matrices with tiled screenshots, since they need to zoom and translate within the same view to get different tiles
			TShaderMapRef<TAmbientOcclusionPixelShader<FDefaultQualityAO,TRUE,FALSE> > PixelShader(GetGlobalShaderMap());
			PixelShader->SetParameters(View, DownsampleDimensions, AOSettings);
		}
		else
		{
			// Assume the projection matrix only scales x and y in the general case
			TShaderMapRef<TAmbientOcclusionPixelShader<FDefaultQualityAO,FALSE,FALSE> > PixelShader(GetGlobalShaderMap());
			PixelShader->SetParameters(View, DownsampleDimensions, AOSettings);
		}
	}

	DrawDenormalizedQuad(
		0, 0,
		DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetSizeY,
		DownsampleDimensions.TargetX, DownsampleDimensions.TargetY,
		DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetSizeY,
		DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetSizeY,
		GSceneRenderTargets.GetAOBufferSizeX(), GSceneRenderTargets.GetAOBufferSizeY()
		);

	if (bUseDepthBufferForAO)
	{
		// Disable Hi Stencil
		RHIEndHiStencil();
	}

	GSceneRenderTargets.FinishRenderingAOOutput(
		FResolveRect(
			DownsampleDimensions.TargetX,
			DownsampleDimensions.TargetY, 
			DownsampleDimensions.TargetX + DownsampleDimensions.TargetSizeX,
			DownsampleDimensions.TargetY + DownsampleDimensions.TargetSizeY
			)
		);
}

/*-----------------------------------------------------------------------------
	FEdgePreservingFilterVertexShader
-----------------------------------------------------------------------------*/

class FEdgePreservingFilterVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FEdgePreservingFilterVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	/** Default constructor. */
	FEdgePreservingFilterVertexShader() {}

	/** Initialization constructor. */
	FEdgePreservingFilterVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
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
	TEdgePreservingFilterPixelShader
-----------------------------------------------------------------------------*/

class TEdgePreservingFilterPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TEdgePreservingFilterPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	/** Default constructor. */
	TEdgePreservingFilterPixelShader() {}

	/** Initialization constructor. */
	TEdgePreservingFilterPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		AOParams.Bind(Initializer.ParameterMap);
		FilterParameters.Bind(Initializer.ParameterMap,TEXT("FilterParameters"), TRUE);
		CustomParameters.Bind(Initializer.ParameterMap,TEXT("CustomParameters"), TRUE);
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << AOParams;
		Ar << FilterParameters;
		Ar << CustomParameters;
		return bShaderHasOutdatedParameters;
	}

	/** Sets shader parameter values */
	void SetParameters(
		const FViewInfo& View, 
		const FDownsampleDimensions& DownsampleDimensions, 
		const FAmbientOcclusionSettings& AOSettings,
		UBOOL bHorizontal)
	{
		AOParams.Set(DownsampleDimensions, this, SF_Point, GSceneRenderTargets.GetAOOutputTexture());

		SetPixelShaderValue(GetPixelShader(), FilterParameters, FVector4(AOSettings.EdgeDistanceThreshold, AOSettings.EdgeDistanceScale, 0.0f, 0.0f));

		FVector4 vCustom(
			GSceneRenderTargets.GetAOBufferSizeX(),
			GSceneRenderTargets.GetAOBufferSizeY(),
			0,
			0);
 
		if(bHorizontal)
		{
			vCustom.Z = 1.0f / GSceneRenderTargets.GetAOBufferSizeX();
		}
		else
		{
			vCustom.W = 1.0f / GSceneRenderTargets.GetAOBufferSizeY();
		}

		SetPixelShaderValue(GetPixelShader(), CustomParameters, vCustom);
	}

private:

	FAmbientOcclusionParams AOParams;
	FShaderParameter FilterParameters;
	FShaderParameter CustomParameters;
};

IMPLEMENT_SHADER_TYPE(,FEdgePreservingFilterVertexShader,TEXT("AmbientOcclusionShader"),TEXT("FilterVertexMain"),SF_Vertex,0,0);
IMPLEMENT_SHADER_TYPE(,TEdgePreservingFilterPixelShader,TEXT("AmbientOcclusionShader"),TEXT("FilterPixelMain"),SF_Pixel,VER_OPTIMIZEDSSAO,0);

/** 
 * Filter the occlusion values to reduce noise, preserving boundaries between objects.
 */
void EdgePreservingFilter(const FViewInfo& View, const FDownsampleDimensions& DownsampleDimensions, UBOOL bHorizontal, const FAmbientOcclusionSettings& AOSettings)
{
	const UBOOL bUseDepthBufferForAO = UseDepthBufferForAO(DownsampleDimensions);
	SCOPED_DRAW_EVENT(FilterEventView)(DEC_SCENE_ITEMS,TEXT("EdgePreservingFilter"));

	GSceneRenderTargets.BeginRenderingAOOutput(bUseDepthBufferForAO);

	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());

	if (bUseDepthBufferForAO)
	{
		// Pass if zero
		RHISetStencilState(TStaticStencilState<
			TRUE,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
			FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
			0xff,0xff,0
		>::GetRHI());

		// Use the Hi Stencil mask setup earlier
		// Don't flush Hi Stencil since it was flushed during the Occlusion pass
		RHIBeginHiStencilPlayback(FALSE);
	}

	const FVector2D AOTexelSize = FVector2D(
		1.0f / (FLOAT)GSceneRenderTargets.GetAOBufferSizeX(), 
		1.0f / (FLOAT)GSceneRenderTargets.GetAOBufferSizeY());

	RHISetViewport(DownsampleDimensions.TargetX, DownsampleDimensions.TargetY, 0.0f, 
		DownsampleDimensions.TargetX + DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetY + DownsampleDimensions.TargetSizeY, 1.0f);				

	TShaderMapRef<FEdgePreservingFilterVertexShader> VertexShader(GetGlobalShaderMap());

	// A macro to handle setting the filter shader for a specific number of samples.
	TShaderMapRef<TEdgePreservingFilterPixelShader > PixelShader(GetGlobalShaderMap());
	PixelShader->SetParameters(View, DownsampleDimensions, AOSettings, bHorizontal);
	{
		static FGlobalBoundShaderState FilterBoundShaderState[2];
		SetGlobalBoundShaderState(FilterBoundShaderState[bHorizontal], GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));
	}

	DrawDenormalizedQuad(
		0, 0,
		DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetSizeY,
		DownsampleDimensions.TargetX, DownsampleDimensions.TargetY,
		DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetSizeY,
		DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetSizeY,
		GSceneRenderTargets.GetAOBufferSizeX(), GSceneRenderTargets.GetAOBufferSizeY()
		);
	
	if (bUseDepthBufferForAO)
	{
		RHIEndHiStencil();
	}

	GSceneRenderTargets.FinishRenderingAOOutput(
		FResolveRect(
			DownsampleDimensions.TargetX,
			DownsampleDimensions.TargetY, 
			DownsampleDimensions.TargetX + DownsampleDimensions.TargetSizeX,
			DownsampleDimensions.TargetY + DownsampleDimensions.TargetSizeY
			)
		);
}

/*-----------------------------------------------------------------------------
	FHistoryUpdateVertexShader
-----------------------------------------------------------------------------*/

class FHistoryUpdateVertexShader : public FShader
{
	DECLARE_SHADER_TYPE(FHistoryUpdateVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform) 
	{ 
		return TRUE;
	}

	FHistoryUpdateVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		ScreenToWorldOffsetParameter.Bind(Initializer.ParameterMap,TEXT("ScreenToWorldOffset"), TRUE);
	}

	FHistoryUpdateVertexShader() {}

	void SetParameters(const FViewInfo& View)
	{
		// Remove translation to the world origin to avoid floating point precision issues far from the origin.
		FMatrix InvViewRotationProjMatrix = View.InvProjectionMatrix * View.ViewMatrix.RemoveTranslation().Inverse();
		FMatrix ScreenToWorldOffset = FMatrix(
			FPlane(1,0,0,0),
			FPlane(0,1,0,0),
			FPlane(0,0,(1.0f - Z_PRECISION),1),
			FPlane(0,0,-View.NearClippingDistance * (1.0f - Z_PRECISION),0)
			) *
			InvViewRotationProjMatrix;

		SetVertexShaderValue(GetVertexShader(), ScreenToWorldOffsetParameter, ScreenToWorldOffset);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << ScreenToWorldOffsetParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter ScreenToWorldOffsetParameter;
};

IMPLEMENT_SHADER_TYPE(,FHistoryUpdateVertexShader,TEXT("AmbientOcclusionShader"),TEXT("HistoryUpdateVertexMain"),SF_Vertex,0,0);

/*-----------------------------------------------------------------------------
	FStaticHistoryUpdatePixelShader
-----------------------------------------------------------------------------*/

class FStaticHistoryUpdatePixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FStaticHistoryUpdatePixelShader,Global);
public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		//new(OutEnvironment.CompilerFlags) ECompilerFlags(CFLAG_Debug);
	}

	static UBOOL ShouldCache(EShaderPlatform Platform) 
	{ 
		return TRUE;
	}

	FStaticHistoryUpdatePixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		AOParams.Bind(Initializer.ParameterMap);
		PrevViewProjMatrixParameter.Bind(Initializer.ParameterMap,TEXT("PrevViewProjMatrix"), TRUE);
		HistoryConvergenceRatesParameter.Bind(Initializer.ParameterMap,TEXT("HistoryConvergenceRates"), TRUE);
	}

	FStaticHistoryUpdatePixelShader() 
	{
	}

	void SetParameters(
		const FViewInfo& View, 
		const FDownsampleDimensions& DownsampleDimensions, 
		const FAmbientOcclusionSettings& AOSettings, 
		FLOAT OcclusionConvergenceRate,
		FLOAT WeightConvergenceRate)
	{
		AOParams.Set(DownsampleDimensions, this, SF_Bilinear, GSceneRenderTargets.GetAOOutputTexture());

		// Instead of finding the world space position of the current pixel, calculate the world space position offset by the camera position, 
		// then translate by the difference between last frame's camera position and this frame's camera position,
		// then apply the rest of the transforms.  This effectively avoids precision issues near the extents of large levels whose world space position is very large.
		FVector ViewOriginDelta = View.ViewOrigin - View.PrevViewOrigin;
		SetPixelShaderValue(GetPixelShader(), PrevViewProjMatrixParameter, FTranslationMatrix(ViewOriginDelta) * View.PrevViewRotationProjMatrix);

		SetPixelShaderValue(GetPixelShader(), HistoryConvergenceRatesParameter, FVector2D(OcclusionConvergenceRate, WeightConvergenceRate));
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << AOParams;
		Ar << PrevViewProjMatrixParameter;
		Ar << HistoryConvergenceRatesParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FAmbientOcclusionParams AOParams;
	FShaderParameter PrevViewProjMatrixParameter;
	FShaderParameter HistoryConvergenceRatesParameter;
};

IMPLEMENT_SHADER_TYPE(,FStaticHistoryUpdatePixelShader,TEXT("AmbientOcclusionShader"),TEXT("StaticHistoryUpdatePixelMain"),SF_Pixel,0,0);

FGlobalBoundShaderState HistoryUpdateBoundShaderState;


/**
 * Updates the occlusion history buffer
 */
void HistoryUpdate(const FScene* Scene, UINT InDepthPriorityGroup, const FViewInfo& View, const FDownsampleDimensions& DownsampleDimensions, const FAmbientOcclusionSettings& AOSettings)
{
	if (GSceneRenderTargets.bAOHistoryNeedsCleared )
	{
		// Clear the entire history buffer to ensure the contents are valid. (no NAN/INFs)
		// This can't be done in InitDynamicRHI since that is called on some platforms (PS3) before the shader cache has been initialized,
		// and shaders are required for clearing the AO history format.
		GSceneRenderTargets.BeginRenderingAOHistory();
		// Set the viewport to the current view in the occlusion history buffer.
		RHISetViewport(0, 0, 0.0f, GSceneRenderTargets.GetAOBufferSizeX(), GSceneRenderTargets.GetAOBufferSizeY(), 1.0f);				
		RHIClear(TRUE, FLinearColor::Black, FALSE, 0, FALSE, 0);
		GSceneRenderTargets.FinishRenderingAOHistory(FResolveParams());
		GSceneRenderTargets.bAOHistoryNeedsCleared = FALSE;
	}

	//@todo - would like to have access to avg framerate, but that is compiled out in final release
	const FLOAT CurrentFPS = 1.0f / Max(View.Family->DeltaWorldTime, 0.0001f);
	// Calculate framerate dependent convergence rate using the approximation that a weight of W will converge in 1 / (1 - W) frames.
	FLOAT OcclusionConvergenceRate = Clamp(1.0f - 1.0f / (CurrentFPS * AOSettings.HistoryOcclusionConvergenceTime), 0.0f, .9999f);
	// Weight converges linearly
	FLOAT WeightConvergenceRate = Clamp(View.Family->DeltaWorldTime / AOSettings.HistoryWeightConvergenceTime, 0.0001f, 1.0f);
	if (View.bPrevTransformsReset)
	{
		// Discard the occlusion history on frames when the previous transforms have been reset
		// This will avoid occlusion in the wrong places after a camera cut
		OcclusionConvergenceRate = 0.0f;
		WeightConvergenceRate = 0.0f;
	}

	const UBOOL bUseDepthBufferForAO = UseDepthBufferForAO(DownsampleDimensions);
	if (bUseDepthBufferForAO)
	{
		// Bind the ambient occlusion history buffer as a render target.
		GSceneRenderTargets.BeginRenderingAOHistory(TRUE);
		// Clear to unoccluded (R channel of 1.0f) and fully converged (G channel of 0.0f)
		// This is necessary because we are using a stencil mask that keeps all pixels from being touched
		RHIClear(TRUE, FLinearColor(1.0f, 0.0f, 0.0f, 0.0f), FALSE, 0, FALSE, 0);
		// Pass if zero
		RHISetStencilState(TStaticStencilState<
			TRUE,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
			FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
			0xff,0xff,0
		>::GetRHI());
		// Use the Hi Stencil mask setup earlier
		// Don't flush Hi Stencil since it was flushed during the Occlusion pass
		RHIBeginHiStencilPlayback(FALSE);
		RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
		// Disable depth test and writes
		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
		RHISetBlendState(TStaticBlendState<>::GetRHI());
	}
	else
	{
		// Bind the ambient occlusion history buffer as a render target.
		GSceneRenderTargets.BeginRenderingAOHistory(FALSE);
	}

	// Draw a full screen quad to update the occlusion history.
	// This pass assumes that the world space position this frame is the same as the world space position last frame,
	// so it is only correct for static primitives.  Movable primitives will streak occlusion and need to be handled separately.
	{
		SCOPED_DRAW_EVENT(OcclusionEventView)(DEC_SCENE_ITEMS,TEXT("StaticHistoryUpdate"));

		TShaderMapRef<FHistoryUpdateVertexShader> VertexShader(GetGlobalShaderMap());
		TShaderMapRef<FStaticHistoryUpdatePixelShader> PixelShader(GetGlobalShaderMap());

		SetGlobalBoundShaderState(HistoryUpdateBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));

		// Set the viewport to the current view in the occlusion history buffer.
		RHISetViewport(DownsampleDimensions.TargetX, DownsampleDimensions.TargetY, 0.0f, 
			DownsampleDimensions.TargetX + DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetY + DownsampleDimensions.TargetSizeY, 1.0f);				

		VertexShader->SetParameters(View);
		PixelShader->SetParameters(View, DownsampleDimensions, AOSettings, OcclusionConvergenceRate, WeightConvergenceRate);

		DrawDenormalizedQuad(
			0, 0,
			DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetSizeY,
			DownsampleDimensions.TargetX, DownsampleDimensions.TargetY,
			DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetSizeY,
			DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetSizeY,
			GSceneRenderTargets.GetAOBufferSizeX(), GSceneRenderTargets.GetAOBufferSizeY()
			);
	}

	if (bUseDepthBufferForAO)
	{
		RHIEndHiStencil();
	}

	GSceneRenderTargets.FinishRenderingAOHistory(
		FResolveParams(FResolveRect(
			DownsampleDimensions.TargetX,
			DownsampleDimensions.TargetY, 
			DownsampleDimensions.TargetX + DownsampleDimensions.TargetSizeX,
			DownsampleDimensions.TargetY + DownsampleDimensions.TargetSizeY
			))
		);
}

/*-----------------------------------------------------------------------------
	TAOApplyPixelShader
-----------------------------------------------------------------------------*/
enum EAOApplyMode
{
	AOApply_Normal,
	AOApply_ReadFromAOHistory,
	AOApply_ApplyFog,
	AOApply_ApplyFogReadFromHistory,
	AOApply_Max
};

template<EAOApplyMode ApplyMode>
class TAOApplyPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(TAOApplyPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform) 
	{ 
		// Only compile combined fog versions on xbox
		const UBOOL bCombinationSupported = Platform == SP_XBOXD3D || ApplyMode == AOApply_Normal || ApplyMode == AOApply_ReadFromAOHistory;
		return bCombinationSupported; 
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		const UBOOL bApplyFromAOHistory = ApplyMode == AOApply_ReadFromAOHistory || ApplyMode == AOApply_ApplyFogReadFromHistory;
		OutEnvironment.Definitions.Set(TEXT("APPLY_FROM_AOHISTORY"), bApplyFromAOHistory ? TEXT("1") : TEXT("0"));
	}

	TAOApplyPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FShader(Initializer)
	{
		AOParams.Bind(Initializer.ParameterMap);
		FogTextureParameter.Bind(Initializer.ParameterMap,TEXT("FogFactorTexture"), TRUE);
		FogColorParameter.Bind(Initializer.ParameterMap,TEXT("FogInScattering"), TRUE);
		TargetSizeParameter.Bind(Initializer.ParameterMap,TEXT("TargetSize"), TRUE);
		OcclusionColorParameter.Bind(Initializer.ParameterMap,TEXT("OcclusionColor"), TRUE);
		InvEncodePowerParameter.Bind(Initializer.ParameterMap,TEXT("InvEncodePower"), TRUE);
		SceneTextureParameters.Bind(Initializer.ParameterMap);
	}
	TAOApplyPixelShader() {}

	void SetParameters(const FViewInfo& View, const FDownsampleDimensions& DownsampleDimensions, FLinearColor OcclusionColor, FLinearColor FogColor)
	{
		// Upsample half resolution with bilinear filtering
		AOParams.Set(DownsampleDimensions, this, SF_Bilinear, GSceneRenderTargets.GetAOOutputTexture());

		SetPixelShaderValue(GetPixelShader(), FogColorParameter, FogColor);

		SetPixelShaderValue(
			GetPixelShader(), 
			TargetSizeParameter, 
			FVector2D(GSceneRenderTargets.GetBufferSizeX(),GSceneRenderTargets.GetBufferSizeY())
			);

		SetTextureParameter(
			GetPixelShader(),
			FogTextureParameter,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			GSceneRenderTargets.GetFogBufferTexture()
			);

		SceneTextureParameters.Set(&View, this);

		SetPixelShaderValue(GetPixelShader(), OcclusionColorParameter, OcclusionColor);

		SetPixelShaderValue(GetPixelShader(), InvEncodePowerParameter, 1.0f);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << AOParams;
		Ar << OcclusionColorParameter;
		Ar << FogColorParameter;
		Ar << TargetSizeParameter;
		Ar << InvEncodePowerParameter;
		Ar << FogTextureParameter;
		Ar << SceneTextureParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FAmbientOcclusionParams AOParams;
	FShaderParameter OcclusionColorParameter;
	FShaderParameter FogColorParameter;
	FShaderParameter TargetSizeParameter;
	FShaderParameter InvEncodePowerParameter;
	FShaderResourceParameter FogTextureParameter;
	FSceneTextureShaderParameters SceneTextureParameters;
};

IMPLEMENT_SHADER_TYPE(template<>,TAOApplyPixelShader<AOApply_Normal>,TEXT("AmbientOcclusionShader"),TEXT("AOApplyMain"),SF_Pixel,VER_IMPROVED_ANGLEBASEDSSAO2,0);
IMPLEMENT_SHADER_TYPE(template<>,TAOApplyPixelShader<AOApply_ReadFromAOHistory>,TEXT("AmbientOcclusionShader"),TEXT("AOApplyMain"),SF_Pixel,VER_IMPROVED_ANGLEBASEDSSAO2,0);
IMPLEMENT_SHADER_TYPE(template<>,TAOApplyPixelShader<AOApply_ApplyFog>,TEXT("AmbientOcclusionShader"),TEXT("AOAndFogApplyMain"),SF_Pixel,VER_IMPROVED_ANGLEBASEDSSAO2,0);
IMPLEMENT_SHADER_TYPE(template<>,TAOApplyPixelShader<AOApply_ApplyFogReadFromHistory>,TEXT("AmbientOcclusionShader"),TEXT("AOAndFogApplyMain"),SF_Pixel,VER_IMPROVED_ANGLEBASEDSSAO2,0);

FGlobalBoundShaderState AOApplyBoundShaderState[AOApply_Max];

/** 
 * Applies the calculated occlusion value to scene color.
 */
void AmbientOcclusionApply(const FScene* Scene, const FViewInfo& View, const FDownsampleDimensions& DownsampleDimensions, UBOOL bReadFromHistoryBuffer, UBOOL bApplyFog, const FAmbientOcclusionSettings& AOSettings)
{
	SCOPED_DRAW_EVENT(OcclusionEventView)(DEC_SCENE_ITEMS,TEXT("AmbientOcclusionApply"));

	FLinearColor OcclusionColor = AOSettings.OcclusionColor;

	// Render to the scene color buffer, everything that's been rendered so far will be affected 
	// (emissive, direct lighting diffuse + specular, indirect lighting from lightmaps and ambient lights)
	GSceneRenderTargets.BeginRenderingSceneColor();

	UBOOL bSSAO = ( ( View.Family->ShowFlags & SHOW_SSAO ) != 0 ) && ( View.Family->ShouldPostProcess() );
	UBOOL bVisualizeSSAO = ( ( View.Family->ShowFlags & SHOW_VisualizeSSAO ) != 0 ) && ( View.Family->ShouldPostProcess() );

	if( bSSAO && bVisualizeSSAO )
	{
		// Rendering AO but not scene color, so overwrite previous scene color calculations.
		// This is useful as a visualization of the occlusion factor.
		RHISetBlendState(TStaticBlendState<BO_Add,BF_SourceAlpha,BF_Zero>::GetRHI());
		OcclusionColor = FLinearColor::White;
	}
	else if (bSSAO && !bVisualizeSSAO && !bApplyFog)
	{
		// Lerp between the occlusion color and existing scene color (which only contains lighting) based on the calculated occlusion factor.
		RHISetBlendState(TStaticBlendState<BO_Add,BF_InverseSourceAlpha,BF_SourceAlpha>::GetRHI());
	}
	else if (bSSAO && !bVisualizeSSAO && bApplyFog)
	{
		// Filter ambient occlusion through fog
		RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_SourceAlpha>::GetRHI());
	}
	else
	{
		check(FALSE);
	}

	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
	// Disable depth test and writes
	RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
	// Preserve destination scene color alpha, which contains scene depth on some platforms.
	RHISetColorWriteMask(CW_RGB);
	RHISetStencilState(TStaticStencilState<>::GetRHI());

	// Set the viewport to the current view
	RHISetViewport(View.RenderTargetX, View.RenderTargetY, 0.0f, View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY, 1.0f);				

	TShaderMapRef<FAmbientOcclusionVertexShader> VertexShader(GetGlobalShaderMap());
	VertexShader->SetParameters(View);

	if (bReadFromHistoryBuffer)
	{
		if (bApplyFog)
		{
			TShaderMapRef<TAOApplyPixelShader<AOApply_ApplyFogReadFromHistory> > PixelShader(GetGlobalShaderMap());
			PixelShader->SetParameters(View, DownsampleDimensions, OcclusionColor, View.HeightFogParams.FogInScattering[0]);
			SetGlobalBoundShaderState(AOApplyBoundShaderState[AOApply_ApplyFogReadFromHistory], GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));
		}
		else
		{
			TShaderMapRef<TAOApplyPixelShader<AOApply_ReadFromAOHistory> > PixelShader(GetGlobalShaderMap());
			PixelShader->SetParameters(View, DownsampleDimensions, OcclusionColor, FLinearColor::Black);
			SetGlobalBoundShaderState(AOApplyBoundShaderState[AOApply_ReadFromAOHistory], GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));
		}
	}
	else
	{
		if (bApplyFog)
		{
			TShaderMapRef<TAOApplyPixelShader<AOApply_ApplyFog> > PixelShader(GetGlobalShaderMap());
			PixelShader->SetParameters(View, DownsampleDimensions, OcclusionColor, View.HeightFogParams.FogInScattering[0]);
			SetGlobalBoundShaderState(AOApplyBoundShaderState[AOApply_ApplyFog], GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));
		}
		else
		{
			TShaderMapRef<TAOApplyPixelShader<AOApply_Normal> > PixelShader(GetGlobalShaderMap());
			PixelShader->SetParameters(View, DownsampleDimensions, OcclusionColor, FLinearColor::Black);
			SetGlobalBoundShaderState(AOApplyBoundShaderState[AOApply_Normal], GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));
		}
	}

	// Draw a full-view quad, with texture coordinates setup to read from the view in the occlusion buffer.
	DrawDenormalizedQuad(
		0, 0,
		View.RenderTargetSizeX, View.RenderTargetSizeY,
		DownsampleDimensions.TargetX, DownsampleDimensions.TargetY,
		DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetSizeY,
		View.RenderTargetSizeX, View.RenderTargetSizeY,
		GSceneRenderTargets.GetAOBufferSizeX(), GSceneRenderTargets.GetAOBufferSizeY()
		);

	RHISetColorWriteMask(CW_RGBA);

	GSceneRenderTargets.FinishRenderingSceneColor(!(bSSAO && !bVisualizeSSAO));
}

FAmbientOcclusionSettings::FAmbientOcclusionSettings(const UAmbientOcclusionEffect* InEffect) :
	OcclusionColor(InEffect->OcclusionColor),
	OcclusionPower(InEffect->OcclusionPower),
	OcclusionScale(InEffect->OcclusionScale),
	OcclusionBias(InEffect->OcclusionBias),
	MinOcclusion(InEffect->MinOcclusion),
	OcclusionRadius(InEffect->OcclusionRadius),
	OcclusionQuality((EAmbientOcclusionQuality)InEffect->OcclusionQuality),
	OcclusionFadeoutMinDistance(InEffect->OcclusionFadeoutMinDistance),
	OcclusionFadeoutMaxDistance(InEffect->OcclusionFadeoutMaxDistance),
	HaloDistanceThreshold(InEffect->HaloDistanceThreshold),
	HaloDistanceScale(InEffect->HaloDistanceScale),
	HaloOcclusion(InEffect->HaloOcclusion),
	EdgeDistanceThreshold(InEffect->EdgeDistanceThreshold),
	EdgeDistanceScale(InEffect->EdgeDistanceScale),
	FilterDistanceScale(InEffect->FilterDistanceScale),
	HistoryOcclusionConvergenceTime(InEffect->HistoryConvergenceTime),
	HistoryWeightConvergenceTime(InEffect->HistoryWeightConvergenceTime),
	bAngleBasedSSAO(InEffect->bAngleBasedSSAO)
{
}

/*-----------------------------------------------------------------------------
	FAmbientOcclusionSceneProxy
-----------------------------------------------------------------------------*/

class FAmbientOcclusionSceneProxy : public FPostProcessSceneProxy
{
public:
	/** 
	 * Initialization constructor. 
	 * @param InEffect - effect to mirror in this proxy
	 */
	FAmbientOcclusionSceneProxy(const UAmbientOcclusionEffect* InEffect,const FPostProcessSettings* WorldSettings) :
		FPostProcessSceneProxy(InEffect),
		AOSettings(InEffect)
	{
#if !FINAL_RELEASE
		{
			// console variable can override
			static IConsoleVariable* CVarMin = GConsoleManager->FindConsoleVariable(TEXT("AmbientOcclusionQuality")); 

			INT Value = CVarMin->GetInt();

			if(Value >= 0)
			{
				// 2 - is needed because the enum is defined backwards (0 = high, 2 = low)
				AOSettings.OcclusionQuality = (EAmbientOcclusionQuality)Clamp(2 - Value, 0, 2);
			}
		}
#endif // !FINAL_RELEASE
	}

	UBOOL UseHistorySmoothing(const FViewInfo& View) const
	{
		// Only use the history when realtime update is on, since the algorithm requires continuous frames
		return View.Family->bRealtimeUpdate
			// Disable the history buffer if smoothing won't be noticeable
			&& AOSettings.HistoryOcclusionConvergenceTime > 0.01f
			// Need bilinear filtering for resampling the floating point history buffer
			&& GSupportsFPFiltering
			// Disable the history for tiled screenshots, frames will not be spatially coherent
			&& !GIsTiledScreenshot
			// Disable the history for high resolution screenshots, frames will not be spatially coherent
			&& !GIsHighResScreenshot;
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
	UBOOL Render(const FScene* Scene, UINT InDepthPriorityGroup, FViewInfo& View, const FMatrix& CanvasTransform,struct FSceneColorLDRInfo& LDRInfo)
	{
		UBOOL bSSAO = ( ( View.Family->ShowFlags & SHOW_SSAO ) != 0 ) && ( View.Family->ShouldPostProcess() );

		if (!bSSAO)
		{
			return FALSE;
		}

		if (!GSystemSettings.bAllowAmbientOcclusion
			|| !View.RenderingOverrides.bAllowAmbientOcclusion)
		{
			return FALSE;
		}

		// Should be enforced by the post process effect
		check(InDepthPriorityGroup == SDPG_World);

		// Disable downsampling on the high quality setting
		INT NewDownsampleFactor = AOSettings.OcclusionQuality == AO_High ? 1 : 2;
#if XBOX
		// AO buffers won't fit in EDRAM at the same time as scene color and depth unless they are downsampled.
		NewDownsampleFactor = 2;
#endif

		GSceneRenderTargets.SetAODownsampleFactor(NewDownsampleFactor);

		SCOPED_DRAW_EVENT(AmbientOcclusionEvent)(DEC_SCENE_ITEMS,TEXT("AmbientOcclusion"));

		FDownsampleDimensions DownsampleDimensions(View);

		const UBOOL bUseHistoryBuffer = UseHistorySmoothing(View);

		// Downsample depth and store it in the occlusion buffer, which reduces texture lookups required during the filter passes.
		const UBOOL bApplyFog = DownsampleDepth(Scene, InDepthPriorityGroup, View, DownsampleDimensions, bUseHistoryBuffer, AOSettings.OcclusionFadeoutMaxDistance);

		// Mark one layer height fog as already rendered for this view
		View.bOneLayerHeightFogRenderedInAO = bApplyFog;

		// Render the occlusion factor
		RenderOcclusion(View, DownsampleDimensions, AOSettings);

		// Filter the occlusion factor to reduce spatial noise

		// X
		EdgePreservingFilter(View, DownsampleDimensions, TRUE, AOSettings);
		// Y
		EdgePreservingFilter(View, DownsampleDimensions, FALSE, AOSettings);

		if (bUseHistoryBuffer)
		{
			// Lerp between the new occlusion value and the running history to reduce temporal noise
			HistoryUpdate(Scene, InDepthPriorityGroup, View, DownsampleDimensions, AOSettings);
		}

		// Apply to scene color
		AmbientOcclusionApply(Scene, View, DownsampleDimensions, bUseHistoryBuffer, bApplyFog, AOSettings);

		// Scene color needs to be resolved before being read again.
		return TRUE;
	}

	/**
	 * Tells FSceneRenderer whether to store the previous frame's transforms.
	 */
	virtual UBOOL RequiresPreviousTransforms(const FViewInfo& View) const
	{
		return UseHistorySmoothing(View);
	}

private:
	/** Mirrored properties. */
	FAmbientOcclusionSettings AOSettings;
};

IMPLEMENT_CLASS(UAmbientOcclusionEffect);

/**
 * Creates a proxy to represent the render info for a post process effect
 * @param WorldSettings - The world's post process settings for the view.
 * @return The proxy object.
 */
FPostProcessSceneProxy* UAmbientOcclusionEffect::CreateSceneProxy(const FPostProcessSettings* WorldSettings)
{
	if (!WorldSettings || WorldSettings->bAllowAmbientOcclusion)
	{
		return new FAmbientOcclusionSceneProxy(this,WorldSettings);
	}
	else
	{
		return NULL;
	}
}

/**
 * @param View - current view
 * @return TRUE if the effect should be rendered
 */
UBOOL UAmbientOcclusionEffect::IsShown(const FSceneView* View) const
{
	UBOOL bIsShown = View->Family->bAllowAmbientOcclusion && UPostProcessEffect::IsShown(View);
	if (IsInGameThread())
	{
		bIsShown = bIsShown && GSystemSettings.bAllowAmbientOcclusion;
	}
	else
	{
		bIsShown = bIsShown && GSystemSettings.bAllowAmbientOcclusion;
	}
	return bIsShown;
}

void UAmbientOcclusionEffect::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Maximum world space threshold allowed
	const FLOAT WorldThresholdMax = 5000.0f;

	// Clamp settings to valid ranges.
	SceneDPG = SDPG_World;
	OcclusionPower = Clamp(OcclusionPower, 0.0001f, 50.0f);
	MinOcclusion = Clamp(MinOcclusion, 0.0f, 1.0f);
	OcclusionRadius = Max(0.0f, OcclusionRadius);
	EdgeDistanceThreshold = Clamp(EdgeDistanceThreshold, 0.0f, WorldThresholdMax);
	EdgeDistanceScale = Clamp(EdgeDistanceScale, 0.0f, 1.0f);
	HaloDistanceThreshold = Clamp(HaloDistanceThreshold, 0.0f, WorldThresholdMax);
	HaloOcclusion = Clamp(HaloOcclusion, 0.0f, 100.0f);
	HaloDistanceScale = Clamp(HaloDistanceScale, 0.0f, 1.0f);
	OcclusionFadeoutMinDistance = Clamp(OcclusionFadeoutMinDistance, 0.0f, OcclusionFadeoutMaxDistance);
	OcclusionFadeoutMaxDistance = Clamp(OcclusionFadeoutMaxDistance, OcclusionFadeoutMinDistance, (FLOAT)HALF_WORLD_MAX);
	FilterDistanceScale = Clamp(FilterDistanceScale, 1.0f, WorldThresholdMax);
	HistoryConvergenceTime = Clamp(HistoryConvergenceTime, 0.0f, 30.0f);
	HistoryWeightConvergenceTime = Clamp(HistoryWeightConvergenceTime, 0.0001f, 30.0f);
}	
