
#include "XWindowPrivate.h"

/*-----------------------------------------------------------------------------
	FWindowUtil.
-----------------------------------------------------------------------------*/

FWindowUtil::FWindowUtil()
{
}

FWindowUtil::~FWindowUtil()
{
}

// Returns the width of InA as a percentage in relation to InB.

FLOAT FWindowUtil::GetWidthPct( const wxRect& InA, const wxRect& InB )
{
	return InA.GetWidth() / (FLOAT)InB.GetWidth();

}

// Returns the height of InA as a percentage in relation to InB.

FLOAT FWindowUtil::GetHeightPct( const wxRect& InA, const wxRect& InB )
{
	return InA.GetHeight() / (FLOAT)InB.GetHeight();

}

// Returns the real client area of this window, minus any toolbars and other docked controls.

wxRect FWindowUtil::GetClientRect( const wxWindow* InThis, const wxToolBar* InToolBar )
{
	wxRect rc = InThis->GetClientRect();

	if( InToolBar )
	{
		rc.y += InToolBar->GetClientRect().GetHeight();
		rc.height -= InToolBar->GetClientRect().GetHeight();
	}
	
	return rc;

}

// Loads the position/size and other information about InWindow from the INI file
// and applies them.

void FWindowUtil::LoadPosSize( const FString& InName, wxWindow* InWindow, INT InX, INT InY, INT InW, INT InH )
{
	FString Wk;
	GConfig->GetString( TEXT("WindowPosManager"), *InName, Wk, GEditorIni );

	TArray<FString> Args;
	wxRect rc(InX,InY,InW,InH);
	if( Wk.ParseIntoArray( TEXT(","), &Args ) == 4 )
	{
		// Break out the arguments

		INT X = appAtoi( *Args(0) );
		INT Y = appAtoi( *Args(1) );
		INT W = appAtoi( *Args(2) );
		INT H = appAtoi( *Args(3) );

		// Make sure that the window is going to be on the visible screen

		INT vleft = ::GetSystemMetrics( SM_XVIRTUALSCREEN );
		INT vtop = ::GetSystemMetrics( SM_YVIRTUALSCREEN );
		INT vright = ::GetSystemMetrics( SM_CXVIRTUALSCREEN );
		INT vbottom = ::GetSystemMetrics( SM_CYVIRTUALSCREEN );

		if( X < vleft || X+W >= vright )		X = vleft;
		if( Y < vtop || Y+H >= vbottom )		Y = vtop;

		// Set the windows attributes

		rc.SetX( X );
		rc.SetY( Y );
		rc.SetWidth( W );
		rc.SetHeight( H );
	}

	InWindow->SetSize( rc );
}

// Saves the position/size and other relevant info about InWindow to the INI file.

void FWindowUtil::SavePosSize( const FString& InName, const wxWindow* InWindow )
{
	wxRect rc = InWindow->GetRect();

	FString Wk = *FString::Printf( TEXT("%d,%d,%d,%d"), rc.GetX(), rc.GetY(), rc.GetWidth(), rc.GetHeight() );
	GConfig->SetString( TEXT("WindowPosManager"), *InName, *Wk, GEditorIni );
}

/*-----------------------------------------------------------------------------
	FTrackPopupMenu.
-----------------------------------------------------------------------------*/

FTrackPopupMenu::FTrackPopupMenu( wxWindow* InWindow )
{
	Window = InWindow;
	Menu = new wxMenu;
	bDeleteMenu = 1;
}

FTrackPopupMenu::FTrackPopupMenu( wxWindow* InWindow, wxMenu* InMenu )
{
	Window = InWindow;
	Menu = InMenu;
	bDeleteMenu = 0;
}

FTrackPopupMenu::~FTrackPopupMenu()
{
	if( bDeleteMenu )
		delete Menu;
}

void FTrackPopupMenu::Show( INT InX, INT InY )
{
	wxPoint pt( InX, InY );
	if( InX == - 1)			// -1 means to use the current mouse position
		pt = Window->ScreenToClient( wxGetMousePosition() );

	Window->PopupMenu( Menu, pt );

}

/*-----------------------------------------------------------------------------
	WxPaintDC.
-----------------------------------------------------------------------------*/

WxPaintDC::WxPaintDC( wxWindow* InWindow )
	: wxPaintDC( InWindow )
{
}

WxPaintDC::~WxPaintDC()
{
}

void WxPaintDC::DrawHTiledBitmap( wxBitmap& InBitmap, INT InX, INT InY, INT InWidth )
{
	INT BmpWidth = InBitmap.GetWidth(),
		Tiles = InWidth / BmpWidth;

	INT XPos = InX;
	for( INT x = 0 ; x < Tiles ; ++x )
		DrawBitmap( InBitmap, XPos*BmpWidth, InY );
}

/*-----------------------------------------------------------------------------
	FDragChecker.
-----------------------------------------------------------------------------*/

FDragChecker::FDragChecker()
{
	bLimitExceeded = 0;
}

FDragChecker::FDragChecker( wxPoint InStart, INT InLimit )
{
	Set( InStart, InLimit );
}

FDragChecker::~FDragChecker()
{
}

void FDragChecker::Set( wxPoint InStart, INT InLimit )
{
	Start = InStart;
	Limit = InLimit;
	bLimitExceeded = 0;
}

UBOOL FDragChecker::ShouldDrag( wxPoint InCurrent )
{
	if( bLimitExceeded )	return 1;

	if( Abs( InCurrent.x - Start.x ) > Limit || Abs( InCurrent.y - Start.y ) > Limit )
	{
		bLimitExceeded = 1;
		return 1;
	}

	return 0;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
