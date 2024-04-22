/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackInstSkelControlStrength extends InterpTrackInst
	native(Interpolation);

/** Save ControlledByAnimMetaData **/
var transient bool bSavedControlledByAnimMetaData;

cpptext
{
	virtual void RestoreActorState(UInterpTrack* Track);

	/** Initialise this Track instance. Called in-game before doing any interpolation. */
	virtual void InitTrackInst(UInterpTrack* Track);

	/** Called when interpolation is done. Should not do anything else with this TrackInst after this. */
	virtual void TermTrackInst(UInterpTrack* Track);
}

