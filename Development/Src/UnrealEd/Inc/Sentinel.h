/*=============================================================================
	Sentinel.h
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SENTINEL_H__
#define __SENTINEL_H__


/** One set of stats specific to a location and rotation */
struct FSentinelStatEntry
{
	FVector	Position;
	FRotator Rotation;

	FLOAT SecondsFromStartOfSession;

	TMap<FString, FLOAT>	StatData;
	TMap<FString, FString>	GameData;
    
    /** Constructor to set default values.**/
	FSentinelStatEntry(): SecondsFromStartOfSession(0.0f) {}
};


struct HSentinelStatProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HSentinelStatProxy,HHitProxy);

	INT	StatIndex;

	HSentinelStatProxy(INT InStatIndex):
		HHitProxy(HPP_UI),
		StatIndex(InStatIndex) {}
};

enum ESentinelFilterMode
{
	EVFM_Off,
	EVFM_Above,
	EVFM_Below
};

/** Main Sentinel viewport window */
class WxSentinel : public wxFrame
{
public:
	WxSentinel( WxEditorFrame* InEd );
	virtual ~WxSentinel();

	/** Called by editor to render stats graphics in 2D viewports. */
	void RenderStats(FEditorLevelViewportClient* ViewportClient, const FSceneView* View, FCanvas* Canvas);
	/** Called by editor to render stats graphics in perspective viewports. */
	void RenderStats3D(const FSceneView* View,class FPrimitiveDrawInterface* PDI);
	/** Called by editor when mouse moves */
	void MouseMoved(FEditorLevelViewportClient* ViewportClient, INT X, INT Y);
	/** Called by editor when key pressed. */
	void InputKey(FEditorLevelViewportClient* ViewportClient, FName Key,EInputEvent Event);

	/** Create connection to databse. */
	UBOOL ConnectToDatabase();

private:
	/** Pointer to owning main UnrealEd frame */
	WxEditorFrame* EditorFrame;

	/** Check box for determining whether or not we should be grabbing game stats **/
	wxCheckBox* UseGameStatsCheck;
	/** Combo that lets you choose which platform you want to see stats on */
	wxComboBox* PlatformCombo;
	/** Combo that lets you choose type of run you want to see stats from. */
	wxComboBox* RunTypeCombo;
	/** Combo that lets you choose which changelist you want to see stats from. */
	wxComboBox* ChangelistCombo;

	/** Combo that lets you choose the stat group for further filtering. */
	wxComboBox* StatGroupCombo;
	/** This stores the stat group selection index as we have some order issues on when this field gets updated and when it doesn't **/
	INT StatGroupSelectionIndex;

	/** Combo that lets you choose the stat you want to use as basis for coloured marks. */
	wxComboBox* StatNameCombo;
	/** Slider that lets you adjust size of stats drawn in viewport. */
	wxSlider* StatDrawSizeSlider;

	wxCheckBox* UsePresetColorsScale;

	wxSlider*	FilterValueSlider;

	wxTextCtrl* FilterValueBox;

	wxComboBox* FilterModeCombo;
	
	/** Status bar in Sentinel window. */
	class WxSentinelStatusBar* StatusBar;
	/** Databse connection object. */
	class FDataBaseConnection* Connection;
	/** Local cache of stats currently being viewed. */
	TArray<FSentinelStatEntry>	DataSet;
	/** Current size we are drawing graphics for stats. */
	FLOAT	StatDrawSize;
	/** Min value of currently selected stat. */
	FLOAT	CurrentStatMin;
	/** Max value of currently selected stat. */
	FLOAT	CurrentStatMax;

	INT	ToolTipStatIndex;
	INT ToolTipX;
	INT ToolTipY;
	FEditorLevelViewportClient* ToolTipViewport;

	UBOOL bUsePresetColors;

	ESentinelFilterMode FilterMode;

	FLOAT CurrentFilterValue;

	/** Calculate range of selected stat (CurrentStatMin/Max) */
	void CalcStatRange();
	/** Query the database to find the types of run that we have stats for. */
	void GetAvailableRunTypes();
	/** Query the database to find changelists available for selected run type and map. Updates ChangelistCombo. */
	void GetAvailableChangelists();
	/** Using selected run type, map and changelist, update local stat cache (DataSet). Also updates StatNameCombo. */
	void GrabDataSet();

	void OnUseGameStatsChange( wxCommandEvent& In );
	void OnPlatformChange( wxCommandEvent& In );
	void OnRunTypeChange( wxCommandEvent& In );
	void OnChangelistChange( wxCommandEvent& In );
	void OnStatGroupChange( wxCommandEvent& In );
	void OnStatChange( wxCommandEvent& In );
	void OnDrawSizeChange(wxScrollEvent& In );
	void OnUsePresetColors( wxCommandEvent& In );
	void OnFilterValueChange( wxScrollEvent& In );
	void OnFilterModeChange( wxCommandEvent& In );
	void OnClose( wxCloseEvent& In );

	DECLARE_EVENT_TABLE()
};

/** Sentinel status bar class */
class WxSentinelStatusBar : public wxStatusBar
{
public:
	WxSentinelStatusBar( wxWindow* InParent, wxWindowID InID);
};

#endif // __STATICMESHEDITOR_H__
