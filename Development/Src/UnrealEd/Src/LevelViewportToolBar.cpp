/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "LevelViewportToolBar.h"
#include "UnTerrain.h"
#include "LayerUtils.h"
#include "DlgDensityRenderingOptions.h"

#include "InterpEditor.h"




#define EVT_CHECKBOX_RANGE(id1, id2, func) wx__DECLARE_EVT2(wxEVT_COMMAND_CHECKBOX_CLICKED, id1, id2, wxCommandEventHandler(func))

BEGIN_EVENT_TABLE( WxLevelViewportToolBar, WxToolBar )
	EVT_TOOL( IDM_REALTIME, WxLevelViewportToolBar::OnRealTime )
	EVT_UPDATE_UI( IDM_REALTIME, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )

	EVT_TOOL( IDM_MOVEUNLIT, WxLevelViewportToolBar::OnMoveUnlit )
	EVT_UPDATE_UI( IDM_MOVEUNLIT, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )

	EVT_TOOL( IDM_LevelStreamingVolumePreVis, WxLevelViewportToolBar::OnLevelStreamingVolumePreVis )
	EVT_TOOL( IDM_PostProcessVolumePreVis, WxLevelViewportToolBar::OnPostProcessVolumePreVis )
	EVT_TOOL( IDM_SQUINTBLURMODE, WxLevelViewportToolBar::OnSquintModeChange )
	EVT_TOOL( IDM_VIEWPORTLOCKED, WxLevelViewportToolBar::OnViewportLocked )

	EVT_TOOL( IDM_BRUSHWIREFRAME, WxLevelViewportToolBar::OnBrushWireframe )
	EVT_TOOL( IDM_WIREFRAME, WxLevelViewportToolBar::OnWireframe )
	EVT_TOOL( IDM_UNLIT, WxLevelViewportToolBar::OnUnlit )
	EVT_TOOL( IDM_LIT, WxLevelViewportToolBar::OnLit )
	EVT_TOOL( IDM_DETAILLIGHTING, WxLevelViewportToolBar::OnDetailLighting )
	EVT_TOOL( IDM_LIGHTINGONLY, WxLevelViewportToolBar::OnLightingOnly )
	EVT_TOOL( IDM_LIGHTCOMPLEXITY, WxLevelViewportToolBar::OnLightComplexity )
	EVT_TOOL( IDM_TEXTUREDENSITY, WxLevelViewportToolBar::OnTextureDensity )
	EVT_TOOL( IDM_SHADERCOMPLEXITY, WxLevelViewportToolBar::OnShaderComplexity )
	EVT_TOOL( IDM_LIGHTMAPDENSITY, WxLevelViewportToolBar::OnLightMapDensity )
	EVT_TOOL( IDM_LITLIGHTMAPDENSITY, WxLevelViewportToolBar::OnLitLightmapDensity )
	EVT_TOOL( IDM_REFLECTIONS, WxLevelViewportToolBar::OnReflections )

	EVT_UPDATE_UI( IDM_BRUSHWIREFRAME, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_UPDATE_UI( IDM_WIREFRAME, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_UPDATE_UI( IDM_UNLIT, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_UPDATE_UI( IDM_LIT, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_UPDATE_UI( IDM_DETAILLIGHTING, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_UPDATE_UI( IDM_LIGHTINGONLY, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_UPDATE_UI( IDM_LIGHTCOMPLEXITY, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_UPDATE_UI( IDM_TEXTUREDENSITY, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_UPDATE_UI( IDM_SHADERCOMPLEXITY, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_UPDATE_UI( IDM_LIGHTMAPDENSITY, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_UPDATE_UI( IDM_LITLIGHTMAPDENSITY, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_UPDATE_UI( IDM_REFLECTIONS, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )

	EVT_TOOL( IDM_PERSPECTIVE, WxLevelViewportToolBar::OnPerspective )
	EVT_TOOL( IDM_TOP, WxLevelViewportToolBar::OnTop )
	EVT_TOOL( IDM_FRONT, WxLevelViewportToolBar::OnFront )
	EVT_TOOL( IDM_SIDE, WxLevelViewportToolBar::OnSide )
	EVT_UPDATE_UI( IDM_PERSPECTIVE, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_UPDATE_UI( IDM_TOP, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_UPDATE_UI( IDM_FRONT, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_UPDATE_UI( IDM_SIDE, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )

	EVT_TOOL( IDM_TearOffNewFloatingViewport, WxLevelViewportToolBar::OnTearOffNewFloatingViewport )

	EVT_MENU_RANGE (IDM_ResizeToDevice_Start, IDM_ResizeToDevice_End, WxLevelViewportToolBar::OnResizeToDevice )

	EVT_TOOL( IDM_AllowMatineePreview, WxLevelViewportToolBar::OnToggleAllowMatineePreview )
	EVT_UPDATE_UI( IDM_AllowMatineePreview, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_TOOL( IDM_LOCK_SELECTED_TO_CAMERA, WxLevelViewportToolBar::OnLockSelectedToCamera )
	EVT_BUTTON( IDM_LevelViewportToolbar_PlayInViewport, WxLevelViewportToolBar::OnPlayInViewport )
	EVT_UPDATE_UI( IDM_LevelViewportToolbar_PlayInViewport, WxLevelViewportToolBar::UpdateUI_OnPlayInViewport )

	EVT_BUTTON( IDM_VIEWPORT_MATINEE_TOGGLE_RECORD_TRACKS, WxLevelViewportToolBar::OnToggleRecordInterpValues )
	EVT_UPDATE_UI( IDM_VIEWPORT_MATINEE_TOGGLE_RECORD_TRACKS, WxLevelViewportToolBar::UpdateUI_RecordInterpValues )
	EVT_COMBOBOX( IDM_VIEWPORT_MATINEE_RECORD_MODE, WxLevelViewportToolBar::OnRecordModeChange)

	EVT_MENU( IDM_MakeOcclusionParent, WxLevelViewportToolBar::OnMakeParentViewport )
	EVT_UPDATE_UI( IDM_MakeOcclusionParent, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )
	EVT_MENU( IDM_ViewportOptions_AddCameraActor, WxLevelViewportToolBar::OnAddCameraActor )

	EVT_TOOL( IDM_LevelViewportOptionsMenu, WxLevelViewportToolBar::OnOptionsMenu )
	EVT_TOOL_RCLICKED( IDM_LevelViewportOptionsMenu, WxLevelViewportToolBar::OnShowFlagsShortcut )
	EVT_MENU( IDM_VIEWPORT_SHOW_FLAGS_QUICK_MENU, WxLevelViewportToolBar::OnShowFlagsShortcut )
	EVT_MENU( IDM_VIEWPORT_SHOW_DEFAULTS, WxLevelViewportToolBar::OnShowDefaults )

	EVT_MENU( IDM_ViewportOptions_ShowFPS, WxLevelViewportToolBar::OnShowFPS )
	EVT_UPDATE_UI( IDM_ViewportOptions_ShowFPS, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )

	EVT_MENU( IDM_ViewportOptions_ShowStats, WxLevelViewportToolBar::OnShowStats )
	EVT_UPDATE_UI( IDM_ViewportOptions_ShowStats, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )

	EVT_MENU_RANGE( IDM_BookmarkSetLocation_START, IDM_BookmarkSetLocation_END, WxLevelViewportToolBar::OnSetBookmark )
	EVT_MENU_RANGE( IDM_BookmarkJumpToLocation_START, IDM_BookmarkJumpToLocation_END, WxLevelViewportToolBar::OnJumpToBookmark )
	EVT_MENU_RANGE( IDM_BookmarkJumpToRestoreLocation_START, IDM_BookmarkJumpToRestoreLocation_END, WxLevelViewportToolBar::OnJumpToRestoreBookmark )

	EVT_MENU( IDM_VIEWPORT_SHOW_GAMEVIEW, WxLevelViewportToolBar::OnGameView )
	EVT_UPDATE_UI( IDM_VIEWPORT_SHOW_GAMEVIEW, WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu )

	EVT_MENU( IDM_VIEWPORT_SHOW_COLLISION_ZEROEXTENT, WxLevelViewportToolBar::OnChangeCollisionMode )
	EVT_MENU( IDM_VIEWPORT_SHOW_COLLISION_NONZEROEXTENT, WxLevelViewportToolBar::OnChangeCollisionMode )
	EVT_MENU( IDM_VIEWPORT_SHOW_COLLISION_RIGIDBODY, WxLevelViewportToolBar::OnChangeCollisionMode )
	EVT_MENU( IDM_VIEWPORT_SHOW_COLLISION_NONE, WxLevelViewportToolBar::OnChangeCollisionMode )
	EVT_MENU( IDM_PerViewLayers_HideAll, WxLevelViewportToolBar::OnPerViewLayersHideAll )
	EVT_MENU( IDM_PerViewLayers_HideNone, WxLevelViewportToolBar::OnPerViewLayersHideNone )
	EVT_MENU( IDM_VIEWPORT_TYPE_CYCLE_BUTTON, WxLevelViewportToolBar::OnViewportTypeLeftClick )
	EVT_TOOL_RCLICKED( IDM_VIEWPORT_TYPE_CYCLE_BUTTON, WxLevelViewportToolBar::OnViewportTypeRightClick )
	EVT_MENU( ID_CAMSPEED_CYCLE_BUTTON, WxLevelViewportToolBar::OnCamSpeedButtonLeftClick )
	EVT_TOOL_RCLICKED( ID_CAMSPEED_CYCLE_BUTTON, WxLevelViewportToolBar::OnCamSpeedButtonRightClick )
	EVT_COMMAND_RANGE( ID_CAMSPEED_SLOW, ID_CAMSPEED_VERYFAST, EVT_CAMSPEED_UPDATE, WxLevelViewportToolBar::OnCamSpeedUpdateEvent )
	EVT_MENU_RANGE (IDM_PerViewLayers_Start, IDM_PerViewLayers_End, WxLevelViewportToolBar::OnTogglePerViewLayer )
	EVT_MENU_RANGE (IDM_VolumeClasses_START, IDM_VolumeClasses_END, WxLevelViewportToolBar::OnChangeVolumeVisibility )
	EVT_MENU( IDM_VolumeActorVisibilityShowAll, WxLevelViewportToolBar::OnToggleAllVolumeActors )
	EVT_MENU( IDM_VolumeActorVisibilityHideAll, WxLevelViewportToolBar::OnToggleAllVolumeActors )
	EVT_MENU( IDM_SpriteComponentVisibilityShowAll, WxLevelViewportToolBar::OnChangeAllSpriteCategoryVisibility )
	EVT_MENU( IDM_SpriteComponentVisibilityHideAll, WxLevelViewportToolBar::OnChangeAllSpriteCategoryVisibility )
	EVT_MENU_RANGE( IDM_SpriteComponentCategories_START, IDM_SpriteComponentCategories_END, WxLevelViewportToolBar::OnChangeSpriteCategoryVisibility )
	EVT_UPDATE_UI_RANGE( IDM_SpriteComponentCategories_START, IDM_SpriteComponentCategories_END, WxLevelViewportToolBar::UpdateUI_SpriteCategoryMenu )
	EVT_MENU_RANGE( IDM_VIEWPORT_SHOWFLAGS_MENU_START, IDM_VIEWPORT_SHOWFLAGS_MENU_END, WxLevelViewportToolBar::OnToggleShowFlagMenu )
	EVT_UPDATE_UI_RANGE( IDM_VIEWPORT_SHOWFLAGS_MENU_START, IDM_VIEWPORT_SHOWFLAGS_MENU_END, WxLevelViewportToolBar::UpdateUI_ShowFlagMenu )
END_EVENT_TABLE()

BEGIN_EVENT_TABLE( WxShowFlagsDialog, wxDialog )
	EVT_CHECKBOX_RANGE( IDM_VIEWPORT_SHOWFLAGS_WINDOW_START, IDM_VIEWPORT_SHOWFLAGS_WINDOW_END, WxShowFlagsDialog::OnToggleShowFlagWindow )
	EVT_BUTTON( IDM_VIEWPORT_SHOW_DEFAULTS, WxShowFlagsDialog::OnShowDefaults )
END_EVENT_TABLE()

/** Converts a Resource ID  into its equivalent ELevelViewport type.
 *
 * @param ViewportID	The resource ID for the viewport type
 * @return	The ELevelViewport type for the passed in resource ID.
 */
static ELevelViewportType ViewportTypeFromResourceID( INT ViewportID )
{
	ELevelViewportType CurrentViewportType;
	switch (ViewportID)
	{
	case IDM_TOP:
		CurrentViewportType = LVT_OrthoXY;
		break;
	case IDM_SIDE:
		CurrentViewportType = LVT_OrthoXZ;
		break;
	case IDM_FRONT:
		CurrentViewportType = LVT_OrthoYZ;
		break;
	case IDM_PERSPECTIVE:
		CurrentViewportType = LVT_Perspective;
		break;
	default:
		CurrentViewportType = LVT_None;
		break;
	}

	return CurrentViewportType;
}

/** Converts a ELevelViewportType into its equivalent resource ID type.
 *
 * @param InType	The ELevelViewport type to convert
 * @return	The resource ID  for the passed in type.
 */
static INT ResourceIDFromViewportType( ELevelViewportType InType )
{
	INT CurrentViewportType;
	switch (InType)
	{
	case LVT_OrthoXY:
		CurrentViewportType = IDM_TOP;
		break;
	case LVT_OrthoXZ:
		CurrentViewportType = IDM_SIDE;
		break;
	case LVT_OrthoYZ:
		CurrentViewportType = IDM_FRONT;
		break;
	case LVT_Perspective:
		CurrentViewportType = IDM_PERSPECTIVE;
		break;
	default:
		CurrentViewportType = -1;
		break;
	}

	return CurrentViewportType;
}

WxLevelViewportToolBar::WxLevelViewportToolBar( wxWindow* InParent, wxWindowID InID, FEditorLevelViewportClient* InViewportClient )
	: WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
	,	LastCameraSpeedID( ID_CAMSPEED_START )
	,	ViewportClient( InViewportClient )
	,	MatineeWindow(NULL)
{
	// Bitmaps
	LockSelectedToCameraB.Load( TEXT("LVT_LockSelectedToCamera") );
	LockViewportB.Load( TEXT("LVT_LockViewport") );
	RealTimeB.Load( TEXT("Realtime") );
	StreamingPrevisB.Load( TEXT("LVT_StreamingPrevis") );
	PostProcessPrevisB.Load( TEXT("LVT_PPPrevis") );
	ShowFlagsB.Load( TEXT("Showflags") );
	GameViewB.Load( TEXT("LVT_GameView" ) );
	SquintButtonB.Load( TEXT("Squint") );
	CamSpeedsB[0].Load( TEXT("CamSlow") );
	CamSpeedsB[1].Load( TEXT("CamNormal") );
	CamSpeedsB[2].Load( TEXT("CamFast") );
	CamSpeedsB[3].Load( TEXT("CamVeryFast") );

	//SetToolBitmapSize( wxSize( 16, 16 ) );

	BrushWireframeB.Load( TEXT("LVT_BrushWire") );
	WireframeB.Load( TEXT("LVT_Wire") );
	UnlitB.Load( TEXT("LVT_Unlit") );
	LitB.Load( TEXT("LVT_Lit") );
	DetailLightingB.Load( TEXT("LVT_DetailLighting") );
	LightingOnlyB.Load( TEXT("LVT_LightingOnly") );
	LightComplexityB.Load( TEXT("LVT_LightingComplexity") );
	TextureDensityB.Load( TEXT("LVT_TextureDensity") );
	ShaderComplexityB.Load( TEXT("LVT_ShaderComplexity") );
	LightMapDensityB.Load( TEXT("LVT_LightMapDensity" ) );
	LitLightmapDensityB.Load( TEXT("LVT_LitLightmapDensity") );

	// Load the viewport type bitmaps
	ViewportTypesB[IDM_PERSPECTIVE - IDM_VIEWPORT_TYPE_START].Load( TEXT("LVT_Perspective") );
	ViewportTypesB[IDM_TOP - IDM_VIEWPORT_TYPE_START].Load( TEXT("LVT_Top") );
	ViewportTypesB[IDM_FRONT - IDM_VIEWPORT_TYPE_START].Load( TEXT("LVT_Front") );
	ViewportTypesB[IDM_SIDE - IDM_VIEWPORT_TYPE_START].Load( TEXT("LVT_Side") );
	

	TearOffNewFloatingViewportB.Load( TEXT( "LVT_TearOffNewFloatingViewport" ) );

	// Play-In-Viewport button
	PlayInViewportStartB.Load( TEXT("PlayInViewportPlay.png") );
	PlayInViewportStopB.Load( TEXT("PlayInEditorStop.png") );

	// Set up the ToolBar
	//AddSeparator();
	AddTool( IDM_LevelViewportOptionsMenu, TEXT(""), ShowFlagsB, *LocalizeUnrealEd("LevelViewportToolBar_OptionsButton_ToolTip") );
	AddSeparator();
	AddTool( IDM_VIEWPORT_TYPE_CYCLE_BUTTON, TEXT(""), ViewportTypesB[ResourceIDFromViewportType(ViewportClient->ViewportType) - IDM_VIEWPORT_TYPE_START], *LocalizeUnrealEd("ToolTip_ViewportTypeSetting") );
	AddCheckTool( IDM_REALTIME, TEXT(""), RealTimeB, RealTimeB, *LocalizeUnrealEd("LevelViewportToolBar_RealTime") );
	AddSeparator();
	AddCheckTool( IDM_BRUSHWIREFRAME, TEXT(""), BrushWireframeB, BrushWireframeB, *LocalizeUnrealEd("LevelViewportToolbar_BrushWireframe") );
	AddCheckTool( IDM_WIREFRAME, TEXT(""), WireframeB, WireframeB, *LocalizeUnrealEd("LevelViewportToolbar_Wireframe") );
	AddCheckTool( IDM_UNLIT, TEXT(""), UnlitB, UnlitB, *LocalizeUnrealEd("LevelViewportToolbar_Unlit") );
	AddCheckTool( IDM_LIT, TEXT(""), LitB, LitB, *LocalizeUnrealEd("LevelViewportToolbar_Lit") );
	AddCheckTool( IDM_DETAILLIGHTING, TEXT(""), DetailLightingB, DetailLightingB, *LocalizeUnrealEd("LevelViewportToolbar_DetailLighting") );
	AddCheckTool( IDM_LIGHTINGONLY, TEXT(""), LightingOnlyB, LightingOnlyB, *LocalizeUnrealEd("LevelViewportToolbar_LightingOnly") );
	AddCheckTool( IDM_LIGHTCOMPLEXITY, TEXT(""), LightComplexityB, LightComplexityB, *LocalizeUnrealEd("LevelViewportToolbar_LightComplexity") );
	AddCheckTool( IDM_TEXTUREDENSITY, TEXT(""), TextureDensityB, TextureDensityB, *LocalizeUnrealEd("LevelViewportToolbar_TextureDensity") );
	AddCheckTool( IDM_SHADERCOMPLEXITY, TEXT(""), ShaderComplexityB, ShaderComplexityB, *LocalizeUnrealEd("LevelViewportToolbar_ShaderComplexity") );
	AddCheckTool( IDM_LIGHTMAPDENSITY, TEXT(""), LightMapDensityB, LightMapDensityB, *LocalizeUnrealEd("LevelViewportToolbar_LightMapDensity") );
	AddCheckTool( IDM_LITLIGHTMAPDENSITY, TEXT(""), LitLightmapDensityB, LitLightmapDensityB, *LocalizeUnrealEd("LevelViewportToolbar_LitLightmapDensity") );
	AddSeparator();
	AddCheckTool( IDM_VIEWPORT_SHOW_GAMEVIEW, TEXT(""), GameViewB, GameViewB, *LocalizeUnrealEd("LevelViewportToolBar_ToggleGameView_ToolTip") );
	AddSeparator();
	AddCheckTool( IDM_VIEWPORTLOCKED, TEXT(""), LockViewportB, LockViewportB, *LocalizeUnrealEd("ToolTip_ViewportLocked") );
	AddCheckTool( IDM_LOCK_SELECTED_TO_CAMERA, TEXT(""), LockSelectedToCameraB, LockSelectedToCameraB, *LocalizeUnrealEd("ToolTip_159") );
	AddSeparator();
	AddCheckTool( IDM_LevelStreamingVolumePreVis, TEXT(""), StreamingPrevisB, StreamingPrevisB, *LocalizeUnrealEd("ToolTip_LevelStreamingVolumePrevis") );
	AddCheckTool( IDM_PostProcessVolumePreVis, TEXT(""), PostProcessPrevisB, PostProcessPrevisB, *LocalizeUnrealEd("ToolTip_PostProcessVolumePrevis") );
#if !UDK
	AddCheckTool( IDM_SQUINTBLURMODE, TEXT(""), SquintButtonB, SquintButtonB, *LocalizeUnrealEd("TollTip_SquintMode") );
#endif
	AddSeparator();
	
	AddTool( ID_CAMSPEED_CYCLE_BUTTON, TEXT(""), CamSpeedsB[LastCameraSpeedID - ID_CAMSPEED_START], *LocalizeUnrealEd("ToolTip_CameraSpeedSetting") );
	AddSeparator();
	
	// Only add the PIV button if its available for this type of viewport
	if( InViewportClient->IsPlayInViewportAllowed() )
	{
		// Set up the play in viewport button
		PlayInViewportButton = new WxBitmapStateButton( this, this, IDM_LevelViewportToolbar_PlayInViewport, wxDefaultPosition, wxSize( 35, 20 ), FALSE, FALSE );
		PlayInViewportButton->AddState( PIV_Play, &PlayInViewportStartB );
		PlayInViewportButton->AddState( PIV_Stop, &PlayInViewportStopB );
		PlayInViewportButton->SetToolTip( *LocalizeUnrealEd("LevelViewportToolbar_PlayInViewportStart") );
		PlayInViewportButton->SetCurrentState( PIV_Play );
		AddControl( PlayInViewportButton );
	}

	if( !ViewportClient->IsFloatingViewport() )
	{
		AddSeparator();

		// Only add the 'tear off' button for non-floating viewports
		AddTool( IDM_TearOffNewFloatingViewport, TEXT( "" ), TearOffNewFloatingViewportB, *LocalizeUnrealEd( "ViewportToolbar_TearOffNewFloatingViewport" ) );
	}

	ShowFlagsDialog = new WxShowFlagsDialog( NULL, wxID_ANY, TEXT( "SHOW FLAGS" ), ViewportClient );

	Realize();
	// UpdateUI must be called *after* Realize.
	UpdateUI();
}

WxLevelViewportToolBar::~WxLevelViewportToolBar()
{
	delete ShowFlagsDialog;
	ShowFlagsDialog = NULL;
}

void WxLevelViewportToolBar::AppendMatineeRecordOptions(WxInterpEd* InInterpEd)
{
	check(InInterpEd);
	check(MatineeWindow==NULL);

	//Save matinee window for interface
	MatineeWindow = InInterpEd;

	//add record toggle button
	MatineeStartRecordB.Load( TEXT("Record.png") );
	MatineeCancelRecordB.Load( TEXT("Stop.png") );

	WxBitmapCheckButton* RecordModeButton = new WxBitmapCheckButton(this, this, IDM_VIEWPORT_MATINEE_TOGGLE_RECORD_TRACKS, &MatineeStartRecordB, &MatineeCancelRecordB, wxDefaultPosition, wxDefaultSize);
	RecordModeButton->SetToolTip(*LocalizeUnrealEd("InterpEd_ToggleMatineeRecord_Tooltip"));
	AddControl( RecordModeButton );

	//add number of "takes" drop down
	WxComboBox* RecordModeCombo = new WxComboBox( this, IDM_VIEWPORT_MATINEE_RECORD_MODE, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY );
	RecordModeCombo->Append( *LocalizeUnrealEd("InterpEd_RecordMode_NewCameraMode"));
	RecordModeCombo->Append( *LocalizeUnrealEd("InterpEd_RecordMode_NewCameraAttachedMode"));
	RecordModeCombo->Append( *LocalizeUnrealEd("InterpEd_RecordMode_DuplicateTracksMode"));
	RecordModeCombo->Append( *LocalizeUnrealEd("InterpEd_RecordMode_ReplaceTracksMode"));
	RecordModeCombo->SetSelection( 0 );
	RecordModeCombo->SetToolTip( *LocalizeUnrealEd("InterpEd_MatineeRecordMode_Tooltip") );

	AddSeparator();
	AddControl( RecordModeCombo );

	//since we're adding new options we have to re-realize the toolbar
	Realize();
	// UpdateUI must be called *after* Realize.
	UpdateUI();
}


void WxLevelViewportToolBar::UpdateUI()
{
	ToggleTool( IDM_VIEWPORT_SHOW_GAMEVIEW, GetInGameViewMode() ? true : false );
	ToggleTool( IDM_REALTIME, ViewportClient->IsRealtime()==TRUE );

	UpdateToolBarButtonEnabledStates();

	INT ViewModeID = -1;
	EShowFlags ViewModeShowFlags = ViewportClient->ShowFlags & SHOW_ViewMode_Mask;

	if( ViewModeShowFlags == SHOW_ViewMode_BrushWireframe )
	{
		ViewModeID = 0;
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_Wireframe )
	{
		ViewModeID = 1;
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_Unlit )
	{
		ViewModeID = 2;
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_Lit)
	{
		if (ViewportClient->bOverrideDiffuseAndSpecular)
		{
			ViewModeID = 4;
		}
		else
		{
			if (ViewportClient->bShowReflectionsOnly)
			{
				ViewModeID = 11;
			}
			else
			{
				ViewModeID = 3;
			}
		}
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_LightingOnly)
	{
		ViewModeID = 5;
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_LightComplexity)
	{
		ViewModeID = 6;
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_TextureDensity)
	{
		ViewModeID = 7;
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_ShaderComplexity)
	{
		ViewModeID = 8;
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_LightMapDensity)
	{
		ViewModeID = 9;
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_LitLightmapDensity)
	{
		ViewModeID = 10;
	}
	SetViewModeUI( ViewModeID );

	// Update the show flag window too (incase the checkbox needs to update)
	ShowFlagsDialog->UpdateUI();
}


/** Updates UI state for the various viewport option menu items */
void WxLevelViewportToolBar::UpdateUI_ViewportOptionsMenu( wxUpdateUIEvent& In )
{
	const bool bIsPerspectiveView = ViewportClient->ViewportType == LVT_Perspective;
	switch( In.GetId() )
	{
		case IDM_REALTIME:
			In.Check( ViewportClient->IsRealtime() ? true : false );
			break;

		// ...

		case IDM_BRUSHWIREFRAME: 
			In.Check( ( ViewportClient->ShowFlags & SHOW_ViewMode_Mask ) == SHOW_ViewMode_BrushWireframe );
			break;

		case IDM_WIREFRAME: 
			In.Check( ( ViewportClient->ShowFlags & SHOW_ViewMode_Mask ) == SHOW_ViewMode_Wireframe );
			break;

		case IDM_UNLIT: 
			In.Check( ( ViewportClient->ShowFlags & SHOW_ViewMode_Mask ) == SHOW_ViewMode_Unlit );
			break;

		case IDM_LIT: 
			In.Check( ( ViewportClient->ShowFlags & SHOW_ViewMode_Mask ) == SHOW_ViewMode_Lit && !ViewportClient->bOverrideDiffuseAndSpecular );
			break;

		case IDM_DETAILLIGHTING: 
			In.Check( ( ViewportClient->ShowFlags & SHOW_ViewMode_Mask ) == SHOW_ViewMode_Lit && ViewportClient->bOverrideDiffuseAndSpecular );
			break;

		case IDM_LIGHTINGONLY: 
			In.Check( ( ViewportClient->ShowFlags & SHOW_ViewMode_Mask ) == SHOW_ViewMode_LightingOnly );
			break;

		case IDM_LIGHTCOMPLEXITY: 
			In.Check( ( ViewportClient->ShowFlags & SHOW_ViewMode_Mask ) == SHOW_ViewMode_LightComplexity );
			break;

		case IDM_TEXTUREDENSITY: 
			In.Check( ( ViewportClient->ShowFlags & SHOW_ViewMode_Mask ) == SHOW_ViewMode_TextureDensity );
			break;

		case IDM_SHADERCOMPLEXITY: 
			In.Check( ( ViewportClient->ShowFlags & SHOW_ViewMode_Mask ) == SHOW_ViewMode_ShaderComplexity );
			break;

		case IDM_LIGHTMAPDENSITY: 
			In.Check( ( ViewportClient->ShowFlags & SHOW_ViewMode_Mask ) == SHOW_ViewMode_LightMapDensity );
			break;

		case IDM_LITLIGHTMAPDENSITY:
			In.Check( (ViewportClient->ShowFlags & SHOW_ViewMode_Mask) == SHOW_ViewMode_LitLightmapDensity );
			break;

		case IDM_REFLECTIONS:
			In.Check( (ViewportClient->ShowFlags & SHOW_ViewMode_Mask) == SHOW_ViewMode_Lit && ViewportClient->bShowReflectionsOnly );
			break;

		// ...

		case IDM_PERSPECTIVE:
			In.Check( ViewportClient->ViewportType == LVT_Perspective );
			break;

		case IDM_TOP:
			In.Check( ViewportClient->ViewportType == LVT_OrthoXY );
			break;

		case IDM_FRONT:
			In.Check( ViewportClient->ViewportType == LVT_OrthoYZ );
			break;

		case IDM_SIDE:
			In.Check( ViewportClient->ViewportType == LVT_OrthoXZ );
			break;

		// ...

		case IDM_AllowMatineePreview:
			In.Enable( bIsPerspectiveView );
			In.Check( bIsPerspectiveView && ViewportClient->AllowMatineePreview() );
			break;

		case IDM_MOVEUNLIT:
			In.Check( ViewportClient->bMoveUnlit == TRUE );
			break;

		case IDM_VIEWPORT_SHOW_GAMEVIEW:
			In.Check( GetInGameViewMode() ? true : false );
			break;

		case IDM_MakeOcclusionParent:
			In.Enable( bIsPerspectiveView );
			In.Check( ViewportClient->ViewState->IsViewParent()==TRUE );
			break;

		case IDM_ViewportOptions_AddCameraActor:
			break;

		// ...

		case IDM_ViewportOptions_ShowFPS:
			In.Check( ViewportClient->ShouldShowFPS() == TRUE );
			break;

		case IDM_ViewportOptions_ShowStats:
			In.Check( ViewportClient->ShouldShowStats() == TRUE );
			break;
	}
}



void WxLevelViewportToolBar::OnSquintModeChange( wxCommandEvent& In )
{
	ViewportClient->SetSquintMode( In.IsChecked() );
	ViewportClient->Viewport->Invalidate();
}

void WxLevelViewportToolBar::OnRealTime( wxCommandEvent& In )
{
	ViewportClient->SetRealtime( In.IsChecked() );
	ViewportClient->Invalidate( FALSE );
}


void WxLevelViewportToolBar::OnMoveUnlit( wxCommandEvent& In )
{
	ViewportClient->bMoveUnlit = In.IsChecked();
	ViewportClient->Invalidate( FALSE );
}



void WxLevelViewportToolBar::OnToggleAllowMatineePreview( wxCommandEvent& In )
{
	// Reset the FOV of Viewport for cases where we have been previewing the matinee with a changing FOV
	ViewportClient->ViewFOV = In.IsChecked() ? ViewportClient->ViewFOV : GEditor->FOVAngle;

	ViewportClient->SetAllowMatineePreview( In.IsChecked() );
	ViewportClient->Invalidate( FALSE );
}

void WxLevelViewportToolBar::OnResizeToDevice( wxCommandEvent& In )
{
	INT InId = In.GetId();
	check( InId >= IDM_ResizeToDevice_Start && InId < IDM_ResizeToDevice_End );

	INT NewDeviceID = (InId - IDM_ResizeToDevice_Start) + BPD_FIRST_VIEWPORT_RESIZE_DEVICE;

	INT NewWidth, NewHeight;
	extern void GetDeviceResolution(const INT DeviceIndex, INT& OutWidth, INT& OutHeight);
	GetDeviceResolution(NewDeviceID, NewWidth, NewHeight);


	if ( GApp && GApp->EditorFrame && GApp->EditorFrame->ViewportConfigData )
	{
		FViewportConfig_Data *ViewportConfig = GApp->EditorFrame->ViewportConfigData;
		if (ViewportConfig->IsViewportMaximized())
		{
			ViewportConfig->ToggleMaximize( ViewportClient->Viewport );
		}
		ViewportConfig->ResizeViewportToSize(ViewportClient->Viewport, NewWidth, NewHeight);
		ViewportClient->Invalidate();
	}
}


/**Callback when toggling recording of matinee camera movement*/
void WxLevelViewportToolBar::OnToggleRecordInterpValues( wxCommandEvent& In )
{
	MatineeWindow->ToggleRecordInterpValues();
}

/**UI Adjustment for recording of matinee camera movement*/
void WxLevelViewportToolBar::UpdateUI_RecordInterpValues (wxUpdateUIEvent& In)
{
	wxObject* EventObject = In.GetEventObject();
	WxBitmapCheckButton* CheckButton = wxDynamicCast(EventObject, WxBitmapCheckButton);
	if (CheckButton)
	{
		//the standard logic (ON) is for scrict intersection
		INT TestState = MatineeWindow->IsRecordingInterpValues() ? WxBitmapCheckButton::STATE_On : WxBitmapCheckButton::STATE_Off;
		if (TestState != CheckButton->GetCurrentState()->ID)
		{
			CheckButton->SetCurrentState(TestState);
		}
	}
	In.Check( MatineeWindow->IsRecordingInterpValues() ? true : false );
}

/**Callback when changing the number of camera takes that will be used when sampling the camera for matinee*/
void WxLevelViewportToolBar::OnRecordModeChange ( wxCommandEvent& In )
{
	MatineeWindow->SetRecordMode(In.GetInt());
}

/** Called from the window event handler to launch Play-In-Editor for this viewport */
void WxLevelViewportToolBar::OnPlayInViewport( wxCommandEvent& In )
{
	// If there isn't already a PIE session in progress, start a new one in the viewport
	if ( PlayInViewportButton->GetCurrentState()->ID == PIV_Play )
	{
		if ( GApp && GApp->EditorFrame && GApp->EditorFrame->ViewportConfigData )
		{
			FVector* StartLocation = NULL;
			FRotator* StartRotation = NULL;
// jmarshall
/*
			// If this is a perspective viewport, then we'll Play From Here
			if( ViewportClient->ViewportType == LVT_Perspective )
			{
				// Start PIE from the camera's location and orientation!
				StartLocation = &ViewportClient->ViewLocation;
				StartRotation = &ViewportClient->ViewRotation;
			}
*/
// jmarshall end
			// Figure out which viewport index we are
			INT MyViewportIndex = -1;
			for( INT CurViewportIndex = 0; CurViewportIndex < GApp->EditorFrame->ViewportConfigData->GetViewportCount(); ++CurViewportIndex )
			{
				FVCD_Viewport& CurViewport = GApp->EditorFrame->ViewportConfigData->AccessViewport( CurViewportIndex );
				if( CurViewport.bEnabled && CurViewport.ViewportWindow->Viewport == ViewportClient->Viewport )
				{
					MyViewportIndex = CurViewportIndex;
					break;
				}
			}

			// Queue a PIE session for this viewport
			GUnrealEd->PlayMap( StartLocation, StartRotation, -1, MyViewportIndex );
		}
	}
	// Stop the current PIE session
	else
	{
		GUnrealEd->EndPlayMap();
	}
}

/** Update the UI for the play in viewport button */
void WxLevelViewportToolBar::UpdateUI_OnPlayInViewport( wxUpdateUIEvent& In )
{
	// If the state of PIE is different from what the PIE button currently shows, update the state and tooltip of the button
	const INT CurState = ( GEditor->PlayWorld != NULL ) ? PIV_Stop : PIV_Play;
	if ( PlayInViewportButton->GetCurrentState()->ID != CurState )
	{
		PlayInViewportButton->SetCurrentState( CurState );
		FString NewToolTip = ( CurState == PIV_Play ) ? LocalizeUnrealEd("LevelViewportToolbar_PlayInViewportStart") : LocalizeUnrealEd("LevelViewportToolbar_PlayInViewportStop");
		PlayInViewportButton->SetToolTip( *NewToolTip );
	}
}



void WxLevelViewportToolBar::OnLevelStreamingVolumePreVis( wxCommandEvent& In )
{
	// Level streaming volume previews is only possible with perspective level viewports.
	check( ViewportClient->IsPerspective() );
	ViewportClient->bLevelStreamingVolumePrevis = In.IsChecked();
	
	// Redraw all viewports, as streaming volume previs draws a camera actor in the ortho viewports as well
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

void WxLevelViewportToolBar::OnPostProcessVolumePreVis( wxCommandEvent& In )
{
	// Post process volume previews is only possible with perspective level viewports.
	check( ViewportClient->IsPerspective() );
	ViewportClient->bPostProcessVolumePrevis = In.IsChecked();
	ViewportClient->Invalidate( FALSE );
}

void WxLevelViewportToolBar::OnViewportLocked( wxCommandEvent& In )
{
	ViewportClient->bViewportLocked = In.IsChecked();
}

void WxLevelViewportToolBar::SetViewModeUI(INT ViewModeID)
{
	ToggleTool( IDM_BRUSHWIREFRAME, false );
	ToggleTool( IDM_WIREFRAME, false );
	ToggleTool( IDM_UNLIT, false );
	ToggleTool( IDM_LIT, false );
	ToggleTool( IDM_DETAILLIGHTING, false );
	ToggleTool( IDM_LIGHTINGONLY, false );
	ToggleTool( IDM_LIGHTCOMPLEXITY, false );
	ToggleTool( IDM_TEXTUREDENSITY, false );
	ToggleTool( IDM_SHADERCOMPLEXITY, false );
	ToggleTool( IDM_LIGHTMAPDENSITY, false );
	ToggleTool( IDM_LITLIGHTMAPDENSITY, false );
	ToggleTool( IDM_REFLECTIONS, false );
	ViewportClient->bOverrideDiffuseAndSpecular = FALSE;
	ViewportClient->bShowReflectionsOnly = FALSE;
	switch( ViewModeID )
	{
	case 0:		ToggleTool( IDM_BRUSHWIREFRAME, true );		break;
	case 1:		ToggleTool( IDM_WIREFRAME, true );			break;
	case 2:		ToggleTool( IDM_UNLIT, true );				break;
	case 3:		ToggleTool( IDM_LIT, true );				break;
	case 4:		ToggleTool( IDM_DETAILLIGHTING, true );		
		ViewportClient->bOverrideDiffuseAndSpecular = TRUE;
		break;
	case 5:		ToggleTool( IDM_LIGHTINGONLY, true );		break;
	case 6:		ToggleTool( IDM_LIGHTCOMPLEXITY, true );	break;
	case 7:		ToggleTool( IDM_TEXTUREDENSITY, true );		break;
	case 8:		ToggleTool( IDM_SHADERCOMPLEXITY, true );	break;
	case 9:		ToggleTool( IDM_LIGHTMAPDENSITY, true );	break;
	case 10:	ToggleTool( IDM_LITLIGHTMAPDENSITY, true );	break;
	case 11:	ToggleTool( IDM_REFLECTIONS, true );
		ViewportClient->bShowReflectionsOnly = TRUE;
		break;
	default:
		break;
	}
}

void WxLevelViewportToolBar::UpdateToolBarButtonEnabledStates()
{
	// Level streaming and post process volume previs is only possible with perspective level viewports.
	const bool bIsPerspectiveView = ViewportClient->ViewportType == LVT_Perspective;
	EnableTool( IDM_LevelStreamingVolumePreVis, bIsPerspectiveView );
	EnableTool( IDM_PostProcessVolumePreVis, bIsPerspectiveView );

	SetToolNormalBitmap( IDM_VIEWPORT_TYPE_CYCLE_BUTTON, ViewportTypesB[ ResourceIDFromViewportType( ViewportClient->ViewportType ) - IDM_VIEWPORT_TYPE_START ] );
	if ( bIsPerspectiveView )
	{
		ToggleTool( IDM_LevelStreamingVolumePreVis, ViewportClient->bLevelStreamingVolumePrevis==TRUE );
		ToggleTool( IDM_PostProcessVolumePreVis, ViewportClient->bPostProcessVolumePrevis==TRUE );
	}
	WxLevelViewportWindow* LevelViewportWindow = static_cast<WxLevelViewportWindow*>(ViewportClient);
	LevelViewportWindow->ResizeToolBar();
}

void WxLevelViewportToolBar::OnBrushWireframe( wxCommandEvent& In )
{
	ViewportClient->ShowFlags &= ~SHOW_ViewMode_Mask;
	ViewportClient->ShowFlags |= SHOW_ViewMode_BrushWireframe;
	SetViewModeUI( 0 );
	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnWireframe( wxCommandEvent& In )
{
	ViewportClient->ShowFlags &= ~SHOW_ViewMode_Mask;
	ViewportClient->ShowFlags |= SHOW_ViewMode_Wireframe;
	SetViewModeUI( 1 );
	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnUnlit( wxCommandEvent& In )
{
	ViewportClient->ShowFlags &= ~SHOW_ViewMode_Mask;
	ViewportClient->ShowFlags |= SHOW_ViewMode_Unlit;
	SetViewModeUI( 2 );
	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnLit( wxCommandEvent& In )
{
	ViewportClient->ShowFlags &= ~SHOW_ViewMode_Mask;
	ViewportClient->ShowFlags |= SHOW_ViewMode_Lit;
	SetViewModeUI( 3 );
	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnDetailLighting( wxCommandEvent& In )
{
	ViewportClient->ShowFlags &= ~SHOW_ViewMode_Mask;
	ViewportClient->ShowFlags |= SHOW_ViewMode_Lit;
	SetViewModeUI( 4 );
	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnLightingOnly( wxCommandEvent& In )
{
	ViewportClient->ShowFlags &= ~SHOW_ViewMode_Mask;
	ViewportClient->ShowFlags |= SHOW_ViewMode_LightingOnly;
	SetViewModeUI( 5 );
	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnLightComplexity( wxCommandEvent& In )
{
	ViewportClient->ShowFlags &= ~SHOW_ViewMode_Mask;
	ViewportClient->ShowFlags |= SHOW_ViewMode_LightComplexity;
	SetViewModeUI( 6 );
	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnTextureDensity( wxCommandEvent& In )
{
	ViewportClient->ShowFlags &= ~SHOW_ViewMode_Mask;
	ViewportClient->ShowFlags |= SHOW_ViewMode_TextureDensity;
	SetViewModeUI( 7 );
	ViewportClient->Invalidate();

	WxDlgDensityRenderingOptions* DensityRenderingOptions = GApp->GetDlgDensityRenderingOptions();
	check(DensityRenderingOptions);
	DensityRenderingOptions->Show(false);
}

void WxLevelViewportToolBar::OnShaderComplexity( wxCommandEvent& In )
{
	ViewportClient->ShowFlags &= ~SHOW_ViewMode_Mask;
	ViewportClient->ShowFlags |= SHOW_ViewMode_ShaderComplexity;;
	SetViewModeUI( 8 );
	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnLightMapDensity( wxCommandEvent& In )
{
	ViewportClient->ShowFlags &= ~SHOW_ViewMode_Mask;
	ViewportClient->ShowFlags |= SHOW_ViewMode_LightMapDensity;
	SetViewModeUI( 9 );
	ViewportClient->Invalidate();

	WxDlgDensityRenderingOptions* DensityRenderingOptions = GApp->GetDlgDensityRenderingOptions();
	check(DensityRenderingOptions);
	DensityRenderingOptions->Show(true);
}

void WxLevelViewportToolBar::OnLitLightmapDensity( wxCommandEvent& In )
{
	ViewportClient->ShowFlags &= ~SHOW_ViewMode_Mask;
	ViewportClient->ShowFlags |= SHOW_ViewMode_LitLightmapDensity;
	SetViewModeUI( 10 );
	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnReflections( wxCommandEvent& In )
{
	ViewportClient->ShowFlags &= ~SHOW_ViewMode_Mask;
	ViewportClient->ShowFlags |= SHOW_ViewMode_Lit;
	SetViewModeUI( 11 );
	ViewportClient->Invalidate();
}

/**
 * Called when the user wants to set a bookmark at the current location
 *
 * @param	In	Event automatically generated by wxWidgets when the user selects the option
 */
void WxLevelViewportToolBar::OnSetBookmark( wxCommandEvent& In )
{
	FEditorModeTools& Tools = GEditorModeTools();
	for (INT iView=0; iView<GEditor->ViewportClients.Num(); iView++)
	{
		FEditorLevelViewportClient* ViewportClient = GEditor->ViewportClients(iView);
		Tools.SetBookmark( ( In.GetId() - IDM_BookmarkSetLocation_START ), ViewportClient );
	}
}

/**
 * Called when the user wants to jump to a bookmark at a saved location
 *
 * @param	In	Event automatically generated by wxWidgets when the user selects the option
 */
void WxLevelViewportToolBar::OnJumpToBookmark( wxCommandEvent& In )
{
	FEditorModeTools& Tools = GEditorModeTools();
	Tools.JumpToBookmark( ( In.GetId() - IDM_BookmarkJumpToLocation_START ), FALSE );
}

/**
 * Called when the user wants to jump to a bookmark at a saved location and restore the visible levels
 *
 * @param	In	Event automatically generated by wxWidgets when the user selects the option
 */
void WxLevelViewportToolBar::OnJumpToRestoreBookmark( wxCommandEvent& In )
{
	FEditorModeTools& Tools = GEditorModeTools();
	Tools.JumpToBookmark( ( In.GetId() - IDM_BookmarkJumpToRestoreLocation_START ), TRUE );
}

/**
 * Sets the show flags to the default game show flags if checked.
 *
 * @param	In	wxWidget event generated if the GameView button is checked or unchecked. 
 */
void WxLevelViewportToolBar::OnGameView( wxCommandEvent& In )
{
	OnGameViewFlagsChanged(In.IsChecked());

	// Make sure menu/toolbar button state is in sync
	UpdateUI();
}

/**
 * Sets the show flags to the default game show flags if bChecked is TRUE.
 *
 * This makes the viewport display only items that would be seen in-game (such as no editor icons).
 *
 * @param	bChecked	If TRUE, sets the show flags to SHOW_DefaultGame; If FALSE, sets it to SHOW_DefaultEditor.
 */
void WxLevelViewportToolBar::OnGameViewFlagsChanged(UBOOL bChecked)
{
	// Save the view mode because in order to completely set the game view show flag, we 
	// need to wipe any existing flags. We don't want to wipe the view mode flags, though.
	EShowFlags CurrentViewFlags = ViewportClient->ShowFlags & SHOW_ViewMode_Mask;

	// If we aren't in Game view mode it means that we are about to go into it, so we need 
	// to cache off the current show flags so that we can go back to those once we return
	// from Game view mode
	if (!GetInGameViewMode())
	{
		ViewportClient->LastShowFlags = ViewportClient->ShowFlags;
	}

	// If the Game View Show Flags are not checked, then we must revert back to the default editor flags, 
	// if they are we revert back to the last shown flags
	ViewportClient->ShowFlags = bChecked ? SHOW_DefaultGame : ViewportClient->LastShowFlags;

	// Now, we can re-add the view mode flags
	ViewportClient->ShowFlags |= CurrentViewFlags;

	SetInGameViewMode(bChecked);

	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnPerspective( wxCommandEvent& In )
{
	ViewportClient->ViewportType = LVT_Perspective;
	UpdateToolBarButtonEnabledStates();
	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnTop( wxCommandEvent& In )
{
	ViewportClient->ViewportType = LVT_OrthoXY;
	UpdateToolBarButtonEnabledStates();
	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnFront( wxCommandEvent& In )
{
	ViewportClient->ViewportType = LVT_OrthoYZ;
	UpdateToolBarButtonEnabledStates();
	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnSide( wxCommandEvent& In )
{
	ViewportClient->ViewportType = LVT_OrthoXZ;
	UpdateToolBarButtonEnabledStates();
	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnViewportTypeLeftClick( wxCommandEvent& In )
{
	// Get the resource ID of the current viewport type and increase it by 1.
	INT NewType = ResourceIDFromViewportType(ViewportClient->ViewportType) + 1;
	if ( NewType >= IDM_VIEWPORT_TYPE_END )
	{
		// Start over
		NewType = IDM_VIEWPORT_TYPE_START;
	}

	// Set the viewport type to the new type.
	ViewportClient->ViewportType = ViewportTypeFromResourceID( NewType );

	//Update the bitmap
	SetToolNormalBitmap( IDM_VIEWPORT_TYPE_CYCLE_BUTTON, ViewportTypesB[NewType - IDM_VIEWPORT_TYPE_START] );
	
	WxLevelViewportWindow* LevelViewportWindow = static_cast<WxLevelViewportWindow*>(ViewportClient);
	LevelViewportWindow->ResizeToolBar();	
	
	// update the window 
	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnViewportTypeRightClick( wxCommandEvent& In )
{
	wxMenu* RightClickMenu = new wxMenu;
	
	// Construct a right-click menu with the current viewport type already selected
	if ( RightClickMenu )
	{
		RightClickMenu->AppendCheckItem( IDM_PERSPECTIVE, *LocalizeUnrealEd( "Perspective" ) );
		RightClickMenu->AppendCheckItem( IDM_TOP, *LocalizeUnrealEd( "Top" ) );
		RightClickMenu->AppendCheckItem( IDM_FRONT, *LocalizeUnrealEd( "Front" ) );
		RightClickMenu->AppendCheckItem( IDM_SIDE, *LocalizeUnrealEd( "Side" ) );
		RightClickMenu->Check( ResourceIDFromViewportType(ViewportClient->ViewportType), TRUE );

		FTrackPopupMenu TrackPopUpMenu( this, RightClickMenu );
		TrackPopUpMenu.Show();
		delete RightClickMenu;
	}
}

void WxLevelViewportToolBar::OnTearOffNewFloatingViewport( wxCommandEvent& In )
{
	if ( GApp && GApp->EditorFrame && GApp->EditorFrame->ViewportConfigData )
	{
		FViewportConfig_Data *ViewportConfig = GApp->EditorFrame->ViewportConfigData;

		// The 'torn off copy' will have the same dimensions and settings as the original window
		FFloatingViewportParams ViewportParams;
		ViewportParams.ParentWxWindow = GApp->EditorFrame;
		ViewportParams.ViewportType = ViewportClient->ViewportType;
		ViewportParams.ShowFlags = ViewportClient->ShowFlags;
		ViewportParams.Width = ViewportClient->Viewport->GetSizeX();
		ViewportParams.Height = ViewportClient->Viewport->GetSizeY();


		// Create the new floating viewport
		INT NewViewportIndex = INDEX_NONE;
		UBOOL bResultValue = ViewportConfig->OpenNewFloatingViewport(ViewportParams, NewViewportIndex);

		if( bResultValue )
		{
			// OK, now copy various settings from our viewport into the newly created viewport
			FVCD_Viewport& NewViewport = ViewportConfig->AccessViewport( NewViewportIndex );
			WxLevelViewportWindow* NewViewportWin = NewViewport.ViewportWindow;
			if( NewViewportWin != NULL )
			{
				NewViewportWin->CopyLayoutFromViewport( *ViewportClient );
				NewViewportWin->Invalidate();
				NewViewportWin->ToolBar->UpdateUI();
			}
		}
		else
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd( "OpenNewFloatingViewport_Error" ) );
		}
	}
}



void WxLevelViewportToolBar::OnLockSelectedToCamera( wxCommandEvent& In )
{
	if ( GApp && GApp->EditorFrame && GApp->EditorFrame->ViewportConfigData )
	{
		FViewportConfig_Data *ViewportConfig = GApp->EditorFrame->ViewportConfigData;
		ViewportClient->bLockSelectedToCamera = In.IsChecked();

		if( !ViewportClient->bLockSelectedToCamera )
		{
			ViewportClient->ViewRotation.Pitch = 0;
			ViewportClient->ViewRotation.Roll = 0;
		}
	}
}

void WxLevelViewportToolBar::OnMakeParentViewport( wxCommandEvent& In )
{
	if ( GApp && GApp->EditorFrame && GApp->EditorFrame->ViewportConfigData )
	{
		// Allow only perspective views to be view parents
		const UBOOL bIsPerspectiveView = ViewportClient->IsPerspective();
		if ( bIsPerspectiveView )
		{
			FlushRenderingCommands();
			const UBOOL bIsViewParent = ViewportClient->ViewState->IsViewParent();

			// First, clear all existing view parent status.
			for( INT ViewportIndex = 0 ; ViewportIndex < GApp->EditorFrame->ViewportConfigData->GetViewportCount() ; ++ViewportIndex )
			{
				FVCD_Viewport& CurViewport = GApp->EditorFrame->ViewportConfigData->AccessViewport( ViewportIndex );
				if( CurViewport.bEnabled )
				{
					CurViewport.ViewportWindow->ViewState->SetViewParent( NULL );
					CurViewport.ViewportWindow->Invalidate();
				}
			}

			// If the view was not a parent, we're toggling occlusion parenting 'on' for this viewport.
			if ( !bIsViewParent )
			{
				for( INT ViewportIndex = 0 ; ViewportIndex < GApp->EditorFrame->ViewportConfigData->GetViewportCount() ; ++ViewportIndex )
				{
					FVCD_Viewport& CurViewport = GApp->EditorFrame->ViewportConfigData->AccessViewport( ViewportIndex );
					if( CurViewport.bEnabled )
					{
						CurViewport.ViewportWindow->ViewState->SetViewParent( ViewportClient->ViewState );
					}
				}
			}

			// Finally, update level viewport toolbar UI.
			for( INT ViewportIndex = 0 ; ViewportIndex < GApp->EditorFrame->ViewportConfigData->GetViewportCount() ; ++ViewportIndex )
			{
				FVCD_Viewport& CurViewport = GApp->EditorFrame->ViewportConfigData->AccessViewport( ViewportIndex );
				if( CurViewport.bEnabled )
				{
					CurViewport.ViewportWindow->ToolBar->UpdateUI();
				}
			}
		}
	}
}

void WxLevelViewportToolBar::OnAddCameraActor( wxCommandEvent& In )
{
	ACameraActor* NewCamera = Cast<ACameraActor>( GWorld->SpawnActor( ACameraActor::StaticClass() ) );
	if( ViewportClient )
	{
		NewCamera->SetLocation( ViewportClient->ViewLocation );
		NewCamera->SetRotation( ViewportClient->ViewRotation );
	}
	ViewportClient->Invalidate();
}

void WxLevelViewportToolBar::OnSceneViewModeSelChange( wxCommandEvent& In )
{
	ViewportClient->ShowFlags &= ~SHOW_ViewMode_Mask;
	switch( In.GetInt() )
	{
		case 0: ViewportClient->ShowFlags |= SHOW_ViewMode_BrushWireframe; break;
		case 1: ViewportClient->ShowFlags |= SHOW_ViewMode_Wireframe; break;
		case 2: ViewportClient->ShowFlags |= SHOW_ViewMode_Unlit; break;
		default:
		case 3: ViewportClient->ShowFlags |= SHOW_ViewMode_Lit; break;
		case 4:	ViewportClient->ShowFlags |= SHOW_ViewMode_LightingOnly; break;
		case 5:	ViewportClient->ShowFlags |= SHOW_ViewMode_LightComplexity; break;
		case 6:	ViewportClient->ShowFlags |= SHOW_ViewMode_TextureDensity; break;
		case 7:	ViewportClient->ShowFlags |= SHOW_ViewMode_ShaderComplexity; break;
		case 8: ViewportClient->ShowFlags |= SHOW_ViewMode_LightMapDensity; break;
			break;
	}
	SetViewModeUI( In.GetInt() );
	ViewportClient->Invalidate();
}


/** for FShowFlagData */
class FShowFlagData
{
public:

	/** The wxWidgets resource ID for responding to events */
	INT				ID;
	/** The display name of this show flag (source for LocalizeUnrealEd) */
	FString			LocalizedName;
	/** The show flag mask */
	EShowFlags		Mask;
	/** Which group the flags should show up */
	EShowFlagGroup	Group;

	FShowFlagData(INT InID, const char *InUnLocalizedName, const EShowFlags& InMask, EShowFlagGroup InGroup = SFG_Normal)
		:	ID( InID )
		,	LocalizedName( *LocalizeUnrealEd(InUnLocalizedName) )
		,	Mask( InMask )
		,	Group( InGroup )
	{}
};

IMPLEMENT_COMPARE_CONSTREF( FShowFlagData, LevelViewportToolBar, { return appStricmp(*A.LocalizedName, *B.LocalizedName); } )

static TArray<FShowFlagData>& GetShowFlagMenuItems( )
{
	static TArray<FShowFlagData> OutShowFlags;

	static UBOOL bFirst = TRUE; 
	if(bFirst)
	{
		// do this only once
		bFirst = FALSE;

		INT MenuId = 0;

		// Present the user with an alphabetical listing of available show flags.
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "BSPSF", SHOW_BSP ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "BSPSplitSF", SHOW_BSPSplit, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "ActorTagsSF", SHOW_ActorTags, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "BoundsSF", SHOW_Bounds, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "AudioRadiusSF", SHOW_AudioRadius, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "BuilderBrushSF", SHOW_BuilderBrush ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "CameraFrustumsSF", SHOW_CamFrustums, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "CollisionSF", SHOW_Collision, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "DecalsSF", SHOW_Decals ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "DecalInfoSF", SHOW_DecalInfo, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "FogSF", SHOW_Fog, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "GridSF", SHOW_Grid ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "HitProxiesSF", SHOW_HitProxies, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "KismetReferencesSF", SHOW_KismetRefs, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "LargeVerticesSF", SHOW_LargeVertices, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "LensFlareSF", SHOW_LensFlares ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "LevelColorationSF", SHOW_LevelColoration, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "PropertyColorationSF", SHOW_PropertyColoration, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "LightInfluencesSF", SHOW_LightInfluences, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "LightRadiusSF", SHOW_LightRadius, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "LODSF", SHOW_LOD, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "MeshEdgesSF", SHOW_MeshEdges, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "ModeWidgetsSF", SHOW_ModeWidgets ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "NavigationNodesSF", SHOW_NavigationNodes, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "PathsSF", SHOW_Paths ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "ViewportShowFlagsMenu_VertexColorsSF", SHOW_VertexColors, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "SelectionSF", SHOW_Selection, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "ShadowFrustumsSF", SHOW_ShadowFrustums, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "SpritesSF", SHOW_Sprites, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "SplinesSF", SHOW_Splines, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "ConstraintsSF", SHOW_Constraints, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "DynamicShadowsSF", SHOW_DynamicShadows, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "ParticlesSF", SHOW_Particles ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "StaticMeshesSF", SHOW_StaticMeshes ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "InstancedStaticMeshesSF", SHOW_InstancedStaticMeshes, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "SkeletalMeshesSF", SHOW_SkeletalMeshes ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "SpeedTreesSF", SHOW_SpeedTrees ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "TerrainSF", SHOW_Terrain ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "TerrainPatchesSF", SHOW_TerrainPatches, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "UnlitTranslucencySF", SHOW_UnlitTranslucency, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "VolumesSF", SHOW_Volumes ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "SceneCaptureUpdatesSF", SHOW_SceneCaptureUpdates, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "SentinelStatsSF", SHOW_SentinelStats, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "LightFunctionsSF", SHOW_LightFunctions, SFG_Advanced ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "TessellationSF", SHOW_Tessellation, SFG_Advanced ) );

		OutShowFlags.AddItem( FShowFlagData( MenuId++, "ZeroExtent", SHOW_CollisionZeroExtent, SFG_ColisionModes ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "NonZeroExtent", SHOW_CollisionNonZeroExtent, SFG_ColisionModes ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "RigidBody", SHOW_CollisionRigidBody, SFG_ColisionModes ) );
//		OutShowFlags.AddItem( FShowFlagData( MenuId++, "Normal", SHOW_Collision_Any, SFG_ColisionModes ) );

		OutShowFlags.AddItem( FShowFlagData( MenuId++, "PostProcessSF", SHOW_PostProcess, SFG_PostProcess ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "MotionBlurSF", SHOW_MotionBlur, SFG_PostProcess ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "ImageGrainSF", SHOW_ImageGrain, SFG_PostProcess ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "DepthOfFieldSF", SHOW_DepthOfField, SFG_PostProcess ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "ImageReflectionsSF", SHOW_ImageReflections, SFG_PostProcess ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "SubsurfaceScatteringSF", SHOW_SubsurfaceScattering, SFG_PostProcess ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "VisualizeDOFLayersSF", SHOW_VisualizeDOFLayers, SFG_PostProcess ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "SSAOSF", SHOW_SSAO, SFG_PostProcess ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "VisualizeSSAOSF", SHOW_VisualizeSSAO, SFG_PostProcess ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "LightShaftsSF", SHOW_LightShafts, SFG_PostProcess ) );
		OutShowFlags.AddItem( FShowFlagData( MenuId++, "PostProcessAASF", SHOW_PostProcessAA, SFG_PostProcess ) );

		// Sort the show flags alphabetically by string.
		Sort<USE_COMPARE_CONSTREF(FShowFlagData,LevelViewportToolBar)>( OutShowFlags.GetTypedData(), OutShowFlags.Num() );

		check(MenuId < SHOWFLAGS_MENU_MAX_NUM );
	}

	return OutShowFlags;
}

static void SetShowFlag( FEditorLevelViewportClient *ViewportClient, INT ID, UBOOL bEnable )
{
	// can be optimized
	const TArray<FShowFlagData>& ShowFlags = GetShowFlagMenuItems();

	for ( INT i = 0 ; i < ShowFlags.Num() ; ++i )
	{
		const FShowFlagData& ShowFlagData = ShowFlags(i);

		if(ID == ShowFlagData.ID)
		{
			UBOOL bCurrentlyEnabled = ( ViewportClient->ShowFlags & ShowFlagData.Mask ) != 0;
			if( bEnable != bCurrentlyEnabled )
			{
				ViewportClient->ShowFlags ^= ShowFlagData.Mask;
			}
			ViewportClient->Invalidate();
			return;
		}
	}

	// GetShowFlagMenuItems doesn't know this control ID
	checkSlow(0);
}

static UBOOL CheckShowFlag( FEditorLevelViewportClient *ViewportClient, INT ID )
{
	// can be optimized
	const TArray<FShowFlagData>& ShowFlags = GetShowFlagMenuItems();

	for ( INT i = 0 ; i < ShowFlags.Num() ; ++i )
	{
		const FShowFlagData& ShowFlagData = ShowFlags(i);

		if(ID == ShowFlagData.ID)
		{
			return ( ( ViewportClient->ShowFlags & ShowFlagData.Mask ) != 0 );
		}
	}
	return FALSE;
}

static void ToggleShowFlag( FEditorLevelViewportClient *ViewportClient, INT ID )
{
	// can be optimized
	const TArray<FShowFlagData>& ShowFlags = GetShowFlagMenuItems();

	for ( INT i = 0 ; i < ShowFlags.Num() ; ++i )
	{
		const FShowFlagData& ShowFlagData = ShowFlags(i);

		if(ID == ShowFlagData.ID)
		{
			ViewportClient->ShowFlags ^= ShowFlagData.Mask;
			ViewportClient->Invalidate();
			return;
		}
	}

	// GetShowFlagMenuItems doesn't know this control ID
	checkSlow(0);
}

/**
 * Displays the options menu for this viewport
 */
void WxLevelViewportToolBar::OnOptionsMenu( wxCommandEvent& In )
{
	wxMenu* OptionsMenu = new wxMenu();

	OptionsMenu->Append( IDM_VIEWPORT_SHOW_FLAGS_QUICK_MENU, *LocalizeUnrealEd( "ViewportOptionsMenu_ShowFlagsQuickMenu" ) );
	OptionsMenu->AppendSeparator();

	const TArray<FShowFlagData>& ShowFlags = GetShowFlagMenuItems();

	// Show flags menu
	wxMenu* ShowMenu = new wxMenu();
	{
		ShowMenu->Append(IDM_VIEWPORT_SHOW_DEFAULTS, *LocalizeUnrealEd("LevelViewportOptions_DefaultShowFlags"));

		ShowMenu->AppendSeparator();

		{
			wxMenu* CollisionMenu = new wxMenu();

			CollisionMenu->AppendCheckItem(IDM_VIEWPORT_SHOW_COLLISION_ZEROEXTENT, *LocalizeUnrealEd("ZeroExtent") );
			CollisionMenu->Check(IDM_VIEWPORT_SHOW_COLLISION_ZEROEXTENT, (ViewportClient->ShowFlags & SHOW_CollisionZeroExtent) ? true : false);

			CollisionMenu->AppendCheckItem(IDM_VIEWPORT_SHOW_COLLISION_NONZEROEXTENT, *LocalizeUnrealEd("NonZeroExtent") );
			CollisionMenu->Check(IDM_VIEWPORT_SHOW_COLLISION_NONZEROEXTENT, (ViewportClient->ShowFlags & SHOW_CollisionNonZeroExtent) ? true : false);

			CollisionMenu->AppendCheckItem(IDM_VIEWPORT_SHOW_COLLISION_RIGIDBODY, *LocalizeUnrealEd("RigidBody") );
			CollisionMenu->Check(IDM_VIEWPORT_SHOW_COLLISION_RIGIDBODY, (ViewportClient->ShowFlags & SHOW_CollisionRigidBody) ? true : false);

			CollisionMenu->AppendCheckItem(IDM_VIEWPORT_SHOW_COLLISION_NONE, *LocalizeUnrealEd("Normal") );
			CollisionMenu->Check(IDM_VIEWPORT_SHOW_COLLISION_NONE, !(ViewportClient->ShowFlags & SHOW_Collision_Any));

			ShowMenu->Append( IDM_VIEWPORT_SHOW_COLLISIONMENU, *LocalizeUnrealEd("CollisionModes"), CollisionMenu );
		}

		ShowMenu->AppendSeparator();

		wxMenu* AdvancedShowMenu = new wxMenu();
		wxMenu* PostProcessShowMenu = new wxMenu();

		for ( INT i = 0 ; i < ShowFlags.Num() ; ++i )
		{
			const FShowFlagData& ShowFlagData = ShowFlags(i);
			
			wxMenu* LocalMenu = ShowMenu;

			if( ShowFlagData.Group == SFG_Normal )
			{
				// is already set
			}
			else if( ShowFlagData.Group == SFG_Advanced )
			{
				LocalMenu = AdvancedShowMenu;
			}
			else if( ShowFlagData.Group == SFG_PostProcess )
			{
				LocalMenu = PostProcessShowMenu;
			}
			else if( ShowFlagData.Group == SFG_ColisionModes )
			{
				continue;	// we add collision modes separately for the menu currently
			}
			else
			{
				checkSlow(0);
			}

			LocalMenu->AppendCheckItem( ShowFlagData.ID + IDM_VIEWPORT_SHOWFLAGS_MENU_START, *ShowFlagData.LocalizedName );
			const UBOOL bShowFlagEnabled = (ViewportClient->ShowFlags & ShowFlagData.Mask) ? TRUE : FALSE;
			LocalMenu->Check( ShowFlagData.ID + IDM_VIEWPORT_SHOWFLAGS_MENU_START, bShowFlagEnabled == TRUE );
		}

		ShowMenu->AppendSeparator();
		ShowMenu->Append( IDM_VIEWPORT_SHOW_POSTPROCESS_MENU, *LocalizeUnrealEd("ShowMenu_PostProcessFlags"), PostProcessShowMenu );
		ShowMenu->Append( IDM_VIEWPORT_SHOW_ADVANCED_MENU, *LocalizeUnrealEd("ShowMenu_AdvancedFlags"), AdvancedShowMenu );
	}

	OptionsMenu->Append( wxID_ANY, *LocalizeUnrealEd( "ViewportOptionsMenu_Show" ), ShowMenu );

	// Show Volumes menu
	{
		wxMenu* VolumeMenu = new wxMenu();

		// Get sorted array of volume classes then create a menu item for each one
		TArray< UClass* > VolumeClasses;

		VolumeMenu->AppendCheckItem( IDM_VolumeActorVisibilityShowAll, *LocalizeUnrealEd( TEXT("ShowAll") ) );
		VolumeMenu->AppendCheckItem( IDM_VolumeActorVisibilityHideAll, *LocalizeUnrealEd( TEXT("HideAll") ) );

		VolumeMenu->AppendSeparator();

		GApp->EditorFrame->GetSortedVolumeClasses( &VolumeClasses );

		INT ID = IDM_VolumeClasses_START;

		// The index to insert menu items
		const INT MenuItemIdx = VolumeMenu->GetMenuItemCount();

		for( INT VolumeIdx = 0; VolumeIdx < VolumeClasses.Num(); VolumeIdx++ )
		{
			// Insert instead of append so that volume classes will be in A-Z order and not Z-A order
			VolumeMenu->InsertCheckItem( MenuItemIdx, ID, *VolumeClasses( VolumeIdx )->GetName() );
			// The menu item should be checked if the bit for this volume class is set
			VolumeMenu->Check( ID, ViewportClient->VolumeActorVisibility(VolumeIdx) != FALSE );
			++ID;
		}

		OptionsMenu->Append( wxID_ANY, *LocalizeUnrealEd( TEXT("VolumeActorVisibility") ), VolumeMenu );
	}

	// Per-layer visibility
	{
		// get all the known layers
		TArray<FName> AllLayers;
		FLayerUtils::GetAllLayers(AllLayers);

		// Create a new menu and add the flags in alphabetical order.
		wxMenu* Menu = new wxMenu();

		// add the hide all/none items
		Menu->Append( IDM_PerViewLayers_HideAll, *LocalizeUnrealEd("PerViewLayerHideAll"));
		Menu->Append( IDM_PerViewLayers_HideNone, *LocalizeUnrealEd("PerViewLayerHideNone"));

		UBOOL bNeedSeparator = TRUE;

		INT MenuID = IDM_PerViewLayers_Start;
		// get all the layer
		for (INT LayerIndex = 0; LayerIndex < AllLayers.Num(); LayerIndex++, MenuID++)
		{
			if( bNeedSeparator )
			{
				Menu->AppendSeparator();
				bNeedSeparator = FALSE;
			}

			// add an item for each layer
			Menu->AppendCheckItem( MenuID, *AllLayers(LayerIndex).ToString());

			// check it if it's visible
			Menu->Check( MenuID, (ViewportClient->ViewHiddenLayers.FindItemIndex(AllLayers(LayerIndex)) != INDEX_NONE));
		}

		OptionsMenu->Append( wxID_ANY, *LocalizeUnrealEd("PerViewLayerHiding"), Menu );

	}

	// Sprite visibility
	{
		wxMenu* SpriteMenu = new wxMenu();

		SpriteMenu->AppendCheckItem( IDM_SpriteComponentVisibilityShowAll, *LocalizeUnrealEd( TEXT("ShowAll") ) );
		SpriteMenu->AppendCheckItem( IDM_SpriteComponentVisibilityHideAll, *LocalizeUnrealEd( TEXT("HideAll") ) );

		SpriteMenu->AppendSeparator();

		const TArray<FString>& SpriteCategories = GUnrealEd->SortedSpriteCategories;
		check( SpriteCategories.Num() <= ( IDM_SpriteComponentCategories_END - IDM_SpriteComponentCategories_START ) );

		for( INT SpriteCatIdx = 0; SpriteCatIdx < SpriteCategories.Num(); ++SpriteCatIdx )
		{
			SpriteMenu->AppendCheckItem( IDM_SpriteComponentCategories_START + SpriteCatIdx, *SpriteCategories( SpriteCatIdx ) );
		}

		OptionsMenu->Append( wxID_ANY, *LocalizeUnrealEd( TEXT("SpriteCategoryVisibility") ), SpriteMenu );
	}

	OptionsMenu->AppendSeparator();

	// Viewport type
	{
		wxMenu* ViewportTypeMenu = new wxMenu();
		ViewportTypeMenu->AppendCheckItem( IDM_PERSPECTIVE, *LocalizeUnrealEd( "Perspective" ) );
		ViewportTypeMenu->AppendCheckItem( IDM_TOP, *LocalizeUnrealEd( "Top" ) );
		ViewportTypeMenu->AppendCheckItem( IDM_FRONT, *LocalizeUnrealEd( "Front" ) );
		ViewportTypeMenu->AppendCheckItem( IDM_SIDE, *LocalizeUnrealEd( "Side" ) );

		OptionsMenu->Append( wxID_ANY, *LocalizeUnrealEd( "ViewportOptionsMenu_Type" ), ViewportTypeMenu );
	}

	// Viewport presets
	{
		wxMenu* ViewportSizeMenu = new wxMenu();
		ViewportSizeMenu->AppendCheckItem( (BPD_IPHONE_3GS - BPD_FIRST_VIEWPORT_RESIZE_DEVICE) + IDM_ResizeToDevice_Start, *LocalizeUnrealEd( "BuildPlay_iPhone3GS" ) );
		ViewportSizeMenu->AppendCheckItem( (BPD_IPHONE_4 - BPD_FIRST_VIEWPORT_RESIZE_DEVICE) + IDM_ResizeToDevice_Start, *LocalizeUnrealEd( "BuildPlay_iPhone4" ) );		
		ViewportSizeMenu->AppendCheckItem( (BPD_IPHONE_5 - BPD_FIRST_VIEWPORT_RESIZE_DEVICE) + IDM_ResizeToDevice_Start, *LocalizeUnrealEd( "BuildPlay_iPhone5" ) );
		ViewportSizeMenu->AppendCheckItem( (BPD_IPOD_TOUCH_4 - BPD_FIRST_VIEWPORT_RESIZE_DEVICE) + IDM_ResizeToDevice_Start, *LocalizeUnrealEd( "BuildPlay_iPodTouch4" ) );
		ViewportSizeMenu->AppendCheckItem( (BPD_IPOD_TOUCH_5 - BPD_FIRST_VIEWPORT_RESIZE_DEVICE) + IDM_ResizeToDevice_Start, *LocalizeUnrealEd( "BuildPlay_iPodTouch5" ) );
		ViewportSizeMenu->AppendCheckItem( (BPD_IPAD - BPD_FIRST_VIEWPORT_RESIZE_DEVICE) + IDM_ResizeToDevice_Start, *LocalizeUnrealEd( "BuildPlay_iPad" ) );
		ViewportSizeMenu->AppendCheckItem( (BPD_IPAD2 - BPD_FIRST_VIEWPORT_RESIZE_DEVICE) + IDM_ResizeToDevice_Start, *LocalizeUnrealEd( "BuildPlay_iPad2" ) );
		ViewportSizeMenu->AppendCheckItem( (BPD_IPAD3 - BPD_FIRST_VIEWPORT_RESIZE_DEVICE) + IDM_ResizeToDevice_Start, *LocalizeUnrealEd( "BuildPlay_iPad3" ) );
		ViewportSizeMenu->AppendCheckItem( (BPD_IPAD4 - BPD_FIRST_VIEWPORT_RESIZE_DEVICE) + IDM_ResizeToDevice_Start, *LocalizeUnrealEd( "BuildPlay_iPad4" ) );
		ViewportSizeMenu->AppendCheckItem( (BPD_IPAD_MINI - BPD_FIRST_VIEWPORT_RESIZE_DEVICE) + IDM_ResizeToDevice_Start, *LocalizeUnrealEd( "BuildPlay_iPadMini" ) );
#if !UDK
		ViewportSizeMenu->AppendCheckItem( (BPD_XBOX_360 - BPD_FIRST_VIEWPORT_RESIZE_DEVICE) + IDM_ResizeToDevice_Start, *LocalizeUnrealEd( "BuildPlay_XBOX_360" ) );
		ViewportSizeMenu->AppendCheckItem( (BPD_PS3 - BPD_FIRST_VIEWPORT_RESIZE_DEVICE) + IDM_ResizeToDevice_Start, *LocalizeUnrealEd( "BuildPlay_PS3" ) );
#endif

		OptionsMenu->Append( wxID_ANY, *LocalizeUnrealEd( "ViewportPresetSizes_Menu" ), ViewportSizeMenu );
	}


	// Realtime
	OptionsMenu->AppendCheckItem( IDM_REALTIME, *LocalizeUnrealEd( "ViewportOptionsMenu_ToggleRealTime" ) );

	OptionsMenu->AppendCheckItem( IDM_ViewportOptions_ShowFPS, *LocalizeUnrealEd( "ViewportOptions_ShowFPS" ), *LocalizeUnrealEd( "ViewportOptions_ShowFPS_ToolTip" ) );
	OptionsMenu->Check( IDM_ViewportOptions_ShowFPS, ViewportClient->ShouldShowFPS() == TRUE );

	OptionsMenu->AppendCheckItem( IDM_ViewportOptions_ShowStats, *LocalizeUnrealEd( "ViewportOptions_ShowStats" ), *LocalizeUnrealEd( "ViewportOptions_ShowStats_ToolTip" ) );
	OptionsMenu->Check( IDM_ViewportOptions_ShowStats, ViewportClient->ShouldShowStats() == TRUE );


	// Shading mode
	{
		OptionsMenu->AppendSeparator();

		OptionsMenu->AppendCheckItem( IDM_BRUSHWIREFRAME, *LocalizeUnrealEd("LevelViewportOptions_BrushWireframe") );
		OptionsMenu->AppendCheckItem( IDM_WIREFRAME, *LocalizeUnrealEd("LevelViewportOptions_Wireframe") );
		OptionsMenu->AppendCheckItem( IDM_UNLIT, *LocalizeUnrealEd("LevelViewportOptions_Unlit") );
		OptionsMenu->AppendCheckItem( IDM_LIT, *LocalizeUnrealEd("LevelViewportOptions_Lit") );
		OptionsMenu->AppendCheckItem( IDM_DETAILLIGHTING, *LocalizeUnrealEd("LevelViewportOptions_DetailLighting") );
		OptionsMenu->AppendCheckItem( IDM_LIGHTINGONLY, *LocalizeUnrealEd("LevelViewportOptions_LightingOnly") );
		OptionsMenu->AppendCheckItem( IDM_LIGHTCOMPLEXITY, *LocalizeUnrealEd("LevelViewportOptions_LightComplexity") );
		OptionsMenu->AppendCheckItem( IDM_TEXTUREDENSITY, *LocalizeUnrealEd("LevelViewportOptions_TextureDensity") );
		OptionsMenu->AppendCheckItem( IDM_SHADERCOMPLEXITY, *LocalizeUnrealEd("LevelViewportOptions_ShaderComplexity") );
		OptionsMenu->AppendCheckItem( IDM_LIGHTMAPDENSITY, *LocalizeUnrealEd("LevelViewportOptions_LightMapDensity") );
		OptionsMenu->AppendCheckItem( IDM_LITLIGHTMAPDENSITY, *LocalizeUnrealEd("LevelViewportOptions_LitLightmapDensity") );
		OptionsMenu->AppendCheckItem( IDM_REFLECTIONS, *LocalizeUnrealEd("LevelViewportOptions_Reflections") );

		OptionsMenu->AppendSeparator();
	}
	// Bookmarks
	wxMenu* BookmarkMenu = new wxMenu();
	BuildBookmarkMenu(BookmarkMenu, TRUE);
	OptionsMenu->AppendSubMenu( BookmarkMenu, *LocalizeUnrealEd("LevelViewportContext_Bookmarks"), *LocalizeUnrealEd("ToolTip_Bookmarks" ) );

	OptionsMenu->AppendSeparator();

	// Game View
	OptionsMenu->AppendCheckItem(IDM_VIEWPORT_SHOW_GAMEVIEW, *LocalizeUnrealEd("ViewportOptionsMenu_ToggleGameView"));
	OptionsMenu->Check(IDM_VIEWPORT_SHOW_GAMEVIEW, GetInGameViewMode() ? true : false );

	OptionsMenu->AppendSeparator();

	{
		OptionsMenu->AppendCheckItem( IDM_AllowMatineePreview, *LocalizeUnrealEd( "ViewportOptionsMenu_AllowMatineePreview" ) );
		OptionsMenu->AppendCheckItem( IDM_MOVEUNLIT, *LocalizeUnrealEd( "ViewportOptionsMenu_UnlitMovement" ) );

		OptionsMenu->AppendCheckItem( IDM_MakeOcclusionParent, *LocalizeUnrealEd( "ViewportOptions_MakeViewOcclusionParent" ), *LocalizeUnrealEd( "ViewportOptions_MakeViewOcclusionParent_ToolTip" ) );
	}

	if( ViewportClient->ViewportType == LVT_Perspective )
	{
		OptionsMenu->AppendSeparator();
		OptionsMenu->Append( IDM_ViewportOptions_AddCameraActor, *LocalizeUnrealEd( "ViewportOptions_AddCameraActor" ), *LocalizeUnrealEd( "ViewportOptions_AddCameraActor_ToolTip" ) );
	}

	// Display the options menu
	FTrackPopupMenu tpm( this, OptionsMenu );
	tpm.Show();
	delete OptionsMenu;
}

void WxLevelViewportToolBar::OnShowFlagsShortcut( wxCommandEvent& In )
{
	// Before showing the Show Flags dialog, hide all the others..!

	for(INT i=0; i<GApp->EditorFrame->ViewportConfigData->GetViewportCount(); i++)
	{
		WxLevelViewportWindow* LevelVC = GApp->EditorFrame->ViewportConfigData->AccessViewport(i).ViewportWindow;
		if(LevelVC)
		{
			WxLevelViewportToolBar* ToolBar = LevelVC->ToolBar;
			if( ToolBar )
			{
				ToolBar->Show_ShowFlagsDialog( FALSE );
			}
		}
	}

	Show_ShowFlagsDialog( TRUE );
}

void WxLevelViewportToolBar::OnShowDefaults( wxCommandEvent& In )
{
	// Setting show flags to the defaults should not stomp on the current viewmode settings.
	const EShowFlags ViewModeShowFlags = ViewportClient->ShowFlags &= SHOW_ViewMode_Mask;
	ViewportClient->ShowFlags = SHOW_DefaultEditor;
	ViewportClient->ShowFlags |= ViewModeShowFlags;
	SetInGameViewMode(FALSE);
	ViewportClient->LastShowFlags = ViewportClient->ShowFlags;

	UpdateUI();

	ViewportClient->Invalidate();
}

/**
 * Sets the show flag to SHOW_DefaultGame if the "Game View Show Flag" is checked. Otherwise, this function sets 
 * it to SHOW_DefaultEditor. In addition, it syncs the result to the "Game View" button on the toolbar.
 *
 * @param	In	wxWidget event the is called when the user toggles the "Game View Show Flag" option in the Show Flags menu.
 */
void WxLevelViewportToolBar::OnShowGameView( wxCommandEvent& In )
{
	OnGameViewFlagsChanged( In.IsChecked() );

	// We need to update the button the toolbar that associates to the game view 
	// flag. Otherwise, the button will be out of sync with the viewport.
	ToggleTool( IDM_VIEWPORT_SHOW_GAMEVIEW, GetInGameViewMode() ? true : false );

}


/**
 * Toggles FPS display in real-time viewports
 *
 * @param	In	wxWidget event the is called when the user toggles the "Show FPS" option
 */
void WxLevelViewportToolBar::OnShowFPS( wxCommandEvent& In )
{
	ViewportClient->SetShowFPS( In.IsChecked() );

	if( In.IsChecked() )
	{
		// Also make sure that real-time mode is enabled
		ViewportClient->SetRealtime( In.IsChecked() );
		ViewportClient->Invalidate( FALSE );
	}
}




/**
 * Toggles stats display in real-time viewports
 *
 * @param	In	wxWidget event the is called when the user toggles the "Show Stats" option
 */
void WxLevelViewportToolBar::OnShowStats( wxCommandEvent& In )
{
	ViewportClient->SetShowStats( In.IsChecked() );

	if( In.IsChecked() )
	{
		// Also make sure that real-time mode is enabled
		ViewportClient->SetRealtime( In.IsChecked() );
		ViewportClient->Invalidate( FALSE );
	}
}

void WxLevelViewportToolBar::OnToggleShowFlagMenu( wxCommandEvent& In )
{
	INT Id = In.GetId();
	check( Id >= IDM_VIEWPORT_SHOWFLAGS_MENU_START && Id <= IDM_VIEWPORT_SHOWFLAGS_MENU_END );
	ToggleShowFlag( ViewportClient, Id - IDM_VIEWPORT_SHOWFLAGS_MENU_START );
}

void WxLevelViewportToolBar::UpdateUI_ShowFlagMenu( wxUpdateUIEvent& In )
{
	// Disable selecting SSAO and motion blur if the far clipping plane is not zero (infinite)
	// since those post process effects do not work with non-infinite farplanes
	if( GEditor->FarClippingPlane != 0.0f )
	{
		INT Id = In.GetId();

		// can be optimized
		static const TArray<FShowFlagData>& ShowFlags = GetShowFlagMenuItems();

		// See if this update UI event was for SSAO or motion blur
		for ( INT i = 0 ; i < ShowFlags.Num() ; ++i )
		{
			const FShowFlagData& ShowFlagData = ShowFlags(i);

			if( Id == ShowFlagData.ID && ( ShowFlagData.Mask == SHOW_SSAO || ShowFlagData.Mask == SHOW_MotionBlur ) )
			{
				In.Enable( FALSE );
				break;
			}
		}
	}

}

/** Handle choosing one of the collision view mode options */
void WxLevelViewportToolBar::OnChangeCollisionMode(wxCommandEvent& In)
{
	INT Id = In.GetId();
	
	// Turn off all collision flags
	ViewportClient->ShowFlags &= ~SHOW_Collision_Any;
	// Turn on lens flare display (we do not want lens flares if any collision flags are set)
	ViewportClient->ShowFlags |= SHOW_LensFlares;

	// Then set the one we want
	// If any are set, turn off lens flares
	if(Id == IDM_VIEWPORT_SHOW_COLLISION_ZEROEXTENT)
	{
		ViewportClient->ShowFlags |= SHOW_CollisionZeroExtent;
		ViewportClient->ShowFlags &= ~SHOW_LensFlares;
	}
	else if(Id == IDM_VIEWPORT_SHOW_COLLISION_NONZEROEXTENT)
	{
		ViewportClient->ShowFlags |= SHOW_CollisionNonZeroExtent;
		ViewportClient->ShowFlags &= ~SHOW_LensFlares;
	}
	else if(Id == IDM_VIEWPORT_SHOW_COLLISION_RIGIDBODY)
	{
		ViewportClient->ShowFlags |= SHOW_CollisionRigidBody;
		ViewportClient->ShowFlags &= ~SHOW_LensFlares;
	}

	ViewportClient->Invalidate();
}


void WxLevelViewportToolBar::OnPerViewLayersHideAll( wxCommandEvent& In )
{
	// get all the known layers
	TArray<FName> AllLayers;
	FLayerUtils::GetAllLayers(AllLayers);

	// hide them all
	ViewportClient->ViewHiddenLayers = AllLayers;

	// update actor visibility for this view
	FLayerUtils::UpdatePerViewVisibility(ViewportClient->ViewIndex);

	ViewportClient->Invalidate(); 
}

void WxLevelViewportToolBar::OnPerViewLayersHideNone( wxCommandEvent& In )
{
	// clear all hidden layers
	ViewportClient->ViewHiddenLayers.Empty();
	
	// update actor visibility for this view
	FLayerUtils::UpdatePerViewVisibility(ViewportClient->ViewIndex);

	ViewportClient->Invalidate(); 
}

void WxLevelViewportToolBar::OnTogglePerViewLayer( wxCommandEvent& In )
{
	// get all the known layers
	TArray<FName> AllLayers;
	FLayerUtils::GetAllLayers(AllLayers);

	// get the name of the chosen one (In.GetString() returns None or something)
	FName LayerName = AllLayers(In.GetId() - IDM_PerViewLayers_Start);

	INT HiddenIndex = ViewportClient->ViewHiddenLayers.FindItemIndex(LayerName);
	if (HiddenIndex == INDEX_NONE)
	{
		ViewportClient->ViewHiddenLayers.AddItem(LayerName);
	}
	else
	{
		ViewportClient->ViewHiddenLayers.Remove(HiddenIndex);
	}

	// update actor visibility for this view
	FLayerUtils::UpdatePerViewVisibility(ViewportClient->ViewIndex, LayerName);

	ViewportClient->Invalidate(); 
}

/**
 * Called in response to the camera speed button being left-clicked. Increments the camera speed setting by one, looping 
 * back to slow if clicked while on the fast setting.
 *
 * @param	In	Event generated by wxWidgets in response to the button left-click
 */
void WxLevelViewportToolBar::OnCamSpeedButtonLeftClick( wxCommandEvent& In )
{
	INT SpeedIdToSet = LastCameraSpeedID + 1;
	
	// Cycle the camera speed back to the start if it was already at its maximum setting
	if ( SpeedIdToSet >= ID_CAMSPEED_END )
	{
		SpeedIdToSet = ID_CAMSPEED_START;
	}
	
	// Generate an event to notify the parent window that the camera speed has been requested to change
	wxCommandEvent CamSpeedEvent( wxEVT_COMMAND_MENU_SELECTED, SpeedIdToSet );
	GetEventHandler()->ProcessEvent( CamSpeedEvent );
}

/**
 * Called in response to the camera speed button being right-clicked. Produces a pop-out menu for the user to select
 * their desired speed setting with.
 *
 * @param	In	Event generated by wxWidgets in response to the button right-click
 */
void WxLevelViewportToolBar::OnCamSpeedButtonRightClick( wxCommandEvent& In )
{
	wxMenu* RightClickMenu = new wxMenu;
	
	// Construct a right-click menu with the current camera speed already selected
	if ( RightClickMenu )
	{
		RightClickMenu->AppendCheckItem( ID_CAMSPEED_SLOW, *LocalizeUnrealEd( "ToolTip_28" ) );
		RightClickMenu->AppendCheckItem( ID_CAMSPEED_NORMAL, *LocalizeUnrealEd( "ToolTip_29" ) );
		RightClickMenu->AppendCheckItem( ID_CAMSPEED_FAST, *LocalizeUnrealEd( "ToolTip_30" ) );
		RightClickMenu->AppendCheckItem( ID_CAMSPEED_VERYFAST, *LocalizeUnrealEd( "ToolTip_VeryFastCameraSpeed" ) );
		RightClickMenu->Check( LastCameraSpeedID, TRUE );

		FTrackPopupMenu TrackPopUpMenu( this, RightClickMenu );
		TrackPopUpMenu.Show();
		delete RightClickMenu;
	}
}

/**
 * Called in response to receiving a custom event type designed to update the UI button for the camera speed.
 *
 * @param	In	Event which contains information pertaining to the currently set camera speed so that the UI can update if necessary
 */
void WxLevelViewportToolBar::OnCamSpeedUpdateEvent( wxCommandEvent& In )
{
	INT InId = In.GetId();
	check( InId >= ID_CAMSPEED_START && InId < ID_CAMSPEED_END );

	// Ensure that the UI is displaying the correct icon for the currently selected camera speed
	if ( LastCameraSpeedID != InId )
	{
		LastCameraSpeedID = InId;
		SetToolNormalBitmap( ID_CAMSPEED_CYCLE_BUTTON, CamSpeedsB[LastCameraSpeedID - ID_CAMSPEED_START] );
		WxLevelViewportWindow* LevelViewportWindow = static_cast<WxLevelViewportWindow*>(ViewportClient);
		LevelViewportWindow->ResizeToolBar();
	}
}

/** Called when a user selects a volume actor visibility menu item. **/
void WxLevelViewportToolBar::OnChangeVolumeVisibility( wxCommandEvent& In )
{
	INT VolumeID = In.GetId() - IDM_VolumeClasses_START;

	// Get a sorted list of volume classes.
	TArray< UClass *> VolumeClasses;
	GApp->EditorFrame->GetSortedVolumeClasses( &VolumeClasses );

	// Get the corresponding volume class for the clicked menu item.
	UClass *SelectedVolumeClass = VolumeClasses( VolumeID );
	// Toggle the flag corresponding to this actors ID.  The VolumeActorVisibility bitfield stores the visible/hidden state for all volume types
	ViewportClient->VolumeActorVisibility( VolumeID ) = !ViewportClient->VolumeActorVisibility( VolumeID );
	// Update the found actors visibility based on the new bitfield
	GUnrealEd->UpdateVolumeActorVisibility( SelectedVolumeClass, ViewportClient );
}

/** Called when a user selects show or hide all from the volume visibility menu. **/
void WxLevelViewportToolBar::OnToggleAllVolumeActors( wxCommandEvent& In )
{
	// Reinitialize the volume actor visibility flags to the new state.  All volumes should be visible if "Show All" was selected and hidden if it was not selected.
	const UBOOL NewState =  In.GetId() == IDM_VolumeActorVisibilityShowAll;
	ViewportClient->VolumeActorVisibility.Init( NewState, ViewportClient->VolumeActorVisibility.Num() );
	// Update visibility based on the new state
	// All volume actor types should be taken since the user clicked on show or hide all to get here
	GUnrealEd->UpdateVolumeActorVisibility( NULL, ViewportClient );
}

/** Called when a user toggles the visibility of a sprite category */
void WxLevelViewportToolBar::OnChangeSpriteCategoryVisibility( wxCommandEvent& In )
{
	const INT CategoryIdx = In.GetId() - IDM_SpriteComponentCategories_START;
	ViewportClient->SetSpriteCategoryVisibility( CategoryIdx, !ViewportClient->GetSpriteCategoryVisibility( CategoryIdx ) );
	ViewportClient->Invalidate();
}

/** Called when a user chooses to set all sprite categories to visible or hidden */
void WxLevelViewportToolBar::OnChangeAllSpriteCategoryVisibility( wxCommandEvent& In )
{
	const UBOOL bVisible = ( In.GetId() == IDM_SpriteComponentVisibilityShowAll );
	ViewportClient->SetAllSpriteCategoryVisibility( bVisible );
	ViewportClient->Invalidate();
}

/** Called by wxWidgets to update the UI for the sprite category menu */
void WxLevelViewportToolBar::UpdateUI_SpriteCategoryMenu( wxUpdateUIEvent& In )
{
	const INT CategoryIdx = In.GetId() - IDM_SpriteComponentCategories_START;
	In.Check( ViewportClient->GetSpriteCategoryVisibility( CategoryIdx ) == TRUE );
}

/** Appends Bookmarking options to the standard toolbar */
void WxLevelViewportToolBar::BuildBookmarkMenu(wxMenu* Menu, const UBOOL bIncludeRestore)
{
	check(Menu);
	FEditorModeTools& Tools = GEditorModeTools();

	// Add the 'set' bookmarks
	{
		wxMenu* SetMenu = new wxMenu();
		for ( UINT i = 0; i < IDM_BookmarkSetLocation_END - IDM_BookmarkSetLocation_START; i++ )
		{
			FString index = *appItoa( i );
			SetMenu->Append( IDM_BookmarkSetLocation_START + i, *FString::Printf( LocalizeSecure( LocalizeUnrealEd("LevelViewportContext_SetBookmarkNum" ), *index, *index ) ), *LocalizeUnrealEd("ToolTip_SetBookmark" ) );
		}
		Menu->AppendSubMenu( SetMenu, *LocalizeUnrealEd("LevelViewportContext_SetBookmark"), *LocalizeUnrealEd("ToolTip_SetBookmark" ) );
	}

	// Add the 'jump to' bookmarks (only add if they are available, only add the restore option if specified)
	{
		wxMenu* JumpToMenu = NULL;
		wxMenu* JumpToRestoreMenu = NULL;
		for ( UINT i = 0; i < IDM_BookmarkJumpToLocation_END - IDM_BookmarkJumpToLocation_START; i++ )
		{
			if ( Tools.CheckBookmark( i ) )
			{
				FString index = *appItoa( i );
				if ( !JumpToMenu )
				{
					JumpToMenu = new wxMenu();
				}
				JumpToMenu->Append( IDM_BookmarkJumpToLocation_START + i, *FString::Printf( LocalizeSecure( LocalizeUnrealEd("LevelViewportContext_JumpToBookmarkNum" ), *index ) ), *LocalizeUnrealEd("ToolTip_JumpToBookmark" ) );
				if ( bIncludeRestore )
				{
					if ( !JumpToRestoreMenu )
					{
						JumpToRestoreMenu = new wxMenu();
					}
					JumpToRestoreMenu->Append( IDM_BookmarkJumpToRestoreLocation_START + i, *FString::Printf( LocalizeSecure( LocalizeUnrealEd("LevelViewportContext_JumpToRestoreBookmarkNum" ), *index ) ), *LocalizeUnrealEd("ToolTip_JumpToRestoreBookmark" ) );
				}
			}
		}
		if ( JumpToMenu )
		{
			Menu->AppendSubMenu( JumpToMenu, *LocalizeUnrealEd("LevelViewportContext_JumpToBookmark"), *LocalizeUnrealEd("ToolTip_JumpToBookmark" ) );
		}
		if ( JumpToRestoreMenu )
		{
			Menu->AppendSubMenu( JumpToRestoreMenu, *LocalizeUnrealEd("LevelViewportContext_JumpToRestoreBookmark"), *LocalizeUnrealEd("ToolTip_JumpToRestoreBookmark" ) );
		}
	}
}

/**
 * Returns the level viewport toolbar height, in pixels.
 */
INT WxLevelViewportToolBar::GetToolbarHeight()
{
	wxSize ClientSize = GetClientSize();
	return ClientSize.GetHeight();
}

/*-----------------------------------------------------------------------------
	WxLevelViewportMaximizeToolBar
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxLevelViewportMaximizeToolBar, WxToolBar )
	EVT_TOOL( IDM_MAXIMIZE_VIEWPORT, WxLevelViewportMaximizeToolBar::OnMaximizeViewport )
	EVT_UPDATE_UI( IDM_MAXIMIZE_VIEWPORT, WxLevelViewportMaximizeToolBar::UpdateMaximizeViewportUI )
END_EVENT_TABLE()

WxLevelViewportMaximizeToolBar::WxLevelViewportMaximizeToolBar( wxWindow* InParent, wxWindowID InID, FEditorLevelViewportClient* InViewportClient )
: WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
,	bIsMaximized( FALSE )
,	ViewportClient( InViewportClient )
{
	MaximizeB.Load( TEXT("LVT_Maximize") );
	RestoreB.Load( TEXT("LVT_Restore") );

	AddTool( IDM_MAXIMIZE_VIEWPORT, TEXT(""), MaximizeB, *LocalizeUnrealEd("Maximize_Viewport") );

	Realize();
}


/** Maximizes the level viewport */
void WxLevelViewportMaximizeToolBar::OnMaximizeViewport( wxCommandEvent& In )
{
	if ( GApp && GApp->EditorFrame && GApp->EditorFrame->ViewportConfigData )
	{
		FViewportConfig_Data *ViewportConfig = GApp->EditorFrame->ViewportConfigData;
		ViewportConfig->ToggleMaximize( ViewportClient->Viewport );
		ViewportClient->Invalidate();
	}
}


/** Updates UI state for the 'Maximize Viewport' toolbar button */
void WxLevelViewportMaximizeToolBar::UpdateMaximizeViewportUI( wxUpdateUIEvent& In )
{
	UBOOL bIsCurrentlyMaximized = FALSE;

	if ( GApp && GApp->EditorFrame && GApp->EditorFrame->ViewportConfigData )
	{
		if( GApp->EditorFrame->ViewportConfigData->IsViewportMaximized() )
		{
			const INT MaximizedViewportIndex = GApp->EditorFrame->ViewportConfigData->MaximizedViewport;
			if( MaximizedViewportIndex >= 0 && MaximizedViewportIndex < 4 )
			{
				if( GApp->EditorFrame->ViewportConfigData->Viewports[ MaximizedViewportIndex ].ViewportWindow == ViewportClient )
				{
					// This viewport is maximized, so check the button!
					bIsCurrentlyMaximized = TRUE;
				}
			}
		}
	}

	// if the viewport maximized state has changed, we need to change the maximize button's icon and tooltip
	if( bIsMaximized != bIsCurrentlyMaximized )
	{
		bIsMaximized = bIsCurrentlyMaximized;

		wxToolBarToolBase* MaximizeTool = FindById( IDM_MAXIMIZE_VIEWPORT );
		if( MaximizeTool )
		{
			wxBitmap* NewBitmap = bIsMaximized? &RestoreB: &MaximizeB;
			FString NewToolTip = bIsMaximized? LocalizeUnrealEd("Restore_Viewport"): LocalizeUnrealEd("Maximize_Viewport");
			INT PositionX = 0;
			INT PositionY = 0;
			INT Width = GetDialogOptionsWidth();
			INT Height = GetDialogOptionsHeight();

			GetPosition(&PositionX, &PositionY);

			MaximizeTool->SetNormalBitmap( *NewBitmap );
			MaximizeTool->SetShortHelp( *NewToolTip );
			Realize();
			// this is a bit of a hack, but calling Realize() resets the size and position of the toolbar
			Move(PositionX, PositionY);
			SetSize(Width, Height);
		}
	}
}

/** Returns the level viewport toolbar width, in pixels. */
INT WxLevelViewportMaximizeToolBar::GetDialogOptionsWidth()
{
	wxSize ClientSize = GetClientSize();
	return ClientSize.GetWidth();
}

/** Returns the level viewport toolbar height, in pixels. */
INT WxLevelViewportMaximizeToolBar::GetDialogOptionsHeight()
{
	wxSize ClientSize = GetClientSize();
	return ClientSize.GetHeight();
}

WxShowFlagsDialog::WxShowFlagsDialog( wxWindow* parent, wxWindowID id, const wxString& title, FEditorLevelViewportClient* InViewportClient )
:	wxDialog( parent, id, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER )
,	ViewportClient( InViewportClient )
{
	// callbacks... nb. CALLBACK_ViewportClientInvalidated is included just because CALLBACK_ShowFlagsChanged can't currently work (showflags are set all over the code)
	GCallbackEvent->Register( CALLBACK_ShowFlagsChanged, this );
	GCallbackEvent->Register( CALLBACK_ViewportClientInvalidated, this );

	wxBoxSizer* InnerSizer = new wxBoxSizer( wxVERTICAL );

	wxPanel* DefaultPanel = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxSizer* DefaultPanel_Sizer = new wxBoxSizer( wxVERTICAL );
	wxButton* ResetToDefaultButton = new wxButton( DefaultPanel, IDM_VIEWPORT_SHOW_DEFAULTS, *LocalizeUnrealEd( "LevelViewportOptions_DefaultShowFlags" ) );
	DefaultPanel_Sizer->Add( ResetToDefaultButton, 0, wxEXPAND | wxALL, 0 );
	DefaultPanel->SetSizer( DefaultPanel_Sizer );
	InnerSizer->Add( DefaultPanel, 0, wxEXPAND | wxALL, 4 );

	ShowFlagsCheckBoxes.Empty();

	wxPanel* NormalPanel = CreatePanelWithShowFlagItems( SFG_Normal, LocalizeUnrealEd( "ShowFlagsWindow_Normal" ) );
	InnerSizer->Add( NormalPanel, 0, wxEXPAND | wxALL, 4 );

	wxPanel* PostProcessPanel = CreatePanelWithShowFlagItems( SFG_PostProcess, LocalizeUnrealEd( "ShowFlagsWindow_PostProcessing" ) );
	InnerSizer->Add( PostProcessPanel, 0, wxEXPAND | wxALL, 4 );

	wxPanel* AdvancedPanel = CreatePanelWithShowFlagItems( SFG_Advanced, LocalizeUnrealEd( "ShowFlagsWindow_Advanced" ) );
	InnerSizer->Add( AdvancedPanel, 0, wxEXPAND | wxALL, 4 );

	wxPanel* CollisionModesPanel = CreatePanelWithShowFlagItems( SFG_ColisionModes, LocalizeUnrealEd( "ShowFlagsWindow_CollisionModes" ) );
	InnerSizer->Add( CollisionModesPanel, 0, wxEXPAND | wxALL, 4 );

	SetSizer( InnerSizer );

	Fit();
	Layout();
	SetAutoLayout( true );

	Hide();
}

wxPanel* WxShowFlagsDialog::CreatePanelWithShowFlagItems( INT Group, FString Name )
{
	wxPanel* MyPanel = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxSizer* MySizer = new wxBoxSizer( wxVERTICAL );

	wxStaticText* Text = new wxStaticText( MyPanel, wxID_ANY, *Name );
	MySizer->Add( Text, 4, wxEXPAND | wxALL, 0 );

	const TArray<FShowFlagData>& ShowFlags = GetShowFlagMenuItems();
	for ( INT i = 0 ; i < ShowFlags.Num() ; ++i )
	{
		const FShowFlagData& ShowFlagData = ShowFlags( i );
		if( ShowFlagData.Group == Group )
		{
			const UBOOL bShowFlagEnabled = ( ViewportClient->ShowFlags & ShowFlagData.Mask ) ? TRUE : FALSE;

			INT ID = ShowFlagData.ID + IDM_VIEWPORT_SHOWFLAGS_WINDOW_START;

			wxCheckBox *MyCheckBox = new wxCheckBox( MyPanel, ID, *ShowFlagData.LocalizedName );
			MyCheckBox->SetValue( bShowFlagEnabled == TRUE );

			ShowFlagsCheckBox* MySFCB = new ShowFlagsCheckBox;
			MySFCB->CheckBox = MyCheckBox;
			MySFCB->ID = ID;
			ShowFlagsCheckBoxes.AddItem( MySFCB );

			MySizer->Add( MyCheckBox, 0, wxEXPAND | wxALL, 0 );
		}
	}

	MySizer->SetSizeHints( this );
	MySizer->Fit( this );

	MyPanel->SetSizer( MySizer );
	MyPanel->Layout();

	return MyPanel;
}

ShowFlagsCheckBox* WxShowFlagsDialog::FindCheckBoxById( INT ID )
{
	for( INT i = 0; i < ShowFlagsCheckBoxes.Num(); i++ )
	{
		ShowFlagsCheckBox* CB = ShowFlagsCheckBoxes( i );
		if( CB->ID == ID )
		{
			return CB;
		}
	}
	return NULL;
}

void WxShowFlagsDialog::OnToggleShowFlagWindow( wxCommandEvent& In )
{
	INT ID = In.GetId();
	check( ID >= IDM_VIEWPORT_SHOWFLAGS_WINDOW_START && ID <= IDM_VIEWPORT_SHOWFLAGS_WINDOW_END );
	UBOOL bIsChecked = FindCheckBoxById( ID )->CheckBox->IsChecked();
	SetShowFlag( ViewportClient, ID - IDM_VIEWPORT_SHOWFLAGS_WINDOW_START, bIsChecked );
}

void WxShowFlagsDialog::OnShowDefaults( wxCommandEvent& In )
{
	// Setting show flags to the defaults should not stomp on the current viewmode settings.
	const EShowFlags ViewModeShowFlags = ViewportClient->ShowFlags &= SHOW_ViewMode_Mask;
	ViewportClient->ShowFlags = SHOW_DefaultEditor;
	ViewportClient->ShowFlags |= ViewModeShowFlags;

	SetInGameViewMode(FALSE);

	ViewportClient->LastShowFlags = ViewportClient->ShowFlags;

	UpdateUI();

	ViewportClient->Invalidate();
}

void WxShowFlagsDialog::UpdateUI()
{
//	ToggleTool( IDM_VIEWPORT_SHOW_GAMEVIEW, GetInGameViewMode() ? true : false );
//	ToggleTool( IDM_REALTIME, ViewportClient->IsRealtime()==TRUE );

//	UpdateToolBarButtonEnabledStates();
	for( INT i = 0; i < ShowFlagsCheckBoxes.Num(); i++ )
	{
		ShowFlagsCheckBox* CB = ShowFlagsCheckBoxes( i );
		bool bSet = CheckShowFlag( ViewportClient, CB->ID - IDM_VIEWPORT_SHOWFLAGS_WINDOW_START ) == TRUE;
		CB->CheckBox->SetValue( bSet );
	}
}

/**
 * Routes the event to the appropriate handlers
 *
 * @param InType the event that was fired
 */
void WxShowFlagsDialog::Send(ECallbackEventType InType)
{
	switch( InType )
	{
		case CALLBACK_ShowFlagsChanged:
		case CALLBACK_ViewportClientInvalidated:
			UpdateUI();
			break;
	}
}

WxShowFlagsDialog::~WxShowFlagsDialog( )
{
	ShowFlagsCheckBoxes.Empty();
}
