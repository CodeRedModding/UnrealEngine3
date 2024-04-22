/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MAINTOOLBAR_H__
#define __MAINTOOLBAR_H__

class wxComboBox;
class wxWindow;
class WxAlphaBitmap;


/**
 * WxMatineeMenuListToolBarButton
 */
class WxMatineeMenuListToolBarButton
	: public WxBitmapButton
{

public:

private:

	/** Called when the tool bar button is clicked */
	void OnClick( wxCommandEvent &In );

	DECLARE_EVENT_TABLE()
};

/**
 * A small class representing a slider that pops up and hides itself when it loses focus or the mouse leaves the frame
 */
class WxPopupSlider : public wxFrame
{
public:
	WxPopupSlider( wxWindow* Parent, const wxPoint& Pos, UINT SliderID, INT StartValue, INT MinValue, INT MaxValue, LONG SliderStyle = wxSL_HORIZONTAL );
	virtual ~WxPopupSlider();
private:
	void OnMouseLeaveWindow( wxMouseEvent& In );
	void OnKillFocus( wxFocusEvent& In );
	wxSlider* Slider;

	DECLARE_EVENT_TABLE()
};

/**
 * The toolbar that sits at the top of the main editor frame.
 */
class WxMainToolBar : public WxToolBar
{
public:
	WxMainToolBar(wxWindow* InParent, wxWindowID InID);

	enum EPlatformButtons { B_PC=0, B_PCMobilePreview, B_PCMobilePreviewSettings, B_PS3, B_XBox360, B_NGP, B_iOS, B_MAX };

	/** Updates the 'Push View' toggle's bitmap and hint text based on the input state. */
	void SetPushViewState(UBOOL bIsPushingView);

	/** Enables/disables the 'Push View' button. */
	void EnablePushView(UBOOL bEnabled);

	/** Updates the LightingQuality button */
	void UpdateLightingQualityState();

private:
	WxMaskedBitmap NewB, OpenB, SaveB, UndoB, RedoB, CutB, CopyB, PasteB, SearchB, FullScreenB, ContentBrowserB, GenericB, KismetB, TranslateB,
		ShowWidgetB, RotateB, ScaleB, ScaleNonUniformB, MouseLockB, BrushPolysB, PrefabLockB, CamSlowB, CamNormalB, CamFastB, ViewPushStartB, ViewPushStopB, ViewPushSyncB,
		DistributionToggleB, SocketsB, PSysRealtimeLODToggleB, PSysHelperToggleB, MatineeListB, SentinelB, GameStatsVisualizerB, LODLockingB, QuickProcBuildingB, MatQualityLowB;
	wxToolBarToolBase* ViewPushStartStopButton;
	WxBitmap SaveAllLevelsB;
	WxBitmap SaveAllWritableB;
	WxBitmap BuildGeomB, BuildLightingB, BuildPathsB, BuildCoverNodesB, BuildAllB, BuildAllSubmitB, BuildAllSubmitDisabledB;
	WxBitmap LightingQualityImages[Quality_MAX];
	WxBitmap PlayOnB[B_MAX];
	WxBitmap SelectTranslucentB;
	WxBitmap PIEVisibleOnlyB;
	WxBitmap OrthoStrictBoxSelectionB;
	WxBitmap OrthoIntersectBoxSelectionB;
	WxBitmap PlayInEditorStartB;
	WxBitmap PlayInEditorStopB;
	WxMaskedBitmap RealtimeKismetDebuggingB;
	WxBitmap RealtimeAudioB;
	WxBitmap EmulateMobileFeaturesB;
	WxMaskedBitmap FavoritesB, DisabledFavoritesB;
	WxMenuButton MRUButton, PasteSpecialButton;
	WxBitmapCheckButton* SelectionModeButton;
	WxBitmapStateButton* PlayInEditorButton;
	wxMenu PasteSpecialMenu;

	/** Drop down of all available Matinee sequences in the level */
	WxMatineeMenuListToolBarButton MatineeListButton;

	/** 
	 * A volume slider that pops up when the toggle realtime audio button is right clicked
	 * It is declared here to prevent multiple sliders from appearing.
	 */
	WxPopupSlider* VolumeSlider;

	/** 
	 *	This is used to track the last lighting quality setting for properly
	 *	updating the button image (and preventing flicker).
	 */
	INT LastLightingQualitySetting;

	DECLARE_EVENT_TABLE();

	/** Called when the trans/rot/scale widget toolbar buttons are right-clicked. */
	void OnTransformButtonRightClick(wxCommandEvent& In);

	/** Called when the Matinee list tool bar button is clicked */
	void OnMatineeListMenu( wxCommandEvent& In );

	/** Called when the LightingQuality toolbar button is right-clicked. */
	void OnLightingQualityButtonRightClick(wxCommandEvent& In);

	/** Called when the LightingQuality toolbar button is left-clicked. */
	void OnLightingQualityButtonLeftClick(wxCommandEvent& In);

	/** Called when the build paths toolbar button is right-clicked. */
	void OnBuildAIPathsButtonRightClick(wxCommandEvent& In);

	/** Called when the play in editor button is right-clicked. */
	void OnPlayInEditorRightClick( wxCommandEvent& In );

	/** Called when the play on console button is right-clicked. */
	void OnPlayOnConsoleRightClick( wxCommandEvent& In );

	/** Called when the play on mobile preview button is right-clicked. */
	void OnPlayUsingMobilePreviewRightClick( wxCommandEvent& In );

	/** Called when the toggle Kismet realtime debugging button is clicked. */
	void OnRealtimeKismetDebugging( wxCommandEvent &In );

	/** Called when the real time audio button is right-clicked. */
	void OnRealTimeAudioRightClick( wxCommandEvent& In );

	/** Called to update the UI for the toggle favorites button */
	void UI_ToggleFavorites( wxUpdateUIEvent& In );

	/** Called whenever the right mouse button is released on a toolbar WxBitmapButton */
	void OnRightButtonUpOnControl( wxMouseEvent& In );

	/** Called whenever the play in editor button is clicked */
	void OnPlayInEditorButtonClick( wxCommandEvent& In );

	/** Called to update the UI for the play in editor button */
	void UI_PlayInEditor( wxUpdateUIEvent& In );

public:
	wxComboBox* CoordSystemCombo;
};

/**
 *	WxLightingQualityToolBarButtonRightClick 
 */
class WxLightingQualityToolBarButtonRightClick : public wxMenu
{
public:
	WxLightingQualityToolBarButtonRightClick(WxMainToolBar* MainToolBar);
	~WxLightingQualityToolBarButtonRightClick();

	/** Called when the LightingQuality button is right-clicked and an entry is selected */
	void OnLightingQualityButton( wxCommandEvent& In );
};

#endif // __MAINTOOLBAR_H__
