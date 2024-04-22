/*=============================================================================
	SkyLightComponent.cpp: SkyLightComponent implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"

IMPLEMENT_CLASS(USkyLightComponent);

/**
 * Information used to render a sky light.
 */
class FSkyLightSceneInfo : public FLightSceneInfo, public FLightSceneDPGInfoInterface
{
public:
	
	/** Initialization constructor. */
	FSkyLightSceneInfo(const USkyLightComponent* Component):
		FLightSceneInfo(Component),
		LowerColor(FLinearColor(Component->LowerColor) * Component->LowerBrightness)
	{
	}

	/** Composites the light's influence into an SH vector. */
	virtual void CompositeInfluence(const FVector& Point, FSHVectorRGB& CompositeSH) const
	{
		CompositeSH += FSHVector::UpperSkyFunction() * Color + FSHVector::LowerSkyFunction() * LowerColor;
	}

	// FLightSceneInfo interface.
	virtual void AttachPrimitive(const FLightPrimitiveInteraction& Interaction)
	{
		if(!Interaction.IsLightMapped())
		{
			Interaction.GetPrimitiveSceneInfo()->UpperSkyLightColor += Color;
			Interaction.GetPrimitiveSceneInfo()->LowerSkyLightColor += LowerColor;

			// Update the primitive's static meshes, to ensure they use the right version of the base pass shaders.
			Interaction.GetPrimitiveSceneInfo()->BeginDeferredUpdateStaticMeshes();
		}
	}
	virtual void DetachPrimitive(const FLightPrimitiveInteraction& Interaction)
	{
		if(!Interaction.IsLightMapped())
		{
			Interaction.GetPrimitiveSceneInfo()->UpperSkyLightColor -= Color;
			Interaction.GetPrimitiveSceneInfo()->LowerSkyLightColor -= LowerColor;

			// Update the primitive's static meshes, to ensure they use the right version of the base pass shaders.
			Interaction.GetPrimitiveSceneInfo()->BeginDeferredUpdateStaticMeshes();
		}
	}
	virtual const FLightSceneDPGInfoInterface* GetDPGInfo(UINT DPGIndex) const
	{
		check(DPGIndex < SDPG_MAX_SceneRender);
		return this;
	}
	virtual FLightSceneDPGInfoInterface* GetDPGInfo(UINT DPGIndex)
	{
		check(DPGIndex < SDPG_MAX_SceneRender);
		return this;
	}

	virtual UBOOL DrawTranslucentMesh(
		const FSceneView& View,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		UBOOL bUseTranslucencyLightAttenuation,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FProjectedShadowInfo* TranslucentPreShadowInfo,
		FHitProxyId HitProxyId
		) const
	{
		return FALSE;
	}

	/**
	* @return modulated shadow projection pixel shader for this light type
	*/
	virtual class FShadowProjectionPixelShaderInterface* GetModShadowProjPixelShader(UBOOL bRenderingBeforeLight) const
	{
		return NULL;
	}

	/**
	* @return Branching PCF modulated shadow projection pixel shader for this light type
	*/
	virtual class FBranchingPCFProjectionPixelShaderInterface* GetBranchingPCFModProjPixelShader(UBOOL bRenderingBeforeLight) const
	{
		return NULL;
	}

	/**
	* @return modulated shadow projection bound shader state for this light type
	*/
	virtual FGlobalBoundShaderState* GetModShadowProjBoundShaderState(UBOOL bRenderingBeforeLight) const
	{
		return NULL;
	}

	/**
	* @return PCF Branching modulated shadow projection bound shader state for this light type
	*/
	virtual FGlobalBoundShaderState* GetBranchingPCFModProjBoundShaderState(UBOOL bRenderingBeforeLight) const
	{
		return NULL;
	}

	// FLightSceneDPGInfoInterface
	virtual UBOOL DrawStaticMeshesVisible(
		const FViewInfo& View,
		const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap,
		ELightPassDrawListType DrawType
		) const
	{
		return FALSE;
	}

	virtual ELightInteractionType AttachStaticMesh(const FLightSceneInfo* LightSceneInfo,FStaticMesh* Mesh)
	{
		return LIT_Uncached;
	}

	virtual void DetachStaticMeshes()
	{
	}

	virtual UBOOL DrawDynamicMesh(
		const FSceneView& View,
		const FLightSceneInfo* LightSceneInfo,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		) const
	{
		return FALSE;
	}

private:

	/** The light color of the lower hemisphere of the sky light. */
	FLinearColor LowerColor;
};

FLightSceneInfo* USkyLightComponent::CreateSceneInfo() const
{
	return new FSkyLightSceneInfo(this);
}

FVector4 USkyLightComponent::GetPosition() const
{
	return FVector4(0,0,1,0);
}

/**
* @return ELightComponentType for the light component class 
*/
ELightComponentType USkyLightComponent::GetLightType() const
{
	return LightType_Sky;
}

/**
 * Called when a property is being changed.
 *
 * @param PropertyThatChanged	Property that changed or NULL if unknown or multiple
 */
void  USkyLightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
 * Called after data has been serialized.
 */
void USkyLightComponent::PostLoad()
{
	Super::PostLoad();
}




