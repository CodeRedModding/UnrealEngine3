/*=============================================================================
	DOFAndBloomEffect.cpp: Combined depth of field and bloom effect implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_DOFANDBLOOMEFFECT
#define _INC_DOFANDBLOOMEFFECT




/** Encapsulates common postprocess parameters. */
class FPostProcessParameters
{
public:

	/** Default constructor. */
	FPostProcessParameters() {}

	/** Initialization constructor. */
	FPostProcessParameters(const FShaderParameterMap& ParameterMap);

	/** Set the dof pixel shader parameter values. */
	void SetPS(FShader* PixelShader, FLOAT BlurKernelSize, FLOAT DOFOcclusionTweak);

#if WITH_D3D11_TESSELLATION
	/** Set the dof geometry shader parameter values. */
	void SetGS(FShader* GeometryShader, FLOAT BlurKernelSize, FLOAT DOFOcclusionTweak);
#endif

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FPostProcessParameters& P);

private:
	FShaderResourceParameter FilterColor0TextureParameter;
	FShaderResourceParameter FilterColor1TextureParameter;
	FShaderResourceParameter FilterColor2TextureParameter;
	FShaderResourceParameter SeparateTranslucencyTextureParameter;
	FShaderParameter DOFKernelSizeParameter;
};



/*-----------------------------------------------------------------------------
TDOFAndBloomGatherPixelShader - Encapsulates the DOF and bloom gather pixel shader.
-----------------------------------------------------------------------------*/
template<UINT NumSamples, UBOOL bSeparateTranslucency> 
class TDOFAndBloomGatherPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDOFAndBloomGatherPixelShader,Global);

public:
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("NUM_SAMPLES"),*FString::Printf(TEXT("%u"),NumSamples));
		OutEnvironment.Definitions.Set(TEXT("USE_SEPARATE_TRANSLUCENCY"), bSeparateTranslucency ? TEXT("1") : TEXT("0"));

		//tell the compiler to unroll
		new(OutEnvironment.CompilerFlags) ECompilerFlags(CFLAG_AvoidFlowControl);
	}

	/** Default constructor. */
	TDOFAndBloomGatherPixelShader() {}

	/** Initialization constructor. */
	TDOFAndBloomGatherPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
		,	DOFParameters(Initializer.ParameterMap)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		BloomScaleAndThresholdParameter.Bind(Initializer.ParameterMap,TEXT("BloomScaleAndThreshold"),TRUE);
		GatherParamsParameter.Bind(Initializer.ParameterMap,TEXT("GatherParams"),TRUE);
		SeparateTranslucencyParameter.Bind(Initializer.ParameterMap, TEXT("SeparateTranslucencyTexture"), TRUE);
		SmallSceneColorTextureParameter.Bind(Initializer.ParameterMap,TEXT("SmallSceneColorTexture"),TRUE);
		VelocityTextureParameter.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"), TRUE );
	}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar	<< DOFParameters << SceneTextureParameters << BloomScaleAndThresholdParameter
			<< SeparateTranslucencyParameter << SmallSceneColorTextureParameter
			<< GatherParamsParameter << VelocityTextureParameter;

		BloomScaleAndThresholdParameter.SetShaderParamName(TEXT("BloomScaleAndThreshold"));
		return bShaderHasOutdatedParameters;
	}
	
	static const TCHAR* GetSourceFilename()
	{
		return TEXT("DOFAndBloomGatherPixelShader");
	}

	FDOFShaderParameters DOFParameters;
	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderParameter BloomScaleAndThresholdParameter;
	FShaderParameter GatherParamsParameter;
	FShaderResourceParameter SeparateTranslucencyParameter;
	FShaderResourceParameter SmallSceneColorTextureParameter;
	FShaderResourceParameter VelocityTextureParameter;
};

/*-----------------------------------------------------------------------------
TDOFGatherPixelShader - Encapsulates the DOF shader only.
-----------------------------------------------------------------------------*/
template<UINT NumSamples> 
class TDOFGatherPixelShader :public TDOFAndBloomGatherPixelShader<NumSamples, FALSE>
{
	DECLARE_SHADER_TYPE(TDOFGatherPixelShader,Global);

	/** Default constructor. */
	TDOFGatherPixelShader() {}

	/** Initialization constructor. */
	TDOFGatherPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	TDOFAndBloomGatherPixelShader<NumSamples, FALSE>(Initializer)
	{
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainGatherDOF");
	}
};

/*-----------------------------------------------------------------------------
TBloomGatherPixelShader - Encapsulates the DOF shader only.
-----------------------------------------------------------------------------*/
template<UINT NumSamples, UBOOL bSeparateTranslucency> 
class TBloomGatherPixelShader :public TDOFAndBloomGatherPixelShader<NumSamples, bSeparateTranslucency>
{
	DECLARE_SHADER_TYPE(TBloomGatherPixelShader,Global);

	/** Default constructor. */
	TBloomGatherPixelShader() {}

	/** Initialization constructor. */
	TBloomGatherPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	TDOFAndBloomGatherPixelShader<NumSamples, bSeparateTranslucency>(Initializer)
	{
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainGatherBloom");
	}
};


/*-----------------------------------------------------------------------------
TMotionBlurGatherPixelShader - Encapsulates the motion blur shader only.
-----------------------------------------------------------------------------*/
template<UINT NumSamples> 
class TMotionBlurGatherPixelShader :public TDOFAndBloomGatherPixelShader<NumSamples, FALSE>
{
	DECLARE_SHADER_TYPE(TMotionBlurGatherPixelShader,Global);

	/** Default constructor. */
	TMotionBlurGatherPixelShader() {}

	/** Initialization constructor. */
	TMotionBlurGatherPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	TDOFAndBloomGatherPixelShader<NumSamples, FALSE>(Initializer)
	{
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainGatherMotionBlur");
	}
};

/*-----------------------------------------------------------------------------
FDOFAndBloomGatherVertexShader - Encapsulates the DOF and bloom gather vertex shader.
-----------------------------------------------------------------------------*/
template<UINT NumSamples> 
class TDOFAndBloomGatherVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDOFAndBloomGatherVertexShader,Global);

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
	TDOFAndBloomGatherVertexShader() {}

	/** Initialization constructor. */
	TDOFAndBloomGatherVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		SampleOffsetsParameter.Bind(Initializer.ParameterMap,TEXT("SampleOffsets"),TRUE);
	}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SampleOffsetsParameter;
		return bShaderHasOutdatedParameters;
	}

	FShaderParameter SampleOffsetsParameter;
};

/** Encapsulates the DOF and bloom blend pixel shader. */
class FDOFAndBloomBlendPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDOFAndBloomBlendPixelShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform);

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);

	/** Default constructor. */
	FDOFAndBloomBlendPixelShader() {}

public:

	FDOFShaderParameters DOFParameters;
	FPostProcessParameters PostProcessParameters;
	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderResourceParameter DoFBlurBufferParameter;
	FShaderParameter BloomTintAndScreenBlendThresholdParameter;

	/** Initialization constructor. */
	FDOFAndBloomBlendPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar);
};

/** Encapsulates the DOF and bloom blend pixel shader. */
class FDOFAndBloomBlendVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDOFAndBloomBlendVertexShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform);
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);

	/** Default constructor. */
	FDOFAndBloomBlendVertexShader() {}

public:

	FShaderParameter SceneCoordinateScaleBiasParameter;

	/** Initialization constructor. */
	FDOFAndBloomBlendVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar);
};

/*-----------------------------------------------------------------------------
FDOFAndBloomPostProcessSceneProxy
-----------------------------------------------------------------------------*/

// used by the other SET/GET_POSTPROCESS_... macros
#define GET_POSTPROCESS_PROPERTY_X(Group, WorldSettingsName, EffectName) \
	(WorldSettings && WorldSettings->bOverride_##Group##_##WorldSettingsName) \
	? WorldSettings->Group##_##WorldSettingsName \
	: InEffect->EffectName

// useful macro to get override postprocess settings if specified
#define GET_POSTPROCESS_PROPERTY1(Group, Name) GET_POSTPROCESS_PROPERTY_X(Group, Name, Name)

// useful macro to get override postprocess settings if specified, gives more control than GET_POSTPROCESS_PROPERTY1()
#define GET_POSTPROCESS_PROPERTY2(Group, Name) GET_POSTPROCESS_PROPERTY_X(Group, Name, Group##Name)

// useful macro to set postprocess settings (internal name need to match)
#define SET_POSTPROCESS_PROPERTY1(Group, Name) Name = GET_POSTPROCESS_PROPERTY_X(Group, Name, Name);

// useful macro to set postprocess settings (internal name need to match), gives more control than SET_POSTPROCESS_PROPERTY1()
#define SET_POSTPROCESS_PROPERTY2(Group, Name) Group##Name = GET_POSTPROCESS_PROPERTY_X(Group, Name, Group##Name);

class FDOFAndBloomPostProcessSceneProxy : public FPostProcessSceneProxy
{
public:
	/** 
	* Initialization constructor. 
	* @param InEffect - DOF post process effect to mirror in this proxy
	*/
	FDOFAndBloomPostProcessSceneProxy(const UDOFAndBloomEffect* InEffect,const FPostProcessSettings* WorldSettings);

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

	/**
	 * Overriden by the DepthOfField effect.
	 * @param Params - The parameters for the effect are returned in this struct.
	 * @return whether the data was filled in.
	 */
	virtual UBOOL ComputeDOFParams(const FViewInfo& View, struct FDepthOfFieldParams &Params ) const;

protected:

	template <class TPShader, class TVShader> void SetupGather2x2(FViewInfo& View, TVShader &VertexShader, const FTexture2DRHIRef& SourceTexture, FLOAT CustomColorScale, UBOOL bBilinearFiltered);

	/**
	 * Calculate depth of field focus distance and focus radius
	 *
	 * @param View current view being rendered
	 * @param OutFocusDistance [out] center of focus distance in clip space
	 * @param OutFocusRadius [out] radius about center of focus in clip space
	 */
	void CalcDoFParams(const FViewInfo& View, FLOAT& OutFocusDistance, FLOAT& OutFocusRadius) const;

	/** required for RenderGatherPass() */
	enum EGatherData
	{
		EGD_DepthOfField,
		EGD_Bloom,
		EGD_MotionBlur
	};

	/**
	* Renders the gather pass which generates input for the subsequent blurring steps
	* to a low resolution filter buffer.
	* @param DestinationData e.g. EGD_DepthOfField, EGD_Bloom, EGD_MotionBlur
	* @param FilterColorIndex which buffer to use FilterColor 0/1/2
	*/
	void RenderGatherPass(FViewInfo& View, EGatherData DestinationData, FSceneRenderTargetIndex FilterColorIndex, FLOAT CustomColorScale, INT Quality = 0, UBOOL bSeparateTranslucency = FALSE);

	/** Down sample scene to half resolution (split screen: one view at a time), depth in alpha */
	void DownSampleSceneAndDepth(FViewInfo& View);

	/** mirrored properties. See DOFEffect.uc for descriptions */
	FLOAT FalloffExponent;
	FLOAT BlurKernelSize;
	FLOAT BlurBloomKernelSize;
	FLOAT MaxNearBlurAmount;
	FLOAT MinBlurAmount;
	FLOAT MaxFarBlurAmount;
	BYTE FocusType;
	FLOAT FocusInnerRadius;
	FLOAT FocusDistance;
	FVector FocusPosition;
	FLOAT BloomScale;
	FLOAT BloomThreshold;
	FLinearColor BloomTint;
	FLOAT BloomScreenBlendThreshold;
	EDOFType DepthOfFieldType;
	EDOFQuality DepthOfFieldQuality;
	UTexture *ColorGrading_LookupTable;
	UTexture2D *BokehTexture;
	FLOAT DOFOcclusionTweak;

	/** Values for clamping range of depths in gather pass */
	FLOAT MinDepth;
	FLOAT MaxDepth;

	/** bound shader state for the blend pass */
	static FGlobalBoundShaderState BlendBoundShaderState;
};

#endif
