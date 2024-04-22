/*=============================================================================
	UnMotionBlurEffect.h: Motion blur post process effect definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
FMotionBlurShaderParameters
-----------------------------------------------------------------------------*/

/** Encapsulates the motion blur shader parameters. */
class FMotionBlurShaderParameters
{
public:

	enum {NUM_SAMPLES=5};

	/** Default constructor. */
	FMotionBlurShaderParameters() {}

	/** Initialization constructor. */
	FMotionBlurShaderParameters(const FShaderParameterMap& ParameterMap)
	{
		VelocityBuffer.Bind(ParameterMap, TEXT("VelocityBuffer"), TRUE );
		LowResSceneBuffer.Bind(ParameterMap, TEXT("LowResSceneBuffer"), TRUE );
		ScreenToWorldParameter.Bind(ParameterMap, TEXT("ScreenToWorld"), TRUE );
		PrevViewProjParameter.Bind(ParameterMap, TEXT("PrevViewProjMatrix"), TRUE );
		StaticVelocityParameters.Bind(ParameterMap, TEXT("StaticVelocityParameters"), TRUE );
		DynamicVelocityParameters.Bind(ParameterMap, TEXT("DynamicVelocityParameters"), TRUE );
		RenderTargetClampParameter.Bind(ParameterMap, TEXT("RenderTargetClampParameter"), TRUE );
		MotionBlurMaskScaleParameter.Bind(ParameterMap, TEXT("MotionBlurMaskScaleAndBias"), TRUE );
		StepOffsetsOpaqueParameter.Bind(ParameterMap, TEXT("StepOffsetsOpaque"), TRUE );
		StepWeightsOpaqueParameter.Bind(ParameterMap, TEXT("StepWeightsOpaque"), TRUE );
		StepOffsetsTranslucentParameter.Bind(ParameterMap, TEXT("StepOffsetsTranslucent"), TRUE );
		StepWeightsTranslucentParameter.Bind(ParameterMap, TEXT("StepWeightsTranslucent"), TRUE );
	}

	/** Set the material shader parameter values. */
	template<typename ShaderRHIParamRef>
	void Set(ShaderRHIParamRef Shader, const FViewInfo& View, const FMotionBlurParams& MotionBlurParams)
	{
		// Calculate the maximum velocities (MAX_PIXELVELOCITY is per 33 ms).
		const FLOAT SizeX = View.RenderTargetSizeX;
		const FLOAT SizeY = View.RenderTargetSizeY;
		const FLOAT AspectRatio = SizeX / SizeY;
		FLOAT MaxVelocityX = MAX_PIXELVELOCITY * MotionBlurParams.MaxVelocity;
		FLOAT MaxVelocityY = MAX_PIXELVELOCITY * MotionBlurParams.MaxVelocity * AspectRatio;

		FLOAT BlurAmount = MotionBlurParams.MotionBlurAmount;

		if(!MotionBlurParams.bFullMotionBlur)
		{
			// motion blur from camera movement/rotation
			BlurAmount = 0;
		}

		const FSceneViewState* ViewState = (FSceneViewState*)View.State;
		BlurAmount *= ViewState ? ViewState->MotionBlurTimeScale : 1.0f;

		// Converts projection space velocity [-1,1] to screen space [0,1].
		FVector4 StaticVelocity(
			0.5f * BlurAmount / MaxVelocityX,
			-0.5f * BlurAmount / MaxVelocityY,
			0, 
			0);
		SetShaderValue(Shader, StaticVelocityParameters, StaticVelocity );

		// translucency velocity scaled separately from opaque and not affected by MaxVelocity
		FLOAT TranslucentVelocityX = 2.0f * MAX_TRANSLUCENT_PIXELVELOCITY;
		FLOAT TranslucentVelocityY = 2.0f * MAX_TRANSLUCENT_PIXELVELOCITY / AspectRatio;

		// Scale values from the biased velocity buffer [-1,+1] to texel space [-MaxVelocity,+MaxVelocity].
		FVector4 DynamicVelocity( MaxVelocityX, MaxVelocityY, TranslucentVelocityX, TranslucentVelocityY );
		SetShaderValue(Shader, DynamicVelocityParameters, DynamicVelocity );

		// Calculate and set the ScreenToWorld matrix.
		FMatrix ScreenToWorld = FMatrix(
				FPlane(1,0,0,0),
				FPlane(0,1,0,0),
				FPlane(0,0,(1.0f - Z_PRECISION),1),
				FPlane(0,0,-View.NearClippingDistance * (1.0f - Z_PRECISION),0) ) * View.InvViewProjectionMatrix;

		ScreenToWorld.M[0][3] = 0.f; // Note that we reset the column here because in the shader we only used
		ScreenToWorld.M[1][3] = 0.f; // the x, y, and z components of the matrix multiplication result and we
		ScreenToWorld.M[2][3] = 0.f; // set the w component to 1 before multiplying by the PrevViewProjMatrix.
		ScreenToWorld.M[3][3] = 1.f;

		FMatrix CombinedMatrix = ScreenToWorld * View.PrevViewProjMatrix;
		SetShaderValue(Shader, PrevViewProjParameter, CombinedMatrix );
		
		// clamp in usable area to support viewports without leaking colors into the other viewports
		{
			// in texels, in half resolution
			UINT MinX = View.RenderTargetX / 2;
			UINT MinY = View.RenderTargetY / 2;
			UINT MaxX = MinX + View.RenderTargetSizeX / 2;
			UINT MaxY = MinY + View.RenderTargetSizeY / 2;

			// half resolution buffer size
			const UINT HalfSizeX = GSceneRenderTargets.GetBufferSizeX() / 2;
			const UINT HalfSizeY = GSceneRenderTargets.GetBufferSizeY() / 2;

			// convert to texture coordinates, reduced by half a texel border to avoid leaking in content from outside
			const FVector4 RenderTargetClampValues(
				(MinX + 0.5f) / (FLOAT)HalfSizeX,
				(MinY + 0.5f) / (FLOAT)HalfSizeY,
				(MaxX - 0.5f) / (FLOAT)HalfSizeX,
				(MaxY - 0.5f) / (FLOAT)HalfSizeY);
	
			SetShaderValue(Shader, RenderTargetClampParameter, RenderTargetClampValues );
		}

		SetTextureParameter(Shader, VelocityBuffer, TStaticSamplerState<SF_Point>::GetRHI(), GSceneRenderTargets.GetVelocityTexture());

		// half res scene
		{
			// SF_Bilinear improves the quality (softer) but it can introduce samples leaking into other content
			const UBOOL bFilteredLookup = TRUE;

			SetTextureParameter(
				Shader,
				LowResSceneBuffer,
				bFilteredLookup ? TStaticSamplerState<SF_Bilinear>::GetRHI() : TStaticSamplerState<SF_Point>::GetRHI(),
				GSceneRenderTargets.GetTranslucencyBufferTexture());
		}


		// offsets/weights when sampling using opaque motion based velocity
		const FLOAT StepOffsetsOpaque[NUM_SAMPLES] = {0.0f / NUM_SAMPLES, 1.0f / NUM_SAMPLES, -1.0f / NUM_SAMPLES, 2.0f / NUM_SAMPLES, -2.0f / NUM_SAMPLES};
		const FLOAT StepWeightsOpaque[NUM_SAMPLES] = {2.0f/10.0f, 2.0f/10.0f, 2.0f/10.0f, 2.0f/10.0f, 2.0f/10.0f};
		// offsets/weights when sampling using translucent non-motion based velocity
		const FLOAT StepOffsetsTranslucent[NUM_SAMPLES] = {0.0f / NUM_SAMPLES, 1.0f / NUM_SAMPLES, 2.0f / NUM_SAMPLES, 3.0f / NUM_SAMPLES, 4.0f /NUM_SAMPLES};
		const FLOAT StepWeightsTranslucent[NUM_SAMPLES] = {1.0f/5.0f, 1.0f/5.0f, 1.0f/5.0f, 1.0f/5.0f, 1.0f/5.0f};
		for (INT Idx=0; Idx < NUM_SAMPLES; Idx++)
		{
			SetShaderValue(Shader, StepOffsetsOpaqueParameter, StepOffsetsOpaque[Idx], Idx );
			SetShaderValue(Shader, StepWeightsOpaqueParameter, StepWeightsOpaque[Idx], Idx );
			SetShaderValue(Shader, StepOffsetsTranslucentParameter, StepOffsetsTranslucent[Idx], Idx );
			SetShaderValue(Shader, StepWeightsTranslucentParameter, StepWeightsTranslucent[Idx], Idx );
		}

		// tweaked constant for the masking to blend between high resolution image and motion blurred half resolution,
		const FLOAT MaskBlendFactor = 80.0f;

		// xy=multipler for the masking to blend between high resolution image and motion blurred half resolution (includes aspect ratio), zw=unused
		FVector4 ScaleAndBias( MaskBlendFactor, MaskBlendFactor / AspectRatio, 0.0f, 0.0f );
		SetShaderValue(Shader, MotionBlurMaskScaleParameter, ScaleAndBias);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FMotionBlurShaderParameters& P)
	{
		Ar << P.LowResSceneBuffer;
		Ar << P.VelocityBuffer;
		Ar << P.ScreenToWorldParameter;
		Ar << P.PrevViewProjParameter;
		Ar << P.StaticVelocityParameters;
		Ar << P.DynamicVelocityParameters;
		Ar << P.RenderTargetClampParameter;
		Ar << P.MotionBlurMaskScaleParameter;
		Ar << P.StepOffsetsOpaqueParameter;
		Ar << P.StepWeightsOpaqueParameter;
		Ar << P.StepOffsetsTranslucentParameter;
		Ar << P.StepWeightsTranslucentParameter;
		return Ar;
	}

private:
	FShaderResourceParameter LowResSceneBuffer;
	FShaderResourceParameter VelocityBuffer;
	FShaderParameter ScreenToWorldParameter;
	FShaderParameter PrevViewProjParameter;
	FShaderParameter StaticVelocityParameters;
	FShaderParameter DynamicVelocityParameters;
	FShaderParameter RenderTargetClampParameter;
	FShaderParameter MotionBlurMaskScaleParameter;
	FShaderParameter StepOffsetsOpaqueParameter;
	FShaderParameter StepWeightsOpaqueParameter;
	FShaderParameter StepOffsetsTranslucentParameter;
	FShaderParameter StepWeightsTranslucentParameter;
};