/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_Toggle extends SequenceAction
	native(Sequence);

cpptext
{
	virtual void PostLoad();
	virtual void Activated();
};


defaultproperties
{
	ObjName="Toggle"
	ObjCategory="Toggle"

	InputLinks(0)=(LinkDesc="Turn On")
	InputLinks(1)=(LinkDesc="Turn Off")
	InputLinks(2)=(LinkDesc="Toggle")

	VariableLinks(0)=(bModifiesLinkedObject=true)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Bool',LinkDesc="Bool",MinVars=0)
	EventLinks(0)=(LinkDesc="Event")
}
