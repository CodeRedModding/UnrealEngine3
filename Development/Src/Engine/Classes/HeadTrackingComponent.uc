/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * When you attach this class, make sure you don't have any other HeadTrackingComponent 
 * That will create conflict. It will warn if it already has headtrackingcomponent
 */
class HeadTrackingComponent extends ActorComponent
	native(Anim);

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

/** Once entered the radius, how long do I really care to look  ? This affects rating. It will give benefit to the person who just entered **/
var() float MaxInterestTime;

/** Actor classes to look at as 0 index being the highest priority if you have anything specific **/
var(Target) array< class<Actor> >  ActorClassesToLookAt;

/** Target Bone Names, where to look at - priority from top to bottom, if not found, it will continue search **/
var(Target) array<name>     TargetBoneNames;

/** Actor to look at information **/
struct native ActorToLookAt
{
	var     Actor   Actor;
	var     float   Rating;
	var     float   EnteredTime;
	var     float   LastKnownDistance;
	var     float   StartTimeBeingLookedAt;
	var     bool    CurrentlyBeingLookedAt;
};

/** Array of actor information **/
var private const transient native map{class AActor*,struct FActorToLookAt* } CurrentActorMap;

/** SkeletalMeshComponent who owns this **/
var SkeletalMeshComponent SkeletalMeshComp;

/** Look at control **/
var private transient array<SkelControlLookAt>	TrackControls;

/** Cached value for where mesh location/rotation is at this tick **/
var private transient vector				RootMeshLocation;
var private transient rotator				RootMeshRotation;
cpptext
{
public:
	/** Enable/Disable HeadTracking **/
	void EnableHeadTracking(UBOOL bEnable);

	/**
	 * Attaches the component to a ParentToWorld transform, owner and scene.
	 * Requires IsValidComponent() == true.
	 */
	virtual void Attach();

	/**
	 * Detaches the component from the scene it is in.
	 * Requires bAttached == true
	 *
	 * @param bWillReattach TRUE is passed if Attach will be called immediately afterwards.  This can be used to
	 *                      preserve state between reattachments.
	 */
	virtual void Detach( UBOOL bWillReattach = FALSE );

	/**
	 * Updates time dependent state for this component.
	 * Requires bAttached == true.
	 * @param DeltaTime - The time since the last tick.
	 */
	virtual void Tick(FLOAT DeltaTime);

	/** Clear list **/
	virtual void BeginDestroy();

	/** Make sure CurrentActorMap is referenced */
	void AddReferencedObjects( TArray<UObject*>& ObjectArray );

private:
	/** 
	 * Update Acotr Map
	 * returns # of actors in the map
	 */
	INT UpdateActorMap(FLOAT CurrentTime);
	/**
	 * Find Best Candidate from the current listing
	*/
	FActorToLookAt * FindBestCandidate(FLOAT CurrentTime);
	/**
	 *  Update Head Tracking
	 */
	void UpdateHeadTracking(FLOAT DeltaTime);

	/** 
	 * Refresh Head Tracking Skel Control List
	 */
	void RefreshTrackControls();
}

defaultproperties
{
	TrackControllerName.Add("HeadLook")
	TrackControllerName.Add("LeftEyeLook")
	TrackControllerName.Add("RightEyeLook")

	ActorClassesToLookAt.Empty

	MinLookAtTime = 3.f
	MaxLookAtTime = 5.f
	MaxInterestTime = 7.f

	LookAtActorRadius = 500.f

	TargetBoneNames.Empty
	TargetBoneNames.Add("b_MF_Head")
	TargetBoneNames.Add("b_MF_Neck")
}
