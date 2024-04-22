/**
 * DamageType, the base class of all damagetypes.
 * this and its subclasses are never spawned, just used as information holders
 *
 * NOTE:  we can not do:  HideDropDown on this class as we need to be able to use it in SeqEvent_TakeDamage for objects taking
 * damage from any DamageType!
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class DamageType extends object
	native
	abstract;

var() bool					bArmorStops;				// does regular armor provide protection against this damage

var   bool					bCausedByWorld;				//this damage was caused by the world (falling off level, into lava, etc)
var   bool					bExtraMomentumZ;			// Add extra Z to momentum on walking pawns to throw them up into the air

/** Can break bits off FracturedStaticMeshActors. */
var() bool					bCausesFracture;

var(RigidBody)	float		KDamageImpulse;				// magnitude of impulse applied to KActor due to this damage type.
var(RigidBody)  float		KDeathVel;					// How fast ragdoll moves upon death
var(RigidBody)  float		KDeathUpKick;				// Amount of upwards kick ragdolls get when they die

/** Size of impulse to apply when doing radial damage. */
var(RigidBody)	float		RadialDamageImpulse;

/** When applying radial impulses, whether to treat as impulse or velocity change. */
var(RigidBody)	bool		bRadialDamageVelChange;

/** multiply damage by this for vehicles */
var float VehicleDamageScaling;							

/** multiply momentum by this for vehicles */
var float VehicleMomentumScaling;

/** The forcefeedback waveform to play when you take damage */
var ForceFeedbackWaveform DamagedFFWaveform;

/** The forcefeedback waveform to play when you are killed by this damage type */
var ForceFeedbackWaveform KilledFFWaveform;

/** Damage imparted by this damage type to fracturable meshes.  Scaled by config WorldInfo.FracturedMeshWeaponDamage. */
var float FracturedMeshDamage;

static function float VehicleDamageScalingFor(Vehicle V)
{
	return Default.VehicleDamageScaling;
}

defaultproperties
{
	bArmorStops=true
	KDamageImpulse=800
	VehicleDamageScaling=+1.0
	VehicleMomentumScaling=+1.0
	bExtraMomentumZ=true
	FracturedMeshDamage=1.0
}
