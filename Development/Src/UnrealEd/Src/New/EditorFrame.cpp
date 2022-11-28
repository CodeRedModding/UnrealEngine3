
#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "Kismet.h"
#include "..\..\engine\inc\EngineParticleClasses.h"
#include "..\..\engine\inc\EngineSequenceClasses.h"

#include "..\..\Launch\Resources\resource.h"

extern class WxUnrealEdApp* GApp;

BEGIN_EVENT_TABLE( WxEditorFrame, wxFrame )

	EVT_SIZE( WxEditorFrame::OnSize )
	EVT_MOVE( WxEditorFrame::OnMove )

	EVT_MENU( IDM_NEW, WxEditorFrame::MenuFileNew )
	EVT_MENU( IDM_OPEN, WxEditorFrame::MenuFileOpen )
	EVT_MENU( IDM_SAVE, WxEditorFrame::MenuFileSave )
	EVT_MENU( IDM_SAVE_AS, WxEditorFrame::MenuFileSaveAs )
	EVT_MENU( IDM_IMPORT_NEW, WxEditorFrame::MenuFileImportNew )
	EVT_MENU( IDM_IMPORT_MERGE, WxEditorFrame::MenuFileImportMerge )
	EVT_MENU( IDM_EXPORT_ALL, WxEditorFrame::MenuFileExportAll )
	EVT_MENU( IDM_EXPORT_SELECTED, WxEditorFrame::MenuFileExportSelected )
	EVT_MENU( IDM_EXIT, WxEditorFrame::MenuFileExit )
	EVT_MENU( IDM_UNDO, WxEditorFrame::MenuEditUndo )
	EVT_MENU( IDM_REDO, WxEditorFrame::MenuEditRedo )
	EVT_MENU( ID_EDIT_SHOW_WIDGET, WxEditorFrame::MenuEditShowWidget )
	EVT_MENU( ID_EDIT_TRANSLATE, WxEditorFrame::MenuEditTranslate )
	EVT_MENU( ID_EDIT_ROTATE, WxEditorFrame::MenuEditRotate )
	EVT_MENU( ID_EDIT_SCALE, WxEditorFrame::MenuEditScale )
	EVT_MENU( ID_EDIT_SCALE_NONUNIFORM, WxEditorFrame::MenuEditScaleNonUniform )
	EVT_COMBOBOX( IDCB_COORD_SYSTEM, WxEditorFrame::CoordSystemSelChanged )
	EVT_MENU( IDM_SEARCH, WxEditorFrame::MenuEditSearch )
	EVT_MENU( IDM_CUT, WxEditorFrame::MenuEditCut )
	EVT_MENU( IDM_COPY, WxEditorFrame::MenuEditCopy )
	EVT_MENU( IDM_PASTE, WxEditorFrame::MenuEditPasteOriginalLocation )
	EVT_MENU( IDM_PASTE_ORIGINAL_LOCATION, WxEditorFrame::MenuEditPasteOriginalLocation )
	EVT_MENU( IDM_PASTE_WORLD_ORIGIN, WxEditorFrame::MenuEditPasteWorldOrigin )
	EVT_MENU( IDM_PASTE_HERE, WxEditorFrame::MenuEditPasteHere )
	EVT_MENU( IDM_DUPLICATE, WxEditorFrame::MenuEditDuplicate )
	EVT_MENU( IDM_DELETE, WxEditorFrame::MenuEditDelete )
	EVT_MENU( IDM_SELECT_NONE, WxEditorFrame::MenuEditSelectNone )
	EVT_MENU( IDM_SELECT_ALL, WxEditorFrame::MenuEditSelectAll )
	EVT_MENU( IDM_SELECT_INVERT, WxEditorFrame::MenuEditSelectInvert )
	EVT_MENU( IDM_LOG, WxEditorFrame::MenuViewLogWindow )
	EVT_MENU( IDM_FULLSCREEN, WxEditorFrame::MenuViewFullScreen )
	EVT_MENU( IDM_BRUSHPOLYS, WxEditorFrame::MenuViewBrushPolys )
	EVT_MENU( ID_CAMSPEED_SLOW, WxEditorFrame::MenuViewCamSpeedSlow )
	EVT_MENU( ID_CAMSPEED_NORMAL, WxEditorFrame::MenuViewCamSpeedNormal )
	EVT_MENU( ID_CAMSPEED_FAST, WxEditorFrame::MenuViewCamSpeedFast )
	EVT_MENU( IDM_ACTOR_PROPERTIES, WxEditorFrame::MenuActorProperties )
	EVT_MENU( IDMENU_ActorPopupProperties, WxEditorFrame::MenuActorProperties )
	EVT_MENU( ID_SyncGenericBrowser, WxEditorFrame::MenuSyncGenericBrowser )
	EVT_MENU( IDMENU_FindActorInKismet, WxEditorFrame::MenuActorFindInKismet )
	EVT_MENU( IDM_SURFACE_PROPERTIES, WxEditorFrame::MenuSurfaceProperties )
	EVT_MENU( IDM_LEVEL_PROPERTIES, WxEditorFrame::MenuLevelProperties )
	EVT_MENU( IDM_BRUSH_ADD, WxEditorFrame::MenuBrushCSG )
	EVT_MENU( IDM_BRUSH_SUBTRACT, WxEditorFrame::MenuBrushCSG )
	EVT_MENU( IDM_BRUSH_INTERSECT, WxEditorFrame::MenuBrushCSG )
	EVT_MENU( IDM_BRUSH_DEINTERSECT, WxEditorFrame::MenuBrushCSG )
	EVT_MENU( IDM_BRUSH_ADD_ANTIPORTAL, WxEditorFrame::MenuBrushAddAntiPortal )
	EVT_MENU( IDM_BRUSH_ADD_SPECIAL, WxEditorFrame::MenuBrushAddSpecial )
	EVT_MENU( IDM_BUILD_PLAY, WxEditorFrame::MenuBuildPlay )
	EVT_MENU( IDM_BUILD_GEOMETRY, WxEditorFrame::MenuBuild )
	EVT_MENU( IDM_BUILD_LIGHTING, WxEditorFrame::MenuBuild )
	EVT_MENU( IDM_BUILD_AI_PATHS, WxEditorFrame::MenuBuild )
	EVT_MENU( IDM_BUILD_COVER, WxEditorFrame::MenuBuild )
	EVT_MENU( IDM_BUILD_ALL, WxEditorFrame::MenuBuild )
	EVT_MENU( IDM_BUILD_OPTIONS, WxEditorFrame::MenuBuildOptions )
	EVT_MENU_RANGE( IDM_BROWSER_START, IDM_BROWSER_END, WxEditorFrame::MenuViewShowBrowser )
	EVT_MENU_RANGE( IDM_MRU_START, IDM_MRU_END, WxEditorFrame::MenuFileMRU )
	EVT_MENU_RANGE( IDM_VIEWPORT_CONFIG_START, IDM_VIEWPORT_CONFIG_END, WxEditorFrame::MenuViewportConfig )
	EVT_MENU_RANGE( IDM_DRAG_GRID_START, IDM_DRAG_GRID_END, WxEditorFrame::MenuDragGrid )
	EVT_MENU_RANGE( ID_BackdropPopupGrid1, ID_BackdropPopupGrid1024, WxEditorFrame::MenuDragGrid )
	EVT_MENU_RANGE( IDM_ROTATION_GRID_START, IDM_ROTATION_GRID_END, WxEditorFrame::MenuRotationGrid )
	EVT_MENU( IDM_OPEN_KISMET, WxEditorFrame::MenuOpenKismet )
	EVT_MENU( WM_REDRAWALLVIEWPORTS, WxEditorFrame::MenuRedrawAllViewports )
	EVT_MENU( IDMN_ALIGN_WALL, WxEditorFrame::MenuAlignWall )
	EVT_MENU( IDMN_TOOL_CHECK_ERRORS, WxEditorFrame::MenuToolCheckErrors )
	EVT_MENU( ID_ReviewPaths, WxEditorFrame::MenuReviewPaths )
	EVT_MENU( ID_RotateActors, WxEditorFrame::MenuRotateActors )
	EVT_MENU( ID_ResetParticleEmitters, WxEditorFrame::MenuResetParticleEmitters )
	EVT_MENU( ID_EditSelectAllSurfs, WxEditorFrame::MenuSelectAllSurfs )
	EVT_MENU( ID_BrushAdd, WxEditorFrame::MenuBrushAdd )
	EVT_MENU( ID_BrushSubtract, WxEditorFrame::MenuBrushSubtract )
	EVT_MENU( ID_BrushIntersect, WxEditorFrame::MenuBrushIntersect )
	EVT_MENU( ID_BrushDeintersect, WxEditorFrame::MenuBrushDeintersect )
	EVT_MENU( ID_BrushAddAntiPortal, WxEditorFrame::MenuBrushAddAntiPortal )
	EVT_MENU( ID_BrushAddSpecial, WxEditorFrame::MenuBrushAddSpecial )
	EVT_MENU( ID_BrushOpen, WxEditorFrame::MenuBrushOpen )
	EVT_MENU( ID_BrushSaveAs, WxEditorFrame::MenuBrushSaveAs )
	EVT_MENU( ID_BRUSH_IMPORT, WxEditorFrame::MenuBrushImport )
	EVT_MENU( ID_BRUSH_EXPORT, WxEditorFrame::MenuBrushExport )
	EVT_MENU( ID_Tools2DEditor, WxEditorFrame::Menu2DShapeEditor )
	EVT_MENU( IDM_WIZARD_NEW_TERRAIN, WxEditorFrame::MenuWizardNewTerrain )	
	EVT_MENU( IDM_REPLACESKELMESHACTORS, WxEditorFrame::MenuReplaceSkelMeshActors )	
	EVT_MENU( IDMENU_ABOUTBOX, WxEditorFrame::MenuAboutBox )	
	EVT_MENU( ID_BackdropPopupAddClassHere, WxEditorFrame::MenuBackdropPopupAddClassHere )
	EVT_MENU_RANGE( ID_BackdropPopupAddLastSelectedClassHere_START, ID_BackdropPopupAddLastSelectedClassHere_END, WxEditorFrame::MenuBackdropPopupAddLastSelectedClassHere )
	EVT_MENU( ID_BackDropPopupPlayFromHere, WxEditorFrame::MenuPlayFromHere )
	EVT_MENU( ID_SurfPopupExtrude16, WxEditorFrame::MenuSurfPopupExtrude16 )
	EVT_MENU( ID_SurfPopupExtrude32, WxEditorFrame::MenuSurfPopupExtrude32 )
	EVT_MENU( ID_SurfPopupExtrude64, WxEditorFrame::MenuSurfPopupExtrude64 )
	EVT_MENU( ID_SurfPopupExtrude128, WxEditorFrame::MenuSurfPopupExtrude128 )
	EVT_MENU( ID_SurfPopupExtrude256, WxEditorFrame::MenuSurfPopupExtrude256 )
	EVT_MENU( ID_SurfPopupApplyMaterial, WxEditorFrame::MenuSurfPopupApplyMaterial )
	EVT_MENU( ID_SurfPopupAlignPlanarAuto, WxEditorFrame::MenuSurfPopupAlignPlanarAuto )
	EVT_MENU( ID_SurfPopupAlignPlanarWall, WxEditorFrame::MenuSurfPopupAlignPlanarWall )
	EVT_MENU( ID_SurfPopupAlignPlanarFloor, WxEditorFrame::MenuSurfPopupAlignPlanarFloor )
	EVT_MENU( ID_SurfPopupAlignBox, WxEditorFrame::MenuSurfPopupAlignBox )
	EVT_MENU( ID_SurfPopupAlignFace, WxEditorFrame::MenuSurfPopupAlignFace )
	EVT_MENU( ID_SurfPopupUnalign, WxEditorFrame::MenuSurfPopupUnalign )
	EVT_MENU( ID_SurfPopupSelectMatchingGroups, WxEditorFrame::MenuSurfPopupSelectMatchingGroups )
	EVT_MENU( ID_SurfPopupSelectMatchingItems, WxEditorFrame::MenuSurfPopupSelectMatchingItems )
	EVT_MENU( ID_SurfPopupSelectMatchingBrush, WxEditorFrame::MenuSurfPopupSelectMatchingBrush )
	EVT_MENU( ID_SurfPopupSelectMatchingTexture, WxEditorFrame::MenuSurfPopupSelectMatchingTexture )
	EVT_MENU( ID_SurfPopupSelectAllAdjacents, WxEditorFrame::MenuSurfPopupSelectAllAdjacents )
	EVT_MENU( ID_SurfPopupSelectAdjacentCoplanars, WxEditorFrame::MenuSurfPopupSelectAdjacentCoplanars )
	EVT_MENU( ID_SurfPopupSelectAdjacentWalls, WxEditorFrame::MenuSurfPopupSelectAdjacentWalls )
	EVT_MENU( ID_SurfPopupSelectAdjacentFloors, WxEditorFrame::MenuSurfPopupSelectAdjacentFloors )
	EVT_MENU( ID_SurfPopupSelectAdjacentSlants, WxEditorFrame::MenuSurfPopupSelectAdjacentSlants )
	EVT_MENU( ID_SurfPopupSelectReverse, WxEditorFrame::MenuSurfPopupSelectReverse )
	EVT_MENU( ID_SurfPopupMemorize, WxEditorFrame::MenuSurfPopupSelectMemorize )
	EVT_MENU( ID_SurfPopupRecall, WxEditorFrame::MenuSurfPopupRecall )
	EVT_MENU( ID_SurfPopupOr, WxEditorFrame::MenuSurfPopupOr )
	EVT_MENU( ID_SurfPopupAnd, WxEditorFrame::MenuSurfPopupAnd )
	EVT_MENU( ID_SurfPopupXor, WxEditorFrame::MenuSurfPopupXor )
	EVT_MENU( IDMENU_ActorPopupSelectAllClass, WxEditorFrame::MenuActorPopupSelectAllClass )
	EVT_MENU( IDMENU_ActorPopupSelectMatchingStaticMeshes, WxEditorFrame::MenuActorPopupSelectMatchingStaticMeshes )
	EVT_MENU( IDMENU_ActorPopupAlignCameras, WxEditorFrame::MenuActorPopupAlignCameras )
	EVT_MENU( IDMENU_ActorPopupLockMovement, WxEditorFrame::MenuActorPopupLockMovement )
	EVT_MENU( IDMENU_ActorPopupMerge, WxEditorFrame::MenuActorPopupMerge )
	EVT_MENU( IDMENU_ActorPopupSeparate, WxEditorFrame::MenuActorPopupSeparate )
	EVT_MENU( IDMENU_ActorPopupToFirst, WxEditorFrame::MenuActorPopupToFirst )
	EVT_MENU( IDMENU_ActorPopupToLast, WxEditorFrame::MenuActorPopupToLast )
	EVT_MENU( IDMENU_ActorPopupToBrush, WxEditorFrame::MenuActorPopupToBrush )
	EVT_MENU( IDMENU_ActorPopupFromBrush, WxEditorFrame::MenuActorPopupFromBrush )
	EVT_MENU( IDMENU_ActorPopupMakeAdd, WxEditorFrame::MenuActorPopupMakeAdd )
	EVT_MENU( IDMENU_ActorPopupMakeSubtract, WxEditorFrame::MenuActorPopupMakeSubtract )
	EVT_MENU( IDMENU_ActorPopupPathPosition, WxEditorFrame::MenuActorPopupPathPosition )
	EVT_MENU( IDMENU_ActorPopupPathRebuildSelected, WxEditorFrame::MenuActorPopupPathRebuildSelected )
	EVT_MENU( IDMENU_ActorPopupPathProscribe, WxEditorFrame::MenuActorPopupPathProscribe )
	EVT_MENU( IDMENU_ActorPopupPathForce, WxEditorFrame::MenuActorPopupPathForce )
	EVT_MENU( IDMENU_ActorPopupPathClearProscribed, WxEditorFrame::MenuActorPopupPathClearProscribed )
	EVT_MENU( IDMENU_ActorPopupPathClearForced, WxEditorFrame::MenuActorPopupPathClearForced )
	EVT_MENU( IDMENU_SnapToFloor, WxEditorFrame::MenuSnapToFloor )
	EVT_MENU( IDMENU_AlignToFloor, WxEditorFrame::MenuAlignToFloor )
	EVT_MENU( IDMENU_SaveBrushAsCollision, WxEditorFrame::MenuSaveBrushAsCollision )
	EVT_MENU( IDMENU_QuantizeVertices, WxEditorFrame::MenuQuantizeVertices )
	EVT_MENU( IDMENU_ConvertToStaticMesh, WxEditorFrame::MenuConvertToStaticMesh )
	EVT_MENU( IDMENU_ActorPopupResetPivot, WxEditorFrame::MenuActorPivotReset )
	EVT_MENU( ID_BackdropPopupPivot, WxEditorFrame::MenuActorPivotMoveHere )
	EVT_MENU( ID_BackdropPopupPivotSnapped, WxEditorFrame::MenuActorPivotMoveHereSnapped )
	EVT_MENU_RANGE( IDMENU_ActorFactory_Start, IDMENU_ActorFactory_End, WxEditorFrame::MenuUseActorFactory )
	EVT_MENU_RANGE( IDMENU_ActorFactoryAdv_Start, IDMENU_ActorFactoryAdv_End, WxEditorFrame::MenuUseActorFactoryAdv )
	EVT_MENU( IDMENU_ActorPopupMirrorX, WxEditorFrame::MenuActorMirrorX )
	EVT_MENU( IDMENU_ActorPopupMirrorY, WxEditorFrame::MenuActorMirrorY )
	EVT_MENU( IDMENU_ActorPopupMirrorZ, WxEditorFrame::MenuActorMirrorZ )
	EVT_COMMAND_RANGE( IDM_VolumeClasses_START, IDM_VolumeClasses_END, wxEVT_COMMAND_MENU_SELECTED, WxEditorFrame::OnAddVolumeClass )
	EVT_MENU( IDMENU_ActorPopupMakeSolid, WxEditorFrame::MenuActorPopupMakeSolid )
	EVT_MENU( IDMENU_ActorPopupMakeSemiSolid, WxEditorFrame::MenuActorPopupMakeSemiSolid )
	EVT_MENU( IDMENU_ActorPopupMakeNonSolid, WxEditorFrame::MenuActorPopupMakeNonSolid )
	EVT_MENU( IDMENU_ActorPopupSelectBrushesAdd, WxEditorFrame::MenuActorPopupBrushSelectAdd )
	EVT_MENU( IDMENU_ActorPopupSelectBrushesSubtract, WxEditorFrame::MenuActorPopupBrushSelectSubtract )
	EVT_MENU( IDMENU_ActorPopupSelectBrushesNonsolid, WxEditorFrame::MenuActorPopupBrushSelectNonSolid )
	EVT_MENU( IDMENU_ActorPopupSelectBrushesSemisolid, WxEditorFrame::MenuActorPopupBrushSelectSemiSolid )

	EVT_UPDATE_UI( IDM_UNDO, WxEditorFrame::UI_MenuEditUndo )
	EVT_UPDATE_UI( IDM_REDO, WxEditorFrame::UI_MenuEditRedo )
	EVT_UPDATE_UI( ID_EDIT_SHOW_WIDGET, WxEditorFrame::UI_MenuEditShowWidget )
	EVT_UPDATE_UI( ID_EDIT_TRANSLATE, WxEditorFrame::UI_MenuEditTranslate )
	EVT_UPDATE_UI( ID_EDIT_ROTATE, WxEditorFrame::UI_MenuEditRotate )
	EVT_UPDATE_UI( ID_EDIT_SCALE, WxEditorFrame::UI_MenuEditScale )
	EVT_UPDATE_UI( ID_EDIT_SCALE_NONUNIFORM, WxEditorFrame::UI_MenuEditScaleNonUniform )
	EVT_UPDATE_UI_RANGE( IDM_VIEWPORT_CONFIG_START, IDM_VIEWPORT_CONFIG_END, WxEditorFrame::UI_MenuViewportConfig )
	EVT_UPDATE_UI_RANGE( IDM_DRAG_GRID_START, IDM_DRAG_GRID_END, WxEditorFrame::UI_MenuDragGrid )
	EVT_UPDATE_UI_RANGE( ID_BackdropPopupGrid1, ID_BackdropPopupGrid1024, WxEditorFrame::UI_MenuDragGrid )
	EVT_UPDATE_UI_RANGE( IDM_ROTATION_GRID_START, IDM_ROTATION_GRID_END, WxEditorFrame::UI_MenuRotationGrid )
	EVT_UPDATE_UI( IDM_FULLSCREEN, WxEditorFrame::UI_MenuViewFullScreen )
	EVT_UPDATE_UI( IDM_BRUSHPOLYS, WxEditorFrame::UI_MenuViewBrushPolys )
	EVT_UPDATE_UI( ID_CAMSPEED_SLOW, WxEditorFrame::UI_MenuViewCamSpeedSlow )
	EVT_UPDATE_UI( ID_CAMSPEED_NORMAL, WxEditorFrame::UI_MenuViewCamSpeedNormal )
	EVT_UPDATE_UI( ID_CAMSPEED_FAST, WxEditorFrame::UI_MenuViewCamSpeedFast )
	
END_EVENT_TABLE()

WxEditorFrame::WxEditorFrame( wxWindow* InParent, wxWindowID InID, const FString& InTitle, const wxPoint& InPos = wxDefaultPosition, const wxSize& InSize = wxDefaultSize )
	: wxFrame( InParent, InID, *InTitle, InPos, InSize )
	, ViewportContainer( NULL )
{
	// Create all the status bars and set the default one.

	StatusBars[ SB_Standard ] = new WxStatusBarStandard;
	StatusBars[ SB_Standard ]->Create( this, -1 );
	StatusBars[ SB_Standard ]->Show( 0 );
	StatusBars[ SB_Progress ] = new WxStatusBarProgress;
	StatusBars[ SB_Progress ]->Create( this, -1 );
	StatusBars[ SB_Progress ]->Show( 0 );

	MainMenuBar = NULL;
	MainToolBar = NULL;
	DragGridMenu = NULL;
	RotationGridMenu = NULL;
	ButtonBar = NULL;
	ViewportConfigData = NULL;

	// Set default grid sizes, powers of two

	for( INT i = 0 ; i < FEditorConstraints::MAX_GRID_SIZES ; ++i )
	{
		GEditor->Constraints.GridSizes[i] = (float)(1 << i);
	}
}

WxEditorFrame::~WxEditorFrame()
{
	FileHelper.AskSaveChanges();

	// Save the viewport configuration to the INI file

	ViewportConfigData->SaveToINI();
	delete ViewportConfigData;

	DockingRoot->SaveConfig();

	//delete MainMenuBar;
	//delete MainToolBar;
	//delete AcceleratorTable;
	//delete DragGridMenu;
	//delete RotationGridMenu;
}

// Gives elements in the UI a chance to update themselves based on internal engine variables.

void WxEditorFrame::UpdateUI()
{
	// Left side button bar

	if( ButtonBar )
	{
		ButtonBar->UpdateUI();
	}

	// Viewport toolbars

	if( ViewportConfigData )
	{
		for( INT x = 0 ; x < 4 ; ++x )
		{
			if( ViewportConfigData->Viewports[x].bEnabled )
			{
				ViewportConfigData->Viewports[x].ViewportWindow->ToolBar->UpdateUI();
			}
		}
	}

	// Status bars

	if( StatusBars[ SB_Standard ] )
	{
		StatusBars[ SB_Standard ]->UpdateUI();
	}

	// Main toolbar

	if( MainToolBar )
	{
		MainToolBar->CoordSystemCombo->SetSelection( GEditorModeTools.CoordSystem );
	}
}

// Creates the child windows for the frame and sets everything to it's initial state.

void WxEditorFrame::SetUp()
{
	// Child windows that control the client area

	ViewportContainer = new wxWindow( (wxWindow*)this, IDW_VIEWPORT_CONTAINER );
	DockingRoot = new WxDockingRoot( (wxWindow*)this, IDW_DOCKING_ROOT );

	// Mode bar

	ModeBar = NULL;

	// Menus

	DragGridMenu = new WxDragGridMenu;
	RotationGridMenu = new WxRotationGridMenu;

	// Common resources

	WhitePlaceholder.Load( TEXT("WhitePlaceholder" ) );
	WizardB.Load( TEXT("Wizard") );
	DownArrowB.Load( TEXT("DownArrow") );
	MaterialEditor_RGBAB.Load( TEXT("MaterialEditor_RGBA") );
	MaterialEditor_RB.Load( TEXT("MaterialEditor_R") );
	MaterialEditor_GB.Load( TEXT("MaterialEditor_G") );
	MaterialEditor_BB.Load( TEXT("MaterialEditor_B") );
	MaterialEditor_AB.Load( TEXT("MaterialEditor_A") );
	MaterialEditor_ControlPanelFillB.Load( TEXT("MaterialEditor_ControlPanelFill") );
	MaterialEditor_ControlPanelCapB.Load( TEXT("MaterialEditor_ControlPanelCap") );
	LeftHandle.Load( TEXT("MaterialEditor_LeftHandle") );
	RightHandle.Load( TEXT("MaterialEditor_RightHandle") );
	RightHandleOn.Load( TEXT("MaterialEditor_RightHandleOn") );
	ArrowUp.Load( TEXT("UpArrowLarge") );
	ArrowDown.Load( TEXT("DownArrowLarge") );
	ArrowRight.Load( TEXT("RightArrowLarge") );

	// Load grid settings

	for( INT i = 0 ; i < FEditorConstraints::MAX_GRID_SIZES ; ++i )
	{
		FString Key = FString::Printf( TEXT("GridSize%d"), i );
		GConfig->GetFloat( TEXT("GridSizes"), *Key, GEditor->Constraints.GridSizes[i], GEditorIni );
	}

	// Viewport configuration options

	FViewportConfig_Template* Template;

	Template = new FViewportConfig_Template;
	Template->Set( VC_2_2_Split );
	ViewportConfigTemplates.AddItem( Template );

	Template = new FViewportConfig_Template;
	Template->Set( VC_1_2_Split );
	ViewportConfigTemplates.AddItem( Template );

	Template = new FViewportConfig_Template;
	Template->Set( VC_1_1_Split );
	ViewportConfigTemplates.AddItem( Template );

	// Main UI components

	MainMenuBar = new WxMainMenu;
	SetMenuBar( MainMenuBar );

	MainToolBar = new WxMainToolBar( (wxWindow*)this, -1 );
	SetToolBar( MainToolBar );

	MainMenuBar->MRUFiles->ReadINI();
	MainMenuBar->MRUFiles->UpdateMenu();

	StatusBars[ SB_Standard ]->SetUp();
	StatusBars[ SB_Progress ]->SetUp();

	SetStatusBar( SB_Standard );

	// Docking windows

	DockingRoot->LoadConfig();

	// Clean up

    wxSizeEvent Event;
	OnSize( Event );
}

// Changes the active status bar

void WxEditorFrame::SetStatusBar( EStatusBar InStatusBar )
{
	wxFrame::SetStatusBar( StatusBars[ InStatusBar ] );

	// Make all statusbars the same size as the SB_Standard one
	// FIXME : This is a bit of a hack as I suspect there must be a nice way of doing this, but I can't see it right now.

	wxRect rect = StatusBars[ SB_Standard ]->GetRect();
	for( INT x = 0 ; x < SB_Max ; ++x )
		StatusBars[ x ]->SetSize( rect );

	// Hide all status bars, except the active one

	for( INT x = 0 ; x < SB_Max ; ++x )
		StatusBars[ x ]->Show( x == InStatusBar );

	wxSafeYield();
}

void WxEditorFrame::SetMapFilename( FString InMapFilename )
{
	MapFilename = InMapFilename;
	RefreshCaption();
}

// Updates the applications caption text

void WxEditorFrame::RefreshCaption()
{
	FString Caption = *FString::Printf( TEXT("%s - UnrealEd"), (MapFilename.Len()?*MapFilename.GetBaseFilename():TEXT("Untitled")) );
	SetTitle( *Caption );
}

// Changes the viewport configuration

void WxEditorFrame::SetViewportConfig( EViewportConfig InConfig )
{
	FViewportConfig_Data* SaveConfig = ViewportConfigData;
	ViewportConfigData = new FViewportConfig_Data;

	SaveConfig->Save();

	ViewportContainer->DestroyChildren();

	ViewportConfigData->SetTemplate( InConfig );
	ViewportConfigData->Load( SaveConfig );

	// The user is changing the viewport config via the main menu. In this situation
	// we want to reset the splitter positions so they are in their default positions.

	GConfig->SetInt( TEXT("ViewportConfig"), TEXT("Splitter0"), 0, GEditorIni );
	GConfig->SetInt( TEXT("ViewportConfig"), TEXT("Splitter1"), 0, GEditorIni );
	GConfig->SetInt( TEXT("ViewportConfig"), TEXT("Splitter2"), 0, GEditorIni );
	GConfig->SetInt( TEXT("ViewportConfig"), TEXT("Splitter3"), 0, GEditorIni );

	ViewportConfigData->Apply( ViewportContainer );

	delete SaveConfig;
}

FGetInfoRet WxEditorFrame::GetInfo( ULevel* Level, INT Item )
{
	FGetInfoRet Ret;

	Ret.iValue = 0;
	Ret.String = TEXT("");

	// ACTORS
	if( Item & GI_NUM_SELECTED
			|| Item & GI_CLASSNAME_SELECTED
			|| Item & GI_CLASS_SELECTED )
	{
		INT NumActors = 0;
		BOOL bAnyClass = FALSE;
		UClass*	AllClass = NULL;

		for( INT i=0; i<Level->Actors.Num(); ++i )
		{
			if( Level->Actors(i) && GSelectionTools.IsSelected( Level->Actors(i) ) )
			{
				if( bAnyClass && Level->Actors(i)->GetClass() != AllClass )
					AllClass = NULL;
				else
					AllClass = Level->Actors(i)->GetClass();

				bAnyClass = TRUE;
				NumActors++;
			}
		}

		if( Item & GI_NUM_SELECTED )
		{
			Ret.iValue = NumActors;
		}
		if( Item & GI_CLASSNAME_SELECTED )
		{
			if( bAnyClass && AllClass )
				Ret.String = AllClass->GetName();
			else
				Ret.String = TEXT("Actor");
		}
		if( Item & GI_CLASS_SELECTED )
		{
			if( bAnyClass && AllClass )
				Ret.pClass = AllClass;
			else
				Ret.pClass = NULL;
		}
	}

	// SURFACES
	if( Item & GI_NUM_SURF_SELECTED)
	{
		INT NumSurfs = 0;

		for( INT i=0; i<Level->Model->Surfs.Num(); ++i )
		{
			FBspSurf *Poly = &Level->Model->Surfs(i);

			if( Poly->PolyFlags & PF_Selected )
			{
				NumSurfs++;
			}
		}

		if( Item & GI_NUM_SURF_SELECTED )
		{
			Ret.iValue = NumSurfs;
		}
	}

	return Ret;

}

// Initializes the classes for the generic dialog system (UOption* classes).  This should be called ONCE per
// editor instance.

void WxEditorFrame::OptionProxyInit()
{
	CREATE_OPTION_PROXY( OPTIONS_2DSHAPERSHEET, UOptions2DShaperSheet );
	CREATE_OPTION_PROXY( OPTIONS_2DSHAPEREXTRUDE, UOptions2DShaperExtrude );
	CREATE_OPTION_PROXY( OPTIONS_2DSHAPEREXTRUDETOPOINT, UOptions2DShaperExtrudeToPoint );
	CREATE_OPTION_PROXY( OPTIONS_2DSHAPEREXTRUDETOBEVEL, UOptions2DShaperExtrudeToBevel );
	CREATE_OPTION_PROXY( OPTIONS_2DSHAPERREVOLVE, UOptions2DShaperRevolve );
	CREATE_OPTION_PROXY( OPTIONS_2DSHAPERBEZIERDETAIL, UOptions2DShaperBezierDetail );
	CREATE_OPTION_PROXY( OPTIONS_DUPOBJECT, UOptionsDupObject );
	CREATE_OPTION_PROXY( OPTIONS_NEWCLASSFROMSEL, UOptionsNewClassFromSel );

}

void WxEditorFrame::OnSize( wxSizeEvent& InEvent )
{
	if( MainToolBar )
	{
		wxRect rc = GetClientRect();
		wxRect tbrc = MainToolBar->GetClientRect();
		INT ToolBarH = tbrc.GetHeight();

		wxRect rcmb( 0, 0, rc.GetWidth(), ModeBar ? ModeBar->GetRect().GetHeight() : 32 );

		if( rcmb.GetWidth() == 0 )
		{
			rcmb.width = 1000;
		}
		if( rcmb.GetHeight() == 0 )
		{
			rcmb.height = 32;
		}

		INT ModeBarH = 0;
		if( ModeBar )
		{
			ModeBar->SetSize( rcmb );
			ModeBarH = rcmb.GetHeight();
		}

		if( !ModeBarH && ModeBar && ModeBar->SavedHeight != -1 )
		{
			ModeBarH = ModeBar->SavedHeight;
			ModeBar->SetSize( rc.GetWidth(), ModeBarH );
		}

		ButtonBar->SetSize( 0, ModeBarH, LEFT_BUTTON_BAR_SZ, rc.GetHeight()-ModeBarH );
		DockingRoot->SetSize( LEFT_BUTTON_BAR_SZ, ModeBarH, rc.GetWidth() - LEFT_BUTTON_BAR_SZ, rc.GetHeight() );

		// Figure out the client area remaining for viewports once the docked windows are taken into account

		INT TopSz = DockingRoot->GetDockingDepth( DE_Top, DOCK_GUTTER_SZ ),
			RightSz = DockingRoot->GetDockingDepth( DE_Right, DOCK_GUTTER_SZ ),
			BottomSz = DockingRoot->GetDockingDepth( DE_Bottom, DOCK_GUTTER_SZ ),
			LeftSz = DockingRoot->GetDockingDepth( DE_Left, DOCK_GUTTER_SZ );

		ViewportContainer->SetSize( LEFT_BUTTON_BAR_SZ+LeftSz, TopSz+ModeBarH, rc.GetWidth() - LeftSz - RightSz, rc.GetHeight() - TopSz - BottomSz - ModeBarH );
		ViewportContainer->Layout();
	}
}

void WxEditorFrame::OnMove( wxMoveEvent& In )
{
	DockingRoot->LayoutDockedWindows();
	In.Skip();
}

void WxEditorFrame::MenuFileNew( wxCommandEvent& In )
{
	FileHelper.NewFile();

	SetMapFilename( TEXT("") );

	//if( GBrowserGroup )
	//	GBrowserGroup->RefreshGroupList();
}

void WxEditorFrame::MenuFileOpen( wxCommandEvent& In )
{
	FileHelper.Load();
}

void WxEditorFrame::MenuFileSave( wxCommandEvent& In )
{
	FileHelper.Save();
}

void WxEditorFrame::MenuFileSaveAs( wxCommandEvent& In )
{
	FileHelper.SaveAs();
}

void WxEditorFrame::MenuFileImportNew( wxCommandEvent& In )
{
	FileHelper.Import( 0 );
}

void WxEditorFrame::MenuFileImportMerge( wxCommandEvent& In )
{
	FileHelper.Import( 1 );
}

void WxEditorFrame::MenuFileExportAll( wxCommandEvent& In )
{
	FileHelper.Export( 0 );
}

void WxEditorFrame::MenuFileExportSelected( wxCommandEvent& In )
{
	FileHelper.Export( 1 );
}

void WxEditorFrame::MenuFileMRU( wxCommandEvent& In )
{
	if( MainMenuBar->MRUFiles->VerifyFile( In.GetId() - IDM_MRU_START ) )
	{
		FileHelper.AskSaveChanges();
		FileHelper.LoadFile( MainMenuBar->MRUFiles->Items( 0 ) );	// The file we wanted will now be the top item in the MRU list after calling VerifyFile.
	}
}

void WxEditorFrame::MenuFileExit( wxCommandEvent& In )
{
	Destroy();
}

void WxEditorFrame::MenuEditUndo( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("TRANSACTION UNDO") );
}

void WxEditorFrame::MenuEditRedo( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("TRANSACTION REDO") );
}

void WxEditorFrame::MenuEditShowWidget( wxCommandEvent& In )
{
	GEditorModeTools.SetShowWidget( !GEditorModeTools.GetShowWidget() );
	GUnrealEd->RedrawAllViewports( 1 );
}

void WxEditorFrame::MenuEditTranslate( wxCommandEvent& In )
{
	GEditorModeTools.SetWidgetMode( WM_Translate );
	GUnrealEd->RedrawAllViewports( 1 );
}

void WxEditorFrame::MenuEditRotate( wxCommandEvent& In )
{
	GEditorModeTools.SetWidgetMode( WM_Rotate );
	GUnrealEd->RedrawAllViewports( 1 );
}

void WxEditorFrame::MenuEditScale( wxCommandEvent& In )
{
	GEditorModeTools.SetWidgetMode( WM_Scale );
	GUnrealEd->RedrawAllViewports( 1 );
}

void WxEditorFrame::MenuEditScaleNonUniform( wxCommandEvent& In )
{
	GEditorModeTools.SetWidgetMode( WM_ScaleNonUniform );
	GUnrealEd->RedrawAllViewports( 1 );
}

void WxEditorFrame::CoordSystemSelChanged( wxCommandEvent& In )
{
	GEditorModeTools.CoordSystem = (ECoordSystem)In.GetInt();
	GUnrealEd->RedrawAllViewports( 1 );
}

void WxEditorFrame::MenuEditSearch( wxCommandEvent& In )
{
	GApp->DlgSearchActors->Show(1);
}

void WxEditorFrame::MenuEditCut( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("EDIT CUT") );
}

void WxEditorFrame::MenuEditCopy( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("EDIT COPY") );
}

void WxEditorFrame::MenuEditPasteOriginalLocation( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("EDIT PASTE") );
}

void WxEditorFrame::MenuEditPasteWorldOrigin( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("EDIT PASTE TO=ORIGIN") );
}

void WxEditorFrame::MenuEditPasteHere( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("EDIT PASTE TO=HERE") );
}

void WxEditorFrame::MenuEditDuplicate( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("DUPLICATE") );
}

void WxEditorFrame::MenuEditDelete( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("DELETE") );
}

void WxEditorFrame::MenuEditSelectNone( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("SELECT NONE") );
}

void WxEditorFrame::MenuEditSelectAll( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT ALL") );
}

void WxEditorFrame::MenuEditSelectInvert( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT INVERT") );
}

void WxEditorFrame::MenuViewLogWindow( wxCommandEvent& In )
{
	if( GLogConsole )
		GLogConsole->Show( TRUE );
}

void WxEditorFrame::MenuViewFullScreen( wxCommandEvent& In )
{
	ShowFullScreen( !IsFullScreen(), wxFULLSCREEN_NOBORDER | wxFULLSCREEN_NOCAPTION );
}

void WxEditorFrame::MenuViewBrushPolys( wxCommandEvent& In )
{
	GEditor->Exec( *FString::Printf( TEXT("MODE SHOWBRUSHMARKERPOLYS=%d"), !GEditor->bShowBrushMarkerPolys ) );
	GEditor->SaveConfig();
}

void WxEditorFrame::MenuViewCamSpeedSlow( wxCommandEvent& In )
{
	GEditor->MovementSpeed = MOVEMENTSPEED_SLOW;
}

void WxEditorFrame::MenuViewCamSpeedNormal( wxCommandEvent& In )
{
	GEditor->MovementSpeed = MOVEMENTSPEED_NORMAL;
}

void WxEditorFrame::MenuViewCamSpeedFast( wxCommandEvent& In )
{
	GEditor->MovementSpeed = MOVEMENTSPEED_FAST;
}

void WxEditorFrame::MenuViewportConfig( wxCommandEvent& In )
{
	EViewportConfig ViewportConfig = VC_None;

	switch( In.GetId() )
	{
		case IDM_VIEWPORT_CONFIG_2_2_SPLIT:	ViewportConfig = VC_2_2_Split;	break;
		case IDM_VIEWPORT_CONFIG_1_2_SPLIT:	ViewportConfig = VC_1_2_Split;	break;
		case IDM_VIEWPORT_CONFIG_1_1_SPLIT:	ViewportConfig = VC_1_1_Split;	break;
	}

	SetViewportConfig( ViewportConfig );
}

void WxEditorFrame::MenuViewShowBrowser( wxCommandEvent& In )
{
	switch( In.GetId() )
	{
		case IDM_BROWSER_ACTOR:			DockingRoot->ShowDockableWindow( DWT_ACTOR_BROWSER );		break;
		case IDM_BROWSER_GROUP:			DockingRoot->ShowDockableWindow( DWT_GROUP_BROWSER );		break;
		case IDM_BROWSER_LAYER:			DockingRoot->ShowDockableWindow( DWT_LAYER_BROWSER );		break;
		case IDM_BROWSER_GENERIC:		DockingRoot->ShowDockableWindow( DWT_GENERIC_BROWSER );		break;
	}
}

void WxEditorFrame::MenuOpenKismet( wxCommandEvent& In )
{
	WxKismet::OpenKismet(GUnrealEd);
}

void WxEditorFrame::MenuActorFindInKismet( wxCommandEvent& In )
{
	AActor* FindActor = GSelectionTools.GetTop<AActor>();
	if(FindActor)
	{
		WxKismet::FindActorReferences(FindActor);
	}
}

void WxEditorFrame::MenuActorProperties( wxCommandEvent& In )
{
	GUnrealEd->ShowActorProperties();
}

void WxEditorFrame::MenuSyncGenericBrowser( wxCommandEvent& In )
{
	WxGenericBrowser* gb = (WxGenericBrowser*)DockingRoot->FindDockableWindow(DWT_GENERIC_BROWSER);

	// If the user has a BSP surface selected, we sync to the material on it.  Otherwise,
	// we look for the first selected actor and sync to it's visual resource (static mesh, skeletal mesh, etc).

	UMaterialInstance* material = NULL;

	for( INT x = 0 ; x < GEngine->GetLevel()->Model->Surfs.Num() ; ++x )
	{
		if( GEngine->GetLevel()->Model->Surfs(x).PolyFlags & PF_Selected )
		{
			material = GEngine->GetLevel()->Model->Surfs(x).Material;
			if( material != NULL )
			{
				break;
			}
		}
	}

	if( material )
	{
		gb->SyncToObject( material );
	}
	else
	{
		for( TSelectedActorIterator It( GEditor->GetLevel() ) ; It ; ++It )
		{
			// Static meshes

			AStaticMeshActor* staticmesh = Cast<AStaticMeshActor>( *It );

			if( staticmesh )
			{
				gb->SyncToObject( staticmesh->StaticMeshComponent->StaticMesh );
				break;
			}
		}
	}

}

void WxEditorFrame::MenuSurfaceProperties( wxCommandEvent& In )
{
	GApp->DlgSurfProp->Show( 1 );
}

void WxEditorFrame::MenuLevelProperties( wxCommandEvent& In )
{
	GUnrealEd->ShowLevelProperties();
}

void WxEditorFrame::MenuBrushCSG( wxCommandEvent& In )
{
	switch( In.GetId() )
	{
		case IDM_BRUSH_ADD:				GUnrealEd->Exec( TEXT("BRUSH ADD") );					break;
		case IDM_BRUSH_SUBTRACT:		GUnrealEd->Exec( TEXT("BRUSH SUBTRACT") );				break;
		case IDM_BRUSH_INTERSECT:		GUnrealEd->Exec( TEXT("BRUSH FROM INTERSECTION") );		break;
		case IDM_BRUSH_DEINTERSECT:		GUnrealEd->Exec( TEXT("BRUSH FROM DEINTERSECTION") );	break;
	}

	GUnrealEd->RedrawLevel( GUnrealEd->Level );
}

void WxEditorFrame::MenuBrushAddAntiPortal( wxCommandEvent& In )
{
	GUnrealEd->Exec(TEXT("BRUSH ADDANTIPORTAL"));
	GUnrealEd->RedrawLevel( GUnrealEd->Level );
}

void WxEditorFrame::MenuBrushAddSpecial( wxCommandEvent& In )
{
	//GDlgAddSpecial->Show();
}

void WxEditorFrame::MenuBuildPlay( wxCommandEvent& In )
{
	GApp->CB_PlayMap();
}

#include "UnPath.h"

void WxEditorFrame::MenuBuild( wxCommandEvent& In )
{
	switch( In.GetId() )
	{
		case IDM_BUILD_GEOMETRY:
			((WPageOptions*)GApp->BuildSheet->PropSheet->Pages(0))->BuildGeometry();
			GApp->BuildSheet->PropSheet->RefreshPages();
			break;

		case IDM_BUILD_LIGHTING:
			((WPageOptions*)GApp->BuildSheet->PropSheet->Pages(0))->BuildLighting();
			GApp->BuildSheet->PropSheet->RefreshPages();
			break;

		case IDM_BUILD_AI_PATHS:
			((WPageOptions*)GApp->BuildSheet->PropSheet->Pages(0))->BuildPaths();
			GApp->BuildSheet->PropSheet->RefreshPages();
			break;

		case IDM_BUILD_COVER:
			//((WPageOptions*)GApp->BuildSheet->PropSheet->Pages(0))->BuildPaths();
			//GApp->BuildSheet->PropSheet->RefreshPages();
			{
				FPathBuilder builder;
				builder.buildCover(GUnrealEd->Level);
				GUnrealEd->RedrawAllViewports(1);;
			}
			break;

		case IDM_BUILD_ALL:
			((WPageOptions*)GApp->BuildSheet->PropSheet->Pages(0))->OnBuildClick();
			GApp->BuildSheet->PropSheet->RefreshPages();
			break;
	}
}

void WxEditorFrame::MenuBuildOptions( wxCommandEvent& In )
{
	GApp->BuildSheet->Show( TRUE );
}

void WxEditorFrame::UI_MenuViewportConfig( wxUpdateUIEvent& In )
{
	switch( In.GetId() )
	{
		case IDM_VIEWPORT_CONFIG_2_2_SPLIT:		In.Check( ViewportConfigData->Template == VC_2_2_Split );			break;
		case IDM_VIEWPORT_CONFIG_1_2_SPLIT:		In.Check( ViewportConfigData->Template == VC_1_2_Split );			break;
		case IDM_VIEWPORT_CONFIG_1_1_SPLIT:		In.Check( ViewportConfigData->Template == VC_1_1_Split );			break;
	}
}

void WxEditorFrame::UI_MenuEditUndo( wxUpdateUIEvent& In )
{
    In.Enable( GUnrealEd->Trans->CanUndo() ? true : false );
	In.SetText( *FString::Printf( TEXT("Undo : %s"), *GUnrealEd->Trans->GetUndoDesc() ) );

}

void WxEditorFrame::UI_MenuEditRedo( wxUpdateUIEvent& In )
{
    In.Enable( GUnrealEd->Trans->CanRedo() ? true : false );
	In.SetText( *FString::Printf( TEXT("Redo : %s"), *GUnrealEd->Trans->GetRedoDesc() ) );

}

void WxEditorFrame::UI_MenuEditShowWidget( wxUpdateUIEvent& In )
{
    In.Check( GEditorModeTools.GetShowWidget() ? true : false );
}

void WxEditorFrame::UI_MenuEditTranslate( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools.GetWidgetMode() == WM_Translate );
    In.Enable( GEditorModeTools.GetShowWidget() ? true : false );
}

void WxEditorFrame::UI_MenuEditRotate( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools.GetWidgetMode() == WM_Rotate );
    In.Enable( GEditorModeTools.GetShowWidget() ? true : false );
}

void WxEditorFrame::UI_MenuEditScale( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools.GetWidgetMode() == WM_Scale );
    In.Enable( GEditorModeTools.GetShowWidget() ? true : false );
}

void WxEditorFrame::UI_MenuEditScaleNonUniform( wxUpdateUIEvent& In )
{
	In.Check( GEditorModeTools.GetWidgetMode() == WM_ScaleNonUniform );

	// Special handling

	switch( GEditorModeTools.GetWidgetMode() )
	{
		// Non-uniform scaling is only possible in local space

		case WM_ScaleNonUniform:
			GEditorModeTools.CoordSystem = COORD_Local;
			MainToolBar->CoordSystemCombo->SetSelection( GEditorModeTools.CoordSystem );
			MainToolBar->CoordSystemCombo->Disable();
			break;

		default:
			MainToolBar->CoordSystemCombo->Enable();
			break;
	}

    In.Enable( GEditorModeTools.GetShowWidget() ? true : false );
}

void WxEditorFrame::UI_MenuDragGrid( wxUpdateUIEvent& In )
{
	INT id = In.GetId();

	if( IDM_DRAG_GRID_TOGGLE == id )
	{
		In.Check( GUnrealEd->Constraints.GridEnabled );
	}
	else 
	{
		INT GridIndex;

		GridIndex = -1;

		if( id >= IDM_DRAG_GRID_1 && id <= IDM_DRAG_GRID_1024 )
		{
			GridIndex = id - IDM_DRAG_GRID_1 ;
		}
		else if( id >= ID_BackdropPopupGrid1 && id <= ID_BackdropPopupGrid1024 )
		{
			GridIndex = id - ID_BackdropPopupGrid1 ;
		}

		if( GridIndex >= 0 && GridIndex < FEditorConstraints::MAX_GRID_SIZES )
		{
			float GridSize;

			GridSize = GEditor->Constraints.GridSizes[GridIndex];
			In.SetText( *FString::Printf(TEXT("%g"), GridSize ) ) ;
			In.Check( GUnrealEd->Constraints.CurrentGridSz == GridIndex );
		}
	}
}

void WxEditorFrame::UI_MenuRotationGrid( wxUpdateUIEvent& In )
{
	switch( In.GetId() )
	{
		case IDM_ROTATION_GRID_TOGGLE:	In.Check( GUnrealEd->Constraints.RotGridEnabled );	break;
		case IDM_ROTATION_GRID_512:		In.Check( GUnrealEd->Constraints.RotGridSize.Pitch == 512 );		break;
		case IDM_ROTATION_GRID_1024:	In.Check( GUnrealEd->Constraints.RotGridSize.Pitch == 1024 );		break;
		case IDM_ROTATION_GRID_4096:	In.Check( GUnrealEd->Constraints.RotGridSize.Pitch == 4096 );		break;
		case IDM_ROTATION_GRID_8192:	In.Check( GUnrealEd->Constraints.RotGridSize.Pitch == 8192 );		break;
		case IDM_ROTATION_GRID_16384:	In.Check( GUnrealEd->Constraints.RotGridSize.Pitch == 16384 );		break;
	}
}

void WxEditorFrame::MenuDragGrid( wxCommandEvent& In )
{
	INT id = In.GetId();

	if( IDM_DRAG_GRID_TOGGLE == id )
	{	
		GUnrealEd->Exec( *FString::Printf( TEXT("MODE GRID=%d"), !GUnrealEd->Constraints.GridEnabled ) );	
	}
	else 
	{
		INT GridIndex;

		GridIndex = -1;

		if( id >= IDM_DRAG_GRID_1 && id <= IDM_DRAG_GRID_1024 )
		{
			GridIndex = id - IDM_DRAG_GRID_1;
		}
		else if( id >= ID_BackdropPopupGrid1 && id <= ID_BackdropPopupGrid1024 )
		{
			GridIndex = id - ID_BackdropPopupGrid1;
		}

		if( GridIndex >= 0 && GridIndex < FEditorConstraints::MAX_GRID_SIZES )
		{
			GEditor->Constraints.SetGridSz( GridIndex );
		}
	}
}

void WxEditorFrame::MenuRotationGrid( wxCommandEvent& In )
{
	INT Angle = 0;
	switch( In.GetId() )
	{
		case IDM_ROTATION_GRID_512:			Angle = 512;		break;
		case IDM_ROTATION_GRID_1024:		Angle = 1024;		break;
		case IDM_ROTATION_GRID_4096:		Angle = 4096;		break;
		case IDM_ROTATION_GRID_8192:		Angle = 8192;		break;
		case IDM_ROTATION_GRID_16384:		Angle = 16384;		break;
	}

	switch( In.GetId() )
	{
		case IDM_ROTATION_GRID_TOGGLE:
			GUnrealEd->Exec( *FString::Printf( TEXT("MODE ROTGRID=%d"), !GUnrealEd->Constraints.RotGridEnabled ) );
			break;

		case IDM_ROTATION_GRID_512:
		case IDM_ROTATION_GRID_1024:
		case IDM_ROTATION_GRID_4096:
		case IDM_ROTATION_GRID_8192:
		case IDM_ROTATION_GRID_16384:
			GUnrealEd->Exec( *FString::Printf( TEXT("MAP ROTGRID PITCH=%d YAW=%d ROLL=%d"), Angle, Angle, Angle ) );
			break;
	}
}

void WxEditorFrame::UI_MenuViewFullScreen( wxUpdateUIEvent& In )
{
	In.Check( IsFullScreen() );
}

void WxEditorFrame::UI_MenuViewBrushPolys( wxUpdateUIEvent& In )
{
	In.Check( GEditor->bShowBrushMarkerPolys );
}

void WxEditorFrame::UI_MenuViewCamSpeedSlow( wxUpdateUIEvent& In )
{
	In.Check( GEditor->MovementSpeed == MOVEMENTSPEED_SLOW );
}

void WxEditorFrame::UI_MenuViewCamSpeedNormal( wxUpdateUIEvent& In )
{
	In.Check( GEditor->MovementSpeed == MOVEMENTSPEED_NORMAL );
}

void WxEditorFrame::UI_MenuViewCamSpeedFast( wxUpdateUIEvent& In )
{
	In.Check( GEditor->MovementSpeed == MOVEMENTSPEED_FAST );
}

void WxEditorFrame::MenuRedrawAllViewports( wxCommandEvent& In )
{
	GUnrealEd->RedrawLevel( GUnrealEd->Level );
	GUnrealEd->RedrawAllViewports( 1 );
}

void WxEditorFrame::MenuAlignWall( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY TEXALIGN WALL") );
}

void WxEditorFrame::MenuToolCheckErrors( wxCommandEvent& In )
{
	GUnrealEd->Exec(TEXT("MAP CHECK"));
}

void WxEditorFrame::MenuReviewPaths( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf(TEXT("PATHS REVIEW")));
}

void WxEditorFrame::MenuRotateActors( wxCommandEvent& In )
{
}

void WxEditorFrame::MenuResetParticleEmitters( wxCommandEvent& In )
{
}

void WxEditorFrame::MenuSelectAllSurfs( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT ALL") );
}

void WxEditorFrame::MenuBrushAdd( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("BRUSH ADD") );
	GUnrealEd->RedrawLevel( GUnrealEd->Level );
}

void WxEditorFrame::MenuBrushSubtract( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("BRUSH SUBTRACT") );
	GUnrealEd->RedrawLevel( GUnrealEd->Level );
}

void WxEditorFrame::MenuBrushIntersect( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("BRUSH FROM INTERSECTION") );
	GUnrealEd->RedrawLevel( GUnrealEd->Level );
}

void WxEditorFrame::MenuBrushDeintersect( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("BRUSH FROM DEINTERSECTION") );
	GUnrealEd->RedrawLevel( GUnrealEd->Level );
}

void WxEditorFrame::MenuBrushOpen( wxCommandEvent& In )
{
	OPENFILENAMEA ofn;
	char File[8192] = "\0";

	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = (HWND)GetHandle();
	ofn.lpstrFile = File;
	ofn.nMaxFile = sizeof(char) * 8192;
	ofn.lpstrFilter = "Brushes (*.u3d)\0*.u3d\0All Files\0*.*\0\0";
	ofn.lpstrInitialDir = "..\\Packages\\Maps";
	ofn.lpstrDefExt = "u3d";
	ofn.lpstrTitle = "Open Brush";
	ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

	// Display the Open dialog box.
	if( GetOpenFileNameA(&ofn) )
	{
		GUnrealEd->Exec( *FString::Printf(TEXT("BRUSH LOAD FILE=\"%s\""), ANSI_TO_TCHAR( File )));
		GUnrealEd->RedrawLevel( GUnrealEd->Level );
	}

	GFileManager->SetDefaultDirectory();
}

void WxEditorFrame::MenuBrushSaveAs( wxCommandEvent& In )
{
	OPENFILENAMEA ofn;
	char File[8192] = "\0";

	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = (HWND)GetHandle();
	ofn.lpstrFile = File;
	ofn.nMaxFile = sizeof(char) * 8192;
	ofn.lpstrFilter = "Brushes (*.u3d)\0*.u3d\0All Files\0*.*\0\0";
	ofn.lpstrInitialDir = "..\\Packages\\Maps";
	ofn.lpstrDefExt = "u3d";
	ofn.lpstrTitle = "Save Brush";
	ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

	if( GetSaveFileNameA(&ofn) )
		GUnrealEd->Exec( *FString::Printf(TEXT("BRUSH SAVE FILE=\"%s\""), ANSI_TO_TCHAR( File )));

	GFileManager->SetDefaultDirectory();
}

void WxEditorFrame::MenuBrushImport( wxCommandEvent& In )
{
	OPENFILENAMEA ofn;
	char File[8192] = "\0";

	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = (HWND)GetHandle();
	ofn.lpstrFile = File;
	ofn.nMaxFile = sizeof(char) * 8192;
	ofn.lpstrFilter = "Import Types (*.t3d, *.dxf, *.asc, *.ase)\0*.t3d;*.dxf;*.asc;*.ase;\0All Files\0*.*\0\0";
	ofn.lpstrInitialDir = TCHAR_TO_ANSI( *(GApp->LastDir[LD_BRUSH]) );
	ofn.lpstrDefExt = "t3d";
	ofn.lpstrTitle = "Import Brush";
	ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

	// Display the Open dialog box.
	if( GetOpenFileNameA(&ofn) )
	{
		WxDlgImportBrush dlg;
		dlg.ShowModal( ANSI_TO_TCHAR( File ) );

		GUnrealEd->RedrawLevel( GUnrealEd->Level );

		FString S = File;
		GApp->LastDir[LD_BRUSH] = S.Left( S.InStr( TEXT("\\"), 1 ) );
	}

	GFileManager->SetDefaultDirectory();
}

void WxEditorFrame::MenuBrushExport( wxCommandEvent& In )
{
	OPENFILENAMEA ofn;
	char File[8192] = "\0";

	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = (HWND)GetHandle();
	ofn.lpstrFile = File;
	ofn.nMaxFile = sizeof(char) * 8192;
	ofn.lpstrFilter = "Unreal Text (*.t3d)\0*.t3d\0All Files\0*.*\0\0";
	ofn.lpstrInitialDir = TCHAR_TO_ANSI( *(GApp->LastDir[LD_BRUSH]) );
	ofn.lpstrDefExt = "t3d";
	ofn.lpstrTitle = "Export Brush";
	ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

	if( GetSaveFileNameA(&ofn) )
	{
		GUnrealEd->Exec( *FString::Printf(TEXT("BRUSH EXPORT FILE=\"%s\""), ANSI_TO_TCHAR( File )));

		FString S = File;
		GApp->LastDir[LD_BRUSH] = S.Left( S.InStr( TEXT("\\"), 1 ) );
	}

	GFileManager->SetDefaultDirectory();
}

void WxEditorFrame::Menu2DShapeEditor( wxCommandEvent& In )
{
	delete GApp->TwoDeeShapeEditor;

	GApp->TwoDeeShapeEditor = new W2DShapeEditor( TEXT("2D Shape Editor"), GApp->OldEditorFrame );
	GApp->TwoDeeShapeEditor->OpenWindow();
}

void WxEditorFrame::MenuWizardNewTerrain( wxCommandEvent& In )
{
	WxWizard_NewTerrain wiz( this );
	wiz.RunWizard();
}

void WxEditorFrame::MenuAboutBox( wxCommandEvent& In )
{
	WxDlgAbout dlg;
	dlg.ShowModal();
}

void WxEditorFrame::MenuBackdropPopupAddClassHere( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("ACTOR ADD CLASS=%s"), GSelectionTools.GetTop<UClass>()->GetName() ) );
}

void WxEditorFrame::MenuBackdropPopupAddLastSelectedClassHere( wxCommandEvent& In )
{
	INT idx = In.GetId() - ID_BackdropPopupAddLastSelectedClassHere_START;

	UClass* Class = GSelectionTools.SelectedClasses( idx );
	UActorFactory* ActorFactory = GEditor->FindActorFactory( Class );
	
	if( ActorFactory )
	{
		GEditor->UseActorFactory(ActorFactory);
	}
	else
	{
		GUnrealEd->Exec( *FString::Printf( TEXT("ACTOR ADD CLASS=%s"), Class->GetName() ) );
	}
}

void WxEditorFrame::MenuSurfPopupExtrude16( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY EXTRUDE DEPTH=16") );
}

void WxEditorFrame::MenuSurfPopupExtrude32( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY EXTRUDE DEPTH=32") );
}

void WxEditorFrame::MenuSurfPopupExtrude64( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY EXTRUDE DEPTH=64") );
}

void WxEditorFrame::MenuSurfPopupExtrude128( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY EXTRUDE DEPTH=128") );
}

void WxEditorFrame::MenuSurfPopupExtrude256( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY EXTRUDE DEPTH=256") );
}

void WxEditorFrame::MenuSurfPopupApplyMaterial( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SETMATERIAL") );
}

void WxEditorFrame::MenuSurfPopupAlignPlanarAuto( wxCommandEvent& In )
{
	GTexAlignTools.GetAligner( TEXALIGN_PlanarAuto )->Align( TEXALIGN_PlanarAuto, GUnrealEd->Level->Model );
}

void WxEditorFrame::MenuSurfPopupAlignPlanarWall( wxCommandEvent& In )
{
	GTexAlignTools.GetAligner( TEXALIGN_PlanarWall )->Align( TEXALIGN_PlanarWall, GUnrealEd->Level->Model );
}

void WxEditorFrame::MenuSurfPopupAlignPlanarFloor( wxCommandEvent& In )
{
	GTexAlignTools.GetAligner( TEXALIGN_PlanarFloor )->Align( TEXALIGN_PlanarFloor, GUnrealEd->Level->Model );
}

void WxEditorFrame::MenuSurfPopupAlignBox( wxCommandEvent& In )
{
	GTexAlignTools.GetAligner( TEXALIGN_Box )->Align( TEXALIGN_Box, GUnrealEd->Level->Model );
}

void WxEditorFrame::MenuSurfPopupAlignFace( wxCommandEvent& In )
{
	GTexAlignTools.GetAligner( TEXALIGN_Face )->Align( TEXALIGN_Face, GUnrealEd->Level->Model );
}

void WxEditorFrame::MenuSurfPopupUnalign( wxCommandEvent& In )
{
	GTexAlignTools.GetAligner( TEXALIGN_Default )->Align( TEXALIGN_Default, GUnrealEd->Level->Model );
}

void WxEditorFrame::MenuSurfPopupSelectMatchingGroups( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MATCHING GROUPS") );
}

void WxEditorFrame::MenuSurfPopupSelectMatchingItems( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MATCHING ITEMS") );
}

void WxEditorFrame::MenuSurfPopupSelectMatchingBrush( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MATCHING BRUSH") );
}

void WxEditorFrame::MenuSurfPopupSelectMatchingTexture( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MATCHING TEXTURE") );
}

void WxEditorFrame::MenuSurfPopupSelectAllAdjacents( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT ADJACENT ALL") );
}

void WxEditorFrame::MenuSurfPopupSelectAdjacentCoplanars( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT ADJACENT COPLANARS") );
}

void WxEditorFrame::MenuSurfPopupSelectAdjacentWalls( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT ADJACENT WALLS") );
}

void WxEditorFrame::MenuSurfPopupSelectAdjacentFloors( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT ADJACENT FLOORS") );
}

void WxEditorFrame::MenuSurfPopupSelectAdjacentSlants( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT ADJACENT SLANTS") );
}

void WxEditorFrame::MenuSurfPopupSelectReverse( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT REVERSE") );
}

void WxEditorFrame::MenuSurfPopupSelectMemorize( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MEMORY SET") );
}

void WxEditorFrame::MenuSurfPopupRecall( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MEMORY RECALL") );
}

void WxEditorFrame::MenuSurfPopupOr( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MEMORY INTERSECTION") );
}

void WxEditorFrame::MenuSurfPopupAnd( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MEMORY UNION") );
}

void WxEditorFrame::MenuSurfPopupXor( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY SELECT MEMORY XOR") );
}

void WxEditorFrame::MenuActorPopupSelectAllClass( wxCommandEvent& In )
{
	FGetInfoRet gir = GetInfo( GUnrealEd->Level, GI_NUM_SELECTED | GI_CLASSNAME_SELECTED );

	if( gir.iValue )
	{
		GUnrealEd->Exec( *FString::Printf( TEXT("ACTOR SELECT OFCLASS CLASS=%s"), *gir.String ) );
	}
}

void WxEditorFrame::MenuActorPopupSelectMatchingStaticMeshes( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR SELECT MATCHINGSTATICMESH") );
}

void WxEditorFrame::MenuActorPopupAlignCameras( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("CAMERA ALIGN") );
}

void WxEditorFrame::MenuActorPopupLockMovement( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR TOGGLE LOCKMOVEMENT") );
}

void WxEditorFrame::MenuActorPopupMerge( wxCommandEvent& In )
{
	GUnrealEd->Exec(TEXT("BRUSH MERGEPOLYS"));
}

void WxEditorFrame::MenuActorPopupSeparate( wxCommandEvent& In )
{
	GUnrealEd->Exec(TEXT("BRUSH SEPARATEPOLYS"));
}

void WxEditorFrame::MenuActorPopupToFirst( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP SENDTO FIRST") );
}

void WxEditorFrame::MenuActorPopupToLast( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP SENDTO LAST") );
}

void WxEditorFrame::MenuActorPopupToBrush( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP BRUSH GET") );
}

void WxEditorFrame::MenuActorPopupFromBrush( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP BRUSH PUT") );
}

void WxEditorFrame::MenuActorPopupMakeAdd( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf(TEXT("MAP SETBRUSH CSGOPER=%d"), (INT)CSG_Add) );
}

void WxEditorFrame::MenuActorPopupMakeSubtract( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf(TEXT("MAP SETBRUSH CSGOPER=%d"), (INT)CSG_Subtract) );
}

void WxEditorFrame::MenuSnapToFloor( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR ALIGN SNAPTOFLOOR ALIGN=0") );
}

void WxEditorFrame::MenuAlignToFloor( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR ALIGN SNAPTOFLOOR ALIGN=1") );
}

void WxEditorFrame::MenuSaveBrushAsCollision( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("STATICMESH SAVEBRUSHASCOLLISION") );
}

void WxEditorFrame::MenuQuantizeVertices( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR ALIGN") );
}

void WxEditorFrame::MenuConvertToStaticMesh( wxCommandEvent& In )
{
	WxDlgPackageGroupName dlg;

	WxGenericBrowser* gb = (WxGenericBrowser*)DockingRoot->FindDockableWindow(DWT_GENERIC_BROWSER);

	UPackage* pkg = gb->GetTopPackage();
	UPackage* grp = gb->GetGroup();

	FString PackageName = pkg ? pkg->GetName() : TEXT("MyPackage");
	FString GroupName = grp ? grp->GetName() : TEXT("");

	if( dlg.ShowModal( PackageName, GroupName, TEXT("MyMesh") ) == wxID_OK )
	{
		FString Wk = FString::Printf( TEXT("STATICMESH FROM SELECTION PACKAGE=%s"), *dlg.Package );
		if( dlg.Group.Len() > 0 )
		{
			Wk += FString::Printf( TEXT(" GROUP=%s"), *dlg.Group );
		}
		Wk += FString::Printf( TEXT(" NAME=%s"), *dlg.Name );

		GUnrealEd->Exec( *Wk );
	}
}

void WxEditorFrame::MenuActorPivotReset( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR RESET PIVOT") );
}

void WxEditorFrame::MenuActorMirrorX( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR MIRROR X=-1") );
}

void WxEditorFrame::MenuActorMirrorY( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR MIRROR Y=-1") );
}

void WxEditorFrame::MenuActorMirrorZ( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("ACTOR MIRROR Z=-1") );
}

void WxEditorFrame::MenuActorPopupMakeSolid( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("MAP SETBRUSH CLEARFLAGS=%d SETFLAGS=%d"), PF_Semisolid + PF_NotSolid, 0 ) );
}

void WxEditorFrame::MenuActorPopupMakeSemiSolid( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("MAP SETBRUSH CLEARFLAGS=%d SETFLAGS=%d"), PF_Semisolid + PF_NotSolid, PF_Semisolid ) );
}

void WxEditorFrame::MenuActorPopupMakeNonSolid( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf( TEXT("MAP SETBRUSH CLEARFLAGS=%d SETFLAGS=%d"), PF_Semisolid + PF_NotSolid, PF_NotSolid ) );
}

void WxEditorFrame::MenuActorPopupBrushSelectAdd( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP SELECT ADDS") );
}

void WxEditorFrame::MenuActorPopupBrushSelectSubtract( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP SELECT SUBTRACTS") );
}

void WxEditorFrame::MenuActorPopupBrushSelectNonSolid( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP SELECT NONSOLIDS") );
}

void WxEditorFrame::MenuActorPopupBrushSelectSemiSolid( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("MAP SELECT SEMISOLIDS") );
}

/**
 * Forces all selected navigation points to position themselves as though building
 * paths (ie FindBase).
 */
void WxEditorFrame::MenuActorPopupPathPosition( wxCommandEvent& In )
{
	TArray<ANavigationPoint*> navPts;
	GetSelectedObjects<ANavigationPoint>(navPts);
	for (INT idx = 0; idx < navPts.Num(); idx++)
	{
		navPts(idx)->FindBase();
	}
}

/**
 * Rebuilds all selected navigation points.
 */
void WxEditorFrame::MenuActorPopupPathRebuildSelected( wxCommandEvent& In )
{
	// set bPathChanged for all selected nodes
	TArray<ANavigationPoint*> navPts;
	for (TObjectIterator<ANavigationPoint> It; It; ++It)
	{
		It->bPathsChanged = It->GetFlags() & RF_EdSelected;
		navPts.AddItem(*It);
	}
	// also rebuild nodes within distance of selected nodes
	TArray<ANavigationPoint*> selectedPts;
	GetSelectedObjects<ANavigationPoint>(selectedPts);
	for (INT idx = 0; idx < selectedPts.Num(); idx++)
	{
		for (INT navIdx = 0; navIdx < navPts.Num(); navIdx++)
		{
			if (!navPts(navIdx)->bPathsChanged &&
				(navPts(navIdx)->Location - selectedPts(idx)->Location).SizeSquared() <= MAXPATHDISTSQ)
			{
				navPts(navIdx)->bPathsChanged = 1;
			}
		}
	}
	// and rebuild changed paths
	FPathBuilder builder;
	builder.definePaths(GUnrealEd->Level,1);
}

/**
 * Proscribes a path between all selected nodes.
 */
void WxEditorFrame::MenuActorPopupPathProscribe( wxCommandEvent& In )
{
	TArray<ANavigationPoint*> navPts;
	GetSelectedObjects<ANavigationPoint>(navPts);
	for (INT idx = 0; idx < navPts.Num(); idx++)
	{
		for (INT proscribeIdx = 0; proscribeIdx < navPts.Num(); proscribeIdx++)
		{
			if (proscribeIdx != idx)
			{
				if (!navPts(idx)->ProscribedPaths.ContainsItem(navPts(proscribeIdx)))
				{
					// add to the list
					navPts(idx)->ProscribedPaths.AddItem(navPts(proscribeIdx));
					// check to see if we're breaking an existing path
					UReachSpec *spec = navPts(idx)->GetReachSpecTo(navPts(proscribeIdx));
					if (spec != NULL)
					{
						// mark the spec as proscribed
						spec->reachFlags = R_PROSCRIBED;
					}
					else
					{
						// otherwise create a new proscribed spec
						navPts(idx)->ProscribePathTo(navPts(proscribeIdx));
					}
				}
			}
		}
	}
}

/**
 * Forces a path between all selected nodes.
 */
void WxEditorFrame::MenuActorPopupPathForce( wxCommandEvent& In )
{
	TArray<ANavigationPoint*> navPts;
	GetSelectedObjects<ANavigationPoint>(navPts);
	for (INT idx = 0; idx < navPts.Num(); idx++)
	{
		for (INT forceIdx = 0; forceIdx < navPts.Num(); forceIdx++)
		{
			if (forceIdx != idx)
			{
				// if not already forced
				if (!navPts(idx)->ForcedPaths.ContainsItem(navPts(forceIdx)))
				{
					// add to the list
					navPts(idx)->ForcedPaths.AddItem(navPts(forceIdx));
					UReachSpec *spec = navPts(idx)->GetReachSpecTo(navPts(forceIdx));
					if (spec != NULL)
					{
						spec->reachFlags = R_FORCED;
					}
					else
					{
						// and create a new spec
						navPts(idx)->ForcePathTo(navPts(forceIdx));
					}
				}
			}
		}
	}
}

/**
 * Clears all proscribed paths between selected nodes, or if just one node
 * is selected, clears all of it's proscribed paths.
 */
void WxEditorFrame::MenuActorPopupPathClearProscribed( wxCommandEvent& In )
{
	TArray<ANavigationPoint*> navPts;
	GetSelectedObjects<ANavigationPoint>(navPts);
	if (navPts.Num() == 1)
	{
		navPts(0)->ProscribedPaths.Empty();
		// remove any proscribed specs
		for (INT idx = 0; idx < navPts(0)->PathList.Num(); idx++)
		{
			if (navPts(0)->PathList(idx) != NULL &&
				navPts(0)->PathList(idx)->reachFlags & R_PROSCRIBED)
			{
				navPts(0)->PathList.Remove(idx--,1);
			}
		}
	}
	else
	{
		// clear any proscribed points between the selected nodes
		for (INT idx = 0; idx < navPts.Num(); idx++)
		{
			for (INT proscribeIdx = 0; proscribeIdx < navPts.Num(); proscribeIdx++)
			{
				if (proscribeIdx != idx)
				{
					navPts(idx)->ProscribedPaths.RemoveItem(navPts(proscribeIdx));
					// remove any proscribed specs to the nav
					for (INT specIdx = 0; specIdx < navPts(idx)->PathList.Num(); specIdx++)
					{
						if (navPts(idx)->PathList(specIdx) != NULL &&
							navPts(idx)->PathList(specIdx)->End == navPts(proscribeIdx) &&
							navPts(idx)->PathList(specIdx)->reachFlags & R_PROSCRIBED)
						{
							navPts(idx)->PathList.Remove(specIdx--,1);
						}
					}
				}
			}
		}
	}
}

/**
 * Clears all forced paths between selected nodes, or if just one node
 * is slected, clears all of it's forced paths.
 */
void WxEditorFrame::MenuActorPopupPathClearForced( wxCommandEvent& In )
{
	TArray<ANavigationPoint*> navPts;
	GetSelectedObjects<ANavigationPoint>(navPts);
	if (navPts.Num() == 1)
	{
		navPts(0)->ForcedPaths.Empty();
		// remove any forced specs
		for (INT idx = 0; idx < navPts(0)->PathList.Num(); idx++)
		{
			if (navPts(0)->PathList(idx) != NULL &&
				navPts(0)->PathList(idx)->reachFlags & R_FORCED)
			{
				navPts(0)->PathList.Remove(idx--,1);
			}
		}
	}
	else
	{
		// clear any forced points between the selected nodes
		for (INT idx = 0; idx < navPts.Num(); idx++)
		{
			for (INT forceIdx = 0; forceIdx < navPts.Num(); forceIdx++)
			{
				if (forceIdx != idx)
				{
					navPts(idx)->ForcedPaths.RemoveItem(navPts(forceIdx));
					// remove any forced specs to the nav
					for (INT specIdx = 0; specIdx < navPts(idx)->PathList.Num(); specIdx++)
					{
						if (navPts(idx)->PathList(specIdx) != NULL &&
							navPts(idx)->PathList(specIdx)->End == navPts(forceIdx) &&
							navPts(idx)->PathList(specIdx)->reachFlags & R_FORCED)
						{
							navPts(idx)->PathList.Remove(specIdx--,1);
						}
					}
				}
			}
		}
	}
}

void WxEditorFrame::OnAddVolumeClass( wxCommandEvent& In )
{
	INT sel = In.GetId() - IDM_VolumeClasses_START;

	UClass* Class = NULL;

	INT idx = 0;
	for( TObjectIterator<UClass> It ; It ; ++It )
	{
		if( It->IsChildOf(AVolume::StaticClass()) )
		{
			if( idx == sel )
			{
				Class = *It;
				break;
			}
			idx++;
		}
	}

	if( Class )
	{
		GUnrealEd->Exec( *FString::Printf( TEXT("BRUSH ADDVOLUME CLASS=%s"), Class->GetName() ) );
	}
}

void WxEditorFrame::MenuActorPivotMoveHere( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("PIVOT HERE") );
}

void WxEditorFrame::MenuActorPivotMoveHereSnapped( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("PIVOT SNAPPED") );
}

void WxEditorFrame::MenuPlayFromHere( wxCommandEvent& In )
{
	UCylinderComponent*	DefaultCollisionComponent = CastChecked<UCylinderComponent>(APlayerStart::StaticClass()->GetDefaultActor()->CollisionComponent);
	FVector				CollisionExtent = FVector(DefaultCollisionComponent->CollisionRadius,DefaultCollisionComponent->CollisionRadius,DefaultCollisionComponent->CollisionHeight),
						StartLocation = GEditor->ClickLocation + GEditor->ClickPlane * (FBoxPushOut(GEditor->ClickPlane,CollisionExtent) + 0.1f);
	GEditor->PlayMap(&StartLocation);
}

void WxEditorFrame::MenuUseActorFactory( wxCommandEvent& In )
{
	INT ActorFactoryIndex = In.GetId() - IDMENU_ActorFactory_Start;
	check( ActorFactoryIndex >= 0 && ActorFactoryIndex < GEditor->ActorFactories.Num() );

	UActorFactory* ActorFactory = GEditor->ActorFactories(ActorFactoryIndex);
	
	GEditor->UseActorFactory(ActorFactory);
}

void WxEditorFrame::MenuUseActorFactoryAdv( wxCommandEvent& In )
{
	INT ActorFactoryIndex = In.GetId() - IDMENU_ActorFactoryAdv_Start;
	check( ActorFactoryIndex >= 0 && ActorFactoryIndex < GEditor->ActorFactories.Num() );

	UActorFactory* ActorFactory = GEditor->ActorFactories(ActorFactoryIndex);

	// Have a first stab at filling in the factory properties.
	ActorFactory->AutoFillFields();

	// Then display dialog to let user see.adjust them.
	WxDlgActorFactory dlg;
	if( dlg.ShowModal(ActorFactory) == wxID_OK )
	{
		if( ActorFactory->CanCreateActor() )
			GEditor->UseActorFactory(ActorFactory);
		else
			appMsgf( 0, TEXT("Sorry - could not create Actor.") );
	}
}

void WxEditorFrame::MenuReplaceSkelMeshActors(  wxCommandEvent& In )
{
	ULevel* Level = GEditor->Level;

	TArray<ASkeletalMeshActor*> NewSMActors;

	for(INT i=0; i<Level->Actors.Num(); i++)
	{
		ASkeletalMeshActor* SMActor = Cast<ASkeletalMeshActor>( Level->Actors(i) );
		if( SMActor && !NewSMActors.ContainsItem(SMActor) )
		{
			USkeletalMesh* SkelMesh = SMActor->SkeletalMeshComponent->SkeletalMesh;
			FVector Location = SMActor->Location;
			FRotator Rotation = SMActor->Rotation;
			FName AttachTag = SMActor->AttachTag;

			UAnimSet* AnimSet = NULL; 
			if(SMActor->SkeletalMeshComponent->AnimSets.Num() > 0)
			{
				AnimSet = SMActor->SkeletalMeshComponent->AnimSets(0);
			}

			// Find any objects in Kismet that reference this SkeletalMeshActor
			TArray<USequenceObject*> SeqVars;
			if(Level->GameSequence)
			{
				Level->GameSequence->FindSeqObjectsByObjectName(SMActor->GetFName(), SeqVars);
			}

			Level->DestroyActor(SMActor);
			SMActor = NULL;

			ASkeletalMeshActor* NewSMActor = CastChecked<ASkeletalMeshActor>( Level->SpawnActor( ASkeletalMeshActor::StaticClass(), NAME_None, Location, Rotation, NULL ) );

			// Set up the SkeletalMeshComponent based on the old one.
			NewSMActor->ClearComponents();

			NewSMActor->SkeletalMeshComponent->SkeletalMesh = SkelMesh;

			if(AnimSet)
			{
				NewSMActor->SkeletalMeshComponent->AnimSets.AddItem( AnimSet );
			}

			NewSMActor->UpdateComponents();

			// Remember the AttachTag
			NewSMActor->AttachTag = AttachTag;

			// Set Kismet Object Vars to new SMActor
			for(INT j=0; j<SeqVars.Num(); j++)
			{
				USeqVar_Object* ObjVar = Cast<USeqVar_Object>( SeqVars(j) );
				if(ObjVar)
				{
					ObjVar->ObjValue = NewSMActor;
				}
			}
			

			// Remeber this SkeletalMeshActor so we don't try and destroy it.
			NewSMActors.AddItem(NewSMActor);
		}
	}
}


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
