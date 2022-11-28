/*=============================================================================
// Ladders are associated with the LadderVolume that encompasses them, and provide AI navigation 
// support for ladder volumes.  Direction should be the direction that climbing pawns
// should face
============================================================================= */

class Ladder extends NavigationPoint
	placeable
	native;

cpptext
{
	INT ProscribedPathTo(ANavigationPoint *Dest);
	void addReachSpecs(AScout *Scout, UBOOL bOnlyChanged=0);
	void InitForPathFinding();
	void ClearPaths();
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
		Sprite=Texture2D'EngineResources.S_Ladder'
	End Object

	Begin Object Class=ArrowComponent Name=Arrow
		ArrowColor=(R=150,G=200,B=255)
	End Object
	Components.Add(Arrow)

	bSpecialMove=true
	bNotBased=true
}
