/** 
 * COG RPG
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class Weap_RPG extends WarWeapon;

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

	ReloadDuration=4.0
	MagazineSize=3
	FireOffset=(X=-80,Y=3,Z=13)
	CriticalAmmoCount=1

	ItemName="RPG"
	WeaponFireAnim=Shoot
	FiringStatesArray(0)=Firing
	FireModeInfoArray(0)=(FireRate=60,Recoil=200,Inaccuracy=3,Name="Auto")

	// Weapon Animation
	Begin Object Class=AnimNodeSequence Name=aWeaponIdleAnim
    	AnimSeqName=Idle
	End Object

	// Weapon SkeletalMesh
	Begin Object Class=SkeletalMeshComponent Name=SkeletalMeshComponent0
		SkeletalMesh=SkeletalMesh'COG_LaserRifle.COG_LaserRifle_AMesh'
		Animations=aWeaponIdleAnim
		AnimSets(0)=AnimSet'COG_LaserRifle.COG_LaserRifleAnimSet'
		CollideActors=false
	End Object
    Mesh=SkeletalMeshComponent0

	// Weapon Mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMesh0
    	TransformedComponent=SkeletalMeshComponent0
		Translation=(X=-9.0,Y=3.0,Z=3.50)
		Scale3D=(X=0.57,Y=0.57,Z=0.57)
		Rotation=(Pitch=0,Roll=0,Yaw=-32768)
    End Object
	MeshTranform=TransformComponentMesh0

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

	HUDElements(0)=(bEnabled=True,DrawX=0.040000,DrawY=0.018000,bCenterX=False,bCenterY=False,DrawColor=(B=255,G=255,R=255,A=128),DrawW=256,DrawH=128,DrawIcon=Texture2D'WarfareHudGFX.HUD.HUD_Weapon_T_ScorchRifleIcon',DrawU=0,DrawV=0,DrawUSize=256,DrawVSize=128,DrawScale=0.700000,DrawFont=None,DrawLabel="")
	HUDElements(1)=(bEnabled=True,DrawX=0.150000,DrawY=0.060000,bCenterX=False,bCenterY=False,DrawColor=(B=200,G=200,R=200,A=128),DrawW=0,DrawH=0,DrawIcon=None,DrawU=0,DrawV=0,DrawUSize=-1,DrawVSize=-1,DrawScale=1.000000,DrawFont=Font'EngineFonts.TinyFont',DrawLabel="ROCKETS")
}