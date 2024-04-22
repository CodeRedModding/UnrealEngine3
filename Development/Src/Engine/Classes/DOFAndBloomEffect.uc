/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Depth of Field post process effect
 *
 */
class DOFAndBloomEffect extends DOFEffect
	native;

/** A scale applied to blooming colors. */
var(Bloom) float BloomScale;

/** Any component of a pixel's color must be larger than this to contribute bloom. */
var(Bloom) float BloomThreshold;

/** Multiplies against the bloom color. */
var(Bloom) color BloomTint;

/** 
 * Scene color luminance must be less than this to receive bloom. 
 * This behaves like Photoshop's screen blend mode and prevents over-saturation from adding bloom to already bright areas.
 * The default value of 1 means that a pixel with a luminance of 1 won't receive any bloom, but a pixel with a luminance of .5 will receive half bloom.
 */
var(Bloom) float BloomScreenBlendThreshold;

/** A multiplier applied to all reads of scene color. */
var deprecated float SceneMultiplier;

/** the radius of the bloom effect 0..64 */
var(Bloom) float BlurBloomKernelSize;

var deprecated bool bEnableReferenceDOF;

/**
 * Allows to specify the depth of field type. Choose depending on performance and quality needs.
 * "SimpleDOF" blurs the out of focus content and recombines that with the unblurred scene (fast, almost constant speed).
 * "ReferenceDOF" makes use of dynamic branching in the pixel shader and features circular Bokeh shape effects (slow for big Kernel Size).
 * "BokehDOF" allows to specify a Bokeh texture and a bigger radius (requires D3D11, slow when using a lot of out of focus content)
 */
var(DepthOfField) enum EDOFType
{
	DOFType_SimpleDOF<DisplayName=SimpleDOF>, 
	DOFType_ReferenceDOF<DisplayName=ReferenceDOF>, 
	DOFType_BokehDOF<DisplayName=BokehDOF>, 
} DepthOfFieldType;

/**
 * Allows to specify the quality of the chose Depth of Field Type.
 * This meaning depends heavily on the current implementation and that might change.
 * If performance is important the lowest acceptable quality should be used.
 */
var(DepthOfField) enum EDOFQuality
{
	DOFQuality_Low<DisplayName=Low>, 
	DOFQuality_Medium<DisplayName=Medium>, 
	DOFQuality_High<DisplayName=High>, 
} DepthOfFieldQuality;

/** only used if BokehDOF is enabled */
var(DepthOfField) Texture2D BokehTexture;

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
	
	// UObject interface

	/**
	* Called after this instance has been serialized.  UberPostProcessEffect should only
	* ever exists in the SDPG_PostProcess scene
	*/
	virtual void PostLoad();

	/**
	* This allows to print a warning when the effect is used.
	*/
	virtual void OnPostProcessWarning(FString& OutWarning) const
	{
		OutWarning = TEXT("Warning: DOFAndBloom should no longer be used, use Uberpostprocess instead.");
	}
}

defaultproperties
{
	BloomScale=1.0
	BloomThreshold=1.0
	BloomTint=(R=255,G=255,B=255)
	BloomScreenBlendThreshold=10
	BlurKernelSize=16.0
	BlurBloomKernelSize=16.0
	bEnableReferenceDOF=false
}