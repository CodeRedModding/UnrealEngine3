/*=============================================================================
	ScalabilityOptions.cpp: Unreal engine HW compat scalability system.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnTerrain.h"
#include "EngineSpeedTreeClasses.h"
#include "EngineDecalClasses.h"
#include "EngineUserInterfaceClasses.h"
#include "SceneRenderTargets.h"
#if IPHONE
	#include "IPhoneObjCWrapper.h"
#endif
#if WITH_OPEN_AUTOMATE
#include "OpenAutomate.h"
#endif

/*-----------------------------------------------------------------------------
	FSystemSettings
-----------------------------------------------------------------------------*/

/** Global accessor */
FSystemSettings GSystemSettings;

FVSSBool SimpleBool;
FVSSGenericInt VSSDetailMode( 0, 2, 1 );
FVSSGenericInt VSSSkeletalMeshLODBias( -2, 2, 1 );
FVSSGenericInt VSSMaxAnisotropy( 1, 16, 1 );
FVSSGenericInt VSSMaxMultiSamples( 1, 16, 1 );
FVSSGenericInt VSSMaxFilterBlurSampleCount( 1, 16, 1 );
FVSSGenericInt VSSMaxShadowResolution( 256, 2048, 256 );
FVSSGenericInt VSSResX( 640, 2560, 40 );
FVSSGenericInt VSSResY( 400, 1600, 20 );
FVSSGenericFloat VSSMaxDrawDistanceScale( 0.5f, 1.5f );
FVSSGenericFloat VSSScreenPercentage( 0.6f, 1.0f );
FVSSGenericFloat VSSShadowTexels( 0.5f, 2.5f );

FSystemSetting FSystemSettings::SystemSettings[] =
{
	/** Whether to allow static decals.	*/
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "StaticDecals" ), &GSystemSettings.bAllowStaticDecals, &SimpleBool, TEXT( "Whether to allow static decals." ) },
	/** Whether to allow dynamic decals. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "DynamicDecals" ), &GSystemSettings.bAllowDynamicDecals, &SimpleBool, TEXT( "Whether to allow dynamic decals." ) },
	/** Whether to allow decals that have not been placed in static draw lists and have dynamic view relevance */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "UnbatchedDecals" ), &GSystemSettings.bAllowUnbatchedDecals, &SimpleBool, TEXT( "Whether to allow decals that have not been placed in static draw lists and have dynamic view relevance." ) },
	/** Scale factor for distance culling decals. */
	{ SST_FLOAT, SSI_SCALABILITY, TEXT( "DecalCullDistanceScale" ), &GSystemSettings.DecalCullDistanceScale, &VSSMaxDrawDistanceScale, TEXT( "Scale factor for distance culling decals." ) },
	/** Whether to allow dynamic lights. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "DynamicLights" ), &GSystemSettings.bAllowDynamicLights, &SimpleBool, TEXT( "Whether to allow dynamic lights." ) },
	/** Whether to allow dynamic shadows. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "DynamicShadows" ), &GSystemSettings.bAllowDynamicShadows, &SimpleBool, TEXT( "Whether to allow dynamic shadows." ) },
	/** Whether to allow dynamic light environments to cast shadows. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "LightEnvironmentShadows" ), &GSystemSettings.bAllowLightEnvironmentShadows, &SimpleBool, TEXT( "Whether to allow dynamic light environments to cast shadows." ) },
	/** Whether to composte dynamic lights into light environments. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "CompositeDynamicLights" ), &GSystemSettings.bUseCompositeDynamicLights, &SimpleBool, TEXT( "Whether to composte dynamic lights into light environments." ) },
	/** Whether to allow light environments to use SH lights for secondary lighting. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "SHSecondaryLighting" ), &GSystemSettings.bAllowSHSecondaryLighting, &SimpleBool, TEXT( "Whether to allow light environments to use SH lights for secondary lighting." ) },
	/** Whether to allow directional lightmaps, which use the material's normal and specular. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "DirectionalLightmaps" ), &GSystemSettings.bAllowDirectionalLightMaps, &SimpleBool, TEXT( "Whether to allow directional lightmaps, which use the material's normal and specular." ) },
	/** Whether to allow motion blur. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "MotionBlur" ), &GSystemSettings.bAllowMotionBlur, &SimpleBool, TEXT( "Whether to allow motion blur." ) },
	/** Whether to allow motion blur to be paused. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "MotionBlurPause" ), &GSystemSettings.bAllowMotionBlurPause, &SimpleBool, TEXT( "Whether to allow motion blur to be paused." ) },
	/** State of the console variable MotionBlurSkinning. */
	{ SST_INT, SSI_UNKNOWN, TEXT( "MotionBlurSkinning" ), &GSystemSettings.MotionBlurSkinning, NULL, TEXT( "State of the console variable MotionBlurSkinning." ) },
	/** Whether to allow depth of field. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "DepthOfField" ), &GSystemSettings.bAllowDepthOfField, &SimpleBool, TEXT( "Whether to allow depth of field." ) },
	/** Whether to allow ambient occlusion. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "AmbientOcclusion" ), &GSystemSettings.bAllowAmbientOcclusion, &SimpleBool, TEXT( "Whether to allow ambient occlusion." ) },
	/** Whether to allow bloom. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "Bloom" ), &GSystemSettings.bAllowBloom, &SimpleBool, TEXT( "Whether to allow bloom." ) },
	/** Whether to allow light shafts. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "bAllowLightShafts" ), &GSystemSettings.bAllowLightShafts, &SimpleBool, TEXT( "Whether to allow light shafts." ) },
	/** Whether to allow distortion. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "Distortion" ), &GSystemSettings.bAllowDistortion, &SimpleBool, TEXT( "Whether to allow distortion." ) },
	/** Whether to allow distortion to use bilinear filtering when sampling the scene color during its apply pass. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "FilteredDistortion" ), &GSystemSettings.bAllowFilteredDistortion, &SimpleBool, TEXT( "Whether to allow distortion to use bilinear filtering when sampling the scene color during its apply pass." ) },
	/** Whether to allow dropping distortion on particles based on WorldInfo::bDropDetail. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "DropParticleDistortion" ), &GSystemSettings.bAllowParticleDistortionDropping, &SimpleBool, TEXT( "Whether to allow dropping distortion on particles based on WorldInfo::bDropDetail." ) },
	/** Whether to allow downsampled transluency. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "bAllowDownsampledTranslucency" ), &GSystemSettings.bAllowDownsampledTranslucency, &SimpleBool, TEXT( "Whether to allow downsampled transluency." ) },
	/** Whether to allow rendering of SpeedTree leaves. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "SpeedTreeLeaves" ), &GSystemSettings.bAllowSpeedTreeLeaves, &SimpleBool, TEXT( "Whether to allow rendering of SpeedTree leaves." ) },
	/** Whether to allow rendering of SpeedTree fronds. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "SpeedTreeFronds" ), &GSystemSettings.bAllowSpeedTreeFronds, &SimpleBool, TEXT( "Whether to allow rendering of SpeedTree fronds." ) },
	/** If enabled, texture will only be streamed in, not out. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "OnlyStreamInTextures" ), &GSystemSettings.bOnlyStreamInTextures, &SimpleBool, TEXT( "If enabled, texture will only be streamed in, not out." ) },
	/** Whether to allow rendering of LensFlares. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "LensFlares" ), &GSystemSettings.bAllowLensFlares, &SimpleBool, TEXT( "Whether to allow rendering of LensFlares." ) },
	/** Whether to allow fog volumes. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "FogVolumes" ), &GSystemSettings.bAllowFogVolumes, &SimpleBool, TEXT( "Whether to allow fog volumes." ) },
	/** Whether to allow floating point render targets to be used. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "FloatingPointRenderTargets" ), &GSystemSettings.bAllowFloatingPointRenderTargets, &SimpleBool, TEXT( "Whether to allow floating point render targets to be used." ) },
	/** Whether to allow the rendering thread to lag one frame behind the game thread. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "OneFrameThreadLag" ), &GSystemSettings.bAllowOneFrameThreadLag, &SimpleBool, TEXT( "Whether to allow the rendering thread to lag one frame behind the game thread." ) },
	/** Whether to use VSync or not. */
	{ SST_BOOL, SSI_PREFERENCE, TEXT( "UseVsync" ), &GSystemSettings.bUseVSync, &SimpleBool, TEXT( "Whether to use VSync or not." ) },
	/** Whether to upscale the screen to take up the full front buffer.	*/
	{ SST_BOOL, SSI_DEBUG, TEXT( "UpscaleScreenPercentage" ), &GSystemSettings.bUpscaleScreenPercentage, &SimpleBool, TEXT( "Whether to upscale the screen to take up the full front buffer." ) },
	/** Fullscreen. */
	{ SST_BOOL, SSI_PREFERENCE, TEXT( "Fullscreen" ), &GSystemSettings.bFullscreen, &SimpleBool, TEXT( "Fullscreen." ) },
	/** Whether to use OpenGL when it's available. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "AllowOpenGL" ), &GSystemSettings.bAllowOpenGL, &SimpleBool, TEXT( "Whether to use OpenGL when it's available." ) },
	/** Whether to allow radial blur effects to render. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "AllowRadialBlur" ), &GSystemSettings.bAllowRadialBlur, &SimpleBool, TEXT( "Whether to allow radial blur effects to render." ) },
	/** Whether to allow sub-surface scattering to render. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "AllowSubsurfaceScattering" ), &GSystemSettings.bAllowSubsurfaceScattering, &SimpleBool, TEXT( "Whether to allow sub-surface scattering to render." ) },
	/** Whether to allow image reflections to render. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "AllowImageReflections" ), &GSystemSettings.bAllowImageReflections, &SimpleBool, TEXT( "Whether to allow image reflections to render." ) },
	/** Whether to allow image reflections to be shadowed. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "AllowImageReflectionShadowing" ),&GSystemSettings.bAllowImageReflectionShadowing, &SimpleBool, TEXT( "Whether to allow image reflections to be shadowed." ) },
	/** Whether to keep separate translucency (for better Depth of Field), experimental. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "bAllowSeparateTranslucency" ), &GSystemSettings.bAllowSeparateTranslucency, &SimpleBool, TEXT( "Whether to keep separate translucency (for better Depth of Field), experimental." ) },
	/** Whether to allow post process MLAA to render. requires extra memory	*/
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "bAllowPostprocessMLAA" ), &GSystemSettings.bAllowPostprocessMLAA, &SimpleBool, TEXT( "Whether to allow post process MLAA to render. requires extra memory." ) },
	/** Whether to use high quality materials when low quality exist. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "bAllowHighQualityMaterials" ), &GSystemSettings.bAllowHighQualityMaterials, &SimpleBool, TEXT( "Whether to use high quality materials when low quality exist." ) },
	/** Max filter sample count (clamp can cause boxy appearance but allows for better performance, only numbers below 16 have effect)	*/
	{ SST_INT, SSI_SCALABILITY, TEXT( "MaxFilterBlurSampleCount" ), &GSystemSettings.MaxFilterBlurSampleCount, &VSSMaxFilterBlurSampleCount, TEXT( "Max filter sample count." ) },
	/** LOD bias for skeletal meshes. */
	{ SST_INT, SSI_SCALABILITY, TEXT( "SkeletalMeshLODBias" ), &GSystemSettings.SkeletalMeshLODBias, &VSSSkeletalMeshLODBias, TEXT( "LOD bias for skeletal meshes." ) },
	/** LOD bias for particle systems. */
	{ SST_INT, SSI_DEBUG, TEXT( "ParticleLODBias" ), &GSystemSettings.ParticleLODBias, NULL, TEXT( "LOD bias for particle systems." ) },
	/** Current detail mode; determines whether components of actors should be updated/ ticked. */
	{ SST_INT, SSI_SCALABILITY, TEXT( "DetailMode" ), &GSystemSettings.DetailMode, &VSSDetailMode, TEXT( "Current detail mode; determines whether components of actors should be updated/ ticked." ) },
	/** Scale applied to primitive's MaxDrawDistance. */
	{ SST_FLOAT, SSI_SCALABILITY, TEXT( "MaxDrawDistanceScale" ), &GSystemSettings.MaxDrawDistanceScale, &VSSMaxDrawDistanceScale, TEXT( "Scale applied to primitive's MaxDrawDistance." ) },
	/** Quality bias for projected shadow buffer filtering. Higher values use better quality filtering. */
	{ SST_INT, SSI_UNKNOWN, TEXT( "ShadowFilterQualityBias" ), &GSystemSettings.ShadowFilterQualityBias, NULL, TEXT( "Quality bias for projected shadow buffer filtering. Higher values use better quality filtering." ) },
	/** Maximum level of anisotropy used. */
	{ SST_INT, SSI_SCALABILITY, TEXT( "MaxAnisotropy" ), &GSystemSettings.MaxAnisotropy, &VSSMaxAnisotropy, TEXT( "Maximum level of anisotropy used." ) },
	/** The maximum number of MSAA samples to use. */
	{ SST_INT, SSI_DEBUG, TEXT( "MaxMultiSamples" ), &GSystemSettings.MaxMultiSamples, &VSSMaxMultiSamples, TEXT( "The maximum number of MSAA samples to use." ) },
	/** UKNOWN */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "bAllowD3D9MSAA" ), &GSystemSettings.bAllowD3D9MSAA, &SimpleBool, TEXT( "UKNOWN" ) },
	/** UKNOWN */
	{ SST_BOOL, SSI_DEBUG, TEXT( "bAllowTemporalAA" ), &GSystemSettings.bAllowTemporalAA, &SimpleBool, TEXT( "UKNOWN" ) },
	/** UKNOWN */
	{ SST_FLOAT, SSI_DEBUG, TEXT( "TemporalAA_MinDepth" ), &GSystemSettings.TemporalAA_MinDepth, NULL, TEXT( "UKNOWN" ) },
	/** UKNOWN */
	{ SST_FLOAT, SSI_DEBUG, TEXT( "TemporalAA_StartDepthVelocityScale" ), &GSystemSettings.TemporalAA_StartDepthVelocityScale, NULL, TEXT( "UKNOWN" ) },
	/** min dimensions (in texels) allowed for rendering shadow subject depths */
	{ SST_INT, SSI_SCALABILITY, TEXT( "MinShadowResolution" ), &GSystemSettings.MinShadowResolution, NULL, TEXT( "min dimensions (in texels) allowed for rendering shadow subject depths." ) },
	/** min dimensions (in texels) allowed for rendering preshadow depths. */
	{ SST_INT, SSI_SCALABILITY, TEXT( "MinPreShadowResolution" ), &GSystemSettings.MinPreShadowResolution, NULL, TEXT( "min dimensions (in texels) allowed for rendering preshadow depths." ) },
	/** max square dimensions (in texels) allowed for rendering shadow subject depths. */
	{ SST_INT, SSI_SCALABILITY, TEXT( "MaxShadowResolution" ), &GSystemSettings.MaxShadowResolution, &VSSMaxShadowResolution, TEXT( "max square dimensions (in texels) allowed for rendering shadow subject depths." ) },
	/** max square dimensions (in texels) allowed for rendering whole scene shadow depths. */
	{ SST_INT, SSI_SCALABILITY, TEXT( "MaxWholeSceneDominantShadowResolution" ), &GSystemSettings.MaxWholeSceneDominantShadowResolution, &VSSMaxShadowResolution, TEXT( "max square dimensions (in texels) allowed for rendering whole scene shadow depths." ) },
	/** Resolution in texel below which shadows are faded out. */
	{ SST_INT, SSI_UNKNOWN, TEXT( "ShadowFadeResolution" ), &GSystemSettings.ShadowFadeResolution, NULL, TEXT( "Resolution in texel below which shadows are faded out." ) },
	/** Resolution in texel below which preshadows are faded out. */
	{ SST_INT, SSI_UNKNOWN, TEXT( "PreShadowFadeResolution" ), &GSystemSettings.PreShadowFadeResolution, NULL, TEXT( "Resolution in texel below which preshadows are faded out." ) },
	/** Controls the rate at which shadows are faded out. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "ShadowFadeExponent" ), &GSystemSettings.ShadowFadeExponent, NULL, TEXT( "Controls the rate at which shadows are faded out." ) },
	/** Screen X resolution. */
	{ SST_INT, SSI_PREFERENCE, TEXT( "ResX" ), &GSystemSettings.ResX, &VSSResX, TEXT( "Screen X resolution." ) },
	/** Screen Y resolution. */
	{ SST_INT, SSI_PREFERENCE, TEXT( "ResY" ), &GSystemSettings.ResY, &VSSResY, TEXT( "Screen Y resolution." ) },
	/** Percentage of screen main view should take up. */
	{ SST_FLOAT, SSI_DEBUG, TEXT( "ScreenPercentage" ), &GSystemSettings.ScreenPercentage, &VSSScreenPercentage, TEXT( "Percentage of screen main view should take up." ) },
	/** Scene capture streaming texture update distance scalar. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "SceneCaptureStreamingMultiplier" ), &GSystemSettings.SceneCaptureStreamingMultiplier, NULL, TEXT( "Scene capture streaming texture update distance scalar." ) },
	/** The ratio of subject pixels to shadow texels. */
	{ SST_FLOAT, SSI_SCALABILITY, TEXT( "ShadowTexelsPerPixel" ), &GSystemSettings.ShadowTexelsPerPixel, &VSSShadowTexels, TEXT( "The ratio of subject pixels to shadow texels." ) },
	/** UKNOWN */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "PreShadowResolutionFactor" ), &GSystemSettings.PreShadowResolutionFactor, NULL, TEXT( "UKNOWN" ) },
	/** Toggle Branching PCF implementation for projected shadows. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "bEnableBranchingPCFShadows" ), &GSystemSettings.bEnableBranchingPCFShadows, &SimpleBool, TEXT( "Toggle Branching PCF implementation for projected shadows." ) },
	/** Whether to allow hardware filtering optimizations like hardware PCF and Fetch4. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "bAllowHardwareShadowFiltering" ), &GSystemSettings.bAllowHardwareShadowFiltering, &SimpleBool, TEXT( "Whether to allow hardware filtering optimizations like hardware PCF and Fetch4." ) },
	/** Global tessellation factor multiplier. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "TessellationAdaptivePixelsPerTriangle" ), &GSystemSettings.TessellationAdaptivePixelsPerTriangle, NULL, TEXT( "Global tessellation factor multiplier." ) },
	/** hack to allow for foreground DPG objects to cast shadows on the world DPG. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "bEnableForegroundShadowsOnWorld" ), &GSystemSettings.bEnableForegroundShadowsOnWorld, &SimpleBool, TEXT( "hack to allow for foreground DPG objects to cast shadows on the world DPG." ) },
	/** Whether to allow foreground DPG self-shadowing. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "bEnableForegroundSelfShadowing" ), &GSystemSettings.bEnableForegroundSelfShadowing, &SimpleBool, TEXT( "Whether to allow foreground DPG self-shadowing." ) },
	/** Whether to allow whole scene dominant shadows. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "bAllowWholeSceneDominantShadows" ), &GSystemSettings.bAllowWholeSceneDominantShadows, &SimpleBool, TEXT( "Whether to allow whole scene dominant shadows." ) },
	/** Whether to use safe and conservative shadow frustum creation that wastes some shadowmap space. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "bUseConservativeShadowBounds" ), &GSystemSettings.bUseConservativeShadowBounds, &SimpleBool, TEXT( "Whether to use safe and conservative shadow frustum creation that wastes some shadowmap space." ) },
	/** Radius, in shadowmap texels, of the filter disk. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "ShadowFilterRadius" ), &GSystemSettings.ShadowFilterRadius, NULL, TEXT( "Radius, in shadowmap texels, of the filter disk." ) },
	/** Depth bias that is applied in the depth pass for all types of projected shadows except VSM. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "ShadowDepthBias" ), &GSystemSettings.ShadowDepthBias, NULL, TEXT( "Depth bias that is applied in the depth pass for all types of projected shadows except VSM." ) },
	/** Higher values make the per object soft shadow comparison sharper, lower values make the transition softer. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "PerObjectShadowTransition" ), &GSystemSettings.PerObjectShadowTransition, NULL, TEXT( "Higher values make the per object soft shadow comparison sharper, lower values make the transition softer." ) },
	/** Higher values make the per scene soft shadow comparison sharper, lower values make the transition softer. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "PerSceneShadowTransition" ), &GSystemSettings.PerSceneShadowTransition, NULL, TEXT( "Higher values make the per scene soft shadow comparison sharper, lower values make the transition softer." ) },
	/** Scale applied to the penumbra size of Cascaded Shadow Map splits, useful for minimizing the transition between splits. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "CSMSplitPenumbraScale" ), &GSystemSettings.CSMSplitPenumbraScale, NULL, TEXT( "Scale applied to the penumbra size of Cascaded Shadow Map splits, useful for minimizing the transition between splits." ) },
	/** Scale applied to the soft comparison transition distance of Cascaded Shadow Map splits, useful for minimizing the transition between splits. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "CSMSplitSoftTransitionDistanceScale" ), &GSystemSettings.CSMSplitSoftTransitionDistanceScale, NULL, TEXT( "Scale applied to the soft comparison transition distance of Cascaded Shadow Map splits, useful for minimizing the transition between splits." ) },
	/** Scale applied to the depth bias of Cascaded Shadow Map splits, useful for minimizing the transition between splits. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "CSMSplitDepthBiasScale" ), &GSystemSettings.CSMSplitDepthBiasScale, NULL, TEXT( "Scale applied to the depth bias of Cascaded Shadow Map splits, useful for minimizing the transition between splits." ) },
	/** Minimum camera FOV for CSM, this is used to prevent shadow shimmering when animating the FOV lower than the min, for example when zooming. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "CSMMinimumFOV" ), &GSystemSettings.CSMMinimumFOV, NULL, TEXT( "Minimum camera FOV for CSM, this is used to prevent shadow shimmering when animating the FOV lower than the min, for example when zooming." ) },
	/** The FOV will be rounded by this factor for the purposes of CSM, which turns shadow shimmering into discrete jumps. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "CSMFOVRoundFactor" ), &GSystemSettings.CSMFOVRoundFactor, NULL, TEXT( "The FOV will be rounded by this factor for the purposes of CSM, which turns shadow shimmering into discrete jumps." ) },
	/** WholeSceneDynamicShadowRadius to use when using CSM to preview unbuilt lighting from a directional light. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "UnbuiltWholeSceneDynamicShadowRadius" ), &GSystemSettings.UnbuiltWholeSceneDynamicShadowRadius, NULL, TEXT( "WholeSceneDynamicShadowRadius to use when using CSM to preview unbuilt lighting from a directional light." ) },
	/** NumWholeSceneDynamicShadowCascades to use when using CSM to preview unbuilt lighting from a directional light. */
	{ SST_INT, SSI_UNKNOWN, TEXT( "UnbuiltNumWholeSceneDynamicShadowCascades" )	, &GSystemSettings.UnbuiltNumWholeSceneDynamicShadowCascades, NULL, TEXT( "NumWholeSceneDynamicShadowCascades to use when using CSM to preview unbuilt lighting from a directional light." ) },
	/** How many unbuilt light-primitive interactions there can be for a light before the light switches to whole scene shadows. */
	{ SST_INT, SSI_UNKNOWN, TEXT( "WholeSceneShadowUnbuiltInteractionThreshold" ), &GSystemSettings.WholeSceneShadowUnbuiltInteractionThreshold, NULL, TEXT( "How many unbuilt light-primitive interactions there can be for a light before the light switches to whole scene shadows." ) },
	/** Whether to allow fractured meshes to take damage. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "bAllowFracturedDamage" ), &GSystemSettings.bAllowFracturedDamage, &SimpleBool, TEXT( "Whether to allow fractured meshes to take damage." ) },
	/** Scales the game-specific number of fractured physics objects allowed. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "NumFracturedPartsScale" ), &GSystemSettings.NumFracturedPartsScale, NULL, TEXT( "Scales the game-specific number of fractured physics objects allowed." ) },
	/** Percent chance of a rigid body spawning after a fractured static mesh is damaged directly.  [0-1] */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "FractureDirectSpawnChanceScale" ), &GSystemSettings.FractureDirectSpawnChanceScale, NULL, TEXT( "Percent chance of a rigid body spawning after a fractured static mesh is damaged directly.  [0-1]" ) },
	/** Percent chance of a rigid body spawning after a fractured static mesh is damaged by radial blast.  [0-1] */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "FractureRadialSpawnChanceScale" ), &GSystemSettings.FractureRadialSpawnChanceScale, NULL, TEXT( "Percent chance of a rigid body spawning after a fractured static mesh is damaged by radial blast.  [0-1]" ) },
	/** Distance scale for whether a fractured static mesh should actually fracture when damaged. */
	{ SST_FLOAT, SSI_SCALABILITY, TEXT( "FractureCullDistanceScale" ), &GSystemSettings.FractureCullDistanceScale, &VSSMaxDrawDistanceScale, TEXT( "Distance scale for whether a fractured static mesh should actually fracture when damaged." ) },
	/** Whether to force CPU access to GPU skinned vertex data. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "bForceCPUAccessToGPUSkinVerts" ), &GSystemSettings.bForceCPUAccessToGPUSkinVerts, &SimpleBool, TEXT( "Whether to force CPU access to GPU skinned vertex data." ) },
	/** Whether to disable instanced skeletal weights. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "bDisableSkeletalInstanceWeights" ), &GSystemSettings.bDisableSkeletalInstanceWeights, &SimpleBool, TEXT( "Whether to disable instanced skeletal weights." ) },
	/** Whether to use high-precision GBuffers. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "HighPrecisionGBuffers" ), &GSystemSettings.bHighPrecisionGBuffers, &SimpleBool, TEXT( "Whether to use high-precision GBuffers." ) },
	/** Whether to allow independent, external displays. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "AllowSecondaryDisplays" )	, &GSystemSettings.bAllowSecondaryDisplays, &SimpleBool, TEXT( "Whether to allow independent, external displays." ) },
	/** The maximum width of any potentially allowed secondary displays (requires bAllowSecondaryDisplays == TRUE) */
	{ SST_INT, SSI_UNKNOWN, TEXT( "SecondaryDisplayMaximumWidth" ), &GSystemSettings.SecondaryDisplayMaximumWidth, NULL, TEXT( "The maximum width of any potentially allowed secondary displays (requires bAllowSecondaryDisplays == TRUE)" ) },
	/** The maximum height of any potentially allowed secondary displays (requires bAllowSecondaryDisplays == TRUE) */
	{ SST_INT, SSI_UNKNOWN, TEXT( "SecondaryDisplayMaximumHeight" ), &GSystemSettings.SecondaryDisplayMaximumHeight, NULL, TEXT( "The maximum height of any potentially allowed secondary displays (requires bAllowSecondaryDisplays == TRUE)" ) },
	/** Enables sleeping once a frame to smooth out CPU usage */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "AllowPerFrameSleep" ), &GSystemSettings.bAllowPerFrameSleep, NULL, TEXT( "TRUE if the application is allowed to sleep once a frame to smooth out CPU usage" ) },
	/** Enables yielding once a frame to give other processes time to run. Note that bAllowPerFrameSleep takes precedence */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "AllowPerFrameYield" ), &GSystemSettings.bAllowPerFrameYield, NULL, TEXT( "TRUE if the application is allowed to yield once a frame to give other processes time to run. Note that bAllowPerFrameSleep takes precedence" ) },

// allow for mobile only platform settings, separated out so they don't need to be specified for non-mobile platforms
#if WITH_MOBILE_RHI
	/** The baseline feature level of the device. */
	{ SST_INT, SSI_MOBILE_SCALABILITY, TEXT( "MobileFeatureLevel" ), &GSystemSettings.MobileFeatureLevel, NULL, TEXT( "The baseline feature level of the device" ) },
	/** Whether to allow fog on mobile. */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "MobileFog" ), &GSystemSettings.bAllowMobileFog, &SimpleBool, TEXT( "Whether to allow fog on mobile." ) },
	/** Whether to use height-fog on mobile, or simple gradient fog. */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "MobileHeightFog" ), &GSystemSettings.bAllowMobileHeightFog, &SimpleBool, TEXT( "Whether to use height-fog on mobile, or simple gradient fog." ) },
	/** Whether to allow vertex specular on mobile. */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "MobileSpecular" ), &GSystemSettings.bAllowMobileSpecular, &SimpleBool, TEXT( "Whether to allow vertex specular on mobile." ) },
	/** Whether to allow bump offset on mobile. */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "MobileBumpOffset" ), &GSystemSettings.bAllowMobileBumpOffset, &SimpleBool, TEXT( "Whether to allow bump offset on mobile" ) },
	/** Whether to allow normal mapping on mobile. */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "MobileNormalMapping" ), &GSystemSettings.bAllowMobileNormalMapping, &SimpleBool, TEXT( "Whether to allow normal mapping on mobile." ) },
	/** Whether to allow environment mapping on mobile. */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "MobileEnvMapping" ), &GSystemSettings.bAllowMobileEnvMapping, &SimpleBool, TEXT( "Whether to allow environment mapping on mobile." ) },
	/** Whether to allow rim lighting on mobile. */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "MobileRimLighting" ), &GSystemSettings.bAllowMobileRimLighting, &SimpleBool, TEXT( "Whether to allow rim lighting on mobile." ) },
	/** Whether to allow color blending on mobile. */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "MobileColorBlending" ), &GSystemSettings.bAllowMobileColorBlending, &SimpleBool, TEXT( "Whether to allow color blending on mobile." ) },
	/** Whether to allow vertex movement on mobile. */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "MobileVertexMovement" ), &GSystemSettings.bAllowMobileVertexMovement, &SimpleBool, TEXT( "Whether to allow vertex movement on mobile." ) },
	/** Whether to allow occlusion queries on mobile. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileOcclusionQueries" ), &GSystemSettings.bAllowMobileOcclusionQueries, &SimpleBool, TEXT( "Whether to allow occlusion queries on mobile." ) },
	/** UKNOWN */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileGlobalGammaCorrection" ), &GSystemSettings.bMobileGlobalGammaCorrection, &SimpleBool, TEXT( "UNKNOWN." ) },
	/** UKNOWN */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileAllowGammaCorrectionWorldOverride" ), &GSystemSettings.bMobileAllowGammaCorrectionLevelOverride, &SimpleBool, TEXT( "UNKNOWN." ) },
	/** Whether to enable a rendering depth pre-pass on mobile. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileAllowDepthPrePass" ), &GSystemSettings.bMobileAllowDepthPrePass, &SimpleBool, TEXT( "Whether to enable a rendering depth pre-pass on mobile." ) },
#if WITH_GFx
	/** Whether to include  */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileGfxGammaCorrection" ), &GSystemSettings.bMobileGfxGammaCorrection, &SimpleBool, TEXT( "Whether to include gamma correction in the scaleform shaders." ) },
#endif
	/** UKNOWN */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileSceneDepthResolveForShadows" ), &GSystemSettings.bMobileSceneDepthResolveForShadows, &SimpleBool, TEXT( "UNKNOWN." ) },
	/** Whether to use preprocessed shaders on mobile. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileUsePreprocessedShaders" ), &GSystemSettings.bUsePreprocessedShaders, &SimpleBool, TEXT( "Whether to use preprocessed shaders on mobile." ) },
	/** Whether to flash the screen red (non-final release only) when a cached shader is not found at runtime. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileFlashRedForUncachedShaders" ), &GSystemSettings.bFlashRedForUncachedShaders, &SimpleBool, TEXT( "Whether to flash the screen red (non-final release only) when a cached shader is not found at runtime." ) },
	/** Whether to issue a "warm-up" draw call for mobile shaders as they are compiled. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileWarmUpPreprocessedShaders" ), &GSystemSettings.bWarmUpPreprocessedShaders, &SimpleBool, TEXT( "Whether to issue a 'warm-up' draw call for mobile shaders as they are compiled." ) },
	/** Whether to dump out preprocessed shaders for mobile as they are encountered/compiled. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileCachePreprocessedShaders" ), &GSystemSettings.bCachePreprocessedShaders, &SimpleBool, TEXT( "Whether to dump out preprocessed shaders for mobile as they are encountered/compiled." ) },
	/** Whether to run dumped out preprocessed shaders through the shader profiler. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileProfilePreprocessedShaders" ), &GSystemSettings.bProfilePreprocessedShaders, &SimpleBool, TEXT( "Whether to run dumped out preprocessed shaders through the shader profiler." ) },
	/** Whether to run the C preprocessor on shaders. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileUseCPreprocessorOnShaders" ), &GSystemSettings.bUseCPreprocessorOnShaders, &SimpleBool, TEXT( "Whether to run the C preprocessor on shaders." ) },
	/** Whether to load the C preprocessed source. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileLoadCPreprocessedShaders" ), &GSystemSettings.bLoadCPreprocessedShaders, &SimpleBool, TEXT( " Whether to load the C preprocessed source." ) },
	/** Whether to share pixel shaders across multiple unreal shaders. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileSharePixelShaders" ), &GSystemSettings.bSharePixelShaders, &SimpleBool, TEXT( "Whether to share pixel shaders across multiple unreal shaders." ) },
	/** Whether to share vertex shaders across multiple unreal shaders. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileShareVertexShaders" ), &GSystemSettings.bShareVertexShaders, &SimpleBool, TEXT( "Whether to share vertex shaders across multiple unreal shaders." ) },
	/** Whether to share shaders program across multiple unreal shaders. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileShareShaderPrograms" ), &GSystemSettings.bShareShaderPrograms, &SimpleBool, TEXT( "Whether to share shaders program across multiple unreal shaders." ) },
	/** Whether to enable MSAA, if the OS supports it. */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "MobileEnableMSAA" ), &GSystemSettings.bEnableMSAA, &SimpleBool, TEXT( "Whether to enable MSAA, if the OS supports it." ) },
	/** TRUE if we try to support mobile modulated shadow. */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "MobileModShadows" ), &GSystemSettings.bMobileModShadows, &SimpleBool, TEXT( "TRUE if we try to support mobile modulated shadow." ) },
	/** TRUE to enable the mobile tilt shift effect. */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "MobileTiltShift" ), &GSystemSettings.bMobileTiltShift, &SimpleBool, TEXT( "TRUE to enable the mobile tilt shift effect." ) },
	/** Value (in MB) to declare for maximum mobile memory on this device. */
	{ SST_INT, SSI_DEBUG, TEXT( "MobileMaxMemory" ), &GSystemSettings.MobileMaxMemory, NULL, TEXT( "Value (in MB) to declare for maximum mobile memory on this device." ) },
	/** Holds if we are using high resolution timing on this device. */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "bMobileUsingHighResolutionTiming" ), &GSystemSettings.bMobileUsingHighResolutionTiming, NULL, TEXT( "TRUE if high resolution timing is enabled for this device" ) },
	/** Whether to clear the depth buffer between DPGs. */
	{ SST_BOOL, SSI_DEBUG, TEXT( "MobileClearDepthBetweenDPG" ), &GSystemSettings.bMobileClearDepthBetweenDPG, &SimpleBool, TEXT( "Whether to clear the depth buffer between DPGs." ) },
	/** Whether to allow color grading on mobile. */
	{ SST_BOOL, SSI_MOBILE_SCALABILITY, TEXT( "MobileColorGrading" ), &GSystemSettings.bAllowMobileColorGrading, &SimpleBool, TEXT( "Whether to allow color grading on mobile." ) },

	/** The maximum number of bones supported for skinning. */
	{ SST_INT, SSI_DEBUG, TEXT( "MobileBoneCount" ), &GSystemSettings.MobileBoneCount, NULL, TEXT( "The maximum number of bones supported for skinning." ) },
	/** The maximum number of bones influences per vertex supported for skinning. */
	{ SST_INT, SSI_DEBUG, TEXT( "MobileBoneWeightCount" ), &GSystemSettings.MobileBoneWeightCount, NULL, TEXT( "The maximum number of bones influences per vertex supported for skinning." ) },
	/** The size of the scratch buffer for vertices (in kB). */
	{ SST_INT, SSI_UNKNOWN, TEXT( "MobileVertexScratchBufferSize" ), &GSystemSettings.MobileVertexScratchBufferSize, NULL, TEXT( "The size of the scratch buffer for vertices (in kB)." ) },
	/** The size of the scratch buffer for indices (in kB). */
	{ SST_INT, SSI_UNKNOWN, TEXT( "MobileIndexScratchBufferSize" ), &GSystemSettings.MobileIndexScratchBufferSize, NULL, TEXT( "The size of the scratch buffer for indices (in kB)." ) },
	/** UNKNOWN */
	{ SST_INT, SSI_UNKNOWN, TEXT( "MobileShadowTextureResolution" ), &GSystemSettings.MobileShadowTextureResolution, NULL, TEXT( "UNKNOWN." ) },

	/** How much to bias all texture mip levels on mobile (usually 0 or negative). */
	{ SST_FLOAT, SSI_MOBILE_SCALABILITY, TEXT( "MobileLODBias" ), &GSystemSettings.MobileLODBias, NULL, TEXT( "How much to bias all texture mip levels on mobile (usually 0 or negative)." ) },
	/** The default global content scale factor to use on device (largely iOS specific). */
	{ SST_FLOAT, SSI_MOBILE_SCALABILITY, TEXT( "MobileContentScaleFactor" ), &GSystemSettings.MobileContentScaleFactor, NULL, TEXT( "The default global content scale factor to use on device (largely iOS specific)." ) },
	/** Position of the focused center of the tilt shift effect (in percent of the screen height). */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "MobileTiltShiftPosition" ), &GSystemSettings.MobileTiltShiftPosition, NULL, TEXT( "Position of the focused center of the tilt shift effect (in percent of the screen height)." ) },
	/** Width of focused area in the tilt shift effect (in percent of the screen height). */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "MobileTiltShiftFocusWidth" ), &GSystemSettings.MobileTiltShiftFocusWidth, NULL, TEXT( "Width of focused area in the tilt shift effect (in percent of the screen height)." ) },
	/** Width of transition area in the tilt shift effect, where it transitions from full focus to full blur (in percent of the screen height). */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "MobileTiltShiftTransitionWidth" ), &GSystemSettings.MobileTiltShiftTransitionWidth, NULL, TEXT( "Width of transition area in the tilt shift effect, where it transitions from full focus to full blur (in percent of the screen height)." ) },

	/** UNKNOWN */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "MobileLightShaftScale" ), &GSystemSettings.MobileLightShaftRadialBlurPercentScale, NULL, TEXT( "UNKNOWN." ) },
	/** UNKNOWN */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "MobileLightShaftFirstPass" ), &GSystemSettings.MobileLightShaftRadialBlurFirstPassRatio, NULL, TEXT( "UNKNOWN." ) },
	/** UNKNOWN */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "MobileLightShaftSecondPass" ), &GSystemSettings.MobileLightShaftRadialBlurSecondPassRatio, NULL, TEXT( "UNKNOWN." ) },
	/** UNKNOWN */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "MobileMaxShadowRange" ), &GSystemSettings.MobileMaxShadowRange, NULL, TEXT( "UNKNOWN." ) },
	/** LOD bias for mobile landscape rendering on this device (in addition to any per-landscape bias set) */
	{ SST_INT, SSI_UNKNOWN, TEXT( "MobileLandscapeLodBias" ), &GSystemSettings.MobileLandscapeLodBias, NULL, TEXT( "LOD bias for mobile landscape rendering on this device (in addition to any per-landscape bias set)." ) },
	/**  Whether to automatically put cooked startup objects in the StartupPackages shader group */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "MobileUseShaderGroupForStartupObjects" ), &GSystemSettings.bMobileUseShaderGroupForStartupObjects, &SimpleBool, TEXT( "Whether to automatically put cooked startup objects in the StartupPackages shader group" ) },
	/**  Whether to disable generating both fog shader permutations on mobile.  When TRUE, it decreases load times but increases GPU cost for materials/levels with fog enabled */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "MobileMinimizeFogShaders" ), &GSystemSettings.bMobileMinimizeFogShaders, &SimpleBool, TEXT( "Whether to disable generating both fog shader permutations on mobile.  When TRUE, it decreases load times but increases GPU cost for materials/levels with fog enabled" ) },
	/** Mobile FXAA quality level.  0 is off. */
	{ SST_INT, SSI_MOBILE_SCALABILITY, TEXT( "MobileFXAAQuality" ), &GSystemSettings.MobileFXAAQuality, NULL, TEXT( "Mobile FXAA quality level.  0 is off. " ) },
#endif

// allow for APEX only settings
#if WITH_APEX
	/** Resource budget for APEX LOD. Higher values indicate the system can handle more APEX load. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "ApexLODResourceBudget" ), &GSystemSettings.ApexLODResourceBudget, NULL, TEXT( "Resource budget for APEX LOD. Higher values indicate the system can handle more APEX load." ) },
	/** The maximum number of active PhysX actors which represent dynamic groups of chunks (islands). */
	{ SST_INT, SSI_UNKNOWN, TEXT( "ApexDestructionMaxChunkIslandCount" ), &GSystemSettings.ApexDestructionMaxChunkIslandCount, NULL, TEXT( "The maximum number of active PhysX actors which represent dynamic groups of chunks (islands)." ) },
	/** The maximum number of PhysX shapes which represent destructible chunks. */
	{ SST_INT, SSI_UNKNOWN, TEXT( "ApexDestructionMaxShapeCount" ), &GSystemSettings.ApexDestructionMaxShapeCount, NULL, TEXT( "The maximum number of PhysX shapes which represent destructible chunks." ) },
	/** Every destructible asset defines a min and max lifetime, and maximum separation distance for its chunks. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "ApexDestructionMaxChunkSeparationLOD" ), &GSystemSettings.ApexDestructionMaxChunkSeparationLOD, NULL, TEXT( "Every destructible asset defines a min and max lifetime, and maximum separation distance for its chunks." ) },
	/** Lets the user throttle the number of fractures processed per frame (per scene) due to destruction, as this can be quite costly. The default is 0xffffffff (unlimited). */
	{ SST_INT, SSI_UNKNOWN, TEXT( "ApexDestructionMaxFracturesProcessedPerFrame" ), &GSystemSettings.ApexDestructionMaxFracturesProcessedPerFrame, NULL, TEXT( "Lets the user throttle the number of fractures processed per frame (per scene) due to destruction, as this can be quite costly. The default is 0xffffffff (unlimited)." ) },
	/** Average Simulation Frequency is estimated with the last n frames. This is used in Clothing when bAllowAdaptiveTargetFrequency is enabled. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "ApexClothingAvgSimFrequencyWindow" ), &GSystemSettings.ApexClothingAvgSimFrequencyWindow, NULL, TEXT( "Average Simulation Frequency is estimated with the last n frames. This is used in Clothing when bAllowAdaptiveTargetFrequency is enabled." ) },
	/** If set to true, destructible chunks with the lowest benefit would get removed first instead of the oldest. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "ApexDestructionSortByBenefit" ), &GSystemSettings.bApexDestructionSortByBenefit, &SimpleBool, TEXT( "If set to true, destructible chunks with the lowest benefit would get removed first instead of the oldest." ) },
	/** Whether or not to use GPU Rigid Bodies. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "ApexGRBEnable" ), &GSystemSettings.bEnableApexGRB, &SimpleBool, TEXT( "Whether or not to use GPU Rigid Bodies." ) },
	/** Amount (in MB) of GPU memory to allocate for GRB scene data (shapes, actors etc). */
	{ SST_INT, SSI_UNKNOWN, TEXT( "ApexGRBGPUMemSceneSize" ), &GSystemSettings.ApexGRBGpuMemSceneSize, NULL, TEXT( "Amount (in MB) of GPU memory to allocate for GRB scene data (shapes, actors etc)." ) },
	/** Amount (in MB) of GPU memory to allocate for GRB temporary data (broadphase pairs, contacts etc). */
	{ SST_INT, SSI_UNKNOWN, TEXT( "ApexGRBGPUMemTempDataSize" ), &GSystemSettings.ApexGRBGpuMemTempDataSize, NULL, TEXT( "Amount (in MB) of GPU memory to allocate for GRB temporary data (broadphase pairs, contacts etc)." ) },
	/** The size of the cells to divide the world into for GPU collision detection. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "ApexGRBMeshCellSize" ), &GSystemSettings.ApexGRBMeshCellSize, NULL, TEXT( "The size of the cells to divide the world into for GPU collision detection." ) },
	/** Number of non-penetration solver iterations. */
	{ SST_INT, SSI_UNKNOWN, TEXT( "ApexGRBNonPenSolverPosIterCount" ), &GSystemSettings.ApexGRBNonPenSolverPosIterCount, NULL, TEXT( "Number of non-penetration solver iterations." ) },
	/** Number of friction solver position iterations. */
	{ SST_INT, SSI_UNKNOWN, TEXT( "ApexGRBFrictionSolverPosIterCount" ), &GSystemSettings.ApexGRBFrictionSolverPosIterCount, NULL, TEXT( "Number of friction solver position iterations." ) },
	/** Number of friction solver velocity iterations. */
	{ SST_INT, SSI_UNKNOWN, TEXT( "ApexGRBFrictionSolverVelIterCount" ), &GSystemSettings.ApexGRBFrictionSolverVelIterCount, NULL, TEXT( "Number of friction solver velocity iterations." ) },
	/**	Collision skin width, as in PhysX. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "ApexGRBSkinWidth" ), &GSystemSettings.ApexGRBSkinWidth, NULL, TEXT( "Collision skin width, as in PhysX." ) },
	/** Maximum linear acceleration. */
	{ SST_FLOAT, SSI_UNKNOWN, TEXT( "ApexGRBMaxLinearAcceleration" ), &GSystemSettings.ApexGRBMaxLinAcceleration, NULL, TEXT( "Maximum linear acceleration." ) },
	/** If TRUE, allow APEX clothing fetch (skinning etc) to be done on multiple threads. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "bEnableParallelApexClothingFetch" ), &GSystemSettings.bEnableParallelApexClothingFetch, &SimpleBool, TEXT( "If TRUE, allow APEX clothing fetch (skinning etc) to be done on multiple threads." ) },
	/** If TRUE, allow APEX skinning to occur without blocking fetch results. bEnableParallelApexClothingFetch must be enabled for this to work. */
	{ SST_BOOL, SSI_SCALABILITY, TEXT( "bApexClothingAsyncFetchResults" ), &GSystemSettings.bApexClothingAsyncFetchResults, &SimpleBool, TEXT( "If TRUE, allow APEX skinning to occur without blocking fetch results. bEnableParallelApexClothingFetch must be enabled for this to work." ) },
	/** Average Simulation Frequency is estimated with the last n frames. */
	{ SST_INT, SSI_UNKNOWN, TEXT( "ApexClothingAvgSimFrequencyWindow" ), &GSystemSettings.ApexClothingAvgSimFrequencyWindow, NULL, TEXT( "Average Simulation Frequency is estimated with the last n frames." ) },
	/** ClothingActors will cook in a background thread to speed up creation time. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "ApexClothingAllowAsyncCooking" ), &GSystemSettings.bApexClothingAllowAsyncCooking, &SimpleBool, TEXT( "ClothingActors will cook in a background thread to speed up creation time." ) },
	/** Allow APEX SDK to interpolate clothing matrices between the substeps. */
	{ SST_BOOL, SSI_UNKNOWN, TEXT( "ApexClothingAllowApexWorkBetweenSubsteps" ), &GSystemSettings.bApexClothingAllowApexWorkBetweenSubsteps, &SimpleBool, TEXT( "Allow APEX SDK to interpolate clothing matrices between the substeps." ) },
	/** UNKNOWN */
	{ SST_INT, SSI_UNKNOWN, TEXT( "ApexDestructionMaxActorCreatesPerFrame" ), &GSystemSettings.ApexDestructionMaxActorCreatesPerFrame, NULL, TEXT( "UNKNOWN." ) },
#endif
};

/**
 * Helpers for reading and writing to specific ini sections
 */
static const FString GetSectionName( UBOOL bIsEditor, const TCHAR* Override )
{
	FString IniSectionName = TEXT( "SystemSettings" );

	UBOOL bIsMobile = ParseParam( appCmdLine(), TEXT( "simmobile" ) );

	// If we're running the editor with mobile settings, always return the mobile editor settings
	if( bIsEditor && bIsMobile )
	{
		return FString( TEXT( "SystemSettingsMobile" ) );
	}

	// If we're running the editor, always return the editor settings
	if( bIsEditor )
	{
		return FString( TEXT( "SystemSettingsEditor" ) );
	}

	// if we are cooking, look for an override on the commandline
	FString OverrideName;
	if( Parse( appCmdLine(), TEXT( "-SystemSettings=" ), OverrideName ) )
	{
		// look for a commandline override
		return FString::Printf( TEXT( "%s%s" ), *IniSectionName, *OverrideName );
	}
	else if( bIsMobile )
	{
		// If there's no device specific override, but we are running mobile, return the default mobile settings
		return FString( TEXT( "SystemSettingsMobile" ) );
	}

#if MOBILE
	return appGetMobileSystemSettingsSectionName();
#else
	INT BucketLevel = 5;

	if( Override != NULL )
	{
		// look for a programmatic override
		IniSectionName = FString::Printf( TEXT( "%s%s" ), *IniSectionName, Override );
	}
#if 0
	// Handle any scalability settings
	else if( ( GOpenAutomate == NULL ) && GConfig->GetInt( TEXT( "AppCompat" ), TEXT( "CompatLevelComposite" ), BucketLevel, GEngineIni ) )
	{
		// set selected bucket only when *NOT* profiling
		IniSectionName = FString::Printf( TEXT( "%sBucket%d" ), *IniSectionName, BucketLevel );
	}
#endif

	// return the proper section for using editor or not
	return IniSectionName;
#endif
}

/**
 * Finds the setting by name
*/
FSystemSetting* FSystemSettings::FindSystemSetting( FString& SettingName, ESystemSettingType SettingType )
{
	for( INT SettingIndex = 0; SettingIndex < ARRAY_COUNT( SystemSettings ); SettingIndex++ )
	{
		FSystemSetting* Setting = SystemSettings + SettingIndex;
		if( SettingType != SST_ANY && SettingType != Setting->SettingType )
		{
			continue;
		}

		if( !appStrnicmp( Setting->SettingName, *SettingName, SettingName.Len() ) )
		{
			return Setting;
		}
	}

	warnf( NAME_Warning, TEXT( "The System Setting %s could not be found." ), *SettingName );
	return NULL;
}

/**
 * ctor
 */
void FSystemSettings::LoadFromIni( const FString IniSection, const TCHAR* IniFilename, UBOOL bAllowMissingValues )
{
	UBOOL bCheckFoundValuesAtEnd = FALSE;
	
	if( !bAllowMissingValues )
	{
		// we need to check the FoundValues after at the end of this function
		bCheckFoundValuesAtEnd = TRUE;

		// Clear out the found status
		for( INT SettingIndex = 0; SettingIndex < ARRAY_COUNT( SystemSettings ); SettingIndex++ )
		{
			SystemSettings[SettingIndex].bFound = FALSE;
		}
	}

	// first, look for a parent section to base off of
	FString BasedOnSection;
	if( GConfig->GetString( *IniSection, TEXT( "BasedOn" ), BasedOnSection, IniFilename ) )
	{
		debugf( TEXT( "SystemSettings based on: %s" ), *BasedOnSection );
		// recurse with the BasedOn section if it existed, always allowing for missing values
		LoadFromIni( BasedOnSection, IniFilename, TRUE );
	}

	for( INT SettingIndex = 0; SettingIndex < ARRAY_COUNT( SystemSettings ); SettingIndex++ )
	{
		FSystemSetting* Setting = SystemSettings + SettingIndex;
		switch( Setting->SettingType )
		{
		case SST_BOOL:
			Setting->bFound |= GConfig->GetBool( *IniSection, Setting->SettingName, *( UBOOL* )Setting->SettingAddress, IniFilename );
			break;

		case SST_INT:
			Setting->bFound |= GConfig->GetInt( *IniSection, Setting->SettingName, *( INT* )Setting->SettingAddress, IniFilename );
			break;

		case SST_FLOAT:
			Setting->bFound |= GConfig->GetFloat( *IniSection, Setting->SettingName, *( FLOAT* )Setting->SettingAddress, IniFilename );
			break;
		}
	}

	// Read the texture group LOD settings.
	TextureLODSettings.Initialize( IniFilename, *IniSection );

	// if this is the top of the recursion stack, and we care about missing values, report them
	if( bCheckFoundValuesAtEnd )
	{
		// Clear out the found status
		for( INT SettingIndex = 0; SettingIndex < ARRAY_COUNT( SystemSettings ); SettingIndex++ )
		{
			checkf( SystemSettings[SettingIndex].bFound,
				TEXT( "Couldn't find system setting %s in Ini section %s in Ini file %s!" ), SystemSettings[SettingIndex].SettingName, *IniSection, IniFilename );
		}
	}
}

/**
* Returns a string for the specified texture group LOD settings to the specified ini.
*
* @param	TextureGroupID		Index/enum of the group
* @param	GroupName			String representation of the texture group
*/
FString FSystemSettings::GetLODGroupString( TextureGroup TextureGroupID, const TCHAR* GroupName )
{
	const FExposedTextureLODSettings::FTextureLODGroup& Group = TextureLODSettings.GetTextureLODGroup(TextureGroupID);

	const INT MinLODSize = 1 << Group.MinLODMipCount;
	const INT MaxLODSize = 1 << Group.MaxLODMipCount;

	FName MinMagFilter = NAME_Aniso;
	FName MipFilter = NAME_Linear;
	switch(Group.Filter)
	{
		case SF_Point:
			MinMagFilter = NAME_Point;
			MipFilter = NAME_Point;
			break;
		case SF_Bilinear:
			MinMagFilter = NAME_Linear;
			MipFilter = NAME_Point;
			break;
		case SF_Trilinear:
			MinMagFilter = NAME_Linear;
			MipFilter = NAME_Linear;
			break;
		case SF_AnisotropicPoint:
			MinMagFilter = NAME_Aniso;
			MipFilter = NAME_Point;
			break;
		case SF_AnisotropicLinear:
			MinMagFilter = NAME_Aniso;
			MipFilter = NAME_Linear;
			break;
	}

	FString NumStreamedMipsText;
	if ( Group.NumStreamedMips >= 0 )
	{
		NumStreamedMipsText = FString::Printf( TEXT(",NumStreamedMips=%i"), Group.NumStreamedMips );
	}

	return FString::Printf( TEXT( "(MinLODSize=%i,MaxLODSize=%i,LODBias=%i,MinMagFilter=%s,MipFilter=%s%s,MipGenSettings=%s)" ),
		MinLODSize, MaxLODSize, Group.LODBias, *MinMagFilter.GetNameString(), *MipFilter.GetNameString(), *NumStreamedMipsText, UTexture::GetMipGenSettingsString( Group.MipGenSettings ) );
}

/**
 * Writes the specified texture group LOD settings to the specified ini.
 *
 * @param	TextureGroupID		Index/enum of the group to parse
 * @param	GroupName			String representation of the texture group, to be used as the ini key.
 * @param	IniSection			The .ini section to save to.
 */
void FSystemSettings::WriteTextureLODGroupToIni( TextureGroup TextureGroupID, const TCHAR* GroupName, const TCHAR* IniSection )
{
	const FString Entry = GetLODGroupString( TextureGroupID, GroupName );
	GConfig->SetString( IniSection, GroupName, *Entry, GSystemSettingsIni );
}

/**
 * Saves the current settings to the compat ini
 */
void FSystemSettings::SaveToIni( const FString IniSection )
{
	for( INT SettingIndex = 0; SettingIndex < ARRAY_COUNT( SystemSettings ); SettingIndex++ )
	{
		FSystemSetting* Setting = SystemSettings + SettingIndex;
		switch( Setting->SettingType )
		{
		case SST_BOOL:
			GConfig->SetBool( *IniSection, Setting->SettingName, *( UBOOL* )Setting->SettingAddress, GSystemSettingsIni );
			break;

		case SST_INT:
			GConfig->SetInt( *IniSection, Setting->SettingName, *( INT* )Setting->SettingAddress, GSystemSettingsIni );
			break;

		case SST_FLOAT:
			GConfig->SetFloat( *IniSection, Setting->SettingName, *( FLOAT* )Setting->SettingAddress, GSystemSettingsIni );
			break;
		}
	}

	// Save the texture group LOD settings.
#define WRITETEXTURELODGROUPTOINI( Group ) WriteTextureLODGroupToIni( Group, TEXT( #Group ), *IniSection );
	FOREACH_ENUM_TEXTUREGROUP( WRITETEXTURELODGROUPTOINI )
#undef WRITETEXTURELODGROUPTOINI

	GConfig->Flush( FALSE, GSystemSettingsIni );
}

/**
 * Dump helpers
 */
void FSystemSettings::DumpTextureLODGroup( FOutputDevice& Ar, TextureGroup TextureGroupID, const TCHAR* GroupName )
{
	const FString Entry = GetLODGroupString( TextureGroupID, GroupName );
	Ar.Logf( TEXT( "    %s: %s" ), GroupName, *Entry );
}

/**
 * Dump texture scalability
 */
void FSystemSettings::DumpTextures( FOutputDevice& Ar )
{
	// Dump the TextureLODSettings
#define DUMPTEXTURELODGROUP( Group ) DumpTextureLODGroup( Ar, Group, TEXT( #Group ) );
	FOREACH_ENUM_TEXTUREGROUP(DUMPTEXTURELODGROUP)
#undef DUMPTEXTURELODGROUP
}

/**
 * Dump scalability
 */
void FSystemSettings::Dump( FOutputDevice& Ar, ESystemSettingIntent SettingIntent )
{
	for( INT SettingIndex = 0; SettingIndex < ARRAY_COUNT( SystemSettings ); SettingIndex++ )
	{
		FSystemSetting* Setting = SystemSettings + SettingIndex;
		if( Setting->SettingIntent == SettingIntent )
		{
			switch( Setting->SettingType )
			{
			case SST_BOOL:
				Ar.Logf( TEXT( "    %s = %s (%s)" ), Setting->SettingName, *( UBOOL* )Setting->SettingAddress ? TEXT( "TRUE" ) : TEXT( "FALSE" ), Setting->SettingHelp );
				break;

			case SST_INT:
				Ar.Logf( TEXT( "    %s = %d (%s)" ), Setting->SettingName, *( INT* )Setting->SettingAddress, Setting->SettingHelp );
				break;

			case SST_FLOAT:
				Ar.Logf( TEXT( "    %s = %g (%s)" ), Setting->SettingName, *( FLOAT* )Setting->SettingAddress, Setting->SettingHelp );
				break;
			}
		}
	}
}

/**
 * Constructor, initializing all member variables.
 */
FSystemSettings::FSystemSettings( void ) :
	bInit( FALSE ),
	bIsEditor( FALSE )
{
	NumberOfSystemSettings = ARRAY_COUNT( SystemSettings );
}

/**
 * Initializes system settings and included texture LOD settings.
 *
 * @param bSetupForEditor	Whether to initialize settings for Editor
 */
void FSystemSettings::Initialize( UBOOL bSetupForEditor )
{
	// Since System Settings is called into before GIsEditor is set, we must cache this value.
	bIsEditor = bSetupForEditor;

	// Load the settings from the ini file
	LoadFromIni( GetSectionName( bIsEditor, NULL ), GSystemSettingsIni, FALSE );

	// fixup resolution scale on Android
#if ANDROID
	extern float GAndroidResolutionScale;
	if( GAndroidResolutionScale < 0 )
	{
		GAndroidResolutionScale = ScreenPercentage / 100.0;
	}
#endif
	
	ApplyOverrides();

	bInit = TRUE;

	// intialize a critical texture streaming value used by texture loading, etc
	verify( GConfig->GetInt( TEXT( "TextureStreaming" ), TEXT( "MinTextureResidentMipCount" ), GMinTextureResidentMipCount, GEngineIni ) );
}

/**
 * Apply any special required overrides
 */
void FSystemSettings::ApplyOverrides( void )
{
	if( ParseParam( appCmdLine(), TEXT( "MSAA" ) ) )
	{
		MaxMultiSamples = GOptimalMSAALevel;
	}

	// look for commandline overrides of specific system settings, the format is:
	//    -ss:name1=val1,name2=val2
	FString Settings;
	if (Parse(appCmdLine(), TEXT("-SS:"), Settings, FALSE))
	{
		debugf(TEXT("Overriding system settings from the commandline: %s"), *Settings);

		// break apart on the commas
		TArray<FString> SettingPairs;
		Settings.ParseIntoArray(&SettingPairs, TEXT(","), TRUE);
		for (INT Index = 0; Index < SettingPairs.Num(); Index++)
		{
			// set each one, by splitting on the =
			FString Key, Value;
			if (SettingPairs(Index).Split(TEXT("="), &Key, &Value))
			{
				Exec(*FString::Printf(TEXT("scale set %s %s"), *Key, *Value), *GLog);
			}
		}
	}

	// look for commandline overrides for per lod group LODBias settings, format is:
	//		-lodbias:group1=val1,group2=val2
	// for instance:
	//		-lodbias:world=1,CharacterNormalMap=5
	if (Parse(appCmdLine(), TEXT("-LODBIAS:"), Settings, FALSE))
	{
		debugf(TEXT("Overriding LOD biases from the commandline:"), *Settings);

		// break apart on the commas
		TArray<FString> SettingPairs;
		Settings.ParseIntoArray(&SettingPairs, TEXT(","), TRUE);
		for (INT Index = 0; Index < SettingPairs.Num(); Index++)
		{
			// set each one, by splitting on the =
			FString Key, Value;
			if (SettingPairs(Index).Split(TEXT("="), &Key, &Value))
			{
				FString FullGroupName = FString("TEXTUREGROUP_") + Key;
				INT Bias = appAtoi(*Value);
				debugf(TEXT("   Setting group %s to %d"), *FullGroupName, Bias);
#define CHECKFORLODOVERRIDE(Group) if (FullGroupName == TEXT(#Group)) TextureLODSettings.GetTextureLODGroup(Group).LODBias = Bias; else
				FOREACH_ENUM_TEXTUREGROUP(CHECKFORLODOVERRIDE)
					// final block for final else
				{ }
#undef CHECKFORLODOVERRIDE
			}
		}
	}

	// look for commandline overrides for per lod group LODBias settings, format is:
	//		-maxlod:group1=val1,group2=val2
	// for instance:
	//		-maxlod:world=1,CharacterNormalMap=5
	if (Parse(appCmdLine(), TEXT("-MAXLOD:"), Settings, FALSE))
	{
		debugf(TEXT("Overriding Max LOD sizes from the commandline:"), *Settings);

		// break apart on the commas
		TArray<FString> SettingPairs;
		Settings.ParseIntoArray(&SettingPairs, TEXT(","), TRUE);
		for (INT Index = 0; Index < SettingPairs.Num(); Index++)
		{
			// set each one, by splitting on the =
			FString Key, Value;
			if (SettingPairs(Index).Split(TEXT("="), &Key, &Value))
			{
				FString FullGroupName = FString("TEXTUREGROUP_") + Key;
				INT MaxLODSize = appAtoi(*Value);
				debugf(TEXT("   Setting group %s to %d"), *FullGroupName, MaxLODSize);
#define CHECKFORLODOVERRIDE(Group) \
				if (FullGroupName == TEXT(#Group)) \
				{ \
					FTextureLODSettings::FTextureLODGroup& LODGroup = TextureLODSettings.GetTextureLODGroup(Group); \
					LODGroup.MaxLODMipCount = appCeilLogTwo(MaxLODSize); \
					LODGroup.MinLODMipCount = Min(LODGroup.MinLODMipCount, LODGroup.MaxLODMipCount); \
				} \
			else
				FOREACH_ENUM_TEXTUREGROUP(CHECKFORLODOVERRIDE)
					// final block for final else
				{ }
#undef CHECKFORLODOVERRIDE
			}
		}
	}

#if CONSOLE
	// Overwrite resolution from Ini with resolution from the console RHI
	ResX = GScreenWidth;
	ResY = GScreenHeight;
#endif

	// if system settings = max quality mode
	// No point in changing scene color format if depth is not stored in the alpha
	if( SystemSettingName == TEXT( "ScreenShot" ) && !GSupportsDepthTextures )
	{
		// Use a 32 bit fp scene color and depth, which reduces distant shadow artifacts due to storing scene depth in 16 bit fp significantly
		GSceneRenderTargets.SetSceneColorBufferFormat( PF_A32B32G32R32F );
	}
}

/**
 * Exec handler implementation.
 *
 * @param Cmd	Command to parse
 * @param Ar	Output device to log to
 *
 * @return TRUE if command was handled, FALSE otherwise
 */
UBOOL FSystemSettings::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FSystemSettings OldSystemSettings = *this;

	// Keep track whether the command was handled or not.
	UBOOL bHandledCommand = FALSE;

	if( ParseCommand(&Cmd,TEXT("SCALE")) )
	{
		if( ParseCommand(&Cmd,TEXT("DUMP")) )
		{
			Ar.Logf( TEXT( "Current scalability system settings:" ) );
			Dump( Ar, SSI_SCALABILITY );
			return TRUE;
		}
		else if( ParseCommand( &Cmd, TEXT( "DUMPMOBILE" ) ) )
		{
			Ar.Logf( TEXT( "Current mobile scalability system settings:" ) );
			Dump( Ar, SSI_MOBILE_SCALABILITY );
			return TRUE;
		}
		else if( ParseCommand( &Cmd, TEXT( "DUMPPREFS" ) ) )
		{
			Ar.Logf( TEXT( "Current preference system settings:" ) );
			Dump( Ar, SSI_PREFERENCE );
			return TRUE;
		}
		else if( ParseCommand( &Cmd, TEXT( "DUMPDEBUG" ) ) )
		{
			Ar.Logf( TEXT( "Current debug system settings:" ) );
			Dump( Ar, SSI_DEBUG );
			return TRUE;
		}
		else if( ParseCommand( &Cmd, TEXT( "DUMPUNKNOWN" ) ) )
			{
			Ar.Logf( TEXT( "Current unknown system settings:" ) );
			Dump( Ar, SSI_UNKNOWN );
			return TRUE;
		}
		else if( ParseCommand( &Cmd, TEXT( "DUMPTEXTURES" ) ) )
		{
			Ar.Logf( TEXT( "Current texture settings:" ) );
			DumpTextures( Ar );
			return TRUE;
		}
		else if( ParseCommand( &Cmd, TEXT( "BUCKET" ) ) )
		{
			FString Token = ParseToken( Cmd, FALSE );
			bHandledCommand = LoadFromIni( *Token );
			if( !bHandledCommand )
			{
				Ar.Logf( TEXT( "Could not find section named '%s'" ), *Token );
			}
		}
		else if( ParseCommand(&Cmd,TEXT("LOWEND")) )
		{
			bHandledCommand = LoadFromIni( TEXT( "Bucket1" ) );
			if( !bHandledCommand )
			{
				Ar.Logf( TEXT( "Could not find section named 'Bucket1'" ) );
			}
		}
		else if( ParseCommand(&Cmd,TEXT("HIGHEND")) )
		{
			// Apply bucket5
			bHandledCommand = LoadFromIni( TEXT( "Bucket5" ) );
			if( !bHandledCommand )
			{
				Ar.Logf( TEXT( "Could not find section named 'Bucket5'" ) );
			}
		}
		else if( ParseCommand( &Cmd, TEXT( "SCREENSHOT" ) ) )
		{
			// Apply bucket5
			bHandledCommand = LoadFromIni( TEXT( "Screenshot" ) );
			if( !bHandledCommand )
			{
				Ar.Logf( TEXT( "Could not find section named 'Screenshot'" ) );
			}
		}
		else if( ParseCommand(&Cmd,TEXT("RESET")) )
		{
			// Reset values to defaults from ini.
			bHandledCommand = LoadFromIni( NULL );

#if CONSOLE
			// Overwrite resolution from Ini with resolution from the console RHI
			ResX = GScreenWidth;
			ResY = GScreenHeight;
#endif
		}
		else if( ParseCommand(&Cmd,TEXT("SET")) )
		{
			FString Token = ParseToken( Cmd, FALSE );
			FSystemSetting* Setting = FindSystemSetting( Token, SST_ANY );
			if( Setting == NULL )
			{
				Ar.Logf( TEXT( "Could not find setting named '%s'" ), *Token );
				return TRUE;
			}

			UBOOL bNewBoolValue = FALSE;
			INT NewIntValue = 0;
			FLOAT NewFloatValue = 0.0f;

			switch( Setting->SettingType )
			{
			case SST_BOOL:
				bNewBoolValue = ParseCommand( &Cmd, TEXT( "TRUE" ) );
				*( UBOOL* )Setting->SettingAddress = bNewBoolValue;
				Ar.Logf( TEXT( "Bool %s set to %u" ), Setting->SettingName, bNewBoolValue );
				bHandledCommand	= TRUE;
				break;

			case SST_INT:
				NewIntValue = appAtoi( Cmd );
				*( INT* )Setting->SettingAddress = NewIntValue;
				Ar.Logf( TEXT("Int %s set to %u"), Setting->SettingName, NewIntValue);
				bHandledCommand = TRUE;
				break;

			case SST_FLOAT:
				NewFloatValue = appAtof( Cmd );
				*( FLOAT* )Setting->SettingAddress = NewFloatValue;
				Ar.Logf( TEXT( "Float %s set to %g"), Setting->SettingName, NewFloatValue);
				bHandledCommand	= TRUE;
				break;
			}
		}
		else if( ParseCommand(&Cmd,TEXT("TOGGLE")) )
		{
			FString Token = ParseToken( Cmd, FALSE );
			FSystemSetting* Setting = FindSystemSetting( Token, SST_BOOL );
			if( Setting == NULL )
			{
				Ar.Logf( TEXT( "Could not find BOOL setting named '%s'" ), *Token );
				return TRUE;
			}

			*( UBOOL* )Setting->SettingAddress = !*( UBOOL* )Setting->SettingAddress;
			Ar.Logf( TEXT( "Bool %s toggled, new value %u"), Setting->SettingName, *( UBOOL* )Setting->SettingAddress );
			bHandledCommand	= TRUE;
		}
		else if( ParseCommand( &Cmd, TEXT( "SHRINK" ) ) )
		{
			FLOAT Step = ( 16.0f / 1280.0f ) * 100.0f;
			ScreenPercentage = Clamp( ScreenPercentage - Step, Step, 100.0f );
			Ar.Logf( TEXT( "ScreenPercentage shrunk to %f" ), ScreenPercentage );
			bHandledCommand	= TRUE;
		}
		else if( ParseCommand( &Cmd, TEXT( "EXPAND" ) ) )
		{
			FLOAT Step = ( 160.f / 1280.0f ) * 100.0f;
			ScreenPercentage = Clamp( ScreenPercentage + Step, Step, 100.0f );
			Ar.Logf( TEXT( "ScreenPercentage expanded to %f" ), ScreenPercentage );
			bHandledCommand	= TRUE;
		}
		else if ( ParseCommand(&Cmd, TEXT("ADJUST")) )
		{
			static UBOOL Adjusting = FALSE;
			static FString SaveLS;
			static FString SaveRS;
			Adjusting = ! Adjusting;
			UPlayerInput *Input = GEngine->GamePlayers(0)->Actor->PlayerInput;
			if( Adjusting )
			{
				SaveLS= Input->GetBind( TEXT("XboxTypeS_LeftShoulder") );
				SaveRS = Input->GetBind( TEXT("XboxTypeS_RightShoulder") );
				Input->ScriptConsoleExec( TEXT("setbind XboxTypeS_LeftShoulder scale decr"), Ar, NULL );
				Input->ScriptConsoleExec( TEXT("setbind XboxTypeS_RightShoulder scale incr"), Ar, NULL );
			}
			else 
			{
				FString SetBind;
				SetBind = FString::Printf( TEXT("setbind XboxTypeS_LeftShoulder %s"), *SaveLS );
				Input->ScriptConsoleExec( *SetBind, Ar, NULL );
				SetBind = FString::Printf( TEXT("setbind XboxTypeS_RightShoulder %s"), *SaveRS );
				Input->ScriptConsoleExec( *SetBind, Ar, NULL );
			}
			bHandledCommand = TRUE;
		}

		if (!bHandledCommand)
		{
			Ar.Logf(TEXT("Unrecognized system setting"));
			Ar.Logf( TEXT( "  Scale <Command> [parameter] [parameter]" ) );
			Ar.Logf( TEXT( "  Scale Dump - displays all scalability settings." ) );
			Ar.Logf( TEXT( "  Scale DumpMobile - displays all mobile scalability settings." ) );
			Ar.Logf( TEXT( "  Scale DumpPrefs - displays all preferences." ) );
			Ar.Logf( TEXT( "  Scale DumpDebug - displays all debug settings." ) );
			Ar.Logf( TEXT( "  Scale DumpUnknown - displays all uncategorised settings." ) );
			Ar.Logf( TEXT( "  Scale DumpTextures - displays the texture settings." ) );
			Ar.Logf( TEXT( "  Scale Bucket BucketName - sets the current settings to the contents of the SystemSettings<BucketName> section." ) );
			Ar.Logf( TEXT( "  Scale LowEnd - sets the current settings to Bucket1." ) );
			Ar.Logf( TEXT( "  Scale HighEnd - sets the current settings to Bucket5." ) );
			Ar.Logf( TEXT( "  Scale Screenshot - sets the current settings to the contents of the SystemSettingsScreenShot section." ) );
			Ar.Logf( TEXT( "  Scale Reset - loads in the default settings." ) );
			Ar.Logf( TEXT( "  Scale Set Key Value - sets the bool, int or float value of Key to Value." ) );
			Ar.Logf( TEXT( "  Scale Toggle Key - toggles the state of the bool named Key." ) );
			Ar.Logf( TEXT( "  Scale Shrink - decrements ScreenPercentage." ) );
			Ar.Logf( TEXT( "  Scale Expand - increments ScreenPercentage." ) );
		}
		else
		{
			// Write the new settings to the INI.
			SaveToIni();

			// Apply the settings
			ApplySettings( OldSystemSettings );
		}
	}

	return bHandledCommand;
}


/**
 * Scale X,Y offset/size of screen coordinates if the screen percentage is not at 100%
 *
 * @param X - in/out X screen offset
 * @param Y - in/out Y screen offset
 * @param SizeX - in/out X screen size
 * @param SizeY - in/out Y screen size
 */
void FSystemSettings::ScaleScreenCoords( INT& X, INT& Y, UINT& SizeX, UINT& SizeY )
{
	// Take screen percentage option into account if percentage != 100.
	if( GSystemSettings.ScreenPercentage != 100.0f && !bIsEditor )
	{
		// Clamp screen percentage to reasonable range.
		FLOAT ScaleFactor = Clamp( GSystemSettings.ScreenPercentage / 100.f, 0.0f, 1.0f );

		FLOAT ScaleFactorX = ScaleFactor;
		FLOAT ScaleFactorY = ScaleFactor;
#if XBOX // When bUpscaleScreenPercentage is false, we use the hardware scaler, which has alignment constraints
		if( !bUpscaleScreenPercentage )
		{
			ScaleFactorX = ( INT( ScaleFactor * GScreenWidth  + 0.5f ) & ~0xf ) / FLOAT( GScreenWidth  );
			ScaleFactorY = ( INT( ScaleFactor * GScreenHeight + 0.5f ) & ~0xf ) / FLOAT( GScreenHeight );
		}
#endif
		INT	OrigX = X;
		INT OrigY = Y;
		UINT OrigSizeX = SizeX;
		UINT OrigSizeY = SizeY;

		// Scale though make sure we're at least covering 1 pixel.
		SizeX = Max(1,appTrunc(ScaleFactorX * OrigSizeX));
		SizeY = Max(1,appTrunc(ScaleFactorY * OrigSizeY));

		// Center scaled view.
		X = OrigX + (OrigSizeX - SizeX) / 2;
		Y = OrigY + (OrigSizeY - SizeY) / 2;
	}
}

/**
 * Reverses the scale and offset done by ScaleScreenCoords() 
 * if the screen percentage is not 100% and upscaling is allowed.
 *
 * @param OriginalX - out X screen offset
 * @param OriginalY - out Y screen offset
 * @param OriginalSizeX - out X screen size
 * @param OriginalSizeY - out Y screen size
 * @param InX - in X screen offset
 * @param InY - in Y screen offset
 * @param InSizeX - in X screen size
 * @param InSizeY - in Y screen size
 */
void FSystemSettings::UnScaleScreenCoords( 
	INT &OriginalX, INT &OriginalY, 
	UINT &OriginalSizeX, UINT &OriginalSizeY, 
	FLOAT InX, FLOAT InY, 
	FLOAT InSizeX, FLOAT InSizeY)
{
	if (NeedsUpscale())
	{
		FLOAT ScaleFactor = Clamp( GSystemSettings.ScreenPercentage / 100.f, 0.0f, 1.0f );

		//undo scaling
		OriginalSizeX = appTrunc(InSizeX / ScaleFactor);
		OriginalSizeY = appTrunc(InSizeY / ScaleFactor);

		//undo centering
		OriginalX = appTrunc(InX - (OriginalSizeX - InSizeX) / 2.0f);
		OriginalY = appTrunc(InY - (OriginalSizeY - InSizeY) / 2.0f);
	}
	else
	{
		OriginalSizeX = appTrunc(InSizeX);
		OriginalSizeY = appTrunc(InSizeY);

		OriginalX = appTrunc(InX);
		OriginalY = appTrunc(InY);
	}
}

/** Indicates whether upscaling is needed */
UBOOL FSystemSettings::NeedsUpscale( void ) const
{
	return bUpscaleScreenPercentage && !bIsEditor && ( ScreenPercentage < 100.0f );
}

/**
 * Reads a single entry and parses it into the group array.
 *
 * @param	TextureGroupID		Index/enum of group to parse
 * @param	MinLODSize			Minimum size, in pixels, below which the code won't bias.
 * @param	MaxLODSize			Maximum size, in pixels, above which the code won't bias.
 * @param	LODBias				Group LOD bias.
 */
void FSystemSettings::SetTextureLODGroup(TextureGroup TextureGroupID, INT MinLODSize, INT MaxLODSize, INT LODBias, TextureMipGenSettings MipGenSettings)
{
	TextureLODSettings.GetTextureLODGroup(TextureGroupID).MinLODMipCount	= appCeilLogTwo( MinLODSize );
	TextureLODSettings.GetTextureLODGroup(TextureGroupID).MaxLODMipCount	= appCeilLogTwo( MaxLODSize );
	TextureLODSettings.GetTextureLODGroup(TextureGroupID).LODBias			= LODBias;
	TextureLODSettings.GetTextureLODGroup(TextureGroupID).MipGenSettings	= MipGenSettings;
}

/**
* Recreates texture resources and drops mips.
*
* @return		TRUE if the settings were applied, FALSE if they couldn't be applied immediately.
*/
UBOOL FSystemSettings::UpdateTextureStreaming( void )
{
	if ( GStreamingManager )
	{
		// Make sure textures can be streamed out so that we can unload current mips.
		const UBOOL bOldOnlyStreamInTextures = bOnlyStreamInTextures;
		bOnlyStreamInTextures = FALSE;

		for( TObjectIterator<UTexture2D> It; It; ++It )
		{
			UTexture* Texture = *It;

			// Update cached LOD bias.
			Texture->CachedCombinedLODBias = TextureLODSettings.CalculateLODBias( Texture );
		}

		// Make sure we iterate over all textures by setting it to high value.
		GStreamingManager->SetNumIterationsForNextFrame( 100 );
		// Update resource streaming with updated texture LOD bias/ max texture mip count.
		GStreamingManager->UpdateResourceStreaming( 0 );
		// Block till requests are finished.
		GStreamingManager->BlockTillAllRequestsFinished();

		// Restore streaming out of textures.
		bOnlyStreamInTextures = bOldOnlyStreamInTextures;
	}

	return TRUE;
}

/**
 * Makes System Settings take effect on the renderthread
 */
void FSystemSettings::SceneRenderTargetsUpdateRHI( const FSystemSettings& OldSettings, const FSystemSettings& NewSettings )
{
	// Shouldn't be reallocating render targets on console
#if !CONSOLE

	// Should we create or release certain rendertargets?
	const UBOOL bUpdateRenderTargets =
		(OldSettings.bAllowMotionBlur						!= NewSettings.bAllowMotionBlur) ||
		(OldSettings.bAllowAmbientOcclusion					!= NewSettings.bAllowAmbientOcclusion) ||
		(OldSettings.bAllowDynamicShadows					!= NewSettings.bAllowDynamicShadows) ||
		(OldSettings.bAllowHardwareShadowFiltering			!= NewSettings.bAllowHardwareShadowFiltering) ||
		(OldSettings.bAllowFogVolumes						!= NewSettings.bAllowFogVolumes) ||
		(OldSettings.bAllowSubsurfaceScattering				!= NewSettings.bAllowSubsurfaceScattering) ||
		(OldSettings.bHighPrecisionGBuffers					!= NewSettings.bHighPrecisionGBuffers) ||
		(OldSettings.bAllowTemporalAA						!= NewSettings.bAllowTemporalAA) ||
		(OldSettings.MaxMultiSamples						!= NewSettings.MaxMultiSamples) ||
		(OldSettings.bAllowPostprocessMLAA					!= NewSettings.bAllowPostprocessMLAA) ||
		(OldSettings.MinShadowResolution					!= NewSettings.MinShadowResolution) ||
		(OldSettings.MaxShadowResolution					!= NewSettings.MaxShadowResolution) ||
		(OldSettings.MaxWholeSceneDominantShadowResolution	!= NewSettings.MaxWholeSceneDominantShadowResolution);

	// Activate certain system settings on the renderthread
	if(bUpdateRenderTargets)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			SceneRenderTargetsUpdateRHICommand,
		{
			UpdateSceneRenderTargetsRHI();
		});
	}
#endif
}

/**
 * Cause a call to UpdateRHI on GSceneRenderTargets
 */
void FSystemSettings::UpdateSceneRenderTargetsRHI()
{
	GSceneRenderTargets.UpdateRHI();
}

/** 
 * Sets the resolution and writes the values to Ini if changed but does not apply the changes (e.g. resize the viewport).
 */
void FSystemSettings::SetResolution(INT InSizeX, INT InSizeY, UBOOL InFullscreen)
{
	if (!bIsEditor)
	{
		const UBOOL bResolutionChanged = 
			ResX != InSizeX ||
			ResY != InSizeY ||
			bFullscreen != InFullscreen;

		if (bResolutionChanged)
		{
			ResX = InSizeX;
			ResY = InSizeY;
			bFullscreen = InFullscreen;
			SaveToIni();
		}
	}
}

/**
 * Overridden function that selects the proper ini section to read from or write to
 */
UBOOL FSystemSettings::LoadFromIni( const TCHAR* Override )
{
	FString SectionName = GetSectionName( bIsEditor, Override );

	// Ensure the section exists
	if( GConfig->GetSectionPrivate( *SectionName, FALSE, FALSE, GSystemSettingsIni ) == NULL )
	{
		return FALSE;
	}

	LoadFromIni( SectionName, GSystemSettingsIni, FALSE );

#if CONSOLE
	// Always default to using VSYNC on consoles.
	bUseVSync = TRUE;
#endif

	// Disable VSYNC if -novsync is on the command line.
	bUseVSync = bUseVSync && !ParseParam(appCmdLine(), TEXT("novsync"));

	// Enable VSYNC if -vsync is on the command line.
	bUseVSync = bUseVSync || ParseParam(appCmdLine(), TEXT("vsync"));

	return TRUE;
}

void FSystemSettings::SaveToIni( void )
{
	// don't write changes in the editor
	if (bIsEditor)
	{
		debugf(TEXT("Can't save system settings to ini in an editor mode"));
		return;
	}

	FSystemSettings::SaveToIni( GetSectionName( bIsEditor, TEXT( "" ) ) );
}

/**
 * Apply any settings that have changed
 */
void FSystemSettings::ApplySettings( FSystemSettings& OldSystemSettings )
{
	// Some of these settings are shared between threads, so we must flush the rendering thread before changing anything.
	FlushRenderingCommands();

	// We don't support switching lightmap type at run-time.
	if( bAllowDirectionalLightMaps != OldSystemSettings.bAllowDirectionalLightMaps )
	{
		debugf( TEXT( "Can't enable/disable directional lightmaps at run-time." ) );
		bAllowDirectionalLightMaps = OldSystemSettings.bAllowDirectionalLightMaps;
	}

	if( OldSystemSettings.bForceCPUAccessToGPUSkinVerts != bForceCPUAccessToGPUSkinVerts || OldSystemSettings.bDisableSkeletalInstanceWeights != bDisableSkeletalInstanceWeights )
	{
		debugf( TEXT( "Can't change bForceCPUAccessToGPUSkinVerts or bDisableSkeletalInstanceWeights at run-time." ) );
		bForceCPUAccessToGPUSkinVerts = OldSystemSettings.bForceCPUAccessToGPUSkinVerts;
		bDisableSkeletalInstanceWeights = OldSystemSettings.bDisableSkeletalInstanceWeights;
	}

	// Reattach components if world-detail settings have changed.
	if( OldSystemSettings.DetailMode != DetailMode || OldSystemSettings.bAllowHighQualityMaterials != bAllowHighQualityMaterials )
	{
		// decals should always reattach after all other primitives have been attached
		TArray<UClass*> ExcludeComponents;
		ExcludeComponents.AddItem( UDecalComponent::StaticClass() );
		ExcludeComponents.AddItem( UAudioComponent::StaticClass() );

		FGlobalComponentReattachContext PropagateDetailModeChanges( ExcludeComponents );
	}

	// Reattach decal components if needed
	if( OldSystemSettings.DetailMode != DetailMode )
	{
		TComponentReattachContext<UDecalComponent> PropagateDecalComponentChanges;
	}

	if( OldSystemSettings.bAllowRadialBlur != bAllowRadialBlur )
	{
		TComponentReattachContext<URadialBlurComponent> PropagateRadialBlurComponentChanges;
	}

	// Update the texture detail
	GSystemSettings.UpdateTextureStreaming();

	// Update the screen resolution
	if( OldSystemSettings.ResX != ResX || OldSystemSettings.ResY != ResY || OldSystemSettings.bFullscreen != bFullscreen )
	{
		if( GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportFrame )
		{
			GEngine->GameViewport->ViewportFrame->Resize( ResX, ResY, bFullscreen );
		}
	}

	// Activate certain system settings on the renderthread
	FSystemSettings::SceneRenderTargetsUpdateRHI( OldSystemSettings, *this );
}

/**
 * Sets new system settings (optionally writes out to the ini). 
 */
void FSystemSettings::ApplyNewSettings( const FSystemSettings& NewSettings, UBOOL bWriteToIni )
{
	// we can set any setting before the engine is initialized so don't bother restoring values.
	UBOOL bEngineIsInitialized = ( GEngine != NULL );

	// if the engine is running, there are certain values we can't set immediately
	if (bEngineIsInitialized)
	{
		// Make a copy of the existing settings so we can compare for changes
		FSystemSettings OldSystemSettings = *this;

		// Read settings from .ini.  This is necessary because settings which need to wait for a restart will be on disk
		// but may not be in memory.  Therefore, we read from disk before capturing old values to revert to.
		LoadFromIni( NULL );

		// apply settings to the runtime system.
		ApplySettings( OldSystemSettings );

		// If requested, save the settings to ini.
		if ( bWriteToIni )
		{
			SaveToIni();
		}

		ApplyOverrides();
	}
	else
	{
		// if the engine is not initialized we don't need to worry about all the deferred settings etc. as we do above.
		( FSystemSettings& )( *this ) = NewSettings;

		// If requested, save the settings to ini.
		if( bWriteToIni )
{
			SaveToIni();
	}

		ApplyOverrides();
		}
	}

/**
 * Ensures that the correct settings are being used based on split screen type.
 */
void FSystemSettings::UpdateSplitScreenSettings( void )
{
}



