/*=============================================================================
	EditorBuildUtils.cpp: Utilities for building in the editor
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EditorBuildUtils.h"
#include "SourceControl.h"
#include "LevelUtils.h"
#include "FileHelpers.h"
#include "DlgMapCheck.h"
#include "BusyCursor.h"
#include "Database.h"
#include "UnPath.h"
#include "DlgBuildProgress.h"
#include "LevelBrowser.h"
#include "LightingBuildOptions.h"
#include "PropertyWindow.h"
#include "Dialogs.h"

IMPLEMENT_CLASS(ULightmassOptionsObject);

/**
 * Dialog window for setting lighting build options. Values are read from the ini and stored again if the user
 * presses OK.
 */
class WxDlgLightingBuildOptions : public wxDialog
{
public:
	/**
	 * Default constructor, initializing dialog.
	 */
	WxDlgLightingBuildOptions();

	/** Advanced settings button */
	wxButton* Advanced;
	/** Checkbox for BSP setting */
	wxCheckBox*	BuildBSPCheckBox;
	/** Checkbox for static mesh setting */
	wxCheckBox*	BuildStaticMeshesCheckBox;
	/** Checkbox for selected actors/brushes/surfaces setting */
	wxCheckBox*	BuildOnlySelectedCheckBox;
	/** Checkbox for current level setting */
	wxCheckBox*	BuildOnlyCurrentLevelCheckBox;
	/** Checkbox for building only levels selected in the level browser. */
	wxCheckBox*	BuildOnlySelectedLevelsCheckBox;
	/** Checkbox for building only visibility. */
	wxCheckBox* BuildOnlyVisibilityCheckBox;
	/** Checkbox for building only visible levels. */
	wxCheckBox* BuildOnlyVisibleLevelsCheckBox;
	/** Checkbox for showing lighting build info after a completed build. */
	wxCheckBox* ShowLightingBuildInfoCheckBox;
#if WITH_MANAGED_CODE
	/** Checkbox for Lightmass setting */
	wxCheckBox*	UseLightmass;
	/** Checkbox for debug coloring */
	wxCheckBox*	UseErrorColoring;
	/** Checkbox for performing full quality build. */
	wxStaticText* QualityLabel;
	wxComboBox* QualityBuildComboBox;
#endif //#if WITH_MANAGED_CODE

	/** Checkbox for building LOD texture generation */
	wxCheckBox*	GenerateBuildingLODTexCheckBox;

	/** The 'advanced' options for Lightmass */
	ULightmassOptionsObject* LightmassOptions;
	/** Property window for displaying 'advanced' options. */
	WxPropertyWindowFrame* Advanced_PropWindow;

	static UBOOL VerifyLightmassPrerequisites(UBOOL bUseLightmass);

	/**
	 * Shows modal dialog populating default Options by reading from ini. The code will store the newly set options
	 * back to the ini if the user presses OK.
	 *
	 * @param	Options		Lighting rebuild options to set.
	 */
	UBOOL ShowModal( FLightingBuildOptions& Options );
private:
	using wxDialog::ShowModal;		// Hide parent implementation
public:

	/**
	 * Route OK to wxDialog.
	 */
	void OnOK( wxCommandEvent& In ) 
	{
		wxDialog::AcceptAndClose();
	}

	/**
	 *	Show the advanced options...
	 */
	void OnAdvanced(wxCommandEvent& In)
	{
		if (Advanced_PropWindow->IsShown())
		{
			Advanced_PropWindow->Hide();
		}
		else
		{
			Advanced_PropWindow->Show();
		}
	}

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(WxDlgLightingBuildOptions, wxDialog)
EVT_BUTTON( wxID_OK, WxDlgLightingBuildOptions::OnOK )
EVT_BUTTON( IDM_ADVANCED_OPTIONS, WxDlgLightingBuildOptions::OnAdvanced )
END_EVENT_TABLE()

/**
 * Default constructor, creating dialog.
 */
WxDlgLightingBuildOptions::WxDlgLightingBuildOptions()
{
	SetExtraStyle(GetExtraStyle()|wxWS_EX_BLOCK_EVENTS);
	wxDialog::Create( GApp->EditorFrame, wxID_ANY, *LocalizeUnrealEd(TEXT("LBO_LightingBuildOptions")), wxDefaultPosition, wxDefaultSize, wxCAPTION|wxDIALOG_MODAL|wxTAB_TRAVERSAL );

	wxBoxSizer* BoxSizer2 = new wxBoxSizer(wxVERTICAL);
	SetSizer(BoxSizer2);

	wxStaticBox* StaticBoxSizer3Static = new wxStaticBox(this, wxID_ANY, *LocalizeUnrealEd(TEXT("LBO_LightingBuildOptions")));
	wxStaticBoxSizer* StaticBoxSizer3 = new wxStaticBoxSizer(StaticBoxSizer3Static, wxVERTICAL);
	BoxSizer2->Add(StaticBoxSizer3, 1, wxGROW|wxALL, 5);

	BuildBSPCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd(TEXT("LBO_BuildBSP")), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	BuildBSPCheckBox->SetValue(TRUE);
	StaticBoxSizer3->Add(BuildBSPCheckBox, 0, wxGROW|wxALL, 5);

	BuildStaticMeshesCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd(TEXT("LBO_BuildStaticMeshes")), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	BuildStaticMeshesCheckBox->SetValue(TRUE);
	StaticBoxSizer3->Add(BuildStaticMeshesCheckBox, 0, wxGROW|wxALL, 5);

	BuildOnlySelectedCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd(TEXT("LBO_BuildOnlySelected")), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	BuildOnlySelectedCheckBox->SetValue(FALSE);
	StaticBoxSizer3->Add(BuildOnlySelectedCheckBox, 0, wxGROW|wxALL, 5);

	BuildOnlyCurrentLevelCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd(TEXT("LBO_BuildOnlyCurrentLevel")), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	BuildOnlyCurrentLevelCheckBox->SetValue(FALSE);
	StaticBoxSizer3->Add(BuildOnlyCurrentLevelCheckBox, 0, wxGROW|wxALL, 5);

	BuildOnlySelectedLevelsCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd(TEXT("LBO_BuildOnlySelectedLevels")), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	BuildOnlySelectedLevelsCheckBox->SetValue(FALSE);
	StaticBoxSizer3->Add(BuildOnlySelectedLevelsCheckBox, 0, wxGROW|wxALL, 5);

	BuildOnlyVisibilityCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd(TEXT("LBO_BuildOnlyVisibility")), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	BuildOnlyVisibilityCheckBox->SetValue(FALSE);
	StaticBoxSizer3->Add(BuildOnlyVisibilityCheckBox, 0, wxGROW|wxALL, 5);

	BuildOnlyVisibleLevelsCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd(TEXT("LBO_BuildOnlyVisibleLevels")), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	BuildOnlyVisibleLevelsCheckBox->SetValue(FALSE);
	StaticBoxSizer3->Add(BuildOnlyVisibleLevelsCheckBox, 0, wxGROW|wxALL, 5);

	// Checkbox for generating building LOD textures
	GenerateBuildingLODTexCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd("GenerateBuildingLODTex"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	GenerateBuildingLODTexCheckBox->SetValue(TRUE);
	StaticBoxSizer3->Add(GenerateBuildingLODTexCheckBox, 0, wxGROW|wxALL, 5);

#if WITH_MANAGED_CODE
	QualityLabel = new wxStaticText(this, -1, *LocalizeUnrealEd("LightQuality"));
	StaticBoxSizer3->Add(QualityLabel, 0, wxGROW|wxALL, 1);
	QualityBuildComboBox = new wxComboBox(this, wxID_ANY, *LocalizeUnrealEd("LightQuality"), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY );
	StaticBoxSizer3->Add( QualityBuildComboBox, 1, wxGROW|wxALL, 4 );
	QualityBuildComboBox->Freeze();
	QualityBuildComboBox->Append( *LocalizeUnrealEd("LightQuality_Preview"), (void*)NULL );
	QualityBuildComboBox->Append( *LocalizeUnrealEd("LightQuality_Medium"), (void*)NULL );
	QualityBuildComboBox->Append( *LocalizeUnrealEd("LightQuality_High"), (void*)NULL );
	QualityBuildComboBox->Append( *LocalizeUnrealEd("LightQuality_Production"), (void*)NULL );
	QualityBuildComboBox->Thaw();

	UseLightmass = new wxCheckBox( this, wxID_ANY, TEXT("Use Lightmass"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	UseLightmass->SetValue(TRUE);
	UseLightmass->SetToolTip( *Localize(TEXT("Lightmass"), TEXT("UseLightmassButton_Tooltip"), TEXT("UnrealEd") ) );
	StaticBoxSizer3->Add(UseLightmass, 0, wxGROW|wxALL, 5);

	/** Checkbox for debug coloring */
	UseErrorColoring = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd("UseErrorColoring"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	UseErrorColoring->SetValue(FALSE);
	UseErrorColoring->SetToolTip( *Localize(TEXT("Lightmass"), TEXT("UseErrorColoringButton_Tooltip"), TEXT("UnrealEd") ) );
	StaticBoxSizer3->Add(UseErrorColoring, 0, wxGROW|wxALL, 5);

#endif //#if WITH_MANAGED_CODE

	/** Checkbox for lighting build info */
	ShowLightingBuildInfoCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd("LightingBuildOptions_ShowLightingBuildInfo"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	ShowLightingBuildInfoCheckBox->SetValue(FALSE);
	StaticBoxSizer3->Add(ShowLightingBuildInfoCheckBox, 0, wxGROW|wxALL, 5);


	UBOOL bAllowAdvancedOptions = FALSE;
	verify(GConfig->GetBool(TEXT("DevOptions.DebugOptions"), TEXT("bAllowAdvancedOptions"), bAllowAdvancedOptions, GLightmassIni));
	if (bAllowAdvancedOptions)
	{
		Advanced = new wxButton(this, IDM_ADVANCED_OPTIONS, *LocalizeUnrealEd(TEXT("Advanced")), wxDefaultPosition, wxDefaultSize, 0 );
		StaticBoxSizer3->Add(Advanced, 0, wxGROW|wxALL, 5);
	}


	wxBoxSizer* BoxSizer9 = new wxBoxSizer(wxHORIZONTAL);
	BoxSizer2->Add(BoxSizer9, 0, wxGROW|wxALL, 5);

	wxButton* Button10 = new wxButton( this, wxID_OK, *LocalizeUnrealEd(TEXT("&OK")) );
	Button10->SetDefault();
	Button10->SetFocus();
	BoxSizer9->Add(Button10, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	wxButton* Button11 = new wxButton( this, wxID_CANCEL, *LocalizeUnrealEd(TEXT("&Cancel")) );
	BoxSizer9->Add(Button11, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// Fit the dialog to the window contents
	BoxSizer2->Fit( this );

	LightmassOptions = ConstructObject<ULightmassOptionsObject>(ULightmassOptionsObject::StaticClass());
	LightmassOptions->AddToRoot();

	Advanced_PropWindow = new WxPropertyWindowFrame;
	Advanced_PropWindow->Create(this, wxID_ANY);
	// Disallow closing
	Advanced_PropWindow->DisallowClose();
	Advanced_PropWindow->SetObject(LightmassOptions, EPropertyWindowFlags::ShouldShowCategories );
	if (Advanced_PropWindow)
	{
		Advanced_PropWindow->ExpandItem("DebugSettings");
		Advanced_PropWindow->ExpandItem("SwarmSettings");
	}
}

extern UBOOL GLightmassDebugMode; 
extern UBOOL GLightmassStatsMode;
extern FSwarmDebugOptions GSwarmDebugOptions;

UBOOL WxDlgLightingBuildOptions::VerifyLightmassPrerequisites(UBOOL bUseLightmass)
{
#if WITH_MANAGED_CODE
	if (bUseLightmass)
	{
		UBOOL bProceedWithBuild = TRUE;
		// Check to see if we support Lighting Builds on this machine.
		if ( GSystemSettings.bAllowFloatingPointRenderTargets == FALSE )
		{
			appMsgf( AMT_OK, *Localize(TEXT("Lightmass"), TEXT("LightmassError_SupportFP"), TEXT("UnrealEd")) );
			bProceedWithBuild = FALSE;
		}
		return bProceedWithBuild;
	}
#endif
	return TRUE;
}

/**
 * Shows modal dialog populating default Options by reading from ini. The code will store the newly set options
 * back to the ini if the user presses OK.
 *
 * @param	Options		Lighting rebuild options to set.
 * @return				TRUE if the lighting rebuild should go ahead.
 */
UBOOL WxDlgLightingBuildOptions::ShowModal( FLightingBuildOptions& Options )
{
	// Retrieve settings from the World Properties.
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	check(WorldInfo);

#if WITH_MANAGED_CODE
	Options.bUseLightmass = WorldInfo->bUseGlobalIllumination;
#else
	Options.bUseLightmass = FALSE;
#endif //#if WITH_MANAGED_CODE

	// Retrieve settings from ini.
	UBOOL bUseErrorColoring = FALSE;
	GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("OnlyBuildSelected"),		Options.bOnlyBuildSelected,			GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("OnlyBuildCurrentLevel"),	Options.bOnlyBuildCurrentLevel,		GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("BuildBSP"),				Options.bBuildBSP,					GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("BuildActors"),			Options.bBuildActors,				GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("OnlyBuildSelectedLevels"),Options.bOnlyBuildSelectedLevels,	GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("OnlyBuildVisibility"),	Options.bOnlyBuildVisibility,		GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("OnlyBuildVisibleLevels"),	Options.bOnlyBuildVisibleLevels,	GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("GenerateBuildingLODTex"),	Options.bGenerateBuildingLODTex,	GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("UseErrorColoring"),		bUseErrorColoring,					GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("ShowLightingBuildInfo"),	Options.bShowLightingBuildInfo,		GEditorUserSettingsIni );
	INT QualityLevel;
	GConfig->GetInt(  TEXT("LightingBuildOptions"), TEXT("QualityLevel"),			QualityLevel,						GEditorUserSettingsIni );
	QualityLevel = Clamp<INT>(QualityLevel, Quality_Preview, Quality_Production);
	Options.QualityLevel = (ELightingBuildQuality)QualityLevel;

	// Populate dialog with read in values.
#if WITH_MANAGED_CODE
	QualityBuildComboBox->SetSelection(Options.QualityLevel);
	UseLightmass->SetValue( Options.bUseLightmass == TRUE );
	UseErrorColoring->SetValue(bUseErrorColoring == TRUE);
#endif //#if WITH_MANAGED_CODE

	BuildOnlySelectedCheckBox->SetValue( Options.bOnlyBuildSelected == TRUE );
	BuildOnlyCurrentLevelCheckBox->SetValue( Options.bOnlyBuildCurrentLevel == TRUE );
	BuildBSPCheckBox->SetValue( Options.bBuildBSP == TRUE );
	BuildStaticMeshesCheckBox->SetValue( Options.bBuildActors == TRUE );
	BuildOnlySelectedLevelsCheckBox->SetValue( Options.bOnlyBuildSelectedLevels == TRUE );
	BuildOnlyVisibilityCheckBox->SetValue( Options.bOnlyBuildVisibility == TRUE );
	BuildOnlyVisibleLevelsCheckBox->SetValue( Options.bOnlyBuildVisibleLevels == TRUE );
	GenerateBuildingLODTexCheckBox->SetValue( Options.bGenerateBuildingLODTex == TRUE );
	ShowLightingBuildInfoCheckBox->SetValue( Options.bShowLightingBuildInfo == TRUE );

	// Copy the current settings in...
	//@lmtodo. For some reason, the global instance is not initializing to the default settings...
	GLightmassDebugOptions.Touch();
	// Since these can be set by the commandline, we need to update them here...
	GLightmassDebugOptions.bDebugMode = GLightmassDebugMode;
	GLightmassDebugOptions.bStatsEnabled = GLightmassStatsMode;
	LightmassOptions->DebugSettings = GLightmassDebugOptions;

	//@lmtodo. For some reason, the global instance is not initializing to the default settings...
	GSwarmDebugOptions.Touch();
	LightmassOptions->SwarmSettings = GSwarmDebugOptions;

	UBOOL bProceedWithBuild = TRUE;

	// Run dialog...
	if( wxDialog::ShowModal() == wxID_OK )
	{
#if WITH_MANAGED_CODE
		bProceedWithBuild = WxDlgLightingBuildOptions::VerifyLightmassPrerequisites(UseLightmass->GetValue());

		// ... and retrieve options if user pressed okay.
		INT QualityLevel = QualityBuildComboBox->GetSelection();
		Options.QualityLevel				= (ELightingBuildQuality)(Clamp<INT>(QualityLevel, Quality_Preview, Quality_Production));
		Options.bUseLightmass				= UseLightmass->GetValue();
		bUseErrorColoring					= UseErrorColoring->GetValue();
#endif //#if WITH_MANAGED_CODE

		Options.bOnlyBuildSelected			= BuildOnlySelectedCheckBox->GetValue();
		Options.bOnlyBuildCurrentLevel		= BuildOnlyCurrentLevelCheckBox->GetValue();
		Options.bBuildBSP					= BuildBSPCheckBox->GetValue();
		Options.bBuildActors				= BuildStaticMeshesCheckBox->GetValue();
		Options.bOnlyBuildSelectedLevels	= BuildOnlySelectedLevelsCheckBox->GetValue();
		Options.bOnlyBuildVisibility		= BuildOnlyVisibilityCheckBox->GetValue();
		Options.bOnlyBuildVisibleLevels		= BuildOnlyVisibleLevelsCheckBox->GetValue();
		Options.bGenerateBuildingLODTex		= GenerateBuildingLODTexCheckBox->GetValue();
		Options.bShowLightingBuildInfo		= ShowLightingBuildInfoCheckBox->GetValue();

		// If necessary, retrieve a list of levels currently selected in the level browser.
		if ( Options.bOnlyBuildSelectedLevels )
		{
			Options.SelectedLevels.Empty();

			WxLevelBrowser* LevelBrowser = GUnrealEd->GetBrowser<WxLevelBrowser>( TEXT("LevelBrowser") );
			if ( LevelBrowser )
			{
				// Assemble an ignore list from the levels that are currently selected in the level browser.
				for ( WxLevelBrowser::TSelectedLevelItemIterator It( LevelBrowser->SelectedLevelItemIterator() ) ; It ; ++It )
				{
					if( It->IsLevel() )
					{
						Options.SelectedLevels.AddItem( It->GetLevel() );
					}
				}
			}
			else
			{
				// Level browser wasn't found -- notify the user and abort the build.
				appMsgf( AMT_OK, *LocalizeUnrealEd("LBO_CantFindLevelBrowser") );
				bProceedWithBuild = FALSE;
			}
		}

		// Save options to ini if things are still looking good.
		if ( bProceedWithBuild )
		{
			if ( Options.bUseLightmass )
			{
				debugf(TEXT("LIGHTMASS: Quality level is %d"), (INT)(Options.QualityLevel));
			}
			GConfig->SetBool( TEXT("LightingBuildOptions"), TEXT("OnlyBuildSelected"),		Options.bOnlyBuildSelected,			GEditorUserSettingsIni );
			GConfig->SetBool( TEXT("LightingBuildOptions"), TEXT("OnlyBuildCurrentLevel"),	Options.bOnlyBuildCurrentLevel,		GEditorUserSettingsIni );
			GConfig->SetBool( TEXT("LightingBuildOptions"), TEXT("BuildBSP"),				Options.bBuildBSP,					GEditorUserSettingsIni );
			GConfig->SetBool( TEXT("LightingBuildOptions"), TEXT("BuildActors"),			Options.bBuildActors,				GEditorUserSettingsIni );
			GConfig->SetBool( TEXT("LightingBuildOptions"), TEXT("OnlyBuildSelectedLevels"),Options.bOnlyBuildSelectedLevels,	GEditorUserSettingsIni );
			GConfig->SetBool( TEXT("LightingBuildOptions"), TEXT("OnlyBuildVisibility"),	Options.bOnlyBuildVisibility,		GEditorUserSettingsIni );
			GConfig->SetBool( TEXT("LightingBuildOptions"), TEXT("OnlyBuildVisibleLevels"),	Options.bOnlyBuildVisibleLevels,	GEditorUserSettingsIni );
			GConfig->SetBool( TEXT("LightingBuildOptions"), TEXT("GenerateBuildingLODTex"),	Options.bGenerateBuildingLODTex,	GEditorUserSettingsIni );
			GConfig->SetBool( TEXT("LightingBuildOptions"), TEXT("ShowLightingBuildInfo"),	Options.bShowLightingBuildInfo,		GEditorUserSettingsIni );
			GConfig->SetBool( TEXT("LightingBuildOptions"), TEXT("UseErrorColoring"),		bUseErrorColoring,					GEditorUserSettingsIni );
			GConfig->SetInt(  TEXT("LightingBuildOptions"), TEXT("QualityLevel"),			Options.QualityLevel,				GEditorUserSettingsIni );
			// Update the lighting quality setting on the toolbar!
			GApp->EditorFrame->UpdateUI();

			// Copy the results back out...
			GLightmassDebugMode = LightmassOptions->DebugSettings.bDebugMode; 
			GLightmassStatsMode = LightmassOptions->DebugSettings.bStatsEnabled;
			GLightmassDebugOptions = LightmassOptions->DebugSettings;

			// Copy the results back out...
			GSwarmDebugOptions = LightmassOptions->SwarmSettings;
		}
	}
	else
	{
		bProceedWithBuild = FALSE;
	}

	LightmassOptions->RemoveFromRoot();

	return bProceedWithBuild;
}

/** Constructor */
FEditorBuildUtils::FEditorAutomatedBuildSettings::FEditorAutomatedBuildSettings()
:	BuildErrorBehavior( ABB_PromptOnError ),
	UnableToCheckoutFilesBehavior( ABB_PromptOnError ),
	NewMapBehavior( ABB_PromptOnError ),
	FailedToSaveBehavior( ABB_PromptOnError ),
	bAutoAddNewFiles( TRUE ),
	bShutdownEditorOnCompletion( FALSE ),
	SCCEventListener( NULL )
{}

/**
 * Start an automated build of all current maps in the editor. Upon successful conclusion of the build, the newly
 * built maps will be submitted to source control.
 *
 * @param	BuildSettings		Build settings used to dictate the behavior of the automated build
 * @param	OutErrorMessages	Error messages accumulated during the build process, if any
 *
 * @return	TRUE if the build/submission process executed successfully; FALSE if it did not
 */

UBOOL FEditorBuildUtils::EditorAutomatedBuildAndSubmit( const FEditorAutomatedBuildSettings& BuildSettings, FString& OutErrorMessages )
{
#if HAVE_SCC && WITH_MANAGED_CODE
	// Assume the build is successful to start
	UBOOL bBuildSuccessful = TRUE;
	
	// Keep a set of packages that should be submitted to source control at the end of a successful build. The build preparation and processing
	// will add and remove from the set depending on build settings, errors, etc.
	TSet<UPackage*> PackagesToSubmit;

	// Perform required preparations for the automated build process
	bBuildSuccessful = PrepForAutomatedBuild( BuildSettings, PackagesToSubmit, OutErrorMessages );

	// If the preparation went smoothly, attempt the actual map building process
	if ( bBuildSuccessful )
	{
		bBuildSuccessful = EditorBuild( IDM_BUILD_ALL_SUBMIT );

		// If the map build failed, log the error
		if ( !bBuildSuccessful )
		{
			LogErrorMessage( LocalizeUnrealEd("AutomatedBuild_Error_BuildFailed"), OutErrorMessages );
		}
	}

	// If any map errors resulted from the build, process them according to the behavior specified in the build settings
	if ( bBuildSuccessful && GApp->GetDlgMapCheck()->HasAnyErrors() )
	{
		bBuildSuccessful = ProcessAutomatedBuildBehavior( BuildSettings.BuildErrorBehavior, LocalizeUnrealEd("AutomatedBuild_Error_MapErrors"), OutErrorMessages );
	}

	// If it's still safe to proceed, attempt to save all of the level packages that have been marked for submission
	if ( bBuildSuccessful )
	{
		UPackage* CurOutermostPkg = GWorld->PersistentLevel->GetOutermost();
		FString PackagesThatFailedToSave;

		// Try to save the p-level if it should be submitted
		if ( PackagesToSubmit.Contains( CurOutermostPkg ) && !FEditorFileUtils::SaveLevel( GWorld->PersistentLevel ) )
		{
			// If the p-level failed to save, remove it from the set of packages to submit
			PackagesThatFailedToSave += FString::Printf( TEXT("%s\n"), *CurOutermostPkg->GetName() );
			PackagesToSubmit.RemoveKey( CurOutermostPkg );
		}
		
		// Try to save each streaming level (if they should be submitted)
		for ( TArray<ULevelStreaming*>::TIterator LevelIter( GWorld->GetWorldInfo()->StreamingLevels ); LevelIter; ++LevelIter )
		{
			ULevelStreaming* CurStreamingLevel = *LevelIter;
			if ( CurStreamingLevel && CurStreamingLevel->LoadedLevel )
			{
				CurOutermostPkg = CurStreamingLevel->LoadedLevel->GetOutermost();
				if ( PackagesToSubmit.Contains( CurOutermostPkg ) && !FEditorFileUtils::SaveLevel( CurStreamingLevel->LoadedLevel ) )
				{
					// If a save failed, remove the streaming level from the set of packages to submit
					PackagesThatFailedToSave += FString::Printf( TEXT("%s\n"), *CurOutermostPkg->GetName() );
					PackagesToSubmit.RemoveKey( CurOutermostPkg );
				}
			}
		}

		// If any packages failed to save, process the behavior specified by the build settings to see how the process should proceed
		if ( PackagesThatFailedToSave.Len() > 0 )
		{
			bBuildSuccessful = ProcessAutomatedBuildBehavior( BuildSettings.FailedToSaveBehavior,
				FString::Printf( LocalizeSecure( LocalizeUnrealEd("AutomatedBuild_Error_FilesFailedSave"), *PackagesThatFailedToSave ) ),
				OutErrorMessages );
		}
	}

	// If still safe to proceed, make sure there are actually packages remaining to submit
	if ( bBuildSuccessful )
	{
		bBuildSuccessful = PackagesToSubmit.Num() > 0;
		if ( !bBuildSuccessful )
		{
			LogErrorMessage( LocalizeUnrealEd("AutomatedBuild_Error_NoValidLevels"), OutErrorMessages );
		}
	}

	// Finally, if everything has gone smoothly, submit the requested packages to source control
	if ( bBuildSuccessful )
	{
		SubmitPackagesForAutomatedBuild( PackagesToSubmit, BuildSettings );
	}

	// Check if the user requested the editor shutdown at the conclusion of the automated build
	if ( BuildSettings.bShutdownEditorOnCompletion )
	{
		appRequestExit( FALSE );
	}

	return bBuildSuccessful;
#else
	return FALSE;
#endif
}


#define PATHBUILDOP(x) \
	if ( !GEditor->GetMapBuildCancelled() ) \
{ \
	FPathBuilder::Exec( TEXT(##x) ); \
} \

UBOOL IsBuildCancelled()
{
	return GEditor->GetMapBuildCancelled();
}

/**
 * Perform an editor build with behavior dependent upon the specified id
 *
 * @param	Id	Action Id specifying what kind of build is requested
 *
 * @return	TRUE if the build completed successfully; FALSE if it did not (or was manually canceled)
 */
UBOOL FEditorBuildUtils::EditorBuild( INT Id )
{
	GWarn->MapCheck_Clear();

	// Make sure to set this flag to FALSE before ALL builds.
	GEditor->SetMapBuildCancelled( FALSE );

	// Will be set to FALSE if, for some reason, the build does not happen.
	UBOOL bDoBuild = TRUE;
	// Indicates whether the persistent level should be dirtied at the end of a build.
	UBOOL bDirtyPersistentLevel = TRUE;

	// Stop rendering thread so we're not wasting CPU cycles.
	StopRenderingThread();

	// Hack: These don't initialize properly and if you pick BuildAll right off the
	// bat when opening a map you will get incorrect values in them.
	GLightmassDebugOptions.Touch();
	GSwarmDebugOptions.Touch();

	// Show option dialog first, before showing the DlgBuildProgress window.
	FLightingBuildOptions LightingBuildOptions;
	if ( Id == IDM_BUILD_LIGHTING )
	{
		WxDlgLightingBuildOptions LightingBuildOptionsDialog;
		bDoBuild = LightingBuildOptionsDialog.ShowModal( LightingBuildOptions );
	}

	// Show the build progress dialog.
	WxDlgBuildProgress::EBuildType BuildType = WxDlgBuildProgress::BUILDTYPE_Geometry;
	switch (Id)
	{
	case IDM_BUILD_GEOMETRY:
	case IDM_BUILD_VISIBLEGEOMETRY:
	case IDM_BUILD_ALL:
	case IDM_BUILD_ALL_ONLY_SELECTED_PATHS:
		BuildType = WxDlgBuildProgress::BUILDTYPE_Geometry;
		break;
	case IDM_BUILD_LIGHTING:
		BuildType = WxDlgBuildProgress::BUILDTYPE_Lighting;
		break;
	case IDM_BUILD_AI_PATHS:
	case IDM_BUILD_COVER:
	case IDM_BUILD_SELECTED_AI_PATHS:
		BuildType = WxDlgBuildProgress::BUILDTYPE_Paths;
		break;
	default:
		BuildType = WxDlgBuildProgress::BUILDTYPE_Unknown;
		break;
	}
	GApp->DlgBuildProgress->ShowDialog(BuildType);
	GCallbackEvent->Send( CALLBACK_EditorPreModal );
	GApp->DlgBuildProgress->MakeModal( TRUE );
	GApp->DlgBuildProgress->MarkBuildStartTime();

	const FString HiddenLevelWarning( FString::Printf(TEXT("%s"), *LocalizeUnrealEd(TEXT("HiddenLevelsContinueWithBuildQ")) ) );
	switch( Id )
	{
	case IDM_BUILD_GEOMETRY:
		{
			FScopedTaskPerfTracker PerfTracker( TEXT("Build Geometry"), *GWorld->GetOutermost()->GetName() );

			// We can't set the busy cursor for all windows, because lighting
			// needs a cursor for the lighting options dialog.
			const FScopedBusyCursor BusyCursor;

			GUnrealEd->Exec( TEXT("MAP REBUILD") );

			// No need to dirty the persient level if we're building BSP for a sub-level.
			bDirtyPersistentLevel = FALSE;
			break;
		}
	case IDM_BUILD_VISIBLEGEOMETRY:
		{
			// If any levels are hidden, prompt the user about how to proceed
			bDoBuild = GEditor->WarnAboutHiddenLevels( TRUE, *HiddenLevelWarning );
			if ( bDoBuild )
			{
				FScopedTaskPerfTracker PerfTracker( TEXT("Build Visible Geometry"), *GWorld->GetOutermost()->GetName() );

				// We can't set the busy cursor for all windows, because lighting
				// needs a cursor for the lighting options dialog.
				const FScopedBusyCursor BusyCursor;

				GUnrealEd->Exec( TEXT("MAP REBUILD ALLVISIBLE") );
			}
			break;
		}

	case IDM_BUILD_LIGHTING:
		{
			if( bDoBuild )
			{
				FScopedTaskPerfTracker PerfTracker( TEXT("Build Lighting"), *GWorld->GetOutermost()->GetName() );


				// We can't set the busy cursor for all windows, because lighting
				// needs a cursor for the lighting options dialog.
				const FScopedBusyCursor BusyCursor;
				GUnrealEd->BuildLighting( LightingBuildOptions );
			}
			break;
		}

	case IDM_BUILD_SELECTED_AI_PATHS:
		{
			bDoBuild = GEditor->WarnAboutHiddenLevels( FALSE, *HiddenLevelWarning );
			if ( bDoBuild )
			{
				FScopedTaskPerfTracker PerfTracker( TEXT("Build AI Paths"), *GWorld->GetOutermost()->GetName() );

				const UBOOL bPartialBuild = (appMsgf( AMT_YesNo, *LocalizeUnrealEd("BuildOnlySelectedPylons") ));
				// We can't set the busy cursor for all windows, because lighting
				// needs a cursor for the lighting options dialog.
				const FScopedBusyCursor BusyCursor;

				FPathBuilder::BuildPaths(GEditor->bBuildReachSpecs,bPartialBuild,&IsBuildCancelled);
			}

			break;
		}
	case IDM_BUILD_AI_PATHS:
		{
			bDoBuild = GEditor->WarnAboutHiddenLevels( FALSE, *HiddenLevelWarning );
			if ( bDoBuild )
			{
				FScopedTaskPerfTracker PerfTracker( TEXT("Build AI Paths"), *GWorld->GetOutermost()->GetName() );
				// We can't set the busy cursor for all windows, because lighting
				// needs a cursor for the lighting options dialog.
				const FScopedBusyCursor BusyCursor;

				FPathBuilder::BuildPaths(GEditor->bBuildReachSpecs,FALSE,&IsBuildCancelled);
			}

			break;
		}

	case IDM_BUILD_COVER:
		{
			bDoBuild = GEditor->WarnAboutHiddenLevels( FALSE, *HiddenLevelWarning );
			if ( bDoBuild )
			{
				FScopedTaskPerfTracker PerfTracker( TEXT("Build Cover"), *GWorld->GetOutermost()->GetName() );

				// We can't set the busy cursor for all windows, because lighting
				// needs a cursor for the lighting options dialog.
				const FScopedBusyCursor BusyCursor;

				if( FPathBuilder::NavMeshWorld() )
				{
					PATHBUILDOP("PREDEFINEPATHS");
					PATHBUILDOP("BUILDCOVER FROMDEFINEPATHS=0");
					PATHBUILDOP("POSTDEFINEPATHS");
					PATHBUILDOP("FINISHPATHBUILD");
				}
				else
				{
					PATHBUILDOP("PREDEFINEPATHS");
					PATHBUILDOP("BUILDCOVER FROMDEFINEPATHS=0");
					PATHBUILDOP("BUILDCOMBATZONES FROMDEFINEPATHS=0");
					PATHBUILDOP("POSTDEFINEPATHS");
					PATHBUILDOP("BUILDNETWORKIDS");
					PATHBUILDOP("FINISHPATHBUILD");
				}

			}

			break;
		}

	case IDM_BUILD_ALL_ONLY_SELECTED_PATHS:
	case IDM_BUILD_ALL:
	case IDM_BUILD_ALL_SUBMIT:
		{
			bDoBuild = GEditor->WarnAboutHiddenLevels( TRUE, *HiddenLevelWarning );
			if ( bDoBuild )
			{
				// We can't set the busy cursor for all windows, because lighting
				// needs a cursor for the lighting options dialog.
				const FScopedBusyCursor BusyCursor;

				{
					FScopedTaskPerfTracker PerfTracker( TEXT("Build All - Geometry"), *GWorld->GetOutermost()->GetName() );
					GUnrealEd->Exec( TEXT("MAP REBUILD ALLVISIBLE") );
				}

				// Regen all proc-buildings as part of 'build all'
				{
					FScopedTaskPerfTracker PerfTracker( TEXT("Build All - Regen ProcBuildings"), *GWorld->GetOutermost()->GetName() );
					GUnrealEd->Exec( TEXT("REGENALLPROCBUILDINGS") );
				}

				{
					GApp->DlgBuildProgress->SetBuildType(WxDlgBuildProgress::BUILDTYPE_Paths);
					FScopedTaskPerfTracker PerfTracker( TEXT("Build All - AI Paths"), *GWorld->GetOutermost()->GetName() );
					FPathBuilder::BuildPaths(GEditor->bBuildReachSpecs,(Id==IDM_BUILD_ALL_ONLY_SELECTED_PATHS),&IsBuildCancelled);
				}

				//Do a canceled check before moving on to the next step of the build.
				if( GEditor->GetMapBuildCancelled() )
				{
					break;
				}
				else
				{
					GApp->DlgBuildProgress->SetBuildType(WxDlgBuildProgress::BUILDTYPE_Lighting);
					FScopedTaskPerfTracker PerfTracker( TEXT("Build All - Lighting"), *GWorld->GetOutermost()->GetName() );
					FLightingBuildOptions LightingOptions;

					INT QualityLevel;

					// Force automated builds to always use production lighting
					if ( Id == IDM_BUILD_ALL_SUBMIT )
					{
						QualityLevel = Quality_Production;
					}
					else
					{
						GConfig->GetInt( TEXT("LightingBuildOptions"), TEXT("QualityLevel"), QualityLevel, GEditorUserSettingsIni);
						QualityLevel = Clamp<INT>(QualityLevel, Quality_Preview, Quality_Production);
					}
					LightingOptions.QualityLevel = (ELightingBuildQuality)QualityLevel;
#if WITH_MANAGED_CODE
					LightingOptions.bUseLightmass = GWorld->GetWorldInfo()->bUseGlobalIllumination;
					const UBOOL bProceedWithBuild = WxDlgLightingBuildOptions::VerifyLightmassPrerequisites(LightingOptions.bUseLightmass);
#else
					LightingOptions.bUseLightmass = FALSE;
					const UBOOL bProceedWithBuild = TRUE;
#endif //#if WITH_MANAGED_CODE
					if (bProceedWithBuild)
					{
						GUnrealEd->BuildLighting(LightingOptions);
					}
				}

				//Do a canceled check before moving on to the next step of the build.
				if( GEditor->GetMapBuildCancelled() )
				{
					break;
				}
				else
				{
					GApp->DlgBuildProgress->SetBuildType(WxDlgBuildProgress::BUILDTYPE_Fluids);
					FScopedTaskPerfTracker PerfTracker( TEXT("Build All - Fluid Surfaces"), *GWorld->GetOutermost()->GetName() );
					GUnrealEd->BuildFluidSurfaces();
				}

			}
			break;
		}

	default:
		warnf(TEXT("Invalid build Id"));
		break;
	}

	// Check map for errors (only if build operation happened)
	if ( bDoBuild && !GEditor->GetMapBuildCancelled() )
	{
		GUnrealEd->Exec( TEXT("MAP CHECK DONTCLEARMESSAGES DONTDISPLAYDIALOG") );
	}

	// Re-start the rendering thread after build operations completed.
	if (GUseThreadedRendering)
	{
		StartRenderingThread();
	}

	if ( bDoBuild )
	{
		// Display elapsed build time.
		debugf( TEXT("Build time %s"), *GApp->DlgBuildProgress->BuildElapsedTimeString() );
		GUnrealEd->IssueDecalUpdateRequest();
	}

	// Build completed, hide the build progress dialog.
	// NOTE: It's important to turn off modalness before hiding the window, otherwise a background
	//		 application may unexpectedly be promoted to the foreground, obscuring the editor.  This
	//		 is due to how MakeModal works in WxWidgets (disabling/enabling all other top-level windows.)
	GApp->DlgBuildProgress->MakeModal( FALSE );
	GCallbackEvent->Send( CALLBACK_EditorPostModal );
	GApp->DlgBuildProgress->Show( FALSE );

	GUnrealEd->RedrawLevelEditingViewports();

	if ( bDoBuild )
	{
		if ( bDirtyPersistentLevel )
		{
			GWorld->MarkPackageDirty();
		}
		GCallbackEvent->Send( CALLBACK_LevelDirtied );
	}

	// Don't show map check if we cancelled build because it may have some bogus data
	const UBOOL bBuildCompleted = bDoBuild && !GEditor->GetMapBuildCancelled();
	if( bBuildCompleted )
	{
		GWarn->LightingBuild_ShowConditionally();
		GWarn->MapCheck_ShowConditionally();
	}

	// If a different application is in the foreground or has focus, then
	// flash the editor in the task bar to inform the user of the completed build
	DWORD WindowProcessID = 0;
	HWND ForegroundWindowHandle = GetForegroundWindow();
	GetWindowThreadProcessId( ForegroundWindowHandle, &WindowProcessID );

	// Compare the foreground window's process ID vs. the editor's process ID
	if ( WindowProcessID != GetCurrentProcessId() )
	{
		GApp->EditorFrame->RequestUserAttention();
	}

	return bBuildCompleted;
}

/**
 * Private helper method to log an error both to GWarn and to the build's list of accumulated errors
 *
 * @param	InErrorMessage			Message to log to GWarn/add to list of errors
 * @param	OutAccumulatedErrors	List of errors accumulated during a build process so far
 */
void FEditorBuildUtils::LogErrorMessage( const FString& InErrorMessage, FString& OutAccumulatedErrors )
{
	OutAccumulatedErrors += FString::Printf( TEXT("%s\n"), *InErrorMessage );
	GWarn->Logf( NAME_Warning, *InErrorMessage );
}

/**
 * Helper method to handle automated build behavior in the event of an error. Depending on the specified behavior, one of three
 * results are possible:
 *	a) User is prompted on whether to proceed with the automated build or not,
 *	b) The error is regarded as a build-stopper and the method returns failure,
 *	or
 *	c) The error is acknowledged but not regarded as a build-stopper, and the method returns success.
 * In any event, the error is logged for the user's information.
 *
 * @param	InBehavior				Behavior to use to respond to the error
 * @param	InErrorMsg				Error to log
 * @param	OutAccumulatedErrors	List of errors accumulated from the build process so far; InErrorMsg will be added to the list
 *
 * @return	TRUE if the build should proceed after processing the error behavior; FALSE if it should not
 */
UBOOL FEditorBuildUtils::ProcessAutomatedBuildBehavior( EAutomatedBuildBehavior InBehavior, const FString& InErrorMsg, FString& OutAccumulatedErrors )
{
#if HAVE_SCC && WITH_MANAGED_CODE
	// Assume the behavior should result in the build being successful/proceeding to start
	UBOOL bSuccessful = TRUE;

	switch ( InBehavior )
	{
		// In the event the user should be prompted for the error, display a modal dialog describing the error and ask the user
		// if the build should proceed or not
	case ABB_PromptOnError:
		{
			WxChoiceDialog PromptDlg( InErrorMsg, LocalizeUnrealEd("AutomatedBuild_Prompt_Title"), 
				WxChoiceDialogBase::Choice( ART_Yes, LocalizeUnrealEd("AutomatedBuild_Prompt_Proceed"), WxChoiceDialogBase::DCT_DefaultAffirmative ),
				WxChoiceDialogBase::Choice( ART_No, LocalizeUnrealEd("AutomatedBuild_Prompt_Cancel"), WxChoiceDialogBase::DCT_DefaultCancel ) );
			
			PromptDlg.ShowModal();
			bSuccessful = ( PromptDlg.GetChoice().ReturnCode == ART_Yes );
		}
		break;

		// In the event that the specified error should abort the build, mark the processing as a failure
	case ABB_FailOnError:
		bSuccessful = FALSE;
		break;
	}

	// Log the error message so the user is aware of it
	LogErrorMessage( InErrorMsg, OutAccumulatedErrors );

	// If the processing resulted in the build inevitably being aborted, write to the log about the abortion
	if ( !bSuccessful )
	{
		LogErrorMessage( LocalizeUnrealEd("AutomatedBuild_Error_AutomatedBuildAborted"), OutAccumulatedErrors );
	}

	return bSuccessful;
#else
	return FALSE;
#endif
}



/**
 * Helper method designed to perform the necessary preparations required to complete an automated editor build
 *
 * @param	BuildSettings		Build settings that will be used for the editor build
 * @param	OutPkgsToSubmit		Set of packages that need to be saved and submitted after a successful build
 * @param	OutErrorMessages	Errors that resulted from the preparation (may or may not force the build to stop, depending on build settings)
 *
 * @return	TRUE if the preparation was successful and the build should continue; FALSE if the preparation failed and the build should be aborted
 */
UBOOL FEditorBuildUtils::PrepForAutomatedBuild( const FEditorAutomatedBuildSettings& BuildSettings, TSet<UPackage*>& OutPkgsToSubmit, FString& OutErrorMessages )
{
#if HAVE_SCC && WITH_MANAGED_CODE
	// Assume the preparation is successful to start
	UBOOL bBuildSuccessful = TRUE;

	OutPkgsToSubmit.Empty();

	// Source control is required for the automated build, so ensure that SCC support is compiled in and
	// that the server is enabled and available for use
	if ( !FSourceControl::IsEnabled() || !FSourceControl::IsServerAvailable() )
	{
		bBuildSuccessful = FALSE;
		LogErrorMessage( LocalizeUnrealEd("AutomatedBuild_Error_SCCError"), OutErrorMessages );
	}

	// Empty changelists aren't allowed; abort the build if one wasn't provided
	if ( bBuildSuccessful && BuildSettings.ChangeDescription.Len() == 0 )
	{
		bBuildSuccessful = FALSE;
		LogErrorMessage( LocalizeUnrealEd("AutomatedBuild_Error_NoCLDesc"), OutErrorMessages );
	}

	TArray<UPackage*> PreviouslySavedWorldPackages;
	TArray<UPackage*> PackagesToCheckout;
	TArray<ULevel*> LevelsToSave;

	if ( bBuildSuccessful )
	{
		TArray<UWorld*> AllWorlds;
		FString UnsavedWorlds;
		FLevelUtils::GetWorlds( AllWorlds, TRUE );

		UBOOL bLightingNeedsRebuild = FALSE;

		// Check all of the worlds that will be built to ensure they have been saved before and have a filename
		// associated with them. If they don't, they won't be able to be submitted to source control.
		FString CurWorldPkgFileName;
		for ( TArray<UWorld*>::TConstIterator WorldIter( AllWorlds ); WorldIter; ++WorldIter )
		{
			const UWorld* CurWorld = *WorldIter;
			check( CurWorld );

			UPackage* CurWorldPackage = CurWorld->GetOutermost();
			check( CurWorldPackage );

			if ( GPackageFileCache->FindPackageFile( *CurWorldPackage->GetName(), NULL, CurWorldPkgFileName ) )
			{
				PreviouslySavedWorldPackages.AddUniqueItem( CurWorldPackage );

				// Add all packages which have a corresponding file to the set of packages to submit for now. As preparation continues
				// any packages that can't be submitted due to some error will be removed.
				OutPkgsToSubmit.Add( CurWorldPackage );
			}
			else
			{
				UnsavedWorlds += FString::Printf( TEXT("%s\n"), *CurWorldPackage->GetName() );
			}

			// Check if lighting needs to be rebuilt with Production lighting
			ELightingBuildQuality LightingQuality = (ELightingBuildQuality)CurWorld->GetWorldInfo()->LevelLightingQuality;
			if( LightingQuality != Quality_Production || CurWorld->GetWorldInfo()->bMapNeedsLightingFullyRebuilt )
			{
				bLightingNeedsRebuild = TRUE;
			}
		}

		// If any of the worlds haven't been saved before, process the build setting's behavior to see if the build
		// should proceed or not
		if ( UnsavedWorlds.Len() > 0 )
		{
			bBuildSuccessful = ProcessAutomatedBuildBehavior( BuildSettings.NewMapBehavior, 
				FString::Printf( LocalizeSecure( LocalizeUnrealEd("AutomatedBuild_Error_UnsavedMap"), *UnsavedWorlds ) ),
				OutErrorMessages );
		}

		if ( bBuildSuccessful && !bLightingNeedsRebuild )
		{
			// TODO - make this a legit command line option, and add localized text
			bBuildSuccessful = FALSE;
			LogErrorMessage( TEXT("Map is already built with Production lighting, skipping."), OutErrorMessages );
		}
	}

	if ( bBuildSuccessful )
	{
		// Update the source control status of any relevant world packages in order to determine which need to be
		// checked out, added to the depot, etc.
		FSourceControl::UpdatePackageStatus( PreviouslySavedWorldPackages );

		FString PkgsThatCantBeCheckedOut;
		for ( TArray<UPackage*>::TConstIterator PkgIter( PreviouslySavedWorldPackages ); PkgIter; ++PkgIter )
		{
			UPackage* CurPackage = *PkgIter;
			const FString CurPkgName = CurPackage->GetName();
			const INT CurSCCState = GPackageFileCache->GetSourceControlState( *CurPkgName );

			switch ( CurSCCState )
			{
				// If the package is already checked out by someone else, not up-to-date, or in an invalid state,
				// remove it from the list of packages that should be submitted
			case SCC_DontCare:
			case SCC_Ignore:
			case SCC_NotCurrent:
			case SCC_CheckedOutOther:
				PkgsThatCantBeCheckedOut += FString::Printf( TEXT("%s\n"), *CurPkgName );
				OutPkgsToSubmit.RemoveKey( CurPackage );
				break;
				
				// If the package is read-only, mark it as a package that needs to be checked out from source control
			case SCC_ReadOnly:
				PackagesToCheckout.AddItem( CurPackage );
				break;

				// If the package isn't in the depot yet, make sure it's not read-only on the user's machine. If it is, remove
				// it from the list of packages to submit, because it won't be able to be saved.
			case SCC_NotInDepot:
				{
					FString CurFilename;
					if ( GPackageFileCache->FindPackageFile( *CurPkgName, NULL, CurFilename ) )
					{
						if ( GFileManager->IsReadOnly( *CurFilename ) )
						{
							PkgsThatCantBeCheckedOut += FString::Printf( TEXT("%s\n"), *CurPkgName );
							OutPkgsToSubmit.RemoveKey( CurPackage );
						}
					}
				}
				break;
			}
		}

		// If any of the packages can't be checked out or are read-only, process the build setting's behavior to see if the build
		// should proceed or not
		if ( PkgsThatCantBeCheckedOut.Len() > 0 )
		{
			bBuildSuccessful = ProcessAutomatedBuildBehavior( BuildSettings.UnableToCheckoutFilesBehavior,
				FString::Printf( LocalizeSecure( LocalizeUnrealEd("AutomatedBuild_Error_UnsaveableFiles"), *PkgsThatCantBeCheckedOut ) ),
				OutErrorMessages );
		}
	}

	if ( bBuildSuccessful )
	{
		TArray<FString> FilesToCheckout;
		for ( TArray<UPackage*>::TConstIterator CheckoutIter( PackagesToCheckout ); CheckoutIter; ++CheckoutIter )
		{
			const UPackage* CurPkg = *CheckoutIter;
			FilesToCheckout.AddItem( CurPkg->GetName() );
		}

		// Check out all of the packages from source control that need to be checked out
		if ( FilesToCheckout.Num() > 0 )
		{
			FSourceControl::ConvertPackageNamesToSourceControlPaths( FilesToCheckout );
			FSourceControl::CheckOut( BuildSettings.SCCEventListener, FilesToCheckout, TRUE );

			// Update the package status of the packages that were just checked out to confirm that they
			// were actually checked out correctly
			FSourceControl::UpdatePackageStatus( PackagesToCheckout );

			FString FilesThatFailedCheckout;
			for ( TArray<UPackage*>::TConstIterator CheckedOutIter( PackagesToCheckout ); CheckedOutIter; ++CheckedOutIter )
			{
				UPackage* CurPkg = *CheckedOutIter;
				const INT SCCState = GPackageFileCache->GetSourceControlState( *CurPkg->GetName() );

				// If any of the packages failed to check out, remove them from the set of packages to submit
				if ( SCCState != SCC_CheckedOut && SCCState != SCC_NotInDepot )
				{
					FilesThatFailedCheckout += FString::Printf( TEXT("%s\n"), *CurPkg->GetName() );
					OutPkgsToSubmit.RemoveKey( CurPkg );
				}
			}

			// If any of the packages failed to check out correctly, process the build setting's behavior to see if the build
			// should proceed or not
			if ( FilesThatFailedCheckout.Len() > 0 )
			{
				bBuildSuccessful = ProcessAutomatedBuildBehavior( BuildSettings.UnableToCheckoutFilesBehavior,
					FString::Printf( LocalizeSecure( LocalizeUnrealEd("AutomatedBuild_Error_FilesFailedCheckout"), *FilesThatFailedCheckout ) ),
					OutErrorMessages );
			}
		}
	}

	// Verify there are still actually any packages left to submit. If there aren't, abort the build and warn the user of the situation.
	if ( bBuildSuccessful )
	{
		bBuildSuccessful = OutPkgsToSubmit.Num() > 0;
		if ( !bBuildSuccessful )
		{
			LogErrorMessage( LocalizeUnrealEd("AutomatedBuild_Error_NoValidLevels"), OutErrorMessages );
		}
	}

	// If the build is safe to commence, force all of the levels visible to make sure the build operates correctly
	if ( bBuildSuccessful )
	{
		UBOOL bVisibilityToggled = FALSE;
		if ( !FLevelUtils::IsLevelVisible( GWorld->PersistentLevel ) )
		{
			FLevelUtils::SetLevelVisibility( NULL, GWorld->PersistentLevel, TRUE, FALSE );
			bVisibilityToggled = TRUE;
		}
		for ( TArray<ULevelStreaming*>::TConstIterator LevelIter( GWorld->GetWorldInfo()->StreamingLevels ); LevelIter; ++LevelIter )
		{
			ULevelStreaming* CurStreamingLevel = *LevelIter;
			if ( CurStreamingLevel && !FLevelUtils::IsLevelVisible( CurStreamingLevel ) )
			{
				CurStreamingLevel->bShouldBeVisibleInEditor = TRUE;
				bVisibilityToggled = TRUE;
			}
		}
		if ( bVisibilityToggled )
		{
			GWorld->UpdateLevelStreaming();
		}
	}

	return bBuildSuccessful;
#else
	LogErrorMessage( LocalizeUnrealEd("AutomatedBuild_Error_SCCSupportNotCompiledIn"), OutErrorMessages );
	return FALSE;
#endif
}


/**
 * Helper method to submit packages to source control outside of the automated build process
 *
 * @param	InPkgsToSubmit		Set of packages which should be submitted to source control
 * @param	ChangeDescription	Description (already localized) to be attached to the check in.
 */
void FEditorBuildUtils::SaveAndCheckInPackages( const TSet<UPackage*>& InPkgsToSubmit, const FString ChangeDescription )
{
	FEditorBuildUtils::FEditorAutomatedBuildSettings MaterialBuildSettings;
	MaterialBuildSettings.ChangeDescription = ChangeDescription;
	MaterialBuildSettings.BuildErrorBehavior = ABB_PromptOnError;
	MaterialBuildSettings.UnableToCheckoutFilesBehavior = ABB_PromptOnError;
	MaterialBuildSettings.FailedToSaveBehavior = ABB_PromptOnError;
	MaterialBuildSettings.bAutoAddNewFiles = FALSE;

	FEditorBuildUtils::SubmitPackagesForAutomatedBuild(InPkgsToSubmit, MaterialBuildSettings);
}
/**
 * Helper method to submit packages to source control as part of the automated build process
 *
 * @param	InPkgsToSubmit	Set of packages which should be submitted to source control
 * @param	BuildSettings	Build settings used during the automated build
 */
void FEditorBuildUtils::SubmitPackagesForAutomatedBuild( const TSet<UPackage*>& InPkgsToSubmit, const FEditorAutomatedBuildSettings& BuildSettings )
{
#if HAVE_SCC && WITH_MANAGED_CODE
	TArray<FString> LevelsToAdd;
	TArray<FString> LevelsToSubmit;

	// Iterate over the set of packages to submit, determining if they need to be checked in or
	// added to the depot for the first time
	for ( TSet<UPackage*>::TConstIterator PkgIter( InPkgsToSubmit ); PkgIter; ++PkgIter )
	{
		const UPackage* CurPkg = *PkgIter;
		const FString PkgName = CurPkg->GetName();
		const INT PackageStatus = GPackageFileCache->GetSourceControlState( *PkgName );
		if ( PackageStatus == SCC_CheckedOut )
		{
			LevelsToSubmit.AddItem( PkgName );
		}
		else if ( BuildSettings.bAutoAddNewFiles && PackageStatus == SCC_NotInDepot )
		{
			LevelsToAdd.AddItem( PkgName );
		}
	}

	// Then, if we've also opted to check in any packages, iterate over that list as well
	if(BuildSettings.bCheckInPackages)
	{
		TArray<FString> PackageNames = BuildSettings.PackagesToCheckIn;
		for ( TArray<FString>::TConstIterator PkgIterName(PackageNames); PkgIterName; PkgIterName++ )
		{
			const FString PkgName = *PkgIterName;
			const INT PackageStatus = GPackageFileCache->GetSourceControlState( *PkgName );
			if ( PackageStatus == SCC_CheckedOut )
			{
				LevelsToSubmit.AddItem( PkgName );
			}
			else if ( PackageStatus == SCC_NotInDepot )
			{
				LevelsToAdd.AddItem( PkgName );
			}
		}
	}

	FSourceControl::ConvertPackageNamesToSourceControlPaths( LevelsToSubmit );
	FSourceControl::ConvertPackageNamesToSourceControlPaths( LevelsToAdd );
	FSourceControl::CheckIn( BuildSettings.SCCEventListener, LevelsToAdd, LevelsToSubmit, LocalizeUnrealEd("AutomatedBuild_AutomaticSubmission") + BuildSettings.ChangeDescription );
#endif
}

