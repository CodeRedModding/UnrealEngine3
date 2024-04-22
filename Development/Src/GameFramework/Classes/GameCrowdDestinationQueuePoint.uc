/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Where crowd agent is going.  Destinations can kill agents that reach them or route them to another destination
 * 
 */
class GameCrowdDestinationQueuePoint extends GameCrowdInteractionPoint
	native;

/** Position behind this one in line */
var() GameCrowdDestinationQueuePoint NextQueuePosition;

/** Position before this one in line */
var GameCrowdInteractionPoint PreviousQueuePosition;

/** Agent currently occupying this queue position */
var GameCrowdAgent QueuedAgent;

/** Which destination this queue is part of the line for.  Used only for queue validation during error checking. */
var transient GameCrowdDestination QueueDestination;

/** Prevent ClearQueue() reentry */
var bool bClearingQueue;

/** Average pause time before agent reacts to queue movement */
var() float AverageReactionTime;

/** True if queued agent is still at this position, but about to advance */
var bool bPendingAdvance;

/** Queue behavior used by this queue point */
var class<GameCrowdBehavior_WaitInQueue> QueueBehaviorClass;

/** 
  *  Returns true if agent at TestPosition is considered to have reached this queue point
  */
native function bool QueueReachedBy(GameCrowdAgent Agent, vector TestPosition);

/**
  * Returns true if this queue has space
  */
simulated function bool HasSpace()
{
	// If I don't have a queued agent, and there's not one about to advance to me, then I have space
	if ( (QueuedAgent == None) && ((NextQueuePosition == None) || !NextQueuePosition.bPendingAdvance || (NextQueuePosition.QueuedAgent == None)) )
	{
		return true;
	}
	if ( NextQueuePosition == None )
	{
		return false;
	}
	return NextQueuePosition.HasSpace();
}

/** 
  * Called after Agent reaches this queue position
  * 
  * @PARAM Agent is the crowd agent that just reached this queue position
  */
simulated event ReachedDestination(GameCrowdAgent Agent)
{
 	    local GameCrowdDestinationQueuePoint QueuePoint;
	
		// if agent in front of me hasn't reached yet and is further than me from the front, switch positions with him
		// FIXMESTEVE - doesn't address the case where the GameCrowdDestination itself appears empty - may not need to be handled
		for ( QueuePoint = Agent.CurrentDestination.QueueHead; QueuePoint!=None; QueuePoint= QueuePoint.NextQueuePosition)
		{
			if ( QueuePoint.NextQueuePosition == self )
			{
				if ( QueuePoint.QueuedAgent == None )
				{
					`warn(agent$"in queue behind empty spot at "$self);
				}
				else if ( !QueuePoint.QueueReachedBy(QueuePoint.QueuedAgent, QueuePoint.QueuedAgent.Location) && (VSizeSq(QueuePoint.Location - Agent.Location) < VSizeSq(QueuePoint.Location - QueuePoint.QueuedAgent.Location)) )
				{
					// switch places for agents
					QueuedAgent = QueuePoint.QueuedAgent;
					QueuePoint.QueuedAgent = Agent;
					GameCrowdBehavior_WaitInQueue(QueuedAgent.CurrentBehavior).QueuePosition = self;
					GameCrowdBehavior_WaitInQueue(QueuePoint.QueuedAgent.CurrentBehavior).QueuePosition = QueuePoint;
					return;
				}
			}
		}
	// note - idle time will increase until next position opens up
	GameCrowdBehavior_WaitInQueue(QueuedAgent.CurrentBehavior).bIdleBehavior = true;
	QueuedAgent.PlayIdleAnimation();
}

/** 
  *  Advance customer to next position in line, with a reaction time delay
  */
simulated function AdvanceCustomerTo(GameCrowdInteractionPoint FrontPosition)
{
	PreviousQueuePosition = FrontPosition;
	bPendingAdvance = true;
	SetTimer(AverageReactionTime, false, 'ActuallyAdvance');
}

/** 
  * Actually advance the customer now
  */
private simulated function ActuallyAdvance()
{
	local GameCrowdDestinationQueuePoint FrontQueuePosition;
	local GameCrowdDestination QueueFront;
	local GameCrowdAgent TempAgent;
	
	bPendingAdvance = false;
	if ( QueuedAgent != None )
	{
		TempAgent = QueuedAgent;
		
		// FIXMESTEVE - creates queue behavior for every spot in line - should re-use
		bClearingQueue = true;
		QueuedAgent.StopBehavior();
		bClearingQueue = false;
		QueuedAgent = None;
		FrontQueuePosition = GameCrowdDestinationQueuePoint(PreviousQueuePosition);
		if ( FrontQueuePosition != None )
		{
			FrontQueuePosition.AddCustomer(TempAgent, None);
		}
		else
		{
			QueueFront = GameCrowdDestination(PreviousQueuePosition);
			if ( QueueFront == None )
			{
				`warn("Illegal front position for queue "$self);
				return;
			}
			QueueFront.IncrementCustomerCount(TempAgent);
		}
		if ( QueuedAgent != None )
		{
			`warn(self$" GOT QUEUED AGENT BACK - Head "$PreviousQueuePosition$" Tail "$NextQueuePosition);
		}
		else if ( NextQueuePosition != None )
		{
			NextQueuePosition.AdvanceCustomerTo(self);
		}
	}
}

/**
  * Add customer to queue
  */
simulated function AddCustomer(GameCrowdAgent NewCustomer, GameCrowdInteractionPoint PreviousPosition)
{
	if ( PreviousPosition != None )
	{
		PreviousQueuePosition = PreviousPosition;
	}
	if ( QueuedAgent == None )
	{
		QueuedAgent = NewCustomer;
		NewCustomer.ActivateInstancedBehavior(new(NewCustomer) QueueBehaviorClass);
		GameCrowdBehavior_WaitInQueue(NewCustomer.CurrentBehavior).QueuePosition = self;
		GameCrowdBehavior_WaitInQueue(NewCustomer.CurrentBehavior).ActionTarget = PreviousQueuePosition;
	}
	else if ( NextQueuePosition != None )
	{
		NextQueuePosition.AddCustomer(NewCustomer, self);
	}
	else
	{
		`warn(self$" Attempted to add customer "$NewCustomer$" beyond end of queue");
	}
}

/**
  *  Clear OldCustomer from this queue position
  *  Advance any customers in line
  */
simulated function ClearQueue(GameCrowdAgent OldCustomer)
{
	if ( !bClearingQueue )
	{
		bClearingQueue = true;
		if ( OldCustomer == QueuedAgent )
		{
			QueuedAgent.StopBehavior();
			QueuedAgent = None;
			if ( NextQueuePosition != None )
			{
				NextQueuePosition.AdvanceCustomerTo(self);
			}
		}
		else
		{
			`warn("Attempted to clear "$OldCustomer$" from queue position with customer "$QueuedAgent);
		}
		bClearingQueue = false;
	}
}

simulated function bool HasCustomer()
{
	return (QueuedAgent != None);
}

defaultproperties
{
	Begin Object NAME=CollisionCylinder
		CollisionRadius=+0100.000000
		CollisionHeight=+0040.000000
	End Object
	
	AverageReactionTime=0.7
	
	QueueBehaviorClass=class'GameCrowdBehavior_WaitInQueue')
}
