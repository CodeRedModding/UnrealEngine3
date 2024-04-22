/*=============================================================================
BranchingPCFShadowRendering.h: PCF rendering definitions.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/** The number of samples used by the Branching PCF implementation to determine where the penumbra lies. 
These must be multiples of 2 for performance reasons.  Larger values result in more accurate penumbra detection
but reduce the benefit of using dynamic branching. */

enum EBranchingPCFEdgeSampleCount
{
	BPCFEdge_Low = 4,
	BPCFEdge_Medium = 8,
	BPCFEdge_High = 8,
};

/** The number of samples used by the Branching PCF implementation to refine the coverage within the penumbra. 
These must be multiples of 4 for performance reasons.  Larger values give smoother transitions in penumbras. */

enum EBranchingPCFRefiningSampleCount
{
	BPCFRefining_Low = 12,
	BPCFRefining_Medium = 24,
	BPCFRefining_High = 32,
};

/** Same meaning as EBranchingPCFEdgeSampleCount, however these are used when hardware PCF is supported. */
enum EBranchingHardwarePCFEdgeSampleCount
{
	BHardwarePCFEdge_Low = 4,
	BHardwarePCFEdge_Medium = 4,
	BHardwarePCFEdge_High = 8,
};

/** Same meaning as EBranchingPCFRefiningSampleCount, however these are used when hardware PCF is supported. */
enum EBranchingHardwarePCFRefiningSampleCount
{
	BHardwarePCFRefining_Low = 4,
	BHardwarePCFRefining_Medium = 8,
	BHardwarePCFRefining_High = 12,
};

/** Same meaning as EBranchingPCFEdgeSampleCount, however these are used when Fetch4 is supported. */
enum EBranchingFetch4PCFEdgeSampleCount
{
	BFetch4PCFEdge_Low = 4,
	BFetch4PCFEdge_Medium = 4,
	BFetch4PCFEdge_High = 8,
};

/** Same meaning as EBranchingPCFRefiningSampleCount, however these are used when Fetch4 is supported. */
enum EBranchingFetch4PCFRefiningSampleCount
{
	BFetch4PCFRefining_Low = 4,
	BFetch4PCFRefining_Medium = 8,
	BFetch4PCFRefining_High = 12,
};

/* Manually cached samples generated with FDiskSampleGenerator */

static const FVector2D FourEdgeSamples[4] = 
{
    FVector2D(-0.096732f, 0.995310f), FVector2D(0.260201f, -0.965555f), FVector2D(0.901758f, 0.432242f), FVector2D(-0.976801f, -0.214147f)    
};

static const FVector2D EightEdgeSamples[8] = 
{
    FVector2D(-0.046244f, -0.998930f), FVector2D(0.316557f, 0.948573f), FVector2D(-0.958236f, 0.285978f), FVector2D(0.912094f, -0.409981f),
    FVector2D(-0.961936f, -0.273275f), FVector2D(0.982681f, 0.185307f), FVector2D(-0.221212f, 0.975226f), FVector2D(0.794196f, -0.607661f)
};

static const FVector2D FourRefiningSamples[4] = 
{
	FVector2D(0.546429f, 0.291679f), FVector2D(-0.327817f, 0.058277f), FVector2D(-0.158216f, -0.463566f), FVector2D(0.487845f, -0.512424f)
};

static const FVector2D EightRefiningSamples[8] = 
{
    FVector2D(-0.071794f, -0.767445f), FVector2D(0.419721f, -0.704903f), FVector2D(-0.000848f, -0.000485f), FVector2D(-0.592816f, -0.303198f),
    FVector2D(0.841066f, -0.308718f), FVector2D(0.228044f, 0.848170f), FVector2D(0.612645f, 0.200965f), FVector2D(-0.588486f, 0.690574f)
};

static const FVector2D TwelveRefiningSamples[12] = 
{
    FVector2D(0.838311f, -0.535163f),FVector2D(0.250466f, -0.220627f),FVector2D(0.387744f, -0.548799f),FVector2D(-0.512371f, 0.570049f),
    FVector2D(-0.907283f, 0.113584f),FVector2D(-0.738344f, -0.505436f),FVector2D(0.085194f, 0.305618f),FVector2D(-0.293107f, -0.016795f),
    FVector2D(0.027704f, 0.708739f),FVector2D(0.053306f, -0.594006f),FVector2D(0.345382f, -0.909465f),FVector2D(-0.586266f, -0.229196f)
};

static const FVector2D TwentyFourRefiningSamples[24] = 
{
    FVector2D(-0.840328f, 0.541797f),FVector2D(0.662232f, -0.656196f),FVector2D(0.559291f, 0.435458f),FVector2D(-0.025865f, -0.064991f),
    FVector2D(0.835040f, 0.210535f),FVector2D(0.271059f, -0.505058f),FVector2D(0.248493f, 0.767084f),FVector2D(-0.596259f, 0.621869f),
    FVector2D(-0.326291f, 0.175500f),FVector2D(0.643448f, -0.439298f),FVector2D(-0.406031f, -0.841996f),FVector2D(-0.795511f, -0.105329f),
    FVector2D(0.309025f, 0.352076f),FVector2D(-0.162909f, 0.375198f),FVector2D(-0.726905f, -0.446674f),FVector2D(0.027331f, -0.337222f),
    FVector2D(0.543546f, 0.735876f),FVector2D(0.309095f, -0.710946f),FVector2D(-0.582790f, 0.154986f),FVector2D(-0.203460f, -0.272108f),
    FVector2D(0.491919f, -0.171667f),FVector2D(-0.405075f, -0.447730f),FVector2D(-0.416467f, -0.072390f),FVector2D(-0.025764f, 0.565929f)
};

static const FVector2D ThirtyTwoRefiningSamples[32] = 
{
	FVector2D(0.522456f, 0.010821f),FVector2D(0.948822f, -0.133876f),FVector2D(-0.250350f, -0.478179f),FVector2D(-0.200341f, -0.181412f),
	FVector2D(-0.216907f, 0.163176f),FVector2D(0.444341f, 0.582881f),FVector2D(0.318114f, -0.339968f),FVector2D(0.323013f, 0.893671f),
	FVector2D(0.103426f, 0.394928f),FVector2D(0.117750f, -0.064166f),FVector2D(-0.540170f, -0.512830f),FVector2D(0.198264f, 0.719767f),
	FVector2D(-0.446541f, 0.255523f),FVector2D(-0.154845f, 0.399522f),FVector2D(-0.132838f, -0.897569f),FVector2D(-0.619868f, 0.685406f),
	FVector2D(0.359397f, 0.231670f),FVector2D(-0.027417f, 0.040958f),FVector2D(-0.044174f, -0.529273f),FVector2D(0.504180f, -0.377851f),
	FVector2D(0.175422f, 0.213013f),FVector2D(-0.039336f, 0.799900f),FVector2D(-0.283380f, -0.786142f),FVector2D(0.241633f, 0.614170f),
	FVector2D(-0.948119f, 0.020092f),FVector2D(0.584043f, 0.276241f),FVector2D(-0.052446f, 0.235604f),FVector2D(-0.077916f, -0.310481f),
	FVector2D(0.139518f, -0.312672f),FVector2D(-0.371752f, 0.465338f),FVector2D(0.287380f, -0.041448f),FVector2D(0.202267f, -0.726132f)
};


/**
* FDiskSampleGenerator
* 
* Provides the functionality to generate samples in a disk.
*/
class FDiskSampleGenerator
{
public:

	FDiskSampleGenerator() {}
	~FDiskSampleGenerator() {}

	/**
	* GenerateSamples - Generates samples in a disk, re-placing if they are too close to already placed ones.
	*
	* @param SampleOffsets - the array to fill with offsets, must be at least length NumSamples
	* @param NumSamples - The number of samples to generate
	* @param RadiusBounds - The minimum radius to place samples is stored in X, max in Y
	* @param DistanceThresholdFactor - Scales the factor used to decide if a sample needs regenerated.  
	*	Reasonable values in [.5, 2].  Lower values allow samples to be clumped together, but higher values
	*	cause samples to be placed many times.
	* @param MaxReplaceTries - the number of times to try placing a sample before the function should give up and not
	*	try to maintain a minimum distance between samples.  This is to provide a bound on the runtime of this algorithm.
	*/
	void GenerateSamples(FVector2D* SampleOffsets, INT NumSamples, FVector2D RadiusBounds, FLOAT DistanceThresholdFactor, INT MaxReplaceTries);
};


class FBranchingPCFProjectionPixelShaderInterface : public FShader
{
	DECLARE_SHADER_TYPE(FBranchingPCFProjectionPixelShaderInterface,Global);
public:

	FBranchingPCFProjectionPixelShaderInterface() {}

	FBranchingPCFProjectionPixelShaderInterface(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FShader(Initializer)
	{ }

	/**
	 * Sets the current pixel shader params
	 * @param View - current view
	 * @param ShadowInfo - projected shadow info for a single light
	 */
	virtual void SetParameters(
		INT ViewIndex,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
	}
};

/**
 * Uses lowest sample counts and supports Hardware PCF
 */
class FLowQualityHwPCF
{
public:
	static const UINT NumEdgeSamples = BHardwarePCFEdge_Low;
	static const UINT NumRefiningSamples = BHardwarePCFRefining_Low;
	static const UBOOL bUseHardwarePCF = TRUE;
	static const UBOOL bUseFetch4 = FALSE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return IsPCPlatform(Platform) || Platform == SP_PS3;
	}
};


/**
 * Uses lowest sample counts and supports Fetch4
 */
class FLowQualityFetch4PCF
{
public:
	static const UINT NumEdgeSamples = BFetch4PCFEdge_Low;
	static const UINT NumRefiningSamples = BFetch4PCFRefining_Low;
	static const UBOOL bUseHardwarePCF = FALSE;
	static const UBOOL bUseFetch4 = TRUE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return IsPCPlatform(Platform); 
	}
};

/**
 * Uses lowest sample counts
 */
class FLowQualityManualPCF
{
public:
	static const UINT NumEdgeSamples = BPCFEdge_Low;
	static const UINT NumRefiningSamples = BPCFRefining_Low;
	static const UBOOL bUseHardwarePCF = FALSE;
	static const UBOOL bUseFetch4 = FALSE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}
};

/**
 * Uses medium sample counts and supports Hardware PCF
 */
class FMediumQualityHwPCF
{
public:
	static const UINT NumEdgeSamples = BHardwarePCFEdge_Medium;
	static const UINT NumRefiningSamples = BHardwarePCFRefining_Medium;
	static const UBOOL bUseHardwarePCF = TRUE;
	static const UBOOL bUseFetch4 = FALSE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return IsPCPlatform(Platform) || Platform == SP_PS3;
	}
};

/**
 * Uses medium sample counts and supports Fetch4
 */
class FMediumQualityFetch4PCF
{
public:
	static const UINT NumEdgeSamples = BFetch4PCFEdge_Medium;
	static const UINT NumRefiningSamples = BFetch4PCFRefining_Medium;
	static const UBOOL bUseHardwarePCF = FALSE;
	static const UBOOL bUseFetch4 = TRUE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return IsPCPlatform(Platform); 	
	}
};

/**
* Uses medium sample counts
*/
class FMediumQualityManualPCF
{
public:
	static const UINT NumEdgeSamples = BPCFEdge_Medium;
	static const UINT NumRefiningSamples = BPCFRefining_Medium;
	static const UBOOL bUseHardwarePCF = FALSE;
	static const UBOOL bUseFetch4 = FALSE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}
};

/**
 * Uses highest sample counts and supports Hardware PCF
 */
class FHighQualityHwPCF
{
public:
	static const UINT NumEdgeSamples = BHardwarePCFEdge_High;
	static const UINT NumRefiningSamples = BHardwarePCFRefining_High;
	static const UBOOL bUseHardwarePCF = TRUE;
	static const UBOOL bUseFetch4 = FALSE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return IsPCPlatform(Platform) || Platform == SP_PS3;
	}
};

/**
 * Uses highest sample counts and supports Fetch4
 */
class FHighQualityFetch4PCF
{
public:
	static const UINT NumEdgeSamples = BFetch4PCFEdge_High;
	static const UINT NumRefiningSamples = BFetch4PCFRefining_High;
	static const UBOOL bUseHardwarePCF = FALSE;
	static const UBOOL bUseFetch4 = TRUE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return IsPCPlatform(Platform);
	}
};

/**
* Uses highest sample counts
*/
class FHighQualityManualPCF
{
public:
	static const UINT NumEdgeSamples = BPCFEdge_High;
	static const UINT NumRefiningSamples = BPCFRefining_High;
	static const UBOOL bUseHardwarePCF = FALSE;
	static const UBOOL bUseFetch4 = FALSE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}
};

/**
* FBranchingPCFProjectionPixelShader
*
* A pixel shader for projecting an object's shadow buffer onto the scene, with the following enhancements:
*	-At startup, Samples are randomly placed in a disk but are replaced if they are too close
*   -While rendering, Samples are taken to detect the penumbra, called the 'edge' samples.  
These are placed around the circumference of the disk.
*	-The edge samples are rotated per-pixel to convert error into noise.
*	-If all of the edge samples pass or fail their depth comparisons, then the early-out branch is taken,
*	-Else more samples are taken to refine the penumbra. 
*/
template<class BranchingPCFPolicy>
class FBranchingPCFProjectionPixelShader : public FBranchingPCFProjectionPixelShaderInterface
{
	DECLARE_SHADER_TYPE(FBranchingPCFProjectionPixelShader,Global);
public:

	/** 
	* Constructor - binds all shader params
	* @param Initializer - init data from shader compiler
	*/
	FBranchingPCFProjectionPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FBranchingPCFProjectionPixelShaderInterface(Initializer)
	{
		SceneTextureParams.Bind(Initializer.ParameterMap);
		ScreenToShadowMatrixParameter.Bind(Initializer.ParameterMap,TEXT("ScreenToShadowMatrix"),TRUE);
		InvRandomAngleTextureSize.Bind(Initializer.ParameterMap,TEXT("InvRandomAngleTextureSize"),TRUE);
		ShadowDepthTextureParameter.Bind(Initializer.ParameterMap,TEXT("ShadowDepthTexture"),TRUE);
		RandomAngleTextureParameter.Bind(Initializer.ParameterMap,TEXT("RandomAngleTexture"),TRUE);
		RefiningSampleOffsetsParameter.Bind(Initializer.ParameterMap,TEXT("RefiningSampleOffsets"),TRUE);
		EdgeSampleOffsetsParameter.Bind(Initializer.ParameterMap,TEXT("EdgeSampleOffsets"),TRUE);
		ShadowBufferSizeParameter.Bind(Initializer.ParameterMap,TEXT("ShadowBufferSize"),TRUE);
		ShadowFadeFractionParameter.Bind(Initializer.ParameterMap,TEXT("ShadowFadeFraction"),TRUE);

		SetSampleOffsets();
	}

	FBranchingPCFProjectionPixelShader() 
	{
		SetSampleOffsets();
	}

/**
* SetSampleOffsets - populates EdgeSampleOffsets and RefiningSampleOffsets by assigning an appropriate cache if one exists
*	or generating new samples.
*/
	void SetSampleOffsets()
	{
		//use the manually cached samples if the appropriate one exists
		
		if (BranchingPCFPolicy::NumEdgeSamples == 4)
		{
			appMemcpy(EdgeSampleOffsets, FourEdgeSamples, 4 * sizeof(FVector2D));
		}
		else if (BranchingPCFPolicy::NumEdgeSamples == 8)
		{
			appMemcpy(EdgeSampleOffsets, EightEdgeSamples, 8 * sizeof(FVector2D));
		}
		//otherwise generate new samples
		else
		{
			//generate samples only around the circumference of the disk for the penumbra detecting samples
			DiskSampleGenerator.GenerateSamples(&EdgeSampleOffsets[0], BranchingPCFPolicy::NumEdgeSamples, FVector2D(1.0f, 1.0f), 1.5f, 10);
		}

		//use the manually cached samples if the appropriate one exists
		if (BranchingPCFPolicy::NumRefiningSamples == 4)
		{
			appMemcpy(RefiningSampleOffsets, FourRefiningSamples, 4 * sizeof(FVector2D));
		}
		else if (BranchingPCFPolicy::NumRefiningSamples == 8)
		{
			appMemcpy(RefiningSampleOffsets, EightRefiningSamples, 8 * sizeof(FVector2D));
		}
		else if (BranchingPCFPolicy::NumRefiningSamples == 12)
		{
			appMemcpy(RefiningSampleOffsets, TwelveRefiningSamples, 12 * sizeof(FVector2D));
		}
		else if (BranchingPCFPolicy::NumRefiningSamples == 24)
		{
			appMemcpy(RefiningSampleOffsets, TwentyFourRefiningSamples, 24 * sizeof(FVector2D));
		}
		else if (BranchingPCFPolicy::NumRefiningSamples == 32)
		{
			appMemcpy(RefiningSampleOffsets, ThirtyTwoRefiningSamples, 32 * sizeof(FVector2D));
		}
		//otherwise generate new samples
		else 
		{
			DiskSampleGenerator.GenerateSamples(&RefiningSampleOffsets[0], BranchingPCFPolicy::NumRefiningSamples, FVector2D(0.0f, 1.0f), 1.0f, 10);
		}
	}
	

	/**
	* Sets the current pixel shader params
	* @param View - current view
	* @param ShadowInfo - projected shadow info for a single light
	*/
	virtual void SetParameters(
		INT ViewIndex,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		SceneTextureParams.Set(&View,this, SF_Point, SceneDepthUsage_ProjectedShadows);
		
		// Set the transform from screen coordinates to shadow depth texture coordinates.
		const FMatrix ScreenToShadow = ShadowInfo->GetScreenToShadowMatrix(View, FALSE);
		SetPixelShaderValue(FShader::GetPixelShader(),ScreenToShadowMatrixParameter,ScreenToShadow);

		FVector2D InvTexSize = FVector2D(View.RenderTargetSizeX / FLOAT(GEngine->RandomAngleTexture->SizeX), View.RenderTargetSizeY / FLOAT(GEngine->RandomAngleTexture->SizeY));
		SetPixelShaderValue(GetPixelShader(),InvRandomAngleTextureSize, InvTexSize);

		const FIntPoint ShadowBufferResolution = ShadowInfo->GetShadowBufferResolution(FALSE);
		if (ShadowBufferSizeParameter.IsBound())
		{
			SetPixelShaderValue(GetPixelShader(),ShadowBufferSizeParameter, FVector2D(
				(FLOAT)ShadowBufferResolution.X, 
				(FLOAT)ShadowBufferResolution.Y));
		}

		SetPixelShaderValue(GetPixelShader(),ShadowFadeFractionParameter, ShadowInfo->FadeAlphas(ViewIndex));

		FTexture2DRHIRef ShadowDepthSampler;
		FSamplerStateRHIParamRef DepthSamplerState;

		if (BranchingPCFPolicy::bUseHardwarePCF)
		{
			//take advantage of linear filtering on nvidia depth stencil textures
			DepthSamplerState = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
			//sample the depth texture
			ShadowDepthSampler = GSceneRenderTargets.GetShadowDepthZTexture(ShadowInfo->IsPrimaryWholeSceneDominantShadow(), ShadowInfo->bAllocatedInPreshadowCache);
		} 
		else if (GSupportsDepthTextures)
		{
			DepthSamplerState = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
			//sample the depth texture
			ShadowDepthSampler = GSceneRenderTargets.GetShadowDepthZTexture(ShadowInfo->IsPrimaryWholeSceneDominantShadow(), ShadowInfo->bAllocatedInPreshadowCache);
		} 
		else if (BranchingPCFPolicy::bUseFetch4)
		{
			//enable Fetch4 on this sampler
			DepthSamplerState = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp,MIPBIAS_Get4>::GetRHI();
			//sample the depth texture
			ShadowDepthSampler = GSceneRenderTargets.GetShadowDepthZTexture(ShadowInfo->IsPrimaryWholeSceneDominantShadow(), ShadowInfo->bAllocatedInPreshadowCache);
		} 
		else
		{
			DepthSamplerState = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
			//sample the color depth texture
			ShadowDepthSampler = GSceneRenderTargets.GetShadowDepthColorTexture(ShadowInfo->IsPrimaryWholeSceneDominantShadow(), ShadowInfo->bAllocatedInPreshadowCache);
		}

		SetTextureParameter(
			GetPixelShader(),
			ShadowDepthTextureParameter,
			DepthSamplerState,
			ShadowDepthSampler
			);

		SetTextureParameter(
			GetPixelShader(),
			RandomAngleTextureParameter,
			TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
			GEngine->RandomAngleTexture->Resource->TextureRHI
			);

		/** ShadowFilterRadius is the radius of the disk of samples that will be used by Branching PCF, in shadowmap texels.  
		Larger values result in larger penumbras, which may require more filter samples to refine and will
		result in more pixels taking the expensive penumbra branch during shadow projection. Larger values also
		reduce 'banding' that occurs on self-shadowing moving objects. */
		const FLOAT InvBufferResolution = 1.0f / (FLOAT)ShadowBufferResolution.X;
		const FLOAT TexelRadius = GSystemSettings.ShadowFilterRadius * InvBufferResolution;

		//pack two samples into each register
		for(INT SampleIndex = 0;SampleIndex < BranchingPCFPolicy::NumEdgeSamples; SampleIndex += 2)
		{
			FVector4 Offsets = FVector4(
				EdgeSampleOffsets[SampleIndex].X * TexelRadius,
				EdgeSampleOffsets[SampleIndex].Y * TexelRadius,
				EdgeSampleOffsets[SampleIndex + 1].X * TexelRadius,
				EdgeSampleOffsets[SampleIndex + 1].Y * TexelRadius);


			SetPixelShaderValue(
				GetPixelShader(),
				EdgeSampleOffsetsParameter,
				Offsets,
				SampleIndex / 2
				);

		}

		for(INT SampleIndex = 0;SampleIndex < BranchingPCFPolicy::NumRefiningSamples; SampleIndex += 2)
		{
			SetPixelShaderValue(
				GetPixelShader(),
				RefiningSampleOffsetsParameter,
				FVector4(
				RefiningSampleOffsets[SampleIndex].X * TexelRadius,
				RefiningSampleOffsets[SampleIndex].Y * TexelRadius,
				RefiningSampleOffsets[SampleIndex + 1].X * TexelRadius,
				RefiningSampleOffsets[SampleIndex + 1].Y * TexelRadius),
				SampleIndex / 2
				);
		}	
	}

	/**
	* @param Platform - hardware platform
	* @return TRUE if this shader should be cached
	*/
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return BranchingPCFPolicy::ShouldCache(Platform);
	}

	/**
	* Add any defines required by the shader or light policy
	* @param OutEnvironment - shader environment to modify
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		//Refining samples are done 4 at a time, to take advantage of vectorized instructions.  Each chunk is then 4 samples,
		//and 2 registers, since 2 samples fit in a register.
		OutEnvironment.Definitions.Set(TEXT("NUM_REFINING_SAMPLE_CHUNKS"), *FString::Printf(TEXT("%u"), BranchingPCFPolicy::NumRefiningSamples / 4));

		//Edge samples are done 2 at a time, to allow smaller increments.  Each chunk is then 2 samples,
		//and 1 register, since 2 samples fit in a register. 
		OutEnvironment.Definitions.Set(TEXT("NUM_EDGE_SAMPLE_CHUNKS"), *FString::Printf(TEXT("%u"), BranchingPCFPolicy::NumEdgeSamples / 2));

		//The HLSL compiler for xenon will not always use predicated instructions without this flag.  
		//On PC the compiler consistently makes the right decision.
		new(OutEnvironment.CompilerFlags) ECompilerFlags(CFLAG_PreferFlowControl);
	}

	/**
	* Serialize shader parameters for this shader
	* @param Ar - archive to serialize with
	*/
	UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SceneTextureParams;
		Ar << ScreenToShadowMatrixParameter;
		Ar << InvRandomAngleTextureSize;
		Ar << ShadowDepthTextureParameter;
		Ar << RandomAngleTextureParameter;
		Ar << RefiningSampleOffsetsParameter;
		Ar << EdgeSampleOffsetsParameter;
		Ar << ShadowBufferSizeParameter;
		Ar << ShadowFadeFractionParameter;
		return bShaderHasOutdatedParameters;
	}

private:

	FVector2D EdgeSampleOffsets[BranchingPCFPolicy::NumEdgeSamples];
	FVector2D RefiningSampleOffsets[BranchingPCFPolicy::NumRefiningSamples];

	FDiskSampleGenerator DiskSampleGenerator;

	FSceneTextureShaderParameters SceneTextureParams;
	FShaderParameter ScreenToShadowMatrixParameter;
	FShaderResourceParameter ShadowDepthTextureParameter;
	FShaderResourceParameter RandomAngleTextureParameter;
	FShaderParameter RefiningSampleOffsetsParameter;	
	FShaderParameter EdgeSampleOffsetsParameter;	
	FShaderParameter InvRandomAngleTextureSize;
	FShaderParameter ShadowBufferSizeParameter;
	FShaderParameter ShadowFadeFractionParameter;
};

/**
* A pixel shader for projecting an object's shadow buffer onto the scene.
* Attenuates shadow based on distance and modulates its color.
* For use with modulated shadows.
*/
template<class BranchingPCFPolicy>
class FBranchingPCFModProjectionPixelShader : public FBranchingPCFProjectionPixelShader< BranchingPCFPolicy >
{
	DECLARE_SHADER_TYPE(FBranchingPCFModProjectionPixelShader,Global);
public:

	/**
	* Constructor
	*/
	FBranchingPCFModProjectionPixelShader() {}

	/**
	* Constructor - binds all shader params
	* @param Initializer - init data from shader compiler
	*/
		FBranchingPCFModProjectionPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FBranchingPCFProjectionPixelShader< BranchingPCFPolicy >(Initializer)
	{
		ShadowModulateColorParam.Bind(Initializer.ParameterMap,TEXT("ShadowModulateColor"));
		ScreenToWorldParam.Bind(Initializer.ParameterMap,TEXT("ScreenToWorld"),TRUE);
	}

	/**
	* Sets the current pixel shader params
	* @param View - current view
	* @param ShadowInfo - projected shadow info for a single light
	*/
	virtual void SetParameters( 
		INT ViewIndex,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		FBranchingPCFProjectionPixelShader< BranchingPCFPolicy >::SetParameters(ViewIndex,View,ShadowInfo);		
		const FLightSceneInfo* LightSceneInfo = ShadowInfo->LightSceneInfo;

		// color to modulate shadowed areas on screen
		SetPixelShaderValue(
			FShader::GetPixelShader(),
			ShadowModulateColorParam,
			Lerp(FLinearColor::White,LightSceneInfo->ModShadowColor,ShadowInfo->FadeAlphas(ViewIndex))
			);
		// screen space to world space transform
		FMatrix ScreenToWorld = FMatrix(
			FPlane(1,0,0,0),
			FPlane(0,1,0,0),
			FPlane(0,0,(1.0f - Z_PRECISION),1),
			FPlane(0,0,-View.NearClippingDistance * (1.0f - Z_PRECISION),0)
			) * 
			View.InvTranslatedViewProjectionMatrix;
		SetPixelShaderValue( FShader::GetPixelShader(), ScreenToWorldParam, ScreenToWorld );
	}

	/**
	* Serialize the parameters for this shader
	* @param Ar - archive to serialize to
	*/
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FBranchingPCFProjectionPixelShader< BranchingPCFPolicy >::Serialize(Ar);
		Ar << ShadowModulateColorParam;
		Ar << ScreenToWorldParam;
		return bShaderHasOutdatedParameters;
	}

	/**
	* Add any defines required by the shader or light policy
	* @param OutEnvironment - shader environment to modify
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FBranchingPCFProjectionPixelShader< BranchingPCFPolicy >::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return FBranchingPCFProjectionPixelShader< BranchingPCFPolicy >::ShouldCache(Platform);
	}

private:
	/** color to modulate shadowed areas on screen */
	FShaderParameter ShadowModulateColorParam;	
	/** needed to get world positions from deferred scene depth values */
	FShaderParameter ScreenToWorldParam;
};

/**
* Attenuation is based on light type so the modulated shadow projection 
* is coupled with a LightTypePolicy type. Use with FModShadowProjectionVertexShader
*/
template<class LightTypePolicy, class BranchingPCFPolicy>
class TBranchingPCFModProjectionPixelShader : public FBranchingPCFModProjectionPixelShader<BranchingPCFPolicy>, public LightTypePolicy::ModShadowPixelParamsType
{
	DECLARE_SHADER_TYPE(TBranchingPCFModProjectionPixelShader,Global);
public:
	typedef typename LightTypePolicy::SceneInfoType LightSceneInfoType;

	/**
	* Constructor
	*/
	TBranchingPCFModProjectionPixelShader() {}

	/**
	* Constructor - binds all shader params
	* @param Initializer - init data from shader compiler
	*/
	TBranchingPCFModProjectionPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FBranchingPCFModProjectionPixelShader<BranchingPCFPolicy>(Initializer)
	{
		LightTypePolicy::ModShadowPixelParamsType::Bind(Initializer.ParameterMap);
	}

	/**
	* Add any defines required by the shader or light policy
	* @param OutEnvironment - shader environment to modify
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FBranchingPCFModProjectionPixelShader<BranchingPCFPolicy>::ModifyCompilationEnvironment(Platform, OutEnvironment);
		LightTypePolicy::ModShadowPixelParamsType::ModifyCompilationEnvironment(Platform, OutEnvironment);	
	}

	/**
	* Sets the current pixel shader params
	* @param View - current view
	* @param ShadowInfo - projected shadow info for a single light
	*/
	void SetParameters(
		INT ViewIndex,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		FBranchingPCFModProjectionPixelShader<BranchingPCFPolicy>::SetParameters(ViewIndex,View,ShadowInfo);
		LightTypePolicy::ModShadowPixelParamsType::SetModShadowLight( this, (const LightSceneInfoType*) ShadowInfo->LightSceneInfo, &View );
	}

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return FBranchingPCFModProjectionPixelShader<BranchingPCFPolicy>::ShouldCache(Platform);
	}

	/**
	* Serialize the parameters for this shader
	* @param Ar - archive to serialize to
	*/
	UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FBranchingPCFModProjectionPixelShader<BranchingPCFPolicy>::Serialize(Ar);
		LightTypePolicy::ModShadowPixelParamsType::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};


/* SetBranchingPCFParameters - chooses the appropriate instance of the FBranchingPCFProjectionPixelShader class template,
*							 and sets its parameters.
*
* @param View - current View
* @param ShadowInfo - current FProjectedShadowInfo
* @param LightShadowQuality - light's filter quality setting
* @return FPixelShaderRHIRef - The pixel shader ref of the chosen instance
*/
extern FShader* SetBranchingPCFParameters(
	INT ViewIndex,
	const FSceneView& View, 
	const FProjectedShadowInfo* ShadowInfo,
	BYTE LightShadowQuality);


/* GetBranchingPCFModProjPixelShaderRef - chooses the appropriate instance of the FBranchingPCFModProjectionPixelShader class template,
*				 and returns a pointer to it.
* 
* @param LightShadowQuality - light's filter quality setting
* @return FBranchingPCFProjectionPixelShaderInterface - a pointer to the chosen instance
*/
template<class LightTypePolicy>
FBranchingPCFProjectionPixelShaderInterface* GetBranchingPCFModProjPixelShaderRef(BYTE LightShadowQuality)
{
	//apply the system settings bias to the light's shadow quality
	BYTE EffectiveShadowFilterQuality = Max(LightShadowQuality + GSystemSettings.ShadowFilterQualityBias, 0);

	//choose quality based on global settings if the light is using the default projection technique,
	//otherwise use the light's projection technique
	if (EffectiveShadowFilterQuality == SFQ_Low) 
	{
		//use the appropriate version based on hardware capabilities
		if (GSceneRenderTargets.IsHardwarePCFSupported())
		{
			TShaderMapRef<TBranchingPCFModProjectionPixelShader<LightTypePolicy, FLowQualityHwPCF> > ModShadowShader(GetGlobalShaderMap());
			return *ModShadowShader;
		}
		else if (GSceneRenderTargets.IsFetch4Supported())
		{
			TShaderMapRef<TBranchingPCFModProjectionPixelShader<LightTypePolicy, FLowQualityFetch4PCF> > ModShadowShader(GetGlobalShaderMap());
			return *ModShadowShader;
		}
		else
		{
			TShaderMapRef<TBranchingPCFModProjectionPixelShader<LightTypePolicy, FLowQualityManualPCF> > ModShadowShader(GetGlobalShaderMap());
			return *ModShadowShader;
		}
	} 
	else if (EffectiveShadowFilterQuality == SFQ_Medium) 
	{
		//use the appropriate version based on hardware capabilities
		if (GSceneRenderTargets.IsHardwarePCFSupported())
		{
			TShaderMapRef<TBranchingPCFModProjectionPixelShader<LightTypePolicy, FMediumQualityHwPCF> > ModShadowShader(GetGlobalShaderMap());
			return *ModShadowShader;
		} 
		else if (GSceneRenderTargets.IsFetch4Supported())
		{
			TShaderMapRef<TBranchingPCFModProjectionPixelShader<LightTypePolicy, FMediumQualityFetch4PCF> > ModShadowShader(GetGlobalShaderMap());
			return *ModShadowShader;
		}
		else
		{
			TShaderMapRef<TBranchingPCFModProjectionPixelShader<LightTypePolicy, FMediumQualityManualPCF> > ModShadowShader(GetGlobalShaderMap());
			return *ModShadowShader;
		}
	}
	else
	{
		//use the appropriate version based on hardware capabilities
		if (GSceneRenderTargets.IsHardwarePCFSupported())
		{
			TShaderMapRef<TBranchingPCFModProjectionPixelShader<LightTypePolicy, FHighQualityHwPCF> > ModShadowShader(GetGlobalShaderMap());
			return *ModShadowShader;
		} 
		else if (GSceneRenderTargets.IsFetch4Supported())
		{
			TShaderMapRef<TBranchingPCFModProjectionPixelShader<LightTypePolicy, FHighQualityFetch4PCF> > ModShadowShader(GetGlobalShaderMap());
			return *ModShadowShader;
		}
		else
		{
			TShaderMapRef<TBranchingPCFModProjectionPixelShader<LightTypePolicy, FHighQualityManualPCF> > ModShadowShader(GetGlobalShaderMap());
			return *ModShadowShader;
		}
	}
}


/** ShouldUseBranchingPCF - indicates whether or not to use the Branching PCF shadows based on global settings and per-light settings
*
* @param ShadowProjectionTechnique - the shadow technique of the light in question, a member of EShadowProjectionTechnique
* @return UBOOL - TRUE if Branching PCF should be used
*/
extern UBOOL ShouldUseBranchingPCF(BYTE ShadowProjectionTechnique);

