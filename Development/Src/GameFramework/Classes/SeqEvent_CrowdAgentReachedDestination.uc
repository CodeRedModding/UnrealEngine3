/**
 * Originator: the pawn that owns this event
 * Instigator: the pawn that was killed
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_CrowdAgentReachedDestination extends SequenceEvent
	native;


cpptext
{
	virtual UBOOL CheckActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest=FALSE, TArray<INT>* ActivateIndices = NULL, UBOOL bPushTop = FALSE);
}

defaultproperties
{
	ObjName="Agent Reached"
	ObjCategory="Crowd"
	MaxTriggerCount=0
	ReTriggerDelay=0.f
	bPlayerOnly=false

	OutputLinks(0)=(LinkDesc="Agent Reached Destination")
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Agent",bWriteable=true)
}