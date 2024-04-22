/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


// represents the path of a teleporter
class TeleportReachSpec extends ReachSpec
	native;

cpptext
{
	virtual INT CostFor(APawn* P);
}

defaultproperties
{
	Distance=100.0
	bAddToNavigationOctree=false
	bCheckForObstructions=false
}
