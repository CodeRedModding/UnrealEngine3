/*
	CameraActor
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class CameraActor extends Actor
	native(Camera)
	ClassGroup(Common)
	placeable;

var()			bool			bConstrainAspectRatio;

var()	interp	float			AspectRatio;

var()	interp	float			FOVAngle;

var deprecated	bool					bCamOverridePostProcess;
/** Blend value for CamOverridePostProcess.  0.f means it's ignored, 1.f means use it exclusively.  */
var()	interp	float					CamOverridePostProcessAlpha<ClampMin=0.0 | ClampMax=1.0>;
var()	interp	PostProcessSettings		CamOverridePostProcess;

var		DrawFrustumComponent	DrawFrustum;
var		StaticMeshComponent		MeshComp;

cpptext
{
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostLoad();

	// AActor interface
	virtual void Spawned();
protected:
	virtual void UpdateComponentsInternal(UBOOL bCollisionUpdate = FALSE);
public:

	// ACameraActor interface
	void UpdateDrawFrustum();

#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
}

replication
{
	if (Role == ROLE_Authority)
		FOVAngle, AspectRatio;
}


/**
 * Returns camera's Point of View.
 * Called by Camera.uc class. Subclass and postprocess to add any effects.
 */
simulated function GetCameraView(float DeltaTime, out TPOV OutPOV)
{
	GetActorEyesViewPoint(OutPOV.Location, OutPOV.Rotation);
	OutPOV.FOV = FOVAngle;
}


/** 
 * list important CameraActor variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local float XL;
	local Canvas Canvas;

	Canvas = HUD.Canvas;

	super.DisplayDebug( HUD, out_YL, out_YPos);

	Canvas.StrLen("TEST", XL, out_YL);
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);
	Canvas.DrawText("FOV:" $ FOVAngle, false);
}

defaultproperties
{
	Physics=PHYS_Interpolating

	FOVAngle=95.0
	bConstrainAspectRatio=TRUE
	AspectRatio=AspectRatio16x9

	Begin Object Class=StaticMeshComponent Name=CamMesh0
		HiddenGame=TRUE
		CollideActors=FALSE
		BlockRigidBody=FALSE
		CastShadow=FALSE
		AlwaysLoadOnClient=FALSE
		AlwaysLoadOnServer=FALSE
	End Object
	MeshComp=CamMesh0
	Components.Add(CamMesh0)

	Begin Object Class=DrawFrustumComponent Name=DrawFrust0
		AlwaysLoadOnClient=FALSE
		AlwaysLoadOnServer=FALSE
	End Object
	DrawFrustum=DrawFrust0
	Components.Add(DrawFrust0)

	
	RemoteRole=ROLE_None
	NetUpdateFrequency=1.f
	bNoDelete=TRUE

	bEdShouldSnap=TRUE
}
