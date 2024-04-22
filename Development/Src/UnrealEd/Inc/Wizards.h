/*=============================================================================
	Wizards.h: User friendly wizards to help with editor tasks

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	WxWizardPageSimple.
-----------------------------------------------------------------------------*/

class WxWizardPageSimple : public wxWizardPageSimple
{
public:
	WxWizardPageSimple( wxWizard* InParent, wxWizardPage* InPrev, wxWizardPage* InNext, const TCHAR* InTemplateName );
	~WxWizardPageSimple();

	wxPanel* Panel;

	virtual UBOOL InputIsValid();

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxWizard_NewTerrain.

	Creating a new terrain from scratch.
-----------------------------------------------------------------------------*/

class WxWizard_NewTerrain : public wxWizard
{
public:
	WxWizard_NewTerrain( wxWindow* InParent, int InID = -1 );
	~WxWizard_NewTerrain();

	UBOOL RunTerrainWizard();
	UBOOL Exec();

	wxPanel* TestPanel;
	class WxNewTerrain_Page1* Page1;
	class WxNewTerrain_Page2* Page2;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxNewTerrain_Page1.
-----------------------------------------------------------------------------*/

class WxNewTerrain_Page1 : public WxWizardPageSimple
{
public:
	WxNewTerrain_Page1( wxWizard* InParent = NULL, wxWizardPage* InPrev = NULL, wxWizardPage* InNext = NULL );
	~WxNewTerrain_Page1();

	wxTextCtrl *XText, *YText, *ZText, *XPatchesText, *YPatchesText;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxNewTerrain_Page2.
-----------------------------------------------------------------------------*/

class WxNewTerrain_Page2 : public WxWizardPageSimple
{
public:
	WxNewTerrain_Page2( wxWizard* InParent = NULL, wxWizardPage* InPrev = NULL, wxWizardPage* InNext = NULL );
	~WxNewTerrain_Page2();

	wxComboBox* LayerSetupCombo;

	DECLARE_EVENT_TABLE()
};

