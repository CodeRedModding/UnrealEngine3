/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackInstSound extends InterpTrackInst
	native(Interpolation);

cpptext
{
	virtual void InitTrackInst(UInterpTrack* Track);
	virtual void TermTrackInst(UInterpTrack* Track);
}

var	float						LastUpdatePosition; 
var	transient AudioComponent	PlayAudioComp;

defaultproperties
{
}
