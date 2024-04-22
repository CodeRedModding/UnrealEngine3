/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/** Used during asset renaming/duplication to specify class-specific package/group targets. */
struct FClassMoveInfo
{
public:
	/** The type of asset this MoveInfo applies to. */
	FStringNoInit ClassName;
	/** The target package info which assets of this type are moved/duplicated. */
	FStringNoInit PackageName;
	/** The target group info which assets of this type are moved/duplicated. */
	FStringNoInit GroupName;
	/** If TRUE, this info is applied when moving/duplicating assets. */
	BITFIELD bActive:1;

	FClassMoveInfo() {}
    FClassMoveInfo(EEventParm)
    {
        appMemzero(this, sizeof(FClassMoveInfo));
    }
};

/** Used for copy/pasting sockets between skeletal meshes. */
struct FSkelSocketCopyInfo
{
	FName			SocketName;
	FName			BoneName;
	FVector			RelativeLocation;
	FRotator		RelativeRotation;
	FVector			RelativeScale;
};

/** Enum for the various states a package can be in for the package modification balloon notifications */
enum EPackageNotifyState
{
	// The user has been prompted with the balloon taskbar message.
	NS_BalloonPrompted, 
	// The user responded to the balloon task bar message and got the modal prompt to checkout dialog and responded to it
	// We will not show packages with this state in any calls to the modal checkout dialog caused by a user clicking on the balloon
	NS_DialogPrompted, 
	// The package has been marked dirty and is pending a balloon prompt.
	NS_PendingPrompt,
};

enum EEngineVerWarningState
{
	// The user needs to be warned about the package
	VWS_PendingWarn,
	// The user has been warned about the package
	VWS_Warned,
	// Warning for the package unnecessary
	VWS_WarningUnnecessary,
};

class UUnrealEdEngine : public UEditorEngine, public FNotifyHook
{
	DECLARE_CLASS_NOEXPORT(UUnrealEdEngine,UEditorEngine,CLASS_Transient|CLASS_Config,UnrealEd)

protected:
	/** Global instance of the editor options class. */
	UUnrealEdOptions* EditorOptionsInst;

	/**
	 * Manager responsible for creating and configuring browser windows
	 */
	UBrowserManager* BrowserManager;
	/** Handles all thumbnail rendering and configuration. */
	UThumbnailManager* ThumbnailManager;

public:
	/**
	 * Holds the name of the browser manager class to instantiate
	 */
	FStringNoInit BrowserManagerClassName;

	/**
	 * Holds the name of the class to instantiate
	 */
	FStringNoInit ThumbnailManagerClassName;

	/** The current autosave number, appended to the autosave map name, wraps after 10 */
	INT				AutoSaveIndex;
	/** The number of seconds since the last autosave. */
	FLOAT			AutosaveCount;
	/** Pause ticking of the autosave counter? */
	BITFIELD		bAutosaveCountPaused:1;

	/** If we are currently autosaving */
	BITFIELD bIsAutoSaving:1;

	/** A buffer for implementing material expression copy/paste. */
	UMaterial*		MaterialCopyPasteBuffer;

	/** A buffer for implementing matinee track/group copy/paste. */
	TArray<UObject*>	MatineeCopyPasteBuffer;

	/** A buffer for implementing sound cue nodes copy/paste. */
	USoundCue*		SoundCueCopyPasteBuffer;

	/** A buffer for implementing socket copy/paste. */
	TArray<FSkelSocketCopyInfo>	SkelSocketPasteBuffer;

	/** Global list of instanced animation compression algorithms. */
	TArray<class UAnimationCompressionAlgorithm*>	AnimationCompressionAlgorithms;

	/** Array of packages to be fully loaded at Editor startup. */
	TArrayNoInit<FString>	PackagesToBeFullyLoadedAtStartup;

	/** class names of Kismet objects to hide in the menus (i.e. because they aren't applicable for this game) */
	TArray<FName> HiddenKismetClassNames;

	/** Names of 'approved' ProcBuilding Ruleset collections */
	TArrayNoInit<FString> ApprovedPBRulesetCollections;

	/** Used during asset renaming/duplication to specify class-specific package/group targets. */
	TArray<FClassMoveInfo>	ClassRelocationInfo;

	/** Current target for LOD parenting operations (actors will use this as the replacement) */
	AActor* CurrentLODParentActor;

	/** If we have packages that are pending and we should notify the user that they need to be checkedout */	
	BITFIELD bNeedToPromptForCheckout:1;

	/** Whether the user needs to be prompted about a package being saved with an engine version newer than the current one or not */
	BITFIELD bNeedWarningForPkgEngineVer:1;

	/** A mapping of packages to their checkout notify state.  This map only contains dirty packages.  Once packages become clean again, they are removed from the map.*/
	TMap<UPackage*, BYTE> PackageToNotifyState;

	/** Map to track which packages have been checked for engine version when modified */
	TMap<FString, BYTE> PackagesCheckedForEngineVersion;

	/** Array of sorted, localized editor sprite categories */
	TArrayNoInit<FString> SortedSpriteCategories;

	/** Mapping of unlocalized sprite category names to their matching indices in the sorted sprite categories array */
	TMap<FName, INT> UnlocalizedCategoryToIndexMap;

	// FNotify interface.
	void NotifyDestroy( void* Src );
	void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange );
	void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );
	void NotifyExec( void* Src, const TCHAR* Cmd );

	// Selection.
	virtual void SelectActor(AActor* Actor, UBOOL bSelect, FViewportClient* InViewportClient, UBOOL bNotify, UBOOL bSelectEvenIfHidden=FALSE);
	virtual void SelectGroup(AGroupActor* InGroupActor, UBOOL bForceSelection=FALSE, UBOOL bInSelected=TRUE);
	

	/**
	 * Selects or deselects a BSP surface in the persistent level's UModel.  Does nothing if GEdSelectionLock is TRUE.
	 *
	 * @param	InModel					The model of the surface to select.
	 * @param	iSurf					The index of the surface in the persistent level's UModel to select/deselect.
	 * @param	bSelected				If TRUE, select the surface; if FALSE, deselect the surface.
	 * @param	bNoteSelectionChange	If TRUE, call NoteSelectionChange().
	 */
	virtual void SelectBSPSurf(UModel* InModel, INT iSurf, UBOOL bSelected, UBOOL bNoteSelectionChange);

	/**
	 * Deselect all actors.  Does nothing if GEdSelectionLock is TRUE.
	 *
	 * @param	bNoteSelectionChange		If TRUE, call NoteSelectionChange().
	 * @param	bDeselectBSPSurfs			If TRUE, also deselect all BSP surfaces.
	 */
	virtual void SelectNone(UBOOL bNoteSelectionChange, UBOOL bDeselectBSPSurfs, UBOOL WarnAboutManyActors=TRUE);

	/**
	* Updates the mouse position status bar field.
	*
	* @param PositionType	Mouse position type, used to decide how to generate the status bar string.
	* @param Position		Position vector, has the values we need to generate the string.  These values are dependent on the position type.
	*/
	virtual void UpdateMousePositionText( EMousePositionType PositionType, const FVector &Position );


	// General functions.
	
	/** Updates the property windows of selected actors */
	virtual void UpdatePropertyWindows();

	/** 
	*	Updates the property windows of the actors in the supplied ActorList
	*
	*	@param	ActorList	The list of actors whose property windows should be updated
	*
	*/
	virtual void UpdatePropertyWindowFromActorList( const TArray< UObject *>& ActorList );

	/**
	 * Fast track function to set render thread flags marking selection rather than reconnecting all components
	 * @param InActor - the actor to toggle view flags for
	 */
	virtual void SetActorSelectionFlags (AActor* InActor);
	virtual void NoteSelectionChange();
	virtual void NoteActorMovement();
	virtual void FinishAllSnaps();

	/**
	 * Cleans up after major events like e.g. map changes.
	 *
	 * @param	ClearSelection	Whether to clear selection
	 * @param	Redraw			Whether to redraw viewports
	 * @param	TransReset		Human readable reason for resetting the transaction system
	 */
	virtual void Cleanse( UBOOL ClearSelection, UBOOL Redraw, const TCHAR* TransReset );

	/**
	 * Replaces the specified actor with a new actor of the specified class.  The new actor
	 * will be selected if the current actor was selected.
	 * 
	 * @param	CurrentActor			The actor to replace.
	 * @param	NewActorClass			The class for the new actor.
	 * @param	Archetype				The template to use for the new actor.
	 * @param	bNoteSelectionChange	If TRUE, call NoteSelectionChange if the new actor was created successfully.
	 * @return							The new actor.
	 */
	virtual AActor* ReplaceActor( AActor* CurrentActor, UClass* NewActorClass, UObject* Archetype, UBOOL bNoteSelectionChange );

	/**
	 * Creates an archetype based on the parameters specified.  If PackageName or
	 * ArchetypeName are not specified, displays a prompt to the user.
	 *
	 * @param	ArchetypeBase	the object to create the archetype from
	 * @param	ArchetypeName	name for the archetype
	 * @param	PackageName		package for the archetype
	 * @param	GroupName		group for the archetype
	 *
	 * @return	a pointer to the archetype that was created
	 */
	virtual UObject* Archetype_CreateFromObject( UObject* ArchetypeBase, FString& ArchetypeName, FString& PackageName, FString& GroupName );

	virtual void Prefab_UpdateFromInstance( class APrefabInstance* Instance, TArray<UObject*>& NewObjects );

	virtual class UPrefab* Prefab_NewPrefab( TArray<UObject*> InObjects );

	virtual void Prefab_CreatePreviewImage( class UPrefab* InPrefab );
	
	virtual void Prefab_ConvertToNormalActors( class APrefabInstance* Instance );

	/**
	 * Returns whether or not the map build in progressed was cancelled by the user. 
	 */
	virtual UBOOL GetMapBuildCancelled() const;

	/**
	 * Sets the flag that states whether or not the map build was cancelled.
	 *
	 * @param InCancelled	New state for the cancelled flag.
	 */
	virtual void SetMapBuildCancelled( UBOOL InCancelled );

	/**
	 * @return Returns the global instance of the editor options class.
	 */
	UUnrealEdOptions* GetUnrealEdOptions();

	// UnrealEdSrv stuff.
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	UBOOL Exec_Edit( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Pivot( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Actor( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Mode( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_SkeletalMesh( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Group( const TCHAR* Str, FOutputDevice& Ar );

	virtual FVector GetPivotLocation();
	/**
	 * Sets the editor's pivot location, and optionally the pre-pivots of actors.
	 *
	 * @param	NewPivot				The new pivot location
	 * @param	bSnapPivotToGrid		If TRUE, snap the new pivot location to the grid.
	 * @param	bIgnoreAxis				If TRUE, leave the existing pivot unaffected for components of NewPivot that are 0.
	 * @param	bAssignPivot			If TRUE, assign the given pivot to any valid actors that retain it (defaults to FALSE)
	 */
	virtual void SetPivot(FVector NewPivot, UBOOL bSnapPivotToGrid, UBOOL bIgnoreAxis, UBOOL bAssignPivot=FALSE);
	virtual void ResetPivot();

	// Editor actor virtuals from UnEdAct.cpp.
	/**
	 * Select all actors and BSP models, except those which are hidden.
	 */
	virtual void edactSelectAll( UBOOL UseLayerSelect = FALSE );

	/**
	 * Invert the selection of all actors and BSP models.
	 */
	virtual void edactSelectInvert();

	/**
	 * Select any actors based on currently selected actors 
	 */
	virtual void edactSelectBased();

	/**
	 * Select all actors in a particular class.
	 */
	virtual void edactSelectOfClass( UClass* Class );

	/**
	 * Select all actors of a particular class and archetype.
	 *
	 * @param	InClass		Class of actor to select
	 * @param	InArchetype	Archetype of actor to select
	 */
	virtual void edactSelectOfClassAndArchetype( const UClass* InClass, const UObject* InArchetype );

	/**
	 * Select all actors in a particular class and its subclasses.
	 */
	virtual void edactSelectSubclassOf( UClass* Class );
	
	/**
	 * Select all actors in a level that are marked for deletion.
	 */
	virtual void edactSelectDeleted();
	
	/**
	 * Select all actors that have the same static mesh assigned to them as the selected ones.
	 *
	 * @param bAllClasses		If TRUE, also select non-AStaticMeshActor actors whose meshes match.
	 */
	virtual void edactSelectMatchingStaticMesh(UBOOL bAllClasses);

	/**
	 * Select all actors that have the same skeletal mesh assigned to them as the selected ones.
	 *
	 * @param bAllClasses		If TRUE, also select non-ASkeletalMeshActor actors whose meshes match.
	 */
	virtual void edactSelectMatchingSkeletalMesh(UBOOL bAllClasses);

	/**
	 * Select all material actors that have the same material assigned to them as the selected ones.
	 */
	virtual void edactSelectMatchingMaterial();

	/**
	 * Select all emitter actors that have the same particle system template assigned to them as the selected ones.
	 */
	virtual void edactSelectMatchingEmitter();

	/**
	* Select all proc buildings that use the same ruleset as this one.
	*/
	virtual void edactSelectMatchingProcBuildingsByRuleset();

	/**
	 * Selects actors in the current level based on whether or not they're referenced by Kismet.
	 *
	 * @param	bReferenced		If TRUE, select actors referenced by Kismet; if FALSE, select unreferenced actors.
	 * @param	bCurrent			If TRUE, select actors in current level; if FALSE, select actors from all levels.
	 */
	virtual void edactSelectKismetReferencedActors(UBOOL bReferenced, UBOOL bCurrent);

	/**
	 * Select the relevant lights for all selected actors
	 */
	virtual void edactSelectRelevantLights(UBOOL bDominantOnly);

	/** Selects all actors that have the same SpeedTree assigned to them as the selected ones. */
	virtual void SelectMatchingSpeedTrees();

	/**
	 * Deletes all selected actors.  bIgnoreKismetReferenced is ignored when bVerifyDeletionCanHappen is TRUE.
	 *
	 * @param		bVerifyDeletionCanHappen	[opt] If TRUE (default), verify that deletion can be performed.
	 * @param		bIgnoreKismetReferenced		[opt] If TRUE, don't delete actors referenced by Kismet.
	 * @return									TRUE unless the delete operation was aborted.
	 */
	virtual UBOOL edactDeleteSelected(UBOOL bVerifyDeletionCanHappen=TRUE, UBOOL bIgnoreKismetReferenced=FALSE);
	
	/**
	 * Create archetypes of the selected actors, and replace those actors with instances of
	 * the archetype class.
	 */
	virtual void edactArchetypeSelected();

	/**
	 *  Update archetype of the selected actor
	 */
	virtual void edactUpdateArchetypeSelected();

	/**
	 * Create a prefab from the selected actors, and replace those actors with an instance of that prefab.
	 */
	virtual void edactPrefabSelected();

	/**
	 * Add the selected prefab at the clicked location.
	 */
	virtual void edactAddPrefab();

	/**
	 * Select all Actors that make up the selected PrefabInstance.
	 */
	virtual void edactSelectPrefabActors();

	/**
	 * Update a prefab from the selected PrefabInstance.
	 */
	virtual void edactUpdatePrefabFromInstance();

	/**
	 * Reset a prefab instance from the prefab.
	 */
	virtual void edactResetInstanceFromPrefab();

	/**
	 * Convert a prefab instance back into normal actors.
	 */
	virtual void edactPrefabInstanceToNormalActors();

	/**
	 * Creates a new group from the current selection maintaining existing groups.
	 */
	virtual UBOOL edactGroupFromSelected();

	/**
	 * Creates a new group from the current selection removing any existing groups.
	 */
	virtual UBOOL edactRegroupFromSelected();

	/**
	 * Disbands any groups in the current selection, does not attempt to maintain any hierarchy
	 */
	virtual UBOOL edactUngroupFromSelected();
	
	/**
	 * Locks any groups in the current selection
	 */
	virtual UBOOL edactLockSelectedGroups();

	/**
	 * Unlocks any groups in the current selection
	 */
	virtual UBOOL edactUnlockSelectedGroups();

	/**
	 * Activates "Add to Group" mode which allows the user to select a group to append current selection
	 */
	virtual UBOOL edactAddToGroup();

	/** 
	 * Removes any groups or actors in the current selection from their immediate parent.
	 * If all actors/subgroups are removed, the parent group will be destroyed.
	 */
	virtual UBOOL edactRemoveFromGroup();

	/**
	 * Gather vertex/triangle/mesh/material stats for any static meshes in the selected group
	 */
	virtual UBOOL edactReportStatsForSelectedGroups();

	/**
	 * Gather vertex/triangle/mesh/material stats for any static meshes in the current selection
	 */
	virtual UBOOL edactReportStatsForSelection();

	/**
	 * Copy selected actors to the clipboard.  Does not copy PrefabInstance actors or parts of Prefabs.
	 *
	 * @param	bReselectPrefabActors	If TRUE, reselect any actors that were deselected prior to export as belonging to prefabs.
	 * @param	bClipPadCanBeUsed		If TRUE, the clip pad is available for use if the user is holding down SHIFT.
	 * @param	DestinationData			If != NULL, additionally copy data to string
	 */
	virtual void edactCopySelected(UBOOL bReselectPrefabActors, UBOOL bClipPadCanBeUsed, FString* DestinationData = NULL);

	/**
	 * Paste selected actors from the clipboard.
	 *
	 * @param	bDuplicate			Is this a duplicate operation (as opposed to a real paste)?
	 * @param	bOffsetLocations	Should the actor locations be offset after they are created?
	 * @param	bClipPadCanBeUsed	If TRUE, the clip pad is available for use if the user is holding down SHIFT.
	 * @param	SourceData			If != NULL, use instead of clipboard data
	 */
	virtual void edactPasteSelected(UBOOL bDuplicate, UBOOL bOffsetLocations, UBOOL bClipPadCanBeUsed, FString* SourceData = NULL);

	/** 
	 * Duplicates selected actors.  Handles the case where you are trying to duplicate PrefabInstance actors.
	 *
	 * @param	bUseOffset		Should the actor locations be offset after they are created?
	 */
	virtual void edactDuplicateSelected(UBOOL bUseOffset);

	/**
	 * Replace all selected brushes with the default brush.
	 */
	virtual void edactReplaceSelectedBrush();

	/**
	 * Replace all selected non-brush actors with the specified class.
	 */
	virtual void edactReplaceSelectedNonBrushWithClass(UClass* Class);

	/**
	 * Replace all actors of the specified source class with actors of the destination class.
	 *
	 * @param	SrcClass	The class of actors to replace.
	 * @param	DstClass	The class to replace with.
	 */
	virtual void edactReplaceClassWithClass(UClass* SrcClass, UClass* DstClass);

	/**
	* Align the origin with the current grid.
	*/
	virtual void edactAlignOrigin();

	/**
	 * Align all vertices with the current grid.
	 */
	virtual void edactAlignVertices();

	/**
	 * Hide selected actors and BSP models by marking their bHiddenEdTemporary flags TRUE. Will not
	 * modify/dirty actors/BSP.
	 */
	virtual void edactHideSelected();

	/**
	 * Hide unselected actors and BSP models by marking their bHiddenEdTemporary flags TRUE. Will not
	 * modify/dirty actors/BSP.
	 */
	virtual void edactHideUnselected();

	/**
	 * Attempt to unhide all actors and BSP models by setting their bHiddenEdTemporary flags to FALSE if they
	 * are TRUE. Note: Will not unhide actors/BSP hidden by higher priority visibility settings, such as bHiddenEdGroup,
	 * but also will not modify/dirty actors/BSP.
	 */
	virtual void edactUnHideAll();

	/**
	 * Mark all selected actors and BSP models to be hidden upon editor startup, by setting their bHiddenEd flag to
	 * TRUE, if it is not already. This directly modifies/dirties the relevant actors/BSP.
	 */
	virtual void edactHideSelectedStartup();

	/**
	 * Mark all actors and BSP models to be shown upon editor startup, by setting their bHiddenEd flag to FALSE, if it is
	 * not already. This directly modifies/dirties the relevant actors/BSP.
	 */
	virtual void edactUnHideAllStartup();

	/**
	 * Mark all selected actors and BSP models to be shown upon editor startup, by setting their bHiddenEd flag to FALSE, if it
	 * not already. This directly modifies/dirties the relevant actors/BSP.
	 */
	virtual void edactUnHideSelectedStartup();

	/** Returns the configuration of attachment that would result from calling AttachSelectedActors at this point in time */
	AActor* GetDesiredAttachmentState(TArray<AActor*>& OutNewChildren);

	/** Uses the current selection state to attach actors together (using SetBase). Last selected Actor becomes the base. */
	void AttachSelectedActors();
	
	/** Adds all selected actors to the attachment editor */
	void AddSelectedToAttachmentEditor();

	/**
	 * Redraws all level editing viewport clients.
	 *
	 * @param	bInvalidateHitProxies		[opt] If TRUE (the default), invalidates cached hit proxies too.
	 */
	virtual void RedrawLevelEditingViewports(UBOOL bInvalidateHitProxies=TRUE);

	/**
	 * Pastes clipboard text into a clippad entry
	 */
	virtual void PasteClipboardIntoClipPad();

	// Hook replacements.
	void ShowActorProperties();
	void ShowWorldProperties( const UBOOL bExpandTo = FALSE, const FString& PropertyName = TEXT( "" ) );

	/**
	 * Checks to see if any worlds are dirty (that is, they need to be saved.)
	 *
	 * @return TRUE if any worlds are dirty
	 */
	UBOOL AnyWorldsAreDirty() const;

	// Misc
	/** Attempts to autosave the level and/or content packages, if those features are enabled. */
	void AttemptAutosave();

	/**
	 * Attempts to prompt the user with a balloon notification to checkout modified packages from source control.  
	 * Will defer prompting the user if they are interacting with something
	 */
	void AttemptModifiedPackageNotification();

	/** 
	 * Alerts the user to any packages that have been modified which have been previously saved with an engine version newer than
	 * the current version. These packages cannot be saved, so the user should be alerted ASAP.
	 */
	void AttemptWarnAboutPackageEngineVersions();

	/** 
	 * Prompts the user with a modal checkout dialog to checkout packages from source control.   
	 * This should only be called by the auto prompt to checkout package notification system.
	 * For a general checkout packages routine use FEditorFileUtils::PromptToCheckoutPackages
	 *
	 * @param bPromptAll	If true we prompt for all packages in the PackageToNotifyState map.  If false only prompt about ones we have never prompted about before.
	 */
	void PromptToCheckoutModifiedPackages( UBOOL bPromptAll = FALSE ); 

	/**
	 * Checks to see if there are any packages in the PackageToNotifyState map that are not checked out by the user
	 *
	 * @return True if packages need to be checked out.
	 */
	UBOOL DoDirtyPackagesNeedCheckout() const;

	/**
	 * Called when a map is about to be unloaded so map packages should be removed from the package checkout data.
	 */
	void PurgeOldPackageCheckoutData();

	/**
	 * Returns true if the user is currently interacting with a viewport.
	 */
	UBOOL IsUserInteracting();

	void SetCurrentClass( UClass* InClass );
	virtual void GetPackageList( TArray<UPackage*>* InPackages, UClass* InClass );

	/**
	 * Checks the state of the selected actors and notifies the user of any potentially unknown destructive actions which may occur as
	 * the result of deleting the selected actors.  In some cases, displays a prompt to the user to allow the user to choose whether to
	 * abort the deletion.
	 *
	 * @param	bOutIgnoreKismetReferenced	[out] Set only if it's okay to delete actors; specifies if the user wants Kismet-refernced actors not deleted.
	 * @return								FALSE to allow the selected actors to be deleted, TRUE if the selected actors should not be deleted.
	 */
	virtual UBOOL ShouldAbortActorDeletion(UBOOL& bOutIgnoreKismetReferenced) const;

	/**
	* Closes the main editor frame.
	*/ 
	virtual void CloseEditor();
	virtual void ShowUnrealEdContextMenu();
	virtual void ShowUnrealEdContextSurfaceMenu();
	virtual void ShowUnrealEdContextCoverSlotMenu(class ACoverLink *Link, FCoverSlot &Slot);

	/**
	 * @return TRUE if selection of translucent objects in perspective viewports is allowed
	 */
	virtual UBOOL AllowSelectTranslucent() const;

	/**
	 * @return TRUE if only editor-visible levels should be loaded in Play-In-Editor sessions
	 */
	virtual UBOOL OnlyLoadEditorVisibleLevelsInPIE() const;


	/**
	 * If all selected actors belong to the same level, that level is made the current level.
	 */
	void MakeSelectedActorsLevelCurrent();

	/**
	 * If all selected actors belong to the same level grid volume, that level grid volume is made current.
	 */
	void MakeSelectedActorsLevelGridVolumeCurrent();

	/** Returns the thumbnail manager and creates it if missing */
	UThumbnailManager* GetThumbnailManager(void);

	/** Returns the browser manager and creates it if missing */
	UBrowserManager* GetBrowserManager(void);

	/**
	 * Returns the named browser, statically cast to the specified type.
	 *
	 * @param BrowserName	Browser name; the same name that would be passed to UBrowserManager::GetBrowserPane.
	 */
	template<class BrowserType>
	BrowserType* GetBrowser(const TCHAR* BrowserName)
	{
		return static_cast<BrowserType*>( GetBrowserManager()->GetBrowserPane( BrowserName ) );
	}

	/** @return Returns whether or not the user is able to autosave. */
	UBOOL CanAutosave() const;
	
	/** @return Returns whether or not autosave is going to occur within the next minute. */
	UBOOL AutoSaveSoon() const;

	/** @return Returns the amount of time until the next autosave in seconds. */
	INT GetTimeTillAutosave() const;
	/**
	 * Resets the autosave timer.
	 */
	virtual void ResetAutosaveTimer();

	/**
	 * Pauses the autosave timer.
	 */
	virtual void PauseAutosaveTimer(UBOOL bPaused);

	// UObject interface.
	virtual void FinishDestroy();
	/** Serializes this object to an archive. */
	virtual void Serialize( FArchive& Ar );

	// UEngine Interface.
	virtual void Init();
	virtual void PreExit();
	virtual void Tick(FLOAT DeltaSeconds);

	// FCallbackEventDevice interface
	virtual void Send( ECallbackEventType InType, UObject* InObject );

	/**
	 * Creates an embedded Play In Editor viewport window (if possible)
	 *
	 * @param ViewportClient The viewport client the new viewport will be associated with
	 * @param InPlayInViewportIndex Viewport index to play in, or -1 for "don't care"
	 *
	 * @return TRUE if successful
	 */
	virtual UBOOL CreateEmbeddedPIEViewport( UGameViewportClient* ViewportClient, INT InPlayInViewportIndex );


	/**
	 * Returns whether saving the specified package is allowed
	 */
	virtual UBOOL CanSavePackage( UPackage* PackageToSave );

	/**
	 * Creates an editor derived from the wxInterpEd class.
	 *
	 * @return		A heap-allocated WxInterpEd instance.
	 */
	virtual WxInterpEd	*CreateInterpEditor( wxWindow* InParent, wxWindowID InID, class USeqAct_Interp* InInterp );

	/**
	 * Creates an editor derived from the WxCameraAnimEd class.
	 *
	 * @return		A heap-allocated WxCameraAnimEd instance.
	 */
	virtual WxCameraAnimEd	*CreateCameraAnimEditor( wxWindow* InParent, wxWindowID InID, class USeqAct_Interp* InInterp );

	/**
	 * Rerender proc building render-to-textures, because lighting has changed and needs to be baked into the textures
	 *
	 * @param bRenderTextures	if TRUE, use render-to-texture to generate unique textures for buildings, if FALSE, set all buildings back to default texture
	 * @param Buildings			optional list of buildings to rerender. If empty, this will rerender all buildings in the world
	 */
	void RegenerateProcBuildingTextures(UBOOL bRenderTextures, TArray<class AProcBuilding*> Buildings=TArray<class AProcBuilding*>());

	/** Renders the scene to texture for each ImageReflectionSceneCapture in ReflectionActors, or all the ones in the scene if ReflectionActors is empty. */
	void RegenerateImageReflectionTextures(const TArray<class AImageReflectionSceneCapture*>& ReflectionActors);

	/** Finds all PrcBuildings that were edited while in quick mode, and regen's them */
	void RegenQuickModeEditedProcBuildings();

	/** Returns the ruleset of the first selected building to have one assigned */
	class UProcBuildingRuleset* GetSelectedBuildingRuleset();

	/** Returns the ruleset of the building that is being geometry edited.  Will return NULL if not in geom mode. */
	class UProcBuildingRuleset* GetGeomEditedBuildingRuleset();

	/** Apply the supplied parameter swatch name to the currently selected building(s) */
	void ApplyParamSwatchToSelectedBuildings(FName SwatchName);

	/** Apply the supplied variation name to the currently selected faces (only works in geom mode) */
	void ApplyRulesetVariationToSelectedFaces(FName VariationName);
	
	/** Apply supplied material to faces of ProcBuilding, for use on roof and non-rect faces */
	void ApplyMaterialToPBFaces(UMaterialInterface* Material);

	/** Clear any face ruleset assignments on the selected buildings */
	void ClearRulesetVariationFaceAssignments();
	
	/** Clear any face material assignments on the selected buildings */
	void ClearPBMaterialFaceAssignments();
	
	/** Show info window showing resources used by currently selected ProcBuilding actors */
	void ShowPBResourceInfo();
	
	/** Select base-most building from the one that is selected */
	void SelectBasePB();
	
	/** Group selected procedural buildings */
	void GroupSelectedPB();
	
	/** Convert select additive BSP brushes into a ProcBuilding */
	void ConvertBSPToProcBuilding();

	/** Rebuild all buildings in the level  */
	void CleanupOldBuildings(UBOOL bSelectedOnly, UBOOL bCheckVersion);

	/** Fix up the textures and materials associated with buildings, that had the wrong flags set on them  */
	void CleanupOldBuildingTextures();

	/** Iterate over all InstancedStaticMeshComponents and see how many instances each have. Info printed to log. */
	void InstancedMeshComponentCount();

	/** 
	 * Brings up a dialog, allowing a user to edit the play world url
	 *
	 * @param ConsoleIndex			A valid console index or -1 if editing for an play in editor session.  This index sets up the proper map name to display in the url edit dialog.
	 * @param bUseMobilePreview		True to use mobile preview mode (only supported on PC)
	 */
	void EditPlayWorldURL(INT ConsoleIndex = -1, const UBOOL bUseMobilePreview = FALSE);

	/** See if the supplied ProcBuilding Ruleset is 'approved' - that is, appears in the AuthorizedPBRulesetCollections list. */
	UBOOL CheckPBRulesetIsApproved(const UProcBuildingRuleset* Ruleset);

	/**
	 * Updates the volume actor visibility for all viewports based on the passed in volume class
	 * 
	 * @param InVolumeActorClass	The type of volume actors to update.  If NULL is passed in all volume actor types are updated.
	 * @param InViewport			The viewport where actor visibility should apply.  Pass NULL for all editor viewports.
	 */
	void UpdateVolumeActorVisibility( const UClass* InVolumeActorClass = NULL , FEditorLevelViewportClient* InViewport = NULL);

	/** Hook for game stats tool to render things in viewport. */
	virtual void GameStatsRender(FEditorLevelViewportClient* ViewportClient, const FSceneView* View, FCanvas* Canvas, ELevelViewportType ViewportType);
	/** Hook for game stats tool to render things in 3D viewport. */
	virtual void GameStatsRender3D(const FSceneView* View,class FPrimitiveDrawInterface* PDI, ELevelViewportType ViewportType);
	/** Hook for sentinel to be informed about mouse movements (for tool tip) */
	virtual void GameStatsMouseMove(FEditorLevelViewportClient* ViewportClient, INT X, INT Y);
	/** Hook for game to be informed about key input */
	virtual void GameStatsInputKey(FEditorLevelViewportClient* ViewportClient, FName Key,EInputEvent Event);

	virtual void SentinelStatRender(FEditorLevelViewportClient* ViewportClient, const FSceneView* View, FCanvas* Canvas);
	virtual void SentinelStatRender3D(const FSceneView* View,class FPrimitiveDrawInterface* PDI);
	virtual void SentinelMouseMove(FEditorLevelViewportClient* ViewportClient, INT X, INT Y);
	virtual void SentinelInputKey(FEditorLevelViewportClient* ViewportClient, FName Key,EInputEvent Event);

	/**
	 * Get the index of the provided sprite category
	 *
	 * @param	InSpriteCategory	Sprite category to get the index of
	 *
	 * @return	Index of the provided sprite category, if possible; INDEX_NONE otherwise
	 */
	virtual INT GetSpriteCategoryIndex( const FName& InSpriteCategory );

	/**
	 * Displays a prompt to save and check in or just mark as dirty any material packages with dependent changes (either parent materials or
	 * material functions) as determined on load.
	 *
	 */
	void PromptToSaveChangedDependentMaterialPackages();

	friend class WxGenericBrowser;
};


