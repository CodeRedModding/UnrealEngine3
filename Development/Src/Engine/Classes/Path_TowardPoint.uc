/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class Path_TowardPoint extends PathConstraint
	native(AI);

cpptext
{
	// Interface
	virtual UBOOL EvaluatePath( UReachSpec* Spec, APawn* Pawn, INT& out_PathCost, INT& out_HeuristicCost );
}

/** Goal trying to find path toward */
var Vector GoalPoint;

static function bool TowardPoint( Pawn P, Vector Point )
{
	local Path_TowardPoint Con;

	if( P != None && Point != Vect(0,0,0) )
	{
		Con = Path_TowardPoint(P.CreatePathConstraint(default.class));
		if( Con != None )
		{
			Con.GoalPoint = Point;
			P.AddPathConstraint( Con );
			return TRUE;
		}
	}

	return FALSE;
}

function Recycle()
{
	Super.Recycle();
	GoalPoint=default.GoalPoint;
}

defaultproperties
{
	Cacheidx=2
}
