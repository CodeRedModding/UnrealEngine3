/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Event mapped to touch input.
 */
class SeqEvent_TouchInput extends SequenceEvent
	native(Sequence);

cpptext
{
	UBOOL RegisterEvent();

	/**
	 * Trigger the event as needed. If this returns TRUE, and bTrapInput is TRUE, then the caller should
	 * stop processing the input.
	 */
	UBOOL CheckInputActivate(INT PlayerIndex, INT TouchIndex, INT TouchpadIndex, EInputEvent Action, const FVector2D& Location);
}

/** Should the input be eaten by the event, or allowed to propagate to gameplay? */
var() bool bTrapInput;

/** -1 for any player, or an index of a specific player to restrict to */
var() int AllowedPlayerIndex;

/** -1 for any player, or an index of a specific player to restrict to */
var() int AllowedTouchIndex;

/** -1 for any player, or an index of a specific player to restrict to */
var() int AllowedTouchpadIndex;

defaultproperties
{
	ObjName="Touch Input"
	ObjCategory="Input"
	bTrapInput=TRUE

	VariableLinks(1)=(LinkDesc="Touch X",ExpectedType=class'SeqVar_Float')
	VariableLinks(2)=(LinkDesc="Touch Y",ExpectedType=class'SeqVar_Float')
	VariableLinks(3)=(LinkDesc="Touch Index",ExpectedType=class'SeqVar_Int')
	VariableLinks(4)=(LinkDesc="Touchpad Index",ExpectedType=class'SeqVar_Int')

	OutputLinks(0)=(LinkDesc="Pressed")
	OutputLinks(1)=(LinkDesc="Repeated")
	OutputLinks(2)=(LinkDesc="Released")

	AllowedPlayerIndex = -1;
	AllowedTouchIndex = 0;
	AllowedTouchpadIndex = 0;
	MaxTriggerCount=0
	ReTriggerDelay=0.0
}
