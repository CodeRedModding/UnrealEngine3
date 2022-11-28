/** 
 * Tyro Pillar Turret Cannon
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class Weap_TyroTurret extends WarWeapon;

State Firing
{
	/** @see Weapon::FireAmmunition */
	function FireAmmunition( byte FireModeNum )
	{
		ProjectileFire();
	}

	/** @see Weapon::GetProjectileClass */
	simulated function class<Projectile> GetProjectileClass()
	{
		return class'Proj_RPGRocket';
	}
}

defaultproperties
{
	FireSound=SoundCue'LD_TempUTSounds.ScorchPrimary'

	ReloadDuration=0
	MagazineSize=0
	FireOffset=(X=-80,Y=3,Z=13)
	CriticalAmmoCount=0

	ItemName="Tyro Pillar Turret Cannon"
	FiringStatesArray(0)=Firing
	FireModeInfoArray(0)=(FireRate=120,Recoil=200,Inaccuracy=3,Name="Auto")

	// Muzzle Flash point light
    Begin Object Class=PointLightComponent Name=LightComponent0
		Brightness=0
        Color=(R=64,G=160,B=255,A=255)
        Radius=256
    End Object
    MuzzleFlashLight=LightComponent0

	// Muzzle Flash point light Transform component
    Begin Object Class=TransformComponent Name=PointLightTransformComponent0
    	TransformedComponent=LightComponent0
		Translation=(X=-100,Y=0,Z=20)
    End Object
    MuzzleFlashLightTransform=PointLightTransformComponent0
}