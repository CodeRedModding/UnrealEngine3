class InterpGroupDirector extends InterpGroup
	native(Interpolation)
	collapsecategories
	hidecategories(Object);

/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Group for controlling properties of a 'player' in the game. This includes switching the player view between different cameras etc.
 */

cpptext
{
	// UInterpGroup interface
	virtual void UpdateGroup(FLOAT NewPosition, class UInterpGroupInst* GrInst, UBOOL bPreview, UBOOL bJump);

	// UInterpGroupDirector interface
	class UInterpTrackDirector* GetDirectorTrack();
	class UInterpTrackFade* GetFadeTrack();
	class UInterpTrackSlomo* GetSlomoTrack();
	class UInterpTrackColorScale* GetColorScaleTrack();
	class UInterpTrackAudioMaster* GetAudioMasterTrack();
}


defaultproperties
{
	GroupName="DirGroup"
}
