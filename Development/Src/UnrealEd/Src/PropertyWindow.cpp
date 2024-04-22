/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnrealEdPrivateClasses.h"
#include "PropertyNode.h"
#include "ObjectsPropertyNode.h"
#include "CategoryPropertyNode.h"
#include "ItemPropertyNode.h"
#include "PropertyWindow.h"
#include "PropertyUtils.h"
#include "PropertyWindowManager.h"
#include "ScopedTransaction.h"
#include "ScopedPropertyChange.h"	// required for access to FScopedPropertyChange

#if WITH_MANAGED_CODE
	#include "ColorPickerShared.h"
#endif

#define BUTTON_SCALE			1.5f
#define FAVORITES_BORDER_SIZE	5
#define LEFT_SIDE_SPLITTER_MINIMUM 100

/** Internal convenience type. */
typedef FObjectPropertyNode::TObjectIterator		TPropObjectIterator;

/** Internal convenience type. */
typedef FObjectPropertyNode::TObjectConstIterator	TPropObjectConstIterator;

///////////////////////////////////////////////////////////////////////////////
//
// Helper Functions
//
///////////////////////////////////////////////////////////////////////////////
UBOOL IsReallyShown(wxWindow* InWindow)
{
	wxWindow* TestWindow = InWindow;
	while (TestWindow)
	{
		if (!TestWindow->IsShown())
		{
			return FALSE;
		}
		TestWindow = TestWindow->GetParent();
	}

	//the window is actually visible
	return TRUE;
}


IMPLEMENT_COMPARE_POINTER( FPropertyNode, FavoriteSortOrder,
{
	const INT ASortIndex = A->GetFavoriteIndex();
	const INT BSortIndex = B->GetFavoriteIndex();

	if (ASortIndex == BSortIndex)
	{
		return 0;
	}

	return ASortIndex < BSortIndex ? -1 : 1;
} )

IMPLEMENT_COMPARE_POINTER( WxPropertyControl, FavoriteSortOrderForWindows,
{
	const INT ASortIndex = A->GetPropertyNode()->GetFavoriteIndex();
	const INT BSortIndex = B->GetPropertyNode()->GetFavoriteIndex();

	if (ASortIndex == BSortIndex)
	{
		return 0;
	}

	return ASortIndex < BSortIndex ? -1 : 1;
} )

///////////////////////////////////////////////////////////////////////////////
//
// Helper class for Fixed Size splitters.
//
///////////////////////////////////////////////////////////////////////////////
class WxFixedSplitter : public wxSplitterWindow
{
public:
	//pass through constructor
	WxFixedSplitter(wxWindow *InParent, wxWindowID InID, const wxPoint& InPosition, const wxSize& InSize, long InStyle):
		wxSplitterWindow(InParent, InID, InPosition, InSize, InStyle)
	{
	}

	/** wx virtual function to disable sash movement */
	virtual bool OnSashPositionChange(int newSashPosition) { return false; }
};

///////////////////////////////////////////////////////////////////////////////
//
// Property Filter Window.
//
///////////////////////////////////////////////////////////////////////////////

/*-----------------------------------------------------------------------------
WxPropertyRequiredToolWindow
-----------------------------------------------------------------------------*/


IMPLEMENT_DYNAMIC_CLASS(WxPropertyRequiredToolWindow,wxPanel);

BEGIN_EVENT_TABLE( WxPropertyRequiredToolWindow, wxPanel )
	EVT_TEXT(ID_PROPWINDOW_FILTER_TEXTBOX,WxPropertyRequiredToolWindow::OnFilterChanged)
	EVT_BUTTON( ID_PROPWINDOW_FAVORITES_WINDOW_BUTTON, WxPropertyRequiredToolWindow::OnToggleWindow)
	EVT_UPDATE_UI( ID_PROPWINDOW_FAVORITES_WINDOW_BUTTON, WxPropertyRequiredToolWindow::UI_FavoritesButton)
END_EVENT_TABLE()


WxPropertyRequiredToolWindow::~WxPropertyRequiredToolWindow(void)
{
}

/** 
 * Used to create a textbox for the filter and a clear filter button
 *
 * @param parent				Parent window.  Should only be used by PropertyWindowHost
 */
void WxPropertyRequiredToolWindow::Create( wxWindow* InParent)
{
	wxEvtHandler::Connect(wxID_ANY, ID_PROPWINDOW_FILTER_TEXTBOX, wxCommandEventHandler(WxPropertyRequiredToolWindow::OnFilterChanged));

	const bool bWasCreationSuccessful = wxWindow::Create( InParent, -1, wxDefaultPosition, wxDefaultSize, wxCLIP_CHILDREN );

	wxBoxSizer* FilterWindowSizer = new wxBoxSizer(wxHORIZONTAL);

	/** Text box*/
	SearchPanel = new WxSearchControl;
	SearchPanel->Create(this, ID_PROPWINDOW_FILTER_TEXTBOX);
	FilterWindowSizer->Add(SearchPanel, 1, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxGROW | wxALL, 0);

	/**Favorite Toggle Images*/
	InitMoveButtons();
	InitFavoritesToggleButton ();
	InitLockActorButton();
	InitOptionsButton();


	FString SaveButtonFileName = TEXT("FileDirty.png");
	SaveImage.Load(SaveButtonFileName);

	wxSize ImageSize;
	ImageSize.Set(SaveImage.GetWidth(), SaveImage.GetHeight());
	ImageSize.Scale(BUTTON_SCALE, BUTTON_SCALE);

	/** Clear Button*/
	SaveButton = new WxBitmapButton(this, ID_PROPWINDOW_SAVE_BUTTON, SaveImage);
	SaveButton->SetPosition(wxDefaultPosition);
	SaveButton->SetSize(ImageSize);
	//SaveButton->SetToolTip(*LocalizeUnrealEd("PropertyWindow_Options"));

	FilterWindowSizer->Add(MoveUpButton, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 0);// | wxSHAPED | wxGROW | wxALL, 0);
	FilterWindowSizer->Add(MoveDownButton, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 0);// | wxSHAPED | wxGROW | wxALL, 0);
	FilterWindowSizer->Add(FavoritesButton, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 0);// | wxSHAPED | wxGROW | wxALL, 0);
	FilterWindowSizer->Add(LockActorButton, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 0);// | wxSHAPED | wxGROW | wxALL, 0);
	FilterWindowSizer->Add(OptionsButton, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 0);// | wxSHAPED | wxGROW | wxALL, 0);
	FilterWindowSizer->Add(SaveButton, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 0);// | wxSHAPED | wxGROW | wxALL, 0);

	SetSizer(FilterWindowSizer);
}

/*
 * Callback used for getting the key command from a TextCtrl
 */
void WxPropertyRequiredToolWindow::OnFilterChanged( wxCommandEvent& In )
{
	switch(In.GetId())
	{
		case ID_PROPWINDOW_FILTER_TEXTBOX:
		{
			WxPropertyWindowHost* HostWindow = wxDynamicCast( GetParent(), WxPropertyWindowHost );
			if (HostWindow)
			{
				HostWindow->SetFilterString (In.GetString().c_str());
			}
		}
	}
}

/**
 * Force clears the filter string
 */
void WxPropertyRequiredToolWindow::ClearFilterString(void)
{
	check(SearchPanel);
	SearchPanel->ClearFilterString();
}


/**
 * Callback for toggling different feature windows
 */
void WxPropertyRequiredToolWindow::OnToggleWindow( wxCommandEvent& In)
{
	if (In.GetId() == ID_PROPWINDOW_FAVORITES_WINDOW_BUTTON)
	{
		check(FavoritesButton);
		check(FavoritesButton->GetCurrentState());
		//toggle value
		SetFavoritesWindowVisible(FavoritesButton->GetCurrentState()->ID ? WxBitmapCheckButton::STATE_Off : WxBitmapCheckButton::STATE_On);
	}
}

/**
 * Updates the UI with the favorites window status.
 */
void WxPropertyRequiredToolWindow::UI_FavoritesButton( wxUpdateUIEvent& In )
{
	check(FavoritesButton);
	check(FavoritesButton->GetCurrentState());
	check(MoveUpButton);
	check(MoveDownButton);

	WxPropertyWindowHost* HostWindow = wxDynamicCast( GetParent(), WxPropertyWindowHost );
	if (HostWindow)
	{
		UBOOL bFavoritesVisible = HostWindow->GetFavoritesVisible();
		INT TestState = bFavoritesVisible ? WxBitmapCheckButton::STATE_On : WxBitmapCheckButton::STATE_Off;
		if (TestState != FavoritesButton->GetCurrentState()->ID)	//to stop circular events from happening
		{
			//set button state and hide move buttons if favorites is currently inactive
			FavoritesButton->SetCurrentState(TestState);
			MoveUpButton->Show(bFavoritesVisible ? true : false);
			MoveDownButton->Show(bFavoritesVisible ? true : false);

			GetSizer()->Layout();
		}

		In.Check( bFavoritesVisible ? true : false );
	}
}


/**
 * Accessor functions for state of the favorites button
 */
void WxPropertyRequiredToolWindow::SetFavoritesWindowVisible(const UBOOL bShow)
{
	//pass to parent window to update visibility
	WxPropertyWindowHost* HostWindow = wxDynamicCast(GetParent(), WxPropertyWindowHost);
	check(HostWindow);
	HostWindow->SetFavoritesWindow(bShow);
	
	// Hide/Show Favorite shift buttons and refresh the layout
 	MoveDownButton->Show(bShow ? true : false);
 	MoveUpButton->Show(bShow ? true : false);
	GetSizer()->Layout();
}

/**
 * Accessor functions for state of the favorites button
 */
void WxPropertyRequiredToolWindow::SetActorLockButtonVisible (const UBOOL bShow, const UBOOL bIsLocked)
{
	check(LockActorButton);
	if (bShow)
	{
		LockActorButton->Show();
		//update the state of this button
		INT TestState = bIsLocked ? WxBitmapCheckButton::STATE_On : WxBitmapCheckButton::STATE_Off;
		if (TestState != LockActorButton->GetCurrentState()->ID)	//to stop circular events from happening
		{
			//set button state
			LockActorButton->SetCurrentState(TestState);
		}

	} 
	else
	{
		LockActorButton->Hide();
	}

	//re-layout buttons
	wxSizer* SizerForRefresh = GetSizer();
	check(SizerForRefresh);
	SizerForRefresh->Layout();
}

void WxPropertyRequiredToolWindow::SetSaveButtonVisible(const UBOOL bShow)
{
	if (bShow)
	{
		SaveButton->Show();
	}
	else
	{
		SaveButton->Hide();
	}
	
	//re-layout buttons
	wxSizer* SizerForRefresh = GetSizer();
	check(SizerForRefresh);
	SizerForRefresh->Layout();
}

/** Rebuild the focus array to go through all open children */
void WxPropertyRequiredToolWindow::AppendFocusWindows (OUT TArray<wxWindow*>& OutFocusArray)
{
	check(SearchPanel);
	SearchPanel->AppendFocusWindows(OUT OutFocusArray);
}

/**
 * Init function to prepare the images and window for toggling the favorites window
 */
void WxPropertyRequiredToolWindow::InitFavoritesToggleButton (void )
{
	check(GPropertyWindowManager);

	wxSize ImageSize;
	ImageSize.Set(GPropertyWindowManager->FavoritesOnImage.GetWidth(), GPropertyWindowManager->FavoritesOnImage.GetHeight());
	ImageSize.Scale(BUTTON_SCALE, BUTTON_SCALE);

	/** Favorite Toggle Button*/
	FavoritesButton = new WxBitmapCheckButton(this, this, ID_PROPWINDOW_FAVORITES_WINDOW_BUTTON, &(GPropertyWindowManager->FavoritesOffImage), &(GPropertyWindowManager->FavoritesOnImage));//*LocalizeUnrealEd("Cancel"));
	FavoritesButton->SetPosition(wxDefaultPosition);
	FavoritesButton->SetSize(ImageSize);
	FavoritesButton->SetToolTip(*LocalizeUnrealEd("PropertyWindow_ToggleFavoritesWindow"));
}
/**
 * Init function to prepare the images and window for toggling actor lock
 */
void WxPropertyRequiredToolWindow::InitLockActorButton (void)
{
	FString LockActorOnFileName = TEXT("LockOn.png");
	FString LockActorOffFileName = TEXT("LockOff.png");
	LockActorOnImage.Load ( LockActorOnFileName );
	LockActorOffImage.Load( LockActorOffFileName );

	ensure ((LockActorOnImage.GetWidth() == LockActorOffImage.GetWidth()) || (LockActorOnImage.GetHeight() == LockActorOffImage.GetHeight()));

	wxSize ImageSize;
	ImageSize.Set(LockActorOnImage.GetWidth(), LockActorOnImage.GetHeight());
	ImageSize.Scale(BUTTON_SCALE, BUTTON_SCALE);

	/** Lock Actor Button*/
	LockActorButton = new WxBitmapCheckButton(this, this, ID_LOCK_SELECTIONS, &LockActorOffImage, &LockActorOnImage);
	LockActorButton->SetPosition(wxDefaultPosition);
	LockActorButton->SetSize(ImageSize);
	LockActorButton->SetToolTip(*LocalizeUnrealEd("PropertyWindow_LockSelectedActorsQ"));
	LockActorButton->Hide();
}

/**
 * Init function to prepare the menu for all infrequently used options
 */
void WxPropertyRequiredToolWindow::InitOptionsButton (void)
{
	FString OptionsButtonFileName = TEXT("Options.png");
	OptionsImage.Load( OptionsButtonFileName );

	wxSize ImageSize;
	ImageSize.Set(OptionsImage.GetWidth(), OptionsImage.GetHeight());
	ImageSize.Scale(BUTTON_SCALE, BUTTON_SCALE);

	/** Clear Button*/
	OptionsButton = new WxBitmapCheckButton(this, this, ID_PROPWINDOW_OPTIONS_BUTTON, &OptionsImage, &OptionsImage);
	OptionsButton->SetPosition(wxDefaultPosition);
	OptionsButton->SetSize(ImageSize);
	OptionsButton->SetToolTip(*LocalizeUnrealEd("PropertyWindow_Options"));
}

/**
 * Init function to prepare the images and buttons for shifting favorite items
 */
void WxPropertyRequiredToolWindow::InitMoveButtons (void)
{
	FString MoveUpFileName = TEXT("UpArrow.png");
	FString MoveDownFileName = TEXT("DownArrowBlue.png");

	MoveUpImage.Load( MoveUpFileName );
	MoveDownImage.Load( MoveDownFileName );

	wxSize MoveUpImageSize;
	MoveUpImageSize.Set(MoveUpImage.GetWidth(), MoveUpImage.GetHeight());
	MoveUpImageSize.Scale(BUTTON_SCALE, BUTTON_SCALE);

	wxSize MoveDownImageSize;
	MoveDownImageSize.Set(MoveDownImage.GetWidth(), MoveDownImage.GetHeight());
	MoveDownImageSize.Scale(BUTTON_SCALE, BUTTON_SCALE);

	MoveUpButton = new WxBitmapButton(this, ID_PROPWINDOW_FAVORITES_MOVE_UP, MoveUpImage);
	MoveUpButton->SetSize(MoveUpImageSize);
	MoveUpButton->SetToolTip(*LocalizeUnrealEd("PropertyWindow_MoveUp"));
	
	MoveDownButton = new WxBitmapButton(this, ID_PROPWINDOW_FAVORITES_MOVE_DOWN, MoveDownImage);
	MoveDownButton->SetSize(MoveDownImageSize);
	MoveDownButton->SetToolTip(*LocalizeUnrealEd("PropertyWindow_MoveDown"));

	// Initially hide move buttons. Only show if the favorites panel is active.
	MoveDownButton->Hide();
	MoveUpButton->Hide();
}

///////////////////////////////////////////////////////////////////////////////
//
// Property window host.
//
///////////////////////////////////////////////////////////////////////////////

/*-----------------------------------------------------------------------------
WxPropertyWindowHost
-----------------------------------------------------------------------------*/

IMPLEMENT_DYNAMIC_CLASS(WxPropertyWindowHost,wxPanel);

BEGIN_EVENT_TABLE( WxPropertyWindowHost, wxPanel )
EVT_BUTTON( ID_PROPWINDOW_OPTIONS_BUTTON, WxPropertyWindowHost::ShowOptionsMenu )
EVT_BUTTON( ID_PROPWINDOW_FAVORITES_MOVE_UP, WxPropertyWindowHost::OnMoveFavoritesUp )
EVT_BUTTON( ID_PROPWINDOW_FAVORITES_MOVE_DOWN, WxPropertyWindowHost::OnMoveFavoritesDown )
EVT_MENU( IDM_COPY, WxPropertyWindowHost::OnCopy )
EVT_MENU( IDM_COPYCOMPLETE, WxPropertyWindowHost::OnCopyComplete )
EVT_MENU( ID_EXPAND_ALL, WxPropertyWindowHost::OnExpandAll )
EVT_MENU( ID_COLLAPSE_ALL, WxPropertyWindowHost::OnCollapseAll )
EVT_BUTTON( ID_LOCK_SELECTIONS, WxPropertyWindowHost::OnLock )
EVT_UPDATE_UI( ID_LOCK_SELECTIONS, WxPropertyWindowHost::UI_Lock )
EVT_SIZE (WxPropertyWindowHost::OnSize)
EVT_BUTTON( ID_PROPWINDOW_SAVE_BUTTON, WxPropertyWindowHost::OnSaveObject )

EVT_CHAR( WxPropertyWindowHost::OnChar )

EVT_TOOL(ID_UI_PROPERTYWINDOW_COPY, WxPropertyWindowHost::OnCopyFocusProperties)
EVT_TOOL(ID_UI_PROPERTYWINDOW_PASTE, WxPropertyWindowHost::OnPasteFromClipboard)

EVT_MENU(ID_TOGGLE_SHOW_HIDDEN_PROPERTIES, WxPropertyWindowHost::MenuPropWinShowHiddenProperties)
EVT_MENU(ID_TOGGLE_SHOW_PROPERTIES_IN_SCRIPT_DEFINED_ORDER, WxPropertyWindowHost::MenuPropToggleUseScriptDefinedOrder)
EVT_MENU(ID_TOGGLE_SHOW_HORIZONTAL_DIVIDERS, WxPropertyWindowHost::MenuPropToggleShowHorizontalDividers)
EVT_MENU(ID_TOGGLE_SHOW_FRIENDLY_PROPERTY_NAMES, WxPropertyWindowHost::MenuPropWinToggleShowFriendlyPropertyNames)
EVT_MENU(ID_TOGGLE_MODIFIED_ITEMS, WxPropertyWindowHost::MenuPropWinToggleShowModifiedProperties)
EVT_MENU(ID_TOGGLE_DIFFERING_ITEMS_ONLY, WxPropertyWindowHost::MenuPropWinToggleShowDifferingProperties)
EVT_UPDATE_UI(ID_TOGGLE_DIFFERING_ITEMS_ONLY, WxPropertyWindowHost::UI_ToggleDifferingUpdate)
EVT_MOUSEWHEEL( WxPropertyWindowHost::OnMouseWheel )

END_EVENT_TABLE()

WxPropertyWindowHost::~WxPropertyWindowHost()
{
	WxPropertyWindow* DeleteWindows[2];
	DeleteWindows[0] = PropertyFavoritesWindow;
	DeleteWindows[1] = PropertyWindow;

	PropertyFavoritesWindow = NULL;
	PropertyWindow = NULL;

	WxPropertyWindow* NormalWindow = PropertyFavoritesWindow;
	if (DeleteWindows[0])
	{
		delete DeleteWindows[0];
	}
	if (DeleteWindows[1])
	{
		delete DeleteWindows[1];
	}
	if (PropertyRequiredToolWindow)
	{
		delete PropertyRequiredToolWindow;
		PropertyRequiredToolWindow = NULL;
	}
}

/** 
 * Window creation function for the host (container of all feature windows)
 * @param Parent			- Parent window this is a child of
 * @param InNotifyHook		- Callback to use on destruction
 * @param InID				- ID to assign to this window
 * @param InFeatureFlags	- Flags that determine which feature windows to use (filter, favorites, toolbar)
 */
void WxPropertyWindowHost::Create( wxWindow* Parent, FNotifyHook* InNotifyHook, wxWindowID InID, UBOOL bShowTools)
{
	//set default windows to invalid value
	PropertyWindow = NULL;
	PropertyFavoritesWindow = NULL;

	check(Parent);	//should only be used by wxPropertyWindowFrame

	const bool bWasCreateSuccessful = wxPanel::Create(Parent, InID, wxDefaultPosition, wxDefaultSize, wxCLIP_CHILDREN | wxWANTS_CHARS);
	check( bWasCreateSuccessful );

	PropertyFeatureSizer = new wxBoxSizer(wxVERTICAL);
	CreateToolbar(PropertyFeatureSizer);

	//Add the splitter between the favorites and main property windows
	FavoritesSplitter = new WxFixedSplitter(this, -1, wxDefaultPosition, wxDefaultSize, wxSP_3D);

	//create panel to limit the size of the favorites property window
	FavoritesPanel = new wxPanel(FavoritesSplitter, -1, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER);

	//Main Property Window
	PropertyWindow = new WxPropertyWindow;
	check( PropertyWindow );
	PropertyWindow->Create( FavoritesSplitter, InNotifyHook );

	/** Favorites Window*/
	PropertyFavoritesWindow = new WxPropertyWindow;
	check( PropertyFavoritesWindow );
	PropertyFavoritesWindow->Create(FavoritesPanel, InNotifyHook);

	//Default windows are not locked
	bLocked = FALSE;
	//hide Favorites based on ini settings
	bFavoritesWindowEnabled = FALSE;
	if (bShowTools)
	{
		GConfig->GetBool(TEXT("PropertyWindow"), TEXT("ShowFavoritesWindow"), bFavoritesWindowEnabled, GEditorUserSettingsIni);
	}
	else
	{
		PropertyRequiredToolWindow->Hide();
	}

	//Create internal sizer
	/**Sizer used to clamp the size of the favorites property window*/
	wxBoxSizer* FavoritesSizer = new wxBoxSizer( wxHORIZONTAL);
	FavoritesSizer->Add(PropertyFavoritesWindow, 1,  wxGROW|wxALL, FAVORITES_BORDER_SIZE);
	FavoritesPanel->SetSizer(FavoritesSizer);

	//add splitter to the main sizer (tool bar is the only other entry)
	PropertyFeatureSizer->Add(FavoritesSplitter, 1, wxGROW|wxALL, 0 );

	SetSizer(PropertyFeatureSizer);

	ApplyFavorites();

	//set up accelerator keys
	wxAcceleratorEntry entries[2];
	entries[0].Set(wxACCEL_CTRL,  (int) 'C',     ID_UI_PROPERTYWINDOW_COPY);
	entries[1].Set(wxACCEL_CTRL,  (int) 'V',     ID_UI_PROPERTYWINDOW_PASTE);
	wxAcceleratorTable accel(2, entries);
	SetAcceleratorTable(accel);

	// Set up prop offset
	PropOffset = PROP_Indent;
	
	//Enable category option by default
	bEnableCategoryOptions = TRUE;
}

/**
 * Creates Filter and tool bar
 */
void WxPropertyWindowHost::CreateToolbar(wxBoxSizer* InParentSizer)
{
	/** Filter Window*/
	PropertyRequiredToolWindow = new WxPropertyRequiredToolWindow;
	check( PropertyRequiredToolWindow );
	PropertyRequiredToolWindow->Create(this);

	//Make the horizontal sider for the entire toolbar
	wxStaticBoxSizer* FilterSizer = new wxStaticBoxSizer(wxHORIZONTAL, this);

	FilterSizer->Add(PropertyRequiredToolWindow, 1, wxALIGN_LEFT | wxGROW | wxALL, 0);

	InParentSizer->Add(FilterSizer, 0, wxALIGN_CENTER | wxGROW | wxALL, 0);
}

/**
 * Pass through function to set Filter String to the Property Window
 */
void WxPropertyWindowHost::SetFilterString(const FString& InFilterString)
{
	check(PropertyWindow);
	PropertyWindow->SetFilterString(InFilterString);
}

void WxPropertyWindowHost::SetFavoritesWindow(const UBOOL bShowFavoritesWindow)
{
	bFavoritesWindowEnabled = bShowFavoritesWindow;

	// If showing the favorites window, be sure to load the proportion for the property splitter as it is likely
	// incorrect as a result of being hidden when the owner frame was shown
	if ( bFavoritesWindowEnabled )
	{
		PropertyFavoritesWindow->LoadSplitterProportion( PropertyFavoritesWindow->GetObjectBaseClass() );
	}
	// Save off the splitter proprition of the favorites window when hiding it
	else
	{
		PropertyFavoritesWindow->SaveSplitterProportion( PropertyFavoritesWindow->GetObjectBaseClass() );
	}
	PropertyFavoritesWindow->Show(bFavoritesWindowEnabled == TRUE);

	//save off state of the filter window
	GConfig->SetBool(TEXT("PropertyWindow"), TEXT("ShowFavoritesWindow"), bFavoritesWindowEnabled, GEditorUserSettingsIni);

	AdjustFavoritesSplitter();
}

/**
 * Putting options menu items item a popup
 */
void WxPropertyWindowHost::ShowOptionsMenu(wxCommandEvent& In)
{
	wxMenu* OptionsMenu = new wxMenu();

	OptionsMenu->Append( IDM_COPY, *LocalizeUnrealEd("PropertyWindow_CopyToClipboard"), TEXT("") );
	OptionsMenu->Append( IDM_COPYCOMPLETE, *LocalizeUnrealEd("PropertyWindow_CompleteCopyToClipboard"), TEXT("") );
	OptionsMenu->AppendSeparator();
	if (bEnableCategoryOptions)
	{
		OptionsMenu->Append( ID_EXPAND_ALL, *LocalizeUnrealEd("PropertyWindow_ExpandAllCategories"), TEXT("") );
		OptionsMenu->Append( ID_COLLAPSE_ALL, *LocalizeUnrealEd("PropertyWindow_CollapseAllCategories"), TEXT("") );
		OptionsMenu->AppendSeparator();
	}

	UBOOL bValue = GPropertyWindowManager->GetShowHiddenProperties();
	//Disabled as a safety precaution.  KNOW WHAT YOU ARE DOING if you enable this
	//OptionsMenu->AppendCheckItem(ID_TOGGLE_SHOW_HIDDEN_PROPERTIES, *LocalizeUnrealEd("PropertyWindow_ShowHiddenProperties"), *LocalizeUnrealEd("PropertyWindow_ToolTip_ShowHiddenProperties"))->Check(bValue ? true : false);

	UBOOL bUseScriptDefinedOrderValue = GPropertyWindowManager->GetUseScriptDefinedOrder();
	OptionsMenu->AppendCheckItem(ID_TOGGLE_SHOW_PROPERTIES_IN_SCRIPT_DEFINED_ORDER, *LocalizeUnrealEd("PropertyWindow_UseScriptDefinedOrder"), *LocalizeUnrealEd("PropertyWindow_ToolTip_ShowHiddenProperties"))->Check(bUseScriptDefinedOrderValue ? true : false);

	UBOOL bShowHorizontalDividers = GPropertyWindowManager->GetShowHorizontalDividers();
	OptionsMenu->AppendCheckItem(ID_TOGGLE_SHOW_HORIZONTAL_DIVIDERS, *LocalizeUnrealEd("PropertyWindow_ShowHorizontalDividers"), *LocalizeUnrealEd("PropertyWindow_ToolTip_ShowHorizontalDividers"))->Check(bShowHorizontalDividers ? true : false);

	UBOOL bShowFriendlyPropertyNames = GPropertyWindowManager->GetShowFriendlyPropertyNames();
	OptionsMenu->AppendCheckItem(ID_TOGGLE_SHOW_FRIENDLY_PROPERTY_NAMES, *LocalizeUnrealEd("PropertyWindow_ShowFriendlyPropertyNames"), *LocalizeUnrealEd("PropertyWindow_ToolTip_ShowFriendlyPropertyNames"))->Check(bShowFriendlyPropertyNames ? true : false);

	UBOOL bShowOnlyModifiedProperties = HasFlags(EPropertyWindowFlags::ShowOnlyModifiedItems);
	OptionsMenu->AppendCheckItem(ID_TOGGLE_MODIFIED_ITEMS, *LocalizeUnrealEd("PropertyWindow_ShowModifiedProperties"), *LocalizeUnrealEd("PropertyWindow_ToolTip_ShowModifiedProperties"))->Check(bShowOnlyModifiedProperties ? true : false);

	UBOOL bShowOnlyDifferingProperties = HasFlags(EPropertyWindowFlags::ShowOnlyDifferingItems);
	OptionsMenu->AppendCheckItem(ID_TOGGLE_DIFFERING_ITEMS_ONLY, *LocalizeUnrealEd("PropertyWindow_ShowDifferingProperties"), *LocalizeUnrealEd("PropertyWindow_ToolTip_ShowDifferingProperties"))->Check(bShowOnlyDifferingProperties ? true : false);

	PopupMenu(OptionsMenu);
}

/**
 * Shifts the selected favorite property node in the given direction.
 * @param InDirection - Direction to shift from selected
 */
void WxPropertyWindowHost::MoveFavorites(const EPropertyWindowDirections::Type InDirection)
{
	// Make sure the property favorites window is currently being focused
	if(NumActiveFocusWindows() == 1 && ActiveFocusWindows(0)->GetPropertyWindow() == PropertyFavoritesWindow)
	{
		// Grab the currently selected node and the nearest in our given direction.
 		FPropertyNode* Node1 = ActiveFocusWindows(0)->GetPropertyNode();
 		FPropertyNode* Node2 = PropertyFavoritesWindow->FindNearestFavoriteNode(InDirection, *Node1);
	
		// If we were able to successfully grab two nodes in the window, swap their favorite indices, save the change and redraw
 		if(Node1 && Node2)
 		{
 			INT FavoriteIndexTemp = Node1->GetFavoriteIndex();
 			Node1->SetFavoriteIndex(Node2->GetFavoriteIndex());
 			Node2->SetFavoriteIndex(FavoriteIndexTemp);
 
 			PropertyFavoritesWindow->SaveFavoriteNodes(*Node1, *Node2);
 			PropertyFavoritesWindow->RefreshEntireTree();
 		}
	}
}
/**Adjusts the layout of the favorites/main property windows*/
void WxPropertyWindowHost::AdjustFavoritesSplitter(UBOOL bFavoritesChanged)
{
	if (PropertyFavoritesWindow && PropertyWindow)
	{
		//don't update while reconfiguring window
		Freeze();
		
		// grab current mouse position
		POINT pt;
		::GetCursorPos(&pt);

		//store some info about the property window
		INT ScrollPos = PropertyWindow->GetScrollBarPos();
		INT ScrollMax = PropertyWindow->GetScrollBarMax();
	
		// check to see if the mouse is currently within the favorites panel
		INT LocalX = pt.x; INT LocalY = pt.y;
		FavoritesPanel->ScreenToClient(&LocalX, &LocalY);
		UBOOL bMouseWithinFavorites = FavoritesPanel->GetRect().Contains(LocalX, LocalY);
		
		// Grab the current panel height, before being changed
		const INT PanelHeightPrevious = FavoritesPanel->GetSize().y;

		check(FavoritesPanel);
		check(FavoritesSplitter);

		//default to hiding the window
		FavoritesSplitter->Unsplit(FavoritesPanel);

		if (bFavoritesWindowEnabled && PropertyFavoritesWindow->HasFlags(EPropertyWindowFlags::HasAnyFavoritesSet))
		{
			//ensure window is shown
			FavoritesPanel->Show();

			//top and bottom border + font.  A bit of a fudge factor
			const INT ApproximateBoxBorderSize = FAVORITES_BORDER_SIZE*3;
			const INT MaxFavoritesHeight = GetClientRect().GetHeight()/2;
			const INT FavoritesHeight = Clamp(PropertyFavoritesWindow->GetMaxHeight() + ApproximateBoxBorderSize, 1, MaxFavoritesHeight);
			FavoritesSplitter->SplitHorizontally(FavoritesPanel, PropertyWindow, FavoritesHeight);
		}
		else
		{
			//split (in case it has never been split before)
			FavoritesSplitter->SplitHorizontally(FavoritesPanel, PropertyWindow, 1);

			//now unsplit to rehide the window
			FavoritesSplitter->Unsplit(FavoritesPanel);
		}
		
		// if our mouse was not in the favorites panel, jump the pointer relative to the difference in panel height
		if(bFavoritesChanged && !bMouseWithinFavorites)
		{
			const INT PanelHeightDelta = FavoritesPanel->GetSize().y - PanelHeightPrevious;
			INT MouseDist = PanelHeightDelta;
			INT ScrollDist = PanelHeightDelta;

			INT NewX = pt.x;
			INT NewY = pt.y + PanelHeightDelta;
			PropertyWindow->ScreenToClient(&NewX, &NewY);

			BOOL bMovePointer = FALSE;
			BOOL bMoveScroll = FALSE;

			//Get a bunch of info about where our pointer and scroll position.
			const BOOL bAddingFavorites = PanelHeightDelta >= 0;
			const BOOL bPointerAtTop = NewY < abs(PanelHeightDelta);
			const BOOL bPointerAtBottom = NewY > (PropertyWindow->GetSize().y - abs(PanelHeightDelta));
			const BOOL bScrollAtTop = ScrollPos <= abs(PanelHeightDelta);
			const BOOL bScrollAtBottom = ScrollPos >= (ScrollMax - abs(PanelHeightDelta));

			//Set the new mouse/scroll position depending on where they are currently
			if (bAddingFavorites)
			{
				if (bPointerAtTop)
				{
					bMovePointer = TRUE;
					if (bScrollAtBottom)
					{
						ScrollDist = -ScrollDist/8;
						bMoveScroll = TRUE;
					}
				}
				else if (bPointerAtBottom)
				{
					bMoveScroll = TRUE;
				}
				else //pointer in the middle
				{
					if (bScrollAtTop)
					{
						bMovePointer = TRUE;
					}
					else 
					{
						bMoveScroll = TRUE;
					}	
				}
			}
			else
			{
				if (bPointerAtTop)
				{
					if (bScrollAtBottom)
					{
						bMoveScroll = TRUE;
						ScrollDist = -ScrollDist/8;
					}
					else
					{
						bMovePointer = TRUE;
					}	
				}
				else //pointer in the middle
				{
					if (bScrollAtTop)
					{
						bMovePointer = TRUE;
					}
					else 
					{
						bMoveScroll = TRUE;
					}			
				}
			}

			if (bMovePointer) 
			{
				SetCursorPos(pt.x, pt.y + MouseDist);	
			}
			if (bMoveScroll)
			{
				PropertyWindow->SetScrollBarPos(ScrollPos + ScrollDist);	
			}
		}

		//Split Filter from Levels
		Layout();

		//don't update while reconfiguring window
		Thaw();
	}
}

/**Tells both favorites and main property windows to re-apply their favorites settings*/
void WxPropertyWindowHost::ApplyFavorites(UBOOL bFavoritesChanged)
{
	check(PropertyFavoritesWindow);
	check(PropertyWindow);

	//favorites window should remark property nodes
	PropertyFavoritesWindow->MarkFavorites(NULL);
	//main property window should mark its favorites
	PropertyWindow->MarkFavorites(NULL);
	AdjustFavoritesSplitter(bFavoritesChanged);
}


/**
 * Copies selected objects to clip board.
 */
void WxPropertyWindowHost::OnCopy( wxCommandEvent& In )
{
	FStringOutputDevice Ar;
	const FExportObjectInnerContext Context;
	for( TPropObjectIterator Itor( ObjectIterator() ) ; Itor ; ++Itor )
	{
		UExporter::ExportToOutputDevice( &Context, *Itor, NULL, Ar, TEXT("T3D"), 0 );
		Ar.Logf(LINE_TERMINATOR);
	}

	appClipboardCopy(*Ar);
}

/**
 * Copies selected objects to clip board.
 */
void WxPropertyWindowHost::OnCopyComplete( wxCommandEvent& In )
{
	FStringOutputDevice Ar;

	// this will contain all objects to export, including editinline objects
	TArray<UObject*> AllObjects;
	for( TPropObjectIterator Itor( ObjectIterator() ) ; Itor ; ++Itor )
	{
		AllObjects.AddUniqueItem( *Itor );
	}

	// now go through and find all of the editinline objects, and add to our list, "recursively"
	// not true recursion because i don't want to fall into some nasty infinite loop
	INT OldNumObjects, NewNumObjects;
	do
	{
		OldNumObjects = AllObjects.Num();
		// go through all objects looking for editlines
		//@todo: this could be faster if we only started looking at the newly added objects
		for (INT iObject = 0; iObject < OldNumObjects; iObject++)
		{
			// loop through all of the properties
			for (TFieldIterator<UProperty> It(AllObjects(iObject)->GetClass()); It; ++It)
			{
				// We need to add on objects that editline object properties point to
				if ((It->PropertyFlags & (CPF_EditInline | CPF_EditInlineUse)) && Cast<UObjectProperty>(*It))
				{
					// get the value of the property
					UObject* EditInlineObject;
					It->CopySingleValue(&EditInlineObject, ((BYTE*)AllObjects(iObject)) + It->Offset);

					// did the property actually point to something?
					if (EditInlineObject)
					{
						// add it to our list
						AllObjects.AddUniqueItem(EditInlineObject);
					}
				}
			}
		}
		// find out if we added any new objects
		NewNumObjects = AllObjects.Num();
	} while (OldNumObjects != NewNumObjects);

	const FExportObjectInnerContext Context;
	for( INT o = 0 ; o < AllObjects.Num() ; ++o )
	{
		UObject* obj = AllObjects(o);
		UExporter::ExportToOutputDevice( &Context, obj, NULL, Ar, TEXT("T3D"), 0 );
		Ar.Logf(LINE_TERMINATOR);
	}

	appClipboardCopy(*Ar);
}

/**
 * Expands all categories for the selected objects.
 */
void WxPropertyWindowHost::OnExpandAll( wxCommandEvent& In )
{
	PropertyWindow->ExpandAllItems();
}

/**
 * Collapses all categories for the selected objects.
 */
void WxPropertyWindowHost::OnCollapseAll( wxCommandEvent& In )
{
	PropertyWindow->CollapseAllItems();
}

/**
 * Locks or unlocks the property window, based on the 'checked' status of the event.
 */
void WxPropertyWindowHost::OnLock( wxCommandEvent& In )
{
	WxBitmapCheckButton* LockButton = wxDynamicCast(In.GetEventObject(), WxBitmapCheckButton);
	check(LockButton);
	check(LockButton->GetCurrentState());
	bLocked = LockButton->GetCurrentState()->ID;
}
/**
 * Updates the UI with the locked status of the contained property window.
 */
void WxPropertyWindowHost::UI_Lock( wxUpdateUIEvent& In )
{
	// Update the UI with the locked status of the contained property window.
	In.Check( IsLocked() ? true : false );
}

/**
 * Captures passed up key events to deal with focus changing
 */
void WxPropertyWindowHost::OnChar( wxKeyEvent& In )
{
	UBOOL bCtrlPressed = In.ControlDown();

	UBOOL bMovedFocus = FALSE;	//if we are only looking at modified and we reset something to default and lost focus
	wxWindow* EventWindow = wxDynamicCast(In.GetEventObject(), wxWindow);

	if (EventWindow)
	{
		switch( In.GetKeyCode() )
		{
		case WXK_DOWN:
			// Move the selection down.
			MoveFocus(EPropertyWindowDirections::Down, EventWindow );
			bMovedFocus = TRUE;
			break;

		case WXK_UP:
			// Move the selection up.
			MoveFocus(EPropertyWindowDirections::Up, EventWindow );
			bMovedFocus = TRUE;
			break;

		case WXK_TAB:
			if( In.ShiftDown() )
			{
				// SHIFT-Tab moves the selection up.
				MoveFocus(EPropertyWindowDirections::Up, EventWindow );
			}
			else
			{
				// Tab moves the selection down.
				MoveFocus(EPropertyWindowDirections::Down, EventWindow );
			}
			bMovedFocus = TRUE;
			break;
		default:
			In.Skip();
			break;
		}
		if (bMovedFocus)
		{
			//add the refresh in here just 
			check(PropertyWindow);
			PropertyWindow->RefreshEntireTree();
		}
	}
}

/**Copies Name/Value pairs from selected properties*/
void WxPropertyWindowHost::OnCopyFocusProperties(wxCommandEvent& In)
{
	FString FinalBuffer;
	FinalBuffer = CopyActiveFocusPropertiesToClipboard();

	//throw the results on the clipboard
	appClipboardCopy(*FinalBuffer);
}

/**Pastes Name/Value pairs from clipboard to properties*/
void WxPropertyWindowHost::OnPasteFromClipboard(wxCommandEvent& In)
{
	FString PropertyBuffer = appClipboardPaste();

	TArray<FString> PropertyNameValuePairs;
	PropertyBuffer.ParseIntoArrayWS(&PropertyNameValuePairs);

	RequestMainWindowTakeFocus();
	ClearActiveFocus();

	//for undo, start transaction
	if ( GEditor )
	{
		GEditor->BeginTransaction( *LocalizeUnrealEd("PropertyWindowEditProperties") );
	}

	if (PropertyFavoritesWindow->IsShown())
	{
		PropertyFavoritesWindow->PastePropertyValuesFromClipboard(PropertyFavoritesWindow->GetRoot(), PropertyNameValuePairs);
	}
	PropertyWindow->PastePropertyValuesFromClipboard(PropertyWindow->GetRoot(), PropertyNameValuePairs);

	// End the property edit transaction.
	if ( GEditor )
	{
		GEditor->EndTransaction();
	}

	GPropertyWindowManager->bChangesTopology = TRUE;
	Freeze();
	PropertyFavoritesWindow->Rebuild();
	PropertyWindow->Rebuild();
	Thaw();
	GPropertyWindowManager->bChangesTopology = FALSE;
}

/** Event handler for toggling whether or not all property windows should display all of their buttons. */
void WxPropertyWindowHost::MenuPropWinShowHiddenProperties(wxCommandEvent &In)
{
	UBOOL bShowAllButtons = In.IsChecked() ? TRUE : FALSE;
	GPropertyWindowManager->SetShowHiddenProperties(bShowAllButtons);
}

/** Event handler for toggling using script defined order */
void WxPropertyWindowHost::MenuPropToggleUseScriptDefinedOrder(wxCommandEvent &In)
{
	UBOOL bUseScriptDefinedOrder = In.IsChecked() ? TRUE : FALSE;
	GPropertyWindowManager->SetUseScriptDefinedOrder(bUseScriptDefinedOrder);
}

/** Event handler for toggling horizontal dividers */
void WxPropertyWindowHost::MenuPropToggleShowHorizontalDividers(wxCommandEvent &In)
{
	UBOOL bShowHorizontalDividers = In.IsChecked() ? TRUE : FALSE;
	GPropertyWindowManager->SetShowHorizontalDividers(bShowHorizontalDividers);

	// save the state to the editor ini
	GConfig->SetBool(TEXT("PropertyWindow"), TEXT("ShowHorizontalDividers"), bShowHorizontalDividers, GEditorUserSettingsIni);
}

/** Event handler for toggling whether or not to display nicer looking property names or the defaults. */
void WxPropertyWindowHost::MenuPropWinToggleShowFriendlyPropertyNames(wxCommandEvent &In)
{
	UBOOL bShowFriendlyPropertyNames = In.IsChecked() ? TRUE : FALSE;
	GPropertyWindowManager->SetShowFriendlyNames(bShowFriendlyPropertyNames);
}

/** Event handler for toggling whether or not all property windows should only display modified properties. */
void WxPropertyWindowHost::MenuPropWinToggleShowModifiedProperties(wxCommandEvent &In)
{
	UBOOL bShowOnlyModified = In.IsChecked() ? TRUE : FALSE;
	check(PropertyWindow);
	PropertyWindow->SetFlags(EPropertyWindowFlags::ShowOnlyModifiedItems, bShowOnlyModified);
	PropertyWindow->FilterWindows();
}

/** Event handler for toggling whether or not all property windows should only display differing properties. */
void WxPropertyWindowHost::MenuPropWinToggleShowDifferingProperties(wxCommandEvent &In)
{
	UBOOL bShowOnlyDiffering = In.IsChecked() ? TRUE : FALSE;
	check(PropertyWindow);
	PropertyWindow->SetFlags(EPropertyWindowFlags::ShowOnlyDifferingItems, bShowOnlyDiffering);
	PropertyWindow->FilterWindows();
}

/** Updates the UI for the menu item allowing . */
void WxPropertyWindowHost::UI_ToggleDifferingUpdate( wxUpdateUIEvent& In )
{
	In.Check( HasFlags(EPropertyWindowFlags::ShowOnlyDifferingItems) ? true : false );
	In.Enable( (GetNumObjects() > 1) ? true : false );
}


/** Event handler for scrolling down a property window*/
void WxPropertyWindowHost::OnMouseWheel( wxMouseEvent& In )
{
	check(PropertyWindow);
	PropertyWindow->OnMouseWheel(In);
}

/**Event handler for resizing of property window host*/
void WxPropertyWindowHost::OnSize( wxSizeEvent& In )
{
	if (PropertyWindow && PropertyFavoritesWindow)
	{
		AdjustFavoritesSplitter();
	}
}

void WxPropertyWindowHost::OnSaveObject(wxCommandEvent& In)
{
	// the button's supposed to be disabled if the conditions are incorrect but it doesn't hurt to be sure
	if (!GIsEditor && GIsGame && GetNumObjects() == 1)
	{
		UObject* TheObject = *ObjectIterator();
		if (TheObject->HasAllFlags(RF_ArchetypeObject | RF_Standalone) && !TheObject->HasAnyFlags(RF_ClassDefaultObject))
		{
			UPackage* Package = TheObject->GetOutermost();
			FGuid PackageGuid = Package->GetGuid();
			FString Filename;
			// find the on disk filename corresponding to this package
			if (GPackageFileCache->FindPackageFile(*Package->GetName(), &PackageGuid, Filename, UObject::GetLanguage()))
			{
				UObject::FlushAsyncLoading();
				UObject::ResetLoaders(TheObject);

				// To maximize safety and avoid cloberring editoronly data that wouldn't have been loaded, we need to convolute this a bit
				// First, reload the file into a different package with editoronly data enabled, so we have a clean slate
				// Copy modified properties from the edited object (and any subobjects) to the version(s) in the new package
				// Fix up object references that were copied; if it's an object that exists in the original, redirect it - if not, duplicate it
				// Save the new package as the original filename
				DWORD OldHackFlags = GUglyHackFlags;

				GUglyHackFlags |= HACK_ForceLoadEditorOnly;
				UPackage* NewPackage = UObject::CreatePackage(NULL, *FString::Printf(TEXT("TMP__%s"), *Package->GetName()));
				UObject::LoadPackage(NewPackage, *Filename, LOAD_None);
				UObject::ResetLoaders(NewPackage);
				GUglyHackFlags = OldHackFlags;

				GUglyHackFlags |= HACK_DisableSubobjectInstancing;
				TMap<UObject*, UObject*> ReplacementMap;
				UBOOL bIsError = FALSE;
				UObject* TheNewObject = NULL;
				for (FObjectIterator It; It; ++It)
				{
					if (*It == TheObject || It->IsIn(TheObject))
					{
						FString Path;
						for (UObject* Obj = *It; Obj != Package; Obj = Obj->GetOuter())
						{
							Path = FString::Printf(TEXT(".%s%s"), *Obj->GetName(), *Path);
						}
						Path = FString::Printf(TEXT("%s%s"), *NewPackage->GetName(), *Path);
						UObject* FoundObj = UObject::StaticFindObject(UObject::StaticClass(), NULL, *Path, FALSE);
						if (FoundObj != NULL)
						{
							if (*It == TheObject)
							{
								TheNewObject = FoundObj;
							}
							ReplacementMap.Set(*It, FoundObj);
							for (UProperty* Prop = It->GetClass()->PropertyLink; Prop != NULL; Prop = Prop->PropertyLinkNext)
							{
								// copy only editable properties
								if ((Prop->PropertyFlags & CPF_Edit) && !(Prop->PropertyFlags & CPF_EditorOnly))
								{
									Prop->CopyCompleteValue((BYTE*)FoundObj + Prop->Offset, (BYTE*)*It + Prop->Offset);
								}
							}
						}
						else if (*It != TheObject)
						{
							// need to duplicate the old object
							Path = Path.Left(Path.InStr(TEXT("."), TRUE));
							UObject* DupOuter = UObject::StaticFindObject(UObject::StaticClass(), NULL, *Path, FALSE);
							FoundObj = UObject::StaticDuplicateObject(*It, *It, DupOuter, *It->GetName());
							ReplacementMap.Set(*It, FoundObj);
						}
						else
						{
							bIsError = TRUE;
						}
					}
				}
				GUglyHackFlags = OldHackFlags;

				if (!bIsError)
				{
					FArchiveReplaceObjectRef<UObject> ReplaceAr(TheNewObject, ReplacementMap, FALSE, FALSE, FALSE);
					for (FObjectIterator It; It; ++It)
					{
						UObject* Obj = *It;
						if (It->IsIn(NewPackage))
						{
							ReplaceAr << Obj;
						}
					}

#if HAVE_SCC
					if (GFileManager->IsReadOnly( *Filename) )
					{
						if (GWarn->YesNof(LocalizeSecure(LocalizeQuery(TEXT("CheckoutPerforce"),TEXT("Core")), *Filename)))
						{
							// source control object
							FSourceControl::Init();

							// make a fully qualified filename
							FString Dir = GFileManager->GetCurrentDirectory();
							FString File = Filename;

							// Attempt to check out the file
							TArray<FString> FileNames;
							FileNames.AddItem(appConvertRelativePathToFull(*(Dir + File)));
							FSourceControl::CheckOut(NULL, FileNames);

							FSourceControl::Close();
						}
					}
#endif

					UObject::SavePackage(NewPackage, NULL, RF_Standalone, *Filename, GWarn);
				}

				UObject::CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, TRUE);

				if (!bIsError)
				{
					// load up the new file's header so Package has its GUID updated
					// this is necessary for the file lookup above to continue working for subsequent saves
					UObject::BeginLoad();
					UObject::GetPackageLinker(Package, *Filename, LOAD_None, NULL, NULL);
					UObject::EndLoad();
					UObject::ResetLoaders(Package);
				}
			}
		}
	}
}

/**
 * Allows child windows to be appended to 
 */
void WxPropertyWindowHost::RebuildFocusWindows (void)
{
	FocusArray.Empty();

	if (PropertyWindow && PropertyFavoritesWindow)
	{
		check(PropertyRequiredToolWindow);
		PropertyRequiredToolWindow->AppendFocusWindows(OUT FocusArray);

		check (PropertyFavoritesWindow);
		PropertyFavoritesWindow->AppendFocusWindows(OUT FocusArray);

		check (PropertyWindow);
		PropertyWindow->AppendFocusWindows(OUT FocusArray);
	}

	//ensure that all focus windows are also visible
	for (int i = ActiveFocusWindows.Num()-1; i >= 0; --i)
	{
		WxPropertyControl* ActiveWindow = ActiveFocusWindows(i);
		if (!FocusArray.ContainsItem(ActiveWindow))
		{
			ActiveFocusWindows.Remove(i);
		}
	}
}

/**
 * Moves focus to the item/category that next/prev to InItem.
 */
void WxPropertyWindowHost::MoveFocus (const EPropertyWindowDirections::Type InDirection, wxWindow* InWindow )
{
	if (FocusArray.Num())
	{
		INT FoundIndex;
		if (FocusArray.FindItem(InWindow, OUT FoundIndex))
		{
			FoundIndex += InDirection;
			if (FoundIndex >= FocusArray.Num())
			{
				FoundIndex = 0;
			}
			else if (FoundIndex < 0)
			{
				FoundIndex = FocusArray.Num()-1;
			}
			wxWindow* NewFocus = FocusArray(FoundIndex);
			check(NewFocus);
			NewFocus->SetFocus();
		} 
		else 
		{
			appMsgf(AMT_OK, TEXT("Item with Focus missing from array."));
		}
	}
}

/**
 * Gives focus directly to the search dialog
 */
void WxPropertyWindowHost::SetSearchFocus (void)
{
	if (PropertyRequiredToolWindow->IsShown())
	{
		TArray<wxWindow*> TempFocusArray;
		PropertyRequiredToolWindow->AppendFocusWindows(TempFocusArray);
		//should only have one entry
		if (TempFocusArray.Num() > 0)
		{
			TempFocusArray(0)->SetFocus();
		}
	}
}

/**Focus helper to help RequestMainWindowTakeFocus */
void WxPropertyWindowHost::SetFocusToOtherPropertyWindow (const WxPropertyWindow* InPropertyWindow)
{
	if (PropertyWindow && PropertyFavoritesWindow)
	{
		if (PropertyWindow == InPropertyWindow)
		{
			PropertyFavoritesWindow->SetFocus();
		}
		else
		{
			PropertyWindow->SetFocus();
		}
	}
	else
	{
		PropertyRequiredToolWindow->SetFocus();
	}
}

/**
 * Toggles a windows inclusion into the active focus array
 * @param InWindow - Window to toggle
 */
void WxPropertyWindowHost::SetActiveFocus(WxPropertyControl* InWindow, const UBOOL bOnOff)
{
	if (bOnOff)
	{
		ActiveFocusWindows.AddUniqueItem(InWindow);
	}
	else
	{
		ActiveFocusWindows.RemoveItem(InWindow);
	}
	GCallbackEvent->Send( CALLBACK_PropertySelectionChange );
}

/**
 * Returns the property node in the focus list at the given index
 * @param InIndex - Index of the focus window
 * @return - PropertyNode at the given index
 */
FPropertyNode* WxPropertyWindowHost::GetFocusNode(const INT InIndex)
{
	check(IsWithin<INT>(InIndex, 0, ActiveFocusWindows.Num()));
	check(ActiveFocusWindows(InIndex));
	return ActiveFocusWindows(InIndex)->GetPropertyNode();
}

/**
 * Copies all property names and values to the clipboard for pasting later
 * @return - returns the buffer of all the property value pairs
 */
FString WxPropertyWindowHost::CopyActiveFocusPropertiesToClipboard (void)
{
	FString PropertyBuffer;
	//path to each variable
	FString Path;
	//value as a string
	FString Value;

	for (INT i = 0; i < ActiveFocusWindows.Num(); ++i)
	{
		WxPropertyControl* NodeWindow = ActiveFocusWindows(i);
		FPropertyNode* PropertyNode = NodeWindow->GetPropertyNode();

		//get the fully qualified name of this node, including array index
		Path.Empty(256);
		const UBOOL bWithArrayIndex = TRUE;
		PropertyNode->GetQualifedName(Path, bWithArrayIndex);

		//Get current value as a string
		Value = NodeWindow->GetPropertyText();

		//append current path/value pair to the final buffer
		PropertyBuffer += Path;
		PropertyBuffer += TEXT(" ");
		PropertyBuffer += Value;
		PropertyBuffer += TEXT("\r\n");
	}
	return PropertyBuffer;
}

UBOOL WxPropertyWindowHost::IsPropertySelected( const FString& InName, const INT InArrayIndex )
{
	return IsPropertyOrChildrenSelected( InName, InArrayIndex, FALSE );
}

UBOOL WxPropertyWindowHost::IsPropertyOrChildrenSelected( const FString& InName, const INT InArrayIndex, UBOOL CheckChildren )
{
	for( INT Count = 0; Count < ActiveFocusWindows.Num(); Count++ )
	{
		WxPropertyControl* PropControl = ActiveFocusWindows( Count );
		FPropertyNode* PropNode = PropControl->GetPropertyNode();

		do
		{
			UBOOL bMatch = TRUE;

			UProperty *Prop = PropNode->GetProperty();
			INT Index = PropNode->GetArrayIndex();
			if( Prop )
			{
				FString Name = Prop->GetName();
				if( Index >= 0 )
				{
					FPropertyNode* ParentPropNode = PropNode->GetParentNode();
					if( ParentPropNode )
					{
						UProperty* ParentProp = ParentPropNode->GetProperty();
						if( ParentProp )
						{
							Name = ParentProp->GetName();
						}
					}
				}
				if( Name != InName )
				{
					bMatch = FALSE;
				}
			}
			else
			{
				bMatch = FALSE;
			}

			if( Index != InArrayIndex )
			{
				bMatch = FALSE;
			}

			if( bMatch == TRUE )
			{
				if( PropNode->GetParentNode()->HasNodeFlags( EPropertyNodeFlags::Expanded ) )
				{
					return TRUE;
				}
			}

			PropNode = PropNode->GetParentNode();
		}
		while( CheckChildren && ( PropNode != NULL ) );
	}
	return FALSE;
}

/**
 * Returns TRUE if the "NewObjects" match the current objects exactly
 */
UBOOL WxPropertyWindowHost::MatchesExistingObjects(const TArray<UObject*>& InTestObjects)
{
	if (InTestObjects.Num() != GetNumObjects())
	{
		return FALSE;
	}
	else
	{
		//sizes match, but make sure the are the same objects
		for( TPropObjectIterator Itor( ObjectIterator() ) ; Itor ; ++Itor )
		{
			UObject* CurrentObject = *Itor;
			if (!InTestObjects.ContainsItem(CurrentObject))
			{
				return FALSE;
			}
		}
	}
	//objects are a perfect match
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
//
// Property window frame.
//
///////////////////////////////////////////////////////////////////////////////

/*-----------------------------------------------------------------------------
	WxPropertyWindowFrame
-----------------------------------------------------------------------------*/

IMPLEMENT_DYNAMIC_CLASS(WxPropertyWindowFrame,wxFrame);

BEGIN_EVENT_TABLE( WxPropertyWindowFrame, wxFrame )
	EVT_SIZE( WxPropertyWindowFrame::OnSize )
	EVT_CLOSE( WxPropertyWindowFrame::OnClose )
	EVT_SET_FOCUS ( WxPropertyWindowFrame::OnSetFocus )
END_EVENT_TABLE()

/** 
 * Window creation function for the host (container of all feature windows)
 * @param Parent			- Parent window this is a child of
 * @param InID				- ID to assign to this window
 * @param InFeatureFlags	- Flags that determine which feature windows to use (filter, favorites, toolbar)
 * @param InNotifyHook		- Callback to use on destruction
 */
void WxPropertyWindowFrame::Create(wxWindow* Parent, wxWindowID id, FNotifyHook* InNotifyHook )
{
	const bool bWasCreateSuccessful = wxFrame::Create(Parent, id, TEXT("Properties"), wxDefaultPosition, wxDefaultSize, (Parent ? wxFRAME_FLOAT_ON_PARENT : wxDIALOG_NO_PARENT ) | wxDEFAULT_FRAME_STYLE | wxFRAME_TOOL_WINDOW | wxFRAME_NO_TASKBAR );
	check( bWasCreateSuccessful );

	RegisterCreation();

	// Closing is allowed if the frame is not parented.
	if ( Parent )
	{
		DisallowClose();
	}
	else
	{
		AllowClose();
	}

	wxBoxSizer* MainSizer = new wxBoxSizer(wxHORIZONTAL);

	// Allocated a window for this frame to contain.
	PropertyWindowHost = new WxPropertyWindowHost;
	check( PropertyWindowHost );

	PropertyWindowHost->Create( this, InNotifyHook, -1 );
	PropertyWindowHost->SetFlags(EPropertyWindowFlags::CanBeHiddenByPropertyWindowManager, TRUE);

	MainSizer->Add( PropertyWindowHost, 1, wxGROW|wxALL, 0 );
	SetSizer(MainSizer);

	// Read the property window's position and size from ini.
	UBOOL bForceLoad = TRUE;
	LoadLayout(bForceLoad);
}

WxPropertyWindowFrame::~WxPropertyWindowFrame()
{
	SaveLayout();
}

void WxPropertyWindowFrame::OnClose( wxCloseEvent& In )
{
	// Apply any outstanding edit changes before we hide/close
	PropertyWindowHost->FlushLastFocused();

	// if closing is allowed,  this property frame will destroy itself.  Otherwise, it just hides.
	if( IsCloseAllowed() )
	{
		// Issue the destroy notification.
		PropertyWindowHost->NotifyDestroy();

		// Pass the close event on to the next handler.
		In.Skip();
	}
	else
	{
		// Closing isn't allowed, so just hide.  Notify the app that the close didn't happen.
		In.Veto();
		Hide();
	}
}

void WxPropertyWindowFrame::OnSize( wxSizeEvent& In )
{
	if ( !IsCreated() )
	{
		In.Skip();
		return;
	}

	check( PropertyWindowHost );

	// Pass the size event down to the handled property window.
	const wxPoint pt = GetClientAreaOrigin();
	wxRect rc = GetClientRect();
	rc.y -= pt.y;

	PropertyWindowHost->SetSize( rc );
}


// Updates the caption of the floating frame based on the objects being edited
void WxPropertyWindowFrame::UpdateTitle()
{
	FString Caption;

	if( !PropertyWindowHost->GetObjectBaseClass() )
	{
		Caption = LocalizeGeneral("PropNone",TEXT("XWindow"));
	}
	else if( (PropertyWindowHost->GetNumObjects() == 1) && !(HasFlags(EPropertyWindowFlags::UseTypeAsTitle)) )
	{
		// if the object is the default metaobject for a UClass, use the UClass's name instead
		const UObject* Object = *ObjectConstIterator();
		FString ObjectName = Object->GetName();
		if ( Object->GetClass()->GetDefaultObject() == Object )
			ObjectName = Object->GetClass()->GetName();

		Caption = FString::Printf( LocalizeSecure(LocalizeGeneral("PropSingle",TEXT("XWindow")), *ObjectName) );
	}
	else
	{
		Caption = FString::Printf( LocalizeSecure(LocalizeGeneral("PropMulti",TEXT("XWindow")), *PropertyWindowHost->GetObjectBaseClass()->GetName(), PropertyWindowHost->GetNumObjects()) );
	}

	SetTitle( *Caption );
}

/**
 * Loads layout based on what object is set.
 */
void WxPropertyWindowFrame::LoadLayout( const UBOOL bInForceLoad )
{
	if ( PropertyWindowHost->GetNumObjects() || bInForceLoad )
	{
		FString NewContextName = GetContextWindowName();

		if ( ( LastContextName != NewContextName ) || ( LastContextName.Len() == 0 ) || bInForceLoad )
		{
			// Load the position and size of the window from INI
			FWindowUtil::LoadPosSize( NewContextName, this, 64, 64, 350, 500 );

			// Iterate over all of the property windows registered to the property window manager, finding all of those that are housed within property window
			// frames. While a list of frames are maintained in UnrealEdEngine, they are only for actors and may not account for all possible floating
			// property frames, so this method is chosen instead. For each found property window in a frame, create a mapping of its HWND to the frame pointer
			// so that the Z-order of the property window can quickly be determined later if need be.
			TMap<HWND, WxPropertyWindowFrame*> HWNDToFrameMap;
			for ( TArray<WxPropertyWindow*>::TConstIterator PropWindowIter( GPropertyWindowManager->PropertyWindows ); PropWindowIter; ++PropWindowIter )
			{
				WxPropertyWindow* CurPropWindow = *PropWindowIter;
				if ( CurPropWindow->GetParentHostWindow() )
				{
					WxPropertyWindowFrame* CurParentFrame = wxDynamicCast( CurPropWindow->GetParentHostWindow()->GetParent(), WxPropertyWindowFrame );
					if ( CurParentFrame && CurParentFrame != this && CurParentFrame->IsShown() )
					{
						HWNDToFrameMap.Set( static_cast<HWND>( CurParentFrame->GetHWND() ), CurParentFrame );
					}
				}
			}

			// If other floating frames already exist, the window should be positioned near the old one with a slight offset. Iterate through the Z-order of
			// the windows until we find the top-most floating property frame.
			if ( HWNDToFrameMap.Num() > 0 )
			{
				HWND TopWindow = ::GetTopWindow( NULL );
				WxPropertyWindowFrame** HighestFrame = NULL;

				while ( TopWindow != NULL && HighestFrame == NULL )
				{
					HighestFrame = HWNDToFrameMap.Find( TopWindow );
					TopWindow = ::GetNextWindow( TopWindow, GW_HWNDNEXT );
				}

				// If none of the frames were found in the Z-order for some reason, just use
				// any of the frames in the map.
				if ( HighestFrame == NULL )
				{
					TMap<HWND, WxPropertyWindowFrame*>::TIterator MapIter( HWNDToFrameMap );
					HighestFrame = &( MapIter.Value() );
				}

				this->SetPosition( (*HighestFrame)->GetPosition() + wxPoint( 25, 25 ) );
			}

			LastContextName = NewContextName;

			//property frames should not default to full screen
			if (IsMaximized())
			{
				Maximize(false);
			}
		}
	}
}

/**
 * Saves layout based on what object is set.
 */
void WxPropertyWindowFrame::SaveLayout (void)
{
	check(PropertyWindowHost);
	//don't bother saving if there is nothing set in this window
	if (PropertyWindowHost->GetNumObjects())
	{
		//property frames should not default to full screen
		if (IsMaximized())
		{
			Maximize(false);
		}

		// Write the property window's position and size to ini.
		FWindowUtil::SavePosSize( GetContextWindowName(), this );
	}
}

/**
 * Returns the context window name for storing window setting PER type of property window requested
 */
FString WxPropertyWindowFrame::GetContextWindowName(void) const
{
	check(PropertyWindowHost);

	FString ContextName = PropertyWindowHost->GetContextName();

	return ContextName + TEXT("FloatingPropertyWindow");
}


///////////////////////////////////////////////////////////////////////////////
//
// Property window.
//
///////////////////////////////////////////////////////////////////////////////

/*-----------------------------------------------------------------------------
	WxPropertyWindow
-----------------------------------------------------------------------------*/

IMPLEMENT_DYNAMIC_CLASS(WxPropertyWindow,wxPanel);

BEGIN_EVENT_TABLE( WxPropertyWindow, wxWindow )
	EVT_SIZE( WxPropertyWindow::OnSize )
	EVT_SCROLL( WxPropertyWindow::OnScroll )
	EVT_MENU( wxID_REFRESH, WxPropertyWindow::OnRefresh )
	EVT_MOUSEWHEEL( WxPropertyWindow::OnMouseWheel )
	EVT_KEY_DOWN(WxPropertyWindow::OnKeyDown)
	EVT_IDLE(WxPropertyWindow::OnIdle)
END_EVENT_TABLE()

void WxPropertyWindow::Create( wxWindow* InParent, FNotifyHook* InNotifyHook )
{
	//register event for handling rebuild messages
	wxEvtHandler::Connect(wxID_ANY, ID_UI_PROPERTYWINDOW_REBUILD, wxCommandEventHandler(WxPropertyWindow::OnRebuild));
	wxEvtHandler::Connect(wxID_ANY, ID_TOGGLE_SHOW_HIDDEN_PROPERTIES, wxCommandEventHandler(WxPropertyWindow::OnSetShowHiddenProperties));

	//when maps change, Null out all property windows that 
	GCallbackEvent->Register(CALLBACK_PreLoadMap, this);

	//reset the filter to empty
	FilterStrings.Empty();

	const bool bWasCreationSuccessful = wxPanel::Create( InParent, -1, wxDefaultPosition, wxDefaultSize, wxCLIP_CHILDREN );
	check( bWasCreationSuccessful );

	RebuildLocked = 0;
	bDeferredRefresh = FALSE;
	SplitterPos = Clamp<INT>( CalculateSplitterPosFromProportion( WxPropertyWindowConstants::DefaultSplitterProportion ), 0, 400 );
	IdealSplitterPos = SplitterPos;
	CachedSplitterProportion = CalculateSplitterProportionFromPos( SplitterPos );
	LastFocused = NULL;
	ThumbPos = 0;
	NotifyHook = InNotifyHook;

	//not in the middle of committing any values
	PerformCallbackCount = 0;

	PropertyWindowFlags = EPropertyWindowFlags::NoFlags;		//we'll turn on the flags at the end

	RegisterCreation();

	// Create a vertical scrollbar for the property window.
	ScrollBar = new wxScrollBar( this, -1, wxDefaultPosition, wxDefaultSize, wxSB_VERTICAL );
	ScrollBar->SetSmallIncrement (PROP_DefaultItemHeight);

	//create the full hierarchy only this once
	WxObjectsPropertyControl* pwo = new WxObjectsPropertyControl;
	PropertyTreeRoot = new FObjectPropertyNode;
	PropertyTreeRoot->InitNode (pwo,
								NULL,				//FPropertyNode* InParentNode
								this,				//WxPropertyWindow* InTopLevelWindow
								NULL,				//UProperty* InProperty
								-1,					// INT InPropertyOffest
								INDEX_NONE);		// INT InArrayIdx

	SetFlags(EPropertyWindowFlags::AllowEnterKeyToApplyChanges | EPropertyWindowFlags::ShouldShowCategories, TRUE);
	StopDraggingSplitter();

	FilterWindows();

	GPropertyWindowManager->RegisterWindow( this );
}

WxPropertyWindow::~WxPropertyWindow()
{
#if WITH_MANAGED_CODE
	UnBindColorPickers(this);
#endif

	//make sure all "focus events" are processed before destroying, leaving them on the stack
	if (IsChildWindowOf(FindFocus(), this))
	{
		RequestMainWindowTakeFocus();
	}

	SaveExpandedItems();
	SaveSplitterProportion(PropertyTreeRoot->GetObjectBaseClass());

	GPropertyWindowManager->UnregisterWindow( this );

	check(PropertyTreeRoot);
	PropertyTreeRoot->RemoveAllObjects();
	FObjectPropertyNode* PropertyTreeRootToDelete = PropertyTreeRoot;
	//we're going to delete this node altogether.  Make sure no one tries to use it.
	PropertyTreeRoot = NULL;
	delete PropertyTreeRootToDelete;
}

/**
 * Something has made the tree change topology.  Rebuild the part of the tree that is no longer valid
 */
void WxPropertyWindow::RebuildSubTree(FPropertyNode* PropertyNode)
{
	WxPropertyControl* OldWindow = PropertyNode->GetNodeWindow();
	FPropertyNode* OldParentNode = PropertyNode->GetParentNode();
	UProperty* OldProperty = PropertyNode->GetProperty();
	INT OldPropertyOffset = PropertyNode->GetPropertyOffset();
	INT OldArrayIdx = PropertyNode->GetArrayIndex();

	//have to give up focus on the child window as it may get destroyed.
	if (IsChildWindowOf(FindFocus(), this))
	{
		RequestMainWindowTakeFocus();
	}

	//now rebuild
	SaveExpandedItems();
	
	//turn of expansion nodes and we'll solve for them again in a second
	PropertyNode->SetNodeFlags(EPropertyNodeFlags::CanBeExpanded | EPropertyNodeFlags::Expanded, FALSE);
	//Get the expansion flags again since we may have changed the number of children, etc.
	PropertyNode->InitExpansionFlags();
	//let children build normally
	PropertyNode->RebuildChildren();
	
	RestoreExpandedItems();

	//now that we've fixed the topology.  Rebuild the focus array, seen list, etc
	FilterWindows();

	//re-mark children as favorites
	MarkFavorites(PropertyNode);
}

/**
 * Requests that the window mark itself for data validity verification
 */
void WxPropertyWindow::RequestReconnectToData(void) 
{ 
	LastFocused = NULL;	//no more events coming through here.
	SetFlags (EPropertyWindowFlags::DataReconnectionRequested, TRUE); 
}

/**
 * Requests main window to take focus
 */
void WxPropertyWindow::RequestMainWindowTakeFocus (void)
{
	//give focus to the OTHER property window so the focus event will work (wx disregards focus events within the same tree)
	check(GetParentHostWindow());
	GetParentHostWindow()->SetFocusToOtherPropertyWindow(this);

	SetFocus();
	ClearLastFocused();
}
/*
 * Sets the filtering of the property window and rebuild the entire window
 * @param InFilterString - Can contain multiple space delimited sub-strings that must all be found in a property
 */
void WxPropertyWindow::SetFilterString(const FString& InFilterString)
{
	FilterStrings.Empty();
	//PARSE STRING
	FString ParseString = InFilterString;
	ParseString.Trim();
	ParseString.TrimTrailing();
	ParseString.ParseIntoArray(&FilterStrings, TEXT(" "), 1);

	FilterWindows();
}

/**
 * Forces a refresh of filtering for the hierarchy
 */
void WxPropertyWindow::FilterWindows(void)
{
	//apply filter we are actively using
	PropertyTreeRoot->FilterWindows(FilterStrings);

	RefreshEntireTree();
}

/**
 * Quickly ensure all the correct windows are visible
 */
void WxPropertyWindow::RefreshEntireTree(void)
{
	//trying to restore node state, we'll just refresh at the end
	if (HasFlags(EPropertyWindowFlags::IgnoreRefreshRequests))
	{
		return;
	}

	//guards around closing the window
	if (PropertyTreeRoot != NULL)
	{
		//ensure the cache has been cleared out
		GPropertyWindowManager->ClearPropertyWindowCachedData();

		//Stop refreshes
		Freeze();

		//based on filtering and visiblity, generate "seen" flag
		UBOOL bAllowChildVisibility = TRUE;
		if (HasFlags(EPropertyWindowFlags::Favorites))
		{
			PropertyTreeRoot->ProcessSeenFlagsForFavorites();
			PropertyTreeRoot->ProcessShowVisibleWindowsFavorites();
		}
		else
		{
			PropertyTreeRoot->ProcessSeenFlags(bAllowChildVisibility);
			//build windows that should be visible, but haven't been made yet
			PropertyTreeRoot->ProcessShowVisibleWindows();
		}


		//position children
		PositionChildren();

		//build windows that should be visible, but haven't been made yet
		PropertyTreeRoot->ProcessControlWindows();

		//find list of all visible windows
		VisibleWindows.Empty();
		PropertyTreeRoot->AppendVisibleWindows (OUT VisibleWindows);

		if (HasFlags(EPropertyWindowFlags::Favorites))
		{
			//sort windows by the favorites sort order
			Sort<USE_COMPARE_POINTER(WxPropertyControl,FavoriteSortOrderForWindows)>( &VisibleWindows(0), VisibleWindows.Num() );
		}

		//rebuild the array that allows focus toggling
		WxPropertyWindowHost* HostWindow = GetParentHostWindow();
		if (HostWindow)
		{
			HostWindow->RebuildFocusWindows();
		}

		//Restart refreshes
		Thaw();

		//draw again
		Refresh();

		//Cached data should no longer be needed
		GPropertyWindowManager->ClearPropertyWindowCachedData();
	}
}

/**
 * Calls refresh entire tree on any window that has been marked for deferred update
 */
void WxPropertyWindow::DeferredRefresh(void)
{
	if (bDeferredRefresh)
	{
		RefreshEntireTree();
		bDeferredRefresh = FALSE;
	}
}

/** Utility for finding if a particular object is being edited by this property window. */
UBOOL WxPropertyWindow::IsEditingObject(const UObject* InObject) const
{
	for( TObjectConstIterator Itor = ObjectConstIterator() ; Itor ; ++Itor )
	{
		UObject* Obj = *Itor;
		if(Obj == InObject)
		{
			return true;
		}
	}

	return false;
}

void WxPropertyWindow::OnSize( wxSizeEvent& In )
{
	if ( !IsCreated() || !IsReallyShown(this) )
	{
		In.Skip();
		return;
	}

	const wxRect rc = GetClientRect();
	const INT Width = wxSystemSettings::GetMetric( wxSYS_VSCROLL_X );

	//Root->SetSize( wxRect( rc.x, rc.y, rc.GetWidth()-Width, rc.GetHeight() ) );
	ScrollBar->SetSize( wxRect( rc.GetWidth()-Width, rc.y, Width, rc.GetHeight() ) );
	
	// Adjust the splitter to maintain the currently cached proportion; this is necessary
	// to maintain the user's specified proportion when property windows are inside other windows
	// (like the static mesh editor) that resize themselves after the property window has already had its settings loaded/restored
	IdealSplitterPos = CalculateSplitterPosFromProportion( CachedSplitterProportion );

	RefreshEntireTree();
}

// Updates the scrollbar values.
void WxPropertyWindow::UpdateScrollBar()
{
	const wxRect rc = GetClientRect();
	const INT iMax = Max(0, MaxH - rc.GetHeight());
	ThumbPos = Clamp( ThumbPos, 0, iMax );

	ScrollBar->SetScrollbar( ThumbPos, rc.GetHeight(), MaxH, rc.GetHeight() );
}

void WxPropertyWindow::OnRefresh( wxCommandEvent& In )
{
	PositionChildren();
}

void WxPropertyWindow::SetScrollBarPos(INT Position)
{
	ThumbPos = Position;

	PositionChildren();
	RefreshEntireTree();
	Update();	
}

INT WxPropertyWindow::GetScrollBarPos() { return ThumbPos; }
INT WxPropertyWindow::GetScrollBarMax() 
{
	const wxRect rc = GetClientRect(); 
	return Max(0, MaxH - rc.GetHeight()); 
}

void WxPropertyWindow::OnScroll( wxScrollEvent& In )
{
	SetScrollBarPos(In.GetPosition());
}

void WxPropertyWindow::OnMouseWheel( wxMouseEvent& In )
{
	ThumbPos -= (In.GetWheelRotation() / In.GetWheelDelta()) * PROP_DefaultItemHeight;
	PositionChildren();
	RefreshEntireTree();
	Update();
}


// Recursive minion of PositionChildren.
INT WxPropertyWindow::PositionChildStandard( WxPropertyControl* InItem, INT InX, INT InY )
{
	FPropertyNode* PropertyNode = InItem->GetPropertyNode();
	check(PropertyNode);

	INT TotalNodeHeight = 0;

	//deals with the root being closed down and not seen yet.
	if (PropertyNode->HasNodeFlags(EPropertyNodeFlags::IsSeen))
	{
		wxRect rc = GetClientRect();
		rc.width -= 2;	// additional space required for cursor, pad and splitter
		rc.width -= wxSystemSettings::GetMetric( wxSYS_VSCROLL_X );

		INT ChildHeight = PropertyNode->GetChildHeight();
		INT ChildSpacer = PropertyNode->GetChildSpacer();

		// Since the item's height can change depending on the input proxy and draw proxy and whether or not the item is focused, we need
		// to generate the item's height specifically for this resize.
		INT ItemHeight = 0;

		if(ChildHeight == PROP_GenerateHeight)
		{
			ItemHeight = InItem->GetPropertyHeight();
		}

		TotalNodeHeight = ChildHeight + ItemHeight;

		InX += PROP_Indent;

		if( ( wxDynamicCast( InItem, WxCategoryPropertyControl ) && InItem->GetParent()->GetParent() == NULL) ||
			  wxDynamicCast( InItem, WxObjectsPropertyControl ) )
		{
			InX -= PROP_Indent;
		}

		////////////////////////////
		// Set position/size of any children this window has.

		for( INT x = 0 ; x < PropertyNode->GetNumChildNodes(); ++x )
		{
			FPropertyNode* ChildTreeNode = PropertyNode->GetChildNode(x);
			//only position children that should be drawn.
			if (ChildTreeNode->HasNodeFlags(EPropertyNodeFlags::IsSeen))
			{
				WxPropertyControl* ChildWindow = ChildTreeNode->GetNodeWindow();

				const UBOOL bShow = IsItemShown( ChildWindow );

				const INT WkH = PositionChildStandard( ChildWindow, InX, TotalNodeHeight );
				if( bShow )
				{
					TotalNodeHeight += WkH;
				}

				//InItem->ChildItems(x)->Show( bShow > 0 );
			}
		}

		const UINT FullExpandedFlags = EPropertyNodeFlags::Expanded | EPropertyNodeFlags::CanBeExpanded;
		const UBOOL bIsSeenDueToFilter = PropertyNode->HasNodeFlags(EPropertyNodeFlags::IsSeenDueToFiltering);
		//if required to be there, or NOT required to be there and is requested.
		const UBOOL bFullExpanded = bIsSeenDueToFilter || (!bIsSeenDueToFilter && (PropertyNode->HasNodeFlags(FullExpandedFlags) == FullExpandedFlags));
		if (bFullExpanded || (PropertyNode->GetParentNode() == NULL))
		//if( InItem->bExpanded || InItem == Root )
		{
			TotalNodeHeight += ChildSpacer;
		}

		////////////////////////////
		// Set pos/size of this window.

		PropertyNode->SetIndentX(InX);
		InItem->SetSize( 0,InY, rc.GetWidth(),TotalNodeHeight );

		// Resize the input proxy on this item.
		rc = InItem->GetClientRect();
		rc.width -= 2;	// additional space required for cursor, splitter and pad
		InItem->InputProxy->ParentResized( InItem, wxRect( rc.x+SplitterPos,rc.y,rc.width-SplitterPos, InItem->GetPropertyHeight() ) );

		// Force redraw.
		InItem->Refresh();
	}

	return TotalNodeHeight;
}


void GatherFavorites(TArray<FPropertyNode*> &OutFavoriteNodes, FPropertyNode& InNode)
{
	// Recursively search through to find all favorite nodes
	if (InNode.HasNodeFlags(EPropertyNodeFlags::IsFavorite))
	{
		OutFavoriteNodes.AddItem(&InNode);
	}
	else
	{
		//if we should bother to continue recursing
		if (InNode.HasNodeFlags(EPropertyNodeFlags::IsSeenDueToChildFavorite))
		{
			for( INT x = 0 ; x < InNode.GetNumChildNodes(); ++x )
			{
				FPropertyNode* ChildTreeNode = InNode.GetChildNode(x);
				GatherFavorites(OutFavoriteNodes, *ChildTreeNode);
			}
		}
	}
}

void GatherAndSortFavorites(TArray<FPropertyNode*> &OutFavoriteNodes, FPropertyNode& InNode)
{
	// Gather...
	GatherFavorites(OutFavoriteNodes, InNode);

	// And sort!
	Sort<USE_COMPARE_POINTER(FPropertyNode,FavoriteSortOrder)>( &OutFavoriteNodes(0), OutFavoriteNodes.Num() );

	//re-index the current favorites to compact the valid indices
	for (INT i = 0; i < OutFavoriteNodes.Num(); ++i)
	{
		OutFavoriteNodes(i)->SetFavoriteIndex(i);
	}
}

/**
 * Recursive minion of PositionChildren for favorites.
 *
 * @param	InX		The horizontal position of the child item.
 * @param	InY		The vertical position of the child item.
 * @return			An offset to the current Y for the next position.
 */
INT WxPropertyWindow::PositionChildFavorites( WxPropertyControl* InItem, INT InX, INT InY)
{
	check(InItem);
	FPropertyNode* PropertyNode = InItem->GetPropertyNode();
	check(PropertyNode);

	INT TotalNodeHeight = 0;
	
	// Gather sorted favorite nodes and set them up to draw them in that order.
	TArray<FPropertyNode*> FavoriteNodes;
	GatherAndSortFavorites(FavoriteNodes, *PropertyNode);
	
	for(INT i=0; i<FavoriteNodes.Num(); ++i)
	{
		TotalNodeHeight += PositionChildStandard(FavoriteNodes(i)->GetNodeWindow(), InX, InY + TotalNodeHeight);
	}

	return TotalNodeHeight;
}

// Look up the tree to determine the hide/show status of InItem.
UBOOL WxPropertyWindow::IsItemShown( WxPropertyControl* InItem )
{
	check(InItem);
	FPropertyNode* PropertyNode = InItem->GetPropertyNode();
	check(PropertyNode);
	return PropertyNode->HasNodeFlags(EPropertyNodeFlags::IsSeen);
}

/**
 * Searches for nearest WxObjectsPropertyControl ancestor, returning NULL if none exists or InNode is NULL.
 * Different from the WxPropertyControl version in that an input of NULL is acceptable, and that
 * calling with an InNode of type WxObjectsPropertyControl returns the *enclosing* WxObjectsPropertyControl,
 * not the input itself.
 */
static FObjectPropertyNode* NotifyFindObjectItemParent(WxPropertyControl* InNode)
{
	FObjectPropertyNode* Result = NULL;
	FPropertyNode *Cur = InNode->GetPropertyNode();
	check (Cur);
	FPropertyNode *ParentNode = Cur->GetParentNode(); 
	if (ParentNode)
	{
		Result = ParentNode->FindObjectItemParent();
	}
	return Result;
}

/**
 * Builds an ordered list of nested UProperty objects from the specified property up through the
 * object tree, as specified by the property window hierarchy.
 * The list tail is the property that was modified.  The list head is the class member property within
 * the top-most property window object.
 */
FEditPropertyChain* WxPropertyWindow::BuildPropertyChain(WxPropertyControl* InItem, UProperty* PropertyThatChanged)
{
	FEditPropertyChain*			PropertyChain	= new FEditPropertyChain;
	WxPropertyControl*		Item			= InItem;
	FPropertyNode*			ItemNode		= InItem->GetPropertyNode();

	FObjectPropertyNode*	ObjectNode		= ItemNode->FindObjectItemParent();
	UProperty* MemberProperty					= PropertyThatChanged;
	do
	{
		if ( ItemNode == ObjectNode )
		{
			MemberProperty = PropertyChain->GetHead()->GetValue();
		}

		UProperty*	Property	= ItemNode->GetProperty();
		if ( Property )
		{
			// Skip over property window items that correspond to a single element in a static array,
			// or the inner property of another UProperty (e.g. UArrayProperty->Inner).
			if ( ItemNode->GetArrayIndex() == INDEX_NONE && Property->GetOwnerProperty() == Property )
			{
				PropertyChain->AddHead( Property );
			}
		}
		ItemNode = ItemNode->GetParentNode();
	} while( ItemNode != NULL );

	// If the modified property was a property of the object at the root of this property window, the member property will not have been set correctly
	if ( ItemNode == ObjectNode )
	{
		MemberProperty = PropertyChain->GetHead()->GetValue();
	}

	PropertyChain->SetActivePropertyNode( PropertyThatChanged );
	PropertyChain->SetActiveMemberPropertyNode( MemberProperty );

	return PropertyChain;
}

// Calls PreEditChange on Root's objects, and passes PropertyAboutToChange to NotifyHook's NotifyPreChange.
void WxPropertyWindow::NotifyPreChange(WxPropertyControl* InItem, UProperty* PropertyAboutToChange, UObject* Object)
{
	FEditPropertyChain*			PropertyChain		= BuildPropertyChain( InItem, PropertyAboutToChange );
	FPropertyNode*			ItemNode			= InItem->GetPropertyNode();
	FObjectPropertyNode*	ObjectNode			= ItemNode->FindObjectItemParent();
	UProperty*					CurProperty			= PropertyAboutToChange;

	// Begin a property edit transaction.
	if ( GEditor )
	{
		GEditor->BeginTransaction( *LocalizeUnrealEd("PropertyWindowEditProperties") );
	}

	// Call through to the property window's notify hook.
	if( NotifyHook )
	{
		if ( PropertyChain == NULL || PropertyChain->Num() == 0 )
		{
			NotifyHook->NotifyPreChange( InItem, PropertyAboutToChange );
		}
		else
		{
			NotifyHook->NotifyPreChange( Object, PropertyChain );
		}
	}

	// Call PreEditChange on the object chain.
	while ( TRUE )
	{
		for( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
		{
			if ( PropertyChain == NULL || PropertyChain->Num() == 0 )
			{
				(*Itor)->PreEditChange( CurProperty );
			}
			else
			{
				(*Itor)->PreEditChange( *PropertyChain );
			}
		}

		// Pass this property to the parent's PreEditChange call.
		CurProperty = ObjectNode->GetStoredProperty();
		FObjectPropertyNode* PreviousObjectNode = ObjectNode;

		// Traverse up a level in the nested object tree.
		ObjectNode = NotifyFindObjectItemParent( ObjectNode->GetNodeWindow() );
		if ( !ObjectNode )
		{
			// We've hit the root -- break.
			break;
		}
		else if ( PropertyChain != NULL && PropertyChain->Num() > 0 )
		{
			PropertyChain->SetActivePropertyNode( CurProperty->GetOwnerProperty() );
			for ( FPropertyNode* BaseItem = PreviousObjectNode; BaseItem && BaseItem != ObjectNode; BaseItem = BaseItem->GetParentNode())
			{
				UProperty* ItemProperty = BaseItem->Property;
				if ( ItemProperty == NULL )
				{
					// if this property item doesn't have a Property, skip it...it may be a category item or the virtual
					// item used as the root for an inline object
					continue;
				}

				// skip over property window items that correspond to a single element in a static array, or
				// the inner property of another UProperty (e.g. UArrayProperty->Inner)
				if ( BaseItem->ArrayIndex == INDEX_NONE && ItemProperty->GetOwnerProperty() == ItemProperty )
				{
					PropertyChain->SetActiveMemberPropertyNode(ItemProperty);
				}
			}
		}
	}

	delete PropertyChain;
}

// Calls PostEditChange on Root's objects, and passes PropertyThatChanged to NotifyHook's NotifyPostChange.
void WxPropertyWindow::NotifyPostChange(WxPropertyControl* InItem, FPropertyChangedEvent& InPropertyChangedEvent)
{
	FEditPropertyChain*		PropertyChain		= BuildPropertyChain( InItem, InPropertyChangedEvent.Property);
	FPropertyNode*			ItemNode			= InItem->GetPropertyNode();
	FObjectPropertyNode*	ObjectNode			= ItemNode->FindObjectItemParent();
	UProperty*				CurProperty			= InPropertyChangedEvent.Property;

	// Fire CALLBACK_LevelDirtied when falling out of scope.
	FScopedLevelDirtied		LevelDirtyCallback;

	//for all subsequent callbacks, let them either rebuild or just repopulate based on the extent of the changes
	if (GPropertyWindowManager)
	{
		GPropertyWindowManager->bChangesTopology = InPropertyChangedEvent.bChangesTopology;
		GPropertyWindowManager->ChangeType = InPropertyChangedEvent.ChangeType;
	}

	// remember the property that was the chain's original active property; this will correspond to the outermost property of struct/array that was modified
	UProperty* OriginalActiveProperty = PropertyChain ? PropertyChain->GetActiveMemberNode()->GetValue() : NULL;

	// Call PostEditChange on the object chain.
	while ( TRUE )
	{
		INT CurrentObjectIndex = 0;
		for( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
		{
			if ( PropertyChain == NULL || PropertyChain->Num() == 0 )
			{
				//copy 
				FPropertyChangedEvent ChangedEvent = InPropertyChangedEvent;
				if (CurProperty != InPropertyChangedEvent.Property)
				{
					//parent object node property.  Reset other internals and leave the event type as unspecified
					ChangedEvent = FPropertyChangedEvent(CurProperty, InPropertyChangedEvent.bChangesTopology);
				}
				ChangedEvent.ObjectIteratorIndex = CurrentObjectIndex;
				(*Itor)->PostEditChangeProperty( ChangedEvent );
			}
			else
			{
				FPropertyChangedEvent ChangedEvent = InPropertyChangedEvent;
				if (CurProperty != InPropertyChangedEvent.Property)
				{
					//parent object node property.  Reset other internals and leave the event type as unspecified
					ChangedEvent = FPropertyChangedEvent(CurProperty, InPropertyChangedEvent.bChangesTopology);
				}
				FPropertyChangedChainEvent ChainEvent(*PropertyChain, ChangedEvent);
				ChainEvent.ObjectIteratorIndex = CurrentObjectIndex;
				(*Itor)->PostEditChangeChainProperty(ChainEvent);
			}
			LevelDirtyCallback.Request();
		}

		// Pass this property to the parent's PostEditChange call.
		CurProperty = ObjectNode->GetStoredProperty();
		FObjectPropertyNode* PreviousObjectNode = ObjectNode;

		// Traverse up a level in the nested object tree.
		ObjectNode = NotifyFindObjectItemParent( ObjectNode->GetNodeWindow() );
		if ( !ObjectNode )
		{
			// We've hit the root -- break.
			break;
		}
		else if ( PropertyChain != NULL && PropertyChain->Num() > 0 )
		{
			PropertyChain->SetActivePropertyNode(CurProperty->GetOwnerProperty());
			for ( FPropertyNode* BaseItem = PreviousObjectNode; BaseItem && BaseItem != ObjectNode; BaseItem = BaseItem->GetParentNode())
			{
				UProperty* ItemProperty = BaseItem->GetProperty();
				if ( ItemProperty == NULL )
				{
					// if this property item doesn't have a Property, skip it...it may be a category item or the virtual
					// item used as the root for an inline object
					continue;
				}

				// skip over property window items that correspond to a single element in a static array, or
				// the inner property of another UProperty (e.g. UArrayProperty->Inner)
				if ( BaseItem->GetArrayIndex() == INDEX_NONE && ItemProperty->GetOwnerProperty() == ItemProperty )
				{
					PropertyChain->SetActiveMemberPropertyNode(ItemProperty);
				}
			}
		}
	}
	//request windows we've just changed to update
	GCallbackEvent->Send(CALLBACK_DeferredRefreshPropertyWindows);


	// Call through to the property window's notify hook.
	if( NotifyHook )
	{
		if ( PropertyChain == NULL || PropertyChain->Num() == 0 )
		{
			NotifyHook->NotifyPostChange( InItem, InPropertyChangedEvent.Property );
		}
		else
		{
			PropertyChain->SetActiveMemberPropertyNode( OriginalActiveProperty );
			PropertyChain->SetActivePropertyNode( InPropertyChangedEvent.Property);
			NotifyHook->NotifyPostChange( InItem, PropertyChain );
		}
	}

	// End the property edit transaction.
	if ( GEditor )
	{
		GEditor->EndTransaction();
	}

	delete PropertyChain;

	bExecDeselectOnEscape = FALSE;

	//restore back to default settings
	if (GPropertyWindowManager)
	{
		GPropertyWindowManager->bChangesTopology = FALSE;
		GPropertyWindowManager->ChangeType = EPropertyChangeType::Unspecified;
	}

}

void WxPropertyWindow::NotifyDestroy()
{
	if( NotifyHook )
	{
		NotifyHook->NotifyDestroy( this );
	}
}



/**
 * Routes the event to the appropriate handlers
 *
 * @param InType the event that was fired
 * @param InFlags the flags for this event
 */
void WxPropertyWindow::Send(ECallbackEventType InType)
{
	switch( InType )
	{
		case CALLBACK_PreLoadMap:
			{
				for (wxWindow* ParentWindow = GetParent(); ParentWindow != NULL; ParentWindow = ParentWindow->GetParent())
				{
					//if a stand alone window
					if (wxDynamicCast(ParentWindow, WxPropertyWindowFrame))
					{
						ClearIfFromMapPackage();
					}
				}
			}
			break;
		default:
			break;
	}
}

/** 
 * For quitting PIE or between loadinng maps.  Objects from content packages do not need to be cleared. 
 */
void WxPropertyWindow::ClearIfFromMapPackage(void)
{
	if (GetNumObjects())
	{
		UObject* FirstObject = PropertyTreeRoot->GetObject(0);
		check(FirstObject);
		UPackage* ObjectPackage = FirstObject->GetOutermost();
		if (ObjectPackage && ObjectPackage->ContainsMap())
		{
			EPropertyWindowFlags::Type OldFlags = HasFlags( EPropertyWindowFlags::ShouldShowCategories | 
									EPropertyWindowFlags::Sorted | 
									EPropertyWindowFlags::ShouldShowNonEditable);
			SetObject(NULL, OldFlags);
			ClearLastFocused();
		}
	}
}

/** If true, favorites are displayed by the parent host window*/
UBOOL WxPropertyWindow::IsFavoritesFeatureEnabled (void) const
{
	const WxPropertyWindowHost* HostWindow = GetParentHostWindow();
	if (HostWindow)
	{
		return HostWindow->GetFavoritesVisible();
	}
	return FALSE;
}

void WxPropertyWindow::SetObject( UObject* InObject, const UINT InWindowFlags )
{
	//if this is happening DURING a callback to this property window bad things could happen.  Bail.
	if (GetActiveCallbackCount() > 0)
	{
		RequestReconnectToData();
		return;
	}

	// Don't allow property editing of the builder brush.
	// (during gameplay, there is no builder brush so we don't have to worry about it)
	AActor* Actor = Cast<AActor>(InObject);
	if ( (GWorld && GWorld->HasBegunPlay()) || Actor == NULL || !Actor->IsABuilderBrush() )
	{
		PreSetObject();
		if( InObject )
		{
			PropertyTreeRoot->AddObject( InObject );
		}

		// if the object's class has CollapseCategories, then don't show categories
		if (InObject && InObject->GetClass()->ClassFlags & CLASS_CollapseCategories)
		{
			SetFlags(EPropertyWindowFlags::ShouldShowCategories, FALSE);
		}

		if ( Actor != NULL && !Actor->IsTemplate() )
		{
			SetFlags(EPropertyWindowFlags::ReadOnly, FLevelUtils::IsLevelLocked(Actor->GetLevel()));
		}

		PostSetObject( InWindowFlags );
	}
}

// Relinquishes focus to LastFocused.
void WxPropertyWindow::FinalizeValues()
{
	if( LastFocused )
	{
		LastFocused->SetFocus();
	}
}

/**
 * Flushes the last focused item to the input proxy.
 */
void WxPropertyWindow::FlushLastFocused()
{
	if ( LastFocused && LastFocused->InputProxy )
	{
		LastFocused->InputProxy->SendToObjects( LastFocused );
	}
}


/**
 * Save off what nodes in the tree are expanded
 */
void WxPropertyWindow::SaveExpandedItems (void)
{
	// Remember expanded items.
	PropertyTreeRoot->SetNodeFlags(EPropertyNodeFlags::Expanded, TRUE);

	//don't save expanded items if this is delayed initialization
	if (PropertyTreeRoot->GetNumChildNodes()==0)
	{
		return;
	}

	TArray<FString> WkExpandedItems;
	PropertyTreeRoot->RememberExpandedItems( WkExpandedItems );

	check(GPropertyWindowManager);
	UClass* BestBaseClass = PropertyTreeRoot->GetObjectBaseClass();
	//while a valid class, and we're either the same as the base class (for multiple actors being selected and base class is AActor) OR we're not down to AActor yet)
	for( UClass* Class = BestBaseClass; Class && ((BestBaseClass == Class) || (Class!=AActor::StaticClass())); Class = Class->GetSuperClass() )
	{
		FString ExpansionName = Class->GetName();
		if (HasFlags(EPropertyWindowFlags::Favorites))
		{
			ExpansionName += TEXT("Favorites");
		}
		GPropertyWindowManager->SetExpandedItems (ExpansionName, WkExpandedItems);
	}
}

/**
 * Restore what nodes in the tree are expanded
 */
void WxPropertyWindow::RestoreExpandedItems (void)
{
	// Restore expanded items.
	TArray<FString> WkExpandedItems;
	UClass* BestBaseClass = PropertyTreeRoot->GetObjectBaseClass();
	//while a valid class, and we're either the same as the base class (for multiple actors being selected and base class is AActor) OR we're not down to AActor yet)
	for( UClass* Class = BestBaseClass; Class && ((BestBaseClass == Class) || (Class!=AActor::StaticClass())); Class = Class->GetSuperClass() )
	{
		FString ExpansionName = Class->GetName();
		if (HasFlags(EPropertyWindowFlags::Favorites))
		{
			ExpansionName += TEXT("Favorites");
		}
		GPropertyWindowManager->GetExpandedItems (ExpansionName, WkExpandedItems);

		PropertyTreeRoot->RestoreExpandedItems( WkExpandedItems );
	}

	const UBOOL bExpand   = TRUE;
	const UBOOL bRecurse  = FALSE;
	PropertyTreeRoot->SetExpanded(bExpand, bRecurse);
}


void WxPropertyWindow::PreSetObject()
{
	SaveExpandedItems();

	// Make sure that the contents of LastFocused get passed to its input proxy.
	if( LastFocused )
	{
		LastFocused->Refresh();
		FlushLastFocused();
		//only if the last focused window REALLY has focus, remove focus.  Do not in the case where some other window has taken focus first.
		ClearLastFocused();
	}

	//it is possible for wx to give focus to a child window without properly alerting the 
	if (IsChildWindowOf(FindFocus(), this))
	{
		RequestMainWindowTakeFocus();
		//SetFocus();	//should kill the last focus (stopping a future commit)
	}

	//Ensure this is purged
	LastFocused = NULL;

	// Remove all objects.
	SetFlags(EPropertyWindowFlags::IgnoreRefreshRequests, TRUE);
	PropertyTreeRoot->RemoveAllObjects();
	SetFlags(EPropertyWindowFlags::IgnoreRefreshRequests, FALSE);
}

void WxPropertyWindow::PostSetObject(const UINT InWindowFlags)
{
	UINT FlagsToTurnOff (EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories | EPropertyWindowFlags::ShouldShowNonEditable | EPropertyWindowFlags::UseTypeAsTitle | EPropertyWindowFlags::ReadOnly);
	SetFlags(FlagsToTurnOff, FALSE);

	//adjust window properties
	UINT NewFlags = InWindowFlags;
	if (GPropertyWindowManager->GetShowHiddenProperties())
	{
		NewFlags |= EPropertyWindowFlags::ShouldShowNonEditable;
	} 
	SetFlags(NewFlags, TRUE);

#if WITH_MANAGED_CODE
	//make sure no color picker is pointing at this data anymore
	UnBindColorPickers(this);
#endif

	UBOOL bIsSeen = IsReallyShown(this);
	//Shut the entire Tree down and rebuild from scratch
	//UI down and rebuild it
	check(PropertyTreeRoot);
	UClass* OldBestClass = PropertyTreeRoot->GetObjectBaseClass();
	WxObjectsPropertyControl* pwo = new WxObjectsPropertyControl;
	PropertyTreeRoot->InitNode (pwo,
		NULL,				//FPropertyNode* InParentNode
		this,				//WxPropertyWindow* InTopLevelWindow
		NULL,				//UProperty* InProperty
		-1,					// INT InPropertyOffest
		INDEX_NONE,			// INT InArrayIdx
		bIsSeen);			// Whether to allow children to initialize

	UClass* NewBestClass = PropertyTreeRoot->GetObjectBaseClass();
	if (NewBestClass != OldBestClass)
	{
		//force clear the filter string
		WxPropertyWindowHost* HostWindow = GetParentHostWindow();
		if (HostWindow)
		{
			HostWindow->ClearFilterString();
		}

		//Store old splitter position
		SaveSplitterProportion(OldBestClass);
		//Load new splitter position
		LoadSplitterProportion(NewBestClass);
	}

	if (bIsSeen)
	{
		SetFlags(EPropertyWindowFlags::IgnoreRefreshRequests, TRUE);

		RestoreExpandedItems();
		SetFlags(EPropertyWindowFlags::IgnoreRefreshRequests, FALSE);
	}
	else
	{
		//next time we are 'truly' visible, rebuild the entire tree
		RequestReconnectToData();
	}
	//force a refresh to deal with clean up of destroyed windows
	FilterWindows();
}

/**
 * Calculates the splitter location that would result from the provided proportion of the window being devoted
 * to showing the property title
 *
 * @param	RequestedProportion	Percentage amount of the window that should be devoted to displaying the property title [0.0f-1.0f]
 *
 * @return	The splitter position necessary to satisfy the requested proportion
 */
INT WxPropertyWindow::CalculateSplitterPosFromProportion( FLOAT RequestedProportion )
{
	INT SplitterPosFromProportion =  appRound( GetClientSize().GetWidth() * Clamp<FLOAT>( RequestedProportion, 0.0f, 1.0f ) );
	return SplitterPosFromProportion = Max(SplitterPosFromProportion, LEFT_SIDE_SPLITTER_MINIMUM);
}

/**
 * Calculates the proportion of the window devoted to showing the property title from the given splitter position
 *
 * @param	RequestedPos	Splitter position to calculate proportion for
 *
 * @return	Proportion of the window [0.0f-1.0f] which would be devoted to showing the property title with the given splitter position
 */
FLOAT WxPropertyWindow::CalculateSplitterProportionFromPos( INT RequestedPos )
{
	const FLOAT WindowWidth = GetClientSize().GetWidth();
	RequestedPos = Max(RequestedPos, LEFT_SIDE_SPLITTER_MINIMUM);
	//if the requested position is even within the window size (which does not happen with default sized windows which end up being 20,20)
	if (IsWithin<INT>(RequestedPos, 0, WindowWidth))
	{
		//return the proportion of the of the window width
		return Clamp<FLOAT>( ( WindowWidth > 0 ) ? RequestedPos / WindowWidth : 0.0f, 0.0f, 1.0f );
	}
	//just return the default.  This will either be correct because nothing will be loaded, OR the saved splitter position will be loaded later
	return WxPropertyWindowConstants::DefaultSplitterProportion;
}

/**Saves splitter proportion to proper context name*/
void WxPropertyWindow::SaveSplitterProportion(UClass* InClass)
{
	if (InClass) 
	{
		FString ContextName = GetContextName(InClass) + TEXT("SplitterPosProportion");
		if (HasFlags(EPropertyWindowFlags::Favorites))
		{
			ContextName += TEXT("Favorites");
		}
		GConfig->SetFloat(TEXT("PropertyWindow"), *ContextName, CalculateSplitterProportionFromPos( IdealSplitterPos ), GEditorUserSettingsIni);
	}
}

/**Loads splitter proportion to proper context name*/
void WxPropertyWindow::LoadSplitterProportion(UClass* InClass)
{
	if (InClass) 
	{
		// Set the splitter pos to the default proportion in case there isn't a saved value to load (clamping it to prevent the title from
		// being really far away from the value)
		IdealSplitterPos = Clamp<INT>( CalculateSplitterPosFromProportion( WxPropertyWindowConstants::DefaultSplitterProportion ), 0, 400 );
		
		// Now calculate the proportion, which could be different than the default in the event of a clamp
		CachedSplitterProportion = CalculateSplitterProportionFromPos( IdealSplitterPos );

		FLOAT SavedProportion = 0.0f;
		FString ContextName = GetContextName(InClass) + TEXT("SplitterPosProportion");
		if (HasFlags(EPropertyWindowFlags::Favorites))
		{
			ContextName += TEXT("Favorites");
		}
		if ( GConfig->GetFloat(TEXT("PropertyWindow"), *ContextName, SavedProportion, GEditorUserSettingsIni) )
		{
			IdealSplitterPos = CalculateSplitterPosFromProportion( SavedProportion );
			CachedSplitterProportion = SavedProportion; 
		}
	}
}


void WxPropertyWindow::Serialize( FArchive& Ar )
{
	// Serialize the root window, which will in turn serialize its children.
	PropertyTreeRoot->Serialize( Ar );
}

/**
 * Recursively searches through children for a property named PropertyName and expands it.
 * If it's a UArrayProperty, the propery's ArrayIndex'th item is also expanded.
 */
void WxPropertyWindow::ExpandItem( const FString& InPropertyName, INT InArrayIndex, const UBOOL bExpandParents )
{
	UBOOL bExpand = TRUE;
	PropertyTreeRoot->SetExpandedItem(InPropertyName, InArrayIndex, bExpand, bExpandParents);
}

/**
 * Expands all items in this property window.
 */
void WxPropertyWindow::ExpandAllItems()
{
	const UBOOL bExpand = TRUE;
	const UBOOL bRecurse = TRUE;
	PropertyTreeRoot->SetExpanded(bExpand, bRecurse);
}

FPropertyNode* WxPropertyWindow::FindPropertyNode(const FString& InPropertyName) const
{
	return PropertyTreeRoot->FindPropertyNode(InPropertyName);
}

/**
 * Recursively searches through children for a property named PropertyName and collapses it.
 * If it's a UArrayProperty, the propery's ArrayIndex'th item is also collapsed.
 */
void WxPropertyWindow::CollapseItem( const FString& InPropertyName, INT InArrayIndex)
{
	UBOOL bExpand = FALSE;
	PropertyTreeRoot->SetExpandedItem(InPropertyName, InArrayIndex, bExpand);
}

/**
 * Collapses all items in this property window.
 */
void WxPropertyWindow::CollapseAllItems()
{
	const UBOOL bExpand = FALSE;
	const UBOOL bRecurse = TRUE;
	PropertyTreeRoot->SetExpanded(bExpand, bRecurse);
}


/**
 * Returns if the property window is using the specified object in the root node of the hierarchy
 * @param InObject - The object to check for.  If NULL is passed in, this will also return true.
 * @return - TRUE if the object is contained within the root node
 */
UBOOL WxPropertyWindow::ContainsObject (const UObject* InObject) const
{
	UBOOL bContained = (InObject==NULL);
	
	if (InObject)
	{
		for (TPropObjectIterator It(PropertyTreeRoot->ObjectIterator()); It; ++It)
		{
			if (*It == InObject)
			{
				bContained = TRUE;
				break;
			}
		}
	}
	return bContained;
}

void WxPropertyWindow::RefreshVisibleWindows()
{
	//some simple value changed, just delete the controls and allow them to be recreated
	for (INT scan = 0; scan < VisibleWindows.Num(); scan++)
	{
		WxPropertyControl* NodeWindow = VisibleWindows(scan);
		check(NodeWindow);
		FPropertyNode* PropertyNode = NodeWindow->GetPropertyNode();
		check(PropertyNode);
		if (PropertyNode->HasNodeFlags(EPropertyNodeFlags::ChildControlsCreated))
		{
			check(NodeWindow->InputProxy);
			NodeWindow->InputProxy->DeleteControls();
			//reset flags so it will be recreated
			PropertyNode->SetNodeFlags(EPropertyNodeFlags::ChildControlsCreated, FALSE);
		}
	}
	//regenerate to create proper controls
	bDeferredRefresh = TRUE;
	//RefreshEntireTree();
}

/**
 * Rebuild all the properties and categories, with the same actors 
 *
 * @param IfContainingObject Only rebuild this property window if it contains the given object in the object hierarchy
 */
void WxPropertyWindow::Rebuild(UObject* IfContainingObject)
{
	// by default, rebuild this window, unless we pass in an object
	if((RebuildLocked == 0) && (!bDeferredRefresh))
	{
		UBOOL bRebuildWindow = ContainsObject(IfContainingObject);

		if (bRebuildWindow)
		{
			//have to give up focus on the child window as it may get destroyed.
			if (IsChildWindowOf(FindFocus(), this))
			{
				RequestMainWindowTakeFocus();
			}
			if (GPropertyWindowManager && GPropertyWindowManager->bChangesTopology)
			{
				//this request doing a reconnect at the next opportunity.  It may be too soon to reconnect at this point
				RequestReconnectToData();
			}
			else
			{
				RefreshVisibleWindows();
			}
		}
	}
}

/**
 * Queues a message in the property window message queue to Rebuild all the properties and categories, with the same actors.  
 *
 * @param IfContainingObject Only rebuild this property window if it contains the given object in the object hierarchy
 */
void WxPropertyWindow::PostRebuild(UObject* IfContainingObject)
{
	wxCommandEvent Event;
	Event.SetEventType(ID_UI_PROPERTYWINDOW_REBUILD);
	Event.SetEventObject(this);
	Event.SetClientData(IfContainingObject);
	GetEventHandler()->AddPendingEvent(Event);
}

/** 
 * Command handler for the rebuild event.
 *
 * @param	Event	Information about the event.
 */
void WxPropertyWindow::OnRebuild(wxCommandEvent &Event)
{
	Rebuild((UObject*)Event.GetClientData());
}

/**
 * Event handler for when a key is pressed.
 *
 * @param	In	Information about the event.
 */
void WxPropertyWindow::OnKeyDown(wxKeyEvent &In)
{
	if(In.GetKeyCode() == WXK_ESCAPE && bExecDeselectOnEscape)
	{
		GUnrealEd->Exec(TEXT("ACTOR DESELECT"));
	}
}

/**
 * Event handler for when a the window is idle.
 *
 * @param	In	Information about the event.
 */
void WxPropertyWindow::OnIdle(wxIdleEvent &In)
{
	//no need to go through the trouble when it's not visible
	if (IsReallyShown(this))
	{
		GPropertyWindowManager->ClearPropertyWindowCachedData();

		check(PropertyTreeRoot);
		//make sure no one has taken data out from under any of the windows.
		if (!PropertyTreeRoot->IsAllDataValid())
		{
			RequestReconnectToData();
		}
		//if the data has been compromised, reconnect now
		if (HasFlags(EPropertyWindowFlags::DataReconnectionRequested))
		{
			TArray<UObject*> ResetArray;
			for ( WxPropertyWindowFrame::TObjectIterator Itor( ObjectIterator() ) ; Itor ; ++Itor )
			{
				ResetArray.AddItem(*Itor);
			}

			EPropertyWindowFlags::Type OldFlags = HasFlags( EPropertyWindowFlags::ShouldShowCategories | 
														EPropertyWindowFlags::Sorted | 
														EPropertyWindowFlags::ShouldShowNonEditable);
			SetObjectArray(ResetArray, OldFlags);

			WxPropertyWindowHost* HostWindow = GetParentHostWindow();
			if (HostWindow)
			{
				HostWindow->ApplyFavorites();
			}

			SetFlags (EPropertyWindowFlags::DataReconnectionRequested, FALSE);
			//no need to refresh anymore
			bDeferredRefresh = FALSE;
		}

		//save last position for comparison
		const INT OldSplitterPos = SplitterPos;
		//assume we're going to the desired position
		SplitterPos = IdealSplitterPos;
		const INT LeftMargin = 20;
		const INT RightMargin = GetClientSize().GetWidth() - 40;
		//Move from the right side first.  We should always see a title (done 2nd)
		SplitterPos = Min(RightMargin, SplitterPos);
		//Now make sure you can see a portion of the label
		SplitterPos = Max(LeftMargin, SplitterPos);
		//if the splitter changed OR we still needed to refresh from a Rebuild, force a refresh
		if ((OldSplitterPos != SplitterPos) || (bDeferredRefresh))
		{
			CachedSplitterProportion = CalculateSplitterProportionFromPos( SplitterPos );
			RefreshEntireTree();
			//no need to refresh anymore
			bDeferredRefresh = FALSE;
		}
	}
}

void WxPropertyWindow::SetFlags (const UINT InFlags, const UBOOL InOnOff)
{
	if (InOnOff)
	{
		PropertyWindowFlags |= InFlags;
	}
	else
	{
		PropertyWindowFlags &= (~InFlags);
	}
}

/**
 * Freezes rebuild requests, so we do not process them.  
 */
void WxPropertyWindow::FreezeRebuild()
{
	RebuildLocked++;
}

/**
 * Resumes rebuild request processing.
 *
 * @param bRebuildNow	Whether or not to rebuild the window right now if rebuilding is unlocked.
 */
void WxPropertyWindow::ThawRebuild(UBOOL bRebuildNow)
{
	RebuildLocked--;

	if(RebuildLocked==0 && bRebuildNow)
	{
		Rebuild(NULL);
	}
}

/**
 * Event handler for showing/hiding all of the property item buttons.
 */
void WxPropertyWindow::OnSetShowHiddenProperties(wxCommandEvent &Event)
{
	if (GPropertyWindowManager->GetShowHiddenProperties())
	{
		//force turn on the hidden properties flag for this window
		PropertyWindowFlags |= EPropertyWindowFlags::ShouldShowNonEditable;
	}
	else
	{
		//force flag back off for this window
		PropertyWindowFlags &= ~(EPropertyWindowFlags::ShouldShowNonEditable);
	}
	//must rebuild the tree as nodes may have been created/destroyed
	RequestReconnectToData();
}

/** Rebuild the focus array to go through all open children */
void WxPropertyWindow::AppendFocusWindows (OUT TArray<wxWindow*>& OutFocusArray)
{
	INT FirstNewIndex = OutFocusArray.Num();
	OutFocusArray.AddZeroed(VisibleWindows.Num());
	for (INT i = 0; i < VisibleWindows.Num(); ++i)
	{
		OutFocusArray(i + FirstNewIndex) = VisibleWindows(i);
	}
}

/**
* Used to hide expansion widgets and disallow expansion/collapsing when using a forced filter
*/
UBOOL WxPropertyWindow::IsNormalExpansionAllowed (const FPropertyNode* InNode)
{
	//if no filter OR (it's allowed to be expanded)
	UBOOL bExpandableDueToFiltering = InNode->HasNodeFlags(EPropertyNodeFlags::IsSeenDueToFiltering | EPropertyNodeFlags::IsParentSeenDueToFiltering);
	UBOOL bSuppressExpansionDueToChildFiltering = InNode->HasNodeFlags(EPropertyNodeFlags::IsSeenDueToChildFiltering);
	if ((FilterStrings.Num()==0) || (bExpandableDueToFiltering && !bSuppressExpansionDueToChildFiltering))
	{
		return TRUE;
	}
	return FALSE;
}

/**Helper Function to get the parent Property Host Window*/
WxPropertyWindowHost* WxPropertyWindow::GetParentHostWindow(void)
{
	for (wxWindow* TestParent = GetParent(); TestParent!= NULL; TestParent=TestParent->GetParent())
	{
		WxPropertyWindowHost* HostWindow = wxDynamicCast( TestParent, WxPropertyWindowHost );
		if (HostWindow != NULL)
		{
			return HostWindow;
		}
	}
	return NULL;
}
const WxPropertyWindowHost* WxPropertyWindow::GetParentHostWindow(void) const
{
	for (const wxWindow* TestParent = GetParent(); TestParent!= NULL; TestParent=TestParent->GetParent())
	{
		const WxPropertyWindowHost* HostWindow = wxDynamicCast( TestParent, WxPropertyWindowHost );
		if (HostWindow != NULL)
		{
			return HostWindow;
		}
	}
	return NULL;
}

/** 
 * Mark Favorites of child nodes.  This function takes a Node so that rebuilding sub trees can be faster
 * @param InRootOfFavoritesUpdate - The root for which to remark favorites.  NULL, means the entire tree.
 */
void WxPropertyWindow::MarkFavorites (FPropertyNode* InRootOfFavoritesUpdate)
{
	if (!InRootOfFavoritesUpdate)
	{
		InRootOfFavoritesUpdate = PropertyTreeRoot;
	}

	//get list of favorites from the ini file
	TMap<FString, INT> FavoritesMap;
	LoadFavorites(FavoritesMap);

	SetFlags(EPropertyWindowFlags::HasAnyFavoritesSet, FavoritesMap.Num() > 0);

	//Send that down recursively
	UBOOL bAnyChanged = MarkFavoritesInternal(InRootOfFavoritesUpdate, FavoritesMap);

	//Refresh when needed
	if (bAnyChanged)
	{
		if (HasFlags(EPropertyWindowFlags::Favorites))
		{
			//rebuild the entire tree if something has changed
			RefreshEntireTree();
			SaveFavorites(FavoritesMap);
		}
		else
		{
			//just refresh the 
			Refresh();
		}
	}
}

/**
 * Sets favorites for this class, then tells the host to refresh as needed
 * @param InNode - Node to be marked as a favorite
 * @param bInShouldBeFavorite - TRUE if this node should be marked as a favorite or removed from the list (if it exists in the list)
 */
void WxPropertyWindow::SetFavorite (FPropertyNode* InNode, const UBOOL bInShouldBeFavorite)
{
	FString NodeName;
	check(InNode);
	const UBOOL bWithArrayIndex = TRUE;
	InNode->GetQualifedName(NodeName, bWithArrayIndex);
	
	//Get old list of favorites for this class
	TMap<FString,INT> FavoritesMap;
	LoadFavorites(FavoritesMap);
	
	if(bInShouldBeFavorite)
	{	
		// Save using default favorite index. Will be modified when favorites are applied below.
		FavoritesMap.Set(NodeName, PROP_Default_Favorite_Index);
	}
	else
	{
		FavoritesMap.Remove(NodeName);
	}

	//set new list of favorites for this class
	SaveFavorites(FavoritesMap);

	//tell host that the favorites have changed
	WxPropertyWindowHost* HostWindow = GetParentHostWindow();
	if (HostWindow)
	{
		HostWindow->ApplyFavorites(TRUE);
	}
}

/**
 * Pastes property values from clipboard into currently open object
 */
void WxPropertyWindow::PastePropertyValuesFromClipboard(FPropertyNode* InNode, TArray<FString>& NameValuePairs)
{
	if (!InNode->IsEditConst())
	{
		FString Path;
		Path.Empty(256);

		//get the fully qualified name of this node WITH array indices
		const UBOOL bWithArrayIndex = TRUE;
		InNode->GetQualifedName(Path, bWithArrayIndex);

		INT NameIndex = NameValuePairs.FindItemIndex(Path);
		if ((NameIndex != INDEX_NONE) && (NameIndex < NameValuePairs.Num() - 1))
		{
			FString ValueToPaste = NameValuePairs(NameIndex+1);

			WxPropertyControl* NodeWindow = InNode->GetNodeWindow();
			//make sure we have a valid proxy and not a object/category node because we can't paste into those
			if (NodeWindow->InputProxy && (InNode->GetObjectNode()==NULL) && (InNode->GetCategoryNode()==NULL))
			{
				// Build up a list of objects to modify.
				FObjectPropertyNode* ObjectNode = InNode->FindObjectItemParent();
				TArray<FObjectBaseAddress> ObjectsToModify;
				for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
				{
					UObject*	Object = *Itor;
					BYTE*		Addr = InNode->GetValueBaseAddress( (BYTE*) Object );
					ObjectsToModify.AddItem( FObjectBaseAddress( Object, Addr ) );

					NotifyPreChange(NodeWindow, NodeWindow->GetProperty(), Object);
				}
				//now import the new values
				NodeWindow->InputProxy->ImportText( NodeWindow, ObjectsToModify, ValueToPaste);

				const UBOOL bChangesTopology = FALSE;
				FPropertyChangedEvent ChangeEvent(NodeWindow->GetProperty(), bChangesTopology);
				for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
				{
					UObject*	Object = *Itor;
					NotifyPostChange(NodeWindow, ChangeEvent);
				}
			}

			//add to the current selection
			GetParentHostWindow()->SetActiveFocus(NodeWindow, TRUE);
		}
	}

	//send to the property windows for pasting
	//recurse for all children
	for( INT x = 0 ; x < InNode->GetNumChildNodes(); ++x )
	{
		FPropertyNode* ChildTreeNode = InNode->GetChildNode(x);
		check(ChildTreeNode);
		PastePropertyValuesFromClipboard (ChildTreeNode, NameValuePairs);
	}
}


/** 
 * Mark Favorites of child nodes.
 * @param InNode - The root for which to remark favorites.  NULL, means the entire tree.
 * @param InFavoritesList - List of all favorites for this class
 * @return - TRUE if any settings have changed during this marking
 */
UBOOL WxPropertyWindow::MarkFavoritesInternal (FPropertyNode* InNode, TMap<FString,INT>& InFavoritesMap)
{
	UBOOL AnyChanged = FALSE;
	check(InNode);

	//allocate space for the path
	{
		FString Path;
		Path.Empty(256);

		//get the fully qualified name of this node
		const UBOOL bWithArrayIndex = TRUE;
		InNode->GetQualifedName(Path, bWithArrayIndex);

		//See if this should be marked as a favorite
		UBOOL bShouldBeFavorite = FALSE;
		for(TMap<FString, INT>::TIterator It(InFavoritesMap); It; ++It)
		{
			if(Path == It.Key())
			{
				if(HasFlags(EPropertyWindowFlags::Favorites))
				{
					// set favorite index if it hasn't been set already
					if(It.Value() != PROP_Default_Favorite_Index)
					{
						InNode->SetFavoriteIndex(It.Value());
					}
					else
					{
						InNode->SetFavoriteIndex(InFavoritesMap.Num());
						InFavoritesMap.Set(It.Key(), InNode->GetFavoriteIndex());
					}
				}
				bShouldBeFavorite = TRUE;
				break;
			}
		}

		//was this a favorite?
		UBOOL bWasFavorite = InNode->HasNodeFlags(EPropertyNodeFlags::IsFavorite) ? TRUE : FALSE;

		//if they are different mark that something has changed
		AnyChanged |= (bShouldBeFavorite != bWasFavorite);

		//set the new state
		InNode->SetNodeFlags(EPropertyNodeFlags::IsFavorite, bShouldBeFavorite);
	}

	//recurse for all children
	for( INT x = 0 ; x < InNode->GetNumChildNodes(); ++x )
	{
		FPropertyNode* ChildTreeNode = InNode->GetChildNode(x);
		check(ChildTreeNode);
		AnyChanged |= MarkFavoritesInternal (ChildTreeNode, InFavoritesMap);
	}

	return AnyChanged;
}

/**
 * Save Favorites to ini file
 * @param InFavoritesMap - Map of favorite nodes by qualified names and favorite indices
 */
void WxPropertyWindow::SaveFavorites(const TMap<FString,INT>& InFavoritesMap) const
{
	UClass* BestClass = PropertyTreeRoot->GetObjectBaseClass();
	FString ContextName = BestClass->GetName() + TEXT("Favorites");
	
	TArray<FString> InFavoritesList;
	for(TMap<FString, INT>::TConstIterator It( InFavoritesMap ); It; ++It)
	{
		InFavoritesList.AddItem( It.Key() );
		InFavoritesList.AddItem( appItoa(It.Value()) );
	}

	GConfig->SetSingleLineArray(TEXT("PropertyWindow"), *ContextName, InFavoritesList, GEditorUserSettingsIni);
}

/**
 * Load Favorites from ini file
 * @param OutFavoritesMap - Map of favorite nodes by qualified names and favorite indices
 */
void WxPropertyWindow::LoadFavorites(TMap<FString,INT>& OutFavoritesMap) const
{
	OutFavoritesMap.Reset();
	
	UClass* BestClass = PropertyTreeRoot->GetObjectBaseClass();
	FString ContextName = BestClass->GetName() + TEXT("Favorites");

	TArray<FString> OutFavoritesList;
	GConfig->GetSingleLineArray(TEXT("PropertyWindow"), *ContextName, OutFavoritesList, GEditorUserSettingsIni);
	UBOOL bUseFavoriteSortKey = FALSE;
	//if there are an even number of entries, there is at least one entry and that entry is numeric
	if (((OutFavoritesList.Num()&0x1) == 0) && (OutFavoritesList.Num()>1) && OutFavoritesList(1).IsNumeric())
	{
		//Use the new loading system
		bUseFavoriteSortKey = TRUE;
	}

	if(bUseFavoriteSortKey)
	{
		for(INT i=0; i<OutFavoritesList.Num(); i+=2)
		{
			FString LoadedFavoriteIndex = OutFavoritesList(i+1);
			if(LoadedFavoriteIndex.IsNumeric())
			{
				INT Test = appAtoi(*LoadedFavoriteIndex);
				OutFavoritesMap.Set(OutFavoritesList(i), Test );
			}
		}
	}
	else
	{
		//load the favorites the way they used to be
		for(INT i=0; i<OutFavoritesList.Num(); ++i)
		{
			OutFavoritesMap.Set(OutFavoritesList(i), i );
		}
	}
}

// Positions existing child items so they are in the proper positions, visible/hidden, etc.
void WxPropertyWindow::PositionChildren()
{
	const wxRect rc = GetClientRect();
	const INT iMax = Max(0, MaxH - rc.GetHeight());
	ThumbPos = Clamp( ThumbPos, 0, iMax );

	const INT OldThumbPos = ThumbPos;

	check(PropertyTreeRoot);
	if (HasFlags(EPropertyWindowFlags::Favorites))
	{
		MaxH = PositionChildFavorites( PropertyTreeRoot->GetNodeWindow(), PROP_Indent, -ThumbPos );
	}
	else
	{
		MaxH = PositionChildStandard( PropertyTreeRoot->GetNodeWindow(), 0, -ThumbPos );
	}

	UpdateScrollBar();

	// This check is for the special case when a category that is so large its header isn't visible is being collapsed.
	// If this check isn't performed then the entire property window will disappear.
	if(OldThumbPos != ThumbPos)
	{
		check(PropertyTreeRoot);
		if (HasFlags(EPropertyWindowFlags::Favorites))
		{
			MaxH = PositionChildFavorites( PropertyTreeRoot->GetNodeWindow(), PROP_Indent, -ThumbPos );
		}
		else
		{
			MaxH = PositionChildStandard( PropertyTreeRoot->GetNodeWindow(), 0, -ThumbPos );
		}
	}
}

/**
 * Begins at the Root node of this window and gathers all favorite
 * nodes parented to the window, searching for the nearest matching
 * FavoriteSortIndex in the given direction.
 * @param InDirection - Direction to search relative to the given index
 * @param PropertyNode - Relative node to search from.
 */
FPropertyNode* WxPropertyWindow::FindNearestFavoriteNode(const EPropertyWindowDirections::Type InDirection, FPropertyNode& InNode) const
{
	// Grab sorted favorite nodes
	TArray<FPropertyNode*> FavoriteNodes;
	GatherAndSortFavorites(FavoriteNodes, *PropertyTreeRoot);

	INT NodeIndex;
	if(FavoriteNodes.FindItem(&InNode, OUT NodeIndex))
	{
		INT DesiredIndex = NodeIndex + InDirection;
		// Ensure desired index is within range
		if(DesiredIndex < FavoriteNodes.Num() && DesiredIndex >= 0)
		{
			return FavoriteNodes(DesiredIndex);
		}
	}
	return NULL;
}

/**
 * Saves the given nodes for inclusion in favorites, including their respective FavoriteSortIndex.
 * @param InNode1 - Node to save
 * @param InNode2 - Node to save
 */
void WxPropertyWindow::SaveFavoriteNodes(const FPropertyNode& InNode1, const FPropertyNode& InNode2)
{
	FString Node1Name, Node2Name;
	const UBOOL bWithArrayIndex = TRUE;
	InNode1.GetQualifedName(Node1Name, bWithArrayIndex);
	InNode2.GetQualifedName(Node2Name, bWithArrayIndex);

	TMap<FString, INT> FavoriteMap;
	LoadFavorites(FavoriteMap);
	FavoriteMap.Set(Node1Name, InNode1.GetFavoriteIndex());
	FavoriteMap.Set(Node2Name, InNode2.GetFavoriteIndex());
	SaveFavorites(FavoriteMap);
}

/* ==========================================================================================================
	FPropertyItemValueDataTracker
========================================================================================================== */
/**
 * Constructor
 *
 * @param	InPropItem		the property window item this struct will hold values for
 * @param	InOwnerObject	the object which contains the property value
 */
FPropertyItemValueDataTracker::FPropertyItemValueDataTracker( WxItemPropertyControl* InPropItem, UObject* InOwnerObject )
: PropertyItem(InPropItem), PropertyOffset(INDEX_NONE)
{
	PropertyValueRoot.OwnerObject = InOwnerObject;
	check(PropertyItem);
	UProperty* Property = PropertyItem->GetProperty();
	check(Property);
	check(PropertyValueRoot.OwnerObject);


	FPropertyNode* PropertyNode = InPropItem->GetPropertyNode();
	FPropertyNode* ParentNode		= PropertyNode->GetParentNode();
	// if the object specified is a class object, transfer to the CDO instead
	if ( PropertyValueRoot.OwnerObject->GetClass() == UClass::StaticClass() )
	{
		PropertyValueRoot.OwnerObject = Cast<UClass>(PropertyValueRoot.OwnerObject)->GetDefaultObject();
	}

	UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
	UArrayProperty* OuterArrayProp = Cast<UArrayProperty>(Property->GetOuter());

	// calculate the values for the current object
	{
		PropertyValueBaseAddress = OuterArrayProp == NULL
			? PropertyNode->GetValueBaseAddress(PropertyValueRoot.ValueAddress)
			: ParentNode->GetValueBaseAddress(PropertyValueRoot.ValueAddress);

		PropertyValueAddress = PropertyNode->GetValueAddress(PropertyValueRoot.ValueAddress);
	}

	// calculate the values for the default object
	if ( HasDefaultValue(&PropertyOffset) )
	{
		PropertyDefaultValueRoot.OwnerObject = PropertyValueRoot.OwnerObject->GetArchetype();
		PropertyDefaultBaseAddress = OuterArrayProp == NULL
			? PropertyNode->GetValueBaseAddress(PropertyDefaultValueRoot.ValueAddress)
			: ParentNode->GetValueBaseAddress(PropertyDefaultValueRoot.ValueAddress);
		PropertyDefaultAddress = PropertyNode->GetValueAddress(PropertyDefaultValueRoot.ValueAddress);

		//////////////////////////
		// If this is an array property, we must take special measures; PropertyDefaultBaseAddress points to an FScriptArray*, while
		// PropertyDefaultAddress points to the FScriptArray's Data pointer.
		if ( ArrayProp != NULL )
		{
			PropertyValueAddress = PropertyValueBaseAddress;
			PropertyDefaultAddress = PropertyDefaultBaseAddress;
		}
	}
}

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
UBOOL FPropertyItemValueDataTracker::HasDefaultValue( INT* pMemberPropertyOffset/*=NULL*/ ) const
{
	check(PropertyValueBaseAddress);
	check(PropertyValueRoot.OwnerObject);

	UBOOL bResult = FALSE;

	UObject* ParentDefault = PropertyValueRoot.OwnerObject->GetArchetype();
	if (ParentDefault != NULL) // will be NULL if the owner object is the default object of class'Object'
	{
		// Find the offset for the member property which contains this item's property
		BYTE* PropertyOffsetAddress = PropertyValueBaseAddress;
		FPropertyNode* MemberPropertyNode = PropertyItem->GetPropertyNode();
		for ( ; MemberPropertyNode != NULL; MemberPropertyNode = MemberPropertyNode->GetParentNode() )
		{
			UProperty* MemberProperty = MemberPropertyNode->GetProperty();
			if ( MemberProperty != NULL )
			{
				if ( MemberProperty->GetOuter()->GetClass() == UClass::StaticClass() )
				{
					break;
				}
			}
		}
		if ( MemberPropertyNode != NULL )
		{
			PropertyOffsetAddress = MemberPropertyNode->GetValueBaseAddress(PropertyValueRoot.ValueAddress);
		}

		// can only reset to default if this property exists in the archetype
		const INT BaseOffset = PropertyOffsetAddress - PropertyValueRoot.ValueAddress;
		if (PropertyValueRoot.OwnerObject->GetClass() == ParentDefault->GetClass()
		||	BaseOffset < ParentDefault->GetClass()->GetPropertiesSize())
		{
			bResult = TRUE;
			if ( pMemberPropertyOffset != NULL )
			{
				*pMemberPropertyOffset = BaseOffset;
			}
		}
	}

	return bResult;
}

/**
 * @return	a pointer to the subobject root (outer-most non-subobject) of the owning object.
 */
UObject* FPropertyItemValueDataTracker::GetTopLevelObject()
{
	check(PropertyItem);
	FPropertyNode* PropertyNode = PropertyItem->GetPropertyNode();
	check(PropertyNode);
	FObjectPropertyNode* RootNode = PropertyNode->FindRootObjectItemParent();
	check(RootNode);

	TArray<UObject*> RootObjects;
	for ( TPropObjectIterator Itor( RootNode->ObjectIterator() ) ; Itor ; ++Itor )
	{
		RootObjects.AddItem(*Itor);
	}

	UObject* Result;
	for ( Result = PropertyValueRoot.OwnerObject; Result; Result = Result->GetOuter() )
	{
		if ( RootObjects.ContainsItem(Result) )
		{
			break;
		}
	}
	check(Result);
	return Result;
}

/* ==========================================================================================================
	FPropertyItemComponentCollector

	Given a property and the address for that property's data, searches for references to components and
	keeps a list of any that are found.
========================================================================================================== */
/** Constructor */
FPropertyItemComponentCollector::FPropertyItemComponentCollector( const FPropertyItemValueDataTracker& InValueTracker )
: ValueTracker(InValueTracker)
{
	check(ValueTracker.PropertyItem);
	FPropertyNode* PropertyNode = ValueTracker.PropertyItem->GetPropertyNode();
	check(PropertyNode);
	UProperty* Prop = PropertyNode->GetProperty();
	if ( PropertyNode->GetArrayIndex() == INDEX_NONE )
	{
		// either the associated property is not an array property, or it's the header for the property (meaning the entire array)
		for ( INT ArrayIndex = 0; ArrayIndex < Prop->ArrayDim; ArrayIndex++ )
		{
			ProcessProperty(Prop, ValueTracker.PropertyValueAddress + ArrayIndex * Prop->ElementSize);
		}
	}
	else
	{
		// single element of either a dynamic or static array
		ProcessProperty(Prop, ValueTracker.PropertyValueAddress);
	}
}

/**
 * Routes the processing to the appropriate method depending on the type of property.
 *
 * @param	Property				the property to process
 * @param	PropertyValueAddress	the address of the property's value
 */
void FPropertyItemComponentCollector::ProcessProperty( UProperty* Property, BYTE* PropertyValueAddress )
{
	if ( Property != NULL )
	{
		if ( ProcessObjectProperty(Cast<UObjectProperty>(Property), PropertyValueAddress) )
		{
			return;
		}
		if ( ProcessStructProperty(Cast<UStructProperty>(Property), PropertyValueAddress) )
		{
			return;
		}
		if ( ProcessInterfaceProperty(Cast<UInterfaceProperty>(Property), PropertyValueAddress) )
		{
			return;
		}
		if ( ProcessDelegateProperty(Cast<UDelegateProperty>(Property), PropertyValueAddress) )
		{
			return;
		}
		if ( ProcessArrayProperty(Cast<UArrayProperty>(Property), PropertyValueAddress) )
		{
			return;
		}
	}
}

/**
 * UArrayProperty version - invokes ProcessProperty on the array's Inner member for each element in the array.
 *
 * @param	ArrayProp				the property to process
 * @param	PropertyValueAddress	the address of the property's value
 *
 * @return	TRUE if the property was handled by this method
 */
UBOOL FPropertyItemComponentCollector::ProcessArrayProperty( UArrayProperty* ArrayProp, BYTE* PropertyValueAddress )
{
	UBOOL bResult = FALSE;

	if ( ArrayProp != NULL )
	{
		UPropertyValue PropValue;
		ArrayProp->GetPropertyValue(PropertyValueAddress, PropValue);

		BYTE* ArrayValue = (BYTE*)PropValue.ArrayValue->GetData();
		for ( INT ArrayIndex = 0; ArrayIndex < PropValue.ArrayValue->Num(); ArrayIndex++ )
		{
			ProcessProperty(ArrayProp->Inner, ArrayValue + ArrayIndex * ArrayProp->Inner->ElementSize);
		}

		bResult = TRUE;
	}

	return bResult;
}

/**
 * UStructProperty version - invokes ProcessProperty on each property in the struct
 *
 * @param	StructProp				the property to process
 * @param	PropertyValueAddress	the address of the property's value
 *
 * @return	TRUE if the property was handled by this method
 */
UBOOL FPropertyItemComponentCollector::ProcessStructProperty( UStructProperty* StructProp, BYTE* PropertyValueAddress )
{
	UBOOL bResult = FALSE;

	if ( StructProp != NULL )
	{
		for ( UProperty* Prop = StructProp->Struct->PropertyLink; Prop; Prop = Prop->PropertyLinkNext )
		{
			for ( INT ArrayIndex = 0; ArrayIndex < Prop->ArrayDim; ArrayIndex++ )
			{
				ProcessProperty(Prop, PropertyValueAddress + Prop->Offset + ArrayIndex * Prop->ElementSize);
			}
		}
		bResult = TRUE;
	}

	return bResult;
}

/**
 * UObjectProperty version - if the object located at the specified address is a UComponent, adds the component the list.
 *
 * @param	ObjectProp				the property to process
 * @param	PropertyValueAddress	the address of the property's value
 *
 * @return	TRUE if the property was handled by this method
 */
UBOOL FPropertyItemComponentCollector::ProcessObjectProperty( UObjectProperty* ObjectProp, BYTE* PropertyValueAddress )
{
	UBOOL bResult = FALSE;

	if ( ObjectProp != NULL )
	{
		UObject* ObjValue = *(UObject**)PropertyValueAddress;
		UComponent* Comp = Cast<UComponent>(ObjValue);

		if ( Comp != NULL )
		{
			Components.AddItem(Comp);
		}

		bResult = TRUE;
	}

	return bResult;
}

/**
 * UInterfaceProperty version - if the FScriptInterface located at the specified address contains a reference to a UComponent, add the component to the list.
 *
 * @param	InterfaceProp			the property to process
 * @param	PropertyValueAddress	the address of the property's value
 *
 * @return	TRUE if the property was handled by this method
 */
UBOOL FPropertyItemComponentCollector::ProcessInterfaceProperty( UInterfaceProperty* InterfaceProp, BYTE* PropertyValueAddress )
{
	UBOOL bResult = FALSE;

	if ( InterfaceProp != NULL )
	{
		UPropertyValue PropValue;
		InterfaceProp->GetPropertyValue(PropertyValueAddress, PropValue);

		UComponent* Comp = Cast<UComponent>(PropValue.InterfaceValue->GetObject());
		if ( Comp != NULL )
		{
			Components.AddItem(Comp);
		}
		bResult = TRUE;
	}

	return bResult;
}

/**
 * UDelegateProperty version - if the FScriptDelegate located at the specified address contains a reference to a UComponent, add the component to the list.
 *
 * @param	DelegateProp			the property to process
 * @param	PropertyValueAddress	the address of the property's value
 *
 * @return	TRUE if the property was handled by this method
 */
UBOOL FPropertyItemComponentCollector::ProcessDelegateProperty( UDelegateProperty* DelegateProp, BYTE* PropertyValueAddress )
{
	UBOOL bResult = FALSE;

	if ( DelegateProp != NULL )
	{
		UPropertyValue PropValue;
		DelegateProp->GetPropertyValue(PropertyValueAddress, PropValue);
		UComponent* Comp = Cast<UComponent>(PropValue.DelegateValue->Object);
		if ( Comp != NULL )
		{
			Components.AddItem(Comp);
		}

		bResult = TRUE;
	}

	return bResult;
}

/* ==========================================================================================================
	UCustomPropertyItemBindings
========================================================================================================== */
IMPLEMENT_CLASS(UCustomPropertyItemBindings);

/**
 * Returns the custom draw proxy class that should be used for the property associated with
 * the WxPropertyControl specified.
 *
 * @param	ProxyOwnerItem	the property window item that will be using this draw proxy
 * @param	InArrayIndex	specifies which element of an array property that this property window will represent.  Only valid
 *							when creating property window items for individual elements of an array.
 *
 * @return	a pointer to a child of UPropertyDrawProxy that should be used as the draw proxy
 *			for the specified property, or NULL if there is no custom draw proxy configured for
 *			the property.
 */
UClass* UCustomPropertyItemBindings::GetCustomDrawProxy( const WxPropertyControl* ProxyOwnerItem, INT InArrayIndex/*=INDEX_NONE*/ )
{
	check(ProxyOwnerItem);
	const FPropertyNode* PropertyNode = ProxyOwnerItem->GetPropertyNode();
	check(PropertyNode);


	const UProperty* InProperty = PropertyNode->GetProperty();
	check(InProperty);

	UClass* Result = NULL;
	const UStructProperty* StructProp = ConstCast<UStructProperty>(InProperty);
	const UBOOL bIsArrayProperty = InProperty->ArrayDim > 1 || (StructProp == NULL && InProperty->IsA(UArrayProperty::StaticClass()));

	const FString PropertyPathName = InProperty->GetPathName();
	const FString StructPathName = StructProp ? StructProp->Struct->GetPathName() : TEXT("");

	// first, see if we have a custom draw proxy class associated with this specific property
	for ( INT Index = 0; Index < CustomPropertyDrawProxies.Num(); Index++ )
	{
		FPropertyItemCustomProxy& CustomProxy = CustomPropertyDrawProxies(Index);

		UBOOL bMatchesCustomProxy =
			CustomProxy.PropertyPathName == PropertyPathName ||

			// if this is a struct property, see if there is a custom editing control for the struct itself
			(StructProp && CustomProxy.PropertyPathName == StructPathName);

		if ( bMatchesCustomProxy )
		{
			// validate that it's OK to use the custom proxy for arrays
			if ( bIsArrayProperty )
			{
				if ( InArrayIndex == INDEX_NONE && !CustomProxy.bReplaceArrayHeaders )
				{
					// this custom draw proxy cannot be used for array headers
					return NULL;
				}

				if ( InArrayIndex != INDEX_NONE && CustomProxy.bIgnoreArrayElements )
				{
					// this custom draw proxy cannot be used for individual array elements
					return NULL;
				}
			}
			// found an entry for this property - if the associated class hasn't yet been loaded, load it now
			if ( CustomProxy.PropertyItemClass == NULL )
			{
				if ( CustomProxy.PropertyItemClassName.Len() == 0 )
				{
					debugf(NAME_Error, TEXT("No PropertyItemClassName specified for custom draw proxy: %s"), *CustomProxy.PropertyPathName);
				}
				else
				{
					UClass* CustomProxyClass = LoadClass<UPropertyDrawProxy>(NULL, *CustomProxy.PropertyItemClassName, NULL, LOAD_None, NULL);
					if ( CustomProxyClass != NULL )
					{
						CustomProxy.PropertyItemClass = CustomProxyClass;
					}
				}
			}

			if ( CustomProxy.PropertyItemClass != NULL &&
				 CustomProxy.PropertyItemClass->GetDefaultObject<UPropertyDrawProxy>()->Supports(PropertyNode, PropertyNode->GetArrayIndex()))
			{
				Result = CustomProxy.PropertyItemClass;
				break;
			}
		}
	}

	// If we haven't found a matching item so far, check to see if we have a custom proxy class associated with this property type
	if ( Result == NULL && StructProp == NULL )
	{
		const UObjectProperty* ObjectProp = ConstCast<UObjectProperty>(InProperty);
		if ( ObjectProp != NULL )
		{
			const FString ObjectClassPathName = ObjectProp->PropertyClass->GetPathName();
			for ( INT Index = 0; Index < CustomPropertyTypeDrawProxies.Num(); Index++ )
			{
				FPropertyTypeCustomProxy& CustomProxy = CustomPropertyTypeDrawProxies(Index);
				if ( CustomProxy.PropertyName == InProperty->GetClass()->GetFName() )
				{
					if ( CustomProxy.PropertyObjectClassPathName.Len() == 0 )
					{
						debugf(NAME_Error, TEXT("No PropertyObjectClassPathName specified for custom draw proxy (type): %s"), *CustomProxy.PropertyName.ToString());
					}
					else if ( CustomProxy.PropertyObjectClassPathName == ObjectClassPathName )
					{
						// validate that it's OK to use the custom proxy for arrays
						if ( bIsArrayProperty )
						{
							if ( InArrayIndex == INDEX_NONE && !CustomProxy.bReplaceArrayHeaders )
							{
								// this custom draw proxy cannot be used for array headers
								return NULL;
							}

							if ( InArrayIndex != INDEX_NONE && CustomProxy.bIgnoreArrayElements )
							{
								// this custom draw proxy cannot be used for individual array elements
								return NULL;
							}
						}
						if ( CustomProxy.PropertyItemClass == NULL )
						{
							if ( CustomProxy.PropertyItemClassName.Len() == 0 )
							{
								debugf(NAME_Error, TEXT("No PropertyItemClassName specified for custom draw proxy item class: %s"), *CustomProxy.PropertyName.ToString());
							}
							else
							{
								UClass* CustomProxyClass = LoadClass<UPropertyDrawProxy>(NULL, *CustomProxy.PropertyItemClassName, NULL, LOAD_None, NULL);
								if ( CustomProxyClass != NULL )
								{
									CustomProxy.PropertyItemClass = CustomProxyClass;
								}
							}
						}

						if ( CustomProxy.PropertyItemClass != NULL &&
							 CustomProxy.PropertyItemClass->GetDefaultObject<UPropertyDrawProxy>()->Supports(PropertyNode, PropertyNode->GetArrayIndex()) )
						{
							Result = CustomProxy.PropertyItemClass;
							break;
						}
					}
				}
			}
		}
	}

	return Result;
}

/**
 * Returns the custom input proxy class that should be used for the property associated with
 * the WxPropertyControl specified.
 *
 * @param	ProxyOwnerItem	the property window item that will be using this input proxy
 * @param	InArrayIndex	specifies which element of an array property that this property window will represent.  Only valid
 *							when creating property window items for individual elements of an array.
 *
 * @return	a pointer to a child of UPropertyInputProxy that should be used as the input proxy
 *			for the specified property, or NULL if there is no custom input proxy configured for
 *			the property.
 */
UClass* UCustomPropertyItemBindings::GetCustomInputProxy( const WxPropertyControl* ProxyOwnerItem, INT InArrayIndex/*=INDEX_NONE*/ )
{
	check(ProxyOwnerItem);
	const FPropertyNode* PropertyNode = ProxyOwnerItem->GetPropertyNode();
	check(PropertyNode);

	const UProperty* InProperty = ProxyOwnerItem->GetProperty();
	check(InProperty);

	UClass* Result = NULL;
	const UStructProperty* StructProp = ConstCast<UStructProperty>(InProperty);
	const UBOOL bIsArrayProperty = InProperty->ArrayDim > 1 || (StructProp == NULL && InProperty->IsA(UArrayProperty::StaticClass()));

	const FString PropertyPathName = InProperty->GetPathName();
	const FString StructPathName = StructProp ? StructProp->Struct->GetPathName() : TEXT("");

	// first, see if we have a custom input proxy class associated with this specific property
	for ( INT Index = 0; Index < CustomPropertyInputProxies.Num(); Index++ )
	{
		FPropertyItemCustomProxy& CustomProxy = CustomPropertyInputProxies(Index);

		UBOOL bMatchesCustomProxy =
			CustomProxy.PropertyPathName == PropertyPathName ||

			// if this is a struct property, see if there is a custom editing control for the struct itself
			(StructProp && CustomProxy.PropertyPathName == StructPathName);

		if ( bMatchesCustomProxy )
		{
			// validate that it's OK to use the custom proxy for arrays
			if ( bIsArrayProperty )
			{
				if ( InArrayIndex == INDEX_NONE && !CustomProxy.bReplaceArrayHeaders )
				{
					// this custom input proxy cannot be used for array headers
					return NULL;
				}

				if ( InArrayIndex != INDEX_NONE && CustomProxy.bIgnoreArrayElements )
				{
					// this custom input proxy cannot be used for individual array elements
					return NULL;
				}
			}
			// found an entry for this property - if the associated class hasn't yet been loaded, load it now
			if ( CustomProxy.PropertyItemClass == NULL )
			{
				if ( CustomProxy.PropertyItemClassName.Len() == 0 )
				{
					debugf(NAME_Error, TEXT("No PropertyItemClassName specified for custom input proxy: %s"), *CustomProxy.PropertyPathName);
				}
				else
				{
					UClass* CustomProxyClass = LoadClass<UPropertyInputProxy>(NULL, *CustomProxy.PropertyItemClassName, NULL, LOAD_None, NULL);
					if ( CustomProxyClass != NULL )
					{
						CustomProxy.PropertyItemClass = CustomProxyClass;
					}
				}
			}

			if ( CustomProxy.PropertyItemClass != NULL &&
				 CustomProxy.PropertyItemClass->GetDefaultObject<UPropertyInputProxy>()->Supports(PropertyNode, PropertyNode->GetArrayIndex()) )
			{
				Result = CustomProxy.PropertyItemClass;
                break;
			}
		}
	}

	// If we haven't found a matching item so far, check to see if we have a custom proxy class associated with this property type
	if ( Result == NULL && StructProp == NULL )
	{
		const UObjectProperty* ObjectProp = ConstCast<UObjectProperty>(InProperty);
		if ( ObjectProp != NULL )
		{
			const FString ObjectClassPathName = ObjectProp->PropertyClass->GetPathName();
			for ( INT Index = 0; Index < CustomPropertyTypeInputProxies.Num(); Index++ )
			{
				FPropertyTypeCustomProxy& CustomProxy = CustomPropertyTypeInputProxies(Index);
				if ( CustomProxy.PropertyName == InProperty->GetClass()->GetFName() )
				{
					if ( CustomProxy.PropertyObjectClassPathName.Len() == 0 )
					{
						debugf(NAME_Error, TEXT("No PropertyObjectClassPathName specified for custom input proxy: %s"), *CustomProxy.PropertyName.ToString());
					}
					else if ( CustomProxy.PropertyObjectClassPathName == ObjectClassPathName )
					{
						// validate that it's OK to use the custom proxy for arrays
						if ( bIsArrayProperty )
						{
							if ( InArrayIndex == INDEX_NONE && !CustomProxy.bReplaceArrayHeaders )
							{
								// this custom input proxy cannot be used for array headers
								return NULL;
							}

							if ( InArrayIndex != INDEX_NONE && CustomProxy.bIgnoreArrayElements )
							{
								// this custom input proxy cannot be used for individual array elements
								return NULL;
							}
						}
						if ( CustomProxy.PropertyItemClass == NULL )
						{
							if ( CustomProxy.PropertyItemClassName.Len() == 0 )
							{
								debugf(NAME_Error, TEXT("No PropertyItemClassName specified for custom input proxy item class: %s"), *CustomProxy.PropertyName.ToString());
							}
							else
							{
								UClass* CustomProxyClass = LoadClass<UPropertyInputProxy>(NULL, *CustomProxy.PropertyItemClassName, NULL, LOAD_None, NULL);
								if ( CustomProxyClass != NULL )
								{
									CustomProxy.PropertyItemClass = CustomProxyClass;
								}
							}
						}

						if ( CustomProxy.PropertyItemClass != NULL &&
							 CustomProxy.PropertyItemClass->GetDefaultObject<UPropertyInputProxy>()->Supports(PropertyNode, PropertyNode->GetArrayIndex()) )
						{
							Result = CustomProxy.PropertyItemClass;
							break;
						}
					}
				}
			}
		}
	}

	return Result;
}

/**
 * Returns the custom property item class that should be used for the property specified.
 *
 * @param	InProperty	the property that will use the custom property item
 *
 * @return	a pointer to a child of WxItemPropertyControl that should be used as the property
 *			item for the specified property, or NULL if there is no custom property item configured
 * 			for the property.
 */
WxItemPropertyControl* UCustomPropertyItemBindings::GetCustomPropertyWindow( UProperty* InProperty, INT InArrayIndex/*=INDEX_NONE*/)
{
	check(InProperty);

	WxItemPropertyControl* Result = NULL;
	UStructProperty* StructProp = Cast<UStructProperty>(InProperty);
	const UBOOL bIsArrayProperty = InProperty->ArrayDim > 1 || (StructProp == NULL && InProperty->IsA(UArrayProperty::StaticClass()));

	const FString PropertyPathName = InProperty->GetPathName();
	const FString StructPathName = StructProp ? StructProp->Struct->GetPathName() : TEXT("");

    // use this to make certain that the correct values are being used for the UCustomPropertyItemBindings look up code
	//debugf( TEXT( "PropertyPathName: %s  StructPathName: %s" ), *PropertyPathName, *StructPathName );

	// first, see if we have a custom control associated with this specific property
	for ( INT Index = 0; Index < CustomPropertyClasses.Num(); Index++ )
	{
		FPropertyItemCustomClass& CustomClass = CustomPropertyClasses(Index);

		UBOOL bMatchesCustomClass =
			CustomClass.PropertyPathName == PropertyPathName ||

			// if this is a struct property, see if there is a custom editing control for the struct itself
			(StructProp && CustomClass.PropertyPathName == StructPathName);

		if ( bMatchesCustomClass )
		{
			// found an entry for this property - if the associated class hasn't yet been loaded, load it now
			if ( CustomClass.WxPropertyItemClass == NULL )
			{
				if ( CustomClass.PropertyItemClassName.Len() == 0 )
				{
					debugf(NAME_Error, TEXT("No PropertyItemClassName specified for custom property item class: %s"), *CustomClass.PropertyPathName);
				}
				else
				{
					CustomClass.WxPropertyItemClass = wxClassInfo::FindClass(*CustomClass.PropertyItemClassName);
				}
			}

			if ( CustomClass.WxPropertyItemClass != NULL )
			{
				// validate that it's OK to use the custom property control for arrays
				if ( bIsArrayProperty )
				{
					if ( InArrayIndex == INDEX_NONE && !CustomClass.bReplaceArrayHeaders )
					{
						// this custom property class cannot be used for array headers
						return NULL;
					}

					if ( InArrayIndex != INDEX_NONE && CustomClass.bIgnoreArrayElements )
					{
						// this custom property class cannot be used for individual array elements
						return NULL;
					}
				}
				Result = wxDynamicCast(CustomClass.WxPropertyItemClass->CreateObject(),WxItemPropertyControl);
				break;
			}
		}
	}

	// If we haven't found a matching item so far, check to see if we have a custom property item class associated with this property type
	if ( Result == NULL && StructProp == NULL )
	{
		UObjectProperty* ObjectProp = Cast<UObjectProperty>(InProperty);
		if ( ObjectProp != NULL )
		{
			const FString ObjectClassPathName = ObjectProp->PropertyClass->GetPathName();

			// next, check to see if we have a custom proxy class associated with this property type
			for ( INT Index = 0; Index < CustomPropertyTypeClasses.Num(); Index++ )
			{
				FPropertyTypeCustomClass& CustomClass = CustomPropertyTypeClasses(Index);
				if ( CustomClass.PropertyName == InProperty->GetClass()->GetFName() )
				{
					if ( CustomClass.PropertyObjectClassPathName.Len() == 0 )
					{
						debugf(NAME_Error, TEXT("No PropertyObjectClassPathName specified for custom property item class: %s"), *CustomClass.PropertyName.ToString());
					}
					else if ( CustomClass.PropertyObjectClassPathName == ObjectClassPathName )
					{
						if ( CustomClass.WxPropertyItemClass == NULL )
						{
							if ( CustomClass.PropertyItemClassName.Len() == 0 )
							{
								debugf(NAME_Error, TEXT("No PropertyItemClassName specified for custom property item class: %s"), *CustomClass.PropertyName.ToString());
							}
							else
							{
								CustomClass.WxPropertyItemClass = wxClassInfo::FindClass(*CustomClass.PropertyItemClassName);
							}
						}

						if ( CustomClass.WxPropertyItemClass != NULL )
						{
							// validate that it's OK to use the custom property control for arrays
							if ( bIsArrayProperty )
							{
								if ( InArrayIndex == INDEX_NONE && !CustomClass.bReplaceArrayHeaders )
								{
									// this custom property class cannot be used for array headers
									return NULL;
								}

								if ( InArrayIndex != INDEX_NONE && CustomClass.bIgnoreArrayElements )
								{
									// this custom property class cannot be used for individual array elements
									return NULL;
								}
							}
							Result = wxDynamicCast(CustomClass.WxPropertyItemClass->CreateObject(), WxItemPropertyControl);
							break;
						}
					}
				}
			}
		}
	}

	return Result;
}


