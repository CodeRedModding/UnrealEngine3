/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*
* Visualizes the movement of players through the map as a series of lines
*/
class PerformanceVisualizer extends HeatmapVisualizer			 
	native(GameStats)
	config(Editor);

cpptext
{
	/** Reset the visualizer to initial state */
	virtual void Reset();

	/** Called before any database entries are given to the visualizer */
	virtual void BeginVisiting();

	/** Called at the end of database entry traversal, returns success or failure */
	virtual UBOOL EndVisiting();

	/** Game locations during the game are stored as GamePositionEntries */
	virtual void Visit(class GamePositionEntry* Entry); 

	/** Player locations during the game are stored as PlayerIntEntries */
	virtual void Visit(class PlayerIntEntry* Entry) { /** Do Nothing */ } 

	/** Player kills during the game are stored as PlayerKillDeathEnties */
	virtual void Visit(class PlayerKillDeathEntry* Entry) { /** Do Nothing */ } 

	/** Player spawns during the game are stored as PlayerSpawnEntries */
	virtual void Visit(class PlayerSpawnEntry* Entry) { /** Do Nothing */ } 

	/** the goats are in the base, and they like to use generic param lists to specify heatmap targets as well */
	virtual void Visit(class GenericParamListEntry* Entry) { /** Do Nothing */ }

	/**
	 * Runs through the data and splats an attenuating set of values for each data point
	 */
	virtual void CreateHeatmapGrid();
}
	
/** Max value recorded in each position */
var array<float> GridPositionMaxValues;
/** Number of times each grid location was hit 2D array in TextureXSize,TextureYSize */
var array<int> GridPositionHitCounts;
/** Sums of values in each grid position */
var array<float> GridPositionSums;

defaultproperties
{
	FriendlyName="Performance Visualizer" 
	OptionsDialogName="ID_HEATMAPOPTIONS"

	CurrentMinDensity=-1
	CurrentMaxDensity=-1
	HeatRadius=5
	NumUnrealUnitsPerPixel=15

	TextureXSize=256
	TextureYSize=256
}
