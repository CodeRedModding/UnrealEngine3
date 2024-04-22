/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __AMBIENTOCCLUSIONRENDERING_H__
#define __AMBIENTOCCLUSIONRENDERING_H__

struct FDownsampleDimensions
{
	UINT Factor;
	INT TargetX;
	INT TargetY;
	INT TargetSizeX;
	INT TargetSizeY;
	FLOAT ViewSizeX;
	FLOAT ViewSizeY;

	FDownsampleDimensions() {}
	FDownsampleDimensions(const FViewInfo& View);
};

struct FAmbientOcclusionSettings;

/** Sets common ambient occlusion parameters */
class FAmbientOcclusionParams
{
public:

	/** Binds the parameters using a compiled shader's parameter map. */
	void Bind(const FShaderParameterMap& ParameterMap);

	/** Sets the scene texture parameters for the given view. */
	void Set(
		const FDownsampleDimensions& DownsampleDimensions,
		FShader* PixelShader,
		ESamplerFilter AOFilter,
		const FTexture2DRHIRef& Input);

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FAmbientOcclusionParams& P);

	FVector4 AOScreenPositionScaleBias;

private:
	FShaderResourceParameter AmbientOcclusionTextureParameter;
	FShaderResourceParameter AOHistoryTextureParameter;
	FShaderParameter AOScreenPositionScaleBiasParameter;
	FShaderParameter ScreenEdgeLimitsParameter;
};

struct FAmbientOcclusionSettings
{
	//See AmbientOcclusionEffect.uc for descriptions
	FLinearColor OcclusionColor;
	FLOAT OcclusionPower;
	FLOAT OcclusionScale;
	FLOAT OcclusionBias;
	FLOAT MinOcclusion;
	FLOAT OcclusionRadius;
	EAmbientOcclusionQuality OcclusionQuality;
	FLOAT OcclusionFadeoutMinDistance;
	FLOAT OcclusionFadeoutMaxDistance;
	FLOAT HaloDistanceThreshold;
	FLOAT HaloDistanceScale;
	FLOAT HaloOcclusion;
	FLOAT EdgeDistanceThreshold;
	FLOAT EdgeDistanceScale;
	FLOAT FilterDistanceScale;
	FLOAT HistoryOcclusionConvergenceTime;
	FLOAT HistoryWeightConvergenceTime;
	UBOOL bAngleBasedSSAO;

	FAmbientOcclusionSettings(const UAmbientOcclusionEffect* InEffect);
};

enum EAOMaskType
{
	AO_OcclusionMask,
	AO_HistoryMask,
	AO_HistoryUpdate,
	AO_HistoryMaskManualDepthTest,
	AO_HistoryUpdateManualDepthTest
};

#endif
