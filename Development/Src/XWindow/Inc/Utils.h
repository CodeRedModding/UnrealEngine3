/*=============================================================================
	Utils.h: Utility classes

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/*-----------------------------------------------------------------------------
	FWindowUtil.

	Utilities for windows.
-----------------------------------------------------------------------------*/

class FWindowUtil
{
	FWindowUtil();
	~FWindowUtil();

public:
	static FLOAT GetWidthPct( const wxRect& InA, const wxRect& InB );
	static FLOAT GetHeightPct( const wxRect& InA, const wxRect& InB );
	static wxRect GetClientRect( const wxWindow* InThis, const wxToolBar* InToolBar );
	static void LoadPosSize( const FString& InName, wxWindow* InWindow, INT InX = -1, INT InY = -1, INT InW = -1, INT InH = -1 );
	static void SavePosSize( const FString& InName, const wxWindow* InWindow );
};

/*-----------------------------------------------------------------------------
	FTrackPopupMenu.

	Utility class for creating popup menus.
-----------------------------------------------------------------------------*/

class FTrackPopupMenu
{
public:
	FTrackPopupMenu( wxWindow* InWindow );
	FTrackPopupMenu( wxWindow* InWindow, wxMenu* InMenu );
	~FTrackPopupMenu();

	void Show( INT InX = -1, INT InY = -1 );

	wxWindow* Window;
	wxMenu* Menu;
	UBOOL bDeleteMenu;			// Should the menu be deleted when class is destructed?
};

/*-----------------------------------------------------------------------------
	WxPaintDC.

	Adds functionality to wxPaintDC.
-----------------------------------------------------------------------------*/

class WxPaintDC : public wxPaintDC
{
public:
	WxPaintDC( wxWindow* InWindow );
	~WxPaintDC();

	void DrawHTiledBitmap( wxBitmap& InBitmap, INT InX, INT InY, INT InWidth );
};

/*-----------------------------------------------------------------------------
	FDragChecker.

	For checking if somethings drag limit has been exceeded.
-----------------------------------------------------------------------------*/

class FDragChecker
{
public:
	FDragChecker();
	FDragChecker( wxPoint InStart, INT InLimit );
	~FDragChecker();

	void Set( wxPoint InStart, INT InLimit );
	UBOOL ShouldDrag( wxPoint InCurrent );

	wxPoint Start;
	INT Limit;
	UBOOL bLimitExceeded;			// If this is set to 1, the checker will always say that dragging is allowed (usually set the first time ShouldDrag succeeds)
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
