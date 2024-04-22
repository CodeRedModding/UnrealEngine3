/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "TextureStatsBrowser.h"
#include "ScopedTransaction.h"
#include "UnConsoleTools.h"
#include "UnConsoleSupportContainer.h"

#if WITH_MANAGED_CODE
	// CLR includes
	#include "ContentBrowserShared.h"
#endif

/** Delay (in milliseconds) after a new character is entered into the search box to wait before updating the list (to give them time to enter a whole string instead of useless updating every time a char is put in) **/
const INT SearchTextUpdateDelay = 500;

FString GetColumnName(ETextureStatsBrowserColumns Key, UBOOL &bOutRightAligned)
{
	bOutRightAligned = FALSE;

	switch(Key)
	{
		case TSBC_Name:
			return LocalizeUnrealEd("TextureStatsBrowser_Name");
		case TSBC_TexType:
			return LocalizeUnrealEd("TextureStatsBrowser_TexType");
		case TSBC_MaxDim:
			return LocalizeUnrealEd("TextureStatsBrowser_Desc");
		case TSBC_CurrentDim:
			return LocalizeUnrealEd("TextureStatsBrowser_CurrentDim");
		case TSBC_Format:
			return LocalizeUnrealEd("TextureStatsBrowser_Format");
		case TSBC_Group:
			return LocalizeUnrealEd("TextureStatsBrowser_Group");
		case TSBC_LODBias:
			return LocalizeUnrealEd("TextureStatsBrowser_LODBias");
		case TSBC_CurrentKB:
			bOutRightAligned = TRUE;
			return LocalizeUnrealEd("TextureStatsBrowser_CurrentKB");
		case TSBC_FullyLoadedKB:
			bOutRightAligned = TRUE;
			return LocalizeUnrealEd("TextureStatsBrowser_FullyLoadedKB");
		case TSBC_NumUses:
			bOutRightAligned = TRUE;
			return LocalizeUnrealEd("TextureStatsBrowser_NumUses");
		case TSBC_LowResMipsKB:
			bOutRightAligned = TRUE;
			return LocalizeUnrealEd("TextureStatsBrowser_LowResMipsKB");
		case TSBC_HighResMipsKB:
			bOutRightAligned = TRUE;
			return LocalizeUnrealEd("TextureStatsBrowser_HighResMipsKB");
		case TSBC_Path:
			return LocalizeUnrealEd("TextureStatsBrowser_Path");
		case TSBC_Packages:
			return LocalizeUnrealEd("TextureStatsBrowser_Packages");
		case TSBC_LastTimeRendered:
			bOutRightAligned = TRUE;
			return LocalizeUnrealEd("TextureStatsBrowser_LastTimeRendered");
		default:
			appErrorf(TEXT("Unhandled case"));
	}

	appErrorf(TEXT("Unhandled case"));
	return TEXT("");
}

UBOOL IsColumnShownInThatMode(ETextureStatsBrowserListMode ListMode, ETextureStatsBrowserColumns Key)
{
	// hide columns that cannot have any data
	if(ListMode != LM_CookerStatistics)
	{
		if(Key == TSBC_LowResMipsKB
		|| Key == TSBC_HighResMipsKB)
		{
			return FALSE;
		}
	}

	if( ListMode == LM_RemoteCapture )
	{
		if( Key == TSBC_Packages )
		{
			return FALSE;
		}
	}

	if(ListMode == LM_CookerStatistics)
	{
		if(Key == TSBC_NumUses
		|| Key == TSBC_CurrentKB
		|| Key == TSBC_LastTimeRendered
		|| Key == TSBC_CurrentDim)
		{
			return FALSE;
		}
	}

	if( ListMode == LM_AllLevels || ListMode == LM_CurrentLevel )
	{
		if(Key == TSBC_Packages)
		{
			return FALSE;
		}
	}


	return TRUE;
}

void FUniqueTextureStats::AddInfo(FUniqueTextureStats& Info)
{
	if(Info.CurrentKB != FLT_MAX)
	{
		CurrentKB += Info.CurrentKB;
	}

	FullyLoadedKB += Info.FullyLoadedKB;

	if(Info.LowResMipsKB != FLT_MAX)
	{
		LowResMipsKB += Info.LowResMipsKB;
	}

	if(Info.HighResMipsKB != FLT_MAX)
	{
		HighResMipsKB += Info.HighResMipsKB;
	}

	if(Info.NumUses != 0xFFFFFFFF)
	{
		NumUses += Info.NumUses;
	}
}

// 0 if not found
static UINT CountLeadingPart(const TCHAR *Name)
{
	UINT n = 1;

	while(*Name != TCHAR('_'))
	{
		if(*Name == 0)
		{
			// drop 0 characters
			return 0;
		}

		++n;
		++Name;
	}

	// drop leading n characters
	return n;
}

void FUniqueTextureStats::GetColumnData(ETextureStatsBrowserColumns Column, FString &Out) const
{
	// non FLOAT
	switch(Column)
	{
		case TSBC_Name:
			Out = Name;
			return;
		case TSBC_TexType:
			Out = TexType;
			return;
		case TSBC_MaxDim:
			Out = MaxDim;
			return;
		case TSBC_CurrentDim:
			Out = CurrentDim;
			return;
		case TSBC_Format:
			{
				static const UINT Lead = CountLeadingPart(UTexture::GetPixelFormatString(PF_MAX));
				Out = UTexture::GetPixelFormatString(Format) + Lead;
			}
			return;
		case TSBC_Group:
			{
				static const UINT Lead = CountLeadingPart(UTexture::GetTextureGroupString(TEXTUREGROUP_MAX));
				Out = UTexture::GetTextureGroupString(Group) + Lead;
			}
			return;
		case TSBC_Path:
			Out = Path;
			return;
		case TSBC_Packages:
			if(Packages.Len())
			{
				Out = Packages;
			}
			else
			{
				Out = TEXT("?");
			}
			return;
	}

	TCHAR *Formatting = TEXT("%.0f");
	// Numbers
	FLOAT Number = FLT_MAX;
	switch(Column)
	{
		case TSBC_LODBias:
			Number = LODBias;
			break;
		case TSBC_CurrentKB:
			Number = CurrentKB;
			break;
		case TSBC_FullyLoadedKB:
			Number = FullyLoadedKB;
			break;
		case TSBC_NumUses:
			if(NumUses != 0xFFFFFFFF)
			{
				Number = NumUses;
			}
			break;
		case TSBC_LowResMipsKB:
			Number = LowResMipsKB;
			break;
		case TSBC_HighResMipsKB:
			Number = HighResMipsKB;
			break;
		case TSBC_LastTimeRendered:
			Formatting = TEXT("%.2f");
			Number = LastTimeRendered;
			break;
		default:
			appErrorf(TEXT("Unhandled case"));
	}

	if(Number == FLT_MAX)
	{
		Out = TEXT("?");
	}
	else
	{
		Out = FString::Printf(Formatting, (float)appCeil(Number));
	}

}

template <class T>
INT CompareValue(const T a, const T b )
{
	if(a == b)
	{
		return 0;
	}
	
	return (a > b) ? 1 : -1;
}

INT FUniqueTextureStats::Compare(const FUniqueTextureStats& Other, ETextureStatsBrowserColumns SortIndex) const
{
	switch(SortIndex)
	{
		case TSBC_Name:
			return appStricmp(*Name, *Other.Name);
		case TSBC_TexType:
			return appStricmp(*TexType, *Other.TexType);
		case TSBC_MaxDim:
			return appStricmp(*MaxDim, *Other.MaxDim);
		case TSBC_CurrentDim:
			return appStricmp(*CurrentDim, *Other.CurrentDim);
		case TSBC_Format:
			return CompareValue(Format, Other.Format);
		case TSBC_Group:
			return CompareValue((UINT)Group, (UINT)Other.Group);
		case TSBC_LODBias:
			return CompareValue(LODBias, Other.LODBias);
		case TSBC_CurrentKB:
			return CompareValue(CurrentKB, Other.CurrentKB);
		case TSBC_FullyLoadedKB:
			return CompareValue(FullyLoadedKB, Other.FullyLoadedKB);
		case TSBC_NumUses:
			return CompareValue(NumUses, Other.NumUses);
		case TSBC_LowResMipsKB:
			return CompareValue(LowResMipsKB, Other.LowResMipsKB);
		case TSBC_HighResMipsKB:
			return CompareValue(HighResMipsKB, Other.HighResMipsKB);
		case TSBC_Path:
			return appStricmp(*Path, *Other.Path);
		case TSBC_Packages:
			return appStricmp(*Packages, *Other.Packages);
		case TSBC_LastTimeRendered:
			return CompareValue(LastTimeRendered, Other.LastTimeRendered);
		default:
			appErrorf(TEXT("Unhandled case"));
	}

	return 0;
}

FArchive& operator<<( FArchive& Ar, FUniqueTextureStats& TextureStats )
{
	// serialize all elements we want the garbage collector to not remove from memory
	return Ar;
}

/**
 * The texture stats browser menu bar.
 */
class WxMBTextureStatsBrowser : public wxMenuBar
{
public:
	WxMBTextureStatsBrowser()
	{
		// File menu
		wxMenu* FileMenu = new wxMenu();
		FileMenu->Append( ID_TEXTURESTATSBROWSER_EXPORT, *LocalizeUnrealEd("TextureStatsBrowser_Export") );
		Append( FileMenu, *LocalizeUnrealEd("File") );
		
		// View menu
		wxMenu* ViewMenu = new wxMenu();
		ViewMenu->Append( IDM_RefreshBrowser, *LocalizeUnrealEd("RefreshWithHotkey"), TEXT("") );
		Append( ViewMenu, *LocalizeUnrealEd("View") );

		// Only add this menu on internal builds
		if( GIsEpicInternal )
		{
			FString RemoteConsoleName;
			GConfig->GetString( TEXT("TextureStatsBrowser"), TEXT("RemoteConsoleName"), RemoteConsoleName, GEditorUserSettingsIni );

			// Console menu for selecting which console to remote capture from
			wxMenu* ConsoleMenu = new wxMenu();
			INT ConsoleIndex = 0;
	
			// Iterate over each supported console type and add a menu for it.
			const INT MaxNumConsoles = ID_TEXTURESTATSBROWSER_CONSOLEMENU_END - ID_TEXTURESTATSBROWSER_CONSOLEMENU_START;
			for (FConsoleSupportIterator It; It && ConsoleIndex < MaxNumConsoles; ++It, ++ConsoleIndex )
			{
				FConsoleSupport* ConsoleSupport = *It;
				// Only add a menu option for this if the console has any targets
				if ( ConsoleSupport->GetTargets(NULL) > 0 )
				{
					FString ConsoleName = ConsoleSupport->GetPlatformName();
					wxMenuItem* Item = ConsoleMenu->AppendRadioItem( ID_TEXTURESTATSBROWSER_CONSOLEMENU_START + ConsoleIndex, *ConsoleName );
					if( ConsoleName == RemoteConsoleName )
					{
						Item->Check();
					}
				}
			}

			Append( ConsoleMenu, *LocalizeUnrealEd("TextureStatsBrowser_ConsoleMenu") );
		}

		WxBrowser::AddDockingMenu( this );
	}
};


BEGIN_EVENT_TABLE(WxTextureStatsBrowser,WxBrowser)
	EVT_MENU(IDM_RefreshBrowser,WxTextureStatsBrowser::OnRefresh)
	EVT_MENU(ID_TEXTURESTATSBROWSER_EXPORT, WxTextureStatsBrowser::OnExport)
	EVT_MENU(ID_TEXTURESTATSBROWSER_COPYSELECTED, WxTextureStatsBrowser::OnCopySelected)
	EVT_SIZE(WxTextureStatsBrowser::OnSize)
    EVT_LIST_COL_CLICK(ID_TEXTURESTATSBROWSER_LISTCONTROL, WxTextureStatsBrowser::OnColumnClick)
	EVT_LIST_COL_RIGHT_CLICK(ID_TEXTURESTATSBROWSER_LISTCONTROL, WxTextureStatsBrowser::OnColumnRightClick)
	EVT_LIST_ITEM_RIGHT_CLICK(ID_TEXTURESTATSBROWSER_LISTCONTROL, WxTextureStatsBrowser::OnItemRightClick)
    EVT_LIST_ITEM_ACTIVATED(ID_TEXTURESTATSBROWSER_LISTCONTROL, WxTextureStatsBrowser::OnItemActivated)
	EVT_LIST_ITEM_SELECTED(ID_TEXTURESTATSBROWSER_LISTCONTROL, WxTextureStatsBrowser::OnItemSelectionChanged)
	EVT_LIST_ITEM_DESELECTED(ID_TEXTURESTATSBROWSER_LISTCONTROL, WxTextureStatsBrowser::OnItemSelectionChanged)
	EVT_MENU(ID_TEXTURESTATS_POPUPMENU_SYNCACTORS, WxTextureStatsBrowser::OnSyncToActors)
	EVT_MENU(ID_TEXTURESTATS_POPUPMENU_SYNCMATERIALS, WxTextureStatsBrowser::OnSyncToMaterials)
	EVT_MENU(ID_TEXTURESTATS_POPUPMENU_INVERTSELECTION, WxTextureStatsBrowser::OnInvertSelection)
	EVT_MENU(ID_TEXTURESTATS_POPUPMENU_HIDESELECTION, WxTextureStatsBrowser::OnHideSelection)
	EVT_MENU_RANGE(ID_TEXTURESTATSBROWSER_CONSOLEMENU_START, ID_TEXTURESTATSBROWSER_CONSOLEMENU_END, WxTextureStatsBrowser::OnChangeConsole )
	EVT_BUTTON(ID_TEXTURESTATS_POPUPMENU_HIDESELECTION, WxTextureStatsBrowser::OnHideSelection)
	EVT_MENU(ID_TEXTURESTATS_POPUPMENU_UNHIDEALL, WxTextureStatsBrowser::OnUnhideAll)
	EVT_BUTTON(ID_TEXTURESTATS_POPUPMENU_UNHIDEALL, WxTextureStatsBrowser::OnUnhideAll)
	EVT_COMBOBOX( ID_TEXTURESTATSBROWSER_LISTMODE_COMBOBOX, WxTextureStatsBrowser::OnTextureListModeChanged )
	EVT_COMBOBOX( ID_TEXTURESTATSBROWSER_SEARCH_COMBOBOX, WxTextureStatsBrowser::OnSearchModeChanged )
	EVT_TEXT( ID_TEXTURESTATSBROWSER_SEARCHBOX, WxTextureStatsBrowser::OnSearchTextChanged )
	EVT_TIMER( ID_TEXTURESTATSBROWSER_TIMER, WxTextureStatsBrowser::OnTimer )
END_EVENT_TABLE()

/** Current sort order (-1 or 1) */
INT WxTextureStatsBrowser::CurrentSortOrder[TSBC_MAX];
/** Primary index/ column to sort by */
ETextureStatsBrowserColumns WxTextureStatsBrowser::PrimarySortIndex = TSBC_FullyLoadedKB;
/** Secondary index/ column to sort by */
ETextureStatsBrowserColumns WxTextureStatsBrowser::SecondarySortIndex = TSBC_FullyLoadedKB;


WxTextureStatsBrowser::WxTextureStatsBrowser() :
	ListControl( NULL ),
	TextureListModes( NULL ),
	CombinedStatsControl( NULL ),
	ColumnHeaders(),
	AllStats(),
	CookerPathData(TEXT("GlobalPersistentCookerData.upk")),
	ColumnsListMode(LM_Max),
	RemoteConsoleIndex(INDEX_NONE)
{
	for(UINT i = 0; i < TSBC_MAX; ++i)
	{
		CurrentSortOrder[i] = 1;
	}
	
	GCallbackEvent->Register( CALLBACK_SelChange, this );
	GCallbackEvent->Register( CALLBACK_NewCurrentLevel, this );

	FString RemoteConsoleName=TEXT("Xbox360");
	GConfig->GetString( TEXT("TextureStatsBrowser"), TEXT("RemoteConsoleName"), RemoteConsoleName, GEditorUserSettingsIni );

	// Fixup old xenon naming
	if( RemoteConsoleName == TEXT("Xenon") )
	{
		RemoteConsoleName == TEXT("Xbox360");
		GConfig->SetString( TEXT("TextureStatsBrowser"), TEXT("RemoteConsoleName"), *RemoteConsoleName, GEditorUserSettingsIni );
	}

	INT ConsoleIndex = 0;
	// Iterate over each supported console type and find the index of the console saved in the ini.
	const INT MaxNumConsoles = ID_TEXTURESTATSBROWSER_CONSOLEMENU_END - ID_TEXTURESTATSBROWSER_CONSOLEMENU_START;
	for (FConsoleSupportIterator It; It && ConsoleIndex < MaxNumConsoles; ++It, ++ConsoleIndex )
	{
		FConsoleSupport* ConsoleSupport = *It;
		if ( ConsoleSupport->GetTargets(NULL) > 0 )
		{
			if( RemoteConsoleName == ConsoleSupport->GetPlatformName() )
			{
				RemoteConsoleIndex = ConsoleIndex;
				break;
			}
		}
	}
}

/**
 * Determines if the passed in texture should be considered for stats
 * For example 2D texture's belonging to a TextureCube should be ignored since the cube will account for them
 * 
 * @param Texture	The texture to check
 * @return			True if the texture is valid for stats
 */
UBOOL WxTextureStatsBrowser::IsTextureValidForStats( UTexture* Texture ) 
{
	UBOOL bIsValid =	Texture && // texture must exist
						TexturesToIgnore.FindItemIndex( Texture ) == INDEX_NONE && // texture is not one that should be ignored
						( Texture->IsA( UTexture2D::StaticClass() ) || Texture->IsA( UTextureCube::StaticClass() ) ); // texture is valid texture class for stat purposes
	
	UTextureCube* CubeTex = Cast<UTextureCube>( Texture );
	if( CubeTex )
	{
		// If the passed in texture is a cube, add all faces of the cube to the ignore list since the cube will account for those
		for( INT FaceIdx = 0; FaceIdx < 6; ++FaceIdx )
		{
			TexturesToIgnore.AddItem( CubeTex->GetFace( FaceIdx ) );
		}
	}

	return bIsValid;
}
/**
 * Inserts the columns into the control.
 */
void WxTextureStatsBrowser::InsertColumns()
{
	ListControl->DeleteAllColumns();

	// Take this opportunity to repopulate the search modes based on what columns are available
	// Cache the current selection so we can re-select later
	INT CurrentSelection = SearchModes->GetCurrentSelection();
	SearchModes->Clear();

	const ETextureStatsBrowserListMode ListMode = (ETextureStatsBrowserListMode)TextureListModes->GetCurrentSelection();

	INT DstColIdx = 0;
	for(INT SrcColIdx = 0; SrcColIdx < TSBC_MAX; SrcColIdx++)
	{
		if(IsColumnShownInThatMode(ListMode, (ETextureStatsBrowserColumns)SrcColIdx))
		{
			UBOOL bRightAlign;
			FString ColumnHeader = GetColumnName((ETextureStatsBrowserColumns)SrcColIdx, bRightAlign);

			int Format = (bRightAlign ? wxLIST_FORMAT_RIGHT : wxLIST_FORMAT_LEFT); 

			ListControl->InsertColumn(DstColIdx++, *ColumnHeader, Format);
			new(ColumnHeaders) FString(ColumnHeader);

			// Add this column as a search option
			SearchModes->Append( *ColumnHeader );
			
			// If this column is the same index as our selection in the search options, reselect it.
			if ( SrcColIdx == CurrentSelection )
			{
				SearchModes->SetSelection( SrcColIdx );
			}
		}
	}

	// If we have no selection or the selection is now invalid, select the first item.
	if( CurrentSelection == -1 || (INT)SearchModes->GetCount() <= CurrentSelection )
	{
		SearchModes->SetSelection( 0 );
	}

}

/**
 * Forwards the call to our base class to create the window relationship.
 * Creates any internally used windows after that
 *
 * @param DockID the unique id to associate with this dockable window
 * @param FriendlyName the friendly name to assign to this window
 * @param Parent the parent of this window (should be a Notebook)
 */
void WxTextureStatsBrowser::Create(INT DockID,const TCHAR* FriendlyName, wxWindow* Parent)
{
	// Let our base class start up the windows
	WxBrowser::Create(DockID,FriendlyName,Parent);	

	// Create the panel for this window
	Panel = new wxPanel( this, ID_TEXTURESTATSBROWSER_PANEL );
	{
		// Create the main sizer for all widgets on the panel
		wxBoxSizer* MainSizer = new wxBoxSizer( wxVERTICAL );
		Panel->SetSizer( MainSizer );

		// Create horizontal sizers for aligning tools 
		wxBoxSizer* ToolsSizer = new wxBoxSizer( wxHORIZONTAL );
		wxBoxSizer* SearchSizer = new wxBoxSizer( wxHORIZONTAL );
		// Create a label for the texture list mode combo box
		wxStaticText* TextureListText = new wxStaticText( Panel, wxID_STATIC, *LocalizeUnrealEd("TextureStatsBrowser_TextureListModeLabel") );
		ToolsSizer->Add( TextureListText, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
		
		// Make a list of available listing modes
		wxString ComboBoxChoiceList[ LM_Max ];
		ComboBoxChoiceList[ LM_SelectedActors ] =		*LocalizeUnrealEd("TextureStatsBrowser_ModeSelectedActors");
		ComboBoxChoiceList[ LM_SelectedMaterials ] =	*LocalizeUnrealEd("TextureStatsBrowser_ModeSelectedMaterials");
		ComboBoxChoiceList[ LM_CurrentLevel ] =			*LocalizeUnrealEd("TextureStatsBrowser_ModeCurrentLevel");
		ComboBoxChoiceList[ LM_AllLevels ] =			*LocalizeUnrealEd("TextureStatsBrowser_ModeAllLevels");
		ComboBoxChoiceList[ LM_CookerStatistics ] =		*LocalizeUnrealEd("TextureStatsBrowser_ModeCookerStatistics");
		ComboBoxChoiceList[ LM_RemoteCapture ] =		*LocalizeUnrealEd("TextureStatsBrowser_ModeRemoteCapture");

		UINT ListModeCount = LM_Max; 
		if(!GIsEpicInternal)
		{
			// as long we rely on remote propagation and this is only visible in internal builds we don't support this mode outside of epic
			// search code for label !RemoteCapture
			--ListModeCount;
		}

		wxArrayString ComboBoxChoices( ListModeCount, ComboBoxChoiceList );
		
		// Attempt to get the current list mode from the config file.  Current level is the default mode
		ETextureStatsBrowserListMode ListMode;
		{
			const ETextureStatsBrowserListMode DefaultMode = LM_CurrentLevel;
			INT ListModeInt = DefaultMode;
			if( GConfig->GetInt( TEXT("TextureStatsBrowser"), TEXT("TextureListMode"), ListModeInt, GEditorUserSettingsIni ) )
			{
				// Make sure the list mode is a valid index into the list of choices
				if( !IsWithin<INT>( ListModeInt, 0, LM_Max-1) )
				{
					// If the user edited the ini and changed the list mode to a value outside of a valid range
					// set the list mode back to default
					ListModeInt = DefaultMode;
				}
			}
			ListMode = (ETextureStatsBrowserListMode)ListModeInt;
		}

		// Create the texture list mode combo box and add it to the tool sizer
		TextureListModes = new WxComboBox( Panel, ID_TEXTURESTATSBROWSER_LISTMODE_COMBOBOX, ComboBoxChoices[ListMode], wxDefaultPosition, wxDefaultSize, ComboBoxChoices, wxCB_DROPDOWN | wxCB_READONLY );
		ToolsSizer->Add( TextureListModes, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
		ToolsSizer->AddSpacer( 20 );

		// Create a label to signal the beginning of the combined stats information
		wxStaticText* CombinedStatsText = new wxStaticText( Panel, wxID_STATIC, *LocalizeUnrealEd("TextureStatsBrowser_CombinedStats") );
		ToolsSizer->Add( CombinedStatsText, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );

		// Create a read-only text control that displays the combined number of CurrentKB, a label and a space
		{
			CombinedStatsControl = new wxTextCtrl( Panel, ID_TEXTURESTATSBROWSER_COMBINEDKBTEXTCONTROL, wxEmptyString, wxDefaultPosition, wxSize( 115, -1 ), wxTE_READONLY | wxTE_CENTER );
			ToolsSizer->Add( CombinedStatsControl, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
			wxStaticText* Label = new wxStaticText( Panel, wxID_STATIC, *LocalizeUnrealEd("TextureStatsBrowser_KB") );
			ToolsSizer->Add( Label, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
		}

		// Hide selected button
		{
			HideSelectionButton = new wxButton( Panel, ID_TEXTURESTATS_POPUPMENU_HIDESELECTION, *LocalizeUnrealEd("TextureStatsBrowser_HideSelection"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
			ToolsSizer->Add(HideSelectionButton, 1, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
		}

		// Unhide all
		{
			UnhideAllButton = new wxButton( Panel, ID_TEXTURESTATS_POPUPMENU_UNHIDEALL, *LocalizeUnrealEd("TextureStatsBrowser_UnhideAll"), wxDefaultPosition, wxSize( -1, -1 ), 0 );
			ToolsSizer->Add(UnhideAllButton, 1, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
		}

		// Search bar and search options
		{
			wxStaticText* SearchLabel = new wxStaticText( Panel, wxID_STATIC, TEXT("Search: ") );
			SearchBox = new wxTextCtrl( Panel, ID_TEXTURESTATSBROWSER_SEARCHBOX, wxEmptyString, wxDefaultPosition, wxSize(215,-1) );
			wxStaticText* ComboLabel = new wxStaticText( Panel, wxID_STATIC, TEXT("Search Column: ") );
			SearchModes = new WxComboBox( Panel, ID_TEXTURESTATSBROWSER_SEARCH_COMBOBOX, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY );
			
			SearchSizer->Add( SearchLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
			SearchSizer->Add( SearchBox, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
			SearchSizer->AddSpacer( 15 );
			SearchSizer->Add( ComboLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
			SearchSizer->Add( SearchModes, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
		}
	
		// Create list control
		ListControl = new WxListView( Panel, ID_TEXTURESTATSBROWSER_LISTCONTROL, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_HRULES | wxLC_VRULES );

		MainSizer->Add( ToolsSizer, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
		MainSizer->Add( SearchSizer, 0, wxALIGN_CENTER_VERTICAL|wxALL, 2 );
		MainSizer->Add( ListControl, 1, wxGROW|wxALL, 2);

		Layout();

	}
	Panel->Fit();
	Panel->GetSizer()->SetSizeHints(Panel);

	// Add a menu bar.
	MenuBar = new WxMBTextureStatsBrowser();

	// Set the search timers owner to this browser so we can receive events.
	SearchTimer.SetOwner( this, ID_TEXTURESTATSBROWSER_TIMER );
}

/**
 * Adds entries to the browser's accelerator key table.
 */
void WxTextureStatsBrowser::AddAcceleratorTableEntries(TArray<wxAcceleratorEntry>& Entries)
{
	// Call up to parent to get parents entries
	WxBrowser::AddAcceleratorTableEntries(Entries);
	// Add an entry for Ctrl+c
	Entries.AddItem( wxAcceleratorEntry( wxACCEL_CTRL, (INT)'C', ID_TEXTURESTATSBROWSER_COPYSELECTED ) );

}

/**
 * Called when the browser is getting activated (becoming the visible
 * window in it's dockable frame).
 */
void WxTextureStatsBrowser::Activated()
{
	// Let the super class do it's thing
	WxBrowser::Activated();
	Update();
}

static INT CompareEntries(const FUniqueTextureStats& A, const FUniqueTextureStats& B)
{
	INT Result = WxTextureStatsBrowser::CurrentSortOrder[WxTextureStatsBrowser::PrimarySortIndex] * A.Compare(B, WxTextureStatsBrowser::PrimarySortIndex);
	if(Result == 0)
	{
		Result = WxTextureStatsBrowser::CurrentSortOrder[WxTextureStatsBrowser::SecondarySortIndex] * A.Compare(B, WxTextureStatsBrowser::SecondarySortIndex);
	}
	return Result;
}

// Sort helper class.
IMPLEMENT_COMPARE_CONSTREF( FUniqueTextureStats, TextureStatsBrowser, { return CompareEntries(A, B); });

/**
 * Tells the browser to update itself
 */
void WxTextureStatsBrowser::Update()
{
	BuildList();
	RefreshList();
}

/**
 * Refreshes the contents of the window when requested
 *
 * @param In the command that was sent
 */
void WxTextureStatsBrowser::OnRefresh(wxCommandEvent& In)
{
	// Do nothing unless we are visible
	if (IsShownOnScreen() == TRUE)
	{
		BuildList();
		RefreshList();
	}
}

/**
 * Called when the Export... menu option is selected.  This will export the texture stats list to a csv file
 */
void WxTextureStatsBrowser::OnExport(wxCommandEvent& In)
{
	// Present the user with a save dialog to save the csv file.
	WxFileDialog FileDialog( this, *LocalizeUnrealEd("SaveAs"), *GApp->LastDir[LD_GENERIC_EXPORT], TEXT(""), TEXT("CSV document|*.csv|All files|*.*"), wxSAVE|wxOVERWRITE_PROMPT, wxDefaultPosition );
	if( FileDialog.ShowModal() == wxID_OK )
	{
		// If the user pressed ok, create a file writer based on the filename the user selected
		FArchive* CSVFile = GFileManager->CreateFileWriter( FileDialog.GetPath().c_str() );

		// Get the current listing mode
		ETextureStatsBrowserListMode ListMode =(ETextureStatsBrowserListMode)TextureListModes->GetCurrentSelection();

		// First write out column headers to the file
		FString ColumnHeaderRow;
		for( INT SrcColIdx = 0; SrcColIdx < TSBC_MAX; SrcColIdx++)
		{
			// Only write out column headers relevant to the current listing mode
			if(IsColumnShownInThatMode(ListMode, (ETextureStatsBrowserColumns)SrcColIdx))
			{
				UBOOL Unused;
				ColumnHeaderRow += GetColumnName( (ETextureStatsBrowserColumns)SrcColIdx, Unused ).TrimTrailing() + TEXT(",");
			}
		}
		// Add a new line at the end
		ColumnHeaderRow += LINE_TERMINATOR;
		// Write to the file
		CSVFile->Serialize( TCHAR_TO_ANSI(*ColumnHeaderRow), ColumnHeaderRow.Len() );

		// Go through each stat and write out its contents to the file
		for( INT StatsIndex = 0; StatsIndex < AllStats.Num(); StatsIndex++ )
		{
			const FUniqueTextureStats& StatsEntry = AllStats(StatsIndex);

			FString FullyQualifiedName = StatsEntry.GetFullyQualifiedName();

			// Skip writing stats out that the user has hidden
			if(HiddenAssetNames.Find(FullyQualifiedName))
			{
				// Skip hidden assets
				continue;
			}

			FString CSVRow;
			for(INT SrcColIdx = 0; SrcColIdx < TSBC_MAX; SrcColIdx++)
			{
				// Only export data that is relevant in the current mode.
				if(IsColumnShownInThatMode(ListMode, (ETextureStatsBrowserColumns)SrcColIdx))
				{
					FString Text;
					StatsEntry.GetColumnData((ETextureStatsBrowserColumns)SrcColIdx, Text);
					CSVRow+=Text+TEXT(",");
				}
			}
			// Add a new line at the end
			CSVRow+=LINE_TERMINATOR;
			// Write to the file
			CSVFile->Serialize( TCHAR_TO_ANSI(*CSVRow), CSVRow.Len() );
		}

		if( CSVFile )
		{
			// Close the file and clean up memory.
			CSVFile->Close();
			delete CSVFile;
			CSVFile = NULL;
		}
	}	
}

/** 
 * Copies selected rows from the texture stats list to the clipboard.  Uses a tab to delimit values for easy pasting into excel
 */
void WxTextureStatsBrowser::OnCopySelected( wxCommandEvent& In )
{
	FString CopyString;
	
	// Get the current listing mode
	ETextureStatsBrowserListMode ListMode =(ETextureStatsBrowserListMode)TextureListModes->GetCurrentSelection();

	// Always copy the column headers.  The user cant select the columns so we just always do this so they know what data goes with what column
	for( INT SrcColIdx = 0; SrcColIdx < TSBC_MAX; SrcColIdx++)
	{
		// Only copy column headers relevant to the current mode
		if(IsColumnShownInThatMode(ListMode, (ETextureStatsBrowserColumns)SrcColIdx))
		{
			UBOOL Unused;
			CopyString += GetColumnName( (ETextureStatsBrowserColumns)SrcColIdx, Unused ).TrimTrailing() + TEXT("\t");
		}
	}
	CopyString += LINE_TERMINATOR;

	// Get each selected row and copy it.
	LONG SelectedIndex = ListControl->GetFirstSelected();
	while (SelectedIndex != -1)
	{	
		FString Row;
		wxListItem TempItem;
		TempItem.SetId( SelectedIndex );
		// We only want the text part of the list item data.
		TempItem.SetMask( wxLIST_MASK_TEXT );

		// We need to get the actual index of the column header used by the list control.  
		// Since some columns may be hidden, the enum value of a specific column may not match the index of the column in the list control
		INT DestColIdx = 0;
		for( INT SrcColIdx = 0; SrcColIdx < TSBC_MAX; ++SrcColIdx )
		{
			if(IsColumnShownInThatMode(ListMode, (ETextureStatsBrowserColumns)SrcColIdx))
			{
				// Get the text from each column
				TempItem.SetColumn(DestColIdx++);
				ListControl->GetItem( TempItem );
				Row+=TempItem.GetText();
				if( SrcColIdx < TSBC_MAX-1)
				{
					// End each column with a tab for easy paste into excel.
					Row+=TEXT("\t");
				}
			}
		}
		
		// Add row to our copy string
		CopyString+=Row;
		CopyString+=LINE_TERMINATOR;
	
		// Get the next selected index
		SelectedIndex = ListControl->GetNextSelected( SelectedIndex );
	
	}
	
	// Copy to clipboard
	appClipboardCopy(*CopyString);
}


/** 
 * FCallbackDevice Interface
 */
void WxTextureStatsBrowser::Send( ECallbackEventType InType )
{
	if( InType == CALLBACK_SelChange || InType == CALLBACK_NewCurrentLevel )
	{
		const ETextureStatsBrowserListMode ListMode = (ETextureStatsBrowserListMode)TextureListModes->GetCurrentSelection();

		if ( ( InType == CALLBACK_SelChange && ListMode != LM_AllLevels && ListMode != LM_CurrentLevel && ListMode != LM_CookerStatistics && ListMode != LM_RemoteCapture ) ||
			 ( InType == CALLBACK_NewCurrentLevel && ListMode == LM_CurrentLevel ) ) 
		{
			// Only do costly updates if we are in a mode that cares about what is currently selected.  The cases for update are: 
			// Selection changes when our mode is not all levels or the current level (changing selection on these modes wont actually change the list)
			// Current level changes when our mode is the current level
			Update();
		}
	}
	else
	{
		// All other callbacks should be routed through the WxBrowser Send function
		WxBrowser::Send( InType );
	}
}

/** 
 * FSerializableObject interface
 */
void WxTextureStatsBrowser::Serialize( FArchive& Ar )
{
	WxReferencedAssetsBrowserBase::Serialize( Ar );

	if( !Ar.IsPersistent() )
	{
		Ar << AllStats;
		Ar << TexturesToIgnore;
		Ar << ReferencingObjects;
	}
}
/**
 * Sets column width.
 */
void WxTextureStatsBrowser::SetAutoColumnWidth()
{
	const ETextureStatsBrowserListMode ListMode = (ETextureStatsBrowserListMode)TextureListModes->GetCurrentSelection();

	INT DstColIdx = 0;
	for(INT SrcColIdx = 0; SrcColIdx < TSBC_MAX; SrcColIdx++)
	{
		if(IsColumnShownInThatMode(ListMode, (ETextureStatsBrowserColumns)SrcColIdx))
		{
			ListControl->SetColumnWidth( DstColIdx, wxLIST_AUTOSIZE_USEHEADER );
			++DstColIdx;
		}
	}
}


void WxTextureStatsBrowser::UpdateListFromCookerData()
{
	UPersistentCookerData* CookerData = LoadObject<UPersistentCookerData>( NULL, TEXT("GlobalPersistentCookerData.PersistentCookerData"), *CookerPathData, LOAD_None, NULL );
	
	if(CookerData)
	{
		for(TMap<FString,FCookedTextureUsageInfo>::TConstIterator It( CookerData->TextureUsageInfos ); It; ++It)
		{
			const FString& TextureName = It.Key();
			const FCookedTextureUsageInfo& UsageInfo = It.Value();

			UpdateListItem(TextureName, UsageInfo);
		}
	}
}

void WxTextureStatsBrowser::UpdateListItem(const FString& TextureName, const FCookedTextureUsageInfo &UsageInfo)
{
	FString MaxDim = *FString::Printf(TEXT("%dx%d"), UsageInfo.SizeX, UsageInfo.SizeY);

	// The package names separated by comma, e.g. "CORE,MP_Level0,MP_Level1"
	FString Packages;
	for(TSet<FString>::TConstIterator it(UsageInfo.PackageNames); it; ++it)
	{
		if(Packages.Len())
		{
			Packages += ",";
		}
		Packages += *it;
	}

	FUniqueTextureStats Stats(
		TextureName, 
		MaxDim,
		TEXT(""),
		TEXT("2D/Cube"),
		Packages,
		(EPixelFormat)UsageInfo.Format, 
		(TextureGroup)UsageInfo.LODGroup,
		UsageInfo.StoredOnceMipSize / 1024.0f, 
		UsageInfo.DuplicatedMipSize / 1024.0f,
		(UsageInfo.StoredOnceMipSize + UsageInfo.DuplicatedMipSize) / 1024.0f,
		FLT_MAX,
		FLT_MAX,
		0,
		0);

	const INT ExistingStatIndex = AllStats.FindItemIndex(Stats);
	if(ExistingStatIndex == INDEX_NONE)
	{
		AllStats.AddItem(Stats);
	}
}

/** 
 * Called from outside the texture stats browser to sync to textures in our current list
 * 
 * @param InTextures	Textures to find and sync to
 * @param bFocusWindow	If we should focus the window
 * @return TRUE if any textures were found, FALSE otherwise
 */
UBOOL WxTextureStatsBrowser::SyncToTextures( const TArray<UObject*>& InTextures, UBOOL bFocusWindow )
{
	ListControl->Freeze();
	if( bFocusWindow )
	{
		ListControl->SetFocus();
	}

	// Get the colum index for texture name and path
	const ETextureStatsBrowserListMode ListMode = (const ETextureStatsBrowserListMode)TextureListModes->GetCurrentSelection();
	INT NameCol = GetColumnIndexFromEnum(ListMode, TSBC_Name);
	INT PathCol = GetColumnIndexFromEnum(ListMode, TSBC_Path);

	// Build a mapping of texture names to list index. 
	// Do this once for fast lookups
	TMap<FString,INT> TextureNameToIndexMap;
	for( INT ListIndex = 0; ListIndex < ListControl->GetItemCount(); ++ListIndex )
	{
		FString Name = ListControl->GetColumnItemText( ListIndex, NameCol ).c_str();
		FString Path = ListControl->GetColumnItemText( ListIndex, PathCol ).c_str();
		
		if( Path.Len() )
		{
			TextureNameToIndexMap.Set( Path + TEXT(".") + Name, ListIndex );
		}
		else
		{
			TextureNameToIndexMap.Set( Name, ListIndex );
		}

		// Also unselect every item
		ListControl->Select( ListIndex, FALSE );
	}

	// Remember the first selected item.  We will make it the first visible item
	INT FirstSelected = INDEX_NONE;
	// Search each passed in texture for an index in the map.  If we find one, we select it in the list
	for( INT TextureIndex = 0; TextureIndex < InTextures.Num(); ++TextureIndex )
	{
		UObject* Texture = InTextures( TextureIndex );
		// Ensure we actually are looking at a texture
		if( Texture->IsA(UTexture::StaticClass() ) )
		{
			const FString PathName = Texture->GetPathName();
			INT* Index = TextureNameToIndexMap.Find( PathName );
			// If an index was found and the user hasnt hidden the asset we are looking for, select it in the list.
			if( Index && !HiddenAssetNames.Find( PathName) )
			{
				ListControl->Select( *Index, TRUE );
				if( FirstSelected == INDEX_NONE )
				{
					// This is the first selected item
					FirstSelected = *Index;
				}
			}
		}
	}

	// Make sure the first selected item is visible
	if( FirstSelected != INDEX_NONE )
	{
		ListControl->EnsureVisible( FirstSelected );
	}
	ListControl->Thaw();

	return FirstSelected != INDEX_NONE;
}

void WxTextureStatsBrowser::UpdateListItem(const FUniqueTextureStats &Stats)
{
	const INT ExistingStatIndex = AllStats.FindItemIndex(Stats);
	if(ExistingStatIndex == INDEX_NONE)
	{
		AllStats.AddItem(Stats);
	}
}


#if WITH_UE3_NETWORKING
/**
* Helper class to receive packets from the attached [remote]client to get assets stats
*/
struct FUdpGetAssetStats : public FUdpLink
{
	// constructor
	FUdpGetAssetStats(WxTextureStatsBrowser &InParent, UINT InOpenTargetConnections) 
		:Parent(InParent), OpenTargetConnections(InOpenTargetConnections)
	{
		StartTime = appSeconds();
	}

	UBOOL IsTimeOut() const
	{
		// all data needs to come in within n seconds
		return appSeconds() - StartTime > 2.0;
	}

	UBOOL IsWithoutOpenConnections() const
	{
		return OpenTargetConnections <= 0;
	}

	// interface FUdpLink ------------------------------------------------

	virtual void OnReceivedData(FIpAddr SrcAddr, BYTE* Data, INT Count)
	{
		// make an archive to read from
		FNboSerializeFromBuffer PayloadReader(Data, Count);

		if(Count != 1)
		{
			// stats of one asset 
			FString FullyQualifiedPath;
			FString MaxDim;
			FString CurrentDim;
			FString TexType;		// e.g. "2D", "Cube", ""
			DWORD FormatId;
			DWORD GroupId;
			FLOAT CurrentKB;
			FLOAT FullyLoadedKB;
			FLOAT LastTimeRendered;
			DWORD NumUses;
			INT	  LODBias;

			PayloadReader >> FullyQualifiedPath >> MaxDim >> CurrentDim >> TexType >> FormatId >> GroupId >> FullyLoadedKB >> CurrentKB >> LastTimeRendered >> NumUses >> LODBias;

			FUniqueTextureStats Stats(FullyQualifiedPath, MaxDim, CurrentDim, TexType, "", (EPixelFormat)FormatId, (TextureGroup)GroupId, FLT_MAX, FLT_MAX, FullyLoadedKB, CurrentKB, LastTimeRendered, NumUses, LODBias); 

			Parent.UpdateListItem(Stats);
		}
		else
		{
			// end terminator
			--OpenTargetConnections;
		}
	}

private: // ----------------------------------------------------

	INT							OpenTargetConnections;
	DOUBLE						StartTime;
	WxTextureStatsBrowser &		Parent;
};
#endif // WITH_UE3_NETWORKING




void WxTextureStatsBrowser::UpdateListFromRemoteCapture()
{	
#if WITH_UE3_NETWORKING

#define ASSETSTATS_PORT 9987	

	if( RemoteConsoleIndex == INDEX_NONE )
	{
		warnf( TEXT("No remote console chosen") );
		return;
	}

	// Get the console to gather stats from.
	FConsoleSupport* ConsoleSupport = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(RemoteConsoleIndex);

	if( !ConsoleSupport )
	{
		warnf(TEXT("Couldn't bind to console support DLL."));
		return;
	}

	// Get the selected targets for this console.  There should really only be one.
	INT NumTargets = ConsoleSupport->GetTargets(NULL);
	TArray<TARGETHANDLE> SelectedMenuItems(NumTargets);

	ConsoleSupport->GetMenuSelectedTargets(&SelectedMenuItems(0), NumTargets);

	// Determine our IP address.  We send this to the console so it knows which IP to send back to.
	FInternetIpAddr LocalAddr;
	GSocketSubsystem->GetLocalHostAddr( *GLog, LocalAddr );
	FIpAddr IpAddr = LocalAddr.GetAddress();
	
	if( NumTargets > 0 )
	{
		// Send the console a command to the first selected target, telling it to gather texture stats.  Send it the IP and port number as it the console not have that information in exec commands.
		ConsoleSupport->SendConsoleCommand( SelectedMenuItems(0), *FString::Printf( TEXT("REMOTETEXTURESTATS %d %d"), IpAddr.Addr, ASSETSTATS_PORT ) );
	}
	
	// client will sent back the data in UDP packets
	FUdpGetAssetStats ToClientConnection( *this, 1 );

	if (!ToClientConnection.BindPort(ASSETSTATS_PORT))
	{
		debugf(TEXT("Failed to bind to port %d to get AssetStats from remote machine."), ASSETSTATS_PORT);
		return;
	}

	// poll until timeout or end was reached
	while(!ToClientConnection.IsTimeOut())
	{
		ToClientConnection.Poll();

		if(ToClientConnection.IsWithoutOpenConnections())
		{
			// all targets delivered their data
			return;
		}
	}

	// timeout, maybe some target crashed
#endif // WITH_UE3_NETWORKING
}

/* Builds "Referencers" so wen can traverse the data. */
void WxTextureStatsBrowser::BuildReferencingData()
{
	const ETextureStatsBrowserListMode ListMode = (ETextureStatsBrowserListMode)TextureListModes->GetCurrentSelection();

	// Don't check for BSP mats if the list mode needs something to be selected
	if( ListMode != LM_SelectedActors && ListMode != LM_SelectedMaterials )
	{
		TArray<UObject*> BspMats;
		// materials to a temp list
		for (INT Index = 0; Index < GWorld->GetModel()->Surfs.Num(); Index++)
		{
			// No point showing the default material
			if (GWorld->GetModel()->Surfs(Index).Material != NULL)
			{
				BspMats.AddUniqueItem(GWorld->GetModel()->Surfs(Index).Material);
			}
		}
		// If any BSP surfaces are selected
		if (BspMats.Num() > 0)
		{
			FReferencedAssets* Referencer = new(Referencers) FReferencedAssets(GWorld->GetModel());

			// Now copy the array
			Referencer->AssetList = BspMats;
			ReferenceGraph.Set(GWorld->GetModel(), BspMats);
		}
	}

	// this is the maximum depth to use when searching for references
	const INT MaxRecursionDepth = 0;

	// Mark all objects so we don't get into an endless recursion
	for (FObjectIterator It; It; ++It)
	{
		// Skip the level, world, and any packages that should be ignored
		if ( ShouldSearchForAssets(*It,IgnoreClasses,IgnorePackages,FALSE) )
		{
			It->SetFlags(RF_TagExp);
		}
		else
		{
			It->ClearFlags(RF_TagExp);
		}
	}

	// Get the objects to search for texture references
	TArray< UObject* > ObjectsToSearch;
	GetObjectsForListMode( ListMode, ObjectsToSearch );

	TArray<UObject*> ObjectsToSkip;

	for( INT ObjIdx = 0; ObjIdx < ObjectsToSearch.Num(); ++ObjIdx )
	{
		UObject* CurrentObject = ObjectsToSearch( ObjIdx );
		if ( !ObjectsToSkip.ContainsItem(CurrentObject) )
		{
			// Create a new entry for this actor
			FReferencedAssets* Referencer = new(Referencers) FReferencedAssets(CurrentObject);

			// Add to the list of referenced assets
			FFindAssetsArchive(CurrentObject,Referencer->AssetList,&ReferenceGraph,MaxRecursionDepth,FALSE,FALSE);
		}
	}

	// Go through all referencers
	for (INT RefIndex = 0; RefIndex < Referencers.Num(); RefIndex++)
	{
		// Look at each referenced asset
		for (INT ReferencedIndex = 0; ReferencedIndex < Referencers(RefIndex).AssetList.Num(); ReferencedIndex++)
		{
			UPrimitiveComponent* ReferencedComponent = Cast<UPrimitiveComponent>(Referencers(RefIndex).AssetList(ReferencedIndex));
			if (ReferencedComponent)
			{
				// If the referenced asset is a primitive component get the materials used by the component
				TArray<UMaterialInterface*> UsedMaterials;
				ReferencedComponent->GetUsedMaterials( UsedMaterials );
				for ( INT MaterialIndex = 0; MaterialIndex < UsedMaterials.Num(); MaterialIndex++ )
				{
					// For each material, find the textures used by that material and add it to the stat list
					UMaterialInterface* CurrentMaterial = UsedMaterials(MaterialIndex);
					if ( CurrentMaterial )
					{
						TArray<UTexture*> UsedTextures;

						CurrentMaterial->GetUsedTextures(UsedTextures);
						for ( INT TextureIndex = 0; TextureIndex < UsedTextures.Num(); TextureIndex++ )
						{
							UTexture* CurrentTexture = UsedTextures(TextureIndex);

							// Check if the texture should be considered for stats before adding it to the stats list
							if(IsTextureValidForStats(CurrentTexture))
							{
								ReferencingObjects.AddUniqueItem(CurrentMaterial);
							}
						}
					}
				}
			}
		}
	}
}

void WxTextureStatsBrowser::UpdateListFromLoadedAssets()
{
	for (INT RefIndex = 0; RefIndex < Referencers.Num(); RefIndex++)
	{
		TArray<UObject*> &AssetList = Referencers(RefIndex).AssetList;

		// Look at each referenced asset
		for (INT ReferencedIndex = 0; ReferencedIndex < AssetList.Num(); ReferencedIndex++)
		{
			UObject* Asset = AssetList(ReferencedIndex);

			UTexture* CurrentTexture = Cast<UTexture>(Asset);

			if(IsTextureValidForStats(CurrentTexture))
			{
				AActor* ActorUsingTexture = Cast<AActor>(Referencers(RefIndex).Referencer);

				// referenced by an actor
				AddStatItem(*CurrentTexture, ActorUsingTexture);
			}

			UPrimitiveComponent* ReferencedComponent = Cast<UPrimitiveComponent>(Asset);
			if (ReferencedComponent)
			{
				// If the referenced asset is a primitive component get the materials used by the component
				TArray<UMaterialInterface*> UsedMaterials;
				ReferencedComponent->GetUsedMaterials( UsedMaterials );
				for(INT MaterialIndex = 0; MaterialIndex < UsedMaterials.Num(); MaterialIndex++)
				{
					// For each material, find the textures used by that material and add it to the stat list
					UMaterialInterface* CurrentMaterial = UsedMaterials(MaterialIndex);
					if(CurrentMaterial)
					{
						TArray<UTexture*> UsedTextures;

						CurrentMaterial->GetUsedTextures(UsedTextures);
						for(INT TextureIndex = 0; TextureIndex < UsedTextures.Num(); TextureIndex++)
						{
							UTexture* CurrentTexture = UsedTextures(TextureIndex);

							if(IsTextureValidForStats(CurrentTexture))
							{
								AActor* ActorUsingTexture = Cast<AActor>(Referencers(RefIndex).Referencer);

								// referenced by an material
								AddStatItem(*CurrentTexture, ActorUsingTexture);
							}
						}
					}
				}
			}
		}
	}
}

void WxTextureStatsBrowser::AddStatItem(UTexture &Tex, AActor* ActorUsingTexture)
{
	FUniqueTextureStats *Found = 0;
	{
		FUniqueTextureStats Test(&Tex);

		const INT ExistingStatIndex = AllStats.FindItemIndex(Test);

		if (ExistingStatIndex == INDEX_NONE)
		{
			AllStats.AddItem(Test);
			Found = &AllStats(AllStats.Num() - 1);
		}
		else
		{
			Found = &AllStats(ExistingStatIndex);
		}
	}

	check(Found);

	if(ActorUsingTexture)
	{
		// increase the number of times the texture is used and
		// update the materials and actors that use the texture.
		Found->NumUses++;
		ReferencingObjects.AddUniqueItem(ActorUsingTexture);
	}
}

/* Find all the actor referencing the given texture, even if only indirectly though a material. */
void WxTextureStatsBrowser::FindActorsUsingIt(UTexture &Texture, TArray<AActor *> &OutActors)
{
	// Go through all referencers
	for (INT RefIndex = 0; RefIndex < Referencers.Num(); RefIndex++)
	{
		AActor* ActorUsingTexture = Cast<AActor>(Referencers(RefIndex).Referencer);

		if(ActorUsingTexture)
		{
			TArray<UObject*> &AssetList = Referencers(RefIndex).AssetList;

			// Look at each referenced asset
			for (INT ReferencedIndex = 0; ReferencedIndex < AssetList.Num(); ReferencedIndex++)
			{
				UObject* Asset = AssetList(ReferencedIndex);

				UTexture* CurrentTexture = Cast<UTexture>(Asset);

				if(CurrentTexture == &Texture)
				{
					OutActors.AddUniqueItem(ActorUsingTexture);
				}

				UPrimitiveComponent* ReferencedComponent = Cast<UPrimitiveComponent>(Asset);
				if (ReferencedComponent)
				{
					// If the referenced asset is a primitive component get the materials used by the component
					TArray<UMaterialInterface*> UsedMaterials;
					ReferencedComponent->GetUsedMaterials( UsedMaterials );
					for(INT MaterialIndex = 0; MaterialIndex < UsedMaterials.Num(); MaterialIndex++)
					{
						// For each material, find the textures used by that material and add it to the stat list
						UMaterialInterface* CurrentMaterial = UsedMaterials(MaterialIndex);
						if(CurrentMaterial)
						{
							TArray<UTexture*> UsedTextures;

							CurrentMaterial->GetUsedTextures(UsedTextures);
							for(INT TextureIndex = 0; TextureIndex < UsedTextures.Num(); TextureIndex++)
							{
								UTexture* CurrentTexture = UsedTextures(TextureIndex);

								if(CurrentTexture == &Texture)
								{
									OutActors.AddUniqueItem(ActorUsingTexture);
								}
							}
						}
					}
				}
			}
		}
	}
}

/* Find all the materials referencing the given texture. */
void WxTextureStatsBrowser::FindMaterialsUsingIt(UTexture &Texture, TArray<UObject *> &OutMaterials)
{
	// Referencers should be already populated

	// Go through all materials that reference textures
	for (INT RefIndex = 0; RefIndex < ReferencingObjects.Num(); RefIndex++)
	{
		UMaterialInterface* CurrentMaterial = Cast<UMaterialInterface>(ReferencingObjects(RefIndex));
		
		if(CurrentMaterial)
		{
			TArray<UTexture*> UsedTextures;

			CurrentMaterial->GetUsedTextures(UsedTextures);
			for(INT TextureIndex = 0; TextureIndex < UsedTextures.Num(); TextureIndex++)
			{
				UTexture* CurrentTexture = UsedTextures(TextureIndex);

				if(CurrentTexture == &Texture)
				{
					OutMaterials.AddUniqueItem(CurrentMaterial);
				}
			}
		}
	}
}

/**
 * Updates the primitives list with new data
 */
void WxTextureStatsBrowser::BuildList()
{
	ReferencingObjects.Empty();
	AllStats.Empty();
	Referencers.Empty();
	ReferenceGraph.Empty();
	TexturesToIgnore.Empty();

	// Do nothing unless we are visible
	if( IsShownOnScreen() == TRUE && !GIsPlayInEditorWorld && GWorld && GWorld->CurrentLevel )
	{
		const ETextureStatsBrowserListMode ListMode = (ETextureStatsBrowserListMode)TextureListModes->GetCurrentSelection();

		if(ListMode == LM_CookerStatistics)
		{
			UpdateListFromCookerData();
		}
		else if(ListMode == LM_RemoteCapture)
		{
			BuildReferencingData();
			UpdateListFromRemoteCapture();
		}
		else
		{
			BuildReferencingData();
			UpdateListFromLoadedAssets();
		}
	}
}

void WxTextureStatsBrowser::RefreshList()
{
	BeginUpdate();

	// Do nothing unless we are visible
	if( IsShownOnScreen() == TRUE && !GIsPlayInEditorWorld && GWorld && GWorld->CurrentLevel )
	{
		const ETextureStatsBrowserListMode ListMode = (ETextureStatsBrowserListMode)TextureListModes->GetCurrentSelection();

		if(ColumnsListMode != ListMode)
		{
			ColumnsListMode = ListMode;
			InsertColumns();
			SetAutoColumnWidth();
		}

		FUniqueTextureStats CombinedStats;

		for (INT StatIndex = 0; StatIndex < AllStats.Num(); StatIndex++)
		{
			CombinedStats.AddInfo(AllStats(StatIndex));
		}

		// Find the index of the top most currently visible entry in the list control
		INT HitTestResultFlags = 0;
		const INT TopmostVisibleIndex = ListControl->HitTest( wxPoint(0,0), HitTestResultFlags );

		// Get the bounds of the top most visible entry to help us scroll to it after the list is repopulated
		WxRect ItemBounds;
		if( TopmostVisibleIndex != wxNOT_FOUND )
		{
			ListControl->GetItemRect( TopmostVisibleIndex, ItemBounds, wxLIST_RECT_LABEL );
		}

		// Get the current scroll position before deleting all entries.
		const INT ScrollBarPos = ListControl->GetScrollPos( wxVERTICAL );
		
		ListControl->Freeze();
		{			
			TSet<FString> SelectedItemNames;

			// Remember the names of whatever we have selected.  
			INT SelectedIndex = ListControl->GetFirstSelected();
			while( SelectedIndex != -1 )
			{
				// Add each selected item to a list before we delete the contents of the list.
				FString Name = GetFullyQualifiedName( SelectedIndex );
				SelectedItemNames.Add( Name	);
				SelectedIndex = ListControl->GetNextSelected( SelectedIndex );
			}

			// Clear existing items.
			ListControl->DeleteAllItems();

			// Sort array of all stats
			Sort<USE_COMPARE_CONSTREF(FUniqueTextureStats,TextureStatsBrowser)>( AllStats.GetTypedData(), AllStats.Num() );

			UINT HiddenCount = 0;

			// Add sorted items.
			for( INT StatsIndex = 0; StatsIndex < AllStats.Num(); StatsIndex++ )
			{
				const FUniqueTextureStats& StatsEntry = AllStats(StatsIndex);

				FString FullyQualifiedName = StatsEntry.GetFullyQualifiedName();

				// Only show stats in the list which shouldnt be hidden and pass the current search filter.  
				FString ColumnData;
				INT CurrentSelection = SearchModes->GetCurrentSelection();
				StatsEntry.GetColumnData( (ETextureStatsBrowserColumns)SearchModes->GetCurrentSelection(), ColumnData );
				if( HiddenAssetNames.Find(FullyQualifiedName) || ( !SearchBox->IsEmpty() && ColumnData.InStr( SearchBox->GetValue().c_str(), FALSE, TRUE ) == INDEX_NONE ) )
				{
					++HiddenCount;
					continue;
				}

				long ItemIndex = ListControl->InsertItem( 0, TEXT("") );

				INT DstColIdx = 0;
				for(INT SrcColIdx = 0; SrcColIdx < TSBC_MAX; SrcColIdx++)
				{
					if(IsColumnShownInThatMode(ListMode, (ETextureStatsBrowserColumns)SrcColIdx))
					{
						FString Text;
						StatsEntry.GetColumnData((ETextureStatsBrowserColumns)SrcColIdx, Text);
						ListControl->SetItem(ItemIndex, DstColIdx++, *Text);
					}
				}

				// See if this asset was previously selected. If it was, select it again
				if( SelectedItemNames.Find( StatsEntry.GetFullyQualifiedName() ) )
				{
					ListControl->Select( ItemIndex, TRUE );
				}
			}			

			if(HiddenCount)
			{
				UnhideAllButton->Enable();
			}
			else
			{
				UnhideAllButton->Disable();
			}
		}
		ListControl->Thaw();

		// Restore the scroll bar position after refreshing the ListControl by scrolling a number of pixels equal to
		// the height of one item multiplied by the cached scroll bar position
		if( ScrollBarPos <= ListControl->GetItemCount() && TopmostVisibleIndex != wxNOT_FOUND )
		{
			ListControl->ScrollList( 0, ScrollBarPos * ItemBounds.GetHeight());
		}

		OnSelectionChanged();

		// Update the combined stats control
		{
			FString CurrentKB;
			FString FullyLoadedKB;

			CombinedStats.GetColumnData(TSBC_CurrentKB, CurrentKB);
			CombinedStats.GetColumnData(TSBC_FullyLoadedKB, FullyLoadedKB);

			CombinedStatsControl->Freeze();
			CombinedStatsControl->SetValue(*(CurrentKB + TEXT(" / ") + FullyLoadedKB));
			CombinedStatsControl->Thaw();
		}
	}

	EndUpdate();
}

/**
 * Sets the size of the list control based upon our new size
 *
 * @param In the command that was sent
 */
void WxTextureStatsBrowser::OnSize( wxSizeEvent& In )
{
	// During the creation process a sizing message can be sent so don't
	// handle it until we are initialized
	if( bAreWindowsInitialized )
	{
		Panel->SetSize( GetClientRect() );
	}
}

/**
 * Handler for column click events
 *
 * @param In the command that was sent
 */
void WxTextureStatsBrowser::OnColumnClick( wxListEvent& In )
{
	INT ColumnIndex = In.GetColumn();
	if(ColumnIndex >= 0)
	{
		const ETextureStatsBrowserListMode ListMode = (const ETextureStatsBrowserListMode)TextureListModes->GetCurrentSelection();
		ETextureStatsBrowserColumns ColumnEnum = GetColumnEnumFromIndex(ListMode, ColumnIndex);

		if( WxTextureStatsBrowser::PrimarySortIndex == ColumnEnum )
		{
			check( ColumnEnum < ARRAY_COUNT(WxTextureStatsBrowser::CurrentSortOrder) );
			WxTextureStatsBrowser::CurrentSortOrder[ColumnEnum] *= -1;
		}
		WxTextureStatsBrowser::PrimarySortIndex = (ETextureStatsBrowserColumns)ColumnEnum;

		RefreshList();
	}
}

/**
 * Handler for column right click events
 *
 * @param In the command that was sent
 */
void WxTextureStatsBrowser::OnColumnRightClick( wxListEvent& In )
{
	INT ColumnIndex  = In.GetColumn();
	if(ColumnIndex >= 0)
	{
		const ETextureStatsBrowserListMode ListMode = (const ETextureStatsBrowserListMode)TextureListModes->GetCurrentSelection();
		ETextureStatsBrowserColumns ColumnEnum = GetColumnEnumFromIndex(ListMode, ColumnIndex);

		if( WxTextureStatsBrowser::SecondarySortIndex == ColumnEnum )
		{
			check( ColumnEnum < ARRAY_COUNT(WxTextureStatsBrowser::CurrentSortOrder) );
			WxTextureStatsBrowser::CurrentSortOrder[ColumnEnum] *= -1;
		}
		WxTextureStatsBrowser::SecondarySortIndex = ColumnEnum;

		RefreshList();
	}
}

void WxTextureStatsBrowser::OnItemRightClick( wxListEvent& In )
{
	WxTextureStatsContextMenu ContextMenu(*this);
	FTrackPopupMenu tpm( this, &ContextMenu );
	tpm.Show();
}

INT GetColumnIndexFromEnum(ETextureStatsBrowserListMode ListMode, ETextureStatsBrowserColumns Enum)
{
	INT DstColIdx = 0;
	for(INT SrcColIdx = 0; SrcColIdx < TSBC_MAX; SrcColIdx++)
	{
		if((ETextureStatsBrowserColumns)SrcColIdx == Enum)
		{
			return DstColIdx;
		}

		if(IsColumnShownInThatMode(ListMode, (ETextureStatsBrowserColumns)SrcColIdx))
		{
			DstColIdx++;
		}
	}

	check(0);
	return 0;
}

ETextureStatsBrowserColumns GetColumnEnumFromIndex(ETextureStatsBrowserListMode ListMode, INT ColumnIndex)
{
	INT DstColIdx = 0;
	for(INT SrcColIdx = 0; SrcColIdx < TSBC_MAX; SrcColIdx++)
	{
		if(IsColumnShownInThatMode(ListMode, (ETextureStatsBrowserColumns)SrcColIdx))
		{
			if(ColumnIndex == DstColIdx)
			{
				return (ETextureStatsBrowserColumns)SrcColIdx;
			}

			DstColIdx++;
		}
	}
	check(0);
	return TSBC_Name;
}

FString WxTextureStatsBrowser::GetFullyQualifiedName(UINT Index) const
{
	const ETextureStatsBrowserListMode ListMode = (const ETextureStatsBrowserListMode)TextureListModes->GetCurrentSelection();

	INT NameCol = GetColumnIndexFromEnum(ListMode, TSBC_Name);
	INT PathCol = GetColumnIndexFromEnum(ListMode, TSBC_Path);

	wxString Name = ListControl->GetColumnItemText(Index, NameCol);
	wxString Path = ListControl->GetColumnItemText(Index, PathCol);

	if(Path.Len())
	{
		return 	FString(Path) + TEXT(".") + FString(Name);
	}
	else
	{
		return FString(Name);
	}
}

/**
 * Handler for item activation (double click) event
 *
 * @param In the command that was sent
 */
void WxTextureStatsBrowser::OnItemActivated( wxListEvent& In )
{
	FString	ResourceName  = GetFullyQualifiedName(In.GetIndex());

	UTexture*	Texture	= FindObject<UTexture>( NULL, *ResourceName );

	if (Texture)
	{
		TArray<UObject*> Objects;
		Objects.AddItem(Texture);

		// Sync the generic browser to the object list.
		GApp->EditorFrame->SyncBrowserToObjects(Objects);
	}
}

void WxTextureStatsBrowser::OnItemSelectionChanged( wxListEvent& In )
{
	OnSelectionChanged();
}

void WxTextureStatsBrowser::OnSyncToActors( wxCommandEvent &In )
{
	FString	ResourceName  = GetFullyQualifiedName(ListControl->GetFocusedItem());
	UTexture*	Texture	= FindObject<UTexture>( NULL, *ResourceName );

	// Make sure this action is undoable.
	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("TextureStatsBrowser_RightClick_SyncToActors")) );

	AActor* FirstActor = NULL;
	
	if( Texture )
	{
		TArray< AActor* > ActorsToSelect;

		FindActorsUsingIt(*Texture, ActorsToSelect);

		if(ActorsToSelect.Num())
		{
			FirstActor = ActorsToSelect(0);
		}

		// Deselect everything.
		GUnrealEd->SelectNone( TRUE, TRUE );
		// Select all actors in he list
		for( INT ActorIdx = 0; ActorIdx < ActorsToSelect.Num(); ++ActorIdx )
		{
			GUnrealEd->SelectActor( ActorsToSelect( ActorIdx ), TRUE, NULL, FALSE );
		}
		GUnrealEd->NoteSelectionChange();
	}
	
	if (FirstActor)
	{
		// Point cam at first actor
		GEditor->MoveViewportCamerasToActor(*FirstActor, FALSE );
	}
}

void WxTextureStatsBrowser::OnSyncToMaterials( wxCommandEvent &In )
{
	FString	ResourceName  = GetFullyQualifiedName(ListControl->GetFocusedItem());
	UTexture* Texture = FindObject<UTexture>(NULL, *ResourceName);

	if(Texture)
	{
		TArray<UObject *> Materials;

		FindMaterialsUsingIt(*Texture, Materials); 
		GApp->EditorFrame->SyncBrowserToObjects(Materials);
	}
}


void WxTextureStatsBrowser::OnInvertSelection( wxCommandEvent &In )
{
	ListControl->Freeze();

	UINT Count = ListControl->GetItemCount();

	for(UINT i = 0; i < Count; ++i)
	{
		ListControl->Select(i, !ListControl->IsSelected(i));
	}

	ListControl->Thaw();

	OnSelectionChanged();
}

void WxTextureStatsBrowser::OnHideSelection( wxCommandEvent &In )
{
	UINT SelectionIndex = ListControl->GetFirstSelected();

	while(SelectionIndex != -1)
	{
		FString	ResourceName = GetFullyQualifiedName(SelectionIndex);

		HiddenAssetNames.Add(ResourceName);
		SelectionIndex = ListControl->GetNextSelected(SelectionIndex);
	}

	RefreshList();

	OnSelectionChanged();
}

void WxTextureStatsBrowser::OnUnhideAll( wxCommandEvent &In )
{
	HiddenAssetNames.Empty();

	RefreshList();

	OnSelectionChanged();
}


/**
 * Called when the texture list mode combo box selection changes 
 */
void WxTextureStatsBrowser::OnTextureListModeChanged( wxCommandEvent& In )
{
	const ETextureStatsBrowserListMode ListMode = (const ETextureStatsBrowserListMode)TextureListModes->GetCurrentSelection();

	if(ListMode == LM_CookerStatistics)
	{
		// default folder in Xbox360
#define COOKED_FOLDER				TEXT("CookedXenon")

		FFilename CookedPackageWildcard = (appGameDir() + COOKED_FOLDER) * TEXT("GlobalPersistentCookerData.upk");

		WxFileDialog OpenFileDialog(this, 
			*LocalizeUnrealEd("TextureStatsBrowser_ImportCookerData"),
			TEXT(""),
			*CookerPathData,
			TEXT("Package files (*.upk)|*.upk|All files|*.*"),
			wxOPEN | wxFILE_MUST_EXIST,
			wxDefaultPosition);

		if(OpenFileDialog.ShowModal() == wxID_OK)
		{
			CookerPathData = OpenFileDialog.GetPath();

			// NormalizePathSeparators
			for( TCHAR* p=(TCHAR*)*CookerPathData; *p; p++ )
			{
				if( *p == '\\' || *p == '/' )
				{
					*p = PATH_SEPARATOR[0];
				}
			}
		}
	}

	Update();
	// Save the current list mode between editor sessions
	GConfig->SetInt( TEXT("TextureStatsBrowser"), TEXT("TextureListMode"), TextureListModes->GetCurrentSelection(), GEditorUserSettingsIni );
}

/** 
 * Called when a new search mode is selected.
 */
void WxTextureStatsBrowser::OnSearchModeChanged( wxCommandEvent& In )
{
	// Simply refresh the list based on the new search mode
	RefreshList();
}

/**
 * Called when the text inside the search box changes 
 * This triggers a timer.  If no more text changes happen before the timer runs out, we update the list based on the text
 */
void WxTextureStatsBrowser::OnSearchTextChanged( wxCommandEvent& In )
{
	// If the timer was already running, reset the timer and start over
	if( SearchTimer.IsRunning() )
	{
		SearchTimer.Stop();		
	}
	SearchTimer.Start( SearchTextUpdateDelay, wxTIMER_ONE_SHOT );
}

/**
 * Called when the timer runs out, which means we have spent enough time waiting for more input and we should refresh the list based on the currently entered text
 */
void WxTextureStatsBrowser::OnTimer( wxTimerEvent& In )
{
	RefreshList();
}
/**
 * Called when a console is changed in the remote menu
 */
void WxTextureStatsBrowser::OnChangeConsole( wxCommandEvent& In )
{
	const INT ConsoleIndex = In.GetId() - ID_TEXTURESTATSBROWSER_CONSOLEMENU_START;
	
	// Set the console index to remote capture from
	RemoteConsoleIndex = INDEX_NONE;
	if( ConsoleIndex >= 0 && ConsoleIndex < FConsoleSupportContainer::GetConsoleSupportContainer()->GetNumConsoleSupports() )
	{
		FConsoleSupport* Console = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(ConsoleIndex);
		GConfig->SetString( TEXT("TextureStatsBrowser"), TEXT("RemoteConsoleName"), Console->GetPlatformName(), GEditorUserSettingsIni );
		RemoteConsoleIndex = ConsoleIndex;
	}
}


/** 
 * Gets a list of objects that should be searched for texture references
 *
 * @param ListMode	The listing mode for texture references.  The objects returned depends on this mode
 * @param OutObjectsToSearch	The list of objects to search
 */
void WxTextureStatsBrowser::GetObjectsForListMode( ETextureStatsBrowserListMode ListMode, TArray<UObject*>& OutObjectsToSearch ) const
{
	if( ListMode == LM_SelectedActors )
	{
		// In this mode only get selected actors
		for( FSelectedActorIterator It; It; ++It )
		{
			OutObjectsToSearch.AddItem( *It );
		}
	}
	else if( ListMode == LM_SelectedMaterials )
	{
		// In this mode only get selected materials
		USelection* SelectedObjects = GEditor->GetSelectedObjects();
		for ( USelection::TObjectIterator It( SelectedObjects->ObjectItor() ); It; ++It )
		{
			UMaterialInterface* Material = Cast<UMaterialInterface>(*It);
			if( Material )
			{
				OutObjectsToSearch.AddItem( Material );
			}
		}
	}
	else if( ListMode == LM_CurrentLevel )
	{
		// In this mode get all actors in the current level
		for (INT ActorIdx = 0; ActorIdx < GWorld->CurrentLevel->Actors.Num(); ++ActorIdx )
		{
			OutObjectsToSearch.AddItem( GWorld->CurrentLevel->Actors(ActorIdx) );
		}
	}
	else if( ListMode == LM_AllLevels || ListMode == LM_RemoteCapture)
	{
		// In this mode get all actors in all levels
		for( INT LevelIdx = 0; LevelIdx < GWorld->Levels.Num(); ++LevelIdx )
		{
			const ULevel* CurrentLevel = GWorld->Levels( LevelIdx );
			for (INT ActorIdx = 0; ActorIdx < CurrentLevel->Actors.Num(); ++ActorIdx )
			{
				OutObjectsToSearch.AddItem( CurrentLevel->Actors(ActorIdx) );
			}
		}
	}
}

WxTextureStatsContextMenu::WxTextureStatsContextMenu(WxTextureStatsBrowser &Parent) 
{
	UINT SelectionIndex = Parent.ListControl->GetFirstSelected();
	UBOOL bSomethingIsSelected = SelectionIndex != -1;
	UBOOL bSingleItemSelected = FALSE;

	if(bSomethingIsSelected && Parent.ListControl->GetNextSelected(SelectionIndex) == -1)
	{
		bSingleItemSelected = TRUE;
	}

	if(bSingleItemSelected)
	{
		Append( ID_TEXTURESTATS_POPUPMENU_SYNCACTORS, *LocalizeUnrealEd("TextureStatsBrowser_RightClick_SyncToActors"), TEXT("") );
		Append( ID_TEXTURESTATS_POPUPMENU_SYNCMATERIALS, *LocalizeUnrealEd("TextureStatsBrowser_RightClick_SyncToMaterials"), TEXT("") );
	}

	if(bSomethingIsSelected)
	{
		Append( ID_TEXTURESTATS_POPUPMENU_INVERTSELECTION, *LocalizeUnrealEd("TextureStatsBrowser_InvertSelection"), TEXT("") );
		Append( ID_TEXTURESTATS_POPUPMENU_HIDESELECTION, *LocalizeUnrealEd("TextureStatsBrowser_HideSelection"), TEXT("") );
	}

	if(Parent.UnhideAllButton->IsEnabled())
	{
		Append( ID_TEXTURESTATS_POPUPMENU_UNHIDEALL, *LocalizeUnrealEd("TextureStatsBrowser_UnhideAll"), TEXT("") );
	}
}

void WxTextureStatsBrowser::OnSelectionChanged()
{
	if(ListControl->GetFirstSelected() == -1)
	{
		HideSelectionButton->Disable();
	}
	else
	{
		HideSelectionButton->Enable();
	}
}
