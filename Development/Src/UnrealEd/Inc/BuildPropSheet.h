/*=============================================================================
	BuildPropSheet : Property sheet for map rebuilding.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/


// --------------------------------------------------------------
//
// WPageOptions
//
// --------------------------------------------------------------

class WPageOptions : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WPageOptions,WPropertyPage,Window)

	WCheckBox *GeomCheck, *BuildOnlyVisibleCheck, *BSPCheck,
		*LightingCheck,
		*DefinePathsCheck, *OptimalButton,
		*DitherLightmapsCheck, *ReportErrorsCheck, *SaveLightmapsCheck;
	WGroupBox *GeomBox, *BSPBox, *OptimizationBox, *LightingBox, *DefinePathsBox;
	WButton *BuildPathsButton, *PathsChangedButton, *BuildButton, *LameButton, *GoodButton, *SaveButton, *DeleteButton;
	WTrackBar *BalanceBar, *PortalBiasBar;
	WLabel *BalanceLabel, *PortalBiasLabel;
	WComboBox *ConfigCombo, *LightmapCompressionCombo;

	// Structors.
	WPageOptions ( WWindow* InOwnerWindow );

	virtual void OpenWindow( INT InDlgId, HMODULE InHMOD );
	void OnDestroy();
	void RefreshConfigCombo();
	virtual void Refresh();
	void InitFromCurrentConfig();
	void UpdateCurrentConfig();
	void OnBalanceBarMove();
	void OnPortalBiasBarMove();
	void OnConfigComboSelChange();
	void OnLightmapCompressionComboSelChange();
	void OnDitherLightmaps();
	void OnSaveLightmaps();
	void OnReportErrors();
	void OnGeomClick();
	void OnLameClicked();
	void OnGoodClicked();
	void OnOptimalClicked();
	void OnBSPClick();
	void OnLightingClick();
	void OnDefinePathsClick();
	void OnBuildOnlyVisibleClick();
	void BuildGeometry();
	void BuildBSP();
	void BuildLighting();
	void BuildPaths();
	void BuildFluids();
	void OnBuildClick();
	void OnBuildPathsClick();
	void OnPathsChangedClick();
	void OnSaveClick();
	void OnDeleteClick();
};

// --------------------------------------------------------------
//
// WPageLevelStats
//
// --------------------------------------------------------------

class WPageLevelStats : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WPageLevelStats,WPropertyPage,Window)

	WGroupBox *GeomBox, *BSPBox, *LightingBox, *DefinePathsBox;
	WButton *RefreshButton, *BuildButton;
	WLabel *BrushesLabel, *ZonesLabel, *PolysLabel, *NodesLabel,
		*RatioLabel, *MaxDepthLabel, *AvgDepthLabel, *LightsLabel;

	// Structors.
	WPageLevelStats ( WWindow* InOwnerWindow );

	virtual void OpenWindow( INT InDlgId, HMODULE InHMOD );
	void OnDestroy();
	virtual void Refresh();
};

// --------------------------------------------------------------
//
// WBuildPropSheet
//
// --------------------------------------------------------------

class WBuildPropSheet : public WWindow
{
	DECLARE_WINDOWCLASS(WBuildPropSheet,WWindow,Window)

	WPropertySheet* PropSheet;
	WPageOptions* OptionsPage;
	WPageLevelStats* LevelStatsPage;

	// Structors.
	WBuildPropSheet( FName InPersistentName, WWindow* InOwnerWindow );

	// WBuildPropSheet interface.
	void OpenWindow();
	void OnCreate();
	void OnDestroy();
	void OnSize( DWORD Flags, INT NewX, INT NewY );
	void PositionChildControls();
	INT OnSysCommand( INT Command );
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
