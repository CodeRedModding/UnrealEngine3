/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "UnrealEd.h"

// Browsers
#include "ActorBrowser.h"
#include "ReferencedAssetsBrowser.h"
#include "LogBrowser.h"
#include "LevelBrowser.h"
#include "LayerBrowser.h"
#include "StaticMeshStatsBrowser.h"
#include "EngineProcBuildingClasses.h"
#include "BuildingStatsBrowser.h"
#include "TextureStatsBrowser.h"
#include "SceneManager.h"
#include "TaskBrowser.h"
#include "ContentBrowserHost.h"
#include "StartPageHost.h"
#include "GameStatsBrowser.h"
#include "AttachmentEditor.h"
#if WITH_MANAGED_CODE
#include "ContentBrowserShared.h"
#include "StartPageShared.h"
#endif

IMPLEMENT_CLASS(UBrowserManager);

// These are for wxWindows' runtime creation of types. These normally would
// live in there respective implementation files. However, the linker would
// strip the statics out because nothing directly called/referenced the
// module they were in. So, they had to be moved here so that the statics
// would be properly instantiated
IMPLEMENT_DYNAMIC_CLASS(WxActorBrowser,WxBrowser);
IMPLEMENT_DYNAMIC_CLASS(WxReferencedAssetsBrowser,WxBrowser);
IMPLEMENT_DYNAMIC_CLASS(WxLogBrowser,WxBrowser);
IMPLEMENT_DYNAMIC_CLASS(WxLevelBrowser,WxBrowser);
IMPLEMENT_DYNAMIC_CLASS(WxLayerBrowser,WxBrowser);
IMPLEMENT_DYNAMIC_CLASS(WxPrimitiveStatsBrowser,WxBrowser);
IMPLEMENT_DYNAMIC_CLASS(WxBuildingStatsBrowser,WxBrowser);
IMPLEMENT_DYNAMIC_CLASS(WxTextureStatsBrowser,WxBrowser);
IMPLEMENT_DYNAMIC_CLASS(WxSceneManager,WxBrowser);
IMPLEMENT_DYNAMIC_CLASS(WxTaskBrowser,WxBrowser);
IMPLEMENT_DYNAMIC_CLASS(WxContentBrowserHost,WxBrowser);
IMPLEMENT_DYNAMIC_CLASS(WxGameStatsVisualizer,WxBrowser);
IMPLEMENT_DYNAMIC_CLASS(WxStartPageHost,WxBrowser);
IMPLEMENT_DYNAMIC_CLASS(WxAttachmentEditor,WxBrowser);

/**
 * Returns the WxBrowser for the specified ID
 *
 * @param PaneID the browser pane id to search for
 */
WxBrowser* UBrowserManager::GetBrowserPane(INT PaneID)
{
	WxBrowser* Pane = NULL;
	// Search through the entries for a matching paneID
	for (INT Index = 0; Index < BrowserPanes.Num() && Pane == NULL; Index++)
	{
		if (BrowserPanes(Index).PaneID == PaneID)
		{
			Pane = (WxBrowser*)BrowserPanes(Index).WxBrowserPtr;
		}
	}
	return Pane;
}

/**
 * Returns the WxBrowser for the specified friendly name.
 *
 * @param FriendlyName the friendly name of the window to search for
 * @param bCannonicalOnly whether to include clones or not
 */
WxBrowser* UBrowserManager::GetBrowserPane(const TCHAR* FriendlyName,
	UBOOL bCannonicalOnly)
{
	WxBrowser* Pane = NULL;
	// Search through the entries for a matching friendly name
	for (INT Index = 0; Index < BrowserPanes.Num() && Pane == NULL; Index++)
	{
		// If the names match and this is not a clone window or they allow
		// clones to be found by name
		if (BrowserPanes(Index).FriendlyName == FriendlyName &&
			(bCannonicalOnly == FALSE || BrowserPanes(Index).CloneOfPaneID < 0))
		{
			Pane = (WxBrowser*)BrowserPanes(Index).WxBrowserPtr;
		}
	}
	return Pane;
}

/**
 * Creates all of the browser windows that are registered with the manager.
 * Also, loads their persistent state for them
 *
 * @param Parent the parent window to use for all browser panes
 */
void UBrowserManager::CreateBrowserPanes(wxWindow* Parent)
{
	// Should we enable the Content Browser?
	UBOOL bShouldInitContentBrowser = FALSE;
#if WITH_MANAGED_CODE
	bShouldInitContentBrowser = FContentBrowser::ShouldUseContentBrowser();
#endif

	UBOOL bShouldInitGameStatsVisualizer = ParseParam( appCmdLine(), TEXT("GameStats") );
#if !UDK
	if (!bShouldInitGameStatsVisualizer)
	{
		// See if the game has made it a default option in their INI file
		GConfig->GetBool( TEXT("GameplayStatsCollection"), TEXT("EnableVisualizers"), bShouldInitGameStatsVisualizer, GEditorIni );
	}
#endif

	// Task browser is disabled in UDK builds as we don't support TestTrack by default
	UBOOL bShouldInitTaskBrowser = TRUE;
#if UDK
	bShouldInitTaskBrowser = FALSE;
#endif

	// For each entry, create the browser pane associated with it
	for (INT Index = 0; Index < BrowserPanes.Num(); Index++)
	{
		// Prevent the user from doing harmful things by hand editing these
		// values to incorrect ones
		if (BrowserPanes(Index).PaneID != Index)
		{
			INT OldPaneID = BrowserPanes(Index).PaneID;
			BrowserPanes(Index).PaneID = Index;
			// Search through and fix any clones of this invalid id
			for (INT SearchIndex = 0; SearchIndex < BrowserPanes.Num(); SearchIndex++)
			{
				if (BrowserPanes(SearchIndex).CloneOfPaneID == OldPaneID)
				{
					// Replace the old bad id, with a valid one
					BrowserPanes(SearchIndex).CloneOfPaneID = Index;
				}
			}
		}
		WxBrowser* Pane = NULL;

		const FString ContentBrowserName( TEXT("WxContentBrowserHost") );
		const FString GameVisualizerName( TEXT("WxGameStatsVisualizer") );
		const FString TaskBrowserName( TEXT( "WxTaskBrowser" ) );

		const UBOOL bIsContentBrowserPane = ContentBrowserName == (*BrowserPanes(Index).WxWindowClassName);
		const UBOOL bIsGameStatsPane = GameVisualizerName == (*BrowserPanes(Index).WxWindowClassName);
		const UBOOL bIsTaskBrowserPane = TaskBrowserName == (*BrowserPanes(Index).WxWindowClassName);
		if ( (bShouldInitContentBrowser && bIsContentBrowserPane) ||
			 (bShouldInitGameStatsVisualizer && bIsGameStatsPane) ||
			 (bShouldInitTaskBrowser && bIsTaskBrowserPane) ||
			 (!bIsContentBrowserPane && !bIsGameStatsPane && !bIsTaskBrowserPane) )
		{
			CreateSingleBrowserPane(Parent, 
									BrowserPanes(Index).WxWindowClassName,
									BrowserPanes(Index).FriendlyName,
									BrowserPanes(Index).bInitiallyHidden,
									BrowserPanes(Index).PaneID, 
									BrowserPanes(Index).CloneOfPaneID, 
									BrowserPanes(Index).CloneNumber,
									OUT BrowserPanes(Index).WxBrowserPtr);
		}
	}
}

/**
 * Creates one browser window.
 * Also, loads their persistent state for them
 *
 * @param Parent the parent window to use for all browser panes
 * @param bInInitiallyHidden True if the window should be hidden if we have no other saved state
 */
void UBrowserManager::CreateSingleBrowserPane(wxWindow* InParent, const FString& InWxName, const FString& InFriendlyName, const UBOOL bInInitiallyHidden, const INT InPaneID, const INT InCloneOfPaneID, const INT InCloneNumber, OUT FPointer& OutWxBrowserPtr)
{
	// Use the wxWindows' RTTI system to create the window
	wxObject* NewObject = wxCreateDynamicObject(*InWxName);
	if (NewObject != NULL)
	{
		// Use wxWindows' RTTI system to verify class type
		WxBrowser* Pane = wxDynamicCast(NewObject,WxBrowser);
		if (Pane != NULL)
		{
			// Now do part 2 of the 2 phase creation process
			Pane->Create(InPaneID, *InFriendlyName,InParent);
			// Load the window state for this pane
			Pane->LoadWindowState( bInInitiallyHidden );
			if (InCloneOfPaneID >= 0)
			{
				// Build the instance of the localized caption
				Pane->SetLocalizedCaption(*FString::Printf(TEXT("%s-%d"),
					Pane->GetLocalizedCaption(),
					InCloneNumber));
			}
			// Cache our pointer
			OutWxBrowserPtr = Pane;
			// And add it to the list of ones we successfully created
			GetCreatedPanes().AddItem(Pane);
		}
		else
		{
			debugf(TEXT("Class %s is not derived from WxBrowser"), *InWxName);
			delete NewObject;
		}
	}
	else
	{
		debugf(TEXT("Failed to create browser pane (%d) with class %s and name %s"),
			InPaneID,
			*InWxName,
			*InFriendlyName);
	}
}


/**
 * Adds another copy of the specified browser pane to the list of registered
 * browser panes. Automatically creates a unique pane id. Also saves the
 * configuration so that the settings are remembered for subsequent sessions.
 *
 * @param Parent the parent window to use for all browser panes
 * @param PaneID the window pane that is being cloned
 *
 * @return the newly created browser pane
 */
WxBrowser* UBrowserManager::CloneBrowserPane(wxWindow* Parent,INT PaneID)
{
	WxBrowser* Pane = NULL;
	// Search through the entries for a matching friendly name
	for (INT Index = 0; Index < BrowserPanes.Num() && Pane == NULL; Index++)
	{
		// If this is the pane we want to clone
		if (BrowserPanes(Index).PaneID == PaneID)
		{
			// Holds the position in the array that this was created at
			INT NewIndex;
			// Figure out what the next id will be
			INT NewPaneID = GetNextPaneID();
			// If the pane is a clone, don't clone it, clone it's source
			if (BrowserPanes(Index).CloneOfPaneID >= 0)
			{
				// Figure out how many clones there are
				INT NextClone = GetCloneCount(BrowserPanes(Index).CloneOfPaneID) + 1;
				// Create a new entry in the list of panes
				NewIndex = BrowserPanes.AddZeroed();
				// And set the values for it
				BrowserPanes(NewIndex).PaneID = NewPaneID;
				BrowserPanes(NewIndex).CloneNumber = NextClone;
				BrowserPanes(NewIndex).CloneOfPaneID =
					BrowserPanes(Index).CloneOfPaneID;
				BrowserPanes(NewIndex).WxWindowClassName =
					BrowserPanes(Index).WxWindowClassName;
				// Build the docking name from the original
				WxBrowser* ClonedPane =
					GetBrowserPane(BrowserPanes(Index).CloneOfPaneID);
				check(ClonedPane && "INI settings are broken as the ID is invalid");
				// Build the new friendly name
				BrowserPanes(NewIndex).FriendlyName = FString::Printf(TEXT("%s-%d"),
					ClonedPane->GetDockingName(),NextClone);
			}
			// Otherwise this is an original pane so clone it
			else
			{
				// Figure out how many clones there are
				INT NextClone = GetCloneCount(BrowserPanes(Index).PaneID) + 1;
				// Create a new entry in the list of panes
				NewIndex = BrowserPanes.AddZeroed();
				// And set the values for it
				BrowserPanes(NewIndex).PaneID = NewPaneID;
				BrowserPanes(NewIndex).CloneNumber = NextClone;
				BrowserPanes(NewIndex).CloneOfPaneID = BrowserPanes(Index).PaneID;
				BrowserPanes(NewIndex).WxWindowClassName =
					BrowserPanes(Index).WxWindowClassName;
				// Build the new friendly name
				BrowserPanes(NewIndex).FriendlyName = FString::Printf(TEXT("%s-%d"),
					*BrowserPanes(Index).FriendlyName,NextClone);
			}
			// And finally save this state so it will be there next time
			SaveConfig();
			// Use the wxWindows' RTTI system to create the window
			Pane = wxDynamicCast(wxCreateDynamicObject(
				*BrowserPanes(NewIndex).WxWindowClassName),WxBrowser);
			if (Pane != NULL)
			{
				// Now do part 2 of the 2 phase creation process
				Pane->Create(BrowserPanes(NewIndex).PaneID,
					*BrowserPanes(NewIndex).FriendlyName,Parent);
				// Load the window state for this pane
				const UBOOL bInitiallyHidden = FALSE;	// Cloned panes are never hidden by default
				Pane->LoadWindowState( bInitiallyHidden );
				// Build the instance of the localized caption
				Pane->SetLocalizedCaption(*FString::Printf(TEXT("%s-%d"),
					Pane->GetLocalizedCaption(),
					BrowserPanes(NewIndex).CloneNumber));
				// Cache our pointer
				BrowserPanes(NewIndex).WxBrowserPtr = Pane;
			}
		}
	}
	return Pane;
}

/**
 * Removes a browser pane from the list of registered panes. If this is a
 * "canonical" pane, then it just hides the pane. If it is a clone, it
 * removes the entry and saves the configuration.
 *
 * @param PaneID the browser pane to remove from the list
 *
 * @return TRUE if it was removed, FALSE if it should just be hidden
 */
UBOOL UBrowserManager::RemoveBrowser(INT PaneID)
{
	UBOOL bWasRemoved = FALSE;
	WxBrowser* Pane = NULL;
	// Search through the entries for a matching pane id
	for (INT Index = 0; Index < BrowserPanes.Num() && Pane == NULL; Index++)
	{
		// If this is the pane we want to remove
		if (BrowserPanes(Index).PaneID == PaneID)
		{
			Pane = (WxBrowser*)BrowserPanes(Index).WxBrowserPtr;
			// If the pane is a clone, destroy it and remove the entry
			if (BrowserPanes(Index).CloneOfPaneID >= 0)
			{
				// Cleanup the frame or the docking container
				if (Pane->IsDocked() == TRUE)
				{
					RemoveDockedBrowser(Pane->GetDockID());
				}
				else
				{
					RemoveFloatingBrowser(Pane->GetDockID());
				}
				check(BrowserMenuPtr);
				// Save the window's state
				Pane->SaveWindowState();
				// Now destroy it
				Pane->Destroy();
				// Remove this entry from our list
				BrowserPanes.Remove(Index);
				// And finally save this state so it won't be there next time
				SaveConfig();
				// Now remove it from the menu
				((wxMenu*)BrowserMenuPtr)->Remove(PaneID + IDM_BROWSER_START);
				bWasRemoved = TRUE;
			}
			// Otherwise this is a canonical pane and can only be hidden
			else
			{
				if (Pane->IsFloating() == TRUE)
				{
					// Hide the frame of the browser
					ShowWindow(Pane->GetDockID(),FALSE);
				}
				// So hide the pane
				Pane->SetVisibleState(FALSE);
			}
		}
	}
	return bWasRemoved;
}

/**
 * Handles removing a pane that is floating. Breaks the link between the
 * parent->child and destroyes the frame. Note this does not destroy the
 * browser, just the frame
 *
 * @param PaneID the ID of the pane to clean up it's frame for
 */
void UBrowserManager::RemoveFloatingBrowser(INT PaneID)
{
	UBOOL bFound = FALSE;
	FFloatingFrameArray& FloatingWindows = GetFloatingWindows();
	// Find this window in the floating frames list
	for (INT Index = 0; Index < FloatingWindows.Num() && bFound == FALSE;
		Index++)
	{
		// Search for a matching browser pane
		if (FloatingWindows(Index)->GetChildWindow()->GetDockID() == PaneID)
		{
			bFound = TRUE;
			// Get the browser pane
			WxBrowser* Pane = FloatingWindows(Index)->GetChildWindow();
			// Remove the menu/frame links
			Pane->DetachMenus();
			// Zero out the child
			FloatingWindows(Index)->SetChildWindow(NULL);
			// Clean up the window
			FloatingWindows(Index)->Destroy();
			FloatingWindows.Remove(Index);
		}
	}
}

/**
 * Handles removing a pane that is docked. Removes the docked page. Note
 * it doesn't destroy the pane just the page that was holding it
 *
 * @param PaneID the ID of the pane to remove from the docking container
 */
void UBrowserManager::RemoveDockedBrowser(INT PaneID)
{
	bIsBrowserLayoutChanging = TRUE;
	// Look through the docking container's list
	for (UINT Index = 0; Index < GetDockingContainer()->GetNotebook()->GetPageCount() ; Index++)
	{
		WxBrowser* Page = (WxBrowser*)GetDockingContainer()->GetNotebook()->GetPage(Index);
		// If this is the page we want
		if (Page->GetDockID() == PaneID)
		{
			// Remove it from the docked window
			GetDockingContainer()->GetNotebook()->RemovePage(Index);
			break;
		}
	}

	bIsBrowserLayoutChanging = FALSE;
}

/**
 * Destroys all browser panes
 */
void UBrowserManager::DestroyBrowserPanes(void)
{
	WxBrowser* Pane = NULL;
	// Loop through destroy each window
	for (INT Index = 0; Index < BrowserPanes.Num(); Index++)
	{
		// Cast it to a wxWindow type so we can do useful things
		Pane = (WxBrowser*)BrowserPanes(Index).WxBrowserPtr;
		if (Pane != NULL)
		{
			// Write out an state changes before destroying
			Pane->SaveWindowState();
			// Destroy it
			Pane->Destroy();
			// Null it out, so we don't try to use it later
			BrowserPanes(Index).WxBrowserPtr = NULL;
		}
	}
}

/**
 * Destroys any allocated resources
 */
void UBrowserManager::DestroyBrowsers(void)
{
	// Now destroy the container
	GetDockingContainer()->Destroy();
	// Destroy the container of created browser pane references (only points at panes; does not own)
	delete reinterpret_cast<FBrowserArray*>(CreatedPanesPtr);

	// Get the floating windows list and destroy those too
	FFloatingFrameArray& FloatingWindows = GetFloatingWindows();
	for (INT Index = 0; Index < FloatingWindows.Num(); Index++)
	{
		FloatingWindows(Index)->Destroy();
	}
	FloatingWindows.Empty();
	// Now delete the array allocation
	delete ((FFloatingFrameArray*)FloatingWindowsArrayPtr);
	// Destroy all browser panes
	DestroyBrowserPanes();
}

/**
 * Saves the currently selected browser to the INI
 */
void UBrowserManager::SaveState(void)
{
	// Figure out which page is selected
	INT Index = GetDockingContainer()->GetNotebook()->GetSelection();
	if (Index > -1)
	{
		// Get the pointer to that browser so we can get the dock id
		WxBrowser* Page = (WxBrowser*)GetDockingContainer()->GetNotebook()->GetPage(Index);
		// Cache it so the auto save will work
		LastSelectedPaneID = Page->GetDockID();
	}
	// Now save our config
	SaveConfig();
}

/**
 * Initializes all of the window states with the list of brower panes that
 * need to have their container window's created.
 */
void UBrowserManager::Initialize(void)
{
	check(bHasCreatedPanes == FALSE);

	// Build the panes from the INI settings
	CreateBrowserPanes(GetDockingContainer()->GetNotebook());
	
	// Set the browser window icon
	{
		extern INT GEditorIcon;

		HWND WindowHnd = (HWND)GetDockingContainer()->GetHandle();
		HICON EditorIconHandle = LoadIcon( hInstance, MAKEINTRESOURCE( GEditorIcon ) );
		if( EditorIconHandle != NULL )
		{
			::SendMessage( WindowHnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)EditorIconHandle );
			::SendMessage( WindowHnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)EditorIconHandle );
		}
	}

	// Indicate that we've done this
	bHasCreatedPanes = TRUE;
}

/**
 * Show the browser windows that were visible when the editor was last closed.
 */
void UBrowserManager::RestoreBrowserWindows()
{
	const FBrowserArray& Panes = GetCreatedPanes();
	// Loop through each of the browser panes creating the container window
	for (INT Index = 0; Index < Panes.Num(); Index++)
	{
		// If this window was previously docked, add a docked container
		if (Panes(Index)->IsDocked())
		{
			AttachDocked(Panes(Index));
		}
		// Otherwise, add a floating container
		else
		{
			AttachFrame(Panes(Index));
		}
	}
	//// Bring up the docked windows
	GetDockingContainer()->Show();
	//// Now make it current
	ShowWindow(LastSelectedPaneID,TRUE);

	bHasStateBeenRestored = TRUE;
}

/**
 * Attaches a floating frame window to the specified browser window
 *
 * @param BrowserPane the browser pane to create a floating frame for
 */
void UBrowserManager::AttachFrame(WxBrowser* BrowserPane)
{
	// Create a new frame
	WxFloatingFrame* Frame = new WxFloatingFrame(GApp->EditorFrame,-1,
		BrowserPane->GetDockingName());
	// Add the browser as a child
	Frame->SetChildWindow(BrowserPane);
	// Resize the child
	wxSizeEvent DummyEvent;
	Frame->OnSize( DummyEvent );
	// Hide/show the window based upon the browser's state
	if (BrowserPane->IsVisible() == TRUE)
	{
		Frame->Show();
	}
	else
	{
		Frame->Close();
	}
	// Mark this as floating in case it was docked and is transitioning to floating
	// This needs to happen before activation so that the menu's are updated
	// properly
	BrowserPane->SetDockStateToFloating();
	// Tell it to be activated
	BrowserPane->Activated();
	// Update the parent's caption
	BrowserPane->SetParentCaption();
	// Add this to our cached list
	GetFloatingWindows().AddItem(Frame);
}

/**
 * Attaches the browser window to the docked window set
 *
 * @param BrowserPane the browser pane to add to the docked container
 */
void UBrowserManager::AttachDocked(WxBrowser* BrowserPane)
{
	// Mark this as docked in case it was floating and is transitioning to docked
	// This has to happen first, since the act of being added calls Activated()
	BrowserPane->SetDockStateToDocked();

	UBOOL bSelect = FALSE;
	WxBrowser* CurrentBrowser = GetBrowserPane(LastSelectedPaneID);
	//if this is the expected first window OR we're past restoration point
	if ((bHasStateBeenRestored) || (CurrentBrowser == BrowserPane))
	{
		bSelect = TRUE;
	}

	// Add the browser pane as a tabbed page
	bIsBrowserLayoutChanging = TRUE;
	GetDockingContainer()->GetNotebook()->AddPage(BrowserPane, BrowserPane->GetLocalizedCaption(),bSelect?true:false);
	bIsBrowserLayoutChanging = FALSE;

	// Force the child to resize
	wxSizeEvent DummyEvent;
	GetDockingContainer()->OnSize(DummyEvent);
	// Update it's parent's caption
	BrowserPane->SetParentCaption();
}

/**
 * Adds the specified browser window to the docking container and kills
 * the floating frame
 *
 * @param PaneID the browser pane to make docked
 */
void UBrowserManager::DockBrowserWindow(INT PaneID)
{
	UBOOL bFound = FALSE;
	FFloatingFrameArray& FloatingWindows = GetFloatingWindows();
	// Find this window in the floating frames list
	for (INT Index = 0; Index < FloatingWindows.Num() && bFound == FALSE;
		Index++)
	{
		// Search for a matching browser pane
		if (FloatingWindows(Index)->GetChildWindow()->GetDockID() == PaneID)
		{
			bFound = TRUE;
			// Get the browser pane
			WxBrowser* Pane = FloatingWindows(Index)->GetChildWindow();
			// Don't bother if it's already docked
			if (Pane->IsFloating() == TRUE)
			{
				// Remove the menu/frame links
				Pane->DetachMenus();
				// Zero out the child
				FloatingWindows(Index)->SetChildWindow(NULL);
				// Switch the parent to the notebook
				Pane->Reparent(GetDockingContainer()->GetNotebook());
				// And move this to the docked container
				AttachDocked(Pane);
				// Clean up the window
				FloatingWindows(Index)->Destroy();
				FloatingWindows.Remove(Index);
			}
		}
	}
}
	
/**
 * Removes the specified browser window from the docking container and creates
 * a floating frame for it
 *
 * @param PaneID the browser pane to make floating
 */
void UBrowserManager::UndockBrowserWindow(INT PaneID)
{
	// Look through the docking container's list
	for (UINT Index = 0 ; Index < GetDockingContainer()->GetNotebook()->GetPageCount() ; Index++)
	{
		WxBrowser* Page = (WxBrowser*)GetDockingContainer()->GetNotebook()->GetPage(Index);
		// If this is the page we want and it's not already floating
		if (Page->GetDockID() == PaneID && Page->IsDocked() == TRUE)
		{
			// Remove it from the docked window
			bIsBrowserLayoutChanging = TRUE;
			GetDockingContainer()->GetNotebook()->RemovePage(Index);
			bIsBrowserLayoutChanging = FALSE;

			// Now give it a frame
			AttachFrame(Page);
			break;
		}
	}
}

/**
 * Locates a dockable window and hides/shows the window depending on
 * the value of bShowHide
 *
 * @param InDockID The window to make visible.
 * @param bShowHide TRUE show the window, FALSE hides it
 */
void UBrowserManager::ShowWindow(INT InDockID,UBOOL bShowHide)
{
	WxDockingContainer* DockingContainer = GetDockingContainer();
	// Look through the docking container's list
	for (UINT Index = 0 ; Index < DockingContainer->GetNotebook()->GetPageCount() ; Index++)
	{
		WxBrowser* Page = (WxBrowser*)DockingContainer->GetNotebook()->GetPage(Index);
		if (Page->GetDockID() == InDockID)
		{
			if (bShowHide == TRUE)
			{
				// Show the browser
				DockingContainer->Show();

				bIsBrowserLayoutChanging = TRUE;
				DockingContainer->GetNotebook()->SetSelection(Index);
				bIsBrowserLayoutChanging = FALSE;

				// Mark the browser as visible
				Page->SetVisibleState(TRUE);
				// Update the parent's caption
				Page->SetParentCaption();
			}
			else
			{
				DockingContainer->Hide();
				// Mark the browser as hidden
				Page->SetVisibleState(FALSE);
			}
			break;
		}
	}
	FFloatingFrameArray& FloatingWindows = GetFloatingWindows();
	// For floating windows, make the parent floating frame visible.
	for (INT Index = 0 ; Index < FloatingWindows.Num() ; Index++)
	{
		if (FloatingWindows(Index)->GetChildWindow()->GetDockID() == InDockID)
		{
			if (bShowHide == TRUE)
			{
				// Make it visible
				FloatingWindows(Index)->Show();
				FloatingWindows(Index)->Raise();
				// Mark the browser as visible
				FloatingWindows(Index)->GetChildWindow()->SetVisibleState(TRUE);
			}
			else
			{
				FloatingWindows(Index)->Hide();
				// Mark the browser as hidden
				FloatingWindows(Index)->GetChildWindow()->SetVisibleState(FALSE);
			}
			break;
		}
	}
}

/**
 * Adds another copy of the specified browser pane to the list of registered
 * browser panes. Automatically creates a unique pane id. Also saves the
 * configuration so that the settings are remembered for subsequent sessions.
 *
 * @param PaneID the window pane that is being cloned
 */
void UBrowserManager::CloneBrowser(INT PaneID)
{
	// Create a clone of the existing pane
	WxBrowser* NewBrowser = CloneBrowserPane(GetDockingContainer()->GetNotebook(),
		PaneID);
	if (NewBrowser != NULL)
	{
		// Now attach it to our container
		AttachDocked(NewBrowser);
		// And add it to the menu
		AddToMenu(NewBrowser);
	}
}

/**
 * Determines whether this is a canonical browser or not
 *
 * @param PaneID the browser to see if it is a clone or original
 */
UBOOL UBrowserManager::IsCanonicalBrowser(INT PaneID)
{
	UBOOL bFound = FALSE;
	UBOOL bIsCanonical = FALSE;
	// Search through the entries for a matching paneID
	for (INT Index = 0; Index < BrowserPanes.Num() && bFound == FALSE; Index++)
	{
		if (BrowserPanes(Index).PaneID == PaneID)
		{
			bFound = TRUE;
			// Set this if there is no clone id
			bIsCanonical = BrowserPanes(Index).CloneOfPaneID < 0;
		}
	}
	return bIsCanonical;
}

/**
 * Uses the menu id to index into the browser list and makes sure that
 * window is shown
 *
 * @param MenuID the id of the menu that corresponds to a browser pane
 */
void UBrowserManager::ShowWindowByMenuID(INT MenuID)
{
	// Map the IDs to dock ids
	INT NormalizedID = MenuID - IDM_BROWSER_START;
	// Try to find the correct pane
	ShowWindow(NormalizedID,TRUE);
}



/** Makes sure that the main docking container window is visible */
void UBrowserManager::ShowDockingContainer()
{
	GetDockingContainer()->Show();
	GetDockingContainer()->Raise();
}


/**
 * Adds the browser windows to the browser menu. Caches a pointer to the
 * menu so that it can make changes to the menu as browsers are created
 * and removed.
 *
 * @param InMenu the menu to modify with browser settings
 */
void UBrowserManager::InitializeBrowserMenu(wxMenu* InMenu)
{
	check(bHasCreatedPanes);
	// Cache this for later
	BrowserMenuPtr = InMenu;
	// Work through the list of registered windows and add them to the menu
	for (INT Index = 0; Index < BrowserPanes.Num(); Index++)
	{
		// Get the browser so we can make calls on it
		WxBrowser* Browser = (WxBrowser*)BrowserPanes(Index).WxBrowserPtr;
		if (Browser != NULL)
		{
			check((Browser->GetDockID() + IDM_BROWSER_START) <= IDM_BROWSER_END);
			// Add the item based off of index into the array
			InMenu->Append(Browser->GetDockID() + IDM_BROWSER_START,
				Browser->GetLocalizedCaption(),
				// Build the localized tool tip text
				*LocalizeUnrealEd(*FString::Printf(TEXT("%s_ToolTip"),
					Browser->GetLocalizationKey())));
		}
	}
}

/**
 * Adds the specified pane to the browser menu
 *
 * @param Pane the browser pane to add to the menu
 */
void UBrowserManager::AddToMenu(WxBrowser* Pane)
{
	check(BrowserMenuPtr);
	// So we don't have to retype the cast
	wxMenu* BrowserMenu = (wxMenu*)BrowserMenuPtr;
	// Figure out how many dynamic items were added (total minus separator & kismet)
	INT Pos = BrowserMenu->GetMenuItemCount() - 2;
	check(Pos >= 0);
	// Now insert the new menu item before the one specified
	BrowserMenu->Insert(Pos,IDM_BROWSER_START + Pane->GetDockID(),
		Pane->GetLocalizedCaption());
}
