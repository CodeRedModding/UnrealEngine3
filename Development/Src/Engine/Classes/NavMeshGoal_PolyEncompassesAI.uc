/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * this goal will throw out polygons which can't fully fit the entity searching
 * this is useful for open-ended path searches (e.g. find any poly outside of a radius) because
 * an edge may support the entity allowing the traversal to enter a polygon, but the entity might not necessarily fully 
 * fit inside the polygon, even though he could move through it
 */
class NavMeshGoal_PolyEncompassesAI extends NavMeshPathGoalEvaluator
	native(AI);


/** This is what we are going to check and make certain we have enough space to spawn this size of extent **/
var transient vector OverrideExtentToCheck;

cpptext
{
	// Interface
	virtual UBOOL EvaluateGoal( PathCardinalType PossibleGoal, const FNavMeshPathParams& PathParams, PathCardinalType& out_GenGoal );	
}


static function bool MakeSureAIFits( NavigationHandle NavHandle, optional const vector InOverrideExtentToCheck  )
{
	local NavMeshGoal_PolyEncompassesAI	Eval;

	if( NavHandle != None )
	{
		Eval = NavMeshGoal_PolyEncompassesAI(NavHandle.CreatePathGoalEvaluator(default.class));

		if( Eval != None )
		{
			Eval.OverrideExtentToCheck = InOverrideExtentToCheck;
			NavHandle.AddGoalEvaluator( Eval );
			return TRUE;
		}
	}

	return FALSE;
}

function Recycle()
{
	Super.Recycle();
}

defaultproperties
{
	// staticobstaclepoint checks performed by EvaluateGoal, use this sparingly	
	MaxPathVisits=64
}
