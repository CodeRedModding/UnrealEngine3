/**
 * Event which is activated by the engine when the level starts at the beginning
 * and can be NOT activated by the gameplay code if, for instance, you are loading
 * a savegame in the middle of the level and don't want beginning of map actions
 * to happen.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_LevelBeginning extends SequenceEvent
	deprecated
	native(Sequence);

cpptext
{
	virtual USequenceObject* ConvertObject();
}

defaultproperties
{
	ObjName="Level Beginning"
	VariableLinks.Empty
	bPlayerOnly=false
}
