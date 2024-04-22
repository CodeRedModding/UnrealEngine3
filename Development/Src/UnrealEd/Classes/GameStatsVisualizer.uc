/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*
* Visualization interface for game statistics
*/
class GameStatsVisualizer extends GameStatsVisitorImpl
	abstract
	native(GameStats);

cpptext
{
	/** Given a chance to initialize */
	virtual void Init() {}

	/** Reset the visualizer to initial state */
	virtual void Reset() {}

	/** Given a chance to cleanup on destruction */
	virtual void Cleanup() {}

	/** 
	 * Draw your stuff 
	 * @param View - the view being drawn in
	 * @param PDI - draw interface for primitives
	 * @param ViewportType - type of viewport being draw (perspective, ortho)
	 */
	virtual void Visualize(const FSceneView* View, class FPrimitiveDrawInterface* PDI, ELevelViewportType ViewportType) {}

	/** 
	 * Draw your stuff as a canvas overlay 
	 * @param ViewportClient - viewport client currently drawing
     * @param View - the view being drawn in
	 * @param Canvas - overlay canvas
	 * @param ViewportType - type of viewport being draw (perspective, ortho)
	 */
	virtual void VisualizeCanvas(FEditorLevelViewportClient* ViewportClient, const FSceneView* View, FCanvas* Canvas, ELevelViewportType ViewportType) {}

	/** Returns the number of data points the visualizer is actively working with */
	virtual INT GetVisualizationSetCount() const { return 0; }

	/** 
	 *	Retrieve some metadata about an event
	 * @param EventIndex - some visualizer relative index about the data to get metadata about
	 * @param MetadataString - return string containing information about the event requested
	 */
	virtual void GetMetadata(INT EventIndex, FString& MetadataString) {}

	/** 
	 * Called when a hitproxy belonging to this visualizer is triggered 
	 * @param HitProxy - hitproxy the editor says was triggered
	 */
	virtual void HandleHitProxy(struct HGameStatsHitProxy* HitProxy) {}

	/** Get the friendly name of the visualization type */
	virtual const FString& GetFriendlyName() { return FriendlyName; }

	/** 
	 * Associates this visualization with the given GUI parent that created it 
	 * @param InParent - Parent to associate with this visualizer
	 */
	virtual void SetParent(WxGameStatsVisualizer* InParent) { Parent = InParent; }

	/** 
	 * Returns a dialog box with options related to the visualizer
	 * @return NULL if no options for this visualizer, else pointer to dialog
	 */
	virtual class WxVisualizerOptionsDialog* GetOptionsDialog() { return NULL; }

	/**
	 * Invalidates all viewports for the specified location/rotation
	 *
	 * @param Location the location to invalidate
	 * @param Rotation the rotation to invalidate
	 */
	void InvalidateViewportsForEvent(const FVector& Location,const FRotator& Rotation);
}

/** The parent gui */
var native private const pointer Parent{class WxGameStatsVisualizer};

/** Human readable name for this visualizer */
var string FriendlyName;

/** Name of the options dialog resource */
var string OptionsDialogName;

/** The options dialog (will be NULL before first call to options) */
var native private const pointer OptionsDialog{class WxVisualizerOptionsDialog};

/** Supported events by this visualizer */
var config array<int> SupportedEvents;

defaultproperties
{
	FriendlyName="Base Visualizer" 
	OptionsDialogName=""
}
