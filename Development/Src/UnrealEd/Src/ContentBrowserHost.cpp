/*=============================================================================
	WxContentBrowserHost.h: Wx dockable host window for the Content Browser
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/


#include "UnrealEd.h"
#include "ContentBrowserHost.h"

#if WITH_MANAGED_CODE
	#include "ContentBrowserShared.h"
#endif


BEGIN_EVENT_TABLE( WxContentBrowserHost, WxBrowser )
	EVT_SIZE( WxContentBrowserHost::OnSize )
	EVT_SET_FOCUS( WxContentBrowserHost::OnReceiveFocus )
END_EVENT_TABLE()


/** Constructor */
WxContentBrowserHost::WxContentBrowserHost()
{
}



/** Destructor */
WxContentBrowserHost::~WxContentBrowserHost()
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
void WxContentBrowserHost::Create( INT DockID, const TCHAR* FriendlyName, wxWindow* Parent )
{
	// Call parent implementation
	WxBrowser::Create( DockID, FriendlyName, Parent);

#if WITH_MANAGED_CODE
	HWND BrowserTabHandle = (HWND)this->GetHandle();

	// Create the content browser object!
	FContentBrowser* NewContentBrowser =
		FContentBrowser::CreateContentBrowser( this, BrowserTabHandle );
	ContentBrowser.Reset( NewContentBrowser );
#endif


	Update();
}


void WxContentBrowserHost::OnSize( wxSizeEvent& In )
{
	//debugf(TEXT("Wx; WM_SIZE: (%d, %d)"), In.GetSize().GetWidth() , In.GetSize().GetHeight());

#if WITH_MANAGED_CODE
	if( ContentBrowser != NULL )
	{
		ContentBrowser->Resize(NULL, 0, 0, In.GetSize().GetWidth(), In.GetSize().GetHeight());
	}
#endif
}

void WxContentBrowserHost::OnReceiveFocus( wxFocusEvent& Event )
{
#if WITH_MANAGED_CODE
	if ( ContentBrowser != NULL )
	{
		ContentBrowser->SetFocus();
	}
#endif
}

void WxContentBrowserHost::Activated()
{
	WxBrowser::Activated();
#if WITH_MANAGED_CODE
	ContentBrowser->SetFocus();
#endif
}

/**
* Make sure nothing is added to the accelerator table; we want to be able to handle our own keys.
*/
void WxContentBrowserHost::AddAcceleratorTableEntries(TArray<wxAcceleratorEntry>& Entries)
{
	// Add nothing to the accelerator table.
}

#if WITH_MANAGED_CODE
/**
 * Accessor for retrieving a reference to this browser window's associated FContentBrowser object.
 * Useful in cases where you need to call methods in FContentBrowser and need to ensure that it is the one
 * associated with a specific WxBrowser window.
 */
FContentBrowser* WxContentBrowserHost::GetContentBrowserInstance()
{
	return ContentBrowser;
}
#endif


// EOF


