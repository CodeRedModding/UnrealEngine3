/**
 * SceneCapture2DActor
 *
 * Place this actor in the level to capture it to a render target texture.
 * Uses a 2D scene capture component
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SceneCapture2DActor extends SceneCaptureActor
	native
	placeable;

/** used to draw the frustum lines and the texture surface */
var const DrawFrustumComponent DrawFrustum;


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
	Components.Remove(Sprite)

	// 2D scene capture
	Begin Object Class=SceneCapture2DComponent Name=SceneCapture2DComponent0
	End Object
	SceneCapture=SceneCapture2DComponent0
	Components.Add(SceneCapture2DComponent0)	

	// camera mesh
	Begin Object Class=StaticMeshComponent Name=CamMesh0
	    HiddenGame=true
		CastShadow=false
		CollideActors=false
		AlwaysLoadOnServer=FALSE
		AlwaysLoadOnClient=FALSE
		StaticMesh=StaticMesh'EditorMeshes.MatineeCam_SM'
	End Object
	Components.Add(CamMesh0)

	// camera frustum of capture - updated in SyncComponents
	Begin Object Class=DrawFrustumComponent Name=DrawFrust0
	    FrustumColor=(R=255,G=255,B=255)
		bAcceptsLights=false
	End Object
	DrawFrustum=DrawFrust0
	Components.Add(DrawFrust0)	
}
