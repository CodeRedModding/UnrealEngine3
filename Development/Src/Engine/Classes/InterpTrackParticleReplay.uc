class InterpTrackParticleReplay extends InterpTrack
	native(Interpolation);

/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * 
 *	This track implements support for creating and playing back captured particle system data
 */

cpptext
{
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

	virtual void PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst);
	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump);

	/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
	* @return	String name of the helper class.*/
	virtual const FString	GetEdHelperClassName() const;

	virtual class UMaterial* GetTrackIcon() const;

	/** Whether or not this track is allowed to be used on static actors. */
	virtual UBOOL AllowStaticActors() { return TRUE; }

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

	virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params );
}


/** Data for a single key in this track */
struct native ParticleReplayTrackKey
{
	/** Position along timeline */
	var	float Time;

	/** Time length this clip should be captured/played for */
	var() float Duration;

	/** Replay clip ID number that identifies the clip we should capture to or playback from */
	var() int ClipIDNumber;
};	

/** Array of keys */
var	editinline array<ParticleReplayTrackKey> TrackKeys;

/** True in the editor if track should be used to capture replay frames instead of play them back */
var transient editoronly const bool bIsCapturingReplay;

/** Current replay fixed time quantum between frames (one over frame rate) */
var transient editoronly const float FixedTimeStep;


defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInstParticleReplay'
	TrackTitle="Particle Replay"
}
