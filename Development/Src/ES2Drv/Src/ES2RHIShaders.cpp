/*=============================================================================
 ES2RHIImplementation.cpp: OpenGL ES 2.0 RHI definitions.
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "Engine.h"
#include "ES2RHIPrivate.h"
#include "../../Engine/Src/SceneRenderTargets.h"	// TEST_PROFILEEXSTATE

#if WITH_ES2_RHI

// Set to 1 to enable shadowing of shader parameters to avoid unnecessary OpenGL calls,
// but at the cost of extra comparisons on the CPU.
#define ES2_SHADOW_UNIFORMS	1

/**
 * Shader manager instance
 */
FES2ShaderManager GShaderManager;
GLint GMaxVertexAttribsGLSL;
GLint GCurrentProgramUsedAttribMask;
GLint* GCurrentProgramUsedAttribMapping;

#if _WINDOWS

static TMap<FString,TArray<FProgramKey> > PixelShaderPreprocessedText;
static TMap<FString,TArray<FProgramKey> > VertexShaderPreprocessedText;

#endif


/** A helper class for tracking equivalent complete programs */
class VertexPixelKeyPair
{
public:
	FProgramKey Vertex;
	FProgramKey Pixel;
	VertexPixelKeyPair(FProgramKey& InVertex, FProgramKey& InPixel)
		:	Vertex(InVertex)
		,	Pixel(InPixel)
	{}

	friend UBOOL operator==(const VertexPixelKeyPair& X, const VertexPixelKeyPair& Y)
	{
		return (
			(X.Vertex == Y.Vertex) &&
			(X.Pixel == Y.Pixel)
			);
	}
	friend UBOOL operator!=(const VertexPixelKeyPair& X, const VertexPixelKeyPair& Y)
	{
		return !(
			(X.Vertex == Y.Vertex) &&
			(X.Pixel == Y.Pixel)
			);
			
	}
	friend DWORD GetTypeHash(const VertexPixelKeyPair& Pair)
	{
		return appMemCrc(&Pair,sizeof(Pair));
	}
};

/** These convert a compiled pixel shader and vertex shader master key into the shader program master key */
static TMap<VertexPixelKeyPair, FProgramKey> MasterKeyPairToMasterProgram;

/**
 * Slots to store parameters in, delayed set until used
 */
enum EStandardUniformSlot
{
	// NOTE: BE SURE TO UPDATE THE ARRAY BELOW WHEN YOU
	// ADD ANY ADDITIONAL VALUES TO THIS ENUM!

	SS_LocalToWorldSlot,
	SS_LocalToWorldRotationSlot,
	SS_WorldToViewSlot,
	SS_ViewProjectionSlot,
	SS_LocalToProjectionSlot,
	SS_LightMapScaleSlot,
	SS_LightmapScaleBiasSlot,
	SS_TransformSlot,
	SS_CameraWorldPosition,
	SS_CameraRight,
	SS_CameraUp, // 10
	SS_AxisRotationVectorSourceIndex,
	SS_AxisRotationVectors,
	SS_ParticleUpRightResultScalars,
	SS_AlphaTestRef,
	SS_TextureTransform,
	SS_FogOneOverSquaredRange,
	SS_FogStartSquared,
	SS_FogColor,
	SS_UniformMultiplyColor,
	SS_FadeColorAndAmount, //20
	SS_Bones,
	SS_LightPosition,
	SS_LightDirection,
	SS_LightColor,
	SS_UpperSkyColor,
	SS_LowerSkyColor,
	SS_SpecularColor,
	SS_LightColorTimesSpecularColor,
	SS_SpecularPower,
	SS_EnvironmentColorScale, // 30
	SS_EnvironmentParameters,
	SS_RimLightingColorAndExponent,
	SS_VertexMovementConstants,
	SS_VertexSwayMatrix,
	SS_DecalMatrix,
	SS_DecalLocation,
	SS_DecalOffset,
	SS_PreMultipliedBumpReferencePlane,
	SS_BumpHeightRatio,
	SS_ConstantEmissiveColor, // 40
	SS_FogDistanceScale,
	SS_FogExtinctionDistance,
	SS_FogStartDistance,
	SS_FogMinHeight,
	SS_FogMaxHeight,
	SS_FogInScattering,

	SS_InverseGamma,
	SS_SampleOffsets4,
	SS_SampleWeights4,
	SS_LUTWeights, // 50
	SS_SceneShadowsAndDesaturation,
	SS_SceneInverseHighLights,
	SS_SceneMidTones,
	SS_SceneScaledLuminanceWeights,
	SS_SceneColorize,
	SS_GammaColorScaleAndInverse,
	SS_GammaOverlayColor,
	SS_BloomScaleAndThreshold,
	SS_ColorScale,
	SS_OverlayColor, // 60

	// UberPostProcessVertexShader.usf
	SS_SceneCoordinate1ScaleBias,
	SS_SceneCoordinate2ScaleBias,
	SS_SceneCoordinate3ScaleBias,
	SS_BloomTintAndScreenBlendThreshold,
	SS_ImageAdjustments1,
	SS_ImageAdjustments2,
	SS_ImageAdjustments3,
	SS_HalfResMaskRect,
	SS_ReferenceDOFKernelSize,

	// LightShaftDownSampleVertexShader.usf
	SS_ScreenToWorld, // 70

	// LightShaftDownSamplePixelShader.usf + LightShaftBlurPixelShader.usf + LightShaftApplyPixelShader.usf
	SS_MinZ_MaxZRatio,
	SS_TextureSpaceBlurOrigin,
	SS_UVMinMax,
	SS_AspectRatioAndInvAspectRatio,
	SS_LightShaftParameters,
	SS_LightShaftBlurParameters,
	SS_BloomTintAndThreshold,
	SS_LightShaftSampleOffsets,

	// LightShaftApplyVertexShader.usf
	SS_SourceTextureScaleBias,
	SS_SceneColorScaleBias, // 80

	// LightShaftApplyPixelShader.usf
	SS_DistanceFade,
	SS_BloomScreenBlendThreshold,

	// ShadowProjectionPixelShader.msf
	SS_ScreenToShadowMatrix,
	SS_HomShadowStartPos,
	SS_ShadowFadeFraction,
	SS_ShadowBufferSizeAndSoftTransitionScale,
	SS_ShadowTexelSize,
	SS_ScreenPositionScaleBias,
	SS_ShadowModulateColor,

	// DefaultVertexShader.msf, when it's used for rendering shadow depth (DEPTH_ONLY is 1)
	SS_ProjectionMatrix,
	SS_InvMaxSubjectDepth, // 90
	SS_DepthBias,

	SS_SampleOffsets16,
	SS_SampleWeights16,
	// Forward shadow projection parameters
	SS_PixelShadowCasterWorldPosition,
	SS_ModShadowColor,

	SS_VisualizeParam,

	SS_ViewportScaleBias,

	// Mobile Color Grading
	SS_MobileColorGradingBlend,
	SS_MobileColorGradingDesaturation,
	SS_MobileColorGradingHighlightsMinusShadows, // 100
	SS_MobileColorGradingMidTones,
	SS_MobileColorGradingShadows,

	SS_MobileOpacityMultiplier,

	SS_TiltShiftParameters,

	SS_DOFPackedParameters,
	SS_DOFFactor,
	SS_DOFMinMaxBlurClamp,

	//Distance Field Fonts
	SS_DistanceFieldSmoothWidth,
	SS_DistanceFieldShadowMultiplier,
	SS_DistanceFieldShadowDirection,
	SS_DistanceFieldShadowColor,
	SS_DistanceFieldShadowSmoothWidth,
	SS_DistanceFieldGlowMultiplier,
	SS_DistanceFieldGlowColor,
	SS_DistanceFieldGlowOuterRadius,
	SS_DistanceFieldGlowInnerRadius,

	// Radial blur
	SS_RadialBlurScale,
	SS_RadialBlurFalloffExp,
	SS_RadialBlurOpacity,
	SS_RadialBlurScreenPositionCenter,

	// FXAA
	SS_FXAAQualityRcpFrame,
	SS_FXAAConsoleRcpFrameOpt,
	SS_FXAAConsoleRcpFrameOpt2,

// 	SS_InverseGamma,
// 	SS_SampleWeights,
// 	SS_ColorScale,
// 	SS_OverlayColor,
// 	SS_PreMultipliedBumpReferencePlane,
// 	SS_BumpHeightRatio,
// 	SS_ConstantEmissiveColor,

#if WITH_GFx
    SS_Transform2D,
    SS_TextureTransform2D,
    SS_fsize,
    SS_texscale,
    SS_srctexscale,
    SS_scolor,
    SS_scolor2,
    SS_ColorMatrix,
    SS_BatchFloat,
    SS_BatchMatrix,
    SS_DFShadowEnable,
    SS_DFSize,
#endif

	SS_LightmapScaleBias,
	SS_LayerUVScaleBias,
	SS_LodValues,
	SS_LodDistancesValues,
	SS_LandscapeMonochromeLayerColors,

    // NOTE: BE SURE TO UPDATE THE ARRAY BELOW WHEN YOU
	// ADD ANY ADDITIONAL VALUES TO THIS ENUM!

	SS_MAX
};

/**
 * The names and sizes (in GLfloats) for each of the uniforms enumerated above
 */
struct UniformSlotInfo {
	const char*	OGLName;
	FName		UE3Name;
	EMobileGlobalShaderType GlobalShaderType;
	/** Size of the parameter value, in number of GLfloats. */
	UINT		Size;
};
UniformSlotInfo StandardUniformSlotInfo[] =
{
	// Names need to be unique (at least if different sizes are specified).
	// The GlobalShaderType can be EGST_None if the shadertype is set somewhere else. In the long run we want to get rid of the setting here.
	{ "LocalToWorld",								FName(TEXT("LocalToWorld")),								EGST_None,					16 },				// SS_LocalToWorldSlot
	{ "LocalToWorldRotation",						FName(TEXT("LocalToWorldRotation")),						EGST_None,					9 },				// SS_LocalToWorldRotationSlot
	{ "WorldToView",								FName(TEXT("WorldToView")),									EGST_None,					16 },				// SS_WorldToViewSlot
	{ "ViewProjection",								FName(TEXT("ViewProjection")),								EGST_None,					16 },				// SS_ViewProjectionSlot
	{ "LocalToProjection",							FName(TEXT("LocalToProjection")),							EGST_None,					16 },				// SS_LocalToProjectionSlot
	{ "LightMapScale",								FName(TEXT("LightMapScale")),								EGST_None,					4 * 2 },				// SS_LightMapScaleSlot
	{ "LightmapCoordinateScaleBias",				FName(TEXT("LightmapCoordinateScaleBias")),					EGST_None,					4 },				// SS_LightmapScaleBiasSlot
	{ "Transform",									FName(TEXT("Transform")),									EGST_None,					16 },				// SS_TransformSlot
	{ "CameraWorldPosition",						FName(TEXT("CameraWorldPosition")),							EGST_None,					4 },				// SS_CameraWorldPosition
	{ "CameraRight",								FName(TEXT("CameraRight")),									EGST_None,					4 },				// SS_CameraRight
	{ "CameraUp",									FName(TEXT("CameraUp")),									EGST_None,					4 },				// SS_CameraUp
	{ "AxisRotationVectorSourceIndex",				FName(TEXT("AxisRotationVectorSourceIndex")),				EGST_None,					1},					// SS_AxisRotationVectorSourceIndex
	{ "AxisRotationVectors",						FName(TEXT("AxisRotationVectors")),							EGST_None,					4 * 2 },			// SS_AxisRotationVectors
	{ "ParticleUpRightResultScalars",				FName(TEXT("ParticleUpRightResultScalars")),				EGST_None,					3 },				// SS_ParticleUpRightResultScalars
	{ "AlphaTestRef",								FName(TEXT("AlphaTestRef")),								EGST_None,					1},					// SS_AlphaTestRef
	{ "TextureTransform",							FName(TEXT("TextureTransform")),							EGST_None,					9 },				// SS_TextureTransform
	{ "FogOneOverSquaredRange",						FName(TEXT("FogOneOverSquaredRange")),						EGST_None,					1},					// SS_FogOneOverSquaredRange
	{ "FogStartSquared",							FName(TEXT("FogStartSquared")),								EGST_None,					1},					// SS_FogStartSquared
	{ "FogColor",									FName(TEXT("FogColor")),									EGST_None,					4 },				// SS_FogColor
	{ "UniformMultiplyColor",						FName(TEXT("UniformMultiplyColor")),						EGST_None,					4 },				// SS_UniformMultiplyColor
	{ "FadeColorAndAmount",							FName(TEXT("FadeColorAndAmount")),							EGST_None,					4 },				// SS_FadeColorAndAmount
	{ "BoneMatrices",								FName(TEXT("BoneMatrices")),								EGST_None,					12 },				// SS_Bones (Note: size is for a single bone only)
	{ "LightPositionAndInvRadius",					FName(TEXT("LightPositionAndInvRadius")),					EGST_None,					4 },				// SS_LightPosition
	{ "LightDirectionAndbDirectional",				FName(TEXT("LightDirectionAndbDirectional")),				EGST_None,					4 },				// SS_LightDirection
	{ "DirectionalLightColor",						FName(TEXT("LightColorAndFalloffExponent")),				EGST_None,					4 },				// SS_LightColor
	{ "UpperSkyColor",								FName(TEXT("UpperSkyColor")),								EGST_None,					4 },				// SS_UpperSkyColor
	{ "LowerSkyColor",								FName(TEXT("LowerSkyColor")),								EGST_None,					4 },				// SS_LowerSkyColor
	{ "SpecularColor",								FName(TEXT("SpecularColor")),								EGST_None,					3 },				// SS_SpecularColor
	{ "LightColorTimesSpecularColor",				FName(TEXT("LightColorTimesSpecularColor")),				EGST_None,					3 },				// SS_LightColorTimesSpecularColor
	{ "SpecularPower",								FName(TEXT("SpecularPower")),								EGST_None,					1},					// SS_SpecularPower
	{ "EnvironmentColorScale",						FName(TEXT("EnvironmentColorScale")),						EGST_None,					3 },				// SS_EnvironmentColorScale
	{ "EnvironmentParameters",						FName(TEXT("EnvironmentParameters")),						EGST_None,					3 },				// SS_EnvironmentParameters
	{ "RimLightingColorAndExponent",				FName(TEXT("RimLightingColorAndExponent")),					EGST_None,					4 },				// SS_RimLightingColorAndExponent
	{ "VertexMovementConstants",					FName(TEXT("VertexMovementConstants")),						EGST_None,					3 },				// SS_VertexMovementConstants
	{ "VertexSwayMatrix",							FName(TEXT("VertexSwayMatrix")),							EGST_None,					16 },				// SS_VertexSwayMatrix
	{ "DecalMatrix",								FName(TEXT("DecalMatrix")),									EGST_None,					16 },				// SS_DecalMatrix
	{ "DecalLocation",								FName(TEXT("DecalLocation")),								EGST_None,					3 },				// SS_DecalLocation
	{ "DecalOffset",								FName(TEXT("DecalOffset")),									EGST_None,					2 },				// SS_DecalOffset
	{ "PreMultipliedBumpReferencePlane",			FName(TEXT("PreMultipliedBumpReferencePlane")),				EGST_None,					1 },				// SS_PreMultipliedBumpReferencePlane
	{ "BumpHeightRatio",							FName(TEXT("BumpHeightRatio")),								EGST_None,					1 },				// SS_BumpHeightRatio
	{ "ConstantEmissiveColor",						FName(TEXT("ConstantEmissiveColor")),						EGST_None,					4 },				// SS_ConstantEmissiveColor
	{ "FogDistanceScale",							FName(TEXT("FogDistanceScale")),							EGST_None,					4 },				// SS_FogDistanceScale
	{ "FogExtinctionDistance",						FName(TEXT("FogExtinctionDistance")),						EGST_None,					4 },				// SS_FogExtinctionDistance
	{ "FogStartDistance",							FName(TEXT("FogStartDistance")),							EGST_None,					4 },				// SS_FogStartDistance
	{ "FogMinHeight",								FName(TEXT("FogMinHeight")),								EGST_None,					4 },				// SS_FogMinHeight
	{ "FogMaxHeight",								FName(TEXT("FogMaxHeight")),								EGST_None,					4 },				// SS_FogMaxHeight
	{ "FogInScattering",							FName(TEXT("FogInScattering")),								EGST_None,					16 },				// SS_FogInScattering

	{ "InverseGamma",								FName(TEXT("InverseGamma")),								EGST_None,					1 },				// SS_InverseGamma
	{ "SampleOffsets4",								FName(TEXT("SampleOffsets4")),								EGST_None,					8 },				// SS_SampleOffsets4
	{ "SampleWeights4",								FName(TEXT("SampleWeights4")),								EGST_None,					16 },				// SS_SampleWeights4
	{ "LUTWeights",									FName(TEXT("LUTWeights")),									EGST_LUTBlender,			5 },				// SS_LUTWeights
	{ "SceneShadowsAndDesaturation",				FName(TEXT("SceneShadowsAndDesaturation")),					EGST_LUTBlender,			4 },				// SS_SceneShadowsAndDesaturation
	{ "SceneInverseHighLights",						FName(TEXT("SceneInverseHighLights")),						EGST_LUTBlender,			4 },				// SS_SceneInverseHighLights
	{ "SceneMidTones",								FName(TEXT("SceneMidTones")),								EGST_LUTBlender,			4 },				// SS_SceneMidTones
	{ "SceneScaledLuminanceWeights",				FName(TEXT("SceneScaledLuminanceWeights")),					EGST_LUTBlender,			4 },				// SS_SceneScaledLuminanceWeights
	{ "SceneColorize",								FName(TEXT("SceneColorize")),								EGST_LUTBlender,			4 },				// SS_SceneColorize
	{ "GammaColorScaleAndInverse",					FName(TEXT("GammaColorScaleAndInverse")),					EGST_LUTBlender,			4 },				// SS_GammaColorScaleAndInverse
	{ "GammaOverlayColor",							FName(TEXT("GammaOverlayColor")),							EGST_LUTBlender,			4 },				// SS_GammaOverlayColor
	{ "BloomScaleAndThreshold",						FName(TEXT("BloomScaleAndThreshold")),						EGST_None,					4 },				// SS_BloomScaleAndThreshold
	{ "ColorScale",									FName(TEXT("ColorScale")),									EGST_None,					4 },				// SS_ColorScale
	{ "OverlayColor",								FName(TEXT("OverlayColor")),								EGST_None,					4 },				// SS_OverlayColor

	{ "SceneCoordinate1ScaleBias",					FName(TEXT("SceneCoordinate1ScaleBias")),					EGST_None,					4 },				// SS_SceneCoordinate1ScaleBias
	{ "SceneCoordinate2ScaleBias",					FName(TEXT("SceneCoordinate2ScaleBias")),					EGST_None,					4 },				// SS_SceneCoordinate2ScaleBias
	{ "SceneCoordinate3ScaleBias",					FName(TEXT("SceneCoordinate3ScaleBias")),					EGST_None,					4 },				// SS_SceneCoordinate3ScaleBias
	{ "BloomTintAndScreenBlendThreshold",			FName(TEXT("BloomTintAndScreenBlendThreshold")),			EGST_UberPostProcess,		4 },				// SS_BloomTintAndScreenBlendThreshold
	{ "ImageAdjustments1",							FName(TEXT("ImageAdjustments1")),							EGST_UberPostProcess,		4 },				// SS_ImageAdjustments1
	{ "ImageAdjustments2",							FName(TEXT("ImageAdjustments2")),							EGST_UberPostProcess,		4 },				// SS_ImageAdjustments2
	{ "ImageAdjustments3",							FName(TEXT("ImageAdjustments3")),							EGST_UberPostProcess,		4 },				// SS_ImageAdjustments3
	{ "HalfResMaskRect",							FName(TEXT("HalfResMaskRect")),								EGST_UberPostProcess,		4 },				// SS_HalfResMaskRect
	{ "ReferenceDOFKernelSize",						FName(TEXT("ReferenceDOFKernelSize")),						EGST_UberPostProcess,		4 },				// SS_ReferenceDOFKernelSize
	{ "ScreenToWorld",								FName(TEXT("ScreenToWorld")),								EGST_None,					16 },				// SS_ScreenToWorld
	{ "MinZ_MaxZRatio",								FName(TEXT("MinZ_MaxZRatio")),								EGST_None,					4 },				// SS_MinZ_MaxZRatio
	{ "TextureSpaceBlurOrigin",						FName(TEXT("TextureSpaceBlurOrigin")),						EGST_None,					2 },				// SS_TextureSpaceBlurOrigin (shared)
	{ "UVMinMax",									FName(TEXT("UVMinMax")),									EGST_None,					4 },				// SS_UVMinMax (shared)
	{ "AspectRatioAndInvAspectRatio",				FName(TEXT("AspectRatioAndInvAspectRatio")),				EGST_None,					4 },				// SS_AspectRatioAndInvAspectRatio (shared)
	{ "LightShaftParameters",						FName(TEXT("LightShaftParameters")),						EGST_None,					4 },				// SS_LightShaftParameters (shared)
	{ "LightShaftBlurParameters",					FName(TEXT("LightShaftBlurParameters")),					EGST_None,					4 },				// SS_LightShaftBlurParameters
	{ "BloomTintAndThreshold",						FName(TEXT("BloomTintAndThreshold")),						EGST_None,					4 },				// SS_BloomTintAndThreshold (shared)
	{ "LightShaftSampleOffsets",					FName(TEXT("LightShaftSampleOffsets")),						EGST_None,					8 },				// SS_LightShaftSampleOffsets
	{ "SourceTextureScaleBias",						FName(TEXT("SourceTextureScaleBias")),						EGST_LightShaftApply,		4 },				// SS_SourceTextureScaleBias
	{ "SceneColorScaleBias",						FName(TEXT("SceneColorScaleBias")),							EGST_LightShaftApply,		4 },				// SS_SceneColorScaleBias
	{ "DistanceFade",								FName(TEXT("DistanceFade")),								EGST_LightShaftApply,		1 },				// SS_DistanceFade
	{ "BloomScreenBlendThreshold",					FName(TEXT("BloomScreenBlendThreshold")),					EGST_LightShaftApply,		1 },				// SS_BloomScreenBlendThreshold

	{ "ScreenToShadowMatrix",						FName(TEXT("ScreenToShadowMatrix")),						EGST_None,					16 },				// SS_ScreenToShadowMatrix
	{ "HomShadowStartPos",							FName(TEXT("HomShadowStartPos")),							EGST_ShadowProjection,		4 },				// SS_HomShadowStartPos
	{ "ShadowFadeFraction",							FName(TEXT("ShadowFadeFraction")),							EGST_None,					1 },				// SS_ShadowFadeFraction
	{ "ShadowBufferSizeAndSoftTransitionScale",		FName(TEXT("ShadowBufferSizeAndSoftTransitionScale")),		EGST_None,					3 },				// SS_ShadowBufferSizeAndSoftTransitionScale
	{ "ShadowTexelSize",							FName(TEXT("ShadowTexelSize")),								EGST_None,					2 },				// SS_ShadowTexelSize
	{ "ScreenPositionScaleBias",					FName(TEXT("ScreenPositionScaleBias")),						EGST_None,					4 },				// SS_ScreenPositionScaleBias
	{ "ShadowModulateColor",						FName(TEXT("ShadowModulateColor")),							EGST_None,					4 },				// SS_ShadowModulateColor

	{ "ProjectionMatrix",							FName(TEXT("ProjectionMatrix")),							EGST_None,					16 },				// SS_ProjectionMatrix
	{ "InvMaxSubjectDepth",							FName(TEXT("InvMaxSubjectDepth")),							EGST_None,					1 },				// SS_InvMaxSubjectDepth
	{ "DepthBias",									FName(TEXT("DepthBias")),									EGST_None,					1 },				// SS_DepthBias

	{ "SampleOffsets16",							FName(TEXT("SampleOffsets16")),								EGST_None,					8*4 },				// SS_SampleOffsets16
	{ "SampleWeights16",							FName(TEXT("SampleWeights16")),								EGST_None,					16*4 },				// SS_SampleWeights16
	{ "ShadowCasterWorldPosition",					FName(TEXT("ShadowCasterWorldPosition")),					EGST_None,					3 },				// SS_PixelShadowCasterWorldPosition
	{ "ModShadowColor",								FName(TEXT("ModShadowColor")),								EGST_None,					3 },				// SS_ModShadowColor
	
	{ "VisualizeParam",								FName(TEXT("VisualizeParam")),								EGST_None,					8 },				// SS_VisualizeParam
	{ "ViewportScaleBias",							FName(TEXT("ViewportScaleBias")),							EGST_None,					4 },				// SS_ViewportScaleBias

	{ "MobileColorGradingBlend",					FName(TEXT("MobileColorGradingBlend")),						EGST_None,					1 },				// SS_MobileColorGradingBlend
	{ "MobileColorGradingDesaturation",				FName(TEXT("MobileColorGradingDesaturation")),				EGST_None,					1 },				// SS_MobileColorGradingDesaturation
	{ "MobileColorGradingHighlightsMinusShadows",	FName(TEXT("MobileColorGradingHighlightsMinusShadows")),	EGST_None,					4 },				// SS_MobileColorGradingHighlightsMinusShadows
	{ "MobileColorGradingMidTones",					FName(TEXT("MobileColorGradingMidTones")),					EGST_None,					4 },				// SS_MobileColorGradingMidTones
	{ "MobileColorGradingShadows",					FName(TEXT("MobileColorGradingShadows")),					EGST_None,					4 },				// SS_MobileColorGradingShadows

	{ "MobileOpacityMultiplier",					FName(TEXT("MobileOpacityMultiplier")),						EGST_None,					1},					// SS_MobileOpacityMultiplier

	{ "TiltShiftParameters",						FName(TEXT("TiltShiftParameters")),							EGST_None,					4 },				// SS_TiltShiftParameters

	{ "DOFPackedParameters",						FName(TEXT("DOFPackedParameters")),							EGST_None,					4 },				// SS_DOFPackedParameters
	{ "DOFFactor",									FName(TEXT("DOFFactor")),									EGST_None,					1 },				// SS_DOFFactor
	{ "DOFMinMaxBlurClamp",							FName(TEXT("DOFMinMaxBlurClamp")),							EGST_None,					4 },				// SS_DOFMinMaxBlurClamp

	{ "SmoothWidth",								FName(TEXT("SmoothWidth")),									EGST_None,					1 },				// SS_DistanceFieldSmoothWidth
	{ "ShadowMultiplier",							FName(TEXT("ShadowMultiplier")),							EGST_None,					1 },				// SS_DistanceFieldShadowMultiplier
	{ "ShadowDirection",							FName(TEXT("ShadowDirection")),								EGST_None,					2 },				// SS_DistanceFieldShadowDirection
	{ "ShadowColor",								FName(TEXT("ShadowColor")),									EGST_None,					4 },				// SS_DistanceFieldShadowColor
	{ "ShadowSmoothWidth",							FName(TEXT("ShadowSmoothWidth")),							EGST_None,					1 },				// SS_DistanceFieldShadowSmoothWidth
	{ "GlowMultiplier",								FName(TEXT("GlowMultiplier")),								EGST_None,					1 },				// SS_DistanceFieldGlowMultiplier
	{ "GlowColor",									FName(TEXT("GlowColor")),									EGST_None,					4 },				// SS_DistanceFieldGlowColor
	{ "GlowOuterRadius",							FName(TEXT("GlowOuterRadius")),								EGST_None,					2 },				// SS_DistanceFieldGlowOuterRadius
	{ "GlowInnerRadius",							FName(TEXT("GlowInnerRadius")),								EGST_None,					2 },				// SS_DistanceFieldGlowInnerRadius

	{ "RadialBlurScale",							FName(TEXT("RadialBlurScale")),								EGST_None,					1 },				// SS_RadialBlurScale
	{ "RadialBlurFalloffExp",						FName(TEXT("RadialBlurFalloffExp")),						EGST_None,					1 },				// SS_RadialBlurFalloffExp
	{ "RadialBlurOpacity",							FName(TEXT("RadialBlurOpacity")),							EGST_None,					1 },				// SS_RadialBlurOpacity
	{ "RadialBlurScreenPositionCenter",				FName(TEXT("RadialBlurScreenPositionCenter")),				EGST_None,					4 },				// SS_RadialBlurScreenPositionCenter

	{ "fxaaQualityRcpFrame",						FName(TEXT("fxaaQualityRcpFrame")),							EGST_None,					2 },				// SS_FXAAQualityRcpFrame
	{ "fxaaConsoleRcpFrameOpt",						FName(TEXT("fxaaConsoleRcpFrameOpt")),						EGST_None,					4 },				// SS_FXAAConsoleRcpFrameOpt
	{ "fxaaConsoleRcpFrameOpt2",					FName(TEXT("fxaaConsoleRcpFrameOpt2")),						EGST_None,					4 },				// SS_FXAAConsoleRcpFrameOpt2

#if WITH_GFx
    { "Transform2D",						        FName(TEXT("Transform2D")),									EGST_None,                  8 },
    { "TextureTransform2D",							FName(TEXT("TextureTransform2D")),					        EGST_None,                  16 },
    { "fsize",										FName(TEXT("fsize")),										EGST_None,                  4 },
    { "texscale",									FName(TEXT("texscale")),									EGST_None,                  2 },
    { "srctexscale",								FName(TEXT("srctexscale")),									EGST_None,                  2 },
    { "scolor",										FName(TEXT("scolor")),										EGST_None,                  4 },
    { "scolor2",									FName(TEXT("scolor2")),										EGST_None,                  4 },
    { "ColorMatrix",								FName(TEXT("ColorMatrix")),									EGST_None,                  16 },
    { "BatchFloat",									FName(TEXT("BatchFloat")),									EGST_None,                  4*10*15 },
    { "BatchMatrix",								FName(TEXT("BatchMatrix")),									EGST_None,                  16*10*15 },
    { "DFShadowEnable",								FName(TEXT("DFShadowEnable")),								EGST_None,                  1 },
    { "DFSize",										FName(TEXT("DFSize")),										EGST_None,                  4 },
#endif

	/* Landscape vertex shader params */
	{ "LightmapScaleBias",							FName(TEXT("LightmapScaleBias")),							EGST_None,					4 },
	{ "LayerUVScaleBias",							FName(TEXT("LayerUVScaleBias")),							EGST_None,					4 },
	{ "LodValues",									FName(TEXT("LodValues")),									EGST_None,					4 },
	{ "LodDistancesValues",							FName(TEXT("LodDistancesValues")),							EGST_None,					4 },
	/* Landscape pixel shader params */
	{ "LandscapeMonochromeLayerColors",				FName(TEXT("LandscapeMonochromeLayerColors")),				EGST_None,					3 * 4 },
};

/**
 * This function returns how which bits to care about in the GStateShadow.BoundTextureMask
 * This is only needed on Flash where the texture format controls the compiled shader.
 * This is used so that the default shader putting DXT5 in slot 5 doesn't make a different
 * texture mask and another compile for the Simple shader, when Simple only cares about slot 0.
 * This is much simpler than trying to UNset texture formats between draw calls
 *
 * NOTE: If the Flash shaders change textures samplers (fs*), this MUST be adjusted!!!!!
 */
static UINT GetPrimitiveTypeSamplerMask(EMobilePrimitiveType Type)
{
#if FLASH
	switch (Type)
	{
		case EPT_Default:
			// just allow for all samplers here 
			return (1 << MAX_Mapped_MobileTexture) - 1;

		case EPT_Particle:
		case EPT_BeamTrailParticle:
		case EPT_LensFlare:
			// particles use samplers 0 and 5
			return (1 << 0) | (1<<5);
				 
		case EPT_Simple:
			// Simple just uses sampler 0
			return (1 << 0);

		// other shaders use RGBA textures, so no need to do any texture format processing
		default:
			return 0;
	}
#else
	return 0;
#endif
}


/**
 * Returns the slot index and the size of a mobile uniform parameter.
 *
 * @param ParamName		Name of the uniform parameter to check for.
 * @param OutNumBytes	[out] Set to the size of the parameter value, in bytes, if the parameter was found.
 * @return				Parameter slot index, or -1 if the parameter was not found.
 */
INT FES2RHI::GetMobileUniformSlotIndexByName(FName ParamName, WORD& NumBytes)
{
	// Adjust any game-specific values in the table
	if ( StandardUniformSlotInfo[SS_Bones].Size != (12 * GSystemSettings.MobileBoneCount) )
	{
		// The bones uniform size is only for a single bone, so scale by the maximum the game supports
		StandardUniformSlotInfo[SS_Bones].Size = (12 * GSystemSettings.MobileBoneCount);
	}

	for( INT SlotIndex = 0; SlotIndex < ARRAY_COUNT(StandardUniformSlotInfo); SlotIndex++ )
	{
		if( ParamName == StandardUniformSlotInfo[SlotIndex].UE3Name )
		{
			NumBytes = StandardUniformSlotInfo[SlotIndex].Size * sizeof(GLfloat);
			return SlotIndex;
		}
	}
	//warnf( NAME_Warning, TEXT("Unable to find slot index for requested name: %s"), *(ParamName.ToString()) );
	return -1;
}

/** 
 * The type of data stored in a delayed/versioned shader parameter 
 */
enum EUniformSetter
{
	US_None,
	US_Uniform1iSetter,
	US_Uniform1fvSetter,
	US_Uniform2fvSetter,
	US_Uniform3fvSetter,
	US_Uniform4fvSetter,
	US_UniformMatrix3fvSetter,
	US_UniformMatrix4fvSetter,
};


/**
 * Container struct for holding information about setting a shader parameter
 * at render time, when it has changed
 */
struct FVersionedShaderParameter 
{
	FVersionedShaderParameter() 
	: Version( 1 ),
	  Setter( US_None ),
	  Count( 0 ),
	  Data( NULL )
	{
	}
	
	// "Version" of the parameter; incremented when it's set by the engine
	INT Version;

	// The type of data in Data
	EUniformSetter Setter;

	// The number of parameters in Data (i.e. for arrays)
	INT	Count;

	// Raw data to hold any parameter array (big enough for the largest param, which is bone data)
	void* Data;
};

IMPLEMENT_COMPARE_CONSTREF( FString, ES2RHIShadersShaderKey, { QWORD AKey = HexStringToQWord(*A); QWORD BKey = HexStringToQWord(*B);  if (AKey < BKey) return -1; if (AKey > BKey) return 1; return 0;} )

/** 
 * Structure to hold a shader and it's compiled instances (one instance per texture 
 * format/alphatest combinations)
 */
class FES2ShaderProgram 
{
public:

	// A single instance of a compiled shader
	struct FProgInstance
	{
		struct UniformShadowData {
			// Unreal uniform slot
			EStandardUniformSlot Slot;
			// The mapping from our symbolic slot name to the actual slot used in the program
			GLint UniformLocation;
			// The last version of the uniform that have been set for this program
			GLint Version;
			// The actual data for the last version of the uniform, allocated at program creation time
			void* Data;
		};

		FProgInstance()
		{
			Program = 0;
			VertexShaderHandle = 0;
			PixelShaderHandle = 0;
			UsedAttribMask = 0;
			for( INT i = 0; i < ARRAY_COUNT(UsedAttribMapping); i++ )
			{
				UsedAttribMapping[i] = -1;
			}
#if !FINAL_RELEASE
			for( INT i = 0; i < ARRAY_COUNT(UsedTextureSlotMapping); i++ )
			{
				UsedTextureSlotMapping[i] = -1;
			}
#endif
			appMemzero( Uniforms, sizeof( Uniforms ) );
			NumUniforms = 0;
			bWarmed = FALSE;
		}

		// GL name for the program
		GLuint Program;

		// Handle to program's vertex shader
		GLuint VertexShaderHandle;

		// Handle to program's pixel shader
		GLuint PixelShaderHandle;

		// The set of attributes used by this program
		GLint UsedAttribMask;  
		// The UE3 to GLSL attribute index remapping array
		GLint UsedAttribMapping[16];
#if !FINAL_RELEASE
		// The texture index mapped in each slot
		GLint UsedTextureSlotMapping[MAX_Mapped_MobileTexture];
#endif

		// A global shadow of program uniform state
		UniformShadowData Uniforms[SS_MAX];

		// Number of uniforms in this program.
		INT NumUniforms;

		// Indicates if the shader has been warmed to the cache or not yet
		UBOOL bWarmed;
	};

	/** The type of shader, one of EMobilePrimitiveType */
	EMobilePrimitiveType ShadedPrimitiveType;

	/** Name of the pixel shader program, to load the shader from disk */
	FString PixelShaderName;
	/** Name of the vertex shader program, to load the shader from disk */
	FString VertexShaderName;

	/** True if this shader supports light maps */
	DWORD BaseFeatures;
	
	/** Mapping of texture format/alphatest key to each compiled instance */
#if FLASH
	TMap<FProgramKey, TMap<DWORD, FProgInstance*> > ProgMap;
#else
	TMap<FProgramKey, FProgInstance*> ProgMap;
#endif

	/** Map to convert a program key to a master program key */
	TMap<FProgramKey, FProgramKey> KeyToMasterProgramKey;

	/** Maps of engine and preprocessed shaders to offset/size in AllShaders.bin (the QWORD has the offset in high 32 bits, size in low 32) */
	static TMap<FString, QWORD> EngineShadersInfo;
	static TMap<FString, QWORD> PreprocessedShadersInfo;

	/** The AllShaders.bin file that we load shader source from */
	static FArchive* AllShadersFile;

	/** A couple class variables to track the current program and program instance */
	static FProgInstance* CurrentProgInstance;
	static FProgInstance* NextProgInstance;
	static GLuint CurrentProgram;

	// Get/set for the ProgMap
	FProgInstance* GetInstance(const FProgramKey& Key, DWORD TextureUsageFlags)
	{
#if FLASH
		// look up based on texture usage
		TMap<DWORD, FProgInstance*>* TextureMap = ProgMap.Find(Key);
		return TextureMap ? TextureMap->FindRef(TextureUsageFlags) : NULL;
#else
		return ProgMap.FindRef(Key);
#endif
	}

	void SetInstance(const FProgramKey& Key, DWORD TextureUsageFlags, FProgInstance* Instance)
	{
#if FLASH
		// set based on texture usage
		TMap<DWORD, FProgInstance*>* TextureMap = ProgMap.Find(Key);
		if (TextureMap == NULL)
		{
			TextureMap = &ProgMap.Set(Key, TMap<DWORD, FProgInstance*>());
		}
		TextureMap->Set(TextureUsageFlags, Instance);
#else
		ProgMap.Set(Key, Instance);
#endif
	}

	/**
	 * Gathers the current state of the ShaderManager and packs it into the supplied ProgramKey
	 *
	 * @param KeyValue	[out] The packed program key data into a single QWORD
	 * @param KeyData	[out] The current ShaderManager state in an unpacked, raw form
	 */
	void GenerateCurrentProgramKey( FProgramKey& KeyValue, FProgramKeyData& KeyData )
	{
		// Mirror the mobile primitive override that exists when rendering.
		EMobilePrimitiveType Type = GShaderManager.PrimitiveType;
		if ((Type == EPT_Simple) && GShaderManager.bIsDistanceFieldFont)
		{
			Type = EPT_DistanceFieldFont;
		}

		// NOTE: Changing this function might require a change to FMaterialResource::GetMobileMaterialSortKey
		// Initialize the ProgramKeyData structure with current values
		KeyData.Start();
		{
			//Reserved for primitive type & platform features
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_PlatformFeatures, GShaderManager.PlatformFeatures);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_PrimitiveType, Type);

			UBOOL bAllowFog = TRUE;
#if !FINAL_RELEASE
			if(!TEST_PROFILEEXSTATE(0x80, GWorld ? GWorld->GetRealTimeSeconds() : 0.f))
			{
				bAllowFog = FALSE;
			}
#endif

			//World/Misc
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsDepthOnlyRendering, GShaderManager.IsDepthOnly() || GShaderManager.IsShadowDepthOnly());
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsGradientFogEnabled, (bAllowFog && GShaderManager.IsGradientFogEnabled() && !GShaderManager.IsFogSaturated()));
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsHeightFogEnabled, bAllowFog && GShaderManager.IsHeightFogEnabled());
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_TwoSided, FALSE);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_ParticleScreenAlignment, GShaderManager.MeshSettings.ParticleScreenAlignment);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_ForwardShadowProjectionShaderType, GShaderManager.IsForwardShadowProjection());
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsMobileColorGradingEnabled, GShaderManager.IsMobileColorGradingEnabled());

			//Vertex Factory Flags
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsLightmap, ((GShaderManager.VertexFactoryFlags & EShaderBaseFeatures::Lightmap) ? TRUE : FALSE));
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsDirectionalLightmap, ((GShaderManager.VertexFactoryFlags & EShaderBaseFeatures::DirectionalLightmap) ? TRUE : FALSE));
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsSkinned, ((GShaderManager.VertexFactoryFlags & EShaderBaseFeatures::GPUSkinning) ? TRUE : FALSE));
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsDecal, ((GShaderManager.VertexFactoryFlags & EShaderBaseFeatures::Decal) ? TRUE : FALSE));
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsSubUV, ((GShaderManager.VertexFactoryFlags & EShaderBaseFeatures::SubUVParticles) ? TRUE : FALSE));
			UBOOL bIsLandscape = ((GShaderManager.VertexFactoryFlags & EShaderBaseFeatures::Landscape) ? TRUE : FALSE);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsLandscape, bIsLandscape);
			//Material provided
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsLightingEnabled, GShaderManager.VertexSettings.bIsLightingEnabled);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_BlendMode, GShaderManager.BlendMode);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_BaseTextureTexCoordsSource, GShaderManager.VertexSettings.BaseTextureTexCoordsSource);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_DetailTextureTexCoordsSource, GShaderManager.VertexSettings.DetailTextureTexCoordsSource);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_MaskTextureTexCoordsSource, GShaderManager.VertexSettings.MaskTextureTexCoordsSource);

			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsBaseTextureTransformed, GShaderManager.VertexSettings.bBaseTextureTransformed);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsEmissiveTextureTransformed, GShaderManager.VertexSettings.bEmissiveTextureTransformed);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsNormalTextureTransformed, GShaderManager.VertexSettings.bNormalTextureTransformed);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsMaskTextureTransformed, GShaderManager.VertexSettings.bMaskTextureTransformed);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsDetailTextureTransformed, GShaderManager.VertexSettings.bDetailTextureTransformed);

			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsSpecularEnabled, GShaderManager.IsSpecularEnabled());
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsDetailNormalEnabled, GShaderManager.VertexSettings.bUseDetailNormal);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsPixelSpecularEnabled, GShaderManager.IsPixelSpecularEnabled());
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsNormalMappingEnabled, GShaderManager.IsNormalMappingEnabled());
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsEnvironmentMappingEnabled, GShaderManager.IsEnvironmentMappingEnabled());
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_MobileEnvironmentBlendMode, GShaderManager.PixelSettings.EnvironmentBlendMode);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsMobileEnvironmentFresnelEnabled, (GShaderManager.VertexSettings.EnvironmentFresnelAmount != 0.0f));
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsBumpOffsetEnabled, GShaderManager.IsBumpOffsetEnabled());
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsUsingOneDetailTexture, GShaderManager.VertexSettings.bIsUsingOneDetailTexture);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsUsingTwoDetailTexture, GShaderManager.VertexSettings.bIsUsingTwoDetailTexture);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsUsingThreeDetailTexture, GShaderManager.VertexSettings.bIsUsingThreeDetailTexture);
			// if there is a material, like water, that will be broken without this feature, force it to stay as desired and not be overwritten by system settings
			KeyData.LockProgramKeyValue(FProgramKeyData::PKDT_IsUsingOneDetailTexture, GShaderManager.VertexSettings.bIsColorTextureBlendingLocked);
			KeyData.LockProgramKeyValue(FProgramKeyData::PKDT_IsUsingTwoDetailTexture, GShaderManager.VertexSettings.bIsColorTextureBlendingLocked);
			KeyData.LockProgramKeyValue(FProgramKeyData::PKDT_IsUsingThreeDetailTexture, GShaderManager.VertexSettings.bIsColorTextureBlendingLocked);

			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_TextureBlendFactorSource, GShaderManager.VertexSettings.TextureBlendFactorSource);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsWaveVertexMovementEnabled, GShaderManager.IsWaveVertexMovementEnabled());
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_SpecularMask, GShaderManager.PixelSettings.SpecularMask);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_AmbientOcclusionSource, GShaderManager.VertexSettings.AmbientOcclusionSource);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_UseUniformColorMultiply, GShaderManager.VertexSettings.bUseUniformColorMultiply);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_UseVertexColorMultiply, GShaderManager.VertexSettings.bUseVertexColorMultiply);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_UseLandscapeMonochromeLayerBlending, bIsLandscape && GShaderManager.VertexSettings.bUseLandscapeMonochromeLayerBlending);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_ColorMultiplySource, GShaderManager.PixelSettings.ColorMultiplySource);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_UseFallbackStreamColor, GShaderManager.IsUsingFallbackColorStream() );
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsRimLightingEnabled, GShaderManager.IsRimLightingEnabled());
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_RimLightingMaskSource, GShaderManager.VertexSettings.RimLightingMaskSource);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_EnvironmentMaskSource, GShaderManager.VertexSettings.EnvironmentMaskSource);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsEmissiveEnabled, GShaderManager.VertexSettings.bIsEmissiveEnabled);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_EmissiveColorSource, GShaderManager.VertexSettings.EmissiveColorSource);

            KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_GFxBlendMode, 0);

			if ( GShaderManager.PrimitiveType == EPT_GlobalShader)
			{
				KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_GlobalShaderType, GShaderManager.GlobalShaderType);
			}
			else
			{
				KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_GlobalShaderType, EGST_None);
			}

			if ( GShaderManager.IsDepthOnly())
			{
				KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_DepthShaderType, MobileDepthShader_Normal);
			}
			else if ( GShaderManager.IsShadowDepthOnly())
			{
				KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_DepthShaderType, MobileDepthShader_Shadow);
			}
			else
			{
				KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_DepthShaderType, MobileDepthShader_None);
			}
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_EmissiveMaskSource, GShaderManager.VertexSettings.EmissiveMaskSource);
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_UseGammaCorrection, GShaderManager.IsGammaCorrectionEnabled());
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_AlphaValueSource, GShaderManager.PixelSettings.AlphaValueSource);
#if WITH_GFx
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsGfxGammaCorrectionEnabled, GSystemSettings.bMobileGfxGammaCorrection);
#else
			KeyData.AssignProgramKeyValue(FProgramKeyData::PKDT_IsGfxGammaCorrectionEnabled, FALSE);
#endif
		}
		KeyData.Stop();

		// Pack the structure into a single key
		KeyData.GetPackedProgramKey( KeyValue );

#if _DEBUG
		// In DEBUG only, do some extra sanity tests on the key packer/unpacker
		FProgramKeyData KeyDataDebugTest;
		KeyDataDebugTest.UnpackProgramKeyData(KeyValue);
		checkf(
			(
			(appMemcmp(KeyData.FieldData0.FieldValue, KeyDataDebugTest.FieldData0.FieldValue, sizeof(BYTE) * 64) == 0) &&
			(appMemcmp(KeyData.FieldData1.FieldValue, KeyDataDebugTest.FieldData1.FieldValue, sizeof(BYTE) * 64) == 0)
			), 
			TEXT("Program key pack/unpack mismatch!") );
#endif
	}

	/** 
	 * Sets the name of this program to the give name (it will use this on first render 
	 * to compile the program)
	 */
	void Init( const EMobilePrimitiveType InShadedPrimitiveType, const EMobileGlobalShaderType GlobalShaderType, const DWORD InBaseFeatures ) 
	{
		ShadedPrimitiveType = InShadedPrimitiveType;
		PixelShaderName = GetES2ShaderFilename(ShadedPrimitiveType,GlobalShaderType,SF_Pixel );
		VertexShaderName = GetES2ShaderFilename(ShadedPrimitiveType,GlobalShaderType,SF_Vertex);
		BaseFeatures = InBaseFeatures;
	}
	
	/**
	 * @return the instance of the program that matches the current texture format/alphatest settings
	 */
	FProgInstance* GetCurrentInstance() 
	{
		FProgramKey Key;
		FProgramKeyData KeyData;
		GenerateCurrentProgramKey( Key, KeyData );

		// get the currently set texture formats, masked by the bits we care about for this primitive type
		DWORD TextureUsageFlags = GStateShadow.BoundTextureMask & GetPrimitiveTypeSamplerMask(GShaderManager.PrimitiveType);

		// look to see if the instance already exists
		FProgInstance* Instance = GetInstance(Key, TextureUsageFlags);
		if (Instance != NULL) 
		{
			return Instance;
		}

		// if not, compile and remember it in the map
		FProgInstance NewInstance;
		UBOOL bSuccess;
		FProgramKey MasterKey = InitNewInstance(NewInstance, Key, KeyData, bSuccess, TextureUsageFlags);
		//for whatever reason, source code didn't exist.  Fall back to Key of 0
		if (!bSuccess)
		{
			KeyData = FProgramKeyData();	//reset the key data to be filled in again
			KeyData.Start();
			for (INT FieldIndex = 0; FieldIndex < FProgramKeyData::PKDT1_MAX; ++FieldIndex)
			{
				KeyData.AssignProgramKeyValue(FieldIndex, 0);
			}
			KeyData.OverrideProgramKeyValue(FProgramKeyData::PKDT_PrimitiveType, GShaderManager.PrimitiveType);
			KeyData.Stop();

			//extract new key
			KeyData.GetPackedProgramKey( Key );

			// look to see if the instance already exists
			FProgInstance* Instance = GetInstance(Key, TextureUsageFlags);
			if (Instance != NULL) 
			{
				return Instance;
			}
			//try again
			MasterKey = InitNewInstance(NewInstance, Key, KeyData, bSuccess, TextureUsageFlags);
			check(bSuccess == TRUE);
		}
		if( MasterKey != Key )
		{
			Instance = GetInstance(MasterKey, TextureUsageFlags);
			check(Instance);
			return Instance;
		}
 		else
 		{
 			// Flag that we'd like to time the next draw call
 			STAT(GES2TimeNextDrawCall = TRUE);
		}

#if !FINAL_RELEASE
		if( GUseSeekFreeLoading &&
			GSystemSettings.bUsePreprocessedShaders &&
			GSystemSettings.bFlashRedForUncachedShaders)
		{
			// We just compiles a new instance of a shader - provide a visual
			// cue that this happened (a red screen flash is simple) so we
			// can connect hitches with shader compiles
			GLCHECK(glClearColor( 1.0, 0.0, 0.0, 1.0 ));
			GLCHECK(glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ));		

			FES2Core::SwapBuffers();
			static UINT NumOnDemandShaders = 0;
			GLog->Logf(TEXT("On-Demand glCompileShader num %d,%s, for material %s"), NumOnDemandShaders++, *Key.ToString(), *GShaderManager.MaterialName);
		}
#endif
		// Add the shader to the globally compiled list of shaders (to mirror what we do when we normally initialize
		// the shader programs.

		checkf(GShaderManager.CompiledShaders.Find(Key) == NULL);

		FProgInstance* StoredInstance = new FProgInstance(NewInstance);
		SetInstance(Key, TextureUsageFlags, StoredInstance);

		GShaderManager.CompiledShaders.Add(Key);

		return StoredInstance;
	}
	
	/**
	 * Looks up and binds the named attribute in the given program instance
	 *
	 * @param Instance The compiled instance of a program
	 * @param Location The preset location (see TranslateUnrealUsageToBindLocation) to bind the attribute to
	 * @param Name Name of the attribute variable in the shader
	 */
	void BindAttribLocation( FProgInstance & Instance, INT UE3Location, const char *Name )
	{
		// First check to see if this attrib exists in this shader
		GLint GLSLLocation = 0;
		GLSLLocation = GLCHECK(glGetAttribLocation(Instance.Program, Name));
		if ( GLSLLocation > -1 )
		{
			// If so, set up the UE3 to GLSL index remapping and the used attribute mask
			Instance.UsedAttribMapping[UE3Location] = GLSLLocation;
			Instance.UsedAttribMask |= ( 1 << GLSLLocation );
		}
	}


	// define how source code is pasted together
	enum ESourceSlots
	{
		SOURCESLOT_Defines,
		SOURCESLOT_Common,
		SOURCESLOT_Prefix,
		SOURCESLOT_ShaderCode,
		SOURCESLOT_MAX
	};


#if _WINDOWS
	/**
	 * Saves off the current set of cached program keys
	 */
	void SaveCachedProgramKeys()
	{
		// Only if we're actually caching program keys
		if( GSystemSettings.bCachePreprocessedShaders )
		{
			// put the output shaders in the cooked directory
			FString CookedDirectory;
			appGetCookedContentPath(appGetPlatformType(), CookedDirectory);
			FString CachedProgramsDirectoryName = CookedDirectory + TEXT("Shaders\\");

			FString CachedProgramKeyFileContents;

			CachedProgramKeyFileContents += FString::Printf( TEXT( "version:%d\r\n"),SHADER_MANIFEST_VERSION);

			for (TMap<FString,TArray<FProgramKey> >::TIterator It(VertexShaderPreprocessedText);It;++It)
			{
				if (It.Value().Num() > 1)
				{
					UBOOL First = TRUE;
					for (TArray<FProgramKey>::TIterator ItQ(It.Value());ItQ;++ItQ,First = FALSE)
					{
						FProgramKey MobileKey = *ItQ;
						CachedProgramKeyFileContents += FString::Printf( TEXT( "%s%s"),
							First ? TEXT("vse:") : TEXT(","), 
							*MobileKey.ToString() );
					}
					CachedProgramKeyFileContents += TEXT("\r\n");
				}
			}
			for (TMap<FString,TArray<FProgramKey> >::TIterator It(PixelShaderPreprocessedText);It;++It)
			{
				if (It.Value().Num() > 1)
				{
					UBOOL First = TRUE;
					for (TArray<FProgramKey>::TIterator ItQ(It.Value());ItQ;++ItQ,First = FALSE)
					{
						FProgramKey MobileKey = *ItQ;
						CachedProgramKeyFileContents += FString::Printf( TEXT( "%s%s"),
							First ? TEXT("pse:") : TEXT(","), 
							*MobileKey.ToString() );
					}
					CachedProgramKeyFileContents += TEXT("\r\n");
				}
			}


			TArray<FString> FoundDirectoryNames;
			GFileManager->FindFiles(FoundDirectoryNames, *(CachedProgramsDirectoryName + TEXT("*")), FALSE, TRUE );
			Sort<USE_COMPARE_CONSTREF(FString,ES2RHIShadersShaderKey)>( FoundDirectoryNames.GetTypedData(), FoundDirectoryNames.Num() );
			for( INT DirIndex = 0; DirIndex < FoundDirectoryNames.Num(); DirIndex++ )
			{
				// Construct the contents of the cache file
				CachedProgramKeyFileContents += ( FoundDirectoryNames(DirIndex) + TEXT("\r\n") );
			}

			FString ProgramKeysFileName = CookedDirectory + TEXT("CachedProgramKeys_Verification.txt");
			appSaveStringToFile(CachedProgramKeyFileContents, *ProgramKeysFileName, TRUE);
		}
	}

	/**
	 * Caches a shader to disk and generates profiling data for the shader
	 *
	 * @param	InSources		Source code
	 * @param	InShaderName	The name of this shader
	 * @param	InShaderType	Type of shader
	 */
	void CacheShaderAndGenerateStats( const FProgramKey& ProgramKey, FString& EntireShaderSource, const FString& InShaderName, const EShaderFrequency InShaderType )
	{
		struct 
		{
			/**
			 * Preprocesses the specified shader source file and saves a new output file with the preprocessed code
			 *
			 * @param	ShaderCode				[In, Out] Shader code to preprocess (will be replaced by preprocessed code string)
			 * @param	InTempUnprocessedFile	File name we'll temporarily save unpreprocessed shader code to (deleted afterwards)
			 * @param	InPreprocessedFile		File name of preprocessed shader code
			 * @param	bCleanWhitespace		True if we should clean whitespace while preprocessing the file
			 */
			UBOOL PreprocessShaderSource( FString& ShaderCode, const FString& InTempUnprocessedFile, const FString& InPreprocessedFile, const UBOOL bCleanWhitespace )
			{
				ShaderCode = RunCPreprocessor(ShaderCode,*InPreprocessedFile,bCleanWhitespace);
				return ShaderCode.Len() > 0;
			}
		} LocalFunctions;


		if( GSystemSettings.bCachePreprocessedShaders )
		{
			FString FinalCode = EntireShaderSource;
			const FString PreprocessedDirectoryName = FString::Printf( TEXT( "%s\\%s\\" ), *( appGameDir() + TEXT("Shaders") ), *ProgramKey.ToString() );
			const FFilename PreprocessedFileName = appConvertRelativePathToFull(PreprocessedDirectoryName + FString::Printf( TEXT( "%s.msf.i" ), *InShaderName ));
			const FFilename UnPreprocessedFileName = appConvertRelativePathToFull(PreprocessedDirectoryName + FString::Printf( TEXT( "%s.msf" ), *InShaderName ));
			if (GSystemSettings.bUseCPreprocessorOnShaders)
			{
				// Always clean white space from preprocessed shaders that we may load later on the device
				const UBOOL bShouldCleanWhitespace = TRUE;
				if( !LocalFunctions.PreprocessShaderSource( FinalCode, UnPreprocessedFileName, PreprocessedFileName, bShouldCleanWhitespace ) )
				{
					// ...
				}
			}

			// If we weren't able to preprocess the file, then just save out the original code
			if (!PreprocessedFileName.FileExists())
			{
				appSaveStringToFile( FinalCode, *PreprocessedFileName );
			}
			if (GSystemSettings.bUseCPreprocessorOnShaders &&
				( GSystemSettings.bShareVertexShaders ||
				GSystemSettings.bSharePixelShaders ||
				GSystemSettings.bShareShaderPrograms ))
			{
				check(ShadedPrimitiveType < EPT_MAX);
				TMap<FString,TArray<FProgramKey> >* TargetMap = &PixelShaderPreprocessedText;
				if (InShaderType == SF_Vertex)
				{
					TargetMap = &VertexShaderPreprocessedText;
				}
				TArray<FProgramKey>* Existing = TargetMap->Find(FinalCode);
				if (!Existing)
				{
					TArray<FProgramKey> Blank;
					TargetMap->Set(FinalCode,Blank);
					Existing = TargetMap->Find(FinalCode);
					check(Existing);
				}
				Existing->AddUniqueItem(ProgramKey);
			}
		}

		if( GSystemSettings.bProfilePreprocessedShaders )
		{
			FString FinalCode = EntireShaderSource;
			const FString DumpFileNameWithoutExt = FString::Printf( TEXT( "%s\\%s_%s" ), *( appProfilingDir() + TEXT("Shaders\\Mobile") ), *InShaderName, *ProgramKey.ToString() );
			const FFilename DumpFileName = DumpFileNameWithoutExt + FString( TEXT( ".msf" ) );

			// Windows OpenGL ES2 implementations do not support overriding the floating point precision, so we only set these when compiling shaders for console targets and for profiling.
			FinalCode = FString(TEXT("#define DEFINE_DEFAULT_PRECISION 1\r\n")) + FinalCode;

			// Delete any existing file with the output file name
			GFileManager->Delete( *DumpFileName );

			// Use preprocessed shader source for profiling if we can!
			if (GSystemSettings.bUseCPreprocessorOnShaders)
			{
				const FFilename PreprocessedFileName = DumpFileName;
				const FFilename UnPreprocessedFileName = DumpFileName + TEXT( ".temp" );
				if (GSystemSettings.bUseCPreprocessorOnShaders)
				{
					// Never clean whitespace from shaders that a human will be inspecting for profiling
					const UBOOL bShouldCleanWhitespace = FALSE;
					if( !LocalFunctions.PreprocessShaderSource( FinalCode, UnPreprocessedFileName, PreprocessedFileName, bShouldCleanWhitespace ) )
					{
						// ...
					}
				}
			}

			// If we don't have
			if (!DumpFileName.FileExists())
			{
				appSaveStringToFile( FinalCode, *DumpFileName );
			}


			// Profile source
			{
				const FFilename ExecutableFileName( FString( appBaseDir() ) + FString( TEXT( "..\\NoRedist\\ImgTec\\PVRUniSCo.exe" ) ) );
				if( ExecutableFileName.FileExists() )
				{
					// Spawn PVR shader compiler to generate profiling data
					const FFilename TargetBinFileName = DumpFileNameWithoutExt + FString( TEXT( ".bin" ) );
					const FString CmdLineParams = FString::Printf( TEXT( "%s %s %s -profile" ), *DumpFileName, *TargetBinFileName, InShaderType == SF_Vertex ? TEXT( "-v" ) : TEXT( "-f" ) );
					const UBOOL bLaunchDetached = TRUE;
					const UBOOL bLaunchHidden = TRUE;
					void* ProcHandle = appCreateProc( *ExecutableFileName, *CmdLineParams, bLaunchDetached, bLaunchHidden );
					if( ProcHandle != NULL )
					{
						UBOOL bProfilingComplete = FALSE;
						INT ReturnCode = 1;
						while( !bProfilingComplete )
						{
							bProfilingComplete = appGetProcReturnCode( ProcHandle, &ReturnCode );
							if( !bProfilingComplete )
							{
								appSleep( 0.01f );
							}
						}

						// Was the shader processed successfully?
						if( ReturnCode == 0 )
						{
							// Remove original source and the ".bin" files since we have no use for them currently
							GFileManager->Delete( *DumpFileName );
							GFileManager->Delete( *TargetBinFileName );

							const FFilename ProfilerInfoFileName = DumpFileNameWithoutExt + FString( TEXT( ".prof" ) );

							// Load up the profiling data for this shader
							FString ProfilerInfo;
							const UBOOL bLoadedSuccessfully = appLoadFileToString( ProfilerInfo, *ProfilerInfoFileName );
							if( bLoadedSuccessfully )
							{
								struct FShaderProfilerData
								{
									/** Total number of cycles used by this shader */
									UINT TotalCycles;

									/** Emulated best case cycle count */
									UINT EmulatedBestCaseCycles;

									/** Emulated worst case cycle count */
									UINT EmulatedWorstCaseCycles;

									/** Number of primary attributes */
									UINT NumPrimaryAttributes;

									/** Number of temporary registers */
									UINT NumTemporaryRegisters;

									/** Number of cycles for each line number with active instructions */
									TMap< UINT, UINT > LineNumberCycles;


									/** Constructor */
									FShaderProfilerData()
										: TotalCycles( 0 ),
										  EmulatedBestCaseCycles( 0 ),
										  EmulatedWorstCaseCycles( 0 ),
										  NumPrimaryAttributes( 0 ),
										  NumTemporaryRegisters( 0 ),
										  LineNumberCycles()
									{
									}
								};


								FShaderProfilerData ProfilerData;

								//
								// Parse PowerVR shader profile info
								//
								// NOTE: This is designed to parse the output of a specific version of the PVR GL ES2 compiler:
								//          PVRUniSco PVR GLSL-ES Compiler for SGX540 rev 201. Version 1.7.17.3700
								//
								{
									const TCHAR* SourceText = *ProfilerInfo;
									FString LineText;
									while( ParseLine( &SourceText, LineText ) )		// "Cycles Consumed By Each Source Line"
									{
										// Line number cycle stats
										if( LineText.StartsWith( TEXT( " line: " ) ) )
										{
											// Find the position of the comma in this line
											const INT CommaPos = LineText.InStr( TEXT( "," ) );
											if( CommaPos != INDEX_NONE )
											{
												const INT LineNumberPos = 7;
												const UINT LineNumber = appAtoi( *LineText.Mid( LineNumberPos, CommaPos - LineNumberPos + 1 ) );
												const INT CycleCountPos = CommaPos + 9;
												const UINT CycleCount = appAtoi( *LineText.Mid( CycleCountPos ) );

												// Update dictionary with cycle count for this line number
												ProfilerData.LineNumberCycles.Set( LineNumber, CycleCount );
											}
										}
										else if( LineText.StartsWith( TEXT( "Total consumed cycles" ) ) )
										{
											ProfilerData.TotalCycles = appAtoi( *LineText.Mid( 24 ) );
										}
										else if( LineText.StartsWith( TEXT( "Total cycles best case" ) ) )
										{
											ProfilerData.EmulatedBestCaseCycles = appAtoi( *LineText.Mid( 25 ) );
										}
										else if( LineText.StartsWith( TEXT( "Total cycles worst case" ) ) )
										{
											ProfilerData.EmulatedWorstCaseCycles = appAtoi( *LineText.Mid( 26 ) );
										}
										else if( LineText.StartsWith( TEXT( "  Ratio between cycles and USSE instructions" ) ) )
										{
											// ...  (not interested in this, yet.)
										}
										else if( LineText.StartsWith( TEXT( "  Number of primary attributes" ) ) )
										{
											ProfilerData.NumPrimaryAttributes = appAtoi( *LineText.Mid( 33 ) );
										}
										else if( LineText.StartsWith( TEXT( "  Number of temporary registers" ) ) )
										{
											ProfilerData.NumTemporaryRegisters = appAtoi( *LineText.Mid( 34 ) );
										}
									}
								}


								// Emit a new version of the GLSL file, decorated with profiler data
								{
									const FFilename DecoratedShaderFileName = DumpFileNameWithoutExt + FString( TEXT( "_prof.msf" ) );
									FString DecoratedShaderSource;

									// Add a file header with the total cycle counts
									DecoratedShaderSource += FString::Printf( TEXT( "/* Total cycle count: %d */\r\n" ), ProfilerData.TotalCycles );
									DecoratedShaderSource += FString::Printf( TEXT( "/* Best/Worse case emulated total cycles: %d/%d */\r\n" ), ProfilerData.EmulatedBestCaseCycles, ProfilerData.EmulatedWorstCaseCycles );
									DecoratedShaderSource += FString::Printf( TEXT( "/* Primary attribute count: %d */\r\n" ), ProfilerData.NumPrimaryAttributes );
									DecoratedShaderSource += FString::Printf( TEXT( "/* Temporary registers: %d */\r\n\r\n" ), ProfilerData.NumTemporaryRegisters );

									// Note: Line numbers are one-based, not zero-based!
									UINT CurLineNumber = 1;

									// Make sure the source file ends with a line feed
									if( !FinalCode.EndsWith( TEXT( "\n" ) ) )
									{
										FinalCode.AppendChar( '\n' );
									}

									UINT FirstCharIndexForCurrentLine = 0;
									for( INT CurCharIndex = 0; CurCharIndex < FinalCode.Len(); ++CurCharIndex )
									{
										const TCHAR CurChar = FinalCode[ CurCharIndex ];
										if( CurChar == '\n' )
										{
											// New line

											FString CurLineText;

											// Prefix the line with cycle counts
											if( ProfilerData.LineNumberCycles.HasKey( CurLineNumber ) )
											{
												CurLineText += FString::Printf( TEXT( "/* %3d */  " ), ProfilerData.LineNumberCycles.FindRef( CurLineNumber ) );
											}
											else
											{
												// No cycle counts for this line
												CurLineText += FString( TEXT( "/*     */  " ) );
											}
											
											// Grab line of text, including the line feed
											CurLineText += FinalCode.Mid( FirstCharIndexForCurrentLine, CurCharIndex - FirstCharIndexForCurrentLine );

											// Emit the current line
											DecoratedShaderSource += CurLineText;

											// Move on to the next line
											++CurLineNumber;
											FirstCharIndexForCurrentLine = CurCharIndex + 1;
										}
										else if( CurChar == '\r' )
										{
											// Swallow line feed characters
										}
									}

									// Save decorated shader source
									const UBOOL bSavedSuccessfully = appSaveStringToFile( DecoratedShaderSource, *DecoratedShaderFileName );
									if( bSavedSuccessfully )
									{
										// Go ahead and delete the original files since we have all of the data now
										GFileManager->Delete( *ProfilerInfoFileName );
									}
									else
									{
										warnf( NAME_Warning, TEXT( "Unable to save decorated shader source file" ) );
									}
								}
							}
							else
							{
								warnf( NAME_Warning, TEXT( "Couldn't locate shader profiling data generated by external application" ) );
							}
						}
						else
						{
							warnf( NAME_Warning, TEXT( "Shader profiling failed within the external application" ) );
						}
					}
					else
					{
						warnf( NAME_Warning, TEXT( "Unable to launch external application to profiler shader" ) );
					}
				}
			}
		}
	}
#endif // _WINDOWS

	void SetParameterTextureUnit( FProgInstance& Instance, const char* UniformName, UINT MobileTextureUnit )
	{
		checkSlowish(MobileTextureUnit < MAX_Mapped_MobileTexture);
		GLint Slot = 0;
		Slot = GLCHECK(glGetUniformLocation(Instance.Program, UniformName));
		if (Slot >= 0) GLCHECK(glUniform1i(Slot, MobileTextureUnit));
#if !FINAL_RELEASE
		Instance.UsedTextureSlotMapping[MobileTextureUnit] = Slot;
#endif
	}

	void LinkProgramAndBindUniforms( FProgInstance& Instance, FProgramKey InKey )
	{
		// We have to link twice so that we can build our list of unused attribs.
		GLCHECK(glLinkProgram(Instance.Program));

		// gather attributes and uniforms
		// The globally bound attribute names.
		BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_Position) + 0, "Position" );
		BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_TextureCoordinate) + 0, "TexCoords0" );
		BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_TextureCoordinate) + 1, "TexCoords1" );
		BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_TextureCoordinate) + 2, "TexCoords2" );
		BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_TextureCoordinate) + 3, "TexCoords3" );
		BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_Normal) + 0, "TangentZ" );
		BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_Tangent) + 0, "TangentX" );


		if( (GShaderManager.VertexFactoryFlags & EShaderBaseFeatures::GPUSkinning) != 0 )
		{
			BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_Color) + 0, "VertexColor" );
			BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_BlendWeight) + 0, "BlendWeight" );
			BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_BlendIndices) + 0, "BlendIndices" );
		}
		else
		if( (GShaderManager.VertexFactoryFlags & EShaderBaseFeatures::Landscape) == 0 )
		{
			BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_TextureCoordinate) + 5, "LightMapA" );
			BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_Color) + 0, "LightMapCoordinate" );
			BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_Color) + 0, "Color" );
			BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_Color) + 1, "VertexColor" );
			BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_Normal) + 0, "OldPosition" );
			BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_Tangent) + 0, "Size" );
			BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_BlendWeight) + 0, "Rotation_Sizer" );
			BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_TextureCoordinate) + 1, "ParticleColor" );
			BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_TextureCoordinate) + 2, "Interp_Sizer" );
#if WITH_GFx
			BindAttribLocation(Instance, TranslateUnrealUsageToBindLocation(VEU_BlendIndices) + 0, "vbatch" );
#endif
		}

#if !FLASH
		GLCHECK(glLinkProgram(Instance.Program));
		PrintInfoLog(Instance.Program, *FString::Printf(TEXT("Shader Key %s"), *InKey.ToString()));
#endif
		GLCHECK(glUseProgram(Instance.Program));

		if( (GShaderManager.VertexFactoryFlags & EShaderBaseFeatures::Landscape) != 0 )
		{
			// Landscape uses a different mapping
			SetParameterTextureUnit(Instance, "TextureBase", LandscapeLayer0_MobileTexture);
			SetParameterTextureUnit(Instance, "TextureDetail", LandscapeLayer1_MobileTexture);
			SetParameterTextureUnit(Instance, "TextureDetail2",  LandscapeLayer2_MobileTexture);
			SetParameterTextureUnit(Instance, "TextureDetail3",  LandscapeLayer3_MobileTexture);
			SetParameterTextureUnit(Instance, "TextureLightmap", Lightmap_MobileTexture);
			SetParameterTextureUnit(Instance, "TextureNormal", Normal_MobileTexture);
			SetParameterTextureUnit(Instance, "TextureMask", Mask_MobileTexture);
		}
		else
		{
			SetParameterTextureUnit(Instance, "TextureBase", Base_MobileTexture);
			SetParameterTextureUnit(Instance, "TextureDetail", Detail_MobileTexture);
#if !FLASH
			SetParameterTextureUnit(Instance, "TextureDetail2",  GetDeviceValidMobileTextureSlot(Detail_MobileTexture2));
			SetParameterTextureUnit(Instance, "TextureDetail3",  GetDeviceValidMobileTextureSlot(Detail_MobileTexture3));
#endif
			SetParameterTextureUnit(Instance, "TextureLightmap", Lightmap_MobileTexture);
			SetParameterTextureUnit(Instance, "TextureEnvironment", Environment_MobileTexture);
			SetParameterTextureUnit(Instance, "TextureNormal", Normal_MobileTexture);
			SetParameterTextureUnit(Instance, "TextureMask", Mask_MobileTexture);
			SetParameterTextureUnit(Instance, "TextureEmissive", Emissive_MobileTexture);
			SetParameterTextureUnit(Instance, "TextureLightmap2", Lightmap2_MobileTexture);
		}

		// some hardcoded PP textures
		GLint Slot = 0;
		Slot = GLCHECK(glGetUniformLocation(Instance.Program, "SourceTexture"));
		if (Slot >= 0) GLCHECK(glUniform1i(Slot, 0));
		Slot = GLCHECK(glGetUniformLocation(Instance.Program, "ApplySourceTexture"));
		if (Slot >= 0) GLCHECK(glUniform1i(Slot, 1));
		Slot = GLCHECK(glGetUniformLocation(Instance.Program, "SceneDepthTexture"));
		if (Slot >= 0) GLCHECK(glUniform1i(Slot, 1));
		Slot = GLCHECK(glGetUniformLocation(Instance.Program, "Texture1"));
		if (Slot >= 0) GLCHECK(glUniform1i(Slot, 0));
		Slot = GLCHECK(glGetUniformLocation(Instance.Program, "Texture2"));
		if (Slot >= 0) GLCHECK(glUniform1i(Slot, 1));
		Slot = GLCHECK(glGetUniformLocation(Instance.Program, "Texture3"));
		if (Slot >= 0) GLCHECK(glUniform1i(Slot, 2));
		Slot = GLCHECK(glGetUniformLocation(Instance.Program, "Texture4"));
		if (Slot >= 0) GLCHECK(glUniform1i(Slot, 3));
		Slot = GLCHECK(glGetUniformLocation(Instance.Program, "SceneColorTexture"));
		if (Slot >= 0) GLCHECK(glUniform1i(Slot, 0));
		Slot = GLCHECK(glGetUniformLocation(Instance.Program, "FilterColor2Texture"));
		if (Slot >= 0) GLCHECK(glUniform1i(Slot, 1));
		Slot = GLCHECK(glGetUniformLocation(Instance.Program, "DoFBlurBuffer"));
		if (Slot >= 0) GLCHECK(glUniform1i(Slot, 2));
		Slot = GLCHECK(glGetUniformLocation(Instance.Program, "ColorGradingLUT"));
		if (Slot >= 0) GLCHECK(glUniform1i(Slot, 3));
		Slot = GLCHECK(glGetUniformLocation(Instance.Program, "ShadowDepthTexture"));
		if (Slot >= 0) GLCHECK(glUniform1i(Slot, 2));

		// Look for each uniform we care about, and hook any up that are in the program
		for( INT SlotIndex = 0; SlotIndex < SS_MAX; SlotIndex++ )
		{
			// Look up the uniform for this program
			GLint UniformLocation = GLCHECK( glGetUniformLocation(Instance.Program, StandardUniformSlotInfo[SlotIndex].OGLName) );
			if( UniformLocation >= 0 )
			{
				// It's in this program, so allocate some cache memory for it
				FES2ShaderProgram::FProgInstance::UniformShadowData& UniformData = Instance.Uniforms[Instance.NumUniforms++];
				UniformData.Slot = EStandardUniformSlot(SlotIndex);
				UniformData.UniformLocation = UniformLocation;
				UniformData.Data = appMalloc(sizeof(GLfloat) * StandardUniformSlotInfo[SlotIndex].Size);
			}
		}
	}

	/**
	 * Loads some ANSI shader source and stores it in a local variable
	 */
	static UBOOL LoadShaderFromAllShaders(const FString& ShaderName, UBOOL bIsEngineShader, FString& OutSource)
	{
		if (!GUseSeekFreeLoading)
		{
			return appLoadFileToString(OutSource, *(appEngineDir() + TEXT("Shaders\\Mobile\\") + ShaderName));
		}

		// which map are we going to use?
		TMap<FString, QWORD>& Metadata = bIsEngineShader ? EngineShadersInfo : PreprocessedShadersInfo;

		OutSource.Empty();

		// open the AllShaders.bin file
		if (AllShadersFile == NULL)
		{
			FString CookedPath;
			appGetCookedContentPath(appGetPlatformType(), CookedPath);

			AllShadersFile = GFileManager->CreateFileReader(*(CookedPath + TEXT("AllShaders.bin")));
			if (!AllShadersFile)
			{
				// fail if the file didn't open
				return FALSE;
			}
		}

		// has the metadata been loaded yet?
		if (Metadata.Num() == 0)
		{
			FString CookedPath;
			appGetCookedContentPath(appGetPlatformType(), CookedPath);

			// open the proper metadata file
			FArchive* MetadataFile = GFileManager->CreateFileReader(*(CookedPath + (bIsEngineShader ? TEXT("EngineShadersInfo.bin") : TEXT("PreprocessedShadersInfo.bin"))));
			if (!MetadataFile)
			{
				// fail if the file didn't open
				return FALSE;
			}
			*MetadataFile << Metadata;
			delete MetadataFile;
		}

		// lookup the shader info
		QWORD* ShaderInfo = Metadata.Find(ShaderName);
		if (ShaderInfo == NULL)
		{
			// fail if it wasn't found
			return FALSE;
		}

		UINT Offset = (UINT)(*ShaderInfo >> 32);
		UINT Size = (UINT)(*ShaderInfo & 0xFFFFFFFF);

		// now read it in
		AllShadersFile->Seek(Offset);
		ANSICHAR* Source = (ANSICHAR*)appMalloc(Size);
		AllShadersFile->Serialize(Source, Size);
		OutSource = FString(Source);
		appFree(Source);

		return TRUE;
	}

	/**
	 * Prepare for loading the preprocessed shaders
	 */
	static void StartLoadingPreprocessedShaderInfos()
	{
		// do a fake read to make sure the PreprocessedShadersInfo is read in
		FString Str;
		LoadShaderFromAllShaders(TEXT("fake"), FALSE, Str);
	}

	/**
	 * Toss the PreprocessedShaderInfos map, when we are done loading all preprocessed shaders
	 */
	static void StopLoadingPreprocessedShaderInfos()
	{
		PreprocessedShadersInfo.Empty();
	}
	
	/**
	 * Compile a new instance of the program
	 *
	 * @param Instance the instance to compile and fill out
	 * @return Canonical, or master key maybe be different from ProgramKey is this is mapping to another shader
	 */
	FProgramKey InitNewInstance(FProgInstance& Instance, const FProgramKey& ProgramKey, /*const*/ FProgramKeyData& KeyData, UBOOL& bSuccess, DWORD TextureUsageFlags) 
	{
		bSuccess = FALSE;
		STAT(DOUBLE MaterialCompilingTime = 0);
		{
			SCOPE_SECONDS_COUNTER(MaterialCompilingTime);
			// Determine if this program is already compiled
			FProgramKey* MasterProgramKey = KeyToMasterProgramKey.Find(ProgramKey);
			if (MasterProgramKey != NULL)
			{
				bSuccess = TRUE;
				return *MasterProgramKey;
			}

			// Determine if the vertex shader is equivalent to one already compiled
			UBOOL bVertexShaderIsReady = FALSE;
			FProgramKey* VertexMasterKey = NULL;
			if (GSystemSettings.bShareVertexShaders || GSystemSettings.bShareShaderPrograms)
			{
				VertexMasterKey = GMobileShaderInitialization.VertexKeyToEquivalentMasterKey.Find(ProgramKey);
				if( GSystemSettings.bShareVertexShaders &&
					VertexMasterKey &&
					GMobileShaderInitialization.VertexMasterKeyToGLShader.Find(*VertexMasterKey) )
				{
					Instance.VertexShaderHandle = GMobileShaderInitialization.VertexMasterKeyToGLShader.FindRef(*VertexMasterKey);
					bVertexShaderIsReady = TRUE;
					INC_DWORD_STAT(STAT_ES2ShaderCacheVSAvoided);
				}
			}
			// Determine if the pixel shader is equivalent to one already compiled
			UBOOL bPixelShaderIsReady = FALSE;
			FProgramKey* PixelMasterKey = NULL;
			if (GSystemSettings.bSharePixelShaders || GSystemSettings.bShareShaderPrograms)
			{
				PixelMasterKey = GMobileShaderInitialization.PixelKeyToEquivalentMasterKey.Find(ProgramKey);
				if( GSystemSettings.bSharePixelShaders && PixelMasterKey)
				{
					// look up the key in the ProgMap (@todo flash: do the same on vertex!!)
					GLuint* GLShader = GMobileShaderInitialization.GetPixelShaderFromPixelMasterKey(*PixelMasterKey, TextureUsageFlags);
					if (GLShader)
					{
						Instance.PixelShaderHandle = *GLShader;
						bPixelShaderIsReady = TRUE;
						INC_DWORD_STAT(STAT_ES2ShaderCachePSAvoided);
					}
				}
			}
			// Determine if this entire program is equivalent to one already compiled
			if( GSystemSettings.bShareShaderPrograms &&
				bVertexShaderIsReady &&
				bPixelShaderIsReady  )
			{
				// Lets see if we have something compatible already linked and ready to go
				FProgramKey* Existing = MasterKeyPairToMasterProgram.Find(VertexPixelKeyPair(*VertexMasterKey,*PixelMasterKey));
				if (Existing)
				{
					KeyToMasterProgramKey.Set(ProgramKey,*Existing);
					INC_DWORD_STAT(STAT_ES2ShaderCacheProgramsAvoided);
					bSuccess = TRUE;

					return *Existing;
				}
			}

			// create a program object for each possible program variation
			Instance.Program = GLCHECK(glCreateProgram());
			if (GShaderManager.IsCurrentPrimitiveTracked())
			{
				INC_DWORD_STAT(STAT_ShaderProgramCount);
			}
			
			if (!bVertexShaderIsReady || !bPixelShaderIsReady)
			{
				FString VertexShaderText;
				FString PixelShaderText;

				// NOTE: We never load cache shaders when running with uncooked content as only the cooker can
				// generate the full shader set.  Otherwise we could be loading stale shader files!
				if (GUseSeekFreeLoading && GSystemSettings.bLoadCPreprocessedShaders)
				{
					// only try for preprocessed shaders if are either currently looping over all the preprocessed
					// shaders, or we didn't loop over all the shaders so we can load preprocessed at any time
					if (GSystemSettings.bUsePreprocessedShaders || PreprocessedShadersInfo.Num())
					{
						FString VertexShaderKey = FString::Printf(TEXT("%s\\%s.msf.i"), *ProgramKey.ToString(), *VertexShaderName);
						FString PixelShaderKey = FString::Printf(TEXT("%s\\%s.msf.i"), *ProgramKey.ToString(), *PixelShaderName);

						// load the shaders
						if (!LoadShaderFromAllShaders(VertexShaderKey, FALSE, VertexShaderText) && 
							!GSystemSettings.bCachePreprocessedShaders )
						{
#if !FINAL_RELEASE || WITH_EDITOR
							warnf(NAME_Warning, TEXT("Failed to load preprocessed source for '%s' requested by %s (Key %s)"),*VertexShaderName, *GShaderManager.MaterialName, *ProgramKey.ToString());
#else
							warnf(NAME_Warning, TEXT("Failed to load preprocessed source for '%s' requested by material (Key %s)"),*VertexShaderName, *ProgramKey.ToString());
#endif
						}

						if (!LoadShaderFromAllShaders(PixelShaderKey, FALSE, PixelShaderText) &&
							!GSystemSettings.bCachePreprocessedShaders)
						{
							warnf(NAME_Warning, TEXT("Failed to load preprocessed source for '%s'"),*PixelShaderName);
						}
					}

#if ANDROID
					// On Android we need to define ALPHAKILL at runtime due to some GPUs not supporting the discard instruction
					if (PixelShaderText.Len() > 0)
					{
						if ((KeyData.GetFieldValue(FProgramKeyData::PKDT_BlendMode) != BLEND_Masked) && (ShadedPrimitiveType != EPT_DistanceFieldFont))
						{
							PixelShaderText = FString::Printf( TEXT("#define ALPHAKILL(Alpha) ;\r\n%s"), *PixelShaderText);
						}
						else
						{
							if (GMobileAllowShaderDiscard)
							{
								PixelShaderText = FString::Printf( TEXT("#define ALPHAKILL(Alpha) if (Alpha <= AlphaTestRef) { discard; }\r\nuniform lowp float AlphaTestRef;\r\n%s"), *PixelShaderText);
							}
							else
							{
								PixelShaderText = FString::Printf( TEXT("#define ALPHAKILL(Alpha) if (Alpha <= AlphaTestRef) { Alpha = 0.0; }\r\nuniform lowp float AlphaTestRef;\r\n%s"), *PixelShaderText);
							}
						}
					}
#endif
				}

				UBOOL bReloadPrefixFiles = FALSE;

LabelRestart:
				if (VertexShaderText.Len() == 0 || PixelShaderText.Len() == 0)
				{
					if (bReloadPrefixFiles)
					{
						LoadShaderFromAllShaders("Prefix_Common.msf", TRUE, GShaderManager.CommonShaderPrefixFile);
						LoadShaderFromAllShaders("Prefix_VertexShader.msf", TRUE, GShaderManager.VertexShaderPrefixFile);
						LoadShaderFromAllShaders("Prefix_PixelShader.msf", TRUE, GShaderManager.PixelShaderPrefixFile);
					}

					// Set whether we're compiling the shader for PC OpenGL ES2 or for a device's native GL renderer.
					// This may affect the actual shader code that is passed to the GL ES2 implementation.
#if CONSOLE
					const UBOOL bIsCompilingForPC = FALSE;
#else
					const UBOOL bIsCompilingForPC = TRUE;
#endif

					FString VertexBody;
					FString PixelBody;

					const EMobileGlobalShaderType GlobalShaderType = (EMobileGlobalShaderType)KeyData.GetFieldValue(FProgramKeyData::PKDT_GlobalShaderType);

					if ((GlobalShaderType != EGST_None) && (ShadedPrimitiveType != EPT_GlobalShader))
					{
						ShadedPrimitiveType = EPT_GlobalShader;
						warnf(TEXT("GlobalShader has never been initialized."));
					}

					LoadShaderFromAllShaders(*(GetES2ShaderFilename(ShadedPrimitiveType,GlobalShaderType,SF_Vertex) + TEXT(".msf")), TRUE, VertexBody);
					LoadShaderFromAllShaders(*(GetES2ShaderFilename(ShadedPrimitiveType,GlobalShaderType,SF_Pixel) + TEXT(".msf")), TRUE, PixelBody);

					UBOOL bSuccess = ES2ShaderSourceFromKeys( 
						appGetPlatformType(),
						ProgramKey, 
						KeyData, 
						ShadedPrimitiveType, 
						bIsCompilingForPC,
						GShaderManager.CommonShaderPrefixFile,
						GShaderManager.VertexShaderPrefixFile,
						GShaderManager.PixelShaderPrefixFile,
						GSystemSettings.MobileLODBias,
						GSystemSettings.MobileBoneCount,
						GSystemSettings.MobileBoneWeightCount,
						*VertexBody,
						*PixelBody,
						VertexShaderText,
						PixelShaderText
						);

					if (!bSuccess)
					{
						warnf(NAME_Warning, TEXT("Failed to generate shader text."));
					}
					else
					{
#if !FINAL_RELEASE || WITH_EDITOR
						FString KeyGeneratedFromSource = FString::Printf( TEXT("Key %s was not preprocessed. This has been compiled from source for material %s"), *ProgramKey.ToString(), (GlobalShaderType == EGST_None) ? *GShaderManager.MaterialName : TEXT("Global Shader") );
#else
						FString KeyGeneratedFromSource = FString::Printf( TEXT("Key %s was not preprocessed. This has been compiled from source"), *ProgramKey.ToString() );
#endif

						GShaderManager.MissingPreprocessedShaders.AddItem( KeyGeneratedFromSource );
					}
				}
				// Create a vertex shader object, if we don't have one already
				STAT(DOUBLE VertexCompileTime = 0);
				if (!bVertexShaderIsReady)
				{
					SCOPE_SECONDS_COUNTER(VertexCompileTime);
					GLuint Shader;
					Shader = GLCHECK(glCreateShader(GL_VERTEX_SHADER));

#if _WINDOWS
					// Strip out the precision qualifiers when emulating on desktop
					FString ReducedVertexShaderText = VertexShaderText;
					ReducedVertexShaderText = ReducedVertexShaderText.Replace( TEXT("lowp "), TEXT("") );
					ReducedVertexShaderText = ReducedVertexShaderText.Replace( TEXT("mediump "), TEXT("") );
					ReducedVertexShaderText = ReducedVertexShaderText.Replace( TEXT("highp "), TEXT("") );
#else				
					const FString& ReducedVertexShaderText = VertexShaderText;
#endif
					ANSICHAR* PreprocessedSource[1]={0};
					PreprocessedSource[0] = (ANSICHAR*)appMalloc(ReducedVertexShaderText.Len() + 1);
					appStrcpyANSI(PreprocessedSource[0], ReducedVertexShaderText.Len() + 1, TCHAR_TO_ANSI(*ReducedVertexShaderText));

					// set the source files (prefix + code)
					GLCHECK(glShaderSource(Shader, 1, (const ANSICHAR**)PreprocessedSource, NULL));
					appFree(PreprocessedSource[0]);

					// compile it and check for errors
					GLCHECK(glCompileShader(Shader));

					if (PrintInfoLog(Shader, *FString::Printf(TEXT("%s - %s"), *VertexShaderName, *ProgramKey.ToString())) == ART_Yes)
					{
						// retry on failure
						VertexShaderText = TEXT("");
						bReloadPrefixFiles = TRUE;
						goto LabelRestart;
					}
#if _WINDOWS
					if( GSystemSettings.bCachePreprocessedShaders ||
						GSystemSettings.bProfilePreprocessedShaders )
					{
						CacheShaderAndGenerateStats( ProgramKey, VertexShaderText, VertexShaderName, SF_Vertex );
					}
#endif
					Instance.VertexShaderHandle = Shader;
					if( VertexMasterKey )
					{
						GMobileShaderInitialization.VertexMasterKeyToGLShader.Set(*VertexMasterKey, Instance.VertexShaderHandle);
					}
				}
				INC_FLOAT_STAT_BY(STAT_ES2VertexProgramCompileTime,(FLOAT)VertexCompileTime);
				STAT(DOUBLE PixelCompileTime = 0);
				if (!bPixelShaderIsReady)
				{
					SCOPE_SECONDS_COUNTER(PixelCompileTime);
					GLuint Shader;
					Shader = GLCHECK(glCreateShader(GL_FRAGMENT_SHADER));

#if _WINDOWS
					// Strip out the precision qualifiers when emulating on desktop
					FString ReducedPixelShaderText = PixelShaderText;
					ReducedPixelShaderText = ReducedPixelShaderText.Replace( TEXT("lowp "), TEXT("") );
					ReducedPixelShaderText = ReducedPixelShaderText.Replace( TEXT("mediump "), TEXT("") );
					ReducedPixelShaderText = ReducedPixelShaderText.Replace( TEXT("highp "), TEXT("") );
#else
#if FLASH
					// replace the format_0 type tags with the actual format in current use
					for (INT Slot=0; Slot < MAX_Mapped_MobileTexture; Slot++)
					{
						FString Tag = FString::Printf(TEXT("format_%d"), Slot);
						FString Format = (TextureUsageFlags & (1 << Slot)) ? TEXT("dxt5") : TEXT("dxt1");

						PixelShaderText = PixelShaderText.Replace(*Tag, *Format);
					}
#endif
					const FString& ReducedPixelShaderText = PixelShaderText;
#endif
					ANSICHAR* PreprocessedSource[1]={0};
					PreprocessedSource[0] = (ANSICHAR*)appMalloc(ReducedPixelShaderText.Len() + 1);
					appStrcpyANSI(PreprocessedSource[0], ReducedPixelShaderText.Len() + 1, TCHAR_TO_ANSI(*ReducedPixelShaderText));

					// set the source files (prefix + code)
					GLCHECK(glShaderSource(Shader, 1, (const ANSICHAR**)PreprocessedSource, NULL));
					appFree(PreprocessedSource[0]);

					// compile it and check for errors
					GLCHECK(glCompileShader(Shader));

					if (PrintInfoLog(Shader, *FString::Printf(TEXT("%s - %s"), *PixelShaderName, *ProgramKey.ToString())) == ART_Yes)
					{
						// retry on failure
						PixelShaderText = TEXT("");
						bReloadPrefixFiles = TRUE;
						goto LabelRestart;
					}
#if _WINDOWS
					if( GSystemSettings.bCachePreprocessedShaders ||
						GSystemSettings.bProfilePreprocessedShaders )
					{
						CacheShaderAndGenerateStats( ProgramKey, PixelShaderText, PixelShaderName, SF_Pixel );
					}
#endif
					Instance.PixelShaderHandle = Shader;

					if( PixelMasterKey )
					{
						GMobileShaderInitialization.SetPixelShaderForPixelMasterKey(*PixelMasterKey, TextureUsageFlags, Instance.PixelShaderHandle);
					}				
				}
				INC_FLOAT_STAT_BY(STAT_ES2PixelProgramCompileTime,(FLOAT)PixelCompileTime);
			}

			// attach the shader to the program
			GLCHECK(glAttachShader(Instance.Program, Instance.PixelShaderHandle));
			GLCHECK(glAttachShader(Instance.Program, Instance.VertexShaderHandle));

			// Finally, link the program and bind all the uniforms
			STAT(DOUBLE ProgramLinkTime = 0);
			{
				SCOPE_SECONDS_COUNTER(ProgramLinkTime);
				LinkProgramAndBindUniforms( Instance, ProgramKey );
			}
			if (VertexMasterKey && PixelMasterKey)
			{
				MasterKeyPairToMasterProgram.Set(VertexPixelKeyPair(*VertexMasterKey,*PixelMasterKey),ProgramKey);
			}

			INC_FLOAT_STAT_BY(STAT_ES2ProgramLinkTime,(FLOAT)ProgramLinkTime);
		}
		INC_FLOAT_STAT_BY(STAT_ES2ShaderCacheCompileTime,(FLOAT)MaterialCompilingTime);

#if _WINDOWS
		// If we're caching shaders, re-cache after each new shader program is added to make sure
		// we don't miss anything in the event of a crash or abnormal program termination
		if( GSystemSettings.bCachePreprocessedShaders )
		{
			SaveCachedProgramKeys();
		}
#endif
		bSuccess = TRUE;
		return ProgramKey; // we built a new shader	
	}


	/**
	 * Destroys an instance instance of a program
	 *
	 * @param Instance the instance to compile and fill out
	 */
	void DestroyInstance( FProgInstance& Instance ) 
	{
		// Detach the shaders and delete them
		GLCHECK(glDetachShader( Instance.Program, Instance.VertexShaderHandle ));
		GLCHECK(glDeleteShader( Instance.VertexShaderHandle ));
		Instance.VertexShaderHandle = 0;
		GLCHECK(glDetachShader( Instance.Program, Instance.PixelShaderHandle ));
		GLCHECK(glDeleteShader( Instance.PixelShaderHandle ));
		Instance.PixelShaderHandle = 0;

		// Now delete the program itself
		GLCHECK(glDeleteProgram( Instance.Program ));
		DEC_DWORD_STAT(STAT_ShaderProgramCount);
		Instance.Program = 0;
		for( INT UniformIndex = 0; UniformIndex < Instance.NumUniforms; UniformIndex++ )
		{
			if( Instance.Uniforms[UniformIndex].Data )
			{
				appFree(Instance.Uniforms[UniformIndex].Data);
				Instance.Uniforms[UniformIndex].Data = 0;
			}
		}
	}


	/** Discard all instances of the shader.  Should be used for debugging only. */
	void DestroyAllInstances()
	{
#if !FINAL_RELEASE && !FLASH // @todo flash: Harder with Flash, skipping for now
		for( TMap<FProgramKey, FProgInstance*>::TIterator ProgIt(ProgMap); ProgIt; ++ProgIt)
		{
			DestroyInstance(*ProgIt.Value());
			delete ProgIt.Value();
		}
		ProgMap.Empty();

		// Clear current program
		CurrentProgram = 0;
		CurrentProgInstance = NULL;
		NextProgInstance = NULL;
#endif // #if !FINAL_RELEASE
	}

	
	/**
	 * Makes one of the internal programs the current program, and sets it in OpenGL
	 */
	UBOOL UpdateCurrentProgram()
	{
		UBOOL bShaderChanged = FALSE;

		CurrentProgInstance = NextProgInstance ? NextProgInstance : GetCurrentInstance();
		if( CurrentProgInstance != NULL &&
			CurrentProgInstance->Program != CurrentProgram )
		{
			if (GShaderManager.IsCurrentPrimitiveTracked())
			{
				INC_DWORD_STAT(STAT_ShaderProgramChanges);
			}
			CurrentProgram = CurrentProgInstance->Program;
			GLCHECK(glUseProgram(CurrentProgInstance->Program));
			GCurrentProgramUsedAttribMask = CurrentProgInstance->UsedAttribMask;
			GCurrentProgramUsedAttribMapping = &CurrentProgInstance->UsedAttribMapping[0];
			bShaderChanged = TRUE;
		}

		return bShaderChanged;
	}
	
	/**
	 * Refresh any uniforms in the program to make up to date with globally set parameters
	 */
	void UpdateCurrentUniforms(const UBOOL bShaderChanged) 
	{
		if( CurrentProgInstance == NULL )
		{
			// Nothing to do here
			return;
		}

		for ( INT UniformIndex=0; UniformIndex < CurrentProgInstance->NumUniforms; ++UniformIndex )
		{
			FES2ShaderProgram::FProgInstance::UniformShadowData& UniformData = CurrentProgInstance->Uniforms[ UniformIndex ];
			const FVersionedShaderParameter& Param = GShaderManager.VersionedShaderParameters[ UniformData.Slot ];

			if ( Param.Count <= 0 )
			{
				// Make sure this shader constant has been set already.  If this asset triggers, it means that a shader
				// parameter was not set by the engine before we tried to draw a primitive that needs it.
//				checkfSlowish( Param.Count > 0, TEXT("Uniform parameter was not set before rendering: %s"), *StandardUniformSlotInfo[UniformData.Slot].UE3Name.ToString() );
			}

			if ( Param.Version != UniformData.Version )
			{
				UniformData.Version = Param.Version;

				switch ( Param.Setter ) 
				{
					case US_Uniform1iSetter:
					{
#if ES2_SHADOW_UNIFORMS
						// Special case, only a single integer value, easy to check
						if( *((GLint*)UniformData.Data) != *((GLint*)Param.Data) )
						{
							// Update our shadow of the uniform value
							*((GLint*)UniformData.Data) = *((GLint*)Param.Data);

							// Send the update
							GLCHECK( glUniform1i( UniformData.UniformLocation, *(GLint*)Param.Data ) );
							if (GShaderManager.IsCurrentPrimitiveTracked())
							{
								INC_DWORD_STAT_BY(STAT_ShaderUniformUpdates, sizeof(GLint));
							}
						}
#else
 						// Send the update
 						GLCHECK( glUniform1i( UniformData.UniformLocation, *(GLint*)Param.Data ) );
#endif
						break;
					}
					case US_Uniform1fvSetter:
					{
#if ES2_SHADOW_UNIFORMS
						UBOOL bSendUpdate = TRUE;
						// Try to optimize the common case
						const GLint ParamCount = Param.Count;
						const GLint ParamBytes = sizeof(GLfloat) * ParamCount;
						const GLfloat* ParamData = (GLfloat*)Param.Data;
						GLfloat* ProgInstanceUniformData = (GLfloat*)UniformData.Data;
						if( (ParamCount == 1) && (!bShaderChanged) )
						{
							// If the uniform value hasn't changed, we can avoid the redundant update
							if( ProgInstanceUniformData[0] == ParamData[0] )
							{
								bSendUpdate = FALSE;
							}
						}
						if( bSendUpdate )
						{
							// Update our shadow of the uniform value
							if( ParamCount == 1 )
							{
								ProgInstanceUniformData[0] = ParamData[0];
							}
							else
							{
								appMemcpy( UniformData.Data, Param.Data, ParamBytes );
							}

							// Send the update
							GLCHECK( glUniform1fv( UniformData.UniformLocation, ParamCount, (const GLfloat*)Param.Data ) );
							if (GShaderManager.IsCurrentPrimitiveTracked())
							{
								INC_DWORD_STAT_BY(STAT_ShaderUniformUpdates, ParamBytes);
							}
						}
#else
 						// Send the update
 						GLCHECK( glUniform1fv( UniformData.UniformLocation, Param.Count, (const GLfloat*)Param.Data ) );
#endif
						break;
					}
					case US_Uniform2fvSetter:
					{
#if ES2_SHADOW_UNIFORMS
						UBOOL bSendUpdate = TRUE;
						// Try to optimize the common case
						const GLint ParamCount = Param.Count;
						const GLint ParamBytes = 2 * sizeof(GLfloat) * ParamCount;
						const GLfloat* ParamData = (GLfloat*)Param.Data;
						GLfloat* ProgInstanceUniformData = (GLfloat*)UniformData.Data;
						if( (ParamCount == 1) && (!bShaderChanged) )
						{
							// If the uniform value hasn't changed, we can avoid the redundant update
							if( ProgInstanceUniformData[0] == ParamData[0] &&
								ProgInstanceUniformData[1] == ParamData[1] )
							{
								bSendUpdate = FALSE;
							}
						}
						if( bSendUpdate )
						{
							// Update our shadow of the uniform value
							if( ParamCount == 1 )
							{
								ProgInstanceUniformData[0] = ParamData[0];
								ProgInstanceUniformData[1] = ParamData[1];
							}
							else
							{
								appMemcpy( UniformData.Data, Param.Data, ParamBytes );
							}

							// Send the update
							GLCHECK( glUniform2fv( UniformData.UniformLocation, ParamCount, (const GLfloat*)Param.Data ) );
							if (GShaderManager.IsCurrentPrimitiveTracked())
							{
								INC_DWORD_STAT_BY(STAT_ShaderUniformUpdates, ParamBytes);
							}
						}
#else
						GLCHECK( glUniform2fv( UniformData.UniformLocation, Param.Count, (const GLfloat*)Param.Data ) );
#endif
						break;
					}
					case US_Uniform3fvSetter:
					{
#if ES2_SHADOW_UNIFORMS
						UBOOL bSendUpdate = TRUE;
						// Try to optimize the common case
						const GLint ParamCount = Param.Count;
						const GLint ParamBytes = 3 * sizeof(GLfloat) * ParamCount;
						const GLfloat* ParamData = (GLfloat*)Param.Data;
						GLfloat* ProgInstanceUniformData = (GLfloat*)UniformData.Data;
						if( (ParamCount == 1) && (!bShaderChanged) )
						{
							// If the uniform value hasn't changed, we can avoid the redundant update
							if( ProgInstanceUniformData[0] == ParamData[0] &&
								ProgInstanceUniformData[1] == ParamData[1] &&
								ProgInstanceUniformData[2] == ParamData[2] )
							{
								bSendUpdate = FALSE;
							}
						}
						if( bSendUpdate )
						{
							// Update our shadow of the uniform value
							if( ParamCount == 1 )
							{
								ProgInstanceUniformData[0] = ParamData[0];
								ProgInstanceUniformData[1] = ParamData[1];
								ProgInstanceUniformData[2] = ParamData[2];
							}
							else
							{
								appMemcpy( UniformData.Data, Param.Data, ParamBytes );
							}

							// Send the update
							GLCHECK( glUniform3fv( UniformData.UniformLocation, ParamCount, (const GLfloat*)Param.Data ) );
							if (GShaderManager.IsCurrentPrimitiveTracked())
							{
								INC_DWORD_STAT_BY(STAT_ShaderUniformUpdates, ParamBytes);
							}
						}
#else
						GLCHECK( glUniform3fv( UniformData.UniformLocation, Param.Count, (const GLfloat*)Param.Data ) );
#endif
						break;
					}
					case US_Uniform4fvSetter:
					{
#if ES2_SHADOW_UNIFORMS
						UBOOL bSendUpdate = TRUE;
						// Try to optimize the common case
						const GLint ParamCount = Param.Count;
						const GLint ParamBytes = 4 * sizeof(GLfloat) * ParamCount;
						const GLfloat* ParamData = (GLfloat*)Param.Data;
						GLfloat* ProgInstanceUniformData = (GLfloat*)UniformData.Data;
						if (!bShaderChanged)
						{
							if( ParamCount == 1 )
							{
								// If the uniform value hasn't changed, we can avoid the redundant update
								if( ProgInstanceUniformData[0] == ParamData[0] &&
									ProgInstanceUniformData[1] == ParamData[1] &&
									ProgInstanceUniformData[2] == ParamData[2] &&
									ProgInstanceUniformData[3] == ParamData[3] )
								{
									bSendUpdate = FALSE;
								}
							}
							else if( ParamCount == 2 )
							{
								// If the uniform value hasn't changed, we can avoid the redundant update
								if( ProgInstanceUniformData[0] == ParamData[0] &&
									ProgInstanceUniformData[1] == ParamData[1] &&
									ProgInstanceUniformData[2] == ParamData[2] &&
									ProgInstanceUniformData[3] == ParamData[3] &&

									ProgInstanceUniformData[4] == ParamData[4] &&
									ProgInstanceUniformData[5] == ParamData[5] &&
									ProgInstanceUniformData[6] == ParamData[6] &&
									ProgInstanceUniformData[7] == ParamData[7] )
								{
									bSendUpdate = FALSE;
								}
							}
						}
						else
						{
							// Don't cache if there's more data than the "optimized" cases
						}
						if( bSendUpdate )
						{
							// Update our shadow of the uniform value
							if( ParamCount == 1 )
							{
								ProgInstanceUniformData[0] = ParamData[0];
								ProgInstanceUniformData[1] = ParamData[1];
								ProgInstanceUniformData[2] = ParamData[2];
								ProgInstanceUniformData[3] = ParamData[3];
							}
							else if( ParamCount == 2 )
							{
								ProgInstanceUniformData[0] = ParamData[0];
								ProgInstanceUniformData[1] = ParamData[1];
								ProgInstanceUniformData[2] = ParamData[2];
								ProgInstanceUniformData[3] = ParamData[3];

								ProgInstanceUniformData[4] = ParamData[4];
								ProgInstanceUniformData[5] = ParamData[5];
								ProgInstanceUniformData[6] = ParamData[6];
								ProgInstanceUniformData[7] = ParamData[7];
							}
							else
							{
								// No need to cache if we're not going to check it
							}

							// Send the update
 							GLCHECK( glUniform4fv( UniformData.UniformLocation, ParamCount, (const GLfloat*)Param.Data ) );
							if (GShaderManager.IsCurrentPrimitiveTracked())
							{
								INC_DWORD_STAT_BY(STAT_ShaderUniformUpdates, ParamBytes);
							}
						}
#else
						GLCHECK( glUniform4fv( UniformData.UniformLocation, Param.Count, (const GLfloat*)Param.Data ) );
#endif
						break;
					}
					case US_UniformMatrix3fvSetter:
					{
#if ES2_SHADOW_UNIFORMS
						UBOOL bSendUpdate = TRUE;
						// Try to optimize the common case
						const GLint ParamCount = Param.Count;
						const GLint ParamBytes = 9 * sizeof(GLfloat) * ParamCount;
						const GLfloat* ParamData = (GLfloat*)Param.Data;
						GLfloat* ProgInstanceUniformData = (GLfloat*)UniformData.Data;
						if( (ParamCount == 1) && (!bShaderChanged) )
						{
							// If the uniform value hasn't changed, we can avoid the redundant update
							if( ProgInstanceUniformData[0] == ParamData[0] &&
								ProgInstanceUniformData[1] == ParamData[1] &&
								ProgInstanceUniformData[2] == ParamData[2] &&

								ProgInstanceUniformData[3] == ParamData[3] &&
								ProgInstanceUniformData[4] == ParamData[4] &&
								ProgInstanceUniformData[5] == ParamData[5] &&

								ProgInstanceUniformData[6] == ParamData[6] &&
								ProgInstanceUniformData[7] == ParamData[7] &&
								ProgInstanceUniformData[8] == ParamData[8] )
							{
								bSendUpdate = FALSE;
							}
						}
						if( bSendUpdate )
						{
							// Update our shadow of the uniform value
							if( ParamCount == 1 )
							{
								ProgInstanceUniformData[0] = ParamData[0];
								ProgInstanceUniformData[1] = ParamData[1];
								ProgInstanceUniformData[2] = ParamData[2];

								ProgInstanceUniformData[3] = ParamData[3];
								ProgInstanceUniformData[4] = ParamData[4];
								ProgInstanceUniformData[5] = ParamData[5];

								ProgInstanceUniformData[6] = ParamData[6];
								ProgInstanceUniformData[7] = ParamData[7];
								ProgInstanceUniformData[8] = ParamData[8];
							}
							else
							{
								appMemcpy( UniformData.Data, Param.Data, ParamBytes );
							}

							// Send the update
							GLCHECK( glUniformMatrix3fv( UniformData.UniformLocation, ParamCount, FALSE, (const GLfloat*)Param.Data ) );
							if (GShaderManager.IsCurrentPrimitiveTracked())
							{
								INC_DWORD_STAT_BY(STAT_ShaderUniformUpdates, ParamBytes);
							}
						}
#else
						GLCHECK( glUniformMatrix3fv( UniformData.UniformLocation, Param.Count, GL_FALSE, (const GLfloat*)Param.Data ) );
#endif
						break;
					}
					case US_UniformMatrix4fvSetter:
					{
#if ES2_SHADOW_UNIFORMS
						UBOOL bSendUpdate = TRUE;
						// Try to optimize the common case
						const GLint ParamCount = Param.Count;
						const GLint ParamBytes = 16 * sizeof(GLfloat) * ParamCount;
						const GLfloat* ParamData = (GLfloat*)Param.Data;
						GLfloat* ProgInstanceUniformData = (GLfloat*)UniformData.Data;
						if( (ParamCount == 1) && (!bShaderChanged) )
						{
							// If the uniform value hasn't changed, we can avoid the redundant update
							if( ProgInstanceUniformData[0] == ParamData[0] &&
								ProgInstanceUniformData[1] == ParamData[1] &&
								ProgInstanceUniformData[2] == ParamData[2] &&
								ProgInstanceUniformData[3] == ParamData[3] &&

								ProgInstanceUniformData[4] == ParamData[4] &&
								ProgInstanceUniformData[5] == ParamData[5] &&
								ProgInstanceUniformData[6] == ParamData[6] &&
								ProgInstanceUniformData[7] == ParamData[7] &&

								ProgInstanceUniformData[8] == ParamData[8] &&
								ProgInstanceUniformData[9] == ParamData[9] &&
								ProgInstanceUniformData[10] == ParamData[10] &&
								ProgInstanceUniformData[11] == ParamData[11] &&

								ProgInstanceUniformData[12] == ParamData[12] &&
								ProgInstanceUniformData[13] == ParamData[13] &&
								ProgInstanceUniformData[14] == ParamData[14] &&
								ProgInstanceUniformData[15] == ParamData[15] )
							{
								bSendUpdate = FALSE;
							}
						}
						if( bSendUpdate )
						{
							// Update our shadow of the uniform value
							if( ParamCount == 1 )
							{
								ProgInstanceUniformData[0] = ParamData[0];
								ProgInstanceUniformData[1] = ParamData[1];
								ProgInstanceUniformData[2] = ParamData[2];
								ProgInstanceUniformData[3] = ParamData[3];

								ProgInstanceUniformData[4] = ParamData[4];
								ProgInstanceUniformData[5] = ParamData[5];
								ProgInstanceUniformData[6] = ParamData[6];
								ProgInstanceUniformData[7] = ParamData[7];

								ProgInstanceUniformData[8] = ParamData[8];
								ProgInstanceUniformData[9] = ParamData[9];
								ProgInstanceUniformData[10] = ParamData[10];
								ProgInstanceUniformData[11] = ParamData[11];

								ProgInstanceUniformData[12] = ParamData[12];
								ProgInstanceUniformData[13] = ParamData[13];
								ProgInstanceUniformData[14] = ParamData[14];
								ProgInstanceUniformData[15] = ParamData[15];
							}
							else
							{
								appMemcpy( UniformData.Data, Param.Data, ParamBytes );
							}

							// Send the update
							GLCHECK( glUniformMatrix4fv( UniformData.UniformLocation, ParamCount, GL_FALSE, (const GLfloat*)Param.Data ) );
							if (GShaderManager.IsCurrentPrimitiveTracked())
							{
								INC_DWORD_STAT_BY(STAT_ShaderUniformUpdates, ParamBytes);
							}
						}
#else
						GLCHECK( glUniformMatrix4fv( UniformData.UniformLocation, Param.Count, GL_FALSE, (const GLfloat*)Param.Data ) );
#endif
						break;
					}
					default:
						break;
				}
			}
		}
	}
};

TMap<FString, QWORD> FES2ShaderProgram::EngineShadersInfo;
TMap<FString, QWORD> FES2ShaderProgram::PreprocessedShadersInfo;
FArchive* FES2ShaderProgram::AllShadersFile = NULL;
FES2ShaderProgram::FProgInstance* FES2ShaderProgram::CurrentProgInstance = NULL;
FES2ShaderProgram::FProgInstance* FES2ShaderProgram::NextProgInstance = NULL;
GLuint FES2ShaderProgram::CurrentProgram = 0;

void ES2StartLoadingPreprocessedShaderInfos()
{
	FES2ShaderProgram::StartLoadingPreprocessedShaderInfos();
}

void ES2StopLoadingPreprocessedShaderInfos()
{
	FES2ShaderProgram::StopLoadingPreprocessedShaderInfos();
}

/**
 * Queue up parameter values to be set later (at render time)
 */
void Uniform1i( EStandardUniformSlot Slot, GLint IntVal ) 
{
	// Verify the uniform size in bytes
	const UINT SizeInBytes = sizeof(GLint);
	checkSlowish(sizeof(GLfloat) * StandardUniformSlotInfo[Slot].Size >= SizeInBytes);

	FVersionedShaderParameter& Param = GShaderManager.GetVersionedParameter(Slot);
	Param.Version++;
	Param.Setter = US_Uniform1iSetter;
	Param.Count = 1;
	*((GLint*)Param.Data) = IntVal;
}

void Uniform1fv( EStandardUniformSlot Slot, int Count, const GLfloat* FloatVals ) 
{
	// Verify the uniform size in bytes
	const UINT SizeInBytes = sizeof(GLfloat) * Count;
	checkSlowish(sizeof(GLfloat) * StandardUniformSlotInfo[Slot].Size >= SizeInBytes);

	FVersionedShaderParameter& Param = GShaderManager.GetVersionedParameter(Slot);
	Param.Version++;
	Param.Setter = US_Uniform1fvSetter;
	Param.Count = Count;
	if( Count == 1 )
	{
		GLfloat* ParamData = (GLfloat*)Param.Data;
		ParamData[0] = FloatVals[0];
	}
	else
	{
		appMemcpy( Param.Data, FloatVals, SizeInBytes );
	}
}

void Uniform2fv( EStandardUniformSlot Slot, int Count, const GLfloat* FloatVals ) 
{
	// Verify the uniform size in bytes
	const UINT SizeInBytes = 2 * sizeof(GLfloat) * Count;
	checkSlowish(sizeof(GLfloat) * StandardUniformSlotInfo[Slot].Size >= SizeInBytes);

	FVersionedShaderParameter& Param = GShaderManager.GetVersionedParameter(Slot);
	Param.Version++;
	Param.Setter = US_Uniform2fvSetter;
	Param.Count = Count;
	if( Count == 1 )
	{
		GLfloat* ParamData = (GLfloat*)Param.Data;
		ParamData[0] = FloatVals[0];
		ParamData[1] = FloatVals[1];
	}
	else
	{
		appMemcpy( Param.Data, FloatVals, SizeInBytes );
	}
}

void Uniform3fv( EStandardUniformSlot Slot, int Count, const GLfloat* FloatVals ) 
{
	// Verify the uniform size in bytes
	const UINT SizeInBytes = 3 * sizeof(GLfloat) * Count;
	checkSlowish(sizeof(GLfloat) * StandardUniformSlotInfo[Slot].Size >= SizeInBytes);

	FVersionedShaderParameter& Param = GShaderManager.GetVersionedParameter(Slot);
	Param.Version++;
	Param.Setter = US_Uniform3fvSetter;
	Param.Count = Count;
	if( Count == 1 )
	{
		GLfloat* ParamData = (GLfloat*)Param.Data;
		ParamData[0] = FloatVals[0];
		ParamData[1] = FloatVals[1];
		ParamData[2] = FloatVals[2];
	}
	else
	{
		appMemcpy( Param.Data, FloatVals, SizeInBytes );
	}
}

void Uniform4fv( EStandardUniformSlot Slot, int Count, const GLfloat* FloatVals ) 
{
	// Verify the uniform size in bytes
	const UINT SizeInBytes = 4 * sizeof(GLfloat) * Count;
	checkSlowish(sizeof(GLfloat) * StandardUniformSlotInfo[Slot].Size >= SizeInBytes);

	FVersionedShaderParameter& Param = GShaderManager.GetVersionedParameter(Slot);
	Param.Version++;
	Param.Setter = US_Uniform4fvSetter;
	Param.Count = Count;
	if( Count == 1 )
	{
		GLfloat* ParamData = (GLfloat*)Param.Data;
		ParamData[0] = FloatVals[0];
		ParamData[1] = FloatVals[1];
		ParamData[2] = FloatVals[2];
		ParamData[3] = FloatVals[3];
	}
	else if( Count == 2 )
	{
		GLfloat* ParamData = (GLfloat*)Param.Data;
		ParamData[0] = FloatVals[0];
		ParamData[1] = FloatVals[1];
		ParamData[2] = FloatVals[2];
		ParamData[3] = FloatVals[3];

		ParamData[4] = FloatVals[4];
		ParamData[5] = FloatVals[5];
		ParamData[6] = FloatVals[6];
		ParamData[7] = FloatVals[7];
	}
	else
	{
		appMemcpy( Param.Data, FloatVals, SizeInBytes );
	}
}

void UniformMatrix3fv( EStandardUniformSlot Slot, int Count, GLboolean Transpose, const GLfloat* FloatVals ) 
{
	// Verify the uniform size in bytes
	const UINT SizeInBytes = 9 * sizeof(GLfloat) * Count;
	checkSlowish(sizeof(GLfloat) * StandardUniformSlotInfo[Slot].Size >= SizeInBytes);

	FVersionedShaderParameter& Param = GShaderManager.GetVersionedParameter(Slot);
	Param.Version++;
	Param.Setter = US_UniformMatrix3fvSetter;
	Param.Count = Count;
	if( Count == 1 )
	{
		GLfloat* ParamData = (GLfloat*)Param.Data;
		ParamData[0] = FloatVals[0];
		ParamData[1] = FloatVals[1];
		ParamData[2] = FloatVals[2];

		ParamData[3] = FloatVals[3];
		ParamData[4] = FloatVals[4];
		ParamData[5] = FloatVals[5];

		ParamData[6] = FloatVals[6];
		ParamData[7] = FloatVals[7];
		ParamData[8] = FloatVals[8];
	}
	else
	{
		appMemcpy( Param.Data, FloatVals, SizeInBytes );
	}
}

void UniformMatrix4fv( EStandardUniformSlot Slot, int Count, GLboolean Transpose, const GLfloat* FloatVals ) 
{
	// Verify the uniform size in bytes
	const UINT SizeInBytes = 16 * sizeof(GLfloat) * Count;
	checkSlowish(sizeof(GLfloat) * StandardUniformSlotInfo[Slot].Size >= SizeInBytes);

	FVersionedShaderParameter& Param = GShaderManager.GetVersionedParameter(Slot);
	Param.Version++;
	Param.Setter = US_UniformMatrix4fvSetter;
	Param.Count = Count;
	if( Count == 1 )
	{
		GLfloat* ParamData = (GLfloat*)Param.Data;
		ParamData[0] = FloatVals[0];
		ParamData[1] = FloatVals[1];
		ParamData[2] = FloatVals[2];
		ParamData[3] = FloatVals[3];

		ParamData[4] = FloatVals[4];
		ParamData[5] = FloatVals[5];
		ParamData[6] = FloatVals[6];
		ParamData[7] = FloatVals[7];

		ParamData[8] = FloatVals[8];
		ParamData[9] = FloatVals[9];
		ParamData[10] = FloatVals[10];
		ParamData[11] = FloatVals[11];

		ParamData[12] = FloatVals[12];
		ParamData[13] = FloatVals[13];
		ParamData[14] = FloatVals[14];
		ParamData[15] = FloatVals[15];
	}
	else
	{
		appMemcpy( Param.Data, FloatVals, SizeInBytes );
	}
}




FPixelShaderRHIRef FES2RHI::CreatePixelShader(const TArray<BYTE>& Code) 
{ 
	return new FES2PixelShader;
} 

FVertexShaderRHIRef FES2RHI::CreateVertexShader(const TArray<BYTE>& Code) 
{ 
	return new FES2VertexShader;
} 

FBoundShaderStateRHIRef FES2RHI::CreateBoundShaderState(FVertexDeclarationRHIParamRef VertexDeclarationRHI, DWORD *StreamStrides, FVertexShaderRHIParamRef VertexShader, FPixelShaderRHIParamRef PixelShader, EMobileGlobalShaderType MobileGlobalShaderType)
{ 
	DYNAMIC_CAST_ES2RESOURCE( VertexDeclaration, VertexDeclaration );
	return new FES2BoundShaderState(VertexDeclaration, VertexShader, PixelShader, MobileGlobalShaderType);
} 



void FES2RHI::SetVertexShaderParameter(FVertexShaderRHIParamRef VertexShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex) 
{ 
	// Since we don't precompile shaders, we can only set parameters based on names instead of the BaseIndex like we would like to use
	// This involves manually setting the names on certain parameters we care about, and then looking up the name
	// of the parameter to determine which Slot to set the value into

	if (ParamIndex == -1)
	{
		// Nothing to do
		return;
	}
	else if (ParamIndex == SS_LocalToWorldSlot)
	{
 		UniformMatrix4fv(SS_LocalToWorldSlot, 1, GL_FALSE, (GLfloat *)NewValue);

		// Along with a LocalToWorld matrix, we also supply a LocalToWorldRotation 3x3 matrix with no scaling
		// that we'll use to rotate vectors.  This lets us avoid expensive normalizations in shaders.
		FMatrix LocalToWorldRotation = (*(FMatrix*)NewValue).InverseSafe().Transpose();
		LocalToWorldRotation.RemoveScaling();
		TMatrix< 3, 3 > LocalToWorldRotation3x3( LocalToWorldRotation );
		UniformMatrix3fv(SS_LocalToWorldRotationSlot, 1, GL_FALSE, (GLfloat *)&LocalToWorldRotation3x3.M);

		FMatrix LocalToProjection = (*(FMatrix*)NewValue) * GShaderManager.GetViewProjectionMatrix();
#if FLASH
		UniformMatrix4fv(SS_LocalToProjectionSlot, 1, GL_FALSE, (GLfloat *)&LocalToProjection);
#else
		FMatrix LocalToProjectionTranspose = LocalToProjection.Transpose();
		UniformMatrix4fv(SS_LocalToProjectionSlot, 1, GL_FALSE, (GLfloat *)&LocalToProjectionTranspose);
#endif
	}
//	else if (ParamIndex == SS_ProjectionMatrix)
//	{
//		// OpenGL uses [-1..1] space to clip all coordinates, while D3D uses [0..1] for Z,
//		// so we need to modify the ProjectionMatrix to get the correct result
//		FScaleMatrix ClipSpaceFixScale(FVector(1.0f, 1.0f, 2.0f));
//		FTranslationMatrix ClipSpaceFixTranslate(FVector(0.0f, 0.0f, -1.0f));	
//		const FMatrix FixedProjectionMatrix( (*(FMatrix*)NewValue) * ClipSpaceFixScale * ClipSpaceFixTranslate );
//
//		UniformMatrix4fv(SS_ProjectionMatrix, 1, GL_FALSE, (GLfloat *)&FixedProjectionMatrix);
//	}
	else if (ParamIndex == SS_Bones)
	{
		Uniform4fv(SS_Bones, Min<UINT>(NumBytes / 16, GSystemSettings.MobileBoneCount * 3), (GLfloat*)NewValue);
	}
	else if (StandardUniformSlotInfo[ParamIndex].Size == 16
#if WITH_GFx
        && ParamIndex != SS_TextureTransform2D
#endif
        )
	{
 		UniformMatrix4fv((EStandardUniformSlot)ParamIndex, 1, GL_FALSE, (GLfloat *)NewValue);
	}
#if WITH_GFx
    else if (ParamIndex == SS_BatchFloat)
    {
        Uniform4fv((EStandardUniformSlot)ParamIndex, NumBytes >> 4, (GLfloat*)NewValue);
    }
    else if (ParamIndex == SS_BatchMatrix)
    {
        UniformMatrix4fv((EStandardUniformSlot)ParamIndex, NumBytes >> 6, GL_FALSE, (GLfloat*)NewValue);
    }
#endif
	// look for non-multiples of 4
	else if (StandardUniformSlotInfo[ParamIndex].Size & 3)
	{
		switch (StandardUniformSlotInfo[ParamIndex].Size)
		{
		case 1:
			Uniform1fv((EStandardUniformSlot)ParamIndex, 1, (GLfloat*)NewValue); break;
		case 2:
			Uniform2fv((EStandardUniformSlot)ParamIndex, 1, (GLfloat*)NewValue); break;
		case 3:
			Uniform3fv((EStandardUniformSlot)ParamIndex, 1, (GLfloat*)NewValue); break;
		case 5:
			Uniform1fv((EStandardUniformSlot)ParamIndex, 5, (GLfloat*)NewValue); break;
		default:
			appErrorf(TEXT("Slot %d has an unhandled number of bytes [%d]"), ParamIndex, StandardUniformSlotInfo[ParamIndex].Size); break;
		}
	}
	else 
	{
		Uniform4fv((EStandardUniformSlot)ParamIndex, StandardUniformSlotInfo[ParamIndex].Size >> 2, (GLfloat*)NewValue);
	}
} 


void FES2RHI::SetPixelShaderParameter(FPixelShaderRHIParamRef PixelShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex) 
{ 
	if (ParamIndex == -1)
	{
		// Nothing to do
		return;
	}
	else if (ParamIndex == SS_UpperSkyColor)
	{
		GShaderManager.SetUpperSkyColor( *static_cast<const FLinearColor*>( NewValue ) );
	}
	else if (ParamIndex == SS_LowerSkyColor)
	{
		GShaderManager.SetLowerSkyColor( *static_cast<const FLinearColor*>( NewValue ) );
	}
	else if (ParamIndex == SS_ScreenToShadowMatrix 
#if WITH_GFx
        || ParamIndex == SS_ColorMatrix
#endif
        )
	{
		UniformMatrix4fv((EStandardUniformSlot)ParamIndex, 1, GL_FALSE, (const GLfloat*)NewValue);
	}
	// look for non-multiples of 4
	else if (StandardUniformSlotInfo[ParamIndex].Size & 3)
	{
		switch (StandardUniformSlotInfo[ParamIndex].Size)
		{
			case 1:
				Uniform1fv((EStandardUniformSlot)ParamIndex, 1, (GLfloat*)NewValue); break;
			case 2:
				Uniform2fv((EStandardUniformSlot)ParamIndex, 1, (GLfloat*)NewValue); break;
			case 3:
				Uniform3fv((EStandardUniformSlot)ParamIndex, 1, (GLfloat*)NewValue); break;
			case 5:
				Uniform1fv((EStandardUniformSlot)ParamIndex, 5, (GLfloat*)NewValue); break;
			default:
				appErrorf(TEXT("Slot %d has an unhandled number of bytes [%d]"), ParamIndex, StandardUniformSlotInfo[ParamIndex].Size); break;
		}
	}
	else
	{
		// it expects count in terms of float4s, hence >> 2
		Uniform4fv((EStandardUniformSlot)ParamIndex, StandardUniformSlotInfo[ParamIndex].Size >> 2, (GLfloat*)NewValue);
	}

	// handle global shader type based on params (this is hacky)
	if (StandardUniformSlotInfo[ParamIndex].GlobalShaderType != EGST_None)
	{
		GShaderManager.SetNextDrawGlobalShader( StandardUniformSlotInfo[ParamIndex].GlobalShaderType );
	}
} 


void FES2RHI::SetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	FES2RHI::SetVertexShaderParameter(VertexShaderRHI, BufferIndex, BaseIndex, NumBytes, NewValue, ParamIndex);
}

void FES2RHI::SetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	FES2RHI::SetPixelShaderParameter(PixelShaderRHI, BufferIndex, BaseIndex, NumBytes, NewValue, ParamIndex);
}


/**
 * Set engine shader parameters for the view.
 * @param View					The current view
 */
void FES2RHI::SetViewParameters(const FSceneView& View)
{
	FES2RHI::SetViewParametersWithOverrides(View, View.TranslatedViewProjectionMatrix, View.DiffuseOverrideParameter, View.SpecularOverrideParameter);
}

/**
 * Set engine shader parameters for the view.
 * @param View					The current view
 * @param ViewProjectionMatrix	Matrix that transforms from world space to projection space for the view
 * @param DiffuseOverride		Material diffuse input override
 * @param SpecularOverride		Material specular input override
 */
void FES2RHI::SetViewParametersWithOverrides( const FSceneView& View, const FMatrix& ViewProjectionMatrix, const FVector4& DiffuseOverride, const FVector4& SpecularOverride )
{
	// Set overlay color for full scene fading
	const UBOOL bEnableColorFading = ( View.OverlayColor.A > KINDA_SMALL_NUMBER );
	GShaderManager.SetColorFading( bEnableColorFading, View.OverlayColor );

	// @todo: Only set this for shaders that actually need it? (currently very few!)
	UniformMatrix4fv(SS_WorldToViewSlot, 1, GL_FALSE, &View.ViewMatrix.M[0][0]);

	// OpenGL uses [-1..1] space to clip all coordinates, while D3D uses [0..1] for Z,
	// so we need to modify the ViewProjectionMatrix to get the correct result
	FScaleMatrix ClipSpaceFixScale(FVector(1.0f, 1.0f, 2.0f));
	FTranslationMatrix ClipSpaceFixTranslate(FVector(0.0f, 0.0f, -1.0f));	
	const FMatrix AdjustedViewProjectionMatrix( ViewProjectionMatrix * ClipSpaceFixScale * ClipSpaceFixTranslate );
	UniformMatrix4fv(SS_ViewProjectionSlot, 1, GL_FALSE, &AdjustedViewProjectionMatrix.M[0][0]);
#if FLASH
	UniformMatrix4fv(SS_LocalToProjectionSlot, 1, GL_FALSE, &AdjustedViewProjectionMatrix.M[0][0]);
#else
	const FMatrix LocalToProjectionTranspose = AdjustedViewProjectionMatrix.Transpose();
	UniformMatrix4fv(SS_LocalToProjectionSlot, 1, GL_FALSE, &LocalToProjectionTranspose.M[0][0]);
#endif
	GShaderManager.SetViewProjectionMatrix(AdjustedViewProjectionMatrix);
}

void FES2RHI::SetViewPixelParameters(const FSceneView* View, FPixelShaderRHIParamRef PixelShader, const class FShaderParameter* SceneDepthCalcParameter, 
	const class FShaderParameter* ScreenPositionScaleBiasParameter,const class FShaderParameter* ScreenAndTexelSizeParameter)
{ 
	if ( SceneDepthCalcParameter && SceneDepthCalcParameter->IsBound() )
	{
		Uniform4fv(SS_MinZ_MaxZRatio, 1, (GLfloat*)&View->InvDeviceZToWorldZTransform);
	}
	if ( ScreenPositionScaleBiasParameter && ScreenPositionScaleBiasParameter->IsBound() )
	{
		SetPixelShaderValue(PixelShader,*ScreenPositionScaleBiasParameter,View->ScreenPositionScaleBias);
	}
	if ( ScreenAndTexelSizeParameter && ScreenAndTexelSizeParameter->IsBound() )
	{
		FVector4 ScreenAndTexelSize(View->SizeX, View->SizeY, 1.0f / (FLOAT)View->RenderTargetSizeX, 1.0f / (FLOAT)View->RenderTargetSizeY);
		SetPixelShaderValue(PixelShader,*ScreenAndTexelSizeParameter,&ScreenAndTexelSize);
	}
} 

void* FES2RHI::GetMobileProgramInstance()
{
	return FES2ShaderProgram::CurrentProgInstance;
}

void FES2RHI::SetMobileProgramInstance(void* ProgramInstance)
{
	//FES2ShaderProgram::NextProgInstance = (FES2ShaderProgram::FProgInstance*)ProgramInstance;
}


/**
 * Constructor
 */
FES2ShaderManager::FES2ShaderManager()
	: bIsColorFadingEnabled(FALSE)
	, BlendMode(BLEND_Opaque)
	, FadeColorAndAmount(0.0f, 0.0f, 0.0f, 0.0f)
	, bIsFogEnabled(FALSE)
	, FogStart(0.0f)
	, FogEnd(0.0f)
	, BumpReferencePlane(0.0f)
	, BumpHeightRatio(0.0f)
	, BumpEnd(0.0f)
	, bUseGammaCorrection(FALSE)
	, bUseFallbackColorStream(FALSE)
	, SwayTime(0.0f)
	, SwayMaxAngle(0.0f)
	, BrightestLightColor(FLinearColor::White)
	, SpecularColor(FLinearColor::White)
	, MaterialPrograms(NULL)
	, GlobalPrograms(NULL)
	, bWillResetHasLightmapOnNextSetSamplerState(TRUE)
	, bHasHadLightmapSet(FALSE)
	, bHasHadDirectionalLightmapSet(FALSE)
#if !FINAL_RELEASE
	, bNewMaterialSet(FALSE)
#endif
	, NextDrawGlobalShaderType(EGST_None)
	, CurrentPrimitiveIndex(0)
	, TrackedPrimitiveDelta(0)
	, TrackedPrimitiveIndex(-1)
	, PlatformFeatures(EPF_HighEndFeatures)
	, PrimitiveType(EPT_Default)
	, VersionedShaderParameters(NULL)
{
	VertexSettings.Reset();
	PixelSettings.Reset();
	MeshSettings.Reset();
}

void FES2ShaderManager::InitRHI()
{
	// Initialize the global shader uniform cache
	VersionedShaderParameters = new FVersionedShaderParameter[SS_MAX];

	// Update for the 
	PlatformFeatures = (EMobilePlatformFeatures)GSystemSettings.MobileFeatureLevel;

	// Allocate the uniform shadow memory
	for( INT SlotIndex = 0; SlotIndex < SS_MAX; SlotIndex++ )
	{
		VersionedShaderParameters[SlotIndex].Data = appMalloc(sizeof(GLfloat) * StandardUniformSlotInfo[SlotIndex].Size);
	}

	// check to see if we should report out warmed shaders
	bDebugShowWarmedKeys = FALSE;
	if (ParseParam(appCmdLine(), TEXT("DebugShowWarmedKeys")))
	{
		bDebugShowWarmedKeys = TRUE;
	}

#if !MOBILESHADER_THREADED_INIT
	// Initialize the shaders now and block the game thread.
	InitShaderPrograms();
#endif
}

/**
 * Allow for recompiling of shaders on the fly
 */
void RecompileES2Shaders()
{
	GShaderManager.ClearCompiledShaders();
	GShaderManager.InitShaderPrograms();
}

void RecompileES2Shader(FProgramKey ProgramKey)
{
	GShaderManager.ClearCompiledShader(ProgramKey);
	GShaderManager.InitPreprocessedShaderProgram(ProgramKey);
}

void RecompileES2GlobalShaders()
{
	static UBOOL bInitialized = FALSE;
	if (!bInitialized)
	{
		GShaderManager.InitGlobalShaderPrograms();
		bInitialized = TRUE;
	}
}

void WarmES2ShaderCache()
{
	GShaderManager.WarmShaderCache();
}

/** 
 * Perform global initialization for all programs
 */
void FES2ShaderManager::InitShaderPrograms()
{
	InitGlobalShaderPrograms();

	InitPreprocessedShaderPrograms();
}

void FES2ShaderManager::InitGlobalShaderPrograms()
{
	// load the prefix files
	UBOOL bLoaded = FES2ShaderProgram::LoadShaderFromAllShaders("Prefix_Common.msf", TRUE, GShaderManager.CommonShaderPrefixFile);
	bLoaded = bLoaded && FES2ShaderProgram::LoadShaderFromAllShaders("Prefix_VertexShader.msf", TRUE, GShaderManager.VertexShaderPrefixFile);
	bLoaded = bLoaded && FES2ShaderProgram::LoadShaderFromAllShaders("Prefix_PixelShader.msf", TRUE, GShaderManager.PixelShaderPrefixFile);

	if (!bLoaded)
	{
		appErrorf(TEXT("Failed to load shader files.\n"));
	}

	// allocate space for our programs
	delete [] MaterialPrograms;
	MaterialPrograms = new FES2ShaderProgram[EPT_MAX];

	// initialize them
	MaterialPrograms[EPT_Default].Init(EPT_Default, EGST_None, EShaderBaseFeatures::Lightmap | EShaderBaseFeatures::DirectionalLightmap | EShaderBaseFeatures::GPUSkinning | EShaderBaseFeatures::Decal | EShaderBaseFeatures::Landscape );
	MaterialPrograms[EPT_Particle].Init(EPT_Particle, EGST_None, EShaderBaseFeatures::SubUVParticles);
	MaterialPrograms[EPT_BeamTrailParticle].Init(EPT_BeamTrailParticle, EGST_None, EShaderBaseFeatures::Default);
	MaterialPrograms[EPT_LensFlare].Init(EPT_LensFlare, EGST_None, EShaderBaseFeatures::Default);
	MaterialPrograms[EPT_Simple].Init(EPT_Simple, EGST_None, EShaderBaseFeatures::Default);
	MaterialPrograms[EPT_DistanceFieldFont].Init(EPT_DistanceFieldFont, EGST_None, EShaderBaseFeatures::Default);

	delete [] GlobalPrograms;
	GlobalPrograms = new FES2ShaderProgram[EGST_MAX];
	for (INT GlobalShaderIndex = 0; GlobalShaderIndex < EGST_MAX; GlobalShaderIndex++)
	{
		if (MobileGlobalShaderExists( (EMobileGlobalShaderType)GlobalShaderIndex ))
		{
			// there's no code for EGST_None
			GlobalPrograms[GlobalShaderIndex].Init(EPT_GlobalShader, (EMobileGlobalShaderType)GlobalShaderIndex, EShaderBaseFeatures::Default);
		}
	}

#if _WINDOWS
	// If we plan to profile the preprocessed shaders, which we can only do in the simulator,
	// we need to avoid using them to ensure we go through the entire path of constructing
	// them before profiling
	if( GSystemSettings.bProfilePreprocessedShaders )
	{
		GSystemSettings.bUsePreprocessedShaders = FALSE;
	}
#endif

}

/** 
 * Clears out any cached shader programs so that they can be forcefully recompiled
 */
void FES2ShaderManager::ClearCompiledShaders()
{
	GShaderManager.CompiledShaders.Empty();
}

/** 
 * Clears out any a specific cached shader programs so that it can be forcefully recompiled
 */
void FES2ShaderManager::ClearCompiledShader(const FProgramKey & ProgramKey)
{
	CompiledShaders.RemoveKey(ProgramKey);
}

/** 
 * Clears out any GPU Resources used by Shader Manager
 */
void FES2ShaderManager::ClearGPUResources()
{

}

/**
 * Discovers and initializes all preprocessed shader program instances
 */
void FES2ShaderManager::InitPreprocessedShaderPrograms()
{
	// Discover and initialize all preprocessed shader program instances, if we're either
	// reading or writing the shader program cache
	if( !(GUseSeekFreeLoading &&
		( GSystemSettings.bUsePreprocessedShaders ||
		GSystemSettings.bCachePreprocessedShaders )) )
	{
		return;
	}

	STAT(DOUBLE InitShaderCacheTime = 0);
	{
		SCOPE_SECONDS_COUNTER(InitShaderCacheTime);
		// the shaders are in the cooked directory
		FString CookedDirectory;
		appGetCookedContentPath(appGetPlatformType(), CookedDirectory);

		//warnf(NAME_Warning, TEXT("GSystemSettings.MobileFeatureLevel == %d"), GSystemSettings.MobileFeatureLevel);

#if _WINDOWS
		// If we're caching program keys during this run, clean out the directory
		if( GSystemSettings.bCachePreprocessedShaders )
		{
			TArray<FString> FoundDirectoryNames;
			FString CachedProgramsDirectoryName = CookedDirectory + TEXT("Shaders\\");
			GFileManager->FindFiles(FoundDirectoryNames, *(CachedProgramsDirectoryName + TEXT("*")), FALSE, TRUE );
			for( INT DirIndex = 0; DirIndex < FoundDirectoryNames.Num(); DirIndex++ )
			{
				GFileManager->DeleteDirectory(*(CachedProgramsDirectoryName + FoundDirectoryNames(DirIndex)), TRUE, TRUE);
			}
		}
#endif
		// If we want to use cached program keys, see if the program key cache exists, then open it and create the programs
		if( GSystemSettings.bUsePreprocessedShaders )
		{
			FString ProgramKeysFileName = CookedDirectory + TEXT("CachedProgramKeys.txt");
			FString ProgramKeysFileText;
			if( appLoadFileToString(ProgramKeysFileText, *ProgramKeysFileName) )
			{
				DOUBLE StartTime(appSeconds());
				TArray<FString> Keys;

				// Extract the key set from the cache
				ProgramKeysFileText.ParseIntoArray(&Keys, TEXT("\r\n"), TRUE);
				if( Keys.Num() > 0 )
				{
					FString& VersionString = Keys(0);
					const FString VersionToken(TEXT("version:"));
					if( VersionString.StartsWith(VersionToken) )
					{
						INT ManifestVersion = appAtoi(*VersionString.Mid(VersionToken.Len()));
						if( ManifestVersion != SHADER_MANIFEST_VERSION )
						{
							warnf(NAME_Warning, TEXT("Shader manifest is an old version, ignoring."));
						}
						else
						{
							// init the preprocessed shader loading
							FES2ShaderProgram::StartLoadingPreprocessedShaderInfos();

							// Shader manifest is valid, proceed to load and cache the shader programs

							// Parse the program keys, looking for equivalence classes as we go
							const FString vse(TEXT("vse:"));
							const FString pse(TEXT("pse:"));
							for( INT ProgramKeyIndex = 1; ProgramKeyIndex < Keys.Num(); ProgramKeyIndex++ )
							{
								FString& ProgramKeyName = Keys(ProgramKeyIndex);

								UBOOL bIsVertexShaderEquivalence = ProgramKeyName.StartsWith(vse);
								UBOOL bIsPixelShaderEquivalence = ProgramKeyName.StartsWith(pse);

								// Check for an equivalence group
								if( bIsVertexShaderEquivalence || bIsPixelShaderEquivalence )
								{
									// We found a group, see if we have that sharing enabled
									if( !GSystemSettings.bShareShaderPrograms )
									{
										if( (bIsVertexShaderEquivalence && !GSystemSettings.bShareVertexShaders) ||
											(bIsPixelShaderEquivalence && !GSystemSettings.bSharePixelShaders) )
										{
											// Ignore because sharing isn't enabled
											continue;
										}
									}
#if _WINDOWS
									if( GSystemSettings.bCachePreprocessedShaders ||
										GSystemSettings.bProfilePreprocessedShaders )
									{
										// We want to force it skip any cache so it fully regenerates the equivalences
										continue;
									}
#endif
									// Decide which equivalence group to work with
									TMap<FProgramKey,FProgramKey>* KeyMap = bIsVertexShaderEquivalence ? &GMobileShaderInitialization.VertexKeyToEquivalentMasterKey : &GMobileShaderInitialization.PixelKeyToEquivalentMasterKey;

									// Extract the equivalence keys
									TArray<FString> EquivalentKeys;
									ProgramKeyName.Mid(4).ParseIntoArray(&EquivalentKeys, TEXT(","), TRUE);
									check(EquivalentKeys.Num() > 1);

									// The master key is used as the key for the entire equivalence class, arbitrarily the first one in the list
									FProgramKey MasterKey( EquivalentKeys(0) );

									// Ensure the master key is new, and set up the initial entry
									check(!KeyMap->Find(MasterKey));
									KeyMap->Set(MasterKey, MasterKey);

									// Walk through the equivalence list and add entries to the key map
									for( INT EquivalentKeyIndex = 1; EquivalentKeyIndex < EquivalentKeys.Num(); EquivalentKeyIndex++ )
									{
										FProgramKey EquivalentKey( EquivalentKeys(EquivalentKeyIndex) );

										// Ensure the key is new, and set up the equivalence entry
										check(!KeyMap->Find(EquivalentKey));
										KeyMap->Set(EquivalentKey, MasterKey);
									}
								}
								else
								{
									// This is a new program key that needs to be compiled

									// Extract the primitive type and the program key from the directory name
									FProgramKey ProgramKey( ProgramKeyName );

									InitPreprocessedShaderProgram(ProgramKey);
								}
							}

							// free up the map of all the preprocessed shaders since we have loaded them all by now
							FES2ShaderProgram::StopLoadingPreprocessedShaderInfos();

							WarmShaderCache();

							// This effectively stops all equivalence checking... we will never find any again anyway
							GMobileShaderInitialization.VertexKeyToEquivalentMasterKey.Empty();
							GMobileShaderInitialization.PixelKeyToEquivalentMasterKey.Empty();

							// Get the memory back
							GMobileShaderInitialization.VertexMasterKeyToGLShader.Empty();
							GMobileShaderInitialization.PixelMasterKeyToGLShader.Empty();
						}
					}
				}
			}
		}
	}
	INC_FLOAT_STAT_BY(STAT_ES2InitShaderCacheTime,(FLOAT)InitShaderCacheTime);
	STAT(debugf(TEXT("Shader compiling: %s"), *appPrettyTime(InitShaderCacheTime)));

}

UBOOL ValidateShaderProgram(GLint Program)
{	
#if FLASH 
	return TRUE;
#else // FLASH
	glValidateProgram(Program);
	GLint ProgramStatus;
	glGetProgramiv(Program, GL_VALIDATE_STATUS, &ProgramStatus);
	if (ProgramStatus == GL_TRUE)
	{
		return TRUE;
	}

#if !FINAL_RELEASE
	debugf(TEXT("Program failed to validate during warm!"));

	if (!glIsProgram(Program))
	{
		debugf(TEXT("Not a valid program"));
	}

	GLint QueryResult;
	GLCHECK(glGetProgramiv(Program, GL_DELETE_STATUS, &QueryResult));
	if (QueryResult==GL_TRUE) debugf(TEXT("\tMarked for deletion"));
	GLCHECK(glGetProgramiv(Program, GL_LINK_STATUS, &QueryResult));
	if (QueryResult==GL_TRUE) debugf(TEXT("\tFailed to link"));
	GLCHECK(glGetProgramiv(Program, GL_ATTACHED_SHADERS, &QueryResult));
	if (QueryResult==GL_TRUE) debugf(TEXT("\t%d attached shaders"), QueryResult);
	GLCHECK(glGetProgramiv(Program, GL_ACTIVE_ATTRIBUTES, &QueryResult));
	if (QueryResult==GL_TRUE) debugf(TEXT("\t%d active attributes"), QueryResult);
	GLCHECK(glGetProgramiv(Program, GL_ATTACHED_SHADERS, &QueryResult));
	if (QueryResult==GL_TRUE) debugf(TEXT("\t%d attached shaders"), QueryResult);

	GLint LogLength;
	glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &LogLength);

	ANSICHAR* InfoLog = new ANSICHAR[LogLength];
	glGetProgramInfoLog(Program, LogLength, NULL, InfoLog);
	debugf(TEXT("Program Log: %s"), ANSI_TO_TCHAR(InfoLog));
	delete [] InfoLog;

	const GLint MaxShaders = 8;
	GLuint Shaders[MaxShaders];
	GLsizei ShaderCount = 0;
	GLint ShaderLogLength = 0;
	glGetAttachedShaders(Program, MaxShaders, &ShaderCount, Shaders);
	for (INT i = 0; i < ShaderCount; ++i)
	{
		glGetShaderiv(Shaders[i], GL_INFO_LOG_LENGTH, &ShaderLogLength);
		ANSICHAR* ShaderInfoLog = new ANSICHAR[ShaderLogLength];
		glGetShaderInfoLog(Shaders[i], ShaderLogLength, NULL, ShaderInfoLog);
		debugf(TEXT("Shader Log %d: %s"), i, ANSI_TO_TCHAR(ShaderInfoLog));
		delete [] ShaderInfoLog;
	}
#endif

	return FALSE;
#endif // FLASH
}

void FES2ShaderManager::WarmShaderCache()
{
	// Make a simple draw call to use the shader once and have the driver warm it up.
	// Without this call, the first time we draw using this shader, we'll still incur a
	// hitch in the driver as it finishes compiling the shader for the HW.

	STAT(DOUBLE InitShaderCacheDrawCallTime = 0);
	INT NumShadersWarmed = 0;

	debugf(TEXT("WarmShaderCache"));

	GStateShadow.InvalidateAndResetDevice();

	if( GSystemSettings.bWarmUpPreprocessedShaders )
	{
		SCOPE_SECONDS_COUNTER(InitShaderCacheDrawCallTime);

		// Set up the degenerate triangle data we'll use to "warm up" each shader
		static FLOAT NULLVertexDataStream[12] = {
									0.0f, 0.0f, 0.0f, 0.0f,
									0.0f, 0.0f, 0.0f, 0.0f,
									0.0f, 0.0f, 0.0f, 0.0f,
		};
		INT Stream;
		// Set up all the possible vertex attribute streams
		for( Stream = 0; Stream < GMaxVertexAttribsGLSL; Stream++ )
		{
			GLCHECK(glDisableVertexAttribArray(Stream));

// @todo flash: Fix this up before merging code back to main
#if FLASH
			GLCHECK(glVertexAttribPointer(
				Stream,
				4,
				GL_FLOAT,
				0,
				0,
				NULLVertexDataStream,
				48
			));
#else
			GLCHECK(glVertexAttribPointer(
				Stream,
				4,
				GL_FLOAT,
				0,
				0,
				NULLVertexDataStream
			));
#endif
		}
		WORD Indicies[3] = {0, 1, 2};
		GLint CurrentActiveAttributeMask = 0;

		// For each of the shader types, for all compiled programs, issue a single draw call to warm them up
		for( INT PrimitiveTypeIndex = 0; PrimitiveTypeIndex < EPT_MAX; PrimitiveTypeIndex++ )
		{
			TArray<FProgramKey> ProgramKeyArray;
			TArray<FES2ShaderProgram::FProgInstance*> ProgramInstanceArray;

			MaterialPrograms[PrimitiveTypeIndex].ProgMap.GenerateKeyArray(ProgramKeyArray);
	// warming up is disabled on Flash, and it's tricky with the extra texture format indirection, but at least this will compile
	#if !FLASH
			MaterialPrograms[PrimitiveTypeIndex].ProgMap.GenerateValueArray(ProgramInstanceArray);
	#endif

			for( INT ProgramIndex = 0; ProgramIndex < ProgramInstanceArray.Num(); ProgramIndex++ )
			{
				if (ProgramInstanceArray(ProgramIndex)->bWarmed)
				{
					continue;
				}

				FProgramKeyData NextKeyData;
				NextKeyData.UnpackProgramKeyData(ProgramKeyArray(ProgramIndex));

				if (bDebugShowWarmedKeys)
				{
					debugf(TEXT("[WARM KEY]:%s"), *ProgramKeyArray(ProgramIndex).ToString());
				}

				ProgramInstanceArray(ProgramIndex)->bWarmed = TRUE;

				// Set up and enable all valid attribute streams for the draw call
				GLint UsedAttribMask = ProgramInstanceArray(ProgramIndex)->UsedAttribMask;
				for( Stream = 0; Stream < GMaxVertexAttribsGLSL; Stream++ )
				{
					GLint CurrentAttributeMask = (1 << Stream);

					// If it needs to be enabled
					if( (UsedAttribMask & CurrentAttributeMask) != 0 )
					{
						// If it's not enabled
						if( (CurrentActiveAttributeMask & CurrentAttributeMask) == 0 )
						{
							CurrentActiveAttributeMask |= CurrentAttributeMask;
							GLCHECK(glEnableVertexAttribArray(Stream));
						}
					}
					else
					{
						// If it is enabled
						if( (CurrentActiveAttributeMask & CurrentAttributeMask) != 0 )
						{
							CurrentActiveAttributeMask &= ~CurrentAttributeMask;
							GLCHECK(glDisableVertexAttribArray(Stream));
						}
					}
				}

				GLCHECK(glUseProgram(ProgramInstanceArray(ProgramIndex)->Program));
				//STAT(DOUBLE DrawCallTime = 0);
				for( INT ColorWriteMaskIndex = 0; ColorWriteMaskIndex < 2; ColorWriteMaskIndex++ )
				{
					switch(ColorWriteMaskIndex)
					{
						case 0:	RHISetColorWriteMask(CW_RGBA); break;
						case 1:	RHISetColorWriteMask(CW_RGB); break;
					}

					//SCOPE_SECONDS_COUNTER(DrawCallTime);

					// FProgramKeyData NextKeyData;
					// NextKeyData.UnpackProgramKeyData(ProgramKeyArray(ProgramIndex));

					switch(NextKeyData.GetFieldValue(FProgramKeyData::PKDT_BlendMode))
					{
						case BLEND_Opaque:
							RHISetBlendState(TStaticBlendState<>::GetRHI());
							break;
						case BLEND_Masked:
							RHISetBlendState(TStaticBlendState<BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero, CF_Greater, 255/3>::GetRHI());
							break;
						case BLEND_Translucent:
							RHISetBlendState(TStaticBlendState<BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_Zero,BF_One>::GetRHI());
							break;
						case BLEND_Additive:
							RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());
							break;
						case BLEND_Modulate:
							RHISetBlendState(TStaticBlendState<BO_Add,BF_DestColor,BF_Zero,BO_Add,BF_Zero,BF_One>::GetRHI());
							break;
						case BLEND_ModulateAndAdd:
							RHISetBlendState( TStaticBlendState<BO_Add, BF_DestColor, BF_One>::GetRHI() );
							break;
					}

					if (ValidateShaderProgram(ProgramInstanceArray(ProgramIndex)->Program))
					{
						GLCHECK(glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, Indicies));
					}

				}				
				NumShadersWarmed++;
			}
		}
		// Restore state and disable the enabled vertex arrays we used for warming up the compiled shaders
		GStateShadow.InvalidateAndResetDevice();
	}

	INC_FLOAT_STAT_BY(STAT_ES2InitShaderCacheDrawCallTime,(FLOAT)InitShaderCacheDrawCallTime);
	debugf(TEXT("WarmShaderCache:ShadersWarmed %d"), NumShadersWarmed);
}


void FES2ShaderManager::InitPreprocessedShaderProgram(const FProgramKey & ProgramKey)
{
	FES2ShaderProgram::StartLoadingPreprocessedShaderInfos();
	// Unpack the program key
	FProgramKeyData KeyData;
	KeyData.UnpackProgramKeyData(ProgramKey);

	//ONLY EVER LOAD THINGS for THIS platform
	if (KeyData.GetFieldValue(FProgramKeyData::PKDT_PlatformFeatures) != PlatformFeatures)
	{
		return;
	}

	// Exit early if the shader has already been compiled
	if (CompiledShaders.Contains(ProgramKey))
	{
		return;
	}
	
	CompiledShaders.Add(ProgramKey);

	// Put vertex factory flags back in place!!!!
	GShaderManager.VertexFactoryFlags = 0;
	GShaderManager.VertexFactoryFlags |= (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsLightmap) != 0) ? EShaderBaseFeatures::Lightmap : 0;
	GShaderManager.VertexFactoryFlags |= (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsDirectionalLightmap) != 0) ? EShaderBaseFeatures::DirectionalLightmap : 0;
	GShaderManager.VertexFactoryFlags |= (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsSkinned) != 0) ? EShaderBaseFeatures::GPUSkinning : 0;
	GShaderManager.VertexFactoryFlags |= (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsDecal) != 0) ? EShaderBaseFeatures::Decal : 0;
	GShaderManager.VertexFactoryFlags |= (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsSubUV) != 0) ? EShaderBaseFeatures::SubUVParticles : 0;
	GShaderManager.VertexFactoryFlags |= (KeyData.GetFieldValue(FProgramKeyData::PKDT_IsLandscape) != 0) ? EShaderBaseFeatures::Landscape : 0;
	PrimitiveType =(EMobilePrimitiveType)KeyData.GetFieldValue(FProgramKeyData::PKDT_PrimitiveType);

	UBOOL bSuccess;
	if (PrimitiveType == EPT_GlobalShader)
	{
		GlobalShaderType = (EMobileGlobalShaderType)KeyData.GetFieldValue(FProgramKeyData::PKDT_GlobalShaderType);
		FES2ShaderProgram::FProgInstance* ProgInstance = GlobalPrograms[GlobalShaderType].GetInstance(ProgramKey, 0);
		if (!ProgInstance)
		{
			FProgramKey MasterKey;
			FES2ShaderProgram::FProgInstance NewInstance;
			STAT(DOUBLE InitShaderCacheCompileTime = 0);
			{
				SCOPE_SECONDS_COUNTER(InitShaderCacheCompileTime);
				MasterKey = GlobalPrograms[GlobalShaderType].InitNewInstance( NewInstance, ProgramKey, KeyData, bSuccess, 0 );
			}
			INC_FLOAT_STAT_BY(STAT_ES2InitShaderCacheCompileTime,(FLOAT)InitShaderCacheCompileTime);

			// See if the resulting program is a new "master" program, add it to the set
			if ( MasterKey == ProgramKey )
			{
				FES2ShaderProgram::FProgInstance* StoredInstance = new FES2ShaderProgram::FProgInstance(NewInstance);
				GlobalPrograms[GlobalShaderType].SetInstance( ProgramKey, 0, StoredInstance );
				INC_DWORD_STAT(STAT_ShaderProgramCountPP);
			}
		}
	}
	else
	{
		FES2ShaderProgram::FProgInstance* ProgInstance = MaterialPrograms[PrimitiveType].GetInstance(ProgramKey, 0);
		if (!ProgInstance)
		{
			FProgramKey MasterKey;
			FES2ShaderProgram::FProgInstance NewInstance;

			// Create the new program instance
			STAT(DOUBLE InitShaderCacheCompileTime = 0);
			{
				SCOPE_SECONDS_COUNTER(InitShaderCacheCompileTime);
				MasterKey = MaterialPrograms[PrimitiveType].InitNewInstance( NewInstance, ProgramKey, KeyData, bSuccess, 0 );
			}
			INC_FLOAT_STAT_BY(STAT_ES2InitShaderCacheCompileTime,(FLOAT)InitShaderCacheCompileTime);

			// See if the resulting program is a new "master" program, add it to the set
			if (MasterKey == ProgramKey)
			{
				// New master shader
				FES2ShaderProgram::FProgInstance* StoredInstance = new FES2ShaderProgram::FProgInstance(NewInstance);
				MaterialPrograms[PrimitiveType].SetInstance( ProgramKey, 0, StoredInstance );
				INC_DWORD_STAT(STAT_ShaderProgramCountPP);
			}
		}
	}
}

/**
 * Clears all shader programs, forcing them to be reconstructed on demand from source
 */
void FES2ShaderManager::ClearShaderProgramInstances()
{
	if (!MaterialPrograms)
	{
		return;
	}

	for( INT CurProgramIndex = 0; CurProgramIndex < EPT_MAX; ++CurProgramIndex )
	{
		MaterialPrograms[ CurProgramIndex ].DestroyAllInstances();
	}
	for( INT CurProgramIndex = 0; CurProgramIndex < EGST_MAX; ++CurProgramIndex )
	{
		GlobalPrograms[ CurProgramIndex ].DestroyAllInstances();
	}
}

void FES2ShaderManager::ResetPlatformFeatures()
{
	PlatformFeatures = (EMobilePlatformFeatures)GSystemSettings.MobileFeatureLevel;

	// Apply ES2 Core state modifications
	FES2Core::InitES2Core();

	CheckOpenGLExtensions();

#if IPHONE || ANDROID || FLASH
	// Disable projected mod shadows if the device doesn't support depth textures.
	if ( !GSupportsDepthTextures && GSystemSettings.bAllowDynamicShadows && GSystemSettings.bMobileModShadows )
	{
		debugf(TEXT("Disabling projected mod shadows because depth textures are not supported on this device."));
		GSystemSettings.bAllowDynamicShadows = FALSE;
		GSystemSettings.bMobileModShadows = FALSE;
	}

	GSystemSettings.MaxAnisotropy = Min( GSystemSettings.MaxAnisotropy, GPlatformFeatures.MaxTextureAnisotropy );

	// Disable MSAA if we're going to use projected mod shadows.
	if ( GSystemSettings.bAllowDynamicShadows && GSystemSettings.bMobileModShadows && GMSAAAllowed && GMSAAEnabled )
	{
		GMSAAEnabled = FALSE;
	}

	// Disable projected mod shadows if the device doesn't support depth textures.
	if ( !GSupportsDepthTextures && GSystemSettings.bAllowDynamicShadows && GSystemSettings.bMobileModShadows )
	{
		GSystemSettings.bAllowDynamicShadows = FALSE;
		GSystemSettings.bMobileModShadows = FALSE;
	}
#endif
}

#if !FINAL_RELEASE
static FString MobileTextureUnitNames[MAX_Mapped_MobileTexture] =
{
	"Base",
	"Detail",
	"Lightmap",
	"Normal",
	"Environment",
	"Mask",
	"Emissive",
	"LightMap2"
};

const FString& GetMobileTextureUnitName(EMobileTextureUnit Unit)
{
	checkSlowish(Unit < MAX_Mapped_MobileTexture);
	return MobileTextureUnitNames[Unit];
}
#endif

/**
 * Set the current program
 *
 * @param Type The primitive type that will be rendered
 * @param InGlobalShaderType The type of global shader if this is non-material program
 */
UBOOL FES2ShaderManager::SetProgramByType(EMobilePrimitiveType Type, EMobileGlobalShaderType InGlobalShaderType)
{
	PrimitiveType = Type;
	GlobalShaderType = InGlobalShaderType;
	
	//if simple and we've set distance field font params
	if ((Type == EPT_Simple) && bIsDistanceFieldFont)
	{
		Type = EPT_DistanceFieldFont;
	}
	
	FES2ShaderProgram* Program;
	if (Type == EPT_GlobalShader)
	{
		Program = &GlobalPrograms[GlobalShaderType];
	}
	else
	{
		Program = &MaterialPrograms[Type];
		//ensure we are not expecting the program to support a feature that it doesn't
		checkSlowish((VertexFactoryFlags & (~Program->BaseFeatures)) == 0);
	}
	UBOOL bShaderChanged = Program->UpdateCurrentProgram();

#if !FINAL_RELEASE
	// check that the material sets all textures it requests
	if (bNewMaterialSet)
	{
		// bNewMaterialSet disables this check for subsequent primitives that use the same material, since we avoid re-setting these textures by design
		bNewMaterialSet = FALSE;
		FES2ShaderProgram::FProgInstance* ProgInstance = Program->CurrentProgInstance;
		for (INT i = 0 ; i < MAX_Mapped_MobileTexture; ++i)
		{
			if ( (ProgInstance->UsedTextureSlotMapping[i] >= 0) && (GStateShadow.LastUpdatePrimitive[i] != CurrentPrimitiveIndex) )
			{
				EMobileTextureUnit TextureUnit = static_cast<EMobileTextureUnit>(i);
				warnf(NAME_Warning, TEXT("Material %s uses %s texture, that is not set.  It is possible that the specified texture has bIsCompositingSource set and is used for rendering."),*MaterialName, *GetMobileTextureUnitName(TextureUnit));
			}
		}
	}
#endif

	// Fog uniform and color updates
	FLinearColor FinalFogColor(0,0,0,0);
	if(IsHeightFogEnabled())
	{
		TStaticArray<FLOAT,4> CameraRelativeFogMinHeight = HeightFogParams.FogMinHeight;
		TStaticArray<FLOAT,4> CameraRelativeFogMaxHeight = HeightFogParams.FogMaxHeight;
		for(UINT LayerIndex = 0;LayerIndex < 4;++LayerIndex)
		{
			CameraRelativeFogMinHeight[LayerIndex] -= CameraPosition.Z;
			CameraRelativeFogMaxHeight[LayerIndex] -= CameraPosition.Z;
		}

		Uniform4fv(SS_FogDistanceScale,1,(GLfloat*)&HeightFogParams.FogDistanceScale);
		Uniform4fv(SS_FogStartDistance,1,(GLfloat*)&HeightFogParams.FogStartDistance);
		Uniform4fv(SS_FogExtinctionDistance,1,(GLfloat*)&HeightFogParams.FogExtinctionDistance);
		Uniform4fv(SS_FogMinHeight,1,(GLfloat*)&CameraRelativeFogMinHeight);
		Uniform4fv(SS_FogMaxHeight,1,(GLfloat*)&CameraRelativeFogMaxHeight);
		TStaticArray<FLinearColor,4> FogScattering = HeightFogParams.FogInScattering;
		if( BlendMode == BLEND_Additive )
		{
			for (INT FogScatterIndex = 0; FogScatterIndex < 4; ++FogScatterIndex)
			{
				// For additive blending, we'll use black as the fog color so that the additive primitives
				// fade away instead of brightening and revealing the transparent (zero-color) parts of the primitive
				FogScattering[FogScatterIndex].R = FogScattering[FogScatterIndex].G = FogScattering[FogScatterIndex].B = 0.0f;
			}
		}

		Uniform4fv(SS_FogInScattering,4,(GLfloat*)&FogScattering);
	}
	else if(IsGradientFogEnabled())
	{
		FinalFogColor = FogColorAndAmount;
		if (!IsFogSaturated())
		{
			FLOAT FogStartSquared = BIG_NUMBER;
			FLOAT FogOneOverSquaredRange = SMALL_NUMBER;
			FLinearColor ShaderFogColor = FogColor;
			if (bIsFogEnabled && VertexSettings.bIsFogEnabledPerPrim)
			{
				// The fog formula we're using is a cheaper approximation than fully the general case:
				//     clamp( ((VertDistSquared) - (FogStartSquared)) / ((FogEndSquared) - (FogStartSquared)), 0, 1 )
				// The advantage is that the math is a bit cheaper and we can almost all of it per-object
				FogStartSquared = FogStart * FogStart;
				FogOneOverSquaredRange = 1.0f / ((FogEnd * FogEnd) - FogStartSquared);

				if( BlendMode == BLEND_Additive )
				{
					// For additive blending, we'll use black as the fog color so that the additive primitives
					// fade away instead of brightening and revealing the transparent (zero-color) parts of the primitive
					ShaderFogColor.R = ShaderFogColor.G = ShaderFogColor.B = 0.0f;
					//At max fog, additive materials should be fully removed
					ShaderFogColor.A = 1.0;
				}
			}

			// Send down the fog settings
			Uniform1fv(SS_FogOneOverSquaredRange, 1, &FogOneOverSquaredRange);
			Uniform1fv(SS_FogStartSquared, 1, &FogStartSquared);
			Uniform4fv(SS_FogColor, 1, (GLfloat*)&ShaderFogColor);
		}
	}

	// Bump offset is enabled per material, and also based on distance from the viewer,
	// so only set these uniforms if bump offset is actually enabled
	if( IsBumpOffsetEnabled() )
	{
		// Pre-multiply and negate the bump reference plane for performance
		FLOAT PreMultipliedBumpReferencePlane = -(BumpReferencePlane * BumpHeightRatio);
		Uniform1fv( SS_PreMultipliedBumpReferencePlane, 1, (const GLfloat *)&PreMultipliedBumpReferencePlane );
		Uniform1fv( SS_BumpHeightRatio, 1, (const GLfloat *)&BumpHeightRatio );
	}

	// We always do some form of color fading/scaling, e.g. fog, color fading (matinee)
	{
		FLinearColor FinalFadeColorAndAmount = FadeColorAndAmount;

		if( BlendMode == BLEND_Additive )
		{
			// For additive blending, we'll use black as the fade color so that the additive primitives
			// fade away instead of brightening and revealing the transparent (zero-color) parts of the primitive
			FinalFadeColorAndAmount.R = FinalFadeColorAndAmount.G = FinalFadeColorAndAmount.B = 0.0f;
			if (IsFogSaturated())
			{
				//additive materials that are fully "fogged" should be transparent
				FinalFadeColorAndAmount.A = 1.0;
			}
		}
		// If not using an additive material, fold in saturated fog color, if applicable
		else if (!IsGradientFogEnabled() || IsFogSaturated())
		{
			FLOAT FogColorAmount = 1.0f - FadeColorAndAmount.A;
			FinalFadeColorAndAmount.R = Lerp(FinalFogColor.R * FogColorAmount, FadeColorAndAmount.R, FadeColorAndAmount.A);
			FinalFadeColorAndAmount.G = Lerp(FinalFogColor.G * FogColorAmount, FadeColorAndAmount.G, FadeColorAndAmount.A);
			FinalFadeColorAndAmount.B = Lerp(FinalFogColor.B * FogColorAmount, FadeColorAndAmount.B, FadeColorAndAmount.A);
			FinalFadeColorAndAmount.A = Lerp(FinalFogColor.A, 1.0f, FadeColorAndAmount.A);
		}

		if (InGlobalShaderType==EGST_ShadowProjection)
		{
			FinalFadeColorAndAmount.B = Clamp(1.f - FinalFadeColorAndAmount.A, 0.f, 1.f);
		}

		// Scene fade color
		Uniform4fv(SS_FadeColorAndAmount, 1, (GLfloat*)&FinalFadeColorAndAmount);
	}

	if (IsMobileColorGradingEnabled())
	{
		FLinearColor HightLightsMinusShadows = ColorGradingParams.HighLights - ColorGradingParams.Shadows;

		Uniform1fv(SS_MobileColorGradingBlend, 1, (GLfloat*)&ColorGradingParams.Blend);
		Uniform1fv(SS_MobileColorGradingDesaturation, 1, (GLfloat*)&ColorGradingParams.Desaturation);
		Uniform4fv(SS_MobileColorGradingHighlightsMinusShadows, 1, (GLfloat*)&HightLightsMinusShadows.R);
		Uniform4fv(SS_MobileColorGradingMidTones, 1, (GLfloat*)&ColorGradingParams.MidTones.R);
		Uniform4fv(SS_MobileColorGradingShadows, 1, (GLfloat*)&ColorGradingParams.Shadows.R);
	}

#if FLASH
	// On Flash, always do a scale and bias to emulate viewport transform
	// This value is calculated in SetViewport
	{
		// debugf(TEXT("Setting SS_ViewportScaleBias to (%f, %f, %f, %f)"), ViewportScaleBias.X, ViewportScaleBias.Y, ViewportScaleBias.Z, ViewportScaleBias.W);
		Uniform4fv(SS_ViewportScaleBias, 1, (GLfloat*)&ViewportScaleBias);
	}
#endif

	// Update the uniforms for the current program
	Program->UpdateCurrentUniforms(bShaderChanged);

	return bShaderChanged;
}

/**
 * Handles updates to the viewport
 *
 * @param	PosX	Origin X
 * @param	PosY	Origin Y
 * @param	SizeX	Width
 * @param	SizeY	Height
 */
void FES2ShaderManager::SetViewport( const UINT PosX, const UINT PosY, const UINT SizeX, const UINT SizeY )
{
#if FLASH
	// Emulate viewport functionality with ViewportScaleBias, which
	// is used in the VS, and a scissor rect, which is supported

	// These values are in Viewport Space. 
	FLOAT ViewportPosX = (FLOAT)PosX / (FLOAT)GStateShadow.RenderTargetWidth;
	FLOAT ViewportPosY = (FLOAT)PosY / (FLOAT)GStateShadow.RenderTargetHeight;
	FLOAT ViewportSizeX = (FLOAT)SizeX / (FLOAT)GStateShadow.RenderTargetWidth;
	FLOAT ViewportSizeY = (FLOAT)SizeY / (FLOAT)GStateShadow.RenderTargetHeight;

	// But the transform is being done in Clip Space: [-w,w]. The multiply by W will happen in the shader so
	// treat the transform as being in Screen Space: [-1,1].
	ViewportScaleBias.X = ViewportSizeX;
	ViewportScaleBias.Y = ViewportSizeY;
	ViewportScaleBias.Z = +2.0f * ViewportPosX + ViewportSizeX - 1.0f;
	ViewportScaleBias.W = -2.0f * ViewportPosY - ViewportSizeY + 1.0f;

 	//debugf(TEXT("SetViewport called with:"));
 	//debugf(TEXT("    %d, %d, %d, %d"), PosX, PosY, SizeX, SizeY);
 	//debugf(TEXT("    %f, %f, %f, %f"), ClipPosX, ClipPosY, ClipSizeX, ClipSizeY);
 	//debugf(TEXT("    %f, %f, %f, %f"), ViewportScaleBias.X, ViewportScaleBias.Y, ViewportScaleBias.Z, ViewportScaleBias.W);

	GLCHECK(glScissor(PosX, PosY, SizeX, SizeY));
#else
	GLCHECK(glViewport(PosX, PosY, SizeX, SizeY));
#endif
}

/**
 * Set the alpha test information for upcoming draw calls
 *
 * @param bEnable Whether or not to enable alpha test
 * @param AlphaRef Value to compare against for alpha kill
 */
void FES2ShaderManager::SetAlphaTest(UBOOL bEnable, FLOAT AlphaRef)
{
	// apply alpha settings
	Uniform1fv(SS_AlphaTestRef, 1, &AlphaRef);
}

void FES2ShaderManager::NewFrame()
{
	// Updated tracked primitive
	if (TrackedPrimitiveDelta != 0)
	{
		TrackedPrimitiveIndex += TrackedPrimitiveDelta;

		if (TrackedPrimitiveIndex >= CurrentPrimitiveIndex)
		{
			TrackedPrimitiveIndex = 0;
		} 
		else if (TrackedPrimitiveIndex < 0)
		{
			TrackedPrimitiveIndex = CurrentPrimitiveIndex - 1;
		}

		TrackedPrimitiveDelta = 0;
	}

	// Reset debug counter for new frame.
	CurrentPrimitiveIndex = 0;

	//reset texture channels for new frame
	for (INT TextureIndex = 0; TextureIndex < MAX_MobileTexture; ++TextureIndex)
	{
		GStateShadow.BoundTextureName[TextureIndex] = 0;
	}

	// reset last primitive indices per texture unit
#if !FINAL_RELEASE
	for (INT TextureIndex = 0; TextureIndex < MAX_Mapped_MobileTexture; ++TextureIndex)
	{
		GStateShadow.LastUpdatePrimitive[TextureIndex] = -1;
	}
#endif

	//reset vertex stream references
	for (INT ArrayBufferIndex = 0; ArrayBufferIndex < 16; ++ArrayBufferIndex)
	{
		GStateShadow.ArrayBuffer[ArrayBufferIndex] = 0;
	}

	//reset index stream references
	GStateShadow.ElementArrayBuffer = 0;
}

/**
  * Prints a list of shader keys which were requested in the preprocessed cached, but were not present
  */
void FES2ShaderManager::PrintMissingShaderKeys()
{
	for( INT Idx = 0; Idx < MissingPreprocessedShaders.Num(); Idx++ )
	{
		GLog->Log( *MissingPreprocessedShaders( Idx ) );
	}
}

/**
 * Resets ALL state and just sets state based on blendmode
 *
 * @param InBlendMode - Material BlendMode
 */
void FES2ShaderManager::SetMobileSimpleParams(const EBlendMode InBlendMode)
{
	VertexSettings.Reset();
	PixelSettings.Reset();
	MeshSettings.Reset();

	SetMobileBlendMode( InBlendMode );
}

/**
 * Sets up the vertex shader state for GL system
 *
 * @param InVertexParams - A composite structure of the mobile vertex parameters needed by GL shaders
 */
void FES2ShaderManager::SetMobileMaterialVertexParams(const FMobileMaterialVertexParams& InVertexParams)
{
	VertexSettings.Reset();

	// Is lighting enabled
	VertexSettings.bIsLightingEnabled = InVertexParams.bUseLighting;

	// Texture coordinate sources
	VertexSettings.BaseTextureTexCoordsSource = InVertexParams.BaseTextureTexCoordsSource;
	VertexSettings.DetailTextureTexCoordsSource = InVertexParams.DetailTextureTexCoordsSource;
	VertexSettings.MaskTextureTexCoordsSource = InVertexParams.MaskTextureTexCoordsSource;

	UBOOL bAnyTextureTransform = FALSE;
	bAnyTextureTransform |= VertexSettings.bBaseTextureTransformed        = InVertexParams.bBaseTextureTransformed;
	bAnyTextureTransform |= VertexSettings.bEmissiveTextureTransformed    = InVertexParams.bEmissiveTextureTransformed;
	bAnyTextureTransform |= VertexSettings.bNormalTextureTransformed      = InVertexParams.bNormalTextureTransformed;
	bAnyTextureTransform |= VertexSettings.bMaskTextureTransformed        = InVertexParams.bMaskTextureTransformed;
	bAnyTextureTransform |= VertexSettings.bDetailTextureTransformed      = InVertexParams.bDetailTextureTransformed;
	if (bAnyTextureTransform && !bIsTextureCoordinateTransformOverriden)
	{
		//copy transform only if we're going to use it. (Otherwise it is not going to be valid)
		UniformMatrix3fv(SS_TextureTransform, 1, FALSE, (const GLfloat *)InVertexParams.TextureTransform.M);
	}
#if WITH_EDITOR
	else
	{
		if (!bAnyTextureTransform && bIsTextureCoordinateTransformOverriden)
		{
			warnf(NAME_Warning, TEXT("MeshSubUV w/out TextureCoordinateTransform enabled?\nMaterial '%s'"),*GShaderManager.MaterialName);
		}
	}
#endif

	// Set up the material side of the program decision making process
	VertexSettings.bIsColorTextureBlendingLocked = InVertexParams.bLockTextureBlend;
	VertexSettings.bIsUsingOneDetailTexture = InVertexParams.bIsUsingOneDetailTexture && (GSystemSettings.bAllowMobileColorBlending || InVertexParams.bLockTextureBlend);
	VertexSettings.bIsUsingTwoDetailTexture = InVertexParams.bIsUsingTwoDetailTexture && (GSystemSettings.bAllowMobileColorBlending || InVertexParams.bLockTextureBlend);
	VertexSettings.bIsUsingThreeDetailTexture = InVertexParams.bIsUsingThreeDetailTexture && (GSystemSettings.bAllowMobileColorBlending || InVertexParams.bLockTextureBlend);
	VertexSettings.TextureBlendFactorSource = InVertexParams.TextureBlendFactorSource;
	BlendMode = InVertexParams.MaterialBlendMode;

	EnableNormalMapping( InVertexParams.bUseNormalMapping );

	EnableEnvironmentMapping( InVertexParams.bUseEnvironmentMapping );
	VertexSettings.EnvironmentFresnelAmount = InVertexParams.EnvironmentFresnelAmount;

	if( IsEnvironmentMappingEnabled() )
	{
		FVector4 EnvironmentParameters(
			InVertexParams.EnvironmentAmount,
			InVertexParams.EnvironmentFresnelAmount,
			InVertexParams.EnvironmentFresnelExponent,
			0.0f );
		Uniform3fv( SS_EnvironmentParameters, 1, (const GLfloat *)&EnvironmentParameters);
	}
	VertexSettings.EnvironmentMaskSource = InVertexParams.EnvironmentMaskSource;

	VertexSettings.bIsEmissiveEnabled = InVertexParams.bUseEmissive;
	VertexSettings.EmissiveColorSource = InVertexParams.EmissiveColorSource;
	VertexSettings.EmissiveMaskSource = InVertexParams.EmissiveMaskSource;
	
	if ( VertexSettings.bIsEmissiveEnabled )
	{
		Uniform4fv( SS_ConstantEmissiveColor, 1, (const GLfloat *)&InVertexParams.EmissiveColor );
	}

	EnableRimLighting( InVertexParams.RimLightingStrength != 0.0f );
	if( IsRimLightingEnabled() )
	{
		// Note: We premultiply the rim lighting strength with the color
		FVector4 RimLightingColorAndExponent(
			InVertexParams.RimLightingColor.R * InVertexParams.RimLightingStrength,
			InVertexParams.RimLightingColor.G * InVertexParams.RimLightingStrength,
			InVertexParams.RimLightingColor.B * InVertexParams.RimLightingStrength,
			InVertexParams.RimLightingExponent );
		Uniform4fv( SS_RimLightingColorAndExponent, 1, (const GLfloat *)&RimLightingColorAndExponent);
	}
	VertexSettings.RimLightingMaskSource = InVertexParams.RimLightingMaskSource;


	EnableSpecular( InVertexParams.bUseSpecular );
	EnablePixelSpecular( InVertexParams.bUseSpecular && InVertexParams.bUsePixelSpecular );
	SpecularColor = InVertexParams.SpecularColor;
	if( IsSpecularEnabled() )
	{
		Uniform3fv( SS_SpecularColor, 1, (const GLfloat *)&SpecularColor );
		Uniform1fv( SS_SpecularPower, 1, (const GLfloat *)&InVertexParams.SpecularPower );
	}

	VertexSettings.bUseDetailNormal = InVertexParams.bUseDetailNormal;
	VertexSettings.AmbientOcclusionSource = InVertexParams.AmbientOcclusionSource;

	//vertex movement
	EnableWaveVertexMovement( InVertexParams.bWaveVertexMovementEnabled );
	if( IsWaveVertexMovementEnabled() )
	{
		FLOAT WorldTime = (GCurrentTime >GStartTime) ? (GCurrentTime-GStartTime) : 0.0f;
		FLOAT TangentModifiedTime = InVertexParams.VertexMovementTangentFrequencyMultiplier*WorldTime;
		FLOAT VerticalModifiedTime = InVertexParams.VertexMovementVerticalFrequencyMultiplier*WorldTime;
		FVector VertexMovementConstants;

#if FLASH
		// we use the sin() instruction

		//Tangent Time
		VertexMovementConstants.X = appFractional(TangentModifiedTime)*(2*PI);
		//Max Amplitude
		VertexMovementConstants.Y = InVertexParams.MaxVertexMovementAmplitude;
		//Vertical Time
		VertexMovementConstants.Z = appFractional(VerticalModifiedTime)*(2*PI);
#else // FLASH
		// CheapWave() function requires different input
		// sin(w) ~= CheapWave(r / PIMul2) * 20.72f

		//Tangent Time
		VertexMovementConstants.X = appFractional(TangentModifiedTime);
		//Max Amplitude (*20.72f to use CheapWave() )
		VertexMovementConstants.Y = InVertexParams.MaxVertexMovementAmplitude * 20.72f;
		//Vertical Time
		VertexMovementConstants.Z = appFractional(VerticalModifiedTime);
#endif // FLASH

		//Send it down
		Uniform3fv( SS_VertexMovementConstants, 1, (const GLfloat *)&VertexMovementConstants );

		//sway is saved and assigned in SetMobileMeshVertexParams
		SwayTime = appFractional(InVertexParams.SwayFrequencyMultiplier*WorldTime)*2*PI;
		SwayMaxAngle = InVertexParams.SwayMaxAngle;
	}

	VertexSettings.bIsFogEnabledPerPrim = InVertexParams.bAllowFog;

	VertexSettings.bUseUniformColorMultiply = InVertexParams.bUseUniformColorMultiply;
	if (VertexSettings.bUseUniformColorMultiply)
	{
		Uniform4fv( SS_UniformMultiplyColor, 1, (const GLfloat *)&InVertexParams.UniformMultiplyColor );
	}
	VertexSettings.bUseVertexColorMultiply = InVertexParams.bUseVertexColorMultiply;

	VertexSettings.bUseLandscapeMonochromeLayerBlending = InVertexParams.bUseLandscapeMonochromeLayerBlending;

#if !FINAL_RELEASE || WITH_EDITOR
	MaterialName = InVertexParams.MaterialName;
#endif
}



/**
 * Sets up the material pixel shader state for GL system
 *
 * @param InPixelParams - A composite structure of the mobile pixel parameters needed by GL shaders
 */
void FES2ShaderManager::SetMobileMaterialPixelParams(const FMobileMaterialPixelParams& InPixelParams)
{
	PixelSettings.Reset();

	SetOpacitySource( InPixelParams.AlphaValueSource );
	EnableBumpOffset( InPixelParams.bBumpOffsetEnabled );

	// Cache off the bump offset reference values (used in SetProgramByType)
	BumpReferencePlane = InPixelParams.BumpReferencePlane;
	BumpHeightRatio = InPixelParams.BumpHeightRatio;

	// Set specular mask for per-vertex specular
	PixelSettings.SpecularMask = InPixelParams.SpecularMask;

	// Set the source for color multiplication (Vertex Color and Default Uniform)
	PixelSettings.ColorMultiplySource = InPixelParams.ColorMultiplySource;

	PixelSettings.EnvironmentBlendMode = InPixelParams.EnvironmentBlendMode;
	if( IsEnvironmentMappingEnabled() )
	{
		Uniform3fv( SS_EnvironmentColorScale, 1, (const GLfloat *)&InPixelParams.EnvironmentColorScale);
	}

	// Set the opacity multiplier, used to control the pixels' final opacity
	Uniform1fv(SS_MobileOpacityMultiplier, 1,  (const GLfloat *)&InPixelParams.OpacityMultiplier);

	if( InPixelParams.bUseLandscapeMonochromeLayerBlending )
	{
		Uniform3fv(SS_LandscapeMonochromeLayerColors, 4, (const GLfloat *)&InPixelParams.LandscapeMonochomeLayerColors[0]);
	}

#if !FINAL_RELEASE
	bNewMaterialSet = TRUE;
#endif
}


/**
 * Sets up vertex shader state for mesh parameters
 *
 * @param InMeshParams - A composite structure of the mobile mesh parameters needed by GL shaders
 */
void FES2ShaderManager::SetMobileMeshVertexParams(const FMobileMeshVertexParams& InMeshParams)
{
	MeshSettings.Reset();
	checkSlowish(InMeshParams.LocalToWorld);

	// First, cache off any useful values
	CameraPosition = InMeshParams.CameraPosition;
	ObjectPosition = InMeshParams.ObjectPosition;
	ObjectDistance = (ObjectPosition - CameraPosition).Size();
	ObjectBounds = InMeshParams.ObjectBounds;

	if( VertexSettings.bIsLightingEnabled || IsSpecularEnabled() )
	{
		// Cache off brightest light color, we'll combine this with the material specular later on
		// before sending to the shader
		BrightestLightColor = InMeshParams.BrightestLightColor;

		// Scale the RGB portion of the brightest color to avoid precision issues
		float BrightestLightColorMax =
			Max( Max( BrightestLightColor.R, BrightestLightColor.G ), BrightestLightColor.B );
		//unify range to avoid lowp clamping inconsistencies
		if (BrightestLightColorMax > 2.0f)
		{
			FLOAT TwoOverBrightestLightColorMax = 2.0f / BrightestLightColorMax;
			BrightestLightColor.R *= TwoOverBrightestLightColorMax;
			BrightestLightColor.G *= TwoOverBrightestLightColorMax;
			BrightestLightColor.B *= TwoOverBrightestLightColorMax;
		}

		FVector4 LightDirectionAndIsDirectional( -InMeshParams.BrightestLightDirection );
		LightDirectionAndIsDirectional.W = 1.0f;
		Uniform4fv( SS_LightDirection, 1, (const GLfloat *)&LightDirectionAndIsDirectional );

		// Light color and falloff (falloff not used on mobile yet)
		Uniform4fv( SS_LightColor, 1, (GLfloat*)&BrightestLightColor );

		if( IsSpecularEnabled() )
		{
			// Multiply the (cached) light color with this material's specular color
			FLinearColor LightColorTimesSpecularColor = InMeshParams.BrightestLightColor * SpecularColor;

			// Scale the RGB portion of the brightest color to avoid precision issues
			float LightColorTimesSpecularColorMax =
				Max( Max( LightColorTimesSpecularColor.R, LightColorTimesSpecularColor.G ), LightColorTimesSpecularColor.B );
			//unify range to avoid lowp clamping inconsistencies
			if (LightColorTimesSpecularColorMax > 2.0f)
			{
				FLOAT TwoOverLightColorTimesSpecularColorMax = 2.0f / LightColorTimesSpecularColorMax;
				LightColorTimesSpecularColor.R *= TwoOverLightColorTimesSpecularColorMax;
				LightColorTimesSpecularColor.G *= TwoOverLightColorTimesSpecularColorMax;
				LightColorTimesSpecularColor.B *= TwoOverLightColorTimesSpecularColorMax;
			}

			Uniform3fv( SS_LightColorTimesSpecularColor, 1, (const GLfloat *)&LightColorTimesSpecularColor );
		}
	}

	if( IsWaveVertexMovementEnabled() )
	{
		FLOAT SwayPositionalOffset = (InMeshParams.ObjectPosition | FVector(1.0, 1.0, 1.0)) /256.0;

		//treat position offset as random angle to rotation about
		FVector AxisOfRotation = InMeshParams.LocalToWorld->TransformNormal(FVector(0.0, 1.0, 0.0));
		AxisOfRotation.Normalize();
		//FVector VerticalAxis(0.0, 0.0, 1.0);
		//AxisOfRotation = AxisOfRotation.RotateAngleAxis(SwayPositionalOffset, VerticalAxis);

		//send down the sway transform
		FLOAT AmountOfSway = appSin(SwayTime + SwayPositionalOffset);
		FLOAT SwayDegrees = AmountOfSway*SwayMaxAngle*2*PI/360.0;
		FQuat SwayQuat(AxisOfRotation, SwayDegrees);
		FRotator SwayRotation(SwayQuat);
		FRotationMatrix SwayMatrix(SwayRotation);

		UniformMatrix4fv(SS_VertexSwayMatrix, 1, FALSE, (const GLfloat *)SwayMatrix.M);
	}

	MeshSettings.ParticleScreenAlignment = InMeshParams.ParticleScreenAlignment;
}


/**
 * Sets up pixel shader state for mesh parameters
 *
 * @param InMeshParams - A composite structure of the mobile mesh parameters needed by GL shaders
 */
void FES2ShaderManager::SetMobileMeshPixelParams(const FMobileMeshPixelParams& InMeshParams)
{
	// Set sky light color parameters based on whether sky lighting is allowed for this mesh
	// NOTE: These parameters are actually used in either the pixel or vertex shader, but we must
	// set them in SetMobileMeshPixelParams() as the engine is expecting to set DLE sky color
	// as pixel shader constants
	if( InMeshParams.bEnableSkyLight )
	{
		Uniform4fv(SS_UpperSkyColor, 1, (GLfloat*)&UpperSkyColor);
		Uniform4fv(SS_LowerSkyColor, 1, (GLfloat*)&LowerSkyColor);
	}
	else
	{
		// Zero out sky light influence
		Uniform4fv(SS_UpperSkyColor, 1, (GLfloat*)&FLinearColor::Black);
		Uniform4fv(SS_LowerSkyColor, 1, (GLfloat*)&FLinearColor::Black);
	}
}


/**
 * Set the active and bound texture
 *
 * @param TextureUnit The unit to bind the texture to
 * @param TextureName
 * @param TextureType
 * @param TextureFormat
 */
UBOOL GForceTextureBind = FALSE;
void FES2ShaderManager::SetActiveAndBoundTexture(UINT TextureUnit, UINT TextureName, UINT TextureType, UINT TextureFormat)
{
	UINT FinalTextureUnit = GetDeviceValidMobileTextureSlot(TextureUnit);
	
#if !FINAL_RELEASE
	checkSlowish(FinalTextureUnit < MAX_Mapped_MobileTexture);
	GStateShadow.LastUpdatePrimitive[FinalTextureUnit] = CurrentPrimitiveIndex;
#endif

	// If not the texture already bound to that texture unit
	if( GStateShadow.BoundTextureType[FinalTextureUnit] != TextureType ||
		GStateShadow.BoundTextureName[FinalTextureUnit] != TextureName ||
		GForceTextureBind)
	{
		// Save off value for next time
		GStateShadow.BoundTextureType[FinalTextureUnit] = TextureType;
		GStateShadow.BoundTextureName[FinalTextureUnit] = TextureName;

		if( GStateShadow.ActiveTexture != GL_TEXTURE0 + FinalTextureUnit )
		{
			GStateShadow.ActiveTexture = GL_TEXTURE0 + FinalTextureUnit;
			GLCHECK(glActiveTexture(GL_TEXTURE0 + FinalTextureUnit));
		}

		GLCHECK(glBindTexture(TextureType, TextureName));
		if( IsCurrentPrimitiveTracked() )
		{
			INC_DWORD_STAT_BY(STAT_BaseTextureBinds, TextureUnit == Base_MobileTexture ? 1 : 0 );
			INC_DWORD_STAT_BY(STAT_DetailTextureBinds, TextureUnit == Detail_MobileTexture ? 1 : 0 );
			INC_DWORD_STAT_BY(STAT_Detail2TextureBinds, TextureUnit == Detail_MobileTexture2 ? 1 : 0 );
			INC_DWORD_STAT_BY(STAT_Detail3TextureBinds, TextureUnit == Detail_MobileTexture3 ? 1 : 0 );
			INC_DWORD_STAT_BY(STAT_LightmapTextureBinds, TextureUnit == Lightmap_MobileTexture ? 1 : 0 );
			INC_DWORD_STAT_BY(STAT_NormalTextureBinds, TextureUnit == Normal_MobileTexture ? 1 : 0 );
			INC_DWORD_STAT_BY(STAT_EnvironmentTextureBinds, TextureUnit == Environment_MobileTexture ? 1 : 0 );
			INC_DWORD_STAT_BY(STAT_MaskTextureBinds, TextureUnit == Mask_MobileTexture ? 1 : 0 );
			INC_DWORD_STAT_BY(STAT_EmissiveTextureBinds, TextureUnit == Emissive_MobileTexture ? 1 : 0 );
		}
	}

	// update the shader manager with the format for this texture
	SetTextureFormat(FinalTextureUnit, (EPixelFormat)TextureFormat);
}

/**
 * Tells the shader manager what format of texture is in use in various texture units
 *
 * @param TextureUnit Which unit to assign a format to
 * @param Format Format of the texture bound to TextureUnit
 */
void FES2ShaderManager::SetTextureFormat(UINT TextureUnit, EPixelFormat Format)
{
	GStateShadow.BoundTextureFormat[TextureUnit] = Format;
#if FLASH
	if (Format == PF_DXT5)
	{
		GStateShadow.BoundTextureMask |= (1 << TextureUnit);
	}
	else
	{
		GStateShadow.BoundTextureMask &= ~(1 << TextureUnit);
	}
#endif
	
	// here we track if we've seen a lightmap texture (reseting after a draw call via 
	// ResetHasLightmapOnNextSetSamplerState())
	if (bWillResetHasLightmapOnNextSetSamplerState)
	{
		// reset our "has seen lightmap" flag
		bHasHadLightmapSet = FALSE;
		bHasHadDirectionalLightmapSet = FALSE;
		
		// don't do this again until requested
		bWillResetHasLightmapOnNextSetSamplerState = FALSE;
	}

	// TextureUnit 2 is the special texture unit for all lightmap textures
	if (TextureUnit == Lightmap_MobileTexture)
	{
		 bHasHadLightmapSet = TRUE;
	}

	if (TextureUnit == Lightmap2_MobileTexture)
	{
		bHasHadDirectionalLightmapSet = TRUE;
	}
}

/**
 * @return the versioned parameter information for the given parameter slot
 */
struct FVersionedShaderParameter& FES2ShaderManager::GetVersionedParameter(INT Slot)
{
	return VersionedShaderParameters[Slot];
}

void FES2ShaderManager::SetMobileTextureTransformOverride(TMatrix<3,3>& InOverrideTransform)
{
	bIsTextureCoordinateTransformOverriden = TRUE;
	UniformMatrix3fv(SS_TextureTransform, 1, FALSE, (const GLfloat *)InOverrideTransform.M);
}

/**
	* Next primitive is a distance field font 
	* @Param Params - all the variables needed for distance field fonts
	*/
void FES2ShaderManager::SetMobileDistanceFieldParams (const struct FMobileDistanceFieldParams& Params)
{
	FLOAT ShadowMultiplier = Params.EnableShadow ? 1.0 : 0.0;
	Uniform1fv(SS_DistanceFieldSmoothWidth, 1, (const GLfloat*)&Params.SmoothWidth);
	Uniform1fv(SS_DistanceFieldShadowMultiplier, 1, (const GLfloat*)&ShadowMultiplier);
	Uniform2fv(SS_DistanceFieldShadowDirection, 1, (const GLfloat*)&Params.ShadowDirection);
	Uniform4fv(SS_DistanceFieldShadowColor, 1, (const GLfloat*)&Params.ShadowColor);
	Uniform1fv(SS_DistanceFieldShadowSmoothWidth,  1, (const GLfloat*)&Params.ShadowSmoothWidth);

	FLOAT GlowMultiplier = Params.GlowInfo.bEnableGlow ? 1.0 : 0.0;
	Uniform1fv(SS_DistanceFieldGlowMultiplier, 1, (const GLfloat*)&GlowMultiplier);
	Uniform4fv(SS_DistanceFieldGlowColor, 1, (const GLfloat*)&Params.GlowInfo.GlowColor);
	Uniform2fv(SS_DistanceFieldGlowOuterRadius, 1, (const GLfloat*)&Params.GlowInfo.GlowOuterRadius);
	Uniform2fv(SS_DistanceFieldGlowInnerRadius, 1, (const GLfloat*)&Params.GlowInfo.GlowInnerRadius);

	SetAlphaTest(TRUE, Params.AlphaRefVal);

	bIsDistanceFieldFont = TRUE;
}


void ResetCurrentProgram()
{
	FES2ShaderProgram::CurrentProgram = 0;
}



/**
 * Allows the engine to tell the this RHI what global shader to use next
 * 
 * @param GlobalShaderType An enum value for the global shader type
 */
void MobileSetNextDrawGlobalShader( EMobileGlobalShaderType GlobalShaderType )
{
	GShaderManager.SetNextDrawGlobalShader( GlobalShaderType );
}


#endif
