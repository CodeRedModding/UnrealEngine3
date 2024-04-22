/*=============================================================================
FogVolumeRendering.cpp: Implementation for rendering fog volumes.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "EngineFogVolumeClasses.h"


/*-----------------------------------------------------------------------------
FFogVolumeShaderParameters
-----------------------------------------------------------------------------*/

/** Binds the parameters */
void FFogVolumeShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	FirstDensityFunctionParameter.Bind(ParameterMap,TEXT("FirstDensityFunctionParameters"),TRUE);
	SecondDensityFunctionParameter.Bind(ParameterMap,TEXT("SecondDensityFunctionParameters"),TRUE);
	StartDistanceParameter.Bind(ParameterMap,TEXT("StartDistance"),TRUE);
	FogVolumeBoxMinParameter.Bind(ParameterMap,TEXT("FogVolumeBoxMin"),TRUE);
	FogVolumeBoxMaxParameter.Bind(ParameterMap,TEXT("FogVolumeBoxMax"),TRUE);
	ApproxFogColorParameter.Bind(ParameterMap,TEXT("ApproxFogColor"),TRUE);
}

/** Sets the parameters on the input VertexShader, using fog volume data from the input DensitySceneInfo. */
void FFogVolumeShaderParameters::SetVertexShader(
	const FSceneView& View,
	const FMaterialRenderProxy* MaterialRenderProxy, 
	FShader* VertexShader, 
	const FFogVolumeDensitySceneInfo* FogVolumeSceneInfo
	) const
{
	Set(View, MaterialRenderProxy, VertexShader->GetVertexShader(), FogVolumeSceneInfo);
}

#if WITH_D3D11_TESSELLATION
/** Sets the parameters on the input DomainShader, using fog volume data from the input DensitySceneInfo. */
void FFogVolumeShaderParameters::SetDomainShader(
	const FSceneView& View,
	const FMaterialRenderProxy* MaterialRenderProxy, 
	FShader* DomainShader, 
	const FFogVolumeDensitySceneInfo* FogVolumeSceneInfo
	) const
{
	Set(View, MaterialRenderProxy, DomainShader->GetDomainShader(), FogVolumeSceneInfo);
}
#endif

/** Sets the parameters on the input Shader, using fog volume data from the input DensitySceneInfo. */
template<typename ShaderRHIParamRef>
void FFogVolumeShaderParameters::Set(
	const FSceneView& View,
	const FMaterialRenderProxy* MaterialRenderProxy, 
	ShaderRHIParamRef Shader, 
	const FFogVolumeDensitySceneInfo* FogVolumeSceneInfo
	) const
{
	if (FogVolumeSceneInfo)
	{
		SetShaderValue( Shader, FirstDensityFunctionParameter, FogVolumeSceneInfo->GetFirstDensityFunctionParameters(View));
		SetShaderValue( Shader, SecondDensityFunctionParameter, FogVolumeSceneInfo->GetSecondDensityFunctionParameters(View));
		SetShaderValue( Shader, StartDistanceParameter, FogVolumeSceneInfo->StartDistance);
		SetShaderValue( Shader, ApproxFogColorParameter, FogVolumeSceneInfo->ApproxFogColor);
		SetShaderValue( Shader, FogVolumeBoxMinParameter, FogVolumeSceneInfo->VolumeBounds.Min + View.PreViewTranslation);
		SetShaderValue( Shader, FogVolumeBoxMaxParameter, FogVolumeSceneInfo->VolumeBounds.Max + View.PreViewTranslation);
	}
	else
	{
		SetShaderValue( Shader, FirstDensityFunctionParameter, FVector4(0.0f, 0.0f, 0.0f, 0.0f));
		SetShaderValue( Shader, SecondDensityFunctionParameter, FVector4(0.0f, 0.0f, 0.0f, 0.0f));
		SetShaderValue( Shader, StartDistanceParameter, 0.0f);
		SetShaderValue( Shader, ApproxFogColorParameter, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		SetShaderValue( Shader, FogVolumeBoxMinParameter, FVector(0.0f, 0.0f, 0.0f));
		SetShaderValue( Shader, FogVolumeBoxMaxParameter, FVector(0.0f, 0.0f, 0.0f));
	}
}

/** Serializer */
FArchive& operator<<(FArchive& Ar,FFogVolumeShaderParameters& Parameters)
{
	Ar << Parameters.FirstDensityFunctionParameter;
	Ar << Parameters.SecondDensityFunctionParameter;
	Ar << Parameters.StartDistanceParameter;
	Ar << Parameters.FogVolumeBoxMinParameter;
	Ar << Parameters.FogVolumeBoxMaxParameter;
	Ar << Parameters.ApproxFogColorParameter;
	return Ar;
}

/*-----------------------------------------------------------------------------
FFogVolumeDensitySceneInfo
-----------------------------------------------------------------------------*/
FFogVolumeDensitySceneInfo::FFogVolumeDensitySceneInfo(const UFogVolumeDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex) : 
	Component(InComponent),
	VolumeBounds(InVolumeBounds),
	DPGIndex(InDPGIndex)
{
	if (InComponent == NULL)
	{
		// InComponent can be NULL when the fog volume is being rendered in thumbnails, so set reasonable defaults.
		StartDistance = 0.0f;
		MaxDistance = 65535.0f;
		bAffectsTranslucency = TRUE;
		bOnlyAffectsTranslucency = FALSE;
		ApproxFogColor = FLinearColor::Black;
		OwnerName = NAME_None;
	}
	else
	{
		StartDistance = InComponent->StartDistance;
		MaxDistance = InComponent->MaxDistance;
		bAffectsTranslucency = InComponent->bAffectsTranslucency;
		bOnlyAffectsTranslucency = InComponent->bOnlyAffectsTranslucency;
		ApproxFogColor = InComponent->ApproxFogLightColor;
		OwnerName = InComponent->GetOwner() ? InComponent->GetOwner()->GetFName() : NAME_None;
	}
}

/*-----------------------------------------------------------------------------
FFogVolumeConstantDensitySceneInfo
-----------------------------------------------------------------------------*/
FFogVolumeConstantDensitySceneInfo::FFogVolumeConstantDensitySceneInfo(const UFogVolumeConstantDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex):
	FFogVolumeDensitySceneInfo(InComponent, InVolumeBounds, InDPGIndex),
	Density(InComponent->Density)
{}

UBOOL FFogVolumeConstantDensitySceneInfo::DrawDynamicMesh(
	const FViewInfo& View,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId)
{
	return TFogIntegralDrawingPolicyFactory<FConstantDensityPolicy>::DrawDynamicMesh( View, Mesh, bBackFace, bPreFog, PrimitiveSceneInfo, HitProxyId, this);
}

UINT FFogVolumeConstantDensitySceneInfo::GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const
{
	const FShader* IntegralPixelShader = MaterialResource->GetShader<TFogIntegralPixelShader<FConstantDensityPolicy> >(InVertexFactory->GetType());
	return IntegralPixelShader->GetNumInstructions();
}

FVector4 FFogVolumeConstantDensitySceneInfo::GetFirstDensityFunctionParameters(const FSceneView& View) const
{
	return FVector4(Density, 0.0f, 0.0f, 0.0f);
}

FLOAT FFogVolumeConstantDensitySceneInfo::GetMaxIntegral() const
{
	return Density * 1000000.0f;
}

/*-----------------------------------------------------------------------------
FFogVolumeLinearHalfspaceDensitySceneInfo
-----------------------------------------------------------------------------*/
FFogVolumeLinearHalfspaceDensitySceneInfo::FFogVolumeLinearHalfspaceDensitySceneInfo(const UFogVolumeLinearHalfspaceDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex):
	FFogVolumeDensitySceneInfo(InComponent, InVolumeBounds, InDPGIndex),
	PlaneDistanceFactor(InComponent->PlaneDistanceFactor),
	HalfspacePlane(InComponent->HalfspacePlane)
{}

UBOOL FFogVolumeLinearHalfspaceDensitySceneInfo::DrawDynamicMesh(
	const FViewInfo& View,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId)
{
	return TFogIntegralDrawingPolicyFactory<FLinearHalfspaceDensityPolicy>::DrawDynamicMesh( View, Mesh, bBackFace, bPreFog, PrimitiveSceneInfo, HitProxyId, this);
}

UINT FFogVolumeLinearHalfspaceDensitySceneInfo::GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const
{
	const FShader* IntegralPixelShader = MaterialResource->GetShader<TFogIntegralPixelShader<FLinearHalfspaceDensityPolicy> >(InVertexFactory->GetType());
	return IntegralPixelShader->GetNumInstructions();
}

FVector4 FFogVolumeLinearHalfspaceDensitySceneInfo::GetFirstDensityFunctionParameters(const FSceneView& View) const
{
	return FVector4(PlaneDistanceFactor, 0.0f, 0.0f, 0.0f);
}

FVector4 FFogVolumeLinearHalfspaceDensitySceneInfo::GetSecondDensityFunctionParameters(const FSceneView& View) const
{
	return FVector4(
		HalfspacePlane.X,
		HalfspacePlane.Y,
		HalfspacePlane.Z,
		HalfspacePlane.W - (View.PreViewTranslation | FVector(HalfspacePlane))
		);
}

FLOAT FFogVolumeLinearHalfspaceDensitySceneInfo::GetMaxIntegral() const
{
	return PlaneDistanceFactor * 100000.0f;
}

/*-----------------------------------------------------------------------------
FFogVolumeSphericalDensitySceneInfo
-----------------------------------------------------------------------------*/
FFogVolumeSphericalDensitySceneInfo::FFogVolumeSphericalDensitySceneInfo(const UFogVolumeSphericalDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex):
	FFogVolumeDensitySceneInfo(InComponent, InVolumeBounds, InDPGIndex),
	MaxDensity(InComponent->MaxDensity),
	Sphere(FSphere(InComponent->SphereCenter, InComponent->SphereRadius))
{}

UBOOL FFogVolumeSphericalDensitySceneInfo::DrawDynamicMesh(
	const FViewInfo& View,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId)
{
	return TFogIntegralDrawingPolicyFactory<FSphereDensityPolicy>::DrawDynamicMesh( View, Mesh, bBackFace, bPreFog, PrimitiveSceneInfo, HitProxyId, this);
}

UINT FFogVolumeSphericalDensitySceneInfo::GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const
{
	const FShader* IntegralPixelShader = MaterialResource->GetShader<TFogIntegralPixelShader<FSphereDensityPolicy> >(InVertexFactory->GetType());
	return IntegralPixelShader->GetNumInstructions();
}

FVector4 FFogVolumeSphericalDensitySceneInfo::GetFirstDensityFunctionParameters(const FSceneView& View) const
{
	return FVector4(MaxDensity, 0.0f, 0.0f, 0.0f);
}

FVector4 FFogVolumeSphericalDensitySceneInfo::GetSecondDensityFunctionParameters(const FSceneView& View) const
{
	return FVector4(Sphere.Center + View.PreViewTranslation, Sphere.W);
}

FLOAT FFogVolumeSphericalDensitySceneInfo::GetMaxIntegral() const
{
	return MaxDensity * 250000.0f;
}

/*-----------------------------------------------------------------------------
FFogVolumeConeDensitySceneInfo
-----------------------------------------------------------------------------*/
FFogVolumeConeDensitySceneInfo::FFogVolumeConeDensitySceneInfo(const UFogVolumeConeDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex):
	FFogVolumeDensitySceneInfo(InComponent, InVolumeBounds, InDPGIndex),
	MaxDensity(InComponent->MaxDensity),
	ConeVertex(InComponent->ConeVertex),
	ConeRadius(InComponent->ConeRadius),
	ConeAxis(InComponent->ConeAxis),
	ConeMaxAngle(InComponent->ConeMaxAngle)
{}

UBOOL FFogVolumeConeDensitySceneInfo::DrawDynamicMesh(
	const FViewInfo& View,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId)
{
	return TFogIntegralDrawingPolicyFactory<FConeDensityPolicy>::DrawDynamicMesh( View, Mesh, bBackFace, bPreFog, PrimitiveSceneInfo, HitProxyId, this);
}

UINT FFogVolumeConeDensitySceneInfo::GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const
{
	const FShader* IntegralPixelShader = MaterialResource->GetShader<TFogIntegralPixelShader<FConeDensityPolicy> >(InVertexFactory->GetType());
	return IntegralPixelShader->GetNumInstructions();
}

FVector4 FFogVolumeConeDensitySceneInfo::GetFirstDensityFunctionParameters(const FSceneView& View) const
{
	return FVector4(ConeVertex + View.PreViewTranslation, MaxDensity);
}

FVector4 FFogVolumeConeDensitySceneInfo::GetSecondDensityFunctionParameters(const FSceneView& View) const
{
	return FVector4(ConeAxis, ConeRadius);
}

FLOAT FFogVolumeConeDensitySceneInfo::GetMaxIntegral() const
{
	return MaxDensity * 250000.0f;
}


IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TFogIntegralVertexShader<FConstantDensityPolicy>,TEXT("FogIntegralVertexShader"),TEXT("Main"),SF_Vertex,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TFogIntegralPixelShader<FConstantDensityPolicy>,TEXT("FogIntegralPixelShader"),TEXT("ConstantDensityMain"),SF_Pixel,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TFogIntegralVertexShader<FLinearHalfspaceDensityPolicy>,TEXT("FogIntegralVertexShader"),TEXT("Main"),SF_Vertex,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TFogIntegralPixelShader<FLinearHalfspaceDensityPolicy>,TEXT("FogIntegralPixelShader"),TEXT("LinearHalfspaceDensityMain"),SF_Pixel,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TFogIntegralVertexShader<FSphereDensityPolicy>,TEXT("FogIntegralVertexShader"),TEXT("Main"),SF_Vertex,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TFogIntegralPixelShader<FSphereDensityPolicy>,TEXT("FogIntegralPixelShader"),TEXT("SphericalDensityMain"),SF_Pixel,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TFogIntegralVertexShader<FConeDensityPolicy>,TEXT("FogIntegralVertexShader"),TEXT("Main"),SF_Vertex,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TFogIntegralPixelShader<FConeDensityPolicy>,TEXT("FogIntegralPixelShader"),TEXT("ConeDensityMain"),SF_Pixel,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);

IMPLEMENT_MATERIAL_SHADER_TYPE(,FFogVolumeApplyVertexShader,TEXT("FogVolumeApplyVertexShader"),TEXT("Main"),SF_Vertex,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);
IMPLEMENT_MATERIAL_SHADER_TYPE(,FFogVolumeApplyPixelShader,TEXT("FogVolumeApplyPixelShader"),TEXT("Main"),SF_Pixel,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);


template<class DensityFunctionPolicy>
TFogIntegralDrawingPolicy<DensityFunctionPolicy>::TFogIntegralDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource)
	:	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource)
{
	VertexShader = InMaterialResource.GetShader<TFogIntegralVertexShader<DensityFunctionPolicy> >(InVertexFactory->GetType());
	PixelShader = InMaterialResource.GetShader<TFogIntegralPixelShader<DensityFunctionPolicy> >(InVertexFactory->GetType());
}

/**
* Match two draw policies
* @param Other - draw policy to compare
* @return TRUE if the draw policies are a match
*/
template<class DensityFunctionPolicy>
UBOOL TFogIntegralDrawingPolicy<DensityFunctionPolicy>::Matches(
	const TFogIntegralDrawingPolicy<DensityFunctionPolicy>& Other
	) const
{
	return FMeshDrawingPolicy::Matches(Other) &&
		VertexShader == Other.VertexShader && 
		PixelShader == Other.PixelShader;
}

/**
* Executes the draw commands which can be shared between any meshes using this drawer.
* @param CI - The command interface to execute the draw commands on.
* @param View - The view of the scene being drawn.
*/
template<class DensityFunctionPolicy>
void TFogIntegralDrawingPolicy<DensityFunctionPolicy>::DrawShared(
	const FViewInfo* View,
	FBoundShaderStateRHIParamRef BoundShaderState,
	const FFogVolumeDensitySceneInfo* DensitySceneInfo,
	UBOOL bBackFace
	) const
{
	// Set the translucent shader parameters for the material instance
	VertexShader->SetParameters(VertexFactory,MaterialRenderProxy,*View);
	PixelShader->SetParameters(VertexFactory,MaterialRenderProxy,*View, DensitySceneInfo, bBackFace);

	// Set shared mesh resources
	FMeshDrawingPolicy::DrawShared(View);

	// Set the actual shader & vertex declaration state
	RHISetBoundShaderState( BoundShaderState);
}

/** 
* Create bound shader state using the vertex decl from the mesh draw policy
* as well as the shaders needed to draw the mesh
* @param DynamicStride - optional stride for dynamic vertex data
* @return new bound shader state object
*/
template<class DensityFunctionPolicy>
FBoundShaderStateRHIRef TFogIntegralDrawingPolicy<DensityFunctionPolicy>::CreateBoundShaderState(DWORD DynamicStride)
{
	FVertexDeclarationRHIRef VertexDeclaration;
	DWORD StreamStrides[MaxVertexElementCount];

	FMeshDrawingPolicy::GetVertexDeclarationInfo(VertexDeclaration, StreamStrides);
	if (DynamicStride)
	{
		StreamStrides[0] = DynamicStride;
	}

	return RHICreateBoundShaderState(VertexDeclaration, StreamStrides, VertexShader->GetVertexShader(), PixelShader->GetPixelShader(), EGST_None);
}

/**
* Sets the render states for drawing a mesh.
* @param PrimitiveSceneInfo - The primitive drawing the dynamic mesh.  If this is a view element, this will be NULL.
* @param Mesh - mesh element with data needed for rendering
* @param ElementData - context specific data for mesh rendering
*/
template<class DensityFunctionPolicy>
void TFogIntegralDrawingPolicy<DensityFunctionPolicy>::SetMeshRenderState(
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
	PixelShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace);

	// Set rasterizer state.
	const FRasterizerStateInitializerRHI Initializer = {
		(Mesh.bWireframe || IsWireframe()) ? FM_Wireframe : FM_Solid,
		IsTwoSided() ? CM_None : (XOR(XOR(View.bReverseCulling,bBackFace), Mesh.ReverseCulling) ? CM_CCW : CM_CW),
		Mesh.DepthBias,
		Mesh.SlopeScaleDepthBias,
		TRUE
	};
	RHISetRasterizerStateImmediate( Initializer);
}


FFogVolumeApplyDrawingPolicy::FFogVolumeApplyDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	const FFogVolumeDensitySceneInfo* DensitySceneInfo,
	UBOOL bInOverrideWithShaderComplexity)
	:	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource,bInOverrideWithShaderComplexity,bInOverrideWithShaderComplexity)
{
	VertexShader = InMaterialResource.GetShader<FFogVolumeApplyVertexShader>(InVertexFactory->GetType());
	PixelShader = InMaterialResource.GetShader<FFogVolumeApplyPixelShader>(InVertexFactory->GetType());

#if !FINAL_RELEASE
	if (bInOverrideWithShaderComplexity)
	{
		// Get the density function specific number of instructions
		NumIntegralInstructions = DensitySceneInfo->GetNumIntegralShaderInstructions(&InMaterialResource, InVertexFactory);
	}
#endif
}

/**
* Match two draw policies
* @param Other - draw policy to compare
* @return TRUE if the draw policies are a match
*/
UBOOL FFogVolumeApplyDrawingPolicy::Matches(
	const FFogVolumeApplyDrawingPolicy& Other
	) const
{
	return FMeshDrawingPolicy::Matches(Other) &&
		VertexShader == Other.VertexShader && 
		PixelShader == Other.PixelShader;
}

/**
* Executes the draw commands which can be shared between any meshes using this drawer.
* @param CI - The command interface to execute the draw commands on.
* @param View - The view of the scene being drawn.
*/
void FFogVolumeApplyDrawingPolicy::DrawShared(
	const FViewInfo* View,
	FBoundShaderStateRHIParamRef BoundShaderState, 
	const FFogVolumeDensitySceneInfo* DensitySceneInfo
	) const
{
	// Set the translucent shader parameters for the material instance
	VertexShader->SetParameters(VertexFactory,MaterialRenderProxy,*View);

#if !FINAL_RELEASE
	if (bOverrideWithShaderComplexity)
	{
		// Add complexity to existing
		RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());

		TShaderMapRef<FShaderComplexityAccumulatePixelShader> ShaderComplexityPixelShader(GetGlobalShaderMap());
		
		const UINT FogDownsampleFactor = GSceneRenderTargets.GetFogAccumulationDownsampleFactor();
		// Approximate the number of instructions used to shade each final pixel of the fog volume
		const UINT NumPixelInstructions = PixelShader->GetNumInstructions() 
			+ NumIntegralInstructions / (FogDownsampleFactor * FogDownsampleFactor);

		// Don't add any vertex complexity
		ShaderComplexityPixelShader->SetParameters( 0, NumPixelInstructions);
	}
	else
#endif
	{
		PixelShader->SetParameters(VertexFactory,MaterialRenderProxy,*View, DensitySceneInfo);
	}

	// Set shared mesh resources
	FMeshDrawingPolicy::DrawShared(View);

	// Set the actual shader & vertex declaration state
	RHISetBoundShaderState( BoundShaderState);
}

/** 
* Create bound shader state using the vertex decl from the mesh draw policy
* as well as the shaders needed to draw the mesh
* @param DynamicStride - optional stride for dynamic vertex data
* @return new bound shader state object
*/
FBoundShaderStateRHIRef FFogVolumeApplyDrawingPolicy::CreateBoundShaderState(DWORD DynamicStride)
{
	FVertexDeclarationRHIRef VertexDeclaration;
	DWORD StreamStrides[MaxVertexElementCount];

	FMeshDrawingPolicy::GetVertexDeclarationInfo(VertexDeclaration, StreamStrides);
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

	return RHICreateBoundShaderState(VertexDeclaration, StreamStrides, VertexShader->GetVertexShader(), PixelShaderRHIRef, EGST_None);
}

/**
* Sets the render states for drawing a mesh.
* @param PrimitiveSceneInfo - The primitive drawing the dynamic mesh.  If this is a view element, this will be NULL.
* @param Mesh - mesh element with data needed for rendering
* @param ElementData - context specific data for mesh rendering
*/
void FFogVolumeApplyDrawingPolicy::SetMeshRenderState(
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

#if !FINAL_RELEASE
	// Don't set pixel shader constants if we are overriding with the shader complexity pixel shader
	if (!bOverrideWithShaderComplexity)
#endif
	{
		PixelShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace);
	}

	// Set rasterizer state.
	const FRasterizerStateInitializerRHI Initializer = {
		(Mesh.bWireframe || IsWireframe()) ? FM_Wireframe : FM_Solid,
		IsTwoSided() ? CM_None : (XOR(XOR(View.bReverseCulling,bBackFace), Mesh.ReverseCulling) ? CM_CCW : CM_CW),
		Mesh.DepthBias,
		Mesh.SlopeScaleDepthBias,
		TRUE
	};
	RHISetRasterizerStateImmediate( Initializer);
}


/**
* Render a dynamic mesh using a distortion mesh draw policy 
* @return TRUE if the mesh rendered
*/
UBOOL FFogVolumeApplyDrawingPolicyFactory::DrawDynamicMesh(
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId, 
	const FFogVolumeDensitySceneInfo* DensitySceneInfo
	)
{
	// draw dynamic mesh element using distortion mesh policy
	FFogVolumeApplyDrawingPolicy DrawingPolicy(
		Mesh.VertexFactory,
		Mesh.MaterialRenderProxy,
		*Mesh.MaterialRenderProxy->GetMaterial(),
		DensitySceneInfo,
		(View.Family->ShowFlags & SHOW_ShaderComplexity) != 0
		);
	DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()), DensitySceneInfo);
	for( INT BatchElementIndex=0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++ )
	{
		DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,FFogVolumeApplyDrawingPolicy::ElementDataType());
		DrawingPolicy.DrawMesh(Mesh,BatchElementIndex);
	}

	return TRUE;
}

/**
Fog Overview:
	Fog contribution is approximated as SrcColor * f + (1-f) * FogVolumeColor, where SrcColor is whatever color is transmitted 
	through the fog volume, FogVolumeColor is the color that the fog volume adds to the scene, and f is the result of the transmittance function.
	So f = exp(-(F(x,y,z) over R)), where F(x,y,z) is the line integral of the density function, 
	and R is the ray from the camera to the first opaque object it intersects for that pixel, clipped to the fog volume.

The goals of this fog volume implementation are:
	-support multiple density functions (not just constant density) that can be evaluated efficiently
	-handle concave objects
	-handle any camera-fogVolume-other object interaction, which basically breaks down into
		-opaque objects intersecting the fog volume
		-the camera inside the fog volume
		-transparent objects intersecting the fog volume

Algorithm:

	clear stencil to 0 if there are any fog volumes
	//downsample depth?
	sort fog volumes with transparent objects and render back to front
	disable depth tests and writes
	for each fog volume
	{
		Integral Accumulation Pass
		begin rendering integral accumulation, the buffer can be downsampled to minimize pixel shader computations
		clear the integral accumulation buffer to 0 on current fog volume pixels
		add in F(backface) (which is the line integral of the density function evaluated from the backface depth to the camera), 
			but use min(backfaceDepth, closestOpaqueOccluderdepth) for depth
		same for frontfaces, but subtract 
		
		Fog Apply Pass
		begin rendering scene color
		need to make sure each pixel is only touched once here
		static int id = 1;
		set stencil to fail on id, set stencil to id, clear stencil if id == 256 and set id = 0
		render fog volume backfaces, lookup integral accumulation 
		{
			//clip() if integral is ~0?
			calculate f = exp(-integral accumulation)
			alpha blend SrcColor * f + (1-f) FogVolumeColor
		}
		
		disable stencil mask
	}
	restore state

Limitations of this implementation:
	-fog volume geometry
		-must be a closed mesh, so any ray cast through it will hit the same number of frontfaces as backfaces.
	-the density function must be chosen so that:
		-we can evaluate the line integral through the fog volume analytically (can't approximate with lots of samples)
		-we can evaluate the integral with only the depth information of the current face being rendered, and not any other faces of the mesh
		-need to evaluate the integral from current face to camera position.  This way when the camera is inside the fog volume 
		and a frontface is clipped by the nearplane the integral will still be correct.
	-fog on translucency intersecting the fog volumes is approximated per-vertex
		-sorting artifacts
		-clipping by the density function is handled, but clipping by the fog volume mesh is not.  Instead the ray is clipped to the AABB of the fog volume mesh.
		-max translucency nesting of 4 height fogs and 1 fog volume
		-fog volumes are affected by the height fog per-vertex, and there are artifacts since only the backface vertices are fogged.
Todo:
		-fixed point blending implementations (xenon) 
			-visible banding, need some dithering to hide it
			-'stripe' artifacts when using linear filtering on the packed integral during the upsample
			-NAN's or some other artifact around the sphere and in the plane of the linear halfspace
			-only support a small number of frontface or backface overlaps (currently 4)
		-cone density function is not yet implemented
		-ATI x1x00 cards only blend fp16 but don't filter, which is not handled

References:

"Real-Time Animation of Realistic Fog" 
http://citeseer.ist.psu.edu/cachedpage/525331/2 - couldn't find the original paper, but the cached version is readable.  
		This is by far the most valuable resource I found on the subject.

"Real time rendering of heterogenous fog based on the graphics hardware acceleration"
http://www.cescg.org/CESCG-2004/web/Zdrojewska-Dorota/ - good overall explanation, but skips some of the most critical parts

"Fog Polygon Volumes"
http://http.download.nvidia.com/developer/SDK/Individual_Samples/3dgraphics_samples.html - the Fog Polygon Volumes sample.  
		Contains tons of useful information on constant density fog volumes, encoding depths, handling opaque intersections, etc.
*/

/**
* This index is used to only render each fog volume pixel once and minimize stencil clears during the apply pass.  
* It is set to 0 before translucency and fog volumes are rendered every frame, and wrapped when it reaches 256.
*/
UINT FogApplyStencilIndex = 0;

/**
* Resets the apply stencil index.  This is called each time translucency is rendered.
*/
void ResetFogVolumeIndex()
{
	FogApplyStencilIndex = 0;
}

/**
* Render a Fog Volume.  The density function to use is found in PrimitiveSceneInfo->Scene's FogVolumes map.
* @return TRUE if the mesh rendered
*
*/
UBOOL RenderFogVolume(
	  const FViewInfo* View,
	  const FMeshBatch& Mesh,
	  UBOOL bBackFace,
	  UBOOL bPreFog,
	  const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	  FHitProxyId HitProxyId)
{
	UBOOL bDirty = FALSE;

	//find the density function information associated with this primitive
	FFogVolumeDensitySceneInfo** FogDensityInfoRef = PrimitiveSceneInfo->Scene->FogVolumes.Find(PrimitiveSceneInfo->Component);
	if (GSystemSettings.bAllowFogVolumes 
		&& FogDensityInfoRef 
		&& !(*FogDensityInfoRef)->bOnlyAffectsTranslucency
		// Render fog volumes in shader complexity mode
		// SHOW_Fog will not be defined in shader complexity mode, because other types of fog do not work correctly, but fog volumes do
		&& (ShouldRenderFog(View->Family->ShowFlags) || (View->Family->ShowFlags & SHOW_ShaderComplexity)))
	{
		FFogVolumeDensitySceneInfo* FogDensityInfo = *FogDensityInfoRef;
		check(FogDensityInfo);
		SCOPED_CONDITIONAL_DRAW_EVENT(FogEvent,FogDensityInfo->OwnerName != NAME_None)(DEC_SCENE_ITEMS, FogDensityInfo->OwnerName.IsValid() ? *FogDensityInfo->OwnerName.ToString() : TEXT(""));

		//calculate the dimensions of the integral accumulation buffers based on the fog downsample factor
		const UINT FogAccumulationDownsampleFactor = GSceneRenderTargets.GetFogAccumulationDownsampleFactor();
		const UINT FogAccumulationBufferX = View->RenderTargetX / FogAccumulationDownsampleFactor;
		const UINT FogAccumulationBufferY = View->RenderTargetY / FogAccumulationDownsampleFactor;
		const UINT FogAccumulationBufferSizeX = View->RenderTargetSizeX / FogAccumulationDownsampleFactor;
		const UINT FogAccumulationBufferSizeY = View->RenderTargetSizeY / FogAccumulationDownsampleFactor;

		//on sm3 and PS3 this will accumulate the integral for both faces
		//on Xenon this will accumulate the integral for back faces only
		GSceneRenderTargets.BeginRenderingFogBackfacesIntegralAccumulation();
		RHISetViewport(FogAccumulationBufferX, FogAccumulationBufferY, 0.0f, FogAccumulationBufferX + FogAccumulationBufferSizeX, FogAccumulationBufferY + FogAccumulationBufferSizeY, 1.0f);
		RHISetViewParameters(*View);
		RHISetMobileHeightFogParams(View->HeightFogParams);

		//clear all channels to 0 so we start with no integral
		RHIClear(TRUE, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), FALSE, 0, FALSE, 0);

		//no depth writes, no depth tests
		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());

		//additive blending for all 4 channels
		RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_One,BF_One>::GetRHI());

#if !XBOX
		//try to save some bandwidth with FP blending implementation, since we are only using the red channel
		RHISetColorWriteMask( CW_RED);
#endif

		//render fog volume backfaces, calculating the integral from the camera to the backface (or an intersecting opaque object)
		//and adding this to the accumulation buffer.
		bDirty |= FogDensityInfo->DrawDynamicMesh(
			*View,
			Mesh,
			TRUE,
			bPreFog,
			PrimitiveSceneInfo,
			HitProxyId);

#if XBOX
		GSceneRenderTargets.FinishRenderingFogBackfacesIntegralAccumulation();

		GSceneRenderTargets.BeginRenderingFogFrontfacesIntegralAccumulation();
		RHISetViewport(FogAccumulationBufferX, FogAccumulationBufferY, 0.0f, FogAccumulationBufferX + FogAccumulationBufferSizeX, FogAccumulationBufferY + FogAccumulationBufferSizeY, 1.0f);
		RHISetViewParameters(*View);
		RHISetMobileHeightFogParams(View->HeightFogParams);
		//clear all channels to 0 so we start with no integral
		RHIClear( TRUE, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), FALSE, 0, FALSE, 0 );

		//render fog volume frontfaces, calculating the integral from the camera to the frontface (or an intersecting opaque object) 
		//and adding this to the accumulation buffer for frontfaces.
		bDirty |= FogDensityInfo->DrawDynamicMesh(
			*View,
			Mesh,
			FALSE,
			bPreFog,
			PrimitiveSceneInfo,
			HitProxyId);

		GSceneRenderTargets.FinishRenderingFogFrontfacesIntegralAccumulation();
#else
		//render fog volume frontfaces, calculating the integral from the camera to the frontface (or an intersecting opaque object) 
		//and subtracting this from the accumulation buffer.
		bDirty |= FogDensityInfo->DrawDynamicMesh(
			*View,
			Mesh,
			FALSE,
			bPreFog,
			PrimitiveSceneInfo,
			HitProxyId);

		GSceneRenderTargets.FinishRenderingFogBackfacesIntegralAccumulation();
#endif

		//restore render targets assumed by transparency even if nothing was rendered in the accumulation passes
		GSceneRenderTargets.BeginRenderingSceneColor(); 

		// Alpha blend color = FogColor * (1 - FogFactor) + DestColor * FogFactor, preserve dest alpha since it stores depth on some platforms
		RHISetBlendState(TStaticBlendState<BO_Add,BF_InverseSourceAlpha,BF_SourceAlpha,BO_Add,BF_Zero,BF_One>::GetRHI());

		RHISetViewport(View->RenderTargetX,View->RenderTargetY,0.0f,View->RenderTargetX + View->RenderTargetSizeX,View->RenderTargetY + View->RenderTargetSizeY,1.0f);
		RHISetViewParameters(*View);
		RHISetMobileHeightFogParams(View->HeightFogParams);

		RHISetColorWriteMask(CW_RGBA);

		if (bDirty)
		{
			// Disable the 'write once' stencil mask when shader complexity is enabled, so that overdraw will be factored in
			if (!(View->Family->ShowFlags & SHOW_ShaderComplexity))
			{
				//we need to only apply the fog to each pixel within the fog volume ONCE
				//this is done with the stencil buffer
				//clear stencil for the first fog volume each frame and wrap the index around if necessary
				const INT MaxStencilValue = 255;
				if (FogApplyStencilIndex == 0 || FogApplyStencilIndex >= MaxStencilValue)
				{
					FogApplyStencilIndex = 0;
					RHIClear( FALSE, FLinearColor::Black, FALSE, 0, TRUE, 0 );
				}

				FogApplyStencilIndex++;

				//accept if != stencil index, set to stencil index
				//this way each pixel is touched just once and a clear is not required after every fog volume
				FStencilStateInitializerRHI FogApplyStencilInitializer(
					TRUE,CF_NotEqual,SO_Keep,SO_Keep,SO_Replace,
					FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
					0xff,0xff,FogApplyStencilIndex);

				RHISetStencilState(RHICreateStencilState(FogApplyStencilInitializer));
			}
			
			//render fog volume backfaces so every fog volume pixel will be rendered even when the camera is inside
			//rendering both faces here will allow texturing on frontfaces but will add overdraw
			//many GPU's do not do early stencil reject when writing to stencil
			FFogVolumeApplyDrawingPolicyFactory::DrawDynamicMesh(
				*View,
				FFogVolumeApplyDrawingPolicyFactory::ContextType(),
				Mesh,
				TRUE,
				bPreFog,
				PrimitiveSceneInfo,
				HitProxyId,
				FogDensityInfo);

			GSceneRenderTargets.FinishRenderingSceneColor(FALSE);
		}

		//restore state assumed by translucency
		RHISetStencilState(TStaticStencilState<>::GetRHI());
		RHISetDepthState( TStaticDepthState<FALSE,CF_LessEqual>::GetRHI() );
	}

	return bDirty;
}

