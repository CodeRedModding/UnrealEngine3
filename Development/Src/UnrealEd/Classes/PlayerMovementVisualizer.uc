/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*
* Visualizes the movement of players through the map as a series of lines
*/
class PlayerMovementVisualizer extends GameStatsVisualizer			 
	native(GameStats)
	config(Editor);

/** Array of drawing properties per stat type setup in .ini */
struct native PlayerMovementStatDrawingProperties
{
	var string PawnClassName;  //Name of the pawn type (checked as a substring so CogMarcus works for CogMarcusMP as well)
	var string SpriteName;     //Name of the sprite resource
	var Texture2D StatSprite;  //Actual sprite texture
};

/** Atomic position/rotation entry at a given time */
struct native PosEntry
{
	var float Time;
	var vector Position;
	var rotator Rotation;
};

/** String of player positions defining a contiguous movement in game */
struct native MovementSegment
{
	var array<PosEntry> Positions;
};

/** A given player's movement throughout the entire data set  */
struct native PlayerMovement
{
	var int PlayerIndex;
	var string PlayerName;
	var array<MovementSegment> Segments;

	var Texture2D StatSprite; //sprite assigned to head of movement

	//Held for sorting at the end then emptied
	var array<PosEntry> TempPositions;
};

cpptext
{
	/** Given a chance to initialize */
	virtual void Init();

	/** Reset the visualizer to initial state */
	virtual void Reset();

	/** 
	 * Draws all players with unique color within the given time period
	 * taking into account time/space jumps
	 * @param View - the view being drawn in
	 * @param PDI - draw interface for primitives
	 * @param ViewportType - type of viewport being draw (perspective, ortho)
	 */
	virtual void Visualize(const FSceneView* View, class FPrimitiveDrawInterface* PDI, ELevelViewportType ViewportType);

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

	/** 
	* Return the drawing properties defined for the given player 
	* @param PawnClassName - Name of the pawn spawned
	*/
	const FPlayerMovementStatDrawingProperties& GetDrawingProperties(const FString& PawnClassName);

	/** Called when a hitproxy belonging to this visualizer is triggered */
	virtual void HandleHitProxy(struct HGameStatsHitProxy* HitProxy);

	/** Player locations during the game are stored as PlayerIntEntries */
	virtual void Visit(class PlayerIntEntry* Entry); 

	/** Player spawns reveal the pawn class in use so we can choose a sprite */
	virtual void Visit(class PlayerSpawnEntry* Entry); 

	/** Create or find a given player entry by index */
	FPlayerMovement& CreateOrFindPlayerEntry(INT PlayerIndex, const FString& PlayerName);
}

/** All data to be drawn by this visualizer */
var array<PlayerMovement> Players;

/** Metadata to help draw the statistics */
var const config array<PlayerMovementStatDrawingProperties> DrawingProperties; 

defaultproperties
{
	FriendlyName="Player Movement Visualizer" 
}