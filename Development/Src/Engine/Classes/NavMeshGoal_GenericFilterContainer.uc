/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * this goal eval will not stop until its out of paths, and will simply return the node with the least cost
 */
class NavMeshGoal_GenericFilterContainer extends NavMeshPathGoalEvaluator
	native(AI);

cpptext
{
	// NavMeshPathGoalEvaluator Interface
	virtual UBOOL EvaluateGoal( PathCardinalType PossibleGoal, const FNavMeshPathParams& PathParams, PathCardinalType& out_GenGoal );
	
	/**
	 * this will ask each filter in this guy's list if the passed poly is a viable seed to get added at start time
	 * @param PossibleSeed - the seed to check viability for
	 * @param PathParams - params for entity searching
	 */
	virtual UBOOL IsValidSeed( FNavMeshPolyBase* PossibleSeed, const FNavMeshPathParams& PathParams );

	/** 
	 * sets up internal vars for path searching, and will early out if something fails
	 * @param Handle - handle we're initializing for
	 * @param PathParams - pathfinding parameter packet
	 * @return - whether or not we should early out form this search
	 */
	virtual UBOOL InitializeSearch( UNavigationHandle* Handle,
									const FNavMeshPathParams& PathParams );



	/**
	 * called when we have hit our upper bound for path iterations, allows 
	 * evaluators to handle this case specifically to their needs
	 * @param BestGuess - last visited edge from open list which is our best guess 
	 * @param out_GenGoal - current generated goal
	 */
	virtual void NotifyExceededMaxPathVisits( PathCardinalType BestGuess, PathCardinalType& out_GenGoal );


	/**
	 * adds initial nodes to the working set.  For basic searches this is just the start node.
	 * @param OpenList		- Pointer to the start of the open list
	 * @param AnchorPoly    - the anchor poly (poly the entity that's searching is in)
	 * @param PathSessionID - unique ID fo this particular path search (used for cheap clearing of path info)
	 * @param PathParams    - the cached pathfinding parameters for this path search
	 * @return - whether or not we were successful in seeding the search
	 */
	virtual UBOOL SeedWorkingSet( PathOpenList& OpenList,
								FNavMeshPolyBase* AnchorPoly,
								DWORD PathSessionID,
								UNavigationHandle* Handle,
								const FNavMeshPathParams& PathParams);



}

var transient instanced array<NavMeshGoal_Filter> GoalFilters;

/** storage of the goal we found an determined was OK (for use when goal does not have a path, but we still want to know what the goal was) */
var transient private native pointer SuccessfulGoal{FNavMeshEdgeBase};

/** Ref to our NavHandle so we can interrogate it for Debug flags. **/
var transient protected NavigationHandle MyNavigationHandle;

/** array of locations that should be used to seed the working set initially  */
var transient array<vector> SeedLocations;



static function NavMeshGoal_GenericFilterContainer CreateAndAddFilterToNavHandle( NavigationHandle NavHandle, optional int InMaxPathVisits=-1)
{
	local NavMeshGoal_GenericFilterContainer	Eval;

	if( NavHandle != None )
	{
		Eval = NavMeshGoal_GenericFilterContainer(NavHandle.CreatePathGoalEvaluator(default.class));

		if( Eval != None )
		{
			if(InMaxPathVisits > 0)
			{
				Eval.MaxPathVisits = InMaxPathVisits;
			}

			Eval.MyNavigationHandle = NavHandle;
			Eval.SeedLocations[Eval.SeedLocations.length] = NavHandle.CachedPathParams.SearchStart;
			NavHandle.AddGoalEvaluator( Eval );
			return Eval;
		}
	}

	return none;
}

static function NavMeshGoal_GenericFilterContainer CreateAndAddFilterToNavHandleFromSeedList( NavigationHandle NavHandle, out array<vector> InSearchSeeds, optional int InMaxPathVisits=-1)
{
	local NavMeshGoal_GenericFilterContainer	Eval;

	if( NavHandle != None )
	{
		Eval = NavMeshGoal_GenericFilterContainer(NavHandle.CreatePathGoalEvaluator(default.class));

		if( Eval != None )
		{
			if(InMaxPathVisits > 0)
			{
				Eval.MaxPathVisits = InMaxPathVisits;
			}

			Eval.MyNavigationHandle = NavHandle;
			Eval.SeedLocations = InSearchSeeds;
			NavHandle.AddGoalEvaluator( Eval );
			return Eval;
		}
	}

	return none;
}

// indireciton to hook into a pool or something if we want
function NavMeshGoal_Filter GetFilterOfType(class<NavMeshGoal_Filter> Filter_Class)
{
	return new(self) Filter_Class;
}

/**
 * returns the center of the poly we found as a valid goal, or 0,0,0 if none found (uses SuccessfulGoal member var0
 */
native function vector GetGoalPoint();



function Recycle()
{
	Super.Recycle();

	GoalFilters.length = 0;
	MaxPathVisits = default.maxPathVisits;
	MyNavigationHandle = None;
}

defaultproperties
{
	MaxPathVisits=2048
}
