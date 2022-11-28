/**
 * WarInventoryManager
 * Warfare inventory definition
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class WarInventoryManager extends InventoryManager
	config(Weapon);

/** 
 * Player Accuracy modifier struct. 
 * Holds variables together in a simple struct, for easier editing/management 
 */
struct PlayerAccuracyModifierStruct
{
	/** crouch modifier */
	var()	float		Crouch;			
	/** target mode modifier */
	var()	float		TargetMode;
	/** character moving modifier */
	var()	float		Moving;
	/** player looking around modifier */
	var()	float		ViewTurning;

	structdefaults
	{
		Crouch=0.67
		TargetMode=0.5
		Moving=1.5
		ViewTurning=1.0
	}
};
var()	PlayerAccuracyModifierStruct	PlayerAccuracyModifiers;

/** default Accuracy interpolation speed */
var()	float							AccuracyInterpSpeed;

//
// Internal
//

/** Weapon Recoil Offset applied to player view. */
var				Rotator	WeaponRecoilOffset;

/** Stacked Recoil Pitch modifications, for later compensation */
var				float	RecoilStackedPitch;
/** Recoil compensation rate */
var()	config	float	RecoilCompensationRate;
/** delay in seconds before compensating recoil */
var()	config	float	RecoilCompensationDelay;
/** last time (seconds) when last recoil modification was received. In order to start compensation after <RecoilCompensationDelay> seconds. */
var				float	LastRecoilTime;

/** TimeStamp since last GetPlayerAccuracy() was called to properly interpolate it */
var		float			AccuracyTimeStamp;

/** Player Accuracy. Do not read/write, use accessor function GetPlayerAccuracy() */
var		float			Accuracy;

/** last player view rotation, to affect accuracy */
var		rotator			AccuracyLastViewRot;

/** draw weapon debug info on HUD */
var		bool			bShowWeaponDebug;


/** @see InventoryManager::StartFire */
simulated function StartFire( byte FireModeNum )
{
	local WarPC	PC;

	PC = WarPC(Pawn(Owner).Controller);

	// Fire weapon only if allowed to
	if( PC == None || PC.CanFireWeapon() )
	{
		super.StartFire( FireModeNum );
	}
}

/**
 * Switches to Previous weapon
 * Network: Client
 */
simulated function PrevWeapon()
{
	if ( WarWeapon(Pawn(Owner).Weapon) != None && WarWeapon(Pawn(Owner).Weapon).DoOverridePrevWeapon() )
		return;

	super.PrevWeapon();
}

/**
 *	Switches to Next weapon
 *	Network: Client
 */
simulated function NextWeapon()
{
	if ( WarWeapon(Pawn(Owner).Weapon) != None && WarWeapon(Pawn(Owner).Weapon).DoOverrideNextWeapon() )
		return;

	super.NextWeapon();
}

/**
 * Cycles to current weapon's next fire mode
 */
exec simulated function SetNextFireMode()
{
	if ( WarWeapon(Pawn(Owner).Weapon) != None )
		WarWeapon(Pawn(Owner).Weapon).SetNextFireMode();
}

/**
 * Forces to reload current weapon
 * i.e change current magazine with a new one
 */
exec simulated function ForceReloadWeapon()
{
	if ( WarWeapon(Pawn(Owner).Weapon) != None )
		WarWeapon(Pawn(Owner).Weapon).ForceReload();
}

/**
 * Add recoil to RecoilOffset, which is added later to player view rotation
 * Recoil is affected by several modifiers applied in GetWeaponRecoilModifier()
 *
 * @param	PitchRecoil, Pitch offset added to player view rotation
 */
simulated function SetWeaponRecoil( int PitchRecoil )
{
	WeaponRecoilOffset.Pitch	+= PitchRecoil;
	WeaponRecoilOffset.Yaw		+= (0.5f - FRand()) * PitchRecoil;

	// save weapon recoil for compensation
	RecoilStackedPitch	+= PitchRecoil;
	LastRecoilTime		 = Level.TimeSeconds;
}

/**
 * Called from PlayerController::UpdateRotation() -> PlayerController::ProcessViewRotation() -> Pawn::ProcessViewRotation() 
 * to (pre)process player ViewRotation.
 * adds delta rot (player input), applies any limits and post-processing
 * returns the final ViewRotation set on PlayerController
 *
 * @param	DeltaTime, time since last frame
 * @param	ViewRotation, actual PlayerController view rotation
 * @input	out_DeltaRot, delta rotation to be applied on ViewRotation. Represents player's input.
 * @return	processed ViewRotation to be set on PlayerController.
 */
simulated function ProcessViewRotation( float DeltaTime, out rotator out_ViewRotation, out Rotator out_DeltaRot )
{
	local Weapon	ActiveWeapon;
	local float		fRecoilCompensatedAmount;

	// Add Weapon recoil
	if ( WeaponRecoilOffset != Rot(0,0,0) )
	{
		out_DeltaRot += WeaponRecoilOffset;
		WeaponRecoilOffset = Rot(0,0,0);
	}

	// Recoil compensation
	if( RecoilStackedPitch > 0 &&
		LastRecoilTime + RecoilCompensationDelay < Level.TimeSeconds )
	{
		fRecoilCompensatedAmount = RecoilStackedPitch - DeltaInterpolationTo( RecoilStackedPitch, 0.f, DeltaTime, RecoilCompensationRate );
		RecoilStackedPitch		-= fRecoilCompensatedAmount;
		out_DeltaRot.Pitch		-= fRecoilCompensatedAmount;
	}

	// Give weapon a chance to modify playercontroller's viewrotation
	ActiveWeapon = Pawn(Owner).Weapon;
	if ( WarWeapon(ActiveWeapon) != None )
	{	
		WarWeapon(ActiveWeapon).ProcessViewRotation( DeltaTime, out_ViewRotation, out_DeltaRot );
	}
}

/**
 * Returns a modifier (scalar) that defines player accuracy.
 * Modifier is defined by several parameters such as player speed, posture, targetting mode etc..
 * Accuracy is then used for weapon firing and other things like affecting recoil.
 *
 * @return	Accuracy, float value. 0 = perfect aim, 1.f worst aim.
 */
simulated function float GetPlayerAccuracy()
{
	local float		fDeltaTime, fModifier, interpSpeed;
	local Rotator	DeltaRot, ViewRot;
	local Vector	ViewLoc;
	local bool		bTargetingMode;
	local WarPawn wfPawn;

	if (WarPC(Instigator.Controller) != None)
	{
		bTargetingMode = WarPC(Instigator.Controller).bTargetingMode;
	}

	fModifier	= 1.f;
	interpSpeed = AccuracyInterpSpeed;
	fDeltaTime	= Level.TimeSeconds - AccuracyTimeStamp;

	if ( fDeltaTime == 0.f )
		return Accuracy;

	// ViewRotation modifier
	if ( AccuracyLastViewRot == Rot(0,0,0) )
		AccuracyLastViewRot = ViewRot;
	Instigator.GetActorEyesViewPoint( ViewLoc, ViewRot );
	DeltaRot = ViewRot - AccuracyLastViewRot;
	AccuracyLastViewRot = ViewRot;

	// Movement modifier
	if ( VSize(Instigator.Velocity) > 1 )
		fModifier *= PlayerAccuracyModifiers.Moving;
	else if ( DeltaRot != rot(0,0,0) )
	{
		fModifier *= PlayerAccuracyModifiers.ViewTurning;
		interpSpeed /= 2;
	}
	else
	{
		// Standing Still modifier
		fModifier = 0.f;

		// if not in targeting mode, slower accuracy recovery
		if ( bTargetingMode )
			interpSpeed /= 2;
	}

	// Crouch modifier
	if ( Instigator.bIsCrouched )
		fModifier *= PlayerAccuracyModifiers.Crouch;

	// targeting mode modifier
	if ( bTargetingMode )
		fModifier *= PlayerAccuracyModifiers.TargetMode;

	// limit to 0.f -> 1.f range
	fModifier = FClamp(fModifier,0.f,1.f);

	// blind fire modifier
	wfPawn = WarPawn(Instigator);
	if (wfPawn != None &&
		(wfPawn.CoverType == CT_Standing ||
		 wfPawn.CoverType == CT_MidLevel) &&
		!bTargetingMode)
	{
		// clamp the min accuracy when blind firing
		fModifier = FMax(fModifier,0.75f);
	}

	// interpolated accuracy
	Accuracy = DeltaInterpolationTo( Accuracy, fModifier, fDeltaTime, interpSpeed );
	AccuracyTimeStamp = Level.TimeSeconds;

	return Accuracy;
}

/**
 * Allows InventoryManager to force or deny player walking.
 * Sets Pawn.bIsWalking. true means Pawn walks.. affects velocity.
 *
 * @param	bNewIsWalking, new bIsWalking flag to set on Pawn.
 */
simulated function bool ForceSetWalking( bool bNewIsWalking )
{
	local Weapon	ActiveWeapon;

	ActiveWeapon = Pawn(Owner).Weapon;
	if ( WarWeapon(ActiveWeapon) != None )
	{	// Give weapon a chance to force or deny player walking
		bNewIsWalking = WarWeapon(ActiveWeapon).ForceSetWalking( bNewIsWalking );
	}

	return bNewIsWalking;
}

/**
 * Overrides Player Targeting mode
 *
 * @return	true if TargetingMode should be ignored.
 */
simulated function bool OverrideTargetingMode()
{
	if ( WarWeapon(Pawn(Owner).Weapon) != None )
	{	// Give weapon a chance to override targeting mode
		return WarWeapon(Pawn(Owner).Weapon).OverrideTargetingMode();
	}
	return false;
}

exec function ShowWeaponDebug()
{
	bShowWeaponDebug = !bShowWeaponDebug;
}

/** Hook called from HUD actor. Gives access to HUD and Canvas */
simulated function DrawHUD( HUD H )
{
	super.DrawHUD( H );
	if ( bShowWeaponDebug && WarWeapon(Pawn(Owner).Weapon) != None )
	{
		WarWeapon(Pawn(Owner).Weapon).DrawWeaponDebug( H );
	}
}

defaultproperties
{
	Accuracy=1.f
	AccuracyInterpSpeed=10.f
	RecoilCompensationRate=4
	RecoilCompensationDelay=0.33
}
