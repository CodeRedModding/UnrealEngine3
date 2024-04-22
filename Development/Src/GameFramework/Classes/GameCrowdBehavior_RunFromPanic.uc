/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameCrowdBehavior_RunFromPanic extends GameCrowdAgentBehavior
	native
	placeable
	dependsOn(GameCrowdAgent);
	
/** Actor who caused panic - if set, flee this actor */
var transient Actor PanicFocus;

function ActivatedBy(Actor NewActionTarget)
{
	local GameCrowdDestination TempDest, PrevDest;

	// don't pass action target to super - we don't want to look at it
	PanicFocus = NewActionTarget; 
	PrevDest = MyAgent.PreviousDestination;

	// see if already heading away from danger
	if( MyAgent.CurrentDestination != None && AllowThisDestination(MyAgent.CurrentDestination) )
	{
		return;
	}
	else if( PrevDest != None && PrevDest.AllowableDestinationFor(MyAgent) )
	{
		// try heading back where agent was coming from
		TempDest = MyAgent.CurrentDestination;
		MyAgent.CurrentDestination.DecrementCustomerCount(MyAgent);
		MyAgent.SetCurrentDestination(MyAgent.PreviousDestination);
		MyAgent.PreviousDestination = TempDest;
		MyAgent.UpdateIntermediatePoint();
	}
}
	
function InitBehavior(GameCrowdAgent Agent)
{
	super.InitBehavior(Agent);

	MyAgent.bIsPanicked = TRUE;
	MyAgent.SetMaxSpeed();
}

function StopBehavior()
{
	Super.StopBehavior();
	MyAgent.bIsPanicked = FALSE;
	MyAgent.SetMaxSpeed();
}

function Actor GetBehaviorInstigator()
{
	return PanicFocus;
}

/**
  * Return true if agent is allowed to go to destination while panicked
  */
function bool AllowThisDestination(GameCrowdDestination Destination)
{
	return !Destination.bAvoidWhenPanicked && !Destination.AtCapacity() && (Destination.bFleeDestination || (PanicFocus == None) || (((Destination.Location - MyAgent.Location) dot (MyAgent.Location - PanicFocus.Location)) > 0.0));
}

/** 
  * return true if get kismet or new behavior from this destination
  */
function bool AllowBehaviorAt(GameCrowdDestination Destination)
{
	return !Destination.bSkipBehaviorIfPanicked && !Destination.bAvoidWhenPanicked;
}

function string GetBehaviorString()
{
	return "Run from PANIC "@PanicFocus;
}

defaultproperties
{
	bIsViralBehavior=TRUE
	MaxPlayerDistance=20000.0

	MyEventType=CBE_Panic
	ViralBehaviorEvent=CBE_Alert

	DebugBehaviorColor=(R=255,G=0,B=0)

}
