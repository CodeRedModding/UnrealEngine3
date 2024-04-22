class InterpTrackAnimControl extends InterpTrackFloatBase
	native(Interpolation);
	
/** 
 * InterpTrackAnimControl
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
 
cpptext
{
	// UObject interface
	virtual void PostLoad();

	// InterpTrack interface
	virtual INT GetNumKeyframes() const;
	virtual void GetTimeRange(FLOAT& StartTime, FLOAT& EndTime) const;
	virtual FLOAT GetTrackEndTime() const;
	virtual FLOAT GetKeyframeTime(INT KeyIndex) const;
	virtual INT AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode);
	virtual INT SetKeyframeTime(INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder=true);
	virtual void RemoveKeyframe(INT KeyIndex);
	virtual INT DuplicateKeyframe(INT KeyIndex, FLOAT NewKeyTime);
	virtual UBOOL GetClosestSnapPosition(FLOAT InPosition, TArray<INT> &IgnoreKeys, FLOAT& OutPosition);
	virtual FColor GetKeyframeColor(INT KeyIndex) const;

	virtual void PreviewUpdateTrack(FLOAT NewPosition, class UInterpTrackInst* TrInst);
	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump);

	/** 
	 * Calculates the reversed time for a sequence key, if the key has bReverse set.
	 *
	 * @param SeqKey		Key that is reveresed and we are trying to find a position for.
	 * @param Seq			Anim sequence the key represents.  If NULL, the function will lookup the sequence.
	 * @param InPosition	Timeline position that we are currently at.
	 *
	 * @return Returns the position in the specified seq key. 
	 */
	FLOAT ConditionallyReversePosition(FAnimControlTrackKey &SeqKey, UAnimSequence* Seq, FLOAT InPosition);

	/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
	* @return	String name of the helper class.*/
	virtual const FString	GetEdHelperClassName() const;

	virtual class UMaterial* GetTrackIcon() const;
	virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params );
	
	// InterpTrackAnimControl interface
	class UAnimSequence* FindAnimSequenceFromName(FName InName);
	const class UAnimSequence* FindAnimSequenceFromName(FName InName) const;
	/** Get Animation for the given Time 
	 *  @return TRUE if it needs the animation to advance timer (from Previous to Current Time for Root Motion
	 */
	UBOOL GetAnimForTime(FLOAT InTime, FName& OutAnimSeqName, FLOAT& OutPosition, UBOOL& bOutLooping);
	FLOAT GetWeightForTime(FLOAT InTime);
	INT SplitKeyAtPosition(FLOAT InPosition);

	/**
	 * Crops the key at the position specified, by deleting the area of the key before or after the position.
	 *
	 * @param InPosition				Position to use as a crop point.
	 * @param bCutAreaBeforePosition	Whether we should delete the area of the key before the position specified or after.
	 *
	 * @return Returns the index of the key that was cropped.
	 */
	INT CropKeyAtPosition(FLOAT InPosition, UBOOL bCutAreaBeforePosition);

	// FInterpEdInputInterface

	/**
	 * Lets the interface object know that we are beginning a drag operation.
	 */
	virtual void BeginDrag(FInterpEdInputData &InputData);

	/**
	 * Lets the interface object know that we are ending a drag operation.
	 */
	virtual void EndDrag(FInterpEdInputData &InputData);

	/**
	 * @return Returns the mouse cursor to display when this input interface is moused over.
	 */
	EMouseCursor GetMouseCursor(FInterpEdInputData &InputData);

	/**
	 * Called when an object is dragged.
	 */
	void ObjectDragged(FInterpEdInputData& InputData);

	/** Calculate the index of this Track within its Slot (for when multiple tracks are using same slot). */
	INT CalcChannelIndex();
}

/** 
 *	DEPRECATED! USE UInterpGroup::GroupAnimSets instead now.
 */
var		array<AnimSet>			AnimSets;

/** 
 *	Name of slot to use when playing animation. Passed to Actor. 
 *	When multiple tracks use the same slot name, they are each given a different ChannelIndex when SetAnimPosition is called. 
 */
var()	name			SlotName;

/** Structure used for holding information for one animation played on the Anim Control track. */
struct native AnimControlTrackKey
{
	/** Position in the Matinee sequence to start playing this animation. */
	var		float	StartTime;
	
	/** Name of AnimSequence to play. */
	var		name	AnimSeqName;
	
	/** Time to start playing AnimSequence at. */
	var		float	AnimStartOffset;
	
	/** Time to end playing the AnimSequence at. */
	var		float	AnimEndOffset;

	/** Playback speed of this animation. */
	var		float	AnimPlayRate;
	
	/** Should this animation loop. */
	var		bool	bLooping;

	/** Whether to play the animation in reverse or not. */
	var		bool	bReverse;
};	

/** Track of different animations to play and when to start playing them. */
var	array<AnimControlTrackKey>	AnimSeqs;

/** Enable root motion. This only works if you delete Movement Track to avoid conflicts **/
var()	bool    bEnableRootMotion;

/**  Skip all anim notifiers **/
var()	bool	bSkipAnimNotifiers;

defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInstAnimControl'
	TrackTitle="Anim"
	bIsAnimControlTrack=true
}
