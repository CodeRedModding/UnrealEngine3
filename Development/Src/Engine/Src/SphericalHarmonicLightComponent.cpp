/*=============================================================================
	SphericalHarmonicLightComponent.cpp: SphericalHarmonicLightComponent implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "LightRendering.h"

IMPLEMENT_CLASS(USphericalHarmonicLightComponent);

/**
 * The SH light policy for TMeshLightingDrawingPolicy.
 */
class FSphericalHarmonicLightPolicy
{
public:
	typedef class FSphericalHarmonicLightSceneInfo SceneInfoType;
	class VertexParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
		}
		template<typename ShaderRHIParamRef>
		void SetLight(ShaderRHIParamRef Shader,const SceneInfoType* Light,const FSceneView* View) const
		{
		}
		void Serialize(FArchive& Ar)
		{
		}
	};

	class PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			WorldIncidentLightingParameter.Bind(ParameterMap,TEXT("WorldIncidentLighting"),TRUE);
		}
		void SetLight(FShader* PixelShader,const SceneInfoType* Light,const FSceneView* View) const;
		void SetLightMesh(FShader* PixelShader,const FPrimitiveSceneInfo* PrimitiveSceneInfo,const SceneInfoType* Light,UBOOL bApplyLightFunctionDisabledBrightness) const {}
		void Serialize(FArchive& Ar)
		{
			Ar << WorldIncidentLightingParameter;
		}

	private:
		FShaderParameter WorldIncidentLightingParameter;
	};

	static UBOOL ShouldCacheStaticLightingShaders()
	{
		return FALSE;
	}

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		// Don't compile for terrain materials as it will never be used for rendering
		return !Material->IsTerrainMaterial();
	}
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment) {}
};

IMPLEMENT_SHADOWLESS_LIGHT_SHADER_TYPE(FSphericalHarmonicLightPolicy,TEXT("SphericalHarmonicLightVertexShader"),TEXT("SphericalHarmonicLightPixelShader"),VER_TRANSLUCENT_PRESHADOWS,0)

/**
 * The scene info for a directional light.
 */
class FSphericalHarmonicLightSceneInfo : public FLightSceneInfo
{
public:

	/** Colored SH coefficients for the light intensity, parameterized by the world-space incident angle. */
	FSHVectorRGB WorldSpaceIncidentLighting;

	/** Initialization constructor. */
	FSphericalHarmonicLightSceneInfo(const USphericalHarmonicLightComponent* Component)
	:	FLightSceneInfo(Component)
	,	WorldSpaceIncidentLighting(Component->WorldSpaceIncidentLighting)
	,	bRenderBeforeModShadows(Component->bRenderBeforeModShadows)
	{}

	/** Composites the light's influence into an SH vector. */
	virtual void CompositeInfluence(const FVector& Point, FSHVectorRGB& CompositeSH) const
	{
		CompositeSH += WorldSpaceIncidentLighting;
	}

	virtual const FSHVectorRGB* GetSHIncidentLighting() const
	{
		return &WorldSpaceIncidentLighting;
	}

	virtual void DetachPrimitive(const FLightPrimitiveInteraction& Interaction) 
	{
		if (Interaction.GetPrimitiveSceneInfo()->SHLightSceneInfo == this)
		{
			Interaction.GetPrimitiveSceneInfo()->SHLightSceneInfo = NULL;
			Interaction.GetPrimitiveSceneInfo()->bRenderSHLightInBasePass = FALSE;
			// Update the primitive's static meshes, to ensure they use the right version of the base pass shaders.
			Interaction.GetPrimitiveSceneInfo()->BeginDeferredUpdateStaticMeshes();
		}
	}

	virtual void AttachPrimitive(const FLightPrimitiveInteraction& Interaction)
	{
		if (LightEnvironment && LightEnvironment == Interaction.GetPrimitiveSceneInfo()->LightEnvironment)
		{
			// Set SHLightSceneInfo whether the light is allowed to be merged into the base pass or not
			// This allows later code to override the behavior
			Interaction.GetPrimitiveSceneInfo()->SHLightSceneInfo = this;
			// If the SH light is allowed to render before modulated shadows, merge it into the base pass
			Interaction.GetPrimitiveSceneInfo()->bRenderSHLightInBasePass = bRenderBeforeModShadows;
			// Update the primitive's static meshes, to ensure they use the right version of the base pass shaders.
			Interaction.GetPrimitiveSceneInfo()->BeginDeferredUpdateStaticMeshes();
		}
	}

	// FLightSceneInfo interface.
	virtual UBOOL GetProjectedShadowInitializer(const FBoxSphereBounds& SubjectBounds,FProjectedShadowInitializer& OutInitializer) const
	{
		return FALSE;
	}
	virtual const FLightSceneDPGInfoInterface* GetDPGInfo(UINT DPGIndex) const
	{
		check(DPGIndex < SDPG_MAX_SceneRender);
		return &DPGInfos[DPGIndex];
	}
	virtual FLightSceneDPGInfoInterface* GetDPGInfo(UINT DPGIndex)
	{
		check(DPGIndex < SDPG_MAX_SceneRender);
		return &DPGInfos[DPGIndex];
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
		// Don't draw if we are a shadow-only light
		if( Color.GetMax() > 0.f )
		{
			return DrawLitDynamicMesh<FSphericalHarmonicLightPolicy>(View, this, Mesh, bBackFace, bPreFog, TRUE, bUseTranslucencyLightAttenuation, PrimitiveSceneInfo, TranslucentPreShadowInfo, HitProxyId);
		}
		return FALSE;
	}

	virtual class FShadowProjectionPixelShaderInterface* GetModShadowProjPixelShader(UBOOL bRenderingBeforeLight) const
	{
		return NULL;
	}
	virtual class FBranchingPCFProjectionPixelShaderInterface* GetBranchingPCFModProjPixelShader(UBOOL bRenderingBeforeLight) const
	{
		return NULL;
	} 
	virtual FGlobalBoundShaderState* GetModShadowProjBoundShaderState(UBOOL bRenderingBeforeLight) const
	{
		return NULL;
	}
	virtual FGlobalBoundShaderState* GetBranchingPCFModProjBoundShaderState(UBOOL bRenderingBeforeLight) const
	{
		return NULL;
	}

private:

	/** The DPG info for the SH light. */
	TLightSceneDPGInfo<FSphericalHarmonicLightPolicy> DPGInfos[SDPG_MAX_SceneRender];

	/**
	 * If TRUE, the SH light can be combined into the base pass as an optimization.  
	 * If FALSE, the SH light will be rendered after modulated shadows.
	 */
	const BITFIELD bRenderBeforeModShadows : 1;
};

static void SetSHPixelParameters(
	FShader* PixelShader, 
	const FSHVectorRGB& WorldSpaceIncidentLighting, 
	const FShaderParameter& WorldIncidentLightingParameter)
{
	// Pack the incident lighting SH coefficients into an array in the format expected by the shader:
	// The first float4 contains the RGB constant term.
	// The next NUM_SH_VECTORS float4s contain the coefficients for the red component of the incident lighting.
	// The next NUM_SH_VECTORS float4s contain the coefficients for the green component of the incident lighting.
	// The next NUM_SH_VECTORS float4s contain the coefficients for the blue component of the incident lighting.
	enum { NUM_SH_VECTORS = (MAX_SH_BASIS - 1 + 3) / 4 };
	FLOAT PackedWorldSpaceIncidentLightingSH[4 + NUM_SH_VECTORS * 4 * 3];
	PackedWorldSpaceIncidentLightingSH[0] = WorldSpaceIncidentLighting.R.V[0];
	PackedWorldSpaceIncidentLightingSH[1] = WorldSpaceIncidentLighting.G.V[0];
	PackedWorldSpaceIncidentLightingSH[2] = WorldSpaceIncidentLighting.B.V[0];
	PackedWorldSpaceIncidentLightingSH[3] = 0.0f;
	for(INT BasisIndex = 1;BasisIndex < MAX_SH_BASIS;BasisIndex++)
	{
		PackedWorldSpaceIncidentLightingSH[4 + 0 * NUM_SH_VECTORS * 4 + BasisIndex - 1] = WorldSpaceIncidentLighting.R.V[BasisIndex];
		PackedWorldSpaceIncidentLightingSH[4 + 1 * NUM_SH_VECTORS * 4 + BasisIndex - 1] = WorldSpaceIncidentLighting.G.V[BasisIndex];
		PackedWorldSpaceIncidentLightingSH[4 + 2 * NUM_SH_VECTORS * 4 + BasisIndex - 1] = WorldSpaceIncidentLighting.B.V[BasisIndex];
	}

	SetPixelShaderValues(
		PixelShader->GetPixelShader(),
		WorldIncidentLightingParameter,
		PackedWorldSpaceIncidentLightingSH,
		ARRAY_COUNT(PackedWorldSpaceIncidentLightingSH)
		);
}

void FSphericalHarmonicLightPolicy::PixelParametersType::SetLight(FShader* PixelShader,const SceneInfoType* Light,const FSceneView* View) const
{
	SetSHPixelParameters(PixelShader, Light->WorldSpaceIncidentLighting, WorldIncidentLightingParameter);
}

FLightSceneInfo* USphericalHarmonicLightComponent::CreateSceneInfo() const
{
	return new FSphericalHarmonicLightSceneInfo(this);
}

FVector4 USphericalHarmonicLightComponent::GetPosition() const
{
	return FVector4(0,0,1,0);
}

ELightComponentType USphericalHarmonicLightComponent::GetLightType() const
{
	return LightType_SphericalHarmonic;
}

void FSHLightLightMapPolicy::SetMesh(
	const FSceneView& View,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const VertexParametersType* VertexShaderParameters,
	const PixelParametersType* PixelShaderParameters,
	FShader* VertexShader,
	FShader* PixelShader,
	const FVertexFactory* VertexFactory,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const ElementDataType& ElementData
	) const
{
	Super::SetMesh(View, PrimitiveSceneInfo, VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, ElementData.SuperElementData);

	if (PixelShaderParameters)
	{
		SetSHPixelParameters(PixelShader, ElementData.WorldSpaceIncidentLighting, PixelShaderParameters->WorldIncidentLightingParameter);
	}
}

void FSHLightAndMultiTypeLightMapPolicy::SetMesh(
	const FSceneView& View,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const VertexParametersType* VertexShaderParameters,
	const PixelParametersType* PixelShaderParameters,
	FShader* VertexShader,
	FShader* PixelShader,
	const FVertexFactory* VertexFactory,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const ElementDataType& ElementData
	) const
{
	Super::SetMesh(View, PrimitiveSceneInfo, VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, ElementData.SuperElementData);

	const FSphericalHarmonicLightSceneInfo* Light = static_cast<const FSphericalHarmonicLightSceneInfo*>(ElementData.PrimitiveSceneInfo->SHLightSceneInfo);
	if (PixelShaderParameters)
	{
		check(Light);
		SetSHPixelParameters(PixelShader, Light->WorldSpaceIncidentLighting, PixelShaderParameters->WorldIncidentLightingParameter);
	}
}
