/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnEdDragTools.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FDragTool
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FDragTool::FDragTool()
	:	bConvertDelta( TRUE )
	,	Start( FVector(0,0,0) )
	,	End( FVector(0,0,0) )
	,	EndWk( FVector(0,0,0) )
	,	bUseSnapping( FALSE )
	,	ViewportClient( NULL )
{
}

/**
 * Updates the drag tool's end location with the specified delta.  The end location is
 * snapped to the editor constraints if bUseSnapping is TRUE.
 *
 * @param	InDelta		A delta of mouse movement.
 */
void FDragTool::AddDelta( const FVector& InDelta )
{
	EndWk += InDelta;

	End = EndWk;

	// Snap to constraints.
	if( bUseSnapping )
	{
		const FLOAT GridSize = GEditor->Constraints.GetGridSize();
		const FVector GridBase( GridSize, GridSize, GridSize );
		GEditor->Constraints.Snap( End, GridBase );
	}
}

/**
 * Starts a mouse drag behavior.  The start location is snapped to the editor constraints if bUseSnapping is TRUE.
 *
 * @param	InViewportClient	The viewport client in which the drag event occurred.
 * @param	InStart				Where the mouse was when the drag started.
 */
void FDragTool::StartDrag(FEditorLevelViewportClient* InViewportClient, const FVector& InStart)
{
	ViewportClient = InViewportClient;
	Start = InStart;

	// Snap to constraints.
	if( bUseSnapping )
	{
		const FLOAT GridSize = GEditor->Constraints.GetGridSize();
		const FVector GridBase( GridSize, GridSize, GridSize );
		GEditor->Constraints.Snap( Start, GridBase );
	}
	End = EndWk = Start;

	// Store button state when the drag began.
	bAltDown = ViewportClient->Input->IsAltPressed();
	bShiftDown = ViewportClient->Input->IsShiftPressed();
	bControlDown = ViewportClient->Input->IsCtrlPressed();
	bLeftMouseButtonDown = ViewportClient->Viewport->KeyState(KEY_LeftMouseButton);
	bRightMouseButtonDown = ViewportClient->Viewport->KeyState(KEY_RightMouseButton);
	bMiddleMouseButtonDown = ViewportClient->Viewport->KeyState(KEY_MiddleMouseButton);
}

/**
 * Ends a mouse drag behavior (the user has let go of the mouse button).
 */
void FDragTool::EndDrag()
{
	Start = End = EndWk = FVector(0,0,0);
	ViewportClient = NULL;
}
