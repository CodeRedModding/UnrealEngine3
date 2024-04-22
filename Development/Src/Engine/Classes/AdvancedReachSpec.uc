//=============================================================================
// AdvancedReachSpec.
//
// An AdvancedReachspec can only be used by Controllers with bCanDoSpecial==true
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
class AdvancedReachSpec extends ReachSpec
	native;

cpptext
{
	virtual FPlane PathColor()
	{
		// purple path = advanced
		return FPlane(1.f,0.f,1.f, 0.f);
	}
	virtual INT CostFor(APawn* P);
}

defaultproperties
{
	bCanCutCorners=false
}

