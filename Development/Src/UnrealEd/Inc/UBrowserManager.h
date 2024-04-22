/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This holds the native implmentation of BrowserManager.uc
 */

	typedef TArray<FBrowserPaneInfo> FBrowserPaneInfoArray;
	typedef TArray<WxBrowser*> FBrowserArray;
	typedef TArray<WxFloatingFrame*> FFloatingFrameArray;

private:
	/**
	 * Returns the floating window array. Creates it if it is missing
	 */
	inline FFloatingFrameArray& GetFloatingWindows(void)
	{
		if (FloatingWindowsArrayPtr == NULL)
		{
			FloatingWindowsArrayPtr = new FFloatingFrameArray;
		}
		return *(FFloatingFrameArray*)FloatingWindowsArrayPtr;
	}

	/**
	 * Returns an array of browser pointers; creates an empty array if it is missing.
	 */
	inline FBrowserArray& GetCreatedPanes(void)
	{
		if (CreatedPanesPtr == NULL)
		{
			CreatedPanesPtr = new FBrowserArray;
		}
		return *reinterpret_cast<FBrowserArray*>(CreatedPanesPtr);
	}
	
	/**
	 * Returns the docking container. Creates it if it is missing
	 */
	inline WxDockingContainer* GetDockingContainer(void)
	{
		if (DockingContainerPtr == NULL)
		{
			extern wxWindow* GetEditorFrame(void);
			DockingContainerPtr = new WxDockingContainer(GetEditorFrame(),-1);
		}
		return (WxDockingContainer*)DockingContainerPtr;
	}

	/**
	 * Adds the specified pane to the browser menu
	 *
	 * @param Pane the browser pane to add to the menu
	 */
	void AddToMenu(WxBrowser* Pane);

protected:
	/**
	 * Returns the number of clones of the specified pane
	 *
	 * @param PaneID the pane id that has been cloned
	 */
	inline INT GetCloneCount(INT PaneID)
	{
		INT Count = 0;
		// Seach each entry for matching IDs
		for (INT Index = 0; Index < BrowserPanes.Num(); Index++)
		{
			// Increment our count if this is a match
			if (BrowserPanes(Index).CloneOfPaneID == PaneID)
			{
				Count = Max<INT>(Count,BrowserPanes(Index).CloneNumber);
			}
		}
		return Count;
	}

	/**
	 * Returns the next pane id to use
	 */
	inline INT GetNextPaneID(void)
	{
		INT NextID = 0;
		// Get the max of all the pane IDs
		for (INT Index = 0; Index < BrowserPanes.Num(); Index++)
		{
			NextID = Max<INT>(NextID,BrowserPanes(Index).PaneID);
		}
		return NextID + 1;
	}

	/**
	 * Gets the notebook that browser panes will be docked in
	 */
	inline wxWindow* GetNotebook(void)
	{
		return GetDockingContainer() ? GetDockingContainer()->GetNotebook() : NULL;
	}

	/**
	 * Returns the list of registered browser panes
	 */
	inline const FBrowserPaneInfoArray& GetBrowserPaneList(void) const
	{
		return BrowserPanes;
	}

	/**
	 * Creates all of the browser windows that are registered with the manager.
	 * Also, loads their persistent state for them
	 *
	 * @param Parent the parent window to use for all browser panes
	 */
	void CreateBrowserPanes(wxWindow* Parent);

	/**
	 * Creates one browser window.
	 * Also, loads their persistent state for them
	 *
	 * @param Parent the parent window to use for all browser panes
	 * @param bInInitiallyHidden True if the window should be initially hidden if we have no other loaded state
	 */
	void CreateSingleBrowserPane(wxWindow* InParent, const FString& InWxName, const FString& InFriendlyName, const UBOOL bInInitiallyHidden, const INT InPaneID, const INT InCloneOfPaneID, const INT InCloneNumber, OUT FPointer& OutWxBrowserPtr);

	/**
	 * Attaches a floating frame window to the specified browser window
	 *
	 * @param BrowserPane the browser pane to create a floating frame for
	 */
	void AttachFrame(WxBrowser* BrowserPane);

	/**
	 * Attaches the browser window to the docked window set
	 *
	 * @param BrowserPane the browser pane to add to the docked container
	 */
	void AttachDocked(WxBrowser* BrowserPane);

	/**
	 * Destroys all browser panes
	 */
	void DestroyBrowserPanes(void);

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
	WxBrowser* CloneBrowserPane(wxWindow* Parent,INT PaneID);

	/**
	 * Handles removing a pane that is floating. Breaks the link between the
	 * parent->child and destroyes the frame. Note this does not destroy the
	 * browser, just the frame
	 *
	 * @param PaneID the ID of the pane to clean up it's frame for
	 */
	void RemoveFloatingBrowser(INT PaneID);

	/**
	 * Handles removing a pane that is docked. Removes the docked page. Note
	 * it doesn't destroy the pane just the page that was holding it
	 *
	 * @param PaneID the ID of the pane to remove from the docking container
	 */
	void RemoveDockedBrowser(INT PaneID);

public:
	/**
	 * Initializes all of the window states with the list of brower panes that
	 * need to have their container window's created.
	 */
	virtual void Initialize(void);

	/**
	 * Show the browser windows that were visible when the editor was last closed.
	 */
	void RestoreBrowserWindows();

	/**
	 * Cleans up all of the resources created by this instance
	 */
	void DestroyBrowsers(void);

	/**
	 * Returns the WxBrowser for the specified ID
	 *
	 * @param PaneID the browser pane id to search for
	 */
	WxBrowser* GetBrowserPane(INT PaneID);

	/**
	 * Returns the WxBrowser for the specified friendly name
	 *
	 * @param FriendlyName the friendly name of the window to search for
	 * @param bCannonicalOnly whether to include clones or not
	 */
	WxBrowser* GetBrowserPane(const TCHAR* FriendlyName,UBOOL bCannonicalOnly = TRUE);

	/**
	 * Adds another copy of the specified browser pane to the list of registered
	 * browser panes. Automatically creates a unique pane id. Also saves the
	 * configuration so that the settings are remembered for subsequent sessions.
	 *
	 * @param PaneID the window pane that is being cloned
	 */
	void CloneBrowser(INT PaneID);

	/**
	 * Removes a browser pane from the list of registered panes. If this is a
	 * "canonical" pane, then it just hides the pane. If it is a clone, it
	 * removes the entry and saves the configuration.
	 *
	 * @param PaneID the browser pane to remove from the list
	 *
	 * @return TRUE if it was removed, FALSE if it should just be hidden
	 */
	UBOOL RemoveBrowser(INT PaneID);

	/**
	 * Locates a dockable window and makes sure it is visible.
	 *
	 * @param InDockID The window to show/hide
	 * @param bShowHide TRUE to show the window FALSE to hide
	 */
	void ShowWindow(INT InDockID,UBOOL bShowHide);

	/**
	 * Adds the specified browser window to the docking container and kills
	 * the floating frame
	 *
	 * @param PaneID the 
	 */
	void DockBrowserWindow(INT PaneID);
	
	/**
	 * Removes the specified browser window from the docking container and creates
	 * a floating frame for it
	 *
	 * @param PaneID the 
	 */
	void UndockBrowserWindow(INT PaneID);

	/**
	 * Tells the browser manager to save its state
	 */
	void SaveState(void);

	/**
	 * Determines whether this is a canonical browser or not
	 *
	 * @param PaneID the browser to see if it is a clone or original
	 */
	UBOOL IsCanonicalBrowser(INT PaneID);

	/**
	 * Uses the menu id to index into the browser list and makes sure that
	 * window is shown
	 *
	 * @param MenuID the id of the menu that corresponds to a browser pane
	 */
	void ShowWindowByMenuID(INT MenuID);


	/** Makes sure that the main docking container window is visible */
	void ShowDockingContainer();


	/**
	 * Adds the browser windows to the browser menu. Caches a pointer to the
	 * menu so that it can make changes to the menu as browsers are created
	 * and removed.
	 *
	 * @param InMenu the menu to modify with browser settings
	 */
	void InitializeBrowserMenu(wxMenu* InMenu);

	/**
	 * Returns the number of browser panes that are in the browser list
	 */
	inline INT GetBrowserCount(void)
	{
		return BrowserPanes.Num();
	}
