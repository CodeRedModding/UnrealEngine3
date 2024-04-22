/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_MobileAddInputZones extends SequenceAction
	native;

cpptext
{
	void Activated();
};

/** Name for this zone, it will be used in Kismet zone input events */
var() name ZoneName;

/** All the details needed to set up a zone */
var() editinline MobileInputZone NewZone;

defaultproperties
{
	ObjName="Add Input Zone"
	ObjCategory="Mobile"

	InputLinks(0)=(LinkDesc="In")
	OutputLinks(0)=(LinkDesc="Out")
	VariableLinks.Empty
}
