/*=============================================================================
	WxStartPageHost.h: Wx dockable host window for the Start Page
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/


#include "UnrealEd.h"
#include "StartPageHost.h"

#if WITH_MANAGED_CODE
	#include "StartPageShared.h"
#endif


BEGIN_EVENT_TABLE( WxStartPageHost, WxBrowser )
	EVT_SIZE( WxStartPageHost::OnSize )
	EVT_SET_FOCUS( WxStartPageHost::OnReceiveFocus )
END_EVENT_TABLE()


/** Constructor */
WxStartPageHost::WxStartPageHost()
{
}



/** Destructor */
WxStartPageHost::~WxStartPageHost()
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
void WxStartPageHost::Create( INT DockID, const TCHAR* FriendlyName, wxWindow* Parent )
{
	// Call parent implementation
	WxBrowser::Create( DockID, FriendlyName, Parent);

#if WITH_MANAGED_CODE
	HWND BrowserTabHandle = (HWND)this->GetHandle();

	// Create the content browser object!
	FStartPage* NewStartPage =
		FStartPage::CreateStartPage( this, BrowserTabHandle );
	StartPage.Reset( NewStartPage );
#endif


	Update();
}

/** Called when the browser window is resized */
void WxStartPageHost::OnSize( wxSizeEvent& Event )
{
	//debugf(TEXT("Wx; WM_SIZE: (%d, %d)"), In.GetSize().GetWidth() , In.GetSize().GetHeight());

#if WITH_MANAGED_CODE
	if( StartPage != NULL )
	{
		StartPage->Resize(NULL, 0, 0, Event.GetSize().GetWidth(), Event.GetSize().GetHeight());
	}
#endif
	Event.Skip();
}

/** Called when the browser window receives focus */
void WxStartPageHost::OnReceiveFocus( wxFocusEvent& Event )
{
#if WITH_MANAGED_CODE
	if ( StartPage != NULL )
	{
		StartPage->SetFocus();
	}
#endif
	Event.Skip();
}

	/** Called when the start page is activated */
void WxStartPageHost::Activated()
{
	WxBrowser::Activated();

	//I only want to set focus if this is TRULY the window that I should be displaying.  When RestoreBrowserWindows gets called, all windows become activated
#if WITH_MANAGED_CODE
	UBrowserManager* BrowserManager = GUnrealEd->GetBrowserManager();
	check(BrowserManager);
	WxBrowser* CurrentBrowser = BrowserManager->GetBrowserPane(BrowserManager->LastSelectedPaneID);
	//if this is the expected first window OR we're past restoration point
	if ((BrowserManager->bHasStateBeenRestored) || (CurrentBrowser == this))
	{
		StartPage->SetFocus();
	}

#endif
}

#if WITH_MANAGED_CODE
/**
 * Accessor for retrieving a reference to this Start Page window's associated FStartPage object.
 * Useful in cases where you need to call methods in FStartPage and need to ensure that it is the one
 * associated with a specific WxBrowser window.
 */
FStartPage* WxStartPageHost::GetStartPageInstance()
{
	return StartPage;
}
#endif

// EOF


