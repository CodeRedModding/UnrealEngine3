/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_SetVelocity extends SequenceAction;

var() Vector VelocityDir;
var() float  VelocityMag;
/** If TRUE given velocity is relative to actor rotation. Otherwise, velocity is in world space. */
var() bool	bVelocityRelativeToActorRotation;

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
	return Super.GetObjClassVersion() + 0;
}


defaultproperties
{
	ObjName="Set Velocity"
	ObjCategory="Actor"
	VariableLinks(1)=(ExpectedType=class'SeqVar_Vector',LinkDesc="Velocity Dir",PropertyName=VelocityDir)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Float',LinkDesc="Velocity Mag",PropertyName=VelocityMag)
}
