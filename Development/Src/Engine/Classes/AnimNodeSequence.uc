/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AnimNodeSequence extends AnimNode
	native(Anim)
	hidecategories(Object);

/** This name will be looked for in all AnimSet's specified in the AnimSets array in the SkeletalMeshComponent. */
var()	const name		AnimSeqName;

/** Speed at which the animation will be played back. Multiplied by the RateScale in the AnimSequence. Default is 1.0 */
var()	float			Rate;

/** Whether this animation is currently playing ie. if the CurrentTime will be advanced when Tick is called. */
var()	bool			bPlaying;

/** If animation is looping. If false, animation will stop when it reaches end, otherwise will continue from beginning. */
var()	bool			bLooping;

/** Should this node call the OnAnimEnd event on its parent Actor when it reaches the end and stops. */
var()	bool			bCauseActorAnimEnd;

/** Should this node call the OnAnimPlay event on its parent Actor when PlayAnim is called on it. */
var()	bool			bCauseActorAnimPlay;

/** Always return a zero rotation (unit quaternion) for the root bone of this animation. */
var()	bool			bZeroRootRotation;
/** Always return root bone translation at the origin. */
var()	bool			bZeroRootTranslation;
/** if TRUE, don't display a warning when animation is not found. */
var()	bool			bDisableWarningWhenAnimNotFound;

/** Current position (in seconds) */
var()	const float				CurrentTime;
// Keep track of where animation was at before being ticked
var		const transient float	PreviousTime;

/** This is the time at which to end the anim.  Example:  You have a 10 second anim where you just want a portion of the anim!   Great for prototyping! */
var const transient float       EndTime;

/** Pointer to actual AnimSequence. Found from SkeletalMeshComponent using AnimSeqName when you call SetAnim. */
var		transient const AnimSequence	AnimSeq;

/** Bone -> Track mapping info for this player node. Index into the LinkupCache array in the AnimSet. Found from AnimSet when you call SetAnim. */
var		transient const int				AnimLinkupIndex;

/** 
 * Total weight that this node must be at in the final blend for notifies to be executed.
 * This is ignored when the node is part of a group.
 */
var()				float	NotifyWeightThreshold;
/** Whether any notifies in the animation sequence should be executed for this node. */
var()				bool	bNoNotifies;
/** Forces the skeletal mesh into the ref pose by setting bForceRespose on the skelmesh comp when not playing. (Optimization) */
var()				bool	bForceRefposeWhenNotPlaying;
/**
 *	Flag that indicates if Notifies are currently being executed.
 *	Allows you to avoid doing dangerous things to this Node while this is going on.
 */
var					bool	bIsIssuingNotifies;

/** name of group this node belongs to */
var(Group) const	Name	SynchGroupName;
/** If TRUE, this node can never be a synchronization master node, always slave. */
var(Group)			bool	bForceAlwaysSlave;

/** 
 * TRUE by default. This node can be synchronized with others, when part of a SynchGroup. 
 * Set to FALSE if node shouldn't be synchronized, but still part of notification group.
 */
var(Group) const	bool	bSynchronize;
/** Relative position offset. */
var(Group)			float	SynchPosOffset;
/** Reverse synchronization. Go in opposite direction. */
var(Group) const    bool    bReverseSync;

/** Display time line slider */
var(Display)		bool	bShowTimeLineSlider;

/** Reference to the CameraAnim to play in conjunction with this animation. */
var(Camera)			CameraAnim	CameraAnim;
/** Ref to the CameraAnimInst that is currently playing. */
var transient CameraAnimInst	ActiveCameraAnimInstance;
/** True to loop the CameraAnim, false for a one-off. */
var(Camera)			bool		bLoopCameraAnim;
/** True to randomize the CameraAnims start position, so it doesn't look the same every time.  Ignored if bLoopCameraAnim is false. */
var(Camera)			bool		bRandomizeCameraAnimLoopStartTime;
/** "Intensity" multiplier applied to the camera anim. */
var(Camera)			float		CameraAnimScale;
/** How fast to playback the camera anim. */
var(Camera)			float		CameraAnimPlayRate;
/** How long to blend in the camera anim. */
var(Camera)			float		CameraAnimBlendInTime;
/** How long to blend out the camera anim. */
var(Camera)			float		CameraAnimBlendOutTime;

/**
 *	This will actually call MoveActor to move the Actor owning this SkeletalMeshComponent.
 *	You can specify the behaviour for each axis (mesh space).
 *	Doing this for multiple skeletal meshes on the same Actor does not make much sense!
 */
enum ERootBoneAxis
{
	/** the default behaviour, leave root translation from animation and do no affect owning Actor movement. */
	RBA_Default,
	/** discard any root bone movement, locking it to the first frame's location. */
	RBA_Discard,
	/** discard root movement on animation, and forward its velocity to the owning actor. */
	RBA_Translate,
};

var() const ERootBoneAxis RootBoneOption[3]; // [X, Y, Z] axes

/**
 * Root Motion Rotation.
 */
enum ERootRotationOption
{
	/** Default, leaves root rotation in the animation. Does not affect actor. */
	RRO_Default,
	/** Discards root rotation from the animation, locks to first frame rotation of animation. Does not affect actor's rotation. */
	RRO_Discard,
	/** Discard root rotation from animation, and forwards it to the actor. (to be used by it or not). */
	RRO_Extract,
};

var() const ERootRotationOption	RootRotationOption[3];	// Roll (X), Pitch (Y), Yaw (Z) axes.

/** 
 * EDITOR ONLY
 * Add Ref Pose to Additive Animation, so it can be viewed fully into the AnimSetViewer.
 */
var const	bool	bEditorOnlyAddRefPoseToAdditiveAnimation;

/** List of SkelControl controlled by MetaData */
var const transient Array<SkelControlBase> MetaDataSkelControlList;

/** This allows up to return from FinishAnim on blendout when wanted */
var transient bool  bCheckForFinishAnimEarly;
/** only setup to work with AnimNodeSlot and FinishAnim right now */
var transient bool  bBlendingOut;

cpptext
{
protected:
	// Internal
	/** Returns the camera associated with the skelmesh's owner, if any. */
	ACamera* GetPlayerCamera() const;

public:
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void BeginDestroy();

	// AnimNode interface
	virtual void InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent );
	/** Deferred Initialization, called only when the node is relevant in the tree. */
	virtual void DeferredInitAnim();
	virtual void ResetAnimNodeToSource(UAnimNode *SourceNode);
    virtual UBOOL GetCachedResults(FBoneAtomArray& OutAtoms, FBoneAtom& OutRootMotionDelta, INT& bOutHasRootMotion, FCurveKeyArray& OutCurveKeys, INT NumDesiredBones);
    virtual UBOOL ShouldSaveCachedResults();
	void ConditionalClearCachedData();
	/** 
	 *	Whether we should keep the cached result for the next frame or not 
	 *	This is to avoid keeping cached result on memory once it ticks. 
	 *	It will release cache result if this returns FALSE
	 **/
	virtual UBOOL ShouldKeepCachedResult();	

	/** AnimSets have been updated, update all animations */
	virtual void AnimSetsUpdated();

	virtual	void TickAnim(FLOAT DeltaSeconds);	 // Progress the animation state, issue AnimEnd notifies.
	virtual void GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys);

	/**
	 * Draws this node in the AnimTreeEditor.
	 *
	 * @param	Canvas			The canvas to use.
	 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
	 * @param	bShowWeight		If TRUE, show the global percentage weight of this node, if applicable.
	 */
	virtual void DrawAnimNode(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes, UBOOL bShowWeight);
	virtual FString GetNodeTitle();

	// AnimNodeSequence interface
	void GetAnimationPose(UAnimSequence* InAnimSeq, INT& InAnimLinkupIndex, FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys);

	// Extract root motion for animation using a specified start and end time
	void ExtractRootMotionUsingSpecifiedTimespan (UAnimSequence* InAnimSeq, const INT &TrackIndex, FBoneAtom& RootBoneAtom, FBoneAtom& RootBoneAtomDeltaMotion, INT& bHasRootMotion, FLOAT StartTime, FLOAT EndTime) const;

	/**
	*  Extract Root Motion for the current Animation Pose.
	*/
	inline virtual void ExtractRootMotion (UAnimSequence* InAnimSeq, const INT &TrackIndex, FBoneAtom& RootBoneAtom, FBoneAtom& RootBoneAtomDeltaMotion, INT& bHasRootMotion) 
	{
		ExtractRootMotionUsingSpecifiedTimespan(InAnimSeq, TrackIndex, RootBoneAtom, RootBoneAtomDeltaMotion, bHasRootMotion, PreviousTime, CurrentTime); 
	}

	/** Advance animation time. Take care of issuing notifies, looping and so on */
	void AdvanceBy(FLOAT MoveDelta, FLOAT DeltaSeconds, UBOOL bFireNotifies);

	/** Issue any notifies that are passed when moving from the current position to DeltaSeconds in the future. Called from TickAnim. */
	void IssueNotifies(FLOAT DeltaSeconds);

	/** Allow negative play rates and still get animnotifies$$$**/
	void IssueNegativeRateNotifies(FLOAT DeltaSecond);

	/** 
	 * notification that current animation has reached the end (will be called even if it loops, unlike OnAnimEnd)
	 * @param	PlayedTime	Time in seconds of animation played. (play rate independant).
	 * @param	ExcessTime	Time in seconds beyond end of animation. (play rate independant).
	 */
	virtual void OnAnimComplete( FLOAT PlayedTime, FLOAT ExcessTime ) {}

	/**
	 * notification that current animation finished playing.
	 * @param	PlayedTime	Time in seconds of animation played. (play rate independant).
	 * @param	ExcessTime	Time in seconds beyond end of animation. (play rate independant).
	 */
	virtual void OnAnimEnd(FLOAT PlayedTime, FLOAT ExcessTime);
	
	// AnimTree editor interface
	virtual INT GetNumSliders() const { return bShowTimeLineSlider ? 1 : 0; }
	virtual FLOAT GetSliderPosition(INT SliderIndex, INT ValueIndex);
	virtual void HandleSliderMove(INT SliderIndex, INT ValueIndex, FLOAT NewSliderValue);
	virtual FString GetSliderDrawValue(INT SliderIndex);

	/** Restart camera animations */
	virtual void OnBecomeRelevant();

	/** Pause camera animations */
	virtual void OnCeaseRelevant();

	/** Starts playing any camera anim we want to play in conjunction with this anim. */
	void StartCameraAnim();
	/** Stops playing any active camera anim playing in conjunction with this anim. */
	void StopCameraAnim();

	/** Update animation usage **/
#if !FINAL_RELEASE
	virtual void UpdateAnimationUsage( FLOAT DeltaSeconds );
#endif	//#if !FINAL_RELEASE

	/** Initialize morph curve information **/
	void InitCurveData();

/** Utility functions to ease off Casting */
	virtual class UAnimNodeSequence* GetAnimNodeSequence() { return this; }
}

/** Change the animation this node is playing to the new name. Will be looked up in owning SkeletaMeshComponent's AnimSets array. */
native function SetAnim( name Sequence );

/** Start the current animation playing with the supplied parameters. */
native function PlayAnim(bool bLoop = false, float InRate = 1.0f, float StartTime = 0.0f);

/** Stop the current animation playing. CurrentTime will stay where it was. */
native function StopAnim();

// calls PlayAnim with the current settings
native function ReplayAnim();

/** Force the animation to a particular time. NewTime is in seconds. */
native function SetPosition(float NewTime, bool bFireNotifies);

/** Get normalized position, from 0.f to 1.f. */
native function float GetNormalizedPosition() const;

/** 
 * Finds out normalized position of a synchronized node given a relative position of a group. 
 * Takes into account node's relative SynchPosOffset.
 */
native function float FindGroupRelativePosition(FLOAT GroupRelativePosition) const;
/** 
 * Finds out position of a synchronized node given a relative position of a group. 
 * Takes into account node's relative SynchPosOffset.
 */
native function float FindGroupPosition(FLOAT GroupRelativePosition) const;
/** 
 * Get relative position of a synchronized node. 
 * Taking into account node's relative offset.
 */
native function float GetGroupRelativePosition() const;

/** Returns the global play rate of this animation. Taking into account all Rate Scales */
native function float GetGlobalPlayRate();

/** Returns the duration (in seconds) of the current animation at the current play rate. Returns 0.0 if no animation. */
native function float GetAnimPlaybackLength();

/**
 * Returns in seconds the time left until the animation is done playing.
 * This is assuming the play rate is not going to change.
 */
native function float GetTimeLeft();

/**
 * Set custom animation root bone options.
 */
final native function SetRootBoneAxisOption
(
	ERootBoneAxis AxisX = RBA_Default,
	ERootBoneAxis AxisY = RBA_Default,
	ERootBoneAxis AxisZ = RBA_Default
 );

/**
 * Set custom animation root rotation options.
 */
final native function SetRootBoneRotationOption
(
	ERootRotationOption AxisX = RRO_Default,
	ERootRotationOption AxisY = RRO_Default,
	ERootRotationOption AxisZ = RRO_Default
);



defaultproperties
{
	Rate=1.0
	NotifyWeightThreshold=0.0
	bSynchronize=TRUE

	RootBoneOption[0] = RBA_Default
	RootBoneOption[1] = RBA_Default
	RootBoneOption[2] = RBA_Default

	RootRotationOption[0]=RRO_Default
	RootRotationOption[1]=RRO_Default
	RootRotationOption[2]=RRO_Default

	CameraAnimPlayRate=1.f
	CameraAnimScale=1.f
	CameraAnimBlendInTime=0.2f
	CameraAnimBlendOutTime=0.2f
}
