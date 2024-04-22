//=============================================================================
// PhysicsVolume:  a bounding volume which affects actor physics
// Each Actor is affected at any time by one PhysicsVolume
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class PhysicsVolume extends Volume
	native
	nativereplication
	placeable;

/** This property is a bit like gravity, though it can be used to apply force in any
 direction using the three axes. Objects within the volume will be accelerated
 up to the designated velocity while taking friction values into effect
 */
var()		interp vector		ZoneVelocity;

var()		bool				bVelocityAffectsWalking;	// Will the zone velocity affect walking?
var()		float				GroundFriction;				// This property allows you to control how much friction you will have as you move across the ground while within the volume. This can be used to simulate slippery surfaces such as ice or oil
var()		float				TerminalVelocity;			// Terminal velocity
var()		float				DamagePerSec;				// This property allows a volume to damage a player as long as they are within it. Using a negative value will allow the volume to have a regenerative effect. NB. bPainCausing must be set to TRUE to activate this
var()		class<DamageType>	DamageType<AllowAbstract>;	// When damage is applied to an object, it is done so using a specific DamageType. Each available DamageType has a KDamageImpulse property which controls the magnitude of the impulse to be applied along the momentum vector. Runtime users should be aware that due to the lack of default weapons and the limited number of damage types, this property will not be extremely useful.
var()		int					Priority;					// This property determines which PhysicsVolume takes precedence if they overlap
var()		float				FluidFriction;				// This property controls the amount of friction applied by the volume as you move through it. The higher this value, the harder it will feel to move through

var()		bool				bPainCausing;				// This property activates the ability to cause damage. Used in conjunction with DamagePerSecond and PainInterval

var()		float				PainInterval;				// Amount of time, in seconds, between damage applications. NB. bPainCausing must be set to TRUE to activate this
var()		bool				bAIShouldIgnorePain;		// If this property is TRUE AI should not treat paths inside this volume differently even if the volume causes pain
var()		bool				bEntryPain;					// This property describes whether or not pain should be caused when something enters the volume - this is in addition to damage per second. NB. bPainCausing must be set to TRUE to activate this
var			bool				BACKUP_bPainCausing;
var()		bool				bDestructive;				// Destroys most actors which enter it
var()		bool				bNoInventory;				// If set, any items dropped within this volume will have a limited lifetime
var()		bool				bMoveProjectiles;			// this velocity zone should impart velocity to projectiles and effects
var()		bool				bBounceVelocity;			// this velocity zone should bounce actors that land in it
var()		bool				bNeutralZone;				// Players can't take damage in this zone

var()		bool				bCrowdAgentsPlayDeathAnim;	// If TRUE, crowd agents entering this volume play their death animation
var()		bool				bPhysicsOnContact;			// By default, the origin of an Actor must be inside a PhysicsVolume for it to affect it. If this flag is true though, if this Actor touches the volume at all, it will affect it
var()		float				RigidBodyDamping;			// This controls the force that will be applied to PHYS_RigidBody objects in this volume to get them to match the ZoneVelocity

var()		float				MaxDampingForce;			// Applies a cap on the maximum damping force that is applied to objects

var			bool				bWaterVolume;				// If set to TRUE, this volume becomes a water volume - applying FluidFriction and allowing sound effects etc as the player enters and exits the water
var			Info				PainTimer;

var			Controller			DamageInstigator;			// Controller that gets credit for any damage caused by this volume

var	transient PhysicsVolume		NextPhysicsVolume;

struct CheckpointRecord
{
	var bool bPainCausing;
	var bool bActive;
};

cpptext
{
	virtual void Spawned();

	virtual void PostLoad();

	virtual void BeginDestroy();

	virtual void Serialize(FArchive& Ar);

	void Register();

	void Unregister();

	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	void SetZone( UBOOL bTest, UBOOL bForceRefresh );
	virtual UBOOL ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags);
	virtual UBOOL WillHurt(APawn *P);
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

	virtual FLOAT GetVolumeRBGravityZ() { return GetGravityZ(); }
}

native function float GetGravityZ();
native function vector GetZoneVelocityForActor(Actor TheActor);

simulated event PostBeginPlay()
{
	Super.PostBeginPlay();

	BACKUP_bPainCausing	= bPainCausing;

	if ( Role < ROLE_Authority )
		return;
	if ( bPainCausing )
	{
		PainTimer = Spawn(class'VolumeTimer', self);
	}
}

/* Reset() - reset actor to initial state - used when restarting level without reloading. */
function Reset()
{
	bPainCausing	= BACKUP_bPainCausing;
	bForceNetUpdate = TRUE;
}

/* Called when an actor in this PhysicsVolume changes its physics mode
*/
event PhysicsChangedFor(Actor Other);

event ActorEnteredVolume(Actor Other);
event ActorLeavingVolume(Actor Other);

event PawnEnteredVolume(Pawn Other);
event PawnLeavingVolume(Pawn Other);

simulated function OnToggle( SeqAct_Toggle inAction )
{
	// don't allow this action to modify the collision of static volumes as we won't be able to update the client
	if (!bStatic || RemoteRole > ROLE_None)
	{
		Super.OnToggle(inAction);
	}

	if (inAction.InputLinks[0].bHasImpulse)
	{
		// Turn on pain if that was it's original state
		bPainCausing = BACKUP_bPainCausing;
	}
	else if (inAction.InputLinks[1].bHasImpulse)
	{
		// Turn off pain
		bPainCausing = FALSE;
	}
	else if (inAction.InputLinks[2].bHasImpulse)
	{
		// Toggle pain off, or on if original state caused pain
		bPainCausing = !bPainCausing && BACKUP_bPainCausing;
	}
}

simulated event CollisionChanged()
{
	// disable Volume behaviour of toggling rigid body collision...
}

/*
TimerPop
damage touched actors if pain causing.
since PhysicsVolume is static, this function is actually called by a volumetimer
*/
function TimerPop(VolumeTimer T)
{
	local Actor A;

	if ( T == PainTimer )
	{
		if ( !bPainCausing )
			return;

		ForEach TouchingActors(class'Actor', A)
		{
			if ( A.bCanBeDamaged && !A.bStatic )
			{
				CausePainTo(A);
			}
		}
	}
}

simulated event Touch( Actor Other, PrimitiveComponent OtherComp, vector HitLocation, vector HitNormal )
{
	Super.Touch(Other, OtherComp, HitLocation, HitNormal);
	if ( (Other == None) || Other.bStatic )
		return;
	if ( bNoInventory && (DroppedPickup(Other) != None) && (Other.Owner == None) )
	{
		Other.LifeSpan = 1.5;
		return;
	}
	if ( bMoveProjectiles && (ZoneVelocity != vect(0,0,0)) )
	{
		if ( Other.Physics == PHYS_Projectile )
			Other.Velocity += ZoneVelocity;
			else if ( (Other.Base == None) && Other.IsA('Emitter') && (Other.Physics == PHYS_None) )
		{
			Other.SetPhysics(PHYS_Projectile);
			Other.Velocity += ZoneVelocity;
		}
	}
	if ( bPainCausing )
	{
		if ( Other.bDestroyInPainVolume )
		{
			Other.VolumeBasedDestroy(self);
			return;
		}
		if (bEntryPain && Other.bCanBeDamaged)
		{
			CausePainTo(Other);
		}
	}
}

function CausePainTo(Actor Other)
{
	if (DamagePerSec > 0)
	{
		if ( WorldInfo.bSoftKillZ && (Other.Physics != PHYS_Walking) )
			return;
		if ( (DamageType == None) || (DamageType == class'DamageType') )
			`log("No valid damagetype ("$DamageType$") specified for "$PathName(self));
		Other.TakeDamage(DamagePerSec*PainInterval, DamageInstigator, Location, vect(0,0,1), DamageType,, self);
	}
	else
	{
		Other.HealDamage(-DamagePerSec * PainInterval, DamageInstigator, DamageType);
	}
}

/** called from GameInfo::SetPlayerDefaults() on the Pawn's PhysicsVolume after the its default movement properties have been restored
 * allows the volume to reapply any movement modifiers on the Pawn
 */
function ModifyPlayer(Pawn PlayerPawn);

/** notification when a Pawn inside this volume becomes the ViewTarget for a PlayerController */
function NotifyPawnBecameViewTarget(Pawn P, PlayerController PC);

/** Kismet hook to set DamageInstigator */
function OnSetDamageInstigator(SeqAct_SetDamageInstigator Action)
{
	DamageInstigator = Action.GetController(Action.DamageInstigator);
}

function bool ShouldSaveForCheckpoint()
{
	return (bPainCausing != BACKUP_bPainCausing);
}

function CreateCheckpointRecord(out CheckpointRecord Record)
{
	Record.bPainCausing = bPainCausing;
}

function ApplyCheckpointRecord(const out CheckpointRecord Record)
{
	bPainCausing = Record.bPainCausing;
}

defaultproperties
{
	Begin Object Name=BrushComponent0
		CollideActors=true
		BlockActors=false
		BlockZeroExtent=true
		BlockNonZeroExtent=true
		BlockRigidBody=false
	End Object

	MaxDampingForce=1000000.0
	FluidFriction=+0.3
	bVelocityAffectsWalking=true
	TerminalVelocity=4000.0
	bAlwaysRelevant=true
	bOnlyDirtyReplication=true
	GroundFriction=+00008.000000
	NetUpdateFrequency=0.1
	bSkipActorPropertyReplication=true
	DamageType=class'Engine.DamageType'
	bEntryPain=true
	PainInterval=1.f

	// LDs might just want to toggle pain, which is server only
	// we prevent the collision toggle from working in cases where that wouldn't replicate
	bForceAllowKismetModification=true
}
