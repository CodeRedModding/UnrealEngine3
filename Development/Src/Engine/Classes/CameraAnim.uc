
/**
 *	CameraAnim: defines a pre-packaged animation to be played on a camera.
 * 	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CameraAnim extends Object
	notplaceable
	native(Camera);

/** The InterpGroup that holds our actual interpolation data. */
var InterpGroupCamera	    CameraInterpGroup;
/** This is to preview and they only exists in editor */
var editoronly transient InterpGroup	PreviewInterpGroup;

/** Length, in seconds. */
var const float		AnimLength;

/** AABB in local space. */
var const box		BoundingBox;

/** The "base" postprocess settings to use, to support non-animating settings. */
var const PostProcessSettings	BasePPSettings;
var const float					BasePPSettingsAlpha;

/** The */
var const float		BaseFOV;

cpptext
{
protected:
	void CalcLocalAABB();

public:
	/** Overridden to calculate the bbox at save time. */
	virtual void PreSave();
	virtual void PostLoad();
	INT GetResourceSize();

	UBOOL CreateFromInterpGroup(class UInterpGroup* SrcGroup, class USeqAct_Interp* Interp);
	FBox GetAABB(FVector const& BaseLoc, FRotator const& BaseRot, FLOAT Scale) const;

	UBOOL InitializeCamera(class UInterpGroup* SrcGroup, class USeqAct_Interp* Interp);
};

defaultproperties
{
	AnimLength=3.f
	BaseFOV=95

	BasePPSettingsAlpha=1.f

	// override nothing unless explicitly chosen
	BasePPSettings={(
		bOverride_EnableBloom=FALSE,
		bOverride_EnableDOF=FALSE,
		bOverride_EnableMotionBlur=FALSE,
		bOverride_EnableSceneEffect=FALSE,
		bOverride_AllowAmbientOcclusion=FALSE,
		bOverride_OverrideRimShaderColor=FALSE,
		bOverride_Bloom_Scale=FALSE,
		bOverride_Bloom_Threshold=FALSE,
		bOverride_Bloom_Tint=FALSE,
		bOverride_Bloom_ScreenBlendThreshold=FALSE,
		bOverride_Bloom_InterpolationDuration=FALSE,
		bOverride_DOF_FalloffExponent=FALSE,
		bOverride_DOF_BlurKernelSize=FALSE,
		bOverride_DOF_BlurBloomKernelSize=FALSE,
		bOverride_DOF_MaxNearBlurAmount=FALSE,
		bOverride_DOF_MaxFarBlurAmount=FALSE,
		bOverride_DOF_FocusType=FALSE,
		bOverride_DOF_FocusInnerRadius=FALSE,
		bOverride_DOF_FocusDistance=FALSE,
		bOverride_DOF_FocusPosition=FALSE,
		bOverride_DOF_InterpolationDuration=FALSE,
		bOverride_MotionBlur_MaxVelocity=FALSE,
		bOverride_MotionBlur_Amount=FALSE,
		bOverride_MotionBlur_FullMotionBlur=FALSE,
		bOverride_MotionBlur_CameraRotationThreshold=FALSE,
		bOverride_MotionBlur_CameraTranslationThreshold=FALSE,
		bOverride_MotionBlur_InterpolationDuration=FALSE,
		bOverride_Scene_Desaturation=FALSE,
		bOverride_Scene_TonemapperScale=FALSE,
		bOverride_Scene_ImageGrainScale=FALSE,
		bOverride_Scene_HighLights=FALSE,
		bOverride_Scene_MidTones=FALSE,
		bOverride_Scene_Shadows=FALSE,
		bOverride_Scene_InterpolationDuration=FALSE,
		bOverride_RimShader_Color=FALSE,
		bOverride_RimShader_InterpolationDuration=FALSE,
	)}
}
