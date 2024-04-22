/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * penalizes specs of a certain class if they are within a set distance of another mantle in the predecessor chain
 */
class NavMeshPath_MinDistBetweenSpecsOfType extends NavMeshPathConstraint
	native(AI);

cpptext
{
	// Interface
	virtual UBOOL EvaluatePath( FNavMeshEdgeBase* Edge, FNavMeshEdgeBase* PredecessorEdge, FNavMeshPolyBase* SrcPoly, FNavMeshPolyBase* DestPoly, const FNavMeshPathParams& PathParams, INT& out_PathCost, INT& out_HeuristicCost, const FVector& EdgePoint );
	UBOOL IsWithinMinDistOfEdgeInPath(FNavMeshEdgeBase* Edge, FNavMeshEdgeBase* PredecessorEdge);
}

/** min dist between edges of the specified type type */
var float MinDistBetweenEdgeTypes;

/** can be used to indicate we last mantled at this location in previous path and we shouldn't take mantles within
   mindistbetweenmantles of that location */
var vector InitLocation;

/** 
  * the type of edge we want to enforce minimum distance between
*/
var ENavMeshEdgeType EdgeType;

/** penalty to apply when two specs are within mindist */
var float Penalty;

static function bool EnforceMinDist( NavigationHandle NavHandle, float InMinDist, ENavMeshEdgeType InEdgeType, optional vector LastLocation, optional float InPenalty )
{
	local NavMeshPath_MinDistBetweenSpecsOfType Con;

	if( NavHandle != None /*&& NavHandle.bCanMantle */ && InMinDist > 0.f )
	{
		Con = NavMeshPath_MinDistBetweenSpecsOfType(NavHandle.CreatePathConstraint(default.class));
		if( Con != None )
		{
			Con.MinDistBetweenEdgeTypes = InMinDist;
			Con.InitLocation = LastLocation;
			Con.EdgeType = InEdgeType;
			Con.Penalty = InPenalty;
			NavHandle.AddPathConstraint( Con );
			return TRUE;
		}
	}

	return FALSE;
}

function Recycle()
{
	Super.Recycle();
	MinDistBetweenEdgeTypes=default.MinDistBetweenEdgeTypes;
	EdgeType = NAVEDGE_Normal;
	InitLocation=vect(0,0,0);
	Penalty=default.Penalty;
}

defaultproperties
{
	Penalty=10000
}
