/**
 * Created By:	Joe Wilcox
 * Copyright:	(c) 2004
 * Company:		Epic Games
*/

class UTWeap_FlakCannon extends UTWeapon;

const SPREADDIST=1400;


/** Contains the # of flak shards to fire each time */

var int NoShardsPerFire;

function CustomFire()
{
	local int i;
    local rotator Adjustment;
   	local vector RealStartLoc, HitLocation, HitNormal, AimDir;
	local TraceHitInfo	HitInfo;
	local Projectile Proj;

	IncrementFlashCount(CurrentFireMode);

	// Trace where crosshair is aiming at. Get hit info.
	CalcWeaponFire( HitLocation, HitNormal, AimDir, HitInfo );

	// this is the location where the projectile is spawned
	RealStartLoc	= GetPhysicalFireStartLoc();

	AimDir  = normal(HitLocation - RealStartLoc);

	for (i=0;i<NoShardsPerFire;i++)
	{
        Adjustment.Yaw   = SPREADDIST * (FRand()-0.5);
        Adjustment.Pitch = SPREADDIST * (FRand()-0.5);
        Adjustment.Roll  = SPREADDIST * (FRand()-0.5);

		Proj = Spawn( GetProjectileClass(),,, RealStartLoc );
		if (Proj!=None)
			Proj.Init( AimDir >> Adjustment );
    }
}


defaultproperties
{

	WeaponColor=(R=255,G=255,B=128,A=255)
	FireInterval(0)=+0.8947
	FireInterval(1)=+0.9
	ItemName="Flak Cannon"
	PlayerViewOffset=(X=0.0,Y=7.0,Z=-9.0)

	Begin Object class=AnimNodeSequence Name=MeshSequenceA
	End Object

	Begin Object Class=SkeletalMeshComponent Name=MeshComponentA
		SkeletalMesh=SkeletalMesh'FlakCannon.Model.SK_Weapons_FlakCannon_Envy'
		PhysicsAsset=none
		AnimSets(0)=AnimSet'FlakCannon.Anims.K_Weapons_Anims_FlakCannon_Envy'
		Animations=MeshSequenceA
		AlwaysLoadOnClient=true
		AlwaysLoadOnServer=true
		CastShadow=false
	End Object
	Mesh=MeshComponentA

	// Weapon Mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMeshA
    	TransformedComponent=MeshComponentA
		Translation=(X=-5,Y=-10,Z=-25)
		Rotation=(Yaw=-16384)
		Scale3D=(X=1.0,Y=1.0,Z=1.0)
		scale=0.70
    End Object
	MeshTransform=TransformComponentMeshA

	AttachmentClass=class'UTGame.UTAttachment_RocketLauncher'
	Components.Add(TransformComponentMeshA)


	PickupMessage="You picked up a Flak Cannon"

	// Pickup mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMesh1

		Begin Object Class=StaticMeshComponent Name=StaticMeshComponent1
			StaticMesh=StaticMesh'FlakCannon.Model.S_Weapons_FlakCannon_Model_3P'
			bOnlyOwnerSee=false
	        CastShadow=false
			CollideActors=false
		End Object


    	TransformedComponent=StaticMeshComponent1
		Translation=(X=0.0,Y=0.0,Z=-10.0)
		Rotation=(Yaw=32768)
		Scale3D=(X=0.3,Y=0.3,Z=0.3)
    End Object
	DroppedPickupMesh=TransformComponentMesh1
	PickupFactoryMesh=TransformComponentMesh1

	// Lighting

	Begin Object class=PointLightComponent name=MuzzleFlashLightC
		Brightness=1.0
		Color=(R=255,G=255,B=128)
		Radius=255
		CastShadows=True
		bEnabled=false
	End Object
	MuzzleFlashLight=MuzzleFlashLightC

	// Muzzle Flashlight Positioning
	Begin Object Class=TransformComponent Name=TransformComponentMesh2
    	TransformedComponent=MuzzleFlashLightC
    	Translation=(X=-60,Z=5)
    End Object
	MuzzleFlashLightTransform=TransformComponentMesh2

	MuzzleFlashLightDuration=0.3
	MuzzleFlashLightBrightness=2.0

	WeaponFireSnd(0)=SoundCue'FlakCannon.Sounds.SC_FlakCannonFire'
	WeaponFireSnd(1)=SoundCue'FlakCannon.Sounds.SC_FlakCannonAltFire'

 	WeaponFireTypes(0)=EWFT_Custom
	WeaponFireTypes(1)=EWFT_Projectile
	WeaponProjectiles(0)=class'UTProj_FlakShard'
	WeaponProjectiles(1)=class'UTProj_FlakShell'

	WeaponFireAnim=WeaponFire
	WeaponPutDownAnim=WeaponPutDown
	WeaponEquipAnim=WeaponEquip

	FireOffset=(X=25)
	NoShardsPerFire=9
}
