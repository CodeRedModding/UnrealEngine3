/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class NavMeshGoal_At extends NavMeshPathGoalEvaluator
	native(AI);

cpptext
{
	// Interface
	virtual UBOOL InitializeSearch( UNavigationHandle* Handle, const FNavMeshPathParams& PathParams);
	virtual UBOOL EvaluateGoal( PathCardinalType PossibleGoal, const FNavMeshPathParams& PathParams, PathCardinalType& out_GenGoal );
	virtual void  NotifyExceededMaxPathVisits( PathCardinalType BestGuess, PathCardinalType& out_GenGoal );
	virtual UBOOL DetermineFinalGoal( PathCardinalType& out_GenGoal, class AActor** out_DestActor, INT* out_DestItem );
	virtual UBOOL SeedWorkingSet( PathOpenList& OpenList,
								FNavMeshPolyBase* AnchorPoly,
								DWORD PathSessionID,
								UNavigationHandle* Handle,
								const FNavMeshPathParams& PathParams);

}

/** Location to reach */
var Vector Goal;
/** Within this acceptable distance */
var float GoalDist;
/** Should keep track of cheapest path even if don't reach goal */
var bool bKeepPartial;
/** if set evaluate best partial path by line distance to goal */
var bool bWeightPartialByDist;
var float PartialDistSq;
var bool bGoalInSamePolyAsAnchor;

// the polygon that contains our goal point
var private native pointer GoalPoly{FNavMeshPolyBase};

// the last partial goal (used when no true path is found)
var private native pointer PartialGoal{FNavMeshEdgeBase};

native function RecycleNative();

static function bool AtActor(NavigationHandle NavHandle, Actor GoalActor, optional float Dist, optional bool bReturnPartial, optional bool bInWeightPartialByDist)
{
	local Controller GoalController;
	local Controller MyController;
	local vector TargetLoc;

	if (NavHandle != None)
	{
		GoalController = Controller(GoalActor);
		if (GoalController != None)
		{
			GoalActor = GoalController.Pawn;
		}

		if (GoalActor != None)
		{
			MyController = Controller(NavHandle.Outer);
			NavHandle.PopulatePathfindingParamCache();
			TargetLoc = GoalActor.GetDestination(MyController);
			return AtLocation(NavHandle, TargetLoc, Dist, bReturnPartial);
		}
	}

	return false;
}

static function bool AtLocation(NavigationHandle NavHandle, Vector GoalLocation, optional float Dist, optional bool bReturnPartial, optional bool bInWeightPartialByDist)
{
	local NavMeshGoal_At Eval;

	if (NavHandle != None)
	{
		Eval = NavMeshGoal_At(NavHandle.CreatePathGoalEvaluator(default.class));

		if (Eval != None)
		{
			Eval.Goal = GoalLocation;
			Eval.GoalDist = Dist;
			Eval.bKeepPartial = bReturnPartial;
			Eval.bWeightPartialByDist = bInWeightPartialByDist;
			NavHandle.AddGoalEvaluator(Eval);
			return true;
		}
	}

	return false;
}


function Recycle()
{
	Goal = vect(0,0,0);
	GoalDist = default.GoalDist;
	bKeepPartial = default.bKeepPartial;
	bWeightPartialByDist = default.bWeightPartialByDist;
	PartialDistSq = default.PartialDistSq;
	RecycleNative();
	Super.Recycle();
}

defaultproperties
{
	MaxPathVisits=1024
	PartialDistSq=1000000000000.0

	bDoPartialAStar=true
	MaxOpenListSize=2048

}
