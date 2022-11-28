class InterpGroupInst extends Object
	native(Interpolation);

/** 
 * InterpGroupInst
 *
 * Created by:	James Golding
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
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
	AActor* GetGroupActor();

	/** Called before Interp editing to save original state of Actor. @see UInterpTrackInst::SaveActorState */
	virtual void SaveGroupActorState();

	/** Called after Interp editing to put object back to its original state. @see UInterpTrackInst::RestoreActorState */
	virtual void RestoreGroupActorState();

	/** 
	 *	Initialse this Group instance. Called from USeqAct_Interp::InitInterp before doing any interpolation.
	 *	Save the Actor for the group and creates any needed InterpTrackInsts
	 */
	virtual void InitGroupInst(UInterpGroup* InGroup, AActor* InGroupActor);

	/** 
	 *	Called when done with interpolation sequence. Cleans up InterpTrackInsts etc. 
	 *	Do not do anything further with the Interpolation after this.
	 */
	virtual void TermGroupInst();
}

/** InterpGroup within the InterpData that this is an instance of. */
var		InterpGroup				Group; 

/** 
 *	Actor that this Group instance is acting upon.
 *	NB: that this may be set to NULL at any time as a result of the Actor being destroyed (reference-NULLing happens in ULevel::CleanupDestroyed).
 */
var		Actor					GroupActor;

/** Array if InterpTrack instances. TrackInst.Num() == InterpGroup.InterpTrack.Num() must be true. */
var		array<InterpTrackInst>	TrackInst; 


