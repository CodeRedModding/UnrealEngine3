class InterpTrackInstEvent extends InterpTrackInst
	native(Interpolation);

/** 
 * InterpTrackInstEvent
 *
 * Created by:	James Golding
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

cpptext
{
	/** 
	 * This will initialise LastUpdatePosition to whatever position the SeqAct_Interp is in, 
	 * so we don't play a bunch of events straight away! 
	 */
	virtual void InitTrackInst(UInterpTrack* Track);
}

/** 
 *	Position we were in last time we evaluated Events. 
 *	During UpdateTrack, events between this time and the current time will be fired. 
 */
var	float LastUpdatePosition; 

defaultproperties
{
}