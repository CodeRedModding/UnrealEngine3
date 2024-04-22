/*=============================================================================
	LightMapRendering.h: Light map rendering definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LIGHTMAPRENDERING_H__
#define __LIGHTMAPRENDERING_H__

extern FLOAT GLargestLightmapMipLevel;
extern INT GMipColorTextureMipLevels;
extern UBOOL GShowDebugSelectedLightmap;
extern FLightMap2D* GDebugSelectedLightmap;
extern UBOOL GVisualizeMipLevels;

/**
 */
class FNullLightMapShaderComponent
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{}
	void Serialize(FArchive& Ar)
	{}
};

/**
 * A policy for shaders without a light-map.
 */
class FNoLightMapPolicy
{
public:

	typedef FNullLightMapShaderComponent VertexParametersType;
	typedef FNullLightMapShaderComponent PixelParametersType;
	typedef FMeshDrawingPolicy::ElementDataType ElementDataType;

	// Translucent lit materials without a light-map should be drawn unlit.
	static const UBOOL bDrawLitTranslucencyUnlit = TRUE;
	static const UBOOL bAllowPreviewSkyLight = TRUE;

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType,UBOOL bEnableSkyLight=FALSE)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("NUM_LIGHTMAP_COEFFICIENTS"),*FString::Printf(TEXT("%u"),NUM_DIRECTIONAL_LIGHTMAP_COEF));
	}

	void Set(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView* View
		) const
	{
		check(VertexFactory);
		VertexFactory->Set();
	}

	/**
	* Get the decl and stream strides for this policy type and vertexfactory
	* @param VertexDeclaration - output decl
	* @param StreamStrides - output array of vertex stream strides
	* @param VertexFactory - factory to be used by this policy
	*/
	void GetVertexDeclarationInfo(FVertexDeclarationRHIParamRef &VertexDeclaration, DWORD *StreamStrides, const FVertexFactory* VertexFactory) const
	{
		check(VertexFactory);
		VertexFactory->GetStreamStrides(StreamStrides);
		VertexDeclaration = VertexFactory->GetDeclaration();
	}

	void SetMesh(
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
	{}

	friend UBOOL operator==(const FNoLightMapPolicy A,const FNoLightMapPolicy B)
	{
		return TRUE;
	}

	friend INT Compare(const FNoLightMapPolicy&,const FNoLightMapPolicy&)
	{
		return 0;
	}

};

/**
 * Base policy for shaders with vertex lightmaps.
 */
class FVertexLightMapPolicy
{
public:

	typedef FLightMapInteraction ElementDataType;

	static const UBOOL bDrawLitTranslucencyUnlit = FALSE;
	static const UBOOL bAllowPreviewSkyLight = FALSE;

	class VertexParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			LightMapScaleParameter.Bind(ParameterMap,TEXT("LightMapScale"), TRUE);
		}
		void SetLightMapScale(FShader* VertexShader,const FLightMapInteraction& LightMap) const
		{
			SetVertexShaderValues(VertexShader->GetVertexShader(),LightMapScaleParameter,LightMap.GetScaleArray(),LightMap.GetNumLightmapCoefficients());
		}
		void Serialize(FArchive& Ar)
		{
			Ar << LightMapScaleParameter;
			
			LightMapScaleParameter.SetShaderParamName(TEXT("LightMapScale"));
		}
	private:
		FShaderParameter LightMapScaleParameter;
	};

	typedef FNullLightMapShaderComponent PixelParametersType;

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->GetLightingModel() != MLM_Unlit
			&& VertexFactoryType->SupportsStaticLighting()
			&& (Material->IsUsedWithStaticLighting() || Material->IsSpecialEngineMaterial())
			//terrain never uses vertex lightmaps
			&& !Material->IsTerrainMaterial();
	}

	void Set(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView* View
		) const
	{}
};

/**
 * Policy for directional vertex lightmaps, where lighting is stored along each of the lightmap bases.
 */
class FDirectionalVertexLightMapPolicy : public FVertexLightMapPolicy
{
public:

	/**
	* Get the decl and stream strides for this policy type and vertexfactory
	* @param VertexDeclaration - output decl
	* @param StreamStrides - output array of vertex stream strides
	* @param VertexFactory - factory to be used by this policy
	*/
	void GetVertexDeclarationInfo(FVertexDeclarationRHIParamRef &VertexDeclaration, DWORD *StreamStrides, const FVertexFactory* VertexFactory) const
	{
		check(VertexFactory);
		VertexFactory->GetVertexLightMapStreamStrides(StreamStrides,TRUE);
		VertexDeclaration = VertexFactory->GetVertexLightMapDeclaration(TRUE);
	}

	void SetMesh(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FLightMapInteraction& LightMapInteraction
		) const
	{
		check(VertexFactory);
		VertexFactory->SetVertexLightMap(LightMapInteraction.GetVertexBuffer(),TRUE);
		VertexShaderParameters->SetLightMapScale(VertexShader,LightMapInteraction);
	}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType,UBOOL bEnableSkyLight=FALSE)
	{
		// if material requires simple static lighting then don't cache full directional shader
		if( Material && Material->RequiresSimpleStaticLighting() )
		{
			return FALSE;
		}
		// instanced meshes doesn't need vertex lighting
		if (appStrstr(VertexFactoryType->GetName(), TEXT("FInstancedStaticMeshVertex")))
		{
			return FALSE;
		}
		else
		{
			return FVertexLightMapPolicy::ShouldCache(Platform, Material, VertexFactoryType);
		}
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("VERTEX_LIGHTMAP"),TEXT("1"));
		OutEnvironment.Definitions.Set(TEXT("NUM_LIGHTMAP_COEFFICIENTS"),*FString::Printf(TEXT("%u"),NUM_DIRECTIONAL_LIGHTMAP_COEF));
	}

	friend UBOOL operator==(const FDirectionalVertexLightMapPolicy A,const FDirectionalVertexLightMapPolicy B)
	{
		return TRUE;
	}

	friend INT Compare(const FDirectionalVertexLightMapPolicy&,const FDirectionalVertexLightMapPolicy&)
	{
		return 0;
	}
};

/**
 * Policy for simple vertex lightmaps, where lighting is only stored along the surface normal.
 */
class FSimpleVertexLightMapPolicy : public FVertexLightMapPolicy
{
public:

	/**
	* Get the decl and stream strides for this policy type and vertexfactory
	* @param VertexDeclaration - output decl
	* @param StreamStrides - output array of vertex stream strides
	* @param VertexFactory - factory to be used by this policy
	*/
	void GetVertexDeclarationInfo(FVertexDeclarationRHIParamRef &VertexDeclaration, DWORD *StreamStrides, const FVertexFactory* VertexFactory) const
	{
		check(VertexFactory);
		VertexFactory->GetVertexLightMapStreamStrides(StreamStrides,FALSE);
		VertexDeclaration = VertexFactory->GetVertexLightMapDeclaration(FALSE);
	}

	void SetMesh(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FLightMapInteraction& LightMapInteraction
		) const
	{
		check(VertexFactory);
		VertexFactory->SetVertexLightMap(LightMapInteraction.GetVertexBuffer(),FALSE);
		VertexShaderParameters->SetLightMapScale(VertexShader,LightMapInteraction);
	}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType,UBOOL bEnableSkyLight=FALSE)
	{
		// instanced meshes doesn't need vertex lighting
		if (appStrstr(VertexFactoryType->GetName(), TEXT("FInstancedStaticMeshVertex")))
		{
			return FALSE;
		}

		return FVertexLightMapPolicy::ShouldCache(Platform, Material, VertexFactoryType)
			//only compile for PC
			&& IsPCPlatform(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("SIMPLE_VERTEX_LIGHTMAP"),TEXT("1"));
		OutEnvironment.Definitions.Set(TEXT("NUM_LIGHTMAP_COEFFICIENTS"),*FString::Printf(TEXT("%u"),NUM_SIMPLE_LIGHTMAP_COEF));
	}

	friend UBOOL operator==(const FSimpleVertexLightMapPolicy A,const FSimpleVertexLightMapPolicy B)
	{
		return TRUE;
	}

	friend INT Compare(const FSimpleVertexLightMapPolicy&,const FSimpleVertexLightMapPolicy&)
	{
		return 0;
	}
};

/**
 * Base policy for shaders with lightmap textures.
 */
class FLightMapTexturePolicy
{
public:

	typedef FLightMapInteraction ElementDataType;

	static const UBOOL bDrawLitTranslucencyUnlit = FALSE;
	static const UBOOL bAllowPreviewSkyLight = FALSE;

	class PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			LightMapTexturesParameter.Bind(ParameterMap,TEXT("LightMapTextures"),TRUE);
			LightMapScaleParameter.Bind(ParameterMap,TEXT("LightMapScale"),TRUE);
		}
		void SetLightMapTextures(
			FShader* PixelShader,
			const UTexture2D* const * LightMapTextures,
			UINT NumLightmapTextures) const
		{
#if WITH_MOBILE_RHI
			if( GUsingMobileRHI )
			{
				for(UINT CoefficientIndex = 0;CoefficientIndex < NumLightmapTextures;CoefficientIndex++)
				{
					FTexture* LightMapTexture = LightMapTextures[CoefficientIndex]->Resource;
					LightMapTexture->LastRenderTime = GCurrentTime;
					RHISetMobileTextureSamplerState(PixelShader->GetPixelShader(), (CoefficientIndex == 0) ? Lightmap_MobileTexture : Lightmap2_MobileTexture, LightMapTexture->SamplerStateRHI, LightMapTexture->TextureRHI, 0.0f, -1.0f, -1.0f);
				}
			}
			else
#endif
			{
				UBOOL bShowMipLevels = FALSE;
#if !FINAL_RELEASE
				if ( GShowDebugSelectedLightmap && GDebugSelectedLightmap )
				{
					bShowMipLevels = GDebugSelectedLightmap->GetTexture(0) == LightMapTextures[0];
				}
				else
				{
					bShowMipLevels = GVisualizeMipLevels;
				}
#endif

				for(UINT CoefficientIndex = 0;CoefficientIndex < NumLightmapTextures;CoefficientIndex++)
				{
					if ( bShowMipLevels == FALSE || CoefficientIndex != 0 )
					{
						FLOAT MipBias = LightMapTextures[CoefficientIndex]->Resource->MipBiasFade.CalcMipBias();
						if ( MipBias != 0.0f )
						{
							INT q=0;
						}
						SetTextureParameter(
							PixelShader->GetPixelShader(),
							LightMapTexturesParameter,
							LightMapTextures[CoefficientIndex]->Resource,
							CoefficientIndex,
							MipBias,
							GLargestLightmapMipLevel
							);
					}
					else
					{
						if ( GMipColorTexture->IsInitialized() == FALSE )
						{
							GMipColorTexture->InitResource();
						}
						FLOAT LargestMipLevel = Max<FLOAT>(GLargestLightmapMipLevel, GMipColorTextureMipLevels - LightMapTextures[CoefficientIndex]->ResidentMips);
						SetTextureParameter(
							PixelShader->GetPixelShader(),
							LightMapTexturesParameter,
							GMipColorTexture,
							CoefficientIndex,
							0.0f,
							LargestMipLevel
							);
						// Make sure we still set LastRenderTime on the original texture so it doesn't get streamed out unnecessarily.
						LightMapTextures[CoefficientIndex]->Resource->LastRenderTime = GCurrentTime;
					}
				}
			}
		}
		void SetLightMapScale(FShader* PixelShader,const FLightMapInteraction& LightMapInteraction) const
		{
			SetPixelShaderValues(PixelShader->GetPixelShader(),LightMapScaleParameter,LightMapInteraction.GetScaleArray(),LightMapInteraction.GetNumLightmapCoefficients());
		}
		void Serialize(FArchive& Ar)
		{
			Ar << LightMapTexturesParameter;
			Ar << LightMapScaleParameter;
			
			// set parameter names for platforms that need them
			LightMapScaleParameter.SetShaderParamName(TEXT("LightMapScale"));
		}
	private:
		FShaderResourceParameter LightMapTexturesParameter;
		FShaderParameter LightMapScaleParameter;
	};

	class VertexParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			LightmapCoordinateScaleBiasParameter.Bind(ParameterMap,TEXT("LightmapCoordinateScaleBias"),TRUE);
		}
		void SetCoordinateTransform(FShader* VertexShader,const FVector2D& LightmapCoordinateScale,const FVector2D& LightmapCoordinateBias) const
		{
			SetVertexShaderValue(VertexShader->GetVertexShader(),LightmapCoordinateScaleBiasParameter,FVector4(
				LightmapCoordinateScale.X,
				LightmapCoordinateScale.Y,
				LightmapCoordinateBias.Y,
				LightmapCoordinateBias.X
				));
		}
		void Serialize(FArchive& Ar)
		{
			Ar << LightmapCoordinateScaleBiasParameter;

			// set parameter names for platforms that need them
			LightmapCoordinateScaleBiasParameter.SetShaderParamName(TEXT("LightmapCoordinateScaleBias"));
		}
	private:
		FShaderParameter LightmapCoordinateScaleBiasParameter;
	};

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->GetLightingModel() != MLM_Unlit && VertexFactoryType->SupportsStaticLighting() && (Material->IsUsedWithStaticLighting() || Material->IsSpecialEngineMaterial());
	}

	/**
	* Get the decl and stream strides for this policy type and vertexfactory
	* @param VertexDeclaration - output decl
	* @param StreamStrides - output array of vertex stream strides
	* @param VertexFactory - factory to be used by this policy
	*/
	void GetVertexDeclarationInfo(FVertexDeclarationRHIParamRef &VertexDeclaration, DWORD *StreamStrides, const FVertexFactory* VertexFactory) const
	{
		check(VertexFactory);
		VertexFactory->GetStreamStrides(StreamStrides);
		VertexDeclaration = VertexFactory->GetDeclaration();
	}

	void SetMesh(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FLightMapInteraction& LightMapInteraction
		) const
	{
		VertexShaderParameters->SetCoordinateTransform(
			VertexShader,
			LightMapInteraction.GetCoordinateScale(),
			LightMapInteraction.GetCoordinateBias()
			);
		if(PixelShaderParameters)
		{
			PixelShaderParameters->SetLightMapScale(PixelShader,LightMapInteraction);
		}
	}
};

/**
 * Policy for directional texture lightmaps, where lighting is stored along each of the lightmap bases.
 */
class FDirectionalLightMapTexturePolicy : public FLightMapTexturePolicy
{
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType,UBOOL bEnableSkyLight=FALSE)
	{
		// if material requires simple static lighting then don't cache full directional shader
		if( Material && Material->RequiresSimpleStaticLighting() )
		{
			return FALSE;
		}
		else
		{
			return FLightMapTexturePolicy::ShouldCache(Platform,Material,VertexFactoryType);
		}
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("TEXTURE_LIGHTMAP"),TEXT("1"));
		OutEnvironment.Definitions.Set(TEXT("NUM_LIGHTMAP_COEFFICIENTS"),*FString::Printf(TEXT("%u"),NUM_DIRECTIONAL_LIGHTMAP_COEF));
	}

	void Set(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView* View
		) const
	{
		check(VertexFactory);
		VertexFactory->Set();
	}

	void SetMesh(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FLightMapInteraction& LightMapInteraction
		) const
	{
		if(PixelShaderParameters)
		{
			const UTexture2D* LightMapTextures[NUM_DIRECTIONAL_LIGHTMAP_COEF];
			for(UINT CoefficientIndex = 0;CoefficientIndex < NUM_DIRECTIONAL_LIGHTMAP_COEF;CoefficientIndex++)
			{
				LightMapTextures[CoefficientIndex] = LightMapInteraction.GetTexture(CoefficientIndex);
			}
			PixelShaderParameters->SetLightMapTextures(PixelShader,LightMapTextures,NUM_DIRECTIONAL_LIGHTMAP_COEF);
		}

		FLightMapTexturePolicy::SetMesh(View, PrimitiveSceneInfo, VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, LightMapInteraction);
	}

	/** Initialization constructor. */
	FDirectionalLightMapTexturePolicy()
	{}

	friend UBOOL operator==(const FDirectionalLightMapTexturePolicy A,const FDirectionalLightMapTexturePolicy B)
	{
		return TRUE;
	}

	friend INT Compare(const FDirectionalLightMapTexturePolicy& A,const FDirectionalLightMapTexturePolicy& B)
	{
		return 0;
	}
};

/**
 * Policy for simple texture lightmaps, where lighting is only stored along the surface normal.
 */
class FSimpleLightMapTexturePolicy : public FLightMapTexturePolicy
{
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType,UBOOL bEnableSkyLight=FALSE)
	{
		return FLightMapTexturePolicy::ShouldCache(Platform,Material,VertexFactoryType)
			//only compile for PC
			&& IsPCPlatform(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("SIMPLE_TEXTURE_LIGHTMAP"),TEXT("1"));
		OutEnvironment.Definitions.Set(TEXT("NUM_LIGHTMAP_COEFFICIENTS"),*FString::Printf(TEXT("%u"),NUM_SIMPLE_LIGHTMAP_COEF));
	}

	/** Initialization constructor. */
	FSimpleLightMapTexturePolicy()
	{}

	void Set(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView* View
		) const
	{
		check(VertexFactory);
		VertexFactory->Set();
	}

	void SetMesh(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,

		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FLightMapInteraction& LightMapInteraction
		) const
	{
		if(PixelShaderParameters)
		{
			const UTexture2D* LightMapTextures[NUM_SIMPLE_LIGHTMAP_COEF];
			for(UINT CoefficientIndex = 0;CoefficientIndex < NUM_SIMPLE_LIGHTMAP_COEF;CoefficientIndex++)
			{
				LightMapTextures[CoefficientIndex] = LightMapInteraction.GetTexture(CoefficientIndex);
			}
			PixelShaderParameters->SetLightMapTextures(PixelShader,LightMapTextures,NUM_SIMPLE_LIGHTMAP_COEF);
		}
		FLightMapTexturePolicy::SetMesh(View, PrimitiveSceneInfo, VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, LightMapInteraction);
	}

	friend UBOOL operator==(const FSimpleLightMapTexturePolicy A,const FSimpleLightMapTexturePolicy B)
	{
		return TRUE;
	}

	friend INT Compare(const FSimpleLightMapTexturePolicy& A,const FSimpleLightMapTexturePolicy& B)
	{
		return 0;
	}
};

/**
 * Policy for 'fake' texture lightmaps, such as the LightMap density rendering mode
 */
class FDummyLightMapTexturePolicy : public FLightMapTexturePolicy
{
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType,UBOOL bEnableSkyLight=FALSE)
	{
		return FLightMapTexturePolicy::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Need this to get the LightMap coordinates
		OutEnvironment.Definitions.Set(TEXT("TEXTURE_LIGHTMAP"),TEXT("1"));
	}

	void Set(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView* View
		) const
	{
		check(VertexFactory);
		VertexFactory->Set();
	}

	/** Initialization constructor. */
	FDummyLightMapTexturePolicy()
	{
	}

	friend UBOOL operator==(const FDummyLightMapTexturePolicy A,const FDummyLightMapTexturePolicy B)
	{
		return TRUE;
	}

	friend INT Compare(const FDummyLightMapTexturePolicy& A,const FDummyLightMapTexturePolicy& B)
	{
		return 0;
	}
};

/** 
 * Encapsulates the parameters needed for doing forward shadowing,
 * Which is rendering the actual receiver geometry to get position information instead of deriving it from the depth buffer like deferred shadows do.
 */
class FForwardShadowingShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		bReceiveDynamicShadowsParameter.Bind(ParameterMap, TEXT("bReceiveDynamicShadows"), TRUE);
		ScreenToShadowMatrixParameter.Bind(ParameterMap,TEXT("ScreenToShadowMatrix"),TRUE);
		ShadowBufferAndTexelSizeParameter.Bind(ParameterMap,TEXT("ShadowBufferAndTexelSize"),TRUE);
		ShadowOverrideFactorParameter.Bind(ParameterMap,TEXT("ShadowOverrideFactor"),TRUE);
		ShadowDepthTextureParameter.Bind(ParameterMap,TEXT("ShadowDepthTexture"),TRUE);
	}

	void Serialize(FArchive& Ar)
	{
		Ar << bReceiveDynamicShadowsParameter;
		Ar << ScreenToShadowMatrixParameter;
		Ar << ShadowBufferAndTexelSizeParameter;
		Ar << ShadowOverrideFactorParameter;
		Ar << ShadowDepthTextureParameter;
	}

	/** Sets the bReceiveDynamicShadowsParameter parameter only. */
	void SetReceiveShadows(FShader* PixelShader, UBOOL bReceiveDynamicShadows) const;

	/** Sets all parameters except bReceiveDynamicShadowsParameter. */
	void Set(
		const FSceneView& View, 
		FShader* PixelShader, 
		UBOOL bOverrideDynamicShadowsOnTranslucency, 
		const FProjectedShadowInfo* TranslucentPreShadowInfo) const;

private:
	FShaderParameter bReceiveDynamicShadowsParameter;
	FShaderParameter ScreenToShadowMatrixParameter;
	FShaderParameter ShadowBufferAndTexelSizeParameter;
	FShaderParameter ShadowOverrideFactorParameter;
	FShaderResourceParameter ShadowDepthTextureParameter;
};

// A light map policy for rendering an directional light in the base pass that can be forward shadowed.
class FDirectionalLightLightMapPolicy
{
public:
	struct VertexParametersType
	{
		FShaderParameter LightPositionAndInvRadiusParameter;

		void Bind(const FShaderParameterMap& ParameterMap)
		{
			LightPositionAndInvRadiusParameter.Bind(ParameterMap, TEXT("LightPositionAndInvRadius"),TRUE);
		}

		void Serialize(FArchive& Ar)
		{
			Ar << LightPositionAndInvRadiusParameter;
		}
	};

	struct PixelParametersType
	{
		FShaderParameter LightColorParameter;
		FForwardShadowingShaderParameters ForwardShadowingParameters;

		void Bind(const FShaderParameterMap& ParameterMap)
		{
			LightColorParameter.Bind(ParameterMap, TEXT("LightColorAndFalloffExponent"), TRUE);
			ForwardShadowingParameters.Bind(ParameterMap);
		}

		void Serialize(FArchive& Ar)
		{
			Ar << LightColorParameter;
			ForwardShadowingParameters.Serialize(Ar);

			// set parameter names for platforms that need them
			LightColorParameter.SetShaderParamName(TEXT("LightColorAndFalloffExponent"));
		}
	};

	struct ElementDataType
	{
		const BITFIELD bReceiveDynamicShadows : 1;
		const BITFIELD bOverrideDynamicShadowsOnTranslucency : 1;
		const FProjectedShadowInfo* TranslucentPreShadowInfo;
		const FLightSceneInfo* Light;

		/** Initialization constructor. */
		ElementDataType(
			UBOOL bInReceiveDynamicShadows,
			UBOOL bInOverrideDynamicShadowsOnTranslucency,
			const FProjectedShadowInfo* InTranslucentPreShadowInfo,
			const FLightSceneInfo* InLight) 
			:
			bReceiveDynamicShadows(bInReceiveDynamicShadows),
			bOverrideDynamicShadowsOnTranslucency(bInOverrideDynamicShadowsOnTranslucency),
			TranslucentPreShadowInfo(InTranslucentPreShadowInfo),
			Light(InLight)
		{}
	};

	// Translucent lit materials with a directional light should not be drawn unlit.
	static const UBOOL bDrawLitTranslucencyUnlit = FALSE;
	static const UBOOL bAllowPreviewSkyLight = FALSE;

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType, UBOOL bEnableSkyLight=FALSE)
	{
		return VertexFactoryType->SupportsDynamicLighting() == TRUE
			&& Material->GetLightingModel() != MLM_Unlit;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("NUM_LIGHTMAP_COEFFICIENTS"),*FString::Printf(TEXT("%u"),NUM_DIRECTIONAL_LIGHTMAP_COEF));
		OutEnvironment.Definitions.Set(TEXT("ENABLE_DIRECTIONAL_LIGHT"), TEXT("1"));
		OutEnvironment.Definitions.Set(TEXT("TRANSLUCENCY_ONEPASS_DYNAMICALLY_SHADOWED"),TEXT("1"));
	}

	void Set(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView* View
		) const
	{
		check(VertexFactory);
		VertexFactory->Set();
	}

	/**
	* Get the decl and stream strides for this policy type and vertexfactory
	* @param VertexDeclaration - output decl
	* @param StreamStrides - output array of vertex stream strides
	* @param VertexFactory - factory to be used by this policy
	*/
	void GetVertexDeclarationInfo(FVertexDeclarationRHIParamRef &VertexDeclaration, DWORD *StreamStrides, const FVertexFactory* VertexFactory) const
	{
		check(VertexFactory);
		VertexFactory->GetStreamStrides(StreamStrides);
		VertexDeclaration = VertexFactory->GetDeclaration();
	}

	void SetMesh(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const ElementDataType& ElementData
		) const;

	friend UBOOL operator==(const FDirectionalLightLightMapPolicy& A,const FDirectionalLightLightMapPolicy& B)
	{
		return TRUE;
	}

	friend INT Compare(const FDirectionalLightLightMapPolicy& A,const FDirectionalLightLightMapPolicy& B)
	{
		return 0;
	}
};

// A light map policy for rendering an SH + directional light in the base pass.
class FSHLightLightMapPolicy : public FDirectionalLightLightMapPolicy
{
	typedef FDirectionalLightLightMapPolicy Super;
public:

	struct PixelParametersType : public Super::PixelParametersType
	{
		FShaderParameter WorldIncidentLightingParameter;

		void Bind(const FShaderParameterMap& ParameterMap)
		{
			Super::PixelParametersType::Bind(ParameterMap);
			WorldIncidentLightingParameter.Bind(ParameterMap,TEXT("WorldIncidentLighting"),TRUE);
		}

		void Serialize(FArchive& Ar)
		{
			Super::PixelParametersType::Serialize(Ar);
			Ar << WorldIncidentLightingParameter;
		}
	};

	struct ElementDataType
	{
		const FSHVectorRGB WorldSpaceIncidentLighting;
		Super::ElementDataType SuperElementData;

		/** Initialization constructor. */
		ElementDataType(
			const FSHVectorRGB& InWorldSpaceIncidentLighting, 
			const Super::ElementDataType& InSuperElementData)
			:
			WorldSpaceIncidentLighting(InWorldSpaceIncidentLighting),
			SuperElementData(InSuperElementData)
		{}
	};

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType, UBOOL bEnableSkyLight=FALSE)
	{
		return Super::ShouldCache(Platform, Material, VertexFactoryType, bEnableSkyLight);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("ENABLE_SH_LIGHT"), TEXT("1"));
	}

	void SetMesh(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const ElementDataType& ElementData
		) const;
};

// A light map policy for rendering a dynamically shadowed directional, spot or point light in the base pass. 
class FDynamicallyShadowedMultiTypeLightLightMapPolicy
{
public:

	struct VertexParametersType : FDirectionalLightLightMapPolicy::VertexParametersType
	{
	};

	struct PixelParametersType
	{
		FShaderParameter bDynamicDirectionalLightParameter;
		FShaderParameter bDynamicSpotLightParameter;
		FShaderParameter LightColorAndFalloffExponentParameter;
		FShaderParameter SpotDirectionParameter;
		FShaderParameter SpotAnglesParameter;
		FShaderParameter bEnableDistanceShadowFadingParameter;
		FShaderParameter DistanceFadeParameter;
		FShaderParameter LightChannelMaskParameter;
		FShaderResourceParameter LightAttenuationTextureParameter;
		FForwardShadowingShaderParameters ForwardShadowingParameters;

		void Bind(const FShaderParameterMap& ParameterMap)
		{
			bDynamicDirectionalLightParameter.Bind(ParameterMap, TEXT("bDynamicDirectionalLight"), TRUE);
			bDynamicSpotLightParameter.Bind(ParameterMap, TEXT("bDynamicSpotLight"), TRUE);
			LightColorAndFalloffExponentParameter.Bind(ParameterMap, TEXT("LightColorAndFalloffExponent"), TRUE);
			SpotDirectionParameter.Bind(ParameterMap, TEXT("SpotDirection"), TRUE);
			SpotAnglesParameter.Bind(ParameterMap, TEXT("SpotAngles"), TRUE);
			bEnableDistanceShadowFadingParameter.Bind(ParameterMap, TEXT("bEnableDistanceShadowFading"), TRUE);
			DistanceFadeParameter.Bind(ParameterMap, TEXT("DistanceFadeParameters"), TRUE);
			LightChannelMaskParameter.Bind(ParameterMap, TEXT("LightChannelMask"), TRUE);
			LightAttenuationTextureParameter.Bind(ParameterMap,TEXT("LightAttenuationTexture"),TRUE);
			ForwardShadowingParameters.Bind(ParameterMap);
		}

		void Serialize(FArchive& Ar)
		{
			Ar << bDynamicDirectionalLightParameter;
			Ar << bDynamicSpotLightParameter;
			Ar << LightColorAndFalloffExponentParameter;
			Ar << SpotDirectionParameter;
			Ar << SpotAnglesParameter;
			Ar << bEnableDistanceShadowFadingParameter;
			Ar << DistanceFadeParameter;
			Ar << LightChannelMaskParameter;
			Ar << LightAttenuationTextureParameter;
			ForwardShadowingParameters.Serialize(Ar);
		}
	};

	struct ElementDataType
	{
		const BITFIELD bReceiveDynamicShadows : 1;
		const BITFIELD bOverrideDynamicShadowsOnTranslucency : 1;
		const FProjectedShadowInfo* TranslucentPreShadowInfo;

		/** Initialization constructor. */
		ElementDataType(
			UBOOL bInReceiveDynamicShadows,
			UBOOL bInOverrideDynamicShadowsOnTranslucency,
			const FProjectedShadowInfo* InTranslucentPreShadowInfo) 
			:
			bReceiveDynamicShadows(bInReceiveDynamicShadows),
			bOverrideDynamicShadowsOnTranslucency(bInOverrideDynamicShadowsOnTranslucency),
			TranslucentPreShadowInfo(InTranslucentPreShadowInfo)
		{}
	};

	static const UBOOL bDrawLitTranslucencyUnlit = FALSE;
	static const UBOOL bAllowPreviewSkyLight = FALSE;

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType, UBOOL bEnableSkyLight=FALSE)
	{
		// Only xbox, PS3, WiiU have GOnePassDominantLight == TRUE
		return (Platform == SP_XBOXD3D || Platform == SP_PS3 || Platform == SP_WIIU)
			&& VertexFactoryType->SupportsDynamicLighting() == TRUE
			&& Material->GetLightingModel() != MLM_Unlit;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Enable masking the directional light by the shadow factor accumulated in the light attenuation buffer
		OutEnvironment.Definitions.Set(TEXT("DYNAMICALLY_SHADOWED_BASEPASS_LIGHT"),TEXT("1"));
		OutEnvironment.Definitions.Set(TEXT("ENABLE_MULTITYPE_BASEPASS_LIGHT"),TEXT("1"));
		OutEnvironment.Definitions.Set(TEXT("NUM_LIGHTMAP_COEFFICIENTS"),*FString::Printf(TEXT("%u"),NUM_DIRECTIONAL_LIGHTMAP_COEF));
	}

	FDynamicallyShadowedMultiTypeLightLightMapPolicy(const FLightSceneInfo* InLightSceneInfo, UBOOL bInUseTranslucencyLightAttenuation):
		LightSceneInfo(InLightSceneInfo),
		bUseTranslucencyLightAttenuation(bInUseTranslucencyLightAttenuation)
	{
		checkSlow(LightSceneInfo);
	}

	void Set(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView* View
		) const;

	/**
	* Get the decl and stream strides for this policy type and vertexfactory
	* @param VertexDeclaration - output decl
	* @param StreamStrides - output array of vertex stream strides
	* @param VertexFactory - factory to be used by this policy
	*/
	void GetVertexDeclarationInfo(FVertexDeclarationRHIParamRef &VertexDeclaration, DWORD *StreamStrides, const FVertexFactory* VertexFactory) const
	{
		check(VertexFactory);
		VertexFactory->GetStreamStrides(StreamStrides);
		VertexDeclaration = VertexFactory->GetDeclaration();
	}

	void SetMesh(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const ElementDataType& ElementData
		) const;

	friend UBOOL operator==(const FDynamicallyShadowedMultiTypeLightLightMapPolicy& A, const FDynamicallyShadowedMultiTypeLightLightMapPolicy& B)
	{
		return A.LightSceneInfo == B.LightSceneInfo
			&& A.bUseTranslucencyLightAttenuation == B.bUseTranslucencyLightAttenuation;
	}

	friend INT Compare(const FDynamicallyShadowedMultiTypeLightLightMapPolicy& A, const FDynamicallyShadowedMultiTypeLightLightMapPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(LightSceneInfo);
		COMPAREDRAWINGPOLICYMEMBERS(bUseTranslucencyLightAttenuation);
		return 0;
	}

protected:
	const FLightSceneInfo* LightSceneInfo;
	const BITFIELD bUseTranslucencyLightAttenuation : 1;
};

// A light map policy for rendering an SH + FDynamicallyShadowedMultiTypeLightLightMapPolicy
class FSHLightAndMultiTypeLightMapPolicy : public FDynamicallyShadowedMultiTypeLightLightMapPolicy
{
	typedef FDynamicallyShadowedMultiTypeLightLightMapPolicy Super;
public:

	struct ElementDataType
	{
		const FPrimitiveSceneInfo* PrimitiveSceneInfo;
		Super::ElementDataType SuperElementData;

		/** Initialization constructor. */
		ElementDataType(
			const FPrimitiveSceneInfo* InPrimitiveSceneInfo,
			const Super::ElementDataType& InSuperElementData) 
			:
			PrimitiveSceneInfo(InPrimitiveSceneInfo),
			SuperElementData(InSuperElementData)
		{}
	};

	struct PixelParametersType : public Super::PixelParametersType
	{
		FShaderParameter WorldIncidentLightingParameter;

		void Bind(const FShaderParameterMap& ParameterMap)
		{
			Super::PixelParametersType::Bind(ParameterMap);
			WorldIncidentLightingParameter.Bind(ParameterMap,TEXT("WorldIncidentLighting"),TRUE);
		}

		void Serialize(FArchive& Ar)
		{
			Super::PixelParametersType::Serialize(Ar);
			Ar << WorldIncidentLightingParameter;
		}
	};

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType, UBOOL bEnableSkyLight=FALSE)
	{
		return Super::ShouldCache(Platform, Material, VertexFactoryType, bEnableSkyLight);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("ENABLE_SH_LIGHT"), TEXT("1"));
	}

	FSHLightAndMultiTypeLightMapPolicy(const FLightSceneInfo* InLightSceneInfo, UBOOL bUseTranslucencyLightAttenuation) :
		Super(InLightSceneInfo, bUseTranslucencyLightAttenuation)
	{}

	void SetMesh(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const ElementDataType& ElementData
		) const;
};

// A light map policy for rendering a precomputed texture map shadowed dynamic light in the base pass.
class FTextureShadowedDynamicLightLightMapPolicy : public FDynamicallyShadowedMultiTypeLightLightMapPolicy
{
	typedef FDynamicallyShadowedMultiTypeLightLightMapPolicy Super;
public:

	struct ElementDataType
	{
		const FTexture* ShadowTexture;
		FVector2D LightmapCoordinateScale;
		FVector2D LightmapCoordinateBias;
		FVector2D ShadowCoordinateScale;
		FVector2D ShadowCoordinateBias;
		Super::ElementDataType SuperElementData;

		/** Initialization constructor. */
		ElementDataType(
			const FTexture* InShadowTexture,
			const FLightMapInteraction& InLightMapInteraction,
			const FVector2D& InShadowCoordinateScale,
			const FVector2D& InShadowCoordinateBias,
			const Super::ElementDataType& InSuperElementData) 
			:
			ShadowTexture(InShadowTexture),
			LightmapCoordinateScale(InLightMapInteraction.GetCoordinateScale()),
			LightmapCoordinateBias(InLightMapInteraction.GetCoordinateBias()),
			ShadowCoordinateScale(InShadowCoordinateScale),
			ShadowCoordinateBias(InShadowCoordinateBias),
			SuperElementData(InSuperElementData)
		{}
	};

	class VertexParametersType : public Super::VertexParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			ShadowCoordinateScaleBiasParameter.Bind(ParameterMap,TEXT("ShadowmapCoordinateScaleBias"),TRUE);
			Super::VertexParametersType::Bind(ParameterMap);
		}

		void SetMesh(FShader* VertexShader,
			const FVector2D& ShadowCoordinateScale ) const
		{
			// NOTE: This path is only used when rendering shadow maps on instanced static meshes
			//		with both a light map and shadow map in a single (bass) pass
			SetVertexShaderValue(VertexShader->GetVertexShader(),ShadowCoordinateScaleBiasParameter,FVector4(
				ShadowCoordinateScale.X,
				ShadowCoordinateScale.Y,
				0.0f,		// Passed per-instance in a vertex stream
				0.0f
				));
		}
		void Serialize(FArchive& Ar)
		{
			Ar << ShadowCoordinateScaleBiasParameter;
			Super::VertexParametersType::Serialize(Ar);
		}
	private:
		FShaderParameter ShadowCoordinateScaleBiasParameter;
	};

	class PixelParametersType : public Super::PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			ShadowCoordinateScaleBiasParameter.Bind(ParameterMap,TEXT("ShadowmapCoordinateScaleBias"),TRUE);
			ShadowTextureParameter.Bind(ParameterMap,TEXT("ShadowTexture"),TRUE);
			Super::PixelParametersType::Bind(ParameterMap);
		}

		void SetMesh(FShader* PixelShader,
			const FTexture* ShadowTexture,
			const FVector2D& LightmapCoordinateScale, 
			const FVector2D& LightmapCoordinateBias, 
			const FVector2D& ShadowCoordinateScale, 
			const FVector2D& ShadowCoordinateBias) const
		{
			checkSlow(LightmapCoordinateScale.X > 0.0f && LightmapCoordinateScale.Y > 0.0f);
			// All vertex factories encode Interpolants.LightMapCoordinate as:
			// Interpolants.LightMapCoordinate = Input.LightMapCoordinate * LightmapCoordinateScaleBias.xy + LightmapCoordinateScaleBias.wz

			// We want to re-use that interpolator for the shadow map coordinate, so we need to solve for Input.LightMapCoordinate and then apply ShadowmapCoordinateScaleBias:
			// return (Interpolants.LightMapCoordinate - LightmapCoordinateScaleBias.wz) / LightmapCoordinateScaleBias.xy * ShadowmapCoordinateScaleBias.xy + ShadowmapCoordinateScaleBias.zw;
			
			// This is collapsed down to one multiply and one add:
			// (Interpolated - LBias) / LScale * ShadowScale + ShadowBias
			// Interpolated / LScale * ShadowScale - LBias / LScale * ShadowScale + ShadowBias
			SetPixelShaderValue(PixelShader->GetPixelShader(),ShadowCoordinateScaleBiasParameter,FVector4(
				ShadowCoordinateScale.X / LightmapCoordinateScale.X,
				ShadowCoordinateScale.Y / LightmapCoordinateScale.Y,
				-LightmapCoordinateBias.X * ShadowCoordinateScale.X / LightmapCoordinateScale.X + ShadowCoordinateBias.X,
				-LightmapCoordinateBias.Y * ShadowCoordinateScale.Y / LightmapCoordinateScale.Y + ShadowCoordinateBias.Y
				));

			UBOOL bShowMipLevels = FALSE;
#if !FINAL_RELEASE
			bShowMipLevels = GVisualizeMipLevels;
#endif
			FLOAT MipBias = ShadowTexture->MipBiasFade.CalcMipBias();
			if ( MipBias != 0.0f )
			{
				INT q=0;
			}
			SetTextureParameter(PixelShader->GetPixelShader(),ShadowTextureParameter,bShowMipLevels ? GBlackTexture : ShadowTexture, 0, MipBias);
		}

		void Serialize(FArchive& Ar)
		{
			Ar << ShadowCoordinateScaleBiasParameter;
			Ar << ShadowTextureParameter;
			Super::PixelParametersType::Serialize(Ar);
		}

	private:
		FShaderParameter ShadowCoordinateScaleBiasParameter;
		FShaderResourceParameter ShadowTextureParameter;
	};

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType, UBOOL bEnableSkyLight=FALSE)
	{
		return Super::ShouldCache(Platform, Material, VertexFactoryType, bEnableSkyLight)
			&& VertexFactoryType->SupportsStaticLighting() &&
			(Material->IsUsedWithStaticLighting() || Material->IsSpecialEngineMaterial());
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("STATICLIGHTING_TEXTUREMASK"),TEXT("1"));
		Super::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FTextureShadowedDynamicLightLightMapPolicy(const FLightSceneInfo* InLightSceneInfo, UBOOL bUseTranslucencyLightAttenuation):
		Super(InLightSceneInfo, bUseTranslucencyLightAttenuation)
	{}

	void Set(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView* View
		) const
	{
		// Passing a NULL VertexFactory to the base class, so that it won't set the vertex factory streams.  
		// This lightmap policy is never used in isolation, but always used with TCombinedShadowedDynamicLightLightMapPolicy,
		// Whose other lightmap policy will take care of setting up vertex factory streams.
		Super::Set(VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, NULL, MaterialRenderProxy, View);
	}

	void SetMesh(
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
		if( VertexShaderParameters != NULL )
		{
			VertexShaderParameters->SetMesh(
				VertexShader,
				ElementData.ShadowCoordinateScale );
		}

		if (PixelShaderParameters)
		{
			PixelShaderParameters->SetMesh(PixelShader,
				ElementData.ShadowTexture,
				ElementData.LightmapCoordinateScale,
				ElementData.LightmapCoordinateBias,
				ElementData.ShadowCoordinateScale,
				ElementData.ShadowCoordinateBias);
		}

		Super::SetMesh(View, PrimitiveSceneInfo, VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, ElementData.SuperElementData);
	}
};

// A light map policy for rendering a signed distance field shadowed dynamic light in the base pass.
class FTextureDistanceFieldShadowedDynamicLightLightMapPolicy : public FTextureShadowedDynamicLightLightMapPolicy
{
	typedef FTextureShadowedDynamicLightLightMapPolicy Super;
public:

	class PixelParametersType : public Super::PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			DistanceFieldParameters.Bind(ParameterMap,TEXT("DistanceFieldParameters"),TRUE);
			Super::PixelParametersType::Bind(ParameterMap);
		}
		void Serialize(FArchive& Ar)
		{
			Ar << DistanceFieldParameters;
			Super::PixelParametersType::Serialize(Ar);
		}
		FShaderParameter DistanceFieldParameters;
	};

	struct ElementDataType : public Super::ElementDataType
	{
		const FLOAT PenumbraBias;
		const FLOAT PenumbraScale;
		const FLOAT ShadowExponent;

		/** Initialization constructor. */
		ElementDataType(
			const FTexture* InShadowTexture,
			const FLightMapInteraction& InLightMapInteraction,
			const FVector2D& InShadowCoordinateScale,
			const FVector2D& InShadowCoordinateBias,
			FLOAT DistanceFieldPenumbraScale,
			const FDynamicallyShadowedMultiTypeLightLightMapPolicy::ElementDataType& InSuperElementData, 
			const FLightSceneInfo* InLightSceneInfo)
			:
			Super::ElementDataType(InShadowTexture, InLightMapInteraction, InShadowCoordinateScale, InShadowCoordinateBias, InSuperElementData),
			// Bias to convert distance from the distance field into the shadow penumbra based on penumbra size
			PenumbraBias(-.5f + Min(InLightSceneInfo->DistanceFieldShadowMapPenumbraSize * DistanceFieldPenumbraScale, 1.0f) * .5f),
			PenumbraScale(1.0f / Min(InLightSceneInfo->DistanceFieldShadowMapPenumbraSize * DistanceFieldPenumbraScale, 1.0f)),
			ShadowExponent(InLightSceneInfo->DistanceFieldShadowMapShadowExponent)
		{}
	};

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType, UBOOL bEnableSkyLight=FALSE)
	{
		return Super::ShouldCache(Platform, Material, VertexFactoryType, bEnableSkyLight);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("STATICLIGHTING_SIGNEDDISTANCEFIELD"),TEXT("1"));
		Super::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FTextureDistanceFieldShadowedDynamicLightLightMapPolicy(const FLightSceneInfo* InLightSceneInfo, UBOOL bUseTranslucencyLightAttenuation):
		FTextureShadowedDynamicLightLightMapPolicy(InLightSceneInfo, bUseTranslucencyLightAttenuation)
	{}

	void SetMesh(
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
		Super::SetMesh(View, PrimitiveSceneInfo, VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, ElementData);

		if (PixelShaderParameters)
		{
			UBOOL bShowMipLevels = FALSE;
#if !FINAL_RELEASE
			bShowMipLevels = GVisualizeMipLevels;
#endif
			if ( bShowMipLevels == FALSE )
			{
				SetPixelShaderValue(PixelShader->GetPixelShader(), PixelShaderParameters->DistanceFieldParameters, FVector(ElementData.PenumbraBias, ElementData.PenumbraScale, ElementData.ShadowExponent));
			}
			else
			{
				SetPixelShaderValue(PixelShader->GetPixelShader(), PixelShaderParameters->DistanceFieldParameters, FVector(0.0f, 1.0f, 1.0));
			}
		}
	}
};

// A light map policy for rendering a vertex shadowed directional light in the base pass.
class FVertexShadowedDynamicLightLightMapPolicy : public FDynamicallyShadowedMultiTypeLightLightMapPolicy
{
	typedef FDynamicallyShadowedMultiTypeLightLightMapPolicy Super;
public:

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType, UBOOL bEnableSkyLight=FALSE)
	{
		return Super::ShouldCache(Platform, Material, VertexFactoryType, bEnableSkyLight)
			&& VertexFactoryType->SupportsStaticLighting() && 
			(Material->IsUsedWithStaticLighting() || Material->IsSpecialEngineMaterial())
			//terrain never uses vertex shadowmaps
			&& !Material->IsTerrainMaterial();
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("STATICLIGHTING_VERTEXMASK"),TEXT("1"));
		Super::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FVertexShadowedDynamicLightLightMapPolicy(const FVertexBuffer* InVertexBuffer, const FLightSceneInfo* InLightSceneInfo, UBOOL bUseTranslucencyLightAttenuation):
		Super(InLightSceneInfo, bUseTranslucencyLightAttenuation),
		VertexBuffer(InVertexBuffer)
	{}

	void Set(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView* View
		) const
	{
		// Passing a NULL VertexFactory to the base class, so that it won't set the vertex factory streams.  
		// This lightmap policy is never used in isolation, but always used with TCombinedShadowedDynamicLightLightMapPolicy,
		// Whose other lightmap policy will take care of setting up vertex factory streams.
		Super::Set(VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, NULL, MaterialRenderProxy, View);
	}

	friend UBOOL operator==(const FVertexShadowedDynamicLightLightMapPolicy& A,const FVertexShadowedDynamicLightLightMapPolicy& B)
	{
		return A.VertexBuffer == B.VertexBuffer
			&& (const Super&)A == (const Super&)B;
	}

	friend INT Compare(const FVertexShadowedDynamicLightLightMapPolicy& A,const FVertexShadowedDynamicLightLightMapPolicy& B)
	{
		//@todo - this makes every vertex shadowmapped mesh be in a different static draw list policy set, 
		// Which means that DrawShared will get called for every mesh, which is suboptimal.
		// Should not compare based on VertexBuffer, just like FDirectionalVertexLightMapPolicy does not compare based on the lightmap vertex buffer.
		COMPAREDRAWINGPOLICYMEMBERS(VertexBuffer);
		return Compare((const Super&)A, (const Super&)B);
	}

protected:
	const FVertexBuffer* VertexBuffer;
};

// A light map policy which combines a shadowed dynamic light policy and another lightmap policy.
// This allows us to keep TBasePassDrawingPolicy the same while using it with two lightmap policies simultaneously.
template<class ShadowedDynamicLightPolicy, class OtherLightMapPolicy>
class TCombinedShadowedDynamicLightLightMapPolicy : public ShadowedDynamicLightPolicy, public OtherLightMapPolicy
{
public:

	struct CombinedElementDataType
	{
		typename ShadowedDynamicLightPolicy::ElementDataType DynamicLightPolicyData;
		typename OtherLightMapPolicy::ElementDataType OtherPolicyData;

		CombinedElementDataType(const typename ShadowedDynamicLightPolicy::ElementDataType& InDynamicLightPolicyData, const typename OtherLightMapPolicy::ElementDataType& InOtherPolicyData) :
			DynamicLightPolicyData(InDynamicLightPolicyData),
			OtherPolicyData(InOtherPolicyData)
		{}
	};
	typedef CombinedElementDataType ElementDataType;

	static const UBOOL bDrawLitTranslucencyUnlit = OtherLightMapPolicy::bDrawLitTranslucencyUnlit && ShadowedDynamicLightPolicy::bDrawLitTranslucencyUnlit;
	static const UBOOL bAllowPreviewSkyLight = OtherLightMapPolicy::bAllowPreviewSkyLight && ShadowedDynamicLightPolicy::bAllowPreviewSkyLight;

	struct VertexParametersType : public OtherLightMapPolicy::VertexParametersType, public ShadowedDynamicLightPolicy::VertexParametersType
	{
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			OtherLightMapPolicy::VertexParametersType::Bind(ParameterMap);
			ShadowedDynamicLightPolicy::VertexParametersType::Bind(ParameterMap);
		}

		void Serialize(FArchive& Ar)
		{
			OtherLightMapPolicy::VertexParametersType::Serialize(Ar);
			ShadowedDynamicLightPolicy::VertexParametersType::Serialize(Ar);
		}
	};

	struct PixelParametersType : public OtherLightMapPolicy::PixelParametersType, public ShadowedDynamicLightPolicy::PixelParametersType
	{
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			OtherLightMapPolicy::PixelParametersType::Bind(ParameterMap);
			ShadowedDynamicLightPolicy::PixelParametersType::Bind(ParameterMap);
		}

		void Serialize(FArchive& Ar)
		{
			OtherLightMapPolicy::PixelParametersType::Serialize(Ar);
			ShadowedDynamicLightPolicy::PixelParametersType::Serialize(Ar);
		}
	};

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType, UBOOL bEnableSkyLight=FALSE)
	{
		return OtherLightMapPolicy::ShouldCache(Platform, Material, VertexFactoryType, bEnableSkyLight) 
			&& ShadowedDynamicLightPolicy::ShouldCache(Platform, Material, VertexFactoryType, bEnableSkyLight);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		ShadowedDynamicLightPolicy::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OtherLightMapPolicy::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	TCombinedShadowedDynamicLightLightMapPolicy(const FLightSceneInfo* InLightSceneInfo, UBOOL bUseTranslucencyLightAttenuation) :
		ShadowedDynamicLightPolicy(InLightSceneInfo, bUseTranslucencyLightAttenuation)
	{}

	TCombinedShadowedDynamicLightLightMapPolicy(const FVertexBuffer* InVertexBuffer, const FLightSceneInfo* InLightSceneInfo, UBOOL bUseTranslucencyLightAttenuation) :
		ShadowedDynamicLightPolicy(InVertexBuffer, InLightSceneInfo, bUseTranslucencyLightAttenuation)
	{}

	void Set(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView* View
		) const
	{
		OtherLightMapPolicy::Set(VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, View);
		ShadowedDynamicLightPolicy::Set(VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, View);
	}

	void GetVertexDeclarationInfo(FVertexDeclarationRHIParamRef &VertexDeclaration, DWORD *StreamStrides, const FVertexFactory* VertexFactory) const
	{
		OtherLightMapPolicy::GetVertexDeclarationInfo(VertexDeclaration, StreamStrides, VertexFactory);
	}

	void SetMesh(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const ElementDataType& DataType
		) const
	{
		OtherLightMapPolicy::SetMesh(View, PrimitiveSceneInfo, VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, DataType.OtherPolicyData);
		ShadowedDynamicLightPolicy::SetMesh(View, PrimitiveSceneInfo, VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, DataType.DynamicLightPolicyData);
	}

	friend UBOOL operator==(const TCombinedShadowedDynamicLightLightMapPolicy<ShadowedDynamicLightPolicy, OtherLightMapPolicy>& A,const TCombinedShadowedDynamicLightLightMapPolicy<ShadowedDynamicLightPolicy, OtherLightMapPolicy>& B)
	{
		return (const OtherLightMapPolicy&)A == (const OtherLightMapPolicy&)B 
			&& (const ShadowedDynamicLightPolicy&)A == (const ShadowedDynamicLightPolicy&)B;
	}

	friend INT Compare(const TCombinedShadowedDynamicLightLightMapPolicy<ShadowedDynamicLightPolicy, OtherLightMapPolicy>& A,const TCombinedShadowedDynamicLightLightMapPolicy<ShadowedDynamicLightPolicy, OtherLightMapPolicy>& B)
	{
		const INT OtherLightMapPolicyResult = Compare((const OtherLightMapPolicy&)A, (const OtherLightMapPolicy&)B);
		if (OtherLightMapPolicyResult == 0)
		{
			return Compare((const ShadowedDynamicLightPolicy&)A, (const ShadowedDynamicLightPolicy&)B);
		}
		return OtherLightMapPolicyResult;
	}
};

/** A light map policy that combines a vertex shadow mapped dynamic light with directional vertex lightmaps. */
class FShadowedDynamicLightDirectionalVertexLightMapPolicy : public TCombinedShadowedDynamicLightLightMapPolicy<FVertexShadowedDynamicLightLightMapPolicy, FDirectionalVertexLightMapPolicy>
{
public:

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType, UBOOL bEnableSkyLight=FALSE)
	{
		return TCombinedShadowedDynamicLightLightMapPolicy<FVertexShadowedDynamicLightLightMapPolicy, FDirectionalVertexLightMapPolicy>::ShouldCache(Platform, Material, VertexFactoryType, bEnableSkyLight);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		TCombinedShadowedDynamicLightLightMapPolicy<FVertexShadowedDynamicLightLightMapPolicy, FDirectionalVertexLightMapPolicy>::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FShadowedDynamicLightDirectionalVertexLightMapPolicy(const FVertexBuffer* InVertexBuffer, const FLightSceneInfo* InLightSceneInfo, UBOOL bUseTranslucencyLightAttenuation) :
		TCombinedShadowedDynamicLightLightMapPolicy<FVertexShadowedDynamicLightLightMapPolicy, FDirectionalVertexLightMapPolicy>(InVertexBuffer, InLightSceneInfo, bUseTranslucencyLightAttenuation)
	{}

	void GetVertexDeclarationInfo(FVertexDeclarationRHIParamRef &VertexDeclaration, DWORD *StreamStrides, const FVertexFactory* VertexFactory) const
	{
		check(VertexFactory);
		VertexFactory->GetVertexLightMapAndShadowMapStreamStrides(StreamStrides);
		VertexDeclaration = VertexFactory->GetVertexLightMapAndShadowMapDeclaration();
	}

	void SetMesh(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const ElementDataType& DataType
		) const
	{
		check(VertexFactory);
		VertexFactory->SetVertexLightMapAndShadowMap(DataType.OtherPolicyData.GetVertexBuffer(), VertexBuffer);
		VertexShaderParameters->SetLightMapScale(VertexShader,DataType.OtherPolicyData);
		// Not calling TCombinedShadowedDynamicLightLightMapPolicy::SetMesh because special streams need to be setup when using vertex shadow maps and vertex lightmaps at the same time
		FVertexShadowedDynamicLightLightMapPolicy::SetMesh(View, PrimitiveSceneInfo, VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, DataType.DynamicLightPolicyData);
	}

	friend UBOOL operator==(const FShadowedDynamicLightDirectionalVertexLightMapPolicy& A, const FShadowedDynamicLightDirectionalVertexLightMapPolicy& B)
	{
		return (TCombinedShadowedDynamicLightLightMapPolicy<FVertexShadowedDynamicLightLightMapPolicy, FDirectionalVertexLightMapPolicy>&)A ==
			   (TCombinedShadowedDynamicLightLightMapPolicy<FVertexShadowedDynamicLightLightMapPolicy, FDirectionalVertexLightMapPolicy>&)B;
	}	
};

/** A light map policy that combines a texture shadow mapped dynamic light with directional texture lightmaps. */
class FShadowedDynamicLightDirectionalLightMapTexturePolicy : public TCombinedShadowedDynamicLightLightMapPolicy<FTextureShadowedDynamicLightLightMapPolicy, FDirectionalLightMapTexturePolicy>
{
public:

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType, UBOOL bEnableSkyLight=FALSE)
	{
		return TCombinedShadowedDynamicLightLightMapPolicy<FTextureShadowedDynamicLightLightMapPolicy, FDirectionalLightMapTexturePolicy>::ShouldCache(Platform, Material, VertexFactoryType, bEnableSkyLight);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		TCombinedShadowedDynamicLightLightMapPolicy<FTextureShadowedDynamicLightLightMapPolicy, FDirectionalLightMapTexturePolicy>::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FShadowedDynamicLightDirectionalLightMapTexturePolicy(const FLightSceneInfo* InLightSceneInfo, UBOOL bUseTranslucencyLightAttenuation) :
		TCombinedShadowedDynamicLightLightMapPolicy<FTextureShadowedDynamicLightLightMapPolicy, FDirectionalLightMapTexturePolicy>(InLightSceneInfo, bUseTranslucencyLightAttenuation)
	{}
	
	friend UBOOL operator==(const FShadowedDynamicLightDirectionalLightMapTexturePolicy& A, const FShadowedDynamicLightDirectionalLightMapTexturePolicy& B)
	{
		return (TCombinedShadowedDynamicLightLightMapPolicy<FTextureShadowedDynamicLightLightMapPolicy, FDirectionalLightMapTexturePolicy>&)A ==
			   (TCombinedShadowedDynamicLightLightMapPolicy<FTextureShadowedDynamicLightLightMapPolicy, FDirectionalLightMapTexturePolicy>&)B;
	}
};

/** A light map policy that combines a texture signed distance field shadow mapped dynamic light with directional texture lightmaps. */
class FDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicy : public TCombinedShadowedDynamicLightLightMapPolicy<FTextureDistanceFieldShadowedDynamicLightLightMapPolicy, FDirectionalLightMapTexturePolicy>
{
public:

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType, UBOOL bEnableSkyLight=FALSE)
	{
		return TCombinedShadowedDynamicLightLightMapPolicy<FTextureDistanceFieldShadowedDynamicLightLightMapPolicy, FDirectionalLightMapTexturePolicy>::ShouldCache(Platform, Material, VertexFactoryType, bEnableSkyLight);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		TCombinedShadowedDynamicLightLightMapPolicy<FTextureDistanceFieldShadowedDynamicLightLightMapPolicy, FDirectionalLightMapTexturePolicy>::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicy(const FLightSceneInfo* InLightSceneInfo, UBOOL bUseTranslucencyLightAttenuation) :
		TCombinedShadowedDynamicLightLightMapPolicy<FTextureDistanceFieldShadowedDynamicLightLightMapPolicy, FDirectionalLightMapTexturePolicy>(InLightSceneInfo, bUseTranslucencyLightAttenuation)
	{}

	friend UBOOL operator==(const FDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicy& A, const FDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicy& B)
	{
		return (TCombinedShadowedDynamicLightLightMapPolicy<FTextureDistanceFieldShadowedDynamicLightLightMapPolicy, FDirectionalLightMapTexturePolicy>&)A ==
			   (TCombinedShadowedDynamicLightLightMapPolicy<FTextureDistanceFieldShadowedDynamicLightLightMapPolicy, FDirectionalLightMapTexturePolicy>&)B;
	}
};

#endif // __LIGHTMAPRENDERING_H__
