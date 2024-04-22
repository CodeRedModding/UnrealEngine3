/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNEDDRAGTOOLS_H__
#define __UNEDDRAGTOOLS_H__

// Forward declarations.
struct FEditorLevelViewportClient;

/**
 * The base class that all drag tools inherit from.
 * The drag tools implement special behaviors for the user clicking and dragging in a viewport.
 */
class FDragTool
{
public:
	FDragTool();
	virtual ~FDragTool() {}

	/**
	 * Updates the drag tool's end location with the specified delta.  The end location is
	 * snapped to the editor constraints if bUseSnapping is TRUE.
	 *
	 * @param	InDelta		A delta of mouse movement.
	 */
	virtual void AddDelta( const FVector& InDelta );

	/**
	 * Starts a mouse drag behavior.  The start location is snapped to the editor constraints if bUseSnapping is TRUE.
	 *
	 * @param	InViewportClient	The viewport client in which the drag event occurred.
	 * @param	InStart				Where the mouse was when the drag started.
	 */
	virtual void StartDrag(FEditorLevelViewportClient* InViewportClient, const FVector& InStart);

	/**
	 * Ends a mouse drag behavior (the user has let go of the mouse button).
	 */
	virtual void EndDrag();
	virtual void Render3D(const FSceneView* View,FPrimitiveDrawInterface* PDI) {}
	virtual void Render(const FSceneView* View,FCanvas* Canvas) {}

	/**
	 * Rendering stub for 2D viewport drag tools.
	 */
	virtual void Render( FCanvas* Canvas ) {}

	/** Does this drag tool need to have the mouse movement converted to the viewport orientation? */
	UBOOL bConvertDelta;

protected:
	/** The start/end location of the current drag. */
	FVector Start, End, EndWk;

	/** If TRUE, the drag tool wants to be passed grid snapped values. */
	UBOOL bUseSnapping;

	/** The viewport client that we are tracking the mouse in. */
	FEditorLevelViewportClient* ViewportClient;

	/** These flags store the state of various buttons that were pressed when the drag was started. */
	UBOOL bAltDown;
	UBOOL bShiftDown;
	UBOOL bControlDown;
	UBOOL bLeftMouseButtonDown;
	UBOOL bRightMouseButtonDown;
	UBOOL bMiddleMouseButtonDown;
};

#endif // __UNEDDRAGTOOLS_H__
