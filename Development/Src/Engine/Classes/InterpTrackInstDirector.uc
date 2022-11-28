class InterpTrackInstDirector extends InterpTrackInst
	native(Interpolation);

/** 
 * InterpTrackInstDirector
 *
 * Created by:	James Golding
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

var	Actor	OldViewTarget;	

cpptext
{
	// InterpTrackInst interface
	virtual void TermTrackInst();
}

defaultproperties
{
}