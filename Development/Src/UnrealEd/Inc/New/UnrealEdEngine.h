/*=============================================================================
	UnrealEdEngine.h: UnrealEdEngine definition

	* Created by Warren Marshall
=============================================================================*/

class UUnrealEdEngine : public UEditorEngine, public FNotifyHook
{
	DECLARE_CLASS(UUnrealEdEngine,UEditorEngine,CLASS_Transient|CLASS_Config,UnrealEd)

	HWND hWndMain;

	// FNotify interface.
	void NotifyDestroy( void* Src );
	void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange );
	void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );
	void NotifyExec( void* Src, const TCHAR* Cmd );

	// Selection.
	virtual void SelectActor( ULevel* Level, AActor* Actor, UBOOL bSelect = 1, UBOOL bNotify = 1 );
	virtual void SelectBSPSurf( ULevel* Level, INT iSurf, UBOOL bSelect = 1, UBOOL bNotify = 1 );
	virtual void SelectNone( ULevel* Level, UBOOL Notify, UBOOL BSPSurfs = 1 );

	// General functions.
	virtual void UpdatePropertyWindows();

	virtual void NoteSelectionChange( ULevel* Level );
	virtual void NoteActorMovement( ULevel* Level );
	virtual void FinishAllSnaps( ULevel* Level );

	// UnrealEdSrv stuff.
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	UBOOL Exec_Edit( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Pivot( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Actor( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Prefab( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Mode( const TCHAR* Str, FOutputDevice& Ar );
	
	virtual void SetPivot( FVector NewPivot, UBOOL SnapPivotToGrid, UBOOL MoveActors, UBOOL bIgnoreAxis );
	virtual FVector GetPivotLocation();
	virtual void ResetPivot();

	// Editor actor virtuals from UnEdAct.cpp.
	virtual void edactSelectAll( ULevel* Level );
	virtual void edactSelectInside( ULevel* Level );
	virtual void edactSelectInvert( ULevel* Level );
	virtual void edactSelectOfClass( ULevel* Level, UClass* Class );
	virtual void edactSelectSubclassOf( ULevel* Level, UClass* Class );
	virtual void edactSelectDeleted( ULevel* Level );
	virtual void edactSelectMatchingStaticMesh( ULevel* Level );
	virtual void edactSelectMatchingZone( ULevel* Level );
	virtual void edactDeleteSelected( ULevel* Level );
	virtual void edactCopySelected( ULevel* Level );
	virtual void edactPasteSelected( ULevel* InLevel, UBOOL InDuplicate, UBOOL InUseOffset );
	virtual void edactReplaceSelectedBrush( ULevel* Level );
	virtual void edactReplaceSelectedWithClass( ULevel* Level, UClass* Class );
	virtual void edactReplaceClassWithClass( ULevel* Level, UClass* Class, UClass* WithClass );
	virtual void edactAlignVertices( ULevel* Level );
	virtual void edactHideSelected( ULevel* Level );
	virtual void edactHideUnselected( ULevel* Level );
	virtual void edactUnHideAll( ULevel* Level );
	virtual void edactApplyTransform( ULevel* Level );
	virtual void edactApplyTransformToBrush( ABrush* InBrush );

	// Editor rendering functions.
	virtual void RedrawLevel(ULevel* Level);

	// Hook replacements.
	void ShowActorProperties();
	void ShowLevelProperties();
	void ShowClassProperties( UClass* Class );
	void ShowSoundProperties( USoundNodeWave* Sound );

	// Misc
	void SetCurrentClass( UClass* InClass );
	void GetPackageList( TArray<UPackage*>* InPackages, UClass* InClass );

	virtual void ShowUnrealEdContextMenu();
	virtual void ShowUnrealEdContextSurfaceMenu();

	// UEngine Interface.

	/**
	 * Looks at all currently loaded packages and prompts the user to save them
	 * if their "bDirty" flag is set.
	 */
	virtual void SaveDirtyPackages();
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
