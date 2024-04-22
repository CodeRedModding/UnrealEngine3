/**
 * Event which is activated when an actor is destroyed.
 * Originator: the Actor that was destroyed.
 * Instigator: the Actor that was destroyed
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_Destroyed extends SequenceEvent
	native(Sequence);

defaultproperties
{
	ObjName="Destroyed"
	ObjCategory="Actor"
}
