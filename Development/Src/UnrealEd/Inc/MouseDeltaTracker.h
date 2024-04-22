/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MOUSEDELTATRACKER_H__
#define __MOUSEDELTATRACKER_H__

// Forward declarations.
class FDragTool;
class FScopedTransaction;
struct FEditorLevelViewportClient;

/**
 * Keeps track of mouse movement deltas in the viewports.
 */
class FMouseDeltaTracker
{
public:
	/** This variable is TRUE if the last call to StartTracking() selected the builder brush because there was
	 * nothing else selected and the ctrl key was being held down.
	 */
	UBOOL bSelectedBuilderBrush;

	FMouseDeltaTracker();
	~FMouseDeltaTracker();

	/**
	 * Begin tracking at the specified location for the specified viewport.
	 */
	void StartTracking(FEditorLevelViewportClient* InViewportClient, const INT InX, const INT InY, UBOOL bArrowMovement = FALSE);

	/**
	 * Called when a mouse button has been released.  If there are no other
	 * mouse buttons being held down, the internal information is reset.
	 */
	UBOOL EndTracking(FEditorLevelViewportClient* InViewportClient);

	/**
	 * Adds delta movement into the tracker.
	 */
	void AddDelta(FEditorLevelViewportClient* InViewportClient, const FName InKey, const INT InDelta, UBOOL InNudge);

	/**
	* Returns the current delta.
	*/
	const FVector GetDelta() const;

	/**
	 * Returns the current snapped delta.
	 */
	const FVector GetDeltaSnapped() const;

	/**
	* Returns the absolute delta since dragging started.
	*/
	const FVector GetAbsoluteDelta() const;

	/**
	* Returns the absolute snapped delta since dragging started. 
	*/
	const FVector GetAbsoluteDeltaSnapped() const;

	/**
	 * Returns the screen space delta since dragging started.
	 */
	const FVector GetScreenDelta() const;

	/**
	 * Converts the delta movement to drag/rotation/scale based on the viewport type or widget axis.
	 */
	void ConvertMovementDeltaToDragRot(FEditorLevelViewportClient* InViewportClient, const FVector& InDragDelta, FVector& InDrag, FRotator& InRotation, FVector& InScale);
	/**
	 * Absolute Translation conversion from mouse position on the screen to widget axis movement/rotation.
	 */
	void AbsoluteTranslationConvertMouseToDragRot(FSceneView* InView, FEditorLevelViewportClient* InViewportClient, FVector& InDrag, FRotator& InRotation, FVector& InScale );

	/**
	 * Subtracts the specified value from End and EndSnapped.
	 */
	void ReduceBy(const FVector& In);

	/**
	 * @return		TRUE if a drag tool is being used by the tracker, FALSE otherwise.
	 */
	UBOOL UsingDragTool() const;

	/**
	 * Marks that something caused the equivalent of mouse dragging, but with other means (keyboard short cut, mouse wheel).  Allows suppression of context menus with flight camera, etc
	 */
	void SetExternalMovement (void)			{ bExternalMovement = TRUE;};
	/**
	 * @return		TRUE if something caused external movement of the mouse (keyboard, mouse wheel)
	 */
	UBOOL WasExternalMovement (void) const { return bExternalMovement; }

	/**
	 * Renders the drag tool.  Does nothing if no drag tool exists.
	 */
	void Render3DDragTool(const FSceneView* View, FPrimitiveDrawInterface* PDI);

	/**
	 * Renders the drag tool.  Does nothing if no drag tool exists.
	 */
	void RenderDragTool(const FSceneView* View, FCanvas* Canvas);

private:
	/** The unsnapped start position of the current mouse drag. */
	FVector Start;
	/** The snapped start position of the current mouse drag. */
	FVector StartSnapped;
	/** The screen space start position of the current mouse drag. */
	FVector StartScreen;
	/** The unsnapped end position of the current mouse drag. */
	FVector End;
	/** The snapped end position of the current mouse drag. */
	FVector EndSnapped;
	/** The screen space end position of the current mouse drag. */
	FVector EndScreen;

	/** The amount that the End vectors have been reduced by since dragging started, this is added to the deltas to get an absolute delta. */
	FVector ReductionAmount;

	/**
	 * If there is a dragging tool being used, this will point to it.
	 * Gets newed/deleted in StartTracking/EndTracking.
	 */
	FDragTool* DragTool;

	/** Count how many transactions we've started. */
	INT		TransCount;

	/** Keeps track of whether AddDelta has been called since StartTracking. */
	UBOOL	bHasReceivedAddDelta;

	/** The current transaction. */
	FScopedTransaction*	ScopedTransaction;

	/** This is set to TRUE if StartTracking() has initiated a transaction. */
	UBOOL bTrackingTransactionStarted;
	/** Tracks whether keyboard/mouse wheel/etc have caused simulated mouse movement.  Reset on StartTracking */
	UBOOL bExternalMovement;


	/**
	 * Sets the current axis of the widget for the specified viewport.
	 *
	 * @param	InViewportClient		The viewport whose widget axis is to be set.
	 */
	void DetermineCurrentAxis(FEditorLevelViewportClient* InViewportClient);

	/**
	 * Initiates a transaction.
	 */
	void BeginTransaction(const TCHAR* SessionName);

	/**
	 * Ends the current transaction, if one exists.
	 */
	void EndTransaction();
};

static const FLOAT MOUSE_CLICK_DRAG_DELTA	= 4.0f;

#endif // __MOUSEDELTATRACKER_H__
