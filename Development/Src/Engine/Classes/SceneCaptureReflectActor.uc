/**
 * SceneCaptureReflectActor
 *
 * Place this actor in a level to capture the reflected/clipped scene
 * to a texture target using a SceneCaptureReflectComponent
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SceneCaptureReflectActor extends SceneCaptureActor
	native
	placeable;

/** draws the face using a static mesh */
var() const StaticMeshComponent StaticMesh;

/** material instance used to apply the target texture to the static mesh */
var transient MaterialInstanceConstant ReflectMaterialInst;

cpptext
{
	// SceneCaptureActor interface

	/**
	* Update any components used by this actor
	*/
	virtual void SyncComponents();

	// AActor interface

	virtual void Spawned();

	// UObject interface

	virtual void FinishDestroy();
	virtual void PostLoad();

private:

	/**
	* Init the helper components
	*/
	virtual void Init();
}

defaultproperties
{
	// reflection is based on actor rotation
	// default to reflect about z-axis so do 90 degree rot
	Rotation=(Pitch=16384,Roll=0,Yaw=0)

	// uses reflection scene capture component
	// this is needed to actually capture the scene to a texture
	Begin Object Class=SceneCaptureReflectComponent Name=SceneCaptureReflectComponent0
		bSkipUpdateIfTextureUsersOccluded=true
	End Object
	SceneCapture=SceneCaptureReflectComponent0
	Components.Add(SceneCaptureReflectComponent0)

	// used to apply the reflection material for visualization
	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
		HiddenGame=true
		CastShadow=false
		bAcceptsLights=false
		CollideActors=false
		Scale3D=(X=4.0,Y=4.0,Z=4.0)
		StaticMesh=StaticMesh'EditorMeshes.TexPropPlane'
	End Object
	StaticMesh=StaticMeshComponent0
	Components.Add(StaticMeshComponent0)

}
