/*=============================================================================
// Ladders are associated with the LadderVolume that encompasses them, and provide AI navigation
// support for ladder volumes.  Direction should be the direction that climbing pawns
// should face
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
============================================================================= */

class Ladder extends NavigationPoint
	placeable
	native;

cpptext
{
#if WITH_EDITOR
	virtual UBOOL CanConnectTo(ANavigationPoint* Dest, UBOOL bCheckDistance);
	void addReachSpecs(AScout *Scout, UBOOL bOnlyChanged=0);
#endif
	void InitForPathFinding();
	void ClearPaths();
	virtual UBOOL ReachedBy(APawn* P, const FVector& TestPosition, const FVector& Dest);
}

var LadderVolume MyLadder;
var Ladder LadderList;

/*
Check if ladder is already occupied
*/
event bool SuggestMovePreparation(Pawn Other)
{
	if ( MyLadder == None )
		return false;

	if ( !MyLadder.InUse(Other) )
	{
		MyLadder.PendingClimber = Other;
		return false;
	}

	Other.Controller.bPreparingMove = true;
	Other.Acceleration = vect(0,0,0);
	return true;
}

defaultproperties
{
	Begin Object NAME=CollisionCylinder
		CollisionRadius=+00040.000000
		CollisionHeight=+00080.000000
	End Object

	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorResources.S_Ladder'
	End Object

	bSpecialMove=true
	bNotBased=true
}
