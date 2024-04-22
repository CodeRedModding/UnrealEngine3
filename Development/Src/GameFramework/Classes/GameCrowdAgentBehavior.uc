/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameCrowdAgentBehavior extends Object
	native
	abstract;

enum ECrowdBehaviorEvent
{
	CBE_None,
	CBE_Spawn,
	CBE_Random,
	CBE_SeePlayer,
	CBE_EncounterAgent,
	CBE_TakeDamage,
	CBE_GroupWaiting,
	CBE_Uneasy,
	CBE_Alert,
	CBE_Panic,
};
var ECrowdBehaviorEvent MyEventType;

/** If non-zero, how long before behavior auto terminates */
var() float DurationOfBehavior;
var   float TimeUntilStopBehavior;
	
/** If true, agent should idle (not move between destinations)/ */
var() bool bIdleBehavior;

/** actor to aim at during actions */
var	Actor ActionTarget;

/** Agent must be within this distance of the player to perform this behavior */
var() float MaxPlayerDistance;

/** If true, must face action target before starting behavior */
var() bool bFaceActionTargetFirst;

/** If true, pass on to agents encountered */
var() bool bIsViralBehavior;
var() ECrowdBehaviorEvent ViralBehaviorEvent;
var() float ViralRadius;

/** 
 * So for some behaviors we only want the original agents to be able to pass on the bViralBehavior flag.
 * You will want to check for this flag in your specific behavior's event PropagateViralBehaviorTo.  
 *
 * NOTE: Currently, there is no default implementation of that that we are are all calling super. to utilize that functionality
 **/
var() bool bPassOnIsViralBehaviorFlag;

var() float DurationBeforeBecomesViral;
var transient float TimeToBecomeViral;

/** 
 * How long we should propagate the viral behavior.  Basically, you can get into situations where the the behavior will never go away as it
 * keeps getting propagated to others over and over and the various timers get started again.
 */
var() float DurationOfViralBehaviorPropagation;
/** This is the time we will stop propagating the bIsViralBehavior flag **/
var transient float TimeToStopPropagatingViralBehavior;

/** Agent currently implementing this behavior instance */
var GameCrowdAgent MyAgent;

var(Debug) Color DebugBehaviorColor;

/**
 *
 * if duration < 0 it is instant
 * if duration == 0 it is eternal
 * if duration > 0 it has a lifespan
 *
 **/
static native function GameCrowdBehaviorPoint TriggerCrowdBehavior( ECrowdBehaviorEvent EventType, Actor Instigator, Vector AtLocation, float InRange, float InDuration, optional Actor BaseActor, optional bool bRequireLOS );

/**
  *  Called every tick when agent is currently idle (because bIdleBehavior is true)
  *
  * @RETURN true if should end idle (bIdleBehavior should also become false)
  */
native function bool ShouldEndIdle();

/**
  *  Agent's current behavior gets ticked
  */
native event Tick(float DeltaTime);

/** 
  * This function is called on an archetype - do not modify any properties here!
  */
function bool CanBeUsedBy(GameCrowdAgent Agent, vector CameraLoc)
{
	if( Agent.CurrentBehavior != None && Agent.CurrentBehavior.MyEventType == MyEventType )
	{
		return FALSE;
	}

	return VSizeSq(CameraLoc - Agent.Location) < MaxPlayerDistance*MaxPlayerDistance;
}

/** 
  *  Event when agent is facing action target, called if bFaceActionTarget=true
  */
event FinishedTargetRotation();

/**
  * Handles movement destination updating for agent.
  *
  * @RETURNS true if destination updating was handled
  */ 
native function bool HandleMovement();

/**
  * Called when Agent activates this behavior
  */
function InitBehavior(GameCrowdAgent Agent)
{
	MyAgent = Agent;

	if( DurationBeforeBecomesViral > 0.f )
	{
		TimeToBecomeViral = MyAgent.WorldInfo.TimeSeconds + DurationBeforeBecomesViral;
	}
	if( DurationOfViralBehaviorPropagation > 0.0f )
	{
		TimeToStopPropagatingViralBehavior = MyAgent.WorldInfo.TimeSeconds + DurationOfViralBehaviorPropagation;
	}
	if( DurationOfBehavior > 0.0f )
	{
		TimeUntilStopBehavior = DurationOfBehavior;
	}
}

/**
  * Called when Agent stops this behavior
  */
function StopBehavior()
{
}

/**
  *  Anim end notification called by GameCrowdAgent.OnAnimEnd().
  */
event OnAnimEnd(AnimNodeSequence SeqNode, float PlayedTime, float ExcessTime);

/** 
  * Get debug string about agent behavior
  */
function string GetBehaviorString()
{
	return "Behavior: "$self;
}

/**
  * Notification that MyAgent is changing destinations
  */
function ChangingDestination(GameCrowdDestination NewDest);

/**
  * Returns action agent wants behavior to be moving toward.
  */
function Actor GetDestinationActor()
{
	return MyAgent.CurrentDestination;
}

/**
  * Called if agent wants to provide an action target to its behavior.
  */
function ActivatedBy(Actor NewActionTarget)
{
	ActionTarget = NewActionTarget;
}

function Actor GetBehaviorInstigator()
{
	return ActionTarget;
}

/**
  * When two agents encounter each other, and one has a viral behavior and the other doesn't,
  * the viral behavior is called to have a chance to propagate itself to the uninfected OtherAgent.
  */
event PropagateViralBehaviorTo( GameCrowdAgent OtherAgent )
{
	if( ViralBehaviorEvent != CBE_None )
	{
		OtherAgent.HandleBehaviorEvent( ViralBehaviorEvent, GetBehaviorInstigator(), TRUE, bPassOnIsViralBehaviorFlag );
	}
}

/**
  * Return true if agent is allowed to go to destination while performing this behavior
  */
function bool AllowThisDestination(GameCrowdDestination Destination)
{
	return true;
}

/** 
  * return true if get kismet or new behavior from this destination
  */
function bool AllowBehaviorAt(GameCrowdDestination Destination)
{
	return true;
}

defaultproperties
{
	MaxPlayerDistance=10000.0
	bPassOnIsViralBehaviorFlag=TRUE
	ViralRadius=512.f
}
