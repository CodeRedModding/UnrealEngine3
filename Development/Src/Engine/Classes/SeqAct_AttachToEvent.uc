/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_AttachToEvent extends SequenceAction
	native(Sequence);

/** prefer to attach events to Controllers instead of Pawns (for events you want to persist beyond the target dying and respawning) */
var() bool bPreferController;

cpptext
{
	void Activated();
};

defaultproperties
{
	ObjName="Attach To Event"
	ObjCategory="Event"

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Attachee")
	EventLinks(0)=(LinkDesc="Event")
}

