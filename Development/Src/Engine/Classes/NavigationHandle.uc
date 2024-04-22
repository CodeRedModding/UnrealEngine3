//=============================================================================
// NavigationHandle
//
// Component that encapsulates navigation behavior.  Attach this to your actor of choice to
// enable that actor to pathfind
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class NavigationHandle extends Object within Actor
	native(AI);

struct native PolySegmentSpan
{
	var native pointer Poly{FNavMeshPolyBase};
	var Vector P1, P2;

	structcpptext
	{
		explicit FPolySegmentSpan( struct FNavMeshPolyBase* inPoly, FVector inP1, FVector inP2 )
		{
			Poly = inPoly;
			P1 = inP1;
			P2 = inP2;
		}
	}
};

/** Current pylon AI is anchored to */
var Pylon		AnchorPylon;
var native pointer AnchorPoly{FNavMeshPolyBase};

/** dummy struct used to match alignment in pathcache array */
struct {FNavMeshEdgeBase*} EdgePointer
{
	var native const pointer Dummy{FNavMeshEdgeBase};
};

// struct containing the pathcache aray.  Used so that
// pathcaches can easily be stored and copied around in script
struct native PathStore
{
	structcpptext
	{
		FORCEINLINE FNavMeshEdgeBase*& operator()( INT i )
		{
			return EdgeList(i);
		}

		FORCEINLINE INT Num()
		{
			return EdgeList.Num();
		}

		FNavMeshEdgeBase*& Last( INT c=0 )
		{
			return EdgeList.Last(c);
		}

		FNavMeshEdgeBase*& Top()
		{
			return Last();
		}
	}

	var native array<EdgePointer>	EdgeList;
};

var PathStore PathCache;

/**
 * This points to the BestUnfinishedPathPoint.  Which will usually be set by some EvaluateGoal or DetermineFinalGoal function/
 * In some cases it is not possible to get a full path, but having the best unfinished path is good enough (e.g. ai on top
 * of a crevice, or on a navmesh island disconnected from everyone else)
 **/
var transient native pointer BestUnfinishedPathPoint{FNavMeshPolyBase};

/** List of polys to move through */
var const native pointer CurrentEdge{FNavMeshEdgeBase};

/** the poly we're currently trying to get inside */
var const native pointer SubGoal_DestPoly{FNavMeshPolyBase};

/** Final destination */
var BasedPosition FinalDestination;
/** AI should not update route cache - flag to prevent cache from being changed when pathing is used to evaluate squad location */
var bool bSkipRouteCacheUpdates;

const LINECHECK_GRANULARITY = 768.f;

/** List of search constraints for pathing */
var NavMeshPathConstraint		PathConstraintList;
var NavMeshPathGoalEvaluator	PathGoalList;

/** when this is TRUE the goal evaluator chain will be treated as an OR chain instead of an AND chain */
var bool bUseORforEvaluateGoal;

/** this is a handy way of ensuring everyone is setting the necessary parameters.. this should always reflect the number of
 *  parameters in the NavMeshPathParams struct, and then in SetupPathingParams a check will be inserted with the
 *  number of params in existence at the time of writing the function, so if the number of params changes and the implementations
 *  are not updated an assert will fire
 */
const NUM_PATHFINDING_PARAMS = 9;

// this struct is where all the non-volatile pathing params are cached at the beginning of a path search.
// Populated from Interface_NavigationHandle::SetupPathfindingParams()
struct native NavMeshPathParams
{
	/** the navhandle interface for the pathing entity */
	var native pointer Interface{IInterface_NavigationHandle};

	/** can this entity use mantle edges? */
	var bool bCanMantle;

	/** do we need to perform extra checks when determining if an edge supports the entiy? */
	var bool bNeedsMantleValidityTest;

	/**  is this entity valid to pathfind (does it have a pawn, etc..) */
	var bool bAbleToSearch;

	/** the size of the entity looking for a path */
	/* @NOTE: this will use the LARGEST of the X/Y dimensions.  Pathfinding extents must be symmetrical, so if the extent is not,
	   it will be made to be symmetric. (for long train-like objects path with the extent of the leader, and the rest will follow)
	   @see FNavMeshEdgeBase::Supports()
    */
	var Vector SearchExtent;
	var float  SearchLaneMultiplier;

	/** the starting location for the path search */
	var vector SearchStart;

	/** the maximum valid height for this entity to 'drop down' (e.g. max height to use on dropdown edges) */
	var float  MaxDropHeight;

	/** the minimum value for the Z component of walkable surfaces */
	var float MinWalkableZ;

	/** max hover distance -- the maximum distance this entity can hover above the surface of a polygon.  (-1 means arbitrarily high) */
	var float MaxHoverDistance;
};

// the cached path params for the current search
var NavMeshPathParams CachedPathParams;

/** when this bool is TRUE, statistics about which constraints are doing what will be printed following
 *  path searches
 */
var(PathDebug) bool bDebugConstraintsAndGoalEvals;

/**
 * when true TONS of information will be printed to the log, as well as a bunch of stuff drawn to the screen.
 * debug lines will be drawn indicating the progress of the path traversal, and whenever a log message is printed related
 * to an edge on the navmesh a number will be printed to the screen above it indexing into the log messages to tell you
 * what that message is.
 * RED lines indicate expansion was stopped at that step, other colors will change depending on the expansion generation
 * (e.g. all edges traversed in the first step will be of the same color, second step a different color etc..
 */
var(PathDebug) bool bUltraVerbosePathDebugging;

/** If true, only adds the visual parts of verbose path debugging, without the log spew */
var(PathDebug) bool bVisualPathDebugging;

/** 
 *  Relevant error code set by FindPath when a path search fails. 
 *  Allows decision on how to resolve a failed search by providing more information on the failure cause .
 *  Only valid if FindPath returns FALSE.  Safe to ignore otherwise.
 *  This value is not updated until the next search fails.
 */  
var(PathDebug) EPathFindingError    LastPathError;
var(PathDebug) float                LastPathFailTime;

/** max breadcrumbs - number of breadcrumbs to keep track of */
const NumBreadCrumbs = 10;
/** ring buffer of breadcrumb positions */
var vector Breadcrumbs[NumBreadCrumbs];
/** index that represents the most recent breadcrumb */
var int BreadCrumbMostRecentIdx;
/** bread crumb interval (how far we need to move before laying a new breadcrumb) */
var float BreadCrumbDistanceInterval;
/** draw breadcrumb debug info? */
var() bool bDebug_Breadcrumbs;

cpptext
{
public:
	UNavigationHandle()
	{
		if(!IsTemplate())
		{
			FNavMeshWorld::RegisterActiveHandle(this);
		}
	}
	// use this in SetupPathingParams and pass the number of params you have populated to ensure when new properties
	// are added to the struct that you are setting them all
	#define VERIFY_NAVMESH_PARAMS(NUM) \
		typedef char ERROR_Missing_NavMesh_param_please_Set_all_Params[( NUM != UCONST_NUM_PATHFINDING_PARAMS ) ? 0 : 1];

	// returns TRUE if the pylon and poly associated with the interface's current location can be found
	static UBOOL GetPylonAndPolyFromActorPos( AActor* Actor, APylon*& out_Pylon, struct FNavMeshPolyBase*& out_Poly);



	/**
	 * GetPylonANdPolyFromPos
	 * - will search for the pylon and polygon that contain this point
	 * @param Pos - position to get pylon and poly from
	 * @param out_Pylon - output var for pylon this position is within
	 * @param out_Poly - output var for poly this position is within
	 * @param PylonsToConsider - only check these pylons (useful for perf)
	 * @return - TRUE if the pylon and poly were found succesfully
	 */
	static UBOOL GetPylonAndPolyFromPos(const FVector& Pos, FLOAT MinWalkableFloorZ, APylon*& out_Pylon, struct FNavMeshPolyBase*& out_Poly, TArray<APylon*>* PylonsToConsider=NULL);
	/**
	 * GetAllPylonsFromPos
	 * - will populate a list of all pylons which are valid (in terms of can a pawn walk from the possible pylon) for the given position
	 * @param Pos - position to test
	 * @param out_Pylons - list of pylons to be populated with results
	 * @param bWalkable - whether or not this pylon's polys need to be "walkable"  FALSE for looking for disconnected pylons
	 * @return TRUE if any pylons were found
	 */
	static UBOOL GetAllPylonsFromPos(const FVector& Pos, const FVector& Extent, TArray<APylon*>& out_Pylons, UBOOL bWalkable = TRUE );

	/**
	 * GetPylonAndPolyFromBox
	 * - will search for the pylon and polygon that contain this box
	 * @param Box - the box to use to find a poly for
	 * @param out_Pylon - output var for pylon this position is within
	 * @param out_Poly - output var for poly this position is within
	 * @return - TRUE if the pylon and poly were found succesfully
	 */
	static UBOOL GetPylonAndPolyFromBox(const FBox& Box, FLOAT MinWalkableZ, APylon*& out_Pylon, struct FNavMeshPolyBase*& out_Poly);


	/**
	 * GetAnchorPoly
	 * - will find a suitable anchor (start) poly for this handle
	 * @return - the suitable poly (if any)
	 */
	FNavMeshPolyBase* GetAnchorPoly();

	/**
	 * GetAllPolysFromPos
	 * will return all polys in any mesh which are within the passed extent
	 * @param Pos - Center of extent to check
	 * @param Extent - extent of box to check
	 * @param out_PolyList - output array of polys to check
	 * @param bIgnoreDynamic - if TRUE, dynamically created submeshes will be ignored
	 * @param bReturnBothDynamicAndStatic - if TRUE, BOTH dynamic and static polys will be returned.. using this is *DANGEROUS*! most of the time you should use dynamic polys if they exist
	 *                                      as they are the 'correct' representation of the mesh at that point
	 * @param PylonsToConsider - list of pylons to consider for this check, defauls to doing an octree check to determine this list
	 * @param TraceFlags - flags for trace dilineation
	 * @return TRUE if polys were found
	 */
	static UBOOL GetAllPolysFromPos( const FVector& Pos, const FVector& Extent, TArray<struct FNavMeshPolyBase*>& out_PolyList, UBOOL bIgnoreDynamic, UBOOL bReturnBothDynamicAndStatic=FALSE, TArray<APylon*>* PylonsToConsider=NULL, DWORD TraceFlags=0);

	/**
	 * GetAllObstaclePolysFromPos
	 * will return all obstacle polys in any mesh which are within the passed extent
	 * @param Pos - Center of extent to check
	 * @param Extent - extent of box to check
	 * @param out_PolyList - output array of polys to check
	 * @param PylonsToCheck - OPTIONAL param indicating the lsit of pylons we should check instead of hitting the pylon octree
	 * @param bSkipDynamicObstacleMesh - OPTIONAL param, when true only static obstacle mesh polys will be returned
	 * @param TraceFlags - flags for trace dilineation
	 * @return TRUE if polys were found
	 */
												 
	static void GetAllObstaclePolysFromPos( const FVector& Pos,
											const FVector& Extent,
											TArray<struct FNavMeshPolyBase*>& out_PolyList,
											const TArray<APylon*>* PylonsToCheck=NULL,
											UBOOL bSkipDynamicObstacleMesh=FALSE, 
											DWORD TraceFlags=0);

	// returns TRUE if the AABB provided intersects a loaded portion of the mesh
	static UBOOL BoxIntersectsMesh( const FVector& Center, const FVector& Extent, APylon*& out_Pylon, struct FNavMeshPolyBase*& out_Poly, DWORD TraceFlags=0);

	/**
	 * returns TRUE if the poly described by the passed list of vectors intersects a loaded portion of the mesh
	 * @param Poly - vertexlist representing the poly we want to query against the mesh
	 * @param out_Pylon - pylon we collided with
	 * @param out_Poly - poly we collided with
	 * @arapm ExclusionPolys - optional list of polys we want to exclude from the search
	 * @param bIgnoreImportedMeshes - when TRUE meshes which unwalkable surfaces will be ignored for overlap testing
	 * @param IgnorePylons - optional pylons to ignore collisions from
	 * @return TRUE if poly intersects
	 */
	static UBOOL PolyIntersectsMesh( TArray<FVector>& Poly, APylon*& out_Pylon, struct FNavMeshPolyBase*& out_Poly, TArray<FNavMeshPolyBase*>* ExclusionPolys=NULL, UBOOL bIgnoreImportedMeshes=FALSE, TArray<APylon*>* IgnorePylons=NULL, DWORD TraceFlags=0);

	// queries the pylon octree and returns a list of pylons that intersect the given AABB
	static void GetIntersectingPylons(const FVector& Loc, const FVector& Extent, TArray<APylon*>& out_Pylons,class AActor* SrcActor=NULL);

	/**
	 * Given a line segment, walks along the segment returning all polys that it crosses as entry and exit points of that segment
	 * (will find spans from any navmesh in the world)
	 * @param Start - Start point of span to check
	 * @param End - end point of span
	 * @Param out_Spans - out array of spans found and the polys they link to
	 */
	static void GetPolySegmentSpanList( FVector& Start, FVector& End, TArray<struct FPolySegmentSpan>& out_Spans );

	/**
	 *  static function that will do an obstacle line check against any pylon's meshes colliding with the line passed
	 *  @param InOuter - The Outer which owns this NavHandle.  Can be used for debugging
	 *  @param Hit - Hitresult struct for line check
	 *  @param Start - start of line check
	 *  @param End - end of line check
	 *  @param Extent - extent of box to sweep
	 *  @param bIgnoreNormalMesh - OPTIONAL - default:FALSE - when TRUE no checks against the walkable mesh will be performed
	 *  @param out_HitPoly - optional output param stuffed with the poly we collided with (if any)
	 *  @param PylonsToCheck - OPTIONAL, if present only these pylons' meshes will be linecheck'd
	 *  @param TraceFlags - bitfield to control tracing options
	 *  @return TRUE if nothing hit
	 */
	static UBOOL StaticObstacleLineCheck( const UObject* const InOuter,
												 FCheckResult& Hit,
												 FVector Start,
												 FVector End,
												 FVector Extent,
												 UBOOL bIgnoreNormalMesh=FALSE,
												 FNavMeshPolyBase** out_HitPoly=NULL,
												 const TArray<APylon*>* PylonsToCheck=NULL,
												 DWORD TraceFlags=0);

	/**
	 * static function that will do a point check agains the obstacle mesh
	 * @param Hit - hitresult struct for point check
	 * @param Pt - centroid of extent box to point check
	 * @param Extent - extent of box to point check
	 * @param out_HitPoly - optional output param stuffed with the poly we collided with (if any)
	 * @param PylonsToCheck - OPTIONAL, if present only these pylons' meshes will be linecheck'd
	 * @param bSkipPointInMeshCheck - OPTIONAL, if TRUE ONLY a pointcheck against the obstacle mesh will be done, no verification that the point is on the mesh somewhere will be done (be careful with this one!)
	 * @return TRUE if nothing hit
	 */
	static UBOOL StaticObstaclePointCheck(FCheckResult& Hit,FVector Pt,FVector Extent, FNavMeshPolyBase** out_HitPoly=NULL, const TArray<APylon*>* PylonsToCheck=NULL, UBOOL bSkipPointInMeshCheck=FALSE, DWORD TraceFlags=0);

    /**
	 *  static function that will do a line check against any pylon's (walkable) meshes colliding with the line passed
	 *  @param Hit - Hitresult struct for line check
	 *  @param Start - start of line check
	 *  @param End - end of line check
	 *  @param Extent - extent of box to sweep
	 *  @param out_HitPoly - OPTIONAL, output param for hit poly
	 *  @param PylonsToCheck - OPTIONAL, if present only these pylons' meshes will be linecheck'd
	 *  @param TraceFlags - bitfield to control tracing options
	 *  @return TRUE if nothing hit
	 */
	static UBOOL StaticLineCheck(FCheckResult& Hit, FVector Start,FVector End,FVector Extent, FNavMeshPolyBase** out_HitPoly=NULL, const TArray<APylon*>* PylonsToCheck=NULL, DWORD TraceFlags=0);

	/**
	 * static function that will do a point check agains the walkable
	 * @param Hit - hitresult struct for point check
	 * @param Pt - centroid of extent box to point check
	 * @param Extent - extent of box to point check
	 * @param out_HitPoly - OPTIONAL, poly the pointcheck hit
	 * @param PylonsToCheck - OPTIONAL, if present only these pylons' meshes will be linecheck'd
	 * @return TRUE of nothing hit
	 */
	static UBOOL StaticPointCheck(FCheckResult& Hit,FVector Pt,FVector Extent, FNavMeshPolyBase** out_HitPoly=NULL, const TArray<APylon*>* PylonsToCheck=NULL, DWORD TraceFlags=0);

	static APylon* StaticGetPylonFromPos( FVector Position );

	UBOOL PathCache_Empty( FPathStore* PCache );
	UBOOL PathCache_AddEdge( FNavMeshEdgeBase* Edge, FPathStore* PCache = NULL  );
	UBOOL PathCache_InsertEdge( FNavMeshEdgeBase* Edge, int Idx=0, FPathStore* PCache = NULL  );
	UBOOL PathCache_RemoveEdge( FNavMeshEdgeBase* Edge, FPathStore* PCache = NULL  );
	UBOOL PathCache_RemoveIndex( int InIdx, int Count, FPathStore* PCache );
	FVector PathCache_GetGoalPoint( FPathStore* PCache );

	// Pathing functions

	/**
	 * internal function which does the heavy lifting for A* searches (typically called from FindPath()
	 * @param out_DestActor - output param goal evals can set if a particular actor was the result of the search
	 * @param out_DestItem - output param goal evans can set if they need an index into something (e.g. cover slot)
	 * @return - TRUE If search found a goal
	 */
	virtual UBOOL GeneratePath( class AActor** out_DestActor, INT* out_DestItem );
	

	/** 
	 *  Adds successor edges from the given poly to the A* search
	 *  @param CurPoly - poly to add sucessor edges for
	 *  @param PathParams - path params being used to search currently
	 *  @param PredecessorEdge - edge we got to this poly from
	 *  @param PathSessionID - SessionID for this pathfind
	 *  @param OpenList - first edge on the open list
	 */
	void AddSuccessorEdgesForPoly(FNavMeshPolyBase* CurPoly,
								const FNavMeshPathParams& PathParams,
								FNavMeshEdgeBase* PredecessorEdge,
								INT PathSessionID,
								PathOpenList& OpenList,
								INT OverrideVisitedCost=-1,
								INT OverrideHeuristicCost=-1);

	/**
	 * finds the best node in the list, and pulls it out and returns it
	 * (assumes list is sorted)
	 * @param OpenList - list to find best node from
	 * @return the best node in the list 
	 */
	PathCardinalType PopBestNode( PathOpenList& OpenList );

	/**
	 * InsertSorted
	 * inserts the passed node into the passed list at the proper spot to maintain sort order
	 * @param NodeForInsertion - node to insert into the list
	 * @param OpenList - list ot insert into
	 * @return TRUE if succesful
	 */
	UBOOL InsertSorted( PathCardinalType NodeForInsertion, PathOpenList& OpenList );
	UBOOL AddNodeToOpen( PathOpenList& OpenList,
						PathCardinalType NodeToAdd,
						INT EdgeCost,
						INT HeuristicCost,
						PathCardinalType Predecessor,
						const FVector& PrevPos,
						FNavMeshPolyBase* DestinationPolyForEdge);
	void RemoveNodeFromOpen( PathCardinalType NodeToRemove, PathOpenList& OpenList );

	/** 
	 * will loop through all constraints in the constraint list and let them have their say on the current cost of the edge being considered
	 * @param Edge - the edge being considered
	 * @param SrcPoly - the poly we're coming from 
	 * @param DestPoly - the poly we're going to!
	 * @param EdgeCost - output param with current edge cost, and should be updated cost after this function is called
	 * @param HeuristicCost - output param with current heuristiccost, and should be updated heuristic cost (h) after this function is called
	 * @param EdgePoint - the point on the edge being used for cost calculations ( a good place to do heuristic weighting )
	 * @return TRUE if this edge is fit at all, FALSE if it should be skipped
	 */
	UBOOL ApplyConstraints( FNavMeshEdgeBase* Edge, FNavMeshEdgeBase* PredecessorEdge, FNavMeshPolyBase* SrcPoly, FNavMeshPolyBase* DestPoly, INT& EdgeCost, INT& HeuristicCost, const FVector& EdgePoint );

	/**
	 * EvaluateGoal handles composition of goal evaluators, and will loop through the goaleval list
	 * calling EvaluateGoal on each one to determine if the possibleGoal is the node we're looking for
	 * @param PossibleGoal - the goal to be evaluated!
	 * @param out_GeneratedGoal - the poly that we have chosen as our successful goal (if any)
	 *                            @NOTE: this will be NULL'd if the collective goals say no to PossibleGOal
	 * @return TRUE if PossibleGoal is a valid node to stop on (search stops once this happens)
	 */
	UBOOL EvaluateGoal(  PathCardinalType PossibleGoal, PathCardinalType& out_GeneratedGoal );

	void ClearCrossLevelRefs(ULevel* Level);

	/**
	 * will clear all references to any navmeshes
	 */
	void ClearAllMeshRefs();

	/**
	 * PostEdgeCleanup
	 * this function is called after an edge has been cleaned up, but before it has been deleted. Whatever triggerd
	 * the edge deletion is finished, so it's safe to call other code that might affect the mesh
	 * @param Edge - the edge that is being cleaned up
	 */
	void PostEdgeCleanup(FNavMeshEdgeBase* Edge);

	// UObject Interface - overidden to clear pathcache on destruction
	virtual void BeginDestroy();

	/**
	 * will operate on the pathcache to generate a set of points to run through.. this is based on the edges in the path
	 * and a 'stringpull' method applied to the edges to find the best route through the edges
	 * @param Interface - the interface of the entity we're computing for
	 * @param PathIdx   - the index of the edge we are computing
	 * @param out_EdgePos - the output of the computation
	 * @param ArrivalDist - the radius around points which the entity will consider itself arrived (used to offset points to make sure the bot moves far enough forward)
	 * @param bConstrainedToCurrentEdge - skip compensating if the edge requires special movement, rely solely upon GetEdgeDestination()
	 * @param out_EdgePoints - optional pointer to an array to fill with all the computed edge movement points
	 */
	void ComputeOptimalEdgePosition(INT PathIdx, FVector& out_EdgePos, FLOAT ArrivalDistance, UBOOL bConstrainedToCurrentEdge = FALSE, TArray<FVector>* out_EdgePoints=NULL);

	/**
	 * This function will generate a move point which will get the bot into the next polygon
	 * by compensating for early-arrival if needed
	 * @param PathIdx - the index of the pathedge position we're trying to resolve (usually 0)
	 * @param out_movePosition - the position we determined to be best to move to (should be set with the current desired location to move to)
	 * @param NextMovePoitn    - the next move point in the path
	 * @param HandleExtent     - the extent of the handle we're resolving for
	 * @param CurHandlePos     - the current world position of the handle we're resolving for
	 * @param ArrivalDistance  - how close to a point we have ot be before moveto() will return
	 */
	void CompensateForEarlyArrivals(INT PathIdx, FVector& out_MovePosition, const FVector& NextMovePoint, const FVector& CurHandlePos, FLOAT ArrivalDistance);

	/**
	 * This function is called from GetNextMoveLocation when it is detected that the entity is not within its path
	 * and it tries to recover from this happening
	 * @param Interface - the handle interface that's walking this path
	 * @param SearchStart - the location of the entity searching (saves on interface vf calls)
	 * @param Extent	  - extent of the entity searching (saves on interface vf calls)
	 * @param ArrivalDistance - how clsoe to a point we have to get before moveto() will return
	 * @param out_Dest		- out_param dictating what our destination should be
	 * @return				- whether we were succesful in finding a valid point to move to
	 */
	UBOOL HandleNotOnPath(FLOAT ArrivalDistance, FVector& out_Dest );

	/**
	 * This function is called to give the Handle a chance to handled the adjustment instead of the controller fallback.
	 * @param HitNoraml - Normal of the wall at point of collision
	 * @param HitActor - Actor that we hit
	 * @return - TRUE if we handled the adjustment, FALSE means allow Outer (usually the AIController)
	 */
	UBOOL HandleWallAdjust( FVector HitNormal, AActor* HitActor );

	/**
	 * This function is called after an adjust to the wall is complete (usually from execPollMoveTo/ward)
	 * @return - TRUE if we handled the adjustment and no other move should occur, FALSE means allow regular movement to continue
	 */
	UBOOL HandleFinishedAdjustMove();

	/**
	 * This function is called by APawn::ReachedDestination and is used to coordinate when to stop moving to a point during
	 * path following (e.g. once we are inside the next poly, stop moving rather than trying to hit an exact point)
	 * @param Destination - destination we're trying to move to
	 * @param out_bReached - whether or not we have reached this destination
	 * @param HandleOuterActor - the actor which implements the navhandle interface for this test
	 * @param ArrivalTreshold  - the radius within which code elsewhere is going to be doing arrival checks, so we can match in this function without breaking parity
	 * @return - returns TRUE If this function was able to determien if we've arrived (FALSE means keep checking elsewhere)
	 */
	UBOOL ReachedDestination(const FVector& Destination, AActor* HandleOuterActor, FLOAT ArrivalThreshold, UBOOL& out_bReached);

	// don't let navmeshes whose edges we are reffing be deleted before we are
	void AddReferencedObjects( TArray<UObject*>& ObjectArray );
	void Serialize( FArchive& Ar );

	/**
	 * do an octree check and return all pylons whose bounds overlap the passed center/extent
	 * @param Ctr - center of box to check for overlap
	 * @param Extent - extent of box to check for overlap
	 * @param out_OverlappingPylons - out param stuffed with pylons
	 */
	static void GetAllOverlappingPylonsFromBox(const FVector& Ctr, const FVector& Extent, TArray<APylon*>& out_OverlappingPylons);

	/**
	 * called from PointReachable, and is recursive to split the cast into several that conform to the mesh
	 *  @param InOuter - The Outer which owns this NavHandle.  Can be used for debugging
	 *  @param Hit - Hitresult struct for line check
	 *  @param Start - start of line check
	 *  @param End - end of line check
	 *  @param Extent - extent of box to sweep
	 *  @param bIgnoreNormalMesh - OPTIONAL - default:FALSE - when TRUE no checks against the walkable mesh will be performed
	 *  @param out_HitPoly - optional output param stuffed with the poly we collided with (if any)
	 *  @param bComparePolyNormalZs - optional bool dictating whether this function should return a collision when any two polys found along
	 *                               the way have very different Normal.Z values
	 *  @param TraceFlags - bitfield to control tracing options
	 *  @return TRUE of nothing hit
	 */
	static UBOOL PointReachableLineCheck( const UObject* const InOuter,
								  FCheckResult& Hit,
								  FVector Start,
								  FVector End,
								  FVector Extent,
								  UBOOL bIgnoreNormalMesh=FALSE,
								  FNavMeshPolyBase** out_HitPoly=NULL,
								  UBOOL bComparePolyNormalZs=FALSE,
								  DWORD TraceFlags=0,
								  UINT StackDepth=0);
	/**
	 * called when a path is searched for and not found
	 * sets the lastpatherror and saves off the time of the failure
	 * @param ErrorType - the type of path error that just occurred 
	 */
	void SetPathError(EPathFindingError ErrorType);

	struct FFitNessFuncParams
	{
		FFitNessFuncParams( UNavigationHandle* InAskingHandle,
			const FVector& InStartPt,
			const FVector& InExtent,
			const FVector& InPoint,
			FNavMeshPolyBase* InPolyContainingPoint,
			const TArray<APylon*>* InPylonsWeCareAbout)
			:
		AskingHandle(InAskingHandle),
		StartPt(InStartPt),
		Extent(InExtent),
		Point(InPoint),
		PolyContainingPoint(InPolyContainingPoint),
		PylonsWeCareAbout(InPylonsWeCareAbout){}

		UNavigationHandle* AskingHandle;
		FVector StartPt;
		FVector Extent;
		FVector Point;
		FNavMeshPolyBase* PolyContainingPoint;
		const TArray<APylon*>* PylonsWeCareAbout;
	};
	// typedef for getvalidpositionsforbox acceptability function pointer
	typedef UBOOL (*ValidBoxPositionFunc)(const FFitNessFuncParams& Params);

	/**
	 * will return a list of valid spots on the mesh which fit the passed extent and are within radius to Pos
	 * @param Pos - Center of bounds to check for polys
	 * @param Radius - radius from Pos to find valid positions within
	 * @param Extent - Extent of entity we're finding a spot for
	 * @param bMustBeReachableFromStartPos - if TRUE, only positions which are directly reachable from the starting position will be returned
	 * @param ValidPositions - out var of valid positions for the passed entity size
	 * @param MaxPositions - the maximum positions needed (e.g. the search for valid positions will stop after this many have been found)
	 * @param MinRadius    - minimum distance from center position to potential spots (default 0)
	 * @param ValidBoxAroundStartPos - when bMustBeReachableFromStartPos is TRUE, all hits that are within this AABB of the start pos will be considered valid
	 * @param ValidBoxPositionFunc - function pointer which can be supplied to filter out potential points from the result list
	 */
	void GetValidPositionsForBoxEx(FVector pos,
		FLOAT Radius,
		FVector Extent,
		UBOOL bMustBeReachableFromStartPos,
		TArray<FVector>& out_ValidPositions,
		INT MaxPositions=-1,
		FLOAT MinRadius=0,
		FVector ValidBoxAroundStartPos=FVector(0.000000,0.000000,0.000000),
		ValidBoxPositionFunc FitnessFunction=NULL);

	// macros to support debuglog, should call these instead of debuglog directly so that it gets compiled out in release builds
	#if !DO_AI_LOGGING
	#define NAVHANDLE_DEBUG_LOG(TXT){}
	#else
	#define NAVHANDLE_DEBUG_LOG(TXT){\
		if(RUNTIME_DO_AI_LOGGING)\
		{\
			IInterface_NavigationHandle* Interface = InterfaceCast<IInterface_NavigationHandle>(GetOuter());\
			if(Interface != NULL)\
			{\
				Interface->DebugLogInternal(TXT);\
			}\
		}\
	}
	#endif
};

/** 
 *  sets the pathcache to a copy of the passed path store
 *  @param PathStore - path store to copy over pathcache
 */
native function CopyPathStoreToPathCache(const out PathStore InStore);

/**
 * checks if we've moved far enough, and if so updates the breadcrumb trail 
 * @param Location - current location
 */
native function UpdateBreadCrumbs(vector InLocation);

/**
 * will return the most recent breadcrumb, and pop it off the list
 */
native function bool GetNextBreadCrumb(out vector out_BreadCrumbLoc);

/**
 * Path constraint operations
 * Allows the user to push a list of constraints which affect pathing heuristics, as well as determine when the path traversal is finished
 */
native function ClearConstraints();
native function AddPathConstraint( NavMeshPathConstraint Constraint );
native function AddGoalEvaluator( NavMeshPathGoalEvaluator Evaluator );

/**
 * returns whether or not pylon A has any path to Pylon B (useful for high level early outs)
 */
native function Pylon BuildFromPylonAToPylonB(Pylon A, Pylon B);

/**
 * returns whether or not pylon A has any path to Pylon B (useful for high level early outs)
 */
native function bool DoesPylonAHaveAPathToPylonB(Pylon A, Pylon B);

/**
* Path shaping creation functions...
* these functions by default will just new the class, but this offers a handy
* interface to override for to do things like pool the constraints
*/
function NavMeshPathConstraint CreatePathConstraint( class<NavMeshPathConstraint> ConstraintClass )
{
	return WorldInfo.GetNavMeshPathConstraintFromCache(ConstraintClass,self);
}
function NavMeshPathGoalEvaluator CreatePathGoalEvaluator( class<NavMeshPathGoalEvaluator> GoalEvalClass )
{
	return WorldInfo.GetNavMeshPathGoalEvaluatorFromCache(GoalEvalClass,self);
}

function int GetPathCacheLength()
{
	return PathCache.EdgeList.Length;
}

/** Path Cache Operations
 *	Allows operations on nodes in the route while modifying route (ie before emptying the cache)
 *	Should override in subclasses as needed
 */
native function bool PathCache_Empty();

/**
 *  After FindPath has been called this will return the location in the world that the pathfind found based on the NavMeshGoals
 *  the FindPath used.
 **/
native function vector PathCache_GetGoalPoint();
native function bool PathCache_RemoveIndex(int InIdx, int Count = 1);

/**
 * This will return the best "unfinished path".  We need this for things where we are using the navmesh to find locations which
 * are not connected (pathable) to the originating NavHandle but are still valid world positions.
 **/
native function vector GetBestUnfinishedPathPoint() const;

// finds the the pylon the AI attached to this handle is within
native function bool FindPylon();
// finds the pylon (if any) assoicated with the position passed
native static function Pylon GetPylonFromPos( vector Position );

/**
 * will return the actual point that we should move to right now to walk along our path
 * this will usually be a point along our current edge, but sometimes will
 * be something else in special situations (e.g. right on top of the edge)
 * @param out_MoveDest - output movement destination we have determined
 * @param ArrivalDistance - this tells getnextmovelocation how close to a point we have to be before MoveTo() returns
 *                          necessary so we can compesnate for early arrivals in some situations
 */
native function bool GetNextMoveLocation( out Vector out_MoveDest, float ArrivalDistance );

/**
 * lets the navigation handle know what the ultimate goal of the current move is
 * @param FinalDest - the destination desired
 * @return - whether or not the final destination is reachable
 */
native function bool SetFinalDestination(Vector FinalDest);

/**
 *  ComputeValidFinalDestination
 *  will find a valid, pathable point near the passed desired destination
 */
native function bool ComputeValidFinalDestination(out vector out_ComputedPosition);

/**
 * this will set up a path search, and ultimately call GeneratePath to do the A* path search
 * Note: it's up to your constraints to determine what it is you're doing.. if you're trying to
 *       path to a particular point you probably want to add a NavmeshGoal_At goal evaluator
 *       and supply it with the position you're pathing toward so the goal evaluator can stop the path search
 *       once the destination is found.  You also need a path constraint to provide a heuristic for the search,
 *       which typically is going to consist of at least a NavMeshPath_Toward which will weight based on
 *       euclidian distance to the goal.
 * @param out_DestActor - output variable which goal evaluators can use to supply the 'found' actor at path finish
 * @param out_DestItem  - output variable which goal evaluators can use to supply extra data to path search clients
 * @return - whether the path search was successful or not
 */
native function bool FindPath( optional out Actor out_DestActor, optional out int out_DestItem );

/**
 * this will notify our curretn edge we're about to traverse it, and allow that edge to perform custom actions for traversal
 * @param MovePt - the point we're about to move to
 * @param C      - controller we're suggesting move prep for
 * @return TRUE if the edge is handling getting the bot to the proper position for the move, FALSE if
 *         calling code is responsible for getting th bot to movept
 */
native function bool SuggestMovePreparation( out Vector MovePt, Controller C );

/**
 * does a line check against the obstacle mesh
 * @param Start - start of the line segment to check
 * @param End   - end position of the line segment to check
 * @param Extent - extent of box to be swept along line segment
 * @param out_HitLoc - hit location of obstacle line check (if any)
 * @param out_hitNorm - hit normal of surface we hit during obstacle line check (if any)
 * @return - TRUE if nothing was hit (note: only collides against the obstacle mesh, not the normal mesh)
 */
static native final function bool ObstacleLineCheck( vector Start, vector End, vector Extent, optional out vector out_HitLoc, optional out vector out_HitNorm );
/**
 * Does a point check against the obstacle mesh
 * @param Pt - centroid of box to check aginast obstacle mesh
 * @param Extent - Extent of box to check against obstacl mesh
 * @return - TRUE if didn't hit anything
 */
static native final function bool ObstaclePointCheck(vector Pt, vector Extent);

/**
 * does a line check against the Walkable mesh
 * @param Start - start of the line segment to check
 * @param End   - end position of the line segment to check
 * @param Extent - extent of box to be swept along line segment
 * @return - TRUE if nothing was hit
 */
native function bool LineCheck( vector Start, vector End, vector Extent, optional out vector out_HitLocation, optional out vector out_HitNormal );

/**
 * Does a point check against the Walkable mesh
 * @param Pt - centroid of box to check against obstacle mesh
 * @param Extent - Extent of box to check against obstacle mesh
 * @return - TRUE if didn't hit anything
 */
native function bool PointCheck( vector Pt, vector Extent );


/**
 * returns TRUE if Point/Actor is directly reachable
 * @param Point - point we want to test to
 * @param OverrideStartPoint (optional) - optional override for starting position of AI (default uses bot location)
 * @param bAllowHitsInEndCollisionBox (optional) - optional.. (defaults to ON) if this is true and a hit is detected that falls within the collision cylinder of the entity, let the hit pass
 * @return TRUE if the point is reachable
 */
native function bool PointReachable( Vector Point, optional Vector OverrideStartPoint, optional bool bAllowHitsInEndCollisionBox=true);
native function bool ActorReachable( Actor A );

// debug function for drawing cache polys
native function DrawPathCache(optional Vector DrawOffset, optional bool bPersistent, optional color DrawColor);

// debug function which prints out info about the current path cache
native function PrintPathCacheDebugText();

/**
 * for debugging.. will return descriptive text about the current edge 
 */
native function string GetCurrentEdgeDebugText();

/**
 * NULLs the currentedge reference 
 */
native function ClearCurrentEdge();

/**
 * @returns the edge type for the current edge (if any)
 */
native function ENavMeshEdgeType GetCurrentEdgeType();

/**
 * returns the center points of all polys within the specefied area
 * @param Pos - Center of bounds to check for polys
 * @param Extent - Extent of area to return
 * @param out_PolyCtrs - out var of poly centers within the specefied area
 */
native static function GetAllPolyCentersWithinBounds(Vector Pos, Vector Extent, out Array<Vector> out_PolyCtrs);

/**
 * will return a list of valid spots on the mesh which fit the passed extent and are within radius to Pos
 * @param Pos - Center of bounds to check for polys
 * @param Radius - radius from Pos to find valid positions within
 * @param Extent - Extent of entity we're finding a spot for
 * @param bMustBeReachableFromStartPos - if TRUE, only positions which are directly reachable from the starting position will be returned
 * @param ValidPositions - out var of valid positions for the passed entity size
 * @param MaxPositions - the maximum positions needed (e.g. the search for valid positions will stop after this many have been found)
 * @param MinRadius    - minimum distance from center position to potential spots (default 0)
 * @param ValidBoxAroundStartPos - when bMustBeReachableFromStartPos is TRUE, all hits that are within this AABB of the start pos will be considered valid
 */
native static function GetValidPositionsForBox(Vector Pos, float Radius, Vector Extent, bool bMustBeReachableFromStartPos, out Array<Vector> out_ValidPositions, optional int MaxPositions=-1, optional float MinRadius, optional vector ValidBoxAroundStartPos=vect(0,0,0));

/**
 * will clip off edges from the pathcache which are greater than the specified distance from the start of the path
 * @param MaxDist - the maximum distance for the path
 */
native function LimitPathCacheDistance(float MaxDist);

/**
 * this function will determine if the poly this entity is currently in is inescapable by that entity
 * @return TRUE If the poly this handle is in isn't escapable
 */
native function bool IsAnchorInescapable();

/**
 * this will a good point on the first edge in the pathcache (or the finaldest if there is no pathcache)
 */
native function vector GetFirstMoveLocation();

/**
 * this will calculate the optimal edge positions along the pathcache, and add up the distances
 * to generate an accurate distance that will be travelled along the current path
 * Note: includes distance to final destination
 * @param FinalDest - optional finaldest override (if not passed will use NavigationHandle.FinalDestination)
 * @return - the path distance calculated
 */
native function float CalculatePathDistance(optional Vector FinalDest);

/**
 * Copies over move points into the passed vector array
 * @param FinalDest - 
 * @param out_MovePoints - array to be stuffed with move points
  */
native function CopyMovePointsFromPathCache(Vector FinalDest, out array<vector> out_MovePoints);

/**
 * this will take the given position and attempt to move it the passed height above the poly that point is in (along a cardinal axis)
 * @param Point - point to adjust
 * @param Height - height above mesh you would like
 * @return Adjusted point
 */
static native function Vector MoveToDesiredHeightAboveMesh(vector Point, float Height);

/**
 * This will attempt to grab an interface_navigationhandle from outer, and have that interface populate
 * our cached pathing params.
 * @return TRUE if cache population was succesful
 */
native final function bool PopulatePathfindingParamCache();

/**
 * Gather all cover slot info within radius of the given point
 * @param FromLoc - location at center of sphere to check
 * @param Radius - radius of sphere within which to gather cover
 * @param out_CoverList - out array filled with cover infos within radius
 * @return TRUE if found any cover
 */
static native final function bool GetAllCoverSlotsInRadius( Vector FromLoc, FLOAT Radius, out array<CoverInfo> out_CoverList );

/** 
 * will get a point nearby which is in a poly that has edges outbound that support this AI
 * @param out_NewAnchorLoc - out param stuffed with the position we found
 * @param OverrideStartLoc - optional param to override the starting location for this query (if none is given this AI's searchstart will be used)
 * @return - TRUE if we found a spot
 */
native final function bool GetValidatedAnchorPosition(out vector out_NewAnchorLoc, optional vector OverrideStartLoc );

/** 
 * will get a point nearby which is in a poly that has edges outbound that support this AI
 * @param out_NewAnchorLoc - out param stuffed with the position we found
 * @param StartCheckBaseLocation - location to start looking (to find poitns near)
 * @param Extent - extent of the AI we're trying to find a spot for
 * @return - TRUE if we found a spot
 */
static native final function bool StaticGetValidatedAnchorPosition(out vector out_NewAnchorLoc, Vector StartCheckBaseLocation, Vector Extent );

function DrawBreadCrumbs(bool bPersistent=false)
{
`if(`notdefined(FINAL_RELEASE))

	local int i,count;
	local vector lastPt,curPt;

	if( bDebug_Breadcrumbs )
	{
		if( bPersistent) FlushPersistentDebugLines();
		for(i=BreadCrumbMostRecentIdx;count < NumBreadCrumbs; ++count)
		{
			curPt = Breadcrumbs[i];
			if( curPt == vect(0,0,0) )
			{
				break;
			}
			if( lastPt != vect(0,0,0) && count < NumBreadCrumbs-1)
			{
				DrawDebugLine(curPt,LastPt,0,255,0,bPersistent);
			}

			DrawDebugBox(curPt,vect(5,5,5),0,0,255,bPersistent);
			--i;
			if( i<0 )
			{
				i=NumBreadCrumbs-1;
			}
			lastPt=CurPt;
		}
	}
	`endif
}
defaultproperties
{
	bDebugConstraintsAndGoalEvals=FALSE
	BreadCrumbDistanceInterval=75
	bDebug_Breadcrumbs=false
}
