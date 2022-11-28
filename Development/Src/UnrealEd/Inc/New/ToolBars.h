/*=============================================================================
	ToolBars.h: The ToolBars used by the editor

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/*-----------------------------------------------------------------------------
	WxMainToolBar.

	Sits at the top of the main editor frame.
-----------------------------------------------------------------------------*/

class WxMainToolBar : public wxToolBar
{
public:
	WxMainToolBar( wxWindow* InParent, wxWindowID InID );
	~WxMainToolBar();

	WxMaskedBitmap NewB, OpenB, SaveB, UndoB, RedoB, CutB, CopyB, PasteB, SearchB, FullScreenB, GenericB, KismetB, TranslateB,
		ShowWidgetB, RotateB, ScaleB, ScaleNonUniformB, BrushPolysB, CamSlowB, CamNormalB, CamFastB;
	WxMenuButton MRUButton, PasteSpecialButton;
	wxMenu PasteSpecialMenu;
	wxComboBox* CoordSystemCombo;
};

/*-----------------------------------------------------------------------------
	WxToggleButtonBarControl.

	A control that goes on an WxToggleButtonBar.
-----------------------------------------------------------------------------*/

class WxToggleButtonBarControl : public wxControl
{
public:
	WxToggleButtonBarControl( wxWindow* InParent, wxWindowID InID, INT InMenuID );
	~WxToggleButtonBarControl();

	INT MenuID;

	virtual int   GetWidth()=0;

	virtual void  SetState( UBOOL InState ) {}
	virtual UBOOL GetState() { return 0; }

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxToggleButtonBarSeparator.

	A spacer between groups of controls.
-----------------------------------------------------------------------------*/

class WxToggleButtonBarSeparator : public WxToggleButtonBarControl
{
public:
	WxToggleButtonBarSeparator( wxWindow* InParent, wxWindowID InID, INT InMenuID );
	~WxToggleButtonBarSeparator();

	virtual int  GetWidth() { return 10; }

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxToggleButtonBarButton.

	A button-like control that toggles its state between on/off.
-----------------------------------------------------------------------------*/

class WxToggleButtonBarButton : public WxToggleButtonBarControl
{
public:
	WxToggleButtonBarButton();
	WxToggleButtonBarButton( wxWindow* InParent, wxWindowID InID, INT InMenuID, FString InOnBitmapFilename, FString InOffBitmapFilename );
	~WxToggleButtonBarButton();

	WxAlphaBitmap *OnB, *OffB;
	UBOOL bState;

	virtual int   GetWidth() { return 20; }

	virtual void  SetState( UBOOL InState );
	virtual UBOOL GetState() { return bState; }

	void OnPaint( wxPaintEvent& In );
	void OnLeftButtonDown( wxMouseEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxToggleButtonBar.

	A ToolBar-like control that toggles buttons between two states.
-----------------------------------------------------------------------------*/

class WxToggleButtonBar : public wxWindow
{
public:
	WxToggleButtonBar( wxWindow* InParent, wxWindowID InID );
	~WxToggleButtonBar();

	TArray<WxToggleButtonBarControl*> Controls;

	virtual void UpdateUI();

	WxToggleButtonBarButton* AddButton( INT InMenuID, FString InOnBitmapFilename, FString InOffBitmapFilename );
	WxToggleButtonBarSeparator* AddSeparator();
	void PositionControls();

	void OnSize( wxSizeEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxMaterialEditorToolBar.
-----------------------------------------------------------------------------*/

class WxMaterialEditorToolBar : public wxToolBar
{
public:
	WxMaterialEditorToolBar( wxWindow* InParent, wxWindowID InID );
	~WxMaterialEditorToolBar();

	WxMaskedBitmap BackgroundB, PlaneB, CylinderB, CubeB, SphereB, HomeB, RealTimeB;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxLevelViewportToolBar.
-----------------------------------------------------------------------------*/

class WxLevelViewportToolBar : public wxToolBar
{
public:
	WxLevelViewportToolBar( wxWindow* InParent, wxWindowID InID, FEditorLevelViewportClient* InViewportClient );
	~WxLevelViewportToolBar();

	WxMaskedBitmap RealTimeB, DownArrowLargeB;
	wxComboBox* ViewportTypeCombo;
	wxComboBox* SceneViewModeCombo;

	FEditorLevelViewportClient* ViewportClient;

	void UpdateUI();

	void OnRealTime( wxCommandEvent& In );
	void OnViewportTypeSelChange( wxCommandEvent& In );
	void OnSceneViewModeSelChange( wxCommandEvent& In );
	void OnShowFlags( wxMenuEvent& In );
	void OnShowStaticMeshes( wxMenuEvent& In );
	void OnShowTerrain( wxMenuEvent& In );
	void OnShowBSP( wxMenuEvent& In );
	void OnShowCollision( wxMenuEvent& In );
	void OnShowGrid( wxMenuEvent& In );
	void OnShowBounds( wxMenuEvent& In );
	void OnShowPaths( wxMenuEvent& In );
	void OnShowMeshEdges( wxMenuEvent& In );
	void OnShowLargeVertices( wxMenuEvent& In );
	void OnShowZoneColors( wxMenuEvent& In );
	void OnShowPortals( wxMenuEvent& In );
	void OnShowHitProxies( wxMenuEvent& In );
	void OnShowShadowFrustums( wxMenuEvent& In );
	void OnShowKismetRefs( wxMenuEvent& In );
	void OnShowVolumes( wxMenuEvent& In );
	void OnShowFog( wxMenuEvent& In );
	void OnShowCamFrustums( wxMenuEvent& In );

	enum	{TOOLBAR_H = 28};

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxGBViewToolBar.
-----------------------------------------------------------------------------*/

class WxGBViewToolBar : public wxToolBar
{
public:
	WxGBViewToolBar( wxWindow* InParent, wxWindowID InID );
	~WxGBViewToolBar();

	WxMaskedBitmap ListB, PreviewB, ThumbnailB, SphereB, CubeB, CylinderB, PlaneB;
	wxStaticText* ZoomST;
	wxComboBox* ViewCB;
	wxStaticText* NameFilterST;
	wxTextCtrl* NameFilterTC;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxModeGeometrySelBar.
-----------------------------------------------------------------------------*/

class WxModeGeometrySelBar : public wxToolBar
{
public:
	WxModeGeometrySelBar( wxWindow* InParent, wxWindowID InID );
	~WxModeGeometrySelBar();

	WxMaskedBitmap WindowB, ObjectB, PolyB, EdgeB, VertexB;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxStaticMeshEditorBar.
-----------------------------------------------------------------------------*/

class WxStaticMeshEditorBar : public wxToolBar
{
public:
	WxStaticMeshEditorBar( wxWindow* InParent, wxWindowID InID );
	~WxStaticMeshEditorBar();

	WxMaskedBitmap OpenEdgesB, WireframeB, BoundsB, CollisionB, LockB, CameraB;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
