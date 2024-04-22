/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameCrowdBehavior_WaitForGroup extends GameCrowdAgentBehavior
	native
	placeable
	dependsOn(GameCrowdAgent);

function InitBehavior(GameCrowdAgent Agent)
{
	Super.InitBehavior(Agent);
	Agent.PlayIdleAnimation();
}

/** 
  * Get debug string about agent behavior
  */
function string GetBehaviorString()
{
	local string BehaviorString;
	
	BehaviorString = "Behavior: "$self;

	if ( bFaceActionTargetFirst )
	{
		BehaviorString = BehaviorString@"Turning toward "$ActionTarget;
	}
	else
	{
		BehaviorString = BehaviorString@"Waiting For Group";
	}
	
	return BehaviorString;
}

/**
  *  Called every tick when agent is currently idle (because bIdleBehavior is true)
  *
  * @RETURN true if should end idle (bIdleBehavior should also become false)
  */
native function bool ShouldEndIdle();

function StopBehavior()
{
	super.StopBehavior();
	MyAgent.StopIdleAnimation();
}

defaultproperties
{
	bIdleBehavior=true
	bFaceActionTargetFirst=true
}
