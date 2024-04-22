/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_FeatureTest extends SeqAct_Log
	native(Sequence);

/** Location and Orientation values for the FreezeAt command. */
var() string FreezeAtParameters;

/** The delay (in seconds) between freezing and taking the screenshot. */
var() float ScreenShotDelay;

/** (Optional) File name for the screenshot. */
var() string ScreenShotName;

/** The time remaining between freezing and taking the screenshot. */
var float RemainingScreenShotDelay;

cpptext
{
	virtual void PostLoad();
	void Activated();
	UBOOL UpdateOp(FLOAT deltaTime);
};

defaultproperties
{
	ObjName="Feature Test"
	ObjCategory="Misc"
	bLatentExecution=TRUE
	bAutoActivateOutputLinks=FALSE
	FreezeAtParameters=""
	ScreenShotDelay=1
	ScreenShotName=""
}
