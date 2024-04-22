/*=============================================================================
	WxContentBrowserHost.cpp: Wx dockable host window for the Content Browser
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ContentBrowserHost_h__
#define __ContentBrowserHost_h__

class FContentBrowser;



/**
 * Dockable host browser window for the Conten Browser
 */
class WxContentBrowserHost
	: public WxBrowser
{
	DECLARE_DYNAMIC_CLASS( WxContentBrowserHost );

public:
	
	/** Constructor */
	WxContentBrowserHost();

	/** Destructor */
	virtual ~WxContentBrowserHost();


	/**
	 * Forwards the call to our base class to create the window relationship.
	 * Creates any internally used windows after that
	 *
	 * @param DockID the unique id to associate with this dockable window
	 * @param FriendlyName the friendly name to assign to this window
	 * @param Parent the parent of this window (should be a Notebook)
	 */
	virtual void Create( INT DockID, const TCHAR* FriendlyName, wxWindow* Parent );

	/** Tells the browser manager whether or not this browser can be cloned */
	virtual UBOOL IsClonable()
	{
		// We support multiple instances
		return TRUE;
	}

	/** Called when the browser window is activated */
	virtual void Activated();


	/** Returns the key to use when looking up values */
	virtual const TCHAR* GetLocalizationKey() const
	{
		return TEXT( "ContentBrowser_Caption" );
	}

	/** Override accelerator table so that ContentBrowser can handle all the keys on its own. */
	virtual void AddAcceleratorTableEntries(TArray<wxAcceleratorEntry>& Entries);

protected:

#if WITH_MANAGED_CODE
	/**
	* Accessor for retrieving a reference to this browser window's associated FContentBrowser object.
	* Useful in cases where you need to call methods in FContentBrowser and need to ensure that it is the one
	* associated with a specific WxBrowser window.
	*
	* NOTE: Using FContentBrowser::GetActiveInstance() is preferred to this method.
	*/
	FContentBrowser* GetContentBrowserInstance();
#endif

	/** Called when the browser window is resized */
	void OnSize( wxSizeEvent& In );

	/** Called when the browser window receives focus */
	void OnReceiveFocus( wxFocusEvent& Event );


private:

#if WITH_MANAGED_CODE
	/** Content Browser */
	TScopedPointer< FContentBrowser > ContentBrowser;
#endif

	DECLARE_EVENT_TABLE();
};


#endif // __ContentBrowserHost_h__
