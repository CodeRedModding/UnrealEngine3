/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackInstLinearColorProp extends InterpTrackInstProperty
	native(Interpolation);


cpptext
{
	virtual void SaveActorState(UInterpTrack* Track);
	virtual void RestoreActorState(UInterpTrack* Track);

	virtual void InitTrackInst(UInterpTrack* Track);
}

/** Pointer to color property in TrackObject. */
var	pointer		ColorProp; 

/** Saved value for restoring state when exiting Matinee. */
var	linearcolor		ResetColor;