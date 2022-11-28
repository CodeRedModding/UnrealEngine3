/** 
 * Bullet tracer projectile
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class Proj_BulletTracer extends Projectile;

var()	TransformComponent	TransCmpnt;

simulated function ProcessTouch(Actor Other, Vector HitLocation, Vector HitNormal)
{
	if ( Other == Instigator )
		return;

	if ( !Other.IsA('Projectile') || Other.bProjTarget )
	{
		if ( Other != Instigator )
		{
			if ( !Other.IsA('Pawn') )
			{
				// Don't spawn bullet impact effect on pawns.
				SpawnExplosionEffect(HitLocation, HitNormal);
			}
			Explode(HitLocation, HitNormal);
		}
	}
}

simulated function HitWall(vector HitNormal, actor Wall)
{
	SpawnExplosionEffect(Location, HitNormal);
	Explode(Location, HitNormal);
}

simulated function SpawnExplosionEffect( vector HitLocation, vector HitNormal )
{
	local Emitter EmitActor;

	if ( Level.NetMode != NM_DedicatedServer )
	{
		EmitActor = Spawn( class'Emit_BulletImpact',,, HitLocation + HitNormal*3, rotator(HitNormal) );
		EmitActor.RemoteRole = ROLE_None;
	}
}

simulated function Explode(vector HitLocation, vector HitNormal)
{
	Destroy();
}

defaultproperties
{
	Lifespan=2.0
	Speed=7500
	MaxSpeed=7500
	Damage=0
	MomentumTransfer=0
	MyDamageType=class'WarDamageType'

	Begin Object Name=CollisionCylinder
		CollisionRadius=0
		CollisionHeight=0
		HiddenGame=false
	End Object

	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
    	StaticMesh=StaticMesh'COG_AssaultRifle.Mesh.TracerMesh'
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
        Scale3D=(X=1.0,Y=0.2,Z=0.2)
		Translation=(X=175)
    End Object
	TransCmpnt=TransformComponent0
	Components.Add(TransformComponent0)

	RemoteRole=ROLE_SimulatedProxy
	bNetInitialRotation=true
}