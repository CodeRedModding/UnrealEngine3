/*=============================================================================
	DOFAndBloomEffect.cpp: Combined depth of field and bloom effect implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"

IMPLEMENT_CLASS(UDOFAndBloomEffect);
IMPLEMENT_CLASS(UBlurEffect);

/*-----------------------------------------------------------------------------
FDOFShaderParameters
-----------------------------------------------------------------------------*/

/** Initialization constructor. */
FDOFShaderParameters::FDOFShaderParameters(const FShaderParameterMap& ParameterMap)
{
	Bind(ParameterMap);
}

void FDOFShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	PackedParameters0.Bind(ParameterMap,TEXT("PackedParameters"),TRUE);
	PackedParameters1.Bind(ParameterMap,TEXT("MinMaxBlurClamp"),TRUE);
}

/** Serializer. */
FArchive& operator<<(FArchive& Ar,FDOFShaderParameters& P)
{
	Ar << P.PackedParameters0;
	Ar << P.PackedParameters1;
	return Ar;
}

/** Set the dof pixel shader parameter values. */
void FDOFShaderParameters::SetPS(FShader* PixelShader, const FDepthOfFieldParams& DepthOfFieldParams) const
{
	if(PackedParameters0.IsBound() || PackedParameters1.IsBound())
	{
		FVector4 ShaderConstants[2];

		ComputeShaderConstants(DepthOfFieldParams, ShaderConstants);

		const FPixelShaderRHIParamRef ShaderRHI = PixelShader->GetPixelShader();

		SetShaderValue(ShaderRHI, PackedParameters0, ShaderConstants[0]);
		SetShaderValue(ShaderRHI, PackedParameters1, ShaderConstants[1]);
	}
}

/** Set the dof vertex shader parameter values from SceneView */
void FDOFShaderParameters::SetVS(FShader* VertexShader, const FDepthOfFieldParams& DepthOfFieldParams) const
{
	if(PackedParameters0.IsBound() || PackedParameters1.IsBound())
	{
		// Do computation only if needed
		FVector4 ShaderConstants[2];

		ComputeShaderConstants(DepthOfFieldParams, ShaderConstants);

		const FVertexShaderRHIParamRef ShaderRHI = VertexShader->GetVertexShader();

		SetShaderValue(ShaderRHI, PackedParameters0, ShaderConstants[0]);
		SetShaderValue(ShaderRHI, PackedParameters1, ShaderConstants[1]);
	}
}

#if WITH_D3D11_TESSELLATION
/** Set the dof geometry shader parameter values. */
void FDOFShaderParameters::SetGS(FShader* GeometryShader, const FDepthOfFieldParams& DepthOfFieldParams) const
{
	if(PackedParameters0.IsBound() || PackedParameters1.IsBound())
	{
		FVector4 ShaderConstants[2];

		ComputeShaderConstants(DepthOfFieldParams, ShaderConstants);

		const FGeometryShaderRHIParamRef ShaderRHI = GeometryShader->GetGeometryShader();

		SetShaderValue(ShaderRHI, PackedParameters0, ShaderConstants[0]);
		SetShaderValue(ShaderRHI, PackedParameters1, ShaderConstants[1]);
	}
}

/** Set the dof domain shader parameter values from SceneView */
void FDOFShaderParameters::SetDS(FShader* DomainShader, const FDepthOfFieldParams& DepthOfFieldParams) const
{
	if(PackedParameters0.IsBound() || PackedParameters1.IsBound())
	{
		// Do computation only if needed
		FVector4 ShaderConstants[2];

		ComputeShaderConstants(DepthOfFieldParams, ShaderConstants);

		SetDomainShaderValue(DomainShader->GetDomainShader(), PackedParameters0, ShaderConstants[0]);
		SetDomainShaderValue(DomainShader->GetDomainShader(), PackedParameters1, ShaderConstants[1]);
	}
}
#endif

void FDOFShaderParameters::ComputeShaderConstants(const FDepthOfFieldParams& Params, FVector4 Out[2])
{
	UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
	UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();

	Out[0] = FVector4(Params.FocusDistance, 1.0f / Params.FocusInnerRadius, Params.FalloffExponent, Clamp(Params.MinBlurAmount, 0.0f, 1.0f));
	Out[1] = FVector4(Min(Params.MaxNearBlurAmount, 1.0f), Min(Params.MaxFarBlurAmount, 1.0f), 1.0f / BufferSizeX, 1.0f / BufferSizeY);
}

/*-----------------------------------------------------------------------------
FPostProcessParameters
-----------------------------------------------------------------------------*/

/** Initialization constructor. */
FPostProcessParameters::FPostProcessParameters(const FShaderParameterMap& ParameterMap)
{
	FilterColor0TextureParameter.Bind(ParameterMap,TEXT("FilterColor0Texture"),TRUE);
	FilterColor1TextureParameter.Bind(ParameterMap,TEXT("FilterColor1Texture"),TRUE);
	FilterColor2TextureParameter.Bind(ParameterMap,TEXT("FilterColor2Texture"),TRUE);
	SeparateTranslucencyTextureParameter.Bind(ParameterMap,TEXT("SeparateTranslucencyTexture"),TRUE);
	DOFKernelSizeParameter.Bind(ParameterMap,TEXT("DOFKernelSize"),TRUE);
}

/** Serializer. */
FArchive& operator<<(FArchive& Ar,FPostProcessParameters& P)
{
	Ar << P.FilterColor0TextureParameter << P.FilterColor1TextureParameter
		<< P.FilterColor2TextureParameter << P.SeparateTranslucencyTextureParameter
		<< P.DOFKernelSizeParameter;
	
#if WITH_MOBILE_RHI
	if (GUsingMobileRHI)
	{
		// Hard-coded to match the shader (it's serialized based on the PC shader, not the NGP shader).
		P.FilterColor1TextureParameter.SetBaseIndex( 1 );
	}
#endif
	
	return Ar;
}

/** Set the dof pixel shader parameter values. */
void FPostProcessParameters::SetPS(FShader* PixelShader, FLOAT BlurKernelSize, FLOAT DOFOcclusionTweak)
{
	const FPixelShaderRHIParamRef ShaderRHI = PixelShader->GetPixelShader();

	{
		FVector4 Value(BlurKernelSize, DOFOcclusionTweak, 0, 0);

		SetPixelShaderValues(
			ShaderRHI,
			DOFKernelSizeParameter,
			&Value,
			1);
	}

	// DepthOfField
	SetTextureParameter(
		ShaderRHI,
		FilterColor0TextureParameter,
		TStaticSamplerState<SF_Bilinear>::GetRHI(),
		GSceneRenderTargets.GetFilterColorTexture(SRTI_FilterColor0)
		);

	// Bloom
	// mobile needs to know the base index (this parameter use set to texunit 1 in the shader)
	SetTextureParameterDirectly(
		ShaderRHI,
		FilterColor1TextureParameter,
		TStaticSamplerState<SF_Bilinear>::GetRHI(),
		GSceneRenderTargets.GetFilterColorTexture(SRTI_FilterColor1)
		);

	// SoftEdgeMotionBlur
	SetTextureParameter(
		ShaderRHI,
		FilterColor2TextureParameter,
		TStaticSamplerState<SF_Bilinear>::GetRHI(),
		GSceneRenderTargets.GetFilterColorTexture(SRTI_FilterColor2)
		);

	// Translucency layer
	SetTextureParameter(
		ShaderRHI,
		SeparateTranslucencyTextureParameter,
		TStaticSamplerState<SF_Point>::GetRHI(),
		GSceneRenderTargets.GetRenderTargetTexture(SeparateTranslucency)
		);
}

#if WITH_D3D11_TESSELLATION
/** Set the dof geometry shader parameter values. */
void FPostProcessParameters::SetGS(FShader* GeometryShader, FLOAT BlurKernelSize, FLOAT DOFOcclusionTweak)
{
	const FGeometryShaderRHIRef& ShaderRHI = GeometryShader->GetGeometryShader();

	{
		FVector4 Value(BlurKernelSize, DOFOcclusionTweak, 0, 0);

		SetShaderValue(
			ShaderRHI,
			DOFKernelSizeParameter,
			Value);
	}
}
#endif


const UINT NumFPFilterSamples = 4;
const UINT HalfNumFPFilterSamples = (NumFPFilterSamples + 1) / 2;



IMPLEMENT_SHADER_TYPE2(template<>,TDOFGatherPixelShader<NumFPFilterSamples>,SF_Pixel,VER_POSTPROCESSUPDATE,0);
//
#undef VARIATION

//shaders to use for the fast version, which only takes 4 point samples from a half resolution scene buffer with depth in the alpha
#define VARIATION1(A) VARIATION2(A, TRUE) VARIATION2(A, FALSE)
#define VARIATION2(A, B) typedef TBloomGatherPixelShader<A, B> FBloomGatherPixelShader##A##B; IMPLEMENT_SHADER_TYPE2(template<>, FBloomGatherPixelShader##A##B, SF_Pixel, 0, 0);
VARIATION1(NumFPFilterSamples)
#undef VARIATION1
#undef VARIATION2

IMPLEMENT_SHADER_TYPE2(template<>,TMotionBlurGatherPixelShader<NumFPFilterSamples>,SF_Pixel,VER_IMPROVED_MOTIONBLUR6,0);
IMPLEMENT_SHADER_TYPE(template<>,TDOFAndBloomGatherVertexShader<NumFPFilterSamples>,TEXT("DOFAndBloomGatherVertexShader"),TEXT("Main"),SF_Vertex,0,0);

/*-----------------------------------------------------------------------------
FDOFAndBloomBlendPixelShader
-----------------------------------------------------------------------------*/

/** Encapsulates the DOF and bloom blend pixel shader. */
UBOOL FDOFAndBloomBlendPixelShader::ShouldCache(EShaderPlatform Platform)
{
	return TRUE;
}

void FDOFAndBloomBlendPixelShader::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
}

/** Initialization constructor. */
FDOFAndBloomBlendPixelShader::FDOFAndBloomBlendPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
:	FGlobalShader(Initializer)
,	DOFParameters(Initializer.ParameterMap)
,	PostProcessParameters(Initializer.ParameterMap)
{
	SceneTextureParameters.Bind(Initializer.ParameterMap);
	DoFBlurBufferParameter.Bind(Initializer.ParameterMap,TEXT("DoFBlurBuffer"),TRUE);
	BloomTintAndScreenBlendThresholdParameter.Bind(Initializer.ParameterMap,TEXT("BloomTintAndScreenBlendThreshold"),TRUE);
}

UBOOL FDOFAndBloomBlendPixelShader::Serialize(FArchive& Ar)
{
	UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
	Ar << DOFParameters << SceneTextureParameters << DoFBlurBufferParameter << PostProcessParameters;
	Ar << BloomTintAndScreenBlendThresholdParameter;

	BloomTintAndScreenBlendThresholdParameter.SetShaderParamName(TEXT("BloomTintAndScreenBlendThreshold"));
	return bShaderHasOutdatedParameters;
}

IMPLEMENT_SHADER_TYPE(,FDOFAndBloomBlendPixelShader,TEXT("DOFAndBloomBlendPixelShader"),TEXT("Main"),SF_Pixel,VER_REMOVED_SEPARATEBLOOM2,0);

/*-----------------------------------------------------------------------------
FDOFAndBloomBlendVertexShader
-----------------------------------------------------------------------------*/

UBOOL FDOFAndBloomBlendVertexShader::ShouldCache(EShaderPlatform Platform)
{
	return TRUE;
}

void FDOFAndBloomBlendVertexShader::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
}

/** Initialization constructor. */
FDOFAndBloomBlendVertexShader::FDOFAndBloomBlendVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
:	FGlobalShader(Initializer)
{
	SceneCoordinateScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("SceneCoordinateScaleBias"),TRUE);
}

// FShader interface.
UBOOL FDOFAndBloomBlendVertexShader::Serialize(FArchive& Ar)
{
	UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
	Ar << SceneCoordinateScaleBiasParameter;
	return bShaderHasOutdatedParameters;
}

IMPLEMENT_SHADER_TYPE(,FDOFAndBloomBlendVertexShader,TEXT("DOFAndBloomBlendVertexShader"),TEXT("Main"),SF_Vertex,VER_SEPARATE_DOF_BLOOM,0);

/*-----------------------------------------------------------------------------
FDOFAndBloomPostProcessSceneProxy
-----------------------------------------------------------------------------*/

extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

/** 
* Initialization constructor. 
* @param InEffect - DOF post process effect to mirror in this proxy
*/
FDOFAndBloomPostProcessSceneProxy::FDOFAndBloomPostProcessSceneProxy(const UDOFAndBloomEffect* InEffect,const FPostProcessSettings* WorldSettings)
:	FPostProcessSceneProxy(InEffect)
,	DepthOfFieldType((EDOFType)InEffect->DepthOfFieldType)
,	DepthOfFieldQuality((EDOFQuality)InEffect->DepthOfFieldQuality)
,	ColorGrading_LookupTable(0)
,	BokehTexture(0)
{
	SET_POSTPROCESS_PROPERTY1(DOF, FalloffExponent);
	SET_POSTPROCESS_PROPERTY1(DOF, BlurKernelSize);
	SET_POSTPROCESS_PROPERTY1(DOF, BlurBloomKernelSize);
	SET_POSTPROCESS_PROPERTY1(DOF, MaxNearBlurAmount);
	SET_POSTPROCESS_PROPERTY1(DOF, MinBlurAmount);
	SET_POSTPROCESS_PROPERTY1(DOF, MaxFarBlurAmount);
	SET_POSTPROCESS_PROPERTY1(DOF, FocusType);
	SET_POSTPROCESS_PROPERTY1(DOF, FocusInnerRadius);
	SET_POSTPROCESS_PROPERTY1(DOF, FocusDistance);
	SET_POSTPROCESS_PROPERTY1(DOF, FocusPosition);
	SET_POSTPROCESS_PROPERTY1(DOF, BokehTexture);
	SET_POSTPROCESS_PROPERTY2(Bloom, Scale);
	SET_POSTPROCESS_PROPERTY2(Bloom, Threshold);
	SET_POSTPROCESS_PROPERTY2(Bloom, Tint);
	SET_POSTPROCESS_PROPERTY2(Bloom, ScreenBlendThreshold);

	if(DepthOfFieldType == DOFType_BokehDOF)
	{
		// not supported always, we fall back
		if(!IsValidRef(GSceneRenderTargets.GetSpecularGBufferTexture())
		|| GRHIShaderPlatform != SP_PCD3D_SM5)
		{
			DepthOfFieldType = DOFType_SimpleDOF;
		}
	}

	// clamp DOF radius in reasonable bound 
	BlurKernelSize = Clamp(BlurKernelSize, 0.0f, 128.0f);

	if(WorldSettings)
	{
		// Note the override to the boolean value is not giving more functionality.
		// Here it is handled for consistency. For post process volumes it has a meaning.

		if(WorldSettings->bOverride_EnableDOF && !WorldSettings->bEnableDOF)
		{
			MaxNearBlurAmount = MaxFarBlurAmount = 0.0f;
		}
		if(WorldSettings->bOverride_EnableBloom && !WorldSettings->bEnableBloom)
		{
			BloomScale = 0.0;
		}

		if(WorldSettings->bOverride_Scene_ColorGradingLUT)
		{
			ColorGrading_LookupTable = WorldSettings->ColorGrading_LookupTable;
		}
	}

	// 0.18f to get natural occlusion, 0.4 to solve layer color leaking issues
	DOFOcclusionTweak = 0.4f;

#if !FINAL_RELEASE
	{
		// console variable override
		static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("DOFOcclusionTweak")); 
		FLOAT Value = CVar->GetFloat();

		if(Value >= 0.0f)
		{
			DOFOcclusionTweak = Value;
		}
	}
#endif // !FINAL_RELEASE

	//default clamping values
	MinDepth = -MAX_FLT;
	MaxDepth = MAX_FLT;
}


/**
 * Overriden by the DepthOfField effect.
 * @param Params - The parameters for the effect are returned in this struct.
 * @return whether the data was filled in.
 */
UBOOL FDOFAndBloomPostProcessSceneProxy::ComputeDOFParams(const FViewInfo& View, struct FDepthOfFieldParams &Params) const
{
	if(View.Family->ShowFlags & SHOW_DepthOfField)
	{
		CalcDoFParams(View,Params.FocusDistance,Params.FocusInnerRadius);
		Params.FalloffExponent = FalloffExponent;
		Params.MaxNearBlurAmount = MaxNearBlurAmount;
		Params.MinBlurAmount = MinBlurAmount;
		Params.MaxFarBlurAmount = MaxFarBlurAmount;
	}
	else
	{
		Params = FDepthOfFieldParams();
	}

	return TRUE;
}

/**
 * Calculate depth of field focus distance and focus radius
 *
 * @param View current view being rendered
 * @param OutFocusDistance [out] center of focus distance in clip space
 * @param OutFocusRadius [out] radius about center of focus in clip space
 */
void FDOFAndBloomPostProcessSceneProxy::CalcDoFParams(const FViewInfo& View, FLOAT& OutFocusDistance, FLOAT& OutFocusRadius) const
{
	// get the view direction vector (normalized)
	FVector ViewDir( 
		View.ViewMatrix.M[0][2], 
		View.ViewMatrix.M[1][2], 
		View.ViewMatrix.M[2][2]);
	ViewDir.Normalize();
	// calc the focus point
	FVector FocusPoint(0.f,0.f,0.f);
	switch( FocusType )
	{
	case FOCUS_Position:
		{
			// world space focus point specified projected onto the view direction
			FocusPoint = (((FocusPosition - View.ViewOrigin) | ViewDir) * ViewDir) + View.ViewOrigin;
		}		
		break;
	case FOCUS_Distance:
	default:
		{
			// focus point based on view distance
			FocusPoint = (FocusDistance * ViewDir) + View.ViewOrigin;
		}		
		break;
	};

	// transform to projected/clip space in order to get w depth values
	OutFocusDistance = Max<FLOAT>(0.f, View.WorldToScreen(FocusPoint).W);
	// Add radius to get the far focus point
	FVector FocusPointFar = (ViewDir * FocusInnerRadius) + FocusPoint;	
	FLOAT FocusDistanceFar = Max<FLOAT>(OutFocusDistance, View.WorldToScreen(FocusPointFar).W);
	OutFocusRadius = Max<FLOAT>((FLOAT)KINDA_SMALL_NUMBER, Abs<FLOAT>(FocusDistanceFar - OutFocusDistance));
}

template <class TPShader, class TVShader>
void FDOFAndBloomPostProcessSceneProxy::SetupGather2x2(FViewInfo& View, TVShader& GatherVertexShader, const FTexture2DRHIRef& SourceTexture, FLOAT CustomColorScale, UBOOL bBilinearFiltered)
{
	static FGlobalBoundShaderState ShaderState;

	// Set the gather pixel shader.
	TShaderMapRef<TPShader> GatherPixelShader(GetGlobalShaderMap());
	RHIReduceTextureCachePenalty(GatherPixelShader->GetPixelShader());
	GatherPixelShader->DOFParameters.SetPS(*GatherPixelShader, View.DepthOfFieldParams);
	GatherPixelShader->SceneTextureParameters.Set(&View, *GatherPixelShader);
	
	{
		FVector4 Value(BloomScale, BloomThreshold, MinDepth, MaxDepth);
		SetPixelShaderValue(GatherPixelShader->GetPixelShader(), GatherPixelShader->BloomScaleAndThresholdParameter, Value);
	}

	{
		FVector4 Value(CustomColorScale, 0, 0, 0);
		SetPixelShaderValue(GatherPixelShader->GetPixelShader(), GatherPixelShader->GatherParamsParameter, Value);
	}

	SetGlobalBoundShaderState(ShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *GatherVertexShader, *GatherPixelShader, sizeof(FFilterVertex), 0, EGST_DOFAndBloomGather);

	SetTextureParameterDirectly( GatherPixelShader->GetPixelShader(), GatherPixelShader->VelocityTextureParameter, TStaticSamplerState<SF_Point>::GetRHI(), GSceneRenderTargets.GetVelocityTexture() );

	// Use half-size scene texture instead of normal scene color.
	FSamplerStateRHIParamRef TextureFilter = bBilinearFiltered 
		? TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI() 
		: TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

	SetTextureParameterDirectly(
		GatherPixelShader->GetPixelShader(),
		GatherPixelShader->SeparateTranslucencyParameter,
		TextureFilter,
		GSceneRenderTargets.GetRenderTargetTexture(SeparateTranslucency)
		);

	SetTextureParameterDirectly(
		GatherPixelShader->GetPixelShader(),
		GatherPixelShader->SmallSceneColorTextureParameter,
		TextureFilter,
		SourceTexture
		);
}

/**
* Renders the gather pass which generates input for the subsequent blurring steps
* to a low resolution filter buffer.
* @param DestinationData e.g. EGD_DepthOfField, EGD_Bloom, EGD_MotionBlur
* @param FilterColorIndex which buffer to use FilterColor 0/1/2
*/
void FDOFAndBloomPostProcessSceneProxy::RenderGatherPass(FViewInfo& View, EGatherData DestinationData, FSceneRenderTargetIndex FilterColorIndex, FLOAT CustomColorScale, INT Quality, UBOOL bSeparateTranslucency)
{
	UINT HalfSizeX = GSceneRenderTargets.GetBufferSizeX() / 2;
	UINT HalfSizeY = GSceneRenderTargets.GetBufferSizeY() / 2;

	FLOAT InvSrcSizeX = 1.0f / (FLOAT)HalfSizeX;
	FLOAT InvSrcSizeY = 1.0f / (FLOAT)HalfSizeY;

	FLOAT OffsetX = - 0.5f * InvSrcSizeX;
	FLOAT OffsetY = - 0.5f * InvSrcSizeY;
	UINT SampleCount = 4;

	const UINT FilterDownsampleFactor = 2;

	// destination render target
	const UINT FilterBufferSizeX = GSceneRenderTargets.GetFilterBufferSizeX();
	const UINT FilterBufferSizeY = GSceneRenderTargets.GetFilterBufferSizeY();

	// destination size (might be slightly smaller than render target)
	const UINT DownsampledSizeX = HalfSizeX / FilterDownsampleFactor;
	const UINT DownsampledSizeY = HalfSizeY / FilterDownsampleFactor;

	// No depth tests, no backface culling.
	RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
	RHISetBlendState(TStaticBlendState<>::GetRHI());

	// Render to the downsampled filter buffer.
	GSceneRenderTargets.BeginRenderingFilter(FilterColorIndex);

	// half the sample count as we pack two samples in one constant
	const UINT NumShaderConstants = (SampleCount + 1) / 2;

	FVector4 PackedDownsampleOffsets[HalfNumFPFilterSamples];
	FVector2D DownsampleOffsets[NumFPFilterSamples];

	UBOOL bBilinearFiltered = FALSE;

	// Better quality if hardware supports it (all modern graphic cards).
	// This is only noticeable with a small blur kernel size.
	// By using more samples we could emulated that on any hardware but here
	// we prefer performance over quality.
	if(Quality == 1	&& GSupportsFPFiltering)
	{
		// 0.118ms -> 0.24ms for 1280x720
		bBilinearFiltered = TRUE;
	}

	if(bBilinearFiltered)
	{
		// kernel: 1221 (better quality, slight blurring)
		//         2442
		//         2442
		//         1221
		DownsampleOffsets[0] = FVector2D(-0.66f * InvSrcSizeX + OffsetX,-0.66f * InvSrcSizeY + OffsetY );
		DownsampleOffsets[1] = FVector2D( 1.66f * InvSrcSizeX + OffsetX,-0.66f * InvSrcSizeY + OffsetY );
		DownsampleOffsets[2] = FVector2D(-0.66f * InvSrcSizeX + OffsetX, 1.66f * InvSrcSizeY + OffsetY );
		DownsampleOffsets[3] = FVector2D( 1.66f * InvSrcSizeX + OffsetX, 1.66f * InvSrcSizeY + OffsetY );
	}
	else
	{
		// kernel: 11
		//         11
		for(UINT SampleY = 0;SampleY < HalfNumFPFilterSamples;SampleY++)
		{
			for(UINT SampleX = 0;SampleX < HalfNumFPFilterSamples;SampleX++)
			{
				const INT SampleIndex = SampleY * HalfNumFPFilterSamples + SampleX;

				DownsampleOffsets[SampleIndex] = FVector2D(
					SampleX * InvSrcSizeX + OffsetX,
					SampleY * InvSrcSizeY + OffsetY
					);
			}
		}
	}

	// Pack the downsamples.
	for(INT ChunkIndex = 0; ChunkIndex < HalfNumFPFilterSamples; ChunkIndex++)
	{
		PackedDownsampleOffsets[ChunkIndex].X = DownsampleOffsets[ChunkIndex * 2 + 0].X;
		PackedDownsampleOffsets[ChunkIndex].Y = DownsampleOffsets[ChunkIndex * 2 + 0].Y;
		PackedDownsampleOffsets[ChunkIndex].Z = DownsampleOffsets[ChunkIndex * 2 + 1].Y;
		PackedDownsampleOffsets[ChunkIndex].W = DownsampleOffsets[ChunkIndex * 2 + 1].X;
	}

	// Set the gather vertex shader.
	TShaderMapRef<TDOFAndBloomGatherVertexShader<NumFPFilterSamples> > GatherVertexShader(GetGlobalShaderMap());
	SetVertexShaderValues(GatherVertexShader->GetVertexShader(),GatherVertexShader->SampleOffsetsParameter,PackedDownsampleOffsets,HalfNumFPFilterSamples);

	switch(DestinationData)
	{
		case EGD_DepthOfField:
			SetupGather2x2<TDOFGatherPixelShader<NumFPFilterSamples> >(View, GatherVertexShader, GSceneRenderTargets.GetTranslucencyBufferTexture(), CustomColorScale, bBilinearFiltered);
			break;
		case EGD_Bloom:
			// We read from HalfResPostProcess to get Bloom from motion blurred content (more correct).
			if (bSeparateTranslucency)
			{
				SetupGather2x2<TBloomGatherPixelShader<NumFPFilterSamples, TRUE> >(View, GatherVertexShader, GSceneRenderTargets.GetHalfResPostProcessTexture(), CustomColorScale, bBilinearFiltered);
			}
			else
			{
				SetupGather2x2<TBloomGatherPixelShader<NumFPFilterSamples, FALSE> >(View, GatherVertexShader, GSceneRenderTargets.GetHalfResPostProcessTexture(), CustomColorScale, bBilinearFiltered);
			}
			break;
		case EGD_MotionBlur:
			SetupGather2x2<TMotionBlurGatherPixelShader<NumFPFilterSamples> >(View, GatherVertexShader, GSceneRenderTargets.GetTranslucencyBufferTexture(), CustomColorScale, bBilinearFiltered);
			break;
		default:
			checkSlow(0);
	}

	//@TODO - We shouldn't have to clear since we fill the entire buffer anyway. :(
#if !PS3
	// Clear the buffer to black.
	RHIClear(TRUE,FLinearColor(0,0,0,0),FALSE,0,FALSE,0);
#endif


	// Draw a quad mapping the view's scene colors/depths to the downsampled filter buffer.
	{
		// /2 as we render to half resolution
		INT ViewOffsetX = View.RenderTargetX / 2;
		INT ViewOffsetY = View.RenderTargetY / 2;

		DrawDenormalizedQuad(
			1, 1,											// DestRect Pos
			DownsampledSizeX, DownsampledSizeY,				// DestRect Size
			ViewOffsetX, ViewOffsetY,						// SrcRect Pos
			HalfSizeX, HalfSizeY,							// SrcRect Size
			FilterBufferSizeX, FilterBufferSizeY,			// Dest Size
			HalfSizeX, HalfSizeY							// Src Size
			);
	}

	// Resolve the filter buffer.
	GSceneRenderTargets.FinishRenderingFilter(FilterColorIndex);
}

/** Down sample scene to half resolution (split screen: one view at a time), depth in alpha */
void FDOFAndBloomPostProcessSceneProxy::DownSampleSceneAndDepth(FViewInfo& View)
{
	SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("DownSampleSceneAndDepth"));

	const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
	const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();

	// if this pointer is set, the downsampled z is put into the alpha channel
	const FSceneView* DownsampleZView = &View;
	
	if(GUsingMobileRHI)
	{
		// on mobile the rendertarget is ARGB8, the depth is not available
		// to z doesn't need to be downsampled
		DownsampleZView = 0;
	}
	
	DrawDownsampledTexture(
		GSceneRenderTargets.GetTranslucencyBufferSurface(),
		GSceneRenderTargets.GetTranslucencyBufferTexture(),
		GSceneRenderTargets.GetEffectiveSceneColorTexture(), 
		FIntPoint(View.RenderTargetX / 2, View.RenderTargetY / 2),
		FIntRect((INT)(View.RenderTargetX), (INT)(View.RenderTargetY), (INT)(View.RenderTargetX + View.RenderTargetSizeX), (INT)(View.RenderTargetY + View.RenderTargetSizeY)),
		FIntPoint(BufferSizeX / 2, BufferSizeY / 2),
		FIntPoint(BufferSizeX, BufferSizeY),
		DownsampleZView
		);
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
UBOOL FDOFAndBloomPostProcessSceneProxy::Render(const FScene* Scene, UINT InDepthPriorityGroup,FViewInfo& View,const FMatrix& CanvasTransform,FSceneColorLDRInfo& LDRInfo)
{
	SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("DOFAndBloom"));

	// this node should not be used for final postprocessing
	// it's kept as it has use for the scene reflections but that will change. 

	if((View.Family->ShowFlags & SHOW_DepthOfField) == 0)
	{
		// switch Depth of Field off. Note this also disables bloom!
		return FALSE;
	}

	const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
	const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();
	const UINT FilterBufferSizeX = GSceneRenderTargets.GetFilterBufferSizeX();
	const UINT FilterBufferSizeY = GSceneRenderTargets.GetFilterBufferSizeY();
	const UINT FilterDownsampleFactor = GSceneRenderTargets.GetFilterDownsampleFactor();
	const UINT DownsampledSizeX = View.RenderTargetSizeX / FilterDownsampleFactor;
	const UINT DownsampledSizeY = View.RenderTargetSizeY / FilterDownsampleFactor;

	DownSampleSceneAndDepth(View);

	FVector2D SampleMaskMin(0 / (FLOAT)BufferSizeX, 0 / (FLOAT)BufferSizeY);
	FVector2D SampleMaskMax((0 + View.SizeX - 1) / (FLOAT)BufferSizeX, (0 + View.SizeY - 1) / (FLOAT)BufferSizeY);

	// to prevent bad colors content to leak in (in split screen or when not using the full render target)
	const UINT AntiLeakBorder = 2;

 	// Depth of Field
 	RenderGatherPass(View, EGD_DepthOfField, SRTI_FilterColor0, 1.0f);
 	GaussianBlurFilterBuffer(View, View.SizeX, DownsampledSizeX + AntiLeakBorder, DownsampledSizeY + AntiLeakBorder, BlurKernelSize, 1.0f, SRTI_FilterColor0, SampleMaskMin, SampleMaskMax);

	// Bloom

	// the color was range compressed to fit in the range so we need to decompress it
	const FLOAT MAX_SCENE_COLOR = 4.0f;

	RenderGatherPass(View, EGD_Bloom, SRTI_FilterColor1, MAX_SCENE_COLOR);
	GaussianBlurFilterBuffer(View, View.SizeX, DownsampledSizeX + AntiLeakBorder, DownsampledSizeY + AntiLeakBorder, BlurBloomKernelSize, 1.0f, SRTI_FilterColor1, SampleMaskMin, SampleMaskMax);

	// Render to the scene color buffer.
	GSceneRenderTargets.BeginRenderingSceneColor();

	// Set the blend vertex shader.
	TShaderMapRef<FDOFAndBloomBlendVertexShader> BlendVertexShader(GetGlobalShaderMap());
	SetVertexShaderValue(
		BlendVertexShader->GetVertexShader(),
		BlendVertexShader->SceneCoordinateScaleBiasParameter,
		FVector4(
		+0.5f,
		-0.5f,
		+0.5f + GPixelCenterOffset / BufferSizeY,
		+0.5f + GPixelCenterOffset / BufferSizeX
		)
		);

	// Set the blend pixel shader.
	TShaderMapRef<FDOFAndBloomBlendPixelShader> BlendPixelShader(GetGlobalShaderMap());

	BlendPixelShader->DOFParameters.SetPS(*BlendPixelShader, View.DepthOfFieldParams);
	BlendPixelShader->PostProcessParameters.SetPS(*BlendPixelShader, BlurKernelSize, DOFOcclusionTweak);
	BlendPixelShader->SceneTextureParameters.Set(&View,*BlendPixelShader);

	SetGlobalBoundShaderState( BlendBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *BlendVertexShader, *BlendPixelShader, sizeof(FFilterVertex));

	// Draw a quad mapping the blurred pixels in the filter buffer to the scene color buffer.
	DrawDenormalizedQuad(
		View.RenderTargetX,View.RenderTargetY,
		View.RenderTargetSizeX,View.RenderTargetSizeY,
		1,1,
		DownsampledSizeX,DownsampledSizeY,
		BufferSizeX,BufferSizeY,
		FilterBufferSizeX,FilterBufferSizeY
		);

	// Resolve the scene color buffer.
	FResolveRect ResolveRect;
	ResolveRect.X1 = View.RenderTargetX;
	ResolveRect.Y1 = View.RenderTargetY;
	ResolveRect.X2 = View.RenderTargetX + View.RenderTargetSizeX;
	ResolveRect.Y2 = View.RenderTargetY + View.RenderTargetSizeY;
	GSceneRenderTargets.FinishRenderingSceneColor(TRUE, ResolveRect);

	return TRUE;
}

FGlobalBoundShaderState FDOFAndBloomPostProcessSceneProxy::BlendBoundShaderState;

/**
 * Creates a proxy to represent the render info for a post process effect
 * @param WorldSettings - The world's post process settings for the view.
 * @return The proxy object.
 */
FPostProcessSceneProxy* UDOFAndBloomEffect::CreateSceneProxy(const FPostProcessSettings* WorldSettings)
{
	if(!WorldSettings || WorldSettings->bEnableDOF || WorldSettings->bEnableBloom)
	{
		return new FDOFAndBloomPostProcessSceneProxy(this,WorldSettings);
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
UBOOL UDOFAndBloomEffect::IsShown(const FSceneView* View) const
{
	return (GSystemSettings.bAllowBloom || GSystemSettings.bAllowDepthOfField) && Super::IsShown( View );
}

/**
* Called after this instance has been serialized.
*/
void UDOFAndBloomEffect::PostLoad()
{
	Super::PostLoad();

	ULinkerLoad* LFLinkerLoad = GetLinker();
	if(LFLinkerLoad && (LFLinkerLoad->Ver() < VER_DEPTHOFFIELD_TYPE))
	{
		DepthOfFieldType = DOFType_SimpleDOF;

		if(bEnableReferenceDOF_DEPRECATED)
		{
			DepthOfFieldType = DOFType_ReferenceDOF;
			DepthOfFieldQuality = DOFQuality_High;
		}
	}
}

/*-----------------------------------------------------------------------------
	FBlurPostProcessSceneProxy
-----------------------------------------------------------------------------*/

class FBlurPostProcessSceneProxy : public FPostProcessSceneProxy
{
public:

	FBlurPostProcessSceneProxy(const UBlurEffect* InEffect) :
		FPostProcessSceneProxy(InEffect),
		BlurKernelSize(InEffect->BlurKernelSize)
	{}

	/**
	* Render the post process effect
	* Called by the rendering thread during scene rendering
	* @param InDepthPriorityGroup - scene DPG currently being rendered
	* @param View - current view
	* @param CanvasTransform - same canvas transform used to render the scene
	* @param LDRInfo - helper information about SceneColorLDR
	* @return TRUE if anything was rendered
	*/
	UBOOL Render(const FScene* Scene, UINT InDepthPriorityGroup,FViewInfo& View,const FMatrix& CanvasTransform,FSceneColorLDRInfo& LDRInfo);

protected:

	INT BlurKernelSize;
};

UBOOL FBlurPostProcessSceneProxy::Render(const FScene* Scene, UINT InDepthPriorityGroup,FViewInfo& View,const FMatrix& CanvasTransform,FSceneColorLDRInfo& LDRInfo)
{
	SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("Blur"));

	// Handle writing to both HDR and LDR scene color
	if (View.bUseLDRSceneColor)
	{
		GSceneRenderTargets.BeginRenderingSceneColorLDR();
	}
	else
	{
		GSceneRenderTargets.BeginRenderingSceneColor();
		// Turn off alpha-writes, since we sometimes store the Z-values there.
		RHISetColorWriteMask(CW_RGB);
	}

	// Blur in the X direction first
	SetupSceneColorGaussianBlurStep(FVector2D(1.0f / View.RenderTargetSizeX, 0), BlurKernelSize, View.bUseLDRSceneColor);

	DrawDenormalizedQuad(
		View.RenderTargetX,View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
		View.RenderTargetX,View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
		GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY(),
		GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());

	if (View.bUseLDRSceneColor)
	{
		GSceneRenderTargets.FinishRenderingSceneColorLDR();
	}
	else
	{
		// Turn on alpha-writes again.
		RHISetColorWriteMask(CW_RGBA);
		GSceneRenderTargets.FinishRenderingSceneColor();
	}

	if (View.bUseLDRSceneColor) // Using 32-bit (LDR) surface
	{
		// If this is the final effect in chain, render to the view's output render target
		// unless an upscale is needed, in which case render to LDR scene color.
		if (FinalEffectInGroup && !GSystemSettings.NeedsUpscale()) 
		{
			// Render to the final render target
			GSceneRenderTargets.BeginRenderingBackBuffer();

			// Blur in the Y direction
			SetupSceneColorGaussianBlurStep(FVector2D(0, 1.0f / View.RenderTargetSizeY), BlurKernelSize, View.bUseLDRSceneColor);
		
			const UINT TargetSizeX = View.Family->RenderTarget->GetSizeX();
			const UINT TargetSizeY = View.Family->RenderTarget->GetSizeY();

			DrawDenormalizedQuad(
				View.X,View.Y, View.SizeX,View.SizeY,
				View.RenderTargetX,View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
				TargetSizeX, TargetSizeY,
				GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());
		}
		else
		{
			GSceneRenderTargets.BeginRenderingSceneColorLDR();

			// Blur in the Y direction
			SetupSceneColorGaussianBlurStep(FVector2D(0, 1.0f / View.RenderTargetSizeY), BlurKernelSize, View.bUseLDRSceneColor);

			DrawDenormalizedQuad(
				View.RenderTargetX,View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
				View.RenderTargetX,View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
				GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY(),
				GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());

			GSceneRenderTargets.FinishRenderingSceneColorLDR();
		}
	}
	else // Using 64-bit (HDR) surface
	{
		GSceneRenderTargets.BeginRenderingSceneColor( RTUsage_FullOverwrite ); // Start rendering to the scene color buffer.
		// Turn off alpha-writes, since we sometimes store the Z-values there.
		RHISetColorWriteMask(CW_RGB);	

		// Blur in the Y direction
		SetupSceneColorGaussianBlurStep(FVector2D(0, 1.0f / View.RenderTargetSizeY), BlurKernelSize, View.bUseLDRSceneColor);
	
		DrawDenormalizedQuad(
			View.RenderTargetX,View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
			View.RenderTargetX,View.RenderTargetY, View.RenderTargetSizeX,View.RenderTargetSizeY,
			GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY(),
			GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());

		// Turn on alpha-writes again.
		RHISetColorWriteMask(CW_RGBA);
		GSceneRenderTargets.FinishRenderingSceneColor();
	}	

	return TRUE;
}

/**
 * Creates a proxy to represent the render info for a post process effect
 * @param WorldSettings - The world's post process settings for the view.
 * @return The proxy object.
 */
FPostProcessSceneProxy* UBlurEffect::CreateSceneProxy(const FPostProcessSettings* WorldSettings)
{
	return new FBlurPostProcessSceneProxy(this);
}

/**
 * @param View - current view
 * @return TRUE if the effect should be rendered
 */
UBOOL UBlurEffect::IsShown(const FSceneView* View) const
{
	return Super::IsShown( View );
}

void UBlurEffect::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}