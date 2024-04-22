/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgActorFactory.h"
#include "DlgActorSearch.h"
#include "StartupTipDialog.h"
#include "DlgLightingResults.h"
#include "DlgDensityRenderingOptions.h"
#if ENABLE_SIMPLYGON_MESH_PROXIES
#include "DlgCreateMeshProxy.h"
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
#if WITH_MANAGED_CODE
	#include "LightmapResRatioWindowShared.h"
	#include "LightingToolsWindowShared.h"
#endif	//#if WITH_MANAGED_CODE
#include "LevelViewportToolBar.h"
#include "MainToolBar.h"
#include "UnLinkedObjEditor.h"
#include "FileHelpers.h"
#include "Kismet.h"
#include "MaterialInstanceConstantEditor.h"
#include "MaterialInstanceTimeVaryingEditor.h"
#include "EngineMaterialClasses.h"
#include "LevelBrowser.h"
#include "UnTexAlignTools.h"
#include "ViewportsContainer.h"
#include "ScopedTransaction.h"
#include "LevelUtils.h"
#include "..\..\engine\inc\EngineSequenceClasses.h"
#include "..\..\engine\inc\EnginePhysicsClasses.h"
#include "..\..\engine\inc\EnginePrefabClasses.h"
#include "..\..\engine\inc\EngineAnimClasses.h"
#include "..\..\engine\inc\EngineSplineClasses.h"
#include "..\..\engine\inc\EngineSoundClasses.h"
#include "EngineDecalClasses.h"
#include "..\..\Launch\Resources\resource.h"
#include "SurfaceIterators.h"
#include "PropertyWindowManager.h"
#include "UnConsoleSupportContainer.h"
#include "EngineMeshClasses.h"
#include "UnEdTran.h"
#include "DlgGenericComboEntry.h"
#include "UnFracturedStaticMesh.h"
#include "StaticMeshEditor.h"
#include "EngineFogVolumeClasses.h"
#include "EngineFluidClasses.h"
#include "EngineSpeedTreeClasses.h"
#include "EngineProcBuildingClasses.h"
#include "EngineFoliageClasses.h"
#include "BSPOps.h"
#include "Sentinel.h"
#include "ContentBrowserHost.h"
#include "StartPageHost.h"
#include "GameStatsBrowser.h"
#include "Menus.h"
#include "AssetSelection.h"
#include "EngineAIClasses.h"
#include "GameFrameworkClasses.h"
#include "EditorLevelUtils.h"
#include "MRUFavoritesList.h"
#include "SourceControl.h"
#include "EditorBuildUtils.h"
#include "UnPath.h"

#if WITH_SIMPLYGON
#include "SimplygonMeshUtilities.h"
#endif // #if WITH_SIMPLYGON

#if _WINDOWS
// Needed for balloon message handling
#pragma pack(push,8)
#include <ShellAPI.h>
#pragma pack(pop)

extern UBOOL IsDirect3D11Supported(UBOOL& OutSupportsD3D11Features);
#endif

#if WITH_FACEFX_STUDIO
	#include "../../../External/FaceFX/Studio/Main/Inc/FxStudioApp.h"
#endif // WITH_FACEFX_STUDIO


#if WITH_MANAGED_CODE
	// CLR includes
	#include "ContentBrowserShared.h"
	#include "StartPageShared.h"
	#include "FileSystemNotificationShared.h"
	#include "BuildAndSubmitWindowShared.h"
	#include "WelcomeScreenShared.h"
	#include "AboutScreenShared.h"
	#include "UnitTestWindowShared.h"
#endif


#if !CONSOLE
	#include "tinyxml.h"
#endif


extern UBOOL GKismetRealtimeDebugging;

////////////////////////////////////////////////////////////////////////////////////////////

/**
* Menu for controlling options related to the rotation grid.
*/
class WxScaleGridMenu : public wxMenu
{
public:
	WxScaleGridMenu()
	{
		AppendCheckItem( IDM_DRAG_GRID_SNAPSCALE, *LocalizeUnrealEd("SnapScaling"), *LocalizeUnrealEd("ToolTip_SnapScaling") );
		AppendSeparator();

		FString ScaleGridSizeTooltip = LocalizeUnrealEd("MainMenu_AdjustScaleGridSize_ToolTip");
		AppendCheckItem( IDM_SCALE_GRID_001, *LocalizeUnrealEd("1Percent"), *ScaleGridSizeTooltip );
		AppendCheckItem( IDM_SCALE_GRID_002, *LocalizeUnrealEd("2Percent"), *ScaleGridSizeTooltip );
		AppendCheckItem( IDM_SCALE_GRID_005, *LocalizeUnrealEd("5Percent"), *ScaleGridSizeTooltip );
		AppendCheckItem( IDM_SCALE_GRID_010, *LocalizeUnrealEd("10Pct"), *ScaleGridSizeTooltip );
		AppendCheckItem( IDM_SCALE_GRID_025, *LocalizeUnrealEd("25Pct"), *ScaleGridSizeTooltip );
		AppendCheckItem( IDM_SCALE_GRID_050, *LocalizeUnrealEd("50Pct"), *ScaleGridSizeTooltip );
	}
};


/**
 * Menu for controlling options related to the rotation grid.
 */
class WxRotationGridMenu : public wxMenu
{
public:
	void BuildMenu( void )
	{
		switch( GUnrealEd->Constraints.AngleSnapType )
		{
			case EST_ANGLE:
			SetLabel( IDM_ROTATION_GRID_512, *LocalizeUnrealEd( "SNAP_Angle_512" ) );
			SetLabel( IDM_ROTATION_GRID_1024, *LocalizeUnrealEd( "SNAP_Angle_1024" ) );
			SetLabel( IDM_ROTATION_GRID_2048, *LocalizeUnrealEd( "SNAP_Angle_2048" ) );
			SetLabel( IDM_ROTATION_GRID_4096, *LocalizeUnrealEd( "SNAP_Angle_4096" ) );
			SetLabel( IDM_ROTATION_GRID_8192, *LocalizeUnrealEd( "SNAP_Angle_8192" ) );
			SetLabel( IDM_ROTATION_GRID_16384, *LocalizeUnrealEd( "SNAP_Angle_16384" ) );
			break;

			case EST_PER90:
			SetLabel( IDM_ROTATION_GRID_512, *LocalizeUnrealEd( "SNAP_Per90_512" ) );
			SetLabel( IDM_ROTATION_GRID_1024, *LocalizeUnrealEd( "SNAP_Per90_1024" ) );
			SetLabel( IDM_ROTATION_GRID_2048, *LocalizeUnrealEd( "SNAP_Per90_2048" ) );
			SetLabel( IDM_ROTATION_GRID_4096, *LocalizeUnrealEd( "SNAP_Per90_4096" ) );
			SetLabel( IDM_ROTATION_GRID_8192, *LocalizeUnrealEd( "SNAP_Per90_8192" ) );
			SetLabel( IDM_ROTATION_GRID_16384, *LocalizeUnrealEd( "SNAP_Per90_16384" ) );
			break;

			case EST_PER360:
			SetLabel( IDM_ROTATION_GRID_512, *LocalizeUnrealEd( "SNAP_Per360_512" ) );
			SetLabel( IDM_ROTATION_GRID_1024, *LocalizeUnrealEd( "SNAP_Per360_1024" ) );
			SetLabel( IDM_ROTATION_GRID_2048, *LocalizeUnrealEd( "SNAP_Per360_2048" ) );
			SetLabel( IDM_ROTATION_GRID_4096, *LocalizeUnrealEd( "SNAP_Per360_4096" ) );
			SetLabel( IDM_ROTATION_GRID_8192, *LocalizeUnrealEd( "SNAP_Per360_8192" ) );
			SetLabel( IDM_ROTATION_GRID_16384, *LocalizeUnrealEd( "SNAP_Per360_16384" ) );
			break;
		}
	}
	WxRotationGridMenu()
	{
		AppendCheckItem( IDM_ROTATION_GRID_TOGGLE, *LocalizeUnrealEd("UseRotationGrid"), *LocalizeUnrealEd("ToolTip_139") );
		AppendSeparator();

		FString SnapAngleTypeToolTip = LocalizeUnrealEd( "MainMenu_SnapAngleType_ToolTip" );
		AppendCheckItem( IDM_ANGLESNAPTYPE_ANGLE, *LocalizeUnrealEd( "SNAP_AngleSnapType_Angle" ), *SnapAngleTypeToolTip );
		AppendCheckItem( IDM_ANGLESNAPTYPE_PER90, *LocalizeUnrealEd( "SNAP_AngleSnapType_Per90" ), *SnapAngleTypeToolTip );
		AppendCheckItem( IDM_ANGLESNAPTYPE_PER360, *LocalizeUnrealEd( "SNAP_AngleSnapType_Per360" ), *SnapAngleTypeToolTip );
		AppendSeparator();

		FString RotationGridAngleToolTip = LocalizeUnrealEd( "MainMenu_AdjustRotationGridAngle_ToolTip" );
		AppendCheckItem( IDM_ROTATION_GRID_512, *LocalizeUnrealEd( "SNAP_Angle_512" ), *RotationGridAngleToolTip );
		AppendCheckItem( IDM_ROTATION_GRID_1024, *LocalizeUnrealEd( "SNAP_Angle_1024" ), *RotationGridAngleToolTip );
		AppendCheckItem( IDM_ROTATION_GRID_2048, *LocalizeUnrealEd( "SNAP_Angle_2048" ), *RotationGridAngleToolTip );
		AppendCheckItem( IDM_ROTATION_GRID_4096, *LocalizeUnrealEd( "SNAP_Angle_4096" ), *RotationGridAngleToolTip );
		AppendCheckItem( IDM_ROTATION_GRID_8192, *LocalizeUnrealEd( "SNAP_Angle_8192" ), *RotationGridAngleToolTip );
		AppendCheckItem( IDM_ROTATION_GRID_16384, *LocalizeUnrealEd( "SNAP_Angle_16384" ), *RotationGridAngleToolTip );
		BuildMenu();
	}
};



/**
* Menu for controlling options related to autosaving
*/
class WxAutoSaveOptionsMenu : public wxMenu
{
public:
	WxAutoSaveOptionsMenu()
	{
		// Construct the package types sub-menu that allows the user to specify which package types
		// will be auto-saved
		wxMenu* PackageTypeSubmenu = new wxMenu();
		PackageTypeSubmenu->AppendCheckItem( IDM_AUTOSAVE_MAPS, *LocalizeUnrealEd("AutoSaveMaps"), *LocalizeUnrealEd("MainMenu_AutosaveMaps_ToolTip") );
		PackageTypeSubmenu->AppendCheckItem( IDM_AUTOSAVE_CONTENT, *LocalizeUnrealEd("AutoSaveContent"), *LocalizeUnrealEd("MainMenu_AutosaveContent_ToolTip") );
		Append( wxID_ANY, *LocalizeUnrealEd("AutoSavePackageTypes"), PackageTypeSubmenu );

		// Construct the interval sub-menu that allows the user to specify the time interval between autosaves
		wxMenu* IntervalSubmenu = new wxMenu();
		FString ChangeIntervalToolTip = LocalizeUnrealEd("MainMenu_ChangeAutoSaveInterval_ToolTip");
		IntervalSubmenu->AppendCheckItem( IDM_AUTOSAVE_001, *LocalizeUnrealEd("AutoSaveInterval1"), *ChangeIntervalToolTip );
		IntervalSubmenu->AppendCheckItem( IDM_AUTOSAVE_002, *LocalizeUnrealEd("AutoSaveInterval2"), *ChangeIntervalToolTip );
		IntervalSubmenu->AppendCheckItem( IDM_AUTOSAVE_003, *LocalizeUnrealEd("AutoSaveInterval3"), *ChangeIntervalToolTip );
		IntervalSubmenu->AppendCheckItem( IDM_AUTOSAVE_004, *LocalizeUnrealEd("AutoSaveInterval4"), *ChangeIntervalToolTip );
		IntervalSubmenu->AppendCheckItem( IDM_AUTOSAVE_005, *LocalizeUnrealEd("AutoSaveInterval5"), *ChangeIntervalToolTip );
		Append( wxID_ANY, *LocalizeUnrealEd("AutoSaveInterval"), IntervalSubmenu );
	}
};

/**
 * Menu for controlling options related to the drag grid.
 */
class WxDragGridMenu : public wxMenu
{
public:
	WxDragGridMenu()
	{
		AppendCheckItem( IDM_DRAG_GRID_TOGGLE, *LocalizeUnrealEd("UseDragGrid"), *LocalizeUnrealEd("ToolTip_138") );
		AppendSeparator();
		FString DragGridSizeToolTip = LocalizeUnrealEd("MainMenu_AdjustDragGridSize_ToolTip");
		AppendCheckItem( IDM_DRAG_GRID_1, *LocalizeUnrealEd("1"), *DragGridSizeToolTip );
		AppendCheckItem( IDM_DRAG_GRID_2, *LocalizeUnrealEd("2"), *DragGridSizeToolTip );
		AppendCheckItem( IDM_DRAG_GRID_4, *LocalizeUnrealEd("4"), *DragGridSizeToolTip );
		AppendCheckItem( IDM_DRAG_GRID_8, *LocalizeUnrealEd("8"), *DragGridSizeToolTip );
		AppendCheckItem( IDM_DRAG_GRID_16, *LocalizeUnrealEd("16"), *DragGridSizeToolTip );
		AppendCheckItem( IDM_DRAG_GRID_32, *LocalizeUnrealEd("32"), *DragGridSizeToolTip );
		AppendCheckItem( IDM_DRAG_GRID_64, *LocalizeUnrealEd("64"), *DragGridSizeToolTip );
		AppendCheckItem( IDM_DRAG_GRID_128, *LocalizeUnrealEd("128"), *DragGridSizeToolTip );
		AppendCheckItem( IDM_DRAG_GRID_256, *LocalizeUnrealEd("256"), *DragGridSizeToolTip );
		AppendCheckItem( IDM_DRAG_GRID_512, *LocalizeUnrealEd("512"), *DragGridSizeToolTip );
		AppendCheckItem( IDM_DRAG_GRID_1024, *LocalizeUnrealEd("1024"), *DragGridSizeToolTip );
	}
};

/**
 * Menu for controlling preferences
 */
class WxPreferencesMenu : public wxMenu
{
public:
	TArray<FString> SupportedLanguageExtensions;	/** Language extensions supported by the editor */

	WxPreferencesMenu()
	{
		{
			wxMenu* FlightCameraOptions = new wxMenu();
			FlightCameraOptions->AppendCheckItem( IDM_MainMenu_FlightCameraOptionsStart, *LocalizeUnrealEd( "MainMenu_AlwaysRemapKeysToFlightCamera" ), *LocalizeUnrealEd( "MainMenu_AlwaysRemapKeysToFlightCamera_Desc" ) );
			FlightCameraOptions->AppendCheckItem( IDM_MainMenu_FlightCameraOptionsStart+1, *LocalizeUnrealEd( "MainMenu_RemapKeysToFlightCameraWithRMB" ), *LocalizeUnrealEd( "MainMenu_RemapKeysToFlightCameraWithRMB_Desc" ) );
			FlightCameraOptions->AppendCheckItem( IDM_MainMenu_FlightCameraOptionsStart+2, *LocalizeUnrealEd( "MainMenu_NeverRemapKeysToFlightCamera" ), *LocalizeUnrealEd( "MainMenu_NeverRemapKeysToFlightCamera_Desc" ) );
			Append(IDM_MainMenu_FlightCameraOptionsSubmenu, *LocalizeUnrealEd( "MainMenu_Preferences_FlightCameraOptions" ), FlightCameraOptions);
		}

		AppendCheckItem( IDM_PAN_MOVES_CANVAS, *LocalizeUnrealEd("MainMenu_PanMovesCanvas"), *LocalizeUnrealEd("MainMenu_PanMovesCanvas_ToolTip"));
		AppendCheckItem( IDM_CENTER_ZOOM_AROUND_CURSOR, *LocalizeUnrealEd("MainMenu_CenterZoomAroundCursor"), *LocalizeUnrealEd("MainMenu_CenterZoomAroundCursor_ToolTip"));
		AppendCheckItem( IDM_MainMenu_ToggleLinkedOrthographicViewports, *LocalizeUnrealEd("MainMenu_LinkedOrthographicViewports"), *LocalizeUnrealEd("MainMenu_LinkedOrthographicViewports_ToolTip") );
		AppendCheckItem( IDM_VIEWPORT_RESIZE_TOGETHER, *LocalizeUnrealEd("ResizeTopAndBottomViewportsTogether"), *LocalizeUnrealEd("ToolTip_ResizeTopAndBottomViewportsTogether"));
		AppendAspectRatioSubmenu();
		AppendSeparator();
		AppendCheckItem( IDM_MainMenu_ViewportHoverFeedback, *LocalizeUnrealEd( "MainMenu_ViewportHoverFeedback" ), *LocalizeUnrealEd("MainMenu_ViewportHoverFeedback_ToolTip"));
		AppendCheckItem( IDM_MainMenu_HighlightWithBrackets, *LocalizeUnrealEd( "MainMenu_HighlightWithBrackets" ), *LocalizeUnrealEd( "MainMenu_HighlightWithBrackets_ToolTip" ));
		AppendCheckItem( IDM_USE_WIREFRAME_HALOS, *LocalizeUnrealEd( "MainMenu_UseWireframeHalos" ), *LocalizeUnrealEd( "MainMenu_UseWireframeHalos_Desc") );
		AppendCheckItem( IDM_MainMenu_DefaultToRealtimeMode, *LocalizeUnrealEd( "MainMenu_DefaultToRealtimeMode" ), *LocalizeUnrealEd("MainMenu_DefaultToRealtimeMode_ToolTip"));
		AppendCheckItem( IDM_MainMenu_ViewportCameraToUpdateFromPIV, *LocalizeUnrealEd( "MainMenu_ViewportCameraToUpdateFromPIV" ), *LocalizeUnrealEd( "MainMenu_ViewportCameraToUpdateFromPIV_ToolTip" ) );
		Append( IDM_MainMenu_ResetSuppressibleDialogs, *LocalizeUnrealEd( "MainMenu_ResetSuppressibleDialogs" ), *LocalizeUnrealEd( "MainMenu_ResetSuppressibleDialogs_ToolTip" ) );
		AppendSeparator();
		AppendCheckItem( IDM_MainMenu_WidgetSettingsUseAbsoluteTranslation, *LocalizeUnrealEd( "WidgetSettings_AbsoluteTranslation" ), *LocalizeUnrealEd("WidgetSettings_AbsoluteTranslation_ToolTip"));
		AppendCheckItem( IDM_MainMenu_UseTranslateRotateZWidget, *LocalizeUnrealEd( "WidgetSettings_IncludeTranslateRotateZWidget" ), *LocalizeUnrealEd("WidgetSettings_IncludeTranslateRotateZWidget_ToolTip"));
		AppendCheckItem( IDM_CLICK_BSP_SELECTS_BRUSH, *LocalizeUnrealEd( "MainMenu_ClickBSPSelectsBrush" ), *LocalizeUnrealEd( "MainMenu_ClickBSPSelectsBrush_ToolTip" ));
		AppendCheckItem( IDM_BSP_AUTO_UPDATE, *LocalizeUnrealEd( "MainMenu_AutoUpdatingBSP" ), *LocalizeUnrealEd( "MainMenu_AutoUpdatingBSP_ToolTip" ));
		AppendCheckItem( IDM_REPLACE_RESPECTS_SCALE, *LocalizeUnrealEd("MainMenu_ReplaceRespectsScale"), *LocalizeUnrealEd("MainMenu_ReplaceRespectsScale_ToolTip"));
		AppendSeparator();
#if HAVE_SCC
		AppendCheckItem( IDM_MainMenu_PromptSCCOnPackageModification, *LocalizeUnrealEd("MainMenu_PromptSCCOnPackageModification"), *LocalizeUnrealEd("MainMenu_PromptSCCOnPackageModification_Desc") );
#endif // #if HAVE_SCC
		AppendCheckItem( IDM_MainMenu_AutoReimportTextures, *LocalizeUnrealEd( "MainMenu_AutoReimportTextures" ), *LocalizeUnrealEd( "MainMenu_AutoReimportTextures_Desc" ) );
		AppendCheckItem( IDM_MainMenu_AutoRestartReimportedFlashMovies, *LocalizeUnrealEd( "MainMenu_AutoRestartReimportedFlashMovies" ), *LocalizeUnrealEd( "MainMenu_AutoRestartReimportedFlashMovies_ToolTip" ) );
		AppendCheckItem( IDM_AlwaysOptimizeContentForMobile, *LocalizeUnrealEd("MainMenu_AlwaysOptimizeContentForMobile"), *LocalizeUnrealEd("MainMenu_AlwaysOptimizeContentForMobile_ToolTip") );
		AppendSeparator();
		AppendCheckItem( IDM_DISTRIBUTION_TOGGLE, *LocalizeUnrealEd("MainMenu_DistributionToggle"), *LocalizeUnrealEd("ToolTip_DistributionToggle") );
		AppendCheckItem( IDM_MaterialQualityToggle, *LocalizeUnrealEd("MainMenu_MatQualityToggle"), *LocalizeUnrealEd("ToolTip_MatQualityToggle") );
	

		if( FEditorFileUtils::GetSimpleMapName().Len() > 0 )
		{
			AppendSeparator();
			AppendCheckItem( IDM_MainMenu_LoadSimpleLevelAtStartup, *LocalizeUnrealEd( "MainMenu_LoadSimpleLevelAtStartup" ), *LocalizeUnrealEd( "MainMenu_LoadSimpleLevelAtStartup_Tooltip") );
		}

		AppendSeparator();
		AppendLanguageSubmenu();
	}

	/**
	 * Append a sub-menu to the preferences menu full of all of the various language options to switch the editor to
	 */
	void AppendLanguageSubmenu()
	{
		wxMenu* LanguageSubMenu = new wxMenu();

		// Get a list of known language extensions
		const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();

		// Ensure that the provided ID range in ResourceIDs.h is enough to support the number of known languages
		check( ( IDM_MainMenu_LanguageOptionEnd - IDM_MainMenu_LanguageOptionStart ) > KnownLanguageExtensions.Num() ); 

		// Append each language extension that there is a corresponding UnrealEd.XXX file for as a menu option
		for ( INT LangIndex = 0; LangIndex < KnownLanguageExtensions.Num(); ++LangIndex )
		{
			// Construct the file name to search for
			FString FileName = FString::Printf( TEXT("%s%s%s%s.%s"), *appEngineDir(), TEXT("Localization\\"), *KnownLanguageExtensions(LangIndex), TEXT("\\UnrealEd"), *KnownLanguageExtensions(LangIndex) );
			
			TArray<FString> FoundFiles;
			GFileManager->FindFiles( FoundFiles, *appConvertRelativePathToFull( FileName ), TRUE, FALSE );
			
			// If the file was found, append it to the menu and to the list of supported language extensions
			if ( FoundFiles.Num() > 0 )
			{
				SupportedLanguageExtensions.AddItem( KnownLanguageExtensions(LangIndex) );
				FString LocalizedLanguage = LocalizeUnrealEd( *KnownLanguageExtensions(LangIndex) );
				FString LocalizedToolTip = FString::Printf( LocalizeSecure( LocalizeUnrealEd("MainMenu_ChangeLanguage_ToolTip"), *LocalizedLanguage ) );
				LanguageSubMenu->AppendCheckItem( IDM_MainMenu_LanguageOptionStart + SupportedLanguageExtensions.Num() - 1, *LocalizedLanguage, *LocalizedToolTip );
			}
		}

		// Append the submenu to the preferences menu
		Append( IDM_MainMenu_LanguageSubmenu, *LocalizeUnrealEd( "MainMenu_Preferences_Language" ), LanguageSubMenu );
	}

	/** Sub menu for how to constrain aspect ratio in the editor */
	void AppendAspectRatioSubmenu()
	{
		wxMenu* AspectRatioSubMenu = new wxMenu();

		AspectRatioSubMenu->AppendCheckItem( IDM_MainMenu_AspectRatioAxisConstraint_Start + AspectRatio_MaintainYFOV, *LocalizeUnrealEd( "MainMenu_AspectRatioAxisConstraint_MaintainYFOV" ) );
		AspectRatioSubMenu->AppendCheckItem( IDM_MainMenu_AspectRatioAxisConstraint_Start + AspectRatio_MaintainXFOV, *LocalizeUnrealEd( "MainMenu_AspectRatioAxisConstraint_MaintainXFOV" ) );
		AspectRatioSubMenu->AppendCheckItem( IDM_MainMenu_AspectRatioAxisConstraint_Start + AspectRatio_MajorAxisFOV, *LocalizeUnrealEd( "MainMenu_AspectRatioAxisConstraint_MajorAxisFOV" ) );

		// Append the submenu to the preferences menu
		Append( IDM_MainMenu_AspectRatioAxisConstraint_Submenu, *LocalizeUnrealEd( "MainMenu_AspectRatioAxisConstraint" ), AspectRatioSubMenu );

	}

};



/*-----------------------------------------------------------------------------
	WxMainMenu.
-----------------------------------------------------------------------------*/

WxMainMenu::WxMainMenu()
{
	// Allocations

	FileMenu = new wxMenu();
	MRUMenu = new wxMenu();
	ImportMenu = new wxMenu();
	ExportMenu = new wxMenu();
	EditMenu = new wxMenu();
	ViewMenu = new wxMenu();
	BrowserMenu = new wxMenu();
	ViewportConfigMenu = new wxMenu();
	OpenNewFloatingViewportMenu = new wxMenu();
	DetailModeMenu = new wxMenu();
	LightingInfoMenu = new wxMenu();
	BrushMenu = new wxMenu();
	BuildMenu = new wxMenu();
	PlayMenu = new wxMenu();
	ToolsMenu = new wxMenu();
	HelpMenu = new wxMenu();
	VolumeMenu = new wxMenu();
	FavoritesMenu = new wxMenu();
	MRUFavoritesCombinedMenu = new wxMenu();
	RenderModeMenu = new wxMenu();
	LockReadOnlyLevelsItem = NULL;
	

	// File menu
	{
		// Popup Menus
		{
			ImportMenu->Append( IDM_IMPORT_NEW, *LocalizeUnrealEd("IntoNewMapE"), *LocalizeUnrealEd("ToolTip_76") );
			ImportMenu->Append( IDM_IMPORT_MERGE, *LocalizeUnrealEd("IntoExistingMapE"), *LocalizeUnrealEd("ToolTip_77") );

			ExportMenu->Append( IDM_EXPORT_ALL, *LocalizeUnrealEd("AllE"), *LocalizeUnrealEd("ToolTip_78") );
			ExportMenu->Append( IDM_EXPORT_SELECTED, *LocalizeUnrealEd("SelectedOnlyE"), *LocalizeUnrealEd("ToolTip_79") );
		}

		FileMenu->Append( IDM_NEWMAP, *LocalizeUnrealEd("MainFileMenu_NewLevel"), *LocalizeUnrealEd("ToolTip_80") );
		FileMenu->Append( IDM_OPEN, *LocalizeUnrealEd("&OpenE"), *LocalizeUnrealEd("ToolTip_81") );
		FileMenu->AppendSeparator();
		FileMenu->Append( IDM_SAVE, *LocalizeUnrealEd("&SaveCurrentLevel"), *LocalizeUnrealEd("ToolTip_82") );
		FileMenu->Append( IDM_SAVE_AS, *LocalizeUnrealEd("Save&AsE"), *LocalizeUnrealEd("ToolTip_83") );
		FileMenu->Append( IDM_SAVE_DLG, *LocalizeUnrealEd("MainFileMenu_SaveModified"), *LocalizeUnrealEd("ToolTip_SaveDlg") );
		FileMenu->Append( IDM_SAVE_ALL, *LocalizeUnrealEd("SaveA&ll"), *LocalizeUnrealEd("ToolTip_84") );
		FileMenu->Append( IDM_SAVE_ALL_WRITABLE, *LocalizeUnrealEd("SaveAllWritable"), *LocalizeUnrealEd("ToolTip_SaveAllWritable") );
		FileMenu->Append( IDM_FORCE_SAVE_ALL, *LocalizeUnrealEd("ForceSaveAll"), *LocalizeUnrealEd("ToolTip_ForceSaveAll") );
		FileMenu->AppendSeparator();

		// Add render modes to the menu
		FileMenu->Append( IDM_EDITOR_PREFERENCES_RENDER_MODE_MENU, *LocalizeUnrealEd( "EditorPreferences_RenderMode" ), RenderModeMenu );

		UBOOL bSupportsD3D11Features = FALSE;

#if _WINDOWS
		UBOOL bSupportsD3D11RHI = IsDirect3D11Supported( bSupportsD3D11Features );
		if( bSupportsD3D11Features )
		{
			RenderModeMenu->AppendCheckItem( IDM_EDITOR_PREFERENCES_DX9_MODE, *LocalizeUnrealEd( "EditorPreferences_RenderMode_DX9" ), *LocalizeUnrealEd( "EditorPreferences_RenderMode_DX9" ) );
			RenderModeMenu->AppendCheckItem( IDM_EDITOR_PREFERENCES_DX11_MODE, *LocalizeUnrealEd( "EditorPreferences_RenderMode_DX11" ), *LocalizeUnrealEd( "EditorPreferences_RenderMode_DX11" ) );
		}
#endif

		FileMenu->Enable( IDM_EDITOR_PREFERENCES_RENDER_MODE_MENU, bSupportsD3D11Features == TRUE );


		FileMenu->AppendSeparator();
		FileMenu->Append( IDM_IMPORT, *LocalizeUnrealEd("&Import"), ImportMenu );
		FileMenu->Append( IDM_EXPORT, *LocalizeUnrealEd("&Export"), ExportMenu );
		//		FileMenu->Append( IDM_CREATEARCHETYPE, TEXT("Save As Archetype...") );
		FileMenu->AppendSeparator();
		FileMenu->Append( IDM_FAVORITES_LIST, *LocalizeUnrealEd("&Favorites"), FavoritesMenu );
		FileMenu->Append( IDM_MRU_LIST, *LocalizeUnrealEd("&Recent"), MRUMenu, *LocalizeUnrealEd("MainToolBar_OpenRecentLevel") );
		FileMenu->AppendSeparator();
		FileMenu->Append( IDM_EXIT, *LocalizeUnrealEd("E&xit"), *LocalizeUnrealEd("ToolTip_85") );

		Append( FileMenu, *LocalizeUnrealEd("File") );
	}

	// Edit menu
	{
		EditMenu->Append( IDM_UNDO, *LocalizeUnrealEd("UndoA"), *LocalizeUnrealEd("ToolTip_86") );
		EditMenu->Append( IDM_REDO, *LocalizeUnrealEd("RedoA"), *LocalizeUnrealEd("ToolTip_87") );
		EditMenu->AppendSeparator();
		EditMenu->AppendCheckItem( ID_EDIT_TRANSLATE, *LocalizeUnrealEd("Translate"), *LocalizeUnrealEd("ToolTip_89") );
		EditMenu->AppendCheckItem( ID_EDIT_ROTATE, *LocalizeUnrealEd("Rotate"), *LocalizeUnrealEd("ToolTip_90") );
		EditMenu->AppendCheckItem( ID_EDIT_SCALE, *LocalizeUnrealEd("Scale"), *LocalizeUnrealEd("ToolTip_91") );
		EditMenu->AppendSeparator();
		EditMenu->Append( IDM_CUT, *LocalizeUnrealEd("CutA"), *LocalizeUnrealEd("ToolTip_93") );
		EditMenu->Append( IDM_COPY, *LocalizeUnrealEd("CopyA"), *LocalizeUnrealEd("ToolTip_94") );
		EditMenu->Append( IDM_PASTE, *LocalizeUnrealEd("PasteA"), *LocalizeUnrealEd("ToolTip_95") );
		EditMenu->Append( IDM_DUPLICATE, *LocalizeUnrealEd("DuplicateA"), *LocalizeUnrealEd("ToolTip_96") );
		EditMenu->Append( IDM_DELETE, *LocalizeUnrealEd("DeleteA"), *LocalizeUnrealEd("ToolTip_97") );
		EditMenu->AppendSeparator();
		EditMenu->Append( IDM_SELECT_NONE, *LocalizeUnrealEd("MainEditMenu_SelectNone"), *LocalizeUnrealEd("ToolTip_98") );
		EditMenu->Append( IDM_SELECT_BUILDER_BRUSH, *LocalizeUnrealEd("MainEditMenu_SelectBuilderBrush"), *LocalizeUnrealEd("MainMenu_SelectBuilderBrush_ToolTip") );
		EditMenu->Append( IDM_SELECT_ALL, *LocalizeUnrealEd("MainEditMenu_SelectAll"), *LocalizeUnrealEd("ToolTip_99") );
		EditMenu->Append( IDM_SELECT_ByProperty, *LocalizeUnrealEd("SelectByProperty"), *LocalizeUnrealEd("ToolTip_SelectByProperty") );
		EditMenu->Append( IDM_SELECT_POST_PROCESS_VOLUME, *LocalizeUnrealEd("SelectPostProcessVolume"), *LocalizeUnrealEd("ToolTip_SelectPostProcessVolume") );
		EditMenu->Append( IDM_SELECT_INVERT, *LocalizeUnrealEd("InvertSelections"), *LocalizeUnrealEd("ToolTip_100") );

		Append( EditMenu, *LocalizeUnrealEd("Edit") );
	}

	// View menu
	{
		// Popup Menus
		{
			GUnrealEd->GetBrowserManager()->InitializeBrowserMenu(BrowserMenu);

			for( INT x = 0 ; x < GApp->EditorFrame->ViewportConfigTemplates.Num() ; ++x )
			{
				FViewportConfig_Template* vct = GApp->EditorFrame->ViewportConfigTemplates(x);
				ViewportConfigMenu->AppendCheckItem( IDM_VIEWPORT_CONFIG_START+x, *(vct->Desc), *FString::Printf( LocalizeSecure(LocalizeUnrealEd("ToolTip_108"), *(vct->Desc)) ) );
			}

			// Open Floating Viewport menu
			OpenNewFloatingViewportMenu->Append( IDM_OpenNewFloatingViewport_Perspective, *LocalizeUnrealEd( "OpenNewFloatingViewport_Perspective" ), *LocalizeUnrealEd( "OpenNewFloatingViewport_Perspective_Desc" ) );
			OpenNewFloatingViewportMenu->Append( IDM_OpenNewFloatingViewport_OrthoXY, *LocalizeUnrealEd( "OpenNewFloatingViewport_OrthoXY" ), *LocalizeUnrealEd( "OpenNewFloatingViewport_OrthoXY_Desc" ) );
			OpenNewFloatingViewportMenu->Append( IDM_OpenNewFloatingViewport_OrthoXZ, *LocalizeUnrealEd( "OpenNewFloatingViewport_OrthoXZ" ), *LocalizeUnrealEd( "OpenNewFloatingViewport_OrthoXZ_Desc" ) );
			OpenNewFloatingViewportMenu->Append( IDM_OpenNewFloatingViewport_OrthoYZ, *LocalizeUnrealEd( "OpenNewFloatingViewport_OrthoYZ" ), *LocalizeUnrealEd( "OpenNewFloatingViewport_OrthoYZ_Desc" ) );

			DetailModeMenu->AppendCheckItem( IDM_VIEW_DETAILMODE_LOW, *LocalizeUnrealEd("Low"), *LocalizeUnrealEd("Low") );
			DetailModeMenu->AppendCheckItem( IDM_VIEW_DETAILMODE_MEDIUM, *LocalizeUnrealEd("Medium"), *LocalizeUnrealEd("Medium") );
			DetailModeMenu->AppendCheckItem( IDM_VIEW_DETAILMODE_HIGH, *LocalizeUnrealEd("High"), *LocalizeUnrealEd("High") );

			LightingInfoMenu->Append( IDM_LIGHTINGINFO_LIGHTING_TOOLS, *LocalizeUnrealEd("LightingTools"), *LocalizeUnrealEd("LightingToolsTip") );
			LightingInfoMenu->Append( IDM_LIGHTINGINFO_RESULTS, *LocalizeUnrealEd("LightingResults"), *LocalizeUnrealEd("LightingResultsToolTip") );
			LightingInfoMenu->Append( IDM_LIGHTINGINFO_BUILDINFO, *LocalizeUnrealEd("LightingBuildInfo"), *LocalizeUnrealEd("LightingBuildInfoToolTip") );
			LightingInfoMenu->Append( IDM_LIGHTINGINFO_STATICMESHINFO, *LocalizeUnrealEd("LightingStaticMeshInfo"), *LocalizeUnrealEd("LightingStaticMeshInfoToolTip") );
			LightingInfoMenu->Append( IDM_LIGHTINGINFO_DENSITY_RENDERINGOPTIONS, *LocalizeUnrealEd("LightMapDensityRenderingOptions"), *LocalizeUnrealEd("LightMapDensityRenderingOptionsToolTip") );
			LightingInfoMenu->Append( IDM_LIGHTINGINFO_RESOLUTION_RATIOADJUST, *LocalizeUnrealEd("LightMapRatioAdjust"), *LocalizeUnrealEd("LightMapRatioAdjustToolTip") );
		}

		ViewMenu->Append( IDM_BROWSER, *LocalizeUnrealEd("BrowserWindows"), BrowserMenu );
		EditMenu->AppendSeparator();
		EditMenu->Append( IDM_SEARCH, *LocalizeUnrealEd("MainMenu_FindActors"), *LocalizeUnrealEd("MainMenu_FindActors_Desc") );
		ViewMenu->AppendSeparator();
		ViewMenu->Append( IDM_ACTOR_PROPERTIES, *LocalizeUnrealEd("ActorProperties"), *LocalizeUnrealEd("ToolTip_109") );
		ViewMenu->Append( IDM_SURFACE_PROPERTIES, *LocalizeUnrealEd("SurfaceProperties"), *LocalizeUnrealEd("ToolTip_110") );
		ViewMenu->Append( IDM_WORLD_PROPERTIES, *LocalizeUnrealEd("WorldProperties"), *LocalizeUnrealEd("ToolTip_111") );
		ViewMenu->AppendSeparator();
		ViewMenu->Append( IDM_OPEN_KISMET, *LocalizeUnrealEd("Kismet"), *LocalizeUnrealEd("ToolTip_107") );
		ViewMenu->Append( IDM_MainMenu_OpenMatinee, *LocalizeUnrealEd( "MainMenu_OpenMatinee" ), *LocalizeUnrealEd( "MainMenu_OpenMatinee_Desc" ) );


#if !UDK
		// If a database is set, add 'Sentinel' option
		FString DummySource;
		if(GConfig->GetString( TEXT("SentinelStats"), TEXT("SentinelStatsSource"), DummySource, GEditorIni))
		{
			ViewMenu->AppendCheckItem( IDM_OPEN_SENTINEL, *LocalizeUnrealEd("OpenSentinel"), TEXT("") );
		}
#endif	//#if !UDK

		ViewMenu->AppendSeparator();
		ViewMenu->Append( IDM_DRAG_GRID, *LocalizeUnrealEd("DragGrid"), GApp->EditorFrame->GetDragGridMenu() );
		ViewMenu->Append( IDM_ROTATION_GRID, *LocalizeUnrealEd("RotationGrid"), GApp->EditorFrame->GetRotationGridMenu() );
		ViewMenu->Append( IDM_SCALE_GRID, *LocalizeUnrealEd("ScaleGrid"), GApp->EditorFrame->GetScaleGridMenu() );
		ViewMenu->AppendSeparator();
		ViewMenu->Append( IDM_AUTOSAVE_INTERVAL, *LocalizeUnrealEd("ToolTip_AutosaveMenu"), GApp->EditorFrame->GetAutoSaveOptionsMenu() );
		ViewMenu->Append( IDM_VIEW_DETAILMODE, *LocalizeUnrealEd("DetailMode"), DetailModeMenu );
		ViewMenu->AppendSeparator();
		ViewMenu->AppendCheckItem( IDM_EmulateMobileFeatures, *LocalizeUnrealEd("MainMenu_EmulateMobileFeatures"), *LocalizeUnrealEd("MainMenu_EmulateMobileFeatures_ToolTip") );
		ViewMenu->AppendSeparator();
		ViewMenu->AppendCheckItem( ID_EDIT_SHOW_WIDGET, *LocalizeUnrealEd("ShowWidget"), *LocalizeUnrealEd("ToolTip_88") );
		ViewMenu->AppendCheckItem( IDM_MainToolBar_SelectTranslucent, *LocalizeUnrealEd("MainMenu_SelectTranslucent"), *LocalizeUnrealEd("MainMenu_SelectTranslucent_ToolTip") );
		ViewMenu->AppendCheckItem( IDM_ToggleGroupsActive, *LocalizeUnrealEd("ToggleGroupsActive"), *LocalizeUnrealEd("ToggleGroupsActive") );
		ViewMenu->AppendCheckItem( IDM_MainMenu_UseStrictBoxSelection, *LocalizeUnrealEd( "MainMenu_UseStrictBoxSelection" ), *LocalizeUnrealEd( "MainMenu_UseStrictBoxSelection_Desc") );
		ViewMenu->AppendCheckItem( IDM_BRUSHPOLYS, *LocalizeUnrealEd("BrushMarkerPolys"), *LocalizeUnrealEd("ToolTip_112") );
		ViewMenu->AppendCheckItem( IDM_MainToolBar_PIEVisibleOnly, *LocalizeUnrealEd("MainMenu_PIEVisibleOnly"), *LocalizeUnrealEd("MainMenu_PIEVisibleOnly_ToolTip") );
		ViewMenu->AppendCheckItem( IDM_TogglePrefabsLocked, *LocalizeUnrealEd("TogglePrefabLock"), *LocalizeUnrealEd("TogglePrefabLock") );
		ViewMenu->AppendCheckItem( ID_ToggleSocketSnapping, *LocalizeUnrealEd("MainMenu_ToggleSocketSnapping"), *LocalizeUnrealEd("ToolTip_ToggleSocketSnapping") );
		ViewMenu->AppendCheckItem( ID_ShowSocketNames, *LocalizeUnrealEd("MainMenu_ShowSocketNames"), *LocalizeUnrealEd("ToolTip_ShowSocketNames") );
		ViewMenu->AppendCheckItem( IDM_PSYSLODREALTIME_TOGGLE, *LocalizeUnrealEd("MainMenu_PSysLODRealtimeToggle"), *LocalizeUnrealEd("ToolTip_PSysLODRealtimeToggle") );
		ViewMenu->AppendCheckItem( IDM_PSYSHELPER_TOGGLE, *LocalizeUnrealEd("MainMenu_PsysHelperToggle"), *LocalizeUnrealEd("ToolTip_PsysHelperToggle") );
		ViewMenu->AppendCheckItem( IDM_ToggleLODLocking, *LocalizeUnrealEd("MainMenu_ToggleLODLocking"), *LocalizeUnrealEd("ToolTip_ToggleLODLocking") );
		ViewMenu->AppendSeparator();
		ViewMenu->Append( IDM_OpenNewFloatingViewport, *LocalizeUnrealEd( "OpenNewFloatingViewport" ), OpenNewFloatingViewportMenu );
		ViewMenu->Append( IDM_VIEWPORT_CONFIG, *LocalizeUnrealEd("ViewportConfiguration"), ViewportConfigMenu );
		ViewMenu->AppendCheckItem( IDM_FULLSCREEN, *LocalizeUnrealEd("MainViewMenu_Fullscreen"), *LocalizeUnrealEd("ToolTip_113") );
		ViewMenu->AppendSeparator();
		ViewMenu->Append( IDM_LIGHTINGINFO, *LocalizeUnrealEd("LightingInfo"), LightingInfoMenu );
#if USE_UNIT_TESTS && WITH_MANAGED_CODE
		ViewMenu->AppendSeparator();
		ViewMenu->Append( IDM_SummonUnitTestDialog, *LocalizeUnrealEd("RunUnitTests"), *LocalizeUnrealEd("ToolTip_RunUnitTests") );
#endif // #if USE_UNIT_TESTS && WITH_MANAGED_CODE


		Append( ViewMenu, *LocalizeUnrealEd("View") );
	}


	// Brush menu
	{
		BrushMenu->Append( IDM_BRUSH_ADD, *LocalizeUnrealEd("MainBrushMenu_CSGAdd"), *LocalizeUnrealEd("ToolTip_114") );
		BrushMenu->Append( IDM_BRUSH_SUBTRACT, *LocalizeUnrealEd("MainBrushMenu_CSGSubtract"), *LocalizeUnrealEd("ToolTip_115") );
		BrushMenu->Append( IDM_BRUSH_INTERSECT, *LocalizeUnrealEd("MainBrushMenu_CSGIntersect"), *LocalizeUnrealEd("ToolTip_116") );
		BrushMenu->Append( IDM_BRUSH_DEINTERSECT, *LocalizeUnrealEd("MainBrushMenu_CSGDeintersect"), *LocalizeUnrealEd("ToolTip_117") );
		BrushMenu->AppendSeparator();
		BrushMenu->Append( IDM_BRUSH_ADD_SPECIAL, *LocalizeUnrealEd("AddSpecial"), *LocalizeUnrealEd("ToolTip_120") );

		// Volume menu
		{
			// Get sorted array of volume classes then create a menu item for each one
			TArray< UClass* > VolumeClasses;

			GApp->EditorFrame->GetSortedVolumeClasses( &VolumeClasses );

			INT ID = IDM_VolumeClasses_START;

			for( INT VolumeIdx = 0; VolumeIdx < VolumeClasses.Num(); VolumeIdx++ )
			{
				FString VolumeName = VolumeClasses( VolumeIdx )->GetName();
				VolumeMenu->Insert( 0, ID, *VolumeName, *FString::Printf( LocalizeSecure( LocalizeUnrealEd("MainMenu_AddAVolume_ToolTip"), *VolumeName ) ), 0 );
				ID++;
			}

			BrushMenu->Append( IDMENU_ActorPopupVolumeMenu, *LocalizeUnrealEd("AddVolumePopUp"), VolumeMenu );
		}

		BrushMenu->AppendSeparator();
		BrushMenu->Append( ID_BRUSH_IMPORT, *LocalizeUnrealEd("ImportE"), *LocalizeUnrealEd("ToolTip_121") );
		BrushMenu->Append( ID_BRUSH_EXPORT, *LocalizeUnrealEd("ExportE"), *LocalizeUnrealEd("ToolTip_122") );

		Append( BrushMenu, *LocalizeUnrealEd("Brush") );
	}

	// Build menu
	{
		BuildMenu->Append( IDM_BUILD_GEOMETRY, *LocalizeUnrealEd("Geometry"), *LocalizeUnrealEd("ToolTip_123") );
		BuildMenu->Append( IDM_BUILD_VISIBLEGEOMETRY, *LocalizeUnrealEd("BuildVisibleGeometry"), *LocalizeUnrealEd("BuildVisibleGeometry_HelpText"));
		BuildMenu->Append( IDM_BUILD_LIGHTING, *LocalizeUnrealEd("MainBuildMenu_Lighting"), *LocalizeUnrealEd("ToolTip_124") );
		BuildMenu->Append( IDM_BUILD_AI_PATHS, *LocalizeUnrealEd("AIPaths"), *LocalizeUnrealEd("ToolTip_148") );
		BuildMenu->Append( IDM_BUILD_SELECTED_AI_PATHS, *LocalizeUnrealEd("SelectedAIPaths"), *LocalizeUnrealEd("ToolTip_125") );
		BuildMenu->AppendSeparator();
		BuildMenu->Append( IDM_BUILD_ALL, *LocalizeUnrealEd("BuildAll"), *LocalizeUnrealEd("ToolTip_127") );
		BuildMenu->Append( IDM_BUILD_ALL_ONLY_SELECTED_PATHS, *LocalizeUnrealEd("BuildAllSelectedPaths"), *LocalizeUnrealEd("ToolTip_149") );
#if HAVE_SCC && WITH_MANAGED_CODE
		BuildMenu->Append( IDM_BUILD_ALL_SUBMIT, *LocalizeUnrealEd("BuildAllAndSubmit"), *LocalizeUnrealEd("BuildAllAndSubmit_ToolTip") );
#endif // #if HAVE_SCC && WITH_MANAGED_CODE

		Append( BuildMenu, *LocalizeUnrealEd("Build") );
		// Add any console-specific menus (max out at 20 consoles)
		INT ConsoleIndex = 0;
		for (FConsoleSupportIterator It; It && ConsoleIndex < 20; ++It, ConsoleIndex++)
		{
			// If this console has any menu items, add the menu
			if (It->GetNumMenuItems() > 0)
			{
				// make a new menu
				ConsoleMenu[ConsoleIndex] = new wxMenu();

				// Put the new menu on the main menu bar
				const wchar_t* ConsoleName = It->GetPlatformName();
				if( ( appStricmp( ConsoleName, CONSOLESUPPORT_NAME_ANDROID ) == 0 ) ||
					( appStricmp( ConsoleName, CONSOLESUPPORT_NAME_IPHONE ) == 0 ) ||
					( appStricmp( ConsoleName, CONSOLESUPPORT_NAME_MAC ) == 0 ) )
				{
					// No need (support?) for mobile platform or Mac specific menus
				}
				else
				{
					Append(ConsoleMenu[ConsoleIndex], ConsoleName);
				}
			}
		}
	}

	// Play Menu
	{
		// if we have any console plugins, add them to the list of places we can play the level
		if (FConsoleSupportContainer::GetConsoleSupportContainer()->GetNumConsoleSupports() > 0)
		{
			// we always can play in the editor
			PlayMenu->Append(IDM_BuildPlayInEditor, *LocalizeUnrealEd("InEditor"), *LocalizeUnrealEd("ToolTip_128"));
			PlayMenu->Append(IDM_PlayInActiveViewport, *LocalizeUnrealEd("PlayInActiveViewport"), *LocalizeUnrealEd("ToolTip_134"));
			// loop through all consoles (only support 20 consoles)
			INT ConsoleIndex = 0;
			for (FConsoleSupportIterator It; It && ConsoleIndex < 20; ++It, ConsoleIndex++)
			{
				const wchar_t* ConsoleName = It->GetPlatformName();
				if( ( appStricmp( ConsoleName, CONSOLESUPPORT_NAME_ANDROID ) == 0 ) ||
					( appStricmp( ConsoleName, CONSOLESUPPORT_NAME_MAC ) == 0 ) )
				{
					// No "Play On" support for the Android or Mac platforms (yet!)
				}
				else
				{
					PlayMenu->AppendSeparator();

					if (appStricmp(ConsoleName, CONSOLESUPPORT_NAME_IPHONE) == 0)
					{
						// Use a custom menu string for iPhone devices.
						PlayMenu->Append(
							IDM_BuildPlayConsole_START + ConsoleIndex, 
							*FString::Printf(LocalizeSecure(LocalizeUnrealEd("ToolTip_InstallOnIOSDevice"), ConsoleName)), 
							*FString::Printf(LocalizeSecure(LocalizeUnrealEd("ToolTip_129"), ConsoleName))
							);

						// We'll also add a 'play using mobile preview' command
						PlayMenu->Append(
							IDM_PlayUsingMobilePreview, 
							*FString::Printf(LocalizeSecure(LocalizeUnrealEd("OnF"), *LocalizeUnrealEd(TEXT("MobilePreview")))), 
							*FString::Printf(LocalizeSecure(LocalizeUnrealEd("ToolTip_129"), *LocalizeUnrealEd(TEXT("MobilePreview"))))
							);
					}
					else
					{
						// add a per-console Play On XXX menu
						PlayMenu->Append(
							IDM_BuildPlayConsole_START + ConsoleIndex, 
							*FString::Printf(LocalizeSecure(LocalizeUnrealEd("OnF"), ConsoleName)), 
							*FString::Printf(LocalizeSecure(LocalizeUnrealEd("ToolTip_129"), ConsoleName))
							);
					}
				}
			}
		}
		else
		{
			// if there are no platforms, then just put the play in editor menu item in the base Build menu
			PlayMenu->Append(IDM_BuildPlayInEditor, *LocalizeUnrealEd("PlayLevel"), *LocalizeUnrealEd("ToolTip_128"));
			PlayMenu->Append(IDM_PlayInActiveViewport, *LocalizeUnrealEd("PlayInActiveViewport"), *LocalizeUnrealEd("ToolTip_134"));
		}

		Append( PlayMenu, *LocalizeUnrealEd("Play") );
	}



	// Tools menu
	{
		ToolsMenu->Append( IDMN_TOOL_CHECK_ERRORS, *LocalizeUnrealEd("CheckMapForErrorsE"), *LocalizeUnrealEd("ToolTip_133") );
		ToolsMenu->AppendSeparator();
		ToolsMenu->Append( IDM_WIZARD_NEW_TERRAIN, *LocalizeUnrealEd("NewTerrainE"), *LocalizeUnrealEd("ToolTip_132") );
		ToolsMenu->AppendSeparator();
		ToolsMenu->Append( IDM_CleanBSPMaterials, *LocalizeUnrealEd("CleanBSPMaterials"), *LocalizeUnrealEd("MainMenu_CleanBSPMaterials_ToolTip") );
		ToolsMenu->AppendSeparator();
		ToolsMenu->Append( IDM_REPLACESKELMESHACTORS, *LocalizeUnrealEd("ReplaceSkeletalMeshActors"), *LocalizeUnrealEd("MainMenu_ReplaceSkeletalMeshActors_ToolTip") );
		ToolsMenu->AppendSeparator();
		ToolsMenu->Append( IDM_REGENALLPROCBUILDINGS, *LocalizeUnrealEd("RegenAllProcBuildings"), *LocalizeUnrealEd("MainMenu_RegenAllProcBuildings_ToolTip") );
		ToolsMenu->Append( IDM_REGENSELPROCBUILDINGS, *LocalizeUnrealEd("RegenSelProcBuildings"), *LocalizeUnrealEd("MainMenu_RegenSelectedProcBuildings_ToolTip") );
		ToolsMenu->AppendSeparator();
		ToolsMenu->Append( IDM_GENALLPROCBUILDINGLODTEX, *LocalizeUnrealEd("GenerateAllProcBuildingLODTex"), *LocalizeUnrealEd("MainMenu_RegenAllProcBuildingLODTex_ToolTip") );
		ToolsMenu->Append( IDM_GENSELPROCBUILDINGLODTEX, *LocalizeUnrealEd("GenerateSelProcBuildingLODTex"), *LocalizeUnrealEd("MainMenu_RegenSelectedProcBuildingLODText_ToolTip") );
		ToolsMenu->AppendSeparator();
		LockReadOnlyLevelsItem = ToolsMenu->Append( IDM_LOCKREADONLYLEVELS, *LocalizeUnrealEd("LockReadOnlyLevels"), *LocalizeUnrealEd("MainMenu_LockReadOnlyLevels_ToolTip") );
		ToolsMenu->AppendSeparator();
		ToolsMenu->Append( IDM_SETFILELISTENERS, *(LocalizeUnrealEd("SetFileListeners")+TEXT("...")), *LocalizeUnrealEd("MainMenu_SetFileListeners_ToolTip") );
		ToolsMenu->AppendSeparator();
		ToolsMenu->Append( IDM_JOURNALUPDATE, *LocalizeUnrealEd("JournalUpdate"), *LocalizeUnrealEd("MainMenu_JournalUpdate_ToolTip") );

		Append( ToolsMenu, *LocalizeUnrealEd("Tools") );
	}


	// Preferences Menu
	{
		Append( GApp->EditorFrame->GetPreferencesMenu(), *LocalizeUnrealEd("MainMenu_Preferences") );
	}


	// Help Menu
	{
#if WITH_MANAGED_CODE
		HelpMenu->Append( IDMENU_HELP_WELCOMESCREEN, *LocalizeUnrealEd("MainMenu_Help_WelcomeScreen"), *LocalizeUnrealEd("MainMenu_Help_WelcomeScreen_ToolTip") );
		HelpMenu->AppendSeparator();
#endif // #if WITH_MANAGED_CODE

		HelpMenu->Append( IDMENU_HELP_ONLINEHELP, *LocalizeUnrealEd("MainMenu_Help_OnlineHelp"), *LocalizeUnrealEd("MainMenu_Help_OnlineHelp_ToolTip") );
		HelpMenu->Append( IDMENU_HELP_SEARCHUDN, *LocalizeUnrealEd("MainMenu_Help_SearchUDN"), *LocalizeUnrealEd("MainMenu_Help_SearchUDN_ToolTip") );
		HelpMenu->Append( IDMENU_HELP_UDKFORUMS, *LocalizeUnrealEd("MainMenu_Help_UDKForums"), *LocalizeUnrealEd("MainMenu_Help_UDKForums_ToolTip") );
		HelpMenu->AppendSeparator();

		HelpMenu->Append( IDMENU_HELP_STARTUPTIP, *LocalizeUnrealEd("MainMenu_Help_StartupTip"), *LocalizeUnrealEd("MainMenu_Help_StartupTip_ToolTip") );
#if UDK
		const FString AppName = LocalizeUnrealEd( "UDKTitle" );
#else
		FString GameName = GConfig->GetStr(TEXT("URL"), TEXT("GameName"), GEngineIni);

		const FString AppName = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "UnrealEdTitle_F" ), *GameName ) );
#endif

#if WITH_MANAGED_CODE
		HelpMenu->Append( IDMENU_HELP_ABOUTBOX,  *FString::Printf(LocalizeSecure(LocalizeUnrealEd("MainMenu_Help_AboutUnrealEdE"), *AppName)), *FString::Printf(LocalizeSecure(LocalizeUnrealEd("MainMenu_Help_AboutUnrealEd_ToolTipE"), *AppName)));
#endif // #if WITH_MANAGED_CODE

		Append( HelpMenu, *LocalizeUnrealEd("Help") );
	}

	// MRU list

	MRUFavoritesList = new FMainMRUFavoritesList( MRUMenu, FavoritesMenu, MRUFavoritesCombinedMenu );
}


WxMainMenu::~WxMainMenu()
{
	MRUFavoritesList->WriteToINI();
	delete MRUFavoritesList;
}

/*-----------------------------------------------------------------------------
	WxDlgImportBrush.
-----------------------------------------------------------------------------*/

class WxDlgImportBrush : public wxDialog
{
public:
	WxDlgImportBrush()
	{
		const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_IMPORTBRUSH") );
		check( bSuccess );

		MergeFacesCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_MERGEFACES" ) );
		check( MergeFacesCheck != NULL );
		SolidRadio = (wxRadioButton*)FindWindow( XRCID( "IDRB_SOLID" ) );
		check( SolidRadio != NULL );
		NonSolidRadio = (wxRadioButton*)FindWindow( XRCID( "IDRB_NONSOLID" ) );
		check( NonSolidRadio != NULL );

		SolidRadio->SetValue( 1 );

		FWindowUtil::LoadPosSize( TEXT("DlgImportBrush"), this );
		FLocalizeWindow( this );
	}

	~WxDlgImportBrush()
	{
		FWindowUtil::SavePosSize( TEXT("DlgImportBrush"), this );
	}

	int ShowModal( const FString& InFilename )
	{
		Filename = InFilename;
		return wxDialog::ShowModal();
	}
private:
	using wxDialog::ShowModal;		// Hide parent implementation
public:

private:
	wxCheckBox *MergeFacesCheck;
	wxRadioButton *SolidRadio, *NonSolidRadio;

	FString Filename;

	void OnOK( wxCommandEvent& In )
	{
		GUnrealEd->Exec(*FString::Printf(TEXT("BRUSH IMPORT FILE=\"%s\" MERGE=%d FLAGS=%d"),
						*Filename,
						MergeFacesCheck->GetValue() ? 1 : 0,
						NonSolidRadio->GetValue() ? PF_NotSolid : 0) );

		GWorld->GetBrush()->Brush->BuildBound();

		wxDialog::AcceptAndClose();
	}

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(WxDlgImportBrush, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgImportBrush::OnOK )
END_EVENT_TABLE()

////////////////////////////////////////////////////////////////////////////////////////////

extern class WxUnrealEdApp* GApp;

BEGIN_EVENT_TABLE( WxEditorFrame, wxFrame )

	EVT_ICONIZE( WxEditorFrame::OnIconize )
	EVT_MAXIMIZE( WxEditorFrame::OnMaximize )
	EVT_SIZE( WxEditorFrame::OnSize )
	EVT_CLOSE( WxEditorFrame::OnClose )
	EVT_MOVE( WxEditorFrame::OnMove )
	EVT_ACTIVATE( WxEditorFrame::OnActivate )

	EVT_SPLITTER_SASH_POS_CHANGING( ID_SPLITTERWINDOW, WxEditorFrame::OnSplitterChanging )
	EVT_SPLITTER_SASH_POS_CHANGING( ID_SPLITTERWINDOW+1, WxEditorFrame::OnSplitterChanging )
	EVT_SPLITTER_SASH_POS_CHANGING( ID_SPLITTERWINDOW+2, WxEditorFrame::OnSplitterChanging )
	EVT_SPLITTER_DCLICK( ID_SPLITTERWINDOW, WxEditorFrame::OnSplitterDblClk )
	EVT_SPLITTER_DCLICK( ID_SPLITTERWINDOW+1, WxEditorFrame::OnSplitterDblClk )
	EVT_SPLITTER_DCLICK( ID_SPLITTERWINDOW+2, WxEditorFrame::OnSplitterDblClk )

	EVT_MENU( IDM_NEWMAP, WxEditorFrame::MenuFileNewMap )
	EVT_MENU( IDM_OPEN, WxEditorFrame::MenuFileOpen )
	EVT_MENU( IDM_SAVE, WxEditorFrame::MenuFileSave )
	EVT_MENU( IDM_SAVE_AS, WxEditorFrame::MenuFileSaveAs )
	EVT_MENU( IDM_SAVE_ALL, WxEditorFrame::MenuFileSaveAll )
	EVT_MENU( IDM_SAVE_ALL_WRITABLE, WxEditorFrame::MenuFileSaveAll )
	EVT_MENU( IDM_SAVE_DLG, WxEditorFrame::MenuFileSaveDlg )
	EVT_MENU( IDM_FORCE_SAVE_ALL, WxEditorFrame::MenuFileForceSaveAll )
	EVT_MENU( IDM_SAVE_ALL_LEVELS, WxEditorFrame::MenuFileSaveAllLevels )
	EVT_MENU_RANGE( IDM_EDITOR_PREFERENCES_RENDER_MODE_MENU, IDM_EDITOR_PREFERENCES_RENDER_MODE_MENU_END, WxEditorFrame::MenuSelectEditorMode )
	EVT_UPDATE_UI_RANGE( IDM_EDITOR_PREFERENCES_RENDER_MODE_MENU, IDM_EDITOR_PREFERENCES_RENDER_MODE_MENU_END, WxEditorFrame::UI_MenuSelectEditorMode )
	EVT_MENU( IDM_IMPORT_NEW, WxEditorFrame::MenuFileImportNew )
	EVT_MENU( IDM_IMPORT_MERGE, WxEditorFrame::MenuFileImportMerge )
	EVT_MENU( IDM_EXPORT_ALL, WxEditorFrame::MenuFileExportAll )
	EVT_MENU( IDM_EXPORT_SELECTED, WxEditorFrame::MenuFileExportSelected )
	EVT_MENU( IDM_CREATEARCHETYPE, WxEditorFrame::MenuFileCreateArchetype )
	EVT_MENU( IDM_EXIT, WxEditorFrame::MenuFileExit )
	EVT_MENU( IDM_UNDO, WxEditorFrame::MenuEditUndo )
	EVT_MENU( IDM_REDO, WxEditorFrame::MenuEditRedo )
	EVT_SLIDER( ID_FarPlaneSlider, WxEditorFrame::MenuFarPlaneScrollChanged )
	EVT_COMMAND_SCROLL_CHANGED( ID_FarPlaneSlider, WxEditorFrame::MenuFarPlaneScrollChangeEnd )
	EVT_MENU( ID_EDIT_MOUSE_LOCK, WxEditorFrame::MenuEditMouseLock )
	EVT_MENU( ID_EDIT_SHOW_WIDGET, WxEditorFrame::MenuEditShowWidget )
	EVT_MENU( ID_EDIT_TRANSLATE, WxEditorFrame::MenuEditTranslate )
	EVT_MENU( ID_EDIT_ROTATE, WxEditorFrame::MenuEditRotate )
	EVT_MENU( ID_EDIT_SCALE, WxEditorFrame::MenuEditScale )
	EVT_MENU( ID_EDIT_SCALE_NONUNIFORM, WxEditorFrame::MenuEditScaleNonUniform )
	EVT_COMBOBOX( IDCB_COORD_SYSTEM, WxEditorFrame::CoordSystemSelChanged )
	EVT_MENU( IDM_SEARCH, WxEditorFrame::MenuEditSearch )
	EVT_MENU( IDM_CUT, WxEditorFrame::MenuEditCut )
	EVT_MENU( IDM_COPY, WxEditorFrame::MenuEditCopy )
	EVT_MENU( IDM_PASTE, WxEditorFrame::MenuEditPasteOriginalLocation )
	EVT_MENU( IDM_PASTE_ORIGINAL_LOCATION, WxEditorFrame::MenuEditPasteOriginalLocation )
	EVT_MENU( IDM_PASTE_WORLD_ORIGIN, WxEditorFrame::MenuEditPasteWorldOrigin )
	EVT_MENU( IDM_PASTE_HERE, WxEditorFrame::MenuEditPasteHere )
	EVT_MENU( IDM_DUPLICATE, WxEditorFrame::MenuEditDuplicate )
	EVT_MENU( IDM_DELETE, WxEditorFrame::MenuEditDelete )
	EVT_MENU( IDM_SELECT_NONE, WxEditorFrame::MenuEditSelectNone )
	EVT_MENU( IDM_SELECT_BUILDER_BRUSH, WxEditorFrame::MenuEditSelectBuilderBrush )
	EVT_MENU( IDM_SELECT_ALL, WxEditorFrame::MenuEditSelectAll )
	EVT_MENU( IDM_SELECT_ByProperty, WxEditorFrame::MenuEditSelectByProperty )
	EVT_MENU( IDM_SELECT_INVERT, WxEditorFrame::MenuEditSelectInvert )
	EVT_MENU( IDM_SELECT_POST_PROCESS_VOLUME, WxEditorFrame::MenuEditSelectPostProcessVolume )
	EVT_TOOL( IDM_MainToolBar_SelectTranslucent, WxEditorFrame::Clicked_SelectTranslucent )
	EVT_UPDATE_UI( IDM_MainToolBar_SelectTranslucent, WxEditorFrame::UpdateUI_SelectTranslucent )
	EVT_TOOL( IDM_MainToolBar_PIEVisibleOnly, WxEditorFrame::Clicked_PIEVisibleOnly )
	EVT_UPDATE_UI( IDM_MainToolBar_PIEVisibleOnly, WxEditorFrame::UpdateUI_PIEVisibleOnly )
	EVT_TOOL( IDM_EmulateMobileFeatures, WxEditorFrame::Clicked_EmulateMobileFeatures )
	EVT_UPDATE_UI( IDM_EmulateMobileFeatures, WxEditorFrame::UpdateUI_EmulateMobileFeatures )
	EVT_MENU( IDM_AlwaysOptimizeContentForMobile, WxEditorFrame::Clicked_AlwaysOptimizeContentForMobile )
	EVT_UPDATE_UI( IDM_AlwaysOptimizeContentForMobile, WxEditorFrame::UpdateUI_AlwaysOptimizeContentForMobile )
	EVT_MENU( IDM_TogglePrefabsLocked, WxEditorFrame::MenuTogglePrefabsLocked )
	EVT_MENU( IDM_ToggleGroupsActive, WxEditorFrame::MenuToggleGroupsActive )
	EVT_MENU( IDM_FULLSCREEN, WxEditorFrame::MenuViewFullScreen )
	EVT_MENU( IDM_BRUSHPOLYS, WxEditorFrame::MenuViewBrushPolys )
	EVT_MENU( IDM_DISTRIBUTION_TOGGLE, WxEditorFrame::MenuViewDistributionToggle )
	EVT_MENU( IDM_MaterialQualityToggle, WxEditorFrame::MenuMaterialQualityToggle )
	EVT_MENU( IDM_ToggleLODLocking, WxEditorFrame::MenuViewToggleLODLocking )
	EVT_MENU( ID_ToggleSocketSnapping, WxEditorFrame::MenuToggleSocketSnapping )
	EVT_MENU( ID_ShowSocketNames, WxEditorFrame::MenuToggleSocketNames )
	EVT_MENU( IDM_PSYSLODREALTIME_TOGGLE, WxEditorFrame::MenuViewPSysRealtimeLODToggle )
	EVT_MENU( IDM_PSYSHELPER_TOGGLE, WxEditorFrame::MenuViewPSysHelperToggle )
	EVT_MENU( IDM_ACTOR_PROPERTIES, WxEditorFrame::MenuActorProperties )
	EVT_MENU( IDMENU_ActorPopupProperties, WxEditorFrame::MenuActorProperties )
	EVT_MENU( ID_SyncContentBrowser, WxEditorFrame::MenuSyncContentBrowser )
	EVT_MENU( ID_MakeSelectedActorsLevelCurrent, WxEditorFrame::MenuMakeSelectedActorsLevelCurrent )
	EVT_MENU( ID_MakeSelectedActorsLevelGridVolumeCurrent, WxEditorFrame::MenuMakeSelectedActorsLevelGridVolumeCurrent )
	EVT_MENU( ID_MoveSelectedActorsToCurrentLevel, WxEditorFrame::MenuMoveSelectedActorsToCurrentLevel )
	EVT_MENU( IDM_LevelViewportContext_FindStreamingVolumeLevelsInLevelBrowser, WxEditorFrame::MenuFindStreamingVolumeLevelsInLevelBrowser )
	EVT_MENU( ID_SelectLevelInLevelBrowser, WxEditorFrame::MenuSelectLevelInLevelBrowser )
	EVT_MENU( ID_SelectLevelOnlyInLevelBrowser, WxEditorFrame::MenuSelectLevelOnlyInLevelBrowser )
	EVT_MENU( ID_DeselectLevelInLevelBrowser, WxEditorFrame::MenuDeselectLevelInLevelBrowser )
	EVT_MENU( IDMENU_MakeLevelGridVolumeCurrent, WxEditorFrame::MenuMakeLevelGridVolumeCurrent )
	EVT_MENU( IDMENU_ClearCurrentLevelGridVolume, WxEditorFrame::MenuClearCurrentLevelGridVolume )
	EVT_MENU( IDMENU_FindActorInKismet, WxEditorFrame::MenuActorFindInKismet )
	EVT_MENU( IDM_SURFACE_PROPERTIES, WxEditorFrame::MenuSurfaceProperties )
	EVT_MENU( IDM_WORLD_PROPERTIES, WxEditorFrame::MenuWorldProperties )
	EVT_MENU( IDM_LIGHTINGINFO_RESULTS, WxEditorFrame::MenuLightingResults )
	EVT_MENU( IDM_LIGHTINGINFO_BUILDINFO, WxEditorFrame::MenuLightingBuildInfo )
	EVT_MENU( IDM_LIGHTINGINFO_STATICMESHINFO, WxEditorFrame::MenuLightingStaticMeshInfo )
	EVT_MENU( IDM_LIGHTINGINFO_DENSITY_RENDERINGOPTIONS, WxEditorFrame::MenuLightMapDensityRenderingOptions )
	EVT_MENU( IDM_LIGHTINGINFO_RESOLUTION_RATIOADJUST, WxEditorFrame::MenuLightMapResolutionRatioAdjust )
	EVT_MENU( IDM_LIGHTINGINFO_LIGHTING_TOOLS, WxEditorFrame::MenuLightingTools )
	EVT_MENU( IDM_BRUSH_ADD, WxEditorFrame::MenuBrushCSG )
	EVT_MENU( IDM_BRUSH_SUBTRACT, WxEditorFrame::MenuBrushCSG )
	EVT_MENU( IDM_BRUSH_INTERSECT, WxEditorFrame::MenuBrushCSG )
	EVT_MENU( IDM_BRUSH_DEINTERSECT, WxEditorFrame::MenuBrushCSG )
	EVT_MENU( IDM_BRUSH_ADD_SPECIAL, WxEditorFrame::MenuBrushAddSpecial )
	EVT_MENU( IDM_BuildPlayInEditor, WxEditorFrame::MenuBuildPlayInEditor )
	EVT_MENU( IDM_PlayInActiveViewport, WxEditorFrame::MenuBuildPlayInActiveViewport )
	EVT_MENU_RANGE( IDM_BuildPlayConsole_START, IDM_BuildPlayConsole_END, WxEditorFrame::MenuBuildPlayOnConsole )
	EVT_MENU_RANGE( IDM_ConsoleSpecific_START, IDM_ConsoleSpecific_END, WxEditorFrame::MenuConsoleSpecific )
	EVT_UPDATE_UI_RANGE(IDM_ConsoleSpecific_START, IDM_ConsoleSpecific_END, WxEditorFrame::UpdateUIConsoleSpecific)
	EVT_MENU( IDM_PlayUsingMobilePreview, WxEditorFrame::MenuBuildPlayUsingMobilePreview )
	EVT_MENU_OPEN(WxEditorFrame::OnMenuOpen)

	EVT_MENU( IDM_BUILD_GEOMETRY, WxEditorFrame::MenuBuild )
	EVT_MENU( IDM_BUILD_VISIBLEGEOMETRY, WxEditorFrame::MenuBuild )
	EVT_MENU( IDM_BUILD_LIGHTING, WxEditorFrame::MenuBuild )
	EVT_MENU( IDM_BUILD_AI_PATHS, WxEditorFrame::MenuBuild )
	EVT_MENU( IDM_BUILD_SELECTED_AI_PATHS, WxEditorFrame::MenuBuild )
	EVT_MENU( IDM_BUILD_COVER, WxEditorFrame::MenuBuild )
	EVT_MENU( IDM_BUILD_ALL, WxEditorFrame::MenuBuild )
	EVT_MENU( IDM_BUILD_ALL_SUBMIT, WxEditorFrame::MenuBuildAndSubmit )
	EVT_MENU( IDM_BUILD_ALL_ONLY_SELECTED_PATHS, WxEditorFrame::MenuBuild )

	EVT_MENU_RANGE( IDM_BROWSER_START, IDM_BROWSER_END, WxEditorFrame::MenuViewShowBrowser )
	EVT_MENU_RANGE( IDM_MRU_START, IDM_MRU_END, WxEditorFrame::MenuFileMRU )
	EVT_MENU_RANGE( IDM_MainToolBar_MatineeListItem_Start, IDM_MainToolBar_MatineeListItem_End, WxEditorFrame::OnMatineeListMenuItem )
	EVT_MENU_RANGE( IDM_VIEWPORT_CONFIG_START, IDM_VIEWPORT_CONFIG_END, WxEditorFrame::MenuViewportConfig )
	EVT_MENU_RANGE( IDM_OpenNewFloatingViewport_Start, IDM_OpenNewFloatingViewport_End, WxEditorFrame::MenuOpenNewFloatingViewport )
	EVT_MENU_RANGE( IDM_DRAG_GRID_START, IDM_DRAG_GRID_END, WxEditorFrame::MenuDragGrid )
	EVT_MENU_RANGE( ID_BackdropPopupGrid1, ID_BackdropPopupGrid1024, WxEditorFrame::MenuDragGrid )
	EVT_MENU_RANGE( IDM_ROTATION_GRID_START, IDM_ROTATION_GRID_END, WxEditorFrame::MenuRotationGrid )
	EVT_MENU_RANGE( IDM_ANGLESNAPTYPE_START, IDM_ANGLESNAPTYPE_END, WxEditorFrame::MenuAngleSnapType )
	EVT_MENU_RANGE( IDM_AUTOSAVE_START, IDM_AUTOSAVE_END, WxEditorFrame::MenuAutoSaveOptions )
	EVT_MENU_RANGE( IDM_SCALE_GRID_START, IDM_SCALE_GRID_END, WxEditorFrame::MenuScaleGrid )
	EVT_MENU( IDM_VIEW_DETAILMODE_LOW, WxEditorFrame::MenuViewDetailModeLow )
	EVT_MENU( IDM_VIEW_DETAILMODE_MEDIUM, WxEditorFrame::MenuViewDetailModeMedium )
	EVT_MENU( IDM_VIEW_DETAILMODE_HIGH, WxEditorFrame::MenuViewDetailModeHigh )
	EVT_MENU( IDM_OPEN_KISMET, WxEditorFrame::MenuOpenKismet )
	EVT_MENU( IDM_MainMenu_OpenMatinee, WxEditorFrame::MenuOpenMatinee )

	EVT_MENU_RANGE( IDM_MainMenu_FlightCameraOptionsStart, IDM_MainMenu_FlightCameraOptionsEnd, WxEditorFrame::MenuAllowFlightCameraToRemapKeys )
	EVT_MENU( IDM_MainMenu_AutoRestartReimportedFlashMovies, WxEditorFrame::MenuAutoRestartReimportedFlashMovies )
	EVT_MENU( IDM_VIEWPORT_RESIZE_TOGETHER, WxEditorFrame::MenuViewportResizeTogether )
	EVT_MENU( IDM_CENTER_ZOOM_AROUND_CURSOR, WxEditorFrame::MenuCenterZoomAroundCursor)
	EVT_MENU( IDM_PAN_MOVES_CANVAS, WxEditorFrame::MenuPanMovesCanvas)
	EVT_MENU( IDM_REPLACE_RESPECTS_SCALE, WxEditorFrame::MenuReplaceRespectsScale)
	EVT_MENU( IDM_USE_WIREFRAME_HALOS, WxEditorFrame::MenuWireframeHalos)
	EVT_MENU( IDM_MainMenu_DefaultToRealtimeMode, WxEditorFrame::MenuDefaultToRealtimeMode)
	EVT_MENU( IDM_MainMenu_WidgetSettingsUseAbsoluteTranslation, WxEditorFrame::MenuToggleAbsoluteTranslation)
	EVT_MENU( IDM_MainMenu_UseTranslateRotateZWidget, WxEditorFrame::MenuToggleTranslateRotateZWidget)
	EVT_MENU( IDM_MainMenu_ViewportHoverFeedback, WxEditorFrame::MenuViewportHoverFeedback)
	EVT_MENU( IDM_MainMenu_HighlightWithBrackets, WxEditorFrame::MenuHighlightWithBrackets)
	EVT_MENU( IDM_CLICK_BSP_SELECTS_BRUSH, WxEditorFrame::MenuClickBSPSelectsBrush)
	EVT_MENU( IDM_BSP_AUTO_UPDATE, WxEditorFrame::MenuBSPAutoUpdate)
	
	EVT_MENU( IDM_MainMenu_UseStrictBoxSelection, WxEditorFrame::MenuUseStrictBoxSelection )
	EVT_MENU( IDM_MainMenu_PromptSCCOnPackageModification, WxEditorFrame::MenuPromptSCCOnPackageModification )
	EVT_BUTTON( IDM_MainMenu_UseStrictBoxSelection, WxEditorFrame::MenuUseStrictBoxSelection )
	EVT_MENU( IDM_MainMenu_AutoReimportTextures, WxEditorFrame::MenuAutoReimportTextures )
	EVT_MENU_RANGE( IDM_MainMenu_LanguageOptionStart, IDM_MainMenu_LanguageOptionEnd, WxEditorFrame::MenuLanguageSelection )
	EVT_MENU( IDM_MainMenu_ToggleLinkedOrthographicViewports, WxEditorFrame::MenuToggleLinkedOrthographicViewports )
	EVT_MENU_RANGE( IDM_MainMenu_AspectRatioAxisConstraint_Start, IDM_MainMenu_AspectRatioAxisConstraint_End, WxEditorFrame::MenuAspectRatioSelection )
	EVT_MENU( IDM_MainMenu_ViewportCameraToUpdateFromPIV, WxEditorFrame::MenuToggleViewportCameraToUpdateFromPIV )
	EVT_MENU( IDM_MainMenu_ResetSuppressibleDialogs, WxEditorFrame::MenuResetSuppressibleDialogs )
	EVT_MENU( IDM_SummonUnitTestDialog, WxEditorFrame::MenuRunUnitTests )

	EVT_MENU( IDM_MainMenu_LoadSimpleLevelAtStartup, WxEditorFrame::MenuLoadSimpleLevelAtStartup )

#if !UDK
	EVT_MENU( IDM_OPEN_SENTINEL, WxEditorFrame::MenuOpenSentinel )
#endif	//#if !UDK
	EVT_MENU( WM_REDRAWALLVIEWPORTS, WxEditorFrame::MenuRedrawAllViewports )
	EVT_MENU( IDMN_ALIGN_WALL, WxEditorFrame::MenuAlignWall )
	EVT_MENU( IDMN_TOOL_CHECK_ERRORS, WxEditorFrame::MenuToolCheckErrors )
	EVT_MENU( ID_ReviewPaths, WxEditorFrame::MenuReviewPaths )
	EVT_MENU( ID_RotateActors, WxEditorFrame::MenuRotateActors )
	EVT_MENU( ID_ResetParticleEmitters, WxEditorFrame::MenuResetParticleEmitters )
	EVT_MENU( ID_EditSelectAllSurfs, WxEditorFrame::MenuSelectAllSurfs )
	EVT_MENU( ID_BrushAdd, WxEditorFrame::MenuBrushAdd )
	EVT_MENU( ID_BrushSubtract, WxEditorFrame::MenuBrushSubtract )
	EVT_MENU( ID_BrushIntersect, WxEditorFrame::MenuBrushIntersect )
	EVT_MENU( ID_BrushDeintersect, WxEditorFrame::MenuBrushDeintersect )
	EVT_MENU( ID_BrushAddSpecial, WxEditorFrame::MenuBrushAddSpecial )
	EVT_MENU( ID_BrushOpen, WxEditorFrame::MenuBrushOpen )
	EVT_MENU( ID_BrushSaveAs, WxEditorFrame::MenuBrushSaveAs )
	EVT_MENU( ID_BRUSH_IMPORT, WxEditorFrame::MenuBrushImport )
	EVT_MENU( ID_BRUSH_EXPORT, WxEditorFrame::MenuBrushExport )
	EVT_MENU( IDM_WIZARD_NEW_TERRAIN, WxEditorFrame::MenuWizardNewTerrain )	
	EVT_MENU( IDM_REPLACESKELMESHACTORS, WxEditorFrame::MenuReplaceSkelMeshActors )	
	EVT_MENU( IDM_REGENALLPROCBUILDINGS,  WxEditorFrame::MenuRegenAllProcBuildings )	
	EVT_MENU( IDM_REGENSELPROCBUILDINGS,  WxEditorFrame::MenuRegenSelProcBuildings )	
	EVT_MENU( IDM_GENALLPROCBUILDINGLODTEX,  WxEditorFrame::MenuGenAllProcBuildingLODTex )	
	EVT_MENU( IDM_GENSELPROCBUILDINGLODTEX,  WxEditorFrame::MenuGenSelProcBuildingLODTex )	
	EVT_MENU( IDM_LOCKREADONLYLEVELS, WxEditorFrame::MenuLockReadOnlyLevels )
	EVT_MENU( IDM_SETFILELISTENERS, WxEditorFrame::MenuSetFileListeners )
	EVT_MENU( IDM_JOURNALUPDATE, WxEditorFrame::MenuJournalUpdate )
	EVT_MENU( IDM_CleanBSPMaterials, WxEditorFrame::MenuCleanBSPMaterials )

	EVT_MENU( IDMENU_HELP_ABOUTBOX, WxEditorFrame::Menu_Help_About )
	EVT_MENU( IDMENU_HELP_UDKFORUMS, WxEditorFrame::Menu_Help_UDKForums )
	EVT_MENU( IDMENU_HELP_ONLINEHELP, WxEditorFrame::Menu_Help_OnlineHelp )
	EVT_MENU( IDMENU_HELP_SEARCHUDN, WxEditorFrame::Menu_Help_SearchUDN )
	EVT_MENU( IDMENU_HELP_STARTUPTIP, WxEditorFrame::Menu_Help_StartupTip )
	EVT_MENU( IDMENU_HELP_WELCOMESCREEN, WxEditorFrame::Menu_Help_WelcomeScreen )

	EVT_MENU( ID_BackdropPopupAddClassHere, WxEditorFrame::MenuBackdropPopupAddClassHere )
	EVT_MENU( ID_BackdropPopupReplaceWithClass, WxEditorFrame::MenuBackdropPopupReplaceWithClass )
	EVT_MENU_RANGE( ID_BackdropPopupAddLastSelectedClassHere_START, ID_BackdropPopupAddLastSelectedClassHere_END, WxEditorFrame::MenuBackdropPopupAddLastSelectedClassHere )
	EVT_MENU( IDM_BackDropPopupPlayFromHereInEditor, WxEditorFrame::MenuPlayFromHereInEditor )
	EVT_MENU( IDM_BackDropPopupForcePlayFromHereInEditor, WxEditorFrame::MenuPlayFromHereInEditor )
	EVT_MENU( IDM_BackDropPopupPlayFromHereInEditorViewport, WxEditorFrame::MenuPlayFromHereInEditorViewport )
	EVT_MENU_RANGE( IDM_BackDropPopupPlayFromHereConsole_START, IDM_BackDropPopupPlayFromHereConsole_END, WxEditorFrame::MenuPlayFromHereOnConsole )
	EVT_MENU( IDM_BackDropPopupPlayFromHereUsingMobilePreview, WxEditorFrame::MenuPlayFromHereUsingMobilePreview )
	EVT_COMBOBOX( IDCB_ObjectPropagation, WxEditorFrame::ObjectPropagationSelChanged )
	EVT_MENU( IDM_PUSHVIEW_StartStop, WxEditorFrame::PushViewStartStop )
	EVT_MENU( IDM_PUSHVIEW_SYNC, WxEditorFrame::PushViewSync )
	EVT_MENU( ID_SurfPopupApplyMaterial, WxEditorFrame::MenuSurfPopupApplyMaterial )
	EVT_MENU( ID_SurfPopupAlignPlanarAuto, WxEditorFrame::MenuSurfPopupAlignPlanarAuto )
	EVT_MENU( ID_SurfPopupAlignPlanarWall, WxEditorFrame::MenuSurfPopupAlignPlanarWall )
	EVT_MENU( ID_SurfPopupAlignPlanarFloor, WxEditorFrame::MenuSurfPopupAlignPlanarFloor )
	EVT_MENU( ID_SurfPopupAlignBox, WxEditorFrame::MenuSurfPopupAlignBox )
	EVT_MENU( ID_SurfPopupAlignFit, WxEditorFrame::MenuSurfPopupAlignFit )
	EVT_MENU( ID_SurfPopupUnalign, WxEditorFrame::MenuSurfPopupUnalign )
	EVT_MENU( ID_SurfPopupSelectMatchingGroups, WxEditorFrame::MenuSurfPopupSelectMatchingGroups )
	EVT_MENU( ID_SurfPopupSelectMatchingItems, WxEditorFrame::MenuSurfPopupSelectMatchingItems )
	EVT_MENU( ID_SurfPopupSelectMatchingBrush, WxEditorFrame::MenuSurfPopupSelectMatchingBrush )
	EVT_MENU( ID_SurfPopupSelectMatchingTexture, WxEditorFrame::MenuSurfPopupSelectMatchingTexture )
	EVT_MENU( ID_SurfPopupSelectMatchingResolution, WxEditorFrame::MenuSurfPopupSelectMatchingResolution )
	EVT_MENU( ID_SurfPopupSelectMatchingResolutionCurrentLevel, WxEditorFrame::MenuSurfPopupSelectMatchingResolution )
	EVT_MENU( ID_SurfPopupSelectAllAdjacents, WxEditorFrame::MenuSurfPopupSelectAllAdjacents )
	EVT_MENU( ID_SurfPopupSelectAdjacentCoplanars, WxEditorFrame::MenuSurfPopupSelectAdjacentCoplanars )
	EVT_MENU( ID_SurfPopupSelectAdjacentWalls, WxEditorFrame::MenuSurfPopupSelectAdjacentWalls )
	EVT_MENU( ID_SurfPopupSelectAdjacentFloors, WxEditorFrame::MenuSurfPopupSelectAdjacentFloors )
	EVT_MENU( ID_SurfPopupSelectAdjacentSlants, WxEditorFrame::MenuSurfPopupSelectAdjacentSlants )
	EVT_MENU( ID_SurfPopupSelectReverse, WxEditorFrame::MenuSurfPopupSelectReverse )
	EVT_MENU( ID_SurfPopupMemorize, WxEditorFrame::MenuSurfPopupSelectMemorize )
	EVT_MENU( ID_SurfPopupRecall, WxEditorFrame::MenuSurfPopupRecall )
	EVT_MENU( ID_SurfPopupOr, WxEditorFrame::MenuSurfPopupOr )
	EVT_MENU( ID_SurfPopupAnd, WxEditorFrame::MenuSurfPopupAnd )
	EVT_MENU( ID_SurfPopupXor, WxEditorFrame::MenuSurfPopupXor )
	EVT_MENU( IDMENU_BlockingVolumeBBox, WxEditorFrame::MenuBlockingVolumeBBox )
	EVT_MENU( IDMENU_BlockingVolumeConvexVolumeHeavy, WxEditorFrame::MenuBlockingVolumeConvexVolumeHeavy )
	EVT_MENU( IDMENU_BlockingVolumeConvexVolumeNormal, WxEditorFrame::MenuBlockingVolumeConvexVolumeNormal )
	EVT_MENU( IDMENU_BlockingVolumeConvexVolumeLight, WxEditorFrame::MenuBlockingVolumeConvexVolumeLight )
	EVT_MENU( IDMENU_BlockingVolumeConvexVolumeRough, WxEditorFrame::MenuBlockingVolumeConvexVolumeRough )
	EVT_MENU( IDMENU_BlockingVolumeColumnX, WxEditorFrame::MenuBlockingVolumeColumnX )
	EVT_MENU( IDMENU_BlockingVolumeColumnY, WxEditorFrame::MenuBlockingVolumeColumnY )
	EVT_MENU( IDMENU_BlockingVolumeColumnZ, WxEditorFrame::MenuBlockingVolumeColumnZ )
	EVT_MENU( IDMENU_BlockingVolumeAutoConvex, WxEditorFrame::MenuBlockingVolumeAutoConvex )
	EVT_MENU( IDMENU_ActorPopupSelectAllClass, WxEditorFrame::MenuActorPopupSelectAllClass )
	EVT_MENU( IDMENU_ActorPopupSelectAllClassWithArchetype, WxEditorFrame::MenuActorPopupSelectAllClassWithArchetype )
	EVT_MENU( IDMENU_ActorPopupSelectAllBased, WxEditorFrame::MenuActorPopupSelectAllBased )
	EVT_MENU( IDMENU_ActorPopupSelectMatchingProcBuildingsByRuleset, WxEditorFrame::MenuActorPopupSelectMatchingProcBuildingsByRuleset)
	EVT_MENU( IDMENU_ActorPopupSelectMatchingStaticMeshesThisClass, WxEditorFrame::MenuActorPopupSelectMatchingStaticMeshesThisClass )
	EVT_MENU( IDMENU_ActorPopupSelectMatchingStaticMeshesAllClasses, WxEditorFrame::MenuActorPopupSelectMatchingStaticMeshesAllClasses )
	EVT_MENU( IDMENU_ActorPopupSelectMatchingSkeletalMeshesThisClass, WxEditorFrame::MenuActorPopupSelectMatchingSkeletalMeshesThisClass )
	EVT_MENU( IDMENU_ActorPopupSelectMatchingSkeletalMeshesAllClasses, WxEditorFrame::MenuActorPopupSelectMatchingSkeletalMeshesAllClasses )
	EVT_MENU( IDMENU_ActorPopupSelectAllWithMatchingMaterial, WxEditorFrame::MenuActorPopupSelectAllWithMatchingMaterial )
	EVT_MENU( IDMENU_ActorPopupSelectMatchingEmitter, WxEditorFrame::MenuActorPopupSelectMatchingEmitter )
	EVT_MENU( IDMENU_ActorPopupToggleDynamicChannel, WxEditorFrame::MenuActorPopupToggleDynamicChannel )
	EVT_MENU( IDMENU_ActorPopupSelectAllRendered, WxEditorFrame::MenuActorPopupSelectAllRendered )
	EVT_MENU( IDMENU_ActorPopupSelectAllLights, WxEditorFrame::MenuActorPopupSelectAllLights )
	EVT_MENU( IDMENU_ActorPopupSelectAllLightsWithSameClassification, WxEditorFrame::MenuActorPopupSelectAllLightsWithSameClassification )
	EVT_MENU( IDMENU_ActorPopupSelectKismetReferenced, WxEditorFrame::MenuActorPopupSelectKismetReferenced )
	EVT_MENU( IDMENU_ActorPopupSelectKismetUnreferenced, WxEditorFrame::MenuActorPopupSelectKismetUnreferenced )
	EVT_MENU( IDMENU_ActorPopupSelectKismetReferencedAll, WxEditorFrame::MenuActorPopupSelectKismetReferencedAll )
	EVT_MENU( IDMENU_ActorPopupSelectKismetUnreferencedAll, WxEditorFrame::MenuActorPopupSelectKismetUnreferencedAll )
	EVT_MENU( IDMENU_ActorPopupSelectMatchingSpeedTrees, WxEditorFrame::MenuActorPopupSelectMatchingSpeedTrees )
	EVT_MENU( IDMENU_ActorPopupAlignCameras, WxEditorFrame::MenuActorPopupAlignCameras )
	EVT_MENU( IDMENU_ActorPopupLockMovement, WxEditorFrame::MenuActorPopupLockMovement )
	EVT_MENU( IDMENU_ActorPopupSnapViewToActor, WxEditorFrame::MenuActorPopupSnapViewToActor )
	EVT_MENU( IDMENU_ActorPopupMerge, WxEditorFrame::MenuActorPopupMerge )
	EVT_MENU( IDMENU_ActorPopupSeparate, WxEditorFrame::MenuActorPopupSeparate )
	EVT_MENU( IDMENU_ActorPopupToFirst, WxEditorFrame::MenuActorPopupToFirst )
	EVT_MENU( IDMENU_ActorPopupToLast, WxEditorFrame::MenuActorPopupToLast )
	EVT_MENU( IDMENU_ActorPopupToBrush, WxEditorFrame::MenuActorPopupToBrush )
	EVT_MENU( IDMENU_ActorPopupFromBrush, WxEditorFrame::MenuActorPopupFromBrush )
	EVT_MENU( IDMENU_ActorPopupMakeAdd, WxEditorFrame::MenuActorPopupMakeAdd )
	EVT_MENU( IDMENU_ActorPopupMakeSubtract, WxEditorFrame::MenuActorPopupMakeSubtract )
	EVT_MENU( IDMENU_ActorPopupPathPosition, WxEditorFrame::MenuActorPopupPathPosition )
	EVT_MENU( IDMENU_ActorPopupPathProscribe, WxEditorFrame::MenuActorPopupPathProscribe )
	EVT_MENU( IDMENU_ActorPopupPathForce, WxEditorFrame::MenuActorPopupPathForce )
	EVT_MENU( IDMENU_ActorPopupPathOverwriteRoute, WxEditorFrame::MenuActorPopupPathAssignWayPointsToRoute )
	EVT_MENU( IDMENU_ActorPopupPathAddRoute, WxEditorFrame::MenuActorPopupPathAssignWayPointsToRoute )
	EVT_MENU( IDMENU_ActorPopupPathRemoveRoute, WxEditorFrame::MenuActorPopupPathAssignWayPointsToRoute )
	EVT_MENU( IDMENU_ActorPopupPathClearRoute, WxEditorFrame::MenuActorPopupPathAssignWayPointsToRoute )
	EVT_MENU( IDMENU_ActorPopupPathSelectRoute, WxEditorFrame::MenuActorPopupPathSelectWayPointsInRoute)
	EVT_MENU( IDMENU_ActorPopupPathOverwriteCoverGroup, WxEditorFrame::MenuActorPopupPathAssignLinksToCoverGroup )
	EVT_MENU( IDMENU_ActorPopupPathAddCoverGroup, WxEditorFrame::MenuActorPopupPathAssignLinksToCoverGroup )
	EVT_MENU( IDMENU_ActorPopupPathRemoveCoverGroup, WxEditorFrame::MenuActorPopupPathAssignLinksToCoverGroup )
	EVT_MENU( IDMENU_ActorPopupPathClearCoverGroup, WxEditorFrame::MenuActorPopupPathAssignLinksToCoverGroup )
	EVT_MENU( IDMENU_ActorPopupPathClearProscribed, WxEditorFrame::MenuActorPopupPathClearProscribed )
	EVT_MENU( IDMENU_ActorPopupPathClearForced, WxEditorFrame::MenuActorPopupPathClearForced )
	EVT_MENU( IDMENU_ActorPopupPathStitchCover, WxEditorFrame::MenuActorPopupPathStitchCover )
	EVT_MENU( IDMENU_ActorPopupLinkCrowdDestinations, WxEditorFrame::MenuActorPopupLinkCrowdDestinations )
	EVT_MENU( IDMENU_ActorPopupUnlinkCrowdDestinations, WxEditorFrame::MenuActorPopupUnlinkCrowdDestinations )
	EVT_MENU( IDMENU_SnapToFloor, WxEditorFrame::MenuSnapToFloor )
	EVT_MENU( IDMENU_AlignToFloor, WxEditorFrame::MenuAlignToFloor )
	EVT_MENU( IDMENU_SnapPivotToFloor, WxEditorFrame::MenuSnapPivotToFloor )
	EVT_MENU( IDMENU_AlignPivotToFloor, WxEditorFrame::MenuAlignPivotToFloor )
	EVT_MENU( IDMENU_MoveToGrid, WxEditorFrame::MenuMoveToGrid )
	EVT_MENU( IDMENU_SaveBrushAsCollision, WxEditorFrame::MenuSaveBrushAsCollision )

	EVT_MENU( IDMENU_ActorPopupConvertKActorToStaticMesh, WxEditorFrame::MenuConvertActors )
	EVT_MENU( IDMENU_ActorPopupConvertKActorToMover, WxEditorFrame::MenuConvertActors )
	EVT_MENU( IDMENU_ActorPopupConvertStaticMeshToKActor, WxEditorFrame::MenuConvertActors )
	EVT_MENU( IDMENU_ActorPopupConvertStaticMeshToMover, WxEditorFrame::MenuConvertActors )
	EVT_MENU( IDMENU_ActorPopupConvertStaticMeshToFSMA, WxEditorFrame::MenuConvertActors )
	EVT_MENU( IDMENU_ActorPopupConvertStaticMeshToInteractiveFoliageMesh, WxEditorFrame::MenuConvertActors )
	EVT_MENU( IDMENU_ActorPopupConvertStaticMeshToSMBasedOnExtremeContent, WxEditorFrame::MenuConvertActors )
	EVT_MENU( IDMENU_ActorPopupConvertFSMAToStaticMesh, WxEditorFrame::MenuConvertActors )
	EVT_MENU( IDMENU_ActorPopupConvertInteractiveFoliageMeshToStaticMesh, WxEditorFrame::MenuConvertActors )
	EVT_MENU( IDMENU_ActorPopupConvertMoverToStaticMesh, WxEditorFrame::MenuConvertActors )
	EVT_MENU( IDMENU_ActorPopupConvertMoverToKActor, WxEditorFrame::MenuConvertActors )

	EVT_MENU( IDMENU_SetCollisionBlockAll, WxEditorFrame::MenuSetCollisionBlockAll )
	EVT_MENU( IDMENU_SetCollisionBlockWeapons, WxEditorFrame::MenuSetCollisionBlockWeapons )
	EVT_MENU( IDMENU_SetCollisionBlockNone, WxEditorFrame::MenuSetCollisionBlockNone )
	EVT_MENU( IDMENU_ConvertToBlockingVolume, WxEditorFrame::MenuConvertToBlockingVolume )

#if WITH_SIMPLYGON
	EVT_MENU( IDMENU_ActorPopup_SimplifyMesh, WxEditorFrame::MenuActorSimplifyMesh )
	EVT_MENU( IDMENU_ActorPopup_SimplifySelectedMeshes, WxEditorFrame::MenuActorSimplifySelectedMeshes )
#endif // #if WITH_SIMPLYGON
	EVT_MENU( IDMENU_ActorPopup_ConvertStaticMeshToNavMesh, WxEditorFrame::MenuActorConvertStaticMeshToNavMesh )

	EVT_MENU( IDMENU_ActorPopup_SetLODParent, WxEditorFrame::MenuActorSetLODParent )
	EVT_MENU( IDMENU_ActorPopup_AddToLODParent, WxEditorFrame::MenuActorAddToLODParent )
	EVT_MENU( IDMENU_ActorPopup_RemoveFromLODParent, WxEditorFrame::MenuActorRemoveFromLODParent )

	EVT_MENU( IDMENU_IDMENU_ActorPopupConvertLightToLightDynamicAffecting, WxEditorFrame::MenuSetLightDataBasedOnClassification )
	EVT_MENU( IDMENU_IDMENU_ActorPopupConvertLightToLightStaticAffecting, WxEditorFrame::MenuSetLightDataBasedOnClassification )
	EVT_MENU( IDMENU_IDMENU_ActorPopupConvertLightToLightDynamicAndStaticAffecting, WxEditorFrame::MenuSetLightDataBasedOnClassification )

	EVT_MENU_RANGE( IDMENU_ConvertLights_START, IDMENU_ConvertLights_END, WxEditorFrame::MenuConvertLights )
	EVT_MENU_RANGE( IDM_ConvertVolumeClasses_START, IDM_ConvertVolumeClasses_END, WxEditorFrame::MenuConvertVolumes )
	EVT_MENU_RANGE( IDMENU_ConvertSkelMesh_START, IDMENU_ConvertSkelMesh_END, WxEditorFrame::MenuConvertSkeletalMeshes )

	EVT_MENU( IDMENU_SplinePopupBreakAllLinks, WxEditorFrame::MenuSplineBreakAll )
	EVT_MENU( IDMENU_SplinePopupConnect, WxEditorFrame::MenuSplineConnect )
	EVT_MENU( IDMENU_SplinePopupBreak, WxEditorFrame::MenuSplineBreak )
	EVT_MENU( IDMENU_SplinePopupReverseAllDirections, WxEditorFrame::MenuSplineReverseAllDirections )
	EVT_MENU( IDMENU_SplinePopupSetStraightTangents, WxEditorFrame::MenuSplineStraightTangents )
	EVT_MENU( IDMENU_SplinePopupSelectAllNodes, WxEditorFrame::MenuSplineSelectAllNodes )
	EVT_MENU( IDMENU_SplinePopupTestRoute, WxEditorFrame::MenuSplineTestRoute )

	EVT_MENU_RANGE( IDMENU_ApplyRulesetVariationToFace_START, IDMENU_ApplyRulesetVariationToFace_END, WxEditorFrame::ApplyRulesetVariationToFace )
	EVT_MENU_RANGE( IDMENU_ChoosePBSwatch_START, IDMENU_ChoosePBSwatch_END, WxEditorFrame::ApplyParamSwatchToBuilding )
	EVT_MENU( IDMENU_ApplySelectedMaterialToPBFace, WxEditorFrame::ApplySelectedMaterialToPBFace )	
	EVT_MENU( IDMENU_ClearFaceRulesetVariations, WxEditorFrame::ClearFaceRulesetVariations )
	EVT_MENU( IDMENU_ClearPBFaceMaterials, WxEditorFrame::ClearPBFaceMaterials )
	EVT_MENU( IDMENU_ProcBuildingResourceInfo, WxEditorFrame::ProcBuildingResourceInfo )
	EVT_MENU( IDMENU_SelectBaseBuilding,  WxEditorFrame::SelectBaseBuilding )
	EVT_MENU( IDMENU_GroupSelectedBuildings,  WxEditorFrame::GroupSelectedBuildings )
	EVT_MENU( IDMENU_ImageReflectionSceneCapture, WxEditorFrame::UpdateImageReflectionSceneCapture )

	EVT_MENU( IDMENU_SnapOriginToGrid, WxEditorFrame::MenuSnapOriginToGrid )
	EVT_MENU( IDMENU_QuantizeVertices, WxEditorFrame::MenuQuantizeVertices )
	EVT_MENU( IDMENU_ConvertToStaticMesh, WxEditorFrame::MenuConvertToStaticMesh )
	EVT_MENU( IDMENU_ConvertToProcBuilding, WxEditorFrame::MenuConvertToProcBuilding )
	EVT_MENU( IDMENU_ActorPopupBakePrePivot, WxEditorFrame::MenuActorBakePrePivot )
	EVT_MENU( IDMENU_ActorPopupUnBakePrePivot, WxEditorFrame::MenuActorUnBakePrePivot )
	EVT_MENU( IDMENU_ActorPopupResetPivot, WxEditorFrame::MenuActorPivotReset )
	EVT_MENU( IDM_SELECT_SHOW, WxEditorFrame::MenuActorSelectShow )
	EVT_MENU( IDM_SELECT_SHOW_AT_STARTUP, WxEditorFrame::MenuActorSelectShowAtStartup )
	EVT_MENU( IDM_SELECT_HIDE, WxEditorFrame::MenuActorSelectHide )
	EVT_MENU( IDM_SELECT_HIDE_AT_STARTUP, WxEditorFrame::MenuActorSelectHideAtStartup )
	EVT_MENU( IDM_SELECT_INVERT, WxEditorFrame::MenuActorSelectInvert )
	EVT_MENU( IDM_SELECT_RELEVANT_LIGHTS, WxEditorFrame::MenuActorSelectRelevantLights )
	EVT_MENU( IDM_SELECT_RELEVANT_DOMINANT_LIGHTS, WxEditorFrame::MenuActorSelectRelevantDominantLights )
	EVT_MENU( IDM_SHOW_ALL, WxEditorFrame::MenuActorShowAll )
	EVT_MENU( IDM_SHOW_ALL_AT_STARTUP, WxEditorFrame::MenuActorShowAllAtStartup )
	EVT_MENU( ID_BackdropPopupPivot, WxEditorFrame::MenuActorPivotMoveHere )
	EVT_MENU( ID_BackdropPopupPivotSnapped, WxEditorFrame::MenuActorPivotMoveHereSnapped )
	EVT_MENU( ID_BackdropPopupPivotSnappedCenterSelection, WxEditorFrame::MenuActorPivotMoveCenterOfSelection )
	EVT_MENU_RANGE( IDMENU_ActorFactory_Start, IDMENU_ActorFactory_End, WxEditorFrame::MenuUseActorFactory )
	EVT_MENU_RANGE( IDMENU_ActorFactoryAdv_Start, IDMENU_ActorFactoryAdv_End, WxEditorFrame::MenuUseActorFactoryAdv )
	EVT_MENU_RANGE( IDMENU_ReplaceWithActorFactory_Start, IDMENU_ReplaceWithActorFactory_End, WxEditorFrame::MenuReplaceWithActorFactory )
	EVT_MENU_RANGE( IDMENU_ReplaceWithActorFactoryAdv_Start, IDMENU_ReplaceWithActorFactoryAdv_End, WxEditorFrame::MenuReplaceWithActorFactoryAdv )
	EVT_MENU( IDMENU_ActorFactory_LoadSelectedAsset, WxEditorFrame::LoadSelectedAssetForActorFactory )
	EVT_MENU( IDMENU_ActorPopupMirrorX, WxEditorFrame::MenuActorMirrorX )
	EVT_MENU( IDMENU_ActorPopupMirrorY, WxEditorFrame::MenuActorMirrorY )
	EVT_MENU( IDMENU_ActorPopupMirrorZ, WxEditorFrame::MenuActorMirrorZ )
	EVT_MENU( IDMENU_ActorPopupDetailModeLow, WxEditorFrame::MenuActorSetDetailModeLow )
	EVT_MENU( IDMENU_ActorPopupDetailModeMedium, WxEditorFrame::MenuActorSetDetailModeMedium )
	EVT_MENU( IDMENU_ActorPopupDetailModeHigh, WxEditorFrame::MenuActorSetDetailModeHigh )
	EVT_COMMAND_RANGE( IDM_VolumeClasses_START, IDM_VolumeClasses_END, wxEVT_COMMAND_MENU_SELECTED, WxEditorFrame::OnAddVolumeClass )
	EVT_MENU( IDMENU_ActorPopupMakeSolid, WxEditorFrame::MenuActorPopupMakeSolid )
	EVT_MENU( IDMENU_ActorPopupMakeSemiSolid, WxEditorFrame::MenuActorPopupMakeSemiSolid )
	EVT_MENU( IDMENU_ActorPopupMakeNonSolid, WxEditorFrame::MenuActorPopupMakeNonSolid )
	EVT_MENU( IDMENU_ActorPopupSelectBrushesAdd, WxEditorFrame::MenuActorPopupBrushSelectAdd )
	EVT_MENU( IDMENU_ActorPopupSelectBrushesSubtract, WxEditorFrame::MenuActorPopupBrushSelectSubtract )
	EVT_MENU( IDMENU_ActorPopupSelectBrushesNonsolid, WxEditorFrame::MenuActorPopupBrushSelectNonSolid )
	EVT_MENU( IDMENU_ActorPopupSelectBrushesSemisolid, WxEditorFrame::MenuActorPopupBrushSelectSemiSolid )
	EVT_MENU(IDMENU_EmitterPopupOptionsAutoPopulate, WxEditorFrame::MenuEmitterAutoPopulate)
	EVT_MENU(IDMENU_EmitterPopupOptionsReset, WxEditorFrame::MenuEmitterReset)

	EVT_MENU( IDM_CREATEARCHETYPE, WxEditorFrame::CreateArchetype )
	EVT_MENU( IDM_UPDATEARCHETYPE, WxEditorFrame::UpdateArchetype )
	EVT_MENU( IDM_CREATEPREFAB, WxEditorFrame::CreatePrefab )
	EVT_MENU( IDM_ADDPREFAB, WxEditorFrame::AddPrefab )
	EVT_MENU( IDM_SELECTALLACTORSINPREFAB, WxEditorFrame::SelectPrefabActors )
	EVT_MENU( IDM_UPDATEPREFABFROMINSTANCE, WxEditorFrame::UpdatePrefabFromInstance )
	EVT_MENU( IDM_RESETFROMPREFAB, WxEditorFrame::ResetInstanceFromPrefab )
	EVT_MENU( IDM_CONVERTPREFABTONORMALACTORS, WxEditorFrame::PrefabInstanceToNormalActors )
	EVT_MENU( IDM_OPENPREFABINSTANCESEQUENCE, WxEditorFrame::PrefabInstanceOpenSequence )

	EVT_MENU( IDM_CREATEGROUP, WxEditorFrame::Group )
	EVT_MENU( IDM_REGROUP, WxEditorFrame::Regroup )
	EVT_MENU( IDM_UNGROUP, WxEditorFrame::Ungroup )
	EVT_MENU( IDM_LOCKGROUP, WxEditorFrame::LockGroup )
	EVT_MENU( IDM_UNLOCKGROUP, WxEditorFrame::UnlockGroup )
	EVT_MENU( IDM_ADDTOGROUP, WxEditorFrame::AddToGroup )
	EVT_MENU( IDM_REMOVEFROMGROUP, WxEditorFrame::RemoveFromGroup )
#if ENABLE_SIMPLYGON_MESH_PROXIES
	EVT_MENU( IDM_COMBINEGROUP, WxEditorFrame::MenuActorCreateMeshProxy )
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
	EVT_MENU( IDM_SELECT_SHOWSTATS, WxEditorFrame::ReportStatsForSelection )

	EVT_MENU_RANGE (IDM_SYNC_GENERICBROWSER_TO_MATERIALINTERFACE_START, IDM_SYNC_GENERICBROWSER_TO_MATERIALINTERFACE_END, WxEditorFrame::MenuSyncMaterialInterface)
	EVT_MENU_RANGE (IDM_SYNC_GENERICBROWSER_TO_BASEMATERIAL_START, IDM_SYNC_GENERICBROWSER_TO_BASEMATERIAL_END, WxEditorFrame::MenuSyncMaterialInterface)
	EVT_MENU_RANGE (IDM_EDIT_MATERIALINTERFACE_START, IDM_EDIT_MATERIALINTERFACE_END, WxEditorFrame::MenuEditMaterialInterface)
	EVT_MENU_RANGE (IDM_CREATE_MATERIAL_INSTANCE_CONSTANT_START, IDM_CREATE_MATERIAL_INSTANCE_CONSTANT_END, WxEditorFrame::MenuCreateMaterialInstance)
	EVT_MENU_RANGE (IDM_CREATE_MATERIAL_INSTANCE_TIME_VARYING_START, IDM_CREATE_MATERIAL_INSTANCE_TIME_VARYING_END, WxEditorFrame::MenuCreateMaterialInstance)
	EVT_MENU_RANGE (IDM_ASSIGN_MATERIALINTERFACE_START, IDM_ASSIGN_MATERIALINTERFACE_END, WxEditorFrame::MenuAssignMaterial)
	EVT_MENU(IDM_ASSIGN_MATERIALINTERFACE_MULTIPLEACTORS, WxEditorFrame::MenuAssignMaterialToMultipleActors)
	EVT_MENU_RANGE (IDM_SYNC_TO_TEXTURE_START, IDM_SYNC_TO_TEXTURE_END, WxEditorFrame::MenuSyncTexture )
	EVT_MENU_RANGE (IDM_COPY_MATERIAL_NAME_TO_CLIPBOARD_START, IDM_COPY_MATERIAL_NAME_TO_CLIPBOARD_END, WxEditorFrame::MenuCopyMaterialName )
	EVT_UPDATE_UI( IDM_UNDO, WxEditorFrame::UI_MenuEditUndo )
	EVT_UPDATE_UI( IDM_REDO, WxEditorFrame::UI_MenuEditRedo )
	EVT_UPDATE_UI( ID_EDIT_MOUSE_LOCK, WxEditorFrame::UI_MenuEditMouseLock )
	EVT_UPDATE_UI( ID_EDIT_SHOW_WIDGET, WxEditorFrame::UI_MenuEditShowWidget )
	EVT_UPDATE_UI( ID_EDIT_TRANSLATE, WxEditorFrame::UI_MenuEditTranslate )
	EVT_UPDATE_UI( ID_EDIT_ROTATE, WxEditorFrame::UI_MenuEditRotate )
	EVT_UPDATE_UI( ID_EDIT_SCALE, WxEditorFrame::UI_MenuEditScale )
	EVT_UPDATE_UI( ID_EDIT_SCALE_NONUNIFORM, WxEditorFrame::UI_MenuEditScaleNonUniform )
	EVT_UPDATE_UI( IDM_VIEW_DETAILMODE_LOW, WxEditorFrame::UI_MenuViewDetailModeLow )
	EVT_UPDATE_UI( IDM_VIEW_DETAILMODE_MEDIUM, WxEditorFrame::UI_MenuViewDetailModeMedium )
	EVT_UPDATE_UI( IDM_VIEW_DETAILMODE_HIGH, WxEditorFrame::UI_MenuViewDetailModeHigh )
	EVT_UPDATE_UI_RANGE( IDM_VIEWPORT_CONFIG_START, IDM_VIEWPORT_CONFIG_END, WxEditorFrame::UI_MenuViewportConfig )
	EVT_UPDATE_UI_RANGE( IDM_DRAG_GRID_START, IDM_DRAG_GRID_END, WxEditorFrame::UI_MenuDragGrid )
	EVT_UPDATE_UI_RANGE( ID_BackdropPopupGrid1, ID_BackdropPopupGrid1024, WxEditorFrame::UI_MenuDragGrid )
	EVT_UPDATE_UI_RANGE( IDM_ROTATION_GRID_START, IDM_ROTATION_GRID_END, WxEditorFrame::UI_MenuRotationGrid )
	EVT_UPDATE_UI_RANGE( IDM_ANGLESNAPTYPE_START, IDM_ANGLESNAPTYPE_END, WxEditorFrame::UI_MenuAngleSnapType )
	EVT_UPDATE_UI_RANGE( IDM_SCALE_GRID_START, IDM_SCALE_GRID_END, WxEditorFrame::UI_MenuScaleGrid )
	EVT_UPDATE_UI( IDM_FULLSCREEN, WxEditorFrame::UI_MenuViewFullScreen )
	EVT_UPDATE_UI( IDM_BRUSHPOLYS, WxEditorFrame::UI_MenuViewBrushPolys )
	EVT_UPDATE_UI( ID_FarPlaneSlider, WxEditorFrame::UI_MenuFarPlaneSlider )
	EVT_UPDATE_UI( IDM_TogglePrefabsLocked, WxEditorFrame::UI_MenuTogglePrefabLock )
	EVT_UPDATE_UI( IDM_ToggleGroupsActive, WxEditorFrame::UI_MenuToggleGroupsActive )
	EVT_UPDATE_UI( IDM_DISTRIBUTION_TOGGLE, WxEditorFrame::UI_MenuViewDistributionToggle )
	EVT_UPDATE_UI( IDM_MaterialQualityToggle, WxEditorFrame::UI_MenuMaterialQualityToggle )
	EVT_UPDATE_UI( IDM_ToggleLODLocking, WxEditorFrame::UI_MenuViewToggleLODLocking )
	EVT_UPDATE_UI( ID_ToggleSocketSnapping, WxEditorFrame::UI_MenuToggleSocketSnapping )
	EVT_UPDATE_UI( ID_ShowSocketNames, WxEditorFrame::UI_MenuToggleSocketNames )
	EVT_UPDATE_UI( IDM_PSYSLODREALTIME_TOGGLE, WxEditorFrame::UI_MenuViewPSysLODRealtimeToggle )
	EVT_UPDATE_UI( IDM_PSYSHELPER_TOGGLE, WxEditorFrame::UI_MenuViewPSysHelperToggle )
	EVT_UPDATE_UI( IDM_BUILD_ALL_SUBMIT, WxEditorFrame::UI_MenuBuildAllSubmit )

	EVT_UPDATE_UI_RANGE( IDM_MainMenu_FlightCameraOptionsStart, IDM_MainMenu_FlightCameraOptionsEnd, WxEditorFrame::UI_MenuAllowFlightCameraToRemapKeys )
	EVT_UPDATE_UI( IDM_MainMenu_AutoRestartReimportedFlashMovies, WxEditorFrame::UI_MenuAutoRestartReimportedFlashMovies )
	EVT_UPDATE_UI( IDM_VIEWPORT_RESIZE_TOGETHER, WxEditorFrame::UI_MenuViewResizeViewportsTogether )
	EVT_UPDATE_UI( IDM_CENTER_ZOOM_AROUND_CURSOR, WxEditorFrame::UI_MenuCenterZoomAroundCursor )
	EVT_UPDATE_UI( IDM_PAN_MOVES_CANVAS, WxEditorFrame::UI_MenuPanMovesCanvas )
	EVT_UPDATE_UI( IDM_REPLACE_RESPECTS_SCALE, WxEditorFrame::UI_MenuReplaceRespectsScale )
	EVT_UPDATE_UI( IDM_MainMenu_DefaultToRealtimeMode, WxEditorFrame::UI_MenuDefaultToRealtimeMode )
	EVT_UPDATE_UI( IDM_MainMenu_WidgetSettingsUseAbsoluteTranslation, WxEditorFrame::UI_MenuToggleAbsoluteTranslation)
	EVT_UPDATE_UI( IDM_MainMenu_UseTranslateRotateZWidget, WxEditorFrame::UI_MenuToggleTranslateRotateZWidget)
	EVT_UPDATE_UI( IDM_MainMenu_ViewportHoverFeedback, WxEditorFrame::UI_MenuViewportHoverFeedback )
	EVT_UPDATE_UI( IDM_MainMenu_HighlightWithBrackets, WxEditorFrame::UI_MenuHighlightWithBrackets )
	EVT_UPDATE_UI( IDM_USE_WIREFRAME_HALOS, WxEditorFrame::UI_MenuWireframeHalos)
	EVT_UPDATE_UI( IDM_MainMenu_UseStrictBoxSelection, WxEditorFrame::UI_MenuUseStrictBoxSelection )
	EVT_UPDATE_UI( IDM_MainMenu_PromptSCCOnPackageModification, WxEditorFrame::UI_MenuPromptSCCOnPackageModification )
	EVT_UPDATE_UI( IDM_MainMenu_AutoReimportTextures, WxEditorFrame::UI_MenuAutoReimportTextures )
	EVT_UPDATE_UI_RANGE( IDM_MainMenu_LanguageOptionStart, IDM_MainMenu_LanguageOptionEnd, WxEditorFrame::UI_MenuLanguageSelection )
	EVT_UPDATE_UI_RANGE( IDM_MainMenu_AspectRatioAxisConstraint_Start, IDM_MainMenu_AspectRatioAxisConstraint_End, WxEditorFrame::UI_MenuAspectRatioSelection )
	EVT_UPDATE_UI( IDM_MainMenu_LoadSimpleLevelAtStartup, WxEditorFrame::UI_MenuLoadSimpleLevelAtStartup )
	EVT_UPDATE_UI_RANGE( IDM_AUTOSAVE_START, IDM_AUTOSAVE_END, WxEditorFrame::UI_MenuAutoSaveOptions )
	EVT_UPDATE_UI( IDM_MainMenu_ToggleLinkedOrthographicViewports, WxEditorFrame::UI_MenuToggleLinkedOrthographicViewports )
	EVT_UPDATE_UI( IDM_MainMenu_ViewportCameraToUpdateFromPIV, WxEditorFrame::UI_MenuToggleViewportCameraToUpdateFromPIV )
	EVT_UPDATE_UI( IDM_CLICK_BSP_SELECTS_BRUSH, WxEditorFrame::UI_MenuClickBSPSelectsBrush )
	EVT_UPDATE_UI( IDM_BSP_AUTO_UPDATE, WxEditorFrame::UI_MenuBSPAutoUpdate )

	EVT_UPDATE_UI( ID_MakeSelectedActorsLevelCurrent, WxEditorFrame::UI_ContextMenuMakeCurrentLevel )
	EVT_UPDATE_UI( ID_MakeSelectedActorsLevelGridVolumeCurrent, WxEditorFrame::UI_ContextMenuMakeCurrentLevelGridVolume )

	EVT_DOCKINGCHANGE( WxEditorFrame::OnDockingChange )

	EVT_MENU( IDM_CoverEditMenu_ToggleEnabled, WxEditorFrame::CoverEdit_ToggleEnabled )
	EVT_MENU( IDM_CoverEditMenu_ToggleAutoAdjust, WxEditorFrame::CoverEdit_ToggleAutoAdjust )
	EVT_MENU( IDM_CoverEditMenu_ToggleTypeAutomatic, WxEditorFrame::CoverEdit_ToggleTypeAutomatic )
	EVT_MENU( IDM_CoverEditMenu_ToggleTypeStanding, WxEditorFrame::CoverEdit_ToggleTypeStanding )
	EVT_MENU( IDM_CoverEditMenu_ToggleTypeMidLevel, WxEditorFrame::CoverEdit_ToggleTypeMidLevel )
	EVT_MENU( IDM_CoverEditMenu_ToggleCoverslip, WxEditorFrame::CoverEdit_ToggleCoverslip )
	EVT_MENU( IDM_CoverEditMenu_ToggleSwatTurn, WxEditorFrame::CoverEdit_ToggleSwatTurn )
	EVT_MENU( IDM_CoverEditMenu_ToggleMantle, WxEditorFrame::CoverEdit_ToggleMantle )
	EVT_MENU( IDM_CoverEditMenu_TogglePopup, WxEditorFrame::CoverEdit_TogglePopup )
	EVT_MENU( IDM_CoverEditMenu_ToggleLeanLeft, WxEditorFrame::CoverEdit_ToggleLeanLeft )
	EVT_MENU( IDM_CoverEditMenu_ToggleLeanRight, WxEditorFrame::CoverEdit_ToggleLeanRight )
	EVT_MENU( IDM_CoverEditMenu_ToggleClimbUp, WxEditorFrame::CoverEdit_ToggleClimbUp )
	EVT_MENU( IDM_CoverEditMenu_TogglePreferLean, WxEditorFrame::CoverEdit_TogglePreferLean )	
	EVT_MENU( IDM_CoverEditMenu_TogglePlayerOnly, WxEditorFrame::CoverEdit_TogglePlayerOnly )
	EVT_MENU( IDM_CoverEditMenu_ToggleForceCanPopup, WxEditorFrame::CoverEdit_ToggleForceCanPopup )
	EVT_MENU( IDM_CoverEditMenu_ToggleForceCanCoverslip_Left, WxEditorFrame::CoverEdit_ToggleForceCanCoverslip_Left )
	EVT_MENU( IDM_CoverEditMenu_ToggleForceCanCoverslip_Right, WxEditorFrame::CoverEdit_ToggleForceCanCoverslip_Right )

	EVT_MENU( IDMENU_ActorPoupupUpdateBaseToProcBuilding, WxEditorFrame::MenuUpdateBaseToProcBuilding )

	EVT_MENU( IDMENU_PopupMoveCameraToPoint, WxEditorFrame::MenuMoveCameraToPoint )

#if WITH_FBX
	EVT_MENU( IDMENU_ActorPoupupExportFBX, WxEditorFrame::MenuExportActorToFBX )
#endif

	EVT_TOOL( IDM_REALTIME_AUDIO, WxEditorFrame::OnClickedRealTimeAudio )
	EVT_UPDATE_UI( IDM_REALTIME_AUDIO, WxEditorFrame::UI_RealTimeAudioButton )
	EVT_COMMAND_SCROLL( ID_VOLUME_SLIDER, WxEditorFrame::OnVolumeChanged )

	EVT_TOOL( IDM_FAVORITES_TOGGLE_BUTTON, WxEditorFrame::OnClickedToggleFavorites )
	EVT_MENU_RANGE( IDM_FAVORITES_START, IDM_FAVORITES_END, WxEditorFrame::MenuFileFavorite )
	

END_EVENT_TABLE()

// Used for dynamic creation of the window. This must be declared for any
// subclasses of WxEditorFrame
IMPLEMENT_DYNAMIC_CLASS(WxEditorFrame,wxFrame);

/**
 * Default constructor. Construction is a 2 phase process: class creation
 * followed by a call to Create(). This is required for dynamically determining
 * which editor frame class to create for the editor's main frame.
 */
WxEditorFrame::WxEditorFrame() :
	ViewportContainer( NULL ),
	DlgFolderList( NULL )
{
	MainMenuBar = NULL;
	MainToolBar = NULL;
	DragGridMenu = NULL;
	RotationGridMenu = NULL;
	AutoSaveOptionsMenu = NULL;
	ButtonBar = NULL;
	ViewportConfigData = NULL;
	bViewportResizeTogether = TRUE;
	bShouldUpdateUI = FALSE;

	FramePos.x = -1;
	FramePos.y = -1;
	FrameSize.Set( -1, -1 );
	bFrameMaximized = TRUE;

	// Set default grid sizes, powers of two
	for( INT i = 0 ; i < FEditorConstraints::MAX_GRID_SIZES ; ++i )
	{
		GEditor->Constraints.GridSizes[i] = (float)(1 << i);
	}

}

/**
 * Part 2 of the 2 phase creation process. First it loads the localized caption.
 * Then it creates the window with that caption. And finally finishes the
 * window initialization
 */
void WxEditorFrame::Create()
{
	wxString Caption = *GetLocalizedCaption();

	GConfig->GetInt( TEXT("EditorFrame"), TEXT("FramePos.x"), (INT&)FramePos.x, GEditorUserSettingsIni );
	GConfig->GetInt( TEXT("EditorFrame"), TEXT("FramePos.y"), (INT&)FramePos.y, GEditorUserSettingsIni );
	GConfig->GetInt( TEXT("EditorFrame"), TEXT("FrameSize.x"), (INT&)FrameSize.x, GEditorUserSettingsIni );
	GConfig->GetInt( TEXT("EditorFrame"), TEXT("FrameSize.y"), (INT&)FrameSize.y, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("EditorFrame"), TEXT("ViewportResizeTogether"), bViewportResizeTogether, GEditorUserSettingsIni );

	// Ensure that the window doesn't overlap the windows toolbar
	FWindowUtil::ForceTopLeftPosOffToolbar( FramePos.x, FramePos.y );

	// Assert if this fails
	const bool bSuccess = wxFrame::Create( NULL, -1, Caption, FramePos, FrameSize );
	check( bSuccess );

	for( INT StatusBarIndex = 0; StatusBarIndex < SB_Max; ++StatusBarIndex )
	{
		StatusBars[ StatusBarIndex ] = NULL;
	}

	HWND WindowHnd = (HWND)this->GetHandle();
	if( WindowHnd )
	{
		// Set the window icon
		{
			extern INT GEditorIcon;

			// @todo: Ideally we could set the icon a bit earlier, or adjust the window class's icon before the window is created
			// @todo: Consider using wxIcon and wxFrame::SetIcon here instead (which will propagate icons to child windows)
			HICON EditorIconHandle = LoadIcon( hInstance, MAKEINTRESOURCE( GEditorIcon ) );
			if( EditorIconHandle != NULL )
			{
				::SendMessage( WindowHnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)EditorIconHandle );
				::SendMessage( WindowHnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)EditorIconHandle );
			}
		}

		// Make window foreground.
		SetWindowPos( WindowHnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
		SetWindowPos( WindowHnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
	}
	
	//read constants from the 
	InitWireframeConstants();

	GCallbackEvent->Register(CALLBACK_FileChanged, this);

	//turn on file notifications if we need to 
	SetFileSystemNotifications(GEditor->AccessUserSettings().bAutoReimportTextures, GEditor->AccessUserSettings().bAutoReimportApexAssets);

	// Set help text to appear in the 5th slot of the status bar
	SetStatusBarPane(5);
}

/**
 * Returns the localized caption for this frame window. It looks in the
 * editor's INI file to determine which localization file to use
 */
FString WxEditorFrame::GetLocalizedCaption(FString levelName)
{
	if(levelName == TEXT(""))
	{
		return FString::Printf(TEXT("Duke's Enormous Tool 2004"));
	}

	return FString::Printf(TEXT("Duke's Enormous Tool 2004: %s"), *levelName);
}

WxEditorFrame::~WxEditorFrame()
{
	GCallbackEvent->UnregisterAll(this);

	// Can't use appRequestExit as it uses PostQuitMessage which is not what we want here.
	GIsRequestingExit = 1;

	if( GEditorModeTools().IsModeActive(EM_CoverEdit) ||
		GEditorModeTools().IsModeActive(EM_MeshPaint) ||
		GEditorModeTools().IsModeActive(EM_InterpEdit) )
	{
		GEditorModeTools().ActivateMode( EM_Default );
	}

	// Save the viewport configuration to the INI file
	if( ViewportConfigData )
	{
		ViewportConfigData->SaveToINI();
	}
	delete ViewportConfigData;
	ViewportConfigData = NULL;

	// Save out any config settings for the editor so they don't get lost
	GEditor->SaveConfig();
	GEditorModeTools().SaveConfig();

	// Let the browser manager save its state before destroying it
	GUnrealEd->GetBrowserManager()->SaveState();
	GUnrealEd->GetBrowserManager()->DestroyBrowsers();

	// Save last used windowed size:
	GConfig->SetInt( TEXT("EditorFrame"), TEXT("FramePos.x"), FramePos.x, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("EditorFrame"), TEXT("FramePos.y"), FramePos.y, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("EditorFrame"), TEXT("FrameSize.x"), FrameSize.x, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("EditorFrame"), TEXT("FrameSize.y"), FrameSize.y, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("EditorFrame"), TEXT("FrameMaximized"), bFrameMaximized, GEditorUserSettingsIni );

	GConfig->SetBool( TEXT("EditorFrame"), TEXT("ViewportResizeTogether"), bViewportResizeTogether, GEditorUserSettingsIni );

	//delete MainMenuBar;
	//delete MainToolBar;
	//delete AcceleratorTable;
	//delete DragGridMenu;
	//delete RotationGridMenu;
}

// Gives elements in the UI a chance to update themselves based on internal engine variables.

void WxEditorFrame::UpdateUI()
{
	bShouldUpdateUI = TRUE;
}

void WxEditorFrame::UpdateDirtiedUI()
{	
	if( bShouldUpdateUI )
	{
		// Left side button bar

		if( ButtonBar )
		{
			ButtonBar->UpdateUI();
		}

		// Viewport toolbars

		if( ViewportConfigData )
		{
			for( INT x = 0 ; x < ViewportConfigData->GetViewportCount(); ++x )
			{
				if( ViewportConfigData->GetViewport( x ).bEnabled )
				{
					ViewportConfigData->AccessViewport( x ).ViewportWindow->ToolBar->UpdateUI();
					ViewportConfigData->AccessViewport( x ).ViewportWindow->ResizeToolBar();
				}
			}
		}

		// Status bars

		if( StatusBars[ SB_Standard ] )
		{
			StatusBars[ SB_Standard ]->UpdateUI();
		}

		// Main toolbar

		if( MainToolBar )
		{
			if (MainToolBar->CoordSystemCombo->GetSelection() != GEditorModeTools().CoordSystem)
			{
				MainToolBar->CoordSystemCombo->SetSelection( GEditorModeTools().CoordSystem );
			}
			MainToolBar->UpdateLightingQualityState();
		}

		bShouldUpdateUI = FALSE;
	}

	MenuUpdateUFEProcs();
}

// Creates the child windows for the frame and sets everything to it's initial state.

void WxEditorFrame::SetUp()
{
	// Child windows that control the client area

	ViewportContainer = new WxViewportsContainer( (wxWindow*)this, IDW_VIEWPORT_CONTAINER );
	ViewportContainer->SetLabel( wxT("ViewportContainer") );

	// Menus

	DragGridMenu = new WxDragGridMenu;
	RotationGridMenu = new WxRotationGridMenu;
	AutoSaveOptionsMenu = new WxAutoSaveOptionsMenu;
	ScaleGridMenu = new WxScaleGridMenu;
	PreferencesMenu = new WxPreferencesMenu;

	MatineeListMenu = new wxMenu();
	MatineeListMenuMap.Reset();

	// Load grid settings

	for( INT i = 0 ; i < FEditorConstraints::MAX_GRID_SIZES ; ++i )
	{
		FString Key = FString::Printf( TEXT("GridSize%d"), i );
		GConfig->GetFloat( TEXT("GridSizes"), *Key, GEditor->Constraints.GridSizes[i], GEditorIni );
	}

	// Viewport configuration options

	FViewportConfig_Template* Template;

	Template = new FViewportConfig_Template;
	Template->Set( VC_2_2_Split );
	ViewportConfigTemplates.AddItem( Template );

	Template = new FViewportConfig_Template;
	Template->Set( VC_1_2_Split );
	ViewportConfigTemplates.AddItem( Template );

	Template = new FViewportConfig_Template;
	Template->Set( VC_1_1_SplitH );
	ViewportConfigTemplates.AddItem( Template );

	Template = new FViewportConfig_Template;
	Template->Set( VC_1_1_SplitV );
	ViewportConfigTemplates.AddItem( Template );


	// Browser window initialization
	GUnrealEd->GetBrowserManager()->Initialize();

	// Main UI components

	MainMenuBar = new WxMainMenu;
	SetMenuBar( MainMenuBar );

	MainToolBar = new WxMainToolBar( (wxWindow*)this, -1 );
	SetToolBar( MainToolBar );

	GetMRUFavoritesList()->ReadFromINI();

	// Create all the status bars and set the default one.
	StatusBars[ SB_Standard ] = new WxStatusBarStandard;
	StatusBars[ SB_Standard ]->Create( this, -1 );
	StatusBars[ SB_Standard ]->SetUp();
	SetStatusBarType( SB_Standard );

	UBOOL bShouldBeMaximized = bFrameMaximized;
	GConfig->GetInt( TEXT("EditorFrame"), TEXT("FrameMaximized"), (INT&)bShouldBeMaximized, GEditorUserSettingsIni );
	bFrameMaximized = bShouldBeMaximized;
	if ( bFrameMaximized )
	{
		Maximize();
	}

	// Clean up
	wxSizeEvent DummyEvent;
	OnSize( DummyEvent );
}

// Changes the active status bar

void WxEditorFrame::SetStatusBarType( EStatusBar InStatusBar )
{
	wxFrame::SetStatusBar( StatusBars[ InStatusBar ] );

	if( StatusBars[ InStatusBar ] != NULL )
	{
		// Make all statusbars the same size as the SB_Standard one
		// FIXME : This is a bit of a hack as I suspect there must be a nice way of doing this, but I can't see it right now.
		wxRect rect = StatusBars[ SB_Standard ]->GetRect();
		for( INT x = 0 ; x < SB_Max ; ++x )
		{
			StatusBars[ x ]->SetSize( rect );
		}

		// Hide all status bars, except the active one
		for( INT x = 0 ; x < SB_Max ; ++x )
		{
			StatusBars[ x ]->Show( x == InStatusBar );
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void WxEditorFrame::Send(ECallbackEventType Event)
{
	//@todo RefreshCaption
}

/** Handle global file changed events. */
void WxEditorFrame::Send( ECallbackEventType InType, const FString& InString, UObject* InObject)
{
	if (InType == CALLBACK_FileChanged)
	{
		FFilename FileName = InString;
		FString FileExtension = FileName.GetExtension();

		UBOOL bTextureFileChanged = ( (FileExtension == "tga") || (FileExtension == "dds") || (FileExtension == "png")
			|| (FileExtension == "psd") || (FileExtension == "bmp") );
		UBOOL bApexFileChanged = ((FileExtension == "apb") || (FileExtension == "apx"));

		//extra guard in case of other file listeners sending events
		if ( !(bTextureFileChanged || bApexFileChanged) )
		{
			//not one of the file extensions to listen for.  Reject.
			return;
		}

		//InString is the filename in "Relative Path" form!
		if (bTextureFileChanged && GEditor->AccessUserSettings().bAutoReimportTextures)
		{
			//iterate through all textures in memory and if they match, then reimport them!
			for( TObjectIterator<UTexture> It; It; ++It )
			{
				UTexture* Texture = *It;
				if (Texture->SourceFilePath == InString)
				{
					UPackage* TexturePackage = Texture->GetOutermost();
					check(TexturePackage);

					const INT SCCState = GPackageFileCache->GetSourceControlState(*TexturePackage->GetName());
					//Make sure we have the package checked out first or that package is new and not yet saved.  
					if (( SCCState == SCC_CheckedOut) || (SCCState == SCC_NotInDepot) || (SCCState == SCC_DontCare))
					{
						//to avoid stalling, turn on defer compression by default
						Texture->DeferCompression = TRUE;

						//found a match, reimport!
						FReimportManager::Instance()->Reimport( Texture );

						FString BalloonMessage = FString::Printf( LocalizeSecure(LocalizeUnrealEd("EditorFrame_AutoTextureReimport"), *Texture->GetName(), *InString) );
						//send notification that we did some work
						FShowBalloonNotification::ShowNotification( LocalizeUnrealEd("EditorFrame_AutoTextureReimport_Alert"), BalloonMessage, ID_BALLOON_NOTIFY_ID );
					}
				}
			}
		} 
		else if (bApexFileChanged && GEditor->AccessUserSettings().bAutoReimportApexAssets)
		{
			//iterate through all textures in memory and if they match, then reimport them!
			for( TObjectIterator<UApexAsset> It; It; ++It )
			{
				UApexAsset* ApexAsset = *It;
				if (ApexAsset->SourceFilePath == InString)
				{
					UPackage* ApexPackage = ApexAsset->GetOutermost();
					check(ApexPackage);

					const INT SCCState = GPackageFileCache->GetSourceControlState(*ApexPackage->GetName());
					//Make sure we have the package checked out first or that package is new and not yet saved.  
					if (( SCCState == SCC_CheckedOut) || (SCCState == SCC_NotInDepot) || (SCCState == SCC_DontCare))
					{
						//found a match, reimport!
						FReimportManager::Instance()->Reimport( ApexPackage );

						FString BalloonMessage = FString::Printf(TEXT("%s reimported from %s"), *ApexAsset->GetName(), *InString);

						//send notification that we did some work
						FShowBalloonNotification::ShowNotification( LocalizeUnrealEd("EditorFrame_AutoApexReimport_Alert"), BalloonMessage, ID_BALLOON_NOTIFY_ID );
					}
				}
			}
		}
	}
}

/**
 * Helper function to enable and disable file listening that puts the file extensions local to one function
 * @param bTextureListenOnOff - TRUE if we want to listen to file system notifications for texture
 * @param bApexListenOnOff - TRUE if we want to listen to file system notifications for apex assets
 */
void WxEditorFrame::SetFileSystemNotifications(const UBOOL bTextureListenOnOff, const UBOOL bApexListenOnOff)
{
#if WITH_MANAGED_CODE
	SetFileSystemNotificationsForEditor(bTextureListenOnOff, bApexListenOnOff);
#endif
}

/**
 * Updates the application's title bar caption.
 */
void WxEditorFrame::RefreshCaption(const FFilename* LevelFilename)
{
	FString BaseCaption;
	
	if(LevelFilename->Len() > 0)
		BaseCaption = GetLocalizedCaption(*LevelFilename);
	else
		BaseCaption = GetLocalizedCaption(TEXT("Untitled"));

	//const FString CaptionWithLevel = *FString::Printf( LocalizeSecure(LocalizeUnrealEd("UnrealEdCaption_F"), (LevelFilename->Len()?*LevelFilename->GetBaseFilename():*LocalizeUnrealEd("Untitled")), *BaseCaption) );
	SetTitle( *BaseCaption );
}

///////////////////////////////////////////////////////////////////////////////

// Changes the viewport configuration

void WxEditorFrame::SetViewportConfig( EViewportConfig InConfig )
{
	FViewportConfig_Data* SaveConfig = ViewportConfigData;
	ViewportConfigData = new FViewportConfig_Data;

	SaveConfig->Save();

	SetPreviousViewportData( SaveConfig );

	// If PIE is still happening, stop it before doing anything
	if( GEditor->PlayWorld )
	{
		GEditor->EndPlayMap();
	}

	ViewportContainer->DestroyChildren();

	ViewportConfigData->SetTemplate( InConfig );

	// NOTE: We'll transfer the floating viewports from the original viewport config over to the new one
	const UBOOL bTransferFloatingViewports = TRUE;
	ViewportConfigData->Load( SaveConfig, bTransferFloatingViewports );

	// The user is changing the viewport config via the main menu. In this situation
	// we want to reset the splitter positions so they are in their default positions.

	GConfig->SetInt( TEXT("ViewportConfig"), TEXT("Splitter0"), 0, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("ViewportConfig"), TEXT("Splitter1"), 0, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("ViewportConfig"), TEXT("Splitter2"), 0, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("ViewportConfig"), TEXT("Splitter3"), 0, GEditorUserSettingsIni );

	ViewportConfigData->Apply( ViewportContainer );

	delete SaveConfig;
}

FGetInfoRet WxEditorFrame::GetInfo( INT Item )
{
	FGetInfoRet Ret;

	Ret.iValue = 0;
	Ret.String = TEXT("");

	// ACTORS
	if( Item & GI_NUM_SELECTED
			|| Item & GI_CLASSNAME_SELECTED
			|| Item & GI_CLASS_SELECTED )
	{
		INT NumActors = 0;
		UBOOL bAnyClass = FALSE;
		UClass*	AllClass = NULL;

		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if( bAnyClass && Actor->GetClass() != AllClass )
			{
				AllClass = NULL;
			}
			else
			{
				AllClass = Actor->GetClass();
			}

			bAnyClass = TRUE;
			NumActors++;
		}

		if( Item & GI_NUM_SELECTED )
		{
			Ret.iValue = NumActors;
		}
		if( Item & GI_CLASSNAME_SELECTED )
		{
			if( bAnyClass && AllClass )
			{
				Ret.String = AllClass->GetName();
			}
			else
			{
				Ret.String = TEXT("Actor");
			}
		}
		if( Item & GI_CLASS_SELECTED )
		{
			if( bAnyClass && AllClass )
			{
				Ret.pClass = AllClass;
			}
			else
			{
				Ret.pClass = NULL;
			}
		}
	}

	// SURFACES
	if( Item & GI_NUM_SURF_SELECTED)
	{
		INT NumSurfs = 0;

		for( INT i=0; i<GWorld->GetModel()->Surfs.Num(); ++i )
		{
			FBspSurf *Poly = &GWorld->GetModel()->Surfs(i);

			if( Poly->PolyFlags & PF_Selected )
			{
				NumSurfs++;
			}
		}

		if( Item & GI_NUM_SURF_SELECTED )
		{
			Ret.iValue = NumSurfs;
		}
	}

	return Ret;
}

/**
 * Accessor for the MRU menu
 *
 * @return	Menu for MRU items
 */
wxMenu* WxEditorFrame::GetMRUMenu()
{
	return MainMenuBar->MRUMenu;
}

/**
 * Accessor for Favorites list menu
 *
 * @return	Menu for the favorites list
 */
wxMenu* WxEditorFrame::GetFavoritesMenu()
{
	return MainMenuBar->FavoritesMenu;
}

/**
 * Accessor for the combined menu of MRU and Favorites items
 *
 * @return	Combined menu with MRU and favorites items
 */
wxMenu* WxEditorFrame::GetCombinedMRUFavoritesMenu()
{
	return MainMenuBar->MRUFavoritesCombinedMenu;
}

/**
 * Accessor for the Lock read-only item
 *
 * @return	Lock item
 */
wxMenuItem* WxEditorFrame::GetLockReadOnlyLevelsItem()
{
	return MainMenuBar->LockReadOnlyLevelsItem;
}

/**
 * Accessor for the MRU/Favorites list
 *
 * @return	MRU/Favorites list
 */
FMainMRUFavoritesList* WxEditorFrame::GetMRUFavoritesList()
{
	return MainMenuBar->MRUFavoritesList;
}

void WxEditorFrame::OnSize( wxSizeEvent& InEvent )
{
	// Don't allow sizing to occur when the frame is iconized, as it leads to problems
	if( MainToolBar && !IsIconized())
	{
		wxRect rc = GetClientRect();
		wxRect tbrc = MainToolBar->GetClientRect();
		INT ToolBarH = tbrc.GetHeight();

		wxRect rcmb( 0, 0, rc.GetWidth(), 32 );

		if( rcmb.GetWidth() == 0 )
		{
			rcmb.width = 1000;
		}
		if( rcmb.GetHeight() == 0 )
		{
			rcmb.height = 32;
		}

		ButtonBar->SetSize( 0, 0, LEFT_BUTTON_BAR_SZ, rc.GetHeight() );

		// Figure out the client area remaining for viewports once the docked windows are taken into account

		wxSize OldSize = ViewportContainer->GetSize();
		wxSize NewSize( rc.GetWidth() - LEFT_BUTTON_BAR_SZ, rc.GetHeight() );
		if ( bFrameMaximized != UBOOL(IsMaximized()) && ViewportConfigData )
		{
			ViewportConfigData->ResizeProportionally( FLOAT(NewSize.x)/FLOAT(OldSize.x), FLOAT(NewSize.y)/FLOAT(OldSize.y), FALSE );
		}
		ViewportContainer->SetSize( LEFT_BUTTON_BAR_SZ, 0, NewSize.x, NewSize.y );

		// SetSize() can be deferred for later, and Layout() doesn't recognize this. But it does use VirtualSize, if specified.
		ViewportContainer->SetVirtualSize( NewSize.x, NewSize.y );
		ViewportContainer->Layout();
		ViewportContainer->SetVirtualSize( -1, -1 );

		if ( ViewportConfigData )
		{
			ViewportConfigData->Layout();
		}

		if ( !IsMaximized() && InEvent.GetSize().x != 0 && InEvent.GetSize().y != 0 )
		{
			FrameSize = InEvent.GetSize();
		}
		bFrameMaximized = IsMaximized() ? TRUE : FALSE;
	}
}

void WxEditorFrame::OnMove( wxMoveEvent& InEvent )
{
	if ( !IsIconized() && !IsMaximized() )
	{
		FramePos = GetPosition();
	}
}

void WxEditorFrame::OnSplitterChanging( wxSplitterEvent& InEvent )
{
	// Prevent user from resizing if we've maximized a viewport
	if ( ViewportConfigData && ViewportConfigData->IsViewportMaximized() )
	{
		InEvent.Veto();
	}
}

void WxEditorFrame::OnSplitterDblClk( wxSplitterEvent& InEvent )
{
	// Always disallow double-clicking on the splitter bars. Default behavior is otherwise to unsplit.
	InEvent.Veto();
}

void WxEditorFrame::OnLightingQualityButton(wxCommandEvent& In)
{
	INT QualityLevel = Clamp<INT>(In.GetId(), Quality_Preview, Quality_Production);
	GConfig->SetInt(TEXT("LightingBuildOptions"), TEXT("QualityLevel"), QualityLevel, GEditorUserSettingsIni);
	MainToolBar->UpdateLightingQualityState();
}

/**Called to send wire frame constants to the render thread*/
void WxEditorFrame::InitWireframeConstants()
{
	WireframeHaloSettings.FadeStartDistance = 20.0f;
	WireframeHaloSettings.FadeGradientDistance = 100.0f;
	WireframeHaloSettings.DepthAcceptanceFactor = 1000.0f;
	WireframeHaloSettings.bEnablePostEffect = TRUE;

	//load constants from ini file
	GConfig->GetFloat(TEXT("Wireframe"), TEXT("FadeStartDistance"),		WireframeHaloSettings.FadeStartDistance,		GEditorUserSettingsIni);
	GConfig->GetFloat(TEXT("Wireframe"), TEXT("FadeGradientDistance"),	WireframeHaloSettings.FadeGradientDistance,		GEditorUserSettingsIni);
	GConfig->GetFloat(TEXT("Wireframe"), TEXT("DepthAcceptanceFactor"),	WireframeHaloSettings.DepthAcceptanceFactor,	GEditorUserSettingsIni);
	GConfig->GetBool(TEXT("Wireframe"), TEXT("EnableWireframeHalos"),	WireframeHaloSettings.bEnablePostEffect,		GEditorUserSettingsIni);

	//send constants down to render thread
	SendWireframeConstantsToRenderThread();
}


extern FDepthDependentHaloSettings GDepthDependentHaloSettings_RenderThread;

/**sends depth dependent halo constants down to render thread*/
void WxEditorFrame::SendWireframeConstantsToRenderThread(void)
{
	//enqueue commands to update values
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		SendWireframeHaloSettings,
		FDepthDependentHaloSettings*,WireframeHaloSettings,&WireframeHaloSettings,
	{
		GDepthDependentHaloSettings_RenderThread = *WireframeHaloSettings;
	});
}

/**
 * Accessor for wizard bitmap that is loaded on demand
 */
WxBitmap& WxEditorFrame::GetWizardB(void)
{
	if (WizardB.GetWidth() == 0)
	{
		WizardB.Load( TEXT("Wizard") );
	}
	return WizardB;
}

/**
 * Accessor for (little) down arrow bitmap that is loaded on demand
 */
WxBitmap& WxEditorFrame::GetDownArrowB(void)
{
	if (DownArrowB.GetWidth() == 0)
	{
		DownArrowB.Load( TEXT("DownArrow.png") );
	}
	return DownArrowB;
}

/**
 * Accessor for (big) arrow down bitmap that is loaded on demand
 */
WxBitmap& WxEditorFrame::GetArrowDown(void)
{
	if (ArrowDown.GetWidth() == 0)
	{
		ArrowDown.Load( TEXT("DownArrowLarge.png") );
	}
	return ArrowDown;
}
/**
 * Accessor for arrow up bitmap that is loaded on demand
 */
WxBitmap& WxEditorFrame::GetArrowUp(void)
{
	if (ArrowUp.GetWidth() == 0)
	{
		ArrowUp.Load( TEXT("UpArrowLarge.png") );
	}
	return ArrowUp;
}




void WxEditorFrame::OnClose( wxCloseEvent& InEvent )
{
	check(GEngine);

	GCallbackEvent->Send( CALLBACK_PreEditorClose );

	// if PIE is still happening, stop it before doing anything
	if (GEditor->PlayWorld)
	{
		GEditor->EndPlayMap();
	}

	// End any play on console games still happening
	GEditor->EndPlayOnConsole();

        // If a FaceFX Studio window is  open we need to make sure it is closed in
	// a certain way so that it can prompt the user to save changes.
#if WITH_FACEFX_STUDIO
	if( OC3Ent::Face::FxStudioApp::GetMainWindow() )
	{
		OC3Ent::Face::FxStudioApp::GetMainWindow()->ProcessEvent(InEvent);
	}
#endif // WITH_FACEFX_STUDIO

	// if this returns false, then we don't want to actually shut down, and we should go back to the editor 
	UBOOL bPromptUserToSave = TRUE;
	UBOOL bSaveMapPackages = TRUE;
	UBOOL bSaveContentPackages = TRUE;
	if( !FEditorFileUtils::SaveDirtyPackages( bPromptUserToSave, bSaveMapPackages, bSaveContentPackages ) && !GIsUnattended )
	{
		// Clear out any pending project changes.  The user canceled the save dialog.
		PendingProjectExe.Empty();
		PendingProjectCmdLine.Empty();

		// if we are forcefully being close, we can't stop the rush
		if (!InEvent.CanVeto())
		{
			appErrorf(TEXT("The user didn't want to quit, but wxWindows is forcing us to quit."));
		}
		InEvent.Veto();
	}
	else
	{
		if( PendingProjectExe.Len() > 0 )
		{
			// If there is a pending project switch, spawn that process now.
			FString Cmd = FString( TEXT( "editor" ) );
			if( PendingProjectCmdLine.Len() > 0 )
			{
				Cmd = PendingProjectCmdLine;
			}

			void* Handle = appCreateProc( *PendingProjectExe, *Cmd );
			if( !Handle )
			{
				// We were not able to spawn the new project exe.
				// Its likely that the exe doesnt exist.
				// Skip shutting down the editor if this happens
				warnf( TEXT("Could not restart the editor") );

				// Clear the pending project to ensure the editor can still be shut down normally
				PendingProjectExe.Empty();

				InEvent.Veto();
				return;
			}
		}

		// otherwise, we destroy the window
		Destroy();
	}
}

/**
* Called when the application is minimized.
*/
void WxEditorFrame::OnIconize( wxIconizeEvent& InEvent )
{
	// Loop through all children and set them to a non-minimized state when the user restores the editor window.
	const UBOOL bMinimized = IsIconized();

	if( bMinimized == FALSE )
	{
		RestoreAllChildren();
	}
	// Pass a dummy size event to the frame when restoring in case any changes have occured while the frame was iconized
	else
	{
		MinimizeAllChildren();
	}
}

/*
 * Manage how the Editors child windows are minimized. An issue exists in WxWidgets where windows with the 
 * "wxFRAME_NO_TASKBAR" flag omitted fail to minimize with the parent. To counter this, these windows (I.e.
 * Content Browser, Kismet etc )are instead hidden.
 *
 * @return  void
 *
 */
void WxEditorFrame::MinimizeAllChildren()
{
	wxWindowList Children = GetChildren();

	wxWindowListNode *Node = Children.GetFirst();
	while (Node)
	{

		wxWindow *Window = Node->GetData();
		wxTopLevelWindow *TopLevelWindow = wxDynamicCast(Window, wxTopLevelWindow);

		if(TopLevelWindow)
		{
			WxDockingContainer* FloatingFrame = wxDynamicCast( TopLevelWindow, WxDockingContainer);
			if( FloatingFrame )
			{
				TopLevelWindow->Hide();
			}
		}

		Node = Node->GetNext();
	}
}

/**
 * Called when the application is maximized.
 */
void WxEditorFrame::OnMaximize( wxMaximizeEvent& InEvent )
{
	// Loop through all children and set them to a non-minimized state when the user restores the editor window.
	const UBOOL bMaximized = IsMaximized();

	if( bMaximized == TRUE )
	{
		RestoreAllChildren();
	}
}

/**
 * Restores all minimized children.
 */
void WxEditorFrame::RestoreAllChildren()
{
	wxWindowList Children = GetChildren();

	wxWindowListNode *Node = Children.GetFirst();
	while (Node)
	{
		wxWindow *Window = Node->GetData();

		wxTopLevelWindow *TopLevelWindow = wxDynamicCast(Window, wxTopLevelWindow);
		if( TopLevelWindow )
		{
			WxDockingContainer* FloatingFrame = wxDynamicCast( TopLevelWindow, WxDockingContainer);
			WxTrackableFrame* TrackableFrame = wxDynamicCast( TopLevelWindow, WxTrackableFrame );

			// If a floating frame is Closed, it is in a hidden state. So we should show it.
			// Don't restore the floating frame if it has been maximized! It looks wrong.
			if( FloatingFrame && !TrackableFrame && !FloatingFrame->IsClosed() && !FloatingFrame->IsMaximized() )
			{
				FloatingFrame->Show();
				FloatingFrame->Restore();
			}
			else if(TopLevelWindow->IsIconized() && TopLevelWindow->IsShown())
			{
				TopLevelWindow->Iconize();
			}
		}

		Node = Node->GetNext();
	}
}


void WxEditorFrame::MenuFileNewMap( wxCommandEvent& In )
{
	// Previously called FEditorFileUtils::NewMap();
	// Current method presents the interactive new map screen.
	FEditorFileUtils::NewMapInteractive();
}

void WxEditorFrame::MenuFileNewProject( wxCommandEvent& In )
{
	FEditorFileUtils::NewProjectInteractive();
}

void WxEditorFrame::MenuFileOpen( wxCommandEvent& In )
{
	FEditorFileUtils::LoadMap();
}


void WxEditorFrame::MenuFileSave( wxCommandEvent& In )
{
	if( GWorld->GetOutermost()->PackageFlags & PKG_Cooked )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
		return;
	}

	if ( FLevelUtils::IsLevelLocked(GWorld->CurrentLevel) )
	{
		appMsgf(AMT_OK, TEXT("SaveLevel: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
	}

	// Prompt the user to check the level out of source control before saving
	else if ( GWorld->CurrentLevel && FEditorFileUtils::PromptToCheckoutLevels( FALSE, GWorld->CurrentLevel ) )
	{
		FEditorFileUtils::SaveLevel( GWorld->CurrentLevel );
	}
}

void WxEditorFrame::MenuFileSaveAs( wxCommandEvent& In )
{
	FEditorFileUtils::SaveAs( GWorld );
	GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_UpdatePackageList ));
}

void WxEditorFrame::MenuFileSaveDlg( wxCommandEvent& In )
{
	const UBOOL bPromptUserToSave = TRUE;
	const UBOOL bSaveMapPackages = TRUE;
	const UBOOL bSaveContentPackages = TRUE;
	FEditorFileUtils::SaveDirtyPackages( bPromptUserToSave, bSaveMapPackages, bSaveContentPackages );
}

void WxEditorFrame::MenuFileSaveAll( wxCommandEvent& In )
{
	const UBOOL bPromptUserToSave = FALSE;
	const UBOOL bSaveMapPackages = TRUE;
	const UBOOL bSaveContentPackages = TRUE;
	// If the menu item that was clicked is save all writable this is a fast save
	const UBOOL bFastSave = (In.GetId() == IDM_SAVE_ALL_WRITABLE) ? TRUE : FALSE;
	FEditorFileUtils::SaveDirtyPackages( bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave );
}

void WxEditorFrame::MenuFileForceSaveAll( wxCommandEvent& In )
{
	const UBOOL bCheckIfDirty = FALSE;
	FEditorFileUtils::SaveAllWritableLevels( bCheckIfDirty );
	GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_UpdatePackageList | CBR_UpdateSCCState ));
}

void WxEditorFrame::MenuFileSaveAllLevels( wxCommandEvent& In )
{
	const UBOOL bPromptUserToSave = FALSE;
	const UBOOL bSaveMapPackages = TRUE;
	const UBOOL bSaveContentPackages = FALSE;
	const UBOOL bFastSave = FALSE; // We are saving all files not just writable ones
	FEditorFileUtils::SaveDirtyPackages( bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave );
}

/**
 * Called when the user selects a new editor (render or mobile) mode from the preferences menu
 *
 * @param	In	Event automatically generated by wxWidgets when the user selects the option to change editor mode
 */
void WxEditorFrame::MenuSelectEditorMode( wxCommandEvent& In )
{
	// If this mode was already checked then we don't need to do anything...
	if( !In.IsChecked() )
	{
		return;
	}

	WxChoiceDialog ConfirmationPrompt(
		LocalizeUnrealEd("RestartEditorPrompt_Message"), 
		LocalizeUnrealEd("RestartEditorPrompt_Title"),
		WxChoiceDialogBase::Choice( AMT_OK, LocalizeUnrealEd("RestartEditorPrompt_Ok"), WxChoiceDialogBase::DCT_DefaultAffirmative ),
		WxChoiceDialogBase::Choice( AMT_OKCancel, LocalizeUnrealEd("RestartEditorPrompt_Cancel"), WxChoiceDialogBase::DCT_DefaultCancel ) );
	ConfirmationPrompt.ShowModal();

	switch( In.GetId() )
	{
		case IDM_EDITOR_PREFERENCES_DX11_MODE:
			GConfig->SetInt( TEXT( "Startup" ), TEXT( "RenderMode" ), RENDER_MODE_DX11 , GEditorUserSettingsIni );
			break;

		case IDM_EDITOR_PREFERENCES_DX9_MODE:
			GConfig->SetInt( TEXT( "Startup" ), TEXT( "RenderMode" ), RENDER_MODE_DX9, GEditorUserSettingsIni );
			break;
	}

	// Save off the new settings 
	GConfig->Flush( FALSE, GEditorUserSettingsIni );

	if ( ConfirmationPrompt.GetChoice().ReturnCode == AMT_OK )
	{
		// Restart the editor if the user selects yes from the popup dialog
		const FString ProjectName(GGameName);
		CreateProjectPath(ProjectName);
		PendingProjectCmdLine = FString::Printf( TEXT( "editor %s" ), appCmdLine() );

		// Remove -dx9 / -dx11 / -sm3 / -sm5 ... these will stop the switch from happening...
		PendingProjectCmdLine = PendingProjectCmdLine.Replace( TEXT( "-sm3" ), TEXT( "" ), TRUE );
		PendingProjectCmdLine = PendingProjectCmdLine.Replace( TEXT( "-sm5" ), TEXT( "" ), TRUE );
		PendingProjectCmdLine = PendingProjectCmdLine.Replace( TEXT( "-dx9" ), TEXT( "" ), TRUE );
		PendingProjectCmdLine = PendingProjectCmdLine.Replace( TEXT( "-dx11" ), TEXT( "" ), TRUE );

		Close();
	}
}

void WxEditorFrame::MenuFileImportNew( wxCommandEvent& In )
{
	// Import, and ask for save first (bMerging flag set to FALSE).
	FEditorFileUtils::Import( FALSE );
}

void WxEditorFrame::MenuFileImportMerge( wxCommandEvent& In )
{
	// Import but don't bother saving since we're merging (bMerging flag set to TRUE).
	FEditorFileUtils::Import( TRUE );
}

void WxEditorFrame::MenuFileExportAll( wxCommandEvent& In )
{
	// Export with bExportSelectedActorsOnly set to FALSE.
	FEditorFileUtils::Export( FALSE );
}

void WxEditorFrame::MenuFileExportSelected( wxCommandEvent& In )
{
	// Export with bExportSelectedActorsOnly set to TRUE.
	FEditorFileUtils::Export( TRUE );
}

void WxEditorFrame::MenuFileMRU( wxCommandEvent& In )
{
	// Save the name of the file we are attempting to load as VerifyFile/AskSaveChanges might rearrange the MRU list on us
	const FFilename NewFilename = GetMRUFavoritesList()->GetMRUItem( In.GetId() - IDM_MRU_START );
	
	if( GetMRUFavoritesList()->VerifyMRUFile( In.GetId() - IDM_MRU_START ) )
	{
		// Prompt the user to save any outstanding changes.
		if( FEditorFileUtils::SaveDirtyPackages(TRUE, TRUE, FALSE) == FALSE )
		{
			// something went wrong or the user pressed cancel.  Return to the editor so the user doesn't lose their changes		
			return;
		}

		// Load the requested level.
		FEditorFileUtils::LoadMap( NewFilename );
	}
}

/** Called when an item in the Matinee list drop-down menu is clicked */
void WxEditorFrame::OnMatineeListMenuItem( wxCommandEvent& In )
{
	const INT MatineeIndex = In.GetId() - IDM_MainToolBar_MatineeListItem_Start;
	if( MatineeIndex >= 0 && MatineeIndex < MatineeListMenuMap.Num() )
	{
		USeqAct_Interp* MatineeSeq = MatineeListMenuMap( MatineeIndex );
		if( MatineeSeq != NULL )
		{
			// Open the Matinee for editing!
			WxKismet::OpenMatinee( MatineeSeq );
		}
	}
}

void WxEditorFrame::MenuFileExit( wxCommandEvent& In )
{
	// By calling Close instead of destroy, our Close Event handler will get called
	Close();
}

void WxEditorFrame::MenuEditUndo( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("TRANSACTION UNDO") );
}

void WxEditorFrame::MenuEditRedo( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("TRANSACTION REDO") );
}

void WxEditorFrame::MenuFarPlaneScrollChanged( wxCommandEvent& In )
{
	wxControl* Control = MainToolBar->FindControl(ID_FarPlaneSlider);
	wxSlider* Slider = wxDynamicCast(Control, wxSlider);

	const INT Value = In.GetInt();

	FLOAT SelectedDist = 0.0f;
	if( Value != Slider->GetMax() )
	{
		SelectedDist = 1024.0f * appPow(1.06f, (FLOAT)Value);
	}

	GUnrealEd->Exec( *FString::Printf(TEXT("FARPLANE DIST=%f"), SelectedDist) );
}

void WxEditorFrame::MenuFarPlaneScrollChangeEnd( wxScrollEvent& In )
{
	GUnrealEd->SaveConfig();
}

void WxEditorFrame::MenuEditMouseLock( wxCommandEvent& In )
{
	GEditorModeTools().SetMouseLock( !GEditorModeTools().GetMouseLock() );
	// Should not be able to see any widget in selection mode.
	GEditorModeTools().SetWidgetMode ( FWidget::WM_None );
	GUnrealEd->RedrawAllViewports();
}

void WxEditorFrame::MenuEditShowWidget( wxCommandEvent& In )
{
	GEditorModeTools().SetShowWidget( !GEditorModeTools().GetShowWidget() );
	GUnrealEd->RedrawAllViewports();
}

void WxEditorFrame::MenuEditTranslate( wxCommandEvent& In )
{
	GEditorModeTools().SetWidgetMode( FWidget::WM_Translate );
	// Turn off mouse lock
	GEditorModeTools().SetMouseLock( FALSE );
	GUnrealEd->RedrawAllViewports();
}

void WxEditorFrame::MenuEditRotate( wxCommandEvent& In )
{
	GEditorModeTools().SetWidgetMode( FWidget::WM_Rotate );
	// Turn off mouse lock
	GEditorModeTools().SetMouseLock( FALSE );
	GUnrealEd->RedrawAllViewports();
}

void WxEditorFrame::MenuEditScale( wxCommandEvent& In )
{
	GEditorModeTools().SetWidgetMode( FWidget::WM_Scale );
	// Turn off mouse lock
	GEditorModeTools().SetMouseLock( FALSE );
	GUnrealEd->RedrawAllViewports();
}

void WxEditorFrame::MenuEditScaleNonUniform( wxCommandEvent& In )
{
	GEditorModeTools().SetWidgetMode( FWidget::WM_ScaleNonUniform );
	// Turn off mouse lock
	GEditorModeTools().SetMouseLock( FALSE );
	GUnrealEd->RedrawAllViewports();
}

void WxEditorFrame::CoordSystemSelChanged( wxCommandEvent& In )
{
	GEditorModeTools().CoordSystem = (ECoordSystem)In.GetInt();
	GUnrealEd->RedrawAllViewports();
}

/**
 * Called in response to the actor search dialog button being pressed;
 * Displays the actor search dialog and grants it focus
 *
 * @param	In	Event generated by wxWidgets in response to the button being pressed
 */
void WxEditorFrame::MenuEditSearch( wxCommandEvent& In )
{
	WxDlgActorSearch* ActorSearchDialog = GApp->GetDlgActorSearch();
	ActorSearchDialog->Show(1);

	// Raise the window and give it focus in case it's already displayed but covered
	// by other windows
	ActorSearchDialog->Raise();
	ActorSearchDialog->SetFocus();
}

void WxEditorFrame::MenuEditCut( wxCommandEvent& In )
{
	//FString cmd = FString::Printf( TEXT("EDIT CUT CLIPPAD=%d"), ( GetAsyncKeyState(VK_SHIFT) & 0x8000 ) ? 1 : 0 );
	FString cmd = TEXT("EDIT CUT");
	GUnrealEd->Exec( *cmd );
}

void WxEditorFrame::MenuEditCopy( wxCommandEvent& In )
{
	//FString cmd = FString::Printf( TEXT("EDIT COPY CLIPPAD=%d"), ( GetAsyncKeyState(VK_SHIFT) & 0x8000 ) ? 1 : 0 );
	FString cmd = TEXT("EDIT COPY");
	GUnrealEd->Exec( *cmd );
}

void WxEditorFrame::MenuEditPasteOriginalLocation( wxCommandEvent& In )
{
	//FString cmd = FString::Printf( TEXT("EDIT PASTE CLIPPAD=%d"), ( GetAsyncKeyState(VK_SHIFT) & 0x8000 ) ? 1 : 0 );
	FString cmd = TEXT("EDIT PASTE");
	GUnrealEd->Exec( *cmd );
}

void WxEditorFrame::MenuEditPasteWorldOrigin( wxCommandEvent& In )
{
	//FString cmd = FString::Printf( TEXT("EDIT PASTE TO=ORIGIN CLIPPAD=%d"), ( GetAsyncKeyState(VK_SHIFT) & 0x8000 ) ? 1 : 0 );
	FString cmd = TEXT("EDIT PASTE TO=ORIGIN");
	GUnrealEd->Exec( *cmd );
}

void WxEditorFrame::MenuEditPasteHere( wxCommandEvent& In )
{
	//FString cmd = FString::Printf( TEXT("EDIT PASTE TO=HERE CLIPPAD=%d"), ( GetAsyncKeyState(VK_SHIFT) & 0x8000 ) ? 1 : 0 );
	FString cmd = TEXT("EDIT PASTE TO=HERE");
	GUnrealEd->Exec( *cmd );
}

void WxEditorFrame::MenuEditDuplicate( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("DUPLICATE") );
}

void WxEditorFrame::MenuEditDelete( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("DELETE") );
}

void WxEditorFrame::MenuEditSelectNone( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("SELECT NONE") );
}

/**
 * Called in response to the user selecting the option to select the builder brush; Deselects everything else in the editor and
 * then selects the builder brush
 *
 * @param	In	Event automatically generated by wxWidgets when the user selects the appropriate menu option
 */
void WxEditorFrame::MenuEditSelectBuilderBrush( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("SELECT BUILDERBRUSH") );
}

void WxEditorFrame::MenuEditSelectAll( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT ALL") );
}

void WxEditorFrame::MenuEditSelectByProperty( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT BYPROPERTY") );
}

void WxEditorFrame::MenuEditSelectInvert( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT INVERT") );
}

/*
 * Selects the current post process volume affecting the camera
 */
void WxEditorFrame::MenuEditSelectPostProcessVolume( wxCommandEvent& In )
{
	if(ViewportConfigData)
	{
		// Find a perspective viewport to use
		WxLevelViewportWindow* Viewport = NULL;
		for(INT Idx = 0; Idx < 4; Idx++)
		{
			if( ViewportConfigData->Viewports[Idx].ViewportType == LVT_Perspective )
			{
				Viewport = ViewportConfigData->Viewports[Idx].ViewportWindow;
				break;
			}
		}

		if(GWorld && Viewport)
		{
			// If a Volume is affecting the PP settings, select that volume
			APostProcessVolume* Volume = GWorld->GetWorldInfo()->GetPostProcessSettings(
				Viewport->ViewLocation,
				Viewport->bPostProcessVolumePrevis,
				Viewport->PostProcessSettings);
			if(Volume)
			{
				GEditor->SelectNone( TRUE, TRUE );
				GEditor->SelectActor( Volume, TRUE, NULL, TRUE );
			}
		}
	}
}

void WxEditorFrame::MenuTogglePrefabsLocked( wxCommandEvent& In )
{
	GUnrealEd->bPrefabsLocked = !GUnrealEd->bPrefabsLocked;
	GEditor->SaveConfig();
}

void WxEditorFrame::MenuToggleGroupsActive( wxCommandEvent& In )
{
	AGroupActor::ToggleGroupMode();
}	

void WxEditorFrame::MenuViewFullScreen( wxCommandEvent& In )
{
	ShowFullScreen( !IsFullScreen(), wxFULLSCREEN_NOBORDER | wxFULLSCREEN_NOCAPTION );
}

void WxEditorFrame::MenuViewBrushPolys( wxCommandEvent& In )
{
	GEditor->Exec( *FString::Printf( TEXT("MODE SHOWBRUSHMARKERPOLYS=%d"), !GEditor->bShowBrushMarkerPolys ? 1 : 0 ) );
	GEditor->SaveConfig();
}

void WxEditorFrame::MenuViewDistributionToggle( wxCommandEvent& In )
{
	// @GEMINI_TODO: Less global var hack
	extern DWORD GDistributionType;
	GDistributionType ^= 1;
}

void WxEditorFrame::MenuMaterialQualityToggle( wxCommandEvent& In )
{
	// to be safe, we wait until rendering thread is complete
	FlushRenderingCommands();

	// toggle the system setting, mimicing what the console command does - it would be nice
	// if system settings had a function interface instead of console commands
	GSystemSettings.bAllowHighQualityMaterials ^= 1;

	TArray<UClass*> ExcludeComponents;
	ExcludeComponents.AddItem(UDecalComponent::StaticClass());
	ExcludeComponents.AddItem(UAudioComponent::StaticClass());

	FGlobalComponentReattachContext PropagateDetailModeChanges(ExcludeComponents);

	// tell viewports to redraw
	GEditor->RedrawAllViewports();
}

void WxEditorFrame::MenuViewToggleLODLocking( wxCommandEvent& In )
{
	GEditor->bEnableLODLocking = !GEditor->bEnableLODLocking;
	GEditor->RedrawLevelEditingViewports();
}

void WxEditorFrame::MenuToggleSocketSnapping( wxCommandEvent& In )
{
	GEditor->bEnableSocketSnapping = !GEditor->bEnableSocketSnapping;
	GEditor->RedrawLevelEditingViewports();
}

void WxEditorFrame::MenuToggleSocketNames( wxCommandEvent& In )
{
	GEditor->bEnableSocketNames = !GEditor->bEnableSocketNames;
	GEditor->RedrawLevelEditingViewports();
}

void WxEditorFrame::MenuViewPSysRealtimeLODToggle( wxCommandEvent& In )
{
	extern UBOOL GbEnableEditorPSysRealtimeLOD;
	GbEnableEditorPSysRealtimeLOD = !GbEnableEditorPSysRealtimeLOD;
	GEditor->RedrawLevelEditingViewports();
}

void WxEditorFrame::MenuViewPSysHelperToggle( wxCommandEvent& In )
{
	GEditor->bDrawParticleHelpers = !GEditor->bDrawParticleHelpers;
}

void WxEditorFrame::MenuViewportConfig( wxCommandEvent& In )
{
	EViewportConfig ViewportConfig = VC_2_2_Split;

	switch( In.GetId() )
	{
		case IDM_VIEWPORT_CONFIG_2_2_SPLIT:	ViewportConfig = VC_2_2_Split;	break;
		case IDM_VIEWPORT_CONFIG_1_2_SPLIT:	ViewportConfig = VC_1_2_Split;	break;
		case IDM_VIEWPORT_CONFIG_1_1_SPLIT_H:	ViewportConfig = VC_1_1_SplitH;	break;
		case IDM_VIEWPORT_CONFIG_1_1_SPLIT_V:	ViewportConfig = VC_1_1_SplitV;	break;
	}

	SetViewportConfig( ViewportConfig );
}



void WxEditorFrame::MenuOpenNewFloatingViewport( wxCommandEvent& In )
{
	FFloatingViewportParams ViewportParams;
	ViewportParams.ParentWxWindow = this;
	ViewportParams.ViewportType = LVT_Perspective;
	ViewportParams.ShowFlags = 0;
	ViewportParams.Width = 1280;
	ViewportParams.Height = 720;

	switch( In.GetId() )
	{
		case IDM_OpenNewFloatingViewport_Perspective:
			ViewportParams.ViewportType = LVT_Perspective;
			ViewportParams.ShowFlags = ( SHOW_DefaultEditor &~ SHOW_ViewMode_Mask ) | SHOW_ViewMode_Lit;
			break;

		case IDM_OpenNewFloatingViewport_OrthoXY:
			ViewportParams.ViewportType = LVT_OrthoXY;
			ViewportParams.ShowFlags = ( SHOW_DefaultEditor &~ SHOW_ViewMode_Mask ) | SHOW_ViewMode_BrushWireframe;
			break;

		case IDM_OpenNewFloatingViewport_OrthoXZ:
			ViewportParams.ViewportType = LVT_OrthoXZ;
			ViewportParams.ShowFlags = ( SHOW_DefaultEditor &~ SHOW_ViewMode_Mask ) | SHOW_ViewMode_BrushWireframe;
			break;

		case IDM_OpenNewFloatingViewport_OrthoYZ:
			ViewportParams.ViewportType = LVT_OrthoYZ;
			ViewportParams.ShowFlags = ( SHOW_DefaultEditor &~ SHOW_ViewMode_Mask ) | SHOW_ViewMode_BrushWireframe;
			break;
	}

	// Create the new floating viewport
	INT OutNewViewportIndex = INDEX_NONE;
	UBOOL bResultValue = ViewportConfigData->OpenNewFloatingViewport(ViewportParams, OutNewViewportIndex);

	if( !bResultValue )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd( "OpenNewFloatingViewport_Error" ) );
	}
}



void WxEditorFrame::MenuViewDetailModeLow(wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf(TEXT("SETDETAILMODEVIEW MODE=%d"), (INT)DM_Low ) );
}

void WxEditorFrame::MenuViewDetailModeMedium(wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf(TEXT("SETDETAILMODEVIEW MODE=%d"), (INT)DM_Medium ) );
}

void WxEditorFrame::MenuViewDetailModeHigh(wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf(TEXT("SETDETAILMODEVIEW MODE=%d"), (INT)DM_High ) );
}

/** Called when 'Enable WASD Camera Controls' is clicked in the Main Menu */
void WxEditorFrame::MenuAllowFlightCameraToRemapKeys( wxCommandEvent& In )
{
	const INT SelectionId = In.GetId();
	const INT SelectionIndex = SelectionId - IDM_MainMenu_FlightCameraOptionsStart;

	check( SelectionId >= IDM_MainMenu_FlightCameraOptionsStart && SelectionId <= IDM_MainMenu_FlightCameraOptionsEnd && SelectionIndex < 3 );

	// Don't do anything if the user selected the camera option the menu is already set to
	if ( (UINT)GEditor->AccessUserSettings().FlightCameraControlType != SelectionIndex )
	{
		GEditor->AccessUserSettings().FlightCameraControlType = SelectionIndex;
		GEditor->SaveUserSettings();
	}
}

/** Called when 'Auto Restart Reimported Flash Movies' is toggled in the preferences menu */
void WxEditorFrame::MenuAutoRestartReimportedFlashMovies( wxCommandEvent& In )
{
	GEditor->AccessUserSettings().bAutoRestartReimportedFlashMovies = In.IsChecked();
	GEditor->SaveUserSettings();
}

/** Called when 'Auto Reimport Textures' is toggled in the preferences menu */
void WxEditorFrame::MenuAutoReimportTextures( wxCommandEvent& In )
{
	GEditor->AccessUserSettings().bAutoReimportTextures = In.IsChecked();
	GEditor->SaveUserSettings();
	SetFileSystemNotifications(GEditor->AccessUserSettings().bAutoReimportTextures, GEditor->AccessUserSettings().bAutoReimportApexAssets);
}

void WxEditorFrame::MenuLoadSimpleLevelAtStartup( wxCommandEvent& In )
{
	GEditor->AccessUserSettings().bLoadSimpleLevelAtStartup = In.IsChecked();
	GEditor->SaveUserSettings();
}

void WxEditorFrame::MenuViewportResizeTogether(wxCommandEvent& In )
{
	if( In.IsChecked() )
	{
		bViewportResizeTogether = TRUE;

		//Make the splitter sashes match up.
		ViewportContainer->MatchSplitterPositions();
	}
	else
	{
		bViewportResizeTogether = FALSE;
	}
}

/** Event handler for toggling whether or not to center viewport zooming around the cursor. */
void WxEditorFrame::MenuCenterZoomAroundCursor(wxCommandEvent &In)
{
	UBOOL bCenterZoomAroundCursor = In.IsChecked() ? TRUE : FALSE;
	GEditorModeTools().SetCenterZoomAroundCursor(bCenterZoomAroundCursor);
}

/** Event handler for toggling whether or not all property windows should only display modified properties. */
void WxEditorFrame::MenuPanMovesCanvas(wxCommandEvent &In)
{
	UBOOL bPanMovesCanvas= In.IsChecked() ? TRUE : FALSE;
	GEditorModeTools().SetPanMovesCanvas(bPanMovesCanvas);
}

/** Event handler for toggling whether or not all property windows should only display modified properties. */
void WxEditorFrame::MenuReplaceRespectsScale(wxCommandEvent &In)
{
	UBOOL bReplaceRespectsScale = In.IsChecked() ? TRUE : FALSE;
	GEditorModeTools().SetReplaceRespectsScale(bReplaceRespectsScale);
}

/** Event handler for toggling whether or not to default to real time mode when opening a new perspective viewport. */
void WxEditorFrame::MenuDefaultToRealtimeMode( wxCommandEvent &In )
{
	GEditor->AccessUserSettings().bStartInRealtimeMode = In.IsChecked() ? TRUE : FALSE;
	// Save the settings to the config file
	GEditor->SaveUserSettings();
}


/**Callback to toggle Absolute Translation Mode */
void WxEditorFrame::MenuToggleAbsoluteTranslation (wxCommandEvent& In)
{
	GEditorModeTools().SetUsingAbsoluteTranslation(In.IsChecked());
	GEditorModeTools().SaveWidgetSettings();
}

/**Callback to toggle use of Translate RotateZ Widget*/
void WxEditorFrame::MenuToggleTranslateRotateZWidget (wxCommandEvent& In)
{
	GEditorModeTools().SetAllowTranslateRotateZWidget(In.IsChecked());
	GEditorModeTools().SaveWidgetSettings();
}

/** Event handler for toggling whether or not clicking a BSP surface selects the brush */
void WxEditorFrame::MenuClickBSPSelectsBrush( wxCommandEvent &In )
{
	UBOOL bClickBSPSelectsBrush = In.IsChecked() ? TRUE : FALSE;
	GEditorModeTools().SetClickBSPSelectsBrush(bClickBSPSelectsBrush);
}

/** Event handler for toggling whether or not BSP should auto-update */
void WxEditorFrame::MenuBSPAutoUpdate( wxCommandEvent &In )
{
	UBOOL bBSPAutoUpdate = In.IsChecked() ? TRUE : FALSE;
	GEditorModeTools().SetBSPAutoUpdate(bBSPAutoUpdate);
}

/** Event handler for toggling editor viewport hover feedback effect */
void WxEditorFrame::MenuViewportHoverFeedback( wxCommandEvent &In )
{
	GEditor->AccessUserSettings().bEnableViewportHoverFeedback = In.IsChecked() ? TRUE : FALSE;
	
	// Save the settings to the config file
	GEditor->SaveUserSettings();
}

/** Event Handler for toggling editor highlight selected objects with brackets */
void WxEditorFrame::MenuHighlightWithBrackets( wxCommandEvent &In )
{
	GEditorModeTools().SetHighlightWithBrackets( In.IsChecked() );
	GEngine->SelectedMaterialColor = In.IsChecked() ? FLinearColor::Black : GEngine->DefaultSelectedMaterialColor;
	GEditor->RedrawAllViewports();
}

/** Event handler for toggling wireframe halos*/
void WxEditorFrame::MenuWireframeHalos( wxCommandEvent &In )
{
	//toggle value
	WireframeHaloSettings.bEnablePostEffect = !WireframeHaloSettings.bEnablePostEffect;

	//save in the ini file
	GConfig->SetBool(TEXT("Wireframe"), TEXT("EnableWireframeHalos"), WireframeHaloSettings.bEnablePostEffect, GEditorUserSettingsIni);

	//send new constants back down to the render thread
	SendWireframeConstantsToRenderThread();

	//make sure to cause all viewports to refresh themselves to ensure this change takes affect
	GUnrealEd->RedrawAllViewports();
}

/**
 * Called when the user selects to use strict box selection, either from the View->Preferences menu or the main toolbar button
 *
 * @param	In	Event generated by wxWidgets when the menu item is selected
 */
void WxEditorFrame::MenuUseStrictBoxSelection( wxCommandEvent& In )
{
	UBOOL bIsChecked = In.IsChecked();

	wxObject* EventObject = In.GetEventObject();
	WxBitmapCheckButton* CheckButton = wxDynamicCast(EventObject, WxBitmapCheckButton);
	if (CheckButton)
	{
		//the standard logic (ON) is for general intersection
		INT CurrentState = CheckButton->GetCurrentState()->ID;
		bIsChecked = !(CurrentState == WxBitmapCheckButton::STATE_On);
	}

	GEditor->AccessUserSettings().bStrictBoxSelection = bIsChecked;
	GEditor->SaveUserSettings();
}

/**
 * Called when the user selects to prompt for checkout on pkg modification from the View->Preferences menu
 *
 * @param	In	Event generated by wxWidgets when the menu item is selected
 */
void WxEditorFrame::MenuPromptSCCOnPackageModification( wxCommandEvent& In )
{
#if HAVE_SCC
	GEditor->AccessUserSettings().bPromptForCheckoutOnPackageModification = In.IsChecked();
	GEditor->SaveUserSettings();
#endif // #if HAVE_SCC
}

/**
 * Called when the user selects "linked orthographic viewport movement" from the View->Preferences menu
 *
 * @param	In	Event generated by wxWidgets when the menu item is selected
 */
void WxEditorFrame::MenuToggleLinkedOrthographicViewports( wxCommandEvent& In )
{
	GEditor->AccessUserSettings().bUseLinkedOrthographicViewports = In.IsChecked();
	GEditor->SaveUserSettings();
}

/**
 * Called when the user selects "Update Viewport Camera From Play-in-Viewport" from the View->Preferences menu
 *
 * @param	In	Event generated by wxWidgets when the menu item is selected
 */
void WxEditorFrame::MenuToggleViewportCameraToUpdateFromPIV( wxCommandEvent& In )
{
	GEditor->AccessUserSettings().bEnableViewportCameraToUpdateFromPIV = In.IsChecked();
	GEditor->SaveUserSettings();
}

/**
 * Reset all suppressible dialogs so that they are shown
 *
 * @param	In	Event generated by wxWidgets when the menu item is selected
 */
void WxEditorFrame::MenuResetSuppressibleDialogs( wxCommandEvent& In )
{
	TArray<FString> SuppressibleDialogs;
	if (GConfig->GetSection(TEXT("SuppressableDialogs"), SuppressibleDialogs, GEditorUserSettingsIni))
	{
		for( INT i = 0; i < SuppressibleDialogs.Num(); i++ )
		{
			INT AssignOpIndex = SuppressibleDialogs(i).InStr( TEXT("="), TRUE );
			if( AssignOpIndex != INDEX_NONE )
			{
				FString SuppressibleDialogsWOValue = SuppressibleDialogs(i).Left(AssignOpIndex);
				GConfig->SetBool( TEXT("SuppressableDialogs"), *SuppressibleDialogsWOValue, FALSE, GEditorUserSettingsIni );
			}
		}
	}
}

/**
 * Generates a string path to the project executable
 *
 * @param	ProjectName	The name of the project to create a path for
 */
void WxEditorFrame::CreateProjectPath(const FString& ProjectName)
{
	// Construct the proper exe to switch to.
	FString Separator = TEXT( "-" );
	FString Game = ProjectName + TEXT( "Game" );
	FString Extension = TEXT( ".exe" );

	// If we are running in 64 bit, launch the 64 bit process
#if _WIN64
	FString PlatformConfig = TEXT( "Win64" );
#else
	FString PlatformConfig = TEXT( "Win32" );
#endif

#if _DEBUG
	FString Config = TEXT( "Debug" );
	PendingProjectExe = appRootDir() * FString( TEXT( "Binaries" ) ) * PlatformConfig * Game + Separator + PlatformConfig + Separator + Config + Extension;
#elif SHIPPING_PC_GAME
	if( ProjectName == TEXT( "UDK" ) )
	{
		Game = TEXT( "UDK" );
		PendingProjectExe = appRootDir() * FString( TEXT( "Binaries" ) ) * PlatformConfig * Game + Extension;
	}
	else
	{
		FString Config = TEXT( "Shipping" );
		PendingProjectExe = appRootDir() * FString( TEXT( "Binaries" ) ) * PlatformConfig * Game + Separator + PlatformConfig + Separator + Config + Extension;
	}
#else
	PendingProjectExe = appRootDir() * FString( TEXT( "Binaries" ) ) * PlatformConfig * Game + Extension;
#endif
}

/**
 * Called when the user selects a language from the View->Preferences->Editor Language menu
 *
 * @param	In	Event generated by wxWidgets when the menu item is selected
 */
void WxEditorFrame::MenuLanguageSelection( wxCommandEvent& In )
{
	const INT SelectionId = In.GetId();
	const INT SelectionIndex = SelectionId - IDM_MainMenu_LanguageOptionStart;

	check( SelectionId >= IDM_MainMenu_LanguageOptionStart && SelectionId < IDM_MainMenu_LanguageOptionEnd &&  SelectionIndex < PreferencesMenu->SupportedLanguageExtensions.Num() );

	// Don't do anything if the user selected the language the menu is already set to
	if ( appStricmp( *( PreferencesMenu->SupportedLanguageExtensions(SelectionIndex) ), *appGetLanguageExt() ) != 0 )
	{
		// Set the ini setting to the user-selected language; the editor will load in the selected language upon a restart
		GConfig->SetString( TEXT("Engine.Engine"), TEXT("Language"), *( PreferencesMenu->SupportedLanguageExtensions(SelectionIndex) ), GEngineIni );

		// Warn the user that their selection will not apply until editor restart
		WxSuppressableWarningDialog LanguageSwitchWarning( LocalizeUnrealEd("EditorLanguageChangeDlg_Warning"), LocalizeUnrealEd("EditorLanguageChangeDlg_Title"), TEXT("LanguageSelectionWarningDlg") );
		LanguageSwitchWarning.ShowModal();
	}
}

/**
 * Called when the user selects a language from the View->Preferences->Editor Language menu
 *
 * @param	In	Event generated by wxWidgets when the menu item is selected
 */
void WxEditorFrame::MenuAspectRatioSelection( wxCommandEvent& In )
{
	GEditor->AccessUserSettings().AspectRatioAxisConstraint = In.GetId() - IDM_MainMenu_AspectRatioAxisConstraint_Start;
	GEditor->SaveUserSettings();

	//redraw in case aspect ratio changed
	GUnrealEd->RedrawLevelEditingViewports();
}

/**
 * Called when the user selects the "Run Unit Tests..." option from the View menu. Summons the unit test
 * dialog.
 *
 * @param	In	Event generated by wxWidgets when the menu item is selected
 */
void WxEditorFrame::MenuRunUnitTests( wxCommandEvent& In )
{
#if USE_UNIT_TESTS && WITH_MANAGED_CODE
	UnitTestWindow::DisplayUnitTestWindow();
#endif // #if USE_UNIT_TESTS && WITH_MANAGED_CODE
}

void WxEditorFrame::MenuViewShowBrowser( wxCommandEvent& In )
{
	GUnrealEd->GetBrowserManager()->ShowWindowByMenuID(In.GetId());
}

void WxEditorFrame::MenuOpenKismet( wxCommandEvent& In )
{
	if (GKismetRealtimeDebugging && (GEditor->PlayWorld != NULL))
	{
		WxKismet::OpenKismetDebugger(GWorld->GetGameSequence(), NULL, NULL, FALSE);
	}
	else
	{
		WxKismet::OpenKismet( NULL, FALSE, GApp->EditorFrame );
	}
}



/** Called when 'UnrealMatinee' is clicked in the main menu */
void WxEditorFrame::MenuOpenMatinee( wxCommandEvent& In )
{
	// Open Matinee!  If there's only one Matinee available to open this will just go ahead and do it, otherwise
	// a menu object will be returned that we'll have to display
	wxMenu* MenuToDisplay = GApp->EditorFrame->OpenMatineeOrBuildMenu();
	if( MenuToDisplay != NULL )
	{
		// Display the menu directly below the mouse cursor
		wxPoint MouseLocalPosition = ScreenToClient( ::wxGetMousePosition() );
		PopupMenu( MenuToDisplay, MouseLocalPosition.x, MouseLocalPosition.y );
	}
}

void WxEditorFrame::MenuOpenSentinel( wxCommandEvent& In )
{
#if !UDK
	check(GApp);

	UBOOL bShowSentinel = In.IsChecked();

	if(bShowSentinel && !GApp->SentinelTool)
	{
		GApp->SentinelTool = new WxSentinel( this );
		if ( GApp->SentinelTool->ConnectToDatabase() )
		{
			GApp->SentinelTool->Show();

			MainMenuBar->Check(IDM_OPEN_SENTINEL, TRUE);
			MainToolBar->ToggleTool(IDM_OPEN_SENTINEL, TRUE);
		}
		else
		{
			GApp->SentinelTool->Close();
		}
	}
	else if(!bShowSentinel && GApp->SentinelTool)
	{
		GApp->SentinelTool->Close();
	}
#endif	//#if !UDK
}

void WxEditorFrame::SentinelClosed()
{
#if !UDK
	check(GApp);
	GApp->SentinelTool = NULL;

	MainMenuBar->Check(IDM_OPEN_SENTINEL, FALSE);
	MainToolBar->ToggleTool(IDM_OPEN_SENTINEL, FALSE);
#endif	//#if !UDK
}

void WxEditorFrame::LockReadOnlyLevels( const UBOOL bLock )
{
	wxMenuItem* pLockReadOnlyLevels = GetLockReadOnlyLevelsItem();
	if ( pLockReadOnlyLevels )
	{
		// Update the menu option to now display the opposite option...
		if ( bLock )
		{
			pLockReadOnlyLevels->SetItemLabel( *LocalizeUnrealEd("UnlockReadOnlyLevels") );
			pLockReadOnlyLevels->SetHelp( *LocalizeUnrealEd("MainMenu_UnlockReadOnlyLevels_ToolTip") );
		}
		else
		{
			pLockReadOnlyLevels->SetItemLabel( *LocalizeUnrealEd("LockReadOnlyLevels") );
			pLockReadOnlyLevels->SetHelp( *LocalizeUnrealEd("MainMenu_LockReadOnlyLevels_ToolTip") );
		}

	}
	GEngine->bLockReadOnlyLevels = bLock;

	WxLevelBrowser* LevelBrowser = GUnrealEd->GetBrowser<WxLevelBrowser>( TEXT("LevelBrowser") );
	if ( LevelBrowser )
	{
		// Update all the levels locked icons
		LevelBrowser->UpdateUIForLevel( NULL );
	}
}

void WxEditorFrame::MenuActorFindInKismet( wxCommandEvent& In )
{
	AActor* FindActor = GEditor->GetSelectedActors()->GetTop<AActor>();
	if(FindActor)
	{
		WxKismet::FindActorReferences(FindActor);
	}
}

void WxEditorFrame::MenuActorProperties( wxCommandEvent& In )
{
	GUnrealEd->ShowActorProperties();
}

void WxEditorFrame::MenuFileCreateArchetype( wxCommandEvent& In )
{
	GEditor->edactArchetypeSelected();
}

void WxEditorFrame::MenuMakeSelectedActorsLevelCurrent( wxCommandEvent& In )
{
	GUnrealEd->MakeSelectedActorsLevelCurrent();
}

void WxEditorFrame::MenuMakeSelectedActorsLevelGridVolumeCurrent( wxCommandEvent& In )
{
	GUnrealEd->MakeSelectedActorsLevelGridVolumeCurrent();
}

void WxEditorFrame::MenuMoveSelectedActorsToCurrentLevel( wxCommandEvent& In )
{
	const UBOOL bUseCurrentLevelGridVolume = TRUE;
    GEditor->MoveSelectedActorsToCurrentLevel( bUseCurrentLevelGridVolume );
}

void WxEditorFrame::MenuSelectLevelInLevelBrowser( wxCommandEvent& In )
{
	WxLevelBrowser* LevelBrowser = GUnrealEd->GetBrowser<WxLevelBrowser>( TEXT("LevelBrowser") );
	if ( LevelBrowser != NULL )
	{
		for ( USelection::TObjectIterator Itor = GEditor->GetSelectedActors()->ObjectItor() ; Itor ; ++Itor )
		{
			AActor* Actor = Cast<AActor>( *Itor );
			if ( Actor )
			{
				ULevel* ActorLevel = Actor->GetLevel();
				LevelBrowser->SelectLevelItem( FLevelBrowserItem( ActorLevel ) );
			}
		}

		// Make sure the window is visible.
		GUnrealEd->GetBrowserManager()->ShowWindow( LevelBrowser->GetDockID(), TRUE );
	}
}


/**
 * Opens the Level Browser and selects all levels associated with the currently-selected level streaming/grid volumes
 */
void WxEditorFrame::MenuFindStreamingVolumeLevelsInLevelBrowser( wxCommandEvent& In )
{
	WxLevelBrowser* LevelBrowser = GUnrealEd->GetBrowser<WxLevelBrowser>( TEXT("LevelBrowser") );
	if ( LevelBrowser != NULL )
	{
		LevelBrowser->DeselectAllLevels();

		TArray< ULevel* > LevelsToSelect;
		for ( USelection::TObjectIterator Itor = GEditor->GetSelectedActors()->ObjectItor() ; Itor ; ++Itor )
		{
			// Level streaming volumes
			ALevelStreamingVolume* LevelStreamingVolume = Cast<ALevelStreamingVolume>( *Itor );
			if( LevelStreamingVolume != NULL )
			{
				for( INT CurLevelIndex = 0; CurLevelIndex < LevelStreamingVolume->StreamingLevels.Num(); ++CurLevelIndex )
				{
					ULevel* CurLevel = LevelStreamingVolume->StreamingLevels( CurLevelIndex )->LoadedLevel;
					if( CurLevel != NULL )
					{
						LevelsToSelect.AddUniqueItem( CurLevel );
					}
				}
			}

			// Level grid volumes
			ALevelGridVolume* LevelGridVolume = Cast<ALevelGridVolume>( *Itor );
			if( LevelGridVolume != NULL )
			{
				// Find all of the levels associated with this volume
				TArray< ULevelStreaming* > GridVolumeLevels;
				LevelGridVolume->GetLevelsForAllCells( GridVolumeLevels );	// Out
				for( INT CurLevelIndex = 0; CurLevelIndex < GridVolumeLevels.Num(); ++CurLevelIndex )
				{
					ULevel* CurLevel = GridVolumeLevels( CurLevelIndex )->LoadedLevel;
					if( CurLevel != NULL )
					{
						LevelsToSelect.AddUniqueItem( CurLevel );
					}
				}
			}
		}

		for( INT CurLevelIndex = 0; CurLevelIndex < LevelsToSelect.Num(); ++CurLevelIndex )
		{
			ULevel* CurLevel = LevelsToSelect( CurLevelIndex );
			LevelBrowser->SelectLevelItem( FLevelBrowserItem( CurLevel ) );
		}

		// Make sure the window is visible.
		GUnrealEd->GetBrowserManager()->ShowWindow( LevelBrowser->GetDockID(), TRUE );
	}
}


void WxEditorFrame::MenuSelectLevelOnlyInLevelBrowser( wxCommandEvent& In )
{
	WxLevelBrowser* LevelBrowser = GUnrealEd->GetBrowser<WxLevelBrowser>( TEXT("LevelBrowser") );
	if ( LevelBrowser != NULL )
	{
		LevelBrowser->DeselectAllLevels();

		for ( USelection::TObjectIterator Itor = GEditor->GetSelectedActors()->ObjectItor() ; Itor ; ++Itor )
		{
			AActor* Actor = Cast<AActor>( *Itor );
			if ( Actor )
			{
				ULevel* ActorLevel = Actor->GetLevel();
				LevelBrowser->SelectLevelItem( FLevelBrowserItem( ActorLevel ) );
			}
		}

		// Make sure the window is visible.
		GUnrealEd->GetBrowserManager()->ShowWindow( LevelBrowser->GetDockID(), TRUE );
	}
}

void WxEditorFrame::MenuDeselectLevelInLevelBrowser( wxCommandEvent& In )
{
	WxLevelBrowser* LevelBrowser = GUnrealEd->GetBrowser<WxLevelBrowser>( TEXT("LevelBrowser") );
	if ( LevelBrowser != NULL )
	{
		for ( USelection::TObjectIterator Itor = GEditor->GetSelectedActors()->ObjectItor() ; Itor ; ++Itor )
		{
			AActor* Actor = Cast<AActor>( *Itor );
			if ( Actor )
			{
				ULevel* ActorLevel = Actor->GetLevel();
				LevelBrowser->DeselectLevelItem( FLevelBrowserItem( ActorLevel ) );
			}
		}

		// Make sure the window is visible.
		GUnrealEd->GetBrowserManager()->ShowWindow( LevelBrowser->GetDockID(), TRUE );
	}
}



/**
 * Makes the selected level grid volume the "current" level grid volume
 */
void WxEditorFrame::MenuMakeLevelGridVolumeCurrent( wxCommandEvent& In )
{
	WxLevelBrowser* LevelBrowser = GUnrealEd->GetBrowser<WxLevelBrowser>( TEXT("LevelBrowser") );
	if ( LevelBrowser != NULL )
	{
		for ( USelection::TObjectIterator Itor = GEditor->GetSelectedActors()->ObjectItor() ; Itor ; ++Itor )
		{
			ALevelGridVolume* LevelGridVolume = Cast<ALevelGridVolume>( *Itor );
			if( LevelGridVolume != NULL )
			{
				LevelBrowser->MakeLevelGridVolumeCurrent( LevelGridVolume );
				break;
			}
		}
	}
}


/**
 * Clears the "current" level grid volume
 */
void WxEditorFrame::MenuClearCurrentLevelGridVolume( wxCommandEvent& In )
{
	WxLevelBrowser* LevelBrowser = GUnrealEd->GetBrowser<WxLevelBrowser>( TEXT("LevelBrowser") );
	if ( LevelBrowser != NULL )
	{
		if( GWorld->CurrentLevelGridVolume != NULL )
		{
			LevelBrowser->MakeLevelGridVolumeCurrent( NULL );
		}
	}
}


void WxEditorFrame::MenuSurfaceProperties( wxCommandEvent& In )
{
	WxDlgSurfaceProperties* SurfacePropertiesWindow = GApp->GetDlgSurfaceProperties();
	check(SurfacePropertiesWindow);
	SurfacePropertiesWindow->Show( 1 );
}

void WxEditorFrame::MenuWorldProperties( wxCommandEvent& In )
{
	GUnrealEd->ShowWorldProperties();
}

void WxEditorFrame::MenuLightingResults( wxCommandEvent& In )
{
	WxDlgLightingResults* LightingResultsWindow = GApp->GetDlgLightingResults();
	check(LightingResultsWindow);
	LightingResultsWindow->Show(1);
}

void WxEditorFrame::MenuLightingBuildInfo( wxCommandEvent& In )
{
	WxDlgLightingBuildInfo* LightingBuildInfoWindow = GApp->GetDlgLightingBuildInfo();
	check(LightingBuildInfoWindow);
	LightingBuildInfoWindow->Show(1);
}

void WxEditorFrame::MenuLightingStaticMeshInfo( wxCommandEvent& In )
{
	WxDlgStaticMeshLightingInfo* StaticMeshLightingInfoWindow = GApp->GetDlgStaticMeshLightingInfo();
	check(StaticMeshLightingInfoWindow);
	StaticMeshLightingInfoWindow->Show(1);
}

void WxEditorFrame::MenuLightMapDensityRenderingOptions( wxCommandEvent& In )
{
	WxDlgDensityRenderingOptions* DensityRenderingOptionsWindow = GApp->GetDlgDensityRenderingOptions();
	check(DensityRenderingOptionsWindow);
	DensityRenderingOptionsWindow->Show(true);
}

void WxEditorFrame::MenuLightMapResolutionRatioAdjust( wxCommandEvent& In )
{
#if WITH_MANAGED_CODE
	static FLightmapResRatioWindow* LightmapResRatioWindow = NULL;
	if (!LightmapResRatioWindow)
	{
		// Create the Lightmap Res Ratio window
		HWND EditorFrameWindowHandle = (HWND)GApp->EditorFrame->GetHandle();

		LightmapResRatioWindow = FLightmapResRatioWindow::CreateLightmapResRatioWindow(EditorFrameWindowHandle);
		check(LightmapResRatioWindow);
		LightmapResRatioWindow->ShowWindow(FALSE);
	}

	LightmapResRatioWindow->ShowWindow(true);
#endif	//#if WITH_MANAGED_CODE
}

PRAGMA_DISABLE_OPTIMIZATION
void WxEditorFrame::MenuLightingTools( wxCommandEvent& In )
{
#if WITH_MANAGED_CODE
	static FLightingToolsWindow* LightingToolsWindow = NULL;
	if (!LightingToolsWindow)
	{
		// Create the Lighting Tools window
		HWND EditorFrameWindowHandle = (HWND)GApp->EditorFrame->GetHandle();

		LightingToolsWindow = FLightingToolsWindow::CreateLightingToolsWindow(EditorFrameWindowHandle);
		check(LightingToolsWindow);
		LightingToolsWindow->ShowWindow(FALSE);
	}

	LightingToolsWindow->ShowWindow(true);
#endif	//#if WITH_MANAGED_CODE
}
PRAGMA_ENABLE_OPTIMIZATION

void WxEditorFrame::MenuBrushCSG( wxCommandEvent& In )
{
	switch( In.GetId() )
	{
		case IDM_BRUSH_ADD:				GUnrealEd->Exec( TEXT("BRUSH ADD") );					break;
		case IDM_BRUSH_SUBTRACT:		GUnrealEd->Exec( TEXT("BRUSH SUBTRACT") );				break;
		case IDM_BRUSH_INTERSECT:		GUnrealEd->Exec( TEXT("BRUSH FROM INTERSECTION") );		break;
		case IDM_BRUSH_DEINTERSECT:		GUnrealEd->Exec( TEXT("BRUSH FROM DEINTERSECTION") );	break;
	}

	GUnrealEd->RedrawLevelEditingViewports();
}

extern class WxDlgAddSpecial* GDlgAddSpecial;

void WxEditorFrame::MenuBrushAddSpecial( wxCommandEvent& In )
{
	GDlgAddSpecial->Show();
}

void WxEditorFrame::MenuBuildPlayInEditor( wxCommandEvent& In )
{
	GUnrealEd->PlayMap();
}

void WxEditorFrame::MenuBuildPlayInActiveViewport( wxCommandEvent& In )
{
	FVector* StartLocation = NULL;
	FRotator* StartRotation = NULL;
	INT MyViewportIndex = -1;
	// Figure out which viewport index we are
	for( INT CurViewportIndex = 0; CurViewportIndex < ViewportConfigData->GetViewportCount(); ++CurViewportIndex )
	{
		FVCD_Viewport& CurViewport = ViewportConfigData->AccessViewport( CurViewportIndex );
		if( CurViewport.bEnabled && CurViewport.ViewportWindow == GCurrentLevelEditingViewportClient)
		{
			// If this is a perspective viewport, then we'll Play From Here
			if( CurViewport.ViewportWindow->ViewportType == LVT_Perspective )
			{
				// Start PIE from the camera's location and orientation!
				StartLocation = &CurViewport.ViewportWindow->ViewLocation;
				StartRotation = &CurViewport.ViewportWindow->ViewRotation;
			}
			MyViewportIndex = CurViewportIndex;
			break;
		}
	}
	GUnrealEd->PlayMap( StartLocation, StartRotation, -1, MyViewportIndex );
}

void WxEditorFrame::MenuBuildPlayOnConsole( wxCommandEvent& In )
{
#if !CONSOLE
	// If shift is held down, launch the UFE with the current map info...
	if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
	{
		MenuBuildCookForConsole( In );
	}
	else
#endif
	{
		const UBOOL bUseMobilePreview = FALSE;
		GUnrealEd->PlayMap(NULL, NULL, In.GetId() - IDM_BuildPlayConsole_START, INDEX_NONE, bUseMobilePreview);
	}
}

void WxEditorFrame::MenuBuildPlayUsingMobilePreview( wxCommandEvent& In )
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
		const UBOOL bUseMobilePreview = TRUE;
		GUnrealEd->PlayMap(NULL, NULL, PCSupportContainerIndex, INDEX_NONE, bUseMobilePreview);
	}
}


void WxEditorFrame::MenuBuildCookForConsole( wxCommandEvent& In )
{
#if !CONSOLE
	// Check to see if the UnrealFrontend binary exists
	const FFilename ExecutableFileName( FString( appBaseDir() ) + FString( TEXT( "..\\UnrealFrontend.exe" ) ) );
	if( ExecutableFileName.FileExists() )
	{
		// Make sure all the packages have been saved
		if( FEditorFileUtils::SaveDirtyPackages( FALSE, TRUE, TRUE ) )	// can't use SavePlayWorldPackages as the cooker doesn't recognize the Autosaves directory!
		{
			// Make sure the map name exists, if it hasn't, it hasn't been saved (could be a new map/template)
			FString MapFileName;
			const FString MapName = GWorld->GetOutermost()->GetName();
			const UBOOL bMapFileExists = GPackageFileCache->FindPackageFile( *MapName, NULL, MapFileName );
			if( bMapFileExists )
			{
				// Create a temporary XML based on the current map settings and the button that was pressed		
				TCHAR TempXMLName[ MAX_SPRINTF ];
				appCreateTempFilename( TEXT( "" ), *MapName, TEXT( ".xml" ), TempXMLName, MAX_SPRINTF );
				const FString XMLName( TempXMLName );

				FFilename XMLFileName;
				if ( MenuCreateXMLForUFE( In, MapName, XMLName, XMLFileName ) )
				{ 
					/*
						Launch the UFE with the new XML as a param
						#add /profile=filename command line argument selects a profile xml file in the UnrealFrontend.Profiles folder for the lifetime of the app instance. Profile selection panel becomes locked and cannot be changed by the user.
						#add /autostart command line flag argument executes a full profile run when the app starts. The equivalent of the user clicking the Start button.
						#add /autoquit command line argument closes the app when a profile run initiated using the /autostart flag completes.
					*/
					FString CmdLine = FString::Printf( TEXT( "/profile=%s " ), *XMLName );
					FString CmdLineParams = TEXT( "/autostart /autoquit" );	// default params
					GConfig->GetString( TEXT("UFELaunch"), TEXT("CmdLineParams"), CmdLineParams, GEditorUserSettingsIni );
					CmdLine += CmdLineParams;
					void* ProcHandle = appCreateProc( *ExecutableFileName, *CmdLine, TRUE, FALSE );
					if( ProcHandle != NULL )
					{
						// Add the proc to our list to track to see when it's completed.
						UFECookInfo.AddItem( FUFECookInfo( ProcHandle, XMLFileName ) );
					}
					else
					{
						// Delete the xml file, it's not used as the proc failed
						if( XMLFileName.FileExists() )
						{
							GFileManager->Delete( *XMLFileName );
						}

						// FAILED TO LAUNCH UFE
						appMsgf( AMT_OK, *LocalizeUnrealEd( "UEFError_FailedToLaunchUFE" ) );
					}
				}
				else
				{
					// FAILED TO CREATE XML
					appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "UEFError_FailedToCreateXML" ), *XMLFileName ) ) );
				}
			}
			else
			{
				// FAILED TO FIND MAP
				appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "UEFError_FailedToFindMap" ), *MapName ) ) );
			}
		}
		else
		{
			// FAILED TO SAVE
			appMsgf( AMT_OK, *LocalizeUnrealEd( "UEFError_FailedToSave" ) );
		}
	}
	else
	{
		// FAILED TO FIND UFE
		appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "UEFError_FailedToFindUFE" ), *ExecutableFileName ) ) );
	}
#endif
}

UBOOL WxEditorFrame::MenuCreateXMLForUFE( wxCommandEvent& In, const FString& MapNameIn, const FString& XMLNameIn, FFilename& XMLFileNameOut ) const
{
#if !CONSOLE
	FConsoleSupport* Console = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport( In.GetId() - IDM_BuildPlayConsole_START );
	const FString Game( Console->GetGameName() );
	const FString Platform( Console->GetPlatformName() );
	const FConsoleSupport::EPlatformType PlatformType( Console->GetPlatformType() );

	FString GamePostfix;
	switch( PlatformType )
	{
	case FConsoleSupport::EPlatformType_Windows:
	case FConsoleSupport::EPlatformType_WindowsConsole:
	case FConsoleSupport::EPlatformType_WindowsServer:
#if _WIN64																							// Can be ReleaseX, DebugX, ShippingX (X = _32 or _64)
		GamePostfix = TEXT( "_64" );
#else
		GamePostfix = TEXT( "_32" );
#endif
		break;
	case FConsoleSupport::EPlatformType_MacOSX:
		GamePostfix = TEXT( "_64" );												// Can be ReleaseX, DebugX, ShippingX (X = _64)
		break;
	default:
		GamePostfix = TEXT( "_32" );												// Can be ReleaseX, DebugX, ShippingX, TestX (X = _32)
		break;
	}				
	const FString ScriptPostfix = TEXT( "Script" );				// Can be ReleaseX, DebugX, FinalReleaseX (X = Script)
#if _WIN64																							// Can be ReleaseX, DebugX, ShippingX* (X = _32 or _64)	*Not Xbox360 or PS3
	const FString CookMakePostfix = TEXT( "_64" );
#else
	const FString CookMakePostfix = TEXT( "_32" );
#endif

	const FString Config( Console->GetConfiguration() );	// Make sure we have valid configs
	FString GameConfig( Config ), ScriptConfig( Config ), CookMakeConfig( Config );
	if ( GameConfig == TEXT( "Test" ) )
	{
		CookMakeConfig = ScriptConfig = TEXT( "Debug" );	// No Test Script/Cook/Make option, use debug
	}
	else if ( GameConfig == TEXT( "Shipping" ) )
	{
		ScriptConfig = TEXT( "FinalRelease" );
		if ( ( PlatformType == FConsoleSupport::EPlatformType_Xbox360 ) || ( PlatformType == FConsoleSupport::EPlatformType_PS3 ) )
		{
			CookMakeConfig = TEXT( "Release" );	// No Shipping Cook/Make option, use release
		}
	}
	else if ( GameConfig != TEXT( "Release" ) && GameConfig != TEXT( "Debug" ) )
	{
		CookMakeConfig = ScriptConfig = GameConfig = TEXT( "Release" );	// Just in-case the Config isn't any of the expected types, default to Release for all
	}
	GameConfig += GamePostfix;
	ScriptConfig += ScriptPostfix;
	CookMakeConfig += CookMakePostfix;

	// Allow the user to override the construction of the config types by specifying them explicitly in their ini, like; Shipping_64, ReleaseScript, Debug_32 (if they wanted!)
	GConfig->GetString( TEXT("UFELaunch"), TEXT("GameConfig"), GameConfig, GEditorUserSettingsIni );
	GConfig->GetString( TEXT("UFELaunch"), TEXT("ScriptConfig"), ScriptConfig, GEditorUserSettingsIni );
	GConfig->GetString( TEXT("UFELaunch"), TEXT("CookMakeConfig"), CookMakeConfig, GEditorUserSettingsIni );

	// Make sure the folder to hold the profile exists
	FString XMLFolderName = FString( appBaseDir() ) + FString( TEXT( "..\\UnrealFrontend.Profiles\\" ) );
	if ( !GFileManager->MakeDirectory( *XMLFolderName ) )
	{
		return FALSE;
	}
	XMLFileNameOut = XMLFolderName + *XMLNameIn;

	TiXmlDocument Doc;
	TiXmlDeclaration* pDec = new TiXmlDeclaration( "1.0", "utf-16", "" );
	Doc.LinkEndChild( pDec );
	TiXmlElement* pProfileData = new TiXmlElement( "ProfileData" );
	Doc.LinkEndChild( pProfileData );
	{
		TiXmlElement* pPipeline = new TiXmlElement( "mPipeline" );
		pProfileData->LinkEndChild( pPipeline );
		{
			TiXmlElement* pTargetPlatformType = new TiXmlElement( "TargetPlatformType" );
			pTargetPlatformType->LinkEndChild( new TiXmlText( TCHAR_TO_ANSI( *Platform ) ) );
			pPipeline->LinkEndChild( pTargetPlatformType );
		}
		TiXmlElement* pCooking_MapsToCook = new TiXmlElement( "mCooking_MapsToCook" );
		pProfileData->LinkEndChild( pCooking_MapsToCook );
		{
			TiXmlElement* pUnrealMap = new TiXmlElement( "UnrealMap" );
			pCooking_MapsToCook->LinkEndChild( pUnrealMap );
			{
				TiXmlElement* pName = new TiXmlElement( "Name" );
				pName->LinkEndChild( new TiXmlText( TCHAR_TO_ANSI( *MapNameIn ) ) );
				pUnrealMap->LinkEndChild( pName );
			}
		}
		TiXmlElement* pMapToPlay = new TiXmlElement( "mMapToPlay" );
		pProfileData->LinkEndChild( pMapToPlay );
		{
			TiXmlElement* pName = new TiXmlElement( "Name" );
			pName->LinkEndChild( new TiXmlText( TCHAR_TO_ANSI( *MapNameIn ) ) );
			pMapToPlay->LinkEndChild( pName );
		}
		TiXmlElement* pLaunchConfiguration = new TiXmlElement( "mLaunchConfiguration" );
		pLaunchConfiguration->LinkEndChild( new TiXmlText( TCHAR_TO_ANSI( *GameConfig ) ) );
		pProfileData->LinkEndChild( pLaunchConfiguration );
		TiXmlElement* pCommandletConfiguration = new TiXmlElement( "mCommandletConfiguration" );
		pCommandletConfiguration->LinkEndChild( new TiXmlText( TCHAR_TO_ANSI( *CookMakeConfig ) ) );
		pProfileData->LinkEndChild( pCommandletConfiguration );
		TiXmlElement* pScriptConfiguration = new TiXmlElement( "mScriptConfiguration" );
		pScriptConfiguration->LinkEndChild( new TiXmlText( TCHAR_TO_ANSI( *ScriptConfig ) ) );
		pProfileData->LinkEndChild( pScriptConfiguration );
		TiXmlElement* pSelectedGameName = new TiXmlElement( "mSelectedGameName" );
		pSelectedGameName->LinkEndChild( new TiXmlText( TCHAR_TO_ANSI( *Game ) ) );
		pProfileData->LinkEndChild( pSelectedGameName );
		TiXmlElement* pTargetPlatformType = new TiXmlElement( "mTargetPlatformType" );
		pTargetPlatformType->LinkEndChild( new TiXmlText( TCHAR_TO_ANSI( *Platform ) ) );
		pProfileData->LinkEndChild( pTargetPlatformType );
	}
	if( Doc.SaveFile( TCHAR_TO_ANSI(*XMLFileNameOut) ) )
	{
		return TRUE;
	}
#endif
	return FALSE;
}

void WxEditorFrame::MenuUpdateUFEProcs()
{
#if !CONSOLE
	// Check to see if any of the UFE procs have completed
	for ( INT i = 0; i < UFECookInfo.Num(); i++ )
	{
		FUFECookInfo& rInfo = UFECookInfo( i );
		check( rInfo.ProcHandle );

		INT ReturnCode = 1;
		UBOOL bComplete = appGetProcReturnCode( rInfo.ProcHandle, &ReturnCode );
		if( bComplete )
		{
			if ( ReturnCode != 0 )
			{
				// UFE FAILED
				appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "UEFError_UFEFailed" ), ReturnCode ) ) );
			}

			// Delete the xml file, it's no longer needed
			if( rInfo.Filename.FileExists() )
			{
				GFileManager->Delete( *rInfo.Filename );
			}

			UFECookInfo.Remove( i-- );
		}
	}
#endif
}

void WxEditorFrame::MenuConsoleSpecific( wxCommandEvent& In )
{
	// Get the index, with the first one possible at 0
	const INT Index = In.GetId() - IDM_ConsoleSpecific_START;
	// There are 40 items per console, so every 40 items is a new console
	const INT Console = Index / MAX_CONSOLES_TO_DISPLAY_IN_MENU;
	// Use mod to get the index inside the console
	const INT MenuItem = Index % MAX_CONSOLES_TO_DISPLAY_IN_MENU;

	if(Console >= 0 && Console < FConsoleSupportContainer::GetConsoleSupportContainer()->GetNumConsoleSupports())
	{
		// Let the plug-in manage it
		TCHAR OutputConsoleCommand[1024] = TEXT("\0");
		FConsoleSupport *Platform = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(Console);
		Platform->ProcessMenuItem(MenuItem, OutputConsoleCommand, sizeof(OutputConsoleCommand));

		bool bIsChecked = false;
		bool bIsRadio = false;
		TARGETHANDLE Handle;

		Platform->GetMenuItem(MenuItem, bIsChecked, bIsRadio, Handle);

		if(bIsRadio)
		{
			if(bIsChecked)
			{
				GObjectPropagator->AddTarget(Handle, htonl(Platform->GetIPAddress(Handle)), Platform->GetIntelByteOrder());
			}
			else
			{
				GObjectPropagator->RemoveTarget(Handle);
			}
		}

		// if the meun item needs to exec a command, do it now
		if (OutputConsoleCommand[0] != 0)
		{
			// handle spcial QUERYVALUE command
			UBOOL bIsQueryingValue = appStrnicmp(OutputConsoleCommand, TEXT("QUERYVALUE"), 10) == 0;
			FStringOutputDevice QueryOutputDevice;

			FOutputDevice& Ar = bIsQueryingValue ? (FOutputDevice&)QueryOutputDevice : (FOutputDevice&)*GLog;
			GEngine->Exec(OutputConsoleCommand, Ar);
			if (bIsQueryingValue)
			{
				FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(Console)->SetValueCallback(*QueryOutputDevice);
			}
		}
	}
}

void WxEditorFrame::UpdateUIConsoleSpecific(wxUpdateUIEvent& In)
{
	// Get the index, with the first one possible at 0
	const INT Index = In.GetId() - IDM_ConsoleSpecific_START;
	// There are MAX_CONSOLES_TO_DISPLAY_IN_MENU items per console, so every MAX_CONSOLES_TO_DISPLAY_IN_MENU items is a new console
	const INT Console = Index / MAX_CONSOLES_TO_DISPLAY_IN_MENU;
	// Use mod to get the index inside the console
	const INT MenuItem = Index % MAX_CONSOLES_TO_DISPLAY_IN_MENU;

	// reget the menu item text, in case it changed by selecting it
	bool bIsChecked = false;
	bool bIsRadio = false;
	TARGETHANDLE Handle;

	if(Console >= 0 && Console < FConsoleSupportContainer::GetConsoleSupportContainer()->GetNumConsoleSupports())
	{
		const TCHAR* MenuLabel = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(Console)->GetMenuItem(MenuItem, bIsChecked, bIsRadio, Handle);
		MainMenuBar->ConsoleMenu[Console]->SetLabel(IDM_ConsoleSpecific_START + Console * MAX_CONSOLES_TO_DISPLAY_IN_MENU + MenuItem, MenuLabel);

		// update checked status
		In.Check(bIsChecked);
	}
}

void WxEditorFrame::OnMenuOpen(wxMenuEvent& In)
{
	wxMenu* EventMenu = In.GetMenu();
	if (EventMenu)
	{
		check(MainMenuBar);

		// Check if this is one of the console menus
		INT ConsoleIndex = 0;
		for (FConsoleSupportIterator It; It && ConsoleIndex < 20; ++It, ConsoleIndex++)
		{
			// If this console has any menu items, add the menu
			if (It->GetNumMenuItems() > 0)
			{
				//first time this menu has been seen
				if ((EventMenu == MainMenuBar->ConsoleMenu[ConsoleIndex]) && (EventMenu->GetMenuItemCount()==0))
				{
					// Add all the items (max out at 40 items)
					for (INT ItemIndex = 0; ItemIndex < It->GetNumMenuItems() && ConsoleIndex < 40; ItemIndex++)
					{
						bool bIsChecked, bIsRadio;
						TARGETHANDLE Handle;

						const TCHAR* MenuLabel = It->GetMenuItem(ItemIndex, bIsChecked, bIsRadio, Handle);
						if (MenuLabel != NULL)
						{
							// @todo: when wx supports radio menu items properly, use this for better feedback to user
							// wxItemKind Kind = bIsRadio ? wxITEM_RADIO : wxITEM_CHECK;
							wxItemKind Kind = wxITEM_CHECK;

							// if it's the special separator text, append a separator, not a real menu item
							if (appStricmp(MenuLabel, MENU_SEPARATOR_LABEL) == 0)
							{
								EventMenu->AppendSeparator();
							}
							else
							{
								EventMenu->Append(IDM_ConsoleSpecific_START + ConsoleIndex * MAX_CONSOLES_TO_DISPLAY_IN_MENU + ItemIndex, MenuLabel, TEXT(""), Kind);
								EventMenu->Check(IDM_ConsoleSpecific_START + ConsoleIndex * MAX_CONSOLES_TO_DISPLAY_IN_MENU + ItemIndex, bIsChecked);
							}
						}
					}
				}
			}
		}
	}
	//let the standard updates take affect
	In.Skip(true);
}


void WxEditorFrame::ObjectPropagationSelChanged( wxCommandEvent& In )
{
	// @todo: Currently this feature is disabled in the editor UI

	// TRUE if propagation destination is 'None'
	const UBOOL bTargetNotNone = In.GetInt() != OPD_None;

	// Enable/disable the ViewPush button based on whether the object propagation destination is 'None'.
	MainToolBar->EnablePushView( bTargetNotNone );

	if ( bTargetNotNone )
	{
		
		FString Prompt = TEXT("Live Update/Object Propagation supports the following:\n");
		Prompt += TEXT(" Basic:\n");
		Prompt += TEXT("   - Moving and deleting non-light actors\n");
		Prompt += TEXT(" Creation:\n");
		Prompt += TEXT("   - Actor duplication and copy & paste of valid actors\n");
		Prompt += TEXT("   - Creation of StaticMesh, SkeletalMesh, and AmbientSound actors (ONLY when using loaded content)\n");
		Prompt += TEXT(" Properties:\n");
		Prompt += TEXT("   - Changes made in actor/component property dialogs (other property dialogs _may_ work)\n\n");
		Prompt += TEXT("The following will not work:\n");
		Prompt += TEXT(" - Moving/deleting/modifying lights\n");
		Prompt += TEXT(" - Making references to unloaded (on the console) content\n");
		Prompt += TEXT(" - Modifying content (textures, materials, etc)\n\n\n");
		Prompt += TEXT("Note: Xbox 360's will need secure networking disabled! (bUseVDP = false)");
		FString Title = TEXT("Live Update support information");
		FString IniSetting = TEXT("ObjectPropagationPrompt");
		WxSuppressableWarningDialog Message( Prompt, Title, IniSetting );
		Message.ShowModal();
	}

	// let the editor handle the logic
	GEditor->SetObjectPropagationDestination(In.GetInt());
}

void WxEditorFrame::MenuBuild( wxCommandEvent& In )
{
	FEditorBuildUtils::EditorBuild( In.GetId() );
}

/**
 * Called when the user selects the option to build all and submit
 *
 * @param	In	Event automatically generated by wxWidgets when the user selects the option to build all and submit
 */
void WxEditorFrame::MenuBuildAndSubmit( wxCommandEvent& In )
{
#if HAVE_SCC && WITH_MANAGED_CODE
	BuildWindows::PromptForBuildAndSubmit( &FContentBrowser::GetActiveInstance() );
#endif // #if HAVE_SCC && WITH_MANAGED_CODE
}

/**
 * Update to display a check beside the active RHI mode
 *
 * @param	In	Event automatically generated by wxWidgets for control to update
 */
void WxEditorFrame::UI_MenuSelectEditorMode( wxUpdateUIEvent& In )
{
	// Display a check beside the selected context menu
	switch( In.GetId() )
	{
		case IDM_EDITOR_PREFERENCES_DX9_MODE:
			In.Check( GRenderMode == RENDER_MODE_DX9 );
			return;

		case IDM_EDITOR_PREFERENCES_DX11_MODE:
			In.Check( GRenderMode == RENDER_MODE_DX11 );
			return;
	}
}

void WxEditorFrame::UI_MenuViewportConfig( wxUpdateUIEvent& In )
{
	switch( In.GetId() )
	{
		case IDM_VIEWPORT_CONFIG_2_2_SPLIT:		In.Check( ViewportConfigData->Template == VC_2_2_Split );			break;
		case IDM_VIEWPORT_CONFIG_1_2_SPLIT:		In.Check( ViewportConfigData->Template == VC_1_2_Split );			break;
		case IDM_VIEWPORT_CONFIG_1_1_SPLIT_H:		In.Check( ViewportConfigData->Template == VC_1_1_SplitH );			break;
		case IDM_VIEWPORT_CONFIG_1_1_SPLIT_V:		In.Check( ViewportConfigData->Template == VC_1_1_SplitV );			break;
	}
}

void WxEditorFrame::UI_MenuEditUndo( wxUpdateUIEvent& In )
{
	In.Enable( GUnrealEd->Trans->CanUndo() == TRUE );
	In.SetText( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("Undo_FA"), *GUnrealEd->Trans->GetUndoDesc()) ) );

}

void WxEditorFrame::UI_MenuEditRedo( wxUpdateUIEvent& In )
{
	In.Enable( GUnrealEd->Trans->CanRedo() == TRUE );
	In.SetText( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("Redo_FA"), *GUnrealEd->Trans->GetRedoDesc()) ) );

}

void WxEditorFrame::UI_MenuEditShowWidget( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools().GetShowWidget() == TRUE );
}

void WxEditorFrame::UI_MenuEditMouseLock( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools().GetMouseLock() == TRUE );
}

void WxEditorFrame::UI_MenuEditTranslate( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools().GetWidgetMode() == FWidget::WM_Translate );
	In.Enable( GEditorModeTools().GetShowWidget() == TRUE );
}

void WxEditorFrame::UI_MenuEditRotate( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools().GetWidgetMode() == FWidget::WM_Rotate );
	In.Enable( GEditorModeTools().GetShowWidget() == TRUE );
}

void WxEditorFrame::UI_MenuEditScale( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools().GetWidgetMode() == FWidget::WM_Scale );
	In.Enable( GEditorModeTools().GetShowWidget() == TRUE );
}

void WxEditorFrame::UI_MenuEditScaleNonUniform( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools().GetWidgetMode() == FWidget::WM_ScaleNonUniform );

	// Special handling

	switch( GEditorModeTools().GetWidgetMode() )
	{
		// Non-uniform scaling is only possible in local space

		case FWidget::WM_ScaleNonUniform:
			GEditorModeTools().CoordSystem = COORD_Local;
			MainToolBar->CoordSystemCombo->SetSelection( GEditorModeTools().CoordSystem );
			MainToolBar->CoordSystemCombo->Disable();
			break;

		default:
			MainToolBar->CoordSystemCombo->Enable();
			break;
	}

	In.Enable( GEditorModeTools().GetShowWidget() == TRUE );
}

void WxEditorFrame::UI_MenuViewDetailModeLow( wxUpdateUIEvent& In )
{
	In.Check( (GEditor->DetailMode == DM_Low) );
}

void WxEditorFrame::UI_MenuViewDetailModeMedium( wxUpdateUIEvent& In )
{
	In.Check( (GEditor->DetailMode == DM_Medium) );
}

void WxEditorFrame::UI_MenuViewDetailModeHigh( wxUpdateUIEvent& In )
{
	In.Check( (GEditor->DetailMode == DM_High) || (GEditor->DetailMode == DM_MAX) );
}

void WxEditorFrame::UI_MenuDragGrid( wxUpdateUIEvent& In )
{
	INT id = In.GetId();

	if( IDM_DRAG_GRID_TOGGLE == id )
	{
		In.Check( GUnrealEd->Constraints.GridEnabled );
	}
	else if( IDM_DRAG_GRID_SNAPSCALE == id )
	{
		In.Check( GUnrealEd->Constraints.SnapScaleEnabled );
	}
	else 
	{
		INT GridIndex;

		GridIndex = -1;

		if( id >= IDM_DRAG_GRID_1 && id <= IDM_DRAG_GRID_1024 )
		{
			GridIndex = id - IDM_DRAG_GRID_1 ;
		}
		else if( id >= ID_BackdropPopupGrid1 && id <= ID_BackdropPopupGrid1024 )
		{
			GridIndex = id - ID_BackdropPopupGrid1 ;
		}

		if( GridIndex >= 0 && GridIndex < FEditorConstraints::MAX_GRID_SIZES )
		{
			float GridSize;

			GridSize = GEditor->Constraints.GridSizes[GridIndex];
			In.SetText( *FString::Printf(TEXT("%g"), GridSize ) ) ;
			In.Check( GUnrealEd->Constraints.CurrentGridSz == GridIndex );
		}
	}
}

void WxEditorFrame::UI_MenuAngleSnapType( wxUpdateUIEvent& In )
{
	switch( In.GetId() )
	{
		case IDM_ANGLESNAPTYPE_ANGLE:	In.Check( GUnrealEd->Constraints.AngleSnapType == EST_ANGLE );			break;
		case IDM_ANGLESNAPTYPE_PER90:	In.Check( GUnrealEd->Constraints.AngleSnapType == EST_PER90 );			break;
		case IDM_ANGLESNAPTYPE_PER360:	In.Check( GUnrealEd->Constraints.AngleSnapType == EST_PER360 );			break;
	}
	RotationGridMenu->BuildMenu();
}

void WxEditorFrame::UI_MenuRotationGrid( wxUpdateUIEvent& In )
{
	switch( In.GetId() )
	{
		case IDM_ROTATION_GRID_TOGGLE:	In.Check( GUnrealEd->Constraints.RotGridEnabled );					break;
		case IDM_ROTATION_GRID_512:		In.Check( GUnrealEd->Constraints.RotGridSize.Pitch == 512 );		break;
		case IDM_ROTATION_GRID_1024:	In.Check( GUnrealEd->Constraints.RotGridSize.Pitch == 1024 );		break;
		case IDM_ROTATION_GRID_2048:	In.Check( GUnrealEd->Constraints.RotGridSize.Pitch == 2048 );		break;
		case IDM_ROTATION_GRID_4096:	In.Check( GUnrealEd->Constraints.RotGridSize.Pitch == 4096 );		break;
		case IDM_ROTATION_GRID_8192:	In.Check( GUnrealEd->Constraints.RotGridSize.Pitch == 8192 );		break;
		case IDM_ROTATION_GRID_16384:	In.Check( GUnrealEd->Constraints.RotGridSize.Pitch == 16384 );		break;
	}
}

void WxEditorFrame::UI_MenuScaleGrid( wxUpdateUIEvent& In )
{
	switch( In.GetId() )
	{
		case IDM_DRAG_GRID_SNAPSCALE:	In.Check( GUnrealEd->Constraints.SnapScaleEnabled );	break;
		case IDM_SCALE_GRID_001:	In.Check( GUnrealEd->Constraints.ScaleGridSize == 1 );		break;
		case IDM_SCALE_GRID_002:	In.Check( GUnrealEd->Constraints.ScaleGridSize == 2 );		break;
		case IDM_SCALE_GRID_005:	In.Check( GUnrealEd->Constraints.ScaleGridSize == 5 );		break;
		case IDM_SCALE_GRID_010:	In.Check( GUnrealEd->Constraints.ScaleGridSize == 10 );		break;
		case IDM_SCALE_GRID_025:	In.Check( GUnrealEd->Constraints.ScaleGridSize == 25 );		break;
		case IDM_SCALE_GRID_050:	In.Check( GUnrealEd->Constraints.ScaleGridSize == 50 );		break;
	}
}

void WxEditorFrame::MenuDragGrid( wxCommandEvent& In )
{
	INT id = In.GetId();

	if( IDM_DRAG_GRID_TOGGLE == id )
	{	
		GUnrealEd->Exec( *FString::Printf( TEXT("MODE GRID=%d"), !GUnrealEd->Constraints.GridEnabled ? 1 : 0 ) );	
	}
	else 
	{
		INT GridIndex;

		GridIndex = -1;

		if( id >= IDM_DRAG_GRID_1 && id <= IDM_DRAG_GRID_1024 )
		{
			GridIndex = id - IDM_DRAG_GRID_1;
		}
		else if( id >= ID_BackdropPopupGrid1 && id <= ID_BackdropPopupGrid1024 )
		{
			GridIndex = id - ID_BackdropPopupGrid1;
		}

		if( GridIndex >= 0 && GridIndex < FEditorConstraints::MAX_GRID_SIZES )
		{
			GEditor->Constraints.SetGridSz( GridIndex );
		}
	}
}

void WxEditorFrame::MenuRotationGrid( wxCommandEvent& In )
{
	INT Angle = 0;
	switch( In.GetId() )
	{
		case IDM_ROTATION_GRID_512:			Angle = 512;		break;
		case IDM_ROTATION_GRID_1024:		Angle = 1024;		break;
		case IDM_ROTATION_GRID_2048:		Angle = 2048;		break;
		case IDM_ROTATION_GRID_4096:		Angle = 4096;		break;
		case IDM_ROTATION_GRID_8192:		Angle = 8192;		break;
		case IDM_ROTATION_GRID_16384:		Angle = 16384;		break;
	}

	switch( In.GetId() )
	{
		case IDM_ROTATION_GRID_TOGGLE:
			GUnrealEd->Exec( *FString::Printf( TEXT("MODE ROTGRID=%d"), !GUnrealEd->Constraints.RotGridEnabled ? 1 : 0 ) );
			break;

		case IDM_ROTATION_GRID_512:
		case IDM_ROTATION_GRID_1024:
		case IDM_ROTATION_GRID_2048:
		case IDM_ROTATION_GRID_4096:
		case IDM_ROTATION_GRID_8192:
		case IDM_ROTATION_GRID_16384:
			GUnrealEd->Exec( *FString::Printf( TEXT("MAP ROTGRID PITCH=%d YAW=%d ROLL=%d"), Angle, Angle, Angle ) );
			break;
	}
}

void WxEditorFrame::MenuAngleSnapType( wxCommandEvent& In )
{
	INT AngleSnapType = EST_ANGLE;
	switch( In.GetId() )
	{
		case IDM_ANGLESNAPTYPE_ANGLE:		AngleSnapType = EST_ANGLE;		break;
		case IDM_ANGLESNAPTYPE_PER90:		AngleSnapType = EST_PER90;		break;
		case IDM_ANGLESNAPTYPE_PER360:		AngleSnapType = EST_PER360;		break;
	}
	GUnrealEd->Exec( *FString::Printf( TEXT("MAP ANGLESNAPTYPE TYPE=%d"), AngleSnapType ) );
}

/**
 * Called in response to the user selecting an option from the autosave option menu. Sets the time interval between
 * autosaves or the package types to autosave.
 *
 * @param	In	Event generated by wxWidgets in response to the user selecting a menu option
 */
void WxEditorFrame::MenuAutoSaveOptions( wxCommandEvent& In )
{
	UEditorUserSettings& EditorUserSettings = GEditor->AccessUserSettings();

	switch( In.GetId() )
	{
	case IDM_AUTOSAVE_001:		EditorUserSettings.AutoSaveTimeMinutes = 1;				break;
	case IDM_AUTOSAVE_002:		EditorUserSettings.AutoSaveTimeMinutes = 5;				break;
	case IDM_AUTOSAVE_003:		EditorUserSettings.AutoSaveTimeMinutes = 10;			break;
	case IDM_AUTOSAVE_004:		EditorUserSettings.AutoSaveTimeMinutes = 15;			break;
	case IDM_AUTOSAVE_005:		EditorUserSettings.AutoSaveTimeMinutes = 30;			break;
	case IDM_AUTOSAVE_MAPS:		EditorUserSettings.bAutoSaveMaps = In.IsChecked();		break;
	case IDM_AUTOSAVE_CONTENT:	EditorUserSettings.bAutoSaveContent = In.IsChecked();	break;
	}

	GEditor->SaveUserSettings();

}

/**
 * Called in response to receiving a UI update event from wxWidgets; updates the autosave menu
 *
 * @param	In	Event generated by wxWidgets to update the UI
 */
void WxEditorFrame::UI_MenuAutoSaveOptions( wxUpdateUIEvent& In )
{
	const UEditorUserSettings& EditorUserSettings = GEditor->AccessUserSettings();
	switch( In.GetId() )
	{
	case IDM_AUTOSAVE_001:		In.Check( EditorUserSettings.AutoSaveTimeMinutes == 1 );	break;
	case IDM_AUTOSAVE_002:		In.Check( EditorUserSettings.AutoSaveTimeMinutes == 5 );	break;
	case IDM_AUTOSAVE_003:		In.Check( EditorUserSettings.AutoSaveTimeMinutes == 10 );	break;
	case IDM_AUTOSAVE_004:		In.Check( EditorUserSettings.AutoSaveTimeMinutes == 15 );	break;
	case IDM_AUTOSAVE_005:		In.Check( EditorUserSettings.AutoSaveTimeMinutes == 30 );	break;
	case IDM_AUTOSAVE_MAPS:		In.Check( EditorUserSettings.bAutoSaveMaps );				break;
	case IDM_AUTOSAVE_CONTENT:	In.Check( EditorUserSettings.bAutoSaveContent );			break;
	}
}

void WxEditorFrame::MenuScaleGrid( wxCommandEvent& In )
{
	if( IDM_DRAG_GRID_SNAPSCALE == In.GetId() )
	{	
		GUnrealEd->Constraints.SnapScaleEnabled = !GUnrealEd->Constraints.SnapScaleEnabled;
	}
	else
	{
		INT Scale = 0;

		switch( In.GetId() )
		{
		case IDM_SCALE_GRID_001:		Scale = 1;		break;
		case IDM_SCALE_GRID_002:		Scale = 2;		break;
		case IDM_SCALE_GRID_005:		Scale = 5;		break;
		case IDM_SCALE_GRID_010:		Scale = 10;		break;
		case IDM_SCALE_GRID_025:		Scale = 25;		break;
		case IDM_SCALE_GRID_050:		Scale = 50;		break;
		}

		GEditor->Constraints.ScaleGridSize = Scale;
	}

	UpdateUI();
}

void WxEditorFrame::UI_MenuViewFullScreen( wxUpdateUIEvent& In )
{
	In.Check( IsFullScreen() );
}

void WxEditorFrame::UI_MenuViewBrushPolys( wxUpdateUIEvent& In )
{
	In.Check( GEditor->bShowBrushMarkerPolys );
}

void WxEditorFrame::UI_MenuFarPlaneSlider( wxUpdateUIEvent& In )
{
	wxControl* Control = MainToolBar->FindControl(ID_FarPlaneSlider);
	wxSlider* Slider = wxDynamicCast(Control, wxSlider);

	INT Value = 0;
	if( Slider && GEditor->FarClippingPlane == 0.0f )
	{
		Value = Slider->GetMax();
	}
	else
	{
		const static FLOAT FarPlaneInverseLogBase = 1.0f / appLoge(1.06f);
		const static FLOAT FarPlaneInverseNearDist = 1.0f / 1024.0f;
		Value = (INT)(appLoge(GEditor->FarClippingPlane * FarPlaneInverseNearDist) * FarPlaneInverseLogBase);
	}

	if (Slider && (Slider->GetValue() != Value))
	{
		Slider->SetValue(Value);
	}
}

void WxEditorFrame::UI_MenuTogglePrefabLock( wxUpdateUIEvent& In )
{
	In.Check( GEditor->bPrefabsLocked );
}
void WxEditorFrame::UI_MenuToggleGroupsActive( wxUpdateUIEvent& In )
{
	In.Check( GEditor->bGroupingActive );
}

void WxEditorFrame::UI_MenuViewDistributionToggle( wxUpdateUIEvent& In )
{
	// @GEMINI_TODO: Less global var hack
	extern DWORD GDistributionType;
	In.Check(GDistributionType == 0);
}

void WxEditorFrame::UI_MenuMaterialQualityToggle( wxUpdateUIEvent& In )
{
	In.Check(GSystemSettings.bAllowHighQualityMaterials == 0);
}

void WxEditorFrame::UI_MenuViewToggleLODLocking( wxUpdateUIEvent& In )
{
	In.Check(GEditor->bEnableLODLocking);
}

void WxEditorFrame::UI_MenuToggleSocketSnapping( wxUpdateUIEvent& In )
{
	In.Check( GEditor->bEnableSocketSnapping );
}

void WxEditorFrame::UI_MenuToggleSocketNames( wxUpdateUIEvent& In )
{
	In.Check( GEditor->bEnableSocketNames );
}

void WxEditorFrame::UI_MenuViewPSysLODRealtimeToggle( wxUpdateUIEvent& In )
{
	extern UBOOL GbEnableEditorPSysRealtimeLOD;
	In.Check( (GbEnableEditorPSysRealtimeLOD == TRUE) );
}

void WxEditorFrame::UI_MenuViewPSysHelperToggle( wxUpdateUIEvent& In )
{
	In.Check(GEditor->bDrawParticleHelpers);
}

/**
 * Called automatically by wxWidgets to update the UI for the build all and submit option
 *
 * @param	In	Event automatically generated by wxWidgets to update the UI
 */
void WxEditorFrame::UI_MenuBuildAllSubmit( wxUpdateUIEvent& In )
{
#if HAVE_SCC && WITH_MANAGED_CODE
	In.Enable( FSourceControl::IsEnabled() == TRUE );
#endif // #if HAVE_SCC && WITH_MANAGED_CODE
}

/** Called by WxWidgets to update the editor UI for 'Enable WASD Camera Controls' option */
void WxEditorFrame::UI_MenuAllowFlightCameraToRemapKeys( wxUpdateUIEvent& In )
{
	const INT SelectionId = In.GetId();
	const INT SelectionIndex = SelectionId - IDM_MainMenu_FlightCameraOptionsStart;

	check( SelectionId >= IDM_MainMenu_FlightCameraOptionsStart && SelectionId <= IDM_MainMenu_FlightCameraOptionsEnd &&  SelectionIndex < 3 );
	In.Check( (UINT)GEditor->AccessUserSettings().FlightCameraControlType == SelectionIndex );
}

/** Called by WxWidgets to update the editor UI for 'Auto Restart Reimported Flash Movies' pref */
void WxEditorFrame::UI_MenuAutoRestartReimportedFlashMovies( wxUpdateUIEvent& In )
{
	In.Check( GEditor->GetUserSettings().bAutoRestartReimportedFlashMovies );
}

void WxEditorFrame::UI_MenuViewResizeViewportsTogether( wxUpdateUIEvent& In )
{
	In.Check( bViewportResizeTogether == TRUE );
}

/** Called by WxWidgets to update the editor UI for 'Enable WASD Camera Controls' option */
void WxEditorFrame::UI_MenuCenterZoomAroundCursor( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools().GetCenterZoomAroundCursor() == TRUE );
}

/** Called by WxWidgets to update the editor UI for 'Enable WASD Camera Controls' option */
void WxEditorFrame::UI_MenuPanMovesCanvas( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools().GetPanMovesCanvas() == TRUE );
}

/** Called by WxWidgets to update the editor UI For 'Replace Respects Scale' option */
void WxEditorFrame::UI_MenuReplaceRespectsScale( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools().GetReplaceRespectsScale() == TRUE );
}

/** Event handler for toggling whether or not to default to real time mode when opening a new perspective viewport. */
void WxEditorFrame::UI_MenuDefaultToRealtimeMode( wxUpdateUIEvent& In )
{
	In.Check( GEditor->AccessUserSettings().bStartInRealtimeMode == TRUE );
}

/** Called by WxWidgets to update the editor UI for viewport hover feedback */
void WxEditorFrame::UI_MenuViewportHoverFeedback( wxUpdateUIEvent& In )
{
	In.Check( GEditor->AccessUserSettings().bEnableViewportHoverFeedback == TRUE );
}

/** Called by WxWidgets to update the editor UI for highlight with brackets */
void WxEditorFrame::UI_MenuHighlightWithBrackets( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools().GetHighlightWithBrackets() == TRUE );
}

/** Called by WxWidgets to update the editor UI for depth dependent halos in wireframe*/
void WxEditorFrame::UI_MenuWireframeHalos( wxUpdateUIEvent& In )
{
	In.Check(WireframeHaloSettings.bEnablePostEffect == TRUE );
}

/** Called for updates to the Absolute Movement menu item*/
void WxEditorFrame::UI_MenuToggleAbsoluteTranslation( wxUpdateUIEvent& In )
{
	In.Check(GEditorModeTools().IsUsingAbsoluteTranslation() == TRUE);
}

/** Called for updates to the Translate RotateZ Widget menu item*/
void WxEditorFrame::UI_MenuToggleTranslateRotateZWidget( wxUpdateUIEvent& In )
{
	In.Check(GEditorModeTools().GetAllowTranslateRotateZWidget() == TRUE);
}

/** Called by WxWidgets to update the editor UI For 'Click BSP Selects Brush' option */
void WxEditorFrame::UI_MenuClickBSPSelectsBrush( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools().GetClickBSPSelectsBrush() == TRUE );
}

/** Called by WxWidgets to update the editor UI for the 'Auto-Updating BSP' option */
void WxEditorFrame::UI_MenuBSPAutoUpdate( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools().GetBSPAutoUpdate() == TRUE );
}

/**
 * Called by wxWidgets to update the editor UI for the 'Strict Box Selection' option
 *
 *	@param	In	Event generated by wxWidgets to update the UI
 */
void WxEditorFrame::UI_MenuUseStrictBoxSelection( wxUpdateUIEvent& In )
{
	wxObject* EventObject = In.GetEventObject();
	WxBitmapCheckButton* CheckButton = wxDynamicCast(EventObject, WxBitmapCheckButton);
	if (CheckButton)
	{
		//the standard logic (ON) is for strict intersection
		INT TestState = GEditor->GetUserSettings().bStrictBoxSelection ? WxBitmapCheckButton::STATE_On : WxBitmapCheckButton::STATE_Off;
		if (TestState != CheckButton->GetCurrentState()->ID)
		{
			CheckButton->SetCurrentState(TestState);
		}
	}
	In.Check( GEditor->GetUserSettings().bStrictBoxSelection );
}

/**
 * Called by wxWidgets to update the editor UI for the 'Prompt for Checkout on Package Modification' option
 *
 *	@param	In	Event generated by wxWidgets to update the UI
 */
void WxEditorFrame::UI_MenuPromptSCCOnPackageModification( wxUpdateUIEvent& In )
{
#if HAVE_SCC
	In.Check( GEditor->GetUserSettings().bPromptForCheckoutOnPackageModification );
#endif // #if HAVE_SCC
}

/**
 * Called by wxWidgets to update the editor UI for the 'Update Viewport Camera From Play-in-Viewport' option
 *
 *	@param	In	Event generated by wxWidgets to update the UI
 */
void WxEditorFrame::UI_MenuToggleViewportCameraToUpdateFromPIV( wxUpdateUIEvent& In )
{
	In.Check( GEditor->GetUserSettings().bEnableViewportCameraToUpdateFromPIV );
}

/**
 * Called by wxWidgets to update the editor UI for the 'linked orthographic viewport movement' option
 *
 *	@param	In	Event generated by wxWidgets to update the UI
 */
void WxEditorFrame::UI_MenuToggleLinkedOrthographicViewports( wxUpdateUIEvent& In )
{
	In.Check( GEditor->GetUserSettings().bUseLinkedOrthographicViewports );
}

/**
 * Called by wxWidgets to update the editor UI for the 'auto reimport textures' option
 *
 *	@param	In	Event generated by wxWidgets to update the UI
 */
void WxEditorFrame::UI_MenuAutoReimportTextures( wxUpdateUIEvent& In )
{
	In.Check( GEditor->GetUserSettings().bAutoReimportTextures );
}

/**
 * Called by wxWidgets to update the editor UI for the Editor Language sub-menu
 *
 *	@param	In	Event generated by wxWidgets to update the UI
 */
void WxEditorFrame::UI_MenuLanguageSelection( wxUpdateUIEvent& In )
{
	const INT SelectionId = In.GetId();
	const INT SelectionIndex = SelectionId - IDM_MainMenu_LanguageOptionStart;

	check( SelectionId >= IDM_MainMenu_LanguageOptionStart && SelectionId < IDM_MainMenu_LanguageOptionEnd &&  SelectionIndex < PreferencesMenu->SupportedLanguageExtensions.Num() );
	In.Check( appStricmp( *appGetLanguageExt(), *( PreferencesMenu->SupportedLanguageExtensions(SelectionIndex) ) ) == 0 );
}


void WxEditorFrame::UI_MenuAspectRatioSelection( wxUpdateUIEvent& In )
{
	const BYTE TestConstraint = In.GetId() - IDM_MainMenu_AspectRatioAxisConstraint_Start;
	In.Check( TestConstraint == GEditor->AccessUserSettings().AspectRatioAxisConstraint);
}

void WxEditorFrame::UI_MenuLoadSimpleLevelAtStartup( wxUpdateUIEvent& In )
{
	In.Check( GEditor->GetUserSettings().bLoadSimpleLevelAtStartup );
}


void WxEditorFrame::UI_ContextMenuMakeCurrentLevel( wxUpdateUIEvent& In )
{
	// Look to the selected actors for the level to make current.
	// If actors from multiple levels are selected, disable the
	// the "Make selected actor's level current" context menu item.

	UBOOL bAllSelectedActorsBelongToCurrentLevel = TRUE;
	ULevel* LevelToMakeCurrent = NULL;
	for ( USelection::TObjectIterator It = GEditor->GetSelectedActors()->ObjectItor() ; It ; ++It )
	{
		AActor* Actor = Cast<AActor>( *It );
		if ( Actor )
		{
			ULevel* ActorLevel = Actor->GetLevel();
			if( bAllSelectedActorsBelongToCurrentLevel )
			{
				if( ActorLevel != GWorld->CurrentLevel )
				{
					bAllSelectedActorsBelongToCurrentLevel = FALSE;
				}
			}
			if ( !LevelToMakeCurrent )
			{
				// First assignment.
				LevelToMakeCurrent = ActorLevel;
			}
			else if ( LevelToMakeCurrent != ActorLevel )
			{
				// Actors from multiple levels are selected -- abort.
				LevelToMakeCurrent = NULL;
				break;
			}
		}
	}

	// We'll also allow the user to select this option if there is a grid level volume current. (That
	// allows them to easily clear the grid volume.)
	In.Enable( LevelToMakeCurrent != NULL && ( GWorld->CurrentLevelGridVolume != NULL || !bAllSelectedActorsBelongToCurrentLevel ) );
}



void WxEditorFrame::UI_ContextMenuMakeCurrentLevelGridVolume( wxUpdateUIEvent& In )
{
	// Look to the selected actors for the level grid volume to make current.
	// If actors from multiple level grid volumes are selected, disable the
	// the "Make selected actor's level grid volume current" context menu item.
	UBOOL bAllSelectedActorsBelongToCurrentLevelGridVolume = TRUE;
	ALevelGridVolume* LevelGridVolumeToMakeCurrent = NULL;
	for ( USelection::TObjectIterator It = GEditor->GetSelectedActors()->ObjectItor() ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		ALevelGridVolume* ActorLevelGridVolume = EditorLevelUtils::GetLevelGridVolumeForActor( Actor );
		if( ActorLevelGridVolume != NULL )
		{
			if ( !LevelGridVolumeToMakeCurrent )
			{
				// First assignment.
				LevelGridVolumeToMakeCurrent = ActorLevelGridVolume;
			}
			else if ( LevelGridVolumeToMakeCurrent != ActorLevelGridVolume )
			{
				// Actors from multiple level grid volumes are selected -- abort.
				LevelGridVolumeToMakeCurrent = NULL;
				break;
			}

			if( ActorLevelGridVolume != GWorld->CurrentLevelGridVolume )
			{
				// Actor doesn't belong to the current level grid volume
				bAllSelectedActorsBelongToCurrentLevelGridVolume = FALSE;
				break;
			}
		}
		else
		{
			// At least one of the selected actors is not in a level grid volume
			LevelGridVolumeToMakeCurrent = NULL;
			bAllSelectedActorsBelongToCurrentLevelGridVolume = FALSE;
			break;
		}
	}


	In.Enable( LevelGridVolumeToMakeCurrent != NULL && !bAllSelectedActorsBelongToCurrentLevelGridVolume );
}


void WxEditorFrame::CoverEdit_ToggleEnabled( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).bEnabled = !Link->Slots(SlotIdx).bEnabled;
				}
			}
		}
	}
}

void WxEditorFrame::CoverEdit_ToggleAutoAdjust( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);

			Link->bAutoAdjust = !Link->bAutoAdjust;
		}
	}
}

void WxEditorFrame::CoverEdit_ToggleTypeAutomatic( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).ForceCoverType = CT_None;
					Link->AutoAdjustSlot(SlotIdx,TRUE);
				}
			}
		}
	}
}

void WxEditorFrame::CoverEdit_ToggleTypeStanding( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).ForceCoverType = CT_Standing;
					Link->AutoAdjustSlot(SlotIdx,TRUE);
				}
			}
		}
	}
					}

void WxEditorFrame::CoverEdit_ToggleTypeMidLevel( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).ForceCoverType = CT_MidLevel;
					Link->AutoAdjustSlot(SlotIdx,TRUE);
				}
			}
		}
	}
}

void WxEditorFrame::CoverEdit_ToggleCoverslip( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).bAllowCoverSlip = !Link->Slots(SlotIdx).bAllowCoverSlip;
				}
			}
		}
	}
}

void WxEditorFrame::CoverEdit_ToggleSwatTurn( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).bAllowSwatTurn = !Link->Slots(SlotIdx).bAllowSwatTurn;
				}
			}
		}
	}
}

void WxEditorFrame::CoverEdit_ToggleMantle( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).bAllowMantle = !Link->Slots(SlotIdx).bAllowMantle;
					if( Link->Slots(SlotIdx).bAllowMantle )
					{
						Link->Slots(SlotIdx).bAllowClimbUp = FALSE;
					}
				}
			}
		}
	}
}

void WxEditorFrame::CoverEdit_TogglePopup( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).bAllowPopup = !Link->Slots(SlotIdx).bAllowPopup;
				}
			}
		}
	}
}

void WxEditorFrame::CoverEdit_ToggleLeanLeft( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).bLeanLeft = !Link->Slots(SlotIdx).bLeanLeft;
					Link->bAutoAdjust = FALSE;
				}
			}
		}
	}
}

void WxEditorFrame::CoverEdit_ToggleLeanRight( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).bLeanRight = !Link->Slots(SlotIdx).bLeanRight;
					Link->bAutoAdjust = FALSE;
				}
			}
		}
	}
}

void WxEditorFrame::CoverEdit_ToggleClimbUp( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).bAllowClimbUp = !Link->Slots(SlotIdx).bAllowClimbUp;
					if( Link->Slots(SlotIdx).bAllowClimbUp )
					{
						Link->Slots(SlotIdx).bAllowMantle = FALSE;
					}
				}
			}
		}
	}
}

void WxEditorFrame::CoverEdit_TogglePreferLean( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).bPreferLeanOverPopup = !Link->Slots(SlotIdx).bPreferLeanOverPopup;
				}
			}
		}
	}
}

void WxEditorFrame::CoverEdit_TogglePlayerOnly( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).bPlayerOnly = !Link->Slots(SlotIdx).bPlayerOnly;
				}
			}
		}
	}
}

void WxEditorFrame::CoverEdit_ToggleForceCanPopup( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).bForceCanPopUp = !Link->Slots(SlotIdx).bForceCanPopUp;
				}
			}
		}
	}
}

void WxEditorFrame::CoverEdit_ToggleForceCanCoverslip_Left( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).bForceCanCoverSlip_Left = !Link->Slots(SlotIdx).bForceCanCoverSlip_Left;
				}
			}
		}
	}
}

void WxEditorFrame::CoverEdit_ToggleForceCanCoverslip_Right( wxCommandEvent& In )
{
	TArray<ACoverLink*> SelectedLinks;
	if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
	{
		for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
		{
			ACoverLink *Link = SelectedLinks(Idx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				if (Link->Slots(SlotIdx).bSelected)
				{
					Link->Slots(SlotIdx).bForceCanCoverSlip_Right = !Link->Slots(SlotIdx).bForceCanCoverSlip_Right;
				}
			}
		}
	}
}

void WxEditorFrame::MenuRedrawAllViewports( wxCommandEvent& In )
{
	GUnrealEd->RedrawAllViewports();
}

void WxEditorFrame::MenuAlignWall( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY TEXALIGN WALL") );
}

void WxEditorFrame::MenuToolCheckErrors( wxCommandEvent& In )
{
	GUnrealEd->Exec(TEXT("MAP CHECK"));
}

void WxEditorFrame::MenuReviewPaths( wxCommandEvent& In )
{
	GWarn->MapCheck_Clear();
	if ( !GEditor->GetMapBuildCancelled() )
	{
		FPathBuilder::Exec( TEXT("REVIEWPATHS") ); 
	}
	GWarn->MapCheck_ShowConditionally();
}

void WxEditorFrame::MenuRotateActors( wxCommandEvent& In )
{
}

void WxEditorFrame::MenuResetParticleEmitters( wxCommandEvent& In )
{
}

void WxEditorFrame::MenuSelectAllSurfs( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT ALL") );
}

void WxEditorFrame::MenuBrushAdd( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("BRUSH ADD") );
	GUnrealEd->RedrawLevelEditingViewports();
}

void WxEditorFrame::MenuBrushSubtract( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("BRUSH SUBTRACT") );
	GUnrealEd->RedrawLevelEditingViewports();
}

void WxEditorFrame::MenuBrushIntersect( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("BRUSH FROM INTERSECTION") );
	GUnrealEd->RedrawLevelEditingViewports();
}

void WxEditorFrame::MenuBrushDeintersect( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("BRUSH FROM DEINTERSECTION") );
	GUnrealEd->RedrawLevelEditingViewports();
}

void WxEditorFrame::MenuBrushOpen( wxCommandEvent& In )
{
	WxFileDialog OpenFileDialog(this, 
		TEXT("Open Brush"), 
		*(appGameDir() + TEXT("Content\\Maps")),
		TEXT(""),
		TEXT("Brushes (*.u3d)|*.u3d|All Files|*.*"),
		wxOPEN | wxFILE_MUST_EXIST,
		wxDefaultPosition);

	if( OpenFileDialog.ShowModal() == wxID_OK )
	{
		FString S(OpenFileDialog.GetPath());
		GUnrealEd->Exec( *FString::Printf(TEXT("BRUSH LOAD FILE=\"%s\""), *S));
		GUnrealEd->RedrawLevelEditingViewports();
	}

	GFileManager->SetDefaultDirectory();
}

void WxEditorFrame::MenuBrushSaveAs( wxCommandEvent& In )
{
	WxFileDialog SaveFileDialog(this, 
		TEXT("Save Brush"), 
		*(appGameDir() + TEXT("Content\\Maps")),
		TEXT(""),
		TEXT("Brushes (*.u3d)|*.u3d|All Files|*.*"),
		wxSAVE,
		wxDefaultPosition);

	if( SaveFileDialog.ShowModal() == wxID_OK )
	{
		FString S(SaveFileDialog.GetPath());
		GUnrealEd->Exec( *FString::Printf(TEXT("BRUSH SAVE FILE=\"%s\""), *S));
	}

	GFileManager->SetDefaultDirectory();
}

void WxEditorFrame::MenuBrushImport( wxCommandEvent& In )
{
	WxFileDialog ImportFileDialog( this, 
		TEXT("Import Brush"), 
		*(GApp->LastDir[LD_BRUSH]),
		TEXT(""),
		TEXT("Import Types (*.t3d, *.dxf, *.asc, *.ase)|*.t3d;*.dxf;*.asc;*.ase;|All Files|*.*"),
		wxOPEN | wxFILE_MUST_EXIST,
		wxDefaultPosition);

	// Display the Open dialog box.
	if( ImportFileDialog.ShowModal() == wxID_OK )
	{
		WxDlgImportBrush dlg;
		dlg.ShowModal( FString(ImportFileDialog.GetPath()) );

		GUnrealEd->RedrawLevelEditingViewports();

		FString S(ImportFileDialog.GetPath());
		GApp->LastDir[LD_BRUSH] = S.Left( S.InStr( TEXT("\\"), 1 ) );
	}

	GFileManager->SetDefaultDirectory();
}

void WxEditorFrame::MenuBrushExport( wxCommandEvent& In )
{
	WxFileDialog SaveFileDialog(this, 
		TEXT("Export Brush"), 
		*(GApp->LastDir[LD_BRUSH]),
		TEXT(""),
		TEXT("Unreal Text (*.t3d)|*.t3d|All Files|*.*"),
		wxSAVE,
		wxDefaultPosition);

	if( SaveFileDialog.ShowModal() == wxID_OK )
	{
		FString S(SaveFileDialog.GetPath());

		GUnrealEd->Exec( *FString::Printf(TEXT("BRUSH EXPORT FILE=\"%s\""), *S));

		GApp->LastDir[LD_BRUSH] = S.Left( S.InStr( TEXT("\\"), 1 ) );
	}

	GFileManager->SetDefaultDirectory();
}

void WxEditorFrame::MenuWizardNewTerrain( wxCommandEvent& In )
{
	WxWizard_NewTerrain wiz( this );
	wiz.RunTerrainWizard();
}

void WxEditorFrame::Menu_Help_About( wxCommandEvent& In )
{
#if WITH_MANAGED_CODE
	FAboutScreen::DisplayAboutScreen();
#endif // #if WITH_MANAGED_CODE
}

/**
 * Navigation helpers to open up UDN pages
 */

// End users don't have access to the secure parts of UDN...
#if SHIPPING_PC_GAME
static FString GURLPrefix( TEXT( "http://" ) );
#else // SHIPPING_PC_GAME
static FString GURLPrefix( TEXT( "https://" ) );
#endif // SHIPPING_PC_GAME

static wxString FixURL( FString URL )
{
	return( wxString( *FString::Printf( TEXT( "%s%s" ), *GURLPrefix, *URL ) ) );
}

void WxEditorFrame::Menu_Help_OnlineHelp( wxCommandEvent& In )
{
	wxLaunchDefaultBrowser( FixURL( "udn.epicgames.com/Three/WebHome" ) );
}
void WxEditorFrame::Menu_Help_UDKForums( wxCommandEvent& In )
{
	wxLaunchDefaultBrowser( TEXT( "http://forums.epicgames.com/forums/372-Questions-and-Community-Answers" ) );
}
void WxEditorFrame::Menu_Help_SearchUDN( wxCommandEvent& In )
{
#if SHIPPING_PC_GAME
	wxLaunchDefaultBrowser( TEXT( "http://udn.epicgames.com/search" ) );
#else // SHIPPING_PC_GAME
	wxLaunchDefaultBrowser( TEXT( "https://udn.epicgames.com/Main/NewSearch" ) );
#endif // SHIPPING_PC_GAME
}

/** Shows the startup tip dialog. */
void WxEditorFrame::Menu_Help_StartupTip(wxCommandEvent& In )
{
	GApp->StartupTipDialog->Show();
}

/**
 * Called when the user selects the menu option to show the welcome screen
 *
 * @param	In	Event generated by wxWidgets when the user selects the option to show the welcome screen
 */
void WxEditorFrame::Menu_Help_WelcomeScreen( wxCommandEvent& In )
{
#if WITH_MANAGED_CODE
	FWelcomeScreen::DisplayWelcomeScreen();
#endif // #if WITH_MANAGED_CODE
}

/**
 * Creates an actor of the specified type, trying first to find an actor factory,
 * falling back to "ACTOR ADD" exec and SpawnActor if no factory is found.
 * Does nothing if ActorClass is NULL.
 */
static void PrivateAddActor(const UClass* ActorClass)
{
	if ( ActorClass )
	{
		// Look for an actor factory capable of creating actors of that type.
		UActorFactory* ActorFactory = GEditor->FindActorFactory( ActorClass );
		if( ActorFactory )
		{
			// Determine if surface orientation should be used (currently only decals require this)
			UBOOL bShouldUseSurfaceOrientation = ( ActorClass->IsChildOf( ADecalActorBase::StaticClass() ) ); 
			GEditor->UseActorFactory( ActorFactory, FALSE, bShouldUseSurfaceOrientation );
		}
		else
		{
			// No actor factory was found; use SpawnActor instead.
			GUnrealEd->Exec( *FString::Printf( TEXT("ACTOR ADD CLASS=%s"), *ActorClass->GetName() ) );
		}
	}
}

void WxEditorFrame::MenuBackdropPopupAddClassHere( wxCommandEvent& In )
{
	const UClass* Class = GEditor->GetSelectedObjects()->GetTop<UClass>();
	PrivateAddActor( Class );
}

void WxEditorFrame::MenuBackdropPopupReplaceWithClass(wxCommandEvent& In)
{
	UClass* Class = GEditor->GetSelectedObjects()->GetTop<UClass>();
	GEditor->ReplaceSelectedActors(NULL, Class);
}

void WxEditorFrame::MenuBackdropPopupAddLastSelectedClassHere( wxCommandEvent& In )
{
	const INT Idx = In.GetId() - ID_BackdropPopupAddLastSelectedClassHere_START;
	const UClass* Class = GEditor->GetSelectedActors()->GetSelectedClass( Idx );
	PrivateAddActor( Class );
}

void WxEditorFrame::MenuSurfPopupApplyMaterial( wxCommandEvent& In )
{
	// Ensure that all selected assets are loaded
	GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded );

	GUnrealEd->Exec( TEXT("POLY SETMATERIAL") );
}

void WxEditorFrame::MenuSurfPopupAlignPlanarAuto( wxCommandEvent& In )
{
	GTexAlignTools.GetAligner( TEXALIGN_PlanarAuto )->Align( TEXALIGN_PlanarAuto );
}

void WxEditorFrame::MenuSurfPopupAlignPlanarWall( wxCommandEvent& In )
{
	GTexAlignTools.GetAligner( TEXALIGN_PlanarWall )->Align( TEXALIGN_PlanarWall );
}

void WxEditorFrame::MenuSurfPopupAlignPlanarFloor( wxCommandEvent& In )
{
	GTexAlignTools.GetAligner( TEXALIGN_PlanarFloor )->Align( TEXALIGN_PlanarFloor );
}

void WxEditorFrame::MenuSurfPopupAlignBox( wxCommandEvent& In )
{
	GTexAlignTools.GetAligner( TEXALIGN_Box )->Align( TEXALIGN_Box );
}

void WxEditorFrame::MenuSurfPopupUnalign( wxCommandEvent& In )
{
	GTexAlignTools.GetAligner( TEXALIGN_Default )->Align( TEXALIGN_Default );
}

void WxEditorFrame::MenuSurfPopupAlignFit( wxCommandEvent& In )
{
	GTexAlignTools.GetAligner( TEXALIGN_Fit )->Align( TEXALIGN_Fit );
}

void WxEditorFrame::MenuSurfPopupSelectMatchingGroups( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MATCHING GROUPS") );
}

void WxEditorFrame::MenuSurfPopupSelectMatchingItems( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MATCHING ITEMS") );
}

void WxEditorFrame::MenuSurfPopupSelectMatchingBrush( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MATCHING BRUSH") );
}

void WxEditorFrame::MenuSurfPopupSelectMatchingTexture( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MATCHING TEXTURE") );
}

void WxEditorFrame::MenuSurfPopupSelectMatchingResolution( wxCommandEvent& In )
{
	if (In.GetId() == ID_SurfPopupSelectMatchingResolutionCurrentLevel)
	{
		GUnrealEd->Exec( TEXT("POLY SELECT MATCHING RESOLUTION CURRENT") );
	}
	else
	{
		GUnrealEd->Exec( TEXT("POLY SELECT MATCHING RESOLUTION") );
	}
}

void WxEditorFrame::MenuSurfPopupSelectAllAdjacents( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT ADJACENT ALL") );
}

void WxEditorFrame::MenuSurfPopupSelectAdjacentCoplanars( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT ADJACENT COPLANARS") );
}

void WxEditorFrame::MenuSurfPopupSelectAdjacentWalls( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT ADJACENT WALLS") );
}

void WxEditorFrame::MenuSurfPopupSelectAdjacentFloors( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT ADJACENT FLOORS") );
}

void WxEditorFrame::MenuSurfPopupSelectAdjacentSlants( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT ADJACENT SLANTS") );
}

void WxEditorFrame::MenuSurfPopupSelectReverse( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT REVERSE") );
}

void WxEditorFrame::MenuSurfPopupSelectMemorize( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MEMORY SET") );
}

void WxEditorFrame::MenuSurfPopupRecall( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MEMORY RECALL") );
}

void WxEditorFrame::MenuSurfPopupOr( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MEMORY INTERSECTION") );
}

void WxEditorFrame::MenuSurfPopupAnd( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MEMORY UNION") );
}

void WxEditorFrame::MenuSurfPopupXor( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MEMORY XOR") );
}

void WxEditorFrame::MenuBlockingVolumeBBox( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("ACTOR CREATE_BV_BOUNDINGBOX SnapToGrid=%d"), ( GetAsyncKeyState(VK_SHIFT) & 0x8000 ) ? 1 : 0 ) );
}

void WxEditorFrame::MenuBlockingVolumeConvexVolumeHeavy( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("ACTOR CREATE_BV_CONVEXVOLUME NORMALTOLERANCE=0.01 SnapToGrid=%d"), ( GetAsyncKeyState(VK_SHIFT) & 0x8000 ) ? 1 : 0 ) );
}

void WxEditorFrame::MenuBlockingVolumeConvexVolumeNormal( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("ACTOR CREATE_BV_CONVEXVOLUME NORMALTOLERANCE=.15 SnapToGrid=%d"), ( GetAsyncKeyState(VK_SHIFT) & 0x8000 ) ? 1 : 0 ) );
}

void WxEditorFrame::MenuBlockingVolumeConvexVolumeLight( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("ACTOR CREATE_BV_CONVEXVOLUME NORMALTOLERANCE=.5 SnapToGrid=%d"), ( GetAsyncKeyState(VK_SHIFT) & 0x8000 ) ? 1 : 0 ) );
}

void WxEditorFrame::MenuBlockingVolumeConvexVolumeRough( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("ACTOR CREATE_BV_CONVEXVOLUME NORMALTOLERANCE=0.75 SnapToGrid=%d"), ( GetAsyncKeyState(VK_SHIFT) & 0x8000 ) ? 1 : 0 ) );
}

void WxEditorFrame::MenuBlockingVolumeColumnX( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("ACTOR CREATE_BV_CONVEXVOLUME NORMALTOLERANCE=.15 NLIMITX=0.2 SnapToGrid=%d"), ( GetAsyncKeyState(VK_SHIFT) & 0x8000 ) ? 1 : 0 ) );
}

void WxEditorFrame::MenuBlockingVolumeColumnY( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("ACTOR CREATE_BV_CONVEXVOLUME NORMALTOLERANCE=.15 NLIMITY=0.2 SnapToGrid=%d"), ( GetAsyncKeyState(VK_SHIFT) & 0x8000 ) ? 1 : 0 ) );
}

void WxEditorFrame::MenuBlockingVolumeColumnZ( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("ACTOR CREATE_BV_CONVEXVOLUME NORMALTOLERANCE=.15 NLIMITZ=0.2 SnapToGrid=%d"), ( GetAsyncKeyState(VK_SHIFT) & 0x8000 ) ? 1 : 0 ) );
}

void WxEditorFrame::MenuBlockingVolumeAutoConvex( wxCommandEvent& In )
{
	WxConvexDecompOptions* AutoConvexCollisionWindow = GApp->GetDlgAutoConvexCollision();
	check(AutoConvexCollisionWindow);
	AutoConvexCollisionWindow->Show();
}

void WxEditorFrame::MenuActorPopupSelectAllClass( wxCommandEvent& In )
{
	FGetInfoRet gir = GetInfo( GI_NUM_SELECTED | GI_CLASSNAME_SELECTED );

	if( gir.iValue )
	{
		GUnrealEd->Exec( *FString::Printf( TEXT("ACTOR SELECT OFCLASS CLASS=%s"), *gir.String ) );
	}
}

/**
 * Selects all actors with the same class and archetype as the selected actors (only if all selected actors
 * already have the same class/archetype)
 *
 * @param	In	Event generated by wxWidgets when the user elects to select by class and archetype
 */
void WxEditorFrame::MenuActorPopupSelectAllClassWithArchetype( wxCommandEvent& In )
{
	// For this function to have been called in the first place, all of the selected actors should be of the same type
	// and with the same archetype; however, it's safest to confirm the assumption first
	UBOOL bAllSameClassAndArchetype = FALSE;
	UClass* FirstClass = NULL;
	UObject* FirstArchetype = NULL;

	// Find the class and archetype of the first selected actor; they will be used to check that all selected actors
	// share the same class and archetype
	FSelectedActorIterator SelectedActorIter;
	if ( SelectedActorIter )
	{
		AActor* FirstActor = *SelectedActorIter;
		check( FirstActor );
		FirstClass = FirstActor->GetClass();
		FirstArchetype = FirstActor->GetArchetype();

		// If the archetype of the first actor is NULL, then do not allow the selection to proceed
		bAllSameClassAndArchetype = FirstArchetype ? TRUE : FALSE;
		
		// Increment the iterator so the search begins on the second selected actor
		++SelectedActorIter;
	}
	// Check all the other selected actors
	for ( SelectedActorIter; SelectedActorIter && bAllSameClassAndArchetype; ++SelectedActorIter )
	{
		AActor* CurActor = *SelectedActorIter;
		if ( CurActor->GetClass() != FirstClass || CurActor->GetArchetype() != FirstArchetype )
		{
			bAllSameClassAndArchetype = FALSE;
			break;
		}
	}

	// If all the selected actors have the same class and archetype, then go ahead and select all other actors
	// matching the same class and archetype
	if ( bAllSameClassAndArchetype )
	{
		FScopedTransaction Transaction( *LocalizeUnrealEd("SelectOfClassAndArchetype") );
		GUnrealEd->edactSelectOfClassAndArchetype( FirstClass, FirstArchetype );
	}
}

void WxEditorFrame::MenuActorPopupSelectAllBased( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT BASED" ) );
}

void WxEditorFrame::MenuActorPopupSelectMatchingProcBuildingsByRuleset( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT MATCHINGPROCBUILDINGRULESETS") );
}

void WxEditorFrame::MenuActorPopupSelectMatchingStaticMeshesThisClass( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT MATCHINGSTATICMESH") );
}

void WxEditorFrame::MenuActorPopupSelectMatchingStaticMeshesAllClasses( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT MATCHINGSTATICMESH ALLCLASSES") );
}

void WxEditorFrame::MenuActorPopupSelectMatchingSkeletalMeshesThisClass( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT MATCHINGSKELETALMESH") );
}

void WxEditorFrame::MenuActorPopupSelectMatchingSkeletalMeshesAllClasses( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT MATCHINGSKELETALMESH ALLCLASSES") );
}

/**
 * Select all emitter actors that have a particle system template in common with currently
 * selected emitters
 *
 * @param	In	Event generated by wxWidgets in response to the menu option being selected
 */
void WxEditorFrame::MenuActorPopupSelectMatchingEmitter( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT MATCHINGEMITTER") );
}

void WxEditorFrame::MenuActorPopupSelectMatchingSpeedTrees( wxCommandEvent& In )
{
	GUnrealEd->SelectMatchingSpeedTrees();
}

/**
 * Called when the "Select all with matching materials" menu item is chosen.
 */
void WxEditorFrame::MenuActorPopupSelectAllWithMatchingMaterial( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT MATCHINGMATERIAL") );
}

/**
 * Toggle the dynamic channel of selected lights without invalidating any cached lighting.
 */
void WxEditorFrame::MenuActorPopupToggleDynamicChannel( wxCommandEvent& In )
{
	// Select all light actors.
	for( FSelectedActorIterator It; It; ++It )
	{
		ALight* Light = Cast<ALight>(*It);
		if( Light && Light->LightComponent )
		{
			FComponentReattachContext ReattachContext( Light->LightComponent );
			Light->LightComponent->LightingChannels.Dynamic = !Light->LightComponent->LightingChannels.Dynamic;
			Light->DetermineAndSetEditorIcon();
		}
	}
}

void WxEditorFrame::MenuActorPopupSelectAllLights( wxCommandEvent& In )
{
	// Select all light actors.
	for( FActorIterator It; It; ++It )
	{
		ALight* Light = Cast<ALight>(*It);
		if( Light && !Light->IsInPrefabInstance() )
		{
			GUnrealEd->SelectActor( Light, TRUE, NULL, TRUE, FALSE );
		}
	}
}



/**
 * Selects all of the actors currently being rendered
 *
 * @param	In	Wx input event
 */
void WxEditorFrame::MenuActorPopupSelectAllRendered( wxCommandEvent& In )
{
	// First deselect all actors
	const UBOOL bNoteSelectionChange = FALSE;
	GEditor->SelectNone( bNoteSelectionChange, FALSE );
	if( GWorld != NULL )
	{
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		if( WorldInfo != NULL )
		{
			for( FActorIterator It; It; ++It )
			{
				AActor* CurActor = *It;
				if( CurActor != NULL )
				{
					// NOTE: This only works with non-realtime viewports
					// NOTE: Beware of multiple viewports drawing different objects
					if( CurActor->LastRenderTime == WorldInfo->TimeSeconds )
					{
						GUnrealEd->SelectActor( CurActor, TRUE, NULL, TRUE, FALSE );
					}
				}
			}
		}
	}

	if( !bNoteSelectionChange )
	{
		GEditor->NoteSelectionChange();
	}
}


void WxEditorFrame::MenuActorPopupSelectAllLightsWithSameClassification( wxCommandEvent& In )
{
	ELightAffectsClassification LightAffectsClassification = LAC_MAX;
	// Find first selected light and use its classification.
	for( FSelectedActorIterator It; It; ++It )
	{
		ALight* Light = Cast<ALight>(*It);
		if( Light && Light->LightComponent )
		{
			LightAffectsClassification = (ELightAffectsClassification)Light->LightComponent->LightAffectsClassification;
			break;
		}
	}
	// Select all lights matching the light classification.
	for( FActorIterator It; It; ++It )
	{
		ALight* Light = Cast<ALight>(*It);
		if( Light && Light->LightComponent )
		{
			if( LightAffectsClassification == Light->LightComponent->LightAffectsClassification )
			{
				if ( !Light->IsInPrefabInstance() )
				{
					GUnrealEd->SelectActor( Light, TRUE, NULL, TRUE, FALSE );
				}
			}
		}
	}
}

void WxEditorFrame::MenuActorPopupSelectKismetUnreferencedAll( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT KISMETREF 0 ALL") );
}

void WxEditorFrame::MenuActorPopupSelectKismetReferencedAll( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT KISMETREF 1 ALL") );
}

void WxEditorFrame::MenuActorPopupSelectKismetUnreferenced( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT KISMETREF 0") );
}

void WxEditorFrame::MenuActorPopupSelectKismetReferenced( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT KISMETREF 1") );
}

void WxEditorFrame::MenuActorPopupAlignCameras( wxCommandEvent& In )
{
	// If Shift is held down then we'll only focus the active viewport around the actor
	const UBOOL bShiftDown = ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0;
	if( bShiftDown )
	{
		GUnrealEd->Exec( TEXT( "CAMERA ALIGN ACTIVEVIEWPORTONLY" ) );
	}
	else
	{
		GUnrealEd->Exec( TEXT( "CAMERA ALIGN" ) );
	}
}

void WxEditorFrame::MenuActorPopupLockMovement( wxCommandEvent& In )
{
	// First figure out if any selected actor is already locked.
	UBOOL bFoundLockedActor = FALSE;
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if( Actor->bLockLocation )
		{
			bFoundLockedActor = TRUE;
			break;
		}
	}


	// Fires CALLBACK_LevelDirtied when falling out of scope.
	FScopedLevelDirtied		LevelDirtyCallback;

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		Actor->Modify();

		// If nothing is locked then we'll turn on locked for all selected actors
		// Otherwise, we'll turn off locking for any actors that are locked
		Actor->bLockLocation = !bFoundLockedActor;

		LevelDirtyCallback.Request();
	}
}

void WxEditorFrame::MenuActorPopupSnapViewToActor( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("CAMERA SNAP") );
}

void WxEditorFrame::MenuActorPopupMerge( wxCommandEvent& In )
{
	GUnrealEd->Exec(TEXT("BRUSH MERGEPOLYS"));
}

void WxEditorFrame::MenuActorPopupSeparate( wxCommandEvent& In )
{
	GUnrealEd->Exec(TEXT("BRUSH SEPARATEPOLYS"));
}

void WxEditorFrame::MenuActorPopupToFirst( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP SENDTO FIRST") );
}

void WxEditorFrame::MenuActorPopupToLast( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP SENDTO LAST") );
}

void WxEditorFrame::MenuActorPopupToBrush( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP BRUSH GET") );
}

void WxEditorFrame::MenuActorPopupFromBrush( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP BRUSH PUT") );
}

void WxEditorFrame::MenuActorPopupMakeAdd( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf(TEXT("MAP SETBRUSH CSGOPER=%d"), (INT)CSG_Add) );
}

void WxEditorFrame::MenuActorPopupMakeSubtract( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf(TEXT("MAP SETBRUSH CSGOPER=%d"), (INT)CSG_Subtract) );
}

void WxEditorFrame::MenuSnapToFloor( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR ALIGN SNAPTOFLOOR ALIGN=0 USEPIVOT=0") );
}

void WxEditorFrame::MenuAlignToFloor( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR ALIGN SNAPTOFLOOR ALIGN=1 USEPIVOT=0") );
}

void WxEditorFrame::MenuSnapPivotToFloor( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR ALIGN SNAPTOFLOOR ALIGN=0 USEPIVOT=1") );
}

void WxEditorFrame::MenuAlignPivotToFloor( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR ALIGN SNAPTOFLOOR ALIGN=1 USEPIVOT=1") );
}

void WxEditorFrame::MenuMoveToGrid( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR ALIGN MOVETOGRID") );
}

void WxEditorFrame::MenuSaveBrushAsCollision( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("STATICMESH SAVEBRUSHASCOLLISION") );
}

void WxEditorFrame::MenuActorSelectShow( wxCommandEvent& In )			
{	
	GUnrealEd->Exec(TEXT("ACTOR HIDE UNSELECTED"));	
}

void WxEditorFrame::MenuActorSelectHide( wxCommandEvent& In )			
{	
	GUnrealEd->Exec(TEXT("ACTOR HIDE SELECTED"));	
}

void WxEditorFrame::MenuActorSelectInvert( wxCommandEvent& In )		
{	
	GUnrealEd->Exec(TEXT("ACTOR SELECT INVERT"));	
}

void WxEditorFrame::MenuActorSelectRelevantLights( wxCommandEvent& In )
{
	GUnrealEd->Exec(TEXT("ACTOR SELECT RELEVANTLIGHTS"));	
}

void WxEditorFrame::MenuActorSelectRelevantDominantLights( wxCommandEvent& In )
{
	GUnrealEd->Exec(TEXT("ACTOR SELECT RELEVANTDOMINANTLIGHTS"));	
}

void WxEditorFrame::MenuActorShowAll( wxCommandEvent& In )				
{	
	GUnrealEd->Exec(TEXT("ACTOR UNHIDE ALL"));	
}

/**
 * Called in response to the user selecting to show all at startup in the context menu
 * (Changes bHiddenEd to FALSE for all actors/BSP)
 *
 * @param	In	Event automatically generated by wxWidgets upon menu item selection
 */
void WxEditorFrame::MenuActorShowAllAtStartup( wxCommandEvent& In )
{
	GUnrealEd->Exec(TEXT("ACTOR UNHIDE ALL STARTUP"));
}

/**
 * Called in response to the user selecting to show selected at startup in the context menu
 * (Changes bHiddenEd to FALSE for all selected actors/BSP)
 *
 * @param	In	Event automatically generated by wxWidgets upon menu item selection
 */
void WxEditorFrame::MenuActorSelectShowAtStartup( wxCommandEvent& In )
{
	GUnrealEd->Exec(TEXT("ACTOR UNHIDE SELECTED STARTUP"));
}

/**
 * Called in response to the user selecting to hide selected at startup in the context menu
 * (Changes bHiddenEd to TRUE for all selected actors/BSP)
 *
 * @param	In	Event automatically generated by wxWidgets upon menu item selection
 */
void WxEditorFrame::MenuActorSelectHideAtStartup( wxCommandEvent& In )
{
	GUnrealEd->Exec(TEXT("ACTOR HIDE SELECTED STARTUP"));
}


/**
 * Called in response to the user selecting Update base to proc building from the context menu 
 * For each static mesh selected, attempt to set the base of the static mesh to a proc building directly below it
 */
void WxEditorFrame::MenuUpdateBaseToProcBuilding( wxCommandEvent& In )
{
	// Find all selected static mesh actors and update their bases
	for( FSelectedActorIterator It; It; ++It )
	{
		AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(*It);
		if( StaticMeshActor )
		{
			GCallbackEvent->Send(CALLBACK_BaseSMActorOnProcBuilding, StaticMeshActor);
		}
	}
}

void WxEditorFrame::MenuMoveCameraToPoint( wxCommandEvent& In )
{
	// Find the active viewport
	for( INT CurViewportIndex = 0; CurViewportIndex < ViewportConfigData->GetViewportCount(); ++CurViewportIndex )
	{
		FVCD_Viewport& CurViewport = ViewportConfigData->AccessViewport( CurViewportIndex );
		if( CurViewport.bEnabled && CurViewport.ViewportWindow == GCurrentLevelEditingViewportClient)
		{
			// Grab the last clicked actor (if any)
			AActor* ClickedActor = GEditor->GetSelectedActors()->GetBottom<AActor>();

			CurViewport.ViewportWindow->TeleportViewportCamera(GEditor->ClickLocation, ClickedActor);
			break;
		}
	}
}

#if WITH_FBX
/**
 * Exports static mesh actors, interp actors and brushes
 */
void WxEditorFrame::MenuExportActorToFBX( wxCommandEvent& In )
{
	GUnrealEd->Exec(TEXT("ACTOR EXPORT FBX"));
}
#endif

/** Utility for copying properties that differ from defaults between mesh types. */
struct FConvertStaticMeshActorInfo
{
	/** The level the source actor belonged to, and into which the new actor is created. */
	ULevel*						SourceLevel;

	// Actor properties.
	FVector						Location;
	FRotator					Rotation;
	FVector						PrePivot;
	FLOAT						DrawScale;
	FVector						DrawScale3D;
	UBOOL						bHidden;
	UBOOL						bCollideActors;
	UBOOL						bCollideWorld;
	UBOOL						bBlockActors;
	UBOOL						bPathColliding;
	UBOOL						bNoEncroachCheck;
	AActor*						Base;
	FName						BaseBoneName;
	USkeletalMeshComponent*		BaseSkelComponent;
	// End actor properties.

	/**
	 * Used to indicate if any of the above properties differ from defaults; if so, they're copied over.
	 * We don't want to simply copy all properties, because classes with different defaults will have
	 * their defaults hosed by other types.
	 */
	UBOOL bActorPropsDifferFromDefaults[14];

	// Component properties.
	UStaticMesh*						StaticMesh;
	TArrayNoInit<UMaterialInterface*>	Materials;
	TArray<FGuid>						IrrelevantLights;
	FLOAT								CachedMaxDrawDistance;
	UBOOL								CastShadow;
	UBOOL								CollideActors;
	UBOOL								BlockActors;
	UBOOL								BlockZeroExtent;
	UBOOL								BlockNonZeroExtent;
	UBOOL								BlockRigidBody;
	UPhysicalMaterial*					PhysMaterialOverride;
	TArray< TArray<FColor> >			OverrideVertexColors;
	// End component properties.

	/**
	 * Used to indicate if any of the above properties differ from defaults; if so, they're copied over.
	 * We don't want to simply copy all properties, because classes with different defaults will have
	 * their defaults hosed by other types.
	 */
	UBOOL bComponentPropsDifferFromDefaults[12];

	TArray<USeqVar_Object*>		KismetObjectVars;
	TArray<USequenceEvent*>		KismetEvents;

	AGroupActor* ActorGroup;

	UBOOL PropsDiffer(const TCHAR* PropertyPath, UObject* Obj)
	{
		const UProperty* PartsProp = FindObjectChecked<UProperty>( ANY_PACKAGE, PropertyPath );

		BYTE* ClassDefaults = Obj->GetClass()->GetDefaults();
		check( ClassDefaults );

		const UBOOL bMatches = PartsProp->Matches(Obj, ClassDefaults, 0, FALSE, 0);
		return !bMatches;
	}

	void GetFromActor(AActor* Actor, UStaticMeshComponent* MeshComp)
	{
		SourceLevel				= Actor->GetLevel();

		// Copy over actor properties.
		Location				= Actor->Location;
		Rotation				= Actor->Rotation;
		PrePivot				= Actor->PrePivot;
		DrawScale				= Actor->DrawScale;
		DrawScale3D				= Actor->DrawScale3D;
		bHidden					= Actor->bHidden;
		bCollideActors			= Actor->bCollideActors;
		bCollideWorld			= Actor->bCollideWorld;
		bBlockActors			= Actor->bBlockActors;
		bPathColliding			= Actor->bPathColliding;
		bNoEncroachCheck		= Actor->bNoEncroachCheck;
		Base					= Actor->Base;
		BaseBoneName			= Actor->BaseBoneName;
		BaseSkelComponent		= Actor->BaseSkelComponent;

		// Record which actor properties differ from their defaults.
		bActorPropsDifferFromDefaults[0] = PropsDiffer( TEXT("Engine.Actor.Location"), Actor );
		bActorPropsDifferFromDefaults[1] = PropsDiffer( TEXT("Engine.Actor.Rotation"), Actor );
		bActorPropsDifferFromDefaults[2] = PropsDiffer( TEXT("Engine.Actor.PrePivot"), Actor );
		bActorPropsDifferFromDefaults[3] = PropsDiffer( TEXT("Engine.Actor.DrawScale"), Actor );
		bActorPropsDifferFromDefaults[4] = PropsDiffer( TEXT("Engine.Actor.DrawScale3D"), Actor );
		bActorPropsDifferFromDefaults[5] = PropsDiffer( TEXT("Engine.Actor.bHidden"), Actor );
		bActorPropsDifferFromDefaults[6] = PropsDiffer( TEXT("Engine.Actor.bCollideActors"), Actor );
		bActorPropsDifferFromDefaults[7] = PropsDiffer( TEXT("Engine.Actor.bCollideWorld"), Actor );
		bActorPropsDifferFromDefaults[8] = PropsDiffer( TEXT("Engine.Actor.bBlockActors"), Actor );
		bActorPropsDifferFromDefaults[9] = PropsDiffer( TEXT("Engine.Actor.bPathColliding"), Actor );
		bActorPropsDifferFromDefaults[10] = PropsDiffer( TEXT("Engine.Actor.bNoEncroachCheck"), Actor );
		bActorPropsDifferFromDefaults[11] = PropsDiffer( TEXT("Engine.Actor.Base"), Actor );
		bActorPropsDifferFromDefaults[12] = PropsDiffer( TEXT("Engine.Actor.BaseBoneName"), Actor );
		bActorPropsDifferFromDefaults[13] = PropsDiffer( TEXT("Engine.Actor.BaseSkelComponent"), Actor );

		// Copy over component properties.
		StaticMesh				= MeshComp->StaticMesh;
		Materials				= MeshComp->Materials;
		IrrelevantLights		= MeshComp->IrrelevantLights;
		CachedMaxDrawDistance	= MeshComp->CachedMaxDrawDistance;
		CastShadow				= MeshComp->CastShadow;
		CollideActors			= MeshComp->CollideActors;
		BlockActors				= MeshComp->BlockActors;
		BlockZeroExtent			= MeshComp->BlockZeroExtent;
		BlockNonZeroExtent		= MeshComp->BlockNonZeroExtent;
		BlockRigidBody			= MeshComp->BlockRigidBody;
		PhysMaterialOverride	= MeshComp->PhysMaterialOverride;

		// Loop over each LODInfo in the static mesh component, storing the override vertex colors
		// in each, if any
		UBOOL bHasAnyVertexOverrideColors = FALSE;
		for ( INT LODIndex = 0; LODIndex < MeshComp->LODData.Num(); ++LODIndex )
		{
			const FStaticMeshComponentLODInfo& CurLODInfo = MeshComp->LODData(LODIndex);
			const FColorVertexBuffer* CurVertexBuffer = CurLODInfo.OverrideVertexColors;

			OverrideVertexColors.AddItem( TArray<FColor>() );
			
			// If the LODInfo has override vertex colors, store off each one
			if ( CurVertexBuffer && CurVertexBuffer->GetNumVertices() > 0 )
			{
				for ( UINT VertexIndex = 0; VertexIndex < CurVertexBuffer->GetNumVertices(); ++VertexIndex )
				{
					OverrideVertexColors(LODIndex).AddItem( CurVertexBuffer->VertexColor(VertexIndex) );
				}
				bHasAnyVertexOverrideColors = TRUE;
			}
		}

		// Record which component properties differ from their defaults.
		bComponentPropsDifferFromDefaults[0] = PropsDiffer( TEXT("Engine.StaticMeshComponent.StaticMesh"), MeshComp );
		bComponentPropsDifferFromDefaults[1] = TRUE; // Assume the materials array always differs.
		bComponentPropsDifferFromDefaults[2] = TRUE; // Assume the set of irrelevant lights always differs.
		bComponentPropsDifferFromDefaults[3] = PropsDiffer( TEXT("Engine.PrimitiveComponent.CachedMaxDrawDistance"), MeshComp );
		bComponentPropsDifferFromDefaults[4] = PropsDiffer( TEXT("Engine.PrimitiveComponent.CastShadow"), MeshComp );
		bComponentPropsDifferFromDefaults[5] = PropsDiffer( TEXT("Engine.PrimitiveComponent.CollideActors"), MeshComp );
		bComponentPropsDifferFromDefaults[6] = PropsDiffer( TEXT("Engine.PrimitiveComponent.BlockActors"), MeshComp );
		bComponentPropsDifferFromDefaults[7] = PropsDiffer( TEXT("Engine.PrimitiveComponent.BlockZeroExtent"), MeshComp );
		bComponentPropsDifferFromDefaults[8] = PropsDiffer( TEXT("Engine.PrimitiveComponent.BlockNonZeroExtent"), MeshComp );
		bComponentPropsDifferFromDefaults[9] = PropsDiffer( TEXT("Engine.PrimitiveComponent.BlockRigidBody"), MeshComp );
		bComponentPropsDifferFromDefaults[10] = PropsDiffer( TEXT("Engine.PrimitiveComponent.PhysMaterialOverride"), MeshComp );
		bComponentPropsDifferFromDefaults[11] = bHasAnyVertexOverrideColors;	// Differs from default if there are any vertex override colors
	}

	void SetToActor(AActor* Actor, UStaticMeshComponent* MeshComp)
	{
		if ( Actor->GetLevel() != SourceLevel )
		{
			appErrorf( *LocalizeUnrealEd("Error_ActorConversionLevelMismatch") );
		}

		// Set actor properties.
		if ( bActorPropsDifferFromDefaults[0] ) Actor->Location				= Location;
		if ( bActorPropsDifferFromDefaults[1] ) Actor->Rotation				= Rotation;
		if ( bActorPropsDifferFromDefaults[2] ) Actor->PrePivot				= PrePivot;
		if ( bActorPropsDifferFromDefaults[3] ) Actor->DrawScale			= DrawScale;
		if ( bActorPropsDifferFromDefaults[4] ) Actor->DrawScale3D			= DrawScale3D;
		if ( bActorPropsDifferFromDefaults[5] ) Actor->bHidden				= bHidden;
		if ( bActorPropsDifferFromDefaults[6] ) Actor->bCollideActors		= bCollideActors;
		if ( bActorPropsDifferFromDefaults[7] ) Actor->bCollideWorld		= bCollideWorld;
		if ( bActorPropsDifferFromDefaults[8] ) Actor->bBlockActors			= bBlockActors;
		if ( bActorPropsDifferFromDefaults[9] ) Actor->bPathColliding		= bPathColliding;
		if ( bActorPropsDifferFromDefaults[10] ) Actor->bNoEncroachCheck		= bNoEncroachCheck;
		if ( bActorPropsDifferFromDefaults[11] ) Actor->Base					= Base;
		if ( bActorPropsDifferFromDefaults[12] ) Actor->BaseBoneName			= BaseBoneName;
		if ( bActorPropsDifferFromDefaults[13] ) Actor->BaseSkelComponent	= BaseSkelComponent;

		// Set component properties.
		if ( bComponentPropsDifferFromDefaults[0] ) MeshComp->StaticMesh			= StaticMesh;
		if ( bComponentPropsDifferFromDefaults[1] ) MeshComp->Materials				= Materials;
		if ( bComponentPropsDifferFromDefaults[2] ) MeshComp->IrrelevantLights		= IrrelevantLights;
		if ( bComponentPropsDifferFromDefaults[3] ) MeshComp->CachedMaxDrawDistance	= CachedMaxDrawDistance;
		if ( bComponentPropsDifferFromDefaults[4] ) MeshComp->CastShadow			= CastShadow;
		if ( bComponentPropsDifferFromDefaults[5] ) MeshComp->CollideActors			= CollideActors;
		if ( bComponentPropsDifferFromDefaults[6] ) MeshComp->BlockActors			= BlockActors;
		if ( bComponentPropsDifferFromDefaults[7] ) MeshComp->BlockZeroExtent		= BlockZeroExtent;
		if ( bComponentPropsDifferFromDefaults[8] ) MeshComp->BlockNonZeroExtent	= BlockNonZeroExtent;
		if ( bComponentPropsDifferFromDefaults[9] ) MeshComp->BlockRigidBody		= BlockRigidBody;
		if ( bComponentPropsDifferFromDefaults[10] ) MeshComp->PhysMaterialOverride	= PhysMaterialOverride;
		if ( bComponentPropsDifferFromDefaults[11] )
		{
			// Ensure the LODInfo has the right number of entries
			MeshComp->SetLODDataCount( OverrideVertexColors.Num(), MeshComp->StaticMesh->LODModels.Num() );
			
			// Loop over each LODInfo to see if there are any vertex override colors to restore
			for ( INT LODIndex = 0; LODIndex < MeshComp->LODData.Num(); ++LODIndex )
			{
				FStaticMeshComponentLODInfo& CurLODInfo = MeshComp->LODData(LODIndex);

				// If there are override vertex colors specified for a particular LOD, set them in the LODInfo
				if ( OverrideVertexColors.IsValidIndex( LODIndex ) && OverrideVertexColors(LODIndex).Num() > 0 )
				{
					const TArray<FColor>& OverrideColors = OverrideVertexColors(LODIndex);
					
					// Destroy the pre-existing override vertex buffer if it's not the same size as the override colors to be restored
					if ( CurLODInfo.OverrideVertexColors && CurLODInfo.OverrideVertexColors->GetNumVertices() != OverrideColors.Num() )
					{
						CurLODInfo.ReleaseOverrideVertexColorsAndBlock();
					}

					// If there is a pre-existing color vertex buffer that is valid, release the render thread's hold on it and modify
					// it with the saved off colors
					if ( CurLODInfo.OverrideVertexColors )
					{								
						CurLODInfo.BeginReleaseOverrideVertexColors();
						FlushRenderingCommands();
						for ( INT VertexIndex = 0; VertexIndex < OverrideColors.Num(); ++VertexIndex )
						{
							CurLODInfo.OverrideVertexColors->VertexColor(VertexIndex) = OverrideColors(VertexIndex);
						}
					}

					// If there isn't a pre-existing color vertex buffer, create one and initialize it with the saved off colors 
					else
					{
						CurLODInfo.OverrideVertexColors = new FColorVertexBuffer();
						CurLODInfo.OverrideVertexColors->InitFromColorArray( OverrideColors );
					}
					BeginInitResource(CurLODInfo.OverrideVertexColors);
				}
			}
		}
	}
};

/**
 * Internal helper function to convert selected brushes into volumes of the provided class.
 *
 * @param	VolumeClass	Class of volume that selected brushes should be converted into
 */
void ConvertSelectedBrushesToVolumes( UClass* VolumeClass )
{
	checkSlow( VolumeClass && VolumeClass->IsChildOf( AVolume::StaticClass() ) );

	// Cache off the current level so it can be restored after this operation is complete.
	// The current level may need to be changed in the process of converting if the user has selected brushes
	// that aren't in the current level.
	ULevel* PrevCurrentLevel = GWorld->CurrentLevel;
	check( PrevCurrentLevel );

	// Iterate over all selected actors, converting the brushes to volumes of the provided class
	for ( FSelectionIterator SelectedActorIter( GEditor->GetSelectedActorIterator() ); SelectedActorIter; ++SelectedActorIter )
	{
		AActor* CurSelectedActor = Cast<AActor>( *SelectedActorIter );
		check( CurSelectedActor );

		if ( CurSelectedActor->IsABrush() && !CurSelectedActor->IsABuilderBrush() )
		{
			ABrush* CurBrushActor = Cast<ABrush>( CurSelectedActor );
			check( CurBrushActor );
			
			ULevel* CurActorLevel = CurBrushActor->GetLevel();
			check( CurActorLevel );

			// Make the actor's level current so that the newly created volume will be placed on the correct level
			GWorld->CurrentLevel = CurActorLevel;

			ABrush* NewVolume = Cast<ABrush>( GWorld->SpawnActor( VolumeClass, NAME_None, CurBrushActor->Location ) );
			if ( NewVolume )
			{
				NewVolume->PreEditChange( NULL );

				FBSPOps::csgCopyBrush( NewVolume, CurBrushActor, 0, RF_Transactional, TRUE, TRUE );

				// Set the texture on all polys to NULL.  This stops invisible texture
				// dependencies from being formed on volumes.
				if( NewVolume->Brush )
				{
					for ( TArray<FPoly>::TIterator PolyIter( NewVolume->Brush->Polys->Element ); PolyIter; ++PolyIter )
					{
						FPoly& CurPoly = *PolyIter;
						CurPoly.Material = NULL;
					}
				}

				NewVolume->PostEditChange();
			}
		}
	}
	GEditor->edactDeleteSelected( FALSE, TRUE );

	// Restore the current level
	GWorld->CurrentLevel = PrevCurrentLevel;
}

void WxEditorFrame::MenuConvertToBlockingVolume( wxCommandEvent& In )
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("ConvertToBlockingVolume")) );
	ConvertSelectedBrushesToVolumes( ABlockingVolume::StaticClass() );
}

void WxEditorFrame::MenuSetCollisionBlockAll( wxCommandEvent& In )
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("SetCollision")) );

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AStaticMeshActor* Actor				= static_cast<AStaticMeshActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if( Actor )
		{
			Actor->PreEditChange(NULL);
			Actor->Modify();
			Actor->SetCollisionType( COLLIDE_BlockAll );
			Actor->PostEditChange();
		}
	}
}

void WxEditorFrame::MenuSetCollisionBlockWeapons( wxCommandEvent& In )
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("SetCollision")) );

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AStaticMeshActor* Actor				= static_cast<AStaticMeshActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if( Actor )
		{
			Actor->PreEditChange(NULL);
			Actor->Modify();
			Actor->SetCollisionType( COLLIDE_BlockWeapons );
			Actor->PostEditChange();
		}
	}
}

void WxEditorFrame::MenuSetCollisionBlockNone( wxCommandEvent& In )
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("SetCollision")) );

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AStaticMeshActor* Actor				= static_cast<AStaticMeshActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if( Actor )
		{
			Actor->PreEditChange(NULL);
			Actor->Modify();
			Actor->SetCollisionType( COLLIDE_NoCollision );
			Actor->PostEditChange();
		}
	}
}

/** Utility for converting between StaticMeshes, KActors and InterpActors (Movers). */
void WxEditorFrame::MenuConvertActors( wxCommandEvent& In )
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("ConvertMeshes")) );

	const UBOOL bFromStaticMesh = (
		In.GetId() == IDMENU_ActorPopupConvertStaticMeshToKActor || 
		In.GetId() == IDMENU_ActorPopupConvertStaticMeshToMover || 
		In.GetId() == IDMENU_ActorPopupConvertStaticMeshToFSMA || 
		In.GetId() == IDMENU_ActorPopupConvertStaticMeshToSMBasedOnExtremeContent ||
		In.GetId() == IDMENU_ActorPopupConvertStaticMeshToInteractiveFoliageMesh);

	const UBOOL bFromKActor = (In.GetId() == IDMENU_ActorPopupConvertKActorToStaticMesh || In.GetId() == IDMENU_ActorPopupConvertKActorToMover);
	const UBOOL bFromMover = (In.GetId() == IDMENU_ActorPopupConvertMoverToStaticMesh || In.GetId() == IDMENU_ActorPopupConvertMoverToKActor);
	const UBOOL bFromFSMA = (In.GetId() == IDMENU_ActorPopupConvertFSMAToStaticMesh);
	const UBOOL bFromInteractiveFoliage = (In.GetId() == IDMENU_ActorPopupConvertInteractiveFoliageMeshToStaticMesh);

	const UBOOL bToStaticMesh = (
		In.GetId() == IDMENU_ActorPopupConvertKActorToStaticMesh || 
		In.GetId() == IDMENU_ActorPopupConvertMoverToStaticMesh || 
		In.GetId() == IDMENU_ActorPopupConvertFSMAToStaticMesh ||
		In.GetId() == IDMENU_ActorPopupConvertInteractiveFoliageMeshToStaticMesh);

	const UBOOL bToKActor = (In.GetId() == IDMENU_ActorPopupConvertStaticMeshToKActor || In.GetId() == IDMENU_ActorPopupConvertMoverToKActor);
	const UBOOL bToMover = (In.GetId() == IDMENU_ActorPopupConvertStaticMeshToMover || In.GetId() == IDMENU_ActorPopupConvertKActorToMover);
	const UBOOL bToFSMA = (In.GetId() == IDMENU_ActorPopupConvertStaticMeshToFSMA);
	const UBOOL bToSMBasedOnExtremeContent = (In.GetId() == IDMENU_ActorPopupConvertStaticMeshToSMBasedOnExtremeContent);
	const UBOOL bToInteractiveFoliage = (In.GetId() == IDMENU_ActorPopupConvertStaticMeshToInteractiveFoliageMesh);

	TArray<AActor*>				SourceActors;
	TArray<FConvertStaticMeshActorInfo>	ConvertInfo;

	// Provide the option to abort up-front.
	UBOOL bIgnoreKismetReferenced = FALSE;
	if ( GUnrealEd->ShouldAbortActorDeletion( bIgnoreKismetReferenced ) )
	{
		return;
	}

	// Iterate over selected Actors.
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor				= static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// Reject Kismet-referenced actors, if the user specified.
		if ( bIgnoreKismetReferenced && Actor->IsReferencedByKismet() )
		{
			continue;
		}

		AStaticMeshActor* SMActor				= bFromStaticMesh ? Cast<AStaticMeshActor>(Actor) : NULL;
		AKActor* KActor							= bFromKActor ? Cast<AKActor>(Actor) : NULL;
		AInterpActor* InterpActor				= bFromMover ? Cast<AInterpActor>(Actor) : NULL;
		AFracturedStaticMeshActor* FracActor	= bFromFSMA ? Cast<AFracturedStaticMeshActor>(Actor) : NULL;
		AInteractiveFoliageActor* FoliageActor	= bFromInteractiveFoliage ? Cast<AInteractiveFoliageActor>(Actor) : NULL;

		const UBOOL bFoundActorToConvert = SMActor || KActor || InterpActor || FracActor || FoliageActor;
		if ( bFoundActorToConvert )
		{
			// If its the type we are converting 'from' copy its properties and remember it.
			FConvertStaticMeshActorInfo Info;
			appMemzero(&Info, sizeof(FConvertStaticMeshActorInfo));

			UBOOL bFoundActorToConvert = FALSE;
			if( SMActor )
			{
				SourceActors.AddItem(Actor);
				Info.GetFromActor(SMActor, SMActor->StaticMeshComponent);
			}
			else if( KActor )
			{
				SourceActors.AddItem(Actor);
				Info.GetFromActor(KActor, KActor->StaticMeshComponent);
			}
			else if( InterpActor )
			{
				SourceActors.AddItem(Actor);
				Info.GetFromActor(InterpActor, InterpActor->StaticMeshComponent);
			}
			else if( FracActor )
			{
				SourceActors.AddItem(Actor);
				Info.GetFromActor(FracActor, FracActor->FracturedStaticMeshComponent);

				// Change info so it contains the unfractured 'source' mesh instead of the fractured version
				UFracturedStaticMesh* FracMesh = CastChecked<UFracturedStaticMesh>(Info.StaticMesh);
				Info.StaticMesh = FracMesh->SourceStaticMesh;
			}
			else if( FoliageActor )
			{
				SourceActors.AddItem(Actor);
				Info.GetFromActor(FoliageActor, FoliageActor->StaticMeshComponent);
			}

			// Get the sequence corresponding to the actor's level.
			ULevel* SourceLevel = Actor->GetLevel();
			USequence* RootSeq = GWorld->GetGameSequence( SourceLevel );
			if ( RootSeq )
			{
				// Look through Kismet to find any references to this Actor and remember them.
				TArray<USequenceObject*> SeqObjects;
				RootSeq->FindSeqObjectsByObjectName(Actor->GetFName(), SeqObjects);
				for(INT i=0; i<SeqObjects.Num(); i++)
				{
					USequenceEvent* Event = Cast<USequenceEvent>( SeqObjects(i) );
					if(Event)
					{
						check(Event->Originator == Actor);
						Info.KismetEvents.AddUniqueItem(Event);
					}

					USeqVar_Object* ObjVar = Cast<USeqVar_Object>( SeqObjects(i) );
					if(ObjVar)
					{
						check(ObjVar->ObjValue == Actor);
						Info.KismetObjectVars.AddUniqueItem(ObjVar);
					}
				}
			}

			// Get the actor group if any
			Info.ActorGroup = AGroupActor::GetParentForActor(Actor);

			const INT NewIndex = ConvertInfo.AddZeroed();
			ConvertInfo(NewIndex) = Info;
		}
	}

	// If going to FSMA, only leave meshes in SourceActors which have a FSM based on them.
	if(bToFSMA)
	{
		TArray<AActor*> NewSourceActors;
		TArray<FConvertStaticMeshActorInfo>	NewConvertInfo;

		// Used to save which mesh you want to use - avoid answering question multiple times
		TMap<UStaticMesh*, UFracturedStaticMesh*> FracMeshMap;

		check(SourceActors.Num() == ConvertInfo.Num());
		for(INT i=0; i<SourceActors.Num(); i++)
		{
			AStaticMeshActor* SMActor = Cast<AStaticMeshActor>( SourceActors(i) );
			if( SMActor && 
				SMActor->StaticMeshComponent && 
				SMActor->StaticMeshComponent->StaticMesh )
			{
				// Find set of FSMs based on this mesh
				TArray<UFracturedStaticMesh*> FracMeshes = FindFracturedVersionsOfMesh(ConvertInfo(i).StaticMesh);
				if(FracMeshes.Num() > 0)
				{
					NewSourceActors.AddItem(SourceActors(i));
					INT SrcIndex = NewConvertInfo.AddItem(ConvertInfo(i));
					
					// If just one, easy case - just use it
					if(FracMeshes.Num() == 1)
					{
						NewConvertInfo(SrcIndex).StaticMesh = FracMeshes(0);
					}
					// More than one - need to let user pick
					else
					{
						// 
						UFracturedStaticMesh** CachedFracMesh = FracMeshMap.Find(ConvertInfo(i).StaticMesh);
						if(CachedFracMesh && *CachedFracMesh)
						{
							NewConvertInfo(SrcIndex).StaticMesh = *CachedFracMesh;
						}
						else
						{
							// Make array of text names
							TArray<FString> FracNames;
							for(INT FracIndex=0; FracIndex < FracMeshes.Num(); FracIndex++)
							{
								FracNames.AddItem(FracMeshes(FracIndex)->GetName());
							}

							// Show combo to let user pick
							WxDlgGenericComboEntry FracDlg;
							if( FracDlg.ShowModal( TEXT("ChooseFracturedMesh"), TEXT("FracturedStaticMesh"), FracNames, 0, TRUE ) == wxID_OK )
							{
								INT UseFracIndex = FracDlg.GetSelectedIndex();
								// Remember this choice
								FracMeshMap.Set(ConvertInfo(i).StaticMesh, FracMeshes(UseFracIndex));
								// Use this FSM
								NewConvertInfo(SrcIndex).StaticMesh = FracMeshes(UseFracIndex);
							}
							// Hit cancel - don't replace this mesh
							else
							{
								NewSourceActors.Remove(SrcIndex);
								NewConvertInfo.Remove(SrcIndex);
							}
						}
					}
				}
			}
		}

		SourceActors = NewSourceActors;
		ConvertInfo = NewConvertInfo;
	}

	// Then clear selection, select and delete the source actors.
	GEditor->SelectNone( FALSE, FALSE );
	for( INT ActorIndex = 0 ; ActorIndex < SourceActors.Num() ; ++ActorIndex )
	{
		AActor* SourceActor = SourceActors(ActorIndex);
		GEditor->SelectActor( SourceActor, TRUE, NULL, FALSE );
	}

	if ( GUnrealEd->edactDeleteSelected( FALSE, bIgnoreKismetReferenced ) )
	{
		// Now we need to spawn some new actors at the desired locations.
		ULevel* OldCurrentLevel = GWorld->CurrentLevel;
		for( INT i = 0 ; i < ConvertInfo.Num() ; ++i )
		{
			FConvertStaticMeshActorInfo& Info = ConvertInfo(i);

			// Spawn correct type, and copy properties from intermediate struct.
			AActor* Actor = NULL;
			if( bToStaticMesh )
			{
				// Make current the level into which the new actor is spawned.
				GWorld->CurrentLevel = Info.SourceLevel;
				AStaticMeshActor* SMActor = CastChecked<AStaticMeshActor>( GWorld->SpawnActor(AStaticMeshActor::StaticClass(), NAME_None, Info.Location, Info.Rotation) );
				SMActor->ClearComponents();
				Info.SetToActor(SMActor, SMActor->StaticMeshComponent);
				SMActor->ConditionalUpdateComponents();
				GEditor->SelectActor( SMActor, TRUE, NULL, FALSE );
				Actor = SMActor;
			}
			else if( bToKActor )
			{
				// Make current the level into which the new actor is spawned.
				GWorld->CurrentLevel = Info.SourceLevel;
				AKActor* KActor = CastChecked<AKActor>( GWorld->SpawnActor(AKActor::StaticClass(), NAME_None, Info.Location, Info.Rotation) );
				KActor->ClearComponents();
				Info.SetToActor(KActor, KActor->StaticMeshComponent);
				KActor->ConditionalUpdateComponents();
				GEditor->SelectActor( KActor, TRUE, NULL, FALSE );
				Actor = KActor;
			}
			else if( bToMover )
			{
				// Make current the level into which the new actor is spawned.
				GWorld->CurrentLevel = Info.SourceLevel;
				AInterpActor* InterpActor = CastChecked<AInterpActor>( GWorld->SpawnActor(AInterpActor::StaticClass(), NAME_None, Info.Location, Info.Rotation) );
				InterpActor->ClearComponents();
				Info.SetToActor(InterpActor, InterpActor->StaticMeshComponent);
				InterpActor->ConditionalUpdateComponents();
				GEditor->SelectActor( InterpActor, TRUE, NULL, FALSE );
				Actor = InterpActor;
			}
			else if(bToFSMA)
			{
				// Make current the level into which the new actor is spawned.
				GWorld->CurrentLevel = Info.SourceLevel;
				AFracturedStaticMeshActor* FSMActor = CastChecked<AFracturedStaticMeshActor>( GWorld->SpawnActor(AFracturedStaticMeshActor::StaticClass(), NAME_None, Info.Location, Info.Rotation) );
				FSMActor->ClearComponents();
				Info.SetToActor(FSMActor, FSMActor->FracturedStaticMeshComponent);
				FSMActor->ConditionalUpdateComponents();
				GEditor->SelectActor( FSMActor, TRUE, NULL, FALSE );
				Actor = FSMActor;
			}
			else if(bToSMBasedOnExtremeContent)
			{
				// Make current the level into which the new actor is spawned.
				GWorld->CurrentLevel = Info.SourceLevel;
				AStaticMeshActorBasedOnExtremeContent* SMActor = CastChecked<AStaticMeshActorBasedOnExtremeContent>( GWorld->SpawnActor(AStaticMeshActorBasedOnExtremeContent::StaticClass(), NAME_None, Info.Location, Info.Rotation) );
				SMActor->ClearComponents();
				Info.SetToActor(SMActor, SMActor->StaticMeshComponent);
				SMActor->ConditionalUpdateComponents();
				GEditor->SelectActor( SMActor, TRUE, NULL, FALSE );
				Actor = SMActor;
			}
			else if(bToInteractiveFoliage)
			{
				// Make current the level into which the new actor is spawned.
				GWorld->CurrentLevel = Info.SourceLevel;
				AInteractiveFoliageActor* FoliageActor = CastChecked<AInteractiveFoliageActor>( GWorld->SpawnActor(AInteractiveFoliageActor::StaticClass(), NAME_None, Info.Location, Info.Rotation) );
				FoliageActor->ClearComponents();
				Info.SetToActor(FoliageActor, FoliageActor->StaticMeshComponent);
				FoliageActor->ConditionalUpdateComponents();
				GEditor->SelectActor( FoliageActor, TRUE, NULL, FALSE );
				Actor = FoliageActor;
			}

			// Fix up Kismet events and obj vars to new Actor.  Also fixup the actor group.
			if( Actor )
			{
				for(INT j=0; j<Info.KismetEvents.Num(); j++)
				{
					Info.KismetEvents(j)->Originator = Actor;
				}

				for(INT j=0; j<Info.KismetObjectVars.Num(); j++)
				{
					Info.KismetObjectVars(j)->ObjValue = Actor;
				}

				if( Info.ActorGroup )
				{
					Info.ActorGroup->Add(*Actor);
				}
			}
		}

		// Restore the current level.
		GWorld->CurrentLevel = OldCurrentLevel;
	}

	GEditor->NoteSelectionChange();
}



/** Called when "Allow Translucent Selection" is clicked */
void WxEditorFrame::Clicked_SelectTranslucent( wxCommandEvent& In )
{
	// Toggle 'allow select translucent'
	GEditor->AccessUserSettings().bAllowSelectTranslucent = In.IsChecked();
	GEditor->SaveUserSettings();

	// Need to refresh hit proxies as we changed what should be rendered into them
	GUnrealEd->RedrawAllViewports();
}



/** Called to update the UI state of the "Allow Translucent Selection" button */
void WxEditorFrame::UpdateUI_SelectTranslucent( wxUpdateUIEvent& In )
{
	In.Check( GEditor->GetUserSettings().bAllowSelectTranslucent == TRUE );
}



/** Called when "Only Load Visible Levels in PIE" is clicked */
void WxEditorFrame::Clicked_PIEVisibleOnly( wxCommandEvent& In )
{
	// Toggle 'PIE visible only'
	GEditor->AccessUserSettings().bOnlyLoadVisibleLevelsInPIE = In.IsChecked();
	GEditor->SaveUserSettings();
}



/** Called to update the UI state of the "Only Load Visible Levels in PIE" button */
void WxEditorFrame::UpdateUI_PIEVisibleOnly( wxUpdateUIEvent& In )
{
	In.Check( GEditor->GetUserSettings().bOnlyLoadVisibleLevelsInPIE == TRUE );
}


/** Called when "Emulate Mobile Features" is clicked */
void WxEditorFrame::Clicked_EmulateMobileFeatures( wxCommandEvent& In )
{
	// Toggle preference
	GEditor->AccessUserSettings().bEmulateMobileFeatures = !GEditor->GetUserSettings().bEmulateMobileFeatures;
	GEditor->SaveUserSettings();

	SetMobileRenderingEmulation( GEditor->GetUserSettings().bEmulateMobileFeatures, GUseGammaCorrectionForMobileEmulation );

	GAlwaysOptimizeContentForMobile = GEditor->AccessUserSettings().bAlwaysOptimizeContentForMobile || GEditor->GetUserSettings().bEmulateMobileFeatures;
	GEmulateMobileInput = GEditor->GetUserSettings().bEmulateMobileFeatures;

	// Redraw the viewports
	GUnrealEd->RedrawAllViewports();
}


/** Called to update the UI state of the "Emulate Mobile Features" button */
void WxEditorFrame::UpdateUI_EmulateMobileFeatures( wxUpdateUIEvent& In )
{
	In.Check( GEditor->GetUserSettings().bEmulateMobileFeatures ? true : false );
}


/** Called when "Always Optimize Content for Mobile" is clicked */
void WxEditorFrame::Clicked_AlwaysOptimizeContentForMobile( wxCommandEvent& In )
{
	// Toggle preference
	GEditor->AccessUserSettings().bAlwaysOptimizeContentForMobile = !GEditor->GetUserSettings().bAlwaysOptimizeContentForMobile;
	GEditor->SaveUserSettings();

	GAlwaysOptimizeContentForMobile = GEditor->AccessUserSettings().bAlwaysOptimizeContentForMobile || GEditor->GetUserSettings().bEmulateMobileFeatures;
}

/** Called to update the UI state of the "Always Optimize Content for Mobile" button */
void WxEditorFrame::UpdateUI_AlwaysOptimizeContentForMobile( wxUpdateUIEvent& In )
{
	In.Check( GEditor->GetUserSettings().bAlwaysOptimizeContentForMobile ? true : false );
}

#if WITH_SIMPLYGON
/**
 * Initiates mesh simplification from an actor pop up menu
 */
void WxEditorFrame::MenuActorSimplifyMesh( wxCommandEvent& In )
{
	// NOTE: This command only supports operating on a single static mesh at a time, since we need to summon
	//    a modeless window where the rest of the setup will occur.

	// Iterate over selected actors.
	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ); It != NULL; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA( AActor::StaticClass() ) );

		UStaticMesh* StaticMesh = NULL;

		// StaticMeshActor
		AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>( Actor );
		if( StaticMeshActor != NULL )
		{
			// All static mesh actors should have one of these!
			check( StaticMeshActor->StaticMeshComponent != NULL );

			// Make sure we have a static mesh
			StaticMesh = StaticMeshActor->StaticMeshComponent->StaticMesh;
		}


		// DynamicSMActor
		ADynamicSMActor* DynamicSMActor = Cast<ADynamicSMActor>( Actor );
		if( DynamicSMActor != NULL )
		{
			// All dynamic static mesh actors should have one of these!
			check( DynamicSMActor->StaticMeshComponent != NULL );

			// Make sure we have a static mesh
			StaticMesh = DynamicSMActor->StaticMeshComponent->StaticMesh;
		}



		if( StaticMesh != NULL )
		{
			// OK, launch the static mesh editor (in 'simplify mode') with the selected mesh
			WxStaticMeshEditor* StaticMeshEditor =
				new WxStaticMeshEditor( GApp->EditorFrame, -1, StaticMesh, TRUE /*bForceSimplificationWindowVisible*/ );
			StaticMeshEditor->Show( TRUE );
		}
		else
		{
			// No mesh associated with this actor's static mesh component!
			appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MeshSimp_NoMeshAssignedToStaticMeshActor_F" ), *Actor->GetName() ) ) );
		}

		// Only one mesh supported at a time (due to modeless dialog fun)
		break;

	}
}

/** Simplifies all static meshes selected by the user. */
void WxEditorFrame::MenuActorSimplifySelectedMeshes( wxCommandEvent& In )
{
	FLOAT MaxDeviationPercentage = 0.0f;
	{
		WxDlgGenericSlider QualityDialog( this );
		if( QualityDialog.ShowModal( *LocalizeUnrealEd( TEXT("MeshSimp_SimplifyAllSelectedDialogTitle") ), *LocalizeUnrealEd( TEXT("MeshSimp_DesiredQuality") ), 0, 100, 90 ) == wxID_OK )
		{
			const INT DesiredQuality = Clamp<INT>( QualityDialog.GetValue(), 0, 100 );
			MaxDeviationPercentage = (100 - DesiredQuality) / 2000.0f;
		}
	}

	TSet<UStaticMesh*> StaticMeshesToOptimize;
	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ); It != NULL; ++It )
	{
		UStaticMesh* StaticMesh = NULL;
		AActor* Actor = CastChecked<AActor>( *It );

		if( Actor->IsA( AStaticMeshActor::StaticClass() ) )
		{
			AStaticMeshActor* StaticMeshActor = (AStaticMeshActor*)Actor;
			check( StaticMeshActor->StaticMeshComponent != NULL );
			StaticMesh = StaticMeshActor->StaticMeshComponent->StaticMesh;
		}
		else if( Actor->IsA( ADynamicSMActor::StaticClass() ) )
		{
			ADynamicSMActor* DynamicSMActor = (ADynamicSMActor*)Actor;
			check( DynamicSMActor->StaticMeshComponent != NULL );
			StaticMesh = DynamicSMActor->StaticMeshComponent->StaticMesh;
		}
		if ( StaticMesh )
		{
			StaticMeshesToOptimize.Add( StaticMesh );
		}
	}

	const INT MeshCount = StaticMeshesToOptimize.Num();
	INT MeshIndex = 1;
	GWarn->BeginSlowTask( *LocalizeUnrealEd( TEXT("MeshSimp_SimplifyingMeshes") ), TRUE );
	for ( TSet<UStaticMesh*>::TIterator It( StaticMeshesToOptimize ); It; ++It )
	{
		UStaticMesh* StaticMesh = *It;
		FStaticMeshOptimizationSettings OptimizationSettings;
		OptimizationSettings.MaxDeviationPercentage = MaxDeviationPercentage;
		GWarn->StatusUpdatef( MeshIndex++, MeshCount, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MeshSimp_SimplifyingMesh_F" ), 0 /*LODIndex*/ , *StaticMesh->GetName() ) ) );
		SimplygonMeshUtilities::OptimizeStaticMesh( StaticMesh, StaticMesh, 0, OptimizationSettings );
	}
	GWarn->EndSlowTask();
}
#endif // #if WITH_SIMPLYGON
#if ENABLE_SIMPLYGON_MESH_PROXIES
/** Creates a proxy for the selected static meshes. */
void WxEditorFrame::MenuActorCreateMeshProxy( wxCommandEvent& In )
{
	WxDlgCreateMeshProxy* CreateMeshProxyDialog = GApp->GetDlgCreateMeshProxy();
	check( CreateMeshProxyDialog );
	CreateMeshProxyDialog->Show(true);
}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES

/**
* Converts the selected StaticMesh into a navmesh, and creates a Pylon associated with it
* Deletes the selected StaticMesh before returning
*/
void WxEditorFrame::MenuActorConvertStaticMeshToNavMesh( wxCommandEvent& In )
{
	// Iterate over selected actors.
	TArray<AStaticMeshActor*> SMActorList;

	FVector AvgPos(0.f);

	USelection* SelectedActors = GEditor->GetSelectedActors();

	TArray<AActor*> DeselectList;
	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ); It != NULL; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA( AActor::StaticClass() ) );


		// StaticMeshActor
		AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>( Actor );
		if( StaticMeshActor != NULL )
		{
			SMActorList.AddItem(StaticMeshActor);
			// mark it as modified so undo/redo works!
			StaticMeshActor->Modify();
			AvgPos += StaticMeshActor->Location;
		}
		else
		{
			// if it's not a static mesh actor deselect it so we don't delete it errantly :) 
			DeselectList.AddItem(Actor);
		}
	}

	for(INT Idx=0;Idx<DeselectList.Num();++Idx)
	{
		SelectedActors->Deselect(DeselectList(Idx));
	}
			
	APylon* NewPylon = NULL;
	if(SMActorList.Num() > 0)
	{
		AvgPos/=SMActorList.Num();

		// Create Pylon to add all the meshes to
		UClass* PylonClass = GEditor->GetClassFromPairMap( FString(TEXT("Pylon")) );
		NewPylon = Cast<APylon>(GWorld->SpawnActor(PylonClass, NAME_None, AvgPos, FRotator::ZeroRotator, NULL, TRUE));

		// Make sure Pylons loaded from the level are in the navigation octree
		GWorld->RemoveLevelNavList(GWorld->CurrentLevel);
		GWorld->AddLevelNavList(GWorld->CurrentLevel);

		// Mark this pylon as imported
		NewPylon->bImportedMesh = TRUE;
				
		// Add Pylon to level
		GWorld->CurrentLevel->AddToNavList(NewPylon);

		NewPylon->AddStaticMeshesToPylon(SMActorList);

		// delete 
		GEditor->edactDeleteSelected( FALSE, TRUE );

		for( APylon* PylonItr = GWorld->CurrentLevel->PylonListStart; PylonItr != NULL; PylonItr = PylonItr->NextPylon )
		{
			PylonItr->ForceUpdateComponents();
		}

		// Refresh the level navlist with the added pylon included
		GWorld->RemoveLevelNavList(GWorld->CurrentLevel);
		GWorld->AddLevelNavList(GWorld->CurrentLevel);
	}
}


/**
 * Sets current LOD parent actor
 */
void WxEditorFrame::MenuActorSetLODParent( wxCommandEvent& In )
{
	// there will be only one actor selected, just use first in iterator
	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ); It != NULL; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );

		// set the current LOD parent 
		// @todo: would be nice if this turned a nice shade of magenta or something :)
		GUnrealEd->CurrentLODParentActor = Actor;
		break;
	}
}

/**
 * Sets the LOD parent actor as the replacement for the selected actors
 */
void WxEditorFrame::MenuActorAddToLODParent( wxCommandEvent& In )
{
	if (GUnrealEd->CurrentLODParentActor)
	{
		// get all selected actors
		TArray<AActor*> SelectedActors;
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			SelectedActors.AddItem( Actor );

		}

		// now set their replacement
		GUnrealEd->AssignReplacementComponentsByActors(SelectedActors, GUnrealEd->CurrentLODParentActor);
	}
}

/**
 * Clears the replacement for the selected actors
 */
void WxEditorFrame::MenuActorRemoveFromLODParent( wxCommandEvent& In )
{
	// get all selected actors
	TArray<AActor*> SelectedActors;
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		SelectedActors.AddItem( Actor );

	}

	// now clear their replacement
	if (SelectedActors.Num() > 0)
	{
		GUnrealEd->AssignReplacementComponentsByActors(SelectedActors, NULL);
	}
}



void WxEditorFrame::MenuSetLightDataBasedOnClassification( wxCommandEvent& In )
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("ChangeLightClassification")) );

	const UBOOL bToLightDynamicAffecting = ( In.GetId() == IDMENU_IDMENU_ActorPopupConvertLightToLightDynamicAffecting );
	const UBOOL bToLightStaticAffecting = ( In.GetId() == IDMENU_IDMENU_ActorPopupConvertLightToLightStaticAffecting );
	const UBOOL bToLightDynamicsAndStaticAffecting = ( In.GetId() == IDMENU_IDMENU_ActorPopupConvertLightToLightDynamicAndStaticAffecting );

	// Iterate over selected actors.
	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		ALight* LightActor	= Cast<ALight>(Actor);

		if( ( LightActor != NULL ) && ( LightActor->LightComponent != NULL ) )
		{
			if( bToLightDynamicAffecting == TRUE )
			{
				LightActor->SetValuesForLight_DynamicAffecting();
			}
			else if( bToLightStaticAffecting == TRUE )
			{
				LightActor->SetValuesForLight_StaticAffecting();
			}
			else if ( bToLightDynamicsAndStaticAffecting == TRUE )
			{
				LightActor->SetValuesForLight_DynamicAndStaticAffecting();
			}

			// now set the icon as we have all the data we need
			LightActor->DetermineAndSetEditorIcon();

			// We need to invalidate the cached lighting as we might have toggled lightmap vs no lightmap.
			LightActor->InvalidateLightingCache();
		}
	}

	GEditor->NoteSelectionChange();
}

/* Called when a user selects a convert lights menu option */
void WxEditorFrame::MenuConvertLights( wxCommandEvent& In )
{
	const INT CommandID = In.GetId();
	// Provide the option to abort the conversion if e.g. Kismet-referenced actors are selected.
	UBOOL bIgnoreKismetReferenced = FALSE;
	if ( GEditor->ShouldAbortActorDeletion(bIgnoreKismetReferenced) )
	{
		return;
	}

	// List of actors to convert
	TArray< AActor* > ActorsToConvert;

	// Support conversion to an from these types of lights and their child classes
	TArray< UClass* > SupportedLightClasses;
	SupportedLightClasses.AddItem( APointLight::StaticClass() );
	SupportedLightClasses.AddItem( ASpotLight::StaticClass() );
	SupportedLightClasses.AddItem( ADirectionalLight::StaticClass() );
	SupportedLightClasses.AddItem( ASkyLight::StaticClass() );

	// The class we will convert all selected lights to
	UClass* ConvertToClass = NULL;

	INT ClassID = IDMENU_ConvertLights_START;
	// Build a list of ID's for the different light classes, so we know which menu item was clicked on.
	// This is computed exactly the same way as when the menu was built.
	for( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
	{
		UClass* CurClass = *ClassIt;

		// Skip non-placables and non lights and UT lights.
		if( (CurClass->ClassFlags & CLASS_Placeable) && CurClass->IsChildOf( ALight::StaticClass() ) )
		{
			// Check to make sure this type of light is supported
			UBOOL bLightClassSupported = FALSE;
			for( INT SupportedIdx = 0; SupportedIdx < SupportedLightClasses.Num(); ++SupportedIdx )
			{
				if( CurClass->IsChildOf( SupportedLightClasses( SupportedIdx) ) )
				{
					bLightClassSupported = TRUE;
					break;
				}
			}

			if( bLightClassSupported )
			{
				if( ClassID == CommandID )
				{
					// We found the same light class that was clicked on in the menu
					ConvertToClass = CurClass;
					break;
				}
				++ClassID;
			}
		}
	}

	// The user should never be able to select a light class they cant convert to.
	check(ConvertToClass);

	// Get a list of valid actors to convert.
	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* ActorToConvert = static_cast<AActor*>( *It );
		// Prevent non light actors from being converted
		// Also prevent light actors from being converted if they are the same time as the new class
		if( ActorToConvert->IsA( ALight::StaticClass() ) && ActorToConvert->GetClass() != ConvertToClass )
		{
			ActorsToConvert.AddItem( ActorToConvert );
		}
	}

	// Undo/Redo support
	const FScopedTransaction Transaction( *LocalizeUnrealEd("ConvertLights") );
	{
		// Convert the lights!
		GEditor->ConvertLightActors( ActorsToConvert, ConvertToClass );
	}
	
}

/** Called when a convert brush to volume menu option is selected */
void WxEditorFrame::MenuConvertVolumes( wxCommandEvent& In )
{
	// Find which volume class the user selected based on the menu id
	TArray<UClass*> VolumeClasses;
	GetSortedVolumeClasses( &VolumeClasses );

	const INT ClickedMenuId = In.GetId();
	const INT VolumeIndex = ClickedMenuId - IDM_ConvertVolumeClasses_START;
	check( ClickedMenuId >= IDM_ConvertVolumeClasses_START && ClickedMenuId <= IDM_ConvertVolumeClasses_END );
	check( VolumeClasses.IsValidIndex( VolumeIndex ) );

	UClass* ConvertClass = VolumeClasses(VolumeIndex);
	check( ConvertClass );
	
	// Convert all selected brushes to the user's selected volume class
	const FScopedTransaction Transaction( *FString::Printf( LocalizeSecure( LocalizeUnrealEd("Transaction_ConvertToVolume"), *ConvertClass->GetName() ) ) );
	ConvertSelectedBrushesToVolumes( ConvertClass );
}

/** Called when the user selects a menu option from the convert skeletal mesh menu */
void WxEditorFrame::MenuConvertSkeletalMeshes( wxCommandEvent& In )
{
	const INT CommandID = In.GetId();

	// Converting will result in the deletion of the old actors, so confirm that
	// deletion is allowed in the first place.
	UBOOL bIgnoreKismetRefActors = FALSE;
	if ( GEditor->ShouldAbortActorDeletion( bIgnoreKismetRefActors ) )
	{
		return;
	}

	TArray<AActor*> ActorsToConvert;

	// Determine which class to convert to by reconstructing the menu options
	// and finding the matching ID
	UClass* ConvertToClass = NULL;
	INT ClassID = IDMENU_ConvertSkelMesh_START;
	for( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
	{
		UClass* CurClass = *ClassIt;
		if( ( CurClass->ClassFlags & CLASS_Placeable ) && CurClass->IsChildOf( ASkeletalMeshActor::StaticClass() ) )
		{
			if( ClassID == CommandID )
			{
				ConvertToClass = CurClass;
				break;
			}
			++ClassID;
		}
	}
	check( ConvertToClass );

	// Gather all of the actors valid for this conversion
	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ); It; ++It )
	{
		ASkeletalMeshActor* ActorToConvert = Cast<ASkeletalMeshActor>( *It );
		if( ActorToConvert && ActorToConvert->GetClass() != ConvertToClass )
		{
			ActorsToConvert.AddItem( ActorToConvert );
		}
	}

	// Make sure to copy the DLE and skeletal mesh components
	TSet<FString> ComponentsToCopy;
	ComponentsToCopy.Add( TEXT("MyLightEnvironment") );
	ComponentsToCopy.Add( TEXT("SkeletalMeshComponent0") );

	const FScopedTransaction Transaction( *LocalizeUnrealEd("ConvertSkeletalMesh") );
	{
		// Convert the actors!
		GEditor->ConvertActors( ActorsToConvert, ConvertToClass, ComponentsToCopy, bIgnoreKismetRefActors );

		// Special-case Hack: SkeletalMeshActorMATs expect their animations variable to be cleared via
		// their default properties, however converting from a skeletal mesh actor to a MAT
		// will copy over the animations because each instance makes a new animnode that the copy
		// code will consider unique because it's technically a different object, even if it's no
		// different from the default bitwise. To address this, forcefully remove the animations value. 
		if ( ConvertToClass->IsChildOf( ASkeletalMeshActorMAT::StaticClass() ) )
		{
			for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ); It; ++It )
			{
				ASkeletalMeshActorMAT* SkelMeshMAT = Cast<ASkeletalMeshActorMAT>( *It );
				if( SkelMeshMAT && SkelMeshMAT->SkeletalMeshComponent->Animations )
				{
					SkelMeshMAT->SkeletalMeshComponent->Animations->MarkPendingKill();
					SkelMeshMAT->SkeletalMeshComponent->Animations = NULL;
				}
			}
		}
	}
}

void WxEditorFrame::MenuSnapOriginToGrid( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR ALIGN ORIGIN") );
}

void WxEditorFrame::MenuQuantizeVertices( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR ALIGN VERTS") );
}

void WxEditorFrame::MenuConvertToStaticMesh( wxCommandEvent& In )
{
	WxDlgPackageGroupName dlg;

	UPackage* pkg = NULL;
	UPackage* grp = NULL;

	const FString PackageName = pkg ? pkg->GetName() : TEXT("MyPackage");
	const FString GroupName = grp ? grp->GetName() : TEXT("");

	if( dlg.ShowModal( PackageName, GroupName, TEXT("MyMesh") ) == wxID_OK )
	{
		FString Wk = FString::Printf( TEXT("STATICMESH FROM SELECTION PACKAGE=%s"), *dlg.GetPackage() );
		if( dlg.GetGroup().Len() > 0 )
		{
			Wk += FString::Printf( TEXT(" GROUP=%s"), *dlg.GetGroup() );
		}
		Wk += FString::Printf( TEXT(" NAME=%s"), *dlg.GetObjectName() );

		GUnrealEd->Exec( *Wk );
	}
}

void WxEditorFrame::MenuConvertToProcBuilding( wxCommandEvent& In )
{
	GUnrealEd->ConvertBSPToProcBuilding();
}

void WxEditorFrame::MenuActorBakePrePivot( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR BAKEPREPIVOT") );
}

void WxEditorFrame::MenuActorUnBakePrePivot( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR UNBAKEPREPIVOT") );
}

void WxEditorFrame::MenuActorPivotReset( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR RESET PIVOT") );
}

void WxEditorFrame::MenuActorMirrorX( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR MIRROR X=-1") );
}

void WxEditorFrame::MenuActorMirrorY( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR MIRROR Y=-1") );
}

void WxEditorFrame::MenuActorMirrorZ( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR MIRROR Z=-1") );
}

void WxEditorFrame::MenuActorSetDetailModeLow( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("SETDETAILMODE MODE=%d"), (INT)DM_Low ) );
}

void WxEditorFrame::MenuActorSetDetailModeMedium( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("SETDETAILMODE MODE=%d"), (INT)DM_Medium ) );
}

void WxEditorFrame::MenuActorSetDetailModeHigh( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("SETDETAILMODE MODE=%d"), (INT)DM_High ) );
}

void WxEditorFrame::MenuActorPopupMakeSolid( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("MAP SETBRUSH CLEARFLAGS=%d SETFLAGS=%d"), PF_Semisolid + PF_NotSolid, 0 ) );
}

void WxEditorFrame::MenuActorPopupMakeSemiSolid( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("MAP SETBRUSH CLEARFLAGS=%d SETFLAGS=%d"), (INT)(PF_Semisolid + PF_NotSolid), (INT)PF_Semisolid ) );
}

void WxEditorFrame::MenuActorPopupMakeNonSolid( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("MAP SETBRUSH CLEARFLAGS=%d SETFLAGS=%d"), (INT)(PF_Semisolid + PF_NotSolid), (INT)PF_NotSolid ) );
}

void WxEditorFrame::MenuActorPopupBrushSelectAdd( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP SELECT ADDS") );
}

void WxEditorFrame::MenuActorPopupBrushSelectSubtract( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP SELECT SUBTRACTS") );
}

void WxEditorFrame::MenuActorPopupBrushSelectNonSolid( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP SELECT NONSOLIDS") );
}

void WxEditorFrame::MenuActorPopupBrushSelectSemiSolid( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP SELECT SEMISOLIDS") );
}

/**
 * Forces all selected navigation points to position themselves as though building
 * paths (ie FindBase).
 */
void WxEditorFrame::MenuActorPopupPathPosition( wxCommandEvent& In )
{
	TArray<ANavigationPoint*> NavPts;
	GEditor->GetSelectedActors()->GetSelectedObjects<ANavigationPoint>(NavPts);
	for (INT Idx = 0; Idx < NavPts.Num(); Idx++)
	{
		NavPts(Idx)->FindBase();
	}
}

/**
 * Proscribes a path between all selected nodes.
 */
void WxEditorFrame::MenuActorPopupPathProscribe( wxCommandEvent& In )
{
	TArray<ANavigationPoint*> NavPts;
	GEditor->GetSelectedActors()->GetSelectedObjects<ANavigationPoint>(NavPts);
	for (INT Idx = 0; Idx < NavPts.Num(); Idx++)
	{
		for (INT ProscribeIdx = 0; ProscribeIdx < NavPts.Num(); ProscribeIdx++)
		{
			if (ProscribeIdx != Idx)
			{
				UBOOL bHasPath = FALSE;
				for (INT PathIdx = 0; PathIdx < NavPts(Idx)->EditorProscribedPaths.Num(); PathIdx++)
				{
					if (NavPts(Idx)->EditorProscribedPaths(PathIdx).Actor == NavPts(ProscribeIdx))
					{
						bHasPath = TRUE;
						break;
					}
				}
				if (!bHasPath)
				{
					// add to the list
					FActorReference NavRef(NavPts(ProscribeIdx),*NavPts(ProscribeIdx)->GetGuid());
					NavPts(Idx)->EditorProscribedPaths.AddItem(NavRef);
					// check to see if we're breaking an existing path
					UReachSpec *Spec = NavPts(Idx)->GetReachSpecTo(NavPts(ProscribeIdx));
					if (Spec != NULL)
					{
						// remove the old Spec
						NavPts(Idx)->PathList.RemoveItem(Spec);
					}
					// create a new Proscribed Spec
					NavPts(Idx)->ProscribePathTo(NavPts(ProscribeIdx));
				}
			}
		}
	}
}

/**
 * Forces a path between all selected nodes.
 */
void WxEditorFrame::MenuActorPopupPathForce( wxCommandEvent& In )
{
	TArray<ANavigationPoint*> NavPts;
	GEditor->GetSelectedActors()->GetSelectedObjects<ANavigationPoint>(NavPts);
	for (INT Idx = 0; Idx < NavPts.Num(); Idx++)
	{
		for (INT ForceIdx = 0; ForceIdx < NavPts.Num(); ForceIdx++)
		{
			if (ForceIdx != Idx)
			{
				// if not already ForceIdx
				UBOOL bHasPath = FALSE;
				for (INT PathIdx = 0; PathIdx < NavPts(Idx)->EditorForcedPaths.Num(); PathIdx++)
				{
					if (NavPts(Idx)->EditorForcedPaths(PathIdx).Actor == NavPts(ForceIdx))
					{
						bHasPath = TRUE;
						break;
					}
				}
				if (!bHasPath)
				{
					// add to the list
					FActorReference NavRef(NavPts(ForceIdx),*NavPts(ForceIdx)->GetGuid());
					NavPts(Idx)->EditorForcedPaths.AddItem(NavRef);
					// remove any normal spec
					UReachSpec *Spec = NavPts(Idx)->GetReachSpecTo(NavPts(ForceIdx));
					if (Spec != NULL)
					{
						NavPts(Idx)->PathList.RemoveItem(Spec);
					}
					// and create a new ForceIdxd Spec
					NavPts(Idx)->ForcePathTo(NavPts(ForceIdx));
				}
			}
		}
	}
}

/**
 * Assigns selected navigation points to selected route actors.
 */
void WxEditorFrame::MenuActorPopupPathAssignWayPointsToRoute( wxCommandEvent& In )
{
	TArray<ARoute*> Routes;
	TArray<AActor*> Points;

	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		ARoute* Route = Cast<ARoute>(*It);
		if( Route )
		{
			Routes.AddItem( Route );
			continue;
		}
		else
		{
			AActor* Point = Cast<AActor>(*It);
			if( Point )
			{
				Points.AddItem( Point );
				continue;
			}
		}
	}

	for( INT Idx = 0; Idx < Routes.Num(); Idx++ )
	{
		ARoute* Route = Routes(Idx);

		// Get fill action
		ERouteFillAction Action = RFA_Overwrite;
		if( In.GetId() == IDMENU_ActorPopupPathAddRoute )
		{
			Action = RFA_Add;
		}
		else
		if( In.GetId() == IDMENU_ActorPopupPathRemoveRoute )
		{
			Action = RFA_Remove;
		}
		else
		if( In.GetId() == IDMENU_ActorPopupPathClearRoute )
		{
			Action = RFA_Clear;
		}

		// Tell route to fill w/ points
		Route->AutoFillRoute( Action, Points );
	}
}

/**
 * Select all actors referenced by selected routes
 */
void WxEditorFrame::MenuActorPopupPathSelectWayPointsInRoute( wxCommandEvent& In )
{
	TArray<ARoute*> Routes;
	TArray<AActor*> Points;

	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		ARoute* Route = Cast<ARoute>(*It);
		if( Route )
		{
			Routes.AddItem( Route );
		}
	}

	for( INT Idx = 0; Idx < Routes.Num(); Idx++ )
	{
		ARoute* Route = Routes(Idx);

		for( INT PtIdx = 0; PtIdx < Route->RouteList.Num(); PtIdx++ )
		{
			Points.AddUniqueItem( ~Route->RouteList(PtIdx) );
		}
	}

	for( INT PtIdx = 0; PtIdx < Points.Num(); PtIdx++ )
	{
		GEditor->SelectActor( Points(PtIdx), TRUE, NULL, FALSE, TRUE );
	}
}


/**
 * Assigns selected navigation points to selected route actors.
 */
void WxEditorFrame::MenuActorPopupPathAssignLinksToCoverGroup( wxCommandEvent& In )
{
	TArray<ACoverGroup*>	Groups;
	TArray<ACoverLink*>		Links;

	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		ACoverGroup* Group = Cast<ACoverGroup>(*It);
		if( Group )
		{
			Groups.AddItem( Group );
			continue;
		}
		ACoverLink* Link = Cast<ACoverLink>(*It);
		if( Link )
		{
			Links.AddItem( Link );
			continue;
		}
	}

	for( INT Idx = 0; Idx < Groups.Num(); Idx++ )
	{
		ACoverGroup* Group = Groups(Idx);

		// Get fill action
		ECoverGroupFillAction Action = CGFA_Overwrite;
		if( In.GetId() == IDMENU_ActorPopupPathAddCoverGroup )
		{
			Action = CGFA_Add;
		}
		else
		if( In.GetId() == IDMENU_ActorPopupPathRemoveCoverGroup )
		{
			Action = CGFA_Remove;
		}
		else
		if( In.GetId() == IDMENU_ActorPopupPathClearCoverGroup )
		{
			Action = CGFA_Clear;
		}

		// Tell route to fill w/ Links
		Group->AutoFillGroup( Action, Links );
	}
}

void ClearProscribedReachSpecs(ANavigationPoint* Orig, ANavigationPoint* Dest)
{
	for (INT SpecIdx = Orig->PathList.Num()-1; SpecIdx >= 0; --SpecIdx)
	{
		if (Orig->PathList(SpecIdx) != NULL &&
			Orig->PathList(SpecIdx)->End == Dest &&
			Orig->PathList(SpecIdx)->IsProscribed())
		{
			Orig->PathList.Remove(SpecIdx,1);
		}
	}
}

void ClearProscribedPath(ANavigationPoint* Orig, ANavigationPoint* Dest)
{
	for (INT OrigIdx = Orig->EditorProscribedPaths.Num()-1; OrigIdx >= 0; --OrigIdx)
	{
		if (Orig->EditorProscribedPaths(OrigIdx).Actor == Dest)
		{
			Orig->EditorProscribedPaths.Remove(OrigIdx, 1);

			if(Dest != NULL)
			{
				for (INT DestIdx = Dest->EditorProscribedPaths.Num()-1; DestIdx >= 0; --DestIdx)
				{
					if (Dest->EditorProscribedPaths(DestIdx) == Orig)
					{
						Dest->EditorProscribedPaths.Remove(DestIdx, 1);
					}
				}
			}
		}
	}

	ClearProscribedReachSpecs(Orig, Dest);
	Orig->MarkComponentsAsDirty();

	if(Dest != NULL)
	{
		ClearProscribedReachSpecs(Dest, Orig);
		Dest->MarkComponentsAsDirty();
	}
}

/**
 * Clears all Proscribed paths between selected nodes, or if just one node
 * is selected, clears all of it's Proscribed paths.
 */
void WxEditorFrame::MenuActorPopupPathClearProscribed( wxCommandEvent& In )
{
	TArray<ANavigationPoint*> NavPts;
	GEditor->GetSelectedActors()->GetSelectedObjects<ANavigationPoint>(NavPts);
	if (NavPts.Num() == 1)
	{
		ANavigationPoint* OtherNav = (NavPts(0)->EditorProscribedPaths.Num() > 0) ? Cast<ANavigationPoint>(NavPts(0)->EditorProscribedPaths(0).Actor) : NULL;
		ClearProscribedPath(NavPts(0), OtherNav);
	}
	else
	{
		// clear any Proscribed points between the selected nodes
		for (INT Idx = 0; Idx < NavPts.Num(); ++Idx)
		{
			ANavigationPoint *Nav = NavPts(Idx);
			for (INT ProscribeIdx = 0; ProscribeIdx < NavPts.Num(); ++ProscribeIdx)
			{
				if (ProscribeIdx != Idx)
				{
					ClearProscribedPath(Nav, NavPts(ProscribeIdx));
				}
			}
		}
	}
}

/**
 * Clears all ForceIdxd paths between selected nodes, or if just one node
 * is slected, clears all of it's ForceIdxd paths.
 */
void WxEditorFrame::MenuActorPopupPathClearForced( wxCommandEvent& In )
{
	TArray<ANavigationPoint*> NavPts;
	GEditor->GetSelectedActors()->GetSelectedObjects<ANavigationPoint>(NavPts);
	if (NavPts.Num() == 1)
	{
		NavPts(0)->EditorForcedPaths.Empty();
		// remove any ForceIdxd Specs
		for (INT Idx = 0; Idx < NavPts(0)->PathList.Num(); Idx++)
		{
			if (NavPts(0)->PathList(Idx) != NULL &&
				NavPts(0)->PathList(Idx)->IsForced())
			{
				NavPts(0)->PathList.Remove(Idx--,1);
			}
		}
	}
	else
	{
		// clear any forced points between the selected nodes
		for (INT Idx = 0; Idx < NavPts.Num(); Idx++)
		{
			ANavigationPoint *Nav = NavPts(Idx);
			for (INT ForceIdx = 0; ForceIdx < NavPts.Num(); ForceIdx++)
			{
				if (ForceIdx != Idx)
				{
					for (INT PathIdx = 0; PathIdx < Nav->EditorForcedPaths.Num(); PathIdx++)
					{
						if (Nav->EditorForcedPaths(PathIdx).Actor == NavPts(ForceIdx))
						{
							Nav->EditorForcedPaths.Remove(PathIdx--,1);
						}
					}
					// remove any forced specs to the nav
					for (INT SpecIdx = 0; SpecIdx < NavPts(Idx)->PathList.Num(); SpecIdx++)
					{
						if (Nav->PathList(SpecIdx) != NULL &&
							Nav->PathList(SpecIdx)->End == NavPts(ForceIdx) &&
							Nav->PathList(SpecIdx)->IsForced())
						{
							Nav->PathList.Remove(SpecIdx--,1);
						}
					}
				}
			}
		}
	}
}

/**
 * Stitches selected coverlinks together, placing all the slots into a single 
 * CoverLink actor.
 */
void WxEditorFrame::MenuActorPopupPathStitchCover( wxCommandEvent& In )
{
	TArray<ACoverLink*> Links;
	GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(Links);
	ACoverLink *DestLink = NULL;
	for (INT Idx = 0; Idx < Links.Num(); Idx++)
	{
		ACoverLink *Link = Links(Idx);
		if (Link == NULL)
		{
			continue;
		}
		// pick the first link as the dest link
		if (DestLink == NULL)
		{
			DestLink = Link;
		}
		else
		{
			// add all of the slots to the destlink
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				FVector SlotLocation = Link->GetSlotLocation(SlotIdx);
				FRotator SlotRotation = Link->GetSlotRotation(SlotIdx);
				DestLink->AddCoverSlot(SlotLocation,SlotRotation,Link->Slots(SlotIdx));
			}
			GWorld->DestroyActor(Link);
		}
	}
	// update the dest link
	if (DestLink != NULL)
	{
		for (INT SlotIdx = 0; SlotIdx < DestLink->Slots.Num(); SlotIdx++)
		{
			// update the slot info
			DestLink->AutoAdjustSlot(SlotIdx,FALSE);
			DestLink->AutoAdjustSlot(SlotIdx,TRUE);
			DestLink->BuildSlotInfo(SlotIdx);
		}
		DestLink->ForceUpdateComponents(FALSE,FALSE);
	}
	GUnrealEd->RedrawAllViewports();
}

void WxEditorFrame::MenuActorPopupLinkCrowdDestinations( wxCommandEvent& In )
{
	TArray<AGameCrowdDestination*> Pts;
	GEditor->GetSelectedActors()->GetSelectedObjects<AGameCrowdDestination>(Pts);
	for (INT Idx = 0; Idx < Pts.Num(); Idx++)
	{
		AGameCrowdDestination *Pt = Pts(Idx);
		for (INT InnerIdx = 0; InnerIdx < Pts.Num(); InnerIdx++)
		{
			if (InnerIdx != Idx)
			{
				Pt->NextDestinations.AddUniqueItem(Pts(InnerIdx));
			}
		}
		UGameDestinationConnRenderingComponent *Comp = NULL;
		if (Pt->Components.FindItemByClass<UGameDestinationConnRenderingComponent>(&Comp))
		{
			FComponentReattachContext Context(Comp);
		}
	}
}

void WxEditorFrame::MenuActorPopupUnlinkCrowdDestinations( wxCommandEvent& In )
{
	TArray<AGameCrowdDestination*> Pts;
	GEditor->GetSelectedActors()->GetSelectedObjects<AGameCrowdDestination>(Pts);
	for (INT Idx = 0; Idx < Pts.Num(); Idx++)
	{
		AGameCrowdDestination *Pt = Pts(Idx);
		for (INT InnerIdx = 0; InnerIdx < Pts.Num(); InnerIdx++)
		{
			if (InnerIdx != Idx)
			{
				Pt->NextDestinations.RemoveItem(Pts(InnerIdx));
			}
		}
		UGameDestinationConnRenderingComponent *Comp = NULL;
		if (Pt->Components.FindItemByClass<UGameDestinationConnRenderingComponent>(&Comp))
		{
			FComponentReattachContext Context(Comp);
		}
	}
}

void WxEditorFrame::MenuSplineBreakAll( wxCommandEvent& In )
{
	GEditor->SplineBreakAll();
}

void WxEditorFrame::MenuSplineConnect( wxCommandEvent& In )
{
	GEditor->SplineConnect();
}

void WxEditorFrame::MenuSplineBreak( wxCommandEvent& In )
{
	GEditor->SplineBreak();
}

void WxEditorFrame::MenuSplineReverseAllDirections( wxCommandEvent& In )
{
	GEditor->SplineReverseAllDirections();
}

void WxEditorFrame::MenuSplineStraightTangents( wxCommandEvent& In )
{
	GEditor->SplineStraightTangents();
}

void WxEditorFrame::MenuSplineSelectAllNodes( wxCommandEvent& In )
{
	GEditor->SplineSelectAllNodes();
}

void WxEditorFrame::MenuSplineTestRoute( wxCommandEvent& In )
{
	GEditor->SplineTestRoute();
}


/**
* For use with the templated sort. Sorts by class name, ascending
*/
namespace WxEditorFrameCompareFunctions
{
	struct FClassNameCompare
	{
		static INT Compare(UClass* A, UClass* B)
		{
			return appStricmp(*B->GetName(),*A->GetName());
		}
	};
}

/**
* Puts all of the AVolume classes into the passed in array and sorts them by class name.
*
* @param	VolumeClasses		Array to populate with AVolume classes.
*/
void WxEditorFrame::GetSortedVolumeClasses( TArray< UClass* >* VolumeClasses )
{
	// Add all of the volume classes to the passed in array and then sort it
	for( TObjectIterator<UClass> It ; It ; ++It )
	{
		if( It->IsChildOf(AVolume::StaticClass()) )
		{
			if ( !(It->ClassFlags & CLASS_Deprecated)
				&& !(It->ClassFlags & CLASS_Abstract)
				&& (It->ClassFlags & CLASS_Placeable) )
			{
				VolumeClasses->AddUniqueItem( *It );
			}
		}
	}

	Sort<UClass*, WxEditorFrameCompareFunctions::FClassNameCompare>( &(*VolumeClasses)(0), VolumeClasses->Num() );
}

void WxEditorFrame::OnAddVolumeClass( wxCommandEvent& In )
{
	const INT VolumeID = In.GetId() - IDM_VolumeClasses_START;

	UClass* Class = NULL;

	TArray< UClass* > VolumeClasses;

	GApp->EditorFrame->GetSortedVolumeClasses( &VolumeClasses );

	check ( VolumeID < VolumeClasses.Num() );

	Class = VolumeClasses( VolumeID );

	if( Class )
	{
		GUnrealEd->Exec( *FString::Printf( TEXT("BRUSH ADDVOLUME CLASS=%s"), *Class->GetName() ) );
	}

	// A new volume actor was added, update the volumes visibility.
	// This volume should be hidden if the user doesnt have this type of volume visible.
	GUnrealEd->UpdateVolumeActorVisibility( Class );
}

/** Retrieves the material belonging to the selected actor given a index into the actor's displayable components and an index into the component's materials. */
static UMaterialInterface* GetMaterialInterface(AActor* InActor, INT ComponentIndex, INT ElementIndex)
{
	UMaterialInterface* FoundMaterialInterface = NULL;
	if(InActor)
	{
		INT NumDisplayableComponents = 0;
		for (INT ComponentIdx = 0; ComponentIdx < InActor->Components.Num(); ComponentIdx++)
		{
			UActorComponent* CurrentComponent = InActor->Components(ComponentIdx);

			UFogVolumeDensityComponent* FogVolumeComponent = Cast<UFogVolumeDensityComponent>(CurrentComponent);
			UFluidSurfaceComponent* FluidSurfaceComponent = Cast<UFluidSurfaceComponent>(CurrentComponent);
			UMeshComponent* MeshComponent = Cast<UMeshComponent>(CurrentComponent);
			UParticleSystemComponent* ParticleComponent = Cast<UParticleSystemComponent>(CurrentComponent);
			USpeedTreeComponent* SpeedTreeComponent = Cast<USpeedTreeComponent>(CurrentComponent);

			// Search through the selected actors displayable components
			// @Note - this criteria must match the criteria used to create the Materials sub menu
			if (CurrentComponent &&
				(FogVolumeComponent
				|| FluidSurfaceComponent
				|| MeshComponent && MeshComponent->GetNumElements() > 0
				|| ParticleComponent && ParticleComponent->Template
				|| SpeedTreeComponent && SpeedTreeComponent->SpeedTree))
			{
				NumDisplayableComponents++;
				if (NumDisplayableComponents - 1 == ComponentIndex)
				{
					if (FogVolumeComponent)
					{
						FoundMaterialInterface = FogVolumeComponent->FogMaterial;
					}
					else if (FluidSurfaceComponent)
					{
						FoundMaterialInterface = FluidSurfaceComponent->FluidMaterial;
					}
					else if (MeshComponent)
					{
						check(MeshComponent->GetNumElements() > 0);
						FoundMaterialInterface = MeshComponent->GetMaterial(ElementIndex);
					}
					else if (ParticleComponent)
					{
						check(ParticleComponent->Template);
						INT ValidMaterialIndex = 0;
						for (INT EmitterIndex=0; EmitterIndex < ParticleComponent->Template->Emitters.Num(); EmitterIndex++)
						{
							const UParticleEmitter* CurrentEmitter = ParticleComponent->Template->Emitters(EmitterIndex);
							if (CurrentEmitter && CurrentEmitter->LODLevels.Num() > 0 && CurrentEmitter->LODLevels(0)->RequiredModule)
							{
								if (ElementIndex == ValidMaterialIndex)
								{
									FoundMaterialInterface = CurrentEmitter->LODLevels(0)->RequiredModule->Material;
									break;
								}
								ValidMaterialIndex++;
							}
						}
					}
					else if (SpeedTreeComponent)
					{
						check(SpeedTreeComponent->SpeedTree);
						// ElementIndex 0 corresponds to STMT_MinMinusOne + 1, which is the first speedtree material
						FoundMaterialInterface = SpeedTreeComponent->GetMaterial((BYTE)ElementIndex + 1);
					}
					break;
				}
			}
		}
	}
	return FoundMaterialInterface;
}

static UMaterialInterface* GetSelectedMaterialInterface(INT ComponentIndex, INT ElementIndex)
{
	// Find the material and mesh the user chose.
	USelection* SelectedActors = GEditor->GetSelectedActors();
	UMaterialInterface* FoundMaterialInterface = NULL;

	if (SelectedActors->Num() == 1)
	{
		AActor* FirstActor = SelectedActors->GetTop<AActor>();
		FoundMaterialInterface = GetMaterialInterface( FirstActor, ComponentIndex, ElementIndex );
	}
	return FoundMaterialInterface;
}

/** Extracts the component and element index from a given Id from the Materials sub menu */
UBOOL GetComponentAndElementIndex(INT EventId, INT StartIdRange, INT EndIdRange, INT& ComponentIndex, INT& ElementIndex)
{
	if (EventId >= StartIdRange && EventId < EndIdRange)
	{
		const INT IdOffset = EventId - StartIdRange;
		// IdOffset is ComponentIndex * MATERIAL_MENU_NUM_MATERIAL_ENTRIES + MaterialIdx;
		ElementIndex = IdOffset % MATERIAL_MENU_NUM_MATERIAL_ENTRIES;
		ComponentIndex = IdOffset / MATERIAL_MENU_NUM_MATERIAL_ENTRIES;
		return TRUE;
	}
	return FALSE;
}

/**
 * Synchronizes an actor with the content browser.
 */
void WxEditorFrame::SyncToContentBrowser()
{
	TArray<UObject*> Objects;

	// If the user has any BSP surfaces selected, sync to the materials on them.
	UBOOL bFoundSurfaceMaterial = FALSE;

	for ( TSelectedSurfaceIterator<> It ; It ; ++It )
	{
		FBspSurf* Surf = *It;
		UMaterialInterface* Material = Surf->Material;
		if( Material )
		{
			Objects.AddUniqueItem( Material );
			bFoundSurfaceMaterial = TRUE;
			//break;
		}
	}

	// Otherwise, assemble a list of resources from selected actors.
	if( !bFoundSurfaceMaterial )
	{
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor						= static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			AStaticMeshActor* StaticMesh		= Cast<AStaticMeshActor>( Actor );
			ADynamicSMActor* DynamicSM			= Cast<ADynamicSMActor>( Actor );
			ASkeletalMeshActor* SkeletalMesh	= Cast<ASkeletalMeshActor>( Actor );
			AEmitter* Emitter					= Cast<AEmitter>( Actor );
			ADecalActorBase* Decal				= Cast<ADecalActorBase>( Actor );
			APrefabInstance* PrefabInstance		= Cast<APrefabInstance>( Actor );
			AKAsset* KAsset						= Cast<AKAsset>( Actor );
			AFracturedStaticMeshActor* FrActor	= Cast<AFracturedStaticMeshActor>(Actor);
			AFluidSurfaceActor* FluidActor		= Cast<AFluidSurfaceActor>(Actor);
			AFogVolumeDensityInfo* FogActor		= Cast<AFogVolumeDensityInfo>(Actor);
			ASpeedTreeActor* SpeedTreeActor		= Cast<ASpeedTreeActor>(Actor);
			ALensFlareSource* LensFlare			= Cast<ALensFlareSource>(Actor);
			AProcBuilding* ProcBuilding			= Cast<AProcBuilding>(Actor);
			ASplineLoftActor* SplineLoftActor	= Cast<ASplineLoftActor>(Actor);
			AImageReflection* ImageReflection	= Cast<AImageReflection>(Actor);
			AAmbientSound* AmbientSound			= Cast<AAmbientSound>(Actor);
#if WITH_APEX_DESTRUCTIBLE
			AApexDestructibleActor* ApexDestructibleActor = Cast<AApexDestructibleActor>(Actor);
#endif
			UObject *Archetype = Actor->GetArchetype();

			UObject* CurObject					= NULL;
			if( StaticMesh )
			{
				CurObject = StaticMesh->StaticMeshComponent->StaticMesh;
			}
			else if( DynamicSM )
			{
				CurObject = DynamicSM->StaticMeshComponent->StaticMesh;
			}
			else if( SkeletalMesh )
			{
				CurObject = SkeletalMesh->SkeletalMeshComponent->SkeletalMesh;
			}
			else if( Emitter )
			{
				CurObject = Emitter->ParticleSystemComponent->Template;
			}
			else if( Decal )
			{
				CurObject = Decal->Decal->GetDecalMaterial();
			}
			else if( PrefabInstance )
			{
				CurObject = PrefabInstance->TemplatePrefab;
			}
			else if( KAsset )
			{
				CurObject = KAsset->SkeletalMeshComponent->PhysicsAsset;
			}
			else if(FrActor)
			{
				CurObject = FrActor->FracturedStaticMeshComponent->StaticMesh;
			}
			else if( FluidActor )
			{
				CurObject = FluidActor->FluidComponent->GetMaterial();
			}
			else if( FogActor )
			{
				CurObject = FogActor->DensityComponent->GetMaterial();
			}
			else if ( SpeedTreeActor != NULL )
			{
				if ( SpeedTreeActor->SpeedTreeComponent != NULL )
				{
					CurObject = SpeedTreeActor->SpeedTreeComponent->SpeedTree;
				}
			}
			else if( LensFlare != NULL )
			{
				if( LensFlare->LensFlareComp != NULL )
				{
					if( LensFlare->LensFlareComp->Template != NULL )
					{
						CurObject = LensFlare->LensFlareComp->Template;
					}
				}
			}
			else if(ProcBuilding)
			{
				// This building might not have a ruleset, it might be getting it from its base, so use that instead
				if(!ProcBuilding->Ruleset)
				{
					ProcBuilding = ProcBuilding->GetBaseMostBuilding();
				}

				CurObject = ProcBuilding->Ruleset;
			}
			else if(SplineLoftActor)
			{
				CurObject = SplineLoftActor->DeformMesh;
			}
			else if(ImageReflection)
			{
				CurObject = ImageReflection->ImageReflectionComponent->ReflectionTexture;
			}
			else if(AmbientSound)
			{
				// Multiple objects may be here, so add to the array directly rather than set CurObject
				if ( AmbientSound->AudioComponent )
				{
					if ( AmbientSound->AudioComponent->SoundCue )
					{
						Objects.AddUniqueItem( AmbientSound->AudioComponent->SoundCue );
					}
					for ( INT i = 0; i < AmbientSound->AudioComponent->InstanceParameters.Num(); i++ )
					{
						const FAudioComponentParam& InstanceParameter = AmbientSound->AudioComponent->InstanceParameters( i );
						if ( InstanceParameter.WaveParam )
						{
							Objects.AddUniqueItem( InstanceParameter.WaveParam );
						}
					}
				}
				AAmbientSoundSimple* AmbientSoundSimple	= Cast<AAmbientSoundSimple>(AmbientSound);
				if ( AmbientSoundSimple && AmbientSoundSimple->AmbientProperties )
				{
					for ( INT i = 0; i < AmbientSoundSimple->AmbientProperties->SoundSlots.Num(); i++ )
					{
						const FAmbientSoundSlot& SoundSlot = AmbientSoundSimple->AmbientProperties->SoundSlots( i );
						if ( SoundSlot.Wave )
						{
							Objects.AddUniqueItem( SoundSlot.Wave );
						}
					}
				}
			}
#if WITH_APEX_DESTRUCTIBLE
			else if(ApexDestructibleActor)
			{
				CurObject = ApexDestructibleActor->StaticDestructibleComponent->Asset;
			}
#endif
			
			// >> Add new types here! <<
			// Finally, see if there is an archetype we can sync to
			else if( Archetype != NULL )
			{
				CurObject = Archetype;
			}


			// If an object was found, add it to the list.
			if ( CurObject )
			{
				Objects.AddItem( CurObject );
			}
		}
	}

	// Sync the content browser to the object list.
	SyncBrowserToObjects(Objects);
}

/**
 * Synchronizes a material with the content browser.
 */
void WxEditorFrame::SyncMaterialToGenericBrowser( INT ComponentIdx, INT MaterialIdx, UBOOL bBase )
{
	TArray<UObject*> ObjectsToSyncTo;
	for ( USelection::TObjectIterator Itor = GEditor->GetSelectedActors()->ObjectItor() ; Itor ; ++Itor )
	{
		AActor* Actor = Cast<AActor>( *Itor );
		if ( Actor )
		{
			UMaterialInterface* MaterialInterface = GetMaterialInterface( Actor, ComponentIdx, MaterialIdx );
			if ( MaterialInterface )
			{
				ObjectsToSyncTo.AddItem( ( bBase ? MaterialInterface->GetMaterial() : MaterialInterface ) );
			}
		}
	}
	SyncBrowserToObjects(ObjectsToSyncTo);
}

/**
 * Synchronizes a texture with the content browser.
 */
void WxEditorFrame::SyncTextureToGenericBrowser( INT ComponentIdx, INT MaterialIdx, INT TextureIdx )
{
	for ( USelection::TObjectIterator Itor = GEditor->GetSelectedActors()->ObjectItor() ; Itor ; ++Itor )
	{
		AActor* Actor = Cast<AActor>( *Itor );
		if ( Actor )
		{
			UMaterialInterface* MaterialInterface = GetMaterialInterface( Actor, ComponentIdx, MaterialIdx );
			if ( MaterialInterface )
			{
				TArray<UTexture*> Textures;
				MaterialInterface->GetUsedTextures( Textures );
				if ( TextureIdx < Textures.Num() )
				{
					UTexture* Texture = Textures( TextureIdx );
					if( Texture )
					{
						GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_SyncAssetView | CBR_FocusBrowser, Texture ) );
					}
				}
			}
		}
	}
}

/**
 * Synchronizes the content browser's selected objects to the collection specified.
 *
 * @param	ObjectSet	the list of objects to sync to
 */
void WxEditorFrame::SyncBrowserToObjects( TArray<UObject*>& ObjectSet )
{
#if WITH_MANAGED_CODE
	// Display the objects in the content browser
	if( FContentBrowser::IsInitialized() && ObjectSet.Num() > 0 )
	{
		FContentBrowser &CBInstance = FContentBrowser::GetActiveInstance();
		// Request to focus the active ContentBrowser
		GCallbackEvent->Send( FCallbackEventParameters (NULL, CALLBACK_RefreshContentBrowser, CBR_FocusBrowser) );
		CBInstance.SyncToObjects(ObjectSet);
	}
#endif
}

void WxEditorFrame::MenuSyncContentBrowser( wxCommandEvent& In )
{
	SyncToContentBrowser();
}

void WxEditorFrame::MenuSyncMaterialInterface( wxCommandEvent &In )
{
	INT ComponentIdx = -1;
	INT MaterialIdx = -1;
	// Check whether we came from the 'sync to base material' menu or the 'sync to material interface' menu
	if (GetComponentAndElementIndex(In.GetId(), IDM_SYNC_GENERICBROWSER_TO_BASEMATERIAL_START, IDM_SYNC_GENERICBROWSER_TO_BASEMATERIAL_END, ComponentIdx, MaterialIdx))
	{
		SyncMaterialToGenericBrowser(ComponentIdx, MaterialIdx, TRUE);
	}
	else if (GetComponentAndElementIndex(In.GetId(), IDM_SYNC_GENERICBROWSER_TO_MATERIALINTERFACE_START, IDM_SYNC_GENERICBROWSER_TO_MATERIALINTERFACE_END, ComponentIdx, MaterialIdx))
	{
		SyncMaterialToGenericBrowser(ComponentIdx, MaterialIdx, FALSE);
	}
}

/**
 * Called when a texture is selected from the materials submenu
 */
void WxEditorFrame::MenuSyncTexture( wxCommandEvent& In )
{
	INT Offset = In.GetId() - IDM_SYNC_TO_TEXTURE_START;

	// Get the TextureIndex and material index
	const INT TextureIndex = Offset % MATERIAL_MENU_NUM_MATERIAL_ENTRIES;
	const INT MaterialIndex = Offset / MATERIAL_MENU_NUM_MATERIAL_ENTRIES;

	// Compute the component index
	Offset /= (TextureIndex + MATERIAL_MENU_NUM_MATERIAL_ENTRIES);
	const INT ComponentIndex = Offset / MATERIAL_MENU_NUM_MATERIAL_ENTRIES;

	SyncTextureToGenericBrowser( ComponentIndex, MaterialIndex, TextureIndex );
}

/**
 * Called from the materials submenu to copy the full material name to the clipboard
 */
void WxEditorFrame::MenuCopyMaterialName( wxCommandEvent& In )
{
	INT Offset = In.GetId() - IDM_COPY_MATERIAL_NAME_TO_CLIPBOARD_START;

	// Get the TextureIndex and material index
	const INT TextureIndex = Offset % MATERIAL_MENU_NUM_MATERIAL_ENTRIES;
	const INT MaterialIndex = Offset / MATERIAL_MENU_NUM_MATERIAL_ENTRIES;

	// Compute the component index
	Offset /= (TextureIndex + MATERIAL_MENU_NUM_MATERIAL_ENTRIES);
	const INT ComponentIndex = Offset / MATERIAL_MENU_NUM_MATERIAL_ENTRIES;

	// Get the selected material's texture specified by the texture index we computed
	UMaterialInterface* SelectedMaterialInterface = GetSelectedMaterialInterface(ComponentIndex, MaterialIndex);
	if( SelectedMaterialInterface )
	{
		const FString FullName = SelectedMaterialInterface->GetFullName();
		TArray<FString> NameParts;
		FullName.ParseIntoArray(&NameParts, TEXT(" "), TRUE);
		check(NameParts.Num() == 2);
		const FString ParsedName = NameParts(0) + "\'" + NameParts(1) + "\'";
		appClipboardCopy(*ParsedName);
	}
}

void WxEditorFrame::MenuEditMaterialInterface( wxCommandEvent &In )
{
	INT ComponentIdx = -1;
	INT MaterialIdx = -1;
	verify(GetComponentAndElementIndex(In.GetId(), IDM_EDIT_MATERIALINTERFACE_START, IDM_EDIT_MATERIALINTERFACE_END, ComponentIdx, MaterialIdx));

	// Find the material the user chose.
	UMaterialInterface* SelectedMaterialInterface = GetSelectedMaterialInterface(ComponentIdx, MaterialIdx);
	if (SelectedMaterialInterface != NULL)
	{
		UBOOL bWasEditorOpened = FALSE;
#if WITH_MANAGED_CODE
		if( FContentBrowser::IsInitialized() )
		{
			GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_ActivateObject, SelectedMaterialInterface ) );
			bWasEditorOpened = TRUE;
		}
#endif
	}
}

/** Sets the given material on the the given actor **/
static void SetMaterial(AActor* Actor, UMaterialInterface* NewMaterial, INT ComponentIndex, INT ElementIndex)
{
	check(Actor);
	check(NewMaterial);

	INT NumDisplayableComponents = 0;
	for (INT ComponentIdx = 0; ComponentIdx < Actor->Components.Num(); ComponentIdx++)
	{
		UActorComponent* CurrentComponent = Actor->Components(ComponentIdx);

		UFogVolumeDensityComponent* FogVolumeComponent = Cast<UFogVolumeDensityComponent>(CurrentComponent);
		UFluidSurfaceComponent* FluidSurfaceComponent = Cast<UFluidSurfaceComponent>(CurrentComponent);
		UMeshComponent* MeshComponent = Cast<UMeshComponent>(CurrentComponent);
		UParticleSystemComponent* ParticleComponent = Cast<UParticleSystemComponent>(CurrentComponent);
		USpeedTreeComponent* SpeedTreeComponent = Cast<USpeedTreeComponent>(CurrentComponent);
		
		// Search through the selected actors displayable components
		// @Note - this criteria must match the criteria used to create the Materials sub menu
		if (CurrentComponent &&
			(FogVolumeComponent
			|| FluidSurfaceComponent
			|| MeshComponent && MeshComponent->GetNumElements() > 0
			|| ParticleComponent && ParticleComponent->Template
			|| SpeedTreeComponent && SpeedTreeComponent->SpeedTree))
		{
			NumDisplayableComponents++;
			if (NumDisplayableComponents - 1 == ComponentIndex)
			{
				if (FogVolumeComponent)
				{
					FogVolumeComponent->Modify();
					FogVolumeComponent->FogMaterial = NewMaterial;
					Actor->ForceUpdateComponents(FALSE,FALSE);
				}
				else if (FluidSurfaceComponent)
				{
					FluidSurfaceComponent->Modify();
					FluidSurfaceComponent->FluidMaterial = NewMaterial;
					Actor->ForceUpdateComponents(FALSE,FALSE);
				}
				else if (MeshComponent)
				{
					check(MeshComponent->GetNumElements() > 0);
					MeshComponent->Modify();
					MeshComponent->SetMaterial(ElementIndex, NewMaterial);
				}
				else if (ParticleComponent)
				{
					// The menu items that allow assigning materials to particle components should not have been created, so if we get here it is due to a code error
					appErrorf(TEXT("Can't assign a material to a particle component as there is no per-component material override."));
				}
				else if (SpeedTreeComponent)
				{
					check(SpeedTreeComponent->SpeedTree);
					// ElementIndex 0 corresponds to STMT_MinMinusOne + 1, which is the first speedtree material
					SpeedTreeComponent->Modify();
					SpeedTreeComponent->SetMaterial((BYTE)ElementIndex + 1, NewMaterial);
				}
				break;
			}
		}
	}
}

/** Callback for creating or editing a material instance. */
void WxEditorFrame::MenuCreateMaterialInstance( wxCommandEvent &In )
{
	INT ComponentIdx = -1;
	INT MaterialIdx = -1;
	UBOOL bCreateMaterialInstanceTimeVarying = FALSE;
	// Check whether this event handler was triggered by the user creating a MITV or a MIC
	if (GetComponentAndElementIndex(In.GetId(), IDM_CREATE_MATERIAL_INSTANCE_TIME_VARYING_START, IDM_CREATE_MATERIAL_INSTANCE_TIME_VARYING_END, ComponentIdx, MaterialIdx))
	{
		bCreateMaterialInstanceTimeVarying = TRUE;
	}
	else 
	{
		verify(GetComponentAndElementIndex(In.GetId(), IDM_CREATE_MATERIAL_INSTANCE_CONSTANT_START, IDM_CREATE_MATERIAL_INSTANCE_CONSTANT_END, ComponentIdx, MaterialIdx));
	}

	UMaterialInterface* SelectedMaterialInterface = GetSelectedMaterialInterface(ComponentIdx, MaterialIdx);

	USelection* SelectedActors = GEditor->GetSelectedActors();
	AActor* FirstActor = SelectedActors->GetTop<AActor>();

	if (SelectedActors->Num() == 1 && SelectedMaterialInterface)
	{
		AActor* FirstActor = SelectedActors->GetTop<AActor>();
		UPackage* LevelPackage = FirstActor->GetOutermost();
		UMaterialInstance* InstanceToEdit = NULL;

		// Ask the user for a name and then check to see if its taken already.
		FString DefaultName = SelectedMaterialInterface->GetName() + TEXT("_INST");
		wxTextEntryDialog TextDlg(this, *LocalizeUnrealEd("EnterMaterialInstanceName"), *LocalizeUnrealEd("PleaseEnterValue"), *DefaultName);
		if (TextDlg.ShowModal() == wxID_OK)
		{
			wxString ObjectName = TextDlg.GetValue();
			UObject* ExistingObject = FindObject<UObject>(LevelPackage, ObjectName.c_str());

			if (ExistingObject == NULL)
			{	
				if (bCreateMaterialInstanceTimeVarying)
				{
					InstanceToEdit = ConstructObject<UMaterialInstanceTimeVarying>(UMaterialInstanceTimeVarying::StaticClass(), LevelPackage, ObjectName.c_str(), RF_Transactional);
				}
				else
				{
					InstanceToEdit = ConstructObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass(), LevelPackage, ObjectName.c_str(), RF_Transactional);
				}

				if(InstanceToEdit)
				{
					InstanceToEdit->SetParent(SelectedMaterialInterface);
					SetMaterial(FirstActor, InstanceToEdit, ComponentIdx, MaterialIdx);
				}
			}
			else
			{
				appMsgf( AMT_OK, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("MaterialInstanceNameTaken_F"), ObjectName.c_str())) );
			}
		}

		// Show the material instance editor if we have an instance
		if (InstanceToEdit != NULL)
		{
			wxFrame* MaterialInstanceEditor = NULL;
			if (bCreateMaterialInstanceTimeVarying)
			{
				MaterialInstanceEditor = new WxMaterialInstanceTimeVaryingEditor( (wxWindow*)GApp->EditorFrame,-1, InstanceToEdit );
			}
			else
			{
				MaterialInstanceEditor = new WxMaterialInstanceConstantEditor( (wxWindow*)GApp->EditorFrame,-1, InstanceToEdit );
			}
			MaterialInstanceEditor->Show();
		}
	}
}

/** Assigns the currently selected content browser material to the selected material slot. */
void WxEditorFrame::MenuAssignMaterial( wxCommandEvent &In )
{
	INT ComponentIdx = -1;
	INT MaterialIdx = -1;
	verify(GetComponentAndElementIndex(In.GetId(), IDM_ASSIGN_MATERIALINTERFACE_START, IDM_ASSIGN_MATERIALINTERFACE_END, ComponentIdx, MaterialIdx));

	const FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT("MenuAssignMaterialToActor") ) );

	// Find the material and mesh the user chose.
	USelection* SelectedActors = GEditor->GetSelectedActors();
	for ( USelection::TObjectIterator It = GEditor->GetSelectedActors()->ObjectItor() ; It ; ++It )
	//if (SelectedActors->Num() == 1)
	{
		AActor* SelectedActor = Cast<AActor>( *It );//->GetTop<AActor>();
		if(SelectedActor)
		{
			GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);

			USelection* MaterialSelection = GEditor->GetSelectedObjects();
			if (MaterialSelection )//&& MaterialSelection->Num() == 1)
			{
				UMaterialInterface* InstanceToAssign = MaterialSelection->GetTop<UMaterialInterface>();
				if( InstanceToAssign )
				{
					SetMaterial(SelectedActor, InstanceToAssign, ComponentIdx, MaterialIdx);
					SelectedActor->Modify();
				}
			}
		}
	}
}


/*
 * Assign a material selected in the Content Browser to multiple actors
 * selected in the editor. This applies to all components, including if
 * a mesh component has multiple elements.
 *
 * @param	In	The windows command id related to this action
 */
void WxEditorFrame::MenuAssignMaterialToMultipleActors( wxCommandEvent &In )
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT("MenuAssignMaterialToMultipleActors") ) );
	// Find the material and mesh the user chose.
	USelection* SelectedActors = GEditor->GetSelectedActors();
	for ( USelection::TObjectIterator It = GEditor->GetSelectedActors()->ObjectItor() ; It ; ++It )
	{
		AActor* SelectedActor = Cast<AActor>( *It );
		if( SelectedActor )
		{
			GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded );

			USelection* MaterialSelection = GEditor->GetSelectedObjects();
			if ( MaterialSelection )
			{
				UMaterialInterface* InstanceToAssign = MaterialSelection->GetTop<UMaterialInterface>();
				if( InstanceToAssign )
				{
					// For every component of each actor, assign our selected material.
					for ( INT ComponentIdx = 0; ComponentIdx < SelectedActor->Components.Num(); ComponentIdx++ )
					{
						UActorComponent* CurrentComponent = SelectedActor->Components( ComponentIdx );

						UFogVolumeDensityComponent* FogVolumeComponent = Cast<UFogVolumeDensityComponent>( CurrentComponent );
						UFluidSurfaceComponent* FluidSurfaceComponent = Cast<UFluidSurfaceComponent>( CurrentComponent );
						UMeshComponent* MeshComponent = Cast<UMeshComponent>( CurrentComponent );
						UParticleSystemComponent* ParticleComponent = Cast<UParticleSystemComponent>( CurrentComponent );
						USpeedTreeComponent* SpeedTreeComponent = Cast<USpeedTreeComponent>( CurrentComponent );

						if ( FogVolumeComponent )
						{
							FogVolumeComponent->Modify();
							FogVolumeComponent->FogMaterial = InstanceToAssign;
							SelectedActor->ForceUpdateComponents( FALSE, FALSE );
						}
						else if ( FluidSurfaceComponent )
						{
							FluidSurfaceComponent->Modify();
							FluidSurfaceComponent->FluidMaterial = InstanceToAssign;
							SelectedActor->ForceUpdateComponents( FALSE, FALSE );
						}
						else if ( MeshComponent )
						{
							check( MeshComponent->GetNumElements() > 0 );
							MeshComponent->Modify();
							// Assign material to all elements of the mesh component
							for( INT ElementIdx = 0; ElementIdx < MeshComponent->GetNumElements(); ElementIdx++ )
							{
								MeshComponent->SetMaterial( ElementIdx, InstanceToAssign );
							}
						}
						else if ( ParticleComponent )
						{
							// The menu items that allow assigning materials to particle components should not have been created, so if we get here it is due to a code error
							appErrorf( TEXT( "Can't assign a material to a particle component as there is no per-component material override." ) );
						}
						else if ( SpeedTreeComponent )
						{
							check( SpeedTreeComponent->SpeedTree );
							// ElementIndex 0 corresponds to STMT_MinMinusOne + 1, which is the first speedtree material
							SpeedTreeComponent->Modify();
							SpeedTreeComponent->SetMaterial( 0, InstanceToAssign );
						}
						CurrentComponent->Modify();
					}
				}
			}
		}
	}
}

void WxEditorFrame::MenuActorPivotMoveHere( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("PIVOT HERE") );
}

void WxEditorFrame::MenuActorPivotMoveHereSnapped( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("PIVOT SNAPPED") );
}

void WxEditorFrame::MenuActorPivotMoveCenterOfSelection( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("PIVOT CENTERSELECTION") );
}

void WxEditorFrame::MenuEmitterAutoPopulate(wxCommandEvent& In)
{
	// Iterate over selected Actors.
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor		= static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		AEmitter* Emitter	= Cast<AEmitter>( Actor );
		if (Emitter)
		{
			Emitter->AutoPopulateInstanceProperties();
		}
	}
}

void WxEditorFrame::MenuEmitterReset(wxCommandEvent& In)
{
	// Iterate over selected Actors.
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor		= static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		AEmitter* Emitter	= Cast<AEmitter>( Actor );
		if (Emitter)
		{
			UParticleSystemComponent* PSysComp = Emitter->ParticleSystemComponent;
			if (PSysComp)
			{
				PSysComp->ResetParticles();
				PSysComp->ActivateSystem();
			}
		}
	}
}

void WxEditorFrame::ApplyParamSwatchToBuilding( wxCommandEvent& In )
{
	// Get index of menu choice
	INT ChoiceIndex = (In.GetId() - IDMENU_ChoosePBSwatch_START);
	check(ChoiceIndex >= 0);

	// If 'no swatch' (choice 0), don't need to look at the ruleset
	if(ChoiceIndex == 0)
	{
		GUnrealEd->ApplyParamSwatchToSelectedBuildings(NAME_None);
	}
	else
	{
		// index of swatch is menu index -1
		INT SwatchIndex = (ChoiceIndex - 1);

		// Get ruleset applied to edited building
		UProcBuildingRuleset* Ruleset = GUnrealEd->GetSelectedBuildingRuleset();
		if(Ruleset)
		{
			// check index is in range of variations 
			if(SwatchIndex < Ruleset->ParamSwatches.Num())
			{
				// get variation name for this index and assign
				FName SwatchName = Ruleset->ParamSwatches(SwatchIndex).SwatchName;
				GUnrealEd->ApplyParamSwatchToSelectedBuildings(SwatchName);
			}
		}
	}
}


void WxEditorFrame::ApplyRulesetVariationToFace(wxCommandEvent& In)
{
	// Get index of menu choice
	INT ChoiceIndex = (In.GetId() - IDMENU_ApplyRulesetVariationToFace_START);
	check(ChoiceIndex >= 0);

	// If 'default' (choice 0), don't need to look at the ruleset
	if(ChoiceIndex == 0)
	{
		GUnrealEd->ApplyRulesetVariationToSelectedFaces(NAME_None);
	}
	// non-default
	else
	{
		// index of variation is menu index -1
		INT VarIndex = (ChoiceIndex - 1);

		// Get ruleset applied to edited building
		UProcBuildingRuleset* Ruleset = GUnrealEd->GetGeomEditedBuildingRuleset();
		if(Ruleset)
		{
			// check index is in range of variations 
			if(VarIndex < Ruleset->Variations.Num())
			{
				// get variation name for this index and assign
				FName VariationName = Ruleset->Variations(VarIndex).VariationName;
				GUnrealEd->ApplyRulesetVariationToSelectedFaces(VariationName);
			}
		}
	}
}

void WxEditorFrame::ApplySelectedMaterialToPBFace(wxCommandEvent& In)
{
	UMaterialInterface* Material = GEditor->GetSelectedObjects()->GetTop<UMaterialInterface>();
	if(Material)
	{
		GUnrealEd->ApplyMaterialToPBFaces(Material);
	}
}

void WxEditorFrame::ClearFaceRulesetVariations( wxCommandEvent& In )
{
	GUnrealEd->ClearRulesetVariationFaceAssignments();	
}

void WxEditorFrame::ClearPBFaceMaterials( wxCommandEvent& In )
{
	GUnrealEd->ClearPBMaterialFaceAssignments();	
}

void WxEditorFrame::ProcBuildingResourceInfo( wxCommandEvent& In )
{
	GUnrealEd->ShowPBResourceInfo();
}

void WxEditorFrame::SelectBaseBuilding( wxCommandEvent& In )
{
	GUnrealEd->SelectBasePB();
}

void WxEditorFrame::GroupSelectedBuildings( wxCommandEvent& In )
{
	GUnrealEd->GroupSelectedPB();
}

void WxEditorFrame::UpdateImageReflectionSceneCapture(wxCommandEvent& In)
{
	TArray<AImageReflectionSceneCapture*> SelectedReflections;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		AImageReflectionSceneCapture* Reflection = Cast<AImageReflectionSceneCapture>(*It);
		if (Reflection)
		{
			SelectedReflections.AddItem(Reflection);
		}
	}

	GUnrealEd->RegenerateImageReflectionTextures(SelectedReflections);
}

void WxEditorFrame::CreateArchetype(wxCommandEvent& In)
{
	// Create an archetype from the selected actor(s).
	GUnrealEd->edactArchetypeSelected();
}

void WxEditorFrame::UpdateArchetype(wxCommandEvent& In)
{
	GUnrealEd->edactUpdateArchetypeSelected();
}

void WxEditorFrame::CreatePrefab(wxCommandEvent& In)
{
	GUnrealEd->edactPrefabSelected();
}

void WxEditorFrame::AddPrefab(wxCommandEvent& In)
{
	GUnrealEd->edactAddPrefab();
}

void WxEditorFrame::SelectPrefabActors(wxCommandEvent& In)
{
	GUnrealEd->edactSelectPrefabActors();
}

void WxEditorFrame::UpdatePrefabFromInstance(wxCommandEvent& In)
{
	GUnrealEd->edactUpdatePrefabFromInstance();
}

void WxEditorFrame::ResetInstanceFromPrefab(wxCommandEvent& In)
{
	GUnrealEd->edactResetInstanceFromPrefab();
}

void WxEditorFrame::PrefabInstanceToNormalActors(wxCommandEvent& In)
{
	GUnrealEd->edactPrefabInstanceToNormalActors();
}

void WxEditorFrame::PrefabInstanceOpenSequence(wxCommandEvent& In)
{
	APrefabInstance* PrefInst = GEditor->GetSelectedActors()->GetTop<APrefabInstance>();
	if(PrefInst && PrefInst->SequenceInstance)
	{
		WxKismet::OpenSequenceInKismet( PrefInst->SequenceInstance, GApp->EditorFrame );
	}
}
void WxEditorFrame::Group(wxCommandEvent& In)
{
	GUnrealEd->edactGroupFromSelected();
}
void WxEditorFrame::Regroup(wxCommandEvent& In)
{
	GUnrealEd->edactRegroupFromSelected();
}
void WxEditorFrame::Ungroup(wxCommandEvent& In)
{
	GUnrealEd->edactUngroupFromSelected();
}
void WxEditorFrame::LockGroup(wxCommandEvent& In)
{
	GUnrealEd->edactLockSelectedGroups();
}
void WxEditorFrame::UnlockGroup(wxCommandEvent& In)
{
	GUnrealEd->edactUnlockSelectedGroups();
}
void WxEditorFrame::AddToGroup(wxCommandEvent& In)
{
	GUnrealEd->edactAddToGroup();
}
void WxEditorFrame::RemoveFromGroup(wxCommandEvent& In)
{
	GUnrealEd->edactRemoveFromGroup();
}
void WxEditorFrame::ReportStatsForGroups(wxCommandEvent& In)
{
	GUnrealEd->edactReportStatsForSelectedGroups();
}
void WxEditorFrame::ReportStatsForSelection(wxCommandEvent& In)
{
	GUnrealEd->edactReportStatsForSelection();
}


/**
 * Launches game (or PIE) for the specific console index
 *
 * @param	WhichConsole		Console index to use or INDEX_NONE to Play in Editor
 * @param	bPlayInViewport		True to launch PIE in the currently active viewport window
 * @param	bUseMobilePreview	True to enable mobile preview mode (PC platform only)
 */
void WxEditorFrame::PlayFromHere(INT WhichConsole, const UBOOL bPlayInViewport, const UBOOL bUseMobilePreview)
{
	UCylinderComponent*	DefaultCollisionComponent = CastChecked<UCylinderComponent>(APlayerStart::StaticClass()->GetDefaultActor()->CollisionComponent);
	FVector				CollisionExtent = FVector(DefaultCollisionComponent->CollisionRadius,DefaultCollisionComponent->CollisionRadius,DefaultCollisionComponent->CollisionHeight),
						StartLocation = GEditor->ClickLocation + GEditor->ClickPlane * (FBoxPushOut(GEditor->ClickPlane,CollisionExtent) + 0.1f);

	FRotator*			StartRotation = NULL;

	// Figure out which viewport index we are
	INT MyViewportIndex = -1;
	for( INT CurViewportIndex = 0; CurViewportIndex < ViewportConfigData->GetViewportCount(); ++CurViewportIndex )
	{
		FVCD_Viewport& CurViewport = ViewportConfigData->AccessViewport( CurViewportIndex );
		if( CurViewport.bEnabled && CurViewport.ViewportWindow == GCurrentLevelEditingViewportClient && CurViewport.ViewportType == LVT_Perspective )
		{
			// grab a pointer to the rotation
			StartRotation = &CurViewport.ViewportWindow->ViewRotation;

			if( bPlayInViewport && WhichConsole == INDEX_NONE )
			{
				MyViewportIndex = CurViewportIndex;
			}
			break;
		}
	}

	// kick off the play from here request
	GUnrealEd->PlayMap(&StartLocation, StartRotation, WhichConsole, MyViewportIndex, bUseMobilePreview);
}

/**
 * Override for handling custom windows messages from a wxWidget window.  We need to handle custom balloon notification messages.
 */
WXLRESULT WxEditorFrame::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
	if( lParam == NIN_BALLOONUSERCLICK )
	{
		if( nMsg == ID_CHECKOUT_BALLOON_ID )
		{
			// User clicked on a checkout balloon, and should now receive the 
			// modal checkout dialog
			GUnrealEd->PromptToCheckoutModifiedPackages();
			// Delete the notification has it has been taken care of
			FShowBalloonNotification::DeleteNotification();
		}
		return 1;
	}
	else if( lParam == NIN_BALLOONTIMEOUT )
	{
		// Delete the notification has it has timed out
		FShowBalloonNotification::DeleteNotification();
		return 1;
	}
	else
	{
		// Must forward all other messages to the parent frame
		return wxFrame::MSWWindowProc(nMsg, wParam, lParam );
	}
}


void WxEditorFrame::MenuPlayFromHereInEditor( wxCommandEvent& In )
{
	// start a play from here, in the local editor
	if (In.GetId()  == IDM_BackDropPopupForcePlayFromHereInEditor)
		GUnrealEd->bForcePlayFromHere =TRUE;
	else
		GUnrealEd->bForcePlayFromHere =FALSE;
	PlayFromHere();
}

void WxEditorFrame::MenuPlayFromHereInEditorViewport( wxCommandEvent& In )
{
	// start a play from here, in the local editor
	PlayFromHere( INDEX_NONE, TRUE );
}

void WxEditorFrame::MenuPlayFromHereOnConsole( wxCommandEvent& In )
{
	// start a play from here, using the console menu index as which console to run it on
	PlayFromHere(In.GetId() - IDM_BackDropPopupPlayFromHereConsole_START);
}

void WxEditorFrame::MenuPlayFromHereUsingMobilePreview( wxCommandEvent& In )
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
		// start a play from here, using the PC mobile previewer
		const UBOOL bPlayInViewport = FALSE;
		const UBOOL bUseMobilePreview = TRUE;
		PlayFromHere(PCSupportContainerIndex, bPlayInViewport, bUseMobilePreview);
	}
}

void WxEditorFrame::PushViewStartStop(wxCommandEvent& In)
{
	// @todo: Currently this feature is disabled in the editor UI

	// Enable/disable the view pushing flag.
	GEditor->bIsPushingView = In.IsChecked() ? TRUE : FALSE;

	// Update the toolbar.
	MainToolBar->SetPushViewState( GEditor->bIsPushingView );

	// Exec the update.
	GEngine->Exec( GEditor->bIsPushingView ? TEXT("REMOTE PUSHVIEW START") : TEXT("REMOTE PUSHVIEW STOP"), *GLog );
}

void WxEditorFrame::PushViewSync(wxCommandEvent& In)
{
	// @todo: Currently this feature is disabled in the editor UI
	GEngine->Exec(TEXT("REMOTE PUSHVIEW SYNC"), *GLog);
}

void WxEditorFrame::MenuUseActorFactory( wxCommandEvent& In )
{
	INT ActorFactoryIndex = In.GetId() - IDMENU_ActorFactory_Start;
	check( ActorFactoryIndex >= 0 && ActorFactoryIndex < GEditor->ActorFactories.Num() );

	UActorFactory* ActorFactory = GEditor->ActorFactories(ActorFactoryIndex);
	
	// Determine if surface orientation should be used (currently only needed for decals)
	UBOOL bShouldUseSurfaceOrientation = ( ActorFactory->NewActorClass && ActorFactory->NewActorClass->IsChildOf( ADecalActorBase::StaticClass() ) );
	GEditor->UseActorFactory( ActorFactory, FALSE, bShouldUseSurfaceOrientation );
}

void WxEditorFrame::MenuUseActorFactoryAdv( wxCommandEvent& In )
{
	INT ActorFactoryIndex = In.GetId() - IDMENU_ActorFactoryAdv_Start;
	check( ActorFactoryIndex >= 0 && ActorFactoryIndex < GEditor->ActorFactories.Num() );

	UActorFactory* ActorFactory = GEditor->ActorFactories(ActorFactoryIndex);

	// Have a first stab at filling in the factory properties.
	ActorFactory->AutoFillFields( GEditor->GetSelectedObjects() );

	// Display actor factory dialog.
	WxDlgActorFactory* ActorFactoryWindow = GApp->GetDlgActorFactory();
	check(ActorFactoryWindow);
	ActorFactoryWindow->ShowDialog( ActorFactory );
}

void WxEditorFrame::MenuReplaceWithActorFactory(wxCommandEvent& In)
{
	INT ActorFactoryIndex = In.GetId() - IDMENU_ReplaceWithActorFactory_Start;
	if (GEditor->ActorFactories.IsValidIndex(ActorFactoryIndex))
	{
		GEditor->ReplaceSelectedActors(GEditor->ActorFactories(ActorFactoryIndex), NULL);
	}
	else
	{
		debugf(NAME_Error, TEXT("Invalid actor factory index in MenuReplaceWithActorFactory()!"));
	}
}

void WxEditorFrame::MenuReplaceWithActorFactoryAdv(wxCommandEvent& In)
{
	INT ActorFactoryIndex = In.GetId() - IDMENU_ReplaceWithActorFactoryAdv_Start;
	if (GEditor->ActorFactories.IsValidIndex(ActorFactoryIndex))
	{
		GEditor->ActorFactories(ActorFactoryIndex)->AutoFillFields(GEditor->GetSelectedObjects());
		WxDlgActorFactory* ActorFactoryWindow = GApp->GetDlgActorFactory();
		check(ActorFactoryWindow);
		ActorFactoryWindow->ShowDialog(GEditor->ActorFactories(ActorFactoryIndex), TRUE);
	}
	else
	{
		debugf(NAME_Error, TEXT("Invalid actor factory index in MenuReplaceWithActorFactoryAdv()!"));
	}
}

/**
 * Event handler for "Load Selected Asset" context menu option.
 */
void WxEditorFrame::LoadSelectedAssetForActorFactory( wxCommandEvent& Event )
{
#if WITH_MANAGED_CODE
	// we should only get here if content browser is enabled - generic browser doesn't show unloaded assets
	FCallbackQueryParameters Parms(NULL, CALLBACK_QuerySelectedAssets);
	if ( FContentBrowser::IsInitialized() && GCallbackQuery->Query(Parms) && Parms.ResultString.Len() > 0 )
	{
		TArray<FSelectedAssetInfo> SelectedAssets;
		FContentBrowser::UnmarshalAssetItems(Parms.ResultString, SelectedAssets);

		for ( INT Idx = 0; Idx < SelectedAssets.Num(); Idx++ )
		{
			FSelectedAssetInfo& AssetInfo = SelectedAssets(Idx);
			if ( !AssetInfo.IsValid(TRUE) )
			{
				AssetInfo.Object = UObject::StaticFindObject(AssetInfo.ObjectClass, ANY_PACKAGE, *AssetInfo.ObjectPathName);
				if ( AssetInfo.Object == NULL )
				{
					// if Obj is NULL, assume it means this is because we haven't loaded this object yet, so do that now
					AssetInfo.Object = UObject::StaticLoadObject(AssetInfo.ObjectClass, NULL, *AssetInfo.ObjectPathName, NULL, LOAD_NoWarn|LOAD_Quiet, NULL, FALSE);
				}
			}

			// make sure it is added to the selection set
			if ( AssetInfo.IsValid(TRUE) )
			{
				GEditor->GetSelectedObjects()->Select(AssetInfo.Object);

				FCallbackEventParameters Parms(NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI, AssetInfo.Object );
				GCallbackEvent->Send(Parms);
			}
		}

		// now that the assets have been loaded, pop-up a new copy of the actor factory menu which
		// will now include the select asset's name in the menu item strings
		if ( SelectedAssets.Num() > 0 )
		{
			wxMenu* CreateActorMenu = NULL;
			wxMenu* ReplaceActorMenu = NULL;
			WxMainContextMenuBase::CreateActorFactoryMenus(SelectedAssets, &CreateActorMenu, &ReplaceActorMenu, NULL );

			wxMenu* NewContextMenu = CreateActorMenu;
			if ( ReplaceActorMenu != NULL )
			{
				NewContextMenu = new wxMenu();
				NewContextMenu->Append( IDMENU_SurfPopupAddActorMenu, *LocalizeUnrealEd("AddActor"), CreateActorMenu );
				NewContextMenu->Append( IDMENU_SurfPopupReplaceActorMenu, *LocalizeUnrealEd("ReplaceWithActorFactory"), ReplaceActorMenu );
			}

			// if an actor was selected in the viewport, we'll need to show the "Replace Actor" menu as well, so
			// create a new top-level menu to hold the two submenus
			FTrackPopupMenu tpm( this, NewContextMenu );
			tpm.Show();
		}
	}
#endif
}

void WxEditorFrame::MenuReplaceSkelMeshActors(  wxCommandEvent& In )
{
	TArray<ASkeletalMeshActor*> NewSMActors;

	for( FActorIterator It; It; ++It )
	{
		ASkeletalMeshActor* SMActor = Cast<ASkeletalMeshActor>( *It );
		if( SMActor && !NewSMActors.ContainsItem(SMActor) )
		{
			USkeletalMesh* SkelMesh = SMActor->SkeletalMeshComponent->SkeletalMesh;
			FVector Location = SMActor->Location;
			FRotator Rotation = SMActor->Rotation;

			UAnimSet* AnimSet = NULL; 
			if(SMActor->SkeletalMeshComponent->AnimSets.Num() > 0)
			{
				AnimSet = SMActor->SkeletalMeshComponent->AnimSets(0);
			}

			// Find any objects in Kismet that reference this SkeletalMeshActor
			TArray<USequenceObject*> SeqVars;
			if(GWorld->GetGameSequence())
			{
				GWorld->GetGameSequence()->FindSeqObjectsByObjectName(SMActor->GetFName(), SeqVars);
			}

			GWorld->DestroyActor(SMActor);
			SMActor = NULL;

			ASkeletalMeshActor* NewSMActor = CastChecked<ASkeletalMeshActor>( GWorld->SpawnActor( ASkeletalMeshActor::StaticClass(), NAME_None, Location, Rotation, NULL ) );

			// Set up the SkeletalMeshComponent based on the old one.
			NewSMActor->ClearComponents();

			NewSMActor->SkeletalMeshComponent->SkeletalMesh = SkelMesh;

			if(AnimSet)
			{
				NewSMActor->SkeletalMeshComponent->AnimSets.AddItem( AnimSet );
			}

			NewSMActor->ConditionalUpdateComponents();

			// Set Kismet Object Vars to new SMActor
			for(INT j=0; j<SeqVars.Num(); j++)
			{
				USeqVar_Object* ObjVar = Cast<USeqVar_Object>( SeqVars(j) );
				if(ObjVar)
				{
					ObjVar->ObjValue = NewSMActor;
				}
			}
			

			// Remeber this SkeletalMeshActor so we don't try and destroy it.
			NewSMActors.AddItem(NewSMActor);
		}
	}
}

void WxEditorFrame::MenuRegenAllProcBuildings(wxCommandEvent& In)
{
	GUnrealEd->Exec(TEXT("REGENALLPROCBUILDINGS"));
}

void WxEditorFrame::MenuRegenSelProcBuildings(wxCommandEvent& In)
{
	GUnrealEd->Exec(TEXT("REGENSELPROCBUILDINGS"));
}

void WxEditorFrame::MenuGenAllProcBuildingLODTex( wxCommandEvent& In )
{
	GWarn->BeginSlowTask( TEXT("Regenerating All ProcBuilding render-to-textures"), TRUE);

	// Find set of selected buildings
	TArray<AProcBuilding*> AllBuildings;
	for (FActorIterator It; It; ++It)
	{
		AProcBuilding* Building = Cast<AProcBuilding>( *It );
		if(Building && !Building->IsPendingKill())
		{
			AllBuildings.AddUniqueItem(Building->GetBaseMostBuilding());
		}
	}

	// If we found some, regen textures
	if(AllBuildings.Num() > 0)
	{
		GUnrealEd->RegenerateProcBuildingTextures(TRUE, AllBuildings);
	}

	GWarn->EndSlowTask();
}

void WxEditorFrame::MenuGenSelProcBuildingLODTex( wxCommandEvent& In )
{
	GWarn->BeginSlowTask( TEXT("Regenerating Selected ProcBuilding render-to-textures"), TRUE);

	// Find set of selected buildings
	TArray<AProcBuilding*> SelectedBuildings;
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AProcBuilding* Building = Cast<AProcBuilding>( *It );
		if(Building)
		{
			SelectedBuildings.AddUniqueItem(Building->GetBaseMostBuilding());
		}
	}

	// If we found some, regen textures
	if(SelectedBuildings.Num() > 0)
	{
		GUnrealEd->RegenerateProcBuildingTextures(TRUE, SelectedBuildings);
	}

	GWarn->EndSlowTask();
}


void WxEditorFrame::MenuLockReadOnlyLevels(wxCommandEvent& In)
{
	LockReadOnlyLevels( !GEngine->bLockReadOnlyLevels );
}

/**
 * Handles event when the user selects
 * "Set File Listeners" from the tools menu
 *
 * @param In - command event details (not used)
 *
 */
void WxEditorFrame::MenuSetFileListeners( wxCommandEvent& In )
{
	if ( !DlgFolderList )
	{
		DlgFolderList = new WxDlgFolderList( this, -1 );	// Destroyed when WxDlgFolderList window closes
		DlgFolderList->Show();
	}
	else
	{
		DlgFolderList->Close();
		DlgFolderList = NULL;
	}
}

void WxEditorFrame::MenuJournalUpdate(wxCommandEvent& In)
{
	GConfig->SetBool(TEXT("GameAssetDatabase"), TEXT("ForceJournalUpdate"), TRUE, GEditorUserSettingsIni);
}

void WxEditorFrame::MenuCleanBSPMaterials(wxCommandEvent& In)
{
	GUnrealEd->Exec( TEXT("CLEANBSPMATERIALS") );
}

/**
 * Handles the deferred docking event notification. Updates the browser panes
 * based upon the event passed in
 */
void WxEditorFrame::OnDockingChange(WxDockEvent& Event)
{
	switch (Event.GetDockingChangeType())
	{
		case DCT_Docking:
		{
			GUnrealEd->GetBrowserManager()->DockBrowserWindow(Event.GetDockID());
			break;
		}
		case DCT_Floating:
		{
			GUnrealEd->GetBrowserManager()->UndockBrowserWindow(Event.GetDockID());
			break;
		}
		case DCT_Clone:
		{
			GUnrealEd->GetBrowserManager()->CloneBrowser(Event.GetDockID());
			break;
		}
		case DCT_Remove:
		{
			GUnrealEd->GetBrowserManager()->RemoveBrowser(Event.GetDockID());
			break;
		}
	}
}


/**
* @return	A pointer to the scale grid menu.
*/
WxScaleGridMenu* WxEditorFrame::GetScaleGridMenu()
{
	return ScaleGridMenu;
}

/**
 * @return	A pointer to the rotation grid menu.
 */
WxRotationGridMenu* WxEditorFrame::GetRotationGridMenu()
{
	RotationGridMenu->BuildMenu();
	return RotationGridMenu;
}

/**
* @return	A pointer to the autosave options menu.
*/
WxAutoSaveOptionsMenu* WxEditorFrame::GetAutoSaveOptionsMenu()
{
	return AutoSaveOptionsMenu;
}

/**
 * @return	A pointer to the drag grid menu.
 */
WxDragGridMenu* WxEditorFrame::GetDragGridMenu()
{
	return DragGridMenu;
}

/**
 * @return	A pointer to the preferences sub-menu.
 */
WxPreferencesMenu* WxEditorFrame::GetPreferencesMenu()
{
	return PreferencesMenu;
}


/**
 * Adds Matinees in the specified level to the Matinee list menu
 *
 * @param	Level				The level to add
 * @param	InOutMatinees		(In/Out) List of Matinee sequences, built up as we go along
 * @param	bInOutNeedSeparator	(In/Out) True if we need to add a separator bar before the next Matinee
 * @param	CurPrefix			Prefix string for the menu items
 */
void WxEditorFrame::AddMatineesInLevelToList( ULevel* Level, TArray< USeqAct_Interp* >& InOutMatinees, UBOOL& bInOutNeedSeparator, FString CurPrefix )
{
	const UINT PrevMatineeCount = InOutMatinees.Num();

	// Only add the level if it contains any sequences.
	if ( Level && Level->GameSequences.Num() > 0 )
	{
		USequence* RootSeq = Level->GameSequences(0);
		if( RootSeq != NULL )
		{
			// Look for Matinees inside of this root sequence
			FString NewPrefix;
			if( CurPrefix.Len() > 0 )
			{
				NewPrefix = CurPrefix + TEXT( "." );
			}
			NewPrefix += RootSeq->ObjName;
			RecursivelyAddMatineesInSequenceToList( RootSeq, InOutMatinees, bInOutNeedSeparator, NewPrefix );
		}
	}

	// If we added any Matinees for this level, make sure we have a separator before we add anyt
	// from the next level
	if( PrevMatineeCount != InOutMatinees.Num() )
	{
		bInOutNeedSeparator = TRUE;
	}
}



/**
 * Recursively adds Matinees in the specified sequence to the Matinee list menu
 *
 * @param	RootSeq				Parent sequence that contains the Matinee sequences we'll be adding
 * @param	InOutMatinees		(In/Out) List of Matinee sequences, built up as we go along
 * @param	bInOutNeedSeparator	(In/Out) True if we need to add a separator bar before the next Matinee
 * @param	CurPrefix			Prefix string for the menu items
 */
void WxEditorFrame::RecursivelyAddMatineesInSequenceToList( USequence* RootSeq, TArray< USeqAct_Interp* >& InOutMatinees, UBOOL& bInOutNeedSeparator, FString CurPrefix )
{
	check( RootSeq != NULL );

	// Find all Matinees
	TArray< USequenceObject* > MatineeSequences;
	RootSeq->FindSeqObjectsByClass( USeqAct_Interp::StaticClass(), MatineeSequences, FALSE );

	// Iterate over Matinee sequences, adding to our list
	for( INT i = 0; i < MatineeSequences.Num(); ++i )
	{
		USeqAct_Interp* MatineeSeq = CastChecked<USeqAct_Interp>( MatineeSequences(i) );

		// If we've already added too many items, then don't add any more
		const INT MaxListSize = ( IDM_MainToolBar_MatineeListItem_End - IDM_MainToolBar_MatineeListItem_Start ) + 1;
		if( InOutMatinees.Num() < MaxListSize )
		{
			// Add a separator bar if we need one
			if( bInOutNeedSeparator )
			{
				// Append a separator between each streaming level in the list
				MatineeListMenu->AppendSeparator();
				bInOutNeedSeparator = FALSE;
			}

			// Add it to the list!
			FString ItemName = CurPrefix + TEXT( "." ) + MatineeSeq->GetName();

			// Append comment if we have one
			if( MatineeSeq->ObjComment.Len() > 0 )
			{
				ItemName += TEXT( " '" );
				ItemName += MatineeSeq->ObjComment;
				ItemName += TEXT( "'" );
			}
			wxMenuItem* NewItem = MatineeListMenu->Append(
				IDM_MainToolBar_MatineeListItem_Start + InOutMatinees.Num(), *ItemName );

			// Add to our list of Matinees
			InOutMatinees.AddItem( MatineeSeq );
		}
	}


	// Find all sequences. The function will always return parents before children in the array.
	TArray<USequenceObject*> SeqObjs;
	RootSeq->FindSeqObjectsByClass( USequence::StaticClass(), SeqObjs, FALSE );

	// Iterate over sequences
	for( INT i = 0; i < SeqObjs.Num(); ++i )
	{
		USequence* Seq = CastChecked<USequence>( SeqObjs(i) );

		// add the child sequences
		const FString NewPrefix = CurPrefix + TEXT( "." ) + Seq->GetName();
		RecursivelyAddMatineesInSequenceToList( Seq, InOutMatinees, bInOutNeedSeparator, NewPrefix );
	}
}



/** If there's only one Matinee available, opens it for editing, otherwise returns a menu to display */
wxMenu* WxEditorFrame::OpenMatineeOrBuildMenu()
{
	check( MatineeListMenu != NULL );

	// @todo: If we ever use this menu in a more persistent way, we should serialize the mapping table entries
	//    to prevent them from being GC'd

	// Clear the list
	while( MatineeListMenu->GetMenuItemCount() > 0 )
	{
		wxMenuItem* ItemToDelete = MatineeListMenu->FindItemByPosition( 0 );
		MatineeListMenu->Delete( ItemToDelete );
	}
	MatineeListMenuMap.Reset();


	// We can't build a list of Matinees while GWorld is a PIE world.
	if( !GIsPlayInEditorWorld )
	{
		UBOOL bNeedSeparator = FALSE;

		// Unless the current level is the persistent level, we'll add the current level's Matinee sequences first.
		// These are usually the sequences the user is most interested in.
		if( GWorld->CurrentLevel != GWorld->PersistentLevel )
		{
			// Add the current level
			FString NewPrefix = GWorld->CurrentLevel->GetOutermost()->GetName();
			AddMatineesInLevelToList( GWorld->CurrentLevel, MatineeListMenuMap, bNeedSeparator, NewPrefix );
		}
		
		// Add the persistent level
		FString CurPrefix = TEXT( "" );
		AddMatineesInLevelToList( GWorld->PersistentLevel, MatineeListMenuMap, bNeedSeparator, CurPrefix );

		// Also add any streaming levels.
		AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
		for( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
		{
			ULevelStreaming* CurStreamingLevel = WorldInfo->StreamingLevels( LevelIndex );
			if( CurStreamingLevel )
			{
				// Skip this level if we've already added it
				if( CurStreamingLevel->LoadedLevel != GWorld->CurrentLevel )
				{
					FString NewPrefix = CurStreamingLevel->LoadedLevel->GetOutermost()->GetName();
					AddMatineesInLevelToList( CurStreamingLevel->LoadedLevel, MatineeListMenuMap, bNeedSeparator, NewPrefix );
				}
			}
		}


		// OK, if we only have a single Matinee anyway, then just go ahead and open it now
		if( MatineeListMenuMap.Num() == 1 )
		{
			USeqAct_Interp* MatineeSeq = MatineeListMenuMap( 0 );

			// Open Matinee for editing!
			WxKismet::OpenMatinee( MatineeSeq );

			// We return NULL so the caller won't try to display a list of Matinees
			return NULL;
		}
	}


	// If we don't have any menu items, then return NULL so that the menu won't even be displayed
	if( MatineeListMenuMap.Num() == 0 )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd( "MainToolBar_MatineeListDropDown_NoMatineeExists" ) );
		return NULL;
	}

	// We'll let the caller display the menu, since it knows where to put it
	return MatineeListMenu;
}

/**
 * Stores of the camera data and viewport type from the previous viewport layout
 *
 * @param	PreviousViewportConfig	The previous viewport layout
 */
void WxEditorFrame::SetPreviousViewportData( FViewportConfig_Data* PreviousViewportConfig )
{
	check( PreviousViewportConfig );
	
	// Copy stored viewport data from the previous viewport layout
	ViewportConfigData->OldViewportData = PreviousViewportConfig->OldViewportData;

	// Update the stored off viewport data with the 'current' camera data from the previous viewport layout
	for( INT CurViewportIndex = 0; CurViewportIndex < 4; ++CurViewportIndex )
	{
		FVCD_Viewport& CurViewport = PreviousViewportConfig->Viewports[ CurViewportIndex ];
		if( CurViewport.bEnabled && CurViewport.ViewportWindow != NULL )
		{
			// Look for an existing entry of the same viewport type, and if found, set the camera position and rotation
			UBOOL MatchFound = FALSE;
			for( INT ViewportDataIdx = 0; ViewportDataIdx < ViewportConfigData->OldViewportData.Num(); ++ViewportDataIdx )
			{
				if( CurViewport.ViewportType == ViewportConfigData->OldViewportData(ViewportDataIdx).Type)
				{
					MatchFound = TRUE;

					ViewportConfigData->OldViewportData(ViewportDataIdx).Location = CurViewport.ViewportWindow->ViewLocation;
					ViewportConfigData->OldViewportData(ViewportDataIdx).Orientation = CurViewport.ViewportWindow->ViewRotation;

					break;
				}								
			}

			// If we've never saved off data for this viewport type, add a new entry
			if( !MatchFound )
			{
				ViewportConfigData->OldViewportData.AddItem( FViewportCameraData(CurViewport.ViewportType, 
															 CurViewport.ViewportWindow->ViewLocation, 
															 CurViewport.ViewportWindow->ViewRotation) );
			}
		}
	}
}

/** Called when the realtime audio button is clicked */
void WxEditorFrame::OnClickedRealTimeAudio( wxCommandEvent& In )
{
	// Toggle real time audio
	GEditor->AccessUserSettings().bEnableRealTimeAudio = In.IsChecked();
	// Save the settings to the config file
	GEditor->SaveUserSettings();
}

/** Called when the realtime audio button needs to be updated */
void WxEditorFrame::UI_RealTimeAudioButton( wxUpdateUIEvent& In )
{
	// the real time audio button should be "pressed" if realtime audio is enabled
	In.Check( GEditor->AccessUserSettings().bEnableRealTimeAudio );
}

/** Called when a user adjusts the volume slider */
void WxEditorFrame::OnVolumeChanged( wxScrollEvent& In )
{
	// Adjust the volume level
	GEditor->AccessUserSettings().EditorVolumeLevel = In.GetPosition() / 100.0f;
	// Save the settings to the config file
	GEditor->SaveUserSettings();
}

/**
 * Called whenever the user presses the toggle favorites button
 *
 * @param	In	Event automatically generated by wxWidgets whenever the toggle favorites button is pressed
 */
void WxEditorFrame::OnClickedToggleFavorites( wxCommandEvent& In )
{
	FMainMRUFavoritesList* MRUFavoritesList = GetMRUFavoritesList();
	check( MRUFavoritesList );

	FString MapFileName;
	const UBOOL bMapFileExists = GPackageFileCache->FindPackageFile( *( GWorld->GetOutermost()->GetName() ), NULL, MapFileName );

	// If the user clicked the toggle favorites button, the map file should exist, but double check to be safe.
	if ( bMapFileExists )
	{
		// If the map was already favorited, remove it from the favorites
		if ( MRUFavoritesList->ContainsFavoritesItem( MapFileName ) )
		{
			MRUFavoritesList->RemoveFavoritesItem( MapFileName );
		}
		// If the map was not already favorited, add it to the favorites
		else
		{
			MRUFavoritesList->AddFavoritesItem( MapFileName );
		}
	}
}

/**
 * Called whenever a user selects a favorite file from a menu to load
 *
 * @param	In	Event automatically generated by wxWidgets whenever a favorite file is selected by the user
 */
void WxEditorFrame::MenuFileFavorite( wxCommandEvent& In )
{
	FMainMRUFavoritesList* MRUFavoritesList = GetMRUFavoritesList();

	const INT FavoriteId = In.GetId() - IDM_FAVORITES_START;
	const FFilename FileName = MRUFavoritesList->GetFavoritesItem( FavoriteId );

	if( MRUFavoritesList->VerifyFavoritesFile( FavoriteId ) )
	{
		// Prompt the user to save any outstanding changes
		if( FEditorFileUtils::SaveDirtyPackages(TRUE, TRUE, FALSE) == FALSE )
		{
			// something went wrong or the user pressed cancel.  Return to the editor so the user doesn't lose their changes		
			return;
		}

		// Load the requested level.
		FEditorFileUtils::LoadMap( FileName );
	}
}
