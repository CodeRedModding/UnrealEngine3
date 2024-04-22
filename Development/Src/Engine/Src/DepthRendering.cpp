/*=============================================================================
	DepthRendering.cpp: Depth rendering implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"

/**
 * A vertex shader for rendering the depth of a mesh.
 */
template <UBOOL bUsePositionOnlyStream>
class TDepthOnlyVertexShader : public FMeshMaterialVertexShader
{
	DECLARE_SHADER_TYPE(TDepthOnlyVertexShader,MeshMaterial);
protected:

	TDepthOnlyVertexShader() {}
	TDepthOnlyVertexShader(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialVertexShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
	}

public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Only the local vertex factory supports the position-only stream
		if (bUsePositionOnlyStream)
		{
			if (appStrstr(VertexFactoryType->GetName(), TEXT("FLocalVertex")) || appStrstr(VertexFactoryType->GetName(), TEXT("FInstancedStaticMeshVertex")) || appStrstr(VertexFactoryType->GetName(), TEXT("FSplineMeshVertex")))
			{
				return Material->IsSpecialEngineMaterial();
			}
			return FALSE;
		}

		// Only compile for the default material and masked materials
		return Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->MaterialModifiesMeshPosition() || Material->RenderLitTranslucencyPrepass() || Material->RenderLitTranslucencyDepthPostpass();
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
		Ar << MaterialParameters;
		return bShaderHasOutdatedParameters;
	}
	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FMaterial& MaterialResource,const FSceneView& View)
	{
		VertexFactoryParameters.Set(this,VertexFactory,View);
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, MaterialResource, View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set(this,MaterialRenderContext);
	}
	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshBatch& Mesh, INT BatchElementIndex,const FSceneView& View)
	{
		VertexFactoryParameters.SetMesh(this, Mesh, BatchElementIndex,View);
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo, Mesh, BatchElementIndex,View);
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
 * Hull shader for depth rendering
 */
class FDepthOnlyHullShader : public FBaseHullShader
{
	DECLARE_SHADER_TYPE(FDepthOnlyHullShader,MeshMaterial);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHullShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& TDepthOnlyVertexShader<FALSE>::ShouldCache(Platform,Material,VertexFactoryType);
	}

	FDepthOnlyHullShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseHullShader(Initializer)
	{}

	FDepthOnlyHullShader() {}
};

/**
 * Domain shader for depth rendering
 */
class FDepthOnlyDomainShader : public FBaseDomainShader
{
	DECLARE_SHADER_TYPE(FDepthOnlyDomainShader,MeshMaterial);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDomainShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& TDepthOnlyVertexShader<FALSE>::ShouldCache(Platform,Material,VertexFactoryType);		
	}

	FDepthOnlyDomainShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseDomainShader(Initializer)
	{}

	FDepthOnlyDomainShader() {}
};

#endif

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDepthOnlyVertexShader<TRUE>,TEXT("PositionOnlyDepthVertexShader"),TEXT("Main"),SF_Vertex,0,0);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDepthOnlyVertexShader<FALSE>,TEXT("DepthOnlyVertexShader"),TEXT("Main"),SF_Vertex,0,0);

#if WITH_D3D11_TESSELLATION
IMPLEMENT_MATERIAL_SHADER_TYPE(,FDepthOnlyHullShader,TEXT("DepthOnlyVertexShader"),TEXT("MainHull"),SF_Hull,0,0);	
IMPLEMENT_MATERIAL_SHADER_TYPE(,FDepthOnlyDomainShader,TEXT("DepthOnlyVertexShader"),TEXT("MainDomain"),SF_Domain,0,0);
#endif

/**
* A pixel shader for rendering the depth of a mesh.
*/
template< UBOOL bUseScreenDoorFade >
class TDepthOnlyPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(TDepthOnlyPixelShader,MeshMaterial);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Compile for materials that are masked.  We'll also compile for default materials as we'll selectively
		// enable the pixel shader for opaque primitives that are currently fading in/out using a screen door fade.
		if( bUseScreenDoorFade )
		{
			return Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->RenderLitTranslucencyPrepass();
		}
		else
		{
			return Material->IsMasked() || Material->RenderLitTranslucencyPrepass();
		}
	}

	TDepthOnlyPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
	}

	TDepthOnlyPixelShader() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		// For the screen door variant on SM3 or higher we'll enable extra clipping functionality in the shader
		if( bUseScreenDoorFade )
		{
			// Force even the default material to cache a depth-only shader variant with screen door clipping enabled
			OutEnvironment.Definitions.Set( TEXT( "MATERIAL_USE_SCREEN_DOOR_FADE" ), TEXT( "1" ) );
		}
	}

	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FMaterial& MaterialResource,const FSceneView* View)
	{
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, MaterialResource, View->Family->CurrentWorldTime, View->Family->CurrentRealTime, View);
		MaterialParameters.Set(this,MaterialRenderContext);
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshBatch& Mesh, INT BatchElementIndex,const FSceneView& View,UBOOL bBackFace)
	{
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
#if PS3
		// @todo hack: this works around a gcc 411 compiler issue where is strips this function out with the -Wl,--gc-sections linker option
		// it just needs to access a global variable to keep the function from being stripped
		GNumHardwareThreads = 2;
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

typedef TDepthOnlyPixelShader<FALSE> TDepthOnlySolidPixelShader;
typedef TDepthOnlyPixelShader<TRUE> TDepthOnlyScreenDoorPixelShader;

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDepthOnlySolidPixelShader,TEXT("DepthOnlyPixelShader"),TEXT("Main"),SF_Pixel,0,0);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDepthOnlyScreenDoorPixelShader,TEXT("DepthOnlyPixelShader"),TEXT("Main"),SF_Pixel,0,0);

/**
* A pixel shader for rendering the depth of a translucent mesh in all areas not 100% transparent.
*/
class FTranslucencyPostRenderDepthPixelShader : public TDepthOnlySolidPixelShader
{
	DECLARE_SHADER_TYPE(FTranslucencyPostRenderDepthPixelShader,MeshMaterial);
public:
	FTranslucencyPostRenderDepthPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	TDepthOnlySolidPixelShader(Initializer)
	{}

	FTranslucencyPostRenderDepthPixelShader() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("OUTPUT_DEPTH_TO_ALPHA"),TEXT("1"));
	}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Only compile for lit translucency materials needing a post-render pass
		return Material->RenderLitTranslucencyDepthPostpass() || Material->GetBlendMode() == BLEND_SoftMasked;
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FTranslucencyPostRenderDepthPixelShader,TEXT("TranslucencyPostRenderDepthPixelShader"),TEXT("Main"),SF_Pixel,0,0);

FDepthDrawingPolicy::FDepthDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	UBOOL bForceUsePixelShader,
	UBOOL bForceDefaultPixelShader,
	UBOOL bOutputDepthToSceneColorAlpha
	):
	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource,FALSE,FALSE,0.f,bOutputDepthToSceneColorAlpha)
{
	// The primitive needs to be rendered with the material's pixel and vertex shaders if it is masked
	bNeedsPixelShader = FALSE;
	if (bForceUsePixelShader || InMaterialResource.IsMasked())
	{
		bNeedsPixelShader = TRUE;

		if( bOutputDepthToSceneColorAlpha )
		{
			PixelShader = InMaterialResource.GetShader<FTranslucencyPostRenderDepthPixelShader>(InVertexFactory->GetType());
		}
		else
		{
			PixelShader = InMaterialResource.GetShader<TDepthOnlySolidPixelShader>(InVertexFactory->GetType());
		}
	}
	else
	{
		// Do we want to use the default material's pixel shader?
		if( bForceDefaultPixelShader && !bOutputDepthToSceneColorAlpha )
		{
			bNeedsPixelShader = TRUE;

			// We were asked to use the default material's pixel shader for this depth drawing policy.  This
			// is used when a primitive may need to be rendered as masked (as a dynamic primitive) even when
			// it's usually treated opaque.  This is used for fading objects in and out using a screen door effect.
			const FMaterial* DefaultMaterialResource = GEngine->DefaultMaterial->GetRenderProxy(FALSE)->GetMaterial();
			PixelShader = (TDepthOnlySolidPixelShader*)DefaultMaterialResource->GetShader<TDepthOnlyScreenDoorPixelShader>(InVertexFactory->GetType());
		}
		else
		{
			PixelShader = NULL;
		}
	}

	const EMaterialTessellationMode TessellationMode = InMaterialResource.GetD3D11TessellationMode();
	VertexShader = NULL;

#if WITH_D3D11_TESSELLATION
	HullShader = NULL;	
	DomainShader = NULL;

	if(GRHIShaderPlatform == SP_PCD3D_SM5
		&& InVertexFactory->GetType()->SupportsTessellationShaders() 
		&& TessellationMode != MTM_NoTessellation)
	{
		VertexShader = InMaterialResource.GetShader<TDepthOnlyVertexShader<FALSE> >(VertexFactory->GetType());
		HullShader = InMaterialResource.GetShader<FDepthOnlyHullShader>(VertexFactory->GetType());
		DomainShader = InMaterialResource.GetShader<FDepthOnlyDomainShader>(VertexFactory->GetType());
	}
	else
#endif
	if(!bForceUsePixelShader && !InMaterialResource.IsMasked() && !InMaterialResource.MaterialModifiesMeshPosition())
	{
		const FMaterial* DefaultMaterialResource = GEngine->DefaultMaterial->GetRenderProxy(FALSE)->GetMaterial();
		VertexShader = DefaultMaterialResource->GetShader<TDepthOnlyVertexShader<FALSE> >(InVertexFactory->GetType());
	}
	else
	{
		VertexShader = InMaterialResource.GetShader<TDepthOnlyVertexShader<FALSE> >(InVertexFactory->GetType());
	}

}

void FDepthDrawingPolicy::DrawShared(const FSceneView* View,FBoundShaderStateRHIParamRef BoundShaderState) const
{
	// Set the depth-only shader parameters for the material.
	VertexShader->SetParameters(VertexFactory,MaterialRenderProxy,*MaterialResource,*View);
#if WITH_D3D11_TESSELLATION
	if(HullShader && DomainShader)
	{
		HullShader->SetParameters(MaterialRenderProxy,*View);
		DomainShader->SetParameters(MaterialRenderProxy,*View);
	}
#endif

	if (bNeedsPixelShader)
	{
		PixelShader->SetParameters(VertexFactory,MaterialRenderProxy,*MaterialResource,View);
	}

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
FBoundShaderStateRHIRef FDepthDrawingPolicy::CreateBoundShaderState(DWORD DynamicStride)
{
	FVertexDeclarationRHIRef VertexDeclaration;
	DWORD StreamStrides[MaxVertexElementCount];

	FMeshDrawingPolicy::GetVertexDeclarationInfo(VertexDeclaration, StreamStrides);

	if (DynamicStride)
	{
        StreamStrides[0] = DynamicStride;
	}	
	
#if WITH_D3D11_TESSELLATION
	return RHICreateBoundShaderStateD3D11(
		VertexDeclaration, 
		StreamStrides, 
		VertexShader->GetVertexShader(),
		GETSAFERHISHADER_HULL(HullShader), 
		GETSAFERHISHADER_DOMAIN(DomainShader),
		bNeedsPixelShader ? PixelShader->GetPixelShader() : NULL,
		NULL,
		EGST_None);
#else
	// Use the compiled pixel shader for masked materials, since they need to clip,
	// and the NULL pixel shader for opaque materials to get double speed z on Xenon.
	return RHICreateBoundShaderState(
		VertexDeclaration, 
		StreamStrides, 
		VertexShader->GetVertexShader(), 
		bNeedsPixelShader ? PixelShader->GetPixelShader() : NULL,
		EGST_None);
#endif
}

void FDepthDrawingPolicy::SetMeshRenderState(
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

	if (bNeedsPixelShader)
	{
		PixelShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace);
	}
	FMeshDrawingPolicy::SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,ElementData);
}

INT Compare(const FDepthDrawingPolicy& A,const FDepthDrawingPolicy& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
#if WITH_D3D11_TESSELLATION
	COMPAREDRAWINGPOLICYMEMBERS(HullShader);
	COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
#endif
	COMPAREDRAWINGPOLICYMEMBERS(bNeedsPixelShader);
	COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
	return 0;
}

FPositionOnlyDepthDrawingPolicy::FPositionOnlyDepthDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource
	):
	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource)
{
	VertexShader = InMaterialResource.GetShader<TDepthOnlyVertexShader<TRUE> >(InVertexFactory->GetType());
}

void FPositionOnlyDepthDrawingPolicy::DrawShared(const FSceneView* View,FBoundShaderStateRHIParamRef BoundShaderState) const
{
	// Set the depth-only shader parameters for the material.
	VertexShader->SetParameters(VertexFactory,MaterialRenderProxy,*MaterialResource,*View);

	// Set the shared mesh resources.
	VertexFactory->SetPositionStream();

	// Set the actual shader & vertex declaration state
	RHISetBoundShaderState( BoundShaderState);
}

/** 
* Create bound shader state using the vertex decl from the mesh draw policy
* as well as the shaders needed to draw the mesh
* @param DynamicStride - optional stride for dynamic vertex data
* @return new bound shader state object
*/
FBoundShaderStateRHIRef FPositionOnlyDepthDrawingPolicy::CreateBoundShaderState(DWORD DynamicStride)
{
	FVertexDeclarationRHIParamRef VertexDeclaration;
	DWORD StreamStrides[MaxVertexElementCount];
	
	VertexFactory->GetPositionStreamStride(StreamStrides);
	VertexDeclaration = VertexFactory->GetPositionDeclaration();

	if (DynamicStride)
	{
        StreamStrides[0] = DynamicStride;
	}	

	checkSlow(MaterialRenderProxy->GetMaterial()->GetBlendMode() == BLEND_Opaque);
	return RHICreateBoundShaderState(VertexDeclaration, StreamStrides, VertexShader->GetVertexShader(), FPixelShaderRHIRef(), EGST_None);
}

void FPositionOnlyDepthDrawingPolicy::SetMeshRenderState(
	const FSceneView& View,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FMeshBatch& Mesh,
	INT BatchElementIndex,
	UBOOL bBackFace,
	const ElementDataType& ElementData
	) const
{
	VertexShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
	FMeshDrawingPolicy::SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,ElementData);
}

INT Compare(const FPositionOnlyDepthDrawingPolicy& A,const FPositionOnlyDepthDrawingPolicy& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
	return 0;
}

void FDepthDrawingPolicyFactory::AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh)
{
	const FMaterialRenderProxy* MaterialRenderProxy = StaticMesh->MaterialRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterial();
	const EBlendMode BlendMode = Material->GetBlendMode();

	if (Material->IsMasked())
	{
		// Add the static mesh to the masked depth-only draw list.

		if(BlendMode == BLEND_SoftMasked)
		{
			// will be always drawn
			Scene->DPGs[StaticMesh->DepthPriorityGroup].SoftMaskedDepthDrawList.AddMesh(
				StaticMesh,
				FDepthDrawingPolicy::ElementDataType(),
				FDepthDrawingPolicy(StaticMesh->VertexFactory,MaterialRenderProxy,*Material)
				);
		}
		else
		{
			// only draw if required
			Scene->DPGs[StaticMesh->DepthPriorityGroup].MaskedDepthDrawList.AddMesh(
				StaticMesh,
				FDepthDrawingPolicy::ElementDataType(),
				FDepthDrawingPolicy(StaticMesh->VertexFactory,MaterialRenderProxy,*Material)
				);
		}
	}
	else
	{
		if (StaticMesh->VertexFactory->SupportsPositionOnlyStream() 
			&& !Material->IsTwoSided()
			&& !Material->MaterialModifiesMeshPosition())
		{
			const FMaterialRenderProxy* DefaultProxy = GEngine->DefaultMaterial->GetRenderProxy(FALSE);
			// Add the static mesh to the position-only depth draw list.
			Scene->DPGs[StaticMesh->DepthPriorityGroup].PositionOnlyDepthDrawList.AddMesh(
				StaticMesh,
				FPositionOnlyDepthDrawingPolicy::ElementDataType(),
				FPositionOnlyDepthDrawingPolicy(StaticMesh->VertexFactory,DefaultProxy,*DefaultProxy->GetMaterial())
				);
		}
		else
		{
			if (!Material->IsTwoSided() && !Material->MaterialModifiesMeshPosition())
			{
				// Override with the default material for everything but opaque two sided materials
				MaterialRenderProxy = GEngine->DefaultMaterial->GetRenderProxy(FALSE);
			}

			// Add the static mesh to the opaque depth-only draw list.
			Scene->DPGs[StaticMesh->DepthPriorityGroup].DepthDrawList.AddMesh(
				StaticMesh,
				FDepthDrawingPolicy::ElementDataType(),
				FDepthDrawingPolicy(StaticMesh->VertexFactory,MaterialRenderProxy,*MaterialRenderProxy->GetMaterial())
				);
		}
	}
}

UBOOL FDepthDrawingPolicyFactory::DrawDynamicMesh(
	const FSceneView& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId
	)
{
	UBOOL bDirty = FALSE;

	//Do a per-FMeshBatch check on top of the proxy check in RenderPrePass to handle the case where a proxy that is relevant 
	//to the depth only pass has to submit multiple FMeshBatchs but only some of them should be used as occluders.
	if (Mesh.bUseAsOccluder)
	{
		const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterial();
		const EBlendMode BlendMode = Material->GetBlendMode();

		// Check to see if the primitive is currently fading in or out using the screen door effect.  If it is,
		// then we can't assume the object is opaque as it may be forcibly masked.
		const FSceneViewState* SceneViewState = static_cast<const FSceneViewState*>( View.State );
		const UBOOL bIsMaskedForScreenDoorFade =
			SceneViewState != NULL && PrimitiveSceneInfo != NULL &&
			SceneViewState->IsPrimitiveFading( PrimitiveSceneInfo->Component );

		if ( BlendMode == BLEND_Opaque 
			&& Mesh.VertexFactory->SupportsPositionOnlyStream() 
			&& !Material->IsTwoSided()
			&& !Material->MaterialModifiesMeshPosition()
			&& !bIsMaskedForScreenDoorFade)
		{
			//render opaque primitives that support a separate position-only vertex buffer
			const FMaterialRenderProxy* DefaultProxy = GEngine->DefaultMaterial->GetRenderProxy(FALSE);
			FPositionOnlyDepthDrawingPolicy DrawingPolicy(Mesh.VertexFactory,DefaultProxy,*DefaultProxy->GetMaterial());
			DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
			for( INT BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
			{
				DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,FPositionOnlyDepthDrawingPolicy::ElementDataType());
				DrawingPolicy.DrawMesh(Mesh,BatchElementIndex);
			}
			bDirty = TRUE;
		}
		else if (!IsTranslucentBlendMode(BlendMode))
		{
			const UBOOL bMaterialMasked = Material->IsMasked() || bIsMaskedForScreenDoorFade;

			UBOOL bDraw = TRUE;

			switch(DrawingContext.DepthDrawingMode)
			{
				case DDM_AllOccluders: 
					break;
				case DDM_NonMaskedOnly: 
					bDraw = !bMaterialMasked;
					break;
				case DDM_SoftMaskedOnly:
					bDraw = BlendMode == BLEND_SoftMasked;
					break;
				default:
					check(!"Unrecognized DepthDrawingMode");
			}

			if(bDraw)
			{
				if (!bMaterialMasked && !Material->IsTwoSided() && !Material->MaterialModifiesMeshPosition())
				{
					// Override with the default material for opaque materials that are not two sided
					MaterialRenderProxy = GEngine->DefaultMaterial->GetRenderProxy(FALSE);
				}

				// If we're drawing a screen door masked primitive then force use of a pixel shader as
				// we need to be able to sample the screen door noise texture and clip the pixel
				const UBOOL bForceDefaultPixelShader = bIsMaskedForScreenDoorFade;

				FDepthDrawingPolicy DrawingPolicy(Mesh.VertexFactory,MaterialRenderProxy,*MaterialRenderProxy->GetMaterial(),FALSE,bForceDefaultPixelShader,DrawingContext.bOutputDepthToSceneAlpha);
				DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
				for( INT BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
				{
					DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,FMeshDrawingPolicy::ElementDataType());
					DrawingPolicy.DrawMesh(Mesh,BatchElementIndex);
				}
				bDirty = TRUE;
			}
		}
	}
	
	return bDirty;
}


UBOOL FDepthDrawingPolicyFactory::DrawStaticMesh(
	const FViewInfo& View,
	ContextType DrawingContext,
	const FStaticMesh& StaticMesh,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId
	)
{
	UBOOL bDirty = FALSE;

	const FMaterial* Material = StaticMesh.MaterialRenderProxy->GetMaterial();
	const EMaterialLightingModel LightingModel = Material->GetLightingModel();
	const UBOOL bNeedsBackfacePass =
		Material->IsTwoSided() &&
		(LightingModel != MLM_NonDirectional) &&
		Material->RenderTwoSidedSeparatePass();
	INT bBackFace = bNeedsBackfacePass ? 1 : 0;
	do
	{
		bDirty |= DrawDynamicMesh(
			View,
			DrawingContext,
			StaticMesh,
			bBackFace,
			bPreFog,
			PrimitiveSceneInfo,
			HitProxyId
			);
		--bBackFace;
	} while( bBackFace >= 0 );

	return bDirty;
}

UBOOL FDepthDrawingPolicyFactory::IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy)
{
	return IsTranslucentBlendMode(MaterialRenderProxy->GetMaterial()->GetBlendMode());
}
