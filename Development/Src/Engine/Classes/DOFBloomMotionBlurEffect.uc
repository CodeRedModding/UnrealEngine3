/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DOFBloomMotionBlurEffect extends DOFAndBloomEffect
	native;

/** Maximum blur velocity amount. This is a clamp on the amount of blur. */
var(MotionBlur) float MaxVelocity;

/** This is a scale that could be considered as the "sensitivity" of the blur. */
var(MotionBlur) float MotionBlurAmount;

/** Whether everything (static/dynamic objects) should motion blur or not. If disabled, only moving objects may blur. */
var(MotionBlur) bool FullMotionBlur;

/** Threshhold for when to turn off motion blur when the camera rotates swiftly during a single frame (in degrees). */
var(MotionBlur) float CameraRotationThreshold;

/** Threshhold for when to turn off motion blur when the camera translates swiftly during a single frame (in world units). */
var(MotionBlur) float CameraTranslationThreshold;

cpptext
{
    // UPostProcessEffect interface

	/**
	 * Creates a proxy to represent the render info for a post process effect
	 * @param WorldSettings - The world's post process settings for the view.
	 * @return The proxy object.
	 */
	virtual class FPostProcessSceneProxy* CreateSceneProxy(const FPostProcessSettings* WorldSettings);

	/**
	 * @param View - current view
	 * @return TRUE if the effect should be rendered
	 */
	virtual UBOOL IsShown(const FSceneView* View) const;
	
	/**
	* This allows to print a warning when the effect is used.
	*/
	virtual void OnPostProcessWarning(FString& OutWarning) const
	{
		OutWarning = TEXT("Warning: DOFAndBloomAndMotionBlur should no longer be used, use Uberpostprocess instead.");
	}
}

defaultproperties
{
	MotionBlurAmount = 0.5f;
	MaxVelocity      = 1.0f;
	FullMotionBlur = true;
	CameraRotationThreshold = 90.0f;
	CameraTranslationThreshold = 10000.0f;
	bShowInEditor	 = false;
}
