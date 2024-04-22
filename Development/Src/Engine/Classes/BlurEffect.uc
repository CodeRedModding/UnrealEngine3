/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Blur post process effect
 *
 */
class BlurEffect extends PostProcessEffect
	native;

/** Distance to blur in pixels */
var() int BlurKernelSize;

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
	BlurKernelSize=2
}