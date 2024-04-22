/**
 * SceneCaptureCubeMapActor
 *
 * Place this actor in the level iot capture it to a cube map render target texture.
 * Uses a Cube map scene capture component
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SceneCaptureCubeMapActor extends SceneCaptureActor
	native
	placeable;

/** for visualizing the cube capture */
var const StaticMeshComponent StaticMesh;

/** material instance used to apply the target texture to the static mesh */
var transient MaterialInstanceConstant CubeMaterialInst;

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
	Components.Remove(Sprite)

	// cube map scene capture component 
	Begin Object Class=SceneCaptureCubeMapComponent Name=SceneCaptureCubeMapComponent0
	End Object
	SceneCapture=SceneCaptureCubeMapComponent0
	Components.Add(SceneCaptureCubeMapComponent0)	

	// sphere for better viz
	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0		
	HiddenGame=true
	CastShadow=false
	bAcceptsLights=false
	CollideActors=false
	Scale3D=(X=0.6,Y=0.6,Z=0.6)
	StaticMesh=StaticMesh'EditorMeshes.TexPropSphere'
	End Object
	StaticMesh=StaticMeshComponent0
	Components.Add(StaticMeshComponent0)

}
