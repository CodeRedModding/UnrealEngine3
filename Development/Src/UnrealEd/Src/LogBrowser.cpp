/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "LogBrowser.h"
#include "Log.h"

BEGIN_EVENT_TABLE( WxLogBrowser, WxBrowser )
	EVT_SIZE( WxLogBrowser::OnSize )
END_EVENT_TABLE()

WxLogBrowser::WxLogBrowser(void)
{
}

/**
 * Forwards the call to our base class to create the window relationship.
 * Creates any internally used windows after that
 *
 * @param DockID the unique id to associate with this dockable window
 * @param FriendlyName the friendly name to assign to this window
 * @param Parent the parent of this window (should be a Notebook)
 */
void WxLogBrowser::Create(INT DockID,const TCHAR* FriendlyName,
	wxWindow* Parent)
{
	// Let our base class start up the windows
	WxBrowser::Create(DockID,FriendlyName,Parent);
	wxRect rc = GetClientRect();

	LogWindow = new WxLogWindow( this );
	rc.Deflate( 8 );
	LogWindow->SetSize( rc );

	// Add a menu bar
	MenuBar = new wxMenuBar();
	// Append the docking menu choices
	WxBrowser::AddDockingMenu(MenuBar);

	Update();

	WxRichTextEditControl::FScopedBatchLogHelper BeginBatchLog(LogWindow->RichTextWindow);
	GLog->SerializeBacklog( LogWindow );
}

void WxLogBrowser::OnSize( wxSizeEvent& In )
{
	// During the creation process a sizing message can be sent so don't
	// handle it until we are initialized
	if (bAreWindowsInitialized)
	{
		wxRect rc = GetClientRect();

		rc.Deflate( 8 );
		LogWindow->SetSize( rc );
	}
}
