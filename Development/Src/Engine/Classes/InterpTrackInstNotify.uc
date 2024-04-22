/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class InterpTrackInstNotify extends InterpTrackInst
	native(Interpolation);
 
cpptext
{
	virtual void InitTrackInst(UInterpTrack* Track);
}

var	float			LastUpdatePosition;
