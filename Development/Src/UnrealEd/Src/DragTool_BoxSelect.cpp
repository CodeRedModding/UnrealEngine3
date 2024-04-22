/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DragTool_BoxSelect.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FDragTool_BoxSelect
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Starts a mouse drag behavior.  The start location is snapped to the editor constraints if bUseSnapping is TRUE.
 *
 * @param	InViewportClient	The viewport client in which the drag event occurred.
 * @param	InStart				Where the mouse was when the drag started.
 */
void FDragTool_BoxSelect::StartDrag(FEditorLevelViewportClient* InViewportClient, const FVector& InStart)
{
	FDragTool::StartDrag(InViewportClient, InStart);
	
	FEditorLevelViewportClient::ClearHoverFromObjects();

	// Create a list of bsp models to check for intersection with the box
	ModelsToCheck.Reset();
	// Do not select BSP if its not visible
	if( (ViewportClient->ShowFlags & SHOW_BSP) != 0 && (ViewportClient->ShowFlags & SHOW_ViewMode_BrushWireframe) == 0 )
	{
		// Add the persistent level always
		ModelsToCheck.AddItem( GWorld->PersistentLevel->Model );
		// Add all streaming level models
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		for( INT LevelIndex = 0; LevelIndex < WorldInfo->StreamingLevels.Num(); ++LevelIndex )
		{
			ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
			// Only add streaming level models if the level is visible
			if( StreamingLevel->bShouldBeVisibleInEditor )
			{	
				ModelsToCheck.AddItem( StreamingLevel->LoadedLevel->Model );
			}
		}
	}
}

void FDragTool_BoxSelect::AddDelta( const FVector& InDelta )
{
	FDragTool::AddDelta( InDelta );

	const UBOOL bUseHoverFeedback = GEditor != NULL && GEditor->GetUserSettings().bEnableViewportHoverFeedback;

	if( bUseHoverFeedback )
	{
		const UBOOL bStrictDragSelection = GEditor->GetUserSettings().bStrictBoxSelection;

		// If we are using over feedback calculate a new box from the one being dragged
		FBox SelBBox;
		CalculateBox( SelBBox );

		// Check every actor to see if it intersects the frustum created by the box
		// If it does, the actor will be selected and should be given a hover cue
		UBOOL bSelectionChanged = FALSE;
		for( FActorIterator It; It; ++It )
		{
			AActor& Actor = **It;
			const UBOOL bActorHitByBox = IntersectsBox( Actor, SelBBox, bStrictDragSelection );

			if( bActorHitByBox )
			{
				// Apply a hover effect to any actor that will be selected
				AddHoverEffect( Actor );
			}
			else
			{
				// Remove any hover effect on this actor as it no longer will be selected by the current box
				RemoveHoverEffect( Actor );
			}
		}

		// Check each model to see if it will be selected
		for( INT ModelIndex = 0; ModelIndex < ModelsToCheck.Num(); ++ModelIndex )
		{
			UModel& Model = *ModelsToCheck(ModelIndex);
			for (INT NodeIndex = 0; NodeIndex < Model.Nodes.Num(); NodeIndex++)
			{
				if( IntersectsBox( Model, NodeIndex, SelBBox, bStrictDragSelection ) )
				{
					// Apply a hover effect to any bsp surface that will be selected
					AddHoverEffect( Model, Model.Nodes(NodeIndex).iSurf );
				}
				else
				{
					// Remove any hover effect on this bsp surface as it no longer will be selected by the current box
					RemoveHoverEffect( Model, Model.Nodes(NodeIndex).iSurf );
				}
			}
		}
	}
}
/**
* Ends a mouse drag behavior (the user has let go of the mouse button).
*/
void FDragTool_BoxSelect::EndDrag()
{
	FEditorModeTools& EdModeTools = GEditorModeTools();
	const UBOOL bGeometryMode = EdModeTools.IsModeActive( EM_Geometry );

	FBox SelBBox;
	CalculateBox( SelBBox );

	// If the user is selecting, but isn't hold down SHIFT, remove all current selections.
	if( bLeftMouseButtonDown && !bShiftDown )
	{
		GEditorModeTools().SelectNone();
	}

	// Let the editor mode try to handle the box selection.
	const UBOOL bEditorModeHandledBoxSelection = GEditorModeTools().BoxSelect( SelBBox, bLeftMouseButtonDown );

	// If the edit mode didn't handle the selection, try normal actor box selection.
	if ( !bEditorModeHandledBoxSelection )
	{
		const UBOOL bStrictDragSelection = GEditor->GetUserSettings().bStrictBoxSelection;

		// If the user is selecting, but isn't hold down SHIFT, remove all current selections.
		if( bLeftMouseButtonDown && !bShiftDown )
		{
			GEditor->SelectNone( TRUE, TRUE );
		}

		// Select all actors that are within the selection box area.  Be aware that certain modes do special processing below.	
		UBOOL bSelectionChanged = FALSE;
		for( FActorIterator It; It; ++It )
		{
			AActor* Actor = *It;
			
			// Select the actor if we need to
			if( IntersectsBox( *Actor, SelBBox, bStrictDragSelection ) )
			{
				GEditor->SelectActor( Actor, bLeftMouseButtonDown, ViewportClient, FALSE );
				bSelectionChanged = TRUE;
			}
		}

		// Check every model to see if its BSP surfaces should be selected
		for( INT ModelIndex = 0; ModelIndex < ModelsToCheck.Num(); ++ModelIndex )
		{
			UModel& Model = *ModelsToCheck(ModelIndex);
			// Check every node in the model
			for (INT NodeIndex = 0; NodeIndex < Model.Nodes.Num(); NodeIndex++)
			{
				if( IntersectsBox( Model, NodeIndex, SelBBox, bStrictDragSelection ) )
				{
					// If the node intersected the frustum select the corresponding surface
					GEditor->SelectBSPSurf( &Model, Model.Nodes(NodeIndex).iSurf, TRUE, FALSE );
					bSelectionChanged = TRUE;
				}
			}
		}

		if ( bSelectionChanged )
		{
			// If any selections were made.  Notify that now.
			GEditor->NoteSelectionChange();
		}
	}

	// Clear any hovered objects that might have been created while dragging
	FEditorLevelViewportClient::ClearHoverFromObjects();

	// Clean up.
	FDragTool::EndDrag();
}

void FDragTool_BoxSelect::Render3D(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	DrawWireBox( PDI, FBox( Start, End ), FColor(255,0,0), SDPG_Foreground );
}

void FDragTool_BoxSelect::CalculateBox( FBox& OutBox )
{
	// Create a bounding box based on the start/end points (normalizes the points).
	OutBox.Init();
	OutBox += Start;
	OutBox += End;

	switch(ViewportClient->ViewportType)
	{
	case LVT_OrthoXY:
		OutBox.Min.Z = -WORLD_MAX;
		OutBox.Max.Z = WORLD_MAX;
		break;
	case LVT_OrthoXZ:
		OutBox.Min.Y = -WORLD_MAX;
		OutBox.Max.Y = WORLD_MAX;
		break;
	case LVT_OrthoYZ:
		OutBox.Min.X = -WORLD_MAX;
		OutBox.Max.X = WORLD_MAX;
		break;
	}
}


/** 
 * Returns TRUE if the passed in Actor intersects with the provided box 
 *
 * @param InActor				The actor to check
 * @param InBox					The box to check against
 * @param bUseStrictSelection	TRUE if the actor must be entirely within the frustum
 */
UBOOL FDragTool_BoxSelect::IntersectsBox( AActor& InActor, const FBox& InBox, UBOOL bUseStrictSelection )
{
	UBOOL bActorHitByBox = FALSE;

	FEditorModeTools& EdModeTools = GEditorModeTools();
	const UBOOL bGeometryMode = EdModeTools.IsModeActive( EM_Geometry );

	// Check for special cases (like certain show flags that might hide an actor)
	UBOOL bActorIsHiddenByShowFlags = FALSE;

	if( InActor.IsA(ANavigationPoint::StaticClass()) && !(ViewportClient->ShowFlags&SHOW_NavigationNodes) )
	{
		bActorIsHiddenByShowFlags = TRUE;
	}
	// Check to see that volume actors are visible in the viewport
	else if( InActor.IsAVolume() && (!(ViewportClient->ShowFlags&SHOW_Volumes) || !ViewportClient->IsVolumeVisibleInViewport(InActor) ) )
	{
		bActorIsHiddenByShowFlags = TRUE;
	}

	// Never drag-select hidden actors or builder brushes. Also, don't consider actors which haven't been recently rendered.
	//@TODO - replace with proper check for if this object was visible last frame.  This is viewport dependent and viewports can use different concepts of time
	//depending on if they are in "realtime" mode or not.  See FEditorLevelViewportClient::Draw for the differing concepts of time.
	const UBOOL bActorRecentlyRendered = true;//Actor->LastRenderTime > ( GWorld->GetTimeSeconds() - 1.0f );

	if( !bActorIsHiddenByShowFlags && !InActor.IsHiddenEd() && !InActor.IsABuilderBrush() && bActorRecentlyRendered )
	{
		// Skeletal meshes.
		APawn* Pawn = Cast<APawn>( &InActor );
		if( Pawn && Pawn->Mesh )
		{
			if( ViewportClient->ComponentIsTouchingSelectionBox( &InActor, Pawn->Mesh, InBox, bGeometryMode, bUseStrictSelection ) )
			{
				bActorHitByBox = TRUE;
			}
		}
		else
		{
			// Iterate over all actor components, selecting out primitive components
			for( INT ComponentIndex = 0 ; ComponentIndex < InActor.Components.Num() ; ++ComponentIndex )
			{
				UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>( InActor.Components(ComponentIndex) );
				if ( PrimitiveComponent && !PrimitiveComponent->HiddenEditor )
				{
					if ( ViewportClient->ComponentIsTouchingSelectionBox( &InActor, PrimitiveComponent, InBox, bGeometryMode, bUseStrictSelection ) )
					{
						bActorHitByBox = TRUE;
						break;
					}
				}
			}
		}
	}

	return bActorHitByBox;
}

/** 
 * Returns TRUE if the provided BSP node intersects with the provided frustum 
 *
 * @param InModel				The model containing BSP nodes to check
 * @param NodeIndex				The index to a BSP node in the model.  This node is used for the bounds check.
 * @param InFrustum				The frustum to check against.
 * @param bUseStrictSelection	TRUE if the node must be entirely within the frustum
 */
UBOOL FDragTool_BoxSelect::IntersectsBox( const UModel& InModel, INT NodeIndex, const FBox& InBox, UBOOL bUseStrictSelection ) const
{
	FBox NodeBB;
	InModel.GetNodeBoundingBox( InModel.Nodes(NodeIndex), NodeBB );

	UBOOL bFullyContained = FALSE;
	UBOOL bIntersects = FALSE;
	if( !bUseStrictSelection )
	{
		bIntersects = InBox.Intersect( NodeBB );
	}
	else
	{
		bIntersects = InBox.IsInside( NodeBB.Max ) && InBox.IsInside( NodeBB.Min );
	}

	return bIntersects;
}

/** Adds a hover effect to the passed in actor */
void FDragTool_BoxSelect::AddHoverEffect( AActor& InActor )
{
	FEditorLevelViewportClient::FViewportHoverTarget HoverTarget( &InActor );
	FEditorLevelViewportClient::AddHoverEffect( HoverTarget );
	FEditorLevelViewportClient::HoveredObjects.Add( HoverTarget );
}

/** Removes a hover effect from the passed in actor */
void FDragTool_BoxSelect::RemoveHoverEffect( AActor& InActor  )
{
	FEditorLevelViewportClient::FViewportHoverTarget HoverTarget( &InActor );
	FSetElementId Id = FEditorLevelViewportClient::HoveredObjects.FindId( HoverTarget );
	if( Id.IsValidId() )
	{
		FEditorLevelViewportClient::RemoveHoverEffect( HoverTarget );
		FEditorLevelViewportClient::HoveredObjects.Remove( Id );
	}
}

/** Adds a hover effect to the passed in bsp surface */
void FDragTool_BoxSelect::AddHoverEffect( UModel& InModel, INT SurfIndex )
{
	FEditorLevelViewportClient::FViewportHoverTarget HoverTarget( &InModel, SurfIndex );
	FEditorLevelViewportClient::AddHoverEffect( HoverTarget );
	FEditorLevelViewportClient::HoveredObjects.Add( HoverTarget );
}

/** Removes a hover effect from the passed in bsp surface */
void FDragTool_BoxSelect::RemoveHoverEffect( UModel& InModel, INT SurfIndex )
{
	FEditorLevelViewportClient::FViewportHoverTarget HoverTarget( &InModel, SurfIndex );
	FSetElementId Id = FEditorLevelViewportClient::HoveredObjects.FindId( HoverTarget );
	if( Id.IsValidId() )
	{
		FEditorLevelViewportClient::RemoveHoverEffect( HoverTarget );
		FEditorLevelViewportClient::HoveredObjects.Remove( Id );
	}
}
