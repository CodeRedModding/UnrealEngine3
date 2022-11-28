
#include "EditorPrivate.h"

FDragTool::FDragTool()
{
	Start = End = EndWk = FVector(0,0,0);
	bUseSnapping = 0;
	bConvertDelta = 1;
}

FDragTool::~FDragTool()
{
}

/**
 * Starts a mouse drag behavior.
 *
 * @param InStart		Where the mouse was when the drag started
 */

void FDragTool::StartDrag( FEditorLevelViewportClient* InViewportClient, const FVector& InStart )
{
	ViewportClient = InViewportClient;
	Start = End = EndWk = InStart;

	if( bUseSnapping )
	{
		GEditor->Constraints.Snap( Start, FVector( GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize() ) );
		End = EndWk = Start;
	}

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

/**
 * Updates the drag tool with the current drag start/end locations.
 *
 * @param InDelta		A delta of mouse movement
 */

void FDragTool::AddDelta( const FVector& InDelta )
{
	EndWk += InDelta;

	End = EndWk;

	if( bUseSnapping )
	{
		GEditor->Constraints.Snap( End, FVector(GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize()) );
	}
}

// -----------------------------------------------------------------------------

FDragTool_BoxSelect::FDragTool_BoxSelect()
{
}

FDragTool_BoxSelect::~FDragTool_BoxSelect()
{
}

void FDragTool_BoxSelect::EndDrag()
{
	// If the user is selecting, but isn't hold down SHIFT, remove all current selections.

	if( bLeftMouseButtonDown && !bShiftDown )
	{
		switch( GEditorModeTools.GetCurrentModeID() )
		{
			case EM_Geometry:
				((FEdModeGeometry*)GEditorModeTools.GetCurrentMode())->SelectNone();
				break;

			default:
				GEditor->SelectNone( GEditor->GetLevel(), 1 );
				break;
		}
	}

	// Create a bounding box based on the start/end points (normalizes the points).

	FBox SelBBox(0);
	SelBBox += Start;
	SelBBox += End;

	switch(ViewportClient->ViewportType)
	{
		case LVT_OrthoXY:
			SelBBox.Min.Z = -WORLD_MAX;
			SelBBox.Max.Z = WORLD_MAX;
			break;
		case LVT_OrthoXZ:
			SelBBox.Min.Y = -WORLD_MAX;
			SelBBox.Max.Y = WORLD_MAX;
			break;
		case LVT_OrthoYZ:
			SelBBox.Min.X = -WORLD_MAX;
			SelBBox.Max.X = WORLD_MAX;
			break;
	}

	// Select all actors that are within the selection box area.  Be aware that certain modes do special processing below.
	
	for( INT i = 0 ; i < GEditor->Level->Actors.Num() ; i++ )
	{
		AActor* Actor = GEditor->Level->Actors(i);

		if(!Actor)
			continue;

		UBOOL	bActorHitByBox = 0;

		switch( GEditorModeTools.GetCurrentModeID() )
		{
			case EM_Geometry:
			{
				GEditorModeTools.GetCurrentMode()->BoxSelect( SelBBox, bLeftMouseButtonDown );
			}
			break;

			default:
			{
				// Skeletal meshes

				if( Cast<APawn>( Actor ) && Cast<APawn>( Actor )->Mesh )
				{
					if( ViewportClient->ComponentIsTouchingSelectionBox( Actor, Cast<APawn>( Actor )->Mesh, SelBBox ) )
					{
						GEditor->SelectActor( GEditor->Level,Actor,1,0);
						bActorHitByBox = 1;
					}
				}

				for( UINT ComponentIndex = 0 ; !bActorHitByBox && ComponentIndex < (UINT)Actor->Components.Num() ; ComponentIndex++ )
				{
					UPrimitiveComponent*	PrimitiveComponent = Cast<UPrimitiveComponent>(Actor->Components(ComponentIndex));

					if(!PrimitiveComponent)
						continue;

					if(PrimitiveComponent->HiddenEditor)
						continue;

					// Brushes are selected if any of their vertices fall within the selection box

					if(PrimitiveComponent->IsA(UBrushComponent::StaticClass()))
					{
						UBrushComponent*	BrushComponent = Cast<UBrushComponent>(PrimitiveComponent);

						if(BrushComponent->Brush && BrushComponent->Brush->Polys)
						{
							for( UINT PolyIndex = 0 ; !bActorHitByBox && PolyIndex < (UINT)BrushComponent->Brush->Polys->Element.Num() ; PolyIndex++ )
							{
								FPoly&	Poly = BrushComponent->Brush->Polys->Element(PolyIndex);

								for( UINT VertexIndex = 0 ; VertexIndex < (UINT)Poly.NumVertices ; VertexIndex++ )
								{
									FVector Location = PrimitiveComponent->LocalToWorld.TransformFVector(Poly.Vertex[VertexIndex]);

									if(FPointBoxIntersection(Location,SelBBox))
									{
										bActorHitByBox = 1;
										break;
									}
								}
							}
						}
					}
					else
						if( Actor != GEditor->Level->Brush() )	// Never select the builder brush
							bActorHitByBox = ViewportClient->ComponentIsTouchingSelectionBox( Actor, PrimitiveComponent, SelBBox );
				}

				// If the actor couldn't be selected through any of it's components, try against it's location
				// Note: we don't select brushes by their locations. only by their vertices.

				if( !bActorHitByBox && !Actor->IsABrush() )
					if( SelBBox.IsInside( Actor->Location ) )
						bActorHitByBox = 1;	
			}
			break;
		}

		// Select the actor if we need to

		if( bActorHitByBox )
		{
			if( bLeftMouseButtonDown )
			{
				GEditor->SelectActor(GEditor->Level,Actor,1,0);
			}
			else
			{
				GEditor->SelectActor(GEditor->Level,Actor,0,0);
			}
		}
	}

	GEditor->NoteSelectionChange( GEditor->GetLevel() );

	// Clean up

	FDragTool::EndDrag();
}

void FDragTool_BoxSelect::RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	PRI->DrawWireBox( FBox( Start, End ), FColor(255,0,0) );
}

// -----------------------------------------------------------------------------

FDragTool_Measure::FDragTool_Measure()
{
	bUseSnapping = 1;
	bConvertDelta = 0;
}

FDragTool_Measure::~FDragTool_Measure()
{
}

void FDragTool_Measure::RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	PRI->DrawLine( Start, End, FColor(255,0,0) );
}

void FDragTool_Measure::Render(const FSceneContext& Context,FRenderInterface* RI)
{
	FVector Mid = Start + ((End - Start) / 2);
	INT dist = appCeil((End - Start).Size());
	if( dist == 0 )
	{
		return;
	}

	INT X, Y;
	Context.Project( Mid, X, Y );
	RI->DrawStringCentered( X, Y, *FString::Printf(TEXT("%d"),dist), GEngine->SmallFont, FColor(255,255,255) );
}

// -----------------------------------------------------------------------------

FMouseDeltaTracker::FMouseDeltaTracker()
{
	DragTool = NULL;
	bIsTransacting = 0;
	Start = FVector(0,0,0);
	StartSnapped = FVector(0,0,0);
	End = FVector(0,0,0);
	EndSnapped = FVector(0,0,0);
}

FMouseDeltaTracker::~FMouseDeltaTracker()
{
}

void FMouseDeltaTracker::DetermineCurrentAxis( FEditorLevelViewportClient* InViewportClient )
{
	UBOOL AltDown = InViewportClient->Input->IsAltPressed(),
		ShiftDown = InViewportClient->Input->IsShiftPressed(),
		ControlDown = InViewportClient->Input->IsCtrlPressed(),
		LeftMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_LeftMouseButton),
		RightMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_RightMouseButton),
		MiddleMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_MiddleMouseButton);

	// CTRL + LEFT/RIGHT mouse button acts the same as dragging the most appropriate widget handle

	if( !AltDown && ControlDown && ( LeftMouseButtonDown || RightMouseButtonDown ) )
	{
		switch( GEditorModeTools.GetWidgetMode() )
		{
			case WM_Scale:
				InViewportClient->Widget->CurrentAxis = AXIS_XYZ;
				break;

			case WM_Translate:
			case WM_ScaleNonUniform:
				switch( InViewportClient->ViewportType )
				{
					case LVT_Perspective:
						if( LeftMouseButtonDown && !RightMouseButtonDown )
						{
							InViewportClient->Widget->CurrentAxis = AXIS_X;
						}
						else if( !LeftMouseButtonDown && RightMouseButtonDown )
						{
							InViewportClient->Widget->CurrentAxis = AXIS_Y;
						}
						else if( LeftMouseButtonDown && RightMouseButtonDown )
						{
							InViewportClient->Widget->CurrentAxis = AXIS_Z;
						}
						break;
					case LVT_OrthoXY:
						InViewportClient->Widget->CurrentAxis = AXIS_XY;
						break;
					case LVT_OrthoXZ:
						InViewportClient->Widget->CurrentAxis = AXIS_XZ;
						break;
					case LVT_OrthoYZ:
						InViewportClient->Widget->CurrentAxis = AXIS_YZ;
						break;
					default:
						break;
				}
			break;

			case WM_Rotate:
				switch( InViewportClient->ViewportType )
				{
					case LVT_Perspective:
					case LVT_OrthoXY:
						InViewportClient->Widget->CurrentAxis = AXIS_Z;
						break;
					case LVT_OrthoXZ:
						InViewportClient->Widget->CurrentAxis = AXIS_Y;
						break;
					case LVT_OrthoYZ:
						InViewportClient->Widget->CurrentAxis = AXIS_X;
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

/**
 * Called when a mouse drag is starting.
 */

void FMouseDeltaTracker::StartTracking( FEditorLevelViewportClient* InViewportClient, const INT InX, const INT InY )
{
	DetermineCurrentAxis( InViewportClient );

	UBOOL AltDown = InViewportClient->Input->IsAltPressed(),
		ShiftDown = InViewportClient->Input->IsShiftPressed(),
		ControlDown = InViewportClient->Input->IsCtrlPressed(),
		LeftMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_LeftMouseButton),
		RightMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_RightMouseButton),
		MiddleMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_MiddleMouseButton);

	if( !GEditor->Trans->IsActive() )
	{
		if( InViewportClient->Widget->CurrentAxis != AXIS_None )
		{
			if( !GEditorModeTools.GetCurrentMode()->StartTracking() )
			{
				if( !GEditor->Trans->IsActive() )
				{
					switch( GEditorModeTools.GetWidgetMode() )
					{
						case WM_Translate:
							GEditor->Trans->Begin( TEXT("Move Actors") );
							break;
						case WM_Rotate:
							GEditor->Trans->Begin( TEXT("Rotate Actors") );
							break;
						case WM_Scale:
							GEditor->Trans->Begin( TEXT("Scale Actors") );
							break;
						case WM_ScaleNonUniform:
							GEditor->Trans->Begin( TEXT("Non-Uniform Scale Actors") );
							break;
						default:
							break;
					}

					bIsTransacting = 1;
				}
		
				// If there are no actors selected, select the builder brush

				if( !GSelectionTools.GetTop<AActor>() )
				{
					GSelectionTools.Select( GEditor->GetLevel()->Brush() );
				}

				for( TSelectedActorIterator It( GEditor->GetLevel() ) ; It ; ++It )
				{
					It->Modify();

					ABrush* brush = Cast<ABrush>( *It );
					if( brush && brush->Brush)
					{
						brush->Brush->Modify();
						UPolys* Polys = brush->Brush->Polys;
						Polys->Element.ModifyAllItems();
					}
				}
			}
		}
	}

	// Perspective window doesn't handle drag tools properly

	if( InViewportClient->ViewportType != LVT_Perspective )
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

	StartSnapped = Start = FVector( InX, InY, 0 );

	if( DragTool )
	{
		DragTool->StartDrag( InViewportClient, GEditor->ClickLocation );
	}
	else
	{
		switch( GEditorModeTools.GetWidgetMode() )
		{
			case WM_Translate:
			case WM_Scale:
			case WM_ScaleNonUniform:
				GEditor->Constraints.Snap( StartSnapped, FVector(GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize()) );
				break;

			case WM_Rotate:
			{
				FRotator rot( StartSnapped.X, StartSnapped.Y, StartSnapped.Z );
				GEditor->Constraints.Snap( rot );
				StartSnapped = FVector( rot.Pitch, rot.Yaw, rot.Roll );
			}
			break;

			default:
				break;
		}
	}

	End = Start;
	EndSnapped = StartSnapped;
}

/**
 * Called when a mouse button has been released.  If there are no other
 * mouse buttons being held down, the internal information is reset.
 */

UBOOL FMouseDeltaTracker::EndTracking( FEditorLevelViewportClient* InViewportClient )
{
	DetermineCurrentAxis( InViewportClient );

	UBOOL AltDown = InViewportClient->Input->IsAltPressed(),
		ShiftDown = InViewportClient->Input->IsShiftPressed(),
		ControlDown = InViewportClient->Input->IsCtrlPressed(),
		LeftMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_LeftMouseButton),
		RightMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_RightMouseButton),
		MiddleMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_MiddleMouseButton);

	// If anything is still being held down, just leave.

	if( LeftMouseButtonDown || RightMouseButtonDown || MiddleMouseButtonDown )
	{
		return 0;
	}

	// Stop transacting

	if( InViewportClient->Widget->CurrentAxis != AXIS_None )
	{
		if( !GEditorModeTools.GetCurrentMode()->EndTracking() )
		{
			if( bIsTransacting )
			{
				GEditor->Trans->End();
				bIsTransacting = 0;

				// Notify the selected actors that they have been moved.
				for( TSelectedActorIterator It( GEditor->GetLevel() ) ; It ; ++It )
				{
					It->PostEditMove();
				}

				GEditorModeTools.GetCurrentMode()->ActorMoveNotify();

				// Draw the scene to show the lighting changes

				InViewportClient->Viewport->Invalidate();
			}
		}
	}

	Start = StartSnapped = End = EndSnapped = FVector(0,0,0);

	if( DragTool )
	{
		DragTool->EndDrag();
		delete DragTool;
		DragTool = NULL;
		return 0;
	}

	return 1;
}

/**
 * Adds delta movement into the tracker.
 */

void FMouseDeltaTracker::AddDelta( FEditorLevelViewportClient* InViewportClient, const FName InKey, const INT InDelta, UBOOL InNudge )
{
	UBOOL LeftMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_LeftMouseButton),
		RightMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_RightMouseButton),
		MiddleMouseButtonDown = InViewportClient->Viewport->KeyState(KEY_MiddleMouseButton),
		bAltDown = InViewportClient->Input->IsAltPressed(),
		bShiftDown = InViewportClient->Input->IsShiftPressed(),
		bControlDown = InViewportClient->Input->IsCtrlPressed();

	if( !LeftMouseButtonDown && !MiddleMouseButtonDown && !RightMouseButtonDown && !InNudge )
	{
		return;
	}

	// If we are using a drag tool, the widget isn't involved so set it to having no active axis.  This
	// means we will get unmodified mouse movement returned to us by other functions.

	EAxis SaveAxis = InViewportClient->Widget->CurrentAxis;

	if( DragTool )
	{
		InViewportClient->Widget->CurrentAxis = AXIS_None;
	}

	// If the user isn't dragging with the left mouse button, clear out the axis 
	// as the widget only responds to the left mouse button.
	//
	// We allow an exception for dragging with the right mouse button while holding control
	// as that simulates using the rotation widget.

	if( !LeftMouseButtonDown && (!RightMouseButtonDown || !bControlDown) )
	{
		InViewportClient->Widget->CurrentAxis = AXIS_None;
	}

	FVector Wk = InViewportClient->TranslateDelta( InKey, InDelta, InNudge );

	if( InViewportClient->Widget->CurrentAxis != AXIS_None )
	{
		// Affect input delta by the camera speed

		switch( GEditorModeTools.GetWidgetMode() )
		{
			case WM_Rotate:
				Wk *= CAMERA_ROTATION_SPEED;
				break;

			default:
				break;
		}

		// Make rotations occur at the same speed, regardless of ortho zoom

		if( InViewportClient->IsOrtho() )
		{
			switch( GEditorModeTools.GetWidgetMode() )
			{
				case WM_Rotate:
				{
					FLOAT Scale = 1.0f;

					if( InViewportClient->IsOrtho() )
					{
						Scale = DEFAULT_ORTHOZOOM / (FLOAT)InViewportClient->OrthoZoom;
					}

					Wk *= Scale;
				}
				break;

				default:
					break;
			}
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

		DragTool->AddDelta( Drag );
	}
	else
	{
		switch( GEditorModeTools.GetWidgetMode() )
		{
			case WM_Translate:
			case WM_Scale:
			case WM_ScaleNonUniform:
				GEditor->Constraints.Snap( EndSnapped, FVector(GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize()) );
				break;

			case WM_Rotate:
			{
				FRotator rot( EndSnapped.X, EndSnapped.Y, EndSnapped.Z );
				GEditor->Constraints.Snap( rot );
				EndSnapped = FVector( rot.Pitch, rot.Yaw, rot.Roll );
			}
			break;

			default:
				break;
		}
	}

	if( DragTool )
	{
		InViewportClient->Widget->CurrentAxis = SaveAxis;
	}
}

/**
 * Retrieve information about the current drag delta.
 */

FVector FMouseDeltaTracker::GetDelta()
{
	return (End - Start);
}

FVector FMouseDeltaTracker::GetDeltaSnapped()
{
	return (EndSnapped - StartSnapped);
}

/**
 * Converts the delta movement to drag/rotation/scale based on the viewport type or widget axis
 */

void FMouseDeltaTracker::ConvertMovementDeltaToDragRot( FEditorLevelViewportClient* InViewportClient, const FVector& InDragDelta, FVector& InDrag, FRotator& InRotation, FVector& InScale )
{
	if( InViewportClient->Widget->CurrentAxis != AXIS_None )
		InViewportClient->Widget->ConvertMouseMovementToAxisMovement( InViewportClient, GEditorModeTools.PivotLocation, InDragDelta, InDrag, InRotation, InScale );
	else
		InViewportClient->ConvertMovementToDragRot( InDragDelta, InDrag, InRotation );
}

void FMouseDeltaTracker::ReduceBy( FVector& In )
{
	End -= In;
	EndSnapped -= In;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
