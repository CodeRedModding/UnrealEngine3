/*============================================================================
	AnimAimOffsetEditor.h
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "TrackableWindow.h"

enum
{
	ID_AOE_PROFILECOMBO,
	ID_AOE_PROFILENEW,
	ID_AOE_PROFILEDELETE,

	ID_AOE_LEFTUP,
	ID_AOE_CENTERUP,
	ID_AOE_RIGHTUP,
	ID_AOE_LEFTCENTER,
	ID_AOE_CENTERCENTER,
	ID_AOE_RIGHTCENTER,
	ID_AOE_LEFTDOWN,
	ID_AOE_CENTERDOWN,
	ID_AOE_RIGHTDOWN,
	ID_AOE_FORCENODE,

	ID_AOE_XENTRY,
	ID_AOE_YENTRY,
	ID_AOE_ZENTRY,
	ID_AOE_WORLDSPACEWIDGET,

	ID_AOE_BONELIST,
	ID_AOE_ADDBONE,
	ID_AOE_REMOVEBONE,

	ID_AOE_ROTATE,
	ID_AOE_TRANSLATE,

	ID_AOE_LOAD,
	ID_AOE_SAVE
};

enum EAAOEMoveMode
{
	AOE_Rotate,
	AOE_Translate
};

/*-----------------------------------------------------------------------------
	WxAnimAimOffsetToolBar
-----------------------------------------------------------------------------*/

class WxAnimAimOffsetToolBar : public WxToolBar
{
public:
	WxAnimAimOffsetToolBar( wxWindow* InParent, wxWindowID InID );
	~WxAnimAimOffsetToolBar();

	WxMaskedBitmap TranslateB, RotateB, BM_AOE_LOAD, BM_AOE_SAVE;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxAnimAimOffsetEditor
-----------------------------------------------------------------------------*/


class WxAnimAimOffsetEditor : public WxTrackableFrame
{
public:
	WxAnimTreeEditor* AnimTreeEditor;

	WxComboBox *ProfileCombo;
	wxTextCtrl *XEntry, *YEntry, *ZEntry;
	wxToggleButton *LUButton, *CUButton, *RUButton, *LCButton, *CCButton, *RCButton, *LDButton, *CDButton, *RDButton;
	wxListBox *BoneList;
	wxCheckBox *ForceNodeCheck, *WorldSpaceWidgetCheck;
	WxAnimAimOffsetToolBar*	ToolBar;
	UAnimNodeEditInfo_AimOffset* EditInfo;

	EAnimAimDir EditDir;
	UINT	EditBoneIndex;
	UBOOL bWorldSpaceWidget;
	EAAOEMoveMode MoveMode;

	WxAnimAimOffsetEditor( WxAnimTreeEditor* InEditor, wxWindowID InID, UAnimNodeEditInfo_AimOffset* InEditInfo );
	~WxAnimAimOffsetEditor();

	/**
	 * This function is called when the window has been selected from within the ctrl + tab dialog.
	 */
	virtual void OnSelected();

	// Control handling
	void OnLoadProfile( wxCommandEvent& In );
	void OnSaveProfile( wxCommandEvent& In );

	void OnClose( wxCloseEvent& In );
	void OnDirButton( wxCommandEvent& In );
	void OnDirEntry( wxCommandEvent& In );
	//void OnGetFocus( wxFocusEvent& In );
	//void OnLoseFocus( wxFocusEvent& In );
	void OnForceNode( wxCommandEvent& In );
	void OnAddBone( wxCommandEvent& In );
	void OnRemoveBone( wxCommandEvent& In );
	void OnSelectBone( wxCommandEvent& In );
	void OnWorldSpaceWidget( wxCommandEvent& In );
	void OnMoveMode( wxCommandEvent& In );
	void OnChangeProfile( wxCommandEvent& In );
	void OnNewProfile( wxCommandEvent& In );
	void OnDelProfile( wxCommandEvent& In );

	// Util
	void ProcessTextEntry();
	void UpdateTextEntry();
	void UpdateBoneList();
	void UncheckAllDirButtonExcept(EAnimAimDir InDir);
	void UpdateProfileCombo();


	DECLARE_EVENT_TABLE()
};
