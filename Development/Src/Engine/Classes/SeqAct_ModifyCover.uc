/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_ModifyCover extends SequenceAction
	native(Sequence)
	dependson(CoverLink);

/** List of slots to modify */
var() array<int> Slots;

/** New cover type to set for "Manual Adjust" */
var() ECoverType ManualCoverType;

/** New bPlayersOnly setting */
var() bool		bManualAdjustPlayersOnly;


cpptext
{
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
}

defaultproperties
{
	ObjName="Modify Cover"
	ObjCategory="Cover"

	InputLinks(0)=(LinkDesc="Enable Slots")
	InputLinks(1)=(LinkDesc="Disable Slots")
	InputLinks(2)=(LinkDesc="Auto Adjust")
	InputLinks(3)=(LinkDesc="Manual Adjust")
}
