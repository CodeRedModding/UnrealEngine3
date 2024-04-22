/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/
class SeqAct_GameCrowdPopulationManagerToggle extends SequenceAction
	implements(GameCrowdSpawnerInterface)
	native;

/** Percentage of max population to immediately spawn when the population manager is toggled on (without respecting visibility checks).  Range is 0.0 to 1.0 */
var() float	WarmupPopulationPct;

/** List of Archetypes of agents for pop manager to spawn when this is toggled on */
var() GameCrowd_ListOfAgents	CrowdAgentList;

/** If true, clear old population manager archetype list rather than adding to it with this toggle action's CrowdAgentList. */
var() bool	bClearOldArchetypes;

/** The maximum number of agents alive at one time. */
var() int	MaxAgents;

/** How many agents per second will be spawned at the target actor(s).  */
var() float	SpawnRate;

/** Whether to enable the light environment on crowd members. */
var() bool	bEnableCrowdLightEnvironment;
/** Whether agents from this spawner should cast shadows */
var() bool	bCastShadows;
/** Lighting channels to put the agents in. */
var LightingChannelContainer	AgentLightingChannel;

/** Max distance allowed for spawns */
var() float MaxSpawnDist;
/** Square of min distance allowed for in line of sight but out of view frustrum agent spawns */
var float MinBehindSpawnDist;
/** List of all GameCrowdDestinations that are PotentialSpawnPoints */
var array<GameCrowdDestination> PotentialSpawnPoints;
var bool bFillPotentialSpawnPoints;

/** Average time to "warm up" spawned agents before letting them sleep if not rendered */
var float AgentWarmupTime;

var()   int NumAgentsToTickPerFrame;

/** If true, force obstacle checking for all agents from this spawner */
var() bool bForceObstacleChecking;
/** If true, force nav mesh navigation for all agents from this spawner */
var() bool bForceNavMeshPathing;

var bool bIndividualSpawner;

var array<GameCrowdAgent>  LastSpawnedList;

cpptext
{
	virtual void Activated();
	UBOOL UpdateOp(FLOAT DeltaTime);
};

static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 5;
}

event FillCrowdSpawnInfoItem( out CrowdSpawnInfoItem out_Item, GameCrowdPopulationManager PopMgr )
{
	local int i;

	// if wanted, clear out current list of agent archetypes
	if( bClearOldArchetypes )
	{
		out_Item.AgentArchetypes.Length = 0;
	}
	if( CrowdAgentList != None )
	{
		// get new agent archetypes to use from kismet action
		for( i = 0; i < CrowdAgentList.ListOfAgents.Length; i++ )
		{
			out_Item.AgentArchetypes[out_Item.AgentArchetypes.Length] = CrowdAgentList.ListOfAgents[i];
		}
	}

	out_Item.MaxSpawnDist           = MaxSpawnDist;
	out_Item.MaxSpawnDistSq         = out_Item.MaxSpawnDist * out_Item.MaxSpawnDist;
	out_Item.MinBehindSpawnDist     = FMin( MinBehindSpawnDist, out_Item.MaxSpawnDist * 0.0625 );
	out_Item.MinBehindSpawnDistSq   = out_Item.MinBehindSpawnDist * out_Item.MinBehindSpawnDist;
	out_Item.AgentWarmupTime        = AgentWarmupTime;

	out_Item.bCastShadows = bCastShadows;
	out_Item.bEnableCrowdLightEnvironment = bEnableCrowdLightEnvironment;

	out_Item.SpawnRate        = SpawnRate;
	out_Item.SpawnNum         = MaxAgents;
	if( class'Engine'.static.IsSplitScreen() )
	{
		out_Item.SpawnNum = PopMgr.SplitScreenNumReduction * float(out_Item.SpawnNum);
	}

	out_Item.bForceObstacleChecking   = bForceObstacleChecking;
	out_Item.bForceNavMeshPathing     = bForceNavMeshPathing;

	out_Item.NumAgentsToTickPerFrame  = NumAgentsToTickPerFrame;
	out_Item.LastAgentTickedIndex     = -1;

	if( bFillPotentialSpawnPoints )
	{
		out_Item.PotentialSpawnPoints     = PotentialSpawnPoints;
	}
}

/** 
  * GameCrowdSpawnerInterface
  */
function float GetMaxSpawnDist()
{
	return MaxSpawnDist;
}

function AgentDestroyed( GameCrowdAgent Agent )
{
	local GameCrowdPopulationManager PopMgr;

	PopMgr = GameCrowdPopulationManager(Agent.WorldInfo.PopulationManager);
	if( PopMgr != None )
	{
		PopMgr.AgentDestroyed( Agent );
	}
}

defaultproperties
{
	bCallHandler=FALSE
	bLatentExecution=TRUE
	ObjName="Population Manager Toggle"
	ObjCategory="Crowd"

	InputLinks(0)=(LinkDesc="Start")
	InputLinks(1)=(LinkDesc="Stop")
	InputLinks(2)=(LinkDesc="Warmup")
	InputLinks(3)=(LinkDesc="Kill Agents")
	InputLinks(4)=(LinkDesc="Stop & Kill")

	OutputLinks.Empty
	OutputLinks(0)=(LinkDesc="Spawned")

	VariableLinks.Empty
	VariableLinks(0)=(LinkDesc="Spawned List",ExpectedType=class'SeqVar_ObjectList',bWriteable=TRUE,MaxVars=1,PropertyName=LastSpawnedList)


	SpawnRate=50
	MaxAgents=700
	AgentWarmupTime=2.0

	MaxSpawnDist=10000.0
	MinBehindSpawnDist=5000.0

	NumAgentsToTickPerFrame=10

	bForceObstacleChecking=FALSE
	bForceNavMeshPathing=TRUE

	AgentLightingChannel=(Crowd=TRUE,bInitialized=TRUE)
}
