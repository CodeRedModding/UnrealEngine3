/*=============================================================================
	EditorFrame.h: The window that houses the entire editor

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

class WxEditorFrame : public wxFrame
{
public:
	WxDockingRoot* DockingRoot;						// Sits inside the client area and handles docking windows to the edges of the editor
	class WxButtonBar* ButtonBar;
	WxModeBar* ModeBar;
	wxWindow* ViewportContainer;					// Holds all open level editing viewports.
	WxStatusBar* StatusBars[ SB_Max ];				// The status bars the editor uses
	FFilename MapFilename;							// The name of the map we have loaded
	WxMainMenu* MainMenuBar;
	WxMainToolBar* MainToolBar;
	FFileHelperMap FileHelper;

	// Viewport configuration vars

	TArray<FViewportConfig_Template*> ViewportConfigTemplates;	// All the possible viewport configurations
	FViewportConfig_Data* ViewportConfigData;					// The viewport configuration data currently in use

	// Common menus that are used in more than one place

	WxDragGridMenu* DragGridMenu;
	WxRotationGridMenu* RotationGridMenu;
	TMap<INT,UOptionsProxy*>* OptionProxies;

	// Bitmaps which are used all over the editor

	WxBitmap WhitePlaceholder, WizardB;
	WxMaskedBitmap DownArrowB, MaterialEditor_RGBAB, MaterialEditor_RB, MaterialEditor_GB, MaterialEditor_BB,
		MaterialEditor_AB, MaterialEditor_ControlPanelFillB, MaterialEditor_ControlPanelCapB,
		LeftHandle, RightHandle, RightHandleOn, ArrowUp, ArrowDown, ArrowRight;

	WxEditorFrame( wxWindow* InParent, wxWindowID InID, const FString& InTitle, const wxPoint& InPos, const wxSize& InSize );
	~WxEditorFrame();

	void UpdateUI();
	void SetUp();
	void SetStatusBar( EStatusBar InStatusBar );
	void SetMapFilename( FString InMapFilename );
	void RefreshCaption();
	void SetViewportConfig( EViewportConfig InConfig );
	FGetInfoRet GetInfo( ULevel* Level, INT Item );
	void OptionProxyInit();

	void OnSize( wxSizeEvent& InEvent );
	void OnMove( wxMoveEvent& In );
	void MenuFileNew( wxCommandEvent& In );
	void MenuFileOpen( wxCommandEvent& In );
	void MenuFileSave( wxCommandEvent& In );
	void MenuFileSaveAs( wxCommandEvent& In );
	void MenuFileImportNew( wxCommandEvent& In );
	void MenuFileImportMerge( wxCommandEvent& In );
	void MenuFileExportAll( wxCommandEvent& In );
	void MenuFileExportSelected( wxCommandEvent& In );
	void MenuFileMRU( wxCommandEvent& In );
	void MenuFileExit( wxCommandEvent& In );
	void MenuEditUndo( wxCommandEvent& In );
	void MenuEditRedo( wxCommandEvent& In );
	void MenuEditShowWidget( wxCommandEvent& In );
	void MenuEditTranslate( wxCommandEvent& In );
	void MenuEditRotate( wxCommandEvent& In );
	void MenuEditScale( wxCommandEvent& In );
	void MenuEditScaleNonUniform( wxCommandEvent& In );
	void CoordSystemSelChanged( wxCommandEvent& In );
	void MenuEditSearch( wxCommandEvent& In );
	void MenuEditCut( wxCommandEvent& In );
	void MenuEditCopy( wxCommandEvent& In );
	void MenuEditDuplicate( wxCommandEvent& In );
	void MenuEditDelete( wxCommandEvent& In );
	void MenuEditSelectNone( wxCommandEvent& In );
	void MenuEditSelectAll( wxCommandEvent& In );
	void MenuEditSelectInvert( wxCommandEvent& In );
	void MenuViewLogWindow( wxCommandEvent& In );
	void MenuViewFullScreen( wxCommandEvent& In );
	void MenuViewBrushPolys( wxCommandEvent& In );
	void MenuViewCamSpeedSlow( wxCommandEvent& In );
	void MenuViewCamSpeedNormal( wxCommandEvent& In );
	void MenuViewCamSpeedFast( wxCommandEvent& In );
	void MenuEditPasteOriginalLocation( wxCommandEvent& In );
	void MenuEditPasteWorldOrigin( wxCommandEvent& In );
	void MenuEditPasteHere( wxCommandEvent& In );
	void MenuDragGrid( wxCommandEvent& In );
	void MenuRotationGrid( wxCommandEvent& In );
	void MenuViewportConfig( wxCommandEvent& In );
	void MenuViewShowBrowser( wxCommandEvent& In );
	void MenuActorProperties( wxCommandEvent& In );
	void MenuSyncGenericBrowser( wxCommandEvent& In );
	void MenuSurfaceProperties( wxCommandEvent& In );
	void MenuLevelProperties( wxCommandEvent& In );
	void MenuBrushCSG( wxCommandEvent& In );
	void MenuBrushAddAntiPortal( wxCommandEvent& In );
	void MenuBrushAddSpecial( wxCommandEvent& In );
	void MenuBuildPlay( wxCommandEvent& In );
	void MenuBuild( wxCommandEvent& In );
	void MenuBuildOptions( wxCommandEvent& In );
	void MenuRedrawAllViewports( wxCommandEvent& In );
	void MenuAlignWall( wxCommandEvent& In );
	void MenuToolCheckErrors( wxCommandEvent& In );
	void MenuReviewPaths( wxCommandEvent& In );
	void MenuRotateActors( wxCommandEvent& In );
	void MenuResetParticleEmitters( wxCommandEvent& In );
	void MenuSelectAllSurfs( wxCommandEvent& In );
	void MenuBrushAdd( wxCommandEvent& In );
	void MenuBrushSubtract( wxCommandEvent& In );
	void MenuBrushIntersect( wxCommandEvent& In );
	void MenuBrushDeintersect( wxCommandEvent& In );
	void MenuBrushOpen( wxCommandEvent& In );
	void MenuBrushSaveAs( wxCommandEvent& In );
	void MenuBrushImport( wxCommandEvent& In );
	void MenuBrushExport( wxCommandEvent& In );
	void Menu2DShapeEditor( wxCommandEvent& In );
	void MenuReplaceSkelMeshActors( wxCommandEvent& In );
	void MenuWizardNewTerrain( wxCommandEvent& In );	
	void MenuAboutBox( wxCommandEvent& In );	
	void MenuBackdropPopupAddClassHere( wxCommandEvent& In );
	void MenuBackdropPopupAddLastSelectedClassHere( wxCommandEvent& In );
	void MenuSurfPopupExtrude16( wxCommandEvent& In );
	void MenuSurfPopupExtrude32( wxCommandEvent& In );
	void MenuSurfPopupExtrude64( wxCommandEvent& In );
	void MenuSurfPopupExtrude128( wxCommandEvent& In );
	void MenuSurfPopupExtrude256( wxCommandEvent& In );
	void MenuSurfPopupApplyMaterial( wxCommandEvent& In );
	void MenuSurfPopupAlignPlanarAuto( wxCommandEvent& In );
	void MenuSurfPopupAlignPlanarWall( wxCommandEvent& In );
	void MenuSurfPopupAlignPlanarFloor( wxCommandEvent& In );
	void MenuSurfPopupAlignBox( wxCommandEvent& In );
	void MenuSurfPopupAlignFace( wxCommandEvent& In );
	void MenuSurfPopupUnalign( wxCommandEvent& In );
	void MenuSurfPopupSelectMatchingGroups( wxCommandEvent& In );
	void MenuSurfPopupSelectMatchingItems( wxCommandEvent& In );
	void MenuSurfPopupSelectMatchingBrush( wxCommandEvent& In );
	void MenuSurfPopupSelectMatchingTexture( wxCommandEvent& In );
	void MenuSurfPopupSelectAllAdjacents( wxCommandEvent& In );
	void MenuSurfPopupSelectAdjacentCoplanars( wxCommandEvent& In );
	void MenuSurfPopupSelectAdjacentWalls( wxCommandEvent& In );
	void MenuSurfPopupSelectAdjacentFloors( wxCommandEvent& In );
	void MenuSurfPopupSelectAdjacentSlants( wxCommandEvent& In );
	void MenuSurfPopupSelectReverse( wxCommandEvent& In );
	void MenuSurfPopupSelectMemorize( wxCommandEvent& In );
	void MenuSurfPopupRecall( wxCommandEvent& In );
	void MenuSurfPopupOr( wxCommandEvent& In );
	void MenuSurfPopupAnd( wxCommandEvent& In );
	void MenuSurfPopupXor( wxCommandEvent& In );
	void MenuActorPopup( wxCommandEvent& In );
	void MenuActorPopupCopy( wxCommandEvent& In );
	void MenuActorPopupPasteOriginal( wxCommandEvent& In );
	void MenuActorPopupPasteHere( wxCommandEvent& In );
	void MenuActorPopupPasteOrigin( wxCommandEvent& In );
	void MenuActorPopupSelectAllClass( wxCommandEvent& In );
	void MenuActorPopupSelectMatchingStaticMeshes( wxCommandEvent& In );
	void MenuActorPopupAlignCameras( wxCommandEvent& In );
	void MenuActorPopupLockMovement( wxCommandEvent& In );
	void MenuActorPopupMerge( wxCommandEvent& In );
	void MenuActorPopupSeparate( wxCommandEvent& In );
	void MenuActorPopupToFirst( wxCommandEvent& In );
	void MenuActorPopupToLast( wxCommandEvent& In );
	void MenuActorPopupToBrush( wxCommandEvent& In );
	void MenuActorPopupFromBrush( wxCommandEvent& In );
	void MenuActorPopupMakeAdd( wxCommandEvent& In );
	void MenuActorPopupMakeSubtract( wxCommandEvent& In );
	void MenuSnapToFloor( wxCommandEvent& In );
	void MenuAlignToFloor( wxCommandEvent& In );
	void MenuSaveBrushAsCollision( wxCommandEvent& In );
	void MenuQuantizeVertices( wxCommandEvent& In );
	void MenuConvertToStaticMesh( wxCommandEvent& In );
	void MenuOpenKismet( wxCommandEvent& In );
	void MenuActorFindInKismet( wxCommandEvent& In );
	void MenuActorPivotReset( wxCommandEvent& In );
	void MenuActorPivotMoveHere( wxCommandEvent& In );
	void MenuActorPivotMoveHereSnapped( wxCommandEvent& In );
	void MenuActorMirrorX( wxCommandEvent& In );
	void MenuActorMirrorY( wxCommandEvent& In );
	void MenuActorMirrorZ( wxCommandEvent& In );
	void OnAddVolumeClass( wxCommandEvent& In );
	void MenuActorPopupMakeSolid( wxCommandEvent& In );
	void MenuActorPopupMakeSemiSolid( wxCommandEvent& In );
	void MenuActorPopupMakeNonSolid( wxCommandEvent& In );
	void MenuActorPopupBrushSelectAdd( wxCommandEvent& In );
	void MenuActorPopupBrushSelectSubtract( wxCommandEvent& In );
	void MenuActorPopupBrushSelectSemiSolid( wxCommandEvent& In );
	void MenuActorPopupBrushSelectNonSolid( wxCommandEvent& In );

	void MenuActorPopupPathPosition( wxCommandEvent& In );
	void MenuActorPopupPathRebuildSelected( wxCommandEvent& In );
	void MenuActorPopupPathProscribe( wxCommandEvent& In );
	void MenuActorPopupPathForce( wxCommandEvent& In );
	void MenuActorPopupPathClearProscribed( wxCommandEvent& In );
	void MenuActorPopupPathClearForced( wxCommandEvent& In );

	void MenuPlayFromHere( wxCommandEvent& In );

	void MenuUseActorFactory( wxCommandEvent& In );
	void MenuUseActorFactoryAdv( wxCommandEvent& In );

	void UI_MenuEditUndo( wxUpdateUIEvent& In );
	void UI_MenuEditRedo( wxUpdateUIEvent& In );
	void UI_MenuEditShowWidget( wxUpdateUIEvent& In );
	void UI_MenuEditTranslate( wxUpdateUIEvent& In );
	void UI_MenuEditRotate( wxUpdateUIEvent& In );
	void UI_MenuEditScale( wxUpdateUIEvent& In );
	void UI_MenuEditScaleNonUniform( wxUpdateUIEvent& In );
	void UI_MenuViewportConfig( wxUpdateUIEvent& In );
	void UI_MenuDragGrid( wxUpdateUIEvent& In );
	void UI_MenuRotationGrid( wxUpdateUIEvent& In );
	void UI_MenuViewFullScreen( wxUpdateUIEvent& In );
	void UI_MenuViewBrushPolys( wxUpdateUIEvent& In );
	void UI_MenuViewCamSpeedSlow( wxUpdateUIEvent& In );
	void UI_MenuViewCamSpeedNormal( wxUpdateUIEvent& In );
	void UI_MenuViewCamSpeedFast( wxUpdateUIEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
