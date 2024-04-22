/*=============================================================================
	DecalRendering.cpp: High-level decal rendering implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"

/**
* Renders the scene's decals
*
* @param DPGIndex - current depth priority group index
* @param bTranslucentPass - if TRUE render translucent decals on opqaue receivers
*							if FALSE render opqaue decals on opaque/translucent receivers
* @return TRUE if anything was rendered to scene color
*/
UBOOL FSceneRenderer::RenderDecals(const FViewInfo& View, UINT DPGIndex, UBOOL bTranslucentPass)
{
	SCOPED_DRAW_EVENT(EventDecals)(DEC_SCENE_ITEMS,TEXT("Decals%s"),bTranslucentPass ? TEXT("Translucent") : TEXT("Opaque"));
	SCOPE_CYCLE_COUNTER(STAT_DecalRenderUnlitTime);	

	UBOOL bDirty = FALSE;

	if( bTranslucentPass )
	{
		// no depth writes for translucent decals
		RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());
	}
	else
	{
		// depth writes enabled for opaque decals
		RHISetDepthState(TStaticDepthState<TRUE,CF_LessEqual>::GetRHI());
	}

	// only render translucent decals on opaque receivers or opaque decals on opaque/translucent receivers
	const FDepthPriorityGroup::EBasePassDrawListType DrawType = 
		bTranslucentPass ? FDepthPriorityGroup::EBasePass_Decals_Translucent : FDepthPriorityGroup::EBasePass_Decals;

	// draw the static draw lists
	bDirty |= Scene->DPGs[DPGIndex].BasePassNoLightMapDrawList[DrawType].DrawVisible(View,View.DecalStaticMeshVisibilityMap);
	bDirty |= Scene->DPGs[DPGIndex].BasePassDirectionalVertexLightMapDrawList[DrawType].DrawVisible(View,View.DecalStaticMeshVisibilityMap);
	bDirty |= Scene->DPGs[DPGIndex].BasePassSimpleVertexLightMapDrawList[DrawType].DrawVisible(View,View.DecalStaticMeshVisibilityMap);
	bDirty |= Scene->DPGs[DPGIndex].BasePassShadowedDynamicLightDirectionalVertexLightMapDrawList[DrawType].DrawVisible(View,View.DecalStaticMeshVisibilityMap);
	bDirty |= Scene->DPGs[DPGIndex].BasePassDirectionalLightMapTextureDrawList[DrawType].DrawVisible(View,View.DecalStaticMeshVisibilityMap);
	bDirty |= Scene->DPGs[DPGIndex].BasePassSimpleLightMapTextureDrawList[DrawType].DrawVisible(View,View.DecalStaticMeshVisibilityMap);
	bDirty |= Scene->DPGs[DPGIndex].BasePassShadowedDynamicLightDirectionalLightMapTextureDrawList[DrawType].DrawVisible(View,View.DecalStaticMeshVisibilityMap);
	bDirty |= Scene->DPGs[DPGIndex].BasePassDistanceFieldShadowedDynamicLightDirectionalLightMapTextureDrawList[DrawType].DrawVisible(View,View.DecalStaticMeshVisibilityMap);
	bDirty |= Scene->DPGs[DPGIndex].BasePassDynamicallyShadowedDynamicLightDrawList[DrawType].DrawVisible(View,View.DecalStaticMeshVisibilityMap);
	bDirty |= Scene->DPGs[DPGIndex].BasePassSHLightAndDynamicLightDrawList[DrawType].DrawVisible(View,View.DecalStaticMeshVisibilityMap);

	if( bTranslucentPass)
	{
		const TArray<const FPrimitiveSceneInfo*,SceneRenderingAllocator>& DPGVisibleDynamicDecalPrimitives = View.VisibleTranslucentDynamicDecalPrimitives[DPGIndex];
		if (DPGVisibleDynamicDecalPrimitives.Num())
		{
			UBOOL bRenderingToLowResTranslucencyBuffer = FALSE;
			UBOOL bAllowDownsampling = FALSE;
			UBOOL bRenderingToDoFBlurBuffer = FALSE;
			const FProjectedShadowInfo* TranslucentPreShadowInfo = NULL;
			// Draw dynamic translucent decals for non-occluded primitives using a translucent drawing policy.
			TDynamicPrimitiveDrawer<FTranslucencyDrawingPolicyFactory> TranslucentDrawer(
				&View,DPGIndex,FTranslucencyDrawingPolicyFactory::ContextType(bRenderingToLowResTranslucencyBuffer,bAllowDownsampling,bRenderingToDoFBlurBuffer,TranslucentPreShadowInfo),TRUE);

			for(INT PrimitiveIndex = 0;PrimitiveIndex < DPGVisibleDynamicDecalPrimitives.Num();PrimitiveIndex++)
			{
				const FPrimitiveSceneInfo* PrimitiveSceneInfo = DPGVisibleDynamicDecalPrimitives(PrimitiveIndex);
				const FPrimitiveViewRelevance& PrimitiveViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);

				const UBOOL bVisible = View.PrimitiveVisibilityMap(PrimitiveSceneInfo->Id);
				const UBOOL bRelevantDPG = PrimitiveViewRelevance.GetDPG(DPGIndex) != 0;

				// Only draw decals if the primitive is visible and relevant in the current DPG.				
				if( bVisible && bRelevantDPG)
				{
					UBOOL bDrawOpaqueDecals;
					UBOOL bDrawTransparentDecals;
					GetDrawDecalFilters (PrimitiveViewRelevance.bOpaqueRelevance, PrimitiveViewRelevance.bTranslucentRelevance, FALSE, bDrawOpaqueDecals, bDrawTransparentDecals);

					TranslucentDrawer.SetPrimitive(PrimitiveSceneInfo);
					PrimitiveSceneInfo->Proxy->DrawDynamicDecalElements(
						&TranslucentDrawer,
						&View,
						DPGIndex,
						FALSE,
						bDrawOpaqueDecals,
						bDrawTransparentDecals,
						FALSE
						);
				}
			}

			bDirty |= TranslucentDrawer.IsDirty(); 
		}
	}
	else
	{
		const TArray<const FPrimitiveSceneInfo*,SceneRenderingAllocator>& DPGVisibleDynamicDecalPrimitives = View.VisibleOpaqueDynamicDecalPrimitives[DPGIndex];
		if (DPGVisibleDynamicDecalPrimitives.Num())
		{
			// Draw dynamic decals for non-occluded primitives using a base pass drawing policy.
			TDynamicPrimitiveDrawer<FBasePassOpaqueDrawingPolicyFactory> OpaqueDrawer(
				&View,DPGIndex,FBasePassOpaqueDrawingPolicyFactory::ContextType(),TRUE);

			for(INT PrimitiveIndex = 0;PrimitiveIndex < DPGVisibleDynamicDecalPrimitives.Num();PrimitiveIndex++)
			{
				const FPrimitiveSceneInfo* PrimitiveSceneInfo = DPGVisibleDynamicDecalPrimitives(PrimitiveIndex);
				const FPrimitiveViewRelevance& PrimitiveViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);

				const UBOOL bVisible = View.PrimitiveVisibilityMap(PrimitiveSceneInfo->Id);
				const UBOOL bRelevantDPG = PrimitiveViewRelevance.GetDPG(DPGIndex) != 0;

				// Only draw decals if the primitive is visible and relevant in the current DPG.
				if(bVisible && bRelevantDPG)
				{
					UBOOL bDrawOpaqueDecals;
					UBOOL bDrawTransparentDecals;
					GetDrawDecalFilters (PrimitiveViewRelevance.bOpaqueRelevance, PrimitiveViewRelevance.bTranslucentRelevance, FALSE, bDrawOpaqueDecals, bDrawTransparentDecals);

					OpaqueDrawer.SetPrimitive(PrimitiveSceneInfo);
					PrimitiveSceneInfo->Proxy->DrawDynamicDecalElements(
						&OpaqueDrawer,
						&View,
						DPGIndex,
						FALSE,
						bDrawOpaqueDecals,
						bDrawTransparentDecals,
						FALSE
						);
				}
			}

			bDirty |= OpaqueDrawer.IsDirty(); 
		}
	}

	if( bTranslucentPass )
	{
		// Restore opaque blending, depth tests and writes.
		RHISetBlendState(TStaticBlendState<>::GetRHI());
		RHISetDepthState(TStaticDepthState<TRUE,CF_LessEqual>::GetRHI());
	}

	return bDirty;
}

