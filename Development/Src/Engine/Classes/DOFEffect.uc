/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Depth of Field post process effect
 *
 */
class DOFEffect extends PostProcessEffect
	native
	abstract;

/** exponent to apply to blur amount after it has been normalized to [0,1] */
var(DepthOfField) float FalloffExponent;
/** affects the radius of the DepthOfField bohek / how blurry the scene gets */
var(DepthOfField) float BlurKernelSize;
/** [0,1] value for clamping how much blur to apply to items in front of the focus plane */
var(DepthOfField, BlurAmount) float MaxNearBlurAmount<DisplayName=MaxNear>;
/** [0,1] value for clamping how much blur to apply */
var(DepthOfField, BlurAmount) float MinBlurAmount<DisplayName=Min>;
/** [0,1] value for clamping how much blur to apply to items behind the focus plane */
var(DepthOfField, BlurAmount) float MaxFarBlurAmount<DisplayName=MaxFar>;

/** control how the focus point is determined */
var(DepthOfField) enum EFocusType
{
	// use distance from the view
	FOCUS_Distance,
	// use a world space point
	FOCUS_Position	
} FocusType;
/** inner focus radius */
var(DepthOfField) float FocusInnerRadius;
/** used when FOCUS_Distance is enabled */
var(DepthOfField) float FocusDistance;
/** used when FOCUS_Position is enabled */
var(DepthOfField) vector FocusPosition;

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

	// UObject inteface

	/** callback for changed property */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
}

defaultproperties
{
	// typical settings
	FocusType=FOCUS_Distance
	FocusDistance=800
	FocusInnerRadius=400
	FalloffExponent=2
	BlurKernelSize=2
	MaxNearBlurAmount=1
	MinBlurAmount=0
	MaxFarBlurAmount=1
}