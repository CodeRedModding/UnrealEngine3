/** 
 * RPG Rocket Projectile
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class Proj_RPGRocket extends Projectile
	config(Weapon);

var()	config	ParticleSystem		ImpactEffect;
var()	config	SoundCue			ExplosionSound;

simulated function ProcessTouch(Actor Other, Vector HitLocation, Vector HitNormal)
{
	if ( Other == Instigator )
		return;

	if ( !Other.IsA('Projectile') || Other.bProjTarget )
	{
		if ( Other != Instigator )
		{
			Explode(HitLocation, HitNormal);
		}
	}
}

simulated function HitWall(vector HitNormal, actor Wall)
{
	Explode(Location, HitNormal);
}

simulated function Explode(vector HitLocation, vector HitNormal)
{
	local Emitter EmitActor;

	if ( Level.NetMode != NM_DedicatedServer )
	{
		EmitActor = Instigator.Spawn( class'Emit_FragGrenadeExplosion',,, HitLocation + HitNormal*3, rotator(HitNormal) );
		CauseCameraShake( Instigator, HitLocation+HitNormal*3, default.DamageRadius*4.f );
	}

	if ( Role == Role_Authority )
	{	// Hurt Radius
		HurtRadius( default.Damage, default.DamageRadius, default.MyDamageType, default.MomentumTransfer, HitLocation+HitNormal*3);
	}

	Destroy();
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
	Speed=3000
	MaxSpeed=3000
	Damage=200
	DamageRadius=400
	MyDamageType=class'DamType_FragGrenade'
	MomentumTransfer=1600

	ExplosionSound=SoundCue'LD_TempUTSounds.BExplosion3'
	ImpactEffect=ParticleSystem'WarFare_Effects.Grandade_Explo'

	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
    	StaticMesh=StaticMesh'COG_AssaultRifle.Mesh.BulletChunk'
        HiddenGame=false
        CastShadow=false
		CollideActors=false
		BlockActors=false
		BlockZeroExtent=false
		BlockNonZeroExtent=false
		BlockRigidBody=false
    End Object

    Begin Object Class=TransformComponent Name=TransformComponent0
    	TransformedComponent=StaticMeshComponent0
        Scale=0.25
    End Object

	RemoteRole=ROLE_SimulatedProxy
	Components.Add(TransformComponent0)
    Lifespan=2.0
	bNetInitialRotation=true
}