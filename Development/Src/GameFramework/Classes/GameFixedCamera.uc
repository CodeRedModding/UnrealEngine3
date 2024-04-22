/**
 * "Fixed" camera mode.  Views through a CameraActor in the level.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class GameFixedCamera extends GameCameraBase
	config(Camera);

/** FOV to fall back to if we can't get one from somewhere else. */
var() const protected float DefaultFOV;

simulated function UpdateCamera(Pawn P, GamePlayerCamera CameraActor, float DeltaTime, out TViewTarget OutVT)
{
	local CameraActor CamActor;

	// are we looking at a camera actor?
	CamActor = CameraActor(OutVT.Target);

	if (CamActor != None)
	{
		// we're attached to a camactor, use it's FOV
		OutVT.POV.FOV = CamActor.FOVAngle;
	}
	else
	{
		OutVT.POV.FOV = DefaultFOV;
	}

	// copy loc/rot from actor we're attached to
	if (OutVT.Target != None)
	{
		OutVT.POV.Location = CamActor.Location;
		OutVT.POV.Rotation = CamActor.Rotation;
	}

	// cameraanims, etc
	PlayerCamera.ApplyCameraModifiers(DeltaTime, OutVT.POV);

	// if we had to reset camera interpolation, then turn off flag once it's been processed.
	bResetCameraInterpolation = FALSE;
}

/** Called when Camera mode becomes active */
function OnBecomeActive( GameCameraBase OldCamera )
{
	// this will cause us to always snap to fixed cameras
	bResetCameraInterpolation = TRUE;

	super.OnBecomeActive( OldCamera );
}


defaultproperties
{
	DefaultFOV=80
}