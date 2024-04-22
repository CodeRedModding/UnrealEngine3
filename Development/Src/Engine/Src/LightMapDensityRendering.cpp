/*=============================================================================
	LightMapDensityRendering.cpp: Implementation for rendering lightmap density.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "ScenePrivate.h"

//-----------------------------------------------------------------------------

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityVertexShader<FNoLightMapPolicy>,TEXT("LightMapDensityShader"),TEXT("MainVertexShader"),SF_Vertex,0,0); 
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityVertexShader<FDirectionalLightMapTexturePolicy>,TEXT("LightMapDensityShader"),TEXT("MainVertexShader"),SF_Vertex,0,0); 
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityVertexShader<FSimpleLightMapTexturePolicy>,TEXT("LightMapDensityShader"),TEXT("MainVertexShader"),SF_Vertex,0,0); 
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityVertexShader<FDummyLightMapTexturePolicy>,TEXT("LightMapDensityShader"),TEXT("MainVertexShader"),SF_Vertex,0,0); 

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityPixelShader<FNoLightMapPolicy>,TEXT("LightMapDensityShader"),TEXT("MainPixelShader"),SF_Pixel,VER_LIGHTMAP_DENSITY_SELECTED_OBJECT,0);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityPixelShader<FDirectionalLightMapTexturePolicy>,TEXT("LightMapDensityShader"),TEXT("MainPixelShader"),SF_Pixel,VER_LIGHTMAP_DENSITY_SELECTED_OBJECT,0);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityPixelShader<FSimpleLightMapTexturePolicy>,TEXT("LightMapDensityShader"),TEXT("MainPixelShader"),SF_Pixel,VER_LIGHTMAP_DENSITY_SELECTED_OBJECT,0);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityPixelShader<FDummyLightMapTexturePolicy>,TEXT("LightMapDensityShader"),TEXT("MainPixelShader"),SF_Pixel,VER_LIGHTMAP_DENSITY_SELECTED_OBJECT,0);

#if WITH_D3D11_TESSELLATION
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityHullShader<FNoLightMapPolicy>,TEXT("LightMapDensityShader"),TEXT("MainHull"),SF_Hull,0,0);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityDomainShader<FNoLightMapPolicy>,TEXT("LightMapDensityShader"),TEXT("MainDomain"),SF_Domain,0,0);

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityHullShader<FDirectionalLightMapTexturePolicy>,TEXT("LightMapDensityShader"),TEXT("MainHull"),SF_Hull,0,0);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityDomainShader<FDirectionalLightMapTexturePolicy>,TEXT("LightMapDensityShader"),TEXT("MainDomain"),SF_Domain,0,0);

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityHullShader<FSimpleLightMapTexturePolicy>,TEXT("LightMapDensityShader"),TEXT("MainHull"),SF_Hull,0,0);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityDomainShader<FSimpleLightMapTexturePolicy>,TEXT("LightMapDensityShader"),TEXT("MainDomain"),SF_Domain,0,0);

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityHullShader<FDummyLightMapTexturePolicy>,TEXT("LightMapDensityShader"),TEXT("MainHull"),SF_Hull,0,0);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityDomainShader<FDummyLightMapTexturePolicy>,TEXT("LightMapDensityShader"),TEXT("MainDomain"),SF_Domain,0,0);
#endif


UBOOL FSceneRenderer::RenderLightMapDensities(UINT DPGIndex)
{
	SCOPED_DRAW_EVENT(EventLightMapDensity)(DEC_SCENE_ITEMS,TEXT("LightMapDensity"));

	UBOOL bWorldDpg = (DPGIndex == SDPG_World);
	UBOOL bDirty=0;

	// Draw the scene's emissive and light-map color.
	for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENT(EventView,Views.Num() > 1)(DEC_SCENE_ITEMS,TEXT("View%d"),ViewIndex);

		FViewInfo& View = Views(ViewIndex);

		// Opaque blending, depth tests and writes.
		RHISetBlendState(TStaticBlendState<>::GetRHI());
		RHISetDepthState(TStaticDepthState<TRUE,CF_LessEqual>::GetRHI());
 
		// Set the device viewport for the view.
		RHISetViewport(View.RenderTargetX, View.RenderTargetY, 0.0f, View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY, 1.0f);
		RHISetViewParameters(View);
		RHISetMobileHeightFogParams(View.HeightFogParams);

		{
			SCOPED_DRAW_EVENT(EventDynamic)(DEC_SCENE_ITEMS,TEXT("Dynamic"));

			if( View.VisibleDynamicPrimitives.Num() > 0 )
			{
				// Draw the dynamic non-occluded primitives using a base pass drawing policy.
				TDynamicPrimitiveDrawer<FLightMapDensityDrawingPolicyFactory> Drawer(
					&View,DPGIndex,FLightMapDensityDrawingPolicyFactory::ContextType(),TRUE);
				for (INT PrimitiveIndex = 0;PrimitiveIndex < View.VisibleDynamicPrimitives.Num();PrimitiveIndex++)
				{
					const FPrimitiveSceneInfo* PrimitiveSceneInfo = View.VisibleDynamicPrimitives(PrimitiveIndex);
					const FPrimitiveViewRelevance& PrimitiveViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);

					const UBOOL bVisible = View.PrimitiveVisibilityMap(PrimitiveSceneInfo->Id);
					const UBOOL bRelevantDPG = PrimitiveViewRelevance.GetDPG(DPGIndex) != 0;

					// Only draw the primitive if it's visible and relevant in the current DPG
					if( bVisible && bRelevantDPG && 
						// only draw opaque and masked primitives if wireframe is disabled
						(PrimitiveViewRelevance.bOpaqueRelevance || ViewFamily.ShowFlags & SHOW_Wireframe) )
					{
						Drawer.SetPrimitive(PrimitiveSceneInfo);
						PrimitiveSceneInfo->Proxy->DrawDynamicElements(
							&Drawer,
							&View,
							DPGIndex,
							0					
							);
					}
				}
				bDirty |= Drawer.IsDirty(); 
			}
		}
	}

	// restore color write mask
	RHISetColorWriteMask(CW_RGBA);

	return bDirty;
}

UBOOL FLightMapDensityDrawingPolicyFactory::DrawDynamicMesh(
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

	const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterial();
	const EBlendMode BlendMode = Material->GetBlendMode();

	const UBOOL bMaterialMasked = Material->IsMasked();
	const UBOOL bMaterialModifiesMesh = Material->MaterialModifiesMeshPosition();
	if (!bMaterialMasked && !bMaterialModifiesMesh)
	{
		// Override with the default material for opaque materials that are not two sided
		MaterialRenderProxy = GEngine->LevelColorationLitMaterial->GetRenderProxy(FALSE);
	}

	const UBOOL bIsLitMaterial = (Material->GetLightingModel() != MLM_Unlit);
	/*const */FLightMapInteraction LightMapInteraction = (Mesh.LCI && bIsLitMaterial) ? Mesh.LCI->GetLightMapInteraction() : FLightMapInteraction();
	// force simple lightmaps based on system settings
	UBOOL bAllowDirectionalLightMaps = GSystemSettings.bAllowDirectionalLightMaps && LightMapInteraction.AllowsDirectionalLightmaps();

	if (bIsLitMaterial && PrimitiveSceneInfo && PrimitiveSceneInfo->Proxy && (PrimitiveSceneInfo->Proxy->GetLightMapType() == LMIT_Texture))
	{
		// Should this object be texture lightmapped? Ie, is lighting not built for it??
		UBOOL bUseDummyLightMapPolicy = FALSE;
		if ((Mesh.LCI == NULL) || (Mesh.LCI->GetLightMapInteraction().GetType() != LMIT_Texture))
		{
			bUseDummyLightMapPolicy = TRUE;

			LightMapInteraction.SetLightMapInteractionType(LMIT_Texture);
			LightMapInteraction.SetCoordinateScale(FVector2D(1.0f,1.0f));
			LightMapInteraction.SetCoordinateBias(FVector2D(0.0f,0.0f));
		}
		if (bUseDummyLightMapPolicy == FALSE)
		{
			if (bAllowDirectionalLightMaps)
			{
				TLightMapDensityDrawingPolicy<FDirectionalLightMapTexturePolicy> DrawingPolicy(Mesh.VertexFactory,MaterialRenderProxy,FDirectionalLightMapTexturePolicy(),BlendMode);
				DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
				for( INT BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
				{
					DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,
						TLightMapDensityDrawingPolicy<FDirectionalLightMapTexturePolicy>::ElementDataType(LightMapInteraction));
					DrawingPolicy.DrawMesh(Mesh, BatchElementIndex);
				}
				bDirty = TRUE;
			}
			else
			{
				TLightMapDensityDrawingPolicy<FSimpleLightMapTexturePolicy> DrawingPolicy(Mesh.VertexFactory,MaterialRenderProxy,FSimpleLightMapTexturePolicy (),BlendMode);
				DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
				for( INT BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
				{
					DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,
						TLightMapDensityDrawingPolicy<FSimpleLightMapTexturePolicy>::ElementDataType(LightMapInteraction));
					DrawingPolicy.DrawMesh(Mesh,BatchElementIndex);
				}
				bDirty = TRUE;
			}
		}
		else
		{
			TLightMapDensityDrawingPolicy<FDummyLightMapTexturePolicy> DrawingPolicy(Mesh.VertexFactory,MaterialRenderProxy,FDummyLightMapTexturePolicy(),BlendMode);
			DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
			for( INT BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
			{
				DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,
					TLightMapDensityDrawingPolicy<FDummyLightMapTexturePolicy>::ElementDataType(LightMapInteraction));
				DrawingPolicy.DrawMesh(Mesh,BatchElementIndex);
			}
			bDirty = TRUE;
		}
	}
	else
	{
		TLightMapDensityDrawingPolicy<FNoLightMapPolicy> DrawingPolicy(Mesh.VertexFactory,MaterialRenderProxy,FNoLightMapPolicy(),BlendMode);
	 	DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
		for( INT BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
		{
			DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,TLightMapDensityDrawingPolicy<FNoLightMapPolicy>::ElementDataType());
			DrawingPolicy.DrawMesh(Mesh,BatchElementIndex);
		}
		bDirty = TRUE;
	}
	
	return bDirty;
}
