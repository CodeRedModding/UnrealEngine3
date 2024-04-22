/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Event mapped to analog input.
 */
class SeqEvent_AnalogInput extends SequenceEvent
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
	UBOOL CheckInputActivate(INT PlayerIndex, FName InputName, FLOAT Value);

	/**
	 * Trigger the event as needed. If this returns TRUE, and bTrapInput is TRUE, then the caller should
	 * stop processing the input.
	 */
	UBOOL CheckInputActivate(INT PlayerIndex, FName InputName, FVector Value);
}

/** Should the input be eaten by the event, or allowed to propagate to gameplay? */
var() bool bTrapInput;

/** -1 for any player, or an index of a specific player to restrict to */
var() int AllowedPlayerIndex;

/** The binding to listen to - this can be something like MouseX, XboxTypeS_LeftX, Tilt  - NOT aStrafe, aBaseX, etc */
var() array<name> InputNames;

defaultproperties
{
	ObjName="Analog Input"
	ObjCategory="Input"
	bTrapInput=TRUE

	VariableLinks(1)=(LinkDesc="Input Name",ExpectedType=class'SeqVar_String')
	VariableLinks(2)=(LinkDesc="Float Value",ExpectedType=class'SeqVar_Float')
	VariableLinks(3)=(LinkDesc="Vector Value",ExpectedType=class'SeqVar_Vector')

	OutputLinks(0)=(LinkDesc="Output")

	AllowedPlayerIndex = -1;
	MaxTriggerCount=0
	ReTriggerDelay=0.0
}
