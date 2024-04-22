/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
* this goal eval will not stop until its out of paths, and will simply return the node with the least cost
*/
class Goal_Null extends PathGoalEvaluator
	native(AI);

cpptext
{
	// Interface
	virtual UBOOL EvaluateGoal(ANavigationPoint*& PossibleGoal, APawn* Pawn);
	virtual void NotifyExceededMaxPathVisits( ANavigationPoint* BestGuess ){/*don't care about best guess.. just ignore this*/}
}

static function bool GoUntilBust( Pawn P, optional int InMaxPathVisits=-1 )
{
	local Goal_Null	Eval;

	if( P != None )
	{
		Eval = Goal_Null(P.CreatePathGoalEvaluator(default.class));

		if( Eval != None )
		{
			if(InMaxPathVisits > 0)
			{
				Eval.MaxPathVisits = InMaxPathVisits;
			}
			P.AddGoalEvaluator( Eval );
			return TRUE;
		}
	}

	return FALSE;
}

function Recycle()
{
	Super.Recycle();
	MaxPathVisits=default.maxPathVisits;
}

defaultproperties
{
	CacheIdx=5
	MaxPathVisits=2048
}
