//=============================================================================
// PhysicsVolume:  a bounding volume which affects actor physics
// Each Actor is affected at any time by one PhysicsVolume
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class PhysicsVolume extends Volume
	native
	nativereplication;

var()		vector		ZoneVelocity;
var()		vector		Gravity;
var			vector		BACKUP_Gravity;

var()		float		GroundFriction;
var()		float		TerminalVelocity;
var()		float		DamagePerSec;
var() class<DamageType>	DamageType;
var()		int			Priority;	// determines which PhysicsVolume takes precedence if they overlap
var() float  FluidFriction;

var()		bool	bPainCausing;			// Zone causes pain.
var			bool	BACKUP_bPainCausing;
var()		bool	bDestructive;			// Destroys most actors which enter it.
var()		bool	bNoInventory;
var()		bool	bMoveProjectiles;		// this velocity zone should impart velocity to projectiles and effects
var()		bool	bBounceVelocity;		// this velocity zone should bounce actors that land in it
var()		bool	bNeutralZone;			// Players can't take damage in this zone.

/** 
 *	By default, the origin of an Actor must be inside a PhysicsVolume for it to affect it.
 *	If this flag is true though, if this Actor touches the volume at all, it will affect it.
 */
var()		bool	bPhysicsOnContact;

var			bool	bWaterVolume;
var	Info PainTimer;

// Rigid Body
var(RigidBody) float KExtraLinearDamping; // Extra damping applied to Rigid Body actors in this volume.
var(RigidBody) float KExtraAngularDamping;
var(RigidBody) float KBuoyancy;			  // How buoyant Rigid Body things are in this volume (if bWaterVolume true). Multiplied by Actors KParams->KBuoyancy.

var PhysicsVolume NextPhysicsVolume;

cpptext
{
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	void SetZone( UBOOL bTest, UBOOL bForceRefresh );
void CheckForErrors();
}

replication
{
	// Things the server should send to the client.
	reliable if( bNetDirty && (Role==ROLE_Authority) )
		Gravity;
}

simulated function PostNetBeginPlay()
{
	Super.PostNetBeginPlay();

	BACKUP_Gravity		= Gravity;
	BACKUP_bPainCausing	= bPainCausing;

	if ( Role < ROLE_Authority )
		return;
	if ( bPainCausing )
		PainTimer = Spawn(class'VolumeTimer', self);
}

/* Reset() - reset actor to initial state - used when restarting level without reloading. */
function Reset()
{
	Gravity			= BACKUP_Gravity;
	bPainCausing	= BACKUP_bPainCausing;
	NetUpdateTime = Level.TimeSeconds - 1;
}

/* Called when an actor in this PhysicsVolume changes its physics mode
*/
event PhysicsChangedFor(Actor Other);

event ActorEnteredVolume(Actor Other);
event ActorLeavingVolume(Actor Other);

event PawnEnteredVolume(Pawn Other);
event PawnLeavingVolume(Pawn Other);

/*
TimerPop
damage touched actors if pain causing.
since PhysicsVolume is static, this function is actually called by a volumetimer
*/
function TimerPop(VolumeTimer T)
{
	local actor A;
	local bool bFound;

	if ( T == PainTimer )
	{
		if ( !bPainCausing )
			return;

		ForEach TouchingActors(class'Actor', A)
			if ( A.bCanBeDamaged && !A.bStatic )
			{
				CausePainTo(A);
				bFound = true;
			}
	}
}

simulated event Touch( Actor Other, vector HitLocation, vector HitNormal )
{
	Super.Touch(Other, HitLocation, HitNormal);
	if ( Other == None )
		return;
		if ( bNoInventory && (Pickup(Other) != None) && (Other.Owner == None) )
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
			Other.Destroy();
			return;
		}
			if ( Other.bCanBeDamaged && !Other.bStatic )
			{
		CausePainTo(Other);
}
	}
}


function CausePainTo(Actor Other)
{
	local float depth;
	local Pawn P;

	// FIXMEZONE figure out depth of actor, and base pain on that!!!
	depth = 1;
	P = Pawn(Other);

	if ( DamagePerSec > 0 )
	{
		if ( Region.Zone.bSoftKillZ && (Other.Physics != PHYS_Walking) )
			return;
		Other.TakeDamage(int(DamagePerSec * depth), None, Location, vect(0,0,0), DamageType);
	}	
	else
	{
		if ( (P != None) && (P.Health < P.HealthMax) )
			P.Health = Min(P.HealthMax, P.Health - depth * DamagePerSec);
	}
}

defaultproperties
{
	Begin Object Name=BrushComponent0
		CollideActors=true
		BlockActors=false
		BlockZeroExtent=false
		BlockNonZeroExtent=true
		BlockRigidBody=false
	End Object

    Gravity=(X=0.000000,Y=0.000000,Z=-750.000000)
	FluidFriction=+0.3
    TerminalVelocity=+02500.000000
	bAlwaysRelevant=true
	bOnlyDirtyReplication=true
    GroundFriction=+00008.000000
	KBuoyancy=1.0
	NetUpdateFrequency=0.1
	bSkipActorPropertyReplication=true
}