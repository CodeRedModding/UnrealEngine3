/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackInstToggle extends InterpTrackInst
	native(Interpolation);

var()	ETrackToggleAction	Action;
/** 
 *	Position we were in last time we evaluated.
 *	During UpdateTrack, toggles between this time and the current time will be processed.
 */
var		float				LastUpdatePosition; 

/** Cached 'active' state for the toggleable actor before we possessed it; restored when Matinee exits */
var bool bSavedActiveState;


cpptext
{
	/** 
	 */
	virtual void InitTrackInst(UInterpTrack* Track);

	/** Called before Interp editing to put object back to its original state. */
	virtual void SaveActorState(UInterpTrack* Track);

	/** Restore the saved state of this Actor. */
	virtual void RestoreActorState(UInterpTrack* Track);
}

defaultproperties
{
}
