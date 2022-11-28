/**
 * WarPlayerInput
 * Warfare player input post processing
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class WarPlayerInput extends PlayerInput within WarPC
	config(Input)
	transient;

//
// View Acceleration
//

/** Acceleration multiplier */
var()	config	float	YawAccelMultiplier;
/** Threshold above when Yaw Acceleration kicks in*/
var()	config	float	YawAccelThreshold;
/** Time for Yaw Accel to ramp up */
var()	config	float	YawAccelRampUpTime;
/** Yaw acceleration percentage applied when fading in or out */
var				float	YawAccelPct;

//
// Friction
//
/** Debug Friction code */
var()	globalconfig bool		bDebugFriction;
/** Friction enabled */
var()	globalconfig bool		bTargetFriction;
/** Maximum distance to acquire a target for friction */
var()	globalconfig float		TargetFrictionDist;
/** FOV of friction. X = Azimuth angle in degrees. Y = Elevation angle in degrees. */
var()	globalconfig Vector2D	Friction_FOV;
/** Friction turning rate scale. X = turn. Y = look up. */
var()	globalconfig Vector2D	Friction_RateScale;

//
// Adhesion
//

/** Debug Adhesion code */
var()	globalconfig bool		bDebugAdhesion;
/** Target adhesion enabled */
var()	globalconfig bool		bTargetAdhesion;
/** Ignore adhesion if player is not moving */
var()	globalconfig bool		bIgnoreAdhesionOnNoMove;
/** Maximum distance to acquire a target for adhesion */
var()	globalconfig float		TargetAdhesionDist;
/**  FOV of adhesion. X = Azimuth angle in degrees. Y = Elevation angle in degrees. */
var()	globalconfig vector2D	Adhesion_FOV;
/** Max amount of adhesion. X = lateral. Y = vertical. */
var()	globalconfig vector2D	Adhesion_Strength;
/** Ramp up time to reach max adhesion */
var()	globalconfig float		AdhesionRampUpTime;
/** time seconds adhesion targets are checked */
var()	globalconfig float		AdhesionTargetFrequencyChecks;

/** Adhesion dot FOV (converted from angles to dot) */
var	Vector2D	Adhesion_DotFOV;
/** Friction dot FOV (converted from angles to dot) */
var	Vector2D	Friction_DotFOV;

/** Dot distance to crosshair of adhesion target. X = Azimuth. Y = Elevation. */
var	Vector2D	AdhesionTarget_DotDist;
/** Dot distance to crosshair of friction target. X = Azimuth. Y = Elevation. */
var Vector2D	FrictionTarget_DotDist;

/** Current alpha fade in scale applied to adhesion */
var	float		AdhesionAlpha;
/** Last rotation to Target. Used by Adhesion to determine delta angle to correct player's view */
var	Rotator		LastRotToTarget;

/** Target used to determine friction for input */
var	Actor		FrictionTarget, LastFrictionTarget;
/** Target used to determine adhesion for input */
var	Actor		AdhesionTarget, LastAdhesionTarget;

//
// Automatic Pitch centering
//

/** auto pitch centering if true */
var()	config	bool	bAutoCenterPitch;
/** Pitch auto centering speed */
var()	config	float	PitchAutoCenterSpeed;
/** delay before starting auto centering when moving */
var()	config	float	PitchAutoCenterDelay;
/** count for delay */
var				float	PitchAutoCenterDelayCount;

//
// Joy Raw Input
//
/** Joypad left thumbstick, vertical axis. Range [-1,+1] */
var		transient	float	RawJoyUp;
/** Joypad left thumbstick, horizontal axis. Range [-1,+1] */
var		transient	float	RawJoyRight;
/** Joypad right thumbstick, horizontal axis. Range [-1,+1] */
var		transient	float	RawJoyLookRight;
/** Joypad right thumbstick, vertical axis. Range [-1,+1] */
var		transient	float	RawJoyLookUp;

/** move forward speed scaling */
var()	config		float	MoveForwardSpeed;
/** strafe speed scaling */
var()	config		float	MoveStrafeSpeed;
/** Yaw turn speed scaling */
var()	config		float	LookRightScale;
/** pitch turn speed scaling */
var()	config		float	LookUpScale;

// Postprocess the player's input.
event PlayerInput( float fDeltaTime )
{
	local vector	CamLoc;
	local Rotator	CamRot;

	// Save Raw values
	RawJoyUp		= aBaseY;
	RawJoyRight		= aStrafe;
	RawJoyLookRight	= aTurn;
	RawJoyLookUp	= aLookUp;

	// Pre-Process input...

	// Accelerate Joystick turning rate
	ViewAcceleration( fDeltaTime );

	// Scale to game speed
	aBaseY		*= 100.f*fDeltaTime * MoveForwardSpeed;
	aStrafe		*= 100.f*fDeltaTime * MoveStrafeSpeed;
	aTurn		*= 100.f*fDeltaTime * LookRightScale;
	aLookUp		*= 100.f*fDeltaTime * LookUpScale;

	GetPlayerViewPoint( CamLoc, CamRot );

	if( !IsTimerActive('GetInputTargets') )
	{
		SetTimer(AdhesionTargetFrequencyChecks, true, 'GetInputTargets');
	}

	// Keep target info up to date
	CheckTarget( AdhesionTarget, LastAdhesionTarget, fDeltaTime, CamLoc, CamRot, true  );
	CheckTarget( FrictionTarget, LastFrictionTarget, fDeltaTime, CamLoc, CamRot, false );

	// Friction
	if( bTargetFriction && Pawn != None && FrictionTarget != None ) 
	{
		PerformFriction();
	}

	// Adhesion code
	if( bTargetAdhesion && Pawn != None && AdhesionTarget != None ) 
	{
		PerformAdhesion( fDeltaTime, CamLoc );
	}
	
	// Debug stuffs
	if( bDebugAdhesion )
	{
		DebugDrawFOV( Adhesion_FOV, CamLoc, CamRot, MakeColor(0,255,0,255), TargetAdhesionDist );
	}

	if( bDebugFriction )
	{
		DebugDrawFOV( Friction_FOV, CamLoc, CamRot, MakeColor(0,255,255,255), TargetFrictionDist );
	}

	super.PlayerInput( fDeltaTime );

	if( bAutoCenterPitch )
	{
		AutoPitchCentering( fDeltaTime );
	}
}

/** 
 * Get target pitch range.
 * If within this range, then do not adjust, otherwise, slowly correct view to be in that range.
 */
function float GetTargetCenteredPitch()
{
	return 1024;
}

/**
 * Automatic pitch centering.
 */
function AutoPitchCentering( float fDeltaTime )
{
	local float	CurrentPitch, TargetPitch, Delta;

	CurrentPitch	= FNormalizedRotAxis(Rotation.Pitch);
	TargetPitch		= GetTargetCenteredPitch();

	if( (Abs(CurrentPitch) < TargetPitch) ||
		(aLookUp != 0) ||
		(aForward == 0 && aStrafe == 0) )
	{
		PitchAutoCenterDelayCount = 0.f;
		return;
	}

	if( PitchAutoCenterDelayCount < PitchAutoCenterDelay )
	{
		PitchAutoCenterDelayCount += fDeltaTime;
		return;
	}

	if( CurrentPitch > 0 )
	{
		Delta = DeltaInterpolationTo(CurrentPitch, TargetPitch, fDeltaTime, PitchAutoCenterSpeed) - CurrentPitch;
	}
	else
	{
		Delta = DeltaInterpolationTo(CurrentPitch, -TargetPitch, fDeltaTime, PitchAutoCenterSpeed) - CurrentPitch;
	}

	aLookup += Delta;
}

/** toggle adhesion debugging */
exec function DebugAdhesion()
{
	bDebugAdhesion = !bDebugAdhesion;
	ClientMessage( "bDebugAdhesion" @ bDebugAdhesion, 'Event' );
}

/** toggle friction debugging */
exec function DebugFriction()
{
	bDebugFriction = !bDebugFriction;
	ClientMessage( "bDebugFriction" @ bDebugFriction, 'Event' );
}

/** Draw FOV for debugging */
function DebugDrawFOV( vector2D FOV, vector POVLoc, Rotator POVRot, Color FOVColor, float FOVMaxDist )
{
	local vector	FOVAzimDotLoc, FOVAzimOffset, FOVElevDotLoc, FOVElevOffset;
	local vector	DrawOrigin, ViewDirX, ViewDirY, ViewDirZ;
	local float		DistToAzimDotLoc, DistToElevDotLoc;

	GetAxes( POVRot, ViewDirX, ViewDirY, ViewDirZ );

	if( Pawn == None )
	{
		DrawOrigin = POVLoc + ViewDirX*16;;
	}
	else
	{
		DrawOrigin = Pawn.Location;
	}

	// Draw adhesion FOV bounding box
	DistToAzimDotLoc	= FOVMaxDist * Cos(FOV.X*Pi/180.f);
	FOVAzimDotLoc		= POVLoc + ViewDirX * DistToAzimDotLoc;
	FOVAzimOffset		= ViewDirY * Sqrt(FOVMaxDist**2 - DistToAzimDotLoc**2);

	// FOV Left edge
	DrawDebugLine(DrawOrigin, FOVAzimDotLoc - FOVAzimOffset, FOVColor.R, FOVColor.G, FOVColor.B );
	
	// FOV right edge
	DrawDebugLine(DrawOrigin, FOVAzimDotLoc + FOVAzimOffset, FOVColor.R, FOVColor.G, FOVColor.B );
	
	DistToElevDotLoc = FOVMaxDist * Cos(FOV.Y*Pi/180.f);
	FOVElevDotLoc	 = POVLoc + ViewDirX * DistToElevDotLoc;
	FOVElevOffset	 = ViewDirZ * Sqrt(FOVMaxDist**2 - DistToElevDotLoc**2);

	// FOV top edge
	DrawDebugLine(DrawOrigin, FOVElevDotLoc + FOVElevOffset, FOVColor.R, FOVColor.G, FOVColor.B );
	
	// FOV bottom edge
	DrawDebugLine(DrawOrigin, FOVElevDotLoc - FOVElevOffset, FOVColor.R, FOVColor.G, FOVColor.B );

	// Link em
	DrawDebugLine(FOVAzimDotLoc - FOVAzimOffset, FOVElevDotLoc + FOVElevOffset, FOVColor.R, FOVColor.G, FOVColor.B );
	DrawDebugLine(FOVElevDotLoc + FOVElevOffset, FOVAzimDotLoc + FOVAzimOffset, FOVColor.R, FOVColor.G, FOVColor.B );
	DrawDebugLine(FOVAzimDotLoc + FOVAzimOffset, FOVElevDotLoc - FOVElevOffset, FOVColor.R, FOVColor.G, FOVColor.B );
	DrawDebugLine(FOVElevDotLoc - FOVElevOffset, FOVAzimDotLoc - FOVAzimOffset, FOVColor.R, FOVColor.G, FOVColor.B );
}

/** Hook called from HUD actor. Gives access to HUD and Canvas */
function DrawHUD( HUD H )
{
	local Canvas	C;
	local float		XL, YL, YOffset;

	C = H.Canvas;
	YOffset = 100;
	C.StrLen("MAG", XL, YL);
	C.SetPos(XL, 100);
	
	if( bDebugAdhesion )
	{
		C.DrawColor = MakeColor(64,255,64,255);
		if ( AdhesionTarget == None )
		{
			C.DrawText( "AdhestionTarget: None" );
		}
		else
		{
			C.DrawText( "AdhestionTarget:" $ AdhesionTarget );

			//aTurn		+= DeltaRot.Yaw * Adhesion_Strength.X * AdhesionPct * AdhesionAlpha;
			YOffset += YL;
			C.SetPos( XL, YOffset );
			C.DrawText(	"TargetAnglePct X:"$FPctByRange(AdhesionTarget_DotDist.X, Adhesion_DotFOV.X, 1.f) @
						"Y:"$FPctByRange(AdhesionTarget_DotDist.Y, Adhesion_DotFOV.Y, 1.f) );

		}

		YOffset += YL;
		C.SetPos( XL, YOffset );
	}

	if( bDebugFriction )
	{
		C.DrawColor = MakeColor(0,255,255,255);
		if ( FrictionTarget == None )
		{
			C.DrawText( "FrictionTarget: None" );
		}
		else
		{
			C.DrawText( "FrictionTarget:" $ FrictionTarget );

			YOffset += YL;
			C.SetPos( XL, YOffset );
			C.DrawText(	"TargetAnglePct X:"$FPctByRange(FrictionTarget_DotDist.X, Friction_DotFOV.X, 1.f) @
						"Y:"$FPctByRange(FrictionTarget_DotDist.Y, Friction_DotFOV.Y, 1.f) );

		}
	}

	super.DrawHUD( H );
}

/**
 * Accelerate Joystick turning rate. (Player control helper).
 *
 * @param	DeltaTime	delta time seconds since last update.
 */
function ViewAcceleration( float DeltaTime )
{
	// Skip if no point in doing it
	if( !bMovementInputEnabled || 
		!bTurningInputEnabled )
	{
		return;
	}

	// If above threshold, accelerate Yaw turning rate
	if( Abs(aTurn) > YawAccelThreshold )
	{
		if( YawAccelPct < 1.f )
		{
			YawAccelPct += DeltaTime / YawAccelRampUpTime;
		}

		if( YawAccelPct > 1.f )
		{
			YawAccelPct = 1.f;
		}
	}
	else
	{
		// Otherwise ramp down to normal rate
		if( YawAccelPct > 0.f )
		{
			YawAccelPct -= 4.f*DeltaTime / YawAccelRampUpTime;
		}

		if( YawAccelPct < 0.f )
		{
			YawAccelPct = 0.f;
		}
	}

	if( aTurn != 0.f )
	{
		aTurn *= (1.f + YawAccelMultiplier*YawAccelPct);
	}
}

/**
 * Perform Adhesion
 */
function PerformAdhesion( float fDeltaTime, vector POVLoc )
{
	local Rotator	RotToTarget, DeltaRot;
	local float		AdhesionPct;

	// Update delta rotation from point of view location to adhesion target
	RotToTarget		= Rotator(AdhesionTarget.Location - POVLoc);
	DeltaRot		= Normalize(RotToTarget - LastRotToTarget);
	LastRotToTarget	= RotToTarget;

	// skip if we're not moving
	if( !bIgnoreAdhesionOnNoMove || VSize(Pawn.Velocity) > 0.f )
	{
		// Horizontal adhesion
		AdhesionPct	= FPctByRange(AdhesionTarget_DotDist.X, Adhesion_DotFOV.X, 1.f);
		aTurn		+= DeltaRot.Yaw * Adhesion_Strength.X * AdhesionPct * AdhesionAlpha;

		// Vertical adhesion
		AdhesionPct	= FPctByRange(AdhesionTarget_DotDist.Y, Adhesion_DotFOV.Y, 1.f);
		aLookUp		+= DeltaRot.Pitch * Adhesion_Strength.Y * AdhesionPct * AdhesionAlpha;
	}
}

/**
 * Perform Friction
 */
function PerformFriction()
{
	local float	FrictionPct;

	// Horizontal friction
	FrictionPct	 = FPctByRange(FrictionTarget_DotDist.X, Friction_DotFOV.X, 1.f);
	aTurn		*= Lerp( FrictionPct, 1.f, Friction_RateScale.X );

	// Vertical friction
	FrictionPct	 = FPctByRange(FrictionTarget_DotDist.Y, Friction_DotFOV.Y, 1.f);
	aLookUp		*= Lerp( FrictionPct, 1.f, Friction_RateScale.Y );
}

/** 
 * Returns Adhesion FOV
 *
 * @output	out_FOV		Adhesion FOV (X=azimuth dot FOV, Y=elevation dot FOV).
 * @output	out_Dist	Maximum distance to acquire an adhesion target.
 */
function GetAdhesionFOV( out vector2D out_FOV, out float out_Dist )
{
	out_FOV.X	= Cos(Adhesion_FOV.X*Pi/180.f);
	out_FOV.Y	= Cos(Adhesion_FOV.Y*Pi/180.f);
	out_Dist	= TargetAdhesionDist;
}

/** 
 * Returns Friciton FOV
 *
 * @output	out_FOV		Friction FOV (X=azimuth dot FOV, Y=elevation dot FOV).
 * @output	out_Dist	Maximum distance to acquire an friction target.
 */
function GetFrictionFOV( out vector2D out_FOV, out float out_Dist )
{
	out_FOV.X	= Cos(Friction_FOV.X*Pi/180.f);
	out_FOV.Y	= Cos(Friction_FOV.Y*Pi/180.f);
	out_Dist	= TargetFrictionDist;
}

/** Pick best targets for adhesion and friction */
function GetInputTargets()
{
	local vector		CamLoc;
	local rotator		CamRot;
	local float			BestDist;

	if( Pawn != None ) 
	{
		GetPlayerViewPoint( CamLoc, CamRot );

		GetAdhesionFOV( Adhesion_DotFOV, BestDist );
		LastAdhesionTarget	= AdhesionTarget;
		AdhesionTarget		= PickBestTargetByDot( AdhesionTarget_DotDist, BestDist, Adhesion_DotFOV, CamRot, CamLoc, BestDist );

		GetAdhesionFOV( Friction_DotFOV, BestDist );
		LastFrictionTarget	= FrictionTarget;
		FrictionTarget		= PickBestTargetByDot( FrictionTarget_DotDist, BestDist, Friction_DotFOV, CamRot, CamLoc, BestDist );
	}
}

/** 
 * Check that target is valid.
 *
 * @output	out_CurrentTarget
 * @output	out_LastTarget
 * @param	DeltaTime
 * @param	POVLoc
 * @param	POVRot
 * @param	bAdhesion
 */
function CheckTarget
( 
	out	Actor	out_CurrentTarget, 
	out Actor	out_LastTarget, 
		float	DeltaTime, 
		vector	POVLoc, 
		rotator	POVRot, 
		bool	bAdhesion 
)
{
	if( out_LastTarget != out_CurrentTarget ) 
	{
		// New target acquired
		if( bAdhesion ) 
		{
			AdhesionAlpha = 0.f;
		}

		out_LastTarget = out_CurrentTarget;
	}
	else if( out_CurrentTarget != None ) 
	{
		if( Pawn != None && (out_CurrentTarget.Location - POVLoc) dot Vector(POVRot) < 0.f ) 
		{
			// Lost current target
			out_CurrentTarget = None;

			if( bAdhesion ) 
			{
				AdhesionAlpha = 0.f;
			}
		}
		else if( bAdhesion && AdhesionAlpha < 1.f ) 
		{
			AdhesionAlpha  = FMin( AdhesionAlpha + (DeltaTime / AdhesionRampUpTime), 1.f );
		}
	}
}

/**
 * Pick target closest to a given line
 */
function Actor PickBestTargetByDot
(
	out	Vector2D	out_TargetDotDist, 
	out	float		out_BestDist, 
		Vector2D	FOVLimits,
		Rotator		Orientation, 
		Vector		Origin, 
		float		MaxRange
)
{
	local Actor		A, NewTarget;
	local float		DistToTarget, NewMetric, BestMetric;
	local vector	AxisX, AxisY, AxisZ, out_ClosestPointToEnemy;
	local vector2D	DotDist;

	// player aim direction coordinate system
	GetAxes( Orientation, AxisX, AxisY, AxisZ );
	BestMetric = 0;

	ForEach CollidingActors ( class'Actor', A, MaxRange, Origin )
	{
		if( IsValidAdhesionTarget(A) )
		{
			GetAdjustedDotDistToTarget(DotDist, out_ClosestPointToEnemy, A, Origin, AxisX, AxisY, AxisZ);

			DistToTarget = VSize(out_ClosestPointToEnemy - Origin);

			// convert to metric valuation
			DotDist.X = Abs(DotDist.X);
			DotDist.Y = 1.f - Abs(DotDist.Y);

			if( DotDist.X   >= 0.f			&&
				DotDist.X   >= FOVLimits.X	&&
				DotDist.Y  >= 0.f			&&
				DotDist.Y  >= FOVLimits.Y	&&
				DistToTarget <= MaxRange	)
			{
				NewMetric = DotDist.X + DotDist.Y;
				if( NewMetric > BestMetric &&
					IsActorVisible(A) ) 
				{
					NewTarget			= A;
					out_TargetDotDist	= DotDist;
					out_bestDist		= DistToTarget;
					BestMetric			= NewMetric;
				}
			}
		}
	}

	return NewTarget;
}

/** 
 * Returns true if actor is visible 
 * quick rewrite of AController::LineOfSightTo for testing
 * with improved behind test, edge tests, and radius modification when leaning.
 */
function bool IsActorVisible( Actor A )
{
	local Vector	ViewLoc, ViewDir, TestLoc, AX, AY, AZ;
	local Rotator	ViewRot;
	local float		ActorRadius, ActorHeight;
	local vector	TestPoints[4];
	local int		i;

	GetPlayerViewPoint( ViewLoc, ViewRot );
	ViewDir = Vector(ViewRot);

	// Fast test, make sure Actor is not behind ViewPoint
	// Take in account Actor's bounding cylinder (for large actors).
	A.GetBoundingCylinder(ActorRadius, ActorHeight);
	TestLoc = A.Location + ViewDir * ActorRadius;
	// FIXME Laurent, use Pawn->PeripheralVision instead of 0.f
	if( ((TestLoc - ViewLoc) dot ViewDir) < 0.f )
	{
		return false;
	}

	// Test At center of Actor
	if( FastTrace(A.Location, ViewLoc) )
	{
		if( bDebugAdhesion )
		{
			DrawDebugLine(A.Location, Pawn.Location, 255, 0, 0 );
		}
		return true;
	}

	if( A.IsA('WarPawn') )
	{
		// If Pawn is leaning, then extend collision radius
		if( WarPawn(A).CoverAction == CA_LeanLeft ||
			WarPawn(A).CoverAction == CA_LeanRight ||
			WarPawn(A).CoverAction == CA_StepLeft ||
			WarPawn(A).CoverAction == CA_StepRight )
		{
			ActorRadius *= 1.25f;
		}
	}

	// only check sides if width of other is significant compared to distance
	if( ActorRadius / Vsize(A.Location - ViewLoc) < 0.01f )
	{
		return false;
	}

	// Check 4 "corners" on collision cylinder
	GetAxes( Rotator(ViewLoc-A.Location), AX, AY, AZ );
	TestPoints[0] = A.Location +  AY * ActorRadius + AZ * ActorHeight * 0.75;
	TestPoints[1] = A.Location -  AY * ActorRadius + AZ * ActorHeight * 0.75;
	TestPoints[2] = A.Location +  AY * ActorRadius - AZ * ActorHeight * 0.75;
	TestPoints[3] = A.Location -  AY * ActorRadius - AZ * ActorHeight * 0.75;

	for( i=0; i<4; i++)
	{
		if( bDebugAdhesion )
		{
			DrawDebugLine(TestPoints[i], Pawn.Location, 255, 0, 0 );
		}
		if( FastTrace(TestPoints[i], ViewLoc) )
		{
			return true;
		}
	}

	// not visible
	return false;
}

/**
 * Return true if actor is a valid target for adhesion.
 */
function bool IsValidAdhesionTarget( Actor Target )
{
	local Pawn	PTarget;

	if( Target.bDeleteMe )
	{
		return false;
	}

	if( Target.IsA('Pawn') )
	{
		PTarget = Pawn(Target);

		// look for best controlled pawn target which is not on same team
		if( PTarget != Pawn	&&
			PTarget.Health > 0 &&
			PTarget.bProjTarget	&&	
			!PTarget.Level.GRI.OnSameTeam(Pawn,PTarget) )
		{
			return true;
		}
	}

	return false;
}

/**
 * Returns adjusted dotted distance to target.
 * That is Azimuth and Elevation dot values of the direction to target based on the reference axis system (AxisX,AxisY,AxisZ).
 * This function picks the closest point to the target using the Target's collision cylinder.
 * If the direction goes through the collision cylinder, then it assumes being aligned (Azimuth=1.f,Elevation=0,f)
 *
 * @see Object::GetDotDistance()
 * Note:	Azimuth (.X) sign is changed to represent left/right and not front/behind. front/behind is the funtion's return value.
 *
 * @param	out_DotDist		.X = 'Direction' dot AxisX relative to plane (AxisX,AxisZ). (== Cos(Azimuth))
 *							.Y = 'Direction' dot AxisX relative to plane (AxisX,AxisY). (== Sin(Elevation))
 * @param	out_distance	distance to Picked Location (Target's location, edge of collision cylinder?)
 * @param	Direction		Vector.
 * @param	AxisX			X component of reference system.
 * @param	AxisY			Y component of reference system.
 * @param	AxisZ			Z component of reference system.
 */
function GetAdjustedDotDistToTarget
( 
	out	vector2D	out_DotDist,
	out vector		out_AdjustedTargetLocation,
		Actor		Target, 
		vector		StartLoc, 
		vector		AxisX,
		vector		AxisY,
		vector		AxisZ
)
{
	local vector	TargetDir, ClosestColEdgeDir, ClosestColEdgeLoc, TargDirX, TargDirY, TargDirZ, AzimEdgeOffset, ElevEdgeOffset;
	local float		DistToTarget, TargColRadius, TargColHeight;
	local int		AzimSign, ElevSign;
	local vector2D	TargetDotDist, EdgeDotDist;

	// Direction to Target
	TargetDir		= Normal(Target.Location - StartLoc);
	DistToTarget	= VSize(Target.Location - StartLoc);

	GetAxes( Rotator(TargetDir), TargDirX, TargDirY, TargDirZ );

	// Get dot distance of target direction
	GetDotDistance( TargetDotDist, TargetDir, AxisX, AxisY, AxisZ );
	AzimSign = 1;
	if( TargetDotDist.X >= 0 )
	{
		AzimSign = -1;
	}

	ElevSign = 1;
	if( TargetDotDist.Y >= 0 )
	{
		ElevSign = -1;
	}

	// Find closest collision cylinder edge based on target's orientation
	// we're trying to find the closest point of the target's collision cylinder to our aim direction (==AxisX)
	Target.GetBoundingCylinder(TargColRadius, TargColHeight); 
	AzimEdgeOffset		= AzimSign * TargColRadius * TargDirY;
	ElevEdgeOffset		= ElevSign * TargColHeight * TargDirZ;
	ClosestColEdgeLoc	= Target.Location + AzimEdgeOffset + ElevEdgeOffset;
	ClosestColEdgeDir	= Normal(ClosestColEdgeLoc  - StartLoc);

	// Get dot distance of collision cylinder edge
	GetDotDistance( EdgeDotDist, ClosestColEdgeDir, AxisX, AxisY, AxisZ );

	// this is the final adjusted target location we pick for dot diff. Depending if col. cylinder edge is closer or not.
	out_AdjustedTargetLocation = Target.Location;

	// Figure out Azimuth
	if( ((TargetDotDist.X < 0.f && EdgeDotDist.X > 0.f) || (TargetDotDist.X > 0.f && EdgeDotDist.X < 0.f)) &&
		((TargetDotDist.Y < 0.f && EdgeDotDist.Y > 0.f) || (TargetDotDist.Y > 0.f && EdgeDotDist.Y < 0.f)) )
	{
		// if we aim right at the collision cylinder, assume we're aligned
		out_DotDist.X = 1.f;
		out_DotDist.Y = 0.f;
	}
	else
	{
		// Pick Target
		out_DotDist = TargetDotDist;

		// And adjust to collision edge location, if it's closer to our crosshair
		if ( Abs(TargetDotDist.X) < Abs(EdgeDotDist.X) )
		{
			out_DotDist.X = EdgeDotDist.X;
			out_AdjustedTargetLocation += AzimEdgeOffset;
		}

		if ( Abs(TargetDotDist.Y) > Abs(EdgeDotDist.Y) )
		{
			out_DotDist.Y = EdgeDotDist.Y;
			out_AdjustedTargetLocation += ElevEdgeOffset;
		}
	}

	if ( bDebugAdhesion )
	{
		// debug, line to picked adjusted location for dot dist.
		DrawDebugLine(Pawn.Location, out_AdjustedTargetLocation, 255, 0, 255 );
	}
}

defaultproperties
{
	MoveForwardSpeed=1200
	MoveStrafeSpeed=1200
	LookRightScale=300
	LookUpScale=-250

	YawAccelThreshold=0.95f
	YawAccelMultiplier=2.f
	YawAccelRampUpTime=0.67f
	
	bAutoCenterPitch=true
	PitchAutoCenterDelay=0.5
	PitchAutoCenterSpeed=0.67

	bTargetFriction=true
	TargetFrictionDist=4096.0
	Friction_FOV=(X=20,Y=30)
	Friction_RateScale=(X=0.55,Y=0.45)

	AdhesionTargetFrequencyChecks=0.25
	bTargetAdhesion=true
	bIgnoreAdhesionOnNoMove=true
	AdhesionRampUpTime=1.5
	TargetAdhesionDist=4096.0
	Adhesion_FOV=(X=55,Y=55)
	//Adhesion_Strength=(X=0.20,Y=0.40)
	Adhesion_Strength=(X=1.0,Y=1.0)
}