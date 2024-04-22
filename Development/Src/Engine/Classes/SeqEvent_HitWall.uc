/**
 * Activated when an actor hits a wall
 * Originator: the actor that owns this event
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_HitWall extends SequenceEvent;

defaultproperties
{
	ObjName="Hit Wall"
	ObjCategory="Physics"

	bAutoActivateOutputLinks=true
	bPlayerOnly=false

	// set a default retrigger delay since touches are fairly frequent
	ReTriggerDelay=0.1f
}
