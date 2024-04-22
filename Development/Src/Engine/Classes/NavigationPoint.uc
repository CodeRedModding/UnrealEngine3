//=============================================================================
// NavigationPoint.
//
// NavigationPoints are organized into a network to provide AIControllers
// the capability of determining paths to arbitrary destinations in a level
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class NavigationPoint extends Actor
	hidecategories(Lighting,LightColor,Force)
	dependson(ReachSpec)
	ClassGroup(Navigation)
	native;

const	INFINITE_PATH_COST	=	10000000;

//------------------------------------------------------------------------------
// NavigationPoint variables

var transient bool bEndPoint;	// used by C++ navigation code
var transient bool bTransientEndPoint; // set right before a path finding attempt, cleared afterward.
var transient bool bHideEditorPaths;	// don't show paths to this node in the editor
var transient bool bCanReach;		// used during paths review in editor

/** structure for inserting things into the navigation octree */
struct native NavigationOctreeObject
{
	/** the bounding box to use */
	var Box BoundingBox;
	/** cached center of that box */
	var vector BoxCenter;
	/** if this object is in the octree, pointer to the node it's in, otherwise NULL */
	var native transient const pointer OctreeNode{class FNavigationOctreeNode};
	/** UObject that owns the entry in the octree */
	var noexport const Object Owner;
	/** bitfield representing common classes of Owner so we can avoid casts */
	var noexport const byte OwnerType;

	structcpptext
	{
	 enum ENavOctreeObjectType
	 {
	 	NAV_NavigationPoint = 0x01,
	 	NAV_ReachSpec = 0x02,
	 };
	 private:
	 	UObject* Owner;
	 	BYTE OwnerType;
	 public:
	 	/** constructor, makes sure OctreeNode is NULL */
	 	FNavigationOctreeObject()
	 	: OctreeNode(NULL), Owner(NULL), OwnerType(0)
	 	{}
	 	/** destructor, removes us from the octree if we're still there */
	 	~FNavigationOctreeObject();
		/** sets the object's owner and initializes the OwnerType for fast cast to common types */
		void SetOwner(UObject* InOwner);

		/** sets the object's bounding box
		 * if the object is currently in the octree, re-adds it
		 * @param InBoundingBox the new bounding box to use
		 */
		void SetBox(const FBox& InBoundingBox);

		/** overlap check function called after the axis aligned bounding box check succeeds
		 * allows for more precise checks for certain object types
		 * @param TestBox the box to check
		 * @return true if the box doesn't overlap this object, false if it does
		 */
		UBOOL OverlapCheck(const FBox& TestBox);

		/** templated accessor to Owner, to avoid casting for common cases
		 * @note T must be UObject or a subclass, or this function will not compile
		 */
		template<class T> FORCEINLINE T* GetOwner()
		{
			return Cast<T>(Owner);
		}
		//@note the specializations for this template are in UnPath.h because they must be outside the struct definition

		void Serialize(FArchive& Ar);
	}
};
var native transient const NavigationOctreeObject NavOctreeObject;

var() bool bBlocked;			// this node is currently unuseable
var() bool bOneWayPath;			// reachspecs from this path only in the direction the path is facing (180 degrees)
var	bool bNeverUseStrafing;	// shouldn't use bAdvancedTactics going to this point
var bool bAlwaysUseStrafing;	// shouldn't use bAdvancedTactics going to this point
var const bool bForceNoStrafing;// override any LD changes to bNeverUseStrafing
var const bool bAutoBuilt;		// placed during execution of "PATHS BUILD"
var	bool bSpecialMove;			// if true, pawn will call SuggestMovePreparation() when moving toward this node
var bool bNoAutoConnect;		// don't connect this path to others except with special conditions (used by LiftCenter, for example)
var	const bool	bNotBased;		// used by path builder - if true, no error reported if node doesn't have a valid base
var const bool  bPathsChanged;	// used for incremental path rebuilding in the editor
var() bool		bDestinationOnly; // used by path building - means no automatically generated paths are sourced from this node
var	bool		bSourceOnly;	// used by path building - means this node is not the destination of any automatically generated path
var bool		bSpecialForced;	// paths that are forced should call the SpecialCost() and SuggestMovePreparation() functions
var bool		bMustBeReachable;	// used for PathReview code
var bool		bBlockable;		// true if path can become blocked (used by pruning during path building)
var	bool		bFlyingPreferred;	// preferred by flying creatures
var bool		bMayCausePain;		// set in C++ if in PhysicsVolume that may cause pain
var transient bool bAlreadyVisited;	// internal use
var() bool 	bVehicleDestination;	// if true, forced paths to this node will have max width to accomodate vehicles
var() bool bMakeSourceOnly;
var	bool	bMustTouchToReach;		// if true. reach tests are based on whether pawn can move to overlap this NavigationPoint (only valid if bCollideActors=true)
/** whether walking on (being based on) this NavigationPoint counts as reaching it */
var bool bCanWalkOnToReach;
/** if true, attempt to build long range (> MAXPATHDIST) paths to/from this node */
var bool bBuildLongPaths;
/** indicates vehicles cannot use this node */
var(VehicleUsage) bool bBlockedForVehicles;
/** vehicles with bUsePreferredVehiclePaths set (large vehicles, usually) will prioritize using these nodes */
var(VehicleUsage) bool bPreferredVehiclePath;

var() editinline const editconst duplicatetransient array<ReachSpec> PathList; //index of reachspecs (used by C++ Navigation code)
/** List of navigation points to prevent paths being built to */
var editoronly duplicatetransient array<ActorReference> EditorProscribedPaths;
/** List of navigation points to force paths to be built to */
var editoronly duplicatetransient array<ActorReference> EditorForcedPaths;
/** List of volumes containing this navigation point relevant for gameplay */
var() const editconst  array<ActorReference>	Volumes;
var int visitedWeight;
var const int bestPathWeight;
var const private NavigationPoint nextNavigationPoint;
var const NavigationPoint nextOrdered;	// for internal use during route searches
var const NavigationPoint prevOrdered;	// for internal use during route searches
var const NavigationPoint previousPath;
var int Cost;					// added cost to visit this pathnode
var() int ExtraCost;			// Extra weight added by level designer
var transient int TransientCost;	// added right before a path finding attempt, cleared afterward.
var	transient int FearCost;		// extra weight diminishing over time (used for example, to mark path where bot died)

/** Mapping of Cost/Description for costs of this node */
struct native DebugNavCost
{
	var string Desc;
	var int Cost;

	structcpptext
	{
		/** constructors */
		FDebugNavCost() {}
		FDebugNavCost(EEventParm)
		{
			appMemzero(this, sizeof(FDebugNavCost));
		}
		UBOOL operator==(const FDebugNavCost& Other)
		{
			return (Other.Cost == Cost && Other.Desc == Desc);
		}
	}
};
var transient array<DebugNavCost> CostArray;

var DroppedPickup	InventoryCache;		// used to point to dropped weapons
var float	InventoryDist;
var const float LastDetourWeight;

var	CylinderComponent		CylinderComponent;

/** path size of the largest ReachSpec in this node's PathList */
var() editconst const Cylinder MaxPathSize;

/** GUID used for linking paths across levels */
var() editconst const duplicatetransient guid NavGuid;

/** Normal editor sprite */
var const transient SpriteComponent GoodSprite;
/** Used to draw bad collision intersection in editor */
var const transient SpriteComponent BadSprite;

/** Does this nav point point to others in separate levels? */
var const bool bHasCrossLevelPaths;

/** Which navigation network does this navigation point connect to? */
var() editconst const int NetworkID;

/** Pawn that is currently anchor to this navigation point */
var transient Pawn AnchoredPawn;
/** Last time a pawn was anchored to this navigation point - set when Pawn chooses a new anchor */
var transient float LastAnchoredPawnTime;

/** whether we need to save this in checkpoints because it has been modified by Kismet */
var transient bool bShouldSaveForCheckpoint;

struct CheckpointRecord
{
	var bool bDisabled;
	var bool bBlocked;
};

cpptext
{
	virtual UClass* GetReachSpecClass( ANavigationPoint* Nav, UClass* DefaultReachSpecClass ) { return DefaultReachSpecClass; }

	virtual void ClearPaths();
	virtual void FindBase();
	virtual void PostScriptDestroyed();
protected:
 	virtual void UpdateComponentsInternal(UBOOL bCollisionUpdate = FALSE);
public:
	void PostEditMove(UBOOL bFinished);
	void Spawned();
	void UpdateMaxPathSize();
	UBOOL FindAlternatePath(UReachSpec* StraightPath, INT AccumulatedDistance);
	virtual UBOOL ShouldBeBased();

	/** Checks to make sure the navigation is at a valid point */
	virtual void Validate();

	virtual void TogglePathRendering(UBOOL bShouldDrawPaths);

#if WITH_EDITOR
	virtual void CheckForErrors();


	/**
	 * Sets the network ID for this nav and all connected navs.
	 */
	virtual void SetNetworkID(INT InNetworkID);
	static void BuildNetworkIDs();

	virtual void ReviewPath(APawn* Scout);
	virtual UBOOL CheckSatisfactoryConnection(ANavigationPoint* Other);

	void CleanUpPruned();
	INT PrunePaths();
	// more aggressive (and expensive) path pruning routine ( should only be called from editor )
	INT AggressivePrunePaths();
	INT SecondPassAggressivePrunePaths();
	virtual UBOOL CanPrunePath(INT index) { return TRUE; }

	virtual void AddForcedSpecs( AScout *Scout );

	virtual UReachSpec* ForcePathTo(ANavigationPoint *Nav, AScout *Scout = NULL, UClass* ReachSpecClass = NULL );
	virtual UBOOL ProscribePathTo(ANavigationPoint *Nav, AScout *Scout = NULL);

	/** builds long range paths (> MAXPATHDIST) between this node and all other reachable nodes
	 * for which a straight path would be significantly shorter or the only way to reach that node
	 * done in a separate pass at the end because it's expensive and so we want to early out in the maximum number
	 * of cases (e.g. if suitable short range paths already get there)
	 */
	void AddLongPaths(AScout* Scout, UBOOL bOnlyChanged);

	virtual void	addReachSpecs(class AScout *Scout, UBOOL bOnlyChanged=0);
	virtual INT AddMyMarker(AActor *S);
	/** returns whether a ReachSpec can be built from this node to Nav
	 * @param Nav the NavigationPoint to check if we can build a path to
	 * @param bCheckDistance whether or not we should check if Nav is close enough (MAXPATHDIST)
	 * @return true if a ReachSpec can be built from this node to Nav, false otherwise
	 */
	virtual UBOOL CanConnectTo(ANavigationPoint* Nav, UBOOL bCheckDistance);

	virtual void OnAddToPrefab();

#endif

	virtual void InitForPathFinding() {};
	virtual void ClearForPathFinding();

	UBOOL CanReach(ANavigationPoint *Dest, FLOAT Dist, UBOOL bUseFlag, UBOOL bAllowFlying);
	virtual class APickupFactory* GetAPickupFactory() { return NULL; }
	virtual void SetVolumes(const TArray<class AVolume*>& Volumes);
	virtual void SetVolumes();
	virtual UBOOL ReachedBy(APawn* P, const FVector& TestPosition, const FVector& Dest);
	virtual UBOOL TouchReachSucceeded(APawn *P, const FVector& TestPosition);
	virtual UBOOL GetUpDir( FVector &V ) { return 0; }


	virtual void AddToNavigationOctree();
	virtual void RemoveFromNavigationOctree();
	virtual UBOOL PlaceScout(class AScout *Scout);

	/** returns the position the AI should move toward to reach this actor */
	FVector GetDestination(AController* C);

	/** sorts the PathList by distance, shortest first */
	void SortPathList();

	/**
	 * Fills the array of any nav references needing to be fixed up.
	 */
	virtual void GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel);
	virtual void ClearCrossLevelReferences();
	virtual FGuid* GetGuid() { return &NavGuid; }


	virtual ANavigationPoint* SpecifyEndAnchor(APawn* RouteFinder);

	/**
	 * Works through the component arrays marking entries as pending kill so references to them
	 * will be NULL'ed.
	 *
	 * @param	bAllowComponentOverride		Whether to allow component to override marking the setting
	 */
	virtual void MarkComponentsAsPendingKill(UBOOL bAllowComponentOverride = FALSE);
}

native function GetBoundingCylinder(out float CollisionRadius, out float CollisionHeight) const;

native final function ReachSpec GetReachSpecTo( NavigationPoint Nav, optional class<ReachSpec> SpecClass );

/** returns whether this NavigationPoint is valid to be considered as an Anchor (start or end) for pathfinding by the given Pawn
 * @param P the Pawn doing pathfinding
 * @return whether or not we can be an anchor
 */
native function bool IsUsableAnchorFor( Pawn P );

/** returns whether this NavigationPoint is a teleporter that can teleport the given Actor */
native function bool CanTeleport(Actor A);

event int SpecialCost(Pawn Seeker, ReachSpec Path);

// Accept an actor that has teleported in.
// used for random spawning and initial placement of creatures
event bool Accept( actor Incoming, actor Source )
{
	local bool bResult;

	// Move the actor here.
	bResult = Incoming.SetLocation( Location );
	if (bResult)
	{
		Incoming.Velocity = vect(0,0,0);
		Incoming.SetRotation(Rotation);
	}
	Incoming.PlayTeleportEffect(true, false);
	return bResult;
}

/* DetourWeight()
value of this path to take a quick detour (usually 0, used when on route to distant objective, but want to grab inventory for example)
*/
event float DetourWeight(Pawn Other,float PathWeight);

/* SuggestMovePreparation()
Optionally tell Pawn any special instructions to prepare for moving to this goal
(called by Pawn.PrepareForMove() if this node's bSpecialMove==true
*/
event bool SuggestMovePreparation( Pawn Other )
{
	// If special move was taken to get to this link
	return Other.SpecialMoveTo(Other.Anchor, self, Other.Controller.MoveTarget);
}

/* ProceedWithMove()
Called by Controller to see if move is now possible when a mover reports to the waiting
pawn that it has completed its move
*/
function bool ProceedWithMove(Pawn Other)
{
	return true;
}

/**
 * Returns the nearest valid navigation point to the given actor.
 */
static final function NavigationPoint GetNearestNavToActor(Actor ChkActor, optional class<NavigationPoint> RequiredClass,optional array<NavigationPoint> ExcludeList,optional float MinDist)
{
	local NavigationPoint Nav, BestNav;
	local float Dist, BestDist;
	if (ChkActor != None)
	{
		// iterate through all points in the level
		foreach ChkActor.WorldInfo.AllNavigationPoints(class'NavigationPoint',Nav)
		{
			// if no filter class specified, and
			// if nav is available to the check actor, and
			// if the nav isn't part of the excluded list,
			if ((RequiredClass == None || Nav.class == RequiredClass) && ExcludeList.Find(Nav) == INDEX_NONE)
			{
				// pick the closest
				Dist = VSize(Nav.Location-ChkActor.Location);
				if (Dist > MinDist)
				{
					if (BestNav == None ||
						Dist < BestDist)
					{
						BestNav = Nav;
						BestDist = Dist;
					}
				}
			}
		}
	}
	return BestNav;
}

/**
 * Returns the nearest valid navigation point to the given point.
 */
static final function NavigationPoint GetNearestNavToPoint(Actor ChkActor,vector ChkPoint, optional class<NavigationPoint> RequiredClass,optional array<NavigationPoint> ExcludeList)
{
	local NavigationPoint Nav, BestNav;
	local float Dist, BestDist;
	if (ChkActor != None)
	{
		// iterate through all points in the level
		foreach ChkActor.WorldInfo.AllNavigationPoints(class'NavigationPoint',Nav)
		{
			// if no filter class specified, and
			// if nav is available to the check actor, and
			// if the nav isn't part of the excluded list,
			if ((RequiredClass == None || Nav.class == RequiredClass) && ExcludeList.Find(Nav) == INDEX_NONE)
			{
				// pick the closest
				Dist = VSize(Nav.Location-ChkPoint);
				if (BestNav == None ||
					Dist < BestDist)
				{
					BestNav = Nav;
					BestDist = Dist;
				}
			}
		}
	}
	return BestNav;
}

/**
 * Returns all navigation points near the ChkPoint specified by Radius.
 */
static native final function bool GetAllNavInRadius( Actor ChkActor, Vector ChkPoint, float Radius, out array<NavigationPoint> out_NavList, optional bool bSkipBlocked, optional int inNetworkID=-1, optional Cylinder MinSize );

/** Returns if this navigation point is on a different network than the given */
native final function bool IsOnDifferentNetwork( NavigationPoint Nav );

/**
 * Toggle the blocked state of a navigation point.
 */
function OnToggle(SeqAct_Toggle inAction)
{
	if (inAction.InputLinks[0].bHasImpulse)
	{
		bBlocked = false;
	}
	else if (inAction.InputLinks[1].bHasImpulse)
	{
		bBlocked = true;
	}
	else if (inAction.InputLinks[2].bHasImpulse)
	{
		bBlocked = !bBlocked;
	}

	WorldInfo.Game.NotifyNavigationChanged(self);

	bShouldSaveForCheckpoint = true;
}

simulated event ShutDown()
{
	Super.ShutDown();

	bBlocked = TRUE;
	WorldInfo.Game.NotifyNavigationChanged(self);

	bShouldSaveForCheckpoint = true;
}

function bool ShouldSaveForCheckpoint()
{
	return bShouldSaveForCheckpoint;
}

function CreateCheckpointRecord(out CheckpointRecord Record)
{
	Record.bBlocked = bBlocked;
}

function ApplyCheckpointRecord(const out CheckpointRecord Record)
{
	bBlocked = Record.bBlocked;
	bShouldSaveForCheckpoint = true;
}

/** @return Debug abbrev for hud printing */
simulated event string GetDebugAbbrev()
{
	return "NP?";
}

defaultproperties
{
	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_NavP'
		HiddenGame=true
		HiddenEditor=false
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Navigation"
	End Object
	Components.Add(Sprite)
	GoodSprite=Sprite

	Begin Object Class=SpriteComponent Name=Sprite2
		Sprite=Texture2D'EditorResources.Bad'
		HiddenGame=true
		HiddenEditor=true
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Navigation"
		Scale=0.25
	End Object
	Components.Add(Sprite2)
	BadSprite=Sprite2

	Begin Object Class=ArrowComponent Name=Arrow
		ArrowColor=(R=150,G=200,B=255)
		ArrowSize=0.5
		bTreatAsASprite=True
		HiddenGame=true
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Navigation"
	End Object
	Components.Add(Arrow)

	Begin Object Class=CylinderComponent Name=CollisionCylinder LegacyClassName=NavigationPoint_NavigationPointCylinderComponent_Class
		CollisionRadius=+0050.000000
		CollisionHeight=+0050.000000
	End Object
	CollisionComponent=CollisionCylinder
	CylinderComponent=CollisionCylinder
	Components.Add(CollisionCylinder)

	Begin Object Class=PathRenderingComponent Name=PathRenderer
	End Object
	Components.Add(PathRenderer)

	bMayCausePain=true
	bStatic=true
	bNoDelete=true

	bHidden=FALSE

	bCollideWhenPlacing=true
	bMustTouchToReach=true
	bBuildLongPaths=true

	bCollideActors=false

	// default to no network id
	NetworkID=-1
	// NavigationPoints are generally server side only so we don't need to worry about client simulation
	bForceAllowKismetModification=true
}
