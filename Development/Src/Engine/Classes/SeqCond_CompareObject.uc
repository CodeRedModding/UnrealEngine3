/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqCond_CompareObject extends SequenceCondition
	native(Sequence);

cpptext
{
	void Activated();
}

defaultproperties
{
	ObjName="Compare Objects"
	ObjCategory="Comparison"

	InputLinks(0)=(LinkDesc="In")
	OutputLinks(0)=(LinkDesc="A == B")
	OutputLinks(1)=(LinkDesc="A != B")

	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="A")
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="B")
}
