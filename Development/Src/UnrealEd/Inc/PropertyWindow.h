
/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PROPERTY_WINDOW_H__
#define __PROPERTY_WINDOW_H__

#include "PropertyNode.h"
#include "ObjectsPropertyNode.h"
#include "CategoryPropertyNode.h"

namespace WxPropertyWindowConstants
{
	const FLOAT DefaultSplitterProportion = 0.5f;
};

//For shifting nodes or focus
namespace EPropertyWindowDirections
{
	typedef INT Type;

	const Type Up = -1;
	const Type Down = 1;
};

class WxFixedSplitter;
///////////////////////////////////////////////////////////////////////////////
//
// Property window.
//
///////////////////////////////////////////////////////////////////////////////

/**
 * Top level window for any property window (other than the optional WxPropertyWindowFrame).
 * WxPropertyWindow objects automatically register/unregister themselves with GPropertyWindowManager
 * on construction/destruction.
 */
class WxPropertyWindow : public wxPanel, public FCallbackEventDevice, public FDeferredInitializationWindow
{
public:
	DECLARE_DYNAMIC_CLASS(WxPropertyWindow);

	/** Destructor */
	virtual ~WxPropertyWindow();

	/**
	 * Initialize this window class.  Must be the first function called after creation.
	 *
	 * @param	parent			The parent window.
	 * @param	InNotifyHook	An optional callback that receives property PreChange and PostChange notifies.
	 */
	virtual void Create( wxWindow* InParent, FNotifyHook* InNotifyHook );
	/**
	 * Something has made the tree change topology.  Rebuild the part of the tree that is no longer valid
	 */
	void RebuildSubTree(FPropertyNode* PropertyTreeNode);

	/**
	 * Requests that the window mark itself for data validity verification
	 */
	void RequestReconnectToData(void);

	/**
	 * Requests main window to take focus
	 */
	void RequestMainWindowTakeFocus (void);

	//FILTERING
	/*
	 * Sets the filtering of the property window.  
	 * @param InFilterString - Can contain multiple space delimited sub-strings that must all be found in a property
	 */
	void SetFilterString(const FString& InFilterString);

	/**
	 * Forces a refresh of filtering for the hierarchy
	 */
	void FilterWindows(void);


	/**
	 * Quickly ensure all the correct windows are visible
	 */
	void RefreshEntireTree(void);

	/**
	 * Calls refresh entire tree on any window that has been marked for deferred update
	 */
	void DeferredRefresh(void);

	/*
	 * Returns the current list of Filter sub-strings that must match to be displayed
	 */
	const TArray<FString>& GetFilterStrings(void) const { return FilterStrings; }
	/**
	 * Sets an object in the property window.  T must be derived from UObject; will not
	 * compile if this is not the case.
	 *
	 * @param	InObject				The object to bind to the property window.  Can be NULL.
	 * @param	InWindowFlags is a some combination of EPropertyWindowFlags
	 */
	void SetObject( UObject* InObject, const UINT InWindowFlags);

	/**
	 * Sets multiple objects in the property window.  T must be derived from UObject; will not
	 * compile if this is not the case.
	 *
	 * @param	InObjects				An array of objects to bind to the property window.
	 * @param	InWindowFlags is a some combination of EPropertyWindowFlags
	 */
	template< class T >
	void SetObjectArray(const TArray<T*>& InObjects, const UINT InWindowFlags)
	{
		//if this is happening DURING a callback to this property window bad things could happen.  Bail.
		if (GetActiveCallbackCount() > 0)
		{
			RequestReconnectToData();
			return;
		}

		Freeze();
		PreSetObject();

		UBOOL bOwnedByLockedLevel = FALSE;
		for( INT ObjectIndex = 0 ; ObjectIndex < InObjects.Num() ; ++ObjectIndex )
		{
			if ( InObjects(ObjectIndex) && (GWorld == NULL || ( GWorld->HasBegunPlay() || InObjects(ObjectIndex) != static_cast<UObject*>(GWorld->GetBrush()) ) ) )
			{
				if ( !bOwnedByLockedLevel )
				{
					AActor* ActorObj = Cast<AActor>(InObjects(ObjectIndex));
					if ( ActorObj != NULL && !ActorObj->IsTemplate() && FLevelUtils::IsLevelLocked(ActorObj->GetLevel()) )
					{
						bOwnedByLockedLevel = TRUE;
					}
				}
				PropertyTreeRoot->AddObject( static_cast<UObject*>( InObjects(ObjectIndex) ) );
			}
		}
		SetFlags(EPropertyWindowFlags::ReadOnly, bOwnedByLockedLevel);
		PostSetObject( InWindowFlags );
		Thaw();
	}

	/** @return		The base-est baseclass for objects in this list. */
	UClass*			GetObjectBaseClass()
	{
		check(PropertyTreeRoot);
		return PropertyTreeRoot->GetObjectBaseClass();
	}
	/** @return		The base-est baseclass for objects in this list. */
	const UClass*	GetObjectBaseClass() const
	{
		check(PropertyTreeRoot);
		return PropertyTreeRoot->GetObjectBaseClass();
	}

	static FString WxPropertyWindow::GetContextName(const UClass* InClass)
	{
		FString ContextName;
		if (InClass)
		{
			if (InClass->IsChildOf(AActor::StaticClass()))
			{
				//all actors get the same regardless of what specific subclass
				ContextName = TEXT("Actor");
			}
			else
			{
				//every other class gets custom
				ContextName = InClass->GetName();
			}
		}
		return ContextName;
	}

	//////////////////////////////////////////////////////////////////////////
	// Object access
	typedef FObjectPropertyNode::TObjectIterator		TObjectIterator;
	typedef FObjectPropertyNode::TObjectConstIterator	TObjectConstIterator;

	TObjectIterator			ObjectIterator()			{ return PropertyTreeRoot->ObjectIterator(); }
	TObjectConstIterator	ObjectConstIterator() const	{ return PropertyTreeRoot->ObjectConstIterator(); }

	/** Returns the number of objects for which properties are currently being edited. */
	INT GetNumObjects() const
	{
		check(PropertyTreeRoot);
		return PropertyTreeRoot->GetNumObjects(); 
	}

	/** Utility for finding if a particular object is being edited by this property window. */
	UBOOL					IsEditingObject(const UObject* InObject) const;

	//////////////////////////////////////////////////////////////////////////
	// Moving between items


	//////////////////////////////////////////////////////////////////////////
	// Item expansion
	TMultiMap<FName,FString> ExpandedItems;

	/**
	 * Recursively searches through children for a property named PropertyName and expands it.
	 * If it's a UArrayProperty, the propery's ArrayIndex'th item is also expanded.
	 */
	void ExpandItem( const FString& InPropertyName, INT InArrayIndex=INDEX_NONE, const UBOOL bExpandParents=FALSE);

	/**
	 * Expands all items in this property window.
	 */
	void ExpandAllItems(void);

	/**
	 * Called when the object list is finalized, Finalize() finishes the property window setup.
	 */
	void Finalize()
	{
		check(PropertyTreeRoot);
		PropertyTreeRoot->Finalize();
	}


	FPropertyNode* FindPropertyNode(const FString& InPropertyName) const;

	/**
	 * Recursively searches through children for a property named PropertyName and collapses it.
	 * If it's a UArrayProperty, the propery's ArrayIndex'th item is also collapsed.
	 */
	void CollapseItem( const FString& InPropertyName, INT InArrayIndex=INDEX_NONE);

	/**
	 * Collapses all items in this property window.
	 */
	void CollapseAllItems();

	/**
	 * Returns if the property window is using the specified object in the root node of the hierarchy
	 * @param InObject - The object to check for
	 * @return - TRUE if the object is contained within the root node
	 */
	UBOOL ContainsObject (const UObject* InObject) const;

	/**
	 * Rebuild all the properties and categories, with the same actors 
	 *
	 * @param IfContainingObject Only rebuild this property window if it contains the given object in the object hierarchy
	 */
	void Rebuild(UObject* IfContainingObject = NULL);

	/**
	 * Refresh all the properties and categories in the visible windows 
	 *
	 */
	void RefreshVisibleWindows();

	/**
	 * Queues a message in the property window message queue to Rebuild all the properties and categories, with the same actors,  
	 *
	 * @param IfContainingObject Only rebuild this property window if it contains the given object in the object hierarchy
	 */
	void PostRebuild(UObject* IfContainingObject = NULL);

	/**
	 * Freezes rebuild requests, so we do not process them.  
	 */
	void FreezeRebuild();

	/**
	 * Resumes rebuild request processing.
	 *
	 * @param bRebuildNow	Whether or not to rebuild the window right now if rebuilding is unlocked.
	 */
	void ThawRebuild(UBOOL bRebuildNow=TRUE);

	//////////////////////////////////////////////////////////////////////////
	// Change notification

	/**
	 * Calls PreEditChange on Root's objects, and passes PropertyAboutToChange to NotifyHook's NotifyPreChange.
	 */
	void NotifyPreChange(WxPropertyControl* InItem, UProperty* PropertyAboutToChange, UObject* Object);

	/**
	 * Calls PostEditChange on Root's objects, and passes PropertyThatChanged to NotifyHook's NotifyPostChange.
	 * @param bChangesTopology - will cause a refresh or a full rebuild depending on the type of change that was made
	 */
	void NotifyPostChange(WxPropertyControl* InItem, FPropertyChangedEvent& InPropertyChangedEvent);

	/**
	 * Passes a pointer to this property window to NotifyHook's NotifyDestroy.
	 */
	void NotifyDestroy();


	/**
	 * Routes the event to the appropriate handlers
	 *
	 * @param InType the event that was fired
	 * @param InFlags the flags for this event
	 */
	void Send(ECallbackEventType InType);

	/** 
	 * For quitting PIE or between loadinng maps.  Objects from content packages do not need to be cleared. 
	 */
	void ClearIfFromMapPackage(void);

	//////////////////////////////////////////////////////////////////////////
	// Splitter
	/**
	 * @return		The current splitter position.
	 */
	INT GetSplitterPos() const			{ return SplitterPos; }

	/**
	 * Moves the splitter position by the specified amount.
	 */
	void MoveSplitterPos(INT Delta)		{ IdealSplitterPos += Delta; }

	/**
	 * @return		TRUE if the splitter is currently being dragged.
	 */
	UBOOL IsDraggingSplitter() const		{ return bDraggingSplitter; }

	void StartDraggingSplitter()			{ bDraggingSplitter = 1; IdealSplitterPos = SplitterPos; }
	void StopDraggingSplitter()				{ bDraggingSplitter = 0; }

	/** If true, favorites are displayed by the parent host window*/
	UBOOL IsFavoritesFeatureEnabled (void) const;

	//////////////////////////////////////////////////////////////////////////
	// Last focused

	/** Returns the last property that had focus. */
	WxPropertyControl* GetLastFocused()						
	{ 
		return LastFocused; 
	}

	/** Returns the last property that had focus. */
	const WxPropertyControl* GetLastFocused() const			
	{ 
		return LastFocused; 
	}


	/** Sets the last property that had focus. */
	void SetLastFocused(WxPropertyControl* InLastFocused)	
	{ 
		LastFocused = InLastFocused; 
	}

	/** Clears reference to the last property that had focus. */
	void ClearLastFocused()										
	{ 
		LastFocused = NULL; 
	}

	/**
	 * Flushes the last focused item to the input proxy.
	 */
	void FlushLastFocused();

	/**
	 * Relinquishes focus to LastFocused.
	 */
	void FinalizeValues();

	/**
	 * Returns the number of active callbacks for this windows.  0 means it's ok to destroy
	 */
	UBOOL GetActiveCallbackCount (void) const { return PerformCallbackCount; }

	/**
	 * Increments/decrements the "PerformCallbackCount" to guard around destroying windows during a callback
	 */
	void ChangeActiveCallbackCount (const INT InDeltaCallbackCount)
	{
		PerformCallbackCount += InDeltaCallbackCount;
		check(PerformCallbackCount >= 0 );
	}

	/**
	 * Looks up the expansion tree to determine the hide/show status of the specified item.
	 *
	 * @param		InItem		The item for which to determine visibility status.
	 * @return					TRUE if InItem is showing, FALSE otherwise.
	 */
	UBOOL IsItemShown( WxPropertyControl* InItem );

	/**
	 * Builds an ordered list of nested UProperty objects from the specified property up through the
	 * object tree, as specified by the property window hierarchy.
	 * The list tail is the property that was modified.  The list head is the class member property within
	 * the top-most property window object.
	 */
	static FEditPropertyChain* BuildPropertyChain(WxPropertyControl* InItem, UProperty* PropertyThatChanged);

	/**
	 * Updates the scrollbar values.
	 */
	void UpdateScrollBar();

	/*
	 * Set/Get the scroll bar position and max range
	 */
	void SetScrollBarPos(INT Position);
	INT GetScrollBarPos();
	INT GetScrollBarMax();

	//////////////////////////////////////////////////////////////////////////
	// Event Handling

	void OnRefresh( wxCommandEvent& In );
	void OnScroll( wxScrollEvent& In );
	void OnMouseWheel( wxMouseEvent& In );
	void OnSize( wxSizeEvent& In );
	void OnKeyDown(wxKeyEvent &In);
	void OnIdle(wxIdleEvent &In);

	FObjectPropertyNode* GetRoot() const { return PropertyTreeRoot; }

	//////////////////////////////////////////////////////////////////////////
	//Flags
	UINT HasFlags (const EPropertyWindowFlags::Type InTestFlags) const				{ return PropertyWindowFlags & InTestFlags; }
	/**
	 * Sets the flags used by the window and the root node
	 * @param InFlags - flags to turn on or off
	 * @param InOnOff - whether to toggle the bits on or off
	 */
	void  SetFlags (const EPropertyWindowFlags::Type InFlags, const UBOOL InOnOff);

	/**
	 * Serializes the root WxObjectsPropertyControl object contained in the window.
	 *
	 * @param		Ar		The archive to read/write.
	 */
	void Serialize( FArchive& Ar );

	/**
	 * Event handler for showing/hiding all of the property item buttons.
	 */
	void OnSetShowHiddenProperties(wxCommandEvent &Event);

	/**
	 * Sets the vertical position of the main window
	 */
	void SetThumbPos (const INT InThumbPos) { ThumbPos = InThumbPos; }

	/** Gets the max size of the property window */
	INT GetMaxHeight (void) const { return MaxH; }

	/** Rebuild the focus array to go through all open children */
	void AppendFocusWindows (OUT TArray<wxWindow*>& FocusArray);

	/**
	 * Used to hide expansion widgets and disallow expansion/collapsing when using a forced filter
	 */
	UBOOL IsNormalExpansionAllowed (const FPropertyNode* InNode);

	/**Helper Function to get the parent Property Host Window*/
	WxPropertyWindowHost* GetParentHostWindow(void);
	const WxPropertyWindowHost* GetParentHostWindow(void) const;

	/** 
	 * Mark Favorites of child nodes.  This function takes a Node so that rebuilding sub trees can be faster
	 * @param InRootOfFavoritesUpdate - The root for which to remark favorites.  NULL, means the entire tree.
     * @param bUseFavoriteIndices - Flag to alter or initialize favorite property indices for sorting. Defaults to false.
	 */
	void MarkFavorites (FPropertyNode* InRootOfFavoritesUpdate);

	/**
	 * Sets favorites for this class, then tells the host to refresh as needed
	 * @param InNode - Node to be marked as a favorite
	 * @param bInShouldBeFavorite - TRUE if this node should be marked as a favorite or removed from the list (if it exists in the list)
	 */
	void SetFavorite (FPropertyNode* InNode, const UBOOL bInShouldBeFavorite);

	/**
	 * Pastes property values from clipboard into currently open object
	 */
	void PastePropertyValuesFromClipboard(FPropertyNode* InNode, TArray<FString>& NameValuePairs);

	/**
	 * Saves splitter proportion to proper context name
	 * @param InClass - The class will provide the context for saving
	 */
	void SaveSplitterProportion(UClass* InClass);

	/**
	 * Loads splitter proportion to proper context name
	 * @param InClass - The class will provide the context for loading
	 */
	void LoadSplitterProportion(UClass* InClass);

	/**
	 * Begins at the Root node of this window and gathers all favorite
	 * nodes parented to the window, searching for the nearest matching
	 * FavoriteSortIndex in the given direction.
	 * @param InDirection - Direction to search relative to the given index
	 * @param PropertyNode - Relative node to search from.
	 */
	FPropertyNode* FindNearestFavoriteNode(const EPropertyWindowDirections::Type InDirection, FPropertyNode& InNode) const;
	
	/**
	 * Saves the given nodes for inclusion in favorites, including their respective FavoriteSortIndex.
	 * @param InNode1 - Node to save
	 * @param InNode2 - Node to save
	 */
	void SaveFavoriteNodes(const FPropertyNode& InNode1, const FPropertyNode& InNode2);

	DECLARE_EVENT_TABLE();

protected:
	/** 
	 * Mark Favorites of child nodes.
	 * @param InNode - The root for which to remark favorites.  NULL, means the entire tree.
	 * @param InFavoritesMap - Map of all favorites and their sort indices for this class
	 * @param bUseFavoriteIndices - Flag to alter or initialize favorite property indices for sorting. Defaults to false.
	 * @return - TRUE if any settings have changed during this marking
	 */
	UBOOL MarkFavoritesInternal (FPropertyNode* InNode, TMap<FString,INT>& InFavoritesMap);

	/**
	 * Save Favorites to ini file
	 * @param InFavoritesMap - Map of favorite nodes by qualified names and favorite indices
	 */
	void SaveFavorites(const TMap<FString,INT>& InFavoritesMap) const;

	/**
	 * Load Favorites from ini file
	 * @param OutFavoritesMap - Map of favorite nodes by qualified names and favorite indices
	 */
	void LoadFavorites(TMap<FString,INT>& OutFavoritesMap) const;

	/**
	 * Positions existing child items so they are in the proper positions, visible/hidden, etc.
	 */
	void PositionChildren();

	/** Flag for whether or not the ACTOR DESELECT command is executed when the Escape key is pressed. */
	UBOOL bExecDeselectOnEscape;

	/** The position of the break between the variable names and their values/editing areas. */
	INT SplitterPos;
	/**The ideal position last requested by the user.  Resizing the window may have forced it smaller, and this let's the splitter rebound*/
	INT IdealSplitterPos;

	/** Cached value of the proportion of the splitter used to showing the property name (e.g. if the value is 0.33f, 1/3rd of the prop window is used to show the name) */
	FLOAT CachedSplitterProportion;

	/** The item window that last had focus. */
	WxPropertyControl* LastFocused;

	/** If non-zero, we do not process rebuild requests, this should be set before operations that may update the property window multiple times and cleared afterwards. */
	INT RebuildLocked;

	/** If true, a refresh is desired but shouldn't be processed */
	UBOOL bDeferredRefresh;

	/** If 1, the user is dragging the splitter bar. */
	UBOOL bDraggingSplitter;

	/**
	 * Flags for how to display the Window
	 */
	UINT PropertyWindowFlags;

	wxScrollBar* ScrollBar;
	INT ThumbPos;
	INT MaxH;

	/* @List of strings that must be in each property name in order to display
	 *					Empty FilterStrings means display as normal
	 *					All strings must match in order to be accepted by the filter
	 */
	TArray<FString> FilterStrings;

	FNotifyHook* NotifyHook;

	/** 
	 * Command handler for the rebuild event.
	 *
	 * @param	Event	Information about the event.
	 */
	virtual void OnRebuild(wxCommandEvent &Event);

private:
	/**
	 * Save off what nodes in the tree are expanded
	 */
	void SaveExpandedItems (void);
	/**
	 * Restore what nodes in the tree are expanded
	 */
	void RestoreExpandedItems (void);

	/**
	 * Performs necessary tasks (remember expanded items, etc.) performed before setting an object.
	 */
	void PreSetObject();
	
	/**
	 * Performs necessary tasks (restoring expanded items, etc.) performed after setting an object.
	 */
	void PostSetObject(const UINT InWindowFlags);

	/**
	 * Calculates the splitter location that would result from the provided proportion of the window being devoted
	 * to showing the property title
	 *
	 * @param	RequestedProportion	Percentage amount of the window that should be devoted to displaying the property title [0.0f-1.0f]
	 *
	 * @return	The splitter position necessary to satisfy the requested proportion
	 */
	INT CalculateSplitterPosFromProportion( FLOAT RequestedProportion );

	/**
	 * Calculates the proportion of the window devoted to showing the property title from the given splitter position
	 *
	 * @param	RequestedPos	Splitter position to calculate proportion for
	 *
	 * @return	Proportion of the window [0.0f-1.0f] which would be devoted to showing the property title with the given splitter position
	 */
	FLOAT CalculateSplitterProportionFromPos( INT RequestedPos );

	/**
	 * Recursive minion of PositionChildren.
	 *
	 * @param	InX		The horizontal position of the child item.
	 * @param	InY		The vertical position of the child item.
	 * @return			An offset to the current Y for the next position.
	 */
	INT PositionChildStandard( WxPropertyControl* InItem, INT InX, INT InY);
	/**
	 * Recursive minion of PositionChildren for favorites.
	 *
	 * @param	InX		The horizontal position of the child item.
	 * @param	InY		The vertical position of the child item.
	 * @return			An offset to the current Y for the next position.
	 */
	INT PositionChildFavorites( WxPropertyControl* InItem, INT InX, INT InY);


	FObjectPropertyNode* PropertyTreeRoot;

	/** Array to use for setting "next/previous focus */
	TArray <WxPropertyControl*> VisibleWindows;

	INT PerformCallbackCount;

	friend class WxPropertyWindowFrame;
	friend class WxObjectsPropertyControl;
	friend class WxCategoryPropertyControl;
};

///////////////////////////////////////////////////////////////////////////////
//
// Property Filter Window.
//
///////////////////////////////////////////////////////////////////////////////
/**
* Offers filtering by string of the Properties in the Property Window held by a Property Window Host
*/
class WxPropertyRequiredToolWindow : public wxPanel, public FDeferredInitializationWindow
{
public:

	DECLARE_DYNAMIC_CLASS(WxPropertyRequiredToolWindow);

	virtual ~WxPropertyRequiredToolWindow(void);

	/**
	 *	Initialize this property window.  Must be the first function called after creation.
	 *
	 * @param	parent			The parent window.
	 */
	virtual void Create( wxWindow* parent);

	//callbacks
	/*
	 * Callback used for getting the key command from a TextCtrl
	 */
	void OnFilterChanged( wxCommandEvent& In );
	/**
	 * Force clears the filter string
	 */
	void ClearFilterString(void);

	/**
	 * Callback for toggling favorites windows
	 */
	void OnToggleWindow( wxCommandEvent& In);

	/**
	 * Updates the UI with the favorites window status.
	 */
	void UI_FavoritesButton( wxUpdateUIEvent& In );

	/**
	 * Accessor functions for state of the favorites button
	 */
	void  SetFavoritesWindowVisible(const UBOOL bShow);
	/**
	 * Accessor functions to show or hide the "actor lock" button
	 */
	void SetActorLockButtonVisible (const UBOOL bShow, const UBOOL bIsLocked);

	/** accessor to the visibility of the save archetype button */
	void SetSaveButtonVisible(const UBOOL bShow);

	/** Rebuild the focus array to go through all open children */
	void AppendFocusWindows (OUT TArray<wxWindow*>& FocusArray);

	DECLARE_EVENT_TABLE();
private:

	/**
	 * Init function to prepare the images and window for toggling the favorites window
	 */
	void InitFavoritesToggleButton (void );
	/**
	 * Init function to prepare the images and window for toggling actor lock
	 */
	void InitLockActorButton (void);
	/**
	 * Init function to prepare the menu for all infrequently used options
	 */
	void InitOptionsButton (void);
	/**
	 * Init function to prepare the images and buttons for shifting favorite items
	 */
	void InitMoveButtons (void);

	/** The wrapper window for a search string. */
	WxSearchControl* SearchPanel;

	//Button for toggling the favorites window on and off
	WxBitmapCheckButton* FavoritesButton;
	//Favorites on/off images
	WxBitmap	FavoritesOnImage;
	WxBitmap	FavoritesOffImage;

	//Button used to lock the selected actor in the property window
	WxBitmapCheckButton* LockActorButton;
	//Images to display if the lock is on or off
	WxBitmap	LockActorOnImage;
	WxBitmap	LockActorOffImage;

	//Button to bring down the options popup.
	WxBitmapCheckButton* OptionsButton;
	//Options button image
	WxBitmap		OptionsImage;

	// save button
	WxBitmapButton* SaveButton;
	WxBitmap SaveImage;

	//Buttons to move a favorite selection up or down
	WxBitmapButton* MoveUpButton;
	WxBitmapButton* MoveDownButton;
	
	//Up/Down button images
	WxBitmap		MoveUpImage;
	WxBitmap		MoveDownImage;
};


///////////////////////////////////////////////////////////////////////////////
//
// Property Window Host.
//
///////////////////////////////////////////////////////////////////////////////

/**
* Property window is the window that just displays the raw properties
* Property Window Frame is a stand alone window that will contain a Property Window Host
* Property Window Host is the window that has the optional features like Favorites, Filter, Toolbar, etc.
*/

class WxPropertyWindowHost : public wxPanel, public FDeferredInitializationWindow
{
public:
	DECLARE_DYNAMIC_CLASS(WxPropertyWindowHost);

	virtual ~WxPropertyWindowHost();

	/**
	 *	Initialize this property window.  Must be the first function called after creation.
	 *
	 * @param	parent			The parent window.
	 * @param	id				The ID for this control.
	 * @param	bShowToolBar	If TRUE, create a toolbar along the top.
	 * @param	InNotifyHook	An optional callback that receives property PreChange and PostChange notifies.
	 */
	virtual void Create( wxWindow* parent, FNotifyHook* InNotifyHook = NULL, wxWindowID id=-1, UBOOL bShowTools = TRUE);

	/**
	 * Creates Filter and tool bar
	 */
	void CreateToolbar(wxBoxSizer* InParentSizer);

	WxPropertyWindow* GetPropertyWindowForCallbacks() 
	{
		check(PropertyWindow);
		return PropertyWindow; 
	}

	/**
	 * Requests that the window mark itself for data validity verification
	 */
	void RequestReconnectToData(void)
	{
		check(PropertyWindow);
		PropertyWindow->RequestReconnectToData();
	}

	/**
	 * Requests that the window take focus to prevent rebuild issues
	 */
	void RequestMainWindowTakeFocus(void)
	{
		check (PropertyWindow);
		PropertyWindow->RequestMainWindowTakeFocus();
	}

	/*
	 * Central hub function for setting Filtering on a Property window (a child of the host) from the PropertyFilterWindow (a different child of the host)
	 *	@param InFilterString - Can contain multiple space delimited sub-strings that must all be found in a property
	 */
	void SetFilterString(const FString& InFilterString);

	/**
	 * Force clears the filter string
	 */
	void ClearFilterString(void)
	{
		check(PropertyRequiredToolWindow);
		PropertyRequiredToolWindow->ClearFilterString();
	}


	/**
	 * Clears out the property window's last item of focus.
	 */
	void ClearLastFocused() 
	{
		check(PropertyWindow); 
		PropertyWindow->ClearLastFocused();
	}

	/**
	 * Rebuild all the properties and categories, with the same actors 
	 *
	 * @param IfContainingObject Only rebuild this property window if it contains the given object in the object hierarchy
	 */
	void Rebuild(UObject* IfContainingObject=NULL) 
	{
		check(PropertyWindow); 
		PropertyWindow->Rebuild(IfContainingObject);
	}

	//////////////////////////////////////////////////////////////////////////
	// Attaching objects.  Simply passes down to the property window.

	/**
	 * Sets an object in the property window.  T must be derived from UObject; will not
	 * compile if this is not the case.
	 *
	 * @param	InObject				The object to bind to the property window.  Can be NULL.
	 * @param	InWindowFlags is a some combination of EPropertyWindowFlags
	 */
	void SetObject( UObject* InObject, const UINT InWindowFlags)
	{
		check(PropertyWindow);
		check(PropertyRequiredToolWindow);

		TArray <UObject*> NewObjects;
		NewObjects.AddItem(InObject);
		if (MatchesExistingObjects(NewObjects))
		{
			//no need to reset the exact same objects
			return;
		}

		PropertyWindow->SetObject( InObject, InWindowFlags);

		//favorites window
		check(PropertyFavoritesWindow);
		PropertyFavoritesWindow->SetObject(InObject, InWindowFlags | EPropertyWindowFlags::Favorites);

		ApplyFavorites();

		UBOOL bShowActorLockButton = GetObjectBaseClass()->IsChildOf(AActor::StaticClass());
		//if not showing the lock, then this shouldn't be locked.
		if (!bShowActorLockButton)
		{
			bLocked = FALSE;
		}
		PropertyRequiredToolWindow->SetActorLockButtonVisible(bShowActorLockButton, bLocked);

		PropertyRequiredToolWindow->SetSaveButtonVisible( !GIsEditor && GIsGame && 
															InObject->HasAllFlags(RF_ArchetypeObject | RF_Standalone) &&
															!InObject->HasAnyFlags(RF_ClassDefaultObject) );
	}

	/**
	 * Sets multiple objects in the property window.  T must be derived from UObject; will not
	 * compile if this is not the case.
	 *
	 * @param	InObjects				An array of objects to bind to the property window.
	 * @param	InWindowFlags is a some combination of EPropertyWindowFlags
	 */
	template< class T >
	void SetObjectArray(const TArray<T*>& InObjects, const UINT InWindowFlags)
	{
		check(PropertyWindow);
		check(PropertyRequiredToolWindow);

		TArray <UObject*> NewObjects;
		for ( TArray<T*>::TConstIterator ObjIter( InObjects ); ObjIter; ++ObjIter )
		{
			NewObjects.AddItem(*ObjIter);
		}

		if (MatchesExistingObjects(NewObjects))
		{
			//no need to reset the exact same objects
			return;
		}

		PropertyWindow->SetObjectArray( InObjects, InWindowFlags);

		//favorites window
		check(PropertyFavoritesWindow);
		PropertyFavoritesWindow->SetObjectArray(InObjects, InWindowFlags | EPropertyWindowFlags::Favorites);

		ApplyFavorites();

		UBOOL bShowActorLockButton = GetObjectBaseClass()->IsChildOf(AActor::StaticClass());
		//if not showing the lock, then this shouldn't be locked.
		if (!bShowActorLockButton)
		{
			bLocked = FALSE;
		}
		PropertyRequiredToolWindow->SetActorLockButtonVisible(bShowActorLockButton, bLocked);

		PropertyRequiredToolWindow->SetSaveButtonVisible( !GIsEditor && GIsGame && NewObjects.Num() == 1 &&
															NewObjects(0)->HasAllFlags(RF_ArchetypeObject | RF_Standalone) &&
															!NewObjects(0)->HasAnyFlags(RF_ClassDefaultObject) );
	}

	/** @return		The base-est baseclass for objects in this list. */
	UClass*			GetObjectBaseClass()
	{
		check(PropertyWindow);
		return PropertyWindow->GetObjectBaseClass();
	}
	/** @return		The base-est baseclass for objects in this list. */
	const UClass*	GetObjectBaseClass() const
	{
		check(PropertyWindow);
		return PropertyWindow->GetObjectBaseClass();
	}

	/** Returns a context name that can be used to save/load settings per property window */
	FString GetContextName (void) const
	{
		//When nothing is selected
		FString ContextName;
		if (GetNumObjects())
		{
			const UClass* ObjectClass = GetObjectBaseClass();
			ContextName = WxPropertyWindow::GetContextName(ObjectClass);
		}
		return ContextName;
	}


	/** Utility for finding if a particular object is being edited by this property window. */
	UBOOL IsEditingObject(const UObject* InObject) const
	{
		check(PropertyWindow);
		return PropertyWindow->IsEditingObject(InObject);
	}

	/**
	 * Recursively searches through children for a property named PropertyName and expands it, along with all parents.
	 * If it's a UArrayProperty, the propery's ArrayIndex'th item is also expanded.
	 */
	void ExpandToItem( const FString& PropertyName, INT ArrayIndex=INDEX_NONE)
	{
		check(PropertyWindow);
		PropertyWindow->ExpandItem(PropertyName, ArrayIndex, TRUE);
	}

	/**
	 * Recursively searches through children for a property named PropertyName and expands it.
	 * If it's a UArrayProperty, the propery's ArrayIndex'th item is also expanded.
	 */
	void ExpandItem( const FString& PropertyName, INT ArrayIndex=INDEX_NONE)
	{
		check(PropertyWindow);
		PropertyWindow->ExpandItem(PropertyName, ArrayIndex);
	}

	/**
	 * Expands all items rooted at the property window node.
	 */
	void ExpandAllItems()
	{
		check(PropertyWindow);
		PropertyWindow->ExpandAllItems();
	}

	/**
	 * Removes all objects from the list.
	 */
	void RemoveAllObjects(void)
	{
		check(PropertyWindow);
		SetObject(NULL, PropertyWindow->HasFlags(EPropertyWindowFlags::AllFlags));
	}

	/**
	 * Called when the object list is finalized, Finalize() finishes the property window setup.
	 */
	void Finalize()
	{
		check(PropertyWindow);
		PropertyWindow->Finalize();
	}

	FPropertyNode* FindPropertyNode(const FString& InPropertyName)
	{
		check(PropertyWindow);
		return PropertyWindow->FindPropertyNode(InPropertyName);
	}

	/**
	 * Recursively searches through children for a property named PropertyName and collapses it.
	 * If it's a UArrayProperty, the propery's ArrayIndex'th item is also collapsed.
	 */
	void CollapseItem( const FString& PropertyName, INT ArrayIndex=INDEX_NONE)
	{
		check(PropertyWindow);
		PropertyWindow->CollapseItem(PropertyName, ArrayIndex);
	}

	/**
	 * Collapses all items in this property window.
	 */
	void CollapseAllItems(void)
	{
		check(PropertyWindow);
		PropertyWindow->CollapseAllItems();
	}


	/**
	 * Queues a message in the property window message queue to Rebuild all the properties and categories, with the same actors,  
	 *
	 * @param IfContainingObject Only rebuild this property window if it contains the given object in the object hierarchy
	 */
	void PostRebuild(UObject* IfContainingObject=NULL)
	{
		check(PropertyWindow);
		PropertyWindow->PostRebuild(IfContainingObject);
	}

	/**
	 * Passes a pointer to this property window to NotifyHook's NotifyDestroy.
	 */
	void NotifyDestroy()
	{
		check(PropertyWindow);
		PropertyWindow->NotifyDestroy();
	}


	/**
	 * Flushes the last focused item to the input proxy.
	 */
	void FlushLastFocused(void)
	{
		check(PropertyWindow);
		PropertyWindow->FlushLastFocused();
	}

	/**
	 * Relinquishes focus to LastFocused.
	 */
	void FinalizeValues(void)
	{
		check(PropertyWindow);
		PropertyWindow->FinalizeValues();
	}


	//////////////////////////////////////////////////////////////////////////
	//Flags
	UINT HasFlags (const EPropertyWindowFlags::Type InTestFlags) const
	{
		check(PropertyWindow);
		return PropertyWindow->HasFlags(InTestFlags);
	}
	/**
	 * Sets the flags used by the window and the root node
	 * @param InFlags - flags to turn on or off
	 * @param InOnOff - whether to toggle the bits on or off
	 */
	void  SetFlags (const EPropertyWindowFlags::Type InFlags, const UBOOL InOnOff)
	{
		check(PropertyWindow);
		PropertyFavoritesWindow->SetFlags(InFlags, InOnOff);
		PropertyWindow->SetFlags(InFlags, InOnOff);
	}

	typedef WxPropertyWindow::TObjectIterator		TObjectIterator;
	typedef WxPropertyWindow::TObjectConstIterator	TObjectConstIterator;

	TObjectIterator			ObjectIterator()			{ return PropertyWindow->ObjectIterator(); }
	TObjectConstIterator	ObjectConstIterator() const	{ return PropertyWindow->ObjectConstIterator(); }

	/** Returns the number of objects for which properties are currently being edited. */
	INT GetNumObjects() const
	{
		check(PropertyWindow);
		return PropertyWindow->GetNumObjects(); 
	}

	/**
	 * Internal function to ensure that the button and state flags are in sync
	 */
	void SetFavoritesWindow(const UBOOL bShowFavoritesWindow);
	/**If true, favorites window is visible*/
	UBOOL GetFavoritesVisible(void) const { return bFavoritesWindowEnabled; }

	/**
	 * Putting options menu items item a popup
	 */
	void ShowOptionsMenu(wxCommandEvent& In);

	/**Adjusts the layout of the favorites/main property windows*/
	void AdjustFavoritesSplitter(UBOOL bFavoritesChanged=FALSE);
	/**Tells both favorites and main property windows to re-apply their favorites settings*/
	void ApplyFavorites(UBOOL bFavoritesChanged=FALSE);

	/**
	 * Copies selected objects to clip board.
	 */
	void OnCopy( wxCommandEvent& In );
	/**
	 * Copies selected objects to clip board.
	 */
	void OnCopyComplete( wxCommandEvent& In );
	/**
	 * Expands all categories for the selected objects.
	 */
	void OnExpandAll( wxCommandEvent& In );

	/**
	 * Collapses all categories for the selected objects.
	 */
	void OnCollapseAll( wxCommandEvent& In );

	/**
	 * Locks or unlocks the property window, based on the 'checked' status of the event.
	 */
	void OnLock( wxCommandEvent& In );
	/**
	 * Returns TRUE if this property window is locked, or FALSE if it is unlocked.
	 */
	UBOOL IsLocked() const			{ return bLocked; }
	/**
	 * Updates the UI with the locked status of the contained property window.
	 */
	void UI_Lock( wxUpdateUIEvent& In );

	/**
	 * Captures passed up key events to deal with focus changing
	 */
	void OnChar( wxKeyEvent& In );

	/**Copies Name/Value pairs from selected properties*/
	void OnCopyFocusProperties(wxCommandEvent& In);

	/**Pastes Name/Value pairs from clipboard to properties*/
	void OnPasteFromClipboard(wxCommandEvent& In);

	/** Event handler for toggling whether or not all property windows should display all of their buttons. */
	void MenuPropWinShowHiddenProperties(wxCommandEvent &In);
	/** Event handler for toggling using script defined order */
	void MenuPropToggleUseScriptDefinedOrder(wxCommandEvent &In);
	/** Event handler for toggling horizontal dividers */
	void MenuPropToggleShowHorizontalDividers(wxCommandEvent &In);
	/** Event handler for toggling whether or not to display nicer looking property names or the defaults. */
	void MenuPropWinToggleShowFriendlyPropertyNames(wxCommandEvent &In);
	/** Event handler for toggling whether or not all property windows should only display modified properties. */
	void MenuPropWinToggleShowModifiedProperties(wxCommandEvent &In);
	/** Event handler for toggling whether or not all property windows should only display differing properties. */
	void MenuPropWinToggleShowDifferingProperties(wxCommandEvent &In);
	/** Updates the UI for the menu item allowing . */
	void UI_ToggleDifferingUpdate( wxUpdateUIEvent& In );
	/* Shifts the selected favorite property node up */
	void OnMoveFavoritesUp(wxCommandEvent& In) { MoveFavorites(-1); }
	/* Shifts the selected favorite property node down */
	void OnMoveFavoritesDown(wxCommandEvent& In) { MoveFavorites(1); }

	/** Event handler for scrolling down a property window*/
	void OnMouseWheel( wxMouseEvent& In );
	/**Event handler for resizing of property window host*/
	void OnSize( wxSizeEvent& In );

	/** event handler for the save object button */
	void OnSaveObject(wxCommandEvent& In);

	/**
	 * Allows child windows to be appended to 
	 */
	void RebuildFocusWindows (void);
	/**
	 * Moves focus to the item/category that next/prev to InItem.
	 */
	void MoveFocus (const EPropertyWindowDirections::Type InDirection, wxWindow* InWindow );

	/**
	 * Gives focus directly to the search dialog
	 */
	void SetSearchFocus (void);

	/**Focus helper to help RequestMainWindowTakeFocus */
	void SetFocusToOtherPropertyWindow (const WxPropertyWindow* InPropertyWindow);

	/** Clears all windows from the active focus array */
	void ClearActiveFocus(void) { ActiveFocusWindows.Empty(); }

	/**
	 * Toggles a windows inclusion into the active focus array
	 * @param InWindow - Window to toggle
	 */
	void SetActiveFocus(WxPropertyControl* InWindow, const UBOOL bOnOff);

	/**
	 * Returns TRUE, if the window has active focus on it (selected or part of a multi-select)
	 */
	UBOOL HasActiveFocus (WxPropertyControl* InWindow) const { return ActiveFocusWindows.ContainsItem(InWindow); }

	/**
	 * Returns count of windows in the "Active Focus" array
	 */
	INT NumActiveFocusWindows() const { return ActiveFocusWindows.Num(); }

	/**
	 * Returns the property node in the foucs list at the given index
	 * @param InIndex - Index of the focus window
	 * @return - Propertynode at the given index
	 */
	FPropertyNode* GetFocusNode(const INT InIndex);

	/**
	 * Returns if a window is currently in the "Active Focus" array
	 * @param InWindow - Window in question
	 */
	INT IsWindowInActiveFocus(WxPropertyControl* InWindow) const { return ActiveFocusWindows.ContainsItem(InWindow); }

	/**
	 * Copies all property names and values to a buffer for future pasting
	 */
	FString CopyActiveFocusPropertiesToClipboard (void);

	/** Gets the offset this property window */
	const INT GetPropOffset( void ) const { return PropOffset; }

	/** Sets the offset for this property window */
	void SetPropOffset( const INT InPropOffset ) { PropOffset = InPropOffset; }

	UBOOL IsPropertySelected( const FString& InName, const INT InArrayIndex = INDEX_NONE );
	UBOOL IsPropertyOrChildrenSelected( const FString& InName, const INT InArrayIndex = INDEX_NONE, const UBOOL CheckChildren = TRUE );

	/** Enables/Disables category based options */
	void EnableCategoryOptions( BOOL bEnable ) { bEnableCategoryOptions = bEnable; }

	DECLARE_EVENT_TABLE();

protected:

	/**
	 * Returns TRUE if the "NewObjects" match the current objects exactly
	 */
	UBOOL MatchesExistingObjects(const TArray<UObject*>& InTestObjects);

	/** Filter Window*/
	WxPropertyRequiredToolWindow* PropertyRequiredToolWindow;

	/** Favorites Window*/
	WxPropertyWindow* PropertyFavoritesWindow;

	/** The property window embedded inside this floating frame. */
	WxPropertyWindow* PropertyWindow;

	/** Sizer to hold all property windows and the tool bar*/
	wxBoxSizer* PropertyFeatureSizer;

	/**Fixed size splitter that separates the favorites property window from the main property window*/
	WxFixedSplitter* FavoritesSplitter;

	/**Dummy Panel used to show and hide favorites property window*/
	wxPanel* FavoritesPanel;

	/** TRUE if the favorites window should be showed*/
	UBOOL bFavoritesWindowEnabled;

	/** Array to use for setting "next/previous focus */
	TArray <wxWindow*> FocusArray;

	/** Active Focus array used for multi-select */
	TArray <WxPropertyControl*> ActiveFocusWindows;

	/**
	 * Flag that sets whether changing the selected object is allowed or not
	 */
	BOOL bLocked;

	/**
	 * Toggle category based options in the options menu (some prop windows don't need these)
	 */
	BOOL bEnableCategoryOptions;

	/** The offset for this property window */
	INT PropOffset;

private:
	/**
	 * Shifts the selected favorite property node in the given direction.
	 * @param InDirection - Direction to shift from selected
	 */
	void MoveFavorites(const EPropertyWindowDirections::Type InDirection);
};

///////////////////////////////////////////////////////////////////////////////
//
// Property window frame.
//
///////////////////////////////////////////////////////////////////////////////

/**
 * Property windows which are meant to be stand-alone windows must sit inside
 * of a WxPropertyWindowFrame.  Floating property windows can contain an optional
 * toolbar, and can be locked to paticular properties.  WxPropertyWindowFrame
 * also manages whether or not a property window can be closed.
 */
class WxPropertyWindowFrame : public wxFrame, public FDeferredInitializationWindow
{
public:
	DECLARE_DYNAMIC_CLASS(WxPropertyWindowFrame);

	virtual ~WxPropertyWindowFrame();

	/**
	 *	Initialize this property window.  Must be the first function called after creation.
	 *
	 * @param	parent			The parent window.
	 * @param	id				The ID for this control.
	 * @param	bShowToolBar	If TRUE, create a toolbar along the top.
	 * @param	InNotifyHook	An optional callback that receives property PreChange and PostChange notifies.
	 */
	virtual void Create(wxWindow* parent, wxWindowID id, FNotifyHook* InNotifyHook = NULL );

	//const WxPropertyWindow* GetPropertyWindow() const { return PropertyWindow; }
	/** 
	 * Returns the PropertyWindow to be used for OnNotifyHook callback comparisons.  Used in cascade
	 */
	WxPropertyWindow* GetPropertyWindowForCallbacks() 
	{
		check(PropertyWindowHost);
		return PropertyWindowHost->GetPropertyWindowForCallbacks(); 
	}

	/**
	 * Returns the flags masked with the subset of InTestFlags
	 */
	UINT HasFlags (const EPropertyWindowFlags::Type InTestFlags) const
	{
		check(PropertyWindowHost);
		return PropertyWindowHost->HasFlags(InTestFlags);
	}
	/**
	 * Sets the flags used by the window and the root node
	 * @param InFlags - flags to turn on or off
	 * @param InOnOff - whether to toggle the bits on or off
	 */
	void  SetFlags (const EPropertyWindowFlags::Type InFlags, const UBOOL InOnOff)
	{
		check(PropertyWindowHost);
		return PropertyWindowHost->SetFlags(InFlags, InOnOff);
	}
	/**
	 * Responds to close events.  Rejects the event if closing is disallowed.
	 */
	void OnClose( wxCloseEvent& In );

	/**
	 * Responds to size events.  Resizes the handled property window.
	 */
	void OnSize( wxSizeEvent& In );

	/**
	 * Returns TRUE if this property window is locked, or FALSE if it is unlocked.
	 */
	UBOOL IsLocked() const			
	{ 
		check(PropertyWindowHost); 
		return PropertyWindowHost->IsLocked();
	}

	UBOOL IsCloseAllowed() const	{ return bAllowClose; }
	void AllowClose()				{ bAllowClose = TRUE; }
	void DisallowClose()			{ bAllowClose = FALSE; }

	/**
	 * Recursively searches through children for a property named PropertyName and expands it.
	 * If it's a UArrayProperty, the propery's ArrayIndex'th item is also expanded.
	 */
	void ExpandItem( const FString& PropertyName, INT ArrayIndex=INDEX_NONE)
	{
		check(PropertyWindowHost);
		PropertyWindowHost->ExpandItem(PropertyName, ArrayIndex);
	}

	/**
	 * Recursively searches through children for a property named PropertyName and expands it, along with all parents.
	 * If it's a UArrayProperty, the propery's ArrayIndex'th item is also expanded.
	 */
	void ExpandToItem( const FString& PropertyName, INT ArrayIndex=INDEX_NONE, UBOOL bShouldFocus = FALSE )
	{
		check(PropertyWindowHost);
		PropertyWindowHost->CollapseAllItems();
		PropertyWindowHost->ExpandToItem( PropertyName, ArrayIndex );
		if( bShouldFocus )
		{
			FPropertyNode* NotifyNode = PropertyWindowHost->FindPropertyNode( PropertyName );
			if( NotifyNode  )
			{
				PropertyWindowHost->ClearActiveFocus();
				PropertyWindowHost->SetActiveFocus( NotifyNode->GetNodeWindow(), TRUE );
			}
		}
	}

	/**
	 * Removes all objects from the list.
	 */
	void RemoveAllObjects(void)
	{
		check(PropertyWindowHost);
		PropertyWindowHost->RemoveAllObjects();
	}

	/**
	 * Updates the caption of the floating frame based on the objects being edited.
	 */
	void UpdateTitle();


	/** 
	 * Overridden show function to allow search bar to get focus by default
	 */
	virtual bool Show(bool bInShow=true)
	{
		if ( bInShow )
		{
			// If the window isn't already shown, make sure to handle loading the layout correctly
			if ( !IsShown() )
			{
				LoadLayout( TRUE );
			}
			check( PropertyWindowHost );
			PropertyWindowHost->SetSearchFocus();
		}
		// If hiding the window, make sure the layout is properly saved off
		else
		{
			SaveLayout();
		}

		return wxWindow::Show( bInShow );
	}

	/** 
	 * Overridden show function to allow search bar to get focus by default
	 */
	void OnSetFocus (wxFocusEvent& In)
	{
		check(PropertyWindowHost);
		PropertyWindowHost->SetSearchFocus();
	}

	/**
	 * Clears out the property window's last item of focus.
	 */
	void ClearLastFocused() 
	{
		check(PropertyWindowHost); 
		PropertyWindowHost->ClearLastFocused();
	}

	/**
	 * Rebuild all the properties and categories, with the same actors 
	 *
	 * @param IfContainingObject Only rebuild this property window if it contains the given object in the object hierarchy
	 */
	void Rebuild(UObject* IfContainingObject=NULL) 
	{
		check(PropertyWindowHost); 
		PropertyWindowHost->Rebuild(IfContainingObject);
	}

	//////////////////////////////////////////////////////////////////////////
	// Attaching objects.  Simply passes down to the property window.

	/**
	 * Sets an object in the property window.  T must be derived from UObject; will not
	 * compile if this is not the case.
	 *
	 * @param	InObject				The object to bind to the property window.  Can be NULL.
	 * @param	InWindowFlags is a some combination of EPropertyWindowFlags
	 */
	void SetObject( UObject* InObject, const UINT InWindowFlags)
	{
		check(PropertyWindowHost);

		SaveLayout();
		PropertyWindowHost->SetObject( InObject, InWindowFlags );
		LoadLayout();
	}

	/**
	 * Sets multiple objects in the property window.  T must be derived from UObject; will not
	 * compile if this is not the case.
	 *
	 * @param	InObjects				An array of objects to bind to the property window.
	 * @param	InWindowFlags is a some combination of EPropertyWindowFlags
	 */
	template< class T >
	void SetObjectArray(const TArray<T*>& InObjects, const UINT InWindowFlags)
	{
		check(PropertyWindowHost);

		SaveLayout();
		PropertyWindowHost->SetObjectArray( InObjects, InWindowFlags );
		LoadLayout();
	}

	//////////////////////////////////////////////////////////////////////////
	// Object iteration.  Simply passes down to the property window.

	typedef WxPropertyWindow::TObjectIterator		TObjectIterator;
	typedef WxPropertyWindow::TObjectConstIterator	TObjectConstIterator;

	TObjectIterator			ObjectIterator()			{ return PropertyWindowHost->ObjectIterator(); }
	TObjectConstIterator	ObjectConstIterator() const	{ return PropertyWindowHost->ObjectConstIterator(); }

	DECLARE_EVENT_TABLE();

protected:

	/**
	 * Loads layout based on what object is set.
	 * @Param bInForceLoad - If true, loads position info even if nothing is selected, for create call
	 */
	void LoadLayout (const UBOOL bInForceLoad=FALSE);

	/**
	 * Saves layout based on what object is set.
	 */
	void SaveLayout (void);

	/**
	 * Returns the context window name for storing window setting PER type of property window requested
	 */
	FString GetContextWindowName(void) const;

	/** The property window embedded inside this floating frame. */
	WxPropertyWindowHost* PropertyWindowHost;

	/** Last context this property window was opened with*/
	FString LastContextName;

	/** If TRUE, this property frame will destroy itself if the user clicks the "X" button.  Otherwise, it just hides. */
	UBOOL bAllowClose;
};

/*-----------------------------------------------------------------------------
	FPropertyItemValueDataTracker
-----------------------------------------------------------------------------*/
/**
 * Calculates and stores the address for both the current and default value of
 * the associated property and the owning object.
 */
struct FPropertyItemValueDataTracker
{
	/**
	 * A union which allows a single address to be represented as a pointer to a BYTE
	 * or a pointer to a UObject.
	 */
	union UPropertyValueRoot
	{
		UObject*	OwnerObject;
		BYTE*		ValueAddress;
	};

	/**
	 * The property window item that we're holding values for.
	 */
	WxItemPropertyControl* PropertyItem;

	/** the number of bytes between the property value's address and the address of the owning object */
	INT PropertyOffset;

	/** The address of the owning object */
	UPropertyValueRoot PropertyValueRoot;

	/**
	 * The base address of this property's value.  i.e. for dynamic arrays, the location of the FScriptArray which
	 * contains the array property's value
	 */
	BYTE* PropertyValueBaseAddress;

	/**
	 * The address of this property's value.
	 */
	BYTE* PropertyValueAddress;

	/**
	 * The address of the owning object's archetype
	 */
	UPropertyValueRoot PropertyDefaultValueRoot;

	/**
	 * The base address of this property's default value (see other comments for PropertyValueBaseAddress)
	 */
	BYTE* PropertyDefaultBaseAddress;

	/**
	 * The address of this property's default value.
	 */
	BYTE* PropertyDefaultAddress;

	/**
	 * Constructor
	 *
	 * @param	InPropItem		the property window item this struct will hold values for
	 * @param	InOwnerObject	the object which contains the property value
	 */
	FPropertyItemValueDataTracker( WxItemPropertyControl* InPropItem, UObject* InOwnerObject );

	/**
	 * Determines whether the property bound to this struct exists in the owning object's archetype.
	 *
	 * @param	pMemberPropertyOffset	if specified, receives the value of the offset from the address of the property's value
	 *									to the address of the owning object.  If the property is not a class member property,
	 *									such as the Inner property of a dynamic array, or a struct member, returns the offset
	 *									of the member property which contains this property.
	 *
	 * @return	TRUE if this property exists in the owning object's archetype; FALSE if the archetype is e.g. a
	 *			CDO for a base class and this property is declared in the owning object's class.
	 */
	UBOOL HasDefaultValue( INT* pMemberPropertyOffset=NULL ) const;

	/**
	 * @return	a pointer to the subobject root (outer-most non-subobject) of the owning object.
	 */
	UObject* GetTopLevelObject();
};

/*-----------------------------------------------------------------------------
	FPropertyItemComponentCollector
-----------------------------------------------------------------------------*/
/**
 * Given a property and the address for that property's data, searches for references to components and keeps a list of any that are found.
 */
struct FPropertyItemComponentCollector
{
	/** contains the property to search along with the value address to use */
	const FPropertyItemValueDataTracker& ValueTracker;

	/** holds the list of components found */
	TLookupMap<UComponent*> Components;

	/** Constructor */
	FPropertyItemComponentCollector( const FPropertyItemValueDataTracker& InValueTracker );

	/**
	 * Routes the processing to the appropriate method depending on the type of property.
	 *
	 * @param	Property				the property to process
	 * @param	PropertyValueAddress	the address of the property's value
	 */
	void ProcessProperty( UProperty* Property, BYTE* PropertyValueAddress );

private:

	/**
	 * UArrayProperty version - invokes ProcessProperty on the array's Inner member for each element in the array.
	 *
	 * @param	ArrayProp				the property to process
	 * @param	PropertyValueAddress	the address of the property's value
	 *
	 * @return	TRUE if the property was handled by this method
	 */
	UBOOL ProcessArrayProperty( UArrayProperty* ArrayProp, BYTE* PropertyValueAddress );

	/**
	 * UStructProperty version - invokes ProcessProperty on each property in the struct
	 *
	 * @param	StructProp				the property to process
	 * @param	PropertyValueAddress	the address of the property's value
	 *
	 * @return	TRUE if the property was handled by this method
	 */
	UBOOL ProcessStructProperty( UStructProperty* StructProp, BYTE* PropertyValueAddress );

	/**
	 * UObjectProperty version - if the object located at the specified address is a UComponent, adds the component the list.
	 *
	 * @param	ObjectProp				the property to process
	 * @param	PropertyValueAddress	the address of the property's value
	 *
	 * @return	TRUE if the property was handled by this method
	 */
	UBOOL ProcessObjectProperty( UObjectProperty* ObjectProp, BYTE* PropertyValueAddress );

	/**
	 * UInterfaceProperty version - if the FScriptInterface located at the specified address contains a reference to a UComponent, add the component to the list.
	 *
	 * @param	InterfaceProp			the property to process
	 * @param	PropertyValueAddress	the address of the property's value
	 *
	 * @return	TRUE if the property was handled by this method
	 */
	UBOOL ProcessInterfaceProperty( UInterfaceProperty* InterfaceProp, BYTE* PropertyValueAddress );

	/**
	 * UDelegateProperty version - if the FScriptDelegate located at the specified address contains a reference to a UComponent, add the component to the list.
	 *
	 * @param	DelegateProp			the property to process
	 * @param	PropertyValueAddress	the address of the property's value
	 *
	 * @return	TRUE if the property was handled by this method
	 */
	UBOOL ProcessDelegateProperty( UDelegateProperty* DelegateProp, BYTE* PropertyValueAddress );
};

#endif // __PROPERTY_WINDOW_H__
