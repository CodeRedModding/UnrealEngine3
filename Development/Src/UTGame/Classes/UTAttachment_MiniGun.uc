
class UTAttachment_MiniGun extends UTWeaponAttachment;

simulated event ImpactEffects()
{
	local Emitter E;
	local vector HitLocation,HitNormal;

	super.ImpactEffects();

	if ( UTPawn(Owner) != none )
	{
		HitLocation = UTPawn(Owner).FlashLocation;
		HitNormal = normal(Owner.Location-HitLocation);

		E = Spawn( class'Emitter',,, HitLocation, rotator(HitNormal) );
		E.SetTemplate( ParticleSystem'WeaponEffects.SparkOneShot', true );// OLD ParticleSystem'WeaponEffects.SparkOneShot' );
	}

}

defaultproperties
{

	// Weapon SkeletalMesh
	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
		StaticMesh=StaticMesh'Stinger.Model.S_Weapons_Stinger_Model_3P'
		bOwnerNoSee=true
		bOnlyOwnerSee=false
		CollideActors=false
		AlwaysLoadOnClient=true
		AlwaysLoadOnServer=true
	End Object
    Mesh=StaticMeshComponent0

	// Weapon mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMesh0
    	TransformedComponent=StaticMeshComponent0
		Translation=(X=-7.0,Y=1.0,Z=1.00)
		Rotation=(Pitch=1024,Roll=49152)
		Scale3D=(X=0.3,Y=0.3,Z=0.3)
		scale=1.2
    End Object
	MeshTransform=TransformComponentMesh0

	Begin Object class=PointLightComponent name=MuzzleFlashLightC
		Brightness=1.0
		Color=(R=255,G=255,B=128)
		Radius=255
		CastShadows=True
		bEnabled=false
	End Object
	MuzzleFlashLight=MuzzleFlashLightC

	// Muzzle Flashlight Positioning
	Begin Object Class=TransformComponent Name=TransformComponentMesh1
    	TransformedComponent=MuzzleFlashLightC
    	Translation=(X=-60,Z=5)
    End Object
	MuzzleFlashLightTransform=TransformComponentMesh1


	MuzzleFlashLightDuration=0.3
	MuzzleFlashLightBrightness=2.0


}
