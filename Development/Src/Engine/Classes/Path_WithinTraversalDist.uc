/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/
class Path_WithinTraversalDist extends PathConstraint
	native(AI);

cpptext
{
	// Interface
	virtual UBOOL EvaluatePath( UReachSpec* Spec, APawn* Pawn, INT& out_PathCost, INT& out_HeuristicCost );
}

/** Maximum distance to traverse along a branch */
var() float	MaxTraversalDist;

/** if this is on instead of throwing out nodes outside traversal distance they will be gradiently penalized the further out they are */
var() bool bSoft;
/** when a path exceeds specified traversal distance this penantly will be applied, and scaled up depending on how far outside the dist it is */
var() float SoftStartPenalty;

static function bool DontExceedMaxDist( Pawn P, float InMaxTraversalDist, bool bInSoft=true )
{
	local Path_WithinTraversalDist Con;

	if( P != None && InMaxTraversalDist > 0.f )
	{
		Con = Path_WithinTraversalDist(P.CreatePathConstraint(default.class));
		if( Con != None )
		{
			Con.MaxTraversalDist = InMaxTraversalDist;
			Con.bSoft = bInSoft;
			P.AddPathConstraint( Con );
			return TRUE;
		}
	}

	return FALSE;
}

function Recycle()
{
	Super.Recycle();
	MaxTraversalDist=default.MaxTraversalDist;
	bSoft=default.bSoft;
	SoftStartPenalty=default.SoftStartPenalty;
}

defaultproperties
{
	bSoft=true
		SoftStartPenalty=320.0f
		CacheIdx=4
}
