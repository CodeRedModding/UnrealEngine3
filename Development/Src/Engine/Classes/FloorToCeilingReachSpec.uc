/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class FloorToCeilingReachSpec extends ForcedReachSpec
	native;

cpptext
{
	virtual INT CostFor(APawn* P);
	virtual INT AdjustedCostFor( APawn* P, const FVector& StartToGoalDir, ANavigationPoint* Goal, INT Cost );
}

defaultproperties
{
	bSkipPrune=TRUE
}
