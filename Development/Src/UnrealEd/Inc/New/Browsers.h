/*=============================================================================
	Browsers.h: The various browser windows.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/*-----------------------------------------------------------------------------
	WxBrowser.

	The base class for all browser windows.
-----------------------------------------------------------------------------*/

class WxBrowser : public WxDockableWindow
{
public:
	WxBrowser( wxWindow* InParent, wxWindowID InID, EDockableWindowType InType, FString InDockingName );
	~WxBrowser();

	FString CaptionText;

	virtual void InitialUpdate();
	virtual void Update();
	virtual void Activated();
};

/*-----------------------------------------------------------------------------
	WxGroupBrowser.
-----------------------------------------------------------------------------*/

class WxGroupBrowser : public WxBrowser
{
public:
	WxGroupBrowser( wxWindow* InParent, wxWindowID InID );
	~WxGroupBrowser();

	void OnEditNewGroup( wxCommandEvent& In );
	void OnEditRenameGroup( wxCommandEvent& In );
	void OnEditDeleteGroup( wxCommandEvent& In );
	void OnEditAddToGroup( wxCommandEvent& In );
	void OnEditDeleteFromGroup( wxCommandEvent& In );
	void OnEditSelect( wxCommandEvent& In );
	void OnEditDeselect( wxCommandEvent& In );
	void OnViewRefresh( wxCommandEvent& In );

	void OnSize( wxSizeEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxLayerWindow.
-----------------------------------------------------------------------------*/

class WxLayerWindow : public wxWindow
{
public:
	WxLayerWindow()
	{}
	WxLayerWindow( wxWindow* InParent, UEdLayer* InLayer );
	virtual ~WxLayerWindow();

	enum { Height=64 };

	UEdLayer* Layer;
	class WxLayerBrowser* LayerBrowser;

	wxCheckBox* VisibleCheck;

	void OnPaint( wxPaintEvent& In );
	void OnLeftButtonDown( wxMouseEvent& In );
	void OnCheckChanged( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxLayerBrowser.
-----------------------------------------------------------------------------*/

class WxLayerBrowser : public WxBrowser
{
public:
	WxLayerBrowser( wxWindow* InParent, wxWindowID InID );
	~WxLayerBrowser();

	wxSplitterWindow* SplitterWnd;
	wxScrolledWindow* LeftWindow;
	WxPropertyWindow* RightWindow;

	TArray<WxLayerWindow*> LayerWindows;
	UEdLayer* SelectedLayer;

	void AddLayer( UEdLayer* InLayer );
	void ClearLayers();
	void LayoutLayerWindows();
	void SelectLayer( WxLayerWindow* InLayerWindow );
	void UpdatePropertyWindow();

	virtual void Update();

	void OnSize( wxSizeEvent& In );
	void OnLeftButtonDown( wxMouseEvent& In );
	void OnEditNew( wxCommandEvent& In );
	void OnEditDelete( wxCommandEvent& In );
	void OnEditAddToLayer( wxCommandEvent& In );
	void OnEditRemoveFromLayer( wxCommandEvent& In );
	void OnEditSelect( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxActorBrowser.
-----------------------------------------------------------------------------*/

class EABItemData : public wxTreeItemData
{
public:
	EABItemData( UClass* In ) : Class( In )
	{}
	UClass* Class;
};

class WxActorBrowser : public WxBrowser
{
public:
	WxActorBrowser( wxWindow* InParent, wxWindowID InID );
	~WxActorBrowser();

	wxCheckBox *ActorAsParentCheck, *PlaceableCheck;
	wxTreeCtrl *TreeCtrl;
	wxStaticText *FullPathStatic;
	wxListBox *PackagesList;
	wxTreeItemId RootId;

	virtual void Update();

	void UpdateActorList();
	UBOOL HasChildren( const UClass* InParent, UBOOL InPlaceableOnly );
	void AddChildren( wxTreeItemId InID );
	void UpdatePackageList();

	void OnFileOpen( wxCommandEvent& In );
	void OnFileExportAll( wxCommandEvent& In );
	void OnItemExpanding( wxTreeEvent& In );
	void OnSelChanged( wxTreeEvent& In );
	void OnUseActorAsParent( wxCommandEvent& In );
	void OnPlaceableClassesOnly( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};


/*-----------------------------------------------------------------------------
	WxGenericBrowser.
-----------------------------------------------------------------------------*/

// Data specific to each browser

enum EGenericBrowser_TreeCtrlImages
{
	GBTCI_None = -1,
	GBTCI_FolderClosed,
	GBTCI_FolderOpen,
	GBTCI_Resource,
	GBTCI_All,
};

class WxBrowserData
{
public:
	WxBrowserData()
	{
		check(0);	// bad ctor
	}
	WxBrowserData( EGenericBrowser_TreeCtrlImages InTreeCtrlImg )
	{
		TreeCtrlImg = InTreeCtrlImg;
		ScrollMax = ScrollPos = 0;
		ZoomFactor = 0.25f;
		FixedSz = 0;
		NameFilter = TEXT("");
	}
	~WxBrowserData()
	{
	}

	INT ScrollMax, ScrollPos;
	FLOAT ZoomFactor;
	INT FixedSz;
	FString NameFilter;
	EGenericBrowser_TreeCtrlImages TreeCtrlImg;
};

class WxTreeLeafData : public wxTreeItemData
{
public:
	WxTreeLeafData()
	{ check(0);	}	// Wrong ctor
	WxTreeLeafData( UObject* InData )
	{
		Data = InData;
	}
	~WxTreeLeafData()
	{
	}

	UObject* Data;

	inline UObject* GetId()
	{
		return Data;
	}
};

class WxGBLeftContainer : public wxPanel, public WxSerializableWindow
{
public:
	WxGBLeftContainer( wxWindow* InParent );
	~WxGBLeftContainer();

	class WxGenericBrowser* GenericBrowser;

	TMap<UPackage*,wxTreeItemId> PackageMap;

	wxComboBox* ResourceTypeCombo;
	wxTreeCtrl* TreeCtrl;

	void SyncToObject( UObject* InObject );

	TArray<UGenericBrowserType*> ResourceTypes;
	UGenericBrowserType* CurrentResourceType;

	void UpdateTree();
	void Update();
	void GetSelectedPackages( TArray<UPackage*>* InPackages );
	UClass* GetCurrentClass();

	void OnTreeExpanding( wxTreeEvent& In );
	void OnTreeCollapsed( wxTreeEvent& In );
	void OnTreeSelChanged( wxTreeEvent& In );
	void OnResourceTypeSelChanged( wxCommandEvent& In );
	void OnSize( wxSizeEvent& In );

	void Serialize(FArchive& Ar);

	DECLARE_EVENT_TABLE()
};

enum ERightViewMode
{
	RVM_List		= 0,
	RVM_Preview		= 1,
	RVM_Thumbnail	= 2,
};

struct HObject : HHitProxy
{
	DECLARE_HIT_PROXY(HObject,HHitProxy);

	UObject*	Object;

	HObject(UObject* InObject): Object(InObject) {}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Object;
	}
};

class WxGBRightContainer : public wxPanel, FViewportClient
{
public:
	WxGBRightContainer( wxWindow* InParent );
	~WxGBRightContainer();

	class WxGenericBrowser* GenericBrowser;
	ERightViewMode ViewMode;

	wxSplitterWindow *SplitterWindowV, *SplitterWindowH;
	WxGBViewToolBar* ToolBar;
	wxListView* ListView;
	class EViewportHolder* ViewportHolder;
	FChildViewport* Viewport;
	EThumbnailPrimType ThumbnailPrimType;

	virtual void InputKey(FChildViewport* Viewport,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f);
	virtual void Draw(FChildViewport* Viewport,FRenderInterface* RI);

	void UpdateList();
	void UpdateUI();
	void Update();
	void SetViewMode( ERightViewMode InViewMode );
	void GetVisibleObjects( TArray<UObject*>* InVisibleObjects );
	void GetThumbnailDimensions( INT InWidth, INT InHeight, TArray<FString>* InLabels, FRenderInterface* InRI, INT* InXL, INT* InYL );
	void GetObjectsInPackages( TArray<UPackage*>* InPackages, TArray<UObject*>* InObjects );
	void ShowContextMenu( UObject* InObject );
	void ShowNewObjectMenu();
	void SyncToObject( UObject* InObject );

	/** If this is non-NULL, the viewport will be scrolled to make sure this object is in view when it draws next. */
	UObject* SyncToNextDraw;

	void OnSize( wxSizeEvent& In );
	void OnViewMode( wxCommandEvent& In );
	void UI_ViewMode( wxUpdateUIEvent& In );
	void OnPrimType( wxCommandEvent& In );
	void UI_PrimType( wxUpdateUIEvent& In );
	void OnSelectionChanged_Select( wxListEvent& In );
	void OnSelectionChanged_Deselect( wxListEvent& In );
	void OnListViewRightClick( wxListEvent& In );
	void OnListItemActivated( wxListEvent& In );
	void OnZoomFactorSelChange( wxCommandEvent& In );
	void OnNameFilterChanged( wxCommandEvent& In );
	void OnScroll( wxScrollEvent& InEvent );

	DECLARE_EVENT_TABLE()
};

class WxGenericBrowser : public WxBrowser
{
public:
	WxGenericBrowser( wxWindow* InParent, wxWindowID InID );
	~WxGenericBrowser();

	TMap<UClass*,WxBrowserData*> BrowserData;				// Individual data for each type of browser

	WxGBLeftContainer* LeftContainer;
	WxGBRightContainer* RightContainer;
	wxSplitterWindow* SplitterWindow;
	wxImageList *ImageListTree, *ImageListView;
	FString LastExportPath, LastImportPath, LastOpenPath;

	// Array of UFactory classes which can create new objects.
	TArray<UClass*>	FactoryNewClasses;

	WxBrowserData* FindBrowserData( UClass* InClass );
	void SelectPackage( UPackage* InPackage );

	virtual void InitialUpdate();
	virtual void Update();
	UPackage* GetTopPackage();
	UPackage* GetGroup();
	void SyncToObject( UObject* InObject );

	void OnSize( wxSizeEvent& In );
	void OnViewRefresh( wxCommandEvent& In );
	void OnFileOpen( wxCommandEvent& In );
	void OnFileSave( wxCommandEvent& In );
	void OnFileNew( wxCommandEvent& In );
	void OnFileImport( wxCommandEvent& In );

	void OnCopyReference(wxCommandEvent& In);
	void OnObjectProperties(wxCommandEvent& In);
	void OnObjectDuplicate(wxCommandEvent& In);
	void OnObjectDelete(wxCommandEvent& In);
	void OnObjectRename(wxCommandEvent& In);
	void OnObjectExport(wxCommandEvent& In);
	void OnContextObjectNew(wxCommandEvent& In);
	void OnObjectEditor(wxCommandEvent& In);
	void OnObjectCustomCommand( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
