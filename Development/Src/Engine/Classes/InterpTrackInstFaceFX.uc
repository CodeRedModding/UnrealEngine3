class InterpTrackInstFaceFX extends InterpTrackInst
	native(Interpolation);
	
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
cpptext
{
	virtual void InitTrackInst(UInterpTrack* Track);
	virtual void TermTrackInst(UInterpTrack* Track);
	virtual void SaveActorState(UInterpTrack* Track);
	virtual void RestoreActorState(UInterpTrack* Track);
}

var	transient bool	bFirstUpdate;
var	float			LastUpdatePosition;
 

