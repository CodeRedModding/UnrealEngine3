/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*
*  Manages adding appropriate crowd population around player
*  Agents will be spawned/recycled at any available active GameCrowdDestination
*
*/
class GameCrowdPopulationManager extends CrowdPopulationManagerBase
	implements(Interface_NavigationHandle)
	dependson(SeqAct_GameCrowdSpawner)
	native;

var CrowdSpawnInfoItem          CloudSpawnInfo;
var array<CrowdSpawnInfoItem>   ScriptedSpawnInfo;

var GameCrowdInfoVolume         ActiveCrowdInfoVolume;
var array<GameCrowdDestination> GlobalPotentialSpawnPoints;

/** How much to reduce number by in splitscreen */
var	float	SplitScreenNumReduction;

/** How far ahead to compute predicted player position for spawn prioritization */
var float PlayerPositionPredictionTime;

/** Offset used to validate spawn by checking above spawn location to see if head/torso would be visible */
var float HeadVisibilityOffset;

/** Navigation Handle used by agents requesting pathing */
var     class<NavigationHandle>         NavigationHandleClass;
var     NavigationHandle                NavigationHandle;

/** Agent requesting navigation handle use */
var GameCrowdAgent QueryingAgent;

var     array<CrowdSpawnerPlayerInfo> PlayerInfo;
var     float                         LastPlayerInfoUpdateTime;

var(Debug) bool bDebugSpawns;
var(Debug) bool bPauseCrowd;

cpptext
{
	virtual UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );
	virtual void TickSpawnInfo( FCrowdSpawnInfoItem& Item, FLOAT DeltaTime );

	virtual void GetAlwaysRelevantDynamics( AGameCrowdAgent* Agent );

	/** Interface_NavigationHandle implementation to grab search params */
	virtual void SetupPathfindingParams( FNavMeshPathParams& out_ParamCache );
	virtual void InitForPathfinding() {}
	virtual INT ExtraEdgeCostToAddWhenActive(FNavMeshEdgeBase* Edge) { return 0; }
	virtual FVector GetEdgeZAdjust(FNavMeshEdgeBase* Edge);

	UBOOL GetSpawnInfoItem( USeqAct_GameCrowdPopulationManagerToggle* inAction, FCrowdSpawnInfoItem*& out_Spawner, UBOOL bCreateIfNotFound = 0 );
}

function PostBeginPlay()
{
	local GameCrowdDestination GCD;

	Super.PostBeginPlay();

	if( !bDeleteMe )
	{
		WorldInfo.PopulationManager = self;
	}

	if( NavigationHandleClass != None )
	{
		NavigationHandle = new(self) NavigationHandleClass;
	}

	// add spawn points that have already begun play
	foreach AllActors(class'GameCrowdDestination', GCD)
	{
		AddSpawnPoint(GCD);
	}
}

// Interface_navigationhandle stub - called when path edge is deleted that this controller is using
event NotifyPathChanged();

function AddSpawnPoint( GameCrowdDestination GCD )
{
	if( GCD.MyPopMgr != None || !GCD.bAllowCloudSpawning )
	{
		return;
	}
	GCD.MyPopMgr = self;
	GlobalPotentialSpawnPoints[GlobalPotentialSpawnPoints.Length] = GCD;

	if( ActiveCrowdInfoVolume == None )
	{
		CloudSpawnInfo.PotentialSpawnPoints[CloudSpawnInfo.PotentialSpawnPoints.Length] = GCD;
	}
}

function RemoveSpawnPoint(GameCrowdDestination GCD)
{
	local int Idx, AgentIdx;

	GCD.MyPopMgr = None;

	// remove from potential spawnpoints and prioritized spawn points list
	// also remove agents moving toward unloaded spawn point
	CloudSpawnInfo.PotentialSpawnPoints.RemoveItem( GCD );
	CloudSpawnInfo.PrioritizedSpawnPoints.RemoveItem( GCD );
	GlobalPotentialSpawnPoints.RemoveItem( GCD );
	for( AgentIdx = 0; AgentIdx < CloudSpawnInfo.ActiveAgents.Length; AgentIdx++ )
	{
		if( CloudSpawnInfo.ActiveAgents[AgentIdx].CurrentDestination == GCD )
		{
			CloudSpawnInfo.ActiveAgents[AgentIdx].Destroy();
		}
	}


	for( Idx = 0; Idx < ScriptedSpawnInfo.Length; Idx++ )
	{
		ScriptedSpawnInfo[Idx].PotentialSpawnPoints.RemoveItem( GCD );
		ScriptedSpawnInfo[Idx].PrioritizedSpawnPoints.RemoveItem( GCD );

		for( AgentIdx = 0; AgentIdx < ScriptedSpawnInfo[Idx].ActiveAgents.Length; AgentIdx++ )
		{
			if( ScriptedSpawnInfo[Idx].ActiveAgents[AgentIdx].CurrentDestination == GCD )
			{
				ScriptedSpawnInfo[Idx].ActiveAgents[AgentIdx].Destroy();
			}
		}
	}
}

function SetCrowdInfoVolume( GameCrowdInfoVolume Vol )
{
	if( Vol != ActiveCrowdInfoVolume )
	{
		ActiveCrowdInfoVolume = Vol;
		if( Vol != None )
		{
			CloudSpawnInfo.PotentialSpawnPoints = Vol.PotentialSpawnPoints;
		}
		else
		{
			CloudSpawnInfo.PotentialSpawnPoints = GlobalPotentialSpawnPoints;
		}

		CloudSpawnInfo.PrioritizedSpawnPoints.Length = 0;
		CloudSpawnInfo.PrioritizationIndex = 0;
		CloudSpawnInfo.PrioritizationUpdateIndex = 0;
	}
}

event int CreateSpawner( SeqAct_GameCrowdPopulationManagerToggle inAction )
{
	local int Idx;
	Idx = ScriptedSpawnInfo.Length;
	ScriptedSpawnInfo.Length = Idx + 1;
	ScriptedSpawnInfo[Idx].SeqSpawner = inAction;
	return Idx;
}

/** Instantly destroy all active agents controlled by this manager. Useful for debugging.  */
event FlushAgents( CrowdSpawnInfoItem Item )
{
	local int AgentIdx;

	for( AgentIdx = 0; AgentIdx < Item.ActiveAgents.Length; AgentIdx++ )
	{
		Item.ActiveAgents[AgentIdx].Destroy();
	}
	Item.ActiveAgents.Length = 0;
}
event  FlushAllAgents()
{
	local int Idx;
	FlushAgents( CloudSpawnInfo );
	for( Idx = 0; Idx < ScriptedSpawnInfo.Length; Idx++ )
	{
		FlushAgents( ScriptedSpawnInfo[Idx] );
	}
}

function AgentDestroyed( GameCrowdAgent Agent )
{
	local int SpawnerIdx;
	local int i;

	SpawnerIdx = ScriptedSpawnInfo.Find('SeqSpawner', SeqAct_GameCrowdPopulationManagerToggle(Agent.MySpawner));
	if( SpawnerIdx >= 0 )
	{
		// now modify the CurrSpawned amount for this archetype since we just destroyed one
		for( i = 0; i < ScriptedSpawnInfo[SpawnerIdx].AgentArchetypes.Length; i++ )
		{
			if( GameCrowdAgent(ScriptedSpawnInfo[SpawnerIdx].AgentArchetypes[i].AgentArchetype) == Agent.MyArchetype )
			{
				ScriptedSpawnInfo[SpawnerIdx].AgentArchetypes[i].CurrSpawned--;
				//`log( GetFuncName() @ `showvar(AgentArchetypes[i].AgentArchetype) @ `showvar(AgentArchetypes[i].CurrSpawned) );
			}
		}

		ScriptedSpawnInfo[SpawnerIdx].ActiveAgents.RemoveItem( Agent );
	}
	else if( Agent.MySpawner != None )
	{
		// now modify the CurrSpawned amount for this archetype since we just destroyed one
		for( i = 0; i < CloudSpawnInfo.AgentArchetypes.Length; i++ )
		{
			if( GameCrowdAgent(CloudSpawnInfo.AgentArchetypes[i].AgentArchetype) == Agent.MyArchetype )
			{
				CloudSpawnInfo.AgentArchetypes[i].CurrSpawned--;
				//`log( GetFuncName() @ `showvar(AgentArchetypes[i].AgentArchetype) @ `showvar(AgentArchetypes[i].CurrSpawned) );
			}
		}

		CloudSpawnInfo.ActiveAgents.RemoveItem( Agent );
	}
}

/**
  *  Use 'GameDebug' console command to show this debug info
  *  Useful to show general debug info not tied to a particular concrete actor.
  */
simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local Canvas	Canvas;
	local int RenderedNum, LOSNum, SimNum, ActualCount, DistanceBucket[20], i, RVONum;
	local Actor HitActor;
	local vector HitNormal, HitLocation;
	local GameCrowdAgent GCA;
	local float Dist;
	local array<GameCrowdAgent> AgentList;
	local int PlayerIdx, SpawnIdx;
	local bool bHasLOS;
	local float BucketSize;

	Canvas = HUD.Canvas;
	Canvas.SetDrawColor(255,255,255);

	Canvas.SetPos(4,out_YPos);
	Canvas.DrawText("---- GameCrowdPopulationManager ---");
	out_YPos += out_YL;

	if( !GetPlayerInfo() )
	{
		return;
	}

	// calculate number of agents being rendered, simulated, and in player's LOS
	ForEach DynamicActors(class'GameCrowdAgent', GCA)
	{
		if( !GCA.bDeleteMe )
		{
			AgentList[AgentList.Length] = GCA;
		}
	}

	BucketSize = (2.f*CloudSpawnInfo.MaxSpawnDist) / ArrayCount(DistanceBucket);
	foreach AgentList(GCA)
	{
		ActualCount++;
		if( GCA.Health > 0 )
		{
			if( GCA.bSimulateThisTick )
			{
				SimNum++;
			}
			if( `TimeSince(GCA.LastRenderTime) < 1.0 && (GCA.LastRenderTime != GCA.InitialLastRenderTime) )
			{
				bHasLOS = TRUE;
				RenderedNum++;
			}
			else
			{
				bHasLOS = FALSE;
				for( PlayerIdx = 0; PlayerIdx < PlayerInfo.Length; PlayerIdx++ )
				{
					HitActor = Trace( HitLocation, HitNormal, GCA.Location, PlayerInfo[PlayerIdx].ViewLocation, FALSE );
					if( HitActor == None )
					{
						bHasLOS = TRUE;
						break;
					}
				}

			}

			if( bHasLOS )
			{
				LOSNum++;
				if( GCA.bSimulateThisTick )
				{
					RVONum++;
				}
			}

			GCA.bSimulateThisTick = FALSE;
		}
		Dist = 999999.f;
		for( PlayerIdx = 0; PlayerIdx < PlayerInfo.Length; PlayerIdx++ )
		{
			Dist = FMin(VSize(PlayerInfo[PlayerIdx].ViewLocation - GCA.Location), Dist);
		}
		DistanceBucket[Min(19, int(Dist/BucketSize))]++;
	}

	Canvas.DrawText("TotalCount: "$ActualCount );
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);

	Canvas.DrawText("Cloud:"@CloudSpawnInfo.ActiveAgents.Length@"Active:"@CloudSpawnInfo.bSpawningActive );
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);
	for( SpawnIdx = 0; SpawnIdx < ScriptedSpawnInfo.Length; SpawnIdx++ )
	{
		Canvas.DrawText("Scripted: "$ScriptedSpawnInfo[SpawnIdx].ActiveAgents.Length@ScriptedSpawnInfo[SpawnIdx].SeqSpawner@"Active:"@ScriptedSpawnInfo[SpawnIdx].bSpawningActive );
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
	}	
	
	Canvas.DrawText("Agents Rendered:"@RenderedNum);
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);
	Canvas.DrawText("Agents LOS:"@LOSNum);
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);
	Canvas.DrawText("Agents Simulated:"@SimNum);
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);
	Canvas.DrawText("Agents RVO:"@RVONum);
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);

	Canvas.DrawText("Distance Buckets");
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);
	for ( i=0; i<19; i++ )
	{
		if( DistanceBucket[i] > 0 )
		{
			Canvas.DrawText(" (<"$BucketSize * (i+1)$")"$DistanceBucket[i]);
			out_YPos += out_YL;
			Canvas.SetPos(4,out_YPos);
		}
	}

}

/** returns whether we want spawning to currently be active */
function bool IsSpawningActive()
{
	local int SpawnerIdx;

	if( CloudSpawnInfo.bSpawningActive )
	{
		return TRUE;
	}

	for( SpawnerIdx = 0; SpawnerIdx < ScriptedSpawnInfo.Length; SpawnerIdx++ )
	{
		if( ScriptedSpawnInfo[SpawnerIdx].bSpawningActive )
		{
			return TRUE;
		}
	}

	return FALSE;
}

simulated function bool ShouldDebugDestinations()
{
	return bDebugSpawns;
}

/**
  * FIXMESTEVE - Nativize?
  */
function Tick( float DeltaTime )
{
`if(`notdefined(FINAL_RELEASE))
	local GameCrowdDestination PickedSpawnPoint;
	local int Idx;
	local int PlayerIdx;
//	local Color C;
`endif

`if(`notdefined(FINAL_RELEASE))
	if( ShouldDebugDestinations() && GetPlayerInfo() )
	{
		for( PlayerIdx = 0; PlayerIdx < PlayerInfo.Length; PlayerIdx++ )
		{
			DrawDebugBox( PlayerInfo[PlayerIdx].PredictLocation, vect(20,20,20), 255, 0, 0 );
			DrawDebugBox( PlayerInfo[PlayerIdx].ViewLocation, vect(10,10,10), 255, 255, 255 );
			DrawDebugLine( PlayerInfo[PlayerIdx].ViewLocation, PlayerInfo[PlayerIdx].ViewLocation + Vector(PlayerInfo[PlayerIdx].ViewRotation) * 64, 255, 255, 255 );
//			DrawDebugSphere( PlayerInfo[PlayerIdx].ViewLocation, GetMaxSpawnDist(), 20, 255, 255, 255 );
		}

		for( Idx = 0; Idx < CloudSpawnInfo.PotentialSpawnPoints.Length; Idx++ )
		{
			PickedSpawnPoint = CloudSpawnInfo.PotentialSpawnPoints[Idx];
			if( PickedSpawnPoint == None )
			{
				continue;
			}
			PickedSpawnPoint.AnalyzeSpawnPoint( PlayerInfo, CloudSpawnInfo.MaxSpawnDistSq, CloudSpawnInfo.bForceNavMeshPathing, NavigationHandle );
			PickedSpawnPoint.PrioritizeSpawnPoint( PlayerInfo, CloudSpawnInfo.MaxSpawnDist );
			PickedSpawnPoint.DrawDebug( PlayerInfo );

			if( !ValidateSpawnAt( CloudSpawnInfo, PickedSpawnPoint) )
			{
				DrawDebugCylinder( PickedSpawnPoint.Location, PickedSpawnPoint.Location, PickedSpawnPoint.CylinderComponent.CollisionRadius, PickedSpawnPoint.CylinderComponent.CollisionHeight, 255, 0, 0 );
			}
		}

// 		for( Idx = 0; Idx < CloudSpawnInfo.ActiveAgents.Length; Idx++ )
// 		{
// 			C = CloudSpawnInfo.ActiveAgents[Idx].DebugAgentColor;
// 			DrawDebugLine( CloudSpawnInfo.ActiveAgents[Idx].DebugSpawnDest.Location, CloudSpawnInfo.ActiveAgents[Idx].Location, C.R, C.G, C.B );
// 		}
	}
`endif

	if( !bPauseCrowd && IsSpawningActive() )
	{
		UpdateAllSpawners( DeltaTime );
	}
}

native function UpdateAllSpawners( float DeltaTime );

event bool UpdateSpawner( out CrowdSpawnInfoItem Item, float DeltaTime )
{
	local GameCrowdDestination PickedSpawnPoint;
	local GameCrowdAgent A;
	local int NumSpawned;

	if( !Item.bSpawningActive || Item.ActiveAgents.Length >= Item.SpawnNum )
	{
		return FALSE;
	}

	if( Item.SeqSpawner != None )
	{
		Item.SeqSpawner.LastSpawnedList.Length = 0;
	}

	Item.Remainder += FMin( DeltaTime, 0.05f ) * Item.SpawnRate;
	if( Item.Remainder > 1.f )
	{
		// Prioritize based on potential visibility and recently spawned agents
		PrioritizeSpawnPoints( Item, DeltaTime );

		// Spawn new agents for this tick
		while( Item.Remainder > 1.f && Item.ActiveAgents.Length < Item.SpawnNum)
		{
			PickedSpawnPoint = PickSpawnPoint( Item );
			if( PickedSpawnPoint != None )
			{
				PickedSpawnPoint.LastSpawnTime = WorldInfo.TimeSeconds;
				A = SpawnAgent( Item, PickedSpawnPoint);
				if( A != None )
				{
					NumSpawned++;
					if( Item.SeqSpawner != None )
					{
						Item.SeqSpawner.LastSpawnedList.AddItem( A );
					}
				}
				Item.Remainder -= 1.f;
			}
			else
			{
				Item.Remainder = 0.0;
			}
		}
	}

	return (NumSpawned>0);
}

/**
  * @RETURN best spawn point to spawn next crowd agent
  */
event GameCrowdDestination PickSpawnPoint( out CrowdSpawnInfoItem Item )
{
	local int StartingIndex, SpawnIdx;
	local GameCrowdDestination Candidate;

	// Go down prioritized list, make sure currently valid (still not visible if not prioritize frame)
	StartingIndex = Min(Item.PrioritizationIndex, Item.PrioritizedSpawnPoints.Length);
	for( SpawnIdx = 0; SpawnIdx < Item.PrioritizedSpawnPoints.Length; SpawnIdx++ )
	{
		Item.PrioritizationIndex = (StartingIndex + SpawnIdx) % Item.PrioritizedSpawnPoints.Length;
		Candidate = Item.PrioritizedSpawnPoints[Item.PrioritizationIndex];
		if( ValidateSpawnAt( Item, Candidate ) )
		{
			return Candidate;
		}
	}

	return None;
}

native simulated function bool GetPlayerInfo();
native static simulated function bool StaticGetPlayerInfo( out array<CrowdSpawnerPlayerInfo> out_PlayerInfo );

/**
  *  Prioritize GameCrowdDestinations as potential spawn points
  */
event PrioritizeSpawnPoints( out CrowdSpawnInfoItem Item, float DeltaTime )
{
	local int UpdateNum;

	if( Item.PotentialSpawnPoints.Length == 0 || !GetPlayerInfo() )
	{
		return;
	}

	// calculate number of potential spawn points to prioritize this tick
	UpdateNum = Max(1, DeltaTime * float(Item.PotentialSpawnPoints.Length)/Item.SpawnPrioritizationInterval);

	// Analyze and prioritize a number of spawn points
	AnalyzeSpawnPoints( Item, Item.PrioritizationUpdateIndex, UpdateNum );
	Item.PrioritizationUpdateIndex = (Item.PrioritizationUpdateIndex + UpdateNum) % Item.PotentialSpawnPoints.Length;
}

function AnalyzeSpawnPoints( out CrowdSpawnInfoItem Item, int StartIndex, int NumToUpdate )
{
	local int UpdateIdx, Idx, NumUpdated;
	local GameCrowdDestination GCD;

	if( StartIndex >= Item.PotentialSpawnPoints.Length || !GetPlayerInfo() )
	{
		return;
	}

	// determine potential visibility of all GameCrowdDestinations
	NumUpdated = 0;
	for( UpdateIdx = 0; NumUpdated < NumToUpdate && UpdateIdx < Item.PotentialSpawnPoints.Length; UpdateIdx++ )
	{
		Idx = (StartIndex + UpdateIdx) % Item.PotentialSpawnPoints.Length;
		GCD = Item.PotentialSpawnPoints[Idx];
		if( GCD == None )
		{
			Item.PotentialSpawnPoints.Remove(UpdateIdx--,1);
			continue;
		}

		Item.PrioritizedSpawnPoints.RemoveItem(GCD);
		if( GCD.AnalyzeSpawnPoint( PlayerInfo, Item.MaxSpawnDistSq, Item.bForceNavMeshPathing, NavigationHandle ) )
		{
			NumUpdated++;

			// add GCD back to list if is potential spawn point
			if( GCD.bCanSpawnHereNow )
			{
				AddPrioritizedSpawnPoint( Item, GCD );
			}
		}
	}
}

/**
  * Prioritize passed in GameCrowdDestination and insert it into ordered PrioritizedSpawnPoints list, offset from current starting point
  */
function AddPrioritizedSpawnPoint( out CrowdSpawnInfoItem Item, GameCrowdDestination GCD )
{
	local int SpawnIdx, StartingIndex;

	GCD.PrioritizeSpawnPoint( PlayerInfo, Item.MaxSpawnDist );

	// insert GCD into prioritized list
	StartingIndex = Min(Item.PrioritizationIndex, Item.PrioritizedSpawnPoints.Length);
	for( SpawnIdx = 0; SpawnIdx < Item.PrioritizedSpawnPoints.Length; SpawnIdx++ )
	{
		Item.PrioritizationIndex = (StartingIndex + SpawnIdx) % Item.PrioritizedSpawnPoints.Length;
		if( Item.PrioritizedSpawnPoints[Item.PrioritizationIndex].Priority < GCD.Priority )
		{
			Item.PrioritizedSpawnPoints.Insert(Item.PrioritizationIndex, 1);
			Item.PrioritizedSpawnPoints[Item.PrioritizationIndex] = GCD;
			return;
		}
	}

	// add right at current index (and increment index since this one should be last
	Item.PrioritizedSpawnPoints.Insert(StartingIndex, 1);
	Item.PrioritizedSpawnPoints[StartingIndex] = GCD;
	Item.PrioritizationIndex = (StartingIndex + 1) % Item.PrioritizedSpawnPoints.Length;
}

/**
  *  Determine whether candidate spawn point is currently valid
  */
function bool ValidateSpawnAt( out CrowdSpawnInfoItem Item, GameCrowdDestination Candidate)
{
	local Actor HitActor;
	local vector HitLocation, HitNormal;
	local float DistSq, MinDistFromViewSq;
	local float DestDotView;
	local int PlayerIdx;

	// make sure candidate not at capacity
	if( !Candidate.bIsEnabled || !Candidate.bAllowsSpawning || Candidate.AtCapacity() )
	{
		return FALSE;
	}

	if( Candidate.bAllowVisibleSpawning )
	{
		return TRUE;
	}

	// check that spawn point is not visible to player
	if( GetPlayerInfo() )
	{
		MinDistFromViewSq = MaxInt;
		for( PlayerIdx = 0; PlayerIdx < PlayerInfo.Length; PlayerIdx++ )
		{
			// if candidate is beyond max (normal) spawn dist, it's a special case and we don't mind if it is visible
			// also don't mind if far away and not in view frustrum
			DistSq = VSizeSq(Candidate.Location - PlayerInfo[PlayerIdx].ViewLocation);
			MinDistFromViewSq = FMin( DistSq, MinDistFromViewSq );
			if( DistSq < Item.MaxSpawnDistSq )
			{
				DestDotView = Normal(Candidate.Location - PlayerInfo[PlayerIdx].ViewLocation) DOT Vector(PlayerInfo[PlayerIdx].ViewRotation);
				if( DistSq < Item.MinBehindSpawnDistSq || DestDotView > 0.7f )
				{
					HitActor = Trace(HitLocation, HitNormal, Candidate.Location + HeadVisibilityOffset*vect(0,0,1), PlayerInfo[PlayerIdx].ViewLocation, FALSE,,, TRACEFLAG_Bullet);
					if( HitActor == None )
					{
						return FALSE;
					}
				}
			}
		}

		if( MinDistFromViewSq < Item.MaxSpawnDistSq )
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**
  *  Actually create a new CrowdAgent actor, and initialise it
  */
native function GameCrowdAgent SpawnAgentByIdx( int SpawnerIdx, GameCrowdDestination SpawnLoc );
native function GameCrowdAgent SpawnAgent( out CrowdSpawnInfoItem Item, GameCrowdDestination SpawnLoc );
native function bool Warmup( out CrowdSpawnInfoItem Item, int WarmupNum );

/**
  * Create new GameCrowdAgent and initialize it
  */
event GameCrowdAgent CreateNewAgent( out CrowdSpawnInfoItem Item, GameCrowdDestination SpawnLoc, GameCrowdAgent AgentTemplate, GameCrowdGroup NewGroup)
{
	local GameCrowdAgent	Agent;
	local rotator	        SpawnRot;
	local vector	        SpawnPos;
	local int i;

	// GameCrowdSpawnInterface provides spawn location (can be line/circle/volume/etc. based)
	GameCrowdSpawnInterface(SpawnLoc).GetSpawnPosition(none, SpawnPos, SpawnRot);
	if( !GetPlayerInfo() )
	{
		return None;
	}

	Agent = Spawn( AgentTemplate.Class,,,SpawnPos,SpawnRot,AgentTemplate);
	Agent.SetLighting(Item.bEnableCrowdLightEnvironment, Item.AgentLightingChannel, Item.bCastShadows);

	if( Item.bForceObstacleChecking )
	{
		Agent.bCheckForObstacles = TRUE;
	}
	if( Item.bForceNavMeshPathing )
	{
		Agent.bUseNavMeshPathing = TRUE;
	}

	// don't prefer visible paths on spawn if on soon to be visible start
	if( SpawnLoc.bWillBeVisible )
	{
		Agent.bPreferVisibleDestinationOnSpawn = Agent.bPreferVisibleDestination;
	}

	Agent.MySpawner = GameCrowdSpawnerInterface(Item.SeqSpawner);
	Item.ActiveAgents[Item.ActiveAgents.Length] = Agent;
	Agent.InitializeAgent(SpawnLoc, PlayerInfo, AgentTemplate, NewGroup, Item.AgentWarmUpTime*2.0*FRand(), (Item.AgentWarmupTime>0.f), TRUE );

	// now find the archetype and update the CurrSpawned
	for( i = 0; i < Item.AgentArchetypes.Length; i++ )
	{
		if( GameCrowdAgent(Item.AgentArchetypes[i].AgentArchetype) == Agent.MyArchetype )
		{
			Item.AgentArchetypes[i].CurrSpawned++;
		}
	}

	return Agent;
}

defaultproperties
{
	NavigationHandleClass=class'NavigationHandle'

	SplitScreenNumReduction=0.5

	PlayerPositionPredictionTime=5.0

	HeadVisibilityOffset=40.0

	RemoteRole=ROLE_None
	NetUpdateFrequency=10
	bHidden=TRUE
	bOnlyDirtyReplication=TRUE
	bSkipActorPropertyReplication=TRUE
}
