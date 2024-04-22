/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __REFERENCEDASSETSBROWSER_H__
#define __REFERENCEDASSETSBROWSER_H__

typedef TMap< UObject*, TArray<UObject*> >	ObjectReferenceGraph;
typedef TMap< UObject*, wxTreeItemId >		ReferenceTreeMap;
typedef TMap< UObject*, FString >			ObjectNameMap;


/**
 * The different ways for indicating how deep to search for referenced assets
 */
enum EReferenceDepthMode
{
	REFDEPTH_Direct,
	REFDEPTH_Infinite,
	REFDEPTH_Custom,
};

/**
 * Data container to hold information about what is referencing a given
 * set of assets.
 */
struct FReferencedAssets
{
	/**
	 * The object that holding a reference to the set of assets
	 */
	UObject* Referencer;
	/**
	 * The set of assets that are being referenced
	 */
	TArray<UObject*> AssetList;

	/** Default ctor */
	FReferencedAssets() :
		Referencer(NULL)
	{}

	/** 
	 * Sets the name of the referencer
	 */
	FReferencedAssets(UObject* InReferencer) :
		Referencer(InReferencer)
	{
	}

	// serializer
	friend FArchive& operator<<( FArchive& Ar, FReferencedAssets& Asset )
	{
		return Ar << Asset.Referencer << Asset.AssetList;
	}
};

/**
 * This archive searches objects for assets. It determines the set of
 * assets by whether they support thumbnails or not. Possibly, not best
 * but it displays everything as thumbnails, so...
 */
class FFindAssetsArchive : public FArchive
{
	/**
	 * The root object that was used to being serialization for this archive
	 */
	UObject* StartObject;

	/**
	 * The object currently being serialized.
	 */
	UObject* CurrentObject;

	/** The array to add any found assets too */
	TArray<UObject*>& AssetList;

	/**
	* Set when the global asset list is updated.  Used to prevent the reference graph from being
	* polluted by calls to the public version of BuildAssetList.
	*/
	ObjectReferenceGraph* CurrentReferenceGraph;

	/**
	 * if FALSE, ignore all assets referenced only through script
	 */
	UBOOL bIncludeScriptRefs;

	/**
	 * if FALSE, ignore all assets referenced only through archetype/class default objects
	 */
	UBOOL bIncludeDefaultRefs;

	/**
	 * Maximum depth to recursively serialize objects; 0 indicates no limit to recursion
	 */
	const INT MaxRecursionDepth;

	/**
	 * Current Recursion Depth
	 */
	INT CurrentDepth;

	/**
	 * Manually serializes the class and archetype for the specified object so that assets which are referenced
	 * through the object's class/archetype can be differentiated.
	 */
	void HandleReferencedObject( UObject* Obj );

	/**
	 * Retrieves the referenced assets list for the specified object.
	 */
	TArray<UObject*>* GetAssetList( UObject* Referencer );

public:
	/**
	 * Functor that starts the serialization process
	 */
	FFindAssetsArchive( 
		UObject* InSearch, 
		TArray<UObject*>& OutAssetList, 
		ObjectReferenceGraph* ReferenceGraph=NULL, 
		INT MaxRecursion=0, 
		UBOOL bIncludeClasses=TRUE, 
		UBOOL bIncludeDefaults=FALSE );

	/**
	 * Adds the object refence to the asset list if it supports thumbnails.
	 * Recursively searches through its references for more assets
	 *
	 * @param Obj the object to inspect
	 */
	FArchive& operator<<(class UObject*& Obj);
};

class WxReferencedAssetsBrowserBase : 
	public WxBrowser,
	public FSerializableObject
{
public:

	WxReferencedAssetsBrowserBase();

	/**
	 * Listens for CALLBACK_MapChange events.  Clears all references to actors in the current level.
	 */
	virtual void Send(ECallbackEventType InType, DWORD Flag);

	/**
	 * Checks an object to see if it should be included for asset searching
	 *
	 * @param Object the object in question
	 * @param ClassesToIgnore the list of classes to skip
	 * @param PackagesToIgnore the list of packages to skip
	 * @param bIncludeDefaults specify TRUE to include content referenced through defaults
	 *
	 * @return TRUE if it should be searched, FALSE otherwise
	 */
	static UBOOL ShouldSearchForAssets( const UObject* Object, const TArray<UClass*>& ClassesToIgnore, const TArray<UObject*>& PackagesToIgnore, UBOOL bIncludeDefaults=FALSE );

	/* === FSerializableObject interface === */
	virtual void Serialize( FArchive& Ar );

protected:

	/**
	 * This is a list of classes that should be ignored when building the
	 * asset list as they are always loaded and therefore not pertinent
	 */
	TArray<UClass*> IgnoreClasses;

	/**
	 * This is a list of packages that should be ignored when building the
	 * asset list as they are always loaded and therefore not pertinent
	 */
	TArray<UObject*> IgnorePackages;

	/**
	 * Holds the list of assets that are being referenced by the current 
	 * selection
	 */
	TArray<FReferencedAssets> Referencers;

	/**
	 * The object graph for the assets referenced by the currently selected actors.
	 */
	ObjectReferenceGraph ReferenceGraph;
};

/**
 * Menu bar for the Referenced Assets Browser.
 */
class WxMBReferencedAssetsBrowser : public wxMenuBar
{
public:
	WxMBReferencedAssetsBrowser();
};

/**
 * This browser shows the list of assets that are used by the set of selected
 * objects.
 */
class WxReferencedAssetsBrowser :
	public WxReferencedAssetsBrowserBase,
	public FTickableObject
{
	DECLARE_DYNAMIC_CLASS(WxReferencedAssetsBrowser);

public:

	/**
	 * The column headers for the referenced asset list view
	 */
	enum EListViewColumnHeaders
	{
		COLHEADER_Name,
		COLHEADER_Info,
		COLHEADER_Group,
		COLHEADER_Size,
		COLHEADER_Class,
		COLHEADER_MAX,
	};

protected:
	/**
	 * Controls how deep to search for referenced assets
	 */
	EReferenceDepthMode DepthMode;
	/**
	 * If TRUE, objects shown in the browser viewport are sorted by name AND grouped by their class names.
	 */
	UBOOL bGroupByClass;

	/**
	 * if TRUE, include assets referenced through the selected actors' archetype
	 */
	UBOOL bIncludeDefaultRefs;

	/**
	 * if TRUE, include assets referenced through the selected actors' class script
	 */
	UBOOL bIncludeScriptRefs;

	/**
	 * Whether the viewport is currently in the scrolling state or not
	 */
	UBOOL bIsScrolling;
	
	/**
	 * Used to determine if the list needs to be rebuilt or not
	 */
	UBOOL bNeedsUpdate;

	/**
	 * Used to prevent recursion when attempting to synchronize the selection set between the various
	 * controls in the RA browser.
	 */
	INT SelectionMutex;

	/**
	 * The position of the MainSplitter's sash.
	 */
	INT SplitterPos;

	/**
	 * The objects currently selected in the browser window
	 */
	static USelection* Selection;

	/** If this is non-NULL, the viewport will be scrolled to make sure this object is in view when it draws next. */
	UObject* SyncToNextDraw;

	/**
	 * The splitter between the reference graph tree and the asset view
	 */
	wxSplitterWindow* MainSplitter;
	
	/**
	 * The toolbar used by this browser
	 */
	class WxRABrowserToolBar* ToolBar;

	/**
	 * Displays the reference graph
	 */
	WxTreeCtrl* ReferenceTree;

	/**
	 * The list that displays the referenced assets, when in list view mode
	 */
	wxListView* ListView;

	/**
	 * The width of each column header
	 */
	INT ColumnWidth[COLHEADER_MAX];

	/**
	 * Stores the mapping of object -> tree item Id
	 */
	ReferenceTreeMap ReferenceGraphMap;

	/**
	 * Tracks the current sort column and order for the list view
	 */
	FListViewSortOptions ListSortOptions;

	/**
	 * Loads user-configurable settings from the editor ini file.
	 */
	void LoadSettings();

	/**
	 * Saves user configurable settings to the editor's ini file.
	 */
	void SaveSettings();

	/**
	 * Builds a list of assets to display from the currently selected actors.
	 * NOTE: It ignores assets that are there because they are always loaded
	 * such as default materials, textures, etc.
	 */
	void BuildAssetList(void);

	/**
	 * Sorts the list of referenced assets that is used for rendering the thumbnails.
	 */
	void SortAssetList();

	/**
	 * Requests that the contents of the currently visible window be refreshed.
	 */
	void Redraw(void);

	/**
	 * Populates the reference tree with the current set of assets
	 */
	void UpdateTree();

	/**
	 * Populates the list view with the current set of assets
	 */
	void UpdateList();

	/**
	 * Returns the object associated with the specified tree item id, or NULL if there is no
	 * object associated with that tree item or 
	 */
	UObject* GetTreeObject( wxTreeItemId TreeId );

	/**
	 * Ensures that the tree item associated with the selected objects is the only items selected in the tree
	 */
	void SynchronizeTreeSelections();

	/**
	 * Adds tree items for the objects specified under the specified parent id
	 *
	 * @param	ParentId	the tree id to add child items under
	 * @param	BaseObject	the object corresponding to the tree item indicated by ParentId
	 * @param	AssetList	the list of objects that should be added to the tree
	 */
	void AddReferencedAssets( class wxTreeItemId ParentId, UObject* BaseObject, TArray<UObject*>* AssetList );

public:
	/**
	 * Default constructor.
	 */
	WxReferencedAssetsBrowser(void);

	/**
	 * Destructor. Cleans up allocated resources
	 */
	virtual ~WxReferencedAssetsBrowser(void);

	/**
	 * Returns the selection set for the this browser.
	 */
	static USelection* GetSelection();

	/**
	 * Forwards the call to our base class to create the window relationship.
	 * Creates any internally used windows after that
	 *
	 * @param DockID the unique id to associate with this dockable window
	 * @param FriendlyName the friendly name to assign to this window
	 * @param Parent the parent of this window (should be a Notebook)
	 */
	virtual void Create(INT DockID,const TCHAR* FriendlyName,wxWindow* Parent);

	/**
	 * Changes the reference depth mode to the mode specified.
	 */
	void SetDepthMode( EReferenceDepthMode InDepthMode );

	/**
	 * Retrieves the currently configured serialization recursion depth, as determine by the depth mode and the configured custom depth.
	 *
	 * @return	the current maximum recursion depth to use when searching for referenced assets; 0 if infinite recursion depth is desired.
	 */
	INT GetRecursionDepth() const;

	/**
	 * Selects the specified resource and scrolls the viewport so that resource is in view.
	 *
	 * @param	InObject		The resource to sync to.  Must be a valid pointer.
	 */
	void SyncToObject(UObject* InObject );

	/**
	 * Called when the browser is getting activated (becoming the visible
	 * window in it's dockable frame).
	 */
	void Activated(void);

	/**
	 * Rebuilds the list of referenced assets.
	 */
	virtual void Update(void);

	/**
	 * Sets a flag indicating that the list of referenced assets needs to be regenerated. It is preferable to call
	 * this method, rather than directly calling Update() in case multiple operations in a single tick/callstack
	 * invalidate the asset list.
	 */
	void RequestUpdate();

	/**
	 * Returns the key to use when looking up values
	 */
	virtual const TCHAR* GetLocalizationKey(void) const
	{
		return TEXT("ReferencedAssetBrowser");
	}

	/**
	 * Returns a list of all assets referenced by the specified UObject.
	 */
	static void BuildAssetList(UObject *Object, const TArray<UClass*>& IgnoreClasses, const TArray<UObject*>& IgnorePackages, TArray<UObject*>& ReferencedAssets);

	/**
	 * Causes the browser to ignore all selection change notifications both from the engine
	 * as well as from any list controls
	 */
	void EnableSelectionNotification()
	{
		SelectionMutex--;
		check(SelectionMutex>=0);
	}

	/**
	 * Reenables listening for selection change notification.
	 */
	void DisableSelectionNotification()
	{
		SelectionMutex++;
	}


	/* === FCallbackEventDevice interface === */
	virtual void Send( ECallbackEventType InType )
	{
		RequestUpdate();
	}
	/**
	 * Refreshes the browser when an object selection changes
	 *
	 * @param InType ignored (we only sign up for selection change)
	 * @param InObject ignored
	 */
	virtual void Send(ECallbackEventType InType,UObject* InObject);

	/**
	 * Listens for CALLBACK_MapChange events.  Clears all references to actors in the current level.
	 */
	virtual void Send(ECallbackEventType InType, DWORD Flag);

	/* === FTickableObject interface === */
	virtual void Tick( FLOAT DeltaTime );

	/**
	 * Determines whether this tickable object is ready to be ticked.
	 */
	virtual UBOOL IsTickable() const;

	/**
	 * Used to determine if an object should be ticked when the game is paused.
	 * Defaults to false, as that mimics old behavior.
	 *
	 * @return TRUE if it should be ticked when paused, FALSE otherwise
	 */
	virtual UBOOL IsTickableWhenPaused() const
	{
		return TRUE;
	}

	/**
	 * Used to determine whether the object should be ticked in the editor.  Defaults to FALSE since
	 * that is the previous behavior.
	 *
	 * @return	TRUE if this tickable object can be ticked in the editor
	 */
	virtual UBOOL IsTickableInEditor() const
	{
		return TRUE;
	}

	/* === FSerializableObject interface === */
	virtual void Serialize( FArchive& Ar );

private:
	/**
	 * Refreshes the contents of the window when requested
	 *
	 * @param In the command that was sent
	 */
	void OnRefresh(wxCommandEvent& In);

	/**
	 * Handler for EVT_SIZE events.
	 *
	 * @param In the command that was sent
	 */
	void OnSize(wxSizeEvent& In);

	void OnActivateListItem( wxListEvent& Event );
	void OnSelectListItem( wxListEvent& Event );
	void OnDeselectListItem( wxListEvent& Event );
	void OnListColumnClicked( wxListEvent& Event );

	void OnTreeItemSelected( wxTreeEvent& Event );
	void OnTreeItemActivated( wxTreeEvent& Event );

	void OnDepthMode( wxCommandEvent& Event );
	void OnIncludeDefaultRefs( wxCommandEvent& Event );
	void OnIncludeScriptRefs( wxCommandEvent& Event );
	void OnGroupByClass( wxCommandEvent& Event );
	void OnCustomDepthChanged( wxCommandEvent& Event );

	void UI_DepthMode( wxUpdateUIEvent& Event );
	void UI_IncludeDefaultRefs( wxUpdateUIEvent& Event );
	void UI_IncludeScriptRefs( wxUpdateUIEvent& Event );
	void UI_GroupByClass( wxUpdateUIEvent& Event );

	DECLARE_EVENT_TABLE();
};

/**
 * This simple struct encapsulates suppressing and unsuppressing selection change notifications.
 */
struct FScopedSelectionNotificationHandler
{
	FScopedSelectionNotificationHandler(WxReferencedAssetsBrowser* inBrowser) : RABrowser(inBrowser)
	{
		check(RABrowser);
		RABrowser->DisableSelectionNotification();
	}
	~FScopedSelectionNotificationHandler()
	{
		RABrowser->EnableSelectionNotification();
	}

private:
	WxReferencedAssetsBrowser* RABrowser;
};

#endif // __REFERENCEDASSETSBROWSER_H__
