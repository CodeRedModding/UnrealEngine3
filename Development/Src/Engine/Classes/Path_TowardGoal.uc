/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class Path_TowardGoal extends PathConstraint
	native(AI);

cpptext
{
	// Interface
	virtual UBOOL EvaluatePath( UReachSpec* Spec, APawn* Pawn, INT& out_PathCost, INT& out_HeuristicCost );
}

/** Goal trying to find path toward */
var Actor	GoalActor;

static function bool TowardGoal( Pawn P, Actor Goal )
{
	local Path_TowardGoal Con;

	if( P != None && Goal != None )
	{
		Con = Path_TowardGoal(P.CreatePathConstraint(default.class));
		if( Con != None )
		{
			Con.GoalActor = Goal;
			P.AddPathConstraint( Con );
			return TRUE;
		}
	}

	return FALSE;
}

function Recycle()
{
	Super.Recycle();
	GoalActor=none;
}

defaultproperties
{
	Cacheidx=1
}
