/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __EDITORFRAME_H__
#define __EDITORFRAME_H__

class WxViewportsContainer;
class USeqAct_Interp;

#include "DepthDependentHaloRendering.h"
#include "DlgFolderList.h"
#if WITH_SIMPLYGON
#include "SimplygonMeshUtilities.h"
#endif // #if WITH_SIMPLYGON

class WxEditorFrame : public wxFrame, public FCallbackEventDevice
{
	// Used for dynamic creation of the window. This must be declared for any
	// subclasses of WxEditorFrame
	DECLARE_DYNAMIC_CLASS(WxEditorFrame);

	/**
	 * Struct for tracking launched UFE instances and their filenames
	 */
	struct FUFECookInfo
	{
		FUFECookInfo( void* ProcHandleIn, const FFilename& FilenameIn )
		{
			ProcHandle	= ProcHandleIn;
			Filename		= FilenameIn;
		}
		void*			ProcHandle;		// The handle for the UFE instance to track its progress
		FFilename	Filename;			// The xml filename that was parsed to the UFE instance
	};

private:
	class WxMainMenu* MainMenuBar;
	UBOOL bShouldUpdateUI;
	TArray<FUFECookInfo> UFECookInfo;

protected:
	/** A cached version of the level filename to incorporate into the editor caption. */
	FString LevelFilename;

	/**
	 * Returns the localized caption for this frame window. It looks in the
	 * editor's INI file to determine which localization file to use
	 */
	virtual FString GetLocalizedCaption(FString levelName = TEXT(""));

public:
	class WxButtonBar* ButtonBar;

	/** Holds all open level editing viewports. */
	WxViewportsContainer* ViewportContainer;

	/** Ptr to the folder list helper. */
	WxDlgFolderList* DlgFolderList;

	/** The status bars the editor uses. */
	WxStatusBar* StatusBars[ SB_Max ];

	/** Window position, size and maximized state, persisted in the .ini file */
	wxPoint	FramePos;
	wxSize	FrameSize;
	UBOOL	bFrameMaximized;

private:
	class WxMainToolBar* MainToolBar;

	///////////////////////////////////////////////////////////
	// Viewport configuration vars

public:
	/** All the possible viewport configurations. */
	TArray<FViewportConfig_Template*> ViewportConfigTemplates;

	/** The viewport configuration data currently in use. */
	FViewportConfig_Data* ViewportConfigData;

	/** List of projects that the user can switch to */
	TArray<FString> ProjectNames;

	/** Returns whether or not we should link the vertical splitters for the top and bottom viewports */
	UBOOL GetViewportsResizeTogether() const
	{
		return bViewportResizeTogether;
	}

private:
	/** Locks the vertical splitter for the top and bottom viewports together */
	UBOOL bViewportResizeTogether;

	/**Settings for depth dependent halos in wireframe*/
	FDepthDependentHaloSettings WireframeHaloSettings;


	// Common menus that are used in more than one place
	class WxDragGridMenu* DragGridMenu;
	class WxRotationGridMenu* RotationGridMenu;
	class WxAutoSaveOptionsMenu* AutoSaveOptionsMenu;
	class WxScaleGridMenu* ScaleGridMenu;
	class WxPreferencesMenu* PreferencesMenu;

	
	/** Matinee list drop-down menu */
	wxMenu* MatineeListMenu;

	/** List of actual Matinees that are available in our drop-down list.  The element index maps to the
	    item ID in the list menu */
	TArray< USeqAct_Interp* > MatineeListMenuMap;

	/** The game exe of a pending project.  When the editor shuts down it will spawn this exe if its not empty */ 
	FString PendingProjectExe;
	FString PendingProjectCmdLine;
public:
	// Bitmaps which are used all over the editor
	/**
	 * Default constructor. Construction is a 2 phase process: class creation
	 * followed by a call to Create(). This is required for dynamically determining
	 * which editor frame class to create for the editor's main frame.
	 */
	WxEditorFrame();

	/**
	 * Part 2 of the 2 phase creation process. First it loads the localized caption.
	 * Then it creates the window with that caption. And finally finishes the
	 * window initialization
	 */
	virtual void Create();

	~WxEditorFrame();

	void UpdateUI();
	void UpdateDirtiedUI();
	virtual void SetUp();
	void SetStatusBarType( EStatusBar InStatusBar );
	void SetViewportConfig( EViewportConfig InConfig );
	FGetInfoRet GetInfo( INT Item );

	/**
	* Puts all of the AVolume classes into the passed in array and sorts them by class name.
	*
	* @param	VolumeClasses		Array to populate with AVolume classes.
	*/
	void GetSortedVolumeClasses( TArray< UClass* >* VolumeClasses );

	/**
	 * @return	A pointer to the rotation grid menu.
	 */
	class WxRotationGridMenu* GetRotationGridMenu();

	/**
	* @return	A pointer to the autosave options menu
	*/
	class WxAutoSaveOptionsMenu* GetAutoSaveOptionsMenu();

	/**
	 * @return	A pointer to the drag grid menu.
	 */
	class WxDragGridMenu* GetDragGridMenu();

	/**
	 * @return	A pointer to the preferences sub-menu.
	 */
	class WxPreferencesMenu* GetPreferencesMenu();

	/**
	* @return	A pointer to the scale grid menu.
	*/
	class WxScaleGridMenu* GetScaleGridMenu();


	/**
	 * If there's only one Matinee available, opens it for editing, otherwise returns a menu to display
	 *
	 * @return NULL if Matinee was opened by this call, otherwise a menu to display to the user
	 */
	wxMenu* OpenMatineeOrBuildMenu();

	/**
	 * Accessor for the MRU menu
	 *
	 * @return	Menu for MRU items
	 */
	wxMenu* GetMRUMenu();

	/**
	 * Accessor for Favorites list menu
	 *
	 * @return	Menu for the favorites list
	 */
	wxMenu* GetFavoritesMenu();

	/**
	 * Accessor for the combined menu of MRU and Favorites items
	 *
	 * @return	Combined menu with MRU and favorites items
	 */
	wxMenu* GetCombinedMRUFavoritesMenu();

	/**
	 * Accessor for the Lock read-only item
	 *
	 * @return	Lock item
	 */
	wxMenuItem* GetLockReadOnlyLevelsItem();

	/**
	 * Accessor for the MRU/Favorites list
	 *
	 * @return	MRU/Favorites list
	 */
	class FMainMRUFavoritesList* GetMRUFavoritesList();

	/**
	 * Process events the editor frame is registered to.
	 *
	 * @param	InEvent		The event that was fired.
	 */
	virtual void Send(ECallbackEventType InEvent);

	/**
	 * Process events the editor frame is registered to (file change notifications)
	 */
	void Send( ECallbackEventType InType, const FString& InString, UObject* InObject);

	/**
	 * Helper function to enable and disable file listening that puts the file extensions local to one function
	 * @param bListenOnOff - TRUE if we want to listen to file system notifications
	 */
	void SetFileSystemNotifications(const UBOOL bTextureListenOnOff, const UBOOL bApexListenOnOff);

	/**
	 * Updates the application's title bar caption.
	 *
	 * @param	LevelFilename		[opt] The level filename to use as editor caption.  Ignored if NULL.
	 */
	void RefreshCaption(const FFilename* InLevelFilename);

	/**
	 * Synchronizes an actor with the content browser.
	 */
	void SyncToContentBrowser();

	/**
	 * Synchronizes a material with the content browser.
	 */
	void SyncMaterialToGenericBrowser( INT ComponentIdx, INT MaterialIdx, UBOOL bBase );

	/**
	 * Synchronizes a texture with the content browser.
	 */
	void SyncTextureToGenericBrowser( INT ComponentIdx, INT MaterialIdx, INT TextureIdx );

	/**
	 * Synchronizes the content browser's selected objects to the collection specified.
	 *
	 * @param	ObjectSet	the list of objects to sync to
	 */
	void SyncBrowserToObjects(TArray<UObject*>& ObjectSet);

	/** Called when Sentinel window is closed, to release pointer. */
	void SentinelClosed();

	/** Called when we want to lock or unlock read only levels. */
	void LockReadOnlyLevels( const UBOOL bLock );

	//////////////////////////////////////////////////////////////
	// WxEvents

	void OnSize( wxSizeEvent& InEvent );
	void OnMove( wxMoveEvent& InEvent );
	void OnSplitterChanging( wxSplitterEvent& InEvent );
	void OnSplitterDblClk( wxSplitterEvent& InEvent );

	/** Called when the LightingQuality button is right-clicked and an entry is selected */
	void OnLightingQualityButton( wxCommandEvent& In );

	/**Called to send wire frame constants to the render thread*/
	void InitWireframeConstants(void);
	/**sends depth dependent halo constants down to render thread*/
	void SendWireframeConstantsToRenderThread(void);

	/**
	 * Accessor for wizard bitmap that is loaded on demand
	 */
	WxBitmap& GetWizardB(void);
	/**
	 * Accessor for (little) down arrow bitmap that is loaded on demand
	 */
	WxBitmap& GetDownArrowB(void);
	/**
	 * Accessor for (big) arrow down bitmap that is loaded on demand
	 */
	WxBitmap& GetArrowDown(void);
	/**
	 * Accessor for arrow up bitmap that is loaded on demand
	 */
	WxBitmap& GetArrowUp(void);

private:
	/**
	 * Called when the app is trying to quit.
	 */
	void OnClose( wxCloseEvent& InEvent );

	/**
	 * Called when the application is minimized.
	 */
	void OnIconize( wxIconizeEvent& InEvent );

	
	/**
	 * Minimizes all children
	 */
	void MinimizeAllChildren();

	/**
	* Called when the application is maximized.
	*/
	void OnMaximize( wxMaximizeEvent& InEvent );

	/**
	 * Restore's all minimized children.
	 */
	void RestoreAllChildren();

	/**
	 * Launches game (or PIE) for the specific console index
	 *
	 * @param	WhichConsole	Console index to use or INDEX_NONE to Play in Editor
	 * @param	bPlayInViewport	True to launch PIE in the currently active viewport window
	 * @param	bUseMobilePreview		True to enable mobile preview mode (PC platform only)
	 */
	void PlayFromHere(INT WhichConsole = INDEX_NONE, const UBOOL bPlayInViewport = FALSE, const UBOOL bUseMobilePreview = FALSE );


	/**
	 * Override for handling custom windows messages from a wxWidget window.  We need to handle custom balloon notification messages.
	 */
	virtual WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);

	/**
	 * Adds Matinees in the specified level to the Matinee list menu
	 *
	 * @param	Level				The level to add
	 * @param	InOutMatinees		(In/Out) List of Matinee sequences, built up as we go along
	 * @param	bInOutNeedSeparator	(In/Out) True if we need to add a separator bar before the next Matinee
	 * @param	CurPrefix			Prefix string for the menu items
	 */
	void AddMatineesInLevelToList( ULevel* Level, TArray< USeqAct_Interp* >& InOutMatinees, UBOOL& bInOutNeedSeparator, FString CurPrefix );


	/**
	 * Recursively adds Matinees in the specified sequence to the Matinee list menu
	 *
	 * @param	RootSeq				Parent sequence that contains the Matinee sequences we'll be adding
	 * @param	InOutMatinees		(In/Out) List of Matinee sequences, built up as we go along
	 * @param	bInOutNeedSeparator	(In/Out) True if we need to add a separator bar before the next Matinee
	 * @param	CurPrefix			Prefix string for the menu items
	 */
	void RecursivelyAddMatineesInSequenceToList( USequence* RootSeq, TArray< USeqAct_Interp* >& InOutMatinees, UBOOL& bInOutNeedSeparator, FString CurPrefix );

	/**
	 * Stores of the camera data and viewport type from the previous viewport layout
	 *
	 * @param	PreviousViewportConfig	The previous viewport layout
	 */
	void SetPreviousViewportData( FViewportConfig_Data* PreviousViewportConfig );

	void MenuFileNewMap( wxCommandEvent& In );
	void MenuFileNewProject( wxCommandEvent& In );
	void MenuFileOpen( wxCommandEvent& In );
	void MenuFileSave( wxCommandEvent& In );
	void MenuFileSaveAllLevels( wxCommandEvent& In );
	void MenuFileSaveAs( wxCommandEvent& In );
	void MenuFileSaveDlg( wxCommandEvent& In );
	void MenuFileSaveAll( wxCommandEvent& In );
	void MenuFileForceSaveAll( wxCommandEvent& In );
	void MenuSelectEditorMode( wxCommandEvent& In );
	void MenuFileImportNew( wxCommandEvent& In );
	void MenuFileImportMerge( wxCommandEvent& In );
	void MenuFileExportAll( wxCommandEvent& In );
	void MenuFileExportSelected( wxCommandEvent& In );
	void MenuFileCreateArchetype( wxCommandEvent& In );
	void MenuFileMRU( wxCommandEvent& In );

	/** Called when an item in the Matinee list drop-down menu is clicked */
	void OnMatineeListMenuItem( wxCommandEvent& In );

	void MenuFileExit( wxCommandEvent& In );
	void MenuEditUndo( wxCommandEvent& In );
	void MenuEditRedo( wxCommandEvent& In );
	void MenuFarPlaneScrollChanged( wxCommandEvent& In );
	void MenuFarPlaneScrollChangeEnd( wxScrollEvent& In );
	void MenuEditMouseLock( wxCommandEvent& In );
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
	void MenuEditSelectBuilderBrush( wxCommandEvent& In );
	void MenuEditSelectAll( wxCommandEvent& In );
	void MenuEditSelectByProperty( wxCommandEvent& In );
	void MenuEditSelectInvert( wxCommandEvent& In );
	void MenuEditSelectPostProcessVolume( wxCommandEvent& In );
	void MenuViewFullScreen( wxCommandEvent& In );
	void MenuViewBrushPolys( wxCommandEvent& In );
	void MenuViewDistributionToggle( wxCommandEvent& In );
	void MenuMaterialQualityToggle( wxCommandEvent& In );
	void MenuViewToggleLODLocking( wxCommandEvent& In );
	void MenuToggleSocketSnapping( wxCommandEvent& In );
	void MenuToggleSocketNames( wxCommandEvent& In );
	void MenuViewPSysRealtimeLODToggle( wxCommandEvent& In );
	void MenuViewPSysHelperToggle( wxCommandEvent& In );
	void MenuAllowFlightCameraToRemapKeys( wxCommandEvent& In );

	/** Called when 'Auto Restart Reimported Flash Movies' is toggled in the preferences menu */
	void MenuAutoRestartReimportedFlashMovies( wxCommandEvent& In );
	void MenuAutoReimportTextures( wxCommandEvent& In );

	void MenuLoadSimpleLevelAtStartup( wxCommandEvent& In );
	
	void MenuEditPasteOriginalLocation( wxCommandEvent& In );
	void MenuEditPasteWorldOrigin( wxCommandEvent& In );
	void MenuEditPasteHere( wxCommandEvent& In );
	void MenuDragGrid( wxCommandEvent& In );
	void MenuRotationGrid( wxCommandEvent& In );
	void MenuAngleSnapType( wxCommandEvent& In );
	void MenuAutoSaveOptions( wxCommandEvent& In );
	void MenuScaleGrid( wxCommandEvent& In );
	void MenuViewportConfig( wxCommandEvent& In );
	void MenuOpenNewFloatingViewport( wxCommandEvent& In );
	void MenuViewportResizeTogether( wxCommandEvent& In );
	void MenuCenterZoomAroundCursor( wxCommandEvent &In );
	void MenuPanMovesCanvas( wxCommandEvent &In );
	void MenuReplaceRespectsScale( wxCommandEvent &In );
	void MenuDefaultToRealtimeMode( wxCommandEvent &In );
	void MenuToggleAbsoluteTranslation( wxCommandEvent &In );
	void MenuToggleTranslateRotateZWidget( wxCommandEvent &In );
	void MenuViewportHoverFeedback( wxCommandEvent &In );
	void MenuHighlightWithBrackets( wxCommandEvent &In );
	void MenuWireframeHalos( wxCommandEvent &In );
	void MenuUseStrictBoxSelection( wxCommandEvent& In );
	void MenuPromptSCCOnPackageModification( wxCommandEvent& In );
	void MenuLanguageSelection( wxCommandEvent& In );
	void MenuAspectRatioSelection( wxCommandEvent& In );
	void MenuRunUnitTests( wxCommandEvent& In );
	void MenuViewDetailModeLow( wxCommandEvent& In );
	void MenuViewDetailModeMedium( wxCommandEvent& In );
	void MenuViewDetailModeHigh( wxCommandEvent& In );
	void MenuViewShowBrowser( wxCommandEvent& In );
	void MenuActorProperties( wxCommandEvent& In );
	void MenuSyncContentBrowser( wxCommandEvent& In );
	void MenuSyncMaterialInterface( wxCommandEvent &In );
	void MenuSyncTexture( wxCommandEvent& In );
	void MenuMakeSelectedActorsLevelCurrent( wxCommandEvent& In );
	void MenuMakeSelectedActorsLevelGridVolumeCurrent( wxCommandEvent& In );
	void MenuMoveSelectedActorsToCurrentLevel( wxCommandEvent& In );
	void MenuSelectLevelInLevelBrowser( wxCommandEvent& In );
	void MenuSelectLevelOnlyInLevelBrowser( wxCommandEvent& In );
	void MenuDeselectLevelInLevelBrowser( wxCommandEvent& In );
	void MenuToggleLinkedOrthographicViewports( wxCommandEvent& In );
	void MenuToggleViewportCameraToUpdateFromPIV( wxCommandEvent& In );
	void MenuResetSuppressibleDialogs( wxCommandEvent& In );
	void MenuClickBSPSelectsBrush( wxCommandEvent &In );
	void MenuBSPAutoUpdate( wxCommandEvent &In );

	/**
	 * Opens the Level Browser and selects all levels associated with the currently-selected level streaming/grid volumes
	 */
	void MenuFindStreamingVolumeLevelsInLevelBrowser( wxCommandEvent& In );

	/**
	 * Makes the selected level grid volume the "current" level grid volume
	 */
	void MenuMakeLevelGridVolumeCurrent( wxCommandEvent& In );

	/**
	 * Clears the "current" level grid volume
	 */
	void MenuClearCurrentLevelGridVolume( wxCommandEvent& In );

	void MenuSurfaceProperties( wxCommandEvent& In );
	void MenuWorldProperties( wxCommandEvent& In );
	void MenuLightingResults( wxCommandEvent& In );
	void MenuLightingBuildInfo( wxCommandEvent& In );
	void MenuLightingStaticMeshInfo( wxCommandEvent& In );
	void MenuLightMapDensityRenderingOptions( wxCommandEvent& In );
	void MenuLightMapResolutionRatioAdjust( wxCommandEvent& In );
	void MenuLightingTools( wxCommandEvent& In );
	void MenuBrushCSG( wxCommandEvent& In );
	void MenuBrushAddSpecial( wxCommandEvent& In );
	void MenuBuildPlayInEditor( wxCommandEvent& In );
	void MenuBuildPlayInActiveViewport( wxCommandEvent& In );
	void MenuBuildPlayOnConsole( wxCommandEvent& In );
	void MenuBuildPlayUsingMobilePreview( wxCommandEvent& In );
	void MenuBuildCookForConsole( wxCommandEvent& In );
	UBOOL MenuCreateXMLForUFE( wxCommandEvent& In, const FString& MapNameIn, const FString& XMLNameIn, FFilename& XMLFileNameOut ) const;
	void MenuUpdateUFEProcs( void );
	void MenuConsoleSpecific( wxCommandEvent& In );
	void UpdateUIConsoleSpecific( wxUpdateUIEvent& In );
	void OnMenuOpen(wxMenuEvent& In);
	void MenuBuild( wxCommandEvent& In );
	void MenuBuildAndSubmit( wxCommandEvent& In );
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
	void MenuReplaceSkelMeshActors( wxCommandEvent& In );
	void MenuRegenAllProcBuildings( wxCommandEvent& In );
	void MenuRegenSelProcBuildings( wxCommandEvent& In );
	void MenuGenAllProcBuildingLODTex( wxCommandEvent& In );
	void MenuGenSelProcBuildingLODTex( wxCommandEvent& In );
	void MenuLockReadOnlyLevels( wxCommandEvent& In );
	void MenuSetFileListeners( wxCommandEvent& In );
	void MenuJournalUpdate( wxCommandEvent& In );
	void MenuCleanBSPMaterials( wxCommandEvent& In );
	void MenuWizardNewTerrain( wxCommandEvent& In );	

	// Help Menu
	void Menu_Help_About( wxCommandEvent& In );	
	void Menu_Help_OnlineHelp( wxCommandEvent& In );
	void Menu_Help_UDKForums( wxCommandEvent& In );
	void Menu_Help_SearchUDN( wxCommandEvent& In );
	void Menu_Help_StartupTip( wxCommandEvent& In );
	void Menu_Help_WelcomeScreen( wxCommandEvent& In );

	void MenuBackdropPopupAddClassHere( wxCommandEvent& In );
	void MenuBackdropPopupReplaceWithClass(wxCommandEvent& In);
	void MenuBackdropPopupAddLastSelectedClassHere( wxCommandEvent& In );
	void MenuSurfPopupApplyMaterial( wxCommandEvent& In );
	void MenuSurfPopupAlignPlanarAuto( wxCommandEvent& In );
	void MenuSurfPopupAlignPlanarWall( wxCommandEvent& In );
	void MenuSurfPopupAlignPlanarFloor( wxCommandEvent& In );
	void MenuSurfPopupAlignBox( wxCommandEvent& In );
	void MenuSurfPopupAlignFit( wxCommandEvent& In );
	void MenuSurfPopupUnalign( wxCommandEvent& In );
	void MenuSurfPopupSelectMatchingGroups( wxCommandEvent& In );
	void MenuSurfPopupSelectMatchingItems( wxCommandEvent& In );
	void MenuSurfPopupSelectMatchingBrush( wxCommandEvent& In );
	void MenuSurfPopupSelectMatchingTexture( wxCommandEvent& In );
	void MenuSurfPopupSelectMatchingResolution( wxCommandEvent& In );
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
	void MenuBlockingVolumeBBox( wxCommandEvent& In );
	void MenuBlockingVolumeConvexVolumeHeavy( wxCommandEvent& In );
	void MenuBlockingVolumeConvexVolumeNormal( wxCommandEvent& In );
	void MenuBlockingVolumeConvexVolumeLight( wxCommandEvent& In );
	void MenuBlockingVolumeConvexVolumeRough( wxCommandEvent& In );
	void MenuBlockingVolumeColumnX( wxCommandEvent& In );
	void MenuBlockingVolumeColumnY( wxCommandEvent& In );
	void MenuBlockingVolumeColumnZ( wxCommandEvent& In );
	void MenuBlockingVolumeAutoConvex( wxCommandEvent& In );
	void MenuActorPopupSelectAllClass( wxCommandEvent& In );
	void MenuActorPopupSelectAllClassWithArchetype( wxCommandEvent& In );
	void MenuActorPopupSelectAllBased( wxCommandEvent& In );
	void MenuActorPopupSelectMatchingProcBuildingsByRuleset( wxCommandEvent& In );
	void MenuActorPopupSelectMatchingStaticMeshesThisClass( wxCommandEvent& In );
	void MenuActorPopupSelectMatchingStaticMeshesAllClasses( wxCommandEvent& In );	
	void MenuActorPopupSelectMatchingSkeletalMeshesThisClass( wxCommandEvent& In );
	void MenuActorPopupSelectMatchingSkeletalMeshesAllClasses( wxCommandEvent& In );
	void MenuActorPopupSelectMatchingEmitter( wxCommandEvent& In );
	void MenuActorPopupSelectMatchingSpeedTrees( wxCommandEvent& In );
	void MenuActorPopupToggleDynamicChannel( wxCommandEvent& In );
	void MenuActorPopupSelectAllLights( wxCommandEvent& In );
	void MenuActorPopupSelectAllRendered( wxCommandEvent& In );
	void MenuActorPopupSelectAllLightsWithSameClassification( wxCommandEvent& In );
	void MenuActorPopupSelectKismetUnreferencedAll( wxCommandEvent& In );
	void MenuActorPopupSelectKismetReferencedAll( wxCommandEvent& In );
	void MenuActorPopupSelectKismetUnreferenced( wxCommandEvent& In );
	void MenuActorPopupSelectKismetReferenced( wxCommandEvent& In );
	void MenuActorPopupAlignCameras( wxCommandEvent& In );
	void MenuActorPopupLockMovement( wxCommandEvent& In );
	void MenuActorPopupSnapViewToActor( wxCommandEvent& In );
	void MenuActorPopupMerge( wxCommandEvent& In );
	void MenuActorPopupSeparate( wxCommandEvent& In );
	void MenuActorPopupToFirst( wxCommandEvent& In );
	void MenuActorPopupToLast( wxCommandEvent& In );
	void MenuActorPopupToBrush( wxCommandEvent& In );
	void MenuActorPopupFromBrush( wxCommandEvent& In );
	void MenuActorPopupMakeAdd( wxCommandEvent& In );
	void MenuActorPopupMakeSubtract( wxCommandEvent& In );
	void MenuActorPopupSelectAllWithMatchingMaterial( wxCommandEvent& In );
	void MenuActorSelectShow( wxCommandEvent& In );
	void MenuActorSelectHide( wxCommandEvent& In );
	void MenuActorSelectInvert( wxCommandEvent& In );
	void MenuActorSelectRelevantLights( wxCommandEvent& In );
	void MenuActorSelectRelevantDominantLights( wxCommandEvent& In );
	void MenuTogglePrefabsLocked( wxCommandEvent& In );
	void MenuActorShowAll( wxCommandEvent& In );
	void MenuToggleGroupsActive( wxCommandEvent& In );

	/**
	 * Called in response to the user selecting to show all at startup in the context menu
	 * (Changes bHiddenEd to FALSE for all actors/BSP)
	 *
	 * @param	In	Event automatically generated by wxWidgets upon menu item selection
	 */
	void MenuActorShowAllAtStartup( wxCommandEvent& In );

	/**
	 * Called in response to the user selecting to show selected at startup in the context menu
	 * (Changes bHiddenEd to FALSE for all selected actors/BSP)
	 *
	 * @param	In	Event automatically generated by wxWidgets upon menu item selection
	 */
	void MenuActorSelectShowAtStartup( wxCommandEvent& In );

	/**
	 * Called in response to the user selecting to hide selected at startup in the context menu
	 * (Changes bHiddenEd to TRUE for all selected actors/BSP)
	 *
	 * @param	In	Event automatically generated by wxWidgets upon menu item selection
	 */
	void MenuActorSelectHideAtStartup( wxCommandEvent& In );

	void MenuMoveToGrid( wxCommandEvent& In );
	void MenuSnapToFloor( wxCommandEvent& In );
	void MenuAlignToFloor( wxCommandEvent& In );
	void MenuSnapPivotToFloor( wxCommandEvent& In );
	void MenuAlignPivotToFloor( wxCommandEvent& In );
	void MenuSaveBrushAsCollision( wxCommandEvent& In );
	void MenuConvertActors( wxCommandEvent& In );
	void MenuConvertToBlockingVolume( wxCommandEvent& In );
	void MenuSetCollisionBlockAll( wxCommandEvent& In );
	void MenuSetCollisionBlockWeapons( wxCommandEvent& In );
	void MenuSetCollisionBlockNone( wxCommandEvent& In );

	/** Called when "Allow Translucent Selection" is clicked */
	void Clicked_SelectTranslucent( wxCommandEvent& In );

	/** Called to update the UI state of the "Allow Translucent Selection" button */
	void UpdateUI_SelectTranslucent( wxUpdateUIEvent& In );

	/** Called when "Only Load Visible Levels in PIE" is clicked */
	void Clicked_PIEVisibleOnly( wxCommandEvent& In );

	/** Called to update the UI state of the "Only Load Visible Levels in PIE" button */
	void UpdateUI_PIEVisibleOnly( wxUpdateUIEvent& In );

	/** Called when "Always Optimize Content for Mobile" is clicked */
	void Clicked_AlwaysOptimizeContentForMobile( wxCommandEvent& In );

	/** Called to update the UI state of the "Always Optimize Content for Mobile" button */
	void UpdateUI_AlwaysOptimizeContentForMobile( wxUpdateUIEvent& In );

	/** Called when "Emulate Mobile Features" is clicked */
	void Clicked_EmulateMobileFeatures( wxCommandEvent& In );

	/** Called to update the UI state of the "Emulate Mobile Features" button */
	void UpdateUI_EmulateMobileFeatures( wxUpdateUIEvent& In );

	/** Called when a convert light menu option is selected */
	void MenuConvertLights( wxCommandEvent& In);

	/** Called when a convert brush to volume menu option is selected */
	void MenuConvertVolumes( wxCommandEvent& In );

	/** Called when the user selects a menu option from the convert skeletal mesh menu */
	void MenuConvertSkeletalMeshes( wxCommandEvent& In );

#if WITH_SIMPLYGON
	/**
	 * Initiates mesh simplification from an actor pop up menu
	 */
	void MenuActorSimplifyMesh( wxCommandEvent& In );

	/** Simplifies all static meshes selected by the user. */
	void MenuActorSimplifySelectedMeshes( wxCommandEvent& In );
#endif // #if WITH_SIMPLYGON

#if ENABLE_SIMPLYGON_MESH_PROXIES
	/** Creates a proxy for the selected static meshes. */
	void MenuActorCreateMeshProxy( wxCommandEvent& In );
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES

	void MenuActorConvertStaticMeshToNavMesh( wxCommandEvent& In );

	/**
	 * Sets current LOD parent actor
	 */
	void MenuActorSetLODParent( wxCommandEvent& In );

	/**
	 * Sets the LOD parent actor as the replacement for the selected actors
	 */
	void MenuActorAddToLODParent( wxCommandEvent& In );

	/**
	 * Clears the replacement for the selected actors
	 */
	void MenuActorRemoveFromLODParent( wxCommandEvent& In );

	void MenuSetLightDataBasedOnClassification( wxCommandEvent& In );
	void MenuSnapOriginToGrid( wxCommandEvent& In );
	void MenuQuantizeVertices( wxCommandEvent& In );
	void MenuConvertToStaticMesh( wxCommandEvent& In );
	void MenuConvertToProcBuilding( wxCommandEvent& In );
	void MenuOpenKismet( wxCommandEvent& In );

	/** Called when 'UnrealMatinee' is clicked in the main menu */
	void MenuOpenMatinee( wxCommandEvent& In );

	void MenuOpenSentinel( wxCommandEvent& In );

	void MenuActorFindInKismet( wxCommandEvent& In );
	void MenuActorBakePrePivot( wxCommandEvent& In );
	void MenuActorUnBakePrePivot( wxCommandEvent& In );
	void MenuActorPivotReset( wxCommandEvent& In );
	void MenuActorPivotMoveHere( wxCommandEvent& In );
	void MenuActorPivotMoveHereSnapped( wxCommandEvent& In );
	void MenuActorPivotMoveCenterOfSelection( wxCommandEvent& In );
	void MenuActorMirrorX( wxCommandEvent& In );
	void MenuActorMirrorY( wxCommandEvent& In );
	void MenuActorMirrorZ( wxCommandEvent& In );
	void MenuActorSetDetailModeLow( wxCommandEvent& In );
	void MenuActorSetDetailModeMedium( wxCommandEvent& In );
	void MenuActorSetDetailModeHigh( wxCommandEvent& In );
	void OnAddVolumeClass( wxCommandEvent& In );
	void MenuCopyMaterialName( wxCommandEvent& In);
	void MenuEditMaterialInterface( wxCommandEvent &In );
	void MenuCreateMaterialInstance( wxCommandEvent &In );
	void MenuAssignMaterial( wxCommandEvent &In );
	void MenuAssignMaterialToMultipleActors( wxCommandEvent &In );
	void MenuActorPopupMakeSolid( wxCommandEvent& In );
	void MenuActorPopupMakeSemiSolid( wxCommandEvent& In );
	void MenuActorPopupMakeNonSolid( wxCommandEvent& In );
	void MenuActorPopupBrushSelectAdd( wxCommandEvent& In );
	void MenuActorPopupBrushSelectSubtract( wxCommandEvent& In );
	void MenuActorPopupBrushSelectSemiSolid( wxCommandEvent& In );
	void MenuActorPopupBrushSelectNonSolid( wxCommandEvent& In );

	void MenuActorPopupPathPosition( wxCommandEvent& In );
	void MenuActorPopupPathProscribe( wxCommandEvent& In );
	void MenuActorPopupPathForce( wxCommandEvent& In );
	void MenuActorPopupPathAssignWayPointsToRoute( wxCommandEvent& In );
	void MenuActorPopupPathSelectWayPointsInRoute( wxCommandEvent& In );
	void MenuActorPopupPathAssignLinksToCoverGroup( wxCommandEvent& In );
	void MenuActorPopupPathClearProscribed( wxCommandEvent& In );
	void MenuActorPopupPathClearForced( wxCommandEvent& In );
	void MenuActorPopupPathStitchCover( wxCommandEvent& In );

	void MenuActorPopupLinkCrowdDestinations( wxCommandEvent& In );
	void MenuActorPopupUnlinkCrowdDestinations( wxCommandEvent& In );

	void MenuSplineBreakAll( wxCommandEvent& In );
	void MenuSplineConnect( wxCommandEvent& In );
	void MenuSplineBreak( wxCommandEvent& In );
	void MenuSplineReverseAllDirections( wxCommandEvent& In );
	void MenuSplineStraightTangents( wxCommandEvent& In );
	void MenuSplineTestRoute( wxCommandEvent& In );
	void MenuSplineSelectAllNodes( wxCommandEvent& In );

	void ApplyParamSwatchToBuilding( wxCommandEvent& In );
	void ApplyRulesetVariationToFace( wxCommandEvent& In );
	void ApplySelectedMaterialToPBFace( wxCommandEvent& In );
	void ClearFaceRulesetVariations( wxCommandEvent& In );
	void ClearPBFaceMaterials( wxCommandEvent& In );
	void ProcBuildingResourceInfo( wxCommandEvent& In );
	void SelectBaseBuilding( wxCommandEvent& In );
	void GroupSelectedBuildings( wxCommandEvent& In );

	void UpdateImageReflectionSceneCapture( wxCommandEvent& In );

	void MenuEmitterAutoPopulate(wxCommandEvent& In);
	void MenuEmitterReset(wxCommandEvent& In);

	void CreateArchetype(wxCommandEvent& In);
	void UpdateArchetype(wxCommandEvent& In);
	void CreatePrefab(wxCommandEvent& In);
	void AddPrefab(wxCommandEvent& In);
	void SelectPrefabActors(wxCommandEvent& In);
	void UpdatePrefabFromInstance(wxCommandEvent& In);
	void ResetInstanceFromPrefab(wxCommandEvent& In);
	void PrefabInstanceToNormalActors(wxCommandEvent& In);
	void PrefabInstanceOpenSequence(wxCommandEvent& In);

	void Group(wxCommandEvent& In);
	void Regroup(wxCommandEvent& In);
	void Ungroup(wxCommandEvent& In);
	void LockGroup(wxCommandEvent& In);
	void UnlockGroup(wxCommandEvent& In);
	void AddToGroup(wxCommandEvent& In);
	void RemoveFromGroup(wxCommandEvent& In);
	void ReportStatsForGroups(wxCommandEvent& In);
	void ReportStatsForSelection(wxCommandEvent& In);

	void MenuPlayFromHereInEditor( wxCommandEvent& In );
	void MenuPlayFromHereInEditorViewport( wxCommandEvent& In );
	void MenuPlayFromHereOnConsole( wxCommandEvent& In );
	void MenuPlayFromHereUsingMobilePreview( wxCommandEvent& In );

	void MenuUseActorFactory( wxCommandEvent& In );
	void MenuUseActorFactoryAdv( wxCommandEvent& In );
	void MenuReplaceWithActorFactory(wxCommandEvent& In);
	void MenuReplaceWithActorFactoryAdv(wxCommandEvent& In);

	void LoadSelectedAssetForActorFactory( wxCommandEvent& Event );

	void OnDockingChange( class WxDockEvent& In );

	void UI_MenuSelectEditorMode( wxUpdateUIEvent& In );
	void UI_MenuEditUndo( wxUpdateUIEvent& In );
	void UI_MenuEditRedo( wxUpdateUIEvent& In );
	void UI_MenuEditMouseLock( wxUpdateUIEvent& In );
	void UI_MenuEditShowWidget( wxUpdateUIEvent& In );
	void UI_MenuEditTranslate( wxUpdateUIEvent& In );
	void UI_MenuEditRotate( wxUpdateUIEvent& In );
	void UI_MenuEditScale( wxUpdateUIEvent& In );
	void UI_MenuEditScaleNonUniform( wxUpdateUIEvent& In );
	void UI_MenuViewDetailModeLow( wxUpdateUIEvent& In );
	void UI_MenuViewDetailModeMedium( wxUpdateUIEvent& In );
	void UI_MenuViewDetailModeHigh( wxUpdateUIEvent& In );
	void UI_MenuViewportConfig( wxUpdateUIEvent& In );
	void UI_MenuDragGrid( wxUpdateUIEvent& In );
	void UI_MenuRotationGrid( wxUpdateUIEvent& In );
	void UI_MenuAngleSnapType( wxUpdateUIEvent& In );
	void UI_MenuScaleGrid( wxUpdateUIEvent& In );
	void UI_MenuViewFullScreen( wxUpdateUIEvent& In );
	void UI_MenuViewBrushPolys( wxUpdateUIEvent& In );
	void UI_MenuFarPlaneSlider( wxUpdateUIEvent& In );
	void UI_MenuTogglePrefabLock( wxUpdateUIEvent& In );
	void UI_MenuToggleGroupsActive( wxUpdateUIEvent& In );
	void UI_MenuViewDistributionToggle( wxUpdateUIEvent& In );
	void UI_MenuMaterialQualityToggle( wxUpdateUIEvent& In );
	void UI_MenuViewToggleLODLocking( wxUpdateUIEvent& In );
	void UI_MenuToggleSocketSnapping( wxUpdateUIEvent& In );
	void UI_MenuToggleSocketNames( wxUpdateUIEvent& In );
	void UI_MenuViewPSysLODRealtimeToggle( wxUpdateUIEvent& In );
	void UI_MenuViewPSysHelperToggle( wxUpdateUIEvent& In );
	void UI_MenuBuildAllSubmit( wxUpdateUIEvent& In );
	void UI_ContextMenuMakeCurrentLevel( wxUpdateUIEvent& In );
	void UI_ContextMenuMakeCurrentLevelGridVolume( wxUpdateUIEvent& In );

	void UI_MenuAllowFlightCameraToRemapKeys( wxUpdateUIEvent& In );

	/** Called by WxWidgets to update the editor UI for 'Auto Restart Reimported Flash Movies' pref */
	void UI_MenuAutoRestartReimportedFlashMovies( wxUpdateUIEvent& In );

	void UI_MenuViewResizeViewportsTogether( wxUpdateUIEvent& In );
	void UI_MenuCenterZoomAroundCursor( wxUpdateUIEvent& In );
	void UI_MenuPanMovesCanvas( wxUpdateUIEvent& In );
	void UI_MenuReplaceRespectsScale( wxUpdateUIEvent& In );
	void UI_MenuDefaultToRealtimeMode( wxUpdateUIEvent& In );
	void UI_MenuToggleAbsoluteTranslation( wxUpdateUIEvent& In );
	void UI_MenuToggleTranslateRotateZWidget( wxUpdateUIEvent& In );
	void UI_MenuViewportHoverFeedback( wxUpdateUIEvent& In );
	void UI_MenuHighlightWithBrackets( wxUpdateUIEvent& In );
	void UI_MenuWireframeHalos( wxUpdateUIEvent& In );
	void UI_MenuUseStrictBoxSelection( wxUpdateUIEvent& In );
	void UI_MenuPromptSCCOnPackageModification( wxUpdateUIEvent& In );
	void UI_MenuAutoReimportTextures( wxUpdateUIEvent& In );
	void UI_MenuLanguageSelection( wxUpdateUIEvent& In );
	void UI_MenuAspectRatioSelection( wxUpdateUIEvent& In );
	void UI_MenuLoadSimpleLevelAtStartup( wxUpdateUIEvent& In );
	void UI_MenuAutoSaveOptions( wxUpdateUIEvent& In );
	void UI_MenuToggleLinkedOrthographicViewports( wxUpdateUIEvent& In );
	void UI_MenuToggleViewportCameraToUpdateFromPIV( wxUpdateUIEvent& In );
	void UI_MenuClickBSPSelectsBrush( wxUpdateUIEvent& In );
	void UI_MenuBSPAutoUpdate( wxUpdateUIEvent& In );

	void CoverEdit_ToggleEnabled( wxCommandEvent& In );
	void CoverEdit_ToggleAutoAdjust( wxCommandEvent& In );
	void CoverEdit_ToggleTypeAutomatic( wxCommandEvent& In );
	void CoverEdit_ToggleTypeStanding( wxCommandEvent& In );
	void CoverEdit_ToggleTypeMidLevel( wxCommandEvent& In );
	void CoverEdit_ToggleCoverslip( wxCommandEvent& In );
	void CoverEdit_ToggleSwatTurn( wxCommandEvent& In );
	void CoverEdit_ToggleMantle( wxCommandEvent& In );
	void CoverEdit_TogglePopup( wxCommandEvent& In );
	void CoverEdit_ToggleLeanLeft( wxCommandEvent& In );
	void CoverEdit_ToggleLeanRight( wxCommandEvent& In );
	void CoverEdit_ToggleClimbUp( wxCommandEvent& In );
	void CoverEdit_TogglePreferLean( wxCommandEvent& In );
	void CoverEdit_TogglePlayerOnly( wxCommandEvent& In );
	void CoverEdit_ToggleForceCanPopup( wxCommandEvent& In );
	void CoverEdit_ToggleForceCanCoverslip_Left( wxCommandEvent& In );
	void CoverEdit_ToggleForceCanCoverslip_Right( wxCommandEvent& In );

	void ObjectPropagationSelChanged( wxCommandEvent& In );
	void PushViewStartStop(wxCommandEvent& In);
	void PushViewSync(wxCommandEvent& In);

	void MenuPublishCook(wxCommandEvent& In);
	void MenuPublishCopy(wxCommandEvent& In);

	void MenuUpdateBaseToProcBuilding( wxCommandEvent& In );

	void MenuMoveCameraToPoint( wxCommandEvent& In );

#if WITH_FBX
	void MenuExportActorToFBX( wxCommandEvent& In );
#endif

	void OnClickedRealTimeAudio( wxCommandEvent& In );
	void UI_RealTimeAudioButton( wxUpdateUIEvent& In );
	void OnVolumeChanged( wxScrollEvent& In);

	/**
	 * Called whenever the user presses the toggle favorites button
	 *
	 * @param	In	Event automatically generated by wxWidgets whenever the toggle favorites button is pressed
	 */
	void OnClickedToggleFavorites( wxCommandEvent& In );

	/**
	 * Called whenever a user selects a favorite file from a menu to load
	 *
	 * @param	In	Event automatically generated by wxWidgets whenever a favorite file is selected by the user
	 */
	void MenuFileFavorite( wxCommandEvent& In );

	/**
	 * Generates a string path to the project executable
	 *
	 * @param	ProjectName	The name of the project to create a path for
	 */
	void CreateProjectPath(const FString& ProjectName);

	//Hidden bitmaps that are loaded on demand
	WxBitmap WizardB;
	WxBitmap DownArrowB, ArrowDown, ArrowUp;

	DECLARE_EVENT_TABLE();
};



/**
 * The menu that sits at the top of the main editor frame.
 */
class WxMainMenu
	: public wxMenuBar
{

public:
	/** Constructor */
	WxMainMenu();

	/** Destructor */
	virtual ~WxMainMenu();

	/** List of MRU and favorite files */
	class FMainMRUFavoritesList*	MRUFavoritesList;

	/** Menu of MRU files */
	wxMenu*							MRUMenu;

	/** Menu of favorite files */
	wxMenu*							FavoritesMenu;

	/** Combined menu of MRU and favorite files */
	wxMenu*							MRUFavoritesCombinedMenu;

	/** Menu item for displaying the lock/unlock read only levels tool */
	wxMenuItem*						LockReadOnlyLevelsItem;

	/** Console-specific menus */
	wxMenu*							ConsoleMenu[20]; // @todo: put a define somewhere for number of consoles and number of menu items per console

private:

	wxMenu*		FileMenu;
	wxMenu*		ImportMenu;
	wxMenu*		ExportMenu;
	wxMenu*		EditMenu;
	wxMenu*		ViewMenu;
	wxMenu*		BrowserMenu;
	wxMenu*		ViewportConfigMenu;
	wxMenu*		OpenNewFloatingViewportMenu;
	wxMenu*		DetailModeMenu;
	wxMenu*		LightingInfoMenu;
	wxMenu*		BrushMenu;
	wxMenu*		VolumeMenu;
	wxMenu*		BuildMenu;
	wxMenu*		PlayMenu;
	wxMenu*		ToolsMenu;
	wxMenu*		RenderModeMenu;
	wxMenu*		HelpMenu;
};



#endif // __EDITORFRAME_H__
