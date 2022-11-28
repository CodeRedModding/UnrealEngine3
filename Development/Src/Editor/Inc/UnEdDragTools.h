/*=============================================================================
	UnEdDragTools.h: Special mouse drag behaviors

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

struct FEditorLevelViewportClient;

// -----------------------------------------------------------------------------

/**
 * FDragTool
 *
 * The base class that all drag tools inherit from.
 *
 * The drag tools implement special behaviors for the user clicking and dragging in a viewport.
 */

class FDragTool
{
public:
	FDragTool();
	virtual ~FDragTool();

	/** The start/end location of the current drag. */
	FVector Start, End, EndWk;

	/** If TRUE, the drag tool wants to be passed grid snapped values. */
	UBOOL bUseSnapping;

	/** Does this drag tool need to have the mouse movement converted to the viewport orientation? */
	UBOOL bConvertDelta;

	/** The viewport client that we are tracking the mouse in. */
	FEditorLevelViewportClient* ViewportClient;

	/** Store the states of various buttons that were pressed when the drag was started */
	UBOOL bAltDown, bShiftDown, bControlDown, bLeftMouseButtonDown, bRightMouseButtonDown, bMiddleMouseButtonDown;

	void AddDelta( const FVector& InDelta );

	virtual void StartDrag( FEditorLevelViewportClient* InViewportClient, const FVector& InStart );
	virtual void EndDrag();
	virtual void RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI) {}
	virtual void Render(const FSceneContext& Context,FRenderInterface* RI) {}
};

// -----------------------------------------------------------------------------

/**
 * FDragTool_BoxSelect
 * 
 * Draws a box in the current viewport and when the mouse button is released,
 * it selects/unselects everything inside of it.
 */

class FDragTool_BoxSelect : public FDragTool
{
public:
	FDragTool_BoxSelect();
	virtual ~FDragTool_BoxSelect();

	virtual void EndDrag();
	virtual void RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
};

// -----------------------------------------------------------------------------

/**
 * FDragTool_Measure
 *
 * Draws a line in the current viewport and displays the distance between
 * its start/end points in the center of it.
 */

class FDragTool_Measure : public FDragTool
{
public:
	FDragTool_Measure();
	virtual ~FDragTool_Measure();

	virtual void RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
	virtual void Render(const FSceneContext& Context,FRenderInterface* RI);
};

/**
 * FMouseDeltaTracker
 * 
 * Keeps track of mouse movement deltas in the viewports.
 */

class FMouseDeltaTracker
{
public:
	FMouseDeltaTracker();
	~FMouseDeltaTracker();

	void DetermineCurrentAxis( FEditorLevelViewportClient* InViewportClient );
	void StartTracking( FEditorLevelViewportClient* InViewportClient, const INT InX, const INT InY );
	UBOOL EndTracking( FEditorLevelViewportClient* InViewportClient );
	void AddDelta( FEditorLevelViewportClient* InViewportClient, const FName InKey, const INT InDelta, UBOOL InNudge );
	FVector GetDelta();
	FVector GetDeltaSnapped();
	void ConvertMovementDeltaToDragRot( FEditorLevelViewportClient* InViewportClient, const FVector& InDragDelta, FVector& InDrag, FRotator& InRotation, FVector& InScale );
	void ReduceBy( FVector& In );

	/** The start/end positions of the current mouse drag, snapped and unsnapped. */
	FVector Start, StartSnapped, End, EndSnapped;
	
	/**
	 * If there is a dragging tool being used, this will point to it.
	 * Gets newed/deleted in StartTracking/EndTracking.
	 */
	FDragTool* DragTool;

	/** Are we transacting the mouse movement? */
	UBOOL bIsTransacting;
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
