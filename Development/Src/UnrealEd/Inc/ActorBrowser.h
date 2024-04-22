/*=============================================================================
	ActorBrowser.h: UnrealEd's actor browser.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ACTORBROWSER_H__
#define __ACTORBROWSER_H__

/**
 * Derived WxTreeCtrl with custom save and restore expansion state
 */
class WxActorBrowserTreeCtrl : public WxTreeCtrl
{
public:
	/** 
	 * Loops through all of the elements of the tree and adds selected items to the selection set,
	 * and expanded items to the expanded set.
	 */
	virtual void SaveSelectionExpansionState();

	/** 
	 * Loops through all of the elements of the tree and sees if the client data of the item is in the 
	 * selection or expansion set, and modifies the item accordingly.
	 */
	virtual void RestoreSelectionExpansionState();
protected:
	/** 
	 * Recursion function that loops through all of the elements of the tree item provided and saves their select/expand state. 
	 * 
	 * @param Item Item to use for the root of this recursion.
	 */
	virtual void SaveSelectionExpansionStateRecurse(wxTreeItemId& Item);

	/** 
	 * Recursion function that loops through all of the elements of the tree item provided and restores their select/expand state. 
	 * 
	 * @param Item Item to use for the root of this recursion.
	 */
	virtual void RestoreSelectionExpansionStateRecurse(wxTreeItemId& Item);

private:

	/** Saved state of client data objects that were selected. Uses ClassIndices instead of object ptrs */
	TMap<INT, INT> ActorBrowserSavedSelections;

	/** Saved state of client data objects that were expanded. Uses ClassIndices instead of object ptrs */
	TMap<INT, INT> ActorBrowserSavedExpansions;

	DECLARE_DYNAMIC_CLASS(WxActorBrowserTreeCtrl);
};



class WxActorBrowser : public WxBrowser, public FSerializableObject
{
	DECLARE_DYNAMIC_CLASS(WxActorBrowser);

public:
	WxActorBrowser();
	~WxActorBrowser();

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

	/** Builds a category based treeview starting at the passed in index */
	void BuildClassGroups( INT RootClassIndex );

	void RepopulateClassTree();
	void AddChildren( wxTreeItemId InID );

	/**
	 * Returns the key to use when looking up values
	 */
	virtual const TCHAR* GetLocalizationKey(void) const
	{
		return TEXT("ActorBrowser");
	}

	/* === FSerializableObject interface === */
	virtual void Serialize( FArchive& Ar );

	// FCallbackEventDevice interface
	virtual void Send(ECallbackEventType InType);
	virtual void Send(ECallbackEventType InType, UObject* InObject);

protected:
	wxCheckBox *ActorAsParentCheck;
	wxCheckBox *PlaceableCheck;
	wxCheckBox *ShowCategoriesCheck;

	WxActorBrowserTreeCtrl *TreeCtrl;
	wxStaticText *FullPathStatic;

	/** Text control for search filter */
	wxTextCtrl* SearchTextCtrl;

	/** Button to clear search filter when clicked */
	wxBitmapButton* ClearSearchButton;

	/** Bitmap to use for the clear search filter button when it is enabled */
	WxBitmap ClearB;

	/** Bitmap to use for the clear search filter button when it is disabled */
	WxBitmap ClearDisabledB;

	/** Refresh timer for search filter box */
	wxTimer SearchTimer;

	/** The class that was last right clicked */
	UClass* RightClickedClass;

	/**
	 * TRUE if an update was requested while the browser tab wasn't active.
	 * The browser will Update() the next time this browser tab is Activated().
	 */
	UBOOL bUpdateOnActivated;

	/** TRUE when we want to update actor classes on next update. */
	UBOOL bForceActorClassesUpdate;

	/** Set of Class indices which are unfiltered and should be shown in the tree */
	TSet<INT> ClassIndicesToShow;

	/** Set of Class indices which should be made to auto-expand, likely because they passed a specific filter */
	TSet<INT> ClassIndicesToExpand;

	/** To prevent multiple internal repopulation, maintain whether or not we're already doing so */	
	UBOOL bIsRepopulating;

private:
	/**
	 * Helper method to filter the class tree by the various filters that might be in effect
	 *
	 * @param	InRootIndex	Index of the class to use as the tree's root
	 */
	void FilterClasses( INT InRootIndex );

	/**
	 * Returns TRUE if any filtering is in effect, FALSE otherwise
	 *
	 * @return	TRUE if any filtering is in effect; FALSE otherwise
	 */
	UBOOL IsFiltering() const;

	/**
	 * Returns TRUE if the provided class index is filtered out under the current filters
	 *
	 * @param	InClassIndex	Class index to check
	 *
	 * @return	TRUE if the provided class index represents a class that is filtered out under the current filters
	 */
	UBOOL IsIndexFiltered( INT InClassIndex ) const;

	/**
	 * Returns TRUE if the provided class index has children which pass all current filters
	 *
	 * @param	InClassIndex	Class index to check
	 *
	 * @return	TRUE if the class represented by the provided class index has at least one child that passes
	 *			all filters currently in effect; FALSE if the class has no children that pass the filters
	 */
	UBOOL IndexHasFilterPassingChildren( INT InClassIndex ) const;

	/**
	 * Returns TRUE if the provided class index passes the provided text filter (or the filter is empty)
	 *
	 * @param	InClassIndex	Class index to check
	 * @param	TextFilter		Text filter to check the class name against
	 *
	 * @return	TRUE if the class represented by the provided class index passed the provided text filter
	 */
	UBOOL PassesTextFilter( INT InClassIndex, const FString& TextFilter ) const;

	/**
	 * Returns TRUE if the provided class index is not a brush
	 *
	 * @param	InClassIndex	Class index to check
	 *
	 * @return	TRUE if the class represented by the provided class index is not a brush
	 */
	UBOOL PassesBrushFilter( INT InClassIndex ) const;

	/**
	 * Returns TRUE if the provided class index passes the placeable filter (or the placeable filter is disabled)
	 *
	 * @param	InClassIndex	Class index to check
	 *
	 * @return	TRUE if the class represented by the provided class index passed the placeable filter; FALSE if it did not
	 */
	UBOOL PassesPlaceableFilter( INT InClassIndex ) const;

	UINT AddUngroupedChildren( INT ClassIndex, const wxTreeItemId& ParentId, TSet<INT>& ClassIndicesAlreadyAdded );
	void AddUngroupedChildrenInternal( INT ClassIndex, const wxTreeItemId& ParentId, TSet<INT>& ClassIndicesAlreadyAdded );

	void OnFileOpen( wxCommandEvent& In );
	void OnFileExportAll( wxCommandEvent& In );
	void OnItemExpanding( wxCommandEvent& In );
	void OnSelChanged( wxCommandEvent& In );
	/** Creates an archetype from the actor class selected in the Actor Browser. */
	void OnCreateArchetype( wxCommandEvent& In );
	void OnItemRightClicked( wxCommandEvent& In );
	void OnUseActorAsParent( wxCommandEvent& In );
	void OnPlaceableClassesOnly( wxCommandEvent& In );
	void OnShowCategories( wxCommandEvent& In );
	void OnBeginDrag( wxCommandEvent& In );

	/** Called when the search timer goes off */
	void OnSearchTimer( wxTimerEvent& In );

	/** Called whenever the search text is changed; kicks off the search timer */
	void OnSearchTextUpdated( wxCommandEvent& In );

	/** Called whenever the "clear" button is clicked */
	void OnClearSearchTextClicked( wxCommandEvent& In );

	/** Called whenever the user presses the enter key in the search text box */
	void OnSearchTextEnter( wxCommandEvent& In );

	/** Called to update the UI for the clear search text button */
	void UpdateUI_ClearSearchTextButton( wxCommandEvent& In );

	DECLARE_EVENT_TABLE();
};

#endif // __ACTORBROWSER_H__
