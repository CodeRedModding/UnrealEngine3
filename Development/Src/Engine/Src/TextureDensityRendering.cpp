/*=============================================================================
	TextureDensityRendering.cpp: Implementation for rendering texture density.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"

// This define must match the corresponding one in TextureDensityShader.usf
#define MAX_LOOKUPS 16

/**
 * A vertex shader for rendering texture density.
 */
class FTextureDensityVertexShader : public FMeshMaterialVertexShader
{
	DECLARE_SHADER_TYPE(FTextureDensityVertexShader,MeshMaterial);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Only compile the shaders for the default material or if it's masked.
		return Material->GetUserTexCoordsUsed() > 0 
			&& (Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->MaterialModifiesMeshPosition())
			&& AllowDebugViewmodes(Platform);
	}

	FTextureDensityVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialVertexShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
	}
	FTextureDensityVertexShader() {}
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
		Ar << MaterialParameters;
		return bShaderHasOutdatedParameters;
	}
	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView& View)
	{
		VertexFactoryParameters.Set(this,VertexFactory,View);
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy,*MaterialRenderProxy->GetMaterial(),View.Family->CurrentWorldTime,View.Family->CurrentRealTime,&View);
		MaterialParameters.Set( this, MaterialRenderContext);
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

IMPLEMENT_MATERIAL_SHADER_TYPE(,FTextureDensityVertexShader,TEXT("TextureDensityShader"),TEXT("MainVertexShader"),SF_Vertex,0,0); 

#if WITH_D3D11_TESSELLATION

/**
 * A hull shader for rendering texture density.
 */
class FTextureDensityHullShader : public FBaseHullShader
{
	DECLARE_SHADER_TYPE(FTextureDensityHullShader,MeshMaterial);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHullShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& FTextureDensityVertexShader::ShouldCache(Platform,Material,VertexFactoryType);
	}

	FTextureDensityHullShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseHullShader(Initializer)
	{}

	FTextureDensityHullShader() {}
};

/**
 * A domain shader for rendering texture density.
 */
class FTextureDensityDomainShader : public FBaseDomainShader
{
	DECLARE_SHADER_TYPE(FTextureDensityDomainShader,MeshMaterial);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDomainShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& FTextureDensityVertexShader::ShouldCache(Platform,Material,VertexFactoryType);		
	}

	FTextureDensityDomainShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseDomainShader(Initializer)
	{}

	FTextureDensityDomainShader() {}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FTextureDensityHullShader,TEXT("TextureDensityShader"),TEXT("MainHull"),SF_Hull,0,0); 
IMPLEMENT_MATERIAL_SHADER_TYPE(,FTextureDensityDomainShader,TEXT("TextureDensityShader"),TEXT("MainDomain"),SF_Domain,0,0);

#endif

/**
 * A pixel shader for rendering texture density.
 */
class FTextureDensityPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FTextureDensityPixelShader,MeshMaterial);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Only compile the shaders for the default material or if it's masked.
		return Material->GetUserTexCoordsUsed() > 0 
			&& (Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->MaterialModifiesMeshPosition())
			&& AllowDebugViewmodes(Platform);
	}

	FTextureDensityPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
		TextureDensityParameters.Bind(Initializer.ParameterMap,TEXT("TextureDensityParameters"));
		TextureLookupInfo.Bind(Initializer.ParameterMap,TEXT("TextureLookupInfo"));
	}

	FTextureDensityPixelShader() {}

	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView& View, const FMaterialRenderProxy* OriginalRenderProxy)
	{
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		const FMaterial* OriginalMaterial = OriginalRenderProxy->GetMaterial();
		const TArray<FMaterial::FTextureLookup> &LookupInfo = OriginalMaterial->GetTextureLookupInfo();
		INT NumLookups = Min( LookupInfo.Num(), MAX_LOOKUPS );

		FVector4 LookupParameters[ MAX_LOOKUPS ];
		FVector4 DensityParameters(
			NumLookups,
			GEngine->MinTextureDensity * GEngine->MinTextureDensity,
			GEngine->IdealTextureDensity * GEngine->IdealTextureDensity,
			GEngine->MaxTextureDensity * GEngine->MaxTextureDensity );

		for( INT LookupIndex=0; LookupIndex < NumLookups; ++LookupIndex )
		{
			const FMaterial::FTextureLookup &Lookup = LookupInfo( LookupIndex );
			if (OriginalMaterial->GetUniform2DTextureExpressions().IsValidIndex(Lookup.TextureIndex))
			{
				const FTexture* Texture = NULL;
				OriginalMaterial->GetUniform2DTextureExpressions()( Lookup.TextureIndex )->GetTextureValue( MaterialRenderContext, *OriginalMaterial, Texture );
				check( Texture );
				LookupParameters[LookupIndex][0] = FLOAT(Texture->GetSizeX()) * Lookup.UScale;
				LookupParameters[LookupIndex][1] = FLOAT(Texture->GetSizeY()) * Lookup.VScale;
			}
			else
			{
				LookupParameters[LookupIndex][0] = 0;
				LookupParameters[LookupIndex][1] = 0;
			}
			
			LookupParameters[LookupIndex][2] = Lookup.TexCoordIndex;
		}

		SetPixelShaderValues( GetPixelShader(), TextureLookupInfo, LookupParameters, NumLookups );
		SetPixelShaderValue( GetPixelShader(), TextureDensityParameters, DensityParameters );

		MaterialParameters.Set( this, MaterialRenderContext);
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View,UBOOL bBackFace)
	{
		MaterialParameters.SetMesh( this, PrimitiveSceneInfo, Mesh, BatchElementIndex, View, bBackFace);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << MaterialParameters;
		Ar << TextureDensityParameters;
		Ar << TextureLookupInfo;
		return bShaderHasOutdatedParameters;
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialPixelShaderParameters MaterialParameters;
	FShaderParameter TextureDensityParameters;
	FShaderParameter TextureLookupInfo;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FTextureDensityPixelShader,TEXT("TextureDensityShader"),TEXT("MainPixelShader"),SF_Pixel,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);


//=============================================================================
/** FTextureDensityDrawingPolicy - Policy to wrap FMeshDrawingPolicy with new shaders. */

FTextureDensityDrawingPolicy::FTextureDensityDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterialRenderProxy* InOriginalRenderProxy
	)
	:	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,*InMaterialRenderProxy->GetMaterial())
	,	OriginalRenderProxy(InOriginalRenderProxy)
{
	const FMaterialShaderMap* MaterialShaderIndex = InMaterialRenderProxy->GetMaterial()->GetShaderMap();
	const FMeshMaterialShaderMap* MeshShaderIndex = MaterialShaderIndex->GetMeshShaderMap(InVertexFactory->GetType());
	const FMaterial* MaterialResource = InMaterialRenderProxy->GetMaterial();

#if WITH_D3D11_TESSELLATION
	HullShader = NULL;
	DomainShader = NULL;

	const EMaterialTessellationMode MaterialTessellationMode = MaterialResource->GetD3D11TessellationMode();
	if(GRHIShaderPlatform == SP_PCD3D_SM5
		&& InVertexFactory->GetType()->SupportsTessellationShaders() 
		&& MaterialTessellationMode != MTM_NoTessellation)
	{
		UBOOL HasHullShader = MeshShaderIndex->HasShader(&FTextureDensityHullShader::StaticType);
		UBOOL HasDomainShader = MeshShaderIndex->HasShader(&FTextureDensityDomainShader::StaticType);

		HullShader = HasHullShader ? MeshShaderIndex->GetShader<FTextureDensityHullShader>() : NULL;
		DomainShader = HasDomainShader ? MeshShaderIndex->GetShader<FTextureDensityDomainShader>() : NULL;
	}
#endif

	UBOOL HasVertexShader = MeshShaderIndex->HasShader(&FTextureDensityVertexShader::StaticType);
	VertexShader = HasVertexShader ? MeshShaderIndex->GetShader<FTextureDensityVertexShader>() : NULL;
	
	UBOOL HasPixelShader = MeshShaderIndex->HasShader(&FTextureDensityPixelShader::StaticType);
	PixelShader = HasPixelShader ? MeshShaderIndex->GetShader<FTextureDensityPixelShader>() : NULL;
}

void FTextureDensityDrawingPolicy::DrawShared( const FSceneView* SceneView, FBoundShaderStateRHIRef ShaderState ) const
{
	// NOTE: Assuming this cast is always safe!
	FViewInfo* View = (FViewInfo*) SceneView;

	// Set the depth-only shader parameters for the material.
	RHISetBoundShaderState( ShaderState );
	VertexShader->SetParameters( VertexFactory, MaterialRenderProxy, *View );
	PixelShader->SetParameters( VertexFactory, MaterialRenderProxy, *View, OriginalRenderProxy );

#if WITH_D3D11_TESSELLATION
	if(HullShader && DomainShader)
	{
		HullShader->SetParameters(MaterialRenderProxy, *View);
		DomainShader->SetParameters(MaterialRenderProxy, *View);
	}
#endif

	// Set the shared mesh resources.
	FMeshDrawingPolicy::DrawShared( View );
}

void FTextureDensityDrawingPolicy::SetMeshRenderState(
	const FSceneView& View,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FMeshBatch& Mesh,
	INT BatchElementIndex,
	UBOOL bBackFace,
	const ElementDataType& ElementData
	) const
{
	VertexShader->SetMesh(PrimitiveSceneInfo, Mesh, BatchElementIndex, View);
	PixelShader->SetMesh(PrimitiveSceneInfo, Mesh, BatchElementIndex, View, bBackFace);

#if WITH_D3D11_TESSELLATION
	if(HullShader && DomainShader)
	{
		HullShader->SetMesh(PrimitiveSceneInfo, Mesh, BatchElementIndex, View);
		DomainShader->SetMesh(PrimitiveSceneInfo, Mesh, BatchElementIndex, View);
	}
#endif

	FMeshDrawingPolicy::SetMeshRenderState(View, PrimitiveSceneInfo, Mesh, BatchElementIndex, bBackFace, ElementData);
}

/** 
 * Create bound shader state using the vertex decl from the mesh draw policy
 * as well as the shaders needed to draw the mesh
 * @param DynamicStride - optional stride for dynamic vertex data
 * @return new bound shader state object
 */
FBoundShaderStateRHIRef FTextureDensityDrawingPolicy::CreateBoundShaderState(DWORD DynamicStride)
{
	FVertexDeclarationRHIRef VertexDeclaration;
	DWORD StreamStrides[MaxVertexElementCount];

	FMeshDrawingPolicy::GetVertexDeclarationInfo(VertexDeclaration, StreamStrides);
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

INT Compare(const FTextureDensityDrawingPolicy& A,const FTextureDensityDrawingPolicy& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
	COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
#if WITH_D3D11_TESSELLATION
	COMPAREDRAWINGPOLICYMEMBERS(HullShader);
	COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
#endif
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
	return 0;
}


//=============================================================================
/** Policy to wrap FMeshDrawingPolicy with new shaders. */

UBOOL FTextureDensityDrawingPolicyFactory::DrawDynamicMesh(
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId
	)
{
	// Only draw opaque materials in the depth pass.
	const FMaterialRenderProxy* OriginalRenderProxy = Mesh.MaterialRenderProxy;
	const FMaterialRenderProxy* MaterialRenderProxy = OriginalRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterial();
	const FMaterial::FTextureLookupInfo& TextureLookupInfo = Material->GetTextureLookupInfo();
	if ( TextureLookupInfo.Num() > 0 )
	{
		if ( !Material->IsMasked() && !Material->MaterialModifiesMeshPosition() )
		{
			// Default material doesn't handle masked.
			MaterialRenderProxy = GEngine->DefaultMaterial->GetRenderProxy(FALSE);
		}
		FTextureDensityDrawingPolicy DrawingPolicy( Mesh.VertexFactory, MaterialRenderProxy, OriginalRenderProxy );
		DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
		for( INT BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
		{
			DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,FMeshDrawingPolicy::ElementDataType());
			DrawingPolicy.DrawMesh(Mesh,BatchElementIndex);
		}
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/** Renders world-space texture density instead of the normal color. */
UBOOL FSceneRenderer::RenderTextureDensities(UINT DPGIndex)
{
	SCOPED_DRAW_EVENT(EventTextureDensity)(DEC_SCENE_ITEMS,TEXT("RenderTextureDensity"));

	UBOOL bWorldDpg = (DPGIndex == SDPG_World);
	UBOOL bDirty = FALSE;

	// Opaque blending, enable depth tests and writes.
	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetDepthState(TStaticDepthState<TRUE,CF_LessEqual>::GetRHI());

	for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENT(EventView,Views.Num() > 1)(DEC_SCENE_ITEMS,TEXT("View%d"),ViewIndex);

		FViewInfo& View = Views(ViewIndex);
		const FSceneViewState* ViewState = (FSceneViewState*)View.State;

		// Set the device viewport for the view.
		RHISetViewport(View.RenderTargetX,View.RenderTargetY,0.0f,View.RenderTargetX + View.RenderTargetSizeX,View.RenderTargetY + View.RenderTargetSizeY,1.0f);
		RHISetViewParameters(View);
		RHISetMobileHeightFogParams(View.HeightFogParams);

		// Draw texture density for dynamic meshes.
		TDynamicPrimitiveDrawer<FTextureDensityDrawingPolicyFactory> Drawer(&View,DPGIndex,FTextureDensityDrawingPolicyFactory::ContextType(),TRUE);
		for(INT PrimitiveIndex = 0;PrimitiveIndex < View.VisibleDynamicPrimitives.Num();PrimitiveIndex++)
		{
			const FPrimitiveSceneInfo* PrimitiveSceneInfo = View.VisibleDynamicPrimitives(PrimitiveIndex);
			const FPrimitiveViewRelevance& PrimitiveViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);

			const UBOOL bVisible = View.PrimitiveVisibilityMap(PrimitiveSceneInfo->Id);
			if( bVisible &&		PrimitiveViewRelevance.GetDPG(DPGIndex) )
			{
				Drawer.SetPrimitive(PrimitiveSceneInfo);
				PrimitiveSceneInfo->Proxy->DrawDynamicElements(
					&Drawer,
					&View,
					DPGIndex
					);
			}
		}
		bDirty |= Drawer.IsDirty();
	}

	return bDirty;
}
