
#include "UnrealEd.h"

// --------------------------------------------------------------
//
// WBrowser
//
// --------------------------------------------------------------

WBrowser::WBrowser( FName InPersistentName, WWindow* InOwnerWindow, HWND InEditorFrame )
	:	WWindow( InPersistentName, InOwnerWindow )
{
	check(InOwnerWindow);
	bDocked = 0;
	MenuID = 0;
	hwndEditorFrame = InEditorFrame;
	Description = TEXT("Browser");
}

// WWindow interface.
void WBrowser::OpenWindow( UBOOL bChild )
{
	MdiChild = 0;

	PerformCreateWindowEx
	(
		0,
		NULL,
		(bChild ? WS_CHILD  : WS_OVERLAPPEDWINDOW) | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		0,
		0,
		320,
		200,
		OwnerWindow ? OwnerWindow->hWnd : NULL,
		NULL,
		hInstance
	);
	bDocked = bChild;
	Show(0);
}
INT WBrowser::OnSysCommand( INT Command )
{
	if( Command == SC_CLOSE )
	{
		Show(0);
		return 1;
	}

	return 0;
}
INT WBrowser::OnSetCursor()
{
	WWindow::OnSetCursor();
	SetCursor(LoadCursorIdX(NULL,IDC_ARROW));
	return 0;
}
void WBrowser::OnCreate()
{
	WWindow::OnCreate();

	// Load windows last position.
	//
	INT X, Y, W, H;

	if(!GConfig->GetInt( *PersistentName, TEXT("X"), X, GEditorIni ))	X = 0;
	if(!GConfig->GetInt( *PersistentName, TEXT("Y"), Y, GEditorIni ))	Y = 0;
	if(!GConfig->GetInt( *PersistentName, TEXT("W"), W, GEditorIni ))	W = 500;
	if(!GConfig->GetInt( *PersistentName, TEXT("H"), H, GEditorIni ))	H = 300;

	if( !W ) W = 320;
	if( !H ) H = 200;

	::MoveWindow( hWnd, X, Y, W, H, TRUE );

}
void WBrowser::SetCaption( FString* Tail )
{
	FString Caption;

	Caption = Description;

	if( Tail && Tail->Len() )
		Caption = *FString::Printf(TEXT("%s - %s"), *Caption, **Tail );

	if( IsDocked() )
		OwnerWindow->SetText( *Caption );
	else
		SetText( *Caption );

}
FString	WBrowser::GetCaption()
{
	return GetText();
}
void WBrowser::UpdateMenu()
{
}
void WBrowser::OnDestroy()
{
	// Save Window position (base class doesn't always do this properly)
	// (Don't do this if the window is minimized.)
	//
	if( !::IsIconic( hWnd ) && !::IsZoomed( hWnd ) )
	{
		RECT R;
		::GetWindowRect(hWnd, &R);

		GConfig->SetInt( *PersistentName, TEXT("Active"), bShow, GEditorIni );
		GConfig->SetInt( *PersistentName, TEXT("Docked"), bDocked, GEditorIni );
		GConfig->SetInt( *PersistentName, TEXT("X"), R.left, GEditorIni );
		GConfig->SetInt( *PersistentName, TEXT("Y"), R.top, GEditorIni );
		GConfig->SetInt( *PersistentName, TEXT("W"), R.right - R.left, GEditorIni );
		GConfig->SetInt( *PersistentName, TEXT("H"), R.bottom - R.top, GEditorIni );
	}

	_DeleteWindows.AddItem( this );

	WWindow::OnDestroy();
}
void WBrowser::OnPaint()
{
	PAINTSTRUCT PS;
	HDC hDC = BeginPaint( *this, &PS );
	FillRect( hDC, GetClientRect(), (HBRUSH)(COLOR_BTNFACE+1) );
	EndPaint( *this, &PS );
}
void WBrowser::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	WWindow::OnSize(Flags, NewX, NewY);
	PositionChildControls();
	InvalidateRect( hWnd, NULL, FALSE );
}
void WBrowser::PositionChildControls( void )
{
}
// Searches a list of filenames and replaces all single NULL's with | characters.  This allows
// the regular parse routine to work correctly.  The return value is the number of NULL's
// that were replaced -- if this is greater than zero, you have multiple filenames.
//
INT WBrowser::FormatFilenames( char* _pchFilenames )
{
	char *pch = _pchFilenames;
	INT l_iNULLs = 0;

	while( true )
	{
		if( *pch == '\0' )
		{
			if( *(pch+1) == '\0') break;

			*pch = '|';
			l_iNULLs++;
		}
		pch++;
	}

	return l_iNULLs;
}
FString WBrowser::GetCurrentPathName( void )
{
	return TEXT("");
}
void WBrowser::UpdateAll()
{
}
void WBrowser::Show( UBOOL Show )
{
	WWindow::Show(Show);
	if( Show )
		BringWindowToTop( hWnd );
}

// Takes a fully pathed filename, and just returns the name.
// i.e. "c:\test\file.txt" gets returned as "file".
//
FString GetFilenameOnly( FString Filename)
{
	FString NewFilename = Filename;

	while( NewFilename.InStr( TEXT("\\") ) != -1 )
		NewFilename = NewFilename.Mid( NewFilename.InStr( TEXT("\\") ) + 1, NewFilename.Len() );

	if( NewFilename.InStr( TEXT(".") ) != -1 )
		NewFilename = NewFilename.Left( NewFilename.InStr( TEXT(".") ) );

	return NewFilename;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
