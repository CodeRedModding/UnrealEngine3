/*=============================================================================
	UnTerrain.cpp: Terrain rendering code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "UnTerrain.h"
#include "UnTerrainRender.h"
#include "LevelUtils.h"
#include "EngineDecalClasses.h"
#include "ScenePrivate.h"
#include "LocalVertexFactoryShaderParms.h"

//
// Check to ensure the encoding of tessellation levels is not going to be incorrect.
//
#if (TERRAIN_MAXTESSELATION > (2 << 6))
	#error Terrain tessellation size too big!
#endif

/**
 *	TesselationLevel
 *	Determines the level of tesselation to use for a terrain patch.
 *
 *	@param	Z		The distance from the viewpoint
 *
 *	@return UINT	The tessellation level to utilize
 */
static UINT TesselationLevel(FLOAT Z, INT MinTessLevel)
{
	if (Z < 4096.0f)
	{
		return 16;
	}
	else
	if (Z < 8192.0f)
	{
		return Max<INT>(MinTessLevel, 8);
	}
	else
	if (Z < 16384.0f)
	{
		return Max<INT>(MinTessLevel, 4);
	}
	else
	if (Z < 32768.0f)
	{
		return Max<INT>(MinTessLevel, 2);
	}
	return Max<INT>(MinTessLevel, 1);
}

//
//	FTerrainMaterialResource
//

FTerrainMaterialResource::FTerrainMaterialResource(ATerrain* InTerrain,const FTerrainMaterialMask& InMask):
	Terrain(InTerrain),
	Mask(InMask),
	bParametersCached(FALSE),
	bEnableSpecular(InTerrain ? InTerrain->bEnableSpecular : FALSE)
{}

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
UBOOL FTerrainMaterialResource::ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
{
	//only compile the vertex factory that will be used based on the morphing and gradient flags
	if (Terrain)
	{
		if (Terrain->bMorphingEnabled)
		{
			if (Terrain->bMorphingGradientsEnabled)
			{
				if (appStrstr(VertexFactoryType->GetName(), TEXT("FTerrainFullMorphVertexFactory")) != NULL)
				{
					return TRUE;
				}
			}
			else
			{
				if (appStrstr(VertexFactoryType->GetName(), TEXT("FTerrainMorphVertexFactory")) != NULL)
				{
					return TRUE;
				}
			}
		}
		else
		{
			if (appStrstr(VertexFactoryType->GetName(), TEXT("FTerrainVertexFactory")) != NULL)
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

/**
 *	CompileTerrainMaterial
 *	Compiles a single terrain material.
 *
 *	@param	Property			The EMaterialProperty that the material is being compiled for
 *	@param	Compiler			The FMaterialCompiler* used to compile the material
 *	@param	TerrainMaterial		The UTerrainMaterial* to the terrain material that is being compiled
 *	@param	Highlighted			TRUE if the component is to be highlighted
 *	@param	HighlightColor		The color to use for the highlight
 *
 *	@return	INT					The resulting code index for the compiled material
 */
INT FTerrainMaterialResource::CompileTerrainMaterial(EMaterialProperty Property,FMaterialCompiler* Compiler,UTerrainMaterial* TerrainMaterial,UBOOL Highlighted, FColor& HighlightColor) const
{
	// FTerrainMaterialCompiler - A proxy compiler that overrides the texture coordinates used by the layer's material with the layer's texture coordinates.

	struct FTerrainMaterialCompiler: FProxyMaterialCompiler
	{
		UTerrainMaterial*	TerrainMaterial;

		FTerrainMaterialCompiler(FMaterialCompiler* InCompiler,UTerrainMaterial* InTerrainMaterial):
			FProxyMaterialCompiler(InCompiler),
			TerrainMaterial(InTerrainMaterial)
		{}

		// Override texture coordinates used by the material with the texture coordinate specified by the terrain material.

		virtual INT TextureCoordinate(UINT CoordinateIndex, UBOOL UnMirrorU, UBOOL UnMirrorV)
		{
			INT	BaseUV;
			switch(TerrainMaterial->MappingType)
			{
			case TMT_Auto:
			case TMT_XY: BaseUV = Compiler->TextureCoordinate(TERRAIN_UV_XY, UnMirrorU, UnMirrorV); break;
			case TMT_XZ: BaseUV = Compiler->TextureCoordinate(TERRAIN_UV_XZ, UnMirrorV, UnMirrorV); break;
			case TMT_YZ: BaseUV = Compiler->TextureCoordinate(TERRAIN_UV_YZ, UnMirrorV, UnMirrorV); break;
			default: appErrorf(TEXT("Invalid mapping type %u"),TerrainMaterial->MappingType); return INDEX_NONE;
			};

			FLOAT	Scale = TerrainMaterial->MappingScale == 0.0f ? 1.0f : TerrainMaterial->MappingScale;

			return Compiler->Add(
					Compiler->AppendVector(
						Compiler->Dot(BaseUV,Compiler->Constant2(+appCos(TerrainMaterial->MappingRotation * PI / 180.0) / Scale,+appSin(TerrainMaterial->MappingRotation * PI / 180.0) / Scale)),
						Compiler->Dot(BaseUV,Compiler->Constant2(-appSin(TerrainMaterial->MappingRotation * PI / 180.0) / Scale,+appCos(TerrainMaterial->MappingRotation * PI / 180.0) / Scale))
						),
					Compiler->Constant2(TerrainMaterial->MappingPanU,TerrainMaterial->MappingPanV)
					);
		}

		virtual INT FlipBookOffset(UTexture* InFlipBook)
		{
			return INDEX_NONE;
		}

		INT LensFlareIntesity()
		{
			return INDEX_NONE;
		}

		INT LensFlareOcclusion()
		{
			return INDEX_NONE;
		}

		INT LensFlareRadialDistance()
		{
			return INDEX_NONE;
		}

		INT LensFlareRayDistance()
		{
			return INDEX_NONE;
		}

		INT LensFlareSourceDistance()
		{
			return INDEX_NONE;
		}

		INT DynamicParameter()
		{
			return INDEX_NONE;
		}

		INT LightmassReplace(INT Realtime, INT Lightmass) { return Realtime; }

		/**
		 * Generates a shader code chunk for the DepthBiasedAlpha expression using the given inputs
		 * @param SrcAlphaIdx = index to source alpha input expression code chunk
		 * @param BiasIdx = index to bias input expression code chunk
		 * @param BiasScaleIdx = index to a scale expression code chunk to apply to the bias
		 */
		virtual INT DepthBiasedAlpha( INT SrcAlphaIdx, INT BiasIdx, INT BiasScaleIdx )
		{
			// Terrain can't reasonably sample the SceneDepth, so just pass the source alpha thru.
			return SrcAlphaIdx;
		}

		virtual INT OcclusionPercentage()
		{
			return Compiler->Constant(1.0f);
		}
	};

	UMaterialInterface*			MaterialInterface = TerrainMaterial ? (TerrainMaterial->Material ? TerrainMaterial->Material : GEngine->DefaultMaterial)  : GEngine->DefaultMaterial;
	UMaterial*					Material = MaterialInterface->GetMaterial();
	FTerrainMaterialCompiler	ProxyCompiler(Compiler,TerrainMaterial);

	INT	Result = Compiler->ForceCast(Material->MaterialResources[MSQ_TERRAIN]->CompileProperty(Property,&ProxyCompiler),GetMaterialPropertyType(Property));

	if(Highlighted)
	{
		FLinearColor SelectionColor(HighlightColor.ReinterpretAsLinear());
		switch(Property)
		{
		case MP_EmissiveColor:
			Result = Compiler->Add(Result,Compiler->Constant3(SelectionColor.R,SelectionColor.G,SelectionColor.B));
			break;
		case MP_DiffuseColor:
			Result = Compiler->Mul(Result,Compiler->Constant3(1.0f - SelectionColor.R,1.0f - SelectionColor.G,1.0f - SelectionColor.B));
			break;
		default: break;
		};
	}

	return Result;
}

/**
 * Gets the material to be used for mobile rendering
 */
const UMaterial* FTerrainMaterialResource::GetMobileMaterial() const
{
	const UMaterial* TerrainMaterialForMobile = NULL;

	check(Terrain);
	if (Terrain->Layers.Num() > 0)
	{
		const FTerrainLayer& TerrainLayer = Terrain->Layers(0);
		const UTerrainLayerSetup* Setup = TerrainLayer.Setup;
		if (Setup->Materials.Num())
		{
			const FTerrainFilteredMaterial& FilteredMaterial = Setup->Materials(0);
			const UTerrainMaterial* TerrainMaterial = FilteredMaterial.Material;
			if (TerrainMaterial)
			{
				TerrainMaterialForMobile = Cast<UMaterial>(TerrainMaterial->Material);
			}
		}
	}
	if (TerrainMaterialForMobile == NULL)
	{
		TerrainMaterialForMobile = GEngine->DefaultMaterial;
	}
	return TerrainMaterialForMobile;
}

/**
 *	CompileProperty
 *	Compiles the resource for the given property type using the given compiler.
 *
 *	@param	Property			The EMaterialProperty that the material is being compiled for
 *	@param	Compiler			The FMaterialCompiler* used to compile the material
 *
 *	@return	INT					The resulting code index for the compiled resource
 */
INT FTerrainMaterialResource::CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler) const
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
		if(Property == MP_TessellationMultiplier)
		{
			return Compiler->Constant(1);
		}
		else
		{
			appErrorf(TEXT("Unhandled terraion hull shader material input!"));
		}
	}

	// Count the number of terrain materials included in this material.
	INT	NumMaterials = 0;
	for(INT MaterialIndex = 0;MaterialIndex < Mask.Num();MaterialIndex++)
	{
		if(Mask.Get(MaterialIndex))
		{
			NumMaterials++;
		}
	}

	if(NumMaterials == 1)
	{
		for(INT MaterialIndex = 0;MaterialIndex < Mask.Num();MaterialIndex++)
		{
			if(Mask.Get(MaterialIndex))
			{
				if (MaterialIndex < Terrain->WeightedMaterials.Num())
				{
					FTerrainWeightedMaterial* WeightedMaterial = &(Terrain->WeightedMaterials(MaterialIndex));
					return CompileTerrainMaterial(
						Property,Compiler,WeightedMaterial->Material,
						WeightedMaterial->Highlighted,
						WeightedMaterial->HighlightColor);
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
			for(MaterialIndex = 0;MaterialIndex < Mask.Num();MaterialIndex++)
			{
				if(Mask.Get(MaterialIndex))
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
										WeightedMaterial->Highlighted, WeightedMaterial->HighlightColor);
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

			for(MaterialIndex = 0;MaterialIndex < Mask.Num();MaterialIndex++)
			{
				if(Mask.Get(MaterialIndex))
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
									WeightedMaterial->Highlighted,
									WeightedMaterial->HighlightColor
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
		return GEngine->DefaultMaterial->GetMaterialResource(MSQ_TERRAIN)->CompileProperty(Property,Compiler);
	}
}

#if WITH_MOBILE_RHI
/**
 * Fills in vertex params for mobile based on terrain materials
 */
void FTerrainMaterialResource::FillMobileMaterialVertexParams(FMobileMaterialVertexParams& OutVertexParams) const
{
	const UMaterial* TerrainMaterialForMobile = GetMobileMaterial();

	FMaterial::FillMobileMaterialVertexParams(TerrainMaterialForMobile, OutVertexParams);
}

/**
 * Fills in pixel params for mobile based on terrain materials
 */
void FTerrainMaterialResource::FillMobileMaterialPixelParams(FMobileMaterialPixelParams& OutPixelParams) const
{
	const UMaterial* TerrainMaterialForMobile = GetMobileMaterial();

	FMaterial::FillMobileMaterialPixelParams(TerrainMaterialForMobile, OutPixelParams);
}
#endif

/**
 *	GetFriendlyName
 *
 *	@return FString		The resource's friendly name
 */
FString FTerrainMaterialResource::GetFriendlyName() const
{
	// Concatenate the TerrainMaterial names.
	FString MaterialNames;
	for(INT MaterialIndex = 0;MaterialIndex < Mask.Num();MaterialIndex++)
	{
		if(Mask.Get(MaterialIndex))
		{
			if(MaterialNames.Len())
			{
				MaterialNames += TEXT("+");
			}
			if (MaterialIndex < Terrain->WeightedMaterials.Num())
			{
				FTerrainWeightedMaterial& WeightedMat = Terrain->WeightedMaterials(MaterialIndex);
				UTerrainMaterial* TerrainMaterial = WeightedMat.Material;
				if (TerrainMaterial)
				{
					MaterialNames += TerrainMaterial->GetName();
				}
				else
				{
					MaterialNames += TEXT("***NULLMAT***");
				}
			}
			else
			{
					MaterialNames += TEXT("***BADMATINDEX***");
			}
		}
	}
	return FString::Printf(TEXT("TerrainMaterialResource:%s"),*MaterialNames);;
}

/** Returns a string that describes the material's usage for debugging purposes. */
FString FTerrainMaterialResource::GetMaterialUsageDescription() const
{
	check(Terrain);
	FString BaseDescription = FString::Printf(TEXT("%s, %s, Terrain"), 
		*GetLightingModelString(GetLightingModel()), *GetBlendModeString(GetBlendMode()));

	if (Terrain->bMorphingEnabled)
	{
		if (Terrain->bMorphingGradientsEnabled)
		{
			BaseDescription += TEXT(", FullMorph");
		}
		else
		{
			BaseDescription += TEXT(", Morph");
		}
	}
	else
	{
		BaseDescription += TEXT(", NonMorph");
	}
	return BaseDescription;
}


/**
 *	GetVectorValue
 *	Retrives the vector value for the given parameter name.
 *
 *	@param	ParameterName		The name of the vector parameter to retrieve
 *	@param	OutValue			Pointer to a FLinearColor where the value should be placed.
 *
 *	@return	UBOOL				TRUE if parameter was found, FALSE if not
 */
UBOOL FTerrainMaterialResource::GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
{
	if (GIsEditor)
	{
		// Do a full search of all weighted materials in the editor, so that terrain MI parameters can be changed at runtime.
		for(INT MaterialIndex = 0;MaterialIndex < Terrain->WeightedMaterials.Num();MaterialIndex++)
		{
			UTerrainMaterial* TerrainMaterial = Terrain->WeightedMaterials(MaterialIndex).Material;
			UMaterialInterface* Material = TerrainMaterial ? 
				(TerrainMaterial->Material ? TerrainMaterial->Material : GEngine->DefaultMaterial) : 
				GEngine->DefaultMaterial;
			if(Material && Material->GetRenderProxy(0)->GetVectorValue(ParameterName, OutValue, Context))
			{
				return TRUE;
			}
		}
	}
	else
	{
		// In game only search the collapsed parameter map.
		// This prevents terrain MI parameters from being changeable in game, but speeds up material parameter setting.
		const FLinearColor* Value = CachedVectorParameterMap.Find(ParameterName);
		if(Value)
		{
			*OutValue = *Value;
			return TRUE;
		}
	}
	return FALSE;
}

/**
 *	GetScalarValue
 *	Retrives the Scalar value for the given parameter name.
 *
 *	@param	ParameterName		The name of the Scalar parameter to retrieve
 *	@param	OutValue			Pointer to a FLOAT where the value should be placed.
 *
 *	@return	UBOOL				TRUE if parameter was found, FALSE if not
 */
UBOOL FTerrainMaterialResource::GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
{
	if (GIsEditor)
	{
		// Do a full search of all weighted materials in the editor, so that terrain MI parameters can be changed at runtime.
		for(INT MaterialIndex = 0;MaterialIndex < Terrain->WeightedMaterials.Num();MaterialIndex++)
		{
			UMaterialInterface* Material = Terrain->WeightedMaterials(MaterialIndex).Material->Material;
			if(Material && Material->GetRenderProxy(0)->GetScalarValue(ParameterName, OutValue, Context))
			{
				return TRUE;
			}
		}
	}
	else
	{
		// In game only search the collapsed parameter map.
		// This prevents terrain MI parameters from being changeable in game, but speeds up material parameter setting.
		const FLOAT* Value = CachedScalarParameterMap.Find(ParameterName);
		if(Value)
		{
			*OutValue = *Value;
			return TRUE;
		}
	}
	return FALSE;
}

/**
 *	GetTextureValue
 *	Retrives the Texture value for the given parameter name.
 *	Checks for the name "TWeightMap*" to catch instances of weight map retrieval requests.
 *
 *	@param	ParameterName		The name of the Texture parameter to retrieve
 *	@param	OutValue			Pointer to a FTexture* where the value should be placed.
 *
 *	@return	UBOOL				TRUE if parameter was found, FALSE if not
 */
UBOOL FTerrainMaterialResource::GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
{
	UTexture2D* const* Texture2D = WeightMapsMap.Find(ParameterName);
	if (Texture2D)
	{
		if (*Texture2D)
		{
			*OutValue = (FTexture*)((*Texture2D)->Resource);
			return TRUE;
		}
	}

	if (GIsEditor)
	{
		// Do a full search of all weighted materials in the editor, so that terrain MI parameters can be changed at runtime.
		for(INT MaterialIndex = 0;MaterialIndex < Terrain->WeightedMaterials.Num();MaterialIndex++)
		{
			FTerrainWeightedMaterial& TWMat = Terrain->WeightedMaterials(MaterialIndex);
			if (TWMat.Material)
			{
				UMaterialInterface* Material = TWMat.Material->Material;
				if (Material)
				{
					const FMaterialRenderProxy* MatInst = Material->GetRenderProxy(FALSE);
					if (MatInst)
					{
						if (MatInst->GetTextureValue(ParameterName,OutValue,Context))
						{
							return TRUE;
						}
					}
				}
			}
		}
	}
	else
	{
		// In game only search the collapsed parameter map.
		// This prevents terrain MI parameters from being changeable in game, but speeds up material parameter setting.
		const UTexture* Value = CachedTextureParameterMap.FindRef(ParameterName);
		if(Value)
		{
			*OutValue = Value->Resource;
			return TRUE;
		}
		
	}
	return FALSE;
}

/**
 * Gathers parameters from the weighted materials that this resource was compiled with for optimal parameter setting performance in-game.
 */
void FTerrainMaterialResource::CacheParameters()
{
	// Don't cache parameters in the editor, otherwise the results of material instance parameter tweaking won't be seen until the component is re-attached.
	if (!GIsEditor && !bParametersCached)
	{
		// Iterate backwards to maintain legacy behavior with identically named parameters in different materials
		for(INT MaterialIndex = Terrain->WeightedMaterials.Num() - 1;MaterialIndex >= 0;MaterialIndex--)
		{
			FTerrainWeightedMaterial& TWMat = Terrain->WeightedMaterials(MaterialIndex);
			// Only get parameters from this material if it is in the mask
			//if (Mask.Get(MaterialIndex) && TWMat.Material)
			if (TWMat.Material)
			{
				UMaterialInterface* Material = TWMat.Material->Material;
				UMaterial* BaseMaterial = Material->GetMaterial();

				TArray<FName> VectorParameterNames;
				TArray<FGuid> VectorParameterIds;
				BaseMaterial->GetAllVectorParameterNames(VectorParameterNames, VectorParameterIds);

				// Cache vector parameters
				for (INT ParamIndex = 0; ParamIndex < VectorParameterNames.Num(); ParamIndex++)
				{
					FLinearColor VectorValue = FLinearColor::Black;
					const FName& ParamName = VectorParameterNames(ParamIndex);
					if (Material->GetVectorParameterValue(ParamName, VectorValue))
					{
						CachedVectorParameterMap.Set(ParamName, VectorValue);
					}
				}

				TArray<FName> ScalarParameterNames;
				TArray<FGuid> ScalarParameterIds;
				BaseMaterial->GetAllScalarParameterNames(ScalarParameterNames, ScalarParameterIds);

				// Cache scalar parameters
				for (INT ParamIndex = 0; ParamIndex < ScalarParameterNames.Num(); ParamIndex++)
				{
					FLOAT ScalarValue = 0.0f;
					const FName& ParamName = ScalarParameterNames(ParamIndex);
					if (Material->GetScalarParameterValue(ParamName, ScalarValue))
					{
						CachedScalarParameterMap.Set(ParamName, ScalarValue);
					}
				}

				TArray<FName> TextureParameterNames;
				TArray<FGuid> TextureParameterIds;
				BaseMaterial->GetAllTextureParameterNames(TextureParameterNames, TextureParameterIds);

				// Cache texture parameters
				for (INT ParamIndex = 0; ParamIndex < TextureParameterNames.Num(); ParamIndex++)
				{
					UTexture* TextureValue = NULL;
					const FName& ParamName = TextureParameterNames(ParamIndex);
					if (Material->GetTextureParameterValue(ParamName, TextureValue))
					{
						CachedTextureParameterMap.Set(ParamName, TextureValue);
					}
				}
			}
		}
		bParametersCached = TRUE;
	}
}

/**
 *	Serialize function for TerrainMaterialResource
 *
 *	@param	Ar			The archive to serialize to.
 *	@param	R			The terrain material resource to serialize.
 *
 *	@return	FArchive&	The archive used.
 */
FArchive& operator<<(FArchive& Ar,FTerrainMaterialResource& R)
{
	R.Serialize(Ar);

	Ar << (UObject*&)R.Terrain << R.Mask;
	Ar << R.MaterialIds;
	if (Ar.Ver() < VER_INTEGRATED_LIGHTMASS)
	{
		R.LightingGuid = appCreateGuid();
	}
	else
	{
		Ar << R.LightingGuid;
	}

	if( Ar.Ver() < VER_TERRAIN_SPECULAR_FIX )
	{
		R.bEnableSpecular = (R.Terrain && R.Terrain->bEnableSpecular);
	}
	else
	{
		Ar << R.bEnableSpecular;
	}

	if (Ar.Ver() < VER_UNIFORM_EXPRESSIONS_IN_SHADER_CACHE)
	{
		// Always add a reference to WeightMapPlaceholderTexture for legacy terrain material resources
		// As it will not be handled by ATerrain::HandleLegacyTextureReferences
		R.UniformExpressionTextures.AddUniqueItem(GEngine->WeightMapPlaceholderTexture);
	}

	return Ar;
}

/**
* Compiles material resources for the current platform if the shader map for that resource didn't already exist.
*
* @param ShaderPlatform - platform to compile for
* @param bFlushExistingShaderMaps - forces a compile, removes existing shader maps from shader cache.
*/
void ATerrain::CacheResourceShaders(EShaderPlatform ShaderPlatform, UBOOL bFlushExistingShaderMaps)
{
	//go through each material resource
	TArrayNoInit<FTerrainMaterialResource*>& CachedMaterials = GetCachedTerrainMaterials();
	for( INT MatIdx=0; MatIdx < CachedMaterials.Num(); MatIdx++ )
	{
		FTerrainMaterialResource* CachedMat = CachedMaterials(MatIdx);
		if( CachedMat )
		{
			//mark the package dirty if the material resource has never been compiled
			if (GIsEditor && !CachedMat->GetId().IsValid())
			{
				MarkPackageDirty();
			}

			if (appGetPlatformType() & UE3::PLATFORM_WindowsServer)
			{	
				//Only allocate shader resources if not running dedicated server
				continue;
			}

			if (bFlushExistingShaderMaps)
			{
				CachedMat->CacheShaders(ShaderPlatform, MSQ_TERRAIN);
				CachedMat->SetLightingGuid();
			}
			else
			{
				CachedMat->InitShaderMap(ShaderPlatform, MSQ_TERRAIN);
			}
		}
	}
}

/**
 *	Called after the terrain material resource has been loaded to
 *  verify the underlying material GUIDs that were stored
 *	with the resource. If any are different, the shader is tossed and has to
 *	be recompiled.
 */
void FTerrainMaterialResource::PostLoad()
{
	// Walk the material Ids and check for validity
	UBOOL bTossShader = FALSE;
	if (MaterialIds.Num() > 0)
	{
		INT IdIndex = 0;
		for(INT MaterialIndex = 0;MaterialIndex < Mask.Num();MaterialIndex++)
		{
			if(Mask.Get(MaterialIndex))
			{
				if (MaterialIndex < Terrain->WeightedMaterials.Num())
				{
					FTerrainWeightedMaterial& WeightedMat = Terrain->WeightedMaterials(MaterialIndex);
					UTerrainMaterial* TerrainMaterial = WeightedMat.Material;
					if (TerrainMaterial)
					{
						UMaterialInterface* MatRes = TerrainMaterial->Material;
						if (MatRes)
						{
							UMaterial* Material = MatRes->GetMaterial();
							if (Material && Material->MaterialResources[MSQ_TERRAIN])
							{
								if (IdIndex < MaterialIds.Num())
								{
									FGuid CheckGuid = MaterialIds(IdIndex++);
									if (Material->MaterialResources[MSQ_TERRAIN]->GetId() != CheckGuid)
									{
										bTossShader = TRUE;
										MaterialIds.Empty();
										break;
									}
								}
							}
							else
							{
								bTossShader = TRUE;
								break;
							}
						}
					}
					else
					{
						bTossShader = TRUE;
						break;
					}
				}
				else
				{
					bTossShader = TRUE;
					break;
				}
			}
		}
	}
	else
	{
		bTossShader = TRUE;
	}

	if (bTossShader == TRUE)
	{
		FMaterialShaderMap* LocalShaderMap = GetShaderMap();
		if (LocalShaderMap)
		{
			LocalShaderMap->Empty();
		}
	}
}

/** 
 *	Called before the resource is saved.  Stores the GUIDs for the weighted materials 
 *  referenced by the cached terrain materials
 */
void FTerrainMaterialResource::PreSave()
{
	// Walk the masks and store the GUIDs
	MaterialIds.Empty();
	for(INT MaterialIndex = 0;MaterialIndex < Mask.Num();MaterialIndex++)
	{
		if(Mask.Get(MaterialIndex))
		{
			if (MaterialIndex < Terrain->WeightedMaterials.Num())
			{
				FTerrainWeightedMaterial& WeightedMat = Terrain->WeightedMaterials(MaterialIndex);
				UTerrainMaterial* TerrainMaterial = WeightedMat.Material;
				if (TerrainMaterial)
				{
					UMaterialInterface* MatRes = TerrainMaterial->Material;
					if (MatRes)
					{
						UMaterial* Material = MatRes->GetMaterial();
						if (Material && Material->MaterialResources[MSQ_TERRAIN])
						{
							MaterialIds.AddItem(Material->MaterialResources[MSQ_TERRAIN]->GetId());
						}
						else
						{
							FGuid BaseId(0,0,0,0);
							MaterialIds.AddItem(BaseId);
						}
					}
				}
				else
				{
					FGuid BaseId(0,0,0,0);
					MaterialIds.AddItem(BaseId);
				}
			}
			else
			{
				FGuid BaseId(0,0,0,0);
				MaterialIds.AddItem(BaseId);
			}
		}
	}
}

/** 
* Returns a cached terrain material containing a given set of weighted materials.
* Generates a new entry if not found
*
* @param Mask - bitmask combination of weight materials to be used
* @param bIsTerrainResource - [out] TRUE if the material resource returned is a terrain material, FALSE if fallback
* @return terrain material resource render proxy or error material render proxy
*/
FMaterialRenderProxy* ATerrain::GetCachedMaterial(const FTerrainMaterialMask& Mask, UBOOL& bIsTerrainResource)
{
	// resulting material proxy used for rendering
	FTerrainMaterialResource* Result = NULL;

	// use the material platform based on the currently running shader platform
	// get the cached terrain materials for the platform 	
	TArrayNoInit<FTerrainMaterialResource*>& CachedMaterials = GetCachedTerrainMaterials();
	// search for an existing cached material entry based on current platform
	for (INT MaterialIndex = 0; MaterialIndex < CachedMaterials.Num(); MaterialIndex++)
	{
		FTerrainMaterialResource* CachedMaterial = CachedMaterials(MaterialIndex);
		// check for matching mask of weighted materials
		if( CachedMaterial && 
			CachedMaterial->GetMask() == Mask )
		{
			// found an existing entry
			Result = CachedMaterial;
			break;
		}
	}
	
#if !CONSOLE	
	// didn't find an existing entry so generate a new one
	if( !Result )
	{
		// create a new cached material entry for the mask
		Result = GenerateCachedMaterial(Mask);
		// cache its shaders
		if( Result )
		{
			// compile for the current RHI platform that is running
			Result->CacheShaders(GRHIShaderPlatform, MSQ_TERRAIN);
			Result->LightingGuid = appCreateGuid();
		}
		// Mark the package dirty so the map will be resaved
		if( GIsEditor )
		{
			MarkPackageDirty();
		}
	}
#endif

	// make sure the material is valid and that its shaders are cached
	if( Result && 
		Result->GetShaderMap() )
	{
		bIsTerrainResource = TRUE;
		return Result;		
	}
	else
	{
		bIsTerrainResource = FALSE;
		return AllowDebugViewmodes() ? 
			GEngine->TerrainErrorMaterial->GetRenderProxy(FALSE) :
			GEngine->DefaultMaterial->GetRenderProxy(FALSE);		
	}
}

/** 
* Creates new cached terrain material entry if it doesn't exist for the given mask
*
* @param Mask - bitmask combination of weight materials to be used
* @param Quality - Quality level to generate a material for
* @return new terrain material resource 
*/
FTerrainMaterialResource* ATerrain::GenerateCachedMaterial(const FTerrainMaterialMask& Mask)
{
	FTerrainMaterialResource* Result = NULL;
	// get the cached terrain materials for the platform 		
	TArrayNoInit<FTerrainMaterialResource*>& CachedMaterials = GetCachedTerrainMaterials();
	// search for an existing cached entry for the given mask
	for( INT MaterialIndex = 0; MaterialIndex < CachedMaterials.Num(); MaterialIndex++ )
	{
		FTerrainMaterialResource* CachedMaterial = CachedMaterials(MaterialIndex);
		if( CachedMaterial != NULL && CachedMaterial->GetMask() == Mask )
		{
			Result = CachedMaterial;
			break;
		}
	}
	// if not found then a new cache entry is added and return it
	if( !Result )
	{
		// find an empty spot
		INT EmptySlot = CachedMaterials.FindItemIndex(NULL);
		if( EmptySlot == INDEX_NONE )
		{
			EmptySlot = CachedMaterials.Add();
			debugf(TEXT("Increasing size of CachedMaterials to %d for %s"), CachedMaterials.Num(),*GetName());
		}
		check(EmptySlot >= 0);
		// create the new material resource for this mask entry
		CachedMaterials(EmptySlot) = Result = new FTerrainMaterialResource(this,Mask);
	}
	return Result;
}

//
//	FTerrainVertexBuffer
//

/**
 *	InitRHI
 *	Initialize the render hardware interface for the terrain vertex buffer.
 */
void FTerrainVertexBuffer::InitRHI()
{
	if (bIsDynamic == TRUE)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_TerrainSmoothTime);
	
	MaxVertexCount = (Component->SectionSizeX * MaxTessellation + 1) * 
		(Component->SectionSizeY * MaxTessellation + 1);

	// Create the buffer rendering resource
	UINT Stride = sizeof(FTerrainVertex);
	if (MorphingFlags == ETMORPH_Height)
	{
		Stride = sizeof(FTerrainMorphingVertex);
	}
	else
	if (MorphingFlags == ETMORPH_Full)
	{
		Stride = sizeof(FTerrainFullMorphingVertex);
	}
	UINT Size = MaxVertexCount * Stride;

	VertexBufferRHI = RHICreateVertexBuffer(Size, NULL, RUF_Static|RUF_WriteOnly);

	// Fill it...
	//@todo.SAS. Should we do this now, and likely repack it the first rendered frame
	// or just defer the packing until we determine the proper tessellation level for
	// the component (which occurs during the draw call)?
	FillData(MaxTessellation);
}

/** 
 * Initialize the dynamic RHI for this rendering resource 
 */
void FTerrainVertexBuffer::InitDynamicRHI()
{
	if (bIsDynamic == FALSE)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_TerrainSmoothTime);
	
	MaxVertexCount = (Component->SectionSizeX * MaxTessellation + 1) * 
		(Component->SectionSizeY * MaxTessellation + 1);

	// Create the buffer rendering resource
	UINT Stride = sizeof(FTerrainVertex);
	if (MorphingFlags == ETMORPH_Height)
	{
		Stride = sizeof(FTerrainMorphingVertex);
	}
	else
	if (MorphingFlags == ETMORPH_Full)
	{
		Stride = sizeof(FTerrainFullMorphingVertex);
	}
	UINT Size = MaxVertexCount * Stride;

	VertexBufferRHI = RHICreateVertexBuffer(Size, NULL, (MorphingFlags == ETMORPH_Full) ? RUF_Volatile : (RUF_Dynamic|RUF_WriteOnly) );

	// Fill it...
	//@todo.SAS. Should we do this now, and likely repack it the first rendered frame
	// or just defer the packing until we determine the proper tessellation level for
	// the component (which occurs during the draw call)?
	bRepackRequired = TRUE;
}

/** 
 * Release the dynamic RHI for this rendering resource 
 */
void FTerrainVertexBuffer::ReleaseDynamicRHI()
{
	if (bIsDynamic == FALSE)
	{
		return;
	}

	if (IsValidRef(VertexBufferRHI) == TRUE)
	{
		VertexBufferRHI.SafeRelease();
		bRepackRequired = TRUE;
	}
}

/**
 *	FillData
 *	Fills in the data for the vertex buffer
 *
 *	@param	TessellationLevel	The tessellation level to generate the vertex data for.
 *
 *	@return	UBOOL				TRUE if successful, FALSE if failed
 */
UBOOL FTerrainVertexBuffer::FillData(INT TessellationLevel)
{
	check(IsValidRef(VertexBufferRHI) == TRUE);
	check(TessellationLevel <= MaxTessellation);
	check(TessellationLevel > 0);

	SCOPE_CYCLE_COUNTER(STAT_TerrainSmoothTime);

	ATerrain* Terrain = CastChecked<ATerrain>(Component->GetOwner());

	INT SectSizeX = TerrainObject->GetComponentSectionSizeX();
	INT SectSizeY = TerrainObject->GetComponentSectionSizeY();
	VertexCount = (SectSizeX * TessellationLevel + 1) * (SectSizeY * TessellationLevel + 1);

	// Create the buffer rendering resource
	UINT Stride = sizeof(FTerrainVertex);
	if (MorphingFlags == ETMORPH_Height)
	{
		Stride = sizeof(FTerrainMorphingVertex);
	}
	else
	if (MorphingFlags == ETMORPH_Full)
	{
		Stride = sizeof(FTerrainFullMorphingVertex);
	}
	UINT Size = VertexCount * Stride;


#if CONSOLE
	// Lock the buffer.
	BYTE* Buffer = (BYTE*)(RHILockVertexBuffer(VertexBufferRHI, 0, Size, FALSE));
#else
	BYTE* OriginalBuffer = (BYTE*)appMalloc(Size);
	BYTE* Buffer = OriginalBuffer;
#endif

	// Determine the actual step size between true vertices for the component
	INT QuadSizeX = TerrainObject->GetComponentTrueSectionSizeX() / SectSizeX;
	INT QuadSizeY = TerrainObject->GetComponentTrueSectionSizeY() / SectSizeY;

	INT PackedCount = 0;

	// Handle non-uniform scaling
	FLOAT ScaleX = Terrain->DrawScale3D.Z / Terrain->DrawScale3D.X;
	FLOAT ScaleY = Terrain->DrawScale3D.Z / Terrain->DrawScale3D.Y;
	INT TerrainMaxTessLevel = Terrain->MaxTesselationLevel;
	const WORD GridLength = TerrainMaxTessLevel / TessellationLevel;

	for (INT Y = 0; Y <= SectSizeY; Y++)
	{
		for (INT X = 0; X <= SectSizeX; X++)
		{
			for (INT SmoothY = 0; SmoothY < (Y < SectSizeY ? TessellationLevel : 1); SmoothY++)
			{
				for (INT SmoothX = 0; SmoothX < (X < SectSizeX ? TessellationLevel : 1); SmoothX++)
				{
					INT OffsetX = SmoothX * GridLength;
					INT OffsetY = SmoothY * GridLength;
					INT LocalX = X * QuadSizeX + OffsetX;
					INT LocalY = Y * QuadSizeY + OffsetY;
					INT TrueX = Component->SectionBaseX + LocalX;
					INT TrueY = Component->SectionBaseY + LocalY;

					FTerrainVertex* DestVertex = (FTerrainVertex*)Buffer;

					DestVertex->X = LocalX;
					DestVertex->Y = LocalY;
					
					WORD Z = Terrain->Height(TrueX, TrueY);
					DestVertex->Z_LOBYTE = Z & 255;
					DestVertex->Z_HIBYTE = Z >> 8;

					const FLOAT HeightNegX = (FLOAT)Terrain->Height(TrueX - GridLength,TrueY);
					const FLOAT HeightPosX = (FLOAT)Terrain->Height(TrueX + GridLength,TrueY);
					const FLOAT HeightNegY = (FLOAT)Terrain->Height(TrueX,TrueY - GridLength);
					const FLOAT HeightPosY = (FLOAT)Terrain->Height(TrueX,TrueY + GridLength);
					DestVertex->GradientX = (SWORD)appTrunc((HeightPosX - HeightNegX) / (FLOAT)GridLength / 2.0f * ScaleX);
					DestVertex->GradientY = (SWORD)appTrunc((HeightPosY - HeightNegY) / (FLOAT)GridLength / 2.0f * ScaleY);

					if ((MorphingFlags && ETMORPH_Height) != 0)
					{
						FTerrainMorphingVertex* DestMorphVert = (FTerrainMorphingVertex*)Buffer;

						// Set the morphing components...
						INT TessIndexX = 0;
						INT TessIndexY = 0;
						INT Divisor = Terrain->MaxTesselationLevel;
						UBOOL bDone = FALSE;

						while (Divisor > 0)
						{
							if ((TrueX % Divisor) > 0)	{	TessIndexX++;	}
							if ((TrueY % Divisor) > 0)	{	TessIndexY++;	}
							Divisor /= 2;
						}

						INT TessIndex = Max<INT>(TessIndexX, TessIndexY);
						check(TessIndex >= 0);
						check(TessIndex <= 4);
						DestMorphVert->TESS_DATA_INDEX_LO = TessIndex;
						//DestMorphVert->TESS_DATA_INDEX_HI

						// Determine the height...
						WORD ZHeight;
						if (TessIndex == 0)
						{
							ZHeight = Terrain->Height(TrueX, TrueY);
							if (MorphingFlags == ETMORPH_Full)
							{
								FTerrainFullMorphingVertex* DestFullMorphVert = (FTerrainFullMorphingVertex*)Buffer;
								DestFullMorphVert->TransGradientX = DestVertex->GradientX;
								DestFullMorphVert->TransGradientY = DestVertex->GradientY;
							}
						}
						else
						{
							INT HeightCheckStepSize = TerrainMaxTessLevel / (TessIndex * 2);
							WORD Z0,Z1;

							// The step size for the actual packed vertex...
							INT PackStepSize = TerrainMaxTessLevel / (TessIndex);
							// The step size for the interpolation points...
							INT InterpStepSize = TerrainMaxTessLevel / (TessIndex * 2);

							// Heights for gradient calculations
							UBOOL bMidX = ((TrueX % PackStepSize) != 0);
							UBOOL bMidY = ((TrueY % PackStepSize) != 0);

							if (bMidX && bMidY)
							{
								// Center square...
								//@todo.SAS. If at 'highest' detail - should respect the orientation flag here!
								Z0 = Terrain->Height(TrueX - InterpStepSize, TrueY - InterpStepSize);
								Z1 = Terrain->Height(TrueX + InterpStepSize, TrueY + InterpStepSize);
							}
							else
							if (bMidX)
							{
								// Horizontal bar...
								Z0 = Terrain->Height(TrueX - InterpStepSize, TrueY);
								Z1 = Terrain->Height(TrueX + InterpStepSize, TrueY);
							}
							else
							if (bMidY)
							{
								// Vertical bar...
								Z0 = Terrain->Height(TrueX, TrueY - InterpStepSize);
								Z1 = Terrain->Height(TrueX, TrueY + InterpStepSize);
							}
							else
							{
								// True point on the grid...
								Z0 = Terrain->Height(TrueX, TrueY);
								Z1 = Terrain->Height(TrueX, TrueY);
							}

							INT TempZ = Z0 + Z1;
							ZHeight = (WORD)(TempZ / 2);

							if (MorphingFlags == ETMORPH_Full)
							{
								FTerrainFullMorphingVertex* DestFullMorphVert = (FTerrainFullMorphingVertex*)Buffer;
								// Heights for gradient calculations
								FLOAT HXN, HXP, HYN, HYP;

								UBOOL bMidXMorph = ((TrueX % PackStepSize) != 0);
								UBOOL bMidYMorph = ((TrueY % PackStepSize) != 0);

								if (bMidXMorph && bMidYMorph)
								{
									// Center square...
									//@todo.SAS. If at 'highest' detail - should respect the orientation flag here!
									HXN = (Terrain->Height(TrueX - InterpStepSize, TrueY - InterpStepSize) + Terrain->Height(TrueX - InterpStepSize, TrueY + InterpStepSize)) / 2;
									HXP = (Terrain->Height(TrueX + InterpStepSize, TrueY - InterpStepSize) + Terrain->Height(TrueX + InterpStepSize, TrueY + InterpStepSize)) / 2;
									HYN = (Terrain->Height(TrueX - InterpStepSize, TrueY - InterpStepSize) + Terrain->Height(TrueX + InterpStepSize, TrueY - InterpStepSize)) / 2;
									HYP = (Terrain->Height(TrueX - InterpStepSize, TrueY + InterpStepSize) + Terrain->Height(TrueX + InterpStepSize, TrueY + InterpStepSize)) / 2;
								}
								else
								if (bMidXMorph)
								{
									// Horizontal bar...
									HXN = Terrain->Height(TrueX - InterpStepSize, TrueY);
									HXP = Terrain->Height(TrueX + InterpStepSize, TrueY);
									HYN = (Terrain->Height(TrueX + InterpStepSize, TrueY) + Terrain->Height(TrueX - InterpStepSize, TrueY - PackStepSize)) / 2;
									HYP = (Terrain->Height(TrueX - InterpStepSize, TrueY) + Terrain->Height(TrueX + InterpStepSize, TrueY + PackStepSize)) / 2;
								}
								else
								if (bMidYMorph)
								{
									// Vertical bar...
									HXN = (Terrain->Height(TrueX - PackStepSize, TrueY - InterpStepSize) + Terrain->Height(TrueX, TrueY + InterpStepSize)) / 2;
									HXP = (Terrain->Height(TrueX, TrueY - InterpStepSize) + Terrain->Height(TrueX + PackStepSize, TrueY + InterpStepSize)) / 2;
									HYN = Terrain->Height(TrueX, TrueY - InterpStepSize);
									HYP = Terrain->Height(TrueX, TrueY + InterpStepSize);
								}
								else
								{
									// True point on the grid...
									HXN = Terrain->Height(TrueX - PackStepSize, TrueY);
									HXP = Terrain->Height(TrueX + PackStepSize, TrueY);
									HYN = Terrain->Height(TrueX, TrueY - PackStepSize);
									HYP = Terrain->Height(TrueX, TrueY + PackStepSize);
								}
								DestFullMorphVert->TransGradientX = (SWORD)appTrunc((HXP - HXN) / (FLOAT)InterpStepSize / 2.0f * ScaleX);
								DestFullMorphVert->TransGradientY = (SWORD)appTrunc((HYP - HYN) / (FLOAT)InterpStepSize / 2.0f * ScaleY);
							}
						}
						DestMorphVert->Z_TRANS_LOBYTE = ZHeight & 255;
						DestMorphVert->Z_TRANS_HIBYTE = ZHeight >> 8;
					}

					Buffer += Stride;
					PackedCount++;
				}
			}
		}
	}

	check(PackedCount == VertexCount);

#if !CONSOLE
	// Lock the real buffer.
	BYTE* ActualBuffer = (BYTE*)(RHILockVertexBuffer(VertexBufferRHI, 0, Size, FALSE));
	appMemcpy(ActualBuffer,OriginalBuffer,Size);
	appFree(OriginalBuffer);
#endif
	// Unlock the buffer.
	RHIUnlockVertexBuffer(VertexBufferRHI);

	CurrentTessellation = TessellationLevel;

	bRepackRequired = FALSE;

	return TRUE;
}

// FTerrainTessellationIndexBuffer
template<typename TerrainQuadRelevance>
struct FTerrainTessellationIndexBuffer: FIndexBuffer
{
	/** Type used to query terrain quad relevance. */
	typedef TerrainQuadRelevance QuadRelevanceType;
	QuadRelevanceType*	QRT;

	/** Terrain object that owns this buffer		*/
	FTerrainObject*		TerrainObject;
	/** The Max tessellation level required			*/
	INT					MaxTesselationLevel;
	/** The current tessellation level packed		*/
	INT					CurrentTessellationLevel;
	/** Total number of triangles packed			*/
	INT					NumTriangles;
	/** Max size of the allocated buffer			*/
	INT					MaxSize;
	/** Flag indicating that data must be repacked	*/
	UBOOL				RepackRequired;
	/** If TRUE, will not be filled at creation		*/
	UBOOL				bDeferredFillData;
	INT					VertexColumnStride;
	INT					VertexRowStride;
	/** Flag indicating it is dynamic				*/
	UBOOL				bIsDynamic;
	/** Flag indicating it is for morphing terrain	*/
	UBOOL				bIsMorphing;

	// Constructor.
protected:
	FTerrainTessellationIndexBuffer(FTerrainObject* InTerrainObject,UINT InMaxTesselationLevel,UBOOL bInDeferredFillData, UBOOL bInIsDynamic = TRUE)
		: QRT(NULL)
		, TerrainObject(InTerrainObject)
		, MaxTesselationLevel(InMaxTesselationLevel)
		, NumTriangles(0)
		, RepackRequired(bInIsDynamic)
		, bDeferredFillData(bInDeferredFillData)
		, bIsDynamic(bInIsDynamic)
		, bIsMorphing(FALSE)
	{
		SetCurrentTessellationLevel(InMaxTesselationLevel);

		if (InTerrainObject)
		{
			if (InTerrainObject->TerrainComponent)
			{
				ATerrain* Terrain = InTerrainObject->TerrainComponent->GetTerrain();
				if (Terrain)
				{
					bIsMorphing = Terrain->bMorphingEnabled;
				}
			}
		}
	}

public:
	virtual ~FTerrainTessellationIndexBuffer()
	{
		delete QRT;
	}

	// RenderResource interface
	/**
	 *	InitRHI
	 *	Initialize the render hardware interface for the index buffer.
	 */
	virtual void InitRHI()
	{
		if (bIsDynamic == TRUE)
		{
			return;
		}

		SCOPE_CYCLE_COUNTER(STAT_TerrainSmoothTime);

		DetermineMaxSize();

		if (MaxSize > 0)
		{
			INT Stride = sizeof(WORD);
			IndexBufferRHI = RHICreateIndexBuffer(Stride, MaxSize, NULL, RUF_Static|RUF_WriteOnly);
			if (bDeferredFillData == FALSE)
			{
				PrimeBuffer();
				FillData();
			}
		}
		else
		{
			NumTriangles = 0;
		}
	}

	/**
	 *	InitDynamicRHI
	 *	Initialize the render hardware interface for the dynamic index buffer.
	 */
	virtual void InitDynamicRHI()
	{
		if (bIsDynamic == FALSE)
		{
			return;
		}

		check(TerrainObject);
		check(TerrainObject->TerrainComponent);
		check(TerrainObject->TerrainComponent->GetOuter());
		check(TerrainObject->TerrainComponent->GetTerrain());
		check(TerrainObject->TerrainComponent->GetOwner());

		SCOPE_CYCLE_COUNTER(STAT_TerrainSmoothTime);

		DetermineMaxSize();

		if (MaxSize > 0)
		{
			INT Stride = sizeof(WORD);
			IndexBufferRHI = RHICreateIndexBuffer(Stride, MaxSize, NULL, RUF_Dynamic|RUF_WriteOnly);
			if (bDeferredFillData == FALSE)
			{
				PrimeBuffer();
				FillData();
			}
		}
	}

	/**
	 *	ReleaseDynamicRHI
	 *	Release the dynamic resource
	 */
	virtual void ReleaseDynamicRHI()
	{
		if (bIsDynamic == FALSE)
		{
			return;
		}

		if (IsValidRef(IndexBufferRHI) == TRUE)
		{
			IndexBufferRHI.SafeRelease();
			RepackRequired = TRUE;
		}
	}

	/**
	 *	SetMaxTesselationLevel
	 *	Sets the maximum tessellation level for the index buffer.
	 *
	 *	@param	InMaxTesselationLevel	The max level to set it to.
	 */
	inline void SetMaxTesselationLevel(INT InMaxTesselationLevel)
	{
		MaxTesselationLevel	= InMaxTesselationLevel;
	}

	/**
	*	GetMaxTesselationLevel
	*	Returns the maximum tessellation level for the index buffer.
	*
	*	@return	MaxTesselationLevel		The max level.
	*/
	inline INT GetMaxTesselationLevel()
	{
		return MaxTesselationLevel;
	}

	/**
	 *	SetCurrentTessellationLevel
	 *	Sets the current tessellation level for the index buffer.
	 *	This is the tessellation level that the indices are packed for.
	 *
	 *	@param	InCurrentTesselationLevel	The max level to set it to.
	 */
	inline void SetCurrentTessellationLevel(INT InCurrentTessellationLevel)
	{
		check(TerrainObject);
		check(TerrainObject->TerrainComponent);

        CurrentTessellationLevel	= InCurrentTessellationLevel;
		VertexColumnStride			= Square(CurrentTessellationLevel);
		VertexRowStride				= 
			(TerrainObject->TerrainComponent->SectionSizeX * VertexColumnStride) + 
			CurrentTessellationLevel;
	}

	/**
	 *	GetCachedTesselationLevel
	 *	Retrieves the tessellation level for the patch at the given X,Y
	 *
	 *	@param	X,Y		The indices of the component of interest
	 *
	 *	@return	UINT	The tessellation level cached
	 */
	FORCEINLINE INT GetCachedTesselationLevel(INT X,INT Y) const
	{
		return TerrainObject->GetTessellationLevel(
			(Y + 1) * (TerrainObject->TerrainComponent->SectionSizeX + 2) + (X + 1));
	}

	// GetVertexIndex
	inline WORD GetVertexIndex(INT PatchX,INT PatchY,INT InteriorX,INT InteriorY) const
	{
		if (InteriorX >= CurrentTessellationLevel)
		{
			return GetVertexIndex(PatchX + 1,PatchY,InteriorX - CurrentTessellationLevel,InteriorY);
		}
		if (InteriorY >= CurrentTessellationLevel)
		{
			return GetVertexIndex(PatchX,PatchY + 1,InteriorX,InteriorY - CurrentTessellationLevel);
		}
		return ((PatchY * VertexRowStride) + 
			(PatchX * ((PatchY < TerrainObject->TerrainComponent->SectionSizeY) ? VertexColumnStride : CurrentTessellationLevel))) + 
			(InteriorY * ((PatchX < TerrainObject->TerrainComponent->SectionSizeX) ? CurrentTessellationLevel : 1)) + 
			InteriorX;
	}

	void DetermineMaxSize()
	{
		check(TerrainObject);
		check(TerrainObject->TerrainComponent);

		// Determine the number of triangles at this tessellation level.
		INT TriCount = 0;
		INT LocalTessellationLevel = MaxTesselationLevel;
		INT StepSizeX = TerrainObject->TerrainComponent->TrueSectionSizeX / TerrainObject->TerrainComponent->SectionSizeX;
		INT StepSizeY = TerrainObject->TerrainComponent->TrueSectionSizeY / TerrainObject->TerrainComponent->SectionSizeY;
		for (INT Y = 0; Y < TerrainObject->TerrainComponent->SectionSizeY; Y++)
		{
			for (INT X = 0; X < TerrainObject->TerrainComponent->SectionSizeX; X++)
			{
				// If we are in the editor, allocate as though ALL patches could be visible.
				// This will allow for editing visibility without deleting/recreating index buffers while painting.
				if (GIsGame == TRUE)
				{
					if (QRT->IsQuadRelevant(
						TerrainObject->TerrainComponent->SectionBaseX + X * StepSizeX,
						TerrainObject->TerrainComponent->SectionBaseY + Y * StepSizeY) == FALSE)
					{
						continue;
					}
				}

				TriCount += 2 * Square<INT>(LocalTessellationLevel - 2); // Interior triangles.
				for (UINT EdgeComponent = 0; EdgeComponent < 2; EdgeComponent++)
				{
					for (UINT EdgeSign = 0; EdgeSign < 2; EdgeSign++)
					{
						TriCount += LocalTessellationLevel - 2 + LocalTessellationLevel;
					}
				}
			}
		}

		MaxSize = TriCount * 3 * sizeof(WORD);
	}

	void PrimeBuffer()
	{
		SCOPE_CYCLE_COUNTER(STAT_TerrainSmoothTime);

		// Determine the number of triangles at this tesselation level.
		NumTriangles = DetermineTriangleCount();
	}

	// Determine triangle count
	INT DetermineTriangleCount() const
	{
		INT TempNumTriangles = 0;
		if (TerrainObject != NULL)
		{
			// Determine the number of triangles at this tesselation level.
			INT StepSizeX = TerrainObject->TerrainComponent->TrueSectionSizeX / TerrainObject->TerrainComponent->SectionSizeX;
			INT StepSizeY = TerrainObject->TerrainComponent->TrueSectionSizeY / TerrainObject->TerrainComponent->SectionSizeY;
			for (INT Y = 0; Y < TerrainObject->TerrainComponent->SectionSizeY; Y++)
			{
				for (INT X = 0; X < TerrainObject->TerrainComponent->SectionSizeX; X++)
				{
					if (QRT->IsQuadRelevant(
						TerrainObject->TerrainComponent->SectionBaseX + X * StepSizeX,
						TerrainObject->TerrainComponent->SectionBaseY + Y * StepSizeY) == FALSE)
					{
						continue;
					}

					INT TesselationLevel = GetCachedTesselationLevel(X,Y);
					TempNumTriangles += 2 * Square(TesselationLevel - 2); // Interior triangles.

					for (UINT EdgeComponent = 0; EdgeComponent < 2; EdgeComponent++)
					{
						for (UINT EdgeSign = 0; EdgeSign < 2; EdgeSign++)
						{
							TempNumTriangles += 
								TesselationLevel - 2 + 
								Min(TesselationLevel, 
									GetCachedTesselationLevel(
										X + (EdgeComponent == 0 ? (EdgeSign ? 1 : -1) : 0),
										Y + (EdgeComponent == 1 ? (EdgeSign ? 1 : -1) : 0)
										)
									);
						}
					}
				}
			}
		}
		return TempNumTriangles;
	}

	// TesselateEdge
	INT TesselateEdge(WORD*& DestIndex, UINT& CurrentOffset, UINT EdgeTesselation, UINT TesselationLevel,
		UINT X, UINT Y, UINT EdgeX, UINT EdgeY, UINT EdgeInnerX, UINT EdgeInnerY, UINT InnerX, UINT InnerY,
		UINT DeltaX, UINT DeltaY, UINT VertexOrder)
	{
		INT PackedCount = 0;
		check(EdgeTesselation <= TesselationLevel);

		UINT	EdgeVertices[TERRAIN_MAXTESSELATION + 1];
		UINT	InnerVertices[TERRAIN_MAXTESSELATION - 1];

		for (UINT VertexIndex = 0;VertexIndex <= EdgeTesselation;VertexIndex++)
		{
			EdgeVertices[VertexIndex] = GetVertexIndex(
				EdgeX,
				EdgeY,
				EdgeInnerX + VertexIndex * DeltaX * CurrentTessellationLevel / EdgeTesselation,
				EdgeInnerY + VertexIndex * DeltaY * CurrentTessellationLevel / EdgeTesselation
				);
		}
		for (UINT VertexIndex = 1;VertexIndex <= TesselationLevel - 1;VertexIndex++)
		{
			InnerVertices[VertexIndex - 1] = GetVertexIndex(
				X,
				Y,
				InnerX + (VertexIndex - 1) * DeltaX * CurrentTessellationLevel / TesselationLevel,
				InnerY + (VertexIndex - 1) * DeltaY * CurrentTessellationLevel / TesselationLevel
				);
		}
		UINT	EdgeVertexIndex = 0,
				InnerVertexIndex = 0;
		while(EdgeVertexIndex < EdgeTesselation || InnerVertexIndex < (TesselationLevel - 2))
		{
			UINT	EdgePercent = EdgeVertexIndex * (TesselationLevel - 1),
					InnerPercent = (InnerVertexIndex + 1) * EdgeTesselation;

			if (EdgePercent < InnerPercent)
			{
				check(EdgeVertexIndex < EdgeTesselation);
				EdgeVertexIndex++;
				*DestIndex++ = EdgeVertices[EdgeVertexIndex - (1 - VertexOrder)];
				PackedCount++;
				*DestIndex++ = EdgeVertices[EdgeVertexIndex - VertexOrder];
				PackedCount++;
				*DestIndex++ = InnerVertices[InnerVertexIndex];
				PackedCount++;

				CurrentOffset += 3;
			}
			else
			{
				check(InnerVertexIndex < TesselationLevel - 2);
				InnerVertexIndex++;
				*DestIndex++ = InnerVertices[InnerVertexIndex - VertexOrder];
				PackedCount++;
				*DestIndex++ = InnerVertices[InnerVertexIndex - (1 - VertexOrder)];
				PackedCount++;
				*DestIndex++ = EdgeVertices[EdgeVertexIndex];
				PackedCount++;

				CurrentOffset += 3;
			}
		};

		return PackedCount;
	}

	// FIndexBuffer interface.
	virtual void FillData()
	{
		if (NumTriangles <= 0)
		{
			return;
		}

		check(TerrainObject);
		check(TerrainObject->TerrainComponent);

		SCOPE_CYCLE_COUNTER(STAT_TerrainSmoothTime);

		INT Stride = sizeof(WORD);
		INT Size = NumTriangles * 3 * Stride;

		check(Size <= MaxSize);

		// Lock the buffer.
		void* Buffer = RHILockIndexBuffer(IndexBufferRHI, 0, Size);
		check(Buffer);

		//
		WORD*	DestIndex		= (WORD*)Buffer;
		UINT	CurrentOffset	= 0;

		ATerrain* LocalTerrain = CastChecked<ATerrain>(TerrainObject->TerrainComponent->GetOwner());
		INT StepSizeX = TerrainObject->TerrainComponent->TrueSectionSizeX / TerrainObject->TerrainComponent->SectionSizeX;
		INT StepSizeY = TerrainObject->TerrainComponent->TrueSectionSizeY / TerrainObject->TerrainComponent->SectionSizeY;
		for (INT Y = 0;Y < TerrainObject->TerrainComponent->SectionSizeY;Y++)
		{
			for (INT X = 0;X < TerrainObject->TerrainComponent->SectionSizeX;X++)
			{
				if (QRT->IsQuadRelevant(
					TerrainObject->TerrainComponent->SectionBaseX + X * StepSizeX,
					TerrainObject->TerrainComponent->SectionBaseY + Y * StepSizeY) == FALSE)
				{
					continue;
				}

				INT	TesselationLevel;
				INT	EdgeTesselationNegX;
				INT	EdgeTesselationPosX;
				INT	EdgeTesselationNegY;
				INT	EdgeTesselationPosY;
				if (TerrainObject->MinTessellationLevel == TerrainObject->MaxTessellationLevel)
				{
					TesselationLevel = TerrainObject->MaxTessellationLevel;
					EdgeTesselationNegX = TesselationLevel;
					EdgeTesselationPosX = TesselationLevel;
					EdgeTesselationNegY = TesselationLevel;
					EdgeTesselationPosY = TesselationLevel;
				}
				else
				{
					TesselationLevel = 
							Max<INT>(
								GetCachedTesselationLevel(X,Y),
								TerrainObject->MinTessellationLevel
								);
					EdgeTesselationNegX = 
							Max<INT>(
								Min(TesselationLevel,GetCachedTesselationLevel(X - 1,Y)),
								TerrainObject->MinTessellationLevel
								);
					EdgeTesselationPosX = 
							Max<INT>(
								Min(TesselationLevel,GetCachedTesselationLevel(X + 1,Y)),
								TerrainObject->MinTessellationLevel
								);
					EdgeTesselationNegY = 
							Max<INT>(
								Min(TesselationLevel,GetCachedTesselationLevel(X,Y - 1)),
								TerrainObject->MinTessellationLevel
								);
					EdgeTesselationPosY = 
							Max<INT>(
								Min(TesselationLevel,GetCachedTesselationLevel(X,Y + 1)),
								TerrainObject->MinTessellationLevel
								);
				}
				check(TesselationLevel > 0);

				if ((TesselationLevel == EdgeTesselationNegX)	 && 
					(EdgeTesselationNegX == EdgeTesselationPosX) && 
					(EdgeTesselationPosX == EdgeTesselationNegY) && 
					(EdgeTesselationNegY == EdgeTesselationPosY))
				{
					INT	TesselationFactor = CurrentTessellationLevel / TesselationLevel;

					WORD	IndexCache[2][TERRAIN_MAXTESSELATION + 1];
					INT		NextCacheLine = 1;
					INT		CurrentVertexIndex = GetVertexIndex(X,Y,0,0);

					for (INT SubX = 0; SubX < TesselationLevel; SubX++, CurrentVertexIndex += TesselationFactor)
					{
						IndexCache[0][SubX] = CurrentVertexIndex;
					}

					IndexCache[0][TesselationLevel] = GetVertexIndex(X + 1,Y,0,0);

					for (INT SubY = 0; SubY < TesselationLevel; SubY++)
					{
						CurrentVertexIndex = GetVertexIndex(X,Y,0,(SubY + 1) * TesselationFactor);

						for (INT SubX = 0; SubX < TesselationLevel; SubX++, CurrentVertexIndex += TesselationFactor)
						{
							IndexCache[NextCacheLine][SubX] = CurrentVertexIndex;
						}

						IndexCache[NextCacheLine][TesselationLevel] = GetVertexIndex(X + 1,Y,0,(SubY + 1) * TesselationFactor);

						for (INT SubX = 0; SubX < TesselationLevel; SubX++)
						{
							WORD	V00 = IndexCache[1 - NextCacheLine][SubX],
									V10 = IndexCache[1 - NextCacheLine][SubX + 1],
									V01 = IndexCache[NextCacheLine][SubX],
									V11 = IndexCache[NextCacheLine][SubX + 1];

							UBOOL bQuadFlipped = FALSE;
							if (CurrentTessellationLevel == LocalTerrain->MaxTesselationLevel)
							{
								bQuadFlipped = LocalTerrain->IsTerrainQuadFlipped(
									TerrainObject->TerrainComponent->SectionBaseX + X * StepSizeX + SubX,
									TerrainObject->TerrainComponent->SectionBaseY + Y * StepSizeY + SubY);
							}


							if (bQuadFlipped == FALSE)
							{
								*DestIndex++ = V00;
								*DestIndex++ = V01;
								*DestIndex++ = V11;

								*DestIndex++ = V00;
								*DestIndex++ = V11;
								*DestIndex++ = V10;
							}
							else
							{
								*DestIndex++ = V00;
								*DestIndex++ = V01;
								*DestIndex++ = V10;

								*DestIndex++ = V10;
								*DestIndex++ = V01;
								*DestIndex++ = V11;
							}
						}

						NextCacheLine = 1 - NextCacheLine;
					}
				}
				else
				{
					INT	TesselationFactor = CurrentTessellationLevel / TesselationLevel;

					// Interior triangles.

					for (INT SubX = 1; SubX < TesselationLevel - 1; SubX++)
					{
						for (INT SubY = 1; SubY < TesselationLevel - 1; SubY++)
						{
							WORD	V00 = GetVertexIndex(X,Y,SubX * TesselationFactor,SubY * TesselationFactor),
									V10 = GetVertexIndex(X,Y,(SubX + 1) * TesselationFactor,SubY * TesselationFactor),
									V01 = GetVertexIndex(X,Y,SubX * TesselationFactor,(SubY + 1) * TesselationFactor),
									V11 = GetVertexIndex(X,Y,(SubX + 1) * TesselationFactor,(SubY + 1) * TesselationFactor);

							*DestIndex++ = V00;
							*DestIndex++ = V01;
							*DestIndex++ = V11;

							*DestIndex++ = V00;
							*DestIndex++ = V11;
							*DestIndex++ = V10;
						}
					}

					// Edges.
					TesselateEdge(DestIndex,CurrentOffset,EdgeTesselationNegX,TesselationLevel,X,Y,X,Y,0,0,CurrentTessellationLevel / TesselationLevel,CurrentTessellationLevel / TesselationLevel,0,1,0);
					TesselateEdge(DestIndex,CurrentOffset,EdgeTesselationPosX,TesselationLevel,X,Y,X + 1,Y,0,0,CurrentTessellationLevel - CurrentTessellationLevel / TesselationLevel,CurrentTessellationLevel / TesselationLevel,0,1,1);
					TesselateEdge(DestIndex,CurrentOffset,EdgeTesselationNegY,TesselationLevel,X,Y,X,Y,0,0,CurrentTessellationLevel / TesselationLevel,CurrentTessellationLevel / TesselationLevel,1,0,1);
					TesselateEdge(DestIndex,CurrentOffset,EdgeTesselationPosY,TesselationLevel,X,Y,X,Y + 1,0,0,CurrentTessellationLevel / TesselationLevel,CurrentTessellationLevel - CurrentTessellationLevel / TesselationLevel,1,0,0);
				}
			}
		}

		// Unlock the buffer.
		RHIUnlockIndexBuffer(IndexBufferRHI);
		RepackRequired = FALSE;
	}
};

/** Considers a quad to be relevant if visible. */
class FTerrainQuadRelevance_IsVisible
{
public:
	FTerrainQuadRelevance_IsVisible(FTerrainObject* InTerrainObject)
		: LocalTerrain( InTerrainObject->GetTerrain() )
	{}

	FORCEINLINE UBOOL IsQuadRelevant(INT X, INT Y) const
	{
		return LocalTerrain->IsTerrainQuadVisible( X, Y );
	}

private:
	const ATerrain* LocalTerrain;
};

/** Considers a quad to be relevant if visible and falling within a specified interval. */
class FTerrainQuadRelevance_IsInInterval : public FTerrainQuadRelevance_IsVisible
{
public:
	FTerrainQuadRelevance_IsInInterval(FTerrainObject* InTerrainObject, INT InMinPatchX, INT InMinPatchY, INT InMaxPatchX, INT InMaxPatchY)
		: FTerrainQuadRelevance_IsVisible( InTerrainObject )
		, MinPatchX( InMinPatchX )
		, MinPatchY( InMinPatchY )
		, MaxPatchX( InMaxPatchX )
		, MaxPatchY( InMaxPatchY )
	{}

	FORCEINLINE UBOOL IsQuadRelevant(INT X, INT Y) const
	{
		if ( FTerrainQuadRelevance_IsVisible::IsQuadRelevant( X, Y ) )
		{
			if ( X >= MinPatchX && X < MaxPatchX && Y >= MinPatchY && Y < MaxPatchY )
			{
				return TRUE;
			}
		}
		return FALSE;
	}

private:
	INT MinPatchX;
	INT MinPatchY;
	INT MaxPatchX;
	INT MaxPatchY;
};

/** Index buffer type for terrain, which use the IsVisible quad relevance function. */
struct TerrainTessellationIndexBufferType : public FTerrainTessellationIndexBuffer<FTerrainQuadRelevance_IsVisible>
{
public:
	typedef FTerrainTessellationIndexBuffer<FTerrainQuadRelevance_IsVisible> Super;
	TerrainTessellationIndexBufferType(FTerrainObject* InTerrainObject,UINT InMaxTesselationLevel,UBOOL bInDeferredFillData, UBOOL bInIsDynamic = TRUE)
		: Super( InTerrainObject, InMaxTesselationLevel, bInDeferredFillData, bInIsDynamic )
	{
		QRT = new FTerrainQuadRelevance_IsVisible( TerrainObject );
	}
};

/** Index buffer type for terrain decals, which use the IsInInterval quad relevance function. */
struct TerrainDecalTessellationIndexBufferType : public FTerrainTessellationIndexBuffer<FTerrainQuadRelevance_IsInInterval>
{
public:
	typedef FTerrainTessellationIndexBuffer<FTerrainQuadRelevance_IsInInterval> Super;
	TerrainDecalTessellationIndexBufferType(INT MinPatchX, INT MinPatchY, INT MaxPatchX, INT MaxPatchY, FTerrainObject* InTerrainObject,UINT InMaxTesselationLevel,UBOOL bInDeferredFillData, UBOOL bInIsDynamic = TRUE)
		: Super( InTerrainObject, InMaxTesselationLevel, bInDeferredFillData, bInIsDynamic )
	{
		QRT = new FTerrainQuadRelevance_IsInInterval( TerrainObject, MinPatchX, MinPatchY, MaxPatchX, MaxPatchY );
	}
};

static INT AddUniqueMask(TArray<FTerrainMaterialMask>& Array,const FTerrainMaterialMask& Mask)
{
	INT	Index = Array.FindItemIndex(Mask);
	if(Index == INDEX_NONE)
	{
		Index = Array.Num();
		new(Array) FTerrainMaterialMask(Mask);
	}
	return Index;
}

/** builds/updates a list of unique blended material combinations used by quads in this terrain section and puts them in the BatchMaterials array,
 * then fills PatchBatches with the index from that array that should be used for each patch. Also updates FullBatch with the index of the full mask.
 */
void UTerrainComponent::UpdatePatchBatches()
{
	ATerrain* Terrain = GetTerrain();
	FTerrainMaterialMask FullMask(Terrain->WeightedMaterials.Num());
	check(Terrain->WeightedMaterials.Num()<=64);

	BatchMaterials.Empty();

	for (INT Y = SectionBaseY; Y < SectionBaseY + TrueSectionSizeY; Y++)
	{
		for (INT X = SectionBaseX; X < SectionBaseX + TrueSectionSizeX; X++)
		{
			FTerrainMaterialMask	Mask(Terrain->WeightedMaterials.Num());

			for (INT MaterialIndex = 0; MaterialIndex < Terrain->WeightedMaterials.Num(); MaterialIndex++)
			{
				FTerrainWeightedMaterial& WeightedMaterial = Terrain->WeightedMaterials(MaterialIndex);
				UINT TotalWeight =	(UINT)WeightedMaterial.Weight(X + 0,Y + 0) +
									(UINT)WeightedMaterial.Weight(X + 1,Y + 0) +
									(UINT)WeightedMaterial.Weight(X + 0,Y + 1) +
									(UINT)WeightedMaterial.Weight(X + 1,Y + 1);
				Mask.Set(MaterialIndex, Mask.Get(MaterialIndex) || TotalWeight > 0);
				FullMask.Set(MaterialIndex, FullMask.Get(MaterialIndex) || TotalWeight > 0);
			}
		}
	}

	FullBatch = AddUniqueMask(BatchMaterials, FullMask);
}

void UTerrainComponent::Attach()
{
	ATerrain* Terrain = Cast<ATerrain>(GetOwner());
	check(Terrain);

	if (DetachFence.GetNumPendingFences() > 0)
	{
		FlushRenderingCommands();
		if (DetachFence.GetNumPendingFences() > 0)
		{
			debugf(TEXT("TerrainComponent::Attach> Still have DetachFence pending???"));
		}
	}

	UpdatePatchBatches();

	UINT NumPatches	= TrueSectionSizeX * TrueSectionSizeY;

	TerrainObject = ::new FTerrainObject(this, Terrain->MaxTesselationLevel);
	check(TerrainObject);

	TerrainObject->InitResources();

	Super::Attach();	

	// update all decals when attaching terrain
	if( GIsEditor && !GIsPlayInEditorWorld )
	{
		if( bAcceptsDynamicDecals || bAcceptsStaticDecals )
		{
			GEngine->IssueDecalUpdateRequest();
		}
	}
}

void UTerrainComponent::UpdateTransform()
{
	Super::UpdateTransform();

#if GEMINI_TODO
	GResourceManager->UpdateResource(&TerrainObject->VertexBuffer);
#endif
}

void UTerrainComponent::Detach( UBOOL bWillReattach )
{
	if (GIsEditor == TRUE)
	{
		FlushRenderingCommands();
	}

	// Take care of the TerrainObject for this component
	if (TerrainObject)
	{
		// Begin releasing the RHI resources used by this skeletal mesh component.
		// This doesn't immediately destroy anything, since the rendering thread may still be using the resources.
		TerrainObject->ReleaseResources();

		// Begin a deferred delete of TerrainObject.  BeginCleanup will call TerrainObject->FinishDestroy after the above release resource
		// commands execute in the rendering thread.
		BeginCleanup(TerrainObject);

		TerrainObject = NULL;
	}

	Super::Detach( bWillReattach );
}

INT UTerrainComponent::GetResourceSize()
{
	if (GExclusiveResourceSizeMode)
	{
		return 0;
	}

	FArchiveCountMem CountBytesSize(this);
	INT ResourceSize = CountBytesSize.GetNum();
	return ResourceSize;
}

//
//	FTerrainObject
//
FTerrainObject::~FTerrainObject()
{
	appFree(TessellationLevels);
	delete SmoothIndexBuffer;
	SmoothIndexBuffer = NULL;
	delete VertexBuffer;
	VertexBuffer = NULL;
	delete VertexFactory;
	VertexFactory = NULL;
	delete DecalVertexFactory;
	DecalVertexFactory = NULL;
}

/** Clamps the value downwards to the nearest interval. */
static FORCEINLINE void MinClampToInterval(INT& Val, INT Interval)
{
	Val -= (Val % Interval);
}

/** Clamps the value upwards to the nearest interval. */
static FORCEINLINE void MaxClampToInterval(INT& Val, INT Interval)
{
	if ( (Val % Interval) > 0 )
	{
		Val += Interval-(Val % Interval);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	FDecalTerrainInteraction
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** An association between a decal and a terrain component. */
class FDecalTerrainInteraction : public FReceiverResource
{
public:
	FDecalTerrainInteraction()
		: DecalComponent( NULL )
		, SmoothIndexBuffer( NULL )
	{}
	FDecalTerrainInteraction(const UDecalComponent* InDecalComponent, const UTerrainComponent* TerrainComponent, INT NumPatchesX, INT NumPatchesY, INT MaxTessellationLevel);
	virtual ~FDecalTerrainInteraction()
	{
		delete SmoothIndexBuffer;
	}

	void InitResources_RenderingThread(FTerrainObject* TerrainObject, INT MinTessellationLevel, INT MaxTessellationLevel);
	void RepackIndexBuffers_RenderingThread(FTerrainObject* TerrainObject, INT TessellationLevel, INT MaxTessellation);
	
	// FReceiverResource Interface
	virtual void OnRelease_RenderingThread()
	{
		if ( SmoothIndexBuffer )
		{
			SmoothIndexBuffer->ReleaseResource();
		}
	}
	/** 
	* @return memory usage in bytes for the receiver resource data for a given decal
	*/
	virtual INT GetMemoryUsage()
	{
		INT MemoryCount=0;
		if( SmoothIndexBuffer )
		{
			MemoryCount += SmoothIndexBuffer->DetermineTriangleCount() * sizeof(WORD);
		}
		return MemoryCount;
	}

	TerrainDecalTessellationIndexBufferType* GetSmoothIndexBuffer()
	{
		return SmoothIndexBuffer;
		//return IsInitialized() ? SmoothIndexBuffer : NULL;
	}

	INT GetMinPatchX() {	return MinPatchX;	}
	INT GetMinPatchY() {	return MinPatchY;	}
	INT GetMaxPatchX() {	return MaxPatchX;	}
	INT GetMaxPatchY() {	return MaxPatchY;	}
private:
	/** The decal component associated with this interaction. */
	const UDecalComponent* DecalComponent;
	/** The index buffer associated with this decal. */
	TerrainDecalTessellationIndexBufferType* SmoothIndexBuffer;
	/** Min/Max patch indices spanned by this decal. */
	INT MinPatchX;
	INT MinPatchY;
	INT MaxPatchX;
	INT MaxPatchY;

	/** If FALSE, the decal is not relevant to this terrain component. */
	UBOOL bRelevant;
};

FDecalTerrainInteraction::FDecalTerrainInteraction(const UDecalComponent* InDecalComponent,
												   const UTerrainComponent* TerrainComponent,
												   INT NumPatchesX,INT NumPatchesY,INT MaxTessellationLevel)
	: DecalComponent( InDecalComponent )
	, SmoothIndexBuffer( NULL )
	, bRelevant( FALSE )
{
	ATerrain* Terrain = TerrainComponent->GetTerrain();
	const FMatrix& TerrainWorldToLocal = Terrain->WorldToLocal();

	// Transform the decal frustum verts into local space and compute mins/maxs.
	FVector FrustumVerts[8];
	DecalComponent->GenerateDecalFrustumVerts( FrustumVerts );

	// Compute mins/maxs of local space frustum verts.
	FrustumVerts[0] = TerrainWorldToLocal.TransformFVector( FrustumVerts[0] );
	FLOAT MinX = FrustumVerts[0].X;
	FLOAT MinY = FrustumVerts[0].Y;
	FLOAT MaxX = FrustumVerts[0].X;
	FLOAT MaxY = FrustumVerts[0].Y;
	FLOAT MinZ = FrustumVerts[0].Z;
	FLOAT MaxZ = FrustumVerts[0].Z;
	for ( INT Index = 1 ; Index < 8 ; ++Index )
	{
		FrustumVerts[Index] = TerrainWorldToLocal.TransformFVector( FrustumVerts[Index] );
		MinX = Min( MinX, FrustumVerts[Index].X );
		MinY = Min( MinY, FrustumVerts[Index].Y );
		MinZ = Min( MinZ, FrustumVerts[Index].Z );
		MaxX = Max( MaxX, FrustumVerts[Index].X );
		MaxY = Max( MaxY, FrustumVerts[Index].Y );
		MaxZ = Max( MaxZ, FrustumVerts[Index].Z );
	}

	// Compute min/max patch indices.
	MinPatchX = Max( 0, appFloor(MinX) );
	MinPatchY = Max( 0, appFloor(MinY) );
	MaxPatchX = Min( TerrainComponent->SectionBaseX + TerrainComponent->TrueSectionSizeX, appCeil(MaxX) );
	MaxPatchY = Min( TerrainComponent->SectionBaseY + TerrainComponent->TrueSectionSizeY, appCeil(MaxY) );

	// @todo decal: update this check to take into account the component's patch base and stride.
	if ( MinPatchX != MaxPatchX && MinPatchY != MaxPatchY )
	{
		// Clamp decal-relevant patch indices to the highest tessellation interval.
		MinClampToInterval( MinPatchX, MaxTessellationLevel );
		MinClampToInterval( MinPatchY, MaxTessellationLevel );
		MaxClampToInterval( MaxPatchX, MaxTessellationLevel );
		MaxClampToInterval( MaxPatchY, MaxTessellationLevel );
		// Offset to terrain component base
		INT TempMinPatchX = MinPatchX - TerrainComponent->SectionBaseX;
		INT TempMaxPatchX = MaxPatchX - TerrainComponent->SectionBaseX;
		INT TempMinPatchY = MinPatchY - TerrainComponent->SectionBaseY;
		INT TempMaxPatchY = MaxPatchY - TerrainComponent->SectionBaseY;

		// Find the min/max height of all the intersecting terrain patches
		FLOAT MinHeight=WORLD_MAX, MaxHeight=-WORLD_MAX;
		for (INT SectionY=TempMinPatchY; SectionY < TempMaxPatchY; SectionY+=MaxTessellationLevel)
		{
			for (INT SectionX=TempMinPatchX; SectionX < TempMaxPatchX; SectionX+=MaxTessellationLevel)
			{
				const INT PatchIdx = SectionY/MaxTessellationLevel * TerrainComponent->SectionSizeX + SectionX/MaxTessellationLevel;
				if (TerrainComponent->PatchBounds.IsValidIndex(PatchIdx))
				{
					const FTerrainPatchBounds&	Patch = TerrainComponent->PatchBounds(PatchIdx);
					MinHeight = Min<FLOAT>(MinHeight,Patch.MinHeight);
					MaxHeight = Max<FLOAT>(MaxHeight,Patch.MaxHeight);
				}
			}
		}
		// See if the decal frustum intersects the terrain patch min/max height
		if ((MinZ >= MinHeight && MinZ <= MaxHeight) ||
			(MaxZ >= MinHeight && MaxZ <= MaxHeight) ||
			(MinHeight >= MinZ && MinHeight <= MaxZ) ||
			(MaxHeight >= MinZ && MaxHeight <= MaxZ))
		{
			bRelevant = TRUE;
		}
	}
	// Otherwise, the decal is hanging off the edge of the terrain and is thus not relevant.
}

void FDecalTerrainInteraction::InitResources_RenderingThread(FTerrainObject* TerrainObject, INT MinTessellationLevel, INT MaxTessellationLevel)
{
	// Don't init resources if the decal isn't relevant.
	if ( bRelevant )
	{
		// Create the tessellation index buffer
		check( !SmoothIndexBuffer );
#if CONSOLE
		SmoothIndexBuffer = ::new TerrainDecalTessellationIndexBufferType(MinPatchX, MinPatchY, MaxPatchX, MaxPatchY, TerrainObject, MaxTessellationLevel, FALSE, TRUE);
#else
		if ((GIsGame == TRUE) && (MinTessellationLevel == MaxTessellationLevel))
		{
			SmoothIndexBuffer = ::new TerrainDecalTessellationIndexBufferType(MinPatchX, MinPatchY, MaxPatchX, MaxPatchY, TerrainObject, MaxTessellationLevel, FALSE, FALSE);
		}
		else
		{
			SmoothIndexBuffer = ::new TerrainDecalTessellationIndexBufferType(MinPatchX, MinPatchY, MaxPatchX, MaxPatchY, TerrainObject, MaxTessellationLevel, FALSE);
		}
#endif
		checkSlow(SmoothIndexBuffer);
		SmoothIndexBuffer->InitResource();

		// Mark that the receiver resource is initialized.
		SetInitialized();
	}
}

void FDecalTerrainInteraction::RepackIndexBuffers_RenderingThread(FTerrainObject* TerrainObject, INT TessellationLevel, INT MaxTessellation)
{
	// Check the tessellation level of each index buffer vs. the cached tessellation level
	if (SmoothIndexBuffer && GIsRHIInitialized)
	{
		if (SmoothIndexBuffer->MaxTesselationLevel != MaxTessellation)
		{
			SmoothIndexBuffer->ReleaseResource();
			delete SmoothIndexBuffer;
#if CONSOLE
			SmoothIndexBuffer = ::new TerrainDecalTessellationIndexBufferType(MinPatchX, MinPatchY, MaxPatchX, MaxPatchY, TerrainObject, MaxTessellation, TRUE, TRUE);
#else
			SmoothIndexBuffer = ::new TerrainDecalTessellationIndexBufferType(MinPatchX, MinPatchY, MaxPatchX, MaxPatchY, TerrainObject, MaxTessellation, TRUE);
#endif
		}
		checkSlow(SmoothIndexBuffer);
		SmoothIndexBuffer->SetCurrentTessellationLevel(TessellationLevel);
		SmoothIndexBuffer->PrimeBuffer();
		if ((SmoothIndexBuffer->NumTriangles > 0) && (IsValidRef(SmoothIndexBuffer->IndexBufferRHI) == FALSE))
		{
			debugf(TEXT("INVALID TERRAIN DECAL INDEX BUFFER 0x%08x!"), SmoothIndexBuffer);
		}
		if (SmoothIndexBuffer->NumTriangles > 0)
		{
			SmoothIndexBuffer->FillData();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define PERF_SHOW_SLOW_ADD_DECAL_TERRAIN_INTERACTIONS 0
static const DOUBLE SLOW_ADD_DECAL_TERRAIN_INTERACTIONS_TIME = 0.5f;

/** Adds a decal interaction to the game object. */
void FTerrainObject::AddDecalInteraction_RenderingThread(FDecalInteraction& DecalInteraction, UINT ProxyMaxTesellation)
{
	checkSlow( IsInRenderingThread() );

#if LOOKING_FOR_PERF_ISSUES || PERF_SHOW_SLOW_ADD_DECAL_TERRAIN_INTERACTIONS
	const DOUBLE StartTime = appSeconds();
#endif // LOOKING_FOR_PERF_ISSUES || SHOW_SLOW_ADD_DECAL_INTERACTIONS

	FDecalTerrainInteraction* Decal = new FDecalTerrainInteraction(DecalInteraction.Decal,TerrainComponent,NumPatchesX,NumPatchesY,MaxTessellationLevel);
	Decal->InitResources_RenderingThread( this, MinTessellationLevel, MaxTessellationLevel );

	const UINT MaxTes = Max(ProxyMaxTesellation,(UINT)MinTessellationLevel);
	Decal->RepackIndexBuffers_RenderingThread( this, MaxTes, MaxTessellationLevel );

	DecalInteraction.RenderData->ReceiverResources.AddItem( Decal );
	if( Decal->GetSmoothIndexBuffer() )
	{
		DecalInteraction.RenderData->NumTriangles = Decal->GetSmoothIndexBuffer()->NumTriangles;
	}

#if LOOKING_FOR_PERF_ISSUES || PERF_SHOW_SLOW_ADD_DECAL_TERRAIN_INTERACTIONS
	const DOUBLE TimeSpent = (appSeconds() - StartTime) * 1000;
	if( TimeSpent > SLOW_ADD_DECAL_TERRAIN_INTERACTIONS_TIME )
	{
		warnf( NAME_DevDecals, TEXT("AddDecal to terrain took: %f"), TimeSpent );
	}
#endif // LOOKING_FOR_PERF_ISSUES || SHOW_SLOW_ADD_DECAL_INTERACTIONS
}

void FTerrainObject::GenerateDecalRenderData(class FDecalState* Decal, TArray< FDecalRenderData* >& OutDecalRenderDatas) const
{
	OutDecalRenderDatas.Reset();

	// create the new decal render data using the vertex factory from the terrain object
	FDecalRenderData* DecalRenderData = new FDecalRenderData( NULL, FALSE, FALSE, VertexFactory );

	// always need at least one triangle
	if ( DecalRenderData )
	{
		DecalRenderData->NumTriangles = 1;
		DecalRenderData->DecalBlendRange = Decal->DecalComponent->CalcDecalDotProductBlendRange();

		OutDecalRenderDatas.AddItem( DecalRenderData );
	}
}

// Called by FTerrainObject's ctor via the FTerrainObject allocation in UTerrainComponent::Attach
void FTerrainObject::Init()
{
	check(TerrainComponent);

	ATerrain* Terrain = TerrainComponent->GetTerrain();

	ComponentSectionSizeX		= TerrainComponent->SectionSizeX;
	ComponentSectionSizeY		= TerrainComponent->SectionSizeY;
	ComponentSectionBaseX		= TerrainComponent->SectionBaseX;
	ComponentSectionBaseY		= TerrainComponent->SectionBaseY;
	ComponentTrueSectionSizeX	= TerrainComponent->TrueSectionSizeX;
	ComponentTrueSectionSizeY	= TerrainComponent->TrueSectionSizeY;
	NumVerticesX				= Terrain->NumVerticesX;
	NumVerticesY				= Terrain->NumVerticesY;
	MaxTessellationLevel		= Terrain->MaxTesselationLevel;
	MinTessellationLevel		= Terrain->MinTessellationLevel;
	EditorTessellationLevel		= Terrain->EditorTessellationLevel;
	TerrainHeightScale			= TERRAIN_ZSCALE;
	TessellationDistanceScale	= Terrain->TesselationDistanceScale;
	LightMapResolution			= Terrain->StaticLightingResolution;
	NumPatchesX					= Terrain->NumPatchesX;
	NumPatchesY					= Terrain->NumPatchesY;

//	ShadowCoordinateScale		= ;
//	ShadowCoordinateBias		= ;

	INT TessellationLevelsCount		= (ComponentSectionSizeX + 2) * (ComponentSectionSizeY + 2);
	TessellationLevels = (BYTE*)appRealloc(TessellationLevels, TessellationLevelsCount);
	check(TessellationLevels);

	//
	if (GIsEditor && (Terrain->EditorTessellationLevel != 0))
	{
		if (Terrain->EditorTessellationLevel <= MaxTessellationLevel)
		{
			MaxTessellationLevel = Terrain->EditorTessellationLevel;
			MinTessellationLevel = Terrain->EditorTessellationLevel;
		}
	}

	// Prep the tessellation level of each terrain quad.
	INT Index = 0;
	for (INT Y = -1; Y <= ComponentSectionSizeY; Y++)
	{
		for (INT X = -1; X <= ComponentSectionSizeX; X++)
		{
			TessellationLevels[Index++] = MaxTessellationLevel;
		}
	}
}

// Called on the game thread by UTerrainComponent::Attach.
void FTerrainObject::InitResources()
{
	//// Decals

	//
	ATerrain* Terrain = TerrainComponent->GetTerrain();

	if ((GIsGame == TRUE) && (MinTessellationLevel == MaxTessellationLevel))
	{
		VertexBuffer = ::new FTerrainVertexBuffer(this, TerrainComponent, MaxTessellationLevel);
	}
	else
	{
		VertexBuffer = ::new FTerrainVertexBuffer(this, TerrainComponent, MaxTessellationLevel, TRUE);
	}
	check(VertexBuffer);
	BeginInitResource(VertexBuffer);

	if (Terrain->bMorphingEnabled == FALSE)
	{
		VertexFactory = ::new FTerrainVertexFactory();
	}
	else
	{
		if (Terrain->bMorphingGradientsEnabled == FALSE)
		{
			VertexFactory = ::new FTerrainMorphVertexFactory();
		}
		else
		{
			VertexFactory = ::new FTerrainFullMorphVertexFactory();
		}
	}
	check(VertexFactory);
	VertexFactory->SetTerrainObject(this);
	VertexFactory->SetTessellationLevel(MaxTessellationLevel);

	// update vertex factory components and sync it
	verify(VertexFactory->InitComponentStreams(VertexBuffer));

	// init rendering resource	
	BeginInitResource(VertexFactory);

	//// Decals
	if (Terrain->bMorphingEnabled == FALSE)
	{
		DecalVertexFactory = ::new FTerrainDecalVertexFactory();
	}
	else
	{
		if (Terrain->bMorphingGradientsEnabled == FALSE)
		{
			DecalVertexFactory = ::new FTerrainMorphDecalVertexFactory();
		}
		else
		{
			DecalVertexFactory = ::new FTerrainFullMorphDecalVertexFactory();
		}
	}
	check(DecalVertexFactory);
	FTerrainVertexFactory* TempVF = DecalVertexFactory->CastToFTerrainVertexFactory();
	TempVF->SetTerrainObject(this);
	TempVF->SetTessellationLevel(MaxTessellationLevel);

	// update vertex factory components and sync it
	verify(TempVF->InitComponentStreams(VertexBuffer));

	// init rendering resource	
	BeginInitResource(TempVF);
	//// Decals

	INT BatchMatCount = TerrainComponent->BatchMaterials.Num();

	// Create the tessellation index buffer
	check(TerrainComponent->GetTerrain());
#if CONSOLE
	SmoothIndexBuffer = ::new TerrainTessellationIndexBufferType(this, MaxTessellationLevel, FALSE, TRUE);
#else
	if ((GIsGame == TRUE) && (MinTessellationLevel == MaxTessellationLevel))
	{
		SmoothIndexBuffer = ::new TerrainTessellationIndexBufferType(this, MaxTessellationLevel, FALSE, FALSE);
	}
	else
	{
		SmoothIndexBuffer = ::new TerrainTessellationIndexBufferType(this, MaxTessellationLevel, FALSE);
	}
#endif
	check(SmoothIndexBuffer);
	BeginInitResource(SmoothIndexBuffer);

	// Initialize decal resources.
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		TerrainObjectReinitDecalResourcesCommand,
		FTerrainObject*, TerrainObject, this,
		{
			TerrainObject->ReinitDecalResources_RenderThread();
		}
	);
}

// Called on the rendering thread from a command enqueued by UTerrainComponent::Attach via FTerrainObject::InitResources().
// When this code is executed, the terrain component's scene proxy will already have been created because UTerrainComponent::Attach
// calls Super::Attach() before calling FTerrainObject::InitResources();
void FTerrainObject::ReinitDecalResources_RenderThread()
{
	checkSlow( IsInRenderingThread() );
	if ( TerrainComponent->SceneInfo && TerrainComponent->SceneInfo->Proxy )
	{
		for (INT DecalType = 0; DecalType < FPrimitiveSceneProxy::NUM_DECAL_TYPES; ++DecalType)
		{
			TArray<FDecalInteraction*>& Interactions = TerrainComponent->SceneInfo->Proxy->Decals[DecalType];

			// Release any outstanding terrain resources on the decals.
			for( INT DecalIndex = 0 ; DecalIndex < Interactions.Num() ; ++DecalIndex)
			{
				FDecalInteraction* DecalInteraction = Interactions(DecalIndex);
				for ( INT i = 0 ; i < DecalInteraction->RenderData->ReceiverResources.Num() ; ++i )
				{
					FReceiverResource* Resource = DecalInteraction->RenderData->ReceiverResources(i);
					Resource->Release_RenderingThread();
					delete Resource;
				}
				DecalInteraction->RenderData->ReceiverResources.Empty();
			}

			// Init the decal resources.
			const UINT ProxyMaxTessellationLevel = ((FTerrainComponentSceneProxy*)TerrainComponent->SceneInfo->Proxy)->GetMaxTessellation();
			for( INT DecalIndex = 0 ; DecalIndex < Interactions.Num() ; ++DecalIndex)
			{
				FDecalInteraction* DecalInteraction = Interactions(DecalIndex);
				AddDecalInteraction_RenderingThread( *DecalInteraction, ProxyMaxTessellationLevel );
			}
		}
	}
}

void FTerrainObject::ReleaseResources()
{
	if (SmoothIndexBuffer)
	{
		BeginReleaseResource(SmoothIndexBuffer);
	}

	if (VertexFactory)
	{
		BeginReleaseResource(VertexFactory);
	}
	if (DecalVertexFactory)
	{
		BeginReleaseResource(DecalVertexFactory->CastToFTerrainVertexFactory());
	}
	if (VertexBuffer)
	{
		BeginReleaseResource(VertexBuffer);
	}
}

void FTerrainObject::Update()
{
/***
	// create the new dynamic data for use by the rendering thread
	// this data is only deleted when another update is sent
	FDynamicTerrainData* NewDynamicData = new FDynamicTerrainData(this);

	// queue a call to update this data
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		TerrainObjectUpdateDataCommand,
		FTerrainObject*, TerrainObject, this,
		FDynamicTerrainData*, NewDynamicData, NewDynamicData,
		{
			TerrainObject->UpdateDynamicData_RenderThread(NewDynamicData);
		}
	);
***/
}

const FVertexFactory* FTerrainObject::GetVertexFactory() const
{
	return VertexFactory;
}

static UBOOL s_bGenerateTerrainResourcesEveryFrame = FALSE;
/** Called by FTerrainComponentSceneProxy; repacks vertex and index buffers as needed. */
UBOOL FTerrainObject::UpdateResources_RenderingThread(INT TessellationLevel,
													  TArray<FDecalInteraction*>& ProxyDecals)
{
	UBOOL bReturn = TRUE;

	// Handle the vertex buffer
	check(VertexBuffer);
	if ((VertexBuffer->GetCurrentTessellation() != TessellationLevel) ||
		(VertexBuffer->GetRepackRequired() == TRUE))
	{
		// We need to repack this
		check(TessellationLevel > 0);
		VertexBuffer->FillData(TessellationLevel);

		// Update the vertex factory
		check(VertexFactory);
		VertexFactory->SetTessellationLevel(TessellationLevel);

		// Update the decal vertex factory
		check(DecalVertexFactory);
		DecalVertexFactory->CastToFTerrainVertexFactory()->SetTessellationLevel(TessellationLevel);

		// set the tessellation level for the cached decalvertex factories as well
		for( INT DecalIdx=0; DecalIdx < ProxyDecals.Num(); DecalIdx++ )
		{
			FDecalInteraction* Decal = ProxyDecals(DecalIdx);
			if( Decal && 
				Decal->RenderData &&
				Decal->RenderData->DecalVertexFactory )
			{
				((FTerrainVertexFactory*)Decal->RenderData->DecalVertexFactory)->SetTessellationLevel(TessellationLevel);
			}
		}

		// Force the index buffer(s) to repack as well
		bRepackRequired = TRUE;

		VertexBuffer->ClearRepackRequired();
	}

	// Handle the index buffer(s)
	if (bRepackRequired == TRUE)
	{
		//@todo.SAS. Fix this usage pattern...
		// This will prevent deleting and recreating the index buffers each time 
		// a repack is required.

		// Check the tessellation level of each index buffer vs. the cached tessellation level
		INT MaxTessellation = MaxTessellationLevel;
		if (SmoothIndexBuffer)
		{
			if (SmoothIndexBuffer->MaxTesselationLevel != MaxTessellation)
			{
				SmoothIndexBuffer->ReleaseResource();
				delete SmoothIndexBuffer;
#if CONSOLE
				SmoothIndexBuffer = ::new TerrainTessellationIndexBufferType(this, MaxTessellationLevel, TRUE, TRUE);
#else
				SmoothIndexBuffer = ::new TerrainTessellationIndexBufferType(this, MaxTessellationLevel, TRUE);
#endif
			}

			SmoothIndexBuffer->SetCurrentTessellationLevel(TessellationLevel);
			SmoothIndexBuffer->PrimeBuffer();
			if ((SmoothIndexBuffer->NumTriangles > 0) && (IsValidRef(SmoothIndexBuffer->IndexBufferRHI) == FALSE))
			{
				debugf(TEXT("INVALID TERRAIN INDEX BUFFER 0x%08x!"), SmoothIndexBuffer);
			}
			if (SmoothIndexBuffer->NumTriangles > 0)
			{
				SmoothIndexBuffer->FillData();
			}
		}

		RepackDecalIndexBuffers_RenderingThread( TessellationLevel, MaxTessellation, ProxyDecals );
	}

	bReturn = (VertexBuffer && VertexFactory && DecalVertexFactory);

	return bReturn;
}

void FTerrainObject::RepackDecalIndexBuffers_RenderingThread(INT InTessellationLevel, INT InMaxTessellation, TArray<FDecalInteraction*>& ProxyDecals)
{
	checkSlow( IsInRenderingThread() );

	for ( INT DecalIndex = 0 ; DecalIndex < ProxyDecals.Num() ; ++ DecalIndex )
	{
		FDecalInteraction* DecalInteraction = ProxyDecals(DecalIndex);
		if( DecalInteraction )
		{
			for ( INT i = 0 ; i < DecalInteraction->RenderData->ReceiverResources.Num() ; ++i )
			{
				FDecalTerrainInteraction* Decal = (FDecalTerrainInteraction*)DecalInteraction->RenderData->ReceiverResources(i);
				if( Decal )
				{
					Decal->RepackIndexBuffers_RenderingThread( this, InTessellationLevel, InMaxTessellation );
					// replace the batched FStaticMesh index buffer with the newly generated one
					if( DecalInteraction->DecalStaticMesh && i == 0 )
					{
						const FIndexBuffer* DecalIndexBuffer = Decal->GetSmoothIndexBuffer();
						DecalInteraction->DecalStaticMesh->Elements(0).IndexBuffer = DecalIndexBuffer;
						DecalInteraction->DecalStaticMesh->Elements(0).NumPrimitives = Decal->GetSmoothIndexBuffer()->NumTriangles;
					}
				}
			}
		}		
	}
}

FPrimitiveSceneProxy* UTerrainComponent::CreateSceneProxy()
{
	FTerrainComponentSceneProxy* TCompProxy = NULL;

	// only create a scene proxy for rendering if properly initialized
	if (TerrainObject)
	{
		ATerrain* Terrain = GetTerrain();
		check(Terrain);

		INT CheckTess = 0;
		if (GEngine->TerrainTessellationCheckCount > 0)
		{
			CheckTess = ((SectionBaseX % GEngine->TerrainTessellationCheckCount) +
						 (SectionBaseY % GEngine->TerrainTessellationCheckCount)) %
						 GEngine->TerrainTessellationCheckCount;
		}

		FLOAT TessCheckDist = Terrain->TessellationCheckDistance;
		if (TessCheckDist < 0.0f)
		{
			TessCheckDist = GEngine->TerrainTessellationCheckDistance;
		}
		if (GEngine->TerrainTessellationCheckDistance < 0.0f)
		{
			// The engine FORCES all components to tessellate when set to -1.0
			TessCheckDist = 0.0f;
		}

		if ((GIsGame && (GetTriangleCount() > 0)) || GIsEditor)
		{
			TCompProxy = ::new FTerrainComponentSceneProxy(this, TessCheckDist, CheckTess);
#if !CONSOLE
			if (GIsEditor && TCompProxy)
			{
				SetupLightmapResolutionViewInfo(*TCompProxy);
			}
#endif
			TCompProxy->UpdateData(this);
		}
	}

	return (FPrimitiveSceneProxy*)TCompProxy;
}

UINT UTerrainComponent::GetTriangleCount()
{
	if (TerrainObject)
	{
		if (TerrainObject->SmoothIndexBuffer)
		{
			return TerrainObject->SmoothIndexBuffer->DetermineTriangleCount();
		}
	}

	return 0;
}

// Calculate # of triangles for the DecalComponent
// Used for AnalyzeReferencedContent - commandlet
// Do not use this in-game. UDecalComponent's receiver has already # of triangles
UINT UTerrainComponent::GetTriangleCountForDecal( UDecalComponent * DecalComponent )
{
	UINT NumTriangles = 0;
	ATerrain * Terrain = GetTerrain();
	// This is used by commandlet to get # of triangles for decalcomponent of terrain
	if ( Terrain )
	{
		FTerrainObject * LocalTerrainObject = ::new FTerrainObject(this, Terrain->MaxTesselationLevel);
		check (LocalTerrainObject);
		
		class FDecalTerrainInteraction* Decal = new FDecalTerrainInteraction(DecalComponent,this,Terrain->NumPatchesX,Terrain->NumPatchesY,Terrain->MaxTesselationLevel);
		
		if ( Decal )
		{
			TerrainDecalTessellationIndexBufferType* SmoothIndexBuffer = NULL;

			if ((GIsGame == TRUE) && (Terrain->MinTessellationLevel == Terrain->MaxTesselationLevel))
			{
				SmoothIndexBuffer = ::new TerrainDecalTessellationIndexBufferType(Decal->GetMinPatchX(), Decal->GetMinPatchY(), Decal->GetMaxPatchX(), Decal->GetMaxPatchY(), LocalTerrainObject, Terrain->MaxTesselationLevel, FALSE, FALSE);
			}
			else
			{
				SmoothIndexBuffer = ::new TerrainDecalTessellationIndexBufferType(Decal->GetMinPatchX(), Decal->GetMinPatchY(), Decal->GetMaxPatchX(), Decal->GetMaxPatchY(), LocalTerrainObject, Terrain->MaxTesselationLevel, FALSE);
			}

			if ( SmoothIndexBuffer )
			{
				NumTriangles = SmoothIndexBuffer->DetermineTriangleCount();

				delete SmoothIndexBuffer;
			}

			delete Decal;
		}

		delete LocalTerrainObject;
	}

	return NumTriangles;
}
//
//	FTerrainIndexBuffer
//
void FTerrainIndexBuffer::InitRHI()
{
	SCOPE_CYCLE_COUNTER(STAT_TerrainSmoothTime);

	INT SectBaseX = TerrainObject->GetComponentSectionBaseX();
	INT SectBaseY = TerrainObject->GetComponentSectionBaseY();
	INT	SectSizeX = TerrainObject->GetComponentSectionSizeX();
	INT	SectSizeY = TerrainObject->GetComponentSectionSizeY();

	UINT Stride = sizeof(WORD);
	UINT Size = 2 * 3 * SectSizeX * SectSizeY * Stride;

	IndexBufferRHI = RHICreateIndexBuffer(Stride, Size, NULL, RUF_Static|RUF_WriteOnly);

	// Lock the buffer.
	WORD* DestIndex = (WORD*)(RHILockIndexBuffer(IndexBufferRHI, 0, Size));

	// Fill in the index data
	// Zero initialize index buffer if we're rendering it for the very first time as we're going to render more quads than are
	// visible. Zeroing them out will cause the extraneous triangles to be degenerate so it won't have an effect on visuals.
	if (NumVisibleTriangles == INDEX_NONE)
	{
		appMemzero(DestIndex, Size);
	}

	NumVisibleTriangles = 0;

	ATerrain* Terrain = CastChecked<ATerrain>(TerrainObject->TerrainComponent->GetOwner());

	for (INT Y = 0; Y < SectSizeY; Y++)
	{
		for (INT X = 0; X < SectSizeX; X++)
		{
			if (Terrain->IsTerrainQuadVisible(SectBaseX + X, SectBaseY + Y) == FALSE)
			{
				continue;
			}

			INT	V1 = Y*(SectSizeX+1) + X;
			INT	V2 = V1+1;
			INT	V3 = (Y+1)*(SectSizeX+1) + (X+1);
			INT	V4 = V3-1;

			if (Terrain->IsTerrainQuadFlipped(SectBaseX + X, SectBaseY + Y) == FALSE)
			{
				*DestIndex++ = V1;
				*DestIndex++ = V4;
				*DestIndex++ = V3;
				NumVisibleTriangles++;

				*DestIndex++ = V3;
				*DestIndex++ = V2;
				*DestIndex++ = V1;
				NumVisibleTriangles++;
			}
			else
			{
				*DestIndex++ = V1;
				*DestIndex++ = V4;
				*DestIndex++ = V2;
				NumVisibleTriangles++;

				*DestIndex++ = V2;
				*DestIndex++ = V4;
				*DestIndex++ = V3;
				NumVisibleTriangles++;
			}
		}
	}

	// Unlock the buffer.
	RHIUnlockIndexBuffer(IndexBufferRHI);
}

//
//	FTerrainComponentSceneProxy
//
FTerrainComponentSceneProxy::FTerrainBatchInfo::FTerrainBatchInfo(
	UTerrainComponent* Component, INT BatchIndex)
{
	ATerrain* Terrain = Component->GetTerrain();

	FTerrainMaterialMask Mask(1);

	if (BatchIndex == -1)
	{
		Mask = Component->BatchMaterials(Component->FullBatch);
	}
	else
	{
		Mask = Component->BatchMaterials(BatchIndex);
	}

	// Fetch the material instance
	MaterialRenderProxy = Terrain->GetCachedMaterial(Mask, bIsTerrainMaterialResourceInstance);

	// Fetch the required weight maps
	WeightMaps.Empty();
	if (bIsTerrainMaterialResourceInstance)
	{
		for (INT MaterialIndex = 0; MaterialIndex < Mask.Num(); MaterialIndex++)
		{
			if (Mask.Get(MaterialIndex))
			{
				FTerrainWeightedMaterial* WeightedMaterial = &(Terrain->WeightedMaterials(MaterialIndex));
				check(WeightedMaterial);
				INT TextureIndex = MaterialIndex / 4;
				if (TextureIndex < Terrain->WeightedTextureMaps.Num())
				{
					UTerrainWeightMapTexture* Texture = Terrain->WeightedTextureMaps(TextureIndex);
					check(Texture && TEXT("Terrain weight texture map not present!"));
					WeightMaps.AddUniqueItem(Texture);
				}
			}
		}
	}
}

FTerrainComponentSceneProxy::FTerrainBatchInfo::~FTerrainBatchInfo()
{
	// Just empty the array...
	WeightMaps.Empty();
}

FTerrainComponentSceneProxy::FTerrainMaterialInfo::FTerrainMaterialInfo(UTerrainComponent* Component)
:	BatchInfo(Component, -1)
{
	ComponentLightInfo = new FTerrainComponentInfo(*Component);
	check(ComponentLightInfo);
}

FTerrainComponentSceneProxy::FTerrainMaterialInfo::~FTerrainMaterialInfo()
{
	delete ComponentLightInfo;
	ComponentLightInfo = NULL;
}

FTerrainComponentSceneProxy::FTerrainComponentSceneProxy(UTerrainComponent* Component, 
														 FLOAT InCheckTessellationDistance, 
														 WORD InCheckTessellationOffset) :
	  FPrimitiveSceneProxy(Component)
	, CheckTessellationCounter(0)	// This will force the first frame to properly tessellate.
	, CheckTessellationOffset(InCheckTessellationOffset)
	, LastTessellationCheck(-1)
	, CheckTessellationDistance(InCheckTessellationDistance * InCheckTessellationDistance)
	, TrackedLastVisibilityChangeTime(0.0f)
	, Owner(Component->GetOwner())
	, ComponentOwner(Component)
	, TerrainObject(Component->TerrainObject)
	, LinearLevelColor(1.0f,1.0f,1.0f)
	, LinearPropertyColor(1.0f,1.0f,1.0f)
	, CullDistance(Component->CachedMaxDrawDistance > 0 ? Component->CachedMaxDrawDistance : FLT_MAX)
	, bCastShadow(Component->CastShadow)
	, CurrentMaterialInfo(NULL)
	, MaxTessellation(1)
{
	check(CheckTessellationDistance >= 0.0f);

	// Try to find a color for level coloration.
	if (Owner)
	{
		ULevel* Level = Owner->GetLevel();
		ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel(Level);
		if (LevelStreaming)
		{
			LinearLevelColor = FLinearColor(LevelStreaming->DrawColor);
		}
	}

	// Get a color for property coloration.
	FColor PropertyColor = FColor(255, 255, 255);
	GEngine->GetPropertyColorationColor( (UObject*)Component, PropertyColor );
	LinearPropertyColor = FLinearColor(PropertyColor);

	ATerrain* const Terrain = ComponentOwner->GetTerrain();

	// Make sure terrain material resource parameters are cached for the current quality level
	TArrayNoInit<FTerrainMaterialResource*>& CachedMaterials = Terrain->GetCachedTerrainMaterials();
	for( INT MatIdx = 0; MatIdx < CachedMaterials.Num(); MatIdx++ )
	{
		CachedMaterials(MatIdx)->CacheParameters();
	}
}

FTerrainComponentSceneProxy::~FTerrainComponentSceneProxy()
{
	delete CurrentMaterialInfo;
	CurrentMaterialInfo = NULL;
}

/**
 * Adds a decal interaction to the primitive.  This is called in the rendering thread by AddDecalInteraction_GameThread.
 */
void FTerrainComponentSceneProxy::AddDecalInteraction_RenderingThread(const FDecalInteraction& DecalInteraction)
{
	INT DecalType;
	//make a copy from the template that will be owned by the proxy
	FDecalInteraction* NewInteraction = new FDecalInteraction(DecalInteraction);
	FPrimitiveSceneProxy::AddDecalInteraction_Internal_RenderingThread( NewInteraction, DecalType);

	if( TerrainObject )
	{
		// If there are an unequal number of index buffers compared to BatchMaterials, force a repack.
		if (TerrainObject->GetRepackRequired() == TRUE)
		{
			// Update the resources for this render call
			INT TessParam = MaxTessellation;
			if (TerrainObject->MorphingFlags != ETMORPH_Disabled)
			{
				TessParam = Clamp<INT>((MaxTessellation*2), 1, TerrainObject->MaxTessellationLevel);
			}
			TArray<FDecalInteraction*> AllDecals;
			AllDecals = Decals[STATIC_DECALS];
			AllDecals.Append(Decals[DYNAMIC_DECALS]);
			TerrainObject->UpdateResources_RenderingThread(TessParam, AllDecals);
			TerrainObject->SetRepackRequired(FALSE);
		}

		TerrainObject->AddDecalInteraction_RenderingThread( *NewInteraction, TerrainObject->MaxTessellationLevel );
	}
}

/** 
* Draw the scene proxy as a dynamic element
*
* @param	PDI - draw interface to render to
* @param	View - current view
* @param	DPGIndex - current depth priority 
* @param	Flags - optional set of flags from EDrawDynamicElementFlags
*/
void FTerrainComponentSceneProxy::DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
{
	// Do not draw the terrain if: using an unsupported shader platform, es2 rendering, or if we are emulating mobile rendering
	if( (GRHIShaderPlatform!=SP_PCD3D_SM3 && GRHIShaderPlatform!=SP_PCD3D_SM4 && GRHIShaderPlatform!=SP_PCD3D_SM5 && GRHIShaderPlatform!=SP_PCOGL && GRHIShaderPlatform!=SP_XBOXD3D && GRHIShaderPlatform!=SP_PS3) || GUsingMobileRHI || GEmulateMobileRendering )
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_TerrainRenderTime);
	
	// It is 'safe' to grab the owner in the case of terrain, as the owner will
	// not FinishDestroy until the render-thread has been flushed.
	ATerrain* Terrain = Cast<ATerrain>(Owner);
	check(Terrain);

	// Determine the DPG the primitive should be drawn in for this view.
	if ((GetDepthPriorityGroup(View) == DPGIndex) && ((View->Family->ShowFlags & SHOW_Terrain) != 0))
	{
		check(TerrainObject);

		// If there are an unequal number of index buffers compared to BatchMaterials, force a repack.
		if (TerrainObject->GetRepackRequired() == TRUE)
		{
			// Update the resources for this render call
			INT TessParam = MaxTessellation;
			if (TerrainObject->MorphingFlags != ETMORPH_Disabled)
			{
				TessParam = Clamp<INT>((MaxTessellation*2), 1, TerrainObject->MaxTessellationLevel);
			}
			TArray<FDecalInteraction*> AllDecals;
			AllDecals = Decals[STATIC_DECALS];
			AllDecals.Append(Decals[DYNAMIC_DECALS]);
			TerrainObject->UpdateResources_RenderingThread(TessParam, AllDecals);
			TerrainObject->SetRepackRequired(FALSE);
		}

		// Render
		Meshes.Empty( 1 );

		const INT TriCount = TerrainObject->SmoothIndexBuffer->NumTriangles;
		FMeshBatch Mesh;
		if (TriCount != 0)
		{
			INC_DWORD_STAT_BY(STAT_TerrainTriangles,TriCount);

			FMeshBatchElement& BatchElement = Mesh.Elements(0);
			BatchElement.NumPrimitives = TriCount;
			BatchElement.IndexBuffer = TerrainObject->SmoothIndexBuffer;
			Mesh.VertexFactory = TerrainObject->VertexFactory;
			Mesh.DynamicVertexData = NULL;
			Mesh.DynamicVertexStride = 0;
			BatchElement.DynamicIndexData = NULL;
			BatchElement.DynamicIndexStride = 0;
			Mesh.LCI = CurrentMaterialInfo->ComponentLightInfo;

			// The full batch is stored at entry 0
			FTerrainBatchInfo* BatchInfo = &CurrentMaterialInfo->BatchInfo;
			FMaterialRenderProxy* MaterialRenderProxy = BatchInfo->MaterialRenderProxy;
			if (MaterialRenderProxy == NULL)
			{
				MaterialRenderProxy = GEngine->DefaultMaterial->GetRenderProxy(0);
			}
			Mesh.MaterialRenderProxy = MaterialRenderProxy;

			BatchElement.LocalToWorld = LocalToWorld;
			BatchElement.WorldToLocal = LocalToWorld.Inverse();
			BatchElement.FirstIndex = 0;
			BatchElement.MinVertexIndex = 0;
			check(TerrainObject->VertexBuffer);
			BatchElement.MaxVertexIndex = TerrainObject->VertexBuffer->GetVertexCount() - 1;
			Mesh.UseDynamicData = FALSE;
			Mesh.ReverseCulling = LocalToWorldDeterminant < 0.0f ? TRUE : FALSE;;
			Mesh.CastShadow = bCastShadow;
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = (ESceneDepthPriorityGroup)DPGIndex;
			Mesh.bUsePreVertexShaderCulling = FALSE;
			Mesh.PlatformMeshData = NULL;

			DrawRichMesh(PDI,Mesh,FLinearColor::White,LinearLevelColor,LinearPropertyColor,PrimitiveSceneInfo,bSelected);

			if (AllowDebugViewmodes() && Terrain->bShowWireframe)
			{
				const FColoredMaterialRenderProxy WireframeMaterialInstance(
					GEngine->WireframeMaterial->GetRenderProxy(FALSE),
					ConditionalAdjustForMobileEmulation(View, Terrain->WireframeColor)
					);

				FMeshBatch WireMesh = Mesh;
				WireMesh.bWireframe = TRUE;
				WireMesh.DepthBias = -0.00002f; // depth bias to prevent z fighting w/ the terrain
				WireMesh.MaterialRenderProxy = &WireframeMaterialInstance;
				DrawRichMesh(PDI,WireMesh,FLinearColor::White,LinearLevelColor,LinearPropertyColor,PrimitiveSceneInfo,bSelected);
			}
		}

		Meshes.AddItem( Mesh );

		if (View->Family->ShowFlags & SHOW_TerrainPatches)
		{
			// Draw a rectangle around the component...
			FBox BoundingBox(0);
			INT Scalar = Terrain->MaxTesselationLevel;
			INT LocalSectionSizeX = TerrainObject->GetComponentSectionSizeX();
			INT LocalSectionSizeY = TerrainObject->GetComponentSectionSizeY();
			for(INT Y = 0;Y < LocalSectionSizeY;Y++)
			{
				for(INT X = 0;X < LocalSectionSizeX;X++)
				{
					const FTerrainPatchBounds&	Patch = TerrainObject->TerrainComponent->PatchBounds(Y * LocalSectionSizeX + X);

					FLOAT XLeft = X * Scalar;
					FLOAT XRight = (X + 1) * Scalar;
					if (X > 0)
					{
						XLeft -= Patch.MaxDisplacement;
					}
					if ((X + 1) < LocalSectionSizeX)
					{
						XRight += Patch.MaxDisplacement;
					}

					FLOAT YLeft = Y * Scalar;
					FLOAT YRight = (Y + 1) * Scalar;
					if (Y > 0)
					{
						YLeft -= Patch.MaxDisplacement;
					}
					if ((Y + 1) < LocalSectionSizeY)
					{
						YRight += Patch.MaxDisplacement;
					}

					BoundingBox += FBox(FVector(XLeft,YLeft,Patch.MinHeight),
										FVector(XRight,YRight,Patch.MaxHeight));
				}
			}
			BoundingBox = BoundingBox.TransformBy(LocalToWorld);
			DrawWireBox(PDI, BoundingBox, FColor(255, 255, 0), DPGIndex);
		}
	}
}

FPrimitiveViewRelevance FTerrainComponentSceneProxy::GetViewRelevance(const FSceneView* View)
{
	FPrimitiveViewRelevance Result;

	if (TerrainObject != NULL)
	{
		if(View->Family->ShowFlags & SHOW_Terrain)
		{
			if (IsShown(View))
			{
				Result.bNeedsPreRenderView = TRUE;
				Result.bDynamicRelevance = TRUE;
				Result.SetDPG(GetDepthPriorityGroup(View),TRUE);
				Result.bDecalStaticRelevance = HasRelevantStaticDecals(View);
				Result.bDecalDynamicRelevance = HasRelevantDynamicDecals(View);
				//@todo.SAS. Kick off the terrain tessellation in a separate here
			}
			if (IsShadowCast(View))
			{
				Result.bShadowRelevance = TRUE;
			}

			Result.bDecalStaticRelevance = HasRelevantStaticDecals(View);
			Result.bDecalDynamicRelevance = HasRelevantDynamicDecals(View);
		}
	}
	return Result;
}

/**
*	Determines the relevance of this primitive's elements to the given light.
*	@param	LightSceneInfo			The light to determine relevance for
*	@param	bDynamic (output)		The light is dynamic for this primitive
*	@param	bRelevant (output)		The light is relevant for this primitive
*	@param	bLightMapped (output)	The light is light mapped for this primitive
*/
void FTerrainComponentSceneProxy::GetLightRelevance(const FLightSceneInfo* LightSceneInfo, UBOOL& bDynamic, UBOOL& bRelevant, UBOOL& bLightMapped) const
{
	// Attach the light to the primitive's static meshes.
	bDynamic = TRUE;
	bRelevant = FALSE;
	bLightMapped = TRUE;

	if (CurrentMaterialInfo)
	{
		if (CurrentMaterialInfo->ComponentLightInfo)
		{
			ELightInteractionType InteractionType = CurrentMaterialInfo->ComponentLightInfo->GetInteraction(LightSceneInfo).GetType();
			if(InteractionType != LIT_CachedIrrelevant)
			{
				bRelevant = TRUE;
			}
			if(InteractionType != LIT_CachedLightMap && InteractionType != LIT_CachedIrrelevant)
			{
				bLightMapped = FALSE;
			}
			if(InteractionType != LIT_Uncached)
			{
				bDynamic = FALSE;
			}
		}
	}
	else
	{
		bRelevant = TRUE;
		bLightMapped = FALSE;
	}
}

IMPLEMENT_COMPARE_CONSTPOINTER( FDecalInteraction, UnTerrainRender,
{
	return (A->DecalState.SortOrder <= B->DecalState.SortOrder) ? -1 : 1;
} );

/**
* Draws the primitive's static decal elements.  This is called from the game thread whenever this primitive is attached
* as a receiver for a decal.
*
* The static elements will only be rendered if GetViewRelevance declares both static and decal relevance.
* Called in the game thread.
*
* @param PDI - The interface which receives the primitive elements.
*/
void FTerrainComponentSceneProxy::DrawStaticDecalElements(FStaticPrimitiveDrawInterface* PDI,const FDecalInteraction& DecalInteraction)
{
	// Do nothing if there is no terrain object.
	if (!TerrainObject )
	{
		return;
	}

	if( !HasViewDependentDPG() && !IsMovable() )
	{
		const FDecalState& DecalState	= DecalInteraction.DecalState;
		const FDecalRenderData* RenderData = DecalInteraction.RenderData;

		const FIndexBuffer* DecalIndexBuffer = NULL;
		if( RenderData &&
			RenderData->ReceiverResources.Num() > 0 )
		{			
			FDecalTerrainInteraction* TerrainDecalResource = (FDecalTerrainInteraction*)RenderData->ReceiverResources(0);
			DecalIndexBuffer = TerrainDecalResource->GetSmoothIndexBuffer();
		}

		const INT TriCount = DecalIndexBuffer ? ((TerrainDecalTessellationIndexBufferType*)DecalIndexBuffer)->NumTriangles : 0;
		if( TriCount > 0 && 
			RenderData->DecalVertexFactory )
		{
			FMeshBatch Mesh;
			FMeshBatchElement& BatchElement = Mesh.Elements(0);
			Mesh.VertexFactory = RenderData->DecalVertexFactory->CastToFVertexFactory();

			// set shader parameters needed for decal vertex factory
			RenderData->DecalVertexFactory->SetDecalMatrix( DecalState.WorldTexCoordMtx );
			RenderData->DecalVertexFactory->SetDecalLocation( DecalState.HitLocation );
			RenderData->DecalVertexFactory->SetDecalOffset( FVector2D(DecalState.OffsetX, DecalState.OffsetY) );
			FVector V1 = DecalState.HitBinormal;
			FVector V2 = DecalState.HitTangent;
			FVector V3 = DecalState.HitNormal;
			V1 = LocalToWorld.Inverse().TransformNormal( V1 ).SafeNormal();
			V2 = LocalToWorld.Inverse().TransformNormal( V2 ).SafeNormal();
			V3 = LocalToWorld.Inverse().TransformNormal( V3 ).SafeNormal();
			RenderData->DecalVertexFactory->SetDecalLocalBinormal( V1 );
			RenderData->DecalVertexFactory->SetDecalLocalTangent( V2 );
			RenderData->DecalVertexFactory->SetDecalLocalNormal( V3 );

			BatchElement.FirstIndex = 0;
			BatchElement.MinVertexIndex = 0;
			check(TerrainObject->VertexBuffer);
			BatchElement.MaxVertexIndex = TerrainObject->VertexBuffer->GetVertexCount() - 1;

			// Set the decal parameters.
			BatchElement.IndexBuffer = DecalIndexBuffer;
			BatchElement.NumPrimitives = TriCount;

			Mesh.MaterialRenderProxy = DecalState.DecalMaterial->GetRenderProxy(0);

			// This makes the decal render using a scissor rect (for performance reasons).
			Mesh.DecalState = &DecalState;
			Mesh.bIsDecal = TRUE;

			// Terrain decals are much less susceptible to z-fighting because of how we draw them.
			// So bias the DepthBias to push the decal closer to the object. Allows us have a much more conservative
			// setting for everything else.
			Mesh.DepthBias = DecalState.DepthBias * 0.1f;
			Mesh.SlopeScaleDepthBias = DecalState.SlopeScaleDepthBias;

			Mesh.CastShadow = FALSE;
			Mesh.DynamicVertexData = NULL;
			Mesh.DynamicVertexStride = 0;
			BatchElement.DynamicIndexData = NULL;
			BatchElement.DynamicIndexStride = 0;
			Mesh.UseDynamicData = FALSE;

			BatchElement.LocalToWorld = LocalToWorld;
			BatchElement.WorldToLocal = LocalToWorld.Inverse();
			Mesh.ReverseCulling = LocalToWorldDeterminant < 0.0f ? TRUE : FALSE;
			Mesh.bUsePreVertexShaderCulling = FALSE;
			Mesh.PlatformMeshData = NULL;

			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = GetStaticDepthPriorityGroup();

			if( DecalState.bDecalMaterialHasStaticLightingUsage )
			{
				Mesh.LCI = CurrentMaterialInfo->ComponentLightInfo;
			}
			else
			{
				Mesh.LCI = NULL;
			}

			PDI->DrawMesh(Mesh,0,FLT_MAX);
		}
	}
}

/**
* Draws the primitive's dynamic decal elements.  This is called from the rendering thread for each frame of each view.
* The dynamic elements will only be rendered if GetViewRelevance declares dynamic relevance.
* Called in the rendering thread.
*
* @param	PDI						The interface which receives the primitive elements.
* @param	View					The view which is being rendered.
* @param	InDepthPriorityGroup	The DPG which is being rendered.
* @param	bDynamicLightingPass	TRUE if drawing dynamic lights, FALSE if drawing static lights.
* @param	bDrawOpaqueDecals		TRUE if we want to draw opaque decals
* @param	bDrawTransparentDecals	TRUE if we want to draw transparent decals
* @param	bTranslucentReceiverPass	TRUE during the decal pass for translucent receivers, FALSE for opaque receivers.
*/
void FTerrainComponentSceneProxy::DrawDynamicDecalElements(
	FPrimitiveDrawInterface* PDI,
	const FSceneView* View,
	UINT InDepthPriorityGroup,
	UBOOL bDynamicLightingPass,
	UBOOL bDrawOpaqueDecals,
	UBOOL bDrawTransparentDecals,
	UBOOL bTranslucentReceiverPass
	)
{
	SCOPE_CYCLE_COUNTER(STAT_DecalRenderDynamicTerrainTime);

	checkSlow( View->Family->ShowFlags & SHOW_Terrain );
	checkSlow( View->Family->ShowFlags & SHOW_Decals );

	// Decals on terrain with translucent materials not currently supported.
	if ( bTranslucentReceiverPass )
	{
		return;
	}

	// Do nothing if there is no terrain object.
	if ( !TerrainObject )
	{
		return;
	}

#if !FINAL_RELEASE
	const UBOOL bRichView = IsRichView(View);
#else
	const UBOOL bRichView = FALSE;
#endif

	// Determine the DPG the primitive should be drawn in for this view.
	if (GetViewRelevance(View).GetDPG(InDepthPriorityGroup) == TRUE)
	{
		// Compute the set of decals in this DPG.
		FMemMark MemStackMark(GRenderingThreadMemStack);
		TArray<FDecalInteraction*,TMemStackAllocator<GRenderingThreadMemStack> > DPGDecals;

		// only render decals that haven't been added to a static batch
		INT StartDecalType = !bRichView ? DYNAMIC_DECALS : STATIC_DECALS;

		for (INT DecalType = StartDecalType; DecalType < NUM_DECAL_TYPES; ++DecalType)
		{
			for ( INT DecalIndex = 0 ; DecalIndex < Decals[DecalType].Num() ; ++DecalIndex )
			{
				FDecalInteraction* Interaction = Decals[DecalType](DecalIndex);
				if( // match current DPG
					InDepthPriorityGroup == Interaction->DecalState.DepthPriorityGroup &&
					// Render all decals during the opaque pass, as the translucent pass is rejected completely above
					//((Interaction->DecalState.MaterialViewRelevance.bTranslucency && bTranslucentReceiverPass) || !bTranslucentReceiverPass) &&
					// only render lit decals during dynamic lighting pass
					((Interaction->DecalState.MaterialViewRelevance.bLit && bDynamicLightingPass) || !bDynamicLightingPass) )
				{
					DPGDecals.AddItem( Interaction );
				}
			}
		}

		// Do nothing if no lit decals exist.
		if ( DPGDecals.Num() == 0 )
		{
			return;
		}

		// Sort and render all decals.
		Sort<USE_COMPARE_CONSTPOINTER(FDecalInteraction,UnTerrainRender)>( DPGDecals.GetTypedData(), DPGDecals.Num() );

		for (INT BatchIndex = 0; BatchIndex < Meshes.Num(); BatchIndex++)
		{
			if ( Meshes(BatchIndex).GetNumPrimitives() > 0 )
			{
				FMeshBatch Mesh( Meshes(BatchIndex) );

				Mesh.VertexFactory = TerrainObject->DecalVertexFactory->CastToFTerrainVertexFactory();
				Mesh.CastShadow = FALSE;

				for ( INT DecalIndex = 0 ; DecalIndex < DPGDecals.Num() ; ++DecalIndex )
				{
					const FDecalInteraction* DecalInteraction = DPGDecals(DecalIndex);
					const FDecalState& DecalState = DecalInteraction->DecalState;
					const FBox& DecalBoundingBox = DecalState.Bounds;

					if( DecalState.bDecalMaterialHasStaticLightingUsage )
					{
						Mesh.LCI = CurrentMaterialInfo->ComponentLightInfo;
					}
					else
					{
						Mesh.LCI = NULL;
					}


					UBOOL bIsDecalVisible = TRUE;

					// Distance cull using decal's CullDistance (perspective views only)
					if( bIsDecalVisible && View->ViewOrigin.W > 0.0f )
					{
						// Compute the distance between the view and the decal
						FLOAT SquaredDistance = ( DecalBoundingBox.GetCenter() - View->ViewOrigin ).SizeSquared();
						const FLOAT SquaredCullDistance = DecalState.SquaredCullDistance;
						if( SquaredCullDistance > 0.0f && SquaredDistance > SquaredCullDistance )
						{
							// Too far away to render
							bIsDecalVisible = FALSE;
						}
					}

					if( bIsDecalVisible )
					{
						// Make sure the decal's frustum bounds are in view
						if( !View->ViewFrustum.IntersectBox( DecalBoundingBox.GetCenter(), DecalBoundingBox.GetExtent() ) )
						{
							bIsDecalVisible = FALSE;
						}
					}


					if( bIsDecalVisible )
					{
						const FDecalRenderData* RenderData = DecalInteraction->RenderData;
						// Compute a scissor rect by clipping the decal frustum vertices to the screen.
						// Don't draw the decal if the frustum projects off the screen.
						FVector2D MinCorner;
						FVector2D MaxCorner;
						if ( DecalState.QuadToClippedScreenSpaceAABB( View, MinCorner, MaxCorner, LocalToWorld ) )
						{
							const FIndexBuffer* DecalIndexBuffer = NULL;
							if ( RenderData->ReceiverResources.Num() > 0 )
							{
								FDecalTerrainInteraction* TerrainDecalResource = (FDecalTerrainInteraction*)RenderData->ReceiverResources(0);
								DecalIndexBuffer = TerrainDecalResource->GetSmoothIndexBuffer();
							}
							if ( DecalIndexBuffer )
							{
								const INT TriCount = ((TerrainDecalTessellationIndexBufferType*)DecalIndexBuffer)->NumTriangles;
								if ( TriCount > 0 )
								{
									FMeshBatchElement& BatchElement = Mesh.Elements(0);
									// Set the decal parameters.
									BatchElement.IndexBuffer = DecalIndexBuffer;
									BatchElement.NumPrimitives = TriCount;
									Mesh.MaterialRenderProxy = DecalState.DecalMaterial->GetRenderProxy(0);
									
									// WRH - 2007/08/24 - Terrain decals are much less susceptible to z-fighting because of how we draw them.
									// So bias the DepthBias to push the decal closer to the object. Allows us have a much more conservative
									// setting for everything else.
									Mesh.DepthBias = DecalState.DepthBias * 0.1f;
									Mesh.SlopeScaleDepthBias = DecalState.SlopeScaleDepthBias;

									// set shader parameters needed for decal vertex factory
									TerrainObject->DecalVertexFactory->SetDecalMatrix( DecalState.WorldTexCoordMtx );
									TerrainObject->DecalVertexFactory->SetDecalLocation( DecalState.HitLocation );
									TerrainObject->DecalVertexFactory->SetDecalOffset( FVector2D(DecalState.OffsetX, DecalState.OffsetY) );
									FVector V1 = DecalState.HitBinormal;
									FVector V2 = DecalState.HitTangent;
									FVector V3 = DecalState.HitNormal;
									V1 = LocalToWorld.Inverse().TransformNormal( V1 ).SafeNormal();
									V2 = LocalToWorld.Inverse().TransformNormal( V2 ).SafeNormal();
									V3 = LocalToWorld.Inverse().TransformNormal( V3 ).SafeNormal();
									TerrainObject->DecalVertexFactory->SetDecalLocalBinormal( V1 );
									TerrainObject->DecalVertexFactory->SetDecalLocalTangent( V2 );
									TerrainObject->DecalVertexFactory->SetDecalLocalNormal( V3 );
									TerrainObject->DecalVertexFactory->SetDecalMinMaxBlend(RenderData->DecalBlendRange);

									// Don't override the light's scissor rect
									if (!bDynamicLightingPass)
									{
										// Set the decal scissor rect.
										RHISetScissorRect( TRUE, appTrunc(MinCorner.X), appTrunc(MinCorner.Y), appTrunc(MaxCorner.X), appTrunc(MaxCorner.Y) );
									}

									static const FLinearColor WireColor(0.5f,1.0f,0.5f);
									const INT NumPasses = DrawRichMesh(PDI,Mesh,WireColor,FLinearColor::White,FLinearColor::White,PrimitiveSceneInfo,FALSE);

									INC_DWORD_STAT_BY(STAT_DecalTriangles,Mesh.GetNumPrimitives()*NumPasses);
									INC_DWORD_STAT(STAT_DecalDrawCalls);

									if (!bDynamicLightingPass)
									{
										// Restore the scissor rect.
										RHISetScissorRect( FALSE, 0, 0, 0, 0 );
									}
								}
							}
							else
							{
								// A decal on the edge of terrain may have nothing to render (ie no index buffer).
								continue;
							}
						}
					}
				}
			}
		}
	}
}

/**
 *	Helper function for determining if a given view requires a tessellation check based on distance.
 *
 *	@param	View		The view of interest.
 *	@return	UBOOL		TRUE if it does, FALSE if it doesn't.
 */
UBOOL FTerrainComponentSceneProxy::CheckViewDistance(const FSceneView* View, const FVector& Position, const FVector& MaxMinusMin, const FLOAT ComponentSize)
{
	UBOOL bResult = FALSE;

	//@NOTE: All views/parent views can be processed to pick the 'ideal' tessellation...
	// For now, we are just going to use the parent view if it is present.
	const FSceneView* LocalView = View;
	if (View->ParentViewFamily)
	{
		if ((View->ParentViewIndex != -1) && (View->ParentViewIndex <= View->ParentViewFamily->Views.Num()))
		{
			// If the ParentViewIndex is set to a valid index, use that View
			LocalView = View->ParentViewFamily->Views(View->ParentViewIndex);
		}
		else
		if (View->ParentViewIndex == -1)
		{
			// Iterate over all the Views in the ParentViewFamily
			FSceneView TempView(
				View->Family,
				View->State,
				-1,
				View->ParentViewFamily,
				View->ActorVisibilityHistory,
				View->ViewActor,
				View->PostProcessChain,
				View->PostProcessSettings,
				View->Drawer,
				View->X,
				View->Y,
				View->ClipX,
				View->ClipY,
				View->SizeX,
				View->SizeY,
				View->ViewMatrix,
				View->ProjectionMatrix,
				View->BackgroundColor,
				View->OverlayColor,
				View->ColorScale,
				View->HiddenPrimitives,
				FRenderingPerformanceOverrides(E_ForceInit),
				View->LODDistanceFactor
				);
			for (INT ViewIdx = 0; ViewIdx < View->ParentViewFamily->Views.Num(); ViewIdx++)
			{
				TempView.ParentViewIndex = ViewIdx;
				if (CheckViewDistance(&TempView, Position, MaxMinusMin, ComponentSize) == TRUE)
				{
					bResult = TRUE;
				}
			}

			return bResult;
		}
	}

	// Perform a distance check here...
	FLOAT CheckDistance = FVector(FVector(LocalView->ViewOrigin) - Position).SizeSquared();

	// Always do the component the view is in, as well as its immediate neighbors.
	if ((CheckDistance <= CheckTessellationDistance) || (CheckTessellationDistance == 0.0f))
	{
		bResult = TRUE;
	}
	else
	{
		// Determine the distance outside
		if (CheckTessellationDistance > 0.0f)
		{
			INT CheckCount = Max<INT>(appTrunc(CheckDistance / CheckTessellationDistance), 1);
			if (((CheckTessellationCounter + CheckTessellationOffset) % CheckCount) == 0)
			{
				bResult = TRUE;
			}
		}
	}

	return bResult;
}

/**
 *	Helper function for calculating the tessellation for a given view.
 *
 *	@param	View		The view of interest.
 *	@param	Terrain		The terrain of interest.
 *	@param	bFirstTime	The first time this call was made in a frame.
 */
void FTerrainComponentSceneProxy::ProcessPreRenderView(const FSceneView* View, ATerrain* Terrain, UBOOL bFirstTime)
{
	//@NOTE: All views/parent views can be processed to pick the 'ideal' tessellation...
	// For now, we are just going to use the parent view if it is present.
	const FSceneView* LocalView = View;
	if (View->ParentViewFamily)
	{
		if ((View->ParentViewIndex != -1) && (View->ParentViewIndex <= View->ParentViewFamily->Views.Num()))
		{
			// If the ParentViewIndex is set to a valid index, use that View
			LocalView = View->ParentViewFamily->Views(View->ParentViewIndex);
		}
		else
		if (View->ParentViewIndex == -1)
		{
			// Iterate over all the Views in the ParentViewFamily
			FSceneView TempView(
				View->Family,
				View->State,
				-1,
				View->ParentViewFamily,
				View->ActorVisibilityHistory,
				View->ViewActor,
				View->PostProcessChain,
				View->PostProcessSettings,
				View->Drawer,
				View->X,
				View->Y,
				View->ClipX,
				View->ClipY,
				View->SizeX,
				View->SizeY,
				View->ViewMatrix,
				View->ProjectionMatrix,
				View->BackgroundColor,
				View->OverlayColor,
				View->ColorScale,
				View->HiddenPrimitives,
				FRenderingPerformanceOverrides(E_ForceInit),
				View->LODDistanceFactor
				);
			for (INT ViewIdx = 0; ViewIdx < View->ParentViewFamily->Views.Num(); ViewIdx++)
			{
				TempView.ParentViewIndex = ViewIdx;
				ProcessPreRenderView(&TempView, Terrain, bFirstTime);
				bFirstTime = FALSE;
			}
			return;
		}
	}

	const FTerrainHeight* TerrainHeights = &(Terrain->Heights(0));
	// Setup a vertex buffer/factory for this tessellation level.
	// Determine the tessellation level of each terrain quad.
	INT TerrainOldTessellationLevel;
	FLOAT ZDistance;
	FLOAT Height;
	INT IndexX, IndexY;
	FVector TerrainVector;
	FVector ViewVector;
	FVector4 ViewVector4;

	INT TerrainMaxTessLevel = TerrainObject->GetMaxTessellationLevel();
	INT SectBaseX = TerrainObject->GetComponentSectionBaseX();
	INT SectBaseY = TerrainObject->GetComponentSectionBaseY();
	INT SectSizeX = TerrainObject->GetComponentSectionSizeX();
	INT SectSizeY = TerrainObject->GetComponentSectionSizeY();
	INT NumVertsX = TerrainObject->GetNumVerticesX();
	INT NumVertsY = TerrainObject->GetNumVerticesY();
	FLOAT TessDistScale = TerrainObject->GetTessellationDistanceScale();
	BYTE* TessLevels = TerrainObject->TessellationLevels;
	INT QuadSizeX = TerrainObject->TerrainComponent->TrueSectionSizeX / SectSizeX;
	INT QuadSizeY = TerrainObject->TerrainComponent->TrueSectionSizeY / SectSizeY;
	INT OffsetX = QuadSizeX / 2;
	INT OffsetY = QuadSizeY / 2;

	UBOOL bIsPerspective = (LocalView->ProjectionMatrix.M[3][3] < 1.0f) ? TRUE : FALSE;
	const FMatrix LocalToView = LocalToWorld * LocalView->ViewMatrix;

	// Check the tessellation on it...
	if (TerrainObject->MinTessellationLevel != TerrainObject->MaxTessellationLevel)
	{
		// Calculate the maximum tessellation level for this sector.
		if (bIsPerspective)
		{
			for (INT X = 0; X < 2; X++)
			{
				for (INT Y = 0; Y < 2; Y++)
				{
					for (INT Z = 0; Z < 2; Z++)
					{
						MaxTessellation = Max<INT>(
							MaxTessellation,
							Min<INT>(
								TerrainObject->MaxTessellationLevel,
								TesselationLevel(LocalView->ViewMatrix.TransformFVector(
									FVector(
										PrimitiveSceneInfo->Bounds.GetBoxExtrema(X).X,
										PrimitiveSceneInfo->Bounds.GetBoxExtrema(Y).Y,
										PrimitiveSceneInfo->Bounds.GetBoxExtrema(Z).Z)).Z * 
									TerrainObject->TessellationDistanceScale,
									TerrainObject->MinTessellationLevel)
								)
							);
					}
				}
			}
		}
	}

	INT Index = 0;
	UBOOL bRepackNeeded = FALSE;

	TerrainVector.Y = -1.0f;

	TerrainVector.Y *= QuadSizeY;
	for(INT Y = -1;Y <= SectSizeY;Y++, TerrainVector.Y += QuadSizeY)
	{
		TerrainVector.X = -1.0f * QuadSizeX;
		for(INT X = -1;X <= SectSizeX;X++, TerrainVector.X += QuadSizeX)
		{
			IndexX = SectBaseX + X * QuadSizeX + OffsetX;
			IndexY = SectBaseY + Y * QuadSizeY + OffsetY;
			if ((IndexX >= NumVertsX) || (IndexY >= NumVertsY) || (IndexX < 0) || (IndexY < 0))
			{
				TessLevels[Index++] = TerrainMaxTessLevel;
				continue;
			}
			TerrainOldTessellationLevel = TessLevels[Index];

			// A load-hit-store is inevitable here because Terrain->Height indexes a WORD array which we have to expand to 32-bit before using a conversion function
			ZDistance = 0.0f;

			if (bIsPerspective)
			{
				Height = (FLOAT)(TerrainHeights[IndexY * Terrain->NumVerticesX + IndexX].Value);
				ViewVector4 = LocalToView.TransformFVector4(
					FVector4(
						X * QuadSizeX + OffsetX,
						Y * QuadSizeY + OffsetY,
						(-32768.0f + Height) * TERRAIN_ZSCALE,
						1.0f)
						);
				ZDistance = ViewVector4.Z * TessDistScale;
			}

			if ((GIsEditor == TRUE) && (Abs(ZDistance) < KINDA_SMALL_NUMBER))
			{
				TessLevels[Index++] = (BYTE)(TerrainObject->MinTessellationLevel);
			}
			else
			{
				if (bFirstTime)
				{
					TessLevels[Index++] = (BYTE)Min<UINT>(
						TerrainMaxTessLevel,
						TesselationLevel(ZDistance, TerrainObject->MinTessellationLevel)
						);
				}
				else
				{
					TessLevels[Index] = 
						(BYTE)Max<UINT>(
							TessLevels[Index], 
							(BYTE)Min<UINT>(
								TerrainMaxTessLevel,
								TesselationLevel(ZDistance, TerrainObject->MinTessellationLevel)
								)
							);
					Index++;
				}
			}

			if (TerrainOldTessellationLevel != TessLevels[Index-1])
			{
				bRepackNeeded = TRUE;
			}
			else
			if (TerrainObject->SmoothIndexBuffer)
			{
				if (TerrainObject->SmoothIndexBuffer->CurrentTessellationLevel < TessLevels[Index - 1])
				{
					bRepackNeeded = TRUE;
				}
			}
		}
	}

	if (bRepackNeeded == TRUE)
	{
		TerrainObject->SetRepackRequired(TRUE);
	}
}

/**
 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
 *  Only called for primitives that are visible and have bDynamicRelevance
 *
 *	@param	ViewFamily		The ViewFamily to pre-render for
 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
 *	@param	FrameNumber		The frame number of this pre-render
 */
void FTerrainComponentSceneProxy::PreRenderView(const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber)
{
	// Don't do anything during precaching.
	// This happens when we stream a new level in, and it uses an identity projection matrix which will corrupt tessellation values.
	extern UBOOL GIsCurrentlyPrecaching; 
	if(GIsCurrentlyPrecaching)
	{
		return;
	}

	// It is 'safe' to grab the owner in the case of terrain, as the owner will
	// not FinishDestroy until the render-thread has been flushed.
	ATerrain* Terrain = Cast<ATerrain>(Owner);
	check(Terrain);

	UBOOL bForceRepack = FALSE;
	UBOOL bCheckTessellation = FALSE;
	UBOOL bFirstTime = FALSE;
	// If it is the first call of the frame...
	if (FrameNumber > LastTessellationCheck)
	{
		// Reset the max tessellation ONLY on the first call of the frame.
		MaxTessellation = TerrainObject->MinTessellationLevel;

		bFirstTime = TRUE;
	}

	// Increment the check tessellation counter, but only on the first call in a single frame.
	if (FrameNumber != LastTessellationCheck)
	{
		CheckTessellationCounter++;
	}

	// Check if we HAVE to check the tessellation
	if ((LastTessellationCheck < 0) || GIsEditor || 
		(TerrainObject->SmoothIndexBuffer->RepackRequired == TRUE) || 
		(TerrainObject->SmoothIndexBuffer->NumTriangles == 0))
	{
		bForceRepack = TRUE;
		bCheckTessellation = TRUE;
	}
	else
	if (PrimitiveSceneInfo->LastRenderTime == -FLT_MAX)
	{
		// Has not been rendered yet... ensure that it is repacked!
		bForceRepack = TRUE;
		bCheckTessellation = TRUE;
	}
	else
	if ((PrimitiveSceneInfo->LastVisibilityChangeTime - TrackedLastVisibilityChangeTime) > 0.033f)
	{
		// Visibility has changed, so check tessellation
		bForceRepack = TRUE;
		bCheckTessellation = TRUE;
	}

	// In the editor, we need to check the tessellation every frame.
	// Otherwise, check the distance if we are NOT being forced to check tessellation.
	if (GIsGame && !bCheckTessellation)
	{
		FVector Position = (PrimitiveSceneInfo->Bounds.GetBoxExtrema(0) + PrimitiveSceneInfo->Bounds.GetBoxExtrema(1)) / 2.0f;
		FVector MaxMinusMin = PrimitiveSceneInfo->Bounds.GetBoxExtrema(1) - PrimitiveSceneInfo->Bounds.GetBoxExtrema(0);
		FLOAT ComponentSize = MaxMinusMin.SizeSquared();

		for (INT ViewIndex = 0; (ViewIndex < ViewFamily->Views.Num()) && !bCheckTessellation; ViewIndex++)
		{
			if (CheckViewDistance(ViewFamily->Views(ViewIndex), Position, MaxMinusMin, ComponentSize) == TRUE)
			{
				bCheckTessellation = TRUE;
			}
		}
	}
	else
	{
		bCheckTessellation = TRUE;
	}

	if (bCheckTessellation)
	{
		// Determine the tessellation level of each terrain quad.
		for (INT ViewIndex = 0; ViewIndex < ViewFamily->Views.Num(); ViewIndex++)
		{
			ProcessPreRenderView(ViewFamily->Views(ViewIndex), Terrain, bFirstTime);
			bFirstTime = FALSE;
		}
	}

	if (bForceRepack)
	{
		TerrainObject->SetRepackRequired(TRUE);
		TerrainObject->VertexBuffer->ForceRepackRequired();
		TerrainObject->SmoothIndexBuffer->RepackRequired = TRUE;
	}

	LastTessellationCheck = FrameNumber;
	TrackedLastVisibilityChangeTime = PrimitiveSceneInfo->LastVisibilityChangeTime;
}

void FTerrainComponentSceneProxy::UpdateData(UTerrainComponent* Component)
{
    FTerrainMaterialInfo* NewMaterialInfo = ::new FTerrainMaterialInfo(Component);

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		TerrainUpdateDataCommand,
		FTerrainComponentSceneProxy*, Proxy, this,
		FTerrainMaterialInfo*, NewMaterialInfo, NewMaterialInfo,
		{
			Proxy->UpdateData_RenderThread(NewMaterialInfo);
		}
		);
}

void FTerrainComponentSceneProxy::UpdateData_RenderThread(FTerrainMaterialInfo* NewMaterialInfo)
{
	// We must be done with it...
	delete CurrentMaterialInfo;

	// Set the new one
	CurrentMaterialInfo = NewMaterialInfo;

	FTerrainBatchInfo* BatchInfo = &CurrentMaterialInfo->BatchInfo;
		FMaterialRenderProxy* MaterialRenderProxy = BatchInfo->MaterialRenderProxy;
		if (MaterialRenderProxy && BatchInfo && (BatchInfo->bIsTerrainMaterialResourceInstance == TRUE))
		{
			FTerrainMaterialResource* TWeightInst = (FTerrainMaterialResource*)MaterialRenderProxy;
			TWeightInst->WeightMaps.Empty(BatchInfo->WeightMaps.Num());
			TWeightInst->WeightMaps.Add(BatchInfo->WeightMaps.Num());
			for (INT WeightMapIndex = 0; WeightMapIndex < BatchInfo->WeightMaps.Num(); WeightMapIndex++)
			{
				UTexture2D* NewWeightMap = BatchInfo->WeightMaps(WeightMapIndex);
				TWeightInst->WeightMaps(WeightMapIndex) = NewWeightMap;
				FName MapName(*(FString::Printf(TEXT("TWeightMap%d"), WeightMapIndex)));
				TWeightInst->WeightMapsMap.Set(MapName, NewWeightMap);
			}
		}
}
