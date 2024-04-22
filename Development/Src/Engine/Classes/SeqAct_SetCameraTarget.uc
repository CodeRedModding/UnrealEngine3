/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_SetCameraTarget extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
};

/** Internal.  Holds a ref to the new desired camera target. */
var transient Actor		CameraTarget;

/** Parameters that define how the camera will transition to the new target. */
var() const Camera.ViewTargetTransitionParams	TransitionParams;

/**
 * Return the version number for this class.  Child classes should increment this method by calling Super then adding
 * a individual class version to the result.  When a class is first created, the number should be 0; each time one of the
 * link arrays is modified (VariableLinks, OutputLinks, InputLinks, etc.), the number that is added to the result of
 * Super.GetObjClassVersion() should be incremented by 1.
 *
 * @return	the version number for this specific class.
 */
static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	ObjName="Set Camera Target"
	ObjCategory="Camera"

	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Cam Target")
}
