/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackInstParticleReplay extends InterpTrackInst
	native(Interpolation);

/** 
 *	Position we were in last time we evaluated.
 *	During UpdateTrack, events between this time and the current time will be processed.
 */
var		float				LastUpdatePosition; 

cpptext
{
	/** Initialise this Track instance. Called in-game before doing any interpolation. */
	virtual void InitTrackInst(UInterpTrack* Track);

	/** Restore the saved state of this Actor. */
	virtual void RestoreActorState(UInterpTrack* Track);
}

defaultproperties
{
}
