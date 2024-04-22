/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*
* visualizes genericparamlistentries
*/
class GenericParamlistVisualizer extends GameStatsVisualizer			 
	native(GameStats)
	config(Editor);

struct native Line
{
	var Vector LineStart;
	var Vector LineEnd;
	var LinearColor  LineColor;
	var float Thickness;
};

struct native DrawBox
{
	var Vector BoxLoc;
	var Vector Extent;
	var Color BoxColor;
};

struct native DrawAtom
{
	var array<Line> Lines;
	var array<DrawBox> Boxes;
	var string ShortName;
	var string LongName;
	var Texture2D Sprite;
	var vector Loc;
	var LinearColor Color;
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


	/** Called when a hitproxy belonging to this visualizer is triggered */
	virtual void HandleHitProxy(struct HGameStatsHitProxy* HitProxy);

	virtual void Visit(class GenericParamListEntry* Entry); 
}
var array<DrawAtom>  DrawAtoms;

var Texture2D DatumSprite;

defaultproperties
{
	FriendlyName="Generic Visualizer (for debug stats mostly)" 
}