/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * penalizes specs of a certain class if they are within a set distance of another mantle in the predecessor chain
 */
class Path_MinDistBetweenSpecsOfType extends PathConstraint
	native(AI);

cpptext
{
	// Interface
	virtual UBOOL EvaluatePath( UReachSpec* Spec, APawn* Pawn, INT& out_PathCost, INT& out_HeuristicCost );
	UBOOL IsNodeWithinMinDistOfSpecInPath(ANavigationPoint* Node);
}

/** min dist between specs of the specified type type */
var float MinDistBetweenSpecTypes;

/** can be used to indicate we last mantled at this location in previous path and we shouldn't take mantles within
   mindistbetweenmantles of that location */
var vector InitLocation;

/** 
  * the class of the reach spec we want to enforce minimum distance between
  * @NOTE: this must be the exact class, child classes will not match
*/
var class<ReachSpec> ReachSpecClass;

static function bool EnforceMinDist( Pawn P, float InMinDist, class<ReachSpec> InSpecClass, optional vector LastLocation )
{
	local Path_MinDistBetweenSpecsOfType Con;

	if( P != None && P.bCanMantle && InMinDist > 0.f )
	{
		Con = Path_MinDistBetweenSpecsOfType(P.CreatePathConstraint(default.class));
		if( Con != None )
		{
			Con.MinDistBetweenSpecTypes = InMinDist;
			Con.InitLocation = LastLocation;
			Con.ReachSpecClass = InSpecClass;
			P.AddPathConstraint( Con );
			return TRUE;
		}
	}

	return FALSE;
}

function Recycle()
{
	Super.Recycle();
	MinDistBetweenSpecTypes=default.MinDistBetweenSpecTypes;
	ReachSpecClass = none;
	InitLocation=vect(0,0,0);
}

defaultproperties
{
	Cacheidx=10
}
