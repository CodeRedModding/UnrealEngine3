/*=============================================================================
	CurveEd.cpp: FInterpCurve editor
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"

const INT	LabelWidth			= 140;
const INT	LabelEntryHeight	= 25;
const INT	ColorKeyWidth		= 6;
const FLOAT ZoomSpeed			= 0.1f;
const FLOAT MouseZoomSpeed		= 0.015f;
const FLOAT HandleLength		= 30.f;
const FLOAT FitMargin			= 0.1f;

static const FColor LabelColor(180, 180, 180);
static const FColor GridColor(125, 125, 125);
static const FColor GridTextColor(200, 200, 200);
static const FColor GraphBkgColor(60, 60, 60);
static const FColor LabelBlockBkgColor(130, 130, 130);
static const FColor SelectedKeyColor(255, 225, 0);

/*-----------------------------------------------------------------------------
	FCurveEdViewportClient
-----------------------------------------------------------------------------*/

FCurveEdViewportClient::FCurveEdViewportClient(WxCurveEditor* InCurveEd)
{
	CurveEd = InCurveEd;

	OldMouseX = 0;
	OldMouseY = 0;

	bPanning = false;
	bZooming = false;
	bDraggingHandle = false;
	bMouseDown = false;
	bBegunMoving = false;
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


void FCurveEdViewportClient::Draw(FChildViewport* Viewport, FRenderInterface* RI)
{
	CurveEd->DrawCurveEditor(Viewport, RI);
}

void FCurveEdViewportClient::InputKey(FChildViewport* Viewport, FName Key, EInputEvent Event,FLOAT)
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
					if(HitResult->IsA(TEXT("HCurveEdLabelProxy")))
					{
						// Nothing on left click
					}
					else if(HitResult->IsA(TEXT("HCurveEdHideCurveProxy")))
					{
						INT CurveIndex = ((HCurveEdHideCurveProxy*)HitResult)->CurveIndex;

						CurveEd->ToggleCurveHidden(CurveIndex);
					}
					else if(HitResult->IsA(TEXT("HCurveEdKeyProxy")))
					{
						INT CurveIndex = ((HCurveEdKeyProxy*)HitResult)->CurveIndex;
						INT SubIndex = ((HCurveEdKeyProxy*)HitResult)->SubIndex;
						INT KeyIndex = ((HCurveEdKeyProxy*)HitResult)->KeyIndex;

						if(!bCtrlDown)
						{
							CurveEd->ClearKeySelection();
							CurveEd->AddKeyToSelection(CurveIndex, SubIndex, KeyIndex);

							if(Event == IE_DoubleClick)
							{
								CurveEd->DoubleClickedKey(CurveIndex, SubIndex, KeyIndex);
							}
						}

						bPanning = true;
					}
					else if(HitResult->IsA(TEXT("HCurveEdKeyHandleProxy")))
					{
						CurveEd->HandleCurveIndex = ((HCurveEdKeyHandleProxy*)HitResult)->CurveIndex;
						CurveEd->HandleSubIndex = ((HCurveEdKeyHandleProxy*)HitResult)->SubIndex;
						CurveEd->HandleKeyIndex = ((HCurveEdKeyHandleProxy*)HitResult)->KeyIndex;
						CurveEd->bHandleArriving = ((HCurveEdKeyHandleProxy*)HitResult)->bArriving;

						bDraggingHandle = true;
					}
					else if(HitResult->IsA(TEXT("HCurveEdLineProxy")))
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

				OldMouseX = HitX;
				OldMouseY = HitY;
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
						INT MaxX = Max(BoxStartX, BoxEndX) + 1;
						INT MaxY = Max(BoxStartY, BoxEndY) + 1;
						INT TestSizeX = MaxX - MinX;
						INT TestSizeY = MaxY - MinY;

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

								if(HitProxy && HitProxy->IsA(TEXT("HCurveEdKeyProxy")))
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
								if(HitResult && HitResult->IsA(TEXT("HCurveEdKeyProxy")))
								{
									INT CurveIndex = ((HCurveEdKeyProxy*)HitResult)->CurveIndex;
									INT SubIndex = ((HCurveEdKeyProxy*)HitResult)->SubIndex;
									INT KeyIndex = ((HCurveEdKeyProxy*)HitResult)->KeyIndex;

									if( CurveEd->KeyIsInSelection(CurveIndex, SubIndex, KeyIndex) )
									{
										CurveEd->RemoveKeyFromSelection(CurveIndex, SubIndex, KeyIndex);
									}
									else
									{
										CurveEd->AddKeyToSelection(CurveIndex, SubIndex, KeyIndex);
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
				}
			}
		}

		if(Event == IE_Released)
		{
			bMouseDown = false;
			DistanceDragged = 0;
			bPanning = false;
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
				if(HitResult->IsA(TEXT("HCurveEdLabelProxy")))
				{
					CurveEd->RightClickCurveIndex = ((HCurveEdLabelProxy*)HitResult)->CurveIndex;

					wxMenu* Menu = new WxMBCurveLabelMenu(CurveEd);
					FTrackPopupMenu tpm(CurveEd, Menu);
					tpm.Show();
					delete Menu;
				}
				else if(HitResult->IsA(TEXT("HCurveEdKeyProxy")))
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

						wxMenu* Menu = new WxMBCurveKeyMenu(CurveEd);
						FTrackPopupMenu tpm(CurveEd, Menu);
						tpm.Show();
						delete Menu;
					}
				}
			}
		}
	}
	else if( Key == KEY_MouseScrollUp && Event == IE_Pressed )
	{
		FLOAT SizeIn = CurveEd->EndIn - CurveEd->StartIn;
		FLOAT DeltaIn = ZoomSpeed * SizeIn;

		FLOAT SizeOut = CurveEd->EndOut - CurveEd->StartOut;
		FLOAT DeltaOut = ZoomSpeed * SizeOut;

		CurveEd->SetCurveView(CurveEd->StartIn - DeltaIn, CurveEd->EndIn + DeltaIn, CurveEd->StartOut - DeltaOut, CurveEd->EndOut + DeltaOut );
		Viewport->Invalidate();
	}
	else if( Key == KEY_MouseScrollDown && Event == IE_Pressed )
	{
		FLOAT SizeIn = CurveEd->EndIn - CurveEd->StartIn;
		FLOAT DeltaIn = ZoomSpeed * SizeIn;

		FLOAT SizeOut = CurveEd->EndOut - CurveEd->StartOut;
		FLOAT DeltaOut = ZoomSpeed * SizeOut;

		CurveEd->SetCurveView(CurveEd->StartIn + DeltaIn, CurveEd->EndIn - DeltaIn, CurveEd->StartOut + DeltaOut, CurveEd->EndOut - DeltaOut );
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
	}
	else if(Event == IE_Released)
	{
		if( Key == KEY_Z )
		{
			CurveEd->EdMode = CEM_Pan;
			CurveEd->ToolBar->SetEditMode(CEM_Pan);
		}
	}

}

void FCurveEdViewportClient::MouseMove(FChildViewport* Viewport, INT X, INT Y)
{
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);

	INT DeltaX = OldMouseX - X;
	OldMouseX = X;

	INT DeltaY = OldMouseY - Y;
	OldMouseY = Y;

	// Update mouse-over keypoint.
	HHitProxy* HitResult = Viewport->GetHitProxy(X,Y);
	if(HitResult && HitResult->IsA(TEXT("HCurveEdKeyProxy")))
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
					}

					CurveEd->MoveSelectedKeys( -DeltaIn, -DeltaOut );
				}
			}
			else if(bPanning)
			{
				CurveEd->SetCurveView(CurveEd->StartIn + DeltaIn, CurveEd->EndIn + DeltaIn, CurveEd->StartOut + DeltaOut, CurveEd->EndOut + DeltaOut );
			}
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
			ZoomDeltaIn = MouseZoomSpeed * SizeIn * Clamp<INT>(-DeltaX, -5, 5);		
		}

		FLOAT ZoomDeltaOut = 0.f;
		if(bLeftMouseDown)
		{
			FLOAT SizeOut = CurveEd->EndOut - CurveEd->StartOut;
			ZoomDeltaOut = MouseZoomSpeed * SizeOut * Clamp<INT>(DeltaY, -5, 5);
		}

		CurveEd->SetCurveView(CurveEd->StartIn - ZoomDeltaIn, CurveEd->EndIn + ZoomDeltaIn, CurveEd->StartOut - ZoomDeltaOut, CurveEd->EndOut + ZoomDeltaOut );
	}

	Viewport->Invalidate();
}

void FCurveEdViewportClient::InputAxis(FChildViewport* Viewport, FName Key, FLOAT Delta, FLOAT DeltaTime)
{

}

/*-----------------------------------------------------------------------------
	WxCurveEditor
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxCurveEditor, wxWindow )
	EVT_SIZE( WxCurveEditor::OnSize )
	EVT_TOOL( IDM_CURVE_FITHORZ, WxCurveEditor::OnFitHorz )
	EVT_TOOL( IDM_CURVE_FITVERT, WxCurveEditor::OnFitVert )
	EVT_MENU( IDM_CURVE_REMOVECURVE, WxCurveEditor::OnContextCurveRemove )
	EVT_MENU_RANGE( IDM_CURVE_PANMODE, IDM_CURVE_ZOOMMODE, WxCurveEditor::OnChangeMode )
	EVT_MENU_RANGE( IDM_CURVE_SETKEYIN, IDM_CURVE_SETKEYOUT, WxCurveEditor::OnSetKey )
	EVT_MENU( IDM_CURVE_SETKEYCOLOR, WxCurveEditor::OnSetKeyColor )
	EVT_MENU_RANGE( IDM_CURVE_MODE_AUTO, IDM_CURVE_MODE_CONSTANT, WxCurveEditor::OnChangeInterpMode )
END_EVENT_TABLE()

WxCurveEditor::WxCurveEditor( wxWindow* InParent, wxWindowID InID, UInterpCurveEdSetup* InEdSetup )
: wxWindow( InParent, InID )
{
	CurveEdVC = new FCurveEdViewportClient(this);
	CurveEdVC->Viewport = GEngine->Client->CreateWindowChildViewport(CurveEdVC, (HWND)GetHandle());
	CurveEdVC->Viewport->CaptureJoystickInput(false);

	ToolBar = new WxCurveEdToolBar( this, -1 );

	EdSetup = InEdSetup;

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

	bShowRegionMarker = false;
	RegionStart = 0.f;
	RegionEnd = 0.f;
	RegionFillColor = FColor(255,255,255,16);

	bSnapEnabled = false;
	InSnapAmount = 1.f;

	NotifyObject = NULL;

	StartIn = EdSetup->Tabs( EdSetup->ActiveTab ).ViewStartInput;
	EndIn = EdSetup->Tabs( EdSetup->ActiveTab ).ViewEndInput;
	StartOut = EdSetup->Tabs( EdSetup->ActiveTab ).ViewStartOutput;
	EndOut = EdSetup->Tabs( EdSetup->ActiveTab ).ViewEndOutput;

    wxSizeEvent Event;
	OnSize( Event );
}

WxCurveEditor::~WxCurveEditor()
{
	GEngine->Client->CloseViewport(CurveEdVC->Viewport);
	CurveEdVC->Viewport = NULL;
	delete CurveEdVC;
}

void WxCurveEditor::OnSize( wxSizeEvent& In )
{
	wxRect rc = GetClientRect();

	if(ToolBar)
	{
		INT ToolbarHeight = 26;

		rc.y += ToolbarHeight;
		rc.height -= ToolbarHeight;

		ToolBar->SetSize( rc.GetWidth(), ToolbarHeight );
	}

	::MoveWindow( (HWND)CurveEdVC->Viewport->GetWindow(), rc.x, rc.y, rc.GetWidth(), rc.GetHeight(), 1 );
}

void WxCurveEditor::OnFitHorz(wxCommandEvent& In)
{
	FLOAT MinIn = BIG_NUMBER;
	FLOAT MaxIn = -BIG_NUMBER;

	// Iterate over all curves to find the max/min Input values.
	for(INT i=0; i<EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num(); i++)
	{
		FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(i);
		if(!Entry.bHideCurve)
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

	// Do nothing if trying to fit something really small..
	if(Size > 0.001f)
	{
		SetCurveView( MinIn - FitMargin*Size, MaxIn + FitMargin*Size, StartOut, EndOut );

		CurveEdVC->Viewport->Invalidate();
	}
}

void WxCurveEditor::OnFitVert(wxCommandEvent& In)
{
	// Always include origin on vertical scaling
	FLOAT MinOut = BIG_NUMBER;
	FLOAT MaxOut = -BIG_NUMBER;

	for(INT i=0; i<EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num(); i++)
	{
		FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(i);
		if(!Entry.bHideCurve)
		{
			FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
			if(EdInterface)
			{
				FLOAT EntryMinOut, EntryMaxOut;
				EdInterface->GetOutRange(EntryMinOut, EntryMaxOut);

				MinOut = ::Min<FLOAT>(EntryMinOut, MinOut);
				MaxOut = ::Max<FLOAT>(EntryMaxOut, MaxOut);
			}
		}
	}

	FLOAT Size = MaxOut - MinOut;

	// If its too small - see if including the origin would give us a sensible range.
	if(Size < 0.001f)
	{
		MinOut = ::Min(0.f, MinOut);
		MaxOut = ::Max(0.f, MaxOut);
		Size = MaxOut - MinOut;
	}

	if(Size > 0.001f)
	{
		SetCurveView( StartIn, EndIn, MinOut - FitMargin*Size, MaxOut + FitMargin*Size );

		CurveEdVC->Viewport->Invalidate();
	}
}

/** Remove a Curve from the selected tab. */
void WxCurveEditor::OnContextCurveRemove(wxCommandEvent& In)
{
	if( RightClickCurveIndex < 0 || RightClickCurveIndex >= EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num() )
	{
		return;
	}

	EdSetup->Tabs(EdSetup->ActiveTab).Curves.Remove(RightClickCurveIndex);

	ClearKeySelection();

	CurveEdVC->Viewport->Invalidate();
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

	// Are we setting the input or ouptput value
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
		TitleString = TEXT("Set Time");
		CaptionString = TEXT("New Time:");
	}
	else
	{
		DefaultNum = FString::Printf( TEXT("%2.3f"), EdInterface->GetKeyOut(SelKey.SubIndex, SelKey.KeyIndex) );
		TitleString = TEXT("Set Value");
		CaptionString = TEXT("New Value:");
	}

	// Show generic string entry dialog box
	WxDlgGenericStringEntry dlg;
	INT Result = dlg.ShowModal( *TitleString, *CaptionString, *DefaultNum );
	if( Result != wxID_OK )
		return;

	// Convert from string to float (if we can).
	double dNewNum;
	UBOOL bIsNumber = dlg.StringEntry->GetValue().ToDouble(&dNewNum);
	if(!bIsNumber)
		return;
	FLOAT NewNum = (FLOAT)dNewNum;

	// Set then set using EdInterface.
	if(bSetIn)
	{
		EdInterface->SetKeyIn(SelKey.KeyIndex, NewNum);
	}
	else
	{
		EdInterface->SetKeyOut(SelKey.SubIndex, SelKey.KeyIndex, NewNum);
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

	// Get current value of curve as a colour.
	FColor InputColor;
	InputColor.R = appTrunc( Clamp<FLOAT>(EdInterface->GetKeyOut(0, SelKey.KeyIndex), 0.f, 255.9f) );
	InputColor.G = appTrunc( Clamp<FLOAT>(EdInterface->GetKeyOut(1, SelKey.KeyIndex), 0.f, 255.9f) );
	InputColor.B = appTrunc( Clamp<FLOAT>(EdInterface->GetKeyOut(2, SelKey.KeyIndex), 0.f, 255.9f) );

	// Display color picking dialog
	wxColourData cd;
	cd.SetChooseFull(1);
	cd.SetColour( wxColour(InputColor.R,InputColor.G,InputColor.B) );
	WxDlgColor dlg;
	dlg.Create( this, &cd );
	if( dlg.ShowModal() != wxID_OK )
		return;

	// Get result, and set back into each sub-track for selected keypoint
	wxColour clr = dlg.GetColourData().GetColour();

	EdInterface->SetKeyOut( 0, SelKey.KeyIndex, (FLOAT)(clr.Red()) );
	EdInterface->SetKeyOut( 1, SelKey.KeyIndex, (FLOAT)(clr.Green()) );
	EdInterface->SetKeyOut( 2, SelKey.KeyIndex, (FLOAT)(clr.Blue()) );

	CurveEdVC->Viewport->Invalidate();
}

/** Set the viewable area for the current tab. */
void WxCurveEditor::OnChangeInterpMode(wxCommandEvent& In)
{
	EInterpCurveMode NewInterpMode = CIM_Linear;
	if(In.GetId() == IDM_CURVE_MODE_AUTO)
		NewInterpMode = CIM_CurveAuto;
	else if(In.GetId() == IDM_CURVE_MODE_USER)
		NewInterpMode = CIM_CurveUser;
	else if(In.GetId() == IDM_CURVE_MODE_BREAK)
		NewInterpMode = CIM_CurveBreak;
	else if(In.GetId() == IDM_CURVE_MODE_LINEAR)
		NewInterpMode = CIM_Linear;
	else if(In.GetId() == IDM_CURVE_MODE_CONSTANT)
		NewInterpMode = CIM_Constant;
	else
		check(0);

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

/** Set the viewable area for the current tab. */
void WxCurveEditor::SetCurveView(FLOAT InStartIn, FLOAT InEndIn, FLOAT InStartOut, FLOAT InEndOut)
{
	// Ensure we are not zooming too big or too small...
	FLOAT InSize = InEndIn - InStartIn;
	FLOAT OutSize = InEndOut - InStartOut;
	if( InSize < 0.001f  || InSize > 100000.f || OutSize < 0.001f || OutSize > 100000.f)
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
		EdInterface->SetKeyInterpMode(NewKeyIndex, CIM_CurveAuto);

		if(NotifyObject)
		{
			NotifyObject->PostEditCurve();
		}

		CurveEdVC->Viewport->Invalidate();
	}
	return NewKeyIndex;
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
void WxCurveEditor::DrawCurveEditor(FChildViewport* Viewport, FRenderInterface* RI)
{
	RI->Clear( GraphBkgColor );

	CurveViewX = Viewport->GetSizeX() - LabelWidth;
	CurveViewY = Viewport->GetSizeY();

	PixelsPerIn = CurveViewX/(EndIn - StartIn);
	PixelsPerOut = CurveViewY/(EndOut - StartOut);

	UBOOL bHitTesting = RI->IsHitTesting();

	// Draw background grid.
	DrawGrid(Viewport, RI);

	// Draw selected-region if desired.
	if(bShowRegionMarker)
	{
		FIntPoint RegionStartPos = CalcScreenPos( FVector2D(RegionStart, 0) );
		FIntPoint RegionEndPos = CalcScreenPos( FVector2D(RegionEnd, 0) );

		RI->DrawTile(RegionStartPos.X, 0, RegionEndPos.X - RegionStartPos.X, CurveViewY, 0.f, 0.f, 1.f, 1.f, RegionFillColor);
	}

	// Draw each curve
	for(INT i=0; i<EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num(); i++)
	{
		// Draw curve itself.
		FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(i);
		if(!Entry.bHideCurve)
		{
			DrawEntry(Viewport, RI, Entry, i);
		}
	}

	// Draw key background block down left hand side.
	if(bHitTesting) RI->SetHitProxy( new HCurveEdLabelBkgProxy() );
	RI->DrawTile( 0, 0, LabelWidth, CurveViewY, 0.f, 0.f, 1.f, 1.f, LabelBlockBkgColor );
	if(bHitTesting) RI->SetHitProxy( NULL );

	// Draw key entry for each curve
	INT CurrentKeyY = 0;
	for(INT i=0; i<EdSetup->Tabs(EdSetup->ActiveTab).Curves.Num(); i++)
	{
		// Draw key entry
		FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(i);

		// Draw background, color-square and text
		if(bHitTesting) RI->SetHitProxy( new HCurveEdLabelProxy(i) );
		RI->DrawTile( 0, CurrentKeyY, LabelWidth, LabelEntryHeight, 0.f, 0.f, 1.f, 1.f, LabelColor );
		RI->DrawTile( 0, CurrentKeyY, ColorKeyWidth, LabelEntryHeight, 0.f, 0.f, 1.f, 1.f, Entry.CurveColor );
		RI->DrawShadowedString( ColorKeyWidth+3, CurrentKeyY+4, *(Entry.CurveName), GEngine->SmallFont, FLinearColor::White );
		if(bHitTesting) RI->SetHitProxy( NULL );

		// Draw hide/unhide button
		FColor ButtonColor = Entry.bHideCurve ? FColor(112,112,112) : FColor(255,200,0);
		if(bHitTesting) RI->SetHitProxy( new HCurveEdHideCurveProxy(i) );
		RI->DrawTile( LabelWidth - 12, CurrentKeyY + LabelEntryHeight - 12, 8, 8, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );
		RI->DrawTile( LabelWidth - 11, CurrentKeyY + LabelEntryHeight - 11, 6, 6, 0.f, 0.f, 1.f, 1.f, ButtonColor );
		if(bHitTesting) RI->SetHitProxy( NULL );

		CurrentKeyY += 	LabelEntryHeight;

		// Draw line under each key entry
		RI->DrawTile( 0, CurrentKeyY-1, LabelWidth, 1, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );
	}

	// Draw line above top-most key entry.
	RI->DrawTile( 0, 0, LabelWidth, 1, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );

	// Line down right of key.
	RI->DrawTile(LabelWidth, 0, 1, CurveViewY, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );

	// Draw box-selection region
	if(CurveEdVC->bBoxSelecting)
	{
		INT MinX = Min(CurveEdVC->BoxStartX, CurveEdVC->BoxEndX);
		INT MinY = Min(CurveEdVC->BoxStartY, CurveEdVC->BoxEndY);
		INT MaxX = Max(CurveEdVC->BoxStartX, CurveEdVC->BoxEndX);
		INT MaxY = Max(CurveEdVC->BoxStartY, CurveEdVC->BoxEndY);

		RI->DrawLine2D(MinX, MinY, MaxX, MinY, FColor(255,0,0));
		RI->DrawLine2D(MaxX, MinY, MaxX, MaxY, FColor(255,0,0));
		RI->DrawLine2D(MaxX, MaxY, MinX, MaxY, FColor(255,0,0));
		RI->DrawLine2D(MinX, MaxY, MinX, MinY, FColor(255,0,0));
	}

	if(bShowPositionMarker)
	{
		FIntPoint MarkerScreenPos = CalcScreenPos( FVector2D(MarkerPosition, 0) );
		if(MarkerScreenPos.X >= LabelWidth)
		{
			RI->DrawLine2D( MarkerScreenPos.X, 0, MarkerScreenPos.X, CurveViewY, MarkerColor );
		}
	}

	if(bShowEndMarker)
	{
		FIntPoint EndScreenPos = CalcScreenPos( FVector2D(EndMarkerPosition, 0) );
		if(EndScreenPos.X >= LabelWidth)
		{
			RI->DrawLine2D( EndScreenPos.X, 0, EndScreenPos.X, CurveViewY, FLinearColor::White );
		}
	}
}


// Draw a linear approximation to curve every CurveDrawRes pixels.
static const INT CurveDrawRes = 5;

static FColor GetLineColor(FCurveEdInterface* EdInterface, FLOAT InVal)
{
	FColor StepColor;

	INT NumSubs = EdInterface->GetNumSubCurves();
	if(NumSubs == 3)
	{
		StepColor.R = appTrunc( Clamp<FLOAT>(EdInterface->EvalSub(0, InVal), 0.f, 255.9f) );
		StepColor.G = appTrunc( Clamp<FLOAT>(EdInterface->EvalSub(1, InVal), 0.f, 255.9f) );
		StepColor.B = appTrunc( Clamp<FLOAT>(EdInterface->EvalSub(2, InVal), 0.f, 255.9f) );
		StepColor.A = 255;
	}
	else if(NumSubs == 1)
	{
		StepColor.R = appTrunc( Clamp<FLOAT>(EdInterface->EvalSub(0, InVal), 0.f, 255.9f) );
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

static FVector2D CalcTangentDir(FLOAT Tangent)
{
	FLOAT Angle = appAtan(Tangent);

	FVector2D Result;
	Result.X = appCos(Angle);
	Result.Y = -appSin(Angle);
	return Result;
}

static FLOAT CalcTangent(FVector2D HandleDelta)
{
	// Ensure X is positive and non-zero.
	HandleDelta.X = ::Max<FLOAT>(HandleDelta.X, static_cast<FLOAT>( KINDA_SMALL_NUMBER ) );

	// Tangent is gradient of handle.
	FLOAT NewTangent = HandleDelta.Y/HandleDelta.X;

	return NewTangent;
}

/** Draw one particular curve using the supplied FRenderInterface. */
void WxCurveEditor::DrawEntry( FChildViewport* Viewport, FRenderInterface* RI, const FCurveEdEntry& Entry, INT CurveIndex )
{
	FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
	if(!EdInterface)
	{
		return;
	}

	UBOOL bHitTesting = RI->IsHitTesting();

	INT NumSubs = EdInterface->GetNumSubCurves();
	INT NumKeys = EdInterface->GetNumKeys();

	for(INT SubIdx=0; SubIdx<NumSubs; SubIdx++)
	{
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
				FLOAT DrawTrackInRes = CurveDrawRes/PixelsPerIn;
				INT NumSteps = appCeil( (NewKey.X - OldKey.X)/DrawTrackInRes );
				FLOAT DrawSubstep = (NewKey.X - OldKey.X)/NumSteps;

				// Find position on first keyframe.
				FVector2D Old = OldKey;
				FIntPoint OldPos = OldKeyPos;

				BYTE InterpMode = EdInterface->GetKeyInterpMode(KeyIdx-1);

				if(bHitTesting) RI->SetHitProxy( new HCurveEdLineProxy(CurveIndex, SubIdx) );
				// For constant interpolation - don't draw ticks - just draw dotted line.
				if(InterpMode == CIM_Constant)
				{
					RI->DrawLine2D( OldKeyPos.X, OldKeyPos.Y, NewKeyPos.X, OldKeyPos.Y, Entry.CurveColor );
					RI->DrawLine2D( NewKeyPos.X, OldKeyPos.Y, NewKeyPos.X, NewKeyPos.Y, Entry.CurveColor );
				}
				else if(InterpMode == CIM_Linear && !Entry.bColorCurve)
				{
					RI->DrawLine2D( OldKeyPos.X, OldKeyPos.Y, NewKeyPos.X, NewKeyPos.Y, Entry.CurveColor );
				}
				else
				{
					// Then draw a line for each substep.
					for(INT j=1; j<NumSteps+1; j++)
					{
						FVector2D New;
						New.X = OldKey.X + j*DrawSubstep;
						New.Y = EdInterface->EvalSub(SubIdx, New.X);

						FColor StepColor = Entry.bColorCurve ? GetLineColor(EdInterface, New.X) : Entry.CurveColor;

						FIntPoint NewPos = CalcScreenPos(New);

						RI->DrawLine2D( OldPos.X, OldPos.Y, NewPos.X, NewPos.Y, StepColor);

						Old = New;
						OldPos = NewPos;
					}
				}

				if(bHitTesting) RI->SetHitProxy( NULL );
			}

			OldKey = NewKey;
			OldKeyPos = NewKeyPos;
		}

		// Draw lines to continue curve beyond last and before first.
		if(bHitTesting) RI->SetHitProxy( new HCurveEdLineProxy(CurveIndex, SubIdx) );

		if( NumKeys > 0 )
		{
			FLOAT RangeStart, RangeEnd;
			EdInterface->GetInRange(RangeStart, RangeEnd);

			if( RangeStart > StartIn )
			{
				FVector2D FirstKey;
				FirstKey.X = RangeStart;
				FirstKey.Y = EdInterface->GetKeyOut(SubIdx, 0);

				FColor StepColor = Entry.bColorCurve ? GetLineColor(EdInterface, RangeStart) : Entry.CurveColor;
				FIntPoint FirstKeyPos = CalcScreenPos(FirstKey);

				RI->DrawLine2D(LabelWidth, FirstKeyPos.Y, FirstKeyPos.X, FirstKeyPos.Y, StepColor);
			}

			if( RangeEnd < EndIn )
			{
				FVector2D LastKey;
				LastKey.X = RangeEnd;
				LastKey.Y = EdInterface->GetKeyOut(SubIdx, NumKeys-1);;

				FColor StepColor = Entry.bColorCurve ? GetLineColor(EdInterface, RangeEnd) : Entry.CurveColor;
				FIntPoint LastKeyPos = CalcScreenPos(LastKey);

				RI->DrawLine2D(LastKeyPos.X, LastKeyPos.Y, LabelWidth+CurveViewX, LastKeyPos.Y, StepColor);
			}
		}
		else // No points - draw line at zero.
		{
			FIntPoint OriginPos = CalcScreenPos( FVector2D(0,0) );
			RI->DrawLine2D( LabelWidth, OriginPos.Y, LabelWidth+CurveViewX, OriginPos.Y, Entry.CurveColor );
		}

		if(bHitTesting) RI->SetHitProxy( NULL );

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

			if(bHitTesting) RI->SetHitProxy( new HCurveEdKeyProxy(CurveIndex, SubIdx, KeyIdx) );
			RI->DrawTile( NewKeyPos.X-3, NewKeyPos.Y-3, 7, 7, 0.f, 0.f, 1.f, 1.f, BorderColor );
			RI->DrawTile( NewKeyPos.X-2, NewKeyPos.Y-2, 5, 5, 0.f, 0.f, 1.f, 1.f, CenterColor );
			if(bHitTesting) RI->SetHitProxy( NULL );

			// If previous section is a curve- show little handles.
			if(bSelectedKey)
			{
				FLOAT ArriveTangent, LeaveTangent;
				EdInterface->GetTangents(SubIdx, KeyIdx, ArriveTangent, LeaveTangent);	

				BYTE PrevMode = (KeyIdx > 0) ? EdInterface->GetKeyInterpMode(KeyIdx-1) : 255;
				BYTE NextMode = (KeyIdx < NumKeys-1) ? EdInterface->GetKeyInterpMode(KeyIdx) : 255;

				// If not first point, and previous mode was a curve type.
				if(PrevMode == CIM_CurveAuto || PrevMode == CIM_CurveUser || PrevMode == CIM_CurveBreak)
				{
					FVector2D HandleDir = CalcTangentDir((PixelsPerOut/PixelsPerIn) * ArriveTangent);

					FIntPoint HandlePos;
					HandlePos.X = NewKeyPos.X - appRound( HandleDir.X * HandleLength );
					HandlePos.Y = NewKeyPos.Y - appRound( HandleDir.Y * HandleLength );

					RI->DrawLine2D(NewKeyPos.X, NewKeyPos.Y, HandlePos.X, HandlePos.Y, FColor(255,255,255));

					if(bHitTesting) RI->SetHitProxy( new HCurveEdKeyHandleProxy(CurveIndex, SubIdx, KeyIdx, true) );
					RI->DrawTile(HandlePos.X-2, HandlePos.Y-2, 5, 5, 0.f, 0.f, 1.f, 1.f, FColor(255,255,255));
					if(bHitTesting) RI->SetHitProxy( NULL );
				}

				// If next section is a curve, draw leaving handle.
				if(NextMode == CIM_CurveAuto || NextMode == CIM_CurveUser || NextMode == CIM_CurveBreak)
				{
					FVector2D HandleDir = CalcTangentDir((PixelsPerOut/PixelsPerIn) * LeaveTangent);

					FIntPoint HandlePos;
					HandlePos.X = NewKeyPos.X + appRound( HandleDir.X * HandleLength );
					HandlePos.Y = NewKeyPos.Y + appRound( HandleDir.Y * HandleLength );

					RI->DrawLine2D(NewKeyPos.X, NewKeyPos.Y, HandlePos.X, HandlePos.Y, FColor(255,255,255));

					if(bHitTesting) RI->SetHitProxy( new HCurveEdKeyHandleProxy(CurveIndex, SubIdx, KeyIdx, false) );
					RI->DrawTile(HandlePos.X-2, HandlePos.Y-2, 5, 5, 0.f, 0.f, 1.f, 1.f, FColor(255,255,255));
					if(bHitTesting) RI->SetHitProxy( NULL );
				}
			}

			// If mouse is over this keypoint, show its value
			if( CurveIndex == MouseOverCurveIndex &&
				SubIdx == MouseOverSubIndex &&
				KeyIdx == MouseOverKeyIndex )
			{
				FString KeyComment = FString::Printf( TEXT("(%3.2f,%3.2f)"), NewKey.X, NewKey.Y );
				RI->DrawString( NewKeyPos.X + 5, NewKeyPos.Y - 5, *KeyComment, GEngine->SmallFont, GridTextColor );
			}
		}
	}
}

static FLOAT GetGridSpacing(INT GridNum)
{
	if(GridNum & 0x01) // Odd numbers
	{
		return appPow( 10.f, 0.5f*((FLOAT)(GridNum-1)) + 1.f );
	}
	else // Even numbers
	{
		return 0.5f * appPow( 10.f, 0.5f*((FLOAT)(GridNum)) + 1.f );
	}
}

/** Draw the background grid and origin lines. */
void WxCurveEditor::DrawGrid(FChildViewport* Viewport, FRenderInterface* RI)
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
	RI->StringSize( GEngine->SmallFont, XL, YL, TEXT("0123456789") );

	// Draw input grid

	INT InNum = appFloor(StartIn/InGridSpacing);
	while( InNum*InGridSpacing < EndIn )
	{
		// Draw grid line.
		FIntPoint GridPos = CalcScreenPos( FVector2D(InNum*InGridSpacing, 0.f) );
		RI->DrawLine2D( GridPos.X, 0, GridPos.X, CurveViewY, GridColor );
		InNum++;
	}

	// Draw output grid

	INT OutNum = appFloor(StartOut/OutGridSpacing);
	while( OutNum*OutGridSpacing < EndOut )
	{
		FIntPoint GridPos = CalcScreenPos( FVector2D(0.f, OutNum*OutGridSpacing) );
		RI->DrawLine2D( LabelWidth, GridPos.Y, LabelWidth + CurveViewX, GridPos.Y, GridColor );
		OutNum++;
	}

	// Calculate screen position of graph origin and draw white lines to indicate it

	FIntPoint OriginPos = CalcScreenPos( FVector2D(0,0) );

	RI->DrawLine2D( LabelWidth, OriginPos.Y, LabelWidth+CurveViewX, OriginPos.Y, FColor(255,255,255) );
	RI->DrawLine2D( OriginPos.X, 0, OriginPos.X, CurveViewY, FColor(255,255,255) );


	// Draw input labels

	InNum = appFloor(StartIn/InGridSpacing);
	while( InNum*InGridSpacing < EndIn )
	{
		// Draw value label
		FIntPoint GridPos = CalcScreenPos( FVector2D(InNum*InGridSpacing, 0.f) );
		FString ScaleLabel = FString::Printf( TEXT("%3.2f"), InNum*InGridSpacing );
		RI->DrawString( GridPos.X + 2, CurveViewY - YL - 2, *ScaleLabel, GEngine->SmallFont, GridTextColor );
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
			RI->DrawString( LabelWidth + 2, GridPos.Y - YL - 2, *ScaleLabel, GEngine->SmallFont, GridTextColor );
		}
		OutNum++;
	}
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
			if(Entry.bColorCurve)
			{
				NewOut = Clamp<FLOAT>(NewOut, 0.f, 255.9f);
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
						TestKey.SubIndex == SelKey.SubIndex &&
						TestKey.KeyIndex > OldKeyIndex && 
						TestKey.KeyIndex <= NewKeyIndex)
					{
						TestKey.KeyIndex--;
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
						TestKey.SubIndex == SelKey.SubIndex &&
						TestKey.KeyIndex < OldKeyIndex && 
						TestKey.KeyIndex >= NewKeyIndex)
					{
						TestKey.KeyIndex++;
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
	if(InterpMode == CIM_CurveAuto)
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

void WxCurveEditor::SetInSnap(UBOOL bEnabled, FLOAT SnapAmount)
{
	bSnapEnabled = bEnabled;
	InSnapAmount = SnapAmount;
}

void WxCurveEditor::SetNotifyObject(FCurveEdNotifyInterface* NewNotifyObject)
{
	NotifyObject = NewNotifyObject;
}

void WxCurveEditor::ToggleCurveHidden(INT InCurveIndex)
{
	FCurveEdEntry& Entry = EdSetup->Tabs(EdSetup->ActiveTab).Curves(InCurveIndex);
	Entry.bHideCurve = !Entry.bHideCurve;

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


/*-----------------------------------------------------------------------------
WxCurveEdToolBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxCurveEdToolBar, wxToolBar )
END_EVENT_TABLE()

WxCurveEdToolBar::WxCurveEdToolBar( wxWindow* InParent, wxWindowID InID )
: wxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_3DBUTTONS )
{
	// create the return to parent sequence button
	FitHorzB.Load(TEXT("CUR_FitHorz"));
	FitVertB.Load(TEXT("CUR_FitVert"));
	PanB.Load(TEXT("CUR_Pan"));
	ZoomB.Load(TEXT("CUR_Zoom"));
	ModeAutoB.Load(TEXT("CUR_ModeAuto"));
	ModeUserB.Load(TEXT("CUR_ModeUser"));
	ModeBreakB.Load(TEXT("CUR_ModeBreak"));
	ModeLinearB.Load(TEXT("CUR_ModeLinear"));
	ModeConstantB.Load(TEXT("CUR_ModeConstant"));

	SetToolBitmapSize( wxSize( 14, 14 ) );

	AddSeparator();
	AddTool(IDM_CURVE_FITHORZ, FitHorzB, TEXT("Fit Horizontally"));
	AddTool(IDM_CURVE_FITVERT, FitVertB, TEXT("Fit Vertically"));
	AddSeparator();
	AddCheckTool(IDM_CURVE_PANMODE, TEXT("Pan Mode"), PanB );
	AddCheckTool(IDM_CURVE_ZOOMMODE, TEXT("Zoom Mode"), ZoomB );
	AddSeparator();
	AddCheckTool(IDM_CURVE_MODE_AUTO, TEXT("Curve (Auto)"), ModeAutoB);
	AddCheckTool(IDM_CURVE_MODE_USER, TEXT("Curve (User)"), ModeUserB);
	AddCheckTool(IDM_CURVE_MODE_BREAK, TEXT("Curve (Break)"), ModeBreakB);
	AddCheckTool(IDM_CURVE_MODE_LINEAR, TEXT("Linear"), ModeLinearB);
	AddCheckTool(IDM_CURVE_MODE_CONSTANT, TEXT("Constant"), ModeConstantB);

	SetEditMode(CEM_Pan);
	SetCurveMode(CIM_Unknown);

	Realize();
}

WxCurveEdToolBar::~WxCurveEdToolBar( void )
{
}

void WxCurveEdToolBar::SetCurveMode( EInterpCurveMode NewMode )
{
	ToggleTool( IDM_CURVE_MODE_AUTO,        NewMode == CIM_CurveAuto );
	ToggleTool( IDM_CURVE_MODE_USER,        NewMode == CIM_CurveUser );
	ToggleTool( IDM_CURVE_MODE_BREAK,       NewMode == CIM_CurveBreak );
	ToggleTool( IDM_CURVE_MODE_LINEAR,      NewMode == CIM_Linear );
	ToggleTool( IDM_CURVE_MODE_CONSTANT,    NewMode == CIM_Constant );
}

void WxCurveEdToolBar::SetButtonsEnabled( UBOOL bEnabled )
{
    EnableTool( IDM_CURVE_MODE_AUTO,     bEnabled ? true : false );
	EnableTool( IDM_CURVE_MODE_USER,     bEnabled ? true : false );
	EnableTool( IDM_CURVE_MODE_BREAK,    bEnabled ? true : false );
	EnableTool( IDM_CURVE_MODE_LINEAR,   bEnabled ? true : false );
	EnableTool( IDM_CURVE_MODE_CONSTANT, bEnabled ? true : false );
}

void WxCurveEdToolBar::SetEditMode( ECurveEdMode NewMode )
{
	ToggleTool( IDM_CURVE_PANMODE,  NewMode == CEM_Pan );
	ToggleTool( IDM_CURVE_ZOOMMODE, NewMode == CEM_Zoom );
}

/*-----------------------------------------------------------------------------
WxMBCurveLabelMenu.
-----------------------------------------------------------------------------*/

WxMBCurveLabelMenu::WxMBCurveLabelMenu( WxCurveEditor * CurveEd )
{
	Append( IDM_CURVE_REMOVECURVE, TEXT("Remove Curve"), TEXT("") );
}

WxMBCurveLabelMenu::~WxMBCurveLabelMenu( void )
{
}

/*-----------------------------------------------------------------------------
WxMBCurveKeyMenu.
-----------------------------------------------------------------------------*/

WxMBCurveKeyMenu::WxMBCurveKeyMenu(WxCurveEditor* CurveEd)
{
	if(CurveEd->SelectedKeys.Num() == 1)
	{
		Append( IDM_CURVE_SETKEYIN, TEXT("Set Time"), TEXT("") );
		Append( IDM_CURVE_SETKEYOUT, TEXT("Set Value"), TEXT("") );

		FCurveEdSelKey& SelKey = CurveEd->SelectedKeys(0);
		FCurveEdEntry& Entry = CurveEd->EdSetup->Tabs(CurveEd->EdSetup->ActiveTab).Curves(SelKey.CurveIndex);
		FCurveEdInterface* EdInterface = UInterpCurveEdSetup::GetCurveEdInterfacePointer(Entry);
		
		if(Entry.bColorCurve && EdInterface->GetNumSubCurves() == 3)
		{
			Append( IDM_CURVE_SETKEYCOLOR, TEXT("Set Color"), TEXT("") );
		}
	}
}

WxMBCurveKeyMenu::~WxMBCurveKeyMenu()
{

}