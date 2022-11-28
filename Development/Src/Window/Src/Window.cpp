/*=============================================================================
	Window.cpp: GUI window management code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "Engine.h"
#include "Window.h"

#include "..\..\Launch\Resources\resource.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

WNDPROC WTabControl::SuperProc;
WNDPROC WLabel::SuperProc;
WNDPROC WGroupBox::SuperProc;
WNDPROC WCustomLabel::SuperProc;
WNDPROC WListView::SuperProc;
WNDPROC WEdit::SuperProc;
WNDPROC WRichEdit::SuperProc;
WNDPROC WListBox::SuperProc;
WNDPROC WCheckListBox::SuperProc;
WNDPROC WBitmapButton::SuperProc;
WNDPROC WColorButton::SuperProc;
WNDPROC WTrackBar::SuperProc;
WNDPROC WProgressBar::SuperProc;
WNDPROC WComboBox::SuperProc;
WNDPROC WButton::SuperProc;
WNDPROC WPropertySheet::SuperProc;
WNDPROC WToolTip::SuperProc;
WNDPROC WCoolButton::SuperProc;
WNDPROC WUrlButton::SuperProc;
WNDPROC WCheckBox::SuperProc;
WNDPROC WScrollBar::SuperProc;
WNDPROC WTreeView::SuperProc;
INT WWindow::ModalCount=0;
TArray<WWindow*> WWindow::_Windows;
TArray<WWindow*> WWindow::_DeleteWindows;
TArray<WProperties*> WProperties::PropertiesWindows;
HBRUSH hBrushBlack;
HBRUSH hBrushWhite;
HBRUSH hBrushOffWhite;
HBRUSH hBrushHeadline;
HBRUSH hBrushCurrent;
HBRUSH hBrushDark;
HBRUSH hBrushGrey;
HBRUSH hBrushGreyWindow;
HBRUSH hBrushGrey160;
HBRUSH hBrushGrey180;
HBRUSH hBrushGrey197;
HBRUSH hBrushCyanHighlight;
HBRUSH hBrushCyanLow;
HBRUSH hBrushDarkGrey;
HFONT hFontUrl;
HFONT hFontText;
HFONT hFontHeadline;
HINSTANCE hInstanceWindow;
UBOOL GNotify=0;
WCoolButton* WCoolButton::GlobalCoolButton=NULL;
UINT WindowMessageOpen;
UINT WindowMessageMouseWheel;

/*-----------------------------------------------------------------------------
	Window manager.
-----------------------------------------------------------------------------*/

W_IMPLEMENT_CLASS(WWindow)
W_IMPLEMENT_CLASS(WControl)
W_IMPLEMENT_CLASS(WTabControl)
W_IMPLEMENT_CLASS(WLabel)
W_IMPLEMENT_CLASS(WGroupBox)
W_IMPLEMENT_CLASS(WCustomLabel)
W_IMPLEMENT_CLASS(WListView)
W_IMPLEMENT_CLASS(WButton)
W_IMPLEMENT_CLASS(WPropertySheet)
W_IMPLEMENT_CLASS(WToolTip)
W_IMPLEMENT_CLASS(WCoolButton)
W_IMPLEMENT_CLASS(WUrlButton)
W_IMPLEMENT_CLASS(WComboBox)
W_IMPLEMENT_CLASS(WEdit)
W_IMPLEMENT_CLASS(WRichEdit)
W_IMPLEMENT_CLASS(WTerminalBase)
W_IMPLEMENT_CLASS(WTerminal)
W_IMPLEMENT_CLASS(WDialog)
W_IMPLEMENT_CLASS(WTrackBar)
W_IMPLEMENT_CLASS(WProgressBar)
W_IMPLEMENT_CLASS(WListBox)
W_IMPLEMENT_CLASS(WItemBox)
W_IMPLEMENT_CLASS(WCheckListBox)
W_IMPLEMENT_CLASS(WBitmapButton)
W_IMPLEMENT_CLASS(WColorButton)
W_IMPLEMENT_CLASS(WPropertiesBase)
W_IMPLEMENT_CLASS(WDragInterceptor)
W_IMPLEMENT_CLASS(WPictureButton)
W_IMPLEMENT_CLASS(WPropertyPage)
W_IMPLEMENT_CLASS(WProperties)
W_IMPLEMENT_CLASS(WObjectProperties)
W_IMPLEMENT_CLASS(WClassProperties)
W_IMPLEMENT_CLASS(WEditTerminal)
W_IMPLEMENT_CLASS(WCheckBox)
W_IMPLEMENT_CLASS(WScrollBar)
W_IMPLEMENT_CLASS(WTreeView)

class UWindowManager : public USubsystem
{
	DECLARE_CLASS(UWindowManager,UObject,CLASS_Transient,Window);

	// Constructor.
	UWindowManager()
	{
		// Init common controls.
		InitCommonControls();

		// Save instance.
		hInstanceWindow = hInstance;

		LoadLibraryX(TEXT("RICHED32.DLL"));

		// Implement window classes.
		IMPLEMENT_WINDOWSUBCLASS(WListBox,TEXT("LISTBOX"));
		IMPLEMENT_WINDOWSUBCLASS(WItemBox,TEXT("LISTBOX"));
		IMPLEMENT_WINDOWSUBCLASS(WCheckListBox,TEXT("LISTBOX"));
		IMPLEMENT_WINDOWSUBCLASS(WBitmapButton,TEXT("BUTTON"));
		IMPLEMENT_WINDOWSUBCLASS(WColorButton,TEXT("BUTTON"));
		IMPLEMENT_WINDOWSUBCLASS(WTabControl,WC_TABCONTROL);
		IMPLEMENT_WINDOWSUBCLASS(WLabel,TEXT("STATIC"));
		IMPLEMENT_WINDOWSUBCLASS(WGroupBox,TEXT("BUTTON"));
		IMPLEMENT_WINDOWSUBCLASS(WCustomLabel,TEXT("STATIC"));
		IMPLEMENT_WINDOWSUBCLASS(WListView,TEXT("SysListView32"));
		IMPLEMENT_WINDOWSUBCLASS(WEdit,TEXT("EDIT"));
		IMPLEMENT_WINDOWSUBCLASS(WRichEdit,TEXT("RICHEDIT"));
		IMPLEMENT_WINDOWSUBCLASS(WComboBox,TEXT("COMBOBOX"));
		IMPLEMENT_WINDOWSUBCLASS(WEditTerminal,TEXT("EDIT"));
		IMPLEMENT_WINDOWSUBCLASS(WButton,TEXT("BUTTON"));
		IMPLEMENT_WINDOWSUBCLASS(WPropertySheet,TEXT("STATIC"));
		IMPLEMENT_WINDOWSUBCLASS(WToolTip,TOOLTIPS_CLASS);
		IMPLEMENT_WINDOWSUBCLASS(WCoolButton,TEXT("BUTTON"));
		IMPLEMENT_WINDOWSUBCLASS(WUrlButton,TEXT("BUTTON"));
		IMPLEMENT_WINDOWSUBCLASS(WCheckBox,TEXT("BUTTON"));
		IMPLEMENT_WINDOWSUBCLASS(WScrollBar,TEXT("SCROLLBAR"));
		IMPLEMENT_WINDOWSUBCLASS(WTreeView,WC_TREEVIEW);
		IMPLEMENT_WINDOWSUBCLASS(WTrackBar,TRACKBAR_CLASS);
		IMPLEMENT_WINDOWSUBCLASS(WProgressBar,PROGRESS_CLASS);
		IMPLEMENT_WINDOWCLASS(WTerminal,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WProperties,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WObjectProperties,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WClassProperties,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WDragInterceptor,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WPictureButton,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WPropertyPage,CS_DBLCLKS);
		
		// Create brushes.
		hBrushBlack			= CreateSolidBrush( RGB(0,0,0) );
		hBrushWhite			= CreateSolidBrush( RGB(255,255,255) );
		hBrushOffWhite		= CreateSolidBrush( RGB(224,224,224) );
		hBrushHeadline		= CreateSolidBrush( RGB(200,200,200) );
		hBrushCurrent		= CreateSolidBrush( RGB(160,160,160) );
		hBrushDark			= CreateSolidBrush( RGB(64,64,64) );
		hBrushGrey			= CreateSolidBrush( RGB(128,128,128) );
		hBrushGreyWindow	= CreateSolidBrush( GetSysColor( COLOR_3DFACE ) );
		hBrushGrey160		= CreateSolidBrush( RGB(160,160,160) );
		hBrushGrey180		= CreateSolidBrush( RGB(180,180,180) );
		hBrushGrey197		= CreateSolidBrush( RGB(197,197,197) );
		hBrushCyanHighlight	= CreateSolidBrush( RGB(0,200,200) );
		hBrushCyanLow		= CreateSolidBrush( RGB(0,140,140) );

		// Create fonts.
		HDC hDC       = GetDC( NULL );
#ifndef JAPANESE
		hFontText     = CreateFont( -MulDiv(9/*PointSize*/,  GetDeviceCaps(hDC, LOGPIXELSY), 72), 0, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, TEXT("Arial") );
		hFontUrl      = CreateFont( -MulDiv(9/*PointSize*/,  GetDeviceCaps(hDC, LOGPIXELSY), 72), 0, 0, 0, 0, 0, 1, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, TEXT("Arial") );
		hFontHeadline = CreateFont( -MulDiv(15/*PointSize*/, GetDeviceCaps(hDC, LOGPIXELSY), 72), 0, 0, FW_BOLD, 1, 1, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, TEXT("Arial") );
#else
		hFontText     = (HFONT)( GetStockObject( DEFAULT_GUI_FONT ) );
		hFontUrl      = (HFONT)( GetStockObject( DEFAULT_GUI_FONT ) );
		hFontHeadline = (HFONT)( GetStockObject( DEFAULT_GUI_FONT ) );
#endif
		ReleaseDC( NULL, hDC );

		// Custom window messages.
		WindowMessageOpen       = RegisterWindowMessageX( TEXT("UnrealOpen") );
		WindowMessageMouseWheel = RegisterWindowMessageX( TEXT("MSWHEEL_ROLLMSG") );

	}

	// FExec interface.
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar )
	{
		return 0;
	}

	// UObject interface.
	void Serialize( FArchive& Ar )
	{
		Super::Serialize( Ar );
		for( INT i=0; i<WWindow::_Windows.Num(); i++ )
			WWindow::_Windows(i)->Serialize( Ar );
		for( INT i=0; i<WWindow::_DeleteWindows.Num(); i++ )
			WWindow::_DeleteWindows(i)->Serialize( Ar );
	}
	void Destroy()
	{
		Super::Destroy();

		// Delete brushes.
		DeleteObject( hBrushBlack );
		DeleteObject( hBrushWhite );
		DeleteObject( hBrushOffWhite );
		DeleteObject( hBrushHeadline );
		DeleteObject( hBrushCurrent );
		DeleteObject( hBrushDark );
		DeleteObject( hBrushGrey );
		DeleteObject( hBrushGreyWindow );
		DeleteObject( hBrushGrey160 );
		DeleteObject( hBrushGrey180 );
		DeleteObject( hBrushGrey197 );
		DeleteObject( hBrushCyanHighlight );
		DeleteObject( hBrushCyanLow );

		check(GWindowManager==this);
		GWindowManager = NULL;
		if( !GIsCriticalError )
			Tick( 0.0 );
		WWindow::_Windows.Empty();
		WWindow::_DeleteWindows.Empty();
		WProperties::PropertiesWindows.Empty();
	}

	// USubsystem interface.
	void Tick( FLOAT DeltaTime )
	{
		while( WWindow::_DeleteWindows.Num() )
		{
			WWindow* W = WWindow::_DeleteWindows( 0 );
			delete W;
			check(WWindow::_DeleteWindows.FindItemIndex(W)==INDEX_NONE);
		}
	}
};
IMPLEMENT_CLASS(UWindowManager);

/*-----------------------------------------------------------------------------
	Functions.
-----------------------------------------------------------------------------*/

static HBITMAP LoadFileToBitmap( const TCHAR* Filename, INT& SizeX, INT& SizeY )
{
	TArray<BYTE> Bitmap;
	if( appLoadFileToArray(Bitmap,Filename) )
	{
		HDC              hDC = GetDC(NULL);
		BITMAPFILEHEADER* FH = (BITMAPFILEHEADER*)&Bitmap(0);
		BITMAPINFO*       BM = (BITMAPINFO      *)(FH+1);
		BITMAPINFOHEADER* IH = (BITMAPINFOHEADER*)(FH+1);
		RGBQUAD*          RQ = (RGBQUAD         *)(IH+1);
		BYTE*             BY = (BYTE            *)(RQ+(1<<IH->biBitCount));
		SizeX                = IH->biWidth;
		SizeY                = IH->biHeight;
		HBITMAP      hBitmap = CreateDIBitmap( hDC, IH, CBM_INIT, BY, BM, DIB_RGB_COLORS );
		ReleaseDC( NULL, hDC );
		return hBitmap;
	}
	return NULL;
}

void InitWindowing()
{
	GWindowManager = new UWindowManager;
	GWindowManager->AddToRoot();
}

/*-----------------------------------------------------------------------------
	Splash screen.
-----------------------------------------------------------------------------*/

#include "..\..\Launch\Resources\resource.h"

/** HWND for splash window set by InitSplash and used by ExitSplash */
static HWND hWndSplash = NULL;

/** Resource ID of icon to use for Window */
extern INT GGameIcon;

/**
 * Dummy WindProc.
 *
 * @param	hWnd		unused
 * @param	uMsg		unused
 * @param	wParam		unused
 * @param	lParam		unused
 */
BOOL CALLBACK SplashDialogProc( HWND /*hWnd*/, UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/ )
{
	return 0;
}

/** 
 * Displays a centered splash screen using the Win32 API
 *
 * @param Filename	name of bitmap to display (needs to be palettized for compat reasons)
 */
void InitSplash( const TCHAR* Filename )
{
	check( Filename );

	INT     SizeX	= 0;
	INT     SizeY	= 0;
	HBITMAP hBitmap	= LoadFileToBitmap( Filename, SizeX, SizeY );
	check(hBitmap);

	hWndSplash = TCHAR_CALL_OS(CreateDialogW(hInstance,MAKEINTRESOURCEW(IDDIALOG_Splash), NULL, SplashDialogProc),CreateDialogA(hInstance, MAKEINTRESOURCEA(IDDIALOG_Splash), NULL, SplashDialogProc) );
	if( hWndSplash )
	{
		//@todo: the switcheroo doesn't happen fast enough so we need a better solution in the long run
		LPARAM Icon = (LPARAM) LoadIconIdX(hInstance,GGameIcon);
		SendMessageX( hWndSplash, WM_SETICON, (WPARAM)ICON_BIG, Icon );
		SendMessageX( hWndSplash, WM_SETICON, (WPARAM)ICON_SMALL, Icon );

		HWND hWndLogo = GetDlgItem(hWndSplash,IDC_Logo);
		if( hWndLogo )
		{
			SetWindowPos( hWndSplash, HWND_TOPMOST,(GetSystemMetrics(SM_CXSCREEN)-SizeX)/2,(GetSystemMetrics(SM_CYSCREEN)-SizeY)/2,SizeX,SizeY,SWP_SHOWWINDOW);
			SetWindowPos( hWndSplash, HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
			SendMessageX( hWndLogo, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap );			
			UpdateWindow( hWndSplash );
		}
	}
}

/**
 * Removes the splash screen.	
 */
void ExitSplash()
{
	DestroyWindow( hWndSplash );
}

/*-----------------------------------------------------------------------------
	WWindow.
-----------------------------------------------------------------------------*/

// Use this procedure for modeless dialogs.
INT_PTR CALLBACK WWindow::StaticDlgProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	INT i;
	for( i=0; i<_Windows.Num(); i++ )
		if( _Windows(i)->hWnd==hwndDlg )
			break;
	if( i==_Windows.Num() && uMsg==WM_INITDIALOG )
	{
		WWindow* WindowCreate = (WWindow*)lParam;
		check(WindowCreate);
		check(!WindowCreate->hWnd);
		WindowCreate->hWnd = hwndDlg;
		for( i=0; i<_Windows.Num(); i++ )
			if( _Windows(i)==WindowCreate )
				break;
		check(i<_Windows.Num());
	}
	if( i!=_Windows.Num() && !GIsCriticalError )
	{
		_Windows(i)->WndProc( uMsg, wParam, lParam );			
	}

	// Give up cycles.
	//
	::Sleep(0);

	return 0;
}

LONG APIENTRY WWindow::StaticWndProc( HWND hWnd, UINT Message, UINT wParam, LONG lParam )
{
	INT i;
	for( i=0; i<_Windows.Num(); i++ )
		if( _Windows(i)->hWnd==hWnd )
			break;

	// Disable autoplay while any Unreal windows are open.
	static UINT QueryCancelAutoPlay = RegisterWindowMessage(TEXT("QueryCancelAutoPlay"));
	if( Message == QueryCancelAutoPlay )
    {
		SetWindowLong(hWnd, DWL_MSGRESULT, TRUE);
		return 1;
    }

	if( i==_Windows.Num() && (Message==WM_NCCREATE || Message==WM_INITDIALOG) )
	{
		WWindow* WindowCreate
		=	Message!=WM_NCCREATE
		?	(WWindow*)lParam
		:	(GetWindowLongX(hWnd,GWL_EXSTYLE) & WS_EX_MDICHILD)
		?	(WWindow*)((MDICREATESTRUCT*)((CREATESTRUCT*)lParam)->lpCreateParams)->lParam
		:	(WWindow*)((CREATESTRUCT*)lParam)->lpCreateParams;
		check(WindowCreate);
		check(!WindowCreate->hWnd);
		WindowCreate->hWnd = hWnd;
		for( i=0; i<_Windows.Num(); i++ )
			if( _Windows(i)==WindowCreate )
				break;
		check(i<_Windows.Num());
	}
	if( i==_Windows.Num() || GIsCriticalError )
	{
		// Gets through before WM_NCCREATE: WM_GETMINMAXINFO
		return DefWindowProcX( hWnd, Message, wParam, lParam );
	}
	else
	{
		return _Windows(i)->WndProc( Message, wParam, lParam );			
	}
}

WNDPROC WWindow::RegisterWindowClass( const TCHAR* Name, DWORD Style )
{
#if UNICODE
	if( GUnicodeOS )
	{
		WNDCLASSEXW Cls;
		appMemzero( &Cls, sizeof(Cls) );
		Cls.cbSize			= sizeof(Cls);
		Cls.style			= Style;
		Cls.lpfnWndProc		= StaticWndProc;
		Cls.hInstance		= hInstanceWindow;
		Cls.hIcon			= LoadIconIdX(hInstanceWindow,GGameIcon);
		Cls.lpszClassName	= Name;
		Cls.hIconSm			= LoadIconIdX(hInstanceWindow,GGameIcon);
		verify(RegisterClassExW( &Cls ));
	}
	else
#endif
	{
		WNDCLASSEXA Cls;
		appMemzero( &Cls, sizeof(Cls) );
		Cls.cbSize			= sizeof(Cls);
		Cls.style			= Style;
		Cls.lpfnWndProc		= StaticWndProc;
		Cls.hInstance		= hInstanceWindow;
		Cls.hIcon			= LoadIconIdX(hInstanceWindow,GGameIcon);
		Cls.lpszClassName	= TCHAR_TO_ANSI(Name);
		Cls.hIconSm			= LoadIconIdX(hInstanceWindow,GGameIcon);
		verify(RegisterClassExA( &Cls ));
	}
	return NULL;
}

#if WIN_OBJ
void WWindow::Destroy()
{
	Super::Destroy();
	MaybeDestroy();
	bShow = FALSE;
	WWindow::_DeleteWindows.RemoveItem( this );
}
#else
WWindow::~WWindow()
{
	MaybeDestroy();
	WWindow::_DeleteWindows.RemoveItem( this );
}
#endif

FRect WWindow::GetClientRect() const
{
	RECT R;
	::GetClientRect( hWnd, &R );
	return FRect( R );
}

void WWindow::MoveWindow( FRect R, UBOOL bRepaint )
{
	::MoveWindow( hWnd, R.Min.X, R.Min.Y, R.Width(), R.Height(), bRepaint );
}

void WWindow::MoveWindow( INT Left, INT Top, INT Width, INT Height, UBOOL bRepaint )
{
	::MoveWindow( hWnd, Left, Top, Width, Height, bRepaint );
}

void WWindow::ResizeWindow( INT Width, INT Height, UBOOL bRepaint )
{
	FRect rc = GetWindowRect();
	::MoveWindow( hWnd, rc.Min.X, rc.Min.Y, Width, Height, bRepaint );
}

FRect WWindow::GetWindowRect( UBOOL bConvert ) const
{
	RECT R;
	::GetWindowRect( hWnd, &R );
	if( bConvert )
		return OwnerWindow ? OwnerWindow->ScreenToClient(R) : FRect(R);
	else
		return FRect(R);
}

FPoint WWindow::ClientToScreen( const FPoint& InP )
{
	POINT P;
	P.x = InP.X;
	P.y = InP.Y;
	::ClientToScreen( hWnd, &P );
	return FPoint( P.x, P.y );
}

FPoint WWindow::ScreenToClient( const FPoint& InP )
{
	POINT P;
	P.x = InP.X;
	P.y = InP.Y;
	::ScreenToClient( hWnd, &P );
	return FPoint( P.x, P.y );
}

FRect WWindow::ClientToScreen( const FRect& InR )
{
	return FRect( ClientToScreen(InR.Min), ClientToScreen(InR.Max) );
}

FRect WWindow::ScreenToClient( const FRect& InR )
{
	return FRect( ScreenToClient(InR.Min), ScreenToClient(InR.Max) );
}

FPoint WWindow::GetCursorPos()
{
	FPoint Mouse;
	::GetCursorPos( Mouse );
	return ScreenToClient( Mouse );
}

void WWindow::Show( UBOOL Show )
{
	bShow = Show;
	ShowWindow( hWnd, Show ? (::IsIconic(hWnd) ? SW_RESTORE : SW_SHOW) : SW_HIDE );
}

void WWindow::Serialize( FArchive& Ar )
{
	//!!UObject interface.
	//!!Super::Serialize( Ar );
	Ar << PersistentName;
}

const TCHAR* WWindow::GetPackageName()
{
	return TEXT("Window");
}

void WWindow::DoDestroy()
{
	if( NotifyHook )
		NotifyHook->NotifyDestroy( this );
	if( hWnd )
		DestroyWindow( *this );
	_Windows.RemoveItem( this );
}

LONG WWindow::WndProc( UINT Message, UINT wParam, LONG lParam )
{
	try
	{
		LastwParam = wParam;
		LastlParam = lParam;

		// Message snoop.
		if( Snoop )
		{
			if( Message==WM_CHAR )
				Snoop->SnoopChar( this, wParam );
			else if( Message==WM_KEYDOWN )
				Snoop->SnoopKeyDown( this, wParam );
			else if( Message==WM_LBUTTONDOWN )
				Snoop->SnoopLeftMouseDown( this, FPoint(LOWORD(lParam),HIWORD(lParam)) );
			else if( Message==WM_RBUTTONDOWN )
				Snoop->SnoopRightMouseDown( this, FPoint(LOWORD(lParam),HIWORD(lParam)) );
		}

		// Special multi-window activation handling.
		if( !MdiChild && !ModalCount )
		{
			static UBOOL AppActive=0;
			if( Message==WM_ACTIVATEAPP )
			{
				AppActive = wParam;
				SendMessageX( hWnd, WM_NCACTIVATE, wParam, 0 );

			}
			else if( Message==WM_NCACTIVATE && AppActive && !wParam )
			{
				return 1;
			}
		}

		// Message processing.
		if( Message==WM_DESTROY ) { OnDestroy(); }
		else if( Message==WM_DRAWITEM )
		{
			DRAWITEMSTRUCT* Info = (DRAWITEMSTRUCT*)lParam;
			for( INT i=0; i<Controls.Num(); i++ )
				if( ((WWindow*)Controls(i))->hWnd==Info->hwndItem )
					{((WWindow*)Controls(i))->OnDrawItem(Info); break;}
			return 1;
		}
		else if( Message==WM_MEASUREITEM )
		{
			MEASUREITEMSTRUCT* Info = (MEASUREITEMSTRUCT*)lParam;
			for( INT i=0; i<Controls.Num(); i++ )
				if( ((WWindow*)Controls(i))->ControlId==Info->CtlID )
					{((WWindow*)Controls(i))->OnMeasureItem(Info); break;}
			return 1;
		}
		else if( Message==WM_CLOSE ) { OnClose(); }
		else if( Message==WM_CHAR ) { OnChar( wParam ); }
		else if( Message==WM_KEYDOWN ) { OnKeyDown( wParam ); }
		else if( Message==WM_PAINT ) { OnPaint(); }
		else if( Message==WM_CREATE ) { OnCreate(); }
		else if( Message==WM_TIMER ) { OnTimer(); }
		else if( Message==WM_INITDIALOG ) { OnInitDialog(); }
		else if( Message==WM_ENTERIDLE ) { OnEnterIdle(); }
		else if( Message==WM_SETFOCUS ) { OnSetFocus( (HWND)wParam ); }
		else if( Message==WM_ACTIVATE ) { OnActivate( LOWORD(wParam)!=0 ); }
		else if( Message==WM_KILLFOCUS ) { OnKillFocus( (HWND)wParam ); }
		else if( Message==WM_SIZE ) { OnSize( wParam, LOWORD(lParam), HIWORD(lParam) ); }
		else if( Message==WM_WINDOWPOSCHANGING )
		{
			WINDOWPOS* wpos = (LPWINDOWPOS)lParam;
			OnWindowPosChanging( &wpos->x, &wpos->y, &wpos->cx, &wpos->cy );
		}
		else if( Message==WM_MOVE ) { OnMove( LOWORD(lParam), HIWORD(lParam) ); }
		else if( Message==WM_PASTE ) { OnPaste(); }
		else if( Message==WM_SHOWWINDOW ) { OnShowWindow( wParam ); }
		else if( Message==WM_COPYDATA ) { OnCopyData( (HWND)wParam, (COPYDATASTRUCT*)lParam ); }
		else if( Message==WM_CAPTURECHANGED ) { OnReleaseCapture(); }
		else if( Message==WM_MDIACTIVATE ) { OnMdiActivate( (HWND)lParam==hWnd ); }
		else if( Message==WM_MOUSEMOVE ) { OnMouseMove( wParam, FPoint(LOWORD(lParam), HIWORD(lParam)) ); }
		else if( Message==WM_LBUTTONDOWN ) { OnLeftButtonDown(); }
		else if( Message==WM_LBUTTONDBLCLK ) { OnLeftButtonDoubleClick(); }
		else if( Message==WM_MBUTTONDBLCLK ) { OnMiddleButtonDoubleClick(); }
		else if( Message==WM_RBUTTONDBLCLK ) { OnRightButtonDoubleClick(); }
		else if( Message==WM_RBUTTONDOWN ) { OnRightButtonDown(); }
		else if( Message==WM_LBUTTONUP ) { OnLeftButtonUp(); }
		else if( Message==WM_RBUTTONUP ) { OnRightButtonUp(); }
		else if( Message==WM_CUT ) { OnCut(); }
		else if( Message==WM_COPY ) { OnCopy(); }
		else if( Message==WM_UNDO ) { OnUndo(); }
		else if( Message==WM_ERASEBKGND ) { if( OnEraseBkgnd() ) return 1; }
		else if( Message==WM_SETCURSOR )
		{
			if( OnSetCursor() )
				return 1;
		}
		else if( Message==WM_NOTIFY )
		{
			for( INT i=0; i<Controls.Num(); i++ )
				if(wParam==((WWindow*)Controls(i))->ControlId
						&& ((WWindow*)Controls(i))->InterceptControlCommand(Message,wParam,lParam) )
					return 1;
			OnCommand( wParam );
		}
		else if( Message==WM_VSCROLL ) { OnVScroll( wParam, lParam ); }
		else if( Message==WM_KEYUP) { OnKeyUp( wParam, lParam ); }
		else if( Message==WM_COMMAND || Message==WM_HSCROLL )
		{
			// Allow for normal handling as well as control delegates
			if( Message==WM_HSCROLL )
				OnHScroll( wParam, lParam );

			for( INT i=0; i<Controls.Num(); i++ )
				if((HWND)lParam==((WWindow*)Controls(i))->hWnd
						&& ((WWindow*)Controls(i))->InterceptControlCommand(Message,wParam,lParam) )
					return 1;
			OnCommand( wParam );
		}
		else if( Message==WM_SYSCOMMAND )
		{
			if( OnSysCommand( wParam ) )
				return 1;
		}
		return CallDefaultProc( Message, wParam, lParam );
	}
	catch( const TCHAR* )
	{
		// This exception prevents the message from being routed to the default proc.
		return 0;
	}
}

INT WWindow::CallDefaultProc( UINT Message, UINT wParam, LONG lParam )
{
	if( MdiChild )
		return DefMDIChildProcX( hWnd, Message, wParam, lParam );
	else
		return DefWindowProcX( hWnd, Message, wParam, lParam );
}

UBOOL WWindow::InterceptControlCommand( UINT Message, UINT wParam, LONG lParam )
{
	return 0;
}

FString WWindow::GetText()
{
	check(hWnd);
	INT Length = GetLength();
#if UNICODE
	if( GUnicode && !GUnicodeOS )
	{
		ANSICHAR* ACh = (ANSICHAR*)appAlloca((Length+1)*sizeof(ANSICHAR));
		SendMessageA( *this, WM_GETTEXT, Length+1, (LPARAM)ACh );
		return FString(ACh);
	}
	else
#endif
	{
		TCHAR* Text = (TCHAR*)appAlloca((Length+1)*sizeof(TCHAR));
		SendMessage( *this, WM_GETTEXT, Length+1, (LPARAM)Text );
		return Text;
	}
}

void WWindow::SetText( const TCHAR* Text )
{
	if( !::IsWindow( hWnd ) ) return;
	SendMessageLX( *this, WM_SETTEXT, 0, Text );
}

INT WWindow::GetLength()
{
	check(hWnd);
	return SendMessageX( *this, WM_GETTEXTLENGTH, 0, 0 );
}

void WWindow::SetNotifyHook( FNotifyHook* InNotifyHook )
{
	NotifyHook = InNotifyHook;
}

void WWindow::OnCopyData( HWND hWndSender, COPYDATASTRUCT* CD ) {}
void WWindow::OnSetFocus( HWND hWndLosingFocus ) {}
void WWindow::OnKillFocus( HWND hWndGaininFocus ) {}
void WWindow::OnSize( DWORD Flags, INT NewX, INT NewY ) {}
void WWindow::OnWindowPosChanging( INT* NewX, INT* NewY, INT* NewWidth, INT* NewHeight ) {}
void WWindow::OnMove( INT NewX, INT NewY ) {}
void WWindow::OnCommand( INT Command ) {}
INT WWindow::OnSysCommand( INT Command ) { return 0; }
void WWindow::OnActivate( UBOOL Active ) { VerifyPosition(); }
void WWindow::OnChar( TCHAR Ch ) {}
void WWindow::OnKeyDown( TCHAR Ch ) {}
void WWindow::OnCut() {}
void WWindow::OnCopy() {}
void WWindow::OnPaste() {}
void WWindow::OnShowWindow( UBOOL bShow ) {}
void WWindow::OnUndo() {}
UBOOL WWindow::OnEraseBkgnd() { return 0; }
void WWindow::OnVScroll( WPARAM wParam, LPARAM lParam ) {}
void WWindow::OnHScroll( WPARAM wParam, LPARAM lParam ) {}
void WWindow::OnKeyUp( WPARAM wParam, LPARAM lParam ) {}
void WWindow::OnPaint() {}
void WWindow::OnCreate() {}
void WWindow::OnDrawItem( DRAWITEMSTRUCT* Info ) {}
void WWindow::OnMeasureItem( MEASUREITEMSTRUCT* Info ) {}
void WWindow::OnInitDialog() {}
void WWindow::OnEnterIdle() {}
void WWindow::OnMouseEnter() {}
void WWindow::OnMouseLeave() {}
void WWindow::OnMouseHover() {}
void WWindow::OnTimer() {}
void WWindow::OnReleaseCapture() {}
void WWindow::OnMdiActivate( UBOOL Active ) {}
void WWindow::OnMouseMove( DWORD Flags, FPoint Location ) {}
void WWindow::OnLeftButtonDown() {}
void WWindow::OnLeftButtonDoubleClick() {}
void WWindow::OnMiddleButtonDoubleClick() {}
void WWindow::OnRightButtonDoubleClick() {}
void WWindow::OnRightButtonDown() {}
void WWindow::OnLeftButtonUp() {}
void WWindow::OnRightButtonUp() {}
void WWindow::OnFinishSplitterDrag( WDragInterceptor* Drag, UBOOL Success ) {}
INT WWindow::OnSetCursor() { return 0; }

void WWindow::OnClose()
{
	if( MdiChild )
	{
		SendMessage( OwnerWindow->hWnd, WM_MDIDESTROY, (WPARAM)hWnd, 0 );
	}
	else if( hWnd )
	{
		::DestroyWindow( hWnd );
		hWnd = NULL;
	}
}

void WWindow::OnDestroy()
{
	check(hWnd);
	if( PersistentName!=NAME_None )
	{
		FRect R = GetWindowRect();
		if( !IsZoomed(hWnd) )
			GConfig->SetString( TEXT("WindowPositions"), *PersistentName, *FString::Printf( TEXT("(X=%i,Y=%i,XL=%i,YL=%i)"), R.Min.X, R.Min.Y, R.Width(), R.Height() ), GEditorIni );
	}
	_Windows.RemoveItem( this );
	hWnd = NULL;
}

void WWindow::SaveWindowPos()
{
}

void WWindow::MaybeDestroy()
{
	if( !Destroyed )
	{
		Destroyed=1;
		DoDestroy();
	}
}

void WWindow::_CloseWindow()
{
	check(hWnd);
	DestroyWindow( *this );
}

void WWindow::SetFont( HFONT hFont )
{
	SendMessageX( *this, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(0,0) );
}
void WWindow::PerformCreateWindowEx( DWORD dwExStyle, LPCTSTR lpWindowName, DWORD dwStyle, INT x, INT y, INT nWidth, INT nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance )
{
	check(hWnd==NULL);

	// Retrieve remembered position.
	FString Pos;
	if
	(	PersistentName!=NAME_None
	&&	GConfig->GetString( TEXT("WindowPositions"), *PersistentName, Pos, GEditorIni ) )
	{
		// Get saved position.
		Parse( *Pos, TEXT("X="), x );
		Parse( *Pos, TEXT("Y="), y );
		if( (dwStyle&WS_SIZEBOX) || (dwStyle&WS_THICKFRAME) )
		{
			Parse( *Pos, TEXT("XL="), nWidth );
			Parse( *Pos, TEXT("YL="), nHeight );
		}

		// Count identical windows already opened.
		INT Count=0;
		for( INT i=0; i<_Windows.Num(); i++ )
		{
			Count += _Windows(i)->PersistentName==PersistentName;
		}
		if( Count )
		{
			// Move away.
			x += Count*16;
			y += Count*16;
		}

		VerifyPosition();
	}

	// Create window.
	_Windows.AddItem( this );
	TCHAR ClassName[256];
	GetWindowClassName( ClassName );
	//hinstance must match window class hinstance!!
	HWND hWndCreated = TCHAR_CALL_OS(CreateWindowEx(dwExStyle,ClassName,lpWindowName,dwStyle,x,y,nWidth,nHeight,hWndParent,hMenu,hInstanceWindow,this),CreateWindowExA(dwExStyle,TCHAR_TO_ANSI(ClassName),TCHAR_TO_ANSI(lpWindowName),dwStyle,x,y,nWidth,nHeight,hWndParent,hMenu,hInstanceWindow,this));
	if( !hWndCreated )
		appErrorf( TEXT("CreateWindowEx failed: %s"), appGetSystemErrorMessage() );
	check(hWndCreated);
	check(hWndCreated==hWnd);
}

// Makes sure the window is on the screen.  If it's off the screen, it moves it to the top/left edges.
void WWindow::VerifyPosition()
{
	RECT winrect;
	::GetWindowRect( hWnd, &winrect );
	RECT screenrect;
	screenrect.left = ::GetSystemMetrics( SM_XVIRTUALSCREEN );
	screenrect.top = ::GetSystemMetrics( SM_YVIRTUALSCREEN );
	screenrect.right = ::GetSystemMetrics( SM_CXVIRTUALSCREEN );
	screenrect.bottom = ::GetSystemMetrics( SM_CYVIRTUALSCREEN );

	if( winrect.left >= screenrect.right+4 || winrect.left < screenrect.left-4 )
		winrect.left = 0;
	if( winrect.top >= screenrect.bottom+4 || winrect.top < screenrect.top-4 )
		winrect.top = 0;

	::SetWindowPos( hWnd, HWND_TOP, winrect.left, winrect.top, winrect.right, winrect.bottom, SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOSIZE );
}

void WWindow::SetRedraw( UBOOL Redraw )
{
	SendMessageX( *this, WM_SETREDRAW, Redraw, 0 );
}

// Used in the editor ... used to draw window edges in custom colors.
void WWindow::MyDrawEdge( HDC hdc, LPRECT qrc, UBOOL bRaised )
{
	HPEN penOld, penRaised = CreatePen( PS_SOLID, 1, RGB(159,159,159) ),
		penSunken = CreatePen( PS_SOLID, 1, RGB(106,106,106) );
	HDC	hDC = GetDC( hWnd );

	RECT rc = *qrc;
	rc.right -= 1;
	rc.bottom -= 1;

	penOld = (HPEN)SelectObject( hDC, (bRaised ? penRaised : penSunken ) );
	::MoveToEx( hDC, rc.left, rc.top, NULL );
	::LineTo( hDC, rc.right, rc.top );
	::MoveToEx( hDC, rc.left, rc.top, NULL );
	::LineTo( hDC, rc.left, rc.bottom);
	SelectObject( hDC, penOld );

	penOld = (HPEN)SelectObject( hDC, (bRaised ? penSunken : penRaised ) );
	::MoveToEx( hDC, rc.right, rc.bottom, NULL );
	::LineTo( hDC, rc.right, rc.top );
	::MoveToEx( hDC, rc.right, rc.bottom, NULL );
	::LineTo( hDC, rc.left, rc.bottom );
	SelectObject( hDC, penOld );

	DeleteObject( penRaised );
	DeleteObject( penSunken );
	::ReleaseDC( hWnd, hDC );

}

/*-----------------------------------------------------------------------------
	WControl.
-----------------------------------------------------------------------------*/

#if WIN_OBJ
void WControl::Destroy()
{
	Super::Destroy();
	check(OwnerWindow);
	OwnerWindow->Controls.RemoveItem( this );
}
#else
WControl::~WControl()
{
	if( OwnerWindow )
		OwnerWindow->Controls.RemoveItem( this );
}
#endif

// WWindow interface.
INT WControl::CallDefaultProc( UINT Message, UINT wParam, LONG lParam )
{
	if( ::IsWindow( hWnd ) )
		return CallWindowProcX( WindowDefWndProc, hWnd, Message, wParam, lParam );
	else
		return 1;
}
WNDPROC WControl::RegisterWindowClass( const TCHAR* Name, const TCHAR* WinBaseClass )
{
	WNDPROC SuperProc=NULL;
#if UNICODE
	if( GUnicodeOS )
	{
		WNDCLASSEXW Cls;
		appMemzero( &Cls, sizeof(Cls) );
		Cls.cbSize        = sizeof(Cls);
		verify( GetClassInfoExW( NULL, WinBaseClass, &Cls ) );
		SuperProc         = Cls.lpfnWndProc;
		Cls.lpfnWndProc   = WWindow::StaticWndProc;
		Cls.lpszClassName = Name;
		Cls.hInstance     = hInstanceWindow;
		check(Cls.lpszMenuName==NULL);
		verify(RegisterClassExW( &Cls ));
	}
	else
#endif
	{
		WNDCLASSEXA Cls;
		appMemzero( &Cls, sizeof(Cls) );
		Cls.cbSize        = sizeof(Cls);
		verify( GetClassInfoExA( NULL, TCHAR_TO_ANSI(WinBaseClass), &Cls ) );
		SuperProc         = Cls.lpfnWndProc;
		Cls.lpfnWndProc   = WWindow::StaticWndProc;
		Cls.lpszClassName = TCHAR_TO_ANSI(Name);
		Cls.hInstance     = hInstanceWindow;
		check(Cls.lpszMenuName==NULL);
		verify(RegisterClassExA( &Cls ));
	}
	return SuperProc;
}

/*-----------------------------------------------------------------------------
	WEditTerminal.
-----------------------------------------------------------------------------*/

void WEditTerminal::OnChar( TCHAR Ch )
{
	if( Ch!=('C'-'@') )
	{
		OwnerTerminal->TypeChar( Ch );
		throw TEXT("NoRoute");
	}
}

void WEditTerminal::OnRightButtonDown()
{
	throw TEXT("NoRoute");
}

void WEditTerminal::OnPaste()
{
	OwnerTerminal->Paste();
	throw TEXT("NoRoute");
}

void WEditTerminal::OnUndo()
{
	throw TEXT("NoRoute");
}

/*-----------------------------------------------------------------------------
	WTerminal.
-----------------------------------------------------------------------------*/

void WTerminal::Serialize( const TCHAR* Data, EName MsgType )
{
	if( MsgType==NAME_Title )
	{
		SetText( Data );
		return;
	}
	else if( Shown )
	{
		Display.SetRedraw( 0 );
		INT LineCount = Display.GetLineCount();
		if( LineCount > MaxLines )
		{
			INT NewLineCount = Max(LineCount-SlackLines,0);
			INT Index = Display.GetLineIndex( LineCount-NewLineCount );
			Display.SetSelection( 0, Index );
			Display.SetSelectedText( TEXT("") );
			INT Length = Display.GetLength();
			Display.SetSelection( Length, Length );
			Display.ScrollCaret();
		}
		TCHAR Temp[1024]=TEXT("");
		appStrncat( Temp, *FName(MsgType), ARRAY_COUNT(Temp) );
		appStrncat( Temp, TEXT(": "), ARRAY_COUNT(Temp) );
		appStrncat( Temp, (TCHAR*)Data, ARRAY_COUNT(Temp) );
		appStrncat( Temp, TEXT("\r\n"), ARRAY_COUNT(Temp) );
		appStrncat( Temp, Typing, ARRAY_COUNT(Temp) );
		Temp[ARRAY_COUNT(Temp)-1] = 0;
		SelectTyping();
		Display.SetRedraw( 1 );
		Display.SetSelectedText( Temp );
	}
}

void WTerminal::OnShowWindow( UBOOL bShow )
{
	WWindow::OnShowWindow( bShow );
	Shown = bShow;
}

void WTerminal::OnCreate()
{
	WWindow::OnCreate();
	Display.OpenWindow( 1, 1, 1 );

	Display.SetFont( (HFONT)GetStockObject(DEFAULT_GUI_FONT) );
	Display.SetText( Typing );
}

void WTerminal::OpenWindow( UBOOL bMdi, UBOOL AppWindow, UBOOL InChild )
{
	MdiChild = bMdi;

	// Extended style

	DWORD ExStyle = (AppWindow?WS_EX_APPWINDOW:0);
	if( bMdi )
		ExStyle = WS_EX_MDICHILD;
	else
		if( InChild )
			ExStyle = 0;

	// Style

	DWORD Style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX;
	if( bMdi )
		Style = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
	else
		if( InChild )
			Style = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	// Create the window

	PerformCreateWindowEx
	(
		ExStyle,
		*FString::Printf( *LocalizeGeneral("LogWindow",TEXT("Window")), *LocalizeGeneral("Product",TEXT("Core")) ),
		Style,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		512,
		256,
		OwnerWindow ? OwnerWindow->hWnd : NULL,
		NULL,
		hInstance
	);
}

void WTerminal::OnSetFocus( HWND hWndLoser )
{
	WWindow::OnSetFocus( hWndLoser );
	SetFocus( Display );
}

void WTerminal::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	WWindow::OnSize( Flags, NewX, NewY );
	Display.MoveWindow( FRect(0,0,NewX,NewY), TRUE );
	Display.ScrollCaret();
}

void WTerminal::Paste()
{
	SelectTyping();
	FString Str = appClipboardPaste();
	appStrncat( Typing, *Str, ARRAY_COUNT(Typing) );
	Typing[ARRAY_COUNT(Typing)-1]=0;
	for( INT i=0; Typing[i]; i++ )
		if( Typing[i]<32 || Typing[i]>=127 )
			Typing[i] = 0;
	UpdateTyping();
}

void WTerminal::TypeChar( TCHAR Ch )
{
	SelectTyping();
	INT Length = appStrlen(Typing);
	if( Ch>=32 )
	{
		if( Length<ARRAY_COUNT(Typing)-1 )
		{
			Typing[Length]=Ch;
			Typing[Length+1]=0;
		}
	}
	else if( Ch==13 && Length>1 )
	{
		UpdateTyping();
		Display.SetSelectedText( TEXT("\r\n>") );
		TCHAR Temp[ARRAY_COUNT(Typing)];
		appStrcpy( Temp, Typing+1 );
		appStrcpy( Typing, TEXT(">") );
		if( Exec )
			if( !Exec->Exec( Temp, *GLog ) )
				Log( LocalizeError("Exec",TEXT("Core")) );
		SelectTyping();
	}
	else if( (Ch==8 || Ch==127) && Length>1 )
	{
		Typing[Length-1] = 0;
	}
	else if( Ch==27 )
	{
		appStrcpy( Typing, TEXT(">") );
	}
	UpdateTyping();
	if( Ch==22 )
	{
		Paste();
	}
}

void WTerminal::SelectTyping()
{
	INT Length = Display.GetLength();
	Display.SetSelection( Max<INT>(Length-appStrlen(Typing),0), Length );
}

void WTerminal::UpdateTyping()
{
	Display.SetSelectedText( Typing );
}

void WTerminal::SetExec( FExec* InExec )
{
	Exec = InExec;
}

/*-----------------------------------------------------------------------------
	WDialog.
-----------------------------------------------------------------------------*/

INT WDialog::CallDefaultProc( UINT Message, UINT wParam, LONG lParam )
{
	return 0;
}

INT WDialog::DoModal( HINSTANCE hInst )
{
	check(hWnd==NULL);
	_Windows.AddItem( this );
	ModalCount++;
	INT Result = TCHAR_CALL_OS(DialogBoxParamW(hInst/*!!*/,MAKEINTRESOURCEW(ControlId),OwnerWindow?OwnerWindow->hWnd:NULL,(INT(APIENTRY*)(HWND,UINT,WPARAM,LPARAM))StaticWndProc,(LPARAM)this),DialogBoxParamA(hInst/*!!*/,MAKEINTRESOURCEA(ControlId),OwnerWindow?OwnerWindow->hWnd:NULL,(INT(APIENTRY*)(HWND,UINT,WPARAM,LPARAM))StaticWndProc,(LPARAM)this));
	ModalCount--;
	return Result;
}

void WDialog::OpenChildWindow( INT InControlId, UBOOL Visible )
{
	check(!hWnd);
	_Windows.AddItem( this );
	HWND hWndParent = InControlId ? GetDlgItem(OwnerWindow->hWnd,InControlId) : OwnerWindow ? OwnerWindow->hWnd : NULL;
	HWND hWndCreated = TCHAR_CALL_OS(CreateDialogParamW(hInstanceWindow/*!!*/,MAKEINTRESOURCEW(ControlId),hWndParent,(INT(APIENTRY*)(HWND,UINT,WPARAM,LPARAM))StaticWndProc,(LPARAM)this),CreateDialogParamA(hInstanceWindow/*!!*/,MAKEINTRESOURCEA(ControlId),hWndParent,(INT(APIENTRY*)(HWND,UINT,WPARAM,LPARAM))StaticWndProc,(LPARAM)this));
	check(hWndCreated);
	check(hWndCreated==hWnd);
	Show( Visible );
}

BOOL CALLBACK WDialog::LocalizeTextEnumProc( HWND hInWmd, LPARAM lParam )
{
	FString String;
	TCHAR** Temp = (TCHAR**)lParam;
#if UNICODE
	if( !GUnicodeOS )
	{
		ANSICHAR ACh[1024]="";
		SendMessageA( hInWmd, WM_GETTEXT, ARRAY_COUNT(ACh), (LPARAM)ACh );
		String = FString(ACh);
	}
	else
#endif
	{
		TCHAR Ch[1024]=TEXT("");
		SendMessage( hInWmd, WM_GETTEXT, ARRAY_COUNT(Ch), (LPARAM)Ch );
		String = Ch;
	}
	if( FString(String).Left(4)==TEXT("IDC_") )
		SendMessageLX( hInWmd, WM_SETTEXT, 0, *LineFormat(*Localize(Temp[0],*String,Temp[1])) );
	else if( String==TEXT("IDOK") )
		SendMessageLX( hInWmd, WM_SETTEXT, 0, *LineFormat(*LocalizeGeneral("OkButton",TEXT("Window"))) );
	else if( String==TEXT("IDCANCEL") )
		SendMessageLX( hInWmd, WM_SETTEXT, 0, *LineFormat(*LocalizeGeneral("CancelButton",TEXT("Window"))) );
	SendMessageX( hInWmd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(1,0) );
	return 1;
}

void WDialog::LocalizeText( const TCHAR* Section, const TCHAR* Package )
{
	const TCHAR* Temp[3];
	Temp[0] = Section;
	Temp[1] = Package;
	Temp[2] = (TCHAR*)this;
	EnumChildWindows( *this, LocalizeTextEnumProc, (LPARAM)Temp );
	LocalizeTextEnumProc( hWnd, (LPARAM)Temp );
}

void WDialog::OnInitDialog()
{
	WWindow::OnInitDialog();
	SendMessageX( hWnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(1,0) );
	for( INT i=0; i<Controls.Num(); i++ )
	{
		// Bind all child controls.
		WControl* Control = Controls(i);
		check(!Control->hWnd);
		Control->hWnd = GetDlgItem( *this, Control->ControlId );
		check(Control->hWnd);
		_Windows.AddItem(Control);
		Control->WindowDefWndProc = (WNDPROC)GetWindowLongX( Control->hWnd, GWL_WNDPROC );
		SetWindowLongX( Control->hWnd, GWL_WNDPROC, (LONG)WWindow::StaticWndProc );
		//warning: Don't set GWL_HINSTANCE, it screws up subclassed edit controls in Win95.
	}
	for( INT i=0; i<Controls.Num(); i++ )
	{
		// Send create to all controls.
		Controls(i)->OnCreate();
	}
	TCHAR Temp[256];
	appSprintf( Temp, TEXT("IDDIALOG_%s"), *PersistentName );
	LocalizeText( Temp, GetPackageName() );
}

void WDialog::EndDialog( INT Result )
{
	::EndDialog( hWnd, Result );
}

void WDialog::EndDialogTrue()
{
	EndDialog( 1 );
}

void WDialog::EndDialogFalse()
{
	EndDialog( 0 );
}

void WDialog::CenterInOwnerWindow()
{
	// Center the dialog inside of it's parent window (if it has one)
	if( OwnerWindow )
	{
		FRect rc = GetWindowRect( 0 ),
			rcOwner = OwnerWindow->GetWindowRect();

		int X = ((rcOwner.Width() - rc.Width() ) / 2),
			Y = ((rcOwner.Height() - rc.Height() ) / 2);
		::SetWindowPos( hWnd, HWND_TOP, X, Y, 0, 0, SWP_NOSIZE );
	}
}

void WDialog::Show( UBOOL Show )
{
	WWindow::Show(Show);
	if( Show )
		BringWindowToTop( hWnd );
}

/*-----------------------------------------------------------------------------
	WDragInterceptor.
-----------------------------------------------------------------------------*/

void WDragInterceptor::OpenWindow()
{
	PerformCreateWindowEx( 0, NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, *OwnerWindow, NULL, hInstance );
	OldMouseLocation = OwnerWindow->GetCursorPos();
	DragStart = DragPos;
	SetCapture( *this );
	SetFocus( *this );
	ClipCursor( ClientToScreen(DragClamp-DragPos+OwnerWindow->GetCursorPos()) );
	ToggleDraw( NULL );
}

void WDragInterceptor::ToggleDraw( HDC hInDC )
{
	HDC hDC = hInDC ? hInDC : GetDC(*OwnerWindow);
	HBRUSH OldBrush = (HBRUSH)SelectObject( hDC, hBrushWhite );
	if( DragIndices.X!=INDEX_NONE )
		PatBlt( hDC, DragPos.X, 0, DrawWidth.X, OwnerWindow->GetClientRect().Height(), PATINVERT );
	if( DragIndices.Y!=INDEX_NONE )
		PatBlt( hDC, 0, DragPos.Y, OwnerWindow->GetClientRect().Width(), DrawWidth.Y, PATINVERT );
	if( !hInDC )
		ReleaseDC( hWnd, hDC );
	SelectObject( hDC, OldBrush );

}

void WDragInterceptor::OnKeyDown( TCHAR Ch )
{
	if( Ch==VK_ESCAPE )
	{
		Success = 0;
		ReleaseCapture();
	}
}

void WDragInterceptor::OnMouseMove( DWORD Flags, FPoint MouseLocation )
{
	ToggleDraw( NULL );
	for( INT i=0; i<FPoint::Num(); i++ )
		if( DragIndices(i)!=INDEX_NONE )
			DragPos(i) = Clamp( DragPos(i) + MouseLocation(i) - OldMouseLocation(i), DragClamp.Min(i), DragClamp.Max(i) );
	ToggleDraw( NULL );
	OldMouseLocation = MouseLocation;
}

void WDragInterceptor::OnReleaseCapture()
{
	ClipCursor( NULL );
	ToggleDraw( NULL );
	OwnerWindow->OnFinishSplitterDrag( this, Success );
	DestroyWindow( *this );
}

void WDragInterceptor::OnLeftButtonUp()
{
	ReleaseCapture();
}

/*-----------------------------------------------------------------------------
	WCheckListBox.
-----------------------------------------------------------------------------*/

WCheckListBox::WCheckListBox( WWindow* InOwner, INT InId, WNDPROC InSuperProc )
	: WListBox( InOwner, InId )
{
	check(OwnerWindow);
	hbmOff = (HBITMAP)LoadImageA( hInstanceWindow, MAKEINTRESOURCEA(IDBM_CHECKBOX_OFF), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS );	check(hbmOff);
	hbmOn = (HBITMAP)LoadImageA( hInstanceWindow, MAKEINTRESOURCEA(IDBM_CHECKBOX_ON), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS );	check(hbmOn);
	bOn = 0;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

