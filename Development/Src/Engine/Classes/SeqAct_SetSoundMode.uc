/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SeqAct_SetSoundMode extends SequenceAction;

/** SoundMode to use, or None for default. */
var() SoundMode SoundMode;

/** Whether this soundmode is the highest priority, game specific and ignored by PlayerController */
var() bool bTopPriority;

/** Call handler manually so we can assume PC without requiring one to be attached **/
event Activated()
{
	local PlayerController PC;

	// find any PC
	PC = GetWorldInfo().GetALocalPlayerController();
	if (PC != None)
	{
		PC.OnSetSoundMode(self);
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
	return Super.GetObjClassVersion() + 3;
}



defaultproperties
{
	ObjName="Set Sound Mode"
	ObjCategory="Sound"

	bCallHandler=false

	InputLinks(0)=(LinkDesc="Start")
	InputLinks(1)=(LinkDesc="Stop")

	VariableLinks.Empty
}
