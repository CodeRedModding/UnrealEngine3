//=============================================================================
// PathNode_Dynamic.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//
// Avoid warnings for pathnodes that are based on and should move with an interpactor platform
// When platform moves, this will cause path costs for ReachSpecs be invalid.
// Update in game specific way... Epic usually handles by forcing paths between static/moving networks
// and using bBlocked to allow movement across the networks at the correct time.
//=============================================================================
class PathNode_Dynamic extends PathNode;

simulated event string GetDebugAbbrev()
{
	return "DynPN";
}

defaultproperties
{
	bStatic=FALSE
}