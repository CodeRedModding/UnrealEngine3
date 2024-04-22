/**
 * Activated when an actor touches another actor.  Will be called on both actors, first on the actor that
 * was originally touched, then on the actor that did the touching
 *
 * Originator: the actor that owns this event
 * Instigator: the actor that was touched
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_Touch extends SequenceEvent
	native(Sequence);

cpptext
{
	virtual UBOOL CheckTouchActivate(AActor *inOriginator, AActor *inInstigator, UBOOL bTest = FALSE);
	virtual UBOOL CheckUnTouchActivate(AActor *inOriginator, AActor *inInstigator, UBOOL bTest = FALSE);

protected:
	// hide the default implementation to force use of CheckTouchActivate/CheckUnTouchActivate
	virtual UBOOL CheckActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest=FALSE, TArray<INT>* ActivateIndices = NULL, UBOOL bPushTop = FALSE);

	virtual void DoTouchActivation(AActor *InOriginator, AActor *InInstigator);
	virtual void DoUnTouchActivation(AActor *InOriginator, AActor *InInstigator, INT TouchIdx);
};

//==========================
// Base variables

/** List of class types that are considered valid for this event */
var(TouchTypes) array<class<Actor> >		ClassProximityTypes<AllowAbstract>;

/** List of class types that are considered valid for this event */
var(TouchTypes) array<class<Actor> >		IgnoredClassProximityTypes<AllowAbstract>;

/** Force the player to be overlapping at the time of activation? */
var() bool bForceOverlapping;

/**
 * Use Instigator, not actual Actor.
 * For projectiles, it returns the instigator.
 */
var() bool bUseInstigator;

/** whether dead (Health < 0) pawns can be considered touching */
var() bool bAllowDeadPawns;

/** List of all actors that have activated this touch event, so that untouch may be properly fired. */
var array<Actor> TouchedList;

native noexport final function bool CheckTouchActivate(Actor InOriginator, Actor InInstigator, optional bool bTest);
native noexport final function bool CheckUnTouchActivate(Actor InOriginator, Actor InInstigator, optional bool bTest);

event Toggled()
{
	local int Idx;
	// if now enabled
	if (bEnabled)
	{
		// check activation for everything currently touching the originator
		if (Originator != None)
		{
			for (Idx = 0; Idx < Originator.Touching.Length; Idx++)
			{
				CheckTouchActivate(Originator,Originator.Touching[Idx]);
			}
		}
	}
	else
	{
		// otherwise clear the touched list
		TouchedList.Length = 0;
	}
}

/** notification that the given Pawn has died while touching an Actor with this event connected to it
 * @param P - the pawn that died
 */
function NotifyTouchingPawnDied(Pawn P)
{
	if (!bAllowDeadPawns)
	{
		CheckUnTouchActivate(Originator, P);
	}
}

/**
 * Return the version number for this class.  Child classes should increment this method by calling Super then adding
 * a individual class version to the result.  When a class is first created, the number should be 0; each time one of the
 * link arrays is modified (VariableLinks, OutputLinks, InputLinks, etc.), the number that is added to the result of
 * Super.GetObjClassVersion() should be incremented by 1.
 *
 * @return	the version number for this specific class.
 */
static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	ObjName="Touch"
	ObjCategory="Physics"
	ClassProximityTypes(0)=class'Pawn'

	OutputLinks(0)=(LinkDesc="Touched")
	OutputLinks(1)=(LinkDesc="UnTouched")
	OutputLinks(2)=(LinkDesc="Empty")

	// default to overlap check, as this is generally the expected behavior
	bForceOverlapping=TRUE

	// set a default retrigger delay since touches are fairly frequent
	ReTriggerDelay=0.1f
}
