/*=============================================================================
	LightFunctionRendering.cpp: Implementation for rendering light functions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"


/**
 * A vertex shader for projecting a light function onto the scene.
 */
class FLightFunctionVertexShader : public FShader
{
	DECLARE_SHADER_TYPE(FLightFunctionVertexShader,Material);
public:

	/**
	  * Makes sure only shaders for materials that are explicitly flagged
	  * as 'UsedAsLightFunction' in the Material Editor gets compiled into
	  * the shader cache.
	  */
	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return Material->IsLightFunction();
	}

	FLightFunctionVertexShader( )	{ }
	FLightFunctionVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
	}

	void SetParameters( const FSceneView* View )
	{
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		return FShader::Serialize(Ar);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FLightFunctionVertexShader,TEXT("LightFunctionVertexShader"),TEXT("Main"),SF_Vertex,0,0);

/**
 * A pixel shader for projecting a light function onto the scene.
 */
class FLightFunctionPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FLightFunctionPixelShader,Material);
public:

	/**
	  * Makes sure only shaders for materials that are explicitly flagged
	  * as 'UsedAsLightFunction' in the Material Editor gets compiled into
	  * the shader cache.
	  */
	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return Material->IsLightFunction();
	}

	FLightFunctionPixelShader() {}
	FLightFunctionPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		SceneColorTextureParameter.Bind(Initializer.ParameterMap,TEXT("SceneColorTexture"),TRUE);
		SceneDepthTextureParameter.Bind(Initializer.ParameterMap,TEXT("SceneDepthTexture"),TRUE);
		SceneDepthCalcParameter.Bind(Initializer.ParameterMap,TEXT("MinZ_MaxZRatio"),TRUE);
		ScreenPositionScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("ScreenPositionScaleBias"),TRUE);
		ScreenToLightParameter.Bind(Initializer.ParameterMap,TEXT("ScreenToLight"),TRUE);
		LightFunctionParameters.Bind(Initializer.ParameterMap,TEXT("LightFunctionParameters"),TRUE);
		MaterialParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters( const FSceneView* View, const FLightSceneInfo* LightSceneInfo, FLOAT ShadowFadeFraction )
	{
		ESceneDepthUsage DepthUsage = SceneDepthUsage_Normal;

		// Set the texture containing the scene depth.
		if(SceneColorTextureParameter.IsBound())
		{
			SetTextureParameter(
				GetPixelShader(),
				SceneColorTextureParameter,
				TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				GSceneRenderTargets.GetSceneColorTexture()
				);
		}
		else if(SceneDepthTextureParameter.IsBound())
		{
#if PS3 || WIIU
			UBOOL bSetSceneDepthTexture = IsValidRef(GSceneRenderTargets.GetSceneDepthTexture());
			DepthUsage = SceneDepthUsage_ProjectedShadows;
#else
			UBOOL bSetSceneDepthTexture = GSupportsDepthTextures && IsValidRef(GSceneRenderTargets.GetSceneDepthTexture());
#endif

			if (bSetSceneDepthTexture)
			{
				SetTextureParameter(
					GetPixelShader(),
					SceneDepthTextureParameter,
					TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
					GSceneRenderTargets.GetSceneDepthTexture()
					);
			}
		}

		// Set the transform from screen space to light space.
		if ( ScreenToLightParameter.IsBound() )
		{
			FVector InverseScale = FVector( 1.f / LightSceneInfo->LightFunctionScale.X, 1.f / LightSceneInfo->LightFunctionScale.Y, 1.f / LightSceneInfo->LightFunctionScale.Z );
			FMatrix WorldToLight = LightSceneInfo->WorldToLight * FScaleMatrix(FVector(InverseScale));	
			FMatrix ScreenToLight = 
				FMatrix(
				FPlane(1,0,0,0),
				FPlane(0,1,0,0),
				FPlane(0,0,(1.0f - Z_PRECISION),1),
				FPlane(0,0,-View->NearClippingDistance * (1.0f - Z_PRECISION),0)
				) * View->InvViewProjectionMatrix * WorldToLight;

			SetPixelShaderValue( GetPixelShader(), ScreenToLightParameter, ScreenToLight );
		}

		// Output greyscale for dominant lights, since different lights will be packed into different channels if GOnePassDominantLight is enabled
		SetPixelShaderValue( 
			GetPixelShader(), 
			LightFunctionParameters, 
			FVector2D(IsDominantLightType(LightSceneInfo->LightType) ? 1.0f : 0.0f, ShadowFadeFraction));

		// Set additional material parameters used by the emissive shader expression.
		// Set the flag to work around mip mapping artifacts when doing a deferred pass with textures with mips
		// This avoids a one pixel artifact at depth discontinuities, where the lowest mip is being used
		// Note: the one pixel line is currently only fixed on Xbox 360 and PS3, due to RHI limitations
		FMaterialRenderContext MaterialRenderContext(LightSceneInfo->LightFunction, *LightSceneInfo->LightFunction->GetMaterial(), View->Family->CurrentWorldTime, View->Family->CurrentRealTime, View, TRUE, TRUE);
		MaterialParameters.Set(this, MaterialRenderContext, DepthUsage);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SceneColorTextureParameter;
		Ar << SceneDepthTextureParameter;
		Ar << SceneDepthCalcParameter;
		Ar << ScreenPositionScaleBiasParameter;
		Ar << ScreenToLightParameter;
		Ar << LightFunctionParameters;
		Ar << MaterialParameters;
		return bShaderHasOutdatedParameters;
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FShaderResourceParameter SceneColorTextureParameter;
	FShaderResourceParameter SceneDepthTextureParameter;
	FShaderParameter SceneDepthCalcParameter;
	FShaderParameter ScreenPositionScaleBiasParameter;
	FShaderParameter ScreenToLightParameter;
	FShaderParameter LightFunctionParameters;
	FMaterialPixelShaderParameters MaterialParameters;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FLightFunctionPixelShader,TEXT("LightFunctionPixelShader"),TEXT("Main"),SF_Pixel,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);

/** The light function vertex declaration resource type. */
class FLightFunctionVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/** Destructor. */
	virtual ~FLightFunctionVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		Elements.AddItem(FVertexElement(0,0,VET_Float3,VEU_Position,0));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** Vertex declaration for the light function fullscreen 2D quad. */
TGlobalResource<FLightFunctionVertexDeclaration> GLightFunctionVertexDeclaration;

/** Returns a fade fraction for a light function and a given view based on the appropriate fade settings. */
static FLOAT GetLightFunctionFadeFraction(const FViewInfo& View, const FLightSceneInfo* LightSceneInfo)
{
	const FSphere LightBounds = LightSceneInfo->GetBoundingSphere();
	// Override the global settings with the light's settings if the light has them specified
	const UINT MinShadowResolution = (LightSceneInfo->MinShadowResolution > 0) ? LightSceneInfo->MinShadowResolution : GSystemSettings.MinShadowResolution;
	const INT ShadowFadeResolution = (LightSceneInfo->ShadowFadeResolution > 0) ? LightSceneInfo->ShadowFadeResolution : GSystemSettings.ShadowFadeResolution;

	// Project the bounds onto the view
	const FVector4 ScreenPosition = View.WorldToScreen(LightBounds.Center);
	const FLOAT ScreenRadius = Max(
		View.SizeX / 2.0f * View.ProjectionMatrix.M[0][0],
		View.SizeY / 2.0f * View.ProjectionMatrix.M[1][1]) *
		LightBounds.W /
		Max(ScreenPosition.W, 1.0f);

	const UINT UnclampedResolution = appTrunc(ScreenRadius * GSystemSettings.ShadowTexelsPerPixel);
	extern FLOAT CalculateShadowFadeAlpha(INT MaxUnclampedResolution, INT ShadowFadeResolution, INT MinShadowResolution);
	const FLOAT ResolutionFadeAlpha = CalculateShadowFadeAlpha(UnclampedResolution, ShadowFadeResolution, MinShadowResolution);
	return ResolutionFadeAlpha;
}

/**
* Used by RenderLights to figure out if light functions need to be rendered to the attenuation buffer.
*
* @param LightSceneInfo Represents the current light
* @return TRUE if anything got rendered
*/
UBOOL FSceneRenderer::CheckForLightFunction( const FLightSceneInfo* LightSceneInfo, UINT DPGIndex ) const
{
	// NOTE: The extra check is necessary because there could be something wrong with the material.
	if( LightSceneInfo->LightFunction && 
		LightSceneInfo->LightFunction->GetMaterial()->IsLightFunction() )
	{
		for (INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views(ViewIndex);
			if(View.VisibleLightInfos(LightSceneInfo->Id).DPGInfo[DPGIndex].bHasVisibleLitPrimitives
				// Only draw the light function if it hasn't completely faded out
				&& GetLightFunctionFadeFraction(View, LightSceneInfo) > 1.0f / 256.0f)
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

/** Returns TRUE if a light function needs to be rendered for the given view. */
UBOOL FSceneRenderer::CheckForLightFunction( const FViewInfo& View, const FLightSceneInfo* LightSceneInfo, UINT DPGIndex, FLOAT& ClosestDistanceFromViews ) const
{
	ClosestDistanceFromViews = FLT_MAX;
	UBOOL bVisible = FALSE;
	// NOTE: The extra check is necessary because there could be something wrong with the material.
	if( (View.Family->ShowFlags & SHOW_LightFunctions) &&
		LightSceneInfo->LightFunction && 
		LightSceneInfo->LightFunction->GetMaterial()->IsLightFunction() )
	{
		if(View.VisibleLightInfos(LightSceneInfo->Id).DPGInfo[DPGIndex].bHasVisibleLitPrimitives
			// Only draw the light function if it hasn't completely faded out
			&& GetLightFunctionFadeFraction(View, LightSceneInfo) > 1.0f / 256.0f)
		{
			//@todo - use distance from cone for spotlights
			const FLOAT DistanceToView = Max(FVector(LightSceneInfo->GetPosition() - View.ViewOrigin).Size() - LightSceneInfo->GetRadius(), 0.0f);
			ClosestDistanceFromViews = Min(ClosestDistanceFromViews, DistanceToView);
			bVisible = TRUE;
		}
	}
	return bVisible;
}

/**
 * Used by RenderLights to render a light function to the attenuation buffer.
 *
 * @param LightSceneInfo Represents the current light
 */
UBOOL FSceneRenderer::RenderLightFunction(const FLightSceneInfo* LightSceneInfo, UINT DPGIndex, UBOOL bShadowsRendered)
{
	UBOOL bStencilDirty = FALSE;

	// NOTE: The extra check is necessary because there could be something wrong with the material.
	if ((ViewFamily.ShowFlags & SHOW_LightFunctions) &&
		LightSceneInfo->LightFunction && 
		LightSceneInfo->LightFunction->GetMaterial()->IsLightFunction())
	{
		SCOPED_DRAW_EVENT(EventLightFunction)(DEC_SCENE_ITEMS,TEXT("LightFunction"));

		const FMaterial* Material = LightSceneInfo->LightFunction->GetMaterial();
		{
			SCOPED_DRAW_EVENT(MaterialEvent)(DEC_SCENE_ITEMS, *Material->GetFriendlyName());
		}
		const FMaterialShaderMap* MaterialShaderMap = Material->GetShaderMap();
		FLightFunctionVertexShader* VertexShader = MaterialShaderMap->GetShader<FLightFunctionVertexShader>();
		FLightFunctionPixelShader* PixelShader = MaterialShaderMap->GetShader<FLightFunctionPixelShader>();

		//@todo - currently not handling switching a light function material on a light at runtime, since the bound shader state is cached
		if (!IsValidRef(LightSceneInfo->LightFunctionBoundShaderState))
		{
			DWORD Strides[MaxVertexElementCount];
			appMemzero(Strides, sizeof(Strides));
			Strides[0] = sizeof(FVector);

			LightSceneInfo->LightFunctionBoundShaderState = RHICreateBoundShaderState(GLightFunctionVertexDeclaration.VertexDeclarationRHI, Strides, VertexShader->GetVertexShader(), PixelShader->GetPixelShader(), EGST_None);
		}

		const FSphere LightBounds = LightSceneInfo->GetBoundingSphere();

		// Render to the light attenuation buffer for all views.
		for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENT(EventView, Views.Num() > 1)(DEC_SCENE_ITEMS,TEXT("View%d"), ViewIndex);
			
			const FViewInfo& View = Views(ViewIndex);
			if (View.VisibleLightInfos(LightSceneInfo->Id).DPGInfo[DPGIndex].bHasVisibleLitPrimitives)
			{
				const FLOAT FadeAlpha = GetLightFunctionFadeFraction(View, LightSceneInfo);
				// Don't draw the light function if it has completely faded out
				if (FadeAlpha > 1.0f / 256.0f)
				{
					// Set the device viewport for the view.
					RHISetViewport(View.RenderTargetX,View.RenderTargetY,0.0f,View.RenderTargetX + View.RenderTargetSizeX,View.RenderTargetY + View.RenderTargetSizeY,1.0f);
					RHISetViewParameters(View);
					RHISetMobileHeightFogParams(View.HeightFogParams);

					// Set the states to modulate the light function with the render target.
					RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
					
					extern UBOOL GUseHiStencil;
					// If we are rendering in the foreground DPG for a dominant light that is being rendered before the base pass,
					// We have to setup stencil to only modify pixels belonging to the foreground DPG, 
					// Since the light attenuation buffer overlaps with scene color and scene color has already been generated for the world DPG.
					if (GOnePassDominantLight
						&& DPGIndex == SDPG_Foreground
						&& IsDominantLightType(LightSceneInfo->LightType))
					{
						bStencilDirty = TRUE;
						// Set stencil to one.
						RHISetStencilState(TStaticStencilState<
							TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,
							FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
							0xff,0xff,1
							>::GetRHI());
						if (GUseHiStencil)
						{
							RHIBeginHiStencilRecord(TRUE, 0);
						}
						RHISetColorWriteEnable(FALSE);
						RHISetBlendState(TStaticBlendState<>::GetRHI());
						RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_CW>::GetRHI());

						TDynamicPrimitiveDrawer<FDepthDrawingPolicyFactory> Drawer(&View,SDPG_Foreground,FDepthDrawingPolicyFactory::ContextType(DDM_AllOccluders),TRUE);
						// Setup a stencil mask for foreground DPG pixels which receive this light function
						//@todo - only render the foreground primitives affected by this light
						for(INT PrimitiveIndex = 0;PrimitiveIndex < View.VisibleDynamicPrimitives.Num();PrimitiveIndex++)
						{
							const FPrimitiveSceneInfo* PrimitiveSceneInfo = View.VisibleDynamicPrimitives(PrimitiveIndex);
							const FPrimitiveViewRelevance& PrimitiveViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);
							if (View.PrimitiveVisibilityMap(PrimitiveSceneInfo->Id)
								&& PrimitiveViewRelevance.GetDPG(DPGIndex)
								&& PrimitiveViewRelevance.bOpaqueRelevance)
							{
								Drawer.SetPrimitive(PrimitiveSceneInfo);
								PrimitiveSceneInfo->Proxy->DrawDynamicElements(&Drawer,&View,SDPG_Foreground);
							}
						}

						if (GUseHiStencil)
						{
							RHIBeginHiStencilPlayback(TRUE);
						}

						// Test stencil for != 0.
						RHISetStencilState(TStaticStencilState<
							TRUE,CF_NotEqual,SO_Keep,SO_Keep,SO_Keep,
							FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
							0xff,0xff,0
							>::GetRHI());

						if (bShadowsRendered)
						{
							// Use modulated blending to RGB since light functions are combined in the same buffer as normal shadows, preserve Alpha
							RHISetBlendState(TStaticBlendState<BO_Add,BF_DestColor,BF_Zero,BO_Add,BF_Zero,BF_One>::GetRHI());
						}
						else
						{
							// We are the first one to render to the light attenuation buffer, 
							// It overlaps with scene color and was not cleared so we need to overwrite the previous values.
							RHISetBlendState(TStaticBlendState<>::GetRHI());
						}
					}
					else
					{
						// Use modulated blending to RGB since light functions are combined in the same buffer as normal shadows, preserve Alpha
						RHISetBlendState(TStaticBlendState<BO_Add,BF_DestColor,BF_Zero,BO_Add,BF_Zero,BF_One>::GetRHI());
					}
					
					if (GOnePassDominantLight && IsDominantLightType(LightSceneInfo->LightType))
					{
						const INT ChannelIndex = View.DominantLightChannelAllocator.GetLightChannel(LightSceneInfo->Id);
						// Only write to the allowed light attenuation channel
						RHISetColorWriteMask(1 << ChannelIndex);
					}
					else
					{
						RHISetColorWriteEnable(TRUE); 
					}

					if (((FVector)View.ViewOrigin - LightBounds.Center).SizeSquared() < Square(LightBounds.W * 1.05f + View.NearClippingDistance * 2.0f))
					{
						// Render backfaces with depth tests disabled since the camera is inside (or close to inside) the light function geometry
						RHISetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid,CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid,CM_CCW>::GetRHI());
					}
					else
					{
						// Render frontfaces with depth tests on to get the speedup from HiZ since the camera is outside the light function geometry
						RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());
						RHISetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid,CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid,CM_CW>::GetRHI());
					}
		
					if (!bIsSceneCapture)
					{
						// Set the light's scissor rectangle.
						LightSceneInfo->SetScissorRect(&View);
					}

					// Render a fullscreen quad.
					VertexShader->SetParameters(&View);
					PixelShader->SetParameters(&View, LightSceneInfo, FadeAlpha);
					RHISetBoundShaderState(LightSceneInfo->LightFunctionBoundShaderState);

					// Project the light function using a sphere around the light
					//@todo - could use a cone for spotlights
					DrawStencilingSphere(LightBounds, View.PreViewTranslation);

					if (bStencilDirty && GUseHiStencil)
					{
						RHIEndHiStencil();
						RHISetStencilState(TStaticStencilState<>::GetRHI());
					}
				}
			}
		}

		// Restore states.
		RHISetDepthState(TStaticDepthState<>::GetRHI());
		RHISetColorWriteEnable(TRUE);
		RHISetBlendState(TStaticBlendState<>::GetRHI());
		RHISetStencilState(TStaticStencilState<>::GetRHI());
		RHISetScissorRect(FALSE,0,0,0,0);

		if (bStencilDirty)
		{
			// Clear the stencil buffer to 0.
			RHIClear(FALSE,FColor(0,0,0),FALSE,0,TRUE,0);
		}
	}
	return bStencilDirty;
}
