/*=============================================================================
	UnLinkedObjEditor.cpp: Base class for boxes-and-lines editing
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "UnLinkedObjDrawUtils.h"

const static FLOAT	ZoomIncrement       = 0.1f;
const static INT	ScrollBorderSize    = 20;
const static FLOAT	ScrollBorderSpeed   = 400.0f;

/*-----------------------------------------------------------------------------
FLinkedObjViewportClient
-----------------------------------------------------------------------------*/

FLinkedObjViewportClient::FLinkedObjViewportClient( class WxLinkedObjEd* InObjEditor )
{
	ObjEditor			= InObjEditor;

	Origin2D			= FIntPoint(0, 0);
	Zoom2D				= 1.0f;
	MinZoom2D			= 0.1f;
	MaxZoom2D			= 1.f;

	bMouseDown			= false;
	OldMouseX			= 0;
	OldMouseY			= 0;

	BoxStartX			= 0;
	BoxStartY			= 0;
	BoxEndX				= 0;
	BoxEndY				= 0;

	ScrollAccum			= FVector2D(0,0);

	DistanceDragged		= 0;

	bTransactionBegun	= false;
	bMakingLine			= false;
	bSpecialDrag		= false;
	bBoxSelecting		= false;
	bAllowScroll		= true;

	MouseOverObject		= NULL;
	MouseOverTime		= 0.f;

	SpecialIndex		= 0;
		
	Realtime			= true;
	RealtimeAudio		= false;
}

FLinkedObjViewportClient::~FLinkedObjViewportClient()
{

}

void FLinkedObjViewportClient::Draw(FChildViewport* Viewport, FRenderInterface* RI)
{
	RI->SetOrigin2D( Origin2D );
	RI->SetZoom2D( Zoom2D );

	// Erase background
	RI->Clear( FColor(197,197,197) );

	ObjEditor->DrawLinkedObjects( RI );

	// Draw new line
	if(bMakingLine && !RI->IsHitTesting())
	{
		FIntPoint StartPoint = ObjEditor->GetSelectedConnLocation(RI);
		FIntPoint EndPoint( (NewX - RI->Origin2D.X)/RI->Zoom2D, (NewY - RI->Origin2D.Y)/RI->Zoom2D );
		INT ConnType = ObjEditor->GetSelectedConnectorType();
		FColor LinkColor = ObjEditor->GetMakingLinkColor();

		if(ObjEditor->bDrawCurves)// && (ConnType != LOC_VARIABLE) && (ConnType != LOC_EVENT))
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
				FLinkedObjDrawUtils::DrawSpline(RI, StartPoint, Tension*FVector2D(-1,0), EndPoint, Tension*FVector2D(-1,0), LinkColor, false);
			}
			else if(ConnType == LOC_OUTPUT)
			{
				FLinkedObjDrawUtils::DrawSpline(RI, StartPoint, Tension*FVector2D(1,0), EndPoint, Tension*FVector2D(1,0), LinkColor, false);
			}
			else if(ConnType == LOC_VARIABLE)
			{
				FLinkedObjDrawUtils::DrawSpline(RI, StartPoint, Tension*FVector2D(0,1), EndPoint, FVector2D(0,0), LinkColor, false);
			}
			else
			{
				FLinkedObjDrawUtils::DrawSpline(RI, StartPoint, Tension*FVector2D(0,1), EndPoint, Tension*FVector2D(0,1), LinkColor, false);
			}
		}
		else
		{
			RI->DrawLine2D( StartPoint.X, StartPoint.Y, EndPoint.X, EndPoint.Y, LinkColor );
		}
	}

	// Draw the box select box
	if(bBoxSelecting)
	{
		INT MinX = (Min(BoxStartX, BoxEndX) - RI->Origin2D.X)/RI->Zoom2D;
		INT MinY = (Min(BoxStartY, BoxEndY) - RI->Origin2D.Y)/RI->Zoom2D;
		INT MaxX = (Max(BoxStartX, BoxEndX) - RI->Origin2D.X)/RI->Zoom2D;
		INT MaxY = (Max(BoxStartY, BoxEndY) - RI->Origin2D.Y)/RI->Zoom2D;

		RI->DrawLine2D(MinX, MinY, MaxX, MinY, FColor(255,0,0));
		RI->DrawLine2D(MaxX, MinY, MaxX, MaxY, FColor(255,0,0));
		RI->DrawLine2D(MaxX, MaxY, MinX, MaxY, FColor(255,0,0));
		RI->DrawLine2D(MinX, MaxY, MinX, MinY, FColor(255,0,0));
	}

	RI->SetOrigin2D( 0, 0 );
	RI->SetZoom2D( 1.f );
}

void FLinkedObjViewportClient::InputKey(FChildViewport* Viewport, FName Key, EInputEvent Event,FLOAT)
{
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	UBOOL bAltDown = Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt);

	INT HitX = Viewport->GetMouseX();
	INT HitY = Viewport->GetMouseY();

	if( Key == KEY_LeftMouseButton )
	{
		switch( Event )
		{
		case IE_Pressed:
		case IE_DoubleClick:
			{
				HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);

				if(HitResult)
				{
					if(HitResult->IsA(TEXT("HLinkedObjProxy")))
					{
						UObject* Obj = ((HLinkedObjProxy*)HitResult)->Obj;

						if(!bCtrlDown)
						{
							ObjEditor->EmptySelection();
							ObjEditor->AddToSelection(Obj);

							ObjEditor->UpdatePropertyWindow();

							if(Event == IE_DoubleClick)
							{
								ObjEditor->DoubleClickedObject(Obj);
								bMouseDown = false;
								return;
							}
						}
					}
					else if(HitResult->IsA(TEXT("HLinkedObjConnectorProxy")))
					{
						HLinkedObjConnectorProxy* ConnProxy = (HLinkedObjConnectorProxy*)HitResult;
						ObjEditor->SetSelectedConnector( ConnProxy->ConnObj, ConnProxy->ConnType, ConnProxy->ConnIndex );

						ObjEditor->EmptySelection();
						ObjEditor->UpdatePropertyWindow();

						bMakingLine = true;
						NewX = HitX;
						NewY = HitY;
					}
					else if(HitResult->IsA(TEXT("HLinkedObjProxySpecial")))
					{
						HLinkedObjProxySpecial* SpecialProxy = (HLinkedObjProxySpecial*)HitResult;
						
						ObjEditor->EmptySelection();
						ObjEditor->AddToSelection(SpecialProxy->Obj);
						ObjEditor->UpdatePropertyWindow();

						// For supporting undo 
						ObjEditor->BeginTransactionOnSelected();
						bTransactionBegun = true;

						bSpecialDrag = true;
						SpecialIndex = SpecialProxy->SpecialIndex;
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
				}

				OldMouseX = HitX;
				OldMouseY = HitY;
				DistanceDragged = 0;
				bMouseDown = true;
				Viewport->LockMouseToWindow(true);
				Viewport->Invalidate();
			}
			break;

		case IE_Released:
			{
				if(bMakingLine)
				{
					Viewport->Invalidate();
					HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);

					if(HitResult)
					{
						if( HitResult->IsA(TEXT("HLinkedObjConnectorProxy")) )
						{
							HLinkedObjConnectorProxy* EndConnProxy = (HLinkedObjConnectorProxy*)HitResult;

							ObjEditor->MakeConnectionToConnector( EndConnProxy->ConnObj, EndConnProxy->ConnType, EndConnProxy->ConnIndex );
						}
						else if( HitResult->IsA(TEXT("HLinkedObjProxy")) )
						{
							UObject* Obj = ((HLinkedObjProxy*)HitResult)->Obj;

							ObjEditor->MakeConnectionToObject( Obj );
						}
					}
				}
				else if( bBoxSelecting )
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

					TArray<UObject*> NewSelection;

					// Find any keypoint hit proxies in the region - add the keypoint to selection.
					for(INT Y=0; Y < TestSizeY; Y++)
					{
						for(INT X=0; X < TestSizeX; X++)
						{
							HHitProxy* HitProxy = ProxyMap(Y * TestSizeX + X);

							if(HitProxy && HitProxy->IsA(TEXT("HLinkedObjProxy")))
							{
								UObject* SelObject = ((HLinkedObjProxy*)HitProxy)->Obj;

								// Don't want to call AddToSelection here because that might invalidate the display and we'll crash.
								NewSelection.AddUniqueItem( SelObject );
							}
						}
					}

					// If shift is down, don't empty, just add to selection.
					if(!bShiftDown)
					{
						ObjEditor->EmptySelection();
					}
					
					// Iterate over array adding each to selection.
					for(INT i=0; i<NewSelection.Num(); i++)
					{
						ObjEditor->AddToSelection( NewSelection(i) );
					}

					ObjEditor->UpdatePropertyWindow();
				}
				else
				{
					HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);

					// If mouse didn't really move since last time, and we released over empty space, deselect everything.
					if( !HitResult && DistanceDragged < 4 )
					{
						NewX = HitX;
						NewY = HitY;
						UBOOL bDoDeselect = ObjEditor->ClickOnBackground();
						if(bDoDeselect)
						{
							ObjEditor->EmptySelection();
							ObjEditor->UpdatePropertyWindow();
						}
					}
					else if(bCtrlDown)
					{
						if(DistanceDragged < 4)
						{
							if( HitResult && HitResult->IsA(TEXT("HLinkedObjProxy")) )
							{
								UObject* Obj = ((HLinkedObjProxy*)HitResult)->Obj;
								UBOOL bAlreadySelected = ObjEditor->IsInSelection(Obj);

								if(!bAlreadySelected)
								{
									ObjEditor->AddToSelection(Obj);
								}
								else
								{
									ObjEditor->RemoveFromSelection(Obj);
								}

								ObjEditor->UpdatePropertyWindow();
							}
						}
						else
						{
							ObjEditor->PositionSelectedObjects();
						}
					}
				}

				if(bTransactionBegun)
				{
					ObjEditor->EndTransactionOnSelected();
					bTransactionBegun = false;
				}

				bMouseDown = false;
				bMakingLine = false;
				bSpecialDrag = false;
				bBoxSelecting = false;
				Viewport->LockMouseToWindow(false);
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
			}
			break;

		case IE_Released:
			{
				if(bMakingLine)
					break;

				INT HitX = Viewport->GetMouseX();
				INT HitY = Viewport->GetMouseY();
				HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);

				wxMenu* menu = NULL;
				if(!HitResult)
				{
					ObjEditor->OpenNewObjectMenu();
				}
				else
				{
					if( HitResult->IsA(TEXT("HLinkedObjConnectorProxy")) )
					{
						HLinkedObjConnectorProxy* ConnProxy = (HLinkedObjConnectorProxy*)HitResult;

						// First select the connector and deselect any objects.
						ObjEditor->SetSelectedConnector( ConnProxy->ConnObj, ConnProxy->ConnType, ConnProxy->ConnIndex );
						ObjEditor->EmptySelection();
						ObjEditor->UpdatePropertyWindow();
						Viewport->Invalidate();

						// Then open connector options menu.
						ObjEditor->OpenConnectorOptionsMenu();
					}
					else if( HitResult->IsA(TEXT("HLinkedObjProxy")) )
					{
						// When right clicking on an unselected object, select it only before opening menu.
						UObject* Obj = ((HLinkedObjProxy*)HitResult)->Obj;

						if( !ObjEditor->IsInSelection(Obj) )
						{
							ObjEditor->EmptySelection();
							ObjEditor->AddToSelection(Obj);
							ObjEditor->UpdatePropertyWindow();
							Viewport->Invalidate();
						}
					
						ObjEditor->OpenObjectOptionsMenu();
					}
				}
			}
			break;
		}
	}
	else if( Key == KEY_MouseScrollDown && Event == IE_Pressed )
	{
		if(Zoom2D < MaxZoom2D)
		{
			FLOAT ViewCenterX = ((((FLOAT)Viewport->GetSizeX()) * 0.5f) - Origin2D.X)/Zoom2D;
			FLOAT ViewCenterY = ((((FLOAT)Viewport->GetSizeY()) * 0.5f) - Origin2D.Y)/Zoom2D;

			Zoom2D = Clamp<FLOAT>(Zoom2D+ZoomIncrement,MinZoom2D,MaxZoom2D);

			FLOAT DrawOriginX = ViewCenterX - ((Viewport->GetSizeX()*0.5f)/Zoom2D);
			FLOAT DrawOriginY = ViewCenterY - ((Viewport->GetSizeY()*0.5f)/Zoom2D);

			Origin2D.X = -(DrawOriginX * Zoom2D);
			Origin2D.Y = -(DrawOriginY * Zoom2D);

			ObjEditor->ViewPosChanged();
			Viewport->Invalidate();
		}
	}
	else if( Key == KEY_MouseScrollUp && Event == IE_Pressed )
	{
		if(Zoom2D > MinZoom2D)
		{
			FLOAT ViewCenterX = ((((FLOAT)Viewport->GetSizeX()) * 0.5f) - Origin2D.X)/Zoom2D;
			FLOAT ViewCenterY = ((((FLOAT)Viewport->GetSizeY()) * 0.5f) - Origin2D.Y)/Zoom2D;

			Zoom2D = Clamp<FLOAT>(Zoom2D-ZoomIncrement,MinZoom2D,MaxZoom2D);

			FLOAT DrawOriginX = ViewCenterX - ((Viewport->GetSizeX()*0.5f)/Zoom2D);
			FLOAT DrawOriginY = ViewCenterY - ((Viewport->GetSizeY()*0.5f)/Zoom2D);

			Origin2D.X = -(DrawOriginX * Zoom2D);
			Origin2D.Y = -(DrawOriginY * Zoom2D);

			ObjEditor->ViewPosChanged();
			Viewport->Invalidate();
		}
	}

	ObjEditor->EdHandleKeyInput(Viewport, Key, Event);
}

void FLinkedObjViewportClient::MouseMove(FChildViewport* Viewport, INT X, INT Y)
{
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);

	INT DeltaX = X - OldMouseX;
	INT DeltaY = Y - OldMouseY;

	if(bMouseDown)
	{
		DistanceDragged += ( Abs<INT>(DeltaX) + Abs<INT>(DeltaY) );
	}

	OldMouseX = X;
	OldMouseY = Y;

	if(bMouseDown)
	{
		if(bMakingLine)
		{
			NewX = X;
			NewY = Y;
		}
		else if(bBoxSelecting)
		{
			BoxEndX = X;
			BoxEndY = Y;
		}
		else if(bSpecialDrag)
		{
			ObjEditor->SpecialDrag( DeltaX * (1.f/Zoom2D), DeltaY * (1.f/Zoom2D), SpecialIndex );
		}
		else if( bCtrlDown && ObjEditor->HaveObjectsSelected() )
		{
			ObjEditor->MoveSelectedObjects( DeltaX * (1.f/Zoom2D), DeltaY * (1.f/Zoom2D) );

			// If haven't started a transaction, and moving some stuff, and have moved mouse far enough, start transaction now.
			if(!bTransactionBegun && DistanceDragged > 4)
			{
				ObjEditor->BeginTransactionOnSelected();
				bTransactionBegun = true;
			}
		}
		else
		if (bAllowScroll)
		{
			Origin2D.X += DeltaX;
			Origin2D.Y += DeltaY;
			ObjEditor->ViewPosChanged();
		}

		Viewport->InvalidateDisplay();
	}

	// Do mouse-over stuff (if mouse button is not held).
	UObject* NewMouseOverObject = NULL;
	INT NewMouseOverConnType = -1;
	INT NewMouseOverConnIndex = INDEX_NONE;
	HHitProxy*	HitResult = NULL;

	if(!bMouseDown || bMakingLine)
	{
		HitResult = Viewport->GetHitProxy(X,Y);
	}

	if( HitResult )
	{
		if( HitResult->IsA(TEXT("HLinkedObjProxy")) )
		{
			NewMouseOverObject = ((HLinkedObjProxy*)HitResult)->Obj;
		}
		else if( HitResult->IsA(TEXT("HLinkedObjConnectorProxy")) )
		{
			NewMouseOverObject = ((HLinkedObjConnectorProxy*)HitResult)->ConnObj;
			NewMouseOverConnType = ((HLinkedObjConnectorProxy*)HitResult)->ConnType;
			NewMouseOverConnIndex = ((HLinkedObjConnectorProxy*)HitResult)->ConnIndex;

			if( !ObjEditor->ShouldHighlightConnector(NewMouseOverObject, NewMouseOverConnType, NewMouseOverConnIndex) )
			{
				NewMouseOverConnType = -1;
				NewMouseOverConnIndex = INDEX_NONE;
			}
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

		ObjEditor->OnMouseOver(MouseOverObject);
	}
}

EMouseCursor FLinkedObjViewportClient::GetCursor(FChildViewport* Viewport,INT X,INT Y)
{
	return MC_Arrow;
}

/** 
 *	See if cursor is in 'scroll' region around the edge, and if so, scroll the view automatically. 
 *	Returns the distance that the view was moved.
 */
FIntPoint FLinkedObjViewportClient::DoScrollBorder(FLOAT DeltaTime)
{
	if (bAllowScroll)
	{
		INT PosX = Viewport->GetMouseX();
		INT PosY = Viewport->GetMouseY();
		INT SizeX = Viewport->GetSizeX();
		INT SizeY = Viewport->GetSizeY();

		if(PosX < ScrollBorderSize)
		{
			ScrollAccum.X += (1.f - ((FLOAT)PosX/(FLOAT)ScrollBorderSize)) * ScrollBorderSpeed * DeltaTime;
		}
		else if(PosX > SizeX - ScrollBorderSize)
		{
			ScrollAccum.X -= ((FLOAT)(PosX - (SizeX - ScrollBorderSize))/(FLOAT)ScrollBorderSize) * ScrollBorderSpeed * DeltaTime;
		}
		else
		{
			ScrollAccum.X = 0.f;
		}

		FLOAT ScrollY = 0.f;
		if(PosY < ScrollBorderSize)
		{
			ScrollAccum.Y += (1.f - ((FLOAT)PosY/(FLOAT)ScrollBorderSize)) * ScrollBorderSpeed * DeltaTime;
		}
		else if(PosY > SizeY - ScrollBorderSize)
		{
			ScrollAccum.Y -= ((FLOAT)(PosY - (SizeY - ScrollBorderSize))/(FLOAT)ScrollBorderSize) * ScrollBorderSpeed * DeltaTime;
		}
		else
		{
			ScrollAccum.Y = 0.f;
		}

		// Apply integer part of ScrollAccum to origin, and save the rest.
		INT MoveX = appFloor(ScrollAccum.X);
		Origin2D.X += MoveX;
		ScrollAccum.X -= MoveX;

		INT MoveY = appFloor(ScrollAccum.Y);
		Origin2D.Y += MoveY;
		ScrollAccum.Y -= MoveY;

		// If view has changed, notify the app
		if( Abs<INT>(MoveX) > 0 || Abs<INT>(MoveY) > 0 )
		{
			ObjEditor->ViewPosChanged();
		}
		return FIntPoint(MoveX, MoveY);
	}
	else
	{
		return FIntPoint(0, 0);
	}
}

void FLinkedObjViewportClient::Tick(FLOAT DeltaSeconds)
{
	FEditorLevelViewportClient::Tick(DeltaSeconds);

	// Auto-scroll display if moving/drawing etc. and near edge.
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	if(	bMouseDown )		
	{
		if(bMakingLine || bBoxSelecting)
		{
			DoScrollBorder(DeltaSeconds);
		}
		else if(bSpecialDrag)
		{
			FIntPoint Delta = DoScrollBorder(DeltaSeconds);
			ObjEditor->SpecialDrag( -Delta.X * (1.f/Zoom2D), -Delta.Y * (1.f/Zoom2D), SpecialIndex );
		}
		else if(bCtrlDown && ObjEditor->HaveObjectsSelected())
		{
			FIntPoint Delta = DoScrollBorder(DeltaSeconds);

			// In the case of dragging boxes around, we move them as well when dragging at the edge of the screen.
			ObjEditor->MoveSelectedObjects( -Delta.X * (1.f/Zoom2D), -Delta.Y * (1.f/Zoom2D) );

			DistanceDragged += ( Abs<INT>(Delta.X) + Abs<INT>(Delta.Y) );

			if(!bTransactionBegun && DistanceDragged > 4)
			{
				ObjEditor->BeginTransactionOnSelected();
				bTransactionBegun = true;
			}
		}
	}

	// Always invalidate the display - so mouse overs and stuff get a chane to redraw.
	Viewport->InvalidateDisplay();
}

/*-----------------------------------------------------------------------------
WxLinkedObjVCHolder
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxLinkedObjVCHolder, wxWindow )
EVT_SIZE( WxLinkedObjVCHolder::OnSize )
END_EVENT_TABLE()

WxLinkedObjVCHolder::WxLinkedObjVCHolder( wxWindow* InParent, wxWindowID InID, WxLinkedObjEd* InObjEditor )
: wxWindow( InParent, InID )
{
	LinkedObjVC = new FLinkedObjViewportClient( InObjEditor );
	LinkedObjVC->Viewport = GEngine->Client->CreateWindowChildViewport(LinkedObjVC, (HWND)GetHandle());
	LinkedObjVC->Viewport->CaptureJoystickInput(false);
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
WxLinkedObjEd
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxLinkedObjEd, wxFrame )
EVT_SIZE( WxLinkedObjEd::OnSize )
END_EVENT_TABLE()


WxLinkedObjEd::WxLinkedObjEd( wxWindow* InParent, wxWindowID InID, const TCHAR* InWinName, UBOOL bTreeControl )
: wxFrame( InParent, InID, TEXT(""), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR )
{
	bDrawCurves = false;

	WinNameString = FString(InWinName);

	SetTitle( *WinNameString );

	// Load the desired window position from .ini file
	FString Wk;
	GConfig->GetString( TEXT("WindowPosManager"), *WinNameString, Wk, GEditorIni );

	TArray<FString> Args;
	wxRect rc(256,256,1024,768);
	INT SashPos = 600;
	INT TreeSashPos = 500;
	INT NumArgs = Wk.ParseIntoArray( TEXT(","), &Args );
	if( NumArgs == 5 || NumArgs == 6 )
	{
		INT X = appAtoi( *Args(0) );
		INT Y = appAtoi( *Args(1) );
		INT W = appAtoi( *Args(2) );
		INT H = appAtoi( *Args(3) );
		INT S = appAtoi( *Args(4) );

		if(NumArgs == 6)
		{
			TreeSashPos = appAtoi( *Args(5) );
		}

		INT vleft = ::GetSystemMetrics( SM_XVIRTUALSCREEN );
		INT vtop = ::GetSystemMetrics( SM_YVIRTUALSCREEN );
		INT vright = ::GetSystemMetrics( SM_CXVIRTUALSCREEN );
		INT vbottom = ::GetSystemMetrics( SM_CYVIRTUALSCREEN );

		if( X < vleft || X+W >= vright )		X = vleft;
		if( Y < vtop || Y+H >= vbottom )		Y = vtop;

		rc.SetX( X );
		rc.SetY( Y );
		rc.SetWidth( W );
		rc.SetHeight( H );
		SashPos = S;
	}

	wxBoxSizer* item2 = new wxBoxSizer(wxVERTICAL);
	SetSizer(item2);
	SetAutoLayout(TRUE);

	SplitterWnd = new wxSplitterWindow(this, ID_SPLITTERWINDOW, wxDefaultPosition, wxSize(100, 100), wxSP_3DBORDER|wxSP_FULLSASH );

	WxLinkedObjVCHolder* UpperWindow = new WxLinkedObjVCHolder( SplitterWnd, -1, this );
	LinkedObjVC = UpperWindow->LinkedObjVC;

	if(bTreeControl)
	{
		TreeSplitterWnd = new wxSplitterWindow(SplitterWnd, IDM_LINKEDOBJED_TREE_SPLITTER, wxDefaultPosition, wxSize(100, 100), wxSP_3DBORDER|wxSP_FULLSASH );

		SplitterWnd->SplitHorizontally( UpperWindow, TreeSplitterWnd, SashPos );

		PropertyWindow = new WxPropertyWindow( TreeSplitterWnd, this );

		TreeImages = new wxImageList( 16, 15 );
		TreeControl = new wxTreeCtrl( TreeSplitterWnd, IDM_LINKEDOBJED_TREE, wxDefaultPosition, wxSize(100,100), wxTR_HAS_BUTTONS, wxDefaultValidator, TEXT("TreeControl") );
		TreeControl->AssignImageList(TreeImages);

		TreeSplitterWnd->SplitVertically( PropertyWindow, TreeControl, TreeSashPos );

		item2->Add(SplitterWnd, 1, wxGROW|wxALL, 0);
	}
	else
	{
		PropertyWindow = new WxPropertyWindow( SplitterWnd, this );

		SplitterWnd->SplitHorizontally( UpperWindow, PropertyWindow, SashPos );
		item2->Add(SplitterWnd, 1, wxGROW|wxALL, 0);

		TreeSplitterWnd = NULL;
		TreeControl = NULL;
	}

	GetSizer()->Fit(this);
	GetSizer()->SetSizeHints(this);
	Centre();

	SetSize(rc) ;
}

WxLinkedObjEd::~WxLinkedObjEd()
{
	wxRect rc = GetRect();

	FString Wk;
	if(TreeSplitterWnd)
	{
		Wk = *FString::Printf( TEXT("%d,%d,%d,%d,%d,%d"), rc.GetX(), rc.GetY(), rc.GetWidth(), rc.GetHeight(), SplitterWnd->GetSashPosition(), TreeSplitterWnd->GetSashPosition() );
	}
	else
	{
		Wk = *FString::Printf( TEXT("%d,%d,%d,%d,%d"), rc.GetX(), rc.GetY(), rc.GetWidth(), rc.GetHeight(), SplitterWnd->GetSashPosition() );
	}
	GConfig->SetString( TEXT("WindowPosManager"), *WinNameString, *Wk, GEditorIni );
}

void WxLinkedObjEd::OnSize( wxSizeEvent& In )
{
	//if( SplitterWnd )
	//{
	//	wxRect rc = GetClientRect();

	//	SplitterWnd->SetSize( rc );
	//}

	In.Skip();
}

void WxLinkedObjEd::RefreshViewport()
{
	LinkedObjVC->Viewport->Invalidate();
}

void WxLinkedObjEd::DrawLinkedObjects(FRenderInterface* RI)
{
	// draw the background texture if specified
	if (BackgroundTexture != NULL)
	{
		RI->Clear( FColor(161,161,161) );

		RI->SetOrigin2D( 0, 0 );
		RI->SetZoom2D( 1.f );

		INT ViewWidth = LinkedObjVC->Viewport->GetSizeX();
		INT ViewHeight = LinkedObjVC->Viewport->GetSizeY();

		// draw the texture to the side, stretched vertically
		RI->DrawTile( ViewWidth - BackgroundTexture->SizeX, 0,
					  BackgroundTexture->SizeX, ViewHeight,
					  0.f, 0.f,
					  1.f, 1.f,
					  FLinearColor::White,
					  BackgroundTexture );

		// stretch the left part of the texture to fill the remaining gap
		if ( ViewWidth > static_cast<INT>( BackgroundTexture->SizeX ) )
		{
			RI->DrawTile( 0, 0,
						  ViewWidth - BackgroundTexture->SizeX, ViewHeight,
						  0.f, 0.f,
						  0.1f, 0.1f,
						  FLinearColor::White,
						  BackgroundTexture );
		}
					

		RI->SetOrigin2D( LinkedObjVC->Origin2D );
		RI->SetZoom2D( LinkedObjVC->Zoom2D );
	}
}

///////////////////////////////////////////////////////////////////////////////////////
// Properties window NotifyHook stuff

void WxLinkedObjEd::NotifyDestroy( void* Src )
{

}

void WxLinkedObjEd::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{
	GEditor->Trans->Begin( TEXT("Edit LinkedObj") );

	TArray<UObject*> Objects;
	PropertyWindow->GetObjectArray(Objects);
	
	for(INT i=0; i<Objects.Num(); i++)
	{
		Objects(i)->Modify();
	}
}

void WxLinkedObjEd::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	GEditor->Trans->End();

	RefreshViewport();
}

void WxLinkedObjEd::NotifyExec( void* Src, const TCHAR* Cmd )
{
	GUnrealEd->NotifyExec(Src, Cmd);
}
/*-----------------------------------------------------------------------------
The End.
-----------------------------------------------------------------------------*/
