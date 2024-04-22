/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_MobileRemoveInputZone extends SequenceAction
	native;

cpptext
{
	void Activated();
};

var() string ZoneName;

defaultproperties
{
	ObjName="Remove Input Zone"
	ObjCategory="Mobile"

	InputLinks(0)=(LinkDesc="In")
	OutputLinks(0)=(LinkDesc="Out")
	VariableLinks.Empty
}
