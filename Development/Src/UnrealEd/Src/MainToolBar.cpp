/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "MainToolBar.h"
#include "DlgTransform.h"
#include "UnConsoleSupportContainer.h"
#include "GameStatsBrowser.h"
#include "SourceControl.h"
#include "EditorBuildUtils.h"
#include "MRUFavoritesList.h"
#include "Factories.h"
#include "Kismet.h"

#if WITH_MANAGED_CODE
	// CLR includes
	#include "ContentBrowserShared.h"
#endif


extern class WxUnrealEdApp* GApp;

BEGIN_EVENT_TABLE( WxMainToolBar, WxToolBar )
	EVT_TOOL_RCLICKED( ID_EDIT_TRANSLATE, WxMainToolBar::OnTransformButtonRightClick )
	EVT_TOOL_RCLICKED( ID_EDIT_ROTATE, WxMainToolBar::OnTransformButtonRightClick )
	EVT_TOOL_RCLICKED( ID_EDIT_SCALE, WxMainToolBar::OnTransformButtonRightClick )
	EVT_TOOL_RCLICKED( ID_EDIT_SCALE_NONUNIFORM, WxMainToolBar::OnTransformButtonRightClick )
	EVT_TOOL_RCLICKED( IDM_LIGHTING_QUALITY, WxMainToolBar::OnLightingQualityButtonRightClick )
	EVT_TOOL_RCLICKED( IDM_BUILD_AI_PATHS, WxMainToolBar::OnBuildAIPathsButtonRightClick )
	EVT_RIGHT_UP( WxMainToolBar::OnRightButtonUpOnControl )
	EVT_BUTTON( IDM_BuildPlayInEditorButton, WxMainToolBar::OnPlayInEditorButtonClick )
	EVT_TOOL_RCLICKED_RANGE(IDM_BuildPlayConsole_START, IDM_BuildPlayConsole_END, WxMainToolBar::OnPlayOnConsoleRightClick )
	EVT_TOOL_RCLICKED( IDM_PlayUsingMobilePreview, WxMainToolBar::OnPlayUsingMobilePreviewRightClick )
	EVT_TOOL( IDM_PlayUsingMobilePreview_Settings, WxMainToolBar::OnPlayUsingMobilePreviewRightClick )
	EVT_TOOL( IDM_KISMET_REALTIMEDEBUGGING, WxMainToolBar::OnRealtimeKismetDebugging )
	EVT_MENU( IDM_LIGHTING_QUALITY, WxMainToolBar::OnLightingQualityButtonLeftClick )
	EVT_TOOL_RCLICKED( IDM_REALTIME_AUDIO, WxMainToolBar::OnRealTimeAudioRightClick )
	EVT_UPDATE_UI( IDM_FAVORITES_TOGGLE_BUTTON, WxMainToolBar::UI_ToggleFavorites )
	EVT_UPDATE_UI( IDM_BuildPlayInEditorButton, WxMainToolBar::UI_PlayInEditor )
END_EVENT_TABLE()

WxMainToolBar::WxMainToolBar( wxWindow* InParent, wxWindowID InID )
	: WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxNO_BORDER | wxTB_FLAT | wxCLIP_CHILDREN | wxTB_NO_TOOLTIPS )
{
	// Menus

	PasteSpecialMenu.Append( IDM_PASTE_ORIGINAL_LOCATION, *LocalizeUnrealEd("OriginalLocation") );
	PasteSpecialMenu.Append( IDM_PASTE_WORLD_ORIGIN, *LocalizeUnrealEd("WorldOrigin") );

	// Bitmaps

	NewB.Load( TEXT("New") );
	OpenB.Load( TEXT("Open") );
	SaveB.Load( TEXT("Save") );
	SaveAllLevelsB.Load( TEXT("SaveAllLevels.png") );
	SaveAllWritableB.Load( TEXT("SaveAllWritable.png") );
	UndoB.Load( TEXT("Undo") );
	RedoB.Load( TEXT("Redo") );
	CutB.Load( TEXT("Cut") );
	CopyB.Load( TEXT("Copy") );
	PasteB.Load( TEXT("Paste") );
	SearchB.Load( TEXT("Search") );
	FullScreenB.Load( TEXT("FullScreen") );
	ContentBrowserB.Load( TEXT( "MainToolBar_ContentBrowser" ) );
	GenericB.Load( TEXT("ThumbnailView") );
	KismetB.Load( TEXT("Kismet") );
	MouseLockB.Load( TEXT("MouseLock") );
	ShowWidgetB.Load( TEXT("ShowWidget") );
	TranslateB.Load( TEXT("Translate") );
	RotateB.Load( TEXT("Rotate") );
	ScaleB.Load( TEXT("Scale") );
	ScaleNonUniformB.Load( TEXT("ScaleNonUniform") );
	BrushPolysB.Load( TEXT("BrushPolys") );
	PrefabLockB.Load( TEXT("LVT_Perspective") );
	DistributionToggleB.Load( TEXT("DistributionToggle") );
	PSysRealtimeLODToggleB.Load( TEXT("PSys_RealtimeLOD") );
	PSysHelperToggleB.Load( TEXT("PSys_Helper") );
	CamSlowB.Load( TEXT("CamSlow") );
	CamNormalB.Load( TEXT("CamNormal") );
	CamFastB.Load( TEXT("CamFast") );
	ViewPushStartB.Load(TEXT("PushView_Start"));
	ViewPushStopB.Load(TEXT("PushView_Stop"));
	ViewPushSyncB.Load(TEXT("PushView_Sync"));
	MatineeListB.Load( TEXT( "MainToolBar_UnrealMatinee" ) );
	SentinelB.Load( TEXT("MainToolBar_Sentinel") );
	GameStatsVisualizerB.Load( TEXT("MainToolBar_GameStatsVisualizer") );
	LODLockingB.Load( TEXT("LODLocking") );
	QuickProcBuildingB.Load( TEXT("QuickProcBuilding") );


	BuildGeomB.Load(TEXT("BuildGeometry.png"));
	BuildLightingB.Load( TEXT("BuildLighting.png"));
	BuildPathsB.Load( TEXT("BuildPaths.png"));
	BuildCoverNodesB.Load( TEXT("BuildCoverNodes.png"));
	BuildAllB.Load( TEXT("BuildAll.png"));
	BuildAllSubmitB.Load( TEXT("BuildAllSubmit.png"));
	BuildAllSubmitDisabledB.Load(TEXT("BuildAllSubmitDisabled.png"));

	LightingQualityImages[Quality_Preview].Load(TEXT("LightingQuality_Preview.png"));
	LightingQualityImages[Quality_Medium].Load(TEXT("LightingQuality_Medium.png"));
	LightingQualityImages[Quality_High].Load(TEXT("LightingQuality_High.png"));
	LightingQualityImages[Quality_Production].Load(TEXT("LightingQuality_Production.png"));

	SocketsB.Load(TEXT("ASV_SocketMgr"));

	// default PC play icon
	PlayOnB[B_PC].Load( TEXT("PlayPC.png"));
	// PC mobile previewer play icon
	PlayOnB[B_PCMobilePreview].Load( TEXT("PlayPCMobilePreview.png"));
	// PC mobile previewer settings icon
	PlayOnB[B_PCMobilePreviewSettings].Load( TEXT("PlayPCMobilePreviewSettings.png"));

	// Playstation play icon
	PlayOnB[B_PS3].Load( TEXT("PlayPS.png"));
	// Xbox play icon
	PlayOnB[B_XBox360].Load( TEXT("PlayXB.png"));
	// NGP play icon
	PlayOnB[B_NGP].Load( TEXT("PlayOnNGP.png"));
	// iOS play icon
	PlayOnB[B_iOS].Load( TEXT("PlayOnIOS.png"));
	
	SelectTranslucentB.Load( TEXT( "MainToolBar_SelectTranslucent.png" ));
	PIEVisibleOnlyB.Load( TEXT( "MainToolBar_PIEVisibleOnly.png" ));
	OrthoStrictBoxSelectionB.Load(TEXT( "MainToolBar_OrthoStrictBoxSelection.png" ));
	OrthoIntersectBoxSelectionB.Load(TEXT("MainToolBar_OrthoIntersectBoxSelection.png"));

	RealtimeAudioB.Load( TEXT("RealTimeAudio.png") );
	FavoritesB.Load( TEXT("FavoriteList_Enabled") );
	DisabledFavoritesB.Load( TEXT("FavoriteList_Disabled") );

	EmulateMobileFeaturesB.Load( TEXT( "MainToolBar_EmulateMobileFeatures.png" ) );

	PlayInEditorStartB.Load( TEXT("PlayInEditorPlay.png") );
	PlayInEditorStopB.Load( TEXT("PlayInEditorStop.png") );

	RealtimeKismetDebuggingB.Load( TEXT("Realtime_Kismet_Debugging.png") );

	MatQualityLowB.Load( TEXT("MatQualityLow.png") );

	VolumeSlider = NULL;

	// Create special windows

	MRUButton.Create( this, IDPB_MRU_DROPDOWN, &GApp->EditorFrame->GetDownArrowB(), GApp->EditorFrame->GetCombinedMRUFavoritesMenu(), wxPoint(0,0), wxSize(-1,21) );
	//MRUButton.SetToolTip( *LocalizeUnrealEd("ToolTip_20") );

	//PasteSpecialButton.Create( this, IDPB_PASTE_SPECIAL, &GApp->EditorFrame->DownArrowB, &PasteSpecialMenu, wxPoint(0,0), wxSize(-1,21) );
	//PasteSpecialButton.SetToolTip( *LocalizeUnrealEd("ToolTip_21") );

	// Far culling plane slider
	wxSlider* FarPlaneSlider = NULL;
	{
		const int SliderMin = 0;
		const int SliderMax = 100;
		FarPlaneSlider = new wxSlider( this, ID_FarPlaneSlider, SliderMax, SliderMin, SliderMax, wxDefaultPosition, wxSize( 80, -1 ) );
		FarPlaneSlider->SetToolTip( *LocalizeUnrealEd("ToolTip_FarPlaneSlider") );
	}

	// Coordinate systems
	CoordSystemCombo = new WxComboBox( this, IDCB_COORD_SYSTEM, TEXT(""), wxDefaultPosition, wxSize( 60, -1 ), 0, NULL, wxCB_READONLY );
	CoordSystemCombo->Append( *LocalizeUnrealEd("World") );
	CoordSystemCombo->Append( *LocalizeUnrealEd("Local") );
	CoordSystemCombo->SetSelection( GEditorModeTools().CoordSystem );
	CoordSystemCombo->SetToolTip( *LocalizeUnrealEd("ToolTip_22") );

	// Set up the ToolBar
	//AddSeparator();
	AddTool( IDM_NEWMAP, TEXT(""), NewB, *LocalizeUnrealEd("ToolTip_23") );
	AddTool( IDM_OPEN, TEXT(""), OpenB, *LocalizeUnrealEd("ToolTip_24") );
	AddControl( &MRUButton );
	AddCheckTool( IDM_FAVORITES_TOGGLE_BUTTON, TEXT(""), FavoritesB, DisabledFavoritesB, *LocalizeUnrealEd("MainToolBar_FavoritesButton_ToolTip") );
	AddTool( IDM_SAVE, TEXT(""), SaveB, *LocalizeUnrealEd("SaveCurrentLevel") );
	AddTool( IDM_SAVE_ALL_LEVELS, TEXT(""), SaveAllLevelsB, *LocalizeUnrealEd("ToolTip_SaveAllLevels") );
	AddTool( IDM_SAVE_ALL_WRITABLE, TEXT(""), SaveAllWritableB, *LocalizeUnrealEd("ToolTip_SaveAllWritable") );
	AddSeparator();

	// Don't bother adding tool bar icons for uncommonly-clicked actions
	// @todo: Consider making this optional?
	const UBOOL bWantAllButtons = FALSE;
	if( bWantAllButtons )
	{
		AddTool( IDM_CUT, TEXT(""), CutB, *LocalizeUnrealEd("ToolTip_38") );
		AddTool( IDM_COPY, TEXT(""), CopyB, *LocalizeUnrealEd("ToolTip_39") );
		AddTool( IDM_PASTE, TEXT(""), PasteB, *LocalizeUnrealEd("ToolTip_40") );
		AddSeparator();
	}

	// NOTE: It's not obvious that Undo/Redo is useful on the tool bar as everyone uses Ctrl+Z/Ctrl+Y
	//		 for this, but often users want to know whether or not they *can* redo based on the enabled
	//		 state of the button, so we'll keep this buttons here.
	AddTool( IDM_UNDO, TEXT(""), UndoB, *LocalizeUnrealEd("ToolTip_26") );
	AddTool( IDM_REDO, TEXT(""), RedoB, *LocalizeUnrealEd("ToolTip_27") );
	AddSeparator();

	AddCheckTool( ID_EDIT_MOUSE_LOCK, TEXT(""), MouseLockB, MouseLockB, *LocalizeUnrealEd("ToolTip_MouseLock") );
	AddCheckTool( ID_EDIT_TRANSLATE, TEXT(""), TranslateB, TranslateB, *LocalizeUnrealEd("ToolTip_32") );
	AddCheckTool( ID_EDIT_ROTATE, TEXT(""), RotateB, RotateB, *LocalizeUnrealEd("ToolTip_33") );
	AddCheckTool( ID_EDIT_SCALE, TEXT(""), ScaleB, ScaleB, *LocalizeUnrealEd("ToolTip_34") );
	AddCheckTool( ID_EDIT_SCALE_NONUNIFORM, TEXT(""), ScaleNonUniformB, ScaleNonUniformB, *LocalizeUnrealEd("ToolTip_35") );
	if( bWantAllButtons )
	{
		AddCheckTool( ID_EDIT_SHOW_WIDGET, TEXT(""), ShowWidgetB, ShowWidgetB, *LocalizeUnrealEd("ToolTip_31") );
	}
	AddSeparator();
	AddControl( CoordSystemCombo );
	AddSeparator();
	AddTool( IDM_SEARCH, TEXT(""), SearchB, *LocalizeUnrealEd("ToolTip_36") );

	AddSeparator();
	UBOOL bHasContentBrowser = FALSE;
#if WITH_MANAGED_CODE
	if( FContentBrowser::IsInitialized() )
	{
		bHasContentBrowser = TRUE;

		// Content Browser
		AddTool( IDM_BROWSER_START, TEXT(""), ContentBrowserB, *LocalizeUnrealEd("MainToolBar_ContentBrowser") );
	}
#endif

	AddSeparator();

	AddTool( IDM_OPEN_KISMET, TEXT(""), KismetB, *LocalizeUnrealEd("ToolTip_42") );

	// Drop-down menu that allows the user to open a Matinee sequence from a list of all of the Matinees in the level
	{
		MatineeListButton.Create( this, IDM_MainToolBar_MatineeList, &MatineeListB );
		MatineeListButton.SetToolTip( *LocalizeUnrealEd( "MainToolBar_MatineeListDropDown" ) );
		AddControl( &MatineeListButton );
	}

#if !UDK
	// If a database is set, add 'Sentinel' button
	FString DummySource;
	if(GConfig->GetString( TEXT("SentinelStats"), TEXT("SentinelStatsSource"), DummySource, GEditorIni))
	{
		AddSeparator();
		AddCheckTool( IDM_OPEN_SENTINEL, TEXT(""), SentinelB, SentinelB, *LocalizeUnrealEd("OpenSentinel") );
	}
#endif	//#if !UDK

	UBOOL bShouldInitGameStatsVisualizer = ParseParam( appCmdLine(), TEXT("GameStats") );
#if !UDK
	if (!bShouldInitGameStatsVisualizer)
	{
		// See if the game has made it a default option in their INI file
		GConfig->GetBool( TEXT("GameplayStatsCollection"), TEXT("EnableVisualizers"), bShouldInitGameStatsVisualizer, GEditorIni );
	}
#endif
	if (bShouldInitGameStatsVisualizer)
	{
		AddSeparator();
		WxGameStatsVisualizer* GameStatsVisualizer = GUnrealEd->GetBrowser<WxGameStatsVisualizer>(TEXT("GameStatsVisualizer"));
		if ( GameStatsVisualizer != NULL )
		{
			AddTool( IDM_BROWSER_START+GameStatsVisualizer->GetDockID(), TEXT(""), GameStatsVisualizerB, *LocalizeUnrealEd("GameStatsVisualizer_MainToolBar") );
		}
	}

	AddSeparator();
	AddControl( FarPlaneSlider );
	AddSeparator();

	AddCheckTool( IDM_MainToolBar_SelectTranslucent, TEXT(""), SelectTranslucentB, SelectTranslucentB, *LocalizeUnrealEd( "MainToolBar_SelectTranslucent_ToolTip" ));
#if !UDK
	AddCheckTool( IDM_MainToolBar_PIEVisibleOnly, TEXT(""), PIEVisibleOnlyB, PIEVisibleOnlyB, *LocalizeUnrealEd( "MainToolBar_PIEVisibleOnly_ToolTip" ));
#endif
	
	//Box Selection Mode
	//AddCheckTool( IDM_MainMenu_UseStrictBoxSelection, TEXT(""), OrthoIntersectBoxSelectionB, OrthoStrictBoxSelectionB, *LocalizeUnrealEd( "MainMenu_UseStrictBoxSelection" ), *LocalizeUnrealEd( "MainMenu_UseStrictBoxSelection_Desc") );
	SelectionModeButton = new WxBitmapCheckButton(this, this, IDM_MainMenu_UseStrictBoxSelection, &OrthoIntersectBoxSelectionB, &OrthoStrictBoxSelectionB, wxDefaultPosition, wxSize(21,21));
	SelectionModeButton->SetToolTip(*LocalizeUnrealEd("MainMenu_UseStrictBoxSelection_Desc"));
	AddControl( SelectionModeButton );

#if !UDK
	AddCheckTool( IDM_BRUSHPOLYS, TEXT(""), BrushPolysB, BrushPolysB, *LocalizeUnrealEd("ToolTip_43") );
	AddCheckTool( IDM_TogglePrefabsLocked, TEXT(""), PrefabLockB, PrefabLockB, *LocalizeUnrealEd("TogglePrefabLock") );
#endif

	if( bWantAllButtons )
	{
		AddCheckTool( IDM_DISTRIBUTION_TOGGLE, TEXT(""), DistributionToggleB, DistributionToggleB, *LocalizeUnrealEd("ToolTip_DistributionToggle") );
	}
	AddCheckTool( IDM_MaterialQualityToggle, TEXT(""), MatQualityLowB, MatQualityLowB, *LocalizeUnrealEd("ToolTip_MatQualityToggle") );

#if !UDK
	AddCheckTool( ID_ToggleSocketSnapping, TEXT(""), SocketsB, SocketsB, *LocalizeUnrealEd("ToolTip_ToggleSocketSnapping") );
	AddCheckTool( ID_ShowSocketNames, TEXT(""), SocketsB, SocketsB, *LocalizeUnrealEd("ToolTip_ShowSocketNames") );
	AddCheckTool( IDM_PSYSLODREALTIME_TOGGLE, TEXT(""), PSysRealtimeLODToggleB, PSysRealtimeLODToggleB, *LocalizeUnrealEd("ToolTip_PSysLODRealtimeToggle") );
	AddCheckTool( IDM_PSYSHELPER_TOGGLE, TEXT(""), PSysHelperToggleB, PSysHelperToggleB, *LocalizeUnrealEd("ToolTip_PSysHelperToggle") );
#endif

	if( bWantAllButtons )
	{
		AddCheckTool( IDM_ToggleLODLocking, TEXT(""), LODLockingB, LODLockingB, *LocalizeUnrealEd("ToolTip_ToggleLODLocking") );
	}
	AddSeparator();

	AddTool( IDM_BUILD_VISIBLEGEOMETRY, TEXT(""), BuildGeomB, *LocalizeUnrealEd("ToolTip_44") );
	AddTool( IDM_BUILD_LIGHTING, TEXT(""), BuildLightingB, *LocalizeUnrealEd("ToolTip_45") );
	AddTool( IDM_BUILD_AI_PATHS, TEXT(""), BuildPathsB, *LocalizeUnrealEd("ToolTip_46") );
	AddTool( IDM_BUILD_COVER, TEXT(""), BuildCoverNodesB, *LocalizeUnrealEd("ToolTip_47") );
	AddTool( IDM_BUILD_ALL, TEXT(""), BuildAllB, *LocalizeUnrealEd("ToolTip_48") );

#if HAVE_SCC && WITH_MANAGED_CODE
	AddTool( IDM_BUILD_ALL_SUBMIT, TEXT(""), BuildAllSubmitB, *LocalizeUnrealEd("BuildAllAndSubmit_ToolTip") );
	SetToolDisabledBitmap( IDM_BUILD_ALL_SUBMIT, BuildAllSubmitDisabledB );
#endif // #if HAVE_SCC && WITH_MANAGED_CODE

	INT CheckQualityLevel;
	GConfig->GetInt(TEXT("LightingBuildOptions"), TEXT("QualityLevel"), CheckQualityLevel, GEditorUserSettingsIni);
	check(CheckQualityLevel < Quality_MAX);
	AddTool( IDM_LIGHTING_QUALITY, TEXT(""), (LightingQualityImages[CheckQualityLevel]), *LocalizeUnrealEd("ToolTip_LightingQuality") );


	// @todo: We've disabled object propagation in the editor UI for now
	ViewPushStartStopButton = NULL;
	// if this changed other code can be adjusted
	// search code for label !RemoteCapture
	if(GIsEpicInternal)
	{
		// add a combo box for where to send object propagation messages to
		wxComboBox* PropagationCombo = new WxComboBox( this, IDCB_ObjectPropagation, TEXT(""), wxDefaultPosition, wxSize( 100, -1 ), 0, NULL, wxCB_READONLY );
		// Standard selection items
		PropagationCombo->Append(*LocalizeUnrealEd("NoPropagation")); // no propagation
		PropagationCombo->Append(*LocalizeUnrealEd("LocalStandalone")); // propagate to 127.0.0.1 (localhost)
		// allow for propagating to all loaded consoles
		for (FConsoleSupportIterator It; It; ++It)
		{
			PropagationCombo->Append(It->GetPlatformName());
		}
	//	PropagationCombo->SetSelection( GEditorModeTools().CoordSystem );
		PropagationCombo->SetToolTip(*LocalizeUnrealEd("ToolTip_49"));
		AddControl(PropagationCombo);

		// start out with no propagation
		PropagationCombo->SetSelection(OPD_None);

		GEditor->SetObjectPropagationDestination(OPD_None);

		ViewPushStartStopButton = AddCheckTool( IDM_PUSHVIEW_StartStop, TEXT(""), ViewPushStartB, ViewPushStartB, *LocalizeUnrealEd("ToolTip_PushViewStart") );

		AddSeparator();
	}

	AddCheckTool( IDM_FULLSCREEN, TEXT(""), FullScreenB, FullScreenB, *LocalizeUnrealEd("ToolTip_37") );
	AddCheckTool( IDM_REALTIME_AUDIO, TEXT(""), RealtimeAudioB, RealtimeAudioB, *LocalizeUnrealEd("MainToolBar_RealTimeAudio") );
	AddSeparator();

	// Emulate Mobile Features toggle
	AddCheckTool( IDM_EmulateMobileFeatures, TEXT(""), EmulateMobileFeaturesB, EmulateMobileFeaturesB, *LocalizeUnrealEd("MainMenu_EmulateMobileFeatures") );
	AddSeparator();


	// loop through all consoles (only support 20 consoles)
	INT ConsoleIndex = 0;
	UBOOL bHasAddedPreviewButton = FALSE;

	WxBitmap* PlayConsoleB = NULL;
	for (FConsoleSupportIterator It; It && ConsoleIndex < 20; ++It, ConsoleIndex++)
	{
		const wchar_t* ConsoleName = It->GetPlatformName();
		UBOOL bIsIPhone = appStricmp( ConsoleName, CONSOLESUPPORT_NAME_IPHONE ) == 0;
		UBOOL bIsNGP = appStricmp( ConsoleName, CONSOLESUPPORT_NAME_NGP ) == 0;
		UBOOL bIsAndroid = appStricmp( ConsoleName, CONSOLESUPPORT_NAME_ANDROID ) == 0;
		UBOOL bIsPS3 = appStricmp( ConsoleName, CONSOLESUPPORT_NAME_PS3 ) == 0;
		UBOOL bIs360 = appStricmp( ConsoleName, CONSOLESUPPORT_NAME_360 ) == 0;
		UBOOL bIsMac = appStricmp( ConsoleName, CONSOLESUPPORT_NAME_MAC ) == 0;

#if WITH_ES2_RHI
		if( !bHasAddedPreviewButton && ( bIsIPhone || bIsNGP || bIsAndroid ) )
		{
			bHasAddedPreviewButton = TRUE;

			// Add a button for the mobile previewer settings dialog
			PlayConsoleB = &PlayOnB[B_PCMobilePreviewSettings];
			AddTool(
				IDM_PlayUsingMobilePreview_Settings, 
				TEXT(""),
				*PlayConsoleB,
				*FString::Printf(*LocalizeUnrealEd("ToolTip_PlayOnMobilePreviewerSettings"))
				);

			PlayConsoleB = &PlayOnB[B_PCMobilePreview];
			AddTool(
				IDM_PlayUsingMobilePreview, 
				TEXT(""),
				*PlayConsoleB,
				*FString::Printf(LocalizeSecure(LocalizeUnrealEd("ToolTip_50_FP"), *LocalizeUnrealEd(TEXT("MobilePreview"))))
				);

			AddSeparator();
		}
#endif

		// No "Play On" support for Android or Mac platforms (yet!)
		if( !bIsAndroid && !bIsMac )
		{
			// select console icon
			FString TooltipStr = *FString::Printf(LocalizeSecure(LocalizeUnrealEd("ToolTip_50_F"), It->GetPlatformName()));
			PlayConsoleB = &PlayOnB[B_PC];
			if( bIsPS3 )
			{
				PlayConsoleB = &PlayOnB[B_PS3];
			}
			else if( bIs360 )
			{
				PlayConsoleB = &PlayOnB[B_XBox360];
			}
			else if( bIsNGP )
			{
				PlayConsoleB = &PlayOnB[B_NGP];
			}
			else if( bIsIPhone )
			{
				PlayConsoleB = &PlayOnB[B_iOS];
				
				TooltipStr = *FString::Printf( *LocalizeUnrealEd("ToolTip_InstallOnIOSDevice_ToolBar") );
			}

			INT MobilePreviewerPos = GetToolPos(IDM_BuildPlayConsole_START);
			if( bIsIPhone && MobilePreviewerPos != wxNOT_FOUND)
			{
				// Insert the tool after the mobile previewer (i.e. the first console) so we keep all the mobile "Play on" buttons together.
				InsertTool( 
					MobilePreviewerPos,
					IDM_BuildPlayConsole_START + ConsoleIndex, 
					*PlayConsoleB,
					wxNullBitmap,
					FALSE,
					NULL,
					*TooltipStr 
					);
			}
			else
			{
				// add a per-console Play On XXX button
				AddTool(
					IDM_BuildPlayConsole_START + ConsoleIndex, 
					TEXT(""),
					*PlayConsoleB,
					*TooltipStr
					);
			}
			AddSeparator();
		}	
	}
	
	AddCheckTool( IDM_KISMET_REALTIMEDEBUGGING, *LocalizeUnrealEd("ToggleRealtimeDebugging"), RealtimeKismetDebuggingB, wxNullBitmap, *LocalizeUnrealEd("ToggleRealtimeDebugging") );

	// Play in editor
	PlayInEditorButton = new WxBitmapStateButton( this, this, IDM_BuildPlayInEditorButton, wxDefaultPosition, wxSize( 35, 21 ), FALSE );
	PlayInEditorButton->AddState( PIV_Play, &PlayInEditorStartB );
	PlayInEditorButton->AddState( PIV_Stop, &PlayInEditorStopB );
	PlayInEditorButton->SetToolTip( *LocalizeUnrealEd("MainToolBar_PlayInEditorPlay_ToolTip") );
	PlayInEditorButton->SetCurrentState( PIV_Play );
	AddControl( PlayInEditorButton );

	Realize();

	// The ViewPush button defaults to disabled because the object propagation destination is 'none'.
	// This must be called AFTER Realize()!
	EnablePushView( FALSE );

	//default lighting quality to invalid state
	LastLightingQualitySetting = Quality_MAX;
}

/** Updates the 'Push View' toggle's bitmap and hint text based on the input state. */
void WxMainToolBar::SetPushViewState(UBOOL bIsPushingView)
{
	if ( ViewPushStartStopButton )
	{
		ViewPushStartStopButton->SetNormalBitmap( bIsPushingView ? ViewPushStopB : ViewPushStartB );
		ViewPushStartStopButton->SetDisabledBitmap( bIsPushingView ? ViewPushStopB : ViewPushStartB );
		ViewPushStartStopButton->SetShortHelp( bIsPushingView ? *LocalizeUnrealEd("ToolTip_PushViewStop") : *LocalizeUnrealEd("ToolTip_PushViewStart") );
	}
}

/** Enables/disables the 'Push View' button. */
void WxMainToolBar::EnablePushView(UBOOL bEnabled)
{
	if ( ViewPushStartStopButton )
	{
		EnableTool( IDM_PUSHVIEW_StartStop, bEnabled ? true : false );
	}
}

/** Updates the LightingQuality button */
void WxMainToolBar::UpdateLightingQualityState()
{
	INT CheckQualityLevel;
	GConfig->GetInt(TEXT("LightingBuildOptions"), TEXT("QualityLevel"), CheckQualityLevel, GEditorUserSettingsIni);
	check(CheckQualityLevel < Quality_MAX);
	//don't call this if already in the right state, it causes flicker
	if (LastLightingQualitySetting != CheckQualityLevel)
	{
		LastLightingQualitySetting = CheckQualityLevel;
		SetToolNormalBitmap(IDM_LIGHTING_QUALITY, (LightingQualityImages[CheckQualityLevel]));
	}
}

/** Called when the trans/rot/scale widget toolbar buttons are right-clicked. */
void WxMainToolBar::OnTransformButtonRightClick(wxCommandEvent& In)
{
	WxDlgTransform::ETransformMode NewMode = WxDlgTransform::TM_Translate;
	switch( In.GetId() )
	{
		case ID_EDIT_ROTATE:
			NewMode = WxDlgTransform::TM_Rotate;
			break;
		case ID_EDIT_SCALE:
		case ID_EDIT_SCALE_NONUNIFORM:
			NewMode = WxDlgTransform::TM_Scale;
			break;
	}

	WxDlgTransform* DlgTransform = GApp->GetDlgTransform();
	check(DlgTransform);

	const bool bIsShown = DlgTransform->IsShown();
	const UBOOL bSameTransformMode = DlgTransform->GetTransformMode() == NewMode;
	if ( bIsShown && bSameTransformMode )
	{
		DlgTransform->Show( false );
	}
	else
	{
		DlgTransform->SetTransformMode( NewMode );
		DlgTransform->Show( true );
	}
}

/** Called when the LightingQuality toolbar button is right-clicked. */
void WxMainToolBar::OnLightingQualityButtonRightClick(wxCommandEvent& In)
{
	WxLightingQualityToolBarButtonRightClick* menu = new WxLightingQualityToolBarButtonRightClick(this);
	if (menu)
	{
		FTrackPopupMenu tpm(this, menu);
		tpm.Show();
		delete menu;
	}
}

void WxMainToolBar::OnBuildAIPathsButtonRightClick(wxCommandEvent& In)
{
	FEditorBuildUtils::EditorBuild( IDM_BUILD_SELECTED_AI_PATHS );
}

/** Called when the play in editor toolbar button is right-clicked */
void WxMainToolBar::OnPlayInEditorRightClick( wxCommandEvent& In )
{
	// Bring up the edit url dialog
	GUnrealEd->EditPlayWorldURL();
}

/** Called when the play on console button is right-clicked */
void WxMainToolBar::OnPlayOnConsoleRightClick( wxCommandEvent& In )
{
	// Bring up the edit url dialog.  Pass in ID of the button that was clicked which corresponds to a console ID
	GUnrealEd->EditPlayWorldURL( In.GetId() - IDM_BuildPlayConsole_START );
}

/** Called when the play using mobile previewer button is right-clicked */
void WxMainToolBar::OnPlayUsingMobilePreviewRightClick( wxCommandEvent& In )
{
	// Figure out which console support contain represents the PC platform
	INT PCSupportContainerIndex = INDEX_NONE;
	const INT ConsoleCount = FConsoleSupportContainer::GetConsoleSupportContainer()->GetNumConsoleSupports();
	for( INT CurConsoleIndex = 0; CurConsoleIndex < ConsoleCount; ++CurConsoleIndex )
	{
		FConsoleSupport* Console = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport( CurConsoleIndex );
		if( Console != NULL && appStricmp( Console->GetPlatformName(), CONSOLESUPPORT_NAME_PC ) == 0 )
		{
			PCSupportContainerIndex = CurConsoleIndex;
			break;
		}
	}

	// This will only work if we have a PC console support container (requires WindowsTools.dll)
	if( PCSupportContainerIndex != INDEX_NONE )
	{
		// Bring up the edit url dialog.  Pass in ID of the button that was clicked which corresponds to a console ID
		const UBOOL bUseMobilePreview = TRUE;
		GUnrealEd->EditPlayWorldURL( PCSupportContainerIndex, bUseMobilePreview );
	}
}

/** If true, Kismet visual debugging is enabled */
UBOOL GKismetRealtimeDebugging = FALSE;

/** Called when the toggle Kismet realtime debugging button is clicked. */
void WxMainToolBar::OnRealtimeKismetDebugging( wxCommandEvent &In )
{
	GKismetRealtimeDebugging = !GKismetRealtimeDebugging;
	ToggleTool(IDM_KISMET_REALTIMEDEBUGGING, GKismetRealtimeDebugging == TRUE);
}

/** Called when the LightingQuality toolbar button is left-clicked. */
void WxMainToolBar::OnLightingQualityButtonLeftClick(wxCommandEvent& In)
{
	INT CheckQualityLevel;
	GConfig->GetInt(TEXT("LightingBuildOptions"), TEXT("QualityLevel"), CheckQualityLevel, GEditorUserSettingsIni);
	CheckQualityLevel++;
	if (CheckQualityLevel >= Quality_NoGlobalIllumination)
	{
		CheckQualityLevel = 0;
	}
	GConfig->SetInt(TEXT("LightingBuildOptions"), TEXT("QualityLevel"), CheckQualityLevel, GEditorUserSettingsIni);
	UpdateLightingQualityState();
}


/** Called when the real time audio button is right-clicked. */
void WxMainToolBar::OnRealTimeAudioRightClick( wxCommandEvent& In )
{
	wxPoint MousePos = wxGetMousePosition();

	if( !VolumeSlider )
	{
		// Volume range for the slider
		const INT MinVolume = 0;
		const INT MaxVolume = 100;
		// Get the existing volume level from the user settings. Convert to an integer for compatibility with the slider.
		INT Volume = appTrunc(GEditor->AccessUserSettings().EditorVolumeLevel * MaxVolume);
		// Account for the possibility that a user edits the ini volume setting with an out of range value
		Volume = Clamp<INT>(Volume, MinVolume, MaxVolume);
		// Create and show the dialog.  Modeless, will be destroyed automatically.
		VolumeSlider = new WxPopupSlider( this, MousePos, ID_VOLUME_SLIDER, Volume, MinVolume, MaxVolume, wxSL_VERTICAL | wxSL_INVERSE );
		
	}
	
	// Move the slider over the current mouse position as the window position and size may have changed so just showing the slider could leave it in some weird place
	VolumeSlider->SetSize( MousePos.x, MousePos.y, -1, -1, wxSIZE_USE_EXISTING );
	// Make the slider the focused window to bring it to the foreground.
	VolumeSlider->SetFocus();
	// Show the window
	VolumeSlider->Show();
}

/** Called to update the UI for the toggle favorites button */
void WxMainToolBar::UI_ToggleFavorites( wxUpdateUIEvent& In )
{
	FString FileName;
	const UBOOL bMapFileExists = GPackageFileCache->FindPackageFile( *( GWorld->GetOutermost()->GetName() ), NULL, FileName );
	
	// Disable the favorites button if the map isn't associated to a file yet (new map, never before saved, etc.)
	In.Enable( bMapFileExists == TRUE );
	
	// If the map exists, determine its state based on whether the map is already favorited or not
	if ( bMapFileExists )
	{
		const UBOOL bAlreadyFavorited = GApp->EditorFrame->GetMRUFavoritesList()->ContainsFavoritesItem( FileName );
		In.Check( bAlreadyFavorited == TRUE );
	}
}

/** Called whenever the right mouse button is released on a toolbar WxBitmapButton */
void WxMainToolBar::OnRightButtonUpOnControl( wxMouseEvent& In )
{
	if ( In.GetId() == IDM_BuildPlayInEditorButton && PlayInEditorButton->GetCurrentState()->ID == PIV_Play )
	{
		// Bring up the edit url dialog
		GUnrealEd->EditPlayWorldURL();
	}
}

/** Called whenever the play in editor button is clicked */
void WxMainToolBar::OnPlayInEditorButtonClick( wxCommandEvent& In )
{
	// If there's no PIE session active, kick one off
	if ( PlayInEditorButton->GetCurrentState()->ID == PIV_Play )
	{
		GUnrealEd->PlayMap();
	}
	// Stop the current PIE session
	else if ( GEditor->PlayWorld != NULL )
	{
		GUnrealEd->EndPlayMap();
	}
}

/** Called to update the UI for the play in editor button */
void WxMainToolBar::UI_PlayInEditor( wxUpdateUIEvent& In )
{
	const INT CurState = ( GEditor->PlayWorld != NULL ) ? PIV_Stop : PIV_Play;
	if ( PlayInEditorButton->GetCurrentState()->ID != CurState )
	{
		PlayInEditorButton->SetCurrentState( CurState );
		FString NewToolTip = ( CurState == PIV_Play ) ? LocalizeUnrealEd("MainToolBar_PlayInEditorPlay_ToolTip") : LocalizeUnrealEd("MainToolBar_PlayInEditorStop_ToolTip");
		PlayInEditorButton->SetToolTip( *NewToolTip );
	}
}


/*-----------------------------------------------------------------------------
WxPopupSlider
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxPopupSlider, wxFrame )
END_EVENT_TABLE()

WxPopupSlider::WxPopupSlider( wxWindow* Parent, const wxPoint& Pos, UINT SliderID, INT StartValue, INT MinValue, INT MaxValue, LONG SliderStyle)
: wxFrame( Parent, wxID_ANY, TEXT(""), Pos, wxDefaultSize, wxFRAME_TOOL_WINDOW ), Slider(NULL)
{
	wxBoxSizer* Sizer = new wxBoxSizer( wxHORIZONTAL );

	Slider = new wxSlider( this, SliderID, StartValue, MinValue, MaxValue, wxDefaultPosition, wxDefaultSize, SliderStyle);
	Sizer->Add( Slider );
	SetSizerAndFit( Sizer );
	Slider->Connect( wxEVT_LEAVE_WINDOW, wxMouseEventHandler( WxPopupSlider::OnMouseLeaveWindow ), NULL, this );
	Slider->Connect( wxEVT_KILL_FOCUS, wxFocusEventHandler( WxPopupSlider::OnKillFocus ), NULL, this );
}

WxPopupSlider::~WxPopupSlider()
{
	//nothing to do
}

/** Called when the mouse leaves the window */
void WxPopupSlider::OnMouseLeaveWindow( wxMouseEvent& In )
{
	// Hide the window
	Show( FALSE );
}

/** Called when the slider loses focus */
void WxPopupSlider::OnKillFocus( wxFocusEvent& In )
{
	// Hide the window
	Show( FALSE );
}

/*-----------------------------------------------------------------------------
WxMatineeMenuListToolBarButton
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxMatineeMenuListToolBarButton, WxBitmapButton )
EVT_COMMAND( IDM_MainToolBar_MatineeList, wxEVT_COMMAND_BUTTON_CLICKED, WxMatineeMenuListToolBarButton::OnClick )
END_EVENT_TABLE()

/** Called when the tool bar button is clicked */
void WxMatineeMenuListToolBarButton::OnClick( wxCommandEvent &In )
{
	// Open Matinee!  If there's only one Matinee available to open this will just go ahead and do it, otherwise
	// a menu object will be returned that we'll have to display
	wxMenu* MenuToDisplay = GApp->EditorFrame->OpenMatineeOrBuildMenu();
	if( MenuToDisplay != NULL )
	{
		// Display the menu directly below the button
		wxRect rc = GetRect();
		PopupMenu( MenuToDisplay, 0, rc.GetHeight() );
	}
}

/**
 *	WxLightingQualityToolBarButtonRightClick 
 */
WxLightingQualityToolBarButtonRightClick::WxLightingQualityToolBarButtonRightClick(WxMainToolBar* MainToolBar)
{
	wxMenuItem* TempItem;

	// This is to ensure that devs update this function when the number of lighting quality settings change - if you see this warning, please update the code below...
	checkAtCompileTime(Quality_MAX == 5, UpdateThisFunctionWhenAddingQualityLevels);

	INT CheckQualityLevel;
	GConfig->GetInt(TEXT("LightingBuildOptions"), TEXT("QualityLevel"), CheckQualityLevel, GEditorUserSettingsIni);

	wxEvtHandler* EvtHandler = GetEventHandler();

	TempItem = AppendCheckItem(Quality_Preview, *LocalizeUnrealEd(TEXT("LightQuality_Preview")));
	check(TempItem->GetId() == Quality_Preview);
	if (CheckQualityLevel == Quality_Preview)
	{
		Check(Quality_Preview, TRUE);
	}
	EvtHandler->Connect(Quality_Preview, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(WxLightingQualityToolBarButtonRightClick::OnLightingQualityButton));

	TempItem = AppendCheckItem(Quality_Medium, *LocalizeUnrealEd(TEXT("LightQuality_Medium")));
	check(TempItem->GetId() == Quality_Medium);
	if (CheckQualityLevel == Quality_Medium)
	{
		Check(Quality_Medium, TRUE);
	}
	EvtHandler->Connect(Quality_Medium, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(WxLightingQualityToolBarButtonRightClick::OnLightingQualityButton));

	TempItem = AppendCheckItem(Quality_High, *LocalizeUnrealEd(TEXT("LightQuality_High")));
	check(TempItem->GetId() == Quality_High);
	if (CheckQualityLevel == Quality_High)
	{
		Check(Quality_High, TRUE);
	}
	EvtHandler->Connect(Quality_High, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(WxLightingQualityToolBarButtonRightClick::OnLightingQualityButton));

	TempItem = AppendCheckItem(Quality_Production, *LocalizeUnrealEd(TEXT("LightQuality_Production")));
	check(TempItem->GetId() == Quality_Production);
	if (CheckQualityLevel == Quality_Production)
	{
		Check(Quality_Production, TRUE);
	}
	EvtHandler->Connect(Quality_Production, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(WxLightingQualityToolBarButtonRightClick::OnLightingQualityButton));
}

WxLightingQualityToolBarButtonRightClick::~WxLightingQualityToolBarButtonRightClick()
{
}

/** Called when the LightingQuality button is right-clicked and an entry is selected */
void WxLightingQualityToolBarButtonRightClick::OnLightingQualityButton( wxCommandEvent& In )
{
	GApp->EditorFrame->OnLightingQualityButton(In);
}
