/**
 * SceneCaptureCubeMapComponent
 *
 * Allows a scene capture to up to 6 2D texture render targets
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SceneCaptureCubeMapComponent extends SceneCaptureComponent
	native;

/** texture targets for the six cubemap faces */
var(Capture) TextureRenderTargetCube TextureTarget;
/** near plane clip distance */
var(Capture) float NearPlane;
/** far plane clip distance */ 
var(Capture) float FarPlane;

/** world location based on parent transform */
var private const transient native Vector WorldLocation;

cpptext 
{
protected:

	// UActorComponent interface.

	/**
	* Attach a new cube capture component
	*/
	virtual void Attach();

	/**
	 * Sets the ParentToWorld transform the component is attached to.
	 * @param ParentToWorld - The ParentToWorld transform the component is attached to.
	 */
	virtual void SetParentToWorld(const FMatrix& ParentToWorld);

public:
	
	// SceneCaptureComponent interface

	/**
	* Create a new probe with info needed to render the scene
	*/
	virtual class FSceneCaptureProbe* CreateSceneCaptureProbe();
}

defaultproperties
{
	NearPlane=20
	FarPlane=500
}
