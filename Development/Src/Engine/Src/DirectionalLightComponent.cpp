/*=============================================================================
	DirectionalLightComponent.cpp: DirectionalLightComponent implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "LightRendering.h"

IMPLEMENT_CLASS(UDirectionalLightComponent);
IMPLEMENT_CLASS(UDominantDirectionalLightComponent);

/**
 * The directional light policy for TMeshLightingDrawingPolicy.
 */
class FDirectionalLightPolicy
{
public:
	typedef FLightSceneInfo SceneInfoType;
	class VertexParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			LightDirectionParameter.Bind(ParameterMap,TEXT("LightDirection"));
		}
		template<typename ShaderRHIParamRef>
		void SetLight(ShaderRHIParamRef Shader,const SceneInfoType* Light,const FSceneView* View) const
		{
			SetShaderValue(Shader,LightDirectionParameter,FVector4(-Light->GetDirection(),0));
		}
		void Serialize(FArchive& Ar)
		{
			Ar << LightDirectionParameter;
			
			// set parameter names for platforms that need them
			LightDirectionParameter.SetShaderParamName(TEXT("LightDirection"));
		}
	private:
		FShaderParameter LightDirectionParameter;
	};
	
	class PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			LightColorParameter.Bind(ParameterMap,TEXT("LightColorAndFalloffExponent"),TRUE);
			bEnableDistanceShadowFadingParameter.Bind(ParameterMap, TEXT("bEnableDistanceShadowFading"), TRUE);
			DistanceFadeParameter.Bind(ParameterMap, TEXT("DistanceFadeParameters"), TRUE);
		}
		void SetLight(FShader* PixelShader,const SceneInfoType* Light,const FSceneView* View) const
		{
			FVector2D DistanceFadeValues;
			const UBOOL bEnableDistanceShadowFading = View->Family->ShouldDrawShadows() 
				&& GSystemSettings.bAllowWholeSceneDominantShadows
				&& (View->RenderingOverrides.bAllowDominantWholeSceneDynamicShadows || !Light->bStaticShadowing)
				&& Light->GetDirectionalLightDistanceFadeParameters(DistanceFadeValues)
				// The lifetime of the whole scene dominant shadow projection is only during the world DPG
				// This prevents objects in the foreground DPG from reading the projection results outside of that lifetime
				&& GSceneRenderTargets.IsWholeSceneDominantShadowValid();

			SetPixelShaderBool(PixelShader->GetPixelShader(), bEnableDistanceShadowFadingParameter, bEnableDistanceShadowFading);
			if (bEnableDistanceShadowFading)
			{
				SetPixelShaderValue(PixelShader->GetPixelShader(), DistanceFadeParameter, FVector4(DistanceFadeValues.X, DistanceFadeValues.Y, 0, 0));
			}
		}
		void SetLightMesh(FShader* PixelShader,const FPrimitiveSceneInfo* PrimitiveSceneInfo,const SceneInfoType* Light,UBOOL bApplyLightFunctionDisabledBrightness) const
		{
			FLOAT ShadowFactor = IsDominantLightType(Light->LightType) ? PrimitiveSceneInfo->DominantShadowFactor : 1.0f;
			if (bApplyLightFunctionDisabledBrightness)
			{
				ShadowFactor *= Light->LightFunctionDisabledBrightness;
			}
			SetPixelShaderValue(PixelShader->GetPixelShader(),LightColorParameter,FVector(Light->Color.R * ShadowFactor, Light->Color.G * ShadowFactor, Light->Color.B * ShadowFactor));
		}
		void Serialize(FArchive& Ar)
		{
			Ar << LightColorParameter;
			Ar << bEnableDistanceShadowFadingParameter;
			Ar << DistanceFadeParameter;

			// set parameter names for platforms that need them
			LightColorParameter.SetShaderParamName(TEXT("LightColor"));
		}
	private:
		FShaderParameter LightColorParameter;
		FShaderParameter bEnableDistanceShadowFadingParameter;
		FShaderParameter DistanceFadeParameter;
	};

	/**
	* Modulated shadow shader params associated with this light policy
	*/
	class ModShadowPixelParamsType
	{
	public:
		void Bind( const FShaderParameterMap& ParameterMap )
		{			
		}
		void SetModShadowLight( FShader* PixelShader, const SceneInfoType* Light,const FSceneView* View) const;
		void Serialize( FArchive& Ar )
		{
		}
		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
			OutEnvironment.Definitions.Set(TEXT("MODSHADOW_LIGHTTYPE_DIRECTIONAL"),TEXT("1"));
		}	
	};

	static UBOOL ShouldCacheStaticLightingShaders()
	{
		return TRUE;
	}

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment) 
	{
		// Xbox, PS3, WiiU have GOnePassDominantLight == TRUE, and therefore won't be applying dominant shadows in the additive lighting passes
		if (Platform != SP_XBOXD3D && Platform != SP_PS3 && Platform != SP_WIIU)
		{
			OutEnvironment.Definitions.Set(TEXT("ENABLE_DISTANCE_SHADOW_FADING"),TEXT("1"));
		}
	}
};

void FDirectionalLightPolicy::ModShadowPixelParamsType::SetModShadowLight(FShader* PixelShader,const SceneInfoType* Light,const FSceneView* View) const
{	
}

IMPLEMENT_LIGHT_SHADER_TYPE(FDirectionalLightPolicy,TEXT("DirectionalLightVertexShader"),TEXT("DirectionalLightPixelShader"),VER_TRANSLUCENT_PRESHADOWS,0)

/**
 * The scene info for a directional light.
 */
class FDirectionalLightSceneInfo : public FLightSceneInfo
{
public:

	/** 
	 * Radius of the whole scene dynamic shadow centered on the viewer, which replaces the precomputed shadows based on distance from the camera.  
	 * A Radius of 0 disables the dynamic shadow. This feature is currently only supported on dominant directional lights.
	 */
	FLOAT WholeSceneDynamicShadowRadius;

	/** 
	 * Number of cascades to split the view frustum into for the whole scene dynamic shadow.  
	 * More cascades result in better shadow resolution and allow WholeSceneDynamicShadowRadius to be further, but add rendering cost.
	 */
	INT NumWholeSceneDynamicShadowCascades;

	/** 
	 * Exponent that is applied to the cascade transition distances as a fraction of WholeSceneDynamicShadowRadius.
	 * An exponent of 1 means that cascade transitions will happen at a distance proportional to their resolution.
	 * A value greater than 1 brings transitions closer to the camera.
	 */
	FLOAT CascadeDistributionExponent;

	/** Initialization constructor. */
	FDirectionalLightSceneInfo(const UDirectionalLightComponent* Component):
		FLightSceneInfo(Component),
		WholeSceneDynamicShadowRadius(Component->WholeSceneDynamicShadowRadius),
		NumWholeSceneDynamicShadowCascades(Component->NumWholeSceneDynamicShadowCascades),
		CascadeDistributionExponent(Component->CascadeDistributionExponent)
	{
		// Convert LightSourceAngle into uniform penumbra size, since LightSourceAngle doesn't have any meaning for distance field shadowed lights
		DistanceFieldShadowMapPenumbraSize = Clamp(Component->LightmassSettings.LightSourceAngle / 3.0f, 0.001f, 1.0f);
		DistanceFieldShadowMapShadowExponent = Component->LightmassSettings.ShadowExponent;
	}

	/** Accesses parameters needed for rendering the light. */
	virtual void GetParameters(FVector4& LightPositionAndInvRadius, FVector4& LightColorAndFalloffExponent, FVector& LightDirection, FVector2D& SpotAngles) const
	{
		LightPositionAndInvRadius = FVector4(0, 0, 0);

		LightColorAndFalloffExponent = FVector4(
			Color.R,
			Color.G,
			Color.B,
			0);

		LightDirection = -GetDirection();

		SpotAngles = FVector2D(0, 0);
	}

	virtual void DetachPrimitive(const FLightPrimitiveInteraction& Interaction) 
	{
		DetachPrimitiveShared(Interaction);
	}

	virtual void AttachPrimitive(const FLightPrimitiveInteraction& Interaction)
	{
		AttachPrimitiveShared(Interaction);

		FPrimitiveSceneInfo* PrimitiveSceneInfo = Interaction.GetPrimitiveSceneInfo();
		if (!PrimitiveSceneInfo->DynamicLightSceneInfo
			&& LightEnvironment 
			&& LightEnvironment == PrimitiveSceneInfo->LightEnvironment)
		{
			PrimitiveSceneInfo->DynamicLightSceneInfo = this;

			// Update the primitive's static meshes, to ensure they use the right version of the base pass shaders.
			PrimitiveSceneInfo->BeginDeferredUpdateStaticMeshes();
		}
	}

	// FLightSceneInfo interface.
	virtual UBOOL GetProjectedShadowInitializer(const FBoxSphereBounds& SubjectBounds,FProjectedShadowInitializer& OutInitializer) const
	{
		// For directional lights, use an orthographic projection.
		return OutInitializer.CalcObjectShadowTransforms(
			-SubjectBounds.Origin,
			FInverseRotationMatrix(FVector(WorldToLight.M[0][2],WorldToLight.M[1][2],WorldToLight.M[2][2]).SafeNormal().Rotation()) *
				FScaleMatrix(FVector(1.0f,1.0f / SubjectBounds.SphereRadius,1.0f / SubjectBounds.SphereRadius)),
			FVector(1,0,0),
			FBoxSphereBounds(FVector(0,0,0),SubjectBounds.BoxExtent,SubjectBounds.SphereRadius),
			FVector4(0,0,0,1),
			-HALF_WORLD_MAX,
			HALF_WORLD_MAX / 8.0f,
			TRUE
			);
	}

	virtual void BeginSceneEditor() 
	{
		checkSlow(GIsEditor);
		// If the light is unbuilt and is not already using CSM, setup CSM settings to preview unbuilt lighting
		if (!bPrecomputedLightingIsValid && bStaticShadowing)
		{
			CascadeDistributionExponent = 4;
			NumWholeSceneDynamicShadowCascades = GSystemSettings.UnbuiltNumWholeSceneDynamicShadowCascades;
			WholeSceneDynamicShadowRadius = GSystemSettings.UnbuiltWholeSceneDynamicShadowRadius;
		}
	}

	/** Returns the number of view dependent shadows this light will create. */
	virtual INT GetNumViewDependentWholeSceneShadows() const 
	{ 
		return WholeSceneDynamicShadowRadius > 0.0f ? NumWholeSceneDynamicShadowCascades : 0; 
	}

	/** Returns TRUE if this light is both directional and has a valid light function */
	virtual UBOOL IsDirectionLightWithLightFunction() const 
	{
		return LightFunction && LightFunction->GetMaterial()->IsLightFunction();
	}

	/**
	 * Sets up a projected shadow initializer that's dependent on the current view for shadows from the entire scene.
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual UBOOL GetViewDependentWholeSceneProjectedShadowInitializer(const FViewInfo& View, INT SplitIndex, FProjectedShadowInitializer& OutInitializer) const
	{
		if (IsDominantLightType((ELightComponentType)LightType) && WholeSceneDynamicShadowRadius > 0.0f)
		{
			const FSphere Bounds = FDirectionalLightSceneInfo::GetShadowSplitBounds(View, SplitIndex);
			const FLOAT ShadowExtent = Bounds.W / appSqrt(3.0f);
			const FBoxSphereBounds SubjectBounds(Bounds.Center, FVector(ShadowExtent, ShadowExtent, ShadowExtent), Bounds.W);
			return OutInitializer.CalcWholeSceneShadowTransforms(
				-Bounds.Center,
				FInverseRotationMatrix(FVector(WorldToLight.M[0][2],WorldToLight.M[1][2],WorldToLight.M[2][2]).SafeNormal().Rotation()) *
				FScaleMatrix(FVector(1.0f,1.0f / Bounds.W,1.0f / Bounds.W)),
				FVector(1,0,0),
				FBoxSphereBounds(FVector(0,0,0),SubjectBounds.BoxExtent,SubjectBounds.SphereRadius),
				FVector4(0,0,0,1),
				-HALF_WORLD_MAX,
				HALF_WORLD_MAX / 8.0f,
				TRUE,
				FALSE,
				SplitIndex
				);
		}
		return FALSE;
	}

	virtual FSphere GetShadowSplitBounds(const FViewInfo& View, INT SplitIndex) const
	{
		// The first split uses the larger shadowmap
		const FIntPoint FirstSplitResolution = GSceneRenderTargets.GetShadowDepthTextureResolution(TRUE);
		const FIntPoint OtherSplitResolution = GSceneRenderTargets.GetShadowDepthTextureResolution(FALSE);
		const FLOAT TotalResolution = FirstSplitResolution.X * FirstSplitResolution.Y + OtherSplitResolution.X * OtherSplitResolution.Y * Max(NumWholeSceneDynamicShadowCascades - 1, 0);
		const FLOAT StartTotalResolution = SplitIndex == 0 ? 0 : FirstSplitResolution.X * FirstSplitResolution.Y + OtherSplitResolution.X * OtherSplitResolution.Y * (SplitIndex - 1);
		const FLOAT EndTotalResolution = FirstSplitResolution.X * FirstSplitResolution.Y + OtherSplitResolution.X * OtherSplitResolution.Y * SplitIndex;
		// Determine split start and end distances based on their fraction of the total resolution
		const FLOAT StartFraction = appPow(StartTotalResolution / TotalResolution, CascadeDistributionExponent);
		const FLOAT EndFraction = appPow(EndTotalResolution / TotalResolution, CascadeDistributionExponent);
		const FLOAT FrustumStartDistance = WholeSceneDynamicShadowRadius * StartFraction;
		const FLOAT FrustumEndDistance = WholeSceneDynamicShadowRadius * EndFraction;

		FLOAT FOV = PI / 4.0f;
		FLOAT AspectRatio = 1.0f;
		if (View.ViewOrigin.W > 0.0f)
		{
			// Derive FOV and aspect ratio from the perspective projection matrix
			FOV = appAtan(1.0f / View.ProjectionMatrix.M[0][0]);
			// Clamp to prevent shimmering when zooming in
			FOV = Max(FOV, GSystemSettings.CSMMinimumFOV * (FLOAT)PI / 180.0f);
			const FLOAT RoundFactorRadians = GSystemSettings.CSMFOVRoundFactor * (FLOAT)PI / 180.0f;
			// Round up to a fixed factor
			// This causes the shadows to make discreet jumps as the FOV animates, instead of slowly crawling over a long period
			FOV = FOV + RoundFactorRadians - appFmod(FOV, RoundFactorRadians);
			AspectRatio = View.ProjectionMatrix.M[1][1] / View.ProjectionMatrix.M[0][0];
		}
		const FLOAT StartHorizontalLength = FrustumStartDistance * appTan(FOV);
		const FVector StartCameraRightOffset = View.ViewMatrix.GetColumn(0) * StartHorizontalLength;
		const FLOAT StartVerticalLength = StartHorizontalLength / AspectRatio;
		const FVector StartCameraUpOffset = View.ViewMatrix.GetColumn(1) * StartVerticalLength;

		const FLOAT EndHorizontalLength = FrustumEndDistance * appTan(FOV);
		const FVector EndCameraRightOffset = View.ViewMatrix.GetColumn(0) * EndHorizontalLength;
		const FLOAT EndVerticalLength = EndHorizontalLength / AspectRatio;
		const FVector EndCameraUpOffset = View.ViewMatrix.GetColumn(1) * EndVerticalLength;

		FVector SplitVertices[8];
		SplitVertices[0] = View.ViewOrigin + View.GetViewDirection() * FrustumStartDistance + StartCameraRightOffset + StartCameraUpOffset;
		SplitVertices[1] = View.ViewOrigin + View.GetViewDirection() * FrustumStartDistance + StartCameraRightOffset - StartCameraUpOffset;
		SplitVertices[2] = View.ViewOrigin + View.GetViewDirection() * FrustumStartDistance - StartCameraRightOffset + StartCameraUpOffset;
		SplitVertices[3] = View.ViewOrigin + View.GetViewDirection() * FrustumStartDistance - StartCameraRightOffset - StartCameraUpOffset;

		SplitVertices[4] = View.ViewOrigin + View.GetViewDirection() * FrustumEndDistance + EndCameraRightOffset + EndCameraUpOffset;
		SplitVertices[5] = View.ViewOrigin + View.GetViewDirection() * FrustumEndDistance + EndCameraRightOffset - EndCameraUpOffset;
		SplitVertices[6] = View.ViewOrigin + View.GetViewDirection() * FrustumEndDistance - EndCameraRightOffset + EndCameraUpOffset;
		SplitVertices[7] = View.ViewOrigin + View.GetViewDirection() * FrustumEndDistance - EndCameraRightOffset - EndCameraUpOffset;

		FVector Center(0,0,0);
		// Weight the far vertices more so that the bounding sphere will be further from the camera
		// This minimizes wasted shadowmap space behind the viewer
		const FLOAT FarVertexWeightScale = 10.0f;
		for (INT VertexIndex = 0; VertexIndex < 8; VertexIndex++)
		{
			const FLOAT Weight = VertexIndex > 3 ? 1 / (4.0f + 4.0f / FarVertexWeightScale) : 1 / (4.0f + 4.0f * FarVertexWeightScale);
			Center += SplitVertices[VertexIndex] * Weight;
		}

		FLOAT RadiusSquared = 0;
		for (INT VertexIndex = 0; VertexIndex < 8; VertexIndex++)
		{
			RadiusSquared = Max(RadiusSquared, (Center - SplitVertices[VertexIndex]).SizeSquared());
		}

		return FSphere(Center, appSqrt(RadiusSquared));
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

	/** Returns TRUE if distance fading is enabled and passes out distance fade parameters in DistanceFadeValues. */
	virtual UBOOL GetDirectionalLightDistanceFadeParameters(FVector2D& DistanceFadeValues) const
	{
		// Ideally we should be able to use a far distance of WholeSceneDynamicShadowRadius * 2, because the shadow bounds are translated to be entirely in front of the view.
		// In reality the camera's side frustum planes intersect the far sides of the shadowmap much closer
		//@todo - calculate this distance in a robust way that handles different camera FOV's
		const FLOAT FarDistance = WholeSceneDynamicShadowRadius * 1.5f;
		const FLOAT NearDistance = FarDistance - WholeSceneDynamicShadowRadius * .1f;
		DistanceFadeValues = FVector2D(NearDistance, 1.0f / (FarDistance - NearDistance));
		return WholeSceneDynamicShadowRadius > 0.0f && NumWholeSceneDynamicShadowCascades > 0;
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
			return DrawLitDynamicMesh<FDirectionalLightPolicy>(View, this, Mesh, bBackFace, bPreFog, TRUE, bUseTranslucencyLightAttenuation, PrimitiveSceneInfo, TranslucentPreShadowInfo, HitProxyId);
		}
		return FALSE;
	}

	/**
	* @return modulated shadow projection pixel shader for this light type
	*/
	virtual class FShadowProjectionPixelShaderInterface* GetModShadowProjPixelShader(UBOOL bRenderingBeforeLight) const
	{
		return GetModProjPixelShaderRef <FDirectionalLightPolicy> (GetEffectiveShadowFilterQuality(bRenderingBeforeLight));
	}

	/**
	* @return Branching PCF modulated shadow projection pixel shader for this light type
	*/
	virtual class FBranchingPCFProjectionPixelShaderInterface* GetBranchingPCFModProjPixelShader(UBOOL bRenderingBeforeLight) const
	{
		return GetBranchingPCFModProjPixelShaderRef <FDirectionalLightPolicy> (GetEffectiveShadowFilterQuality(bRenderingBeforeLight));
	} 

	/**
	* @return modulated shadow projection bound shader state for this light type
	*/
	virtual FGlobalBoundShaderState* GetModShadowProjBoundShaderState(UBOOL bRenderingBeforeLight) const
	{
		FGlobalBoundShaderState* CurrentBoundShaderState = ChooseBoundShaderState(GetEffectiveShadowFilterQuality(bRenderingBeforeLight),ModShadowProjBoundShaderStates);
		return CurrentBoundShaderState;
	}

	/**
	* @return PCF Branching modulated shadow projection bound shader state for this light type
	*/
	virtual FGlobalBoundShaderState* GetBranchingPCFModProjBoundShaderState(UBOOL bRenderingBeforeLight) const
	{
		//allow the BPCF implementation to choose which of the loaded bound shader states should be used
		FGlobalBoundShaderState* CurrentBPCFBoundShaderState = ChooseBoundShaderState(GetEffectiveShadowFilterQuality(bRenderingBeforeLight),ModBranchingPCFBoundShaderStates);

		return CurrentBPCFBoundShaderState;
	}

private:

	/** The DPG info for the directional light. */
	TLightSceneDPGInfo<FDirectionalLightPolicy> DPGInfos[SDPG_MAX_SceneRender];
};

/**
 * Called after property has changed via e.g. property window or set command.
 *
 * @param	PropertyThatChanged	UProperty that has been changed, NULL if unknown
 */
void UDirectionalLightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	LightmassSettings.LightSourceAngle = Max(LightmassSettings.LightSourceAngle, 0.0f);
	LightmassSettings.IndirectLightingScale = Max(LightmassSettings.IndirectLightingScale, 0.0f);
	LightmassSettings.IndirectLightingSaturation = Max(LightmassSettings.IndirectLightingSaturation, 0.0f);
	LightmassSettings.ShadowExponent = Clamp(LightmassSettings.ShadowExponent, .5f, 8.0f);

	WholeSceneDynamicShadowRadius = Max(WholeSceneDynamicShadowRadius, 0.0f);
	NumWholeSceneDynamicShadowCascades = Clamp(NumWholeSceneDynamicShadowCascades, 0, 10);
	CascadeDistributionExponent = Clamp(CascadeDistributionExponent, .1f, 10.0f);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FLightSceneInfo* UDirectionalLightComponent::CreateSceneInfo() const
{
	return new FDirectionalLightSceneInfo(this);
}

FVector4 UDirectionalLightComponent::GetPosition() const
{
	return FVector4(-GetDirection() * TraceDistance, 0 );
}

/**
* @return ELightComponentType for the light component class 
*/
ELightComponentType UDirectionalLightComponent::GetLightType() const
{
	return LightType_Directional;
}

void UDominantDirectionalLightComponent::Serialize(FArchive& Ar)
{
	if (Ar.Ver() >= VER_DOMINANTLIGHT_NORMALSHADOWS)
	{
		Ar << DominantLightShadowMap;
		if (Ar.IsLoading())
		{
			INC_DWORD_STAT_BY(STAT_DominantShadowTransitionMemory, DominantLightShadowMap.GetAllocatedSize());
		}
	}
	
	Super::Serialize(Ar);
}

ELightComponentType UDominantDirectionalLightComponent::GetLightType() const
{
	return LightType_DominantDirectional;
}

void UDominantDirectionalLightComponent::InvalidateLightingCache()
{
	DominantLightShadowInfo.ShadowMapSizeX = DominantLightShadowInfo.ShadowMapSizeY = 0;
	DEC_DWORD_STAT_BY(STAT_DominantShadowTransitionMemory, DominantLightShadowMap.GetAllocatedSize());
	DominantLightShadowMap.Empty();
	Super::InvalidateLightingCache();
}

void UDominantDirectionalLightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	bForceDynamicLight = FALSE;
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UDominantDirectionalLightComponent::FinishDestroy()
{
	DEC_DWORD_STAT_BY(STAT_DominantShadowTransitionMemory, DominantLightShadowMap.GetAllocatedSize());
	Super::FinishDestroy();
}

/** Returns information about the data used to calculate dominant shadow transition distance. */
void UDominantDirectionalLightComponent::GetInfo(INT& SizeX, INT& SizeY, SIZE_T& ShadowMapBytes) const
{
	SizeX = DominantLightShadowInfo.ShadowMapSizeX;
	SizeY = DominantLightShadowInfo.ShadowMapSizeY;
	ShadowMapBytes = DominantLightShadowMap.GetAllocatedSize();
}

/** Populates DominantLightShadowMap and DominantLightShadowInfo with the results from a lighting build. */
void UDominantDirectionalLightComponent::Initialize(const FDominantShadowInfo& InInfo, const TArray<WORD>& InShadowMap, UBOOL bOnlyVisible)
{
	checkSlow(InShadowMap.Num() > 0);
	if (bOnlyVisible
		&& DominantLightShadowInfo.WorldToLight == InInfo.WorldToLight 
		&& DominantLightShadowInfo.LightSpaceImportanceBounds.GetCenter() == InInfo.LightSpaceImportanceBounds.GetCenter()
		&& DominantLightShadowInfo.LightSpaceImportanceBounds.GetExtent() == InInfo.LightSpaceImportanceBounds.GetExtent()
		&& DominantLightShadowInfo.ShadowMapSizeX == InInfo.ShadowMapSizeX
		&& DominantLightShadowInfo.ShadowMapSizeY == InInfo.ShadowMapSizeY
		&& DominantLightShadowMap.Num() == InShadowMap.Num())
	{
		// Try merging dominant shadow map...
		for (INT Idx = 0; Idx < DominantLightShadowMap.Num(); ++Idx)
		{
			DominantLightShadowMap(Idx) = Min<WORD>(InShadowMap(Idx), DominantLightShadowMap(Idx));
		}
	}
	else
	{
		DominantLightShadowMap = InShadowMap;
	}
	checkSlow(InInfo.ShadowMapSizeX > 0 && InInfo.ShadowMapSizeY > 0);
	DominantLightShadowInfo = InInfo;

	INC_DWORD_STAT_BY(STAT_DominantShadowTransitionMemory, DominantLightShadowMap.GetAllocatedSize());
}

/** Returns the distance to the nearest dominant shadow transition, in world space units, starting from the edge of the bounds. */
FLOAT UDominantDirectionalLightComponent::GetDominantShadowTransitionDistance(
	const FBoxSphereBounds& Bounds, 
	FLOAT MaxSearchDistance, 
	UBOOL bDebugSearch, 
	TArray<FDebugShadowRay>& DebugRays,
	UBOOL& bLightingIsBuilt) const
{
	if (DominantLightShadowMap.Num() > 0)
	{
		bLightingIsBuilt = TRUE;
		checkSlow(DominantLightShadowInfo.ShadowMapSizeX > 0 && DominantLightShadowInfo.ShadowMapSizeY > 0);
		// Transform the bounds origin into the coordinate space that the shadow map entries are stored in
		const FVector LightSpaceOrigin = DominantLightShadowInfo.WorldToLight.TransformFVector(Bounds.Origin);
		if (LightSpaceOrigin.Z + Bounds.SphereRadius < DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Z)
		{
			// The query is closer to the light than the start depth of the shadow map, return unshadowed
			return 0.0f;
		}
		const FLOAT XScale = 1.0f / (DominantLightShadowInfo.LightSpaceImportanceBounds.Max.X - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.X);
		const FLOAT YScale = 1.0f / (DominantLightShadowInfo.LightSpaceImportanceBounds.Max.Y - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Y);

		// 2D radius of a single shadowmap cell
		const FLOAT ShadowMapCellRadius = .5f * FVector2D(
			(DominantLightShadowInfo.LightSpaceImportanceBounds.Max.X - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.X) / DominantLightShadowInfo.ShadowMapSizeX,
			(DominantLightShadowInfo.LightSpaceImportanceBounds.Max.Y - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Y) / DominantLightShadowInfo.ShadowMapSizeY).Size();

		// Calculate the coordinates of the box containing the search bounds in the shadow map
		const INT MinShadowMapX = Max(appTrunc(DominantLightShadowInfo.ShadowMapSizeX * (LightSpaceOrigin.X - Bounds.SphereRadius - MaxSearchDistance - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.X) * XScale), 0);
		const INT MaxShadowMapX = Min(appTrunc(DominantLightShadowInfo.ShadowMapSizeX * (LightSpaceOrigin.X + Bounds.SphereRadius + MaxSearchDistance - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.X) * XScale), DominantLightShadowInfo.ShadowMapSizeX - 1);
		const INT MinShadowMapY = Max(appTrunc(DominantLightShadowInfo.ShadowMapSizeY * (LightSpaceOrigin.Y - Bounds.SphereRadius - MaxSearchDistance - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Y) * YScale), 0);
		const INT MaxShadowMapY = Min(appTrunc(DominantLightShadowInfo.ShadowMapSizeY * (LightSpaceOrigin.Y + Bounds.SphereRadius + MaxSearchDistance - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Y) * YScale), DominantLightShadowInfo.ShadowMapSizeY - 1);

		if (MinShadowMapX >= MaxShadowMapX || MinShadowMapY >= MaxShadowMapY)
		{
			// The query is outside the shadow map, return unshadowed
			return 0.0f;
		}

		// Only attempt the early out when there is going to be a slow area search
		if ((MaxShadowMapX - MinShadowMapX) * (MaxShadowMapY - MinShadowMapY) > 25)
		{
			const INT ObjectCenterShadowMapX = Clamp(appTrunc(DominantLightShadowInfo.ShadowMapSizeX * (LightSpaceOrigin.X - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.X) * XScale), 0, DominantLightShadowInfo.ShadowMapSizeX - 1);
			const INT ObjectCenterShadowMapY = Clamp(appTrunc(DominantLightShadowInfo.ShadowMapSizeY * (LightSpaceOrigin.Y - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Y) * YScale), 0, DominantLightShadowInfo.ShadowMapSizeY - 1);

			// Check to see if the distance to the nearest ray to the center of the object is nearly 0, so we can avoid doing an area search
			const WORD QuantizedCurrentSampleDistance = DominantLightShadowMap(ObjectCenterShadowMapY * DominantLightShadowInfo.ShadowMapSizeX + ObjectCenterShadowMapX);
			const FLOAT CurrentSampleDistance = QuantizedCurrentSampleDistance / 65535.0f * (DominantLightShadowInfo.LightSpaceImportanceBounds.Max.Z - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Z);
			const FLOAT LightSpaceXPosition = ObjectCenterShadowMapX / (FLOAT)(DominantLightShadowInfo.ShadowMapSizeX - 1) * (DominantLightShadowInfo.LightSpaceImportanceBounds.Max.X - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.X) + DominantLightShadowInfo.LightSpaceImportanceBounds.Min.X;
			const FLOAT LightSpaceYPosition = ObjectCenterShadowMapY / (FLOAT)(DominantLightShadowInfo.ShadowMapSizeY - 1) * (DominantLightShadowInfo.LightSpaceImportanceBounds.Max.Y - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Y) + DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Y;

			const FVector CurrentSamplePosition(LightSpaceXPosition, LightSpaceYPosition, Min(LightSpaceOrigin.Z, DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Z + CurrentSampleDistance));
			const FLOAT CurrentDistance = Max((CurrentSamplePosition - LightSpaceOrigin).Size() - ShadowMapCellRadius - Bounds.SphereRadius, 0.0f);

			if (CurrentDistance < KINDA_SMALL_NUMBER)
			{

#if !FINAL_RELEASE
				if (bDebugSearch)
				{
					DebugRays.AddItem(FDebugShadowRay(
						DominantLightShadowInfo.LightToWorld.TransformFVector(FVector(LightSpaceXPosition, LightSpaceYPosition, DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Z)),
						DominantLightShadowInfo.LightToWorld.TransformFVector(FVector(LightSpaceXPosition, LightSpaceYPosition, CurrentSampleDistance + DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Z)),
						FALSE));
				}
#endif
				// The center ray went through the object, no need to search further
				return 0;
			}
		}

		FLOAT MinTransitionDistance = MaxSearchDistance; 
		UBOOL bFoundSample = FALSE;
		FVector ClosestSamplePosition(0,0,0);
		INT ClosestSampleIndex = INDEX_NONE;
		// Search the box containing the search bounds for the closest shadow transition
		for (INT Y = MinShadowMapY; Y <= MaxShadowMapY && (MinTransitionDistance > 0.0f || bDebugSearch); Y++)
		{
			const FLOAT LightSpaceYPosition = Y / (FLOAT)(DominantLightShadowInfo.ShadowMapSizeY - 1) * (DominantLightShadowInfo.LightSpaceImportanceBounds.Max.Y - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Y) + DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Y;
			for (INT X = MinShadowMapX; X <= MaxShadowMapX && (MinTransitionDistance > 0.0f || bDebugSearch); X++)
			{
				const WORD QuantizedCurrentSampleDistance = DominantLightShadowMap(Y * DominantLightShadowInfo.ShadowMapSizeX + X);
				const FLOAT CurrentSampleDistance = QuantizedCurrentSampleDistance / 65535.0f * (DominantLightShadowInfo.LightSpaceImportanceBounds.Max.Z - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Z);
				const FLOAT LightSpaceXPosition = X / (FLOAT)(DominantLightShadowInfo.ShadowMapSizeX - 1) * (DominantLightShadowInfo.LightSpaceImportanceBounds.Max.X - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.X) + DominantLightShadowInfo.LightSpaceImportanceBounds.Min.X;
				const FVector CurrentSamplePosition(LightSpaceXPosition, LightSpaceYPosition, Min(LightSpaceOrigin.Z, DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Z + CurrentSampleDistance));
				const FLOAT CurrentDistance = Max((CurrentSamplePosition - LightSpaceOrigin).Size() - ShadowMapCellRadius - Bounds.SphereRadius, 0.0f);

#if !FINAL_RELEASE
				if (bDebugSearch)
				{
					const INT CurrentIndex = DebugRays.AddItem(FDebugShadowRay(
						DominantLightShadowInfo.LightToWorld.TransformFVector(FVector(LightSpaceXPosition, LightSpaceYPosition, DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Z)),
						DominantLightShadowInfo.LightToWorld.TransformFVector(FVector(LightSpaceXPosition, LightSpaceYPosition, CurrentSampleDistance + DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Z)),
						FALSE));

					if (CurrentDistance < MinTransitionDistance)
					{
						ClosestSampleIndex = CurrentIndex;
						ClosestSamplePosition = CurrentSamplePosition;
					}
				}
#endif

				// Find the minimum distance in the light space XY plane from the edge of the bounds to a visible shadow map cell
				if (CurrentDistance < MinTransitionDistance)
				{
					MinTransitionDistance = CurrentDistance;
					bFoundSample = TRUE;
				}
			}
		}

#if !FINAL_RELEASE
		if (bDebugSearch && bFoundSample)
		{
			checkSlow(ClosestSampleIndex != INDEX_NONE);
			DebugRays(ClosestSampleIndex).bHit = TRUE;
			DebugRays.AddItem(FDebugShadowRay(
				DominantLightShadowInfo.LightToWorld.TransformFVector(ClosestSamplePosition),
				Bounds.Origin,
				MinTransitionDistance < MaxSearchDistance));
		}
#endif

		return MinTransitionDistance;
	}
	bLightingIsBuilt = GetOwner()->bMovable;
	return 0.0f;
}
