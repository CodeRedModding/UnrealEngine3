/**
 * a path that opens and closes via some trigger, usually Kismet controlled
 * this differs from triggering a normal NavigationPoint in that the AI
 * considers these to always be traversible, but may need to do something before using them,
 * whereas normal NavigationPoints are considered off limits when Kismet toggles them off
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class TriggeredPath extends NavigationPoint
	placeable;

/** whether the path is currently usable */
var() bool bOpen;

/** the trigger, button, etc that will make this path usable */
var() Actor MyTrigger;

function OnToggle(SeqAct_Toggle InAction)
{
	if (InAction.InputLinks[0].bHasImpulse)
	{
		bOpen = true;
	}
	else if (InAction.InputLinks[1].bHasImpulse)
	{
		bOpen = false;
	}
	else if (InAction.InputLinks[2].bHasImpulse)
	{
		bOpen = !bOpen;
	}

	WorldInfo.Game.NotifyNavigationChanged(self);
}

event Actor SpecialHandling(Pawn Other)
{
	local Actor TouchActor;

	if (bOpen || MyTrigger == None)
	{
		return self;
	}
	else
	{
		TouchActor = MyTrigger.SpecialHandling(Other);
		if (TouchActor == None)
		{
			TouchActor = MyTrigger;
		}
		return TouchActor;
	}
}

event bool SuggestMovePreparation(Pawn Other)
{
	if (bOpen)
	{
		return false;
	}
	else if (MyTrigger != None && Other.Controller.ActorReachable(MyTrigger))
	{
		// go to trigger instead
		if (Other.Controller.Focus == Other.Controller.MoveTarget)
		{
			Other.Controller.Focus = MyTrigger;
		}
		Other.Controller.MoveTarget = MyTrigger;
		Other.Controller.CurrentPath = None;
		Other.Controller.NextRoutePath = None;
		return false;
	}
	else
	{
		// wait for path to open
		Other.Controller.MoveTimer = 1.0;
		Other.Controller.bPreparingMove = true;
		Other.Velocity = vect(0,0,0);
		Other.Acceleration = vect(0,0,0);
		return true;
	}
}

defaultproperties
{
	RemoteRole=ROLE_None
	bNoDelete=true
	ExtraCost=100
	bSpecialMove=true
}
