/*=============================================================================
	TemporalAARendering.cpp: Temporal anti-aliasing implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"

/*-----------------------------------------------------------------------------
	FTemporalAAMaskVertexShader - Used to mask out dynamic objects 
-----------------------------------------------------------------------------*/

class FTemporalAAMaskVertexShader : public FMeshMaterialVertexShader
{
	DECLARE_SHADER_TYPE(FTemporalAAMaskVertexShader,MeshMaterial);
protected:

	FTemporalAAMaskVertexShader() {}

	FTemporalAAMaskVertexShader(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialVertexShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
	}

public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Platform != SP_XBOXD3D 
			&& (Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->MaterialModifiesMeshPosition());
	}

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
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set(this,MaterialRenderContext);
	}
	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMeshBatch& Mesh, INT BatchElementIndex,const FSceneView& View)
	{
		VertexFactoryParameters.SetMesh(this, Mesh, BatchElementIndex ,View);
		MaterialParameters.SetMesh(this, PrimitiveSceneInfo, Mesh, BatchElementIndex, View);
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialVertexShaderParameters MaterialParameters;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FTemporalAAMaskVertexShader,TEXT("TemporalAAMaskShader"),TEXT("VertexMain"),SF_Vertex,0,0);

/*-----------------------------------------------------------------------------
	FTemporalAAMaskPixelShader - Used to mask out dynamic objects 
-----------------------------------------------------------------------------*/

class FTemporalAAMaskPixelShader : public FMeshMaterialPixelShader
{
	DECLARE_SHADER_TYPE(FTemporalAAMaskPixelShader,MeshMaterial);
public:
	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType) 
	{ 
		return Platform != SP_XBOXD3D 
			&& (Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->MaterialModifiesMeshPosition());
	}

	FTemporalAAMaskPixelShader() {}

	FTemporalAAMaskPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialPixelShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
		SceneTextureParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView* View)
	{
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), View->Family->CurrentWorldTime, View->Family->CurrentRealTime, View);
		MaterialParameters.Set(this,MaterialRenderContext);
		SceneTextureParameters.Set(View, this);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FMeshMaterialPixelShader::Serialize(Ar);
		Ar << MaterialParameters;
		Ar << SceneTextureParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	
	FMaterialPixelShaderParameters MaterialParameters;
	FSceneTextureShaderParameters SceneTextureParameters;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FTemporalAAMaskPixelShader,TEXT("TemporalAAMaskShader"),TEXT("PixelMain"),SF_Pixel,0,0);

/**
 * Drawing policy for masking out dynamic objects from temporal AA
 */
class FTemporalAAMaskDrawingPolicy : public FMeshDrawingPolicy
{
public:
		
	FTemporalAAMaskDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy
		):
		FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,*InMaterialRenderProxy->GetMaterial())
	{
		const FMaterial* MaterialResource = InMaterialRenderProxy->GetMaterial();

		if (MaterialResource->IsMasked())
		{
			PixelShader = MaterialResource->GetShader<FTemporalAAMaskPixelShader>(InVertexFactory->GetType());
		}
		else
		{
			const FMaterial* DefaultMaterialResource = GEngine->DefaultMaterial->GetRenderProxy(FALSE)->GetMaterial();
			PixelShader = DefaultMaterialResource->GetShader<FTemporalAAMaskPixelShader>(InVertexFactory->GetType());
		}

		if (!MaterialResource->IsMasked() && !MaterialResource->MaterialModifiesMeshPosition())
		{
			const FMaterial* DefaultMaterialResource = GEngine->DefaultMaterial->GetRenderProxy(FALSE)->GetMaterial();
			VertexShader = DefaultMaterialResource->GetShader<FTemporalAAMaskVertexShader>(InVertexFactory->GetType());
		}
		else
		{
			VertexShader = MaterialResource->GetShader<FTemporalAAMaskVertexShader>(InVertexFactory->GetType());
		}
	}

	// FMeshDrawingPolicy interface.
	UBOOL Matches(const FTemporalAAMaskDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) 
			&& VertexShader == Other.VertexShader
			&& PixelShader == Other.PixelShader;
	}

		
	void DrawShared(const FSceneView* View,FBoundShaderStateRHIParamRef BoundShaderState) const
	{
		// Set the depth-only shader parameters for the material.
		VertexShader->SetParameters(VertexFactory,MaterialRenderProxy,*View);
		PixelShader->SetParameters(MaterialRenderProxy,View);

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
	FBoundShaderStateRHIRef CreateBoundShaderState(DWORD DynamicStride)
	{
		FVertexDeclarationRHIRef VertexDeclaration;
		DWORD StreamStrides[MaxVertexElementCount];

		FMeshDrawingPolicy::GetVertexDeclarationInfo(VertexDeclaration, StreamStrides);

		if (DynamicStride)
		{
			StreamStrides[0] = DynamicStride;
		}	

		return RHICreateBoundShaderState(
			VertexDeclaration, 
			StreamStrides, 
			VertexShader->GetVertexShader(), 
			PixelShader->GetPixelShader(),
			EGST_None);
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

		FMeshDrawingPolicy::SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,ElementData);
	}

	friend INT Compare(const FTemporalAAMaskDrawingPolicy& A,const FTemporalAAMaskDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
		COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		return 0;
	}

private:
	FTemporalAAMaskVertexShader* VertexShader;
	FTemporalAAMaskPixelShader* PixelShader;
};

/**
 * Draws meshes using FTemporalAAMaskDrawingPolicy
 */
class FTemporalAAMaskDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = FALSE };
	struct ContextType {};

	static UBOOL DrawDynamicMesh(
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

		const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterial();
		const EBlendMode BlendMode = Material->GetBlendMode();

		if (!IsTranslucentBlendMode(BlendMode))
		{
			if (!Material->IsMasked() && !Material->IsTwoSided() && !Material->MaterialModifiesMeshPosition())
			{
				// Override with the default material for opaque materials that are not two sided
				MaterialRenderProxy = GEngine->DefaultMaterial->GetRenderProxy(FALSE);
			}

			FTemporalAAMaskDrawingPolicy DrawingPolicy(Mesh.VertexFactory,MaterialRenderProxy);
			DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
			for( INT BatchElementIndex=0;BatchElementIndex < Mesh.Elements.Num();BatchElementIndex++ )
			{
				DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,FMeshDrawingPolicy::ElementDataType());
				DrawingPolicy.DrawMesh(Mesh,BatchElementIndex);
			}
			bDirty = TRUE;
		}
		
		return bDirty;
	}

	static UBOOL IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy) { return FALSE; }
};

/** A vertex shader used with the various temporal AA pixel shaders. */
class FTemporalAAVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FTemporalAAVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	FTemporalAAVertexShader()	{}

	FTemporalAAVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		ScreenPositionScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("ScreenPositionScaleBias"),TRUE);
	}

	void SetParameters(const FViewInfo& View)
	{
		SetVertexShaderValue(
			GetVertexShader(),
			ScreenPositionScaleBiasParameter,
			View.ScreenPositionScaleBias
			);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << ScreenPositionScaleBiasParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter ScreenPositionScaleBiasParameter;
};

IMPLEMENT_SHADER_TYPE(,FTemporalAAVertexShader,TEXT("TemporalAA"),TEXT("MainVertexShader"),SF_Vertex,0,0);

/** Generates a depth based mask on non-Xbox platforms */
class FTemporalAAMaskSetupPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FTemporalAAMaskSetupPixelShader,Global)

public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform != SP_XBOXD3D;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FTemporalAAMaskSetupPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader(Initializer)
	{
		TemporalAAParameters.Bind(Initializer.ParameterMap,TEXT("TemporalAAStartDepth"),TRUE);
		SceneTextures.Bind(Initializer.ParameterMap);
	}

	FTemporalAAMaskSetupPixelShader()
	{
	}

	void SetParameters(const FViewInfo& View)
	{
		SetPixelShaderValue(
			GetPixelShader(),
			TemporalAAParameters,
			View.TemporalAAParameters.StartDepth
			);

		SceneTextures.Set(&View,this,SF_Point);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{		
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << TemporalAAParameters;
		Ar << SceneTextures;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter TemporalAAParameters;
	FSceneTextureShaderParameters SceneTextures;
};

IMPLEMENT_SHADER_TYPE(,FTemporalAAMaskSetupPixelShader,TEXT("TemporalAA"),TEXT("MaskSetupPixelShader"),SF_Pixel,0,0);

/** Expands the temporal AA mask along depth discontinuities to avoid jittering. */
class FTemporalAAMaskExpandPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FTemporalAAMaskExpandPixelShader,Global)

public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FTemporalAAMaskExpandPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		TexelSizesParameter.Bind(Initializer.ParameterMap,TEXT("TexelSizes"),TRUE);
		TemporalAAParameters.Bind(Initializer.ParameterMap,TEXT("TemporalAAStartDepth"),TRUE);
		SceneTextures.Bind(Initializer.ParameterMap);
	}

	FTemporalAAMaskExpandPixelShader()
	{
	}

	void SetParameters(const FViewInfo& View)
	{
		SetPixelShaderValue(
			GetPixelShader(),
			TexelSizesParameter,
			FVector2D(1.0f / GSceneRenderTargets.GetBufferSizeX(), 1.0f / GSceneRenderTargets.GetBufferSizeY()));

		// Set the temporal AA parameters.
		SetPixelShaderValue(
			GetPixelShader(),
			TemporalAAParameters,
			View.TemporalAAParameters.StartDepth
			);

		SceneTextures.Set(&View,this,SF_Point);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{		
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << TexelSizesParameter;
		Ar << TemporalAAParameters;
		Ar << SceneTextures;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter TexelSizesParameter;
	FShaderParameter TemporalAAParameters;
	FSceneTextureShaderParameters SceneTextures;
};

IMPLEMENT_SHADER_TYPE(,FTemporalAAMaskExpandPixelShader,TEXT("TemporalAA"),TEXT("MaskExpandPixelShader"),SF_Pixel,0,0);

/** Blends the reprojected previous frame's scene color with this frame. */
class FTemporalAAPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FTemporalAAPixelShader,Global)

public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FTemporalAAPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader(Initializer)
	{
		CurrentFrameSceneColorParameter.Bind(Initializer.ParameterMap,TEXT("CurrentFrameSceneColor"),TRUE);
		PreviousFrameSceneColorParameter.Bind(Initializer.ParameterMap,TEXT("PreviousFrameSceneColor"),TRUE);
		CurrentFrameToPreviousFrameTransformParameter.Bind(Initializer.ParameterMap,TEXT("CurrentFrameToPreviousFrameTransform"),TRUE);
		TemporalAAParameters.Bind(Initializer.ParameterMap,TEXT("TemporalAAStartDepth"),TRUE);
		SceneTextures.Bind(Initializer.ParameterMap);
	}

	FTemporalAAPixelShader()
	{
	}

	void SetParameters(const FViewInfo& View)
	{
		SetTextureParameter(
			GetPixelShader(),
			CurrentFrameSceneColorParameter,
			TStaticSamplerState<>::GetRHI(),
			GSceneRenderTargets.GetRenderTargetTexture(CurrentFrameSceneColor)
			);

		SetTextureParameter(
			GetPixelShader(),
			PreviousFrameSceneColorParameter,
			TStaticSamplerState<>::GetRHI(),
			GSceneRenderTargets.GetRenderTargetTexture(PreviousFrameSceneColor)
			);

		FMatrix ScreenToTranslatedWorld = FMatrix(
				FPlane(1,0,0,0),
				FPlane(0,1,0,0),
				FPlane(0,0,(1.0f - Z_PRECISION),1),
				FPlane(0,0,-View.NearClippingDistance * (1.0f - Z_PRECISION),0) ) * View.InvTranslatedViewProjectionMatrix;

		ScreenToTranslatedWorld.M[0][3] = 0.f; // Note that we reset the column here because in the shader we only used
		ScreenToTranslatedWorld.M[1][3] = 0.f; // the x, y, and z components of the matrix multiplication result and we
		ScreenToTranslatedWorld.M[2][3] = 0.f; // set the w component to 1 before multiplying by the CurrentFrameToPreviousFrameTransform.
		ScreenToTranslatedWorld.M[3][3] = 1.f;

		const FMatrix CurrentFrameToPreviousFrameTransform =
			ScreenToTranslatedWorld
			* FTranslationMatrix(View.PrevPreViewTranslation - View.PreViewTranslation)
			* View.PrevTranslatedViewProjectionMatrix;
		SetPixelShaderValue(GetPixelShader(),CurrentFrameToPreviousFrameTransformParameter,CurrentFrameToPreviousFrameTransform);

		// Set the temporal AA parameters.
		SetPixelShaderValue(
			GetPixelShader(),
			TemporalAAParameters,
			View.TemporalAAParameters.StartDepth
			);

		SceneTextures.Set(&View,this,SF_Point);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{		
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << CurrentFrameSceneColorParameter;
		Ar << PreviousFrameSceneColorParameter;
		Ar << CurrentFrameToPreviousFrameTransformParameter;
		Ar << TemporalAAParameters;
		Ar << SceneTextures;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter CurrentFrameSceneColorParameter;
	FShaderResourceParameter PreviousFrameSceneColorParameter;
	FShaderParameter CurrentFrameToPreviousFrameTransformParameter;
	FShaderParameter TemporalAAParameters;
	FSceneTextureShaderParameters SceneTextures;
};

IMPLEMENT_SHADER_TYPE(,FTemporalAAPixelShader,TEXT("TemporalAA"),TEXT("MainPixelShader"),SF_Pixel,0,0);

FGlobalBoundShaderState GTemporalAAMaskSetupBoundShaderState;
FGlobalBoundShaderState GTemporalAAMaskExpandBoundShaderState;
FGlobalBoundShaderState GTemporalAABoundShaderState;

/** Expands the temporal AA mask where needed to avoid visible jittering. */
void ExpandTemporalAAMask(const FViewInfo& View)
{
	// No depth or stencil tests, no backface culling.
	RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
	RHISetStencilState(TStaticStencilState<>::GetRHI());

	// Write into the alpha channel only, expanding the temporal AA mask along depth discontinuities to avoid jittering
	RHISetBlendState(TStaticBlendState<BO_Add,BF_Zero,BF_One,BO_Add,BF_InverseDestAlpha,BF_One>::GetRHI());

	// Set the temporal AA shaders.
	TShaderMapRef<FTemporalAAVertexShader> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FTemporalAAMaskExpandPixelShader> PixelShader(GetGlobalShaderMap());

	SetGlobalBoundShaderState(
		GTemporalAAMaskExpandBoundShaderState,
		GFilterVertexDeclaration.VertexDeclarationRHI,
		*VertexShader,
		*PixelShader,
		sizeof(FFilterVertex)
		);
	VertexShader->SetParameters(View);
	PixelShader->SetParameters(View);

	// Draw a quad over the viewport.
	DrawDenormalizedQuad( 
		0, 0, 
		View.RenderTargetSizeX, View.RenderTargetSizeY,
		View.RenderTargetX, View.RenderTargetY, 
		View.RenderTargetSizeX, View.RenderTargetSizeY,
		View.RenderTargetSizeX, View.RenderTargetSizeY,
		GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());
}

/**
 * Prepares texture pool memory to fit temporal AA allocation, if needed.
 * Must be called on the gamethread.
 */
void FSceneRenderer::PrepareTemporalAAAllocation()
{
	// Check if any view has temporal AA enabled.
	UBOOL bAnyViewHasTemporalAA = FALSE;
	for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views(ViewIndex);
		if (View.bRenderTemporalAA)
		{
			bAnyViewHasTemporalAA = TRUE;
		}
	}

	const UBOOL bNeedsAllocation = bAnyViewHasTemporalAA && GSystemSettings.bAllowTemporalAA;
	if( bNeedsAllocation  )
	{
		// If necessary, try to free up some memory to fit the Temporal AA buffer.
		GSceneRenderTargets.PrepareTemporalAAAllocation();
	}
}

/** Renders temporal anti-aliasing. */
void FSceneRenderer::RenderTemporalAA()
{
	if (!bIsSceneCapture)
	{
		// Check if any view has temporal AA enabled.
		UBOOL bAnyViewHasTemporalAA = FALSE;
		for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views(ViewIndex);
			if (View.bRenderTemporalAA)
			{
				bAnyViewHasTemporalAA = TRUE;
			}
		}

		const UBOOL bNeedsAllocation = bAnyViewHasTemporalAA && GSystemSettings.bAllowTemporalAA;

		// Allocate/free as needed.
		GSceneRenderTargets.UpdateTemporalAAAllocation(bNeedsAllocation);

		// Is it currently allocated?
		const UBOOL bIsAllocated = IsValidRef(GSceneRenderTargets.GetRenderTargetTexture(PreviousFrameSceneColor));

		// Check whether temporal AA is enabled in this configuration, and that the memory has been allocated.
		if ( bAnyViewHasTemporalAA && bIsAllocated )
		{
			SCOPED_DRAW_EVENT(EventRenderTAA)(DEC_SCENE_ITEMS,TEXT("Temporal AA"));

	#if XBOX

			// Bind the scene color buffer and render into it's alpha channel, expanding the temporal AA mask where necessary
			// On Xbox the base pass output the temporal AA mask to scene color alpha based on depth and whether the object is movable
			RHISetRenderTarget(GSceneRenderTargets.GetSceneColorSurface(), FSurfaceRHIRef());

			for (INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				const FViewInfo& View = Views(ViewIndex);

				if (View.bRenderTemporalAA)
				{
					// Set the device viewport for the view.
					RHISetViewport(View.RenderTargetX, View.RenderTargetY, 0.0f, View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY, 1.0f);
					RHISetViewParameters(View);
					RHISetMobileHeightFogParams(View.HeightFogParams);

					ExpandTemporalAAMask(View);
				}
			}

			// Copy the current frame's scene color and mask to CurrentFrameSceneColor, which will be passed to next frame's RenderTemporalAA
			RHICopyToResolveTarget(GSceneRenderTargets.GetSceneColorRawSurface(),FALSE,FResolveParams(FResolveRect(),CubeFace_PosX,GSceneRenderTargets.GetRenderTargetTexture(CurrentFrameSceneColor)));
	#else
			RHICopyToResolveTarget(GSceneRenderTargets.GetSceneColorSurface(),FALSE,FResolveParams(FResolveRect(),CubeFace_PosX,GSceneRenderTargets.GetRenderTargetTexture(CurrentFrameSceneColor)));

			{
				RHISetRenderTarget(GSceneRenderTargets.GetRenderTargetSurface(CurrentFrameSceneColor), FSurfaceRHIRef());

				SCOPED_DRAW_EVENT(EventDynamic)(DEC_SCENE_ITEMS,TEXT("TemporalAAMask"));

				for (INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
				{
					const FViewInfo& View = Views(ViewIndex);

					if (View.bRenderTemporalAA)
					{
						// Set the device viewport for the view.
						RHISetViewport(View.RenderTargetX, View.RenderTargetY, 0.0f, View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY, 1.0f);
						RHISetViewParameters(View);
						RHISetMobileHeightFogParams(View.HeightFogParams);

						{
							// No depth or stencil tests, no backface culling.
							RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
							RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
							RHISetStencilState(TStaticStencilState<>::GetRHI());
							// Only writing into the alpha channel, which stores the mask
							RHISetBlendState(TStaticBlendState<BO_Add,BF_Zero,BF_One,BO_Add,BF_One,BF_Zero>::GetRHI());

							// Set the temporal AA mask setup shaders.
							TShaderMapRef<FTemporalAAVertexShader> VertexShader(GetGlobalShaderMap());
							TShaderMapRef<FTemporalAAMaskSetupPixelShader> PixelShader(GetGlobalShaderMap());

							SetGlobalBoundShaderState(
								GTemporalAAMaskSetupBoundShaderState,
								GFilterVertexDeclaration.VertexDeclarationRHI,
								*VertexShader,
								*PixelShader,
								sizeof(FFilterVertex)
								);
							VertexShader->SetParameters(View);
							PixelShader->SetParameters(View);

							// Setup a mask in the saved scene color alpha based on depth
							DrawDenormalizedQuad( 
								0, 0, 
								View.RenderTargetSizeX, View.RenderTargetSizeY,
								View.RenderTargetX, View.RenderTargetY, 
								View.RenderTargetSizeX, View.RenderTargetSizeY,
								View.RenderTargetSizeX, View.RenderTargetSizeY,
								GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());
						}
						
						// Render movable objects, writing 0 into the alpha channel mask
						TDynamicPrimitiveDrawer<FTemporalAAMaskDrawingPolicyFactory> Drawer(&View,SDPG_World,FTemporalAAMaskDrawingPolicyFactory::ContextType(),TRUE);
						{
							for (INT PrimitiveIndex = 0; PrimitiveIndex < View.VisibleDynamicPrimitives.Num(); PrimitiveIndex++)
							{
								const FPrimitiveSceneInfo* PrimitiveSceneInfo = View.VisibleDynamicPrimitives(PrimitiveIndex);
								const FPrimitiveViewRelevance& PrimitiveViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);
								const UBOOL bVisible = View.PrimitiveVisibilityMap(PrimitiveSceneInfo->Id);
			
								if (bVisible 
									&& PrimitiveSceneInfo->Proxy->IsMovable() 
									&& PrimitiveViewRelevance.bOpaqueRelevance 
									&& PrimitiveViewRelevance.GetDPG(SDPG_World))
								{
									Drawer.SetPrimitive(PrimitiveSceneInfo);
									PrimitiveSceneInfo->Proxy->DrawDynamicElements(
										&Drawer,
										&View,
										SDPG_World
										);
								}
							}
						}

						// Expand the mask along silhouettes
						ExpandTemporalAAMask(View);
					}
				}

				RHICopyToResolveTarget(GSceneRenderTargets.GetRenderTargetSurface(CurrentFrameSceneColor),FALSE,FResolveParams());
			}
#endif

			RHISetRenderTarget(GSceneRenderTargets.GetSceneColorSurface(), FSurfaceRHIRef());

			// Don't apply temporal AA if PreviousFrameSceneColor doesn't contain the back buffer from the previous frame.
			// This happens on the first frame, when toggling temporal AA, etc.
			if (GSceneRenderTargets.IsPreviousFrameSceneColorValid(FrameNumber))
			{
				for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					const FViewInfo& View = Views(ViewIndex);

					// Don't draw temporal AA if this frame is unrelated to the previous frame, or if temporal AA is disabled for this view by the rendering overrides.
					if (!View.bPrevTransformsReset && View.bRenderTemporalAA)
					{
						// Set the device viewport for the view.
						RHISetViewport(View.RenderTargetX, View.RenderTargetY, 0.0f, View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY, 1.0f);
						RHISetViewParameters(View);
						RHISetMobileHeightFogParams(View.HeightFogParams);

						// No depth or stencil tests, no backface culling.
						RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
						RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
						RHISetStencilState(TStaticStencilState<>::GetRHI());

						RHISetBlendState(TStaticBlendState<BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_Zero,BF_One>::GetRHI());

						// Set the temporal AA shaders.
						TShaderMapRef<FTemporalAAVertexShader> VertexShader(GetGlobalShaderMap());
						TShaderMapRef<FTemporalAAPixelShader> PixelShader(GetGlobalShaderMap());

						SetGlobalBoundShaderState(
							GTemporalAABoundShaderState,
							GFilterVertexDeclaration.VertexDeclarationRHI,
							*VertexShader,
							*PixelShader,
							sizeof(FFilterVertex)
							);
						VertexShader->SetParameters(View);
						PixelShader->SetParameters(View);

						// Draw a quad over the viewport.
						DrawDenormalizedQuad( 
							0, 0, 
							View.RenderTargetSizeX, View.RenderTargetSizeY,
							View.RenderTargetX, View.RenderTargetY, 
							View.RenderTargetSizeX, View.RenderTargetSizeY,
							View.RenderTargetSizeX, View.RenderTargetSizeY,
							GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());
					}
				}
			}

			// Reinterpret the saved copy of the current frame's back buffer as the next frame's previous frame's back buffer.
			GSceneRenderTargets.SwapCurrentFrameAndPreviousFrameSavedBackBuffers(FrameNumber);
		}
	}
}
