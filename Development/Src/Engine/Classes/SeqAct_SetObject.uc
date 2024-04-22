/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_SetObject extends SeqAct_SetSequenceVariable
	native(Sequence);

cpptext
{
	virtual void Activated();
};

/** Default value to use if no variables are linked */
var() Object DefaultValue;

var Object Value;

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
	ObjName="Object"
	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Value",PropertyName=Value)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Target",PropertyName=Targets)
}
