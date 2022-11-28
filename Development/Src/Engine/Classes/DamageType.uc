//=============================================================================
// DamageType, the base class of all damagetypes.
// this and its subclasses are never spawned, just used as information holders
//=============================================================================
class DamageType extends object
	native
	abstract;

// Description of a type of damage.
var() localized string     	DeathString;	 				// string to describe death by this type of damage
var() localized string		FemaleSuicide, MaleSuicide;		// Strings to display when someone dies
var() string			   	DamageWeaponName; 				// weapon that caused this damage
var() bool					bArmorStops;					// does regular armor provide protection against this damage
var() bool                  bAlwaysGibs;
var() bool                  bLocationalHit;
var() bool                  bSkeletize;         // swap model to skeleton
var() bool					bCausesBlood;
var() bool					bKUseOwnDeathVel;	// For ragdoll death. Rather than using default - use death velocity specified in this damage type.

var() float					GibModifier;

var(RigidBody)	float		KDamageImpulse;		// magnitude of impulse applied to KActor due to this damage type.
var(RigidBody)  float		KDeathVel;			// How fast ragdoll moves upon death
var(RigidBody)  float		KDeathUpKick;		// Amount of upwards kick ragdolls get when they die

var float VehicleDamageScaling;		// multiply damage by this for vehicles
var float VehicleMomentumScaling;

/** The forcefeedback waveform to play when you take damage */
var ForceFeedbackWaveform DamagedFFWaveform;

/** The forcefeedback waveform to play when you are killed by this damage type */
var ForceFeedbackWaveform KilledFFWaveform;

static function IncrementKills(Controller Killer);

static function ScoreKill(Controller Killer, Controller Killed)
{
	IncrementKills(Killer);
}

static function string DeathMessage(PlayerReplicationInfo Killer, PlayerReplicationInfo Victim)
{
	return Default.DeathString;
}

static function string SuicideMessage(PlayerReplicationInfo Victim)
{
	if ( Victim.bIsFemale )
		return Default.FemaleSuicide;
	else
		return Default.MaleSuicide;
}

static function string GetWeaponClass()
{
	return "";
}

defaultproperties
{
	DeathString="%o was killed by %k."
	FemaleSuicide="%o killed herself."
	MaleSuicide="%o killed himself."
	bArmorStops=true
	GibModifier=+1.0
    bLocationalHit=true
	bCausesBlood=true
	KDamageImpulse=800
	VehicleDamageScaling=+1.0
    VehicleMomentumScaling=+1.0
}