//=============================================================================
// ReachSpec.
//
// A Reachspec describes the reachability requirements between two NavigationPoints
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class ReachSpec extends Object
	native;

const BLOCKEDPATHCOST = 10000000; // any path cost >= this value indicates the path is blocked to the pawn

/** pointer to object in navigation octree */
var native transient const editconst pointer NavOctreeObject{struct FNavigationOctreeObject};

cpptext
{
	/*
	supports() -
	returns true if it supports the requirements of aPawn.  Distance is not considered.
	*/
	inline UBOOL supports (INT iRadius, INT iHeight, INT moveFlags, INT iMaxFallVelocity)
	{
		return ( (CollisionRadius >= iRadius)
			&& (CollisionHeight >= iHeight)
			&& ((reachFlags & moveFlags) == reachFlags)
			&& (MaxLandingVelocity <= iMaxFallVelocity) );
	}

	FVector GetDirection();

	/** CostFor()
	Returns the "cost" in unreal units
	for Pawn P to travel from the start to the end of this reachspec
	*/
	virtual INT CostFor( APawn* P );
	/** AdjustedCostFor
	*	Used by NewBestPathTo for heuristic weighting
	*/
	virtual INT AdjustedCostFor( APawn* P, const FVector& StartToGoalDir, ANavigationPoint* Goal, INT Cost );
	virtual UBOOL PrepareForMove( AController * C );
	virtual UBOOL IsForced() { return false; }
	virtual UBOOL IsProscribed() const { return false; }
	virtual INT defineFor (class ANavigationPoint *begin, class ANavigationPoint * dest, class APawn * Scout);
	int operator<= (const UReachSpec &Spec);
	virtual FPlane PathColor();
	virtual void AddToDebugRenderProxy(class FDebugRenderSceneProxy* DRSP);
	int findBestReachable(class AScout *Scout);
	UBOOL ShouldPruneAgainst( UReachSpec* Spec );

	/** If bAddToNavigationOctree is true, adds the ReachSpec to the navigation octree */
	void AddToNavigationOctree();
	void RemoveFromNavigationOctree();
	/** returns whether TestBox overlaps the path this ReachSpec represents
	 * @note this function assumes that TestBox has already passed a bounding box overlap check
	 * @param TestBox the box to check
	 * @return true if the box doesn't overlap this path, false if it does
	 */
	UBOOL NavigationOverlapCheck(const FBox& TestBox);
	/** returns whether Point is within MaxDist units of the path this ReachSpec represents
	 * @param Point the point to check
	 * @param MaxDist the maximum distance the point can be from the path
	 * @return true if the point is within MaxDist units, false otherwise
	 */
	UBOOL IsOnPath(const FVector& Point, FLOAT MaxDist);
	/** returns whether this path is currently blocked and unusable to the given pawn */
	UBOOL IsBlockedFor(APawn* P);

	virtual void FinishDestroy();

	/** Get path size for a forced path between Start/End */
	virtual FVector GetForcedPathSize( class ANavigationPoint* Start, class ANavigationPoint* End, class AScout* Scout );

	/** return TRUE if it's safe to skip ahead past this edge, FALSE otherwise */
	virtual UBOOL CanBeSkipped( APawn* P )
	{
		return TRUE;
	}

	//debug
	FString PrintDebugInfo();
}

var	int		Distance;
var Vector	Direction;	// only valid when both start/end are static
var() const editconst NavigationPoint	Start;		// navigationpoint at start of this path
var() const editconst ActorReference	End;
var() const editconst int				CollisionRadius;
var() const editconst int				CollisionHeight;
var	int		reachFlags;					// see EReachSpecFlags definition in UnPath.h
var	int		MaxLandingVelocity;
var	byte	bPruned;
var byte	PathColorIndex;				// used to look up pathcolor, set when reachspec is created
/** whether or not this ReachSpec should be added to the navigation octree */
var const editconst bool bAddToNavigationOctree;
/** If true, pawns moving along this path can cut corners transitioning between this reachspec and adjacent reachspecs */
var bool bCanCutCorners;
/** whether AI should check for dynamic obstructions (Actors with bBlocksNavigation=true) when traversing this ReachSpec */
var bool bCheckForObstructions;
/** Prune paths should skip trying to prune along these */
var const bool	bSkipPrune;
/** Can always prune against these types of specs (even though class doesn't match) */
var const array< class<ReachSpec> > PruneSpecList;

/** Actor that is blocking this ReachSpec, making it temporarily unusable */
var Actor BlockedBy;
/** Reachspec has been disabled/blocked by kismet */
var() editconst bool  bDisabled;

/** CostFor()
Returns the "cost" in unreal units
for Pawn P to travel from the start to the end of this reachspec
*/
native final noexport function int CostFor(Pawn P);

/**
 * Returns nav point reference at end of spec
 */
native final noexport function NavigationPoint GetEnd();

/**
 * Returns direction of this reach spec (considers non-static nodes)
 */
native final noexport function Vector GetDirection();

function bool IsBlockedFor(Pawn P)
{
	return (CostFor(P) >= BLOCKEDPATHCOST);
}

defaultproperties
{
	bAddToNavigationOctree=true
	bCanCutCorners=true
	bCheckForObstructions=true
}
