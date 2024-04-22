class InterpTrackToggle extends InterpTrack
	native(Interpolation);

/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * 
 *	A track containing toggle actions that are triggered as its played back. 
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

/** Enumeration indicating toggle action	*/
enum ETrackToggleAction
{
	ETTA_Off,
	ETTA_On,
	ETTA_Toggle,
	ETTA_Trigger
};

/** Information for one toggle in the track. */
struct native ToggleTrackKey
{
	var		float				Time;
	var()	ETrackToggleAction	ToggleAction;
};	

/** Array of events to fire off. */
var	array<ToggleTrackKey>	ToggleTrack;

/** 
 *	If true, the track will call ActivateSystem on the emitter each update (the old 'incorrect' behavior).
 *	If false (the default), the System will only be activated if it was previously inactive.
 */
var() bool	bActivateSystemEachUpdate;

/** 
 *	If true, the track will activate the system w/ the 'Just Attached' flag.
 */
var() bool	bActivateWithJustAttachedFlag;

/** If events should be fired when passed playing the sequence forwards. */
var() bool	bFireEventsWhenForwards;

/** If events should be fired when passed playing the sequence backwards. */
var() bool	bFireEventsWhenBackwards;

/** If true, events on this track are fired even when jumping forwads through a sequence - for example, skipping a cinematic. */
var() bool	bFireEventsWhenJumpingForwards;


defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInstToggle'
	TrackTitle="Toggle"
	bActivateSystemEachUpdate=false
	bActivateWithJustAttachedFlag=true
	bFireEventsWhenForwards=true
	bFireEventsWhenBackwards=true
	bFireEventsWhenJumpingForwards=true
}
