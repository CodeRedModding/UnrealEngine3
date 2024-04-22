/**
 * This can have state.  This is the primary difference between GoalEvaluators and PathContraints.
 * 
 * Additionally, once the Goal's EvaluateGoal returns TRUE that path search will end.
 * 
 * A goal is a great place to have EvaluateGoal aggregate all of the possible "goals" that have passed the
 * constraints.  At some point (time based, search space based) EvaluateGoal will return TRUE and then 
 * DetermineFinalGoal will be called which one can then do final evaluation of the valid (from the path constraints) 
 * goals.
 * 
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class NavMeshPathGoalEvaluator extends Object
	native(AI);

cpptext
{
	// public Interface
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

	/** 
	 * sets up internal vars for path searching, and will early out if something fails
	 * @param Handle - handle we're initializing for
	 * @param PathParams - pathfinding parameter packet
	 * @return - whether or not we should early out form this search
	 */
	virtual UBOOL InitializeSearch( UNavigationHandle* Handle,
									const FNavMeshPathParams& PathParams );
	
	/**
	 * Called each time a node is popped off the working set to determine
	 * whether or not we should finish the search (e.g. did we find the node we're looking for)
	 * @param PossibleGoal - the chosen (cheapest) successor from the open list
	 * @param PathParams   - the cached pathfinding params for the pathing entity
	 * @param out_GenGoal  - the node we should consider the 'goal'.  (Normally PossibleGOal when this returns true, but doesn't have to be)
	 * @return - TRUE indicates we have found the node we're looking for and we should stop the search
	 */
	virtual UBOOL EvaluateGoal( PathCardinalType PossibleGoal,
								const FNavMeshPathParams& PathParams,
								PathCardinalType& out_GenGoal );
	
	/**
	 * after the search is over this is called to allow the goal evaluator to determine the final result from the search.
	 * this is useful if your search is gathering lots of nodes and you need to pick the most fit after your search is complete
	 * @param out_GenGoal - the edge that got us to our final goal
	 * @param out_DestACtor - custom user usable actor output pointer
	 * @param out_DestItem  - custom user usable integter output 
	 * @return - if no final goal could be determined this should return false inidcating failure
	 */
	virtual UBOOL DetermineFinalGoal( PathCardinalType& out_GenGoal, class AActor** out_DestActor, INT* out_DestItem );
	
	/**
	 * called when we have hit our upper bound for path iterations, allows 
	 * evaluators to handle this case specifically to their needs
	 * @param BestGuess - last visited edge from open list which is our best guess 
	 * @param out_GenGoal - current generated goal
	 */
	virtual void  NotifyExceededMaxPathVisits( PathCardinalType BestGuess, PathCardinalType& out_GenGoal );

	/**
	 * walks the previousPath chain back and saves out edges into the handle's pathcache for that handle to follow
	 * @param StartingPoly - the Polygon we are walking backwards toward
	 * @param GoalPoly     - the polygon to begin walking backwards from
	 * @param Handle	   - the handle to save the path out to 
	 * @param GoalEdge     - the edge that lead us to the goal poly
	 */
	virtual void SaveResultingPath( FNavMeshPolyBase* StartingPoly, FNavMeshPolyBase* GoalPoly, UNavigationHandle* Handle, PathCardinalType GoalEdge );

	/** 
	 *  Allows any pathobjects in the path to modify the final path after it has been generated
	 *  @param Handle - the navigation handle we're pathfinding for
	 *  @return - TRUE if a path object modified the path
	 */
	virtual UBOOL DoPathObjectPathMods( UNavigationHandle* Handle );
}

/** list of goals to search for */
struct native BiasedGoalActor
{
	/** the goal to search for */
	var Actor Goal;
	/** an additional cost (in units) to add to the pathfinding distance to bias against this choice
	 * (e.g. if one choice is enough better than the others that it should be prioritized if it's only a little bit further away)
	 */
	var int ExtraCost;
};

/** Next goal evaluator */
var transient protected NavMeshPathGoalEvaluator NextEvaluator;

/** maximum number of NavigationPoints to test before giving up */
var protected int MaxPathVisits;


/** this bool determines if this evaluator's 'EvaluageGoal' function gets called even after a determination has been made
 *  about the current goal.  E.G. a previous evaluator returned FALSE indicating the search was not complete, but we still want
 *  EvaluateGoal called even when the outcome has already been decided.  This is useful for 
 *  evaluators that need to see all incoming candidates regardless of whether or not another evaluator is throwing htem out
 */
var bool bAlwaysCallEvaluateGoal;

/** Debug var to keep track of how many nodes this goal evaluator has nixed */
var transient int NumNodesThrownOut;
/** Debug var to keep track of how many nodes this goal evaluator has processed */
var transient int NumNodesProcessed;

/** bool indicating whether we should attempt partial A* and restrict the open list size */
var bool bDoPartialAStar;
/** when doing partial Astar, limit open list to this size */
var int MaxOpenListSize;

event Recycle()
{
	NumNodesThrownOut=0;
	NumNodesProcessed=0;
	NextEvaluator=none;
}

event String GetDumpString()
{
	return String(self);
}

defaultproperties
{
	MaxPathVisits=1024
}
