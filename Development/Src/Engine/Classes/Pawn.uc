//=============================================================================
// Pawn, the base class of all actors that can be controlled by players or AI.
//
// Pawns are the physical representations of players and creatures in a level.
// Pawns have a mesh, collision, and physics.  Pawns can take damage, make sounds,
// and hold weapons and other inventory.  In short, they are responsible for all
// physical interaction between the player or AI and the world.
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class Pawn extends Actor
	abstract
	native(Pawn)
	placeable
	config(Game)
	dependson(Controller)
	nativereplication
	implements(Interface_Speaker);

var const float			MaxStepHeight,
						MaxJumpHeight;
var const float			WalkableFloorZ;		/** minimum z value for floor normal (if less, not a walkable floor for this pawn) */

/** Used in determining if pawn is going off ledge.  If the ledge is "shorter" than this value then the pawn will be able to walk off it. **/
var const float     LedgeCheckThreshold;
var const Vector    PartialLedgeMoveDir;

/** Controller currently possessing this Pawn */
var editinline repnotify Controller Controller;

/** Chained pawn list */
var const Pawn NextPawn;

/** Used for cacheing net relevancy test */
var float				NetRelevancyTime;
var playerController	LastRealViewer;
var actor				LastViewer;

/** If true, call the script TickSpecial() event. */
var bool bScriptTickSpecial;

// Physics related flags.
var bool		bUpAndOut;			// used by swimming
var bool		bIsWalking;			// currently walking (can't jump, affects animations)

/** Physics to use when walking. Typically set to PHYS_Walking or PHYS_NavMeshWalking */
var(Movement) EPhysics  WalkingPhysics;

// Crouching
var				bool	bWantsToCrouch;		// if true crouched (physics will automatically reduce collision height to CrouchHeight)
var		const	bool	bIsCrouched;		// set by physics to specify that pawn is currently crouched
var		const	bool	bTryToUncrouch;		// when auto-crouch during movement, continually try to uncrouch
var()			bool	bCanCrouch;			// if true, this pawn is capable of crouching
var		const	float	UncrouchTime;		// when auto-crouch during movement, continually try to uncrouch once this decrements to zero
var				float	CrouchHeight;		// CollisionHeight when crouching
var				float	CrouchRadius;		// CollisionRadius when crouching
var		const	int		FullHeight;			// cached for pathfinding

var bool		bCrawler;			// crawling - pitch and roll based on surface pawn is on

/** Used by movement natives to slow pawn as it reaches its destination to prevent overshooting */
var const bool	bReducedSpeed;

var bool		bJumpCapable;
var	bool		bCanJump;			// movement capabilities - used by AI
var	bool		bCanWalk;
var	bool		bCanSwim;
var	bool		bCanFly;
var	bool		bCanClimbLadders;
var	bool		bCanStrafe;
var	bool		bAvoidLedges;		// don't get too close to ledges
var	bool		bStopAtLedges;		// if bAvoidLedges and bStopAtLedges, Pawn doesn't try to walk along the edge at all
var bool		bAllowLedgeOverhang;// if TRUE then allow the pawn to hang off ledges based on the cylinder extent
var const bool  bPartiallyOverLedge;// if TRUE pawn was over a ledge without falling, allows us to handle case if player stops
var const bool	bSimulateGravity;	// simulate gravity for this pawn on network clients when predicting position (true if pawn is walking or falling)
var	bool		bIgnoreForces;		// if true, not affected by external forces
var	bool		bCanWalkOffLedges;	// Can still fall off ledges, even when walking (for Player Controlled pawns)
var bool		bCanBeBaseForPawns;	// all your 'base', are belong to us
var const bool	bSimGravityDisabled;	// used on network clients
var bool		bDirectHitWall;		// always call pawn hitwall directly (no controller notifyhitwall)
var const bool	bPushesRigidBodies;	// Will do a check to find nearby PHYS_RigidBody actors and will give them a 'soft' push.
var	bool		bForceFloorCheck;	// force the pawn in PHYS_Walking to do a check for a valid floor even if he hasn't moved.	Cleared after next floor check.
var bool		bForceKeepAnchor;	// Force ValidAnchor function to accept any non-NULL anchor as valid (used to override when we want to set anchor for path finding)

var config bool bCanMantle;			// can this pawn mantle over cover
var config bool bCanClimbUp;		// can this pawn climb up cover wall
var		   bool bCanClimbCeilings;	// can this pawn climb ceiling nodes
var config bool bCanSwatTurn;		// can this pawn swat turn between cover
var config bool bCanLeap;			// can this pawn use LeapReachSpec
var config bool	bCanCoverSlip;		// can this pawn coverslip

/** if set, display "MAP HAS PATHING ERRORS" and message in the log when a Pawn fails a full path search */
var globalconfig bool bDisplayPathErrors;

// AI related flags
var		bool	bCanPickupInventory;	// if true, will pickup inventory when touching pickup actors
var		bool	bAmbientCreature;		// AIs will ignore me
var(AI) bool	bLOSHearing;			// can hear sounds from line-of-sight sources (which are close enough to hear)
										// bLOSHearing=true is like UT/Unreal hearing
var(AI) bool	bMuffledHearing;		// can hear sounds through walls (but muffled - sound distance increased to double plus 4x the distance through walls
var(AI) bool	bDontPossess;			// if true, Pawn won't be possessed at game start
var		bool	bRollToDesired;			// Update roll when turning to desired rotation (normally false)
var		bool	bStationary;			// pawn can't move

var		bool	bCachedRelevant;		// network relevancy caching flag
var		bool	bNoWeaponFiring;		// TRUE indicates that weapon firing is disabled for this pawn
var		bool	bModifyReachSpecCost;	// pawn should call virtual function to modify reach spec costs
var		bool	bModifyNavPointDest;	// pawn should call virtual function to modify destination location when moving to nav point
/** set if Pawn counts as a vehicle for pathfinding checks (so don't use bBlockedForVehicles nodes, etc) */
var bool bPathfindsAsVehicle;
/** Pawn multiplies cost of NavigationPoints that don't have bPreferredVehiclePath set by this number */
var float NonPreferredVehiclePathMultiplier;

/** Previously used ShouldBypassSimulatedClientPhysics flag **/
var bool bPrevBypassSimulatedClientPhysics;

// AI basics.
enum EPathSearchType
{
	PST_Default,
	PST_Breadth,
	PST_NewBestPathTo,
	PST_Constraint,
};
var EPathSearchType	PathSearchType;

/** List of search constraints for pathing */
var PathConstraint		PathConstraintList;
var PathGoalEvaluator	PathGoalList;

var		float	DesiredSpeed;
var		float	MaxDesiredSpeed;
var(AI) float	HearingThreshold;	// max distance at which a makenoise(1.0) loudness sound can be heard
var(AI)	float	Alertness;			// -1 to 1 ->Used within specific states for varying reaction to stimuli
var(AI)	float	SightRadius;		// Maximum seeing distance.
var(AI)	float	PeripheralVision;	// Cosine of limits of peripheral vision.
var const float	AvgPhysicsTime;		// Physics updating time monitoring (for AI monitoring reaching destinations)
var			  float		  Mass;				// Mass of this pawn.
var			  float		  Buoyancy;			// Water buoyancy. A ratio (1.0 = neutral buoyancy, 0.0 = no buoyancy)
var		float	MeleeRange;			// Max range for melee attack (not including collision radii)
var const NavigationPoint Anchor;			// current nearest path;
var const int             AnchorItem;       // Used to index into nav mesh polys
var const NavigationPoint LastAnchor;		// recent nearest path
var		float	FindAnchorFailedTime;	// last time a FindPath() attempt failed to find an anchor.
var		float	LastValidAnchorTime;	// last time a valid anchor was found
var		float	DestinationOffset;	// used to vary destination over NavigationPoints
var		float	NextPathRadius;		// radius of next path in route
var		vector	SerpentineDir;		// serpentine direction
var		float	SerpentineDist;
var		float	SerpentineTime;		// how long to stay straight before strafing again
var		float	SpawnTime;			// worldinfo time when this pawn was spawned
var		int		MaxPitchLimit;		// limit on view pitching

// Movement.
var	bool	bRunPhysicsWithNoController;	// When there is no Controller, Walking Physics abort and force a velocity and acceleration of 0. Set this to TRUE to override.
var bool	bForceMaxAccel;	// ignores Acceleration component, and forces max AccelRate to drive Pawn at full velocity.
var float	GroundSpeed;	// The maximum ground speed.
var float	WaterSpeed;		// The maximum swimming speed.
var float	AirSpeed;		// The maximum flying speed.
var float	LadderSpeed;	// Ladder climbing speed
var float	AccelRate;		// max acceleration rate
var float	JumpZ;			// vertical acceleration w/ jump
var float	OutofWaterZ;	/** z velocity applied when pawn tries to get out of water */
var float	MaxOutOfWaterStepHeight;	/** Maximum step height for getting out of water */
var bool	bLimitFallAccel; // should acceleration be limited (by a factor of GroundSpeed and AirControl) when in PHYS_Falling?
var float	AirControl;		// amount of AirControl available to the pawn
var float	WalkingPct;		// pct. of running speed that walking speed is
var float	MovementSpeedModifier; // a modifier that can be used to override the movement speed.
var float	CrouchedPct;	// pct. of running speed that crouched walking speed is
var float	MaxFallSpeed;	// max speed pawn can land without taking damage

/** AI will take paths that require a landing velocity less than (MaxFallSpeed * AIMaxFallSpeedFactor) */
var float AIMaxFallSpeedFactor;

var(Camera) float	BaseEyeHeight;	// Base eye height above collision center.
var(Camera) float		EyeHeight;		// Current eye height, adjusted for bobbing and stairs.
var	vector			Floor;			// Normal of floor pawn is standing on (only used by PHYS_Spider and PHYS_Walking)
var float			SplashTime;		// time of last splash
var transient PhysicsVolume HeadVolume;		// physics volume of head
var() int Health;		/** amount of health this Pawn has */
var() int HealthMax;		/** normal maximum health of Pawn - defaults to default.Health unless explicitly set otherwise */
var bool bReplicateHealthToAll; /** if true, replicate this Pawn's health to all clients; otherwise, only if owned by or ViewTarget of a client */
var	float			BreathTime;		// used for getting BreathTimer() messages (for no air, etc.)
var float			UnderWaterTime; // how much time pawn can go without air (in seconds)
var	float			LastPainTime;	// last time pawn played a takehit animation (updated in PlayHit())
var float           KismetDeathDelayTime;

/** RootMotion derived velocity calculated by APawn::CalcVelocity() (used when replaying client moves in net games (since can't rely on animation when replaying moves)) */
var vector RMVelocity;

/** this flag forces APawn::CalcVelocity() to just use RMVelocity directly */
var bool bForceRMVelocity;

/** this flag forces APawn::CalcVelocity() to never use root motion derived velocity */
var bool bForceRegularVelocity;

// Sound and noise management
// remember location and position of last noises propagated
var const	vector		noise1spot;
var const	float		noise1time;
var const	pawn		noise1other;
var const	float		noise1loudness;
var const	vector		noise2spot;
var const	float		noise2time;
var const	pawn		noise2other;
var const	float		noise2loudness;

var float SoundDampening;
var float DamageScaling;

var localized  string MenuName; // Name used for this pawn type in menus (e.g. player selection)

var class<AIController> ControllerClass;	// default class to use when pawn is controlled by AI

var editinline RepNotify PlayerReplicationInfo PlayerReplicationInfo;

var LadderVolume OnLadder;		// ladder currently being climbed

var name LandMovementState;		// PlayerControllerState to use when moving on land or air
var name WaterMovementState;	// PlayerControllerState to use when moving in water

var PlayerStart LastStartSpot;	// used to avoid spawn camping
var float LastStartTime;

var vector				TakeHitLocation;		// location of last hit (for playing hit/death anims)
var class<DamageType>	HitDamageType;			// damage type of last hit (for playing hit/death anims)
var vector				TearOffMomentum;		// momentum to apply when torn off (bTearOff == true)
var bool				bPlayedDeath;			// set when death animation has been played (used in network games)

var() SkeletalMeshComponent	Mesh;

var	CylinderComponent		CylinderComponent;

var()	float				RBPushRadius; // Unreal units
var()	float				RBPushStrength;

var	repnotify	Vehicle DrivenVehicle;

var float AlwaysRelevantDistanceSquared;	// always relevant to other clients if closer than this distance to viewer, and have controller

/** replicated to we can see where remote clients are looking */
var		const	byte	RemoteViewPitch;

/** Radius that is checked for nearby vehicles when pressing use */
var() float	VehicleCheckRadius;

var Controller LastHitBy; //give kill credit to this guy if hit momentum causes pawn to fall to his death

var()	float	ViewPitchMin;
var()	float	ViewPitchMax;

/** Max difference between pawn's Rotation.Yaw and DesiredRotation.Yaw for pawn to be considered as having reached its desired rotation */
var		int		AllowedYawError;
/** Desired Target Rotation : Physics will smoothly rotate actor to this rotation **/
/** In future I will uncomment this change. Currently Actor has the variable.**/
var(Movement)	const rotator     DesiredRotation;
/** DesiredRotation is set by somebody - Pawn's default behavior (using direction for desiredrotation) does not work **/
var				const private{private} bool		bDesiredRotationSet;
/** Do not overwrite current DesiredRotation **/
var				const private{private} bool		bLockDesiredRotation;
/** Unlock DesiredRotation when Reached to the destination
  * This is used when bLockDesiredRotation=TRUE
  * This will set bLockDesiredRotation = FALSE when reached to DesiredRotation
  */
var				const private{private} bool		bUnlockWhenReached;
/** Inventory Manager */
var class<InventoryManager>		InventoryManagerClass;
var repnotify InventoryManager			InvManager;

/** Weapon currently held by Pawn */
var		Weapon					Weapon;

/**
 * This next group of replicated properties are used to cause 3rd person effects on
 * remote clients.	FlashLocation and FlashCount are altered by the weapon to denote that
 * a shot has occured and FiringMode is used to determine what type of shot.
 */

/** Hit Location of instant hit weapons. vect(0,0,0) = not firing. */
var repnotify	vector	FlashLocation;
/** last FlashLocation that was an actual shot, i.e. not counting clears to (0,0,0)
 * this is used to make sure we set unique values to FlashLocation for consecutive shots even when there was a clear in between,
 * so that if a client missed the clear due to low net update rate, it still gets the new firing location
 */
var vector LastFiringFlashLocation;
/** increased when weapon fires. 0 = not firing. 1 - 255 = firing */
var repnotify	byte	FlashCount;
/** firing mode used when firing */
var	repnotify	byte	FiringMode;
/** tracks the number of consecutive shots. Note that this is not replicated, so it's not correct on remote clients. It's only updated when the pawn is relevant. */
var				int		ShotCount;

/** set in InitRagdoll() to old CollisionComponent (since it must be Mesh for ragdolls) so that TermRagdoll() can restore it */
var PrimitiveComponent PreRagdollCollisionComponent;

/** Physics object created to create contacts with physics objects, used to push them around. */
var	RB_BodyInstance		PhysicsPushBody;

/** @HACK: count of times processLanded() was called but it failed without changing physics for some reason
 * so we can detect and avoid a rare case where Pawns get stuck in that state
 */
var int FailedLandingCount;

/** Controls whether the pawn needs the base ticked before this one can be ticked */
var bool bNeedsBaseTickedFirst;

/** Array of Slots */
var transient Array<AnimNodeSlot>	SlotNodes;
/** List of Matinee InterpGroup controlling this actor. */
var transient Array<InterpGroup>	InterpGroupList;

/** AudioComponent used by FaceFX */
var	transient protected AudioComponent				FacialAudioComp;

/** General material used to control common pawn material parameters (e.g. burning) */
var protected transient MaterialInstanceConstant MIC_PawnMat;
var protected transient MaterialInstanceConstant MIC_PawnHair;

/** If TRUE, translation of mesh is updated to match desired floor translation (0 unless special move desired floor conforming) */
var transient repnotify bool		bUsedByMatinee;

struct native ScalarParameterInterpStruct
{
	/** Name of parameter to change */
	var() Name ParameterName;
	/** Desired Parameter Value */
	var() float ParameterValue;
	/** Desired Interpolation Time */
	var() float InterpTime;
	/** Time before interpolation starts */
	var() float WarmUpTime;
};
var() Array<ScalarParameterInterpStruct> ScalarParameterInterpArray;

/** Whether root motion should be extracted from the interp curve or not */
/** NOTE: Currently assumes blending isn't altering the root bone */
var bool					bRootMotionFromInterpCurve;
var RootMotionCurve			RootMotionInterpCurve;
var float					RootMotionInterpRate;
var float					RootMotionInterpCurrentTime;
var Vector					RootMotionInterpCurveLastValue;

//debug
var(Debug) bool bDebugShowCameraLocation;

/** 
 *  Set this to TRUE if riding on a moving base that you know is clear from non-moving world obstructions.
 *  This can solve move-order dependencies when riding a mover, and it's faster.
 */
var() bool bFastAttachedMove;

cpptext
{
	// declare type for node evaluation functions
	typedef FLOAT ( *NodeEvaluator ) (ANavigationPoint*, APawn*, FLOAT);

	virtual void PostBeginPlay();
	virtual void PostScriptDestroyed();

	// AActor interface.

	APawn* GetPlayerPawn() const;
	virtual FLOAT GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, APlayerController* Viewer, UActorChannel* InChannel, FLOAT Time, UBOOL bLowBandwidth);
	virtual INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	virtual void NotifyBump(AActor *Other, UPrimitiveComponent* OtherComp, const FVector &HitNormal);
	virtual void TickSimulated( FLOAT DeltaSeconds );
	virtual void TickSpecial( FLOAT DeltaSeconds );
	UBOOL PlayerControlled();
	virtual void SetBase(AActor *NewBase, FVector NewFloor = FVector(0,0,1), INT bNotifyActor=1, USkeletalMeshComponent* SkelComp=NULL, FName BoneName=NAME_None );
#if WITH_EDITOR
	virtual void EditorApplyRotation(const FRotator& DeltaRotation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
	virtual void CheckForErrors();
#endif
	virtual UBOOL IsNetRelevantFor(APlayerController* RealViewer, AActor* Viewer, const FVector& SrcLocation);
	UBOOL CacheNetRelevancy(UBOOL bIsRelevant, APlayerController* RealViewer, AActor* Viewer);
	virtual UBOOL ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags);
	virtual void PreNetReceive();
	virtual void PostNetReceiveLocation();
	virtual void SmoothCorrection(const FVector& OldLocation);
	virtual APawn* GetAPawn() { return this; }
	virtual const APawn* GetAPawn() const { return this; }

	/**
	 * Used for SkelControlLimb in BCS_BaseMeshSpace mode.
	 *
	 * @param SkelControlled - the mesh being modified by the skel control
	 *
	 * @return the skeletal mesh component to use for the transform calculation
	 */
	virtual USkeletalMeshComponent* GetMeshForSkelControlLimbTransform(const USkeletalMeshComponent* SkelControlled) { return Mesh; }

	/**
	 * Sets the hard attach flag by first handling the case of already being
	 * based upon another actor
	 *
	 * @param bNewHardAttach the new hard attach setting
	 */
	virtual void SetHardAttach(UBOOL bNewHardAttach);

	virtual UBOOL CanCauseFractureOnTouch()
	{
		return TRUE;
	}

	// Level functions
	void SetZone( UBOOL bTest, UBOOL bForceRefresh );

	// AI sensing
	virtual void CheckNoiseHearing(AActor* NoiseMaker, FLOAT Loudness, FName NoiseType=NAME_None );
	virtual FLOAT DampenNoise(AActor* NoiseMaker, FLOAT Loudness, FName NoiseType=NAME_None );


	// Latent movement
	virtual void setMoveTimer(FVector MoveDir);
	FLOAT GetMaxSpeed();
	virtual UBOOL moveToward(const FVector &Dest, AActor *GoalActor);
	virtual UBOOL IsGlider();
	virtual void rotateToward(FVector FocalPoint);
	void StartNewSerpentine(const FVector& Dir, const FVector& Start);
	void ClearSerpentine();
	virtual UBOOL SharingVehicleWith(APawn *P);
	void InitSerpentine();
	virtual void HandleSerpentineMovement(FVector& out_Direction, FLOAT Distance, const FVector& Dest);

	// reach tests
	virtual UBOOL ReachedDestination(const FVector &Start, const FVector &Dest, AActor* GoalActor, UBOOL bCheckHandle=FALSE);
	virtual int pointReachable(FVector aPoint, int bKnowVisible=0);
	virtual int actorReachable(AActor *Other, UBOOL bKnowVisible=0, UBOOL bNoAnchorCheck=0);
	virtual int Reachable(FVector aPoint, AActor* GoalActor);
	int walkReachable(const FVector &Dest, const FVector &Start, int reachFlags, AActor* GoalActor);
	int flyReachable(const FVector &Dest, const FVector &Start, int reachFlags, AActor* GoalActor);
	int swimReachable(const FVector &Dest, const FVector &Start, int reachFlags, AActor* GoalActor);
	int ladderReachable(const FVector &Dest, const FVector &Start, int reachFlags, AActor* GoalActor);
	INT spiderReachable( const FVector &Dest, const FVector &Start, INT reachFlags, AActor* GoalActor );
	FVector GetGravityDirection();
	virtual UBOOL TryJumpUp(FVector Dir, FVector Destination, DWORD TraceFlags, UBOOL bNoVisibility);
	virtual UBOOL ReachedBy(APawn* P, const FVector& TestPosition, const FVector& Dest);
	virtual UBOOL ReachThresholdTest(const FVector &TestPosition, const FVector &Dest, AActor* GoalActor, FLOAT UpThresholdAdjust, FLOAT DownThresholdAdjust, FLOAT ThresholdAdjust);
	virtual UBOOL SetHighJumpFlag() { return false; }

	// movement component tests (used by reach tests)
	void TestMove(const FVector &Delta, FVector &CurrentPosition, FCheckResult& Hit, const FVector &CollisionExtent);
	FVector GetDefaultCollisionSize();
	FVector GetCrouchSize();
	ETestMoveResult walkMove(FVector Delta, FVector &CurrentPosition, const FVector &CollisionExtent, FCheckResult& Hit, AActor* GoalActor, FLOAT threshold);
	ETestMoveResult flyMove(FVector Delta, FVector &CurrentPosition, AActor* GoalActor, FLOAT threshold);
	ETestMoveResult swimMove(FVector Delta, FVector &CurrentPosition, AActor* GoalActor, FLOAT threshold);
	virtual ETestMoveResult FindBestJump(FVector Dest, FVector &CurrentPosition);
	virtual ETestMoveResult FindJumpUp(FVector Direction, FVector &CurrentPosition);
	ETestMoveResult HitGoal(AActor *GoalActor);
	virtual UBOOL HurtByDamageType(class UClass* DamageType);
	UBOOL CanCrouchWalk( const FVector& StartLocation, const FVector& EndLocation, AActor* HitActor );
	/** updates the highest landing Z axis velocity encountered during a reach test */
	virtual void SetMaxLandingVelocity(FLOAT NewLandingVelocity) {}

	// Path finding
	UBOOL GeneratePath();
	FLOAT findPathToward(AActor *goal, FVector GoalLocation, NodeEvaluator NodeEval, FLOAT BestWeight, UBOOL bWeightDetours, INT MaxPathLength = 0, UBOOL bReturnPartial = FALSE, INT SoftMaxNodes = 200);
	ANavigationPoint* BestPathTo(NodeEvaluator NodeEval, ANavigationPoint *start, FLOAT *Weight, UBOOL bWeightDetours, INT MaxPathLength = 0, INT SoftMaxNodes = 200);
	virtual ANavigationPoint* CheckDetour(ANavigationPoint* BestDest, ANavigationPoint* Start, UBOOL bWeightDetours);
	virtual INT calcMoveFlags();
	/** returns the maximum falling speed an AI will accept along a path */
	FORCEINLINE FLOAT GetAIMaxFallSpeed() { return MaxFallSpeed * AIMaxFallSpeedFactor; }
	virtual void MarkEndPoints(ANavigationPoint* EndAnchor, AActor* Goal, const FVector& GoalLocation);
	virtual FLOAT SecondRouteAttempt(ANavigationPoint* Anchor, ANavigationPoint* EndAnchor, NodeEvaluator NodeEval, FLOAT BestWeight, AActor *goal, const FVector& GoalLocation, FLOAT StartDist, FLOAT EndDist, INT MaxPathLength, INT SoftMaxNodes);
	/** finds the closest NavigationPoint within MAXPATHDIST that is usable by this pawn and directly reachable to/from TestLocation
	 * @param TestActor the Actor to find an anchor for
	 * @param TestLocation the location to find an anchor for
	 * @param bStartPoint true if we're finding the start point for a path search, false if we're finding the end point
	 * @param bOnlyCheckVisible if true, only check visibility - skip reachability test
	 * @param Dist (out) if an anchor is found, set to the distance TestLocation is from it. Set to 0.f if the anchor overlaps TestLocation
	 * @return a suitable anchor on the navigation network for reaching TestLocation, or NULL if no such point exists
	 */
	ANavigationPoint* FindAnchor(AActor* TestActor, const FVector& TestLocation, UBOOL bStartPoint, UBOOL bOnlyCheckVisible, FLOAT& Dist);
	virtual INT		ModifyCostForReachSpec( UReachSpec* Spec, INT Cost ) { return 0; }
	virtual void	InitForPathfinding( AActor* Goal, ANavigationPoint* EndAnchor ) {}
	// allows pawn subclasses to veto anchor validity
	virtual UBOOL	IsValidAnchor( ANavigationPoint* AnchorCandidate ){ return TRUE; }

	/*
	 * Route finding notifications (sent to target)
	 */
	virtual ANavigationPoint* SpecifyEndAnchor(APawn* RouteFinder);
	virtual UBOOL AnchorNeedNotBeReachable();
	virtual void NotifyAnchorFindingResult(ANavigationPoint* EndAnchor, APawn* RouteFinder);

	// Pawn physics modes
	virtual void setPhysics(BYTE NewPhysics, AActor* NewFloor = NULL, FVector NewFloorV = FVector(0,0,1));
	virtual void performPhysics(FLOAT DeltaSeconds);
	/** Called in PerformPhysics(), after StartNewPhysics() is done moving the Actor, and before the PendingTouch() event is dispatched. */
	virtual void PostProcessPhysics( FLOAT DeltaSeconds, const FVector& OldVelocity );
	/** If TRUE, bypass simulated client physics, and run owned/server physics instead. Do not perform simulation/correction. */
	virtual UBOOL ShouldBypassSimulatedClientPhysics();
	virtual FVector CheckForLedges(FVector AccelDir, FVector Delta, FVector GravDir, int &bCheckedFall, int &bMustJump );
	virtual void physWalking(FLOAT deltaTime, INT Iterations);
	virtual void physNavMeshWalking(FLOAT deltaTime);
	virtual void physFlying(FLOAT deltaTime, INT Iterations);
	virtual void physSwimming(FLOAT deltaTime, INT Iterations);
	virtual void physFalling(FLOAT deltaTime, INT Iterations);
	virtual void physSpider(FLOAT deltaTime, INT Iterations);
	virtual void physLadder(FLOAT deltaTime, INT Iterations);
	virtual void startNewPhysics(FLOAT deltaTime, INT Iterations);
	virtual void GetNetBuoyancy(FLOAT &NetBuoyancy, FLOAT &NetFluidFriction);
	virtual void StartFalling(INT Iterations, FLOAT remainingTime, FLOAT timeTick, const FVector& Delta, const FVector& subLoc);
	virtual UBOOL ShouldCatchAir(const FVector& OldFloor, const FVector& Floor);
	void startSwimming(FVector OldLocation, FVector OldVelocity, FLOAT timeTick, FLOAT remainingTime, INT Iterations);
	virtual void physicsRotation(FLOAT deltaTime, FVector OldVelocity);
	void processLanded(FVector const& HitNormal, AActor *HitActor, FLOAT remainingTime, INT Iterations);
	virtual void SetPostLandedPhysics(AActor *HitActor, FVector HitNormal);
	virtual void processHitWall(FCheckResult const& Hit, FLOAT TimeSlice=0.f);
	virtual void Crouch(INT bClientSimulation=0);
	virtual void UnCrouch(INT bClientSimulation=0);
	void SmoothHitWall(FVector const& HitNormal, AActor *HitActor);
	virtual FVector NewFallVelocity(FVector OldVelocity, FVector OldAcceleration, FLOAT timeTick);
	void stepUp(const FVector& GravDir, const FVector& DesiredDir, const FVector& Delta, FCheckResult &Hit);
	virtual FLOAT MaxSpeedModifier();
	virtual FLOAT GetMaxAccel( FLOAT SpeedModifier = 1.f );
	virtual FVector CalculateSlopeSlide(const FVector& Adjusted, const FCheckResult& Hit);
	virtual UBOOL IgnoreBlockingBy(const AActor* Other) const;
	virtual void PushedBy(AActor* Other);
	virtual void UpdateBasedRotation(FRotator &FinalRotation, const FRotator& ReducedRotation);
	virtual void ReverseBasedRotation();

	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);

	/** Update information used to detect overlaps between this actor and physics objects, used for 'pushing' things */
	virtual void UpdatePushBody();

	/** Called when the push body 'sensor' overlaps a physics body. Allows you to add a force to that body to move it. */
	virtual void ProcessPushNotify(const FRigidBodyCollisionInfo& PushedInfo, const TArray<FRigidBodyContactInfo>& ContactInfos);

	virtual UBOOL HasAudibleAmbientSound(const FVector& SrcLocation) { return FALSE; }

	//superville: Chance for pawn to say he has reached a location w/o touching it (ie cover slot)
	virtual UBOOL HasReached( ANavigationPoint *Nav, UBOOL& bFinalDecision ) { return FALSE; }

	virtual FVector GetIdealCameraOrigin()
	{
		return FVector(Location.X,Location.Y,Location.Z + BaseEyeHeight);
	}

	/**
	 * Checks whether this pawn needs to have its base ticked first and does so if requested
	 *
	 * @return TRUE if the actor was ticked, FALSE if it was aborted (e.g. because it's in stasis)
	 */
	virtual UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );

	/** Build AnimSet list, called by UpdateAnimSetList() */
	virtual void BuildAnimSetList();
	void RestoreAnimSetsToDefault();

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

	/** Called each frame by Matinee for InterpMoveTrack to adjust their location/rotation **/
	virtual void AdjustInterpTrackMove(FVector& Pos, FRotator& Rot, FLOAT DeltaTime, UBOOL bIgnoreRotation = FALSE);

	virtual UBOOL FindInterpMoveTrack(class UInterpTrackMove** MoveTrack, class UInterpTrackInstMove** MoveTrackInst, class USeqAct_Interp** OutSeq);

	void UpdateScalarParameterInterp(FLOAT DeltaTime);

protected:
	virtual void ApplyVelocityBraking(FLOAT DeltaTime, FLOAT Friction);
	virtual void CalcVelocity(FVector &AccelDir, FLOAT DeltaTime, FLOAT MaxSpeed, FLOAT Friction, INT bFluid, INT bBrake, INT bBuoyant);

private:
	UBOOL Pick3DWallAdjust(FVector WallHitNormal, AActor* HitActor);
	FLOAT Swim(FVector Delta, FCheckResult &Hit);
	FVector findWaterLine(FVector Start, FVector End);
	void SpiderstepUp(const FVector& DesiredDir, const FVector& Delta, FCheckResult &Hit);
	int findNewFloor(FVector OldLocation, FLOAT deltaTime, FLOAT remainingTime, INT Iterations);
	int checkFloor(FVector Dir, FCheckResult &Hit);
}

replication
{
	// Variables the server should send ALL clients.
	if( bNetDirty )
		FlashLocation, bSimulateGravity, bIsWalking, PlayerReplicationInfo, HitDamageType,
		TakeHitLocation, DrivenVehicle, bUsedByMatinee, bFastAttachedMove;
	if (bNetDirty && (bNetOwner || bReplicateHealthToAll /* || IsViewTargetOfReplicationViewer() */))
		Health;

	// variables sent to owning client
	if ( bNetDirty && bNetOwner )
		InvManager, Controller, GroundSpeed, WaterSpeed, AirSpeed, AccelRate, JumpZ, AirControl;
	if (bNetDirty && bNetOwner && bNetInitial)
		bCanSwatTurn;

	// sent to non owning clients
	if ( bNetDirty && (!bNetOwner || bDemoRecording) )
		bIsCrouched, FlashCount, FiringMode;

	// variable sent to all clients when Pawn has been torn off. (bTearOff)
	if( bTearOff && bNetDirty )
		TearOffMomentum;

	// variables sent to all but the owning client
	if ( (!bNetOwner || bDemoRecording) )
		RemoteViewPitch;

	if( bNetInitial && !bNetOwner )
		bRootMotionFromInterpCurve;

	if( bNetInitial && !bNetOwner && bRootMotionFromInterpCurve )
		RootMotionInterpRate, RootMotionInterpCurrentTime, RootMotionInterpCurveLastValue;

	// Replicated to ALL
	if( ( Role == Role_Authority ) && bNetDirty )
		HealthMax;
}

native final function bool PickWallAdjust(Vector WallHitNormal, Actor HitActor);

/** DesiredRotation related function **/
/** SetDesiredRotation function
  * @param TargetDesiredRotation: DesiredRotation you want
  * @param InLockDesiredRotation: I'd like to lock up DesiredRotation, please nobody else can touch it until I say it's done
  * @param InUnlockWhenReached: When you lock, set this to TRUE if you want it to be auto Unlock when reached desired rotation
  * @param InterpolationTime: Give interpolation time to get to the desired rotation - Ignore default RotationRate, but use this to get there
  * @return TRUE if properly set, otherwise, return FALSE
  **/
native final function bool SetDesiredRotation(Rotator TargetDesiredRotation, bool InLockDesiredRotation=FALSE, bool InUnlockWhenReached=FALSE, FLOAT InterpolationTime=-1.f, bool bResetRotationRate=TRUE);

/** LockDesiredRotation function
  * @param Lock: Lock or Unlock CurrentDesiredRotation
  * @param InUnlockWhenReached: Unlock when reached desired rotation. This is only valid when Lock = true
  */
native final function LockDesiredRotation(bool Lock, bool InUnlockWhenReached=false/** This is only valid if Lock=true **/);
/** ResetDesiredRotation function
  * Clear RotationRate/Flag to go back to default behavior
  * Unless it's locked.
  */
native final function ResetDesiredRotation();
/** CheckDesiredRotation function
* Check to see if DesiredRotation is met, and it need to be clear or not
* This is called by physicsRotation to make sure it needs to be cleared
*/
native final function CheckDesiredRotation();
/** IsDesiredRotationInUse()
* See if DesiredRotation is used by somebody
*/
native final function bool IsDesiredRotationInUse();
/** IsDesiredRotationLocked()
* See if DesiredRotation is locked by somebody
*/
native final function bool IsDesiredRotationLocked();

simulated event PostInitAnimTree(SkeletalMeshComponent SkelComp)
{
	Super.PostInitAnimTree(SkelComp);

	// Only refresh anim nodes if our main mesh was updated
	if (SkelComp == Mesh)
	{
		ClearAnimNodes();
		CacheAnimNodes();
	}
}

/** Save off commonly used nodes so the tree doesn't need to be iterated over often */
simulated native event CacheAnimNodes();

/** Remove references to the saved nodes */
simulated function ClearAnimNodes()
{
	SlotNodes.Length = 0;
}

/** Update list of AnimSets for this Pawn */
simulated native final function UpdateAnimSetList();
/** Build AnimSet list Script version, called by UpdateAnimSetList() */
simulated event BuildScriptAnimSetList();

/**
 * Add a given list of anim sets on the top of the list (so they override the other ones
 * !! Only use within BuildScriptAnimSetList() !!
 */
simulated native final function AddAnimSets(const out array<AnimSet> CustomAnimSets);

/** Called after UpdateAnimSetList does its job */
simulated event AnimSetListUpdated();

simulated event bool RestoreAnimSetsToDefault()
{
	Mesh.AnimSets = default.Mesh.AnimSets;
	return TRUE;
}

// Matinee Track Support Start

/** Called when we start an AnimControl track operating on this Actor. Supplied is the set of AnimSets we are going to want to play from. */
simulated event BeginAnimControl(InterpGroup InInterpGroup)
{
//	`log(self@" Begin Anim Control : Rotation:"@Rotation@" Location:"@Location);
	MAT_BeginAnimControl(InInterpGroup);
}
/** Start AnimControl. Add required AnimSets. */
native function MAT_BeginAnimControl(InterpGroup InInterpGroup);

/** Called when we are done with the AnimControl track. */
simulated event FinishAnimControl(InterpGroup InInterpGroup)
{
	MAT_FinishAnimControl(InInterpGroup);
}
/** End AnimControl. Release required AnimSets */
native function MAT_FinishAnimControl(InterpGroup InInterpGroup);

/** Called each from while the Matinee action is running, with the desired sequence name and position we want to be at. */
simulated event SetAnimPosition(name SlotName, int ChannelIndex, name InAnimSeqName, float InPosition, bool bFireNotifies, bool bLooping, bool bEnableRootMotion)
{
//	`log(self@" Slot:"@SlotName@" AnimSeqName:"@InAnimSeqName@" InPosition:"@InPosition@" Rotation:"@Rotation@" Location:"@Location);
	MAT_SetAnimPosition(SlotName, ChannelIndex, InAnimSeqName, InPosition, bFireNotifies, bLooping, bEnableRootMotion);
}

/** Update AnimTree from track info */
native function MAT_SetAnimPosition(name SlotName, int ChannelIndex, name InAnimSeqName, float InPosition, bool bFireNotifies, bool bLooping, bool bEnableRootMotion);

/** Update AnimTree from track weights */
native function MAT_SetAnimWeights(Array<AnimSlotInfo> SlotInfos);

native function MAT_SetMorphWeight(name MorphNodeName, float MorphWeight);

native function MAT_SetSkelControlScale(name SkelControlName, float Scale);

native function MAT_SetSkelControlStrength(name SkelControlName, float ControlStrength);

/** called when a SeqAct_Interp action starts interpolating this Actor via matinee
 * @note this function is called on clients for actors that are interpolated clientside via MatineeActor
 * @param InterpAction the SeqAct_Interp that is affecting the Actor
 */
simulated event InterpolationStarted(SeqAct_Interp InterpAction, InterpGroupInst GroupInst)
{

	Super.InterpolationStarted( InterpAction, GroupInst );
}

/** called when a SeqAct_Interp action finished interpolating this Actor
 * @note this function is called on clients for actors that are interpolated clientside via MatineeActor
 * @param InterpAction the SeqAct_Interp that was affecting the Actor
 */
simulated event InterpolationFinished(SeqAct_Interp InterpAction)
{
	Super.InterpolationFinished( InterpAction );
}

simulated function BeginAIGroup();
simulated function FinishAIGroup();

event MAT_BeginAIGroup(vector StartLoc, rotator StartRot)
{
	SetLocation(StartLoc);
	SetRotation(StartRot);
	BeginAIGroup();

	bUsedByMatinee = true;
}

event MAT_FinishAIGroup()
{
	FinishAIGroup();
	bUsedByMatinee = false;
}
/**
 * Play FaceFX animations on this Actor.
 * Returns TRUE if succeeded, if failed, a log warning will be issued.
 */
simulated event bool PlayActorFaceFXAnim(FaceFXAnimSet AnimSet, String GroupName, String SeqName, SoundCue SoundCueToPlay )
{
	return Mesh.PlayFaceFXAnim(AnimSet, SeqName, GroupName, SoundCueToPlay);
}

/** Stop any matinee FaceFX animations on this Actor. */
event StopActorFaceFXAnim()
{
	Mesh.StopFaceFXAnim();
}

/** Used to let FaceFX know what component to play dialogue audio on. */
simulated event AudioComponent GetFaceFXAudioComponent()
{
	return FacialAudioComp;
}

/**
 * Returns TRUE if Actor is playing a FaceFX anim.
 * Implement in sub-class.
 */
simulated function bool IsActorPlayingFaceFXAnim()
{
	return (Mesh != None && Mesh.IsPlayingFaceFXAnim());
}

/**
* Returns FALSE??? if Actor can play facefx
* Implement in sub-class.
*/
simulated function bool CanActorPlayFaceFXAnim()
{
	return TRUE;
}

/** Function for handling the SeqAct_PlayFaceFXAnim Kismet action working on this Actor. */
simulated function OnPlayFaceFXAnim(SeqAct_PlayFaceFXAnim inAction)
{
	//`log("Play FaceFX animation from KismetAction for" @ Self @ "GroupName:" @ inAction.FaceFXGroupName @ "AnimName:" @ inAction.FaceFXAnimName);
	Mesh.PlayFaceFXAnim(inAction.FaceFXAnimSetRef, inAction.FaceFXAnimName, inAction.FaceFXGroupName, inAction.SoundCueToPlay);
}

/**
 * Called via delegate when FacialAudioComp is finished.
 */
simulated function FaceFXAudioFinished(AudioComponent AC)
{
}

/** Used by Matinee in-game to mount FaceFXAnimSets before playing animations. */
event FaceFXAsset GetActorFaceFXAsset()
{
	if (Mesh.SkeletalMesh != None && !Mesh.bDisableFaceFX)
	{	
		return Mesh.SkeletalMesh.FaceFXAsset;
	}
	else
	{
		return None;
	}
}

/** Called each frame by Matinee to update the weight of a particular MorphNodeWeight. */
event SetMorphWeight(name MorphNodeName, float MorphWeight)
{
	MAT_SetMorphweight(MorphNodeName, MorphWeight);
}

/** Called each frame by Matinee to update the scaling on a SkelControl. */
event SetSkelControlScale(name SkelControlName, float Scale)
{
	MAT_SetSkelControlScale(SkelControlName, Scale);
}

// Matinee Track Support End

//
/** Check on various replicated data and act accordingly. */
simulated event ReplicatedEvent( name VarName )
{
	//`log( WorldInfo.TimeSeconds @ GetFuncName() @ "VarName:" @ VarName );

	super.ReplicatedEvent( VarName );

	if( VarName == 'FlashCount' )	// FlashCount and FlashLocation are changed when a weapon is fired.
	{
		FlashCountUpdated(Weapon, FlashCount, TRUE);
	}
	else if( VarName == 'FlashLocation' ) // FlashCount and FlashLocation are changed when a weapon is fired.
	{
		FlashLocationUpdated(Weapon, FlashLocation, TRUE);
	}
	else if( VarName == 'FiringMode' )
	{
		FiringModeUpdated(Weapon, FiringMode, TRUE);
	}
	else if ( VarName == 'DrivenVehicle' )
	{
		if ( DrivenVehicle != None )
		{
			// since pawn doesn't have a PRI while driving, and may become initially relevant while driving,
			// we may only be able to ascertain the pawn's team (for team coloring, etc.) through its drivenvehicle
			NotifyTeamChanged();
		}
	}
	else if ( VarName == 'PlayerReplicationInfo' )
	{
		NotifyTeamChanged();
	}
	else if ( VarName == 'Controller' )
	{
		if ( (Controller != None) && (Controller.Pawn == None) )
		{
			Controller.Pawn = self;
			if ( (PlayerController(Controller) != None)
				&& (PlayerController(Controller).ViewTarget == Controller) )
				PlayerController(Controller).SetViewTarget(self);
		}
	}
	else if ( VarName == 'bUsedByMatinee')
	{
		if (bUsedByMatinee)
		{
			BeginAIGroup();
		}
		else
		{
			FinishAIGroup();
		}
	}
}


// =============================================================

/** Returns TRUE if Pawn is alive and doing well */
final virtual simulated native function bool IsAliveAndWell() const;

final native virtual function Vector AdjustDestination( Actor GoalActor, optional Vector Dest );

/** Is the current anchor valid? */
final native function bool ValidAnchor();

/**
 * SuggestJumpVelocity()
 * returns true if succesful jump from start to destination is possible
 * returns a suggested initial falling velocity in JumpVelocity
 * Uses GroundSpeed and JumpZ as limits
 *
 * @param	JumpVelocity        The vector to fill with the calculated jump velocity
 * @param   Destination         The destination location of the jump
 * @param   Start               The start location of the jump
 * @param   bRequireFallLanding If true, the jump calculated will have a velocity in the negative Z at the destination
*/
native function bool SuggestJumpVelocity(out vector JumpVelocity, vector Destination, vector Start, optional bool bRequireFallLanding);

/**
 *	GetFallDuration
 *	returns time before impact if pawn falls from current position with current velocity
 */
native function float GetFallDuration();

/** returns if we are a valid enemy for PRI
 * checks things like whether we're alive, teammates, etc
 * works on clients and servers
 */
native function bool IsValidEnemyTargetFor(const PlayerReplicationInfo PRI, bool bNoPRIisEnemy);

/**
@RETURN true if pawn is invisible to AI
*/
native function bool IsInvisible();

/**
 * Set Pawn ViewPitch, so we can see where remote clients are looking.
 *
 * @param	NewRemoteViewPitch	Pitch component to replicate to remote (non owned) clients.
 */
native final function SetRemoteViewPitch( int NewRemoteViewPitch );

native function SetAnchor( NavigationPoint NewAnchor );
native function NavigationPoint GetBestAnchor( Actor TestActor, Vector TestLocation, bool bStartPoint, bool bOnlyCheckVisible, out float out_Dist );
native function bool ReachedDestination(Actor Goal);
native function bool ReachedPoint( Vector Point, Actor NewAnchor );
native function ForceCrouch();
native function SetPushesRigidBodies( bool NewPush );
native final virtual function bool ReachedDesiredRotation();

native function GetBoundingCylinder(out float CollisionRadius, out float CollisionHeight) const;

/**
 * Does the following:
 *	- Assign the SkeletalMeshComponent 'Mesh' to the CollisionComponent
 *	- Call InitArticulated on the SkeletalMeshComponent.
 *	- Change the physics mode to PHYS_RigidBody
 */
native function bool InitRagdoll();
/** the opposite of InitRagdoll(); resets CollisionComponent to the default,
 * sets physics to PHYS_Falling, and calls TermArticulated() on the SkeletalMeshComponent
 * @return true on success, false if there is no Mesh, the Mesh is not in ragdoll, or we're otherwise not able to terminate the physics
 */
native function bool TermRagdoll();

/** Give pawn the chance to do something special moving between points */
function bool SpecialMoveTo( NavigationPoint Start, NavigationPoint End, Actor Next );
event bool SpecialMoveThruEdge( ENavMeshEdgeType EdgeType, INT Dir, Vector MoveStart, Vector MoveDest, optional Actor RelActor, optional int RelItem, optional NavigationHandle NavHandle );

simulated function SetBaseEyeheight()
{
	if ( !bIsCrouched )
		BaseEyeheight = Default.BaseEyeheight;
	else
		BaseEyeheight = FMin(0.8 * CrouchHeight, CrouchHeight - 10);
}

function PlayerChangedTeam()
{
	Died( None, class'DamageType', Location );
}

/* Reset()
reset actor to initial state - used when restarting level without reloading.
*/
function Reset()
{
	if ( (Controller == None) || Controller.bIsPlayer )
	{
		DetachFromController();
		Destroy();
	}
	else
		super.Reset();
}

function bool StopFiring()
{
	if( Weapon != None )
	{
		 Weapon.StopFire(Weapon.CurrentFireMode);
	}
	return TRUE;
}


/**
 * Pawn starts firing!
 * Called from PlayerController::StartFiring
 * Network: Local Player
 *
 * @param	FireModeNum		fire mode number
 */
simulated function StartFire(byte FireModeNum)
{
	if( bNoWeaponFIring )
	{
		return;
	}

	if( Weapon != None )
	{
		Weapon.StartFire(FireModeNum);
	}
}


/**
 * Pawn stops firing!
 * i.e. player releases fire button, this may not stop weapon firing right away. (for example press button once for a burst fire)
 * Network: Local Player
 *
 * @param	FireModeNum		fire mode number
 */
simulated function StopFire(byte FireModeNum)
{
	if( Weapon != None )
	{
		Weapon.StopFire(FireModeNum);
	}
}


/*********************************************************************************************
 * Remote Client Firing Magic...
 * @See Weapon::IncrementFlashCount()
 ********************************************************************************************/

/** Return FiringMode currently in use by weapon InWeapon */
simulated function byte GetWeaponFiringMode(Weapon InWeapon)
{
	return FiringMode;
}

/**
 * Set firing mode replication for remote clients trigger update notification.
 * Network: LocalPlayer and Server
 */
simulated function SetFiringMode(Weapon InWeapon, byte InFiringMode)
{
	//`log( WorldInfo.TimeSeconds @ GetFuncName() @ "old:" @ FiringMode @  "new:" @ FiringModeNum );
	if( FiringMode != InFiringMode )
	{
		FiringMode = InFiringMode;
		bForceNetUpdate = TRUE;

		// call updated event locally
		FiringModeUpdated(InWeapon, FiringMode, FALSE);
	}
}

/**
 * Called when FiringMode has been updated.
 *
 * Network: ALL
 */
simulated function FiringModeUpdated(Weapon InWeapon, byte InFiringMode, bool bViaReplication)
{
	if( InWeapon != None )
	{
		InWeapon.FireModeUpdated(InFiringMode, bViaReplication);
	}
}


/**
 * This function's responsibility is to signal clients that non-instant hit shot
 * has been fired. Call this on the server and local player.
 *
 * Network: Server and Local Player
 */
simulated function IncrementFlashCount(Weapon InWeapon, byte InFiringMode)
{
	bForceNetUpdate = TRUE;	// Force replication
	FlashCount++;

	// Make sure it's not 0, because it means the weapon stopped firing!
	if( FlashCount == 0 )
	{
		FlashCount += 2;
	}
	// Make sure firing mode is updated
	SetFiringMode(InWeapon, InFiringMode);

	// This weapon has fired.
	FlashCountUpdated(InWeapon, FlashCount, FALSE);
}


/**
 * Called when FlashCount has been updated.
 * Trigger appropritate events based on FlashCount's value.
 * = 0 means Weapon Stopped firing
 * > 0 means Weapon just fired
 *
 * Network: ALL
 */
simulated function FlashCountUpdated(Weapon InWeapon, Byte InFlashCount, bool bViaReplication)
{
	//`log( WorldInfo.TimeSeconds @ GetFuncName() @ "FlashCount:" @ FlashCount @ "bViaReplication:" @ bViaReplication );
	if( InFlashCount > 0 )
	{
		WeaponFired(InWeapon, bViaReplication);
	}
	else
	{
		WeaponStoppedFiring(InWeapon, bViaReplication);
	}
}

/**
 * Clear flashCount variable. and call WeaponStoppedFiring event.
 * Call this on the server and local player.
 *
 * Network: Server or Local Player
 */
simulated function ClearFlashCount(Weapon InWeapon)
{
	if( FlashCount != 0 )
	{
		bForceNetUpdate = TRUE;	// Force replication
		FlashCount = 0;

		// This weapon stopped firing
		FlashCountUpdated(InWeapon, FlashCount, FALSE);
	}
}


/**
 * This function sets up the Location of a hit to be replicated to all remote clients.
 * It is also responsible for fudging a shot at (0,0,0).
 *
 * Network: Server only (unless using client-side hit detection)
 */

simulated function SetFlashLocation(Weapon InWeapon, byte InFiringMode, vector NewLoc)
{
	// Make sure 2 consecutive flash locations are different, for replication
	if( NewLoc == LastFiringFlashLocation )
	{
		NewLoc += vect(0,0,1);
	}

	// If we are aiming at the origin, aim slightly up since we use 0,0,0 to denote
	// not firing.
	if( NewLoc == vect(0,0,0) )
	{
		NewLoc = vect(0,0,1);
	}

	bForceNetUpdate = TRUE; // Force replication
	FlashLocation = NewLoc;
	LastFiringFlashLocation = NewLoc;

	// Make sure firing mode is updated
	SetFiringMode(InWeapon, InFiringMode);

	// This weapon has fired.
	FlashLocationUpdated(InWeapon, FlashLocation, FALSE);
}


/**
 * Reset flash location variable. and call stop firing.
 * Network: Server only
 */
function ClearFlashLocation(Weapon InWeapon)
{
	if( !IsZero(FlashLocation) )
	{
		bForceNetUpdate = TRUE;	// Force replication
		FlashLocation = vect(0,0,0);
		FlashLocationUpdated(InWeapon, FlashLocation, FALSE);
	}
}

/**
 * Called when FlashLocation has been updated.
 * Trigger appropritate events based on FlashLocation's value.
 * == (0,0,0) means Weapon Stopped firing
 * != (0,0,0) means Weapon just fired
 *
 * Network: ALL
 */
simulated function FlashLocationUpdated(Weapon InWeapon, Vector InFlashLocation, bool bViaReplication)
{
	//`log( WorldInfo.TimeSeconds @ GetFuncName() @ "FlashLocation:" @ FlashLocation @ "bViaReplication:" @ bViaReplication );
	if( !IsZero(InFlashLocation) )
	{
		WeaponFired(InWeapon, bViaReplication, InFlashLocation);
	}
	else
	{
		WeaponStoppedFiring(InWeapon, bViaReplication);
	}
}

/**
 * Called when a pawn's weapon has fired and is responsibile for
 * delegating the creation of all of the different effects.
 *
 * bViaReplication denotes if this call in as the result of the
 * flashcount/flashlocation being replicated. It's used filter out
 * when to make the effects.
 *
 * Network: ALL
 */
simulated function WeaponFired(Weapon InWeapon, bool bViaReplication, optional vector HitLocation)
{
	// increment number of consecutive shots.
	ShotCount++;

	// By default we just call PlayFireEffects on the weapon.
	if( InWeapon != None )
	{
		InWeapon.PlayFireEffects(GetWeaponFiringMode(InWeapon), HitLocation);
	}
}


/**
 * Called when a pawn's weapon has stopped firing and is responsibile for
 * delegating the destruction of all of the different effects.
 *
 * bViaReplication denotes if this call in as the result of the
 * flashcount/flashlocation being replicated.
 *
 * Network: ALL
 */
simulated function WeaponStoppedFiring(Weapon InWeapon, bool bViaReplication)
{
	// reset number of consecutive shots fired.
	ShotCount = 0;

	if( InWeapon != None )
	{
		InWeapon.StopFireEffects(GetWeaponFiringMode(InWeapon));
	}
}


/**
AI Interface for combat
**/
function bool BotFire(bool bFinished)
{
	StartFire(0);
	return true;
}

function bool CanAttack(Actor Other)
{
	if ( Weapon == None )
		return false;
	return Weapon.CanAttack(Other);
}

function bool TooCloseToAttack(Actor Other)
{
	return false;
}

function bool FireOnRelease()
{
	if (Weapon != None)
		return Weapon.FireOnRelease();

	return false;
}

function bool HasRangedAttack()
{
	return ( Weapon != None );
}

function bool IsFiring()
{
	if (Weapon != None)
		return Weapon.IsFiring();

	return false;
}

/** returns whether we need to turn to fire at the specified location */
function bool NeedToTurn(vector targ)
{
	local vector LookDir, AimDir;

	LookDir = Vector(Rotation);
	LookDir.Z = 0;
	LookDir = Normal(LookDir);
	AimDir = targ - Location;
	AimDir.Z = 0;
	AimDir = Normal(AimDir);

	return ((LookDir Dot AimDir) < 0.93);
}

simulated function String GetHumanReadableName()
{
	if ( PlayerReplicationInfo != None )
		return PlayerReplicationInfo.PlayerName;
	return MenuName;
}

function PlayTeleportEffect(bool bOut, bool bSound)
{
	MakeNoise(1.0);
}

/** NotifyTeamChanged()
Called when PlayerReplicationInfo is replicated to this pawn, or PlayerReplicationInfo team property changes.

Network:  client
*/
simulated function NotifyTeamChanged();

/* PossessedBy()
 Pawn is possessed by Controller
*/
function PossessedBy(Controller C, bool bVehicleTransition)
{
	Controller			= C;
	NetPriority			= 3;
	NetUpdateFrequency	= 100;
	bForceNetUpdate = TRUE;

	if ( C.PlayerReplicationInfo != None )
	{
		PlayerReplicationInfo = C.PlayerReplicationInfo;
	}
	UpdateControllerOnPossess(bVehicleTransition);

	SetOwner(Controller);	// for network replication
	Eyeheight = BaseEyeHeight;

	if ( C.IsA('PlayerController') )
	{
		if ( WorldInfo.NetMode != NM_Standalone )
		{
			RemoteRole = ROLE_AutonomousProxy;
		}

		// inform client of current weapon
		if( Weapon != None )
		{
			Weapon.ClientWeaponSet(FALSE);
		}
	}
	else
	{
		RemoteRole = Default.RemoteRole;
	}


	//Update the AIController cache
	if (Weapon != None)
	{
		Weapon.CacheAIController();
	}
}

/* UpdateControllerOnPossess()
update controller - normally, just change its rotation to match pawn rotation
*/
function UpdateControllerOnPossess(bool bVehicleTransition)
{
	// don't set pawn rotation on possess if was driving vehicle, so face
	// same direction when get out as when driving
	if ( !bVehicleTransition )
	{
		Controller.SetRotation(Rotation);
	}
}

function UnPossessed()
{
	bForceNetUpdate = TRUE;
	if ( DrivenVehicle != None )
		NetUpdateFrequency = 5;

	PlayerReplicationInfo = None;
	SetOwner(None);
	Controller = None;
}

/**
 * returns default camera mode when viewing this pawn.
 * Mainly called when controller possesses this pawn.
 *
 * @param	PlayerController requesting the default camera view
 * @return	default camera view player should use when controlling this pawn.
 */
simulated function name GetDefaultCameraMode( PlayerController RequestedBy )
{
	if ( RequestedBy != None && RequestedBy.PlayerCamera != None && RequestedBy.PlayerCamera.CameraStyle == 'Fixed' )
		return 'Fixed';

	return 'FirstPerson';
}

function DropToGround()
{
	bCollideWorld = True;
	if ( Health > 0 )
	{
		SetCollision(true,true);
		SetPhysics(PHYS_Falling);
		if ( IsHumanControlled() )
			Controller.GotoState(LandMovementState);
	}
}

function bool CanGrabLadder()
{
	return ( bCanClimbLadders
			&& (Controller != None)
			&& (Physics != PHYS_Ladder)
			&& ((Physics != Phys_Falling) || (abs(Velocity.Z) <= JumpZ)) );
}

function bool RecommendLongRangedAttack()
{
	return false;
}

function float RangedAttackTime()
{
	return 0;
}

/**
 * Called every frame from PlayerInput or PlayerController::MoveAutonomous()
 * Sets bIsWalking flag, which defines if the Pawn is walking or not (affects velocity)
 *
 * @param	bNewIsWalking, new walking state.
 */
event SetWalking( bool bNewIsWalking )
{
	if ( bNewIsWalking != bIsWalking )
	{
		bIsWalking = bNewIsWalking;
	}
}

simulated function bool CanSplash()
{
	if ( (WorldInfo.TimeSeconds - SplashTime > 0.15)
		&& ((Physics == PHYS_Falling) || (Physics == PHYS_Flying))
		&& (Abs(Velocity.Z) > 100) )
	{
		SplashTime = WorldInfo.TimeSeconds;
		return true;
	}
	return false;
}

function EndClimbLadder(LadderVolume OldLadder)
{
	if ( Controller != None )
		Controller.EndClimbLadder();
	if ( Physics == PHYS_Ladder )
		SetPhysics(PHYS_Falling);
}

function ClimbLadder(LadderVolume L)
{
	OnLadder = L;
	SetRotation(OnLadder.WallDir);
	SetPhysics(PHYS_Ladder);
	if ( IsHumanControlled() )
		Controller.GotoState('PlayerClimbing');
}

/**
 * list important Pawn variables on canvas.	 HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local string	T;
	local Canvas	Canvas;
	local AnimTree	AnimTreeRootNode;
	local int		i;

	Canvas = HUD.Canvas;

	if ( PlayerReplicationInfo == None )
	{
		Canvas.DrawText("NO PLAYERREPLICATIONINFO", false);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
	}
	else
	{
		PlayerReplicationInfo.DisplayDebug(HUD,out_YL,out_YPos);
	}

	super.DisplayDebug(HUD, out_YL, out_YPos);

	Canvas.SetDrawColor(255,255,255);

	Canvas.DrawText("Health "$Health);
	out_YPos += out_YL;
	Canvas.SetPos(4, out_YPos);

	if (HUD.ShouldDisplayDebug('AI'))
	{
		Canvas.DrawText("Anchor "$Anchor$" Serpentine Dist "$SerpentineDist$" Time "$SerpentineTime);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
	}

	if (HUD.ShouldDisplayDebug('physics'))
	{
		T = "Floor "$Floor$" DesiredSpeed "$DesiredSpeed$" Crouched "$bIsCrouched;
		if ( (OnLadder != None) || (Physics == PHYS_Ladder) )
			T=T$" on ladder "$OnLadder;
		Canvas.DrawText(T);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);

		T = "Collision Component:" @ CollisionComponent;
		Canvas.DrawText(T);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);

		T = "bForceMaxAccel:" @ bForceMaxAccel;
		Canvas.DrawText(T);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);

		if( Mesh != None  )
		{
			T = "RootMotionMode:" @ Mesh.RootMotionMode @ "RootMotionVelocity:" @ Mesh.RootMotionVelocity;
			Canvas.DrawText(T);
			out_YPos += out_YL;
			Canvas.SetPos(4,out_YPos);
		}
	}

	if (HUD.ShouldDisplayDebug('camera'))
	{
		Canvas.DrawText("EyeHeight "$Eyeheight$" BaseEyeHeight "$BaseEyeHeight);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
	}

	// Controller
	if ( Controller == None )
	{
		Canvas.SetDrawColor(255,0,0);
		Canvas.DrawText("NO CONTROLLER");
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
		HUD.PlayerOwner.DisplayDebug(HUD, out_YL, out_YPos);
	}
	else
	{
		Controller.DisplayDebug(HUD, out_YL, out_YPos);
	}

	// Weapon
	if (HUD.ShouldDisplayDebug('weapon'))
	{
		if ( Weapon == None )
		{
			Canvas.SetDrawColor(0,255,0);
			Canvas.DrawText("NO WEAPON");
			out_YPos += out_YL;
			Canvas.SetPos(4, out_YPos);
		}
		else
			Weapon.DisplayDebug(HUD, out_YL, out_YPos);
	}

	if( HUD.ShouldDisplayDebug('animation') )
	{
		if( Mesh != None && Mesh.Animations != None )
		{
			AnimTreeRootNode = AnimTree(Mesh.Animations);
			if( AnimTreeRootNode != None )
			{
				Canvas.DrawText("AnimGroups count:" @ AnimTreeRootNode.AnimGroups.Length);
				out_YPos += out_YL;
				Canvas.SetPos(4,out_YPos);

				for(i=0; i<AnimTreeRootNode.AnimGroups.Length; i++)
				{
					Canvas.DrawText(" GroupName:" @ AnimTreeRootNode.AnimGroups[i].GroupName @ "NodeCount:" @ AnimTreeRootNode.AnimGroups[i].SeqNodes.Length @ "RateScale:" @ AnimTreeRootNode.AnimGroups[i].RateScale);
					out_YPos += out_YL;
					Canvas.SetPos(4,out_YPos);
				}
			}
		}
	}
}

//***************************************
// Interface to Pawn's Controller

/**
 * IsHumanControlled()
 * @param PawnController - optional parameter so you can pass a controller that is associated with this pawn but is not attached to it
 * @return - true if controlled by a real live human on the local machine.  On client, only local player's pawn returns true
*/
simulated final native function bool IsHumanControlled(optional Controller PawnController);

/**
 * @return - true if controlled by local (not network) player
 */
simulated native final function bool IsLocallyControlled();

/** IsPlayerPawn()
return true if controlled by a Player (AI or human) on local machine (any controller on server, localclient's pawn on client)
*/
simulated native function bool IsPlayerPawn() const;

// return true if viewing this pawn in first person pov. useful for determining what and where to spawn effects
simulated function bool IsFirstPerson()
{
	local PlayerController PC;

	PC = PlayerController(Controller);
	return ( PC!=None && PC.UsingFirstPersonCamera() );
}

/**
 * Called from PlayerController UpdateRotation() -> ProcessViewRotation() to (pre)process player ViewRotation
 * adds delta rot (player input), applies any limits and post-processing
 * returns the final ViewRotation set on PlayerController
 *
 * @param	DeltaTime, time since last frame
 * @param	ViewRotation, actual PlayerController view rotation
 * @input	out_DeltaRot, delta rotation to be applied on ViewRotation. Represents player's input.
 * @return	processed ViewRotation to be set on PlayerController.
 */
simulated function ProcessViewRotation( float DeltaTime, out rotator out_ViewRotation, out Rotator out_DeltaRot )
{
	// Add Delta Rotation
	out_ViewRotation	+= out_DeltaRot;
	out_DeltaRot		 = rot(0,0,0);

	// Limit Player View Pitch
	if ( PlayerController(Controller) != None )
	{
		out_ViewRotation = PlayerController(Controller).LimitViewRotation( out_ViewRotation, ViewPitchMin, ViewPitchMax );
	}
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
	out_Location = GetPawnViewLocation();
	out_Rotation = GetViewRotation();
}

/** @return the rotation the Pawn is looking
 */
simulated native event rotator GetViewRotation();

/**
 * returns the Eye location of the Pawn.
 *
 * @return	Pawn's eye location
 */
simulated native event vector GetPawnViewLocation();

/**
 * Return world location to start a weapon fire trace from.
 *
 * @return	World location where to start weapon fire traces from
 */
simulated event Vector GetWeaponStartTraceLocation(optional Weapon CurrentWeapon)
{
	local vector	POVLoc;
	local rotator	POVRot;

	// If we have a controller, by default we start tracing from the player's 'eyes' location
	// that is by default Controller.Location for AI, and camera (crosshair) location for human players.
	if ( Controller != None )
	{
		Controller.GetPlayerViewPoint( POVLoc, POVRot );
		return POVLoc;
	}

	// If we have no controller, we simply traces from pawn eyes location
	return GetPawnViewLocation();
}


/**
 * returns base Aim Rotation without any adjustment (no aim error, no autolock, no adhesion.. just clean initial aim rotation!)
 *
 * @return	base Aim rotation.
 */
simulated singular event Rotator GetBaseAimRotation()
{
	local vector	POVLoc;
	local rotator	POVRot;

	// If we have a controller, by default we aim at the player's 'eyes' direction
	// that is by default Controller.Rotation for AI, and camera (crosshair) rotation for human players.
	if( Controller != None && !InFreeCam() )
	{
		Controller.GetPlayerViewPoint(POVLoc, POVRot);
		return POVRot;
	}

	// If we have no controller, we simply use our rotation
	POVRot = Rotation;

	// If our Pitch is 0, then use RemoveViewPitch
	if( POVRot.Pitch == 0 )
	{
		POVRot.Pitch = RemoteViewPitch << 8;
	}

	return POVRot;
}

/** return true if player is viewing this Pawn in FreeCam */
simulated event bool InFreeCam()
{
	local PlayerController	PC;

	PC = PlayerController(Controller);
	return (PC != None && PC.PlayerCamera != None && (PC.PlayerCamera.CameraStyle == 'FreeCam' || PC.PlayerCamera.CameraStyle == 'FreeCam_Default') );
}

/**
 * Adjusts weapon aiming direction.
 * Gives Pawn a chance to modify its aiming. For example aim error, auto aiming, adhesion, AI help...
 * Requested by weapon prior to firing.
 *
 * @param	W, weapon about to fire
 * @param	StartFireLoc, world location of weapon fire start trace, or projectile spawn loc.
 * @param	BaseAimRot, original aiming rotation without any modifications.
 */
simulated function Rotator GetAdjustedAimFor( Weapon W, vector StartFireLoc )
{
	// If controller doesn't exist or we're a client, get the where the Pawn is aiming at
	if ( Controller == None || Role < Role_Authority )
	{
		return GetBaseAimRotation();
	}

	// otherwise, give a chance to controller to adjust this Aim Rotation
	return Controller.GetAdjustedAimFor( W, StartFireLoc );
}

simulated function SetViewRotation(rotator NewRotation )
{
	if (Controller != None)
	{
		Controller.SetRotation(NewRotation);
	}
	else
	{
		SetRotation(NewRotation);
	}
}

function bool InGodMode()
{
	return ( (Controller != None) && Controller.bGodMode );
}

function SetMoveTarget(Actor NewTarget )
{
	if ( Controller != None )
		Controller.MoveTarget = NewTarget;
}

function bool LineOfSightTo(actor Other)
{
	return ( (Controller != None) && Controller.LineOfSightTo(Other) );
}

function HandlePickup(Inventory Inv)
{
	MakeNoise(0.2);
	if ( Controller != None )
		Controller.HandlePickup(Inv);
}

event ClientMessage( coerce string S, optional Name Type )
{
	if ( PlayerController(Controller) != None )
		PlayerController(Controller).ClientMessage( S, Type );
}

simulated event FellOutOfWorld(class<DamageType> dmgType)
{
	if ( Role == ROLE_Authority )
	{
		Health = -1;
		Died( None, dmgType, Location );
		if ( dmgType == None )
		{
			SetPhysics(PHYS_None);
			SetHidden(True);
			LifeSpan = FMin(LifeSpan, 1.0);
		}
	}
}

simulated singular event OutsideWorldBounds()
{
	// AI pawns on the server just destroy
	if (Role == ROLE_Authority && PlayerController(Controller) == None)
	{
		Destroy();
	}
	else
	{
		// simply destroying the Pawn could cause synchronization issues with the client controlling it
		// so kill it, disable it, and wait a while to give it time to replicate before destroying it
		if (Role == ROLE_Authority)
		{
			KilledBy(self);
		}
		SetPhysics(PHYS_None);
		SetHidden(True);
		LifeSpan = FMin(LifeSpan, 1.0);
    }
}

/**
 * Makes sure a Pawn is not crouching, telling it to stand if necessary.
 */
simulated function UnCrouch()
{
	if( bIsCrouched || bWantsToCrouch )
	{
		ShouldCrouch( false );
	}
}

/**
 * Controller is requesting that pawn crouches.
 * This is not guaranteed as it depends if crouching collision cylinder can fit when Pawn is located.
 *
 * @param	bCrouch		true if Pawn should crouch.
 */
function ShouldCrouch( bool bCrouch )
{
	bWantsToCrouch = bCrouch;
}

/**
 * Event called from native code when Pawn stops crouching.
 * Called on non owned Pawns through bIsCrouched replication.
 * Network: ALL
 *
 * @param	HeightAdjust	height difference in unreal units between default collision height, and actual crouched cylinder height.
 */
simulated event EndCrouch( float HeightAdjust )
{
	EyeHeight -= HeightAdjust;
	SetBaseEyeHeight();
}

/**
 * Event called from native code when Pawn starts crouching.
 * Called on non owned Pawns through bIsCrouched replication.
 * Network: ALL
 *
 * @param	HeightAdjust	height difference in unreal units between default collision height, and actual crouched cylinder height.
 */
simulated event StartCrouch( float HeightAdjust )
{
	EyeHeight += HeightAdjust;
	SetBaseEyeHeight();
}

function HandleMomentum( vector Momentum, Vector HitLocation, class<DamageType> DamageType, optional TraceHitInfo HitInfo )
{
	AddVelocity( Momentum, HitLocation, DamageType, HitInfo );
}

function AddVelocity( vector NewVelocity, vector HitLocation, class<DamageType> damageType, optional TraceHitInfo HitInfo )
{
	if ( bIgnoreForces || (NewVelocity == vect(0,0,0)) )
		return;
	if ( (Physics == PHYS_Walking)
		|| (((Physics == PHYS_Ladder) || (Physics == PHYS_Spider)) && (NewVelocity.Z > Default.JumpZ)) )
		SetPhysics(PHYS_Falling);
	if ( (Velocity.Z > Default.JumpZ) && (NewVelocity.Z > 0) )
		NewVelocity.Z *= 0.5;
	Velocity += NewVelocity;
}

function KilledBy( pawn EventInstigator )
{
	local Controller Killer;

	Health = 0;
	if ( EventInstigator != None )
	{
		Killer = EventInstigator.Controller;
		LastHitBy = None;
	}
	Died( Killer, class'DmgType_Suicided', Location );
}

function TakeFallingDamage()
{
	local float EffectiveSpeed;

	if (Velocity.Z < -0.5 * MaxFallSpeed)
	{
		if ( Role == ROLE_Authority )
		{
			MakeNoise(1.0);
			if (Velocity.Z < -1 * MaxFallSpeed)
			{
				EffectiveSpeed = Velocity.Z;
				if (TouchingWaterVolume())
				{
					EffectiveSpeed += 100;
				}
				if (EffectiveSpeed < -1 * MaxFallSpeed)
				{
					TakeDamage(-100 * (EffectiveSpeed + MaxFallSpeed)/MaxFallSpeed, None, Location, vect(0,0,0), class'DmgType_Fell');
				}
				}
		}
	}
	else if (Velocity.Z < -1.4 * JumpZ)
		MakeNoise(0.5);
	else if ( Velocity.Z < -0.8 * JumpZ )
		MakeNoise(0.2);
}

function Restart();

simulated function ClientReStart()
{
	ZeroMovementVariables();
	SetBaseEyeHeight();
}

function ClientSetRotation( rotator NewRotation )
{
	if ( Controller != None )
		Controller.ClientSetRotation(NewRotation);
}

/** Script function callable from C++ to update the Pawn's rotation, and goes through the FaceRotation logic to apply rotation constraints */
final event simulated UpdatePawnRotation(Rotator NewRotation)
{
	FaceRotation(NewRotation, 0.f);
}

simulated function FaceRotation(rotator NewRotation, float DeltaTime)
{
	// Do not update Pawn's rotation depending on controller's ViewRotation if in FreeCam.
	if (!InFreeCam())
	{
		if ( Physics == PHYS_Ladder )
		{
			NewRotation = OnLadder.Walldir;
		}
		else if ( (Physics == PHYS_Walking) || (Physics == PHYS_Falling) )
		{
			NewRotation.Pitch = 0;
		}

		SetRotation(NewRotation);
	}
}

//==============
// Encroachment
event bool EncroachingOn( actor Other )
{
	if ( Other.bWorldGeometry || Other.bBlocksTeleport )
		return true;

	if ( ((Controller == None) || !Controller.bIsPlayer) && (Pawn(Other) != None) )
		return true;

	return false;
}

event EncroachedBy( actor Other )
{
	// Allow encroachment by Vehicles so they can push the pawn out of the way
	if ( Pawn(Other) != None && Vehicle(Other) == None )
		gibbedBy(Other);
}

function gibbedBy(actor Other)
{
	if ( Role < ROLE_Authority )
		return;
	if ( Pawn(Other) != None )
		Died(Pawn(Other).Controller, class'DmgType_Telefragged', Location);
	else
		Died(None, class'DmgType_Telefragged', Location);
}

//Base change - if new base is pawn or decoration, damage based on relative mass and old velocity
// Also, non-players will jump off pawns immediately
function JumpOffPawn()
{
	Velocity += (100 + CylinderComponent.CollisionRadius) * VRand();
	if ( VSize2D(Velocity) > FMax(500.0, GroundSpeed) )
	{
		Velocity = FMax(500.0, GroundSpeed) * Normal(Velocity);
	}
	Velocity.Z = 200 + CylinderComponent.CollisionHeight;
	SetPhysics(PHYS_Falling);
}

/** Called when pawn cylinder embedded in another pawn.  (Collision bug that needs to be fixed).
*/
event StuckOnPawn(Pawn OtherPawn);

/**
  * Event called after actor's base changes.
*/
singular event BaseChange()
{
	local DynamicSMActor Dyn;

	// Pawns can only set base to non-pawns, or pawns which specifically allow it.
	// Otherwise we do some damage and jump off.
	if (Pawn(Base) != None && (DrivenVehicle == None || !DrivenVehicle.IsBasedOn(Base)))
	{
		if( !Pawn(Base).CanBeBaseForPawn(Self) )
		{
			Pawn(Base).CrushedBy(self);
			JumpOffPawn();
		}
	}

	// If it's a KActor, see if we can stand on it.
	Dyn = DynamicSMActor(Base);
	if( Dyn != None && !Dyn.CanBasePawn(self) )

	{
		JumpOffPawn();
	}
}


/**
 * Are we allowing this Pawn to be based on us?
 */
simulated function bool CanBeBaseForPawn(Pawn APawn)
{
	return bCanBeBaseForPawns;
}

/** CrushedBy()
Called for pawns that have bCanBeBaseForPawns=false when another pawn becomes based on them
*/
function CrushedBy(Pawn OtherPawn)
{
	TakeDamage( (1-OtherPawn.Velocity.Z/400)* OtherPawn.Mass/Mass, OtherPawn.Controller,Location, vect(0,0,0) , class'DmgType_Crushed');
}

//=============================================================================

/**
 * Call this function to detach safely pawn from its controller
 *
 * @param	bDestroyController	if true, then destroy controller. (only AI Controllers, not players)
 */
function DetachFromController( optional bool bDestroyController )
{
	local Controller OldController;

	// if we have a controller, notify it we're getting destroyed
	// be careful with bTearOff, we're authority on client! Make sure our controller and pawn match up.
	if ( Controller != None && Controller.Pawn == Self )
	{
		OldController = Controller;
		Controller.PawnDied( Self );
		if ( Controller != None )
		{
			Controller.UnPossess();
		}

		if ( bDestroyController && OldController != None && !OldController.bDeleteMe && !OldController.bIsPlayer )
		{
			OldController.Destroy();
		}
		Controller = None;
	}
}

simulated event Destroyed()
{
	DetachFromController();

	if ( InvManager != None )
		InvManager.Destroy();

	if ( WorldInfo.NetMode == NM_Client )
		return;

	// Clear anchor to avoid checkpoint crash
	SetAnchor( None );

	Weapon = None;

	//debug
	ClearPathStep();

	super.Destroyed();
}

//=============================================================================
//
// Called immediately before gameplay begins.
//
simulated event PreBeginPlay()
{
	// important that this comes before Super so mutators can modify it
	if (HealthMax == 0)
	{
		HealthMax = default.Health;
	}

	Super.PreBeginPlay();

	Instigator = self;
	SetDesiredRotation(Rotation);
	EyeHeight = BaseEyeHeight;
}

event PostBeginPlay()
{
	super.PostBeginPlay();

	SplashTime = 0;
	SpawnTime = WorldInfo.TimeSeconds;
	EyeHeight	= BaseEyeHeight;

	// automatically add controller to pawns which were placed in level
	// NOTE: pawns spawned during gameplay are not automatically possessed by a controller
	if ( WorldInfo.bStartup && (Health > 0) && !bDontPossess )
	{
		SpawnDefaultController();
	}

	if( FacialAudioComp != None )
	{
		FacialAudioComp.OnAudioFinished = FaceFXAudioFinished;
	}

	// Spawn Inventory Container
	if (Role == ROLE_Authority && InvManager == None && InventoryManagerClass != None)
	{
		InvManager = Spawn(InventoryManagerClass, Self);
		if ( InvManager == None )
			`log("Warning! Couldn't spawn InventoryManager" @ InventoryManagerClass @ "for" @ Self @ GetHumanReadableName() );
		else
			InvManager.SetupFor( Self );
	}

	//debug
	ClearPathStep();
}


/**
 * Spawn default controller for this Pawn, get possessed by it.
 */
function SpawnDefaultController()
{
	if ( Controller != None )
	{
		`log("SpawnDefaultController" @ Self @ ", Controller != None" @ Controller );
		return;
	}

	if ( ControllerClass != None )
	{
		Controller = Spawn(ControllerClass);
	}

	if ( Controller != None )
	{
		Controller.Possess( Self, false );
	}
}

simulated event ReceivedNewEvent(SequenceEvent Evt)
{
	if (Controller != None)
	{
		Controller.ReceivedNewEvent(Evt);
	}
	Super.ReceivedNewEvent(Evt);
}

/**
 * Deletes the current controller if it exists and creates a new one
 * using the specified class.
 * Event called from Kismet.
 *
 * @param		inAction - scripted action that was activated
 */
function OnAssignController(SeqAct_AssignController inAction)
{

	if ( inAction.ControllerClass != None )
	{
		if ( Controller != None )
		{
			DetachFromController( true );
		}

		Controller = Spawn(inAction.ControllerClass);
		Controller.Possess( Self, false );

		// Set class as the default one if pawn is restarted.
		if ( Controller.IsA('AIController') )
		{
			ControllerClass = class<AIController>(Controller.Class);
		}
	}
	else
	{
		`warn("Assign controller w/o a class specified!");
	}
}

/**
 * Iterates through the list of item classes specified in the action
 * and creates instances that are addeed to this Pawn's inventory.
 *
 * @param		inAction - scripted action that was activated
 */
simulated function OnGiveInventory(SeqAct_GiveInventory InAction)
{
	local int Idx;
	local class<Inventory> InvClass;

	if (InAction.bClearExisting)
	{
		InvManager.DiscardInventory();
	}

	if (InAction.InventoryList.Length > 0 )
	{
		for (Idx = 0; Idx < InAction.InventoryList.Length; Idx++)
		{
			InvClass = InAction.InventoryList[idx];
			if (InvClass != None)
			{
				// only create if it doesn't already exist
				if (FindInventoryType(InvClass,FALSE) == None)
				{
					CreateInventory(InvClass);
				}
			}
			else
			{
				InAction.ScriptLog("WARNING: Attempting to give NULL inventory!");
			}
		}
	}
	else
	{
		InAction.ScriptLog("WARNING: Give Inventory without any inventory specified!");
	}
}

function Gasp();

function SetMovementPhysics()
{
	// check for water volume
	if (PhysicsVolume.bWaterVolume)
	{
		SetPhysics(PHYS_Swimming);
	}
	else if (Physics != PHYS_Falling)
	{
		SetPhysics(PHYS_Falling);
	}
}

/* AdjustDamage()
adjust damage based on inventory, other attributes
*/
function AdjustDamage(out int InDamage, out vector Momentum, Controller InstigatedBy, vector HitLocation, class<DamageType> DamageType, TraceHitInfo HitInfo, Actor DamageCauser);

event bool HealDamage(int Amount, Controller Healer, class<DamageType> DamageType)
{
	// not if already dead or already at full
	if (Health > 0 && Health < HealthMax)
	{
		Health = Min(HealthMax, Health + Amount);
		return true;
	}
	else
	{
		return false;
	}
}

/** Take a list of bones passed to TakeRadiusDamageOnBones and remove the ones that don't matter */
function PruneDamagedBoneList( out array<Name> Bones );

/**
 *	Damage radius applied to specific bones on the skeletal mesh
 */
event bool TakeRadiusDamageOnBones
(
 Controller			InstigatedBy,
 float				BaseDamage,
 float				DamageRadius,
class<DamageType>	DamageType,
	float				Momentum,
	vector				HurtOrigin,
	bool				bFullDamage,
	Actor				DamageCauser,
	array<Name>			Bones
	)
{

	local int			Idx;
	local TraceHitInfo	HitInfo;
	local bool			bResult;
	local float			DamageScale, Dist;
	local vector		Dir, BoneLoc;

	PruneDamagedBoneList( Bones );

	for( Idx = 0; Idx < Bones.Length; Idx++ )
	{
		HitInfo.BoneName	 = Bones[Idx];
		HitInfo.HitComponent = Mesh;

		BoneLoc = Mesh.GetBoneLocation(Bones[Idx]);
		Dir		= BoneLoc - HurtOrigin;
		Dist	= VSize(Dir);
		Dir		= Normal(Dir);
		if( bFullDamage )
		{
			DamageScale = 1.f;
		}
		else
		{
			DamageScale = 1.f - Dist/DamageRadius;
		}

		if( DamageScale > 0.f )
		{
			TakeDamage
			(
				DamageScale * BaseDamage,
				InstigatedBy,
				BoneLoc,
				DamageScale * Momentum * Dir,
				DamageType,
				HitInfo,
				DamageCauser
			);
		}

		bResult = TRUE;
	}

	return bResult;
}

/** sends any notifications to anything that needs to know this pawn has taken damage */
function NotifyTakeHit(Controller InstigatedBy, vector HitLocation, int Damage, class<DamageType> DamageType, vector Momentum, Actor DamageCauser)
{
	if (Controller != None)
	{
		Controller.NotifyTakeHit(InstigatedBy, HitLocation, Damage, DamageType, Momentum);
	}
}

function controller SetKillInstigator(Controller InstigatedBy, class<DamageType> DamageType)
{
	if ( (InstigatedBy != None) && (InstigatedBy != Controller) )
	{
		return InstigatedBy;
	}
	else if ( DamageType.default.bCausedByWorld && (LastHitBy != None) )
	{
		return LastHitBy;
	}
	return InstigatedBy;
}

event TakeDamage(int Damage, Controller InstigatedBy, vector HitLocation, vector Momentum, class<DamageType> DamageType, optional TraceHitInfo HitInfo, optional Actor DamageCauser)
{
	local int actualDamage;
	local PlayerController PC;
	local Controller Killer;

	if ( (Role < ROLE_Authority) || (Health <= 0) )
	{
		return;
	}

	if ( damagetype == None )
	{
		if ( InstigatedBy == None )
			`warn("No damagetype for damage with no instigator");
		else
			`warn("No damagetype for damage by "$instigatedby.pawn$" with weapon "$InstigatedBy.Pawn.Weapon);
		//scripttrace();
		DamageType = class'DamageType';
	}
	Damage = Max(Damage, 0);

	if (Physics == PHYS_None && DrivenVehicle == None)
	{
		SetMovementPhysics();
	}
	if (Physics == PHYS_Walking && damageType.default.bExtraMomentumZ)
	{
		momentum.Z = FMax(momentum.Z, 0.4 * VSize(momentum));
	}
	momentum = momentum/Mass;

	if ( DrivenVehicle != None )
	{
		DrivenVehicle.AdjustDriverDamage( Damage, InstigatedBy, HitLocation, Momentum, DamageType );
	}

	ActualDamage = Damage;
	WorldInfo.Game.ReduceDamage(ActualDamage, self, instigatedBy, HitLocation, Momentum, DamageType, DamageCauser);
	AdjustDamage(ActualDamage, Momentum, instigatedBy, HitLocation, DamageType, HitInfo, DamageCauser);

	// call Actor's version to handle any SeqEvent_TakeDamage for scripting
	Super.TakeDamage(ActualDamage, InstigatedBy, HitLocation, Momentum, DamageType, HitInfo, DamageCauser);

	Health -= actualDamage;
	if (HitLocation == vect(0,0,0))
	{
		HitLocation = Location;
	}

	if ( Health <= 0 )
	{
		PC = PlayerController(Controller);
		// play force feedback for death
		if (PC != None)
		{
			PC.ClientPlayForceFeedbackWaveform(damageType.default.KilledFFWaveform);
		}
		// pawn died
		Killer = SetKillInstigator(InstigatedBy, DamageType);
		TearOffMomentum = momentum;
		Died(Killer, damageType, HitLocation);
	}
	else
	{
		HandleMomentum( momentum, HitLocation, DamageType, HitInfo );
		NotifyTakeHit(InstigatedBy, HitLocation, ActualDamage, DamageType, Momentum, DamageCauser);
		if (DrivenVehicle != None)
		{
			DrivenVehicle.NotifyDriverTakeHit(InstigatedBy, HitLocation, actualDamage, DamageType, Momentum);
		}
		if ( instigatedBy != None && instigatedBy != controller )
		{
			LastHitBy = instigatedBy;
		}
	}
	PlayHit(actualDamage,InstigatedBy, hitLocation, damageType, Momentum, HitInfo);
	MakeNoise(1.0);
}

/*
 * Queries the PRI and returns our current team index.
 */
simulated native function byte GetTeamNum();


simulated function TeamInfo GetTeam()
{
	if (Controller != None && Controller.PlayerReplicationInfo != None)
	{
		return Controller.PlayerReplicationInfo.Team;
	}
	else if (PlayerReplicationInfo != None)
	{
		return PlayerReplicationInfo.Team;
	}
	else if (DrivenVehicle != None && DrivenVehicle.PlayerReplicationInfo != None)
	{
		return DrivenVehicle.PlayerReplicationInfo.Team;
	}
	else
	{
		return None;
	}
}

/** Returns true of pawns are on the same team, false otherwise */
simulated event bool IsSameTeam( Pawn Other )
{
	 return ( Other != None &&
		Other.GetTeam() != None &&
		Other.GetTeam() == GetTeam() );
}

/** called to throw any weapon(s) that should be thrown on death */
function ThrowWeaponOnDeath()
{
	ThrowActiveWeapon();
}

/**
 * This pawn has died.
 *
 * @param	Killer			Who killed this pawn
 * @param	DamageType		What killed it
 * @param	HitLocation		Where did the hit occur
 *
 * @returns true if allowed
 */
function bool Died(Controller Killer, class<DamageType> DamageType, vector HitLocation)
{
	local SeqAct_Latent Action;

	// ensure a valid damagetype
	if ( damageType == None )
	{
		damageType = class'DamageType';
	}
	// if already destroyed or level transition is occuring then ignore
	if ( bDeleteMe || WorldInfo.Game == None || WorldInfo.Game.bLevelChange )
	{
		return FALSE;
	}
	// if this is an environmental death then refer to the previous killer so that they receive credit (knocked into lava pits, etc)
	if ( DamageType.default.bCausedByWorld && (Killer == None || Killer == Controller) && LastHitBy != None )
	{
		Killer = LastHitBy;
	}
	// gameinfo hook to prevent deaths
	// WARNING - don't prevent bot suicides - they suicide when really needed
	if ( WorldInfo.Game.PreventDeath(self, Killer, damageType, HitLocation) )
	{
		Health = max(Health, 1);
		return false;
	}
	Health = Min(0, Health);
	
	// activate death events
	if( default.KismetDeathDelayTime > 0 )
	{
		DelayTriggerDeath();
	}
	else
	{
		TriggerEventClass( class'SeqEvent_Death', self );
	}

	KismetDeathDelayTime = default.KismetDeathDelayTime + WorldInfo.TimeSeconds;

	// and abort any latent actions
	foreach LatentActions(Action)
	{
		Action.AbortFor(self);
	}
	LatentActions.Length = 0;
	// notify the vehicle we are currently driving
	if ( DrivenVehicle != None )
	{
		Velocity = DrivenVehicle.Velocity;
		DrivenVehicle.DriverDied(DamageType);
	}
	else if ( Weapon != None )
	{
		Weapon.HolderDied();
		ThrowWeaponOnDeath();
	}
	// notify the gameinfo of the death
	if ( Controller != None )
	{
		WorldInfo.Game.Killed(Killer, Controller, self, damageType);
	}
	else
	{
		WorldInfo.Game.Killed(Killer, Controller(Owner), self, damageType);
	}
	DrivenVehicle = None;
	// notify inventory manager
	if ( InvManager != None )
	{
		InvManager.OwnerDied();
	}
	// push the corpse upward (@fixme - somebody please remove this?)
	Velocity.Z *= 1.3;
	// if this is a human player then force a replication update
	if ( IsHumanControlled() )
	{
		PlayerController(Controller).ForceDeathUpdate();
	}
	NetUpdateFrequency = Default.NetUpdateFrequency;
	PlayDying(DamageType, HitLocation);
	return TRUE;
}

function DelayTriggerDeath()
{
	TriggerEventClass( class'SeqEvent_Death', self );
}

event Falling();

event Landed(vector HitNormal, Actor FloorActor)
{
	TakeFallingDamage();
	if ( Health > 0 )
		PlayLanded(Velocity.Z);
	LastHitBy = None;
}

/** 
  *  Called if bScriptTickSpecial is set
  *  This tick function is called after physics has executed
  */
event TickSpecial( float DeltaTime );

event HeadVolumeChange(PhysicsVolume newHeadVolume)
{
	if ( (WorldInfo.NetMode == NM_Client) || (Controller == None) )
		return;
	if ( HeadVolume != None && HeadVolume.bWaterVolume )
	{
		if (!newHeadVolume.bWaterVolume)
		{
			if ( Controller.bIsPlayer && (BreathTime > 0) && (BreathTime < 8) )
				Gasp();
			BreathTime = -1.0;
		}
	}
	else if ( newHeadVolume.bWaterVolume )
	{
		BreathTime = UnderWaterTime;
	}
}

function bool TouchingWaterVolume()
{
	local PhysicsVolume V;

	ForEach TouchingActors(class'PhysicsVolume',V)
		if ( V.bWaterVolume )
			return true;

	return false;
}

event BreathTimer()
{
	if ( HeadVolume.bWaterVolume )
	{
		if ( (Health < 0) || (WorldInfo.NetMode == NM_Client) || (DrivenVehicle != None) )
			return;
		TakeDrowningDamage();
		if ( Health > 0 )
			BreathTime = 2.0;
	}
	else
	{
		BreathTime = 0.0;
	}
}

function TakeDrowningDamage();

function bool CheckWaterJump(out vector WallNormal)
{
	local actor HitActor;
	local vector HitLocation, HitNormal, Checkpoint, start, checkNorm, Extent;

	if ( AIController(Controller) != None )
	{
		if ( Controller.InLatentExecution(Controller.LATENT_MOVETOWARD) && (Controller.Movetarget != None)
			&& !Controller.MoveTarget.PhysicsVolume.bWaterVolume )
		{
			CheckPoint = Normal(Controller.MoveTarget.Location - Location);
		}
		else
		{
			Checkpoint = Acceleration;
		}
		Checkpoint.Z = 0.0;
	}
	if ( Checkpoint == vect(0,0,0) )
	{
		Checkpoint = vector(Rotation);
	}
	Checkpoint.Z = 0.0;
	checkNorm = Normal(Checkpoint);
	Checkpoint = Location + 1.2 * CylinderComponent.CollisionRadius * checkNorm;
	Extent = CylinderComponent.CollisionRadius * vect(1,1,0);
	Extent.Z = CylinderComponent.CollisionHeight;
	HitActor = Trace(HitLocation, HitNormal, Checkpoint, Location, true, Extent,,TRACEFLAG_Blocking);
	if ( (HitActor != None) && (Pawn(HitActor) == None) )
	{
		WallNormal = -1 * HitNormal;
		start = Location;
		start.Z += MaxOutOfWaterStepHeight;
		checkPoint = start + 3.2 * CylinderComponent.CollisionRadius * WallNormal;
		HitActor = Trace(HitLocation, HitNormal, Checkpoint, start, true,,,TRACEFLAG_Blocking);
		if ( (HitActor == None) || (HitNormal.Z > 0.7) )
			return true;
	}

	return false;
}

//Player Jumped
function bool DoJump( bool bUpdating )
{
	if (bJumpCapable && !bIsCrouched && !bWantsToCrouch && (Physics == PHYS_Walking || Physics == PHYS_Ladder || Physics == PHYS_Spider))
	{
		if ( Physics == PHYS_Spider )
			Velocity = JumpZ * Floor;
		else if ( Physics == PHYS_Ladder )
			Velocity.Z = 0;
		else if ( bIsWalking )
			Velocity.Z = Default.JumpZ;
		else
			Velocity.Z = JumpZ;
		if (Base != None && !Base.bWorldGeometry && Base.Velocity.Z > 0.f)
		{
			Velocity.Z += Base.Velocity.Z;
		}
		SetPhysics(PHYS_Falling);
		return true;
	}
	return false;
}

function PlayDyingSound();

function PlayHit(float Damage, Controller InstigatedBy, vector HitLocation, class<DamageType> damageType, vector Momentum, TraceHitInfo HitInfo)
{
	if ( (Damage <= 0) && ((Controller == None) || !Controller.bGodMode) )
		return;

	LastPainTime = WorldInfo.TimeSeconds;
}

/** TurnOff()
Freeze pawn - stop sounds, animations, physics, weapon firing
*/
simulated function TurnOff()
{
	if (Role == ROLE_Authority)
	{
		RemoteRole = ROLE_SimulatedProxy;
	}
	if (WorldInfo.NetMode != NM_DedicatedServer && Mesh != None)
	{
		Mesh.bPauseAnims = true;
		if (Physics == PHYS_RigidBody)
		{
			Mesh.PhysicsWeight = 1.0;
			Mesh.bUpdateKinematicBonesFromAnimation = false;
		}
	}
	SetCollision(true,false);
	bNoWeaponFiring = true;
	Velocity = vect(0,0,0);
	SetPhysics(PHYS_None);
	bIgnoreForces = true;
	if (Weapon != None)
	{
		Weapon.StopFire(Weapon.CurrentFireMode);
	}

}

/**
  * Set physics for dying pawn
  * Always set to falling, unless already a ragdoll
  */
function SetDyingPhysics()
{
	if( Physics != PHYS_RigidBody )
	{
		SetPhysics(PHYS_Falling);
	}
}

State Dying
{
ignores Bump, HitWall, HeadVolumeChange, PhysicsVolumeChange, Falling, BreathTimer, FellOutOfWorld;

	simulated function PlayWeaponSwitch(Weapon OldWeapon, Weapon NewWeapon) {}
	simulated function PlayNextAnimation() {}
	singular event BaseChange() {}
	event Landed(vector HitNormal, Actor FloorActor) {}

	function bool Died(Controller Killer, class<DamageType> damageType, vector HitLocation);

	  simulated singular event OutsideWorldBounds()
	  {
		  SetPhysics(PHYS_None);
		  SetHidden(True);
		  LifeSpan = FMin(LifeSpan, 1.0);
	  }

	event Timer()
	{
		if ( !PlayerCanSeeMe() )
		{
			Destroy();
		}
		else
		{
			SetTimer(2.0, false);
		}
	}

	event TakeDamage(int Damage, Controller EventInstigator, vector HitLocation, vector Momentum, class<DamageType> DamageType, optional TraceHitInfo HitInfo, optional Actor DamageCauser)
	{
		SetPhysics(PHYS_Falling);

		if ( (Physics == PHYS_None) && (Momentum.Z < 0) )
			Momentum.Z *= -1;

		Velocity += 3 * momentum/(Mass + 200);

		if ( damagetype == None )
		{
			// `warn("No damagetype for damage by "$instigatedby.pawn$" with weapon "$InstigatedBy.Pawn.Weapon);
			DamageType = class'DamageType';
		}

		Health -= Damage;
	}

	event BeginState(Name PreviousStateName)
	{
		local Actor A;
		local array<SequenceEvent> TouchEvents;
		local int i;

		if ( bTearOff && (WorldInfo.NetMode == NM_DedicatedServer) )
		{
			LifeSpan = 2.0;
		}
		else
		{
			SetTimer(5.0, false);
			// add a failsafe termination
			LifeSpan = 25.f;
		}

		SetDyingPhysics();

		SetCollision(true, false);

		if ( Controller != None )
		{
			if ( Controller.bIsPlayer )
			{
				DetachFromController();
			}
			else
			{
				Controller.Destroy();
			}
		}

		foreach TouchingActors(class'Actor', A)
		{
			if (A.FindEventsOfClass(class'SeqEvent_Touch', TouchEvents))
			{
				for (i = 0; i < TouchEvents.length; i++)
				{
					SeqEvent_Touch(TouchEvents[i]).NotifyTouchingPawnDied(self);
				}
				// clear array for next iteration
				TouchEvents.length = 0;
			}
		}
		foreach BasedActors(class'Actor', A)
		{
			A.PawnBaseDied();
		}
	}

Begin:
	Sleep(0.2);
	PlayDyingSound();
}

//=============================================================================
// Animation interface for controllers

/* PlayXXX() function called by controller to play transient animation actions
*/
/* PlayDying() is called on server/standalone game when killed
and also on net client when pawn gets bTearOff set to true (and bPlayedDeath is false)
*/
simulated function PlayDying(class<DamageType> DamageType, vector HitLoc)
{
	GotoState('Dying');
	bReplicateMovement = false;
	bTearOff = true;
	Velocity += TearOffMomentum;
	SetDyingPhysics();
	bPlayedDeath = true;

	KismetDeathDelayTime = default.KismetDeathDelayTime + WorldInfo.TimeSeconds;
}

simulated event TornOff()
{
	// assume dead if bTearOff
	if ( !bPlayedDeath )
	{
		PlayDying(HitDamageType,TakeHitLocation);
	}
}

/**
 * PlayFootStepSound()
 * called by AnimNotify_Footstep
 *
 * FootDown specifies which foot hit
 */
event PlayFootStepSound(int FootDown);

//=============================================================================
// Pawn internal animation functions

// Animation group checks (usually implemented in subclass)
function bool CannotJumpNow()
{
	return false;
}

function PlayLanded(float impactVel);

native function Vehicle GetVehicleBase();

function Suicide()
{
	KilledBy(self);
}

// toss out a weapon
// check before throwing
simulated function bool CanThrowWeapon()
{
	return ( (Weapon != None) && Weapon.CanThrow() );
}

/************************************************************************************
 * Vehicle driving
 ***********************************************************************************/


/**
 * StartDriving() and StopDriving() also called on clients
 * on transitions of DrivenVehicle variable.
 * Network: ALL
 */
simulated event StartDriving(Vehicle V)
{
	StopFiring();
	if ( Health <= 0 )
		return;

	DrivenVehicle = V;
	bForceNetUpdate = TRUE;

	// Move the driver into position, and attach to car.
	ShouldCrouch(false);
	bIgnoreForces = true;
	bCanTeleport = false;
	BreathTime = 0.0;
	V.AttachDriver( Self );
}

/**
 * StartDriving() and StopDriving() also called on clients
 * on transitions of DrivenVehicle variable.
 * Network: ALL
 */
simulated event StopDriving(Vehicle V)
{
	if ( Mesh != None )
	{
		Mesh.SetCullDistance(Default.Mesh.CachedMaxDrawDistance);
		Mesh.SetShadowParent(None);
	}
	bForceNetUpdate = TRUE;
	if (V != None  )
	{
		V.StopFiring();
	}

	if ( Physics == PHYS_RigidBody )
	{
		return;
	}

	DrivenVehicle = None;
	bIgnoreForces = false;
	SetHardAttach(false);
	bCanTeleport = true;
	bCollideWorld = true;

	if ( V != None )
	{
		V.DetachDriver( Self );
	}

	SetCollision(true, true);

	if ( Role == ROLE_Authority )
	{
		if ( PhysicsVolume.bWaterVolume && (Health > 0) )
		{
			SetPhysics(PHYS_Swimming);
		}
		else
		{
			SetPhysics(PHYS_Falling);
		}
		SetBase(None);
		SetHidden(False);
	}
}

//
// Inventory related functions
//

/* AddDefaultInventory:
	Add Pawn default Inventory.
	Called from GameInfo.AddDefaultInventory()
*/
function AddDefaultInventory();

/* epic ===============================================
* ::CreateInventory
*
* Create Inventory Item, adds it to the Pawn's Inventory
* And returns it for post processing.
*
* =====================================================
*/
event final Inventory CreateInventory( class<Inventory> NewInvClass, optional bool bDoNotActivate )
{
	if ( InvManager != None )
		return InvManager.CreateInventory( NewInvClass, bDoNotActivate );

	return None;
}

/* FindInventoryType:
	returns the inventory item of the requested class if it exists in this Pawn's inventory
*/
simulated final function Inventory FindInventoryType(class<Inventory> DesiredClass, optional bool bAllowSubclass)
{
	return (InvManager != None) ? InvManager.FindInventoryType(DesiredClass, bAllowSubclass) : None;
}

/** Hook called from HUD actor. Gives access to HUD and Canvas */
simulated function DrawHUD( HUD H )
{
	if ( InvManager != None )
	{
		InvManager.DrawHUD( H );
	}
}

/**
 * Toss active weapon using default settings (location+velocity).
 *
 * @param DamageType  allows this function to do different behaviors based on the damage type
 */
function ThrowActiveWeapon( optional bool bDestroyWeap )
{
	if ( Weapon != None )
	{
		TossInventory(Weapon);
	}
}

function TossInventory(Inventory Inv, optional vector ForceVelocity)
{
	local vector	POVLoc, TossVel;
	local rotator	POVRot;
	local Vector	X,Y,Z;

	if ( ForceVelocity != vect(0,0,0) )
	{
		TossVel = ForceVelocity;
	}
	else
	{
		GetActorEyesViewPoint(POVLoc, POVRot);
		TossVel = Vector(POVRot);
		TossVel = TossVel * ((Velocity Dot TossVel) + 500) + Vect(0,0,200);
	}

	GetAxes(Rotation, X, Y, Z);
	Inv.DropFrom(Location + 0.8 * CylinderComponent.CollisionRadius * X - 0.5 * CylinderComponent.CollisionRadius * Y, TossVel);
}

/* SetActiveWeapon
	Set this weapon as the Pawn's active weapon
*/
simulated function SetActiveWeapon( Weapon NewWeapon )
{
	if ( InvManager != None )
	{
		InvManager.SetCurrentWeapon( NewWeapon );
	}
}


/**
 * Player just changed weapon. Called from InventoryManager::ChangedWeapon().
 * Network: Local Player and Server.
 *
 * @param	OldWeapon	Old weapon held by Pawn.
 * @param	NewWeapon	New weapon held by Pawn.
 */
simulated function PlayWeaponSwitch(Weapon OldWeapon, Weapon NewWeapon);

// Cheats - invoked by CheatManager
function bool CheatWalk()
{
	UnderWaterTime = Default.UnderWaterTime;
	SetCollision(true, true);
	SetPhysics(PHYS_Falling);
	bCollideWorld = true;
	SetPushesRigidBodies(Default.bPushesRigidBodies);
	return true;
}

function bool CheatGhost()
{
	UnderWaterTime = -1.0;
	SetCollision(false, false);
	bCollideWorld = false;
	SetPushesRigidBodies(false);
	return true;
}

function bool CheatFly()
{
	UnderWaterTime = Default.UnderWaterTime;
	SetCollision(true, true);
	bCollideWorld = true;
	return true;
}

/**
 * Returns the collision radius of our cylinder
 * collision component.
 *
 * @return	the collision radius of our pawn
 */
simulated function float GetCollisionRadius()
{
	return (CylinderComponent != None) ? CylinderComponent.CollisionRadius : 0.f;
}

/**
 * Returns the collision height of our cylinder
 * collision component.
 *
 * @return	collision height of our pawn
 */
simulated function float GetCollisionHeight()
{
	return (CylinderComponent != None) ? CylinderComponent.CollisionHeight : 0.f;
}

/** @return a vector representing the box around this pawn's cylinder collision component, for use with traces */
simulated final function vector GetCollisionExtent()
{
	local vector Extent;

	Extent = GetCollisionRadius() * vect(1,1,0);
	Extent.Z = GetCollisionHeight();
	return Extent;
}

/**
 * Pawns by nature are not stationary.	Override if you want exact findings
 */
function bool IsStationary()
{
	return false;
}

event SpawnedByKismet()
{
	// notify controller
	if (Controller != None)
	{
		Controller.SpawnedByKismet();
	}
}


/** Performs actual attachment. Can be subclassed for class specific behaviors. */
function DoKismetAttachment(Actor Attachment, SeqAct_AttachToActor Action)
{
	local bool	bOldCollideActors, bOldBlockActors, bValidBone, bValidSocket;

	// If a bone/socket has been specified, see if it is valid
	if( Mesh != None && Action.BoneName != '' )
	{
		// See if the bone name refers to an existing socket on the skeletal mesh.
		bValidSocket	= (Mesh.GetSocketByName(Action.BoneName) != None);
		bValidBone		= (Mesh.MatchRefBone(Action.BoneName) != INDEX_NONE);

		// Issue a warning if we were expecting to attach to a bone/socket, but it could not be found.
		if( !bValidBone && !bValidSocket )
		{
			`log(WorldInfo.TimeSeconds @ class @ GetFuncName() @ "bone or socket" @ Action.BoneName @ "not found on actor" @ Self @ "with mesh" @ Mesh);
		}
	}

	// Special case for handling relative location/rotation w/ bone or socket
	if( bValidBone || bValidSocket )
	{
		// disable collision, so we can successfully move the attachment
		bOldCollideActors	= Attachment.bCollideActors;
		bOldBlockActors		= Attachment.bBlockActors;
		Attachment.SetCollision(FALSE, FALSE);
		Attachment.SetHardAttach(Action.bHardAttach);

		// Sockets by default move the actor to the socket location.
		// This is not the case for bones!
		// So if we use relative offsets, then first move attachment to bone's location.
		if( bValidBone && !bValidSocket )
		{
			if( Action.bUseRelativeOffset )
			{
				Attachment.SetLocation(Mesh.GetBoneLocation(Action.BoneName));
			}

			if( Action.bUseRelativeRotation )
			{
				Attachment.SetRotation(QuatToRotator(Mesh.GetBoneQuaternion(Action.BoneName)));
			}
		}

		// Attach attachment to base.
		Attachment.SetBase(Self,, Mesh, Action.BoneName);

		if( Action.bUseRelativeRotation )
		{
			Attachment.SetRelativeRotation(Attachment.RelativeRotation + Action.RelativeRotation);
		}

		// if we're using the offset, place attachment relatively to the target
		if( Action.bUseRelativeOffset )
		{
			Attachment.SetRelativeLocation(Attachment.RelativeLocation + Action.RelativeOffset);
		}

		// restore previous collision
		Attachment.SetCollision(bOldCollideActors, bOldBlockActors);
	}
	else
	{
		// otherwise base on location
		Super.DoKismetAttachment(Attachment, Action);
	}
}


/** returns the amount this pawn's damage should be scaled by */
function float GetDamageScaling()
{
	return DamageScaling;
}

function OnSetMaterial(SeqAct_SetMaterial Action)
{
	if (Mesh != None)
	{
		Mesh.SetMaterial( Action.MaterialIndex, Action.NewMaterial );
	}
}

/** Kismet teleport handler, overridden so that updating rotation properly updates our Controller as well */
simulated function OnTeleport(SeqAct_Teleport Action)
{
	local array<Object> objVars;

	// find the first supplied actor
	Action.GetObjectVars(objVars,"Destination");
	if( !HandleTeleport( objVars, Action.bUpdateRotation, Action.bCheckOverlap, ,Action.TeleportVolumes ) )
	{
		`warn( "failed to handle teleport kismet action properly"@Action );		
	}
}

simulated function bool HandleTeleport( array<Object> DestList, bool bUpdateRotation, bool bCheckOverlap, optional float TeleportDistance, optional array<Volume> TeleportVolumes, optional int PreferredDestIndex )
{
	local int idx, cnt;
	local Actor destActor, tempActor, A;
	local Controller C;
	local bool bOccupiedDest, bColliding;
	local Vector Extent;

	Extent = GetCollisionExtent();
	bOccupiedDest = FALSE;

	// Make sure there's actually a list
	if (DestList.Length > 0)
	{
		// Make sure INDEX_NONE (-1) gets resolved before accessing the DestList
		idx = (PreferredDestIndex >= 0) ? PreferredDestIndex : 0;
		if( idx >= DestList.length )
		{
			idx = 0;
			PreferredDestIndex = 0;
		}
		cnt = 0;

		do
		{
			tempActor = Actor(DestList[idx]);
			if( tempActor != None )
			{
				// If its a player variable, teleport to the Pawn not the Controller.
				C = Controller(tempActor);
				if(C != None && C.Pawn != None)
				{
					tempActor = C.Pawn;
				}

				if( bCheckOverlap )
				{
					bColliding = FALSE;
					foreach VisibleCollidingActors ( class'Actor', A, Extent.X * 2.f, tempActor.Location, FALSE, Extent, TRUE )
					{
						if( IsBlockedBy( A ) )
						{
							bColliding = TRUE;
							break;
						}
					}

					bOccupiedDest = bColliding;
				}

				destActor = tempActor;
				if( (!bCheckOverlap || !bOccupiedDest) && destActor != None )
				{
					break;
				}
			}

			// Increment idx
			++idx;
			if( idx >= DestList.length )
			{
				idx = 0;
			}
			++cnt;

		} until (idx == PreferredDestIndex || cnt >= DestList.Length);
	}
	else
	{
		`warn("Unable to teleport - no destination list given");
	}
	
	// and set to that actor's location
	if( destActor != None && class'SeqAct_Teleport'.static.ShouldTeleport( self, destActor.Location, TeleportDistance, TeleportVolumes ) )
	{
		if (SetLocation(destActor.Location))
		{
			// If the pawn being teleported is a client player we need to tell him to move on his side
			if (!IsLocallyControlled() && PlayerController(Controller) != None)
			{
				PlayerController(Controller).ClientSetLocation(destActor.Location, Rotation);
			}

			PlayTeleportEffect(false, true);
			if( bUpdateRotation )
			{
				SetRotation(destActor.Rotation);
				if (Controller != None)
				{
					Controller.SetRotation(destActor.Rotation);
					Controller.ClientSetRotation(destActor.Rotation);
				}
			}
			// Tell controller we teleported (Pass None to avoid recursion)
			if( Controller != None )
			{
				Controller.OnTeleport( None );
			}
			return TRUE;
		}

		`warn("Unable to teleport to"@destActor);
		return FALSE;
	}
	
	if( destActor == None )
	{
		`warn("Unable to teleport - no destination given");
		return FALSE;
	}

	return TRUE;
}

/**
  * For debugging.  Causes a string to be displayed on the HUD.
  */
final event MessagePlayer( coerce String Msg )
{
`if(`notdefined(FINAL_RELEASE))
	local PlayerController PC;

	foreach LocalPlayerControllers(class'PlayerController', PC)
	{
		PC.ClientMessage( Msg );
	}
`endif
}

simulated event BecomeViewTarget(PlayerController PC)
{
	if (PhysicsVolume != None)
	{
		PhysicsVolume.NotifyPawnBecameViewTarget(self, PC);
	}

	// if we don't normally replicate health, but will want to do so now to this client, force an update
	if (!bReplicateHealthToAll && WorldInfo.NetMode != NM_Client)
	{
		PC.ForceSingleNetUpdateFor(self);
	}
}

/** For AI debugging */
event SoakPause()
{
	local PlayerController PC;

	ForEach WorldInfo.LocalPlayerControllers(class'PlayerController', PC)
	{
		PC.SoakPause(self);
		break;
	}
}

native function ClearConstraints();
native function AddPathConstraint( PathConstraint Constraint );
native function AddGoalEvaluator( PathGoalEvaluator Evaluator );

/**
 * Path shaping creation functions...
 * these functions by default will just new the class, but this offers a handy
 * interface to override for to do things like pool the constraints
 */
function PathConstraint CreatePathConstraint( class<PathConstraint> ConstraintClass )
{
	return new(self) ConstraintClass;
}
function PathGoalEvaluator CreatePathGoalEvaluator( class<PathGoalEvaluator> GoalEvalClass )
{
	return new(self) GoalEvalClass;
}

native function IncrementPathStep( int Cnt, Canvas C );
native function IncrementPathChild( int Cnt, Canvas C );
native function DrawPathStep( Canvas C );
native function	ClearPathStep();

simulated function ZeroMovementVariables()
{
	Velocity = vect(0,0,0);
	Acceleration = vect(0,0,0);
}

simulated function SetCinematicMode( bool bInCinematicMode );

native function SetRootMotionInterpCurrentTime( float inTime, optional float DeltaTime, optional bool bUpdateSkelPose  );

/** Set a ScalarParameter to Interpolate */
final simulated native function SetScalarParameterInterp(const out ScalarParameterInterpStruct ScalarParameterInterp);

/** Simple interface for handling pawn dialogue. */
simulated event Speak(SoundCue Cue)
{
	// Trivial implementation for now.

	// @TODO: handle things like
	// - speaking one line at a time
	// - playing facefx if appropriate
	// - dialogue-specific soundmodes
	// - etc

	PlaySound(Cue, TRUE);
}

/**
 *	Handler for the SeqAct_SetVelocity action. Allows level designer to impart a velocity on the actor.
 */
simulated function OnSetVelocity( SeqAct_SetVelocity Action )
{
	Super.OnSetVelocity(Action);
	// fake acceleration - Assuming default delta time is 0.2f
	if (Action.VelocityMag == 0)
	{
		Acceleration = vect(0, 0, 0);
	}
	else
	{
		Acceleration = Velocity/0.2f;
	}
}

defaultproperties
{
	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_Actor'
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Pawns"
	End Object
	Components.Add(Sprite)

	// Pawns often manipulate physics components so need to be done pre-async
	TickGroup=TG_PreAsyncWork

	InventoryManagerClass=class'InventoryManager'
	ControllerClass=class'AIController'

	// Flags
	bCanBeDamaged=true
	bCanCrouch=false
	bCanFly=false
	bCanJump=true
	bCanSwim=false
	bCanTeleport=true
	bCanWalk=true
	bJumpCapable=true
	bProjTarget=true
	bSimulateGravity=true
	bShouldBaseAtStartup=true

	// Locomotion
	WalkingPhysics=PHYS_Walking
	LandMovementState=PlayerWalking
	WaterMovementState=PlayerSwimming

	AccelRate=+02048.000000
	DesiredSpeed=+00001.000000
	MaxDesiredSpeed=+00001.000000
	MaxFallSpeed=+1200.0
	AIMaxFallSpeedFactor=1.0
	NonPreferredVehiclePathMultiplier=1.0

	AirSpeed=+00600.000000
	GroundSpeed=+00600.000000
	JumpZ=+00420.000000
	OutofWaterZ=+420.0
	LadderSpeed=+200.0
	WaterSpeed=+00300.000000

	bLimitFallAccel=TRUE
	AirControl=+0.05

	CrouchedPct=+0.5
	WalkingPct=+0.5
	MovementSpeedModifier=+1.0

	// Sound
	bLOSHearing=true
	HearingThreshold=+2800.0
	SoundDampening=+00001.000000
	noise1time=-00010.000000
	noise2time=-00010.000000

	// Physics
	AvgPhysicsTime=+00000.100000
	bPushesRigidBodies=false
	RBPushRadius=10.0
	RBPushStrength=50.0

	// FOV / Sight
	ViewPitchMin=-16384
	ViewPitchMax=16383
	RotationRate=(Pitch=20000,Yaw=20000,Roll=20000)
	MaxPitchLimit=3072

	SightRadius=+05000.000000

	// Network
	RemoteRole=ROLE_SimulatedProxy
	NetPriority=+00002.000000
	bUpdateSimulatedPosition=true

	// GamePlay
	DamageScaling=+00001.000000
	Health=100
	bReplicateHealthToAll=false

	// Collision
	BaseEyeHeight=+00064.000000
	EyeHeight=+00054.000000

	CrouchHeight=+40.0
	CrouchRadius=+34.0

	MaxStepHeight=35.0
	MaxJumpHeight=96.0
	WalkableFloorZ=0.7		   // 0.7 ~= 45 degree angle for floor
	LedgeCheckThreshold=4.0f

	MaxOutOfWaterStepHeight=40.0
	AllowedYawError=2000
	Mass=+00100.000000

	bCollideActors=true
	bCollideWorld=true
	bBlockActors=true

	Begin Object Class=CylinderComponent Name=CollisionCylinder
		CollisionRadius=+0034.000000
		CollisionHeight=+0078.000000
		BlockNonZeroExtent=true
		BlockZeroExtent=true
		BlockActors=true
		CollideActors=true
	End Object
	CollisionComponent=CollisionCylinder
	CylinderComponent=CollisionCylinder
	Components.Add(CollisionCylinder)

	Begin Object Class=ArrowComponent Name=Arrow
		ArrowColor=(R=150,G=200,B=255)
		bTreatAsASprite=True
		SpriteCategoryName="Pawns"
	End Object
	Components.Add(Arrow)

	VehicleCheckRadius=150

	bAllowLedgeOverhang=TRUE

	RootMotionInterpRate=1.f

	bModifyNavPointDest=true
}
