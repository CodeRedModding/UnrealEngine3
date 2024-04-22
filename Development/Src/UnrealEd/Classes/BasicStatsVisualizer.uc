/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*
* Basic stat visualizer for game statistics
*/
class BasicStatsVisualizer extends GameStatsVisualizer			 
	native(GameStats)
	config(Editor);

/** Array of drawing properties per stat type setup in .ini */
struct native StatDrawingProperties
{
	var int EventID;		   //EventID associated with these properties
	var color StatColor;	   //Color to draw the sprite
	var float Size;			   //Size of the sprite
	var string SpriteName;     //Name of the sprite resource
	var Texture2D StatSprite;  //Actual sprite texture
};

/** Generic stat container */
struct native BasicStatEntry 
{
	var int EventID;
	var string EventName;
	var float EventTime;
	var vector Location;
	var rotator Rotation;
};

/** Stat container with enough data to draw player information */
struct native PlayerEntry extends BasicStatEntry
{
	var int PlayerIndex;
	var string PlayerName;
	var string WeaponName;
};

/** Stat container for a player player interaction */
struct native PlayerPlayerEntry extends BasicStatEntry 
{
	var int Player1Index;
	var string Player1Name;
	var int Player2Index;
	var string Player2Name;
	var vector Player2Location;
	var rotator Rotation2;
};

/** Stat container for a player targeting action (kill/melee/death/etc) */
struct native PlayerTargetEntry extends BasicStatEntry
{
	var string KillType;
	var string DamageType;
	
	var int PlayerIndex;
	var string PlayerName;

	var int TargetIndex;
	var string TargetName;
	var vector TargetLocation;
	var rotator TargetRotation;
};

cpptext
{
	/** Given a chance to initialize */
	virtual void Init();

	/** Reset the visualizer to initial state */
	virtual void Reset();

	/** 
	 * Visualizes all stats in a very basic way (sprite at a location with an orientation arrow typically)
	 * @param View - the view being drawn in
	 * @param PDI - draw interface for primitives
	 * @param ViewportType - type of viewport being draw (perspective, ortho)
	 */
	virtual void Visualize(const FSceneView* View, class FPrimitiveDrawInterface* PDI, ELevelViewportType ViewportType);

	/** 
	 * Draw your stuff as a canvas overlay 
	 * @param ViewportClient - viewport client currently drawing
     * @param View - the view being drawn in
	 * @param Canvas - overlay canvas
	 * @param ViewportType - type of viewport being draw (perspective, ortho)
	 */
	virtual void VisualizeCanvas(FEditorLevelViewportClient* ViewportClient, const FSceneView* View, FCanvas* Canvas, ELevelViewportType ViewportType);

	/** 
	 * Return the drawing properties defined for the given EventID 
	 * @param EventID - EventID to get the drawing property for
	 */
	const FStatDrawingProperties& GetDrawingProperties(int EventID);

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

	/** 
	  * Basic idea here is to just transfer/copy the stat information out of the database
	  * and into a form capable of displaying a sprite/arrow/color at a given position
	  */
	virtual void Visit(class GameStringEntry* Entry); 
	virtual void Visit(class GameIntEntry* Entry);
	virtual void Visit(class GamePositionEntry* Entry);
	virtual void Visit(class TeamIntEntry* Entry); 
	virtual void Visit(class PlayerStringEntry* Entry);
	virtual void Visit(class PlayerIntEntry* Entry); 
	virtual void Visit(class PlayerFloatEntry* Entry); 
	virtual void Visit(class PlayerLoginEntry* Entry);
	virtual void Visit(class PlayerSpawnEntry* Entry);
	virtual void Visit(class PlayerKillDeathEntry* Entry);
	virtual void Visit(class PlayerPlayerEntry * Entry);
	virtual void Visit(class WeaponEntry* Entry);
	virtual void Visit(class DamageEntry* Entry); 
	virtual void Visit(class ProjectileIntEntry* Entry);
}

/** Metadata to help draw the statistics */
var const config array<StatDrawingProperties> DrawingProperties;

/** All data to be drawn by this visualizer */
var array<BasicStatEntry> BasicEntries;
var array<PlayerEntry> PlayerEntries;
var array<PlayerPlayerEntry> PlayerPlayerEntries;
var array<PlayerTargetEntry> PlayerTargetEntries;

defaultproperties
{
	FriendlyName="Basic Stats Visualizer" 
}
