class AnimNodeSequence extends AnimNode
	native(Anim)
	hidecategories(Object);

/** This name will be looked for in all AnimSet's specified in the AnimSets array in the SkeletalMeshComponent. */
var()	name			AnimSeqName;

/** Speed at which the animation will be played back. Multiplied by the RateScale in the AnimSequence. Default is 1.0 */
var()	float			Rate;

/** Whether this animation is currently playing ie. if the CurrentTime will be advanced when Tick is called. */
var()	bool			bPlaying;

/** If animation is looping. If false, animation will stop when it reaches end, otherwise will continue from beginning. */
var()	bool			bLooping;

/** Whether any notifies in the animation sequence should be executed for this node. */
var()	bool			bNoNotifies;

/** Should this node call the OnAnimEnd event on its parent Actor when it reaches the end and stops. */
var()	bool			bCauseActorAnimEnd;

/** Should this node call the OnAnimPlay event on its parent Actor when PlayAnim is called on it. */
var()	bool			bCauseActorAnimPlay;

/** Always return a zero translation for the root bone of this animation. */
var()	bool			bZeroRootTranslationX;
var()	bool			bZeroRootTranslationY;
var()	bool			bZeroRootTranslationZ;

/** Current position (in seconds) */
var()	float			CurrentTime;

/** Total weight that this node must be at in the final blend for notifies to be executed. */
var()	float			NotifyWeightThreshold;

/** Pointer to actual AnimSequence. Found from SkeletalMeshComponent using AnimSeqName when you call SetAnim. */
var		transient const AnimSequence	AnimSeq;	

/** Bone -> Track mapping info for this player node. AnimSetMeshLinkup pointer. Found from AnimSet when you call SetAnim. */
var		transient const pointer			AnimLinkup;


/** For debugging. Track graphic used for showing animation position. */
var	texture2D	DebugTrack;

/** For debugging. Small icon to show current position in animation. */
var texture2D	DebugCarat;

cpptext
{
	// UObject interface

	virtual void PostEditChange(UProperty* PropertyThatChanged);


	// AnimNode interface

	virtual void InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent );
	virtual	void TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight );	 // Progress the animation state, issue AnimEnd notifies.
	virtual void GetBoneAtoms( TArray<struct FBoneAtom>& Atoms);

	virtual void DrawDebug(FRenderInterface* RI, FLOAT ParentPosX, FLOAT ParentPosY, FLOAT PosX, FLOAT PosY, FLOAT Weight);

	virtual void DrawAnimNode(FRenderInterface* RI, UBOOL bSelected);

	// AnimNodeSequence interface

	void SetAnim( FName SequenceName );
	void PlayAnim( UBOOL bLoop=false, FLOAT Rate=1.0f, FLOAT StartTime=0.0f );
	void StopAnim();
	void SetPosition( FLOAT NewTime );

	FLOAT GetAnimLength();

	/** Issue any notifies tha are passed when moving from the current position to DeltaSeconds in the future. Called from TickAnim. */
	void IssueNotifies(FLOAT DeltaSeconds);
}

/** Change the animation this node is playing to the new name. Will be looked up in owning SkeletaMeshComponent's AnimSets array. */
native function SetAnim( name Sequence );

/** Start the current animation playing with the supplied parameters. */
native function PlayAnim( optional bool bLoop, optional float Rate, optional float StartTime );

/** Stop the current animation playing. CurrentTime will stay where it was. */
native function StopAnim();

/** Force the animation to a particular time. NewTime is in seconds. */
native function SetPosition( float NewTime );

/** Returns the duration (in seconds) of the currently played animation. Returns 0.0 if no animation. */
native function float GetAnimLength();




defaultproperties
{
	Rate=1.0
	NotifyWeightThreshold=0.0
	DebugTrack=Texture2D'EngineResources.AnimPlayerTrack'
	DebugCarat=Texture2D'EngineResources.AnimPlayerCarat'
}