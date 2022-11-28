/** 
 * Proj_Grenade
 * Grenade Projectile Implementation
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class Proj_Grenade extends Projectile
	abstract
	native;

/** Simulate gravity */
var()	bool	bPerformGravity;

/** Scale Gravity force */
var()	float	fGravityScale;

/** Maximum number of bounces */
var()	int		MaxBounceCount;

//
// Internal
//

/** Number of times grenade has bounced */
var	int		BounceCount;

/** Velocity is scaled by this value after each bounce */
var	float	VelocityDampingFactor;

/** Bounce reflected velocity is scaled by this value. Affects bounciness of grenade */
var float	fBounciness;

/** Time grenade has been cooked */
var RepNotify float	CookedTime;

//
// debugging
//
/** cached positions of grenade to redraw everything */
var	Array<Vector>	CachedPositions;

/** when true, grenade is frozen. (freezes physics) */
var	bool			bFreeze;

cpptext
{
	void physProjectile(FLOAT deltaTime, INT Iterations);
}

replication
{
	unreliable if ( (bNetInitial || bNetDirty) && (Role==ROLE_Authority) )
		CookedTime;
}

event ReplicatedEvent(string VarName)
{
	if ( VarName ~= "CookedTime" )
	{	// if CookedTime is set, adjust grenade's lifespan accordingly
		SetCookedTime( CookedTime );
	}
}

simulated event Destroyed()
{
	Explode( Location, Vect(0,0,1) );
}

/**
 * Render grenade trajectory
 */
simulated function Tick( float DeltaTime )
{
	local vector	Start;
	local int		i;
	local PlayerController PC;

	// do not draw if not visible
	ForEach LocalPlayerControllers(PC)
		break;
	if ( PC == None )
		return;

	// Add actual position to cache for rendering
	if ( bBounce )
		CachedPositions[CachedPositions.Length] = Location;

	// render cached trajectory
	if ( CachedPositions.Length > 1 )
	{
		Start = CachedPositions[0];
		for (i=1; i<CachedPositions.Length; i++)
		{
			DrawDebugLine(Start, CachedPositions[i], 0, 255, 0 );
			Start = CachedPositions[i];
		}
	}

}

/**
 * Set time grenade has been cooked (held in player's hand with pin pulled.
 * reduces LifeSpan accordingly.
 *
 * @param	TimeCooked		Time grenade was held, deducted from its LifeSpan.
 */
simulated function SetCookedTime( float TimeCooked )
{
	CookedTime	 = TimeCooked;
	LifeSpan	-= TimeCooked;
}

simulated singular event Touch( Actor Other, vector HitLocation, vector HitNormal )
{
	//log("Touch Other:" $ Other );
	if ( ShoundBounce( Other, Instigator, BounceCount ) )
		Bounce( HitLocation, HitNormal );
}

simulated event Bump( Actor Other, Vector HitNormal )
{
	//log("Bump Other:" @ Other );
	if ( ShoundBounce( Other, Instigator, BounceCount ) )
		Bounce( Location, HitNormal );
}

simulated function HitWall(vector HitNormal, actor Wall)
{
	//log("HitWall Wall:" @ Wall );
	if ( ShoundBounce( Wall, Instigator, BounceCount ) )
		Bounce( Location, HitNormal );
}

/**
 * Reflect grenade off a wall
 *
 * @param	HitLocation
 * @param	HitNormal
 */
simulated function Bounce( Vector HitLocation, Vector HitNormal )
{
	local float	TestSpeed;

	//log( "DoRealCollision BounceCount:" $ BounceCount @ Level.TimeSeconds );
	if ( !bBounce )
		return;

	BounceCount++;
	UpdateBounceVelocity( Velocity, HitLocation, HitNormal );
	
	TestSpeed = VSize(Velocity);
	if ( TestSpeed < 10 || BounceCount > MaxBounceCount ) 
	{
		bBounce = false;
		SetPhysics(PHYS_None);
		return;
	}

	RandSpin( 100000 );
}

/**
 * Test condition to validate a rebound
 * Made static, so can be used for grenade simulation
 *
 * @param	Touched, actor the grenade touched
 * @param	Inst, Instigator (Pawn who did throw the grenande)
 * @param	bounceNb, number of bounces
 * @return	true if the grenade should bounce off
 */
final static simulated function bool ShoundBounce( Actor Touched, Actor Inst, int BounceNb )
{
	if ( Touched == None )
		return false;

	if ( Touched.bWorldGeometry )
		return true;

	if ( !Touched.bProjTarget && !Touched.bBlockActors )  
		return false;

	// cannot collide with instigator until it has bounced once
	if ( BounceNb > 1 || (Touched != Inst) )
		return true;

	return false;
}

/**
 * Update velocity to bounce off grenade
 * Made static, because also used for HUD rendered simulation
 *
 * @input	out_Velocity, current velocity, to update
 * @param	HitLocation
 * @param	HitNormal
 */
final static simulated function UpdateBounceVelocity( out Vector out_Velocity, Vector HitLocation, Vector HitNormal )
{
	out_Velocity	+= -2.f * default.fBounciness * (out_Velocity Dot HitNormal) * HitNormal;	// bounce off wall with damping
	out_Velocity	*= default.VelocityDampingFactor;
}

simulated function Explode(vector HitLocation, vector HitNormal)
{
	StaticExplosion( Instigator, HitLocation, HitNormal );
}

/**
 * Grenade explodes
 * Made static, so can be called when cooking grenade, and player failed to throw it in time
 *
 * @param	HitLocation
 * @param	HitNormal
 */
static simulated function StaticExplosion( Pawn Instigator, vector HitLocation, vector HitNormal )
{
	local Emitter EmitActor;

	if ( Instigator.Level.NetMode != NM_DedicatedServer )
	{
		EmitActor = Instigator.Spawn( class'Emitter',,, HitLocation + HitNormal*3, rotator(HitNormal) );
		EmitActor.SetTemplate( ParticleSystem'WeaponEffects.SparkOneShot', true );

		CauseCameraShake( Instigator, HitLocation+HitNormal*3, default.DamageRadius*4.f );
	}
}

/** FIXME LAURENT Hacked in for greenlight. This should become a cleaner generic function at some point */
static function CauseCameraShake( Pawn Instigator, vector ShakeEpiCenter, float ShakeRadius )
{
	local Controller	C;
	local float			Pct, DistToOrigin;
	local vector		CamLoc;
	local rotator		CamRot;

	// propagate to all player controllers
	for ( C=Instigator.Level.ControllerList; C!=None; C=C.NextController )
	{
		if( C.IsA('PlayerController') )
		{
			C.GetPlayerViewPoint(CamLoc, CamRot);
			DistToOrigin = VSize(ShakeEpiCenter - CamLoc);
			if( DistToOrigin < ShakeRadius )		
			{
				Pct = 1.f - (DistToOrigin / ShakeRadius);
				
				// remove me, lazy effect boost
				Pct = 2.f * Pct;
				PlayerController(C).CameraShake
				(	
					Pct * 1.f, 
					Pct * vect(100,100,200), 
					Pct * vect(10,10,25),  
					Pct * vect(0,3,5), 
					Pct * vect(1,10,20),
					Pct * 2,
					Pct * 5
				);
			}
		}
	}


}

defaultproperties
{
	VelocityDampingFactor=0.8
	fBounciness=0.6
	bPerformGravity=true
	fGravityScale=2.0f
	MaxBounceCount=8

	bBounce=true
	Physics=PHYS_Projectile
	Speed=1500
	MaxSpeed=0
	Damage=50
	MomentumTransfer=100
	MyDamageType=class'DamageType'

	bNetInitialRotation=true
	RemoteRole=ROLE_SimulatedProxy
    Lifespan=4.0
}
