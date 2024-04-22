/**
 * SceneCaptureReflectComponent
 *
 * Captures the reflection of the current view to a
 * 2D texture render target.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SceneCaptureReflectComponent extends SceneCaptureComponent
	native;

/** render target resource to set as target for capture */
var(Capture) TextureRenderTarget2D TextureTarget;
/** scale field of view so that there can be some overdraw */
var(Capture) float ScaleFOV;

cpptext
{
public:

	// UActorComponent interface

	/**
	* Attach a new reflect capture component
	*/
	virtual void Attach();

	// SceneCaptureComponent interface

	/**
	* Create a new probe with info needed to render the scene
	*/
	virtual class FSceneCaptureProbe* CreateSceneCaptureProbe();
}

defaultproperties
{
	ScaleFOV=1.f
	FrameRate=1000
}
