//=============================================================================
// LiftExit.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class LiftExit extends NavigationPoint
	placeable
	native;

var()	LiftCenter				MyLiftCenter;
var()	bool					bExitOnly;			// if true, can only get off lift here.

cpptext
{
	virtual void ReviewPath(APawn* Scout);
}

function bool CanBeReachedFromLiftBy(Pawn Other)
{
	return ( (Location.Z < Other.Location.Z + Other.GetCollisionHeight())
			 && Other.LineOfSightTo(self) );
}

function WaitForLift(Pawn Other)
{
	if (MyLiftCenter != None)
	{
		Other.SetDesiredRotation(rotator(Location - Other.Location));
		Other.Controller.WaitForMover(MyLiftCenter.MyLift);
	}
}

event bool SuggestMovePreparation(Pawn Other)
{
	local Controller C;

	if ( (MyLiftCenter == None) || (Other.Controller == None) )
		return false;
	if ( Other.Physics == PHYS_Flying )
	{
		if ( Other.AirSpeed > 0 )
			Other.Controller.MoveTimer = 2+ VSize(Location - Other.Location)/Other.AirSpeed;
		return false;
	}
	if ( (Other.Base == MyLiftCenter.Base) || Other.ReachedDestination(MyLiftCenter) )
	{
		// if pawn is on the lift, see if it can get off and go to this lift exit
		if ( CanBeReachedFromLiftBy(Other) )
		{
			return false;
		}

		// make pawn wait on the lift
		WaitForLift(Other);
		return true;
	}
	else if (MyLiftCenter != None)
	{
		foreach WorldInfo.AllControllers(class'Controller', C)
		{
			if ( (C.Pawn != None) && (C.PendingMover == MyLiftCenter.MyLift) && WorldInfo.GRI.OnSameTeam(C,Other.Controller) && C.Pawn.ReachedDestination(self) )
			{
				WaitForLift(Other);
				return true;
			}
		}
		Other.Controller.ReadyForLift();
	}
	return false;
}

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorResources.Lift_Exit'
	End Object

	bSpecialMove=true
	bNeverUseStrafing=true
	bForceNoStrafing=true
}
