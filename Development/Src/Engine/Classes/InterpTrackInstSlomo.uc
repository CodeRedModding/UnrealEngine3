/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackInstSlomo extends InterpTrackInst
	native(Interpolation);


/** Backup of initial LevelInfo TimeDilation setting when interpolation started. */
var	float	OldTimeDilation;

cpptext
{
	// InterpTrackInst interface
	virtual void SaveActorState(UInterpTrack* Track);
	virtual void RestoreActorState(UInterpTrack* Track);
	virtual void InitTrackInst(UInterpTrack* Track);
	virtual void TermTrackInst(UInterpTrack* Track);

	/** @return whether the slomo track's effects should actually be applied. We want to only do this once for the server
	 * and not at all for the clients regardless of the number of instances created for the various players
	 * to avoid collisions and replication issues
	 */
	UBOOL ShouldBeApplied();
}
