/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LEVELBROWSER_H__
#define __LEVELBROWSER_H__

// Forward declarations.
class ULevel;
class WxGroupPane;
class WxLevelPane;
class WxPropertyWindowFrame;
template<typename ObjectType>
class WxCheckBoxListWindow;


namespace LevelBrowser 
{

	void InitializeStreamingMethods();

	/**
	 * Encapsulates calls to setting level visibility so that selected actors
	 * in levels that are being hidden can be deselected.
	 */
	void SetLevelVisibility(ULevel* Level, UBOOL bVisible);
}



/**
 * A single selectable item in the level browser level pane.  May represent either a level or a level grid volume.
 */
class FLevelBrowserItem
{

public:

	/**
	 * Construct this item from a level
	 *
	 * @param	InLevel		Level that this item will represent
	 */
	FLevelBrowserItem( ULevel* InLevel )
		: Level( InLevel ),
		  LevelGridVolume( NULL )
	{
		check( Level != NULL );
	}


	/**
	 * Construct this item from a level grid volume
	 *
	 * @param	InLevelGridVolume	Level grid volume that this item will represent
	 */
	FLevelBrowserItem( ALevelGridVolume* InLevelGridVolume )
		: Level( NULL ),
		  LevelGridVolume( InLevelGridVolume )
	{
		check( LevelGridVolume != NULL );
	}


	/**
	 * Returns true if this item represents a level
	 *
	 * @return	True if this item represents a level, otherwise false
	 */
	const UBOOL IsLevel() const
	{
		return Level != NULL;
	}


	/**
	 * Returns true if this item represents a level grid volume 
	 *
	 * @return	True if this item represents a level grid volume, otherwise false
	 */
	const UBOOL IsLevelGridVolume() const
	{
		return LevelGridVolume != NULL;
	}


	/**
	 * Access the level associated with this item.  This will assert if called on a an item that
	 * does not represent a level
	 *
	 * @return	The level that this item represents
	 */
	ULevel* GetLevel()
	{
		check( Level != NULL );
		return Level;
	}


	/**
	 * Access the level grid volume associated with this item.  This will assert if called on a an
	 * item that does not represent a level grid volume
	 *
	 * @return	The level grid volume that this item represents
	 */
	ALevelGridVolume* GetLevelGridVolume()
	{
		check( LevelGridVolume != NULL );
		return LevelGridVolume;
	}


	/** Comparison operator */
	UBOOL operator==( const FLevelBrowserItem& RHS ) const
	{
		return Level == RHS.Level && LevelGridVolume == RHS.LevelGridVolume;
	}



private:

	/** Level */
	ULevel* Level;

	/** Level grid volume */
	ALevelGridVolume* LevelGridVolume;

};



class WxLevelBrowser : public WxBrowser, public FNotifyHook, public FSerializableObject
{
	DECLARE_DYNAMIC_CLASS( WxLevelBrowser );

public:
	WxLevelBrowser();
	virtual ~WxLevelBrowser();

	/**
	 * Generate the new list of levels to display
	 * @param OutLevelList - List of Levels that should be presently displayed
	 */
	void GenerateNewLevelList(TArray<FLevelBrowserItem>& OutLevelList);

	void UpdateLevelList();
	void UpdateLevelPane();
	void UpdateUIForLevel( const ULevel* InLevel );

	////////////////////////////////
	// Level selection

	/**
	 * Clears the level selection, then sets the specified level.  Refreshes.
	 */
	void SelectSingleLevelItem( FLevelBrowserItem InLevelItem );

	/**
	 * Adds the specified level to the selection set.  Refreshes.
	 */
	void SelectLevelItem( FLevelBrowserItem InLevelItem );

	/**
	 * Selects the level (and other levels as appropriate) as if it were shift-clicked upon. Behavior
	 * roughly mimics that of Windows Explorer with some slight differences. Refreshes.
	 *
	 * @param	InLevelItem	Level that was shift-clicked upon
	 */
	void ShiftSelectLevelItem( FLevelBrowserItem InLevelItem );

	/**
	 * Selects all selected levels.  Refreshes.
	 */
	void SelectAllLevels();

	/**
	 * Deselects the specified level.  Refreshes.
	 */
	void DeselectLevelItem( FLevelBrowserItem InLevelItem );

	/**
	 * Deselects all selected levels.  Refreshes.
	 */
	void DeselectAllLevels();

	/**
	 * Selects all currently visible levels.  Refreshes.
	 */
	void SelectAllVisibleLevels();

	/**
	 * Inverts the level selection.  Refreshes.
	 */
	void InvertLevelSelection();

	/**
	 * @return		TRUE if the specified level is selected in the level browser.
	 */
	UBOOL IsItemSelected( FLevelBrowserItem InLevelItem ) const;

	/**
	 * Returns the head of the selection list, or NULL if nothing is selected.
	 */
	FLevelBrowserItem* GetSelectedLevelItem();
	const FLevelBrowserItem* GetSelectedLevelItem() const;

	/**
	 * Returns NULL if the number of selected level is zero or more than one.  Otherwise,
	 * returns the singly selected level.
	 */
	FLevelBrowserItem* GetSingleSelectedLevelItem();

	/**
	 * Returns the number of selected levels.
	 */
	INT GetNumSelectedLevelItems() const;

	////////////////////////////////
	// Selected level iteration

	typedef TArray<FLevelBrowserItem>::TIterator TSelectedLevelItemIterator;
	typedef TArray<FLevelBrowserItem>::TConstIterator TSelectedLevelItemConstIterator;

	TSelectedLevelItemIterator		SelectedLevelItemIterator();
	TSelectedLevelItemConstIterator	SelectedLevelItemConstIterator() const;

	/**
 	 * Makes the specified level the current level.  Refreshes.
	 */
	void MakeLevelCurrent(ULevel* InLevel);

	/**
	 * Makes the specified level grid volume 'current'.  Refreshes.
	 */
	void MakeLevelGridVolumeCurrent( ALevelGridVolume* InLevelGridVolume );


	void RequestUpdate(UBOOL bFullUpdate=FALSE);

	/**
 	 * Requests that we do an update of the level browser on the next frame tick.
	 */
	void RequestDelayedUpdate() { bDoTickUpdate = TRUE; }

	/**
 	 * Merges all visible levels into the persistent level at the top.  All included levels are then removed.
	 *
	 * @param bDiscardHiddenLevels	If TRUE, all hidden levels are discarded when the visible levels are merged; if FALSE, they are preserved
	 *
	 * @param bForceSaveAs			If TRUE, prompts the user to save newly merged level into a different filename. If FALSE, will just remain dirty.		
	 */
	void MergeVisibleLevels( UBOOL bDiscardHiddenLevels, UBOOL bForceSaveAs = TRUE );

	/**
	 * Selects all actors in the selected levels.
	 */
	void SelectActorsOfSelectedLevelItems();

	////////////////////////////////
	// WxBrowser interface

	/**
	* Forwards the call to our base class to create the window relationship.
	* Creates any internally used windows after that
	*
	* @param DockID the unique id to associate with this dockable window
	* @param FriendlyName the friendly name to assign to this window
	* @param Parent the parent of this window (should be a Notebook)
	*/
	virtual void Create(INT DockID,const TCHAR* FriendlyName,wxWindow* Parent);

	virtual void Update();
	virtual void Activated();

	/**
	* Returns the key to use when looking up values
	*/
	virtual const TCHAR* GetLocalizationKey() const
	{
		return TEXT("LevelBrowser");
	}


	/**
 	* Adds entries to the browser's accelerator key table.  Derived classes should call up to their parents.
	*/
	virtual void AddAcceleratorTableEntries(TArray<wxAcceleratorEntry>& Entries);

	////////////////////////////////
	// FCallbackEventDevice interface

	virtual void Send(ECallbackEventType Event);
	virtual void Send(ECallbackEventType Event, UObject* InObject);
	
	/**
	 * Notifies all observers that are registered for this event type
	 * that the event has fired
	 *
	 * @param InType the event that was fired
	 * @param InString the string information associated with this event
	 * @param InObject the object associated with this event
	 */
	virtual void Send(ECallbackEventType InType,const FString& InString, UObject* InObject);

	////////////////////////////////
	// FNotifyHook interface

	/**
	 * Refreshes the level browser if any level properties were modified.  Refreshes indirectly.
	 */
	virtual void NotifyPostChange(void* Src, UProperty* PropertyThatChanged);

	/**
	 * Used to track when the level browser's level property window is destroyed.
	 */
	virtual void NotifyDestroy(void* Src);

	/**
	 * Since this class holds onto an object reference, it needs to be serialized
	 * so that the objects aren't GCed out from underneath us.
	 *
	 * @param	Ar			The archive to serialize with.
	 */
	virtual void Serialize(FArchive& Ar);

	/**
	 * Updates the property window that contains level streaming objects, if it exists.
	 */
	void UpdateLevelPropertyWindow();

	/**
	 * Increases the mutex which controls whether Update() will be executed.  Should be called just before beginning an operation which might result in a
	 * call to Update, if the calling code must manually call Update immediately afterwards.
	 */
	void DisableUpdate();

	/**
	 * Decreases the mutex which controls whether Update() will be executed.
	 */
	void EnableUpdate();

	/**
	 *	Returns TRUE if the displaying of size data is enabled; FALSE otherwise
	 *
	 * @return	TRUE if the displaying of size data is enabled; FALSE otherwise
	 */
	UBOOL IsSizeDataEnabled() const
	{
		return bShouldDisplaySizeData;
	}


	/** Should be called before every tick in the editor to make sure that recently moved/changed actors are
	    moved (copied/pasted) into their most-appropriate levels, if needed */
	void ProcessUpdatesToDeferredActors();

	/**
	 * WxBrowser: Loads the window state for this dockable window
	 *
	 * @param bInInitiallyHidden True if the window should be hidden if we have no saved state
	 */
	virtual void LoadWindowState( const UBOOL bInInitiallyHidden );

	void SuppressUpdates( UBOOL bSuppress ) { bUpdateSuppressed = bSuppress; }

	/**
	 * Used to perform any per-frame actions
	 *
	 * @param DeltaSeconds Time since the last call
	 */
	virtual void Tick( FLOAT DeltaSeconds );


protected:
	/** List of levels currently displayed in the level browser */
	TArray< FLevelBrowserItem > AllLevelItems;
	TArray< FLevelBrowserItem > SelectedLevelItems;

	/** List of actors that have recently moved or changed and may need to be moved to a new level */
	TArray< AActor* > DeferredActorsToUpdateLevelsFor;

	/** True if we should check all actors to see if they should be moved to a new level */
	UBOOL bDeferredUpdateLevelsForAllActors;

	wxPanel*						FilterWindow;
	WxCheckBoxListWindow<FString>*	FilterCheckWindow;
	WxLevelPane*					LevelPane;
	wxScrolledWindow*				RightSidePanel;
	wxSplitterWindow*				SplitterWnd;
	wxSplitterWindow*				LevelFilterSplitter;

	WxPropertyWindowFrame*	LevelPropertyWindow;

	/** TRUE if a full update was requested. */
	UBOOL					bDoFullUpdate;

	/** TRUE if we should do an update in the per-frame tick. */
	UBOOL					bDoTickUpdate;

	/**
	 * TRUE if an update was requested while the browser tab wasn't active.
	 * The browser will Update() the next time this browser tab is Activated().
	 */
	UBOOL					bUpdateOnActivated;

	/** If TRUE, data about the level lightmap, shadowmap, and file size usage will be displayed */
	UBOOL					bShouldDisplaySizeData;

	/** If TRUE, we will ignore update requests - this is useful for large operations that fire off multiple update requests */
	UBOOL					bUpdateSuppressed;

	/**
	 * Displays a property window for the selected levels.
	 */
	void ShowPropertiesForSelectedLevelItems();

	/**
	 * Disassociates all objects from the level property window, then hides it.
	 */
	void ClearPropertyWindow();

	/**
	 * Presents an "Open File" dialog to the user and adds the selected level(s) to the world.  Refreshes.
	 */
	void ImportLevelsFromFile();

	/**
	 * Sets visibility for the selected levels.
	 */
	void SetSelectedLevelVisibility(UBOOL bVisible);

	/**
	 * @param bShowSelected		If TRUE, show only selected levels; if FALSE show only unselected levels.
	 */
	void ShowOnlySelectedLevels(UBOOL bShowSelected);

private:
	/** If greater than 0, ignore calls to Update. */
	INT SuppressUpdateMutex;

	/**
	 * Helper function for shifting level ordering; if a single level is selected, the
	 * level is shifted up or down one position in the WorldInfo streaming level ordering,
	 * depending on the value of the bUp argument.
	 *
	 * @param	bUp		TRUE to shift level up one position, FALSe to shife level down one position.
	 */
	void ShiftSingleSelectedLevel(UBOOL bUp);

	/**
	 * Helper method to split/unsplit the level/filter panes as necessary.
	 *
	 * @param	bShow	If true, the filter pane should be shown, split horizontally above the level pane. If false,
	 *					only the level pane will be shown.
	 */
	void SetFilterWindowVisibility( UBOOL bShow );

	///////////////////
	// Wx events.

		
	/** 
	 * Command handler for the the keyword filter changing
	 *
	 * @param	Event	Information about the event.
	 */
	void FilterChange(wxCommandEvent &Event);

	/** Helper function to force wx to resize the scroll bar */
	void WxLevelBrowser::FixupScrollbar(void);


	void OnSize(wxSizeEvent& In);

	/** 
	 * Helper function to re-layout level pane AND deselect levels that are now hidden due to filtering
	 */
	void LayoutLevelWindows(void);

	/**
	 * Handler for IDM_RefreshBrowser events; updates the browser contents.
	 */
	void OnRefresh(wxCommandEvent& In);

	/**
	 * Called when the user selects the "New Level" option from the level browser's file menu.
	 */
	void OnNewLevel(wxCommandEvent& In);

	/**
	 * Called when the user selects the "New Level From Selected Actors" option from the level browser's file menu.
	 */
	void OnNewLevelFromSelectedActors(wxCommandEvent& In);

	/**
	 * Called when the user selects the "Import Level" option from the level browser's file menu.
	 */
	void OnImportLevel(wxCommandEvent& In);
	void OnRemoveLevelFromWorld(wxCommandEvent& In);

	void OnMakeLevelCurrent(wxCommandEvent& In);
	void OnMergeVisibleLevels(wxCommandEvent& In);
	void OnUpdateLevelsForAllActors( wxCommandEvent& In );
	void OnAutoUpdateLevelsForChangedActors( wxCommandEvent& In );
	void UpdateUI_AutoUpdateLevelsForChangedActors( wxUpdateUIEvent& In );
	void OnSaveSelectedLevels(wxCommandEvent& In);
	void ShowSelectedLevelsInSceneManager(wxCommandEvent& In);
	void ShowSelectedLevelsInContentBrowser(wxCommandEvent& In);

	void MoveActorsToThisLevel(wxCommandEvent& In);

	/**
	 * Selects all actors in the selected levels.
	 */
	void OnSelectAllActors(wxCommandEvent& In);

	void ShiftLevelUp(wxCommandEvent& In);
	void ShiftLevelDown(wxCommandEvent& In);

	/** Event to bring up appMsgf with details on how to add filters */
	void OnFilterStringsDetail(wxCommandEvent& In);
	/** Event to toggle the visiblity of the filter window */
	void OnToggleFilterWindow(wxCommandEvent& In);

	/**
	 * Called when the user checks or unchecks the option to display level size data
	 *
	 * @param	In	Event automatically generated by wxWidgets when the user checks/unchecks the option
	 */
	void OnShowSizeData(wxCommandEvent& In);

	/**
	 * Shows the selected level in a property window.
	 */
	void OnProperties(wxCommandEvent& In);

	void OnShowSelectedLevels(wxCommandEvent& In);
	void OnHideSelectedLevels(wxCommandEvent& In);
	void OnShowAllLevels(wxCommandEvent& In);
	void OnHideAllLevels(wxCommandEvent& In);

	/**
	 * Called when the user elects to lock all of the currently selected levels.
	 *
	 * @param	In	Event generated by wxWidget when the appropriate menu item is selected
	 */
	void OnLockSelectedLevels(wxCommandEvent& In);
	
	/**
	 * Called when the user elects to unlock all of the currently selected levels.
	 *
	 * @param	In	Event generated by wxWidget when the appropriate menu item is selected
	 */
	void OnUnlockSelectedLevels(wxCommandEvent& In);

	/**
	 * Called when the user elects to lock all of the levels.
	 *
	 * @param	In	Event generated by wxWidget when the appropriate menu item is selected
	 */
	void OnLockAllLevels(wxCommandEvent& In);

	/**
	 * Called when the user elects to unlock all of the levels.
	 *
	 * @param	In	Event generated by wxWidget when the appropriate menu item is selected
	 */
	void OnUnlockAllLevels(wxCommandEvent& In);

	void OnSelectAllLevels(wxCommandEvent& In);
	void OnDeselectAllLevels(wxCommandEvent& In);
	void OnSelectAllVisibleLevels(wxCommandEvent& In);
	void OnInvertSelection(wxCommandEvent& In);

	void SetStreamingLevelVolumes(const TArray<ALevelStreamingVolume*>& LevelStreamingVolumes);
	void AddStreamingLevelVolumes(const TArray<ALevelStreamingVolume*>& LevelStreamingVolumes);
	void ClearStreamingLevelVolumes();
	void SelectStreamingLevelVolumes();

	void OnSetStreamingLevelVolumes(wxCommandEvent& In);
	void OnAddStreamingLevelVolumes(wxCommandEvent& In);
	void OnClearStreamingLevelVolumes(wxCommandEvent& In);
	void OnSelectStreamingLevelVolumes(wxCommandEvent& In);

	void OnAssignKeywords (wxCommandEvent& In);

	void OnMouseWheel(wxMouseEvent& In);

	void OnShowOnlySelectedLevels(wxCommandEvent& In);
	void OnShowOnlyUnselectedLevels(wxCommandEvent& In);

	DECLARE_EVENT_TABLE();
};

#endif // __LEVELBROWSER_H__
