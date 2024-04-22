/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Used by actor classes which want to customize chosing the spawn position of agents spawned at their location.
 */

interface GameCrowdSpawnInterface;

function GetSpawnPosition(SeqAct_GameCrowdSpawner Spawner, out vector SpawnPos, out rotator SpawnRot);