/*=============================================================================
	Docking.h: Classes to support window docking

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "TrackableWindow.h"

// Forward declarations

class WxFloatingFrame;
class WxBrowser;
class WxDockingContainer;
class FDockingManager;

/**
* This enum lists the types of docking events that may occur
*/
enum EDockingChangeType
{
	DCT_Docking,
	DCT_Floating,
	DCT_Clone,
	DCT_Remove
};

/**
* A wxEvent that gets generated when a WxDockableWindow is changing its docked/floating status.
*/
class WxDockEvent : public wxEvent
{
public:
	WxDockEvent(INT InDockID, EDockingChangeType InChangeType)
		:	DockID( InDockID ), ChangeType(InChangeType)
	{}

	/** Returns a heap-allocated copy of this event.  Clients assume ownership of the cloned copy. */
	wxEvent* Clone() const { return new WxDockEvent( *this ); }

	/** Returns the DockID of the WxDockableWindow that generated this WxDockEvent. */
	INT GetDockID() const { return DockID; }

	/**
	* Returns the type of event this is
	*/
	EDockingChangeType GetDockingChangeType(void) const
	{
		return ChangeType;
	}

protected:
	/**
	* The type of event that needs response for
	*/
	EDockingChangeType ChangeType;
	/** DockID of the WxDockableWindow that generated this WxDockEvent. */
	INT DockID;
};

// use wxWidgets as a DLL
BEGIN_DECLARE_EVENT_TYPES()
DECLARE_LOCAL_EVENT_TYPE(wxEVT_DOCKINGCHANGE, 0)
END_DECLARE_EVENT_TYPES()

typedef void (wxEvtHandler::*WxDockEventFunction)(WxDockEvent&);

#define EVT_DOCKINGCHANGE(fn) \
	DECLARE_EVENT_TABLE_ENTRY(wxEVT_DOCKINGCHANGE, \
	wxID_ANY, wxID_ANY, (wxObjectEventFunction) \
	(wxEventFunction) wxStaticCastEvent( \
	WxDockEventFunction, &fn), (wxObject *) NULL),

/**
 * Any floating windows will reside inside of one of these frames.
 */
class WxFloatingFrame : public wxFrame
{
	DECLARE_DYNAMIC_CLASS(WxFloatingFrame)

public:
	WxFloatingFrame(){ };
	WxFloatingFrame( wxWindow* parent, wxWindowID id, const FString& InDockName );
	virtual ~WxFloatingFrame();

	void OnSize( wxSizeEvent& In );

	// Retrieve whether the floating frame is in a closed state. I.e. Hidden!
	virtual UBOOL IsClosed()
	{
		return bClosed;
	}

	////////////////////////////////
	// Child window -- the window this floating frame contains.
	/** Sets the child window.  InChildWindow must be a valid pointer. */
	
	void SetChildWindow(WxBrowser* InChildWindow);
	
	WxBrowser* GetChildWindow() const
	{
		return ChildWindow;
	}

protected:
	/** The name of the window that this frame is being created for.  This is saved just so it's pos/size can restored. */
	FString DockName;

	/** The window that this floating frame contains.  This window takes up the entire client area. */
	WxBrowser* ChildWindow;

	void OnClose( wxCloseEvent& In );
	void OnShow( wxShowEvent& In );
	void OnIconize( wxIconizeEvent& In );
	UBOOL bClosed;

private:
	///////////////////////////////////
	// Wx events.

	void OnCommand( wxCommandEvent& In );

	void UI_DockWindow( wxUpdateUIEvent& In );
	void UI_FloatWindow( wxUpdateUIEvent& In );


	DECLARE_EVENT_TABLE();
};

// -----------------------------------------------------------------------------
	/** Indicates the state of a dockable window. */
enum EBrowserDockState
{
	/** Docked inside of the docking container. */
	BDS_Docked,
	/** Floating in a frame. */
	BDS_Floating
};

/**
 * The base class for all browser windows.
 */
class WxBrowser :
	public WxTrackableWindow,
	// Interface for event handling
	public FCallbackEventDevice
{
	// Used for dynamic creation of the window. This must be declared for any
	// subclasses of WxBrowser
	DECLARE_DYNAMIC_CLASS(WxBrowser);

protected:
	/**
	 * Whether this class has initialized its windows or not
	 */
	UBOOL bAreWindowsInitialized;
	/**
	 * Uniquely identifies this browser
	 */
	INT DockID;
	/**
	 * Friendly name that is used for saving state and displaying in the tabs.
	 */
	FString DockingName;
	/**
	 * The docking state this window is currently in.
	 */
	EBrowserDockState DockState;
	/**
	 * Whether this window is visible or not. This is persistent state that is
	 * loaded/saved.
	 */
	UBOOL bIsVisible;
	/**
	 * This is the localized title to use when docked in a tab control
	 */
	FString LocalizedCaption;
	/**
	 * A panel that was loaded for the browser
	 */
	wxPanel* Panel;
	/**
	 * The set of menus for this browser
	 */
	wxMenuBar* MenuBar;

	/**
	 * Gets the frame window that holds/will hold the menus
	 */
	inline wxFrame* GetFrame()
	{
		wxFrame* Frame = (wxFrame*)GetParent();
		if (IsDocked())
		{
			Frame = (wxFrame*)Frame->GetParent();
		}
		return Frame;
	}

public:
	/** Default constructor */
	WxBrowser();

	/**
	 * Destructor. Clears out *all* registered events
	 */
	virtual ~WxBrowser()
	{
		GCallbackEvent->UnregisterAll(this);
	}

	/**
	 * This function is called when the window has been selected from within the ctrl + tab dialog.
	 */
	virtual void OnSelected();

	/** Called when the browser is first constructed.  Gives it a chance to fill controls, lists and such. */
	virtual void InitialUpdate();

	virtual void Update();

	/** Called when the browser is getting activated (becoming the visible window in it's dockable frame). */
	virtual void Activated();

	virtual void PostCreateInit();

	/**
	 * Adds entries to the browser's accelerator key table.  Derived classes should call up to their parents.
	 */
	virtual void AddAcceleratorTableEntries(TArray<wxAcceleratorEntry>& Entries);

	// FCallbackEventDevice interface

	/**
	 * Refreshes the browser. Note this will only be called if a child class
	 * registers an event type
	 *
	 * @param InType the event that was fired
	 */
	virtual void Send(ECallbackEventType InType)
	{
		Update();
	}

	// End FCallbackEventDevice interface

	virtual void SetParentCaption();

	/**
	 * Part 2 of the 2 phase creation process. Sets the pane id, friendly name,
	 * and the parent window pointers. Forwards the call to wxWindow
	 *
	 * @param DockID the unique id to associate with this dockable window
	 * @param FriendlyName the friendly name to assign to this window
	 * @param Parent the parent of this window (should be a Notebook)
	 */
	virtual void Create(INT DockID,const TCHAR* FriendlyName,wxWindow* Parent);

	/**
	 * Tells the browser manager whether or not this browser can be cloned
	 */
	virtual UBOOL IsClonable()
	{
		return TRUE;
	}

	/**
	 * Returns the docking id for this window
	 */
	INT GetDockID() const
	{
		return DockID;
	}

	/**
	 * Returns true if the window is docked, false otherwise
	 */
	UBOOL IsDocked() const
	{
		return DockState == BDS_Docked;
	}

	/**
	 * Returns true if the window is floating, false otherwise
	 */
	UBOOL IsFloating() const
	{
		return DockState == BDS_Floating;
	}

	/**
	 * Sets current docking state to docked
	 */
	void SetDockStateToDocked()
	{
		DockState = BDS_Docked;
	}

	/**
	 * Sets current docking state to floating
	 */
	void SetDockStateToFloating()
	{ 
		DockState = BDS_Floating; 
	}

	/**
	 * Sets the "friendly" name of the dockable window
	 *
	 * @param InName the new name to assign to this window
	 */
	void SetDockingName(const TCHAR* InName)
	{
		DockingName = InName;
	}

	/**
	 * Returns the "friendly" name of the dockable window
	 */
	const TCHAR* GetDockingName() const
	{
		return *DockingName;
	}

	/**
	 * Sets the visible window state for loading/saving to the INI file
	 */
	void SetVisibleState(UBOOL InbIsVisible)
	{
		bIsVisible = InbIsVisible;
	}

	/**
	 * Returns whether or not this window should be visible
	 */
	UBOOL IsVisible() const
	{
		return bIsVisible;
	}

	/**
	 * Returns the localized caption for this window
	 */
	const TCHAR* GetLocalizedCaption() const
	{
		return *LocalizedCaption;
	}

	/**
	 * Sets the localized caption for this window. Done when this is a clone
	 * of an existing window to append the intance number
	 *
	 * @param NewCaption the string to use as the caption
	 */
	void SetLocalizedCaption(const TCHAR* NewCaption)
	{
		LocalizedCaption = NewCaption;
	}

	/**
	 * Returns the key to use when looking up values
	 */
	virtual const TCHAR* GetLocalizationKey() const
	{
		check(0 && "Please override as this is pure virtual");
		return TEXT("");
	}

	/**
	 * Loads the window state for this dockable window
	 *
	 * @param bInInitiallyHidden True if the window should be hidden if we have no saved state
	 */
	virtual void LoadWindowState( const UBOOL bInInitiallyHidden );

	/**
	 * Saves the window state for this dockable window
	 */
	virtual void SaveWindowState();

	/**
	 * Lets the browser detach any menus when changing docking state
	 */
	void DetachMenus();

	/**
	 * Attaches the menus to the correct frame window
	 */
	void AttachMenus();

	/**
	 * Builds a docking menu and appends it to the menu passed in
	 *
	 * @param InMenu the menu to add the hand built one to
	 */
	static void AddDockingMenu(wxMenuBar* InMenu);

protected:
	/** Temporary used to time browser updates. */
	DOUBLE	UpdateStartTime;

	/** Called by derived classes' Update() to mark the beginning of an update timing period. */
	void BeginUpdate();

	/** Called by derived classes' Update() to mark the end of an update timing period. */
	void EndUpdate();

private:

	/**
	 * Assembles an accelerator key table for the browser by calling down to AddAcceleratorTableEntries.
	 */
	void SetBrowserAcceleratorTable(wxFrame* Frame);

	////////////////////
	// Wx events.

	/** Handler for EVT_SIZE events. */
	void OnSize( wxSizeEvent& In );
	/** Handler for IDM_FloatWindow menu events. */
	void OnFloatWindow( wxCommandEvent& In );
	/** Handler for IDM_DockWindow menu events. */
	void OnDockWindow( wxCommandEvent& In );

	/** Handler for IDM_CloneBrowser menu events. */
	void OnClone( wxCommandEvent& In );
	/** Handler for IDM_RemoveBrowser menu events. */
	void OnRemove( wxCommandEvent& In );

	/**
	 * Default handler for IDM_RefreshBrowser events.  Derived classes
	 * override this function to handle browser refreshes.
	 */
	void OnRefresh( wxCommandEvent& In );

	DECLARE_EVENT_TABLE();
};

// -----------------------------------------------------------------------------

/**
 * This is the docking container.  If a window is docked, it is part of this windows list of tabs.
 */
class WxDockingContainer : public WxFloatingFrame
{
	DECLARE_DYNAMIC_CLASS(WxDockingContainer)

public:
	WxDockingContainer(){ };
	WxDockingContainer( wxWindow* InParent, wxWindowID InID );
	~WxDockingContainer();

	void OnSize( wxSizeEvent& In );
	void OnActivate(wxActivateEvent& Event);

	///////////////////////////////////
	// wxNotebook control.  Returned pointers are always valid.

	const wxNotebook* GetNotebook() const
	{
		return Notebook;
	}

	wxNotebook* GetNotebook()
	{
		return Notebook;
	}

protected:
	/** One tab for every docked window. This control takes up the entire client area of this window. */
	wxNotebook* Notebook;

private:
	///////////////////////////////////
	// Wx events.

	/**
	 *  The docked page in the browser is about to switch 
	 *
	 *  @param In	The switch event
	 */
	void OnPageAboutToChange( wxNotebookEvent& In);
	void OnPageChanged(	wxNotebookEvent& In );
	void OnCommand( wxCommandEvent& In );

	void OnClose( wxCloseEvent& In );
	void UI_DockWindow( wxUpdateUIEvent& In );
	void UI_FloatWindow( wxUpdateUIEvent& In );

	/**
	 * Determines whether this menu item can be selected or not. Docked clones
	 * are removed. Docked originals aren't affected. Floating originals are
	 * hidden. Floating clones are removable.
	 *
	 * @param Event the update event to modify
	 */
	void UI_RemoveWindow(wxUpdateUIEvent& Event);

	/**
	 * Determines whether this menu item can be selected or not. It only allows
	 * the number of panes that matches the number of available menu items
	 *
	 * @param Event the update event to modify
	 */
	void UI_CloneWindow(wxUpdateUIEvent& Event);

	/** The number of docked items */
	INT NumberDockedTabs;

	DECLARE_EVENT_TABLE();
};
