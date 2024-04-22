/**
 * Event which is activated by the gameplay code when a pawn dies.
 * Originator: either the Pawn that died or the Controller for that Pawn
 * Instigator: the pawn that died.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_Death extends SequenceEvent;

defaultproperties
{
	ObjName="Death"
	ObjCategory="Pawn"
	bPlayerOnly=false
}
