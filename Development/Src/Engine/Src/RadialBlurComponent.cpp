/*=============================================================================
	RadialBlurComponent.cpp: URadialBlurComponent implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"

/*-----------------------------------------------------------------------------
 URadialBlurComponent
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(URadialBlurComponent);

/**
 * Cache the current parent to world of the component before attachment.
 *
 * @param ParentToWorld - The ParentToWorld transform the component is attached to.
 */
void URadialBlurComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	Super::SetParentToWorld(ParentToWorld);

	LocalToWorld = ParentToWorld;
}

/**
 * Attach the component to its owner and scene.  Also creates the radial blur
 * scene proxy and adds it to the scene.
 */
void URadialBlurComponent::Attach()
{
	Super::Attach();
	Scene->AddRadialBlur(this);
}

/**
 * If the ParentToWorld for the component was updated then
 * update its scene proxy.
 */
void URadialBlurComponent::UpdateTransform()
{
	Super::UpdateTransform();

	Scene->RemoveRadialBlur(this);
	Scene->AddRadialBlur(this);
}

/**
 * Detach the component to its owner and scene.  Also removes the radial blur
 * scene proxy for the component from the scene.
 */
void URadialBlurComponent::Detach(UBOOL bWillReattach)
{
	Scene->RemoveRadialBlur(this);
	Super::Detach(bWillReattach);
}

/**
 * Set a new material. And reattach component.
 *
 * @param InMaterial - Material to affect radial blur opacity/color
 */
void URadialBlurComponent::SetMaterial(UMaterialInterface* InMaterial)
{
	Material = InMaterial;

	BeginDeferredReattach();
}

/**
 * Set new blur scale. And reattach component.
 *
 * @param InBlurScale - Scale for the overall blur vectors
 */
void URadialBlurComponent::SetBlurScale(FLOAT InBlurScale)
{
	BlurScale = InBlurScale;

	BeginDeferredReattach();
}

/**
 * Set new blur falloff exponent. And reattach component.
 *
 * @param InBlurFalloffExponent - Exponent for falloff rate of blur vectors
 */
void URadialBlurComponent::SetBlurFalloffExponent(FLOAT InBlurFalloffExponent)
{
	BlurFalloffExponent = InBlurFalloffExponent;

	BeginDeferredReattach();
}

/**
 * Set new blur opacity. And reattach component.
 *
 * @param InBlurOpacity - Amount to alpha blend the blur effect with the existing scene
 */
void URadialBlurComponent::SetBlurOpacity(FLOAT InBlurOpacity)
{
	BlurOpacity = InBlurOpacity;

	BeginDeferredReattach();
}

/**
 * Toggle if rendering is enabled. And reattach component.
 *
 * @param InBlurOpacity - Amount to alpha blend the blur effect with the existing scene
 */
void URadialBlurComponent::SetEnabled(UBOOL bInEnabled)
{
	bEnabled = bInEnabled;

	BeginDeferredReattach();
}

/*-----------------------------------------------------------------------------
 FRadialBlurVertexShader
-----------------------------------------------------------------------------*/

/**
 * A vertex shader for drawing full screen radial blur
 */
class FRadialBlurVertexShader : public FShader
{
	DECLARE_SHADER_TYPE(FRadialBlurVertexShader,Material);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return Material->IsUsedWithRadialBlur() || Material->IsSpecialEngineMaterial();
	}

	FRadialBlurVertexShader( )	{ }
	FRadialBlurVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		WorldCenterPosParameter.Bind(Initializer.ParameterMap,TEXT("WorldCenterPos"),TRUE);
	}

	void SetParameters( const FSceneView* View, const FVector4& WorldCenterPos )
	{
		SetVertexShaderValue(GetVertexShader(),WorldCenterPosParameter,WorldCenterPos);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << WorldCenterPosParameter;
		return bShaderHasOutdatedParameters;
	}
private:	
	FShaderParameter WorldCenterPosParameter;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FRadialBlurVertexShader,TEXT("RadialBlurScreenShader"),TEXT("MainRadialBlurVS"),SF_Vertex,0,0);

/*-----------------------------------------------------------------------------
 FRadialBlurPixelShader
-----------------------------------------------------------------------------*/

/**
 * A pixel shader for drawing full screen radial blur
 */
class FRadialBlurPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FRadialBlurPixelShader,Material);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return Material->IsUsedWithRadialBlur() || Material->IsSpecialEngineMaterial();
	}

	FRadialBlurPixelShader() {}
	FRadialBlurPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		RadialBlurScaleParameter.Bind(Initializer.ParameterMap,TEXT("RadialBlurScale"),TRUE);
		RadialBlurFalloffExpParameter.Bind(Initializer.ParameterMap,TEXT("RadialBlurFalloffExp"),TRUE);
		RadialBlurOpacityParameter.Bind(Initializer.ParameterMap,TEXT("RadialBlurOpacity"),TRUE);
#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			RadialBlurScreenPositionCenterMobileParameter.Bind(Initializer.ParameterMap,TEXT("RadialBlurScreenPositionCenter"),TRUE);
		}
#endif
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		MaterialParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters( 
		const FSceneView* View, 
		const FMaterialRenderProxy* MaterialProxy, 
		const FRadialBlurSceneProxy* RadialBlurInfo, 
		FLOAT RadialBlurScale, 
		FLOAT RadialBlurFalloffExp,
		FLOAT RadialBlurOpacity,
		const FVector4& ScreenPositionCenter)
	{
		FMaterialRenderContext MaterialRenderContext(MaterialProxy, *MaterialProxy->GetMaterial(), View->Family->CurrentWorldTime, View->Family->CurrentRealTime, View);
		MaterialParameters.Set(this,MaterialRenderContext);
		SceneTextureParameters.Set(View, this, SF_Bilinear);

		// the (non-mobile) shader used to divide the offset vec by 4 - it's been removed, but the scale is applied here to keep existing data consistent
		SetPixelShaderValue(GetPixelShader(),RadialBlurScaleParameter,RadialBlurScale*0.25f);
		SetPixelShaderValue(GetPixelShader(),RadialBlurFalloffExpParameter,RadialBlurFalloffExp);
		SetPixelShaderValue(GetPixelShader(),RadialBlurOpacityParameter,RadialBlurOpacity);

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			// Non mobile version projects WorldCenterPos in the vertex shader to support RealD.  
			// On mobile we just calculate it on CPU and send it to the pixel shader as a uniform.
			const FScaleMatrix ClipSpaceFixScale(FVector(1.0f, 1.0f, 2.0f));
			const FTranslationMatrix ClipSpaceFixTranslate(FVector(0.0f, 0.0f, -1.0f));	
			const FMatrix AdjustedViewProjectionMatrix( View->TranslatedViewProjectionMatrix * ClipSpaceFixScale * ClipSpaceFixTranslate );
			FVector4 WorldCenterPos = AdjustedViewProjectionMatrix.TransformFVector4(ScreenPositionCenter);
			WorldCenterPos.W = abs(WorldCenterPos.W);
			WorldCenterPos = WorldCenterPos/WorldCenterPos.W;
			WorldCenterPos.X = Clamp(WorldCenterPos.X, -1.f, 1.f);
			WorldCenterPos.Y = Clamp(WorldCenterPos.Y, -1.f, 1.f);

			SetPixelShaderValue(GetPixelShader(), RadialBlurScreenPositionCenterMobileParameter, WorldCenterPos);
		}
#endif

	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << RadialBlurScaleParameter;
		Ar << RadialBlurFalloffExpParameter;
		Ar << RadialBlurOpacityParameter;
		Ar << SceneTextureParameters;
		Ar << MaterialParameters;
		RadialBlurScaleParameter.SetShaderParamName(TEXT("RadialBlurScale"));
		RadialBlurFalloffExpParameter.SetShaderParamName(TEXT("RadialBlurFalloffExp"));
		RadialBlurOpacityParameter.SetShaderParamName(TEXT("RadialBlurOpacity"));
#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			RadialBlurScreenPositionCenterMobileParameter.SetShaderParamName(TEXT("RadialBlurScreenPositionCenter"));
			SceneTextureParameters.SceneColorTextureParameter.SetBaseIndex(0,TRUE);
		}
#endif
		return bShaderHasOutdatedParameters;
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FShaderParameter RadialBlurScaleParameter;
	FShaderParameter RadialBlurFalloffExpParameter;
	FShaderParameter RadialBlurOpacityParameter;
	FShaderParameter RadialBlurScreenPositionCenterMobileParameter;
	FSceneTextureShaderParameters SceneTextureParameters;
	FMaterialPixelShaderParameters MaterialParameters;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FRadialBlurPixelShader,TEXT("RadialBlurScreenShader"),TEXT("MainRadialBlurPS"),SF_Pixel,0,0);

/*-----------------------------------------------------------------------------
 FRadialBlurVertexShader
-----------------------------------------------------------------------------*/

/**
 * A vertex shader for drawing radial blur velocities
 */
class FRadialBlurVelocityVertexShader : public FRadialBlurVertexShader
{
	DECLARE_SHADER_TYPE(FRadialBlurVelocityVertexShader,Material);
public:

	FRadialBlurVelocityVertexShader() {}
	FRadialBlurVelocityVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FRadialBlurVertexShader(Initializer)
	{
	}
};

//IMPLEMENT_MATERIAL_SHADER_TYPE(,FRadialBlurVelocityVertexShader,TEXT("RadialBlurScreenShader"),TEXT("MainRadialBlurVelocityVS"),SF_Vertex,0,0);

/*-----------------------------------------------------------------------------
 FRadialBlurVelocityPixelShader
-----------------------------------------------------------------------------*/

/**
 * A pixel shader for drawing radial blur velocities
 */
class FRadialBlurVelocityPixelShader : public FRadialBlurPixelShader
{
	DECLARE_SHADER_TYPE(FRadialBlurVelocityPixelShader,Material);
public:

	FRadialBlurVelocityPixelShader() {}
	FRadialBlurVelocityPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FRadialBlurPixelShader(Initializer)
	{		
	}	
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FRadialBlurVelocityPixelShader,TEXT("RadialBlurScreenShader"),TEXT("MainRadialBlurVelocityPS"),SF_Pixel,0,0);

/*-----------------------------------------------------------------------------
 FRadialBlurSceneProxy
-----------------------------------------------------------------------------*/

/**
 * Constructor
 * @param InRadialBlurComponent - Component this proxy is being created for
 */
FRadialBlurSceneProxy::FRadialBlurSceneProxy(class URadialBlurComponent* InRadialBlurComponent)
:	RadialBlurComponent(InRadialBlurComponent)
,	WorldPosition(RadialBlurComponent->LocalToWorld.GetOrigin())
,	MaterialProxy(NULL)
,	DesiredDPG(InRadialBlurComponent->bRenderAsVelocity ? SDPG_World : InRadialBlurComponent->DepthPriorityGroup)
,	BlurScale(Clamp<FLOAT>(InRadialBlurComponent->BlurScale,-10.f,10.f))
,	BlurFalloffExp(Clamp<FLOAT>(InRadialBlurComponent->BlurFalloffExponent,-100.f,100.f))
,	BlurOpacity(Clamp<FLOAT>(InRadialBlurComponent->BlurOpacity,0.f,1.f))
,	MaxCullDistance(Clamp<FLOAT>(InRadialBlurComponent->MaxCullDistance,1.f,10000.0f))
,	DistanceFalloffExponent(Clamp<FLOAT>(InRadialBlurComponent->DistanceFalloffExponent,1/1000.f,1000.f))
,	bRenderAsVelocity(InRadialBlurComponent->bRenderAsVelocity)
{
	if (InRadialBlurComponent->Material != NULL &&
		InRadialBlurComponent->Material->CheckMaterialUsage(MATUSAGE_RadialBlur))
	{
		MaterialProxy = InRadialBlurComponent->Material->GetRenderProxy(FALSE);
	}
	else if (GEngine->DefaultMaterial != NULL)
	{
		MaterialProxy = GEngine->DefaultMaterial->GetRenderProxy(FALSE);
	}
}

/**
 * Determine if the radial blur proxy is renderable for the current view
 *
 * @param View - View region that is currently being rendered for the scene
 * @param DPGIndex - Current Depth Priority Group being rendered for the scene
 * @param Flags - Scene rendering flags
 * @return TRUE if the effect should be rendered
 */
UBOOL FRadialBlurSceneProxy::IsRenderable(const class FSceneView* View,UINT DPGIndex,DWORD Flags) const
{
	UBOOL bShouldRender = (
		DPGIndex == DesiredDPG && 
		BlurOpacity > KINDA_SMALL_NUMBER &&
		Abs(BlurScale) > KINDA_SMALL_NUMBER &&		
		MaterialProxy && MaterialProxy->GetMaterial() &&
		(FVector(View->ViewOrigin) - WorldPosition).SizeSquared() < (MaxCullDistance*MaxCullDistance)
		);
	return bShouldRender;
}

/**
 * Calculate the final blur scale to render radial blur based on 
 * configured parameters and distance to the view origin.
 *
 * @param View - View region that is currently being rendered for the scene
 * @return Blur scale float (can be negative)
 */
FLOAT FRadialBlurSceneProxy::CalcBlurScale(const class FSceneView* View) const
{
	// world space view direction
	FVector ViewDirVec(View->ViewMatrix.M[0][2], View->ViewMatrix.M[1][2], View->ViewMatrix.M[2][2]);
	// world space vector to radial blur actor position
	FVector RadialBlurDirVec(WorldPosition - View->ViewOrigin);

	FLOAT DistanceRatio = Min<FLOAT>(RadialBlurDirVec.Size() / MaxCullDistance,1.0f);
	FLOAT DistanceFalloff = appPow(1.f - DistanceRatio,DistanceFalloffExponent);
	FLOAT AngleFalloff = Max<FLOAT>(ViewDirVec.SafeNormal() | RadialBlurDirVec.SafeNormal(),0.0f);
	return BlurScale * DistanceFalloff * AngleFalloff;
}

/**
 * Draw the radial blur effect for this proxy directly to the scene color buffer.
 *
 * @param View - View region that is currently being rendered for the scene
 * @param DPGIndex - Current Depth Priority Group being rendered for the scene
 * @param Flags - Scene rendering flags
 * @return TRUE if the effect was rendered
 */
UBOOL FRadialBlurSceneProxy::Draw(const class FSceneView* View,UINT DPGIndex,DWORD Flags)
{
	UBOOL bDirty = FALSE;

	if (IsRenderable(View,DPGIndex,Flags))
	{
		const FMaterialShaderMap* MaterialShaderMap = MaterialProxy->GetMaterial()->GetShaderMap();
		
		// set vertex shader parameters
		FRadialBlurVertexShader* VertexShader = MaterialShaderMap->GetShader<FRadialBlurVertexShader>();
		// offset with the current view translation to get the world position
		FVector4 WorldCenterPos(WorldPosition + View->PreViewTranslation);
		VertexShader->SetParameters(View,WorldCenterPos);

		// set pixel shader parameters
		FRadialBlurPixelShader* PixelShader = MaterialShaderMap->GetShader<FRadialBlurPixelShader>();

		PixelShader->SetParameters(View,MaterialProxy, this, CalcBlurScale(View), BlurFalloffExp, BlurOpacity, WorldCenterPos);

		// Create and cache the bound shader state object if it isn't valid
		if (!IsValidRef(BoundShaderState))
		{
			DWORD Strides[MaxVertexElementCount];
			appMemzero(Strides, sizeof(Strides));
			Strides[0] = sizeof(FFilterVertex);

			BoundShaderState = RHICreateBoundShaderState(
				GFilterVertexDeclaration.VertexDeclarationRHI, 
				Strides, 
				VertexShader->GetVertexShader(), 
				PixelShader->GetPixelShader(),
				EGST_RadialBlur
				);
		}
		RHISetBoundShaderState(BoundShaderState);

		UBOOL bUseAlphaBlending = TRUE;
#if WITH_MOBILE_RHI
		// mobile version blends in the shader
		bUseAlphaBlending = FALSE;
#endif
		if (bUseAlphaBlending)
		{
			// Enable alpha blending
			RHISetBlendState(TStaticBlendState<BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_Zero,BF_One>::GetRHI());
		}
		else
		{
			RHISetBlendState(TStaticBlendState<>::GetRHI());
			RHISetColorWriteMask(CW_RGB);
		}

		// No depth tests/writes
		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
		// No culling or wireframe
		RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());

		const INT TextureSizeX=1;
		const INT TextureSizeY=1;
		DrawDenormalizedQuad(
			0,0,
			GSceneRenderTargets.GetBufferSizeX(),GSceneRenderTargets.GetBufferSizeY(),
			0,0,
			TextureSizeX,TextureSizeY,
			GSceneRenderTargets.GetBufferSizeX(),GSceneRenderTargets.GetBufferSizeY(),
			TextureSizeX,TextureSizeY
			);

		bDirty = TRUE;

		if (!bUseAlphaBlending)
		{
			RHISetColorWriteMask(CW_RGBA);
		}

	}
	
	return bDirty;
}

/**
 * Draw the radial blur effect for this proxy to the velocity buffer.
 *
 * @param View - View region that is currently being rendered for the scene
 * @param DPGIndex - Current Depth Priority Group being rendered for the scene
 * @param Flags - Scene rendering flags
 * @return TRUE if the effect was rendered
 */
UBOOL FRadialBlurSceneProxy::DrawVelocity(const class FSceneView* View,UINT DPGIndex,DWORD Flags)
{
	UBOOL bDirty = FALSE;

	if (IsRenderable(View,DPGIndex,Flags))
	{
		const FMaterialShaderMap* MaterialShaderMap = MaterialProxy->GetMaterial()->GetShaderMap();
		
		// set vertex shader parameters
		//FRadialBlurVertexShader* VertexShader = MaterialShaderMap->GetShader<FRadialBlurVelocityVertexShader>();
		FRadialBlurVertexShader* VertexShader = MaterialShaderMap->GetShader<FRadialBlurVertexShader>();
		// offset with the current view translation to get the world position
		FVector4 WorldCenterPos(WorldPosition + View->PreViewTranslation);
		VertexShader->SetParameters(View,WorldCenterPos);

		// set pixel shader parameters
		FRadialBlurPixelShader* PixelShader = MaterialShaderMap->GetShader<FRadialBlurVelocityPixelShader>();

		PixelShader->SetParameters(View, MaterialProxy, this, CalcBlurScale(View), BlurFalloffExp, BlurOpacity, WorldCenterPos);

		// Create and cache the bound shader state object if it isn't valid
		if (!IsValidRef(BoundShaderStateVelocity))
		{
			DWORD Strides[MaxVertexElementCount];
			appMemzero(Strides, sizeof(Strides));
			Strides[0] = sizeof(FFilterVertex);

			BoundShaderStateVelocity = RHICreateBoundShaderState(
				GFilterVertexDeclaration.VertexDeclarationRHI, 
				Strides, 
				VertexShader->GetVertexShader(), 
				PixelShader->GetPixelShader(),
				EGST_None
				);
		}
		RHISetBoundShaderState(BoundShaderStateVelocity);

		// No depth tests/writes
		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
		// No culling or wireframe
		RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());

		const INT TextureSizeX=1;
		const INT TextureSizeY=1;
		DrawDenormalizedQuad(
			0,0,
			GSceneRenderTargets.GetVelocityBufferSizeX(),GSceneRenderTargets.GetVelocityBufferSizeY(),
			0,0,
			TextureSizeX,TextureSizeY,
			GSceneRenderTargets.GetVelocityBufferSizeX(),GSceneRenderTargets.GetVelocityBufferSizeY(),
			TextureSizeX,TextureSizeY
			);

		bDirty = TRUE;
	}

	return bDirty;
}
