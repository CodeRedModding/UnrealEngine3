/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/
class PathGoalEvaluator extends Object
	native(AI);

cpptext
{
	// Interface
	virtual UBOOL InitialAbortCheck( ANavigationPoint* Start, APawn* Pawn );
	virtual UBOOL EvaluateGoal(ANavigationPoint*& PossibleGoal, APawn* Pawn);
	virtual UBOOL DetermineFinalGoal( ANavigationPoint*& out_GoalNav );
	virtual void  NotifyExceededMaxPathVisits( ANavigationPoint* BestGuess );
}

/** Next goal evaluator */
var protected PathGoalEvaluator NextEvaluator;

/** Goal that was reached */
var protected NavigationPoint GeneratedGoal;

/** maximum number of NavigationPoints to test before giving up */
var protected int MaxPathVisits;

// index into the goaleval cache for this class' pool
var const int CacheIdx;

event Recycle()
{
	GeneratedGoal=none;
	NextEvaluator=none;
}

event String GetDumpString()
{
	return String(self);
}

defaultproperties
{
	MaxPathVisits=2048

	CacheIdx=-1
}
