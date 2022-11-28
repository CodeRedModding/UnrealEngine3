/** 
 * Scorch Rifle Laser Projectile Red Geist version
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class Proj_ScorchLaserRed extends Proj_ScorchLaser;

defaultproperties
{
	ImpactEffect=ParticleSystem'AW-Particles.ScorchSparks_Red'

	Begin Object Name=StaticMeshComponent0
		Materials(0)=Material'COG_AssaultRifle.Materials.ScorchBulletMat_Red'
    End Object

    Begin Object Name=TransformComponent0
		Scale3D=(X=1.0,Y=0.5,Z=0.5)
    End Object
}