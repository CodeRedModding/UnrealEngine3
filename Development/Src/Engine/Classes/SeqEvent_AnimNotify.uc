/**
 * Activated when an AnimNotify_Kismet is fired on this actor
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_AnimNotify extends SequenceEvent
	native(Sequence);

var() name NotifyName;

defaultproperties
{
	ObjName="Anim Notify"
	ObjCategory="Actor"

	bAutoActivateOutputLinks=true
	bPlayerOnly=false

	VariableLinks.Empty

	//ReTriggerDelay=0.1f
}
