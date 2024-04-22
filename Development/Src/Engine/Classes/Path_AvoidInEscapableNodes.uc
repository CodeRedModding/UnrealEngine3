/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
* - this constraint will throw out nodes which the pathing bot could get to, but not get away from.  Normally this
*   is not necessary as you're usually pathing somewhere so a node which you can't get out of is thrown out by A*, but if 
*   you're doing some sort of generic search (ie. for a node in range of something) you could end up with a node the bot can't
*   escape from, thus causing him to be forever stuck.. this constraint will prevent that from happening
*/
class Path_AvoidInEscapableNodes extends PathConstraint
	native(AI);

var int Radius;
var int Height;
var int MaxFallSpeed;
var int moveFlags;

cpptext
{
	// Interface
	virtual UBOOL EvaluatePath( UReachSpec* Spec, APawn* Pawn, INT& out_PathCost, INT& out_HeuristicCost );
}

private native function CachePawnReacFlags(Pawn P);

static function bool DontGetStuck( Pawn P )
{
	local Path_AvoidInEscapableNodes Con;

	if( P != None)
	{
		Con = Path_AvoidInEscapableNodes(P.CreatePathConstraint(default.class));
		if( Con != None )
		{
			Con.CachePawnReacFlags(P);
			P.AddPathConstraint( Con );
			return TRUE;
		}
	}

	return FALSE;
}

function Recycle()
{
	Super.Recycle();
	Radius=0;
	Height=0;
	MaxFallSpeed=0;
	moveFlags=0;	
}

defaultproperties
{
	Cacheidx=11
}
