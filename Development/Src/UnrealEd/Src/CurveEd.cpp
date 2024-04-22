/*=============================================================================
	CurveEd.cpp: FInterpCurve editor
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "CurveEd.h"
#include "CurveEdPresetDlg.h"

const INT	LabelWidth			= 200;
const INT	LabelEntryHeight	= 36;
const INT	MaxLabels			= 75;
const INT	ToolbarHeight		= 26;
const INT	ColorKeyWidth		= 6;
const FLOAT ZoomSpeed			= 0.1f;
const FLOAT MouseZoomSpeed		= 0.015f;
const FLOAT HandleLength		= 30.f;
const FLOAT FitMargin			= 0.1f;

UINT CurveEd_ContentHeight		= 0;
INT CurveEd_ContentBoxHeight	= 0;

#define CURVEEDENTRY_HIDECURVE(x)					(x & 0x00000001)
#define CURVEEDENTRY_TOGGLE_HIDECURVE(x)			(x = (x ^ 0x00000001))

#define CURVEEDENTRY_HIDESUBCURVE(x, idx)			(x & (1 << (idx + 1)))
#define CURVEEDENTRY_TOGGLE_HIDESUBCURVE(x, idx)	(x = (x ^ (1 << (idx + 1))))

#define CURVEEDENTRY_SELECTED(x)					(x & 0x80000000)
#define CURVEEDENTRY_TOGGLE_SELECTED(x)				(x = (x ^ 0x80000000))
#define CURVEEDENTRY_SET_SELECTED(x, bSel)			(x = bSel ? (x | 0x80000000) : (x & 0x7fffffff))

IMPLEMENT_CLASS(UCurveEdOptions);

/*-----------------------------------------------------------------------------
	FCurveEdViewportClient
-----------------------------------------------------------------------------*/

FCurveEdViewportClient::FCurveEdViewportClient(WxCurveEditor* InCurveEd)
{
	// This window will be 2D/canvas only, so set the viewport type to None
	ViewportType = LVT_None;

	CurveEd = InCurveEd;

	DragStartMouseX = 0;
	DragStartMouseY = 0;
	OldMouseX = 0;
	OldMouseY = 0;

	bPanning = false;
	bZooming = false;
	bDraggingHandle = false;
	bMouseDown = false;
	bBegunMoving = false;
	MovementAxisLock = ECurveEdMovementAxisLock::None;
	bBoxSelecting = false;
	bKeyAdded = false;

	BoxStartX = 0;
	BoxStartY = 0;
	BoxEndX = 0;
	BoxEndY = 0;

	DistanceDragged = 0;
}

FCurveEdViewportClient::~FCurveEdViewportClient()
{

}


void FCurveEdViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	CurveEd->DrawCurveEditor(Viewport, Canvas);
}

UBOOL FCurveEdViewportClient::InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT /*AmountDepressed*/,UBOOL /*Gamepad*/)
{
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	UBOOL bAltDown = Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt);

	INT HitX = Viewport->GetMouseX();
	INT HitY = Viewport->GetMouseY();
	FIntPoint MousePos = FIntPoint( HitX, HitY );

	if( Key == KEY_LeftMouseButton )
	{
		if( CurveEd->EdMode == CEM_Pan )
		{
			if(Event == IE_Pressed)
			{
				HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);
				if(HitResult)
				{
					if(HitResult->IsA(HCurveEdLabelProxy::StaticGetType()))
					{
						// Notify containing tool that a curve label was clicked on
						if( CurveEd->NotifyObject != NULL )
						{
							const INT CurveIndex = ( (HCurveEdLabelProxy*)HitResult )->CurveIndex;

							FCurveEdEntry& Entry = CurveEd->EdSetup->Tabs( CurveEd->EdSetup->ActiveTab ).Curves( CurveIndex  );

							CurveEd->NotifyObject->OnCurveLabelClicked( Entry.CurveObject );
						}
					}
					else if(HitResult->IsA(HCurveEdHideCurveProxy::StaticGetType()))
					{
						INT CurveIndex = ((HCurveEdHideCurveProxy*)HitResult)->CurveIndex;

						CurveEd->ToggleCurveHidden(CurveIndex);
					}
					else
					if (HitResult->IsA(HCurveEdHideSubCurveProxy::StaticGetType()))
					{
						HCurveEdHideSubCurveProxy* SubCurveProxy = (HCurveEdHideSubCurveProxy*)HitResult;

						INT	CurveIndex		= SubCurveProxy->CurveIndex;
						INT	SubCurveIndex	= SubCurveProxy->SubCurveIndex;

						CurveEd->ToggleSubCurveHidden(CurveIndex, SubCurveIndex);
					}
					else if(HitResult->IsA(HCurveEdKeyProxy::StaticGetType()))
					{
						INT CurveIndex = ((HCurveEdKeyProxy*)HitResult)->CurveIndex;
						INT SubIndex = ((HCurveEdKeyProxy*)HitResult)->SubIndex;
						INT KeyIndex = ((HCurveEdKeyProxy*)HitResult)->KeyIndex;

// 						if(!bCtrlDown)
// 						{
// 							CurveEd->ClearKeySelection();
// 							CurveEd->AddKeyToSelection(CurveIndex, SubIndex, KeyIndex);
// 
// 							if(Event == IE_DoubleClick)
// 							{
// 								CurveEd->DoubleClickedKey(CurveIndex, SubIndex, KeyIndex);
// 							}
// 						}

						if(!bCtrlDown)
						{
							CurveEd->ClearKeySelection();
							if (!bShiftDown)
							{
								CurveEd->AddKeyToSelection(CurveIndex, SubIndex, KeyIndex);
								if(Event == IE_DoubleClick)
								{
									CurveEd->DoubleClickedKey(CurveIndex, SubIndex, KeyIndex);
								}
							}
							else
							{
								FCurveEdEntry& Entry = CurveEd->EdSetup->Tabs(CurveEd->EdSetup->ActiveTab).Curves(CurveIndex);
								if(!CURVEEDENTRY_HIDECURVE(Entry.bHideCurve))
								{
									FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
									if(EdInterface)
									{
										for (INT SubIdx = 0; SubIdx < EdInterface->GetNumSubCurves(); SubIdx++)
										{
											// Holding SHIFT while clicking will select all keys at that point.
											CurveEd->AddKeyToSelection(CurveIndex, SubIdx, KeyIndex);
											if(Event == IE_DoubleClick)
											{
												CurveEd->DoubleClickedKey(CurveIndex, SubIdx, KeyIndex);
											}
										}
									}
								}
							}
						}

						bPanning = true;
					}
					else if(HitResult->IsA(HCurveEdKeyHandleProxy::StaticGetType()))
					{
						HCurveEdKeyHandleProxy* HandleProxy = (HCurveEdKeyHandleProxy*)HitResult;

						CurveEd->HandleCurveIndex = ((HCurveEdKeyHandleProxy*)HitResult)->CurveIndex;
						CurveEd->HandleSubIndex = ((HCurveEdKeyHandleProxy*)HitResult)->SubIndex;
						CurveEd->HandleKeyIndex = ((HCurveEdKeyHandleProxy*)HitResult)->KeyIndex;
						CurveEd->bHandleArriving = ((HCurveEdKeyHandleProxy*)HitResult)->bArriving;

						// Notify a containing tool we are about to move a handle
						if (CurveEd->NotifyObject)
						{
							FCurveEdEntry& Entry = CurveEd->EdSetup->Tabs(CurveEd->EdSetup->ActiveTab).Curves(HandleProxy->CurveIndex);
							TArray<UObject*> CurvesAboutToChange;
							CurvesAboutToChange.AddItem(Entry.CurveObject);
							CurveEd->NotifyObject->PreEditCurve(CurvesAboutToChange);
						}

						bDraggingHandle = true;
					}
					else if(HitResult->IsA(HCurveEdLineProxy::StaticGetType()))
					{
						if(bCtrlDown)
						{
							// Clicking on the line creates a new key.
							INT CurveIndex = ((HCurveEdLineProxy*)HitResult)->CurveIndex;
							INT SubIndex = ((HCurveEdLineProxy*)HitResult)->SubIndex;

							INT NewKeyIndex = CurveEd->AddNewKeypoint( CurveIndex, SubIndex, FIntPoint(HitX, HitY) );

							// Select just this new key straight away so we can drag stuff around.
							if(NewKeyIndex != INDEX_NONE)
							{
								CurveEd->ClearKeySelection();
								CurveEd->AddKeyToSelection(CurveIndex, SubIndex, NewKeyIndex);
								bKeyAdded = true;
							}
						}
						else
						{
							bPanning = true;
						}
					}
				}
				else
				{
					if(bCtrlDown && bAltDown)
					{
						BoxStartX = BoxEndX = HitX;
						BoxStartY = BoxEndY = HitY;

						bBoxSelecting = true;
					}
					else
					{
						bPanning = true;
					}
				}

				DragStartMouseX = OldMouseX = HitX;
				DragStartMouseY = OldMouseY = HitY;
				bHasMouseMovedSinceClick = FALSE;
				bMouseDown = true;
				DistanceDragged = 0;
				Viewport->LockMouseToWindow(true);
				Viewport->Invalidate();
			}
			else if(Event == IE_Released)
			{
				if( !bKeyAdded )
				{
					if( bBoxSelecting )
					{
						INT MinX = Min(BoxStartX, BoxEndX);
						INT MinY = Min(BoxStartY, BoxEndY);
						INT MaxX = Max(BoxStartX, BoxEndX);
						INT MaxY = Max(BoxStartY, BoxEndY);
						INT TestSizeX = MaxX - MinX + 1;
						INT TestSizeY = MaxY - MinY + 1;

						// We read back the hit proxy map for the required region.
						TArray<HHitProxy*> ProxyMap;
						Viewport->GetHitProxyMap((UINT)MinX, (UINT)MinY, (UINT)MaxX, (UINT)MaxY, ProxyMap);

						TArray<FCurveEdSelKey> NewSelection;

						// Find any keypoint hit proxies in the region - add the keypoint to selection.
						for(INT Y=0; Y < TestSizeY; Y++)
						{
							for(INT X=0; X < TestSizeX; X++)
							{
								HHitProxy* HitProxy = ProxyMap(Y * TestSizeX + X);

								if(HitProxy && HitProxy->IsA(HCurveEdKeyProxy::StaticGetType()))
								{
									INT CurveIndex = ((HCurveEdKeyProxy*)HitProxy)->CurveIndex;
									INT SubIndex = ((HCurveEdKeyProxy*)HitProxy)->SubIndex;
									INT KeyIndex = ((HCurveEdKeyProxy*)HitProxy)->KeyIndex;								

									NewSelection.AddItem( FCurveEdSelKey(CurveIndex, SubIndex, KeyIndex) );
								}
							}
						}

						// If shift is down, don't empty, just add to selection.
						if(!bShiftDown)
						{
							CurveEd->ClearKeySelection();
						}

						// Iterate over array adding each to selection.
						for(INT i=0; i<NewSelection.Num(); i++)
						{
							CurveEd->AddKeyToSelection( NewSelection(i).CurveIndex, NewSelection(i).SubIndex, NewSelection(i).KeyIndex );
						}
					}
					else
					{
						HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);

						if( !HitResult && DistanceDragged < 4 )
						{
							CurveEd->ClearKeySelection();
						}
						else if(bCtrlDown)
						{
							if(DistanceDragged < 4)
							{
								if(HitResult && HitResult->IsA(HCurveEdKeyProxy::StaticGetType()))
								{
									INT CurveIndex = ((HCurveEdKeyProxy*)HitResult)->CurveIndex;
									INT SubIndex = ((HCurveEdKeyProxy*)HitResult)->SubIndex;
									INT KeyIndex = ((HCurveEdKeyProxy*)HitResult)->KeyIndex;

									if( CurveEd->KeyIsInSelection(CurveIndex, SubIndex, KeyIndex) )
									{
										if (!bShiftDown)
										{
											CurveEd->RemoveKeyFromSelection(CurveIndex, SubIndex, KeyIndex);
										}
										else
										{
											FCurveEdEntry& Entry = CurveEd->EdSetup->Tabs(CurveEd->EdSetup->ActiveTab).Curves(CurveIndex);
											if(!CURVEEDENTRY_HIDECURVE(Entry.bHideCurve))
											{
												FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
												if(EdInterface)
												{
													for (INT SubIdx = 0; SubIdx < EdInterface->GetNumSubCurves(); SubIdx++)
													{
														CurveEd->RemoveKeyFromSelection(CurveIndex, SubIdx, KeyIndex);
													}
												}
											}
										}
									}
									else
									{
										if (!bShiftDown)
										{
											CurveEd->AddKeyToSelection(CurveIndex, SubIndex, KeyIndex);
											if(Event == IE_DoubleClick)
											{
												CurveEd->DoubleClickedKey(CurveIndex, SubIndex, KeyIndex);
											}
										}
										else
										{
											FCurveEdEntry& Entry = CurveEd->EdSetup->Tabs(CurveEd->EdSetup->ActiveTab).Curves(CurveIndex);
											if(!CURVEEDENTRY_HIDECURVE(Entry.bHideCurve))
											{
												FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
												if(EdInterface)
												{
													for (INT SubIdx = 0; SubIdx < EdInterface->GetNumSubCurves(); SubIdx++)
													{
														// Holding SHIFT while clicking will select all keys at that point.
														CurveEd->AddKeyToSelection(CurveIndex, SubIdx, KeyIndex);
														if(Event == IE_DoubleClick)
														{
															CurveEd->DoubleClickedKey(CurveIndex, SubIdx, KeyIndex);
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}

				if(bBegunMoving)
				{
					CurveEd->EndMoveSelectedKeys();
					bBegunMoving = false;

					// Make sure that movement axis lock is no longer enabled
					MovementAxisLock = ECurveEdMovementAxisLock::None;
				}
			}
		}

		if(Event == IE_Released)
		{
			bMouseDown = false;
			DistanceDragged = 0;
			bPanning = false;
			// Notify a containing tool we are about to move a handle
			if (bDraggingHandle && CurveEd->NotifyObject)
			{
				CurveEd->NotifyObject->PostEditCurve();
			}
			bDraggingHandle = false;
			bBoxSelecting = false;
			bKeyAdded = false;

			Viewport->LockMouseToWindow(false);
			Viewport->Invalidate();
		}
	}
	else if( Key == KEY_RightMouseButton )
	{
		if(Event == IE_Released)
		{
			HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);
			if(HitResult)
			{
				if(HitResult->IsA(HCurveEdLabelProxy::StaticGetType()))
				{
					CurveEd->RightClickCurveIndex = ((HCurveEdLabelProxy*)HitResult)->CurveIndex;

					WxMBCurveLabelMenu Menu( CurveEd );
					FTrackPopupMenu tpm( CurveEd, &Menu );
					tpm.Show();
				}
				else if(HitResult->IsA(HCurveEdKeyProxy::StaticGetType()))
				{
					if( CurveEd->EdMode == CEM_Pan)
					{
						INT CurveIndex = ((HCurveEdKeyProxy*)HitResult)->CurveIndex;
						INT SubIndex = ((HCurveEdKeyProxy*)HitResult)->SubIndex;
						INT KeyIndex = ((HCurveEdKeyProxy*)HitResult)->KeyIndex;

						if( !CurveEd->KeyIsInSelection(CurveIndex, SubIndex, KeyIndex) )
						{
							CurveEd->ClearKeySelection();
							CurveEd->AddKeyToSelection(CurveIndex, SubIndex, KeyIndex);
						}

						WxMBCurveKeyMenu Menu( CurveEd );
						FTrackPopupMenu tpm( CurveEd, &Menu );
						tpm.Show();
					}
				}
			}
			else
			{
				if (CurveEd->EdMode != CEM_Zoom)
				{
					// Show the general context menu
					WxMBCurveMenu Menu(CurveEd);
					FTrackPopupMenu tpm( CurveEd, &Menu );
					tpm.Show();
				}
			}
		}
	}
	else if (((Key == KEY_MouseScrollDown) || (Key == KEY_MouseScrollUp && Event == IE_Pressed)) && (Event == IE_Pressed))
	{
		//down = zoom out.
		FLOAT Direction = (Key == KEY_MouseScrollDown) ? -1.0f : 1.0f;
		FLOAT SizeIn = CurveEd->EndIn - CurveEd->StartIn;
		FLOAT DeltaIn = ZoomSpeed * SizeIn * Direction;

		FLOAT SizeOut = CurveEd->EndOut - CurveEd->StartOut;
		FLOAT DeltaOut = ZoomSpeed * SizeOut * Direction;

		FLOAT NewStartIn = CurveEd->StartIn + DeltaIn;
		FLOAT NewEndIn = CurveEd->EndIn - DeltaIn;
		FLOAT NewStartOut = CurveEd->StartOut + DeltaOut;
		FLOAT NewEndOut = CurveEd->EndOut - DeltaOut;

		if (GEditorModeTools().GetCenterZoomAroundCursor())
		{
			FLOAT MouseX = Viewport->GetMouseX()-LabelWidth;
			FLOAT MouseY = Viewport->GetMouseY();
			FLOAT ViewportWidth = Viewport->GetSizeX()-LabelWidth;
			FLOAT ViewportHeight = Viewport->GetSizeY();			
			check(ViewportWidth > 0);
			check(ViewportHeight > 0);
			
			//(Keep left side the same) - at viewport x = 0, offset is -DeltaIn
			//(Stay Centered) - at viewport x = viewport->Getwidth/2, offset is 0
			//(Keep right side the same) - at viewport x = viewport->Getwidth, offset is DeltaIn
			FLOAT OffsetX = ((MouseX / ViewportWidth)-.5f)*2.0f*DeltaIn;
			//negate why to account for y being inverted
			FLOAT OffsetY = -((MouseY / ViewportHeight)-.5f)*2.0f*DeltaOut;

			NewStartIn += OffsetX;
			NewEndIn += OffsetX;
			NewStartOut += OffsetY;
			NewEndOut += OffsetY;
		}

		CurveEd->SetCurveView(NewStartIn, NewEndIn, NewStartOut, NewEndOut);
		Viewport->Invalidate();
	}
	else if(Event == IE_Pressed)
	{
		if( Key == KEY_Delete )
		{
			CurveEd->DeleteSelectedKeys();
		}
		else if( Key == KEY_Z && bCtrlDown )
		{
			if(CurveEd->NotifyObject)
			{
				CurveEd->NotifyObject->DesireUndo();
			}
		}
		else if( Key == KEY_Y && bCtrlDown)
		{
			if(CurveEd->NotifyObject)
			{
				CurveEd->NotifyObject->DesireRedo();
			}
		}
		else if( Key == KEY_Z )
		{
			if(!bBoxSelecting && !bBegunMoving && !bDraggingHandle)
			{
				CurveEd->EdMode = CEM_Zoom;
				CurveEd->ToolBar->SetEditMode(CEM_Zoom);
			}
		}
		else if ((Key == KEY_F) && bCtrlDown)
		{
			if(!bBoxSelecting && !bBegunMoving && !bDraggingHandle)
			{
				CurveEd->FitViewToAll();
			}
		}
		else if ((Key == KEY_D) && bCtrlDown)
		{
			if(!bBoxSelecting && !bBegunMoving && !bDraggingHandle)
			{
				CurveEd->FitViewToSelected();
			}
		}
        else
        {
			// Handle hotkey bindings.
			UUnrealEdOptions* UnrealEdOptions = GUnrealEd->GetUnrealEdOptions();

			if(UnrealEdOptions)
			{
				FString Cmd = UnrealEdOptions->GetExecCommand(Key, bAltDown, bCtrlDown, bShiftDown, TEXT("CurveEditor"));

				if(Cmd.Len())
				{
					Exec(*Cmd);
				}
			}
        }
	}
	else if(Event == IE_Released)
	{
		if( Key == KEY_Z )
		{
			CurveEd->EdMode = CEM_Pan;
			CurveEd->ToolBar->SetEditMode(CEM_Pan);
		}
	}

	// Handle viewport screenshot.
	InputTakeScreenshot( Viewport, Key, Event );

	return TRUE;
}

void FCurveEdViewportClient::MouseMove(FViewport* Viewport, INT X, INT Y)
{
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);

	INT DeltaX = OldMouseX - X;
	OldMouseX = X;

	INT DeltaY = OldMouseY - Y;
	OldMouseY = Y;

	// Update mouse-over keypoint.
	HHitProxy* HitResult = Viewport->GetHitProxy(X,Y);
	if(HitResult && HitResult->IsA(HCurveEdKeyProxy::StaticGetType()))
	{
		CurveEd->MouseOverCurveIndex = ((HCurveEdKeyProxy*)HitResult)->CurveIndex;
		CurveEd->MouseOverSubIndex = ((HCurveEdKeyProxy*)HitResult)->SubIndex;
		CurveEd->MouseOverKeyIndex = ((HCurveEdKeyProxy*)HitResult)->KeyIndex;
	}
	else
	{
		CurveEd->MouseOverCurveIndex = INDEX_NONE;
		CurveEd->MouseOverSubIndex = INDEX_NONE;
		CurveEd->MouseOverKeyIndex = INDEX_NONE;
	}

	// If in panning mode, do moving/panning stuff.
	if(CurveEd->EdMode == CEM_Pan)
	{
		if(bMouseDown)
		{
			//only left mouse button can cause this
			if ((DeltaX || DeltaY) && (Viewport->KeyState(KEY_LeftMouseButton)))
			{
				MarkMouseMovedSinceClick();
			}

			// Update total milage of mouse cursor while button is pressed.
			DistanceDragged += ( Abs<INT>(DeltaX) + Abs<INT>(DeltaY) );

			// Distance mouse just moved in 'curve' units.
			FLOAT DeltaIn = DeltaX / CurveEd->PixelsPerIn;
			FLOAT DeltaOut = -DeltaY / CurveEd->PixelsPerOut;

			// If we are panning around, update the Start/End In/Out values for this view.
			if(bDraggingHandle)
			{
				FVector2D HandleVal = CurveEd->CalcValuePoint( FIntPoint(X,Y) );
				CurveEd->MoveCurveHandle(HandleVal);
			}
			else if(bBoxSelecting)
			{
				BoxEndX = X;
				BoxEndY = Y;
			}
			else if(bCtrlDown)
			{
				if(CurveEd->SelectedKeys.Num() > 0 && DistanceDragged > 4)
				{
					if(!bBegunMoving)
					{
						CurveEd->BeginMoveSelectedKeys();
						bBegunMoving = true;

						// If the Shift key is held down when the user starts dragging, then we'll
						// lock key movement to the specified axis
						MovementAxisLock = ECurveEdMovementAxisLock::None;
						if( bShiftDown )
						{
							// Set movement axis lock based on the user's mouse position
							if( Abs( X - DragStartMouseX ) > Abs( Y - DragStartMouseY ) )
							{
								MovementAxisLock = ECurveEdMovementAxisLock::Horizontal;
							}
							else
							{
								MovementAxisLock = ECurveEdMovementAxisLock::Vertical;
							}
						}
					}

					CurveEd->MoveSelectedKeys(
						MovementAxisLock == ECurveEdMovementAxisLock::Vertical ? 0 : -DeltaIn,
						MovementAxisLock == ECurveEdMovementAxisLock::Horizontal ? 0 : -DeltaOut );
				}
			}
			else if(bPanning)
			{
				CurveEd->SetCurveView(CurveEd->StartIn + DeltaIn, CurveEd->EndIn + DeltaIn, CurveEd->StartOut + DeltaOut, CurveEd->EndOut + DeltaOut );
			}
		}
		else
		{
			//no mouse down = no grab hand
			bHasMouseMovedSinceClick = FALSE;
		}
	}
	// Otherwise we are in zooming mode, so look at mouse buttons and update viewport size.
	else if(CurveEd->EdMode == CEM_Zoom)
	{
		UBOOL bLeftMouseDown = Viewport->KeyState( KEY_LeftMouseButton );
		UBOOL bRightMouseDown = Viewport->KeyState( KEY_RightMouseButton );

		FLOAT ZoomDeltaIn = 0.f;
		if(bRightMouseDown)
		{
			FLOAT SizeIn = CurveEd->EndIn - CurveEd->StartIn;
			ZoomDeltaIn = MouseZoomSpeed * SizeIn * Clamp<INT>(DeltaX - DeltaY, -5, 5);		
		}

		FLOAT ZoomDeltaOut = 0.f;
		if(bLeftMouseDown)
		{
			FLOAT SizeOut = CurveEd->EndOut - CurveEd->StartOut;
			ZoomDeltaOut = MouseZoomSpeed * SizeOut * Clamp<INT>(-DeltaY + DeltaX, -5, 5);
		}

		CurveEd->SetCurveView(CurveEd->StartIn - ZoomDeltaIn, CurveEd->EndIn + ZoomDeltaIn, CurveEd->StartOut - ZoomDeltaOut, CurveEd->EndOut + ZoomDeltaOut );
	}

	Viewport->Invalidate();
}

UBOOL FCurveEdViewportClient::InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad)
{
	if ( Key == KEY_MouseX || Key == KEY_MouseY )
	{
		INT X = Viewport->GetMouseX();
		INT Y = Viewport->GetMouseY();
		MouseMove(Viewport, X, Y);
		return TRUE;
	}
	return FALSE;
}

/** Exec handler */
void FCurveEdViewportClient::Exec(const TCHAR* Cmd)
{
	const TCHAR* Str = Cmd;
	
	if(ParseCommand(&Str, TEXT("CURVEEDITOR")))
	{
		if( ParseCommand(&Str, TEXT("ChangeInterpModeAUTO")) )
		{
			CurveEd->ChangeInterpMode(CIM_CurveAuto);
		}
		if( ParseCommand(&Str, TEXT("ChangeInterpModeAUTOCLAMPED")) )
		{
			CurveEd->ChangeInterpMode(CIM_CurveAutoClamped);
		}
		else if( ParseCommand(&Str, TEXT("ChangeInterpModeUSER")) )
		{
			CurveEd->ChangeInterpMode(CIM_CurveUser);
		}
		else if( ParseCommand(&Str, TEXT("ChangeInterpModeBREAK")) )
		{
			CurveEd->ChangeInterpMode(CIM_CurveBreak);
		}
		else if( ParseCommand(&Str, TEXT("ChangeInterpModeLINEAR")) )
		{
			CurveEd->ChangeInterpMode(CIM_Linear);
		}
		else if( ParseCommand(&Str, TEXT("ChangeInterpModeCONSTANT")) )
		{
			CurveEd->ChangeInterpMode(CIM_Constant);
		}
		else if( ParseCommand( &Str, TEXT( "FitViewHorizontally" ) ) )
		{
			CurveEd->FitViewHorizontally();
		}
		else if( ParseCommand( &Str, TEXT( "FitViewVertically" ) ) )
		{
			CurveEd->FitViewVertically();
		}
		else if( ParseCommand( &Str, TEXT( "FitViewToAll" ) ) )
		{
			CurveEd->FitViewToAll();
		}
		else if( ParseCommand( &Str, TEXT( "FitViewToSelected" ) ) )
		{
			CurveEd->FitViewToSelected();
		}
	}
}



/*-----------------------------------------------------------------------------
	WxCurveEditor
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxCurveEditor, wxWindow )
	EVT_SIZE( WxCurveEditor::OnSize )
	EVT_TOOL( IDM_CURVE_FITHORZ, WxCurveEditor::OnFitHorz )
	EVT_TOOL( IDM_CURVE_FITVERT, WxCurveEditor::OnFitVert )
	EVT_TOOL( IDM_CURVE_FitViewToAll, WxCurveEditor::OnFitViewToAll )
	EVT_TOOL( IDM_CURVE_FitViewToSelected, WxCurveEditor::OnFitViewToSelected )
	EVT_MENU( IDM_CURVE_REMOVECURVE, WxCurveEditor::OnContextCurveRemove )
	EVT_MENU( IDM_CURVE_REMOVEALLCURVES, WxCurveEditor::OnContextCurveRemoveAll )
	EVT_MENU( IDM_CURVE_PRESETCURVE, WxCurveEditor::OnPresetCurves )
	EVT_MENU( IDM_CURVE_SAVE_PRESETCURVE, WxCurveEditor::OnSavePresetCurves )
	EVT_MENU( IDM_CURVE_CurveLabelContext_UpgradeInterpMethod, WxCurveEditor::OnCurveLabelContext_UpgradeInterpMethod )
	EVT_MENU_RANGE( IDM_CURVE_PANMODE, IDM_CURVE_ZOOMMODE, WxCurveEditor::OnChangeMode )
	EVT_MENU_RANGE( IDM_CURVE_SETKEYIN, IDM_CURVE_SETKEYOUT, WxCurveEditor::OnSetKey )
	EVT_MENU( IDM_CURVE_SETKEYCOLOR, WxCurveEditor::OnSetKeyColor )
	EVT_MENU( IDM_CURVE_DeleteSelectedKeys, WxCurveEditor::OnDeleteSelectedKeys )
	EVT_MENU( IDM_CURVE_SCALETIMES, WxCurveEditor::OnScaleTimes )
	EVT_MENU( IDM_CURVE_SCALEVALUES, WxCurveEditor::OnScaleValues )
	EVT_MENU_RANGE( IDM_CURVE_MODE_AUTO, IDM_CURVE_MODE_CONSTANT, WxCurveEditor::OnChangeInterpMode )
	EVT_MENU( IDM_CURVE_FLATTEN_TANGENTS_TO_AXIS, WxCurveEditor::OnFlattenTangentsToAxis )
	EVT_MENU( IDM_CURVE_STRAIGHTEN_TANGENTS, WxCurveEditor::OnStraightenTangents )
	EVT_MENU( IDM_CURVE_ShowAllCurveTangents, WxCurveEditor::OnShowAllCurveTangents )
	EVT_UPDATE_UI( IDM_CURVE_ShowAllCurveTangents, WxCurveEditor::UpdateShowAllCurveTangentsUI )
	EVT_MENU(IDM_CURVE_TAB_CREATE, WxCurveEditor::OnTabCreate)
	EVT_COMBOBOX(IDM_CURVE_TABCOMBO, WxCurveEditor::OnChangeTab)
	EVT_MENU(IDM_CURVE_TAB_DELETE, WxCurveEditor::OnTabDelete)
	EVT_SCROLL(WxCurveEditor::OnScroll)
	EVT_MOUSEWHEEL(WxCurveEditor::OnMouseWheel)
END_EVENT_TABLE()

WxCurveEditor::WxCurveEditor( wxWindow* InParent, wxWindowID InID, UInterpCurveEdSetup* InEdSetup )
: wxWindow( InParent, InID )
{
	CurveEdVC = new FCurveEdViewportClient(this);

	//
	CurveEdVC->Viewport = GEngine->Client->CreateWindowChildViewport(CurveEdVC, (HWND)GetHandle());
	CurveEdVC->Viewport->CaptureJoystickInput(false);

	EditorOptions = ConstructObject<UCurveEdOptions>(UCurveEdOptions::StaticClass());
	check(EditorOptions);
	MinViewRange = EditorOptions->MinViewRange;
	MaxViewRange = EditorOptions->MaxViewRange;
	BackgroundColor = EditorOptions->BackgroundColor;
	LabelColor = EditorOptions->LabelColor;
	SelectedLabelColor = EditorOptions->SelectedLabelColor;
	GridColor = EditorOptions->GridColor;
	GridTextColor = EditorOptions->GridTextColor;
	LabelBlockBkgColor = EditorOptions->LabelBlockBkgColor;
	SelectedKeyColor = EditorOptions->SelectedKeyColor;

	ToolBar = new WxCurveEdToolBar( this, -1 );

	EdSetup = InEdSetup;


	// Scroll bar starts at the top of the list!
	ThumbPos_Vert	= 0;

	// Create the vertical scroll bar.  We want this on the LEFT side, so the tracks line up in Matinee
	ScrollBar_Vert = new wxScrollBar(this, ID_CURVE_VERT_SCROLL_BAR, wxDefaultPosition, wxDefaultSize, wxSB_VERTICAL);

	// Setup the initial metrics for the scroll bar
	AdjustScrollBar();


	RightClickCurveIndex = INDEX_NONE;

	EdMode = CEM_Pan;

	MouseOverCurveIndex = INDEX_NONE;
	MouseOverSubIndex = INDEX_NONE;
	MouseOverKeyIndex = INDEX_NONE;

	HandleCurveIndex = INDEX_NONE;
	HandleSubIndex = INDEX_NONE;
	HandleKeyIndex = INDEX_NONE;
	bHandleArriving = false;

	bShowPositionMarker = false;
	MarkerPosition = 0.f;
	MarkerColor = FColor(255,255,255);

	bShowEndMarker = false;
	EndMarkerPosition = 0.f;

	bSnapToFrames = FALSE;
	bShowRegionMarker = false;
	RegionStart = 0.f;
	RegionEnd = 0.f;
	RegionFillColor = FColor(255,255,255,16);

	bShowAllCurveTangents = FALSE;

	bSnapEnabled = false;
	InSnapAmount = 1.f;

	NotifyObject = NULL;

	StartIn = EdSetup->Tabs( EdSetup->ActiveTab ).ViewStartInput;
	EndIn = EdSetup->Tabs( EdSetup->ActiveTab ).ViewEndInput;
	StartOut = EdSetup->Tabs( EdSetup->ActiveTab ).ViewStartOutput;
	EndOut = EdSetup->Tabs( EdSetup->ActiveTab ).ViewEndOutput;

	LabelOrigin2D.X	= 0;
	LabelOrigin2D.Y	= 0;

	FillTabCombo();

	wxSizeEvent DummyEvent;
	OnSize( DummyEvent );

	PresetDialog	= new WxCurveEdPresetDlg(this);
}

WxCurveEditor::~WxCurveEditor()
{
	GEngine->Client->CloseViewport(CurveEdVC->Viewport);
	CurveEdVC->Viewport = NULL;
	delete CurveEdVC;
	delete ScrollBar_Vert;
	delete PresetDialog;
}

void WxCurveEditor::OnSize( wxSizeEvent& In )
{
	// Window dimensions may have changed, so we'll update the scroll bar
	AdjustScrollBar();

	// Make sure the toolbar size is into account when updating the window dimensions
	wxRect ClientRect = GetClientRect();
	wxRect ViewportRect = ClientRect;
	if( ToolBar != NULL )
	{
		ViewportRect.y += ToolbarHeight;
		ViewportRect.height -= ToolbarHeight;
	}

	// Make sure the track window's group list is positioned to the right of the scroll bar
	if( ScrollBar_Vert != NULL )
	{
		ViewportRect.x += ScrollBar_Vert->GetClientRect().GetWidth();
		ViewportRect.width -= ScrollBar_Vert->GetRect().GetWidth();
	}

	// Update the window position and size
	::MoveWindow( (HWND)CurveEdVC->Viewport->GetWindow(), ViewportRect.x, ViewportRect.y, ViewportRect.GetWidth(), ViewportRect.GetHeight(), 1 );

	// Also update the toolbar size
	if( ToolBar != NULL )
	{
		ToolBar->SetSize( ClientRect.GetWidth(), ToolbarHeight );
	}
}



/** Fits the curve editor view horizontally to the curve data */
void WxCurveEditor::FitViewHorizontally()
{
	FLOAT MinIn = BIG_NUMBER;
	FLOAT MaxIn = -BIG_NUMBER;

	// Iterate over all curves to find the max/min Input values.
	for(INT i=0; i<EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num(); i++)
	{
		FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(i);
		if(!CURVEEDENTRY_HIDECURVE(Entry.bHideCurve))
		{
			FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
			if(EdInterface)
			{
				FLOAT EntryMinIn, EntryMaxIn;
				EdInterface->GetInRange(EntryMinIn, EntryMaxIn);

				MinIn = ::Min<FLOAT>(EntryMinIn, MinIn);
				MaxIn = ::Max<FLOAT>(EntryMaxIn, MaxIn);
			}
		}
	}

	FLOAT Size = MaxIn - MinIn;

	// Clamp the minimum size
	if( Size < MinViewRange )
	{
//		MinIn -= MinViewRange * 0.5f;
//		MaxIn += MaxViewRange * 0.5f;
		MinIn -= 0.005f;
		MaxIn += 0.005f;
		Size = MaxIn - MinIn;
	}

	SetCurveView( MinIn - FitMargin*Size, MaxIn + FitMargin*Size, StartOut, EndOut );

	CurveEdVC->Viewport->Invalidate();
}



/** Fits the curve editor view vertically to the curve data */
void WxCurveEditor::FitViewVertically()
{
	FLOAT MinOut = BIG_NUMBER;
	FLOAT MaxOut = -BIG_NUMBER;

	for(INT i=0; i<EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num(); i++)
	{
		FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(i);
		if(!CURVEEDENTRY_HIDECURVE(Entry.bHideCurve))
		{
			FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
			if(EdInterface)
			{
				FLOAT EntryMinOut = BIG_NUMBER;
				FLOAT EntryMaxOut = -BIG_NUMBER;

				// Iterate over each subcurve - only looking at points which are shown
				INT NumSubCurves = EdInterface->GetNumSubCurves();
				for(INT SubIndex = 0; SubIndex < EdInterface->GetNumSubCurves(); SubIndex++)
				{
					if(!CURVEEDENTRY_HIDESUBCURVE(Entry.bHideCurve, SubIndex))
					{
						// If we can see this curve - iterate over keys to find min and max 'out' value
						for(INT KeyIndex = 0; KeyIndex < EdInterface->GetNumKeys(); KeyIndex++)
						{
							FLOAT OutVal = EdInterface->GetKeyOut(SubIndex, KeyIndex);
							EntryMinOut = ::Min<FLOAT>(EntryMinOut, OutVal);
							EntryMaxOut = ::Max<FLOAT>(EntryMaxOut, OutVal);
						}
					}
				}

				// Update overall min and max
				MinOut = ::Min<FLOAT>(EntryMinOut, MinOut);
				MaxOut = ::Max<FLOAT>(EntryMaxOut, MaxOut);
			}
		}
	}

	FLOAT Size = MaxOut - MinOut;

	// Clamp the minimum size
	if( Size < MinViewRange )
	{
//		MinOut -= MinViewRange * 0.5f;
//		MaxOut += MaxViewRange * 0.5f;
		MinOut -= 0.005f;
		MaxOut += 0.005f;
		Size = MaxOut - MinOut;
	}


	SetCurveView( StartIn, EndIn, MinOut - FitMargin*Size, MaxOut + FitMargin*Size );

	CurveEdVC->Viewport->Invalidate();
}



/** Fits the view (horizontally and vertically) to the all curve data */
void WxCurveEditor::FitViewToAll()
{
	FitViewHorizontally();
	FitViewVertically();
}



/** Fits the view (horizontally and vertically) to the currently selected keys */
void WxCurveEditor::FitViewToSelected()
{
	FLOAT MinOut = BIG_NUMBER;
	FLOAT MaxOut = -BIG_NUMBER;
	FLOAT MinIn = BIG_NUMBER;
	FLOAT MaxIn = -BIG_NUMBER;

	for( INT i = 0; i < SelectedKeys.Num(); ++i )
	{
		FCurveEdSelKey& SelKey = SelectedKeys( i );

		FCurveEdEntry& CurveEntry = EdSetup->Tabs( EdSetup->ActiveTab ).Curves( SelKey.CurveIndex );
		FCurveEdInterface* CurveInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer( CurveEntry );

		if( !CURVEEDENTRY_HIDESUBCURVE( CurveEntry.bHideCurve, SelKey.SubIndex ) )
		{
			const FLOAT KeyIn = CurveInterface->GetKeyIn( SelKey.KeyIndex );
			const FLOAT KeyOut = CurveInterface->GetKeyOut( SelKey.SubIndex, SelKey.KeyIndex );

			// Update overall min and max
			MinOut = ::Min<FLOAT>( KeyOut, MinOut );
			MaxOut = ::Max<FLOAT>( KeyOut, MaxOut );
			MinIn = ::Min<FLOAT>( KeyIn, MinIn );
			MaxIn = ::Max<FLOAT>( KeyIn, MaxIn );
		}
	}

	FLOAT SizeOut = MaxOut - MinOut;
	FLOAT SizeIn = MaxIn - MinIn;

	// Clamp the minimum size
	if( SizeOut < MinViewRange )
	{
		MinOut -= MinViewRange * 0.5f;
		MaxOut += MinViewRange * 0.5f;
		SizeOut = MaxOut - MinOut;
	}
	if( SizeIn < MinViewRange )
	{
		MinIn -= MinViewRange * 0.5f;
		MaxIn += MinViewRange * 0.5f;
		SizeIn = MaxIn - MinIn;
	}

	SetCurveView(
		MinIn - FitMargin * SizeIn,
		MaxIn + FitMargin * SizeIn,
		MinOut - FitMargin * SizeOut,
		MaxOut + FitMargin * SizeOut );

	CurveEdVC->Viewport->Invalidate();
}



void WxCurveEditor::OnFitHorz(wxCommandEvent& In)
{
	FitViewHorizontally();
}



void WxCurveEditor::OnFitVert(wxCommandEvent& In)
{
	FitViewVertically();
}



/** Fits the view (horizontally and vertically) to the all curve data */
void WxCurveEditor::OnFitViewToAll( wxCommandEvent& In )
{
	FitViewToAll();
}



/** Fits the view (horizontally and vertically) to the currently selected keys */
void WxCurveEditor::OnFitViewToSelected( wxCommandEvent& In )
{
	FitViewToSelected();
}



/** Remove a Curve from the selected tab. */
void WxCurveEditor::OnContextCurveRemove(wxCommandEvent& In)
{
	if( RightClickCurveIndex < 0 || RightClickCurveIndex >= EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num() )
	{
		return;
	}

	EdSetup->Tabs(EdSetup->ActiveTab).Curves.Remove(RightClickCurveIndex);

	// Window content's may have changed, so update our scroll bar
	AdjustScrollBar();

	ClearKeySelection();

	CurveEdVC->Viewport->Invalidate();
}

void WxCurveEditor::OnContextCurveRemoveAll(wxCommandEvent& In)
{
	UBOOL bShouldPromptOnCurveRemoveAll;
	GConfig->GetBool(TEXT("CurveEditor"), TEXT("bShouldPromptOnCurveRemoveAll"), bShouldPromptOnCurveRemoveAll, GEditorIni);

	if (!bShouldPromptOnCurveRemoveAll || appMsgf(AMT_YesNo, *LocalizeUnrealEd("RemoveAllCurvesPrompt")))
	{
		for (INT TabIndex = 0; TabIndex < EdSetup->Tabs.Num(); TabIndex++)
		{
			EdSetup->Tabs(TabIndex).Curves.Empty();
		}

		// Window content's may have changed, so update our scroll bar
		AdjustScrollBar();
		ClearKeySelection();
		CurveEdVC->Viewport->Invalidate();
	}
}

void WxCurveEditor::OnChangeMode(wxCommandEvent& In)
{
	if(In.GetId() == IDM_CURVE_PANMODE)
	{
		EdMode = CEM_Pan;
		ToolBar->SetEditMode(CEM_Pan);

	}
	else
	{
		EdMode = CEM_Zoom;
		ToolBar->SetEditMode(CEM_Zoom);
	}
}

/** Set the input or output value of a key by entering a text value. */
void WxCurveEditor::OnSetKey(wxCommandEvent& In)
{
	// Only works on single key...
	if(SelectedKeys.Num() != 1)
		return;

	// Are we setting the input or output value
	UBOOL bSetIn = (In.GetId() == IDM_CURVE_SETKEYIN);

	// Find the EdInterface for this curve.
	FCurveEdSelKey& SelKey = SelectedKeys(0);
	FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(SelKey.CurveIndex);
	FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);

	// Set the default text to the current input/output.
	FString DefaultNum, TitleString, CaptionString;
	if(bSetIn)
	{
		DefaultNum = FString::Printf( TEXT("%2.3f"), EdInterface->GetKeyIn(SelKey.KeyIndex) );
		TitleString = FString(TEXT("SetTime"));
		CaptionString = FString(TEXT("NewTime"));
	}
	else
	{
		DefaultNum = FString::Printf( TEXT("%f"), EdInterface->GetKeyOut(SelKey.SubIndex, SelKey.KeyIndex) );
		TitleString = FString(TEXT("SetValue"));
		CaptionString = FString(TEXT("NewValue"));
	}

	// Show generic string entry dialog box
	WxDlgGenericStringEntry dlg;
	INT Result = dlg.ShowModal( *TitleString, *CaptionString, *DefaultNum );
	if( Result != wxID_OK )
		return;

	// Convert from string to float (if we can).
	double dNewNum;
	const UBOOL bIsNumber = dlg.GetStringEntry().GetValue().ToDouble(&dNewNum);
	if(!bIsNumber)
		return;
	FLOAT NewNum = (FLOAT)dNewNum;

	if (NotifyObject)
	{
		// Make a list of all curves we are going to remove keys from.
		TArray<UObject*> CurvesAboutToChange;
		if(Entry.CurveObject)
		{
			CurvesAboutToChange.AddUniqueItem( Entry.CurveObject );
			// Notify a containing tool that keys are about to be removed
			NotifyObject->PreEditCurve(CurvesAboutToChange);
		}
	}

	// Set then set using EdInterface.
	if(bSetIn)
	{
		EdInterface->SetKeyIn(SelKey.KeyIndex, NewNum);
	}
	else
	{
		if (Entry.bClamp)
		{
			NewNum = Clamp<FLOAT>(NewNum, Entry.ClampLow, Entry.ClampHigh);
		}
		EdInterface->SetKeyOut(SelKey.SubIndex, SelKey.KeyIndex, NewNum);
	}

	if (NotifyObject)
	{
		NotifyObject->PostEditCurve();
	}

	CurveEdVC->Viewport->Invalidate();
}

/** Kind of special case function for setting a color keyframe. Will affect all 3 sub-tracks. */
void WxCurveEditor::OnSetKeyColor(wxCommandEvent& In)
{
	// Only works on single key...
	if(SelectedKeys.Num() != 1)
		return;

	// Find the EdInterface for this curve.
	FCurveEdSelKey& SelKey = SelectedKeys(0);
	FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(SelKey.CurveIndex);
	if(!Entry.bColorCurve)
		return;

	// We only do this special case if curve has 3 sub-curves.
	FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
	if(EdInterface->GetNumSubCurves() != 3)
		return;

	if (NotifyObject)
	{
		// Make a list of all curves we are going to remove keys from.
		TArray<UObject*> CurvesAboutToChange;
		if(Entry.CurveObject)
		{
			CurvesAboutToChange.AddUniqueItem( Entry.CurveObject );
			// Notify a containing tool that keys are about to be removed
			NotifyObject->PreEditCurve(CurvesAboutToChange);
		}
	}

	// Get current value of curve as a colour.
	FColor InputColor;
	if (Entry.bFloatingPointColorCurve)
	{
		FLOAT Value;

		Value	= EdInterface->GetKeyOut(0, SelKey.KeyIndex) * 255.9f;
		InputColor.R = appTrunc(Value);
		Value	= EdInterface->GetKeyOut(1, SelKey.KeyIndex) * 255.9f;
		InputColor.G = appTrunc(Value);
		Value	= EdInterface->GetKeyOut(2, SelKey.KeyIndex) * 255.9f;
		InputColor.B = appTrunc(Value);
	}
	else
	{
		InputColor.R = appTrunc( Clamp<FLOAT>(EdInterface->GetKeyOut(0, SelKey.KeyIndex), 0.f, 255.9f) );
		InputColor.G = appTrunc( Clamp<FLOAT>(EdInterface->GetKeyOut(1, SelKey.KeyIndex), 0.f, 255.9f) );
		InputColor.B = appTrunc( Clamp<FLOAT>(EdInterface->GetKeyOut(2, SelKey.KeyIndex), 0.f, 255.9f) );
	}

	//since the data isn't stored in standard colors, a temp color is used
	FColor TempColor = InputColor;

	FPickColorStruct PickColorStruct;
	PickColorStruct.RefreshWindows.AddItem(this);
	PickColorStruct.DWORDColorArray.AddItem(&TempColor);
	PickColorStruct.bModal = TRUE;
	
	if (PickColor(PickColorStruct) == ColorPickerConstants::ColorAccepted)
	{
		FLOAT Value;
		if (Entry.bFloatingPointColorCurve)
		{
			Value	= (FLOAT)TempColor.R / 255.9f;
			if (Entry.bClamp)
			{
				Value = Clamp<FLOAT>(Value, Entry.ClampLow, Entry.ClampHigh);
			}
			EdInterface->SetKeyOut(0, SelKey.KeyIndex, Value);
			Value	= (FLOAT)TempColor.G / 255.9f;
			if (Entry.bClamp)
			{
				Value = Clamp<FLOAT>(Value, Entry.ClampLow, Entry.ClampHigh);
			}
			EdInterface->SetKeyOut(1, SelKey.KeyIndex, Value);
			Value	= (FLOAT)TempColor.B / 255.9f;
			if (Entry.bClamp)
			{
				Value = Clamp<FLOAT>(Value, Entry.ClampLow, Entry.ClampHigh);
			}
			EdInterface->SetKeyOut(2, SelKey.KeyIndex, Value);
		}
		else
		{
			Value = (FLOAT)(TempColor.R);
			if (Entry.bClamp)
			{
				Value = Clamp<FLOAT>(Value, Entry.ClampLow, Entry.ClampHigh);
			}
			EdInterface->SetKeyOut(0, SelKey.KeyIndex, Value);
			Value = (FLOAT)(TempColor.G);
			if (Entry.bClamp)
			{
				Value = Clamp<FLOAT>(Value, Entry.ClampLow, Entry.ClampHigh);
			}
			EdInterface->SetKeyOut(1, SelKey.KeyIndex, Value);
			Value = (FLOAT)(TempColor.B);
			if (Entry.bClamp)
			{
				Value = Clamp<FLOAT>(Value, Entry.ClampLow, Entry.ClampHigh);
			}
			EdInterface->SetKeyOut(2, SelKey.KeyIndex, Value);
		}
	}

	if (NotifyObject)
	{
		NotifyObject->PostEditCurve();
	}

	CurveEdVC->Viewport->Invalidate();
}



/** Called to delete the currently selected keys */
void WxCurveEditor::OnDeleteSelectedKeys( wxCommandEvent& In )
{
	// Delete selected keys
	DeleteSelectedKeys();
}

/** Helper function to handle undo/redo */
UBOOL WxCurveEditor::NotifyPendingCurveChange(UBOOL bSelectedOnly)
{
	if (NotifyObject)
	{
		// Make a list of all curves we are going to remove keys from.
		TArray<UObject*> CurvesAboutToChange;
		for (INT CurveIdx = 0; CurveIdx < EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num(); CurveIdx++)
		{
			FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(CurveIdx);
			if (Entry.CurveObject)
			{
				CurvesAboutToChange.AddUniqueItem(Entry.CurveObject);
			}
		}
		// Notify a containing tool that keys are about to be removed
		NotifyObject->PreEditCurve(CurvesAboutToChange);

		return TRUE;
	}

	return FALSE;
}

/**
 *	Retrieve a scalar value from the user.
 *
 *	@param	InPrompt	The prompt to display to the user
 *	@param	InDefault	The default number to display
 *	@param	OutScalar	The user-select scalar value
 *	
 *	@return	UBOOL		TRUE if successful, FALSE if not
 */
UBOOL WxCurveEditor::GetScalarValue(FString& InPrompt, FLOAT InDefault, FLOAT& OutScalar)
{
	// Set the default text to the current input/output.
	FString DefaultNum, TitleString, CaptionString;
	DefaultNum = FString::Printf(TEXT("%2.1f"), InDefault);
	TitleString = FString(TEXT("EnterScalar"));

	// Show generic string entry dialog box
	WxDlgGenericStringEntry dlg;
	INT Result = dlg.ShowModal(*TitleString, *InPrompt, *DefaultNum );
	if (Result != wxID_OK)
	{
		return FALSE;
	}

	// Convert from string to float (if we can).
	double dNewNum;
	const UBOOL bIsNumber = dlg.GetStringEntry().GetValue().ToDouble(&dNewNum);
	if (!bIsNumber)
	{
		return FALSE;
	}

	OutScalar = (FLOAT)dNewNum;
	return TRUE;
}

/** Called to scale the times by a user-select value */
void WxCurveEditor::OnScaleTimes( wxCommandEvent& In )
{
	FLOAT ScaleByValue = 1.0f;
	FString Prompt = TEXT("ScaleTimesPrompt");
	if (GetScalarValue(Prompt, 1.0f, ScaleByValue) == TRUE)
	{
		UBOOL bNotified = NotifyPendingCurveChange(FALSE);

		// Scale the In values by the selected scalar
		for (INT CurveIdx = 0; CurveIdx < EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num(); CurveIdx++)
		{
			FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(CurveIdx);
			FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
			if(EdInterface)
			{
				// For each key
				if (ScaleByValue >= 1.0f)
				{
					for (INT KeyIndex = EdInterface->GetNumKeys() - 1; KeyIndex >= 0; KeyIndex--)
					{
						FLOAT InVal = EdInterface->GetKeyIn(KeyIndex);
						EdInterface->SetKeyIn(KeyIndex, InVal * ScaleByValue);
					}
				}
				else
				{
					for (INT KeyIndex = 0; KeyIndex < EdInterface->GetNumKeys(); KeyIndex++)
					{
						FLOAT InVal = EdInterface->GetKeyIn(KeyIndex);
						EdInterface->SetKeyIn(KeyIndex, InVal * ScaleByValue);
					}
				}
			}
		}

		if (bNotified && NotifyObject)
		{
			NotifyObject->PostEditCurve();
		}

		CurveEdVC->Viewport->Invalidate();
	}
}

/** Called to scale the values by a user-select value */
void WxCurveEditor::OnScaleValues( wxCommandEvent& In )
{
	FLOAT ScaleByValue = 1.0f;
	FString Prompt = TEXT("ScaleValuesPrompt");
	if (GetScalarValue(Prompt, 1.0f, ScaleByValue) == TRUE)
	{
		UBOOL bNotified = NotifyPendingCurveChange(FALSE);

		// Scale the In values by the selected scalar
		for (INT CurveIdx = 0; CurveIdx < EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num(); CurveIdx++)
		{
			FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(CurveIdx);
			FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
			if(EdInterface)
			{
				// For each sub-curve
				for (INT SubIndex = 0; SubIndex < EdInterface->GetNumSubCurves(); SubIndex++)
				{
					// For each key
					for (INT KeyIndex = 0; KeyIndex < EdInterface->GetNumKeys(); KeyIndex++)
					{
						FLOAT OutVal = EdInterface->GetKeyOut(SubIndex, KeyIndex);
						EdInterface->SetKeyOut(SubIndex, KeyIndex, OutVal * ScaleByValue);
					}
				}
			}
		}

		if (bNotified && NotifyObject)
		{
			NotifyObject->PostEditCurve();
		}

		CurveEdVC->Viewport->Invalidate();
	}
}

/** Set the viewable area for the current tab. */
void WxCurveEditor::OnChangeInterpMode(wxCommandEvent& In)
{
	EInterpCurveMode NewInterpMode = CIM_Unknown;
	if(In.GetId() == IDM_CURVE_MODE_AUTO)
	{
		NewInterpMode = CIM_CurveAuto;
	}
	else if(In.GetId() == IDM_CURVE_MODE_AUTO_CLAMPED)
	{
		NewInterpMode = CIM_CurveAutoClamped;
	}
	else if(In.GetId() == IDM_CURVE_MODE_USER)
	{
		NewInterpMode = CIM_CurveUser;
	}
	else if(In.GetId() == IDM_CURVE_MODE_BREAK)
	{
		NewInterpMode = CIM_CurveBreak;
	}
	else if(In.GetId() == IDM_CURVE_MODE_LINEAR)
	{
		NewInterpMode = CIM_Linear;
	}
	else if(In.GetId() == IDM_CURVE_MODE_CONSTANT)
	{
		NewInterpMode = CIM_Constant;
	}
	else
	{
		check(0);
	}

	for(INT i=0; i<SelectedKeys.Num(); i++)
	{
		FCurveEdSelKey& SelKey = SelectedKeys(i);

		FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(SelKey.CurveIndex);
		FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);		

		EdInterface->SetKeyInterpMode(SelKey.KeyIndex, NewInterpMode);
	}

	CurveEdVC->Viewport->Invalidate();

	UpdateInterpModeButtons();
}



/** Toggles showing all curve tangents */
void WxCurveEditor::OnShowAllCurveTangents( wxCommandEvent& In )
{
	bShowAllCurveTangents = !bShowAllCurveTangents;

	CurveEdVC->Viewport->Invalidate();
}



/** Updates UI state for the 'show all curve tangents' button */
void WxCurveEditor::UpdateShowAllCurveTangentsUI( wxUpdateUIEvent& In )
{
	In.Check( bShowAllCurveTangents ? true : false );
}

						 

void WxCurveEditor::OnTabCreate(wxCommandEvent& In)
{
	// Prompt the user for the name of the tab
	WxDlgGenericStringEntry dlg;
	if (dlg.ShowModal(TEXT("CreateTab"), TEXT("Name"), TEXT("")) == wxID_OK)
	{
		FString	newName = dlg.GetEnteredString();
		UBOOL	bFound	= false;

		// Verify that the name is not already in use
		for (INT ii = 0; ii < EdSetup->Tabs.Num(); ii++)
		{
			FCurveEdTab* Tab = &EdSetup->Tabs(ii);
			if (Tab->TabName == newName)
			{
				appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("TabNameInUse"), *newName));
				bFound	= true;
				break;
			}
		}

		if (!bFound)
		{
			// Add the tab, and set the active tab to it.
			INT NewIndex = ToolBar->TabCombo->GetCount();
			EdSetup->CreateNewTab(newName);
			FillTabCombo();
			ToolBar->TabCombo->SetSelection(NewIndex);
			check(EdSetup->Tabs.Num() == (NewIndex + 1));
			EdSetup->ActiveTab = NewIndex;

			// We may have changed tabs, so update the scroll bar
			AdjustScrollBar();

			CurveEdVC->Viewport->Invalidate();
		}
	}
}

void WxCurveEditor::OnChangeTab(wxCommandEvent& In)
{
	INT Index = ToolBar->TabCombo->GetSelection();
	if ((Index >= 0) && (Index < EdSetup->Tabs.Num()))
	{
		EdSetup->ActiveTab = Index;

		FCurveEdTab* Tab = &(EdSetup->Tabs(Index));
		SetCurveView(Tab->ViewStartInput, Tab->ViewEndInput, Tab->ViewStartOutput, Tab->ViewEndOutput);

		// We may have changed tabs, so update the scroll bar
		AdjustScrollBar();

		CurveEdVC->Viewport->Invalidate();
	}
}

void WxCurveEditor::OnTabDelete(wxCommandEvent& In)
{
	if (ToolBar->TabCombo->GetSelection() == 0)
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("DefaultTabCannotBeDeleted"));
		return;
	}

	// Remove the tab...
	FString Name = (const TCHAR*)ToolBar->TabCombo->GetStringSelection();
	EdSetup->RemoveTab(Name);

	// Force a reset of the combo contents
	ToolBar->TabCombo->SetSelection(0);
	FillTabCombo();

	// We may have changed tabs, so update the scroll bar
	AdjustScrollBar();

	CurveEdVC->Viewport->Invalidate();
}

/** Allow usage of preset curves					*/
void WxCurveEditor::OnPresetCurves(wxCommandEvent& In)
{
	check(CURVEED_MAX_CURVES == WxCurveEdPresetDlg::CEP_CURVE_MAX);

	for (INT ClearIndex = 0; ClearIndex < CURVEED_MAX_CURVES; ClearIndex++)
	{
		GeneratedPoints[ClearIndex].Empty();
		RequiredKeyInTimes[ClearIndex].Empty();
		CopiedCurves[ClearIndex].Empty();
	}

	if ((RightClickCurveIndex < 0) || 
		(RightClickCurveIndex >= EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num()))
	{
		return;
	}

	FCurveEdEntry* Entry	= &(EdSetup->Tabs(EdSetup->ActiveTab).Curves(RightClickCurveIndex));
	if (Entry == NULL)
	{
		return;
	}

	INT	ValidCurves	= DetermineValidPresetFlags(Entry);
	if (ValidCurves == 0)
	{
		// That means it's not a curve!
		return;
	}

	// Generate the list of available presets
	if (GeneratePresetClassList() == FALSE)
	{
		warnf(TEXT("Failed to find any preset classes??"));
	}

	if (SetupDistributionVariables(Entry) == FALSE)
	{
		warnf(TEXT("Failed to setup the distribution variables??"));
	}

	// Show the preset dialog...
	if (PresetDialog == NULL)
	{
		PresetDialog	= new WxCurveEdPresetDlg(this);
	}

	if (PresetDialog)
	{
		PresetDialog->ShowDialog(TRUE, *(Entry->CurveName), ValidCurves, CurveEdPresets);
	}

	CurveEdVC->Viewport->Invalidate();
}

/** Allow saving preset curves								*/
void WxCurveEditor::OnSavePresetCurves(wxCommandEvent& In)
{
	check(CURVEED_MAX_CURVES == WxCurveEdPresetDlg::CEP_CURVE_MAX);

	for (INT ClearIndex = 0; ClearIndex < CURVEED_MAX_CURVES; ClearIndex++)
	{
		GeneratedPoints[ClearIndex].Empty();
		RequiredKeyInTimes[ClearIndex].Empty();
		CopiedCurves[ClearIndex].Empty();
	}

	if ((RightClickCurveIndex < 0) || 
		(RightClickCurveIndex >= EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num()))
	{
		return;
	}

	FCurveEdEntry* Entry	= &(EdSetup->Tabs(EdSetup->ActiveTab).Curves(RightClickCurveIndex));
	if (Entry == NULL)
	{
		return;
	}

	INT	ValidCurves	= DetermineValidPresetFlags(Entry);
	if (ValidCurves == 0)
	{
		// That means it's not a curve!
		return;
	}

	// Generate the list of available presets
	if (GeneratePresetClassList(TRUE) == FALSE)
	{
		warnf(TEXT("Failed to find any preset classes??"));
	}

	if (SetupDistributionVariables(Entry) == FALSE)
	{
		warnf(TEXT("Failed to setup the distribution variables??"));
	}

	// Prompt for user setup parameters
	// Show the preset dialog...
	if (PresetDialog == NULL)
	{
		PresetDialog	= new WxCurveEdPresetDlg(this);
	}

	if (PresetDialog)
	{
		PresetDialog->ShowDialog(TRUE, *(Entry->CurveName), ValidCurves, CurveEdPresets, TRUE);
	}
}



/** Called when 'Upgrade Curve Tangents' is clicked in the curve label context menu */
void WxCurveEditor::OnCurveLabelContext_UpgradeInterpMethod( wxCommandEvent& In )
{
	// Grab curve editor interface for the curve entry that was right clicked on
	if( ( RightClickCurveIndex >= 0 ) &&
		( RightClickCurveIndex < EdSetup->Tabs( EdSetup->ActiveTab ).Curves.Num() ) )
	{
		FCurveEdEntry* Entry = &( EdSetup->Tabs( EdSetup->ActiveTab ).Curves( RightClickCurveIndex ) );
		if( Entry != NULL )
		{
			FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer( *Entry );
			if( EdInterface != NULL )
			{
				// OK, now check to see if this curve is using 'legacy' tangents
				if( EdInterface->UsingLegacyInterpMethod() )
				{
					// Go ahead and upgrade the curve!  All sub-curves will be affected.
					EdInterface->UpgradeInterpMethod();

					// Mark the package as dirty
					Entry->CurveObject->MarkPackageDirty();

					// Interp modes changed, so update the toolbar
					UpdateInterpModeButtons();

					// Curve shape may have changed, so redraw the viewport
					if( CurveEdVC->Viewport )
					{
						CurveEdVC->Viewport->Invalidate();
					}
				}
			}
		}
	}
}



/** Handle scrolling								*/
void WxCurveEditor::OnScroll(wxScrollEvent& In)
{
    wxScrollBar* InScrollBar = wxDynamicCast(In.GetEventObject(), wxScrollBar);
    if (InScrollBar) 
	{
		ThumbPos_Vert = In.GetPosition();
		LabelOrigin2D.Y = -ThumbPos_Vert;
		LabelOrigin2D.Y = Min(0, LabelOrigin2D.Y);

		if (CurveEdVC->Viewport)
		{
			CurveEdVC->Viewport->Invalidate();
			// Force it to draw so the view change is seen
			CurveEdVC->Viewport->Draw();
		}
	}
}

/** Handle the mouse wheel moving					*/
void WxCurveEditor::OnMouseWheel(wxMouseEvent& In)
{
}

/** Update the scroll bar position					*/
void WxCurveEditor::UpdateScrollBar(INT Vert)
{
	ThumbPos_Vert = -Vert;
	ScrollBar_Vert->SetThumbPosition(ThumbPos_Vert);
}



/**
 * Updates the scroll bar for the current state of the window's size and content layout.  This should be called
 *  when either the window size changes or the vertical size of the content contained in the window changes.
 */
void WxCurveEditor::AdjustScrollBar()
{
	if( ScrollBar_Vert != NULL )
	{
		// Grab the height of the client window
		wxRect rc = GetClientRect();
		const UINT ViewportHeight = rc.GetHeight();

		// Compute scroll bar layout metrics
		CurveEd_ContentHeight = ComputeCurveLabelEntryListContentHeight();

		// Compute size of content area (where the actual curves, label entries and scroll bar live)
		INT ContentBoxTopY = 0;
		CurveEd_ContentBoxHeight = rc.GetHeight();
		if( ToolBar != NULL )
		{
			ContentBoxTopY += ToolbarHeight;
			CurveEd_ContentBoxHeight -= ToolbarHeight;
		}

		// Configure the scroll bar's position and size
		wxRect rcSBV = ScrollBar_Vert->GetClientRect();
		ScrollBar_Vert->SetSize( 0, ContentBoxTopY, rcSBV.GetWidth(), CurveEd_ContentBoxHeight );

		SetScrollBarPosition();
	}
}

/** Set the position of the scroll bar... */
void WxCurveEditor::SetScrollBarPosition()
{
	// The current scroll bar position
	const INT ScrollBarPos = ThumbPos_Vert;

	// The thumb size is the number of 'scrollbar units' currently visible
	const UINT ScrollBarThumbSize = CurveEd_ContentBoxHeight;

	// The size of a 'scrollbar page'.  This is how much to scroll when paging up and down.
	const UINT ScrollBarPageSize = ScrollBarThumbSize;

	// Configure the scroll bar layout metrics
	ScrollBar_Vert->SetScrollbar(
		ScrollBarPos,				// Position
		ScrollBarThumbSize,			// Thumb size
		CurveEd_ContentHeight,		// Range
		ScrollBarPageSize );		// Page size
}

/** Fill the tab selection combo. */
void WxCurveEditor::FillTabCombo()
{
	if (!ToolBar)
	{
		return;
	}

	check(EdSetup && EdSetup->Tabs.Num());

	INT	CurrSelection = ToolBar->TabCombo->GetSelection();
	
	if (CurrSelection == -1)
	{
		CurrSelection = 0;
	}

	ToolBar->TabCombo->Clear();
	for (INT ii = 0; ii < EdSetup->Tabs.Num(); ii++)
	{
		FCurveEdTab* Tab = &EdSetup->Tabs(ii);
		ToolBar->TabCombo->Append(*(Tab->TabName));
	}
	ToolBar->TabCombo->SetSelection(CurrSelection);
	EdSetup->ActiveTab = CurrSelection;

	// OK, now that our data is all filled in, make sure the scroll bar is up to date!
	AdjustScrollBar();
}

/** Determine the valid preset flags for the given CurveEdEntry		*/
INT	WxCurveEditor::DetermineValidPresetFlags(FCurveEdEntry* Entry)
{
	INT ValidFlags	= 0;

	if (Entry != NULL)
	{
		UDistributionFloatConstantCurve*	FloatConstantCurveDist	= Cast<UDistributionFloatConstantCurve>(Entry->CurveObject);
		UDistributionFloatUniformCurve*		FloatUniformCurveDist	= Cast<UDistributionFloatUniformCurve>(Entry->CurveObject);
		UDistributionVectorConstantCurve*	VectorConstantCurveDist	= Cast<UDistributionVectorConstantCurve>(Entry->CurveObject);
		UDistributionVectorUniformCurve*	VectorUniformCurveDist	= Cast<UDistributionVectorUniformCurve>(Entry->CurveObject);

		if (FloatConstantCurveDist)
		{
			ValidFlags	= WxCurveEdPresetDlg::VCL_FloatConstantCurve;
		}
		else
		if (FloatUniformCurveDist)
		{
			ValidFlags	= WxCurveEdPresetDlg::VCL_FloatUniformCurve;
		}
		else
		if (VectorConstantCurveDist)
		{
			ValidFlags	= WxCurveEdPresetDlg::VCL_VectorConstantCurve;
		}
		else
		if (VectorUniformCurveDist)
		{
			//@todo. This is disabling presets for Vector uniform curves...
			// Once the issue is resolved internally, re-enable them!!!!
			ValidFlags	= WxCurveEdPresetDlg::VCL_VectorUniformCurve;
//			debugf(TEXT("VectorUniformCurve presets are temporarily disabled!"));
//			ValidFlags	= 0;
		}
	}

	return ValidFlags;
}

/** Generate the list of CurveEdPreset classes					*/
UBOOL WxCurveEditor::GeneratePresetClassList(UBOOL bIsSaving)
{
	CurveEdPresets.Empty();
	for (TObjectIterator<UClass> It; It; ++It)
	{
		// Find all CurveEdPreset classes (ignoring abstract)
		if (It->IsChildOf(UCurveEdPresetBase::StaticClass()))
		{
			if (!(It->ClassFlags & CLASS_Abstract))
			{
				if (appStrcmp(*It->GetName(), TEXT("CurveEdPreset_Nothing")) == 0)
				{
					CurveEdPresets.InsertItem(*It, 0);
				}
				else
				{
					if (bIsSaving)
					{
						if (appStrcmp(*It->GetName(), TEXT("CurveEdPreset_UserSet")) == 0)
						{
							CurveEdPresets.AddItem(*It);
						}
					}
					else
					{
						CurveEdPresets.AddItem(*It);
					}
				}
			}
		}
	}

#if defined(_PRESETS_DEBUG_ENABLED_)
	for (INT PresetIndex = 0; PresetIndex < CurveEdPresets.Num(); PresetIndex++)
	{
		debugf(TEXT("Preset %2d - %s"), PresetIndex, CurveEdPresets(PresetIndex)->GetName());
	}
#endif	//#if defined(_PRESETS_DEBUG_ENABLED_)

	if (CurveEdPresets.Num() == 0)
	{
		return FALSE;
	}

	return TRUE;
}

UBOOL WxCurveEditor::SetupDistributionVariables(FCurveEdEntry* Entry)
{
	if (Entry == NULL)
	{
		return FALSE;
	}

	FloatCC		= Cast<UDistributionFloatConstantCurve>(Entry->CurveObject);
	FloatUC		= Cast<UDistributionFloatUniformCurve>(Entry->CurveObject);
	VectorCC	= Cast<UDistributionVectorConstantCurve>(Entry->CurveObject);
	VectorUC	= Cast<UDistributionVectorUniformCurve>(Entry->CurveObject);

	bMinMaxValid	= (FloatUC || VectorUC) ? TRUE : FALSE;
	bFloatDist		= (FloatCC || FloatUC) ? TRUE : FALSE;

	Distribution	= UInterpCurveEdSetup::GetCurveEdInterfacePointer(*Entry);
	FloatDist		= Cast<UDistributionFloat>(Entry->CurveObject);
	VectorDist		= Cast<UDistributionVector>(Entry->CurveObject);

	return TRUE;
}

/** */
UBOOL WxCurveEditor::GetSetupDataAndRequiredKeyInPoints(WxCurveEdPresetDlg* PresetDlg)
{
	INT BlockIndex, KeyInIndex;

	for (BlockIndex = 0; BlockIndex < WxCurveEdPresetDlg::CEP_CURVE_MAX; BlockIndex++)
	{
		if (PresetDialog->IsCurveValid(BlockIndex) == FALSE)
		{
			continue;
		}

		UCurveEdPresetBase*	PresetClass = PresetDlg->GetPreset(BlockIndex);
		check(PresetClass);
		if (PresetClass->IsA(UCurveEdPreset_Nothing::StaticClass()))
		{
			// Save the values and put the KeyIn values into the array...
			CopiedCurves[BlockIndex].Empty();
			RequiredKeyInTimes[BlockIndex].Empty();
			for (KeyInIndex = 0; KeyInIndex < Distribution->GetNumKeys(); KeyInIndex++)
			{
				INT NewIndex = RequiredKeyInTimes[BlockIndex].Add(1);
				RequiredKeyInTimes[BlockIndex](NewIndex)	= Distribution->GetKeyIn(KeyInIndex);
			}
		}
		else
		if (PresetClass->eventFetchRequiredKeyInTimes(RequiredKeyInTimes[BlockIndex]) == FALSE)
		{
			warnf(TEXT("Failed to fetch required key in times for %s"), *PresetClass->GetName());
		}

#if defined(_PRESETS_DEBUG_ENABLED_)
		debugf(TEXT("Curve %d = %s"), BlockIndex, *PresetClass->GetName());
		debugf(TEXT("\tRequired KeyIns:"));
		for (KeyInIndex = 0; KeyInIndex < RequiredKeyInTimes[BlockIndex].Num(); KeyInIndex++)
		{
			debugf(TEXT("\t\t%2d --> %f"), KeyInIndex, RequiredKeyInTimes[BlockIndex](KeyInIndex));
		}
#endif	//#if defined(_PRESETS_DEBUG_ENABLED_)
	}

	return TRUE;
}

/** */
UBOOL WxCurveEditor::GenerateCompleteKeyInList()
{
	INT	BlockIndex, KeyInIndex;

	TArray<FLOAT>	TempCompleteKeyInTimes;
	TempCompleteKeyInTimes.Empty();

	for (BlockIndex = 0; BlockIndex < WxCurveEdPresetDlg::CEP_CURVE_MAX; BlockIndex++)
	{
		for (KeyInIndex = 0; KeyInIndex < RequiredKeyInTimes[BlockIndex].Num(); KeyInIndex++)
		{
			TempCompleteKeyInTimes.AddUniqueItem(RequiredKeyInTimes[BlockIndex](KeyInIndex));
		}
	}

	CompleteKeyInTimes.Empty();
	for (BlockIndex = 0; BlockIndex < TempCompleteKeyInTimes.Num(); BlockIndex++)
	{
		FLOAT	Value	= TempCompleteKeyInTimes(BlockIndex);
		INT		Count	= CompleteKeyInTimes.Num();

		INT		CheckIndex;
		if (Count > 0)
		{
			for (CheckIndex = 0; CheckIndex < CompleteKeyInTimes.Num(); CheckIndex++)
			{
				if (CompleteKeyInTimes(CheckIndex) > Value)
				{
					break;
				}
			}

			CompleteKeyInTimes.InsertZeroed(CheckIndex);
			CompleteKeyInTimes(CheckIndex)	= Value;
		}
		else
		{
			CompleteKeyInTimes.InsertZeroed(0);
			CompleteKeyInTimes(0) = TempCompleteKeyInTimes(0);
		}
	}

#if defined(_PRESETS_DEBUG_ENABLED_)
	debugf(TEXT("CompleteKeyInTimes:"));
	for (BlockIndex = 0; BlockIndex < CompleteKeyInTimes.Num(); BlockIndex++)
	{
		debugf(TEXT("\t%d --> %f"), BlockIndex, CompleteKeyInTimes(BlockIndex));
	}
#endif	//#if defined(_PRESETS_DEBUG_ENABLED_)

	return TRUE;
}

/** */
UBOOL WxCurveEditor::GeneratePresetSamples(WxCurveEdPresetDlg* PresetDlg)
{
	UBOOL bResult = TRUE;

	INT	BlockIndex, PointIndex;

	for (BlockIndex = 0; BlockIndex < WxCurveEdPresetDlg::CEP_CURVE_MAX; BlockIndex++)
	{
		GeneratedPoints[BlockIndex].Empty();
	}

	for (BlockIndex = 0; BlockIndex < WxCurveEdPresetDlg::CEP_CURVE_MAX; BlockIndex++)
	{
		UCurveEdPresetBase*	PresetClass = PresetDlg->GetPreset(BlockIndex);
		check(PresetClass);
		if (PresetClass->IsA(UCurveEdPreset_Nothing::StaticClass()) == FALSE)
		{
			if (PresetClass->eventGenerateCurveData(CompleteKeyInTimes, GeneratedPoints[BlockIndex]) == FALSE)
			{
				bResult = FALSE;
			}
		}
		else
		{
			// Save the values and put the KeyIn values into the array...
			CopiedCurves[BlockIndex].Empty();

			INT	SubIndex = BlockIndex;
			if (bFloatDist)
			{
				if (bMinMaxValid)
				{
					if (BlockIndex == 3)
					{
						SubIndex = 1;
					}
					else
					if (BlockIndex == 0)
					{
					}
					else
					{
						continue;
					}
				}
				else
				{
					if (BlockIndex != 0)
					{
						continue;
					}
				}
			}
			else
			{
				if (bMinMaxValid == FALSE)
				{
					if (BlockIndex >= 3)
					{
						continue;
					}
				}
				else
				{
					switch (BlockIndex)
					{
					case 0:		SubIndex	= 0;	break;
					case 1:		SubIndex	= 3;	break;
					case 2:		SubIndex	= 1;	break;
					case 3:		SubIndex	= 4;	break;
					case 4:		SubIndex	= 2;	break;
					case 5:		SubIndex	= 5;	break;
					default:	continue;
					}
				}
			}

			for (PointIndex = 0; PointIndex < CompleteKeyInTimes.Num(); PointIndex++)
			{
				FLOAT	KeyIn	= CompleteKeyInTimes(PointIndex);
				FLOAT	KeyOut	= 0.f;

				INT NewIndex = CopiedCurves[BlockIndex].Add(1);
				FPresetGeneratedPoint* Generated = &(CopiedCurves[BlockIndex](NewIndex));

				Generated->KeyIn		= KeyIn;

				if (FloatCC)
				{
					KeyOut	= FloatCC->GetValue(KeyIn);
				}
				else
				if (FloatUC)
				{
					FVector2D	TempVal	= FloatUC->GetMinMaxValue(KeyIn);

					if (SubIndex == 0)
					{
						KeyOut	= TempVal.X;
					}
					else
					{
						KeyOut	= TempVal.Y;
					}
				}
				else
				if (VectorCC)
				{
					FVector	TempVal	= VectorCC->GetValue(KeyIn);
					KeyOut	= TempVal[SubIndex];
				}
				else
				if (VectorUC)
				{
					FTwoVectors	TempVal	= VectorUC->GetMinMaxValue(KeyIn);
					if (BlockIndex < 3)
					{
						KeyOut	= TempVal.v1[SubIndex - 0];
					}
					else
					{
						KeyOut	= TempVal.v2[SubIndex - 3];
					}
				}

				Generated->KeyOut			= KeyOut;
				Generated->TangentsValid	= FALSE;
			}
		}
	}

	return TRUE;
}

/** */
INT WxCurveEditor::DetermineSubCurveIndex(INT CurveIndex)
{
	INT	SubIndex = -1;
	if (bFloatDist)
	{
		if (bMinMaxValid == TRUE)
		{
			if (CurveIndex == 0)
			{
				SubIndex = 0;
			}
			else
			if (CurveIndex == 3)
			{
				SubIndex = 1;
			}
			else
			{
				// Error...
			}
		}
		else
		{
			if (CurveIndex == 0)
			{
				SubIndex = 0;
			}
			else
			{
				// Error...
			}
		}
	}
	else
	{
		if (bMinMaxValid == TRUE)
		{
			switch (CurveIndex)
			{
			case 0:		SubIndex = 0;		break;
			case 1:		SubIndex = 2;		break;
			case 2:		SubIndex = 4;		break;
			case 3:		SubIndex = 1;		break;
			case 4:		SubIndex = 3;		break;
			case 5:		SubIndex = 5;		break;
			}
		}
		else
		{
			if (CurveIndex < 3)
			{
				SubIndex = CurveIndex;
			}
			else
			{
				SubIndex = -1;
			}
		}
	}

	return SubIndex;
}

/** Set the viewable area for the current tab. */
void WxCurveEditor::SetCurveView(FLOAT InStartIn, FLOAT InEndIn, FLOAT InStartOut, FLOAT InEndOut)
{
	// Ensure we are not zooming too big or too small...
	FLOAT InSize = InEndIn - InStartIn;
	FLOAT OutSize = InEndOut - InStartOut;
	if( InSize < MinViewRange  || InSize > MaxViewRange || OutSize < MinViewRange || OutSize > MaxViewRange)
	{
		return;
	}

	StartIn		= EdSetup->Tabs( EdSetup->ActiveTab ).ViewStartInput	= InStartIn;
	EndIn		= EdSetup->Tabs( EdSetup->ActiveTab ).ViewEndInput		= InEndIn;
	StartOut	= EdSetup->Tabs( EdSetup->ActiveTab ).ViewStartOutput	= InStartOut;
	EndOut		= EdSetup->Tabs( EdSetup->ActiveTab ).ViewEndOutput		= InEndOut;
}

INT WxCurveEditor::AddNewKeypoint( INT InCurveIndex, INT InSubIndex, const FIntPoint& ScreenPos )
{
	check( InCurveIndex >= 0 && InCurveIndex < EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num() );

	FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(InCurveIndex);
	FVector2D NewKeyVal = CalcValuePoint(ScreenPos);
	FLOAT NewKeyIn = SnapIn(NewKeyVal.X);

	FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);

	INT NewKeyIndex = INDEX_NONE;
	if(EdInterface)
	{
		// Notify a containing tool etc. before and after we add the new key.
		if(NotifyObject)
		{
			TArray<UObject*> CurvesAboutToChange;
			CurvesAboutToChange.AddItem(Entry.CurveObject);
			NotifyObject->PreEditCurve(CurvesAboutToChange);
		}

		NewKeyIndex = EdInterface->CreateNewKey(NewKeyIn);
		EdInterface->SetKeyInterpMode(NewKeyIndex, CIM_CurveAutoClamped);

		if(NotifyObject)
		{
			NotifyObject->PostEditCurve();
		}

		CurveEdVC->Viewport->Invalidate();
	}
	return NewKeyIndex;
}



/** 
 * Returns the vertical size of the entire curve editor curve label entry list in pixels
 */
UINT WxCurveEditor::ComputeCurveLabelEntryListContentHeight() const
{
	INT HeightInPixels = 0;

	// Loop through labels adding height contribution
	for( INT EntryIdx = 0; EntryIdx < EdSetup->Tabs( EdSetup->ActiveTab ).Curves.Num(); ++EntryIdx )
	{
		const FCurveEdEntry& Entry = EdSetup->Tabs( EdSetup->ActiveTab ).Curves( EntryIdx );

		HeightInPixels += LabelEntryHeight;
	}

	return HeightInPixels;
}



/** Calculate the point on the screen that corresponds to the supplied In/Out value pair. */
FIntPoint WxCurveEditor::CalcScreenPos(const FVector2D& Val)
{
	FIntPoint Result;
	Result.X = LabelWidth + ((Val.X - StartIn) * PixelsPerIn);
	Result.Y = CurveViewY - ((Val.Y - StartOut) * PixelsPerOut);
	return Result;
}

/** Calculate the actual value that the supplied point on the screen represents. */
FVector2D WxCurveEditor::CalcValuePoint(const FIntPoint& Pos)
{
	FVector2D Result;
	Result.X = StartIn + ((Pos.X - LabelWidth) / PixelsPerIn);
	Result.Y = StartOut + ((CurveViewY - Pos.Y) / PixelsPerOut);
	return Result;
}

/** Draw the curve editor into the viewport. This includes, key, grid and curves themselves. */
void WxCurveEditor::DrawCurveEditor(FViewport* Viewport, FCanvas* Canvas)
{
	if(Viewport->GetSizeX() <= LabelWidth || Viewport->GetSizeY() <= 1)
	{
		return;
	}

	Clear(Canvas, BackgroundColor);

	CurveViewX = Viewport->GetSizeX() - LabelWidth;
	CurveViewY = Viewport->GetSizeY();

	PixelsPerIn = CurveViewX/(EndIn - StartIn);
	PixelsPerOut = CurveViewY/(EndOut - StartOut);

	// Draw background grid.
	DrawGrid(Viewport, Canvas);

	// Draw selected-region if desired.
	if(bShowRegionMarker)
	{
		FIntPoint RegionStartPos = CalcScreenPos( FVector2D(RegionStart, 0) );
		FIntPoint RegionEndPos = CalcScreenPos( FVector2D(RegionEnd, 0) );

		DrawTile(Canvas,RegionStartPos.X, 0, RegionEndPos.X - RegionStartPos.X, CurveViewY, 0.f, 0.f, 1.f, 1.f, RegionFillColor);
	}

	// Draw each curve
	for(INT i=0; i<EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num(); i++)
	{
		// Draw curve itself.
		FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(i);
		if(!CURVEEDENTRY_HIDECURVE(Entry.bHideCurve))
		{
			DrawEntry(Viewport, Canvas, Entry, i);
		}
	}

	// Draw key background block down left hand side.
	if (Canvas->IsHitTesting())
	{
		Canvas->SetHitProxy( new HCurveEdLabelBkgProxy() );
	}
	DrawTile(Canvas, 0, 0, LabelWidth, CurveViewY, 0.f, 0.f, 1.f, 1.f, LabelBlockBkgColor );
	if (Canvas->IsHitTesting())
	{
		Canvas->SetHitProxy( NULL );
	}

	// Draw key entry for each curve
	Canvas->PushRelativeTransform(FTranslationMatrix(FVector(LabelOrigin2D.X,LabelOrigin2D.Y,0)));
	INT CurrentKeyY = 0;
	for(INT i=0; i<EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num(); i++)
	{
		// Draw key entry
		FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(i);

		FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
		if (EdInterface)
		{
			// Draw background, color-square and text
			if (Canvas->IsHitTesting())
			{
				Canvas->SetHitProxy( new HCurveEdLabelProxy(i) );
			}
			if (CURVEEDENTRY_SELECTED(Entry.bHideCurve))
			{
				DrawTile(Canvas,0, CurrentKeyY, LabelWidth, LabelEntryHeight, 0.f, 0.f, 1.f, 1.f, SelectedLabelColor);
			}
			else
			{
				DrawTile(Canvas, 0, CurrentKeyY, LabelWidth, LabelEntryHeight, 0.f, 0.f, 1.f, 1.f, LabelColor );
			}
			DrawTile(Canvas, 0, CurrentKeyY, ColorKeyWidth, LabelEntryHeight, 0.f, 0.f, 1.f, 1.f, Entry.CurveColor );
			DrawShadowedString(Canvas, ColorKeyWidth+3, CurrentKeyY+4, *(Entry.CurveName), GEngine->SmallFont, FLinearColor::White );

			if( EdInterface->UsingLegacyInterpMethod() )
			{
				// This curve is using legacy auto tangents, so display a visual cue to the user
				DrawShadowedString(
					Canvas,
					ColorKeyWidth + 45,
					CurrentKeyY + 19,
					TEXT( "* OLD TANGENTS *" ),
					GEngine->SmallFont,
					FLinearColor( 1.0f, 0.2f, 0.2f ) );
			}

			if (Canvas->IsHitTesting())
			{
				Canvas->SetHitProxy( NULL );
			}

			// Draw hide/unhide button
			FColor ButtonColor = CURVEEDENTRY_HIDECURVE(Entry.bHideCurve) ? FColor(112,112,112) : FColor(255,200,0);
			if (Canvas->IsHitTesting())
			{
				Canvas->SetHitProxy( new HCurveEdHideCurveProxy(i) );
			}
			DrawTile(Canvas, LabelWidth - 12, CurrentKeyY + LabelEntryHeight - 12, 8, 8, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );
			DrawTile(Canvas, LabelWidth - 11, CurrentKeyY + LabelEntryHeight - 11, 6, 6, 0.f, 0.f, 1.f, 1.f, ButtonColor );
			if (Canvas->IsHitTesting())
			{
				Canvas->SetHitProxy( NULL );
			}

			// Draw the sub-curve hide/unhide buttons
			INT		SubCurveButtonOffset	= 8;

			INT NumSubs = EdInterface->GetNumSubCurves();
			for (INT ii = 0; ii < NumSubs; ii++)
			{
				ButtonColor = EdInterface->GetSubCurveButtonColor( ii, CURVEEDENTRY_HIDESUBCURVE(Entry.bHideCurve, ii) );

				if (Canvas->IsHitTesting())
				{
					Canvas->SetHitProxy(new HCurveEdHideSubCurveProxy(i, ii));
				}
				DrawTile(Canvas,SubCurveButtonOffset + 0, CurrentKeyY + LabelEntryHeight - 12, 8, 8, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );
				DrawTile(Canvas,SubCurveButtonOffset + 1, CurrentKeyY + LabelEntryHeight - 11, 6, 6, 0.f, 0.f, 1.f, 1.f, ButtonColor );

				SubCurveButtonOffset +=12;

				if (Canvas->IsHitTesting())
				{
					Canvas->SetHitProxy(NULL);
				}
			}
		}

		CurrentKeyY += 	LabelEntryHeight;

		// Draw line under each key entry
		DrawTile(Canvas, 0, CurrentKeyY-1, LabelWidth, 1, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );
	}

	Canvas->PopTransform();

	// Draw line above top-most key entry.
	DrawTile(Canvas, 0, 0, LabelWidth, 1, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );

	// Line down right of key.
	DrawTile(Canvas,LabelWidth, 0, 1, CurveViewY, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );

	// Draw box-selection region
	if(CurveEdVC->bBoxSelecting)
	{
		INT MinX = Min(CurveEdVC->BoxStartX, CurveEdVC->BoxEndX);
		INT MinY = Min(CurveEdVC->BoxStartY, CurveEdVC->BoxEndY);
		INT MaxX = Max(CurveEdVC->BoxStartX, CurveEdVC->BoxEndX);
		INT MaxY = Max(CurveEdVC->BoxStartY, CurveEdVC->BoxEndY);

		DrawLine2D(Canvas,FVector2D(MinX, MinY), FVector2D(MaxX, MinY), FColor(255,0,0));
		DrawLine2D(Canvas,FVector2D(MaxX, MinY), FVector2D(MaxX, MaxY), FColor(255,0,0));
		DrawLine2D(Canvas,FVector2D(MaxX, MaxY), FVector2D(MinX, MaxY), FColor(255,0,0));
		DrawLine2D(Canvas,FVector2D(MinX, MaxY), FVector2D(MinX, MinY), FColor(255,0,0));
	}

	if(bShowPositionMarker)
	{
		FIntPoint MarkerScreenPos = CalcScreenPos( FVector2D(MarkerPosition, 0) );
		if(MarkerScreenPos.X >= LabelWidth)
		{
			DrawLine2D(Canvas, FVector2D(MarkerScreenPos.X, 0), FVector2D(MarkerScreenPos.X, CurveViewY), MarkerColor );
		}
	}

	if(bShowEndMarker)
	{
		FIntPoint EndScreenPos = CalcScreenPos( FVector2D(EndMarkerPosition, 0) );
		if(EndScreenPos.X >= LabelWidth)
		{
			DrawLine2D(Canvas, FVector2D(EndScreenPos.X, 0), FVector2D(EndScreenPos.X, CurveViewY), FLinearColor::White );
		}
	}
}


// Draw a linear approximation to curve every CurveDrawRes pixels.
static const INT CurveDrawRes = 5;

static FColor GetLineColor(FCurveEdInterface* EdInterface, FLOAT InVal, UBOOL bFloatingPointColor)
{
	FColor StepColor;

	INT NumSubs = EdInterface->GetNumSubCurves();
	if(NumSubs == 3)
	{
		if (bFloatingPointColor == TRUE)
		{
			FLOAT Value;

			Value = EdInterface->EvalSub(0, InVal);
			Value *= 255.9f;
			StepColor.R = appTrunc( Clamp<FLOAT>(Value, 0.f, 255.9f) );
			Value = EdInterface->EvalSub(1, InVal);
			Value *= 255.9f;
			StepColor.G = appTrunc( Clamp<FLOAT>(Value, 0.f, 255.9f) );
			Value = EdInterface->EvalSub(2, InVal);
			Value *= 255.9f;
			StepColor.B = appTrunc( Clamp<FLOAT>(Value, 0.f, 255.9f) );
		}
		else
		{
			StepColor.R = appTrunc( Clamp<FLOAT>(EdInterface->EvalSub(0, InVal), 0.f, 255.9f) );
			StepColor.G = appTrunc( Clamp<FLOAT>(EdInterface->EvalSub(1, InVal), 0.f, 255.9f) );
			StepColor.B = appTrunc( Clamp<FLOAT>(EdInterface->EvalSub(2, InVal), 0.f, 255.9f) );
		}
		StepColor.A = 255;
	}
	else if(NumSubs == 1)
	{
		if (bFloatingPointColor == TRUE)
		{
			FLOAT Value;

			Value = EdInterface->EvalSub(0, InVal);
			Value *= 255.9f;
			StepColor.R = appTrunc( Clamp<FLOAT>(Value, 0.f, 255.9f) );
		}
		else
		{
			StepColor.R = appTrunc( Clamp<FLOAT>(EdInterface->EvalSub(0, InVal), 0.f, 255.9f) );
		}
		StepColor.G = StepColor.R;
		StepColor.B = StepColor.R;
		StepColor.A = 255;
	}
	else
	{
		StepColor = FColor(0,0,0);
	}

	return StepColor;
}

static inline FVector2D CalcTangentDir(FLOAT Tangent)
{
	const FLOAT Angle = appAtan(Tangent);
	return FVector2D( appCos(Angle), -appSin(Angle) );
}

static inline FLOAT CalcTangent(const FVector2D& HandleDelta)
{
	// Ensure X is positive and non-zero.
	// Tangent is gradient of handle.
	return HandleDelta.Y / ::Max<DOUBLE>(HandleDelta.X, KINDA_SMALL_NUMBER);
}

/** Draw one particular curve using the supplied FCanvas. */
void WxCurveEditor::DrawEntry( FViewport* Viewport, FCanvas* Canvas, const FCurveEdEntry& Entry, INT CurveIndex )
{
	FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
	if(!EdInterface)
	{
		return;
	}

	INT NumSubs = EdInterface->GetNumSubCurves();
	INT NumKeys = EdInterface->GetNumKeys();

	for(INT SubIdx=0; SubIdx<NumSubs; SubIdx++)
	{
		if (CURVEEDENTRY_HIDESUBCURVE(Entry.bHideCurve, SubIdx))
		{
			continue;
		}

		FVector2D OldKey(0.f, 0.f);
		FIntPoint OldKeyPos(0, 0);

		// Draw curve
		for(INT KeyIdx=0; KeyIdx<NumKeys; KeyIdx++)
		{
			FVector2D NewKey;
			NewKey.X = EdInterface->GetKeyIn(KeyIdx);
			NewKey.Y = EdInterface->EvalSub(SubIdx, NewKey.X);

			FIntPoint NewKeyPos = CalcScreenPos(NewKey);

			// If this section is visible then draw it!
			UBOOL bSectionVisible = true;
			if(NewKey.X < StartIn || OldKey.X > EndIn)
			{
				bSectionVisible = false;
			}

			if(KeyIdx>0 && bSectionVisible)
			{
				FLOAT KeyDiff = NewKey.X - OldKey.X;
				// We need to take the total range into account... 
				// otherwise, we end up w/ 100,000s of steps.
				FLOAT Scalar = 1.0f;
				FLOAT KeyDiffTemp = KeyDiff;
				while (appTrunc(KeyDiffTemp / Scalar) > 1.0f)
				{
					Scalar *= 10.0f;
				}
				FLOAT DrawTrackInRes = CurveDrawRes/PixelsPerIn;
				DrawTrackInRes *= Scalar;
				INT NumSteps = appCeil( KeyDiff/DrawTrackInRes );

				if( Scalar > 1.0f )
				{
					const INT MinStepsToConsider = 30;
					if( NumSteps < MinStepsToConsider )
					{
						// Make sure at least some steps are drawn.  The scalar might have made it so that only 1 step is drawn
						NumSteps = MinStepsToConsider;
					}
				}

				FLOAT DrawSubstep = KeyDiff/NumSteps;

				// Find position on first keyframe.
				FVector2D Old = OldKey;
				FIntPoint OldPos = OldKeyPos;

				BYTE InterpMode = EdInterface->GetKeyInterpMode(KeyIdx-1);

				if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HCurveEdLineProxy(CurveIndex, SubIdx) );
				// For constant interpolation - don't draw ticks - just draw dotted line.
				if(InterpMode == CIM_Constant)
				{
					DrawLine2D(Canvas, FVector2D(OldKeyPos.X, OldKeyPos.Y), FVector2D(NewKeyPos.X, OldKeyPos.Y), Entry.CurveColor );
					DrawLine2D(Canvas, FVector2D(NewKeyPos.X, OldKeyPos.Y), FVector2D(NewKeyPos.X, NewKeyPos.Y), Entry.CurveColor );
				}
				else if(InterpMode == CIM_Linear && !Entry.bColorCurve)
				{
					DrawLine2D(Canvas, FVector2D(OldKeyPos.X, OldKeyPos.Y), FVector2D(NewKeyPos.X, NewKeyPos.Y), Entry.CurveColor );
				}
				else
				{
					// Then draw a line for each substep.
					for(INT j=1; j<NumSteps+1; j++)
					{
						FVector2D New;
						New.X = OldKey.X + j*DrawSubstep;
						New.Y = EdInterface->EvalSub(SubIdx, New.X);

						FColor StepColor = Entry.bColorCurve ? GetLineColor(EdInterface, New.X, Entry.bFloatingPointColorCurve) : Entry.CurveColor;

						FIntPoint NewPos = CalcScreenPos(New);

						DrawLine2D(Canvas, OldPos, NewPos, StepColor);

						Old = New;
						OldPos = NewPos;
					}
				}

				if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
			}

			OldKey = NewKey;
			OldKeyPos = NewKeyPos;
		}

		// Draw lines to continue curve beyond last and before first.
		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HCurveEdLineProxy(CurveIndex, SubIdx) );

		if( NumKeys > 0 )
		{
			FLOAT RangeStart, RangeEnd;
			EdInterface->GetInRange(RangeStart, RangeEnd);

			if( RangeStart > StartIn )
			{
				FVector2D FirstKey;
				FirstKey.X = RangeStart;
				FirstKey.Y = EdInterface->GetKeyOut(SubIdx, 0);

				FColor StepColor = Entry.bColorCurve ? GetLineColor(EdInterface, RangeStart, Entry.bFloatingPointColorCurve) : Entry.CurveColor;
				FIntPoint FirstKeyPos = CalcScreenPos(FirstKey);

				DrawLine2D(Canvas,FVector2D(LabelWidth, FirstKeyPos.Y), FVector2D(FirstKeyPos.X, FirstKeyPos.Y), StepColor);
			}

			if( RangeEnd < EndIn )
			{
				FVector2D LastKey;
				LastKey.X = RangeEnd;
				LastKey.Y = EdInterface->GetKeyOut(SubIdx, NumKeys-1);;

				FColor StepColor = Entry.bColorCurve ? GetLineColor(EdInterface, RangeEnd, Entry.bFloatingPointColorCurve) : Entry.CurveColor;
				FIntPoint LastKeyPos = CalcScreenPos(LastKey);

				DrawLine2D(Canvas,FVector2D(LastKeyPos.X, LastKeyPos.Y), FVector2D(LabelWidth+CurveViewX, LastKeyPos.Y), StepColor);
			}
		}
		else // No points - draw line at zero.
		{
			FIntPoint OriginPos = CalcScreenPos( FVector2D(0,0) );
			DrawLine2D(Canvas, FVector2D(LabelWidth, OriginPos.Y), FVector2D(LabelWidth+CurveViewX, OriginPos.Y), Entry.CurveColor );
		}

		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );

		// Draw keypoints on top of curve
		for(INT KeyIdx=0; KeyIdx<NumKeys; KeyIdx++)
		{
			FVector2D NewKey;
			NewKey.X = EdInterface->GetKeyIn(KeyIdx);
			NewKey.Y = EdInterface->GetKeyOut(SubIdx, KeyIdx);

			FIntPoint NewKeyPos = CalcScreenPos(NewKey);

			FCurveEdSelKey TestKey(CurveIndex, SubIdx, KeyIdx);
			UBOOL bSelectedKey = SelectedKeys.ContainsItem(TestKey);
			FColor BorderColor = EdInterface->GetKeyColor(SubIdx, KeyIdx, Entry.CurveColor);
			FColor CenterColor = bSelectedKey ? SelectedKeyColor : Entry.CurveColor;

			if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HCurveEdKeyProxy(CurveIndex, SubIdx, KeyIdx) );
			DrawTile(Canvas, NewKeyPos.X-4, NewKeyPos.Y-4, 9, 9, 0.f, 0.f, 1.f, 1.f, BorderColor );
			DrawTile(Canvas, NewKeyPos.X-2, NewKeyPos.Y-2, 5, 5, 0.f, 0.f, 1.f, 1.f, CenterColor );
			if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );

			// If previous section is a curve- show little handles.
			if( bSelectedKey || bShowAllCurveTangents )
			{
				FLOAT ArriveTangent, LeaveTangent;
				EdInterface->GetTangents(SubIdx, KeyIdx, ArriveTangent, LeaveTangent);	

				BYTE PrevMode = (KeyIdx > 0) ? EdInterface->GetKeyInterpMode(KeyIdx-1) : 255;
				BYTE NextMode = (KeyIdx < NumKeys-1) ? EdInterface->GetKeyInterpMode(KeyIdx) : 255;

				// If not first point, and previous mode was a curve type.
				if(PrevMode == CIM_CurveAuto || PrevMode == CIM_CurveAutoClamped || PrevMode == CIM_CurveUser || PrevMode == CIM_CurveBreak)
				{
					FVector2D HandleDir = CalcTangentDir((PixelsPerOut/PixelsPerIn) * ArriveTangent);

					FIntPoint HandlePos;
					HandlePos.X = NewKeyPos.X - appRound( HandleDir.X * HandleLength );
					HandlePos.Y = NewKeyPos.Y - appRound( HandleDir.Y * HandleLength );

					DrawLine2D(Canvas,FVector2D(NewKeyPos.X, NewKeyPos.Y), FVector2D(HandlePos.X, HandlePos.Y), FColor(255,255,255));

					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HCurveEdKeyHandleProxy(CurveIndex, SubIdx, KeyIdx, true) );
					DrawTile(Canvas,HandlePos.X-2, HandlePos.Y-2, 5, 5, 0.f, 0.f, 1.f, 1.f, FColor(255,255,255));
					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
				}

				// If next section is a curve, draw leaving handle.
				if(NextMode == CIM_CurveAuto || NextMode == CIM_CurveAutoClamped || NextMode == CIM_CurveUser || NextMode == CIM_CurveBreak)
				{
					FVector2D HandleDir = CalcTangentDir((PixelsPerOut/PixelsPerIn) * LeaveTangent);

					FIntPoint HandlePos;
					HandlePos.X = NewKeyPos.X + appRound( HandleDir.X * HandleLength );
					HandlePos.Y = NewKeyPos.Y + appRound( HandleDir.Y * HandleLength );

					DrawLine2D(Canvas,NewKeyPos, HandlePos, FColor(255,255,255));

					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HCurveEdKeyHandleProxy(CurveIndex, SubIdx, KeyIdx, false) );
					DrawTile(Canvas,HandlePos.X-2, HandlePos.Y-2, 5, 5, 0.f, 0.f, 1.f, 1.f, FColor(255,255,255));
					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
				}
			}

			// If mouse is over this keypoint, show its value
			if( CurveIndex == MouseOverCurveIndex &&
				SubIdx == MouseOverSubIndex &&
				KeyIdx == MouseOverKeyIndex )
			{
				FString KeyComment;
				if (bSnapToFrames)
					KeyComment = FString::Printf( TEXT("(%df,%3.2f)"), appRound(NewKey.X/InSnapAmount), NewKey.Y );
				else
					KeyComment = FString::Printf( TEXT("(%3.2f,%3.2f)"), NewKey.X, NewKey.Y );

				DrawString(Canvas, NewKeyPos.X + 5, NewKeyPos.Y - 5, *KeyComment, GEngine->SmallFont, GridTextColor );
			}
		}
	}
}

// Externs from InterpEditorDraw.cpp.
extern FLOAT GetGridSpacing(INT GridNum);
extern UINT CalculateBestFrameStep(FLOAT SnapAmount, FLOAT PixelsPerSec, FLOAT MinPixelsPerGrid);

/** Draw the background grid and origin lines. */
void WxCurveEditor::DrawGrid(FViewport* Viewport, FCanvas* Canvas)
{
	// Determine spacing for In and Out grid lines
	INT MinPixelsPerInGrid = 35;
	INT MinPixelsPerOutGrid = 25;

	FLOAT MinGridSpacing = 0.001f;
	INT GridNum = 0;

	FLOAT InGridSpacing = MinGridSpacing;
	while( InGridSpacing * PixelsPerIn < MinPixelsPerInGrid )
	{
		InGridSpacing = MinGridSpacing * GetGridSpacing(GridNum);
		GridNum++;
	}

	GridNum = 0;

	FLOAT OutGridSpacing = MinGridSpacing;
	while( OutGridSpacing * PixelsPerOut < MinPixelsPerOutGrid )
	{
		OutGridSpacing = MinGridSpacing * GetGridSpacing(GridNum);
		GridNum++;
	}

	INT XL, YL;
	StringSize( GEngine->SmallFont, XL, YL, TEXT("0123456789") );


	// Calculate best frames' step.
	UINT FrameStep = 1; // Important frames' density.
	UINT AuxFrameStep = 1; // Auxiliary frames' density.

	if ( bSnapToFrames )
	{
		InGridSpacing  = InSnapAmount;	
		FrameStep = CalculateBestFrameStep(InSnapAmount, PixelsPerIn, MinPixelsPerInGrid);
		AuxFrameStep = CalculateBestFrameStep(InSnapAmount, PixelsPerIn, 6);
	}

	// Draw input grid

	INT InNum = appFloor(StartIn/InGridSpacing);
	while( InNum*InGridSpacing < EndIn )
	{
		FColor LineColor = GridColor;
				
		// Change line color for important frames.
		if ( bSnapToFrames )
		{
			LineColor = FColor(80,80,80);
			if (InNum % FrameStep == 0 )
			{
				LineColor = FColor(110,110,110);
			}
		}

		// Draw grid line.
		// In frames mode auxiliary lines cannot be too close.
		FIntPoint GridPos = CalcScreenPos( FVector2D(InNum*InGridSpacing, 0.f) );		
		if (!bSnapToFrames || (Abs(InNum) % AuxFrameStep == 0))
		{
			DrawLine2D(Canvas, FVector2D(GridPos.X, 0), FVector2D(GridPos.X, CurveViewY), LineColor );
		}
		InNum++;
	}

	// Draw output grid
	INT OutNum = appFloor(StartOut/OutGridSpacing);
	while( OutNum*OutGridSpacing < EndOut )
	{
		FIntPoint GridPos = CalcScreenPos( FVector2D(0.f, OutNum*OutGridSpacing) );
		DrawLine2D(Canvas, FVector2D(LabelWidth, GridPos.Y), FVector2D(LabelWidth + CurveViewX, GridPos.Y), GridColor );
		OutNum++;
	}

	// Calculate screen position of graph origin and draw white lines to indicate it

	FIntPoint OriginPos = CalcScreenPos( FVector2D(0,0) );

	DrawLine2D(Canvas, FVector2D(LabelWidth, OriginPos.Y), FVector2D(LabelWidth+CurveViewX, OriginPos.Y), FColor(255,255,255) );
	DrawLine2D(Canvas, FVector2D(OriginPos.X, 0), FVector2D(OriginPos.X, CurveViewY), FColor(255,255,255) );


	// Draw input labels

	InNum = appFloor(StartIn/InGridSpacing);
	while( InNum*InGridSpacing < EndIn )
	{
		// Draw value label
		FIntPoint GridPos = CalcScreenPos( FVector2D(InNum*InGridSpacing, 0.f) );
		
		// Show time or important frames' numbers (based on FrameStep).
		if ( !bSnapToFrames || Abs(InNum) % FrameStep == 0 )
		{				
			FString Label;
			if (bSnapToFrames)
			{
				// Show frames' numbers.
				Label = FString::Printf( TEXT("%d"), InNum );
			}
			else
			{
				// Show time.
				Label = FString::Printf( TEXT("%3.2f"), InNum*InGridSpacing );
			}
			DrawString(Canvas, GridPos.X + 2, CurveViewY - YL - 2, *Label, GEngine->SmallFont, GridTextColor );
		}
		InNum++;
	}

	// Draw output labels

	OutNum = appFloor(StartOut/OutGridSpacing);
	while( OutNum*OutGridSpacing < EndOut )
	{
		FIntPoint GridPos = CalcScreenPos( FVector2D(0.f, OutNum*OutGridSpacing) );
		if(GridPos.Y < CurveViewY - YL) // Only draw Output scale numbering if its not going to be on top of input numbering.
		{
			FString ScaleLabel = FString::Printf( TEXT("%3.2f"), OutNum*OutGridSpacing );
			DrawString(Canvas, LabelWidth + 2, GridPos.Y - YL - 2, *ScaleLabel, GEngine->SmallFont, GridTextColor );
		}
		OutNum++;
	}
}




/**
 * Sets the tangents for the selected key(s) to be flat along the horizontal axis
 */
void WxCurveEditor::OnFlattenTangentsToAxis( wxCommandEvent& In )
{
	for( INT i = 0; i < SelectedKeys.Num(); ++i )
	{
		FCurveEdSelKey& SelKey = SelectedKeys( i );

		FCurveEdEntry& Entry = EdSetup->Tabs( EdSetup->ActiveTab ).Curves( SelKey.CurveIndex );
		FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer( Entry );		

		// If we're in auto-curve mode, change the interp mode to USER
		EInterpCurveMode CurInterpMode = ( EInterpCurveMode )EdInterface->GetKeyInterpMode( SelKey.KeyIndex );
		if( CurInterpMode == CIM_CurveAuto || CurInterpMode == CIM_CurveAutoClamped )
		{
			EdInterface->SetKeyInterpMode( SelKey.KeyIndex, CIM_CurveUser );
		}

		// Flatten the tangents along the horizontal axis by zeroing out their slope
 		EdInterface->SetTangents( SelKey.SubIndex, SelKey.KeyIndex, 0.0f, 0.0f );
	}

	UpdateInterpModeButtons();
	CurveEdVC->Viewport->Invalidate();
}



/**
 * Straightens the tangents for the selected key(s) by averaging their direction
 */
void WxCurveEditor::OnStraightenTangents( wxCommandEvent& In )
{
	for( INT i = 0; i < SelectedKeys.Num(); ++i )
	{
		FCurveEdSelKey& SelKey = SelectedKeys( i );

		FCurveEdEntry& Entry = EdSetup->Tabs( EdSetup->ActiveTab ).Curves( SelKey.CurveIndex );
		FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer( Entry );

		// If we're in auto-curve mode, change the interp mode to USER
		EInterpCurveMode CurInterpMode = ( EInterpCurveMode )EdInterface->GetKeyInterpMode( SelKey.KeyIndex );
		if( CurInterpMode == CIM_CurveAuto || CurInterpMode == CIM_CurveAutoClamped )
		{
			EdInterface->SetKeyInterpMode( SelKey.KeyIndex, CIM_CurveUser );
		}

		// Grab the current incoming and outgoing tangent vectors
		FLOAT CurInTangent, CurOutTangent;
		EdInterface->GetTangents( SelKey.SubIndex, SelKey.KeyIndex, CurInTangent, CurOutTangent );

		// Average the tangents
		FLOAT StraightTangent = ( CurInTangent + CurOutTangent ) * 0.5f;

		// Straighten the tangents out!
 		EdInterface->SetTangents( SelKey.SubIndex, SelKey.KeyIndex, StraightTangent, StraightTangent );
	}

	UpdateInterpModeButtons();
	CurveEdVC->Viewport->Invalidate();
}



void WxCurveEditor::AddKeyToSelection(INT InCurveIndex, INT InSubIndex, INT InKeyIndex)
{
	if( !KeyIsInSelection(InCurveIndex, InSubIndex, InKeyIndex) )
	{
		SelectedKeys.AddItem( FCurveEdSelKey(InCurveIndex, InSubIndex, InKeyIndex) );
	}

	UpdateInterpModeButtons();
}

void WxCurveEditor::RemoveKeyFromSelection(INT InCurveIndex, INT InSubIndex, INT InKeyIndex)
{
	FCurveEdSelKey TestKey(InCurveIndex, InSubIndex, InKeyIndex);

	for(INT i=0; i<SelectedKeys.Num(); i++)
	{
		if( SelectedKeys(i) == TestKey )
		{
			SelectedKeys.Remove(i);
			return;
		}
	}

	UpdateInterpModeButtons();
}

void WxCurveEditor::ClearKeySelection()
{
	SelectedKeys.Empty();
	UpdateInterpModeButtons();
}

UBOOL WxCurveEditor::KeyIsInSelection(INT InCurveIndex, INT InSubIndex, INT InKeyIndex)
{
	FCurveEdSelKey TestKey(InCurveIndex, InSubIndex, InKeyIndex);

	for(INT i=0; i<SelectedKeys.Num(); i++)
	{
		if( SelectedKeys(i) == TestKey )
		{
			return true;
		}
	}

	return false;
}

struct FCurveEdModKey
{
	INT CurveIndex;
	INT KeyIndex;

	FCurveEdModKey(INT InCurveIndex, INT InKeyIndex)
	{
		CurveIndex = InCurveIndex;
		KeyIndex = InKeyIndex;
	}

	UBOOL operator==(const FCurveEdModKey& Other) const
	{
		return( (CurveIndex == Other.CurveIndex) && (KeyIndex == Other.KeyIndex) );
	}
};

void WxCurveEditor::MoveSelectedKeys(FLOAT DeltaIn, FLOAT DeltaOut )
{
	// To avoid applying an input-modify twice to the same key (but on different subs), we note which 
	// curve/key combination we have already change the In of.
	TArray<FCurveEdModKey> MovedInKeys;

	for(INT i=0; i<SelectedKeys.Num(); i++)
	{
		FCurveEdSelKey& SelKey = SelectedKeys(i);

		FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(SelKey.CurveIndex);
		FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);

		// If there is a change in the Output, apply it.
		if( Abs<FLOAT>(DeltaOut) > 0.f )
		{
			SelKey.UnsnappedOut += DeltaOut;
			FLOAT NewOut = SelKey.UnsnappedOut;

			// For colour curves, clamp keys to between 0 and 255(ish)
			if(Entry.bColorCurve && (Entry.bFloatingPointColorCurve == FALSE))
			{
				NewOut = Clamp<FLOAT>(NewOut, 0.f, 255.9f);
			}
			if (Entry.bClamp)
			{
				NewOut = Clamp<FLOAT>(NewOut, Entry.ClampLow, Entry.ClampHigh);
			}
			EdInterface->SetKeyOut(SelKey.SubIndex, SelKey.KeyIndex, NewOut);
		}

		FCurveEdModKey TestKey(SelKey.CurveIndex, SelKey.KeyIndex);

		// If there is a change in the Input, apply it.
		// This is slightly complicated because it may change the index of the selected key, so we have to update the selection as we do it.
		if( Abs<FLOAT>(DeltaIn) > 0.f && !MovedInKeys.ContainsItem(TestKey) )
		{
			SelKey.UnsnappedIn += DeltaIn;
			FLOAT NewIn = SnapIn(SelKey.UnsnappedIn);

			INT OldKeyIndex = SelKey.KeyIndex;
			INT NewKeyIndex = EdInterface->SetKeyIn(SelKey.KeyIndex, NewIn);
			SelKey.KeyIndex = NewKeyIndex;

			// If the key changed index we need to search for any other selected keys on this track that may need their index adjusted because of this change.
			INT KeyMove = NewKeyIndex - OldKeyIndex;
			if(KeyMove > 0)
			{
				for(INT j=0; j<SelectedKeys.Num(); j++)
				{
					if( j == i ) // Don't look at one we just changed.
						continue;

					FCurveEdSelKey& TestKey = SelectedKeys(j);
					if( TestKey.CurveIndex == SelKey.CurveIndex && 
						TestKey.KeyIndex > OldKeyIndex && 
						TestKey.KeyIndex <= NewKeyIndex)
					{
						TestKey.KeyIndex--;
					}
					// change index of subcurves also
					else if (TestKey.CurveIndex == SelKey.CurveIndex && 
							 TestKey.KeyIndex == OldKeyIndex )
					{
						check( TestKey.SubIndex != SelKey.SubIndex );
						TestKey.KeyIndex = NewKeyIndex;
					}
				}
			}
			else if(KeyMove < 0)
			{
				for(INT j=0; j<SelectedKeys.Num(); j++)
				{
					if( j == i )
						continue;

					FCurveEdSelKey& TestKey = SelectedKeys(j);
					if( TestKey.CurveIndex == SelKey.CurveIndex && 
						TestKey.KeyIndex < OldKeyIndex && 
						TestKey.KeyIndex >= NewKeyIndex)
					{
						TestKey.KeyIndex++;
					}
					// change index of subcurves also
					else if (TestKey.CurveIndex == SelKey.CurveIndex && 
							 TestKey.KeyIndex == OldKeyIndex )
					{
						check( TestKey.SubIndex != SelKey.SubIndex );
						TestKey.KeyIndex = NewKeyIndex;
					}
				}
			}

			// Remember we have adjusted the In of this key.
			TestKey.KeyIndex = NewKeyIndex;
			MovedInKeys.AddItem(TestKey);
		}
	} // FOR each selected key

	// Call the notify object if present.
	if(NotifyObject)
	{
		NotifyObject->MovedKey();
	}
}

void WxCurveEditor::MoveCurveHandle(const FVector2D& NewHandleVal)
{
	FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(HandleCurveIndex);
	FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
	check(EdInterface);

	FVector2D KeyVal;
	KeyVal.X = EdInterface->GetKeyIn(HandleKeyIndex);
	KeyVal.Y = EdInterface->GetKeyOut(HandleSubIndex, HandleKeyIndex);

	// Find vector (in 'curve space') between key point and mouse position.
	FVector2D HandleDelta = NewHandleVal - KeyVal;

	// If 'arriving' handle (at end of section), the handle points the other way.
	if(bHandleArriving)
	{
		HandleDelta *= -1.f;
	}

	FLOAT NewTangent = CalcTangent( HandleDelta );

	FLOAT ArriveTangent, LeaveTangent;
	EdInterface->GetTangents(HandleSubIndex, HandleKeyIndex, ArriveTangent, LeaveTangent);

	// If adjusting the handle on an 'auto' keypoint, automagically convert to User mode.
	BYTE InterpMode = EdInterface->GetKeyInterpMode(HandleKeyIndex);
	if(InterpMode == CIM_CurveAuto || InterpMode == CIM_CurveAutoClamped)
	{
		EdInterface->SetKeyInterpMode(HandleKeyIndex, CIM_CurveUser);
		UpdateInterpModeButtons();
	}

	// In both User and Auto (non-Break curve modes) - enforce smoothness.
	if(InterpMode != CIM_CurveBreak)
	{
		ArriveTangent = NewTangent;
		LeaveTangent = NewTangent;
	}
	else
	{
		if(bHandleArriving)
		{
			ArriveTangent = NewTangent;
		}
		else
		{
			LeaveTangent = NewTangent;
		}
	}

	EdInterface->SetTangents(HandleSubIndex, HandleKeyIndex, ArriveTangent, LeaveTangent);
}

void WxCurveEditor::UpdateDisplay()
{
	CurveEdVC->Viewport->Invalidate();
}

void WxCurveEditor::CurveChanged()
{
	ClearKeySelection();

	// A new tab may have been added or removed, so update the scroll bar
	AdjustScrollBar();

	UpdateDisplay();
}

void WxCurveEditor::SetPositionMarker(UBOOL bEnabled, FLOAT InPosition, const FColor& InMarkerColor)
{
	bShowPositionMarker = bEnabled;
	MarkerPosition = InPosition;
	MarkerColor = InMarkerColor;

	UpdateDisplay();
}

void WxCurveEditor::SetEndMarker(UBOOL bEnable, FLOAT InEndPosition)
{
	bShowEndMarker = bEnable;
	EndMarkerPosition = InEndPosition;

	UpdateDisplay();
}

void WxCurveEditor::SetRegionMarker(UBOOL bEnabled, FLOAT InRegionStart, FLOAT InRegionEnd, const FColor& InRegionFillColor)
{
	bShowRegionMarker = bEnabled;
	RegionStart = InRegionStart;
	RegionEnd = InRegionEnd;
	RegionFillColor = InRegionFillColor;
	
	UpdateDisplay();
}

void WxCurveEditor::SetInSnap(UBOOL bEnabled, FLOAT SnapAmount, UBOOL bInSnapToFrames)
{
	bSnapEnabled = bEnabled;
	InSnapAmount = SnapAmount;
	bSnapToFrames = bInSnapToFrames;
}

void WxCurveEditor::SetNotifyObject(FCurveEdNotifyInterface* NewNotifyObject)
{
	NotifyObject = NewNotifyObject;
}

/** Set the selected state of the given curve curve to bSelected					*/
void WxCurveEditor::SetCurveSelected(UObject* InCurve, UBOOL bSelected)
{
	for (INT ii = 0; ii < EdSetup->Tabs.Num(); ii++)
	{
		for (INT jj = 0; jj < EdSetup->Tabs(ii).Curves.Num(); jj++)
		{
			FCurveEdEntry* Entry = &(EdSetup->Tabs(ii).Curves(jj));
			if (Entry->CurveObject == InCurve)
			{
				CURVEEDENTRY_SET_SELECTED(Entry->bHideCurve, bSelected);
				break;
			}
		}
	}
}

/** Clear all selected curve flags													*/
void WxCurveEditor::ClearAllSelectedCurves()
{
	for (INT ii = 0; ii < EdSetup->Tabs.Num(); ii++)
	{
		for (INT jj = 0; jj < EdSetup->Tabs(ii).Curves.Num(); jj++)
		{
			FCurveEdEntry* Entry = &(EdSetup->Tabs(ii).Curves(jj));
			CURVEEDENTRY_SET_SELECTED(Entry->bHideCurve, FALSE);
		}
	}
}

/** Scroll the window to display the first selected module */
void WxCurveEditor::ScrollToFirstSelected()
{
	INT CurveCount = EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num();

	if ((INT)(LabelEntryHeight * CurveCount) < CurveEd_ContentBoxHeight)
	{
		// All are inside the current box...
		return;
	}

	INT SelectedIndex = -1;
	for (INT CurveIndex = 0; CurveIndex < CurveCount; CurveIndex++)
	{
		FCurveEdEntry* Entry = &(EdSetup->Tabs(EdSetup->ActiveTab).Curves(CurveIndex));
		if (CURVEEDENTRY_SELECTED(Entry->bHideCurve))
		{
			SelectedIndex = CurveIndex;
			break;
		}
	}

	if ((SelectedIndex >= 0) && (SelectedIndex < CurveCount))
	{
		INT ApproxIndex = ThumbPos_Vert / LabelEntryHeight;
		INT FitCount = CurveEd_ContentBoxHeight / LabelEntryHeight;

		// Pop it to the middle...
		INT JumpIndex = SelectedIndex - (FitCount / 2);
		if ((CurveCount - JumpIndex) < FitCount)
		{
			JumpIndex = CurveCount - FitCount;
		}
		Clamp<INT>(JumpIndex, 0, (CurveCount - FitCount));
		ThumbPos_Vert = JumpIndex * LabelEntryHeight;
		LabelOrigin2D.Y = -ThumbPos_Vert;
		LabelOrigin2D.Y = Min(0, LabelOrigin2D.Y);
		SetScrollBarPosition();
	}
}

/** Clear all selected curve flags													*/
UBOOL WxCurveEditor::PresetDialog_OnOK()
{
	if (PresetDialog)
	{
		if (PresetDialog->GetIsSaveDlg())
		{
			for (INT BlockIndex = 0; BlockIndex < WxCurveEdPresetDlg::CEP_CURVE_MAX; BlockIndex++)
			{
				UCurveEdPresetBase* PresetBase	= PresetDialog->GetPreset(BlockIndex);
				if (PresetBase->IsA(UCurveEdPreset_UserSet::StaticClass()))
				{
					UCurveEdPreset_UserSet*	UserSet	= Cast<UCurveEdPreset_UserSet>(PresetBase);
					check(UserSet);

					if (UserSet->UserCurve)
					{
						INT CurveIndex = DetermineSubCurveIndex(BlockIndex);

						// Grab the curve points
						if (UserSet->UserCurve->StoreCurvePoints(CurveIndex, Distribution) == FALSE)
						{
							warnf(TEXT("Failed to store curve points for sub-curve %d"), CurveIndex);
						}
						else
						{
							UserSet->UserCurve->MarkPackageDirty();
						}
					}
					else
					{
						warnf(TEXT("Invalid user curve for sub-curve %d"), BlockIndex);
					}
				}
			}
		}
		else
		{
			INT BlockIndex;
			INT PointIndex;
			INT CheckKeyIndex;
			INT	SubIndex;
			FPresetGeneratedPoint* GeneratedPoint = NULL;

			// Determine the KeyIn points for each required curve,
			// and save the curves for any set to "Do not preset"
			if (GetSetupDataAndRequiredKeyInPoints(PresetDialog) == FALSE)
			{
				warnf(TEXT("Failed to get setup data and keyin points"));
			}

			// Sort the number of keyin times...
			TArray<INT>	LowToHighKeyInCounts;

			LowToHighKeyInCounts.Empty();

			for (BlockIndex = 0; BlockIndex < WxCurveEdPresetDlg::CEP_CURVE_MAX; BlockIndex++)
			{
				INT CheckIndex;
				INT	Count		= RequiredKeyInTimes[BlockIndex].Num();
				if (Count > 0)
				{
					UBOOL bSkipIt = FALSE;
					for (CheckIndex = 0; CheckIndex < LowToHighKeyInCounts.Num(); CheckIndex++)
					{
						if (LowToHighKeyInCounts(CheckIndex) == Count)
						{
							bSkipIt = TRUE;
							break;
						}
						else
						if (LowToHighKeyInCounts(CheckIndex) > Count)
						{
							break;
						}
					}

					if (bSkipIt == FALSE)
					{
						LowToHighKeyInCounts.InsertZeroed(CheckIndex);
						LowToHighKeyInCounts(CheckIndex)	= Count;
					}
				}
			}

#if defined(_PRESETS_DEBUG_ENABLED_)
			debugf(TEXT("LowToHighKeyInCounts"));
			for (BlockIndex = 0; BlockIndex < LowToHighKeyInCounts.Num(); BlockIndex++)
			{
				debugf(TEXT("\t%d --> %2d"), BlockIndex, LowToHighKeyInCounts(BlockIndex));
			}
#endif	//#if defined(_PRESETS_DEBUG_ENABLED_)

			// Fill in the overall keyin array to use when generating points.
			if (GenerateCompleteKeyInList() == FALSE)
			{
				warnf(TEXT("Failed to generate complete keyin list"));
			}

			// Grab the samples
			if (GeneratePresetSamples(PresetDialog) == FALSE)
			{
				warnf(TEXT("Failed to generate preset samples"));
			}

			// Fill in the curves here!
			// Empty the curve
			for (INT KeyIndex = Distribution->GetNumKeys() - 1; KeyIndex >= 0; KeyIndex--)
			{
				Distribution->DeleteKey(KeyIndex);
			}

			// We need to determine a number of curves to utilize...
			for (INT KeyInCountIndex = 0; KeyInCountIndex < LowToHighKeyInCounts.Num(); KeyInCountIndex++)
			{
				INT	CurrentCount	= LowToHighKeyInCounts(KeyInCountIndex);
				for (BlockIndex = 0; BlockIndex < WxCurveEdPresetDlg::CEP_CURVE_MAX; BlockIndex++)
				{
					if (CurrentCount != RequiredKeyInTimes[BlockIndex].Num())
					{
						continue;
					}
					SubIndex = DetermineSubCurveIndex(BlockIndex);
					if (SubIndex == -1)
					{
						continue;
					}
					UCurveEdPresetBase*	PresetClass = PresetDialog->GetPreset(BlockIndex);
					check(PresetClass);

#if defined(_PRESETS_DEBUG_ENABLED_)
					debugf(TEXT("Setting data for Curve %2d (SubIndex = %2d) - %s"), 
						BlockIndex, SubIndex, *PresetClass->GetName());
#endif	//#if defined(_PRESETS_DEBUG_ENABLED_)

					if (PresetClass->IsA(UCurveEdPreset_Nothing::StaticClass()) == FALSE)
					{
						for (PointIndex = 0; PointIndex < GeneratedPoints[BlockIndex].Num(); PointIndex++)
						{
							GeneratedPoint = &(GeneratedPoints[BlockIndex](PointIndex));

							// See if the key already exists
							INT	KeyIndex	= -1;

							for (CheckKeyIndex = 0; CheckKeyIndex < Distribution->GetNumKeys(); CheckKeyIndex++)
							{
								FLOAT	CheckKeyIn	= Distribution->GetKeyIn(CheckKeyIndex);
								if (CheckKeyIn == GeneratedPoint->KeyIn)
								{
									KeyIndex = CheckKeyIndex;
									break;
								}
							}

							if (KeyIndex == -1)
							{
								KeyIndex = Distribution->CreateNewKey(GeneratedPoint->KeyIn);
							}

#if defined(_PRESETS_DEBUG_ENABLED_)
							debugf(TEXT("\tKeyIndex %2d - KeyIn = %f, KeyOut = %f"),
								KeyIndex, GeneratedPoint->KeyIn, GeneratedPoint->KeyOut);
#endif	//#if defined(_PRESETS_DEBUG_ENABLED_)

							FLOAT NewOut = GeneratedPoint->KeyOut;
/***
							@todo. Clamp these values??
							if (Entry.bClamp)
							{
								NewOut = Clamp<FLOAT>(NewOut, Entry.ClampLow, Entry.ClampHigh);
							}
***/
							Distribution->SetKeyOut(SubIndex, KeyIndex, NewOut);
							Distribution->SetKeyInterpMode(KeyIndex, CIM_CurveAuto);
							if (GeneratedPoint->TangentsValid == TRUE)
							{
								Distribution->SetTangents(SubIndex, KeyIndex, GeneratedPoint->TangentIn, GeneratedPoint->TangentOut);
							}
						}
					}
					else
					{
						if (CopiedCurves[BlockIndex].Num() < 0)
						{
							warnf(TEXT("Do not preset curve %s - no generated points"), BlockIndex);
						}
						else
						{
							for (PointIndex = 0; PointIndex < CopiedCurves[BlockIndex].Num(); PointIndex++)
							{
								FPresetGeneratedPoint* Generated = &(CopiedCurves[BlockIndex](PointIndex));

								// See if the key already exists
								INT	KeyIndex	= -1;

								for (CheckKeyIndex = 0; CheckKeyIndex < Distribution->GetNumKeys(); CheckKeyIndex++)
								{
									FLOAT	CheckKeyIn	= Distribution->GetKeyIn(CheckKeyIndex);
									if (CheckKeyIn == Generated->KeyIn)
									{
										KeyIndex = CheckKeyIndex;
										break;
									}
								}

								if (KeyIndex == -1)
								{
									KeyIndex = Distribution->CreateNewKey(Generated->KeyIn);
								}

#if defined(_PRESETS_DEBUG_ENABLED_)
							debugf(TEXT("\tKeyIndex %2d - KeyIn = %f, KeyOut = %f"),
								KeyIndex, GeneratedPoint->KeyIn, GeneratedPoint->KeyOut);
#endif	//#if defined(_PRESETS_DEBUG_ENABLED_)

								FLOAT NewOut = Generated->KeyOut;
/***
								@todo. Clamp these values??
								if (Entry.bClamp)
								{
									NewOut = Clamp<FLOAT>(NewOut, Entry.ClampLow, Entry.ClampHigh);
								}
***/
								Distribution->SetKeyOut(SubIndex, KeyIndex, NewOut);
								Distribution->SetKeyInterpMode(KeyIndex, CIM_CurveAuto);
								if (Generated->TangentsValid == TRUE)
								{
									Distribution->SetTangents(SubIndex, KeyIndex, Generated->TangentIn, Generated->TangentOut);
								}
							}
						}
					}

					// Mark it dirty...
					if (FloatDist && FloatDist->GetOuter())
					{
						FloatDist->GetOuter()->MarkPackageDirty();
					}
					else
					if (VectorDist && VectorDist->GetOuter())
					{
						VectorDist->GetOuter()->MarkPackageDirty();
					}
				}

				if (FloatCC)
				{
					FloatCC->ConstantCurve.AutoSetTangents();
				}
				else
				if (FloatUC)
				{
					FloatUC->ConstantCurve.AutoSetTangents();
				}
				else
				if (VectorCC)
				{
					VectorCC->ConstantCurve.AutoSetTangents();
				}
				else
				if (VectorUC)
				{
					VectorUC->ConstantCurve.AutoSetTangents();
				}
			}
		}
	}
	
	return TRUE;
}

void WxCurveEditor::ToggleCurveHidden(INT InCurveIndex)
{
	FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(InCurveIndex);
	CURVEEDENTRY_TOGGLE_HIDECURVE(Entry.bHideCurve);

	// Remove any key we have selected in the current curve.
	INT i=0;
	while(i<SelectedKeys.Num())
	{
		if(SelectedKeys(i).CurveIndex == InCurveIndex)
		{
			SelectedKeys.Remove(i);
		}
		else
		{
			i++;
		}
	}
}

void WxCurveEditor::ToggleSubCurveHidden(INT InCurveIndex, INT InSubCurveIndex)
{
	FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(InCurveIndex);
	check(InSubCurveIndex < 6);
	CURVEEDENTRY_TOGGLE_HIDESUBCURVE(Entry.bHideCurve, InSubCurveIndex);
}

void WxCurveEditor::UpdateInterpModeButtons()
{
	if(SelectedKeys.Num() == 0)
	{
		ToolBar->SetCurveMode(CIM_Unknown);
		ToolBar->SetButtonsEnabled(false);
	}
	else
	{
		BYTE Mode = CIM_Unknown;

		for(INT i=0; i<SelectedKeys.Num(); i++)
		{
			FCurveEdSelKey& SelKey = SelectedKeys(i);

			FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(SelKey.CurveIndex);
			FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);

			BYTE KeyMode = EdInterface->GetKeyInterpMode(SelKey.KeyIndex);

			// If first key we look at, use it as the group one.
			if(i==0)
			{
				Mode = KeyMode;
			}
			// If we find a key after the first of a different type, set selected key type to Unknown.
			else if(Mode != KeyMode)
			{
				Mode = CIM_Unknown;
			}
		}

		ToolBar->SetButtonsEnabled(true);
		ToolBar->SetCurveMode( (EInterpCurveMode)Mode );
	}
}

void WxCurveEditor::DeleteSelectedKeys()
{
	// Make a list of all curves we are going to remove keys from.
	TArray<UObject*> CurvesAboutToChange;
	for(INT i=0; i<SelectedKeys.Num(); i++)
	{
		FCurveEdSelKey& SelKey = SelectedKeys(i);
		FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(SelKey.CurveIndex);

		if(Entry.CurveObject)
		{
			CurvesAboutToChange.AddUniqueItem( Entry.CurveObject );
		}
	}

	// Notify a containing tool that keys are about to be removed
	if(NotifyObject)
	{
		NotifyObject->PreEditCurve(CurvesAboutToChange);
	}

	// Iterate over selected keys and actually remove them.
	for(INT i=0; i<SelectedKeys.Num(); i++)
	{
		FCurveEdSelKey& SelKey = SelectedKeys(i);

		FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(SelKey.CurveIndex);
		FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);

		EdInterface->DeleteKey(SelKey.KeyIndex);

		// Do any updating on the rest of the selection.
		INT j=i+1;
		while( j<SelectedKeys.Num() )
		{
			// If key is on same curve..
			if( SelectedKeys(j).CurveIndex == SelKey.CurveIndex )
			{
				// If key is same curve and key, but different sub, remove it.
				if( SelectedKeys(j).KeyIndex == SelKey.KeyIndex )
				{
					SelectedKeys.Remove(j);
				}
				// If its on same curve but higher key index, decrement it
				else if( SelectedKeys(j).KeyIndex > SelKey.KeyIndex )
				{
					SelectedKeys(j).KeyIndex--;
					j++;
				}
				// Otherwise, do nothing.
				else
				{
					j++;
				}
			}
			// Otherwise, do nothing.
			else
			{
				j++;
			}
		}
	}

	if(NotifyObject)
	{
		NotifyObject->PostEditCurve();
	}
		
	// Finally deselect everything.
	ClearKeySelection();

	CurveEdVC->Viewport->Invalidate();
}

void WxCurveEditor::BeginMoveSelectedKeys()
{
	TArray<UObject*> CurvesAboutToChange;

	for(INT i=0; i<SelectedKeys.Num(); i++)
	{
		FCurveEdSelKey& SelKey = SelectedKeys(i);

		FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(SelKey.CurveIndex);
		FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
		check(EdInterface);

		SelectedKeys(i).UnsnappedIn = EdInterface->GetKeyIn(SelKey.KeyIndex);
		SelectedKeys(i).UnsnappedOut = EdInterface->GetKeyOut(SelKey.SubIndex, SelKey.KeyIndex);

		// Make a list of all curves we are going to move keys in.
		if(Entry.CurveObject)
		{
			CurvesAboutToChange.AddUniqueItem( Entry.CurveObject );
		}
	}

	if(NotifyObject)
	{
		NotifyObject->PreEditCurve(CurvesAboutToChange);
	}
}

void WxCurveEditor::EndMoveSelectedKeys()
{
	if(NotifyObject)
	{
		NotifyObject->PostEditCurve();
	}
}

void WxCurveEditor::DoubleClickedKey(INT InCurveIndex, INT InSubIndex, INT InKeyIndex)
{

}

FLOAT WxCurveEditor::SnapIn(FLOAT InValue)
{
	if(bSnapEnabled)
	{
		return InSnapAmount * appRound(InValue/InSnapAmount);
	}
	else
	{
		return InValue;
	}
}

void WxCurveEditor::ChangeInterpMode(EInterpCurveMode NewInterpMode/*=CIM_Unknown*/)
{
	for(INT i=0; i<SelectedKeys.Num(); i++)
	{
		FCurveEdSelKey& SelKey = SelectedKeys(i);

		FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(SelKey.CurveIndex);
		FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);		

		EdInterface->SetKeyInterpMode(SelKey.KeyIndex, NewInterpMode);
	}

	CurveEdVC->Viewport->Invalidate();

	UpdateInterpModeButtons();
}


/*-----------------------------------------------------------------------------
WxCurveEdToolBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxCurveEdToolBar, WxToolBar )
END_EVENT_TABLE()

WxCurveEdToolBar::WxCurveEdToolBar( wxWindow* InParent, wxWindowID InID )
: WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
{
	// create the return to parent sequence button
	FitHorzB.Load(TEXT("CUR_FitHorz"));
	FitVertB.Load(TEXT("CUR_FitVert"));
	FitViewToAllB.Load( TEXT( "CUR_FitViewToAll" ) );
	FitViewToSelectedB.Load( TEXT( "CUR_FitViewToSelected" ) );
	PanB.Load(TEXT("CUR_Pan"));
	ZoomB.Load(TEXT("CUR_Zoom"));
	ModeAutoB.Load(TEXT("CUR_ModeAuto"));
	ModeAutoClampedB.Load(TEXT("CUR_ModeAutoClamped"));
	ModeUserB.Load(TEXT("CUR_ModeUser"));
	ModeBreakB.Load(TEXT("CUR_ModeBreak"));
	ModeLinearB.Load(TEXT("CUR_ModeLinear"));
	ModeConstantB.Load(TEXT("CUR_ModeConstant"));
	TabCreateB.Load(TEXT("CUR_TabCreate"));
	TabDeleteB.Load(TEXT("CUR_TabDelete"));
	FlattenTangentsToAxisB.Load(TEXT("CUR_FlattenTangentsToAxis"));
	StraightenTangentsB.Load(TEXT("CUR_StraightenTangents"));
	ShowAllCurveTangentsB.Load( TEXT( "CUR_ShowAllCurveTangents" ) );

	SetToolBitmapSize( wxSize( 14, 14 ) );

	AddSeparator();
	AddTool(IDM_CURVE_FITHORZ, FitHorzB, *LocalizeUnrealEd("FitHorizontally"));
	AddTool(IDM_CURVE_FITVERT, FitVertB, *LocalizeUnrealEd("FitVertically"));
	AddTool(IDM_CURVE_FitViewToAll, FitViewToAllB, *LocalizeUnrealEd( "CurveEd_ToolBar_FitViewToAll" ) );
	AddTool(IDM_CURVE_FitViewToSelected, FitViewToSelectedB, *LocalizeUnrealEd( "CurveEd_ToolBar_FitViewToSelected" ) );
	AddSeparator();
	AddCheckTool(IDM_CURVE_PANMODE, *LocalizeUnrealEd("PanMode"), PanB, wxNullBitmap, *LocalizeUnrealEd("PanMode") );
	AddCheckTool(IDM_CURVE_ZOOMMODE, *LocalizeUnrealEd("ZoomMode"), ZoomB, wxNullBitmap, *LocalizeUnrealEd("ZoomMode") );
	AddSeparator();
	AddCheckTool(IDM_CURVE_MODE_AUTO, *LocalizeUnrealEd("CurveAuto"), ModeAutoB, wxNullBitmap, *LocalizeUnrealEd("CurveAuto"));
	AddCheckTool(IDM_CURVE_MODE_AUTO_CLAMPED, *LocalizeUnrealEd("CurveAutoClamped"), ModeAutoClampedB, wxNullBitmap, *LocalizeUnrealEd("CurveAutoClamped"));
	AddCheckTool(IDM_CURVE_MODE_USER, *LocalizeUnrealEd("CurveUser"), ModeUserB, wxNullBitmap, *LocalizeUnrealEd("CurveUser"));
	AddCheckTool(IDM_CURVE_MODE_BREAK, *LocalizeUnrealEd("CurveBreak"), ModeBreakB, wxNullBitmap, *LocalizeUnrealEd("CurveBreak"));
	AddCheckTool(IDM_CURVE_MODE_LINEAR, *LocalizeUnrealEd("Linear"), ModeLinearB, wxNullBitmap, *LocalizeUnrealEd("Linear"));
	AddCheckTool(IDM_CURVE_MODE_CONSTANT, *LocalizeUnrealEd("Constant"), ModeConstantB, wxNullBitmap, *LocalizeUnrealEd("Constant"));
	AddSeparator();
	AddTool(IDM_CURVE_FLATTEN_TANGENTS_TO_AXIS, FlattenTangentsToAxisB, *LocalizeUnrealEd("FlattenTangentsToAxis"));
	AddTool(IDM_CURVE_STRAIGHTEN_TANGENTS, StraightenTangentsB, *LocalizeUnrealEd("StraightenTangents"));
	AddSeparator();
	AddCheckTool( IDM_CURVE_ShowAllCurveTangents, *LocalizeUnrealEd( "CurveEd_ToolBar_ShowAllCurveTangents" ), ShowAllCurveTangentsB, wxNullBitmap, *LocalizeUnrealEd( "CurveEd_ToolBar_ShowAllCurveTangents" ) );
	AddSeparator();
	AddTool(IDM_CURVE_TAB_CREATE, TabCreateB, *LocalizeUnrealEd("CurveTabCreate"));
	// Create the tab combo
	TabCombo = new WxComboBox(this, IDM_CURVE_TABCOMBO, TEXT(""), wxDefaultPosition, wxSize(150, -1), 0, NULL, wxCB_READONLY);
	TabCombo->SetToolTip( *LocalizeUnrealEd( "CurveEd_TabComboBox_Desc" ) );
	AddControl(TabCombo);
	AddTool(IDM_CURVE_TAB_DELETE, TabDeleteB, *LocalizeUnrealEd("CurveTabDelete"));

	SetEditMode(CEM_Pan);
	SetCurveMode(CIM_Unknown);

	Realize();
}

WxCurveEdToolBar::~WxCurveEdToolBar()
{
}

void WxCurveEdToolBar::SetCurveMode(EInterpCurveMode NewMode)
{
	ToggleTool(IDM_CURVE_MODE_AUTO, NewMode == CIM_CurveAuto);
	ToggleTool(IDM_CURVE_MODE_AUTO_CLAMPED, NewMode == CIM_CurveAutoClamped);
	ToggleTool(IDM_CURVE_MODE_USER, NewMode == CIM_CurveUser);
	ToggleTool(IDM_CURVE_MODE_BREAK, NewMode == CIM_CurveBreak);
	ToggleTool(IDM_CURVE_MODE_LINEAR, NewMode == CIM_Linear);
	ToggleTool(IDM_CURVE_MODE_CONSTANT, NewMode == CIM_Constant);
}

void WxCurveEdToolBar::SetButtonsEnabled(UBOOL bEnabled)
{
	EnableTool(IDM_CURVE_MODE_AUTO, bEnabled == TRUE);
	EnableTool(IDM_CURVE_MODE_AUTO_CLAMPED, bEnabled == TRUE);
	EnableTool(IDM_CURVE_MODE_USER, bEnabled == TRUE);
	EnableTool(IDM_CURVE_MODE_BREAK, bEnabled == TRUE);
	EnableTool(IDM_CURVE_MODE_LINEAR, bEnabled == TRUE);
	EnableTool(IDM_CURVE_MODE_CONSTANT, bEnabled == TRUE);

	EnableTool(IDM_CURVE_FLATTEN_TANGENTS_TO_AXIS, bEnabled == TRUE );
	EnableTool(IDM_CURVE_STRAIGHTEN_TANGENTS, bEnabled == TRUE );
}

void WxCurveEdToolBar::SetEditMode(ECurveEdMode NewMode)
{
	ToggleTool(IDM_CURVE_PANMODE, NewMode == CEM_Pan);
	ToggleTool(IDM_CURVE_ZOOMMODE, NewMode == CEM_Zoom);
}

/*-----------------------------------------------------------------------------
WxMBCurveLabelMenu.
-----------------------------------------------------------------------------*/

WxMBCurveLabelMenu::WxMBCurveLabelMenu(WxCurveEditor* CurveEd)
{
	Append( IDM_CURVE_REMOVECURVE, *LocalizeUnrealEd("RemoveCurve"), TEXT("") );
	AppendSeparator();
	Append( IDM_CURVE_REMOVEALLCURVES, *LocalizeUnrealEd("RemoveAllCurves"), TEXT("") );
	AppendSeparator();
	Append( IDM_CURVE_PRESETCURVE, *LocalizeUnrealEd("PresetCurve"), TEXT("") );
	Append( IDM_CURVE_SAVE_PRESETCURVE, *LocalizeUnrealEd("SavePresetCurve"), TEXT("") );

	// Grab curve editor interface for the curve entry that was right clicked on
	if( ( CurveEd->RightClickCurveIndex >= 0 ) &&
		( CurveEd->RightClickCurveIndex < CurveEd->EdSetup->Tabs( CurveEd->EdSetup->ActiveTab ).Curves.Num() ) )
	{
		FCurveEdEntry* Entry = &( CurveEd->EdSetup->Tabs( CurveEd->EdSetup->ActiveTab ).Curves( CurveEd->RightClickCurveIndex ) );
		if( Entry != NULL )
		{
			FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer( *Entry );
			if( EdInterface != NULL )
			{
				// OK, now check to see if this curve is using 'legacy' tangents.  If so, we'll provide
				// an option to upgrade in the context menu
				if( EdInterface->UsingLegacyInterpMethod() )
				{
					AppendSeparator();
					Append( IDM_CURVE_CurveLabelContext_UpgradeInterpMethod, *LocalizeUnrealEd( "CurveEd_CurveLabelContext_UpgradeInterpMethod" ) );
				}
			}
		}
	}
}

WxMBCurveLabelMenu::~WxMBCurveLabelMenu()
{

}

/*-----------------------------------------------------------------------------
WxMBCurveKeyMenu.
-----------------------------------------------------------------------------*/

WxMBCurveKeyMenu::WxMBCurveKeyMenu(WxCurveEditor* CurveEd)
{
	if(CurveEd->SelectedKeys.Num() == 1)
	{
		Append( IDM_CURVE_SETKEYIN, *LocalizeUnrealEd("SetTime"), TEXT("") );
		Append( IDM_CURVE_SETKEYOUT, *LocalizeUnrealEd("SetValue"), TEXT("") );

		FCurveEdSelKey& SelKey = CurveEd->SelectedKeys(0);
		FCurveEdEntry& Entry = CurveEd->EdSetup->Tabs(CurveEd->EdSetup->ActiveTab).Curves(SelKey.CurveIndex);
		FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
		
		if(Entry.bColorCurve && EdInterface->GetNumSubCurves() == 3)
		{
			Append( IDM_CURVE_SETKEYCOLOR, *LocalizeUnrealEd("SetColor"), TEXT("") );
		}

		AppendSeparator();
	}

	Append( IDM_CURVE_DeleteSelectedKeys, *LocalizeUnrealEd( "CurveEd_KeyContext_DeleteSelected" ) );
}

WxMBCurveKeyMenu::~WxMBCurveKeyMenu()
{

}

/*-----------------------------------------------------------------------------
	WxMBCurveMenu
-----------------------------------------------------------------------------*/
WxMBCurveMenu::WxMBCurveMenu(WxCurveEditor* CurveEd)
{
	Append(IDM_CURVE_SCALETIMES, *LocalizeUnrealEd("ScaleTimes"), *LocalizeUnrealEd("ScaleTimes_ToolTip"));
	Append(IDM_CURVE_SCALEVALUES, *LocalizeUnrealEd("ScaleValues"), *LocalizeUnrealEd("ScaleValues_ToolTip"));
}

WxMBCurveMenu::~WxMBCurveMenu()
{
}
