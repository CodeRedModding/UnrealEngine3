/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DRAGTOOL_BOXSELECT_H__
#define __DRAGTOOL_BOXSELECT_H__

#include "UnEdDragTools.h"

/**
 * Draws a box in the current viewport and when the mouse button is released,
 * it selects/unselects everything inside of it.
 */
class FDragTool_BoxSelect : public FDragTool
{
public:

	/**
	 * Starts a mouse drag behavior.  The start location is snapped to the editor constraints if bUseSnapping is TRUE.
	 *
	 * @param	InViewportClient	The viewport client in which the drag event occurred.
	 * @param	InStart				Where the mouse was when the drag started.
	 */
	virtual void StartDrag(FEditorLevelViewportClient* InViewportClient, const FVector& InStart);

	/* Updates the drag tool's end location with the specified delta.  The end location is
	 * snapped to the editor constraints if bUseSnapping is TRUE.
	 *
	 * @param	InDelta		A delta of mouse movement.
	 */
	virtual void AddDelta( const FVector& InDelta );

	/**
	* Ends a mouse drag behavior (the user has let go of the mouse button).
	*/
	virtual void EndDrag();
	virtual void Render3D(const FSceneView* View,FPrimitiveDrawInterface* PDI);

private:
	/** 
	 * Calculates a box to check actors against 
	 * 
	 * @param OutBox	The created box.
	 */
	void CalculateBox( FBox& OutBox );

	/** 
	 * Returns TRUE if the passed in Actor intersects with the provided box 
	 *
	 * @param InActor				The actor to check
	 * @param InBox					The box to check against
	 * @param bUseStrictSelection	TRUE if the actor must be entirely within the frustum
	 */
	UBOOL IntersectsBox( AActor& InActor, const FBox& InBox, UBOOL bUseStrictSelection );

	/** 
	 * Returns TRUE if the provided BSP node intersects with the provided frustum 
	 *
	 * @param InModel				The model containing BSP nodes to check
	 * @param NodeIndex				The index to a BSP node in the model.  This node is used for the bounds check.
	 * @param InFrustum				The frustum to check against.
	 * @param bUseStrictSelection	TRUE if the node must be entirely within the frustum
	 */
	UBOOL IntersectsBox( const UModel& InModel, INT NodeIndex, const FBox& InBox, UBOOL bUseStrictSelection ) const;
	
	/** Adds a hover effect to the passed in actor */
	void AddHoverEffect( AActor& InActor );

	/** Adds a hover effect to the passed in bsp surface */
	void AddHoverEffect( UModel& InModel, INT SurfIndex );

	/** Removes a hover effect from the passed in actor */
	void RemoveHoverEffect( AActor& InActor );

	/** Removes a hover effect from the passed in bsp surface */
	void RemoveHoverEffect( UModel& InModel, INT SurfIndex );

	/** List of BSP models to check for selection */
	TArray<UModel*> ModelsToCheck;
};

#endif // __DRAGTOOL_BOXSELECT_H__
