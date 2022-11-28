/*
	Camera: defines the Point of View of a player in world space.
	---
	Copyright 2004 Epic Games, Inc. All Rights Reserved.
	Created by Laurent Delayen
*/

class Camera extends Actor
	notplaceable
	native;

cpptext
{
	void CheckViewTarget( FsViewTarget* VT );
	void SetViewTarget( AActor* NewViewTarget );
	AActor* GetViewTarget();
}

var		PlayerController	PCOwner;		// PlayerController Owning this Camera Actor
var()	float				fBlendTime;		// Blend time between 2 view targets

// !!LD CAM
var					Name 			CameraStyle;

/** default FOV */
var()	float	CamFOV;

/** true if FOV is locked */
var		bool	bLockedFOV;

/** If we should insert black areas when rendering the scene to ensure an aspect ratio of ConstrainedAspectRatio */
var		bool	bConstrainAspectRatio;

/** If we should apply FadeColor/FadeAmount to the screen. */
var		bool	bEnableFading;

/** value FOV is locked at */
var		float	LockedFOV;

/** If bConstrainAspectRatio is true, add black regions to ensure aspect ratio is this. Ratio is horizontal/vertical. */
var		float	ConstrainedAspectRatio;

/** Color to fade to. */
var		color	FadeColor;

/** Amount of fading to apply. */
var		float	FadeAmount;

/* View Target definition 
	A View Target is responsible for providing the Camera with an ideal Point of View (POV) */
struct native sViewTarget
{
	var	const	Actor		Target;				// Target Actor, provides ideal POV
	var	const	Controller	TargetController;	// Controller of Target	(only for non Locally controlled Pawns)
	var const	Vector		LastLocation;		// Last Target Location
	var const	Rotator		LastRotation;		// Last Target Rotation
};
var const	sViewTarget		primaryVT;		// Current (primary) ViewTarget

/* Caching Camera, for optimization */
struct native sCameraCache
{
	var const float		LastTimeStamp;		// Last Time Stamp
	var	const vector	LastLocation;		// Last know Cam Location
	var	const Rotator	LastRotation;		// Last know Cam Rotation
};
var	sCameraCache	CameraCache;


/** List of camera modifiers to apply during update of camera position/ rotation */
var array<CameraModifier>	ModifierList;


/** 
 * Initialize Camera for associated PlayerController 
 *
 * @param	myPC	PlayerController attached to this Camera.
 */
function InitializeFor( PlayerController myPC )
{
	PCOwner = myPC;
	SetViewTarget( PCOwner );
}

event Destroyed()
{
	local int ModifierIdx;

	super.Destroyed();

	// Loop through modifier list and delete each modifier object
	// Then empty the list
	for( ModifierIdx = 0; ModifierIdx < ModifierList.Length; ModifierIdx++ ) 
	{
//		delete ModifierList[ModifierIdx];
	}
	ModifierList.Length = 0;
}


/**
 * returns player's FOV angle 
 */
function float GetFOVAngle()
{
	if( bLockedFOV )
	{
		return LockedFOV;
	}

	return CamFOV;
}

/**
 * Lock FOV to a specific value, for debugging 
 */
function SetFOV( float newFOV )
{
	if( NewFOV < 1 || NewFOV > 170 )
	{
		bLockedFOV = false;
		return;
	}

	bLockedFOV	= true;
	LockedFOV	= NewFOV;
}


/** 
 * Master function to retrieve Camera's actual view point.
 * If it needs to, camera will update itself.
 * do not call this directly, call PlayerController::GetPlayerViewPoint() instead.
 * @todo	add camera caching optimization.
 *
 * @output	out_CamLoc	Camera Location
 * @output	out_CamRot	Camera Rotation
 */
final function GetCameraViewPoint( out vector out_CamLoc, out rotator out_CamRot )
{
	local float fDeltaTime;

	out_CamLoc = CameraCache.LastLocation;
	out_CamRot = CameraCache.LastRotation;

	// Check if camera should be updated, otherwise return cached values.
	if( !ShouldUpdateCamera() )
	{
		return;
	}

	// Delta since last camera update
	fDeltaTime = Level.TimeSeconds - CameraCache.LastTimeStamp;
	UpdateCamera( fDeltaTime, out_CamLoc, out_CamRot );

	FillCameraCache( out_CamLoc, out_CamRot );
}

/** Return true if camera should be updated. Otherwise return cached values */
final function bool ShouldUpdateCamera()
{
	local vector	POVLoc;
	local rotator	POVRot;

	// Update at least once per frame
	// (also forced when a new viewtarget has been set)
	if( CameraCache.LastTimeStamp != Level.TimeSeconds )
	{
		return true;
	}

	// If primary target has moved, then update
	if( primaryVT.Target != None )
	{
		primaryVT.Target.GetActorEyesViewPoint( POVLoc, POVRot);

		if( primaryVT.LastLocation != POVLoc ||
			primaryVT.LastRotation != POVRot )
		{
			return true;
		}
	}

	return false;
}

/**
 * Cache camera, for optimization. 
 */
native final function FillCameraCache( vector NewCamLoc, rotator NewCamRot );

/**
 * Main Camera Updating function... Queries ViewTargets and updates itself 
 *
 * @param	fDeltaTime	Delta Time since last camera update (in seconds).
 * @input	out_CamLoc	Camera Location of last update, to be updated with new location.
 * @input	out_CamRot	Camera Rotation of last update, to be updated with new rotation.
 */
function UpdateCamera( float fDeltaTime, out vector out_CamLoc, out rotator out_CamRot )
{
	local Vector	Pos;
	local name		CameraMode;
	local vector	HitLocation, HitNormal;
	local Actor		HitActor;

	// Make sure we have a valid target
	if ( primaryVT.Target == None )
	{
		log("Camera::UpdateCamera primaryVT.Target == None");
		return;
	}

	// Viewing through a camera actor.
	if ( CameraActor(primaryVT.Target) != None )
	{
		CameraActor(primaryVT.Target).GetCameraView( fDeltaTime, out_CamLoc, out_CamRot, CamFOV );

		// Grab aspect ratio info from the CameraActor.
		bConstrainAspectRatio = CameraActor(primaryVT.Target).bConstrainAspectRatio;
		ConstrainedAspectRatio = CameraActor(primaryVT.Target).AspectRatio;

		return;
	}
	else
	{
		bConstrainAspectRatio = false;
		CamFOV = default.CamFOV;
	}

	CameraMode = CameraStyle;
	if ( CameraMode == 'FirstPerson' )
	{	
		// Simple first person view implementation. View from view target's 'eyes' view point.
		primaryVT.Target.GetActorEyesViewPoint( out_CamLoc, out_CamRot );
	}
	else if ( CameraMode == 'Fixed' )				
	{	
		// Simple Fixed camera mode, do not update camera.
	}
	else if ( CameraMode == 'FixedTracking' )			
	{
		// Fixed Camera, but track view target's eyes location
		primaryVT.Target.GetActorEyesViewPoint( Pos, out_CamRot );
		out_CamRot = Rotator(Pos - out_CamLoc);
	}
	else if ( CameraMode == 'ThirdPerson' || CameraMode == 'FreeCam' )
	{
		// Simple third person view implementation
		primaryVT.Target.GetActorEyesViewPoint( out_CamLoc, out_CamRot );
		Pos = out_CamLoc - Vector(out_CamRot) * 256;
		HitActor = Trace( HitLocation, HitNormal, Pos, out_CamLoc, false, vect(0,0,0) );
		if ( HitActor != None )
		{
			out_CamLoc = HitLocation + HitNormal*2;
		}
		else
		{
			out_CamLoc = Pos;
		}
	}

	// If Pawn provides own camera view, let it handle that
	else if ( Pawn(primaryVT.Target) != None && Pawn(primaryVT.Target).PawnCalcCamera(fDeltaTime, out_CamLoc, out_CamRot, CamFOV) )
	{
		return;
	}
	else
	{
		primaryVT.Target.GetActorEyesViewPoint( out_CamLoc, out_CamRot );
	}
}

/**
 * Set a new ViewTarget with optional BlendTime 
 */
native final function SetViewTarget( Actor NewViewTarget );

/** 
 * Allow each modifier a chance to change view rotation/deltarot
 */
function ProcessViewRotation( float DeltaTime, out rotator out_ViewRotation, out Rotator out_DeltaRot )
{
	local int ModifierIdx;

	for( ModifierIdx = 0; ModifierIdx < ModifierList.Length; ModifierIdx++ )
	{
		if( ModifierList[ModifierIdx] != None ) 
		{
			if( ModifierList[ModifierIdx].ProcessViewRotation( primaryVT.Target, DeltaTime, out_ViewRotation, out_DeltaRot ) )
			{
				break;
			}
		}
	}
}


/** 
 * list important Camera variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
  * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local Vector	EyesLoc;
	local Rotator	EyesRot;
	local Canvas Canvas;

	Canvas = HUD.Canvas;

	Canvas.SetDrawColor(255,255,255);

	Canvas.DrawText("++ Camera Mode:" $ CameraStyle @ "ViewTarget:"$primaryVT.Target @ "CamFOV:" $ CamFOV );
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);

	Canvas.DrawText("   CamLoc:"$CameraCache.LastLocation @ "CamRot:"$CameraCache.LastRotation);
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);

	if ( primaryVT.Target != None )
	{
		primaryVT.Target.GetActorEyesViewPoint( EyesLoc, EyesRot );
		Canvas.DrawText("   EyesLoc:"$EyesLoc @ "EyesRot:"$EyesRot);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
	}
}

defaultproperties
{
	CollisionComponent=None
	Components.Remove(CollisionCylinder)
	Components.Remove(Sprite)
	CamFOV=90.f
	bHidden=true
	RemoteRole=ROLE_None
}