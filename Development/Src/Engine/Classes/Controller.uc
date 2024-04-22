//=============================================================================
// Controller, the base class of players or AI.
//
// Controllers are non-physical actors that can be attached to a pawn to control
// its actions.  PlayerControllers are used by human players to control pawns, while
// AIControFllers implement the artificial intelligence for the pawns they control.
// Controllers take control of a pawn using their Possess() method, and relinquish
// control of the pawn by calling UnPossess().
//
// Controllers receive notifications for many of the events occuring for the Pawn they
// are controlling.  This gives the controller the opportunity to implement the behavior
// in response to this event, intercepting the event and superceding the Pawn's default
// behavior.
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class Controller extends Actor
	native(Controller)
	nativereplication
	implements(Interface_NavigationHandle)
	abstract;

//=============================================================================
// BASE VARIABLES

/** Pawn currently being controlled by this controller.  Use Pawn.Possess() to take control of a pawn */
var editinline repnotify Pawn Pawn;

/** PlayerReplicationInfo containing replicated information about the player using this controller (only exists if bIsPlayer is true). */
var editinline repnotify PlayerReplicationInfo 	PlayerReplicationInfo;

var const int							PlayerNum;						// The player number - per-match player number.
var const private Controller			NextController;					// chained Controller list

var		bool        					bIsPlayer;						// Pawn is a player or a player-bot.
var		bool							bGodMode;		   				// cheat - when true, can't be killed or hurt

var		bool		bSoaking;			// pause and focus on pawn controlled by this controller if it encounters a problem
var		bool		bSlowerZAcquire;	// AI acquires targets above or below more slowly than at same height


// Input buttons.
var input byte							bFire;

/** If true, the controlled pawn will attempt to alt fire */
var input byte							bAltFire;

//=============================================================================
// PHYSICS VARIABLES

var		bool							bNotifyPostLanded;				// if true, event NotifyPostLanded() after pawn lands after falling.
var		bool							bNotifyApex;					// if true, event NotifyJumpApex() when at apex of jump
var	 	float							MinHitWall;						// Minimum HitNormal dot Velocity.Normal to get a HitWall event from the physics


//=============================================================================
// NAVIGATION VARIABLES

/** Navigation handle used for pathing when using NavMesh */
var     class<NavigationHandle>         NavigationHandleClass;
var     editinline NavigationHandle     NavigationHandle;
/** Override search start position for navhandle path cache info */
var     bool                            bOverrideSearchStart;
var     Vector                          OverrideSearchStart;

var		bool							bAdvancedTactics;				// serpentine movement between pathnodes
var		bool							bCanDoSpecial;					// are we able to traverse R_SPECIAL reach specs?
var		bool							bAdjusting;						// adjusting around obstacle
var		bool							bPreparingMove;					// set true while pawn sets up for a latent move

/** Used by AI, set true to force AI to use serpentine/strafing movement when possible. */
var		bool							bForceStrafe;

var 	float							MoveTimer;						// internal timer for latent moves, useful for setting a max duration
var 	Actor							MoveTarget;						// actor being moved toward
var		BasedPosition					DestinationPosition;			// destination controlled pawn is moving toward
var		BasedPosition					FocalPosition;					// position controlled pawn is looking at
var		Actor							Focus;							// actor being looked at
var		Actor							GoalList[4];					// used by navigation AI - list of intermediate goals
var		BasedPosition					AdjustPosition;					// intermediate destination used while adjusting around obstacle (bAdjusting is true)
var NavigationPoint 					StartSpot;  					// where player started the match

/** Cached list of nodes filled in by the last call to FindPathXXX */
var array<NavigationPoint> RouteCache;

var 	ReachSpec						CurrentPath;					// Current path being moved along
var		ReachSpec						NextRoutePath;					// Next path in route to destination
var 	vector							CurrentPathDir;					// direction vector of current path
var 	Actor							RouteGoal;						// final destination for current route
var 	float							RouteDist;						// total distance for current route
var		float							LastRouteFind;					// time at which last route finding occured
var		InterpActor						PendingMover;					// InterpActor that controller is currently waiting on (e.g. door or lift)

/** used for discovering navigation failures */
var		Actor							FailedMoveTarget;
var		int								MoveFailureCount;

var float GroundPitchTime;

var Pawn ShotTarget;													// Target most recently aimed at
var const Actor							LastFailedReach;				// cache to avoid trying failed actorreachable more than once per frame
var const float 						FailedReachTime;
var const vector 						FailedReachLocation;

const LATENT_MOVETOWARD = 503; // LatentAction number for Movetoward() latent function

//=============================================================================
// AI VARIABLES

var const bool							bLOSflag;						// used for alternating LineOfSight traces
var		bool							bSkipExtraLOSChecks;			// Skip viewport nudging checks for LOS
var		bool							bNotifyFallingHitWall;			// If true, controller gets NotifyFallingHitWall() when pawn hits wall while falling
var		float							SightCounter;					// Used to keep track of when to check player visibility
var		float							SightCounterInterval;			// how often player visibility is checked
var     bool                            bEarlyOutOfSighTestsForSameType;// when an AI already has an enemy of this type, early out from sight tests to me
/** multiplier to cost of NavigationPoints that another Pawn is currently anchored to */
var float InUseNodeCostMultiplier;

/** additive modifier to cost of NavigationPoints that require high jumping */
var int HighJumpNodeCostModifier;

/** Max time when moving toward a pawn target before latent movetoward returns (allowing reassessment of movement) */
var float MaxMoveTowardPawnTargetTime;

// Enemy information
var	 	Pawn					    	Enemy;

/** Forces all velocity to be directed towards reaching Destination */
var bool bPreciseDestination;

/** Do visibility checks, call SeePlayer events() for pawns on same team as self.  Setting to true will result in a lot more AI visibility line checks. */
var bool bSeeFriendly;

/** List of destinations whose source portals are visible to this Controller */
struct native VisiblePortalInfo
{
	/** source actor of portal */
	var Actor Source;
	/** destination actor of portal */
	var Actor Destination;

	structcpptext
	{
		FVisiblePortalInfo()
		{}
		FVisiblePortalInfo(EEventParm)
		{
			appMemzero(this, sizeof(FVisiblePortalInfo));
		}
		FVisiblePortalInfo(AActor* InSource, AActor* InDest)
		: Source(InSource), Destination(InDest)
		{}

		UBOOL operator==(const FVisiblePortalInfo& Other)
		{
			return Other.Source == Source && Other.Destination == Destination;
		}
	}
};
var array<VisiblePortalInfo> VisiblePortals;

/** indicates that the AI is within a lane in its CurrentPath (like a road)
 * to avoid ramming other Pawns also using that path
 * set by MoveToward() when it detects multiple AI pawns using the same path
 * when this is true, serpentine movement and cutting corners are disabled
 */
var bool bUsingPathLanes;

/** the offset from the center of CurrentPath to the center of the lane in use (the Pawn's CollisionRadius defines the extent)
 * positive values are to the Pawn's right, negative to the Pawn's left
 */
var float LaneOffset;

/** Used for reversing rejected mover base movement */
var const rotator OldBasedRotation;

/** allows easy modification of the search extent provided by setuppathfindingparams() */
var vector NavMeshPath_SearchExtent_Modifier;

cpptext
{
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );
	virtual void Spawned();
	virtual void BeginDestroy();

	virtual UBOOL IsPlayerOwned()
	{
		return IsPlayerOwner();
	}

	virtual UBOOL IsPlayerOwner()
	{
		return bIsPlayer;
	}
	virtual AController* GetAController() { return this; }

	// Seeing and hearing checks
	virtual UBOOL CanHear(const FVector& NoiseLoc, FLOAT Loudness, AActor *Other);
	virtual void ShowSelf();
	virtual UBOOL ShouldCheckVisibilityOf(AController* C);
	virtual DWORD SeePawn(APawn *Other, UBOOL bMaySkipChecks = TRUE);
	virtual DWORD LineOfSightTo(const AActor* Other, INT bUseLOSFlag=0, const FVector* chkLocation = NULL, UBOOL bTryAlternateTargetLoc = FALSE);
	void CheckEnemyVisible();
	virtual void HearNoise(AActor* NoiseMaker, FLOAT Loudness, FName NoiseType);

	AActor* HandleSpecial(AActor *bestPath);
	virtual INT AcceptNearbyPath(AActor* goal);
	virtual UReachSpec* PrepareForMove( ANavigationPoint *NavGoal, UReachSpec* Path );
	UReachSpec* GetNextRoutePath( ANavigationPoint *NavGoal );
	virtual void AdjustFromWall(FVector HitNormal, AActor* HitActor);
	void SetRouteCache( ANavigationPoint *EndPath, FLOAT StartDist, FLOAT EndDist );
	AActor* FindPath(const FVector& Point, AActor* Goal, UBOOL bWeightDetours, INT MaxPathLength, UBOOL bReturnPartial);
	/** given the passed in goal for pathfinding, set bTransientEndPoint on all NavigationPoints that are acceptable
	 * destinations on the path network
	 * @param EndAnchor the Anchor for the goal on the navigation network
	 * @param Goal the goal actor we're pathfinding toward (may be NULL)
	 * @param GoalLocation the goal world location we're pathfinding toward
	 */
	virtual void MarkEndPoints(ANavigationPoint* EndAnchor, AActor* Goal, const FVector& GoalLocation);
	/** gives the Controller a chance to pre-empt pathfinding with its own result (if a cached path is still valid, for example)
	 * called just before navigation network traversal, after Anchor determination and NavigationPoint transient properties are set up
	 * only called when using the 'FindEndPoint' node evaluator
	 * @param EndAnchor - Anchor for Goal on the path network
	 * @param Goal - Destination Actor we're trying to path to (may be NULL)
	 * @param GoalLocation - the goal world location we're pathfinding toward
	 * @param bWeightDetours - whether we should consider short detours for pickups and such
	 * @param BestWeight - weighting value for best node on path - if this function returns true, findPathToward() will return this value
	 * @return whether the normal pathfinding should be skipped
	 */
	virtual UBOOL OverridePathTo(ANavigationPoint* EndAnchor, AActor* Goal, const FVector& GoalLocation, UBOOL bWeightDetours, FLOAT& BestWeight)
	{
		return FALSE;
	}
	AActor* SetPath(INT bInitialPath=1);
	virtual UBOOL WantsLedgeCheck();
	virtual UBOOL StopAtLedge();
	virtual void PrePollMove()
	{}
	virtual void PostPollMove()
	{}
	virtual void PollMoveComplete();
	virtual AActor* GetViewTarget();
	virtual void UpdateEnemyInfo(APawn* AcquiredEnemy) {};
	virtual void JumpOverWall(FVector WallNormal);
	virtual void UpdatePawnRotation();
	virtual UBOOL ForceReached(ANavigationPoint *Nav, const FVector& TestPosition);
	virtual FRotator SetRotationRate(FLOAT deltaTime);
	virtual FVector DesiredDirection();
	/** activates path lanes for this Controller's current movement and adjusts its destination accordingly
	 * @param DesiredLaneOffset the offset from the center of the Controller's CurrentPath that is desired
	 * 				the Controller sets its LaneOffset as close as it can get to it without
	 *				allowing any part of the Pawn's cylinder outside of the CurrentPath
	 */
	void SetPathLane(FLOAT InPathOffset);
	virtual void FailMove();

	// falling physics AI hooks
	virtual void PreAirSteering(FLOAT DeltaTime) {};
	virtual void PostAirSteering(FLOAT DeltaTime) {};
	virtual void PostPhysFalling(FLOAT DeltaTime) {};
	virtual void PostPhysWalking(FLOAT DeltaTime) {};
	virtual void PostPhysSpider(FLOAT DeltaTime) {};
	virtual UBOOL AirControlFromWall(float DeltaTime, FVector& RealAcceleration) { return FALSE; };
	virtual void NotifyJumpApex();

	virtual void PostBeginPlay();
	virtual void PostScriptDestroyed();

	virtual void ClearCrossLevelPaths(ULevel *Level);

	// Natives.
	DECLARE_FUNCTION(execPollWaitForLanding);
	virtual DECLARE_FUNCTION(execPollMoveTo);
	virtual DECLARE_FUNCTION(execPollMoveToward);
	DECLARE_FUNCTION(execPollFinishRotation);

	virtual UBOOL ShouldOffsetCorners() { return TRUE; }
	virtual UBOOL ShouldUsePathLanes() { return TRUE; }
	virtual UBOOL ShouldIgnoreNavigationBlockingFor(const AActor* Other){ return !Other->bBlocksNavigation; }

	// AnimControl Matinee Track support

	/** Used to provide information on the slots that this Actor provides for animation to Matinee. */
	virtual void GetAnimControlSlotDesc(TArray<struct FAnimSlotDesc>& OutSlotDescs);

	/**
	 *	Called by Matinee when we open it to start controlling animation on this Actor.
	 *	Is also called again when the GroupAnimSets array changes in Matinee, so must support multiple calls.
	 */
	virtual void PreviewBeginAnimControl(class UInterpGroup* InInterpGroup);

	/** Called each frame by Matinee to update the desired sequence by name and position within it. */
	virtual void PreviewSetAnimPosition(FName SlotName, INT ChannelIndex, FName InAnimSeqName, FLOAT InPosition, UBOOL bLooping, UBOOL bFireNotifies, UBOOL bEnableRootMotion, FLOAT DeltaTime);

	/** Called each frame by Matinee to update the desired animation channel weights for this Actor. */
	virtual void PreviewSetAnimWeights(TArray<FAnimSlotInfo>& SlotInfos);

	/** Called by Matinee when we close it after we have been controlling animation on this Actor. */
	virtual void PreviewFinishAnimControl(class UInterpGroup* InInterpGroup);

	/** Function used to control FaceFX animation in the editor (Matinee). */
	virtual void PreviewUpdateFaceFX(UBOOL bForceAnim, const FString& GroupName, const FString& SeqName, FLOAT InPosition);

	/** Used by Matinee playback to start a FaceFX animation playing. */
	virtual void PreviewActorPlayFaceFX(const FString& GroupName, const FString& SeqName, USoundCue* InSoundCue);

	/** Used by Matinee to stop current FaceFX animation playing. */
	virtual void PreviewActorStopFaceFX();

	/** Used in Matinee to get the AudioComponent we should play facial animation audio on. */
	virtual UAudioComponent* PreviewGetFaceFXAudioComponent();

	/** Get the UFaceFXAsset that is currently being used by this Actor when playing facial animations. */
	virtual class UFaceFXAsset* PreviewGetActorFaceFXAsset();

	/** Called each frame by Matinee to update the weight of a particular MorphNodeWeight. */
	virtual void PreviewSetMorphWeight(FName MorphNodeName, FLOAT MorphWeight);

	/** Called each frame by Matinee to update the scaling on a SkelControl. */
	virtual void PreviewSetSkelControlScale(FName SkelControlName, FLOAT Scale);

	/** Called each frame by Matinee to update the controlstrength on a SkelControl. */
	virtual void SetSkelControlStrength(FName SkelControlName, FLOAT ControlStrength);

	/** Called each from while the Matinee action is running, to set the animation weights for the actor. */
	virtual void SetAnimWeights( const TArray<struct FAnimSlotInfo>& SlotInfos );

	/** base function called to kick off moveto latent action (called from execMoveTo and execMoveToDirectNonPathPos) */
	virtual void MoveTo(const FVector& Dest, AActor* ViewFocus, FLOAT DesiredOffset, UBOOL bShouldWalk);

	/** base function called to kick off movetoward latent action (called from execMoveToward) */
	virtual void MoveToward(AActor* goal, AActor* viewfocus, FLOAT DesiredOffset, UBOOL bStrafe, UBOOL bShouldWalk);


	/** IMPLEMENT Interface_NavigationHandle */
	virtual UBOOL	CanCoverSlip(ACoverLink* Link, INT SlotIdx);
	virtual void SetupPathfindingParams( FNavMeshPathParams& out_ParamCache );
	virtual void InitForPathfinding() {}
	virtual INT ExtraEdgeCostToAddWhenActive(FNavMeshEdgeBase* Edge) { return 0; }
	virtual FVector GetEdgeZAdjust(FNavMeshEdgeBase* Edge);
	/** END */

	virtual FLOAT GetMaxDropHeight();
}


replication
{
	if (bNetDirty && Role==ROLE_Authority)
		PlayerReplicationInfo, Pawn;
}

/** returns whether this Controller is a locally controlled PlayerController
 * @note not valid until the Controller is completely spawned (i.e, unusable in Pre/PostBeginPlay())
 */
native function bool IsLocalPlayerController();

/** returns whether this controller is a local controller.
  * @RETURN true always for non-playercontroller
 */
native function bool IsLocalController();

/** Route Cache Operations
 *	Allows operations on nodes in the route while modifying route (ie before emptying the cache)
 *	Should override in subclasses as needed
 */
native function RouteCache_Empty();
native function RouteCache_AddItem( NavigationPoint Nav );
native function RouteCache_InsertItem( NavigationPoint Nav, int Idx=0 );
native function RouteCache_RemoveItem( NavigationPoint Nav );
native function RouteCache_RemoveIndex( int InIndex, int Count=1 );

/** Set FocalPoint as absolute position or offset from base */
native final function SetFocalPoint( Vector FP, optional bool bOffsetFromBase );
/** Retrive the final position that controller should be looking at */
native final function Vector GetFocalPoint();

/** Set Destination as absolute position or offset from base */
native final function SetDestinationPosition( Vector Dest, optional bool bOffsetFromBase );
/** Retrive the final position that controller should be moving to */
native final function Vector GetDestinationPosition();

native final virtual function SetAdjustLocation( Vector NewLoc, bool bAdjust, optional bool bOffsetFromBase );
native final function Vector GetAdjustLocation();

/**
 *  this event is called when an edge is deleted that this controller's handle is actively using
 */
event NotifyPathChanged();


/** Called when we start an AnimControl track operating on this Actor. Supplied is the set of AnimSets we are going to want to play from. */
simulated event BeginAnimControl(InterpGroup InInterpGroup)
{
	Pawn.BeginAnimControl(InInterpGroup);
}

/** Called each from while the Matinee action is running, with the desired sequence name and position we want to be at. */
simulated event SetAnimPosition(name SlotName, int ChannelIndex, name InAnimSeqName, float InPosition, bool bFireNotifies, bool bLooping, bool bEnableRootMotion)
{
	Pawn.SetAnimPosition(SlotName, ChannelIndex, InAnimSeqName, InPosition, bFireNotifies, bLooping, bEnableRootMotion);
}

/** Called when we are done with the AnimControl track. */
simulated event FinishAnimControl(InterpGroup InInterpGroup)
{
	Pawn.FinishAnimControl(InInterpGroup);
}

/**
 * Play FaceFX animations on this Actor.
 * Returns TRUE if succeeded, if failed, a log warning will be issued.
 */
event bool PlayActorFaceFXAnim(FaceFXAnimSet AnimSet, String GroupName, String SeqName, SoundCue SoundCueToPlay )
{
	return Pawn.PlayActorFaceFXAnim(AnimSet, GroupName, SeqName, SoundCueToPlay);
}

/** Stop any matinee FaceFX animations on this Actor. */
event StopActorFaceFXAnim()
{
	Pawn.StopActorFaceFXAnim();
}

/** Called each frame by Matinee to update the weight of a particular MorphNodeWeight. */
event SetMorphWeight(name MorphNodeName, float MorphWeight)
{
	Pawn.SetMorphweight(MorphNodeName, MorphWeight);
}

/** Called each frame by Matinee to update the scaling on a SkelControl. */
event SetSkelControlScale(name SkelControlName, float Scale)
{
	Pawn.SetSkelControlScale(SkelControlName, Scale);
}
/* epic ===============================================
* ::PostBeginPlay
*
* Overridden to create the player replication info and
* perform other mundane initialization tasks.
*
* =====================================================
*/
event PostBeginPlay()
{
	Super.PostBeginPlay();

	if ( !bDeleteMe && (WorldInfo.NetMode != NM_Client) )
	{
		if( bIsPlayer )
		{
			// create a new player replication info
			InitPlayerReplicationInfo();
		}
		InitNavigationHandle();
	}
	// randomly offset the sight counter to avoid hitches
	SightCounter = SightCounterInterval * FRand();
}

/* epic ===============================================
* ::Reset
*
* Resets various properties to their default state, used
* for round resetting, etc.
*
* =====================================================
*/
function Reset()
{
	super.Reset();
	Enemy			= None;
	StartSpot		= None;
	bAdjusting		= false;
	bPreparingMove	= false;
	MoveTimer		= -1;
	MoveTarget		= None;
	CurrentPath		= None;
	RouteGoal		= None;
}

/* epic ===============================================
* ::ClientSetLocation
*
* Replicated function to set the pawn location and
* rotation, allowing server to force (ex. teleports).
*
* =====================================================
*/
reliable client function ClientSetLocation( vector NewLocation, rotator NewRotation )
{
	SetRotation(NewRotation);
	if ( Pawn != None )
	{
		if ( (Rotation.Pitch > Pawn.MaxPitchLimit)
			&& (Rotation.Pitch < 65536 - Pawn.MaxPitchLimit) )
		{
			If (Rotation.Pitch < 32768)
				NewRotation.Pitch = Pawn.MaxPitchLimit;
			else
				NewRotation.Pitch = 65536 - Pawn.MaxPitchLimit;
		}
		NewRotation.Roll  = 0;
		Pawn.SetRotation( NewRotation );
		Pawn.SetLocation( NewLocation );
	}
}

/* epic ===============================================
* ::ClientSetRotation
*
* Replicated function to set the pawn rotation, allowing
* the server to force.
*
* =====================================================
*/
reliable client function ClientSetRotation( rotator NewRotation, optional bool bResetCamera )
{
	SetRotation(NewRotation);
	if ( Pawn != None )
	{
		NewRotation.Pitch = 0;
		NewRotation.Roll  = 0;
		Pawn.SetRotation( NewRotation );
	}
}

/* epic ===============================================
* ::ReplicatedEvent
*
* Called when a variable with the property flag "RepNotify" is replicated
*
* =====================================================
*/
simulated event ReplicatedEvent(name VarName)
{
	if (VarName == 'PlayerReplicationInfo')
	{
		if (PlayerReplicationInfo != None)
		{
			PlayerReplicationInfo.ClientInitialize(self);
		}
	}
	else
	{
		Super.ReplicatedEvent(VarName);
	}
}

/** Kismet Action to possess a Pawn or a vehicle */
function OnPossess(SeqAct_Possess inAction)
{
	local Pawn		OldPawn;
	local Vehicle	V;

	// if we're driving a vehicle, and we should try to get out first.
	V = Vehicle(Pawn);
	if( inAction.bTryToLeaveVehicle &&
		V != None )
	{
		V.DriverLeave( TRUE );
	}

	if( inAction.PawnToPossess != None )
	{
		V = Vehicle(inAction.PawnToPossess);
		if( Pawn!= None && V != None )
		{
			V.TryToDrive( Pawn );
		}
		else
		{
			OldPawn = Pawn;
			UnPossess();
			Possess( inAction.PawnToPossess, FALSE );

			if( inAction.bKillOldPawn && OldPawn != None )
			{
				OldPawn.Destroy();
			}
		}
	}
}


/* epic ===============================================
* ::Possess
*
* Handles attaching this controller to the specified
* pawn.
*
* =====================================================
*/
event Possess(Pawn inPawn, bool bVehicleTransition)
{
	if (inPawn.Controller != None)
	{
		inPawn.Controller.UnPossess();
	}

	inPawn.PossessedBy(self, bVehicleTransition);
	Pawn = inPawn;

	// preserve Pawn's rotation initially for placed Pawns
	SetFocalPoint( Pawn.Location + 512*vector(Pawn.Rotation), TRUE );
	Restart(bVehicleTransition);

	if( Pawn.Weapon == None )
	{
		ClientSwitchToBestWeapon();
	}
}

/* epic ===============================================
* ::UnPossess
*
* Called to unpossess our pawn for any reason that is not death
* (death handled by PawnDied())
*
* =====================================================
*/
event UnPossess()
{
    if ( Pawn != None )
	{
		Pawn.UnPossessed();
		Pawn = None;
	}
}

/* epic ===============================================
* ::PawnDied
*
* Called to unpossess our pawn because it has died
* (other unpossession handled by UnPossess())
*
* =====================================================
*/
function PawnDied(Pawn inPawn)
{
	local int idx;

	if ( inPawn != Pawn )
	{	// if that's not our current pawn, leave
		return;
	}

	// abort any latent actions
	TriggerEventClass(class'SeqEvent_Death',self);
	for (idx = 0; idx < LatentActions.Length; idx++)
	{
		if (LatentActions[idx] != None)
		{
			LatentActions[idx].AbortFor(self);
		}
	}
	LatentActions.Length = 0;

	if ( Pawn != None )
	{
		SetLocation(Pawn.Location);
		Pawn.UnPossessed();
	}
	Pawn = None;

	// if we are a player, transition to the dead state
	if ( bIsPlayer )
	{
		// only if the game hasn't ended,
		if ( !GamePlayEndedState() )
		{
			// so that we can respawn
			GotoState('Dead');
		}
	}
	// otherwise destroy this controller
	else
	{
		Destroy();
	}
}

function bool GamePlayEndedState()
{
	return false;
}

/* epic ===============================================
* ::NotifyPostLanded
*
* Called after pawn lands after falling if bNotifyPostLanded is true
*
* =====================================================
*/
event NotifyPostLanded();

/* epic ===============================================
* ::Destroyed
*
* Called once this controller has been deleted, overridden
* to cleanup any lingering references, etc.
*
* =====================================================
*/
event Destroyed()
{
	if (Role == ROLE_Authority)
	{
		// if we are a player, log out
		if ( bIsPlayer && (WorldInfo.Game != None) )
		{
			WorldInfo.Game.logout(self);
		}
		if ( PlayerReplicationInfo != None )
		{
			// remove from team if applicable
			if ( !PlayerReplicationInfo.bOnlySpectator &&
				PlayerReplicationInfo.Team != None )
			{
				PlayerReplicationInfo.Team.RemoveFromTeam(self);
			}
			CleanupPRI();
		}
	}
	Super.Destroyed();
}

/* epic ===============================================
* ::CleanupPRI
*
* Called from Destroyed().  Cleans up PlayerReplicationInfo.
*
* =====================================================
*/
function CleanupPRI()
{
	PlayerReplicationInfo.Destroy();
	PlayerReplicationInfo = None;
}

/* epic ===============================================
* ::Restart
*
* Called upon possessing a new pawn, perform any specific
* cleanup/initialization here.
*
* =====================================================
*/
function Restart(bool bVehicleTransition)
{
	Pawn.Restart();
	if ( !bVehicleTransition )
	{
		Enemy = None;
	}

	// if not vehicle transition, clear controller information
	if ( bVehicleTransition == FALSE && Pawn.InvManager != None )
	{
		Pawn.InvManager.UpdateController();
	}
}

/* epic ===============================================
* ::BeyondFogDistance
*
* Returns true if OtherPoint is occluded by fog when viewed from ViewPoint.
*
* =====================================================
*/
final native function bool BeyondFogDistance(vector ViewPoint, vector OtherPoint);

/* epic ===============================================
* ::EnemyJustTeleported
*
* Notification that Enemy just went through a teleporter.
*
* =====================================================
*/
function EnemyJustTeleported()
{
	LineOfSightTo(Enemy);
}

/* epic ===============================================
* ::NotifyTakeHit
*
* Notification from pawn that it has received damage
* via TakeDamage().
*
* =====================================================
*/
function NotifyTakeHit(Controller InstigatedBy, vector HitLocation, int Damage, class<DamageType> damageType, vector Momentum);

/** spawns and initializes the PlayerReplicationInfo for this Controller */
function InitPlayerReplicationInfo()
{
	PlayerReplicationInfo = Spawn(WorldInfo.Game.PlayerReplicationInfoClass, self,, vect(0,0,0),rot(0,0,0));
	// force a default player name if necessary
	if (PlayerReplicationInfo.PlayerName == "")
	{
		// don't call SetPlayerName() as that will broadcast entry messages but the GameInfo hasn't had a chance
		// to potentionally apply a player/bot name yet
		PlayerReplicationInfo.PlayerName = class'GameInfo'.default.DefaultPlayerName;
	}
}

/*
 * Queries the PRI and returns our current team index.
 */
simulated native function byte GetTeamNum();

/* epic ===============================================
* ::ServerRestartPlayer
*
* Attempts to restart this player, generally called from
* the client upon respawn request.
*
* =====================================================
*/
reliable server function ServerRestartPlayer()
{
	if (WorldInfo.NetMode != NM_Client &&
		Pawn != None)
	{
		ServerGivePawn();
	}
}

/* epic ===============================================
* ::ServerGivePawn
*
* Requests a pawn from the server for this controller,
* as part of the respawn process.
*
* =====================================================
*/
function ServerGivePawn();

/* epic ===============================================
* ::SetCharacter
*
* Sets the character for this controller for future
* pawn spawns.
*
* =====================================================
*/
function SetCharacter(string inCharacter);

/* epic ===============================================
* ::GameHasEnded
*
* Called from game info upon end of the game, used to
* transition to proper state.
*
* =====================================================
*/
function GameHasEnded(optional Actor EndGameFocus, optional bool bIsWinner)
{
	// and transition to the game ended state
	GotoState('RoundEnded');
}

/* epic ===============================================
* ::NotifyKilled
*
* Notification from game that a pawn has been killed.
*
* =====================================================
*/
function NotifyKilled(Controller Killer, Controller Killed, pawn KilledPawn, class<DamageType> damageTyp)
{
	if( Pawn != None )
	{
		Pawn.TriggerEventClass( class'SeqEvent_SeeDeath', KilledPawn );
	}

	if (Enemy == KilledPawn)
	{
		Enemy = None;
	}
}

function NotifyProjLanded( Projectile Proj )
{
	if( Proj != None && Pawn != None )
	{
		Pawn.TriggerEventClass( class'SeqEvent_ProjectileLanded', Proj );
	}
}

/**
 *	Notification from given projectil that it is about to explode
 */
function WarnProjExplode( Projectile Proj );

//=============================================================================
// INVENTORY FUNCTIONS

/* epic ===============================================
* ::RatePickup
*
* Callback from PickupFactory that allows players to
* additionally weight certain pickups.
*
* =====================================================
*/
event float RatePickup(Actor PickupHolder, class<Inventory> inPickup);

/* epic ===============================================
* ::FireWeaponAt
*
* Should cause this player to fire a weapon at the given
* actor.
*
* =====================================================
*/
function bool FireWeaponAt(Actor inActor);

/* epic ===============================================
* ::StopFiring
*
* Stop firing of our current weapon.
*
* =====================================================
*/
event StopFiring()
{
	bFire = 0;
	if ( Pawn != None )
		Pawn.StopFiring();
}

/* epic ===============================================
* ::RoundHasEnded
*
*
* =====================================================
*/
function RoundHasEnded(optional Actor EndRoundFocus)
{
	GotoState('RoundEnded');
}

/* epic ===============================================
* ::HandlePickup
*
* Called whenever our pawn runs over a new pickup.
*
* =====================================================
*/
function HandlePickup(Inventory Inv);

/**
 * Adjusts weapon aiming direction.
 * Gives controller a chance to modify the aiming of the pawn. For example aim error, auto aiming, adhesion, AI help...
 * Requested by weapon prior to firing.
*
 * @param	W, weapon about to fire
 * @param	StartFireLoc, world location of weapon fire start trace, or projectile spawn loc.
 */
function Rotator GetAdjustedAimFor( Weapon W, vector StartFireLoc )
{
	// by default, return Rotation. This is the standard aim for controllers
	// see implementation for PlayerController.
	if ( Pawn != None )
	{
		return Pawn.GetBaseAimRotation();
	}
	return Rotation;
}

/* epic ===============================================
* ::InstantWarnTarget
*
* Warn a target it is about to be shot at with an instant hit
* weapon.
*
* =====================================================
*/
function InstantWarnTarget(Actor InTarget, Weapon FiredWeapon, vector FireDir)
{
	local Pawn P;

	P = Pawn(InTarget);
	if (P != None && P.Controller != None)
	{
		P.Controller.ReceiveWarning(Pawn, -1, FireDir);
	}
}

/* epic ===============================================
* ::ReceiveWarning
*
* Notification that the pawn is about to be shot by a
* trace hit weapon.
*
* =====================================================
*/
function ReceiveWarning(Pawn shooter, float projSpeed, vector FireDir);

/* epic ===============================================
* ::ReceiveProjectileWarning
*
* Notification that the pawn is about to be shot by a
* projectile.
*
* =====================================================
*/
function ReceiveProjectileWarning(Projectile Proj);

/* epic ===============================================
* ::SwitchToBestWeapon
*
* Rates the pawn's weapon loadout and chooses the best
* weapon, bringing it up as the active weapon.
*
* =====================================================
*/

exec function SwitchToBestWeapon(optional bool bForceNewWeapon)
{
	if ( Pawn == None || Pawn.InvManager == None )
		return;

	Pawn.InvManager.SwitchToBestWeapon(bForceNewWeapon);
}

/* epic ===============================================
* ::ClientSwitchToBestWeapon
*
* Forces the client to switch to a new weapon, allowing
* the server control.
*
* =====================================================
*/
reliable client function ClientSwitchToBestWeapon(optional bool bForceNewWeapon)
{
    SwitchToBestWeapon(bForceNewWeapon);
}

/* epic ===============================================
* ::NotifyChangedWeapon
*
* Notification from pawn that the current weapon has
* changed.
* Network: LocalPlayer
* =====================================================
*/
function NotifyChangedWeapon( Weapon PrevWeapon, Weapon NewWeapon );

//=============================================================================
// AI FUNCTIONS

/* epic ===============================================
* ::LineOfSightTo
*
* Returns true if the specified actor has line of sight
* to our pawn.
*
* NOTE: No FOV is accounted for, use CanSee().
*
* =====================================================
*/
native(514) noexport final function bool LineOfSightTo(Actor Other, optional vector chkLocation, optional bool bTryAlternateTargetLoc);

/* epic ===============================================
* ::CanSee
*
* Returns true if the specified pawn is visible within
* our peripheral vision.  If the pawn is not our current
* enemy then LineOfSightTo() will be called instead.
*
* =====================================================
*/
native(533) final function bool CanSee(Pawn Other);

/* epic ===============================================
* ::CanSee
*
* Returns true if the test location is visible within
* our peripheral vision (from view location).
*
* =====================================================
*/
native(537) final function bool CanSeeByPoints( Vector ViewLocation, Vector TestLocation, Rotator ViewRotation );

/* epic ===============================================
* ::PickTarget
*
* Evaluates pawns in the local area and returns the
* one that is closest to the specified FireDir.
*
* =====================================================
*/
native(531) final function Pawn PickTarget(class<Pawn> TargetClass, out float bestAim, out float bestDist, vector FireDir, vector projStart, float MaxRange);

/* epic ===============================================
* ::HearNoise
*
* Counterpart to the Actor::MakeNoise() function, called
* whenever this player is within range of a given noise.
* Used as AI audio cues, instead of processing actual
* sounds.
*
* =====================================================
*/
event HearNoise( float Loudness, Actor NoiseMaker, optional Name NoiseType );

/* epic ===============================================
* ::SeePlayer
*
* Called whenever Seen is within of our line of sight
* if Seen.bIsPlayer==true.
*
* =====================================================
*/
event SeePlayer( Pawn Seen );

/* epic ===============================================
* ::SeeMonster
*
* Called whenever Seen is within of our line of sight
* if Seen.bIsPlayer==false.
*
* =====================================================
*/
event SeeMonster( Pawn Seen );

/* epic ===============================================
* ::EnemyNotVisible
*
* Called whenever Enemy is no longer within of our line
* of sight.
*
* =====================================================
*/
event EnemyNotVisible();

//=============================================================================
// NAVIGATION FUNCTIONS

/* epic ===============================================
* ::MoveTo
*
* Latently moves our pawn to the desired location, which
* is cached in Destination.
*
* =====================================================
*/
native(500) noexport final latent function MoveTo(vector NewDestination, optional Actor ViewFocus, optional float DestinationOffset, optional bool bShouldWalk = (Pawn != None) ? Pawn.bIsWalking : false);

/* epic ===============================================
* ::MoveToDirectNonPathPos
*
* Latently moves our pawn to the desired location, which
* is cached in Destination.
* NOTE: this function should be used only when moving directly to a final goal (e.g. not following a path)
* it will properly set the final destination on the navhandle (and ensure things are cleared out which would normally be set by GetNextMoveLocation)
* @Param NewDestination - destination to move to
* @param ViewFocus      - actor to look at while moving
* @param DestinationOffset - distance from goal we would like to get within
* @param bShouldWalk    - should the AI walk for this move?
* =====================================================
*/
native noexport final latent function MoveToDirectNonPathPos(vector NewDestination, optional Actor ViewFocus, optional float DestinationOffset, optional bool bShouldWalk = (Pawn != None) ? Pawn.bIsWalking : false);

/* epic ===============================================
* ::MoveToward
*
* Latently moves our pawn to the desired actor, which
* is cached in MoveTarget.  Takes advantage of the
* navigation network anchors when moving to other Pawn
* and Inventory actors.
*
* =====================================================
*/
native(502) noexport final latent function MoveToward(Actor NewTarget, optional Actor ViewFocus, optional float DestinationOffset, optional bool bUseStrafing, optional bool bShouldWalk = (Pawn != None) ? Pawn.bIsWalking : false);

/* epic ===============================================
* ::SetupSpecialPathAbilities
*
* Called before path finding to allow setup of transient
* navigation flags.
*
* =====================================================
*/
event SetupSpecialPathAbilities();

/* epic ===============================================
* ::FinishRotation
*
* Latently waits for our pawn's rotation to match the
* pawn's DesiredRotation.
*
* =====================================================
*/
native(508) final latent function FinishRotation();

/* epic ===============================================
* ::FindPathTo
*
* Searches the navigation network for a path to the
* node closest to the given point.
*
* =====================================================
*/
native(518) final function Actor FindPathTo( Vector aPoint, optional int MaxPathLength, optional bool bReturnPartial );

/* epic ===============================================
* ::FindPathToward
*
* Searches the navigation network for a path to the
* node closest to the given actor.
*
* =====================================================
*/
native(517) final function Actor FindPathToward( Actor anActor, optional bool bWeightDetours, optional int MaxPathLength, optional bool bReturnPartial );

/* epic ===============================================
* ::FindPathTowardNearest
*
* Searches the navigation network for a path to the
* closest node of the specified type.
*
* =====================================================
*/
native final function Actor FindPathTowardNearest(class<NavigationPoint> GoalClass, optional bool bWeightDetours, optional int MaxPathLength, optional bool bReturnPartial );

/* epic ===============================================
* ::FindRandomDest
*
* Returns a random node on the network.
*
* =====================================================
*/
native(525) final function NavigationPoint FindRandomDest();

native final function Actor FindPathToIntercept(Pawn P, Actor InRouteGoal, optional bool bWeightDetours, optional int MaxPathLength, optional bool bReturnPartial );

/* epic ===============================================
* ::PointReachable
*
* Returns true if the given point is directly reachable
* given our pawn's current movement capabilities.
*
* NOTE: This function is potentially expensive and should
* be used sparingly.  If at all possible, use ActorReachable()
* instead, since that has more possible optimizations
* over PointReachable().
*
* =====================================================
*/
native(521) final function bool PointReachable(vector aPoint);

/* epic ===============================================
* ::ActorReachable
*
* Returns true if the given actor is directly reachable
* given our pawn's current movement capabilities.  Takes
* advantage of the navigation network anchors when
* possible.
*
* NOTE: This function is potentially expensive and should
* be used sparingly.
*
* =====================================================
*/
native(520) final function bool ActorReachable(actor anActor);

/**
 * Called by APawn::moveToward when the point is unreachable
 * due to obstruction or height differences.
 */
event MoveUnreachable(vector AttemptedDest, Actor AttemptedTarget)
{
}

/* epic ===============================================
* ::PickWallAdjust
*
* Checks if we could jump over obstruction (if within
* knee height), or attempts to move to either side of
* the obstruction.
*
* =====================================================
*/
native(526) final function bool PickWallAdjust(vector HitNormal);

/* epic ===============================================
* ::WaitForLanding
*
* Latently waits until the pawn has landed.  Only valid
* with PHYS_Falling.  Optionally specify the max amount
* of time to wait, which defaults to 4 seconds.
*
* NOTE: If the pawn hasn't landed before the duration
* is exceeded, it will abort and call LongFall().
*
* =====================================================
*/
native(527) noexport final latent function WaitForLanding(optional float waitDuration);

/* epic ===============================================
* ::LongFall
*
* Called once the duration for WaitForLanding has been
* exceeded without hitting ground.
*
* =====================================================
*/
event LongFall();

/* epic ===============================================
* ::EndClimbLadder
*
* Aborts a latent move action if the MoveTarget is
* currently a ladder.
*
* =====================================================
*/
native function EndClimbLadder();

/* epic ===============================================
* ::MayFall
*
* Called when a pawn is about to fall off a ledge, and
* allows the controller to prevent a fall by toggling
* bCanJump.
* This event is also passed if a floor is found over the edge, and the normal of the floor if so.
*
* =====================================================
*/
event MayFall(bool bFloor, vector FloorNormal);

/* epic ===============================================
* ::AllowDetourTo
*
* Return true to allow a detour to a given point, or
* false to avoid it.  Called during any sort of path
* finding (FindPathXXX() functions).
*
* =====================================================
*/
event bool AllowDetourTo(NavigationPoint N)
{
	return true;
}

/* epic ===============================================
* ::WaitForMover
*
* Used to notify AI controller that it needs to wait
* at its current position for an interpActor to
* become properly positioned.
*
* =====================================================
*/
function WaitForMover(InterpActor M)
{
	PendingMover = M;
	M.bMonitorMover = true;
	bPreparingMove = true;
	Pawn.Acceleration = vect(0,0,0);
}

/* epic ===============================================
* ::MoverFinished
*
* Called by InterpActor when it stops
* if this controller has it set as its PendingMover
*
* =====================================================
*/
event bool MoverFinished()
{
	if (Pawn == None || PendingMover.MyMarker == None || PendingMover.MyMarker.ProceedWithMove(Pawn))
	{
		PendingMover = None;
		bPreparingMove = false;
		return true;
	}
	return false;
}

/** Called when the this Controller's Pawn gets hit by an InterpActor interpolating downward while this Controller has Lift
 *	set as its PendingMover
 *	@param Lift the LiftCenter associated with the InterpActor that hit the Pawn
 */
function UnderLift(LiftCenter Lift);

/** called when a ReachSpec the AI wants to use is blocked by a dynamic obstruction
 * gives the AI an opportunity to do something to get rid of it instead of trying to find another path
 * @note MoveTarget is the actor the AI wants to move toward, CurrentPath the ReachSpec it wants to use
 * @param BlockedBy the object blocking the path
 * @return true if the AI did something about the obstruction and should use the path anyway, false if the path
 * is unusable and the bot must find some other way to go
 */
event bool HandlePathObstruction(Actor BlockedBy);

//=============================================================================
// CAMERA FUNCTIONS

/**
 * Returns Player's Point of View
 * For the AI this means the Pawn's 'Eyes' ViewPoint
 * For a Human player, this means the Camera's ViewPoint
*
 * @output	out_Location, view location of player
 * @output	out_rotation, view rotation of player
*/
simulated event GetPlayerViewPoint( out vector out_Location, out Rotator out_rotation )
{
	out_Location = Location;
	out_Rotation = Rotation;
}

/**
 * returns the point of view of the actor.
 * note that this doesn't mean the camera, but the 'eyes' of the actor.
 * For example, for a Pawn, this would define the eye height location,
 * and view rotation (which is different from the pawn rotation which has a zeroed pitch component).
 * A camera first person view will typically use this view point. Most traces (weapon, AI) will be done from this view point.
*
 * @output	out_Location, location of view point
 * @output	out_Rotation, view rotation of actor.
*/
simulated event GetActorEyesViewPoint( out vector out_Location, out Rotator out_Rotation )
{
	// If we have a Pawn, this is our view point.
	if ( Pawn != None )
	{
		Pawn.GetActorEyesViewPoint( out_Location, out_Rotation );
	}
	else
	{	// Otherwise controller location/rotation is our view point.
		out_Location = Location;
		out_Rotation = Rotation;
	}
}

/**
 * This will return whether or not this controller is aiming at the passed in actor.
 * We are defining AIMing to mean that you are attempting to target the actor.  Not just looking in the
 * direction of the actor.
 *
 **/
simulated function bool IsAimingAt( Actor ATarget, float Epsilon )
{
	local Vector Loc;
	local Rotator Rot;

	// grab camera location/rotation for checking Dist
	GetPlayerViewPoint( Loc, Rot );

	// a decent epsilon value is 0.98f as that allows a for a little bit of slop
	// NOTE: if you want to aim DIRECTLY at something then you would want ~= 1.0f
	return ( (Normal(ATarget.Location - Loc) dot vector(Rot)) >= Epsilon );
}

/** LandingShake()
returns true if controller wants landing view shake
*/
simulated function bool LandingShake()
{
	return false;
}

//=============================================================================
// PHYSICS FUNCTIONS

/* epic ===============================================
* ::NotifyPhysicsVolumeChange
*
* Called when our pawn enters a new physics volume.
*
* =====================================================
*/
event NotifyPhysicsVolumeChange(PhysicsVolume NewVolume);

/* epic ===============================================
* ::NotifyHeadVolumeChange
*
* Called when our pawn's head enters a new physics
* volume.
* return true to prevent HeadVolumeChange() notification on the pawn.
*
* =====================================================
*/
event bool NotifyHeadVolumeChange(PhysicsVolume NewVolume);

/* epic ===============================================
* ::NotifyLanded
*
* Called when our pawn has landed from a fall, return
* true to prevent Landed() notification on the pawn.
*
* =====================================================
*/
event bool NotifyLanded(vector HitNormal, Actor FloorActor);

/* epic ===============================================
* ::NotifyHitWall
*
* Called when our pawn has collided with a blocking
* piece of world geometry, return true to prevent
* HitWall() notification on the pawn.
*
* =====================================================
*/
event bool NotifyHitWall(vector HitNormal, actor Wall);

/* epic ===============================================
* ::NotifyFallingHitWall
*
* Called when our pawn has collided with a blocking
* piece of world geometry while falling, only called if
* bNotifyFallingHitWall is true
* =====================================================
*/
event NotifyFallingHitWall(vector HitNormal, actor Wall);

/* epic ===============================================
* ::NotifyBump
*
* Called when our pawn has collided with a blocking player,
* return true to prevent Bump() notification on the pawn.
*
* =====================================================
*/
event bool NotifyBump(Actor Other, Vector HitNormal);

/* epic ===============================================
* ::NotifyJumpApex
*
* Called when our pawn has reached the apex of a jump.
*
* =====================================================
*/
event NotifyJumpApex();

/* epic ===============================================
* ::NotifyMissedJump
*
* Called when our pawn misses a jump while performing
* latent movement (ie MoveToward()).
*
* =====================================================
*/
event NotifyMissedJump();

/** Called when our pawn reaches Controller.Destination after setting bPreciseDestination = TRUE */
event ReachedPreciseDestination();

//=============================================================================
// MISC FUNCTIONS

/* epic ===============================================
* ::InLatentExecution
*
* Returns true if currently in the specified latent
* action.
*
* =====================================================
*/
native final function bool InLatentExecution(int LatentActionNumber);

/* epic ===============================================
* ::StopLatentExecution
*
* Stops any active latent action.
*
* =====================================================
*/
native final function StopLatentExecution();

/**
 * list important Controller variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local Canvas Canvas;

	Canvas = HUD.Canvas;

	if ( Pawn == None )
	{
		if ( PlayerReplicationInfo == None )
		{
			Canvas.DrawText("NO PLAYERREPLICATIONINFO", false);
		}
		else
		{
			PlayerReplicationInfo.DisplayDebug(HUD,out_YL,out_YPos);
		}
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);

		super.DisplayDebug(HUD,out_YL,out_YPos);
		return;
	}

	Canvas.SetDrawColor(255,0,0);
	Canvas.DrawText("CONTROLLER "$GetItemName(string(Self))$" Pawn "$GetItemName(string(Pawn)));
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);

	Canvas.DrawText(" bPreciseDestination:" @ bPreciseDestination);
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);

	if (HUD.ShouldDisplayDebug('AI'))
	{
		if ( Enemy != None )
		{
			Canvas.DrawText("     STATE: "$GetStateName()$" Enemy "$Enemy.GetHumanReadableName(), false);
		}
		else
		{
			Canvas.DrawText("     STATE: "$GetStateName()$" NO Enemy ", false);
		}
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
	}
}

/* epic ===============================================
* ::GetHumanReadableName
*
* Returns the PRI player name if available,	or of this
* controller object.
*
* =====================================================
*/
simulated function String GetHumanReadableName()
{
	if ( PlayerReplicationInfo != None )
	{
		return PlayerReplicationInfo.PlayerName;
	}
	else
	{
		return GetItemName(String(self));
	}
}

//=============================================================================
// CONTROLLER STATES

function bool IsDead();

/* epic ===============================================
* state Dead
*
* Base state entered upon pawn death, if bIsPlayer is
* set to true.
*
* =====================================================
*/
State Dead
{
ignores SeePlayer, HearNoise, KilledBy;

	function bool IsDead() { return TRUE; }

	function PawnDied(Pawn P)
	{
		if ( WorldInfo.NetMode != NM_Client )
		{
			`warn( Self @ "Pawndied while dead" );
			ScriptTrace();
		}
	}

	/* epic ===============================================
	* ::ServerRestartPlayer
	*
	* Attempts to restart the player if not a client.
	*
	* =====================================================
	*/
	reliable server function ServerReStartPlayer()
	{
		if ( WorldInfo.NetMode == NM_Client )
			return;

		// If we're still attached to a Pawn, leave it
		if ( Pawn != None )
		{
			UnPossess();
		}

		WorldInfo.Game.RestartPlayer( Self );
	}
}

/* epic ===============================================
* state RoundEnded
*
* Base state entered upon the end of a game round, instigated
* by the
*
* =====================================================
*/
state RoundEnded
{
ignores SeePlayer, HearNoise, KilledBy, NotifyBump, HitWall, NotifyPhysicsVolumeChange, NotifyHeadVolumeChange, Falling, TakeDamage, ReceiveWarning;

	function bool GamePlayEndedState()
	{
		return true;
	}

	event BeginState(Name PreviousStateName)
	{
		// if we still have a valid pawn,
		if ( Pawn != None )
		{
			// stop it in midair and detach
			Pawn.TurnOff();
			StopFiring();
			if (!bIsPlayer)
			{
				Pawn.UnPossessed();
				Pawn = None;
			}
		}
		if ( !bIsPlayer )
		{
			Destroy();
		}
	}
}

/**
 * Overridden to redirect to pawn, since teleporting the controller
 * would be useless.
 * If Action == None, this was called from the Pawn already
 */
simulated function OnTeleport(SeqAct_Teleport Action)
{
	if( Action != None )
	{
		if (Pawn != None)
		{
			Pawn.OnTeleport(Action);
		}
		else
		{
			Super.OnTeleport(Action);
		}
	}
}

function OnAttachToActor(SeqAct_AttachToActor Action)
{
	if (Pawn != None)
	{
		Pawn.OnAttachToActor(Action);
	}
	else
	{
		Super.OnAttachToActor(Action);
	}
}

/**
 * Sets god mode based on the activated link.
 */
function OnToggleGodMode(SeqAct_ToggleGodMode inAction)
{
	if (inAction.InputLinks[0].bHasImpulse)
	{
		bGodMode = true;
	}
	else if (inAction.InputLinks[1].bHasImpulse)
	{
		bGodMode = false;
	}
	else
	{
		bGodMode = !bGodMode;
	}
}

/** Redirects SetPhysics kismet action to the pawn */
simulated function OnSetPhysics(SeqAct_SetPhysics Action)
{
	if( Pawn != None )
	{
		Pawn.OnSetPhysics(Action);
	}
	else
	{
		Super.OnSetPhysics(Action);
	}
}

/** Redirects SetVelocity kismet action to the pawn */
simulated function OnSetVelocity( SeqAct_SetVelocity Action )
{
	if( Pawn != None )
	{
		Pawn.OnSetVelocity(Action);
	}
	else
	{
		Super.OnSetVelocity(Action);
	}
}



/**
 * Called when a slot is disabled with this controller as the current owner.
 */
simulated function NotifyCoverDisabled( CoverLink Link, int SlotIdx, optional bool bAdjacentIdx );

/**
 * Called when a slot is adjusted with this controller as the current owner.
 */
simulated event NotifyCoverAdjusted()
{
	// do nothing.  intended to be overloaded, but needs a body because it's an event.
}

/**
 *	Called when cover that AI had claimed is claimed forceably by someone else (usually a player)
 */
simulated function bool NotifyCoverClaimViolation( Controller NewClaim, CoverLink Link, int SlotIdx );

/**
* Redirect to pawn.
*/
simulated function OnModifyHealth(SeqAct_ModifyHealth Action)
{
	if (Pawn != None)
	{
		Pawn.OnModifyHealth(Action);
	}
}

/**
 * Called when an inventory item is given to our Pawn
 * (owning client only)
 * @param NewItem the Inventory item that was added
 */
function NotifyAddInventory(Inventory NewItem);

/**
 * Overridden to redirect to the pawn if available.
 */
simulated function OnToggleHidden(SeqAct_ToggleHidden Action)
{
	if (Pawn != None)
	{
		Pawn.OnToggleHidden(Action);
	}
}

/** Returns true if controller is spectating */
event bool IsSpectating()
{
	return false;
}

/** Returns if controller is in combat */
event bool IsInCombat( optional bool bForceCheck );

/**
 * Called when the level this controller is in is unloaded via streaming.
 */
event CurrentLevelUnloaded();

function SendMessage(PlayerReplicationInfo Recipient, name MessageType, float Wait, optional class<DamageType> DamageType);

function ReadyForLift();

/** spawn and init Navigation Handle */
simulated function InitNavigationHandle()
{
	if( NavigationHandleClass != None )
	{
		NavigationHandle = new(self) NavigationHandleClass;
	}
}

simulated event InterpolationStarted(SeqAct_Interp InterpAction, InterpGroupInst GroupInst)
{
	if (Pawn!=none)
	{
		Pawn.InterpolationStarted(InterpAction, GroupInst);
	}

	Super.InterpolationStarted( InterpAction, GroupInst );
}

/** called when a SeqAct_Interp action finished interpolating this Actor
 * @note this function is called on clients for actors that are interpolated clientside via MatineeActor
 * @param InterpAction the SeqAct_Interp that was affecting the Actor
 */
simulated event InterpolationFinished(SeqAct_Interp InterpAction)
{
	if (Pawn!=none)
	{
		Pawn.InterpolationFinished(InterpAction);
	}

	Super.InterpolationFinished( InterpAction );
}

event bool GeneratePathToActor( Actor Goal, optional float WithinDistance, optional bool bAllowPartialPath );
event bool GeneratePathToLocation( Vector Goal, optional float WithinDistance, optional bool bAllowPartialPath );

defaultproperties
{
	RotationRate=(Pitch=30000,Yaw=30000,Roll=2048)
	bHidden=TRUE
	bHiddenEd=TRUE
	MinHitWall=-1.f
	bSlowerZAcquire=TRUE
	RemoteRole=ROLE_None
	bOnlyRelevantToOwner=TRUE

	SightCounterInterval=0.2
	MaxMoveTowardPawnTargetTime=1.2

	NavigationHandleClass=class'NavigationHandle'
}
