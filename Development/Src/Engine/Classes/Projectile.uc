//=============================================================================
// Projectile.
//
// A delayed-hit projectile that moves around for some time after it is created.
//=============================================================================
class Projectile extends Actor
	abstract
	native;

//-----------------------------------------------------------------------------
// Projectile variables.

// Motion information.
var		float   Speed;					// Initial speed of projectile.
var		float   MaxSpeed;				// Limit on speed of projectile (0 means no limit)
var		float	TossZ;
var		bool	bSwitchToZeroCollision; // if collisionextent nonzero, and hit actor with bBlockNonZeroExtents=0, switch to zero extent collision
var		Actor	ZeroCollider;

var		bool	bBegunPlay;

// Damage attributes.
var   float    Damage;
var	  float	   DamageRadius;
var   float	   MomentumTransfer; // Momentum magnitude imparted by impacting projectile.
var   class<DamageType>	   MyDamageType;

// Projectile sound effects
var   SoundCue	SpawnSound;		// Sound made when projectile is spawned.
var   SoundCue ImpactSound;		// Sound made when projectile hits something.

// explosion effects
// AJS_HACK var   class<Projector> ExplosionDecal;
var   float		ExploWallOut;	// distance to move explosions out from wall
var Controller	InstigatorController;

cpptext
{
	void BoundProjectileVelocity();
	virtual UBOOL ShrinkCollision(AActor *HitActor, const FVector &StartLocation);
    virtual UBOOL IsAProjectile() { return true; }
    virtual AProjectile* GetAProjectile() { return this; } 
}

//==============
// Encroachment
function bool EncroachingOn( actor Other )
{
	if ( Brush(Other) != None )
		return true;
		
	return false;
}

simulated function PostBeginPlay()
{
	//local PlayerController PC;

    if ( Role == ROLE_Authority && Instigator != None && Instigator.Controller != None )
    {
    	// FIXMESTEVE if ( Instigator.Controller.ShotTarget != None && Instigator.Controller.ShotTarget.Controller != None )
	//		Instigator.Controller.ShotTarget.Controller.ReceiveProjectileWarning( Self );

		InstigatorController = Instigator.Controller;
    }
	bBegunPlay = true;
}

/* Init()
initialize velocity and rotation of projectile
*/
function Init(vector Direction)
{
	SetRotation(Rotator(Direction));
	Velocity = Speed * Direction;
}

simulated function bool CanSplash()
{
	return bBegunPlay;
}

function Reset()
{
	Destroy();
}

//==============
// Touching
simulated singular event Touch( Actor Other, vector HitLocation, vector HitNormal )
{
	if ( Other == None ) // Other just got destroyed in its touch?
		return;

	if ( Other.bProjTarget || Other.bBlockActors )
	{
		ProcessTouch(Other, HitLocation, HitNormal);
	}
}

simulated function ProcessTouch(Actor Other, Vector HitLocation, Vector HitNormal)
{
	if ( Other != Instigator )
		Explode( HitLocation, HitNormal );
}

simulated function HitWall(vector HitNormal, actor Wall)
{
	if ( Role == ROLE_Authority )
	{
		MakeNoise(1.0);
	}

	Explode(Location + ExploWallOut * HitNormal, HitNormal);
}

simulated function BlowUp(vector HitLocation)
{
	HurtRadius(Damage,DamageRadius, MyDamageType, MomentumTransfer, HitLocation );
	if ( Role == ROLE_Authority )
		MakeNoise(1.0);
}

simulated function Explode(vector HitLocation, vector HitNormal)
{
	Destroy();
}

simulated final function RandSpin(float spinRate)
{
	DesiredRotation = RotRand();
	RotationRate.Yaw = spinRate * 2 *FRand() - spinRate;
	RotationRate.Pitch = spinRate * 2 *FRand() - spinRate;
	RotationRate.Roll = spinRate * 2 *FRand() - spinRate;	
}

defaultproperties
{
	Begin Object Name=CollisionCylinder
		CollisionRadius=0
		CollisionHeight=0
	End Object

	bCanBeDamaged=true
	DamageRadius=+220.0
	MaxSpeed=+02000.000000
	Speed=+02000.000000
	bCollideActors=true
	bCollideWorld=true
	bNetTemporary=true
	bGameRelevant=true
	bReplicateInstigator=true
	Physics=PHYS_Projectile
	LifeSpan=+0014.000000
	NetPriority=+00002.500000
	MyDamageType=class'DamageType'
	RemoteRole=ROLE_SimulatedProxy
	TossZ=+100.0
}
