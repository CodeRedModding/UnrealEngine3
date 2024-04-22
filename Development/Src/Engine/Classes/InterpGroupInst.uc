class InterpGroupInst extends Object
	native(Interpolation);

/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * 
 * An instance of an InterpGroup for a particular Actor. There may be multiple InterpGroupInsts for a single
 * InterpGroup in the InterpData, if multiple Actors are connected to the same InterpGroup. 
 * The Outer of an InterpGroupInst is the SeqAct_Interp
 */

cpptext
{
	/** 
	 *	Returns the Actor that this GroupInstance is working on. 
	 *	Should use this instead of just referencing GroupActor, as it check bDeleteMe for you.
	 */
	virtual AActor* GetGroupActor();

	/** Called before Interp editing to save original state of Actor. @see UInterpTrackInst::SaveActorState */
	virtual void SaveGroupActorState();

	/** Called after Interp editing to put object back to its original state. @see UInterpTrackInst::RestoreActorState */
	virtual void RestoreGroupActorState();

	/**  
	 * Returns if this group contains this Actor
	 */
	virtual UBOOL HasActor(AActor * InActor)
	{
		return (GetGroupActor() == InActor);
	};

	/** 
	 *	Initialse this Group instance. Called from USeqAct_Interp::InitInterp before doing any interpolation.
	 *	Save the Actor for the group and creates any needed InterpTrackInsts
	 */
	virtual void InitGroupInst(UInterpGroup* InGroup, AActor* InGroupActor);

	/** 
	 *	Called when done with interpolation sequence. Cleans up InterpTrackInsts etc. 
	 *	Do not do anything further with the Interpolation after this.
	 */
	virtual void TermGroupInst(UBOOL bDeleteTrackInst);

	/** Force any actors attached to this group's actor to update their position using their relative location/rotation. */
	void UpdateAttachedActors();

	/** Caches or Restores the PPS at the Start/End of the matinee sequence */
	UBOOL HasPPS( void );
	void CreatePPS( void );
	void CachePPS( const FPostProcessSettings& PPSettings );
	void RestorePPS( FPostProcessSettings& PPSettings );
	void DestroyPPS( void );
	void FreePPS( void );
}

/** InterpGroup within the InterpData that this is an instance of. */
var		InterpGroup				Group; 

/** 
 *	Actor that this Group instance is acting upon.
 *	NB: that this may be set to NULL at any time as a result of the Actor being destroyed.
 */
var		Actor					GroupActor;

/** Array if InterpTrack instances. TrackInst.Num() == InterpGroup.InterpTrack.Num() must be true. */
var		array<InterpTrackInst>	TrackInst;

/** A cached copy of the group actor's pps (if it's a CameraActor) to prevent overwriting defaults */
var		private native transient pointer	CachedCamOverridePostProcess{FPostProcessSettings};		// Just need to cache the bOverride_* flags