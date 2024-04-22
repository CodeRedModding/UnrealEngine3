/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * To use the mobile input kismet actions with the mouse, set bFakeMobileTouches=true in the [GameFramework.MobilePlayerInput] section of your game.ini
 */
class SeqAct_ToggleMouseCursor extends SequenceAction;

defaultproperties
{
	ObjName="Toggle Mouse Cursor"
	ObjCategory="Input"

	InputLinks(0)=(LinkDesc="Enable")
	InputLinks(1)=(LinkDesc="Disable")

}