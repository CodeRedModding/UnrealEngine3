/*=============================================================================
DockingParent.cpp: This file holds the FDockingParent class that should be inherited by classes that wish to become parents for dockable windows
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DockingParent.h"


FDockingParent::FDockingParent( wxFrame* InParent ) : 
Parent(InParent)
{
	check( InParent != NULL );

	// Setup window menu handler
	WindowMenuHandler.SetOwner( this );

	// Initialize AUI manager for this frame
	AUIManager.SetManagedWindow( InParent );

	// @todo AUI: These values don't quite work the way I expected.  Setting these to 1.0 gives us the result we
	//   want, but it seems like we should be using a smaller value here (default is 0.3)
	AUIManager.SetDockSizeConstraint( 1.0, 1.0 );

	// Associate our menu handler with the parent window
	InParent->PushEventHandler( &WindowMenuHandler );

	// NOTE: Due to a bug in the Wx AUI code where custom event handlers are ignored (on both the AuiManager and
	// on associated frame windows), we work around it by routing events through a derived class.
	AUIManager.SetCustomEventHandler( &WindowMenuHandler );
}

FDockingParent::~FDockingParent()
{
	// Disassociate our menu handler from the parent window
	Parent->RemoveEventHandler( &WindowMenuHandler );

	// Disassociate from the AUI manager, too
	AUIManager.SetCustomEventHandler(NULL );

	// Kill AUI manager
	AUIManager.UnInit();
}


/**
 * This define is a global modifier on top of the individual window's layout versions. It can be incremented to 
 * invalidate all docking layout files from a previous version.
 */
#define DOCKING_INTERNAL_VERSION 1

/**
 * Loads the docking window layout from an XML file.  This should be called after all docking windows have been added.
 * This function checks to see if the docking parent version number stored in the INI file is less than the current version number.
 * If it is, it will not load the settings and let the new defaults be loaded instead.
 * @return Whether or not a layout was loaded, if a layout was NOT loaded, the calling function should set default docking window positions.
 */
UBOOL FDockingParent::LoadDockingLayout()
{
	// Check to see if the version of the layout that was previously saved out is outdated.
	INT SavedVersion = -1;
	
	GConfig->GetInt(TEXT("DockingLayoutVersionsAUI"), GetDockingParentName(), SavedVersion, GEditorUserSettingsIni);
	
	const INT CurrentVersion = GetDockingParentVersion() + DOCKING_INTERNAL_VERSION;
	const UBOOL bOldVersion = CurrentVersion > SavedVersion;

	UBOOL bFileLoadedOK = FALSE;

	if( bOldVersion == FALSE)
	{
		// The version is OK to load, go ahead and load the XML file from disk, checking to make sure
		// it loads properly.
		FString FileName;
		GetLayoutFileName(FileName);

		FString LoadedPerspectiveString;
		if( appLoadFileToString( LoadedPerspectiveString, *FileName ) )
		{
			if( LoadedPerspectiveString.Len() > 0 )
			{
				// Load and apply the layout changes
				AUIManager.LoadPerspective( *LoadedPerspectiveString );

				// Make sure the UI gets updated for these changes
				UpdateUIForDockingChanges();

				bFileLoadedOK = TRUE;
			}
		}
	}

	return bFileLoadedOK;
}

/**
 * Saves the current docking layout to an XML file.  This should be called in the destructor of the subclass.
 */
void FDockingParent::SaveDockingLayout()
{
	// Save the layout of this file out to the config folder.
	FString LayoutFileName;
	GetLayoutFileName(LayoutFileName);

	// Save layout info
	FString SavedPerspectiveString = AUIManager.SavePerspective().c_str();
	if( SavedPerspectiveString.Len() > 0 )
	{
		if( appSaveStringToFile(*SavedPerspectiveString, *LayoutFileName) )
		{
			// Save the current version of the file to a INI file.
			GConfig->SetInt(TEXT("DockingLayoutVersionsAUI"), GetDockingParentName(), GetDockingParentVersion() + DOCKING_INTERNAL_VERSION, GEditorUserSettingsIni);
		}
	}
}

/**
 * Creates a new docking window using the parameters passed in. 
 *
 * @param ClientWindow	The client window for this docking window, most likely a wxPanel with some controls on it.
 * @param DockHost		The dock host that the new docking widget should be attached to.
 * @param WindowTitle	The title for this window.
 * @param WindowName	Unique identifying name for this dock window, should be set when the window title is auto generated.
 * @param BestSize		Optional parameter specifying the best size for this docking window
 */
void FDockingParent::AddDockingWindow( wxWindow* ClientWindow, EDockHost DockHost, const TCHAR* WindowTitle, const TCHAR* WindowName, wxSize BestSize )
{
	INT Direction = wxAUI_DOCK_CENTER;
	switch( DockHost )
	{
		case DH_Left:
			Direction = wxAUI_DOCK_LEFT;
			break;

		case DH_Right:
			Direction = wxAUI_DOCK_RIGHT;
			break;

		case DH_Top:
			Direction = wxAUI_DOCK_TOP;
			break;

		case DH_Bottom:
			Direction = wxAUI_DOCK_BOTTOM;
			break;

		default:
		case DH_None:
			Direction = wxAUI_DOCK_CENTER;
			break;
	}


	wxAuiPaneInfo PaneInfo;

	PaneInfo.caption = WindowTitle != NULL ? WindowTitle : wxEmptyString;
	PaneInfo.name = WindowName != NULL ? WindowName : PaneInfo.caption;
	// PaneInfo.window = ???;
	// PaneInfo.frame = ???;

	PaneInfo.state = 0;
	PaneInfo.dock_direction = Direction;

	if( Direction == wxAUI_DOCK_CENTER )
	{
		// Make sure the center window is named as such unless we already have a name
		if( PaneInfo.name.empty() )
		{
			PaneInfo.name = wxT( "CenterWindow" );
		}

		// Center windows always use specific flags
		PaneInfo.state |= wxAuiPaneInfo::optionPaneBorder;
		PaneInfo.state |= wxAuiPaneInfo::optionResizable;
	}
	else
	{
		// @todo AUI: Expose most of these as options for default dockable window layout?  Remember, most of
		//   these settings will be loaded from the docking window config anyway, so it's really only setting
		//   the initial state for new configs

		// PaneInfo.state |= wxAuiPaneInfo::optionFloating;
		PaneInfo.state |= wxAuiPaneInfo::optionLeftDockable;
		PaneInfo.state |= wxAuiPaneInfo::optionRightDockable;
		PaneInfo.state |= wxAuiPaneInfo::optionTopDockable;
		PaneInfo.state |= wxAuiPaneInfo::optionBottomDockable;
		PaneInfo.state |= wxAuiPaneInfo::optionFloatable;
		PaneInfo.state |= wxAuiPaneInfo::optionMovable;
		PaneInfo.state |= wxAuiPaneInfo::optionResizable;
		PaneInfo.state |= wxAuiPaneInfo::optionPaneBorder;
		PaneInfo.state |= wxAuiPaneInfo::optionCaption;
		// PaneInfo.state |= wxAuiPaneInfo::optionGripper;
		// PaneInfo.state |= wxAuiPaneInfo::optionDestroyOnClose;
		// PaneInfo.state |= wxAuiPaneInfo::optionToolbar;
		// PaneInfo.state |= wxAuiPaneInfo::optionGripperTop;
		// PaneInfo.state |= wxAuiPaneInfo::optionMaximized;

		// NOTE: Disabling the Close button unfortunately still displays the Close button icon on floating
		//   window title bars, which kind of sucks.
		PaneInfo.state |= wxAuiPaneInfo::buttonClose;

		// PaneInfo.state |= wxAuiPaneInfo.buttonMaximize;
		// PaneInfo.state |= wxAuiPaneInfo.buttonMinimize;
		// PaneInfo.state |= wxAuiPaneInfo.buttonPin;
		
		PaneInfo.dock_layer = 0;	// @todo AUI:  Allow these to be specified as params (for default layout)
		PaneInfo.dock_row = 0;		// @todo AUI
		PaneInfo.dock_pos = 0;		// @todo AUI

		PaneInfo.best_size = BestSize;	// @todo AUI
// 			PaneInfo.min_size.Set( -1, -1 );
// 			PaneInfo.max_size.Set( -1, -1 );

		PaneInfo.floating_pos = wxDefaultPosition;	// @todo AUI
		PaneInfo.floating_size = wxDefaultSize;		// @todo AUI
		PaneInfo.dock_proportion = 0;	// @todo AUI: What is this?

		// PaneInfo.buttons = ???;
		// PaneInfo.rect = ???;
	}

	verify( AUIManager.AddPane( ClientWindow, PaneInfo ) );


	// Window layout changed, so update UI
	// @todo AUI: Ideally we would only call this after *all* docking windows have been added, to reduce flicker
	UpdateUIForDockingChanges();
}
	

/**
 * Makes sure the specified client window is no longer associated with a dock window
 *
 * @param ClientWindow Window to disassociate
 */
void FDockingParent::UnbindDockingWindow( wxWindow* ClientWindow )
{
	AUIManager.DetachPane( ClientWindow );

	// NOTE: We don't bother applying this change with UpdateUIForDockingChanges since this is usually
	//   called on destruction of some parent window and we're not interested in seeing the results visually.
}


/**
 * Appends toggle items to the menu passed in that control visibility of all of the docking windows registered with the layout manager.
 *
 * @param MenuBar	wxMenuBar to append the docking menu to.
 *
 * @return Returns new Window menu
 */
wxMenu* FDockingParent::AppendWindowMenu( wxMenuBar* MenuBar )
{
	// Create the new Window menu
	wxMenu* WindowMenu = new wxMenu;

	// Store the window menu so we can talk to it later
	WindowMenuHandler.SetWindowMenu( WindowMenu );

	// Add the new menu to the owner's menu bar
	MenuBar->Append( WindowMenu, *LocalizeUnrealEd( TEXT( "&Window" ) ) );	

	return WindowMenu;
}



/**
* Sets the default area size of a dock host.
* @param Size	New Size for the dock host.
*/
void FDockingParent::SetDockHostSize(EDockHost DockHost, INT Size)
{
	// @todo AUI: Should we bother supporting this?
}

/**
 * Generates a filename using the docking parent name, the game config directory, and the current game name for saving/loading layout files.
 * @param OutFileName	Storage string for the generated filename.
 */
void FDockingParent::GetLayoutFileName(FString &OutFileName)
{
	OutFileName = FString::Printf(TEXT("%s%s%s_layoutAUI_%s.txt"),*appGameConfigDir(), GGameName,GetDockingParentName(), UObject::GetLanguage());
}


/**
 * Changes the title of an existing docking window
 *
 * @param InWindow The window associated with the pane that you'd like to change the title of
 * @param InNewTitle The new window title
 */
void FDockingParent::SetDockingWindowTitle( wxWindow* InWindow, const TCHAR* InNewTitle )
{
	wxAuiPaneInfo& WindowPaneInfo = AUIManager.GetPane( InWindow );
	if( WindowPaneInfo.IsOk() )
	{
		WindowPaneInfo.Caption( InNewTitle );

		// Apply changes to window title
		UpdateUIForDockingChanges();
	}
}



/**
 * Queries the state of a specific dockable window
 *
 * @param InWindow The window you're interested in querying
 * @param OutState [Out] Dockable window state
 *
 * @return TRUE if the specified window was found and state was returned
 */
UBOOL FDockingParent::GetDockingWindowState( wxWindow* InWindow, FDockingParent::FDockWindowState& OutState )
{
	OutState.bIsVisible = FALSE;
	OutState.bIsDocked = FALSE;

	const wxAuiPaneInfo& WindowPaneInfo = AUIManager.GetPane( InWindow );
	if( WindowPaneInfo.IsOk() )
	{
		OutState.bIsVisible = WindowPaneInfo.IsShown();
		OutState.bIsDocked = WindowPaneInfo.IsDocked();

		return TRUE;
	}

	// Not found
	return FALSE;
}



/**
 * Shows or hides a dockable window (along with it's associated pane)
 *
 * @param Window The window you're interested in showing or hiding
 * @param bInShow TRUE to show the window or FALSE to hide it
 */
void FDockingParent::ShowDockingWindow( wxWindow* Window, UBOOL bInShow )
{
	wxAuiPaneInfo& WindowPaneInfo = AUIManager.GetPane( Window );
	if( WindowPaneInfo.IsOk() )
	{
		WindowPaneInfo.Show( bInShow ? true : false );

		// Apply changes to docking window
		UpdateUIForDockingChanges();
	}
}




/** Call this to apply any changes you've made to docked windows */
void FDockingParent::UpdateUIForDockingChanges()
{
	// Tell AUI to apply all of the changes to panes we made since we started initializing
	AUIManager.Update();

	// Update our window menu if we have one
	WindowMenuHandler.RefreshMenuItems();

	// Run event to let derived classes know that something changed!
	OnWindowDockingLayoutChanged();
}




IMPLEMENT_DYNAMIC_CLASS( WxEventCatchingAuiManager, wxAuiManager )

BEGIN_EVENT_TABLE( WxEventCatchingAuiManager, wxAuiManager )
	EVT_AUI_PANE_CLOSE( WxEventCatchingAuiManager::OnEventReceived )
	// NOTE: Add other AUI-related events here as needed!
END_EVENT_TABLE()

/**
 * Called when an event is received.  Forwards the event to the custom event handler.
 */
void WxEventCatchingAuiManager::OnEventReceived( wxAuiManagerEvent& Event )
{
	if( CustomEventHandler != NULL )
	{
		CustomEventHandler->ProcessEvent( Event );
	}
}




IMPLEMENT_DYNAMIC_CLASS( WxDockableWindowMenuHandler, wxEvtHandler )

BEGIN_EVENT_TABLE( WxDockableWindowMenuHandler, wxEvtHandler )
    EVT_MENU( -1, WxDockableWindowMenuHandler::OnMenu )
	EVT_AUI_PANE_CLOSE( WxDockableWindowMenuHandler::OnPaneClosed )
END_EVENT_TABLE()


/** Constructor */
WxDockableWindowMenuHandler::WxDockableWindowMenuHandler()
	: Owner( NULL ),
	  WindowMenu( NULL )
{
}



/** Destructor */
WxDockableWindowMenuHandler::~WxDockableWindowMenuHandler()
{
	Owner = NULL;

	// Wx will handle deletion of these objects
	WindowMenu = NULL;
}


/**
 * Sets the owner of this object
 *
 * @param NewOwner The new owner for this object
 */
void WxDockableWindowMenuHandler::SetOwner( FDockingParent* NewOwner )
{
	Owner = NewOwner;
}



/**
 * Sets the 'Window' menu that we should be tracking
 *
 * @param NewWindowMenu The new 'Window' menu to track
 */
void WxDockableWindowMenuHandler::SetWindowMenu( wxMenu* NewWindowMenu )
{
	WindowMenu = NewWindowMenu;

	// Go ahead and setup existing panes immediately
	RefreshMenuItems();
}


	
void WxDockableWindowMenuHandler::RefreshMenuItems()
{
	if( WindowMenu != NULL && Owner != NULL )
	{
		const wxAuiPaneInfoArray& AllPanes = Owner->AccessAUIManager().GetAllPanes();

		for( UINT CurPaneIndex = 0; CurPaneIndex < AllPanes.size(); ++CurPaneIndex )
		{
			const wxAuiPaneInfo& CurPane = AllPanes[ CurPaneIndex ];

			if( CurPane.IsOk() )
			{
				// Ignore panes that are not really dockable (center panes)
				const UBOOL IsClosableWindow =
					( CurPane.IsFloatable() ||
					  CurPane.IsTopDockable() ||
					  CurPane.IsBottomDockable() ||
					  CurPane.IsLeftDockable() ||
					  CurPane.IsRightDockable() );
				if( IsClosableWindow )
				{
					INT MenuItemID = WindowMenu->FindItem( CurPane.caption );

					wxMenuItem* MenuItem = NULL;
					if( MenuItemID == wxNOT_FOUND )
					{
						// No menu item for this pane yet, so we'll add it now!
						MenuItem = WindowMenu->AppendCheckItem( -1, CurPane.caption );
					}
					else
					{
						// Found existing menu item with the same caption.
						MenuItem = WindowMenu->FindItem( MenuItemID );
					}

					if( MenuItem != NULL )
					{
						// Update the checked state of this item
						MenuItem->Check( CurPane.IsShown() );
					}
				}
			}
		}
	}
}

/** Clear the windows in the window list  */
void WxDockableWindowMenuHandler::ClearWindowList()
{
	if(WindowMenu != NULL && Owner != NULL)
	{
		const wxAuiPaneInfoArray& AllPanes = Owner->AccessAUIManager().GetAllPanes();
		for( UINT CurPaneIndex = 0; CurPaneIndex < AllPanes.size(); ++CurPaneIndex )
		{
			const wxAuiPaneInfo& CurPane = AllPanes[ CurPaneIndex ];

			if( CurPane.IsOk() )
			{
				// Ignore panes that are not really dockable (center panes)
				const UBOOL IsClosableWindow =
					( CurPane.IsFloatable() ||
					CurPane.IsTopDockable() ||
					CurPane.IsBottomDockable() ||
					CurPane.IsLeftDockable() ||
					CurPane.IsRightDockable() );
				if( IsClosableWindow )
				{
					INT MenuItemID = WindowMenu->FindItem( CurPane.caption );

					if( MenuItemID != wxNOT_FOUND )
					{
						// Remove the item from the menu to keep the menu clean
						WindowMenu->Remove(MenuItemID);
					}
				}
			}
		}
	}
}



/** Wx event: Called when a menu item is interacted with */
void WxDockableWindowMenuHandler::OnMenu( wxCommandEvent& Event )
{
	UBOOL bWasHandled = FALSE;

    if( Event.GetId() != -1 && WindowMenu != NULL && Owner != NULL )
	{
	    wxMenuItem* MenuItem = WindowMenu->FindItem( Event.GetId() );
		if( MenuItem != NULL )
		{
		    wxString MenuItemName = MenuItem->GetLabel();

			// Find the dockable window with a caption that matches this item's name
			const wxAuiPaneInfoArray& AllPanes = Owner->AccessAUIManager().GetAllPanes();
			for( UINT CurPaneIndex = 0; CurPaneIndex < AllPanes.size(); ++CurPaneIndex )
			{
				wxAuiPaneInfo& CurPane = AllPanes[ CurPaneIndex ];

				if( CurPane.IsOk() )
				{
					if( CurPane.caption	== MenuItemName )
					{
						// Found it!
						CurPane.Show( MenuItem->IsChecked() );

						// Apply changes to docking window
						Owner->UpdateUIForDockingChanges();

						bWasHandled = TRUE;

						break;
					}
				}
			}
		}
	}

	if( !bWasHandled )
	{
		// Skip the event to allow further processing
		Event.Skip( true );
	}
}



/** Wx AUI event: Called when a dockable window is closed */
void WxDockableWindowMenuHandler::OnPaneClosed( wxAuiManagerEvent& Event )
{
	if( WindowMenu != NULL && Owner != NULL )
	{
		wxAuiPaneInfo* ClosingPane = Event.pane;
		if( ClosingPane != NULL && ClosingPane->IsOk() )
		{
			INT MenuItemID = WindowMenu->FindItem( ClosingPane->caption );

			if( MenuItemID != wxNOT_FOUND )
			{
				// Found the menu item!
				wxMenuItem* MenuItem = WindowMenu->FindItem( MenuItemID );

				if( MenuItem != NULL )
				{
					// Update the checked state of this item
					MenuItem->Check( false );
				}
			}
		}
	}
}


