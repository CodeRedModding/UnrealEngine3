/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_PlayMusicTrack extends SequenceAction
	native(Sequence)
	dependson(MusicTrackDataStructures);

var() MusicTrackStruct MusicTrack;

cpptext
{
	virtual void Activated();
	virtual void PreSave();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
}

defaultproperties
{
	ObjName="Play Music Track"
	ObjCategory="Sound"

	VariableLinks.Empty
}