/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class GameCameraBase extends Object
	abstract
	native(Camera)
	config(Camera);

var transient GamePlayerCamera	PlayerCamera;

/** resets camera interpolation. Set on first frame and teleports to prevent long distance or wrong camera interpolations. */
var transient bool				bResetCameraInterpolation;


/** Called when the camera becomes active */
function OnBecomeActive( GameCameraBase OldCamera );
/** Called when the camera becomes inactive */
function OnBecomeInActive( GameCameraBase NewCamera );

/** Called to indicate that the next update should skip interpolation and snap to desired values. */
function ResetInterpolation()
{
	bResetCameraInterpolation = TRUE;
}

/** Expected to fill in OutVT with new camera pos/loc/fov. */
function UpdateCamera(Pawn P, GamePlayerCamera CameraActor, float DeltaTime, out TViewTarget OutVT);

function ProcessViewRotation( float DeltaTime, Actor ViewTarget, out Rotator out_ViewRotation, out Rotator out_DeltaRot );

simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos);

function Init();

event ModifyPostProcessSettings(out PostProcessSettings PP);

defaultproperties
{
}
