/*=============================================================================
	GameStatsVisualizer.cpp: Browser window for working with game stats files
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "Editor.h"
#include "GameFrameworkGameStatsClasses.h"
#include "UnrealEdGameStatsClasses.h"
#include "GameplayEventsUtilities.h"
#include "GameStatsBrowser.h"
#include "GameStatsReport.h"
#include "Factories.h"

#undef  AddWxEventHandler
#undef	AddWxListEventHandler
#undef	AddNotebookEventHandler
#undef	AddWxSpinEventHandler
#undef	AddWxScrollEventHandler

#define AddWxEventHandler( id, event, func )\
		{\
		wxEvtHandler* eh = GetEventHandler();\
		eh->Connect( id, event, wxCommandEventHandler(func) );\
		}

#define AddWxListEventHandler( id, event, func )\
		{\
		wxEvtHandler* eh = GetEventHandler();\
		eh->Connect( id, event, wxListEventHandler(func) );\
		}

#define AddNotebookEventHandler( id, event, func )\
		{\
		wxEvtHandler* eh = GetEventHandler();\
		eh->Connect( id, event, wxNotebookEventHandler(func) );\
		}
	 
#define AddWxSpinEventHandler( id, event, func )\
		{\
		wxEvtHandler* eh = GetEventHandler();\
		eh->Connect( id, event, wxSpinEventHandler(func) );\
		}

#define AddWxScrollEventHandler( id, event, func )\
		{\
		wxEvtHandler* eh = GetEventHandler();\
		eh->Connect( id, event, wxScrollEventHandler(func) );\
		}

const TCHAR TitleBarStringLocName[] = TEXT("GameStatsVisualizer_TitleBar");
const TCHAR ImportGameStatsFileLocString[] = TEXT("GameStatsVisualizer_OpenFileDialogTitle");

/** Date Filter Friendly Names */
const TCHAR* GameStatsDateFiltersNames[ GSDF_MAX ] =
{
	TEXT( "GameStatsVisualizer_DateFilterToday" ),
	TEXT( "GameStatsVisualizer_DateFilterLast3Days" ),
	TEXT( "GameStatsVisualizer_DateFilterLastWeek" ),
};

/** Game session column list*/
enum GameSessionColumns
{
	GameplaySessionTimestamp = 0,
	GameplayMapName,
	GameplayMapURL,
	GamePlaySessionDuration,
	AppTitleID,
	GameplaySessionID,
	PlatformType,
	Language,
	/** The total number of column IDs.  Must be the last entry in the enum! */
	NumGameSessionsColumns
};

/** Game session column names */
const TCHAR* GameSessionColumnNames[ NumGameSessionsColumns ] =
{
	TEXT( "GameStatsVisualizer_GameplaySessionTimestamp" ),
	TEXT( "GameStatsVisualizer_MapName" ),
	TEXT( "GameStatsVisualizer_MapURL" ),
	TEXT( "GameStatsVisualizer_GamePlaySessionDuration" ),
	TEXT( "GameStatsVisualizer_TitleID" ),
	TEXT( "GameStatsVisualizer_GameplaySessionID" ),
	TEXT( "GameStatsVisualizer_PlatformType" ),
	TEXT( "GameStatsVisualizer_Language" ),
};

/** Event list columns */
enum EventListColumns
{
	EventID = 0,
	EventName,
	EventCount,
	/** The total number of column IDs.  Must be the last entry in the enum! */
	NumEventListColumns
};

/** Event list column names */
const TCHAR* EventListColumnNames[ NumEventListColumns ] =
{
	TEXT( "GameStatsVisualizer_EventID" ),
	TEXT( "GameStatsVisualizer_EventName" ),
	TEXT( "GameStatsVisualizer_EventCount" )
};

/** Team list columns */
enum TeamListColumns
{
	TeamNum = 0,
	TeamName,
	//TeamColor,
	/** The total number of column IDs.  Must be the last entry in the enum! */
	NumTeamListColumns
};

/** Team list column names */
const TCHAR* TeamListColumnNames[ NumTeamListColumns ] =
{
	TEXT( "GameStatsVisualizer_TeamNum" ),
	TEXT( "GameStatsVisualizer_TeamName" ),
	//TEXT( "GameStatsVisualizer_TeamColor" )
};

/** Player list columns */
enum PlayerListColumns
{
	PlayerName = 0,
	IsBot,
	/** The total number of column IDs.  Must be the last entry in the enum! */
	NumPlayerListColumns
};

/** Player list column names */
const TCHAR* PlayerListColumnNames[ NumPlayerListColumns ] =
{
	TEXT( "GameStatsVisualizer_Name" ),
	TEXT( "GameStatsVisualizer_IsBot" ),
};

#define QUERY_TIMEUPDATED			0x00000001 //Time parameters of the query have changed
#define QUERY_SESSIONUPDATED		0x00000002 //Session parameters of the query have changed
#define QUERY_VISUALIZERCHANGED		0x00000004 //Visualizer on the tab has changed
#define QUERY_EVENTSUPDATED			0x00000008 //User selected event list changed
#define QUERY_TEAMSUPDATED			0x00000010 //User selected team list changed
#define QUERY_PLAYERSUPDATED		0x00000020 //User selected player list changed

/** Determines visibility of a given window */
extern UBOOL IsReallyShown(wxWindow* InWindow);

/** 
 * Get the user data associated with a given list control
 * @param ListCtrl - Control containing selected items of interest
 * @param SelectedItems - out value array of indices for the selected elements of the list control
 */
static void GetSelectedItems(wxListCtrl* ListCtrl, TArray<INT>& SelectedItems)
{
	SelectedItems.Empty();

	LONG ItemIndex = -1;

	for (;;) 
	{
		ItemIndex = ListCtrl->GetNextItem(ItemIndex,
			wxLIST_NEXT_ALL,
			wxLIST_STATE_SELECTED);

		if (ItemIndex == -1)
		{
			break;
		}

		// Got the selected item index
		SelectedItems.AddItem((INT)ItemIndex);
	}
}

/** 
* Get the user data associated with a given list control
* @param ListCtrl - Control containing user data of interest
* @param SelectedItems - out value array of user data for the selected elements of the list control
*/
static void GetSelectedItemsUserData(wxListCtrl* ListCtrl, TArray<INT>& SelectedItems)
{
	SelectedItems.Empty();

	LONG ItemIndex = -1;

	for (;;) 
	{
		ItemIndex = ListCtrl->GetNextItem(ItemIndex,
			wxLIST_NEXT_ALL,
			wxLIST_STATE_SELECTED);

		if (ItemIndex == -1)
		{
			break;
		}

		// Got the selected item index, get the user data
		UINT UserData = ListCtrl->GetItemData(ItemIndex);
		SelectedItems.AddItem((INT)UserData);
	}
}

/**
 *	Take the current perspective viewport view location/rotation and use that to take a snapshot
 *	and save it as a texture with the Prefab.
 */
void CreateHeatmapScreenshot( UHeatmapVisualizer* HeatmapVis, const FString& Filename )
{
	FVector WorldCenter = (HeatmapVis->WorldMinPos + HeatmapVis->WorldMaxPos) * 0.5f;
	FLOAT WorldWidth = HeatmapVis->WorldMaxPos.X - HeatmapVis->WorldMinPos.X; 
	FLOAT WorldHeight = HeatmapVis->WorldMaxPos.Y - HeatmapVis->WorldMinPos.Y; 

	//UEditorEngine* EditorEngine = CastChecked<UEditorEngine>(GEngine);
	//const UINT MinRenderTargetSize = 1024;
	//UTextureRenderTarget2D* RendTarget = EditorEngine->GetScratchRenderTarget( MinRenderTargetSize );

	UTextureRenderTargetFactoryNew* NewFactory = CastChecked<UTextureRenderTargetFactoryNew>( ConstructObject<UTextureRenderTargetFactoryNew>(UTextureRenderTargetFactoryNew::StaticClass()) );

	// Create a texture twice as dense as the heatmap
	NewFactory->Width = Min<FLOAT>(Max<FLOAT>((2.0f * WorldHeight + 0.5f) / HeatmapVis->NumUnrealUnitsPerPixel, 1), 2048);
	NewFactory->Height = Min<FLOAT>(Max<FLOAT>((2.0f * WorldWidth + 0.5f) / HeatmapVis->NumUnrealUnitsPerPixel, 1), 2048);
	UObject* NewObj = NewFactory->FactoryCreateNew( UTextureRenderTarget2D::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Transient, NULL, GWarn );
	UTextureRenderTarget2D* RendTarget = CastChecked<UTextureRenderTarget2D>(NewObj);

	// Get the setting for the orthographic viewport
	FSceneInterface* Scene = NULL;
	FViewport* Viewport = NULL;
	FEditorLevelViewportClient* LevelVC = NULL;
	for(INT i=0; i<GApp->EditorFrame->ViewportConfigData->GetViewportCount(); i++)
	{
		// Iterate over the 4 main viewports.
		LevelVC = GApp->EditorFrame->ViewportConfigData->AccessViewport(i).ViewportWindow;
		if(LevelVC && LevelVC->ViewportType == LVT_OrthoXY)
		{
			Viewport = LevelVC->Viewport;
			Scene = LevelVC->GetScene();
			break;
		}
	}

	// Center camera pointing straight down
	FRotator ViewRotation = FRotator(-16384,0,0);
	FVector ViewLocation = WorldCenter;

	// Make the screen shot slightly larger than the heatmap bounds
	// and ensure a minimal size 
	const float xScale = 1.2f;
	const float yScale = 1.2f;
	FLOAT MyWidth = Max<FLOAT>(WorldWidth * xScale, 1024);
	FLOAT MyHeight = Max<FLOAT>(WorldHeight * yScale, 1024);

	// If we found the viewport, render the scene to the probe now.
	if(LevelVC && Scene && Viewport)
	{	
		// view matrix to match current perspective viewport view orientation
		FMatrix ViewMatrix = FTranslationMatrix(-ViewLocation);
		ViewMatrix = ViewMatrix * FInverseRotationMatrix(ViewRotation);
		ViewMatrix = ViewMatrix * FMatrix(
			FPlane(0,	0,	1,	0),
			FPlane(1,	0,	0,	0),
			FPlane(0,	1,	0,	0),
			FPlane(0,	0,	0,	1));

		// Orthographic projection matrix
		FMatrix ProjectionMatrix = FOrthoMatrix(
			MyHeight / 2.0f,
			MyWidth / 2.0f,
			0.5f / HALF_WORLD_MAX,
			HALF_WORLD_MAX
			);

		FSceneViewFamilyContext ViewFamily(
			Viewport,
			Scene,
			SHOW_DefaultEditor,
			GWorld->GetTimeSeconds(),
			GWorld->GetDeltaSeconds(),
			GWorld->GetRealTimeSeconds(),
			LevelVC->IsRealtime(),
			FALSE);
		FSceneView* View = LevelVC->CalcSceneView(&ViewFamily);

		// render capture show flags use lit without shadows, fog, or post process effects
		EShowFlags CaptureShowFlags = SHOW_DefaultGame & (~SHOW_SceneCaptureUpdates) & (~SHOW_Fog) & (~SHOW_PostProcess);
		CaptureShowFlags = (CaptureShowFlags&~SHOW_ViewMode_Mask) | (SHOW_ViewMode_Lit & ~SHOW_Fog);

		// create the 2D capture probe, this is deleted on the render thread after the capture proxy renders
		FSceneCaptureProbe2D* CaptureProbe = new FSceneCaptureProbe2D(
			NULL, 
			RendTarget,
			CaptureShowFlags,
			FLinearColor::Black,
			0, 
			NULL,
			FALSE,
			FALSE,
			FALSE,
			FALSE,
			WORLD_MAX,
			WORLD_MAX,
			0.0,
			ViewMatrix,
			ProjectionMatrix 
			);

		// render the capture without relying on regular viewport draw loop by using a scene capture proxy
		FSceneCaptureProxy SceneCaptureProxy(Viewport,&ViewFamily);
		SceneCaptureProxy.Render(CaptureProbe,TRUE);

		// Render the heatmap on top of the scene
		FCanvas* Canvas = new FCanvas(RendTarget->GameThread_GetRenderTargetResource(), NULL);
		check(Canvas);

		FLOAT X = 0;
		FLOAT Y = 0;
		FLOAT SizeX = RendTarget->GetSurfaceWidth();
		FLOAT SizeY = RendTarget->GetSurfaceHeight();

		FMatrix ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;
		FMatrix InvViewProjectionMatrix = ViewProjectionMatrix.Inverse();

		// Calculate screen space coordinates of the data (ScreenToPixel, WorldToScreen from SceneView)
		FVector2D MinScreen;
		FVector4 WorldMinScreen = ViewProjectionMatrix.TransformFVector4(FVector4(HeatmapVis->WorldMinPos,1));
		if(WorldMinScreen.W > 0.0f)
		{
			FLOAT InvW = 1.0f / WorldMinScreen.W;
			MinScreen = FVector2D(
				X + (0.5f + WorldMinScreen.X * 0.5f * InvW) * SizeX,
				Y + (0.5f - WorldMinScreen.Y * 0.5f * InvW) * SizeY
				);
		}

		FVector2D MaxScreen;
		FVector4 WorldMaxScreen = ViewProjectionMatrix.TransformFVector4(FVector4(HeatmapVis->WorldMaxPos,1));
		if(WorldMaxScreen.W > 0.0f)
		{
			FLOAT InvW = 1.0f / WorldMaxScreen.W;
			MaxScreen = FVector2D(
				X + (0.5f + WorldMaxScreen.X * 0.5f * InvW) * SizeX,
				Y + (0.5f - WorldMaxScreen.Y * 0.5f * InvW) * SizeY
				);

		}

		HeatmapVis->VisualizeCanvas(Canvas, MinScreen, MaxScreen);
		Canvas->Flush();
		Canvas->SetRenderTarget(NULL);
		FlushRenderingCommands();
		delete Canvas;
		
		// Read the contents of the target into an array.
		FTextureRenderTargetResource* RenderTarget = RendTarget->GameThread_GetRenderTargetResource();	
		if( RenderTarget && 
			RendTarget->Format == PF_A8R8G8B8)
		{
			TArray<FColor> Bitmap;
			if( RenderTarget->ReadPixels(Bitmap) )
			{					
				// Save the contents of the array to a bitmap file.
				appCreateBitmap(*Filename, RendTarget->GetSurfaceWidth(), RendTarget->GetSurfaceHeight(), &Bitmap(0) ,GFileManager);
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	WxGameStatsPopUpMenu - right click menu for various options
-----------------------------------------------------------------------------*/
WxGameStatsPopUpMenu::WxGameStatsPopUpMenu(WxGameStatsVisualizer* GameStatsBrowser)
{
	check(GameStatsBrowser);

	// Upload to database command
	Append( IDM_GAMESTATSVISUALIZER_UPLOADFILE, *LocalizeUnrealEd("GameStatsVisualizer_UploadFile"), TEXT("") );
	Enable(IDM_GAMESTATSVISUALIZER_UPLOADFILE, false);
	
	AppendSeparator();
	
	// Create report sub menu
	wxMenu * SubMenu = new wxMenu();
	Append(-1, *LocalizeUnrealEd("GameStatsVisualizer_CreateReport"), SubMenu );
	SubMenu->Append( IDM_GAMESTATSVISUALIZER_CREATEREPORT, *LocalizeUnrealEd("GameStatsVisualizer_MultiplayerReport"), TEXT("") );
	SubMenu->Enable( IDM_GAMESTATSVISUALIZER_CREATEREPORT, true );	
}

WxGameStatsPopUpMenu::~WxGameStatsPopUpMenu()
{
}

/** 
 * Given a session and heatmap query description, generate heatmap image files
 * @param SessionID - session of interest
 * @param HeatmapQueries - definition of individual heatmap queries to run
 */
void WxGameStatsVisualizer::CreateHeatmaps(const FString& SessionID, const TArray<FHeatmapQuery>& HeatmapQueries)
{
	FGameStatsSearchQuery SearchQuery(EC_EventParm);

	// Fill out the basics for the heatmap queries
	SearchQuery.SessionIDs.AddItem(SessionID);

	FGameSessionInformation	SessionInfo(EC_EventParm);
	GameStatsDB->GetSessionInfoBySessionID(SessionID, SessionInfo);
	SearchQuery.StartTime = 0.0f;
	SearchQuery.EndTime = SessionInfo.GameplaySessionEndTime - SessionInfo.GameplaySessionStartTime;
	SearchQuery.PlayerIndices.AddItem(FSessionIndexPair(SessionID, FGameStatsSearchQuery::ALL_PLAYERS));
	SearchQuery.TeamIndices.AddItem(FSessionIndexPair(SessionID, FGameStatsSearchQuery::ALL_TEAMS));

	// Render the heatmap and add it to a screenshot of the scene
	UHeatmapVisualizer* HeatmapVis = ConstructObject<UHeatmapVisualizer>(UHeatmapVisualizer::StaticClass());
	if (HeatmapVis)
	{
		UBOOL bWorldPosSet = FALSE;
		FVector WorldMinPos(0.0f), WorldMaxPos(0.0f);

		HeatmapVis->Init();

		DOUBLE QueryTime, VisitTime;
		INT ResultCount;
		for (INT HeatmapQueryIdx = 0; HeatmapQueryIdx < HeatmapQueries.Num(); HeatmapQueryIdx++)
		{
			const FHeatmapQuery& CurrentQuery = HeatmapQueries(HeatmapQueryIdx);
			SearchQuery.EventIDs = CurrentQuery.EventIDs;
			if (QueryDatabase(SearchQuery, HeatmapVis, ResultCount, QueryTime, VisitTime))
			{
				// Save/restore the largest world bounds (based on location heatmap)
				if (!bWorldPosSet)
				{
					WorldMaxPos = HeatmapVis->WorldMaxPos;
					WorldMinPos = HeatmapVis->WorldMinPos;
					bWorldPosSet = TRUE;
				}
				else
				{
					HeatmapVis->WorldMaxPos = WorldMaxPos;
					HeatmapVis->WorldMinPos = WorldMinPos;
				}

				AddVisualizerOutput(TEXT("Creating heatmap for session."));
				// Adjust the heatmap thresholds
				HeatmapVis->NumUnrealUnitsPerPixel = 30.0f;
				HeatmapVis->CreateHeatmapGrid();
				HeatmapVis->UpdateDensityMapping(HeatmapVis->MinDensity, HeatmapVis->MaxDensity * .1f);
				HeatmapVis->CreateHeatmapTexture();
				CreateHeatmapScreenshot(HeatmapVis, CurrentQuery.ImageFilename);
			}
		}

		HeatmapVis->Cleanup();
	}
}

void WxGameStatsVisualizer::OnGameSessionListHandleCommand(wxCommandEvent &In)
{
	// Id of selection in popup menu.
	INT Command = In.GetId();
	//debugf(TEXT("OnGameSessionListHandleCommand. Command: %d"), Command);

	TArray<INT> SelectedSessions;
	GetSelectedItemsUserData(GameSessionInfoCtrl, SelectedSessions);

	if( Command == IDM_GAMESTATSVISUALIZER_UPLOADFILE )
	{
		// Convert all selected sessions to filenames (if they exist locally)
		for (INT SessionIter = 0; SessionIter < SelectedSessions.Num(); SessionIter++)
		{
			const INT SessionIndex = SelectedSessions(SessionIter);
			const FString& SessionID = GameSessionsInDB(SessionIndex);

			FString Question = FString::Printf(TEXT("Upload session %s?"), *SessionID);
			if( appMsgf( AMT_YesNo, *Question ) )
			{
				GWarn->BeginSlowTask( *LocalizeUnrealEd("Uploading game stats file"), TRUE );
				if (GameStatsDB->UploadSession(SessionID))
				{
					AddVisualizerOutput(FString::Printf(TEXT("Session %s uploaded."), *SessionID));
				}
				else
				{
					AddVisualizerOutput(FString::Printf(TEXT("Failed to upload session %s"), *SessionID));
				}
				GWarn->EndSlowTask();
			}
		}
	}
	else if (Command == IDM_GAMESTATSVISUALIZER_CREATEREPORT)
	{
		for (INT SessionIter = 0; SessionIter < SelectedSessions.Num(); SessionIter++)
		{
			FGameStatsReportWorker* Worker = NULL;

			const INT SessionIndex = SelectedSessions(SessionIter);
			const FString& SessionID = GameSessionsInDB(SessionIndex);
			const FString* SessionFilename = GameStatsDB->SessionFilenamesBySessionID.Find(SessionID);
			if (SessionFilename != NULL)
			{
				// Read from file implementation
				FGameStatsReportWorkerFile* FileWorker = new FGameStatsReportWorkerFile;
				if (FileWorker->Init(*SessionFilename))
				{
					Worker = FileWorker;
				}
				else
				{
					delete FileWorker;
				}
			}
			else if (GameStatsDB->HasRemoteConnection() && GameStatsDB->IsSessionIDLocal(SessionID) == FALSE)
			{
				// Read from database implementation
				FGameSessionInformation SessionInfo(EC_EventParm);
				GameStatsDB->GetSessionInfoBySessionID(SessionID, SessionInfo);
				FGameStatsReportWorkerDB* DBWorker = new FGameStatsReportWorkerDB;
				if (DBWorker->Init(SessionInfo))
				{
					Worker = DBWorker;
				}
				else
				{
					delete DBWorker;
				}
			}

			if (Worker)
			{
				// Write the XML
				AddVisualizerOutput(TEXT("Generating match report for session."));
				INT Result = Worker->Main();
				if (Result == ERROR_SUCCESS)
				{
					// Find the data the report wants for heatmap generation
					TArray<FHeatmapQuery> HeatmapQueries;
					Worker->GetHeatmapQueries(HeatmapQueries);

					if (HeatmapQueries.Num() > 0)
					{
						CreateHeatmaps(SessionID, HeatmapQueries);
					}

					// Launch the webpage if possible
					FString ReportURL = Worker->GetReportURL(RT_SingleSession);
					if (ReportURL.Len() > 0)
					{
						AddVisualizerOutput(TEXT("Launching report in browser."));
						appLaunchURL(*ReportURL);
					}
				}
				else
				{
					AddVisualizerOutput(TEXT("Failed to generate report."));
				}

				// Cleanup 
				delete Worker;
			}
		}
	}
}

/************************************************************************/
/* WxVisualizerTab	                                                    */
/************************************************************************/

BEGIN_EVENT_TABLE( WxVisualizerTab, wxNotebookPage )
EVT_CLOSE( WxVisualizerTab::OnClose )
EVT_SIZE( WxVisualizerTab::OnSize )
END_EVENT_TABLE()


WxVisualizerTab::WxVisualizerTab()
:	
	TabID(-1),
	Parent(NULL),
	VisualizerPanel(NULL),
	AvailableVisualizers(NULL),
	Visualizer(NULL),
	GameplayEventsListCtrl(NULL),
	PlayerListCtrl(NULL),
	TeamListCtrl(NULL),
	ResultCount(0),
	QueryFlags(0),
	bIsEnabled(FALSE)
{
	appMemzero(&Query, sizeof(FGameStatsSearchQuery));
}

WxVisualizerTab::~WxVisualizerTab()
{

}

/** Second step of creation, required to initialize the window */
UBOOL WxVisualizerTab::Create(wxWindow* InParent, UGameStatsVisualizer* InVisualizer)
{
	if ( !InParent || !InVisualizer || wxWindow::Create( InParent, -1, wxDefaultPosition, wxDefaultSize, 0, *InVisualizer->GetFriendlyName()) == FALSE)
	{
		return FALSE;
	}

	static INT NextTabID = 0;
	TabID = NextTabID++;

	//Give the visualizer a chance to initialize itself
	Visualizer = InVisualizer;
	Visualizer->Init();

	// Load the window hierarchy from the .xrc file
	VisualizerPanel = wxXmlResource::Get()->LoadPanel(this, TEXT("ID_FILTERS"));
	if (VisualizerPanel)
	{
		//Cache/Init some common controls
		GameplayEventsListCtrl = static_cast< wxListCtrl* >(
			VisualizerPanel->FindWindow( TEXT( "ID_EVENTSCTRL" ) ) );
		check( GameplayEventsListCtrl != NULL );

		InitEventsListControl();

		PlayerListCtrl = static_cast< wxListCtrl* >(
			VisualizerPanel->FindWindow( TEXT( "ID_PLAYERNAMES" ) ) );
		check( PlayerListCtrl != NULL );

		InitPlayerListControl();

		TeamListCtrl = static_cast< wxListCtrl* >(
			VisualizerPanel->FindWindow( TEXT( "ID_TEAMCTRL" ) ) );
		check( TeamListCtrl != NULL );

		InitTeamListControl();

		AvailableVisualizers = static_cast< wxChoice* >(
			VisualizerPanel->FindWindow( TEXT( "ID_VISUALIZERLIST" ) ) );
		check( AvailableVisualizers != NULL );

		wxCheckBox* EnabledBox = static_cast< wxCheckBox* >(
			VisualizerPanel->FindWindow( TEXT( "ID_VISENABLED" ) ) );
		check( EnabledBox != NULL );
		EnabledBox->SetValue(bIsEnabled ? true : false);

		//Localize separately because weird things happen (lost text/positioning) when I tried higher
		FLocalizeWindow( EnabledBox, FALSE, TRUE );

		wxButton* OptionsButton = static_cast< wxButton* >(
			VisualizerPanel->FindWindow( TEXT( "ID_VISOPTIONS" ) ) );
		check( OptionsButton != NULL );

		//Localize separately because weird things happen (lost text/positioning) when I tried higher
		FLocalizeWindow( OptionsButton, FALSE, TRUE );

		//Only allow options if its supported
		if (Visualizer->OptionsDialogName.Len() == 0)
		{
			OptionsButton->Enable(false);
		}
		else
		{
			OptionsButton->Enable(true);
		}
	}
																															 
	{
		//Event handler registration
		AddWxListEventHandler( XRCID( "ID_EVENTSCTRL" ), wxEVT_COMMAND_LIST_ITEM_SELECTED, WxVisualizerTab::OnEventsListItemSelected );	   
		AddWxListEventHandler( XRCID( "ID_EVENTSCTRL" ), wxEVT_COMMAND_LIST_ITEM_DESELECTED, WxVisualizerTab::OnEventsListItemSelected );

		AddWxListEventHandler( XRCID( "ID_TEAMCTRL" ), wxEVT_COMMAND_LIST_ITEM_SELECTED, WxVisualizerTab::OnTeamListItemSelected );
		AddWxListEventHandler( XRCID( "ID_TEAMCTRL" ), wxEVT_COMMAND_LIST_ITEM_DESELECTED, WxVisualizerTab::OnTeamListItemSelected );

		AddWxListEventHandler( XRCID( "ID_PLAYERNAMES" ), wxEVT_COMMAND_LIST_COL_CLICK, WxVisualizerTab::OnPlayerListColumnButtonClicked );
		AddWxListEventHandler( XRCID( "ID_PLAYERNAMES" ), wxEVT_COMMAND_LIST_ITEM_ACTIVATED, WxVisualizerTab::OnPlayerListItemActivated );
		AddWxListEventHandler( XRCID( "ID_PLAYERNAMES" ), wxEVT_COMMAND_LIST_ITEM_SELECTED, WxVisualizerTab::OnPlayerListItemSelected );
		AddWxListEventHandler( XRCID( "ID_PLAYERNAMES" ), wxEVT_COMMAND_LIST_ITEM_DESELECTED, WxVisualizerTab::OnPlayerListItemSelected );

		AddWxEventHandler( XRCID( "ID_VISENABLED" ), wxEVT_COMMAND_CHECKBOX_CLICKED , WxVisualizerTab::OnEnabled );
		AddWxEventHandler( XRCID( "ID_VISOPTIONS" ), wxEVT_COMMAND_BUTTON_CLICKED, WxVisualizerTab::OnOptionsClicked );

		AddWxEventHandler( XRCID( "ID_VISUALIZERLIST" ), wxEVT_COMMAND_CHOICE_SELECTED, WxVisualizerTab::OnAvailableVisualizerChoiceSelected );
	}

	return TRUE;
}

/** Set all the visualizer tab widgets back to initial state */
void WxVisualizerTab::Reset()
{
	InitPlayerListControl();
	InitTeamListControl();
	InitEventsListControl();
}

/** 
*  Get the string to display in the tab label area
*/
const FString& WxVisualizerTab::GetTabCaption() const
{
	return Visualizer->GetFriendlyName();
}

/** 
* Callback on whole window size adjustments
* @param In - Properties of the event triggered
*/
void WxVisualizerTab::OnSize( wxSizeEvent& In )
{
	if( VisualizerPanel )
	{
		VisualizerPanel->SetSize( GetClientRect() );
	}
}

/**
* Callback when the window is closed, shuts down the visualizer panel
* @param In - Properties of the event triggered
*/
void WxVisualizerTab::OnClose( wxCloseEvent& In )
{
	if (Visualizer != NULL)
	{
		Visualizer->Cleanup();
	}

	Destroy();
}

/** Initialize the event lists control */
void WxVisualizerTab::InitEventsListControl()
{
	check( GameplayEventsListCtrl != NULL );

	// Clear everything
	GameplayEventsListCtrl->DeleteAllColumns();
	GameplayEventsListCtrl->DeleteAllItems();
	GameplayEventsListUserData.Empty();

	// Add columns
	for( INT CurColumnIndex = 0; CurColumnIndex < NumEventListColumns; ++CurColumnIndex )
	{
		const FString LocalizedColumnName = LocalizeUnrealEd( EventListColumnNames[ CurColumnIndex ] );
		GameplayEventsListCtrl->InsertColumn(
			CurColumnIndex,				// ID
			*LocalizedColumnName,		// Name
			wxLIST_FORMAT_LEFT,			// Format
			-1 );						// Width
	}

	// Update list column sizes
	for( INT CurColumnIndex = 0; CurColumnIndex < NumEventListColumns; ++CurColumnIndex )
	{
		INT Width = 0;
		GameplayEventsListCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE );
		Width = Max( GameplayEventsListCtrl->GetColumnWidth( CurColumnIndex ), Width );
		GameplayEventsListCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE_USEHEADER );
		Width = Max( GameplayEventsListCtrl->GetColumnWidth( CurColumnIndex ), Width );
		GameplayEventsListCtrl->SetColumnWidth( CurColumnIndex, Width );
	}
}

/*
*   Update the events list control with all known events in the database
*   @param StatsDB - the database to query for all recorded events
*/
void WxVisualizerTab::UpdateEventsListControl(const FString& SessionID, TArray<FGameplayEventMetaData>& GameplayEvents)
{
	if (Parent->bAreControlsInitialized && Parent->GameStatsDB != NULL)
	{
		check( GameplayEventsListCtrl != NULL );

		// Add events
		for (INT EventIdx=0; EventIdx < GameplayEvents.Num(); EventIdx++)
		{
			const FGameplayEventMetaData& GameplayEvent = GameplayEvents(EventIdx);
			if (GameplayEvent.EventID == -1 || Visualizer->SupportedEvents.ContainsItem(GameplayEvent.EventID) == FALSE)
			{
				continue;
			}

			//Add this entry to the user data (uniquely, using index as a key whether or not we've set this before)
			INT UserDataIndex = GameplayEventsListUserData.AddUniqueItem(GameplayEvent.EventID);
			long ItemIndex = GameplayEventsListCtrl->FindItem(-1, UserDataIndex);
			if (ItemIndex < 0)
			{
				const INT EventTypeCount = Parent->GameStatsDB->GetEventCountByType(SessionID, GameplayEvent.EventID);
				if (EventTypeCount > 0)
				{
					// Add an empty string item
					const INT ItemIndex = GameplayEventsListCtrl->GetItemCount();
					GameplayEventsListCtrl->InsertItem( ItemIndex, TEXT( "" ) );

					// Event ID
					GameplayEventsListCtrl->SetItem( ItemIndex, EventID, *appItoa(GameplayEvent.EventID) );

					// Event Name
					GameplayEventsListCtrl->SetItem( ItemIndex, EventName, *GameplayEvent.EventName.ToString() );

					// Event Count
					GameplayEventsListCtrl->SetItem( ItemIndex, EventCount, *appItoa(EventTypeCount) );

					// Store the original index of this task
					GameplayEventsListCtrl->SetItemData( ItemIndex, UserDataIndex );
				}
			}
			else
			{
				//Get the aggregate count
				wxListItem ItemData;

				ItemData.SetId(ItemIndex);
				ItemData.SetColumn(EventCount);
				ItemData.SetMask(wxLIST_MASK_TEXT);
				GameplayEventsListCtrl->GetItem(ItemData);

				//Update the count
				INT Count = appAtoi(ItemData.GetText());
				Count += Parent->GameStatsDB->GetEventCountByType(SessionID, GameplayEvent.EventID);

				//Store it back
				ItemData.SetText(*appItoa(Count));
				GameplayEventsListCtrl->SetItem(ItemData);
			}
		}

		// Update list column sizes
		for( INT CurColumnIndex = 0; CurColumnIndex < NumEventListColumns; ++CurColumnIndex )
		{
			INT Width = 0;
			GameplayEventsListCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE );
			Width = Max( GameplayEventsListCtrl->GetColumnWidth( CurColumnIndex ), Width );
			GameplayEventsListCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE_USEHEADER );
			Width = Max( GameplayEventsListCtrl->GetColumnWidth( CurColumnIndex ), Width );
			GameplayEventsListCtrl->SetColumnWidth( CurColumnIndex, Width );
		}
	}
}

/*
*   Initialize the controls related to the team list control
*/
void WxVisualizerTab::InitTeamListControl()
{
	check( TeamListCtrl != NULL );

	// Clear everything
	TeamListCtrl->DeleteAllColumns();
	TeamListCtrl->DeleteAllItems();
	TeamListUserData.Empty();

	// Add columns
	for( INT CurColumnIndex = 0; CurColumnIndex < NumTeamListColumns; ++CurColumnIndex )
	{
		const FString LocalizedColumnName = LocalizeUnrealEd( TeamListColumnNames[ CurColumnIndex ] );
		TeamListCtrl->InsertColumn(
			CurColumnIndex,				// ID
			*LocalizedColumnName,		// Name
			wxLIST_FORMAT_LEFT,			// Format
			-1 );						// Width
	}

	// Update list column sizes
	for( INT CurColumnIndex = 0; CurColumnIndex < NumTeamListColumns; ++CurColumnIndex )
	{
		INT Width = 0;
		TeamListCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE );
		Width = Max( TeamListCtrl->GetColumnWidth( CurColumnIndex ), Width );
		TeamListCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE_USEHEADER );
		Width = Max( TeamListCtrl->GetColumnWidth( CurColumnIndex ), Width );
		TeamListCtrl->SetColumnWidth( CurColumnIndex, Width );
	}
}

/*
* Update the team list control to reflect the current team list
* @param TeamList - array of teams found in the game stats file
*/
void WxVisualizerTab::UpdateTeamListControl(const FString& SessionID, TArray<FTeamInformation>& TeamList)
{
	check( TeamListCtrl != NULL );

	// Add teams
	wxColor ItemColor;
	for (INT TeamIdx=0; TeamIdx < TeamList.Num(); TeamIdx++)
	{
		const FTeamInformation& TeamInfo = TeamList(TeamIdx);

		// Add an empty string item
		const INT ItemIndex = TeamListCtrl->GetItemCount();
		TeamListCtrl->InsertItem( ItemIndex, TEXT( "" ) );

		// Team Num
		TeamListCtrl->SetItem( ItemIndex, TeamNum, *appItoa(TeamInfo.TeamIndex) );

		// Team Name
		TeamListCtrl->SetItem( ItemIndex, TeamName, *TeamInfo.TeamName );

		// Team Color
		ItemColor.Set(TeamInfo.TeamColor.R, TeamInfo.TeamColor.G, TeamInfo.TeamColor.B);
		TeamListCtrl->SetItemBackgroundColour( ItemIndex, ItemColor );

		// Store the original index of this task
		FSessionIndexPair UserData(SessionID, TeamIdx);
		INT UserDataIndex = TeamListUserData.AddItem(UserData);
		TeamListCtrl->SetItemData( ItemIndex, UserDataIndex );
	}

	// Update list column sizes
	for( INT CurColumnIndex = 0; CurColumnIndex < NumTeamListColumns; ++CurColumnIndex )
	{
		INT Width = 0;
		TeamListCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE );
		Width = Max( TeamListCtrl->GetColumnWidth( CurColumnIndex ), Width );
		TeamListCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE_USEHEADER );
		Width = Max( TeamListCtrl->GetColumnWidth( CurColumnIndex ), Width );
		TeamListCtrl->SetColumnWidth( CurColumnIndex, Width );
	}
}

/** Called when an item in the stats event list is selected */
void WxVisualizerTab::OnEventsListItemSelected( wxListEvent& In )
{
	QueryFlags |= QUERY_EVENTSUPDATED;
}

/** 
* Called when an item in the team list is selected 
* @param In - Properties of the event triggered
*/
void WxVisualizerTab::OnTeamListItemSelected( wxListEvent& In )
{
	QueryFlags |= QUERY_TEAMSUPDATED;
}

/** 
* Called when an item in the task list is double-clicked
* @param In - Properties of the event triggered
*/
void WxVisualizerTab::OnPlayerListItemActivated( wxListEvent& In )
{
}

/** 
* Called when an item in the player list is selected
* @param In - Properties of the event triggered
*/
void WxVisualizerTab::OnPlayerListItemSelected( wxListEvent& In )
{
	QueryFlags |= QUERY_PLAYERSUPDATED;
}

/** 
* Called when a column button is clicked on the player list 
* @param In - Properties of the event triggered
*/
void WxVisualizerTab::OnPlayerListColumnButtonClicked( wxListEvent& In )
{
}

/** 
*  Poll all controls related to queries to fill in the required search query struct
*	@param SearchQuery - empty search query object to fill in
*/
void WxVisualizerTab::GenerateSearchQuery(FGameStatsSearchQuery& SearchQuery)
{
	//Get the time frame
	Parent->GetCurrentStartEndWindow(SearchQuery.StartTime, SearchQuery.EndTime);

	//Get the list of selected sessions
	Parent->GetSelectedSessions(SearchQuery.SessionIDs);

	//Get the list of selected players
	TArray<INT> SelectedPlayers;
	GetSelectedItemsUserData(PlayerListCtrl, SelectedPlayers);

	for (INT PlayerIndex = 0; PlayerIndex < SelectedPlayers.Num(); PlayerIndex++)
	{
		const FSessionIndexPair& UserData = PlayerListUserData(SelectedPlayers(PlayerIndex));
		SearchQuery.PlayerIndices.AddItem(UserData);
	}

	//Get the list of selected teams
	TArray<INT> SelectedTeams;
	GetSelectedItemsUserData(TeamListCtrl, SelectedTeams);

	for (INT TeamIndex = 0; TeamIndex < SelectedTeams.Num(); TeamIndex++)
	{
		const FSessionIndexPair& UserData = TeamListUserData(SelectedTeams(TeamIndex));
		SearchQuery.TeamIndices.AddItem(UserData);
	}

	//Get the list of selected events
	TArray<INT> SelectedEventIDs;
	GetSelectedItemsUserData(GameplayEventsListCtrl, SelectedEventIDs);

	if (SelectedEventIDs.Num() == 0)
	{
		// Only use events supported
		SearchQuery.EventIDs = Visualizer->SupportedEvents;
	}
	else
	{
		// Otherwise search specifically for user requested data
		for (INT EventIndex = 0; EventIndex < SelectedEventIDs.Num(); EventIndex++)
		{
			const INT UserData = GameplayEventsListUserData(SelectedEventIDs(EventIndex));
			SearchQuery.EventIDs.AddItem(UserData);
		}
	}

	//Make sure everything is filled out properly
	if (SearchQuery.PlayerIndices.Num() == 0)
	{
		for (INT SessionIter = 0; SessionIter < SearchQuery.SessionIDs.Num(); SessionIter++)
		{
			SearchQuery.PlayerIndices.AddItem(FSessionIndexPair(SearchQuery.SessionIDs(SessionIter), FGameStatsSearchQuery::ALL_PLAYERS));
		}
	}

	if (SearchQuery.TeamIndices.Num() == 0)
	{
		for (INT SessionIter = 0; SessionIter < SearchQuery.SessionIDs.Num(); SessionIter++)
		{
			SearchQuery.TeamIndices.AddItem(FSessionIndexPair(SearchQuery.SessionIDs(SessionIter), FGameStatsSearchQuery::ALL_TEAMS));
		}
	}

	if (SearchQuery.EventIDs.Num() == 0)
	{
		SearchQuery.EventIDs.AddItem(FGameStatsSearchQuery::ALL_EVENTS);
	}
}

/**
*   Called when any parameters of the query have changed, refreshes the visualizer
*/
void WxVisualizerTab::QueryUpdate(DOUBLE& OutQueryTime, DOUBLE& OutVisitTime)
{
	appMemzero(&Query, sizeof(FGameStatsSearchQuery));
	GenerateSearchQuery(Query);
	if (Parent->QueryDatabase(Query, Visualizer, ResultCount, OutQueryTime, OutVisitTime))
	{
		//Success
	}
	else
	{
		//Failure, should cleanup the last query
		Visualizer->Reset();
	}

	//Update the result count
	const FString& NewLabel = FString::Printf(TEXT("%s [%d/%d]"), *GetTabCaption(), Visualizer->GetVisualizationSetCount(), ResultCount);
	Parent->SetVisualizerTabLabel(TabID, *NewLabel);

	QueryFlags = 0;
}


/** 
* Called when a new filter is selected in the choice box
* @param In - Properties of the event triggered
*/
void WxVisualizerTab::OnAvailableVisualizerChoiceSelected( wxCommandEvent& In )
{
	//Parent->AddVisualizerOutput(FString::Printf(TEXT("%s selected"), AvailableVisualizers->GetStringSelection().GetData()));

	//Create a new visualizer (only if different)
	INT VisIdx = AvailableVisualizers->GetSelection();
	UClass* VisClassToCreate = Parent->VisualizerClasses(VisIdx);

	if (Visualizer && Visualizer->IsA(VisClassToCreate))
	{
		return;
	}

	UGameStatsVisualizer* NewVis = ConstructObject<UGameStatsVisualizer>(VisClassToCreate);
	if (NewVis != NULL)
	{
		//Remove previous options if open
		WxVisualizerOptionsDialog* Dialog = Visualizer->GetOptionsDialog();
		if (Dialog)
		{
			Dialog->Hide();
		}

		//Cleanup the old visualizer
		Visualizer->Cleanup();

		//Initialize the new visualizer
		NewVis->Init();
		Visualizer = NewVis;

		//update caption
		Parent->SetVisualizerTabLabel(TabID, GetTabCaption());

		//Only allow options if its supported
		wxButton* OptionsButton = static_cast< wxButton* >(
			VisualizerPanel->FindWindow( TEXT( "ID_VISOPTIONS" ) ) );
		check( OptionsButton != NULL );
		if (Visualizer->OptionsDialogName.Len() == 0)
		{
			OptionsButton->Enable(false);
		}
		else
		{
			OptionsButton->Enable(true);
		}

		// Reset the controls
		Reset();

		TArray<INT> SelectedSessions;
		GetSelectedItemsUserData(Parent->GameSessionInfoCtrl, SelectedSessions);

		for (INT SessionIter = 0; SessionIter < SelectedSessions.Num(); SessionIter++)
		{
			TArray<FGameplayEventMetaData> GameplayEvents; 
			TArray<FPlayerInformation> PlayerList;
			TArray<FTeamInformation> TeamList;

			const INT SessionIndex = SelectedSessions(SessionIter);
			const FString& SessionID = Parent->GameSessionsInDB(SessionIndex);
			Parent->GameStatsDB->GetPlayersListBySessionID(SessionID, PlayerList);
			Parent->GameStatsDB->GetEventsListBySessionID(SessionID, GameplayEvents);
			Parent->GameStatsDB->GetTeamListBySessionID(SessionID, TeamList);

			//Update the player list
			UpdatePlayerListControl(SessionID, PlayerList);
			//Update the events list
			UpdateEventsListControl(SessionID, GameplayEvents);
			//Update the team list
			UpdateTeamListControl(SessionID, TeamList);
		}

		QueryFlags = QUERY_VISUALIZERCHANGED;
	}
	else
	{
		Parent->AddVisualizerOutput(FString::Printf(TEXT("Unable to create visualizer %s."), *VisClassToCreate->GetName()));
	}

	//Redraw the viewports
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

/*
* Initialize the player list control
*/
void WxVisualizerTab::InitPlayerListControl()
{
	check( PlayerListCtrl != NULL );

	// Clear everything
	PlayerListCtrl->DeleteAllColumns();
	PlayerListCtrl->DeleteAllItems();
	PlayerListUserData.Empty();

	// Add columns
	for( INT CurColumnIndex = 0; CurColumnIndex < NumPlayerListColumns; ++CurColumnIndex )
	{
		const FString LocalizedColumnName = LocalizeUnrealEd( PlayerListColumnNames[ CurColumnIndex ] );
		PlayerListCtrl->InsertColumn(
			CurColumnIndex,				// ID
			*LocalizedColumnName,		// Name
			wxLIST_FORMAT_LEFT,			// Format
			-1 );						// Width
	}

	// Update list column sizes
	for( INT CurColumnIndex = 0; CurColumnIndex < NumPlayerListColumns; ++CurColumnIndex )
	{
		INT Width = 0;
		PlayerListCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE );
		Width = Max( PlayerListCtrl->GetColumnWidth( CurColumnIndex ), Width );
		PlayerListCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE_USEHEADER );
		Width = Max( PlayerListCtrl->GetColumnWidth( CurColumnIndex ), Width );
		PlayerListCtrl->SetColumnWidth( CurColumnIndex, Width );
	}
}

/*
* Update the player list control to reflect the current player list
* @param SessionID - session this player list is relevant to
* @param PlayerList - array of players found in the game stats file
*/
void WxVisualizerTab::UpdatePlayerListControl(const FString& SessionID, TArray<FPlayerInformation>& PlayerList)
{
	extern FColor GetColorByPlayerIndex(const INT PlayerIndex);
	check( PlayerListCtrl != NULL );

	// Add players
	wxColor ItemColor;
	for (INT PlayerIdx=0; PlayerIdx < PlayerList.Num(); PlayerIdx++)
	{
		const FPlayerInformation& Player = PlayerList(PlayerIdx);

		// Add an empty string item
		const INT ItemIndex = PlayerListCtrl->GetItemCount();
		PlayerListCtrl->InsertItem( ItemIndex, TEXT( "" ) );

		// Player name
		PlayerListCtrl->SetItem( ItemIndex, PlayerName, *Player.PlayerName );

		// Bot-ness
		PlayerListCtrl->SetItem( ItemIndex, IsBot, Player.bIsBot ? TEXT("YES") : TEXT("NO") );

		// Setup a color for the player
		FColor PlayerColor = GetColorByPlayerIndex(PlayerIdx);
		ItemColor.Set(PlayerColor.R, PlayerColor.G, PlayerColor.B);
		PlayerListCtrl->SetItemTextColour( ItemIndex, ItemColor );

		// Store the original index of this task
		FSessionIndexPair UserData(SessionID, PlayerIdx);
		INT UserDataIndex = PlayerListUserData.AddItem(UserData);
		PlayerListCtrl->SetItemData( ItemIndex, UserDataIndex );
	}

	// Update list column sizes
	for( INT CurColumnIndex = 0; CurColumnIndex < NumPlayerListColumns; ++CurColumnIndex )
	{
		INT Width = 0;
		PlayerListCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE );
		Width = Max( PlayerListCtrl->GetColumnWidth( CurColumnIndex ), Width );
		PlayerListCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE_USEHEADER );
		Width = Max( PlayerListCtrl->GetColumnWidth( CurColumnIndex ), Width );
		PlayerListCtrl->SetColumnWidth( CurColumnIndex, Width );
	}
}


/**
* Callback when the tab is enabled/disabled by the user
* @param In - Properties of the event triggered
*/
void WxVisualizerTab::OnEnabled( wxCommandEvent& In )
{
	bIsEnabled = In.IsChecked();
	
	//Redraw the viewports
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

/**
* Callback when options button is clicked
* @param In - Properties of the event triggered
*/
void WxVisualizerTab::OnOptionsClicked( wxCommandEvent& In )
{
	if (Visualizer != NULL)
	{
		//Get the dialog from the visualizer
		WxVisualizerOptionsDialog* OptionsDialog = Visualizer->GetOptionsDialog();
		if (OptionsDialog)
		{
			OptionsDialog->Show();
		}
	}
}

/************************************************************************/
/* WxGameStatsVisualizer                                                */
/************************************************************************/


BEGIN_EVENT_TABLE( WxGameStatsVisualizer, WxBrowser )
	EVT_CLOSE( WxGameStatsVisualizer::OnClose )
	EVT_SIZE( WxGameStatsVisualizer::OnSize )
	EVT_MENU( IDM_GAMESTATSVISUALIZER_UPLOADFILE, WxGameStatsVisualizer::OnGameSessionListHandleCommand)
	EVT_MENU( IDM_GAMESTATSVISUALIZER_CREATEREPORT, WxGameStatsVisualizer::OnGameSessionListHandleCommand)
END_EVENT_TABLE()


/** WxGameStatsVisualizer constructor */
WxGameStatsVisualizer::WxGameStatsVisualizer()
:	MainBrowserPanel(NULL), 
	VisualizerTabRoot(NULL),
	GameStatsDB(NULL),
	bForceQueryUpdate(FALSE),
	DateFilterList(NULL),
	GameTypeFilterList(NULL),
	GameSessionInfoCtrl(NULL),
	TimeScrubSlider(NULL),
	StartSpinner(NULL),
	EndSpinner(NULL),
	VisualizerOutputCtrl(NULL),
	bAreControlsInitialized(FALSE),
	bIsDBInitialized(FALSE),
	bSteppingTime(FALSE),
	CurrentTime(0.0f),
	TooltipVisualizerHit(NULL),
	ToolTipStatIndex(INDEX_NONE),
	ToolTipX(0),
	ToolTipY(0),
	ToolTipViewport(NULL)
{
	// We want to receive callbacks when a map change happens. 
	GCallbackEvent->Register( CALLBACK_MapChange, this );
}

/** WxGameStatsVisualizer destructor */
WxGameStatsVisualizer::~WxGameStatsVisualizer()
{
	// Unregister all callbacks
	GCallbackEvent->UnregisterAll( this );
}

/**
* Forwards the call to our base class to create the window relationship.
* Creates any internally used windows after that
*
* @param	DockID			The unique id to associate with this dockable window
* @param	FriendlyName	The friendly name to assign to this window
* @param	Parent			The parent of this window (should be a Notebook)
*/
void WxGameStatsVisualizer::Create( INT DockID, const TCHAR* FriendlyName, wxWindow* Parent )
{
	// Call parent implementation
	WxBrowser::Create( DockID, FriendlyName, Parent );

	// Menu bar
	{
		MenuBar = new wxMenuBar();

		// Append the docking menu choices
		WxBrowser::AddDockingMenu( MenuBar );
	}

	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));

	CreateControls();

	check(GApp);
	GApp->GameStatsVisualizer = this;
}

/** Called when the browser is getting activated (becoming the visible window in it's dockable frame). */
void WxGameStatsVisualizer::Activated()
{
	WxBrowser::Activated();

	if (!bIsDBInitialized)
	{
		extern const FString GetMapNameStatic();
		const FString& LoadedMap = GetMapNameStatic();

		//Remove all the visualizers that were created
		//@TODO - Hide the dialog for the visualizer options

		//Reset all controls
		InitAllControls();

		//Fill up the database
		InitDB(LoadedMap);
	}
}

/** Tells the browser to update itself */
void WxGameStatsVisualizer::Update()
{
	// Visual refresh.  Nothing to do here, since we rely on other events to trigger updates.
}

/**
* Called from within UnLevTic.cpp after ticking all actors or from
* the rendering thread (depending on bIsRenderingThreadObject)
*
* @param DeltaTime	Game time passed since the last call.
*/
void WxGameStatsVisualizer::Tick(FLOAT DeltaTime)
{
	if (bAreControlsInitialized)
	{
		if (bSteppingTime)
		{
			//Get the time window
			INT TimeSize = EndSpinner->GetValue() - StartSpinner->GetValue();
			INT TimeHalfSize = TimeSize / 2;

			//We can only step to the thumb position which is in the middle of the time window
			const FLOAT MaxTime = TimeScrubSlider->GetMax() - TimeHalfSize;
			CurrentTime = Min(CurrentTime + DeltaTime, (FLOAT)MaxTime); 
			SetTimeControls(CurrentTime);
			if (CurrentTime >= MaxTime)
			{
				bSteppingTime = FALSE;
				//Renable related widgets
				StartSpinner->Enable(TRUE);
				EndSpinner->Enable(TRUE);
				TimeScrubSlider->Enable(TRUE);
			}

			bForceQueryUpdate = TRUE;
		}

		//Check for any query updates required
		UpdateAllQueries();
	}
}

/**
 * Listens for CALLBACK_MapChange events.  Clears all references to actors in the current level.
 */
void WxGameStatsVisualizer::Send( ECallbackEventType InType, DWORD InFlag )
{
	if( InType == CALLBACK_MapChange )
	{
		if (InFlag == MapChangeEventFlags::NewMap)
		{
			OnLoadMap();
		}
		else if (InFlag == MapChangeEventFlags::WorldTornDown)
		{
			OnCloseMap();
		}
	}
}

/** FSerializableObject: Serialize object references for garbage collector */
void WxGameStatsVisualizer::Serialize( FArchive& Ar )
{
	//The database is a UObject
	if (GameStatsDB)
	{
	   Ar << GameStatsDB;
	}

	//All the visualizers are UObjects
	const INT PageCount = VisualizerTabRoot->GetPageCount();
	for (INT PageIdx = 0; PageIdx < PageCount; PageIdx++)
	{
		WxVisualizerTab* Page = static_cast<WxVisualizerTab*>(VisualizerTabRoot->GetPage(PageIdx));
		if (Page && Page->Visualizer)
		{
			Ar << Page->Visualizer;
		}
	}
}

/** Give the window a chance to create all controls relevant to successful operation */
void WxGameStatsVisualizer::CreateControls()
{
	// Load the window hierarchy from the .xrc file
	MainBrowserPanel = wxXmlResource::Get()->LoadPanel(this, TEXT("ID_VISUALIZER_PANEL"));
	check(MainBrowserPanel);

	// Setup event handlers
	{
		AddWxEventHandler( XRCID( "ID_DATELIST" ), wxEVT_COMMAND_CHOICE_SELECTED, WxGameStatsVisualizer::OnDateListSelected );
		AddWxEventHandler( XRCID( "ID_GAMETYPELIST" ), wxEVT_COMMAND_CHOICE_SELECTED, WxGameStatsVisualizer::OnGameTypeListSelected );

		AddWxListEventHandler( XRCID( "ID_GAMESESSIONINFO" ), wxEVT_COMMAND_LIST_ITEM_SELECTED, WxGameStatsVisualizer::OnGameSessionListItemSelected );
		AddWxListEventHandler( XRCID( "ID_GAMESESSIONINFO" ), wxEVT_COMMAND_LIST_ITEM_DESELECTED, WxGameStatsVisualizer::OnGameSessionListItemSelected );
		AddWxListEventHandler( XRCID( "ID_GAMESESSIONINFO" ), wxEVT_COMMAND_LIST_ITEM_ACTIVATED, WxGameStatsVisualizer::OnGameSessionListItemActivated );
		AddWxListEventHandler( XRCID( "ID_GAMESESSIONINFO" ), wxEVT_COMMAND_LIST_ITEM_RIGHT_CLICK, WxGameStatsVisualizer::OnGameSessionListRightClick );

		AddWxSpinEventHandler( XRCID( "ID_FILTERSTARTTIME" ), wxEVT_COMMAND_SPINCTRL_UPDATED, WxGameStatsVisualizer::OnDurationStartFilterUpdated );
		AddWxSpinEventHandler( XRCID( "ID_FILTERENDTIME" ), wxEVT_COMMAND_SPINCTRL_UPDATED, WxGameStatsVisualizer::OnDurationEndFilterUpdated );

		AddWxScrollEventHandler( XRCID( "ID_TIMESCRUBSLIDER" ), wxEVT_SCROLL_CHANGED, WxGameStatsVisualizer::OnTimeScrubUpdate );
		AddWxScrollEventHandler( XRCID( "ID_TIMESCRUBSLIDER" ), wxEVT_SCROLL_THUMBTRACK, WxGameStatsVisualizer::OnTimeScrubTrack );

		AddWxEventHandler( XRCID( "ID_PLAYTOGGLE" ), wxEVT_COMMAND_BUTTON_CLICKED, WxGameStatsVisualizer::OnPlayToggle );
		AddWxEventHandler( XRCID( "ID_STOPTOGGLE" ), wxEVT_COMMAND_BUTTON_CLICKED, WxGameStatsVisualizer::OnStopToggle );

		AddNotebookEventHandler( XRCID( "ID_VISUALIZERBOOK" ), wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED, WxGameStatsVisualizer::OnNotebookPageChanged );
	}

	// Localize all of the static text in the loaded window
	// doesn't do visualizer tabs because they aren't created yet (handled separately)
	FLocalizeWindow( MainBrowserPanel, FALSE, TRUE );

	InitAllControls();

	bAreControlsInitialized = TRUE;
}

/** 
* Called when the visualizer tab changes
* @param In - Properties of the event triggered
*/
void WxGameStatsVisualizer::OnNotebookPageChanged( wxNotebookEvent& In )
{
	if( VisualizerTabRoot )
	{
		// Remember the old page.
		int OldPageIndex = In.GetOldSelection();
		if (OldPageIndex >= 0)
		{
			WxVisualizerTab* VisTab = static_cast<WxVisualizerTab*>(VisualizerTabRoot->GetPage(OldPageIndex));
			if (VisTab && VisTab->Visualizer)
			{
				WxVisualizerOptionsDialog* Dialog = VisTab->Visualizer->GetOptionsDialog();
				if (Dialog)
				{
					Dialog->Hide();
				}
			}
		}
	}

	In.Skip();
}

/** Resets all widget controls to their default state */
void WxGameStatsVisualizer::InitAllControls()
{
	//Initialize textbox first so we can output to the screen as necessary
	InitGameStatsOutputControl();

	//Initalize the filters
	InitGameStatsFilterControl();

	//Top panel list of all game sessions
	InitGameSessionInfoControl();

	//Tabbed visualizer widget
	InitVisualizerControls();

	//Time bar
	InitTimeScrubControls();
}

/** Initialize the controls related to the output window */
void WxGameStatsVisualizer::InitGameStatsOutputControl()
{
	VisualizerOutputCtrl = static_cast< wxTextCtrl* >(
		MainBrowserPanel->FindWindow( TEXT( "ID_VISRESULTS" ) ) );
	check( VisualizerOutputCtrl != NULL );

	VisualizerOutputCtrl->Clear();
}

/** Initialize the controls related to top level filters */
void WxGameStatsVisualizer::InitGameStatsFilterControl()
{
	DateFilterList = static_cast< wxChoice* >(
		MainBrowserPanel->FindWindow( TEXT( "ID_DATELIST" ) ) );
	check( DateFilterList != NULL );

	DateFilterList->Clear();
	for (INT DateFilterIdx=0; DateFilterIdx<GSDF_MAX; DateFilterIdx++)
	{
		const FString LocalizedDateFilter = LocalizeUnrealEd( GameStatsDateFiltersNames[ DateFilterIdx ] );
		DateFilterList->AppendString(*LocalizedDateFilter);
	}

	DateFilterList->SetSelection(GSDF_Today);

	GameTypeFilterList = static_cast< wxChoice* >(
		MainBrowserPanel->FindWindow( TEXT( "ID_GAMETYPELIST" ) ) );
	check( GameTypeFilterList != NULL );
	GameTypeFilterList->Clear();
}

/**
 *   Update the game types filter
 * @param GameTypes - the gametypes to populate the filter
 */
void WxGameStatsVisualizer::UpdateGameTypesFilterControl(const TArray<FString>& GameTypes)
{
	check( GameTypeFilterList != NULL );

	GameTypeFilterList->Clear();
	GameTypeFilterList->AppendString(TEXT("Any"));

	for (INT i=0; i<GameTypes.Num(); i++)
	{
	   GameTypeFilterList->AppendString(*GameTypes(i));
	}

	GameTypeFilterList->SetSelection(0);
}

/*
*   Initialize the controls related to available visualizers
*/
void WxGameStatsVisualizer::InitVisualizerControls()
{
	VisualizerTabRoot = static_cast< wxNotebook* >(
		MainBrowserPanel->FindWindow( TEXT( "ID_VISUALIZERBOOK" ) ) );
	check( VisualizerTabRoot != NULL );
	VisualizerTabRoot->DeleteAllPages();

	//Discover all available visualizers in the editor
	if (VisualizerClasses.Num() == 0)
	{
		// Construct list of non-abstract gameplay visualizer object classes.
		for(TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if( !(Class->ClassFlags & CLASS_Abstract) && !(Class->ClassFlags & CLASS_Hidden) && !(Class->ClassFlags & CLASS_Deprecated) )
			{
				if(Class->IsChildOf(UGameStatsVisualizer::StaticClass()))
				{
					//Hold onto the list of all available classes
					VisualizerClasses.AddItem(Class);
				}
			}
		}
	}

	// Create all the visualizer tabs(initially one per type)
	for (INT ClassIdx = 0; ClassIdx < VisualizerClasses.Num(); ClassIdx++)
	{
		UClass* Class = VisualizerClasses(ClassIdx);

		//Create a visualizer to stick in the tab
		UGameStatsVisualizer* NewVis = ConstructObject<UGameStatsVisualizer>(Class);
		if (NewVis == NULL)
		{
			AddVisualizerOutput(FString::Printf(TEXT("Unable to create visualizer for %s."), *Class->GetName()));
			continue;
		}

		//Create a tab for each visualizer type
		WxVisualizerTab* NewTab = new WxVisualizerTab();
		if (NewTab)
		{
			// Only enable the first visualizer by default
			NewTab->bIsEnabled = ClassIdx == 0 ? TRUE : FALSE;
			if (NewTab->Create(VisualizerTabRoot, NewVis))
			{
				//Access to the main window
				NewTab->Parent = this;

				INT MyClassIdx = 0;

				//Give each tab a list of the other choices so we can swap between them
				for (INT OtherClassIdx = 0; OtherClassIdx < VisualizerClasses.Num(); OtherClassIdx++)
				{
					UClass* OtherClass = VisualizerClasses(OtherClassIdx);
					UGameStatsVisualizer* VisObj = OtherClass->GetDefaultObject<UGameStatsVisualizer>();
					const FString& VisName = VisObj->GetFriendlyName();
					NewTab->AvailableVisualizers->AppendString(*VisName);

					if (Class == OtherClass)
					{
						MyClassIdx = OtherClassIdx;
					}
				}

				//Set my choice box to the class type in use
				NewTab->AvailableVisualizers->SetSelection(MyClassIdx);

				//Add the page
				VisualizerTabRoot->AddPage(NewTab, *NewTab->GetTabCaption(), false);
			}
			else
			{
				AddVisualizerOutput(FString::Printf(TEXT("Unable to create tab for %s."), *NewVis->GetFriendlyName()));
				NewTab->Destroy();
			}
		}
	}

	//Finalize the notebook
	VisualizerTabRoot->Layout();
	VisualizerTabRoot->ChangeSelection(0);
}

/**
* Set the label on a given visualizer tab
* @param TabID - the unique ID for the tab to set the label on
* @param Label - the string to display
*/
void WxGameStatsVisualizer::SetVisualizerTabLabel(INT TabID, const FString& Label)
{
	if (VisualizerTabRoot)
	{
		const INT TabCount = VisualizerTabRoot->GetPageCount();
		for (INT TabIdx = 0; TabIdx < TabCount; TabIdx++)
		{
			WxVisualizerTab* Page = static_cast<WxVisualizerTab*>(VisualizerTabRoot->GetPage(TabIdx));
			if (Page->TabID == TabID)
			{
				VisualizerTabRoot->SetPageText(TabIdx, *Label);
			}
		}
	}
}

/**
*   Get the list of session IDs current selected in the UI
*   @param OutArray - array of sessions IDs selected
*/
void WxGameStatsVisualizer::GetSelectedSessions(TArray<FString>& OutArray)
{
	OutArray.Empty();

	//Get the list of selected sessions
	TArray<INT> SelectedSessions;
	GetSelectedItemsUserData(GameSessionInfoCtrl, SelectedSessions);

	if (SelectedSessions.Num() > 0)
	{
		for (INT SessionIndex = 0; SessionIndex < SelectedSessions.Num(); SessionIndex++)
		{
			INT SessionDBIndex = SelectedSessions(SessionIndex);
			if (GameSessionsInDB.IsValidIndex(SessionDBIndex))
			{
				OutArray.AddItem(GameSessionsInDB(SessionDBIndex));
			}
		}
	}
}

/** 
* Return the time adjusted value for the current database times requested 
* @param StartTime - out value for start time in database unit
* @param EndTime - out value for end time in database units
*/
void WxGameStatsVisualizer::GetCurrentStartEndWindow(INT& StartTime, INT& EndTime)
{
	if (GameStatsDB == NULL)
	{
		return;
	}

	//If you can manipulate the time controls or are stepping time, get the values from it
	if (bSteppingTime || (StartSpinner->IsEnabled() && EndSpinner->IsEnabled() && TimeScrubSlider->IsEnabled()))
	{
		StartTime = StartSpinner->GetValue();
		EndTime = EndSpinner->GetValue();
	}
	else
	{
		//Otherwise just consider all time
		StartTime = 0;
		EndTime = MAXINT;
	}
}

/*
*   Initialize the controls related to the search criteria
*/
void WxGameStatsVisualizer::InitTimeScrubControls()
{
	//Setup the time shuttle control
	TimeScrubSlider = static_cast< wxSlider* >(
		MainBrowserPanel->FindWindow( TEXT( "ID_TIMESCRUBSLIDER" ) ) );
	check( TimeScrubSlider != NULL );
	TimeScrubSlider->SetRange(0,0);
	TimeScrubSlider->SetSelection(0,20);
	TimeScrubSlider->Enable(FALSE);

	//Set the start/end ranges for the duration spinners
	StartSpinner = static_cast< wxSpinCtrl* >(
		MainBrowserPanel->FindWindow( TEXT( "ID_FILTERSTARTTIME" ) ) );
	check( StartSpinner != NULL );

	StartSpinner->SetRange(0,0);
	StartSpinner->SetValue(0);
	StartSpinner->Enable(FALSE);

	EndSpinner = static_cast< wxSpinCtrl* >(
		MainBrowserPanel->FindWindow( TEXT( "ID_FILTERENDTIME" ) ) );
	check( EndSpinner != NULL );

	EndSpinner->SetRange(9999, 9999);
	EndSpinner->SetValue(9999);
	EndSpinner->Enable(FALSE);

	//Play button
	wxBitmapButton* PlayButton = static_cast< wxBitmapButton* >(
		MainBrowserPanel->FindWindow( TEXT( "ID_PLAYTOGGLE" ) ) );
	PlayB.Load( TEXT("MAT_Play") );
	PlayButton->SetBitmapLabel(PlayB);
	PlayButton->Enable(FALSE);

	//Stop button
	wxBitmapButton* StopButton = static_cast< wxBitmapButton* >(
		MainBrowserPanel->FindWindow( TEXT( "ID_STOPTOGGLE" ) ) );
	StopB.Load( TEXT("MAT_Stop") );
	StopButton->SetBitmapLabel(StopB);
	StopButton->Enable(FALSE);
}

/*
 *   Update the GUI to reflect the current available options for the search criteria
 */
void WxGameStatsVisualizer::UpdateTimeScrubControls(INT SessionStart, INT SessionEnd)
{
	if (bAreControlsInitialized && (GameStatsDB != NULL))
	{
		EnableTimeControls(TRUE);

		//Set the start/end ranges for the duration spinners
		const INT StartTime = 0;
		const INT TimeWindow = (INT)((SessionEnd - SessionStart) * 0.2f);

		StartSpinner->SetRange(StartTime, StartTime + TimeWindow); 
		StartSpinner->SetValue(StartTime);

		EndSpinner->SetRange(StartTime, SessionEnd - SessionStart);
		EndSpinner->SetValue(StartTime + TimeWindow);

		//Time scrub control
		INT ThumbSize = EndSpinner->GetValue() - StartSpinner->GetValue();

		//Position the slider in the middle of the default selection range
		INT HalfSize = ThumbSize / 2;
		TimeScrubSlider->SetRange(0, SessionEnd - SessionStart);
		TimeScrubSlider->SetSelection(0, ThumbSize);
		TimeScrubSlider->SetValue(HalfSize);
	}
}

/** 
* Set the enable state of the time controls (spinners/sliders/play/stop)
*   @param bEnabled - TRUE to enable the controls, FALSE to disable
*/
void WxGameStatsVisualizer::EnableTimeControls(UBOOL bEnabled)
{
	bool bEnableCast = bEnabled ? true : false;
	StartSpinner->Enable(bEnableCast);
	EndSpinner->Enable(bEnableCast);
	TimeScrubSlider->Enable(bEnableCast);

	//Enable the play/stop buttons
	wxBitmapButton* PlayButton = static_cast< wxBitmapButton* >(
		MainBrowserPanel->FindWindow( TEXT( "ID_PLAYTOGGLE" ) ) );
	PlayButton->Enable(bEnableCast);
	wxBitmapButton* StopButton = static_cast< wxBitmapButton* >(
		MainBrowserPanel->FindWindow( TEXT( "ID_STOPTOGGLE" ) ) );
	StopButton->Enable(bEnableCast);

	//Regardless of state, stop playback
	bSteppingTime = FALSE;
}

/** 
* Called when the play button is toggled, starts ticking the query
* @param In - event callback data
*/
void WxGameStatsVisualizer::OnPlayToggle( wxCommandEvent& In )
{
	//Disable manipulation of the widgets while playing (for now?)
	StartSpinner->Enable(FALSE);
	EndSpinner->Enable(FALSE);
	TimeScrubSlider->Enable(FALSE);

	//Start playback at the position of the slider
	bSteppingTime = TRUE;
	CurrentTime = TimeScrubSlider->GetValue();
}

/** 
* Called when the stop button is toggled, stops ticking the query
* @param In - event callback data
*/
void WxGameStatsVisualizer::OnStopToggle( wxCommandEvent& In )
{
	//Enable manipulation of the widgets while playing (for now?)
	StartSpinner->Enable(TRUE);
	EndSpinner->Enable(TRUE);
	TimeScrubSlider->Enable(TRUE);

	//Stop playback
	bSteppingTime = FALSE;
}

/** 
*  Adjust the start/end spinners and slider position to represent a given time
* @param TimePosition - the middle time position the spinners/slider should represent
*/
void WxGameStatsVisualizer::SetTimeControls( INT TimePosition )
{
	if (bAreControlsInitialized)
	{
		//Get the old time selection size
		INT TimeSize = EndSpinner->GetValue() - StartSpinner->GetValue();
		INT TimeHalfSize = TimeSize / 2;

		//Get the new middle position of the slider (clamped to range)
		INT TimeMiddle = Max(StartSpinner->GetMin() + TimeHalfSize, TimePosition);
		TimeMiddle = Min(TimeMiddle, EndSpinner->GetMax() - TimeHalfSize);
		TimeScrubSlider->SetValue(TimeMiddle);

		//Set the start/end ranges for the duration spinners
		INT TimeBegin = Max(StartSpinner->GetMin(), TimeMiddle - TimeHalfSize);
		INT TimeEnd = Min(TimeMiddle + TimeHalfSize, EndSpinner->GetMax());

		TimeScrubSlider->SetSelection(TimeBegin, TimeEnd);

		//Set the start/end ranges for the duration spinners (ranges first so values won't be clamped)
		StartSpinner->SetRange(StartSpinner->GetMin(), TimeEnd);
		EndSpinner->SetRange(TimeBegin, EndSpinner->GetMax());

		StartSpinner->SetValue(TimeBegin);
		EndSpinner->SetValue(TimeEnd);
		bForceQueryUpdate = TRUE;
	}
}

/** 
 * Called at the end of an action against the time control slider
 * @param In - event callback data
 */
void WxGameStatsVisualizer::OnTimeScrubUpdate( wxScrollEvent& In )
{
	if (bAreControlsInitialized)
	{
		SetTimeControls(In.GetPosition());
	}
}

/** 
* Called during mouse held movement of the thumb icon
* @param In - event callback data
*/
void WxGameStatsVisualizer::OnTimeScrubTrack( wxScrollEvent& In )
{
	if (bAreControlsInitialized)
	{
		SetTimeControls(In.GetPosition());
	}
}

/** 
* Event handler for clicking on the up/down buttons of the Start Time search criteria
* @param In - Properties of the event triggered
*/
void WxGameStatsVisualizer::OnDurationStartFilterUpdated( wxSpinEvent& In )
{
	if (bAreControlsInitialized)
	{
		//The End Time new "min" is the Start Time current value so we never invert
		INT TimeBegin = StartSpinner->GetValue();
		INT TimeEnd = EndSpinner->GetValue();
		INT HalfTime = (TimeEnd - TimeBegin) / 2;

		EndSpinner->SetRange(TimeBegin, EndSpinner->GetMax());

		TimeScrubSlider->SetValue(TimeBegin + HalfTime);
		TimeScrubSlider->SetSelection(TimeBegin, TimeEnd);

		bForceQueryUpdate = TRUE;
	}
}

/** 
* Event handler for clicking on the up/down buttons of the End Time search criteria
* @param In - Properties of the event triggered
*/
void WxGameStatsVisualizer::OnDurationEndFilterUpdated( wxSpinEvent& In )
{
	if (bAreControlsInitialized)
	{
		//The Start Time new "max" is the End Time current value so we never invert
		INT TimeBegin = StartSpinner->GetValue();
		INT TimeEnd = EndSpinner->GetValue();
		INT HalfTime = (TimeEnd - TimeBegin) / 2;

		StartSpinner->SetRange(StartSpinner->GetMin(), TimeEnd);

		TimeScrubSlider->SetValue(TimeBegin + HalfTime);
		TimeScrubSlider->SetSelection(TimeBegin, TimeEnd);

		bForceQueryUpdate = TRUE;
	}
}

/** Update all currently active queries with the current time settings */
void WxGameStatsVisualizer::UpdateAllQueries()
{
	UBOOL bUpdatedAnyTab = FALSE;
	DOUBLE TotalQueryTime = 0;
	DOUBLE TotalVisitTime = 0;

	DOUBLE CurrentQueryTime, CurrentVisitTime;
	INT VisTabCount = VisualizerTabRoot->GetPageCount();
	for (INT VisTabIdx = 0; VisTabIdx < VisTabCount; VisTabIdx++)
	{
		WxVisualizerTab* VisTab = static_cast<WxVisualizerTab*>(VisualizerTabRoot->GetPage(VisTabIdx));
		if (bForceQueryUpdate || (VisTab->QueryFlags != 0))
		{
			VisTab->QueryUpdate(CurrentQueryTime, CurrentVisitTime);
			TotalQueryTime += CurrentQueryTime;
			TotalVisitTime += CurrentVisitTime;
			bUpdatedAnyTab = TRUE;
		}
	}

	if (bUpdatedAnyTab)
	{
		if (TotalQueryTime > 0 || TotalVisitTime > 0)
		{
			AddVisualizerOutput(*FString::Printf(TEXT("Query update took %.2f secs, visit took %.2f secs"), TotalQueryTime, TotalVisitTime));
		}

		//Redraw the viewports
		GCallbackEvent->Send( CALLBACK_RedrawAllViewports );

		//Nullify the tooltip
		TooltipVisualizerHit = NULL;
		ToolTipStatIndex = INDEX_NONE;
		ToolTipX = 0;
		ToolTipY = 0;
		ToolTipViewport = NULL;
	}

	bForceQueryUpdate = FALSE;
}

/** 
 * Query the connected database, passing relevant data to the visualizer expected to handle the data
 * @param SearchQuery - the query to run on the database
 * @param Visualizer - the visualizer to receive/process the data
 * @param TotalResults - out value of total search results found
 * @param TotalQueryTime - out value of time taken to search the database
 * @param TotalVisitTiem - out value of time taken to process the data by the visualizer
 * @return TRUE is the query was successful (Visualizer has something to work with) and FALSE otherwise
 */
UBOOL WxGameStatsVisualizer::QueryDatabase(const FGameStatsSearchQuery& SearchQuery, UGameStatsVisualizer* Visualizer, INT& TotalResults, DOUBLE& TotalQueryTime, DOUBLE& TotalVisitTime)
{
	if( GameStatsDB == NULL )
	{
		return FALSE;
	}

	FGameStatsRecordSet QueryResults;

	DOUBLE QueryStartTime, VisitStartTime, EndTime;
	QueryStartTime = appSeconds();

	//Query the database to return the array of relevant indices found in the database
	GameStatsDB->QueryDatabase(SearchQuery, QueryResults);
	TotalResults = QueryResults.GetNumResults();

	VisitStartTime = appSeconds();

	UBOOL bSuccess = FALSE;
	//Make sure there is something to process
	if (TotalResults > 0)
	{
		//Let the visualizer get a look at all the data
		if (GameStatsDB->VisitEntries(QueryResults, Visualizer))
		{
			EndTime = appSeconds();
			bSuccess = TRUE;
		}
		else
		{
			EndTime = appSeconds();
			AddVisualizerOutput(TEXT("Visualizer reported failure."));
		}
	}
	else
	{
		EndTime = appSeconds();
		AddVisualizerOutput(TEXT("Query returned no results."));
	}

	//Tally up performance information
	TotalQueryTime = VisitStartTime - QueryStartTime;
	TotalVisitTime = EndTime - VisitStartTime;
	return bSuccess;
}
  
/** 
 * Adds a string entry into the output window of the visualizer
 * @param Output - string to display in the window
 */
void WxGameStatsVisualizer::AddVisualizerOutput( const FString& Output )
{
	VisualizerOutputCtrl->AppendText(*FString::Printf(TEXT("[%s] %s\n"), appTimestamp(), *Output));
	VisualizerOutputCtrl->MarkDirty();
}


/** @return the Date filter in current use */
GameStatsDateFilters WxGameStatsVisualizer::GetDateFilter()
{
	check(DateFilterList);
	return (GameStatsDateFilters)DateFilterList->GetSelection();
}

/** @return the GameType filter in current use */
FString WxGameStatsVisualizer::GetGameTypeFilter()
{
	check(GameTypeFilterList);
	// Return whats in the combo box for now (ideally friendly name with userdata lookup)
	return GameTypeFilterList->GetStringSelection().GetData();
}

/*
 * Called when the user selects a date filter from the list
 * @param In - Properties of the event triggered
 */
void WxGameStatsVisualizer::OnDateListSelected( wxCommandEvent& In )
{
	AddVisualizerOutput(FString::Printf(TEXT("%s selected"), DateFilterList->GetStringSelection().GetData()));

	// Update the game session control
	PopulateGameSessions(GetDateFilter(), GetGameTypeFilter());

	INT NumVisPages = VisualizerTabRoot->GetPageCount();
	for (INT VisTabIdx = 0; VisTabIdx < NumVisPages; VisTabIdx++)
	{
		WxVisualizerTab* VisTab = static_cast<WxVisualizerTab*>(VisualizerTabRoot->GetPage(VisTabIdx));
		VisTab->Reset();
	}

	//Refresh the visualizers
	bForceQueryUpdate = TRUE;
}

/*
 * Called when the user selects a game type filter from the list
 * @param In - Properties of the event triggered
 */
void WxGameStatsVisualizer::OnGameTypeListSelected( wxCommandEvent& In )
{
	AddVisualizerOutput(FString::Printf(TEXT("%s selected"), GameTypeFilterList->GetStringSelection().GetData()));

	// Update the game session control
	PopulateGameSessions(GetDateFilter(), GetGameTypeFilter());

	INT NumVisPages = VisualizerTabRoot->GetPageCount();
	for (INT VisTabIdx = 0; VisTabIdx < NumVisPages; VisTabIdx++)
	{
		WxVisualizerTab* VisTab = static_cast<WxVisualizerTab*>(VisualizerTabRoot->GetPage(VisTabIdx));
		VisTab->Reset();
	}

	//Refresh the visualizers
	bForceQueryUpdate = TRUE;
}

/** Initialize the controls related to the game session info list */
void WxGameStatsVisualizer::InitGameSessionInfoControl()
{
	GameSessionInfoCtrl = static_cast< wxListCtrl* >(
		MainBrowserPanel->FindWindow( TEXT( "ID_GAMESESSIONINFO" ) ) );
	check( GameSessionInfoCtrl != NULL );

	// Clear everything
	GameSessionInfoCtrl->DeleteAllColumns();
	GameSessionInfoCtrl->DeleteAllItems();

	// Add columns
	for( INT CurColumnIndex = 0; CurColumnIndex < NumGameSessionsColumns; ++CurColumnIndex )
	{
		const FString LocalizedColumnName = LocalizeUnrealEd( GameSessionColumnNames[ CurColumnIndex ] );
		GameSessionInfoCtrl->InsertColumn(
			CurColumnIndex,				// ID
			*LocalizedColumnName,		// Name
			wxLIST_FORMAT_LEFT,			// Format
			-1 );						// Width
	}

	// Update list column sizes
	for( INT CurColumnIndex = 0; CurColumnIndex < NumGameSessionsColumns; ++CurColumnIndex )
	{
		INT Width = 0;
		GameSessionInfoCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE );
		Width = Max( GameSessionInfoCtrl->GetColumnWidth( CurColumnIndex ), Width );
		GameSessionInfoCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE_USEHEADER );
		Width = Max( GameSessionInfoCtrl->GetColumnWidth( CurColumnIndex ), Width );
		GameSessionInfoCtrl->SetColumnWidth( CurColumnIndex, Width );
	}
}

/** 
 * Update the game session control with information for the given game session
 * @param GameSessionInfo - info to add to the control
 * @TODO break out into Init/Update function for multiple files
 */
void WxGameStatsVisualizer::UpdateGameSessionInfo(TArray<FGameSessionInformation>& GameSessionInfoList)
{
	GameSessionInfoCtrl = static_cast< wxListCtrl* >(
		MainBrowserPanel->FindWindow( TEXT( "ID_GAMESESSIONINFO" ) ) );
	check( GameSessionInfoCtrl != NULL );

	// Clear everything
	GameSessionInfoCtrl->DeleteAllItems();

	wxColor LocalItemColor(60, 60, 220);
	wxColor RemoteItemColor(0, 0, 0);

	// Populate the game session info fields
	for (INT GameSessionIndex = 0; GameSessionIndex < GameSessionInfoList.Num(); GameSessionIndex++)
	{
		const INT ItemIndex = GameSessionInfoCtrl->GetItemCount();

		const FGameSessionInformation& GameSessionInfo = GameSessionInfoList(GameSessionIndex);
		const FString SessionID = GameSessionInfo.GetSessionID();
		UBOOL bIsLocal = GameStatsDB->IsSessionIDLocal(SessionID); 
		
		GameSessionInfoCtrl->InsertItem( ItemIndex, TEXT( "" ) );
		GameSessionInfoCtrl->SetItem( ItemIndex, AppTitleID, *FString::Printf(TEXT("0x%08x"), GameSessionInfo.AppTitleID) );
		GameSessionInfoCtrl->SetItem( ItemIndex, GameplaySessionID, *SessionID );
		GameSessionInfoCtrl->SetItem( ItemIndex, GameplayMapName, *GameSessionInfo.MapName );
		GameSessionInfoCtrl->SetItem( ItemIndex, GameplayMapURL, *GameSessionInfo.MapURL );
	

		// UTC conversion
		time_t TimestampSeconds = appStrToSeconds(*GameSessionInfo.GameplaySessionTimestamp);
		if (TimestampSeconds > 0)
		{
			INT Year, Month, DayOfWeek, Day, Hour, Min, Sec;
			TimestampSeconds += appUTCOffset() * 60;
			appSecondsToLocalTime(TimestampSeconds, Year, Month, DayOfWeek, Day, Hour, Min, Sec);

			GameSessionInfoCtrl->SetItem( ItemIndex, GameplaySessionTimestamp, *FString::Printf(TEXT("%02d/%02d/%04d %02d:%02d:%02d"), Month, Day, Year, Hour, Min, Sec) );
		}
		else
		{
			GameSessionInfoCtrl->SetItem( ItemIndex, GameplaySessionTimestamp, TEXT("UNKNOWN"));
		}

		// Make pretty time
		FLOAT SessionDuration = GameSessionInfo.GameplaySessionEndTime - GameSessionInfo.GameplaySessionStartTime;
		const FString PrettyTime = appPrettyTime(SessionDuration);
		GameSessionInfoCtrl->SetItem( ItemIndex, GamePlaySessionDuration, *PrettyTime);
		GameSessionInfoCtrl->SetItem( ItemIndex, PlatformType, *appItoa(GameSessionInfo.PlatformType) );
		GameSessionInfoCtrl->SetItem( ItemIndex, Language, *GameSessionInfo.Language );
		GameSessionInfoCtrl->SetItemData( ItemIndex, GameSessionsInDB.FindItemIndex(SessionID) );

		GameSessionInfoCtrl->SetItemTextColour( ItemIndex, bIsLocal ? LocalItemColor : RemoteItemColor);
	}

	// Update list column sizes
	for( INT CurColumnIndex = 0; CurColumnIndex < NumGameSessionsColumns; ++CurColumnIndex )
	{
		INT Width = 0;
		GameSessionInfoCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE );
		Width = Max( GameSessionInfoCtrl->GetColumnWidth( CurColumnIndex ), Width );
		GameSessionInfoCtrl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE_USEHEADER );
		Width = Max( GameSessionInfoCtrl->GetColumnWidth( CurColumnIndex ), Width );
		GameSessionInfoCtrl->SetColumnWidth( CurColumnIndex, Width );
	}
}

/*
* Called when the user selects a game session from the list
* @param In - Properties of the event triggered
*/
void WxGameStatsVisualizer::OnGameSessionListItemSelected( wxListEvent& In )
{
	if (bAreControlsInitialized)
	{
		BeginUpdate();

		INT NumVisPages = VisualizerTabRoot->GetPageCount();
		//Clear the query element lists
		for (INT VisTabIdx = 0; VisTabIdx < NumVisPages; VisTabIdx++)
		{
			WxVisualizerTab* VisTab = static_cast<WxVisualizerTab*>(VisualizerTabRoot->GetPage(VisTabIdx));
			VisTab->Reset();
		}

		TArray<INT> SelectedSessions;
		GetSelectedItemsUserData(GameSessionInfoCtrl, SelectedSessions);

		for (INT SessionIter = 0; SessionIter < SelectedSessions.Num(); SessionIter++)
		{
			TArray<FGameplayEventMetaData> GameplayEvents; 
			TArray<FPlayerInformation> PlayerList;
			TArray<FTeamInformation> TeamList;

			const INT SessionIndex = SelectedSessions(SessionIter);
			const FString& SessionID = GameSessionsInDB(SessionIndex);
			if (!GameStatsDB->IsSessionIDLocal(SessionID))
			{
				DOUBLE PopulateStart = appSeconds();
				// Make sure we cache the data
				GameStatsDB->PopulateSessionData(SessionID);
				DOUBLE PopulateEnd = appSeconds();
				AddVisualizerOutput(FString::Printf(TEXT("Populate database took %.2f secs"), PopulateEnd - PopulateStart));
			}
			
			GameStatsDB->GetPlayersListBySessionID(SessionID, PlayerList);
			GameStatsDB->GetEventsListBySessionID(SessionID, GameplayEvents);
			GameStatsDB->GetTeamListBySessionID(SessionID, TeamList);

			for (INT VisTabIdx = 0; VisTabIdx < NumVisPages; VisTabIdx++)
			{
				WxVisualizerTab* VisTab = static_cast<WxVisualizerTab*>(VisualizerTabRoot->GetPage(VisTabIdx));

				//Update the player list
				VisTab->UpdatePlayerListControl(SessionID, PlayerList);
				//Update the events list
				VisTab->UpdateEventsListControl(SessionID, GameplayEvents);
				//Update the team list
				VisTab->UpdateTeamListControl(SessionID, TeamList);
			}
		}

		//Only allow time manipulation for single sessions
		if (SelectedSessions.Num() == 1)
		{
			FGameSessionInformation SessionInfo(EC_EventParm);
			GameStatsDB->GetSessionInfoBySessionID(GameSessionsInDB(SelectedSessions(0)), SessionInfo);
			INT SessionStart = appFloor(SessionInfo.GameplaySessionStartTime);
			INT SessionEnd = appCeil(SessionInfo.GameplaySessionEndTime);
			UpdateTimeScrubControls(SessionStart, SessionEnd);
		}
		else
		{
			EnableTimeControls(FALSE);
		}

		//Refresh the visualizers
		bForceQueryUpdate = TRUE;
		EndUpdate();
	}
}

/*
 * Called when the user double clicks a game session from the list
 * @param In - Properties of the event triggered
 */
void WxGameStatsVisualizer::OnGameSessionListItemActivated( wxListEvent& In )
{
}

/*
 * Called when the user right clicks a game session from the list
 * @param In - Properties of the event triggered
 */
void WxGameStatsVisualizer::OnGameSessionListRightClick( wxListEvent& In )
{
	// Pop up menu
	WxGameStatsPopUpMenu Menu( this );
	FTrackPopupMenu TrackMenu( this, &Menu );
	TrackMenu.Show();
}

/** Called by editor to render stats graphics in 2D viewports. */
void WxGameStatsVisualizer::RenderStats(FEditorLevelViewportClient* ViewportClient, const FSceneView* View, FCanvas* Canvas, ELevelViewportType ViewportType)
{
	if (ViewportClient == NULL || ViewportClient->Viewport == NULL || IsShownOnScreen() == FALSE)
	{
		 return;
	}

	if (ViewportType != LVT_Perspective)
	{
		//Give every known visualizer a chance to draw itself
		INT VisTabCount = VisualizerTabRoot->GetPageCount();
		for (INT VisTabIdx = 0; VisTabIdx < VisTabCount; VisTabIdx++)
		{
			WxVisualizerTab* VisTab = static_cast<WxVisualizerTab*>(VisualizerTabRoot->GetPage(VisTabIdx));
			if (VisTab->bIsEnabled)
			{
				VisTab->Visualizer->VisualizeCanvas(ViewportClient, View, Canvas, ViewportType);
			}
		}
	}

	// If we have a tool tip to draw in this viewport...
	if(ViewportClient->Viewport->IsCursorVisible() && TooltipVisualizerHit != NULL && ToolTipStatIndex != INDEX_NONE && ToolTipViewport == ViewportClient && GCurrentLevelEditingViewportClient == ViewportClient)
	{
		FString TooltipString;
		TooltipVisualizerHit->GetMetadata(ToolTipStatIndex, TooltipString);

		//Figure out the text height
		FTextSizingParameters Parameters(GEngine->SmallFont,1.0f,1.0f);
		UCanvas::CanvasStringSize(Parameters, *TooltipString);
		INT YL = appTrunc(Parameters.DrawYL);

		//Parse string into lines
		TArray<FString> TooltipLines;
		INT NumLines = TooltipString.ParseIntoArray(&TooltipLines, TEXT("\n"), FALSE);

		//Render the tooltip
		INT YPos = ToolTipY;
		for (INT i=0; i<NumLines; i++)
		{
			DrawShadowedString(Canvas, ToolTipX + 15, YPos, *TooltipLines(i), GEngine->SmallFont, FColor(255,255,255));
			YPos += YL;
		}
	}
}

/** Called by editor to render stats graphics in perspective viewports. */
void WxGameStatsVisualizer::RenderStats3D(const FSceneView* View, class FPrimitiveDrawInterface* PDI, ELevelViewportType ViewportType)
{
	if (IsShownOnScreen() == FALSE)
	{
		return;
	}

	//Give every known visualizer a chance to draw itself
	INT VisTabCount = VisualizerTabRoot->GetPageCount();
	for (INT VisTabIdx = 0; VisTabIdx < VisTabCount; VisTabIdx++)
	{
		WxVisualizerTab* VisTab = static_cast<WxVisualizerTab*>(VisualizerTabRoot->GetPage(VisTabIdx));
		if (VisTab->bIsEnabled)
		{
			VisTab->Visualizer->Visualize(View, PDI, ViewportType);
		}
	}
}

/** Called by editor when mouse moves */
void WxGameStatsVisualizer::MouseMoved(FEditorLevelViewportClient* ViewportClient, INT X, INT Y)
{
	//if this window is visible AND this is the active viewport
	if(IsShownOnScreen() && GCurrentLevelEditingViewportClient == ViewportClient)
	{
		HHitProxy* HitResult = ViewportClient->Viewport->GetHitProxy(X, Y);
		if( HitResult && HitResult->IsA(HGameStatsHitProxy::StaticGetType()) )
		{
			HGameStatsHitProxy* StatProxy = (HGameStatsHitProxy*)HitResult;

			if (StatProxy->Visualizer != TooltipVisualizerHit || StatProxy->StatIndex != ToolTipStatIndex)
			{
				TooltipVisualizerHit = StatProxy->Visualizer;							
				ToolTipStatIndex = StatProxy->StatIndex;
				ToolTipX = X;
				ToolTipY = Y;
				ToolTipViewport = ViewportClient;
				ViewportClient->Viewport->InvalidateDisplay();
			}
		}
		else
		{
			if (TooltipVisualizerHit != NULL)
			{
				ViewportClient->Viewport->InvalidateDisplay();
			}

			TooltipVisualizerHit = NULL;
			ToolTipStatIndex = INDEX_NONE;
			ToolTipX = 0;
			ToolTipY = 0;
			ToolTipViewport = NULL;
		}
	}
}

/** Called by editor when key pressed. */
void WxGameStatsVisualizer::InputKey(FEditorLevelViewportClient* ViewportClient, FName Key,EInputEvent Event)
{
	if (ViewportClient->ViewportType == LVT_OrthoXY)
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

				if( HitResult && HitResult->IsA(HGameStatsHitProxy::StaticGetType()) )
				{
					HGameStatsHitProxy* StatProxy = (HGameStatsHitProxy*)HitResult;
					//debugf(TEXT("Hit proxy found 0x%08x, idx: %d, vis: %s"), StatProxy, StatProxy->StatIndex, *StatProxy->Visualizer->GetFriendlyName());
					StatProxy->Visualizer->HandleHitProxy(StatProxy);
				}
			}
		}
	}
}

/**
* Callback when a game stat file is requested to be open
* @param In - Properties of the event triggered
*/
// void WxGameStatsVisualizer::OnOpenFileClicked(wxCommandEvent& In)
// {		
// #if 0
// 	WxFileDialog* OpenDialog = new WxFileDialog(
// 		this, *LocalizeUnrealEd(ImportGameStatsFileLocString), TEXT(""), TEXT(""),
// 		TEXT("Stats files (*") GAME_STATS_FILE_EXT TEXT(")|*") GAME_STATS_FILE_EXT,
// 		wxFD_OPEN, wxDefaultPosition);
// 
// 	// Creates a "open file" dialog
// 	if (OpenDialog->ShowModal() == wxID_OK) // if the user click "Open" instead of "cancel"
// 	{
// 		wxString CurrentFile = OpenDialog->GetPath();
// 		FString Filename(CurrentFile.GetData());
// 	}
// #endif
// }

/** Called when a map is closed/torn down */
void WxGameStatsVisualizer::OnCloseMap()
{
	INT VisTabCount = VisualizerTabRoot->GetPageCount();
	for (INT VisTabIdx = 0; VisTabIdx < VisTabCount; VisTabIdx++)
	{
		WxVisualizerTab* VisTab = static_cast<WxVisualizerTab*>(VisualizerTabRoot->GetPage(VisTabIdx));
		if (VisTab && VisTab->Visualizer)
		{
			VisTab->Visualizer->Cleanup();
		}
	}

	// Unload the previous database
	if (GameStatsDB != NULL)
	{
		GameStatsDB->ClearDatabase();
	}
	bIsDBInitialized = FALSE;
}

/** Called when a new map is loaded */
void WxGameStatsVisualizer::OnLoadMap()
{
	if (IsReallyShown(this))
	{
		extern const FString GetMapNameStatic();
		const FString& LoadedMap = GetMapNameStatic();

		//Remove all the visualizers that were created
		//@TODO - Hide the dialog for the visualizer options

		//Reset all controls
		InitAllControls();

		//Fill up the database
		InitDB(LoadedMap);
	}
}

/** 
 * Collate the data from files on disk as well as the remote database 
 * @param LoadedMap - map name to filter the database on
 */
void WxGameStatsVisualizer::InitDB(const FString& LoadedMap)
{
	// Unload the previous database
	if (GameStatsDB != NULL)
	{
		GameStatsDB->ClearDatabase();
	}
	else
	{
		//@TODO load game custom database from string
		FString DatabaseClassname;
		if(!GConfig->GetString( TEXT("UnrealEd.GameStatsBrowser"), TEXT("GameStatsDBClassname"), DatabaseClassname, GEditorIni))
		{
			AddVisualizerOutput(FString::Printf(TEXT("Failed to find the game stats database classname in Editor.ini")));
			return;
		}

		UClass* DBClass = LoadClass<UGameStatsDatabase>(NULL, *DatabaseClassname, NULL, LOAD_None, NULL);
		if (DBClass == NULL)
		{
			AddVisualizerOutput(FString::Printf(TEXT("Failed to load class %s for game stats visualization"), *DatabaseClassname));
			return;
		}

		GameStatsDB = ConstructObject<UGameStatsDatabase>(DBClass);

		if (GameStatsDB == NULL || GameStatsDB->IsA(UGameStatsDatabase::StaticClass()) == FALSE)
		{
			GameStatsDB = NULL;
			AddVisualizerOutput(FString::Printf(TEXT("Class %s is not a valid game stats database type"), *DatabaseClassname));
			return;
		}

		//Store off a mapping of map name to filename one time
		GameStatsDB->CacheLocalFilenames();
	}

	if (GameStatsDB != NULL)
	{
		DOUBLE PopulateStart = appSeconds();

		GameStatsDateFilters DateFilter = GetDateFilter();

		// Parse local files and make remote database queries
		GameStatsDB->Init(LoadedMap, DateFilter);

		// Populate the game type filter
		TArray<FString> GameTypes;
		GameStatsDB->GetGameTypes(GameTypes);
		UpdateGameTypesFilterControl(GameTypes);

		// Populate the game sessions control filtered by date/gametype
		PopulateGameSessions(DateFilter, GetGameTypeFilter());

		DOUBLE PopulateEnd = appSeconds();
		AddVisualizerOutput(FString::Printf(TEXT("Populate database took %.2f secs"), PopulateEnd - PopulateStart));
		bIsDBInitialized = TRUE;
	}
	else
	{
		AddVisualizerOutput(FString::Printf(TEXT("Failed to populate database because database doesn't exist")));
	}
}

/** 
 *	Fill out the game session control using filters specified by the user
 * @param DateFilter - date range to filter
 * @param GameTypeFilter - string representing GameClass
 */
void WxGameStatsVisualizer::PopulateGameSessions(GameStatsDateFilters DateFilter, const FString& GameTypeFilter)
{
	TArray<FGameSessionInformation> GameSessionInfos;

	//Enumerate session information in the database
	GameSessionsInDB.Empty();
	if (GameStatsDB)
	{
		GameStatsDB->GetSessionIDs(DateFilter, GameTypeFilter, GameSessionsInDB);
		for (INT GameSessionIndex = 0; GameSessionIndex < GameSessionsInDB.Num(); GameSessionIndex++)
		{
			INT NewIndex = GameSessionInfos.AddZeroed();
			const FString& SessionID = GameSessionsInDB(GameSessionIndex);
			GameStatsDB->GetSessionInfoBySessionID(SessionID, GameSessionInfos(NewIndex));
		}
	}

	//Repopulate the game session info list
	UpdateGameSessionInfo(GameSessionInfos);
}

/**
 * Callback when the window is closed, shuts down the visualizer panel
 * @param In - Properties of the event triggered
 */
void WxGameStatsVisualizer::OnClose(wxCloseEvent& In)
{
	//Disconnect the database
	if (GameStatsDB != NULL)
	{
		GameStatsDB->ClearDatabase();
		GameStatsDB = NULL;
	}

	//Remove all the visualizers that were created
	//@TODO - Hide the dialog for the visualizer options

	Destroy();

	//Remove any data we had drawn
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

/** 
 * Callback on whole window size adjustments
 * @param In - Properties of the event triggered
 */
void WxGameStatsVisualizer::OnSize( wxSizeEvent& In )
{
	// During the creation process a sizing message can be sent so don't
	// handle it until we are initialized
	if( bAreControlsInitialized )
	{
		// Freeze the window
		BeginUpdate();

		// Resize controls
		MainBrowserPanel->SetSize( GetClientRect() );

		// Thaw the window
		EndUpdate();
	}
}

