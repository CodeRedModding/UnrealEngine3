/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SeqAct_CameraFade extends SequenceAction
	native(Sequence);

/** Color to use as the fade */
var() color FadeColor;
/** Range of alpha to fade, FadeAlpha.X + ((1.f - FadeTimeRemaining/FadeTime) * (FadeAlpha.Y - FadeAlpha.X)) */
var deprecated vector2d FadeAlpha;
/** The opacity that the camera will fade to */
var() float FadeOpacity<ClampMin=0.0 | ClampMax=1.0>;
/** How long to fade to FadeOpacity from the camera's current fade opacity */
var() float FadeTime<ClampMin=0.0>;
/** Should the fade persist? */
var() bool bPersistFade;

/** Should audio be faded as well */
var() bool bFadeAudio;

/** Time left before reaching full alpha */
var float FadeTimeRemaining;
/** List of PCs this action is applied to */
var transient array<PlayerController> CachedPCs;

cpptext
{
	void Activated();
	UBOOL UpdateOp(FLOAT DeltaTime);
	virtual void UpdateObject();
};

static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	ObjName="Fade"
	ObjCategory="Camera"

	bLatentExecution=TRUE
	bAutoActivateOutputLinks=FALSE

	FadeAlpha=(X=0.f,Y=1.f)
	FadeOpacity=1.f
	FadeTime=1.f
	bPersistFade=TRUE

	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Target",PropertyName=Targets,bHidden=TRUE)

	OUtputLinks(0)=(LinkDesc="Out")
	OUtputLinks(1)=(LinkDesc="Finished")
}
