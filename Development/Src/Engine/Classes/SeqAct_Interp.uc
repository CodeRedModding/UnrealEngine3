/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_Interp extends SeqAct_Latent
	dependson(EngineBaseTypes)
	config(Engine)
	native(Sequence);

cpptext
{
	// UObject interface.
	/**
	 * Serialize function.
	 *
	 * @param	Ar		The archive to serialize with.
	 */
	virtual void Serialize(FArchive& Ar);

	// USequenceAction interface

	virtual void Activated();
	virtual UBOOL UpdateOp(FLOAT deltaTime);
	virtual void DeActivated();
	virtual void OnCreated();
	virtual void Initialize();

	virtual void UpdateObject();

	// USeqAct_Interp interface

	/**
	 * Begin playback of this sequence. Only called in game.
	 * Will then advance Position by (PlayRate * Deltatime) each time the SeqAct_Interp is ticked.
	 */
	void Play(UBOOL OnlyAIGroup=FALSE);

	/** Similar to play, but the playback will go backwards until the beginning of the sequence is reached. */
	void Reverse();

	/** Hold playback at its current position, but leave the sequence initialised. Calling Pause again will continue playback in its current direction. */
	void Pause();

	/** Changes the direction of playback (go in reverse if it was going forward, or vice versa) */
	void ChangeDirection();

	/** Called to notify affected actors when a new impulse changes the interpolation (paused, reversed direction, etc) */
	void NotifyActorsOfChange();

	/** Increment track forwards by given timestep and iterate over each track updating any properties. */
	virtual void StepInterp(FLOAT DeltaTime, UBOOL bPreview=FALSE);

	/** Move interpolation to new position and iterate over each track updating any properties. */
	virtual void UpdateInterp(FLOAT NewPosition, UBOOL bPreview=FALSE, UBOOL bJump=FALSE, UBOOL OnlyAIGroup=FALSE);

	/**
	 *	Updates the streaming system with the camera locations for the upcoming camera cuts, so
	 *	that it can start streaming in textures for those locations now.
	 *
	 *	@param	CurrentTime		Current time within the matinee, in seconds
	 *	@param	bPreview		If we are previewing sequence (ie. viewing in editor without gameplay running)
	 */
	void UpdateStreamingForCameraCuts(FLOAT CurrentTime, UBOOL bPreview=FALSE);

	/** For each InterGroup/Actor combination, create a InterpGroupInst, assign Actor and initialise each track. */
	void InitInterp();

	/** Destroy all InterpGroupInst. */
	void TermInterp();

	/** Reset the 'initial transform' for all movement tracks to be from the current actor positions. */
	void ResetMovementInitialTransforms();

	/** See if there is an instance referring to the supplied Actor. Returns NULL if not. */
	class UInterpGroupInst* FindGroupInst(AActor* Actor);

	/** Find the first group instance based on the given InterpGroup. */
	class UInterpGroupInst* FindFirstGroupInst(class UInterpGroup* InGroup);

	/** Find the first group instance based on the InterpGroup with the given name. */
	class UInterpGroupInst* FindFirstGroupInstByName( FName InGroupName );
	class UInterpGroupInst* FindFirstGroupInstByName( const FString& InGroupName );

	/** Resolves Named and External variables for the matinee preview */
	void GetNamedObjVars(TArray<UObject**>& OutObjects, const TCHAR* InDesc);

	/** Find the InterpData connected to the first Variable connector. Returns NULL if none attached. */
	class UInterpData* FindInterpDataFromVariable();

	/** Finds and returns the Director group, or NULL if not found. */
	class UInterpGroupDirector* FindDirectorGroup();

	/** Synchronise the variable connectors with the currently attached InterpData. */
	virtual void UpdateConnectorsFromData();

	/** Use any existing DirectorGroup to see which Actor we currently want to view through. */
	class AActor* FindViewedActor();

	/**
	 *	Utility for getting all Actors currently being worked on by this Matinee action.
	 *	If bMovementTrackOnly is set, Actors must have a Movement track in their group to be included in the results.
	 */
	void GetAffectedActors(TArray<AActor*>& OutActors, UBOOL bMovementTrackOnly);

	/**
	 * Conditionally saves state for the specified actor and its children
	 */
	void ConditionallySaveActorState( UInterpGroupInst* GroupInst, AActor* Actor );

	/**
	 * Adds the specified actor and any actors attached to it to the list
	 * of saved actor transforms.  Does nothing if an actor has already
	 * been saved.
	 */
	void SaveActorTransforms( AActor* Actor, UBOOL bOnlyChildren );

	/**
	 * Applies the saved locations and rotations to all saved actors.
	 */
	void RestoreActorTransforms();

	/** Saves whether or not this actor is hidden so we can restore it later */
	void SaveActorVisibility( AActor* Actor );

	/** Applies the saved visibility state for all saved actors */
	void RestoreActorVisibilities();

	/**
	 * Stores the current scrub position, restores all saved actor transforms,
	 * then saves off the transforms for actors referenced (directly or indirectly)
	 * by group instances, and finally restores the scrub position.
	 */
	void RecaptureActorState();

	/** Disables the radio filter effect if "Disable Radio Filter" is checked. */
	void DisableRadioFilterIfNeeded();

	/** Enables the radio filter */
	void EnableRadioFilter();

	/** called when the level that contains this sequence object is being removed/unloaded */
	virtual void CleanUp();

	/** Sets up the group actor for the specified InterpGroup. */
	void InitGroupActorForGroup(class UInterpGroup* InGroup, class AActor* GroupActor);

	/** Sets up the group actor for the specified InterpGroup. */
	void InitSeqObjectForGroup(class UInterpGroup* InGroup, USequenceObject* SequenceObject);

	/**
	 * Checks to see if this Matinee should be associated with the specified player.  This is a relatively
	 * quick test to perform.
	 *
	 * @param InPC The player controller to check
	 *
	 * @return TRUE if this Matinee sequence is compatible with the specified player
	 */
	UBOOL IsMatineeCompatibleWithPlayer( APlayerController* InPC ) const;

	/**
	 * Activates the output for the named event.
	 */
	virtual void NotifyEventTriggered(class UInterpTrackEvent const* EventTrack, INT EventIdx);

	/** Scans the matinee for camera cuts and sets up the CameraCut array. */
	void SetupCameraCuts();

	/** Copies the values from all VariableLinks to the member variable [of this sequence op] associated with that VariableLink */
	virtual void PublishLinkedVariableValues();

	/** Retrieve group linked variable **/
	AActor * FindGroupLinkedVariable(INT Index, const TArray<UObject**> &ObjectVars);
	AActor * FindUnusedGroupLinkedVariable(FName GroupName);

#if WITH_EDITOR
	/** Refresh variable links if it needs to be **/
	virtual void OnVariableConnect(USequenceVariable *Var, INT LinkIdx);
#endif
}

/**
 * Helper type for storing actors' World-space locations/rotations.
 */
struct native export SavedTransform
{
	var vector	Location;
	var rotator	Rotation;
};

/** Helper struct for storing the camera world-position for each camera cut in the cinematic. */
struct native CameraCutInfo
{
	var vector	Location;
	var float	Timestamp;
};

/** A map from actors to their pre-Matinee world-space positions/orientations.  Includes actors attached to Matinee-affected actors. */
var private const transient noimport native map{AActor*,FSavedTransform} SavedActorTransforms;

/** A map from actors to their pre-Matinee visibility state */
var private const transient noimport native map{AActor*,BYTE} SavedActorVisibilities;

/** Time multiplier for playback. */
var()	float					PlayRate;

/** Time position in sequence - starts at 0.0 */
var		float					Position;

/** Time position to always start at if bForceStartPos is set to TRUE. */
var()	float					ForceStartPosition;

/** If sequence is currently playing. */
var		bool					bIsPlaying;

/** Sequence is initialised, but ticking will not increment its current position. */
var		bool					bPaused;

/** Indicates whether this SeqAct_Interp is currently open in the Matinee tool. */
var		transient bool			bIsBeingEdited;

/**
 *	If sequence should pop back to beginning when finished.
 *	Note, if true, will never get Completed/Reversed events - sequence must be explicitly Stopped.
 */
var()	bool					bLooping;

/** If true, sequence will rewind itself back to the start each time the Play input is activated. */
var()	bool					bRewindOnPlay;

/**
 *	If true, when rewinding this interpolation, reset the 'initial positions' of any RelateToInitial movements to the current location.
 *	This allows the next loop of movement to proceed from the current locations.
 */
var()	bool					bNoResetOnRewind;

/**
 *	Only used if bRewindOnPlay if true. Defines what should happen if the Play input is activated while currently playing.
 *	If true, hitting Play while currently playing will pop the position back to the start and begin playback over again.
 *	If false, hitting Play while currently playing will do nothing.
 */
var()	bool					bRewindIfAlreadyPlaying;

/** If sequence playback should be reversed. */
var		bool					bReversePlayback;

/** Whether this action should be initialised and moved to the 'path building time' when building paths. */
var()	bool					bInterpForPathBuilding;

/** Lets you force the sequence to always start at ForceStartPosition */
var()	bool					bForceStartPos;

/** If true, disables the realtime radio effect */
var()	bool					bDisableRadioFilter;

/** Indicates that this interpolation does not affect gameplay. This means that:
 * -it is not replicated via MatineeActor
 * -it is not ticked if no affected Actors are visible
 * -on dedicated servers, it is completely ignored
 */
var() bool bClientSideOnly;

/** if bClientSideOnly is true, whether this matinee should be completely skipped if none of the affected Actors are visible */
var() bool bSkipUpdateIfNotVisible;

/** Lets you skip the matinee with the CANCELMATINEE exec command. Triggers all events to the end along the way. */
var()	bool					bIsSkippable;

/** Cover linked to this matinee that should be updated once path building time has been played */
var() array<CoverLink>			LinkedCover;

/** Actual track data. Can be shared between SeqAct_Interps. */
var		export InterpData		InterpData;

/** Instance data for interp groups. One for each variable/group combination. */
var		array<InterpGroupInst>	GroupInst;

/** on a net server, actor spawned to handle replicating relevant data to the client */
var const class<MatineeActor> ReplicatedActorClass;
var const transient MatineeActor ReplicatedActor;

/** Preferred local viewport number (when split screen is active) the director track should associate with, or zero for 'all'. */
var() int PreferredSplitScreenNum;

/** Cached value that indicates whether or not gore was enabled when the sequence was started */
var transient bool bShouldShowGore;

/** Contains the camera world-position for each camera cut in the cinematic. */
var transient array<CameraCutInfo> CameraCuts;

/** last time TermInterp() was called on this action. Only updated in game */
var float TerminationTime;

/** 
 * Rendering overrides of this matinee, which will be used if the matinee has a director track.
 * Cinematics add a bunch of rendering cost with their lighting and shadowing, so we need to disable some features to make room for that.
 */
var() globalconfig RenderingPerformanceOverrides RenderingOverrides;

/** Constant camera anim to add to matinee, set to 1 (future we could have more) */
var(ConstantCameraAnim) byte ConstantCameraAnim;
var(ConstantCameraAnim) float ConstantCameraAnimRate;

/** sets the position of the interpolation
 * @note if the interpolation is not currently active, this function doesn't send any Kismet or UnrealScript events
 * @param NewPosition the new position to set the interpolation to
 * @param bJump if true, teleport to the new position (don't trigger any events between the old and new positions, etc)
 */
native final function SetPosition(float NewPosition, optional bool bJump = false);

/** stops playback at current position */
native final function Stop();

/** adds the passed in PlayerController to all running Director tracks so that its camera is controlled
 * all PCs that are available at playback start time are hooked up automatically, but this needs to be called to hook up
 * any that are created during playback (player joining a network game during a cinematic, for example)
 * @param PC the PlayerController to add
 */
native final function AddPlayerToDirectorTracks(PlayerController PC);

function Reset()
{
	SetPosition(0.0, false);
	// stop if currently playing
	if (bActive)
	{
		InputLinks[2].bHasImpulse = true;
	}
}

static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	ObjName="Matinee"

	PlayRate=1.0

	InputLinks(0)=(LinkDesc="Play")
	InputLinks(1)=(LinkDesc="Reverse")
	InputLinks(2)=(LinkDesc="Stop")
	InputLinks(3)=(LinkDesc="Pause")
	InputLinks(4)=(LinkDesc="Change Dir")

	OutputLinks(0)=(LinkDesc="Completed")
	OutputLinks(1)=(LinkDesc="Reversed")

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'InterpData',LinkDesc="Data",MinVars=1,MaxVars=1)

	ReplicatedActorClass=class'MatineeActor'

	ConstantCameraAnim=0
	ConstantCameraAnimRate=4.0
}
