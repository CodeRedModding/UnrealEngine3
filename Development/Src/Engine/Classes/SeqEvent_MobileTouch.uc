/**
 * SeqEvent_MobileTouch
 *
 * This event gets called when an actor in the world gets "touched" by the player through a touch-screen device.
 * The actor must have bEnableMobileTouch set for this to work.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_MobileTouch extends SequenceEvent;

defaultproperties
{
	ObjName="MobileInput Touch Actor"
	ObjCategory="Input"

	MaxTriggerCount=0

	OutputLinks(0)=(LinkDesc="Tap")

	ReTriggerDelay=0.2f
}
