/*=============================================================================
	Modes.h: Windows to handle the different editor modes and the tools
	         specific to them.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/*-----------------------------------------------------------------------------
	WxModeBar.
-----------------------------------------------------------------------------*/

class WxModeBar : public wxPanel
{
public:
	WxModeBar( wxWindow* InParent, wxWindowID InID );
	WxModeBar( wxWindow* InParent, wxWindowID InID, FString InPanelName );
	~WxModeBar();

	INT SavedHeight;				// The height recorded when the bar was initially loaded

	wxPanel* Panel;

	virtual void Refresh();
	virtual void SaveData() {}
	virtual void LoadData() {}

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxModeBarDefault.
-----------------------------------------------------------------------------*/

class WxModeBarDefault : public WxModeBar
{
public:
	WxModeBarDefault( wxWindow* InParent, wxWindowID InID );
	~WxModeBarDefault();

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxModeBarTerrainEdit.
-----------------------------------------------------------------------------*/

class WxModeBarTerrainEdit : public WxModeBar
{
public:
	WxModeBarTerrainEdit( wxWindow* InParent, wxWindowID InID );
	~WxModeBarTerrainEdit();

	virtual void SaveData();
	virtual void LoadData();
	void RefreshLayers();

	void OnToolsSelChange( wxCommandEvent& In );
	void SaveData( wxCommandEvent& In );
	void OnUpdateRadiusMin( wxCommandEvent& In );
	void OnUpdateRadiusMax( wxCommandEvent& In );
	void OnTerrainSelChange( wxCommandEvent& In );

	void UI_PerToolSettings( wxUpdateUIEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxModeBarGeometry.
-----------------------------------------------------------------------------*/

class WxModeBarGeometry : public WxModeBar
{
public:
	WxModeBarGeometry( wxWindow* InParent, wxWindowID InID );
	~WxModeBarGeometry();
	
	wxCheckBox *ShowNormalsCheck, *AffectWidgetOnlyCheck, *UseSoftSelectionCheck;
	wxTextCtrl *SoftSelectionRadiusText;
	wxStaticText *SoftSelectionLabel;

	virtual void SaveData();
	virtual void LoadData();
	void RefreshLayers();

	void OnToggleWindow( wxCommandEvent& In );
	void OnSelObject( wxCommandEvent& In );
	void OnSelPoly( wxCommandEvent& In );
	void OnSelEdge( wxCommandEvent& In );
	void OnSelVertex( wxCommandEvent& In );
	void UI_ToggleWindow( wxUpdateUIEvent& In );
	void UI_SelObject( wxUpdateUIEvent& In );
	void UI_SelPoly( wxUpdateUIEvent& In );
	void UI_SelEdge( wxUpdateUIEvent& In );
	void UI_SelVertex( wxUpdateUIEvent& In );

	void OnShowNormals( wxCommandEvent& In );
	void OnAffectWidgetOnly( wxCommandEvent& In );
	void OnUseSoftSelection( wxCommandEvent& In );
	void OnSoftSelectionRadius( wxCommandEvent& In );

	void UI_ShowNormals( wxUpdateUIEvent& In );
	void UI_AffectWidgetOnly( wxUpdateUIEvent& In );
	void UI_UseSoftSelection( wxUpdateUIEvent& In );
	void UI_SoftSelectionRadius( wxUpdateUIEvent& In );
	void UI_SoftSelectionLabel( wxUpdateUIEvent& In );

	wxToolBar* ToolBar;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxModeBarTexture.
-----------------------------------------------------------------------------*/

class WxModeBarTexture : public WxModeBar
{
public:
	WxModeBarTexture( wxWindow* InParent, wxWindowID InID );
	~WxModeBarTexture();

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
