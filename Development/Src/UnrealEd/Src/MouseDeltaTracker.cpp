/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "MouseDeltaTracker.h"
#include "DragTool_BoxSelect.h"
#include "DragTool_Measure.h"
#include "DragTool_FrustumSelect.h"
#include "ScopedTransaction.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FMouseDeltaTracker
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FMouseDeltaTracker::FMouseDeltaTracker()
	:	Start( FVector(0,0,0) )
	,	StartSnapped( FVector(0,0,0) )
	,	StartScreen( FVector(0,0,0) )
	,	End( FVector(0,0,0) )
	,	EndSnapped( FVector(0,0,0) )
	,	EndScreen( FVector(0,0,0) )
	,   ReductionAmount( FVector(0,0,0) )
	,	DragTool( NULL )
	,	TransCount( 0 )
	,	ScopedTransaction( NULL )
	,	bTrackingTransactionStarted(FALSE)
	,	bSelectedBuilderBrush(FALSE)
{
}

FMouseDeltaTracker::~FMouseDeltaTracker()
{
	// End potentially outstnading transaction.
	EndTransaction();
}

/**
 * Sets the current axis of the widget for the specified viewport.
 *
 * @param	InViewportClient		The viewport whose widget axis is to be set.
 */
void FMouseDeltaTracker::DetermineCurrentAxis(FEditorLevelViewportClient* InViewportClient)
{
	const UBOOL TabDown = InViewportClient->Input->IsPressed( KEY_Tab );
	const UBOOL AltDown = InViewportClient->Input->IsAltPressed();
	const UBOOL ShiftDown = InViewportClient->Input->IsShiftPressed();
	const UBOOL ControlDown = InViewportClient->Input->IsCtrlPressed();
	const UBOOL LeftMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_LeftMouseButton);
	const UBOOL RightMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_RightMouseButton);
	const UBOOL MiddleMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_MiddleMouseButton);

	// CTRL/ALT + LEFT/RIGHT mouse button acts the same as dragging the most appropriate widget handle.
	if( ((TabDown && !AltDown && ! ControlDown) || (AltDown && !ControlDown && !TabDown) || (!AltDown && ControlDown && !TabDown)) &&
		(LeftMouseButtonDown || RightMouseButtonDown)
		)
	{
		// Only try to pick an axis if we're not dragging by widget handle.
		if ( InViewportClient->Widget->GetCurrentAxis() == AXIS_None )
		{
			InViewportClient->bWidgetAxisControlledByDrag = TRUE;

			switch( GEditorModeTools().GetWidgetMode() )
			{
				case FWidget::WM_Scale:
					InViewportClient->Widget->SetCurrentAxis( AXIS_XYZ );
					break;

				case FWidget::WM_Translate:
				case FWidget::WM_TranslateRotateZ:
				case FWidget::WM_ScaleNonUniform:
					switch( InViewportClient->ViewportType )
					{
						case LVT_Perspective:
							if( LeftMouseButtonDown && !RightMouseButtonDown )
							{
								InViewportClient->Widget->SetCurrentAxis( AXIS_X );
							}
							else if( !LeftMouseButtonDown && RightMouseButtonDown )
							{
								InViewportClient->Widget->SetCurrentAxis( AXIS_Y );
							}
							else if( LeftMouseButtonDown && RightMouseButtonDown )
							{
								InViewportClient->Widget->SetCurrentAxis( AXIS_Z );
							}
							break;
						case LVT_OrthoXY:
							InViewportClient->Widget->SetCurrentAxis( AXIS_XY );
							break;
						case LVT_OrthoXZ:
							InViewportClient->Widget->SetCurrentAxis( AXIS_XZ );
							break;
						case LVT_OrthoYZ:
							InViewportClient->Widget->SetCurrentAxis( AXIS_YZ );
							break;
						default:
							break;
					}
				break;

				case FWidget::WM_Rotate:
					switch( InViewportClient->ViewportType )
					{
						case LVT_Perspective:
							if (LeftMouseButtonDown && !RightMouseButtonDown)
							{
								InViewportClient->Widget->SetCurrentAxis(AXIS_X);
							}
							else if (!LeftMouseButtonDown && RightMouseButtonDown)
							{
								InViewportClient->Widget->SetCurrentAxis(AXIS_Y);
							}
							else if (LeftMouseButtonDown && RightMouseButtonDown)
							{
								InViewportClient->Widget->SetCurrentAxis(AXIS_Z);
							}
							break;
						case LVT_OrthoXY:
							InViewportClient->Widget->SetCurrentAxis( AXIS_Z );
							break;
						case LVT_OrthoXZ:
							InViewportClient->Widget->SetCurrentAxis( AXIS_Y );
							break;
						case LVT_OrthoYZ:
							InViewportClient->Widget->SetCurrentAxis( AXIS_X );
							break;
						default:
							break;
					}
				break;
			
				default:
					break;
			}
		}
	}
}

/**
 * Initiates a transaction.
 */
void FMouseDeltaTracker::BeginTransaction(const TCHAR* SessionName)
{
	EndTransaction();
	ScopedTransaction = new FScopedTransaction( SessionName );
}

/**
 * Ends the current transaction, if one exists.
 */
void FMouseDeltaTracker::EndTransaction()
{
	delete ScopedTransaction;
	ScopedTransaction = NULL;
}

/**
 * Begin tracking at the specified location for the specified viewport.
 */
void FMouseDeltaTracker::StartTracking(FEditorLevelViewportClient* InViewportClient, const INT InX, const INT InY, UBOOL bArrowMovement /* = FALSE */)
{
	// If the builder brush was selected and is still selected we don't want to deselect it due to the possibility that you can get two mouse down
	// events on the same viewport before you get a mouse up event.
	UBOOL bNoActorSelected = !GEditor->GetSelectedActors()->GetTop<AActor>();
	if(bSelectedBuilderBrush && (bNoActorSelected || !GEditor->GetSelectedActors()->GetTop<AActor>()->IsABrush()))
	{
		bSelectedBuilderBrush = FALSE;
	}

	bTrackingTransactionStarted = FALSE;
	DetermineCurrentAxis( InViewportClient );

	const UBOOL AltDown = InViewportClient->Input->IsAltPressed();
	const UBOOL ShiftDown = InViewportClient->Input->IsShiftPressed();
	const UBOOL ControlDown = InViewportClient->Input->IsCtrlPressed();
	const UBOOL LeftMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_LeftMouseButton);
	const UBOOL RightMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_RightMouseButton);
	const UBOOL MiddleMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_MiddleMouseButton);

	// Begin transacting.  Give the current editor mode an opportunity to do the transacting.
	const UBOOL bTransactingHandledByEditorMode = GEditorModeTools().StartTracking();

	UBOOL bIsDragging =
		((ControlDown || ShiftDown) && (LeftMouseButtonDown || RightMouseButtonDown || MiddleMouseButtonDown)) ||
		 (InViewportClient->Widget->GetCurrentAxis() != AXIS_None) ||
		 bArrowMovement;

	if (InViewportClient->Widget)
	{
		InViewportClient->Widget->SetDragging (bIsDragging);
		if (GEditorModeTools().GetWidgetMode() == FWidget::WM_Rotate)
		{
			InViewportClient->Invalidate();
		}

		// If any actor in the selection requires snapping, they all need to be snapped.
		UBOOL bNeedMovementSnapping = FALSE;
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );
			if ( Actor->bEdShouldSnap )
			{
				bNeedMovementSnapping = TRUE;
				break;
			}
		}
		InViewportClient->Widget->SetSnapEnabled(bNeedMovementSnapping);
	}

	// Clear bool that tracks whether AddDelta has been called
	bHasReceivedAddDelta = FALSE;

	if( !bTransactingHandledByEditorMode )
	{
		if( bIsDragging )
		{
			TransCount++;

			switch( GEditorModeTools().GetWidgetMode() )
			{
				case FWidget::WM_Translate:
					BeginTransaction( TEXT("Move Actors (FMDT::ST)") );
					bTrackingTransactionStarted = TRUE;
					break;
				case FWidget::WM_Rotate:
					BeginTransaction( TEXT("Rotate Actors (FMDT::ST)") );
					bTrackingTransactionStarted = TRUE;
					break;
				case FWidget::WM_Scale:
					BeginTransaction( TEXT("Scale Actors (FMDT::ST)") );
					bTrackingTransactionStarted = TRUE;
					break;
				case FWidget::WM_ScaleNonUniform:
					BeginTransaction( TEXT("Non-Uniform Scale Actors (FMDT::ST)") );
					bTrackingTransactionStarted = TRUE;
					break;
				case FWidget::WM_TranslateRotateZ:
					BeginTransaction( TEXT("Translate/RotateZ Actors (FMDT::ST)") );
					bTrackingTransactionStarted = TRUE;
					break;
				default:
					if(bArrowMovement)
					{
						BeginTransaction( TEXT("Move Actors (FMDT::ST)") );
						bTrackingTransactionStarted = TRUE;
					}
					break;
			}
		}
	}

	UBOOL bShouldSelectBuilder = bIsDragging && !AltDown && ControlDown;
	if ( bShouldSelectBuilder )
	{
		// If there are no actors selected, select the builder brush
		if( bNoActorSelected )
		{
			// Instigate a transaction if one hasn't already been.
			bSelectedBuilderBrush = TRUE;
			bTrackingTransactionStarted = TRUE;
			BeginTransaction( TEXT("Select Builder Brush (FMDT::ST)") );
			GEditor->GetSelectedActors()->Modify();
			GEditor->GetSelectedActors()->Select( GWorld->GetBrush() );	
			// We need to note the selection change so that the pivot location will be updated to the newly selected builder.
			GEditor->NoteSelectionChange();
		}
		else
		{
			GEditor->GetSelectedActors()->Modify();
		}
	}

	check( DragTool == NULL );

	// Create a drag tool.  Note that drag tools aren't used in the perspective viewport.
	if( InViewportClient->IsOrtho() )
	{
		// (ALT+CTRL) + LEFT/RIGHT mouse button invokes box selections
		if( (LeftMouseButtonDown || RightMouseButtonDown) && (AltDown && ControlDown) )
		{
			DragTool = new FDragTool_BoxSelect();
		}

		// MIDDLE mouse button invokes measuring tool
		if( !(ControlDown+AltDown+ShiftDown) && MiddleMouseButtonDown )
		{
			DragTool = new FDragTool_Measure();
		}
	}
	else
	{
		// (ALT+CTRL) + LEFT/RIGHT mouse button invokes box selections
		if( (LeftMouseButtonDown || RightMouseButtonDown) && (AltDown && ControlDown) )
		{
			DragTool = new FDragTool_FrustumSelect();
		}
	}


	StartSnapped = Start = StartScreen = FVector( InX, InY, 0 );

	if( DragTool )
	{
		// If a drag tool is active, let it handle snapping as it likes.
		DragTool->StartDrag( InViewportClient, GEditor->ClickLocation );
		
	}
	else
	{
		// No drag tool is active, so handle snapping.
		switch( GEditorModeTools().GetWidgetMode() )
		{
			case FWidget::WM_Translate:
				GEditor->Constraints.Snap( StartSnapped, FVector(GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize()) );
				break;

			case FWidget::WM_Scale:
			case FWidget::WM_ScaleNonUniform:
				GEditor->Constraints.SnapScale( StartSnapped, FVector(GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize()) );
				break;

			case FWidget::WM_Rotate:
			{
				FRotator rot( StartSnapped.X, StartSnapped.Y, StartSnapped.Z );
				GEditor->Constraints.Snap( rot );
				StartSnapped = FVector( rot.Pitch, rot.Yaw, rot.Roll );
			}
			break;
			case FWidget::WM_TranslateRotateZ:
				GEditor->Constraints.Snap( StartSnapped, FVector(GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize()) );
				break;

			default:
				break;
		}
	}

	End = EndScreen = Start;
	EndSnapped = StartSnapped;

	bExternalMovement = FALSE;	//no external movement has occured yet.
	//reset delta angle
	GEditorModeTools().TotalDeltaRotation = 0;
}

/**
 * Called when a mouse button has been released.  If there are no other
 * mouse buttons being held down, the internal information is reset.
 */
UBOOL FMouseDeltaTracker::EndTracking(FEditorLevelViewportClient* InViewportClient)
{
	DetermineCurrentAxis( InViewportClient );

	const UBOOL AltDown = InViewportClient->Input->IsAltPressed();
	const UBOOL ShiftDown = InViewportClient->Input->IsShiftPressed();
	const UBOOL ControlDown = InViewportClient->Input->IsCtrlPressed();
	const UBOOL LeftMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_LeftMouseButton);
	const UBOOL RightMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_RightMouseButton);
	const UBOOL MiddleMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_MiddleMouseButton);

	if (InViewportClient->Widget)
	{
		InViewportClient->Widget->SetDragging (FALSE);
	}

	// here we check to see if anything of worth actually changed when ending our MouseMovement
	// If the TransCount > 0 (we changed something of value) so we need to call PostEditMove() on stuff
	// if we didn't change anything then don't call PostEditMove()
	UBOOL bDidAnythingActuallyChange = FALSE;

	// Stop transacting.  Give the current editor mode an opportunity to do the transacting.
	const UBOOL bTransactingHandledByEditorMode = GEditorModeTools().EndTracking();
	if( !bTransactingHandledByEditorMode )
	{
		if( TransCount > 0 )
		{
			bDidAnythingActuallyChange = TRUE;
			TransCount--;
		}
	}

	// Notify the selected actors that they have been moved.
	// Don't do this if AddDelta was never called.
	if( bDidAnythingActuallyChange && bHasReceivedAddDelta )
	{
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );
			Actor->PostEditMove( TRUE );
		}
	}

	// End the transaction here if one was started in StartTransaction()
	if(bTrackingTransactionStarted)
	{
		EndTransaction();
		bTrackingTransactionStarted = FALSE;
	}

	TArray<FEdMode*> ActiveModes; 
	GEditorModeTools().GetActiveModes( ActiveModes );
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		// Also notify the current editing modes if they are interested.
		ActiveModes(ModeIndex)->ActorMoveNotify();
	}

	Start = StartSnapped = StartScreen = End = EndSnapped = EndScreen = FVector(0,0,0);
	ReductionAmount = FVector(0,0,0);

	if( bDidAnythingActuallyChange )
	{
		FScopedLevelDirtied LevelDirtyCallback;
		LevelDirtyCallback.Request();

		GEditor->RedrawLevelEditingViewports();
	}

	//reset delta angle
	GEditorModeTools().TotalDeltaRotation = 0;

	// Delete the drag tool if one exists.
	if( DragTool )
	{
		DragTool->EndDrag();
		delete DragTool;
		DragTool = NULL;
		return FALSE;
	}

	return TRUE;
}

/**
 * Adds delta movement into the tracker.
 */
void FMouseDeltaTracker::AddDelta(FEditorLevelViewportClient* InViewportClient, const FName InKey, const INT InDelta, UBOOL InNudge)
{
	const UBOOL LeftMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_LeftMouseButton);
	const UBOOL RightMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_RightMouseButton);
	const UBOOL MiddleMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_MiddleMouseButton);
	const UBOOL bAltDown = InViewportClient->Input->IsAltPressed();
	const UBOOL bShiftDown = InViewportClient->Input->IsShiftPressed();
	const UBOOL bControlDown = InViewportClient->Input->IsCtrlPressed();
	const UBOOL bTabDown = InViewportClient->Input->IsPressed(KEY_Tab);

	if( !LeftMouseButtonDown && !MiddleMouseButtonDown && !RightMouseButtonDown && !InNudge )
	{
		return;
	}

	// Note that AddDelta has been called since StartTracking
	bHasReceivedAddDelta = TRUE;

	// If we are using a drag tool, the widget isn't involved so set it to having no active axis.  This
	// means we will get unmodified mouse movement returned to us by other functions.

	const EAxis SaveAxis = InViewportClient->Widget->GetCurrentAxis();

	// If the user isn't dragging with the left mouse button, clear out the axis 
	// as the widget only responds to the left mouse button.
	//
	// We allow an exception for dragging with the right mouse button while holding control
	// as that simulates using the rotation widget.

	const UBOOL bUsingAxis = LeftMouseButtonDown || ( (bControlDown || bAltDown || bTabDown) && RightMouseButtonDown);

	if( DragTool || !InViewportClient->bIsTracking || !bUsingAxis )
	{
		InViewportClient->Widget->SetCurrentAxis( AXIS_None );
	}

	FVector Wk = InViewportClient->TranslateDelta( InKey, InDelta, InNudge );

	EndScreen += Wk;

	if( InViewportClient->Widget->GetCurrentAxis() != AXIS_None )
	{
		// Affect input delta by the camera speed

		FWidget::EWidgetMode WidgetMode = GEditorModeTools().GetWidgetMode();
		UBOOL bIsRotation = (WidgetMode == FWidget::WM_Rotate) || ((WidgetMode == FWidget::WM_TranslateRotateZ) && (InViewportClient->Widget->GetCurrentAxis()==AXIS_XY));
		if (bIsRotation)
		{
			Wk *= CAMERA_ROTATION_SPEED;
		}

		// Make rotations occur at the same speed, regardless of ortho zoom

		if( InViewportClient->IsOrtho() )
		{
			if (bIsRotation)
			{
				FLOAT Scale = 1.0f;

				if( InViewportClient->IsOrtho() )
				{
					Scale = DEFAULT_ORTHOZOOM / (FLOAT)InViewportClient->OrthoZoom;
				}

				Wk *= Scale;
			}
		}
		//if Absolute Translation, and not just moving the camera around
		else if (InViewportClient->IsUsingAbsoluteTranslation())
		{
			// Compute a view.
			FSceneViewFamilyContext ViewFamily(InViewportClient->Viewport, InViewportClient->GetScene(),InViewportClient->ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds(), InViewportClient->IsRealtime());
			FSceneView* View = InViewportClient->CalcSceneView( &ViewFamily );

			//calculate mouse position
			check(InViewportClient->Viewport);
			FVector2D MousePosition(InViewportClient->Viewport->GetMouseX(), InViewportClient->Viewport->GetMouseY());
			FVector WidgetPosition = GEditorModeTools().PivotLocation;

			FRotator TempRot;
			FVector TempScale;
			InViewportClient->Widget->AbsoluteTranslationConvertMouseMovementToAxisMovement(View, InViewportClient, WidgetPosition, MousePosition, Wk, TempRot, TempScale );
		}
	}

	End += Wk;
	EndSnapped = End;

	if( DragTool )
	{
		FVector Drag = Wk;
		if( DragTool->bConvertDelta )
		{
			FRotator Rot;
			InViewportClient->ConvertMovementToDragRot( Wk, Drag, Rot );
		}

		if ( InViewportClient->ViewportType == LVT_OrthoXY )
		{
			DragTool->AddDelta( FVector(Drag.Y, -Drag.X, Drag.Z) );
		}
		else if ( InViewportClient->ViewportType == LVT_Perspective )
		{
			if(InKey == KEY_MouseX )
			{
				DragTool->AddDelta( FVector(InDelta, 0.0f, 0.0f ));
			}
			else
			{
				DragTool->AddDelta( FVector(0.0f, -InDelta, 0.0f ));
			}
		}
		else
		{
			DragTool->AddDelta( Drag );
		}
	}
	else
	{
		switch( GEditorModeTools().GetWidgetMode() )
		{
			case FWidget::WM_Translate:
				GEditor->Constraints.Snap( EndSnapped, FVector(GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize()) );
				break;

			case FWidget::WM_Scale:
			case FWidget::WM_ScaleNonUniform:
				GEditor->Constraints.SnapScale( EndSnapped, FVector(GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize()) );
				break;

			case FWidget::WM_Rotate:
			{
				FRotator rot( EndSnapped.X, EndSnapped.Y, EndSnapped.Z );
				GEditor->Constraints.Snap( rot );
				EndSnapped = FVector( rot.Pitch, rot.Yaw, rot.Roll );
			}
			break;
			case FWidget::WM_TranslateRotateZ:
			{
				if (InViewportClient->Widget->GetCurrentAxis() == AXIS_ZROTATION)
				{
					FRotator rot( EndSnapped.X, EndSnapped.Y, EndSnapped.Z );
					GEditor->Constraints.Snap( rot );
					EndSnapped = FVector( rot.Pitch, rot.Yaw, rot.Roll );
				}
				else
				{
					//translation (either xy plane or z)
					GEditor->Constraints.Snap( EndSnapped, FVector(GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize()) );
				}
			}

			default:
				break;
		}
	}

	if( DragTool )
	{
		InViewportClient->Widget->SetCurrentAxis( SaveAxis );
	}
}

/**
 * Returns the current delta.
 */
const FVector FMouseDeltaTracker::GetDelta() const
{
	const FVector Delta( End - Start );
	return Delta;
}

/**
 * Returns the current snapped delta.
 */
const FVector FMouseDeltaTracker::GetDeltaSnapped() const
{
	const FVector SnappedDelta( EndSnapped - StartSnapped );
	return SnappedDelta;
}


/**
* Returns the absolute delta since dragging started.
*/
const FVector FMouseDeltaTracker::GetAbsoluteDelta() const
{
	const FVector Delta( End - Start + ReductionAmount );
	return Delta;
}

/**
* Returns the absolute snapped delta since dragging started. 
*/
const FVector FMouseDeltaTracker::GetAbsoluteDeltaSnapped() const
{
	const FVector SnappedDelta( EndSnapped - StartSnapped + ReductionAmount );
	return SnappedDelta;
}

/**
 * Returns the screen space delta since dragging started.
 */
const FVector FMouseDeltaTracker::GetScreenDelta() const
{
	const FVector Delta( EndScreen - StartScreen );
	return Delta;
}

/**
 * Converts the delta movement to drag/rotation/scale based on the viewport type or widget axis
 */
void FMouseDeltaTracker::ConvertMovementDeltaToDragRot(FEditorLevelViewportClient* InViewportClient, const FVector& InDragDelta, FVector& InDrag, FRotator& InRotation, FVector& InScale)
{
	InDrag = FVector(0,0,0);
	InRotation = FRotator(0,0,0);
	InScale = FVector(0,0,0);

	if( InViewportClient->Widget->GetCurrentAxis() != AXIS_None )
	{
		InViewportClient->Widget->ConvertMouseMovementToAxisMovement( InViewportClient, GEditorModeTools().PivotLocation, InDragDelta, InDrag, InRotation, InScale );
	}
	else
	{
		InViewportClient->ConvertMovementToDragRot( InDragDelta, InDrag, InRotation );
	}
}

/**
 * Absolute Translation conversion from mouse position on the screen to widget axis movement/rotation.
 */
void FMouseDeltaTracker::AbsoluteTranslationConvertMouseToDragRot(FSceneView* InView, FEditorLevelViewportClient* InViewportClient,FVector& InDrag, FRotator& InRotation, FVector& InScale )
{
	InDrag = FVector(0,0,0);
	InRotation = FRotator(0,0,0);
	InScale = FVector(0,0,0);

	check ( InViewportClient->Widget->GetCurrentAxis() != AXIS_None );

	//calculate mouse position
	check(InViewportClient->Viewport);
	FVector2D MousePosition(InViewportClient->Viewport->GetMouseX(), InViewportClient->Viewport->GetMouseY());

	InViewportClient->Widget->AbsoluteTranslationConvertMouseMovementToAxisMovement(InView, InViewportClient, GEditorModeTools().PivotLocation, MousePosition, InDrag, InRotation, InScale );
}

/**
 * Subtracts the specified value from End and EndSnapped.
 */
void FMouseDeltaTracker::ReduceBy(const FVector& In)
{
	End -= In;
	EndSnapped -= In;
	ReductionAmount += In;
}

/**
 * @return		TRUE if a drag tool is being used by the tracker, FALSE otherwise.
 */
UBOOL FMouseDeltaTracker::UsingDragTool() const
{
	return DragTool != NULL;
}

/**
 * Renders the drag tool.  Does nothing if no drag tool exists.
 */
void FMouseDeltaTracker::Render3DDragTool(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	if ( DragTool )
	{
		DragTool->Render3D( View, PDI );
	}
}

/**
 * Renders the drag tool.  Does nothing if no drag tool exists.
 */
void FMouseDeltaTracker::RenderDragTool(const FSceneView* View,FCanvas* Canvas)
{
	if ( DragTool )
	{
		DragTool->Render( View, Canvas );
	}
}

