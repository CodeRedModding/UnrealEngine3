class InterpTrackInstAnimControl extends InterpTrackInst
	native(Interpolation);
	
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
cpptext
{
	/** Initialise this Track instance. Called in-game before doing any interpolation. */
	virtual void InitTrackInst(UInterpTrack* Track);

	/** Called when interpolation is done. Should not do anything else with this TrackInst after this. */
	virtual void TermTrackInst(UInterpTrack* Track);
}

var	transient float			LastUpdatePosition;

var editoronly transient vector        InitPosition; 
var editoronly transient rotator       InitRotation; 

