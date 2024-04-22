/**
 * SceneCapturePortalActor
 *
 * Place this actor in a level to capture the scene
 * to a texture target using a SceneCapturePortalComponent
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SceneCapturePortalActor extends SceneCaptureReflectActor
	native
	placeable;


cpptext
{
	// SceneCaptureActor interface

	/** 
	* Update any components used by this actor
	*/
	virtual void SyncComponents();
}

defaultproperties
{
	// actor's facing direction is the portal capture direction
	Rotation=(Pitch=0,Roll=0,Yaw=0)

	Components.Remove(SceneCaptureReflectComponent0)
	Components.Remove(Sprite)
	Components.Remove(StaticMeshComponent0)

	// uses portal scene capture component
	// this is needed to actually capture the scene to a texture
	Begin Object Class=SceneCapturePortalComponent Name=SceneCapturePortalComponent0
	End Object
	SceneCapture=SceneCapturePortalComponent0
	Components.Add(SceneCapturePortalComponent0)

	// used to visualize facing direction 
	// note that capture direction is opposite of actor facing direction (-x)
	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent1
		HiddenGame=true
		CastShadow=false
		CollideActors=false
		AlwaysLoadOnServer=FALSE
		AlwaysLoadOnClient=FALSE
		Scale3D=(X=-1,Y=1,Z=1)
		StaticMesh=StaticMesh'EditorMeshes.MatineeCam_SM'
	End Object
	Components.Add(StaticMeshComponent1)

	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent2
		HiddenGame=true
		CastShadow=false
		CollideActors=false
		AlwaysLoadOnServer=FALSE
		AlwaysLoadOnClient=FALSE
		Scale3D=(X=1.0,Y=1.0,Z=1.0)
		StaticMesh=StaticMesh'EditorMeshes.TexPropPlane'
	End Object
	StaticMesh=StaticMeshComponent2
	Components.Add(StaticMeshComponent2)
}
