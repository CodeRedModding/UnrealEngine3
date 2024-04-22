/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_ToggleInput extends SeqAct_Toggle;

var() bool bToggleMovement;
var() bool bToggleTurning;


defaultproperties
{
	ObjName="Toggle Input"
	ObjCategory="Toggle"
	VariableLinks.RemoveIndex(1)
	bToggleMovement=true
	bToggleTurning=true
}
