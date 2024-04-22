/*=============================================================================
	LightRendering.cpp: Light rendering implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "LightRendering.h"
#include "SceneFilterRendering.h"

/** Returns TRUE if the mesh - light combination have a deferred interaction. */
UBOOL HasDeferredInteraction(
	const FMaterial* Material,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FLightSceneInfo* LightSceneInfo,
	const FMeshBatch& Mesh
	)
{
	const BYTE PrimitiveChannelMask = PrimitiveSceneInfo->LightingChannels.GetDeferredShadingChannelMask();
	const BYTE LightChannelMask = LightSceneInfo->LightingChannels.GetDeferredShadingChannelMask();
	return ShouldUseDeferredShading()
		&& MeshSupportsDeferredLighting(Material, PrimitiveSceneInfo)
		&& LightSceneInfo->SupportsDeferredShading()
		// At least one of the deferred lighting channels must match for there to be a deferred interaction
		&& (PrimitiveChannelMask & LightChannelMask)
		// We can only apply deferred lighting to the world DPG.
		&& Mesh.DepthPriorityGroup == SDPG_World;
}

/** A vertex shader for rendering the light in a deferred pass. */
template<UBOOL bRadialLight>
class TDeferredLightVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDeferredLightVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM5;
	}

	TDeferredLightVertexShader()	{}
	TDeferredLightVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(const FViewInfo& View)
	{
		DeferredParameters.Set(View, this);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredVertexShaderParameters DeferredParameters;
};

// Implement a version for directional lights, and a version for point / spot lights
IMPLEMENT_SHADER_TYPE(template<>,TDeferredLightVertexShader<FALSE>,TEXT("DeferredLightShaders"),TEXT("DirectionalVertexMain"),SF_Vertex,0,0);
IMPLEMENT_SHADER_TYPE(template<>,TDeferredLightVertexShader<TRUE>,TEXT("DeferredLightShaders"),TEXT("RadialVertexMain"),SF_Vertex,0,0);

/** A pixel shader for rendering the light in a deferred pass. */
template<UBOOL bSupportMSAA, UBOOL bRadialAttenuation, UBOOL bSpotAttenuation>
class TDeferredLightPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDeferredLightPixelShader,Global)
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM5;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("DEFERRED_LIGHT_MSAA"), bSupportMSAA ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("RADIAL_ATTENUATION"), bRadialAttenuation ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("SPOT_ATTENUATION"), bSpotAttenuation ? TEXT("1") : TEXT("0"));
	}

	TDeferredLightPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		LightPositionAndInvRadiusParameter.Bind(Initializer.ParameterMap, TEXT("LightPositionAndInvRadius"), TRUE);
		LightColorAndFalloffExponentParameter.Bind(Initializer.ParameterMap, TEXT("LightColorAndFalloffExponent"), TRUE);
		LightDirectionParameter.Bind(Initializer.ParameterMap, TEXT("LightDirectionAndChannel"), TRUE);
		SpotAnglesParameter.Bind(Initializer.ParameterMap, TEXT("SpotAngles"), TRUE);
		DistanceFadeParameter.Bind(Initializer.ParameterMap, TEXT("DistanceFadeParameters"), TRUE);
		LightAttenuationTextureParameter.Bind(Initializer.ParameterMap, TEXT("LightAttenuationTexture"), TRUE);
	}

	TDeferredLightPixelShader()
	{
	}

	void SetParameters(const FSceneView& View, const FLightSceneInfo* LightSceneInfo)
	{
		DeferredParameters.Set(View, this);

		FVector4 LightPositionAndInvRadius;
		FVector4 LightColorAndFalloffExponent;
		FVector LightDirection;
		FVector2D SpotAngles;
		// Get the light parameters
		LightSceneInfo->GetParameters(LightPositionAndInvRadius, LightColorAndFalloffExponent, LightDirection, SpotAngles);

		SetPixelShaderValue(GetPixelShader(), LightPositionAndInvRadiusParameter, LightPositionAndInvRadius);
		SetPixelShaderValue(GetPixelShader(), LightColorAndFalloffExponentParameter, LightColorAndFalloffExponent);
		SetPixelShaderValue(GetPixelShader(), LightDirectionParameter, FVector4(LightDirection, LightSceneInfo->LightingChannels.GetDeferredShadingChannelMask()));
		SetPixelShaderValue(GetPixelShader(), SpotAnglesParameter, SpotAngles);

		FVector2D DistanceFadeValues;
		const UBOOL bEnableDistanceShadowFading = View.Family->ShouldDrawShadows()
			&& GSystemSettings.bAllowWholeSceneDominantShadows
			&& (View.RenderingOverrides.bAllowDominantWholeSceneDynamicShadows || !LightSceneInfo->bStaticShadowing)
			&& LightSceneInfo->GetDirectionalLightDistanceFadeParameters(DistanceFadeValues);

		if (bEnableDistanceShadowFading)
		{
			SetPixelShaderValue(GetPixelShader(), DistanceFadeParameter, FVector4(DistanceFadeValues.X, DistanceFadeValues.Y, 0, 0));
		}
		else
		{
			SetPixelShaderValue(GetPixelShader(), DistanceFadeParameter, FVector4(0, 0, 0, 0));
		}

		if(LightAttenuationTextureParameter.IsBound())
		{
			SetTextureParameter(
				GetPixelShader(),
				LightAttenuationTextureParameter,
				TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
				GSceneRenderTargets.GetEffectiveLightAttenuationTexture(TRUE, TRUE)
				);
		}
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{		
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << LightPositionAndInvRadiusParameter;
		Ar << LightColorAndFalloffExponentParameter;
		Ar << LightDirectionParameter;
		Ar << SpotAnglesParameter;
		Ar << DistanceFadeParameter;
		Ar << LightAttenuationTextureParameter;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter LightPositionAndInvRadiusParameter;
	FShaderParameter LightColorAndFalloffExponentParameter;
	FShaderParameter LightDirectionParameter;
	FShaderParameter SpotAnglesParameter;
	FShaderParameter DistanceFadeParameter;
	FShaderResourceParameter LightAttenuationTextureParameter;
};

// Typedef is necessary because the C preprocessor thinks the comma in the template parameter list is a comma in the macro parameter list.
#define IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(bSupportMSAA, bRadialAttenuation, bSpotAttenuation, EntryName) \
	typedef TDeferredLightPixelShader<bSupportMSAA,bRadialAttenuation,bSpotAttenuation> TDeferredLightPixelShader##bSupportMSAA##bRadialAttenuation##bSpotAttenuation; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDeferredLightPixelShader##bSupportMSAA##bRadialAttenuation##bSpotAttenuation,TEXT("DeferredLightShaders"),EntryName,SF_Pixel,0,0);

// Implement a version for each light type, and then combinations for with MSAA and without
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(TRUE, TRUE, TRUE, TEXT("RadialPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(TRUE, TRUE, FALSE, TEXT("RadialPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(TRUE, FALSE, FALSE, TEXT("DirectionalPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(FALSE, TRUE, TRUE, TEXT("RadialPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(FALSE, TRUE, FALSE, TEXT("RadialPixelMain"));
IMPLEMENT_DEFERREDLIGHT_PIXELSHADER_TYPE(FALSE, FALSE, FALSE, TEXT("DirectionalPixelMain"));

// Bound shader states used for the first shading pass when MSAA is enabled
FGlobalBoundShaderState DeferredLightFirstPassBoundState[3];
// Bound shader states used when MSAA is disabled
FGlobalBoundShaderState DeferredLightNoMSAABoundState[3];

/** A pixel shader for rendering the light in a deferred pass that shades each MSAA sample. */
template<UBOOL bRadialAttenuation, UBOOL bSpotAttenuation>
class TDeferredLightPerSamplePixelShader : public TDeferredLightPixelShader<TRUE, bRadialAttenuation, bSpotAttenuation>
{
	DECLARE_SHADER_TYPE(TDeferredLightPerSamplePixelShader,Global)
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM5;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		TDeferredLightPixelShader<TRUE, bRadialAttenuation, bSpotAttenuation>::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	TDeferredLightPerSamplePixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	TDeferredLightPixelShader<TRUE, bRadialAttenuation, bSpotAttenuation>(Initializer)
	{
	}

	TDeferredLightPerSamplePixelShader()
	{
	}
};

#define IMPLEMENT_DEFERREDLIGHT_PERSAMPLE_PIXELSHADER_TYPE(bRadialAttenuation, bSpotAttenuation, EntryName) \
	typedef TDeferredLightPerSamplePixelShader<bRadialAttenuation,bSpotAttenuation> TDeferredLightPerSamplePixelShader##bRadialAttenuation##bSpotAttenuation; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDeferredLightPerSamplePixelShader##bRadialAttenuation##bSpotAttenuation,TEXT("DeferredLightShaders"),EntryName,SF_Pixel,0,0);

// Implement one shader per light type
IMPLEMENT_DEFERREDLIGHT_PERSAMPLE_PIXELSHADER_TYPE(TRUE, TRUE, TEXT("RadialSampleMain"));
IMPLEMENT_DEFERREDLIGHT_PERSAMPLE_PIXELSHADER_TYPE(TRUE, FALSE, TEXT("RadialSampleMain"));
IMPLEMENT_DEFERREDLIGHT_PERSAMPLE_PIXELSHADER_TYPE(FALSE, FALSE, TEXT("DirectionalSampleMain"));

// Bound shader states used for the second shader pass when MSAA is enabled
FGlobalBoundShaderState DeferredLightSecondPassBoundState[3];

/**
 * Helper function to encapsulate the logic for whether a light should be rendered,
 * which is needed in more than one place.
 */
static UBOOL ShouldRenderLight(const FVisibleLightViewInfo& VisibleLightViewInfo, UINT DPGIndex)
{
	// only render the light if it has visible primitives and is in the view frustum,
	// since only lights whose volumes are visible will contribute to the visible primitives.
	return VisibleLightViewInfo.DPGInfo[DPGIndex].bHasVisibleLitPrimitives && VisibleLightViewInfo.bInViewFrustum;
}

ELightInteractionType FMeshLightingDrawingPolicyFactory::AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh,FLightSceneInfo* Light)
{
	if (!StaticMesh->IsTranslucent() && !StaticMesh->IsDistortion())
	{
		// Don't draw the light on the mesh if it's unlit.
		// SoftMasked cannot handle additive lights properly so better not affect it at all
		const FMaterial* const Material = StaticMesh->MaterialRenderProxy->GetMaterial();

		if (Material->GetLightingModel() != MLM_Unlit 
			&& Material->GetBlendMode() != BLEND_SoftMasked 
			// Don't render the light affecting this mesh if it will be handled in a deferred pass
			&& !HasDeferredInteraction(Material, StaticMesh->PrimitiveSceneInfo, Light,*StaticMesh))
		{
			return Light->GetDPGInfo(StaticMesh->DepthPriorityGroup)->AttachStaticMesh(Light,StaticMesh);
		}
	}

	return LIT_CachedIrrelevant;
}

UBOOL FMeshLightingDrawingPolicyFactory::DrawDynamicMesh(
	const FSceneView& View,
	const FLightSceneInfo* Light,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId
	)
{
	if (!Mesh.IsTranslucent() && !Mesh.IsDistortion())
	{
		// Don't draw the light on the mesh if it's unlit.
		const FMaterial* const Material = Mesh.MaterialRenderProxy->GetMaterial();
		const EBlendMode BlendMode = Material->GetBlendMode();

		// SoftMasked cannot handle additive lights properly so better not affect it at all
		if (!IsTranslucentBlendMode(BlendMode) 
			&& BlendMode != BLEND_SoftMasked
			&& Material->GetLightingModel() != MLM_Unlit
			// Don't render the light affecting this mesh if it will be handled in a deferred pass
			&& !HasDeferredInteraction(Material, PrimitiveSceneInfo, Light, Mesh))
		{
			// Draw the light's effect on the primitive.
			return Light->GetDPGInfo(Mesh.DepthPriorityGroup)->DrawDynamicMesh(
				View,
				Light,
				Mesh,
				bBackFace,
				bPreFog,
				PrimitiveSceneInfo,
				HitProxyId
				);
		}
	}

	return FALSE;
}

UBOOL FSceneRenderer::RenderLights(UINT DPGIndex,UBOOL bAffectedByModulatedShadows, UBOOL bWasSceneColorDirty)
{
	UBOOL bSceneColorDirty = bWasSceneColorDirty;
	UBOOL bStencilBufferDirty = FALSE;	// The stencil buffer should've been cleared to 0 already
	UBOOL bSceneColorBound = FALSE;

	// Draw each light.
	for(TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights);LightIt;++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;

		// The light is affected by modulated shadows if it's a static light, or it has the bAffectedByModulatedShadows flag.
		const UBOOL bIsLightAffectedbyModulatedShadows =
			LightSceneInfoCompact.bStaticLighting ||
			LightSceneInfoCompact.bCastCompositeShadow;
		if(XOR(bIsLightAffectedbyModulatedShadows,bAffectedByModulatedShadows))
		{
			continue;
		}

		const UBOOL bIsLightBlack =
			Square(LightSceneInfoCompact.Color.R) < DELTA &&
			Square(LightSceneInfoCompact.Color.G) < DELTA &&
			Square(LightSceneInfoCompact.Color.B) < DELTA;

		// Nothing to do for black lights as modulated shadows are rendered independently.
		if( bIsLightBlack )
		{
			continue;
		}

		const INT LightId = LightIt.GetIndex();
		// Check if the light is visible in any of the views.
		UBOOL bLightIsVisibleInAnyView = FALSE;
		for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			if (ShouldRenderLight(Views(ViewIndex).VisibleLightInfos(LightId), DPGIndex))
			{
				bLightIsVisibleInAnyView = TRUE;
				break;
			}
		}

		if(bLightIsVisibleInAnyView)
		{
			const FLightSceneInfo* const LightSceneInfo = LightIt->LightSceneInfo;

			// Skip dominant lights if they are being rendered in the base pass
			if (IsDominantLightType(LightSceneInfo->LightType) && GOnePassDominantLight)
			{
				continue;
			}

			UBOOL bUseAttenuationBuffer = FALSE;
			UBOOL bSavedRawSceneColor = FALSE;

			// Assuming that lights not using the dynamic channel are cinematic lights, since that is the most common use case
			const TCHAR* CineStr = LightSceneInfo->LightingChannels.Dynamic ? TEXT("") : TEXT("Cine");
			SCOPED_DRAW_EVENT(EventLightPass)(DEC_SCENE_ITEMS,TEXT("%s %s"),*LightSceneInfo->GetLightName().ToString(), CineStr);

			// Check for a need to use the attenuation buffer
			const UBOOL bDrawShadows = ViewFamily.ShouldDrawShadows();

			// Normal shadows are needed if the light's LightShadowMode requires it or if bNonModulatedSelfShadowing is enabled
			const UBOOL bDrawNormalShadows = LightSceneInfo->LightShadowMode == LightShadow_Normal || LightSceneInfo->bNonModulatedSelfShadowing;

			if(bDrawShadows)
			{
				if (bDrawNormalShadows)
				{
					// Render non-modulated projected shadows to the attenuation buffer.
					bUseAttenuationBuffer |= CheckForProjectedShadows( LightSceneInfo, DPGIndex );
				}
			}

			// Render light function to the attenuation buffer.
			bUseAttenuationBuffer |= CheckForLightFunction( LightSceneInfo, DPGIndex );

			if (bUseAttenuationBuffer)
			{
				if (IsDominantLightType(LightSceneInfo->LightType) && !bIsSceneCapture)
				{
					INC_DWORD_STAT(STAT_ShadowCastingDominantLights);
				}

#if XBOX
				if (bSceneColorDirty)
				{
					// Save the color buffer
					GSceneRenderTargets.SaveSceneColorRaw();
					bSavedRawSceneColor = TRUE;
				}
#endif

				if (bSceneColorBound)
				{
					// Do not resolve to scene color texture, this is done lazily
					GSceneRenderTargets.FinishRenderingSceneColor(FALSE);
				}

				// Clear the light attenuation surface to white.
				GSceneRenderTargets.BeginRenderingLightAttenuation();
				bSceneColorBound = FALSE;

				if (GRHIShaderPlatform == SP_PCD3D_SM3)
				{
					for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
					{
						const FViewInfo& View = Views(ViewIndex);
						// Set the viewport for each view before clearing
						// This works around a bug with a distortion material and dynamic shadows visible in a scene with a dominant light and show bounds enabled,
						// Where this clear gets ignored unless the viewport is set first, possibly the result of a driver bug.
						RHISetViewport(View.RenderTargetX, View.RenderTargetY, 0.0f, View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY, 1.0f);
						RHIClear(TRUE, FLinearColor::White, FALSE, 0, FALSE, 0);
					}
				}
				else
				{
					RHIClear(TRUE, FLinearColor::White, FALSE, 0, FALSE, 0);
				}
				
				if (bDrawShadows && bDrawNormalShadows)
				{
					// Render non-modulated projected shadows to the attenuation buffer.
					RenderProjectedShadows( LightSceneInfo, DPGIndex, TRUE );
				}
			}
	
			if (bUseAttenuationBuffer)
			{
				// Render light function to the attenuation buffer.
				RenderLightFunction( LightSceneInfo, DPGIndex, FALSE );
				
				// Resolve light attenuation buffer
				GSceneRenderTargets.FinishRenderingLightAttenuation();

#if !DISABLE_TRANSLUCENCY_DOMINANT_LIGHT_ATTENUATION
				if (bHasInheritDominantShadowMaterials 
					&& !GOnePassDominantLight 
					&& LightSceneInfo->LightType == LightType_DominantDirectional
					&& DPGIndex == SDPG_World)
				{
					SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("CopyToTranslucencyAttenTex"));
					// Resolve the dominant directional light's screenspace shadow factors to the TranslucencyDominantLightAttenuationTexture, so it can be used later on translucency
					RHICopyToResolveTarget(GSceneRenderTargets.GetLightAttenuationSurface(), FALSE, FResolveParams(FResolveRect(-1, -1, -1, -1), CubeFace_PosX, GSceneRenderTargets.GetTranslucencyDominantLightAttenuationTexture()));
					GSceneRenderTargets.bResolvedTranslucencyDominantLightAttenuationTexture = TRUE;
				}
#endif
			}

			GSceneRenderTargets.SetLightAttenuationMode(bUseAttenuationBuffer);

			// Rendering the light pass to the scene color buffer.

			if( bSavedRawSceneColor )
			{
				GSceneRenderTargets.RestoreSceneColorRaw();
				GSceneRenderTargets.BeginRenderingSceneColor(RTUsage_Default, FALSE, TRUE);
			}
			// Only bind the scene color render target if it is not already bound
			// This saves GPU time (strangely enough) in DX11 with lots of deferred lights
			else if (!bSceneColorBound)
			{
				GSceneRenderTargets.BeginRenderingSceneColor(bUseAttenuationBuffer ? RTUsage_RestoreSurface : RTUsage_Default, FALSE, TRUE);
			}
			bSceneColorBound = TRUE;

			// Render the light to the scene color buffer, conditionally using the attenuation buffer or a 1x1 white texture as input 
			bSceneColorDirty |= RenderLight( LightSceneInfo, DPGIndex );
		}
	}

	if (bSceneColorBound)
	{
		// Do not resolve to scene color texture, this is done lazily
		GSceneRenderTargets.FinishRenderingSceneColor(FALSE);
	}

	// Restore the default mode
	GSceneRenderTargets.SetLightAttenuationMode(TRUE);

	// The whole scene dominant shadow projection is no longer valid
	GSceneRenderTargets.SetWholeSceneDominantShadowValid(FALSE);

	return bSceneColorDirty;
}

/**
 * Used by RenderLights to render a light to the scene color buffer.
 *
 * @param LightSceneInfo Represents the current light
 * @param LightIndex The light's index into FScene::Lights
 * @return TRUE if anything got rendered
 */
UBOOL FSceneRenderer::RenderLight( const FLightSceneInfo* LightSceneInfo, UINT DPGIndex )
{
	UBOOL bDirty = FALSE;

	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_LightingDrawTime, !bIsSceneCapture);

	if (ShouldUseDeferredShading() && LightSceneInfo->SupportsDeferredShading() && DPGIndex == SDPG_World)
	{
		// Render the light's influence in a deferred pass on meshes that support it
		bDirty |= RenderLightDeferred(LightSceneInfo, DPGIndex);
	}

	for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		const FViewInfo& View = Views(ViewIndex);
		const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos(LightSceneInfo->Id);

		if( ShouldRenderLight(VisibleLightViewInfo, DPGIndex) )
		{
			SCOPED_CONDITIONAL_DRAW_EVENT(EventRenderLight,ViewIndex == 0)(DEC_SCENE_ITEMS,TEXT("Light"));
			SCOPED_CONDITIONAL_DRAW_EVENT(EventView,Views.Num() > 1)(DEC_SCENE_ITEMS,TEXT("View%d"),ViewIndex);
			// Set the device viewport for the view.
			RHISetViewport(View.RenderTargetX,View.RenderTargetY,0.0f,View.RenderTargetX + View.RenderTargetSizeX,View.RenderTargetY + View.RenderTargetSizeY,1.0f);
			RHISetViewParameters(View);
			RHISetMobileHeightFogParams(View.HeightFogParams);

			// The scissor rect calculation uses the last column of the projection matrix, 
			// Which is manipulated by scene capture reflect actors to handle clipping, so the scissor does not work correctly.
			if (!bIsSceneCapture)
			{
				// Set the light's scissor rectangle.
				LightSceneInfo->SetScissorRect(&View);
				// Set the light's depth bounds
				LightSceneInfo->SetDepthBounds(&View);
			}
			
			// Additive blending, depth tests, no depth writes, preserve destination alpha.
			RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());
			RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());

			// Draw the light's effect on static meshes.
			bDirty |= LightSceneInfo->GetDPGInfo(DPGIndex)->DrawStaticMeshesVisible(View,View.StaticMeshVisibilityMap,FLightSceneDPGInfoInterface::ELightPass_Default);

			// Draw the static batched decals affected by the light
			{
				SCOPE_CYCLE_COUNTER(STAT_DecalRenderLitTime);

				UBOOL bAnyDecalsRendered =
					LightSceneInfo->GetDPGInfo(DPGIndex)->DrawStaticMeshesVisible(
						View,View.DecalStaticMeshVisibilityMap,FLightSceneDPGInfoInterface::ELightPass_Decals);

				// If we rendered any decals, then its likely that the scissor rect was changed by the
				// decal drawing code (FMeshDrawingPolicy::SetMeshRenderState), so we'll reset the light's
				// scissor rect
				if( bAnyDecalsRendered && !bIsSceneCapture )
				{
					LightSceneInfo->SetScissorRect( &View );
				}

				bDirty |= bAnyDecalsRendered;
			}

			{
				// Draw the light's effect on dynamic meshes.
				TDynamicPrimitiveDrawer<FMeshLightingDrawingPolicyFactory> Drawer(&View,DPGIndex,LightSceneInfo,TRUE);
				{
					for(INT PrimitiveIndex = 0;PrimitiveIndex < VisibleLightViewInfo.DPGInfo[DPGIndex].VisibleDynamicLitPrimitives.Num();PrimitiveIndex++)
					{
						const FPrimitiveSceneInfo* PrimitiveSceneInfo = VisibleLightViewInfo.DPGInfo[DPGIndex].VisibleDynamicLitPrimitives(PrimitiveIndex);
						if(View.PrimitiveVisibilityMap(PrimitiveSceneInfo->Id))
						{
							Drawer.SetPrimitive(PrimitiveSceneInfo);
							PrimitiveSceneInfo->Proxy->DrawDynamicElements(&Drawer,&View,DPGIndex);
						}
					}
					bDirty |= Drawer.IsDirty();
				}

				// Render decals for visible primitives affected by this light.
				{
					Drawer.ClearDirtyFlag();
					for(INT PrimitiveIndex = 0;PrimitiveIndex < VisibleLightViewInfo.DPGInfo[DPGIndex].VisibleLitDecalPrimitives.Num();PrimitiveIndex++)
					{
						const FPrimitiveSceneInfo* PrimitiveSceneInfo = VisibleLightViewInfo.DPGInfo[DPGIndex].VisibleLitDecalPrimitives(PrimitiveIndex);
						const FPrimitiveViewRelevance& PrimitiveViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);
						if(View.PrimitiveVisibilityMap(PrimitiveSceneInfo->Id))
						{
							SCOPE_CYCLE_COUNTER(STAT_DecalRenderLitTime);

							UBOOL bDrawOpaqueDecals;
							UBOOL bDrawTransparentDecals;
							GetDrawDecalFilters (PrimitiveViewRelevance.bOpaqueRelevance, PrimitiveViewRelevance.bTranslucentRelevance, FALSE, bDrawOpaqueDecals, bDrawTransparentDecals);

							Drawer.SetPrimitive(PrimitiveSceneInfo);
							PrimitiveSceneInfo->Proxy->DrawDynamicDecalElements(&Drawer,&View,DPGIndex,TRUE,bDrawOpaqueDecals,bDrawTransparentDecals,FALSE);
						}
					}

					// If we rendered any decals, then its likely that the scissor rect was changed by the
					// decal drawing code (FMeshDrawingPolicy::SetMeshRenderState), so we'll reset the light's
					// scissor rect
					if( Drawer.IsDirty() && !bIsSceneCapture )
					{
						LightSceneInfo->SetScissorRect( &View );
					}

					bDirty |= Drawer.IsDirty();
				}
			}

			// Reset the scissor rectangle and stencil state.
			RHISetScissorRect(FALSE,0,0,0,0);
			RHISetDepthBoundsTest( FALSE, FVector4(0.0f,0.0f,0.0f,1.0f), FVector4(0.0f,0.0f,1.0f,1.0f));
		}
	}
	return bDirty;
}

/** Renders the light's influence on the scene in a deferred pass. */
UBOOL FSceneRenderer::RenderLightDeferred(const FLightSceneInfo* LightSceneInfo, UINT DPGIndex)
{
	// Use additive blending for color, and keep the destination alpha.
	RHISetBlendState(TStaticBlendState<BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI());
	
	UBOOL bStencilDirty = FALSE;
	const FSphere LightBounds = LightSceneInfo->GetBoundingSphere();

	for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views(ViewIndex);

		const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos(LightSceneInfo->Id);

		if (ShouldRenderLight(VisibleLightViewInfo, DPGIndex))
		{
			SCOPED_CONDITIONAL_DRAW_EVENT(EventRenderLight,ViewIndex == 0)(DEC_SCENE_ITEMS,TEXT("DeferredLight"));

			// Set the device viewport for the view.
			RHISetViewport(View.RenderTargetX, View.RenderTargetY, 0.0f, View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY, 1.0f);
			RHISetViewParameters(View);
			RHISetMobileHeightFogParams(View.HeightFogParams);

			if (LightSceneInfo->LightType == LightType_Directional || LightSceneInfo->LightType == LightType_DominantDirectional)
			{
				TShaderMapRef<TDeferredLightVertexShader<FALSE> > VertexShader(GetGlobalShaderMap());
				VertexShader->SetParameters(View);

				RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
				RHISetDepthState(TStaticDepthState<FALSE, CF_Always>::GetRHI());

				if (GSystemSettings.UsesMSAA())
				{
					SCOPED_DRAW_EVENT(EventRenderPerSample)(DEC_SCENE_ITEMS,TEXT("PerSample Light"));

					// Set stencil to one.
					RHISetStencilState(TStaticStencilState<
						TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,
						FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
						0xff,0xff,1
						>::GetRHI());

					TShaderMapRef<TDeferredLightPixelShader<TRUE, FALSE, FALSE> > PixelShader(GetGlobalShaderMap());
					PixelShader->SetParameters(View, LightSceneInfo);

					SetGlobalBoundShaderState(
						DeferredLightFirstPassBoundState[0],
						GFilterVertexDeclaration.VertexDeclarationRHI,
						*VertexShader,
						*PixelShader,
						sizeof(FFilterVertex)
						);

					// Render lighting for all pixels not on a geometry edge, and marking them as processed with a stencil value of 1
					// Pixels determined to be on a geometry edge will have a stencil value of 0 after this pass
					DrawDenormalizedQuad( 
						View.RenderTargetX, View.RenderTargetY, 
						View.RenderTargetSizeX, View.RenderTargetSizeY,
						View.RenderTargetX, View.RenderTargetY, 
						View.RenderTargetSizeX, View.RenderTargetSizeY,
						View.RenderTargetSizeX, View.RenderTargetSizeY,
						GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());

					// Pass if 0
					RHISetStencilState(TStaticStencilState<
						TRUE,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
						FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
						0xff,0,0
						>::GetRHI());

					bStencilDirty = TRUE;

					TShaderMapRef<TDeferredLightPerSamplePixelShader<FALSE, FALSE> > PerSamplePixelShader(GetGlobalShaderMap());
					PerSamplePixelShader->SetParameters(View, LightSceneInfo);

					// Render lighting for all pixels on a geometry edge, calculating the lighting per MSAA sample
					SetGlobalBoundShaderState(
						DeferredLightSecondPassBoundState[0],
						GFilterVertexDeclaration.VertexDeclarationRHI,
						*VertexShader,
						*PerSamplePixelShader,
						sizeof(FFilterVertex)
						);
				}
				else
				{
					TShaderMapRef<TDeferredLightPixelShader<FALSE, FALSE, FALSE> > PixelShader(GetGlobalShaderMap());
					PixelShader->SetParameters(View, LightSceneInfo);

					SetGlobalBoundShaderState(
						DeferredLightNoMSAABoundState[0],
						GFilterVertexDeclaration.VertexDeclarationRHI,
						*VertexShader,
						*PixelShader,
						sizeof(FFilterVertex)
						);
				}

				// Apply the directional light as a full screen quad
				DrawDenormalizedQuad( 
					View.RenderTargetX, View.RenderTargetY, 
					View.RenderTargetSizeX, View.RenderTargetSizeY,
					View.RenderTargetX, View.RenderTargetY, 
					View.RenderTargetSizeX, View.RenderTargetSizeY,
					View.RenderTargetSizeX, View.RenderTargetSizeY,
					GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());
			}
			else
			{
				TShaderMapRef<TDeferredLightVertexShader<TRUE> > VertexShader(GetGlobalShaderMap());
				VertexShader->SetParameters(View);

				const UBOOL bPointLight = LightSceneInfo->LightType == LightType_Point || LightSceneInfo->LightType == LightType_DominantPoint;
				const UBOOL bSpotLight = LightSceneInfo->LightType == LightType_Spot || LightSceneInfo->LightType == LightType_DominantSpot;
				const UBOOL bReverseCulling = (View.bReverseCulling ^ (bSpotLight && !IsNegativeFloat(LightSceneInfo->LightToWorld.Determinant()))) ? TRUE : FALSE;
				const UBOOL bCameraInsideLightGeometry = ((FVector)View.ViewOrigin - LightBounds.Center).SizeSquared() < Square(LightBounds.W * 1.05f + View.NearClippingDistance * 2.0f);
				if (bCameraInsideLightGeometry)
				{
					RHISetDepthState(TStaticDepthState<FALSE, CF_Always>::GetRHI());
					// Render backfaces with depth tests disabled since the camera is inside (or close to inside) the light geometry
					RHISetRasterizerState(bReverseCulling ? TStaticRasterizerState<FM_Solid,CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid,CM_CCW>::GetRHI());
				}
				else
				{
					// Render frontfaces with depth tests on to get the speedup from HiZ since the camera is outside the light geometry
					RHISetDepthState(TStaticDepthState<FALSE, CF_LessEqual>::GetRHI());
					RHISetRasterizerState(bReverseCulling ? TStaticRasterizerState<FM_Solid,CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid,CM_CW>::GetRHI());
				}

				if (GSystemSettings.UsesMSAA())
				{
					// Set stencil to one.
					RHISetStencilState(TStaticStencilState<
						TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,
						FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
						0xff,0xff,1
						>::GetRHI());

					{
						SCOPED_DRAW_EVENT(EventRenderPerSample)(DEC_SCENE_ITEMS,TEXT("PerSample Light"));

						if (bPointLight)
						{
							TShaderMapRef<TDeferredLightPixelShader<TRUE, TRUE, FALSE> > PixelShader(GetGlobalShaderMap());
							PixelShader->SetParameters(View, LightSceneInfo);

							SetGlobalBoundShaderState(
								DeferredLightFirstPassBoundState[1],
								GShadowFrustumVertexDeclaration.VertexDeclarationRHI,
								*VertexShader,
								*PixelShader,
								sizeof(FVector)
								);

							// Render lighting for all pixels not on a geometry edge, and marking them as processed with a stencil value of 1
							// Pixels determined to be on a geometry edge will have a stencil value of 0 after this pass
							DrawStencilingSphere(LightBounds, View.PreViewTranslation);
						}
						else if (bSpotLight)
						{
							TShaderMapRef<TDeferredLightPixelShader<TRUE, TRUE, TRUE> > PixelShader(GetGlobalShaderMap());
							PixelShader->SetParameters(View, LightSceneInfo);

							SetGlobalBoundShaderState(
								DeferredLightFirstPassBoundState[2],
								GShadowFrustumVertexDeclaration.VertexDeclarationRHI,
								*VertexShader,
								*PixelShader,
								sizeof(FVector)
								);

							DrawStencilingCone(
								LightSceneInfo->LightToWorld, 
								LightSceneInfo->GetOuterConeAngle(), 
								LightSceneInfo->GetRadius(),
								View.PreViewTranslation);
						}
					}

					bStencilDirty = TRUE;

					// Pass if 0
					RHISetStencilState(TStaticStencilState<
						TRUE,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
						FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
						0xff,0,0
						>::GetRHI());

					if (bPointLight)
					{
						TShaderMapRef<TDeferredLightPerSamplePixelShader<TRUE, FALSE> > PixelShader(GetGlobalShaderMap());
						PixelShader->SetParameters(View, LightSceneInfo);

						SetGlobalBoundShaderState(
							DeferredLightSecondPassBoundState[1],
							GShadowFrustumVertexDeclaration.VertexDeclarationRHI,
							*VertexShader,
							*PixelShader,
							sizeof(FVector)
							);

						// Apply the point or spot light with some approximately bounding geometry, 
						// So we can get speedups from depth testing and not processing pixels outside of the light's influence.
						DrawStencilingSphere(LightBounds, View.PreViewTranslation);
					}
					else if (bSpotLight)
					{
						TShaderMapRef<TDeferredLightPerSamplePixelShader<TRUE, TRUE> > PixelShader(GetGlobalShaderMap());
						PixelShader->SetParameters(View, LightSceneInfo);

						SetGlobalBoundShaderState(
							DeferredLightSecondPassBoundState[2],
							GShadowFrustumVertexDeclaration.VertexDeclarationRHI,
							*VertexShader,
							*PixelShader,
							sizeof(FVector)
							);

						DrawStencilingCone(
							LightSceneInfo->LightToWorld, 
							LightSceneInfo->GetOuterConeAngle(), 
							LightSceneInfo->GetRadius(),
							View.PreViewTranslation);
					}
				}
				else
				{
					if (LightSceneInfo->LightType == LightType_Point || LightSceneInfo->LightType == LightType_DominantPoint)
					{
						TShaderMapRef<TDeferredLightPixelShader<FALSE, TRUE, FALSE> > PixelShader(GetGlobalShaderMap());
						PixelShader->SetParameters(View, LightSceneInfo);

						SetGlobalBoundShaderState(
							DeferredLightNoMSAABoundState[1],
							GShadowFrustumVertexDeclaration.VertexDeclarationRHI,
							*VertexShader,
							*PixelShader,
							sizeof(FVector)
							);

						// Apply the point or spot light with some approximately bounding geometry, 
						// So we can get speedups from depth testing and not processing pixels outside of the light's influence.
						DrawStencilingSphere(LightBounds, View.PreViewTranslation);
					}
					else if (LightSceneInfo->LightType == LightType_Spot || LightSceneInfo->LightType == LightType_DominantSpot)
					{
						TShaderMapRef<TDeferredLightPixelShader<FALSE, TRUE, TRUE> > PixelShader(GetGlobalShaderMap());
						PixelShader->SetParameters(View, LightSceneInfo);

						SetGlobalBoundShaderState(
							DeferredLightNoMSAABoundState[2],
							GShadowFrustumVertexDeclaration.VertexDeclarationRHI,
							*VertexShader,
							*PixelShader,
							sizeof(FVector)
							);

						DrawStencilingCone(
							LightSceneInfo->LightToWorld, 
							LightSceneInfo->GetOuterConeAngle(), 
							LightSceneInfo->GetRadius(),
							View.PreViewTranslation);
					}
				}
			}
		}
	}

	if (bStencilDirty)
	{
		// Clear the stencil buffer to 0.
		RHIClear(FALSE,FColor(0,0,0),FALSE,0,TRUE,0);
	}

	// Reset stencil state to defaults
	RHISetStencilState(TStaticStencilState<>::GetRHI());

	return TRUE;
}

UBOOL FSceneRenderer::RenderModulatedShadows(UINT DPGIndex)
{
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_RenderModulatedShadowsTime, !bIsSceneCapture);
	UBOOL bSceneColorDirty = FALSE;
	SCOPED_DRAW_EVENT(EventRenderModShadow)(DEC_SCENE_ITEMS,TEXT("ModShadow"));

	// Render modulated projected shadows to scene color buffer.
	GSceneRenderTargets.BeginRenderingSceneColor(RTUsage_Default);
	
	// Draw each light.
	for(INT LightIndex=0; LightIndex<VisibleShadowCastingLightInfos.Num(); LightIndex++ )
	{
		const FLightSceneInfo* LightSceneInfo = VisibleShadowCastingLightInfos(LightIndex);
		
		// Only look at lights using modulated shadows.
		const UBOOL bHasModulatedShadows =
			LightSceneInfo->LightShadowMode == LightShadow_Modulate;
		if(LightSceneInfo->LightType != LightType_Sky && bHasModulatedShadows 
			// Skip the modulated shadow completely if the self shadowing has already been rendered and only self shadowing is allowed
			&& !(LightSceneInfo->bNonModulatedSelfShadowing && LightSceneInfo->bSelfShadowOnly))
		{
			bSceneColorDirty |= RenderProjectedShadows( LightSceneInfo, DPGIndex, FALSE );
		}
	}	

	GSceneRenderTargets.FinishRenderingSceneColor(FALSE);
	return bSceneColorDirty;
}

#if WITH_MOBILE_RHI
/**
 * Prepares and renders all the modulated shadows to the scene color buffer on mobile.
 * @param	DPGIndex					Current DPG used to draw items.
 * @return TRUE if anything got rendered
 */
UBOOL FSceneRenderer::PrepareMobileModulatedShadows(UINT DPGIndex)
{
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_RenderModulatedShadowsTime, !bIsSceneCapture);
	SCOPED_DRAW_EVENT(EventRenderModShadow)(DEC_SCENE_ITEMS,TEXT("PrepareMobileModShadow"));

	UBOOL bSceneColorDirty = FALSE;
	MobileProjectedShadows.Reset();

	// Gather shadows for each light
	for( INT LightIndex=0; LightIndex < VisibleShadowCastingLightInfos.Num(); LightIndex++ )
	{
		const FLightSceneInfo* LightSceneInfo = VisibleShadowCastingLightInfos(LightIndex);

		// Only look at non-sky lights using modulated shadows
		if( LightSceneInfo->LightShadowMode == LightShadow_Modulate &&
			LightSceneInfo->LightType != LightType_Sky &&
			// And skip the modulated shadow completely if the self shadowing has already been rendered and only self shadowing is allowed
			!(LightSceneInfo->bNonModulatedSelfShadowing && LightSceneInfo->bSelfShadowOnly))
		{
			bSceneColorDirty |= GatherMobileProjectedShadows( DPGIndex, LightSceneInfo );
		}
	}

	if( MobileProjectedShadows.Num() )
	{
		GSceneRenderTargets.BeginRenderingShadowDepth(FALSE);
		{
			// Clear the entire shadow buffer to hint a "discard" of the rendertarget (avoid restore).
			RHIClear(TRUE,FColor(255,255,255),TRUE,1.0f,TRUE,0);

			extern UBOOL GMobileRenderingShadowDepth;
			GMobileRenderingShadowDepth = TRUE;

			bSceneColorDirty |= RenderMobileProjectedShadows( DPGIndex );

			GMobileRenderingShadowDepth = FALSE;
		}
		GSceneRenderTargets.FinishRenderingShadowDepth(FALSE, FResolveRect());
	}

	return bSceneColorDirty;
}
UBOOL FSceneRenderer::ApplyMobileModulatedShadows(UINT DPGIndex)
{
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_RenderModulatedShadowsTime, !bIsSceneCapture);
	SCOPED_DRAW_EVENT(EventRenderModShadow)(DEC_SCENE_ITEMS,TEXT("ApplyMobileModShadow"));

	return ApplyMobileProjectedShadows(DPGIndex);
}
#endif

