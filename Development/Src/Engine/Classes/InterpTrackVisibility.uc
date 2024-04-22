class InterpTrackVisibility extends InterpTrack
	native(Interpolation);

/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * 
 *	This track implements support for setting or toggling the visibility of the associated actor
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

	virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params );
}


/** Visibility track actions */
enum EVisibilityTrackAction
{
	/** Hides the object */
	EVTA_Hide,

	/** Shows the object */
	EVTA_Show,

	/** Toggles visibility of the object */
	EVTA_Toggle
};



/** Required condition for firing this event */
enum EVisibilityTrackCondition
{
	/** Always play this event */
	EVTC_Always,

	/** Only play this event when extreme content (gore) is enabled */
	EVTC_GoreEnabled,

	/** Only play this event when extreme content (gore) is disabled */
	EVTC_GoreDisabled
};



/** Information for one toggle in the track. */
struct native VisibilityTrackKey
{
	var		float					Time;
	var()	EVisibilityTrackAction	Action;

	/** Condition that must be satisfied for this key event to fire */
	var EVisibilityTrackCondition ActiveCondition;
};	

/** Array of events to fire off. */
var	array<VisibilityTrackKey>	VisibilityTrack;

/** If events should be fired when passed playing the sequence forwards. */
var() bool	bFireEventsWhenForwards;

/** If events should be fired when passed playing the sequence backwards. */
var() bool	bFireEventsWhenBackwards;

/** If true, events on this track are fired even when jumping forwads through a sequence - for example, skipping a cinematic. */
var() bool	bFireEventsWhenJumpingForwards;

defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInstVisibility'
	TrackTitle="Visibility"
	bFireEventsWhenForwards=true
	bFireEventsWhenBackwards=true
	bFireEventsWhenJumpingForwards=true
}
