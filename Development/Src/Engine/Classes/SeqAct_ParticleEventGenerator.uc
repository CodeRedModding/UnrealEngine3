/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_ParticleEventGenerator extends SequenceAction
	native(Sequence);

/** Is this event generator enabled? */
var() bool bEnabled;

/** Player that trigger the event (Controller or Pawn) */
var Actor Instigator;
/** Name of the event to generate. */
var array<string> EventNames;
/** The 'time' the event occured. */
var float EventTime;
/** The location of the event. */
var vector EventLocation;
/** The direction of the event. */
var vector EventDirection;
/** The velocity of the event. */
var vector EventVelocity;
/** The (hit) normal of the event. */
var vector EventNormal;

/** If TRUE, use the Emitter target position as the Location. */
var() bool bUseEmitterLocation;

cpptext
{
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	void Activated();
	virtual UBOOL UpdateOp(FLOAT DeltaTime);
	void DeActivated();

	/**
	 * Checks any of the bEnabled inputs and sets the new value.
	 */
	void CheckToggle()
	{
		if (InputLinks(1).bHasImpulse)
		{
			bEnabled = TRUE;
		}
		else
		if (InputLinks(2).bHasImpulse)
		{
			bEnabled = FALSE;
		}
		else
		if (InputLinks(3).bHasImpulse)
		{
			bEnabled = !bEnabled;
		}
	}
};

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
	return Super.GetObjClassVersion() + 0;
}

defaultproperties
{
	ObjName="ParticleEvent Generator"
	ObjCategory="Particles"

	bEnabled=true

	InputLinks(0)=(LinkDesc="Trigger Event")
	InputLinks(1)=(LinkDesc="Enable")
	InputLinks(2)=(LinkDesc="Disable")
	InputLinks(3)=(LinkDesc="Toggle")

	VariableLinks(1)=(ExpectedType=class'SeqVar_String',LinkDesc="EventNames",PropertyName=EventNames)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Float',LinkDesc="EventTime",PropertyName=EventTime,bWriteable=FALSE,MaxVars=1)
	VariableLinks(3)=(ExpectedType=class'SeqVar_Vector',LinkDesc="Location",PropertyName=EventLocation,bWriteable=FALSE,MaxVars=1)
	VariableLinks(4)=(ExpectedType=class'SeqVar_Vector',LinkDesc="Direction",PropertyName=EventDirection,bWriteable=FALSE,MaxVars=1)
	VariableLinks(5)=(ExpectedType=class'SeqVar_Vector',LinkDesc="Velocity",PropertyName=EventVelocity,bWriteable=FALSE,MaxVars=1)
	VariableLinks(6)=(ExpectedType=class'SeqVar_Vector',LinkDesc="Normal",PropertyName=EventNormal,bWriteable=FALSE,MaxVars=1)
}
