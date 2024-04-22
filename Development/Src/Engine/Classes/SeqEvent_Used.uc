/**
 * Activated when a "usable" actor (such as a trigger) is used.
 * Originator: the usable actor that was used
 * Instigator: The pawn associated with the player did the using
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_Used extends SequenceEvent
	native(Sequence);

cpptext
{
	virtual UBOOL CheckActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest=FALSE, TArray<INT>* ActivateIndices = NULL, UBOOL bPushTop = FALSE);
}

/** if true, requires player to aim at trigger to be able to interact with it. False, simple radius check will be performed */
var() bool	bAimToInteract;

/** Max distance from instigator to allow interactions. */
var() float InteractDistance;

/** Text to display when looking at this event */
var() string InteractText;

/** Icon to display when looking at this event */
var() Texture2D InteractIcon;

/** List of class types that are considered valid for this event */
var(TouchTypes) array<class<Actor> >		ClassProximityTypes<AllowAbstract>;

/** List of class types that are considered valid for this event */
var(TouchTypes) array<class<Actor> >		IgnoredClassProximityTypes<AllowAbstract>;

defaultproperties
{
	bAimToInteract=true
	ObjName="Used"
	InteractDistance=128.f
	InteractText="Use"
	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="Distance",bWriteable=true)
	OutputLinks(0)=(LinkDesc="Used")
	OutputLinks(1)=(LinkDesc="Unused")
}
