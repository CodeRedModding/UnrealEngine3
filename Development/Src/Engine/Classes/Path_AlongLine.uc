/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/
class Path_AlongLine extends PathConstraint
	native(AI);

cpptext
{
	// Interface
	virtual UBOOL EvaluatePath( UReachSpec* Spec, APawn* Pawn, INT& out_PathCost, INT& out_HeuristicCost );
}

/** Direction to move in */
var Vector	Direction;

static function bool AlongLine( Pawn P, Vector Dir )
{
	local Path_AlongLine Con;

	if( P != None && !IsZero( Dir ) )
	{
		Con = Path_AlongLine(P.CreatePathConstraint(default.class));
		if( Con != None )
		{
			Con.Direction = Dir;
			P.AddPathConstraint( Con );
			return TRUE;
		}
	}

	return FALSE;
}

function Recycle()
{
	Super.Recycle();
	Direction=vect(0,0,0);
}

defaultproperties
{
	CacheIdx=0
}
