class WarPlayerCamera extends Camera
	config(Camera);

const CLIP_EXTENT_SCALE = 1.2;

/** Current relative offset applied to our camera */
var		transient	vector	CurrentCameraLocation;
/** Current direction of camera */
var		transient	Rotator	CurrentCameraRotation;
/** Last player origin, for lazy cam interpolation. It's only applied to player's origin, not view offsets, for faster/smoother response */
var		transient	vector	LastPlayerOrigin;
/** Lazy cam interpolation speed */
var()	config		float	LazyCamSpeed;

/** 
 * Last pawn relative offset, for slow offsets interpolation. 
 * This is because this offset is relative to the Pawn's rotation, which can change abruptly (when snapping to cover).
 * Used to adjust the camera position (evade, lean..)
 */
var		transient	vector	LastPawnRelativeOffset;
/** replative offset interpolation speed */
var()	config		float	RelativeOffsetInterpSpeed;

/*********** CAMERA VARIABLES ***********/ 
/******* CAMERA MODES *******/
/** Base camera position when walking */
var(Camera)	editinline	WarCameraMode		WarCamDefault;
/** Cover camera position when walking */
var(Camera) editinline	WarCameraMode		WarCamCover;
/** crouch camera mode */
var(Camera)	editinline	WarCameraMode		WarCamCrouch;
/** targeting camera mode */
var(Camera)	editinline	WarCameraMode		WarCamTargeting;

/******* CAMERA MODIFIERS *******/
/** Camera modifier for leaning left around cover */
var(Camera) editinline	WarCameraModifier	WarCamMod_MoveMood;
/** Camera modifier for setting absolute positions */
var(Camera) editinline	WarCameraModifier	WarCamMod_AbsPos;

var(Camera) editinline	WarCamMod_ScreenShake	WarCamMod_ScreenShake;

//
// Player 'WarCam' camera mode system
//

/** Look At distance. This defines the Camera Rotation */
var()	config	float			LookAtDist;
/** Current WarCam Mode */
var()			WarCameraMode	CurrentWarCam;
/** Desired cam mode when blending. */
var	transient	WarCameraMode	DesiredWarCam;
/** Seconds left when blending 2 WarCams */
var	transient	float			WarCamBlendToGo;
/** Length of blend */
var	transient	float			WarCamBlendLength;
/** Last WarCam offset. */
var	transient	vector			LastWarCamOffset;
/** Last WarCan FOV */	
var	transient	float			LastWarCamFOV;

/** true when blending from a blend */
var	transient	bool			bBlendFromSaved;
/** saved blended FOV */
var transient	float			SavedFOV;
/** saved blended offset */
var transient	vector			SavedOffset;

//
// Focus Point adjustment
//

/** last offset adjustement, for smooth blend out */
var transient	float	LastHeightAdjustment;
/** last adjusted pitch, for smooth blend out */
var transient	float	LastPitchAdjustment;
/**  move back pct based on move up */
var()	config	float	Focus_BackOffStrength;
/** Z offset step for every try */
var()	config	float	Focus_StepHeightAdjustment;
/** number of tries to have focus in view */
var()	config	int		Focus_maxTries;
/** interpolation speed */
var()	config	float	Focus_InterpSpeed;

function PostBeginPlay()
{
	super.PostBeginPlay();

	// Setup camera modes
	if ( WarCamDefault == None )
	{
		WarCamDefault	= new(Outer) class'WarCameraMode_Default';
		WarCamCover		= new(Outer) class'WarCam_Cover';
		//WarCamCover		= new(Outer) class'WarCameraMode_Cover';
		WarCamCrouch	= new(Outer) class'WarCam_Crouch';
		WarCamTargeting	= new(Outer) class'WarCam_Targeting';
	}

	// Setup camera modifiers
	if ( WarCamMod_MoveMood == None )
	{
		WarCamMod_MoveMood		= new(Outer) class'WarCamMod_MovementMood';
	}
	WarCamMod_MoveMood.Init();
	WarCamMod_MoveMood.AddCameraModifier( Self );

	if ( WarCamMod_AbsPos == None )
	{
		WarCamMod_AbsPos		= new(Outer) class'WarCamMod_AbsolutePosition';
	}
	WarCamMod_AbsPos.Init();
	WarCamMod_AbsPos.AddCameraModifier( Self );

	if ( WarCamMod_ScreenShake == None )
	{
		WarCamMod_ScreenShake		= new(Outer) class'WarCamMod_ScreenShake';
	}
	WarCamMod_ScreenShake.Init();
	WarCamMod_ScreenShake.AddCameraModifier( Self );
}

/* Cleanup cameras  */
//@TODO - Need delete for objects added to script like in UC2
simulated function Destroyed()
{
	super.Destroyed();

	/*
	if( WarCamDefault != None ) {
//		delete WarCamDefault;
//		WarCamDefault  = None;
	}
	if( WarCamCover != None ) {
		//delete WarCamCover;
		//WarCamCover = None;
	}

	// Cleanup Modifiers
	if( WarCamMod_MoveMood != None )
	{
//		delete WarCamMod_MoveMood;
//		WarCamMod_MoveMood = None;
	}
	if( WarCamMod_AbsPos != None )
	{
//		delete WarCamMod_AbsPos;
//		WarCamMod_AbsPos = None;
	}
	*/
}

/**
 * Camera Shake
 * Plays camera shake effect
 *
 * @param	Duration			Duration in seconds of shake
 * @param	newRotAmplitude		view rotation amplitude (pitch,yaw,roll)
 * @param	newRotFrequency		frequency of rotation shake
 * @param	newLocAmplitude		relative view offset amplitude (x,y,z)
 * @param	newLocFrequency		frequency of view offset shake
 * @param	newFOVAmplitude		fov shake amplitude
 * @param	newFOVFrequency		fov shake frequency
 */
function CameraShake
( 
	float	Duration, 
	vector	newRotAmplitude, 
	vector	newRotFrequency,
	vector	newLocAmplitude, 
	vector	newLocFrequency, 
	float	newFOVAmplitude,
	float	newFOVFrequency
)
{
	WarCamMod_ScreenShake.StartNewShake( Duration, newRotAmplitude, newRotFrequency, newLocAmplitude, newLocFrequency, newFOVAmplitude, newFOVFrequency );
}

/**
 * Prints out camera debug information to log file
 *
 * @param	Msg		String to display
 * @param	FuncStr	String telling where the log came from (format: Class::Function)
 */
simulated function CamLog( String Msg, String FuncStr )
{
	log( "[" $ Level.TimeSeconds $"]" @ Msg @ "(" $ FuncStr $ ")" );
}

/**
 * Main Camera Updating function... Queries ViewTargets and updates itself 
 *
 * @param	fDeltaTime	Delta Time since last camera update (in seconds).
 * @input	out_CamLoc	Camera Location of last update, to be updated with new location.
 * @input	out_CamRot	Camera Rotation of last update, to be updated with new rotation.
 */
function UpdateCamera( float fDeltaTime, out vector out_CamLoc, out rotator out_CamRot )
{
	// Make sure we have a valid target
	if( primaryVT.Target == None )
	{
		log("Camera::UpdateCamera primaryVT.Target == None");
		return;
	}
	
	// by default disable aspect ratio. Viewtargets can set it to true below.
	bConstrainAspectRatio = false;

	// Default Camera Behavior
	if ( CameraStyle == 'default' && WarPawn(primaryVT.Target) != None )
	{
		PlayerUpdateCamera( WarPawn(primaryVT.Target), fDeltaTime, out_CamLoc, out_CamRot );
	}
	else
	{
		super.UpdateCamera(fDeltaTime, out_CamLoc, out_CamRot);
	}
}

/**
 * Player Update Camera code
 */
function PlayerUpdateCamera( WarPawn P, float fDeltaTime, out vector out_CamLoc, out rotator out_CamRot )
{
	local Rotator	RelativeRot;
	local int		ModifierIdx;
	
	local vector	DesiredCameraOffset, DesiredPawnOffset, DesiredCameraLocation, DesiredCameraOrigin;
	local Rotator	DesiredCameraRotation;

	local vector	X, Y, Z;

	if ( P.PawnCalcCamera(fDeltaTime, out_CamLoc, out_CamRot, CamFOV) )
	{
		return;
	}

	// Find best camera mode, update transitions
	UpdateWarCam( P, fDeltaTime );

	DesiredCameraRotation = out_CamRot;
	GetWarCamOffset( P, fDeltaTime, DesiredCameraOffset, DesiredCameraRotation, CamFOV, DesiredPawnOffset );

	// Loop through each camera modifier
	for( ModifierIdx = 0; ModifierIdx < ModifierList.Length; ModifierIdx++ ) 
	{
		// Apply camera modification and output into DesiredCameraOffset/DesiredCameraRotation
		if( ModifierList[ModifierIdx] != None && 
			ModifierList[ModifierIdx].IsDisabled() == false ) 
		{
			// If ModifyCamera returns true, exit loop
			// Allows high priority things to dictate if they are 
			// the last modifier to be applied
			if( ModifierList[ModifierIdx].ModifyCamera( self, fDeltaTime, DesiredCameraOffset, DesiredCameraRotation, CamFOV ) )
			{
				break;
			}
		}
	}

	//RelativeRot = primaryVT.Target.Rotation;
	RelativeRot = P.Controller.Rotation;
	RelativeRot.Pitch	= 0;
	RelativeRot.Roll	= 0;

	LastPawnRelativeOffset = VectorProportionalMoveTo( LastPawnRelativeOffset, DesiredPawnOffset, fDeltaTime, RelativeOffsetInterpSpeed );

	// Offset relative to Pawn's rotation
	GetAxes(P.Rotation, X, Y, Z);
	DesiredCameraOrigin = P.Location + P.EyePosition() + LastPawnRelativeOffset.X*X + LastPawnRelativeOffset.Y*Y + LastPawnRelativeOffset.Z*Z;;

	// smooth 'origin' interpolation: lazy cam
	LastPlayerOrigin = VectorProportionalMoveTo( LastPlayerOrigin, DesiredCameraOrigin, fDeltaTime, LazyCamSpeed );

	// Add camera relative offset
	GetAxes(RelativeRot, X, Y, Z);
	DesiredCameraLocation = LastPlayerOrigin + DesiredCameraOffset.X*X + DesiredCameraOffset.Y*Y + DesiredCameraOffset.Z*Z;

	// try to have a focus point in view
	AdjustToFocusPoint( P, fDeltaTime, DesiredCameraLocation, DesiredCameraRotation );

	// Do traces and adjust location if colliding with geometry
	// FIXME LAURENT -- Add global cam interpolation, only when collision happens for smooth transitions
	PreventCameraPenetration( primaryVT.Target, DesiredCameraLocation, DesiredCameraRotation );

	CurrentCameraLocation = DesiredCameraLocation;
	CurrentCameraRotation = DesiredCameraRotation;

	/*
	CurrentCameraLocation = VectorProportionalMoveTo( 
									CurrentCameraLocation, 
									DesiredCameraOffset,
									fDeltaTime,
									GlobalCamInterpolationSpeed );
	*/
	// Just accept desired rotation

	out_CamLoc = CurrentCameraLocation;
	out_CamRot = CurrentCameraRotation;
}

/**
 * Adjust Camera location and rotation, to try to have a FocusPoint (point in world space) in view.
 * Also supports blending in and out from that adjusted location/rotation.
 *
 * @param	P			Warfare Pawn currently being viewed.
 * @param	DeltaTime	seconds since last rendered frame.
 * @param	CamLoc		(out) cam location
 * @param	CamRot		(out) cam rotation
 */
simulated function AdjustToFocusPoint( WarPawn P, float DeltaTime, out vector CamLoc, out rotator CamRot )
{
	local vector	FocusPoint, tmpLoc;
	local rotator	RotToFocus;
	local int		nbTries;
	local float		HeightOffset, DeltaPitch;
	local bool		bProcessedFocusPoint;

	// if player has a grenade in hand, get potential focus point.
	if( Weap_Grenade(P.Weapon) != None )
	{
		FocusPoint = Weap_Grenade(P.Weapon).GetFocusPoint();
	}

	// If we have a valid focus point, try to have in view.
	// step up and back, step by step, and trace to see if any geometry is blocking our view
	if( !IsZero(FocusPoint) && 
		(vector(CamRot) dot (FocusPoint - P.Location) > 0) )
	{
		while( nbTries < Focus_maxTries )
		{
			HeightOffset	= Focus_StepHeightAdjustment * nbTries;
			tmpLoc			= CamLoc + vect(0,0,1)*HeightOffset + ((vect(-1,0,0) * HeightOffset * Focus_BackOffStrength) >> CamRot);
			if( FastTrace(FocusPoint,  tmpLoc) )
			{
				break;
			}

			nbTries++;
		}

		// if we can successfully view focus point, then adjust cam loc and cam rot
		if( nbtries < Focus_maxTries )
		{
			LastHeightAdjustment	 = DeltaInterpolationTo( LastHeightAdjustment, HeightOffset, DeltaTime, Focus_InterpSpeed );
			CamLoc					+= vect(0,0,1) * LastHeightAdjustment + ((vect(-1,0,0) * LastHeightAdjustment * Focus_BackOffStrength) >> CamRot);

			RotToFocus				 = Rotator( FocusPoint - CamLoc );
			DeltaPitch				 = FNormalizedRotAxis(RotToFocus.Pitch) - FNormalizedRotAxis(CamRot.Pitch);
			LastPitchAdjustment		 = DeltaInterpolationTo( FNormalizedRotAxis(LastPitchAdjustment), FNormalizedRotAxis(DeltaPitch), DeltaTime, Focus_InterpSpeed );
			CamRot.Pitch			+= LastPitchAdjustment;
			bProcessedFocusPoint = true;
		}
	}

	// if we're not viewing a focus point, reset slowly
	if( !bProcessedFocusPoint )
	{
		// blend out of vertical offset adjustement
		if( LastHeightAdjustment != 0 )
		{
			LastHeightAdjustment	 = DeltaInterpolationTo( LastHeightAdjustment, 0, DeltaTime, Focus_InterpSpeed );
			CamLoc					+= vect(0,0,1) * LastHeightAdjustment + ((vect(-1,0,0) * LastHeightAdjustment * Focus_BackOffStrength) >> CamRot);
		}

		// blend out of pitch adjustement
		if( LastPitchAdjustment != 0 )
		{
			LastPitchAdjustment  = DeltaInterpolationTo( LastPitchAdjustment, 0, DeltaTime, Focus_InterpSpeed );
			CamRot.Pitch		+= LastPitchAdjustment;
		}
	}
}

/**
 * Evaluates the current situation and returns the camera mode
 * that best matches, ie targeting/crouched/etc.
 * 
 * @return 	  	new camera mode to use
 */
simulated function WarCameraMode FindBestCameraMode( WarPawn P )
{
	local WarCameraMode	NewCamMode;

	if( P.CoverType != CT_None ) 
	{
		NewCamMode = WarCamCover;
	}
	else
	{
		if( WarPC(P.Controller) != None && 
			WarPC(P.Controller).bTargetingMode )
		{
			NewCamMode = WarCamTargeting;
		}
		else if( P.bIsCrouched )
		{
			NewCamMode = WarCamCrouch;
		}
		else
		{
			NewCamMode = WarCamDefault;
		}
	}

	return NewCamMode;
}

/** Update Camera modes. Pick Best, handle transitions */
function UpdateWarCam( WarPawn P, float fDeltaTime )
{
	local WarCameraMode	NewWarCam;
	local float			NewBlendTime;

	// Pick most suitable camera mode
	NewWarCam = FindBestCameraMode( P );
	if( NewWarCam == None )
	{
		CamLog("FindBestCameraMode returned none!!", "WarPlayerCamera::UpdateWarCam");
	}
	
	// If we have no camera mode at the moment, set directly
	if( CurrentWarCam == None )
	{	
		//CamLog("CurrentWarCam == None, setting NewWarCam:"$NewWarCam , "WarPlayerCamera::UpdateWarCam");
		NewWarCam.OnBecomeActive( Self );
		CurrentWarCam = NewWarCam;
	}
	// If transitioning from one WarCam to another, set up blending
	else if( (NewWarCam != DesiredWarCam) &&
			((NewWarCam != CurrentWarCam) || WarCamBlendToGo > 0.f) )
	{
		// init new cam mode
		if( NewWarCam != DesiredWarCam && 
			NewWarCam != CurrentWarCam )
		{
			//CamLog("New camera mode Init:"$NewWarCam, "WarPlayerCamera::UpdateWarCam");
			NewWarCam.OnBecomeActive( Self );
		}

		// Define blend duration
		if( NewWarCam == CurrentWarCam ) 
		{
			// If blending back to same camera mode
			NewBlendTime = FMax(NewWarCam.BlendTime - WarCamBlendToGo, WarCamBlendToGo);
		}
		else 
		{
			NewBlendTime = NewWarCam.BlendTime;
		}

		if( NewBlendTime > 0 )
		{
			//CamLog("Blend to new WarCam:"$NewWarCam @ "NewBlendTime:"$NewBlendTime, "WarPlayerCamera::UpdateWarCam");
			// if we're already blending, blend from current position to new one for smooth transitioning
			// We ignore the ideal view point provided by the current view, 
			// and force a locked offset from the current blend
			if( WarCamBlendToGo > 0 )
			{
				bBlendFromSaved = true;
				SavedFOV		= LastWarCamFOV;
				SavedOffset		= LastWarCamOffset;
			}

			WarCamBlendToGo		= NewBlendTime;
			DesiredWarCam		= NewWarCam;
			WarCamBlendLength	= NewBlendTime;
		}
		else
		{
			//CamLog("Blend to new WarCam:"$NewWarCam @ "instantly.", "WarPlayerCamera::UpdateWarCam");
			WarCamBlendToGo	= 0.f;
			CurrentWarCam	= NewWarCam;
			DesiredWarCam	= None;
		}
	}

	if( WarCamBlendToGo > 0.f ) 
	{
		// Check Blending
		WarCamBlendToGo -= fDeltaTime;

		// If not blending anymore
		if ( WarCamBlendToGo <= 0.f )
		{
			//CamLog("finished blend to:"$DesiredWarCam, "WarPlayerCamera::UpdateWarCam");

			// Clear out target camera info
			CurrentWarCam	= DesiredWarCam;
			DesiredWarCam	= None;		
			WarCamBlendToGo	= 0.f;
			bBlendFromSaved = false;
		}
	}
}

/**
 * Get war cam offsets. Queries current war cams and retrieve offsets. Also handles blending between 2 cam modes 
 * 
 * @param	P						viewed WarPawn
 * @param	fDeltaTime				Time since last frame
 * @param	out_CamRelativeOffset	Cam relative offset
 * @param	out_CamRot				Cam desired rotation
 * @param	out_FOV					Desired FOV
 * @param	out_PawnOffset			desired pawn relative offset
 */
function GetWarCamOffset
( 
	WarPawn			P, 
	float			fDeltaTime, 
	out vector		out_CamRelativeOffset, 
	out rotator		out_CamRot, 
	out float		out_FOV, 
	out vector		out_PawnOffset
)
{
	local Vector		EyesLoc, CamLookAt, IdealOffset, DesiredOffset, FromOffset;
	local Rotator		NewRot, EyesRot;
	local float			BlendPct, FromFOV, DesiredFOV;

	P.GetActorEyesViewPoint(EyesLoc, EyesRot);
	EyesLoc = P.EyePosition();

	// If blending, blend from current to desired war cam
	if( WarCamBlendToGo > 0.f )
	{
		// Compute percent of blend time
		BlendPct = (WarCamBlendLength - WarCamBlendToGo) / WarCamBlendLength;

		// Blend the ideal offsets
		if( bBlendFromSaved )
		{
			FromOffset	= SavedOffset;
			FromFOV		= SavedFOV;
		}
		else
		{
			CurrentWarCam.GetCamModeViewOffset( P, fDeltaTime, FromOffset, EyesRot );
			FromFOV		= CurrentWarCam.GetDesiredFOV( P );
		}

		DesiredWarCam.GetCamModeViewOffset( P, fDeltaTime, DesiredOffset, EyesRot );
		DesiredFOV	= DesiredWarCam.GetDesiredFOV( P );

		// Blend offsets
		IdealOffset	= VSmerp( BlendPct, FromOffset, DesiredOffset );

		out_PawnOffset = DesiredWarCam.GetPawnRelativeOffset( P );

		// Blend FOVs
		out_FOV = Smerp( BlendPct, FromFOV, DesiredFOV );
	}
	else
	{
		// Use current view mode FOV and ideal offset
		CurrentWarCam.GetCamModeViewOffset( P, fDeltaTime, IdealOffset, EyesRot );

		out_PawnOffset	= CurrentWarCam.GetPawnRelativeOffset( P );
		out_FOV			= CurrentWarCam.GetDesiredFOV( P );
	}

	// Save values, in case we need start a blend during a blend
	// (then we blend from current blended position)
	LastWarCamFOV		= out_FOV;
	LastWarCamOffset	= IdealOffset;

	// Define look at point
	CamLookAt = EyesLoc + Vector(EyesRot) * LookAtDist;

	// Set up view point
	// CamRotation is always looking at the LookAt Point.
	NewRot		= Rotator(CamLookAt - IdealOffset);
	NewRot.Roll = EyesRot.Roll;

	out_CamRelativeOffset	= IdealOffset;
	out_CamRot				= NewRot;
}

/**
 * Handles traces to make sure camera does not penetrate geometry and tries to find
 * the best location for the camera
 *
 * @param	ViewTarget			- Actor camera is attached to
 * @param	out_CameraLocation	- Desired camera location (in)
 *								- New camera location (out)
 * @param	out_CameraRotation	- Desired camera rotation (in)
 *								- New camera rotation (out)
 */
function PreventCameraPenetration
( 
		Actor	ViewTarget, 
	out Vector	out_CameraLocation, 
	out Rotator out_CameraRotation 
)
{
	local Vector HitLocation, HitNormal;
	local Vector WorstLocation, CamExtent;
	local Actor	 HitActor;
	local Pawn	 P;

	// Must have a view target
	if( ViewTarget == None ) 
	{
		return;
	}

	// Get worst location
	P = Pawn(ViewTarget);
	if( P != None ) 
	{
		// If a pawn - use eye location 
		WorstLocation = P.EyePosition() + P.Location;
		CamExtent	  = GetCameraExtent();
	}
	else 
	{
		// Otherwise, just use location
		WorstLocation = ViewTarget.Location;
	}

	// Trace from worst to desired location
	HitActor = ViewTarget.Trace( HitLocation, HitNormal, out_CameraLocation, WorstLocation, false, CamExtent );

	// If hit something
	if( HitActor != None ) 
	{
		out_CameraLocation.X = HitLocation.X;
		out_CameraLocation.Y = HitLocation.Y;

		HitActor = ViewTarget.Trace( HitLocation, HitNormal, out_CameraLocation, WorstLocation, false, CamExtent );
		if( HitActor != None )
		{
			out_CameraLocation = HitLocation;
		}
	}

	/*
	@todo - alpha out character if camera is too close
	// If camera is inside player 
	if( P != None &&
		PointInBox( out_CameraLocation, P.Location, P.GetCollisionExtent() * CLIP_EXTENT_SCALE ) )
	{
		// Use worst location
		out_CameraLocation = WorstLocation;
	}*/
}

/** Return camera collision size */
function Vector GetCameraExtent()
{
	return vect(15,15,15);
}

/** Give WarCams a chance to change player view rotation */
function ProcessViewRotation( float fDeltaTime, out rotator out_ViewRotation, out Rotator out_DeltaRot )
{
	if( CurrentWarCam != None ) 
	{
		CurrentWarCam.ProcessViewRotation( fDeltaTime, primaryVT.Target, out_ViewRotation, out_DeltaRot );
	}

	if( DesiredWarCam != None )
	{
		DesiredWarCam.ProcessViewRotation( fDeltaTime, primaryVT.Target, out_ViewRotation, out_DeltaRot );
	}
}


//	WarCam=(vCamExtent=(X=15,Y=15,Z=15),LookAtDist=16384,fLazyCamSpeed=8)

/*
	WarCamDefault=(CamName=default,OffsetHigh=(X=0,Y=48,Z=80),OffsetMid=(X=-128,Y=36,Z=10),OffsetLow=(X=-104,Y=48,Z=-96),FOVAngle=85)
	WarCamTargeting=(CamName=targeting,OffsetHigh=(X=16,Y=40,Z=16),OffsetMid=(X=-40,Y=40,Z=0),OffsetLow=(X=-60,Y=40,Z=-44),FOVAngle=75,BlendTime=0.15)
	WarCamCrouch=(CamName=crouch,OffsetHigh=(X=0,Y=56,Z=112),OffsetMid=(X=-192,Y=48,Z=24),OffsetLow=(X=-128,Y=56,Z=-128),FOVAngle=90,BlendTime=0.67)
	WarCamGrenade=(CamName=grenade,OffsetHigh=(X=32,Y=32,Z=128),OffsetMid=(X=-128,Y=32,Z=64),OffsetLow=(X=-128,Y=32,Z=-64),FOVAngle=95,BlendTime=0.5)
*/
defaultproperties
{
	LazyCamSpeed=16
	RelativeOffsetInterpSpeed=10
	LookAtDist=65535

	Focus_BackOffStrength=0.33f
	Focus_StepHeightAdjustment= 64
	Focus_maxTries=4
	Focus_InterpSpeed=4
}