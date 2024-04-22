/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqCond_CompareBool extends SequenceCondition
	native(Sequence);

/** Result of comparison is written to this variable */
var bool bResult;

cpptext
{
	void Activated();
};

static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	ObjName="Compare Bool"
	ObjCategory="Comparison"

	InputLinks(0)=(LinkDesc="In")
	OutputLinks(0)=(LinkDesc="True")
	OutputLinks(1)=(LinkDesc="False")

	VariableLinks(0)=(ExpectedType=class'SeqVar_Bool',LinkDesc="Bool")
	VariableLinks(1)=(ExpectedType=class'SeqVar_Bool',LinkDesc="Result",bWriteable=true,PropertyName=bResult)
}
