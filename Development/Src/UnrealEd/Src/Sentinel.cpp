/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjDrawUtils.h"
#include "Sentinel.h"
#include "Database.h"

BEGIN_EVENT_TABLE( WxSentinel, wxFrame )
	EVT_CHECKBOX( IDM_SENTINEL_USEGAMESTATS, WxSentinel::OnUseGameStatsChange )
	EVT_COMBOBOX( IDM_SENTINEL_PLATFORMCOMBO, WxSentinel::OnPlatformChange )
	EVT_COMBOBOX( IDM_SENTINEL_RUNTYPECOMBO, WxSentinel::OnRunTypeChange )
	EVT_COMBOBOX( IDM_SENTINEL_CHANGELISTCOMBO, WxSentinel::OnChangelistChange )
	EVT_COMBOBOX( IDM_SENTINEL_STATGROUPCOMBO, WxSentinel::OnStatGroupChange )
	EVT_COMBOBOX( IDM_SENTINEL_STATNAMECOMBO, WxSentinel::OnStatChange )
	EVT_COMMAND_SCROLL( IDM_SENTINEL_STATSIZESLIDER, WxSentinel::OnDrawSizeChange )
	EVT_CHECKBOX( IDM_SENTINEL_USEPRESETCOLORS, WxSentinel::OnUsePresetColors )
	EVT_COMMAND_SCROLL( IDM_SENTINEL_FILTERSLIDER, WxSentinel::OnFilterValueChange )
	EVT_COMBOBOX( IDM_SENTINEL_FILTERMODE, WxSentinel::OnFilterModeChange )
	EVT_CLOSE( WxSentinel::OnClose )
END_EVENT_TABLE()

#define STAT_SIZE_ORTHO_SCALE (100000.f)
#define STAT_SIZE_PERSPECTIVE_SCALE (7.5f)

WxSentinel::WxSentinel( WxEditorFrame* InEd )
	: wxFrame( InEd, -1, wxString(*LocalizeUnrealEd("Sentinel")), wxDefaultPosition, wxSize(450, 500), wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT )
	, Connection(NULL)
	, ToolTipStatIndex(INDEX_NONE)
	, ToolTipX(0)
	, ToolTipY(0)
	, ToolTipViewport(NULL)
	, bUsePresetColors(TRUE)
	, CurrentFilterValue(0.f)
	, FilterMode(EVFM_Above)
	, StatGroupSelectionIndex( -1 )
{
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));

	FWindowUtil::LoadPosSize( TEXT("Sentinel"), this, -1, -1, 450, 500 );

	EditorFrame = InEd;

	wxBoxSizer* TopVSizer = new wxBoxSizer(wxVERTICAL);
	{
		wxFlexGridSizer* SliderGrid = new wxFlexGridSizer(10, 2, 0, 0);
		{
			SliderGrid->AddGrowableCol(1);

			// Whether to use GameStats or PerfStats
			wxStaticText* GameStatsText = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("UseGameplayStats"), wxDefaultPosition, wxDefaultSize, 0 );
			SliderGrid->Add(GameStatsText, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

			UseGameStatsCheck = new wxCheckBox( this, IDM_SENTINEL_USEGAMESTATS, TEXT(""), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
			UseGameStatsCheck->SetValue(FALSE);
			SliderGrid->Add(UseGameStatsCheck, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

			// Platform 
			wxStaticText* PlatformText = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("Platform"), wxDefaultPosition, wxDefaultSize, 0 );
			SliderGrid->Add(PlatformText, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

			PlatformCombo = new wxComboBox( this, IDM_SENTINEL_PLATFORMCOMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
			PlatformCombo->Append( TEXT("Xbox360") );
			PlatformCombo->Append( TEXT("PS3") );
			PlatformCombo->Append( TEXT("Windows") );
			PlatformCombo->SetSelection(0);
			SliderGrid->Add(PlatformCombo, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

			// Run type
			wxStaticText* TaskTypeText = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("TaskType"), wxDefaultPosition, wxDefaultSize, 0 );
			SliderGrid->Add(TaskTypeText, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

			RunTypeCombo = new wxComboBox( this, IDM_SENTINEL_RUNTYPECOMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
			SliderGrid->Add(RunTypeCombo, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

			// Changelist
			wxStaticText* ChangelistText = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("Changelist"), wxDefaultPosition, wxDefaultSize, 0 );
			SliderGrid->Add(ChangelistText, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

			ChangelistCombo = new wxComboBox( this, IDM_SENTINEL_CHANGELISTCOMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
			SliderGrid->Add(ChangelistCombo, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

			// Stat group
			wxStaticText* StatGroupText = new wxStaticText( this, IDM_SENTINEL_STATGROUPCOMBO, *LocalizeUnrealEd("StatGroup"), wxDefaultPosition, wxDefaultSize, 0 );
			SliderGrid->Add(StatGroupText, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

			StatGroupCombo = new wxComboBox( this, IDM_SENTINEL_STATGROUPCOMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
			SliderGrid->Add(StatGroupCombo, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

			// Stat name
			wxStaticText* StatNameText = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("StatName"), wxDefaultPosition, wxDefaultSize, 0 );
			SliderGrid->Add(StatNameText, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

			StatNameCombo = new wxComboBox( this, IDM_SENTINEL_STATNAMECOMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
			SliderGrid->Add(StatNameCombo, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

			// Draw size
			wxStaticText* StatDrawSizeText = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("StatDrawSize"), wxDefaultPosition, wxDefaultSize, 0 );
			SliderGrid->Add(StatDrawSizeText, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

			StatDrawSizeSlider = new wxSlider( this, IDM_SENTINEL_STATSIZESLIDER, 25, 1, 200, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL );
			SliderGrid->Add(StatDrawSizeSlider, 1, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);
			StatDrawSize = 25;

			// Custom colors
			wxStaticText* PresetColorsText = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("UsePresetColors"), wxDefaultPosition, wxDefaultSize, 0 );
			SliderGrid->Add(PresetColorsText, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

			UsePresetColorsScale = new wxCheckBox( this, IDM_SENTINEL_USEPRESETCOLORS, TEXT("") );
			UsePresetColorsScale->SetValue(TRUE);
			SliderGrid->Add(UsePresetColorsScale, 1, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

			SliderGrid->AddStretchSpacer(0);
			SliderGrid->AddStretchSpacer(0);

			// Filter
			wxStaticText* StatFilterText = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("StatFilter"), wxDefaultPosition, wxDefaultSize, 0 );
			SliderGrid->Add(StatFilterText, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

			FilterValueSlider = new wxSlider( this, IDM_SENTINEL_FILTERSLIDER, 0, 0, 200, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL );
			FilterValueSlider->SetValue(0);
			SliderGrid->Add(FilterValueSlider, 1, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

			SliderGrid->AddStretchSpacer(0);

			FilterValueBox = new wxTextCtrl( this, IDM_SENTINEL_FILTERTEXT, TEXT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
			FilterValueBox->SetValue(TEXT("0.0"));
			SliderGrid->Add(FilterValueBox, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

			SliderGrid->AddStretchSpacer(0);

			FilterModeCombo = new wxComboBox( this, IDM_SENTINEL_FILTERMODE, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
			FilterModeCombo->Append( TEXT("Off") );
			FilterModeCombo->Append( TEXT("Above") );
			FilterModeCombo->Append( TEXT("Below") );
			FilterModeCombo->SetSelection(1);
			SliderGrid->Add(FilterModeCombo, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

		}
		TopVSizer->Add(SliderGrid, 1, wxGROW|wxALL, 5);
	}
	SetSizer(TopVSizer);

	// Create status bar
	StatusBar = new WxSentinelStatusBar( this, -1 );
	check(StatusBar);
	SetStatusBar( StatusBar );

	for(INT i=0; i<GApp->EditorFrame->ViewportConfigData->GetViewportCount(); i++)
	{
		WxLevelViewportWindow* LevelVC = GApp->EditorFrame->ViewportConfigData->AccessViewport(i).ViewportWindow;
		if(LevelVC)
		{
			// Turn on 'show sentinel stats' flag for top viewport
			if(LevelVC->ViewportType == LVT_OrthoXY)
			{				
				LevelVC->ShowFlags |= SHOW_SentinelStats;
			}

		}
	}	
}

WxSentinel::~WxSentinel()
{
	// If we have a connection at this point it was initialized so we need to close it.
	if( Connection )
	{
		Connection->Close();
		delete Connection;
		Connection = NULL;
		debugf(NAME_DevDataBase,TEXT("SENTINEL Connection closed"));
	}

	FWindowUtil::SavePosSize( TEXT("Sentinel"), this );
}

void WxSentinel::OnClose(wxCloseEvent& In)
{
	check(EditorFrame);
	EditorFrame->SentinelClosed();

	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );

	Destroy();
}

void WxSentinel::OnUseGameStatsChange(wxCommandEvent& In)
{
	GetAvailableChangelists();
	GrabDataSet();
	CalcStatRange();
	StatGroupSelectionIndex = -1;

	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

void WxSentinel::OnPlatformChange(wxCommandEvent& In)
{
	GetAvailableChangelists();
	GrabDataSet();
	CalcStatRange();
	StatGroupSelectionIndex = -1;

	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

void WxSentinel::OnRunTypeChange( wxCommandEvent& In )
{
	GetAvailableChangelists();
	GrabDataSet();
	CalcStatRange();
	StatGroupSelectionIndex = -1;

	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

void WxSentinel::OnChangelistChange( wxCommandEvent& In )
{
	GrabDataSet();
	CalcStatRange();
	StatGroupSelectionIndex = -1;

	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

void WxSentinel::OnStatGroupChange( wxCommandEvent& In )
{
	StatGroupSelectionIndex = static_cast<INT>(StatGroupCombo->GetSelection());

	//warnf( TEXT("OnStatGroupChange %s %d"), (const TCHAR*)StatGroupCombo->GetValue(), StatGroupSelectionIndex);
	GrabDataSet();
	CalcStatRange();

	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

void WxSentinel::OnStatChange( wxCommandEvent& In )
{
	CalcStatRange();

	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

void WxSentinel::OnDrawSizeChange(wxScrollEvent& In )
{
	StatDrawSize = StatDrawSizeSlider->GetValue();
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

void WxSentinel::OnUsePresetColors( wxCommandEvent& In )
{
	bUsePresetColors = (In.IsChecked() ? TRUE : FALSE);

	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

void WxSentinel::OnFilterValueChange( wxScrollEvent& In )
{
	const INT SliderVal = FilterValueSlider->GetValue();
	const FLOAT StatRange = CurrentStatMax - CurrentStatMin;
	CurrentFilterValue = CurrentStatMin + ((((FLOAT)SliderVal)/200.f)*StatRange);

	const FString ShowString = FString::Printf(TEXT("%3.2f"), CurrentFilterValue);
	FilterValueBox->SetValue(*ShowString);

	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

void WxSentinel::OnFilterModeChange( wxCommandEvent& In )
{
	const INT ComboSel = FilterModeCombo->GetSelection();

	FilterMode = EVFM_Off;
	if(ComboSel == 1)
	{
		FilterMode = EVFM_Above;
	}
	else if(ComboSel == 2)
	{
		FilterMode = EVFM_Below;
	}

	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

/** Create connection to databse. */
UBOOL WxSentinel::ConnectToDatabase()
{
	UBOOL bConnected = FALSE;
#if !UDK
	FString DataSource, Catalog;
	if(!GConfig->GetString( TEXT("SentinelStats"), TEXT("SentinelStatsSource"), DataSource, GEditorIni))
	{
		return bConnected;
	}

	if(!GConfig->GetString( TEXT("SentinelStats"), TEXT("SentinelCatalog"), Catalog, GEditorIni))
	{
		return bConnected;
	}

	// Create the connection string with Windows Authentication as the way to handle permissions/ login/ security.
	FString ConnectionString = FString::Printf(TEXT("Provider=sqloledb;Data Source=%s;Initial Catalog=%s;Trusted_Connection=Yes;"), *DataSource, *Catalog);

	// Create the connection object; needs to be deleted via "delete".
	GWarn->BeginSlowTask( *LocalizeUnrealEd("SentinelConnectingToDatabase") , TRUE );
	Connection = FDataBaseConnection::CreateObject();
	
	// Try to open connection to DB - this is a synchronous operation.
	if( Connection && Connection->Open( *ConnectionString, NULL, NULL ) )
	{
		debugf(NAME_DevDataBase,TEXT("SENTINEL Connection to %s.%s succeeded"),*DataSource,*Catalog);
		
		bConnected = TRUE;
		// Update 'run type' combo from DB
		GetAvailableRunTypes();
		// Given a run type and a map name, update the changelist combo
		GetAvailableChangelists();
		// We now have map, runtype and changelist, grab local data set
		GrabDataSet();
		// Calc the min/max for current selected stat
		CalcStatRange();
	}
	// Connection failed :(
	else
	{
		debugf(NAME_DevDataBase,TEXT("SENTINEL Connection to %s.%s failed"), *DataSource, *Catalog);
		// Only delete object - no need to close as connection failed.
		delete Connection;
		Connection = NULL;

		appMsgf( AMT_OK, *LocalizeUnrealEd("SentinelDatabaseConnectionProblem") );
	}

	GWarn->EndSlowTask();

#endif	//#if !UDK
	return bConnected;
}

/** Calculate range of selected stat (CurrentStatMin/Max) */
void WxSentinel::CalcStatRange()
{
	CurrentStatMin = BIG_NUMBER;
	CurrentStatMax = -BIG_NUMBER;

	const FString CurrentStatName = (const TCHAR*)StatNameCombo->GetValue();
	UBOOL bFoundStat = FALSE;
	for(INT i=0; i<DataSet.Num(); i++)
	{
		const FSentinelStatEntry& Entry = DataSet(i);

		const FLOAT* StatPtr = Entry.StatData.Find(CurrentStatName);
		if(StatPtr && (Abs(*StatPtr) > SMALL_NUMBER))
		{
			bFoundStat = TRUE;
			CurrentStatMin = ::Min(CurrentStatMin, *StatPtr);
			CurrentStatMax = ::Max(CurrentStatMax, *StatPtr);
		}
	}

	if(!bFoundStat)
	{
		CurrentStatMin = CurrentStatMax = 0.f;
	}

	// Reset filter
	FString ShowString = FString::Printf(TEXT("%3.2f"), CurrentStatMin);
	FilterValueBox->SetValue(*ShowString);
	CurrentFilterValue = CurrentStatMin;
	FilterValueSlider->SetValue(0);

	FilterModeCombo->SetSelection(1);
	FilterMode = EVFM_Above;
}

/** Query the database to find the types of run that we have stats for. */
void WxSentinel::GetAvailableRunTypes()
{
	if(Connection)
	{
		const FString QueryString(TEXT("dbo.GetTasks"));

		// Run the query on the database
		FDataBaseRecordSet* RecordSet = NULL;
		const UBOOL bSuccess = Connection->Execute( *QueryString, RecordSet );
		// If success, update combo
		if(bSuccess)
		{
			RunTypeCombo->Clear();

			for( FDataBaseRecordSet::TIterator It( RecordSet ); It; ++It )
			{
				const FString RunTypeString = It->GetString(TEXT("TaskDescription"));
				RunTypeCombo->Append(*RunTypeString); 
			}

			delete RecordSet;
			RecordSet = NULL;
		}
	}

	// Make sure we have _something_ in the combo
	if(RunTypeCombo->GetCount() == 0)
	{
		RunTypeCombo->Append( TEXT("-- None --") );
	}

	RunTypeCombo->SetSelection(0);
}

/** Query the database to find changelists available for selected run type and map. Updates ChangelistCombo. */
void WxSentinel::GetAvailableChangelists()
{
	if(Connection)
	{
		// Get desired platform
		const FString PlatName  = (const TCHAR*)PlatformCombo->GetValue();

		// Get run type
		const FString RunType = (const TCHAR*)RunTypeCombo->GetValue();

		// Get map name
		extern const FString GetMapNameStatic(); 
		const FString LevelName = GetMapNameStatic();

		FString QueryString = TEXT("");
		QueryString = FString::Printf(TEXT("dbo.GetChangelists @Task='%s', @PlatformName='%s', @LevelName='%s'"), *RunType, *PlatName, *LevelName);
		

		// Run the query on the database
		FDataBaseRecordSet* RecordSet = NULL;
		const UBOOL bSuccess = Connection->Execute( *QueryString, RecordSet );
		// If success, update combo
		if(bSuccess)
		{
			ChangelistCombo->Clear();

			for( FDataBaseRecordSet::TIterator It( RecordSet ); It; ++It )
			{
				const INT Changelist = It->GetInt(TEXT("Changelist"));
				const FString ChangelistString = FString::Printf(TEXT("%d"), Changelist);
				ChangelistCombo->Append(*ChangelistString, (void*)Changelist); 
			}

			delete RecordSet;
			RecordSet = NULL;
		}
	}

	// Make sure we have _something_ in the combo
	if(ChangelistCombo->GetCount() == 0)
	{
		ChangelistCombo->Append( TEXT("-- None --"), (void*)-1 );
	}

	ChangelistCombo->SetSelection(0);
}

/** Using selected run type, map and changelist, update local stat cache (DataSet). Also updates StatNameCombo. */
void WxSentinel::GrabDataSet()
{
	if(Connection)
	{
		// Get desired platform
		const FString PlatName = (const TCHAR*)PlatformCombo->GetValue();

		// Get run type
		const FString RunType = (const TCHAR*)RunTypeCombo->GetValue();

		// Get map name
		extern const FString GetMapNameStatic(); 
		FString LevelName = GetMapNameStatic();

		// Get changelist from combo
		const INT ChangelistComboSelection = ChangelistCombo->GetSelection();
		const PTRINT ChangelistComboPtr = reinterpret_cast< PTRINT >( ChangelistCombo->GetClientData(ChangelistComboSelection) );
		const INT Changelist = static_cast< INT >( ChangelistComboPtr );

		// Invalid changelist
		if(Changelist == -1)
		{
			return;
		}


		// we now need to get the StatGroups
		FDataBaseRecordSet* RecordSetStatGroups = NULL;
		FString QueryStringStatGroups = TEXT("");
		if( UseGameStatsCheck->GetValue() == TRUE )
		{
			QueryStringStatGroups = FString::Printf(TEXT("dbo.GetGameDataEventNames @Task='%s', @PlatformName='%s', @LevelName='%s', @Changelist='%d'"), *RunType, *PlatName, *LevelName, Changelist);
		}
		else
		{
			QueryStringStatGroups = FString::Printf(TEXT("dbo.GetSentinelDataStatGroups @Task='%s', @PlatformName='%s', @LevelName='%s', @Changelist='%d'"), *RunType, *PlatName, *LevelName, Changelist);
		}


		UBOOL bSuccessStatGroups = Connection->Execute( *QueryStringStatGroups, RecordSetStatGroups );

		INT RecordCount = 0;

		// HACK - Count number of records
		if(bSuccessStatGroups)
		{
			for( FDataBaseRecordSet::TIterator TempIt( RecordSetStatGroups ); TempIt; ++TempIt )
			{
				RecordCount++;
			}

			if(RecordCount == 0)
			{
				bSuccessStatGroups = FALSE;
			}
		}

		TArray<FString> ColumnNames_StatGroup; // the list of StatGroups

		if(bSuccessStatGroups)
		{
			for( FDataBaseRecordSet::TIterator TempIt( RecordSetStatGroups ); TempIt; ++TempIt )
			{
				if( UseGameStatsCheck->GetValue() == TRUE )
				{
					const FString DataName = TempIt->GetString(TEXT("GameEventName"));
					ColumnNames_StatGroup.AddItem( DataName );
				}
				else
				{
					const FString DataName = TempIt->GetString(TEXT("StatGroupName"));
					ColumnNames_StatGroup.AddItem( DataName );
				}
			}
		}

		// clean up the StatNames recordset
		delete RecordSetStatGroups;
		RecordSetStatGroups = NULL;

		// If success, 
		if(bSuccessStatGroups)
		{
			// Fill the stat combo based on column titles of returned table
			StatGroupCombo->Clear();
			// Add something like this for setting all/none for filtering StatGroupCombo->Append( TEXT("-- None --") );
			for(INT i=0; i<ColumnNames_StatGroup.Num(); i++)
			{
				// @todo move the set of "summary" type stats to an .ini
				if( ColumnNames_StatGroup(i) != TEXT("FPSBuckets") )
				{
					StatGroupCombo->Append(*ColumnNames_StatGroup(i));
				}
			}

			// default to the UnitFPS as that is what we are mostly concentrating on atm
			const INT SelectionIndex = StatGroupCombo->FindString( TEXT("UnitFPS") );

			if( StatGroupSelectionIndex != -1 )
			{
				StatGroupCombo->SetSelection(StatGroupSelectionIndex);
			}
			else
			{
				StatGroupCombo->SetSelection(SelectionIndex);
			}
		}
		// no records found so clear it
		else
		{
			// Make sure we have _something_ in the combo
			StatGroupCombo->Clear();
			StatGroupCombo->Append( TEXT("-- None --") );
			StatGroupCombo->SetSelection(0);
		}


		// we now need to get the StatNames
		FDataBaseRecordSet* RecordSetStatNames = NULL;
		FString QueryStringStatNames = TEXT("");

		const FString GroupName = (const TCHAR*)StatGroupCombo->GetValue();
		//warnf( TEXT("StatGroupCombo %d"),ChangelistCombo->GetSelection());

		if( UseGameStatsCheck->GetValue() == TRUE )
		{
			QueryStringStatNames = FString::Printf(TEXT("dbo.GetGameDataEventDescNames @Task='%s', @PlatformName='%s', @LevelName='%s', @Changelist='%d', @GameEventName='%s'"), *RunType, *PlatName, *LevelName, Changelist, *GroupName );
		}
		else
		{
			QueryStringStatNames = FString::Printf(TEXT("dbo.GetSentinelDataStatNames @Task='%s', @PlatformName='%s', @LevelName='%s', @Changelist='%d', @StatGroupName='%s'"), *RunType, *PlatName, *LevelName, Changelist,  *GroupName);
		}

		//warnf( TEXT("%s"), *QueryStringStatNames);
		UBOOL bSuccessStatNames = Connection->Execute( *QueryStringStatNames, RecordSetStatNames );

		RecordCount = 0;

		// HACK - Count number of records
		if(bSuccessStatNames)
		{
			for( FDataBaseRecordSet::TIterator TempIt( RecordSetStatNames ); TempIt; ++TempIt )
			{
				RecordCount++;
			}

			if(RecordCount == 0)
			{
				bSuccessStatNames = FALSE;
			}
		}

		TArray<FString> ColumnNames; // the list of StatNames

		if(bSuccessStatNames)
		{
			for( FDataBaseRecordSet::TIterator TempIt( RecordSetStatNames ); TempIt; ++TempIt )
			{
				if( UseGameStatsCheck->GetValue() == TRUE )
				{
					const FString StatName = TempIt->GetString(TEXT("GameEventDesc"));
					ColumnNames.AddItem( StatName );
				}
				else
				{
					const FString StatName = TempIt->GetString(TEXT("StatName"));
					ColumnNames.AddItem( StatName );
				}
			}
		}

		// clean up the StatNames recordset
		delete RecordSetStatNames;
		RecordSetStatNames = NULL;


		// so now that we have the StatNames we need to map all of the data to a Location
        // data is now in this format:
        // StatName | StatValue | RotRoll | RotPitch | RotYaw | LocX | LocY | LocZ | <other IDs>
        // where you have 10 of thousands of rows returned for a query
		FDataBaseRecordSet* RecordSet = NULL;
		FString QueryString = TEXT("");
		if( UseGameStatsCheck->GetValue() == TRUE )
		{
			QueryString = FString::Printf(TEXT("dbo.GetGameData @Task='%s', @PlatformName='%s', @LevelName='%s', @Changelist='%d'"), *RunType, *PlatName, *LevelName, Changelist);
		}
		else
		{
			QueryString = FString::Printf(TEXT("dbo.GetSentinelData @Task='%s', @PlatformName='%s', @LevelName='%s', @Changelist='%d'"), *RunType, *PlatName, *LevelName, Changelist);
		}

		const DOUBLE StartTime = appSeconds();
		UBOOL bSuccess = Connection->Execute( *QueryString, RecordSet );
		const DOUBLE QueryTime = appSeconds() - StartTime;

		RecordCount = 0; // reset the RecordCount 

		// HACK - Count number of records
		if(bSuccess)
		{
			for( FDataBaseRecordSet::TIterator TempIt( RecordSet ); TempIt; ++TempIt )
			{
				RecordCount++;
			}

			if(RecordCount == 0)
			{
				bSuccess = FALSE;
			}
		}

		// If success, 
		if(bSuccess)
		{
			DataSet.Empty(); // empty out the dataset so we can refill it with the new data we just got

			// Fill the stat combo based on column titles of returned table
			StatNameCombo->Clear();
			for(INT i=0; i<ColumnNames.Num(); i++)
			{
				StatNameCombo->Append(*ColumnNames(i));
			}
			StatNameCombo->SetSelection(0);

			// Iterate over all rows in record set and log them.
			for( FDataBaseRecordSet::TIterator It( RecordSet ); It; ++It )
			{
				INT LocX = It->GetInt( TEXT("LocX") );
				INT LocY = It->GetInt( TEXT("LocY") );
				INT LocZ = It->GetInt( TEXT("LocZ") );
				const FVector Position = FVector(LocX, LocY, LocZ);

				// Get rotation as Rotator and convert into degrees
				INT Yaw = It->GetInt( TEXT("RotYaw") );
				Yaw = Yaw * (180.f / 32768.f);

				// Unwind to make sure its in range -180 -> +180
				while(Yaw > 180.f)
				{
					Yaw -= 360.f;
				}

				while(Yaw < -180.f)
				{
					Yaw += 360.f;
				}

				INT Pitch = It->GetInt( TEXT("RotPitch") );
				INT Roll = It->GetInt( TEXT("RotRoll") );
				const FRotator Rotation = FRotator(Pitch, Yaw, Roll);

				FString StatName = TEXT("");
				FLOAT StatValue = -1.0f;
				FString GameValue = TEXT( "" );
				FLOAT SecondsFromStartOfSession = 0.0f;

				if( UseGameStatsCheck->GetValue() == TRUE )
				{
					StatName = It->GetString( TEXT("GameEventDesc" ) );
					// here we need to do some concatenation
					GameValue = It->GetString( TEXT("GameEventDesc" ) ) + It->GetString( TEXT("GamePlayerName" ) );
					SecondsFromStartOfSession = It->GetFloat( TEXT("SecondsFromStart" ) );
				}
				else
				{
					StatName = It->GetString( TEXT("StatName" ) );
					StatValue = It->GetFloat( TEXT("StatValue" ) );
				}

				// so now we need to look up in the DataSet for this location
				// if we already have this location we need to update it
				// else we need to add a new entry to it

				// O(N^2) :-(
				UBOOL bFound = FALSE;
			
				for( INT i = 0; i < DataSet.Num(); ++i )
				{
					FSentinelStatEntry& Entry = DataSet(i);
					if( (Entry.Position == Position) && (Entry.Rotation == Rotation) && (Entry.SecondsFromStartOfSession == SecondsFromStartOfSession) )
					{
						bFound = TRUE;
						//warnf( TEXT( "Updating: %s %f at %s %s" ), *StatName, StatValue, *Position.ToString(), *Rotation.ToString() );
						Entry.StatData.Set( StatName, StatValue );
						Entry.GameData.Set( StatName, GameValue );
						break;
					}
				}

				// we need to add a new entry if we did not find the location / rotation
				if( bFound == FALSE )
				{
					FSentinelStatEntry NewEntry; 
					NewEntry.Position = Position;
					NewEntry.Rotation = Rotation;
					NewEntry.SecondsFromStartOfSession = SecondsFromStartOfSession;
					NewEntry.StatData.Set( StatName, StatValue );
					NewEntry.GameData.Set( StatName, GameValue );
					//warnf( TEXT( "Adding: %s %f at %s %s" ), *StatName, StatValue, *Position.ToString(), *Rotation.ToString() );
					DataSet.AddItem( NewEntry );
				}

			}

			FString StatusString = FString::Printf(TEXT("Query succeeded! (%3.2f ms) - %d Records"), QueryTime*1000.f, RecordCount);
			StatusBar->SetStatusText(*StatusString, 0);

			// Clean up record set now that we are done.
			delete RecordSet;
			RecordSet = NULL;
		}
		else
		{
			// Make sure we have _something_ in the combo
			StatNameCombo->Clear();
			StatNameCombo->Append( TEXT("-- None --") );
			StatNameCombo->SetSelection(0);

			FString StatusString = FString::Printf(TEXT("Query failed. (%3.2f ms)"), QueryTime*1000.f);
			StatusBar->SetStatusText(*StatusString, 0);
		}
	}

	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

static void DrawWedge3D(FDynamicMeshBuilder& MeshBuilder, const FVector& Location, FLOAT Rot, FLOAT Size, const FColor& WedgeColor)
//static void DrawWedge3D(FPrimitiveDrawInterface* PDI, const FVector& Location, FLOAT Rot, FLOAT Size, const FColor& WedgeColor)
{
	FLOAT SinRot = appSin(Rot * (PI/180.f));
	FLOAT CosRot = appCos(Rot * (PI/180.f));

	FVector V0, V1;

	V0.X = (1.f*CosRot) - (0.383f*SinRot);
	V0.Y = (1.f*SinRot) + (0.383f*CosRot);
	V0.Z = 0.f;

	V1.X = (1.f*CosRot) - (-0.383f*SinRot);
	V1.Y = (1.f*SinRot) + (-0.383f*CosRot);
	V1.Z = 0.f;

	//FDynamicMeshBuilder MeshBuilder;

	INT VertexIndices[3];

	VertexIndices[0] = MeshBuilder.AddVertex(Location, FVector2D(0,0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), WedgeColor);
	VertexIndices[1] = MeshBuilder.AddVertex(Location + (V0 * Size), FVector2D(0,0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), WedgeColor);
	VertexIndices[2] = MeshBuilder.AddVertex(Location + (V1 * Size), FVector2D(0,0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), WedgeColor);

	MeshBuilder.AddTriangle(VertexIndices[0],VertexIndices[1],VertexIndices[2]);

	//if(GEngine->VertexColorMaterial)
	//{
	//	MeshBuilder.Draw(PDI, FMatrix::Identity, GEngine->VertexColorMaterial->GetRenderProxy(0), SDPG_World);
	//}
}

void WxSentinel::RenderStats3D(const FSceneView* View,class FPrimitiveDrawInterface* PDI)
{
	const FString CurrentStatName = (const TCHAR*)StatNameCombo->GetValue();

	FDynamicMeshBuilder MeshBuilder;

	if( UseGameStatsCheck->GetValue() == TRUE )
	{
		for(INT i=0; i<DataSet.Num(); i++)
		{
			const FSentinelStatEntry& Entry = DataSet(i);

			const FString* StatPtr = Entry.GameData.Find(CurrentStatName);
			if(StatPtr)
			{
				FColor StatColor(0,0,0);
				// First see if we have an engine-level stat colour mapping

				// If not, just color from green (min) to red (max)
				const FLOAT Alpha = 1.0f;
				const FVector ColorV = Lerp(FVector(0,1,0), FVector(1,0,0), Alpha);

				StatColor = FLinearColor(ColorV.X, ColorV.Y, ColorV.Z);

				StatColor.A = 255;

				//PDI->SetHitProxy( new HSentinelStatProxy(i) );
				//DrawWedge3D(PDI, Entry.Position, Entry.Yaw, STAT_SIZE_PERSPECTIVE_SCALE * StatDrawSize, StatColor);
				DrawWedge3D(MeshBuilder, Entry.Position, Entry.Rotation.Yaw, STAT_SIZE_PERSPECTIVE_SCALE * StatDrawSize, StatColor);
				//PDI->SetHitProxy( NULL );		
			}
		}
	}
	else
	{
		for(INT i=0; i<DataSet.Num(); i++)
		{
			const FSentinelStatEntry& Entry = DataSet(i);

			const FLOAT* StatPtr = Entry.StatData.Find(CurrentStatName);
			if(StatPtr)
			{
				// Check filter settings
				if( FilterMode == EVFM_Off || 
					(FilterMode == EVFM_Above && *StatPtr > CurrentFilterValue) || 
					(FilterMode == EVFM_Below && *StatPtr < CurrentFilterValue) ) 
				{
					FColor StatColor(0,0,0);
					// First see if we have an engine-level stat colour mapping
					if(!bUsePresetColors || !GEngine->GetStatValueColoration(CurrentStatName, *StatPtr, StatColor))
					{
						// If not, just color from green (min) to red (max)
						const FLOAT Alpha = Clamp<FLOAT>((*StatPtr - CurrentStatMin) / (CurrentStatMax - CurrentStatMin), 0.f, 1.f);
						const FVector ColorV = Lerp(FVector(0,1,0), FVector(1,0,0), Alpha);

						StatColor = FLinearColor(ColorV.X, ColorV.Y, ColorV.Z);
					}
					StatColor.A = 255;

					//PDI->SetHitProxy( new HSentinelStatProxy(i) );
					//DrawWedge3D(PDI, Entry.Position, Entry.Yaw, STAT_SIZE_PERSPECTIVE_SCALE * StatDrawSize, StatColor);
					DrawWedge3D(MeshBuilder, Entry.Position, Entry.Rotation.Yaw, STAT_SIZE_PERSPECTIVE_SCALE * StatDrawSize, StatColor);
					//PDI->SetHitProxy( NULL );
				}
			}
		}
	}

	if(GEngine->VertexColorMaterial)
	{
		MeshBuilder.Draw(PDI, FMatrix::Identity, GEngine->VertexColorMaterial->GetRenderProxy(0), SDPG_World,0.f);
	}
}

static void DrawWedge(FCanvas* Canvas, const FVector2D& Location, FLOAT Rot, FLOAT Size, const FColor& WedgeColor)
{
	FLOAT SinRot = appSin(Rot * (PI/180.f));
	FLOAT CosRot = appCos(Rot * (PI/180.f));

	FVector2D V0, V1;

	// Handle difference between canvas X/Y directions and top-view X/Y direction
	V0.X = (1.f*SinRot) + (0.383f*CosRot); 
	V0.Y = -1.f * ((1.f*CosRot) - (0.383f*SinRot));

	V1.X = (1.f*SinRot) + (-0.383f*CosRot); 
	V1.Y = -1.f * ((1.f*CosRot) - (-0.383f*SinRot));

	DrawTriangle2D(Canvas,
			Location, FVector2D(0,0),
			Location + (V0 * Size), FVector2D(0,0),
			Location + (V1 * Size), FVector2D(0,0),
			WedgeColor);
}

void WxSentinel::RenderStats(FEditorLevelViewportClient* ViewportClient, const FSceneView* View, FCanvas* Canvas)
{
	const FString CurrentStatName = (const TCHAR*)StatNameCombo->GetValue();

	if( UseGameStatsCheck->GetValue() == TRUE )
	{
		for(INT i=0; i<DataSet.Num(); i++)
		{
			const FSentinelStatEntry& Entry = DataSet(i);

			const FString* StatPtr = Entry.GameData.Find(CurrentStatName);

			if(StatPtr)
			{
				FVector2D PixelLocation;
				if(View->ScreenToPixel(View->WorldToScreen(Entry.Position),PixelLocation))
				{
					FColor StatColor(0,0,0);
					// First see if we have an engine-level stat colour mapping

						// If not, just color from green (min) to red (max)
					const FLOAT Alpha = 1.0f;
					const FVector ColorV = Lerp(FVector(0,1,0), FVector(1,0,0), Alpha);

					StatColor = FLinearColor(ColorV.X, ColorV.Y, ColorV.Z);

					StatColor.A = 255;

					FLOAT DrawSize = (StatDrawSize * STAT_SIZE_ORTHO_SCALE)/ ViewportClient->OrthoZoom;

					Canvas->SetHitProxy( new HSentinelStatProxy(i) );
					DrawWedge(Canvas, PixelLocation, Entry.Rotation.Yaw, DrawSize, StatColor);
					Canvas->SetHitProxy( NULL );
				}
			}
		}
	}
	else
	{
		for(INT i=0; i<DataSet.Num(); i++)
		{
			const FSentinelStatEntry& Entry = DataSet(i);

			const FLOAT* StatPtr = Entry.StatData.Find(CurrentStatName);
			if(StatPtr)
			{
				// Check filter settings
				if( FilterMode == EVFM_Off || 
					(FilterMode == EVFM_Above && *StatPtr > CurrentFilterValue) || 
					(FilterMode == EVFM_Below && *StatPtr < CurrentFilterValue) ) 
				{
					FVector2D PixelLocation;
					if(View->ScreenToPixel(View->WorldToScreen(Entry.Position),PixelLocation))
					{
						FColor StatColor(0,0,0);
						// First see if we have an engine-level stat colour mapping
						if(!bUsePresetColors || !GEngine->GetStatValueColoration(CurrentStatName, *StatPtr, StatColor))
						{
							// If not, just color from green (min) to red (max)
							const FLOAT Alpha = Clamp<FLOAT>((*StatPtr - CurrentStatMin) / (CurrentStatMax - CurrentStatMin), 0.f, 1.f);
							const FVector ColorV = Lerp(FVector(0,1,0), FVector(1,0,0), Alpha);

							StatColor = FLinearColor(ColorV.X, ColorV.Y, ColorV.Z);
						}
						StatColor.A = 255;

						FLOAT DrawSize = (StatDrawSize * STAT_SIZE_ORTHO_SCALE)/ ViewportClient->OrthoZoom;

						Canvas->SetHitProxy( new HSentinelStatProxy(i) );
						DrawWedge(Canvas, PixelLocation, Entry.Rotation.Yaw, DrawSize, StatColor);
						Canvas->SetHitProxy( NULL );
					}
				}
			}
		}
	}

	// If we have a tool tip to draw in this viewport...
	if(ToolTipStatIndex != INDEX_NONE && ToolTipViewport == ViewportClient && GCurrentLevelEditingViewportClient == ViewportClient)
	{
		const FSentinelStatEntry& Entry = DataSet(ToolTipStatIndex);


		if( UseGameStatsCheck->GetValue() == TRUE )
		{
			const FString* StatPtr = Entry.GameData.Find(CurrentStatName);
			if(StatPtr)
			{
				FString ToolTipString = FString::Printf(TEXT("%s"), **StatPtr);
				DrawShadowedString(Canvas, ToolTipX + 15, ToolTipY, *ToolTipString, GEngine->SmallFont, FColor(255,255,255));
			}
		}
		else
		{
			const FLOAT* StatPtr = Entry.StatData.Find(CurrentStatName);
			if(StatPtr)
			{
				FString ToolTipString = FString::Printf(TEXT("%3.2f"), *StatPtr);
				DrawShadowedString(Canvas, ToolTipX + 15, ToolTipY, *ToolTipString, GEngine->SmallFont, FColor(255,255,255));
			}
		}
	}
}

/** Called by editor when mouse moves */
void WxSentinel::MouseMoved(FEditorLevelViewportClient* ViewportClient, INT X, INT Y)
{
	//if this window is visible AND this is the active viewport
	if(IsShownOnScreen() && GCurrentLevelEditingViewportClient == ViewportClient)
	{
		HHitProxy* HitResult = ViewportClient->Viewport->GetHitProxy(X, Y);
		if( HitResult && HitResult->IsA(HSentinelStatProxy::StaticGetType()) )
		{
			HSentinelStatProxy* StatProxy = (HSentinelStatProxy*)HitResult;

			if (StatProxy->StatIndex != ToolTipStatIndex)
			{
				ToolTipStatIndex = StatProxy->StatIndex;
				ToolTipX = X;
				ToolTipY = Y;
				ToolTipViewport = ViewportClient;
				ViewportClient->Viewport->InvalidateDisplay();
			}
		}
		else
		{
			if (ToolTipStatIndex != INDEX_NONE)
			{
				ViewportClient->Viewport->InvalidateDisplay();
			}

			ToolTipStatIndex = INDEX_NONE;
			ToolTipX = 0;
			ToolTipY = 0;
			ToolTipViewport = NULL;
		}
	}
}

/** Called by editor when key pressed. */
void WxSentinel::InputKey(FEditorLevelViewportClient* ViewportClient, FName Key,EInputEvent Event)
{
	if(Key == KEY_LeftMouseButton)
	{
		// Double click jumps perspective viewports to that locaion
		if(Event == IE_DoubleClick)
		{
			ViewportClient->Viewport->Invalidate();

			// Find the stat we clicked on
			const INT	HitX = ViewportClient->Viewport->GetMouseX();
			const INT HitY = ViewportClient->Viewport->GetMouseY();
			HHitProxy* HitResult = ViewportClient->Viewport->GetHitProxy(HitX, HitY);

			if( HitResult && HitResult->IsA(HSentinelStatProxy::StaticGetType()) )
			{
				HSentinelStatProxy* StatProxy = (HSentinelStatProxy*)HitResult;
				const FSentinelStatEntry& Entry = DataSet(StatProxy->StatIndex);

				// Iterate over each viewport, updating view if its perspective.
				for(INT i=0; i<GApp->EditorFrame->ViewportConfigData->GetViewportCount(); i++)
				{
					FEditorLevelViewportClient* LevelVC = GApp->EditorFrame->ViewportConfigData->AccessViewport(i).ViewportWindow;
					if(LevelVC && LevelVC->ViewportType == LVT_Perspective)
					{
						LevelVC->ViewLocation = Entry.Position;
						LevelVC->ViewRotation = FRotator(0, appRound(Entry.Rotation.Yaw * (32768.f / 180.f)), 0);
						LevelVC->Viewport->Invalidate();
					}
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	WxSentinelStatusBar.
-----------------------------------------------------------------------------*/

WxSentinelStatusBar::WxSentinelStatusBar( wxWindow* InParent, wxWindowID InID )
:	wxStatusBar( InParent, InID )
{
	INT Widths[1] = {-1};

	SetFieldsCount(1, Widths);
}

