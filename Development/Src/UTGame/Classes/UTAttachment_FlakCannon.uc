/**
 * Created By:	Joe Wilcox
 * Copyright:	(c) 2004
 * Company:		Epic Games
*/

class UTAttachment_FlakCannon extends UTWeaponAttachment;

defaultproperties
{

	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
		StaticMesh=StaticMesh'FlakCannon.Model.S_Weapons_FlakCannon_Model_3P'
		bOwnerNoSee=true
		CollideActors=false
	End Object
    Mesh=StaticMeshComponent0

	// Weapon Mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMesh0
    	TransformedComponent=StaticMeshComponent0
		Translation=(X=-2.0,Y=0.0,Z=2.0)
		Rotation=(Roll=49152)
		Scale3D=(X=0.3,Y=0.3,Z=0.3)
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
