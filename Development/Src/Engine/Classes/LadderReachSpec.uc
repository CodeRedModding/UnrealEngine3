//=============================================================================
// LadderReachSpec.
//
// A LadderReachSpec connects Ladder NavigationPoints in a LadderVolume
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class LadderReachSpec extends ReachSpec
	native;

cpptext
{
	virtual FPlane PathColor()
	{
		// light purple = ladder
		return FPlane(1.f,0.5f, 1.f,0.f);
	}
	virtual INT CostFor(APawn* P);
}

defaultproperties
{
	bCanCutCorners=false
}

