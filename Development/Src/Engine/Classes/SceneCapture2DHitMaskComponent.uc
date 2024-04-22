/**
 * SceneCapture2DHitMaskComponent
 *
 * Allows owner's skeletal mesh capture to a 2D texture render target with mask information
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SceneCapture2DHitMaskComponent extends SceneCaptureComponent
	native;

/** render target resource to set as target for capture */
var transient const TextureRenderTarget2D TextureTarget;

/** mesh to render to target resource - Owner's SkeletalMesh**/
var transient const SkeletalMeshComponent SkeletalMeshComp;

/** Which section to render for mask **/
var int     MaterialIndex;

/** Which LOD to force - -1 == use current**/
/** Other LOD hasn't been tested and there could be issue with bone transform not matching up **/
/** To do this properly we need to use ForcedLOD system of SkeletalMeshComponent **/
var int     ForceLOD;

/** Hit Mask Cull Distance. If a point is further than this distance, it will ignore **/
var int     HitMaskCullDistance;

/** Fading related variable **/
/** Fading start time after hit - in second - by default 10 seconds 
 *  Infinite if less than zero 
 **/
var float   FadingStartTimeSinceHit;
/** What % of color to apply - Range of 0 to 1 **/
var float   FadingPercentage;
/** Fading duration time since fading starts - in second **/
var float   FadingDurationTime;
/** Fading interval - in second **/
var float   FadingIntervalTime;

cpptext
{
protected:

	// UActorComponent interface

	/**
	* Attach a new 2d capture component
	*/
	virtual void Attach();

public:

	/**
	* Constructor
	*/
	USceneCapture2DHitMaskComponent()
		{}

	// SceneCaptureComponent interface

	/**
	* Create a new probe with info needed to render the scene
	*/
	virtual class FSceneCaptureProbe* CreateSceneCaptureProbe();

	/** Set Capture Parameters 
	 *  This sent message to render thread with parameter information
	 *  Render thread will modify the texture
	 */
	void SetCaptureParameters( const FVector & InMaskPosition, const FLOAT InMaskRadius, const FVector& InStartupPosition, const UBOOL bOnlyWhenFacing );
	/** Set Fading Start Time Since Hit
	 *  Will update the value in render thread - SceneProbe
	 */
	void SetFadingStartTimeSinceHit( const FLOAT InFadingStartTimeSinceHit );
	/**
	 * Sets the ParentToWorld transform the component is attached to.
	 * @param ParentToWorld - The ParentToWorld transform the component is attached to.
	 */
	virtual void SetParentToWorld(const FMatrix& ParentToWorld);

	/** When parent transforms, to prevent super behavior of re-attaching **/
	virtual void UpdateTransform();

}

/** interface for changing target texture */
native noexport final function SetCaptureTargetTexture( const TextureRenderTarget2D InTextureTarget );
/** interface for changing parameters */
native noexport final function SetCaptureParameters( const vector InMaskPosition, const float InMaskRadius, const vector InStartupPosition, const bool bOnlyWhenFacing );
/** interface for changing fading parameter. Negative will disable it. */
native noexport final function SetFadingStartTimeSinceHit( const float InFadingStartTimeSinceHit );

defaultproperties
{
	MaterialIndex = 0
	ForceLOD = -1
	FadingStartTimeSinceHit = 10.f
	FadingPercentage = 0.99f
	FadingDurationTime = 50.f
	FadingIntervalTime = 3.f

	HitMaskCullDistance = 100.f
}
