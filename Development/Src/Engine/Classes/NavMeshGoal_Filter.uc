/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * -this is a helper base class which is for filters that are meant to be used in conjunction with NavMshGoal_GenericFilterContainer
 *  these goals should only answer this question "is this a valid final goal or not?" and do nothing else.  
 */
class NavMeshGoal_Filter extends Object
	native(AI)
	abstract;

var bool bShowDebug;


/** Debug var to keep track of how many nodes this goal evaluator has nixed */
var transient int NumNodesThrownOut;
/** Debug var to keep track of how many nodes this goal evaluator has processed */
var transient int NumNodesProcessed;


cpptext
{
	/**
	 * Called on each Goal_Filter that is in the list of a generic filter container to determine a goal's fitness for being 'the one'
	 * @param PossibleGoal - the chosen (cheapest) successor from the open list
	 * @param PathParams   - the cached pathfinding params for the pathing entity
	 * @return - TRUE indicates according to this filter's criteria this goal is valid
	 */
	virtual UBOOL IsValidFinalGoal( PathCardinalType PossibleGoal,
								const FNavMeshPathParams& PathParams){return FALSE;}

	/**
	 * Called on each filter in the GenericFilterContainer to verify that a particular node is valid to be added
	 * as a seed poly (default is to just call IsValidFinalGoal)
 	 * @param PossibleGoal - the chosen (cheapest) successor from the open list
	 * @param PathParams   - the cached pathfinding params for the pathing entity
	 * @return - TRUE indicates according to this filter's criteria this goal is valid
	 */
	virtual UBOOL IsValidSeed( FNavMeshPolyBase* PossibleGoal,
								const FNavMeshPathParams& PathParams){return TRUE;}
	 
}

event String GetDumpString()
{
	return String(self);
}



defaultproperties
{
	bShowDebug=FALSE
}
