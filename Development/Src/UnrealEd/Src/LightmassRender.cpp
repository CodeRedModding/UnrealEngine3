/*=============================================================================
	LightmassRender.cpp: lightmass rendering-related implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"

#if WITH_MANAGED_CODE

#include "LightmassPublic.h"
#include "Lightmass.h"
#include "LightmassRender.h"

// FLightmassMaterialCompiler - A proxy compiler that overrides various compiler functions for potential problem expressions.
struct FLightmassMaterialCompiler : public FProxyMaterialCompiler
{
	UBOOL bUsedWithSpeedTree;

	FLightmassMaterialCompiler(FMaterialCompiler* InCompiler, UBOOL bInUsedWithSpeedTree) :
		FProxyMaterialCompiler(InCompiler),
		bUsedWithSpeedTree(bInUsedWithSpeedTree)
	{}

	/**
	 *	Add the shader code for sampling the scene depth
	 *	@param	bNormalize - @todo implement
	 *	@param	CoordinateIdx - index of shader code for user specified tex coords
	 */
	virtual INT SceneTextureDepth( UBOOL bNormalize, INT CoordinateIdx)
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered SceneTextureDepth... Forcing constant 0.0f."));
		return Compiler->Constant(0.0f);
	}

	virtual INT DestColor()
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered DestColor... Forcing constant 0.0f,0.0f,0.0f."));
		return Compiler->Constant3(0.0f,0.0f,0.0f);
	}

	virtual INT DestDepth(UBOOL bNormalize)
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered DestDepth... Forcing constant 0.0f."));
		return Compiler->Constant(0.0f);
	}

	virtual INT DepthBiasedAlpha(INT SrcAlphaIdx, INT BiasIdx, INT BiasScaleIdx)
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered DepthBiasedAlpha... Passing through source alpha."));
		return SrcAlphaIdx;
	}
	
	/**
	 *	Add the shader code for sampling from the scene texture
	 *	@param	TexType - scene texture type to sample from
	 *	@param	CoordinateIdx - index of shader code for user specified tex coords
	 *	@param	ScreenAlign - Whether to map [0,1] UVs to the view within the back buffer
	 */
	virtual INT SceneTextureSample(BYTE TexType, INT CoordinateIdx, UBOOL ScreenAlign)
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered SceneTextureSample... Forcing constant (0.0f,0.0f,0.0f)."));
		return Compiler->Constant3(0.0f,0.0f,0.0f);
	}

	virtual INT ParticleMacroUV(UBOOL bUseViewSpace)
	{
		return Compiler->ParticleMacroUV(bUseViewSpace);
	}

	virtual INT WorldPosition()
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered WorldPosition... Forcing constant (0.0f,0.0f,0.0f)."));
		return Compiler->Constant3(0.0f,0.0f,0.0f);
	}

	virtual INT ObjectWorldPosition() 
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered ObjectWorldPosition... Forcing constant (0.0f,0.0f,0.0f)."));
		return Compiler->Constant3(0.0f,0.0f,0.0f);
	}

	virtual INT ActorWorldPosition() 
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered ActorWorldPosition... Forcing constant (0.0f,0.0f,0.0f)."));
		return Compiler->Constant3(0.0f,0.0f,0.0f);
	}

	virtual INT ObjectRadius() 
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered ObjectRadius... Forcing constant 500.0f."));
		return Compiler->Constant(500);
	}

	virtual INT CameraWorldPosition()
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered CameraWorldPosition... Forcing constant (0.0f,0.0f,0.0f)."));
		return Compiler->Constant3(0.0f,0.0f,0.0f);
	}

	virtual INT CameraVector()
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered CameraVector... Forcing constant (0.0f,0.0f,1.0f)."));
		return Compiler->Constant3(0.0f,0.0f,1.0f);
	}

	virtual INT LightVector()
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered LightVector... Forcing constant (1.0f,0.0f,0.0f)."));
		return Compiler->Constant3(1.0f,0.0f,0.0f);
	}

	virtual INT ReflectionVector()
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered ReflectionVector... Forcing constant (0.0f,0.0f,-1.0f)."));
		return Compiler->Constant3(0.0f,0.0f,-1.0f);
	}

	/**
	 *	Generate shader code for transforming a vector
	 */
	virtual INT TransformVector(BYTE SourceCoordType,BYTE DestCoordType,INT A)
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered TransformVector... Passing thru source vector untouched."));
		return A;
	}

	/**
	 *	Generate shader code for transforming a position
	 *
	 *	@param	CoordType - type of transform to apply. see EMaterialExpressionTransformPosition 
	 *	@param	A - index for input vector parameter's code
	 */
	virtual INT TransformPosition(BYTE SourceCoordType,BYTE DestCoordType,INT A)
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered TransformPosition... Passing thru source vector untouched."));
		return A;
	}

	virtual INT VertexColor()
	{
		if (!bUsedWithSpeedTree)
		{
			debugfSlow(TEXT("Lightmass material compiler has encountered VertexColor... Forcing constant (1.0f,1.0f,1.0f,1.0f)."));
			return Compiler->Constant4(1.0f,1.0f,1.0f,1.0f);
		}
		else
		{
			warnf(TEXT("Lightmass material compiler has encountered VertexColor w/ bUsedWithSpeedTree... Forcing constant (1.0f,1.0f,1.0f,0.0f)."));
			return Compiler->Constant4(1.0f,1.0f,1.0f,0.0f);
		}
	}

	virtual INT RealTime()
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered RealTime... Forcing constant 0.0f."));
		return Compiler->Constant(0.0f);
	}

	virtual INT GameTime()
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered GameTime... Forcing constant 0.0f."));
		return Compiler->Constant(0.0f);
	}

	virtual INT FlipBookOffset(UTexture* InFlipBook)
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered FlipBookOffset... Forcing constant (0.0f, 0.0f)."));
		return Compiler->Constant2(0.0f, 0.0f);
	}

	virtual INT LensFlareIntesity(void)
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered LensFlareIntesity... Forcing constant 1.0f."));
		return Compiler->Constant(1.0f);
	}

	virtual INT LensFlareOcclusion(void)
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered LensFlareOcclusion... Forcing constant 1.0f."));
		return Compiler->Constant(1.0f);
	}

	virtual INT LensFlareRadialDistance(void)
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered LensFlareRadialDistance... Forcing constant 0.0f."));
		return Compiler->Constant(0.0f);
	}

	virtual INT LensFlareRayDistance(void)
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered LensFlareRayDistance... Forcing constant 0.0f."));
		return Compiler->Constant(0.0f);
	}

	virtual INT LensFlareSourceDistance(void)
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered LensFlareSourceDistance... Forcing constant 0.0f."));
		return Compiler->Constant(0.0f);
	}

	virtual INT LightmassReplace(INT Realtime, INT Lightmass) { return Lightmass; }

	virtual INT OcclusionPercentage()
	{
		debugfSlow(TEXT("Lightmass material compiler has encountered OcclusionPercentage... Forcing constant 1.0f."));
		return Compiler->Constant(1.0f);
	}
};

/**
 * Class for rendering previews of material expressions in the material editor's linked object viewport.
 */
class FLightmassMaterialProxy : public FMaterial, public FMaterialRenderProxy
{
public:
	FLightmassMaterialProxy()
	{}

	FLightmassMaterialProxy(UMaterialInterface* InMaterialInterface, EMaterialProperty InPropertyToCompile) :
		  MaterialInterface(InMaterialInterface)
		, PropertyToCompile(InPropertyToCompile)
	{
		// Have to properly handle compilation of static switches in MaterialInstance* cases...
		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(InMaterialInterface);
		if (MaterialInstance)
		{
			// Get a pointer to the parent material.
			UMaterial* ParentMaterial = MaterialInstance->GetMaterial();

			// Override the static switches with the MaterialInstance settings.
			FStaticParameterSet StaticParamSet;
			if (ParentMaterial)
			{
				MaterialInstance->GetStaticParameterValues(&StaticParamSet);
				ParentMaterial->SetStaticParameterOverrides(&StaticParamSet);
			}

			// always use high quality for lightmass
			CacheShaders(&StaticParamSet, GRHIShaderPlatform, MSQ_HIGH, TRUE);

			// Restore the parent material static settings.
			if (ParentMaterial)
			{
				ParentMaterial->ClearStaticParameterOverrides();
			}
		}
		else
		{
			// always use high quality for lightmass
			CacheShaders(GRHIShaderPlatform, MSQ_HIGH);
		}
	}

	/**
	 * Should the shader for this material with the given platform, shader type and vertex 
	 * factory type combination be compiled
	 *
	 * @param Platform		The platform currently being compiled for
	 * @param ShaderType	Which shader is being compiled
	 * @param VertexFactory	Which vertex factory is being compiled (can be NULL)
	 *
	 * @return TRUE if the shader should be compiled
	 */
	virtual UBOOL ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
	{
		if (VertexFactoryType == FindVertexFactoryType(FName(TEXT("FLocalVertexFactory"), FNAME_Find)))
		{
			// we only need the non-light-mapped, base pass, local vertex factory shaders for drawing an opaque Material Tile
			// @todo: Added a FindShaderType by fname or something"

			if(appStristr(ShaderType->GetName(), TEXT("BasePassVertexShaderFNoLightMapPolicyFNoDensityPolicy")))
			{
				return TRUE;
			}
			else if(appStristr(ShaderType->GetName(), TEXT("BasePassPixelShaderFNoLightMapPolicy")))
			{
				return TRUE;
			}
		}

		return FALSE;
	}

	////////////////
	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterial() const
	{
		if(GetShaderMap())
		{
			return this;
		}
		else
		{
			return GEngine->DefaultMaterial->GetRenderProxy(FALSE)->GetMaterial();
		}
	}

	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		return MaterialInterface->GetRenderProxy(0)->GetVectorValue(ParameterName, OutValue, Context);
	}

	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
	{
		return MaterialInterface->GetRenderProxy(0)->GetScalarValue(ParameterName, OutValue, Context);
	}

	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		return MaterialInterface->GetRenderProxy(0)->GetTextureValue(ParameterName,OutValue,Context);
	}

	// Material properties.
	/** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
	virtual INT CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler) const
	{
		Compiler->SetMaterialProperty(Property);
		// MAKE SURE THIS MATCHES THE CHART IN WillFillData
		// 						  RETURNED VALUES (F16 'textures')
		// 	BLEND MODE  | DIFFUSE     | SPECULAR     | EMISSIVE    | NORMAL    | TRANSMISSIVE              |
		// 	------------+-------------+--------------+-------------+-----------+---------------------------|
		// 	Opaque      | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | 0 (EMPTY)                 |
		// 	Masked      | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | Opacity Mask              |
		// 	Translucent | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | (Emsv | Diffuse)*Opacity  |
		// 	Additive    | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | (Emsv | Diffuse)*Opacity  |
		// 	Modulative  | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | Emsv | Diffuse            |
		// 	SoftMasked  | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | Opacity Mask              |
		// 	------------+-------------+--------------+-------------+-----------+---------------------------|
		if( Property == MP_EmissiveColor )
		{
			UMaterial* Material = MaterialInterface->GetMaterial();
			check(Material);

			FLightmassMaterialCompiler ProxyCompiler(Compiler, Material->bUsedWithSpeedTree);
			switch (PropertyToCompile)
			{
			case MP_EmissiveColor:
				// Emissive is ALWAYS returned...
				return Compiler->ForceCast(Material->EmissiveColor.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
			case MP_DiffuseColor:
				// Only return for Opaque and Masked...
				if (Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
				{
					if (Material->LightingModel != MLM_Custom)
					{
						return Compiler->ForceCast(Material->DiffuseColor.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
					}
					else
					{
						return Compiler->ForceCast(Material->CustomSkylightDiffuse.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
					}
				}
				break;
			case MP_SpecularColor: 
				// Only return for Opaque and Masked...
				if (Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
				{
					if (Material->LightingModel != MLM_Custom)
					{
						return Compiler->AppendVector(
							Compiler->ForceCast(Material->SpecularColor.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE), 
							Compiler->ForceCast(Material->SpecularPower.Compile(&ProxyCompiler,15.0f),MCT_Float1));
					}
					else
					{
						return Compiler->Constant4(0.0f,0.0f,0.0f,0.0f);
					}
				}
				break;
			case MP_Normal:
				// Only return for Opaque and Masked...
				if (Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
				{
					return Compiler->ForceCast( Material->Normal.Compile(&ProxyCompiler, FVector( 0, 0, 1 ) ), MCT_Float3, TRUE, TRUE );
				}
				break;
			
			case MP_Opacity:
				if (Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
				{
					ULandscapeMaterialInstanceConstant* MIC = Cast<ULandscapeMaterialInstanceConstant>(MaterialInterface);
					if (Material->bUsedWithLandscape && MIC)
					{
						//ULandscapeMaterialInstanceConstant* ParentMIC = Cast<ULandscapeMaterialInstanceConstant>(MIC->Parent);
						if (MIC->DataWeightmapIndex != INDEX_NONE && MIC->DataWeightmapSize > 0)
						{
							return Compiler->Sub( Compiler->Constant(1.0f), 
								Compiler->Dot(
								Compiler->TextureSample(
								Compiler->TextureParameter(FName(*FString::Printf(TEXT("Weightmap%d"), MIC->DataWeightmapIndex)), GEngine->WeightMapPlaceholderTexture), 
								Compiler->Add(
								Compiler->Mul( 
								Compiler->Floor( 
								Compiler->Mul(Compiler->Add( Compiler->TextureCoordinate(1, FALSE, FALSE), Compiler->Constant(-0.5f/MIC->DataWeightmapSize) ), Compiler->Constant((FLOAT)MIC->DataWeightmapSize) ) ),
								Compiler->Constant(1.f/(FLOAT)MIC->DataWeightmapSize) ),
								Compiler->Constant(0.5f/MIC->DataWeightmapSize) ) ),
								Compiler->VectorParameter(FName(*FString::Printf(TEXT("LayerMask_%s"), *ALandscape::DataWeightmapName.ToString())), FLinearColor::Black) ) );
						}
					}
					return Material->OpacityMask.Compile(&ProxyCompiler, 1.0f);
				}
				else if ((IsTranslucentBlendMode((EBlendMode)Material->BlendMode) || Material->BlendMode == BLEND_DitheredTranslucent) && Material->GetCastShadowAsMasked())
				{
					return Material->Opacity.Compile(&ProxyCompiler, 1.0f);
				}
				else if ((Material->BlendMode == BLEND_Modulate) || (Material->BlendMode == BLEND_ModulateAndAdd))
				{
					if (Material->LightingModel == MLM_Unlit)
					{
						return Compiler->ForceCast(Material->EmissiveColor.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
					}
					else
					{
						if (Material->LightingModel != MLM_Custom)
						{
							return Compiler->ForceCast(Material->DiffuseColor.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
						}
						else
						{
							return Compiler->ForceCast(Material->CustomSkylightDiffuse.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
						}
					}
				}
				else if ((Material->BlendMode == BLEND_Translucent) || (Material->BlendMode == BLEND_AlphaComposite) || (Material->BlendMode == BLEND_Additive) || (Material->BlendMode == BLEND_DitheredTranslucent))
				{
					INT ColoredOpacity = -1;
					if (Material->LightingModel == MLM_Unlit)
					{
						ColoredOpacity = Compiler->ForceCast(Material->EmissiveColor.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
					}
					else
					{
						if (Material->LightingModel != MLM_Custom)
						{
							ColoredOpacity = Compiler->ForceCast(Material->DiffuseColor.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
						}
						else
						{
							ColoredOpacity = Compiler->ForceCast(Material->CustomLighting.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
						}
					}
					return Compiler->Lerp(Compiler->Constant3(1.0f, 1.0f, 1.0f), ColoredOpacity, Compiler->ForceCast(Material->Opacity.Compile(&ProxyCompiler,1.0f),MCT_Float1));
				}
				break;
			default:
				return Compiler->Constant(1.0f);
			}
	
			return Compiler->Constant(0.0f);
		}
		else if( Property == MP_WorldPositionOffset)
		{
			//This property MUST return 0 as a default or during the process of rendering textures out for lightmass to use, pixels will be off by 1.
			return Compiler->Constant(0.0f);
		}
		else
		{
			return Compiler->Constant(1.0f);
		}
	}

	virtual FString GetMaterialUsageDescription() const { return FString::Printf(TEXT("FLightmassMaterialRenderer %s"), MaterialInterface ? *MaterialInterface->GetName() : TEXT("NULL")); }
	virtual UBOOL IsTwoSided() const 
	{ 
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->TwoSided == 1);
		}
		return FALSE;
	}
	virtual UBOOL RenderTwoSidedSeparatePass() const
	{ 
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->TwoSidedSeparatePass == 1);
		}
		return FALSE;
	}
	virtual UBOOL RenderLitTranslucencyPrepass() const
	{ 
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bUseLitTranslucencyDepthPass == 1);
		}
		return FALSE;
	}
	virtual UBOOL RenderLitTranslucencyDepthPostpass() const
	{ 
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bUseLitTranslucencyPostRenderDepthPass == 1);
		}
		return FALSE;
	}
	virtual UBOOL CastLitTranslucencyShadowAsMasked() const
	{ 
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bCastLitTranslucencyShadowAsMasked == 1);
		}
		return FALSE;
	}
	virtual UBOOL NeedsDepthTestDisabled() const
	{
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bDisableDepthTest == 1);
		}
		return FALSE;
	}
	virtual UBOOL IsLightFunction() const
	{
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bUsedAsLightFunction == 1);
		}
		return FALSE;
	}
	virtual UBOOL IsUsedWithFogVolumes() const
	{
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bUsedWithFogVolumes == 1);
		}
		return FALSE;
	}
	virtual UBOOL IsSpecialEngineMaterial() const
	{
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bUsedAsSpecialEngineMaterial == 1);
		}
		return FALSE;
	}
	virtual UBOOL IsUsedWithMobileLandscape() const
	{
		return FALSE;
	}
	virtual UBOOL IsTerrainMaterial() const
	{
		return FALSE;
	}
	virtual UBOOL IsDecalMaterial() const
	{
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bUsedWithDecals == 1);
		}
		return FALSE;
	}
	virtual UBOOL IsWireframe() const
	{
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->Wireframe == 1);
		}
		return FALSE;
	}
	virtual UBOOL IsDistorted() const								{ return FALSE; }
	virtual UBOOL HasSubsurfaceScattering() const					{ return FALSE; }
	virtual UBOOL HasSeparateTranslucency() const					{ return FALSE; }
	virtual UBOOL IsMasked() const									{ return FALSE; }
	virtual enum EBlendMode GetBlendMode() const					{ return BLEND_Opaque; }
	virtual enum EMaterialLightingModel GetLightingModel() const	{ return MLM_Unlit; }
	virtual FLOAT GetOpacityMaskClipValue() const					{ return 0.5f; }
	virtual FString GetFriendlyName() const { return FString::Printf(TEXT("FLightmassMaterialRenderer %s"), MaterialInterface ? *MaterialInterface->GetName() : TEXT("NULL")); }
	/**
	 * Should shaders compiled for this material be saved to disk?
	 */
	virtual UBOOL IsPersistent() const { return FALSE; }

	const UMaterialInterface* GetMaterialInterface() const
	{
		return MaterialInterface;
	}

	friend FArchive& operator<< ( FArchive& Ar, FLightmassMaterialProxy& V )
	{
		return Ar << V.MaterialInterface;
	}

	/**
	 *	Checks if the configuration of the material proxy will generate a uniform
	 *	value across the sampling... (Ie, nothing is hooked to the property)
	 *
	 *	@param	OutUniformValue		The value that will be returned.
	 *
	 *	@return	UBOOL				TRUE if a single value would be generated.
	 *								FALSE if not.
	 */
	UBOOL WillGenerateUniformData(FFloat16Color& OutUniformValue)
	{
		// Pre-fill the value...
		OutUniformValue.R = 0.0f;
		OutUniformValue.G = 0.0f;
		OutUniformValue.B = 0.0f;
		OutUniformValue.A = 0.0f;

		UMaterial* Material = MaterialInterface->GetMaterial();
		check(Material);
		UBOOL bExpressionIsNULL = FALSE;
		switch (PropertyToCompile)
		{
		case MP_EmissiveColor:
			// Emissive is ALWAYS returned...
			bExpressionIsNULL = (Material->EmissiveColor.Expression == NULL);
			break;
		case MP_DiffuseColor:
			// Only return for Opaque and Masked...
			if (Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
			{
				if (Material->LightingModel != MLM_Custom)
				{
					bExpressionIsNULL = (Material->DiffuseColor.Expression == NULL);
				}
				else
				{
					bExpressionIsNULL = (Material->CustomSkylightDiffuse.Expression == NULL);
				}
			}
			break;
		case MP_SpecularColor: 
			// Only return for Opaque and Masked...
			if (Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
			{
				if (Material->LightingModel != MLM_Custom)
				{
					bExpressionIsNULL = (Material->SpecularColor.Expression == NULL);
					OutUniformValue.A = 15.0f;
				}
				else
				{
					bExpressionIsNULL = TRUE;
					OutUniformValue.A = 15.0f;
				}
			}
			break;
		case MP_Normal:
			// Only return for Opaque and Masked...
			if (Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
			{
				bExpressionIsNULL = (Material->Normal.Expression == NULL);
				OutUniformValue.B = 1.0f;	// Default normal is (0,0,1)
			}
			break;
		case MP_Opacity:
			if (Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
			{
				bExpressionIsNULL = (Material->OpacityMask.Expression == NULL) && !Material->bUsedWithLandscape;
				OutUniformValue.R = 1.0f;
				OutUniformValue.G = 1.0f;
				OutUniformValue.B = 1.0f;
				OutUniformValue.A = 1.0f;
			}
			else
			if ((Material->BlendMode == BLEND_Modulate) ||
				(Material->BlendMode == BLEND_ModulateAndAdd) ||
				(Material->BlendMode == BLEND_Translucent) || 
				(Material->BlendMode == BLEND_AlphaComposite) || 
				(Material->BlendMode == BLEND_Additive) ||
				(Material->BlendMode == BLEND_DitheredTranslucent))
			{
				UBOOL bColorInputIsNULL = FALSE;
				if (Material->LightingModel == MLM_Unlit)
				{
					bColorInputIsNULL = Material->EmissiveColor.Expression == NULL;
				}
				else
				{
					if (Material->LightingModel != MLM_Custom)
					{
						bColorInputIsNULL = Material->DiffuseColor.Expression == NULL;
					}
					else
					{
						bColorInputIsNULL = (Material->CustomSkylightDiffuse.Expression == NULL);
					}
				}
				if (Material->BlendMode == BLEND_Translucent
					|| Material->BlendMode == BLEND_AlphaComposite
					|| Material->BlendMode == BLEND_Additive
					|| Material->BlendMode == BLEND_DitheredTranslucent)
				{
					bExpressionIsNULL = bColorInputIsNULL && Material->Opacity.Expression == NULL;
				}
				else
				{
					bExpressionIsNULL = bColorInputIsNULL;
				}
			}
			break;
		}

		return bExpressionIsNULL;
	}

	/**
	 *	Retrieves the desired render target format and size for the given property.
	 *	This will allow for overriding the format and/or size based on the material and property of interest.
	 *
	 *	@param	InMaterialProperty	The material property that is going to be captured in the render target.
	 *	@param	OutFormat			The format the render target should use.
	 *	@param	OutSizeX			The width to use for the render target.
	 *	@param	OutSizeY			The height to use for the render target.
	 *
	 *	@return	UBOOL				TRUE if data is good, FALSE if not (do not create render target)
	 */
	UBOOL GetRenderTargetFormatAndSize(EMaterialProperty InMaterialProperty, EPixelFormat& OutFormat, FLOAT SizeScale, INT& OutSizeX, INT& OutSizeY)
	{
		OutFormat = PF_FloatRGB;

		INT GlobalSize = 0;
		// For now, just look them up in the config file...
		if (InMaterialProperty == MP_DiffuseColor)
		{
			verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("DiffuseSampleSize"), GlobalSize, GLightmassIni));
		}
		else
		if (InMaterialProperty == MP_SpecularColor)
		{
			verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("SpecularSampleSize"), GlobalSize, GLightmassIni));
		}
		else
		if (InMaterialProperty == MP_EmissiveColor)
		{
			verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("EmissiveSampleSize"), GlobalSize, GLightmassIni));
		}
		else
		if (InMaterialProperty == MP_Normal)
		{
			verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("NormalSampleSize"), GlobalSize, GLightmassIni));
		}
		else
		if (InMaterialProperty == MP_Opacity)
		{
			verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("TransmissionSampleSize"), GlobalSize, GLightmassIni));
		}
		else
		{
			OutSizeX = 0;
			OutSizeY = 0;
			return FALSE;
		}
		OutSizeX = OutSizeY = appTrunc(GlobalSize * SizeScale);
		return TRUE;
	}

	static UBOOL WillFillData(EBlendMode InBlendMode, EMaterialProperty InMaterialProperty)
	{
		// MAKE SURE THIS MATCHES THE CHART IN CompileProperty
		// 						  RETURNED VALUES (F16 'textures')
		// 	BLEND MODE  | DIFFUSE     | SPECULAR     | EMISSIVE    | NORMAL    | TRANSMISSIVE              |
		// 	------------+-------------+--------------+-------------+-----------+---------------------------|
		// 	Opaque      | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | 0 (EMPTY)                 |
		// 	Masked      | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | Opacity Mask              |
		// 	Translucent | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | (Emsv | Diffuse)*Opacity  |
		// 	Additive    | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | (Emsv | Diffuse)*Opacity  |
		// 	Modulative  | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | Emsv | Diffuse            |
		// 	SoftMasked  | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | Opacity Mask              |
		// 	------------+-------------+--------------+-------------+-----------+---------------------------|

		if (InMaterialProperty == MP_EmissiveColor)
		{
			return TRUE;
		}

		switch (InBlendMode)
		{
		case BLEND_Opaque:
			{
				switch (InMaterialProperty)
				{
				case MP_DiffuseColor:	return TRUE;
				case MP_SpecularColor:	return TRUE;
				case MP_Normal:			return TRUE;
				case MP_Opacity:		return FALSE;
				}
			}
			break;
		case BLEND_Masked:
		case BLEND_SoftMasked:
			{
				switch (InMaterialProperty)
				{
				case MP_DiffuseColor:	return TRUE;
				case MP_SpecularColor:	return TRUE;
				case MP_Normal:			return TRUE;
				case MP_Opacity:		return TRUE;
				}
			}
			break;
		case BLEND_AlphaComposite:
		case BLEND_Translucent:
		case BLEND_DitheredTranslucent:
			{
				switch (InMaterialProperty)
				{
				case MP_DiffuseColor:	return FALSE;
				case MP_SpecularColor:	return FALSE;
				case MP_Normal:			return FALSE;
				case MP_Opacity:		return TRUE;
				}
			}
			break;
		case BLEND_Additive:
			{
				switch (InMaterialProperty)
				{
				case MP_DiffuseColor:	return FALSE;
				case MP_SpecularColor:	return FALSE;
				case MP_Normal:			return FALSE;
				case MP_Opacity:		return TRUE;
				}
			}
			break;
		case BLEND_Modulate:
		case BLEND_ModulateAndAdd:
			{
				switch (InMaterialProperty)
				{
				case MP_DiffuseColor:	return FALSE;
				case MP_SpecularColor:	return FALSE;
				case MP_Normal:			return FALSE;
				case MP_Opacity:		return TRUE;
				}
			}
			break;
		}
		return FALSE;
	}

private:
	/** The material interface for this proxy */
	UMaterialInterface* MaterialInterface;
	/** The property to compile for rendering the sample */
	EMaterialProperty PropertyToCompile;
};

/**
 * Class for rendering Lightmass captures of terrain material.
 */
class FLightmassTerrainMaterialResourceProxy : public FMaterial, public FMaterialRenderProxy
{
public:
	FLightmassTerrainMaterialResourceProxy()
	{}

	FLightmassTerrainMaterialResourceProxy(FTerrainMaterialResource* InResource, EMaterialProperty InPropertyToCompile) :
		  Resource(InResource)
		, PropertyToCompile(InPropertyToCompile)
	{
		// always use terrain level quality for lightmass
		CacheShaders(GRHIShaderPlatform, MSQ_TERRAIN);
	}

	/**
	 * Should the shader for this material with the given platform, shader type and vertex 
	 * factory type combination be compiled
	 *
	 * @param Platform		The platform currently being compiled for
	 * @param ShaderType	Which shader is being compiled
	 * @param VertexFactory	Which vertex factory is being compiled (can be NULL)
	 *
	 * @return TRUE if the shader should be compiled
	 */
	virtual UBOOL ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
	{
		if (VertexFactoryType == FindVertexFactoryType(FName(TEXT("FLocalVertexFactory"), FNAME_Find)))
		{
			// we only need the non-light-mapped, base pass, local vertex factory shaders for drawing an opaque Material Tile
			// @todo: Added a FindShaderType by fname or something"

			if(appStristr(ShaderType->GetName(), TEXT("BasePassVertexShaderFNoLightMapPolicyFNoDensityPolicy")))
			{
				return TRUE;
			}
			else if(appStristr(ShaderType->GetName(), TEXT("BasePassPixelShaderFNoLightMapPolicy")))
			{
				return TRUE;
			}
		}

		return FALSE;
	}

	////////////////
	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterial() const
	{
		if(GetShaderMap())
		{
			return this;
		}
		else
		{
			return GEngine->DefaultMaterial->GetRenderProxy(FALSE)->GetMaterial();
		}
	}

	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		return Resource->GetVectorValue(ParameterName, OutValue, Context);
	}

	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
	{
		return Resource->GetScalarValue(ParameterName, OutValue, Context);
	}

	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		return Resource->GetTextureValue(ParameterName,OutValue,Context);
	}

	/**
	 *	CompileTerrainMaterial
	 *	Compiles a single terrain material.
	 *
	 *	@param	Property			The EMaterialProperty that the material is being compiled for
	 *	@param	Compiler			The FMaterialCompiler* used to compile the material
	 *	@param	TerrainMaterial		The UTerrainMaterial* to the terrain material that is being compiled
	 *	@param	Terrain				The terrain the material is being compiled for
	 *
	 *	@return	INT					The resulting code index for the compiled material
	 */
	INT CompileTerrainMaterial(EMaterialProperty Property,FMaterialCompiler* Compiler,UTerrainMaterial* TerrainMaterial,ATerrain* Terrain) const
	{
		// FTerrainMaterialCompiler - A proxy compiler that overrides the texture coordinates used by the layer's material with the layer's texture coordinates.
		struct FLightmassTerrainMaterialCompiler : public FLightmassMaterialCompiler
		{
			UTerrainMaterial* TerrainMaterial;
			ATerrain* Terrain;

			FLightmassTerrainMaterialCompiler(FMaterialCompiler* InCompiler,UTerrainMaterial* InTerrainMaterial,ATerrain* InTerrain):
				FLightmassMaterialCompiler(InCompiler, FALSE),
				TerrainMaterial(InTerrainMaterial),
				Terrain(InTerrain)
			{}

			// Override texture coordinates used by the material with the texture coordinate specified by the terrain material.

			virtual INT TextureCoordinate(UINT CoordinateIndex, UBOOL UnMirrorU, UBOOL UnMirrorV)
			{
				// Scale it so it covers the whole terrain...
				INT BaseUV = Compiler->TextureCoordinate(0, UnMirrorU, UnMirrorV);
				FLOAT Scale = TerrainMaterial->MappingScale == 0.0f ? 1.0f : TerrainMaterial->MappingScale;
				FLOAT ScaleX = Terrain->NumPatchesX / Scale;
				FLOAT ScaleY = Terrain->NumPatchesY / Scale;

				return Compiler->Add(
					Compiler->AppendVector(
					Compiler->Dot(BaseUV,Compiler->Constant2(+appCos(TerrainMaterial->MappingRotation * PI / 180.0) * ScaleX,+appSin(TerrainMaterial->MappingRotation * PI / 180.0) * ScaleY)),
					Compiler->Dot(BaseUV,Compiler->Constant2(-appSin(TerrainMaterial->MappingRotation * PI / 180.0) * ScaleX,+appCos(TerrainMaterial->MappingRotation * PI / 180.0) * ScaleY))
					),
					Compiler->Constant2(TerrainMaterial->MappingPanU,TerrainMaterial->MappingPanV)
					);
			}
		};

		if( Property == MP_EmissiveColor )
		{
			UMaterialInterface*					MaterialInterface = TerrainMaterial ? (TerrainMaterial->Material ? TerrainMaterial->Material : GEngine->DefaultMaterial)  : GEngine->DefaultMaterial;
			UMaterial*							Material = MaterialInterface->GetMaterial();

			// If the property is not active, don't compile it
			if (!IsActiveMaterialProperty(Material, PropertyToCompile))
			{
				return INDEX_NONE;
			}

			FLightmassTerrainMaterialCompiler	ProxyCompiler(Compiler,TerrainMaterial,Terrain);
			switch (PropertyToCompile)
			{
			case MP_EmissiveColor:
				// Emissive is ALWAYS returned...
				return Compiler->ForceCast(Material->EmissiveColor.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
			case MP_DiffuseColor:
				// Only return for Opaque and Masked...
				if (Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
				{
					if (Material->LightingModel != MLM_Custom)
					{
						return Compiler->ForceCast(Material->DiffuseColor.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
					}
					else
					{
						return Compiler->ForceCast(Material->CustomLighting.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
					}
				}
				break;
			case MP_SpecularColor: 
				// Only return for Opaque and Masked...
				if (Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
				{
 					return Compiler->AppendVector(
 						Compiler->ForceCast(Material->SpecularColor.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE), 
 						Compiler->ForceCast(Material->SpecularPower.Compile(&ProxyCompiler,15.0f),MCT_Float1));
				}
				break;
			case MP_Normal:
				// Only return for Opaque and Masked...
				if (Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
				{
					return Compiler->ForceCast(Material->Normal.Compile(&ProxyCompiler,FVector(0,0,1)),MCT_Float3,TRUE,TRUE);
				}
				break;
			case MP_Opacity:
				if (Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
				{
					return Material->OpacityMask.Compile(&ProxyCompiler, 1.0f);
				}
				else if ((Material->BlendMode == BLEND_Modulate) || (Material->BlendMode == BLEND_ModulateAndAdd))
				{
					if (Material->LightingModel == MLM_Unlit)
					{
						return Compiler->ForceCast(Material->EmissiveColor.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
					}
					else
					{
						if (Material->LightingModel != MLM_Custom)
						{
							return Compiler->ForceCast(Material->DiffuseColor.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
						}
						else
						{
							return Compiler->ForceCast(Material->CustomLighting.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
						}
					}
				}
				else
				if ((Material->BlendMode == BLEND_Translucent) || (Material->BlendMode == BLEND_AlphaComposite) || (Material->BlendMode == BLEND_Additive) || (Material->BlendMode == BLEND_DitheredTranslucent))
				{
					INT ColoredOpacity = -1;
					if (Material->LightingModel == MLM_Unlit)
					{
						ColoredOpacity = Compiler->ForceCast(Material->EmissiveColor.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
					}
					else
					{
						if (Material->LightingModel != MLM_Custom)
						{
							ColoredOpacity = Compiler->ForceCast(Material->DiffuseColor.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
						}
						else
						{
							ColoredOpacity = Compiler->ForceCast(Material->CustomLighting.Compile(&ProxyCompiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
						}
					}
					return Compiler->Lerp(Compiler->Constant3(1.0f, 1.0f, 1.0f), ColoredOpacity, Compiler->ForceCast(Material->Opacity.Compile(&ProxyCompiler,1.0f),MCT_Float1));
				}
				break;
			default:
				return Compiler->Constant(1.0f);
			}
	
			return Compiler->Constant(0.0f);
		}
		else
		{
			return Compiler->Constant(1.0f);
		}
	}

	// Material properties.
	/** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
	virtual INT CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler) const
	{
		const EShaderFrequency ShaderFrequency = GetMaterialPropertyShaderFrequency(Property);
		Compiler->SetMaterialProperty(Property);
		// Terrain does not support vertex shader inputs,
		// Since there's no way to blend between layers in the vertex shader based on the layer weight texture.
		if (ShaderFrequency == SF_Vertex)
		{
			if (Property == MP_WorldPositionOffset)
			{
				return Compiler->Constant3(0,0,0);
			}
			else
			{
				appErrorf(TEXT("Unhandled terrain vertex shader material input!"));
			}
		}
		else if(ShaderFrequency == SF_Domain)
		{
			if (Property == MP_WorldDisplacement)
			{
				return Compiler->Constant3(0,0,0);
			}
			else
			{
				appErrorf(TEXT("Unhandled terrain domain shader material input!"));
			}
		}
		else if(ShaderFrequency == SF_Hull)
		{
			if (Property == MP_TessellationMultiplier)
			{
				return Compiler->Constant(1.0f);
			}
			else
			{
				appErrorf(TEXT("Unhandled terrain hull shader material input!"));
			}
		}

		

		// MAKE SURE THIS MATCHES THE CHART IN WillFillData
		// 						  RETURNED VALUES (F16 'textures')
		// 	BLEND MODE  | DIFFUSE     | SPECULAR     | EMISSIVE    | NORMAL    | TRANSMISSIVE              |
		// 	------------+-------------+--------------+-------------+-----------+---------------------------|
		// 	Opaque      | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | 0 (EMPTY)                 |
		// 	Masked      | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | Opacity Mask              |
		// 	Translucent | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | (Emsv | Diffuse)*Opacity  |
		// 	Additive    | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | (Emsv | Diffuse)*Opacity  |
		// 	Modulative  | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | Emsv | Diffuse            |
		// 	------------+-------------+--------------+-------------+-----------+---------------------------|
		INT	NumMaterials = 0;
		const FTerrainMaterialMask& ResourceMask = Resource->GetMask();
		for (INT MaterialIndex = 0; MaterialIndex < ResourceMask.Num(); MaterialIndex++)
		{
			if (ResourceMask.Get(MaterialIndex))
			{
				NumMaterials++;
			}
		}

		ATerrain* Terrain = Resource->GetTerrain();
		if (NumMaterials == 1)
		{
			for (INT MaterialIndex = 0; MaterialIndex < ResourceMask.Num(); MaterialIndex++)
			{
				if (ResourceMask.Get(MaterialIndex))
				{
					if (MaterialIndex < Terrain->WeightedMaterials.Num())
					{
						FTerrainWeightedMaterial* WeightedMaterial = &(Terrain->WeightedMaterials(MaterialIndex));
						return CompileTerrainMaterial(
							Property,Compiler,WeightedMaterial->Material,
							Terrain);
					}
				}
			}
	#if defined(_TERRAIN_CATCH_MISSING_MATERIALS_)
			appErrorf(TEXT("Single material has disappeared!"));
	#endif	//#if defined(_TERRAIN_CATCH_MISSING_MATERIALS_)
			return INDEX_NONE;
		}
		else if(NumMaterials > 1)
		{
			INT MaterialIndex;
			FTerrainWeightedMaterial* WeightedMaterial;

			INT	Result = INDEX_NONE;
			INT TextureCount = 0;
			if (GEngine->TerrainMaterialMaxTextureCount > 0)
			{
				// Do a quick preliminary check to ensure we don't use too many textures.
				TArray<UTexture*> CheckTextures;
				INT WeightMapCount = 0;
				for (MaterialIndex = 0; MaterialIndex < ResourceMask.Num(); MaterialIndex++)
				{
					if (ResourceMask.Get(MaterialIndex))
					{
						if (MaterialIndex < Terrain->WeightedMaterials.Num())
						{
							WeightMapCount = Max<INT>(WeightMapCount, (MaterialIndex / 4) + 1);
							WeightedMaterial = &(Terrain->WeightedMaterials(MaterialIndex));
							if (WeightedMaterial->Material && WeightedMaterial->Material->Material)
							{
								WeightedMaterial->Material->Material->GetUsedTextures(CheckTextures);
							}
						}
					}
				}

				TextureCount = CheckTextures.Num() + WeightMapCount;
			}
			if (TextureCount >= GEngine->TerrainMaterialMaxTextureCount)
			{
				// With a shadow map (or light maps) this will fail!
				return Compiler->Error(TEXT("TerrainMat_TooManyTextures"));
			}
			else
			{
				if ((Property == MP_Normal) && (Terrain->NormalMapLayer != -1))
				{
					if (Terrain->NormalMapLayer < Terrain->Layers.Num())
					{
						// Grab the layer indexed by the NormalMapLayer
						FTerrainLayer& Layer = Terrain->Layers(Terrain->NormalMapLayer);
						UTerrainLayerSetup* LayerSetup = Layer.Setup;
						if (LayerSetup)
						{
							if (LayerSetup->Materials.Num() > 0)
							{
								//@todo. Allow selection of 'sub' materials in layers? (Procedural has multiple mats...)
								FTerrainFilteredMaterial& TFilteredMat = LayerSetup->Materials(0);
								UTerrainMaterial* TMat = TFilteredMat.Material;
								for (INT WeightedIndex = 0; WeightedIndex < Terrain->WeightedMaterials.Num(); WeightedIndex++)
								{
									FTerrainWeightedMaterial* WeightedMaterial = &(Terrain->WeightedMaterials(WeightedIndex));
									if (WeightedMaterial->Material == TMat)
									{
										return CompileTerrainMaterial(
											Property,Compiler,WeightedMaterial->Material,
											Terrain);
									}
								}
							}
						}
					}

					// Have all failure cases compile using the standard compilation path.
				}

				FString RootName;
				FName WeightTextureName;

				INT MaskArg;
				INT MulArgA;
				INT	MulArgB;
				INT	IndividualResult;
				INT TextureCodeIndex;
				INT TextureCoordArg = Compiler->TextureCoordinate(0, FALSE, FALSE);

				for (MaterialIndex = 0; MaterialIndex < ResourceMask.Num(); MaterialIndex++)
				{
					if (ResourceMask.Get(MaterialIndex))
					{
						if (MaterialIndex < Terrain->WeightedMaterials.Num())
						{
							RootName = FString::Printf(TEXT("TWeightMap%d"), MaterialIndex / 4);
							WeightTextureName = FName(*RootName);

							WeightedMaterial = &(Terrain->WeightedMaterials(MaterialIndex));

							TextureCodeIndex = Compiler->TextureParameter(WeightTextureName, GEngine->WeightMapPlaceholderTexture);
							MaskArg = Compiler->TextureSample(
											TextureCodeIndex, 
											TextureCoordArg								
											);
							UBOOL bRed = FALSE;
							UBOOL bGreen = FALSE;
							UBOOL bBlue = FALSE;
							UBOOL bAlpha = FALSE;
							INT DataIndex = MaterialIndex % 4;
							switch (DataIndex)
							{
							case 0:		bBlue = TRUE;		break;
							case 1:		bGreen = TRUE;		break;
							case 2:		bRed = TRUE;		break;
							case 3:		bAlpha = TRUE;		break;
							}
							MulArgA = Compiler->ComponentMask(
										MaskArg, 
										bRed, bGreen, bBlue, bAlpha
										);
							MulArgB = CompileTerrainMaterial(
										Property,Compiler,
										WeightedMaterial->Material,
										Terrain
										);

							IndividualResult = Compiler->Mul(MulArgA, MulArgB);

							//@todo.SAS. Implement multipass rendering option here?
							if (Result == INDEX_NONE)
							{
								Result = IndividualResult;
							}
							else
							{
								Result = Compiler->Add(Result,IndividualResult);
							}
						}
					}
				}
			}
			return Result;
		}
		else
		{
			return GEngine->DefaultMaterial->GetRenderProxy(0)->GetMaterial()->CompileProperty(Property,Compiler);
		}
	}

	virtual FString GetMaterialUsageDescription() const
	{
		return FString::Printf(TEXT("FLightmassTerrainMaterialResourceProxy %s"), Resource ? *Resource->GetFriendlyName() : TEXT("NULL"));
	}
	virtual UBOOL IsTwoSided() const 
	{ 
		return (Resource ? Resource->IsTwoSided() : FALSE);
	}
	virtual UBOOL RenderTwoSidedSeparatePass() const
	{ 
		return FALSE;
	}
	virtual UBOOL RenderLitTranslucencyPrepass() const
	{ 
		return FALSE;
	}
	virtual UBOOL RenderLitTranslucencyDepthPostpass() const
	{ 
		return FALSE;
	}
	virtual UBOOL CastLitTranslucencyShadowAsMasked() const
	{ 
		return FALSE;
	}
	virtual UBOOL NeedsDepthTestDisabled() const
	{
		return FALSE;
	}
	virtual UBOOL IsLightFunction() const
	{
		return (Resource ? Resource->IsLightFunction() : FALSE);
	}
	virtual UBOOL IsUsedWithFogVolumes() const
	{
		return (Resource ? Resource->IsUsedWithFogVolumes() : FALSE);
	}
	virtual UBOOL IsSpecialEngineMaterial() const
	{
		return (Resource ? Resource->IsSpecialEngineMaterial() : FALSE);
	}
	virtual UBOOL IsUsedWithMobileLandscape() const
	{
		return FALSE;
	}
	virtual UBOOL IsTerrainMaterial() const
	{
// 		return TRUE;
		return FALSE;
	}
	virtual UBOOL IsDecalMaterial() const
	{
		return (Resource ? Resource->IsDecalMaterial() : FALSE);
	}
	virtual UBOOL IsWireframe() const
	{
		return (Resource ? Resource->IsWireframe() : FALSE);
	}
	virtual UBOOL IsDistorted() const								{ return FALSE; }
	virtual UBOOL HasSubsurfaceScattering() const					{ return FALSE; }
	virtual UBOOL HasSeparateTranslucency() const					{ return FALSE; }
	virtual UBOOL IsMasked() const									{ return FALSE; }
	virtual enum EBlendMode GetBlendMode() const					{ return BLEND_Opaque; }
	virtual enum EMaterialLightingModel GetLightingModel() const	{ return MLM_Unlit; }
	virtual FLOAT GetOpacityMaskClipValue() const					{ return 0.5f; }
	virtual FString GetFriendlyName() const
	{
		return FString::Printf(TEXT("FLightmassTerrainMaterialResourceProxy %s"), Resource ? *Resource->GetFriendlyName() : TEXT("NULL"));
	}
	/**
	 * Should shaders compiled for this material be saved to disk?
	 */
	virtual UBOOL IsPersistent() const { return FALSE; }

	const FTerrainMaterialResource* GetResource() const
	{
		return Resource;
	}

	friend FArchive& operator<< ( FArchive& Ar, FLightmassTerrainMaterialResourceProxy& V )
	{
		return Ar << *(V.Resource);
	}

	/**
	 *	Checks if the configuration of the material proxy will generate a uniform
	 *	value across the sampling... (Ie, nothing is hooked to the property)
	 *
	 *	@param	OutUniformValue		The value that will be returned.
	 *
	 *	@return	UBOOL				TRUE if a single value would be generated.
	 *								FALSE if not.
	 */
	UBOOL WillGenerateUniformData(FFloat16Color& OutUniformValue)
	{
		// Pre-fill the value...
		OutUniformValue.R = 0.0f;
		OutUniformValue.G = 0.0f;
		OutUniformValue.B = 0.0f;
		OutUniformValue.A = 0.0f;

		return FALSE;
	}

	/**
	 *	Retrieves the desired render target format and size for the given property.
	 *	This will allow for overriding the format and/or size based on the material and property of interest.
	 *
	 *	@param	InMaterialProperty	The material property that is going to be captured in the render target.
	 *	@param	OutFormat			The format the render target should use.
	 *	@param	OutSizeX			The width to use for the render target.
	 *	@param	OutSizeY			The height to use for the render target.
	 *
	 *	@return	UBOOL				TRUE if data is good, FALSE if not (do not create render target)
	 */
	UBOOL GetRenderTargetFormatAndSize(EMaterialProperty InMaterialProperty, EPixelFormat& OutFormat, FLOAT SizeScale, INT& OutSizeX, INT& OutSizeY)
	{
		OutFormat = PF_FloatRGB;

		INT GlobalSize = 0;
		// For now, just look them up in the config file...
		if (InMaterialProperty == MP_DiffuseColor)
		{
			verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("DiffuseSampleSize"), GlobalSize, GLightmassIni));
		}
		else
		if (InMaterialProperty == MP_SpecularColor)
		{
			verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("SpecularSampleSize"), GlobalSize, GLightmassIni));
		}
		else
		if (InMaterialProperty == MP_EmissiveColor)
		{
			verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("EmissiveSampleSize"), GlobalSize, GLightmassIni));
		}
		else
		if (InMaterialProperty == MP_Normal)
		{
			verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("NormalSampleSize"), GlobalSize, GLightmassIni));
		}
		else
		if (InMaterialProperty == MP_Opacity)
		{
			verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("TransmissionSampleSize"), GlobalSize, GLightmassIni));
		}
		else
		{
			OutSizeX = 0;
			OutSizeY = 0;
			return FALSE;
		}

		INT GlobalScalar = 1;
		verify(GConfig->GetInt(TEXT("DevOptions.StaticLightingMaterial"), TEXT("TerrainSampleScalar"), GlobalScalar, GLightmassIni));

		OutSizeX = OutSizeY = appTrunc((GlobalSize * GlobalScalar) * SizeScale);
		return TRUE;
	}

private:
	/** The terrain material resource for this proxy */
	FTerrainMaterialResource* Resource;
	/** The property to compile for rendering the sample */
	EMaterialProperty PropertyToCompile;
};

//
// FLightmassMaterialRenderer
//
FLightmassMaterialRenderer::~FLightmassMaterialRenderer()
{
	if (RenderTarget)
	{
		RenderTarget->RemoveFromRoot();
	}
	RenderTarget = NULL;
	delete Canvas;
	Canvas = NULL;
}

/**
 *	Generate the required material data for the given material.
 *
 *	@param	Material				The material of interest.
 *	@param	bInWantNormals			True if normals should be generated as well
 *	@param	MaterialEmissive		The emissive samples for the material.
 *	@param	MaterialDiffuse			The diffuse samples for the material.
 *	@param	MaterialSpecular		The specular samples for the material.
 *	@param	MaterialSpecularPower	The specular power samples for the material.
 *	@param	MaterialTransmission	The transmission samples for the material.
 *
 *	@return	UBOOL					TRUE if successful, FALSE if not.
 */
UBOOL FLightmassMaterialRenderer::GenerateMaterialData(UMaterialInterface& InMaterial, UBOOL bInWantNormals, Lightmass::FMaterialData& OutMaterialData, 
	TArray<FFloat16Color>& OutMaterialEmissive, TArray<FFloat16Color>& OutMaterialDiffuse, 
	TArray<FFloat16Color>& OutMaterialSpecular, TArray<FFloat16Color>& OutMaterialTransmission,
	TArray<FFloat16Color>& OutMaterialNormal)
{
	UBOOL bResult = TRUE;

	UMaterial* Material = InMaterial.GetMaterial();
	if (Material == NULL)
	{
		return FALSE;
	}

 	if ((Material->LightingModel != MLM_Phong) &&
		(Material->LightingModel != MLM_NonDirectional) &&
		(Material->LightingModel != MLM_Custom) &&
		(Material->LightingModel != MLM_Unlit))
	{
		warnf(NAME_Warning, TEXT("LIGHTMASS: Material has an unsupported lighting model: %d on %s"), 
			(INT)Material->LightingModel,
			*(InMaterial.GetPathName()));
	}

	UBOOL bLandscapeVisibility = FALSE;
	BYTE OldBlendMode = Material->BlendMode;
	UMaterialInstance* MatInst = Cast<UMaterialInstance>(&InMaterial);
	if (MatInst && Material->bUsedWithLandscape)
	{
		ULandscapeMaterialInstanceConstant* LandscapeMat = Cast<ULandscapeMaterialInstanceConstant>(MatInst);
		// Override the static switches with the MaterialInstance settings.
		if ( LandscapeMat &&  LandscapeMat->DataWeightmapIndex != INDEX_NONE && LandscapeMat->DataWeightmapSize > 0 )
		{
			bLandscapeVisibility = TRUE;
			Material->BlendMode = BLEND_Masked;
		}
	}

	// Set the blend mode
	checkAtCompileTime(EBlendMode::BLEND_MAX == Lightmass::BLEND_MAX, DebugTypeSizesMustMatch);
	OutMaterialData.BlendMode = (Lightmass::EBlendMode)(Material->BlendMode);
	// Set the two-sided flag
	OutMaterialData.bTwoSided = Material->TwoSided;
	OutMaterialData.bCastShadowAsMasked = Material->GetCastShadowAsMasked() && (IsTranslucentBlendMode((EBlendMode)Material->BlendMode) || Material->BlendMode == BLEND_DitheredTranslucent);
	OutMaterialData.OpacityMaskClipValue = Material->OpacityMaskClipValue;

	GForceDisableEmulateMobileRendering = TRUE;

	// Diffuse
	if (FLightmassMaterialProxy::WillFillData((EBlendMode)(Material->BlendMode), MP_DiffuseColor) == TRUE)
	{
		if (GenerateMaterialPropertyData(InMaterial, MP_DiffuseColor, OutMaterialData.DiffuseSize, OutMaterialData.DiffuseSize, OutMaterialDiffuse) == FALSE)
		{
			warnf(NAME_Warning, TEXT("Failed to generate diffuse material samples for %s: %s"),
				*(InMaterial.LightingGuid.String()), *(InMaterial.GetPathName()));
			bResult = FALSE;
			OutMaterialData.DiffuseSize = 0;
		}
	}

	// Specular
	if (FLightmassMaterialProxy::WillFillData((EBlendMode)(Material->BlendMode), MP_SpecularColor) == TRUE)
	{
		if (GenerateMaterialPropertyData(InMaterial, MP_SpecularColor, OutMaterialData.SpecularSize, OutMaterialData.SpecularSize, OutMaterialSpecular) == FALSE)
		{
			warnf(NAME_Warning, TEXT("Failed to generate specular material samples for %s: %s"),
				*(InMaterial.LightingGuid.String()), *(InMaterial.GetPathName()));
			bResult = FALSE;
			OutMaterialData.SpecularSize = 0;
		}
	}

	// Emissive
	if (FLightmassMaterialProxy::WillFillData((EBlendMode)(Material->BlendMode), MP_EmissiveColor) == TRUE)
	{
		if (GenerateMaterialPropertyData(InMaterial, MP_EmissiveColor, OutMaterialData.EmissiveSize, OutMaterialData.EmissiveSize, OutMaterialEmissive) == FALSE)
		{
			warnf(NAME_Warning, TEXT("Failed to generate emissive material samples for %s: %s"),
				*(InMaterial.LightingGuid.String()), *(InMaterial.GetPathName()));
			bResult = FALSE;
			OutMaterialData.EmissiveSize = 0;
		}
	}

	// Transmission
	if (FLightmassMaterialProxy::WillFillData((EBlendMode)(Material->BlendMode), MP_Opacity) == TRUE)
	{
		if (GenerateMaterialPropertyData(InMaterial, MP_Opacity, OutMaterialData.TransmissionSize, OutMaterialData.TransmissionSize, OutMaterialTransmission) == FALSE)
		{
			warnf(NAME_Warning, TEXT("Failed to generate transmissive material samples for %s: %s"),
				*(InMaterial.LightingGuid.String()), *(InMaterial.GetPathName()));
			bResult = FALSE;
			OutMaterialData.TransmissionSize = 0;
		}
	}

	// Normal
	if (FLightmassMaterialProxy::WillFillData((EBlendMode)(Material->BlendMode), MP_Normal) == TRUE)
	{
		// We'll only bother generating normal data if the user has opted to use this in the lighting
		// calculations.  This data takes up a lot of space so there's no point generating it unless it
		// will actually be used!
		if( bInWantNormals )
		{
			if (GenerateMaterialPropertyData(InMaterial, MP_Normal, OutMaterialData.NormalSize, OutMaterialData.NormalSize, OutMaterialNormal) == FALSE)
			{
				warnf(NAME_Warning, TEXT("Failed to generate normal material samples for %s: %s"),
					*(InMaterial.LightingGuid.String()), *(InMaterial.GetPathName()));
				bResult = FALSE;
				OutMaterialData.NormalSize = 0;
			}
		}
	}

	GForceDisableEmulateMobileRendering = FALSE;

	if (bLandscapeVisibility)
	{
		FlushRenderingCommands();
		Material->BlendMode = OldBlendMode;
		//Material->bIsMasked = FALSE;
		//Material->PostEditChange();
	}

	return bResult;
}

/**
 *	Generate the material data for the given material and it's given property.
 *
 *	@param	InMaterial				The material of interest.
 *	@param	InMaterialProperty		The property to generate the samples for
 *	@param	InOutSizeX				The desired X size of the sample to capture (in), the resulting size (out)
 *	@param	InOutSizeY				The desired Y size of the sample to capture (in), the resulting size (out)
 *	@param	OutMaterialSamples		The samples for the material.
 *
 *	@return	UBOOL					TRUE if successful, FALSE if not.
 */
UBOOL FLightmassMaterialRenderer::GenerateMaterialPropertyData(UMaterialInterface& InMaterial, 
	EMaterialProperty InMaterialProperty, INT& InOutSizeX, INT& InOutSizeY, 
	TArray<FFloat16Color>& OutMaterialSamples)
{
	FLightmassMaterialProxy* MaterialProxy = new FLightmassMaterialProxy(&InMaterial, InMaterialProperty);
	if (MaterialProxy == NULL)
	{
		warnf(NAME_Warning, TEXT("Failed to create FLightmassMaterialProxy!"));
		return FALSE;
	}

	UBOOL bResult = TRUE;

	FFloat16Color UniformValue;
	if (MaterialProxy->WillGenerateUniformData(UniformValue) == FALSE)
	{
		//@lmtodo. The format may be determined by the material property...
		// For example, if Diffuse doesn't need to be F16 it can create a standard RGBA8 target.
		EPixelFormat Format = PF_FloatRGB;
		if (MaterialProxy->GetRenderTargetFormatAndSize(InMaterialProperty, Format, InMaterial.GetExportResolutionScale(), InOutSizeX, InOutSizeY))
		{
			if (CreateRenderTarget(Format, InOutSizeX, InOutSizeY) == FALSE)
			{
				warnf(NAME_Warning, TEXT("Failed to create %4dx%4d render target!"), InOutSizeX, InOutSizeY);
				bResult = FALSE;
			}
			else
			{
				// At this point, we can't just return false at failure since we have some clean-up to do...
				Canvas->SetRenderTarget(RenderTarget->GetRenderTargetResource());
				RHIBeginScene();
				// Clear the render target to black
				Clear(Canvas, FLinearColor(0,0,0,0));
				// Freeze time while capturing the material's inputs
				::DrawTile(Canvas,0,0,InOutSizeX,InOutSizeY,0,0,1,1,MaterialProxy,TRUE);
				Canvas->Flush();
				FlushRenderingCommands();
				Canvas->SetRenderTarget(NULL);
				FlushRenderingCommands();
				RHIEndScene();

				if (GLightmassDebugOptions.bDebugMaterials == TRUE)
				{
					TArray<FColor> OutputBuffer;
					if (RenderTarget->GetRenderTargetResource()->ReadPixels(OutputBuffer))
					{
						// Create screenshot folder if not already present.
						// Save the contents of the array to a bitmap file.
						FString TempPath = appScreenShotDir();
						TempPath += TEXT("/Materials");
						GFileManager->MakeDirectory(*TempPath, TRUE);
						FString TempName = InMaterial.GetPathName();
						TempName = TempName.Replace(TEXT("."), TEXT("_"));
						TempName = TempName.Replace(TEXT(":"), TEXT("_"));
						FString OutputName = TempPath * TempName;
						OutputName += TEXT("_");
						switch (InMaterialProperty)
						{
						case MP_DiffuseColor:	OutputName += TEXT("Diffuse");			break;
						case MP_EmissiveColor:	OutputName += TEXT("Emissive");			break;
						case MP_SpecularColor:	OutputName += TEXT("Specular");			break;
						case MP_Normal:			OutputName += TEXT("Normal");			break;
						case MP_Opacity:		OutputName += TEXT("Transmissive");		break;
						}
						OutputName += TEXT(".BMP");
						appCreateBitmap(*OutputName,InOutSizeX,InOutSizeY,&OutputBuffer(0),GFileManager);
					}
				}

				// Read in the data
				//@lmtodo. Check the format! RenderTarget->Format
				// If we are going to allow non-F16 formats, then the storage will have to be aware of it!
				if (RenderTarget->GetRenderTargetResource()->ReadFloat16Pixels(OutMaterialSamples) == FALSE)
				{
					warnf(NAME_Warning, TEXT("Failed to read Float16Pixels for 0x%08x property of %s: %s"), 
						(DWORD)InMaterialProperty, *(InMaterial.LightingGuid.String()), *(InMaterial.GetPathName()));
					bResult = FALSE;
				}
			}
		}
		else
		{
			warnf(NAME_Warning, TEXT("Failed to get render target format and size for 0x%08x property of %s: %s"), 
				(DWORD)InMaterialProperty, *(InMaterial.LightingGuid.String()), *(InMaterial.GetPathName()));
			bResult = FALSE;
		}
	}
	else
	{
		// Single value... fill it in.
		InOutSizeX = 1;
		InOutSizeY = 1; 
		OutMaterialSamples.Empty(1);
		OutMaterialSamples.AddZeroed(1);
		OutMaterialSamples(0) = UniformValue;
	}

	// Clean up
	delete MaterialProxy;

	return bResult;
}

/**
 *	Create the required render target.
 *
 *	@param	InFormat	The format of the render target
 *	@param	InSizeX		The X resolution of the render target
 *	@param	InSizeY		The Y resolution of the render target
 *
 *	@return	UBOOL		TRUE if it was successful, FALSE if not
 */
UBOOL FLightmassMaterialRenderer::CreateRenderTarget(EPixelFormat InFormat, INT InSizeX, INT InSizeY)
{
	if (RenderTarget && 
		((RenderTarget->Format != InFormat) || (RenderTarget->SizeX != InSizeX) || (RenderTarget->SizeY != InSizeY))
		)
	{
		RenderTarget->RemoveFromRoot();
		RenderTarget = NULL;
		delete Canvas;
		Canvas = NULL;
	}

	if (RenderTarget == NULL)
	{
		RenderTarget = new UTextureRenderTarget2D();
		check(RenderTarget);
		RenderTarget->AddToRoot();
		RenderTarget->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		RenderTarget->Init(InSizeX, InSizeY, InFormat);

		Canvas = new FCanvas(RenderTarget->GetRenderTargetResource(), NULL);
		check(Canvas);
	}

	return (RenderTarget != NULL);
}

/**
 * Class for rendering sample 'textures' of terrain material properties.
 */
FLightmassTerrainMaterialRenderer::~FLightmassTerrainMaterialRenderer()
{
}

/**
 *	Generate the required material data for the given material.
 *
 *	@param	Material				The material of interest.
 *	@param	bInWantNormals			True if normal data should be generated as well
 *	@param	MaterialEmissive		The emissive samples for the material.
 *	@param	MaterialDiffuse			The diffuse samples for the material.
 *	@param	MaterialSpecular		The specular samples for the material.
 *	@param	MaterialSpecularPower	The specular power samples for the material.
 *	@param	MaterialTransmission	The transmission samples for the material.
 *
 *	@return	UBOOL					TRUE if successful, FALSE if not.
 */
UBOOL FLightmassTerrainMaterialRenderer::GenerateTerrainMaterialData(FTerrainMaterialResource& InMaterial,
	UBOOL bInWantNormals, Lightmass::FMaterialData& OutMaterialData, TArray<FFloat16Color>& OutMaterialEmissive, 
	TArray<FFloat16Color>& OutMaterialDiffuse, TArray<FFloat16Color>& OutMaterialSpecular, 
	TArray<FFloat16Color>& OutMaterialTransmission, TArray<FFloat16Color>& OutMaterialNormal)
{
	UBOOL bResult = TRUE;

	// Set the blend mode
	checkAtCompileTime(EBlendMode::BLEND_MAX == Lightmass::BLEND_MAX, DebugTypeSizesMustMatch);
	OutMaterialData.BlendMode = (Lightmass::EBlendMode)(InMaterial.GetBlendMode());
	// Set the two-sided flag
	OutMaterialData.bTwoSided = InMaterial.IsTwoSided();
	OutMaterialData.OpacityMaskClipValue = InMaterial.GetOpacityMaskClipValue();

	// Diffuse
	if (FLightmassMaterialProxy::WillFillData((EBlendMode)(InMaterial.GetBlendMode()), MP_DiffuseColor) == TRUE)
	{
		if (GenerateTerrainMaterialPropertyData(InMaterial, MP_DiffuseColor, OutMaterialData.DiffuseSize, OutMaterialData.DiffuseSize, OutMaterialDiffuse) == FALSE)
		{
			warnf(NAME_Warning, TEXT("Failed to generate diffuse material samples for %s: %s"),
				*(InMaterial.GetLightingGuid().String()), *(InMaterial.GetFriendlyName()));
			bResult = FALSE;
			OutMaterialData.DiffuseSize = 0;
		}
	}

	// Specular
	if (FLightmassMaterialProxy::WillFillData((EBlendMode)(InMaterial.GetBlendMode()), MP_SpecularColor) == TRUE)
	{
		if (GenerateTerrainMaterialPropertyData(InMaterial, MP_SpecularColor, OutMaterialData.SpecularSize, OutMaterialData.SpecularSize, OutMaterialSpecular) == FALSE)
		{
			warnf(NAME_Warning, TEXT("Failed to generate specular material samples for %s: %s"),
				*(InMaterial.GetLightingGuid().String()), *(InMaterial.GetFriendlyName()));
			bResult = FALSE;
			OutMaterialData.SpecularSize = 0;
		}
	}

	// Emissive
	if (FLightmassMaterialProxy::WillFillData((EBlendMode)(InMaterial.GetBlendMode()), MP_EmissiveColor) == TRUE)
	{
		if (GenerateTerrainMaterialPropertyData(InMaterial, MP_EmissiveColor, OutMaterialData.EmissiveSize, OutMaterialData.EmissiveSize, OutMaterialEmissive) == FALSE)
		{
			warnf(NAME_Warning, TEXT("Failed to generate emissive material samples for %s: %s"),
				*(InMaterial.GetLightingGuid().String()), *(InMaterial.GetFriendlyName()));
			bResult = FALSE;
			OutMaterialData.EmissiveSize = 0;
		}
	}

	// Transmission
	if (FLightmassMaterialProxy::WillFillData((EBlendMode)(InMaterial.GetBlendMode()), MP_Opacity) == TRUE)
	{
		if (GenerateTerrainMaterialPropertyData(InMaterial, MP_Opacity, OutMaterialData.TransmissionSize, OutMaterialData.TransmissionSize, OutMaterialTransmission) == FALSE)
		{
			warnf(NAME_Warning, TEXT("Failed to generate transmissive material samples for %s: %s"),
				*(InMaterial.GetLightingGuid().String()), *(InMaterial.GetFriendlyName()));
			bResult = FALSE;
			OutMaterialData.TransmissionSize = 0;
		}
	}

	// Normal
	if (FLightmassMaterialProxy::WillFillData((EBlendMode)(InMaterial.GetBlendMode()), MP_Normal) == TRUE)
	{
		// We'll only bother generating normal data if the user has opted to use this in the lighting
		// calculations.  This data takes up a lot of space so there's no point generating it unless it
		// will actually be used!
		if( bInWantNormals )
		{
			if (GenerateTerrainMaterialPropertyData(InMaterial, MP_Normal, OutMaterialData.NormalSize, OutMaterialData.NormalSize, OutMaterialNormal) == FALSE)
			{
				warnf(NAME_Warning, TEXT("Failed to generate normal material samples for %s: %s"),
					*(InMaterial.GetLightingGuid().String()), *(InMaterial.GetFriendlyName()));
				bResult = FALSE;
				OutMaterialData.NormalSize = 0;
			}
		}
	}

	return bResult;
}

/**
 *	Generate the material data for the given material and it's given property.
 *
 *	@param	InMaterial				The material of interest.
 *	@param	InMaterialProperty		The property to generate the samples for
 *	@param	InOutSizeX				The desired X size of the sample to capture (in), the resulting size (out)
 *	@param	InOutSizeY				The desired Y size of the sample to capture (in), the resulting size (out)
 *	@param	OutMaterialSamples		The samples for the material.
 *
 *	@return	UBOOL					TRUE if successful, FALSE if not.
 */
UBOOL FLightmassTerrainMaterialRenderer::GenerateTerrainMaterialPropertyData(FTerrainMaterialResource& InMaterial, 
	EMaterialProperty InMaterialProperty, INT& InOutSizeX, INT& InOutSizeY, TArray<FFloat16Color>& OutMaterialSamples)
{
	FLightmassTerrainMaterialResourceProxy* MaterialProxy = new FLightmassTerrainMaterialResourceProxy(&InMaterial, InMaterialProperty);
	if (MaterialProxy == NULL)
	{
		warnf(NAME_Warning, TEXT("Failed to create FLightmassTerrainMaterialResourceProxy!"));
		return FALSE;
	}

	UBOOL bResult = TRUE;

	FFloat16Color UniformValue;
	if (MaterialProxy->WillGenerateUniformData(UniformValue) == FALSE)
	{
		//@lmtodo. The format may be determined by the material property...
		// For example, if Diffuse doesn't need to be F16 it can create a standard RGBA8 target.
		EPixelFormat Format = PF_FloatRGB;
		if (MaterialProxy->GetRenderTargetFormatAndSize(InMaterialProperty, Format, 1.0f, InOutSizeX, InOutSizeY))
		{
			if (CreateRenderTarget(Format, InOutSizeX, InOutSizeY) == FALSE)
			{
				warnf(NAME_Warning, TEXT("Failed to create %4dx%4d render target!"), InOutSizeX, InOutSizeY);
				bResult = FALSE;
			}
			else
			{
				// At this point, we can't just return false at failure since we have some clean-up to do...
				Canvas->SetRenderTarget(RenderTarget->GetRenderTargetResource());
				RHIBeginScene();
				// Clear the render target to black
				Clear(Canvas, FLinearColor(0,0,0,0));
				// Freeze time while capturing the material's inputs
				::DrawTile(Canvas,0,0,InOutSizeX,InOutSizeY,0,0,1,1,MaterialProxy,TRUE);
				Canvas->Flush();
				FlushRenderingCommands();
				Canvas->SetRenderTarget(NULL);
				FlushRenderingCommands();
				RHIEndScene();

				if (GLightmassDebugOptions.bDebugMaterials == TRUE)
				{
					TArray<FColor> OutputBuffer;
					if (RenderTarget->GetRenderTargetResource()->ReadPixels(OutputBuffer))
					{
						// Create screenshot folder if not already present.
						// Save the contents of the array to a bitmap file.
						FString TempPath = appScreenShotDir();
						TempPath += TEXT("/Materials");
						GFileManager->MakeDirectory(*TempPath, TRUE);
						FString TempName = InMaterial.GetFriendlyName();
						TempName = TempName.Replace(TEXT("."), TEXT("_"));
						TempName = TempName.Replace(TEXT(":"), TEXT("_"));
						FString OutputName = TempPath * TempName;
						OutputName += TEXT("_");
						switch (InMaterialProperty)
						{
						case MP_DiffuseColor:	OutputName += TEXT("Diffuse");			break;
						case MP_EmissiveColor:	OutputName += TEXT("Emissive");			break;
						case MP_SpecularColor:	OutputName += TEXT("Specular");			break;
						case MP_Normal:			OutputName += TEXT("Normal");			break;
						case MP_Opacity:		OutputName += TEXT("Transmissive");		break;
						}
						OutputName += TEXT(".BMP");
						appCreateBitmap(*OutputName,InOutSizeX,InOutSizeY,&OutputBuffer(0),GFileManager);
					}
				}

				// Read in the data
				//@lmtodo. Check the format! RenderTarget->Format
				// If we are going to allow non-F16 formats, then the storage will have to be aware of it!
				if (RenderTarget->GetRenderTargetResource()->ReadFloat16Pixels(OutMaterialSamples) == FALSE)
				{
					warnf(NAME_Warning, TEXT("Failed to read Float16Pixels for 0x%08x property of %s: %s"), 
						(DWORD)InMaterialProperty, *(InMaterial.GetLightingGuid().String()), *(InMaterial.GetFriendlyName()));
					bResult = FALSE;
				}
			}
		}
		else
		{
			warnf(NAME_Warning, TEXT("Failed to get render target format and size for 0x%08x property of %s: %s"), 
				(DWORD)InMaterialProperty, *(InMaterial.GetLightingGuid().String()), *(InMaterial.GetFriendlyName()));
			bResult = FALSE;
		}
	}
	else
	{
		// Single value... fill it in.
		InOutSizeX = 1;
		InOutSizeY = 1; 
		OutMaterialSamples.Empty(1);
		OutMaterialSamples.AddZeroed(1);
		OutMaterialSamples(0) = UniformValue;
	}

	// Clean up
	delete MaterialProxy;

	return bResult;
}

#endif // WITH_MANAGED_CODE
