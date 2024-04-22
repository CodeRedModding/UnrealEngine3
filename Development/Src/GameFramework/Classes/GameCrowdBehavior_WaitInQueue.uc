/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameCrowdBehavior_WaitInQueue extends GameCrowdAgentBehavior
	native
	notplaceable
	dependsOn(GameCrowdAgent);

/** Keep from re-entering StopBehavior during queue clean up */
var bool bStoppingBehavior;
 
 /** Current Queue position (associated with CurrentDestination */
 var GameCrowdDestinationQueuePoint QueuePosition;

/**
  * Handles movement destination updating for agent.
  *
  * @RETURNS true if destination updating was handled
  */ 
native function bool HandleMovement();

/**
  * Notification that MyAgent is changing destinations
  */
function ChangingDestination(GameCrowdDestination NewDest)
{
	if ( QueuePosition == None )
	{
	 `warn(MyAgent$" should never have no QueuePosition");
	}
	MyAgent.StopBehavior();
}

function Actor GetDestinationActor()
{
	return QueuePosition;
}

function string GetBehaviorString()
{
	if ( QueuePosition != None )
	{
		return self$" Waiting in line at "$QueuePosition;
	}
	else
	{
		return self$" Queue Behavior with NO QUEUEPOSITION!";
	}
}

native function bool ShouldEndIdle();

function StopBehavior()
{
	if ( !bStoppingBehavior )
	{
		bStoppingBehavior = true;
		super.StopBehavior();
		if ( QueuePosition != None )
		{
			QueuePosition.ClearQueue(MyAgent);
		}
		QueuePosition = None;
		MyAgent.StopIdleAnimation();
		bStoppingBehavior = false;
	}
}

defaultproperties
{
	bIdleBehavior=true
