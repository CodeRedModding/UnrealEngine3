class WarCameraMode_Cover extends WarCameraMode_Default
	config(Camera);

/* Smerp vector interpolation instead of lerp */
var() config		bool	bSmerp;

/* Standing cover offsets */
var() config		Vector	MinOffset,
						MidOffset,
						MaxOffset;
/* Crouched cover offsets */
var() config		Vector	CrouchMinOffset,
						CrouchMidOffset,
						CrouchMaxOffset;

/* Standing lean cover offsets */
var() config		Vector	LeanMinOffset,
						LeanMidOffset,
						LeanMaxOffset;

/* Crouched lean cover offsets */
var() config		Vector	CrouchLeanMinOffset,
						CrouchLeanMidOffset,
						CrouchLeanMaxOffset;

/* Amount of standing roll to apply to camera when leaning */
var() config		int		LeanMinRoll,
						LeanMaxRoll;

/* Amount of crouching roll to apply to camera when leaning */
var() config		int		CrouchLeanMinRoll,
						CrouchLeanMaxRoll;

/* Rotation to adjust ViewTarget.Rotation */
var()	config		Rotator	BaseRotationOffset;

/* Upper/Lower limits for clamping rotation (-1 means do not clamp) */
var() config		Rotator UpperLimit,
						LowerLimit;

/* Upper/Lower limits for lean rotation clamping */
var() config		Rotator LeanUpperLimit,
						LeanLowerLimit;

/** Alpha in/out time for lean and rotation clamping */
var() config		float	AlphaInTime,
						AlphaOutTime;

/** Target alpha for lean/clamp rotation */
var()				float	TargetAlpha;

/** Current alpha */
var transient	float	Alpha;

/** Old cover type of pawn */
var	transient CoverNode.ECoverType OldCoverType;

/** Last known cover direction of the player */
var transient WarPC.ECoverDirection OldCoverDirection;

/** FOV default, when in Targeting mode */
var()	config	float	TargetingModeFOV;
/** FOV interpolation speed (from in and out of Targeting mode ) */
var()	config	float	FOVInterpSpeed;
/** Current (interpolated) FOV */
var transient	float	CurrentFOV;
/** Target/Desired FOV */
var transient	float	TargetFOV;


/** Called when Camera mode becomes active */
function OnBecomeActive( Camera CameraOwner )
{
	super.OnBecomeActive( CameraOwner );

	TargetFOV			= FOVAngle;
	CurrentFOV			= FOVAngle;
	Alpha				= 0.0;
	TargetAlpha			= 0.0;
	OldCoverType		= CT_None;
	OldCoverDirection	= CD_Default;
}

/**
 * Handles updating alpha as time progreses
 *
 * @param	DeltaTime - change in time since last update
 */
simulated function UpdateAlpha( float DeltaTime )
{
	local float Time;

	// Alpha out
	if( TargetAlpha == 0.0 ) 
	{
		Time = AlphaOutTime;		
	}
	else 
	{
		// Otherwise, alpha in
		Time = AlphaInTime;
	}

	// If no time available - just set alpha
	if( Time <= 0.0 )
	{
		Alpha = TargetAlpha;
	}
	else if( Alpha > TargetAlpha )
	{
		// Otherwise, alpha down
		Alpha = FMax( Alpha - (DeltaTime * (1.0 / Time)), TargetAlpha );
	}
	else 
	{
		// Or alpha up
		Alpha = FMin( Alpha + (DeltaTime * (1.0 / Time)), TargetAlpha );
	}
}

/** Update FOV interpolation */
simulated function UpdateFOV( float fDeltaTime )
{
	CurrentFOV = DeltaInterpolationTo( CurrentFOV, TargetFOV, fDeltaTime, FOVInterpSpeed);
}

/**
 * Calculates and returns the ideal view offset for the specified camera mode.
 * The offset is relative to the player origin and calculated by interpolating 2 ideal view points based on the player view pitch.
 * 
 * @param	DeltaTime			Delta time since last frame.
 * @param	out_Offset			Ideal offset from Pawn's origin.
 * @param	out_ViewRotation
 * @param	out_FOV				Desired FOV.
 */
simulated function GetCamModeViewOffset
(
		WarPawn		ViewedPawn,
		float		DeltaTime,
	out	Vector		out_Offset,
	out Rotator		out_ViewRotation
)
{
	local float					Pct;
	local int					MinYaw, MaxYaw;
	local int					RMin, RMax, RollOffset;
	local Vector				Min, Mid, Max, Tmp;
	local Rotator				MaxViewRotLimit, MinViewRotLimit;
	local CoverNode.ECoverType	CT;
	local WarPC.ECoverDirection CD;

	UpdateFOV( DeltaTime );

	CT = ViewedPawn.CoverType;
	CD = WarPC(ViewedPawn.Controller).GetCoverDirection();

	// Analyze change in cover type
	if( CT != OldCoverType ) 
	{
		// If moved from no cover to cover, zero alpha
		if( OldCoverType == CT_None )
		{
			Alpha = 0.0;
		}

		// If moving to no cover, zero target alpha
		if( CT == CT_None )
		{
			TargetAlpha = 0.0;
		}
	}
	
	// Analyze change in cover action
	if( CD != OldCoverDirection && 
		CT != CT_None		 )
	{
		// Figure out target alpha
		if( CD == CD_Right ) 
		{
			TargetAlpha =  1.0;
		}
		else if( CD == CD_Left )
		{
			TargetAlpha = -1.0;
		}
		else 
		{
			TargetAlpha =  0.0;
		}

		// If moving from one lean to another, half transition speed
		if( (CD == CD_Left &&
			OldCoverDirection == CD_Right) ||
			(CD == CD_Right &&
			OldCoverDirection == CD_Left) )
		{
			AlphaInTime = default.AlphaInTime / 2.0;
		}
		else 
		{
			AlphaInTime = default.AlphaInTime;
		}
	}

	// Store new cover state
	OldCoverType	= CT;
	OldCoverDirection = CD;

	// Compute rotation limits
	GetViewRotationLimits(ViewedPawn, MaxViewRotLimit, MinViewRotLimit);

	// Update alpha to be used by roll/rotation clamping
	UpdateAlpha( DeltaTime );

	// Figure out min/max yaw available based on rotation limits
	MaxYaw = MaxViewRotLimit.Yaw;
	MinYaw = MinViewRotLimit.Yaw;

	// Compute percent of yaw rotation between limits
	if ( MaxYaw != MinYaw ) 
	{
		Pct	= FClamp( Abs((FNormalizedRotAxis(out_ViewRotation.Yaw - ViewedPawn.Rotation.Yaw) - MinYaw) / (MaxYaw - MinYaw)), 0.f, 1.f );
	}
	else 
	{
		Pct	= 0.0;
	}

	// If pawn is crouching
	if ( ViewedPawn.bIsCrouched ) 
	{
		// If leaning
		if( CD != CD_Default ) 
		{
			// Use crouch lean offets
			Min  = CrouchLeanMinOffset;
			Mid  = CrouchLeanMidOffset;
			Max  = CrouchLeanMaxOffset;

			RMin = CrouchLeanMinRoll;
			RMax = CrouchLeanMaxRoll;
		}
		else
		{
			// Otherwise, use crouch offsets
			Min  = CrouchMinOffset;
			Mid  = CrouchMidOffset;
			Max  = CrouchMaxOffset;
		}
	}
	else
	{
		// Otherwise, pawn is standing

		// If pawn is leaning
		if( CD != CD_Default ) 
		{
			// Use lean offsets
			Min  = LeanMinOffset;
			Mid  = LeanMidOffset;
			Max	 = LeanMaxOffset;

			RMin = LeanMinRoll;
			RMax = LeanMaxRoll;
		}
		else
		{
			// Otherwise, use standing offsets
			Min  = MinOffset;
			Mid  = MidOffset;
			Max  = MaxOffset;
		}
	}

	// If leaning right, mirror min/max and flip Y component of offsets
	if( CD == CD_Left ) 
	{
		Tmp	  = Min;
		Min	  = Max;
		Max	  = Tmp;

		Min.Y = -Min.Y;
		Mid.Y = -Mid.Y;
		Max.Y = -Max.Y;
	}

	// Compute roll offset by percent and apply roll to view rotation (scale by alpha)
	RollOffset	= RMin + (RMax - RMin) * Pct;
	out_ViewRotation.Roll = RollOffset * Alpha;

	// If in upper percent range
	if( Pct > 0.5 ) 
	{
		// Rescale to the (0.5, 1.0] range
		Pct = (Pct - 0.5) / 0.5;

		// Interp between Mid/Max offsets
		if( bSmerp )
		{
			out_Offset = VSmerp( Pct, Mid, Max );
		}
		else 
		{
			out_Offset = VLerp( Pct, Mid, Max );
		}
	}
	else 
	{
		// Otherwise, percent is in lower range

		// Rescale to the [0.0, 0.5] range
		Pct = Pct / 0.5;

		// Interp between Min/Mid
		if( bSmerp )
		{
			out_Offset = VSmerp( Pct, Min, Mid );
		}
		else
		{
			out_Offset = VLerp( Pct, Min, Mid );
		}
	}
}

/**
 * Handles clamping view rotation within limits for cover / leaning
 *
 * @param	DeltaTime		- change in time
 * @param	ViewTarget		- Actor camera is attached to
 * @param	ViewRotation	- Current view rotation
 * @param	out_DeltaRot	- Change in rotation to possibly be applied (in)
 *							- Change in rotation to continue (out)
 */
simulated function ProcessViewRotation
( 
		float	DeltaTime, 
		Actor	ViewTarget, 
	out	Rotator out_ViewRotation, 
	out Rotator out_DeltaRot 
)
{
	local Rotator	MaxLimit, MinLimit;
	local WarPawn	ViewedPawn;

	ViewedPawn = WarPawn(ViewTarget);

	// If player is in CoverMode, then we limit the view rotation
	if ( ViewedPawn != None && ViewedPawn.CoverType != CT_None ) 
	{
		// Retrieve upper/lower clamp limits based on cover state
		GetViewRotationLimits(ViewedPawn, MaxLimit, MinLimit);

		// Clamp Yaw
		if ( MinLimit.Yaw != 0 || MaxLimit.Yaw != 0 )
		{
			//ClampRotAxis( ViewRotation.Yaw-ViewTarget.Rotation.Yaw, out_DeltaRot.Yaw, MaxLimit.Yaw, MinLimit.Yaw );
			SClampRotAxis( DeltaTime, out_ViewRotation.Yaw-ViewTarget.Rotation.Yaw, out_DeltaRot.Yaw, MaxLimit.Yaw, MinLimit.Yaw, 8.f );
		}

		// Clamp Pitch
		if ( MinLimit.Pitch != 0 || MaxLimit.Pitch != 0 )
		{
			//ClampRotAxis( ViewRotation.Pitch-ViewTarget.Rotation.Pitch, out_DeltaRot.Pitch, MaxLimit.Pitch, MinLimit.Pitch );
			SClampRotAxis( DeltaTime, out_ViewRotation.Pitch-ViewTarget.Rotation.Pitch, out_DeltaRot.Pitch, MaxLimit.Pitch, MinLimit.Pitch, 8.f );
		}
		
	}
}

/**
 * Returns View Rotation Clipping limits depending on WarPawn's cover state
 *
 * @output	out_MaxLimit	min view rotation limit
 * @output	out_MinLimit	min view rotation limit
 */
simulated function GetViewRotationLimits
( 
		WarPawn	ViewedPawn,
	out	Rotator	out_MaxLimit,
	out	Rotator	out_MinLimit
)
{
	local WarPC wfPC;
	wfPC = WarPC(ViewedPawn.Controller);
	if ( wfPC != None &&
		 wfPC.CoverDirection == CD_Left )
	{
		out_MaxLimit		= LeanUpperLimit;
		out_MinLimit		= LeanLowerLimit;
		out_MaxLimit.Yaw	= -LeanLowerLimit.Yaw;
		out_MinLimit.Yaw	= -LeanUpperLimit.Yaw;
	}
	else if ( wfPC != None &&
			  wfPC.CoverDirection == CD_Right )
	{
		out_MaxLimit = LeanUpperLimit;
		out_MinLimit = LeanLowerLimit;
	}
	else 
	{
		out_MaxLimit = UpperLimit;
		out_MinLimit = LowerLimit;
	}
}

/**
 * Overridden to add extra FOV when fully leaning out from cover.
 */
function float GetDesiredFOV( WarPawn ViewedPawn )
{
	if( ViewedPawn.CoverAction == CA_LeanLeft ||
		ViewedPawn.CoverAction == CA_LeanRight ||
		ViewedPawn.CoverAction == CA_PopUp ||
		(ViewedPawn.CoverType == CT_Crouching && WarPC(ViewedPawn.Controller).bTargetingMode) )
	{
		TargetFOV = TargetingModeFOV;
	}
	else
	{
		TargetFOV = FOVAngle;
	}

	return CurrentFOV;
}

defaultproperties
{
	FOVInterpSpeed=10
	TargetingModeFOV=45
	FOVAngle=70

	AlphaInTime=0.5
	AlphaOutTime=0.5
	
	MinOffset=(X=0,Y=100,Z=0)
	MidOffset=(X=-100,Y=0,Z=0)
	MaxOffset=(X=0,Y=-100,Z=0)

	CrouchMinOffset=(X=-60,Y=100,Z=-30)
	CrouchMidOffset=(X=-60,Y=0,Z=-30)
	CrouchMaxOffset=(X=-60,Y=-100,Z=-30)

	LeanMinOffset=(X=-100,Y=100,Z=-10)
	LeanMidOffset=(X=-14,Y=15,Z=0)
	LeanMaxOffset=(X=30,Y=-70,Z=5)

	CrouchLeanMinOffset=(X=-64,Y=100,Z=-30)
	CrouchLeanMidOffset=(X=-14,Y=15,Z=-25)
	CrouchLeanMaxOffset=(X=30,Y=-70,Z=-20)

	//LeanMinRoll=3276
	LeanMinRoll=1920
	LeanMaxRoll=0
	CrouchLeanMinRoll=1920
	CrouchLeanMaxRoll=0
	
	UpperLimit=(Pitch=0,Yaw=16384,Roll=0)
	LowerLimit=(Pitch=0,Yaw=-16384,Roll=0)
	LeanUpperLimit=(Pitch=0,Yaw=16384,Roll=0)
	LeanLowerLimit=(Pitch=0,Yaw=-512,Roll=0)

//	bSmerp=true

	//debug
//	bDebug=true
}
