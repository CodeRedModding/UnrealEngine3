/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


/** event that is triggered when a PickupFactory's status changes */
class SeqEvent_PickupStatusChange extends SequenceEvent;

defaultproperties
{
	ObjName="Pickup Status Change"
	OutputLinks[0]=(LinkDesc="Available")
	OutputLinks[1]=(LinkDesc="Taken")
	bPlayerOnly=false
	MaxTriggerCount=0
}
