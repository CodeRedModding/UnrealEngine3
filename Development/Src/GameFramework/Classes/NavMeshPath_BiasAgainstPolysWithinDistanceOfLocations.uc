/**
 * This constraint is to make path searches be biased against choosing in the same set polys over and over.
 * For Example: if we spawn in place a b c  we do not want to spawn there again if there is a place d that matches
 * our criteria for spawning of guys.   We often use this for determining places that No One Has Spawned Near Here Before.
 *
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class NavMeshPath_BiasAgainstPolysWithinDistanceOfLocations extends NavMeshPathConstraint
	native;

/** Location to compare from **/
var transient vector Location;

var transient vector Rotation;

/** How far we want to spawn away from a previous spawn. **/
var transient float DistanceToCheck;

/** Set of places we have spawned before **/
var transient array<vector> LocationsToCheck;


cpptext
{
	// Interface
	virtual UBOOL EvaluatePath( FNavMeshEdgeBase* Edge, FNavMeshEdgeBase* PredecessorEdge, FNavMeshPolyBase* SrcPoly, FNavMeshPolyBase* DestPoly, const FNavMeshPathParams& PathParams, INT& out_PathCost, INT& out_HeuristicCost, const FVector& EdgePoint );
}


static function bool BiasAgainstPolysWithinDistanceOfLocations ( NavigationHandle NavHandle, const vector InLocation, const rotator InRotation, const float InDistanceToCheck, const array<vector> InLocationsToCheck )
{
	local NavMeshPath_BiasAgainstPolysWithinDistanceOfLocations Con;

	if( NavHandle != None )
	{
		Con = NavMeshPath_BiasAgainstPolysWithinDistanceOfLocations(NavHandle.CreatePathConstraint(default.class));
		if( Con != None )
		{
			Con.Location = InLocation;
			Con.Rotation = vector(InRotation);
			Con.DistanceToCheck = InDistanceToCheck;
			Con.LocationsToCheck = InLocationsToCheck;

			NavHandle.AddPathConstraint( Con );

			return TRUE;
		}
	}

	return FALSE;
}

function Recycle()
{
	Super.Recycle();
}

defaultproperties
{
}
