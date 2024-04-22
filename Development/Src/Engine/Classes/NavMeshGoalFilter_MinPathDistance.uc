/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class NavMeshGoalFilter_MinPathDistance extends NavMeshGoal_Filter
	native(AI);

var transient protected int MinDistancePathShouldBe;


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


static function bool MustBeLongerPathThan( NavMeshGoal_GenericFilterContainer FilterContainer, int InMinDistancePathShouldBe )
{
	local NavMeshGoalFilter_MinPathDistance	Eval;

	if( FilterContainer != None )
	{
		Eval = NavMeshGoalFilter_MinPathDistance(FilterContainer.GetFilterOfType(default.class));

		if( Eval != None )
		{
			Eval.MinDistancePathShouldBe = InMinDistancePathShouldBe;
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