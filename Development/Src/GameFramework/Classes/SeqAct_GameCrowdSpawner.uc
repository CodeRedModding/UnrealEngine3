/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/
class SeqAct_GameCrowdSpawner extends SeqAct_GameCrowdPopulationManagerToggle
	abstract
	native;

static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 5;
}

defaultproperties
{
	ObjName="Game Scripted Crowd Spawner"
	ObjCategory="Crowd"

	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Spawn Points",PropertyName=PotentialSpawnPoints)

	bFillPotentialSpawnPoints=TRUE
	bIndividualSpawner=TRUE
}
