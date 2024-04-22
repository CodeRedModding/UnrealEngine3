/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackInstEvent extends InterpTrackInst
	native(Interpolation);


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
