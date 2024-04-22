/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Convenience subclass, with typical settings for interaction points
 * 
 */
class GameCrowdInteractionDestination extends GameCrowdDestination;

defaultproperties
{
	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.Crowd.T_Crowd_Destination'
	End Object

	Capacity=1
	bAllowsSpawning=false
	bAvoidWhenPanicked=true
	bMustReachExactly=true
}
