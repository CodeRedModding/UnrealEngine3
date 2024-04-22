/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackInstFloatParticleParam extends InterpTrackInst
	native(Interpolation);

cpptext
{
	virtual void SaveActorState(UInterpTrack* Track);
	virtual void RestoreActorState(UInterpTrack* Track);
}

/** Saved value for restoring state when exiting Matinee. */
var	float		ResetFloat;
