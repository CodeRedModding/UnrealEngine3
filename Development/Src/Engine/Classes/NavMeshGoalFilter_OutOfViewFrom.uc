/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class NavMeshGoalFilter_OutOfViewFrom extends NavMeshGoal_Filter
	native(AI);

// the polygon that contains our goal point
var transient  private native pointer GoalPoly{FNavMeshPolyBase};

var transient vector OutOfViewLocation;

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


static function bool MustBeHiddenFromThisPoint( NavMeshGoal_GenericFilterContainer FilterContainer, Vector InOutOfViewLocation  )
{
	local NavMeshGoalFilter_OutOfViewFrom	Eval;

	if( FilterContainer != None )
	{
		Eval = NavMeshGoalFilter_OutOfViewFrom(FilterContainer.GetFilterOfType(default.class));

		if( Eval != None )
		{
			Eval.OutOfViewLocation = InOutOfViewLocation;
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