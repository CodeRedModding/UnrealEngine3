/**
 * Simple action that records the amount of time elapsed
 * between activating the first link "Start" and the second
 * link "Stop".
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_Timer extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated()
	{
		// reset the activation time
		Time = 0.f;
		ActivationTime = GWorld != NULL ? GWorld->GetTimeSeconds() : 0.f;
	}

	UBOOL UpdateOp(FLOAT DeltaTime)
	{
		// check for stop
		if (InputLinks(1).bHasImpulse)
		{
			// record the exact duration
			Time = GWorld != NULL ? GWorld->GetTimeSeconds() - ActivationTime : 0.f;
			// finish the op
			return TRUE;
		}
		else
		{
			// update the current time
			Time += DeltaTime;
			// and force any attached variables to get the new value
			PopulateLinkedVariableValues();
		}
		return FALSE;
	}
};

/** World time at point of activation */
var transient float ActivationTime;

/** Amount of time this timer has been active */
var() float Time;


defaultproperties
{
	ObjName="Timer"
	ObjCategory="Misc"

	bLatentExecution=TRUE

	InputLinks(0)=(LinkDesc="Start")
	InputLinks(1)=(LinkDesc="Stop")

	VariableLinks.Empty
	VariableLinks(0)=(LinkDesc="Time",ExpectedType=class'SeqVar_Float',PropertyName=Time)
}
