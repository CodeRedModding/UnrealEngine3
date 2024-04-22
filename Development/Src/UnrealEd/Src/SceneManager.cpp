/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "SceneManager.h"
#include "..\..\Launch\Resources\resource.h"
#include "UnTerrain.h"
#include "EngineSequenceClasses.h"
#include "PropertyWindow.h"

/** IDs for Scene Manager window elements. */
#define SM_ID_PANEL 10009
#define SM_ID_SHOWBRUSHES 10004
#define SM_ID_TYPEFILTER 10005
#define SM_ID_NAMEFILTER 10006
#define SM_ID_GRID 10007
#define SM_ID_LEVELLIST 10008

/** The key code for CTRL + A. */
#define SM_CTRL_A	1

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxMBSceneManager
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Scene Manager menu bar.
 */
class WxMBSceneManager : public wxMenuBar
{
public:
	WxMBSceneManager()
	{
		// View menu
		wxMenu* ViewMenu = new wxMenu();
		ViewMenu->Append( IDM_RefreshBrowser, *LocalizeUnrealEd("RefreshWithHotkey"), TEXT("") );
		Append( ViewMenu, *LocalizeUnrealEd("View") );

		// Edit menu
		wxMenu* EditMenu = new wxMenu();
		EditMenu->Append( IDMN_SCENEMANAGER_DELETE, *LocalizeUnrealEd("Delete"), TEXT("") );
		Append( EditMenu, *LocalizeUnrealEd("Edit") );

		WxBrowser::AddDockingMenu( this );
	}
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxSceneManager
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define SM_PROPS_WIDTH		300
#define SM_LEVELLIST_WIDTH	150
#define SM_LEVELLIST_HEIGHT	96

/** Enums for scene manager columns. */
enum
{
	SM_ACTOR,
	SM_TAG,
	SM_TYPE,
	SM_LAYERS,
	SM_ATTACHMENT,
	SM_KISMET,
	SM_LOCATION,
	SM_PACKAGE,
	SM_LAYER,
	SM_NUM
};

/** Column headings. */
static const char *GGridHeadings[SM_NUM] =
{ 
	"Actor",
	"Tag",
	"Type",
	"Layers",
	"AttachmentBase",
	"Kismet",
	"Location",
	"Package",
	"Layer",
	//"Memory",
	//"Polygons",
};

/** Compare function implementations for column sorting. */
IMPLEMENT_COMPARE_POINTER( AActor, SM_ACTOR, \
{ \
	return appStricmp( *A->GetName(), *B->GetName() ); \
} )

IMPLEMENT_COMPARE_POINTER( AActor, SM_TAG, \
{ \
	UBOOL Comp = appStricmp( *(A->Tag.ToString()), *(B->Tag.ToString()) ); \
	return (Comp == 0) ? \
			appStricmp( *A->GetName(), *B->GetName() ) : \
			Comp; \
} )

IMPLEMENT_COMPARE_POINTER( AActor, SM_TYPE, \
{ \
	UBOOL Comp = appStricmp( *A->GetClass()->GetName(), *B->GetClass()->GetName() ); \
	return (Comp == 0)? \
			appStricmp( *A->GetName(), *B->GetName() ) : \
			Comp; \
} )

IMPLEMENT_COMPARE_POINTER( AActor, SM_LAYERS, \
{ \
	UBOOL Comp = appStricmp( *(A->Layer.ToString()), *(B->Layer.ToString()) ); \
	return (Comp == 0) ? \
			appStricmp( *A->GetName(), *B->GetName() ) : \
			Comp; \
} )

IMPLEMENT_COMPARE_POINTER( AActor, SM_ATTACHMENT, \
{ \
	FString _CompStr_A = (A->Base) ? A->Base->GetName() : TEXT(""); \
	FString _CompStr_B = (B->Base) ? B->Base->GetName() : TEXT(""); \
	UBOOL Comp = appStricmp( *_CompStr_A, *_CompStr_B ); \
	return (Comp == 0)? \
			appStricmp( *A->GetName(), *B->GetName() ) : \
			Comp; \
} )

IMPLEMENT_COMPARE_POINTER( AActor, SM_KISMET, \
{ \
	USequence* RootSeqA = GWorld->GetGameSequence( A->GetLevel() ); \
	USequence* RootSeqB = GWorld->GetGameSequence( B->GetLevel() ); \
	const TCHAR * _CompStr_A = ( RootSeqA && RootSeqA->ReferencesObject(A) ) ? TEXT("TRUE") : TEXT(""); \
	const TCHAR * _CompStr_B = ( RootSeqB && RootSeqB->ReferencesObject(B) ) ? TEXT("TRUE") : TEXT(""); \
	UBOOL Comp = appStricmp( _CompStr_A, _CompStr_B ); \
	return (Comp == 0)? \
			appStricmp( *A->GetName(), *B->GetName() ) : \
			Comp; \
} )

BEGIN_EVENT_TABLE( WxSceneManager, WxBrowser )
	// Menu events
	EVT_MENU( IDMN_SCENEMANAGER_DELETE, WxSceneManager::OnDelete )
	EVT_MENU( IDM_RefreshBrowser, WxSceneManager::OnRefresh )

	// Top Toolbar events
    EVT_CHECKBOX( IDMN_SCENEMANAGER_AUTOFOCUS, WxSceneManager::OnAutoFocus )
    EVT_BUTTON( IDMN_SCENEMANAGER_AUTOFOCUS, WxSceneManager::OnAutoFocus )
	EVT_CHECKBOX( IDMN_SCENEMANAGER_AUTOSELECT, WxSceneManager::OnAutoSelect )
    EVT_BUTTON( IDMN_SCENEMANAGER_FOCUS, WxSceneManager::OnFocus )
    EVT_BUTTON( IDMN_SCENEMANAGER_DELETE, WxSceneManager::OnDelete )
	EVT_BUTTON( IDM_RefreshBrowser, WxSceneManager::OnRefresh )

	// Filtering ToolBar events
    EVT_COMBOBOX( SM_ID_TYPEFILTER, WxSceneManager::OnTypeFilterSelected )
    EVT_CHECKBOX( SM_ID_SHOWBRUSHES, WxSceneManager::OnShowBrushes )
    EVT_TEXT_ENTER( SM_ID_NAMEFILTER, WxSceneManager::OnNameFilterChanged )	

	// Splitter window and grid events   
	EVT_GRID_CMD_RANGE_SELECT( SM_ID_GRID, WxSceneManager::OnGridRangeSelect )
    EVT_GRID_LABEL_LEFT_CLICK( WxSceneManager::OnLabelLeftClick )
	EVT_GRID_CELL_LEFT_DCLICK( WxSceneManager::OnGridCellDoubleClick )

	// Level List Events
	EVT_LISTBOX( SM_ID_LEVELLIST, WxSceneManager::OnLevelSelected )
END_EVENT_TABLE()


// PCF Begin
/*-----------------------------------------------------------------------------
	WxDelGrid
-----------------------------------------------------------------------------*/
BEGIN_EVENT_TABLE(WxDelGrid, wxGrid)
	EVT_CHAR(WxDelGrid::OnChar)
	EVT_GRID_COL_SIZE(WxDelGrid::OnGridColSize)
END_EVENT_TABLE()

WxDelGrid::WxDelGrid(wxWindow *parent, wxWindowID id, const wxPoint& pos,
							 const wxSize& size, long style) : 
					wxGrid(parent, id, pos, size, style),
					Parent(NULL) 
{
}

/** Keypress event handler. */
void WxDelGrid::OnChar(wxKeyEvent &Event)
{
	if ( Event.GetKeyCode() == WXK_DELETE )
	{
		if ( Parent )
		{
			Parent->DeleteSelectedActors();
		}
	}
	else if( Event.GetKeyCode() == SM_CTRL_A )
	{
		SelectAll();
	}
	else
	{
		Event.Skip();
	}
}

/** Column resize event handler */
void WxDelGrid::OnGridColSize(wxGridSizeEvent &Event)
{
	// if we aren't manually resizing the column
	if (m_dragLastPos == -1)
	{
		AutoSizeColumn(Event.GetRowOrCol(), false);
	}	
}

/** save grid position */
void WxDelGrid::SaveGridPosition()
{
	GetViewStart(&PrevSX,&PrevSY);
	PrevCX = GetGridCursorRow();
    PrevCY = GetGridCursorCol();
}

/** restore grid position */
void WxDelGrid::RestoreGridPosition()
{
	// restore grid position
	Scroll(PrevSX,PrevSY);
	if( PrevCX != -1 && PrevCY != -1 )
	{
		SetGridCursor(PrevCX,PrevCY);
	}
}

/*-----------------------------------------------------------------------------
	WxSelectAllListBox
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxSelectAllListBox, wxListBox)
	EVT_CHAR(WxSelectAllListBox::OnChar)
END_EVENT_TABLE()

/**
 * Default Constructor.
 *
 * @param	InParent		The owner of this list box. Needed for callbacks.
 * @param	InParentWindow	The actual window holding this list box.
 * @param	InID			The assigned ID for this widget.
 * @param	InPosition		The starting position of the list box.
 * @param	InSize			The starting size of this list box. 
 */
WxSelectAllListBox::WxSelectAllListBox( WxSceneManager* InParent, wxWindow* InParentWindow, wxWindowID InID, const wxPoint& InPosition, const wxSize& InSize )
:	wxListBox( InParentWindow, InID, InPosition, InSize, 0, NULL, wxLB_EXTENDED )
,	Parent(InParent)
{
}

/** 
 * Handles key press event, specifically Ctrl+A for Select All. 
 *
 * @param	Event	The key press that was fired. 
 */
void WxSelectAllListBox::OnChar( wxKeyEvent& Event )
{
	// Handle Ctrl + A. wxWidgets defines that combination to one. 
	if( Event.GetKeyCode() == SM_CTRL_A )
	{
		if( Parent )
		{
			Parent->SelectAllLevels();
		}
	}
}
// PCF End



WxSceneManager::WxSceneManager()
	:	bAreWindowsInitialized( FALSE )
	,	PropertyWindow(NULL)
	,	TypeFilter_Combo(NULL)
	,	TypeFilter_Selection(NULL)
	,	ShowBrushes_Check(NULL)
	,	Grid(NULL)
	,	SortColumn(0)
	,	bAutoFocus( FALSE )
	,	bAutoSelect( TRUE )
	,	bUpdateOnActivated( FALSE )
	,	Level_List(NULL)
{
	Panel = NULL;
	MenuBar = NULL;

	GCallbackEvent->Register(CALLBACK_RefreshEditor_SceneManager,this);
	GCallbackEvent->Register(CALLBACK_MapChange,this);
	GCallbackEvent->Register(CALLBACK_LevelRemovedFromWorld,this);
	GCallbackEvent->Register(CALLBACK_Undo,this);
}

/**
* Forwards the call to our base class to create the window relationship.
* Creates any internally used windows after that
*
* @param DockID the unique id to associate with this dockable window
* @param FriendlyName the friendly name to assign to this window
* @param Parent the parent of this window (should be a Notebook)
*/
void WxSceneManager::Create(INT DockID,const TCHAR* FriendlyName,wxWindow* Parent)
{
	WxBrowser::Create(DockID,FriendlyName,Parent);

	// Main Panel
	Panel = new wxPanel( this, SM_ID_PANEL);
	CreateControls();  
	Panel->Fit();
    Panel->GetSizer()->SetSizeHints(Panel);

	MenuBar = new WxMBSceneManager();

	// This function will call Update(), which is why 
	// it's not explicitly called after the selection.
	SelectAllLevels();

	SetLabel(FriendlyName);
}

//  PCF version accordance with wxWidgets standards
void WxSceneManager::CreateControls()
{  
	// Main Sizer
    wxBoxSizer* MainSizer = new wxBoxSizer(wxVERTICAL);
	Panel->SetSizer(MainSizer);

	// Toolbar Sizers
	wxBoxSizer* MainToolbarSizer = new wxBoxSizer(wxHORIZONTAL);
	MainSizer->Add(MainToolbarSizer, 0, wxGROW|wxALL, 2);

	wxBoxSizer* ToolbarLeftSizer = new wxBoxSizer(wxVERTICAL);
	MainToolbarSizer->Add(ToolbarLeftSizer, 0, wxGROW|wxALL, 2);

	wxBoxSizer* ToolbarRightSizer = new wxBoxSizer(wxVERTICAL);
	MainToolbarSizer->Add(ToolbarRightSizer, 0, wxGROW|wxALL, 2);
	
	// Top Left Toolbar (Level Selector)
	{
		Level_List = new WxSelectAllListBox( this, Panel, SM_ID_LEVELLIST, wxDefaultPosition, wxSize(SM_LEVELLIST_WIDTH, SM_LEVELLIST_HEIGHT) ); 
		ToolbarLeftSizer->Add( Level_List, 0, wxALL, 2 );
	}

	// Top Upper Toolbar (Focus, Refresh, Delete)
	{
		wxImage		TempImage;
		wxBitmap*	BitMap;

		// Toolbar Sizer
		wxBoxSizer* ToolbarSizer = new wxBoxSizer(wxHORIZONTAL);
		ToolbarRightSizer->Add(ToolbarSizer, 0, wxGROW|wxALL, 5);

		// AutoFocus Text
		wxStaticText* AutoFocusText = new wxStaticText( Panel, wxID_STATIC, TEXT("AutoFocus"), 
			wxDefaultPosition, wxDefaultSize, 0 );
		ToolbarSizer->Add(AutoFocusText, 0, wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

		// AutoFocus CheckBox
		wxCheckBox* AutoFocusCheckBox = new wxCheckBox( Panel, IDMN_SCENEMANAGER_AUTOFOCUS, TEXT(""),
			wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
		AutoFocusCheckBox->SetValue( bAutoFocus == TRUE );
		ToolbarSizer->Add(AutoFocusCheckBox, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

		// AutoSelect Text
		wxStaticText* AutoSelectText = new wxStaticText( Panel, wxID_STATIC, TEXT("AutoSelect"), 
			wxDefaultPosition, wxDefaultSize, 0 );
		ToolbarSizer->Add(AutoSelectText, 0, wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

		// AutoSelect CheckBox
		wxCheckBox* AutoSelectCheckBox = new wxCheckBox( Panel, IDMN_SCENEMANAGER_AUTOSELECT, TEXT(""),
			wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
		AutoSelectCheckBox->SetValue( bAutoSelect == TRUE );
		ToolbarSizer->Add(AutoSelectCheckBox, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

		// Focus Button
		wxBitmap FocusBitmap(wxNullBitmap);
		wxBitmapButton* FocusButton = new wxBitmapButton( Panel, IDMN_SCENEMANAGER_FOCUS, 
			FocusBitmap, wxDefaultPosition, wxSize(24,24), wxBU_AUTODRAW|wxBU_EXACTFIT );
		ToolbarSizer->Add(FocusButton, 0,wxALIGN_CENTER_VERTICAL|wxALL,2);
		TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\SceneManager_Focus.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
		BitMap = new wxBitmap(TempImage);
		FocusButton->SetBitmapLabel(*BitMap);
		FocusButton->SetToolTip(*LocalizeUnrealEd("SceneManager_Focus"));

		// Refresh Button
		wxBitmap RefreshBitmap(wxNullBitmap);
		wxBitmapButton* RefreshButton = new wxBitmapButton( Panel, IDM_RefreshBrowser, 
			RefreshBitmap, wxDefaultPosition, wxSize(24,24), wxBU_AUTODRAW|wxBU_EXACTFIT );
		ToolbarSizer->Add(RefreshButton, 0,wxALIGN_CENTER_VERTICAL|wxALL,2);
		TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\SceneManager_Refresh.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
		BitMap = new wxBitmap(TempImage);
		RefreshButton->SetBitmapLabel(*BitMap);
		RefreshButton->SetToolTip(*LocalizeUnrealEd("Refresh"));

		// Delete Button
		wxBitmap DeleteBitmap(wxNullBitmap);
		wxBitmapButton* DeleteButton = new wxBitmapButton( Panel, IDMN_SCENEMANAGER_DELETE, 
			DeleteBitmap, wxDefaultPosition, wxSize(24,24), wxBU_AUTODRAW|wxBU_EXACTFIT );
		ToolbarSizer->Add(DeleteButton, 0,wxALIGN_CENTER_VERTICAL|wxALL,2);
		TempImage.LoadFile(*FString::Printf(TEXT("%swxres\\SceneManager_Delete.bmp"), *GetEditorResourcesDir()), wxBITMAP_TYPE_BMP);
		BitMap = new wxBitmap(TempImage);
		DeleteButton->SetBitmapLabel(*BitMap);
		DeleteButton->SetToolTip(*LocalizeUnrealEd("Delete"));
	}

	// Top Bottom Toolbar (Filter, Show Brushes)
	{
		// Toolbar Sizer
		wxBoxSizer* ToolbarSizer = new wxBoxSizer(wxVERTICAL);

		//	TOP-HALF OF THE TOOLBAR
		wxBoxSizer* TopToolbarSizer = new wxBoxSizer(wxHORIZONTAL);

		// Filter Type
		wxStaticText* FilterType = new wxStaticText( Panel, wxID_STATIC, TEXT("SceneManager_FilterT"), 
			wxDefaultPosition, wxDefaultSize, 0 );
		TopToolbarSizer->Add(FilterType, 0, wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

		// Filter Combo
		wxComboBox* FilterCombo = new wxComboBox( Panel, SM_ID_TYPEFILTER, TEXT(""), 
			wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
		TopToolbarSizer->Add(FilterCombo, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

		// Show Brushes Text
		wxStaticText* ShowBText = new wxStaticText( Panel, wxID_STATIC, TEXT("ShowBrushes"),
			wxDefaultPosition, wxDefaultSize, 0 );
		TopToolbarSizer->Add(ShowBText, 0, wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

		// Show Brushes Checkbox
		wxCheckBox* ShowBCheckbox = new wxCheckBox( Panel, SM_ID_SHOWBRUSHES, TEXT(""),
			wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
		TopToolbarSizer->Add(ShowBCheckbox, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

		// Show Brushes defaults to enabled
		ShowBCheckbox->SetValue( true );

		ToolbarSizer->Add(TopToolbarSizer, 1, wxALL|wxEXPAND, 5);

		//	BOTTOM-HALF OF THE TOOLBAR
		wxBoxSizer* BottomToolbarSizer = new wxBoxSizer(wxHORIZONTAL);

		// Filter Text
		wxStaticText* FilterText = new wxStaticText( Panel, wxID_STATIC, TEXT("SceneManager_FilterQ"), 
			wxDefaultPosition, wxDefaultSize, 0 );
		BottomToolbarSizer->Add(FilterText, 0, wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);
	 
		// Filter Edit
		wxTextCtrl* FilterEdit = new wxTextCtrl( Panel, SM_ID_NAMEFILTER, TEXT(""), 
			wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER );
		BottomToolbarSizer->Add(FilterEdit, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

		ToolbarSizer->Add( BottomToolbarSizer, 1, wxALL|wxEXPAND, 5 );


		ToolbarRightSizer->Add(ToolbarSizer, 1, wxEXPAND, 5);

		ShowBrushes_Check = ShowBCheckbox;
		TypeFilter_Combo = FilterCombo;
		TypeFilter_Selection = 0;
	}

	// Grid & Property Panel: Sizer & Splitter
	wxBoxSizer* GridAndPropertiesSizer = new wxBoxSizer( wxHORIZONTAL );
	wxSplitterWindow* SplitterWindow = new wxSplitterWindow( Panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D);
	SplitterWindow->SetMinimumPaneSize( 100 );
	GridAndPropertiesSizer->Add( SplitterWindow, 1, wxEXPAND, 5 );	
	MainSizer->Add( GridAndPropertiesSizer, 1, wxEXPAND, 5 );
	
	// Grid
	wxPanel* GridPanel;
	{
		GridPanel = new wxPanel( SplitterWindow, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL  );
		wxBoxSizer* GridSizer = new wxBoxSizer( wxVERTICAL );
		
		GridSizer->SetMinSize(wxSize( 100,100 )); 
		Grid = new WxDelGrid( GridPanel, SM_ID_GRID, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS );
		Grid->Parent = this; // set parent for callbacks
		GridSizer->Add( Grid, 1, wxALL|wxEXPAND, 5 );
		Grid->EnableEditing(false);
		Grid->SetDefaultColSize(70);
		Grid->SetDefaultRowSize(18);
		Grid->SetColLabelSize(25);
		Grid->SetRowLabelSize(40);
		Grid->EnableDragRowSize(false);
		Grid->EnableCellEditControl(false);
		//Grid->EnableDragGridSize(false);
		//Grid->SetCellHighlightPenWidth(0);

		// Assign column headings
		wxArrayString colHeadings;
		for ( UINT i = 0; i < SM_NUM; i++)
		{
			colHeadings.Add(*LocalizeUnrealEd(GGridHeadings[i]));
		}

		// Initialize grid headings
		Grid->CreateGrid(0, colHeadings.Count(), wxGrid::wxGridSelectRows);
		for ( UINT i=0; i < colHeadings.Count(); i++)
		{
			Grid->SetColLabelValue(i, colHeadings[i]);
		}

		Grid->SetColSize(0,135);

		GridPanel->SetSizer( GridSizer );
		GridPanel->Layout();
		GridSizer->Fit( GridPanel );
		
	}

	FLocalizeWindow( this );

	// Property Panel
	wxPanel* PropertyPanel;
	{
		PropertyPanel = new wxPanel( SplitterWindow, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
		wxBoxSizer* PropertySizer = new wxBoxSizer( wxVERTICAL );	
		PropertySizer->SetMinSize(wxSize( 100,100 )); 
		
		PropertyWindow = new WxPropertyWindowHost;
		PropertyWindow->Create( PropertyPanel, NULL );	
		PropertySizer->Add( PropertyWindow, 1, wxALL|wxEXPAND, 5 );

		PropertyPanel->SetSizer( PropertySizer );
		PropertyPanel->Layout();
		PropertySizer->Fit( PropertyPanel );
	}

	SplitterWindow->SetSize(0,0,1000,500); // temporary fake size for correct sash position (wxWidgets bug)
	SplitterWindow->SplitVertically( GridPanel, PropertyPanel, -SM_PROPS_WIDTH );
	SplitterWindow->SetSashGravity(1);
	
	this->Layout();

	SortColumn = 0;
	bAutoFocus = FALSE; 	

	Refresh();
	bAreWindowsInitialized = TRUE;
}

// PCF End

IMPLEMENT_COMPARE_CONSTREF( FString, SceneManager, { return appStricmp( *A, *B ); } );

void WxSceneManager::PopulateCombo(const TArray<ULevel*>& Levels)
{
	// Do nothing if browser isn't initialized.
	if( bAreWindowsInitialized )
	{

		TArray<FString> ClassList;
		
		for ( INT LevelIndex = 0 ; LevelIndex < Levels.Num() ; ++ LevelIndex )
		{
			ULevel* Level = Levels( LevelIndex );
			for( INT ActorIndex = 0 ; ActorIndex < Level->Actors.Num() ; ++ActorIndex )
			{
				AActor* Actor = Level->Actors(ActorIndex);

				// Ignore world info and builder brush actors
				if( Actor && !Actor->IsABuilderBrush() && Actor->GetClass() != AWorldInfo::StaticClass() )
				{
					ClassList.AddUniqueItem( *Actor->GetClass()->GetName() );
				}
			}
		}

		// Sort the class list alphabetically
		Sort<USE_COMPARE_CONSTREF( FString, SceneManager )>( ClassList.GetTypedData(), ClassList.Num() );

		// Populate TypeFilter_Combo.
		TypeFilter_Combo->Clear();
		TypeFilter_Combo->Append( TEXT("") );

		// Fixup the TypeFilter_Selection index if the selected levels changed to preserve the 
		// type filter. If the the section index is zero, then there is no type filtering.
		UBOOL bSyncTypeIndexFromClassName = ( TypeFilter_Selection != 0 );
		
		// Reset the selection to zero in the event that a level selection change has made it such that
		// the class type is no longer present; if the class type is still present, it will be fixed up below
		if ( bSyncTypeIndexFromClassName )
		{
			TypeFilter_Selection = 0;
		}

		for ( INT ClassIndex = 0 ; ClassIndex < ClassList.Num() ; ++ClassIndex )
		{
			// If we need to synchronize the index with the class name, check if this class is the filtered type. 
			if( bSyncTypeIndexFromClassName )
			{
				if( TypeFilter_ClassName.IsSameAs( *(ClassList(ClassIndex)) ) )
				{
					// The current class name is the filter type, so update the index.
					// NOTE:	Add one to the class index because the first entry in 
					//			the type filter is a null string representing no filter.
					TypeFilter_Selection = ClassIndex + 1;
				}
			}

			TypeFilter_Combo->Append( *ClassList(ClassIndex) );
		}

		TypeFilter_Combo->SetSelection( TypeFilter_Selection );
	}
}

void WxSceneManager::PopulateGrid(const TArray<ULevel*>& Levels)
{
	// No wxWidget operations if browser isn't initialized
	if( !bAreWindowsInitialized )
	{
		return;
	}

	// save grid position
	Grid->SaveGridPosition();

	// Create Actors array
	GridActors.Empty();

	const UBOOL bShowBrushes = ShowBrushes_Check->IsChecked();
	const wxString ClassFilterString( TypeFilter_Combo->GetStringSelection() );
	const UBOOL bClassFilterEmpty = ClassFilterString.IsSameAs( TEXT("") ) ? TRUE : FALSE;
	const FString NameFilterToUpper( NameFilter.ToUpper() );

	//DOUBLE UpdateStartTime1 = appSeconds();
	for ( INT LevelIndex = 0 ; LevelIndex < Levels.Num() ; ++LevelIndex )
	{
		ULevel* Level = Levels( LevelIndex );
		TTransArray<AActor*>& Actors = Level->Actors;
		for( INT ActorIndex = 0 ; ActorIndex < Actors.Num() ; ++ActorIndex )
		{
			AActor* Actor = Actors(ActorIndex);
			if ( Actor )
			{
				if ( bClassFilterEmpty || ClassFilterString.IsSameAs(*Actor->GetClass()->GetName()) )
				{
					// Disallow brushes?
					if ( !bShowBrushes && Actor->IsABrush() )
					{
						continue;
					}

					// Never show builder brushes
					if( Actor->IsABuilderBrush() )
					{
						continue;
					}

					// Never show WorldInfo actors
					if( Actor->IsA( AWorldInfo::StaticClass() ) )
					{
						continue;
					}

					// Never show hidden actors
					if( GIsEditor && Actor->bHiddenEdScene )
					{
						continue;
					}

					/* Show only Placeable Objects
					if( (Actor->GetClass()->ClassFlags & CLASS_Hidden) || 
						(!ShowBrushes_Check->IsChecked() && !(Actor->GetClass()->ClassFlags & CLASS_Placeable)) )
						continue;
					*/

					// Check to see if we need to filter based on name.
					if ( NameFilter.Len() > 0 )
					{
						if ( !appStrstr(*FString( *Actor->GetName() ).ToUpper(), *NameFilterToUpper) )
						{
							// Names don't match, advance to the next one.
							continue;
						}
					}

					GridActors.AddItem(Actor);
				}
			}
		}
	}
	//debugf( TEXT("%s Update() - %i ms"), TEXT("GridActors"), appTrunc((appSeconds() - UpdateStartTime1) * 1000) );

	//DOUBLE UpdateStartTime2 = appSeconds();	
	// Sort actors array
	switch (SortColumn)
	{
	case SM_ACTOR: 
		Sort<USE_COMPARE_POINTER(AActor, SM_ACTOR)>( &GridActors(0), GridActors.Num() );
		break;
	case SM_TAG:
		Sort<USE_COMPARE_POINTER(AActor, SM_TAG)>( &GridActors(0), GridActors.Num() );
		break;
	case SM_TYPE:
		Sort<USE_COMPARE_POINTER(AActor, SM_TYPE)>( &GridActors(0), GridActors.Num() );
		break;
	case SM_LAYERS:
		Sort<USE_COMPARE_POINTER(AActor, SM_LAYERS)>( &GridActors(0), GridActors.Num() );
		break;
	case SM_ATTACHMENT:
		Sort<USE_COMPARE_POINTER(AActor, SM_ATTACHMENT)>( &GridActors(0), GridActors.Num() );
		break;
	case SM_KISMET:
		Sort<USE_COMPARE_POINTER(AActor, SM_KISMET)>( &GridActors(0), GridActors.Num() );
		break;
	case SM_LOCATION: break;
	case SM_PACKAGE: break;
	//case SM_MEMORY: break;
	//case SM_POLYGONS: break;
	}
	//debugf( TEXT("%s Update() - %i ms"), TEXT("Sort"), appTrunc((appSeconds() - UpdateStartTime2) * 1000) );


	//DOUBLE UpdateStartTime20 = appSeconds();	
	// Delete existing grid rows
	if ( Grid->GetNumberRows() > 0 )
	{
		Grid->DeleteRows( 0, Grid->GetNumberRows() );
	}

	// Create Grid
	Grid->AppendRows( GridActors.Num() );
	//debugf( TEXT("%s Update() - %i ms"), TEXT("Del and Create"), appTrunc((appSeconds() - UpdateStartTime20) * 1000) );

	//DOUBLE UpdateStartTime3 = appSeconds();
	for( INT ActorIndex = 0 ; ActorIndex < GridActors.Num() ; ++ActorIndex )
	{
		AActor *Actor = GridActors(ActorIndex);
		Grid->SetCellValue( ActorIndex, SM_ACTOR, *Actor->GetName() );
		Grid->SetCellValue( ActorIndex, SM_TAG, *(Actor->Tag.ToString()) );
		Grid->SetCellValue( ActorIndex, SM_TYPE, *Actor->GetClass()->GetName() );
		Grid->SetCellValue( ActorIndex, SM_LAYERS, *(Actor->Layer.ToString()) );
		
		if (Actor->Base)
		{
			Grid->SetCellValue( ActorIndex, SM_ATTACHMENT, *Actor->Base->GetName() );
		}

		const FString str = FString::Printf( TEXT("%.3f, %.3f, %.3f"), Actor->Location.X, Actor->Location.Y, Actor->Location.Z);
		Grid->SetCellValue( ActorIndex, SM_LOCATION, *str );

		// Get the kismet sequence for the level the actor belongs to.
		USequence* RootSeq = GWorld->GetGameSequence( Actor->GetLevel() );
		if ( RootSeq && RootSeq->ReferencesObject(Actor) )
		{
			Grid->SetCellValue( ActorIndex, SM_KISMET, TEXT("TRUE") );
		}

		//if ( Actor->GetOuter() && Actor->GetOuter()->GetOuter() && Actor->GetOuter()->GetOuter()->GetOuter() )
		{
			//const UPackage* pkg = (UPackage*)Actor->GetOuter()->GetOuter()->GetOuter();
			const UPackage* Package = Cast<UPackage>( Actor->GetOutermost() );
			if ( Package )
			{
				Grid->SetCellValue( ActorIndex, SM_PACKAGE, *Package->GetName() );
			}
		}
	}
	//debugf( TEXT("%s Update() - %i ms"), TEXT("CellValues"), appTrunc((appSeconds() - UpdateStartTime3) * 1000) );
	//DOUBLE UpdateStartTime4 = appSeconds();		
	
	// restore grid position
	Grid->RestoreGridPosition();
}

void WxSceneManager::GetSelectedActors(TArray<UObject*>& OutSelectedActors)
{
	if( bAreWindowsInitialized && GridActors.Num() > 0 )
	{
		// Collect selected rows
		wxArrayInt SelectedRows = Grid->GetSelectedRows();
		for( UINT RowIndex = 0 ; RowIndex < SelectedRows.Count() ; ++RowIndex )
		{	
			if ( SelectedRows[RowIndex] < GridActors.Num() )
			{
				OutSelectedActors.AddItem( GridActors(SelectedRows[RowIndex]) );
			}
		}

		// Collect rows in a selected block of cells
		const wxGridCellCoordsArray TopLeft = Grid->GetSelectionBlockTopLeft();
		const wxGridCellCoordsArray BottomRight = Grid->GetSelectionBlockBottomRight();

		if ( TopLeft.Count() && ( TopLeft.Count() == BottomRight.Count() ) )
		{
			// There are potentially multiple selection blocks, so iterate over each block
			for ( UINT SelectionBlockIndex = 0; SelectionBlockIndex < TopLeft.Count(); ++SelectionBlockIndex )
			{
				INT StartingRow = TopLeft[SelectionBlockIndex].GetRow();
				INT EndingRow = BottomRight[SelectionBlockIndex].GetRow();
				for ( INT RowIndex = StartingRow; RowIndex <= EndingRow; ++RowIndex )
				{
					if ( RowIndex < GridActors.Num() )
					{
						OutSelectedActors.AddItem( GridActors( RowIndex ) );
					}
				}
			}
		}

		// Collect selected cells
		const wxGridCellCoordsArray Cells = Grid->GetSelectedCells();
		for( UINT c = 0 ; c < Cells.Count() ; ++c )
		{		
			const INT RowIndex = Cells[c].GetRow();
			if ( RowIndex < GridActors.Num() )
			{
				OutSelectedActors.AddItem( GridActors(RowIndex) );
			}
		}
	}
}

/**
 * Overloaded, wrapper implementation that filters and returns only AActor pointers of selected objects in the grid.
 *
 * @param	OutSelectedActors	[out] The set of selected actors.
 */
void WxSceneManager::GetSelectedActors( TArray<AActor*>& OutSelectedActors )
{
	// Get the selected objects
	TArray<UObject*> SelectedObjects;
	GetSelectedActors( SelectedObjects );

	OutSelectedActors.Empty();

	// Filter the list of selected objects so that the parameter array is only filled with valid
	// AActor pointers
	for ( TArray<UObject*>::TConstIterator ObjIter( SelectedObjects ); ObjIter; ++ObjIter )
	{
		AActor* ObjAsActor = Cast<AActor>( *ObjIter );
		if ( ObjAsActor )
		{
			OutSelectedActors.AddItem( ObjAsActor );
		}
	}
}

void WxSceneManager::PopulatePropPanel()
{
	if( bAreWindowsInitialized )
	{
		// Check if the set of selected actors has changed.
		TArray<UObject*> NewSelectedActors;
		GetSelectedActors( NewSelectedActors );

		UBOOL bActorsChanged = FALSE;

		if ( NewSelectedActors.Num() != SelectedActors.Num() )
		{
			bActorsChanged = TRUE;
		}
		else
		{
			for( INT i = 0 ; i < SelectedActors.Num() ; i++ )
			{
				if ( SelectedActors(i) != NewSelectedActors(i) )
				{
					bActorsChanged = TRUE;
					break;
				}
			}
		}

		if ( bActorsChanged && NewSelectedActors.Num() > 0 )
		{
			PropertyWindow->SetObjectArray( NewSelectedActors, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories );
			PropertyWindow->Show( TRUE );
		}

		SelectedActors = NewSelectedActors;
	}
}

/**
*  Populate level list based on world info and mark ActiveLevels as selected
*/
void WxSceneManager::PopulateLevelList()
{
	Level_List->Clear();
	if ( GWorld && !GIsPlayInEditorWorld )
	{
		INT item = 0;

		// add persistent level
		AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
		Level_List->Append( *WorldInfo->GetLevel()->GetName() );
		
		// select level if is active
		if ( ActiveLevels.FindItem(WorldInfo->GetLevel(), item) )
		{
			Level_List->SetSelection(0);
		}

		// add streaming levels
		for( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
		{
			ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
			if( StreamingLevel && StreamingLevel->LoadedLevel )
			{
				Level_List->Append(*StreamingLevel->PackageName.ToString());				
		
				// select level if is active			
				if ( ActiveLevels.FindItem(StreamingLevel->LoadedLevel, item) )
				{
					Level_List->SetSelection(LevelIndex+1);
				}
			}
		}
		
	}
}

void WxSceneManager::Update()
{
	BeginUpdate();

	Grid->ClearSelection();

	// Forces all levels to be selected. 
	UBOOL bUpdate = IsShownOnScreen();
	if( bSelectAllLevelsOnNextUpdate )
	{
		SelectAllLevelsInternal();
		bSelectAllLevelsOnNextUpdate = FALSE;
		bUpdate = TRUE;
	}

	if ( bUpdate )
	{
		PopulateLevelList();
		PopulateCombo( ActiveLevels );
		PopulateGrid( ActiveLevels );
		PopulatePropPanel();
		bUpdateOnActivated = FALSE;
	}
	else
	{
		bUpdateOnActivated = TRUE;
	}

	EndUpdate();
}

void WxSceneManager::Activated()
{
	WxBrowser::Activated();
	if ( bUpdateOnActivated )
	{
		Update();
	}
}

/**
 * Selects all levels  currently loaded in the scene browser.
 */
void WxSceneManager::SelectAllLevels()
{
	bSelectAllLevelsOnNextUpdate = TRUE;
	Update();
}

/**
 * Sets up all levels  currently loaded in the scene browser to be selected on next update. 
 */
void WxSceneManager::SelectAllLevelsInternal()
{
	if( GWorld && !GIsPlayInEditorWorld )
	{
		// Clear out the active levels first to avoid an duplicate levels. 
		ActiveLevels.Empty();

		// Add the persistent level. 
		ActiveLevels.AddItem( GWorld->PersistentLevel );

		AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();

		if( WorldInfo != NULL )
		{
			// Add streaming levels
			for( TArray<ULevelStreaming*>::TIterator LevelIter(WorldInfo->StreamingLevels); LevelIter; ++LevelIter )
			{
				ULevelStreaming* StreamingLevel = *LevelIter;

				if( StreamingLevel && StreamingLevel->LoadedLevel )
				{
					ActiveLevels.AddItem( StreamingLevel->LoadedLevel );
				}
			}
		}
	}
}

void WxSceneManager::OnGridRangeSelect( wxGridRangeSelectEvent& event )
{
	// If auto-focus is enabled, select in the editor all of the actors that are selected in the grid,
	// and focus the viewport camera on them
	if ( bAutoFocus )
	{
		FocusOnSelected();
	}
	// If auto-select is enabled, select in the editor all of the actors selected in the grid
	else if ( bAutoSelect )
	{
		TArray<AActor*> ActorsToSelect;
		GetSelectedActors( ActorsToSelect );
		SelectActorsInEditor( ActorsToSelect );
	}
	PopulatePropPanel();
}

/**
 * Called when the user double-clicks a grid cell. Focuses the camera to actors represented by selected cells.
 *
 * @param	event	Event automatically generated by wxWidgets when the user double-clicks a grid cell.
 */
void WxSceneManager::OnGridCellDoubleClick( wxGridEvent& event )
{
	// Focus on the selected actors
	FocusOnSelected();
	PopulatePropPanel();
}

void WxSceneManager::OnLabelLeftClick( wxGridEvent& event )
{
	if ( event.GetCol() > -1 && 
		 SortColumn != event.GetCol() &&
		 event.GetCol() != SM_LOCATION )
	{
		SortColumn = event.GetCol();
		PopulateGrid( ActiveLevels );
	}

	if ( event.GetRow() > -1 )
	{
		event.Skip();
	}
}

void WxSceneManager::OnCellChange( wxGridEvent& event )
{
}

/** Handler for IDM_RefreshBrowser events; updates the browser contents. */
void WxSceneManager::OnRefresh( wxCommandEvent& In )
{
	// backup selection
	// http://wxforum.shadonet.com/viewtopic.php?t=5618 // GetSelectedRows bug
	wxGridCellCoordsArray tl=Grid->GetSelectionBlockTopLeft();
	wxGridCellCoordsArray br=Grid->GetSelectionBlockBottomRight();

	Update();
	
	// restore selection
	for (UINT i=0; i<tl.GetCount(); i++)
	{
		Grid->SelectBlock(tl[i],br[i],true);
	}
}

void WxSceneManager::OnFileOpen( wxCommandEvent& In )
{
	WxFileDialog OpenFileDialog( this, 
		*LocalizeUnrealEd("OpenPackage"), 
		*appScriptOutputDir(),
		TEXT(""),
		TEXT("Class Packages (*.u)|*.u|All Files|*.*\0\0"),
		wxOPEN | wxFILE_MUST_EXIST | wxMULTIPLE,
		wxDefaultPosition);
}

void WxSceneManager::FocusOnSelected()
{
	// Get all of the selected actors from the grid
	TArray<AActor*> SelectedActors;
	GetSelectedActors( SelectedActors );

	// Select each actor and focus the viewport cameras on them
	if ( SelectedActors.Num() > 0 )
	{
		SelectActorsInEditor( SelectedActors );
		GUnrealEd->MoveViewportCamerasToActor( SelectedActors, FALSE );
	}
}

/**
 * Helper function to select in the editor all of the actors specified by the parameter
 *
 * @param	InActorsToSelect	
 */
void WxSceneManager::SelectActorsInEditor( const TArray<AActor*>& InActorsToSelect ) const
{
	if ( InActorsToSelect.Num() > 0 )
	{
		// Deselect everything in the editor
		GEditor->SelectNone( FALSE, TRUE );

		// Select each valid actor
		for ( TArray<AActor*>::TConstIterator ActorIter( InActorsToSelect ); ActorIter; ++ActorIter )
		{
			AActor* CurActor = *ActorIter;
			if ( CurActor )
			{
				GEditor->SelectActor( CurActor, TRUE, NULL, TRUE );
			}
		}
	}
}

/**
 * Sets the set of levels to visualize the scene manager.
 */
void WxSceneManager::SetActiveLevels(const TArray<ULevel*>& InActiveLevels)
{
	ActiveLevels = InActiveLevels;
	Update();
}

void WxSceneManager::OnAutoFocus( wxCommandEvent& In )
{
	bAutoFocus = !bAutoFocus;
}

/**
 * Called in response to the user toggling the auto-select check box
 *
 * @param	In	Event automatically generated by wxWidgets when the user clicks the check box 
 */
void WxSceneManager::OnAutoSelect( wxCommandEvent& In )
{
	bAutoSelect = In.IsChecked();
}

void WxSceneManager::OnFocus( wxCommandEvent& In )
{
	FocusOnSelected();
}

// PCF Begin
void WxSceneManager::DeleteSelectedActors()
{
	FocusOnSelected();
	GEditor->Exec( TEXT("DELETE") );
}

void WxSceneManager::OnDelete( wxCommandEvent& In )
{
	DeleteSelectedActors();
}
// PCF End

void WxSceneManager::OnTypeFilterSelected( wxCommandEvent& event )
{
	TypeFilter_Selection = event.GetInt();
	TypeFilter_ClassName  = TypeFilter_Combo->GetStringSelection();
	Grid->ClearSelection();
	PopulateGrid( ActiveLevels );
}

void WxSceneManager::OnShowBrushes( wxCommandEvent& event )
{
	Grid->ClearSelection();
	PopulateGrid( ActiveLevels );
}

void WxSceneManager::OnNameFilterChanged( wxCommandEvent& In )
{
	NameFilter = In.GetString();
	PopulateGrid( ActiveLevels );
}

// PCF Begin
void WxSceneManager::OnLevelSelected( wxCommandEvent& event )
{
	// build new level list based on selection
	GetSelectedLevelsFromList(ActiveLevels);

	// refresh controls
	PopulateCombo( ActiveLevels );
	PopulateGrid( ActiveLevels );
	PopulatePropPanel();
}

/** 
* Build levels array based on names of selected levels from list.
*/
void WxSceneManager::GetSelectedLevelsFromList(TArray<ULevel*>& InLevels)
{
	InLevels.Empty();

	if ( GWorld && !GIsPlayInEditorWorld )
	{
		wxArrayInt Selection;
		Level_List->GetSelections(Selection);
		for (UINT i=0; i<Selection.Count(); i++)
		{
			INT idx = Selection.Item(i);
			wxString LevelName = Level_List->GetString(idx);				
			
			// is persistent level?
			AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
			if (appStrcmp(*WorldInfo->GetLevel()->GetName(),LevelName) == 0)
			{
				InLevels.AddItem(WorldInfo->GetLevel());
				continue;
			}
			
			// is streaming level?
			for( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
			{
				ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels( LevelIndex );
				if( StreamingLevel && StreamingLevel->LoadedLevel )
				{
					if (appStrcmp(*StreamingLevel->PackageName.ToString(), LevelName) == 0)
					{
						InLevels.AddItem(StreamingLevel->LoadedLevel);
						break;
					}
				}
			}

		}
	}

}

void WxSceneManager::Send(ECallbackEventType Event)
{
	PropertyWindow->RemoveAllObjects();
	GridActors.Empty();
	SelectedActors.Empty();		

	// ActiveLevels.Empty(); deprecated - moved to GetSelectedLevelsFromList
	// restore ActiveLevels array based on selected levels from Level_List
	GetSelectedLevelsFromList(ActiveLevels);

	Update();

	// Select all the levels upon the next update, which should be after the new 
	// map is loaded. If set this boolean before the above Update() call, then 
	// we wouldn't get the newly-loaded levels selected.
	if( !GWorld )
	{
		bSelectAllLevelsOnNextUpdate = TRUE;
	}
}

/**
 * Handles callbacks for UObject-specific changes, such as a level being removed.
 *
 * @param	Event		The type of event that was fired.
 * @param	EventObject	The object that was modified when this event happened. 
 */
void WxSceneManager::Send( ECallbackEventType Event, UObject* EventObject )
{
	// The only event handled by this callback is a level-removed event. 
	// This gets called when a level is added or removed from the 
	// world. In both cases, we want to re-selected all levels. 
	bSelectAllLevelsOnNextUpdate = TRUE;
}

/**
 * Adds entries to the browser's accelerator key table.  Derived classes should call up to their parents.
 */
void WxSceneManager::AddAcceleratorTableEntries(TArray<wxAcceleratorEntry>& Entries)
{
}

/**
 * Since this class holds onto an object reference, it needs to be serialized
 * so that the objects aren't GCed out from underneath us.
 *
 * @param	Ar			The archive to serialize with.
 */
void WxSceneManager::Serialize(FArchive& Ar)
{
	Ar << GridActors;
	Ar << ActiveLevels;
}
