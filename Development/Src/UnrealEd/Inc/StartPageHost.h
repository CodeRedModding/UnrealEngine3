/*=============================================================================
	WxStartPageHost.cpp: Wx dockable host window for the Start Page
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __StartPageHost_h__
#define __StartPageHost_h__

class FStartPage;

/**
 * Dockable host start page window for the UDK Start Page
 */
class WxStartPageHost
	: public WxBrowser
{
	DECLARE_DYNAMIC_CLASS( WxStartPageHost );

public:
	
	/** Constructor */
	WxStartPageHost();

	/** Destructor */
	virtual ~WxStartPageHost();


	/**
	 * Forwards the call to our base class to create the window relationship.
	 * Creates any internally used windows after that
	 *
	 * @param DockID the unique id to associate with this dockable window
	 * @param FriendlyName the friendly name to assign to this window
	 * @param Parent the parent of this window (should be a Notebook)
	 */
	virtual void Create( INT DockID, const TCHAR* FriendlyName, wxWindow* Parent );

	/** Called when the browser window is resized */
	void OnSize( wxSizeEvent& In );

	/** Called when the browser window receives focus */
	void OnReceiveFocus( wxFocusEvent& Event );

	/** Called when the start page is activated */
	virtual void Activated();

	/** Returns the key to use when looking up values */
	virtual const TCHAR* GetLocalizationKey() const
	{
		return TEXT( "StartPage_Caption" );
	}

protected:

#if WITH_MANAGED_CODE
	/**
	* Accessor for retrieving a reference to this start page's associated FStartPage object.
	* Useful in cases where you need to call methods in FStartPage and need to ensure that it is the one
	* associated with a specific WxBrowser window.
	*
	* NOTE: Using FStartPage::GetActiveInstance() is preferred to this method.
	*/
	FStartPage* GetStartPageInstance();
#endif

private:

#if WITH_MANAGED_CODE
	/** Start Page */
	TScopedPointer< FStartPage > StartPage;
#endif

	DECLARE_EVENT_TABLE();
};


#endif // __StartPageHost_h__
