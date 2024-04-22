/*=============================================================================
	BasePassRendering.h: Base pass rendering definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "LightMapRendering.h"
#include "TessellationRendering.h"

/** Whether to render the dominant lights in the base pass, which is significantly faster than using an extra light pass. */
/** Whether to render dominant lights in the base pass, which is significantly faster than using an extra light pass. */
#if XBOX || PS3 || WIIU
// Currently only enabled on Xbox and PS3, PC SM3 doesn't have enough interpolators.
	#define GOnePassDominantLight (TRUE)
#else 
	#define GOnePassDominantLight (FALSE)
#endif

/** Whether to use deferred shading, where the base pass outputs G buffer attributes, and lighting passes fetch these attributes and do shading based on them. */
extern UBOOL GAllowDeferredShading;

/** Returns TRUE if the given material and primitive can be lit in a deferred pass. */
extern UBOOL MeshSupportsDeferredLighting(const FMaterial* Material, const FPrimitiveSceneInfo* PrimitiveSceneInfo);

/** Returns TRUE if the engine should use deferred shading instead of forward lighting. */
inline UBOOL ShouldUseDeferredShading()
{
#if CONSOLE
	return FALSE;
#else
	return GAllowDeferredShading && GRHIShaderPlatform == SP_PCD3D_SM5;
#endif
}

/**
 * The base shader type for vertex shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */
template<typename LightMapPolicyType,typename FogDensityPolicyType>
class TBasePassVertexShader : public FMeshMaterialVertexShader, public LightMapPolicyType::VertexParametersType
{
	DECLARE_SHADER_TYPE(TBasePassVertexShader,MeshMaterial);

protected:

	TBasePassVertexShader() {}
	TBasePassVertexShader(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialVertexShader(Initializer)
	{
		LightMapPolicyType::VertexParametersType::Bind(Initializer.ParameterMap);
		MaterialParameters.Bind(Initializer.ParameterMap);
		HeightFogParameters.Bind(Initializer.ParameterMap);
		FogVolumeParameters.Bind(Initializer.ParameterMap);
	}

public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Opaque and modulated materials shouldn't apply fog volumes in their base pass.
		const EBlendMode BlendMode = Material->GetBlendMode();
		const UBOOL bUseFogVolume = IsTranslucentBlendMode(BlendMode) && ((BlendMode != BLEND_Modulate) && (BlendMode != BLEND_ModulateAndAdd));
		const UBOOL bIsFogVolumeShader = FogDensityPolicyType::DensityFunctionType != FVDF_None;
		return	(bUseFogVolume || !bIsFogVolumeShader) &&
				FogDensityPolicyType::ShouldCache(Platform,Material,VertexFactoryType) && 
				LightMapPolicyType::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, OutEnvironment);
		FogDensityPolicyType::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
#if PS3
		//@hack - compiler bug? optimized version crashes during FShader::Serialize call
		static INT RemoveMe=0;	RemoveMe=1;
#endif
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		LightMapPolicyType::VertexParametersType::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
		Ar << HeightFogParameters;
		Ar << MaterialParameters;
		Ar << FogVolumeParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& InMaterialResource,
		const FSceneView& View,
		const UBOOL bAllowGlobalFog
		)
	{
		VertexFactoryParameters.Set(this,VertexFactory,View);
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, InMaterialResource, View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set(this,MaterialRenderContext);
		HeightFogParameters.SetVertexShader(VertexFactory, MaterialRenderProxy, MaterialRenderContext.Material, &View, bAllowGlobalFog, this);
	}

	void SetFogVolumeParameters(
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View,
		typename FogDensityPolicyType::ElementDataType FogVolumeElementData
		)
	{
		FogVolumeParameters.SetVertexShader(View,MaterialRenderProxy, this, FogVolumeElementData);
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshBatch& Mesh, INT BatchElementIndex,const FSceneView& View)
	{
		VertexFactoryParameters.SetMesh(this,Mesh,BatchElementIndex,View);
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialVertexShaderParameters MaterialParameters;

	/** The parameters needed to calculate the fog contribution from height fog layers. */
	FHeightFogShaderParameters HeightFogParameters;

	/** The parameters needed to calculate the fog contribution from an intersecting fog volume. */
	typename FogDensityPolicyType::ShaderParametersType FogVolumeParameters;
};

#if WITH_D3D11_TESSELLATION

/**
 * The base shader type for hull shaders.
 */
template<typename LightMapPolicyType,typename FogDensityPolicyType>
class TBasePassHullShader : public FBaseHullShader
{
	DECLARE_SHADER_TYPE(TBasePassHullShader,MeshMaterial);

protected:

	TBasePassHullShader() {}

	TBasePassHullShader(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseHullShader(Initializer)
	{}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use vertex shader gating
		return FBaseHullShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& TBasePassVertexShader<LightMapPolicyType,FogDensityPolicyType>::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use vertex shader compilation environment
		TBasePassVertexShader<LightMapPolicyType,FogDensityPolicyType>::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}
};

/**
 * The base shader type for Domain shaders.
 */
template<typename LightMapPolicyType,typename FogDensityPolicyType>
class TBasePassDomainShader : public FBaseDomainShader
{
	DECLARE_SHADER_TYPE(TBasePassDomainShader,MeshMaterial);

protected:

	TBasePassDomainShader() {}

	TBasePassDomainShader(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseDomainShader(Initializer)
	{
		FogVolumeParameters.Bind(Initializer.ParameterMap);
		HeightFogParameters.Bind(Initializer.ParameterMap);
	}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use vertex shader gating
		return FBaseDomainShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& TBasePassVertexShader<LightMapPolicyType,FogDensityPolicyType>::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use vertex shader compilation environment
		TBasePassVertexShader<LightMapPolicyType,FogDensityPolicyType>::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

public:

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FBaseDomainShader::Serialize(Ar);
		Ar << HeightFogParameters;
		Ar << FogVolumeParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View
		)
	{
		FBaseDomainShader::SetParameters(MaterialRenderProxy, View);
		HeightFogParameters.SetDomainShader(VertexFactory, MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), &View, this);
	}

	void SetFogVolumeParameters(
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View,
		typename FogDensityPolicyType::ElementDataType FogVolumeElementData
		)
	{
		FogVolumeParameters.SetDomainShader(View,MaterialRenderProxy, this, FogVolumeElementData);
	}

private:

	/** The parameters needed to calculate the fog contribution from height fog layers. */
	FHeightFogShaderParameters HeightFogParameters;

	/** The parameters needed to calculate the fog contribution from an intersecting fog volume. */
	typename FogDensityPolicyType::ShaderParametersType FogVolumeParameters;
};

#endif

/**
 * The base type for pixel shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 * The base type is shared between the versions with and without sky light.
 */
template<typename LightMapPolicyType>
class TBasePassPixelShaderBaseType : public FMeshMaterialPixelShader, public LightMapPolicyType::PixelParametersType
{
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType,UBOOL bEnableSkyLight)
	{
		return LightMapPolicyType::ShouldCache(Platform,Material,VertexFactoryType,bEnableSkyLight);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Initialization constructor. */
	TBasePassPixelShaderBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialPixelShader(Initializer)
	{
		LightMapPolicyType::PixelParametersType::Bind(Initializer.ParameterMap);
		MaterialParameters.Bind(Initializer.ParameterMap);
		AmbientColorAndSkyFactorParameter.Bind(Initializer.ParameterMap,TEXT("AmbientColorAndSkyFactor"),TRUE);
		TemporalAAParameters.Bind(Initializer.ParameterMap,TEXT("TemporalAAParametersPS"),TRUE);
		UpperSkyColorParameter.Bind(Initializer.ParameterMap,TEXT("UpperSkyColor"),TRUE);
		LowerSkyColorParameter.Bind(Initializer.ParameterMap,TEXT("LowerSkyColor"),TRUE);
		DeferredRenderingParameters.Bind(Initializer.ParameterMap,TEXT("DeferredRenderingParameters"),TRUE);
	}
	TBasePassPixelShaderBaseType() {}

	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FMaterial& MaterialResource,const FSceneView* View,UBOOL bDrawLitTranslucencyUnlit)
	{
		VertexFactoryParameters.Set(this, VertexFactory, *View);
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, MaterialResource, View->Family->CurrentWorldTime, View->Family->CurrentRealTime, View);
		MaterialParameters.Set(this,MaterialRenderContext);

		if (AmbientColorAndSkyFactorParameter.IsBound())
		{
			// Draw the surface unlit if it's an unlit view, or it's a lit material without a light-map.
			const UBOOL bIsTranslucentLitMaterial = IsTranslucentBlendMode(MaterialRenderContext.Material.GetBlendMode()) && MaterialRenderContext.Material.GetLightingModel() != MLM_Unlit;
			const UBOOL bIsUnlitView = !(View->Family->ShowFlags & SHOW_Lighting);
			const UBOOL bDrawSurfaceUnlit = bIsUnlitView || (LightMapPolicyType::bDrawLitTranslucencyUnlit && bDrawLitTranslucencyUnlit && bIsTranslucentLitMaterial);
			SetPixelShaderValue(
				GetPixelShader(),
				AmbientColorAndSkyFactorParameter,
				bDrawSurfaceUnlit ? FLinearColor(1,1,1,0) : FLinearColor(0,0,0,1)
				);
		}
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshBatch& Mesh, INT BatchElementIndex,const FSceneView& View,UBOOL bBackFace,EBlendMode BlendMode)
	{
		VertexFactoryParameters.SetMesh(this, Mesh, BatchElementIndex, View);
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace);

#if XBOX
		if (PrimitiveSceneInfo)
		{
			const UBOOL bShouldDisableTemporalAA = !View.bRenderTemporalAA || ( PrimitiveSceneInfo->bMovable && !IsTranslucentBlendMode(BlendMode) );
			SetPixelShaderValue(
				GetPixelShader(),
				TemporalAAParameters,
				FVector2D(View.TemporalAAParameters.StartDepth, bShouldDisableTemporalAA ? 0.0f : 1.0f)
				);
		}
#endif

#if PLATFORM_SUPPORTS_D3D10_PLUS
		if (DeferredRenderingParameters.IsBound() && PrimitiveSceneInfo)
		{
			const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial();
			const UBOOL bSupportsDeferredLighting = MeshSupportsDeferredLighting(Material, PrimitiveSceneInfo);

			SetPixelShaderValue(
				GetPixelShader(),
				DeferredRenderingParameters,
				FVector4(
					bSupportsDeferredLighting ? 1.0f : 0.0f, 
					Material->GetImageReflectionNormalDampening(),
					PrimitiveSceneInfo->LightingChannels.GetDeferredShadingChannelMask()
					)
				);
		}
#endif
	}

	void SetSkyColor(const FLinearColor& UpperSkyColor,const FLinearColor& LowerSkyColor)
	{
		SetPixelShaderValue(GetPixelShader(),UpperSkyColorParameter,UpperSkyColor);
		SetPixelShaderValue(GetPixelShader(),LowerSkyColorParameter,LowerSkyColor);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
#if PS3
		//@hack - compiler bug? optimized version crashes during FShader::Serialize call
		static INT RemoveMe=0;	RemoveMe=1;
#endif
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
		LightMapPolicyType::PixelParametersType::Serialize(Ar);
		Ar << MaterialParameters;
		Ar << AmbientColorAndSkyFactorParameter;
		Ar << TemporalAAParameters;
		Ar << UpperSkyColorParameter;
		Ar << LowerSkyColorParameter;
		Ar << DeferredRenderingParameters;

		// set parameter names for platforms that need them
		UpperSkyColorParameter.SetShaderParamName(TEXT("UpperSkyColor"));
		LowerSkyColorParameter.SetShaderParamName(TEXT("LowerSkyColor"));
		
		return bShaderHasOutdatedParameters;
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialPixelShaderParameters MaterialParameters;
	FShaderParameter AmbientColorAndSkyFactorParameter;
	FShaderParameter TemporalAAParameters;
	FShaderParameter UpperSkyColorParameter;
	FShaderParameter LowerSkyColorParameter;
	FShaderParameter DeferredRenderingParameters;
};

/** The concrete base pass pixel shader type, parameterized by whether sky lighting is needed. */
template<typename LightMapPolicyType,UBOOL bEnableSkyLight>
class TBasePassPixelShader : public TBasePassPixelShaderBaseType<LightMapPolicyType>
{
	DECLARE_SHADER_TYPE(TBasePassPixelShader,MeshMaterial);
public:
	
	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		//don't compile skylight versions if the material is unlit
		const UBOOL bCacheShaders = !bEnableSkyLight || (Material->GetLightingModel() != MLM_Unlit);
		return bCacheShaders && 
			TBasePassPixelShaderBaseType<LightMapPolicyType>::ShouldCache(Platform, Material, VertexFactoryType, bEnableSkyLight);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		TBasePassPixelShaderBaseType<LightMapPolicyType>::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("ENABLE_SKY_LIGHT"),bEnableSkyLight ? TEXT("1") : TEXT("0"));
	}
	
	/** Initialization constructor. */
	TBasePassPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		TBasePassPixelShaderBaseType<LightMapPolicyType>(Initializer)
	{}

	/** Default constructor. */
	TBasePassPixelShader() {}
};

/**
 * Draws the emissive color and the light-map of a mesh.
 */
template<typename LightMapPolicyType,typename FogDensityPolicyType>
class TBasePassDrawingPolicy : public FMeshDrawingPolicy
{
public:

	/** The data the drawing policy uses for each mesh element. */
	class ElementDataType
	{
	public:

		/** The element's light-map data. */
		typename LightMapPolicyType::ElementDataType LightMapElementData;

		/** The element's fog volume data. */
		typename FogDensityPolicyType::ElementDataType FogVolumeElementData;

		/** Default constructor. */
		ElementDataType()
		{}

		/** Initialization constructor. */
		ElementDataType(
			const typename LightMapPolicyType::ElementDataType& InLightMapElementData,
			const typename FogDensityPolicyType::ElementDataType& InFogVolumeElementData
			):
			LightMapElementData(InLightMapElementData),
			FogVolumeElementData(InFogVolumeElementData)
		{}
	};

	/** Initialization constructor. */
	TBasePassDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		LightMapPolicyType InLightMapPolicy,
		EBlendMode InBlendMode,
		UBOOL bInEnableSkyLight,
		UBOOL bOverrideWithShaderComplexity = FALSE,
		UBOOL bInDrawLitTranslucencyUnlit = TRUE,
		UBOOL bInRenderingToLowResTranslucency = FALSE,
		UBOOL bInRenderingToDoFBlurBuffer = FALSE,
		UBOOL bInShouldOverwriteTranslucentAlpha = FALSE,
		UBOOL bInAllowGlobalFog = FALSE
		):
		FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource,bOverrideWithShaderComplexity),
		LightMapPolicy(InLightMapPolicy),
		BlendMode(InBlendMode),
		bEnableSkyLight(bInEnableSkyLight),
		bDrawLitTranslucencyUnlit(bInDrawLitTranslucencyUnlit),
		bRenderingToLowResTranslucency(bInRenderingToLowResTranslucency),
		bRenderingToDoFBlurBuffer(bInRenderingToDoFBlurBuffer),
		bShouldOverwriteTranslucentAlpha(bInShouldOverwriteTranslucentAlpha),
		bAllowGlobalFog(bInAllowGlobalFog)
	{
#if WITH_D3D11_TESSELLATION
		HullShader = NULL;
		DomainShader = NULL;
	
		const EMaterialTessellationMode MaterialTessellationMode = InMaterialResource.GetD3D11TessellationMode();

		if (GRHIShaderPlatform == SP_PCD3D_SM5
			&& InVertexFactory->GetType()->SupportsTessellationShaders() 
			&& MaterialTessellationMode != MTM_NoTessellation)
		{
			// Find the base pass tessellation shaders since the material is tessellated
			HullShader = InMaterialResource.GetShader<TBasePassHullShader<LightMapPolicyType,FogDensityPolicyType> >(VertexFactory->GetType());
			DomainShader = InMaterialResource.GetShader<TBasePassDomainShader<LightMapPolicyType,FogDensityPolicyType> >(VertexFactory->GetType());
		}
#endif
		VertexShader = InMaterialResource.GetShader<TBasePassVertexShader<LightMapPolicyType,FogDensityPolicyType> >(InVertexFactory->GetType());

		// Find the appropriate shaders based on whether sky lighting is needed.
		if (bEnableSkyLight)
		{
			PixelShader = InMaterialResource.GetShader<TBasePassPixelShader<LightMapPolicyType,TRUE> >(InVertexFactory->GetType());
		}
		else
		{
			PixelShader = InMaterialResource.GetShader<TBasePassPixelShader<LightMapPolicyType,FALSE> >(InVertexFactory->GetType());
		}
	}

	// FMeshDrawingPolicy interface.

	UBOOL Matches(const TBasePassDrawingPolicy& Other) const
	{
#if WITH_MOBILE_RHI
		if( GUsingMobileRHI )
		{
			//For mobile use the internally computed material key to get around the "uber-shader" having many different programs behind the scenes
			if ( !FMeshDrawingPolicy::Matches(Other) )
			{
				return FALSE;
			}
			FProgramKey ShaderKeyA = MaterialRenderProxy->GetMaterial()->GetMobileMaterialSortKey();
			FProgramKey ShaderKeyB = Other.MaterialRenderProxy->GetMaterial()->GetMobileMaterialSortKey();
			return (ShaderKeyA == ShaderKeyB);
		}
		else
#endif
		{
			return FMeshDrawingPolicy::Matches(Other) &&
				VertexShader == Other.VertexShader &&
				PixelShader == Other.PixelShader &&
#if WITH_D3D11_TESSELLATION
				HullShader == Other.HullShader &&
				DomainShader == Other.DomainShader &&
#endif
				bDrawLitTranslucencyUnlit == Other.bDrawLitTranslucencyUnlit &&
				bRenderingToLowResTranslucency == Other.bRenderingToLowResTranslucency &&
				bRenderingToDoFBlurBuffer == Other.bRenderingToDoFBlurBuffer &&
				bShouldOverwriteTranslucentAlpha == Other.bShouldOverwriteTranslucentAlpha &&
				bAllowGlobalFog == Other.bAllowGlobalFog &&

				LightMapPolicy == Other.LightMapPolicy;
		}
	}

	void DrawShared(const FSceneView* View,FBoundShaderStateRHIParamRef BoundShaderState) const
	{
		VertexShader->SetParameters(VertexFactory, MaterialRenderProxy, *MaterialResource, *View, bAllowGlobalFog);
#if WITH_D3D11_TESSELLATION
		if (HullShader)
		{
			HullShader->SetParameters(MaterialRenderProxy, *View);
		}
		if (DomainShader)
		{
			DomainShader->SetParameters(VertexFactory, MaterialRenderProxy, *View);
		}
#endif
#if !FINAL_RELEASE
		if (bOverrideWithShaderComplexity)
		{
			// If we are in the translucent pass or rendering a masked material then override the blend mode, otherwise maintain opaque blending
			if (BlendMode != BLEND_Opaque)
			{
				// Add complexity to existing
				RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());
			}

			TShaderMapRef<FShaderComplexityAccumulatePixelShader> ShaderComplexityPixelShader(GetGlobalShaderMap());
			const UINT NumPixelShaderInstructions = bRenderingToLowResTranslucency ? 
				// Reduce the instruction count by the resolution factor used with downsampled translucency
				//@todo - would be nice to factor in the constant composite overhead somehow
				PixelShader->GetNumInstructions() / Square(GSceneRenderTargets.GetSmallColorDepthDownsampleFactor()) :
				PixelShader->GetNumInstructions();

			const UINT NumVertexShaderInstructions = VertexShader->GetNumInstructions();
			ShaderComplexityPixelShader->SetParameters(NumVertexShaderInstructions,NumPixelShaderInstructions);
		}
		else
#endif
		{
			PixelShader->SetParameters(VertexFactory,MaterialRenderProxy,*MaterialResource,View,bDrawLitTranslucencyUnlit);

			EBlendMode EffectiveBlendMode = BlendMode;
			// Use an opaque blend mode with one layer distortion, blending will be done manually in the shader
			if (IsTranslucentBlendMode(BlendMode) && MaterialRenderProxy->GetMaterial()->UsesOneLayerDistortion())
			{
				EffectiveBlendMode = BLEND_Opaque;
			}

			switch(EffectiveBlendMode)
			{
			default:
			case BLEND_Opaque:
				// Opaque materials are rendered together in the base pass, where the blend state is set at a higher level
#if WITH_MOBILE_RHI
				// mobile needs to reset the blend state to not leave masked enabled for opaque
				if( GUsingMobileRHI )
				{
					RHISetBlendState(TStaticBlendState<>::GetRHI());
				}
#else
				//In case this is transparent and using one layer distortion or some other transformation that modifies BlendMode
				if (EffectiveBlendMode != BlendMode)
				{
					RHISetBlendState(TStaticBlendState<>::GetRHI());
				}
#endif
				break;
			case BLEND_DitheredTranslucent:
			case BLEND_Masked:
#if WITH_MOBILE_RHI
				if( GUsingMobileRHI )
				{
					// if we are using simplified, flattened materials, we won't know if masking is enabled, so enable alphatest with a default value of .33
					RHISetBlendState(TStaticBlendState<BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero, CF_Greater, 255/3>::GetRHI());
				}
				else
				{
					RHISetBlendState(TStaticBlendState<>::GetRHI());
				}
#endif
				// Masked materials are rendered together in the base pass, where the blend state is set at a higher level
				break;
			case BLEND_SoftMasked:
				RHISetBlendState(TStaticBlendState<BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_Zero,BF_One>::GetRHI());
				break;
			case BLEND_Translucent:
				{
					UBOOL bInvOpacityInAlpha = bRenderingToLowResTranslucency;
#if !CONSOLE
					if(GSystemSettings.bAllowSeparateTranslucency)
					{
						bInvOpacityInAlpha = TRUE;
					}
#endif
					RHISetBlendState(
						bInvOpacityInAlpha ?
							// Accumulate added color in rgb, accumulate inverse opacity in alpha.
							TStaticBlendState<BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_Zero,BF_InverseSourceAlpha>::GetRHI() :
						!bShouldOverwriteTranslucentAlpha ?
							// Blend with the existing scene color, preserve destination alpha.
							TStaticBlendState<BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_Zero,BF_One>::GetRHI() :
							// Blend with the existing scene color, overwriting destination alpha.
							TStaticBlendState<BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_One,BF_Zero>::GetRHI()
						);
#if XBOX
					if(bRenderingToDoFBlurBuffer)
					{
						RHISetMRTBlendState(TStaticBlendState<BO_Max,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI(),1);
					}
#endif
				}
				break;
			case BLEND_Additive:
				if(GSystemSettings.bAllowSeparateTranslucency)
				{
					RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_InverseSourceAlpha>::GetRHI());
				}
				else
				{
					// Add to the existing scene color, preserve destination alpha.
					RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());
#if XBOX
					if(bRenderingToDoFBlurBuffer)
					{
						RHISetMRTBlendState(TStaticBlendState<BO_Max,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI(),1);
					}
#endif
				}
				break;
			case BLEND_Modulate:
				RHISetBlendState(
					// Modulate with the existing scene color, preserve destination alpha.
					TStaticBlendState<BO_Add,BF_DestColor,BF_Zero,BO_Add,BF_Zero,BF_One>::GetRHI()
					);
#if XBOX
				if(bRenderingToDoFBlurBuffer)
				{
					RHISetMRTBlendState(TStaticBlendState<BO_Max,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI(),1);
				}
#endif
				break;
			case BLEND_ModulateAndAdd:
				RHISetBlendState( TStaticBlendState<BO_Add, BF_DestColor, BF_One>::GetRHI() );
				break;
            case BLEND_AlphaComposite:
                // Blend with existing scene color. New color is premultiplied by alpha.
                RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_InverseSourceAlpha,BO_Add,BF_One,BF_InverseSourceAlpha>::GetRHI());
                break;
			};
		}

		// Set the light-map policy.
		LightMapPolicy.Set(VertexShader,bOverrideWithShaderComplexity ? NULL : PixelShader,VertexShader,PixelShader,VertexFactory,MaterialRenderProxy,View);

		// Set the actual shader & vertex declaration state
		RHISetBoundShaderState( BoundShaderState);
	}

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
	* @return new bound shader state object
	*/
	FBoundShaderStateRHIRef CreateBoundShaderState(DWORD DynamicStride = 0)
	{
		FVertexDeclarationRHIParamRef VertexDeclaration;
		DWORD StreamStrides[MaxVertexElementCount];

		LightMapPolicy.GetVertexDeclarationInfo(VertexDeclaration, StreamStrides, VertexFactory);
		if (DynamicStride)
		{
			StreamStrides[0] = DynamicStride;
		}

		FPixelShaderRHIParamRef PixelShaderRHIRef = PixelShader->GetPixelShader();

#if !FINAL_RELEASE
		if (bOverrideWithShaderComplexity)
		{
			TShaderMapRef<FShaderComplexityAccumulatePixelShader> ShaderComplexityAccumulatePixelShader(GetGlobalShaderMap());
			PixelShaderRHIRef = ShaderComplexityAccumulatePixelShader->GetPixelShader();
		}
#endif
		FBoundShaderStateRHIRef BoundShaderState;

#if WITH_D3D11_TESSELLATION
		BoundShaderState = RHICreateBoundShaderStateD3D11(
			VertexDeclaration, 
			StreamStrides, 
			VertexShader->GetVertexShader(),
			GETSAFERHISHADER_HULL(HullShader), 
			GETSAFERHISHADER_DOMAIN(DomainShader), 
			PixelShaderRHIRef,
			FGeometryShaderRHIRef(),
			EGST_None);
#else
			BoundShaderState = RHICreateBoundShaderState(VertexDeclaration, StreamStrides, VertexShader->GetVertexShader(), PixelShaderRHIRef,EGST_None);
#endif

		return BoundShaderState;
	}

	void SetMeshRenderState(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		UBOOL bBackFace,
		const ElementDataType& ElementData
		) const
	{
		// Set the fog volume parameters.
		VertexShader->SetFogVolumeParameters(VertexFactory,MaterialRenderProxy,View,ElementData.FogVolumeElementData);
		VertexShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);

		// Set the light-map policy's mesh-specific settings.
		LightMapPolicy.SetMesh(
			View,
			PrimitiveSceneInfo,
			VertexShader,
			bOverrideWithShaderComplexity ? NULL : PixelShader,
			VertexShader,
			PixelShader,
			VertexFactory,
			MaterialRenderProxy,
			ElementData.LightMapElementData);

#if WITH_D3D11_TESSELLATION
		if(HullShader && DomainShader)
		{
			// Set the fog volume parameters.
			HullShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
			DomainShader->SetFogVolumeParameters(VertexFactory,MaterialRenderProxy,View,ElementData.FogVolumeElementData);
			DomainShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
		}
#endif

#if !FINAL_RELEASE
		//don't set the draw policies' pixel shader parameters if the shader complexity viewmode is enabled
		//since they will overwrite the FShaderComplexityAccumulatePixelShader parameters
		if(!bOverrideWithShaderComplexity)
#endif
		{
			if(bEnableSkyLight)
			{
				FLinearColor UpperSkyLightColor = FLinearColor::Black;
				FLinearColor LowerSkyLightColor = FLinearColor::Black;
				if(PrimitiveSceneInfo)
				{
					UpperSkyLightColor = PrimitiveSceneInfo->UpperSkyLightColor;
					if (GIsEditor 
						&& LightMapPolicyType::bAllowPreviewSkyLight 
						&& PrimitiveSceneInfo->bAcceptsLights 
						&& PrimitiveSceneInfo->bStaticShadowing)
					{
						// Add the scene's preview sky color for primitives that should be lightmapped but are using FNoLightMapPolicy
						UpperSkyLightColor += PrimitiveSceneInfo->GetPreviewSkyLightColor();
					}
					LowerSkyLightColor = PrimitiveSceneInfo->LowerSkyLightColor;
				}
				PixelShader->SetSkyColor(UpperSkyLightColor,LowerSkyLightColor);
			}

			PixelShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace,BlendMode);
		}

		FMeshDrawingPolicy::SetMeshRenderState(View,PrimitiveSceneInfo, Mesh, BatchElementIndex, bBackFace,FMeshDrawingPolicy::ElementDataType());
	}

	friend INT Compare(const TBasePassDrawingPolicy& A,const TBasePassDrawingPolicy& B)
	{
#if WITH_MOBILE_RHI
		if( GUsingMobileRHI )
		{
			//For mobile use the internally computed material key to get around the "uber-shader" having many different programs behind the scenes
			//const UTexture2D* SimpleLightMapA = A.ElementDataType.LightMapElementData.GetTexture(0);
			FProgramKey ShaderKeyA = A.MaterialRenderProxy->GetMaterial()->GetMobileMaterialSortKey();
			FProgramKey ShaderKeyB = B.MaterialRenderProxy->GetMaterial()->GetMobileMaterialSortKey();
			if(ShaderKeyA < ShaderKeyB) 
			{ 
				return -1; 
			} 
			else if(ShaderKeyA > ShaderKeyB) 
			{ 
				return +1; 
			}
			COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
			return 0;
		}
		else
#endif
		{
			COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
			COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
#if WITH_D3D11_TESSELLATION
			COMPAREDRAWINGPOLICYMEMBERS(HullShader);
			COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
#endif
			COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
			COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);

			COMPAREDRAWINGPOLICYMEMBERS(bDrawLitTranslucencyUnlit);
			COMPAREDRAWINGPOLICYMEMBERS(bRenderingToLowResTranslucency);
			COMPAREDRAWINGPOLICYMEMBERS(bRenderingToDoFBlurBuffer);
			COMPAREDRAWINGPOLICYMEMBERS(bShouldOverwriteTranslucentAlpha);
			COMPAREDRAWINGPOLICYMEMBERS(bAllowGlobalFog);

			return Compare(A.LightMapPolicy,B.LightMapPolicy);
		}
	}

protected:
	TBasePassVertexShader<LightMapPolicyType,FogDensityPolicyType>* VertexShader;

#if WITH_D3D11_TESSELLATION
	TBasePassHullShader<LightMapPolicyType,FogDensityPolicyType>* HullShader;
	TBasePassDomainShader<LightMapPolicyType,FogDensityPolicyType>* DomainShader;
#endif

	TBasePassPixelShaderBaseType<LightMapPolicyType>* PixelShader;

	LightMapPolicyType LightMapPolicy;
	EBlendMode BlendMode;

	BITFIELD bEnableSkyLight : 1;
	BITFIELD bDrawLitTranslucencyUnlit : 1;
	BITFIELD bRenderingToLowResTranslucency : 1;
	BITFIELD bRenderingToDoFBlurBuffer : 1;
	BITFIELD bShouldOverwriteTranslucentAlpha : 1;
	BITFIELD bAllowGlobalFog : 1;

	friend class FDrawTranslucentMeshAction;
};

/**
 * A drawing policy factory for the base pass drawing policy.
 */
class FBasePassOpaqueDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = TRUE };
	struct ContextType {};

	static void AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh,ContextType DrawingContext = ContextType());
	static UBOOL DrawDynamicMesh(
		const FSceneView& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);
	static UBOOL IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy)
	{
		// Ignore non-opaque materials in the opaque base pass.
		return MaterialRenderProxy && IsTranslucentBlendMode(MaterialRenderProxy->GetMaterial()->GetBlendMode());
	}
};

/** The parameters used to process a base pass mesh. */
class FProcessBasePassMeshParameters
{
public:

	const FMeshBatch& Mesh;
	const FMaterial* Material;
	const FPrimitiveSceneInfo* PrimitiveSceneInfo;
	EBlendMode BlendMode;
	EMaterialLightingModel LightingModel;
	const UBOOL bAllowFog;

	/** Initialization constructor. */
	FProcessBasePassMeshParameters(
		const FMeshBatch& InMesh,
		const FMaterial* InMaterial,
		const FPrimitiveSceneInfo* InPrimitiveSceneInfo,
		UBOOL InbAllowFog
		):
		Mesh(InMesh),
		Material(InMaterial),
		PrimitiveSceneInfo(InPrimitiveSceneInfo),
		BlendMode(InMaterial->GetBlendMode()),
		LightingModel(InMaterial->GetLightingModel()),
		bAllowFog(InbAllowFog)
	{
	}
};

#if 1
/** The parameters used to draw a translucent mesh. */
class FDrawTranslucentMeshAction
{
public:

	const FViewInfo& View;
	const FProjectedShadowInfo* TranslucentPreShadowInfo;
	const FLightSceneInfo* MergedDynamicLight;
	const FSHVectorRGB& CompositedDynamicLighting;
	UBOOL bUseCompositedTranslucentLighting;
	UBOOL bBackFace;
	UBOOL bDrawLitTranslucencyUnlit;
	UBOOL bRenderingToLowResTranslucencyBuffer;
	UBOOL bRenderingToDoFBlurBuffer;
	FHitProxyId HitProxyId;

	/** Initialization constructor. */
	FDrawTranslucentMeshAction(
		const FViewInfo& InView,
		const FProjectedShadowInfo* InTranslucentPreShadowInfo,
		const FLightSceneInfo* InMergedDynamicLight,
		const FSHVectorRGB& InCompositedDynamicLighting,
		UBOOL bInUseCompositedTranslucentLighting,
		UBOOL bInBackFace,
		FHitProxyId InHitProxyId,
		UBOOL bInDrawLitTranslucencyUnlit,
		UBOOL bInIsRenderingToLowResTranslucencyBuffer,
		UBOOL bInIsRenderingToDoFBlurBuffer
		):
	View(InView),
		TranslucentPreShadowInfo(InTranslucentPreShadowInfo),
		MergedDynamicLight(InMergedDynamicLight),
		CompositedDynamicLighting(InCompositedDynamicLighting),
		bUseCompositedTranslucentLighting(bInUseCompositedTranslucentLighting),
		bBackFace(bInBackFace),
		bDrawLitTranslucencyUnlit(bInDrawLitTranslucencyUnlit),
		bRenderingToLowResTranslucencyBuffer(bInIsRenderingToLowResTranslucencyBuffer),
		bRenderingToDoFBlurBuffer(bInIsRenderingToDoFBlurBuffer),
		HitProxyId(InHitProxyId)
	{}

	ESceneDepthPriorityGroup GetDPG(const FProcessBasePassMeshParameters& Parameters) const
	{
		return (ESceneDepthPriorityGroup)Parameters.PrimitiveSceneInfo->Proxy->GetDepthPriorityGroup((const FSceneView*)&View);
	}

	UBOOL ShouldReceiveDominantShadows(const FProcessBasePassMeshParameters& Parameters) const
	{
		return Parameters.PrimitiveSceneInfo->bAcceptsDynamicDominantLightShadows 
			// Only receive shadows if this translucent mesh has a preshadow
			// Or the mesh is not allowed to receive dynamic shadows and it should be completely shadowed.
			&& (TranslucentPreShadowInfo != NULL || !Parameters.PrimitiveSceneInfo->bAllowDynamicShadowsOnTranslucency && Parameters.PrimitiveSceneInfo->bTranslucencyShadowed)
			|| UseTranslucencyLightAttenuation(Parameters);
	}

	UBOOL ShouldOverrideDynamicShadowsOnTranslucency(const FProcessBasePassMeshParameters& Parameters) const
	{
		// Only override if the mesh is not allowed to receive dynamic shadows and it should be completely shadowed
		return Parameters.PrimitiveSceneInfo && !Parameters.PrimitiveSceneInfo->bAllowDynamicShadowsOnTranslucency && Parameters.PrimitiveSceneInfo->bTranslucencyShadowed;
	}

	UBOOL UseTranslucencyLightAttenuation(const FProcessBasePassMeshParameters& Parameters) const
	{
		return GSceneRenderTargets.bResolvedTranslucencyDominantLightAttenuationTexture 
			&& Parameters.Material->TranslucencyInheritDominantShadowsFromOpaque();
	}

	const FLightSceneInfo* GetTranslucencyMergedDynamicLightInfo() const
	{
		return MergedDynamicLight;
	}

	const FSHVectorRGB* GetTranslucencyCompositedDynamicLighting() const 
	{ 
		return bUseCompositedTranslucentLighting ? &CompositedDynamicLighting : NULL; 
	}

	const FProjectedShadowInfo* GetTranslucentPreShadow() const 
	{ 
		return TranslucentPreShadowInfo; 
	}

	/** Draws the translucent mesh with a specific light-map type, and fog volume type */
	template<typename LightMapPolicyType,typename FogDensityPolicyType>
	void Process(
		const FProcessBasePassMeshParameters& Parameters,
		const LightMapPolicyType& LightMapPolicy,
		const typename LightMapPolicyType::ElementDataType& LightMapElementData,
		const typename FogDensityPolicyType::ElementDataType& FogDensityElementData
		) const;
};
#endif

/** Processes a base pass mesh using a known light map policy, and unknown fog density policy. */
template<typename ProcessActionType,typename LightMapPolicyType>
void ProcessBasePassMesh_LightMapped(
	const FProcessBasePassMeshParameters& Parameters,
	const ProcessActionType& Action,
	const LightMapPolicyType& LightMapPolicy,
	const typename LightMapPolicyType::ElementDataType& LightMapElementData
	)
{
	// Don't render fog on opaque or modulated materials and GPU skinned meshes.
	const UBOOL bDisableFog =
		!Parameters.bAllowFog ||
		!IsTranslucentBlendMode(Parameters.BlendMode) ||
		(Parameters.BlendMode == BLEND_Modulate) ||
		(Parameters.BlendMode == BLEND_ModulateAndAdd) ||
		Parameters.Mesh.VertexFactory->IsGPUSkinned() ||
		!Parameters.Material->AllowsFog() ||
		// Fog volume policies don't compile for decals so force fog volumes to be disables
		Parameters.Material->IsUsedWithDecals();

	// Determine the density function of the fog volume the primitive is in.
	const EFogVolumeDensityFunction FogVolumeDensityFunction = 
		!bDisableFog && Parameters.PrimitiveSceneInfo && Parameters.PrimitiveSceneInfo->FogVolumeSceneInfo ?
			Parameters.PrimitiveSceneInfo->FogVolumeSceneInfo->GetDensityFunctionType() :
			FVDF_None;

	// Define a macro to handle a specific case of fog volume density function.
	#define HANDLE_FOG_VOLUME_DENSITY_FUNCTION(FogDensityPolicyType,FogDensityElementData) \
		case FogDensityPolicyType::DensityFunctionType: \
			Action.template Process<LightMapPolicyType,FogDensityPolicyType>(Parameters,LightMapPolicy,LightMapElementData,FogDensityElementData); \
			break;

	// Call Action.Process with the appropriate fog volume density policy type.
	switch(FogVolumeDensityFunction)
	{
		HANDLE_FOG_VOLUME_DENSITY_FUNCTION(FConstantDensityPolicy,Parameters.PrimitiveSceneInfo->FogVolumeSceneInfo);
		HANDLE_FOG_VOLUME_DENSITY_FUNCTION(FLinearHalfspaceDensityPolicy,Parameters.PrimitiveSceneInfo->FogVolumeSceneInfo);
		HANDLE_FOG_VOLUME_DENSITY_FUNCTION(FSphereDensityPolicy,Parameters.PrimitiveSceneInfo->FogVolumeSceneInfo);
		HANDLE_FOG_VOLUME_DENSITY_FUNCTION(FConeDensityPolicy,Parameters.PrimitiveSceneInfo->FogVolumeSceneInfo);
		default:
		HANDLE_FOG_VOLUME_DENSITY_FUNCTION(FNoDensityPolicy,FNoDensityPolicy::ElementDataType());
	};

	#undef HANDLE_FOG_VOLUME_DENSITY_FUNCTION
}

/** Processes a base pass mesh using an unknown light map policy, and unknown fog density policy. */
template<typename ProcessActionType>
void ProcessBasePassMesh(
	const FProcessBasePassMeshParameters& Parameters,
	const ProcessActionType& Action
	)
{
	// Check for a cached light-map.
	const UBOOL bIsLitMaterial = Parameters.LightingModel != MLM_Unlit;
	const FLightMapInteraction LightMapInteraction = (Parameters.Mesh.LCI && bIsLitMaterial) ? Parameters.Mesh.LCI->GetLightMapInteraction() : FLightMapInteraction();

	UBOOL bShouldRenderDominantLight = FALSE;
	FLightInteraction DominantLightInteraction = FLightInteraction::Uncached();
	// If we're doing a one pass dominant dynamic light, check if this primitive has a dominant light associated with it
	if (GOnePassDominantLight
		&& bIsLitMaterial 
		&& Parameters.Mesh.LCI 
		&& Parameters.PrimitiveSceneInfo 
		&& Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo
		&& IsDominantLightType(Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo->LightType)
		&& LightMapInteraction.GetType() != LMIT_None)
	{
		bShouldRenderDominantLight = TRUE;
		// Check if the primitive has a cached interaction with the dominant light
		DominantLightInteraction = Parameters.Mesh.LCI->GetInteraction(Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo);
	}
	const ELightInteractionType DominantLightInteractionType = DominantLightInteraction.GetType();

	// force simple lightmaps based on system settings
	const UBOOL bAllowDirectionalLightMaps = GSystemSettings.bAllowDirectionalLightMaps && LightMapInteraction.AllowsDirectionalLightmaps();
	const UBOOL bReceiveDynamicShadows = Parameters.PrimitiveSceneInfo ? 
		Action.ShouldReceiveDominantShadows(Parameters) : 
		FALSE;
	const UBOOL bOverrideDynamicShadowsOnTranslucency = Action.ShouldOverrideDynamicShadowsOnTranslucency(Parameters);
	const UBOOL bUseTranslucencyLightAttenuation = Action.UseTranslucencyLightAttenuation(Parameters);

	if (DominantLightInteractionType == LIT_CachedShadowMap2D 
		// Handle a dominant light without static shadowing with texture lightmaps
		|| (bShouldRenderDominantLight && DominantLightInteractionType == LIT_Uncached && LightMapInteraction.GetType() == LMIT_Texture))
	{
		checkSlow(bIsLitMaterial && Parameters.Mesh.LCI && Parameters.PrimitiveSceneInfo && Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo);
		// Can't mix texture shadow maps with vertex lightmaps
		checkSlow(LightMapInteraction.GetType() == LMIT_Texture);
		checkSlow(bAllowDirectionalLightMaps);
		// Use a white shadow texture if the dominant light isn't using static shadowing
		// This is done to avoid adding shader combinations to handle a dominant light without static shadowing with lightmaps
		FTexture* ShadowTexture = GWhiteTexture;
		FVector2D CoordinateScale(1,1);
		FVector2D CoordinateBias(0,0);
		if (DominantLightInteractionType == LIT_CachedShadowMap2D)
		{
			ShadowTexture = DominantLightInteraction.GetShadowTexture()->Resource;
			CoordinateScale = DominantLightInteraction.GetShadowCoordinateScale();
			CoordinateBias = DominantLightInteraction.GetShadowCoordinateBias();
		}
		// Render the mesh with a texture shadow mapped directional light and texture lightmaps
		ProcessBasePassMesh_LightMapped<ProcessActionType,FShadowedDynamicLightDirectionalLightMapTexturePolicy>(
			Parameters,
			Action,
			FShadowedDynamicLightDirectionalLightMapTexturePolicy(Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo, bUseTranslucencyLightAttenuation),
			FShadowedDynamicLightDirectionalLightMapTexturePolicy::ElementDataType(
				FTextureShadowedDynamicLightLightMapPolicy::ElementDataType(
					ShadowTexture,
					LightMapInteraction, 
					CoordinateScale, 
					CoordinateBias, 
					FDynamicallyShadowedMultiTypeLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow())), 
				LightMapInteraction)); 
	}
	else if (DominantLightInteractionType == LIT_CachedSignedDistanceFieldShadowMap2D)
	{
		checkSlow(bIsLitMaterial && Parameters.Mesh.LCI && Parameters.PrimitiveSceneInfo && Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo);
		// Can't mix texture shadow maps with vertex lightmaps
		checkSlow(LightMapInteraction.GetType() == LMIT_Texture);
		checkSlow(bAllowDirectionalLightMaps);
		// Render the mesh with a texture signed distance field shadow mapped directional light and texture lightmaps
		ProcessBasePassMesh_LightMapped<ProcessActionType,FDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicy>(
			Parameters,
			Action,
			FDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicy(Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo, bUseTranslucencyLightAttenuation),
			FDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicy::ElementDataType(
				FTextureDistanceFieldShadowedDynamicLightLightMapPolicy::ElementDataType(
					DominantLightInteraction.GetShadowTexture()->Resource,
					LightMapInteraction, 
					DominantLightInteraction.GetShadowCoordinateScale(), 
					DominantLightInteraction.GetShadowCoordinateBias(), 
					Parameters.Mesh.MaterialRenderProxy->GetDistanceFieldPenumbraScale(),
					FDynamicallyShadowedMultiTypeLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow()),
					Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo), 
				LightMapInteraction)); 
	}
	else if (DominantLightInteractionType == LIT_CachedShadowMap1D
		// Handle a dominant light without static shadowing with vertex lightmaps
		|| (bShouldRenderDominantLight && DominantLightInteractionType == LIT_Uncached && LightMapInteraction.GetType() == LMIT_Vertex))
	{
		checkSlow(bIsLitMaterial && Parameters.Mesh.LCI && Parameters.PrimitiveSceneInfo && Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo);
		// Can't mix vertex shadow maps with texture lightmaps
		checkSlow(LightMapInteraction.GetType() == LMIT_Vertex);
		checkSlow(bAllowDirectionalLightMaps);
		// Use a white shadow vertex buffer if the dominant light isn't using static shadowing
		// This is done to avoid adding shader combinations to handle a dominant light without static shadowing with lightmaps
		const FVertexBuffer* ShadowBuffer = DominantLightInteractionType == LIT_CachedShadowMap1D ? DominantLightInteraction.GetShadowVertexBuffer() : &GNullShadowmapVertexBuffer;
		// Render the mesh with a vertex shadow mapped directional light and vertex lightmaps
		ProcessBasePassMesh_LightMapped<ProcessActionType,FShadowedDynamicLightDirectionalVertexLightMapPolicy>(
			Parameters,
			Action,
			FShadowedDynamicLightDirectionalVertexLightMapPolicy(ShadowBuffer, Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo, bUseTranslucencyLightAttenuation),
			FShadowedDynamicLightDirectionalVertexLightMapPolicy::ElementDataType(
				FDynamicallyShadowedMultiTypeLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow()), 
				LightMapInteraction)); 
	}
	else
	{
		// Define a macro to handle a specific case of light-map type.
		#define HANDLE_LIGHTMAP_TYPE(LightMapInteractionType,DirectionalLightMapPolicyType,SimpleLightMapPolicyType,LightMapPolicyParameters,LightMapElementData) \
			case LightMapInteractionType: \
				if( bAllowDirectionalLightMaps ) \
				{ \
					ProcessBasePassMesh_LightMapped<ProcessActionType,DirectionalLightMapPolicyType>(Parameters,Action,DirectionalLightMapPolicyType LightMapPolicyParameters,LightMapElementData); \
				} \
				else \
				{ \
					ProcessBasePassMesh_LightMapped<ProcessActionType,SimpleLightMapPolicyType>(Parameters,Action,SimpleLightMapPolicyType LightMapPolicyParameters,LightMapElementData); \
				} \
				break;

		switch(LightMapInteraction.GetType())
		{
			HANDLE_LIGHTMAP_TYPE(LMIT_Vertex,FDirectionalVertexLightMapPolicy,FSimpleVertexLightMapPolicy,(),LightMapInteraction);
			HANDLE_LIGHTMAP_TYPE(LMIT_Texture,FDirectionalLightMapTexturePolicy,FSimpleLightMapTexturePolicy,(),LightMapInteraction);
			default:
				{
					// Check if we should use a directional light in the base pass
					if (bIsLitMaterial 
						&& Parameters.PrimitiveSceneInfo 
						// Shaders not compiled with decal usage due to not enough constant registers
						&& !Parameters.Material->IsUsedWithDecals())
					{
						const FSHVectorRGB* TranslucencyMergedLighting = Action.GetTranslucencyCompositedDynamicLighting();
						// If this element is doing approximate one pass lighting for translucency, use a lightmap policy that supports this
						// Note that Action.GetTranslucencyMergedDynamicLightInfo() can still be NULL if no directional, spot or point light was found affecting the translucency
						if (TranslucencyMergedLighting)
						{
							ProcessBasePassMesh_LightMapped<ProcessActionType, FSHLightLightMapPolicy>(Parameters, Action, 
								FSHLightLightMapPolicy(), 
								FSHLightLightMapPolicy::ElementDataType(
									*TranslucencyMergedLighting,
									FDirectionalLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow(), Action.GetTranslucencyMergedDynamicLightInfo())));
						}
						else if (Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo)
						{
							// Check if we should use a dynamically shadowed dynamic light in the base pass
							if (GOnePassDominantLight && IsDominantLightType(Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo->LightType))
							{
								// No need to check PrimitiveSceneInfo->bRenderSHLightInBasePass, 
								// When combined with a dominant light the SH light can always be merged into the base pass. 
								if (Parameters.PrimitiveSceneInfo->SHLightSceneInfo)
								{
									// Render the SH light in the base pass instead of as a separate pass, along with a dynamically shadowed dynamic light
									ProcessBasePassMesh_LightMapped<ProcessActionType, FSHLightAndMultiTypeLightMapPolicy>(Parameters, Action, 
										FSHLightAndMultiTypeLightMapPolicy(Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo, bUseTranslucencyLightAttenuation), 
										FSHLightAndMultiTypeLightMapPolicy::ElementDataType(
											Parameters.PrimitiveSceneInfo,
											FDynamicallyShadowedMultiTypeLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow())));
								}
								else
								{
									// Render just a dynamically shadowed light in the base pass
									ProcessBasePassMesh_LightMapped<ProcessActionType, FDynamicallyShadowedMultiTypeLightLightMapPolicy>(Parameters, Action, 
										FDynamicallyShadowedMultiTypeLightLightMapPolicy(Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo, bUseTranslucencyLightAttenuation), 
										FDynamicallyShadowedMultiTypeLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow()));
								}
							}
							else
							{
								// Using an unshadowed directional light
								// Check if we should also use a spherical harmonic light in the base pass
								if (Parameters.PrimitiveSceneInfo->bRenderSHLightInBasePass 
									// Also use an SH light in the base pass if one is affecting this primitive, the primitive is in the foreground DPG for this view and foreground self-shadowing is disabled.
									// There will be no modulated shadow between the base pass and SH light pass in this case so it is more efficient to merge them together.
									|| (Parameters.PrimitiveSceneInfo->SHLightSceneInfo && !GSystemSettings.bEnableForegroundSelfShadowing && Action.GetDPG(Parameters) == SDPG_Foreground)) 
								{
									ProcessBasePassMesh_LightMapped<ProcessActionType, FSHLightLightMapPolicy>(Parameters, Action, 
										FSHLightLightMapPolicy(), 
										FSHLightLightMapPolicy::ElementDataType(
											*Parameters.PrimitiveSceneInfo->SHLightSceneInfo->GetSHIncidentLighting(),
											FDirectionalLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow(), Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo)));
								}
								else
								{
									ProcessBasePassMesh_LightMapped<ProcessActionType, FDirectionalLightLightMapPolicy>(Parameters, Action, 
										FDirectionalLightLightMapPolicy(), 
										FDirectionalLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow(), Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo));
								}
							}
						}
						else
						{
							ProcessBasePassMesh_LightMapped<ProcessActionType, FNoLightMapPolicy>(Parameters,Action,FNoLightMapPolicy(),FNoLightMapPolicy::ElementDataType());
						}
					}
					else
					{
						ProcessBasePassMesh_LightMapped<ProcessActionType, FNoLightMapPolicy>(Parameters,Action,FNoLightMapPolicy(),FNoLightMapPolicy::ElementDataType());
					}
				}
				break;
		};
	}

	#undef HANDLE_LIGHTMAP_TYPE
}
