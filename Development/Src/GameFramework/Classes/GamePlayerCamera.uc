/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GamePlayerCamera extends Camera
	config(Camera)
	native(Camera);

// NOTE FOR REFERENCE
// >> IS LOCAL->WORLD (no transpose)
// << IS WORLD->LOCAL (has the transpose)

/**
 *  Architectural overview
 *  GamePlayerCamera is an override of the Engine camera, and is the master camera that the player actually looks through.  This camera
 *      will lean on whatever GameCameraBase(s) it is currently using to calc the final camera properties.
 *  GameCameraBase is a base class for defining specific camera algorithms (e.g. fixed, specatator, first person, third person).  Certain
 *      instances of these can contain another layer of "modes", e.g. ... 
 *  GameThirdPersonCameraModeBase is a base class for defining specific variants of the GameThirdPersonCamera.
 *  
 *  */


/////////////////////
// Camera Modes
/////////////////////

/** Implements typical third person camera. */
var(Camera) editinline transient GameCameraBase			ThirdPersonCam;
/** Class to use for third person camera. */
var(Camera) protected const  class<GameCameraBase>      ThirdPersonCameraClass;

/** Implements fixed camera, used for viewing through pre-placed camera actors. */
var(Camera) editinline transient GameCameraBase			FixedCam;
/** Class to use for third person camera. */
var(Camera) protected const  class<GameCameraBase>      FixedCameraClass;


/** Which camera is currently active. */
var(Camera) editinline transient GameCameraBase	CurrentCamera;


/////////////////////
// FOV Overriding
/////////////////////

/** Should the FOV be overridden? */
var transient bool		bUseForcedCamFOV;
/** If bUseForcedCamFOV is true, use this angle */
var transient float		ForcedCamFOV;


/////////////////////
// Interpolation
/////////////////////

var transient bool						bInterpolateCamChanges;

var transient private Actor				LastViewTarget;

/** Indicates if we should reset interpolation on whichever active camera processes next. */
var transient private bool				bResetInterp;


/////////////////////
// Camera Shakes
/////////////////////

/** Scalar applied to all screen shakes in splitscreen. Normally used to dampen, since shakes feel more intense in a smaller viewport. */
var() protected const float SplitScreenShakeScale;


///////////////////////
//// Shaky-cam management
///////////////////////

///** The pawn that was last used to cache the animnodes used for shakycam.  Changing viewtarget pawns will trigger a re-cache. */
//var private transient Pawn						ShakyCamAnimNodeCachePawn;
///** AnimNode names for the "standing idle" camera animation. */
//var() private const array<name>					StandingIdleSequenceNodeNames;
///** Cached refs to the standing idle animations. */
//var private transient array<AnimNodeSequence>	StandingIdleSequenceNodes;


/////////////////////
// Etc
/////////////////////

// dealing with situations where camera target is based on another actor
var transient protected Actor LastTargetBase;
var transient protected matrix LastTargetBaseTM;


cpptext
{
	virtual void AddPawnToHiddenActorsArray( APawn *PawnToHide );
	virtual void RemovePawnFromHiddenActorsArray( APawn *PawnToHide );
	virtual void ModifyPostProcessSettings(FPostProcessSettings& PPSettings) const;
};


/**
 * Internal. Creates and initializes a new camera of the specified class, returns the object ref.
 */
protected function GameCameraBase CreateCamera(class<GameCameraBase> CameraClass)
{
	local GameCameraBase NewCam;
	NewCam = new(Outer) CameraClass;
	NewCam.PlayerCamera = self;
	NewCam.Init();
	return NewCam;
}

protected native function CacheLastTargetBaseInfo(Actor TargetBase);


function PostBeginPlay()
{
	super.PostBeginPlay();

	// Setup camera modes
	if ( (ThirdPersonCam == None) && (ThirdPersonCameraClass != None) )
	{
		ThirdPersonCam = CreateCamera(ThirdPersonCameraClass);
	}
 	if ( (FixedCam == None) && (FixedCameraClass != None) )
 	{
 		FixedCam = CreateCamera(FixedCameraClass);
 	}
}

// reset the camera to a good state
function Reset()
{
	bUseForcedCamFOV = false;
}


/**
 * Internal.  Polls game state to determine best camera to use.
 */
protected function GameCameraBase FindBestCameraType(Actor CameraTarget)
{
 	local GameCameraBase BestCam;

	// if not using a game-specific camera (i.e. not 'default'), we'll let the engine handle it
	if (CameraStyle == 'default')
	{
		if (CameraActor(CameraTarget) != None)
		{
			// if attached to a CameraActor and not spectating, use Fixed
			BestCam = FixedCam;
		}
		else 
 		{
			BestCam = ThirdPersonCam;
 		}
	}

	return BestCam;
}


/**
 * Available for overriding.
 */
function bool ShouldConstrainAspectRatio()
{
	return FALSE;
}

/**
 * Query ViewTarget and outputs Point Of View.
 *
 * @param	OutVT		ViewTarget to use.
 * @param	DeltaTime	Delta Time since last camera update (in seconds).
 */
function UpdateViewTarget(out TViewTarget OutVT, float DeltaTime)
{
	local Pawn P;
	local GameCameraBase		NewCamera;
    local CameraActor CamActor;

	// Don't update outgoing viewtarget during an interpolation 
	if( PendingViewTarget.Target != None && OutVT == ViewTarget && BlendParams.bLockOutgoing )
	{
		return;
	}

	// Make sure we have a valid target
	if( OutVT.Target == None )
	{
		`log("Camera::UpdateViewTarget OutVT.Target == None");
		return;
	}

	P = Pawn(OutVT.Target);

	// decide which camera to use
	NewCamera = FindBestCameraType(OutVT.Target);

	// handle a switch if necessary
	if (CurrentCamera != NewCamera)
	{
		if (CurrentCamera != None)
		{
			CurrentCamera.OnBecomeInActive( NewCamera );
		}

		if (NewCamera != None)
		{
			NewCamera.OnBecomeActive( CurrentCamera );
		}

		CurrentCamera = NewCamera;
	}

	// update current camera
	if (CurrentCamera != None)
	{
		// we wait to apply this here in case the above code changed currentcamera on us
		if (bResetInterp && !bInterpolateCamChanges)
		{
			CurrentCamera.ResetInterpolation();
		}

		// Make sure overridden post process settings have a chance to get applied
		CamActor = CameraActor(OutVT.Target);
		if( CamActor != None )
		{
		    CamActor.GetCameraView(DeltaTime, OutVT.POV);

			// Check to see if we should be constraining the viewport aspect.  We'll only allow aspect
			// ratio constraints for fixed cameras (non-spectator)
			if( CurrentCamera == FixedCam && CamActor.bConstrainAspectRatio )
			{
				// Grab aspect ratio from the CameraActor
				bConstrainAspectRatio = true;
				OutVT.AspectRatio = CamActor.AspectRatio;
			}

			// See if the CameraActor wants to override the PostProcess settings used.
			CamOverridePostProcessAlpha = CamActor.CamOverridePostProcessAlpha;
			if( CamOverridePostProcessAlpha > 0.f )
			{
				CamPostProcessSettings = CamActor.CamOverridePostProcess;
			}
		}

		CurrentCamera.UpdateCamera(P,self,DeltaTime, OutVT);
		if( CameraStyle == 'FreeCam_Default' )
		{
			Super.UpdateViewTarget(OutVT, DeltaTime);
		}
	}
	else
	{
		// let the engine handle updating
		super.UpdateViewTarget(OutVT, DeltaTime);
	}

	// check for forced fov
	if (bUseForcedCamFOV)
	{
		OutVT.POV.FOV = ForcedCamFOV;
	}

	// adjust FOV for splitscreen, 4:3, whatever
	OutVT.POV.FOV = AdjustFOVForViewport(OutVT.POV.FOV, P);

	// set camera's location and rotation, to handle cases where we are not locked to view target
	SetRotation(OutVT.POV.Rotation);
	SetLocation(OutVT.POV.Location);

	UpdateCameraLensEffects( OutVT );


	// store info about the target's base, to handle target's that are standing on moving geometry
	CacheLastTargetBaseInfo(OutVT.Target.Base);

	bResetInterp = FALSE;
}


/** Update any attached camera lens effects (e.g. blood) **/
simulated function UpdateCameraLensEffects( const out TViewTarget OutVT )
{
	local int Idx;

	for (Idx=0; Idx<CameraLensEffects.length; ++Idx)
	{
		if (CameraLensEffects[Idx] != None)
		{
			CameraLensEffects[Idx].UpdateLocation(OutVT.POV.Location, OutVT.POV.Rotation, OutVT.POV.FOV);
		}
	}
}


simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local Canvas	Canvas;

	Super.DisplayDebug(HUD, out_YL, out_YPos);

	Canvas = HUD.Canvas;
	Canvas.SetDrawColor(255,255,255);

	Canvas.DrawText( "  " @ `showvar(CurrentCamera));
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);

	if( CurrentCamera != None )
	{
		CurrentCamera.DisplayDebug(HUD, out_YL, out_YPos);
	}
}


/**
 * Sets the new color scale
 */
simulated function SetColorScale( vector NewColorScale )
{
	if( bEnableColorScaling == TRUE )
	{
		// set the default color scale
		bEnableColorScaling = TRUE;
		ColorScale = NewColorScale;
		bEnableColorScaleInterp = false;
	}
}

/** Stop interpolation for this frame and just let everything go to where it's supposed to be. */
simulated function ResetInterpolation()
{
	bResetInterp = TRUE;
}


/**
 * Give cameras a chance to influence player view rotation.
 */
function ProcessViewRotation(float DeltaTime, out rotator out_ViewRotation, out rotator out_DeltaRot)
{
	if( CurrentCamera != None )
	{
		CurrentCamera.ProcessViewRotation(DeltaTime, ViewTarget.Target, out_ViewRotation, out_DeltaRot);
	}
}

/**
* Given a horizontal FOV that assumes a 16:9 viewport, return an appropriately
* adjusted FOV for the viewport of the target pawn.
* Used to correct for splitscreen.
*/
final protected native function float AdjustFOVForViewport(float inHorizFOV, Pawn CameraTargetPawn) const;


defaultproperties
{
	DefaultFOV=70.f

	CameraStyle=Default

	ThirdPersonCameraClass=class'GameThirdPersonCamera'
	FixedCameraClass=class'GameFixedCamera'
}

