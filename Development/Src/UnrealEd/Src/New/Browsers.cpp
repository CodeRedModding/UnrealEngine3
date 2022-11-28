
#include "UnrealEd.h"
#include "UnTerrain.h"
#include "EngineSoundClasses.h"
#include "EngineParticleClasses.h"
#include "EngineAnimClasses.h"
#include "EngineSequenceClasses.h"

#include "AnimSetViewer.h"
#include "UnLinkedObjEditor.h"
#include "SoundCueEditor.h"

#include "..\..\Launch\Resources\resource.h"

/*-----------------------------------------------------------------------------
	WxBrowser.
-----------------------------------------------------------------------------*/

WxBrowser::WxBrowser( wxWindow* InParent, wxWindowID InID, EDockableWindowType InType, FString InDockingName )
	: WxDockableWindow( (wxWindow*)InParent, InID, InType, InDockingName )
{
}

WxBrowser::~WxBrowser()
{
}

// Called when the browser is first constructed.  Gives it a chance to fill controls, lists and such.

void WxBrowser::InitialUpdate()
{
}

void WxBrowser::Update()
{
}

// Called when the browser is getting activated (becoming the visible window in it's dockable frame)

void WxBrowser::Activated()
{
	((wxFrame*)GetParent()->GetParent())->SetMenuBar( MenuBar );
	((wxFrame*)GetParent()->GetParent())->SetToolBar( ToolBar );
}

/*-----------------------------------------------------------------------------
	WxGroupBrowser.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxGroupBrowser, WxBrowser )
	EVT_SIZE( WxGroupBrowser::OnSize )
	EVT_MENU( IDMN_GB_NEW_GROUP, WxGroupBrowser::OnEditNewGroup )
	EVT_MENU( IDMN_GB_RENAME_GROUP, WxGroupBrowser::OnEditRenameGroup )
	EVT_MENU( IDMN_GB_DELETE_GROUP, WxGroupBrowser::OnEditDeleteGroup )
	EVT_MENU( IDMN_GB_ADD_TO_GROUP, WxGroupBrowser::OnEditAddToGroup )
	EVT_MENU( IDMN_GB_DELETE_FROM_GROUP, WxGroupBrowser::OnEditDeleteFromGroup )
	EVT_MENU( IDMN_GB_SELECT, WxGroupBrowser::OnEditSelect )
	EVT_MENU( IDMN_GB_DESELECT, WxGroupBrowser::OnEditDeselect )
	EVT_MENU( IDMN_GB_REFRESH, WxGroupBrowser::OnViewRefresh )
END_EVENT_TABLE()

WxGroupBrowser::WxGroupBrowser( wxWindow* InParent, wxWindowID InID )
	: WxBrowser( InParent, InID, DWT_GROUP_BROWSER, TEXT("Groups") )
{
	::SetParent( GApp->BrowserGroup->hWnd, (HWND)GetHandle() );
	MenuBar = new WxMBGroupBrowser();
}

WxGroupBrowser::~WxGroupBrowser()
{
}

void WxGroupBrowser::OnSize( wxSizeEvent& In )
{
	wxRect rc = GetClientRect();

	::SetWindowPos( GApp->BrowserGroup->hWnd, HWND_TOP, 0, 0, rc.GetWidth(), rc.GetHeight(), SWP_SHOWWINDOW );

}

void WxGroupBrowser::OnEditNewGroup( wxCommandEvent& In )		{	SendMessageX( GApp->BrowserGroup->hWnd, WM_COMMAND, IDMN_GB_NEW_GROUP, 0L );	}
void WxGroupBrowser::OnEditRenameGroup( wxCommandEvent& In )		{	SendMessageX( GApp->BrowserGroup->hWnd, WM_COMMAND, IDMN_GB_RENAME_GROUP, 0L );	}
void WxGroupBrowser::OnEditDeleteGroup( wxCommandEvent& In )		{	SendMessageX( GApp->BrowserGroup->hWnd, WM_COMMAND, IDMN_GB_DELETE_GROUP, 0L );	}
void WxGroupBrowser::OnEditAddToGroup( wxCommandEvent& In )		{	SendMessageX( GApp->BrowserGroup->hWnd, WM_COMMAND, IDMN_GB_ADD_TO_GROUP, 0L );	}
void WxGroupBrowser::OnEditDeleteFromGroup( wxCommandEvent& In )	{	SendMessageX( GApp->BrowserGroup->hWnd, WM_COMMAND, IDMN_GB_DELETE_FROM_GROUP, 0L );	}
void WxGroupBrowser::OnEditSelect( wxCommandEvent& In )			{	SendMessageX( GApp->BrowserGroup->hWnd, WM_COMMAND, IDMN_GB_SELECT, 0L );	}
void WxGroupBrowser::OnEditDeselect( wxCommandEvent& In )		{	SendMessageX( GApp->BrowserGroup->hWnd, WM_COMMAND, IDMN_GB_DESELECT, 0L );	}
void WxGroupBrowser::OnViewRefresh( wxCommandEvent& In )			{	SendMessageX( GApp->BrowserGroup->hWnd, WM_COMMAND, IDMN_GB_REFRESH, 0L );	}

/*-----------------------------------------------------------------------------
	WxLayerWindow.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxLayerWindow, wxWindow )
	EVT_PAINT( WxLayerWindow::OnPaint )
	EVT_LEFT_DOWN( WxLayerWindow::OnLeftButtonDown )
	EVT_CHECKBOX( IDCK_VISIBLE, WxLayerWindow::OnCheckChanged )
END_EVENT_TABLE()

WxLayerWindow::WxLayerWindow( wxWindow* InParent, UEdLayer* InLayer )
	: wxWindow( InParent, -1, wxDefaultPosition, wxDefaultSize )
	,	Layer(InLayer)
{
	LayerBrowser = (WxLayerBrowser*)(InParent->GetParent()->GetParent());

	VisibleCheck = new wxCheckBox( this, IDCK_VISIBLE, TEXT("Visible?"), wxPoint(6,26), wxSize(64,12) );
	VisibleCheck->SetValue( Layer->bVisible );
}

WxLayerWindow::~WxLayerWindow()
{
}

void WxLayerWindow::OnPaint( wxPaintEvent& In )
{
	wxPaintDC dc(this);
	wxRect rc = GetClientRect();

	dc.SetFont( *wxNORMAL_FONT );

	if( LayerBrowser->SelectedLayer == Layer )
	{
		wxColour basecolor = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
		basecolor.Set( basecolor.Red(), basecolor.Green(), basecolor.Blue() + 64 );
		wxColour darkercolor( basecolor.Red()-32, basecolor.Green()-32, basecolor.Blue()-32 );

		dc.SetPen( *wxMEDIUM_GREY_PEN );
		dc.SetBrush( wxBrush( wxColour(64,64,64), wxSOLID ) );
		dc.DrawRoundedRectangle( rc.x+1,rc.y+1, rc.width-2,rc.height-2, 5 );
		dc.SetPen( *wxTRANSPARENT_PEN );
		dc.SetBrush( wxBrush( darkercolor, wxSOLID ) );
		dc.DrawRectangle( rc.x+2,rc.y+20, rc.width-4,rc.height-27 );

		VisibleCheck->SetBackgroundColour( darkercolor );

		dc.SetTextForeground( *wxWHITE );
		dc.SetTextBackground( darkercolor );

		wxFont Font = dc.GetFont();
		Font.SetWeight( wxBOLD );
		dc.SetFont( Font );
	}
	else
	{
		dc.SetPen( *wxMEDIUM_GREY_PEN );
		dc.SetBrush( *wxTRANSPARENT_BRUSH );
		dc.DrawRoundedRectangle( rc.x+1,rc.y+1, rc.width-2,rc.height-2, 5 );

		VisibleCheck->SetBackgroundColour( wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE) );
	}

	INT Count = 0;
	for( TActorIterator It( GUnrealEd->GetLevel() ) ; It ; ++It )
	{
		if( It->Layer == Layer )
		{
			Count++;
		}
	}

	FString Desc = FString::Printf( TEXT("%s (%d actors)"), *Layer->Desc, Count );

	dc.DrawText( *Desc, rc.x+6,rc.y+3 );
}

void WxLayerWindow::OnLeftButtonDown( wxMouseEvent& In )
{
	LayerBrowser->SelectLayer( this );
}

void WxLayerWindow::OnCheckChanged( wxCommandEvent& In )
{
	Layer->bVisible = !Layer->bVisible;
	LayerBrowser->SelectLayer( this );
}

/*-----------------------------------------------------------------------------
	WxLayerBrowser.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxLayerBrowser, WxBrowser )
	EVT_SIZE( WxLayerBrowser::OnSize )
	EVT_MENU( IDM_LB_NEW, WxLayerBrowser::OnEditNew )
	EVT_MENU( IDM_LB_DELETE, WxLayerBrowser::OnEditDelete )
	EVT_MENU( IDM_LB_ADDTOLAYER, WxLayerBrowser::OnEditAddToLayer )
	EVT_MENU( IDM_LB_REMOVEFROMLAYER, WxLayerBrowser::OnEditRemoveFromLayer )
	EVT_MENU( IDM_LB_SELECT, WxLayerBrowser::OnEditSelect )
END_EVENT_TABLE()

WxLayerBrowser::WxLayerBrowser( wxWindow* InParent, wxWindowID InID )
	: WxBrowser( InParent, InID, DWT_LAYER_BROWSER, TEXT("Layers") )
{
	MenuBar = new WxMBLayerBrowser();
	//ToolBar = new WxMaterialEditorToolBar( (wxWindow*)this, -1 );

	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(sizer);
	SetAutoLayout(TRUE);

	SplitterWnd = new wxSplitterWindow(this, ID_SPLITTERWINDOW, wxDefaultPosition, wxSize(100, 100), wxSP_3D|wxSP_FULLSASH );

	LeftWindow = new wxScrolledWindow( SplitterWnd, ID_LAYER_LIST );
	RightWindow = new WxPropertyWindow( (wxWindow*)SplitterWnd, GUnrealEd );

	SplitterWnd->SplitVertically( LeftWindow, RightWindow, 350 );
	sizer->Add(SplitterWnd, 1, wxGROW|wxALL, 0);

	GetSizer()->Fit(this);
	GetSizer()->SetSizeHints(this);

	SelectedLayer = NULL;
}

WxLayerBrowser::~WxLayerBrowser()
{
}

void WxLayerBrowser::OnSize( wxSizeEvent& In )
{
	wxRect rc = GetClientRect();
	SplitterWnd->SetSize( rc );
	LayoutLayerWindows();
}

void WxLayerBrowser::Update()
{
	// Fill the main control with all existing layers

	ClearLayers();

	ALevelInfo* li = GUnrealEd->GetLevel()->GetLevelInfo();
	for( INT x = 0 ; x < li->Layers.Num() ; ++x )
	{
		AddLayer( li->Layers(x) );
	}

	LayoutLayerWindows();
}

void WxLayerBrowser::OnEditNew( wxCommandEvent& In )
{
	// Create a new layer

	SelectedLayer = ConstructObject<UEdLayer>( UEdLayer::StaticClass(), GUnrealEd->GetLevel() );
	AddLayer( SelectedLayer );

	// Add the new layer into the master list

	ALevelInfo* li = GUnrealEd->GetLevel()->GetLevelInfo();
	li->Layers.AddItem( SelectedLayer );

	// Update the left control to show the new layer list

	Update();

	// Update the property window

	UpdatePropertyWindow();

	// Add any selected actors n the world to the new layer

    wxCommandEvent Event;
	OnEditAddToLayer( Event );
}

void WxLayerBrowser::OnEditDelete( wxCommandEvent& In )
{
	// Update the level info

	ALevelInfo* li = GUnrealEd->GetLevel()->GetLevelInfo();
	INT idx = li->Layers.FindItemIndex( SelectedLayer );
	li->Layers.Remove( idx );

	delete SelectedLayer;
	SelectedLayer = NULL;

	// Delete this layer and remove references to it in the level actors

	for( TActorIterator It( GUnrealEd->GetLevel() ) ; It ; ++It )
	{
		if( It->Layer == SelectedLayer )
		{
			It->Layer = NULL;
		}
	}

	// Update the left control to show the new layer list

	Update();

	// Update the property window

	UpdatePropertyWindow();
}

void WxLayerBrowser::OnEditAddToLayer( wxCommandEvent& In )
{
	if( !SelectedLayer )
		return;

	// Assign all selected actors to the currently selected layer

	for( TSelectedActorIterator It( GUnrealEd->GetLevel() ) ; It ; ++It )
	{
		// Can't add the builder brush to layers.

		if( GEditor->GetLevel()->Actors(1) == *It )
		{
			continue;
		}

		It->Layer = SelectedLayer;
	}

	Refresh();
}

void WxLayerBrowser::OnEditRemoveFromLayer( wxCommandEvent& In )
{
	// Remove any references to the current layer from any selected actors

	for( TSelectedActorIterator It( GUnrealEd->GetLevel() ) ; It ; ++It )
	{
		if( It->Layer == SelectedLayer )
		{
			It->Layer = NULL;
		}
	}

	Refresh();
}

void WxLayerBrowser::OnEditSelect( wxCommandEvent& In )
{
	GUnrealEd->SelectNone( GUnrealEd->GetLevel(), 1 );

	// Assign all selected actors to the currently selected layer

	for( TActorIterator It( GUnrealEd->GetLevel() ) ; It ; ++It )
	{
		if( It->Layer == SelectedLayer )
		{
			GUnrealEd->SelectActor( GUnrealEd->GetLevel(), *It );
		}
	}

	Refresh();
}

void WxLayerBrowser::AddLayer( UEdLayer* InLayer )
{
	// If this layer is already listed, leave.

	for( INT x = 0 ; x < LayerWindows.Num() ; ++x )
	{
		if( LayerWindows(x)->Layer == InLayer )
		{
			return;
		}
	}

	// Create a new layer window to represent InLayer

	WxLayerWindow* lw = new WxLayerWindow( LeftWindow, InLayer );
	LayerWindows.AddItem( lw );

	LayoutLayerWindows();
}

void WxLayerBrowser::ClearLayers()
{
	LeftWindow->DestroyChildren();
	LayerWindows.Empty();
}

void WxLayerBrowser::LayoutLayerWindows()
{
	// Stack the layer windows

	INT Top = 0;
	wxRect rc = LeftWindow->GetClientRect();

	for( INT x = 0 ; x < LayerWindows.Num() ; ++x )
	{
		LayerWindows(x)->SetSize( 0,Top,rc.GetWidth(),WxLayerWindow::Height );
		Top += WxLayerWindow::Height;
	}

	// Tell the scrolling window how large the virtual area is

	LeftWindow->SetVirtualSize( rc.GetWidth(), LayerWindows.Num() * WxLayerWindow::Height );
}

void WxLayerBrowser::SelectLayer( WxLayerWindow* InLayerWindow )
{
	SelectedLayer = InLayerWindow->Layer;
	UpdatePropertyWindow();
	Refresh();

	GCallback->Send( CALLBACK_RedrawAllViewports );
}

void WxLayerBrowser::UpdatePropertyWindow()
{
	RightWindow->SetObject( SelectedLayer, 1, 1, 0 );
}

/*-----------------------------------------------------------------------------
	WxActorBrowser.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxActorBrowser, WxBrowser )
	EVT_MENU( IDMN_AB_FileOpen, WxActorBrowser::OnFileOpen )
	EVT_MENU( IDMN_AB_EXPORT_ALL, WxActorBrowser::OnFileExportAll )
END_EVENT_TABLE()

WxActorBrowser::WxActorBrowser( wxWindow* InParent, wxWindowID InID )
	: WxBrowser( InParent, InID, DWT_ACTOR_BROWSER, TEXT("Actors") )
{
	Panel = (wxPanel*)wxXmlResource::Get()->LoadPanel( this, TEXT("ID_PANEL_ACTORCLASSBROWSER") );
	Panel->Fit();

	TreeCtrl = wxDynamicCast( FindWindow( XRCID( "IDTC_CLASSES" ) ), wxTreeCtrl );
	ActorAsParentCheck = wxDynamicCast( FindWindow( XRCID( "IDCK_USE_ACTOR_AS_PARENT" ) ), wxCheckBox );
	PlaceableCheck = wxDynamicCast( FindWindow( XRCID( "IDCK_PLACEABLE_CLASSES_ONLY" ) ), wxCheckBox );
	FullPathStatic = wxDynamicCast( FindWindow( XRCID( "IDST_FULLPATH" ) ), wxStaticText );
	PackagesList = wxDynamicCast( FindWindow( XRCID( "IDLB_PACKAGES" ) ), wxListBox );

	Update();

	MenuBar = new WxMBActorBrowser();

	ADDEVENTHANDLER( XRCID("IDTC_CLASSES"), wxEVT_COMMAND_TREE_ITEM_EXPANDING, &WxActorBrowser::OnItemExpanding );
	ADDEVENTHANDLER( XRCID("IDTC_CLASSES"), wxEVT_COMMAND_TREE_SEL_CHANGED, &WxActorBrowser::OnSelChanged );
	ADDEVENTHANDLER( XRCID("IDCK_USE_ACTOR_AS_PARENT"), wxEVT_COMMAND_CHECKBOX_CLICKED , &WxActorBrowser::OnUseActorAsParent );
	ADDEVENTHANDLER( XRCID("IDCK_PLACEABLE_CLASSES_ONLY"), wxEVT_COMMAND_CHECKBOX_CLICKED , &WxActorBrowser::OnPlaceableClassesOnly );
}

WxActorBrowser::~WxActorBrowser()
{
}

void WxActorBrowser::Update()
{
	UpdatePackageList();
	UpdateActorList();
}

void WxActorBrowser::UpdatePackageList()
{
	TArray<UPackage*> Packages;
	GUnrealEd->GetPackageList( &Packages, NULL );

	PackagesList->Clear();

	for( INT x = 0 ; x < Packages.Num() ; ++x )
		PackagesList->Append( Packages(x)->GetName(), Packages(x) );
}

void WxActorBrowser::UpdateActorList()
{
	TreeCtrl->DeleteAllItems();

	if( ActorAsParentCheck->IsChecked() )
		RootId = TreeCtrl->AddRoot( TEXT("Actor"), -1, -1, new EABItemData( AActor::StaticClass() ) );
	else
		RootId = TreeCtrl->AddRoot( TEXT("Object"), -1, -1, new EABItemData( UObject::StaticClass() ) );

	AddChildren( RootId );
	TreeCtrl->Expand( RootId );
}

UBOOL WxActorBrowser::HasChildren( const UClass* InParent, UBOOL InPlaceableOnly )
{
	for( TObjectIterator<UClass> It ; It ; ++It )
	{
		if( It->GetSuperClass() == InParent )
		{
			if( It->ClassFlags & CLASS_Hidden )
				return 0;
			else if( !InPlaceableOnly || (It->ClassFlags & CLASS_Placeable) )
				return 1;
			else
				if( HasChildren( *It, InPlaceableOnly ) )
					return 1;
		}
	}
	return 0;
}

void WxActorBrowser::AddChildren( wxTreeItemId InID )
{
	FWaitCursor wc;

	EABItemData* ItemData = ((EABItemData*)TreeCtrl->GetItemData( InID ));
	check(ItemData);
	UClass*	Class = ItemData->Class;

	UBOOL bPlaceableOnly = PlaceableCheck->IsChecked();

	for( TObjectIterator<UClass> It ; It ; ++It )
	{
		if( It->GetSuperClass() == Class )
		{
			if( (It->ClassFlags & CLASS_Hidden) || (bPlaceableOnly && !(It->ClassFlags & CLASS_Placeable) && !HasChildren( *It, 1 )) )
				continue;

			wxTreeItemId Wk = TreeCtrl->AppendItem( InID, It->GetName(), -1, -1, new EABItemData( *It ) );

			// Mark placeable classes in bold.
			if( (It->ClassFlags & CLASS_Placeable) && !(It->ClassFlags & CLASS_Abstract) )
				TreeCtrl->SetItemBold( Wk );
			else
				TreeCtrl->SetItemTextColour( Wk, wxColour(96,96,96) );

            TreeCtrl->SetItemHasChildren( Wk, HasChildren( *It, bPlaceableOnly ) ? true : false );
		}
	}

	TreeCtrl->SortChildren( InID );
}

void WxActorBrowser::OnFileOpen( wxCommandEvent& In )
{
	wxFileDialog OpenFileDialog( this, 
		TEXT("Open Package"), 
		*GSys->Paths(0),
		TEXT(""),
		TEXT("Class Packages (*.u)|*.u|All Files|*.*\0\0"),
		wxOPEN | wxFILE_MUST_EXIST | wxHIDE_READONLY | wxMULTIPLE,
		wxDefaultPosition);

	if( OpenFileDialog.ShowModal() == wxID_OK )
	{
		wxArrayString	OpenFilePaths;
		OpenFileDialog.GetPaths(OpenFilePaths);

		for(UINT FileIndex = 0;FileIndex < OpenFilePaths.Count();FileIndex++)
			GUnrealEd->Exec(*FString::Printf(TEXT("OBJ LOAD FILE=\"%s\""),*FString(OpenFilePaths[FileIndex])));
	}

	GCallback->Send( CALLBACK_RefreshEditor, ERefreshEditor_AllBrowsers );
	Update();
}

void WxActorBrowser::OnFileExportAll( wxCommandEvent& In )		
{
	if( ::MessageBox( (HWND)GetHandle(), TEXT("This option will export all classes to text .uc files which can later be rebuilt. Do you want to do this?"), TEXT("Export classes to *.uc files"), MB_YESNO) == IDYES)
	{
		GUnrealEd->Exec( TEXT("CLASS SPEW") );
	}
}

void WxActorBrowser::OnItemExpanding( wxTreeEvent& In )
{
	TreeCtrl->DeleteChildren( In.GetItem() );
	AddChildren( In.GetItem() );
}

void WxActorBrowser::OnSelChanged( wxTreeEvent& In )
{
	if( ((EABItemData*)TreeCtrl->GetItemData( In.GetItem() )) )
	GUnrealEd->SetCurrentClass( ((EABItemData*)TreeCtrl->GetItemData( In.GetItem() ))->Class );
	FullPathStatic->SetLabel( *GSelectionTools.GetTop<UClass>()->GetPathName() );
}

void WxActorBrowser::OnUseActorAsParent( wxCommandEvent& In )
{
	UpdateActorList();
}

void WxActorBrowser::OnPlaceableClassesOnly( wxCommandEvent& In )
{
	UpdateActorList();
}

/*-----------------------------------------------------------------------------
	WxGenericBrowser.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxGenericBrowser, WxBrowser )
	EVT_SIZE( WxGenericBrowser::OnSize )
	EVT_MENU( IDMN_REFRESH, WxGenericBrowser::OnViewRefresh )
	EVT_MENU( IDMN_FileOpen, WxGenericBrowser::OnFileOpen )
	EVT_MENU( IDMN_FileSave, WxGenericBrowser::OnFileSave )
	EVT_MENU( IDM_NEW, WxGenericBrowser::OnFileNew )
	EVT_MENU( IDM_IMPORT, WxGenericBrowser::OnFileImport )
	EVT_MENU( IDM_EXPORT, WxGenericBrowser::OnObjectExport )
	EVT_MENU_RANGE( ID_NEWOBJ_START, ID_NEWOBJ_END, WxGenericBrowser::OnContextObjectNew )

	EVT_MENU( IDMN_ObjectContext_CopyReference, WxGenericBrowser::OnCopyReference )
	EVT_MENU( IDMN_ObjectContext_Properties, WxGenericBrowser::OnObjectProperties )
	EVT_MENU( IDMN_ObjectContext_Duplicate, WxGenericBrowser::OnObjectDuplicate )
	EVT_MENU( IDMN_ObjectContext_Rename, WxGenericBrowser::OnObjectRename )
	EVT_MENU( IDMN_ObjectContext_Delete, WxGenericBrowser::OnObjectDelete )
	EVT_MENU( IDMN_ObjectContext_Export, WxGenericBrowser::OnObjectExport )
	EVT_MENU( IDMN_ObjectContext_Editor, WxGenericBrowser::OnObjectEditor )
	EVT_MENU_RANGE( IDMN_ObjectContext_Custom_START, IDMN_ObjectContext_Custom_END, WxGenericBrowser::OnObjectCustomCommand )

END_EVENT_TABLE()

WxGenericBrowser::WxGenericBrowser( wxWindow* InParent, wxWindowID InID )
	: WxBrowser( InParent, InID, DWT_GENERIC_BROWSER, TEXT("Browser") )
{
	MenuBar = new WxMBGenericBrowser();
	
	ImageListTree = new wxImageList( 16, 15 );
	ImageListView = new wxImageList( 16, 15 );

	ImageListTree->Add( WxBitmap( "FolderClosed" ), wxColor( 192,192,192 ) );
	ImageListTree->Add( WxBitmap( "FolderOpen" ), wxColor( 192,192,192 ) );
	ImageListTree->Add( WxBitmap( "Texture" ), wxColor( 192,192,192 ) );
	ImageListTree->Add( WxBitmap( "Sound" ), wxColor( 192,192,192 ) );
	ImageListTree->Add( WxBitmap( "Prefab" ), wxColor( 192,192,192 ) );
	ImageListTree->Add( WxBitmap( "StaticMesh" ), wxColor( 192,192,192 ) );
	ImageListTree->Add( WxBitmap( "Material" ), wxColor( 192,192,192 ) );
	ImageListTree->Add( WxBitmap( "SkeletalMesh" ), wxColor( 192,192,192 ) );
	ImageListTree->Add( WxBitmap( "PhysicsAsset" ), wxColor( 192,192,192 ) );

	*ImageListView = *ImageListTree;

	SplitterWindow = new wxSplitterWindow( this, -1, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE | wxSP_BORDER );

	LeftContainer = new WxGBLeftContainer( SplitterWindow );
	RightContainer = new WxGBRightContainer( SplitterWindow );

	SplitterWindow->SplitVertically( LeftContainer, RightContainer, STD_SPLITTER_SZ );
	LeftContainer->TreeCtrl->AssignImageList( ImageListTree );
	LeftContainer->GenericBrowser = this;

	RightContainer->ListView->AssignImageList( ImageListView, wxIMAGE_LIST_SMALL );
	RightContainer->GenericBrowser = this;

	LastExportPath = LastImportPath = LastOpenPath = GApp->LastDir[LD_GENERIC];

	for( INT r = 0 ; r < LeftContainer->ResourceTypes.Num() ; ++r )
	{
		for( INT c = 0 ; c < LeftContainer->ResourceTypes(r)->SupportInfo.Num() ; ++c )
		{
			BrowserData.Set( LeftContainer->ResourceTypes(r)->SupportInfo(c).Class, new WxBrowserData( GBTCI_Resource ) );
		}
	}

	InitialUpdate();

	// Initialise array of UFactory classes that can create new objects.
	for( TObjectIterator<UClass> It ; It ; ++It )
	{
		if( It->IsChildOf(UFactory::StaticClass()) && !(It->ClassFlags & CLASS_Abstract) )
		{
			UClass* FactoryClass = *It;

			UFactory* Factory = ConstructObject<UFactory>( FactoryClass );
			if( Factory->bCreateNew )
			{
				FactoryNewClasses.AddItem( FactoryClass );
			}
			delete Factory;
		}
	}
}

WxGenericBrowser::~WxGenericBrowser()
{
}

WxBrowserData* WxGenericBrowser::FindBrowserData( UClass* InClass )
{
	WxBrowserData* bd = *BrowserData.Find( InClass );
	check(bd);		// Unknown class type!
	return bd;
}

// Finds and selects InPackage in the tree control

void WxGenericBrowser::SelectPackage( UPackage* InPackage )
{
	wxTreeItemId tid = LeftContainer->TreeCtrl->GetRootItem();
	LONG cookie = 100;
	tid = LeftContainer->TreeCtrl->GetFirstChild( tid, cookie );

	do
	{
		WxTreeLeafData* tld = (WxTreeLeafData*)LeftContainer->TreeCtrl->GetItemData(tid);
		if( tld )
		{
			UPackage* Package = (UPackage*)(tld->Data);
			if( InPackage == Package )
			{
				LeftContainer->TreeCtrl->SelectItem( tid );
				LeftContainer->TreeCtrl->EnsureVisible( tid );
			}
		}

		tid = LeftContainer->TreeCtrl->GetNextChild( tid, cookie );

	} while( tid != NULL );
}

void WxGenericBrowser::InitialUpdate()
{
	Update();
}

void WxGenericBrowser::Update()
{
	LeftContainer->Update();
	RightContainer->Update();
}

void WxGenericBrowser::OnSize( wxSizeEvent& In )
{
	wxRect rc = GetClientRect();
	SplitterWindow->SetSize( rc );

}

void WxGenericBrowser::OnViewRefresh( wxCommandEvent& In )
{
	Update();
}

void WxGenericBrowser::OnFileOpen( wxCommandEvent& In )
{
	// Get the filename
	wxChar* FileTypes = TEXT("All Files|*.*");

	wxFileDialog OpenFileDialog( this, 
		TEXT("Open Package"), 
		*LastOpenPath,
		TEXT(""),
		FileTypes,
		wxOPEN | wxFILE_MUST_EXIST | wxHIDE_READONLY | wxMULTIPLE,
		wxDefaultPosition);

	TArray<UPackage*> NewPackages;

	if( OpenFileDialog.ShowModal() == wxID_OK )
	{
		wxArrayString OpenFilePaths;
		OpenFileDialog.GetPaths(OpenFilePaths);

		for(UINT FileIndex = 0;FileIndex < OpenFilePaths.Count();FileIndex++)
		{
			FFilename filename = OpenFilePaths[FileIndex];
			LastOpenPath = filename.GetPath();
			GUnrealEd->Exec(*FString::Printf(TEXT("OBJ LOAD FILE=\"%s\""),*filename));

			UPackage* Package = Cast<UPackage>(GEngine->StaticFindObject( UPackage::StaticClass(), ANY_PACKAGE, *filename.GetBaseFilename() ) );
			if( Package )
				NewPackages.AddItem( Package );
		}
	}

	Update();

	LeftContainer->TreeCtrl->UnselectAll();
	for( INT x = 0 ; x < NewPackages.Num() ; ++x )
		SelectPackage( NewPackages(x) );

	GFileManager->SetDefaultDirectory();

}

void WxGenericBrowser::OnFileSave( wxCommandEvent& In )
{
	// Generate a list of unique packages

	TArray<UPackage*> WkPackages;
	LeftContainer->GetSelectedPackages(&WkPackages);

	TArray<UPackage*> Packages;
	for( INT x = 0 ; x < WkPackages.Num() ; ++x )
		Packages.AddUniqueItem( (UPackage*)WkPackages(x)->GetOutermost() ? (UPackage*)WkPackages(x)->GetOutermost() : WkPackages(x) );
	
	// Dialog

	FString FileTypes( TEXT("All Files|*.*") );
	for(INT i=0; i<GSys->Extensions.Num(); i++)
		FileTypes += FString::Printf( TEXT("|(*.%s)|*.%s"), *GSys->Extensions(i), *GSys->Extensions(i) );

	for(INT i=0; i<Packages.Num(); i++)
	{
		FString ExistingFile, File, Directory;
		const TCHAR* PackageName = Packages(i)->GetName();

		if( GPackageFileCache->FindPackageFile( PackageName, NULL, ExistingFile ) )
		{
			FString Filename, Extension;
			GPackageFileCache->SplitPath( *ExistingFile, Directory, Filename, Extension );
			File = FString::Printf( TEXT("%s.%s"), *Filename, *Extension );
		}
		else
		{
			Directory = TEXT("");
			File = FString::Printf( TEXT("%s.upk"), PackageName );
		}

		wxFileDialog SaveFileDialog( this, 
			TEXT("Save Package"), 
			*Directory,
			*File,
			*FileTypes,
			wxSAVE,
			wxDefaultPosition);

		FString SaveFileName;

		if( SaveFileDialog.ShowModal() == wxID_OK )
		{
			SaveFileName = FString( SaveFileDialog.GetPath() );

			if( GFileManager->IsReadOnly( *SaveFileName ) )
			{
				appMsgf( 0, TEXT("Couldn't save package - maybe file is read-only?") );
			}
			else
			{
				GUnrealEd->Exec( *FString::Printf(TEXT("OBJ SAVEPACKAGE PACKAGE=\"%s\" FILE=\"%s\""), PackageName, *SaveFileName) );
			}
		}
	}

	GFileManager->SetDefaultDirectory();

}

UPackage* WxGenericBrowser::GetTopPackage()
{
	TArray<UPackage*> Packages;
	LeftContainer->GetSelectedPackages(&Packages);

	UPackage* pkg = Packages.Num() ? Packages(0) : NULL;

	// Walk the package chain to the top to get the proper package.

	if( pkg && pkg->GetOuter() )
	{
		pkg = (UPackage*)pkg->GetOutermost();
	}

	return pkg;
}

UPackage* WxGenericBrowser::GetGroup()
{
	TArray<UPackage*> Packages;
	LeftContainer->GetSelectedPackages(&Packages);

	UPackage* grp = Packages.Num() ? Packages(0) : NULL;

	// If the selected package has an outer, it must be a group.

	if( grp && grp->GetOuter() )
	{
		return grp;
	}

	// If we get here, the user has a top level package selected, so there is no valid group to pass back.

	return NULL;
}

/**
 * Synchronizes the tree control and the thumbnail viewport to
 * an object.
 *
 * @param	InObject	The object to sync to
 */

void WxGenericBrowser::SyncToObject( UObject* InObject )
{
	// Change the current type to "All" so that the object is guaranteed to show up.

	LeftContainer->ResourceTypeCombo->SetSelection( 0 );
	LeftContainer->OnResourceTypeSelChanged( wxCommandEvent() );

	// Sync the windows

	LeftContainer->SyncToObject( InObject );
	RightContainer->SyncToObject( InObject );
}

void WxGenericBrowser::OnFileNew( wxCommandEvent& In )
{
	UObject* pkg = GetTopPackage();
	UObject* grp = GetGroup();

	WxDlgNewGeneric dlg;
	dlg.ShowModal( pkg ? pkg->GetName() : TEXT("MyPackage"), grp ? grp->GetName() : TEXT("") );

	Update();
}

void WxGenericBrowser::OnContextObjectNew(wxCommandEvent& In)
{
	INT FactoryClassIndex = In.GetId() - ID_NEWOBJ_START;
	check( FactoryClassIndex >= 0 && FactoryClassIndex < FactoryNewClasses.Num() );

	UClass* FactoryClass = FactoryNewClasses(FactoryClassIndex);
	check( FactoryClass->IsChildOf(UFactory::StaticClass()) );

	TArray<UPackage*> Packages;
	LeftContainer->GetSelectedPackages(&Packages);

	WxDlgNewGeneric dlg;
	UPackage* pkg = GetTopPackage();
	UPackage* grp = GetGroup();
	dlg.ShowModal( pkg ? pkg->GetName() : TEXT("MyPackage"), grp ? grp->GetName() : TEXT(""), FactoryClass );

	Update();
}

void WxGenericBrowser::OnObjectEditor( wxCommandEvent& In )
{
	LeftContainer->CurrentResourceType->ShowObjectEditor();
}

void WxGenericBrowser::OnObjectCustomCommand( wxCommandEvent& In )
{
	LeftContainer->CurrentResourceType->InvokeCustomCommand( In.m_id - IDMN_ObjectContext_Custom_START );
}

void WxGenericBrowser::OnFileImport( wxCommandEvent& In )
{
	FString FileTypes, AllExtensions;
	TArray<UFactory*> Factories;			// All the factories we can possibly use

	// Get the list of file filters from the factories

	for( TObjectIterator<UClass> It ; It ; ++It )
	{
		if( It->IsChildOf(UFactory::StaticClass()) && !(It->ClassFlags & CLASS_Abstract) )
		{
			UFactory* Factory = ConstructObject<UFactory>( *It );
			if( Factory->bEditorImport && !Factory->bCreateNew )
			{
				Factory->bOKToAll = 0;
				Factories.AddItem( Factory );
				for( INT x = 0 ; x < Factory->Formats.Num() ; ++x )
				{
					FString Wk = Factory->Formats(x);

					TArray<FString> Components;
					Wk.ParseIntoArray( TEXT(";"), &Components );

					for( INT c = 0 ; c < Components.Num() ; c += 2 )
					{
						FString Ext = Components(c),
							Desc = Components(c+1),
							Line = FString::Printf( TEXT("%s (*.%s)|*.%s"), *Desc, *Ext, *Ext );

						if(AllExtensions.Len())
							AllExtensions += TEXT(";");
						AllExtensions += FString::Printf(TEXT("*.%s"), *Ext);

						if(FileTypes.Len())
							FileTypes += TEXT("|");
						FileTypes += Line;
					}
				}
			}
		}
	}

	FileTypes = FString::Printf(TEXT("All Files (%s)|%s|%s"),*AllExtensions,*AllExtensions,*FileTypes);

	// Prompt the user for the filenames

	wxFileDialog OpenFileDialog( this, 
		TEXT("Import"),
		*LastImportPath,
		TEXT(""),
		*FileTypes,
		wxOPEN | wxFILE_MUST_EXIST | wxHIDE_READONLY | wxMULTIPLE,
		wxDefaultPosition);

	if( OpenFileDialog.ShowModal() == wxID_OK )
	{
		wxArrayString OpenFilePaths;
		OpenFileDialog.GetPaths( OpenFilePaths );

		UObject* pkg = GetTopPackage();
		UObject* grp = GetGroup();
		FString Package = pkg ? pkg->GetName() : TEXT("MyPackage");
		FString Group = grp ? grp->GetName() : TEXT("");

		// For each filename, open up the import dialog and get required user input.

		for( INT FileIndex = 0 ; FileIndex < static_cast<INT>( OpenFilePaths.Count() ) ; FileIndex++ )
		{
			FFilename filename = OpenFilePaths[FileIndex];
			LastImportPath = filename.GetPath();

			UFactory* factory = NULL;

			for( INT f = 0 ; f < Factories.Num() ; ++f )
			{
				for(UINT FormatIndex = 0;FormatIndex < (UINT)Factories(f)->Formats.Num();FormatIndex++)
				{
					if( Factories(f)->Formats(FormatIndex).Left(filename.GetExtension().Len()) == filename.GetExtension() )
					{
						factory = Factories(f);
						goto FoundFactory;
					}
				}
			}

			FoundFactory:
			if( factory )
			{
				WxDlgImportGeneric dlg;
				dlg.ShowModal( filename, Package, Group, factory->SupportedClass, factory );
				Package = dlg.Package;
				Group = dlg.Group;
			}
		}
	}

	// Clean up

	for( INT f = 0 ; f < Factories.Num() ; ++f )
		delete Factories(f);
	Factories.Empty();

	Update();

}

void WxGenericBrowser::OnCopyReference(wxCommandEvent& In)
{
	TArray<UObject*> SelectedObjects;
	FString Ref;

	for( INT c = 0 ; c < LeftContainer->CurrentResourceType->SupportInfo.Num() ; ++c )
		for(TSelectedObjectIterator It(LeftContainer->CurrentResourceType->SupportInfo(c).Class);It;++It)
		{
			if( Ref.Len() )
				Ref += TEXT("\n");
			Ref += It->GetPathName();
		}

	appClipboardCopy( *Ref );
}

void WxGenericBrowser::OnObjectProperties(wxCommandEvent& In)
{
	TArray<UObject*>	SelectedObjects;
	for( INT c = 0 ; c < LeftContainer->CurrentResourceType->SupportInfo.Num() ; ++c )
		for(TSelectedObjectIterator It(LeftContainer->CurrentResourceType->SupportInfo(c).Class);It;++It)
			SelectedObjects.AddItem(*It);

	if(!GApp->ObjectPropertyWindow)
	{
		GApp->ObjectPropertyWindow = new WxPropertyWindowFrame( GApp->EditorFrame, -1, 1, GUnrealEd );
		GApp->ObjectPropertyWindow->SetSize( 64,64, 350,600 );
	}
	GApp->ObjectPropertyWindow->PropertyWindow->SetObjectArray( &SelectedObjects, 0,1,1 );
	GApp->ObjectPropertyWindow->Show();
}

void WxGenericBrowser::OnObjectDuplicate(wxCommandEvent& In)
{
	RightContainer->Viewport->Invalidate();

	TArray<UObject*> SelectedObjects;

	for( INT c = 0 ; c < LeftContainer->CurrentResourceType->SupportInfo.Num() ; ++c )
	{
		for(TSelectedObjectIterator It(LeftContainer->CurrentResourceType->SupportInfo(c).Class);It;++It)
		{
			SelectedObjects.AddItem(*It);
		}
	}

	for(UINT ObjectIndex = 0;ObjectIndex < (UINT)SelectedObjects.Num();ObjectIndex++)
	{
		UObject*			Object = SelectedObjects(ObjectIndex);
		UOptionsDupObject*	Proxy = (UOptionsDupObject*)*GApp->EditorFrame->OptionProxies->Find( OPTIONS_DUPOBJECT );
		WxDlgGenericOptions	DupDialog;
		FString Reason;

		Proxy->Package = Object->GetOuter()->GetPathName();
		Proxy->Name = FString::Printf(TEXT("%s_Dup"),Object->GetName());

		if( DupDialog.ShowModal( Proxy, TEXT("Duplicate Object") ) == wxID_OK )
		{
			if(!Proxy->Package.Len() || !Proxy->Name.Len())
			{
				appMsgf(0,TEXT("Invalid input."));
			}
			else if( !FIsValidObjectName( *Proxy->Name, Reason ) || !FIsUniqueObjectName( *Proxy->Name, Reason ) )
			{
				appMsgf( 0, *Reason );
			}
			else
			{
				// HACK: Make sure the Outers of parts of PhysicsAssets are correct before trying to duplicate it.
				UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(Object);
				if(PhysicsAsset)
				{
					PhysicsAsset->FixOuters();
				}

				UObject* DupObject = GEditor->StaticDuplicateObject(Object,Object,UObject::CreatePackage(NULL,*Proxy->Package),*Proxy->Name);
				if(DupObject)
				{
					DupObject->MarkPackageDirty();
				}

				GSelectionTools.Select(Object,0);
			}
		}
	}

	GCallback->Send( CALLBACK_RefreshEditor, ERefreshEditor_GenericBrowser );
}

void WxGenericBrowser::OnObjectRename(wxCommandEvent& In)
{
	RightContainer->Viewport->Invalidate();

	WxDlgRename RenameDialog;

	for( INT c = 0 ; c < LeftContainer->CurrentResourceType->SupportInfo.Num() ; ++c )
	{
		for(TSelectedObjectIterator It(LeftContainer->CurrentResourceType->SupportInfo(c).Class);It;++It)
		{
			FString	Group, Package;

			if(!Cast<UPackage>(It->GetOuter()->GetOuter()))
			{
				Group = TEXT("");
				Package = It->GetOuter()->GetName();
			}
			else
			{			
				Group = It->GetOuter()->GetName();
				Package = It->GetOuter()->GetOuter()->GetName();
			}

			if( RenameDialog.ShowModal(Package, Group, It->GetName()) == wxID_OK )
			{
				if(!RenameDialog.NewPackage.Len() || !RenameDialog.NewName.Len())
					appMsgf(0,TEXT("Invalid input."));
				else
				{
					// Look for the new name and see if it already exists.  If so, don't allow the rename.

					FString Pkg;
					if( RenameDialog.NewGroup.Len() > 0 )
					{
						Pkg = FString::Printf(TEXT("%s.%s.%s"),*RenameDialog.NewPackage,*RenameDialog.NewGroup,*RenameDialog.NewName);
					}
					else
					{
						Pkg = FString::Printf(TEXT("%s.%s"),*RenameDialog.NewPackage,*RenameDialog.NewName);
					}

					if( UObject::StaticFindObject(UObject::StaticClass(),NULL,*Pkg) )
					{
						appMsgf(0,TEXT("Object already exists with this name."));
					}
					else
					{
						UPackage* NewPackage = UObject::CreatePackage(NULL,*(RenameDialog.NewGroup.Len() ? RenameDialog.NewPackage + TEXT(".") + RenameDialog.NewGroup : RenameDialog.NewPackage));
						GEditor->RenameObject(*It, NewPackage, *RenameDialog.NewName);
					}
				}
			}
		}
	}

	GCallback->Send( CALLBACK_RefreshEditor, ERefreshEditor_GenericBrowser );
}

void WxGenericBrowser::OnObjectDelete(wxCommandEvent& In)
{
	RightContainer->Viewport->Invalidate();

	for( INT c = 0 ; c < LeftContainer->CurrentResourceType->SupportInfo.Num() ; ++c )
	{
		for(TSelectedObjectIterator It(LeftContainer->CurrentResourceType->SupportInfo(c).Class);It;++It)
		{
			GEditor->DeleteObject(*It);
		}
	}

	GEditor->Exec( TEXT("obj garbage") );

	LeftContainer->Update();
}

void WxGenericBrowser::OnObjectExport(wxCommandEvent& In)
{
	// Create an array of all available exporters. TODO: Could we just do this once?
	TArray<UExporter*> Exporters;
	for( TObjectIterator<UClass> It ; It ; ++It )
	{
		if( It->IsChildOf(UExporter::StaticClass()) && !(It->ClassFlags & CLASS_Abstract) )
		{
			UExporter* Exporter = ConstructObject<UExporter>( *It );
			Exporters.AddItem( Exporter );		
		}
	}

	// Iterate over each object you wish to export
	for( INT c = 0 ; c < LeftContainer->CurrentResourceType->SupportInfo.Num() ; ++c )
	{
		for(TSelectedObjectIterator It(LeftContainer->CurrentResourceType->SupportInfo(c).Class);It;++It)
		{		
			UObject* ExportObject = *It;

			// Find all the exporters that can export this type of object and construct export box.
			FString FileTypes, AllExtensions;

			for(INT i=0; i<Exporters.Num(); i++)
			{
				UExporter* E = Exporters(i);
				if( E->SupportedClass && ExportObject->IsA(E->SupportedClass) )
				{
					check( E->FormatExtension.Num() == E->FormatDescription.Num() );

					for(INT j=0; j<E->FormatExtension.Num(); j++)
					{
						FString Line = FString::Printf( TEXT("%s (*.%s)|*.%s"), *(E->FormatDescription(j)), *(E->FormatExtension(j)), *(E->FormatExtension(j)) );

						if(FileTypes.Len())
							FileTypes += TEXT("|");
						FileTypes += Line;

						if(AllExtensions.Len())
							AllExtensions += TEXT(";");
						AllExtensions += FString::Printf( TEXT("*.%s"), *(E->FormatExtension(j)) );
					}
				}
			}

			FileTypes = FString::Printf(TEXT("All Files (%s)|%s|%s"), *AllExtensions, *AllExtensions, *FileTypes);

			// Open dialog so user can chose save filename.
			wxFileDialog SaveFileDialog( this, 
				*FString::Printf( TEXT("Save: %s"), ExportObject->GetName() ),
				*LastExportPath,
				ExportObject->GetName(),
				*FileTypes,
				wxSAVE,
				wxDefaultPosition);

			FFilename SaveFileName;

			if( SaveFileDialog.ShowModal() == wxID_OK )
			{
				SaveFileName = FFilename( SaveFileDialog.GetPath() );
				LastExportPath = SaveFileName.GetPath();

				if( GFileManager->IsReadOnly( *SaveFileName ) )
				{
					appMsgf( 0, TEXT("Couldn't write to file. Maybe file is read-only?") );
				}
				else
				{
					// We have a writeable file. Now go through that list of exporters again and find the right exporter and use it.
					UExporter* UseExporter = NULL;

					for(INT i=0; i<Exporters.Num() && !UseExporter; i++)
					{
						UExporter* E = Exporters(i);
						if( E->SupportedClass && ExportObject->IsA(E->SupportedClass) )
						{
							check( E->FormatExtension.Num() == E->FormatDescription.Num() );

							for( INT j=0; j<E->FormatExtension.Num() && !UseExporter; j++ )
							{
								if(	appStricmp( *(E->FormatExtension(j)), *SaveFileName.GetExtension() )==0 || appStricmp( *(E->FormatExtension(j)), TEXT("*") )==0 )
									UseExporter = E;
							}
						}
					}

					if(UseExporter)
					{
						WxDlgExportGeneric dlg;
						dlg.ShowModal( SaveFileName, ExportObject, UseExporter );
					}
				}
			}
		}
	}

	// Clean up list of exporters
	//for( INT i = 0 ; i < Exporters.Num() ; i++ )
	//	delete Exporters(i);
	Exporters.Empty();
}

/*-----------------------------------------------------------------------------
	WxGBLeftContainer.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxGBLeftContainer, wxPanel )
	EVT_TREE_ITEM_EXPANDING( ID_PKG_GRP_TREE, WxGBLeftContainer::OnTreeExpanding )
	EVT_TREE_ITEM_COLLAPSED( ID_PKG_GRP_TREE, WxGBLeftContainer::OnTreeCollapsed )
	EVT_TREE_SEL_CHANGED( ID_PKG_GRP_TREE, WxGBLeftContainer::OnTreeSelChanged )
	EVT_COMBOBOX( ID_RESOURCE_TYPE, WxGBLeftContainer::OnResourceTypeSelChanged )
	EVT_SIZE( WxGBLeftContainer::OnSize )
END_EVENT_TABLE()

WxGBLeftContainer::WxGBLeftContainer( wxWindow* InParent )
	: wxPanel( InParent, -1 )
{
	// Create the list of available resource types

	for( TObjectIterator<UClass> ItC ; ItC ; ++ItC )
	{
		if( *ItC != UGenericBrowserType_All::StaticClass() && ItC->IsChildOf(UGenericBrowserType::StaticClass()) && !(ItC->ClassFlags&CLASS_Abstract) )
		{
			UGenericBrowserType* gbt = ConstructObject<UGenericBrowserType>( *ItC );
			if( gbt )
			{
				gbt->Init();
				ResourceTypes.AddItem( gbt );
			}
		}
	}

	// Create the "all" type last as it needs all the others to be created before it

	UGenericBrowserType* gbt = ConstructObject<UGenericBrowserType>( UGenericBrowserType_All::StaticClass() );
	gbt->Init();
	ResourceTypes.AddItem( gbt );

	wxString dummy[1];
	ResourceTypeCombo = new wxComboBox( this, ID_RESOURCE_TYPE, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, dummy, wxCB_READONLY | wxCB_SORT );
	for( INT x = 0 ; x < ResourceTypes.Num() ; ++x )
	{
		ResourceTypeCombo->Append( *ResourceTypes(x)->GetBrowserTypeDescription(), ResourceTypes(x) );
	}

	TreeCtrl = new wxTreeCtrl( this, ID_PKG_GRP_TREE, wxDefaultPosition, wxDefaultSize, wxTR_HAS_BUTTONS | wxTR_MULTIPLE | wxTR_LINES_AT_ROOT );

	ResourceTypeCombo->SetSelection( 0 );
	CurrentResourceType = ResourceTypes( ResourceTypes.Num() - 1 );
}

WxGBLeftContainer::~WxGBLeftContainer()
{
}

void WxGBLeftContainer::Update()
{
	UpdateTree();
}

void WxGBLeftContainer::UpdateTree()
{
	// Remember the current selections

	TArray<UPackage*> SelectedPackages;
	GetSelectedPackages( &SelectedPackages );

	// Init

	TreeCtrl->DeleteAllItems();
	PackageMap.Empty();

	// Generate a list of packages
	//
	// Only show packages that match the current resource type filter.

	TArray<UObject*> List;

	for( TObjectIterator<UObject> It ; It ; ++It )
	{
		if( CurrentResourceType->Supports( *It ) )
		{
			if( Cast<UPackage>(It->GetOutermost()) )
				List.AddUniqueItem( Cast<UPackage>(It->GetOutermost()) );
		}
	}

	// Populate the tree/package map with outermost packages

	TreeCtrl->AddRoot( TEXT("Packages") );

	for( INT x = 0 ; x < List.Num() ; ++x )
	{
		INT Id = TreeCtrl->AppendItem( TreeCtrl->GetRootItem(), List(x)->GetName(), GBTCI_FolderClosed, -1, new WxTreeLeafData( List(x) ) );
		PackageMap.Set( Cast<UPackage>(List(x)), Id );
	}

	// Look for groups and add them into the tree/package map.

	for( TObjectIterator<UPackage> It ; It ; ++It )
	{
		// If an object is a UPackage and it has a valid outer, it must be a group.

		if( It->GetOuter() )
		{
			wxTreeItemId* parentid = PackageMap.Find( Cast<UPackage>(It->GetOuter()) );

			if( parentid )
			{
				INT Id = TreeCtrl->AppendItem( *parentid, It->GetName(), GBTCI_FolderClosed, -1, new WxTreeLeafData( *It ) );
				PackageMap.Set( *It, Id );
			}
		}
	}

	// Sort the root of the tree

	for( TMap<UPackage*,wxTreeItemId>::TIterator It(PackageMap) ; It ; ++It )
		TreeCtrl->SortChildren( It.Value() );

	TreeCtrl->SortChildren( TreeCtrl->GetRootItem() );

	// Expand the root item

	TreeCtrl->Expand( TreeCtrl->GetRootItem() );

	// Find the packages that were previously selected and make sure they stay that way

	TreeCtrl->UnselectAll();
	for( INT x = 0 ; x < SelectedPackages.Num() ; ++x )
	{
		UPackage* package = (UPackage*)SelectedPackages(x);
		wxTreeItemId* tid = PackageMap.Find( package );
		if( tid )
		{
			TreeCtrl->SelectItem( *tid );
			TreeCtrl->EnsureVisible( *tid );
		}
	}
}

/**
 * Opens the tree control to the package/group belonging to the specified object.
 *
 * @param	InObject	The object to sync to
 */
 
void WxGBLeftContainer::SyncToObject( UObject* InObject )
{
	UPackage* pkg = (UPackage*)InObject->GetOuter();
	wxTreeItemId* tid = PackageMap.Find( pkg );

	if( tid )
	{
		TreeCtrl->SelectItem( *tid );
		TreeCtrl->EnsureVisible( *tid );
	}
}

// Fills InPackages with a list of selected packages/groups

void WxGBLeftContainer::GetSelectedPackages( TArray<UPackage*>* InPackages )
{
	InPackages->Empty();

	wxArrayTreeItemIds tids;
	TreeCtrl->GetSelections( tids );

	for( INT x = 0 ; x < static_cast<INT>( tids.GetCount() ); ++ x )
		if( TreeCtrl->IsSelected( tids[x] ) && TreeCtrl->GetItemData(tids[x]) )
			InPackages->AddItem( (UPackage*)(((WxTreeLeafData*)TreeCtrl->GetItemData(tids[x]))->Data) );
}

void WxGBLeftContainer::OnTreeExpanding( wxTreeEvent& In )
{
	TreeCtrl->SetItemImage( In.GetItem(), GBTCI_FolderOpen );
}

void WxGBLeftContainer::OnTreeCollapsed( wxTreeEvent& In )
{
	TreeCtrl->SetItemImage( In.GetItem(), GBTCI_FolderClosed );
}

void WxGBLeftContainer::OnTreeSelChanged( wxTreeEvent& In )
{
	GenericBrowser->RightContainer->Update();
}

void WxGBLeftContainer::OnResourceTypeSelChanged( wxCommandEvent& In )
{
	CurrentResourceType = (UGenericBrowserType*)ResourceTypeCombo->GetClientData( ResourceTypeCombo->GetSelection() );

	GenericBrowser->Update();
}

void WxGBLeftContainer::OnSize( wxSizeEvent& In )
{
	wxRect rc = GetClientRect();
	wxRect rcT = ResourceTypeCombo->GetClientRect();

	ResourceTypeCombo->SetSize( 0,0, rc.GetWidth(), rcT.GetHeight() );
	TreeCtrl->SetSize( 0, rcT.GetHeight(), rc.GetWidth(), rc.GetHeight() - rcT.GetHeight() );

}

void WxGBLeftContainer::Serialize(FArchive& Ar)
{
	Ar << ResourceTypes << CurrentResourceType;
}

/*-----------------------------------------------------------------------------
	WxGBRightContainer.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxGBRightContainer, wxPanel )
	EVT_SIZE( WxGBRightContainer::OnSize )
	EVT_LIST_ITEM_SELECTED( ID_LIST_VIEW, WxGBRightContainer::OnSelectionChanged_Select )
	EVT_LIST_ITEM_DESELECTED( ID_LIST_VIEW, WxGBRightContainer::OnSelectionChanged_Deselect )
	EVT_LIST_ITEM_RIGHT_CLICK( ID_LIST_VIEW, WxGBRightContainer::OnListViewRightClick )
	EVT_LIST_ITEM_ACTIVATED( ID_LIST_VIEW, WxGBRightContainer::OnListItemActivated )
	EVT_MENU_RANGE( IDM_VIEW_START, IDM_VIEW_END, WxGBRightContainer::OnViewMode )
	EVT_UPDATE_UI_RANGE( IDM_VIEW_START, IDM_VIEW_END, WxGBRightContainer::UI_ViewMode )
	EVT_MENU_RANGE( ID_PRIMTYPE_START, ID_PRIMTYPE_END, WxGBRightContainer::OnPrimType )
	EVT_UPDATE_UI_RANGE( ID_PRIMTYPE_START, ID_PRIMTYPE_END, WxGBRightContainer::UI_PrimType )
	EVT_COMBOBOX( ID_ZOOM_FACTOR, WxGBRightContainer::OnZoomFactorSelChange )
	EVT_TEXT( ID_NAME_FILTER, WxGBRightContainer::OnNameFilterChanged )
	EVT_SCROLL( WxGBRightContainer::OnScroll )
END_EVENT_TABLE()

WxGBRightContainer::WxGBRightContainer( wxWindow* InParent )
	: wxPanel( InParent, -1 )
{
	ToolBar = new WxGBViewToolBar( (wxWindow*)this, -1 );

	SplitterWindowV = new wxSplitterWindow( this, -1, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE | wxSP_BORDER );
	ViewportHolder = new EViewportHolder( (wxWindow*)SplitterWindowV, -1, 1 );
	ListView = new wxListView( SplitterWindowV, ID_LIST_VIEW, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_ALIGN_LEFT | wxLC_SORT_ASCENDING );

	SplitterWindowV->SplitVertically(
		ListView,
		ViewportHolder,
		STD_SPLITTER_SZ );

	ListView->InsertColumn( 0, TEXT("Object"), wxLIST_FORMAT_LEFT, 150 );
	ListView->InsertColumn( 1, TEXT("Info"), wxLIST_FORMAT_LEFT, 1000 );

	Viewport = GEngine->Client->CreateWindowChildViewport( this, (HWND)ViewportHolder->GetHandle() );
	Viewport->CaptureJoystickInput(false);

	ViewportHolder->SetViewport( Viewport );

	SetViewMode( RVM_Thumbnail );

	ThumbnailPrimType = TPT_Sphere;
}

WxGBRightContainer::~WxGBRightContainer()
{
	if(Viewport)
	{
		GEngine->Client->CloseViewport(Viewport);
		Viewport = NULL;
	}
}

int wxCALLBACK ObjectCompareFunction( long item1, long item2, long sortData )
{
	return appStrcmp( ((UObject*)item1)->GetName(), ((UObject*)item2)->GetName() );
}

// Fills the list view with items relating to the selected packages on the left side.

void WxGBRightContainer::UpdateList()
{
	ListView->Freeze();
	ListView->DeleteAllItems();

	// See what is selected on the left side.

	TArray<UPackage*> SelectedPackages;
	GenericBrowser->LeftContainer->GetSelectedPackages( &SelectedPackages );

	TArray<UObject*> Objects;
	GetObjectsInPackages( &SelectedPackages, &Objects );

	for( INT x = 0 ; x < Objects.Num() ; ++x )
	{
		// WDM : should grab a bitmap to use for each different kind of resource.  For now, they'll all look the same.
		INT item = ListView->InsertItem( 0, Objects(x)->GetName(), GBTCI_Resource );

		ListView->SetItem( item, 1, *Objects(x)->GetDesc() );
		ListView->SetItemData( item, (long)Objects(x) );
	}

	ListView->SortItems( ObjectCompareFunction, 0 );
	ListView->Thaw();
	Viewport->Invalidate();
}

void WxGBRightContainer::Update()
{
	UpdateUI();
	UpdateList();
}

void WxGBRightContainer::UpdateUI()
{
	WxBrowserData* bd = GenericBrowser->FindBrowserData( GenericBrowser->LeftContainer->CurrentResourceType->SupportInfo(0).Class );
	//ToolBar->NameFilterTC->SetValue( *bd->NameFilter );

	FString Wk;
	if( !bd->FixedSz )
		Wk = FString::Printf( TEXT("%d%%"), (INT)(bd->ZoomFactor*100) );
	else
		Wk = FString::Printf( TEXT("%dx%d"), bd->FixedSz, bd->FixedSz );

	ToolBar->ViewCB->Select( ToolBar->ViewCB->FindString( *Wk ) );
}

void WxGBRightContainer::OnSize( wxSizeEvent& In )
{
	wxRect rc = GetClientRect();
	wxRect rcT = ToolBar->GetClientRect();

	ToolBar->SetSize( 0, 0, rc.GetWidth(), rcT.GetHeight() );
	SplitterWindowV->SetSize( 0, rcT.GetHeight(), rc.GetWidth(), rc.GetHeight() - rcT.GetHeight() );

}

void WxGBRightContainer::OnViewMode( wxCommandEvent& In )
{
	switch( In.GetId() )
	{
		case ID_VIEW_LIST:
			SetViewMode( RVM_List );
			break;

		case ID_VIEW_PREVIEW:
			SetViewMode( RVM_Preview );
			break;

		case ID_VIEW_THUMBNAIL:
			SetViewMode( RVM_Thumbnail );
			break;
	}

	OnSize( wxSizeEvent() );
	Update();
}

void WxGBRightContainer::OnPrimType( wxCommandEvent& In )
{
	switch( In.GetId() )
	{
		case ID_PRIMTYPE_SPHERE:	ThumbnailPrimType = TPT_Sphere;		break;
		case ID_PRIMTYPE_CUBE:		ThumbnailPrimType = TPT_Cube;		break;
		case ID_PRIMTYPE_CYLINDER:	ThumbnailPrimType = TPT_Cylinder;	break;
		case ID_PRIMTYPE_PLANE:		ThumbnailPrimType = TPT_Plane;		break;
	}

	//OnSize( wxSizeEvent() );
	Update();
}

void WxGBRightContainer::OnSelectionChanged_Select( wxListEvent& In )
{
	UObject* obj = (UObject*)In.GetData();
	GSelectionTools.Select( obj, 1 );

	Viewport->Invalidate();
}

void WxGBRightContainer::OnSelectionChanged_Deselect( wxListEvent& In )
{
	UObject* obj = (UObject*)In.GetData();
	GSelectionTools.Select( obj, 0 );

	Viewport->Invalidate();
}

void WxGBRightContainer::OnListViewRightClick( wxListEvent& In )
{
	UObject* obj = (UObject*)( ListView->GetItemData( ListView->GetFocusedItem() ) );

	if( obj )
	{
		GSelectionTools.SelectNone( obj->GetClass() );
		GSelectionTools.Select( obj );
		
		ShowContextMenu( obj );
	}
}

void WxGBRightContainer::OnListItemActivated( wxListEvent& In )
{
	UObject* obj = (UObject*)( ListView->GetItemData( ListView->GetFocusedItem() ) );

	if( obj )
	{
		GSelectionTools.SelectNone( obj->GetClass() );
		GSelectionTools.Select( obj );

		GenericBrowser->LeftContainer->CurrentResourceType->DoubleClick();
	}
}

void WxGBRightContainer::SetViewMode( ERightViewMode InViewMode )
{
	ViewMode = InViewMode;

	ListView->Show();
	ViewportHolder->Show();
	SplitterWindowV->SplitVertically( ListView, ViewportHolder, STD_SPLITTER_SZ );

	switch( ViewMode )
	{
		case RVM_List:
			SplitterWindowV->Unsplit( ViewportHolder );
			break;

		case RVM_Thumbnail:
			SplitterWindowV->Unsplit( ListView );
			break;
	}
}

void WxGBRightContainer::UI_ViewMode( wxUpdateUIEvent& In )
{
	switch( In.GetId() )
	{
		case ID_VIEW_LIST:		In.Check( ViewMode == RVM_List );			break;
		case ID_VIEW_PREVIEW:	In.Check( ViewMode == RVM_Preview );		break;
		case ID_VIEW_THUMBNAIL:	In.Check( ViewMode == RVM_Thumbnail );		break;
	}
}

void WxGBRightContainer::UI_PrimType( wxUpdateUIEvent& In )
{
	WxBrowserData* bd = GenericBrowser->FindBrowserData( GenericBrowser->LeftContainer->CurrentResourceType->SupportInfo(0).Class );

	switch( In.GetId() )
	{
		case ID_PRIMTYPE_SPHERE:	In.Check( ThumbnailPrimType == TPT_Sphere );	break;
		case ID_PRIMTYPE_CUBE:		In.Check( ThumbnailPrimType == TPT_Cube );		break;
		case ID_PRIMTYPE_CYLINDER:	In.Check( ThumbnailPrimType == TPT_Cylinder );	break;
		case ID_PRIMTYPE_PLANE:		In.Check( ThumbnailPrimType == TPT_Plane );		break;
	}
}

void WxGBRightContainer::InputKey(FChildViewport* Viewport,FName Key,EInputEvent Event,FLOAT)
{
	if((Key == KEY_LeftMouseButton || Key == KEY_RightMouseButton) && (Event == IE_Pressed || Event == IE_DoubleClick))
	{
		INT			HitX = Viewport->GetMouseX(),
					HitY = Viewport->GetMouseY();
		HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);
		UObject*	HitObject = NULL;

		if(HitResult)
		{
			if(HitResult->IsA(TEXT("HObject")))
				HitObject = ((HObject*)HitResult)->Object;
		}

		if( HitObject )
		{
			if( !Viewport->KeyState(KEY_LeftControl) && !Viewport->KeyState(KEY_RightControl) )
				for( INT c = 0 ; c < GenericBrowser->LeftContainer->CurrentResourceType->SupportInfo.Num() ; ++c )
					GSelectionTools.SelectNone( GenericBrowser->LeftContainer->CurrentResourceType->SupportInfo(c).Class );

			if( Key == KEY_RightMouseButton )
			{
				GSelectionTools.Select( HitObject );
			}
			else
			{
				GSelectionTools.ToggleSelect( HitObject );

				// If selecting a material with the left mouse button, apply it to all selected BSP.

				if( Key == KEY_LeftMouseButton && GSelectionTools.IsSelected( HitObject ) && Cast<UMaterialInstance>( HitObject) )
				{
					GUnrealEd->Exec( TEXT("POLY SETMATERIAL") );
				}
			}
		}

		Viewport->InvalidateDisplay();

		if( HitObject )
		{
			if( Key == KEY_LeftMouseButton && Event == IE_DoubleClick )
			{
				GenericBrowser->LeftContainer->CurrentResourceType->DoubleClick();
			}
			else if( Key == KEY_RightMouseButton && Event == IE_Pressed )
			{
				ShowContextMenu( HitObject );
			}
		}
		else
		{
			// Right clicking on background - show 'New' menu.
			if( Key == KEY_RightMouseButton && Event == IE_Pressed )
			{
				ShowNewObjectMenu();
			}
		}
	}

	// Find the first selected object and start to drag it's name

	/*
	UObject* obj = GSelectionTools.GetTop<UObject>();

	if( obj )
	{
		wxTextDataObject textData( obj->GetName() );
		wxDropSource source(textData, this,
						wxDROP_ICON(IDCUR_COPY),
						wxDROP_ICON(IDCUR_MOVE),
						wxDROP_ICON(IDCUR_NODROP));

		source.DoDragDrop( wxDrag_AllowMove );
	}
	*/
}

void WxGBRightContainer::Draw( FChildViewport* Viewport, FRenderInterface* RI )
{
	UBOOL	HitTesting = RI->IsHitTesting();

	RI->DrawTile
	(
		0,
		0,
		Viewport->GetSizeX(),
		Viewport->GetSizeY(),
		0.0f,
		0.0f,
		(FLOAT)Viewport->GetSizeX() / (FLOAT)GEditor->Bkgnd->SizeX,
		(FLOAT)Viewport->GetSizeY() / (FLOAT)GEditor->Bkgnd->SizeY,
		FLinearColor::White,
		GEditor->Bkgnd
	);

	TArray<UObject*> VisibleObjects;
	GetVisibleObjects( &VisibleObjects );
	INT ScrollBarPosition = ViewportHolder->GetScrollThumbPos();

	if(ViewMode == RVM_Preview || ViewMode == RVM_Thumbnail)
	{
		INT XPos, YPos, YHighest, dummy, FontHeight, SBTotalHeight;

		XPos = YPos = STD_TNAIL_PAD_HALF;
		YHighest = SBTotalHeight = 0;

		YPos -= ViewportHolder->GetScrollThumbPos();

		RI->StringSize( GEngine->SmallFont, dummy, FontHeight, TEXT("X") );

		FMemMark	Mark(GMem);

		for( INT x = 0 ; x < VisibleObjects.Num() ; ++x )
		{
			UObject* obj = VisibleObjects(x);

			// If we are trying to sync to a specific object and we find it, record the vertical position.

			if( SyncToNextDraw == obj )
			{
				ScrollBarPosition = YPos + ViewportHolder->GetScrollThumbPos();
			}
			
			WxBrowserData* bd = GenericBrowser->FindBrowserData( GenericBrowser->LeftContainer->CurrentResourceType->SupportInfo(0).Class );
			FThumbnailDesc td = obj->GetThumbnailDesc( RI, bd->ZoomFactor, bd->FixedSz );

			TArray<FString> Labels;
			obj->GetThumbnailLabels( &Labels );

			INT ThumbnailHeight = td.Height;
			GetThumbnailDimensions( td.Width, td.Height, &Labels, RI, &td.Width, &td.Height );

			// If this thumbnail is too large for the current line, move to the next line.

			if( XPos + td.Width > static_cast<INT>( Viewport->GetSizeX() ) )
			{
				YPos += YHighest + STD_TNAIL_PAD_HALF;
				SBTotalHeight += YHighest + STD_TNAIL_PAD_HALF;
				YHighest = 0;
				XPos = STD_TNAIL_PAD_HALF;
			}

			if(YPos + td.Height + STD_TNAIL_PAD >= 0 && YPos <= (INT)Viewport->GetSizeY())
			{
				if(HitTesting)
				{
					// If hit testing, draw a tile instead of the actual thumbnail to avoid rendering thumbnail scenes(which screws up hit detection).

					RI->SetHitProxy(new HObject(obj));
					RI->DrawTile(XPos,YPos,td.Width,td.Height,0.0f,0.0f,1.f,1.f,FLinearColor::White,GEditor->BkgndHi);
					RI->SetHitProxy(NULL);
				}
				else
				{
					// Draw a colored border around the resource

					RI->DrawTile
					(
						XPos-STD_TNAIL_HIGHLIGHT_EDGE,
						YPos-STD_TNAIL_HIGHLIGHT_EDGE,
						td.Width+(STD_TNAIL_HIGHLIGHT_EDGE*2),
						td.Height+(STD_TNAIL_HIGHLIGHT_EDGE*2),
						0.0f,0.0f,1.f,1.f,
						GenericBrowser->LeftContainer->CurrentResourceType->GetBorderColor( obj )
					);

					// If this is a selected resource, draw the highlight box behind it

					FColor BackColor(0,0,0);
					if( GSelectionTools.IsSelected( obj ) )
						BackColor = FColor(183,199,187);

					RI->DrawTile
					(
						XPos-STD_TNAIL_HIGHLIGHT_EDGE+2,
						YPos-STD_TNAIL_HIGHLIGHT_EDGE+2,
						td.Width+(STD_TNAIL_HIGHLIGHT_EDGE*2)-4,
						td.Height+(STD_TNAIL_HIGHLIGHT_EDGE*2)-4,
						0.0f,
						0.0f,
						1.f,
						1.f,
						BackColor,
						GEditor->BkgndHi
					);

					// Draw the thumbnail

					obj->DrawThumbnail( ThumbnailPrimType, XPos, YPos, Viewport, RI, bd->ZoomFactor, 0, 1, bd->FixedSz );
	
					// Draw the text labels

					for( INT line = 0 ; line < Labels.Num() ; ++line )
						RI->DrawString( XPos, YPos+ThumbnailHeight+(line*FontHeight), *Labels(line), GEngine->SmallFont, FLinearColor::White );
				}
			}

			// Keep track of the tallest thumbnail on this line

			if( td.Height > YHighest )
				YHighest = td.Height;

			XPos += td.Width + STD_TNAIL_PAD_HALF;
		}

		Mark.Pop();

		// Update the scrollbar in the viewport holder

		SBTotalHeight += YHighest + STD_TNAIL_PAD_HALF;
		ViewportHolder->UpdateScrollBar( ScrollBarPosition, SBTotalHeight );

		// If we are trying to sync to a specific object, redraw the viewport so it
		// will be visible.

		if( SyncToNextDraw )
		{
			SyncToNextDraw = NULL;
			Draw( Viewport, RI );
		}
	}
}

// Fills a list of UObject*'s that represent the currently selected objects

void WxGBRightContainer::GetVisibleObjects( TArray<UObject*>* InVisibleObjects )
{
	InVisibleObjects->Empty();

	if( ViewMode == RVM_Thumbnail )
	{
		TArray<UPackage*> Packages;
		GenericBrowser->LeftContainer->GetSelectedPackages( &Packages );
		GetObjectsInPackages( &Packages, InVisibleObjects );
	}
	else
	{
		for( INT Sel = ListView->GetFirstSelected() ; Sel != -1 ; Sel = ListView->GetNextSelected( Sel ) )
		{
			UObject* obj = (UObject*)ListView->GetItemData( Sel );
			InVisibleObjects->AddItem( obj );
		}
	}
}

// Loads up InObjects with the objects (of type InType) contained in any of the InPackages

int CDECL ObjNameCompare(const void *A, const void *B)
{
	return appStricmp((*(UObject **)A)->GetName(),(*(UObject **)B)->GetName());
}

void WxGBRightContainer::GetObjectsInPackages( TArray<UPackage*>* InPackages, TArray<UObject*>* InObjects )
{
	FMemMark Mark(GMem);
	enum {MAX=16384};
	UObject** List = new(GMem,MAX)UObject*;
	int ListIdx = 0;

	for( TObjectIterator<UObject> It ; It ; ++It )
	{
		if( It->GetOuter() && !It->IsA( UPackage::StaticClass() ) )
		{
			WxBrowserData* bd = GenericBrowser->FindBrowserData( GenericBrowser->LeftContainer->CurrentResourceType->SupportInfo(0).Class );
			FString A = It->GetName(),
				B = bd->NameFilter;
			A = A.Caps();
			B = B.Caps();
			if( appStrstr( *A, *B ) )
			{
				UPackage* PkgOuter = Cast<UPackage>( It->GetOuter() );
				UPackage* PkgOutermost = Cast<UPackage>( It->GetOutermost() );

				// Only include objects from the selected packages

				if( InPackages->FindItemIndex( PkgOuter ) != INDEX_NONE
						|| InPackages->FindItemIndex( PkgOutermost ) != INDEX_NONE )
				{
					if( GenericBrowser->LeftContainer->CurrentResourceType->Supports( *It ) )
						List[ListIdx++] = *It;
				}
			}
		}
	}

	// Sort the resulting list of objects

	appQsort( &List[0], ListIdx, sizeof(UObject*), ObjNameCompare );

	// Fill the results array

	for( INT x = 0 ; x < ListIdx ; ++x )
		InObjects->AddItem( List[x] );

	Mark.Pop();
}

void WxGBRightContainer::GetThumbnailDimensions( INT InWidth, INT InHeight, TArray<FString>* InLabels, FRenderInterface* InRI, INT* InXL, INT* InYL )
{
	*InXL = InWidth;
	*InYL = InHeight;

	INT W = 0,
		H = 0;

	for( INT x = 0 ; x < InLabels->Num() ; ++x )
	{
		InRI->StringSize( GEngine->SmallFont, W, H, *((*InLabels)(x)) );

		if( W > *InXL )
			*InXL = W;

		*InYL += H;
	}
}

// Bring up a context menu that allows creation of new objects using Factories. Alternative to choosing 'New' from the File menu.
void WxGBRightContainer::ShowNewObjectMenu()
{
	wxMenu* NewMenu = new wxMenu();

	for( INT i=0; i<GenericBrowser->FactoryNewClasses.Num(); i++ )
	{
		UFactory* DefFactory = CastChecked<UFactory>( GenericBrowser->FactoryNewClasses(i)->GetDefaultObject() );
		FString ItemText = FString::Printf( TEXT("New %s"), *DefFactory->Description );

		NewMenu->Append( ID_NEWOBJ_START + i, *ItemText, TEXT("") );
	}

	POINT pt;
	::GetCursorPos(&pt);

	GenericBrowser->PopupMenu(NewMenu,GenericBrowser->ScreenToClient(wxPoint(pt.x,pt.y)));
	delete NewMenu;
}

/**
 * Selects the resource specified by InObject and scrolls the viewport
 * so that resource is in view.
 *
 * @param	InObject	The resource to sync to
 */

void WxGBRightContainer::SyncToObject( UObject* InObject )
{
	GSelectionTools.SelectNone( InObject->GetClass() );
	GSelectionTools.Select( InObject );

    // Scroll the object into view
	
	SyncToNextDraw = InObject;

	Viewport->Invalidate();
}

void WxGBRightContainer::ShowContextMenu( UObject* InObject )
{
	POINT pt;
	::GetCursorPos(&pt);

	wxMenu*	ContextMenu = GenericBrowser->LeftContainer->CurrentResourceType->GetContextMenu( InObject );

	if(ContextMenu)
	{
		GenericBrowser->PopupMenu(ContextMenu,GenericBrowser->ScreenToClient(wxPoint(pt.x,pt.y)));
	}
	else
	{
		HMENU menu = LoadMenuIdX(hInstance, IDMENU_BrowserActor_Context),
			submenu = GetSubMenu( menu, 0 );
		TrackPopupMenu( submenu,
			TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
			pt.x, pt.y, 0,
			(HWND)(GApp->EditorFrame->DockingRoot->FindDockableWindow(DWT_ACTOR_BROWSER)->GetHandle()), NULL);
		DestroyMenu( menu );
	}
}

void WxGBRightContainer::OnZoomFactorSelChange( wxCommandEvent& In )
{
	WxBrowserData* bd = GenericBrowser->FindBrowserData( GenericBrowser->LeftContainer->CurrentResourceType->SupportInfo(0).Class );

	bd->FixedSz = 0;
	FString WkString = In.GetString();

	if( WkString.InStr( TEXT("%") ) > -1 )
	{
		bd->ZoomFactor = appAtof( *WkString );
		bd->ZoomFactor /= 100;
	}
	else
	{
		INT Pos = WkString.InStr( TEXT("x") );
		bd->FixedSz = appAtoi( *WkString.Left( Pos ) );
	}

	Viewport->Invalidate();
}

void WxGBRightContainer::OnNameFilterChanged( wxCommandEvent& In )
{
	WxBrowserData* bd = GenericBrowser->FindBrowserData( GenericBrowser->LeftContainer->CurrentResourceType->SupportInfo(0).Class );
	bd->NameFilter = In.GetString();
	Update();
}

void WxGBRightContainer::OnScroll( wxScrollEvent& InEvent )
{
	ViewportHolder->SetScrollThumbPos( InEvent.GetPosition() );
	Viewport->Invalidate();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
