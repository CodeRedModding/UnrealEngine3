/*=============================================================================
	PointLightSceneInfo.h: Point light scene info definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_POINTLIGHTSCENEINFO
#define _INC_POINTLIGHTSCENEINFO

/**
 * Compute the screen bounds of a point light along one axis.
 * Based on http://www.gamasutra.com/features/20021011/lengyel_06.htm
 * and http://sourceforge.net/mailarchive/message.php?msg_id=10501105
 */
static bool GetPointLightBounds(
	FLOAT LightX,
	FLOAT LightZ,
	FLOAT Radius,
	const FVector& Axis,
	FLOAT AxisSign,
	const FSceneView* View,
	FLOAT ViewX,
	FLOAT ViewSizeX,
	INT& OutMinX,
	INT& OutMaxX
	)
{
	// Vertical planes: T = <Nx, 0, Nz, 0>
	FLOAT Discriminant = (Square(LightX) - Square(Radius) + Square(LightZ)) * Square(LightZ);
	if(Discriminant >= 0)
	{
		FLOAT Nxa = (Radius * LightX - appSqrt(Discriminant)) / (Square(LightX) + Square(LightZ));
		FLOAT Nxb = (Radius * LightX + appSqrt(Discriminant)) / (Square(LightX) + Square(LightZ));
		FLOAT Nza = (Radius - Nxa * LightX) / LightZ;
		FLOAT Nzb = (Radius - Nxb * LightX) / LightZ;
		FLOAT Pza = LightZ - Radius * Nza;
		FLOAT Pzb = LightZ - Radius * Nzb;

		// Tangent a
		if(Pza > 0)
		{
			FLOAT Pxa = -Pza * Nza / Nxa;
			FVector4 P = View->ProjectionMatrix.TransformFVector4(FVector4(Axis.X * Pxa,Axis.Y * Pxa,Pza,1));
			FLOAT X = (Dot3(P,Axis) / P.W + 1.0f * AxisSign) / 2.0f * AxisSign;
			if(IsNegativeFloat(Nxa) ^ IsNegativeFloat(AxisSign))
			{
				OutMaxX = Min<LONG>(appCeil(ViewSizeX * X + ViewX),OutMaxX);
			}
			else
			{
				OutMinX = Max<LONG>(appFloor(ViewSizeX * X + ViewX),OutMinX);
			}
		}

		// Tangent b
		if(Pzb > 0)
		{
			FLOAT Pxb = -Pzb * Nzb / Nxb;
			FVector4 P = View->ProjectionMatrix.TransformFVector4(FVector4(Axis.X * Pxb,Axis.Y * Pxb,Pzb,1));
			FLOAT X = (Dot3(P,Axis) / P.W + 1.0f * AxisSign) / 2.0f * AxisSign;
			if(IsNegativeFloat(Nxb) ^ IsNegativeFloat(AxisSign))
			{
				OutMaxX = Min<LONG>(appCeil(ViewSizeX * X + ViewX),OutMaxX);
			}
			else
			{
				OutMinX = Max<LONG>(appFloor(ViewSizeX * X + ViewX),OutMinX);
			}
		}
	}

	return OutMinX <= OutMaxX;
}

extern TGlobalResource<FShadowFrustumVertexDeclaration> GShadowFrustumVertexDeclaration;

/** The parts of the point light scene info that aren't dependent on the light policy type. */
class FPointLightSceneInfoBase : public FLightSceneInfo
{
public:

	/** The light radius. */
	FLOAT Radius;

	/** One over the light's radius. */
	FLOAT InvRadius;

	/** The light falloff exponent. */
	FLOAT FalloffExponent;

	/** Falloff for shadow when using LightShadow_Modulate */
	FLOAT ShadowFalloffExponent;

	/** multiplied by the radius of object's bounding sphere, default - 1.1*/
	FLOAT ShadowRadiusMultiplier;

	/** A scale that's applied to the light's shadow falloff distance. */
	FLOAT ShadowFalloffScale;

	/** A bias that's applied to the light's shadow falloff distance. */
	FLOAT ShadowFalloffBias;

	/** Initialization constructor. */
	FPointLightSceneInfoBase(const UPointLightComponent* Component)
	:	FLightSceneInfo(Component)
	,	FalloffExponent(Component->FalloffExponent)
	,	ShadowFalloffExponent(Component->ShadowFalloffExponent)
	,	ShadowRadiusMultiplier( Component->ShadowRadiusMultiplier )
	{
		UpdateRadius(Component->Radius,Component->MinShadowFalloffRadius);
		// Convert LightSourceRadius into uniform penumbra size, since LightSourceRadius doesn't have any meaning for distance field shadowed lights
		DistanceFieldShadowMapPenumbraSize = Clamp(Component->LightmassSettings.LightSourceRadius / 100.0f, 0.001f, 1.0f);
		DistanceFieldShadowMapShadowExponent = Component->LightmassSettings.ShadowExponent;
	}

	/**
	* Called on the light scene info after it has been passed to the rendering thread to update the rendering thread's cached info when
	* the light's radius changes.
	*/
	void UpdateRadius_GameThread(UPointLightComponent* Component);

	// FLightSceneInfo interface.

	/** @return radius of the light or 0 if no radius */
	virtual FLOAT GetRadius() const 
	{ 
		return Radius; 
	}

	virtual FLinearColor GetDirectIntensity(const FVector& Point) const;

	virtual const FPointLightSceneInfoBase* GetPointLightInfo() const { return this; }

	virtual UBOOL AffectsBounds(const FBoxSphereBounds& Bounds) const
	{
		if((Bounds.Origin - this->LightToWorld.GetOrigin()).SizeSquared() > Square(Radius + Bounds.SphereRadius))
		{
			return FALSE;
		}

		if(!FLightSceneInfo::AffectsBounds(Bounds))
		{
			return FALSE;
		}

		return TRUE;
	}

	virtual UBOOL GetProjectedShadowInitializer(const FBoxSphereBounds& SubjectBounds,FProjectedShadowInitializer& OutInitializer) const
	{
		// For point lights, use a perspective projection looking at the primitive from the light position.
		FVector LightPosition = LightToWorld.GetOrigin();
		FVector LightVector = SubjectBounds.Origin - LightPosition;
		FLOAT LightDistance = LightVector.Size();
		FLOAT SilhouetteRadius = 0.0f;
		const FLOAT SubjectRadius = SubjectBounds.SphereRadius;

		if(LightDistance > SubjectRadius)
		{
			SilhouetteRadius = Min(SubjectRadius * appInvSqrt((LightDistance - SubjectRadius) * (LightDistance + SubjectRadius)), 1.0f);
		}

		if( LightDistance <= SubjectRadius * ShadowRadiusMultiplier )
		{
			// Make primitive fit in a single <90 degree FOV projection.
			LightVector = SubjectRadius * LightVector.SafeNormal() * ShadowRadiusMultiplier;
			LightPosition = (SubjectBounds.Origin - LightVector );
			LightDistance = SubjectRadius * ShadowRadiusMultiplier;
			SilhouetteRadius = 1.0f;
		}

		FLOAT MaxDistanceToCastInLightW = Radius;
#if WITH_MOBILE_RHI
		if ( GUsingMobileRHI )
		{
			MaxDistanceToCastInLightW = Min( MaxDistanceToCastInLightW, SubjectBounds.SphereRadius + GSystemSettings.MobileMaxShadowRange );
		}
#endif
		
		// The primitive now fits in a single <90 degree FOV projection.
		return OutInitializer.CalcObjectShadowTransforms(
			-LightPosition,
			FInverseRotationMatrix((LightVector / LightDistance).Rotation()) *
			FScaleMatrix(FVector(1.0f,1.0f / SilhouetteRadius,1.0f / SilhouetteRadius)),
			FVector(1,0,0),
			FBoxSphereBounds(
			SubjectBounds.Origin - LightPosition,
			SubjectBounds.BoxExtent,
			SubjectBounds.SphereRadius
			),
			FVector4(0,0,1,0),
			0.1f,
			MaxDistanceToCastInLightW,
			FALSE
			);
	}

	virtual void SetDepthBounds(const FSceneView* View) const
	{
		//transform the light's position into view space
		FVector ViewSpaceLightPosition = View->ViewMatrix.TransformFVector(LightToWorld.GetOrigin());

		//subtract the light's radius from the view space depth to get the near position
		//this assumes that Radius in world space is the same length in view space, ie ViewMatrix has no scaling
		FVector NearLightPosition = ViewSpaceLightPosition;
		NearLightPosition.Z -= Radius;

		//add the light's radius to the view space depth to get the far position
		FVector FarLightPosition = ViewSpaceLightPosition;
		FarLightPosition.Z += Radius;

		//transform both near and far positions into clip space
		FVector4 ClipSpaceNearPos = View->ProjectionMatrix.TransformFVector(NearLightPosition);
		FVector4 ClipSpaceFarPos = View->ProjectionMatrix.TransformFVector(FarLightPosition);

		//be sure to disable depth bounds test after drawing!
		RHISetDepthBoundsTest(TRUE, ClipSpaceNearPos, ClipSpaceFarPos);
	}

	virtual void SetScissorRect(const FSceneView* View) const
	{
		// Calculate a scissor rectangle for the light's radius.
		if((this->LightToWorld.GetOrigin() - View->ViewOrigin).Size() > Radius)
		{
			FVector LightVector = View->ViewMatrix.TransformFVector(this->LightToWorld.GetOrigin());

			INT ScissorMinX = appFloor(View->RenderTargetX);
			INT ScissorMaxX = appCeil(View->RenderTargetX + View->RenderTargetSizeX);
			if(!GetPointLightBounds(
				LightVector.X,
				LightVector.Z,
				Radius,
				FVector(+1,0,0),
				+1,
				View,
				View->RenderTargetX,
				View->RenderTargetSizeX,
				ScissorMinX,
				ScissorMaxX))
			{
				return;
			}

			INT ScissorMinY = appFloor(View->RenderTargetY);
			INT ScissorMaxY = appCeil(View->RenderTargetY + View->RenderTargetSizeY);
			if(!GetPointLightBounds(
				LightVector.Y,
				LightVector.Z,
				Radius,
				FVector(0,+1,0),
				-1,
				View,
				View->RenderTargetY,
				View->RenderTargetSizeY,
				ScissorMinY,
				ScissorMaxY))
			{
				return;
			}

			RHISetScissorRect(TRUE,ScissorMinX,ScissorMinY,ScissorMaxX,ScissorMaxY);
		}
		else
		{
			RHISetScissorRect(FALSE,0,0,0,0);
		}
	}

private:

	/** Updates the light scene info's radius from the component. */
	void UpdateRadius(FLOAT ComponentRadius,FLOAT ComponentMinShadowFalloffRadius)
	{
		Radius = ComponentRadius;
		InvRadius = 1.0f / ComponentRadius;
		ShadowFalloffScale = 1.0f / Max(DELTA,1.0f - ComponentMinShadowFalloffRadius / ComponentRadius);
		ShadowFalloffBias = -ComponentMinShadowFalloffRadius / (ComponentRadius - ComponentMinShadowFalloffRadius);
	}
};

/**
 * The scene info for a point light.
 * This is in LightRendering.h because it is used by both PointLightComponent.cpp and SpotLightComponent.cpp
 */
template<typename LightPolicyType>
class TPointLightSceneInfo : public FPointLightSceneInfoBase
{
public:

	/** Initialization constructor. */
	TPointLightSceneInfo(const UPointLightComponent* Component)
		:	FPointLightSceneInfoBase(Component)
	{
	}

	// FLightSceneInfo interface.

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
		if( Color.GetMax() > 0.f )
		{
			return DrawLitDynamicMesh<LightPolicyType>(View, this, Mesh, bBackFace, bPreFog, TRUE, bUseTranslucencyLightAttenuation, PrimitiveSceneInfo, TranslucentPreShadowInfo, HitProxyId);
		}
		return FALSE;
	}

	/**
	* @return modulated shadow projection pixel shader for this light type
	*/
	virtual class FShadowProjectionPixelShaderInterface* GetModShadowProjPixelShader(UBOOL bRenderingBeforeLight) const
	{
		return GetModProjPixelShaderRef <LightPolicyType> (GetEffectiveShadowFilterQuality(bRenderingBeforeLight));
	}

	/**
	* @return Branching PCF modulated shadow projection pixel shader for this light type
	*/
	virtual class FBranchingPCFProjectionPixelShaderInterface* GetBranchingPCFModProjPixelShader(UBOOL bRenderingBeforeLight) const
	{
		return GetBranchingPCFModProjPixelShaderRef <LightPolicyType> (GetEffectiveShadowFilterQuality(bRenderingBeforeLight));
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

	/** The DPG info for the point light. */
	TLightSceneDPGInfo<LightPolicyType> DPGInfos[SDPG_MAX_SceneRender];
};

#endif
