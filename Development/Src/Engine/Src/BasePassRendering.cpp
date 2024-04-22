/*=============================================================================
	BasePassRendering.cpp: Base pass rendering implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"

/** Whether to use deferred shading, where the base pass outputs G buffer attributes, and lighting passes fetch these attributes and do shading based on them. */
UBOOL GAllowDeferredShading = TRUE;

/** Returns TRUE if the given material and primitive can be lit in a deferred pass. */
UBOOL MeshSupportsDeferredLighting(const FMaterial* Material, const FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	const UBOOL bSupportsDeferredLighting = 
		// Only Phong supported for now
		Material->GetLightingModel() == MLM_Phong
		// Deferred passes don't output the MRT values that subsurface scattering needs
		&& !Material->HasSubsurfaceScattering()
		// Only render deferred if any of the channels supported by the channel mask are set
		&& PrimitiveSceneInfo->LightingChannels.GetDeferredShadingChannelMask() > 0;

	return bSupportsDeferredLighting;
}

/** Whether to replace lightmap textures with solid colors to visualize the mip-levels. */
UBOOL GVisualizeMipLevels = FALSE;

#if WITH_D3D11_TESSELLATION
// Typedef is necessary because the C preprocessor thinks the comma in the template parameter list is a comma in the macro parameter list.
// BasePass Vertex Shader needs to include hull and domain shaders for tessellation, these only compile for D3D11
#define IMPLEMENT_BASEPASS_VERTEXSHADER_TYPE(LightMapPolicyType,FogDensityPolicyType) \
	typedef TBasePassVertexShader<LightMapPolicyType,FogDensityPolicyType> TBasePassVertexShader##LightMapPolicyType##FogDensityPolicyType; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassVertexShader##LightMapPolicyType##FogDensityPolicyType,TEXT("BasePassVertexShader"),TEXT("Main"),SF_Vertex,0,0); \
	typedef TBasePassHullShader<LightMapPolicyType,FogDensityPolicyType> TBasePassHullShader##LightMapPolicyType##FogDensityPolicyType; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassHullShader##LightMapPolicyType##FogDensityPolicyType,TEXT("BasePassTessellationShaders"),TEXT("MainHull"),SF_Hull,0,0); \
	typedef TBasePassDomainShader<LightMapPolicyType,FogDensityPolicyType> TBasePassDomainShader##LightMapPolicyType##FogDensityPolicyType; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassDomainShader##LightMapPolicyType##FogDensityPolicyType,TEXT("BasePassTessellationShaders"),TEXT("MainDomain"),SF_Domain,0,0); 
#else

#define IMPLEMENT_BASEPASS_VERTEXSHADER_TYPE(LightMapPolicyType,FogDensityPolicyType) \
	typedef TBasePassVertexShader<LightMapPolicyType,FogDensityPolicyType> TBasePassVertexShader##LightMapPolicyType##FogDensityPolicyType; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassVertexShader##LightMapPolicyType##FogDensityPolicyType,TEXT("BasePassVertexShader"),TEXT("Main"),SF_Vertex,0,0); 
#endif

#define IMPLEMENT_BASEPASS_PIXELSHADER_TYPE(LightMapPolicyType,bEnableSkyLight,SkyLightShaderName) \
	typedef TBasePassPixelShader<LightMapPolicyType,bEnableSkyLight> TBasePassPixelShader##LightMapPolicyType##SkyLightShaderName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassPixelShader##LightMapPolicyType##SkyLightShaderName,TEXT("BasePassPixelShader"),TEXT("Main"),SF_Pixel,VER_FIXED_TRANSLUCENT_SHADOW_FILTERING,0);

// Implement a vertex shader for each supported combination of affecting fog primitives
// These are for forward per-vertex fogging of translucency, opaque materials will always use FNoDensityPolicy
#define IMPLEMENT_BASEPASS_LIGHTMAPPED_VERTEXONLY_TYPE(LightMapPolicyType) \
	IMPLEMENT_BASEPASS_VERTEXSHADER_TYPE(LightMapPolicyType,FNoDensityPolicy); \
	IMPLEMENT_BASEPASS_VERTEXSHADER_TYPE(LightMapPolicyType,FConstantDensityPolicy); \
	IMPLEMENT_BASEPASS_VERTEXSHADER_TYPE(LightMapPolicyType,FLinearHalfspaceDensityPolicy); \
	IMPLEMENT_BASEPASS_VERTEXSHADER_TYPE(LightMapPolicyType,FSphereDensityPolicy); \
	IMPLEMENT_BASEPASS_VERTEXSHADER_TYPE(LightMapPolicyType,FConeDensityPolicy);

// Implement a pixel shader type for skylights and one without, and one vertex shader that will be shared between them
#define IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(LightMapPolicyType) \
	IMPLEMENT_BASEPASS_LIGHTMAPPED_VERTEXONLY_TYPE(LightMapPolicyType) \
	IMPLEMENT_BASEPASS_PIXELSHADER_TYPE(LightMapPolicyType,FALSE,NoSkyLight); \
	IMPLEMENT_BASEPASS_PIXELSHADER_TYPE(LightMapPolicyType,TRUE,SkyLight);

// Implement shader types per lightmap policy
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FNoLightMapPolicy); 
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FDirectionalVertexLightMapPolicy); 
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FSimpleVertexLightMapPolicy); 
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FDirectionalLightMapTexturePolicy); 
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FSimpleLightMapTexturePolicy); 
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FDirectionalLightLightMapPolicy); 
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FDynamicallyShadowedMultiTypeLightLightMapPolicy); 
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FSHLightAndMultiTypeLightMapPolicy); 
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FSHLightLightMapPolicy); 
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FShadowedDynamicLightDirectionalVertexLightMapPolicy); 
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FShadowedDynamicLightDirectionalLightMapTexturePolicy); 
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicy);

#if 1
/** Draws the translucent mesh with a specific light-map type, and fog volume type */
template<typename LightMapPolicyType,typename FogDensityPolicyType>
void FDrawTranslucentMeshAction::Process(
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
	for( INT BatchElementIndex=0;BatchElementIndex < Parameters.Mesh.Elements.Num();BatchElementIndex++ )
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

extern void TransitionToFullResolutionTranslucency(const FViewInfo& View, UBOOL& bRenderingToLowResTranslucencyBuffer);

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
				// BLEND_Modulate and BLEND_ModulateAndAdd can't be supported with the current translucency buffer format,
				// Which only has one float for multiplying with scene color during the composite, but BLEND_Modulate and BLEND_ModulateAdd need three.
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
				for( const FLightPrimitiveInteraction* Interaction = PrimitiveSceneInfo->LightList; Interaction; Interaction = Interaction->GetNextLight() )
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

/** The action used to draw a base pass static mesh element. */
class FDrawBasePassStaticMeshAction
{
public:

	FScene* Scene;
	FStaticMesh* StaticMesh;

	/** Initialization constructor. */
	FDrawBasePassStaticMeshAction(FScene* InScene,FStaticMesh* InStaticMesh):
		Scene(InScene),
		StaticMesh(InStaticMesh)
	{}

	ESceneDepthPriorityGroup GetDPG(const FProcessBasePassMeshParameters& Parameters) const
	{
		return (ESceneDepthPriorityGroup)Parameters.PrimitiveSceneInfo->Proxy->GetStaticDepthPriorityGroup();
	}

	UBOOL ShouldReceiveDominantShadows(const FProcessBasePassMeshParameters& Parameters) const
	{
		// Only allow receiving dynamic shadows from the dominant light if bUseAsOccluder == TRUE, 
		// Since dominant light shadows are projected using depths from the depth only pass, which bUseAsOccluder controls.
		return GOnePassDominantLight && Parameters.PrimitiveSceneInfo->bAcceptsDynamicDominantLightShadows && Parameters.PrimitiveSceneInfo->bUseAsOccluder;
	}

	UBOOL ShouldOverrideDynamicShadowsOnTranslucency(const FProcessBasePassMeshParameters& Parameters) const
	{
		return FALSE;
	}

	UBOOL UseTranslucencyLightAttenuation(const FProcessBasePassMeshParameters& Parameters) const
	{
		return FALSE;
	}

	const FLightSceneInfo* GetTranslucencyMergedDynamicLightInfo() const
	{ 
		return NULL;
	}

	const FSHVectorRGB* GetTranslucencyCompositedDynamicLighting() const 
	{ 
		return NULL; 
	}

	const FProjectedShadowInfo* GetTranslucentPreShadow() const { return NULL; }

	/** Draws the translucent mesh with a specific light-map type, and fog volume type */
	template<typename LightMapPolicyType,typename FogDensityPolicyType>
	void Process(
		const FProcessBasePassMeshParameters& Parameters,
		const LightMapPolicyType& LightMapPolicy,
		const typename LightMapPolicyType::ElementDataType& LightMapElementData,
		const typename FogDensityPolicyType::ElementDataType& FogDensityElementData
		) const
	{
		FDepthPriorityGroup::EBasePassDrawListType DrawType = FDepthPriorityGroup::EBasePass_Default;		
 
		if( StaticMesh->IsDecal() )
		{
			// handle decal case by adding to the decal base pass draw lists
			if( StaticMesh->IsTranslucent() )
			{
				// transparent decals rendered in the base pass are handled separately
				DrawType = FDepthPriorityGroup::EBasePass_Decals_Translucent;
			}
			else
			{
				DrawType = FDepthPriorityGroup::EBasePass_Decals;
			}
		}
		else if (StaticMesh->IsMasked())
		{
			DrawType = FDepthPriorityGroup::EBasePass_Masked;	
		}

		// Find the appropriate draw list for the static mesh based on the light-map policy type.
		TStaticMeshDrawList<TBasePassDrawingPolicy<LightMapPolicyType,FNoDensityPolicy> >& DrawList =
			Scene->DPGs[StaticMesh->DepthPriorityGroup].GetBasePassDrawList<LightMapPolicyType>(DrawType);

		const UBOOL bUsePreviewSkyLight = GIsEditor && LightMapPolicyType::bAllowPreviewSkyLight && Parameters.PrimitiveSceneInfo && Parameters.PrimitiveSceneInfo->bStaticShadowing;
		// Add the static mesh to the draw list.
		DrawList.AddMesh(
			StaticMesh,
			typename TBasePassDrawingPolicy<LightMapPolicyType,FNoDensityPolicy>::ElementDataType(LightMapElementData,FNoDensityPolicy::ElementDataType()),
			TBasePassDrawingPolicy<LightMapPolicyType,FNoDensityPolicy>(
			StaticMesh->VertexFactory,
			StaticMesh->MaterialRenderProxy,
			*Parameters.Material,
			LightMapPolicy,
			Parameters.BlendMode,
			Parameters.LightingModel != MLM_Unlit && (StaticMesh->PrimitiveSceneInfo->HasDynamicSkyLighting() || bUsePreviewSkyLight)
			)
			);
	}
};

void FBasePassOpaqueDrawingPolicyFactory::AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh,ContextType)
{
	// Determine the mesh's material and blend mode.
	const FMaterial* Material = StaticMesh->MaterialRenderProxy->GetMaterial();
	const EBlendMode BlendMode = Material->GetBlendMode();

	// Only draw opaque, non-distorted materials.
	if( (!IsTranslucentBlendMode(BlendMode) && BlendMode != BLEND_SoftMasked && !Material->IsDistorted()) ||
		// allow for decals to batch using translucent materials when rendered on opaque meshes
		StaticMesh->IsDecal() )
	{
		ProcessBasePassMesh(
			FProcessBasePassMeshParameters(
				*StaticMesh,
				Material,
				StaticMesh->PrimitiveSceneInfo,
				FALSE
				),
			FDrawBasePassStaticMeshAction(Scene,StaticMesh)
			);
	}
}

/** The action used to draw a base pass dynamic mesh element. */
class FDrawBasePassDynamicMeshAction
{
public:

	const FSceneView& View;
	UBOOL bBackFace;
	UBOOL bPreFog;
	FHitProxyId HitProxyId;

	/** Initialization constructor. */
	FDrawBasePassDynamicMeshAction(
		const FSceneView& InView,
		const UBOOL bInBackFace,
		const FHitProxyId InHitProxyId
		)
		: View(InView)
		, bBackFace(bInBackFace)
		, HitProxyId(InHitProxyId)
	{}

	ESceneDepthPriorityGroup GetDPG(const FProcessBasePassMeshParameters& Parameters) const
	{
		return (ESceneDepthPriorityGroup)Parameters.PrimitiveSceneInfo->Proxy->GetDepthPriorityGroup(&View);
	}

	UBOOL ShouldReceiveDominantShadows(const FProcessBasePassMeshParameters& Parameters) const
	{
		// Only allow receiving dynamic shadows from the dominant light if bUseAsOccluder == TRUE, 
		// Since dominant light shadows are projected using depths from the depth only pass, which bUseAsOccluder controls.
		return GOnePassDominantLight && Parameters.PrimitiveSceneInfo->bAcceptsDynamicDominantLightShadows && Parameters.PrimitiveSceneInfo->bUseAsOccluder;
	}

	UBOOL ShouldOverrideDynamicShadowsOnTranslucency(const FProcessBasePassMeshParameters& Parameters) const
	{
		return FALSE;
	}

	UBOOL UseTranslucencyLightAttenuation(const FProcessBasePassMeshParameters& Parameters) const
	{
		return FALSE;
	}

	const FLightSceneInfo* GetTranslucencyMergedDynamicLightInfo() const
	{
		return NULL;
	}

	const FSHVectorRGB* GetTranslucencyCompositedDynamicLighting() const 
	{ 
		return NULL; 
	}

	const FProjectedShadowInfo* GetTranslucentPreShadow() const { return NULL; }

	/** Draws the translucent mesh with a specific light-map type, fog volume type, and shader complexity predicate. */
	template<typename LightMapPolicyType>
	void Process(
		const FProcessBasePassMeshParameters& Parameters,
		const LightMapPolicyType& LightMapPolicy,
		const typename LightMapPolicyType::ElementDataType& LightMapElementData
		) const
	{
		const UBOOL bIsLitMaterial = Parameters.LightingModel != MLM_Unlit;

#if !FINAL_RELEASE
		// When rendering masked materials in the shader complexity viewmode, 
		// We want to overwrite complexity for the pixels which get depths written,
		// And accumulate complexity for pixels which get killed due to the opacity mask being below the clip value.
		// This is accomplished by forcing the masked materials to render depths in the depth only pass, 
		// Then rendering in the base pass with additive complexity blending, depth tests on, and depth writes off.
		if ((View.Family->ShowFlags & SHOW_ShaderComplexity) != 0)
		{
			RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());
		}
#endif

		const UBOOL bUsePreviewSkyLight = GIsEditor && LightMapPolicyType::bAllowPreviewSkyLight && Parameters.PrimitiveSceneInfo && Parameters.PrimitiveSceneInfo->bStaticShadowing;
		TBasePassDrawingPolicy<LightMapPolicyType,FNoDensityPolicy> DrawingPolicy(
			Parameters.Mesh.VertexFactory,
			Parameters.Mesh.MaterialRenderProxy,
			*Parameters.Material,
			LightMapPolicy,
			Parameters.BlendMode,
			(Parameters.PrimitiveSceneInfo && (Parameters.PrimitiveSceneInfo->HasDynamicSkyLighting() || bUsePreviewSkyLight)) && bIsLitMaterial,
			(View.Family->ShowFlags & SHOW_ShaderComplexity) != 0
			);
		DrawingPolicy.DrawShared(
			&View,
			DrawingPolicy.CreateBoundShaderState(Parameters.Mesh.GetDynamicVertexStride())
			);

		for( INT BatchElementIndex=0;BatchElementIndex<Parameters.Mesh.Elements.Num();BatchElementIndex++ )
		{
			DrawingPolicy.SetMeshRenderState(
				View,
				Parameters.PrimitiveSceneInfo,
				Parameters.Mesh,
				BatchElementIndex,
				bBackFace,
				typename TBasePassDrawingPolicy<LightMapPolicyType,FNoDensityPolicy>::ElementDataType(
				  LightMapElementData,
				  FNoDensityPolicy::ElementDataType()
				  )
				);
			DrawingPolicy.DrawMesh(Parameters.Mesh, BatchElementIndex);
		}

#if !FINAL_RELEASE
		if ((View.Family->ShowFlags & SHOW_ShaderComplexity) != 0)
		{
			RHISetDepthState(TStaticDepthState<TRUE,CF_LessEqual>::GetRHI());
		}
#endif
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
		if((View.Family->ShowFlags & SHOW_Lighting) != 0)
		{
			Process<LightMapPolicyType>(Parameters,LightMapPolicy,LightMapElementData);
		}
		else
		{
			Process<FNoLightMapPolicy>(Parameters,FNoLightMapPolicy(),FNoLightMapPolicy::ElementDataType());
		}
	}
};

UBOOL FBasePassOpaqueDrawingPolicyFactory::DrawDynamicMesh(
	const FSceneView& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId
	)
{
	// Determine the mesh's material and blend mode.
	const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial();
	const EBlendMode BlendMode = Material->GetBlendMode();

	// Only draw opaque, non-distorted materials.
	if(!IsTranslucentBlendMode(BlendMode) && BlendMode != BLEND_SoftMasked && !Material->IsDistorted())
	{
		ProcessBasePassMesh(
			FProcessBasePassMeshParameters(
				Mesh,
				Material,
				PrimitiveSceneInfo,
				!bPreFog
				),
			FDrawBasePassDynamicMeshAction(
				View,
				bBackFace,
				HitProxyId
				)
			);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/** Maps the no light-map case to the appropriate base pass draw list. */
template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FNoLightMapPolicy,FNoDensityPolicy> >& FDepthPriorityGroup::GetBasePassDrawList<FNoLightMapPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassNoLightMapDrawList[DrawType];
}

/** Maps the directional vertex light-map case to the appropriate base pass draw list. */
template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FDirectionalVertexLightMapPolicy,FNoDensityPolicy> >& FDepthPriorityGroup::GetBasePassDrawList<FDirectionalVertexLightMapPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassDirectionalVertexLightMapDrawList[DrawType];
}

/** Maps the simple vertex light-map case to the appropriate base pass draw list. */
template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FSimpleVertexLightMapPolicy,FNoDensityPolicy> >& FDepthPriorityGroup::GetBasePassDrawList<FSimpleVertexLightMapPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassSimpleVertexLightMapDrawList[DrawType];
}

/** Maps the directional light-map texture case to the appropriate base pass draw list. */
template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FDirectionalLightMapTexturePolicy,FNoDensityPolicy> >& FDepthPriorityGroup::GetBasePassDrawList<FDirectionalLightMapTexturePolicy>(EBasePassDrawListType DrawType)
{
	return BasePassDirectionalLightMapTextureDrawList[DrawType];
}

/** Maps the simple light-map texture case to the appropriate base pass draw list. */
template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FSimpleLightMapTexturePolicy,FNoDensityPolicy> >& FDepthPriorityGroup::GetBasePassDrawList<FSimpleLightMapTexturePolicy>(EBasePassDrawListType DrawType)
{
	return BasePassSimpleLightMapTextureDrawList[DrawType];
}

template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FDirectionalLightLightMapPolicy,FNoDensityPolicy> >& FDepthPriorityGroup::GetBasePassDrawList<FDirectionalLightLightMapPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassDirectionalLightDrawList[DrawType];
}

template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FDynamicallyShadowedMultiTypeLightLightMapPolicy,FNoDensityPolicy> >& FDepthPriorityGroup::GetBasePassDrawList<FDynamicallyShadowedMultiTypeLightLightMapPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassDynamicallyShadowedDynamicLightDrawList[DrawType];
}

template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FSHLightAndMultiTypeLightMapPolicy,FNoDensityPolicy> >& FDepthPriorityGroup::GetBasePassDrawList<FSHLightAndMultiTypeLightMapPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassSHLightAndDynamicLightDrawList[DrawType];
}

template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FSHLightLightMapPolicy,FNoDensityPolicy> >& FDepthPriorityGroup::GetBasePassDrawList<FSHLightLightMapPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassSHLightDrawList[DrawType];
}

template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FShadowedDynamicLightDirectionalVertexLightMapPolicy,FNoDensityPolicy> >& FDepthPriorityGroup::GetBasePassDrawList<FShadowedDynamicLightDirectionalVertexLightMapPolicy>(EBasePassDrawListType DrawType)
{
	return BasePassShadowedDynamicLightDirectionalVertexLightMapDrawList[DrawType];
}

template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FShadowedDynamicLightDirectionalLightMapTexturePolicy,FNoDensityPolicy> >& FDepthPriorityGroup::GetBasePassDrawList<FShadowedDynamicLightDirectionalLightMapTexturePolicy>(EBasePassDrawListType DrawType)
{
	return BasePassShadowedDynamicLightDirectionalLightMapTextureDrawList[DrawType];
}

template<>
TStaticMeshDrawList<TBasePassDrawingPolicy<FDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicy,FNoDensityPolicy> >& FDepthPriorityGroup::GetBasePassDrawList<FDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicy>(EBasePassDrawListType DrawType)
{
	return BasePassDistanceFieldShadowedDynamicLightDirectionalLightMapTextureDrawList[DrawType];
}
