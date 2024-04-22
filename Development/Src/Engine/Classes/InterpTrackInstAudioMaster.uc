/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackInstAudioMaster extends InterpTrackInst
	native(Interpolation);


cpptext
{
	// InterpTrackInst interface
	virtual void InitTrackInst(UInterpTrack* Track);
	virtual void TermTrackInst(UInterpTrack* Track);
}
