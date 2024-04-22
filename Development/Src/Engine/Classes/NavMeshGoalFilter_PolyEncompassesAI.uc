/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * this goal will throw out polygons which can't fully fit the entity searching
 * this is useful for open-ended path searches (e.g. find any poly outside of a radius) because
 * an edge may support the entity allowing the traversal to enter a polygon, but the entity might not necessarily fully 
 * fit inside the polygon, even though he could move through it
 */
class NavMeshGoalFilter_PolyEncompassesAI extends NavMeshGoal_Filter
	native(AI);


/** This is what we are going to check and make certain we have enough space to spawn this size of extent **/
var transient vector OverrideExtentToCheck;

cpptext
{
	// Interface
		/**
	 * Called on each Goal_Filter that is in the list of a generic filter container to determine a goal's fitness for being 'the one'
	 * @param PossibleGoal - the chosen (cheapest) successor from the open list
	 * @param PathParams   - the cached pathfinding params for the pathing entity
	 * @return - TRUE indicates according to this filter's criteria this goal is valid
	 */
	virtual UBOOL IsValidFinalGoal( PathCardinalType PossibleGoal,
								const FNavMeshPathParams& PathParams);
}


static function bool MakeSureAIFits( NavMeshGoal_GenericFilterContainer FilterContainer, optional const vector InOverrideExtentToCheck )
{
	local NavMeshGoalFilter_PolyEncompassesAI	Eval;

	if( FilterContainer != None )
	{
		Eval = NavMeshGoalFilter_PolyEncompassesAI(FilterContainer.GetFilterOfType(default.class));

		if( Eval != None )
		{
			Eval.OverrideExtentToCheck = InOverrideExtentToCheck;
			FilterContainer.GoalFilters.AddItem(Eval);
			return TRUE;
		}
	}

	return FALSE;
}



defaultproperties
{
	bShowDebug=FALSE
}