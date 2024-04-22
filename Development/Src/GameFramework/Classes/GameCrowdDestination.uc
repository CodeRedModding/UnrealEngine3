/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Where crowd agent is going.  Destinations can kill agents that reach them or route them to another destination
 * 
 */
class GameCrowdDestination extends GameCrowdInteractionPoint
	implements(GameCrowdSpawnInterface)
	implements(EditorLinkSelectionInterface)
	dependsOn(GameCrowdAgent)
	native;

/** If TRUE, kill crowd members when they reach this destination. */
var()	bool			bKillWhenReached;

// randomly pick from this list of active destinations
var() duplicatetransient array<GameCrowdDestination> NextDestinations;

/** queue point to use if this destination is at capacity */
var() duplicatetransient GameCrowdDestinationQueuePoint QueueHead;

// whether agents previous destination can be used as a destination if in list of NextDestinations
var() bool bAllowAsPreviousDestination;

/** How many agents can simultaneously have this as a destination */
var() int Capacity;

/** Adjusts the likelihood of agents to select this destination from list at previous destination*/
var() float Frequency;

/** Current number of agents using this destination */
var private int CustomerCount;

/** if set, only agents of this class can use this destination */
var(Restrictions) array<class<GameCrowdAgent> >  SupportedAgentClasses;

/** if set, agents from this archetype can use this destination */
var(Restrictions) array<object>  SupportedArchetypes;

/** if set, agents of this class cannot use this destination */
var(Restrictions) array<class<GameCrowdAgent> >  RestrictedAgentClasses;

/** if set, agents from this archetype cannot use this destination */
var(Restrictions) array<object>  RestrictedArchetypes;

/** Don't go to this destination if panicked */
var() bool bAvoidWhenPanicked;

/** Don't perform kismet or custom behavior at this destination if panicked */
var() bool bSkipBehaviorIfPanicked;

/** Always run toward this destination */
var() bool bFleeDestination;

/** Must reach this destination exactly - will force movement when close */
var() bool bMustReachExactly;

var float ExactReachTolerance;

/** True if has supported class/archetype restrictions */
var bool bHasRestrictions;

/** Type of interaction */
var()	Name			InteractionTag;

/** Time before an agent is allowed to attempt this sort of interaction again */
var()	float			InteractionDelay;

/** True if spawning permitted at this node */
var(Spawning)	bool	bAllowsSpawning;
var(Spawning)   bool    bAllowCloudSpawning;
var(Spawning)   bool    bAllowVisibleSpawning;

/** Spawn in a line rather than in a circle. */
var(Spawning)	bool	bLineSpawner;

/** Whether to spawn agents only at the edge of the circle, or at any point within the circle. */
var(Spawning)	bool	bSpawnAtEdge;

/** Agents reaching this destination will pick a behavior from this list */
var() array<BehaviorEntry>  ReachedBehaviors;

/** Whether agent should stop on reach edge of destination radius (if not reach exactly), or have a "soft" perimeter */
var() bool bSoftPerimeter;

/** Agent currently coming to this destination.  Not guaranteed to be correct/exhaustive.  Used to allow agents to trade places with nearer agent for destinations with queueing */
var GameCrowdAgent AgentEnRoute;

//=========================================================
/** The following properties are set and used by the GameCrowdPopulationManager class for selecting at which destinations to spawn agents */

/** True if currently in line of sight of a player (may not be within view frustrum) */
var bool bIsVisible;

/** True if will become visible shortly based on player's current velocity */ 
var bool bWillBeVisible;

/** This destination is currently available for spawning */
var bool bCanSpawnHereNow;

/** This destination is beyond the maximum spawn distance */
var bool bIsBeyondSpawnDistance;

/** Cache that node is currently adjacent to a visible node */
var bool bAdjacentToVisibleNode;

/** Whether there is a valid NavigationMesh around this destination */
var bool bHasNavigationMesh;

/** Priority for spawning agents at this destination */
var float Priority;

/** Most recent time at which agent was spawned at this destination */
var float LastSpawnTime;

/** Population manager with which this destination is associated */
var transient GameCrowdPopulationManager MyPopMgr;

cpptext
{
	/** EditorLinkSelectionInterface */
	virtual void LinkSelection(USelection* SelectedActors);
	virtual void UnLinkSelection(USelection* SelectedActors);
	
	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
};



/**
  * @PARAM Agent is the agent being checked
    @PARAM Testposition is the position to be tested
    @PARAM bTestExactly if true and GameCrowdDestination.bMustReachExactly is true means ReachedByAgent() only returns true if right on the destination
  * @RETURNS TRUE if Agent has reached this destination
  */
native simulated function bool ReachedByAgent(GameCrowdAgent Agent, vector TestPosition, bool bTestExactly);

simulated function PostBeginPlay()
{
	local int i;
	local GameCrowdPopulationManager PopMgr;

	super.PostBeginPlay();
	
	bHasRestrictions = (SupportedAgentClasses.Length > 0) || (SupportedArchetypes.Length > 0) || (RestrictedAgentClasses.Length > 0) || (RestrictedArchetypes.Length > 0);

	// don't allow automatic agent spawning at destinations with Queues, or small capacities
	if( QueueHead != None || bKillWhenReached )
	{
		bAllowsSpawning = false;
	}

	// verify behavior lists
	for ( i=0; i< ReachedBehaviors.Length; i++ )
	{
		if ( ReachedBehaviors[i].BehaviorArchetype == None )
		{
			`warn(self$" missing BehaviorArchetype at ReachedBehavior "$i);
			ReachedBehaviors.remove(i,1);
			i--;
		}
	}

	// Add self to population manager list
	PopMgr = GameCrowdPopulationManager(WorldInfo.PopulationManager);
	if ( PopMgr != None )
	{
		PopMgr.AddSpawnPoint(self);
	}
}

simulated function Destroyed()
{
	super.Destroyed();

	if ( MyPopMgr != None )
	{
		MyPopMgr.RemoveSpawnPoint(self);
	}
}

	
/** 
  * Called after Agent reaches this destination
  * Will be called every tick as long as ReachedByAgent() is true and agent is not idle, so should change
  * Agent to avoid this (change current destination, make agent idle, or kill agent) )
  * 
  * @PARAM Agent is the crowd agent that just reached this destination
  * @PARAM bIgnoreKismet skips generating Kismet event if true.
  */
simulated event ReachedDestination(GameCrowdAgent Agent)
{
	local int i,j;
	local SeqEvent_CrowdAgentReachedDestination ReachedEvent;
	local bool bEventActivated;

	// check if kismet event on reaching this destination
	for( i = 0; i < GeneratedEvents.Length; i++ )
	{
		ReachedEvent = SeqEvent_CrowdAgentReachedDestination(GeneratedEvents[i]);
		for( j = 0; j < ReachedEvent.OutputLinks[0].Links.Length; j++ )
		{
			// HACKY - clear bActive on output ops so this agent can get in on an already active latent action
			ReachedEvent.OutputLinks[0].Links[j].LinkedOp.bActive = FALSE;
		}
		bEventActivated = ReachedEvent.CheckActivate( self, Agent );
		break;
	}

	// kill agent that reached me?
	if( bKillWhenReached )
	{
		// If desired, kill actor when it reaches an attractor
		DecrementCustomerCount(Agent);
		Agent.CurrentDestination = None;
		Agent.KillAgent();
		return;
	}

	// mark the interaction if tagged
	if( InteractionTag != '' )
	{
		i = Agent.RecentInteractions.Add(1);
		Agent.RecentInteractions[i].InteractionTag = InteractionTag;
		if( InteractionDelay > 0.f )
		{
			// mark the time to remove this interaction from history
			Agent.RecentInteractions[i].InteractionDelay = WorldInfo.TimeSeconds + InteractionDelay;
		}
	}
	
	// Can agent perform a custom behavior here 
	if( Agent.BehaviorDestination != self && (Agent.CurrentBehavior == None || Agent.CurrentBehavior.AllowBehaviorAt(self)) )
	{
		// Assign a reachedbehavior to the agent
		if( ReachedBehaviors.Length > 0 )
		{
			Agent.PickBehaviorFrom(ReachedBehaviors);
		}

		if( ReachedEvent != None )
		{
			Agent.BehaviorDestination = self;
		}
	}
	
	// choose next destination
	if( !bEventActivated && NextDestinations.Length > 0 )
	{
		PickNewDestinationFor( Agent, FALSE );
		
		if( Agent.CurrentDestination == None )
		{
			// if haven't been visible for a while, just kill
			if( Agent.NotVisibleLifeSpan > 0.f && `TimeSince(Agent.LastRenderTime) > Agent.NotVisibleLifeSpan )
			{
				Agent.KillAgent();
			}
			else
			{
				// failed with restrictions, so pick any - FIXMESTEVE probably want more refined fallback
				PickNewDestinationFor( Agent, TRUE );
			}
		}
	}
	
	// first in group to get new destination should update others
	if( Agent.MyGroup != None )
	{
		Agent.MyGroup.UpdateDestinations(Agent.CurrentDestination);
	}
}

/** 
  * Pick a new destination from this one for agent.
  */
simulated function PickNewDestinationFor(GameCrowdAgent Agent, bool bIgnoreRestrictions )
{
	local int i;
	local float DestinationFrequencySum, DestinationPickValue;
	local array<GameCrowdDestination> DestOptions;
	
	// Pick a new destination from available list
	DecrementCustomerCount(Agent);
	Agent.CurrentDestination = None;
	Agent.BehaviorDestination = None;

	// init DestinationFrequencySum
	for( i=0; i< NextDestinations.Length; i++ )
	{
		if ( (NextDestinations[i] != None) && NextDestinations[i].bHasNavigationMesh && (bIgnoreRestrictions || NextDestinations[i].AllowableDestinationFor(Agent)) )
		{
			// bonus to this potential destination's frequency if current destination is not visible, and destination is, and agent prefers visible destinations
			DestinationFrequencySum += NextDestinations[i].Frequency * ((!bIsVisible && Agent.bPreferVisibleDestination && (NextDestinations[i].bIsVisible || NextDestinations[i].bWillBeVisible)) ? 2.0 : 1.0);
			DestOptions.AddItem( NextDestinations[i] );
		}
	}
	
	DestinationPickValue = DestinationFrequencySum * FRand();
	DestinationFrequencySum = 0.0;
	for( i = 0; i < DestOptions.Length; i++ )
	{
		if( DestOptions[i] != None && DestOptions[i].bHasNavigationMesh )
		{
			// bonus to this potential destination's frequency if current destination is not visible, and destination is, and agent prefers visible destinations
			DestinationFrequencySum += DestOptions[i].Frequency * ((!bIsVisible && Agent.bPreferVisibleDestination && (DestOptions[i].bIsVisible || DestOptions[i].bWillBeVisible)) ? 2.0 : 1.0);
			if( DestinationPickValue < DestinationFrequencySum )
			{
				Agent.SetCurrentDestination(DestOptions[i]);
				Agent.PreviousDestination = self;
				Agent.UpdateIntermediatePoint();
				break;
			}
		}
	}

	Agent.PreviousDestination = self;
}

/**
  * Decrement customer count.  Update Queue if have one
  * Be sure to decrement customer count from old destination before setting a new one!
  * FIXMESTEVE - should probably wrap decrement old customercount into GameCrowdAgent.SetDestination()
  */
simulated event DecrementCustomerCount(GameCrowdAgent DepartingAgent)
{
	local GameCrowdDestinationQueuePoint QP;
	local bool bIsInQueue;
	
	// Check to make sure that the current destination is ourself, to prevent double decrementing
	if( DepartingAgent.CurrentDestination == self )
	{
		// check if departing agent is in queue
		for( QP = QueueHead; QP != None; QP = QP.NextQueuePosition )
		{
			if ( QP.QueuedAgent == DepartingAgent )
			{
				bIsInQueue = true;
				QP.ClearQueue(DepartingAgent);
				break;
			}
		}
		
		if( !bIsInQueue )
		{
			// agent was customer, so clear him out
			CustomerCount--;
			if( QueueHead != None && QueueHead.HasCustomer() )
			{
				QueueHead.AdvanceCustomerTo(self);
			}
		}
	}
}

/**
  * Increment customer count, or add agent to queue if needed 
  */
simulated event IncrementCustomerCount(GameCrowdAgent ArrivingAgent)
{
	// if at capacity, or queue is about to move forward, add to queue rather than directly
	if( AtCapacity() || (Queuehead != None && Queuehead.bPendingAdvance) )
	{
		// add to queue
		if( QueueHead != None && QueueHead.HasSpace() )
		{
			// maybe switch with agent currently in route, if ArrivingAgent is closer
			if ( (AgentEnRoute != None) && (AgentEnRoute.CurrentBehavior == None) && !ReachedByAgent(AgentEnRoute, AgentEnRoute.Location, false) 
				 && (VSizeSq(ArrivingAgent.Location - Location) < VSizeSq(AgentEnRoute.Location - Location)) )
			{
				// switch places
				//`log("Switching "$ArrivingAgent$" for "$AgentEnRoute);
				QueueHead.AddCustomer(AgentEnRoute,self);
				AgentEnRoute = ArrivingAgent;
			}
			else
			{
				QueueHead.AddCustomer(ArrivingAgent,self);
			}
		}
		else
		{
			if( QueueHead != None )
			{
				`warn(self$" added customer "$ArrivingAgent$" beyond capacity with queue "$QueueHead);
			}
		}
	}
	else
	{
		AgentEnRoute = ArrivingAgent;
	CustomerCount++;
}
}

simulated function bool AtCapacity( optional byte CheckCnt )
{
	return (CustomerCount + CheckCnt) >= Capacity;
}

/**
  * Returns true if this destination is valid for Agent
  */
simulated event bool AllowableDestinationFor(GameCrowdAgent Agent)
{
	local int i;
	local bool bSupported;
	
	if( !bHasNavigationMesh || !bIsEnabled )
	{
		return FALSE;
	}

	if( bIsBeyondSpawnDistance )
	{
		// FIXMESTEVE - maybe allow moving to beyond max spawn distance destination if currently close enough
		return FALSE;
	}
	
	if( !bAllowAsPreviousDestination && Agent.PreviousDestination == self ) 
	{
		return FALSE;
	}
	
	// check if allowed by agent's behavior
	if( Agent.CurrentBehavior != None && !Agent.CurrentBehavior.AllowThisDestination(self) )
	{
		return FALSE;
	}	

	// check if destination has room - make sure there is room for the whole group
	if( (Agent.MyGroup != None && AtCapacity(Agent.MyGroup.Members.Length-1)) || AtCapacity() || (QueueHead != None && !QueueHead.HasSpace()) )
	{
		return FALSE;
	}
	
	// check if this interaction is tagged
	if( InteractionTag != '' )
	{
		i = Agent.RecentInteractions.Find('InteractionTag',InteractionTag);
		if( i != INDEX_NONE && (Agent.RecentInteractions[i].InteractionDelay == 0.f || WorldInfo.TimeSeconds < Agent.RecentInteractions[i].InteractionDelay) )
		{
			return FALSE;
		}
		else if( i != INDEX_NONE )
		{
			// clear out the old interaction
			Agent.RecentInteractions.Remove(i,1);
		}
	}

	if( bHasRestrictions )
	{
		// make sure the agent class/archetype is supported
		if( SupportedAgentClasses.Length > 0 || SupportedArchetypes.Length > 0 )
		{
			bSupported = FALSE;
			for( i = 0; i < SupportedAgentClasses.Length; i++ )
			{
				if( ClassIsChildOf( Agent.Class, SupportedAgentClasses[i] ) )
				{
					bSupported = TRUE;
					break;
				}
			}

			// only check against supported archetypes if failed supported classes list
			if( !bSupported )
			{
				for( i = 0; i < SupportedArchetypes.Length; i++ )
				{
					if( SupportedArchetypes[i] == Agent.MyArchetype )
					{
						bSupported = TRUE;
						break;
					}
				}
			}

			if( !bSupported )
			{
				return FALSE;
			}
		}
		
	
		// if passed the supported test, make sure not in restricted classes list
		for( i = 0; i < RestrictedAgentClasses.Length; i++ )
		{
			if( ClassIsChildOf( Agent.Class, RestrictedAgentClasses[i] ) )
			{
				return FALSE;
			}
		}

		for( i = 0; i < RestrictedArchetypes.Length; i++ )
		{
			if( RestrictedArchetypes[i] == Agent.MyArchetype )
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

simulated function float GetSpawnRadius()
{
	return CylinderComponent.CollisionRadius;
}

// FIXMESTEVE - natively show the spawn line in the editor if bLineSpawner
simulated function GetSpawnPosition(SeqAct_GameCrowdSpawner Spawner, out vector SpawnPos, out rotator SpawnRot)
{
	local vector SpawnLine;
	local float	RandScale;
	
	// LINE SPAWN
	if(bLineSpawner)
	{
		// Scale between -1.0 and 1.0
		RandScale = -1.0 + (2.0 * FRand());
		// Get line along which to spawn.
		SpawnLine = vect(0,1,0) >> Rotation;
		// Now make the position
		SpawnPos = Location + (RandScale * SpawnLine * GetSpawnRadius());

		// Always face same way as spawn location
		SpawnRot.Yaw = Rotation.Yaw;
	}
	else
	{
		// CIRCLE SPAWN
		SpawnRot = RotRand(false);
		SpawnRot.Pitch = 0;

		if(bSpawnAtEdge)
		{
			SpawnPos = Location + ((vect(1,0,0) * GetSpawnRadius()) >> SpawnRot);
		}
		else
		{
			SpawnPos = Location + ((vect(1,0,0) * FRand() * GetSpawnRadius()) >> SpawnRot);
		}
	}
}

simulated function bool AnalyzeSpawnPoint( const out array<CrowdSpawnerPlayerInfo> PlayerInfo, float MaxSpawnDistSq, bool bForceNavMeshPathing, NavigationHandle NavHandle )
{
	local Actor HitActor;
	local vector HitLocation, HitNormal;
	local int NextIdx, PlayerIdx;
	local GameCrowdDestination NextGCD;
	local float DistFromView, DistFromPred;

	bIsVisible = TRUE;
	bAdjacentToVisibleNode = FALSE;
	bWillBeVisible = FALSE;
	Priority = 0.f;
	bCanSpawnHereNow = FALSE;
	bHasNavigationMesh = TRUE;
	bIsBeyondSpawnDistance = TRUE;
	for( PlayerIdx = 0; PlayerIdx < PlayerInfo.Length; PlayerIdx++ )
	{
		DistFromView = VSizeSq(PlayerInfo[PlayerIdx].ViewLocation - Location);
		DistFromPred = VSizeSq(PlayerInfo[PlayerIdx].PredictLocation - Location);
		if( FMin( DistFromView, DistFromPred ) < MaxSpawnDistSq )
		{
			bIsBeyondSpawnDistance = FALSE;
			break;
		}
	}

	if( bIsEnabled && bAllowsSpawning )
	{
		if( bForceNavMeshPathing && NavHandle.LineCheck(Location, Location - vect(0,0,3)* CylinderComponent.CollisionHeight, vect(0,0,0)) )
		{
			// no nav mesh streamed in, so can't use for spawning
			bHasNavigationMesh = FALSE;
		}
		else
		{
			if( !bIsBeyondSpawnDistance )
			{
				bCanSpawnHereNow = TRUE;
				bIsVisible = FALSE;
				for( PlayerIdx = 0; PlayerIdx < PlayerInfo.Length; PlayerIdx++ )
				{
					HitActor = Trace(HitLocation, HitNormal, Location, PlayerInfo[PlayerIdx].ViewLocation, FALSE);
					if( HitActor == None )
					{
						bIsVisible = TRUE;
						break;
					}
				}

				if( !bIsVisible )
				{
					for( PlayerIdx = 0; PlayerIdx < PlayerInfo.Length; PlayerIdx++ )
					{
						HitActor = Trace(HitLocation, HitNormal, Location, PlayerInfo[PlayerIdx].PredictLocation, FALSE);
						if( HitActor == None )
						{
							bWillBeVisible = TRUE;
							break;
						}
					}
				}
			}

			if( bIsVisible )
			{
				// Allow spawning at destinations beyond the max spawn dist if connected to visible destinations inside the spawn dist
				for( NextIdx = 0; NextIdx < NextDestinations.Length; NextIdx++ )
				{
					NextGCD = NextDestinations[NextIdx];
					if( NextGCD != None && NextGCD.bIsVisible && NextGCD.bCanSpawnHereNow && !NextGCD.bIsBeyondSpawnDistance )
					{
						bAdjacentToVisibleNode = TRUE;
						if( bIsBeyondSpawnDistance )
						{
							bCanSpawnHereNow = TRUE;
						}
					}
				}
			}
		}
		return TRUE;
	}

	return FALSE;
}

simulated function PrioritizeSpawnPoint( const out array<CrowdSpawnerPlayerInfo> PlayerInfo, float MaxSpawnDist )
{
	local float DistToSpawn;
	local int PlayerIdx;

	// Priority based on inverse of distance
	DistToSpawn = 999999.f;
	for( PlayerIdx = 0; PlayerIdx < PlayerInfo.Length; PlayerIdx++ )
	{
		DistToSpawn = FMin( DistToSpawn, VSize(Location - PlayerInfo[PlayerIdx].ViewLocation));
	}
	Priority = 1.f - ((MaxSpawnDist - DistToSpawn) / MaxSpawnDist);

	// prefer destinations that are about to become visible
	if( bWillBeVisible )
	{
		Priority *= 10.f;
	}
	else if( bAdjacentToVisibleNode )
	{
		Priority *= 5.f;
	}

	// prefer destinations that haven't been used recently
	Priority *= FMin(`TimeSince(LastSpawnTime), 10.0);
}

function float GetDestinationRadius()
{
	return CylinderComponent.CollisionRadius;
}

simulated function DrawDebug( const out array<CrowdSpawnerPlayerInfo> PlayerInfo, optional bool bPresistent )
{
	local int PlayerIdx;
	local Vector Extent;

	Extent.X = CylinderComponent.CollisionRadius;
	Extent.Y = CylinderComponent.CollisionRadius;
	Extent.Z = CylinderComponent.CollisionHeight * 2.f;

	for( PlayerIdx = 0; PlayerIdx < PlayerInfo.Length; PlayerIdx++ )
	{
		if( bIsBeyondSpawnDistance )
		{
			DrawDebugLine( Location, PlayerInfo[PlayerIdx].ViewLocation, 255, 0, 0, bPresistent );
			if( PlayerIdx == 0 )
			{
				DrawDebugSphere( Location, 20, 20, 255, 0, 0, bPresistent );
			}
		}
		else if( !bIsEnabled || !bAllowsSpawning )
		{
			if( PlayerIdx == 0 )
			{
				DrawDebugLine( Location, PlayerInfo[PlayerIdx].ViewLocation, 128, 0, 0, bPresistent );
			}
			DrawDebugSphere( Location, 20, 20, 128, 0, 0, bPresistent );
		}
		else if( bIsVisible )
		{
			if( PlayerIdx == 0 )
			{
				DrawDebugLine( Location, PlayerInfo[PlayerIdx].ViewLocation, 255, 0, 0, bPresistent );
			}
			DrawDebugBox( Location, Extent, 255, 0, 0, bPresistent );
		}
		else
		{
			if( PlayerIdx == 0 )
			{
				DrawDebugLine( Location, PlayerInfo[PlayerIdx].ViewLocation, 0, 255, 0, bPresistent );
			}
			DrawDebugBox( Location, Extent, 0, 255, 0, bPresistent );
		}
	}

	if( bAdjacentToVisibleNode )
	{
		DrawDebugStar( Location, 8, 0, 255, 0, bPresistent );
	}
	if( bWillBeVisible )
	{
		DrawDebugStar( Location + vect(0,0,8), 8, 0, 0, 255, bPresistent );
	}
	if( bCanSpawnHereNow )
	{
		DrawDebugStar( Location + vect(0,0,16), 8, 255, 255, 255, bPresistent );
	}

//	`log( self@"Pri:"@Priority );
}

defaultproperties
{
	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.Crowd.T_Crowd_Destination'
		Scale=0.5
	End Object

	bAllowAsPreviousDestination=false
	bAvoidWhenPanicked=false
	bSkipBehaviorIfPanicked=true
	Capacity=1000
	Frequency=1.0
	ExactReachTolerance=3.0
	bAllowsSpawning=TRUE
	bAllowCloudSpawning=TRUE
	bSoftPerimeter=true
	bStatic=true
	bForceAllowKismetModification=true
	bHasNavigationMesh=true
	
	SupportedEvents.Empty
	SupportedEvents(0)=class'SeqEvent_CrowdAgentReachedDestination'
	
	Begin Object Class=GameDestinationConnRenderingComponent Name=ConnectionRenderer
	End Object
	Components.Add(ConnectionRenderer)

}