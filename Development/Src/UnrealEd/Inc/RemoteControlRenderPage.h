/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __REMOTECONTROLRENDERPAGE_H__
#define __REMOTECONTROLRENDERPAGE_H__

#include "RemoteControlPage.h"

// Forward declarations.
class wxNotebook;
class wxChoice;
class wxTextCtrl;
class wxCheckBox;
class wxCommandEvent;

/**
 * Standard RemoteControl render page
 */
class WxRemoteControlRenderPage : public WxRemoteControlPage
{
public:
	WxRemoteControlRenderPage(FRemoteControlGame* Game, wxNotebook* Notebook);

	/**
	 * Return's the page's title, displayed on the notebook tab.
	 */
	virtual const TCHAR *GetPageTitle() const;

	/**
	 * Refreshes page contents.
	 */
	virtual void RefreshPage(UBOOL bForce = FALSE);

private:
	UBOOL bShowSkin;
	UBOOL bShowBones;

	wxChoice* GameResolutionChoice;
	wxChoice* MaxTextureSizeChoice;
	wxChoice* MaxShadowSizeChoice;
	wxChoice* UsePostProcessChoice;
	wxChoice* ViewmodeChoice;

	wxTextCtrl* SlomoTextCtrl;
	wxTextCtrl* FOVTextCtrl;

	wxCheckBox* DynamicShadowsCheckBox;
	wxCheckBox* ShowHUDCheckBox;
	wxCheckBox* PlayersOnlyCheckBox;
	wxCheckBox* FramesPerSecondCheckBox;
	wxCheckBox* D3DSceneCheckBox;
	wxCheckBox* MemoryCheckBox;

	/**
	 * Helper function for refreshing the value of DynamicShadows check box.
	 */
	void UpdateDynamicShadowsCheckBox();

	/**
	 * Helper function for refreshing the value of ShowHUD check box.
	 */
	void UpdatePlayersOnlyCheckBox();

	/**
	 * Helper function for refreshing the value of ShowHUD check box.
	 */
	void UpdateShowHUDCheckBox();

	/**
	 * Helper function for refreshing the value of Slomo text field.
	 */
	void UpdateSlomoTextCtrl();

	/**
	 * Helper function for refreshing the value of FOV text field.
	 */
	void UpdateFOVTextCtrl();

	///////////////////////////////////////
	// Wx event handlers.

	void OnGameResolutionChoice(wxCommandEvent& In);
	void OnPostProcessChoice(wxCommandEvent& In);
	void OnMaxTextureSizeChoice(wxCommandEvent& In);
	void OnMaxShadowSizeChoice(wxCommandEvent& In);
	void OnViewmodeChoice(wxCommandEvent& In);

	void OnOpenLevel(wxCommandEvent& In);
	void OnShowFlags(wxCommandEvent& In);

	void OnFPSClicked(wxCommandEvent& In);
	void OnD3DSceneClicked(wxCommandEvent& In);
	void OnMemoryClicked(wxCommandEvent& In);
	void OnApplySlomo(wxCommandEvent& In);
	void OnApplyFOV(wxCommandEvent& In);
	void OnDynamicShadows(wxCommandEvent &In);
	void OnShowHUD(wxCommandEvent& In);
	void OnPlayersOnly(wxCommandEvent& In);

	void OnShowStaticMeshes(wxCommandEvent& In);
	void OnShowTerrain(wxCommandEvent& In);
	void OnShowBSP(wxCommandEvent& In);
	void OnShowBSPSplit(wxCommandEvent& In);
	void OnShowCollision(wxCommandEvent& In);
	void OnShowBounds(wxCommandEvent& In);
	void OnShowZoneColors(wxCommandEvent& In );
	void OnShowMeshEdges(wxCommandEvent& In);
	void OnShowVertexColors(wxCommandEvent& In);
	void OnShowSceneCaptureUpdates (wxCommandEvent& In);
	void OnShowShadowFrustums(wxCommandEvent& In);
	void OnShowHitProxies(wxCommandEvent& In);
	void OnShowFog(wxCommandEvent& In);
	void OnShowParticles(wxCommandEvent& In);
	void OnShowConstraints(wxCommandEvent& In);
	void OnShowTerrainPatches(wxCommandEvent& In);
	void OnShowSkeletalMeshes(wxCommandEvent& In);
    void OnShowBones(wxCommandEvent& In);
    void OnShowSkin(wxCommandEvent& In);
	void OnShowDecals(wxCommandEvent& In);
	void OnShowDecalInfo(wxCommandEvent& In);
	void OnShowLevelColoration(wxCommandEvent& In);
	void OnShowSprites(wxCommandEvent& In);

	DECLARE_EVENT_TABLE()
};

#endif // __REMOTECONTROLRENDERPAGE_H__
