/**
 * Event which is activated by gameplay code when a projectile lands.
 * Originator: the Pawn that owns this event.
 * Instigator: a projectile actor which was fired by the Pawn that owns this event
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_ProjectileLanded extends SequenceEvent
	native(Sequence);

cpptext
{
	virtual UBOOL CheckActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest=FALSE, TArray<INT>* ActivateIndices = NULL, UBOOL bPushTop = FALSE);
};

var() float MaxDistance;

defaultproperties
{
	ObjName="Projectile Landed"
	ObjCategory="Physics"
	bPlayerOnly=false

	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Projectile",bWriteable=true)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Shooter",bWriteable=true)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Object',LinkDesc="Witness",bWriteable=true)
}
