/*=============================================================================
	LightMapDensityRendering.h: Definitions for rendering lightmap density.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
/**
 * The base shader type for vertex shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */
template<typename LightMapPolicyType>
class TLightMapDensityVertexShader : public FMeshMaterialVertexShader, public LightMapPolicyType::VertexParametersType
{
	DECLARE_SHADER_TYPE(TLightMapDensityVertexShader,MeshMaterial);

public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return (Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->MaterialModifiesMeshPosition()) && 
				(Platform != SP_XBOXD3D) && (Platform != SP_PS3) &&
				LightMapPolicyType::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Initialization constructor. */
	TLightMapDensityVertexShader(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialVertexShader(Initializer)
	{
		LightMapPolicyType::VertexParametersType::Bind(Initializer.ParameterMap);
		MaterialParameters.Bind(Initializer.ParameterMap);
	}
	TLightMapDensityVertexShader() {}

	virtual UBOOL Serialize(FArchive& Ar)
	{
#if PS3
		//@hack - compiler bug? optimized version crashes during FShader::Serialize call
		static INT RemoveMe=0;	RemoveMe=1;
#endif
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		LightMapPolicyType::VertexParametersType::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
		Ar << MaterialParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View
		)
	{
		VertexFactoryParameters.Set(this,VertexFactory,View);
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set(this,MaterialRenderContext);
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMeshBatch& Mesh, INT BatchElementIndex,const FSceneView& View)
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
 * The base shader type for hull shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */
template<typename LightMapPolicyType>
class TLightMapDensityHullShader : public FBaseHullShader
{
	DECLARE_SHADER_TYPE(TLightMapDensityHullShader,MeshMaterial);

public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHullShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& TLightMapDensityVertexShader<LightMapPolicyType>::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Initialization constructor. */
	TLightMapDensityHullShader(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseHullShader(Initializer)
	{}

	TLightMapDensityHullShader() {}
};

/**
 * The base shader type for domain shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */
template<typename LightMapPolicyType>
class TLightMapDensityDomainShader : public FBaseDomainShader
{
	DECLARE_SHADER_TYPE(TLightMapDensityDomainShader,MeshMaterial);

public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDomainShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& TLightMapDensityVertexShader<LightMapPolicyType>::ShouldCache(Platform, Material, VertexFactoryType);		
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Initialization constructor. */
	TLightMapDensityDomainShader(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseDomainShader(Initializer)
	{}

	TLightMapDensityDomainShader() {}
};

#endif

/**
 * The base type for pixel shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 * The base type is shared between the versions with and without sky light.
 */
template<typename LightMapPolicyType>
class TLightMapDensityPixelShader : public FShader, public LightMapPolicyType::PixelParametersType
{
	DECLARE_SHADER_TYPE(TLightMapDensityPixelShader,MeshMaterial);

public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return (Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->MaterialModifiesMeshPosition()) && 
				(Platform != SP_XBOXD3D) && (Platform != SP_PS3) &&
				LightMapPolicyType::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Initialization constructor. */
	TLightMapDensityPixelShader(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		LightMapPolicyType::PixelParametersType::Bind(Initializer.ParameterMap);
		MaterialParameters.Bind(Initializer.ParameterMap);
		LightMapDensityParameter.Bind(Initializer.ParameterMap,TEXT("LightMapDensityParameters"),TRUE);
		BuiltLightingAndSelectedFlagsParameter.Bind(Initializer.ParameterMap,TEXT("BuiltLightingAndSelectedFlags"),TRUE);
		DensitySelectedColorParameter.Bind(Initializer.ParameterMap,TEXT("DensitySelectedColor"),TRUE);
		LightMapResolutionScaleParameter.Bind(Initializer.ParameterMap,TEXT("LightMapResolutionScale"),TRUE);
		LightMapDensityDisplayOptionsParameter.Bind(Initializer.ParameterMap,TEXT("LightMapDensityDisplayOptions"),TRUE);
		VertexMappedColorParameter.Bind(Initializer.ParameterMap,TEXT("VertexMappedColor"),TRUE);
		GridTextureParameter.Bind(Initializer.ParameterMap,TEXT("GridTexture"),TRUE);
	}
	TLightMapDensityPixelShader() {}

	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView* View)
	{
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), View->Family->CurrentWorldTime, View->Family->CurrentRealTime, View);
		MaterialParameters.Set(this,MaterialRenderContext);
		if (GridTextureParameter.IsBound())
		{
			SetTextureParameter(
				GetPixelShader(),
				GridTextureParameter,
				TStaticSamplerState<SF_Bilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
				GEngine->LightMapDensityTexture->Resource->TextureRHI);
		}
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View,UBOOL bBackFace,FVector& InBuiltLightingAndSelectedFlags,FVector2D& InLightMapResolutionScale, UBOOL bTextureMapped)
	{
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace);
		if (LightMapDensityParameter.IsBound())
		{
			FVector4 DensityParameters(
				1,
				GEngine->MinLightMapDensity * GEngine->MinLightMapDensity,
				GEngine->IdealLightMapDensity * GEngine->IdealLightMapDensity,
				GEngine->MaxLightMapDensity * GEngine->MaxLightMapDensity );
			SetPixelShaderValue(GetPixelShader(),LightMapDensityParameter,DensityParameters,0);
		}
		if (BuiltLightingAndSelectedFlagsParameter.IsBound())
		{
			SetPixelShaderValue(GetPixelShader(),BuiltLightingAndSelectedFlagsParameter,InBuiltLightingAndSelectedFlags,0);
		}
		if (DensitySelectedColorParameter.IsBound())
		{
			SetPixelShaderValue(GetPixelShader(),DensitySelectedColorParameter,GEngine->LightMapDensitySelectedColor,0);
		}
		if (LightMapResolutionScaleParameter.IsBound())
		{
			SetPixelShaderValue(GetPixelShader(),LightMapResolutionScaleParameter,InLightMapResolutionScale,0);
		}
		if (LightMapDensityDisplayOptionsParameter.IsBound())
		{
			FVector4 OptionsParameter(
				GEngine->bRenderLightMapDensityGrayscale ? GEngine->RenderLightMapDensityGrayscaleScale : 0.0f,
				GEngine->bRenderLightMapDensityGrayscale ? 0.0f : GEngine->RenderLightMapDensityColorScale,
				(bTextureMapped == TRUE) ? 1.0f : 0.0f,
				(bTextureMapped == FALSE) ? 1.0f : 0.0f
				);
			SetPixelShaderValue(GetPixelShader(),LightMapDensityDisplayOptionsParameter,OptionsParameter,0);
		}
		if (VertexMappedColorParameter.IsBound())
		{
			SetPixelShaderValue(GetPixelShader(),VertexMappedColorParameter,GEngine->LightMapDensityVertexMappedColor,0);
		}
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
#if PS3
		//@hack - compiler bug? optimized version crashes during FShader::Serialize call
		static INT RemoveMe=0;	RemoveMe=1;
#endif
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		LightMapPolicyType::PixelParametersType::Serialize(Ar);
		Ar << MaterialParameters;
		Ar << LightMapDensityParameter;
		Ar << BuiltLightingAndSelectedFlagsParameter;
		Ar << DensitySelectedColorParameter;
		Ar << LightMapResolutionScaleParameter;
		Ar << LightMapDensityDisplayOptionsParameter;
		Ar << VertexMappedColorParameter;
		Ar << GridTextureParameter;
		return bShaderHasOutdatedParameters;
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialPixelShaderParameters MaterialParameters;
 	FShaderParameter LightMapDensityParameter;
	FShaderParameter BuiltLightingAndSelectedFlagsParameter;
	FShaderParameter DensitySelectedColorParameter;
	FShaderParameter LightMapResolutionScaleParameter;
	FShaderParameter LightMapDensityDisplayOptionsParameter;
	FShaderParameter VertexMappedColorParameter;
 	FShaderResourceParameter GridTextureParameter;
// 	FShaderResourceParameter GridNormalTextureParameter;
};

/**
 * 
 */
template<typename LightMapPolicyType>
class TLightMapDensityDrawingPolicy : public FMeshDrawingPolicy
{
public:
	/** The data the drawing policy uses for each mesh element. */
	class ElementDataType
	{
	public:

		/** The element's light-map data. */
		typename LightMapPolicyType::ElementDataType LightMapElementData;

		/** Default constructor. */
		ElementDataType()
		{}

		/** Initialization constructor. */
		ElementDataType(
			const typename LightMapPolicyType::ElementDataType& InLightMapElementData
			):
			LightMapElementData(InLightMapElementData)
		{}
	};

	/** Initialization constructor. */
	TLightMapDensityDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		LightMapPolicyType InLightMapPolicy,
		EBlendMode InBlendMode
		):
		FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy, *InMaterialRenderProxy->GetMaterial()),
		LightMapPolicy(InLightMapPolicy),
		BlendMode(InBlendMode)
	{
		const FMaterial* MaterialResource = InMaterialRenderProxy->GetMaterial();

#if WITH_D3D11_TESSELLATION
		HullShader = NULL;
		DomainShader = NULL;

		const EMaterialTessellationMode MaterialTessellationMode = MaterialResource->GetD3D11TessellationMode();
		if(GRHIShaderPlatform == SP_PCD3D_SM5
			&& InVertexFactory->GetType()->SupportsTessellationShaders() 
			&& MaterialTessellationMode != MTM_NoTessellation)
		{
			HullShader = MaterialResource->GetShader<TLightMapDensityHullShader<LightMapPolicyType> >(VertexFactory->GetType());
			DomainShader = MaterialResource->GetShader<TLightMapDensityDomainShader<LightMapPolicyType> >(VertexFactory->GetType());
		}
#endif
		VertexShader = MaterialResource->GetShader<TLightMapDensityVertexShader<LightMapPolicyType> >(InVertexFactory->GetType());
		PixelShader = MaterialResource->GetShader<TLightMapDensityPixelShader<LightMapPolicyType> >(InVertexFactory->GetType());
	}

	// FMeshDrawingPolicy interface.
	UBOOL Matches(const TLightMapDensityDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) &&
			VertexShader == Other.VertexShader &&
			PixelShader == Other.PixelShader &&
#if WITH_D3D11_TESSELLATION
			HullShader == Other.HullShader &&
			DomainShader == Other.DomainShader &&
#endif
			LightMapPolicy == Other.LightMapPolicy;
	}

	void DrawShared( const FSceneView* View, FBoundShaderStateRHIRef ShaderState ) const
	{
		// Set the base pass shader parameters for the material.
		VertexShader->SetParameters(VertexFactory,MaterialRenderProxy,*View);
		PixelShader->SetParameters(VertexFactory,MaterialRenderProxy,View);

#if WITH_D3D11_TESSELLATION
		if(HullShader && DomainShader)
		{
			HullShader->SetParameters(MaterialRenderProxy,*View);
			DomainShader->SetParameters(MaterialRenderProxy,*View);
		}
#endif

		RHISetBlendState(TStaticBlendState<>::GetRHI());

		// Set the light-map policy.
		LightMapPolicy.Set(VertexShader,PixelShader,VertexShader,PixelShader,VertexFactory,MaterialRenderProxy,View);

		// Set the actual shader & vertex declaration state
		RHISetBoundShaderState(ShaderState);
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
		VertexShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);

#if WITH_D3D11_TESSELLATION
		if(HullShader && DomainShader)
		{
			HullShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
			DomainShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
		}
#endif

		// Set the light-map policy's mesh-specific settings.
		LightMapPolicy.SetMesh(View,PrimitiveSceneInfo,VertexShader,PixelShader,VertexShader,PixelShader,VertexFactory,MaterialRenderProxy,ElementData.LightMapElementData);

		// BuiltLightingAndSelectedFlags informs the shader is lighting is built or not for this primitive
		FVector BuiltLightingAndSelectedFlags(0.0f,0.0f,0.0f);
		// LMResolutionScale is the physical resolution of the lightmap texture
		FVector2D LMResolutionScale(1.0f,1.0f);

		UBOOL bTextureMapped = FALSE;
		if (Mesh.LCI &&
			(Mesh.LCI->GetLightMapInteraction().GetType() == LMIT_Texture) &&
			Mesh.LCI->GetLightMapInteraction().GetTexture(0))
		{
			LMResolutionScale.X = Mesh.LCI->GetLightMapInteraction().GetTexture(0)->SizeX;
			LMResolutionScale.Y = Mesh.LCI->GetLightMapInteraction().GetTexture(0)->SizeY;
			bTextureMapped = TRUE;

			BuiltLightingAndSelectedFlags.X = 1.0f;
			BuiltLightingAndSelectedFlags.Y = 0.0f;
		}
		else
 		if (PrimitiveSceneInfo && PrimitiveSceneInfo->Proxy)
 		{
 			LMResolutionScale = PrimitiveSceneInfo->Proxy->GetLightMapResolutionScale();
			BuiltLightingAndSelectedFlags.X = 0.0f;
			BuiltLightingAndSelectedFlags.Y = 1.0f;
			if (PrimitiveSceneInfo->Proxy->GetLightMapType() == LMIT_Texture)
			{
				if (PrimitiveSceneInfo->Proxy->IsLightMapResolutionPadded() == TRUE)
				{
					LMResolutionScale.X -= 2.0f;
					LMResolutionScale.Y -= 2.0f;
				}
				bTextureMapped = TRUE;
				if (PrimitiveSceneInfo->Component->IsA(UStaticMeshComponent::StaticClass()))
				{
					BuiltLightingAndSelectedFlags.X = 1.0f;
					BuiltLightingAndSelectedFlags.Y = 0.0f;
				}
			}
		}

		if (Mesh.MaterialRenderProxy && (Mesh.MaterialRenderProxy->IsSelected() == TRUE))
		{
			BuiltLightingAndSelectedFlags.Z = 1.0f;
		}
		else
		{
			BuiltLightingAndSelectedFlags.Z = 0.0f;
		}

		// Adjust for the grid texture being 2x2 repeating pattern...
		LMResolutionScale *= 0.5f;
		PixelShader->SetMesh(PrimitiveSceneInfo, Mesh, BatchElementIndex, View, bBackFace, BuiltLightingAndSelectedFlags, LMResolutionScale, bTextureMapped);	
		FMeshDrawingPolicy::SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,FMeshDrawingPolicy::ElementDataType());
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

		FBoundShaderStateRHIRef BoundShaderState;

	#if WITH_D3D11_TESSELLATION
		BoundShaderState = RHICreateBoundShaderStateD3D11(
			VertexDeclaration, 
			StreamStrides, 
			VertexShader->GetVertexShader(),
			GETSAFERHISHADER_HULL(HullShader), 
			GETSAFERHISHADER_DOMAIN(DomainShader),
			PixelShader->GetPixelShader(),
			FGeometryShaderRHIRef(),
			EGST_None);
	#else
		BoundShaderState = RHICreateBoundShaderState(VertexDeclaration, StreamStrides, VertexShader->GetVertexShader(), PixelShader->GetPixelShader(), EGST_None);
	#endif

		return BoundShaderState;
	}

	friend INT Compare(const TLightMapDensityDrawingPolicy& A,const TLightMapDensityDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
		COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
#if WITH_D3D11_TESSELLATION
		COMPAREDRAWINGPOLICYMEMBERS(HullShader);
		COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
#endif
		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		return Compare(A.LightMapPolicy,B.LightMapPolicy);
	}

private:
	TLightMapDensityVertexShader<LightMapPolicyType>* VertexShader;
	TLightMapDensityPixelShader<LightMapPolicyType>* PixelShader;

#if WITH_D3D11_TESSELLATION
	TLightMapDensityHullShader<LightMapPolicyType>* HullShader;
	TLightMapDensityDomainShader<LightMapPolicyType>* DomainShader;
#endif

	LightMapPolicyType LightMapPolicy;
	EBlendMode BlendMode;
};

/**
 * A drawing policy factory for rendering lightmap density.
 */
class FLightMapDensityDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = FALSE };
	struct ContextType {};

	static UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);
	static UBOOL IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy)
	{
		return (MaterialRenderProxy && 
				!(	MaterialRenderProxy->GetMaterial()->IsSpecialEngineMaterial() || 
					MaterialRenderProxy->GetMaterial()->IsMasked() ||
					MaterialRenderProxy->GetMaterial()->MaterialModifiesMeshPosition()
				));
	}
};
