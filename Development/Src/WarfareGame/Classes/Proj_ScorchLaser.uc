/** 
 * Scorch Rifle Laser Projectile
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class Proj_ScorchLaser extends Projectile;

var()	ParticleSystem		ImpactEffect;

simulated function ProcessTouch(Actor Other, Vector HitLocation, Vector HitNormal)
{
	if ( Other == Instigator )
		return;

	if ( !Other.IsA('Projectile') || Other.bProjTarget )
	{
		if ( Role == Role_Authority )
		{
			Other.TakeDamage(Damage, Instigator, HitLocation, Vector(Rotation)*MomentumTransfer, MyDamageType);
		}

		if ( Other != Instigator )
			Explode(HitLocation, HitNormal);
	}
}

simulated function HitWall(vector HitNormal, actor Wall)
{
	if ( Role == Role_Authority )
	{
		Wall.TakeDamage(Damage, Instigator, Location, Normal(Velocity)*MomentumTransfer, MyDamageType);
	}
	Explode(Location, HitNormal);
}

simulated function Explode(vector HitLocation, vector HitNormal)
{
	local Emitter EmitActor;

	if ( Level.NetMode != NM_DedicatedServer )
	{
		EmitActor = Spawn( class'Emitter',,, HitLocation + HitNormal*3, rotator(HitNormal) );
		EmitActor.SetTemplate( ImpactEffect, true );
		EmitActor.RemoteRole = ROLE_None;
	}

	Destroy();
}

defaultproperties
{
	Speed=3000
	MaxSpeed=3000
	Damage=20
	MomentumTransfer=1000
	MyDamageType=class'WarDamageType'

	ImpactEffect=ParticleSystem'WarFare_Effects.Scorch_Impact_Blue'

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