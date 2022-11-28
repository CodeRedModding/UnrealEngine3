class WarCam_Cover extends WarCameraMode
	config(Camera);

/** MidLevel cover lean pawn relative offsets */
var()	config	vector	PawnRel_MidLvlLeanRightMax;
var()	config	vector	PawnRel_MidLvlLeanRightMid;
var()	config	vector	PawnRel_MidLvlLeanRightMin;

/** Default lean pawn relative offset */
var()	config	vector	PawnRel_LeanRight;
/** MidLevel stand up pawn relative offset */
var()	config	vector	PawnRel_StandUp;
/** Crouching pawn relative offset */
var()	config	vector	PawnRel_Crouching;
/** default view limits */
var()	config	rotator	ViewMaxLimit;
var()	config	rotator	ViewMinLimit;

/** FOV default, when in Targeting mode */
var()	config	float	TargetingModeFOV;
/** FOV interpolation speed (from in and out of Targeting mode ) */
var()	config	float	FOVInterpSpeed;
/** Current (interpolated) FOV */
var transient	float	CurrentFOV;
/** Target/Desired FOV */
var transient	float	TargetFOV;
/** Percentage of viewrotation Yaw within limits */
var	transient	float	ViewYawPct;

/** Called when Camera mode becomes active */
function OnBecomeActive( Camera CameraOwner )
{
	super.OnBecomeActive( CameraOwner );

	TargetFOV	= FOVAngle;
	CurrentFOV	= FOVAngle;
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
	UpdateFOV( DeltaTime );

	super.GetCamModeViewOffset(ViewedPawn, DeltaTime, out_Offset, out_ViewRotation);
}

/** Get Pawn's relative offset (from location based on pawn's rotation */
function vector GetPawnRelativeOffset( WarPawn P )
{
	local	CoverNode.ECoverType	CT;
	local	CoverNode.ECoverAction	CA;
	local	WarPC.ECoverDirection	CD;
	local	vector					LeanOffset;
	//local	float					Pct;

	CA = P.CoverAction;
	CT = P.CoverType;
	CD = WarPC(P.Controller).GetCoverDirection();

	if( CD == CD_Up ) /*|| 
		(CT == CT_MidLevel && CD == CD_Default) ) */
	{
		return PawnRel_StandUp;
	}

	if( CT == CT_Crouching )
	{
		return PawnRel_Crouching;
	}

	if( CT == CT_None || CD == CD_Default )
	{
		return PawnRelativeOffset;
	}
	else
	{	/*
		if( CT == CT_MidLevel )
		{
			if( CD == CD_Right )
			{
				Pct = ViewYawPct;
			}
			else
			{
				Pct = 1.f - ViewYawPct;
			}

			// If in upper percent range
			if( Pct > 0.5 ) 
			{
				// Rescale to the (0.5, 1.0] range
				Pct			= (Pct - 0.5) / 0.5;
				LeanOffset	= VLerp( Pct, PawnRel_MidLvlLeanRightMid, PawnRel_MidLvlLeanRightMax );
			}
			else 
			{
				// Otherwise, percent is in lower range
				// Rescale to the [0.0, 0.5] range
				Pct			= Pct / 0.5;
				LeanOffset	= VLerp( Pct, PawnRel_MidLvlLeanRightMin, PawnRel_MidLvlLeanRightMid );
			}
		}
		else
		{ */
			LeanOffset = PawnRel_LeanRight;
		//}

		if( CD == CD_Right )
		{
			return	LeanOffset;
		}

		// for left lean, mirror Y vector
		LeanOffset.Y = -LeanOffset.Y;
		return LeanOffset;
	}
}

/** Update FOV interpolation */
simulated function UpdateFOV( float fDeltaTime )
{
	CurrentFOV = DeltaInterpolationTo( CurrentFOV, TargetFOV, fDeltaTime, FOVInterpSpeed);
}

/**
 * Overridden to add extra FOV when fully leaning out from cover.
 */
function float GetDesiredFOV( WarPawn ViewedPawn )
{
	if( WarPC(ViewedPawn.Controller).bTargetingMode )
	/*
	if( ViewedPawn.CoverAction == CA_LeanLeft ||
		ViewedPawn.CoverAction == CA_LeanRight ||
		ViewedPawn.CoverAction == CA_PopUp ||
		(ViewedPawn.CoverType == CT_Crouching && WarPC(ViewedPawn.Controller).bTargetingMode) )
	*/
	{
		TargetFOV = TargetingModeFOV;
	}
	else
	{
		TargetFOV = FOVAngle;
	}

	return CurrentFOV;
}

/**
 * Handles clamping view rotation within limits for cover / leaning
 *
 * @param	DeltaTime		- change in time
 * @param	ViewTarget		- Actor camera is attached to
 * @param	ViewRotation	- Current view rotation
 * @param	out_DeltaRot	- Change in rotation to possibly be applied (in)
							- Change in rotation to continue (out)
 *
 * @return Rotator of new view rotation
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
	ViewYawPct = 0.f;

	// If player is in CoverMode, then we limit the view rotation
	if( ViewedPawn != None && 
		ViewedPawn.CoverType != CT_None &&
		ViewedPawn.Controller != None &&
		ViewedPawn.Controller.IsInState('Cover_Circular') ) 
	{
		// Retrieve upper/lower clamp limits based on cover state
		GetViewRotationLimits(ViewedPawn, MaxLimit, MinLimit);

		// Compute percent of yaw rotation between limits
		if( MaxLimit.Yaw != MinLimit.Yaw ) 
		{
			ViewYawPct = FClamp( Abs((FNormalizedRotAxis(out_ViewRotation.Yaw - ViewedPawn.Rotation.Yaw) - MinLimit.Yaw) / (MaxLimit.Yaw - MinLimit.Yaw)), 0.f, 1.f );
		}

		// Clamp Yaw
		if ( MinLimit.Yaw != 0 || MaxLimit.Yaw != 0 )
		{
			SClampRotAxis( DeltaTime, out_ViewRotation.Yaw-ViewTarget.Rotation.Yaw, out_DeltaRot.Yaw, MaxLimit.Yaw, MinLimit.Yaw, 8.f );
		}

		// Clamp Pitch
		if ( MinLimit.Pitch != 0 || MaxLimit.Pitch != 0 )
		{
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
	out_MaxLimit = ViewMaxLimit;
	out_MinLimit = ViewMinLimit;
}

defaultproperties
{
	ViewMaxLimit=(Pitch=0,Yaw=11000,Roll=0)
	ViewMinLimit=(Pitch=0,Yaw=-11000,Roll=0)

	PawnRel_MidLvlLeanRightMax=(Y=48,Z=-12)
	PawnRel_MidLvlLeanRightMid=(Y=80,Z=+12)
	PawnRel_MidLvlLeanRightMin=(Y=80,Z=+24)

	PawnRel_LeanRight=(Y=80)

	PawnRel_StandUp=(Z=45)
	PawnRel_Crouching=(Z=10)

	ViewOffsetHigh=(X=-16,Y=16,Z=80)
	ViewOffsetLow=(X=-96,Y=16,Z=-132)
	ViewOffsetMid=(X=-160,Y=16,Z=0)

	FOVInterpSpeed=10
	TargetingModeFOV=45
	FOVAngle=70

	BlendTime=0.33
}