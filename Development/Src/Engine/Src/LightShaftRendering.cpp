/*=============================================================================
	LightShaftRendering.cpp: Light shaft implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "SceneFilterRendering.h"

#if WITH_REALD
#include "RealD/RealD.h"
#endif

#if WITH_MOBILE_RHI
extern void MobileSetNextDrawGlobalShader( EMobileGlobalShaderType GlobalShaderType );
#else
static FORCEINLINE void MobileSetNextDrawGlobalShader( EMobileGlobalShaderType GlobalShaderType )
{
}
#endif

DECLARE_STATS_GROUP(TEXT("Lights"),STATGROUP_Lights);
DECLARE_DWORD_COUNTER_STAT(TEXT("Light shafts"),STAT_NumLightShafts,STATGROUP_Lights);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Scene Lights"),STAT_NumSceneLights,STATGROUP_Lights);

const FLOAT PointLightFadeDistanceIncrease = 200;
const FLOAT PointLightRadiusFadeFactor = 5;

/** Sets spotlight parameters for light shafts on the passed in shader. */
extern void SetSpotLightShaftParameters(
	FShader* Shader, 
	const FLightSceneInfo* LightSceneInfo, 
	const FShaderParameter& WorldSpaceSpotDirectionParameter, 
	const FShaderParameter& SpotAnglesParameter);

/*-----------------------------------------------------------------------------
	FSimpleF32VertexShader
-----------------------------------------------------------------------------*/

class FSimpleF32VertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSimpleF32VertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	/** Default constructor. */
	FSimpleF32VertexShader() {}

	/** Initialization constructor. */
	FSimpleF32VertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	/** Sets shader parameter values */
	void SetParameters(const FViewInfo& View)
	{
	}
};

IMPLEMENT_SHADER_TYPE(,FSimpleF32VertexShader,TEXT("SimpleF32VertexShader"),TEXT("main"),SF_Vertex,VER_RES_INDEPENDENT_LIGHTSHAFTS,0);

/*-----------------------------------------------------------------------------
	FSimpleF32PixelShader
-----------------------------------------------------------------------------*/

class FSimpleF32PixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSimpleF32PixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE; 
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	/** Default constructor. */
	FSimpleF32PixelShader() {}

	/** Initialization constructor. */
	FSimpleF32PixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FGlobalShader(Initializer)
	{
		SourceTextureParameter.Bind(Initializer.ParameterMap,TEXT("SourceTexture"), TRUE);
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SourceTextureParameter;

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			SourceTextureParameter.SetBaseIndex(0, TRUE);
		}
#endif
		return bShaderHasOutdatedParameters;
	}

	/** Sets shader parameter values */
	void SetParameters(const FViewInfo& View, FTextureRHIParamRef TextureRHI)
	{
		SetTextureParameterDirectly(
			GetPixelShader(),
			SourceTextureParameter,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			TextureRHI );
	}
private:
	FShaderResourceParameter SourceTextureParameter;
};

IMPLEMENT_SHADER_TYPE(,FSimpleF32PixelShader,TEXT("SimpleF32PixelShader"),TEXT("main"),SF_Pixel,VER_RES_INDEPENDENT_LIGHTSHAFTS,0);



/*-----------------------------------------------------------------------------
	FLightShaftPixelShaderParameters
-----------------------------------------------------------------------------*/

/** Light shaft parameters that are shared between multiple pixel shaders. */
class FLightShaftPixelShaderParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		TextureSpaceBlurOriginParameter.Bind(ParameterMap,TEXT("TextureSpaceBlurOrigin"), TRUE);
		WorldSpaceBlurOriginAndRadiusParameter.Bind(ParameterMap,TEXT("WorldSpaceBlurOriginAndRadius"), TRUE);
		WorldSpaceSpotDirectionParameter.Bind(ParameterMap,TEXT("WorldSpaceSpotDirection"), TRUE);
		SpotAnglesParameter.Bind(ParameterMap,TEXT("SpotAngles"), TRUE);
		WorldSpaceCameraPositionParameter.Bind(ParameterMap,TEXT("WorldSpaceCameraPositionAndDistance"), TRUE);
		UVMinMaxParameter.Bind(ParameterMap,TEXT("UVMinMax"), TRUE);
		AspectRatioAndInvAspectRatioParameter.Bind(ParameterMap,TEXT("AspectRatioAndInvAspectRatio"), TRUE);
		LightShaftParameters.Bind(ParameterMap,TEXT("LightShaftParameters"), TRUE);
		BloomTintAndThresholdParameter.Bind(ParameterMap,TEXT("BloomTintAndThreshold"), TRUE);
		BloomScreenBlendThresholdParameter.Bind(ParameterMap,TEXT("BloomScreenBlendThreshold"), TRUE);
		DistanceFadeParameter.Bind(ParameterMap,TEXT("DistanceFade"), TRUE);
		SourceTextureParameter.Bind(ParameterMap,TEXT("SourceTexture"), TRUE);
	}

	friend FArchive& operator<<(FArchive& Ar,FLightShaftPixelShaderParameters& Parameters)
	{
		Ar << Parameters.TextureSpaceBlurOriginParameter;
		Ar << Parameters.WorldSpaceBlurOriginAndRadiusParameter;
		Ar << Parameters.SpotAnglesParameter;
		Ar << Parameters.WorldSpaceSpotDirectionParameter;
		Ar << Parameters.WorldSpaceCameraPositionParameter;
		Ar << Parameters.UVMinMaxParameter;
		Ar << Parameters.AspectRatioAndInvAspectRatioParameter;
		Ar << Parameters.LightShaftParameters;
		Ar << Parameters.BloomTintAndThresholdParameter;
		Ar << Parameters.BloomScreenBlendThresholdParameter;
		Ar << Parameters.DistanceFadeParameter;
		Ar << Parameters.SourceTextureParameter;

		Parameters.TextureSpaceBlurOriginParameter.SetShaderParamName(TEXT("TextureSpaceBlurOrigin"));
		Parameters.UVMinMaxParameter.SetShaderParamName(TEXT("UVMinMax"));
		Parameters.AspectRatioAndInvAspectRatioParameter.SetShaderParamName(TEXT("AspectRatioAndInvAspectRatio"));
		Parameters.LightShaftParameters.SetShaderParamName(TEXT("LightShaftParameters"));
		Parameters.BloomTintAndThresholdParameter.SetShaderParamName(TEXT("BloomTintAndThreshold"));
		return Ar;
	}

	void SetParameters(FShader* Shader, const FLightSceneInfo* LightSceneInfo, const FSceneView& View, FSceneRenderTargetIndex FilterColorIndex)
	{
		const UINT FilterDownsampleFactor = GSceneRenderTargets.GetFilterDownsampleFactor();
		const UINT DownsampledViewSizeX = appFloor(View.SizeX / FilterDownsampleFactor);
		const UINT DownsampledViewSizeY = appFloor(View.SizeY / FilterDownsampleFactor);
		const FVector2D ViewRatioOfBuffer((FLOAT)DownsampledViewSizeX / GSceneRenderTargets.GetFilterBufferSizeX(), (FLOAT)DownsampledViewSizeY / GSceneRenderTargets.GetFilterBufferSizeY());
		const FVector4 AspectRatioAndInvAspectRatio(
			ViewRatioOfBuffer.X, 
			(FLOAT)GSceneRenderTargets.GetFilterBufferSizeX() * ViewRatioOfBuffer.Y / GSceneRenderTargets.GetFilterBufferSizeY(), 
			1.0f / ViewRatioOfBuffer.X, 
			(FLOAT)GSceneRenderTargets.GetFilterBufferSizeY() / (GSceneRenderTargets.GetFilterBufferSizeX() * ViewRatioOfBuffer.Y));

		SetPixelShaderValue(Shader->GetPixelShader(),AspectRatioAndInvAspectRatioParameter,AspectRatioAndInvAspectRatio);

		const FVector WorldSpaceBlurOrigin = LightSceneInfo->GetPosition();
		// Transform into post projection space
		const FVector4 ProjectedBlurOrigin = View.ViewProjectionMatrix.TransformFVector(WorldSpaceBlurOrigin);
		// Perspective divide and transform into texture coordinates
		FVector2D ScreenSpaceBlurOrigin(
			ProjectedBlurOrigin.X / ProjectedBlurOrigin.W * View.ScreenPositionScaleBias.X + View.ScreenPositionScaleBias.W,
			ProjectedBlurOrigin.Y / ProjectedBlurOrigin.W * View.ScreenPositionScaleBias.Y + View.ScreenPositionScaleBias.Z);

		FVector2D TextureSpaceBlurOrigin(ScreenSpaceBlurOrigin * FVector2D(AspectRatioAndInvAspectRatio.Z, AspectRatioAndInvAspectRatio.W));

		SetPixelShaderValue(Shader->GetPixelShader(), TextureSpaceBlurOriginParameter, TextureSpaceBlurOrigin);

		SetPixelShaderValue(Shader->GetPixelShader(), WorldSpaceBlurOriginAndRadiusParameter, FVector4(WorldSpaceBlurOrigin, LightSceneInfo->GetRadius()));

		SetSpotLightShaftParameters(Shader, LightSceneInfo, WorldSpaceSpotDirectionParameter, SpotAnglesParameter);

		const FLOAT DistanceFromLight = ((FVector)View.ViewOrigin - WorldSpaceBlurOrigin).Size() + PointLightFadeDistanceIncrease;
		SetPixelShaderValue(Shader->GetPixelShader(), WorldSpaceCameraPositionParameter, FVector4(View.ViewOrigin, DistanceFromLight));

		const UINT FilterBufferSizeX = GSceneRenderTargets.GetFilterBufferSizeX();
		const UINT FilterBufferSizeY = GSceneRenderTargets.GetFilterBufferSizeY();
		const UINT DownsampledX = View.RenderTargetX / FilterDownsampleFactor;
		const UINT DownsampledY = View.RenderTargetY / FilterDownsampleFactor;
		const UINT DownsampledSizeX = View.RenderTargetSizeX / FilterDownsampleFactor;
		const UINT DownsampledSizeY = View.RenderTargetSizeY / FilterDownsampleFactor;

		// Limits for where the pixel shader is allowed to sample
		// Prevents reading from outside the valid region of a render target
		// Clamp to 1 less than the actual max, 
		// Since the bottom-right row/column of texels will contain some unwanted values if the size of scene color is not a factor of the downsample factor
		FLOAT MinU, MinV, MaxU, MaxV;
#if WITH_ES2_RHI && !FLASH
		if ( GUsingES2RHI )
		{
			MinU = DownsampledX / (FLOAT)FilterBufferSizeX;
			MinV = (FLOAT(FilterBufferSizeY) - (DownsampledSizeY + DownsampledY - 1)) / (FLOAT)FilterBufferSizeY;
			MaxU = (FLOAT(DownsampledX) + DownsampledSizeX - 1) / (FLOAT)FilterBufferSizeX;
			MaxV = (FLOAT(FilterBufferSizeY) - DownsampledY) / (FLOAT)FilterBufferSizeY;
		}
		else
#endif
		{
			MinU = DownsampledX / (FLOAT)FilterBufferSizeX;
			MinV = DownsampledY / (FLOAT)FilterBufferSizeY;
			MaxU = (FLOAT(DownsampledX) + DownsampledSizeX - 1) / (FLOAT)FilterBufferSizeX;
			MaxV = (FLOAT(DownsampledY) + DownsampledSizeY - 1) / (FLOAT)FilterBufferSizeY;
		}

		FVector4 UVMinMax( MinU, MinV, MaxU, MaxV );
		SetPixelShaderValue(Shader->GetPixelShader(), UVMinMaxParameter, UVMinMax);

		const FLinearColor BloomTint = LightSceneInfo->BloomTint;
		SetPixelShaderValue(Shader->GetPixelShader(), BloomTintAndThresholdParameter, FVector4(BloomTint.R, BloomTint.G, BloomTint.B, LightSceneInfo->BloomThreshold));

		FLOAT RadialBlurFactor = LightSceneInfo->RadialBlurPercent / 100.0f;
		const FVector4 LightShaftParameterValues(1.0f / LightSceneInfo->OcclusionDepthRange, LightSceneInfo->BloomScale, RadialBlurFactor, LightSceneInfo->OcclusionMaskDarkness);
		SetPixelShaderValue(Shader->GetPixelShader(), LightShaftParameters, LightShaftParameterValues);
		SetPixelShaderValue(Shader->GetPixelShader(), BloomScreenBlendThresholdParameter, LightSceneInfo->BloomScreenBlendThreshold);

		FLOAT DistanceFade = 0.0f;
		if (LightSceneInfo->LightType != LightType_Directional && LightSceneInfo->LightType != LightType_DominantDirectional)
		{
			DistanceFade = Clamp(DistanceFromLight / (LightSceneInfo->GetRadius() * PointLightRadiusFadeFactor), 0.0f, 1.0f);
		}

		SetPixelShaderValue(Shader->GetPixelShader(), DistanceFadeParameter, DistanceFade);

		SetTextureParameterDirectly(
			Shader->GetPixelShader(),
			SourceTextureParameter,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			GSceneRenderTargets.GetFilterColorTexture(FilterColorIndex)
			);
	}
private:
#if WITH_MOBILE_RHI
	friend class FApplyLightShaftsPixelShader;
	friend class FBlurLightShaftsPixelShader;
#endif
	FShaderParameter TextureSpaceBlurOriginParameter;
	FShaderParameter WorldSpaceBlurOriginAndRadiusParameter;
	FShaderParameter SpotAnglesParameter;
	FShaderParameter WorldSpaceSpotDirectionParameter;
	FShaderParameter WorldSpaceCameraPositionParameter;
	FShaderParameter UVMinMaxParameter;
	FShaderParameter AspectRatioAndInvAspectRatioParameter;
	FShaderParameter LightShaftParameters;
	FShaderParameter BloomTintAndThresholdParameter;
	FShaderParameter BloomScreenBlendThresholdParameter;
	FShaderParameter DistanceFadeParameter;
	FShaderResourceParameter SourceTextureParameter;
};

/*-----------------------------------------------------------------------------
	FDownsampleLightShaftsVertexShader
-----------------------------------------------------------------------------*/

class FDownsampleLightShaftsVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDownsampleLightShaftsVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	/** Default constructor. */
	FDownsampleLightShaftsVertexShader() {}

	/** Initialization constructor. */
	FDownsampleLightShaftsVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		ScreenToWorldParameter.Bind(Initializer.ParameterMap,TEXT("ScreenToWorld"), TRUE);
		//@BEGIN - JTM - Aug 23, 2012 02:50PM - Added shader parameter for multi-viewport lightshafts
		ScreenToViewportParameter.Bind(Initializer.ParameterMap,TEXT("ScreenToViewport"), TRUE);
		//@END
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << ScreenToWorldParameter;
		//@BEGIN - JTM - Aug 23, 2012 02:50PM - Added shader parameter for multi-viewport lightshafts
		Ar << ScreenToViewportParameter;
		//@END

		ScreenToWorldParameter.SetShaderParamName(TEXT("ScreenToWorld"));
		//@BEGIN - JTM - Aug 23, 2012 02:50PM - Added shader parameter for multi-viewport lightshafts
		ScreenToViewportParameter.SetShaderParamName(TEXT("ScreenToViewport"));
		//@END

		return bShaderHasOutdatedParameters;
	}

	/** Sets shader parameter values */
	void SetParameters(const FViewInfo& View)
	{
		//@BEGIN - JTM - Aug 23, 2012 02:50PM - Added shader parameter for multi-viewport lightshafts
		const UINT FilterBufferSizeX = GSceneRenderTargets.GetFilterBufferSizeX();
		const UINT FilterBufferSizeY = GSceneRenderTargets.GetFilterBufferSizeY();
		const UINT FilterDownsampleFactor = GSceneRenderTargets.GetFilterDownsampleFactor();
		const UINT DownsampledX = View.RenderTargetX / FilterDownsampleFactor;
		const UINT DownsampledY = View.RenderTargetY / FilterDownsampleFactor;
		const UINT DownsampledSizeX = View.RenderTargetSizeX / FilterDownsampleFactor;
		const UINT DownsampledSizeY = View.RenderTargetSizeY / FilterDownsampleFactor;

		const FLOAT Sx = (DownsampledSizeX > 0)
			? (FLOAT)FilterBufferSizeX / (FLOAT)DownsampledSizeX
			: 1.0f;
		const FLOAT Sy = (DownsampledSizeY > 0)
			? (FLOAT)FilterBufferSizeY / (FLOAT)DownsampledSizeY
			: 1.0f;
		const FLOAT Dx = (FilterBufferSizeX > 0)
			? ((2.0f * (FLOAT)DownsampledX + DownsampledSizeX) / (FLOAT)FilterBufferSizeX) - 1.0f
			: 0.0f;
		const FLOAT Dy = (FilterBufferSizeY > 0)
			? ((2.0f * (FLOAT)DownsampledY + DownsampledSizeY) / (FLOAT)FilterBufferSizeY) - 1.0f
			: 0.0f;

		FMatrix ScreenToViewport = FMatrix(
			FPlane(Sx,0,0,0),
			FPlane(0,Sy,0,0),
			FPlane(0,0,1,0),
			FPlane(-Sx * Dx,-Sy * Dy,0,1));

		SetVertexShaderValue(GetVertexShader(),ScreenToViewportParameter,ScreenToViewport);
		//@END

		FMatrix ScreenToWorld = FMatrix(
			FPlane(1,0,0,0),
			FPlane(0,1,0,0),
			FPlane(0,0,(1.0f - Z_PRECISION),1),
			FPlane(0,0,-View.NearClippingDistance * (1.0f - Z_PRECISION),0)
			) *
			View.InvTranslatedViewProjectionMatrix;

		// Set the view constants, as many as were bound to the parameter.
		SetVertexShaderValue(GetVertexShader(),ScreenToWorldParameter,ScreenToWorld);
	}

private:

	FShaderParameter ScreenToWorldParameter;
	//@BEGIN - JTM - Aug 23, 2012 02:50PM - Added shader parameter for multi-viewport lightshafts
	FShaderParameter ScreenToViewportParameter;
	//@END
};

IMPLEMENT_SHADER_TYPE(,FDownsampleLightShaftsVertexShader,TEXT("LightShaftShader"),TEXT("DownsampleLightShaftsVertexMain"),SF_Vertex,VER_RES_INDEPENDENT_LIGHTSHAFTS,0);

/** Enumerates the types of lights that are handled by the light shaft shaders. */
enum ELightShaftLightType
{
	LS_Point,
	LS_Spot,
	LS_Directional
};

/*-----------------------------------------------------------------------------
	TDownsampleLightShaftsPixelShader
-----------------------------------------------------------------------------*/

template<ELightShaftLightType LightType> 
class TDownsampleLightShaftsPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDownsampleLightShaftsPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE; 
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("POINT_LIGHT_SHAFTS"), *FString::Printf(TEXT("%u"), (UBOOL)(LightType == LS_Point || LightType == LS_Spot)));
		OutEnvironment.Definitions.Set(TEXT("SPOT_LIGHT_SHAFTS"), *FString::Printf(TEXT("%u"), (UBOOL)(LightType == LS_Spot)));
		OutEnvironment.Definitions.Set(TEXT("POINT_LIGHT_RADIUS_FADE_FACTOR"), *FString::Printf(TEXT("%f"), PointLightRadiusFadeFactor));
	}

	/** Default constructor. */
	TDownsampleLightShaftsPixelShader() {}

	/** Initialization constructor. */
	TDownsampleLightShaftsPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		LightShaftParameters.Bind(Initializer.ParameterMap);
		SampleOffsetsParameter.Bind(Initializer.ParameterMap,TEXT("SampleOffsets"), TRUE);
		SceneTextureParams.Bind(Initializer.ParameterMap);
		SmallSceneColorTextureParameter.Bind(Initializer.ParameterMap,TEXT("SmallSceneColorTexture"),TRUE);
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << LightShaftParameters;
		Ar << SampleOffsetsParameter;
		Ar << SceneTextureParams;
		Ar << SmallSceneColorTextureParameter;

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			SceneTextureParams.SceneColorTextureParameter.SetBaseIndex( 0, TRUE );
			SceneTextureParams.SceneDepthTextureParameter.SetBaseIndex( 1, TRUE );
			SceneTextureParams.SceneDepthCalcParameter.SetShaderParamName(TEXT("MinZ_MaxZRatio"));
			SampleOffsetsParameter.SetShaderParamName(TEXT("LightShaftSampleOffsets"));
		}
#endif

		return bShaderHasOutdatedParameters;
	}

	/** Sets shader parameter values */
	void SetParameters(const FLightSceneInfo* LightSceneInfo, const FViewInfo& View)
	{
		LightShaftParameters.SetParameters(this, LightSceneInfo, View, SRTI_FilterColor0);

		const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
		const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();
		FVector4 SampleOffsets[2];
		SampleOffsets[0] = FVector4(0, 0, 2.0f / BufferSizeX, 0);
		SampleOffsets[1] = FVector4(0, 2.0f / BufferSizeY, 2.0f / BufferSizeX, 2.0f / BufferSizeY);
		SetPixelShaderValue(GetPixelShader(),SampleOffsetsParameter,SampleOffsets);

		SceneTextureParams.Set(&View, this, SF_Bilinear);

#if XBOX
		// Use the quarter-size resolve texture instead of normal scene color.
		SetTextureParameter(
			GetPixelShader(),
			SmallSceneColorTextureParameter,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			GSceneRenderTargets.GetQuarterSizeSceneColorTexture()
			);
#endif

		RHIReduceTextureCachePenalty(GetPixelShader());
	}

private:

	FLightShaftPixelShaderParameters LightShaftParameters;
	FShaderParameter SampleOffsetsParameter;
	FSceneTextureShaderParameters SceneTextureParams;
	FShaderResourceParameter SmallSceneColorTextureParameter;
};

IMPLEMENT_SHADER_TYPE(template<>,TDownsampleLightShaftsPixelShader<LS_Point>,TEXT("LightShaftShader"),TEXT("DownsampleLightShaftsPixelMain"),SF_Pixel,VER_RES_INDEPENDENT_LIGHTSHAFTS,0);
IMPLEMENT_SHADER_TYPE(template<>,TDownsampleLightShaftsPixelShader<LS_Spot>,TEXT("LightShaftShader"),TEXT("DownsampleLightShaftsPixelMain"),SF_Pixel,VER_RES_INDEPENDENT_LIGHTSHAFTS,0);
IMPLEMENT_SHADER_TYPE(template<>,TDownsampleLightShaftsPixelShader<LS_Directional>,TEXT("LightShaftShader"),TEXT("DownsampleLightShaftsPixelMain"),SF_Pixel,VER_RES_INDEPENDENT_LIGHTSHAFTS,0);

/*-----------------------------------------------------------------------------
	FBlurLightShaftsPixelShader
-----------------------------------------------------------------------------*/

class FBlurLightShaftsPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FBlurLightShaftsPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE; 
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	/** Default constructor. */
	FBlurLightShaftsPixelShader() {}

	/** Initialization constructor. */
	FBlurLightShaftsPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		LightShaftParameters.Bind(Initializer.ParameterMap);
		BlurPassIndexParameter.Bind(Initializer.ParameterMap,TEXT("BlurPassIndex"), TRUE);
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << LightShaftParameters;
		Ar << BlurPassIndexParameter;

		LightShaftBlurParameter.SetShaderParamName( TEXT("LightShaftBlurParameters") );

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			LightShaftParameters.SourceTextureParameter.SetBaseIndex(0,TRUE);
		}
#endif

		return bShaderHasOutdatedParameters;
	}

	/** Sets shader parameter values */
	void SetParameters(const FLightSceneInfo* LightSceneInfo, const FViewInfo& View, INT BlurPassIndex, INT BlurPassCount, FSceneRenderTargetIndex SourceFilterBuffer, FLOAT BlurVectorScale)
	{
		LightShaftParameters.SetParameters(this, LightSceneInfo, View, SourceFilterBuffer);
		SetPixelShaderValue(GetPixelShader(),BlurPassIndexParameter,(FLOAT)BlurPassIndex);

#if WITH_MOBILE_RHI
		if ( GUsingMobileRHI )
		{
			FLOAT BlurPassPercent = 1.0f / (FLOAT)BlurPassCount;
			FVector4 LightShaftBlurValue( BlurPassPercent * BlurVectorScale, BlurPassIndex * BlurPassPercent * BlurVectorScale, BlurPassCount, 0.0f );
			SetVertexShaderValue(GetVertexShader(),LightShaftBlurParameter,LightShaftBlurValue);
		}
#endif
	}

private:

	FLightShaftPixelShaderParameters LightShaftParameters;
	FShaderParameter BlurPassIndexParameter;
	FShaderParameter LightShaftBlurParameter;
};

IMPLEMENT_SHADER_TYPE(,FBlurLightShaftsPixelShader,TEXT("LightShaftShader"),TEXT("BlurLightShaftsMain"),SF_Pixel,VER_RES_INDEPENDENT_LIGHTSHAFTS,0);

/*-----------------------------------------------------------------------------
	FApplyLightShaftsVertexShader
-----------------------------------------------------------------------------*/

class FApplyLightShaftsVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FApplyLightShaftsVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	/** Default constructor. */
	FApplyLightShaftsVertexShader() {}

	/** Initialization constructor. */
	FApplyLightShaftsVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		SourceTextureScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("SourceTextureScaleBias"));
		SceneColorScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("SceneColorScaleBias"));
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SourceTextureScaleBiasParameter;
		Ar << SceneColorScaleBiasParameter;

		SourceTextureScaleBiasParameter.SetShaderParamName(TEXT("SourceTextureScaleBias"));
		SceneColorScaleBiasParameter.SetShaderParamName(TEXT("SceneColorScaleBias"));
		return bShaderHasOutdatedParameters;
	}

	/** Sets shader parameter values */
	void SetParameters(const FViewInfo& View)
	{
		// Calculate the transform from NDC position into source texture UVs
		const UINT FilterDownsampleFactor = GSceneRenderTargets.GetFilterDownsampleFactor();
		const UINT DownsampledX = View.RenderTargetX / FilterDownsampleFactor;
		const UINT DownsampledY = View.RenderTargetY / FilterDownsampleFactor;
		// Round off view sizes that are not a multiple of the downsample factor
		const UINT DownsampledViewSizeX = appFloor(View.SizeX / FilterDownsampleFactor);
		const UINT DownsampledViewSizeY = appFloor(View.SizeY / FilterDownsampleFactor);
		const UINT FilterBufferSizeX = GSceneRenderTargets.GetFilterBufferSizeX();
		const UINT FilterBufferSizeY = GSceneRenderTargets.GetFilterBufferSizeY();
		const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
		const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();

		FLOAT ScaleX, ScaleY;
		FLOAT BiasX, BiasY;

		// Undo the viewport scale and bias to get back to -1..+1 space:
		FLOAT ViewportPosX  = (FLOAT)View.RenderTargetX     / (FLOAT)BufferSizeX;
		FLOAT ViewportPosY  = (FLOAT)View.RenderTargetY     / (FLOAT)BufferSizeY;
		FLOAT ViewportSizeX = (FLOAT)View.RenderTargetSizeX / (FLOAT)BufferSizeX;
		FLOAT ViewportSizeY = (FLOAT)View.RenderTargetSizeY / (FLOAT)BufferSizeY;
		FLOAT ViewportScaleX = ViewportSizeX;
		FLOAT ViewportScaleY = ViewportSizeY;
		FLOAT ViewportBiasX = +2.0f * ViewportPosX + ViewportSizeX - 1.0f;
		FLOAT ViewportBiasY = -2.0f * ViewportPosY - ViewportSizeY + 1.0f;

#if WITH_ES2_RHI
		if ( GUsingES2RHI )
		{
			INT MaxY = DownsampledY + DownsampledViewSizeY;
			ScaleX = (FLOAT)DownsampledViewSizeX / FilterBufferSizeX / +2.0f;
			ScaleY = (FLOAT)DownsampledViewSizeY / FilterBufferSizeY / +2.0f,
			BiasX = ((FLOAT)DownsampledViewSizeX / 2.0f + GPixelCenterOffset + DownsampledX) / FilterBufferSizeX;
			BiasY = ((FLOAT)DownsampledViewSizeY / 2.0f + GPixelCenterOffset + FilterBufferSizeY - MaxY) / FilterBufferSizeY;
		}
		else
#endif
		{
			ScaleX = (FLOAT)DownsampledViewSizeX / FilterBufferSizeX / +2.0f;
			ScaleY = (FLOAT)DownsampledViewSizeY / FilterBufferSizeY / -2.0f,
			BiasX = ((FLOAT)DownsampledViewSizeX / 2.0f + GPixelCenterOffset + DownsampledX) / FilterBufferSizeX;
			BiasY = ((FLOAT)DownsampledViewSizeY / 2.0f + GPixelCenterOffset + DownsampledY) / FilterBufferSizeY;
		}

		FVector4 SourceTextureScaleBias(
			 ScaleX / ViewportScaleX,
			 ScaleY / ViewportScaleY,
			-ScaleY / ViewportScaleY * ViewportBiasY + BiasY,
			-ScaleX / ViewportScaleX * ViewportBiasX + BiasX
			);
		SetVertexShaderValue(GetVertexShader(),SourceTextureScaleBiasParameter,SourceTextureScaleBias);
		
#if XBOX
		{
			// Calculate the transform from NDC position into downsampled scene color UVs
			const UINT SceneColorDownsampleFactor = GSceneRenderTargets.GetSmallColorDepthDownsampleFactor();
			const UINT DownsampledX = View.RenderTargetX / SceneColorDownsampleFactor;
			const UINT DownsampledY = View.RenderTargetY / SceneColorDownsampleFactor;
			// Round off view sizes that are not a multiple of the downsample factor
			const UINT DownsampledViewSizeX = appFloor(View.SizeX / SceneColorDownsampleFactor);
			const UINT DownsampledViewSizeY = appFloor(View.SizeY / SceneColorDownsampleFactor);
			const UINT DownsampledSceneColorBufferSizeX = GSceneRenderTargets.GetBufferSizeX() / SceneColorDownsampleFactor;
			const UINT DownsampledSceneColorBufferSizeY = GSceneRenderTargets.GetBufferSizeY() / SceneColorDownsampleFactor;

			const FVector4 SceneColorScaleBias(
				(FLOAT)DownsampledViewSizeX / DownsampledSceneColorBufferSizeX / +2.0f,
				(FLOAT)DownsampledViewSizeY / DownsampledSceneColorBufferSizeY / -2.0f,
				((FLOAT)DownsampledViewSizeY / 2.0f + GPixelCenterOffset + DownsampledY) / DownsampledSceneColorBufferSizeY,
				((FLOAT)DownsampledViewSizeX / 2.0f + GPixelCenterOffset + DownsampledX) / DownsampledSceneColorBufferSizeX
				);
			SetVertexShaderValue(GetVertexShader(),SceneColorScaleBiasParameter,SceneColorScaleBias);
		}
#else
		// Apply the new screen-to-texture transform:
		FVector4 ScreenPositionScaleBias(
			 View.ScreenPositionScaleBias.X / ViewportScaleX,
			 View.ScreenPositionScaleBias.Y / ViewportScaleY,
			-View.ScreenPositionScaleBias.Y / ViewportScaleY * ViewportBiasY + View.ScreenPositionScaleBias.Z,
			-View.ScreenPositionScaleBias.X / ViewportScaleX * ViewportBiasX + View.ScreenPositionScaleBias.W
		);

		// Sets the new scale and bias as:
		//   (XY - OldBias)/OldScale * NewScale + NewBias
		//   --> XY * (NewScale/OldScale) + (-OldBias*NewScale/OldScale + NewBias)
		SetVertexShaderValue(GetVertexShader(),SceneColorScaleBiasParameter,ScreenPositionScaleBias);
#endif
	}

private:

	FShaderParameter SourceTextureScaleBiasParameter;
	FShaderParameter SceneColorScaleBiasParameter;
};

IMPLEMENT_SHADER_TYPE(,FApplyLightShaftsVertexShader,TEXT("LightShaftShader"),TEXT("ApplyLightShaftsVertexMain"),SF_Vertex,VER_RES_INDEPENDENT_LIGHTSHAFTS,0);

/*-----------------------------------------------------------------------------
	FApplyLightShaftsPixelShader
-----------------------------------------------------------------------------*/

class FApplyLightShaftsPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FApplyLightShaftsPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE; 
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	/** Default constructor. */
	FApplyLightShaftsPixelShader() {}

	/** Initialization constructor. */
	FApplyLightShaftsPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		LightShaftParameters.Bind(Initializer.ParameterMap);
		SceneTextureParams.Bind(Initializer.ParameterMap);
		SmallSceneColorTextureParameter.Bind(Initializer.ParameterMap,TEXT("SmallSceneColorTexture"),TRUE);
	}

	/** Serializer */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << LightShaftParameters;
		Ar << SceneTextureParams;
		Ar << SmallSceneColorTextureParameter;

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			LightShaftParameters.BloomScreenBlendThresholdParameter.SetShaderParamName(TEXT("BloomScreenBlendThreshold"));
			LightShaftParameters.DistanceFadeParameter.SetShaderParamName(TEXT("DistanceFade"));
			LightShaftParameters.SourceTextureParameter.SetBaseIndex(1,TRUE);
			SceneTextureParams.SceneColorTextureParameter.SetBaseIndex(0,TRUE);
		}
#endif

		return bShaderHasOutdatedParameters;
	}

	/** Sets shader parameter values */
	void SetParameters(const FLightSceneInfo* LightSceneInfo, const FViewInfo& View, FSceneRenderTargetIndex FilterColorIndex)
	{
		LightShaftParameters.SetParameters(this, LightSceneInfo, View, FilterColorIndex);

#if WITH_MOBILE_RHI
		if ( GUsingMobileRHI )
		{
			SceneTextureParams.Set(&View, this, SF_Point);
		}
		else
#endif
		{
			SceneTextureParams.Set(&View, this, SF_Bilinear);
		}

#if XBOX
		// Use the quarter-size resolve texture instead of normal scene color.
		SetTextureParameter(
			GetPixelShader(),
			SmallSceneColorTextureParameter,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			GSceneRenderTargets.GetQuarterSizeSceneColorTexture()
			);
#endif
	}

private:

	FLightShaftPixelShaderParameters LightShaftParameters;
	FSceneTextureShaderParameters SceneTextureParams;
	FShaderResourceParameter SmallSceneColorTextureParameter;
};

IMPLEMENT_SHADER_TYPE(,FApplyLightShaftsPixelShader,TEXT("LightShaftShader"),TEXT("ApplyLightShaftsPixelMain"),SF_Pixel,VER_RES_INDEPENDENT_LIGHTSHAFTS,0);

FGlobalBoundShaderState DownsamplePointLightShaftsBoundShaderState;
FGlobalBoundShaderState DownsampleSpotLightShaftsBoundShaderState;
FGlobalBoundShaderState DownsampleDirectionalLightShaftsBoundShaderState;
FGlobalBoundShaderState BlurLightShaftsBoundShaderState;
FGlobalBoundShaderState ApplyLightShaftsBoundShaderState;

/** The apply vertex declaration resource type. */
class FApplyVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor.
	virtual ~FApplyVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		Elements.AddItem(FVertexElement(0,0,VET_Float2,VEU_Position,0));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FApplyVertexDeclaration> GApplyLightShaftsVertexDeclaration;

/** Resolves an MSAA depth surface to the ResolvedDepthBuffer texture, if necessary. */
void FSceneRenderer::ResolveMSAADepthSurface( const FViewInfo& View )
{
#if USE_GXM_RHI
	// Downsample MSAA depth buffer if necessary
	extern INT MobileGetMSAAFactor();
	if ( MobileGetMSAAFactor() == 4 )
	{
		const UINT SrcSizeX = GSceneRenderTargets.GetBufferSizeX() * 2;
		const UINT SrcSizeY = GSceneRenderTargets.GetBufferSizeY() * 2;
		const UINT DstSizeX = SrcSizeX / 2;
		const UINT DstSizeY = SrcSizeY / 2;
		FIntRect SrcRect(FIntPoint(0, 0), FIntPoint(SrcSizeX, SrcSizeY));
		FIntRect DstRect(FIntPoint(0, 0), FIntPoint(DstSizeX, DstSizeY));
		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
		RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
		RHISetBlendState(TStaticBlendState<>::GetRHI());
		RHISetRenderTarget(GSceneRenderTargets.GetResolvedDepthSurface(), FSurfaceRHIRef());
		RHISetViewport(0, 0, 0.0f, DstSizeX, DstSizeY, 1.0f);
		TShaderMapRef<FSimpleF32VertexShader> VertexShader(GetGlobalShaderMap());
		TShaderMapRef<FSimpleF32PixelShader> PixelShader(GetGlobalShaderMap());
		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState( BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));
		VertexShader->SetParameters( View );
		PixelShader->SetParameters( View, GSceneRenderTargets.GetSceneDepthTexture() );
		MobileSetNextDrawGlobalShader( EGST_SimpleF32 );
		DrawDenormalizedQuad(
			DstRect.Min.X, DstRect.Min.Y, DstRect.Width(), DstRect.Height(),
			SrcRect.Min.X, SrcRect.Min.X, SrcRect.Width(), SrcRect.Height(),
			DstSizeX, DstSizeY,
			SrcSizeX, SrcSizeY, 0.5f
			);
	}
#endif
}

/**
 * Applies a radial blur to the specified filter buffer, for light shafts.
 *
 * @param View					The current view
 * @param LightSceneInfo		The current light that produces light shafts
 * @param SourceFilterBuffer	The filter buffer to use as source texture, containing the downsampled scene color with light shaft parameter in the alpha channel
 * @param DestFilterBuffer		The filter buffer to render the blurred result into
 * @param BlurVectorScale		Scale factor for the blur vector length
 */
void ApplyRadialBlur( const FViewInfo& View, const FLightSceneInfo* const LightSceneInfo, const FSceneRenderTargetIndex SourceFilterBuffer, const FSceneRenderTargetIndex DestFilterBuffer, FLOAT BlurVectorScale, INT NumPasses )
{
	TShaderMapRef<FScreenVertexShader> ScreenVertexShader(GetGlobalShaderMap());

	const UINT FilterDownsampleFactor = GSceneRenderTargets.GetFilterDownsampleFactor();
	const UINT DownsampledX = View.RenderTargetX / FilterDownsampleFactor;
	const UINT DownsampledY = View.RenderTargetY / FilterDownsampleFactor;
	const UINT DownsampledSizeX = View.RenderTargetSizeX / FilterDownsampleFactor;
	const UINT DownsampledSizeY = View.RenderTargetSizeY / FilterDownsampleFactor;
	const UINT FilterBufferSizeX = GSceneRenderTargets.GetFilterBufferSizeX();
	const UINT FilterBufferSizeY = GSceneRenderTargets.GetFilterBufferSizeY();

	GSceneRenderTargets.BeginRenderingFilter(DestFilterBuffer);

#if WITH_MOBILE_RHI
	RHIClear(TRUE,FLinearColor(0,0,0,0),FALSE,0,FALSE,0);
#endif

	for (INT PassIndex = 0; PassIndex < NumPasses; PassIndex++)
	{
#if WITH_MOBILE_RHI
		if ( GUsingMobileRHI && PassIndex > 0 )
		{
			RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_One,BF_One>::GetRHI());
		}
		else
#endif		
		{
			RHISetBlendState(TStaticBlendState<>::GetRHI());
		}

		TShaderMapRef<FBlurLightShaftsPixelShader> BlurLightShaftsPixelShader(GetGlobalShaderMap());
		SetGlobalBoundShaderState(BlurLightShaftsBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *BlurLightShaftsPixelShader, sizeof(FFilterVertex), NULL, EGST_LightShaftBlur);
		BlurLightShaftsPixelShader->SetParameters(LightSceneInfo, View, PassIndex, NumPasses, SourceFilterBuffer, BlurVectorScale);

		{
			SCOPED_DRAW_EVENT(EventBlurLightShafts)(DEC_SCENE_ITEMS,TEXT("RadialBlur"));
			// Apply a radial blur to the bloom and occlusion mask
			DrawDenormalizedQuad( 
				DownsampledX, DownsampledY, 
				DownsampledSizeX, DownsampledSizeY,
				DownsampledX, DownsampledY, 
				DownsampledSizeX, DownsampledSizeY,
				FilterBufferSizeX, FilterBufferSizeY,
				FilterBufferSizeX, FilterBufferSizeY);
		}
	}

	GSceneRenderTargets.FinishRenderingFilter(DestFilterBuffer);
}

#if WITH_MOBILE_RHI
/** The position only vertex declaration resource type. */
class FPositionOnlyVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual ~FPositionOnlyVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		Elements.AddItem(FVertexElement(0,0,VET_Float4,VEU_Position,0));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};
TGlobalResource<FPositionOnlyVertexDeclaration> GPositionOnlyVertexDeclaration;

void DrawBlackQuad(
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
	)
{
	static FGlobalBoundShaderState BoundShaderState;
	static FShaderParameter ViewProjectionParameter;
	SetGlobalBoundShaderState(BoundShaderState, GPositionOnlyVertexDeclaration.VertexDeclarationRHI, NULL, NULL, sizeof(FVector4), 0, EGST_PositionOnly);
	if ( !ViewProjectionParameter.IsBound() )
	{
		ViewProjectionParameter.SetShaderParamName( TEXT("ViewProjection") );
	}
	SetVertexShaderValue(NULL, ViewProjectionParameter, FMatrix::Identity);
	DrawDenormalizedQuad(X, Y, SizeX, SizeY, U, V, SizeU, SizeV, TargetSizeX, TargetSizeY, TextureSizeX, TextureSizeY, ClipSpaceQuadZ );
}
#endif

//@todo ib2merge: Put these into SystemSettings? The Ratio was moved, but not the drawcount...
INT GRadialBlurFirstPassDrawCount = 2;
INT GRadialBlurSecondPassDrawCount = 2;

/** Renders light shafts. */
UBOOL FSceneRenderer::RenderLightShafts()
{
	UBOOL bRenderedLightShafts = FALSE;
	UBOOL bAnyViewAllowsLightShafts = FALSE;

	for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		bAnyViewAllowsLightShafts = bAnyViewAllowsLightShafts || Views(ViewIndex).RenderingOverrides.bAllowLightShafts;
	}

	if (GSystemSettings.bAllowLightShafts
		&& bAnyViewAllowsLightShafts
		&& (ViewFamily.ShowFlags & SHOW_Lighting)
		&& (ViewFamily.ShowFlags & SHOW_LightShafts)
		&& !(ViewFamily.ShowFlags & SHOW_ShaderComplexity)
		&& !(ViewFamily.ShowFlags & SHOW_VisualizeSSAO)
		// Doesn't work with tiledshot due to being a screenspace effect
		&& !GIsTiledScreenshot)
	{
		UBOOL bResolvedSceneColor = FALSE;
		FSceneRenderTargetIndex DestFilterBuffer = SRTI_FilterColor1;
		FSceneRenderTargetIndex TempFilterBuffer = SRTI_FilterColor0;

		for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
		{
			const FLightSceneInfo* const LightSceneInfo = LightIt->LightSceneInfo;

			if (LightSceneInfo->bRenderLightShafts 
				&& LightSceneInfo->LightType != LightType_Sky 
				&& LightSceneInfo->LightType != LightType_SphericalHarmonic
				// Only render if the additive or darkening components will be visible
				&& (LightSceneInfo->BloomScale > 0.001f || LightSceneInfo->OcclusionMaskDarkness < 1.0f))
			{
				SCOPED_DRAW_EVENT(EventLightShafts)(DEC_SCENE_ITEMS,TEXT("LightShafts %s"),*LightSceneInfo->GetLightName().ToString());
				
				const UINT BufferSizeX				= GSceneRenderTargets.GetBufferSizeX();
				const UINT BufferSizeY				= GSceneRenderTargets.GetBufferSizeY();
				const UINT FilterBufferSizeX		= GSceneRenderTargets.GetFilterBufferSizeX();
				const UINT FilterBufferSizeY		= GSceneRenderTargets.GetFilterBufferSizeY();
				const UINT FilterDownsampleFactor	= GSceneRenderTargets.GetFilterDownsampleFactor();
				const FVector WorldSpaceBlurOrigin	= LightSceneInfo->GetPosition();

				for (INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
				{
					FViewInfo& View = Views(ViewIndex);

					const UINT DownsampledX			= View.RenderTargetX / FilterDownsampleFactor;
					const UINT DownsampledY			= View.RenderTargetY / FilterDownsampleFactor;
					const UINT DownsampledSizeX		= View.RenderTargetSizeX / FilterDownsampleFactor;
					const UINT DownsampledSizeY		= View.RenderTargetSizeY / FilterDownsampleFactor;

					// Transform into post projection space
					const FVector4 ProjectedBlurOrigin = View.ViewProjectionMatrix.TransformFVector(WorldSpaceBlurOrigin);

					const FLOAT DistanceToBlurOrigin = ((FVector)View.ViewOrigin - WorldSpaceBlurOrigin).Size() + PointLightFadeDistanceIncrease;
					// Don't render if the light's origin is behind the view
					if (ProjectedBlurOrigin.W >= 0.0f
						&& View.RenderingOverrides.bAllowLightShafts
						// Don't render point lights that have completely faded out
						&& (LightSceneInfo->LightType == LightType_Directional 
						|| LightSceneInfo->LightType == LightType_DominantDirectional
						|| DistanceToBlurOrigin < LightSceneInfo->GetRadius() * PointLightRadiusFadeFactor))
					{
						bRenderedLightShafts = TRUE;

						// Resolves an MSAA depth surface to the ResolvedDepthBuffer texture, if necessary (NGP). 
						ResolveMSAADepthSurface(View);

						if (!bResolvedSceneColor)
						{
#if WITH_MOBILE_RHI
							if ( GUsingMobileRHI && !GSupportsDepthTextures )
							{
								// Flag the alpha channel for all pixels that passes the depth test.
								GSceneRenderTargets.BeginRenderingSceneColor();
								RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());
								RHISetBlendState(TStaticBlendState<>::GetRHI());
								RHISetColorWriteMask(CW_ALPHA);
								RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
								static FLOAT ClipSpaceZ = 0.996f;
								DrawBlackQuad( 
									0, 0, 
									FamilySizeX, FamilySizeY,
									0, 0, 
									FamilySizeX, FamilySizeY,
									BufferSizeX, BufferSizeY,
									BufferSizeX, BufferSizeY,
									ClipSpaceZ );
								RHISetColorWriteMask(CW_RGBA);
								GSceneRenderTargets.FinishRenderingSceneColor(FALSE);
							}
#endif

							bResolvedSceneColor = TRUE;
#if XBOX && !USE_NULL_RHI
							// Downsample via resolve
							// The surface was created with 4xMSAA, so the resolve operation will combine multiple full resolution samples
							// This method of downsampling only takes .07ms, instead of the usual .53ms to resolve scene color
							// Additionally, the downsample shader will be faster because there will be less texture cache thrashing (smaller texture and 32bpp instead of 64)
							// Resolve with an exponent bias of +3 to make the best use of PF_A2B10G10R10 precision (on top of the already existing +3), for a total of +6
							FSurfaceRHIRef ResolveRef = FSurfaceRHIRef(GSceneRenderTargets.GetQuarterSizeSceneColorTexture(),GSceneRenderTargets.GetQuarterSizeSceneColorSurface(),FXeSurfaceInfo(0,0,6,0));
							FResolveRect ResolveRect(0,0,BufferSizeX,BufferSizeY);
							RHICopyToResolveTarget(ResolveRef, FALSE, FResolveParams(ResolveRect));
#else
							// Resolve scene color once so that it can be read from
							// Note: assuming scene depth has already been resolved
							UBOOL bKeepOriginalSurface = TRUE;
							GSceneRenderTargets.ResolveSceneColor(FResolveRect(0, 0, FamilySizeX, FamilySizeY), bKeepOriginalSurface);
#endif
						}

						GSceneRenderTargets.BeginRenderingFilter(DestFilterBuffer);

						// Clear the buffer to black, hides sampling out of bounds artifacts that happen in 480p
						RHIClear(TRUE,FLinearColor(0,0,0,0),FALSE,0,FALSE,0);

						// Set the device viewport for the view.
						RHISetViewParameters(View);

						// Set shaders and texture
						TShaderMapRef<FDownsampleLightShaftsVertexShader> DownsampleLightShaftsVertexShader(GetGlobalShaderMap());

						if (LightSceneInfo->LightType == LightType_Directional || LightSceneInfo->LightType == LightType_DominantDirectional)
						{
							TShaderMapRef<TDownsampleLightShaftsPixelShader<LS_Directional> > DownsampleLightShaftsPixelShader(GetGlobalShaderMap());
							SetGlobalBoundShaderState(DownsampleDirectionalLightShaftsBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *DownsampleLightShaftsVertexShader, *DownsampleLightShaftsPixelShader, sizeof(FFilterVertex));
							DownsampleLightShaftsPixelShader->SetParameters(LightSceneInfo, View);
						}
						else if(LightSceneInfo->LightType == LightType_Spot || LightSceneInfo->LightType == LightType_DominantSpot)
						{
							TShaderMapRef<TDownsampleLightShaftsPixelShader<LS_Spot> > DownsampleLightShaftsPixelShader(GetGlobalShaderMap());
							SetGlobalBoundShaderState(DownsampleSpotLightShaftsBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *DownsampleLightShaftsVertexShader, *DownsampleLightShaftsPixelShader, sizeof(FFilterVertex));
							DownsampleLightShaftsPixelShader->SetParameters(LightSceneInfo, View);
						}
						else
						{
							TShaderMapRef<TDownsampleLightShaftsPixelShader<LS_Point> > DownsampleLightShaftsPixelShader(GetGlobalShaderMap());
							SetGlobalBoundShaderState(DownsamplePointLightShaftsBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *DownsampleLightShaftsVertexShader, *DownsampleLightShaftsPixelShader, sizeof(FFilterVertex));
							DownsampleLightShaftsPixelShader->SetParameters(LightSceneInfo, View);
						}

						DownsampleLightShaftsVertexShader->SetParameters(View);

						// No depth tests, no backface culling.
						RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
						RHISetBlendState(TStaticBlendState<>::GetRHI());
						RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());

						MobileSetNextDrawGlobalShader( GSupportsDepthTextures ? EGST_LightShaftDownSample : EGST_LightShaftDownSample_NoDepth );

						{
							SCOPED_DRAW_EVENT(EventDownsampleLightShafts)(DEC_SCENE_ITEMS,TEXT("Downsample"));

							// Downsample scene color and depth, and convert them into a bloom term and an occlusion masking term
							DrawDenormalizedQuad( 
								DownsampledX, DownsampledY, 
								DownsampledSizeX, DownsampledSizeY,
								View.RenderTargetX, View.RenderTargetY, 
								View.RenderTargetSizeX, View.RenderTargetSizeY,
								FilterBufferSizeX, FilterBufferSizeY,
								BufferSizeX, BufferSizeY );

							GSceneRenderTargets.FinishRenderingFilter(DestFilterBuffer);
						}

#if WITH_MOBILE_RHI
						if ( GUsingMobileRHI )
						{
							// Do two blur passes, 16 taps each. One pass uses a much smaller blur vector.
							ApplyRadialBlur( View, LightSceneInfo, DestFilterBuffer, TempFilterBuffer, GSystemSettings.MobileLightShaftRadialBlurFirstPassRatio, GRadialBlurFirstPassDrawCount );
							Swap( TempFilterBuffer, DestFilterBuffer );
							ApplyRadialBlur( View, LightSceneInfo, DestFilterBuffer, TempFilterBuffer, GSystemSettings.MobileLightShaftRadialBlurSecondPassRatio, GRadialBlurSecondPassDrawCount );
						}
						else
#endif
						{
							//@todo - implement multipass blurring which should be more efficient on non-mobile platforms as well.
							ApplyRadialBlur( View, LightSceneInfo, DestFilterBuffer, TempFilterBuffer, 1.0f, 1 );
						}

						GSceneRenderTargets.BeginRenderingSceneColor();

#if FLASH
						RHIClear(TRUE,FLinearColor(0,0,0,0),TRUE,1.0f,TRUE,0);
#elif WITH_MOBILE_RHI
						if (GUsingMobileRHI)
						{
							RHIClear(TRUE,FLinearColor(0,0,0,0),FALSE,1.0f,FALSE,0);
						}
#endif

#if WITH_MOBILE_RHI
						if (GUsingMobileRHI)
						{
							RHISetBlendState(TStaticBlendState<>::GetRHI());
						}
						else
#endif		
						{
							RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_SourceAlpha>::GetRHI());
						}

						TShaderMapRef<FApplyLightShaftsVertexShader> ApplyLightShaftsVertexShader(GetGlobalShaderMap());
						TShaderMapRef<FApplyLightShaftsPixelShader> ApplyLightShaftsPixelShader(GetGlobalShaderMap());

						SetGlobalBoundShaderState(ApplyLightShaftsBoundShaderState, GApplyLightShaftsVertexDeclaration.VertexDeclarationRHI, *ApplyLightShaftsVertexShader, *ApplyLightShaftsPixelShader, sizeof(FVector2D), NULL, EGST_LightShaftApply);

						ApplyLightShaftsVertexShader->SetParameters(View);
						ApplyLightShaftsPixelShader->SetParameters(LightSceneInfo, View, TempFilterBuffer);

						// Preserve scene color alpha
						RHISetColorWriteMask(CW_RGB);
						{
							SCOPED_DRAW_EVENT(EventApplyLightShafts)(DEC_SCENE_ITEMS,TEXT("Apply"));

							FVector2D Vertices[4] =
							{
								FVector2D(View.RenderTargetX,                          View.RenderTargetY),
								FVector2D(View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY),
								FVector2D(View.RenderTargetX,                          View.RenderTargetY + View.RenderTargetSizeY),
								FVector2D(View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY),
							};
							static const WORD Indices[6] =
							{
								0, 1, 3,
								0, 3, 2
							};
							for ( INT VertexIndex=0; VertexIndex < 4; ++VertexIndex )
							{
								Vertices[VertexIndex].X = -1.0f + 2.0f * Vertices[VertexIndex].X / (FLOAT)BufferSizeX;
								Vertices[VertexIndex].Y = +1.0f - 2.0f * Vertices[VertexIndex].Y / (FLOAT)BufferSizeY;
							}
							// Apply the bloom and occlusion mask to scene color
							// Can't use DrawDenormalizedQuad here because we need 2 UV sets for 2 differently sized textures, 
							// And DrawDenormalizedQuad embeds the half texel offset in the vertex position.
							RHIDrawIndexedPrimitiveUP(
								PT_TriangleList,
								0,
								ARRAY_COUNT(Vertices),
								2,
								Indices,
								sizeof(Indices[0]),
								Vertices,
								sizeof(Vertices[0])
								);

							// No need to resolve since we used alpha blending
							GSceneRenderTargets.FinishRenderingSceneColor(FALSE);

							INC_DWORD_STAT(STAT_NumLightShafts);
						}
						RHISetColorWriteMask(CW_RGBA);
					}
				}
			}
		}
	}

	return bRenderedLightShafts;
}
