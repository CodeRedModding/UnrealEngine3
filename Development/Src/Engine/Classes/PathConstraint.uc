/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/
class PathConstraint extends Object
	native(AI);

// index into the constraint cache for this class' pool
var const int CacheIdx;

cpptext
{
	// Interface
	virtual UBOOL EvaluatePath( UReachSpec* Spec, APawn* Pawn, INT& out_PathCost, INT& out_HeuristicCost );
}

/** Next constraint in the list */
var PathConstraint NextConstraint;

// called when this object is about to be re-used from the cache
event Recycle()
{
	NextConstraint = None;
}

event String GetDumpString()
{
	return String(self);
}

defaultproperties
{
	// need to set this in subclasses
	CacheIdx=-1
}
