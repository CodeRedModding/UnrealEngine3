class InterpGroupDirector extends InterpGroup
	native(Interpolation)
	collapsecategories
	hidecategories(Object);

/** 
 * InterpGroupDirector
 *
 * Group for controlling properties of a 'player' in the game. This includes switching the player view between different cameras etc.
 */

cpptext
{
	// UInterpGroup interface
	virtual void UpdateGroup(FLOAT NewPosition, class UInterpGroupInst* GrInst, UBOOL bPreview=false);

	// UInterpGroupDirector interface
	class UInterpTrackDirector* GetDirectorTrack();
	class UInterpTrackFade* GetFadeTrack();
	class UInterpTrackSlomo* GetSlomoTrack();
}


defaultproperties
{
	GroupName="DirGroup"
}