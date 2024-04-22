/*=============================================================================
	LightRendering.h: Light rendering definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_LIGHTRENDERING
#define _INC_LIGHTRENDERING

extern UBOOL GVisualizeMipLevels;

/**
 */
class FNullLightShaderComponent
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{}
	void Serialize(FArchive& Ar)
	{}
};

/**
 * A shadowing policy for TMeshLightingDrawingPolicy which uses no static shadowing.
 */
class FNoStaticShadowingPolicy
{
public:

	typedef FNullLightShaderComponent VertexParametersType;
	typedef FNullLightShaderComponent PixelParametersType;
	typedef FMeshDrawingPolicy::ElementDataType ElementDataType;

	static UBOOL ShouldCache(
		EShaderPlatform Platform,
		const FMaterial* Material,
		const FVertexFactoryType* VertexFactoryType,
		UBOOL bLightRequiresStaticLightingShaders
		)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	void Set(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView* View
		) const
	{
		check(VertexFactory);
		VertexFactory->Set();
	}

	/**
	* Get the decl and stream strides for this policy type and vertexfactory
	* @param VertexDeclaration - output decl 
	* @param StreamStrides - output array of vertex stream strides 
	* @param VertexFactory - factory to be used by this policy
	*/
	void GetVertexDeclarationInfo(FVertexDeclarationRHIParamRef &VertexDeclaration, DWORD *StreamStrides, const FVertexFactory* VertexFactory)
	{
		check(VertexFactory);
		VertexFactory->GetStreamStrides(StreamStrides);
		VertexDeclaration = VertexFactory->GetDeclaration();
	}

	void SetMesh(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		const ElementDataType& ElementData
		) const
	{}
	friend UBOOL operator==(const FNoStaticShadowingPolicy A,const FNoStaticShadowingPolicy B)
	{
		return TRUE;
	}
	friend INT Compare(const FNoStaticShadowingPolicy&,const FNoStaticShadowingPolicy&)
	{
		return 0;
	}
};

/**
 * A shadowing policy for TMeshLightingDrawingPolicy which uses a 2D shadow texture.
 */
class FShadowTexturePolicy
{
public:

	class VertexParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			ShadowCoordinateScaleBiasParameter.Bind(ParameterMap,TEXT("LightmapCoordinateScaleBias"),TRUE);
		}
		void SetCoordinateTransform(FShader* VertexShader,const FVector2D& ShadowCoordinateScale,const FVector2D& ShadowCoordinateBias) const
		{
			SetVertexShaderValue(VertexShader->GetVertexShader(),ShadowCoordinateScaleBiasParameter,FVector4(
				ShadowCoordinateScale.X,
				ShadowCoordinateScale.Y,
				ShadowCoordinateBias.Y,
				ShadowCoordinateBias.X
				));
		}
		void Serialize(FArchive& Ar)
		{
			Ar << ShadowCoordinateScaleBiasParameter;
			
			// set parameter names for platforms that need them
			ShadowCoordinateScaleBiasParameter.SetShaderParamName(TEXT("LightmapCoordinateScaleBias"));
		}
	private:
		FShaderParameter ShadowCoordinateScaleBiasParameter;
	};

	class PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			ShadowTextureParameter.Bind(ParameterMap,TEXT("ShadowTexture"),TRUE);
		}
		void SetShadowTexture(FShader* PixelShader,const FTexture* ShadowTexture) const
		{
			UBOOL bShowMipLevels = FALSE;
#if !FINAL_RELEASE
			bShowMipLevels = GVisualizeMipLevels;
#endif
			FLOAT MipBias = ShadowTexture->MipBiasFade.CalcMipBias();
			if ( MipBias != 0.0f )
			{
				INT q=0;
			}
			SetTextureParameter(PixelShader->GetPixelShader(),ShadowTextureParameter, bShowMipLevels ? GBlackTexture : ShadowTexture, 0, MipBias);
		}
		void Serialize(FArchive& Ar)
		{
			Ar << ShadowTextureParameter;
		}
	private:
		FShaderResourceParameter ShadowTextureParameter;
	};

	struct ElementDataType : public FMeshDrawingPolicy::ElementDataType
	{
		FVector2D ShadowCoordinateScale;
		FVector2D ShadowCoordinateBias;

		/** Default constructor. */
		ElementDataType() {}

		/** Initialization constructor. */
		ElementDataType(const FVector2D& InShadowCoordinateScale,const FVector2D& InShadowCoordinateBias):
			ShadowCoordinateScale(InShadowCoordinateScale),
			ShadowCoordinateBias(InShadowCoordinateBias)
		{}
	};

	static UBOOL ShouldCache(
		EShaderPlatform Platform,
		const FMaterial* Material,
		const FVertexFactoryType* VertexFactoryType,
		UBOOL bLightRequiresStaticLightingShaders
		)
	{
		return VertexFactoryType->SupportsStaticLighting() &&
			(Material->IsUsedWithStaticLighting() || Material->IsSpecialEngineMaterial()) &&
			bLightRequiresStaticLightingShaders;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("STATICLIGHTING_TEXTUREMASK"),TEXT("1"));
	}

	FShadowTexturePolicy(const UTexture2D* InTexture):
		Texture(InTexture)
	{}

	// FShadowingPolicyInterface.
	void Set(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView* View
		) const
	{
		check(VertexFactory);
		VertexFactory->Set();
		if (PixelShaderParameters)
		{
			PixelShaderParameters->SetShadowTexture(PixelShader,Texture->Resource);
		}
	}

	/**
	* Get the decl and stream strides for this policy type and vertexfactory
	* @param VertexDeclaration - output decl 
	* @param StreamStrides - output array of vertex stream strides 
	* @param VertexFactory - factory to be used by this policy
	*/
	void GetVertexDeclarationInfo(FVertexDeclarationRHIParamRef &VertexDeclaration, DWORD *StreamStrides, const FVertexFactory* VertexFactory)
	{
		check(VertexFactory);
		VertexFactory->GetStreamStrides(StreamStrides);
		VertexDeclaration = VertexFactory->GetDeclaration();
	}

	void SetMesh(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		const ElementDataType& ElementData
		) const
	{
		VertexShaderParameters->SetCoordinateTransform(
			VertexShader,
			ElementData.ShadowCoordinateScale,
			ElementData.ShadowCoordinateBias
			);
	}
	friend UBOOL operator==(const FShadowTexturePolicy A,const FShadowTexturePolicy B)
	{
		return A.Texture == B.Texture;
	}
	friend INT Compare(const FShadowTexturePolicy& A,const FShadowTexturePolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(Texture);
		return 0;
	}

private:
	const UTexture2D* Texture;
};

/**
 * A shadowing policy for TMeshLightingDrawingPolicy which uses a 2D signed distance field texture to reconstruct static shadows.
 */
class FSignedDistanceFieldShadowTexturePolicy : public FShadowTexturePolicy
{
	typedef FShadowTexturePolicy Super;
public:

	class PixelParametersType : public Super::PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			DistanceFieldParameters.Bind(ParameterMap,TEXT("DistanceFieldParameters"),TRUE);
			Super::PixelParametersType::Bind(ParameterMap);
		}
		void Serialize(FArchive& Ar)
		{
			Ar << DistanceFieldParameters;
			Super::PixelParametersType::Serialize(Ar);
		}
		FShaderParameter DistanceFieldParameters;
	};

	struct ElementDataType : public Super::ElementDataType
	{
		const FLOAT PenumbraBias;
		const FLOAT PenumbraScale;
		const FLOAT ShadowExponent;

		/** Initialization constructor. */
		ElementDataType(const FVector2D& InShadowCoordinateScale,const FVector2D& InShadowCoordinateBias, const FLightSceneInfo* InLightSceneInfo, FLOAT DistanceFieldPenumbraScale):
			Super::ElementDataType(InShadowCoordinateScale, InShadowCoordinateBias),
			// Bias to convert distance from the distance field into the shadow penumbra based on penumbra size
			PenumbraBias(-.5f + Min(InLightSceneInfo->DistanceFieldShadowMapPenumbraSize * DistanceFieldPenumbraScale, 1.0f) * .5f),
			PenumbraScale(1.0f / Min(InLightSceneInfo->DistanceFieldShadowMapPenumbraSize * DistanceFieldPenumbraScale, 1.0f)),
			ShadowExponent(InLightSceneInfo->DistanceFieldShadowMapShadowExponent)
		{}
	};

	static UBOOL ShouldCache(
		EShaderPlatform Platform,
		const FMaterial* Material,
		const FVertexFactoryType* VertexFactoryType,
		UBOOL bLightRequiresStaticLightingShaders
		)
	{
		return Super::ShouldCache(Platform, Material, VertexFactoryType, bLightRequiresStaticLightingShaders)
			// Xbox, PS3, WiiU has GOnePassDominantLight=TRUE
			&& (Platform != SP_XBOXD3D && Platform != SP_PS3 && Platform != SP_WIIU);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("STATICLIGHTING_SIGNEDDISTANCEFIELD"),TEXT("1"));
	}

	FSignedDistanceFieldShadowTexturePolicy(const UTexture2D* InTexture):
		Super(InTexture)
	{}

	void SetMesh(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		const ElementDataType& ElementData
		) const
	{
		Super::SetMesh(VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, Mesh, BatchElementIndex, ElementData);
		if (PixelShaderParameters)
		{
			UBOOL bShowMipLevels = FALSE;
#if !FINAL_RELEASE
			bShowMipLevels = GVisualizeMipLevels;
#endif
			if ( bShowMipLevels == FALSE )
			{
				SetPixelShaderValue(PixelShader->GetPixelShader(), PixelShaderParameters->DistanceFieldParameters, FVector(ElementData.PenumbraBias, ElementData.PenumbraScale, ElementData.ShadowExponent));
			}
			else
			{
				SetPixelShaderValue(PixelShader->GetPixelShader(), PixelShaderParameters->DistanceFieldParameters, FVector(0.0f, 1.0f, 1.0f));
			}
		}
	}
};

/**
 * A shadowing policy for TMeshLightingDrawingPolicy which uses a shadow vertex buffer.
 */
class FShadowVertexBufferPolicy
{
public:

	typedef FNullLightShaderComponent VertexParametersType;
	typedef FNullLightShaderComponent PixelParametersType;
	typedef FMeshDrawingPolicy::ElementDataType ElementDataType;

	static UBOOL ShouldCache(
		EShaderPlatform Platform,
		const FMaterial* Material,
		const FVertexFactoryType* VertexFactoryType,
		UBOOL bLightRequiresStaticLightingShaders
		)
	{
		return VertexFactoryType->SupportsStaticLighting() && 
			(Material->IsUsedWithStaticLighting() || Material->IsSpecialEngineMaterial())
			//terrain never uses vertex shadowmaps
			&& !Material->IsTerrainMaterial() &&
			bLightRequiresStaticLightingShaders;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("STATICLIGHTING_VERTEXMASK"),TEXT("1"));
	}

	FShadowVertexBufferPolicy(const FVertexBuffer* InVertexBuffer):
		VertexBuffer(InVertexBuffer)
	{}

	void Set(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView* View
		) const
	{
		check(VertexFactory);
		VertexFactory->SetVertexShadowMap(VertexBuffer);
	}

	/**
	* Get the decl and stream strides for this policy type and vertexfactory
	* @param VertexDeclaration - output decl 
	* @param StreamStrides - output array of vertex stream strides 
	* @param VertexFactory - factory to be used by this policy
	*/
	void GetVertexDeclarationInfo(FVertexDeclarationRHIParamRef &VertexDeclaration, DWORD *StreamStrides, const FVertexFactory* VertexFactory)
	{
		check(VertexFactory);
		VertexFactory->GetVertexShadowMapStreamStrides(StreamStrides);
		VertexDeclaration = VertexFactory->GetVertexShadowMapDeclaration();
	}

	void SetMesh(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		const ElementDataType& ElementData
		) const
	{}
	friend UBOOL operator==(const FShadowVertexBufferPolicy A,const FShadowVertexBufferPolicy B)
	{
		return A.VertexBuffer == B.VertexBuffer;
	}
	friend INT Compare(const FShadowVertexBufferPolicy& A,const FShadowVertexBufferPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexBuffer);
		return 0;
	}

private:
	const FVertexBuffer* VertexBuffer;
};

/**
 * The vertex shader used to draw the effect of a light on a mesh.
 */
template<typename LightTypePolicy,typename ShadowingTypePolicy>
class TLightVertexShader :
	public FMeshMaterialVertexShader,
	public LightTypePolicy::VertexParametersType,
	public ShadowingTypePolicy::VertexParametersType
{
	DECLARE_SHADER_TYPE(TLightVertexShader,MeshMaterial);

protected:

	TLightVertexShader() {}

	TLightVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialVertexShader(Initializer)
	{
		LightTypePolicy::VertexParametersType::Bind(Initializer.ParameterMap);
		ShadowingTypePolicy::VertexParametersType::Bind(Initializer.ParameterMap);
		MaterialParameters.Bind(Initializer.ParameterMap);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		ShadowingTypePolicy::ModifyCompilationEnvironment(Platform, OutEnvironment);
		LightTypePolicy::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->GetLightingModel() != MLM_Unlit &&
			ShadowingTypePolicy::ShouldCache(Platform,Material,VertexFactoryType,LightTypePolicy::ShouldCacheStaticLightingShaders()) &&
			LightTypePolicy::ShouldCache(Platform,Material,VertexFactoryType);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		LightTypePolicy::VertexParametersType::Serialize(Ar);
		ShadowingTypePolicy::VertexParametersType::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
		Ar << MaterialParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FMaterialRenderProxy* MaterialRenderProxy,const FMaterial& MaterialResource,const FVertexFactory* VertexFactory,const FSceneView& View)
	{
		VertexFactoryParameters.Set(this,VertexFactory,View);
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, MaterialResource, View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set(this,MaterialRenderContext);
	}
	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshBatch& Mesh,INT BatchElementIndex,const FSceneView& View)
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
};

#if WITH_D3D11_TESSELLATION

/**
 * The domain shader used to draw the effect of a light on a mesh.
 */
template<typename LightTypePolicy,typename ShadowingTypePolicy>
class TLightDomainShader : public FBaseDomainShader
{
	DECLARE_SHADER_TYPE(TLightDomainShader,MeshMaterial);

protected:

	TLightDomainShader() {}

	TLightDomainShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FBaseDomainShader(Initializer)
	{}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDomainShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& TLightVertexShader<LightTypePolicy,ShadowingTypePolicy>::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		ShadowingTypePolicy::ModifyCompilationEnvironment(Platform, OutEnvironment);
		LightTypePolicy::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}
};

/**
 * The hull shader used to draw the effect of a light on a mesh.
 */
template<typename LightTypePolicy,typename ShadowingTypePolicy>
class TLightHullShader : public FBaseHullShader
{
	DECLARE_SHADER_TYPE(TLightHullShader,MeshMaterial);

protected:

	TLightHullShader() {}

	TLightHullShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FBaseHullShader(Initializer)
	{}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHullShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& TLightVertexShader<LightTypePolicy,ShadowingTypePolicy>::ShouldCache(Platform,Material,VertexFactoryType);		
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		ShadowingTypePolicy::ModifyCompilationEnvironment(Platform, OutEnvironment);
		LightTypePolicy::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}
};

#endif

/**
 * The pixel shader used to draw the effect of a light on a mesh.
 */
template<typename LightTypePolicy,typename ShadowingTypePolicy>
class TLightPixelShader :
	public FMeshMaterialPixelShader,
	public LightTypePolicy::PixelParametersType,
	public ShadowingTypePolicy::PixelParametersType
{
	DECLARE_SHADER_TYPE(TLightPixelShader,MeshMaterial);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->GetLightingModel() != MLM_Unlit &&
			ShadowingTypePolicy::ShouldCache(Platform,Material,VertexFactoryType,LightTypePolicy::ShouldCacheStaticLightingShaders()) &&
			LightTypePolicy::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		//The HLSL compiler for xenon will not always use predicated instructions without this flag.  
		//On PC the compiler consistently makes the right decision.
		new(OutEnvironment.CompilerFlags) ECompilerFlags(CFLAG_PreferFlowControl);
		if( Platform == SP_XBOXD3D )
		{
			// workaround for validator issue when using the [ifAll] attribute on Xenon
			new(OutEnvironment.CompilerFlags) ECompilerFlags(CFLAG_SkipValidation);
		}
		
		ShadowingTypePolicy::ModifyCompilationEnvironment(Platform, OutEnvironment);
		LightTypePolicy::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	TLightPixelShader() {}

	TLightPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialPixelShader(Initializer)
	{
		LightTypePolicy::PixelParametersType::Bind(Initializer.ParameterMap);
		ShadowingTypePolicy::PixelParametersType::Bind(Initializer.ParameterMap);
		MaterialParameters.Bind(Initializer.ParameterMap);
		LightAttenuationTextureParameter.Bind(Initializer.ParameterMap,TEXT("LightAttenuationTexture"),TRUE);
		ForwardShadowingParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& MaterialResource,
		const FVertexFactory* VertexFactory,
		const FSceneView* View,
		UBOOL bReceiveDynamicShadows,
		UBOOL bUseTranslucencyLightAttenuation)
	{
		VertexFactoryParameters.Set(this, VertexFactory, *View);
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, MaterialResource, View->Family->CurrentWorldTime, View->Family->CurrentRealTime, View);
		MaterialParameters.Set(this,MaterialRenderContext);

		ForwardShadowingParameters.SetReceiveShadows(this, bReceiveDynamicShadows);

		if(LightAttenuationTextureParameter.IsBound())
		{
			SetTextureParameter(
				GetPixelShader(),
				LightAttenuationTextureParameter,
				TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
				bUseTranslucencyLightAttenuation ? (const FTextureRHIRef&)GSceneRenderTargets.GetTranslucencyDominantLightAttenuationTexture() : GSceneRenderTargets.GetEffectiveLightAttenuationTexture(TRUE, TRUE)
				);
		}
	}

	void SetMesh(
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		const FSceneView& View,
		UBOOL bReceiveDynamicShadows,
		const FProjectedShadowInfo* TranslucentPreShadowInfo,
		UBOOL bOverrideDynamicShadowsOnTranslucency,
		UBOOL bBackFace)
	{
		VertexFactoryParameters.SetMesh(this,Mesh,BatchElementIndex,View);
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace);
		ForwardShadowingParameters.Set(View, this, bOverrideDynamicShadowsOnTranslucency, TranslucentPreShadowInfo);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
		LightTypePolicy::PixelParametersType::Serialize(Ar);
		ShadowingTypePolicy::PixelParametersType::Serialize(Ar);
		Ar << MaterialParameters;
		Ar << LightAttenuationTextureParameter;
		ForwardShadowingParameters.Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialPixelShaderParameters MaterialParameters;
	FShaderResourceParameter LightAttenuationTextureParameter;
	FForwardShadowingShaderParameters ForwardShadowingParameters;
};


#if WITH_D3D11_TESSELLATION

	#define IMPLEMENT_LIGHTSHADOWING_SHADER_TYPE_TESSELLATION(LightPolicyType,HullShaderFilename,DomainShaderFilename,ShadowingPolicyType,MinPackageVersion,MinLicenseePackageVersion) \
		typedef TLightHullShader<LightPolicyType,ShadowingPolicyType> TLightHullShader##LightPolicyType##ShadowingPolicyType; \
		IMPLEMENT_MATERIAL_SHADER_TYPE( \
			template<>, \
			TLightHullShader##LightPolicyType##ShadowingPolicyType, \
			HullShaderFilename, \
			TEXT("MainHull"), \
			SF_Hull, \
			Max((UINT)0,(UINT)MinPackageVersion), \
			MinLicenseePackageVersion \
			); \
		typedef TLightDomainShader<LightPolicyType,ShadowingPolicyType> TLightDomainShader##LightPolicyType##ShadowingPolicyType; \
		IMPLEMENT_MATERIAL_SHADER_TYPE( \
			template<>, \
			TLightDomainShader##LightPolicyType##ShadowingPolicyType, \
			DomainShaderFilename, \
			TEXT("MainDomain"), \
			SF_Domain, \
			Max((UINT)0,(UINT)MinPackageVersion), \
			MinLicenseePackageVersion \
			); 
#else
// define empty for non tessellation cases
#define IMPLEMENT_LIGHTSHADOWING_SHADER_TYPE_TESSELLATION(LightPolicyType,HullShaderFilename,DomainShaderFilename,ShadowingPolicyType,MinPackageVersion,MinLicenseePackageVersion) 
#endif

/** 
* Implements the vertex shader and pixel shader for a given light type.  
*/

#define IMPLEMENT_LIGHTSHADOWING_SHADER_TYPE(LightPolicyType,VertexShaderFilename,HullShaderFilename,DomainShaderFilename,PixelShaderFilename,ShadowingPolicyType,MinPackageVersion,MinLicenseePackageVersion) \
	typedef TLightVertexShader<LightPolicyType,ShadowingPolicyType> TLightVertexShader##LightPolicyType##ShadowingPolicyType; \
	IMPLEMENT_MATERIAL_SHADER_TYPE( \
		template<>, \
		TLightVertexShader##LightPolicyType##ShadowingPolicyType, \
		VertexShaderFilename, \
		TEXT("Main"), \
		SF_Vertex, \
		Max((UINT)0,(UINT)MinPackageVersion), \
		MinLicenseePackageVersion \
		);	\
	IMPLEMENT_LIGHTSHADOWING_SHADER_TYPE_TESSELLATION(LightPolicyType,HullShaderFilename,DomainShaderFilename,ShadowingPolicyType,MinPackageVersion,MinLicenseePackageVersion)\
	typedef TLightPixelShader<LightPolicyType,ShadowingPolicyType> TLightPixelShader##LightPolicyType##ShadowingPolicyType; \
	IMPLEMENT_MATERIAL_SHADER_TYPE( \
		template<>, \
		TLightPixelShader##LightPolicyType##ShadowingPolicyType, \
		PixelShaderFilename, \
		TEXT("Main"), \
		SF_Pixel, \
		MinPackageVersion, \
		MinLicenseePackageVersion \
	); 

/** 
* Implements a version of TBranchingPCFModProjectionPixelShader
*/
#define IMPLEMENT_LIGHT_BPCF_SHADER_TYPE(LightPolicyType,EntryFunctionName,BranchingPCFPolicy,MinPackageVersion,MinLicenseePackageVersion) \
	typedef TBranchingPCFModProjectionPixelShader<LightPolicyType,BranchingPCFPolicy> TBranchingPCFModProjectionPixelShader##LightPolicyType##BranchingPCFPolicy; \
	IMPLEMENT_SHADER_TYPE( \
		template<>, \
		TBranchingPCFModProjectionPixelShader##LightPolicyType##BranchingPCFPolicy, \
		TEXT("BranchingPCFModProjectionPixelShader"), \
		EntryFunctionName, \
		SF_Pixel, \
		MinPackageVersion, \
		MinLicenseePackageVersion \
	);

/** 
* Implements a version of TModShadowProjectionPixelShader
*/
#define IMPLEMENT_LIGHT_UNIFORMPCF_SHADER_TYPE(LightPolicyType,EntryFunctionName,UniformPCFPolicy,MinPackageVersion,MinLicenseePackageVersion) \
	typedef TModShadowProjectionPixelShader<LightPolicyType,UniformPCFPolicy> TModShadowProjectionPixelShader##LightPolicyType##UniformPCFPolicy; \
	IMPLEMENT_SHADER_TYPE( \
		template<>, \
		TModShadowProjectionPixelShader##LightPolicyType##UniformPCFPolicy, \
		TEXT("ModShadowProjectionPixelShader"), \
		EntryFunctionName, \
		SF_Pixel, \
		MinPackageVersion, \
		MinLicenseePackageVersion \
	);

/**
* Implements all of the shader types which must be compiled for a particular light policy type. 
*
* A IMPLEMENT_LIGHTSHADOWING_SHADER_TYPE for each static shadowing policy
* 5 IMPLEMENT_LIGHT_UNIFORMPCF_SHADER_TYPE's for support for Hardware PCF, Fetch4.
* 9 IMPLEMENT_LIGHT_BPCF_SHADER_TYPE's for Hardware PCF, Fetch4 and 3 quality levels.
*/

#define IMPLEMENT_SHADOWLESS_LIGHT_SHADER_TYPE(LightPolicyType,VertexShaderFilename,PixelShaderFilename,MinPackageVersion,MinLicenseePackageVersion) \
	IMPLEMENT_LIGHTSHADOWING_SHADER_TYPE(LightPolicyType,VertexShaderFilename,VertexShaderFilename,VertexShaderFilename,PixelShaderFilename,FNoStaticShadowingPolicy,Max((UINT)VER_ONEPASS_TRANSLUCENCY_LIGHTING,(UINT)MinPackageVersion),MinLicenseePackageVersion) \
	IMPLEMENT_LIGHTSHADOWING_SHADER_TYPE(LightPolicyType,VertexShaderFilename,VertexShaderFilename,VertexShaderFilename,PixelShaderFilename,FShadowTexturePolicy,Max((UINT)VER_ONEPASS_TRANSLUCENCY_LIGHTING,(UINT)MinPackageVersion),MinLicenseePackageVersion) \
	IMPLEMENT_LIGHTSHADOWING_SHADER_TYPE(LightPolicyType,VertexShaderFilename,VertexShaderFilename,VertexShaderFilename,PixelShaderFilename,FSignedDistanceFieldShadowTexturePolicy,Max((UINT)VER_ONEPASS_TRANSLUCENCY_LIGHTING,(UINT)MinPackageVersion),MinLicenseePackageVersion) \
	IMPLEMENT_LIGHTSHADOWING_SHADER_TYPE(LightPolicyType,VertexShaderFilename,VertexShaderFilename,VertexShaderFilename,PixelShaderFilename,FShadowVertexBufferPolicy,Max((UINT)VER_ONEPASS_TRANSLUCENCY_LIGHTING,(UINT)MinPackageVersion),MinLicenseePackageVersion)

#define IMPLEMENT_LIGHT_SHADER_TYPE(LightPolicyType,VertexShaderFilename,PixelShaderFilename,MinPackageVersion,MinLicenseePackageVersion) \
	IMPLEMENT_SHADOWLESS_LIGHT_SHADER_TYPE(LightPolicyType,VertexShaderFilename,PixelShaderFilename,MinPackageVersion,MinLicenseePackageVersion) \
	IMPLEMENT_LIGHT_UNIFORMPCF_SHADER_TYPE(LightPolicyType,TEXT("HardwarePCFMain"),F4SampleHwPCF,Max((UINT)VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,(UINT)MinPackageVersion),MinLicenseePackageVersion) \
	IMPLEMENT_LIGHT_UNIFORMPCF_SHADER_TYPE(LightPolicyType,TEXT("Main"),F4SampleManualPCFPerPixel,Max((UINT)VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,(UINT)MinPackageVersion),MinLicenseePackageVersion) \
	IMPLEMENT_LIGHT_UNIFORMPCF_SHADER_TYPE(LightPolicyType,TEXT("HardwarePCFMain"),F16SampleHwPCF,Max((UINT)VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,(UINT)MinPackageVersion),MinLicenseePackageVersion) \
	IMPLEMENT_LIGHT_UNIFORMPCF_SHADER_TYPE(LightPolicyType,TEXT("Fetch4Main"),F16SampleFetch4PCF,MinPackageVersion,MinLicenseePackageVersion) \
	IMPLEMENT_LIGHT_UNIFORMPCF_SHADER_TYPE(LightPolicyType,TEXT("Main"),F16SampleManualPCFPerPixel,Max((UINT)VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,(UINT)MinPackageVersion),MinLicenseePackageVersion) \
	IMPLEMENT_LIGHT_BPCF_SHADER_TYPE(LightPolicyType,TEXT("HardwarePCFMain"),FLowQualityHwPCF,Max((UINT)VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,(UINT)MinPackageVersion),MinLicenseePackageVersion) \
	IMPLEMENT_LIGHT_BPCF_SHADER_TYPE(LightPolicyType,TEXT("HardwarePCFMain"),FMediumQualityHwPCF,Max((UINT)VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,(UINT)MinPackageVersion),MinLicenseePackageVersion) \
	IMPLEMENT_LIGHT_BPCF_SHADER_TYPE(LightPolicyType,TEXT("HardwarePCFMain"),FHighQualityHwPCF,Max((UINT)VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,(UINT)MinPackageVersion),MinLicenseePackageVersion) \
	IMPLEMENT_LIGHT_BPCF_SHADER_TYPE(LightPolicyType,TEXT("Fetch4Main"),FLowQualityFetch4PCF,MinPackageVersion,MinLicenseePackageVersion) \
	IMPLEMENT_LIGHT_BPCF_SHADER_TYPE(LightPolicyType,TEXT("Fetch4Main"),FMediumQualityFetch4PCF,MinPackageVersion,MinLicenseePackageVersion) \
	IMPLEMENT_LIGHT_BPCF_SHADER_TYPE(LightPolicyType,TEXT("Fetch4Main"),FHighQualityFetch4PCF,MinPackageVersion,MinLicenseePackageVersion) \
	IMPLEMENT_LIGHT_BPCF_SHADER_TYPE(LightPolicyType,TEXT("Main"),FLowQualityManualPCF,Max((UINT)VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,(UINT)MinPackageVersion),MinLicenseePackageVersion) \
	IMPLEMENT_LIGHT_BPCF_SHADER_TYPE(LightPolicyType,TEXT("Main"),FMediumQualityManualPCF,Max((UINT)VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,(UINT)MinPackageVersion),MinLicenseePackageVersion) \
	IMPLEMENT_LIGHT_BPCF_SHADER_TYPE(LightPolicyType,TEXT("Main"),FHighQualityManualPCF,Max((UINT)VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,(UINT)MinPackageVersion),MinLicenseePackageVersion) \

/**
 * A drawing policy factory for the lighting drawing policy.
 */
class FMeshLightingDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = FALSE };
	typedef const FLightSceneInfo* ContextType;

	static ELightInteractionType AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh,FLightSceneInfo* Light);
	static UBOOL DrawDynamicMesh(
		const FSceneView& View,
		const FLightSceneInfo* Light,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);
	static UBOOL IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy)
	{
		return MaterialRenderProxy 
			// Dynamically lit translucency is not rendered through this drawing policy factory
			&& (IsTranslucentBlendMode(MaterialRenderProxy->GetMaterial()->GetBlendMode())
			|| MaterialRenderProxy->GetMaterial()->GetLightingModel() == MLM_Unlit);
	}
};

/**
 * Draws a light's interaction with a mesh.
 */
template<typename ShadowPolicyType,typename LightPolicyType>
class TMeshLightingDrawingPolicy : public FMeshDrawingPolicy
{
public:

	typedef typename ShadowPolicyType::ElementDataType ElementDataType;
	typedef typename LightPolicyType::SceneInfoType LightSceneInfoType;

	typedef TLightVertexShader<LightPolicyType,ShadowPolicyType> VertexShaderType;

#if WITH_D3D11_TESSELLATION
	typedef TLightHullShader<LightPolicyType,ShadowPolicyType> HullShaderType;
	typedef TLightDomainShader<LightPolicyType,ShadowPolicyType> DomainShaderType;
#endif

	typedef TLightPixelShader<LightPolicyType,ShadowPolicyType> PixelShaderType;

	void FindShaders(const FVertexFactory* InVertexFactory,const FMaterialRenderProxy* InMaterialRenderProxy,const FMaterial& InMaterialResource)
	{
		// Find the shaders used to render this material/vertexfactory/light combination.

#if WITH_D3D11_TESSELLATION
		HullShader = NULL;
		DomainShader = NULL;

		EMaterialTessellationMode MaterialTessellationMode = InMaterialResource.GetD3D11TessellationMode();

		if(GRHIShaderPlatform == SP_PCD3D_SM5
			&& InVertexFactory->GetType()->SupportsTessellationShaders() 
			&& MaterialTessellationMode != MTM_NoTessellation)
		{
			HullShader = InMaterialResource.GetShader<TLightHullShader<LightPolicyType,ShadowPolicyType> >(VertexFactory->GetType());
			DomainShader = InMaterialResource.GetShader<TLightDomainShader<LightPolicyType,ShadowPolicyType> >(VertexFactory->GetType());
		}
#endif
		VertexShader = InMaterialResource.GetShader<TLightVertexShader<LightPolicyType,ShadowPolicyType> >(InVertexFactory->GetType());
		PixelShader = InMaterialResource.GetShader<PixelShaderType>(InVertexFactory->GetType());
	}

	/** Initialization constructor. */
	TMeshLightingDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		const LightSceneInfoType* InLight,
		ShadowPolicyType InShadowPolicy,
		UBOOL bInReceiveDynamicShadows,
		UBOOL bOverrideWithShaderComplexity = FALSE,
		const FProjectedShadowInfo* InTranslucentPreShadowInfo = NULL,
		UBOOL bInOverrideDynamicShadowsOnTranslucency = FALSE,
		UBOOL bInUseTranslucencyLightAttenuation = FALSE
		):
		FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource,bOverrideWithShaderComplexity),
		Light(InLight),
		bReceiveDynamicShadows(bInReceiveDynamicShadows),
		bOverrideDynamicShadowsOnTranslucency(bInOverrideDynamicShadowsOnTranslucency),
		bUseTranslucencyLightAttenuation(bInUseTranslucencyLightAttenuation),
		ShadowPolicy(InShadowPolicy),
		TranslucentPreShadowInfo(InTranslucentPreShadowInfo)
	{
		FindShaders(InVertexFactory,InMaterialRenderProxy,InMaterialResource);
	}

	TMeshLightingDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		const LightSceneInfoType* InLight
		):
		FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource),
		Light(InLight),
		bReceiveDynamicShadows(TRUE),
		bOverrideDynamicShadowsOnTranslucency(FALSE),
		bUseTranslucencyLightAttenuation(FALSE),
		TranslucentPreShadowInfo(NULL)
	{
		FindShaders(InVertexFactory,InMaterialRenderProxy,InMaterialResource);
	}

	// FMeshDrawingPolicy interface.
	UBOOL Matches(const TMeshLightingDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) &&
			VertexShader == Other.VertexShader &&
			PixelShader == Other.PixelShader &&
#if WITH_D3D11_TESSELLATION
			HullShader == Other.HullShader &&
			DomainShader == Other.DomainShader &&
#endif
			Light == Other.Light &&
			ShadowPolicy == Other.ShadowPolicy &&
			bReceiveDynamicShadows == Other.bReceiveDynamicShadows &&
			bUseTranslucencyLightAttenuation == Other.bUseTranslucencyLightAttenuation;
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
		ShadowPolicy.SetMesh(VertexShader,PixelShader,VertexShader,PixelShader,Mesh,BatchElementIndex,ElementData);

#if !FINAL_RELEASE
		// Don't set pixel shader constants if we are overriding with the shader complexity shader
		if (!bOverrideWithShaderComplexity)
#endif
		{
			PixelShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bReceiveDynamicShadows,TranslucentPreShadowInfo,bOverrideDynamicShadowsOnTranslucency,bBackFace);
			// Apply the light function's disabled brightness setting if it is disabled due to show flags,
			// So that the light will be the right overall brightness even though the light function is disabled.
			const UBOOL bApplyLightFunctionDisabledBrightness = Light->LightFunction && !(View.Family->ShowFlags & SHOW_DynamicShadows);
			PixelShader->SetLightMesh(PixelShader,PrimitiveSceneInfo,Light,bApplyLightFunctionDisabledBrightness);
		}

		VertexShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);

#if WITH_D3D11_TESSELLATION
		if(HullShader && DomainShader)
		{
			HullShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
			DomainShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
		}
#endif
		
		FMeshDrawingPolicy::SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,FMeshDrawingPolicy::ElementDataType());
	}

	void DrawShared(const FSceneView* View,FBoundShaderStateRHIParamRef BoundShaderState) const
	{

#if !FINAL_RELEASE
		if (bOverrideWithShaderComplexity)
		{
			TShaderMapRef<FShaderComplexityAccumulatePixelShader> ShaderComplexityPixelShader(GetGlobalShaderMap());
			//don't add any vertex complexity
			ShaderComplexityPixelShader->SetParameters(0, PixelShader->GetNumInstructions());
		}
		else
#endif
		{
			PixelShader->SetParameters(MaterialRenderProxy,*MaterialResource,VertexFactory,View,bReceiveDynamicShadows,bUseTranslucencyLightAttenuation);
			PixelShader->SetLight(PixelShader,Light,View);
		}

		ShadowPolicy.Set(VertexShader,bOverrideWithShaderComplexity ? NULL : PixelShader,PixelShader,VertexFactory,MaterialRenderProxy,View);

		VertexShader->SetParameters(MaterialRenderProxy,*MaterialResource,VertexFactory,*View);
		VertexShader->SetLight(VertexShader->GetVertexShader(),Light,View);

#if WITH_D3D11_TESSELLATION
		if(HullShader && DomainShader)
		{
			HullShader->SetParameters(MaterialRenderProxy,*View);
			DomainShader->SetParameters(MaterialRenderProxy,*View);
		}
#endif
		
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

		ShadowPolicy.GetVertexDeclarationInfo(VertexDeclaration, StreamStrides, VertexFactory);
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
			BoundShaderState = RHICreateBoundShaderState(VertexDeclaration, StreamStrides, VertexShader->GetVertexShader(), PixelShaderRHIRef, EGST_None);
#endif

		return BoundShaderState;
	}

	friend INT Compare(const TMeshLightingDrawingPolicy& A,const TMeshLightingDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
		COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
#if WITH_D3D11_TESSELLATION
		COMPAREDRAWINGPOLICYMEMBERS(HullShader);
		COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
#endif
		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		COMPAREDRAWINGPOLICYMEMBERS(bReceiveDynamicShadows);
		COMPAREDRAWINGPOLICYMEMBERS(bUseTranslucencyLightAttenuation);
		return Compare(A.ShadowPolicy,B.ShadowPolicy);
	}

private:
	const LightSceneInfoType* Light;
	const BITFIELD bReceiveDynamicShadows : 1;
	const BITFIELD bOverrideDynamicShadowsOnTranslucency : 1;
	const BITFIELD bUseTranslucencyLightAttenuation : 1;

protected:
	VertexShaderType* VertexShader;
	PixelShaderType* PixelShader;

#if WITH_D3D11_TESSELLATION
	HullShaderType* HullShader;
	DomainShaderType* DomainShader;
#endif

	ShadowPolicyType ShadowPolicy;
	const FProjectedShadowInfo* TranslucentPreShadowInfo;
};

template<class LightPolicyType>
UBOOL DrawLitDynamicMesh(
	const FSceneView& View,
	const FLightSceneInfo* LightSceneInfo,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	UBOOL bTranslucent,
	UBOOL bUseTranslucencyLightAttenuation,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FProjectedShadowInfo* TranslucentPreShadowInfo,
	FHitProxyId HitProxyId
	) 
{
	// Check for cached shadow data.
	FLightInteraction CachedInteraction = FLightInteraction::Uncached();
	if(Mesh.LCI)
	{
		CachedInteraction = Mesh.LCI->GetInteraction(LightSceneInfo);
	}

	const UBOOL bRenderShaderComplexity = (View.Family->ShowFlags & SHOW_ShaderComplexity) != 0;
	// Only allow receiving dynamic shadows from non-dominant lights or from dominant lights
	// When bUseAsOccluder == TRUE, to emulate the behavior when doing one pass dominant lighting, and when bAcceptsDynamicDominantLightShadows == TRUE.
	UBOOL bReceiveDynamicShadows = FALSE;
	if (PrimitiveSceneInfo)
	{	
		bReceiveDynamicShadows = bTranslucent && (bUseTranslucencyLightAttenuation || TranslucentPreShadowInfo || !PrimitiveSceneInfo->bAllowDynamicShadowsOnTranslucency && PrimitiveSceneInfo->bTranslucencyShadowed)
			|| !bTranslucent && (!IsDominantLightType(LightSceneInfo->LightType) || PrimitiveSceneInfo->bUseAsOccluder && PrimitiveSceneInfo->bAcceptsDynamicDominantLightShadows);
	}
	const UBOOL bOverrideDynamicShadowsOnTranslucency = bTranslucent && !PrimitiveSceneInfo->bAllowDynamicShadowsOnTranslucency && PrimitiveSceneInfo->bTranslucencyShadowed;

	const FMaterial& MaterialResource = *Mesh.MaterialRenderProxy->GetMaterial();

	// Add the mesh to the appropriate draw list for the cached shadow data type.
	switch(CachedInteraction.GetType())
	{
	case LIT_CachedShadowMap1D:
		{
			TMeshLightingDrawingPolicy<FShadowVertexBufferPolicy,LightPolicyType> DrawingPolicy(
				Mesh.VertexFactory,
				Mesh.MaterialRenderProxy,
				MaterialResource,
				(typename LightPolicyType::SceneInfoType*)LightSceneInfo,
				FShadowVertexBufferPolicy(CachedInteraction.GetShadowVertexBuffer()),
				bReceiveDynamicShadows,
				bRenderShaderComplexity,
				TranslucentPreShadowInfo,
				bOverrideDynamicShadowsOnTranslucency,
				bUseTranslucencyLightAttenuation
				);
			DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
			for( INT BatchElementIndex=0;BatchElementIndex < Mesh.Elements.Num();BatchElementIndex++ )
			{
				DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,FShadowVertexBufferPolicy::ElementDataType());
				DrawingPolicy.DrawMesh(Mesh, BatchElementIndex);
			}
			return TRUE;
		}
	case LIT_CachedShadowMap2D:
		{
			TMeshLightingDrawingPolicy<FShadowTexturePolicy,LightPolicyType> DrawingPolicy(
				Mesh.VertexFactory,
				Mesh.MaterialRenderProxy,
				MaterialResource,
				(typename LightPolicyType::SceneInfoType*)LightSceneInfo,
				FShadowTexturePolicy(CachedInteraction.GetShadowTexture()),
				bReceiveDynamicShadows,
				bRenderShaderComplexity,
				TranslucentPreShadowInfo,
				bOverrideDynamicShadowsOnTranslucency,
				bUseTranslucencyLightAttenuation
				);
			DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
			for( INT BatchElementIndex=0;BatchElementIndex < Mesh.Elements.Num();BatchElementIndex++ )
			{
				DrawingPolicy.SetMeshRenderState(
					View,
					PrimitiveSceneInfo,
					Mesh,
					BatchElementIndex,
					bBackFace,
					FShadowTexturePolicy::ElementDataType(
					CachedInteraction.GetShadowCoordinateScale(),
					CachedInteraction.GetShadowCoordinateBias()
					)
					);
				DrawingPolicy.DrawMesh(Mesh, BatchElementIndex);
			}
			return TRUE;
		}
	case LIT_CachedSignedDistanceFieldShadowMap2D:
		{
			checkSlow(!GOnePassDominantLight);
			TMeshLightingDrawingPolicy<FSignedDistanceFieldShadowTexturePolicy,LightPolicyType> DrawingPolicy(
				Mesh.VertexFactory,
				Mesh.MaterialRenderProxy,
				MaterialResource,
				(typename LightPolicyType::SceneInfoType*)LightSceneInfo,
				FSignedDistanceFieldShadowTexturePolicy(CachedInteraction.GetShadowTexture()),
				bReceiveDynamicShadows,
				bRenderShaderComplexity,
				TranslucentPreShadowInfo,
				bOverrideDynamicShadowsOnTranslucency,
				bUseTranslucencyLightAttenuation
				);
			DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
			for( INT BatchElementIndex=0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++ )
			{
				DrawingPolicy.SetMeshRenderState(
					View,
					PrimitiveSceneInfo,
					Mesh,
					BatchElementIndex,
					bBackFace,
					FSignedDistanceFieldShadowTexturePolicy::ElementDataType(
					CachedInteraction.GetShadowCoordinateScale(),
					CachedInteraction.GetShadowCoordinateBias(),
					LightSceneInfo,
					Mesh.MaterialRenderProxy->GetDistanceFieldPenumbraScale()
					)
					);
				DrawingPolicy.DrawMesh(Mesh, BatchElementIndex);
			}
			return TRUE;
		}
	case LIT_Uncached:
		{
			TMeshLightingDrawingPolicy<FNoStaticShadowingPolicy,LightPolicyType> DrawingPolicy(
				Mesh.VertexFactory,
				Mesh.MaterialRenderProxy,
				MaterialResource,
				(typename LightPolicyType::SceneInfoType*)LightSceneInfo,
				FNoStaticShadowingPolicy(),
				bReceiveDynamicShadows,
				bRenderShaderComplexity,
				TranslucentPreShadowInfo,
				bOverrideDynamicShadowsOnTranslucency,
				bUseTranslucencyLightAttenuation
				);
			DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
			for( INT BatchElementIndex=0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++ )
			{
				DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo, Mesh, BatchElementIndex, bBackFace,FNoStaticShadowingPolicy::ElementDataType());
				DrawingPolicy.DrawMesh(Mesh, BatchElementIndex);
			}
			return TRUE;
		}
	case LIT_CachedIrrelevant:
	case LIT_CachedLightMap:
	default:
		return FALSE;
	};
}

/** Information about a light's effect on a scene's DPG. */
template<class LightPolicyType>
class TLightSceneDPGInfo : public FLightSceneDPGInfoInterface
{
public:

	// FLightSceneDPGInfoInterface
	virtual UBOOL DrawStaticMeshesVisible(
		const FViewInfo& View,
		const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap,
		ELightPassDrawListType DrawType
		) const
	{
		UBOOL bDirty = FALSE;
		bDirty |= NoStaticShadowingDrawList[DrawType].DrawVisible(View,StaticMeshVisibilityMap);
		bDirty |= ShadowTextureDrawList[DrawType].DrawVisible(View,StaticMeshVisibilityMap);
		bDirty |= SignedDistanceFieldShadowTextureDrawList[DrawType].DrawVisible(View,StaticMeshVisibilityMap);
		bDirty |= ShadowVertexBufferDrawList[DrawType].DrawVisible(View,StaticMeshVisibilityMap);
		return bDirty;
	}

	virtual ELightInteractionType AttachStaticMesh(const FLightSceneInfo* LightSceneInfo,FStaticMesh* Mesh)
	{
		// Check for cached shadow data.
		FLightInteraction CachedInteraction = FLightInteraction::Uncached();
		if(Mesh->LCI)
		{
			CachedInteraction = Mesh->LCI->GetInteraction(LightSceneInfo);
		}

		ELightPassDrawListType DrawType = ELightPass_Default;
		if( Mesh->IsDecal() )
		{	
			// handle decal case by adding to the decal lightinfo draw lists
			DrawType = ELightPass_Decals;
		}

		// Only allow receiving dynamic shadows from non-dominant lights or from dominant lights
		// When bUseAsOccluder == TRUE, to emulate the behavior when doing one pass dominant lighting, and when bAcceptsDynamicDominantLightShadows == TRUE.
		const UBOOL bReceiveDynamicShadows = !IsDominantLightType(LightSceneInfo->LightType) 
			|| (Mesh->PrimitiveSceneInfo ? Mesh->PrimitiveSceneInfo->bUseAsOccluder && Mesh->PrimitiveSceneInfo->bAcceptsDynamicDominantLightShadows : FALSE);

		const FMaterial& MaterialResource = *Mesh->MaterialRenderProxy->GetMaterial();

		// Add the mesh to the appropriate draw list for the cached shadow data type.
		switch(CachedInteraction.GetType())
		{
		case LIT_CachedShadowMap1D:
			{
				ShadowVertexBufferDrawList[DrawType].AddMesh(
					Mesh,
					FShadowVertexBufferPolicy::ElementDataType(),
					TMeshLightingDrawingPolicy<FShadowVertexBufferPolicy,LightPolicyType>(
						Mesh->VertexFactory,
						Mesh->MaterialRenderProxy,
						MaterialResource,
						(typename LightPolicyType::SceneInfoType*)LightSceneInfo,
						FShadowVertexBufferPolicy(CachedInteraction.GetShadowVertexBuffer()),
						bReceiveDynamicShadows
						)
					);
				break;
			}
		case LIT_CachedShadowMap2D:
			{
				ShadowTextureDrawList[DrawType].AddMesh(
					Mesh,
					FShadowTexturePolicy::ElementDataType(
						CachedInteraction.GetShadowCoordinateScale(),
						CachedInteraction.GetShadowCoordinateBias()
						),
					TMeshLightingDrawingPolicy<FShadowTexturePolicy,LightPolicyType>(
						Mesh->VertexFactory,
						Mesh->MaterialRenderProxy,
						MaterialResource,
						(typename LightPolicyType::SceneInfoType*)LightSceneInfo,
						FShadowTexturePolicy(CachedInteraction.GetShadowTexture()),
						bReceiveDynamicShadows
						)
					);
				break;
			}
		case LIT_CachedSignedDistanceFieldShadowMap2D:
			{
				checkSlow(!GOnePassDominantLight);
				SignedDistanceFieldShadowTextureDrawList[DrawType].AddMesh(
					Mesh,
					FSignedDistanceFieldShadowTexturePolicy::ElementDataType(
						CachedInteraction.GetShadowCoordinateScale(),
						CachedInteraction.GetShadowCoordinateBias(),
						LightSceneInfo,
						Mesh->MaterialRenderProxy->GetDistanceFieldPenumbraScale()
						),
					TMeshLightingDrawingPolicy<FSignedDistanceFieldShadowTexturePolicy,LightPolicyType>(
						Mesh->VertexFactory,
						Mesh->MaterialRenderProxy,
						MaterialResource,
						(typename LightPolicyType::SceneInfoType*)LightSceneInfo,
						FSignedDistanceFieldShadowTexturePolicy(CachedInteraction.GetShadowTexture()),
						bReceiveDynamicShadows
						)
					);
				break;
			}
		case LIT_Uncached:
			{
				NoStaticShadowingDrawList[DrawType].AddMesh(
					Mesh,
					FNoStaticShadowingPolicy::ElementDataType(),
					TMeshLightingDrawingPolicy<FNoStaticShadowingPolicy,LightPolicyType>(
						Mesh->VertexFactory,
						Mesh->MaterialRenderProxy,
						MaterialResource,
						(typename LightPolicyType::SceneInfoType*)LightSceneInfo,
						FNoStaticShadowingPolicy(),
						bReceiveDynamicShadows
						)
					);
				break;
			}
		case LIT_CachedIrrelevant:
		case LIT_CachedLightMap:
		default:
			break;
		};

		return CachedInteraction.GetType();
	}

	virtual void DetachStaticMeshes()
	{
		for(INT PassIndex = 0;PassIndex < ELightPass_MAX;PassIndex++)
		{
			NoStaticShadowingDrawList[PassIndex].RemoveAllMeshes();
			ShadowTextureDrawList[PassIndex].RemoveAllMeshes();
			SignedDistanceFieldShadowTextureDrawList[PassIndex].RemoveAllMeshes();
			ShadowVertexBufferDrawList[PassIndex].RemoveAllMeshes();
		}
	}

	virtual UBOOL DrawDynamicMesh(
		const FSceneView& View,
		const FLightSceneInfo* LightSceneInfo,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		) const
	{
		return DrawLitDynamicMesh<LightPolicyType>(View, LightSceneInfo, Mesh, bBackFace, bPreFog, FALSE, FALSE, PrimitiveSceneInfo, NULL, HitProxyId);
	}

private:
	TStaticMeshDrawList<TMeshLightingDrawingPolicy<FNoStaticShadowingPolicy,LightPolicyType> > NoStaticShadowingDrawList[ELightPass_MAX];
	TStaticMeshDrawList<TMeshLightingDrawingPolicy<FShadowTexturePolicy,LightPolicyType> > ShadowTextureDrawList[ELightPass_MAX];
	TStaticMeshDrawList<TMeshLightingDrawingPolicy<FSignedDistanceFieldShadowTexturePolicy,LightPolicyType> > SignedDistanceFieldShadowTextureDrawList[ELightPass_MAX];
	TStaticMeshDrawList<TMeshLightingDrawingPolicy<FShadowVertexBufferPolicy,LightPolicyType> > ShadowVertexBufferDrawList[ELightPass_MAX];
};

#endif
