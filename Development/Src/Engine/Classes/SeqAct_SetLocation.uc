/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_SetLocation extends SeqAct_SetSequenceVariable
	native(Sequence);

cpptext
{
	void Activated();
};

/** Default value to use if no variables are linked */
var() bool bSetLocation;
var() vector LocationValue;
/** Default value to use if no variables are linked */
var() Rotator RotationValue;
var() bool bSetRotation;


/** Object that will be moved */ 
var Object Target;


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
	return Super.GetObjClassVersion() + 2;
}

defaultproperties
{
	ObjName="Set Actor Location"
	ObjCategory="Actor"

	bSetLocation=TRUE
	bSetRotation=TRUE

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Vector',LinkDesc="Location",MaxVars=1)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Vector',LinkDesc="Rotation",MaxVars=1)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Object',LinkDesc="Target",bWriteable=true,PropertyName=Target,MaxVars=1)
}
