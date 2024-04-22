/*=============================================================================
	TranslucentRendering.cpp: Translucent rendering implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "ScreenRendering.h"

IMPLEMENT_SHADER_TYPE(,FLDRExtractVertexShader,TEXT("LDRExtractVertexShader"),TEXT("Main"),SF_Vertex,0,0);

/** Controls whether translucents resolve to the raw format or not */
UBOOL GRenderMinimalTranslucency = TRUE;

FLOAT GDownsampledTransluencyDistanceThreshold = 0.0f;

#if 0
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
		return (ESceneDepthPriorityGroup)Parameters.PrimitiveSceneInfo->Proxy->GetDepthPriorityGroup(&View);
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
		) const
	{
		const UBOOL bIsLitMaterial = Parameters.LightingModel != MLM_Unlit;
		const UBOOL bUsePreviewSkyLight = GIsEditor && LightMapPolicyType::bAllowPreviewSkyLight && Parameters.PrimitiveSceneInfo && Parameters.PrimitiveSceneInfo->bStaticShadowing;
		//Disable fog for objects that are in the foreground DPG (like the aiming players hair)
		//An alternate solution is to apply fog a 2nd time selectively in the foreground DPG, but at the cost of performance
		const UBOOL bAllowFog = Parameters.bAllowFog && (Parameters.Mesh.DepthPriorityGroup <= SDPG_World);

		TBasePassDrawingPolicy<LightMapPolicyType,FogDensityPolicyType> DrawingPolicy(
			Parameters.Mesh.VertexFactory,
			Parameters.Mesh.MaterialRenderProxy,
			*Parameters.Material,
			LightMapPolicy,
			Parameters.BlendMode,
			Parameters.PrimitiveSceneInfo && (Parameters.PrimitiveSceneInfo->HasDynamicSkyLighting() || bUsePreviewSkyLight) && bIsLitMaterial,
			(View.Family->ShowFlags & SHOW_ShaderComplexity) != 0,
			bDrawLitTranslucencyUnlit,
			bRenderingToLowResTranslucencyBuffer,
			bRenderingToDoFBlurBuffer,
			View.Family->bWriteOpacityToAlpha,
			bAllowFog
			);
		DrawingPolicy.DrawShared(
			&View,
			DrawingPolicy.CreateBoundShaderState(Parameters.Mesh.GetDynamicVertexStride())
			);
		for (INT BatchElementIndex=0;BatchElementIndex<Parameters.Mesh.Elements.Num();BatchElementIndex++)
		{
			DrawingPolicy.SetMeshRenderState(
				View,
				Parameters.PrimitiveSceneInfo,
				Parameters.Mesh,
				BatchElementIndex,
				bBackFace,
				typename TBasePassDrawingPolicy<LightMapPolicyType,FogDensityPolicyType>::ElementDataType(
				LightMapElementData,
				FogDensityElementData
				)
				);
			DrawingPolicy.DrawMesh(Parameters.Mesh, BatchElementIndex);
		}
	}
};
#endif

extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

FGlobalBoundShaderState CompositeLowResolutionTranslucencyBoundShaderState;

/** Composites the low resolution translucency buffer with the full resolution translucency in scene color. */
static void CompositeLowResolutionTranslucency( const FViewInfo& View )
{
	GSceneRenderTargets.BeginRenderingSceneColor();
	SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("CompositeLowResTranslucency"));

	// Set shaders and texture
	TShaderMapRef<FScreenVertexShader> ScreenVertexShader(GetGlobalShaderMap());
	TShaderMapRef<FScreenPixelShader> ScreenPixelShader(GetGlobalShaderMap());

	SetGlobalBoundShaderState( CompositeLowResolutionTranslucencyBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *ScreenPixelShader, sizeof(FFilterVertex) );
	
	FTexture TranslucencyBufferTexture;
	TranslucencyBufferTexture.TextureRHI = GSceneRenderTargets.GetTranslucencyBufferTexture();
	// Bilinear filtering for upsampling
	TranslucencyBufferTexture.SamplerStateRHI = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
	ScreenPixelShader->SetParameters(&TranslucencyBufferTexture);

	// RGB of the translucency buffer contains the color to add to scene color, alpha contains the factor to multiply with scene color
	// Don't modify scene color alpha, as that contains scene depth on some platforms
	RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_SourceAlpha,BO_Add,BF_Zero,BF_One>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
	RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());

	// Draw viewport covering quad
	RHISetViewport( View.RenderTargetX, View.RenderTargetY, 0.0f, View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY, 1.0f);
	RHISetViewParameters(View);
	RHISetMobileHeightFogParams(View.HeightFogParams);
	const UINT DownsampledSizeX = appTrunc(View.RenderTargetSizeX / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());
	const UINT DownsampledSizeY = appTrunc(View.RenderTargetSizeY / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());
	const UINT TranslucencyBufferSizeX = GSceneRenderTargets.GetTranslucencyBufferSizeX();
	const UINT TranslucencyBufferSizeY = GSceneRenderTargets.GetTranslucencyBufferSizeY();
	DrawDenormalizedQuad( 
		0, 0, 
		View.RenderTargetSizeX, View.RenderTargetSizeY,
		View.RenderTargetX / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor(), View.RenderTargetY / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor(), 
		DownsampledSizeX, DownsampledSizeY,
		View.RenderTargetSizeX, View.RenderTargetSizeY,
		TranslucencyBufferSizeX, TranslucencyBufferSizeY );
}

/** 
 * If not already rendering full resolution translucency, this function resolves the low resolution translucency buffer,
 * Composites it with scene color, and sets up render state to render full resolution translucency.
 */
/*static*/ void TransitionToFullResolutionTranslucency(const FViewInfo& View, UBOOL& bRenderingToLowResTranslucencyBuffer)
{
	if( bRenderingToLowResTranslucencyBuffer )
	{
		bRenderingToLowResTranslucencyBuffer = FALSE;
		const UINT DownsampledX = appTrunc(View.RenderTargetX / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());
		const UINT DownsampledY = appTrunc(View.RenderTargetY / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());
		const UINT DownsampledSizeX = appTrunc(View.RenderTargetSizeX / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());
		const UINT DownsampledSizeY = appTrunc(View.RenderTargetSizeY / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());
		GSceneRenderTargets.FinishRenderingTranslucency(
			FResolveParams(FResolveRect(
				DownsampledX,
				DownsampledY, 
				DownsampledX + DownsampledSizeX,
				DownsampledY + DownsampledSizeY
			)),
			TRUE);

		// Composite the translucency buffer back into the main scene
		CompositeLowResolutionTranslucency(View);

		// Restore states changed by the apply
		GSceneRenderTargets.BeginRenderingTranslucency(View, FALSE, TRUE);
	}
}

UBOOL DWNewMerge = TRUE;

#if 0
/**
 * Render a dynamic mesh using a translucent draw policy
 * @return TRUE if the mesh rendered
 */
UBOOL FTranslucencyDrawingPolicyFactory::DrawDynamicMesh(
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId
	)
{
	UBOOL bDirty = FALSE;

	// Determine the mesh's material and blend mode.
	const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial();
	const EBlendMode BlendMode = Material->GetBlendMode();
	const EMaterialLightingModel LightingModel = Material->GetLightingModel();

	// Only render translucent materials.
	if(IsTranslucentBlendMode(BlendMode) || BlendMode == BLEND_SoftMasked)
	{
		// Handle the fog volume case.
		if (Material->IsUsedWithFogVolumes())
		{
			// Fog volumes use a blend mode that can't be handled with the downsampled translucency buffer
			TransitionToFullResolutionTranslucency(View, DrawingContext.bRenderingToLowResTranslucencyBuffer);
			bDirty = RenderFogVolume( &View, Mesh, bBackFace, bPreFog, PrimitiveSceneInfo, HitProxyId );
		}
		else
		{
#if PS3
			//@todo - support for PS3
			if (FALSE)
#else
			if( GSystemSettings.bAllowDownsampledTranslucency && !View.Family->bWriteOpacityToAlpha )
#endif
			{
				// BLEND_Modulate can't be supported with the current translucency buffer format, 
				// Which only has one float for multiplying with scene color during the composite, but BLEND_Modulate needs three.
				if( DrawingContext.bAllowDownsampling
					&& BlendMode != BLEND_Modulate
					&& BlendMode != BLEND_ModulateAndAdd
					&& IsValidRef(GSceneRenderTargets.GetTranslucencyBufferSurface()))
				{
					// Downsampled decals are currently not supported.
					checkSlow(!Mesh.IsDecal());
					// Downsampled FTileRenderer rendering is currently not supported, 
					// Which is the only use of FTranslucencyDrawingPolicyFactory that passes in a NULL PrimitiveSceneInfo.
					checkSlow(PrimitiveSceneInfo);
					// One layer distortion can't be downsampled since blending is done manually in the shader.
					checkSlow(!View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id).bOneLayerDistortionRelevance);

					// Switch to low resolution translucent rendering if this is the first
					if( !DrawingContext.bRenderingToLowResTranslucencyBuffer )
					{
						GSceneRenderTargets.BeginRenderingTranslucency(View, TRUE, TRUE);
						DrawingContext.bRenderingToLowResTranslucencyBuffer = TRUE;
					}
				}
				else
				{
					// Switch to full resolution rendering at the first need
					TransitionToFullResolutionTranslucency(View, DrawingContext.bRenderingToLowResTranslucencyBuffer);
				}
			}


			const UBOOL bDisableDepthTesting = Material->NeedsDepthTestDisabled();
			// Allow the material to turn off depth testing if desired
			if (bDisableDepthTesting)
			{
				// No depth tests or depth writes
				RHISetDepthState( TStaticDepthState<FALSE,CF_Always>::GetRHI() );
			}
			const UBOOL bOneLayerDistortion = Material->UsesOneLayerDistortion();
			// Disable alpha writes with one layer distortion as the material will be using an opaque blend mode
			if (bOneLayerDistortion)
			{
				RHISetColorWriteMask(CW_RGB);
			}

			const UBOOL bRenderLit = LightingModel != MLM_Unlit && (View.Family->ShowFlags & SHOW_Lighting) != 0;
			// The light that will be applied as a directional light in the base pass
			const FLightSceneInfo* MergedDynamicLight = NULL;
			// Composition of all other lights influences to be applied in the base pass
			FSHVectorRGB CompositedDynamicLighting;
			// Use approximate one pass lighting if the primitive requires it
			// One pass lighting is useful for translucency since multi pass lighting does not blend in the correct order
			const UBOOL bUseCompositedTranslucentLighting = PrimitiveSceneInfo && PrimitiveSceneInfo->bUseOnePassLightingOnTranslucency && bRenderLit;
			if (bUseCompositedTranslucentLighting)
			{
				check(!DrawingContext.TranslucentPreShadowInfo || DrawingContext.TranslucentPreShadowInfo->LightSceneInfo == PrimitiveSceneInfo->BrightestDominantLightSceneInfo);
				if (PrimitiveSceneInfo->BrightestDominantLightSceneInfo)
				{
					// Always use the affecting dominant light if there is one
					MergedDynamicLight = PrimitiveSceneInfo->BrightestDominantLightSceneInfo;
				}
				else
				{
					// Find the brightest primary light to be applied as a directional light
					for (const FLightPrimitiveInteraction* Interaction = PrimitiveSceneInfo->LightList; Interaction; Interaction = Interaction->GetNextLight())
					{
						const FLightSceneInfo* Light = Interaction->GetLight();

						if (!Light->LightEnvironment
							&& Light->Color.GetMax() > 0.0f 
							// Don't use a sky or SH light as the merged light, since the merged light will be applied as a directional light
							&& Light->LightType != LightType_Sky
							&& Light->LightType != LightType_SphericalHarmonic
							// Use the primary shadow casting light in cinematic setups
							// Rim lights will have bSelfShadowOnly = TRUE
							//@todo - what to do when both lights have bSelfShadowOnly = TRUE as an optimization?
							&& !Light->bSelfShadowOnly)
						{
							MergedDynamicLight = Light;
						}
					}
				}

				for (const FLightPrimitiveInteraction* Interaction = PrimitiveSceneInfo->LightList; Interaction; Interaction = Interaction->GetNextLight())
				{
					const FLightSceneInfo* Light = Interaction->GetLight();

					// Skip lights that have compositing disabled (muzzle flashes)
					if (Light != MergedDynamicLight && Light->bAllowCompositingIntoDLE)
					{
						// Composite all other lights into an SH environment as an approximation of their influence
						Light->CompositeInfluence(PrimitiveSceneInfo->Bounds.Origin, CompositedDynamicLighting);
					}
				}
			}

			ProcessBasePassMesh(
				FProcessBasePassMeshParameters(
					Mesh,
					Material,
					PrimitiveSceneInfo, 
					!bPreFog
					),
				FDrawTranslucentMeshAction(
					View,
					DrawingContext.TranslucentPreShadowInfo,
					MergedDynamicLight,
					CompositedDynamicLighting,
					bUseCompositedTranslucentLighting,
					bBackFace,
					HitProxyId,
					!bRenderLit,
					DrawingContext.bRenderingToLowResTranslucencyBuffer,
					DrawingContext.bRenderingToDoFBlurBuffer
					)
				);
			bDirty = TRUE;

			// Render multi pass lighting if the primitive does not use approximate one pass lighting
			if (PrimitiveSceneInfo && !PrimitiveSceneInfo->bUseOnePassLightingOnTranslucency && bRenderLit)
			{
				if ((View.Family->ShowFlags & SHOW_ShaderComplexity) != 0)
				{
					// Accumulate complexity
					RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());
				}
				else
				{
					// Render lights
					switch(BlendMode)
					{
					case BLEND_AlphaComposite:
					case BLEND_Translucent:
						// Base bass blended the framebuffer with 1-Alpha, so for extra passes we don't modulate the framebuffer, just source.
						RHISetBlendState(TStaticBlendState<BO_Add,BF_SourceAlpha,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());
						break;
					default:
						// Add to the existing scene color, preserve destination alpha.
						RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());
						break;
					}
				}

				for( const FLightPrimitiveInteraction* Interaction = PrimitiveSceneInfo->LightList; Interaction; Interaction = Interaction->GetNextLight() )
				{
					if( Interaction->NeedsLightRenderingPass() )
					{
						// Disable writing to the blur render target before we render any lights
						if( DrawingContext.bRenderingToDoFBlurBuffer )
						{
							GSceneRenderTargets.FinishRenderingDoFBlurBuffer();
							DrawingContext.bRenderingToDoFBlurBuffer = FALSE;
						}

						const FLightSceneInfo* Light = Interaction->GetLight();
						const FProjectedShadowInfo* EffectiveTranslucentPreShadowInfo = 
							DrawingContext.TranslucentPreShadowInfo && DrawingContext.TranslucentPreShadowInfo->LightSceneInfo == Light ?
							DrawingContext.TranslucentPreShadowInfo : 
							NULL;
						const UBOOL bUseTranslucencyLightAttenuation = 
							Light->LightType == LightType_DominantDirectional
							// Can only read from the translucency light attenuation buffer if it has been resolved to
							&& GSceneRenderTargets.bResolvedTranslucencyDominantLightAttenuationTexture
							&& Material->TranslucencyInheritDominantShadowsFromOpaque();
						Light->DrawTranslucentMesh(View,Mesh,bBackFace,bPreFog,bUseTranslucencyLightAttenuation,PrimitiveSceneInfo,EffectiveTranslucentPreShadowInfo,HitProxyId);
					}
				}
			}

			if (bDisableDepthTesting)
			{
				// Restore translucency default depth state (depth tests, no depth writes)
				RHISetDepthState( TStaticDepthState<FALSE,CF_LessEqual>::GetRHI() );
			}
			if (bOneLayerDistortion)
			{
				RHISetColorWriteMask(CW_RGBA);
			}
		}
	}
	return bDirty;
}
#endif

/**
 * Render a static mesh using a translucent draw policy
 * @return TRUE if the mesh rendered
 */
UBOOL FTranslucencyDrawingPolicyFactory::DrawStaticMesh(
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

/**
* Render a dynamic mesh using a dynamic lit translucent draw policy
* @return TRUE if the mesh rendered
*/
template<UBOOL bIsPostRenderDepthPass>
UBOOL TDynamicLitTranslucencyDepthDrawingPolicyFactory<bIsPostRenderDepthPass>::DrawDynamicMesh(
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId	)
{
	UBOOL bDirty = FALSE;

	const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterial();
	const EMaterialLightingModel LightingModel = Material->GetLightingModel();
	const UBOOL bRender = bIsPostRenderDepthPass ? Material->RenderLitTranslucencyDepthPostpass() : Material->RenderLitTranslucencyPrepass();
	if( bRender )
	{
		FDepthDrawingPolicy DrawingPolicy(Mesh.VertexFactory,MaterialRenderProxy,*Material,TRUE,FALSE,bIsPostRenderDepthPass);
		DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
		for( INT BatchElementIndex=0;BatchElementIndex < Mesh.Elements.Num();BatchElementIndex++ )
		{
			DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,FMeshDrawingPolicy::ElementDataType());
			DrawingPolicy.DrawMesh(Mesh, BatchElementIndex);
		}
		bDirty = TRUE;
	}
	return bDirty;
}


/**
* Render a static mesh using a dynamic lit translucent draw policy
* @return TRUE if the mesh rendered
*/
template<UBOOL bIsPostRenderDepthPass>
UBOOL TDynamicLitTranslucencyDepthDrawingPolicyFactory<bIsPostRenderDepthPass>::DrawStaticMesh(
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
	if (LightingModel != MLM_Unlit)
	{
	    const UBOOL bNeedsBackfacePass = FALSE;
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
    }
	return bDirty;
}
						
/*-----------------------------------------------------------------------------
FTranslucentPrimSet
-----------------------------------------------------------------------------*/

/**
* Iterate over the sorted list of prims and draw them
* @param ViewInfo - current view used to draw items
* @param DPGIndex - current DPG used to draw items
* @param PassId 0:before SceneColor resolve 1:using SceneColor 2:separate translucency
* @return TRUE if anything was drawn
*/
UBOOL FTranslucentPrimSet::Draw(
	const FViewInfo& View,
	FSceneRenderer& Renderer,
	UINT DPGIndex,
	UINT PassId
	) const
{
	UBOOL bDirty = FALSE;

	const TArray<FSortedPrim,SceneRenderingAllocator>* PhaseSortedPrimitivesPtr = NULL;

	switch(PassId)
	{
		case 0:
			PhaseSortedPrimitivesPtr = &SortedPreSceneColorPrims;
			break;
		case 1:
			PhaseSortedPrimitivesPtr = &SortedPrims;
			break;
		case 2:
			PhaseSortedPrimitivesPtr = &SortedSceneColorPrims;
			break;
		case 3:
			PhaseSortedPrimitivesPtr = &SortedSeparateTranslucencyPrims;
			break;
		default:
			checkSlow(0);
	}

	// copy to reference for easier access
	const TArray<FSortedPrim,SceneRenderingAllocator>& PhaseSortedPrimitives = *PhaseSortedPrimitivesPtr;

	if( PhaseSortedPrimitives.Num() )
	{
		UBOOL bRenderingToLowResTranslucencyBuffer = FALSE;
		UBOOL bAllowDownsampling = TRUE;
		UBOOL bRenderingToDoFBlurBuffer = FALSE;
		const FProjectedShadowInfo* TranslucentPreShadowInfo = NULL;

		// For drawing scene prims with dynamic relevance.
		TDynamicPrimitiveDrawer<FTranslucencyDrawingPolicyFactory> TranslucencyDrawer(
			&View,
			DPGIndex,
			FTranslucencyDrawingPolicyFactory::ContextType(bRenderingToLowResTranslucencyBuffer,bAllowDownsampling,bRenderingToDoFBlurBuffer,TranslucentPreShadowInfo),
			FALSE
			);

		UBOOL bResolvedForOneLayerDistortion = FALSE;

		// Draw sorted scene prims
		for( INT PrimIdx=0; PrimIdx < PhaseSortedPrimitives.Num(); PrimIdx++ )
		{
			TranslucentPreShadowInfo = NULL;
			FPrimitiveSceneInfo* PrimitiveSceneInfo = PhaseSortedPrimitives(PrimIdx).PrimitiveSceneInfo;
			const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);

			if (PrimitiveSceneInfo)
			{
				bAllowDownsampling = View.ViewMatrix.TransformFVector(PrimitiveSceneInfo->Bounds.Origin).Z < GDownsampledTransluencyDistanceThreshold;
			}
			else
			{
				bAllowDownsampling = FALSE;
			}

			if (PrimitiveSceneInfo && PrimitiveSceneInfo->bCastDynamicShadow && PrimitiveSceneInfo->bAllowDynamicShadowsOnTranslucency)
			{
				// If the primitive receives dynamic shadows, search for the first light that casts a preshadow on the primitive
				for (const FLightPrimitiveInteraction* Interaction = PrimitiveSceneInfo->LightList; Interaction; Interaction = Interaction->GetNextLight())
				{
					if (Interaction->HasShadow() && Interaction->GetLight()->bAllowPreShadow && Interaction->GetLight() == PrimitiveSceneInfo->BrightestDominantLightSceneInfo)
					{
						// Can't use downsampling while receiving shadows as they share the same EDRAM on Xbox 360
						TransitionToFullResolutionTranslucency(View, bRenderingToLowResTranslucencyBuffer);
						bAllowDownsampling = FALSE;

						// Render depths for the translucent preshadow.
						// Preshadow depths for this primitive have probably already been rendered earlier in the frame, 
						// But the render target has already been reused for other purposes.
						TranslucentPreShadowInfo = Renderer.RenderTranslucentShadowDepths(Interaction->GetLight(), PrimitiveSceneInfo, DPGIndex);
						if (TranslucentPreShadowInfo)
						{
							// Restore states changed by shadow depth rendering
							GSceneRenderTargets.BeginRenderingTranslucency(View, FALSE, TRUE);
							// Enable depth test, disable depth writes.
							RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());
							break;
						}
					}
				}
			}

			if (ViewRelevance.bOneLayerDistortionRelevance && !bResolvedForOneLayerDistortion)
			{
				// One layer distortion does blending manually, so it can't be downsampled
				TransitionToFullResolutionTranslucency(View, bRenderingToLowResTranslucencyBuffer);
				bAllowDownsampling = FALSE;
				// Resolve scene color for the first one layer distortion primitive encountered
				GSceneRenderTargets.SaveSceneColorRaw(TRUE);
				bResolvedForOneLayerDistortion = TRUE;

				// Restore states changed by the resolve
				GSceneRenderTargets.BeginRenderingTranslucency(View, FALSE, TRUE);
			}

			if (ViewRelevance.bTranslucencyDoFRelevance && (View.Family->ShowFlags & SHOW_TranslucencyDoF) != 0 && DPGIndex == SDPG_World)
			{
				bAllowDownsampling = FALSE;
				if (!bRenderingToDoFBlurBuffer)
				{
					bRenderingToDoFBlurBuffer = TRUE;
					GSceneRenderTargets.BeginRenderingDoFBlurBuffer();
				}
			}
			else
			{
				if (bRenderingToDoFBlurBuffer)
				{
					GSceneRenderTargets.FinishRenderingDoFBlurBuffer();
					bRenderingToDoFBlurBuffer = FALSE;
				}
			}

			checkSlow(ViewRelevance.bTranslucentRelevance);
			
			// Render dynamic scene prim
			if( ViewRelevance.bDynamicRelevance )
			{
				TranslucencyDrawer.SetPrimitive(PrimitiveSceneInfo);
				PrimitiveSceneInfo->Proxy->DrawDynamicElements(
					&TranslucencyDrawer,
					&View,
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
					if (View.StaticMeshVisibilityMap(StaticMesh.Id)
						// Only render static mesh elements using translucent materials
						&& StaticMesh.IsTranslucent() )
					{
						bDirty |= FTranslucencyDrawingPolicyFactory::DrawStaticMesh(
							View,
							FTranslucencyDrawingPolicyFactory::ContextType(bRenderingToLowResTranslucencyBuffer,bAllowDownsampling,bRenderingToDoFBlurBuffer,TranslucentPreShadowInfo),
							StaticMesh,
							FALSE,
							PrimitiveSceneInfo,
							StaticMesh.HitProxyId
							);
					}
				}
			}
			
			if( ViewRelevance.IsDecalRelevant() )
			{
				SCOPE_CYCLE_COUNTER(STAT_DecalRenderUnlitTime);

				bAllowDownsampling = FALSE;
				// render dynamic translucent decals on translucent receivers
				if( ViewRelevance.bDecalDynamicRelevance )
				{
					UBOOL bDrawOpaqueDecals;
					UBOOL bDrawTransparentDecals;
					GetDrawDecalFilters (ViewRelevance.bOpaqueRelevance, ViewRelevance.bTranslucentRelevance, TRUE, bDrawOpaqueDecals, bDrawTransparentDecals);

					TranslucencyDrawer.SetPrimitive(PrimitiveSceneInfo);
					PrimitiveSceneInfo->Proxy->DrawDynamicDecalElements(
						&TranslucencyDrawer,
						&View,
						DPGIndex,
						FALSE,
						bDrawOpaqueDecals,
						bDrawTransparentDecals,
						TRUE
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
							View.DecalStaticMeshVisibilityMap(Decal->DecalStaticMesh->Id) &&
							Decal->DecalStaticMesh->IsTranslucent() )
						{
							bDirty |= FTranslucencyDrawingPolicyFactory::DrawStaticMesh(
								View,
								FTranslucencyDrawingPolicyFactory::ContextType(bRenderingToLowResTranslucencyBuffer,bAllowDownsampling,bRenderingToDoFBlurBuffer,TranslucentPreShadowInfo),
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

		if (bRenderingToDoFBlurBuffer)
		{
			GSceneRenderTargets.FinishRenderingDoFBlurBuffer();
			bRenderingToDoFBlurBuffer = FALSE;
		}

		// If still rendering to the translucency buffer, resolve and apply it now
		TransitionToFullResolutionTranslucency(View, bRenderingToLowResTranslucencyBuffer);

		// Mark dirty if dynamic drawer rendered
		bDirty |= TranslucencyDrawer.IsDirty();
	}
	return bDirty;
}


/**
* Render Lit Translucent prims that need a depth prepass
* @param ViewInfo - current view used to draw items
* @param DPGIndex - current DPG used to draw items
* @return TRUE if anything was drawn
*/
UBOOL FTranslucentPrimSet::DrawPrepass(
	const FViewInfo& View,
	UINT DPGIndex
	)
{
	UBOOL bDirty=FALSE;

	// For drawing depth prepass for dynamic lit translucent scene prims with dynamic relevance.
	TDynamicPrimitiveDrawer<TDynamicLitTranslucencyDepthDrawingPolicyFactory<FALSE> > LitTranslucencyDepthDrawer(
		&View,
		DPGIndex,
		TDynamicLitTranslucencyDepthDrawingPolicyFactory<FALSE>::ContextType(),
		FALSE,
		FALSE,
		FALSE,
		TRUE
		);		


	// Draw depth-only prims
	for( INT PrimIdx=0; PrimIdx < LitTranslucencyDepthPrepassPrims.Num(); PrimIdx++ )
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = LitTranslucencyDepthPrepassPrims(PrimIdx);
		const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);

		// Render dynamic scene prim
		if( ViewRelevance.bDynamicRelevance )
		{
			LitTranslucencyDepthDrawer.SetPrimitive(PrimitiveSceneInfo);
			PrimitiveSceneInfo->Proxy->DrawDynamicElements(
				&LitTranslucencyDepthDrawer,
				&View,
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
				if (View.StaticMeshVisibilityMap(StaticMesh.Id)
					// Only render static mesh elements using translucent materials
					&& StaticMesh.IsTranslucent() )
				{
					bDirty |= TDynamicLitTranslucencyDepthDrawingPolicyFactory<FALSE>::DrawStaticMesh(
						View,
						TDynamicLitTranslucencyDepthDrawingPolicyFactory<FALSE>::ContextType(),
						StaticMesh,
						FALSE,
						PrimitiveSceneInfo,
						StaticMesh.HitProxyId
						);
				}
			}
		}
	}

	return bDirty;
}

/**
* Render Lit Translucent prims that need a post-rendering depth pass
* @param ViewInfo - current view used to draw items
* @param DPGIndex - current DPG used to draw items
* @return TRUE if anything was drawn
*/
UBOOL FTranslucentPrimSet::DrawPostpass(
	const FViewInfo& View,
	UINT DPGIndex
	)
{
	UBOOL bDirty=FALSE;

	// For post-render drawing of depth for dynamic lit translucent scene prims with dynamic relevance.
	TDynamicPrimitiveDrawer<TDynamicLitTranslucencyDepthDrawingPolicyFactory<TRUE> > LitTranslucencyDepthDrawer(
		&View,
		DPGIndex,
		TDynamicLitTranslucencyDepthDrawingPolicyFactory<TRUE>::ContextType(),
		FALSE,
		FALSE,
		FALSE,
		TRUE
		);		


	// Draw depth-only prims
	for( INT PrimIdx=0; PrimIdx < LitTranslucencyDepthPostpassPrims.Num(); PrimIdx++ )
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = LitTranslucencyDepthPostpassPrims(PrimIdx);
		const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);

		// Render dynamic scene prim
		if( ViewRelevance.bDynamicRelevance )
		{
			LitTranslucencyDepthDrawer.SetPrimitive(PrimitiveSceneInfo);
			PrimitiveSceneInfo->Proxy->DrawDynamicElements(
				&LitTranslucencyDepthDrawer,
				&View,
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
				if (View.StaticMeshVisibilityMap(StaticMesh.Id)
					// Only render static mesh elements using translucent materials
					&& StaticMesh.IsTranslucent() )
				{
					bDirty |= TDynamicLitTranslucencyDepthDrawingPolicyFactory<TRUE>::DrawStaticMesh(
						View,
						TDynamicLitTranslucencyDepthDrawingPolicyFactory<TRUE>::ContextType(),
						StaticMesh,
						FALSE,
						PrimitiveSceneInfo,
						StaticMesh.HitProxyId
						);
				}
			}
		}
	}

	// Mark dirty if dynamic drawer rendered
	bDirty |= LitTranslucencyDepthDrawer.IsDirty();

	return bDirty;
}


/**
* Render Soft masked prims depth pass
* @param ViewInfo - current view used to draw items
* @param DPGIndex - current DPG used to draw items
* @return TRUE if anything was drawn
*/
UBOOL FTranslucentPrimSet::DrawSoftMaskedDepth(
	const FViewInfo& View,
	UINT DPGIndex 
	)
{
	UBOOL bDirty = FALSE;

	TDynamicPrimitiveDrawer<FDepthDrawingPolicyFactory> Drawer(&View, DPGIndex, FDepthDrawingPolicyFactory::ContextType(DDM_SoftMaskedOnly, TRUE), TRUE);

	for( INT PrimIdx=0; PrimIdx < SoftMaskedSortedPrims.Num(); PrimIdx++ )
	{
		FDepthSortedPrim &PrimRef = SoftMaskedSortedPrims(PrimIdx); 
		FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimRef.PrimitiveSceneInfo;
		const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);

		// Render dynamic scene prim
		if( ViewRelevance.bDynamicRelevance )
		{
			Drawer.SetPrimitive(PrimitiveSceneInfo);
			PrimitiveSceneInfo->Proxy->DrawDynamicElements(
				&Drawer,
				&View,
				DPGIndex
				);
			bDirty |= Drawer.IsDirty();
		}
		// Render static scene prim
		if( ViewRelevance.bStaticRelevance )
		{
			// Render static meshes from static scene prim
			for( INT StaticMeshIdx=0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++ )
			{
				FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes(StaticMeshIdx);
				if (View.StaticMeshVisibilityMap(StaticMesh.Id)
					&& StaticMesh.IsSoftMasked() )
				{
					bDirty |= FDepthDrawingPolicyFactory::DrawStaticMesh(
						View,
						FDepthDrawingPolicyFactory::ContextType(DDM_AllOccluders,TRUE),
						StaticMesh,
						FALSE,
						PrimitiveSceneInfo,
						StaticMesh.HitProxyId
						);
				}
			}
		}
	}

	return bDirty;
}



/**
* Render Soft masked prims after depth pass, sorted back to front
* @param ViewInfo - current view used to draw items
* @param DPGIndex - current DPG used to draw items
* @return TRUE if anything was drawn
*/
UBOOL FTranslucentPrimSet::DrawSoftMaskedBase(
	const FViewInfo& View,
	UINT DPGIndex
	)
{
	UBOOL bDirty = FALSE;
	UBOOL bRenderingToLowResTranslucencyBuffer = FALSE;
	UBOOL bAllowDownsampling = FALSE;
	UBOOL bRenderingToDoFBlurBuffer = FALSE;
	const FProjectedShadowInfo* TranslucentPreShadowInfo = NULL;

	// For post-render drawing of depth for dynamic lit translucent scene prims with dynamic relevance.
	TDynamicPrimitiveDrawer<FTranslucencyDrawingPolicyFactory> Drawer(
		&View,
		DPGIndex,
		FTranslucencyDrawingPolicyFactory::ContextType(bRenderingToLowResTranslucencyBuffer,bAllowDownsampling,bRenderingToDoFBlurBuffer,TranslucentPreShadowInfo),
		FALSE,
		FALSE,
		FALSE,
		TRUE
		);		

	for( INT PrimIdx=0; PrimIdx < SoftMaskedSortedPrims.Num(); PrimIdx++ )
	{
		FDepthSortedPrim &PrimRef = SoftMaskedSortedPrims(PrimIdx); 
		FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimRef.PrimitiveSceneInfo;
		const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);

		// Render dynamic scene prim
		if( ViewRelevance.bDynamicRelevance )
		{
			Drawer.SetPrimitive(PrimitiveSceneInfo);
			PrimitiveSceneInfo->Proxy->DrawDynamicElements(
				&Drawer,
				&View,
				DPGIndex
				);
			bDirty |= Drawer.IsDirty();
		}
		// Render static scene prim
		if( ViewRelevance.bStaticRelevance )
		{
			// Render static meshes from static scene prim
			for( INT StaticMeshIdx=0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++ )
			{
				FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes(StaticMeshIdx);
				if (View.StaticMeshVisibilityMap(StaticMesh.Id)
					&& StaticMesh.IsSoftMasked() )
				{
					bDirty |= FTranslucencyDrawingPolicyFactory::DrawStaticMesh(
						View,
						FTranslucencyDrawingPolicyFactory::ContextType(bRenderingToLowResTranslucencyBuffer,bAllowDownsampling,bRenderingToDoFBlurBuffer,TranslucentPreShadowInfo),
						StaticMesh,
						TRUE,
						PrimitiveSceneInfo,
						StaticMesh.HitProxyId
						);
				}
			}
		}
	}

	return bDirty;
}

/**
* Add a new primitive to the list of sorted prims
* @param PrimitiveSceneInfo - primitive info to add. Origin of bounds is used for sort.
* @param ViewInfo - used to transform bounds to view space
* @param bUsesSceneColor - primitive samples from scene color
* @param bNeedsDepthPrepass - primitive needs a depth prepass (for Opacity=1.0 pixels)
* @param bNeedsDepthPostpass - primitive needs a post-render pass (for Opacity>0.0 pixels)
*/
void FTranslucentPrimSet::AddScenePrimitive(
	FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FViewInfo& ViewInfo, 
	UBOOL bUsesSceneColor, 
	UBOOL bSceneTextureRenderBehindTranslucency,
	UBOOL bNeedsDepthPrepass, 
	UBOOL bNeedsDepthPostpass, 
	UBOOL bUseSeparateTranslucency)
{
	FLOAT SortKey=0.f;
	FFogVolumeDensitySceneInfo** FogDensityInfoRef = PrimitiveSceneInfo->Scene->FogVolumes.Find(PrimitiveSceneInfo->Component);
	UBOOL bIsFogVolume = FogDensityInfoRef != NULL;
	if (bIsFogVolume)
	{
		const FFogVolumeDensitySceneInfo* FogDensityInfo = *FogDensityInfoRef;
		check(FogDensityInfo);
		if (FogDensityInfo->bAffectsTranslucency)
		{
			//sort by view space depth + primitive radius, so that intersecting translucent objects are drawn first,
			//which is needed for fogging the translucent object.
			SortKey = ViewInfo.ViewMatrix.TransformFVector(PrimitiveSceneInfo->Bounds.Origin).Z + 0.7f * PrimitiveSceneInfo->Bounds.SphereRadius;
		}
		else
		{
			//sort based on view space depth
			SortKey = ViewInfo.ViewMatrix.TransformFVector(PrimitiveSceneInfo->Bounds.Origin).Z;
		}
	}
	else
	{
		//sort based on view space depth
		SortKey = ViewInfo.ViewMatrix.TransformFVector(PrimitiveSceneInfo->Bounds.Origin).Z;

		PrimitiveSceneInfo->FogVolumeSceneInfo = NULL;
		INT DPGIndex = PrimitiveSceneInfo->Proxy->GetDepthPriorityGroup(&ViewInfo);
		FLOAT LargestFogVolumeRadius = 0.0f;
		//find the largest fog volume this translucent object is intersecting with
		for( TMap<const UPrimitiveComponent*, FFogVolumeDensitySceneInfo*>::TIterator FogVolumeIt(PrimitiveSceneInfo->Scene->FogVolumes); FogVolumeIt; ++FogVolumeIt )
		{
			const UPrimitiveComponent* FogVolumePrimComponent = FogVolumeIt.Key();
			FFogVolumeDensitySceneInfo* FogVolumeDensityInfo = FogVolumeIt.Value();
			if (FogVolumePrimComponent 
				&& FogVolumeDensityInfo 
				&& FogVolumeDensityInfo->bAffectsTranslucency
				&& FogVolumeDensityInfo->DPGIndex == DPGIndex)
			{
				const FLOAT FogVolumeRadius = FogVolumePrimComponent->Bounds.SphereRadius;
				const FLOAT TranslucentObjectRadius = PrimitiveSceneInfo->Bounds.SphereRadius;
				if (FogVolumeRadius > LargestFogVolumeRadius)
				{
					const FLOAT DistSquared = (FogVolumePrimComponent->Bounds.Origin - PrimitiveSceneInfo->Bounds.Origin).SizeSquared();
					if (DistSquared < FogVolumeRadius * FogVolumeRadius + TranslucentObjectRadius * TranslucentObjectRadius)
					{
						LargestFogVolumeRadius = FogVolumeRadius;
						PrimitiveSceneInfo->FogVolumeSceneInfo = FogVolumeDensityInfo;
					}
				}
			}
		}
	}

	if( bUseSeparateTranslucency )
	{
		// add to list of translucent prims that use scene color
		new(SortedSeparateTranslucencyPrims) FSortedPrim(PrimitiveSceneInfo,SortKey,PrimitiveSceneInfo->TranslucencySortPriority);
	}
	else
	{
		if( bUsesSceneColor )
		{
			if (bSceneTextureRenderBehindTranslucency)
			{
				// add to list of translucent prims that use scene color
				new(SortedPreSceneColorPrims) FSortedPrim(PrimitiveSceneInfo,SortKey,PrimitiveSceneInfo->TranslucencySortPriority);
			}
			else
			{
				// add to list of translucent prims that use scene color
				new(SortedSceneColorPrims) FSortedPrim(PrimitiveSceneInfo,SortKey,PrimitiveSceneInfo->TranslucencySortPriority);
			}
		}
		else
		{
			// add to list of translucent prims
			new(SortedPrims) FSortedPrim(PrimitiveSceneInfo,SortKey,PrimitiveSceneInfo->TranslucencySortPriority);
		}
	}

	if( bNeedsDepthPrepass )
	{
		LitTranslucencyDepthPrepassPrims.AddItem(PrimitiveSceneInfo);
	}

	if( bNeedsDepthPostpass )
	{
		LitTranslucencyDepthPostpassPrims.AddItem(PrimitiveSceneInfo);
	}
}

/**
* Add a new primitive to the list of sorted prims
* @param PrimitiveSceneInfo - primitive info to add. Origin of bounds is used for sort.
*/
void FTranslucentPrimSet::AddScenePrimitiveSoftMasked(FPrimitiveSceneInfo* PrimitiveSceneInfo, const FViewInfo& ViewInfo)
{
	//sort based on view space depth
	FLOAT SortKey = ViewInfo.ViewMatrix.TransformFVector(PrimitiveSceneInfo->Bounds.Origin).Z;

	new(SoftMaskedSortedPrims) FDepthSortedPrim(PrimitiveSceneInfo,SortKey);
}

/**
* Sort any primitives that were added to the set back-to-front
*/
void FTranslucentPrimSet::SortPrimitives()
{
	// sort prims based on depth
	Sort<USE_COMPARE_CONSTREF(FSortedPrim,TranslucentRender)>(&SortedPreSceneColorPrims(0),SortedPreSceneColorPrims.Num());
	Sort<USE_COMPARE_CONSTREF(FSortedPrim,TranslucentRender)>(&SortedPrims(0),SortedPrims.Num());
	Sort<USE_COMPARE_CONSTREF(FSortedPrim,TranslucentRender)>(&SortedSceneColorPrims(0),SortedSceneColorPrims.Num());
	Sort<USE_COMPARE_CONSTREF(FSortedPrim,TranslucentRender)>(&SortedSeparateTranslucencyPrims(0),SortedSeparateTranslucencyPrims.Num());
	Sort<USE_COMPARE_CONSTREF(FDepthSortedPrim,TranslucentRender)>(&SoftMaskedSortedPrims(0),SoftMaskedSortedPrims.Num());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FSceneRenderer::RenderTranslucency
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Renders the scene's translucency.
 *
 * @param	DPGIndex	Current DPG used to draw items.
 * @return				TRUE if anything was drawn.
 */
UBOOL FSceneRenderer::RenderTranslucency(UINT DPGIndex)
{
	SCOPED_DRAW_EVENT(EventTranslucent)(DEC_SCENE_ITEMS,TEXT("Translucency"));

#if PS3 && !USE_NULL_RHI
	extern UBOOL GEnableDrawCounter;
	extern UBOOL GTriggerDrawCounter;
	if ( GEnableDrawCounter )
	{
		GTriggerDrawCounter = TRUE;
	}
#endif

	UBOOL bRenderPrepass=FALSE;
	UBOOL bRender=FALSE;
	for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		const FViewInfo& View = Views(ViewIndex);
		const UBOOL bViewHasTranslucentViewMeshElements = View.bHasTranslucentViewMeshElements & (1<<DPGIndex);
		if( View.TranslucentPrimSet[DPGIndex].NumPrims() > 0 || bViewHasTranslucentViewMeshElements )
		{
			bRender=TRUE;
			if( bRender && bRenderPrepass )
				break;
		}
		if( View.TranslucentPrimSet[DPGIndex].NumDepthPrepassPrims() > 0 )
		{
			bRenderPrepass=TRUE;
			if( bRender && bRenderPrepass )
				break;
		}
	}

	UBOOL bDirty = FALSE;
	if( bRender )
	{
		// Render prepass
		if( bRenderPrepass )
		{
			GSceneRenderTargets.BeginRenderingPrePass();

			for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				FViewInfo& View = Views(ViewIndex);
				if( View.TranslucentPrimSet[DPGIndex].NumDepthPrepassPrims() == 0 )
				{
					continue;
				}

				SCOPED_CONDITIONAL_DRAW_EVENT(EventView,Views.Num() > 1)(DEC_SCENE_ITEMS,TEXT("PrePassView%d"),ViewIndex);

				// viewport to match view size
				RHISetViewport(View.RenderTargetX,View.RenderTargetY,0.0f,View.RenderTargetX + View.RenderTargetSizeX,View.RenderTargetY + View.RenderTargetSizeY,1.0f);
				RHISetViewParameters(View);
				RHISetMobileHeightFogParams(View.HeightFogParams);

				// Opaque blending, depth tests and writes.
				RHISetBlendState(TStaticBlendState<>::GetRHI());
				RHISetDepthState(TStaticDepthState<TRUE,CF_LessEqual>::GetRHI());

				bDirty |= View.TranslucentPrimSet[DPGIndex].DrawPrepass(View,DPGIndex);
			}

			GSceneRenderTargets.FinishRenderingPrePass();
		}

		//reset the fog apply stencil index for this frame
		ResetFogVolumeIndex();

		for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENT(EventView,Views.Num() > 1)(DEC_SCENE_ITEMS,TEXT("View%d"),ViewIndex);

			const FViewInfo& View = Views(ViewIndex);

			// begin by assuming we will resolve the entire view family
			FResolveRect ResolveRect = FResolveRect(0, 0, FamilySizeX, FamilySizeY);

			UBOOL bRenderSceneColorMaterials = FALSE;
			if (View.TranslucentPrimSet[DPGIndex].NumSceneColorPrims() > 0 || View.TranslucentPrimSet[DPGIndex].NumPreSceneColorPrims() > 0)
			{
				bRenderSceneColorMaterials = bRender;
				// when enabled, determine the minimal screen space area to work in
				if (GRenderMinimalTranslucency)
				{
					FIntRect PixelRect;
					if (ComputeTranslucencyResolveRectangle(DPGIndex, PixelRect))
					{
						// update the custom resolve parameters
						ResolveRect.X1 = PixelRect.Min.X;
						ResolveRect.X2 = PixelRect.Max.X;
						ResolveRect.Y1 = PixelRect.Min.Y;
						ResolveRect.Y2 = PixelRect.Max.Y;
					}
					else
					{
						// if no custom bounds were found, assume we do not need to do any further work
						bRenderSceneColorMaterials = FALSE;
					}
				}
			}

			if (View.TranslucentPrimSet[DPGIndex].NumPreSceneColorPrims() > 0
				&& bRenderSceneColorMaterials)
			{
				// resolve scene color
				GSceneRenderTargets.SaveSceneColorRaw(TRUE, ResolveRect);
			}

			GSceneRenderTargets.BeginRenderingTranslucency(View, FALSE, FALSE);

			if (View.TranslucentPrimSet[DPGIndex].NumPreSceneColorPrims() > 0
				&& bRenderSceneColorMaterials)
			{
				if (GRenderMinimalTranslucency)
				{
					// Add a scissor rect to match the previously resolved area
					// We do this to prevent primitives with innacurate bounds from rendering outside the resolved area.
					// In non-final builds, we allow the out-of-bounds rendering to occur, helping visually identify these primitives.
					RHISetScissorRect(TRUE, ResolveRect.X1, ResolveRect.Y1, ResolveRect.X2, ResolveRect.Y2);
				}

				// Draw only translucent prims that read from scene color, and that should be rendered before everything else
				bDirty |= View.TranslucentPrimSet[DPGIndex].Draw(View,*this,DPGIndex,0);

				if (GRenderMinimalTranslucency)
				{
					// clear the scissor rect
					RHISetScissorRect(FALSE, 0, 0, 0, 0);
				}
			}
			
			UBOOL bRenderingToLowResTranslucencyBuffer = FALSE;
			UBOOL bAllowDownsampling = FALSE;
			UBOOL bRenderingToDoFBlurBuffer = FALSE;
			const FProjectedShadowInfo* TranslucentPreShadowInfo = NULL;

			// Draw the view's mesh elements with the translucent drawing policy.
			bDirty |= DrawViewElements<FTranslucencyDrawingPolicyFactory>(View,FTranslucencyDrawingPolicyFactory::ContextType(bRenderingToLowResTranslucencyBuffer,bAllowDownsampling,bRenderingToDoFBlurBuffer,TranslucentPreShadowInfo),DPGIndex,FALSE);

			// Draw only translucent prims that don't read from scene color
			bDirty |= View.TranslucentPrimSet[DPGIndex].Draw(View,*this,DPGIndex,1);

			if (View.TranslucentPrimSet[DPGIndex].NumSceneColorPrims() > 0
				&& bRenderSceneColorMaterials)
			{
				// resolve scene color
				GSceneRenderTargets.SaveSceneColorRaw(TRUE, ResolveRect);
				
				// Restore states changed by the resolve
				GSceneRenderTargets.BeginRenderingTranslucency(View, FALSE, FALSE);

				if (GRenderMinimalTranslucency)
				{
					// Add a scissor rect to match the previously resolved area
					// We do this to prevent primitives with innacurate bounds from rendering outside the resolved area.
					// In non-final builds, we allow the out-of-bounds rendering to occur, helping visually identify these primitives.
					RHISetScissorRect(TRUE, ResolveRect.X1, ResolveRect.Y1, ResolveRect.X2, ResolveRect.Y2);
				}
				
				// Draw only translucent prims that read from scene color
				bDirty |= View.TranslucentPrimSet[DPGIndex].Draw(View,*this,DPGIndex,2);

				if (GRenderMinimalTranslucency)
				{
					// clear the scissor rect
					RHISetScissorRect(FALSE, 0, 0, 0, 0);
				}
			}

			if (View.TranslucentPrimSet[DPGIndex].NumSeparateTranslucencyPrims() > 0)
			{
				GSceneRenderTargets.BeginRenderingSeparateTranslucency(View);

				// Draw only translucent prims that are in the SeparateTranslucency pass
				bDirty |= View.TranslucentPrimSet[DPGIndex].Draw(View,*this,DPGIndex,3);

				GSceneRenderTargets.FinishRenderingSeparateTranslucency();
			}
		}
	}

	bDirty |= RenderPostTranslucencyDepths(DPGIndex);

#if PS3 && !USE_NULL_RHI
	GTriggerDrawCounter = FALSE;
#endif

#if XBOX
	if (DPGIndex == SDPG_World)
	{
		for (INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			FViewInfo& View = Views(ViewIndex);
			if ((View.Family->ShowFlags & SHOW_TranslucencyDoF) != 0 && View.bRenderedToDoFBlurBuffer)
			{
				RHICopyToResolveTarget(GSceneRenderTargets.GetDoFBlurBufferSurface(), FALSE, FResolveParams());
				break;
			}
		}
	}
#endif

	return bDirty;
}


/** 
* Renders any depths for pixels touched by lit translucent objects, for correct fog and DoF.
*
* @param	DPGIndex	Current DPG used to draw items.
* @return				TRUE if anything was drawn.
*/
UBOOL FSceneRenderer::RenderPostTranslucencyDepths(UINT DPGIndex)
{
	UBOOL bRender=FALSE;
	for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		const FViewInfo& View = Views(ViewIndex);
		if( View.TranslucentPrimSet[DPGIndex].NumDepthPostpassPrims() > 0 )
		{
			bRender=TRUE;
			break;
		}
	}

	UBOOL bDirty = FALSE;
	if( bRender )
	{
		GSceneRenderTargets.BeginRenderingPostTranslucencyDepth();

		for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			FViewInfo& View = Views(ViewIndex);
			if( View.TranslucentPrimSet[DPGIndex].NumDepthPostpassPrims() == 0 )
			{
				continue;
			}

			SCOPED_CONDITIONAL_DRAW_EVENT(EventView,Views.Num() > 1)(DEC_SCENE_ITEMS,TEXT("PostTranslucencyDepthView%d"),ViewIndex);

			// viewport to match view size
			RHISetViewport(View.RenderTargetX,View.RenderTargetY,0.0f,View.RenderTargetX + View.RenderTargetSizeX,View.RenderTargetY + View.RenderTargetSizeY,1.0f);
			RHISetViewParameters(View);
			RHISetMobileHeightFogParams(View.HeightFogParams);

			// Opaque blending, depth tests and writes.
			RHISetBlendState(TStaticBlendState<>::GetRHI());
			RHISetDepthState(TStaticDepthState<TRUE,CF_LessEqual>::GetRHI());

			bDirty |= View.TranslucentPrimSet[DPGIndex].DrawPostpass(View,DPGIndex);
		}

		GSceneRenderTargets.FinishRenderingPostTranslucencyDepth(DPGIndex==SDPG_World && bDirty);
	}

	return bDirty;
}


/** 
* Renders soft masked objects depth pass (only required on hardware where we cannot access the native z buffer).
*
* @param	DPGIndex	Current DPG used to draw items.
* @return				TRUE if anything was drawn.
*/
UBOOL FSceneRenderer::RenderSoftMaskedDepth(UINT DPGIndex)
{
	// On PC we need to setup depth in the alpha channel and because we do blending
	// with the alpha value we need this additional pass.
	check(IsPCPlatform(GRHIShaderPlatform));

	UBOOL bDirty = FALSE;

	for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		FViewInfo& View = Views(ViewIndex);
		if( View.TranslucentPrimSet[DPGIndex].NumSoftMaskedSortedPrims() == 0 )
		{
			continue;
		}

		SCOPED_CONDITIONAL_DRAW_EVENT(EventView,Views.Num() > 1)(DEC_SCENE_ITEMS,TEXT("SoftMaskedDepthView%d"),ViewIndex);

		// viewport to match view size
		RHISetViewport(View.RenderTargetX,View.RenderTargetY,0.0f,View.RenderTargetX + View.RenderTargetSizeX,View.RenderTargetY + View.RenderTargetSizeY,1.0f);
		RHISetViewParameters(View);
		RHISetMobileHeightFogParams(View.HeightFogParams);

		RHISetDepthState(TStaticDepthState<TRUE,CF_LessEqual>::GetRHI());
		bDirty |= View.TranslucentPrimSet[DPGIndex].DrawSoftMaskedDepth(View, DPGIndex);
	}

	return bDirty;
}



/** 
* Renders soft masked objects as alpha transparent, sorted back to front.
*
* @param	DPGIndex	Current DPG used to draw items.
* @return				TRUE if anything was drawn.
*/
UBOOL FSceneRenderer::RenderSoftMaskedBase(UINT DPGIndex)
{
	SCOPED_DRAW_EVENT(EventRenderSoftMasked)(DEC_SCENE_ITEMS,TEXT("RenderSoftMasked"));
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_SoftMaskedDrawTime, !bIsSceneCapture);

	UBOOL bDirty = FALSE;
	UBOOL bChangedGPRAllocation = FALSE;

	for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		FViewInfo& View = Views(ViewIndex);
		if( View.TranslucentPrimSet[DPGIndex].NumSoftMaskedSortedPrims() == 0 )
		{
			continue;
		}

		SCOPED_CONDITIONAL_DRAW_EVENT(EventView,Views.Num() > 1)(DEC_SCENE_ITEMS,TEXT("SoftMaskedBaseView%d"),ViewIndex);

		// viewport to match view size
		RHISetViewport(View.RenderTargetX,View.RenderTargetY,0.0f,View.RenderTargetX + View.RenderTargetSizeX,View.RenderTargetY + View.RenderTargetSizeY,1.0f);
		RHISetViewParameters(View);
		RHISetMobileHeightFogParams(View.HeightFogParams);

		RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());

		if (!bChangedGPRAllocation && View.TranslucentPrimSet[DPGIndex].NumSoftMaskedSortedPrims() > 0)
		{
			extern INT GBasePassMaskedPixelGPRs;
			RHISetShaderRegisterAllocation(128 - GBasePassMaskedPixelGPRs, GBasePassMaskedPixelGPRs);
			bChangedGPRAllocation = TRUE;
		}

		bDirty |= View.TranslucentPrimSet[DPGIndex].DrawSoftMaskedBase(View, DPGIndex);
	}

	if (bChangedGPRAllocation)
	{
		RHISetShaderRegisterAllocation(64, 64);
	}

	return bDirty;
}
