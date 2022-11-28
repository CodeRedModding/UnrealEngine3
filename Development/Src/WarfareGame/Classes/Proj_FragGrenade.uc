/** 
 * Grenade_Frag
 * Frag Grenade projectile implementation
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class Proj_FragGrenade extends Proj_Grenade;

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
		EmitActor = Instigator.Spawn( class'Emit_FragGrenadeExplosion',,, HitLocation + HitNormal*3, rotator(HitNormal) );
		CauseCameraShake( Instigator, HitLocation+HitNormal*3, default.DamageRadius*4.f );
	}

	if ( Instigator.Role == Role_Authority )
	{	// Hurt Radius
		Instigator.Weapon.HurtRadius( default.Damage, default.DamageRadius, default.MyDamageType, default.MomentumTransfer, HitLocation+HitNormal*3);
	}
}

defaultproperties
{
	Begin Object Class=DrawSphereComponent Name=DrawSphere0
			SphereColor=(R=0,G=255,B=0,A=255)
			SphereRadius=400.0
			HiddenGame=true
	End Object
	Components.Add(DrawSphere0)

	Damage=200
	DamageRadius=400
	MyDamageType=class'DamType_FragGrenade'
	MomentumTransfer=1600

	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
    	StaticMesh=StaticMesh'COG_Grunt_FragGrenade.COG_Grunt_FragGrenade_SMesh'
		CollideActors=false
		BlockActors=false
		BlockZeroExtent=false
		BlockNonZeroExtent=false
		BlockRigidBody=false
    End Object

    Begin Object Class=TransformComponent Name=TransformComponent0
    	TransformedComponent=StaticMeshComponent0
    End Object
	Components.Add(TransformComponent0)
}