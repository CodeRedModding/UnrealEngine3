class InterpTrackInstSlomo extends InterpTrackInst
	native(Interpolation);

/** 
 * InterpTrackInstSlomo
 *
 * Created by:	James Golding
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

/** Backup of initial LevelInfo TimeDilation setting when interpolation started. */
var	float	OldTimeDilation;

cpptext
{
	// InterpTrackInst interface
	virtual void SaveActorState();
	virtual void RestoreActorState();
	virtual void InitTrackInst(UInterpTrack* Track);
	virtual void TermTrackInst();
}