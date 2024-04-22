/*=============================================================================
	DistortionRendering.cpp: Distortion rendering implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"

/** Controls whether distortion is applied to the minimal screen extents. False applys distortion to the entire screen */
UBOOL GRenderMinimalDistortion = TRUE;

/**
* A vertex shader for rendering the full screen distortion pass
*/
class FDistortionApplyScreenVertexShader : public FShader
{
	DECLARE_SHADER_TYPE(FDistortionApplyScreenVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	FDistortionApplyScreenVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FShader(Initializer)
	{
		TransformParameter.Bind(Initializer.ParameterMap,TEXT("Transform"));
	}
	FDistortionApplyScreenVertexShader() {}

	void SetParameters(const FMatrix& Transform)
	{
		SetVertexShaderValue(GetVertexShader(),TransformParameter,Transform);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << TransformParameter;
		return bShaderHasOutdatedParameters;
	}

private:	
	FShaderParameter TransformParameter;
};

/** distortion apply screen vertex shader implementation */
IMPLEMENT_SHADER_TYPE(,FDistortionApplyScreenVertexShader,TEXT("DistortApplyScreenVertexShader"),TEXT("Main"),SF_Vertex,0,0);

/**
* A pixel shader for rendering the full screen distortion pass
*/
class FDistortionApplyScreenPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FDistortionApplyScreenPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	FDistortionApplyScreenPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FShader(Initializer)
	{
		AccumulatedDistortionTextureParam.Bind(Initializer.ParameterMap,TEXT("AccumulatedDistortionTexture"));
		SceneColorTextureParam.Bind(Initializer.ParameterMap,TEXT("SceneColorTexture"));
		SceneColorRectParameter.Bind(Initializer.ParameterMap,TEXT("SceneColorRect"));
	}
	FDistortionApplyScreenPixelShader() {}

	void SetParameters(FTextureRHIParamRef AccumulatedDistortionTexture,FTextureRHIParamRef SceneColorTexture, const FVector4& SceneColorRect )
	{
		// Here we use SF_Point as in fullscreen the pixels are 1:1 mapped.
		// Seems in split screen there is a half texel shift but with SF_Point even that disappears.
		SetTextureParameter(
			GetPixelShader(),
			AccumulatedDistortionTextureParam,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			AccumulatedDistortionTexture
			);

		if (GSystemSettings.bAllowFilteredDistortion)
		{
			SetTextureParameter(
				GetPixelShader(),
				SceneColorTextureParam,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				SceneColorTexture
				);
		}
		else
		{
			SetTextureParameter(
				GetPixelShader(),
				SceneColorTextureParam,
				TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				SceneColorTexture
				);
		}		
		SetPixelShaderValue(
			GetPixelShader(),
			SceneColorRectParameter,
			SceneColorRect
			);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
#if PS3
		//@hack - compiler bug? optimized version crashes during FShader::Serialize call
		static INT RemoveMe=0;	RemoveMe=1;
#endif
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << AccumulatedDistortionTextureParam;
		Ar << SceneColorTextureParam;
		Ar << SceneColorRectParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter AccumulatedDistortionTextureParam;
	FShaderResourceParameter SceneColorTextureParam;
	FShaderParameter SceneColorRectParameter;
};

/** distortion apply screen pixel shader implementation */
IMPLEMENT_SHADER_TYPE(,FDistortionApplyScreenPixelShader,TEXT("DistortApplyScreenPixelShader"),TEXT("Main"),SF_Pixel,VER_DISTORTIONEFFECT2,0);

/**
* Base draw policy for distort meshes
*/
class FDistortMeshPolicy
{
public:
	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
 		return Material &&
 			IsTranslucentBlendMode(Material->GetBlendMode()) &&
 			Material->IsDistorted();
	}
};

/**
* Policy for drawing distortion mesh accumulated offsets
*/
class FDistortMeshAccumulatePolicy : public FDistortMeshPolicy
{	
};

/**
* A vertex shader for rendering distortion meshes
*/
template<class DistortMeshPolicy>
class TDistortionMeshVertexShader : public FMeshMaterialVertexShader
{
	DECLARE_SHADER_TYPE(TDistortionMeshVertexShader,MeshMaterial);

protected:

	TDistortionMeshVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialVertexShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
	}

	TDistortionMeshVertexShader()
	{
	}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return DistortMeshPolicy::ShouldCache(Platform,Material,VertexFactoryType);
	}

public:

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
		Ar << MaterialParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView* View)
	{
		VertexFactoryParameters.Set(this,VertexFactory,*View);
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), View->Family->CurrentWorldTime, View->Family->CurrentRealTime, View);
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
 * A hull shader for rendering distortion meshes
 */
template<class DistortMeshPolicy>
class TDistortionMeshHullShader : public FBaseHullShader
{
	DECLARE_SHADER_TYPE(TDistortionMeshHullShader,MeshMaterial);

protected:

	TDistortionMeshHullShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseHullShader(Initializer)
	{}

	TDistortionMeshHullShader() {}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHullShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& DistortMeshPolicy::ShouldCache(Platform, Material, VertexFactoryType);
	}
};

/**
 * A domain shader for rendering distortion meshes
 */
template<class DistortMeshPolicy>
class TDistortionMeshDomainShader : public FBaseDomainShader
{
	DECLARE_SHADER_TYPE(TDistortionMeshDomainShader,MeshMaterial);

protected:

	TDistortionMeshDomainShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseDomainShader(Initializer)
	{}

	TDistortionMeshDomainShader() {}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDomainShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& DistortMeshPolicy::ShouldCache(Platform, Material, VertexFactoryType);
	}
};

#endif

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDistortionMeshVertexShader<FDistortMeshAccumulatePolicy>,TEXT("DistortAccumulateVertexShader"),TEXT("Main"),SF_Vertex,0,0); 

#if WITH_D3D11_TESSELLATION
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDistortionMeshHullShader<FDistortMeshAccumulatePolicy>,TEXT("DistortAccumulateVertexShader"),TEXT("MainHull"),SF_Hull,0,0); 
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDistortionMeshDomainShader<FDistortMeshAccumulatePolicy>,TEXT("DistortAccumulateVertexShader"),TEXT("MainDomain"),SF_Domain,0,0);
#endif


/**
* A pixel shader to render distortion meshes
*/
template<class DistortMeshPolicy>
class TDistortionMeshPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(TDistortionMeshPixelShader,MeshMaterial);

public:
	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return DistortMeshPolicy::ShouldCache(Platform,Material,VertexFactoryType);
	}

	TDistortionMeshPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
	}

	TDistortionMeshPixelShader()
	{
	}

	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView* View)
	{
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), View->Family->CurrentWorldTime, View->Family->CurrentRealTime, View);
		MaterialParameters.Set(this,MaterialRenderContext);
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshBatch& Mesh,INT BatchElementIndex,const FSceneView& View,UBOOL bBackFace)
	{
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{		
#if PS3
		//@hack - compiler bug? optimized version crashes during FShader::Serialize call
		static INT RemoveMe=0;	RemoveMe=1;
#endif
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << MaterialParameters;
		return bShaderHasOutdatedParameters;
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialPixelShaderParameters MaterialParameters;
};

//** distortion accumulate pixel shader type implementation */
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDistortionMeshPixelShader<FDistortMeshAccumulatePolicy>,TEXT("DistortAccumulatePixelShader"),TEXT("Main"),SF_Pixel,0,0);

/*-----------------------------------------------------------------------------
TDistortionMeshDrawingPolicy
-----------------------------------------------------------------------------*/

/**
* Distortion mesh drawing policy
*/
template<class DistortMeshPolicy>
class TDistortionMeshDrawingPolicy : public FMeshDrawingPolicy
{
public:
	/** context type */
	typedef FMeshDrawingPolicy::ElementDataType ElementDataType;

	/**
	* Constructor
	* @param InIndexBuffer - index buffer for rendering
	* @param InVertexFactory - vertex factory for rendering
	* @param InMaterialRenderProxy - material instance for rendering
	* @param bInOverrideWithShaderComplexity - whether to override with shader complexity
	*/
	TDistortionMeshDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& MaterialResouce,
		UBOOL bInitializeOffsets,
		UBOOL bInOverrideWithShaderComplexity
		);

	// FMeshDrawingPolicy interface.

	/**
	* Match two draw policies
	* @param Other - draw policy to compare
	* @return TRUE if the draw policies are a match
	*/
	UBOOL Matches(const TDistortionMeshDrawingPolicy& Other) const;

	/**
	* Executes the draw commands which can be shared between any meshes using this drawer.
	* @param CI - The command interface to execute the draw commands on.
	* @param View - The view of the scene being drawn.
	*/
	void DrawShared(const FSceneView* View,FBoundShaderStateRHIParamRef BoundShaderState) const;
	
	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
	* @return new bound shader state object
	*/
	FBoundShaderStateRHIRef CreateBoundShaderState(DWORD DynamicStride = 0);

	/**
	* Sets the render states for drawing a mesh.
	* @param PrimitiveSceneInfo - The primitive drawing the dynamic mesh.  If this is a view element, this will be NULL.
	* @param Mesh - mesh element with data needed for rendering
	* @param ElementData - context specific data for mesh rendering
	*/
	void SetMeshRenderState(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		UBOOL bBackFace,
		const ElementDataType& ElementData
		) const;

private:
	/** vertex shader based on policy type */
	TDistortionMeshVertexShader<DistortMeshPolicy>* VertexShader;

#if WITH_D3D11_TESSELLATION
	TDistortionMeshHullShader<DistortMeshPolicy>* HullShader;
	TDistortionMeshDomainShader<DistortMeshPolicy>* DomainShader;
#endif

	/** whether we are initializing offsets or accumulating them */
	UBOOL bInitializeOffsets;
	/** pixel shader based on policy type */
	TDistortionMeshPixelShader<DistortMeshPolicy>* DistortPixelShader;
	/** pixel shader used to initialize offsets */
	FShaderComplexityAccumulatePixelShader* InitializePixelShader;
};

/**
* Constructor
* @param InIndexBuffer - index buffer for rendering
* @param InVertexFactory - vertex factory for rendering
* @param InMaterialRenderProxy - material instance for rendering
*/
template<class DistortMeshPolicy> 
TDistortionMeshDrawingPolicy<DistortMeshPolicy>::TDistortionMeshDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	UBOOL bInInitializeOffsets,
	UBOOL bInOverrideWithShaderComplexity
	)
:	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource,bInOverrideWithShaderComplexity)
,	bInitializeOffsets(bInInitializeOffsets)
{
#if WITH_D3D11_TESSELLATION
	HullShader = NULL;
	DomainShader = NULL;

	const EMaterialTessellationMode MaterialTessellationMode = InMaterialResource.GetD3D11TessellationMode();
	if(GRHIShaderPlatform == SP_PCD3D_SM5
		&& InVertexFactory->GetType()->SupportsTessellationShaders() 
		&& MaterialTessellationMode != MTM_NoTessellation)
	{
		HullShader = InMaterialResource.GetShader<TDistortionMeshHullShader<DistortMeshPolicy> >(VertexFactory->GetType());
		DomainShader = InMaterialResource.GetShader<TDistortionMeshDomainShader<DistortMeshPolicy> >(VertexFactory->GetType());
	}
#endif

	VertexShader = InMaterialResource.GetShader<TDistortionMeshVertexShader<DistortMeshPolicy> >(InVertexFactory->GetType());

	if (bInitializeOffsets)
	{
		InitializePixelShader = GetGlobalShaderMap()->GetShader<FShaderComplexityAccumulatePixelShader>();
		DistortPixelShader = NULL;
	}
	else
	{
		DistortPixelShader = InMaterialResource.GetShader<TDistortionMeshPixelShader<DistortMeshPolicy> >(InVertexFactory->GetType());
		InitializePixelShader = NULL;
	}
}

/**
* Match two draw policies
* @param Other - draw policy to compare
* @return TRUE if the draw policies are a match
*/
template<class DistortMeshPolicy>
UBOOL TDistortionMeshDrawingPolicy<DistortMeshPolicy>::Matches(
	const TDistortionMeshDrawingPolicy& Other
	) const
{
	return FMeshDrawingPolicy::Matches(Other) &&
		VertexShader == Other.VertexShader &&
#if WITH_D3D11_TESSELLATION
		HullShader == Other.HullShader &&
		DomainShader == Other.DomainShader &&
#endif
		bInitializeOffsets == Other.bInitializeOffsets &&
		DistortPixelShader == Other.DistortPixelShader &&
		InitializePixelShader == Other.InitializePixelShader;
}

/**
* Executes the draw commands which can be shared between any meshes using this drawer.
* @param CI - The command interface to execute the draw commands on.
* @param View - The view of the scene being drawn.
*/
template<class DistortMeshPolicy>
void TDistortionMeshDrawingPolicy<DistortMeshPolicy>::DrawShared(
	const FSceneView* View,
	FBoundShaderStateRHIParamRef BoundShaderState
	) const
{
	// Set the translucent shader parameters for the material instance
	VertexShader->SetParameters(VertexFactory,MaterialRenderProxy,View);

#if WITH_D3D11_TESSELLATION
	if(HullShader && DomainShader)
	{
		HullShader->SetParameters(MaterialRenderProxy,*View);
		DomainShader->SetParameters(MaterialRenderProxy,*View);
	}
#endif

#if !FINAL_RELEASE
	if (bOverrideWithShaderComplexity)
	{
		checkSlow(!bInitializeOffsets);
		TShaderMapRef<FShaderComplexityAccumulatePixelShader> ShaderComplexityPixelShader(GetGlobalShaderMap());
		//don't add any vertex complexity
		ShaderComplexityPixelShader->SetParameters(0, DistortPixelShader->GetNumInstructions());
	}
	else
#endif
	{
		if (bInitializeOffsets)
		{
			InitializePixelShader->SetParameters(0, 0);
		}
		else
		{
			DistortPixelShader->SetParameters(VertexFactory,MaterialRenderProxy,View);
		}
	}
	
	// Set shared mesh resources
	FMeshDrawingPolicy::DrawShared(View);

	// Set the actual shader & vertex declaration state
	RHISetBoundShaderState(BoundShaderState);
}

/** 
* Create bound shader state using the vertex decl from the mesh draw policy
* as well as the shaders needed to draw the mesh
* @param DynamicStride - optional stride for dynamic vertex data
* @return new bound shader state object
*/
template<class DistortMeshPolicy>
FBoundShaderStateRHIRef TDistortionMeshDrawingPolicy<DistortMeshPolicy>::CreateBoundShaderState(DWORD DynamicStride)
{
	FVertexDeclarationRHIRef VertexDeclaration;
	DWORD StreamStrides[MaxVertexElementCount];

	FMeshDrawingPolicy::GetVertexDeclarationInfo(VertexDeclaration, StreamStrides);
	if (DynamicStride)
	{
		StreamStrides[0] = DynamicStride;
	}

	FPixelShaderRHIParamRef PixelShaderRHIRef = NULL;

#if !FINAL_RELEASE
	if (bOverrideWithShaderComplexity)
	{
		checkSlow(!bInitializeOffsets);
		TShaderMapRef<FShaderComplexityAccumulatePixelShader> ShaderComplexityAccumulatePixelShader(GetGlobalShaderMap());
		PixelShaderRHIRef = ShaderComplexityAccumulatePixelShader->GetPixelShader();
	}
#endif

	if (bInitializeOffsets)
	{
		PixelShaderRHIRef = InitializePixelShader->GetPixelShader();
	}
	else
	{
		PixelShaderRHIRef = DistortPixelShader->GetPixelShader();
	}

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

/**
* Sets the render states for drawing a mesh.
* @param PrimitiveSceneInfo - The primitive drawing the dynamic mesh.  If this is a view element, this will be NULL.
* @param Mesh - mesh element with data needed for rendering
* @param ElementData - context specific data for mesh rendering
*/
template<class DistortMeshPolicy>
void TDistortionMeshDrawingPolicy<DistortMeshPolicy>::SetMeshRenderState(
	const FSceneView& View,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FMeshBatch& Mesh,
	INT BatchElementIndex,
	UBOOL bBackFace,
	const ElementDataType& ElementData
	) const
{
	EmitMeshDrawEvents(PrimitiveSceneInfo, Mesh);

	// Set transforms
	VertexShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);

#if WITH_D3D11_TESSELLATION
	if(HullShader && DomainShader)
	{
		HullShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
		DomainShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
	}
#endif

#if !FINAL_RELEASE
	// Don't set pixel shader constants if we are overriding with the shader complexity pixel shader
	if (!bOverrideWithShaderComplexity)
#endif
	{
		if (!bInitializeOffsets)
		{
			DistortPixelShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace);
		}
	}
	
	// Set rasterizer state.
	const FRasterizerStateInitializerRHI Initializer = {
		(Mesh.bWireframe || IsWireframe()) ? FM_Wireframe : FM_Solid,
		IsTwoSided() ? CM_None : (XOR( XOR(View.bReverseCulling,bBackFace), Mesh.ReverseCulling) ? CM_CCW : CM_CW),
		Mesh.DepthBias,
		Mesh.SlopeScaleDepthBias,
		TRUE
	};
	RHISetRasterizerStateImmediate( Initializer);
}

/*-----------------------------------------------------------------------------
TDistortionMeshDrawingPolicyFactory
-----------------------------------------------------------------------------*/

/**
* Distortion mesh draw policy factory. 
* Creates the policies needed for rendering a mesh based on its material
*/
template<class DistortMeshPolicy>
class TDistortionMeshDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = FALSE };
	typedef UBOOL ContextType;

	/**
	* Render a dynamic mesh using a distortion mesh draw policy 
	* @return TRUE if the mesh rendered
	*/
	static UBOOL DrawDynamicMesh(
		const FSceneView& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);

	/**
	* Render a dynamic mesh using a distortion mesh draw policy 
	* @return TRUE if the mesh rendered
	*/
	static UBOOL DrawStaticMesh(
		const FSceneView* View,
		ContextType DrawingContext,
		const FStaticMesh& StaticMesh,
		UBOOL bBackFace,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);

	static UBOOL IsMaterialIgnored(const FMaterialRenderProxy* Material);
};

/**
* Render a dynamic mesh using a distortion mesh draw policy 
* @return TRUE if the mesh rendered
*/
template<class DistortMeshPolicy>
UBOOL TDistortionMeshDrawingPolicyFactory<DistortMeshPolicy>::DrawDynamicMesh(
	const FSceneView& View,
	ContextType bInitializeOffsets,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId
	)
{
	if(Mesh.IsDistortion() && !bBackFace)
	{
		// draw dynamic mesh element using distortion mesh policy
		TDistortionMeshDrawingPolicy<DistortMeshPolicy> DrawingPolicy(
			Mesh.VertexFactory,
			Mesh.MaterialRenderProxy,
			*Mesh.MaterialRenderProxy->GetMaterial(),
			bInitializeOffsets,
			(View.Family->ShowFlags & SHOW_ShaderComplexity) != 0
			);
		DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
		for( INT BatchElementIndex=0;BatchElementIndex < Mesh.Elements.Num();BatchElementIndex++ )
		{
			DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,typename TDistortionMeshDrawingPolicy<DistortMeshPolicy>::ElementDataType());
			DrawingPolicy.DrawMesh(Mesh,BatchElementIndex);
		}

		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/**
* Render a dynamic mesh using a distortion mesh draw policy 
* @return TRUE if the mesh rendered
*/
template<class DistortMeshPolicy>
UBOOL TDistortionMeshDrawingPolicyFactory<DistortMeshPolicy>::DrawStaticMesh(
	const FSceneView* View,
	ContextType bInitializeOffsets,
	const FStaticMesh& StaticMesh,
	UBOOL bBackFace,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId
	)
{
	if(StaticMesh.IsDistortion())
	{
		// draw static mesh element using distortion mesh policy
		TDistortionMeshDrawingPolicy<DistortMeshPolicy> DrawingPolicy(
			StaticMesh.VertexFactory,
			StaticMesh.MaterialRenderProxy,
			*StaticMesh.MaterialRenderProxy->GetMaterial(),
			bInitializeOffsets,
			(View->Family->ShowFlags & SHOW_ShaderComplexity) != 0
			);
		DrawingPolicy.DrawShared(View,DrawingPolicy.CreateBoundShaderState());
		for( INT BatchElementIndex=0; BatchElementIndex < StaticMesh.Elements.Num(); BatchElementIndex++ )
		{
			DrawingPolicy.SetMeshRenderState(*View,PrimitiveSceneInfo,StaticMesh,BatchElementIndex,bBackFace,typename TDistortionMeshDrawingPolicy<DistortMeshPolicy>::ElementDataType());
			DrawingPolicy.DrawMesh(StaticMesh, BatchElementIndex);
		}

		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

template<typename DistortMeshPolicy>
UBOOL TDistortionMeshDrawingPolicyFactory<DistortMeshPolicy>::IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy)
{
	// Non-distorted materials are ignored when rendering distortion.
	return MaterialRenderProxy && !MaterialRenderProxy->GetMaterial()->IsDistorted();
}

/*-----------------------------------------------------------------------------
	FDistortionPrimSet
-----------------------------------------------------------------------------*/

/** 
* Iterate over the distortion prims and draw their accumulated offsets
* @param ViewInfo - current view used to draw items
* @param DPGIndex - current DPG used to draw items
* @return TRUE if anything was drawn
*/
UBOOL FDistortionPrimSet::DrawAccumulatedOffsets(const FViewInfo* ViewInfo,UINT DPGIndex,UBOOL bInitializeOffsets)
{
	UBOOL bDirty=FALSE;

	// Draw the view's elements with the translucent drawing policy.
	bDirty |= DrawViewElements<TDistortionMeshDrawingPolicyFactory<FDistortMeshAccumulatePolicy> >(
		*ViewInfo,
		bInitializeOffsets,
		DPGIndex,
		FALSE // Distortion is rendered post fog.
		);

	if( Prims.Num() )
	{
		// For drawing scene prims with dynamic relevance.
		TDynamicPrimitiveDrawer<TDistortionMeshDrawingPolicyFactory<FDistortMeshAccumulatePolicy> > Drawer(
			ViewInfo,
			DPGIndex,
			bInitializeOffsets,
			FALSE // Distortion is rendered post fog.
			);

		// Draw sorted scene prims
		for( INT PrimIdx=0; PrimIdx < Prims.Num(); PrimIdx++ )
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = Prims(PrimIdx);
			const FPrimitiveViewRelevance& ViewRelevance = ViewInfo->PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);

			if( ViewRelevance.bDistortionRelevance )
			{
				// Render dynamic scene prim
				if( ViewRelevance.bDynamicRelevance )
				{				
					Drawer.SetPrimitive(PrimitiveSceneInfo);
					PrimitiveSceneInfo->Proxy->DrawDynamicElements(
						&Drawer,
						ViewInfo,
						DPGIndex
						);
				}
				// Render static scene prim
				if( ViewRelevance.bStaticRelevance )
				{
					// Render static meshes from static scene prim
					for( INT StaticMeshIdx=0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++ )
					{
						FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes(StaticMeshIdx);
						if( ViewInfo->StaticMeshVisibilityMap(StaticMesh.Id)
							// Only render static mesh elements using translucent materials
							&& StaticMesh.IsTranslucent() )
						{
							bDirty |= TDistortionMeshDrawingPolicyFactory<FDistortMeshAccumulatePolicy>::DrawStaticMesh(
								ViewInfo,
								bInitializeOffsets,
								StaticMesh,
								FALSE,
								PrimitiveSceneInfo,
								StaticMesh.HitProxyId
								);
						}
					}
				}
			}

			// render distortion offsets for decals
			if( ViewRelevance.IsDecalRelevant() )
			{
				SCOPE_CYCLE_COUNTER(STAT_DecalRenderUnlitTime);

				// render dynamic translucent decals on translucent receivers
				if( ViewRelevance.bDecalDynamicRelevance )
				{
					UBOOL bDrawOpaqueDecals = FALSE;
					UBOOL bDrawTransparentDecals = TRUE;

					Drawer.SetPrimitive(PrimitiveSceneInfo);
					PrimitiveSceneInfo->Proxy->DrawDynamicDecalElements(
						&Drawer,
						ViewInfo,
						DPGIndex,
						FALSE,
						bDrawOpaqueDecals,
						bDrawTransparentDecals,
						ViewRelevance.bTranslucentRelevance
						);
				}
				// render static translucent decals on translucent receivers
				if( ViewRelevance.bDecalStaticRelevance )
				{
					// Render static meshes from static scene prim
					for( INT DecalIdx=0; DecalIdx < PrimitiveSceneInfo->Proxy->Decals[FPrimitiveSceneProxy::STATIC_DECALS].Num(); DecalIdx++ )
					{
						FDecalInteraction* Decal = PrimitiveSceneInfo->Proxy->Decals[FPrimitiveSceneProxy::STATIC_DECALS](DecalIdx);
						if( Decal && 
							ViewInfo->DecalStaticMeshVisibilityMap(Decal->DecalStaticMesh->Id) &&
							Decal->DecalStaticMesh->IsDistortion() &&
							Decal->DecalStaticMesh->IsTranslucent() )
						{
							bDirty |= TDistortionMeshDrawingPolicyFactory<FDistortMeshAccumulatePolicy>::DrawStaticMesh(
								ViewInfo,
								bInitializeOffsets,
								*Decal->DecalStaticMesh,
								FALSE,
								PrimitiveSceneInfo,
								Decal->DecalStaticMesh->HitProxyId
								);								
						}
					}
				}
			}
		}
		// Mark dirty if dynamic drawer rendered
		bDirty |= Drawer.IsDirty();
	}
	return bDirty;
}

FGlobalBoundShaderState FDistortionPrimSet::ApplyScreenBoundShaderState;
FGlobalBoundShaderState FDistortionPrimSet::ShaderComplexityTransferBoundShaderState;

/** 
* Convert a Min,Max screen rectangle region into normalized screen space quad vertices
* 
* @param QuadRect - Min,Max screen rectangle region
* @param BufferSizeX - width of the buffer
* @param BufferSizeY - height of the buffer
* @param Verts - [out] normalized screen space vertices generated 
*/
static void ScreenToViewspaceCoordsPos(const FIntRect& QuadRect, const FVector2D& BufferSize, const FVector2D& RTSize, const FVector2D& RTOffset, FBox& Verts)
{
	// Quad Dimensions
	const FLOAT QuadX1= (FLOAT)QuadRect.Min.X;
	const FLOAT QuadY1= (FLOAT)QuadRect.Min.Y;
	const FLOAT QuadX2= (FLOAT)QuadRect.Max.X;
	const FLOAT QuadY2= (FLOAT)QuadRect.Max.Y;

	// Vertices
	const FLOAT HalfPixelX= (-GPixelCenterOffset / BufferSize.X * +2.0f);
	const FLOAT HalfPixelY= (-GPixelCenterOffset / BufferSize.Y * -2.0f);
	const FLOAT VertX1= (((QuadX1 - RTOffset.X) / RTSize.X * +2.0f) - 1.0f) + HalfPixelX;
	const FLOAT VertX2= (((QuadX2 - RTOffset.X) / RTSize.X * +2.0f) - 1.0f) + HalfPixelX;
	const FLOAT VertY1= (((QuadY1 - RTOffset.Y) / RTSize.Y * -2.0f) + 1.0f) + HalfPixelY;
	const FLOAT VertY2= (((QuadY2 - RTOffset.Y) / RTSize.Y * -2.0f) + 1.0f) + HalfPixelY;

	Verts.Min= FVector(VertX1, VertY1, 0.0f);
	Verts.Max= FVector(VertX2, VertY2, 0.0f);
	Verts.IsValid= 1;
}

/** 
* Convert a Min,Max screen rectangle region into normalized screen space UVs
* 
* @param QuadRect - Min,Max screen rectangle region
* @param BufferSizeX - width of the buffer
* @param BufferSizeY - height of the buffer
* @param UVs - [out] UVs generated for sampling buffer
*/
static void ScreenToViewspaceCoordsUV(const FIntRect& QuadRect, const FVector2D& BufferSize, FBox& UVs)
{
	// Quad Dimensions
	const FLOAT QuadX1= (FLOAT)QuadRect.Min.X;
	const FLOAT QuadY1= (FLOAT)QuadRect.Min.Y;
	const FLOAT QuadX2= (FLOAT)QuadRect.Max.X;
	const FLOAT QuadY2= (FLOAT)QuadRect.Max.Y;

	// Quad UVs
	const FLOAT QuadU1= QuadX1 / BufferSize.X;
	const FLOAT QuadU2= QuadX2 / BufferSize.X;
	const FLOAT QuadV1= QuadY1 / BufferSize.Y;
	const FLOAT QuadV2= QuadY2 / BufferSize.Y;

	UVs.Min= FVector(QuadU1, QuadV1, 0.0f);
	UVs.Max= FVector(QuadU2, QuadV2, 0.0f);
	UVs.IsValid= 1;
}

/** 
* Apply distortion using the accumulated offsets as a fullscreen quad
* @param ViewInfo - current view used to draw items
* @param DPGIndex - current DPG used to draw items
* @param CanvasTransform - default canvas transform used by scene rendering
* @return TRUE if anything was drawn
*/
void FDistortionPrimSet::DrawScreenDistort(const class FViewInfo* ViewInfo,UINT DPGIndex,const FMatrix& CanvasTransform, const FIntRect& QuadRect, const FTexture2DRHIRef& SceneTexture)
{
	// Buffer size
	FVector2D BufferSize(GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());
	FVector2D RTSize(ViewInfo->RenderTargetSizeX, ViewInfo->RenderTargetSizeY);
	FVector2D RTOffset(ViewInfo->RenderTargetX, ViewInfo->RenderTargetY);

	// compute clamp rectangle to avoid leaking in colors from outside of the view
	// one texel smaller because of bilinear filtering 
	FVector4 SceneColorRect;
	{
		FBox ClampUV(0);
		ScreenToViewspaceCoordsUV(FIntRect(QuadRect.Min, QuadRect.Max - FIntPoint(1, 1)), BufferSize, ClampUV);

		SceneColorRect = FVector4(ClampUV.Min.X, ClampUV.Min.Y, ClampUV.Max.X, ClampUV.Max.Y);
	}

	TShaderMapRef<FDistortionApplyScreenVertexShader> VertexShader(GetGlobalShaderMap());
	check(*VertexShader);
	VertexShader->SetParameters(FMatrix::Identity);

	if ((ViewInfo->Family->ShowFlags & SHOW_ShaderComplexity) != 0)
	{
		// If the shader complexity viewmode is enabled then we have accumulated complexity in the light attenuation texture
		// Now apply that to scene color
		TShaderMapRef<FScreenPixelShader> PixelShader(GetGlobalShaderMap());
		FTexture AccumulatedShaderComplexityTexture;
		AccumulatedShaderComplexityTexture.TextureRHI = GSceneRenderTargets.GetLightAttenuationTexture();
		AccumulatedShaderComplexityTexture.SamplerStateRHI = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		PixelShader->SetParameters(&AccumulatedShaderComplexityTexture);
		SetGlobalBoundShaderState(ShaderComplexityTransferBoundShaderState, GSimpleElementVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FSimpleElementVertex));
	}
	else
	{
		// Distortion offsets have been accumulated in the light attenuation texture
		TShaderMapRef<FDistortionApplyScreenPixelShader> PixelShader(GetGlobalShaderMap());
		PixelShader->SetParameters(GSceneRenderTargets.GetLightAttenuationTexture(),SceneTexture, SceneColorRect);
		SetGlobalBoundShaderState(ApplyScreenBoundShaderState, GSimpleElementVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FSimpleElementVertex));
	}
	
	// set fill state
	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());

	// quad verts
	void* VertPtr=NULL;
	RHIBeginDrawPrimitiveUP(PT_TriangleStrip, 2, 4, sizeof(FSimpleElementVertex), VertPtr);
	check(VertPtr);
	FSimpleElementVertex* Vertices = (FSimpleElementVertex*)VertPtr;
	
	FBox Vert(0);
	ScreenToViewspaceCoordsPos(QuadRect, BufferSize, RTSize, RTOffset, Vert);

	FBox QuadUV(0);
	ScreenToViewspaceCoordsUV(QuadRect, BufferSize, QuadUV);

	Vertices[2] = FSimpleElementVertex(FVector4(Vert.Min.X,	Vert.Min.Y,	0,1),FVector2D(QuadUV.Min.X, QuadUV.Min.Y),	FColor(255,255,255),FHitProxyId());
	Vertices[3] = FSimpleElementVertex(FVector4(Vert.Max.X,	Vert.Min.Y,	0,1),FVector2D(QuadUV.Max.X, QuadUV.Min.Y),	FColor(255,255,255),FHitProxyId());		
	Vertices[1] = FSimpleElementVertex(FVector4(Vert.Max.X,	Vert.Max.Y,	0,1),FVector2D(QuadUV.Max.X, QuadUV.Max.Y),	FColor(255,255,255),FHitProxyId());
	Vertices[0] = FSimpleElementVertex(FVector4(Vert.Min.X,	Vert.Max.Y,	0,1),FVector2D(QuadUV.Min.X, QuadUV.Max.Y),	FColor(255,255,255),FHitProxyId());	

	// Draw the screen quad 
	RHIEndDrawPrimitiveUP();
}

/**
* Add a new primitive to the list of distortion prims
* @param PrimitiveSceneInfo - primitive info to add.
* @param ViewInfo - used to transform bounds to view space
*/
void FDistortionPrimSet::AddScenePrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo,const FViewInfo& ViewInfo)
{
	Prims.AddItem(PrimitiveSceneInfo);
}

/*-----------------------------------------------------------------------------
	FSceneRenderer
-----------------------------------------------------------------------------*/

/** 
 * Renders the scene's distortion 
 */
UBOOL FSceneRenderer::RenderDistortion(UINT DPGIndex)
{
	SCOPED_DRAW_EVENT(EventDistortion)(DEC_SCENE_ITEMS,TEXT("Distortion"));

	UBOOL bRender=FALSE;
	// Only render distortion if allowed by system settings
	if (GSystemSettings.bAllowDistortion)
	{
		for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views(ViewIndex);
			if( View.DistortionPrimSet[DPGIndex].NumPrims() > 0 )
			{
				bRender=TRUE;
				break;
			}
		}
	}

	UBOOL bDirty = FALSE;
	FResolveRect ResolveRect = FResolveRect();
	UBOOL bCustomResolveParams = FALSE;

#if XBOX
	// when enabled, determine the minimal screen space area to work in
	if( bRender)
	{
		if (GRenderMinimalDistortion)
		{
			FIntRect PixelRect;
			if (ComputeDistortionResolveRectangle(DPGIndex, PixelRect))
			{
				// update the custom resolve parameters
				ResolveRect.X1 = PixelRect.Min.X;
				ResolveRect.X2 = PixelRect.Max.X;
				ResolveRect.Y1 = PixelRect.Min.Y;
				ResolveRect.Y2 = PixelRect.Max.Y;
				bCustomResolveParams= TRUE;
			}
			else
			{
				// if no custom bounds were found, assume we do not need to do any further work
				bRender = FALSE;
			}
		}
	}
#endif

	// Render accumulated distortion offsets
	if( bRender)
	{
		// Save the color buffer as a raw surface
		GSceneRenderTargets.SaveSceneColorRaw(FALSE, ResolveRect);

		SCOPED_DRAW_EVENT(EventDistortionAccum)(DEC_SCENE_ITEMS,TEXT("Distortion Accum"));

		GSceneRenderTargets.BeginRenderingDistortionAccumulation();
		RHIBeginHiStencilRecord(TRUE, 0);

		for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENT(EventView,Views.Num() > 1)(DEC_SCENE_ITEMS,TEXT("View%d"),ViewIndex);

			FViewInfo& View = Views(ViewIndex);
			// viewport to match view size
			RHISetViewport(View.RenderTargetX,View.RenderTargetY,0.0f,View.RenderTargetX + View.RenderTargetSizeX,View.RenderTargetY + View.RenderTargetSizeY,1.0f);
			RHISetViewParameters(View);
			RHISetMobileHeightFogParams(View.HeightFogParams);

			// add scissor rect to match resolved area
			if (bCustomResolveParams)
			{
				// we do this out of paranoia to ensure our render is limited to the area we actually resolved
				RHISetScissorRect(TRUE, ResolveRect.X1, ResolveRect.Y1, ResolveRect.X2, ResolveRect.Y2);
			}

#if PS3
			// clear offsets to 0
			RHIClear(TRUE,FLinearColor(0,0,0,0),FALSE,0,FALSE,0);
#elif XBOX
			// don't clear offsets, clear stencil to 0.
			// the distortion accumulation buffer overlaps scene color, 
			// so we must not touch any pixels that will not be written in the apply pass
			RHIClear(FALSE,FLinearColor(0,0,0,0),FALSE,0,TRUE,0);

			// Set stencil to one.
			RHISetStencilState(TStaticStencilState<
				TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,
				FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
				0xff,0xff,1
			>::GetRHI());
#else
			// clear offsets to 0, stencil to 0
			RHIClear(TRUE,FLinearColor(0,0,0,0),FALSE,0,TRUE,0);

			// Set stencil to one.
			RHISetStencilState(TStaticStencilState<
				TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,
				FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
				0xff,0xff,1
				>::GetRHI());
#endif

			// enable depth test but disable depth writes
			RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());

#if XBOX
			{
				SCOPED_DRAW_EVENT(EventDistortionClear)(DEC_SCENE_ITEMS,TEXT("ClearDistortionOffsets"));
				// opaque blend
				RHISetBlendState(TStaticBlendState<>::GetRHI());

				// initialize offsets to 0
				// this can't be done with a clear since we are accumulating offsets on top of scene color,
				// only pixels that have an offset rendered will setup stencil and be written during the apply pass
				bDirty |= View.DistortionPrimSet[DPGIndex].DrawAccumulatedOffsets(&View,DPGIndex,TRUE);
			}
#endif

			// additive blending of offsets (or complexity if the shader complexity viewmode is enabled)
			RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_One,BF_One>::GetRHI());

			// draw only distortion meshes to accumulate their offsets
			bDirty |= View.DistortionPrimSet[DPGIndex].DrawAccumulatedOffsets(&View,DPGIndex,FALSE);

			// remove scissor rect used to match resolved area
			if (bCustomResolveParams)
			{
				RHISetScissorRect(FALSE, 0, 0, 0, 0);
			}

		}

		if (bDirty)
		{
			// resolve using the current ResolveParams 
			GSceneRenderTargets.FinishRenderingDistortionAccumulation(ResolveRect);
		}
	}


	if( bDirty )
	{
		SCOPED_DRAW_EVENT(EventDistortionApply)(DEC_SCENE_ITEMS,TEXT("Distortion Apply"));

#if PS3
		// Swap surface and texture pointer. SceneColorTexture now contains everything we previously rendered.
		// SceneColorSurface now contains something undefined, so we need to fill it up completely.
		RHICopyToResolveTarget(GSceneRenderTargets.GetSceneColorSurface(), FALSE, FResolveParams());
#endif

		GSceneRenderTargets.BeginRenderingSceneColorRaw();
		RHIBeginHiStencilPlayback(TRUE);

		// Apply distortion as a full-screen pass		
		for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENT(EventView,Views.Num() > 1)(DEC_SCENE_ITEMS,TEXT("View%d"),ViewIndex);

			FViewInfo& View = Views(ViewIndex);
			// viewport to match view size
			RHISetViewport(View.RenderTargetX,View.RenderTargetY,0.0f,View.RenderTargetX + View.RenderTargetSizeX,View.RenderTargetY + View.RenderTargetSizeY,1.0f);				
			RHISetViewParameters(View);
			RHISetMobileHeightFogParams(View.HeightFogParams);

			// disable depth test & writes
			RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());

			if ((View.Family->ShowFlags & SHOW_ShaderComplexity) != 0)
			{
				// Add the accumulated complexity with scene color complexity
				RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());
			}
			else
			{
				// opaque blend
				RHISetBlendState(TStaticBlendState<>::GetRHI());
			}

#if !PS3
			//pass if non-zero, so only the pixels that accumulated an offset will be rendered
			RHISetStencilState(TStaticStencilState<
				TRUE,CF_NotEqual,SO_Keep,SO_Keep,SO_Keep,
				FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
				0xff,0xff,0
				>::GetRHI());
#endif

			// by default, we'll render a full screen quad
			FIntRect QuadRect(	View.RenderTargetX, 
								View.RenderTargetY, 
								View.RenderTargetX + View.RenderTargetSizeX, 
								View.RenderTargetY + View.RenderTargetSizeY);

			// if a custom resolve rect is in use, we need to respect it's bounds
			if (bCustomResolveParams)
			{
				QuadRect.Min.X= Max(QuadRect.Min.X, ResolveRect.X1);
				QuadRect.Min.Y= Max(QuadRect.Min.Y, ResolveRect.Y1);
				QuadRect.Max.X= Min(QuadRect.Max.X, ResolveRect.X2);
				QuadRect.Max.Y= Min(QuadRect.Max.Y, ResolveRect.Y2);
			}

			// draw the quad
			View.DistortionPrimSet[DPGIndex].DrawScreenDistort(&View,DPGIndex,CanvasTransform, QuadRect,GSceneRenderTargets.GetSceneColorRawTexture());
		}

		RHIEndHiStencil();
		GSceneRenderTargets.FinishRenderingSceneColor(FALSE);
	}
	else if ( bRender )
	{
		RHIEndHiStencil();

		if (bCustomResolveParams)
		{
			// when a custom resolve rectangle is used, we can't copy the whole 
			// color texture as a raw surface. We can only use the area
			// previously resolved.
			FVector2D BufferSize(GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());
			FVector2D RTOffset(0, 0);

			const FIntRect QuadRect(	ResolveRect.X1, 
										ResolveRect.Y1, 
										ResolveRect.X2, 
										ResolveRect.Y2);
			FBox Vert(0);
			ScreenToViewspaceCoordsPos(QuadRect, BufferSize, BufferSize, RTOffset, Vert);

			// Restore scene color texture as a raw-surface
			GSceneRenderTargets.RestoreSceneColorRectRaw(Vert.Min.X, Vert.Min.Y, Vert.Max.X, Vert.Max.Y);
		}
		else
		{
			// Restore scene color texture as a raw-surface
			GSceneRenderTargets.RestoreSceneColorRaw();
		}

		bDirty = TRUE;
	}

	//restore default stencil state
	RHISetStencilState(TStaticStencilState<>::GetRHI());
	return bDirty;
}
