/*=============================================================================
	UnLinkedEdInterface.cpp: Base class for boxes-and-lines editing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "UnLinkedObjDrawUtils.h"
#include "PropertyWindow.h"
#include "EngineSequenceClasses.h"

extern UBOOL GKismetRealtimeDebugging;

const static FLOAT	LinkedObjectEditor_ZoomIncrement = 0.1f;
const static FLOAT	LinkedObjectEditor_ZoomSpeed = 0.005f;
const static FLOAT	LinkedObjectEditor_ZoomNotchThresh = 0.007f;
const static INT	LinkedObjectEditor_ScrollBorderSize = 20;
const static FLOAT	LinkedObjectEditor_ScrollBorderSpeed = 400.f;


/*-----------------------------------------------------------------------------
	FLinkedObjViewportClient
-----------------------------------------------------------------------------*/

FLinkedObjViewportClient::FLinkedObjViewportClient( FLinkedObjEdNotifyInterface* InEdInterface )
	:	bAlwaysDrawInTick( FALSE )
{
	// No postprocess.
	ShowFlags			&= ~SHOW_PostProcess;

	// This window will be 2D/canvas only, so set the viewport type to None
	ViewportType = LVT_None;

	EdInterface			= InEdInterface;

	Origin2D			= FIntPoint(0, 0);
	Zoom2D				= 1.0f;
	MinZoom2D			= 0.1f;
	MaxZoom2D			= 1.f;

	bMouseDown			= FALSE;
	OldMouseX			= 0;
	OldMouseY			= 0;

	BoxStartX			= 0;
	BoxStartY			= 0;
	BoxEndX				= 0;
	BoxEndY				= 0;

	DeltaXFraction		= 0.0f;
	DeltaYFraction		= 0.0f;

	ScrollAccum			= FVector2D(0,0);

	DistanceDragged		= 0;

	bTransactionBegun	= FALSE;
	bMakingLine			= FALSE;
	bMovingConnector	= FALSE;
	bSpecialDrag		= FALSE;
	bBoxSelecting		= FALSE;
	bAllowScroll		= TRUE;

	MouseOverObject		= NULL;
	MouseOverTime		= 0.f;
	ToolTipDelayMS		= -1;
	GConfig->GetInt( TEXT( "ToolTips" ), TEXT( "DelayInMS" ), ToolTipDelayMS, GEditorUserSettingsIni );

	SpecialIndex		= 0;
	
	DesiredPanTime		= 0.f;

	NewX = NewY = 0;

	SetRealtime( FALSE );

	CachedRealTime		= 0.f;
}

void FLinkedObjViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	Canvas->PushAbsoluteTransform(FScaleMatrix(Zoom2D) * FTranslationMatrix(FVector(Origin2D.X,Origin2D.Y,0)));
	{
		// Erase background
		Clear(Canvas, FColor(197,197,197) );

		// Update the canvas with the last updated real time.
		if (IsRealtime())
		{
			 CachedRealTime = appSeconds();
		}

		// Pass the latest updated time through to the canvas,
		// it will then be used by the individual material expressions.
		Canvas->CachedTime = CachedRealTime;

		EdInterface->DrawObjects( Viewport, Canvas );

		// Draw new line
		if(bMakingLine && !Canvas->IsHitTesting())
		{
			FIntPoint StartPoint = EdInterface->GetSelectedConnLocation(Canvas);
			FIntPoint EndPoint( (NewX - Origin2D.X)/Zoom2D, (NewY - Origin2D.Y)/Zoom2D );
			INT ConnType = EdInterface->GetSelectedConnectorType();
			FColor LinkColor = EdInterface->GetMakingLinkColor();

			// Curves
			{
				FLOAT Tension;
				if(ConnType == LOC_INPUT || ConnType == LOC_OUTPUT)
				{
					Tension = Abs<INT>(StartPoint.X - EndPoint.X);
				}
				else
				{
					Tension = Abs<INT>(StartPoint.Y - EndPoint.Y);
				}


				if(ConnType == LOC_INPUT)
				{
					FLinkedObjDrawUtils::DrawSpline(Canvas, StartPoint, Tension*FVector2D(-1,0), EndPoint, Tension*FVector2D(-1,0), LinkColor, FALSE);
				}
				else if(ConnType == LOC_OUTPUT)
				{
					FLinkedObjDrawUtils::DrawSpline(Canvas, StartPoint, Tension*FVector2D(1,0), EndPoint, Tension*FVector2D(1,0), LinkColor, FALSE);
				}
				else if(ConnType == LOC_VARIABLE)
				{
					FLinkedObjDrawUtils::DrawSpline(Canvas, StartPoint, Tension*FVector2D(0,1), EndPoint, FVector2D(0,0), LinkColor, FALSE);
				}
				else
				{
					FLinkedObjDrawUtils::DrawSpline(Canvas, StartPoint, Tension*FVector2D(0,1), EndPoint, Tension*FVector2D(0,1), LinkColor, FALSE);
				}
			}
		}

	}
	Canvas->PopTransform();

	Canvas->PushAbsoluteTransform(FTranslationMatrix(FVector(Origin2D.X,Origin2D.Y,0)));
	{
		// Draw the box select box
		if(bBoxSelecting)
		{
			INT MinX = (Min(BoxStartX, BoxEndX) - BoxOrigin2D.X);
			INT MinY = (Min(BoxStartY, BoxEndY) - BoxOrigin2D.Y);
			INT MaxX = (Max(BoxStartX, BoxEndX) - BoxOrigin2D.X);
			INT MaxY = (Max(BoxStartY, BoxEndY) - BoxOrigin2D.Y);

			DrawLine2D(Canvas,FVector2D(MinX, MinY), FVector2D(MaxX, MinY), FColor(255,0,0));
			DrawLine2D(Canvas,FVector2D(MaxX, MinY), FVector2D(MaxX, MaxY), FColor(255,0,0));
			DrawLine2D(Canvas,FVector2D(MaxX, MaxY), FVector2D(MinX, MaxY), FColor(255,0,0));
			DrawLine2D(Canvas,FVector2D(MinX, MaxY), FVector2D(MinX, MinY), FColor(255,0,0));
		}
	}
	Canvas->PopTransform();
}

UBOOL FLinkedObjViewportClient::InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT /*AmountDepressed*/,UBOOL /*Gamepad*/)
{
	const UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	const UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	const UBOOL bAltDown = Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt);

	const INT HitX = Viewport->GetMouseX();
	const INT HitY = Viewport->GetMouseY();

	if ( !Viewport->HasMouseCapture() )
	{
		Viewport->ShowCursor( TRUE );
		Viewport->LockMouseToWindow( FALSE );
	}

	static EInputEvent LastEvent = IE_Pressed;

	if( Key == KEY_LeftMouseButton )
	{
		switch( Event )
		{
		case IE_Pressed:
		case IE_DoubleClick:
			{
				DeltaXFraction = 0.0f;
				DeltaYFraction = 0.0f;
				HHitProxy* HitResult = Viewport->GetHitProxy(HitX,HitY);

				if( HitResult )
				{
					// Handle click/double-click on line proxy
					if ( HitResult->IsA( HLinkedObjLineProxy::StaticGetType() ) )
					{
						// clicked on a line
						HLinkedObjLineProxy *LineProxy = (HLinkedObjLineProxy*)HitResult;
						if ( Event == IE_Pressed )
						{
							if( !EdInterface->HaveObjectsSelected() )
							{
								EdInterface->ClickedLine(LineProxy->Src,LineProxy->Dest);
							}
						}
						else if ( Event == IE_DoubleClick )
						{
							EdInterface->DoubleClickedLine( LineProxy->Src,LineProxy->Dest );
						}
					}
					// Handle click/double-click on object proxy
					else if( HitResult->IsA( HLinkedObjProxy::StaticGetType() ) )
					{
						UObject* Obj = ( (HLinkedObjProxy*)HitResult )->Obj;
						if( !bCtrlDown )
						{
							if( Event == IE_DoubleClick )
							{
								EdInterface->EmptySelection();
								EdInterface->AddToSelection( Obj );
								EdInterface->UpdatePropertyWindow();	
								EdInterface->DoubleClickedObject( Obj );
								bMouseDown = FALSE;
								return TRUE;
							}
							else if( !EdInterface->HaveObjectsSelected() )
							{
								// if there are no objects selected add this object. 
								// if objects are selected, we should not clear the selection until a mouse up occurs
								// since the user might be trying to pan. Panning should not break selection.
								EdInterface->AddToSelection( Obj );
								EdInterface->UpdatePropertyWindow();
							}
						}
					}
					// Handle click/double-click on connector proxy
					else if( HitResult->IsA(HLinkedObjConnectorProxy::StaticGetType()) )
					{
						HLinkedObjConnectorProxy* ConnProxy = (HLinkedObjConnectorProxy*)HitResult;
						EdInterface->SetSelectedConnector( ConnProxy->Connector );

						EdInterface->EmptySelection();
						EdInterface->UpdatePropertyWindow();

						if ( bAltDown )
						{
							// break the connectors
							EdInterface->AltClickConnector( ConnProxy->Connector );
						}
						else if( bCtrlDown )
						{
							// Begin moving a connector if ctrl+mouse click over a connector was done
							bMovingConnector = TRUE;
							EdInterface->SetSelectedConnectorMoving( bMovingConnector );
						}
						else
						{
							if ( Event == IE_DoubleClick )
							{
								EdInterface->DoubleClickedConnector( ConnProxy->Connector );
							}
							else if( !(GKismetRealtimeDebugging && GEditor->PlayWorld) )
							{
								bMakingLine = TRUE;
								NewX = HitX;
								NewY = HitY;
							}
						}
					}
					// Handle click/double-click on special proxy
					else if( HitResult->IsA(HLinkedObjProxySpecial::StaticGetType()) )
					{
						HLinkedObjProxySpecial* SpecialProxy = (HLinkedObjProxySpecial*)HitResult;

						// Copy properties out of SpecialProxy first, in case it gets invalidated!
						INT ProxyIndex = SpecialProxy->SpecialIndex;
						UObject* ProxyObj = SpecialProxy->Obj;

						FIntPoint MousePos( (HitX - Origin2D.X)/Zoom2D, (HitY - Origin2D.Y)/Zoom2D );

						// If object wasn't selected already OR 
						// we didn't handle it all in special click - change selection
						if( !EdInterface->IsInSelection( ProxyObj ) || 
							!EdInterface->SpecialClick(  MousePos.X, MousePos.Y, ProxyIndex, Viewport, ProxyObj ) )
						{
							bSpecialDrag = TRUE;
							SpecialIndex = ProxyIndex;

							// Slightly quirky way of avoiding selecting the same thing again.
							if( !( EdInterface->GetNumSelected() == 1 && EdInterface->IsInSelection( ProxyObj ) ) )
							{
								EdInterface->EmptySelection();
								EdInterface->AddToSelection( ProxyObj );
								EdInterface->UpdatePropertyWindow();
							}

							// For supporting undo 
							EdInterface->BeginTransactionOnSelected();
							bTransactionBegun = TRUE;
						}
					}
				}
				else
				{
					if( bCtrlDown && bAltDown )
					{
						BoxOrigin2D = Origin2D;
						BoxStartX = BoxEndX = HitX;
						BoxStartY = BoxEndY = HitY;

						bBoxSelecting = TRUE;
					}
				}

				OldMouseX = HitX;
				OldMouseY = HitY;
				//default to not having moved yet
				bHasMouseMovedSinceClick = FALSE;
				DistanceDragged = 0;
				bMouseDown = TRUE;

				if( !bMakingLine && !bBoxSelecting && !bSpecialDrag && !(bCtrlDown && EdInterface->HaveObjectsSelected()) && bAllowScroll )
				{
				}
				else
				{
					Viewport->LockMouseToWindow(TRUE);
				}

				Viewport->Invalidate();
			}
			break;

		case IE_Released:
			{
				if( bMakingLine )
				{
					Viewport->Invalidate();
					HHitProxy* HitResult = Viewport->GetHitProxy( HitX,HitY );
					if( HitResult )
					{
						if( HitResult->IsA(HLinkedObjConnectorProxy::StaticGetType()) )
						{
							HLinkedObjConnectorProxy* EndConnProxy = (HLinkedObjConnectorProxy*)HitResult;

							if( DistanceDragged < 4 )
							{
								HLinkedObjConnectorProxy* ConnProxy = (HLinkedObjConnectorProxy*)HitResult;
								UBOOL bDoDeselect = EdInterface->ClickOnConnector( EndConnProxy->Connector.ConnObj, EndConnProxy->Connector.ConnType, EndConnProxy->Connector.ConnIndex );
								if( bDoDeselect && LastEvent != IE_DoubleClick )
								{
									EdInterface->EmptySelection();
									EdInterface->UpdatePropertyWindow();
								}
							}
							else if ( bAltDown )
							{
								EdInterface->AltClickConnector( EndConnProxy->Connector );
							}
							else
							{
								EdInterface->MakeConnectionToConnector( EndConnProxy->Connector );
							}
						}
						else if( HitResult->IsA(HLinkedObjProxy::StaticGetType()) )
						{
							UObject* Obj = ( (HLinkedObjProxy*)HitResult )->Obj;

							EdInterface->MakeConnectionToObject( Obj );
						}
					}
				}
				else if( bBoxSelecting )
				{
					// When box selecting, the region that user boxed can be larger than the size of the viewport
					// so we use the viewport as a max region and loop through the box, rendering different chunks of it
					// and reading back its hit proxy map to check for objects.
					TArray<UObject*> NewSelection;
					
					// Save the current origin since we will be modifying it.
					FVector2D SavedOrigin2D;
					
					SavedOrigin2D.X = Origin2D.X;
					SavedOrigin2D.Y = Origin2D.Y;

					// Calculate the size of the box and its extents.
					const INT MinX = Min(BoxStartX, BoxEndX);
					const INT MinY = Min(BoxStartY, BoxEndY);
					const INT MaxX = Max(BoxStartX, BoxEndX) + 1;
					const INT MaxY = Max(BoxStartY, BoxEndY) + 1;

					const INT ViewX = Viewport->GetSizeX()-1;
					const INT ViewY = Viewport->GetSizeY()-1;

					const INT BoxSizeX = MaxX - MinX;
					const INT BoxSizeY = MaxY - MinY;

					const FLOAT BoxMinX = MinX-BoxOrigin2D.X;
					const FLOAT BoxMinY = MinY-BoxOrigin2D.Y;
					const FLOAT BoxMaxX = BoxMinX + BoxSizeX;
					const FLOAT BoxMaxY = BoxMinY + BoxSizeY;

					// Loop through 'tiles' of the box using the viewport size as our maximum tile size.
					INT TestSizeX = Min(ViewX, BoxSizeX);
					INT TestSizeY = Min(ViewY, BoxSizeY);

					FLOAT TestStartX = BoxMinX;
					FLOAT TestStartY = BoxMinY;

					while(TestStartX < BoxMaxX)
					{
						TestStartY = BoxMinY;
						TestSizeY = Min(ViewY, BoxSizeY);
				
						while(TestStartY < BoxMaxY)
						{
							// We read back the hit proxy map for the required region.
							Origin2D.X = -TestStartX;
							Origin2D.Y = -TestStartY;

							TArray<HHitProxy*> ProxyMap;
							Viewport->Invalidate();
							Viewport->GetHitProxyMap( (UINT)0, (UINT)0, (UINT)TestSizeX, (UINT)TestSizeY, ProxyMap );

							// Find any keypoint hit proxies in the region - add the keypoint to selection.
							for( INT Y=0; Y < TestSizeY; Y++ )
							{
								for( INT X=0; X < TestSizeX; X++ )
								{
									HHitProxy* HitProxy = NULL;							
									INT ProxyMapIndex = Y * TestSizeX + X; // calculate location in proxy map
									if( ProxyMapIndex < ProxyMap.Num() ) // If within range, grab the hit proxy from there
									{
										HitProxy = ProxyMap(ProxyMapIndex);
									}

									UObject* SelObject = NULL;

									// If we got one, add it to the NewSelection list.
									if( HitProxy )
									{

										if(HitProxy->IsA(HLinkedObjProxy::StaticGetType()))
										{
											SelObject = ((HLinkedObjProxy*)HitProxy)->Obj;
										}
										// Special case for the little resizer triangles in the bottom right corner of comment frames
										else if( HitProxy->IsA(HLinkedObjProxySpecial::StaticGetType()) )
										{
											SelObject = ((HLinkedObjProxySpecial*)HitProxy)->Obj;

											if( !SelObject->IsA(USequenceFrame::StaticClass()) )
											{
												SelObject = NULL;
											}
										}

										if( SelObject )
										{
											// Don't want to call AddToSelection here because that might invalidate the display and we'll crash.
											NewSelection.AddUniqueItem( SelObject );
										}
									}
								}
							}

							TestStartY += ViewY;
							TestSizeY = Min(ViewY, appTrunc(BoxMaxY - TestStartY));
						}

						TestStartX += ViewX;
						TestSizeX = Min(ViewX, appTrunc(BoxMaxX - TestStartX));
					}

		
					// restore the original viewport settings
					Origin2D.X = SavedOrigin2D.X;
					Origin2D.Y = SavedOrigin2D.Y;

					// If shift is down, don't empty, just add to selection.
					if( !bShiftDown )
					{
						EdInterface->EmptySelection();
					}
					
					// Iterate over array adding each to selection.
					for( INT i=0; i<NewSelection.Num(); i++ )
					{
						EdInterface->AddToSelection( NewSelection(i) );
					}

					EdInterface->UpdatePropertyWindow();
				}
				else
				{
					HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);

					// If mouse didn't really move since last time, and we released over empty space, deselect everything.
					if( !HitResult && DistanceDragged < 4 )
					{
						NewX = HitX;
						NewY = HitY;
						const UBOOL bDoDeselect = EdInterface->ClickOnBackground() && !bCtrlDown;
						if( bDoDeselect && LastEvent != IE_DoubleClick )
						{
							EdInterface->EmptySelection();
							EdInterface->UpdatePropertyWindow();
						}
					}
					// If we've hit something and not really dragged anywhere, attempt to select
					// whatever has been hit
					else if ( HitResult && DistanceDragged < 4 )
					{
						// Released on a line
						if ( HitResult->IsA(HLinkedObjLineProxy::StaticGetType()) )
						{
							HLinkedObjLineProxy *LineProxy = (HLinkedObjLineProxy*)HitResult;
							EdInterface->ClickedLine( LineProxy->Src,LineProxy->Dest );
						}
						// Released on an object
						else if( HitResult->IsA(HLinkedObjProxy::StaticGetType()) )
						{
							UObject* Obj = (( HLinkedObjProxy*)HitResult )->Obj;

							if( !bCtrlDown )
							{
								EdInterface->EmptySelection();
								EdInterface->AddToSelection(Obj);
							}
							else if( EdInterface->IsInSelection( Obj ) )
							{
								// Remove the object from selection its in the selection and this isn't the initial click where the object was added
								EdInterface->RemoveFromSelection( Obj );
							}
							// At this point, the user CTRL-clicked on an unselected kismet object.
							// Since the the user isn't dragging, add the object the selection.
							else
							{
								// The user is trying to select multiple objects at once
								EdInterface->AddToSelection( Obj );
							}
							EdInterface->UpdatePropertyWindow();
						}
					}
					else if( bCtrlDown && DistanceDragged >= 4 )
					{
						EdInterface->PositionSelectedObjects();
					}
				}

				if( bTransactionBegun )
				{
					EdInterface->EndTransactionOnSelected();
					bTransactionBegun = FALSE;
				}

				bMouseDown = FALSE;
				bMakingLine = FALSE;
				// Mouse was released, stop moving the connector
				bMovingConnector = FALSE;
				EdInterface->SetSelectedConnectorMoving( bMovingConnector );
				bSpecialDrag = FALSE;
				bBoxSelecting = FALSE;

				Viewport->LockMouseToWindow( FALSE );
				Viewport->Invalidate();
			}
			break;
		}
	}
	else if( Key == KEY_RightMouseButton )
	{
		switch( Event )
		{
		case IE_Pressed:
			{
				NewX = Viewport->GetMouseX();
				NewY = Viewport->GetMouseY();
				DeltaXFraction = 0.0f;
				DeltaYFraction = 0.0f;
				DistanceDragged = 0;
			}
			break;

		case IE_Released:
			{
				Viewport->Invalidate();

				if(bMakingLine || Viewport->KeyState(KEY_LeftMouseButton))
					break;

				INT HitX = Viewport->GetMouseX();
				INT HitY = Viewport->GetMouseY();

				// If right clicked and dragged - don't pop up menu. Have to click and release in roughly the same spot.
				if( Abs(HitX - NewX) + Abs(HitY - NewY) > 4 || DistanceDragged > 4)
					break;

				HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);

				wxMenu* menu = NULL;
				if(!HitResult)
				{
					EdInterface->OpenNewObjectMenu();
				}
				else
				{
					if( HitResult->IsA(HLinkedObjConnectorProxy::StaticGetType()) )
					{
						HLinkedObjConnectorProxy* ConnProxy = (HLinkedObjConnectorProxy*)HitResult;

						// First select the connector and deselect any objects.
						EdInterface->SetSelectedConnector( ConnProxy->Connector );
						EdInterface->EmptySelection();
						EdInterface->UpdatePropertyWindow();
						Viewport->Invalidate();

						// Then open connector options menu.
						EdInterface->OpenConnectorOptionsMenu();
					}
					else if( HitResult->IsA(HLinkedObjProxy::StaticGetType()) )
					{
						// When right clicking on an unselected object, select it only before opening menu.
						UObject* Obj = ((HLinkedObjProxy*)HitResult)->Obj;

						if( !EdInterface->IsInSelection(Obj) )
						{
							EdInterface->EmptySelection();
							EdInterface->AddToSelection(Obj);
							EdInterface->UpdatePropertyWindow();
							Viewport->Invalidate();
						}
					
						EdInterface->OpenObjectOptionsMenu();
					}
				}
			}
			break;
		}
	}
	else if ( (Key == KEY_MouseScrollDown || Key == KEY_MouseScrollUp) && Event == IE_Pressed )
	{
		// Mousewheel up/down zooms in/out.
		const FLOAT DeltaZoom = (Key == KEY_MouseScrollDown ? -LinkedObjectEditor_ZoomIncrement : LinkedObjectEditor_ZoomIncrement );

		if( (DeltaZoom < 0.f && Zoom2D > MinZoom2D) ||
			(DeltaZoom > 0.f && Zoom2D < MaxZoom2D) )
		{
			//Default zooming to center of the viewport
			FLOAT CenterOfZoomX = Viewport->GetSizeX()*0.5f;
			FLOAT CenterOfZoomY= Viewport->GetSizeY()*0.5f;
			if (GEditorModeTools().GetCenterZoomAroundCursor())
			{
				//center of zoom is now around the mouse
				CenterOfZoomX = OldMouseX;
				CenterOfZoomY = OldMouseY;
			}
			//Old offset
			const FLOAT ViewCenterX = (CenterOfZoomX - Origin2D.X)/Zoom2D;
			const FLOAT ViewCenterY = (CenterOfZoomY - Origin2D.Y)/Zoom2D;

			//change zoom ratio
			Zoom2D = Clamp<FLOAT>(Zoom2D+DeltaZoom,MinZoom2D,MaxZoom2D);

			//account for new offset
			FLOAT DrawOriginX = ViewCenterX - (CenterOfZoomX/Zoom2D);
			FLOAT DrawOriginY = ViewCenterY - (CenterOfZoomY/Zoom2D);

			//move origin by delta we've calculated
			Origin2D.X = -(DrawOriginX * Zoom2D);
			Origin2D.Y = -(DrawOriginY * Zoom2D);

			EdInterface->ViewPosChanged();
			Viewport->Invalidate();
		}
	}
	// Handle the user pressing the first thumb mouse button
	else if ( Key == KEY_ThumbMouseButton && Event == IE_Pressed && !bMouseDown )
	{
		// Attempt to set the navigation history for the ed interface back an entry
		EdInterface->NavigationHistoryBack();
	}
	// Handle the user pressing the second thumb mouse button
	else if ( Key == KEY_ThumbMouseButton2 && Event == IE_Pressed && !bMouseDown )
	{
		// Attempt to set the navigation history for the ed interface forward an entry
		EdInterface->NavigationHistoryForward();
	}
	else if ( Event == IE_Pressed )
	{
		// Bookmark support
		TCHAR CurChar = 0;
		if( Key == KEY_Zero )		CurChar = '0';
		else if( Key == KEY_One )	CurChar = '1';
		else if( Key == KEY_Two )	CurChar = '2';
		else if( Key == KEY_Three )	CurChar = '3';
		else if( Key == KEY_Four )	CurChar = '4';
		else if( Key == KEY_Five )	CurChar = '5';
		else if( Key == KEY_Six )	CurChar = '6';
		else if( Key == KEY_Seven )	CurChar = '7';
		else if( Key == KEY_Eight )	CurChar = '8';
		else if( Key == KEY_Nine )	CurChar = '9';

		if( ( CurChar >= '0' && CurChar <= '9' ) && !bAltDown && !bShiftDown && !bMouseDown)
		{
			// Determine the bookmark index based on the input key
			const INT BookmarkIndex = CurChar - '0';

			// CTRL+# will set a bookmark, # will jump to it.
			if( bCtrlDown )
			{
				EdInterface->SetBookmark( BookmarkIndex );
			}
			else
			{
				EdInterface->JumpToBookmark( BookmarkIndex );
			}
		}
	}

	EdInterface->EdHandleKeyInput(Viewport, Key, Event);

	LastEvent = Event;

	// Hide and lock mouse cursor if we're capturing mouse input.
	// But don't hide mouse cursor if drawing lines and other special cases.
	UBOOL bCanLockMouse = (Key == KEY_LeftMouseButton) || (Key == KEY_RightMouseButton);
	if ( Viewport->HasMouseCapture() && bCanLockMouse )
	{
		//Update cursor and lock to window if invisible
		UBOOL bShowCursor = UpdateCursorVisibility ();
		UBOOL bDraggingObject = (bCtrlDown && EdInterface->HaveObjectsSelected());
		Viewport->LockMouseToWindow( !bShowCursor || bDraggingObject || bMakingLine || bBoxSelecting || bSpecialDrag);
	}

	// Handle viewport screenshot.
	InputTakeScreenshot( Viewport, Key, Event );

	return TRUE;
}

void FLinkedObjViewportClient::MouseMove(FViewport* Viewport, INT X, INT Y)
{
	INT DeltaX = X - OldMouseX;
	INT DeltaY = Y - OldMouseY;
	OldMouseX = X;
	OldMouseY = Y;

	// Do mouse-over stuff (if mouse button is not held).
	OnMouseOver( X, Y );
}

void FLinkedObjViewportClient::CapturedMouseMove(FViewport* InViewport, INT InMouseX, INT InMouseY)
{
	// Override to prevent FEditorLevelViewportClient propagating mouse movement to EditorModeTools as we don't want them updating at the same time.
}

/** Handle mouse over events */
void FLinkedObjViewportClient::OnMouseOver( INT X, INT Y )
{
	// Do mouse-over stuff (if mouse button is not held).
	UObject *NewMouseOverObject = NULL;
	INT NewMouseOverConnType = -1;
	INT NewMouseOverConnIndex = INDEX_NONE;
	HHitProxy*	HitResult = NULL;

	if(!bMouseDown || bMakingLine)
	{
		HitResult = Viewport->GetHitProxy(X,Y);
	}

	if( HitResult )
	{
		if( HitResult->IsA(HLinkedObjProxy::StaticGetType()) )
		{
			NewMouseOverObject = ((HLinkedObjProxy*)HitResult)->Obj;
		}
		else if( HitResult->IsA(HLinkedObjConnectorProxy::StaticGetType()) )
		{
			NewMouseOverObject = ((HLinkedObjConnectorProxy*)HitResult)->Connector.ConnObj;
			NewMouseOverConnType = ((HLinkedObjConnectorProxy*)HitResult)->Connector.ConnType;
			NewMouseOverConnIndex = ((HLinkedObjConnectorProxy*)HitResult)->Connector.ConnIndex;

			if( !EdInterface->ShouldHighlightConnector(((HLinkedObjConnectorProxy*)HitResult)->Connector) )
			{
				NewMouseOverConnType = -1;
				NewMouseOverConnIndex = INDEX_NONE;
			}
		}
		else if (HitResult->IsA(HLinkedObjLineProxy::StaticGetType()) && !bMakingLine)		// don't mouse-over lines when already creating a line
		{
			HLinkedObjLineProxy *LineProxy = (HLinkedObjLineProxy*)HitResult;
			NewMouseOverObject = LineProxy->Src.ConnObj;
			NewMouseOverConnType = LineProxy->Src.ConnType;
			NewMouseOverConnIndex = LineProxy->Src.ConnIndex;
		}

	}

	if(	NewMouseOverObject != MouseOverObject || 
		NewMouseOverConnType != MouseOverConnType ||
		NewMouseOverConnIndex != MouseOverConnIndex )
	{
		MouseOverObject = NewMouseOverObject;
		MouseOverConnType = NewMouseOverConnType;
		MouseOverConnIndex = NewMouseOverConnIndex;
		MouseOverTime = appSeconds();

		Viewport->InvalidateDisplay();
		EdInterface->OnMouseOver(MouseOverObject);
	}
}


UBOOL FLinkedObjViewportClient::InputAxis(FViewport* Viewport,INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime, UBOOL bGamepad)
{
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);

	// DeviceDelta is not constricted to mouse locks
	FLOAT DeviceDeltaX = (Key == KEY_MouseX) ? Delta : 0;
	FLOAT DeviceDeltaY = (Key == KEY_MouseY) ? -Delta : 0;

	// Mouse variables represent the actual (potentially constrained) location of the mouse
	INT MouseX = Viewport->GetMouseX();	
	INT MouseY = Viewport->GetMouseY();
	INT MouseDeltaX = MouseX - OldMouseX;
	INT MouseDeltaY = MouseY - OldMouseY;

	// Accumulate delta fractions, since these will get dropped when truncated to INT.
	DeltaXFraction += MouseDeltaX * (1.f/Zoom2D) - INT(MouseDeltaX * (1.f/Zoom2D));
	DeltaYFraction += MouseDeltaY * (1.f/Zoom2D) - INT(MouseDeltaY * (1.f/Zoom2D));
	INT DeltaXAdd = INT(DeltaXFraction);
	INT DeltaYAdd = INT(DeltaYFraction);
	DeltaXFraction -= DeltaXAdd;
	DeltaYFraction -= DeltaYAdd;

	UBOOL bLeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton);
	UBOOL bRightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton);

	if( Key == KEY_MouseX || Key == KEY_MouseY )
	{
		DistanceDragged += Abs(MouseDeltaX) + Abs(MouseDeltaY);
	}

	// If holding both buttons, we are zooming.
	if(bLeftMouseButtonDown && bRightMouseButtonDown)
	{
		if(Key == KEY_MouseY)
		{
			//Always zoom around center for two button zoom
			FLOAT CenterOfZoomX = Viewport->GetSizeX()*0.5f;
			FLOAT CenterOfZoomY= Viewport->GetSizeY()*0.5f;

			const FLOAT ZoomDelta = -Zoom2D * Delta * LinkedObjectEditor_ZoomSpeed;

			const FLOAT ViewCenterX = (CenterOfZoomX - (FLOAT)Origin2D.X)/Zoom2D;
			const FLOAT ViewCenterY = (CenterOfZoomY - (FLOAT)Origin2D.Y)/Zoom2D;

			Zoom2D = Clamp<FLOAT>(Zoom2D+ZoomDelta,MinZoom2D,MaxZoom2D);

			// We have a 'notch' around 1.f to make it easy to get back to normal zoom factor.
			if( Abs(Zoom2D - 1.f) < LinkedObjectEditor_ZoomNotchThresh )
			{
				Zoom2D = 1.f;
			}

			const FLOAT DrawOriginX = ViewCenterX - (CenterOfZoomX/Zoom2D);
			const FLOAT DrawOriginY = ViewCenterY - (CenterOfZoomY/Zoom2D);

			Origin2D.X = -appRound(DrawOriginX * Zoom2D);
			Origin2D.Y = -appRound(DrawOriginY * Zoom2D);

			EdInterface->ViewPosChanged();
			Viewport->Invalidate();
		}
	}
	else if(bLeftMouseButtonDown || bRightMouseButtonDown)
	{
		UBOOL bInvalidate = FALSE;
		if(bMakingLine)
		{
			NewX = MouseX;
			NewY = MouseY;
			bInvalidate = TRUE;
		}
		else if(bBoxSelecting)
		{
			BoxEndX = MouseX + (BoxOrigin2D.X - Origin2D.X);
			BoxEndY = MouseY + (BoxOrigin2D.Y - Origin2D.Y);
			bInvalidate = TRUE;
		}
		else if(bSpecialDrag)
		{
			FIntPoint MousePos( (MouseX - Origin2D.X)/Zoom2D, (MouseY - Origin2D.Y)/Zoom2D );
			EdInterface->SpecialDrag( MouseDeltaX * (1.f/Zoom2D) + DeltaXAdd, MouseDeltaY * (1.f/Zoom2D) + DeltaYAdd, MousePos.X, MousePos.Y, SpecialIndex );
			bInvalidate = TRUE;
		}
		else if( bCtrlDown && EdInterface->HaveObjectsSelected() )
		{
			EdInterface->MoveSelectedObjects( MouseDeltaX * (1.f/Zoom2D) + DeltaXAdd, MouseDeltaY * (1.f/Zoom2D) + DeltaYAdd );

			// If haven't started a transaction, and moving some stuff, and have moved mouse far enough, start transaction now.
			if(!bTransactionBegun && DistanceDragged > 4)
			{
				EdInterface->BeginTransactionOnSelected();
				bTransactionBegun = TRUE;
			}
			bInvalidate = TRUE;
		}
		else if( bCtrlDown && bMovingConnector )
		{
			// A connector is being moved.  Calculate the delta it should move
			const FLOAT InvZoom2D = 1.f/Zoom2D;
			INT DX = MouseDeltaX * InvZoom2D + DeltaXAdd;
			INT DY = MouseDeltaY * InvZoom2D + DeltaYAdd;
			EdInterface->MoveSelectedConnLocation(DX,DY);
			bInvalidate = TRUE;
		}
		else if(bAllowScroll && Viewport->HasMouseCapture())
		{
			//Default to using device delta
			INT DeltaXForScroll;
			INT DeltaYForScroll;
			if (GEditorModeTools().GetPanMovesCanvas())
			{
				//override to stay with the mouse. it's pixel accurate.
				DeltaXForScroll = DeviceDeltaX;
				DeltaYForScroll = DeviceDeltaY;
				if (DeltaXForScroll || DeltaYForScroll)
				{
					MarkMouseMovedSinceClick();
				}
				//assign both so the updates work right (now) AND the assignment at the bottom works (after).
				OldMouseX = MouseX = OldMouseX + DeviceDeltaX;
				OldMouseY = MouseY = OldMouseY + DeviceDeltaY;

				UpdateCursorVisibility();
				UpdateMousePosition();
			}
			else
			{
				DeltaXForScroll = -DeviceDeltaX;
				DeltaYForScroll = -DeviceDeltaY;
			}

			Origin2D.X += DeltaXForScroll;
			Origin2D.Y += DeltaYForScroll;
			EdInterface->ViewPosChanged();
			bInvalidate = TRUE;
		}

		OnMouseOver( MouseX, MouseY );

		if ( bInvalidate )
		{
			Viewport->InvalidateDisplay();
		}
	}

	//save the latest mouse position
	OldMouseX = MouseX;
	OldMouseY = MouseY;

	return TRUE;
}

EMouseCursor FLinkedObjViewportClient::GetCursor(FViewport* Viewport,INT X,INT Y)
{
	UBOOL bLeftDown  = Viewport->KeyState(KEY_LeftMouseButton) ? TRUE : FALSE;
	UBOOL bRightDown = Viewport->KeyState(KEY_RightMouseButton) ? TRUE : FALSE;

	//if we're allowed to scroll, we are in "canvas move mode" and ONLY one mouse button is down
	if ((bAllowScroll) && GEditorModeTools().GetPanMovesCanvas() && (bLeftDown ^ bRightDown))
	{
		const UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	
		//double check there is no other overriding operation (other than panning)
		if (!bMakingLine && !bBoxSelecting && !bSpecialDrag)
		{
			if ((bCtrlDown && EdInterface->HaveObjectsSelected()))
			{
				return MC_SizeAll;
			}
			else if (bHasMouseMovedSinceClick)
			{
				return MC_GrabHand;
			}
		}
	}

	return MC_Arrow;
}


/**
 * Sets the cursor to be visible or not.  Meant to be called as the mouse moves around in "move canvas" mode (not just on button clicks)
 */
UBOOL FLinkedObjViewportClient::UpdateCursorVisibility (void)
{
	UBOOL bShowCursor = ShouldCursorBeVisible();

	UBOOL bCursorWasVisible = Viewport->IsCursorVisible() ;
	Viewport->ShowCursor( bShowCursor);

	//first time showing the cursor again.  Update old mouse position so there isn't a jump as well.
	if (!bCursorWasVisible && bShowCursor)
	{
		OldMouseX = Viewport->GetMouseX();
		OldMouseY = Viewport->GetMouseY();
	}

	return bShowCursor;
}

/**
 * Given that we're in "move canvas" mode, set the snap back visible mouse position to clamp to the viewport
 */
void FLinkedObjViewportClient::UpdateMousePosition(void)
{
	const INT SizeX = Viewport->GetSizeX();
	const INT SizeY = Viewport->GetSizeY();

	INT ClampedMouseX = Clamp<INT>(OldMouseX, 0, SizeX);
	INT ClampedMouseY = Clamp<INT>(OldMouseY, 0, SizeY);

	Viewport->SetMouse(ClampedMouseX, ClampedMouseY);
}

/** Determines if the cursor should presently be visible
 * @return - TRUE if the cursor should remain visible
 */
UBOOL FLinkedObjViewportClient::ShouldCursorBeVisible (void)
{
	UBOOL bLeftDown  = Viewport->KeyState(KEY_LeftMouseButton) ? TRUE : FALSE;
	UBOOL bRightDown = Viewport->KeyState(KEY_RightMouseButton) ? TRUE : FALSE;
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);

	const INT SizeX = Viewport->GetSizeX();
	const INT SizeY = Viewport->GetSizeY();

	UBOOL bInViewport = IsWithin<INT>(OldMouseX, 1, SizeX-1) && IsWithin<INT>(OldMouseY, 1, SizeY-1);

	//both mouse button zoom hides mouse as well
	UBOOL bShowMouseOnScroll = (!bAllowScroll) || GEditorModeTools().GetPanMovesCanvas() && (bLeftDown ^ bRightDown) && bInViewport;
	//if scrolling isn't allowed, or we're in "inverted" pan mode, lave the mouse visible
	UBOOL bHideCursor = !bMakingLine && !bBoxSelecting && !bSpecialDrag && !(bCtrlDown && EdInterface->HaveObjectsSelected()) && !bShowMouseOnScroll;

	return !bHideCursor;

}
/** 
 *	See if cursor is in 'scroll' region around the edge, and if so, scroll the view automatically. 
 *	Returns the distance that the view was moved.
 */
FIntPoint FLinkedObjViewportClient::DoScrollBorder(FLOAT DeltaTime)
{
	FIntPoint Result( 0, 0 );

	if (bAllowScroll)
	{
		const INT PosX = Viewport->GetMouseX();
		const INT PosY = Viewport->GetMouseY();
		const INT SizeX = Viewport->GetSizeX();
		const INT SizeY = Viewport->GetSizeY();

		DeltaTime = Clamp(DeltaTime, 0.01f, 1.0f);

		if(PosX < LinkedObjectEditor_ScrollBorderSize)
		{
			ScrollAccum.X += (1.f - ((FLOAT)PosX/(FLOAT)LinkedObjectEditor_ScrollBorderSize)) * LinkedObjectEditor_ScrollBorderSpeed * DeltaTime;
		}
		else if(PosX > SizeX - LinkedObjectEditor_ScrollBorderSize)
		{
			ScrollAccum.X -= ((FLOAT)(PosX - (SizeX - LinkedObjectEditor_ScrollBorderSize))/(FLOAT)LinkedObjectEditor_ScrollBorderSize) * LinkedObjectEditor_ScrollBorderSpeed * DeltaTime;
		}
		else
		{
			ScrollAccum.X = 0.f;
		}

		FLOAT ScrollY = 0.f;
		if(PosY < LinkedObjectEditor_ScrollBorderSize)
		{
			ScrollAccum.Y += (1.f - ((FLOAT)PosY/(FLOAT)LinkedObjectEditor_ScrollBorderSize)) * LinkedObjectEditor_ScrollBorderSpeed * DeltaTime;
		}
		else if(PosY > SizeY - LinkedObjectEditor_ScrollBorderSize)
		{
			ScrollAccum.Y -= ((FLOAT)(PosY - (SizeY - LinkedObjectEditor_ScrollBorderSize))/(FLOAT)LinkedObjectEditor_ScrollBorderSize) * LinkedObjectEditor_ScrollBorderSpeed * DeltaTime;
		}
		else
		{
			ScrollAccum.Y = 0.f;
		}

		// Apply integer part of ScrollAccum to origin, and save the rest.
		const INT MoveX = appFloor(ScrollAccum.X);
		Origin2D.X += MoveX;
		ScrollAccum.X -= MoveX;

		const INT MoveY = appFloor(ScrollAccum.Y);
		Origin2D.Y += MoveY;
		ScrollAccum.Y -= MoveY;

		// Update the box selection if necessary
		if (bBoxSelecting)
		{
			BoxEndX += MoveX;
			BoxEndY += MoveY;
		}

		// If view has changed, notify the app and redraw the viewport.
		if( Abs<INT>(MoveX) > 0 || Abs<INT>(MoveY) > 0 )
		{
			EdInterface->ViewPosChanged();
			Viewport->Invalidate();
		}
		Result = FIntPoint(MoveX, MoveY);
	}

	return Result;
}

/**
 * Sets whether or not the viewport should be invalidated in Tick().
 */
void FLinkedObjViewportClient::SetRedrawInTick(UBOOL bInAlwaysDrawInTick)
{
	bAlwaysDrawInTick = bInAlwaysDrawInTick;
}

void FLinkedObjViewportClient::Tick(FLOAT DeltaSeconds)
{
	FEditorLevelViewportClient::Tick(DeltaSeconds);

	// Auto-scroll display if moving/drawing etc. and near edge.
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	if(	bMouseDown )		
	{
		// If holding both buttons, we are zooming.
		if(Viewport->KeyState(KEY_RightMouseButton) && Viewport->KeyState(KEY_LeftMouseButton))
		{
		}
		else if(bMakingLine || bBoxSelecting)
		{
			DoScrollBorder(DeltaSeconds);
		}
		else if(bSpecialDrag)
		{
			FIntPoint Delta = DoScrollBorder(DeltaSeconds);
			if(Delta.Size() > 0)
			{
				EdInterface->SpecialDrag( -Delta.X * (1.f/Zoom2D), -Delta.Y * (1.f/Zoom2D), 0, 0, SpecialIndex ); // TODO fix mouse position in this case.
			}
		}
		else if(bCtrlDown && EdInterface->HaveObjectsSelected())
		{
			FIntPoint Delta = DoScrollBorder(DeltaSeconds);

			// In the case of dragging boxes around, we move them as well when dragging at the edge of the screen.
			EdInterface->MoveSelectedObjects( -Delta.X * (1.f/Zoom2D), -Delta.Y * (1.f/Zoom2D) );

			DistanceDragged += ( Abs<INT>(Delta.X) + Abs<INT>(Delta.Y) );

			if(!bTransactionBegun && DistanceDragged > 4)
			{
				EdInterface->BeginTransactionOnSelected();
				bTransactionBegun = TRUE;
			}
		}
	}

	// Pan to DesiredOrigin2D within DesiredPanTime seconds.
	if( DesiredPanTime > 0.f )
	{
		Origin2D.X = Lerp( Origin2D.X, DesiredOrigin2D.X, Min(DeltaSeconds/DesiredPanTime,1.f) );
		Origin2D.Y = Lerp( Origin2D.Y, DesiredOrigin2D.Y, Min(DeltaSeconds/DesiredPanTime,1.f) );
		DesiredPanTime -= DeltaSeconds;
		Viewport->InvalidateDisplay();
	}
	else if ( bAlwaysDrawInTick  )
	{
		Viewport->InvalidateDisplay();
	}

	if (MouseOverObject 
		&& ToolTipDelayMS > 0
		&& (appSeconds() -  MouseOverTime) > ToolTipDelayMS * .001f)
	{
		// Redraw so that tooltips can be displayed
		Viewport->InvalidateDisplay();
	}
}

/*-----------------------------------------------------------------------------
WxLinkedObjVCHolder
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxLinkedObjVCHolder, wxWindow )
EVT_SIZE( WxLinkedObjVCHolder::OnSize )
END_EVENT_TABLE()

WxLinkedObjVCHolder::WxLinkedObjVCHolder( wxWindow* InParent, wxWindowID InID, FLinkedObjEdNotifyInterface* InEdInterface )
: wxWindow( InParent, InID )
{
	LinkedObjVC = new FLinkedObjViewportClient( InEdInterface );
	LinkedObjVC->Viewport = GEngine->Client->CreateWindowChildViewport(LinkedObjVC, (HWND)GetHandle());
	LinkedObjVC->Viewport->CaptureJoystickInput(FALSE);
}

WxLinkedObjVCHolder::~WxLinkedObjVCHolder()
{
	GEngine->Client->CloseViewport(LinkedObjVC->Viewport);
	LinkedObjVC->Viewport = NULL;
	delete LinkedObjVC;
}

void WxLinkedObjVCHolder::OnSize( wxSizeEvent& In )
{
	wxRect rc = GetClientRect();

	::MoveWindow( (HWND)LinkedObjVC->Viewport->GetWindow(), 0, 0, rc.GetWidth(), rc.GetHeight(), 1 );
}

/*-----------------------------------------------------------------------------
	FLinkedObjEdNavigationHistoryData
-----------------------------------------------------------------------------*/

/**
 * Construct a FLinkedObjEdNavigationHistoryData object
 *
 * @param	InHistoryString	String to set HistoryString to; Will display in the navigation menu; CANNOT be empty
 */
FLinkedObjEdNavigationHistoryData::FLinkedObjEdNavigationHistoryData( FString InHistoryString )
:	HistoryString( InHistoryString ),
	HistoryCameraZoom2D( 0.0f ),
	HistoryCameraXPosition( 0 ),
	HistoryCameraYPosition( 0 )
{
	checkf( InHistoryString.Len() > 0, TEXT("Cannot have history data without a valid string!") );
}

/**
 * Convenience function to quickly set the zoom and position values of the struct
 *
 * @param	InXPos		Value to set HistoryXPos to; Represents X-position of the camera at the time of the history event
 * @param	InYPos		Value to set HistoryYPos to; Represents Y-position of the camera at the time of the history event
 * @param	InZoom2D	Value to set HistoryZoom2D to; Represents zoom of the camera at the time of the history event
 */
void FLinkedObjEdNavigationHistoryData::SetPositionAndZoomData( INT InXPos, INT InYPos, FLOAT InZoom2D )
{
	HistoryCameraXPosition = InXPos;
	HistoryCameraYPosition = InYPos;
	HistoryCameraZoom2D = InZoom2D;
}


/*-----------------------------------------------------------------------------
	FLinkedObjEdNavigationHistory
-----------------------------------------------------------------------------*/

/**
 * Construct a FLinkedObjEdNavigationHistory object
 */
FLinkedObjEdNavigationHistory::FLinkedObjEdNavigationHistory()
:	HistoryMenu( NULL ),
	bAttachedToToolBar ( FALSE ),
	CurNavHistoryIndex( 0 ),
	NavHistoryIndexBeforeLastNavCommand( 0 )
{
}

/**
 * Destroy a FLinkedObjEdNavigationHistory object
 */
FLinkedObjEdNavigationHistory::~FLinkedObjEdNavigationHistory()
{
	// Free memory allocated to the menu
	if (HistoryMenu)
	{
	delete HistoryMenu;
	HistoryMenu = NULL;
	}

	// Free memory allocated for navigation data
	for ( TArray<FLinkedObjEdNavigationHistoryData*>::TIterator NavHistoryIter( HistoryNavigationData ); NavHistoryIter; ++NavHistoryIter )
	{
		delete *NavHistoryIter;
		*NavHistoryIter = NULL;
	}
	HistoryNavigationData.Empty();
}

/**
 * Attach back, forward, and drop-down buttons to the specified toolbar
 *
 * @param	InParentToolBar			Toolbar to attach buttons to
 * @param	bInsertSeparatorBefore	If TRUE, a separator will be placed on the toolbar in a position prior to the new buttons being added
 * @param	bInsertSeparatorAfter	If TRUE, a separator will be placed on the toolbar in a position after the new buttons being added
 * @param	InPosition				If specified, the position that the buttons will start being inserted at on the toolbar; If negative or not-specified,
 *									the buttons will be placed at the end of the toolbar
 */
void FLinkedObjEdNavigationHistory::AttachToToolBar( wxToolBar* InParentToolBar, UBOOL bInsertSeparatorBefore /*= FALSE*/, UBOOL bInsertSeparatorAfter /*= FALSE*/, INT InPosition /*= -1*/ )
{
	if ( InParentToolBar && !bAttachedToToolBar )
	{
		// Load the bitmaps required for the tools
		BackB.Load( TEXT("BackArrow.png") );
		ForwardB.Load( TEXT("ForwardArrow.png") );
		DownArrowB.Load( TEXT("DownArrow.png") );

		// Create the history drop-down button and its associated menu
		HistoryMenu = new wxMenu();
		HistoryListButton.Create( InParentToolBar, IDPB_LINKED_OBJ_EDITOR_NAV_HISTORY, &DownArrowB, HistoryMenu, wxPoint(0,0), wxSize(-1,21) );
		HistoryListButton.SetToolTip( *LocalizeUnrealEd( TEXT("LinkedObjEditor_NavHistory_PulldownTooltip") ) );

		// Determine where the tools will be placed, depending upon whether the user specified an explicit position or not
		INT InsertionPosition = ( InPosition >= 0 ) ? InPosition : InParentToolBar->GetToolsCount();
	
		// Place a separator on the toolbar before adding the new tools, if the user desires
		if ( bInsertSeparatorBefore )
		{
			InParentToolBar->InsertSeparator( InsertionPosition++ );
		}

		// Place the tools on the toolbar
		InParentToolBar->InsertTool( InsertionPosition++, IDM_LinkedObjNavHistory_Back, BackB, wxNullBitmap, FALSE, NULL, *LocalizeUnrealEd( TEXT("LinkedObjEditor_NavHistory_BackTooltip") ) );
		InParentToolBar->InsertTool( InsertionPosition++, IDM_LinkedObjNavHistory_Forward, ForwardB, wxNullBitmap, FALSE, NULL, *LocalizeUnrealEd( TEXT("LinkedObjEditor_NavHistory_ForwardTooltip") ) );
		InParentToolBar->InsertControl( InsertionPosition++, &HistoryListButton );
		
		// Place a separator on the toolbar after adding the new tools, if the user desires
		if ( bInsertSeparatorAfter )
		{
			InParentToolBar->InsertSeparator( InsertionPosition++ );
		}

		// Update the toolbar to acknowledge its new tools
		InParentToolBar->Realize();
		bAttachedToToolBar = TRUE;

		// Update the drop-down history menu
		UpdateMenu();
	}
}

/**
 * Add a new history data object to the navigation history at the current index, and make it the current item within the system. If new history 
 * data is added while the user is not currently at the last history item, all history items following the insertion point are discarded, just
 * as occurs with a web browser. If new history data is added while the system is currently at its maximum number of entries, the oldest entry is
 * discarded first to make room. Once data is added to the class, it will be maintained internally and deleted when it is no longer required or
 * upon destruction. Outside code should *NOT* maintain references to history data for this reason.
 *
 * @param	InData	History data object to add to the navigation history
 */
void FLinkedObjEdNavigationHistory::AddHistoryNavigationData( FLinkedObjEdNavigationHistoryData* InData )
{
	check(InData);
	const INT CurNumDataEntries = HistoryNavigationData.Num();

	// Handle the case where the added data will be the first data in the array
	if ( CurNumDataEntries == 0 )
	{
		HistoryNavigationData.AddUniqueItem( InData );
		CurNavHistoryIndex = 0;
		NavHistoryIndexBeforeLastNavCommand = 0;
	}

	// Handle the case where the added data will be at the end of the array
	else if ( CurNavHistoryIndex == CurNumDataEntries - 1 )
	{
		// If the array is currently at its enforced capacity, delete the oldest history item to make room
		if ( CurNumDataEntries == MAX_HISTORY_ENTRIES )
		{
			FLinkedObjEdNavigationHistoryData* HistoryDataToDelete = HistoryNavigationData( 0 );
			HistoryNavigationData.RemoveItem( HistoryDataToDelete );
			delete HistoryDataToDelete;
		}
		HistoryNavigationData.AddUniqueItem( InData );
		CurNavHistoryIndex = HistoryNavigationData.Num() - 1;
		NavHistoryIndexBeforeLastNavCommand = CurNavHistoryIndex - 1;
	}

	// Handle the case where the added data will be inserted anywhere else
	else
	{
		// All history items beyond the current history item are removed from the history array and destroyed, as
		// it doesn't make sense to keep them if the user is not at the end of the list (that would be like supporting
		// alternate time-lines)
		for ( INT RemovalIndex = CurNavHistoryIndex + 1; RemovalIndex < HistoryNavigationData.Num(); ++RemovalIndex )
		{
			delete HistoryNavigationData( RemovalIndex );
			HistoryNavigationData( RemovalIndex ) = NULL;
		}
		if ( HistoryNavigationData.IsValidIndex( CurNavHistoryIndex + 1 ) )
		{
			HistoryNavigationData.Remove( CurNavHistoryIndex + 1, CurNumDataEntries - ( CurNavHistoryIndex + 1 ) );
		}
		
		HistoryNavigationData.AddUniqueItem( InData );
		CurNavHistoryIndex = HistoryNavigationData.Num() - 1;
		NavHistoryIndexBeforeLastNavCommand = CurNavHistoryIndex - 1;
	}

	// Update the navigation pull-down menu so that it includes the newly added data
	UpdateMenu();
}

/**
 * Make the history item that precedes the current history item the new current history item if possible, and return a pointer to it.
 *
 * @return	Pointer to the current history item after the back operation has occurred; NULL if there are no navigation history items or
 *			if the user is already at the first entry in the navigation history
 */
FLinkedObjEdNavigationHistoryData* FLinkedObjEdNavigationHistory::Back()
{
	FLinkedObjEdNavigationHistoryData* DataToReturn = NULL;
	
	// Can only advance backwards if there are data items and the current data item isn't the first one in the array
	if ( HistoryNavigationData.Num() > 0 && CurNavHistoryIndex != 0 )
	{
		NavHistoryIndexBeforeLastNavCommand = CurNavHistoryIndex;
		CurNavHistoryIndex--;

		// Update the menu to show the new current data item
		UpdateMenu();
		DataToReturn = HistoryNavigationData( CurNavHistoryIndex );
	}

	return DataToReturn;
}

/**
 * Make the history item that follows the current history item the new current history item if possible, and return a pointer to it.
 *
 * @return	Pointer to the current history item after the forward operation has occurred; NULL if there are no navigation history items
 *			or if the user is already at the last entry in the navigation history
 */
FLinkedObjEdNavigationHistoryData* FLinkedObjEdNavigationHistory::Forward()
{
	FLinkedObjEdNavigationHistoryData* DataToReturn = NULL;

	// Can only advance forward if there are data items and the current data item isn't the last one in the array
	if ( HistoryNavigationData.Num() >= 0 && CurNavHistoryIndex != HistoryNavigationData.Num() - 1 )
	{
		NavHistoryIndexBeforeLastNavCommand = CurNavHistoryIndex;
		CurNavHistoryIndex++;

		// Update the menu to show the new current data item
		UpdateMenu();
		DataToReturn = HistoryNavigationData( CurNavHistoryIndex );
	}

	return DataToReturn;
}

/**
 * Make the history item specified by the provided index the current history item if possible, and return a pointer to it.
 *
 * @return	Pointer to the current history item after the jump operation has occurred; NULL if there are no navigation history items
 */
FLinkedObjEdNavigationHistoryData* FLinkedObjEdNavigationHistory::JumpTo( INT InIndex )
{
	check( InIndex >= 0 && InIndex < HistoryNavigationData.Num() && InIndex < MAX_HISTORY_ENTRIES );

	FLinkedObjEdNavigationHistoryData* DataToReturn = NULL;

	// Can only jump to data if the index is valid
	if ( HistoryNavigationData.IsValidIndex( InIndex ) )
	{
		NavHistoryIndexBeforeLastNavCommand = CurNavHistoryIndex;
		CurNavHistoryIndex = InIndex;

		// Update the menu to show the new current data item
		UpdateMenu();
		DataToReturn = HistoryNavigationData( CurNavHistoryIndex );
	}

	return DataToReturn;
}

/**
 * Clear all history items from the navigation history data
 */
void FLinkedObjEdNavigationHistory::ClearHistory()
{
	// Free memory allocated for navigation data
	for ( TArray<FLinkedObjEdNavigationHistoryData*>::TIterator NavHistoryIter( HistoryNavigationData ); NavHistoryIter; ++NavHistoryIter )
	{
		delete *NavHistoryIter;
		*NavHistoryIter = NULL;
	}
	HistoryNavigationData.Empty();

	// Reset the nav indices
	CurNavHistoryIndex = 0;
	NavHistoryIndexBeforeLastNavCommand = 0;
}

/**
 * Const accessor to the current navigation history data item
 *
 * @return	The current navigation history data item, if one exists; NULL otherwise
 */
const FLinkedObjEdNavigationHistoryData* FLinkedObjEdNavigationHistory::GetCurrentNavigationHistoryData() const
{
	return ( ( HistoryNavigationData.Num() > 0 ) ? HistoryNavigationData(CurNavHistoryIndex) : NULL );
}

/**
 * Non-const accessor to the current navigation history data item
 *
 * @return	The current navigation history data item, if one exists; NULL otherwise
 */
FLinkedObjEdNavigationHistoryData* FLinkedObjEdNavigationHistory::GetCurrentNavigationHistoryData()
	{
	return ( ( HistoryNavigationData.Num() > 0 ) ? HistoryNavigationData(CurNavHistoryIndex) : NULL );
}

/**
 * Forcibly remove the specified history item from the navigation system and set the current history item to the last item before a
 * navigation operation if the removed item was the current item. This is used if an editor identifies a data item it has processed
 * as containing invalid data (data refers to objects that were deleted, etc.). The editor can then remove the data from the navigation
 * system, as it is no longer relevant.
 *
 * @param	DataToRemove	History object to remove from the navigation system
 * @param	bWarnUser		If TRUE, a message box pops up informing the user that the data has been forcibly removed
 */
void FLinkedObjEdNavigationHistory::ForceRemoveNavigationHistoryData( FLinkedObjEdNavigationHistoryData* DataToRemove, UBOOL bWarnUser )
{
	// Ensure the provided data is actually in the array
	if ( HistoryNavigationData.Num() > 0 && HistoryNavigationData.ContainsItem( DataToRemove ) )
	{
		// Remove the data from the array, but cache the index it was stored at
		INT IndexToBeRemoved = HistoryNavigationData.FindItemIndex( DataToRemove );
		HistoryNavigationData.RemoveItem( DataToRemove );
		delete DataToRemove;
		
		// If the index the deleted item was stored at occurred before the cached index that signifies the index before the last
		// navigation command, decrement that index in order to keep it correct
		if ( IndexToBeRemoved < NavHistoryIndexBeforeLastNavCommand )
		{
			NavHistoryIndexBeforeLastNavCommand--;
		}

		// If the index the deleted item was stored at is the same as the cached index that signifies the index before the last
		// navigation command, reset that index to the current index, as it'll become invalid after the operation
		else if ( IndexToBeRemoved == NavHistoryIndexBeforeLastNavCommand )
		{
			NavHistoryIndexBeforeLastNavCommand = CurNavHistoryIndex;
		}

		// If the index to be removed is the same as the current data item, set the current data item to be whichever data item
		// was current prior to the last navigation command, under the assumption it is safe
		if ( IndexToBeRemoved == CurNavHistoryIndex )
		{
			CurNavHistoryIndex = NavHistoryIndexBeforeLastNavCommand;
		}
		// If the index to be removed occurred before the current data item, decrement the current data item index to keep the data
		// correct
		else if ( IndexToBeRemoved < CurNavHistoryIndex )
		{
			CurNavHistoryIndex--;
		}
		
		// Update the drop-down menu to signify the deletion and potential current item change
		UpdateMenu();

		// Display a message box warning the user about the deletion if specified by the parameter
		if ( bWarnUser )
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd( TEXT("LinkedObjEditor_NavHistory_ForceRemoveNavDataWarning") ) );
		}
	}
}

/**
 * Update the back/forward buttons to be disabled/enabled, as appropriate. This method is usually forwarded a wxUpdateUIEvent
 * from elsewhere, as the class is not an actual wxWidget-derived object and does not itself receive events automatically. 
 *
 * @param	InEvent	Update UI event specifying which button ID to update
 */
void FLinkedObjEdNavigationHistory::UpdateUI( wxUpdateUIEvent& InEvent )
{
	if ( bAttachedToToolBar )
	{
		switch ( InEvent.GetId() )
		{
			// Disable the back button if the current data item is the first in the array (or there are no data items)
			case IDM_LinkedObjNavHistory_Back:
				InEvent.Enable( HistoryNavigationData.Num() != 0 && CurNavHistoryIndex != 0 );
				break;

			// Disable the forward button if the current data item is the last in the array (or there are no data items)
			case IDM_LinkedObjNavHistory_Forward:
				InEvent.Enable( HistoryNavigationData.Num() != 0 && CurNavHistoryIndex != HistoryNavigationData.Num() - 1 );
				break;
		}
	}
}

/**
 * Internally update the menu that displays the navigation history strings
 */
void FLinkedObjEdNavigationHistory::UpdateMenu()
{
	if ( HistoryMenu )
	{
		// Remove all of the pre-existing entries from the menu
		for ( INT MenuID = IDM_LinkedObjNavHistory_Item_Index_Start; MenuID < IDM_LinkedObjNavHistory_Item_Index_End; ++MenuID )
		{
			if ( HistoryMenu->FindItem( MenuID ) )
			{
				HistoryMenu->Delete( MenuID );
			}
		}

		// Add all of the history data such that the newest data is the top menu choice
		for ( INT DataIndex = HistoryNavigationData.Num() - 1; DataIndex >= 0; --DataIndex )
		{
			const FLinkedObjEdNavigationHistoryData* CurData = HistoryNavigationData( DataIndex );
			check( CurData && CurData->HistoryString.Len() > 0 );

			const INT MenuID = IDM_LinkedObjNavHistory_Item_Index_Start + DataIndex;
			HistoryMenu->Append( MenuID,  *( CurData->HistoryString ) );
		}
		
		// Intentionally disable the current history item to distinguish it from the others
		if ( HistoryMenu->GetMenuItemCount() > 0 )
		{
			HistoryMenu->Enable( IDM_LinkedObjNavHistory_Item_Index_Start + CurNavHistoryIndex, FALSE );
		}
	}
}



/*-----------------------------------------------------------------------------
WxLinkedObjEd
-----------------------------------------------------------------------------*/


BEGIN_EVENT_TABLE( WxLinkedObjEd, WxTrackableFrame )
EVT_SIZE( WxLinkedObjEd::OnSize )
EVT_MENU( IDM_LinkedObjNavHistory_Back, WxLinkedObjEd::OnHistoryBackButton )
EVT_MENU( IDM_LinkedObjNavHistory_Forward, WxLinkedObjEd::OnHistoryForwardButton )
EVT_MENU_RANGE( IDM_LinkedObjNavHistory_Item_Index_Start, IDM_LinkedObjNavHistory_Item_Index_End, WxLinkedObjEd::OnHistoryPulldownMenu )
EVT_UPDATE_UI_RANGE( IDM_LinkedObjNavHistory_Back, IDM_LinkedObjNavHistory_Forward, WxLinkedObjEd::OnUpdateUIForHistory )
END_EVENT_TABLE()


WxLinkedObjEd::WxLinkedObjEd( wxWindow* InParent, wxWindowID InID, const TCHAR* InWinName ) : 
WxTrackableFrame( InParent, InID, InWinName, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | (InParent ? wxFRAME_FLOAT_ON_PARENT : 0) | wxFRAME_NO_TASKBAR ),
FDockingParent(this)
, PropertyWindow(NULL), GraphWindow(NULL), LinkedObjVC(NULL), TreeControl(NULL), TreeImages(NULL), BackgroundTexture(NULL)
{
	WinNameString = FString(InWinName);

	SetTitle( *WinNameString );
	SetName(InWinName);
}

/**
 * This function is called when the window has been selected from within the ctrl + tab dialog.
 */
void WxLinkedObjEd::OnSelected()
{
	this->Raise();
}

void WxLinkedObjEd::CreateControls( UBOOL bTreeControl )
{
	GraphWindow = new WxLinkedObjVCHolder( this, -1, this );
	LinkedObjVC = GraphWindow->LinkedObjVC;

	if(bTreeControl)
	{
		PropertyWindow = new WxPropertyWindowHost;
		PropertyWindow->Create( this, this );
		AddDockingWindow(PropertyWindow, FDockingParent::DH_Bottom, *LocalizeUnrealEd("Properties"));

		CreateTreeControl(this);
		check(TreeControl);
		AddDockingWindow(TreeControl, FDockingParent::DH_Bottom, *LocalizeUnrealEd("Sequences"));
	}
	else
	{
		PropertyWindow = new WxPropertyWindowHost;
		PropertyWindow->Create( this, this );
		AddDockingWindow(PropertyWindow, FDockingParent::DH_Bottom, *LocalizeUnrealEd("Properties"));
	}

	// Add main docking pane.
	AddDockingWindow( GraphWindow, FDockingParent::DH_None, NULL );

	// Try to load a existing layout for the docking windows.
	LoadDockingLayout();
}

WxLinkedObjEd::~WxLinkedObjEd()
{
}

/** 
* Saves Window Properties
*/ 
void WxLinkedObjEd::SaveProperties()
{
	FString KeyName;
	const TCHAR* ConfigName = GetConfigName();

	// Save Window Position and Size
	KeyName = FString::Printf(TEXT("%s_PosSize"), ConfigName);
	FWindowUtil::SavePosSize(KeyName, this);

	// Save Docking Layout
	SaveDockingLayout();
}

/**
* Loads Window Properties
*/
void WxLinkedObjEd::LoadProperties()
{
	FString KeyName;
	const TCHAR* ConfigName = GetConfigName();

	// Load Window Position and Size
	KeyName = FString::Printf(TEXT("%s_PosSize"), ConfigName);
	FWindowUtil::LoadPosSize(KeyName, this, 256, 256, 1024, 768);
	Refresh();

	Layout();
}

/**
 * Creates the tree control for this linked object editor.  Only called if TRUE is specified for bTreeControl
 * in the constructor.
 *
 * @param	TreeParent	the window that should be the parent for the tree control
 */
void WxLinkedObjEd::CreateTreeControl( wxWindow* TreeParent )
{
	TreeImages = new wxImageList( 16, 15 );
	TreeControl = new wxTreeCtrl( TreeParent, IDM_LINKEDOBJED_TREE, wxDefaultPosition, wxSize(100,100), wxTR_HAS_BUTTONS, wxDefaultValidator, TEXT("TreeControl") );
	TreeControl->AssignImageList(TreeImages);
}

/**
 * Used to serialize any UObjects contained that need to be to kept around.
 *
 * @param Ar The archive to serialize with
 */
void WxLinkedObjEd::Serialize(FArchive& Ar)
{
	// Need to call Serialize(Ar) on super class in case we ever move inheritance from FSerializeObject up the chain.
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << BackgroundTexture;
	}
}

void WxLinkedObjEd::OnSize( wxSizeEvent& In )
{
	if ( LinkedObjVC && LinkedObjVC->Viewport )
	{
		LinkedObjVC->Viewport->Invalidate();
	}
	In.Skip();
}

void WxLinkedObjEd::RefreshViewport()
{
	LinkedObjVC->Viewport->Invalidate();
}

void WxLinkedObjEd::DrawObjects(FViewport* Viewport, FCanvas* Canvas)
{
	// draw the background texture if specified
	if (BackgroundTexture != NULL)
	{
		Canvas->PushAbsoluteTransform(FMatrix::Identity);

		Clear(Canvas, FColor(161,161,161) );

		const INT ViewWidth = GraphWindow->GetSize().x;
		const INT ViewHeight = GraphWindow->GetSize().y;

		// draw the texture to the side, stretched vertically
		DrawTile(Canvas, ViewWidth - BackgroundTexture->SizeX, 0,
					  BackgroundTexture->SizeX, ViewHeight,
					  0.f, 0.f,
					  1.f, 1.f,
					  FLinearColor::White,
					  BackgroundTexture->Resource );

		// stretch the left part of the texture to fill the remaining gap
		if (ViewWidth > BackgroundTexture->SizeX)
		{
			DrawTile(Canvas, 0, 0,
						  ViewWidth - BackgroundTexture->SizeX, ViewHeight,
						  0.f, 0.f,
						  0.1f, 0.1f,
						  FLinearColor::White,
						  BackgroundTexture->Resource );
		}

		Canvas->PopTransform();
	}
}

///////////////////////////////////////////////////////////////////////////////////////
// Properties window NotifyHook stuff

void WxLinkedObjEd::NotifyDestroy( void* Src )
{

}

void WxLinkedObjEd::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{
	GEditor->BeginTransaction( *LocalizeUnrealEd("EditLinkedObj") );

	for ( WxPropertyWindow::TObjectIterator Itor( PropertyWindow->ObjectIterator() ) ; Itor ; ++Itor )
	{
		(*Itor)->Modify();
	}
}

void WxLinkedObjEd::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	GEditor->EndTransaction();

	RefreshViewport();
}

void WxLinkedObjEd::NotifyExec( void* Src, const TCHAR* Cmd )
{
	GUnrealEd->NotifyExec(Src, Cmd);
}

//////////////////////////////////////////////////////////////////////////
// FDockingParent Interface
//////////////////////////////////////////////////////////////////////////

/**
 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
 *  @return A string representing a name to use for this docking parent.
 */
const TCHAR* WxLinkedObjEd::GetDockingParentName() const
{
	return GetConfigName();
}

/**
 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
 */
const INT WxLinkedObjEd::GetDockingParentVersion() const
{
	return 0;
}

/**
 * Sets the user's navigation history back one entry, if possible (and processes it).
 */
void WxLinkedObjEd::NavigationHistoryBack()
{
	// Update the last navigation history entry with any relevant changes, because we're about to add switch
	// to a different history object and the changes would otherwise be lost
	UpdateCurrentNavigationHistoryData();
	
	FLinkedObjEdNavigationHistoryData* Data = NavHistory.Back();
	if ( Data )
	{
		if ( !ProcessNavigationHistoryData( Data ) )
		{
			NavHistory.ForceRemoveNavigationHistoryData( Data, TRUE );
		}
	}
}

/**
 * Sets the user's navigation history forward one entry, if possible (and processes it).
 */
void WxLinkedObjEd::NavigationHistoryForward()
{
	// Update the last navigation history entry with any relevant changes, because we're about to add switch
	// to a different history object and the changes would otherwise be lost
	UpdateCurrentNavigationHistoryData();
	
	FLinkedObjEdNavigationHistoryData* Data = NavHistory.Forward();
	if ( Data )
	{
		if ( !ProcessNavigationHistoryData( Data ) )
		{
			NavHistory.ForceRemoveNavigationHistoryData( Data, TRUE );
		}
	}
}

/**
 * Jumps the user's navigation history to the entry at the specified index, if possible (and processes it).
 *
 * @param	InIndex	Index of history entry to jump to
 */
void WxLinkedObjEd::NavigationHistoryJump( INT InIndex )
{
	// Update the last navigation history entry with any relevant changes, because we're about to add switch
	// to a different history object and the changes would otherwise be lost
	UpdateCurrentNavigationHistoryData();
	
	FLinkedObjEdNavigationHistoryData* Data = NavHistory.JumpTo( InIndex );
	if ( Data )
	{
		if ( !ProcessNavigationHistoryData( Data ) )
		{
			NavHistory.ForceRemoveNavigationHistoryData( Data, TRUE );
		}
	}
}

/**
 * Add a new history data item to the user's navigation history, storing the current state
 *
 * @param	InHistoryString		The string that identifies the history data operation and will display in a navigation menu (CANNOT be empty)
 */
void WxLinkedObjEd::AddNewNavigationHistoryDataItem( FString InHistoryString )
{
	check( InHistoryString.Len() > 0 );
	FLinkedObjEdNavigationHistoryData* NewHistoryData = new FLinkedObjEdNavigationHistoryData( InHistoryString );
	check( NewHistoryData );

	// Store the currently set zoom/position by default (can be updated later if desired)
	NewHistoryData->SetPositionAndZoomData( LinkedObjVC->Origin2D.X, LinkedObjVC->Origin2D.Y, LinkedObjVC->Zoom2D );
	
	// Add the new history to the nav history system
	NavHistory.AddHistoryNavigationData( NewHistoryData );
}

/**
 * Update the current history data item of the user's navigation history with any desired changes that have occurred since it was first added,
 * such as camera updates, etc.
 */
void WxLinkedObjEd::UpdateCurrentNavigationHistoryData()
{
	// Get the current history data from the nav history system
	FLinkedObjEdNavigationHistoryData* CurHistoryData = NavHistory.GetCurrentNavigationHistoryData();
	
	// If there's a current history data object, update its camera position with the current camera settings
	if ( CurHistoryData )
	{
		CurHistoryData->SetPositionAndZoomData( LinkedObjVC->Origin2D.X, LinkedObjVC->Origin2D.Y, LinkedObjVC->Zoom2D );
	}
}

/**
 * Process a specified history data object by responding to its contents accordingly (here by adjusting the camera
 * to the specified zoom and position)
 *
 * @param	InData	History data to process
 *
 * @return	TRUE if the navigation history data was successfully processed; FALSE otherwise
 */
UBOOL WxLinkedObjEd::ProcessNavigationHistoryData( const FLinkedObjEdNavigationHistoryData* InData )
{
	// Change viewport client to the specified zoom and position
	LinkedObjVC->Zoom2D = InData->HistoryCameraZoom2D;
	LinkedObjVC->Origin2D.X = InData->HistoryCameraXPosition;
	LinkedObjVC->Origin2D.Y = InData->HistoryCameraYPosition;

	// Update the viewport accordingly
	ViewPosChanged();
	RefreshViewport();

	return TRUE;
}

/**
 * Function called in response to the navigation history back button being pressed
 *
 * @param	InEvent	Event generated by wxWidgets in response to the back button being pressed
 */
void WxLinkedObjEd::OnHistoryBackButton( wxCommandEvent& InEvent )
{
	this->NavigationHistoryBack();
}

/**
 * Function called in response to the navigation history forward button being pressed
 *
 * @param	InEvent	Event generated by wxWidgets in response to the forward button being pressed
 */
void WxLinkedObjEd::OnHistoryForwardButton( wxCommandEvent& InEvent )
{
	this->NavigationHistoryForward();
}

/**
 * Function called in response to a navigation history pull-down menu item being selected
 *
 * @param	InEvent	Event generated by wxWidgets in response to a pull-down menu item being selected
 */
void WxLinkedObjEd::OnHistoryPulldownMenu( wxCommandEvent& InEvent )
{
	this->NavigationHistoryJump( InEvent.GetId() - IDM_LinkedObjNavHistory_Item_Index_Start );
}

/**
 * Function called in response to the wxWidgets update UI event for the back/forward buttons
 *
 * @param	InEvent	Event generated by wxWidgets to update the UI
 */
void WxLinkedObjEd::OnUpdateUIForHistory( wxUpdateUIEvent& InEvent )
{
	NavHistory.UpdateUI( InEvent );
}
