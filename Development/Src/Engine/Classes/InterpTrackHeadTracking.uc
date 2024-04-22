class InterpTrackHeadTracking extends InterpTrack
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

private:
	/** Update Actor List for look at candidate **/
	void    UpdateHeadTracking(AActor* Actor, UInterpTrackInst* TrInst, FLOAT DeltaTime);
}


/** HeadTracking actions */
enum EHeadTrackingAction
{
	/** Disable Head Tracking */
	EHTA_DisableHeadTracking,

	/** Enable Head Tracking */
	EHTA_EnableHeadTracking,
};

/** Information for one toggle in the track. */
struct native HeadTrackingKey
{
	var		float					Time;
	var()	EHeadTrackingAction	    Action;
};	

/** Array of keys . */
var	array<HeadTrackingKey>	HeadTrackingTrack;

/** SkelControlLookAt name in the AnimTree of the SkeletalMesh **/
var() array<name>  TrackControllerName;

/** Will pick up actor within this radius **/
var() float LookAtActorRadius;

/** Interp back to zero strength if limit surpassed */
var() bool	bDisableBeyondLimit;

/** How long can one person to look at one **/
var() float MaxLookAtTime;
/** At least this time to look at one **/
var() float MinLookAtTime;

/** Once entered the radius, how long do I really care to lok  ? This affects rating. It will give benefit to the person who just entered **/
var() float MaxInterestTime;

/** Quick check box for allowing it to look Pawn - due to Pawn not being listed in the Actor class **/
var(Target) bool bLookAtPawns;

/** Actor classes to look at as 0 index being the highest priority if you have anything specific **/
var(Target) array< class<Actor> >  ActorClassesToLookAt;

/** Target Bone Names, where to look at - priority from top to bottom, if not found, it will continue search **/
var(Target) array<name>     TargetBoneNames;

defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInstHeadTracking'
	TrackTitle="HeadTracking"
	TrackControllerName.Add("HeadLook")
	TrackControllerName.Add("LeftEyeLook")
	TrackControllerName.Add("RightEyeLook")

	ActorClassesToLookAt.Empty

	MinLookAtTime = 3.f
	MaxLookAtTime = 5.f
	MaxInterestTime = 7.f

	LookAtActorRadius = 500.f
	bLookAtPawns = TRUE

	TargetBoneNames.Empty
	TargetBoneNames.Add("b_MF_Head")
	TargetBoneNames.Add("b_MF_Neck")
}
