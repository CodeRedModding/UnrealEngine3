/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Event mapped to button/key input.
 */
class SeqEvent_Input extends SequenceEvent
	native(Sequence);

cpptext
{
	UBOOL RegisterEvent();

	/**
	 * @return Does this event care about the given input name?
	 */
	UBOOL HasMatchingInput(FName InputName);

	/**
	 * Trigger the event as needed. If this returns TRUE, and bTrapInput is TRUE, then the caller should
	 * stop processing the input.
	 */
	UBOOL CheckInputActivate(INT PlayerIndex, FName InputName, EInputEvent Action);
}

/** Should the input be eaten by the event, or allowed to propagate to gameplay? */
var() bool bTrapInput;

/** -1 for any player, or an index of a specific player to restrict to */
var() int AllowedPlayerIndex;

/** The binding to listen to - this can be something like SpaceBar, XboxTypeS_A, Fire, or GBA_Jump */
var() array<name> InputNames;

defaultproperties
{
	ObjName="Key/Button Pressed"
	ObjCategory="Input"
	bTrapInput=TRUE

	VariableLinks(1)=(LinkDesc="Input Name",ExpectedType=class'SeqVar_String')

	OutputLinks(0)=(LinkDesc="Pressed")
	OutputLinks(1)=(LinkDesc="Repeated")
	OutputLinks(2)=(LinkDesc="Released")

	AllowedPlayerIndex = -1;
	MaxTriggerCount=0
	ReTriggerDelay=0.01
}
