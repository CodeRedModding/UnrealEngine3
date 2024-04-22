/*=============================================================================
	PointLightComponent.cpp: PointLightComponent implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "LightRendering.h"
#include "PointLightSceneInfo.h"

IMPLEMENT_CLASS(UPointLightComponent);
IMPLEMENT_CLASS(UDominantPointLightComponent);

/**
 * The point light policy for TMeshLightingDrawingPolicy.
 */
class FPointLightPolicy
{
public:
	typedef TPointLightSceneInfo<FPointLightPolicy> SceneInfoType;
	class VertexParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			LightPositionAndInvRadiusParameter.Bind(ParameterMap,TEXT("LightPositionAndInvRadius"));
		}

		template<typename ShaderRHIParamRef>
		void SetLight(ShaderRHIParamRef Shader,const TPointLightSceneInfo<FPointLightPolicy>* Light,const FSceneView* View) const;

		void Serialize(FArchive& Ar)
		{
			Ar << LightPositionAndInvRadiusParameter;
			
			// set parameter names for platforms that need them
			LightPositionAndInvRadiusParameter.SetShaderParamName(TEXT("LightPositionAndInvRadius"));
		}
	private:
		FShaderParameter LightPositionAndInvRadiusParameter;
	};

	class PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			LightColorAndFalloffExponentParameter.Bind(ParameterMap,TEXT("LightColorAndFalloffExponent"),TRUE);
		}
		void SetLight(FShader* PixelShader,const TPointLightSceneInfo<FPointLightPolicy>* Light,const FSceneView* View) const {}
		void SetLightMesh(FShader* PixelShader,const FPrimitiveSceneInfo* PrimitiveSceneInfo,const SceneInfoType* Light,UBOOL bApplyLightFunctionDisabledBrightness) const
		{
			FLOAT ShadowFactor = IsDominantLightType(Light->LightType) ? PrimitiveSceneInfo->DominantShadowFactor : 1.0f;
			if (bApplyLightFunctionDisabledBrightness)
			{
				ShadowFactor *= Light->LightFunctionDisabledBrightness;
			}
			SetPixelShaderValue(
				PixelShader->GetPixelShader(),
				LightColorAndFalloffExponentParameter,
				FVector4(
					Light->Color.R * ShadowFactor,
					Light->Color.G * ShadowFactor,
					Light->Color.B * ShadowFactor,
					Light->FalloffExponent
					)
				);
		}
		void Serialize(FArchive& Ar)
		{
			Ar << LightColorAndFalloffExponentParameter;
			
			// set parameter names for platforms that need them
			LightColorAndFalloffExponentParameter.SetShaderParamName(TEXT("LightColorAndFalloffExponent"));
		}
	private:
		FShaderParameter LightColorAndFalloffExponentParameter;
	};

	/**
	* Modulated shadow shader params associated with this light policy
	*/
	class ModShadowPixelParamsType
	{
	public:
		void Bind( const FShaderParameterMap& ParameterMap )
		{
			LightPositionParam.Bind(ParameterMap,TEXT("LightPosition"));
			FalloffParameters.Bind(ParameterMap,TEXT("FalloffParameters"));
		}
		void SetModShadowLight( FShader* PixelShader, const TPointLightSceneInfo<FPointLightPolicy>* Light,const FSceneView* View) const;
		void Serialize( FArchive& Ar )
		{
			Ar << LightPositionParam;
			Ar << FalloffParameters;
		}
		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
			OutEnvironment.Definitions.Set(TEXT("MODSHADOW_LIGHTTYPE_POINT"),TEXT("1"));
		}
	private:
		/** world position of light casting a shadow. Note: w = 1.0 / Radius */
		FShaderParameter LightPositionParam;
		/** attenuation exponent for light casting a shadow */
		FShaderParameter FalloffParameters;
	};

	static UBOOL ShouldCacheStaticLightingShaders()
	{
		return TRUE;
	}

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return TRUE;
	}
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment) {}
};

//
template<typename ShaderRHIParamRef>
void FPointLightPolicy::VertexParametersType::SetLight(ShaderRHIParamRef Shader,const TPointLightSceneInfo<FPointLightPolicy>* Light,const FSceneView* View) const
{
	SetShaderValue(
		Shader,
		LightPositionAndInvRadiusParameter,
		FVector4(
			Light->GetOrigin() + View->PreViewTranslation,
			Light->InvRadius
			)
		);
}

void FPointLightPolicy::ModShadowPixelParamsType::SetModShadowLight(FShader* PixelShader,const TPointLightSceneInfo<FPointLightPolicy>* Light,const FSceneView* View) const
{
	// set world light position and falloff rate
	SetPixelShaderValue(
		PixelShader->GetPixelShader(),
		LightPositionParam,
		FVector4(
			Light->GetOrigin() + View->PreViewTranslation,
			1.0f / Light->Radius
			)
		);
	SetPixelShaderValue(
		PixelShader->GetPixelShader(),
		FalloffParameters, 
		FVector(
			Light->ShadowFalloffExponent,
			Light->ShadowFalloffScale,
			Light->ShadowFalloffBias
			)
		);
}

IMPLEMENT_LIGHT_SHADER_TYPE(FPointLightPolicy,TEXT("PointLightVertexShader"),TEXT("PointLightPixelShader"),VER_TRANSLUCENT_PRESHADOWS,0);

void FPointLightSceneInfoBase::UpdateRadius_GameThread(UPointLightComponent* Component)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		UpdateRadius,
		FPointLightSceneInfoBase*,LightSceneInfo,this,
		FLOAT,ComponentRadius,Component->Radius,
		FLOAT,ComponentMinShadowFalloffRadius,Component->MinShadowFalloffRadius,
	{
		LightSceneInfo->UpdateRadius(ComponentRadius,ComponentMinShadowFalloffRadius);
	});
}

FLinearColor FPointLightSceneInfoBase::GetDirectIntensity(const FVector& Point) const
{
	FLOAT RadialAttenuation = appPow( Max(1.0f - ((GetOrigin() - Point) / Radius).SizeSquared(),0.0f), FalloffExponent );
	return Color * RadialAttenuation;
}

class FPointLightSceneInfo : public TPointLightSceneInfo<FPointLightPolicy>
{
public:

	/** Plane used for planar shadows on mobile.  */
	FPlane ShadowPlane;

	virtual FPlane GetShadowPlane() const 
	{ 
		return ShadowPlane; 
	}

	/** Accesses parameters needed for rendering the light. */
	virtual void GetParameters(FVector4& LightPositionAndInvRadius, FVector4& LightColorAndFalloffExponent, FVector& LightDirection, FVector2D& SpotAngles) const
	{
		LightPositionAndInvRadius = FVector4(
			GetOrigin(),
			InvRadius);

		LightColorAndFalloffExponent = FVector4(
			Color.R,
			Color.G,
			Color.B,
			FalloffExponent);

		LightDirection = FVector(0, 0, 0);
		SpotAngles = FVector2D(0, 0);
	}

	virtual void DetachPrimitive(const FLightPrimitiveInteraction& Interaction) 
	{
		DetachPrimitiveShared(Interaction);
	}

	virtual void AttachPrimitive(const FLightPrimitiveInteraction& Interaction)
	{
		AttachPrimitiveShared(Interaction);
	}

	/**
	 * Sets up a projected shadow initializer for shadows from the entire scene.
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual UBOOL GetWholeSceneProjectedShadowInitializer(const TArray<FViewInfo>& Views, TArray<FProjectedShadowInitializer, TInlineAllocator<6> >& OutInitializers) const
	{
		UBOOL bAnyTransformsCreated = FALSE;

		if (GRenderOnePassPointLightShadows && GRHIShaderPlatform == SP_PCD3D_SM5)
		{
			OutInitializers.Add();

			// Create a single shadow projection that will render to a cube map, marking it as a one pass point light shadow
			bAnyTransformsCreated = OutInitializers.Last().CalcWholeSceneShadowTransforms(
				-LightToWorld.GetOrigin(),
				WorldToLight.RemoveTranslation(),
				FVector(0,0,1),
				FBoxSphereBounds(
					FVector(0, 0, 0),
					FVector(Radius,Radius,Radius),
					Radius
					),
				FVector4(0,0,1,0),
				0.1f,
				Radius,
				FALSE,
				TRUE,
				INDEX_NONE
				);
		}
		else
		{
			// 6 directions of the 90 degree projections that will be used to render whole scene shadows for this light
			static const FVector Directions[6] = {
				FVector(0,0,1),
				FVector(0,0,-1),
				FVector(0,1,0),
				FVector(0,-1,0),
				FVector(1,0,0),
				FVector(-1,0,0)
			};

			const UINT EffectiveMinShadowResolution = (MinShadowResolution > 0) ? MinShadowResolution : GSystemSettings.MinShadowResolution;
			// Create a scale factor that will make the point light frustums overlap slightly to hide artifacts where the projections meet
			// Ideally we want to overlap by one texel on each projection side, but we don't know the resolution that will be used at this point,
			// So handle the worst case by overlapping one texel of the minimum resolution.
			const FLOAT ResolutionScale = (FLOAT)(EffectiveMinShadowResolution - 1) / EffectiveMinShadowResolution;

			const FVector Scales[6] = {
				FVector(-ResolutionScale, ResolutionScale, 1),
				FVector(-ResolutionScale, ResolutionScale, 1),
				FVector(-ResolutionScale, 1, ResolutionScale),
				FVector(-ResolutionScale, 1, ResolutionScale),
				FVector(1, -ResolutionScale, ResolutionScale),
				FVector(1, -ResolutionScale, ResolutionScale)
			};

			for (INT DirectionIndex = 0; DirectionIndex < 6; DirectionIndex++)
			{
				const FBoxSphereBounds Bounds(
					LightToWorld.RemoveTranslation().TransformFVector(Directions[DirectionIndex] * FVector(Radius / 2.0f, Radius / 2.0f, Radius / 2.0f)),
					FVector(Radius/2.0f,Radius/2.0f,Radius/2.0f),
					Radius / 2.0f);

				// Only render projections that are in the view frustum
				UBOOL bInAnyViewFrustum = FALSE;
				for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					if (Views(ViewIndex).ViewFrustum.IntersectBox(Bounds.Origin + LightToWorld.GetOrigin(), Bounds.BoxExtent))
					{
						bInAnyViewFrustum = TRUE;
						break;
					}
				}

				if (bInAnyViewFrustum)
				{
					OutInitializers.Add(1);
					const UBOOL bCreatedTransforms = OutInitializers.Last().CalcWholeSceneShadowTransforms(
						-LightToWorld.GetOrigin(),
						WorldToLight.RemoveTranslation() * 
							FScaleMatrix(Scales[DirectionIndex]),
						Directions[DirectionIndex],
						Bounds,
						FVector4(0,0,1,0),
						0.1f,
						Radius,
						FALSE,
						FALSE,
						DirectionIndex
						);
					bAnyTransformsCreated = bAnyTransformsCreated || bCreatedTransforms;
				}
			}
		}
		return bAnyTransformsCreated;
	}

	FPointLightSceneInfo(const UPointLightComponent* Component) :
		TPointLightSceneInfo<FPointLightPolicy>(Component),
		ShadowPlane(Component->ShadowPlane)
	{}
};

//
FLightSceneInfo* UPointLightComponent::CreateSceneInfo() const
{
	return new FPointLightSceneInfo(this);
}

//
UBOOL UPointLightComponent::AffectsBounds(const FBoxSphereBounds& Bounds) const
{
	if((Bounds.Origin - LightToWorld.GetOrigin()).SizeSquared() > Square(Radius + Bounds.SphereRadius))
	{
		return FALSE;
	}

	if(!Super::AffectsBounds(Bounds))
	{
		return FALSE;
	}

	return TRUE;
}

//
void UPointLightComponent::SetTransformedToWorld()
{
	// apply our translation to the parent matrix

	LightToWorld = FMatrix(
						FPlane(+0,+0,+1,+0),
						FPlane(+0,+1,+0,+0),
						FPlane(+1,+0,+0,+0),
						FPlane(+0,+0,+0,+1)
						) *
					FTranslationMatrix(Translation) * 
					CachedParentToWorld;
	LightToWorld.RemoveScaling();
	WorldToLight = LightToWorld.InverseSafe();
}

//
void UPointLightComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	CachedParentToWorld = ParentToWorld;
}

//
void UPointLightComponent::UpdatePreviewLightRadius()
{
	if ( PreviewLightRadius )
	{
		PreviewLightRadius->SphereRadius = Radius;
		PreviewLightRadius->Translation = Translation;
	}
}

/** Update the PreviewLightSourceRadius */
void UPointLightComponent::UpdatePreviewLightSourceRadius()
{
	if (PreviewLightSourceRadius)
	{
		if (GWorld && GWorld->GetWorldInfo() && (GWorld->GetWorldInfo()->bUseGlobalIllumination == TRUE))
		{
			PreviewLightSourceRadius->SphereRadius = LightmassSettings.LightSourceRadius;
			PreviewLightSourceRadius->Translation = Translation;
		}
		else
		{
			PreviewLightSourceRadius->SphereRadius = 0.0f;
		}
	}
}

//
void UPointLightComponent::Attach()
{
	// Call SetTransformedToWorld before ULightComponent::Attach, so the FLightSceneInfo is constructed with the right transform.
	SetTransformedToWorld();

	UpdatePreviewLightRadius();
	UpdatePreviewLightSourceRadius();

	Super::Attach();
}

//
void UPointLightComponent::UpdateTransform()
{
	SetTransformedToWorld();

	// Update the scene info's cached radius-dependent data.
	if(SceneInfo)
	{
		((FPointLightSceneInfoBase*)SceneInfo)->UpdateRadius_GameThread(this);
	}

	Super::UpdateTransform();

	UpdatePreviewLightRadius();
}

//
FVector4 UPointLightComponent::GetPosition() const
{
	return FVector4(LightToWorld.GetOrigin(),1);
}

/**
* @return ELightComponentType for the light component class 
*/
ELightComponentType UPointLightComponent::GetLightType() const
{
	return LightType_Point;
}

//
FBox UPointLightComponent::GetBoundingBox() const
{
	return FBox(GetOrigin() - FVector(Radius,Radius,Radius),GetOrigin() + FVector(Radius,Radius,Radius));
}

//
FLinearColor UPointLightComponent::GetDirectIntensity(const FVector& Point) const
{
	FLOAT RadialAttenuation = appPow( Max(1.0f - ((GetOrigin() - Point) / Radius).SizeSquared(),0.0f), FalloffExponent );
	return Super::GetDirectIntensity(Point) * RadialAttenuation;
}

//
void UPointLightComponent::SetTranslation(FVector NewTranslation)
{
	Translation = NewTranslation;
	BeginDeferredUpdateTransform();
}

/**
 * Called after property has changed via e.g. property window or set command.
 *
 * @param	PropertyThatChanged	UProperty that has been changed, NULL if unknown
 */
void UPointLightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Make sure exponent is > 0.
	FalloffExponent = Max( (FLOAT) KINDA_SMALL_NUMBER, FalloffExponent );
	LightmassSettings.LightSourceRadius = Max(LightmassSettings.LightSourceRadius, 0.0f);
	LightmassSettings.IndirectLightingScale = Max(LightmassSettings.IndirectLightingScale, 0.0f);
	LightmassSettings.IndirectLightingSaturation = Max(LightmassSettings.IndirectLightingSaturation, 0.0f);
	LightmassSettings.ShadowExponent = Clamp(LightmassSettings.ShadowExponent, .5f, 8.0f);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UPointLightComponent::PostLoad()
{
	Super::PostLoad();
	if ( GIsEditor
	&& !IsTemplate(RF_ClassDefaultObject)

	// FReloadObjectArcs call PostLoad() *prior* to instancing components for objects being reloaded, so this check is invalid in that case
	&& (GUglyHackFlags&HACK_IsReloadObjArc) == 0 )
	{
		if ( PreviewLightRadius && PreviewLightRadius->GetOuter() != GetOuter() )
		{
			// so if we are here, then the owning light actor was definitely created after the fixup code was added, so there is some way that this bug is still occurring
			// I need to figure out how this is occurring so let's annoy the designer into bugging me.
			debugf(TEXT("PointLightComponent %s has an invalid PreviewLightRadius '%s' even though package has been resaved since this bug was fixed.  Please let Ron know about this immediately!"), *GetFullName(), *PreviewLightRadius->GetFullName());
			//@todo ronp - remove this once we've verified that this is no longer occurring.
			appMsgf(AMT_OK, TEXT("PointLightComponent %s has an invalid PreviewLightRadius '%s' even though package has been resaved since this bug was fixed.  Please let Ron know about this immediately!"), *GetFullName(), *PreviewLightRadius->GetFullName());
		}
	}
}

ELightComponentType UDominantPointLightComponent::GetLightType() const
{
	return LightType_DominantPoint;
}

void UDominantPointLightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	bForceDynamicLight = FALSE;
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/** Returns the distance to the nearest dominant shadow transition, in world space units, starting from the edge of the bounds. */
FLOAT UDominantPointLightComponent::GetDominantShadowTransitionDistance(
	const FBoxSphereBounds& Bounds, 
	FLOAT MaxSearchDistance, 
	UBOOL bDebugSearch, 
	TArray<FDebugShadowRay>& DebugRays,
	UBOOL& bLightingIsBuilt) const
{
	// Only returning distance to the point light's influence for now, visibility is not handled
	const FLOAT CenterToCenterDistance = (Bounds.Origin - LightToWorld.GetOrigin()).Size();
	bLightingIsBuilt = TRUE;
	return Clamp(CenterToCenterDistance - Bounds.SphereRadius * 2.0f - Radius, 0.0f, MaxSearchDistance);
}

void FDirectionalLightLightMapPolicy::SetMesh(
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
	const FLightSceneInfo* const Light = ElementData.Light;
	// Light can be NULL if we are doing approximate one pass lighting for translucency, and the DLE did not create a directional light.
	// See FDrawTranslucentMeshAction::GetTranslucencyMergedDynamicLightInfo
	if (Light)
	{
		const FVector LightVector = ((FVector)Light->Position - PrimitiveSceneInfo->Bounds.Origin * Light->Position.W).SafeNormal();
		SetVertexShaderValue(
			VertexShader->GetVertexShader(),
			VertexShaderParameters->LightPositionAndInvRadiusParameter,
			FVector4(-Light->GetDirection(),0.0f)
			);

		if (PixelShaderParameters)
		{
			// Support emulating a point or spot light with this directional light by evaluating the light attenuation at the bounds origin
			const FLinearColor LightColor = Light->GetDirectIntensity(PrimitiveSceneInfo->Bounds.Origin) 
				* (IsDominantLightType(Light->LightType) ? PrimitiveSceneInfo->DominantShadowFactor : 1.0f);
			SetPixelShaderValue(PixelShader->GetPixelShader(), PixelShaderParameters->LightColorParameter, FVector4(LightColor.R, LightColor.G, LightColor.B, 0.0f));

			// Support forward shadowing for translucency
			PixelShaderParameters->ForwardShadowingParameters.SetReceiveShadows(PixelShader, ElementData.bReceiveDynamicShadows);
			PixelShaderParameters->ForwardShadowingParameters.Set(View, PixelShader, ElementData.bOverrideDynamicShadowsOnTranslucency, ElementData.TranslucentPreShadowInfo);
		}
	}
	else
	{
		// No extra shader permutation is compiled to handle an SH light without a directional light, so we have to handle a NULL directional light here.
		SetVertexShaderValue(
			VertexShader->GetVertexShader(),
			VertexShaderParameters->LightPositionAndInvRadiusParameter,
			FVector4(0,0,1,0)
			);

		if (PixelShaderParameters)
		{
			SetPixelShaderValue(PixelShader->GetPixelShader(), PixelShaderParameters->LightColorParameter, FVector4(0.0f, 0.0f, 0.0f, 0.0f));
			PixelShaderParameters->ForwardShadowingParameters.SetReceiveShadows(PixelShader, FALSE);
		}
	}
}
