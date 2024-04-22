/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_GetDistance extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
}

var() editconst float Distance;

defaultproperties
{
	ObjName="Get Distance"
	ObjCategory="Actor"

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="A")
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="B")
	VariableLinks(2)=(ExpectedType=class'SeqVar_Float',LinkDesc="Distance",bWriteable=true,PropertyName=Distance)
}
