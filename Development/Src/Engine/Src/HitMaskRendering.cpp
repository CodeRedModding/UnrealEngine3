/*================================================================================
	HitMaskRendering.cpp: Hit Mask Rendering drawing/shader related code
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/
#include "EnginePrivate.h"
#include "ScenePrivate.h"
/**
* A vertex shader for rendering hit mask
*/
class FHitMaskVertexShader : public FMeshMaterialVertexShader
{
	DECLARE_SHADER_TYPE(FHitMaskVertexShader,MeshMaterial);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Only DefaultEngine material
		return (Material->IsSpecialEngineMaterial());
	}

	FHitMaskVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialVertexShader(Initializer)
	{
		PixelCenterOffsetParameter.Bind(Initializer.ParameterMap,TEXT("PixelCenterOffset"), TRUE);
		MaterialParameters.Bind(Initializer.ParameterMap);
	}

	FHitMaskVertexShader() {}
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		bShaderHasOutdatedParameters |= (Ar << VertexFactoryParameters);
		Ar << MaterialParameters;
		Ar << PixelCenterOffsetParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView& View, const HitInfoStruct & HitInfo)
	{
		VertexFactoryParameters.Set(this,VertexFactory,View);
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set(this,MaterialRenderContext);

		// Get a half texel value for shader to apply offset to correct output position
		FVector2D PixelCenterOffset = FVector2D( GPixelCenterOffset / (FLOAT)HitInfo.HitMaskTexture->GetSizeX(), GPixelCenterOffset / (FLOAT)HitInfo.HitMaskTexture->GetSizeY() );
		SetVertexShaderValue( GetVertexShader(), PixelCenterOffsetParameter, PixelCenterOffset	);
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

 	/**
 	 * Can be overridden by FShader subclasses to modify their compile environment just before compilation occurs.
 	 */
 	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
 	{
 		OutEnvironment.Definitions.Set(TEXT("WORLD_COORDS"),TEXT("1"));
 	}

private:
	FMaterialVertexShaderParameters MaterialParameters; 
	// Since when rendered to screen, it moves to left/upper
	// To shift to left/up a half texel when sampling
	FShaderParameter PixelCenterOffsetParameter;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FHitMaskVertexShader,TEXT("HitMaskVertexShader"),TEXT("Main"),SF_Vertex,VER_HITMASK_MIRRORING_SUPPORT,0);

/**
* A pixel shader for rendering the depth of a mesh.
*/
class FHitMaskPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FHitMaskPixelShader,MeshMaterial);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Only DefaultEngine material
		return Material->IsSpecialEngineMaterial();
	}

	FHitMaskPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FShader(Initializer)
	{
		// Binding 
		MaterialParameters.Bind(Initializer.ParameterMap);
		HitLocationParameter.Bind(Initializer.ParameterMap,TEXT("HitLocation"), TRUE);
		HitStartLocationParameter.Bind(Initializer.ParameterMap,TEXT("HitStartLocation"), TRUE);
		HitRadiusParameter.Bind(Initializer.ParameterMap,TEXT("HitRadius"), TRUE);
		HitCullDistanceParameter.Bind(Initializer.ParameterMap,TEXT("HitCullDistance"), TRUE);
		HitCullNormalParameter.Bind(Initializer.ParameterMap,TEXT("HitCullNormal"), TRUE);
		HitMaskTextureParameter.Bind( Initializer.ParameterMap, TEXT( "CurrentMaskTexture" ), TRUE );
	}

	FHitMaskPixelShader() {}

 	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView* View, const HitInfoStruct & HitInfo)
 	{
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), View->Family->CurrentWorldTime, View->Family->CurrentRealTime, View);
		MaterialParameters.Set(this,MaterialRenderContext);

		SetPixelShaderValues<FVector>( GetPixelShader(), HitLocationParameter, &HitInfo.HitLocation, 1 );
		SetPixelShaderValues<FVector>( GetPixelShader(), HitStartLocationParameter, &HitInfo.HitStartLocation, 1 );
		SetPixelShaderValues<FLOAT>( GetPixelShader(), HitRadiusParameter, &HitInfo.HitRadius, 1 );
		SetPixelShaderValues<FLOAT>( GetPixelShader(), HitCullDistanceParameter, &HitInfo.HitCullDistance, 1 );
		
		// when not only facing, set -1 so that it allows all angle
		FLOAT HitCullNormal = 0.f;
		if ( !HitInfo.OnlyWhenFacing )
		{
			HitCullNormal = -1.f;
		}

		SetPixelShaderValues<FLOAT>( GetPixelShader(), HitCullNormalParameter, &HitCullNormal, 1 );
		
		SetTextureParameter( GetPixelShader(), HitMaskTextureParameter,	HitInfo.HitMaskTexture );
 	}

 	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View,UBOOL bBackFace)
 	{
 		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace);
 	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << MaterialParameters;
		Ar << HitStartLocationParameter;
		Ar << HitLocationParameter;
		Ar << HitRadiusParameter;
		Ar << HitCullDistanceParameter;
		Ar << HitCullNormalParameter;
		Ar << HitMaskTextureParameter;
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Can be overridden by FShader subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("WORLD_COORDS"),TEXT("1"));
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialPixelShaderParameters MaterialParameters; 
	// hit mask shader custom parameters
	FShaderParameter HitLocationParameter;
	FShaderParameter HitStartLocationParameter;
	FShaderParameter HitRadiusParameter;
	FShaderParameter HitCullDistanceParameter;
	FShaderParameter HitCullNormalParameter;
	FShaderResourceParameter HitMaskTextureParameter;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FHitMaskPixelShader,TEXT("HitMaskPixelShader"),TEXT("Main"),SF_Pixel,0,0);

FHitMaskDrawingPolicy::FHitMaskDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource
	):
	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource)
{
	VertexShader = InMaterialResource.GetShader<FHitMaskVertexShader>(InVertexFactory->GetType());
	PixelShader = InMaterialResource.GetShader<FHitMaskPixelShader>(InVertexFactory->GetType());
}

void FHitMaskDrawingPolicy::DrawShared(const FSceneView* View,FBoundShaderStateRHIParamRef BoundShaderState, const ElementDataType& ElementData) const
{
	// Set the shader parameters for the material - HitInfoStruct data.
	VertexShader->SetParameters(VertexFactory,MaterialRenderProxy,*View, ElementData);
	PixelShader->SetParameters(VertexFactory,MaterialRenderProxy,View, ElementData);

	// Set the shared mesh resources.
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
FBoundShaderStateRHIRef FHitMaskDrawingPolicy::CreateBoundShaderState(DWORD DynamicStride)
{
	FVertexDeclarationRHIRef VertexDeclaration;
	DWORD StreamStrides[MaxVertexElementCount];

	FMeshDrawingPolicy::GetVertexDeclarationInfo(VertexDeclaration, StreamStrides);

	if (DynamicStride)
	{
		StreamStrides[0] = DynamicStride;
	}	

	// Use the compiled pixel shader for masked materials, since they need to clip,
	// and the NULL pixel shader for opaque materials to get double speed z on Xenon.
	return RHICreateBoundShaderState(
		VertexDeclaration, 
		StreamStrides, 
		VertexShader->GetVertexShader(), 
		PixelShader->GetPixelShader(),
		EGST_None);
}

void FHitMaskDrawingPolicy::SetMeshRenderState(
	const FSceneView& View,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FMeshBatch& Mesh,
	INT BatchElementIndex,
	UBOOL bBackFace,
	const ElementDataType& ElementData
	) const
{
	VertexShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
	PixelShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace);

	FMeshDrawingPolicy::SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,FMeshDrawingPolicy::ElementDataType());
}

INT Compare(const FHitMaskDrawingPolicy& A,const FHitMaskDrawingPolicy& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
	COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
	return 0;
}

UBOOL FHitMaskDrawingPolicyFactory::IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy)
{
	// if not DefaultMaterial, just ignore
	return (!MaterialRenderProxy->GetMaterial()->IsSpecialEngineMaterial());
}

UBOOL FHitMaskDrawingPolicyFactory::DrawDynamicMesh(
	const FSceneView& View,
	ContextType HitInfo,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId
	)
{
	// Use DefaultEngine material
	const FMaterialRenderProxy* MaterialRenderProxy = GEngine->DefaultMaterial->GetRenderProxy(FALSE);
	const FMaterial* Material = MaterialRenderProxy->GetMaterial();
	const EBlendMode BlendMode = Material->GetBlendMode();

	FHitMaskDrawingPolicy DrawingPolicy(Mesh.VertexFactory,MaterialRenderProxy,*Material);
	DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()), HitInfo);
	for( INT BatchElementIndex=0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++ )
	{
		DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,HitInfo);
		DrawingPolicy.DrawMesh(Mesh,BatchElementIndex);
	}
	return TRUE;
}
