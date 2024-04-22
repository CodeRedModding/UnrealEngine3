/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpGroupInstAI extends InterpGroupInst
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

	virtual UBOOL HasActor(AActor * InActor);

	/** 
	 *	Initialze this Group instance. Called from USeqAct_Interp::InitInterp before doing any interpolation.
	 *	Save the Actor for the group and creates any needed InterpTrackInsts
	 */
	virtual void InitGroupInst( UInterpGroup* InGroup, AActor* InGroupActor );

	/** 
	 *	Initialze this Group instance from Seq Variable
	 */
	void UpdatePreviewPawnFromSeqVarCharacter( UInterpGroup* InGroup, const USeqVar_Character* InGroupObject );

	/**
	 * Create Preview Pawn/Destroy Preview Pawn
	 */ 
	void CreatePreviewPawn();
	void DestroyPreviewPawn();

	/**
	 * Get Stage Mark Actor ground position & rotation
	 */
	FVector     GetStageMarkPosition(FRotator* Rotation = NULL);
	
	/** 
	 *  Update Stage Mark Group Actor
	 */ 
	void UpdateStageMarkGroupActor(USeqAct_Interp * Seq);


	/** 
	 *	Called when done with interpolation sequence. Cleans up InterpTrackInsts etc. 
	 *	Do not do anything further with the Interpolation after this.
	 */
	virtual void TermGroupInst(UBOOL bDeleteTrackInst);

	/** 
	 *  Update Physics state if it includes Movement Track 
	 *  Or terminate if bInit = FALSE
	 */ 
	void UpdatePhysics(UBOOL bInit);
}

/** Cache data to AIGroup **/
var transient InterpGroupAI AIGroup;

/** Saved Physics state to go back to **/
var EPhysics  	SavedPhysics;
var bool        bSavedNoEncroachCheck;
var bool		bSavedCollideActors;
var bool		bSavedBlockActors;

/** Preview Pawn for only editor - in game it should be AI **/
var   editoronly transient Pawn PreviewPawn;

/** Stage Mark Actor - from StageMark Group **/
var transient Actor   StageMarkActor;

defaultproperties
{
}
