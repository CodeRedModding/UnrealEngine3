/**
 * Event which is activated by GameInfo.StartMatch when the match begins.
 * Originator: current WorldInfo
 * Instigator: None
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_LevelStartup extends SequenceEvent
	deprecated
	native(Sequence);

cpptext
{
	virtual USequenceObject* ConvertObject();
}

defaultproperties
{
	ObjName="Level Startup"
	VariableLinks.Empty
	bPlayerOnly=false
}
