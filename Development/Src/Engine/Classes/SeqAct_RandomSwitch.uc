/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_RandomSwitch extends SeqAct_Switch
	native(Sequence);

/** List of indices we've already used once and disabled (for when bLooping and bAutoDisableLinks are both checked) **/
var array<INT> AutoDisabledIndices; 

cpptext
{
	virtual void Activated();
};

static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	ObjName="Random"
	ObjCategory="Switch"
	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Int',LinkDesc="Active Link",bWriteable=true,MinVars=0,PropertyName=Indices)

	InputLinks(1)=(LinkDesc="Reset")
}
