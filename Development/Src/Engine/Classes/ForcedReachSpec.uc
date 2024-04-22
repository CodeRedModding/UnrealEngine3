//=============================================================================
// ForcedReachSpec.
//
// A ForcedReachspec is forced by the level designer
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class ForcedReachSpec extends ReachSpec
	native;

cpptext
{
	virtual FPlane PathColor()
	{
		// yellow for forced paths
		return FPlane(1.f, 1.f, 0.f, 0.f);
	}

	virtual UBOOL IsForced() { return true; }
	virtual UBOOL PrepareForMove( AController * C );
	virtual INT CostFor(APawn* P);
}

defaultproperties
{
}

