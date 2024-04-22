/*=============================================================================
BranchingPCFShadowRendering.cpp: PCF rendering implementation
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "UnTextureLayout.h"
 
/*-----------------------------------------------------------------------------
DiskSampleGenerator
-----------------------------------------------------------------------------*/

/**
* GenerateSamples - Generates samples in a disk, re-placing if they are too close to already placed ones.
*
* @param SampleOffsets - the array to fill with offsets, must be at least length NumSamples
* @param NumSamples - The number of samples to generate
* @param RadiusBounds - The minimum radius to place samples is stored in X, max in Y
* @param DistanceThresholdFactor - Scales the factor used to decide if a sample needs regenerated.  
*	Reasonable values in [.5, 2].  Lower values allow samples to be clumped together, but higher values
*	cause samples to be placed many times.
* @param MaxReplaceTries - the number of times to try placing a sample before the function should give up and not
*	try to maintain a minimum distance between samples.  This is to provide a bound on the runtime of this algorithm.
*/ 
void FDiskSampleGenerator::GenerateSamples(FVector2D* SampleOffsets, INT NumSamples, FVector2D RadiusBounds, FLOAT DistanceThresholdFactor, INT MaxReplaceTries)
{
	FLOAT SampleRadius = RadiusBounds.Y;
	//calculate the minimum distance that samples must be apart to keep a new sample
	//this distance increases with the radius and decreases with the number of samples required along one axis
	FLOAT DistanceThreshold = DistanceThresholdFactor * SampleRadius / appSqrt(NumSamples);
	
	//debugf(TEXT("Begin %i samples ------------------------------"), NumSamples);
	for (INT i = 0; i < NumSamples; i++)
	{
		FLOAT ClosestSampleDist;
		INT RegenCounter = 0;

		do
		{
			RegenCounter++;
			//get a random distance between the bounds
			FLOAT RandomDist = appFrand() * (RadiusBounds.Y - RadiusBounds.X) + RadiusBounds.X;
			//get a random angle
			FLOAT RandomAngle = appFrand() * 2.0f * (FLOAT)PI;
			//convert the cylindrical coords (radius, theta) to cartesian coordinates
			SampleOffsets[i] = FVector2D(RandomDist * appCos(RandomAngle), RandomDist * appSin(RandomAngle));

			ClosestSampleDist = FLT_MAX;
			//go through each previous sample
			for (INT j = 0; j < i; j++)
			{
				//calculate the distance between them
				FLOAT CurrentDist = (SampleOffsets[i] - SampleOffsets[j]).Size();
				//update the closest distance if appropriate
				if (CurrentDist < ClosestSampleDist)
				{
					ClosestSampleDist = CurrentDist;
				}
			}
		}
		//regenerate the sample if it is closer to another sample than the minimum distance, and not too many tries
		while (ClosestSampleDist < DistanceThreshold && RegenCounter < MaxReplaceTries);

		//debugf(TEXT("FVector2D(%ff, %ff),"), SampleOffsets[i].X, SampleOffsets[i].Y);
	}
}

/*-----------------------------------------------------------------------------
FBranchingPCFProjectionPixelShader
-----------------------------------------------------------------------------*/

/**
* A macro for declaring a new instance of the FBranchingPCFProjectionPixelShader template
*/
#define IMPLEMENT_BPCF_SHADER_TYPE(BranchingPCFPolicy,ShaderName,SourceFilename,FunctionName,Frequency,MinVersion) \
	template<> \
	FBranchingPCFProjectionPixelShader<BranchingPCFPolicy>::ShaderMetaType FBranchingPCFProjectionPixelShader<BranchingPCFPolicy>::StaticType( \
	TEXT(#ShaderName), \
	SourceFilename, \
	FunctionName, \
	Frequency, \
	Max((UINT)VER_MIN_SHADER,(UINT)MinVersion), \
	Max((UINT)LICENSEE_VER_MIN_SHADER,(UINT)0), \
	FBranchingPCFProjectionPixelShader<BranchingPCFPolicy>::ConstructSerializedInstance, \
	FBranchingPCFProjectionPixelShader<BranchingPCFPolicy>::ConstructCompiledInstance, \
	FBranchingPCFProjectionPixelShader<BranchingPCFPolicy>::ModifyCompilationEnvironment, \
	FBranchingPCFProjectionPixelShader<BranchingPCFPolicy>::ShouldCache \
	);

// Branching PCF shadow projection global pixel shaders
// typedefs required to get around macro expansion failure due to commas in template argument list
// 3 quality levels of manual PCF

typedef FBranchingPCFProjectionPixelShader<FLowQualityManualPCF> LowQualityShaderName;
IMPLEMENT_BPCF_SHADER_TYPE(FLowQualityManualPCF,LowQualityShaderName,TEXT("BranchingPCFProjectionPixelShader"),TEXT("Main"),SF_Pixel, VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD);

typedef FBranchingPCFProjectionPixelShader<FMediumQualityManualPCF> MediumQualityShaderName;
IMPLEMENT_BPCF_SHADER_TYPE(FMediumQualityManualPCF,MediumQualityShaderName,TEXT("BranchingPCFProjectionPixelShader"),TEXT("Main"),SF_Pixel, VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD);

typedef FBranchingPCFProjectionPixelShader<FHighQualityManualPCF> HighQualityShaderName;
IMPLEMENT_BPCF_SHADER_TYPE(FHighQualityManualPCF,HighQualityShaderName,TEXT("BranchingPCFProjectionPixelShader"),TEXT("Main"),SF_Pixel, VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD);

// 3 quality levels of hardware PCF
typedef FBranchingPCFProjectionPixelShader<FLowQualityHwPCF> HwPCFLowQualityShaderName;
IMPLEMENT_BPCF_SHADER_TYPE(FLowQualityHwPCF,HwPCFLowQualityShaderName,TEXT("BranchingPCFProjectionPixelShader"),TEXT("HardwarePCFMain"),SF_Pixel, VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD);

typedef FBranchingPCFProjectionPixelShader<FMediumQualityHwPCF> HwPCFMediumQualityShaderName;
IMPLEMENT_BPCF_SHADER_TYPE(FMediumQualityHwPCF,HwPCFMediumQualityShaderName,TEXT("BranchingPCFProjectionPixelShader"),TEXT("HardwarePCFMain"),SF_Pixel, VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD);

typedef FBranchingPCFProjectionPixelShader<FHighQualityHwPCF> HwPCFHighQualityShaderName;
IMPLEMENT_BPCF_SHADER_TYPE(FHighQualityHwPCF,HwPCFHighQualityShaderName,TEXT("BranchingPCFProjectionPixelShader"),TEXT("HardwarePCFMain"),SF_Pixel, VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD);

// 3 quality levels of Fetch4 PCF
typedef FBranchingPCFProjectionPixelShader<FLowQualityFetch4PCF> Fetch4PCFLowQualityShaderName;
IMPLEMENT_BPCF_SHADER_TYPE(FLowQualityFetch4PCF,Fetch4PCFLowQualityShaderName,TEXT("BranchingPCFProjectionPixelShader"),TEXT("Fetch4Main"),SF_Pixel, 0);

typedef FBranchingPCFProjectionPixelShader<FMediumQualityFetch4PCF> Fetch4PCFMediumQualityShaderName;
IMPLEMENT_BPCF_SHADER_TYPE(FMediumQualityFetch4PCF,Fetch4PCFMediumQualityShaderName,TEXT("BranchingPCFProjectionPixelShader"),TEXT("Fetch4Main"),SF_Pixel, 0);

typedef FBranchingPCFProjectionPixelShader<FHighQualityFetch4PCF> Fetch4PCFHighQualityShaderName;
IMPLEMENT_BPCF_SHADER_TYPE(FHighQualityFetch4PCF,Fetch4PCFHighQualityShaderName,TEXT("BranchingPCFProjectionPixelShader"),TEXT("Fetch4Main"),SF_Pixel, 0);


/* SetBranchingPCFParameters - chooses the appropriate instance of the FBranchingPCFProjectionPixelShader class template,
*							 and sets its parameters.
*
* @param View - current View
* @param ShadowInfo - current FProjectedShadowInfo
* @param LightShadowQuality - light's filter quality setting
*/
FShader* SetBranchingPCFParameters(
	INT ViewIndex,
	const FSceneView& View, 
	const FProjectedShadowInfo* ShadowInfo, 
	BYTE LightShadowQuality) 
{
	FBranchingPCFProjectionPixelShaderInterface * BranchingPCFPixelShader = NULL;

	//apply the system settings bias to the light's shadow quality
	BYTE EffectiveShadowFilterQuality = Max(LightShadowQuality + GSystemSettings.ShadowFilterQualityBias, 0);

	//choose quality based on global settings if the light is using the default projection technique,
	//otherwise use the light's projection technique
	if (EffectiveShadowFilterQuality == SFQ_Low) 
	{
		//use the appropriate shader based on hardware capabilities
		if (GSceneRenderTargets.IsHardwarePCFSupported())
		{
			TShaderMapRef< FBranchingPCFProjectionPixelShader<FLowQualityHwPCF> > PixelShader(GetGlobalShaderMap());
			BranchingPCFPixelShader = *PixelShader;
		} 
		else if (GSceneRenderTargets.IsFetch4Supported())
		{
			TShaderMapRef< FBranchingPCFProjectionPixelShader<FLowQualityFetch4PCF> > PixelShader(GetGlobalShaderMap());
			BranchingPCFPixelShader = *PixelShader;
		} 
		else
		{
			TShaderMapRef< FBranchingPCFProjectionPixelShader<FLowQualityManualPCF> > PixelShader(GetGlobalShaderMap());
			BranchingPCFPixelShader = *PixelShader;
		}
	} 
	else if (EffectiveShadowFilterQuality == SFQ_Medium) 
	{
		//use the appropriate shader based on hardware capabilities
		if (GSceneRenderTargets.IsHardwarePCFSupported())
		{
			TShaderMapRef< FBranchingPCFProjectionPixelShader<FMediumQualityHwPCF> > PixelShader(GetGlobalShaderMap());
			BranchingPCFPixelShader = *PixelShader;
		} 
		else if (GSceneRenderTargets.IsFetch4Supported())
		{
			TShaderMapRef< FBranchingPCFProjectionPixelShader<FMediumQualityFetch4PCF> > PixelShader(GetGlobalShaderMap());
			BranchingPCFPixelShader = *PixelShader;
		} 
		else
		{
			TShaderMapRef< FBranchingPCFProjectionPixelShader<FMediumQualityManualPCF> > PixelShader(GetGlobalShaderMap());
			BranchingPCFPixelShader = *PixelShader;
		}
	}
	else
	{
		//use the appropriate shader based on hardware capabilities
		if (GSceneRenderTargets.IsHardwarePCFSupported())
		{
			TShaderMapRef< FBranchingPCFProjectionPixelShader<FHighQualityHwPCF> > PixelShader(GetGlobalShaderMap());
			BranchingPCFPixelShader = *PixelShader;
		} 
		else if (GSceneRenderTargets.IsFetch4Supported())
		{
			TShaderMapRef< FBranchingPCFProjectionPixelShader<FHighQualityFetch4PCF> > PixelShader(GetGlobalShaderMap());
			BranchingPCFPixelShader = *PixelShader;
		} 
		else
		{
			TShaderMapRef< FBranchingPCFProjectionPixelShader<FHighQualityManualPCF> > PixelShader(GetGlobalShaderMap());
			BranchingPCFPixelShader = *PixelShader;
		}
	}

	BranchingPCFPixelShader->SetParameters(ViewIndex,View,ShadowInfo);
	return BranchingPCFPixelShader;
}

/** ShouldUseBranchingPCF - indicates whether or not to use the Branching PCF shadows based on global settings and per-light settings
*
* @param ShadowProjectionTechnique - the shadow technique of the light in question, a member of EShadowProjectionTechnique
* @return UBOOL - TRUE if Branching PCF should be used
*/
UBOOL ShouldUseBranchingPCF(BYTE ShadowProjectionTechnique)
{
	// Uniform PCF with fetch 4 currently does not work
	if (GSceneRenderTargets.IsFetch4Supported())
	{
		return TRUE;
	}

	//if the light has no technique specified and the global setting is to use BPCF
	if ((ShadowProjectionTechnique == ShadowProjTech_Default) && GSystemSettings.bEnableBranchingPCFShadows)
	{
		return TRUE;
	}
	//if the light has a technique specified and it is to use BPCF
	else if (ShadowProjectionTechnique == ShadowProjTech_BPCF_Low 
		|| ShadowProjectionTechnique == ShadowProjTech_BPCF_Medium 
		|| ShadowProjectionTechnique == ShadowProjTech_BPCF_High )
	{
		return TRUE;
	} 

	return FALSE;
}
