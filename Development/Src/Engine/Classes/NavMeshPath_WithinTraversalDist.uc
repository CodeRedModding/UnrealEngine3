/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/
class NavMeshPath_WithinTraversalDist extends NavMeshPathConstraint
	native(AI);

cpptext
{
	// Interface
	virtual UBOOL EvaluatePath( FNavMeshEdgeBase* Edge, FNavMeshEdgeBase* PredecessorEdge, FNavMeshPolyBase* SrcPoly, FNavMeshPolyBase* DestPoly, const FNavMeshPathParams& PathParams, INT& out_PathCost, INT& out_HeuristicCost, const FVector& EdgePoint );
}

/** Maximum distance to traverse along a branch */
var() float	MaxTraversalDist;

/** if this is on instead of throwing out nodes outside traversal distance they will be gradiently penalized the further out they are */
var() bool bSoft;
/** when a path exceeds specified traversal distance this penantly will be applied, and scaled up depending on how far outside the dist it is */
var() float SoftStartPenalty;

static function bool DontExceedMaxDist( NavigationHandle NavHandle, float InMaxTraversalDist, bool bInSoft=true )
{
	local NavMeshPath_WithinTraversalDist Con;

	if( NavHandle != None && InMaxTraversalDist > 0.f )
	{
		Con = NavMeshPath_WithinTraversalDist(NavHandle.CreatePathConstraint(default.class));
		if( Con != None )
		{
			Con.MaxTraversalDist = InMaxTraversalDist;
			Con.bSoft = bInSoft;
			NavHandle.AddPathConstraint( Con );
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
}
