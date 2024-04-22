/** 
 * Event triggered by AI when an actor along a route is reached.
 * Firing the event at the proper time should be handled by game code.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SeqEvent_AIReachedRouteActor extends SequenceEvent;

defaultproperties
{
	ObjCategory="AI"
	ObjName="Reached Route Actor"
	bPlayerOnly=false
}
