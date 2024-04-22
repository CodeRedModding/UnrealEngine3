/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*
* Visualizes the movement of players through the map as a series of lines
*/
class HeatmapVisualizer extends GameStatsVisualizer			 
	native(GameStats)
	config(Editor);

/** Atomic position/rotation entry at a given time */
struct native HeatMapPosEntry
{				
	/** Time entry was recorded */
	var float Time; 
	/** Location of entry */
	var vector Position;
	/** Strength to give position */
	var float Strength;
};


cpptext
{
	/** Given a chance to initialize */
	virtual void Init();

	/** Reset the visualizer to initial state */
	virtual void Reset();

	/** 
	 * Returns a dialog box with options related to the visualizer
	 * @return NULL if no options for this visualizer, else pointer to dialog
	 */
	virtual class WxVisualizerOptionsDialog* GetOptionsDialog();

	/** 
	 * Draws all players with unique color within the given time period
	 * taking into account time/space jumps
	 * @param View - the view being drawn in
	 * @param PDI - draw interface for primitives
	 * @param ViewportType - type of viewport being draw (perspective, ortho)
	 */
	virtual void Visualize(const FSceneView* View, class FPrimitiveDrawInterface* PDI, ELevelViewportType ViewportType);

	/** 
	 * Draw your stuff as a canvas overlay 
	 * @param View - the view being drawn in
	 * @param Canvas - overlay canvas
 	 * @param ViewportType - type of viewport being draw (perspective, ortho)
	 */
	virtual void VisualizeCanvas(FEditorLevelViewportClient* ViewportClient, const FSceneView* View, FCanvas* Canvas, ELevelViewportType ViewportType);

	/*
	 *   Actual DrawTile call to the canvas, using min/max screen values to properly position the texture
	 * @param Canvas - canvas to draw to
	 * @param MinScreen - WorldMin position of the heatmap, converted to screen space
	 * @param MaxScreen - WorldMax position of the heatmap, converted to screen space
	 */
	void VisualizeCanvas(FCanvas* Canvas, const FVector2D& MinScreen, const FVector2D& MaxScreen);

	/** Called before any database entries are given to the visualizer */
	virtual void BeginVisiting();

	/** Called at the end of database entry traversal, returns success or failure */
	virtual UBOOL EndVisiting();

	/** Returns the number of data points the visualizer is actively working with */
	virtual INT GetVisualizationSetCount() const;

	/** 
	 *	Retrieve some metadata about an event
	 * @param EventIndex - some visualizer relative index about the data to get metadata about
	 * @param MetadataString - return string containing information about the event requested
	 */
	virtual void GetMetadata(INT EventIndex, FString& MetadataString);

	/** Called when a hitproxy belonging to this visualizer is triggered */
	virtual void HandleHitProxy(struct HGameStatsHitProxy* HitProxy);

	/** Game locations during the game are stored as GamePositionEntries */
	virtual void Visit(class GamePositionEntry* Entry); 

	/** Player locations during the game are stored as PlayerIntEntries */
	virtual void Visit(class PlayerIntEntry* Entry); 

	/** Player kills during the game are stored as PlayerKillDeathEnties */
	virtual void Visit(class PlayerKillDeathEntry* Entry); 

	/** Player spawns during the game are stored as PlayerSpawnEntries */
	virtual void Visit(class PlayerSpawnEntry* Entry); 

	/** Projectile ints the game are stored as ProjectileIntEntries */
	virtual void Visit(class ProjectileIntEntry* Entry); 

	/** the goats are in the base, and they like to use generic param lists to specify heatmap targets as well */
	virtual void Visit(class GenericParamListEntry* Entry);

	/** adds a new point to the heatmap, and adjusts bounds for incoming position */
	virtual void AddNewPoint(const FVector& Pt, FLOAT Time, FLOAT Strength);
	/**
	 * Runs through the data and creates a heatmap texture, normalizing values
	 */
	virtual void CreateHeatmapTexture();
	/**
	 * Runs through the data and splats an attenuating set of values for each data point
	 */
	virtual void CreateHeatmapGrid();
	/*
	 * Scan the array of grid points and find the min/max grid values, or specify custom min/max values
	 * @param NewMinDensity - all values at or below this value get the lowest coloring
	 * @param NewMaxDensity - all values at or above this value get the highest coloring
	 */
	virtual void UpdateDensityMapping(INT NewMinDensity, INT NewMaxDensity);

	/**
	 * Called before destroying the object.  This is called immediately upon deciding to destroy the object, to allow the object to begin an
	 * asynchronous cleanup process.
	 */
	void BeginDestroy();
}

/** World bounds */
var vector WorldMinPos;
var vector WorldMaxPos;

/** The dimensions of the heatmap texture */
var int TextureXSize;
var int TextureYSize;

/** User defined value used to normalize rendered data */ 
var int CurrentMinDensity;
/** Uset defined value used to normalize rendered data */
var int CurrentMaxDensity;

/** Min count (non-zero) of the stat found in all heatmap buckets */ 
var int MinDensity;
/** Max count of the stat found in all heatmap buckets */
var int MaxDensity;
/** Radius of "bleed" added at each point on the heatmap */
var int HeatRadius;
/** Number of Unreal units per pixel when generating the heatmap */
var float NumUnrealUnitsPerPixel;

/** Reference to the material that renders the heatmap */
var MaterialInstanceConstant HeatmapMaterial;

/** Reference to the texture generated from the heatmap data */
var Texture2D OverlayTexture;

/** All data to be drawn by this visualizer */
var array<HeatMapPosEntry> HeatmapPositions;

/** Non-normalized heat data as draw from the heatmap positions */
var array<float> PositionGrid;

defaultproperties
{
	FriendlyName="Heatmap Visualizer" 
	OptionsDialogName="ID_HEATMAPOPTIONS"

	CurrentMinDensity=-1
	CurrentMaxDensity=-1
	HeatRadius=5
	NumUnrealUnitsPerPixel=15

	TextureXSize=256
	TextureYSize=256
}
