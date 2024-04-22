/*=============================================================================
	SpotLightComponent.cpp: LightComponent implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "LightRendering.h"
#include "PointLightSceneInfo.h"

IMPLEMENT_CLASS(USpotLightComponent);
IMPLEMENT_CLASS(UDominantSpotLightComponent);

/**
 * The spot light policy for TMeshLightingDrawingPolicy.
 */
class FSpotLightPolicy
{
public:
	typedef class FSpotLightSceneInfo SceneInfoType;
	class VertexParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			LightPositionAndInvRadiusParameter.Bind(ParameterMap,TEXT("LightPositionAndInvRadius"));
		}
		template<typename ShaderRHIParamRef>
		void SetLight(ShaderRHIParamRef Shader,const FSpotLightSceneInfo* Light,const FSceneView* View) const;
		void Serialize(FArchive& Ar)
		{
			Ar << LightPositionAndInvRadiusParameter;
		}
	private:
		FShaderParameter LightPositionAndInvRadiusParameter;
	};

	class PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			SpotAnglesParameter.Bind(ParameterMap,TEXT("SpotAngles"),TRUE);
			SpotDirectionParameter.Bind(ParameterMap,TEXT("SpotDirection"),TRUE);
			LightColorAndFalloffExponentParameter.Bind(ParameterMap,TEXT("LightColorAndFalloffExponent"),TRUE);
		}
		void SetLight(FShader* PixelShader,const FSpotLightSceneInfo* Light,const FSceneView* View) const;
		void SetLightMesh(FShader* PixelShader,const FPrimitiveSceneInfo* PrimitiveSceneInfo,const SceneInfoType* Light,UBOOL bApplyLightFunctionDisabledBrightness) const;
		void Serialize(FArchive& Ar)
		{
			Ar << SpotAnglesParameter;
			Ar << SpotDirectionParameter;
			Ar << LightColorAndFalloffExponentParameter;
		}
	private:
		FShaderParameter SpotAnglesParameter;
		FShaderParameter SpotDirectionParameter;
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
			SpotDirectionParam.Bind(ParameterMap,TEXT("SpotDirection"),TRUE);
			SpotAnglesParam.Bind(ParameterMap,TEXT("SpotAngles"),TRUE);
		}
		void SetModShadowLight( FShader* PixelShader, const FSpotLightSceneInfo* Light,const FSceneView* View ) const;
		void Serialize( FArchive& Ar )
		{
			Ar << LightPositionParam;
			Ar << FalloffParameters;
			Ar << SpotDirectionParam;
			Ar << SpotAnglesParam;
		}
		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
			OutEnvironment.Definitions.Set(TEXT("MODSHADOW_LIGHTTYPE_SPOT"),TEXT("1"));
		}
	private:
		/** world position of light casting a shadow. Note: w = 1.0 / Radius */
		FShaderParameter LightPositionParam;
		/** attenuation exponent for light casting a shadow */
		FShaderParameter FalloffParameters;
		/** spot light direction vector in world space */
		FShaderParameter SpotDirectionParam;
		/** spot light cone cut-off angles */
		FShaderParameter SpotAnglesParam;
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

IMPLEMENT_LIGHT_SHADER_TYPE(FSpotLightPolicy,TEXT("SpotLightVertexShader"),TEXT("SpotLightPixelShader"),VER_TRANSLUCENT_PRESHADOWS,0);

/**
 * The scene info for a spot light.
 */
class FSpotLightSceneInfo : public TPointLightSceneInfo<FSpotLightPolicy>
{
public:

	/** Outer cone angle in radians, clamped to a valid range. */
	FLOAT OuterConeAngle;

	/** Cosine of the spot light's inner cone angle. */
	FLOAT CosInnerCone;

	/** Cosine of the spot light's outer cone angle. */
	FLOAT CosOuterCone;

	/** 1 / (CosInnerCone - CosOuterCone) */
	FLOAT InvCosConeDifference;

	/** Sine of the spot light's outer cone angle. */
	FLOAT SinOuterCone;

	/** 1 / Tangent of the spot light's outer cone angle. */
	FLOAT InvTanOuterCone;

	/** Cosine of the spot light's outer light shaft cone angle. */
	FLOAT CosLightShaftConeAngle;

	/** 1 / (appCos(ClampedInnerLightShaftConeAngle) - CosLightShaftConeAngle) */
	FLOAT InvCosLightShaftConeDifference;

	/** Initialization constructor. */
	FSpotLightSceneInfo(const USpotLightComponent* Component):
		TPointLightSceneInfo<FSpotLightPolicy>(Component)
	{
		const FLOAT ClampedInnerConeAngle = Clamp(Component->InnerConeAngle,0.0f,89.0f) * (FLOAT)PI / 180.0f;
		const FLOAT ClampedOuterConeAngle = Clamp(Component->OuterConeAngle * (FLOAT)PI / 180.0f,ClampedInnerConeAngle + 0.001f,89.0f * (FLOAT)PI / 180.0f + 0.001f);
		OuterConeAngle = ClampedOuterConeAngle;
		CosOuterCone = appCos(ClampedOuterConeAngle);
		SinOuterCone = appSin(ClampedOuterConeAngle);
		CosInnerCone = appCos(ClampedInnerConeAngle);
		InvCosConeDifference = 1.0f / (CosInnerCone - CosOuterCone);
		InvTanOuterCone = 1.0f / appTan(ClampedOuterConeAngle);
		const FLOAT ClampedOuterLightShaftConeAngle = Clamp(Component->LightShaftConeAngle * (FLOAT)PI / 180.0f, 0.001f, 89.0f * (FLOAT)PI / 180.0f + 0.001f);
		// Use half the outer light shaft cone angle as the inner angle to provide a nice falloff
		// Not exposing the inner light shaft cone angle as it is probably not needed
		const FLOAT ClampedInnerLightShaftConeAngle = .5f * ClampedOuterLightShaftConeAngle;
		CosLightShaftConeAngle = appCos(ClampedOuterLightShaftConeAngle);
		InvCosLightShaftConeDifference = 1.0f / (appCos(ClampedInnerLightShaftConeAngle) - CosLightShaftConeAngle);
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

		LightDirection = -GetDirection();
		SpotAngles = FVector2D(CosOuterCone, InvCosConeDifference);
	}

	virtual void DetachPrimitive(const FLightPrimitiveInteraction& Interaction) 
	{
		DetachPrimitiveShared(Interaction);
	}

	virtual void AttachPrimitive(const FLightPrimitiveInteraction& Interaction)
	{
		AttachPrimitiveShared(Interaction);
	}

	virtual FLinearColor GetDirectIntensity(const FVector& Point) const
	{
		FVector LightVector = (Point - GetOrigin()).SafeNormal();
		FLOAT SpotAttenuation = Square(Clamp<FLOAT>(((LightVector | GetDirection()) - CosOuterCone) / (CosInnerCone - CosOuterCone),0.0f,1.0f));
		return FPointLightSceneInfoBase::GetDirectIntensity(Point) * SpotAttenuation;
	}

	// FLightSceneInfo interface.
	virtual UBOOL AffectsBounds(const FBoxSphereBounds& Bounds) const
	{
		if(!TPointLightSceneInfo<FSpotLightPolicy>::AffectsBounds(Bounds))
		{
			return FALSE;
		}

		FVector	U = GetOrigin() - (Bounds.SphereRadius / SinOuterCone) * GetDirection(),
				D = Bounds.Origin - U;
		FLOAT	dsqr = D | D,
				E = GetDirection() | D;
		if(E > 0.0f && E * E >= dsqr * Square(CosOuterCone))
		{
			D = Bounds.Origin - GetOrigin();
			dsqr = D | D;
			E = -(GetDirection() | D);
			if(E > 0.0f && E * E >= dsqr * Square(SinOuterCone))
				return dsqr <= Square(Bounds.SphereRadius);
			else
				return TRUE;
		}

		return FALSE;
	}
	
	/**
	 * Sets up a projected shadow initializer for shadows from the entire scene.
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual UBOOL GetWholeSceneProjectedShadowInitializer(const TArray<FViewInfo>& Views, TArray<FProjectedShadowInitializer, TInlineAllocator<6> >& OutInitializers) const
	{
		OutInitializers.Add();
		// Create a shadow projection that includes the entire spot light cone.
		return OutInitializers.Last().CalcWholeSceneShadowTransforms(
			-LightToWorld.GetOrigin(),
			WorldToLight.RemoveTranslation() * 
				FScaleMatrix(FVector(-InvTanOuterCone,InvTanOuterCone,1.0f)),
			FVector(0,0,1),
			FBoxSphereBounds(
				LightToWorld.RemoveTranslation().TransformFVector(FVector(0,0,Radius / 2.0f)),
				FVector(Radius/2.0f,Radius/2.0f,Radius/2.0f),
				Radius / 2.0f
				),
			FVector4(0,0,1,0),
			0.1f,
			Radius,
			FALSE,
			FALSE,
			INDEX_NONE
			);
	}

	virtual const FSpotLightSceneInfo* GetSpotLightInfo() const { return this; }

	virtual FLOAT GetOuterConeAngle() const { return OuterConeAngle; }

	virtual FSphere GetBoundingSphere() const
	{
		// Use the law of cosines to find the distance to the furthest edge of the spotlight cone from a position that is halfway down the spotlight direction
		const FLOAT BoundsRadius = appSqrt(1.25f * Radius * Radius - Radius * Radius * CosOuterCone);
		return FSphere(GetOrigin() + .5f * GetDirection() * Radius, BoundsRadius);
	}
};

template<typename ShaderRHIParamRef>
void FSpotLightPolicy::VertexParametersType::SetLight(ShaderRHIParamRef Shader,const FSpotLightSceneInfo* Light,const FSceneView* View) const
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

void FSpotLightPolicy::PixelParametersType::SetLight(FShader* PixelShader,const FSpotLightSceneInfo* Light,const FSceneView* View) const
{
	SetPixelShaderValue(PixelShader->GetPixelShader(),SpotAnglesParameter,FVector4(Light->CosOuterCone,Light->InvCosConeDifference,0,0));
	SetPixelShaderValue(PixelShader->GetPixelShader(),SpotDirectionParameter,Light->GetDirection());
}

void FSpotLightPolicy::PixelParametersType::SetLightMesh(FShader* PixelShader,const FPrimitiveSceneInfo* PrimitiveSceneInfo,const SceneInfoType* Light,UBOOL bApplyLightFunctionDisabledBrightness) const
{
	FLOAT ShadowFactor = IsDominantLightType(Light->LightType) ? PrimitiveSceneInfo->DominantShadowFactor : 1.0f;
	if (bApplyLightFunctionDisabledBrightness)
	{
		ShadowFactor *= Light->LightFunctionDisabledBrightness;
	}
	SetPixelShaderValue(PixelShader->GetPixelShader(),LightColorAndFalloffExponentParameter,
		FVector4(Light->Color.R * ShadowFactor,Light->Color.G * ShadowFactor,Light->Color.B * ShadowFactor,Light->FalloffExponent));
}

void FSpotLightPolicy::ModShadowPixelParamsType::SetModShadowLight(FShader* PixelShader,const FSpotLightSceneInfo* Light,const FSceneView* View) const
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
	// set spot light direction
	SetPixelShaderValue(PixelShader->GetPixelShader(),SpotDirectionParam,Light->GetDirection());
	// set spot light inner/outer cone angles
	SetPixelShaderValue(PixelShader->GetPixelShader(),SpotAnglesParam,FVector4(Light->CosOuterCone,Light->InvCosConeDifference,0,0));
}

/** Sets spotlight parameters for light shafts on the passed in shader. */
void SetSpotLightShaftParameters(
	FShader* Shader, 
	const FLightSceneInfo* LightSceneInfo, 
	const FShaderParameter& WorldSpaceSpotDirectionParameter, 
	const FShaderParameter& SpotAnglesParameter)
{
	const FSpotLightSceneInfo* SpotLightInfo = LightSceneInfo->GetSpotLightInfo();
	if (SpotLightInfo)
	{
		SetPixelShaderValue(Shader->GetPixelShader(), WorldSpaceSpotDirectionParameter, SpotLightInfo->GetDirection());

		SetPixelShaderValue(Shader->GetPixelShader(), SpotAnglesParameter, FVector2D(SpotLightInfo->CosLightShaftConeAngle, SpotLightInfo->InvCosLightShaftConeDifference));
	}
}

FLightSceneInfo* USpotLightComponent::CreateSceneInfo() const
{
	return new FSpotLightSceneInfo(this);
}

UBOOL USpotLightComponent::AffectsBounds(const FBoxSphereBounds& Bounds) const
{
	if(!Super::AffectsBounds(Bounds))
	{
		return FALSE;
	}

	FLOAT	ClampedInnerConeAngle = Clamp(InnerConeAngle,0.0f,89.0f) * (FLOAT)PI / 180.0f,
			ClampedOuterConeAngle = Clamp(OuterConeAngle * (FLOAT)PI / 180.0f,ClampedInnerConeAngle + 0.001f,89.0f * (FLOAT)PI / 180.0f + 0.001f);

	FLOAT	Sin = appSin(ClampedOuterConeAngle),
			Cos = appCos(ClampedOuterConeAngle);

	FVector	U = GetOrigin() - (Bounds.SphereRadius / Sin) * GetDirection(),
			D = Bounds.Origin - U;
	FLOAT	dsqr = D | D,
			E = GetDirection() | D;
	if(E > 0.0f && E * E >= dsqr * Square(Cos))
	{
		D = Bounds.Origin - GetOrigin();
		dsqr = D | D;
		E = -(GetDirection() | D);
		if(E > 0.0f && E * E >= dsqr * Square(Sin))
			return dsqr <= Square(Bounds.SphereRadius);
		else
			return TRUE;
	}

	return FALSE;
}

void USpotLightComponent::Attach()
{
	Super::Attach();

	if ( PreviewInnerCone )
	{
		PreviewInnerCone->ConeRadius = Radius;
		PreviewInnerCone->ConeAngle = InnerConeAngle;
		PreviewInnerCone->Translation = Translation;
		PreviewInnerCone->Rotation = Rotation;
	}

	if ( PreviewOuterCone )
	{
		PreviewOuterCone->ConeRadius = Radius;
		PreviewOuterCone->ConeAngle = OuterConeAngle;
		PreviewOuterCone->Translation = Translation;
		PreviewOuterCone->Rotation = Rotation;
	}
}

void USpotLightComponent::SetRotation( FRotator NewRotation )
{
	Rotation = NewRotation;
	BeginDeferredUpdateTransform();
}

void USpotLightComponent::SetTransformedToWorld()
{
	// apply our translation to the parent matrix
  	LightToWorld = FMatrix(
  		FPlane(+0,+0,+1,+0),
  		FPlane(+0,+1,+0,+0),
  		FPlane(+1,+0,+0,+0),
  		FPlane(+0,+0,+0,+1)
  		) *
  		FRotationTranslationMatrix(Rotation, Translation) * 
  		CachedParentToWorld;
	LightToWorld.RemoveScaling();
	WorldToLight = LightToWorld.InverseSafe();
}

FLinearColor USpotLightComponent::GetDirectIntensity(const FVector& Point) const
{
	FLOAT	ClampedInnerConeAngle = Clamp(InnerConeAngle,0.0f,89.0f) * (FLOAT)PI / 180.0f,
			ClampedOuterConeAngle = Clamp(OuterConeAngle * (FLOAT)PI / 180.0f,ClampedInnerConeAngle + 0.001f,89.0f * (FLOAT)PI / 180.0f + 0.001f),
			OuterCone = appCos(ClampedOuterConeAngle),
			InnerCone = appCos(ClampedInnerConeAngle);

	FVector LightVector = (Point - GetOrigin()).SafeNormal();
	FLOAT SpotAttenuation = Square(Clamp<FLOAT>(((LightVector | GetDirection()) - OuterCone) / (InnerCone - OuterCone),0.0f,1.0f));
	return Super::GetDirectIntensity(Point) * SpotAttenuation;
}

/**
* @return ELightComponentType for the light component class 
*/
ELightComponentType USpotLightComponent::GetLightType() const
{
	return LightType_Spot;
}

void USpotLightComponent::PostLoad()
{
	Super::PostLoad();

	if ( GIsEditor
	&& !IsTemplate(RF_ClassDefaultObject)
	// FReloadObjectArcs call PostLoad() *prior* to instancing components for objects being reloaded, so this check is invalid in that case
	&& (GUglyHackFlags&HACK_IsReloadObjArc) == 0 )
	{
		if ( PreviewInnerCone != NULL && PreviewInnerCone->GetOuter() != GetOuter() )
		{
			// so if we are here, then the owning light actor was definitely created after the fixup code was added, so there is some way that this bug is still occurring
			// I need to figure out how this is occurring so let's annoy the designer into bugging me.
			debugf(TEXT("%s has an invalid PreviewInnerCone '%s' even though package has been resaved since this bug was fixed.  Please let Ron know about this immediately!"), *GetFullName(), *PreviewInnerCone->GetFullName());
			//@todo ronp - remove this once we've verified that this is no longer occurring.
			appMsgf(AMT_OK, TEXT("%s has an invalid PreviewInnerCone '%s' even though package has been resaved since this bug was fixed.  Please let Ron know about this immediately! (this message has already been written to the log)"), *GetFullName(), *PreviewInnerCone->GetFullName());
		}
		else if ( PreviewOuterCone != NULL && PreviewOuterCone->GetOuter() != GetOuter() )
		{
			// so if we are here, then the owning light actor was definitely created after the fixup code was added, so there is some way that this bug is still occurring
			// I need to figure out how this is occurring so let's annoy the designer into bugging me.
			debugf(TEXT("%s has an invalid PreviewOuterCone '%s' even though package has been resaved since this bug was fixed.  Please let Ron know about this immediately!"), *GetFullName(), *PreviewOuterCone->GetFullName());
			//@todo ronp - remove this once we've verified that this is no longer occurring.
			appMsgf(AMT_OK, TEXT("%s has an invalid PreviewOuterCone '%s' even though package has been resaved since this bug was fixed.  Please let Ron know about this immediately! (this message has already been written to the log)"), *GetFullName(), *PreviewOuterCone->GetFullName());
		}
	}
}

void UDominantSpotLightComponent::Serialize(FArchive& Ar)
{
	if (Ar.Ver() >= VER_SPOTLIGHT_DOMINANTSHADOW_TRANSITION)
	{
		Ar << DominantLightShadowMap;
		if (Ar.IsLoading())
		{
			INC_DWORD_STAT_BY(STAT_DominantShadowTransitionMemory, DominantLightShadowMap.GetAllocatedSize());
		}
	}
	
	Super::Serialize(Ar);
}

ELightComponentType UDominantSpotLightComponent::GetLightType() const
{
	return LightType_DominantSpot;
}

void UDominantSpotLightComponent::InvalidateLightingCache()
{
	DominantLightShadowInfo.ShadowMapSizeX = DominantLightShadowInfo.ShadowMapSizeY = 0;
	DEC_DWORD_STAT_BY(STAT_DominantShadowTransitionMemory, DominantLightShadowMap.GetAllocatedSize());
	DominantLightShadowMap.Empty();
	Super::InvalidateLightingCache();
}

/** Returns information about the data used to calculate dominant shadow transition distance. */
void UDominantSpotLightComponent::GetInfo(INT& SizeX, INT& SizeY, SIZE_T& ShadowMapBytes) const
{
	SizeX = DominantLightShadowInfo.ShadowMapSizeX;
	SizeY = DominantLightShadowInfo.ShadowMapSizeY;
	ShadowMapBytes = DominantLightShadowMap.GetAllocatedSize();
}

/** Populates DominantLightShadowMap and DominantLightShadowInfo with the results from a lighting build. */
void UDominantSpotLightComponent::Initialize(const FDominantShadowInfo& InInfo, const TArray<WORD>& InShadowMap, UBOOL bOnlyVisible)
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

void UDominantSpotLightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	bForceDynamicLight = FALSE;
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UDominantSpotLightComponent::FinishDestroy()
{
	DEC_DWORD_STAT_BY(STAT_DominantShadowTransitionMemory, DominantLightShadowMap.GetAllocatedSize());
	Super::FinishDestroy();
}

/** Returns the distance to the nearest dominant shadow transition, in world space units, starting from the edge of the bounds. */
FLOAT UDominantSpotLightComponent::GetDominantShadowTransitionDistance(
	const FBoxSphereBounds& Bounds, 
	FLOAT MaxSearchDistance, 
	UBOOL bDebugSearch, 
	TArray<FDebugShadowRay>& DebugRays,
	UBOOL& bLightingIsBuilt) const
{
	const FLOAT ClampedInnerConeAngle = Clamp(InnerConeAngle, 0.0f, 89.0f) * (FLOAT)PI / 180.0f;
	const FLOAT ClampedOuterConeAngle = Clamp(OuterConeAngle * (FLOAT)PI / 180.0f, ClampedInnerConeAngle + 0.001f, 89.0f * (FLOAT)PI / 180.0f + 0.001f);
	const FVector V = Bounds.Origin - GetOrigin();
	const FLOAT DistanceToConeOrigin = V.Size();
	FLOAT DistanceToCone = DistanceToConeOrigin;
	if (DistanceToCone > KINDA_SMALL_NUMBER)
	{
		const FLOAT AngDiff = appAcos((GetDirection() | V) / DistanceToCone) - ClampedOuterConeAngle;
		FLOAT SinDiff;
		FLOAT CosDiff;

		if (AngDiff <= 0.0f)
		{
			SinDiff = 0.0f;
			CosDiff = DistanceToCone;
		}
		else if (AngDiff < (FLOAT)PI * 0.5f)
		{
			SinDiff = DistanceToCone * appSin(AngDiff);
			CosDiff = DistanceToCone * appCos(AngDiff);
		}
		else
		{
			SinDiff = DistanceToCone;
			CosDiff = 0.0f;
		}

		const FLOAT DY = SinDiff;
		const FLOAT DX = Max<FLOAT>(0.0f, CosDiff - Radius);
		DistanceToCone = appSqrt(DX * DX + DY * DY);
	}

	// DistanceToCone is now 0 inside the cone
	DistanceToCone = Max(DistanceToCone - Bounds.SphereRadius, 0.0f);

	if (DominantLightShadowMap.Num() > 0)
	{
		bLightingIsBuilt = TRUE;
		checkSlow(DominantLightShadowInfo.ShadowMapSizeX > 0 && DominantLightShadowInfo.ShadowMapSizeY > 0);

		// 2D radius of a single shadowmap cell at the radius of the spotlight
		const FLOAT MaxShadowMapCellRadius = .5f * FVector2D(
			(DominantLightShadowInfo.LightSpaceImportanceBounds.Max.X - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.X) / DominantLightShadowInfo.ShadowMapSizeX,
			(DominantLightShadowInfo.LightSpaceImportanceBounds.Max.Y - DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Y) / DominantLightShadowInfo.ShadowMapSizeY).Size();

		// Only search the shadowmap if the search position is close to the cone
		if (DistanceToCone < MaxSearchDistance + MaxShadowMapCellRadius)
		{
			checkSlow(Abs(DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Z - 0.0f) < KINDA_SMALL_NUMBER);
			// Transform the bounds origin into the coordinate space that the shadow map entries are stored in
			const FVector LightSpaceOrigin = DominantLightShadowInfo.WorldToLight.TransformFVector(Bounds.Origin);
			if (LightSpaceOrigin.Z + Bounds.SphereRadius < DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Z)
			{
				// Return unshadowed if the bounds are behind the light
				return MaxSearchDistance;
			}

			const FLOAT DistanceToLight = LightSpaceOrigin.Size();
			const FVector ShadowmapBoundsMinAtRadius(DominantLightShadowInfo.LightSpaceImportanceBounds.Min.X, DominantLightShadowInfo.LightSpaceImportanceBounds.Min.Y, DominantLightShadowInfo.LightSpaceImportanceBounds.Max.Z);
			// Calculate the shadowmap bounds at the Z of the bound's origin using similar triangles
			const FVector ShadowmapBoundsMin = ShadowmapBoundsMinAtRadius * LightSpaceOrigin.Z / ShadowmapBoundsMinAtRadius.Z;
			const FVector ShadowmapBoundsMax = DominantLightShadowInfo.LightSpaceImportanceBounds.Max * LightSpaceOrigin.Z / DominantLightShadowInfo.LightSpaceImportanceBounds.Max.Z;

			// 2D radius of a single shadowmap cell
			const FLOAT ShadowMapCellRadius = .5f * FVector2D(
				(ShadowmapBoundsMax.X - ShadowmapBoundsMin.X) / DominantLightShadowInfo.ShadowMapSizeX,
				(ShadowmapBoundsMax.Y - ShadowmapBoundsMin.Y) / DominantLightShadowInfo.ShadowMapSizeY).Size();

			const FLOAT XScale = 1.0f / (ShadowmapBoundsMax.X - ShadowmapBoundsMin.X);
			const FLOAT YScale = 1.0f / (ShadowmapBoundsMax.Y - ShadowmapBoundsMin.Y);

			// Calculate the coordinates of the box containing the search bounds in the shadow map
			const INT MinShadowMapX = Clamp(appTrunc(DominantLightShadowInfo.ShadowMapSizeX * (LightSpaceOrigin.X - Bounds.SphereRadius - MaxSearchDistance - ShadowMapCellRadius - ShadowmapBoundsMin.X) * XScale), 0, DominantLightShadowInfo.ShadowMapSizeX - 1);
			const INT MaxShadowMapX = Clamp(appTrunc(DominantLightShadowInfo.ShadowMapSizeX * (LightSpaceOrigin.X + Bounds.SphereRadius + MaxSearchDistance + ShadowMapCellRadius - ShadowmapBoundsMin.X) * XScale), 0, DominantLightShadowInfo.ShadowMapSizeX - 1);
			const INT MinShadowMapY = Clamp(appTrunc(DominantLightShadowInfo.ShadowMapSizeY * (LightSpaceOrigin.Y - Bounds.SphereRadius - MaxSearchDistance - ShadowMapCellRadius - ShadowmapBoundsMin.Y) * YScale), 0, DominantLightShadowInfo.ShadowMapSizeY - 1);
			const INT MaxShadowMapY = Clamp(appTrunc(DominantLightShadowInfo.ShadowMapSizeY * (LightSpaceOrigin.Y + Bounds.SphereRadius + MaxSearchDistance + ShadowMapCellRadius - ShadowmapBoundsMin.Y) * YScale), 0, DominantLightShadowInfo.ShadowMapSizeY - 1);

			const FLOAT MaxPossibleDistance = Max(DominantLightShadowInfo.LightSpaceImportanceBounds.Max.Size(), (FLOAT)KINDA_SMALL_NUMBER);

			FLOAT MinTransitionDistance = MaxSearchDistance; 
			UBOOL bFoundSample = FALSE;
			FVector ClosestSamplePosition(0,0,0);
			INT ClosestSampleIndex = INDEX_NONE;
			// Search the box containing the search bounds for the closest shadow transition
			for (INT Y = MinShadowMapY; Y <= MaxShadowMapY && MinTransitionDistance > 0.0f; Y++)
			{
				const FLOAT LightSpaceYPosition = Y / (FLOAT)(DominantLightShadowInfo.ShadowMapSizeY - 1) * (ShadowmapBoundsMax.Y - ShadowmapBoundsMin.Y) + ShadowmapBoundsMin.Y;
				for (INT X = MinShadowMapX; X <= MaxShadowMapX && MinTransitionDistance > 0.0f; X++)
				{
					const WORD QuantizedCurrentSampleDistance = DominantLightShadowMap(Y * DominantLightShadowInfo.ShadowMapSizeX + X);
					// Reconstruct the shadowmap sample distance which is in coordinate space defined by DominantLightShadowInfo.WorldToLight
					const FLOAT CurrentSampleDistance = QuantizedCurrentSampleDistance / 65535.0f * MaxPossibleDistance;
					const FLOAT LightSpaceXPosition = X / (FLOAT)(DominantLightShadowInfo.ShadowMapSizeX - 1) * (ShadowmapBoundsMax.X - ShadowmapBoundsMin.X) + ShadowmapBoundsMin.X;
					const FVector SampleDirection = FVector(LightSpaceXPosition, LightSpaceYPosition, LightSpaceOrigin.Z).SafeNormal();
					// Only consider shadowmap cells inside the spotlight cone
					if ((SampleDirection | FVector(0,0,1)) > appCos(ClampedOuterConeAngle))
					{
						const FVector CurrentSamplePosition(SampleDirection * Min(DistanceToLight, CurrentSampleDistance));
						const FLOAT CurrentDistance = Max((CurrentSamplePosition - LightSpaceOrigin).Size() - ShadowMapCellRadius - Bounds.SphereRadius, 0.0f);

#if !FINAL_RELEASE
						if (bDebugSearch)
						{
							const FVector4 RayEnd = FVector(LightSpaceXPosition, LightSpaceYPosition, LightSpaceOrigin.Z).UnsafeNormal() * CurrentSampleDistance;
							const INT CurrentIndex = DebugRays.AddItem(FDebugShadowRay(
								DominantLightShadowInfo.LightToWorld.TransformFVector(FVector(0, 0, 0)),
								DominantLightShadowInfo.LightToWorld.TransformFVector(RayEnd),
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
			}

#if !FINAL_RELEASE
			if (bDebugSearch && bFoundSample)
			{
				checkSlow(ClosestSampleIndex != INDEX_NONE);
				DebugRays(ClosestSampleIndex).bHit = TRUE;
				DebugRays.AddItem(FDebugShadowRay(
					DominantLightShadowInfo.LightToWorld.TransformFVector(ClosestSamplePosition),
					Bounds.Origin,
					TRUE));
			}
#endif

			const FLOAT DistanceFromConeCap = DistanceToConeOrigin - Radius - Bounds.SphereRadius;
			if (DistanceFromConeCap >= 0.0f && MinTransitionDistance < MaxSearchDistance)
			{
				// If the search position is outside of the cone's cap and it is close to unshadowed, 
				// Return the max of the distance to the cap and the distance to the transition.
				return Max(DistanceFromConeCap, MinTransitionDistance);
			}
			// Return the distance to the closest dominant shadow transition.
			return MinTransitionDistance;
		}
		else
		{
			return MaxSearchDistance;
		}
	}

	bLightingIsBuilt = FALSE;
	return DistanceToCone;
}

void FDynamicallyShadowedMultiTypeLightLightMapPolicy::Set(
	const VertexParametersType* VertexShaderParameters,
	const PixelParametersType* PixelShaderParameters,
	FShader* VertexShader,
	FShader* PixelShader,
	const FVertexFactory* VertexFactory,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const FSceneView* View
	) const
{
	// Set() is only called once per static draw list bucket as an optimization, so we set all shader parameters that are specific to a bucket here.
	// VertexFactory parameters can be set here because FMeshDrawingPolicy::Matches sorts by VertexFactory.
	// FDynamicallyShadowedMultiTypeLightLightMapPolicy operator== compares LightSceneInfo's, so anything dependent only on LightSceneInfo can also be set here.

	// Derived policies pass in a NULL VertexFactory if they don't want this lightmap policy to setup vertex factory streams
	if (VertexFactory)
	{
		VertexFactory->Set();
	}

	check(LightSceneInfo);
	const FPointLightSceneInfoBase* PointLightInfo = LightSceneInfo->GetPointLightInfo();
	SetVertexShaderValue(
		VertexShader->GetVertexShader(),
		VertexShaderParameters->LightPositionAndInvRadiusParameter,
		PointLightInfo
		? FVector4((LightSceneInfo->GetOrigin() + View->PreViewTranslation) * PointLightInfo->InvRadius,PointLightInfo->InvRadius)
		: FVector4(-LightSceneInfo->GetDirection(),0.0f)
		);

	if (PixelShaderParameters)
	{
		const FSpotLightSceneInfo* SpotLightInfo = LightSceneInfo->GetSpotLightInfo();
		if (SpotLightInfo)
		{
			// Set spotlight-only parameters
			// These will still be bound for other light types, but not used for rendering
			SetPixelShaderValue(PixelShader->GetPixelShader(), PixelShaderParameters->SpotDirectionParameter, SpotLightInfo->GetDirection());
			SetPixelShaderValue(PixelShader->GetPixelShader(), PixelShaderParameters->SpotAnglesParameter, FVector4(SpotLightInfo->CosOuterCone, SpotLightInfo->InvCosConeDifference, 0, 0));
		}

		FVector2D DistanceFadeValues;
		const UBOOL bEnableDistanceShadowFading = View->Family->ShouldDrawShadows() 
			&& GSystemSettings.bAllowWholeSceneDominantShadows
			&& (View->RenderingOverrides.bAllowDominantWholeSceneDynamicShadows || !LightSceneInfo->bStaticShadowing)
			&& LightSceneInfo->GetDirectionalLightDistanceFadeParameters(DistanceFadeValues)
			// Translucent objects will never receive cascaded shadow maps, and therefore should never attempt to fade between static shadowing and cascaded shadow maps.  Always use static shadowing in that case.
			&& !bUseTranslucencyLightAttenuation
			// The lifetime of the whole scene dominant shadow projection is only during the world DPG
			// This prevents objects in the foreground DPG from reading the projection results outside of that lifetime
			&& GSceneRenderTargets.IsWholeSceneDominantShadowValid();

		SetPixelShaderBool(PixelShader->GetPixelShader(), PixelShaderParameters->bEnableDistanceShadowFadingParameter, bEnableDistanceShadowFading);
		if (bEnableDistanceShadowFading)
		{
			SetPixelShaderValue(PixelShader->GetPixelShader(), PixelShaderParameters->DistanceFadeParameter, FVector4(DistanceFadeValues.X, DistanceFadeValues.Y, 0, 0));
		}

		// Set bools used for static branching
		SetPixelShaderBool(PixelShader->GetPixelShader(), PixelShaderParameters->bDynamicDirectionalLightParameter, PointLightInfo == NULL);
		SetPixelShaderBool(PixelShader->GetPixelShader(), PixelShaderParameters->bDynamicSpotLightParameter, SpotLightInfo != NULL);
	}

	if (PixelShaderParameters->LightAttenuationTextureParameter.IsBound())
	{
		// Bind the appropriate light attenuation texture for this light
		const UBOOL bUseLightAttenuation0 = View->DominantLightChannelAllocator.GetTextureIndex(LightSceneInfo->Id) == 0;
		SetTextureParameter(
			PixelShader->GetPixelShader(),
			PixelShaderParameters->LightAttenuationTextureParameter,
			TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
			bUseTranslucencyLightAttenuation ? (const FTextureRHIRef&)GSceneRenderTargets.GetTranslucencyDominantLightAttenuationTexture() : GSceneRenderTargets.GetEffectiveLightAttenuationTexture(TRUE, bUseLightAttenuation0)
			);
	}

	const INT ChannelIndex = View->DominantLightChannelAllocator.GetLightChannel(LightSceneInfo->Id);
	checkSlow(View->DominantLightChannelAllocator.GetNumChannels() <= 3);
	const FVector4 LightChannelMask(ChannelIndex == 0 ? 1.0f : 0.0f, ChannelIndex == 1 ? 1.0f : 0.0f, ChannelIndex == 2 ? 1.0f : 0.0f, ChannelIndex == INDEX_NONE ? 1.0f : 0.0f);
	SetPixelShaderValue(PixelShader->GetPixelShader(), PixelShaderParameters->LightChannelMaskParameter, LightChannelMask);
}

void FDynamicallyShadowedMultiTypeLightLightMapPolicy::SetMesh(
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
	if (PixelShaderParameters)
	{
		check(LightSceneInfo);
		const FPointLightSceneInfoBase* PointLightInfo = LightSceneInfo->GetPointLightInfo();
		const FLOAT FalloffExponent = PointLightInfo ? PointLightInfo->FalloffExponent : 1.0f;
		FLOAT ShadowFactor = PrimitiveSceneInfo->DominantShadowFactor;
		// Apply the light function's disabled brightness setting if it is disabled due to show flags,
		// So that the light will be the right overall brightness even though the light function is disabled.
		if (LightSceneInfo->LightFunction && !(View.Family->ShowFlags & SHOW_DynamicShadows))
		{
			ShadowFactor *= LightSceneInfo->LightFunctionDisabledBrightness;
		}
		const FVector LightColor = FVector(LightSceneInfo->Color.R, LightSceneInfo->Color.G, LightSceneInfo->Color.B) * ShadowFactor;
		SetPixelShaderValue(PixelShader->GetPixelShader(), PixelShaderParameters->LightColorAndFalloffExponentParameter, FVector4(LightColor, FalloffExponent));
		
		PixelShaderParameters->ForwardShadowingParameters.SetReceiveShadows(PixelShader, ElementData.bReceiveDynamicShadows);
		PixelShaderParameters->ForwardShadowingParameters.Set(View, PixelShader, ElementData.bOverrideDynamicShadowsOnTranslucency, ElementData.TranslucentPreShadowInfo);
	}
}
