/**
 * Event which is activated by the physics system when a joint is broken.
 * Originator: the actor that owns the join which was broken
 * Instigator: the Actor that owns the joint which was broken.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_ConstraintBroken extends SequenceEvent
	native(Sequence);

defaultproperties
{
	ObjName="Constraint Broken"
	ObjCategory="Physics"
	bPlayerOnly=false
}
