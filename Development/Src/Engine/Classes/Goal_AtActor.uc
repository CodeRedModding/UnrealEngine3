/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class Goal_AtActor extends PathGoalEvaluator
	native(AI);

cpptext
{
	// Interface
	virtual UBOOL InitialAbortCheck( ANavigationPoint* Start, APawn* Pawn );
	virtual UBOOL EvaluateGoal(ANavigationPoint*& PossibleGoal, APawn* Pawn);
	virtual void  NotifyExceededMaxPathVisits( ANavigationPoint* BestGuess );
}

/** Actor to reach */
var Actor GoalActor;
/** Within this acceptable distance */
var float GoalDist;
/** Should keep track of cheapest path even if don't reach goal */
var bool  bKeepPartial;

static function bool AtActor( Pawn P, Actor Goal, optional float Dist, optional bool bReturnPartial )
{
	local Goal_AtActor Eval;
	local Pawn GoalPawn;
	local Controller GoalController;
	local float AnchorDist;

	if( P != None )
	{
		GoalPawn = Pawn(Goal);
		GoalController = Controller(Goal);
		if( GoalController != None )
		{
			if( GoalController.Pawn != None )
			{
				GoalPawn = GoalController.Pawn;
			}
			else
			{
				Goal = None;
			}
		}
		// redirect to best nav point if possible
		if( GoalPawn != None )
		{
			// If moving to a pawn with a valid anchor, make sure the anchor can support the searching pawn
			if( GoalPawn.ValidAnchor() && GoalPawn.Anchor.IsUsableAnchorFor( P ) )
			{
				Goal = GoalPawn.Anchor;
			}
			else
			{
				Goal = P.GetBestAnchor(GoalPawn, GoalPawn.Location, FALSE, FALSE, AnchorDist);
			}
		}
		else if (NavigationPoint(Goal) == None)
		{
			Goal = P.GetBestAnchor(Goal, Goal.Location, false, false, AnchorDist);
			if(Goal == none)
			{
				`log("PATHWARNING: Not pushing AtActor goal constraint because we couldn't find an anchor for goal!");
			}
		}

		if( Goal != None )
		{
			Eval = Goal_AtActor(P.CreatePathGoalEvaluator(default.class));

			if( Eval != None )
			{
				Eval.GoalActor		= Goal;
				Eval.GoalDist		= Dist;
				Eval.bKeepPartial	= bReturnPartial;
				P.AddGoalEvaluator( Eval );
				return TRUE;
			}
		}
	}

	return FALSE;
}

function Recycle()
{
	GoalActor = none;
	GoalDist = default.GoalDist;
	bKeepPartial = default.bKeepPartial;
	Super.Recycle();
}

defaultproperties
{
	CacheIdx=0
	MaxPathVisits=1024
}
