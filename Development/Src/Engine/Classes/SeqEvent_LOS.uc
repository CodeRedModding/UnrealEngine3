/**
 * Event which is activated when some pawn has a line of sight to an LOS trigger.
 * Originator: the trigger that owns this event
 * Instigator: the Pawn of the Player that saw the LOS trigger which owns this event
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_LOS extends SequenceEvent;

/** Distance from the screen center before activating this event */
var() float ScreenCenterDistance;

/** Distance from the trigger before activating this event */
var() float TriggerDistance;

/** Force a clear line of sight to the trigger? */
var() bool bCheckForObstructions;

static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	ObjName="Line Of Sight"
	ObjCategory="Pawn"

	OutputLinks(0)=(LinkDesc="Look")
	OutputLinks(1)=(LinkDesc="Stop Look")

	ScreenCenterDistance=50.f
	TriggerDistance=2048.f
	bCheckForObstructions=true
}
