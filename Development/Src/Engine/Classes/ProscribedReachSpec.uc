//=============================================================================
// ProscribedReachSpec.
//
// A ProscribedReachSpec is forced off by the level designer
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class ProscribedReachSpec extends ReachSpec
	native;

cpptext
{
	virtual FPlane PathColor()
	{
		// red is reserved for proscribed paths
		return FPlane(1.f, 0.f, 0.f, 0.f);
	}

	virtual UBOOL IsProscribed() const { return TRUE; }
	virtual INT CostFor(APawn* P);
}

defaultproperties
{
	bAddToNavigationOctree=false
	bCanCutCorners=false
}
