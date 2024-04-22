/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineProcBuildingClasses.h"
#include "BuildingStatsBrowser.h"
#include "ScopedTransaction.h"

/** Whether the stats should be dumped to CSV during the next update. */
UBOOL GDumpBuildingStatsToCSVDuringNextUpdate;

BEGIN_EVENT_TABLE(WxBuildingStatsBrowser,WxBrowser)
	EVT_SIZE(WxBuildingStatsBrowser::OnSize)
    EVT_LIST_COL_CLICK(ID_BUILDINGSTATSBROWSER_LISTCONTROL, WxBuildingStatsBrowser::OnColumnClick)
	EVT_LIST_COL_RIGHT_CLICK(ID_BUILDINGSTATSBROWSER_LISTCONTROL, WxBuildingStatsBrowser::OnColumnRightClick)
    EVT_LIST_ITEM_ACTIVATED(ID_BUILDINGSTATSBROWSER_LISTCONTROL, WxBuildingStatsBrowser::OnItemActivated)
END_EVENT_TABLE()

/** Current sort order (-1 or 1) */
INT WxBuildingStatsBrowser::CurrentSortOrder[BSBC_MAX] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
/** Primary index/ column to sort by */
INT WxBuildingStatsBrowser::PrimarySortIndex = BSBC_LightmapMemBytes;
/** Secondary index/ column to sort by */
INT WxBuildingStatsBrowser::SecondarySortIndex = BSBC_NumInstancedTris;

/**
 * Inserts a column into the control.
 *
 * @param	ColumnId		Id of the column to insert
 * @param	ColumnHeader	Header/ description of the column.
 */
void WxBuildingStatsBrowser::InsertColumn( EBuildingStatsBrowserColumns ColumnId, const TCHAR* ColumnHeader, int Format )
{
	ListControl->InsertColumn( ColumnId, ColumnHeader, Format );
	new(ColumnHeaders) FString(ColumnHeader);
}

/**
 * Forwards the call to our base class to create the window relationship.
 * Creates any internally used windows after that
 *
 * @param DockID the unique id to associate with this dockable window
 * @param FriendlyName the friendly name to assign to this window
 * @param Parent the parent of this window (should be a Notebook)
 */
void WxBuildingStatsBrowser::Create(INT DockID,const TCHAR* FriendlyName, wxWindow* Parent)
{
	// Let our base class start up the windows
	WxBrowser::Create(DockID,FriendlyName,Parent);	

	// Add a menu bar
	MenuBar = new wxMenuBar();
	// Append the docking menu choices
	WxBrowser::AddDockingMenu(MenuBar);
	
	// Create list control
	ListControl = new WxListView( this, ID_BUILDINGSTATSBROWSER_LISTCONTROL, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_HRULES | wxLC_VRULES );

	// Insert columns.
	InsertColumn( BSBC_Name,						*LocalizeUnrealEd("BuildingStatsBrowser_Name")												);
	InsertColumn( BSBC_Ruleset,						*LocalizeUnrealEd("BuildingStatsBrowser_Ruleset")											);
	InsertColumn( BSBC_NumStaticMeshComps,			*LocalizeUnrealEd("BuildingStatsBrowser_NumStaticMeshComps"),			wxLIST_FORMAT_RIGHT	);
	InsertColumn( BSBC_NumInstancedStaticMeshComps,	*LocalizeUnrealEd("BuildingStatsBrowser_NumInstancedStaticMeshComps"),	wxLIST_FORMAT_RIGHT	);
	InsertColumn( BSBC_NumInstancedTris,			*LocalizeUnrealEd("BuildingStatsBrowser_NumInstancedTris"),				wxLIST_FORMAT_RIGHT	);
	InsertColumn( BSBC_LightmapMemBytes,			*LocalizeUnrealEd("BuildingStatsBrowser_LightmapMemBytes"),				wxLIST_FORMAT_RIGHT	);
	InsertColumn( BSBC_ShadowmapMemBytes,			*LocalizeUnrealEd("BuildingStatsBrowser_ShadowmapMemBytes"),			wxLIST_FORMAT_RIGHT	);
	InsertColumn( BSBC_LODDiffuseMemBytes,			*LocalizeUnrealEd("BuildingStatsBrowser_LODDiffuseMemBytes"),			wxLIST_FORMAT_RIGHT	);
	InsertColumn( BSBC_LODLightingMemBytes,			*LocalizeUnrealEd("BuildingStatsBrowser_LODLightingMemBytes"),			wxLIST_FORMAT_RIGHT	);
}

/**
 * Called when the browser is getting activated (becoming the visible
 * window in it's dockable frame).
 */
void WxBuildingStatsBrowser::Activated()
{
	// Let the super class do it's thing
	WxBrowser::Activated();
	Update();
}


static INT CompareEntries(const FPBMemUsageInfo& A, const FPBMemUsageInfo& B)
{
	INT Result = WxBuildingStatsBrowser::CurrentSortOrder[WxBuildingStatsBrowser::PrimarySortIndex] * A.Compare(B, WxBuildingStatsBrowser::PrimarySortIndex);
	if(Result == 0)
	{
		Result = WxBuildingStatsBrowser::CurrentSortOrder[WxBuildingStatsBrowser::SecondarySortIndex] * A.Compare(B, WxBuildingStatsBrowser::SecondarySortIndex);
	}
	return Result;
}

// Sort helper class.
IMPLEMENT_COMPARE_CONSTREF( FPBMemUsageInfo, BuildingStatsBrowser, { return CompareEntries(A, B); });

/**
 * Returns whether the passed in object is part of a visible level.
 *
 * @param Object	object to check
 * @return TRUE if object is inside (as defined by chain of outers) in a visible level, FALSE otherwise
 */
static UBOOL IsInVisibleLevel( UObject* Object )
{
	check( Object );
	UObject* ObjectPackage = Object->GetOutermost();
	for( INT LevelIndex=0; LevelIndex<GWorld->Levels.Num(); LevelIndex++ )
	{
		ULevel* Level = GWorld->Levels(LevelIndex);
		if( Level && Level->GetOutermost() == ObjectPackage )
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Tells the browser to update itself
 */
void WxBuildingStatsBrowser::Update()
{
	UpdateList(TRUE);
}

/**
 * Sets auto column width. Needs to be called after resizing as well.
 */
void WxBuildingStatsBrowser::SetAutoColumnWidth()
{
	// Set proper column width
	for( INT ColumnIndex=0; ColumnIndex<ARRAY_COUNT(WxBuildingStatsBrowser::CurrentSortOrder); ColumnIndex++ )
	{
		INT Width = 0;
		ListControl->SetColumnWidth( ColumnIndex, wxLIST_AUTOSIZE );
		Width = Max( ListControl->GetColumnWidth( ColumnIndex ), Width );
		ListControl->SetColumnWidth( ColumnIndex, wxLIST_AUTOSIZE_USEHEADER );
		Width = Max( ListControl->GetColumnWidth( ColumnIndex ), Width );
		ListControl->SetColumnWidth( ColumnIndex, Width );
	}
}

/**
 * Updates the primitives list with new data
 *
 * @param bResizeColumns	Whether or not to resize the columns after updating data.
 */
void WxBuildingStatsBrowser::UpdateList(UBOOL bResizeColumns)
{
	BeginUpdate();

	// Do nothing unless we are visible
	if( IsShownOnScreen() == TRUE && !GIsPlayInEditorWorld && GWorld && GWorld->CurrentLevel )
	{
		TArray<FPBMemUsageInfo> AllStats;
		FPBMemUsageInfo	CombinedStats(0);

		// Iterate over all static mesh components.
		for( TObjectIterator<AProcBuilding> It; It; ++It )
		{
			AProcBuilding* Building = *It;

			// If its in a visible level, and its the 'base' building of a group
			if( IsInVisibleLevel(Building) && (Building->GetBaseMostBuilding() == Building) )
			{
				FPBMemUsageInfo Stats = Building->GetBuildingMemUsageInfo();
				AllStats.AddItem(Stats);

				CombinedStats.AddInfo(Stats);
			}
		}

		ListControl->Freeze();
		{
			// Clear existing items.
			ListControl->DeleteAllItems();

			// Sort array of all stats
			Sort<USE_COMPARE_CONSTREF(FPBMemUsageInfo,BuildingStatsBrowser)>( AllStats.GetTypedData(), AllStats.Num() );

			// Add sorted items.
			for( INT StatsIndex=0; StatsIndex<AllStats.Num(); StatsIndex++ )
			{
				const FPBMemUsageInfo& StatsEntry = AllStats(StatsIndex);

				long ItemIndex = ListControl->InsertItem( 0, *StatsEntry.Building->GetName() );
				ListControl->SetItem( ItemIndex, BSBC_Name,	*StatsEntry.Building->GetPathName() );
				ListControl->SetItem( ItemIndex, BSBC_Ruleset,	*StatsEntry.Ruleset->GetPathName() );

				for(INT ColIdx=2; ColIdx<BSBC_MAX; ColIdx++)
				{
					ListControl->SetItem( ItemIndex, ColIdx, *StatsEntry.GetColumnDataString(ColIdx) );
				}
			}

			// Add combined stats.
			if( TRUE )
			{
				long ItemIndex = ListControl->InsertItem( 0, TEXT("") );
				ListControl->SetItem( ItemIndex, BSBC_Name, *LocalizeUnrealEd("BuildingStatsBrowser_CombinedStats") );
				ListControl->SetItem( ItemIndex, BSBC_Ruleset, TEXT("") );

				for(INT ColIdx=2; ColIdx<BSBC_MAX; ColIdx++)
				{
					ListControl->SetItem( ItemIndex, ColIdx, *CombinedStats.GetColumnDataString(ColIdx) );
				}
			}

			// Dump to CSV file if wanted.
			if( GDumpBuildingStatsToCSVDuringNextUpdate )
			{
				// Number of rows == number of primitive stats plus combined stat.
				INT NumRows = AllStats.Num() + 1;
				DumpToCSV( NumRows );
			}

			// Set proper column width.
			if(bResizeColumns == TRUE)
			{
				SetAutoColumnWidth();
			}
		}
		ListControl->Thaw();
	}

	EndUpdate();
}

/**
 * Dumps current stats to CVS file.
 *
 * @param NumRows	Number of rows to dump
 */
void WxBuildingStatsBrowser::DumpToCSV( INT NumRows )
{
	check(BSBC_MAX == ColumnHeaders.Num());

	// Create string with system time to create a unique filename.
	INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
	appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );
	FString	CurrentTime = FString::Printf(TEXT("%i.%02i.%02i-%02i.%02i.%02i"), Year, Month, Day, Hour, Min, Sec );

	// CSV: Human-readable spreadsheet format.
	FString CSVFilename	= FString::Printf(TEXT("%sPrimitiveStats-%s-%s-%i-%s.csv"), 
								*appGameLogDir(), 
								*GWorld->GetOutermost()->GetName(), 
								GGameName, 
								GEngineVersion, 
								*CurrentTime);
	FArchive* CSVFile = GFileManager->CreateFileWriter( *CSVFilename );
	if( CSVFile )
	{
		// Write out header row.
		FString HeaderRow;
		for( INT ColumnIndex=0; ColumnIndex<BSBC_MAX; ColumnIndex++ )
		{
			HeaderRow += ColumnHeaders(ColumnIndex);
			HeaderRow += TEXT(",");
		}
		HeaderRow += LINE_TERMINATOR;
		CSVFile->Serialize( TCHAR_TO_ANSI( *HeaderRow ), HeaderRow.Len() );

		// Write individual rows. The + 1 is for the combined stats.
		for( INT RowIndex=0; RowIndex<NumRows; RowIndex++ )
		{
			FString Row;
			FString RowText;
			for( INT ColumnIndex=0; ColumnIndex<BSBC_MAX; ColumnIndex++ )
			{
				RowText = ListControl->GetColumnItemText( RowIndex, ColumnIndex );
				Row += RowText.Replace(TEXT(","), TEXT(""));// cheap trick to get rid of ,
				Row += TEXT(",");
			}
			Row += LINE_TERMINATOR;
			CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );
		}

		// Close and delete archive.
		CSVFile->Close();
		delete CSVFile;
		CSVFile = NULL;
	}

	// Reset variable now that we dumped the stats to CSV.
	GDumpBuildingStatsToCSVDuringNextUpdate = FALSE;
}


/**
 * Sets the size of the list control based upon our new size
 *
 * @param In the command that was sent
 */
void WxBuildingStatsBrowser::OnSize( wxSizeEvent& In )
{
	// During the creation process a sizing message can be sent so don't
	// handle it until we are initialized
	if( bAreWindowsInitialized )
	{
		ListControl->SetSize( GetClientRect() );
		ListControl->Freeze();
		SetAutoColumnWidth();
		ListControl->Thaw();
	}
}

/**
 * Handler for column click events
 *
 * @param In the command that was sent
 */
void WxBuildingStatsBrowser::OnColumnClick( wxListEvent& In )
{
	INT ColumnIndex = In.GetColumn();

	if( ColumnIndex >= 0 )
	{
		if( WxBuildingStatsBrowser::PrimarySortIndex == ColumnIndex )
		{
			check( ColumnIndex < ARRAY_COUNT(WxBuildingStatsBrowser::CurrentSortOrder) );
			WxBuildingStatsBrowser::CurrentSortOrder[ColumnIndex] *= -1;
		}
		WxBuildingStatsBrowser::PrimarySortIndex = ColumnIndex;

		// Recreate the list from scratch.
		UpdateList(FALSE);
	}
}

/**
 * Handler for column right click events
 *
 * @param In the command that was sent
 */
void WxBuildingStatsBrowser::OnColumnRightClick( wxListEvent& In )
{
	INT ColumnIndex = In.GetColumn();

	if( ColumnIndex >= 0 )
	{
		if( WxBuildingStatsBrowser::SecondarySortIndex == ColumnIndex )
		{
			check( ColumnIndex < ARRAY_COUNT(WxBuildingStatsBrowser::CurrentSortOrder) );
			WxBuildingStatsBrowser::CurrentSortOrder[ColumnIndex] *= -1;
		}
		WxBuildingStatsBrowser::SecondarySortIndex = ColumnIndex;

		// Recreate the list from scratch.
		UpdateList(FALSE);
	}
}

/**
 * Handler for item activation (double click) event
 *
 * @param In the command that was sent
 */
void WxBuildingStatsBrowser::OnItemActivated( wxListEvent& In )
{
	// Try to find building matching the name.
	FString				ResourceName	= FString( ListControl->GetColumnItemText( In.GetIndex(), BSBC_Name ) );
	AProcBuilding*		Building		= FindObject<AProcBuilding>( NULL, *ResourceName );
	if( Building )
	{
		// Make sure this action is undoable.
		const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("BuildingStatsBrowser_ItemActivated")) );
		// Deselect everything.
		GUnrealEd->SelectNone( TRUE, TRUE );
		// Select clicked building 
		GUnrealEd->SelectActor( Building, TRUE, NULL, FALSE );
		GUnrealEd->NoteSelectionChange();

		// Point cam at building as well
		GEditor->MoveViewportCamerasToActor( *Building, FALSE );
	}
}

