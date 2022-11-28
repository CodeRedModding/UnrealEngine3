class UTWeap_InstagibRifle extends UTWeapon;

/* No ammo use for instagib rifle */

simulated function float GetAmmoLeftPercent( byte FireModeNum )
{
	return 1;
}

simulated function bool CheckAmmo( byte FireModeNum, optional int Amount )
{
	return true;
}

simulated function bool HasAnyAmmo()
{
	return true;
}

defaultproperties
{
	WeaponColor=(R=255,G=0,B=64,A=255)
	FireInterval(0)=+1.1
	FireInterval(1)=+1.1
	ItemName="Instagib Rifle"
	//FireSound=
	PlayerViewOffset=(X=0.0,Y=7.0,Z=-9.0)

	// Weapon SkeletalMesh
	Begin Object Class=StaticMeshComponent Name=MeshComponent0
		StaticMesh=StaticMesh'ShockRifle.Model.S_Weapons_ShockRifle_Model_3P'
		bOnlyOwnerSee=true
        CastShadow=false
		CollideActors=false
	End Object
    Mesh=MeshComponent0

	// Weapon Mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMesh0
    	TransformedComponent=MeshComponent0
		Translation=(X=0.0,Y=0.0,Z=0)
		Scale3D=(X=0.2,Y=0.2,Z=0.2)
		Rotation=(Yaw=32768,roll=-16384)
    End Object
	MeshTransform=TransformComponentMesh0

	AttachmentClass=class'UTGame.UTAttachment_InstagibRifle'
	Components.Add(TransformComponentMesh0)

	PickupMessage="You picked up a Super ShockRifle"

	// Pickup staticmesh

	// Pickup Mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMesh1

		Begin Object Class=StaticMeshComponent Name=StaticMeshComponent1
			StaticMesh=StaticMesh'ShockRifle.Model.S_Weapons_ShockRifle_Model_3P'
			bOnlyOwnerSee=false
	        CastShadow=false
			CollideActors=false
		End Object


    	TransformedComponent=StaticMeshComponent1
		Translation=(X=0.0,Y=0.0,Z=-20.0)
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

	InstantHitDamage(0)=1000
	InstantHitDamage(1)=1000

 	InstantHitDamageTypes(0)=class<UTDmgType_Instagib>
	InstantHitDamageTypes(1)=class<UTDmgType_Instagib>

    WeaponEquipSnd=SoundCue'ShockRifle.SC_SwitchToShockRifle'
    WeaponPutDownSnd=SoundCue'ShockRifle.SC_SwitchToShockRifle'

    WeaponFireSnd(0)=SoundCue'ShockRifle.SC_InstagibRifleFire'
    WeaponFireSnd(1)=SoundCue'ShockRifle.SC_InstagibRifleFire'


}
