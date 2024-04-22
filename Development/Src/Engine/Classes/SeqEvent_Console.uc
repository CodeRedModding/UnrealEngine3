/**
 * Event which is activated when the user a console command.
 * Originator: the PlayerController for the player that typed the console command
 * Instigator: the pawn of the player that typed the console command.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_Console extends SequenceEvent
	native(Sequence);

/** Name to match when entered from the console */
var() Name ConsoleEventName<autocomment=true>;

/** Description to display when listing summary of all console events */
var() string EventDesc;

cpptext
{
#if WITH_EDITOR
protected:
	FString GetDisplayTitle() const;
#endif
};

defaultproperties
{
	ObjName="Console Event"
	ConsoleEventName=Default

	EventDesc="No description"

	MaxTriggerCount=0
}
