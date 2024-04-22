/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DOCKINGPARENT_H__
#define __DOCKINGPARENT_H__


class FDockingParent;


/**
 * WxEventCatchingAuiManager
 *
 * A simple wrapper class around wxAuiManager that allows us to intercept events from AUI.
 * Normally we wouldn't need this, but AuiManager doesn't respect custom event handlers applied
 * to either the manager itself or the associated frame window.  Boo!
 */
class WxEventCatchingAuiManager
	: public wxAuiManager
{
	DECLARE_DYNAMIC_CLASS( WxEventCatchingAuiManager );
    DECLARE_EVENT_TABLE()

public:

	/** Constructor */
	WxEventCatchingAuiManager( wxWindow* managed_wnd = NULL, unsigned int flags = wxAUI_MGR_DEFAULT )
		: wxAuiManager( managed_wnd, flags ),
		  CustomEventHandler( NULL )
	{
	}


	/** Destructor */
	virtual ~WxEventCatchingAuiManager()
	{
	}


	/**
	 * Sets the custom event handler to use.  You should pass NULL here if your event handler object is destroyed.
	 *
	 * @param NewEventHandler The new event handler object to use
	 */
	void SetCustomEventHandler( wxEvtHandler* NewEventHandler )
	{
		CustomEventHandler = NewEventHandler;
	}


protected:

	/**
	 * Called when an event is received.  Forwards the event to the custom event handler.
	 */
	void OnEventReceived( wxAuiManagerEvent& Event );


private:

	/** Custom event handler */
	wxEvtHandler* CustomEventHandler;
};



/**
 * WxDockableWindowMenuHandler
 *
 * Implements support for managing a 'Window' menu in a menu bar that updates automatically in response
 * to dockable windows being opened, closed, docked and floated.  In general, this serves as a tool for
 * the user to unhide floating windows that have been hidden (by pressing the close button.)
 */
class WxDockableWindowMenuHandler
	: public wxEvtHandler
{
	DECLARE_DYNAMIC_CLASS( WxDockableWindowMenuHandler );
    DECLARE_EVENT_TABLE()

public:

	/** Constructor */
	WxDockableWindowMenuHandler();

	/** Destructor */
	virtual ~WxDockableWindowMenuHandler();

	/**
	 * Sets the owner of this object.  Must be called to initialize the object.
	 *
	 * @param NewOwner The new owner for this object
	 */
	void SetOwner( FDockingParent* NewOwner );

	/**
	 * Sets the 'Window' menu that we should be tracking
	 *
	 * @param NewWindowMenu The new 'Window' menu to track
	 */
	void SetWindowMenu( wxMenu* NewWindowMenu );

	/** Refreshes all menu items.  Should be called after a docking window is added, removed, shown or hidden. */
	void RefreshMenuItems();

	/** Clear window menu */
	void ClearWindowList();

public:

	/** Wx event: Called when a menu item is interacted with */
    void OnMenu( wxCommandEvent& Event );

	/** Wx AUI event: Called when a dockable window is closed */
    void OnPaneClosed( wxAuiManagerEvent& Event );


private:

	/** Pointer to the FDockingParent that owns this object */
	FDockingParent* Owner;

	/** 'Window' menu on the menu bar.  If you set this, DockingParent will automatically manage the contents
		of this menu using the docked/floating state of all dockable windows (AUI mode only) */
    wxMenu* WindowMenu;

};



/**
 *	Abstract class that encapsulates wxWidgets docking functionality.  Should be inherited by frames that wish to have dockable children.
 */
class FDockingParent
{
public:
	enum EDockHost
	{
		DH_None,
		DH_Top,
		DH_Bottom,
		DH_Left,
		DH_Right
	};


	/** State of a dockable window */
	struct FDockWindowState
	{
		/** True if window is currently visible.  Windows that are closed when the user clicks on 'X' are no
		    longer visible. */
		UBOOL bIsVisible;

		/** The window's current 'docked' state. */
		UBOOL bIsDocked;
	};


	/** Constructor */
	FDockingParent( wxFrame* InParent );

	/** Destructor */
	virtual ~FDockingParent();

	/**
	 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
	 *  @return A string representing a name to use for this docking parent.
	 */
	virtual const TCHAR* GetDockingParentName() const = 0;

	/**
	 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
	 */
	virtual const INT GetDockingParentVersion() const = 0;

	/**
	 * Loads the docking window layout from an XML file.  This should be called after all docking windows have been added.
	 * This function checks to see if the docking parent version number stored in the INI file is less than the current version number.
	 * If it is, it will not load the settings and let the new defaults be loaded instead.
	 * @return Whether or not a layout was loaded, if a layout was NOT loaded, the calling function should set default docking window positions.
	 */
	UBOOL LoadDockingLayout();

	/**
	 * Saves the current docking layout to an XML file.  This should be called in the destructor of the subclass.
	 */
	void SaveDockingLayout();

	/**
	 * Creates a new docking window using the parameters passed in. 
	 *
	 * @param ClientWindow	The client window for this docking window, most likely a wxPanel with some controls on it.
	 * @param DockHost		The dock host that the new docking widget should be attached to.
	 * @param WindowTitle	The title for this window.
	 * @param WindowName	Unique identifying name for this dock window, should be set when the window title is auto generated.
	 * @param BestSize		Optional parameter specifying the best size for this docking window
	 */
	void AddDockingWindow( wxWindow* ClientWindow, EDockHost DockHost, const TCHAR* WindowTitle, const TCHAR* WindowName = NULL, wxSize BestSize = wxSize(300, 240) );


	/**
	 * Makes sure the specified client window is no longer associated with a dock window
	 *
	 * @param ClientWindow Window to disassociate
	 */
	void UnbindDockingWindow( wxWindow* ClientWindow );


	/**
	 * Appends toggle items to the menu passed in that control visibility of all of the docking windows registered with the layout manager.
	 *
	 * @param MenuBar	wxMenuBar to append the docking menu to.
	 *
	 * @return Returns new Window menu
	 */
	wxMenu* AppendWindowMenu( wxMenuBar* MenuBar );

	/**
	 * Sets the default area size of a dock host.
	 * @param Size	New Size for the dock host.
	 */
	void SetDockHostSize(EDockHost DockHost, INT Size);


	/**
	 * Changes the title of an existing docking window
	 *
	 * @param InWindow The window associated with the pane that you'd like to change the title of
	 * @param InNewTitle The new window title
	 */
	void SetDockingWindowTitle( wxWindow* InWindow, const TCHAR* InNewTitle );


	/**
	 * Queries the state of a specific dockable window
	 *
	 * @param InWindow The window you're interested in querying
	 * @param OutState [Out] Dockable window state
	 *
	 * @return TRUE if the specified window was found and state was returned
	 */
	UBOOL GetDockingWindowState( wxWindow* InWindow, FDockingParent::FDockWindowState& OutState );


	/**
	 * Shows or hides a dockable window (along with it's associated pane)
	 *
	 * @param Window The window you're interested in showing or hiding
	 * @param bInShow TRUE to show the window or FALSE to hide it
	 */
	void ShowDockingWindow( wxWindow* Window, UBOOL bInShow );


	/** Call this to apply any changes you've made to docked windows */
	void UpdateUIForDockingChanges();


	/** Access the AUIManager object directly.  Don't call this method unless you know what you're doing! */
	wxAuiManager& AccessAUIManager()
	{
		return AUIManager;
	}

	void ClearWindowList()
	{
		WindowMenuHandler.ClearWindowList();
	}


protected:

	/** Called when a docking window state has changed.  Override this in your derived class!  */
	virtual void OnWindowDockingLayoutChanged()
	{
	}

	/**
	 * Generates a filename using the docking parent name, the game config directory, and the current game name for saving/loading layout files.
	 * @param OutFileName	Storage string for the generated filename.
	 */
	void GetLayoutFileName(FString &OutFileName);

	/** Frame window associated with this docking class. */
	wxFrame* Parent;


private:

	/** Wx AUI Manager object that's used to manage docked windows for this frame */
	WxEventCatchingAuiManager AUIManager;

	/** Handler for 'Window' menu, if we're using that feature (AUI mode only) */
	WxDockableWindowMenuHandler WindowMenuHandler;

};



#endif

