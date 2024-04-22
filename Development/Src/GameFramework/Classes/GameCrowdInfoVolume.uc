/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameCrowdInfoVolume extends Volume
	native
	placeable;

/** List of all GameCrowdDestinations that are PotentialSpawnPoints */
var() array<GameCrowdDestination> PotentialSpawnPoints;

simulated function Touch( Actor Other, PrimitiveComponent OtherComp, vector HitLocation, vector HitNormal )
{
	local Pawn P;
	local GameCrowdPopulationManager PopMgr;

	Super.Touch( Other, OtherComp, HitLocation, HitNormal );

	P = Pawn(Other);
	if( P != None && P.IsHumanControlled() )
	{
		PopMgr = GameCrowdPopulationManager(WorldInfo.PopulationManager);
		if( PopMgr != None )
		{
			PopMgr.SetCrowdInfoVolume( self );
		}
	}
}

simulated function UnTouch( Actor Other )
{
	local Pawn P;
	local GameCrowdPopulationManager PopMgr;

	Super.UnTouch( Other );

	P = Pawn(Other);
	if( P != None && P.IsHumanControlled() )
	{
		PopMgr = GameCrowdPopulationManager(WorldInfo.PopulationManager);
		if( PopMgr != None )
		{
			PopMgr.SetCrowdInfoVolume( None );
		}
	}
}

defaultproperties
{
}