
#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	WxSash.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxSash, wxWindow )
	EVT_PAINT( WxSash::OnPaint )
	EVT_LEFT_DOWN( WxSash::OnLeftButtonDown )
	EVT_LEFT_UP( WxSash::OnLeftButtonUp )
	EVT_MOTION( WxSash::OnMouseMove )
END_EVENT_TABLE()

WxSash::WxSash( wxWindow* InParent, wxWindowID InID, ESashOrientation InOrientation )
	: wxWindow( InParent, InID )
{
	Orientation = InOrientation;
}

WxSash::~WxSash()
{
}

void WxSash::SetOrientation( ESashOrientation InOrientation )
{
	Orientation = InOrientation;
}

void WxSash::OnPaint( wxPaintEvent& In )
{
	WxPaintDC dc(this);

	if( dc.Ok() )
	{
		INT w, h;
		dc.GetSize( &w, &h );
		w--;
		h--;

		// Give the window a 3D look

		dc.SetBrush( wxBrush( wxColour(236,233,216), wxSOLID ) );
		dc.SetPen( wxPen( wxColour(236,233,216), 1, wxSOLID ) );
		dc.DrawRectangle( 0, 0, w, DOCKING_DRAG_BAR_H );

		dc.SetPen( wxPen( wxColour(172,168,153), 1, wxSOLID ) );
		dc.DrawLine( w,0,	w,h );
		dc.DrawLine( w,h,	0,h );

		dc.SetPen( wxPen( wxColour(255,255,255), 1, wxSOLID ) );
		dc.DrawLine( 0,0,	w,0 );
		dc.DrawLine( 0,0,	0,h );
	}

	In.Skip();

}

void WxSash::OnLeftButtonDown( wxMouseEvent& In )
{
	MouseStart = wxGetMousePosition();
	CaptureMouse();

}

void WxSash::OnLeftButtonUp( wxMouseEvent& In )
{
	if( wxWindow::GetCapture() == this )
	{
		ReleaseCapture();
	
		wxSize Diff( wxGetMousePosition().x - MouseStart.x, wxGetMousePosition().y - MouseStart.y );

		if( Orientation == SO_Horizontal )	Diff.x = 0;
		else								Diff.y = 0;

		WxDockingTabbedContainer* Parent = (WxDockingTabbedContainer*)GetParent();
		check(Parent);
		wxRect rc = Parent->GetRect();
		Parent->SetSize( rc.GetWidth()+Diff.x, rc.GetHeight()+Diff.y );

		if( Orientation == SO_Horizontal )	Parent->DockingDepth += ((Parent->Edge == DE_Top) ? Diff.y : -Diff.y);
		else								Parent->DockingDepth += ((Parent->Edge == DE_Left) ? Diff.x : -Diff.x);

        wxSizeEvent Event;
		GApp->EditorFrame->OnSize( Event );
		GApp->EditorFrame->DockingRoot->LayoutDockedWindows();
	}

}

void WxSash::OnMouseMove( wxMouseEvent& In )
{
	check( Orientation != SO_None );

	if( Orientation == SO_Horizontal )
		SetCursor( wxCURSOR_SIZENS );
	else
		SetCursor( wxCURSOR_SIZEWE );

}

/*-----------------------------------------------------------------------------
	WxDockableWindow.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxDockableWindow, wxPanel )
	EVT_SIZE( WxDockableWindow::OnSize )
END_EVENT_TABLE()

WxDockableWindow::WxDockableWindow( wxWindow* InParent, wxWindowID InID, EDockableWindowType InType, FString InDockingName )
	: wxPanel( InParent, InID )
{
	Type = InType;
	DockingName = InDockingName;
	MenuBar = NULL;
	ToolBar = NULL;
}

WxDockableWindow::~WxDockableWindow()
{
}

void WxDockableWindow::Update()
{
}

void WxDockableWindow::Activated()
{
	((wxFrame*)GetParent()->GetParent())->SetMenuBar( NULL );
	((wxFrame*)GetParent()->GetParent())->SetToolBar( NULL );
}

void WxDockableWindow::OnActivate( wxActivateEvent& In )
{
	if( In.GetActive() )
		Activated();
}

void WxDockableWindow::OnSize( wxSizeEvent& In )
{
	if( Panel )
	{
		wxRect rc = GetClientRect();
		Panel->SetSize( rc );
	}
}

/*-----------------------------------------------------------------------------
	WxDockingNotebook.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxDockingNotebook, wxNotebook )
	EVT_LEFT_DOWN( WxDockingNotebook::OnLeftButtonDown )
	EVT_LEFT_UP( WxDockingNotebook::OnLeftButtonUp )
	EVT_MOTION( WxDockingNotebook::OnMouseMove )
END_EVENT_TABLE()

WxDockingNotebook::WxDockingNotebook( wxWindow* InParent, wxWindowID InID )
	: wxNotebook( InParent, InID, wxDefaultPosition, wxDefaultSize, wxNB_TOP | wxNB_MULTILINE )
{
	bStartingDrag = 0;
}

WxDockingNotebook::~WxDockingNotebook()
{
}

void WxDockingNotebook::OnLeftButtonDown( wxMouseEvent& In )
{
	bStartingDrag = 1;
	DragChecker.Set( In.GetPosition(), STD_DRAG_LIMIT );
	CaptureMouse();

	In.Skip();

}

void WxDockingNotebook::OnLeftButtonUp( wxMouseEvent& In )
{
	if( GetCapture() == this )
	{
		bStartingDrag = 0;
		ReleaseMouse();
	}
	In.Skip();

}

void WxDockingNotebook::OnMouseMove( wxMouseEvent& In )
{
	if( bStartingDrag && DragChecker.ShouldDrag( In.GetPosition() ) )
	{
		bStartingDrag = 0;
		ReleaseMouse();

		WxDockingTabbedContainer* dtcParent = (WxDockingTabbedContainer*)GetParent();
		WxDockableWindow* dw = (WxDockableWindow*)GetPage( GetSelection() );
		WxDockingTabbedContainer* dtc = GApp->EditorFrame->DockingRoot->AddTabbedContainer( GApp->EditorFrame->DockingRoot, 0, 0, dtcParent->FloatingRect.GetWidth(), dtcParent->FloatingRect.GetHeight() );
		dtc->Show();
		dtc->AddDockableWindow( dw->Type );
		GApp->EditorFrame->DockingRoot->Dock( dtc, DE_Floating );
		dtcParent->RemoveDockableWindow( dw->Type );
		
		// Center the window on the mouse cursor

		wxPoint pt = wxGetMousePosition();
		wxRect rc = dtc->GetRect();
		dtc->Move( pt.x - rc.GetWidth()/2, pt.y - 4 );
		dtc->Raise();

		// Tell the new tabbed container that it is being dragged.

		dtc->OnLeftButtonDown( wxMouseEvent() );
		
		GApp->EditorFrame->DockingRoot->CleanUpDeadContainers();

		GApp->EditorFrame->OnSize( wxSizeEvent() );
		GApp->EditorFrame->DockingRoot->LayoutDockedWindows();
	}

	In.Skip();

}

/*-----------------------------------------------------------------------------
	WxDockingTabbedContainer.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxDockingTabbedContainer, wxFrame )
	EVT_SIZE( WxDockingTabbedContainer::OnSize )
	EVT_LEFT_DOWN( WxDockingTabbedContainer::OnLeftButtonDown )
	EVT_LEFT_UP( WxDockingTabbedContainer::OnLeftButtonUp )
	EVT_MOTION( WxDockingTabbedContainer::OnMouseMove )
	EVT_RIGHT_DOWN( WxDockingTabbedContainer::OnRightMouseButtonDown )
	EVT_MOVE( WxDockingTabbedContainer::OnWindowMove )
	EVT_MENU_RANGE( wxID_HIGHEST, wxID_HIGHEST+65535, WxDockingTabbedContainer::OnCommand )
	EVT_NOTEBOOK_PAGE_CHANGED( ID_NOTEBOOK, WxDockingTabbedContainer::OnPageChanged )
	EVT_CLOSE( WxDockingTabbedContainer::OnClose )
END_EVENT_TABLE()

// A hack function to make sure the message gets to the proper window

void WxDockingTabbedContainer::OnCommand( wxCommandEvent& In )
{
	WxDockableWindow* dw = (WxDockableWindow*)NotebookCtrl->GetPage( NotebookCtrl->GetSelection() );
	::wxPostEvent( dw, In );
}

WxDockingTabbedContainer::WxDockingTabbedContainer( wxWindow* InParent, wxWindowID InID )
	: wxFrame( InParent, InID, TEXT(""), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU | wxMAXIMIZE_BOX | wxFRAME_TOOL_WINDOW | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR )
{
	Edge = DE_None;
	DockingDepth = DEF_DOCKING_DEPTH;
	FloatingRect = wxRect( 50, 50, 200, 200 );
	bIsDragging = bStartingDrag = 0;
	NotebookCtrl = new WxDockingNotebook( (wxWindow*)this, ID_NOTEBOOK );
	SizingSash = new WxSash( (wxWindow*)this, -1, SO_None );
}

WxDockingTabbedContainer::~WxDockingTabbedContainer()
{
}

// Adds a dockable window to this container.

void WxDockingTabbedContainer::AddDockableWindow( EDockableWindowType InType )
{
	WxDockableWindow* dw = GApp->EditorFrame->DockingRoot->CreateDockableWindow( (wxWindow*)NotebookCtrl, InType );

	if(dw)
	{
		DockableWindows.AddItem( dw );
		NotebookCtrl->AddPage( dw, *(dw->DockingName) );
	}

}

void WxDockingTabbedContainer::RemoveDockableWindow( EDockableWindowType InType )
{
	// Find the dockable window and remove the page

	for( INT x = 0 ; x < DockableWindows.Num() ; ++x )
	{
		WxDockableWindow* dw = DockableWindows(x);
		if( dw->Type == InType )
		{
			DockableWindows.Remove(x);
			NotebookCtrl->DeletePage( x );
			break;
		}
	}

}

void WxDockingTabbedContainer::OnSize( wxSizeEvent& In )
{
	wxRect rc = GetClientRect();

	NotebookCtrl->SetSize( rc );

	if( IsDocked() )
	{
		wxRect rcSash;
		ESashOrientation Orientation = SO_None;
		INT w = rc.GetWidth(),
			h = rc.GetHeight();

		// Place the sizing sash in a good location and set its orientation

		switch( Edge )
		{
			case DE_Left:
				Orientation = SO_Vertical;
				rcSash = wxRect( w-STD_SASH_SZ, 0, STD_SASH_SZ, h );
				break;

			case DE_Right:
				Orientation = SO_Vertical;
				rcSash = wxRect( 0, 0, STD_SASH_SZ, h );
				break;

			case DE_Top:
				Orientation = SO_Horizontal;
				rcSash = wxRect( 0, h-STD_SASH_SZ, w, STD_SASH_SZ );
				break;

			case DE_Bottom:
				Orientation = SO_Horizontal;
				rcSash = wxRect( 0, 0, w, STD_SASH_SZ );
				break;
		}

		SizingSash->SetOrientation( Orientation );
		SizingSash->SetSize( rcSash );
		SizingSash->Show( 1 );
	}
	else
		SizingSash->Show( 0 );

}

void WxDockingTabbedContainer::OnLeftButtonDown( wxMouseEvent& In )
{
	bIsDragging = 1;
	GApp->EditorFrame->DockingRoot->DraggedContainer = this;
	GApp->EditorFrame->DockingRoot->DestContainer = NULL;
	DragChecker.Set( In.GetPosition(), STD_DRAG_LIMIT );

	MouseLast = wxGetMousePosition();
	CaptureMouse();

}

void WxDockingTabbedContainer::OnLeftButtonUp( wxMouseEvent& In )
{
	if( bIsDragging )
	{
		bIsDragging = 0;
		ReleaseMouse();

		GApp->EditorFrame->DockingRoot->Dock( this, GApp->EditorFrame->DockingRoot->GetEdgeFromMouseOver( GApp->EditorFrame->DockingRoot->MouseOver ) );

		GApp->EditorFrame->DockingRoot->OnSize( wxSizeEvent() );
		GApp->EditorFrame->DockingRoot->LayoutDockedWindows();

		GApp->EditorFrame->DockingRoot->MouseOver = DMO_None;
		GApp->EditorFrame->DockingRoot->UpdateMouseCursor();
		GApp->EditorFrame->DockingRoot->DraggedContainer = NULL;
	}

}

void WxDockingTabbedContainer::OnMouseMove( wxMouseEvent& In )
{
	if( bIsDragging && DragChecker.ShouldDrag( In.GetPosition() ) )
	{
		// If the window is currently docked, we need to float it so the user can drag it

		if( IsDocked() )
		{
			bStartingDrag = 0;

			GApp->EditorFrame->DockingRoot->Float( this );
			
			// Center the window horizontally on the mouse cursor

			wxPoint pt = wxGetMousePosition();
			wxRect rc = GetRect();
			Move( pt.x - rc.GetWidth()/2, rc.y );
		}

		wxPoint MousePos = wxGetMousePosition(),
			Delta = MousePos - MouseLast,
			WindowPos = GetPosition() + Delta;

		Move( WindowPos.x, WindowPos.y );

		MouseLast = MousePos;

		GApp->EditorFrame->DockingRoot->UpdateMouseOver();
		GApp->EditorFrame->DockingRoot->UpdateMouseCursor();
	}

}

void WxDockingTabbedContainer::OnWindowMove( wxMoveEvent& In )
{
	GApp->EditorFrame->DockingRoot->DraggedContainer = this;
	GApp->EditorFrame->DockingRoot->DestContainer = NULL;

	GApp->EditorFrame->DockingRoot->UpdateMouseOver();
	GApp->EditorFrame->DockingRoot->UpdateMouseCursor();
}

void WxDockingTabbedContainer::OnRightMouseButtonDown( wxMouseEvent& In )
{
}

void WxDockingTabbedContainer::OnPageChanged( wxNotebookEvent& In )
{
	WxDockableWindow* dw = (WxDockableWindow*)NotebookCtrl->GetPage( NotebookCtrl->GetSelection() );
	dw->Activated();
}

void WxDockingTabbedContainer::OnClose( wxCloseEvent& In )
{
	// Don't actually close these windows.  Just hide them so they can be recalled later.

	Hide();
	GApp->EditorFrame->OnSize( wxSizeEvent() );
	GApp->EditorFrame->DockingRoot->OnSize( wxSizeEvent() );

	In.Veto();
}

/*-----------------------------------------------------------------------------
	WxDockingRoot.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxDockingRoot, wxWindow )
	EVT_SIZE( WxDockingRoot::OnSize )
END_EVENT_TABLE()

WxDockingRoot::WxDockingRoot( wxWindow* InParent, wxWindowID InID )
	: wxWindow( InParent, InID )
{
	MouseOver = DMO_None;
}

WxDockingRoot::~WxDockingRoot()
{
}

void WxDockingRoot::UpdateMouseOver()
{
	// Set up

	MouseOver = DMO_None;
	DestContainer = NULL;

	wxRect rc = GetClientRect();
	wxPoint MousePos = ScreenToClient( wxGetMousePosition() );

	// Check docking frame edges

	if( wxRect( rc.x, rc.y, rc.GetWidth(), DOCK_GUTTER_SZ ).Inside( MousePos ) )
		MouseOver = DMO_FrameTop;
	else if( wxRect( rc.x, rc.y+rc.GetHeight()-DOCK_GUTTER_SZ, rc.GetWidth(), DOCK_GUTTER_SZ ).Inside( MousePos ) )
		MouseOver = DMO_FrameBottom;
	else if( wxRect( rc.x, rc.y, DOCK_GUTTER_SZ, rc.GetHeight() ).Inside( MousePos ) )
		MouseOver = DMO_FrameLeft;
	else if( wxRect( rc.x+rc.GetWidth()-DOCK_GUTTER_SZ, rc.y, DOCK_GUTTER_SZ, rc.GetHeight() ).Inside( MousePos ) )
		MouseOver = DMO_FrameRight;
	else
	{
		// Check against other tabbed containers

		for( INT x = 0 ; x < TabbedContainers.Num() ; ++x )
		{
			WxDockingTabbedContainer* dtc = TabbedContainers(x);
			if( dtc->IsShown() && dtc != DraggedContainer )
			{
				wxRect rc = dtc->GetRect();
				if( rc.Inside( wxGetMousePosition() ) )
				{
					DestContainer = dtc;
					MouseOver = DMO_TabbedContainer;
					break;
				}
			}
		}
	}

}

void WxDockingRoot::UpdateMouseCursor()
{
	switch( MouseOver )
	{
		case DMO_None:					DraggedContainer->SetCursor( wxCURSOR_ARROW );		break;
		case DMO_FrameTop:
		case DMO_FrameRight:
		case DMO_FrameBottom:
		case DMO_FrameLeft:				DraggedContainer->SetCursor( wxCURSOR_WAIT );		break;
		case DMO_TabbedContainer:		DraggedContainer->SetCursor( wxCURSOR_ARROWWAIT );	break;
	}

}

WxDockingTabbedContainer* WxDockingRoot::AddTabbedContainer( wxWindow* InParent, INT InX, INT InY, INT InWidth, INT InHeight )
{
	WxDockingTabbedContainer* dtc = new WxDockingTabbedContainer( InParent, -1 );
	TabbedContainers.AddItem( dtc );
	dtc->SetSize( InX, InY, InWidth, InHeight );

	return dtc;

}

// Removes all pages from InB and adds them to InA

void WxDockingRoot::MergeTabbedContainers( WxDockingTabbedContainer* InA, WxDockingTabbedContainer* InB )
{
	if( !InA || !InB )
		return;

	check( InA != InB );

	for( INT x = 0 ; x < InB->NotebookCtrl->GetPageCount() ; ++x )
	{
		WxDockableWindow* dw = ((WxDockableWindow*)InB->NotebookCtrl->GetPage(x));
		EDockableWindowType Type = dw->Type;
		InA->AddDockableWindow( Type );
	}

	InB->NotebookCtrl->DeleteAllPages();
	InB->DockableWindows.Empty();

}

// Docks InDockingTabbedContainer against InEdge

void WxDockingRoot::Dock( WxDockingTabbedContainer* InDockingTabbedContainer, EDockingEdge InEdge )
{
	// Can't dock a window that is not being set to a valid edge

	if( InEdge == DE_Floating )
		return;

	// Remove the resizing border.

	DWORD Style = InDockingTabbedContainer->GetWindowStyleFlag();
	Style &= ~wxRESIZE_BORDER;
	InDockingTabbedContainer->SetWindowStyleFlag( Style );

	// Set the docking depth

	InDockingTabbedContainer->Edge = InEdge;
	InDockingTabbedContainer->DockingDepth = GetDockingDepth( InEdge, DEF_DOCKING_DEPTH );
	InDockingTabbedContainer->FloatingRect = InDockingTabbedContainer->GetRect();

	if( InEdge == DE_Container )
	{
		// If docking inside another tabbed container, we need to destroy the dockable windows inside
		// the source container and add new ones to the destination container.

		MergeTabbedContainers( DestContainer, InDockingTabbedContainer );
		CleanUpDeadContainers();
	}
	else

	// Update all docking window positions

	GApp->EditorFrame->OnSize( wxSizeEvent() );
	LayoutDockedWindows();

}

// Destroys any containers that don't have any windows docked in them.

void WxDockingRoot::CleanUpDeadContainers()
{
	for( INT x = 0 ; x < TabbedContainers.Num() ; ++x )
	{
		WxDockingTabbedContainer* dtc = TabbedContainers(x);
		if( !dtc->NotebookCtrl->GetPageCount() )
		{
			dtc->Destroy();
			TabbedContainers.Remove(x);
			x = -1;
		}
	}

}

void WxDockingRoot::DeleteTabbedContainer( WxDockingTabbedContainer* InTabbedContainer )
{
	INT idx = TabbedContainers.FindItemIndex( InTabbedContainer );
	check( idx != INDEX_NONE );
	TabbedContainers.Remove( idx );
	delete InTabbedContainer;

}

// Turns InDockingTabbedContainer into a floating window by adding a caption and a resizing border.

void WxDockingRoot::Float( WxDockingTabbedContainer* InDockingTabbedContainer )
{
	// Add a resizing border.

	InDockingTabbedContainer->SetWindowStyleFlag( InDockingTabbedContainer->GetWindowStyleFlag() | wxRESIZE_BORDER );
	InDockingTabbedContainer->Edge = DE_Floating;
	InDockingTabbedContainer->SetSize( InDockingTabbedContainer->FloatingRect );

	GApp->EditorFrame->OnSize( wxSizeEvent() );
	LayoutDockedWindows();

}

wxRect WxDockingRoot::GetEdgeRect( EDockingEdge InEdge )
{
	wxRect rc = GetClientRect();
	INT TopDepth = GetDockingDepth( DE_Top, 0 ),
		RightDepth = GetDockingDepth( DE_Right, 0 ),
		BottomDepth = GetDockingDepth( DE_Bottom, 0 ),
		LeftDepth = GetDockingDepth( DE_Left, 0 );

	wxRect EdgeRect;

	switch( InEdge )
	{
		case DE_Top:	EdgeRect = wxRect( 0, 0, rc.GetWidth()-RightDepth, TopDepth );										break;
		case DE_Right:	EdgeRect = wxRect( rc.GetWidth()-RightDepth, 0, RightDepth, rc.GetHeight()-BottomDepth );			break;
		case DE_Bottom:	EdgeRect = wxRect( LeftDepth, rc.GetHeight()-BottomDepth, rc.GetWidth()-LeftDepth, BottomDepth );	break;
		case DE_Left:	EdgeRect = wxRect( 0, TopDepth, LeftDepth, rc.GetHeight()-TopDepth );								break;
	}

	return EdgeRect;

}

// Sets a good position/size for all docked windows

void WxDockingRoot::LayoutDockedWindows()
{
	wxRect rc = GetClientRect();
	wxRect rcTopEdge = GetEdgeRect( DE_Top ),
		rcRightEdge = GetEdgeRect( DE_Right ),
		rcBottomEdge = GetEdgeRect( DE_Bottom ),
		rcLeftEdge = GetEdgeRect( DE_Left );

	for( INT e = DE_Dockable_Start ; e < DE_Dockable_End ; ++e )
	{
		INT Offset = 0;
		FLOAT WindowsOnEdge = (FLOAT)CountWindowsOnEdge( (EDockingEdge)e );
		FLOAT Div = 1.f / Max( WindowsOnEdge, 1.f );

		for( INT x = 0 ; x < TabbedContainers.Num() ; ++x )
		{
			WxDockingTabbedContainer* dtc = TabbedContainers(x);

			if( dtc->Edge == e )
			{
				switch( e )
				{
					case DE_Top:
					{
						wxPoint pos = ClientToScreen( wxPoint( Offset, 0 ) );

						dtc->SetSize( pos.x, pos.y, rcTopEdge.GetWidth() * Div, rcTopEdge.GetHeight() );
						if( dtc->IsShown() )
							Offset += rcTopEdge.GetWidth() * Div;
					}
					break;

					case DE_Right:
					{
						wxPoint pos = ClientToScreen( wxPoint( rc.GetWidth()-rcRightEdge.GetWidth(), Offset ) );

						dtc->SetSize( pos.x, pos.y, rcRightEdge.GetWidth(), rcRightEdge.GetHeight() * Div );
						if( dtc->IsShown() )
							Offset += rcRightEdge.GetHeight() * Div;
					}
					break;

					case DE_Bottom:
					{
						wxPoint pos = ClientToScreen( wxPoint( rcLeftEdge.GetWidth()+Offset, rc.GetHeight()-rcBottomEdge.GetHeight() ) );

						dtc->SetSize( pos.x, pos.y, rcBottomEdge.GetWidth() * Div, rcBottomEdge.GetHeight() );
						if( dtc->IsShown() )
							Offset += rcBottomEdge.GetWidth() * Div;
					}
					break;

					case DE_Left:
					{
						wxPoint pos = ClientToScreen( wxPoint( 0, Offset+rcTopEdge.GetHeight() ) );

						dtc->SetSize( pos.x, pos.y, rcLeftEdge.GetWidth(), rcLeftEdge.GetHeight() * Div );
						if( dtc->IsShown() )
							Offset += rcLeftEdge.GetHeight() * Div;
					}
					break;
				}
			}
		}
	}

	Lower();

}

// Determines the largest docking depth for all windows on InEdge

INT WxDockingRoot::GetDockingDepth( EDockingEdge InEdge, INT InDefValue )
{
	INT DockingDepth = InDefValue;

	for( INT x = 0 ; x < TabbedContainers.Num() ; ++x )
	{
		WxDockingTabbedContainer* dtc = TabbedContainers(x);
		if( dtc->IsShown() && dtc->Edge == InEdge && dtc->DockingDepth > DockingDepth )
				DockingDepth = dtc->DockingDepth;
	}

	return DockingDepth;

}

EDockingEdge WxDockingRoot::GetEdgeFromMouseOver( EDockingMouseOver InMouseOver )
{
	switch( InMouseOver )
	{
		case DMO_FrameTop:				return DE_Top;
		case DMO_FrameRight:			return DE_Right;
		case DMO_FrameBottom:			return DE_Bottom;
		case DMO_FrameLeft:				return DE_Left;
		case DMO_TabbedContainer:		return DE_Container;
	}

	return DE_Floating;

}

void WxDockingRoot::OnSize( wxSizeEvent& In )
{
	LayoutDockedWindows();

	In.Skip();

}

INT WxDockingRoot::CountWindowsOnEdge( EDockingEdge InEdge )
{
	INT Count = 0;

	for( INT x = 0 ; x < TabbedContainers.Num() ; ++x )
	{
		WxDockingTabbedContainer* dtc = TabbedContainers(x);
		if( dtc->IsShown() && dtc->IsDocked() && dtc->Edge == InEdge )
			Count++;
	}

	return Count;

}

WxDockableWindow* WxDockingRoot::CreateDockableWindow( wxWindow* InParent, EDockableWindowType InType )
{
	switch( InType )
	{
		case DWT_GROUP_BROWSER:			return new WxGroupBrowser( InParent, -1 );
		case DWT_LAYER_BROWSER:			return new WxLayerBrowser( InParent, -1 );
		case DWT_ACTOR_BROWSER:			return new WxActorBrowser( InParent, -1 );		
		case DWT_GENERIC_BROWSER:		return new WxGenericBrowser( InParent, -1 );
	}

	return NULL;

}

// Writes all docking state information to the INI file

void WxDockingRoot::SaveConfig()
{
	GConfig->EmptySection( TEXT("Docking"), GEditorIni );
	GConfig->SetInt( TEXT("Docking"), TEXT("NumContainers"), TabbedContainers.Num(), GEditorIni );

	for( INT tc = 0 ; tc < TabbedContainers.Num() ; ++tc )
	{
		WxDockingTabbedContainer* dtc = TabbedContainers(tc);

		// Update information before saving

		if( dtc->IsDocked() )
			dtc->DockingDepth = GetDockingDepth( dtc->Edge, DEF_DOCKING_DEPTH );
		else
			dtc->FloatingRect = dtc->GetRect();

		FString Wk;
		Wk += FString::Printf( TEXT("%d,%d,%d,%d,%d,%d,%d"), dtc->IsShown(), (INT)dtc->Edge, dtc->DockingDepth, dtc->FloatingRect.GetLeft(), dtc->FloatingRect.GetTop(), dtc->FloatingRect.GetWidth(), dtc->FloatingRect.GetHeight() );
		for( INT x = 0 ; x < dtc->NotebookCtrl->GetPageCount() ; ++x )
		{
			WxDockableWindow* dw = (WxDockableWindow*)dtc->NotebookCtrl->GetPage( x );
			Wk += FString::Printf( TEXT(",%d"), (INT)dw->Type );
		}

		GConfig->SetString( TEXT("Docking"), *FString::Printf( TEXT("Container%d"), tc ), *Wk, GEditorIni );
	}

	GConfig->Flush( 0, GEditorIni );

}

// Reads all docking state information from the INI file and creates/positions windows accordingly

void WxDockingRoot::LoadConfig()
{
	INT NumContainers;
	if( !GConfig->GetInt( TEXT("Docking"), TEXT("NumContainers"), NumContainers, GEditorIni ) )
		NumContainers = 0;

	for( INT tc = 0 ; tc < NumContainers ; ++tc )
	{
		FString Wk;
		GConfig->GetString( TEXT("Docking"), *FString::Printf( TEXT("Container%d"), tc ), Wk, GEditorIni );

		// Break the string down into fields

		TArray<FString> Fields;
		Wk.ParseIntoArray( TEXT(","), &Fields );

		// Extract information related to the tabbed container

		WxDockingTabbedContainer* dtc = AddTabbedContainer( this, 0, 0, 100, 100 );
        dtc->Show( appAtoi( *Fields(0) ) ? true : false );
		dtc->Edge = (EDockingEdge)appAtoi( *Fields(1) );
		dtc->DockingDepth = appAtoi( *Fields(2) );
		dtc->FloatingRect.x = appAtoi( *Fields(3) );
		dtc->FloatingRect.y = appAtoi( *Fields(4) );
		dtc->FloatingRect.width = appAtoi( *Fields(5) );
		dtc->FloatingRect.height = appAtoi( *Fields(6) );
		dtc->SetSize( dtc->FloatingRect );

		// Create whichever docking windows go inside of this container

		for( INT x = 7 ; x < Fields.Num() ; ++x )
			dtc->AddDockableWindow( (EDockableWindowType)appAtoi( *Fields(x) ) );

		// Final set up on the tabbed container

		if( dtc->IsDocked() )
			Dock( dtc, dtc->Edge );
		else
			Float( dtc );
	}

}

// Finds the tabbed container that houses the specified window type, and shows it

void WxDockingRoot::ShowDockableWindow( EDockableWindowType InType )
{
	for( INT tc = 0 ; tc < TabbedContainers.Num() ; ++tc )
	{
		WxDockingTabbedContainer* dtc = TabbedContainers(tc);

		for( INT x = 0 ; x < dtc->DockableWindows.Num() ; ++x )
		{
			if( dtc->DockableWindows(x)->Type == InType )
			{
				dtc->Show();
				dtc->NotebookCtrl->SetSelection( x );
				dtc->Raise();
				OnSize( wxSizeEvent() );
				GApp->EditorFrame->OnSize( wxSizeEvent() );
				return;
			}
		}
	}

	// If we got this far, the docking window doesn't exist.  So find the first
	// available docking container and create it inside of there.

	for( INT tc = 0 ; tc < TabbedContainers.Num() ; ++tc )
	{
		WxDockingTabbedContainer* dtc = TabbedContainers(tc);
		dtc->AddDockableWindow( InType );
		ShowDockableWindow( InType );
		break;
	}
}

// Finds a dockable window from its type

WxDockableWindow* WxDockingRoot::FindDockableWindow( EDockableWindowType InType )
{
	for( INT tc = 0 ; tc < TabbedContainers.Num() ; ++tc )
	{
		WxDockingTabbedContainer* dtc = TabbedContainers(tc);

		for( INT x = 0 ; x < dtc->DockableWindows.Num() ; ++x )
		{
			if( dtc->DockableWindows(x)->Type == InType )
			{
				return dtc->DockableWindows(x);
			}
		}
	}

	check(0);	// Should never get here
	return NULL;
}

void WxDockingRoot::RefreshEditor( DWORD InFlags )
{
	if( InFlags&ERefreshEditor_ActorBrowser )
		((WxActorBrowser*)FindDockableWindow(DWT_ACTOR_BROWSER))->Refresh();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
