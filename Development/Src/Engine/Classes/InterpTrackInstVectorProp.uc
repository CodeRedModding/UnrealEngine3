/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackInstVectorProp extends InterpTrackInstProperty
	native(Interpolation);


cpptext
{
	virtual void SaveActorState(UInterpTrack* Track);
	virtual void RestoreActorState(UInterpTrack* Track);

	virtual void InitTrackInst(UInterpTrack* Track);
}

/** Pointer to vector property in TrackObject. */
var	pointer		VectorProp; 

/** Saved value for restoring state when exiting Matinee. */
var	vector		ResetVector;
