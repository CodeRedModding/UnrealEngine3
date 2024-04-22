/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackInstFloatProp extends InterpTrackInstProperty
	native(Interpolation);

cpptext
{
	virtual void SaveActorState(UInterpTrack* Track);
	virtual void RestoreActorState(UInterpTrack* Track);

	virtual void InitTrackInst(UInterpTrack* Track);
	virtual void TermTrackInst(UInterpTrack* Track);
}

/** Pointer to float property in TrackObject. */
var	pointer		FloatProp; 

/** Saved value for restoring state when exiting Matinee. */
var	float		ResetFloat;

/** Pointer to FMatineeRawFloatDistribution if the FloatProp pointer belongs to one, NULL otherwise. */
var	pointer		DistributionProp;
