class InterpTrackInst extends Object
	native(Interpolation);

/** 
 * InterpTrackInst
 *
 * Created by:	James Golding
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 *
 * 
 * The Outer of an InterpTrackInst is the InterpGroupInst.
 */

cpptext
{
	/** 
	 *	Return the Actor associated with this instance of a Group. 
	 *	Note that all Groups have at least 1 instance, even if no Actor variable is attached, so this may return NULL. 
	 */
	AActor* GetGroupActor();

	/** Called before Interp editing to put object back to its original state. */
	virtual void SaveActorState() {}

	/** Restore the saved state of this Actor. */
	virtual void RestoreActorState() {}

	/** Initialise this Track instance. Called in-game before doing any interpolation. */
	virtual void InitTrackInst(UInterpTrack* Track) {}

	/** Called when interpolation is done. Should not do anything else with this TrackInst after this. */
	virtual void TermTrackInst() {}
}

