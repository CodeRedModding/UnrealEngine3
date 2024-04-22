/*=============================================================================
	MaterialShader.h: Material shader definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "DiagnosticTable.h"

//
// Globals
//
TMap<FStaticParameterSet,FMaterialShaderMap*> FMaterialShaderMap::GIdToMaterialShaderMap[SP_NumPlatforms];
// The Id of 0 is reserved for global shaders
UINT FMaterialShaderMap::NextCompilingId = 1;
/** Tracks material resources and their shader maps that need to be compiled but whose compilation is being deferred. */
TMap<FMaterialShaderMap*, TArray<FMaterial*> > FMaterialShaderMap::ShaderMapsBeingCompiled;
/** 
 * Map from shader map CompilingId to an ANSI version of the material's shader code.
 * This is stored outside of the shader's environment includes to reduce memory usage, since many shaders share the same material shader code.
 */
TMap<UINT, const ANSICHAR*> FMaterialShaderMap::MaterialCodeBeingCompiled;

/** Converts an EMaterialLightingModel to a string description. */
FString GetLightingModelString(EMaterialLightingModel LightingModel)
{
	FString LightingModelName;
	switch(LightingModel)
	{
		case MLM_Phong: LightingModelName = TEXT("MLM_Phong"); break;
		case MLM_NonDirectional: LightingModelName = TEXT("MLM_NonDirectional"); break;
		case MLM_Unlit: LightingModelName = TEXT("MLM_Unlit"); break;
		case MLM_SHPRT: LightingModelName = TEXT("MLM_SHPRT"); break;
		case MLM_Custom: LightingModelName = TEXT("MLM_Custom"); break;
		default: LightingModelName = TEXT("Unknown"); break;
	}
	return LightingModelName;
}

/** Converts an EBlendMode to a string description. */
FString GetBlendModeString(EBlendMode BlendMode)
{
	FString BlendModeName;
	switch(BlendMode)
	{
		case BLEND_Opaque: BlendModeName = TEXT("BLEND_Opaque"); break;
		case BLEND_Masked: BlendModeName = TEXT("BLEND_Masked"); break;
		case BLEND_Translucent: BlendModeName = TEXT("BLEND_Translucent"); break;
		case BLEND_Additive: BlendModeName = TEXT("BLEND_Additive"); break;
		case BLEND_Modulate: BlendModeName = TEXT("BLEND_Modulate"); break;
		case BLEND_ModulateAndAdd: BlendModeName = TEXT("BLEND_ModulateAndAdd"); break;
		case BLEND_SoftMasked: BlendModeName = TEXT("BLEND_SoftMasked"); break;
        case BLEND_AlphaComposite: BlendModeName = TEXT("BLEND_AlphaComposite"); break;
		case BLEND_DitheredTranslucent: BlendModeName = TEXT("BLEND_DitheredTranslucent"); break;
		default: BlendModeName = TEXT("Unknown"); break;
	}
	return BlendModeName;
}

/** Called for every material shader to update the appropriate stats. */
void UpdateMaterialShaderCompilingStats(const FMaterial* Material)
{
	INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumTotalMaterialShaders,1);

	switch(Material->GetBlendMode())
	{
		case BLEND_Opaque: INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumOpaqueMaterialShaders,1); break;
		case BLEND_Masked: 
		case BLEND_SoftMasked:
		case BLEND_DitheredTranslucent: INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumMaskedMaterialShaders,1); break;
		default: INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumTransparentMaterialShaders,1); break;
	}

	switch(Material->GetLightingModel())
	{
		case MLM_Phong: INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumLitMaterialShaders,1); break;
		case MLM_Unlit: INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumUnlitMaterialShaders,1); break;
		default: break;
	};

	if (Material->IsSpecialEngineMaterial())
	{
		INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumSpecialMaterialShaders,1);
	}
	if (Material->IsTerrainMaterial())
	{
		INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumTerrainMaterialShaders,1);
	}
	if (Material->IsUsedWithDecals())
	{
		INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumDecalMaterialShaders,1);
	}
	if (Material->IsUsedWithParticleSystem())
	{
		INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumParticleMaterialShaders,1);
	}
	if (Material->IsUsedWithSkeletalMesh())
	{
		INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumSkinnedMaterialShaders,1);
	}
}

/** 
* Indicates whether two static parameter sets are unequal.  This takes into account parameter override settings.
* 
* @param ReferenceSet	The set to compare against
* @return				TRUE if the sets are not equal
*/
UBOOL FStaticParameterSet::ShouldMarkDirty(const FStaticParameterSet* ReferenceSet)
{
	if (ReferenceSet->StaticSwitchParameters.Num() != StaticSwitchParameters.Num()
		|| ReferenceSet->StaticComponentMaskParameters.Num() != StaticComponentMaskParameters.Num()
		|| ReferenceSet->NormalParameters.Num() != NormalParameters.Num()
		|| ReferenceSet->TerrainLayerWeightParameters.Num() !=TerrainLayerWeightParameters.Num())
	{
		return TRUE;
	}

	//switch parameters
	for (INT RefParamIndex = 0;RefParamIndex < ReferenceSet->StaticSwitchParameters.Num();RefParamIndex++)
	{
		const FStaticSwitchParameter * ReferenceSwitchParameter = &ReferenceSet->StaticSwitchParameters(RefParamIndex);
		for (INT ParamIndex = 0;ParamIndex < StaticSwitchParameters.Num();ParamIndex++)
		{
			FStaticSwitchParameter * SwitchParameter = &StaticSwitchParameters(ParamIndex);
			if (SwitchParameter->ParameterName == ReferenceSwitchParameter->ParameterName
				&& SwitchParameter->ExpressionGUID == ReferenceSwitchParameter->ExpressionGUID)
			{
				SwitchParameter->bOverride = ReferenceSwitchParameter->bOverride;
				if (SwitchParameter->Value != ReferenceSwitchParameter->Value)
				{
					return TRUE;
				}
			}
		}
	}

	//component mask parameters
	for (INT RefParamIndex = 0;RefParamIndex < ReferenceSet->StaticComponentMaskParameters.Num();RefParamIndex++)
	{
		const FStaticComponentMaskParameter * ReferenceComponentMaskParameter = &ReferenceSet->StaticComponentMaskParameters(RefParamIndex);
		for (INT ParamIndex = 0;ParamIndex < StaticComponentMaskParameters.Num();ParamIndex++)
		{
			FStaticComponentMaskParameter * ComponentMaskParameter = &StaticComponentMaskParameters(ParamIndex);
			if (ComponentMaskParameter->ParameterName == ReferenceComponentMaskParameter->ParameterName
				&& ComponentMaskParameter->ExpressionGUID == ReferenceComponentMaskParameter->ExpressionGUID)
			{
				ComponentMaskParameter->bOverride = ReferenceComponentMaskParameter->bOverride;
				if (ComponentMaskParameter->R != ReferenceComponentMaskParameter->R
					|| ComponentMaskParameter->G != ReferenceComponentMaskParameter->G
					|| ComponentMaskParameter->B != ReferenceComponentMaskParameter->B
					|| ComponentMaskParameter->A != ReferenceComponentMaskParameter->A)
				{
					return TRUE;
				}
			}
		}
	}

	// normal parameters
	for (INT RefParamIndex = 0;RefParamIndex < ReferenceSet->NormalParameters.Num();RefParamIndex++)
	{
		const FNormalParameter * ReferenceNormalParameter  = &ReferenceSet->NormalParameters(RefParamIndex);
		for (INT ParamIndex = 0;ParamIndex < NormalParameters.Num();ParamIndex++)
		{
			FNormalParameter * NormalParameter = &NormalParameters(ParamIndex);
			if (NormalParameter->ParameterName == ReferenceNormalParameter ->ParameterName
				&& NormalParameter->ExpressionGUID == ReferenceNormalParameter ->ExpressionGUID)
			{
				NormalParameter->bOverride = ReferenceNormalParameter ->bOverride;
				if (NormalParameter->CompressionSettings != ReferenceNormalParameter->CompressionSettings)
				{
					return TRUE;
				}
			}
		}
	}

	// Terrain layer weight parameters
	for (INT RefParamIndex = 0;RefParamIndex < ReferenceSet->TerrainLayerWeightParameters.Num();RefParamIndex++)
	{
		const FStaticTerrainLayerWeightParameter * ReferenceTerrainLayerWeightParameter  = &ReferenceSet->TerrainLayerWeightParameters(RefParamIndex);
		for (INT ParamIndex = 0;ParamIndex < TerrainLayerWeightParameters.Num();ParamIndex++)
		{
			FStaticTerrainLayerWeightParameter * TerrainLayerWeightParameter = &TerrainLayerWeightParameters(ParamIndex);
			if (TerrainLayerWeightParameter->ParameterName == ReferenceTerrainLayerWeightParameter ->ParameterName
				&& TerrainLayerWeightParameter->ExpressionGUID == ReferenceTerrainLayerWeightParameter ->ExpressionGUID)
			{
				TerrainLayerWeightParameter->bOverride = ReferenceTerrainLayerWeightParameter ->bOverride;
				if (TerrainLayerWeightParameter->WeightmapIndex != ReferenceTerrainLayerWeightParameter->WeightmapIndex)
				{
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

FString FStaticParameterSet::GetSummaryString() const
{
	return FString::Printf(TEXT("(Base Guid %s, %u switches, %u masks, %u normal params, %u terrain layer weight params)"),
		*BaseMaterialId.String(),
		StaticSwitchParameters.Num(),
		StaticComponentMaskParameters.Num(),
		NormalParameters.Num(),
		TerrainLayerWeightParameters.Num()
		);
}

/** 
* Tests this set against another for equality, disregarding override settings.
* 
* @param ReferenceSet	The set to compare against
* @return				TRUE if the sets are equal
*/
UBOOL FStaticParameterSet::operator==(const FStaticParameterSet &ReferenceSet) const
{
	if (BaseMaterialId == ReferenceSet.BaseMaterialId)
	{
		if (StaticSwitchParameters.Num() == ReferenceSet.StaticSwitchParameters.Num()
			&& StaticComponentMaskParameters.Num() == ReferenceSet.StaticComponentMaskParameters.Num()
			&& NormalParameters.Num() == ReferenceSet.NormalParameters.Num() 
			&& TerrainLayerWeightParameters.Num() == ReferenceSet.TerrainLayerWeightParameters.Num() )
		{
			for (INT SwitchIndex = 0; SwitchIndex < StaticSwitchParameters.Num(); SwitchIndex++)
			{
				if (StaticSwitchParameters(SwitchIndex).ParameterName != ReferenceSet.StaticSwitchParameters(SwitchIndex).ParameterName
					|| StaticSwitchParameters(SwitchIndex).ExpressionGUID != ReferenceSet.StaticSwitchParameters(SwitchIndex).ExpressionGUID
					|| StaticSwitchParameters(SwitchIndex).Value != ReferenceSet.StaticSwitchParameters(SwitchIndex).Value)
				{
					return FALSE;
				}
			}

			for (INT ComponentMaskIndex = 0; ComponentMaskIndex < StaticComponentMaskParameters.Num(); ComponentMaskIndex++)
			{
				if (StaticComponentMaskParameters(ComponentMaskIndex).ParameterName != ReferenceSet.StaticComponentMaskParameters(ComponentMaskIndex).ParameterName
					|| StaticComponentMaskParameters(ComponentMaskIndex).ExpressionGUID != ReferenceSet.StaticComponentMaskParameters(ComponentMaskIndex).ExpressionGUID
					|| StaticComponentMaskParameters(ComponentMaskIndex).R != ReferenceSet.StaticComponentMaskParameters(ComponentMaskIndex).R
					|| StaticComponentMaskParameters(ComponentMaskIndex).G != ReferenceSet.StaticComponentMaskParameters(ComponentMaskIndex).G
					|| StaticComponentMaskParameters(ComponentMaskIndex).B != ReferenceSet.StaticComponentMaskParameters(ComponentMaskIndex).B
					|| StaticComponentMaskParameters(ComponentMaskIndex).A != ReferenceSet.StaticComponentMaskParameters(ComponentMaskIndex).A)
				{
					return FALSE;
				}
			}

			for (INT NormalIndex = 0; NormalIndex < NormalParameters.Num(); NormalIndex++)
			{
				if (NormalParameters(NormalIndex).ParameterName != ReferenceSet.NormalParameters(NormalIndex).ParameterName
					|| NormalParameters(NormalIndex).ExpressionGUID != ReferenceSet.NormalParameters(NormalIndex).ExpressionGUID
					|| NormalParameters(NormalIndex).CompressionSettings != ReferenceSet.NormalParameters(NormalIndex).CompressionSettings)
				{
					return FALSE;
				}
			}

			for (INT TerrainLayerWeightIndex = 0; TerrainLayerWeightIndex < TerrainLayerWeightParameters.Num(); TerrainLayerWeightIndex++)
			{
				if (TerrainLayerWeightParameters(TerrainLayerWeightIndex).ParameterName != ReferenceSet.TerrainLayerWeightParameters(TerrainLayerWeightIndex).ParameterName
					|| TerrainLayerWeightParameters(TerrainLayerWeightIndex).ExpressionGUID != ReferenceSet.TerrainLayerWeightParameters(TerrainLayerWeightIndex).ExpressionGUID
					|| TerrainLayerWeightParameters(TerrainLayerWeightIndex).WeightmapIndex != ReferenceSet.TerrainLayerWeightParameters(TerrainLayerWeightIndex).WeightmapIndex)
				{
					return FALSE;
				}
			}

			return TRUE;
		}
	}
	return FALSE;
}

#if PLATFORM_SUPPORTS_D3D10_PLUS
/** A texture that contains sets of stratified translucency samples. */
template<UINT NumSamples,UINT NumSets>
class TStratifiedTranslucencySampleTexture : public FTextureResource
{
public:
	// FResource interface.
	virtual void InitRHI()
	{
		// Create the texture RHI.
		FTexture2DRHIRef Texture2D = RHICreateTexture2D(NumSamples,NumSets,PF_R32F,1,TexCreate_Uncooked,NULL);
		TextureRHI = Texture2D;

		// Write the contents of the texture.
		UINT DestStride;
		BYTE* DestBuffer = (BYTE*)RHILockTexture2D(Texture2D,0,TRUE,DestStride,FALSE);

		for(UINT SetIndex = 0;SetIndex < NumSets;++SetIndex)
		{
			UINT Samples[NumSamples];
			for(UINT SampleIndex = 0;SampleIndex < NumSamples;++SampleIndex)
			{
				UBOOL bIsUniqueSample;
				do 
				{
					Samples[SampleIndex] = appRand() % NumSamples;

					bIsUniqueSample = TRUE;
					for(UINT OtherSampleIndex = 0;OtherSampleIndex < SampleIndex;++OtherSampleIndex)
					{
						if(Samples[OtherSampleIndex] == Samples[SampleIndex])
						{
							bIsUniqueSample = FALSE;
							break;
						}
					}
				}
				while(!bIsUniqueSample);
			}

			FLOAT* SetSampleFractions = (FLOAT*)(DestBuffer + DestStride * SetIndex);
			for(UINT SampleIndex = 0;SampleIndex < NumSamples;++SampleIndex)
			{
				SetSampleFractions[SampleIndex]  = (FLOAT)(Samples[SampleIndex] + appFrand()) / (FLOAT)NumSamples;
			}
		}

		RHIUnlockTexture2D(Texture2D,0,FALSE);

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point,AM_Wrap,AM_Wrap,AM_Wrap);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
	}

	/** Returns the width of the texture in pixels. */
	virtual UINT GetSizeX() const
	{
		return NumSamples;
	}

	/** Returns the height of the texture in pixels. */
	virtual UINT GetSizeY() const
	{
		return NumSets;
	}
};

FTexture* GStratifiedTranslucencySampleTexture = new TGlobalResource<TStratifiedTranslucencySampleTexture<4,16> >;
#endif

FArchive& operator<<(FArchive& Ar,FMaterialShaderParameters& Parameters)
{
	Ar << Parameters.CameraWorldPositionParameter;
	Ar << Parameters.TemporalAAParameters;
	Ar << Parameters.ObjectWorldPositionAndRadiusParameter;
	Ar << Parameters.ObjectOrientationParameter;
	Ar << Parameters.WindDirectionAndSpeedParameter;
	Ar << Parameters.FoliageImpulseDirectionParameter;
	Ar << Parameters.FoliageNormalizedRotationAxisAndAngleParameter;
	Ar << Parameters.UniformScalarShaderParameters;
	Ar << Parameters.UniformVectorShaderParameters;
	Ar << Parameters.Uniform2DShaderResourceParameters;
	Ar << Parameters.LocalToWorldParameter;
	Ar << Parameters.WorldToLocalParameter;
	Ar << Parameters.WorldToViewParameter;
	Ar << Parameters.ViewToWorldParameter;
	Ar << Parameters.InvViewProjectionParameter;
	Ar << Parameters.ViewProjectionParameter;
	Ar << Parameters.ActorWorldPositionParameter;
	return Ar;
}

void FMaterialShaderParameters::Bind(const FShaderParameterMap& ParameterMap, EShaderFrequency Frequency)
{
	// only used if Material has a Transform expression 
	LocalToWorldParameter.Bind(ParameterMap,TEXT("LocalToWorldMatrix"),TRUE);
	WorldToLocalParameter.Bind(ParameterMap,TEXT("WorldToLocalMatrix"),TRUE);
	WorldToViewParameter.Bind(ParameterMap,TEXT("WorldToViewMatrix"),TRUE);
	ViewToWorldParameter.Bind(ParameterMap,TEXT("ViewToWorldMatrix"),TRUE);
	InvViewProjectionParameter.Bind(ParameterMap,TEXT("InvViewProjectionMatrix"),TRUE);
	ViewProjectionParameter.Bind(ParameterMap,TEXT("ViewProjectionMatrix"),TRUE);

	CameraWorldPositionParameter.Bind(ParameterMap,TEXT("CameraWorldPos"),TRUE);
	TemporalAAParameters.Bind(ParameterMap,TEXT("TemporalAAParameters"),TRUE);
	ObjectWorldPositionAndRadiusParameter.Bind(ParameterMap, TEXT("ObjectWorldPositionAndRadius"),TRUE);
	/* owning actor's world position */
	ActorWorldPositionParameter.Bind(ParameterMap, TEXT("ActorWorldPos"),TRUE);
	ObjectOrientationParameter.Bind(ParameterMap, TEXT("ObjectOrientation"),TRUE);
	WindDirectionAndSpeedParameter.Bind(ParameterMap, TEXT("WindDirectionAndSpeed"),TRUE);
	FoliageImpulseDirectionParameter.Bind(ParameterMap, TEXT("FoliageImpulseDirection"),TRUE);
	FoliageNormalizedRotationAxisAndAngleParameter.Bind(ParameterMap, TEXT("FoliageNormalizedRotationAxisAndAngle"),TRUE);

	const TCHAR* ShaderFrequencyName = GetShaderFrequencyName(Frequency);
	const FShaderFrequencyUniformExpressions& ShaderUniformExpressions = ParameterMap.UniformExpressionSet->GetExpresssions(Frequency);
	// Bind uniform scalar expression parameters.
	for(INT ParameterIndex = 0;ParameterIndex < ShaderUniformExpressions.UniformScalarExpressions.Num();ParameterIndex += 4)
	{
		FShaderParameter ShaderParameter;
		FString ParameterName = FString::Printf(TEXT("Uniform%sScalars_%u"), ShaderFrequencyName, ParameterIndex / 4);
		ShaderParameter.Bind(ParameterMap,*ParameterName,TRUE);
		if(ShaderParameter.IsBound())
		{
			TUniformParameter<FShaderParameter>* UniformParameter = new(UniformScalarShaderParameters) TUniformParameter<FShaderParameter>();
			UniformParameter->Index = ParameterIndex / 4;
			UniformParameter->ShaderParameter = ShaderParameter;
		}
	}

	// Bind uniform vector expression parameters.
	for(INT ParameterIndex = 0;ParameterIndex < ShaderUniformExpressions.UniformVectorExpressions.Num();ParameterIndex++)
	{
		FShaderParameter ShaderParameter;
		FString ParameterName = FString::Printf(TEXT("Uniform%sVector_%u"), ShaderFrequencyName, ParameterIndex);
		ShaderParameter.Bind(ParameterMap,*ParameterName,TRUE);
		if(ShaderParameter.IsBound())
		{
			TUniformParameter<FShaderParameter>* UniformParameter = new(UniformVectorShaderParameters) TUniformParameter<FShaderParameter>();
			UniformParameter->Index = ParameterIndex;
			UniformParameter->ShaderParameter = ShaderParameter;
		}
	}

	// Bind uniform 2D texture parameters.
	for(INT ParameterIndex = 0;ParameterIndex < ShaderUniformExpressions.Uniform2DTextureExpressions.Num();ParameterIndex++)
	{
		FShaderResourceParameter ShaderParameter;
		FString ParameterName = FString::Printf(TEXT("%sTexture2D_%u"), ShaderFrequencyName, ParameterIndex);
		ShaderParameter.Bind(ParameterMap,*ParameterName,TRUE);
		if(ShaderParameter.IsBound())
		{
			TUniformParameter<FShaderResourceParameter>* UniformParameter = new(Uniform2DShaderResourceParameters) TUniformParameter<FShaderResourceParameter>();
			UniformParameter->Index = ParameterIndex;
			UniformParameter->ShaderParameter = ShaderParameter;
		}
	}

	DOFParameters.Bind(ParameterMap);
}

/** Sets shader parameters that are material specific but not FMeshBatch specific. */
template<typename ShaderRHIParamRef>
void FMaterialShaderParameters::SetShader(
	const ShaderRHIParamRef ShaderRHI, 
	const FShaderFrequencyUniformExpressions& InExpressions, 
	const FMaterialRenderContext& MaterialRenderContext,
	FShaderFrequencyUniformExpressionValues& InValues) const
{
	// Set the uniform parameters.
	FShaderFrequencyUniformExpressionValues NewValues;
	FShaderFrequencyUniformExpressionValues* CachedValues;
	if (MaterialRenderContext.bAllowUniformParameterCaching)
	{
		// Update the cache
		InValues.Update(InExpressions, MaterialRenderContext, !MaterialRenderContext.MaterialRenderProxy->bCacheable);
		CachedValues = &InValues;
	}
	else 
	{
		// Update the temporary values since we are not caching
		NewValues.Update(InExpressions, MaterialRenderContext, TRUE);
		CachedValues = &NewValues;
	}

	for (INT ParameterIndex = 0; ParameterIndex < UniformScalarShaderParameters.Num(); ParameterIndex++)
	{
		const TUniformParameter<FShaderParameter>& UniformParameter = UniformScalarShaderParameters(ParameterIndex);
		if (UniformParameter.Index >= (InExpressions.UniformScalarExpressions.Num() + 3) / 4)
		{
			continue;
		}
		const FVector4& Value = CachedValues->CachedScalarParameters(UniformParameter.Index);
		SetShaderValue(ShaderRHI,UniformParameter.ShaderParameter,Value);
	}

	for (INT ParameterIndex = 0; ParameterIndex < UniformVectorShaderParameters.Num(); ParameterIndex++)
	{
		const TUniformParameter<FShaderParameter>& UniformParameter = UniformVectorShaderParameters(ParameterIndex);
		if (UniformParameter.Index >= InExpressions.UniformVectorExpressions.Num())
		{
			continue;
		}
		const FVector4& Value = CachedValues->CachedVectorParameters(UniformParameter.Index);
		SetShaderValue(ShaderRHI,UniformParameter.ShaderParameter,Value);
	}

#if WITH_MOBILE_RHI
	if( !GUsingMobileRHI )
#endif
	{
		for(INT ParameterIndex = 0;ParameterIndex < Uniform2DShaderResourceParameters.Num();ParameterIndex++)
		{
			const TUniformParameter<FShaderResourceParameter>& UniformResourceParameter = Uniform2DShaderResourceParameters(ParameterIndex);
			if (UniformResourceParameter.Index >= InExpressions.Uniform2DTextureExpressions.Num())
			{
				continue;
			}
			const FTexture* Value = CachedValues->CachedTexture2DParameters(UniformResourceParameter.Index);
			checkSlow(Value);
			const FLOAT MipBias = Value->MipBiasFade.CalcMipBias();
			// Set the min mip level to 3 if we are told to work around deferred mip artifacts
			// Textures with mip maps in deferred passes cause problems because the GPU picks a very low mip at large depth discontinuities, 
			// Which manifests as a one pixel line around foreground objects.  Disallowing the lower mips works around this.
			const INT MinMipLevel = MaterialRenderContext.bWorkAroundDeferredMipArtifacts ? 3 : -1;
			// Also force linear filtering on the min filter if we are working around deferred mip artifacts,
			// Because anisotropic filtering doesn't respect min mip level (at least on Xbox 360)
			SetTextureParameter(ShaderRHI,UniformResourceParameter.ShaderParameter,Value,0,MipBias,-1,MinMipLevel,MaterialRenderContext.bWorkAroundDeferredMipArtifacts);
		}
	}


	// set view matrix for use by view space Transform expressions
	// Note because the shader uses only the rotation component we can safely use ViewMatrix instead of TranslatedViewMatrix
	SetShaderValue(ShaderRHI,WorldToViewParameter,MaterialRenderContext.View->ViewMatrix);
	SetShaderValue(ShaderRHI,ViewToWorldParameter,MaterialRenderContext.View->InvViewMatrix);


	// set camera world position
	SetShaderValue(ShaderRHI,CameraWorldPositionParameter,MaterialRenderContext.View->ViewOrigin);
}

/**
* Set the material shader parameters which depend on the mesh element being rendered.
* @param Shader - The shader to set the parameters for.
* @param View - The view that is being rendered.
* @param Mesh - The mesh that is being rendered.
*/
template<typename ShaderRHIParamRef>
void FMaterialShaderParameters::SetMeshShader(
	const ShaderRHIParamRef& Shader,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FMeshBatch& Mesh,
	INT BatchElementIndex,
	const FSceneView& View
	) const
{
	const FMeshBatchElement& BatchElement = Mesh.Elements(BatchElementIndex);
	if (PrimitiveSceneInfo)
	{
		/* Owning actor world position */
		if (ActorWorldPositionParameter.IsBound() )
		{
			if( PrimitiveSceneInfo->Owner != NULL )
			{
				SetShaderValue(Shader,ActorWorldPositionParameter,FVector(PrimitiveSceneInfo->Owner->Location));
			}
			else
			{
				SetShaderValue(Shader,ActorWorldPositionParameter,FVector(0.f));
			}
		}

		if (ObjectWorldPositionAndRadiusParameter.IsBound())
		{
			SetShaderValue(Shader,ObjectWorldPositionAndRadiusParameter,FVector4(PrimitiveSceneInfo->Bounds.Origin, PrimitiveSceneInfo->Bounds.SphereRadius));
		}
		
		if (TemporalAAParameters.IsBound())
		{
			const UBOOL bAllowTemporalAA = View.bRenderTemporalAA
				&& View.ViewProjectionMatrix.TransformFVector(PrimitiveSceneInfo->Bounds.Origin).Z - PrimitiveSceneInfo->Bounds.SphereRadius > View.TemporalAAParameters.StartDepth
				&& !PrimitiveSceneInfo->bMovable
				&& !Mesh.IsTranslucent();

			SetShaderValue(Shader, TemporalAAParameters, FVector(View.TemporalAAParameters.Offset.X, View.TemporalAAParameters.Offset.Y, bAllowTemporalAA ? 1.0f : 0.0f));
		}

		if (ObjectOrientationParameter.IsBound())
		{
			// Set the object orientation parameter as the local space up vector, in world space
			SetShaderValue(Shader,ObjectOrientationParameter,BatchElement.LocalToWorld.GetAxis(2).SafeNormal());
		}
		
		if (WindDirectionAndSpeedParameter.IsBound())
		{
			SetShaderValue(Shader,WindDirectionAndSpeedParameter,PrimitiveSceneInfo->Scene->GetWindParameters(PrimitiveSceneInfo->Bounds.Origin));
		}

		if (FoliageImpulseDirectionParameter.IsBound() || FoliageNormalizedRotationAxisAndAngleParameter.IsBound())
		{
			FVector FoliageImpluseDirection;
			FVector4 FoliageNormalizedRotationAxisAndAngle;
			PrimitiveSceneInfo->Proxy->GetFoliageParameters(FoliageImpluseDirection, FoliageNormalizedRotationAxisAndAngle);
			SetShaderValue(Shader,FoliageImpulseDirectionParameter,FoliageImpluseDirection);
			SetShaderValue(Shader,FoliageNormalizedRotationAxisAndAngleParameter,FoliageNormalizedRotationAxisAndAngle);
		}
	}
	else
	{
		SetShaderValue(Shader, TemporalAAParameters, FVector(0, 0, 0));
	}

	// set world matrix for use by world/view space Transform expressions
	SetShaderValue(Shader,LocalToWorldParameter,BatchElement.LocalToWorld);
	// set world to local matrix used by Transform expressions
	SetShaderValue(Shader,WorldToLocalParameter,BatchElement.WorldToLocal);
}

UBOOL FMaterialShaderParameters::IsUniformExpressionSetValid(const FShaderFrequencyUniformExpressions& UniformExpressions) const
{
	for (INT ParameterIndex = 0; ParameterIndex < UniformScalarShaderParameters.Num(); ParameterIndex++)
	{
		const TUniformParameter<FShaderParameter>& UniformParameter = UniformScalarShaderParameters(ParameterIndex);
		if (UniformParameter.Index >= (UniformExpressions.UniformScalarExpressions.Num() + 3) / 4)
		{
			return FALSE;
		}
	}

	for (INT ParameterIndex = 0; ParameterIndex < UniformVectorShaderParameters.Num(); ParameterIndex++)
	{
		const TUniformParameter<FShaderParameter>& UniformParameter = UniformVectorShaderParameters(ParameterIndex);
		if (UniformParameter.Index >= UniformExpressions.UniformVectorExpressions.Num())
		{
			return FALSE;
		}
	}

	for(INT ParameterIndex = 0;ParameterIndex < Uniform2DShaderResourceParameters.Num();ParameterIndex++)
	{
		const TUniformParameter<FShaderResourceParameter>& UniformResourceParameter = Uniform2DShaderResourceParameters(ParameterIndex);
		if (UniformResourceParameter.Index >= UniformExpressions.Uniform2DTextureExpressions.Num())
		{
			return FALSE;
		}
	}
	
	return TRUE;
}

void FMaterialPixelShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	FMaterialShaderParameters::Bind(ParameterMap, SF_Pixel);

	// Bind uniform cube texture parameters.
	for(INT ParameterIndex = 0;ParameterIndex < ParameterMap.UniformExpressionSet->UniformCubeTextureExpressions.Num();ParameterIndex++)
	{
		FShaderResourceParameter ShaderParameter;
		FString ParameterName = FString::Printf(TEXT("PixelTextureCube_%u"),ParameterIndex);
		ShaderParameter.Bind(ParameterMap,*ParameterName,TRUE);
		if(ShaderParameter.IsBound())
		{
			TUniformParameter<FShaderResourceParameter>* UniformParameter = new(UniformPixelCubeShaderResourceParameters) TUniformParameter<FShaderResourceParameter>();
			UniformParameter->Index = ParameterIndex;
			UniformParameter->ShaderParameter = ShaderParameter;
		}
	}

	SceneTextureParameters.Bind(ParameterMap);

	// Only used for two-sided materials.
	TwoSidedSignParameter.Bind(ParameterMap,TEXT("TwoSidedSign"),TRUE);
	// Only used when material needs gamma correction
	InvGammaParameter.Bind(ParameterMap,TEXT("MatInverseGamma"),TRUE);
	// Only used for decal materials
	DecalNearFarPlaneDistanceParameter.Bind(ParameterMap,TEXT("DecalNearFarPlaneDistance"),TRUE);	
	ObjectPostProjectionPositionParameter.Bind(ParameterMap, TEXT("ObjectPostProjectionPosition"),TRUE);
	ObjectMacroUVScalesParameter.Bind(ParameterMap, TEXT("ObjectMacroUVScales"),TRUE);
	ObjectNDCPositionParameter.Bind(ParameterMap, TEXT("ObjectNDCPosition"),TRUE);
	OcclusionPercentageParameter.Bind(ParameterMap, TEXT("OcclusionPercentage"), TRUE);

	// Used for all material shaders that set MATERIAL_USE_SCREEN_DOOR_FADE to 1
	EnableScreenDoorFadeParameter.Bind(ParameterMap,TEXT("bEnableScreenDoorFade"),TRUE);
	ScreenDoorFadeSettingsParameter.Bind(ParameterMap,TEXT("ScreenDoorFadeSettings"),TRUE);
	ScreenDoorFadeSettings2Parameter.Bind(ParameterMap,TEXT("ScreenDoorFadeSettings2"),TRUE);
	ScreenDoorNoiseTextureParameter.Bind(ParameterMap,TEXT("ScreenDoorNoiseTexture"),TRUE);

	AlphaSampleTextureParameter.Bind(ParameterMap,TEXT("AlphaSampleTexture"),TRUE);
	FluidDetailNormalTextureParameter.Bind(ParameterMap,TEXT("FluidDetailNormalTexture"),TRUE);
}

/** Sets pixel parameters that are material specific but not FMeshBatch specific. */
void FMaterialPixelShaderParameters::Set(FShader* PixelShader,const FMaterialRenderContext& MaterialRenderContext, ESceneDepthUsage DepthUsage/*=SceneDepthUsage_Normal*/) const
{
	const FPixelShaderRHIParamRef PixelShaderRHI = PixelShader->GetPixelShader();
	const FMaterialRenderProxy& MaterialRenderProxy = *MaterialRenderContext.MaterialRenderProxy;
	const FUniformExpressionSet& UniformExpressionSet = MaterialRenderContext.Material.ShaderMap->GetUniformExpressionSet();
	FMaterialShaderParameters::SetShader(PixelShaderRHI, UniformExpressionSet.PixelExpressions, MaterialRenderContext, MaterialRenderProxy.UniformParameterCache.PixelValues);

#if WITH_MOBILE_RHI
	if( GUsingMobileRHI )
	{
		FTexture* BaseTexture = MaterialRenderContext.MaterialRenderProxy->GetMobileTexture(Base_MobileTexture);

		if (BaseTexture)
		{
			RHISetMobileTextureSamplerState(PixelShader->GetPixelShader(), Base_MobileTexture, BaseTexture->SamplerStateRHI, BaseTexture->TextureRHI, 0.0f, -1.0f, -1.0f);
		}

		FTexture* MobileDetailTexture = MaterialRenderContext.MaterialRenderProxy->GetMobileTexture(Detail_MobileTexture);
		if (MobileDetailTexture)
		{
			RHISetMobileTextureSamplerState(PixelShader->GetPixelShader(), Detail_MobileTexture, MobileDetailTexture->SamplerStateRHI, MobileDetailTexture->TextureRHI, 0.0f, -1.0f, -1.0f);
		}

		FTexture* MobileDetailTexture2 = MaterialRenderContext.MaterialRenderProxy->GetMobileTexture(Detail_MobileTexture2);
		if (MobileDetailTexture2)
		{
			RHISetMobileTextureSamplerState(PixelShader->GetPixelShader(), Detail_MobileTexture2, MobileDetailTexture2->SamplerStateRHI, MobileDetailTexture2->TextureRHI, 0.0f, -1.0f, -1.0f);
		}

		FTexture* MobileDetailTexture3 = MaterialRenderContext.MaterialRenderProxy->GetMobileTexture(Detail_MobileTexture3);
		if (MobileDetailTexture3)
		{
			RHISetMobileTextureSamplerState(PixelShader->GetPixelShader(), Detail_MobileTexture3, MobileDetailTexture3->SamplerStateRHI, MobileDetailTexture3->TextureRHI, 0.0f, -1.0f, -1.0f);
		}

		FTexture* MobileNormalTexture = MaterialRenderContext.MaterialRenderProxy->GetMobileTexture(Normal_MobileTexture);
		if (MobileNormalTexture)
		{
			RHISetMobileTextureSamplerState(PixelShader->GetPixelShader(), Normal_MobileTexture, MobileNormalTexture->SamplerStateRHI, MobileNormalTexture->TextureRHI, 0.0f, -1.0f, -1.0f);
		}

		FTexture* MobileEnvironmentTexture = MaterialRenderContext.MaterialRenderProxy->GetMobileTexture(Environment_MobileTexture);
		if (MobileEnvironmentTexture)
		{
			RHISetMobileTextureSamplerState(PixelShader->GetPixelShader(), Environment_MobileTexture, MobileEnvironmentTexture->SamplerStateRHI, MobileEnvironmentTexture->TextureRHI, 0.0f, -1.0f, -1.0f);
		}

		FTexture* MobileMaskTexture = MaterialRenderContext.MaterialRenderProxy->GetMobileTexture(Mask_MobileTexture);
		if (MobileMaskTexture)
		{
			RHISetMobileTextureSamplerState(PixelShader->GetPixelShader(), Mask_MobileTexture, MobileMaskTexture->SamplerStateRHI, MobileMaskTexture->TextureRHI, 0.0f, -1.0f, -1.0f);
		}

		FTexture* MobileEmissiveTexture = MaterialRenderContext.MaterialRenderProxy->GetMobileTexture(Emissive_MobileTexture);
		if (MobileEmissiveTexture)
		{
			RHISetMobileTextureSamplerState(PixelShader->GetPixelShader(), Emissive_MobileTexture, MobileEmissiveTexture->SamplerStateRHI, MobileEmissiveTexture->TextureRHI, 0.0f, -1.0f, -1.0f);
		}
	
		FMobileMaterialPixelParams MobileMaterialPixelParams;
		MaterialRenderContext.MaterialRenderProxy->FillMobileMaterialPixelParams(MobileMaterialPixelParams);
		RHISetMobileMaterialPixelParams(MobileMaterialPixelParams);
	}
	else
#endif
	{
		const FUniformExpressionSet& UniformExpressionSet = MaterialRenderContext.Material.ShaderMap->GetUniformExpressionSet();
		for(INT ParameterIndex = 0;ParameterIndex < UniformPixelCubeShaderResourceParameters.Num();ParameterIndex++)
		{
			const TUniformParameter<FShaderResourceParameter>& UniformResourceParameter = UniformPixelCubeShaderResourceParameters(ParameterIndex);
			checkSlow(UniformResourceParameter.Index < UniformExpressionSet.UniformCubeTextureExpressions.Num());
			const FTexture* Value = NULL;
			UniformExpressionSet.UniformCubeTextureExpressions(UniformResourceParameter.Index)->GetTextureValue(MaterialRenderContext,MaterialRenderContext.Material,Value);
			if (!Value)
			{
				Value = GWhiteTextureCube;
			}

			checkSlow(Value);
			Value->LastRenderTime = GCurrentTime;
			const INT MinMipLevel = MaterialRenderContext.bWorkAroundDeferredMipArtifacts ? 3 : -1;
			SetTextureParameter(PixelShaderRHI,UniformResourceParameter.ShaderParameter,Value,0,0,-1,MinMipLevel,MaterialRenderContext.bWorkAroundDeferredMipArtifacts);
		}
	}

	// set the inverse projection transform
	SetPixelShaderValue(
		PixelShaderRHI,
		InvViewProjectionParameter,
		MaterialRenderContext.View->InvViewProjectionMatrix
		);

	// set the transform from world space to post-projection space
	SetPixelShaderValue(
		PixelShaderRHI,
		ViewProjectionParameter,
		MaterialRenderContext.View->TranslatedViewProjectionMatrix
		);

	if( InvGammaParameter.IsBound() && MaterialRenderContext.Material.IsUsedWithGammaCorrection() )
	{			
		// set inverse gamma shader constant
		checkSlow(MaterialRenderContext.View->Family->GammaCorrection > 0.0f );
		SetPixelShaderValue( PixelShaderRHI, InvGammaParameter, 1.0f / MaterialRenderContext.View->Family->GammaCorrection );
	}

	SceneTextureParameters.Set(
		MaterialRenderContext.View,
		PixelShader,
		SF_Point, 
		DepthUsage);

#if PLATFORM_SUPPORTS_D3D10_PLUS
	SetTextureParameter(
		PixelShaderRHI,
		AlphaSampleTextureParameter,
		GStratifiedTranslucencySampleTexture
		);
#endif
}

/**
* Set local transforms for rendering a material with a single mesh
* @param MaterialRenderContext - material specific info for setting the shader
* @param LocalToWorld - l2w for rendering a single mesh
*/
void FMaterialPixelShaderParameters::SetMesh(
	FShader* PixelShader,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FMeshBatch& Mesh,
	INT BatchElementIndex,
	const FSceneView& View,
	UBOOL bBackFace
	) const
{
	FMaterialShaderParameters::SetMeshShader(PixelShader->GetPixelShader(),PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
	DOFParameters.SetPS(PixelShader, View.DepthOfFieldParams);

	// Set the two-sided sign parameter.
	SetPixelShaderValue(
		PixelShader->GetPixelShader(),
		TwoSidedSignParameter,
		XOR(bBackFace,XOR(View.bReverseCulling,Mesh.ReverseCulling)) ? -1.0f : +1.0f
		);

	// set the distance to the decal far plane used for clipping the decal
	if( DecalNearFarPlaneDistanceParameter.IsBound() )
	{	
		FLOAT DecalNearFarPlaneDist[2] = {-65536.f,65536.f};
		if( Mesh.bIsDecal && 
			Mesh.DecalState &&
			!Mesh.DecalState->bUseSoftwareClip &&
			!Mesh.bWireframe )
		{
			// far plane decal distance relative to attachment transform
			DecalNearFarPlaneDist[0] = Mesh.DecalState->NearPlaneDistance;
			DecalNearFarPlaneDist[1] = Mesh.DecalState->FarPlaneDistance;
		}
		SetPixelShaderValue(PixelShader->GetPixelShader(),DecalNearFarPlaneDistanceParameter,DecalNearFarPlaneDist);
	}

	if (PrimitiveSceneInfo)
	{
		if (ObjectPostProjectionPositionParameter.IsBound() || ObjectMacroUVScalesParameter.IsBound())
		{
			FVector ObjectPostProjectionPosition;
			FVector ObjectNDCPosition;
			FVector4 ObjectMacroUVScales;
			PrimitiveSceneInfo->Proxy->GetObjectPositionAndScale(
				View, 
				ObjectPostProjectionPosition,
				ObjectNDCPosition,
				ObjectMacroUVScales);

			SetPixelShaderValue(PixelShader->GetPixelShader(),ObjectPostProjectionPositionParameter,ObjectPostProjectionPosition);
			SetPixelShaderValue(PixelShader->GetPixelShader(),ObjectNDCPositionParameter,ObjectNDCPosition);
			SetPixelShaderValue(PixelShader->GetPixelShader(),ObjectMacroUVScalesParameter,ObjectMacroUVScales);
		}

		if (OcclusionPercentageParameter.IsBound())
		{
			SetPixelShaderValue(PixelShader->GetPixelShader(), OcclusionPercentageParameter, 
				PrimitiveSceneInfo->Proxy->GetOcclusionPercentage(View));
		}

		if (FluidDetailNormalTextureParameter.IsBound())
		{
			const FTexture2DRHIRef* FluidDetailNormal = PrimitiveSceneInfo->Scene->GetFluidDetailNormal();
			if (!FluidDetailNormal || !IsValidRef(*FluidDetailNormal))
			{
				// Use the black texture if no valid fluid surface is active, 
				// This will result in a normal straight up in tangent space, since only the tangent X and Y are stored in the texture
				FluidDetailNormal = (FTexture2DRHIRef*)&GBlackTexture->TextureRHI;
			}
			SetTextureParameter(
				PixelShader->GetPixelShader(),
				FluidDetailNormalTextureParameter,
				TStaticSamplerState<SF_Trilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
				*FluidDetailNormal
				);
		}
	}


	if( EnableScreenDoorFadeParameter.IsBound() )
	{
		// Grab the current fade opacity for this primitive in this view
		FLOAT FadeOpacity = 1.0f;
		EScreenDoorPattern::Type ScreenDoorPattern = EScreenDoorPattern::Normal;
		if( PrimitiveSceneInfo != NULL )
		{
			const FSceneViewState* SceneViewState = static_cast<const FSceneViewState*>( View.State );
			if( SceneViewState != NULL )
			{
				FadeOpacity = SceneViewState->GetPrimitiveFadeOpacity( PrimitiveSceneInfo->Component, Mesh.LODIndex, ScreenDoorPattern );
			}
		}
		const UBOOL bIsCurrentlyFading = ( FadeOpacity < 0.99f );
		
		
		// Whether screen door fade is enabled or not.  The shader may branch off this bool.
		SetPixelShaderBool(
			PixelShader->GetPixelShader(),
			EnableScreenDoorFadeParameter,
			bIsCurrentlyFading );


		// Only set the other screen door fade parameters if the primitive is actually fading
		if( bIsCurrentlyFading )
		{
			FVector4 ScreenDoorFadeSettings;
			FVector4 ScreenDoorFadeSettings2;
			{
				// Send the object opacity to the shader
				ScreenDoorFadeSettings.X = FadeOpacity;		// X = Opacity

				// Set the noise value scale/bias.  This is used to enable cross-fading between objects
				// that are directly overlapping while avoiding Z-fighting artifacts.  Object A will fade
				// in using inverted screen door noise values from Object B that's fading out.
				const UBOOL bUseInvertedNoiseValues = ( ScreenDoorPattern == EScreenDoorPattern::Inverse );
				if( bUseInvertedNoiseValues )
				{
					// Inverted!
					ScreenDoorFadeSettings.Y = -1.0f;			// Y = Noise value scale
					ScreenDoorFadeSettings.Z = 1.0f;			// Z = Noise value bias
				}
				else
				{
					// Non-inverted
					ScreenDoorFadeSettings.Y = 1.0f;			// Y = Noise value scale
					ScreenDoorFadeSettings.Z = 0.0f;			// Z = Noise value bias
				}

				// unused
				ScreenDoorFadeSettings.W = 0;

				// Set this to TRUE to enable a "TV static" style dissolve instead of a fixed dissolve pattern
				const UBOOL bUseTVStaticDissolve = FALSE;

				ScreenDoorFadeSettings2.X = bUseTVStaticDissolve ? View.ScreenDoorRandomOffset.X : 0.0f;
				ScreenDoorFadeSettings2.Y = bUseTVStaticDissolve ? View.ScreenDoorRandomOffset.Y : 0.0f;

				// Set the noise texture UV scaling amount.  Note that currently the noise texture is 64x64
				// we map to the 64x64 tiling texture
				ScreenDoorFadeSettings2.Z = 1.0f / 64.0f;
				ScreenDoorFadeSettings2.W = -1.0f / 64.0f;
			}
			SetPixelShaderValue(
				PixelShader->GetPixelShader(),
				ScreenDoorFadeSettingsParameter,
				ScreenDoorFadeSettings );
			SetPixelShaderValue(
				PixelShader->GetPixelShader(),
				ScreenDoorFadeSettings2Parameter,
				ScreenDoorFadeSettings2 );
		}

		// Screen door noise texture
		// Note: Always set this if the shader reference it, if even it's if:ed out.
		// Otherwise it'll be detected as an missing texture error by the PS3 RHI.
		if ( ScreenDoorNoiseTextureParameter.IsBound() )
		{
			UTexture2D* ScreenDoorNoiseTexture = GEngine->ScreenDoorNoiseTexture;
			checkSlow( ScreenDoorNoiseTexture != NULL );
			SetTextureParameter(
				PixelShader->GetPixelShader(),
				ScreenDoorNoiseTextureParameter,
				TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
				ScreenDoorNoiseTexture->Resource->TextureRHI );
		}
	}



#if WITH_MOBILE_RHI
	if( GUsingMobileRHI )
	{
		FMobileMeshPixelParams MobileMeshPixelParams;

		// Set whether sky light affects this primitive
		MobileMeshPixelParams.bEnableSkyLight = PrimitiveSceneInfo != NULL ? PrimitiveSceneInfo->HasDynamicSkyLighting() : FALSE;

		RHISetMobileMeshPixelParams( MobileMeshPixelParams );
	}
#endif
}

FArchive& operator<<(FArchive& Ar,FMaterialPixelShaderParameters& Parameters)
{
	check(Ar.Ver() >= VER_MIN_MATERIAL_PIXELSHADER);

	Ar << (FMaterialShaderParameters&)Parameters;

	Ar << Parameters.UniformPixelCubeShaderResourceParameters;
	Ar << Parameters.SceneTextureParameters;
	Ar << Parameters.TwoSidedSignParameter;
	Ar << Parameters.InvGammaParameter;
	Ar << Parameters.DecalNearFarPlaneDistanceParameter;
	Ar << Parameters.ObjectPostProjectionPositionParameter;
	Ar << Parameters.ObjectMacroUVScalesParameter;
	Ar << Parameters.ObjectNDCPositionParameter;
	Ar << Parameters.OcclusionPercentageParameter;
	Ar << Parameters.EnableScreenDoorFadeParameter;
	Ar << Parameters.ScreenDoorFadeSettingsParameter;
	Ar << Parameters.ScreenDoorFadeSettings2Parameter;
	Ar << Parameters.ScreenDoorNoiseTextureParameter;
	Ar << Parameters.AlphaSampleTextureParameter;
	Ar << Parameters.FluidDetailNormalTextureParameter;
	Ar << Parameters.DOFParameters;

#if WITH_MOBILE_RHI
	if( GUsingMobileRHI )
	{
		Parameters.SceneTextureParameters.SceneColorTextureParameter.Unbind();
		Parameters.SceneTextureParameters.SceneDepthTextureParameter.Unbind();
		Parameters.SceneTextureParameters.SceneDepthSurfaceParameter.Unbind();
		Parameters.EnableScreenDoorFadeParameter.Unbind();
#if !CONSOLE
		Parameters.SceneTextureParameters.NvStereoFixTextureParameter.Unbind();
#endif
	}
#endif

	return Ar;
}

UBOOL FMaterialPixelShaderParameters::IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const
{
	for(INT ParameterIndex = 0;ParameterIndex < UniformPixelCubeShaderResourceParameters.Num();ParameterIndex++)
	{
		const TUniformParameter<FShaderResourceParameter>& UniformResourceParameter = UniformPixelCubeShaderResourceParameters(ParameterIndex);
		if (UniformResourceParameter.Index >= UniformExpressionSet.UniformCubeTextureExpressions.Num())
		{
			return FALSE;
		}
	}
	return FMaterialShaderParameters::IsUniformExpressionSetValid(UniformExpressionSet.GetExpresssions(SF_Pixel));
}

#if WITH_D3D11_TESSELLATION

void FMaterialDomainShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	FMaterialShaderParameters::Bind(ParameterMap, SF_Domain);
}

/** Sets domain shader parameters that are material specific but not FMeshBatch specific. */
void FMaterialDomainShaderParameters::Set(FShader* DomainShader,const FMaterialRenderContext& MaterialRenderContext) const
{
	const FUniformExpressionSet& UniformExpressionSet = MaterialRenderContext.Material.ShaderMap->GetUniformExpressionSet();
	FMaterialShaderParameters::SetShader(
		DomainShader->GetDomainShader(), 
		UniformExpressionSet.DomainExpressions,
		MaterialRenderContext, 
		MaterialRenderContext.MaterialRenderProxy->UniformParameterCache.DomainValues);
}

/**
* Set local transforms for rendering a material with a single mesh
* @param MaterialRenderContext - material specific info for setting the shader
* @param LocalToWorld - l2w for rendering a single mesh
*/
void FMaterialDomainShaderParameters::SetMesh(
	FShader* DomainShader,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FMeshBatch& Mesh,
	INT BatchElementIndex,
	const FSceneView& View
	) const
{
	FMaterialShaderParameters::SetMeshShader(DomainShader->GetDomainShader(),PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
	DOFParameters.SetDS(DomainShader, View.DepthOfFieldParams);
}

void FMaterialHullShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	FMaterialShaderParameters::Bind(ParameterMap, SF_Hull);
}

/** Sets hull shader parameters that are material specific but not FMeshBatch specific. */
void FMaterialHullShaderParameters::Set(FShader* HullShader,const FMaterialRenderContext& MaterialRenderContext) const
{
	const FUniformExpressionSet& UniformExpressionSet = MaterialRenderContext.Material.ShaderMap->GetUniformExpressionSet();
	FMaterialShaderParameters::SetShader(
		HullShader->GetHullShader(), 
		UniformExpressionSet.HullExpressions,
		MaterialRenderContext, 
		MaterialRenderContext.MaterialRenderProxy->UniformParameterCache.HullValues);
}

/**
* Set local transforms for rendering a material with a single mesh
* @param MaterialRenderContext - material specific info for setting the shader
* @param LocalToWorld - l2w for rendering a single mesh
*/
void FMaterialHullShaderParameters::SetMesh(
	FShader* HullShader,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FMeshBatch& Mesh,
	INT BatchElementIndex,
	const FSceneView& View
	) const
{
	FMaterialShaderParameters::SetMeshShader(HullShader->GetHullShader(),PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
}

#endif

void FMaterialVertexShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	FMaterialShaderParameters::Bind(ParameterMap, SF_Vertex);
}

/** Sets vertex parameters that are material specific but not FMeshBatch specific. */
void FMaterialVertexShaderParameters::Set(FShader* VertexShader,const FMaterialRenderContext& MaterialRenderContext) const
{
	const FUniformExpressionSet& UniformExpressionSet = MaterialRenderContext.Material.ShaderMap->GetUniformExpressionSet();
	FMaterialShaderParameters::SetShader(
		VertexShader->GetVertexShader(), 
		UniformExpressionSet.VertexExpressions,
		MaterialRenderContext, 
		MaterialRenderContext.MaterialRenderProxy->UniformParameterCache.VertexValues);

#if WITH_MOBILE_RHI
	if( GUsingMobileRHI )
	{
		FMobileMaterialVertexParams MobileMaterialVertexParams;
		MaterialRenderContext.Material.FillMobileMaterialVertexParams(MobileMaterialVertexParams);
		//overrides PER proxy (mesh particles)
		MaterialRenderContext.MaterialRenderProxy->FillMobileMaterialVertexParams(MobileMaterialVertexParams);
		RHISetMobileMaterialVertexParams(MobileMaterialVertexParams);
	}
#endif
}


/**
 * Set the material shader parameters which depend on the mesh element being rendered.
 * @param View - The view that is being rendered.
 * @param Mesh - The mesh that is being rendered.
 */
void FMaterialVertexShaderParameters::SetMesh(
	FShader* VertexShader,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FMeshBatch& Mesh,
	INT BatchElementIndex,
	const FSceneView& View
	) const
{
	const FMeshBatchElement& BatchElement = Mesh.Elements(BatchElementIndex);
	FMaterialShaderParameters::SetMeshShader(VertexShader->GetVertexShader(), PrimitiveSceneInfo, Mesh, BatchElementIndex, View);
	DOFParameters.SetVS(VertexShader, View.DepthOfFieldParams);

#if WITH_MOBILE_RHI
	if( GUsingMobileRHI )
	{
		FMobileMeshVertexParams MobileMeshVertexParams;

		{
			// Find the brightest light affecting this primitive
			// Note: This is currently only used for per-vertex specular on mobile platforms
			FLightSceneInfo* BrightestLightSceneInfo = NULL;
			if( PrimitiveSceneInfo != NULL )
			{
				// Iterate over all lights this primitive is interacting with.
				FLightPrimitiveInteraction* LightPrimitiveInteraction = PrimitiveSceneInfo->LightList;
				while( LightPrimitiveInteraction )
				{
					FLightSceneInfo* CurLight = LightPrimitiveInteraction->GetLight();

					// We only care about directional lights for per-vertex specular on mobile
					if( CurLight->LightType == LightType_Directional || CurLight->LightType == LightType_DominantDirectional )
					{
						if (!BrightestLightSceneInfo
							|| CurLight->Color.GetMax() > BrightestLightSceneInfo->Color.GetMax())
						{
							BrightestLightSceneInfo = CurLight;
						}	
					}

					LightPrimitiveInteraction = LightPrimitiveInteraction->GetNextLight();
				}
			}

			if( BrightestLightSceneInfo != NULL )
			{
				MobileMeshVertexParams.BrightestLightDirection = BrightestLightSceneInfo->GetDirection();
				MobileMeshVertexParams.BrightestLightColor = BrightestLightSceneInfo->Color;
			}
			else
			{
 				MobileMeshVertexParams.BrightestLightDirection = FVector( 0, 0, -1 );
 				MobileMeshVertexParams.BrightestLightColor = FLinearColor( 1, 1, 1, 1 );
			}
		}

		MobileMeshVertexParams.CameraPosition = View.ViewOrigin;
		if (PrimitiveSceneInfo)
		{
			MobileMeshVertexParams.ObjectPosition = PrimitiveSceneInfo->Bounds.Origin;
			MobileMeshVertexParams.ObjectBounds = PrimitiveSceneInfo->Bounds;
		}
		else
		{
			//for canvas material
			MobileMeshVertexParams.ObjectPosition = View.ViewOrigin;
			MobileMeshVertexParams.ObjectBounds = FBoxSphereBounds(View.ViewOrigin, FVector(1.0f, 1.0f, 1.0f), 1.0f);
		}
		MobileMeshVertexParams.LocalToWorld = &BatchElement.LocalToWorld;

		MobileMeshVertexParams.ParticleScreenAlignment = Mesh.VertexFactory->GetSpriteScreenAlignment();

		RHISetMobileMeshVertexParams(MobileMeshVertexParams);
	}
#endif
}

/**
* Finds a FMaterialShaderType by name.
*/
FMaterialShaderType* FMaterialShaderType::GetTypeByName(const FString& TypeName)
{
	for(TLinkedList<FShaderType*>::TIterator It(FShaderType::GetTypeList()); It; It.Next())
	{
		FString CurrentTypeName = FString(It->GetName());
		FMaterialShaderType* CurrentType = It->GetMaterialShaderType();
		if (CurrentType && CurrentTypeName == TypeName)
		{
			return CurrentType;
		}
	}
	return NULL;
}

/**
 * Enqueues a compilation for a new shader of this type.
 * @param Material - The material to link the shader with.
 * @param MaterialShaderCode - The shader code for the material.
 */
void FMaterialShaderType::BeginCompileShader(
	UINT ShaderMapId,
	const FMaterial* Material,
	const ANSICHAR* MaterialShaderCode,
	EShaderPlatform Platform
	)
{
	// Construct the shader environment.
	FShaderCompilerEnvironment Environment;
	
	Material->SetupMaterialEnvironment(Platform, NULL, Environment);
	Environment.MaterialShaderCode = MaterialShaderCode;

	warnf(NAME_DevShadersDetailed, TEXT("			%s"), GetName());

	//update material shader stats
	UpdateMaterialShaderCompilingStats(Material);

	// Enqueue the compilation
	FShaderType::BeginCompileShader(ShaderMapId, NULL, Platform, Environment);
}

/**
 * Either creates a new instance of this type or returns an equivalent existing shader.
 * @param Material - The material to link the shader with.
 * @param CurrentJob - Compile job that was enqueued by BeginCompileShader.
 */
FShader* FMaterialShaderType::FinishCompileShader(
	const FUniformExpressionSet& UniformExpressionSet,
	const FShaderCompileJob& CurrentJob)
{
	check(CurrentJob.bSucceeded);
	// Check for shaders with identical compiled code.
	FShader* Shader = FindShaderByOutput(CurrentJob.Output);

	if (!Shader)
	{
		// Create the shader.
		CurrentJob.Output.ParameterMap.UniformExpressionSet = &UniformExpressionSet;
		Shader = (*ConstructCompiledRef)(CompiledShaderInitializerType(this,CurrentJob.Output));
		CurrentJob.Output.ParameterMap.VerifyBindingsAreComplete(GetName(), (EShaderFrequency)CurrentJob.Output.Target.Frequency, CurrentJob.VFType);
	}
	return Shader;
}

/**
* Finds the shader map for a material.
* @param StaticParameterSet - The static parameter set identifying the shader map
* @param Platform - The platform to lookup for
* @return NULL if no cached shader map was found.
*/
FMaterialShaderMap* FMaterialShaderMap::FindId(const FStaticParameterSet& StaticParameterSet, EShaderPlatform InPlatform)
{
	return GIdToMaterialShaderMap[InPlatform].FindRef(StaticParameterSet);
}

/** Flushes the given shader types from any loaded FMaterialShaderMap's. */
void FMaterialShaderMap::FlushShaderTypes(TArray<FShaderType*>& ShaderTypesToFlush, TArray<const FVertexFactoryType*>& VFTypesToFlush)
{
	for (INT PlatformIndex = 0; PlatformIndex < SP_NumPlatforms; PlatformIndex++)
	{
		for (TMap<FStaticParameterSet,FMaterialShaderMap*>::TIterator It(FMaterialShaderMap::GIdToMaterialShaderMap[PlatformIndex]); It; ++It)
		{
			FMaterialShaderMap* CurrentShaderMap = It.Value();

			for (INT ShaderTypeIndex = 0; ShaderTypeIndex < ShaderTypesToFlush.Num(); ShaderTypeIndex++)
			{
				CurrentShaderMap->FlushShadersByShaderType(ShaderTypesToFlush(ShaderTypeIndex));
			}
			for (INT VFTypeIndex = 0; VFTypeIndex < VFTypesToFlush.Num(); VFTypeIndex++)
			{
				CurrentShaderMap->FlushShadersByVertexFactoryType(VFTypesToFlush(VFTypeIndex));
			}
		}
	}
}

FMaterialShaderMap::~FMaterialShaderMap()
{
	if (bRegistered)
	{
		GIdToMaterialShaderMap[Platform].Remove(StaticParameters);
	}
	ShaderMapsBeingCompiled.Remove(this);
}

/** 
 * Flag indicating a material should be forced to non-persistent.
 * Used when compiling materials for other platforms in the ContentBrowser.
 */
UBOOL GbForceMaterialNonPersistent = FALSE;

/**
* Compiles the shaders for a material and caches them in this shader map.
* @param Material - The material to compile shaders for.
* @param InStaticParameters - the set of static parameters to compile for
* @param MaterialShaderCode - The shader code for Material.
* @param Platform - The platform to compile to
* @param OutErrors - Upon compilation failure, OutErrors contains a list of the errors which occured.
* @param bDebugDump - Dump out the preprocessed and disassembled shader for debugging.
* @return True if the compilation succeeded.
*/
UBOOL FMaterialShaderMap::Compile(
	FMaterial* Material,
	const FStaticParameterSet* InStaticParameters,
	const TCHAR* MaterialShaderCode,
	const FUniformExpressionSet& InUniformExpressionSet,
	EShaderPlatform InPlatform,
	TArray<FString>& OutErrors,
	UBOOL bDebugDump)
{
#if CONSOLE
	appErrorf( TEXT("Trying to compile %s at run-time, which is not supported on consoles!"), *Material->GetFriendlyName() );
	return FALSE;
#else
	if (appGetPlatformType() & UE3::PLATFORM_Stripped)
	{
		warnf( TEXT("Trying to compile %s at run-time but material expressions have been stripped!"), *Material->GetFriendlyName() );
		return FALSE;
	}

	// Store the material name for debugging purposes.
	// Note: Material instances with static parameters will have the same FriendlyName for their shader maps!
	FriendlyName = Material->GetFriendlyName();
	UniformExpressionSet = InUniformExpressionSet;
	MaterialId = Material->GetId();
	StaticParameters = *InStaticParameters;
	Platform = InPlatform;
	bIsPersistent = Material->IsPersistent() && (!GbForceMaterialNonPersistent);

	const FString MaterialUsage = Material->GetMaterialUsageDescription();
	const INT MaterialCodeLength = appStrlen(MaterialShaderCode) + 1;
	ANSICHAR* AnsiMaterialCode = new ANSICHAR[MaterialCodeLength];
	appStrcpyANSI(AnsiMaterialCode, MaterialCodeLength, TCHAR_TO_ANSI(MaterialShaderCode));
	const ANSICHAR* StoredMaterialShaderCode = MaterialCodeBeingCompiled.Set(CompilingId, AnsiMaterialCode);

	// Add this shader map and material resource to ShaderMapsBeingCompiled
	TArray<FMaterial*>* CorrespondingMaterials = ShaderMapsBeingCompiled.Find(this);
	if (CorrespondingMaterials)
	{
		CorrespondingMaterials->AddUniqueItem(Material);
	}
	else
	{
		// Assign a unique identifier so that shaders from this shader map can be associated with it after a deferred compile
		CompilingId = NextCompilingId;
		check(NextCompilingId < UINT_MAX);
		NextCompilingId++;

		TArray<FMaterial*> NewCorrespondingMaterials;
		NewCorrespondingMaterials.AddItem(Material);
		ShaderMapsBeingCompiled.Set(this, NewCorrespondingMaterials);

		warnf(NAME_DevShaders, TEXT("	Compiling %s for %s: %s"), *FriendlyName, ShaderPlatformToText(InPlatform), *MaterialUsage);
	}

	UINT NumShaders = 0;
	UINT NumVertexFactories = 0;

	// Iterate over all vertex factory types.
	for(TLinkedList<FVertexFactoryType*>::TIterator VertexFactoryTypeIt(FVertexFactoryType::GetTypeList());VertexFactoryTypeIt;VertexFactoryTypeIt.Next())
	{
		FVertexFactoryType* VertexFactoryType = *VertexFactoryTypeIt;
		check(VertexFactoryType);

		if(VertexFactoryType->IsUsedWithMaterials())
		{
			FMeshMaterialShaderMap* MeshShaderMap = NULL;

			// look for existing map for this vertex factory type
			INT MeshShaderMapIndex = INDEX_NONE;
			for (INT ShaderMapIndex = 0; ShaderMapIndex < MeshShaderMaps.Num(); ShaderMapIndex++)
			{
				if (MeshShaderMaps(ShaderMapIndex).GetVertexFactoryType() == VertexFactoryType)
				{
					MeshShaderMap = &MeshShaderMaps(ShaderMapIndex);
					MeshShaderMapIndex = ShaderMapIndex;
					break;
				}
			}

			if (MeshShaderMap == NULL)
			{
				// Create a new mesh material shader map.
				MeshShaderMapIndex = MeshShaderMaps.Num();
				MeshShaderMap = new(MeshShaderMaps) FMeshMaterialShaderMap;
			}

			// Enqueue compilation all mesh material shaders for this material and vertex factory type combo.
			const UINT MeshShaders = MeshShaderMap->BeginCompile(CompilingId,Material,StoredMaterialShaderCode,VertexFactoryType,InPlatform);
			NumShaders += MeshShaders;
			if (MeshShaders > 0)
			{
				NumVertexFactories++;
			}
		}
	}

	// Iterate over all material shader types.
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FMaterialShaderType* ShaderType = ShaderTypeIt->GetMaterialShaderType();
		if (ShaderType && 
			ShaderType->ShouldCache(InPlatform,Material) && 
			Material->ShouldCache(InPlatform, ShaderType, NULL)
			)
		{
			// Compile this material shader for this material.
			TArray<FString> ShaderErrors;

			// Only compile the shader if we don't already have it
			if (!HasShader(ShaderType))
			{
				ShaderType->BeginCompileShader(
					CompilingId,
					Material,
					StoredMaterialShaderCode,
					InPlatform
					);
			}
			NumShaders++;
		}
	}

	if (!CorrespondingMaterials)
	{
		warnf(NAME_DevShaders, TEXT("		%u Shaders among %u VertexFactories"), NumShaders, NumVertexFactories);
	}

	if (bDebugDump)
	{
		const FString UsageFilePath = FString(appShaderDir()) * ShaderPlatformToText((EShaderPlatform)InPlatform) * FriendlyName * TEXT("Usage.txt");
		appSaveStringToFile(MaterialUsage, *UsageFilePath);
	}

	// Register this shader map in the global map with the material's ID.
	Register();

	// Mark the shader map as not having been finalized with ProcessCompilationResults
	bCompilationFinalized = FALSE;
	bCompiledSuccessfully = TRUE;

	// Compile the shaders for this shader map now if the material is not deferring and deferred compiles are not enabled globally
	if (bDebugDump || !Material->DeferFinishCompiling() && !GShaderCompilingThreadManager->IsDeferringCompilation())
	{
		GShaderCompilingThreadManager->FinishDeferredCompilation(*FriendlyName, Material->IsSpecialEngineMaterial(), bDebugDump);
	}
	else if (bIsPersistent)
	{
		// Mark the local shader cache dirty since this shader map will not be added to it until FinishDeferredCompilation is called
		GetLocalShaderCache(Platform)->MarkDirty();
	}

	// Note: If compilation was deferred, this will always return TRUE, and any functionality that depends on this return value will not work correctly.
	return bCompiledSuccessfully;
#endif
}

/** 
 * Processes an array of completed shader compile jobs. 
 * This is called by FShaderCompilingThreadManager after compilation of this shader map's shaders has completed.
 */
void FMaterialShaderMap::ProcessCompilationResults(
	const TArray<TRefCountPtr<FShaderCompileJob> >& InCompilationResults,
	UBOOL bSuccess)
{
#if CONSOLE
	return;
#else

	if (bSuccess)
	{
		// Pass the compile results to each vertex factory
		for(TLinkedList<FVertexFactoryType*>::TIterator VertexFactoryTypeIt(FVertexFactoryType::GetTypeList());VertexFactoryTypeIt;VertexFactoryTypeIt.Next())
		{
			FVertexFactoryType* VertexFactoryType = *VertexFactoryTypeIt;
			check(VertexFactoryType);

			if(VertexFactoryType->IsUsedWithMaterials())
			{
				FMeshMaterialShaderMap* MeshShaderMap = NULL;

				// look for existing map for this vertex factory type
				INT MeshShaderMapIndex = INDEX_NONE;
				for (INT ShaderMapIndex = 0; ShaderMapIndex < MeshShaderMaps.Num(); ShaderMapIndex++)
				{
					if (MeshShaderMaps(ShaderMapIndex).GetVertexFactoryType() == VertexFactoryType)
					{
						MeshShaderMap = &MeshShaderMaps(ShaderMapIndex);
						MeshShaderMapIndex = ShaderMapIndex;
						break;
					}
				}
				if (MeshShaderMap)
				{
					// Create new shaders from the compiled jobs and cache them in this shader map.
					MeshShaderMap->FinishCompile(CompilingId, UniformExpressionSet, InCompilationResults);

					if (MeshShaderMap->GetNumShaders() == 0)
					{
						// If the mesh material shader map is complete and empty, discard it.
						MeshShaderMaps.Remove(MeshShaderMapIndex);
					}
				}
			}
		}

		// Add FMaterialShaderType results to this shader map.
		for (INT JobIndex = 0; JobIndex < InCompilationResults.Num(); JobIndex++)
		{
			const FShaderCompileJob& CurrentJob = *InCompilationResults(JobIndex);
			if (CurrentJob.Id == CompilingId)
			{
				for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
				{
					FMaterialShaderType* MaterialShaderType = ShaderTypeIt->GetMaterialShaderType();
					if (*ShaderTypeIt == CurrentJob.ShaderType && MaterialShaderType != NULL)
					{
						FShader* Shader = MaterialShaderType->FinishCompileShader(UniformExpressionSet, CurrentJob);
						check(Shader);
						AddShader(MaterialShaderType,Shader);
					}
				}
			}
		}
	}

	// Reinitialize the ordered mesh shader maps
	InitOrderedMeshShaderMaps();

	// Add the persistent shaders to the local shader cache.
	if (bIsPersistent)
	{
		GetLocalShaderCache(Platform)->AddMaterialShaderMap(this);
	}

	// The shader map can now be used on the rendering thread
	bCompilationFinalized = TRUE;
#endif
}

UBOOL FMaterialShaderMap::IsComplete(const FMaterial* Material, UBOOL bSilent) const
{
	UBOOL bIsComplete = TRUE;

	TArray<FMaterial*>* CorrespondingMaterials = FMaterialShaderMap::ShaderMapsBeingCompiled.Find(this);
	if (CorrespondingMaterials)
	{
		return FALSE;
	}

	// Iterate over all vertex factory types.
	for(TLinkedList<FVertexFactoryType*>::TIterator VertexFactoryTypeIt(FVertexFactoryType::GetTypeList());VertexFactoryTypeIt;VertexFactoryTypeIt.Next())
	{
		FVertexFactoryType* VertexFactoryType = *VertexFactoryTypeIt;

		if(VertexFactoryType->IsUsedWithMaterials())
		{
			// Find the shaders for this vertex factory type.
			const FMeshMaterialShaderMap* MeshShaderMap = GetMeshShaderMap(VertexFactoryType);
			if(!FMeshMaterialShaderMap::IsComplete(MeshShaderMap,Platform,Material,VertexFactoryType,bSilent))
			{
				if (!MeshShaderMap && !bSilent)
				{
					warnf(NAME_DevShaders, TEXT("Incomplete material %s, missing Vertex Factory %s."), *Material->GetFriendlyName(), VertexFactoryType->GetName());
				}
				bIsComplete = FALSE;
				break;
			}
		}
	}

	// Iterate over all material shader types.
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		// Find this shader type in the material's shader map.
		FMaterialShaderType* ShaderType = ShaderTypeIt->GetMaterialShaderType();
		if (ShaderType && 
			ShaderType->ShouldCache(Platform,Material) && 
			Material->ShouldCache(Platform, ShaderType, NULL) &&
			!HasShader(ShaderType)
			)
		{
			if (!bSilent)
			{
				warnf(NAME_DevShaders, TEXT("Incomplete material %s, missing FMaterialShader %s."), *Material->GetFriendlyName(), ShaderType->GetName());
			}
			bIsComplete = FALSE;
			break;
		}
	}

	return bIsComplete;
}

/** Returns TRUE if all the shaders in this shader map have their compressed shader code in Cache. */
UBOOL FMaterialShaderMap::IsCompressedShaderCacheComplete(const FCompressedShaderCodeCache* const Cache) const
{
	UBOOL bCompressedShaderCacheIsComplete = TRUE;
	// Can only check this for the current platform and on platforms that use shader compression.
	if (GRHIShaderPlatform == GetShaderPlatform() && UseShaderCompression(GetShaderPlatform()))
	{
		check(Cache);
		for (TMap<FShaderType*,TRefCountPtr<FShader> >::TConstIterator It(GetShaders()); It; ++It)
		{
			const FShader* CurrentShader = It.Value();
			if (CurrentShader)
			{
				if (bCompressedShaderCacheIsComplete && !Cache->HasShader(CurrentShader))
				{
					warnf(NAME_DevShaders, 
						TEXT("Compressed shader cache %s didn't contain code for shader %s of material %s!  Can't reuse this cache, shader memory will be higher until a full recook."),
						*Cache->CacheName,
						CurrentShader->GetType()->GetName(),
						*FriendlyName);
				}
				bCompressedShaderCacheIsComplete = bCompressedShaderCacheIsComplete && Cache->HasShader(CurrentShader);
			}
		}

		for (INT ShaderMapIndex = 0; ShaderMapIndex < MeshShaderMaps.Num(); ShaderMapIndex++)
		{
			for (TMap<FShaderType*,TRefCountPtr<FShader> >::TConstIterator It(MeshShaderMaps(ShaderMapIndex).GetShaders()); It; ++It)
			{
				const FShader* CurrentShader = It.Value();
				if (CurrentShader)
				{
					if (bCompressedShaderCacheIsComplete && !Cache->HasShader(CurrentShader))
					{
						warnf(NAME_DevShaders, 
							TEXT("Compressed shader cache %s didn't contain code for shader %s %s of VF %s of material %s!  Can't reuse this cache, shader memory will be higher until a full recook."),
							*Cache->CacheName,
							CurrentShader->GetType()->GetName(),
							*CurrentShader->GetId().String(),
							MeshShaderMaps(ShaderMapIndex).GetVertexFactoryType()->GetName(),
							*FriendlyName);
					}
					bCompressedShaderCacheIsComplete = bCompressedShaderCacheIsComplete && Cache->HasShader(CurrentShader);
				}
			}
		}
	}
	return bCompressedShaderCacheIsComplete;
}

UBOOL FMaterialShaderMap::IsUniformExpressionSetValid() const
{
	for (TMap<FShaderType*,TRefCountPtr<FShader> >::TConstIterator It(GetShaders()); It; ++It)
	{
		const FShader* CurrentShader = It.Value();
		if (CurrentShader)
		{
			if (!CurrentShader->IsUniformExpressionSetValid(UniformExpressionSet))
			{
				return FALSE;
			}
		}
	}

	for (INT ShaderMapIndex = 0; ShaderMapIndex < MeshShaderMaps.Num(); ShaderMapIndex++)
	{
		for (TMap<FShaderType*,TRefCountPtr<FShader> >::TConstIterator It(MeshShaderMaps(ShaderMapIndex).GetShaders()); It; ++It)
		{
			const FShader* CurrentShader = It.Value();
			if (CurrentShader)
			{
				if (!CurrentShader->IsUniformExpressionSetValid(UniformExpressionSet))
				{
					return FALSE;
				}
			}
		}
	}
	return TRUE;
}

void FMaterialShaderMap::GetShaderList(TMap<FGuid,FShader*>& OutShaders) const
{
	TShaderMap<FMaterialShaderType>::GetShaderList(OutShaders);
	for(INT Index = 0;Index < MeshShaderMaps.Num();Index++)
	{
		MeshShaderMaps(Index).GetShaderList(OutShaders);
	}
}

void FMaterialShaderMap::BeginInit()
{
#if !INIT_SHADERS_ON_DEMAND
	// Don't initialize shaders through this mechanism in the editor, 
	// they will be initialized on demand through FShader::GetPixelShader() or GetVertexShader()
	if (!GIsEditor)
	{
		check(bCompilationFinalized);
		TShaderMap<FMaterialShaderType>::BeginInit();
		for(INT MapIndex = 0;MapIndex < MeshShaderMaps.Num();MapIndex++)
		{
			MeshShaderMaps(MapIndex).BeginInit();
		}
	}
#endif
}

void FMaterialShaderMap::BeginRelease()
{
#if !INIT_SHADERS_ON_DEMAND
	// Don't release shaders through this mechanism in the editor, 
	// they will be initialized on demand through FShader::GetPixelShader() or GetVertexShader()
	if (!GIsEditor)
	{
		check(bCompilationFinalized);
		TShaderMap<FMaterialShaderType>::BeginRelease();
		for(INT MapIndex = 0;MapIndex < MeshShaderMaps.Num();MapIndex++)
		{
			MeshShaderMaps(MapIndex).BeginRelease();
		}
	}
#endif
}

/**
 * Registers a material shader map in the global map so it can be used by materials.
 */
void FMaterialShaderMap::Register() 
{ 
	GIdToMaterialShaderMap[Platform].Set(StaticParameters,this); 
	bRegistered = TRUE;
}

FMaterialShaderMap* FMaterialShaderMap::AttemptRegistration()
{
	FMaterialShaderMap** FoundMap = GIdToMaterialShaderMap[Platform].Find(StaticParameters);
	if (FoundMap != NULL)
	{
		return *FoundMap;
	}

	Register();
	return this;
}

/**
 * Merges in OtherMaterialShaderMap's shaders and FMeshMaterialShaderMaps
 */
void FMaterialShaderMap::Merge(const FMaterialShaderMap* OtherMaterialShaderMap)
{
	// check that both shadermaps are for the same material, and just have different shaders
	check(OtherMaterialShaderMap 
		&& GetShaderPlatform() == OtherMaterialShaderMap->GetShaderPlatform()
		&& StaticParameters == OtherMaterialShaderMap->GetMaterialId());

	check(bCompilationFinalized && OtherMaterialShaderMap->bCompilationFinalized);

	// merge the FMaterialShaderTypes
	TShaderMap<FMaterialShaderType>::Merge(OtherMaterialShaderMap);

	// go through every vertex factory
	for(TLinkedList<FVertexFactoryType*>::TIterator It(FVertexFactoryType::GetTypeList()); It; It.Next())
	{
		FVertexFactoryType* CurrentVFType = *It;
		// find the mesh material shader map corresponding with the current vertex factory in the material shader map to copy
		const FMeshMaterialShaderMap* OtherMeshMaterialShaderMap = OtherMaterialShaderMap->GetMeshShaderMap(CurrentVFType);
		if (OtherMeshMaterialShaderMap)
		{
			// find the mesh material shader map in this shader map
			FMeshMaterialShaderMap* CurrentMeshMaterialShaderMap = OrderedMeshShaderMaps(CurrentVFType->GetId());
			if (CurrentMeshMaterialShaderMap)
			{
				// if both material shader maps have shaders for this vertex factory then merge them
				CurrentMeshMaterialShaderMap->Merge(OtherMeshMaterialShaderMap);
			}
			else
			{
				// if only the other shader map has shaders for this vertex factory then copy them over
				new (MeshShaderMaps) FMeshMaterialShaderMap(*OtherMeshMaterialShaderMap);
			}
		}
	}

	// Reinitialize the ordered mesh shader maps
	InitOrderedMeshShaderMaps();
}

/**
 * AddGuidAliases - finds corresponding guids and adds them to the FShaders alias list
 * @param OtherMaterialShaderMap contains guids that will exist in a compressed shader cache, but will not necessarily have FShaders
 * @return FALSE if these two shader maps are not compatible
 */
UBOOL FMaterialShaderMap::AddGuidAliases(const FMaterialShaderMap* OtherMaterialShaderMap)
{

	check(bCompilationFinalized && OtherMaterialShaderMap->bCompilationFinalized);

	// Add guids for the FMaterialShaderTypes
	if (!TShaderMap<FMaterialShaderType>::AddGuidAliases(OtherMaterialShaderMap))
	{
		return FALSE;
	}


	// go through every vertex factory
	for(TLinkedList<FVertexFactoryType*>::TIterator It(FVertexFactoryType::GetTypeList()); It; It.Next())
	{
		FVertexFactoryType* CurrentVFType = *It;
		// find the mesh material shader map corresponding with the current vertex factory in the material shader map to copy
		const FMeshMaterialShaderMap* OtherMeshMaterialShaderMap = OtherMaterialShaderMap->GetMeshShaderMap(CurrentVFType);
		if (OtherMeshMaterialShaderMap)
		{
			// find the mesh material shader map in this shader map
			FMeshMaterialShaderMap* CurrentMeshMaterialShaderMap = OrderedMeshShaderMaps(CurrentVFType->GetId());
			if (CurrentMeshMaterialShaderMap)
			{
				// if both material shader maps have shaders for this vertex factory then merge them
				if (!CurrentMeshMaterialShaderMap->AddGuidAliases(OtherMeshMaterialShaderMap))
				{
					return FALSE;
				}
			}
			else
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

/**
 * Removes all entries in the cache with exceptions based on a shader type
 * @param ShaderType - The shader type to flush
 */
void FMaterialShaderMap::FlushShadersByShaderType(FShaderType* ShaderType)
{
	// flush from all the vertex factory shader maps
	for(INT Index = 0;Index < MeshShaderMaps.Num();Index++)
	{
		MeshShaderMaps(Index).FlushShadersByShaderType(ShaderType);
	}

	if (ShaderType->GetMaterialShaderType())
	{
		RemoveShaderType(ShaderType->GetMaterialShaderType());	
	}
}

/**
 * Removes all entries in the cache with exceptions based on a vertex factory type
 * @param ShaderType - The shader type to flush
 */
void FMaterialShaderMap::FlushShadersByVertexFactoryType(const FVertexFactoryType* VertexFactoryType)
{
	for (INT Index = 0; Index < MeshShaderMaps.Num(); Index++)
	{
		FVertexFactoryType* VFType = MeshShaderMaps(Index).GetVertexFactoryType();
		// determine if this shaders vertex factory type should be flushed
		if (VFType == VertexFactoryType)
		{
			// remove the shader map
			MeshShaderMaps.Remove(Index);
			// fix up the counter
			Index--;
		}
	}

	// reset the OrderedMeshShaderMap to remove references to the removed maps
	InitOrderedMeshShaderMaps();
}

void FMaterialShaderMap::Serialize(FArchive& Ar)
{
	check(Ar.Ver() >= VER_MIN_MATERIALSHADERMAP);
	TShaderMap<FMaterialShaderType>::Serialize(Ar);
	Ar << MeshShaderMaps;
	Ar << MaterialId;
	Ar << FriendlyName;

	StaticParameters.Serialize(Ar);

	if (Ar.Ver() >= VER_UNIFORM_EXPRESSIONS_IN_SHADER_CACHE)
	{
		UniformExpressionSet.Serialize(Ar);
	}

	// serialize the platform enum as a BYTE
	INT TempPlatform = (INT)Platform;
	Ar << TempPlatform;
	Platform = (EShaderPlatform)TempPlatform;

	if(Ar.IsLoading())
	{
		// When loading, reinitialize OrderedMeshShaderMaps from the new contents of MeshShaderMaps.
		InitOrderedMeshShaderMaps();
	}
}

void FMaterialShaderMap::RemovePendingMaterial(FMaterial* Material)
{
	for (TMap<FMaterialShaderMap*, TArray<FMaterial*> >::TIterator It(ShaderMapsBeingCompiled); It; ++It)
	{
		TArray<FMaterial*>& Materials = It.Value();
		Materials.RemoveItem(Material);
	}
}

const FMeshMaterialShaderMap* FMaterialShaderMap::GetMeshShaderMap(FVertexFactoryType* VertexFactoryType) const
{
	checkSlow(bCompilationFinalized);
	const FMeshMaterialShaderMap* MeshShaderMap = OrderedMeshShaderMaps(VertexFactoryType->GetId());
	checkSlow(!MeshShaderMap || MeshShaderMap->GetVertexFactoryType() == VertexFactoryType);
	return MeshShaderMap;
}

void FMaterialShaderMap::InitOrderedMeshShaderMaps()
{
	OrderedMeshShaderMaps.Empty(FVertexFactoryType::GetNumVertexFactoryTypes());
	OrderedMeshShaderMaps.AddZeroed(FVertexFactoryType::GetNumVertexFactoryTypes());
	for (INT Index = 0;Index < MeshShaderMaps.Num();Index++)
	{
		// it's possible for mobile devices that load the d3d shader cache to have this be NULL
		if (MeshShaderMaps(Index).GetVertexFactoryType())
		{
			const INT VFIndex = MeshShaderMaps(Index).GetVertexFactoryType()->GetId();
			OrderedMeshShaderMaps(VFIndex) = &MeshShaderMaps(Index);
		}
	}
}

/**
 * Dump material stats for a given platform.
 * 
 * @param	Platform	Platform to dump stats for.
 */
void DumpMaterialStats( EShaderPlatform Platform )
{
#if ALLOW_DEBUG_FILES
	FDiagnosticTableViewer MaterialViewer(*FDiagnosticTableViewer::GetUniqueTemporaryFilePath(TEXT("MaterialStats")));

	// Mapping from friendly material name to shaders associated with it.
	TMultiMap<FString,FShader*> MaterialToShaderMap;
	// Set of material names.
	TSet<FString> MaterialNames;

#if !CONSOLE
	// Load shader cache. We only look at reference shader cache.
	UShaderCache* ShaderCache = GetReferenceShaderCache( Platform );

	// Iterate over all material maps part of the shader cache.
	TArray<TRefCountPtr<FMaterialShaderMap> > MaterialShaderMapArray = ShaderCache->GetMaterialShaderMap();
	for( INT MaterialShaderMapArrayIndex=0; MaterialShaderMapArrayIndex<MaterialShaderMapArray.Num(); MaterialShaderMapArrayIndex++ )
	{
		// Get list of shaders associated with material.
		TRefCountPtr<FMaterialShaderMap> MaterialShaderMap = MaterialShaderMapArray(MaterialShaderMapArrayIndex);
#else
	// Look at in-memory shader use.
	for( TMap<FStaticParameterSet,FMaterialShaderMap*>::TIterator It(FMaterialShaderMap::GIdToMaterialShaderMap[Platform]); It; ++It )
	{
		FMaterialShaderMap* MaterialShaderMap = It.Value();
#endif
		TMap<FGuid,FShader*> Shaders;
		MaterialShaderMap->GetShaderList( Shaders );

		// Add friendly name to list of materials.
		FString FriendlyName = MaterialShaderMap->GetFriendlyName();
		MaterialNames.Add( FriendlyName );

		// Add shaders to mapping per friendly name as there might be multiple
		for( TMap<FGuid,FShader*>::TConstIterator It(Shaders); It; ++It )
		{
			FShader* Shader = It.Value();
			MaterialToShaderMap.AddUnique(FriendlyName,Shader);
		}
	}

	// Write a row of headings for the table's columns.
	MaterialViewer.AddColumn(TEXT("Name"));
	MaterialViewer.AddColumn(TEXT("Shaders"));
	MaterialViewer.AddColumn(TEXT("Code Size"));
	MaterialViewer.CycleRow();

	// Iterate over all materials, gathering shader stats.
	INT TotalCodeSize		= 0;
	INT TotalShaderCount	= 0;
	for( TSet<FString>::TConstIterator It(MaterialNames); It; ++It )
	{
		// Retrieve list of shaders in map.
		TArray<FShader*> Shaders;
		MaterialToShaderMap.MultiFind( *It, Shaders );
		
		// Iterate over shaders and gather stats.
		INT CodeSize = 0;
		for( INT ShaderIndex=0; ShaderIndex<Shaders.Num(); ShaderIndex++ )
		{
			FShader* Shader = Shaders(ShaderIndex);
			CodeSize += Shader->GetCode().Num();
		}

		TotalCodeSize += CodeSize;
		TotalShaderCount += Shaders.Num();

		// Dump stats
		MaterialViewer.AddColumn(**It);
		MaterialViewer.AddColumn(TEXT("%u"),Shaders.Num());
		MaterialViewer.AddColumn(TEXT("%u"),CodeSize);
		MaterialViewer.CycleRow();
	}

	// Add a total row.
	MaterialViewer.AddColumn(TEXT("Total"));
	MaterialViewer.AddColumn(TEXT("%u"),TotalShaderCount);
	MaterialViewer.AddColumn(TEXT("%u"),TotalCodeSize);
	MaterialViewer.CycleRow();
#endif
}
