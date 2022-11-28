/*
	CameraActor
*/

class CameraActor extends Actor
	native
	placeable;

var()			bool			bConstrainAspectRatio;

var()			float			AspectRatio;

var()	interp	float			FOVAngle;

var		DrawFrustumComponent	DrawFrustum;
var		StaticMeshComponent		MeshComp;

function GetCameraView(float fDeltaTime, out vector POVLoc, out rotator POVRot, out float newFOVAngle )
{
	GetActorEyesViewPoint( POVLoc, POVRot );
	newFOVAngle = FOVAngle;
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

cpptext
{
	// UObject interface
	virtual void PostEditChange(UProperty* PropertyThatChanged);

	// AActor interface
	virtual void Spawned();

	// ACameraActor interface
	void UpdateDrawFrustum();
}



defaultproperties
{
	Physics=PHYS_Interpolating

	FOVAngle=90.0
	bConstrainAspectRatio=true
	AspectRatio=1.7777777777777

	Begin Object Class=StaticMeshComponent Name=CamMesh0
		HiddenGame=true
		CollideActors=false
		CastShadow=false
	End Object
	MeshComp=CamMesh0

	Begin Object Class=DrawFrustumComponent Name=DrawFrust0
	End Object
	DrawFrustum=DrawFrust0

	Components.Remove(Sprite)
	Components.Add(CamMesh0)
	Components.Add(DrawFrust0)
}