/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_MobileClearInputZones extends SequenceAction
	native;

cpptext
{
	void Activated();
};

defaultproperties
{
	ObjName="Clear Input Zones"
	ObjCategory="Mobile"

	InputLinks(0)=(LinkDesc="In")
	OutputLinks(0)=(LinkDesc="Out")
	VariableLinks.Empty
}
