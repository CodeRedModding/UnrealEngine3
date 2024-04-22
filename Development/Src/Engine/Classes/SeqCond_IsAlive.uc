/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class SeqCond_IsAlive extends SequenceCondition
	native(Sequence);

cpptext
{
	virtual void Activated();
}

defaultproperties
{
	ObjName="Is Alive"

	OutputLinks(0)=(LinkDesc="True")
	OutputLinks(1)=(LinkDesc="False")
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Players")
}