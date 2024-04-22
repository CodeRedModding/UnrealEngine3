/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This volume is used to control which levels are loaded/unloaded during
 * gameplay.
 */
class TriggerStreamingLevel extends Trigger;

/** Holds the various settings needed to un/load a streaming level */
struct LevelStreamingData
{
	/** Whether the level should be loaded */
	var() bool bShouldBeLoaded;
	/** Whether the level should be visible if it is loaded */
	var() bool bShouldBeVisible;
	/** Whether we want to force a blocking load */
	var() bool bShouldBlockOnLoad;
	/** The level that will be streamed in */
	var() LevelStreaming Level;
};

/** Holds the list of levels to load/unload when triggered */
var() editinline array<LevelStreamingData> Levels;

/**
 * Loads & unloads the specified streaming levels when triggered
 *
 * @param Other the actor generating the event
 * @param HitLocation the location of the touch
 * @param HitNormal the normal generated at the touch
 */
event Touch(Actor Other, PrimitiveComponent OtherComp, vector HitLocation, vector HitNormal)
{
	local PlayerController PlayerCon;
	local int Index;

	Super.Touch(Other,OtherComp,HitLocation,HitNormal);
	// Iterate through the levels un/loading them
	for (Index = 0; Index < Levels.Length; Index++)
	{
		// Notify each player of the change in level load status
		foreach WorldInfo.AllControllers(class'PlayerController',PlayerCon)
		{
			// Set the blocking flag
			Levels[Index].Level.bShouldBlockOnLoad = Levels[Index].bShouldBlockOnLoad;
			// Now un/load the level
			PlayerCon.LevelStreamingStatusChanged( 
				Levels[Index].Level,
				Levels[Index].bShouldBeLoaded,
				Levels[Index].bShouldBeVisible,
				Levels[Index].bShouldBlockOnLoad);
		}
	}
}

