
#include "UnrealEd.h"

extern class WxUnrealEdApp* GApp;

/*-----------------------------------------------------------------------------
	WxMainToolBar.
-----------------------------------------------------------------------------*/

WxMainToolBar::WxMainToolBar( wxWindow* InParent, wxWindowID InID )
	: wxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxNO_BORDER | wxTB_3DBUTTONS )
{
	// Menus

	PasteSpecialMenu.Append( IDM_PASTE_ORIGINAL_LOCATION, TEXT("&Original Location") );
	PasteSpecialMenu.Append( IDM_PASTE_WORLD_ORIGIN, TEXT("&World Origin") );

	// Bitmaps

	NewB.Load( TEXT("New") );
	OpenB.Load( TEXT("Open") );
	SaveB.Load( TEXT("Save") );
	UndoB.Load( TEXT("Undo") );
	RedoB.Load( TEXT("Redo") );
	CutB.Load( TEXT("Cut") );
	CopyB.Load( TEXT("Copy") );
	PasteB.Load( TEXT("Paste") );
	SearchB.Load( TEXT("Search") );
	FullScreenB.Load( TEXT("FullScreen") );
	GenericB.Load( TEXT("ThumbnailView") );
	KismetB.Load( TEXT("Kismet") );
	ShowWidgetB.Load( TEXT("ShowWidget") );
	TranslateB.Load( TEXT("Translate") );
	RotateB.Load( TEXT("Rotate") );
	ScaleB.Load( TEXT("Scale") );
	ScaleNonUniformB.Load( TEXT("ScaleNonUniform") );
	BrushPolysB.Load( TEXT("BrushPolys") );
	CamSlowB.Load( TEXT("CamSlow") );
	CamNormalB.Load( TEXT("CamNormal") );
	CamFastB.Load( TEXT("CamFast") );

	// Create special windows

	MRUButton.Create( this, IDPB_MRU_DROPDOWN, &GApp->EditorFrame->DownArrowB, GApp->EditorFrame->MainMenuBar->MRUMenu, wxPoint(0,0), wxSize(-1,21) );
	//MRUButton.SetToolTip( TEXT("Open a Recently Opened Map") );

	//PasteSpecialButton.Create( this, IDPB_PASTE_SPECIAL, &GApp->EditorFrame->DownArrowB, &PasteSpecialMenu, wxPoint(0,0), wxSize(-1,21) );
	//PasteSpecialButton.SetToolTip( TEXT("Special Paste Options") );

	// Coordinate systems

	CoordSystemCombo = new wxComboBox( this, IDCB_COORD_SYSTEM, TEXT(""), wxDefaultPosition, wxSize( 80, -1 ), 0, NULL, wxCB_READONLY );
	CoordSystemCombo->Append( TEXT("World") );
	CoordSystemCombo->Append( TEXT("Local") );
	CoordSystemCombo->SetSelection( GEditorModeTools.CoordSystem );
	CoordSystemCombo->SetToolTip( TEXT("Reference Coordinate System") );

	// Set up the ToolBar

	AddSeparator();
	AddTool( IDM_NEW, TEXT(""), NewB, TEXT("Create a new map") );
	AddTool( IDM_OPEN, TEXT(""), OpenB, TEXT("Open an existing map") );
	AddControl( &MRUButton );
	AddTool( IDM_SAVE, TEXT(""), SaveB, TEXT("Save the map") );
	AddSeparator();
	AddTool( IDM_UNDO, TEXT(""), UndoB, TEXT("Undo the last action") );
	AddTool( IDM_REDO, TEXT(""), RedoB, TEXT("Redo the previously undone action") );
	AddSeparator();
	AddCheckTool( ID_EDIT_SHOW_WIDGET, TEXT(""), ShowWidgetB, ShowWidgetB, TEXT("Show/Use the widget.") );
	AddCheckTool( ID_EDIT_TRANSLATE, TEXT(""), TranslateB, TranslateB, TEXT("Use the translation widget.") );
	AddCheckTool( ID_EDIT_ROTATE, TEXT(""), RotateB, RotateB, TEXT("Use the rotation widget.") );
	AddCheckTool( ID_EDIT_SCALE, TEXT(""), ScaleB, ScaleB, TEXT("Use the scaling widget.") );
	AddCheckTool( ID_EDIT_SCALE_NONUNIFORM, TEXT(""), ScaleNonUniformB, ScaleNonUniformB, TEXT("Use the non-uniform scaling widget.") );
	AddSeparator();
	AddControl( CoordSystemCombo );
	AddSeparator();
	AddTool( IDM_SEARCH, TEXT(""), SearchB, TEXT("Search for actors") );
	AddSeparator();
	AddCheckTool( IDM_FULLSCREEN, TEXT(""), FullScreenB, FullScreenB, TEXT("Toggle fullscreen mode") );
	AddSeparator();
	AddTool( IDM_CUT, TEXT(""), CutB, TEXT("Cut out the selection and put it into the clipboard") );
	AddTool( IDM_COPY, TEXT(""), CopyB, TEXT("Copy the selection and put it into the clipboard") );
	AddTool( IDM_PASTE, TEXT(""), PasteB, TEXT("Paste data in the clipboard into the map") );
	//AddControl( &PasteSpecialButton );
	AddSeparator();
	AddTool( IDM_BROWSER_GENERIC, TEXT(""), GenericB, TEXT("Open the Generic Browser window") );
	AddSeparator();
	AddTool( IDM_OPEN_KISMET, TEXT(""), KismetB, TEXT("Open Kismet") );
	AddSeparator();
	AddCheckTool( IDM_BRUSHPOLYS, TEXT(""), BrushPolysB, BrushPolysB, TEXT("Toggle Brush Polys") );
	AddSeparator();
	AddCheckTool( ID_CAMSPEED_SLOW, TEXT(""), CamSlowB, CamSlowB, TEXT("Slowest Camera Speed") );
	AddCheckTool( ID_CAMSPEED_NORMAL, TEXT(""), CamNormalB, CamNormalB, TEXT("Normal Camera Speed") );
	AddCheckTool( ID_CAMSPEED_FAST, TEXT(""), CamFastB, CamFastB, TEXT("Fastest Camera Speed") );

	Realize();
}

WxMainToolBar::~WxMainToolBar()
{
}

/*-----------------------------------------------------------------------------
	WxToggleButtonBarControl.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxToggleButtonBarControl, wxControl )
END_EVENT_TABLE()

WxToggleButtonBarControl::WxToggleButtonBarControl( wxWindow* InParent, wxWindowID InID, INT InMenuID )
	: wxControl( InParent, InID )
{
	MenuID = InMenuID;
}

WxToggleButtonBarControl::~WxToggleButtonBarControl()
{
}

/*-----------------------------------------------------------------------------
	WxToggleButtonBarSeparator.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxToggleButtonBarSeparator, WxToggleButtonBarControl )
END_EVENT_TABLE()

WxToggleButtonBarSeparator::WxToggleButtonBarSeparator( wxWindow* InParent, wxWindowID InID, INT InMenuID )
	: WxToggleButtonBarControl( InParent, InID, InMenuID )
{
}

WxToggleButtonBarSeparator::~WxToggleButtonBarSeparator()
{
}

/*-----------------------------------------------------------------------------
	WxToggleButtonBarButton.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxToggleButtonBarButton, WxToggleButtonBarControl )
	EVT_PAINT( WxToggleButtonBarButton::OnPaint )
	EVT_LEFT_DOWN( WxToggleButtonBarButton::OnLeftButtonDown )
END_EVENT_TABLE()

WxToggleButtonBarButton::WxToggleButtonBarButton( wxWindow* InParent, wxWindowID InID, INT InMenuID, FString InOnBitmapFilename, FString InOffBitmapFilename )
	: WxToggleButtonBarControl( InParent, InID, InMenuID )
{
	OnB = new WxAlphaBitmap( InOnBitmapFilename, 0 );
	OffB = new WxAlphaBitmap( InOffBitmapFilename, 0 );

	bState = 0;
}

WxToggleButtonBarButton::~WxToggleButtonBarButton()
{
	delete OnB;
	delete OffB;
}

void WxToggleButtonBarButton::SetState( UBOOL InState )
{
	bState = InState;
}

void WxToggleButtonBarButton::OnPaint( wxPaintEvent& In )
{
	wxPaintDC dc(this);
	wxRect rc = GetClientRect();
	wxBitmap* bmp = bState ? OnB : OffB;

	dc.DrawBitmap( *bmp, (rc.width - bmp->GetWidth() ) / 2, (rc.height - bmp->GetHeight() ) / 2 );
}

void WxToggleButtonBarButton::OnLeftButtonDown( wxMouseEvent& In )
{
	SetState( !bState );

    wxCommandEvent  eventCommand = wxCommandEvent( wxEVT_COMMAND_MENU_SELECTED, MenuID );
	((WxToggleButtonBar*)GetParent())->AddPendingEvent( eventCommand );
	wxYield();
	GCallback->Send( CALLBACK_UpdateUI );
}

/*-----------------------------------------------------------------------------
	WxToggleButtonBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxToggleButtonBar, wxWindow )
	EVT_SIZE( WxToggleButtonBar::OnSize )
END_EVENT_TABLE()

WxToggleButtonBar::WxToggleButtonBar( wxWindow* InParent, wxWindowID InID )
	: wxWindow( InParent, InID )
{
}

WxToggleButtonBar::~WxToggleButtonBar()
{
}

WxToggleButtonBarButton* WxToggleButtonBar::AddButton( INT InMenuID, FString InOnBitmapFilename, FString InOffBitmapFilename )
{
	WxToggleButtonBarButton* Wk = new WxToggleButtonBarButton( this, -1, InMenuID, InOnBitmapFilename, InOffBitmapFilename );
	Controls.AddItem( Wk );
	return Wk;
}

WxToggleButtonBarSeparator* WxToggleButtonBar::AddSeparator()
{
	WxToggleButtonBarSeparator* Wk = new WxToggleButtonBarSeparator( this, -1, -1 );
	Controls.AddItem( Wk );
	return Wk;
}

// Finds good positions for all buttons

void WxToggleButtonBar::PositionControls()
{
	INT XPos = 2;

	for( INT x = 0 ; x < Controls.Num() ; ++x )
	{
		WxToggleButtonBarControl* ctrl = Controls(x);
		ctrl->SetSize( XPos,2, ctrl->GetWidth(), 24 );
		XPos += ctrl->GetWidth();
	}
}

void WxToggleButtonBar::OnSize( wxSizeEvent& In )
{
	PositionControls();
}

void WxToggleButtonBar::UpdateUI()
{
}

/*-----------------------------------------------------------------------------
	WxMaterialEditorToolBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxMaterialEditorToolBar, wxToolBar )
END_EVENT_TABLE()

WxMaterialEditorToolBar::WxMaterialEditorToolBar( wxWindow* InParent, wxWindowID InID )
	: wxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_3DBUTTONS )
{
	// Bitmaps

	BackgroundB.Load( TEXT("Background") );
	PlaneB.Load( TEXT("ME_Plane") );
	CylinderB.Load( TEXT("ME_Cylinder") );
	CubeB.Load( TEXT("ME_Cube") );
	SphereB.Load( TEXT("ME_Sphere") );
	HomeB.Load( TEXT("Home") );
	RealTimeB.Load( TEXT("Realtime") );
	SetToolBitmapSize( wxSize( 18, 18 ) );

	// Set up the ToolBar

	AddCheckTool( IDM_SHOW_BACKGROUND, TEXT(""), BackgroundB, BackgroundB, TEXT("Show Background?") );
	AddSeparator();
	AddRadioTool( ID_PRIMTYPE_PLANE, TEXT(""), PlaneB, wxNullBitmap, TEXT("Plane") );
	AddRadioTool( ID_PRIMTYPE_CYLINDER, TEXT(""), CylinderB, wxNullBitmap, TEXT("Cylinder") );
	AddRadioTool( ID_PRIMTYPE_CUBE, TEXT(""), CubeB, wxNullBitmap, TEXT("Cube") );
	AddRadioTool( ID_PRIMTYPE_SPHERE, TEXT(""), SphereB, wxNullBitmap, TEXT("Sphere") );
	AddSeparator();
	AddTool( ID_GO_HOME, TEXT(""), HomeB, TEXT("Home") );
	AddSeparator();
	AddCheckTool( IDM_REALTIME, TEXT(""), RealTimeB, RealTimeB, TEXT("Real Time") );

	Realize();
}

WxMaterialEditorToolBar::~WxMaterialEditorToolBar()
{
}

/*-----------------------------------------------------------------------------
	WxLevelViewportToolBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxLevelViewportToolBar, wxToolBar )
	EVT_TOOL( IDM_REALTIME, WxLevelViewportToolBar::OnRealTime )
	EVT_COMBOBOX( IDCB_VIEWPORT_TYPE, WxLevelViewportToolBar::OnViewportTypeSelChange )
	EVT_COMBOBOX( IDCB_SCENE_VIEW_MODE, WxLevelViewportToolBar::OnSceneViewModeSelChange )
	EVT_MENU( IDM_SHOW_FLAGS, WxLevelViewportToolBar::OnShowFlags )
	EVT_MENU( IDM_VIEWPORT_SHOW_STATICMESHES, WxLevelViewportToolBar::OnShowStaticMeshes )
	EVT_MENU( IDM_VIEWPORT_SHOW_TERRAIN, WxLevelViewportToolBar::OnShowTerrain )
	EVT_MENU( IDM_VIEWPORT_SHOW_BSP, WxLevelViewportToolBar::OnShowBSP )
	EVT_MENU( IDM_VIEWPORT_SHOW_COLLISION, WxLevelViewportToolBar::OnShowCollision )
	EVT_MENU( IDM_VIEWPORT_SHOW_GRID, WxLevelViewportToolBar::OnShowGrid )
	EVT_MENU( IDM_VIEWPORT_SHOW_BOUNDS, WxLevelViewportToolBar::OnShowBounds )
	EVT_MENU( IDM_VIEWPORT_SHOW_PATHS, WxLevelViewportToolBar::OnShowPaths )
	EVT_MENU( IDM_VIEWPORT_SHOW_MESHEDGES, WxLevelViewportToolBar::OnShowMeshEdges )
	EVT_MENU( IDM_VIEWPORT_SHOW_LARGEVERTICES, WxLevelViewportToolBar::OnShowLargeVertices )
	EVT_MENU( IDM_VIEWPORT_SHOW_ZONECOLORS, WxLevelViewportToolBar::OnShowZoneColors )
	EVT_MENU( IDM_VIEWPORT_SHOW_PORTALS, WxLevelViewportToolBar::OnShowPortals )
	EVT_MENU( IDM_VIEWPORT_SHOW_HITPROXIES, WxLevelViewportToolBar::OnShowHitProxies )
	EVT_MENU( IDM_VIEWPORT_SHOW_SHADOWFRUSTUMS, WxLevelViewportToolBar::OnShowShadowFrustums )
	EVT_MENU( IDM_VIEWPORT_SHOW_KISMETREFS, WxLevelViewportToolBar::OnShowKismetRefs )
	EVT_MENU( IDM_VIEWPORT_SHOW_VOLUMES, WxLevelViewportToolBar::OnShowVolumes )
	EVT_MENU( IDM_VIEWPORT_SHOW_CAMFRUSTUMS, WxLevelViewportToolBar::OnShowCamFrustums )
	EVT_MENU( IDM_VIEWPORT_SHOW_FOG, WxLevelViewportToolBar::OnShowFog )
END_EVENT_TABLE()

WxLevelViewportToolBar::WxLevelViewportToolBar( wxWindow* InParent, wxWindowID InID, FEditorLevelViewportClient* InViewportClient )
	: wxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_3DBUTTONS )
	,	ViewportClient( InViewportClient )
{
	// Bitmaps

	RealTimeB.Load( TEXT("Realtime") );
	DownArrowLargeB.Load( TEXT("DownArrowLarge") );

	//SetToolBitmapSize( wxSize( 16, 16 ) );

	ViewportTypeCombo = new wxComboBox( this, IDCB_VIEWPORT_TYPE, TEXT(""), wxDefaultPosition, wxSize( 90, -1 ), 0, NULL, wxCB_READONLY );
	ViewportTypeCombo->Append( TEXT("Perspective") );
	ViewportTypeCombo->Append( TEXT("Top") );
	ViewportTypeCombo->Append( TEXT("Side") );
	ViewportTypeCombo->Append( TEXT("Front") );
	ViewportTypeCombo->SetToolTip( TEXT("Viewport Type") );

	SceneViewModeCombo = new wxComboBox( this, IDCB_SCENE_VIEW_MODE, TEXT(""), wxDefaultPosition, wxSize( 120, -1 ), 0, NULL, wxCB_READONLY );
	SceneViewModeCombo->Append( TEXT("Brush Wireframe") );
	SceneViewModeCombo->Append( TEXT("Wireframe") );
	SceneViewModeCombo->Append( TEXT("Unlit") );
	SceneViewModeCombo->Append( TEXT("Lit") );
	SceneViewModeCombo->Append( TEXT("Lighting Only") );
	SceneViewModeCombo->Append( TEXT("Light Complexity") );
	SceneViewModeCombo->SetToolTip( TEXT("Scene View Mode") );

	UpdateUI();

	// Set up the ToolBar

	AddSeparator();
	AddCheckTool( IDM_REALTIME, TEXT(""), RealTimeB, RealTimeB, TEXT("Real Time") );
	AddSeparator();
	AddControl( ViewportTypeCombo );
	AddControl( SceneViewModeCombo );
	AddSeparator();
	AddTool( IDM_SHOW_FLAGS, TEXT(""), DownArrowLargeB, TEXT("Toggle Show Flags") );

	Realize();
}

WxLevelViewportToolBar::~WxLevelViewportToolBar()
{
}

void WxLevelViewportToolBar::UpdateUI()
{
	if( ViewportClient->Realtime )
	{
		ToggleTool(IDM_REALTIME, true);
	}
	else
	{
		ToggleTool(IDM_REALTIME, false);
	}

	switch( ViewportClient->ViewportType )
	{
		case LVT_Perspective:	ViewportTypeCombo->SetSelection( 0 );	break;
		case LVT_OrthoXY:		ViewportTypeCombo->SetSelection( 1 );	break;
		case LVT_OrthoXZ:		ViewportTypeCombo->SetSelection( 2 );	break;
		case LVT_OrthoYZ:		ViewportTypeCombo->SetSelection( 3 );	break;
		default:
			break;
	}

	switch( ViewportClient->ViewMode )
	{
		case SVM_BrushWireframe:	SceneViewModeCombo->SetSelection( 0 );	break;
		case SVM_Wireframe:			SceneViewModeCombo->SetSelection( 1 );	break;
		case SVM_Unlit:				SceneViewModeCombo->SetSelection( 2 );	break;
		case SVM_Lit:				SceneViewModeCombo->SetSelection( 3 );	break;
		case SVM_LightingOnly:		SceneViewModeCombo->SetSelection( 4 );	break;
		case SVM_LightComplexity:	SceneViewModeCombo->SetSelection( 5 );	break;
		default:
			break;
	}
}

void WxLevelViewportToolBar::OnRealTime( wxCommandEvent& In )
{
	ViewportClient->Realtime		= In.IsChecked();
	ViewportClient->RealtimeAudio	= In.IsChecked();
	ViewportClient->Viewport->Invalidate();
}

void WxLevelViewportToolBar::OnViewportTypeSelChange( wxCommandEvent& In )
{
	switch( In.GetInt() )
	{
		case 0:					ViewportClient->ViewportType = LVT_Perspective;		break;
		case 1:					ViewportClient->ViewportType = LVT_OrthoXY;			break;
		case 2:					ViewportClient->ViewportType = LVT_OrthoXZ;			break;
		case 3:					ViewportClient->ViewportType = LVT_OrthoYZ;			break;
		default:
			break;
	}
	ViewportClient->Viewport->Invalidate();
}

void WxLevelViewportToolBar::OnSceneViewModeSelChange( wxCommandEvent& In )
{
	switch( In.GetInt() )
	{
		case 0:					ViewportClient->ViewMode = SVM_BrushWireframe;		break;
		case 1:					ViewportClient->ViewMode = SVM_Wireframe;			break;
		case 2:					ViewportClient->ViewMode = SVM_Unlit;				break;
		case 3:					ViewportClient->ViewMode = SVM_Lit;					break;
		case 4:					ViewportClient->ViewMode = SVM_LightingOnly;		break;
		case 5:					ViewportClient->ViewMode = SVM_LightComplexity;		break;
		default:
			break;
	}
	ViewportClient->Viewport->Invalidate();
}

void WxLevelViewportToolBar::OnShowFlags( wxMenuEvent& In )
{
	// Show the user a menu of all the available show flags

	wxMenu* menu = new wxMenu( TEXT("SHOW FLAGS") );

	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_STATICMESHES, TEXT("Static Meshes") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_TERRAIN, TEXT("Terrain") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_BSP, TEXT("BSP") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_COLLISION, TEXT("Collision") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_GRID, TEXT("Grid") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_BOUNDS, TEXT("Bounds") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_PATHS, TEXT("Paths") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_MESHEDGES, TEXT("Mesh Edges") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_LARGEVERTICES, TEXT("Large Vertices") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_ZONECOLORS, TEXT("Zone Colors") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_PORTALS, TEXT("Portals") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_HITPROXIES, TEXT("Hit Proxies") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_SHADOWFRUSTUMS, TEXT("Shadow Frustums") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_KISMETREFS, TEXT("Kismet References") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_VOLUMES, TEXT("Volumes") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_FOG, TEXT("Fog") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_CAMFRUSTUMS, TEXT("Camera Frustums") );

    menu->Check( IDM_VIEWPORT_SHOW_STATICMESHES,    ( ViewportClient->ShowFlags & SHOW_StaticMeshes ) ? true : false );
    menu->Check( IDM_VIEWPORT_SHOW_TERRAIN,         ( ViewportClient->ShowFlags & SHOW_Terrain ) ? true : false );
    menu->Check( IDM_VIEWPORT_SHOW_BSP,             ( ViewportClient->ShowFlags & SHOW_BSP ) ? true : false );
    menu->Check( IDM_VIEWPORT_SHOW_COLLISION,       ( ViewportClient->ShowFlags & SHOW_Collision ) ? true : false );
    menu->Check( IDM_VIEWPORT_SHOW_GRID,            ( ViewportClient->ShowFlags & SHOW_Grid ) ? true : false );
	menu->Check( IDM_VIEWPORT_SHOW_BOUNDS,          ( ViewportClient->ShowFlags & SHOW_Bounds ) ? true : false );
	menu->Check( IDM_VIEWPORT_SHOW_PATHS,           ( ViewportClient->ShowFlags & SHOW_Paths ) ? true : false );
	menu->Check( IDM_VIEWPORT_SHOW_MESHEDGES,       ( ViewportClient->ShowFlags & SHOW_MeshEdges ) ? true : false );
	menu->Check( IDM_VIEWPORT_SHOW_LARGEVERTICES,   ( ViewportClient->ShowFlags & SHOW_LargeVertices ) ? true : false );
	menu->Check( IDM_VIEWPORT_SHOW_ZONECOLORS,      ( ViewportClient->ShowFlags & SHOW_ZoneColors ) ? true : false );
	menu->Check( IDM_VIEWPORT_SHOW_PORTALS,         ( ViewportClient->ShowFlags & SHOW_Portals ) ? true : false );
	menu->Check( IDM_VIEWPORT_SHOW_HITPROXIES,      ( ViewportClient->ShowFlags & SHOW_HitProxies ) ? true : false );
	menu->Check( IDM_VIEWPORT_SHOW_SHADOWFRUSTUMS,  ( ViewportClient->ShowFlags & SHOW_ShadowFrustums ) ? true : false );
	menu->Check( IDM_VIEWPORT_SHOW_KISMETREFS,      ( ViewportClient->ShowFlags & SHOW_KismetRefs ) ? true : false );
	menu->Check( IDM_VIEWPORT_SHOW_VOLUMES,         ( ViewportClient->ShowFlags & SHOW_Volumes ) ? true : false );
	menu->Check( IDM_VIEWPORT_SHOW_FOG,             ( ViewportClient->ShowFlags & SHOW_Fog ) ? true : false );
	menu->Check( IDM_VIEWPORT_SHOW_CAMFRUSTUMS,     ( ViewportClient->ShowFlags & SHOW_CamFrustums ) ? true : false );

	FTrackPopupMenu tpm( this, menu );
	tpm.Show();
	delete menu;
}

void WxLevelViewportToolBar::OnShowStaticMeshes( wxMenuEvent& In )	{	ViewportClient->ShowFlags ^= SHOW_StaticMeshes;		ViewportClient->Viewport->Invalidate();	}
void WxLevelViewportToolBar::OnShowTerrain( wxMenuEvent& In )		{	ViewportClient->ShowFlags ^= SHOW_Terrain;			ViewportClient->Viewport->Invalidate();	}
void WxLevelViewportToolBar::OnShowBSP( wxMenuEvent& In )			{	ViewportClient->ShowFlags ^= SHOW_BSP;				ViewportClient->Viewport->Invalidate();	}
void WxLevelViewportToolBar::OnShowCollision( wxMenuEvent& In )		{	ViewportClient->ShowFlags ^= SHOW_Collision;		ViewportClient->Viewport->Invalidate();	}
void WxLevelViewportToolBar::OnShowGrid( wxMenuEvent& In )			{	ViewportClient->ShowFlags ^= SHOW_Grid;				ViewportClient->Viewport->Invalidate();	}
void WxLevelViewportToolBar::OnShowBounds( wxMenuEvent& In )			{	ViewportClient->ShowFlags ^= SHOW_Bounds;			ViewportClient->Viewport->Invalidate();	}
void WxLevelViewportToolBar::OnShowPaths( wxMenuEvent& In )			{	ViewportClient->ShowFlags ^= SHOW_Paths;			ViewportClient->Viewport->Invalidate();	}
void WxLevelViewportToolBar::OnShowMeshEdges( wxMenuEvent& In )		{	ViewportClient->ShowFlags ^= SHOW_MeshEdges;		ViewportClient->Viewport->Invalidate();	}
void WxLevelViewportToolBar::OnShowLargeVertices( wxMenuEvent& In )	{	ViewportClient->ShowFlags ^= SHOW_LargeVertices;	ViewportClient->Viewport->Invalidate();	}
void WxLevelViewportToolBar::OnShowZoneColors( wxMenuEvent& In )		{	ViewportClient->ShowFlags ^= SHOW_ZoneColors;		ViewportClient->Viewport->Invalidate(); }
void WxLevelViewportToolBar::OnShowPortals( wxMenuEvent& In )		{	ViewportClient->ShowFlags ^= SHOW_Portals;			ViewportClient->Viewport->Invalidate(); }
void WxLevelViewportToolBar::OnShowHitProxies( wxMenuEvent& In )		{	ViewportClient->ShowFlags ^= SHOW_HitProxies;		ViewportClient->Viewport->Invalidate(); }
void WxLevelViewportToolBar::OnShowShadowFrustums( wxMenuEvent& In )	{	ViewportClient->ShowFlags ^= SHOW_ShadowFrustums;	ViewportClient->Viewport->Invalidate(); }
void WxLevelViewportToolBar::OnShowKismetRefs( wxMenuEvent& In )		{	ViewportClient->ShowFlags ^= SHOW_KismetRefs;		ViewportClient->Viewport->Invalidate(); }
void WxLevelViewportToolBar::OnShowVolumes( wxMenuEvent& In )		{	ViewportClient->ShowFlags ^= SHOW_Volumes;			ViewportClient->Viewport->Invalidate(); }
void WxLevelViewportToolBar::OnShowFog( wxMenuEvent& In )			{	ViewportClient->ShowFlags ^= SHOW_Fog;				ViewportClient->Viewport->Invalidate(); }
void WxLevelViewportToolBar::OnShowCamFrustums( wxMenuEvent& In )	{	ViewportClient->ShowFlags ^= SHOW_CamFrustums;		ViewportClient->Viewport->Invalidate(); }

/*-----------------------------------------------------------------------------
	WxGBViewToolBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxGBViewToolBar, wxToolBar )
END_EVENT_TABLE()

WxGBViewToolBar::WxGBViewToolBar( wxWindow* InParent, wxWindowID InID )
	: wxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_3DBUTTONS )
{
	// Bitmaps

	ListB.Load( TEXT("ListView") );
	PreviewB.Load( TEXT("PreviewView") );
	ThumbnailB.Load( TEXT("ThumbnailView") );
	SphereB.Load( TEXT("Sphere") );
	CubeB.Load( TEXT("Cube") );
	CylinderB.Load( TEXT("Cylinder") );
	PlaneB.Load( TEXT("Plane") );

	ZoomST = new wxStaticText( this, -1, TEXT("View: ") );

	wxString dummy[1];
	ViewCB = new wxComboBox( this, ID_ZOOM_FACTOR, TEXT(""), wxDefaultPosition, wxSize(75,-1), 0, dummy, wxCB_READONLY );

	ViewCB->Append( TEXT("300%") );
	ViewCB->Append( TEXT("200%") );
	ViewCB->Append( TEXT("100%") );
	ViewCB->Append( TEXT("50%") );
	ViewCB->Append( TEXT("25%") );
	ViewCB->Append( TEXT("10%") );
	ViewCB->Append( TEXT("64x64") );
	ViewCB->Append( TEXT("128x128") );
	ViewCB->Append( TEXT("256x256") );
	ViewCB->Append( TEXT("512x512") );
	ViewCB->Append( TEXT("1024x1024") );
	ViewCB->SetToolTip( TEXT("Thumbnail Format") );

	ViewCB->SetSelection( 2 );

	NameFilterST = new wxStaticText( this, -1, TEXT("Filter: ") );
	NameFilterTC = new wxTextCtrl( this, ID_NAME_FILTER, TEXT(""), wxDefaultPosition, wxSize(100,-1), wxTE_LEFT | wxTE_DONTWRAP );

	// Set up the ToolBar

	AddRadioTool( ID_VIEW_LIST, TEXT(""), ListB, ListB, TEXT("List") );
	AddRadioTool( ID_VIEW_PREVIEW, TEXT(""), PreviewB, PreviewB, TEXT("Preview") );
	AddRadioTool( ID_VIEW_THUMBNAIL, TEXT(""), ThumbnailB, ThumbnailB, TEXT("Thumbnails") );
	AddSeparator();
	AddRadioTool( ID_PRIMTYPE_SPHERE, TEXT(""), SphereB, SphereB, TEXT("Sphere Primitive") );
	AddRadioTool( ID_PRIMTYPE_CUBE, TEXT(""), CubeB, CubeB, TEXT("Cube Primitive") );
	AddRadioTool( ID_PRIMTYPE_CYLINDER, TEXT(""), CylinderB, CylinderB, TEXT("Cylinder Primitive") );
	AddRadioTool( ID_PRIMTYPE_PLANE, TEXT(""), PlaneB, PlaneB, TEXT("Plane Primitive") );
	AddSeparator();
	AddControl( ZoomST );
	AddControl( ViewCB );
	AddSeparator();
	AddControl( NameFilterST );
	AddControl( NameFilterTC );

	Realize();
}

WxGBViewToolBar::~WxGBViewToolBar()
{
}

/*-----------------------------------------------------------------------------
	WxModeGeometrySelBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxModeGeometrySelBar, wxToolBar )
END_EVENT_TABLE()

WxModeGeometrySelBar::WxModeGeometrySelBar( wxWindow* InParent, wxWindowID InID )
	: wxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_NODIVIDER )
{
	// Bitmaps

	WindowB.Load( TEXT("Window") );
	ObjectB.Load( TEXT("Geom_Object") );
	PolyB.Load( TEXT("Geom_Poly") );
	EdgeB.Load( TEXT("Geom_Edge") );
	VertexB.Load( TEXT("Geom_Vertex") );

	// Set up the ToolBar

	AddCheckTool( ID_TOGGLE_WINDOW, TEXT(""), WindowB, WindowB, TEXT("Toggle Modifier Window") );
	AddSeparator();
	AddRadioTool( ID_GEOM_OBJECT, TEXT(""), ObjectB, ObjectB, TEXT("Object") );
	AddRadioTool( ID_GEOM_POLY, TEXT(""), PolyB, PolyB, TEXT("Poly") );
	AddRadioTool( ID_GEOM_EDGE, TEXT(""), EdgeB, EdgeB, TEXT("Edge") );
	AddRadioTool( ID_GEOM_VERTEX, TEXT(""), VertexB, VertexB, TEXT("Vertex") );

	Realize();
}

WxModeGeometrySelBar::~WxModeGeometrySelBar()
{
}

/*-----------------------------------------------------------------------------
	WxStaticMeshEditorBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxStaticMeshEditorBar, wxToolBar )
END_EVENT_TABLE()

WxStaticMeshEditorBar::WxStaticMeshEditorBar( wxWindow* InParent, wxWindowID InID )
	: wxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_NODIVIDER )
{
	// Bitmaps

	OpenEdgesB.Load( TEXT("OpenEdges") );
	WireframeB.Load( TEXT("Wireframe") );
	BoundsB.Load( TEXT("Bounds") );
	CollisionB.Load( TEXT("Collision") );
	LockB.Load( TEXT("Lock") );
	CameraB.Load( TEXT("Camera") );

	// Set up the ToolBar

	AddCheckTool( ID_SHOW_OPEN_EDGES, TEXT(""), OpenEdgesB, OpenEdgesB, TEXT("Show Open Edges") );
	AddCheckTool( ID_SHOW_WIREFRAME, TEXT(""), WireframeB, WireframeB, TEXT("Show Wireframe") );
	AddCheckTool( ID_SHOW_BOUNDS, TEXT(""), BoundsB, BoundsB, TEXT("Show Bounds") );
	AddCheckTool( IDMN_COLLISION, TEXT(""), CollisionB, CollisionB, TEXT("Show Collision") );
	AddSeparator();
	AddCheckTool( ID_LOCK_CAMERA, TEXT(""), LockB, LockB, TEXT("Lock Camera") );
	AddSeparator();
	AddTool( ID_SAVE_THUMBNAIL, TEXT(""), CameraB, TEXT("Save Thumbnail Angle") );

	Realize();
}

WxStaticMeshEditorBar::~WxStaticMeshEditorBar()
{
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
