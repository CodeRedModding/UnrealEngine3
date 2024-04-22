/*=============================================================================
	PlayLevel.cpp: In-editor level playing.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineAnimClasses.h"
#include "RemoteControl.h"
#include "LevelUtils.h"
#include "BusyCursor.h"
#include "ScopedTransaction.h"
#include "UnConsoleSupportContainer.h"
#include "Database.h"
#include "CrossLevelReferences.h"
#include "FileHelpers.h"
#include "UnPackageTools.h"
#include "Kismet.h"
#include "KismetDebugger.h"

#if WITH_SUBSTANCE_AIR
#include "SubstanceAirEdHelpers.h"
#endif

extern UBOOL GKismetRealtimeDebugging;

UBOOL UEditorPlayer::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	UBOOL Handled = FALSE;

	if( ParseCommand(&Cmd,TEXT("CloseEditorViewport")) 
	||	ParseCommand(&Cmd,TEXT("Exit")) 
	||	ParseCommand(&Cmd,TEXT("Quit")))
	{
		ViewportClient->CloseRequested(ViewportClient->Viewport);
		Handled = TRUE;
	}
	else if( Super::Exec(Cmd,Ar) )
	{
		Handled = TRUE;
	}

	return Handled;
}
IMPLEMENT_CLASS(UEditorPlayer);

void UEditorEngine::EndPlayMap()
{
	const FScopedBusyCursor BusyCursor;
	check(PlayWorld);

#if USING_REMOTECONTROL
	if ( RemoteControlExec )
	{
		// Notify RemoteControl that PIE is ending.
		RemoteControlExec->OnEndPlayMap();
	}
#endif

	// Enable screensavers when ending PIE.
	GEngine->EnableScreenSaver( TRUE );

	// clear out the PIE cross level manager, we don't need it when cleaning up PIE world
	FCrossLevelReferenceManager::SwitchToPIEManager();
	GCrossLevelReferenceManager->Reset();
	FCrossLevelReferenceManager::SwitchToStandardManager();

	// let the editor know
	GCallbackEvent->Send(CALLBACK_EndPIE);

	// clean up any previous Play From Here sessions
	if ( GameViewport != NULL && GameViewport->Viewport != NULL )
	{
		GameViewport->CloseRequested(GameViewport->Viewport);
	}
	CleanupGameViewport();

	// no longer queued
	bIsPlayWorldQueued = FALSE;

	// Clear the spectate flag
	bStartInSpectatorMode = FALSE;

	// Clear out viewport index
	PlayInEditorViewportIndex = -1;

	// Change GWorld to be the play in editor world during cleanup.
	UWorld* EditorWorld = GWorld;
	GWorld = PlayWorld;
	GIsPlayInEditorWorld = TRUE;
	
	// tell any player controllers that they are about to be destroyed (special case for cleaning
	// up, since normally Destroyed won't be called due to GC deleting objects)
	for (FDynamicActorIterator It; It; ++It)
	{
		if (It->IsA(APlayerController::StaticClass()))
		{
			It->eventDestroyed();
		}
	}
	
	// Stop all audio and remove references to temp level.
	if( Client && Client->GetAudioDevice() )
	{
		Client->GetAudioDevice()->Flush( PlayWorld->Scene );
	}

	// find objects like Textures in the playworld levels that won't get garbage collected as they are marked RF_Standalone
	for( FObjectIterator It; It; ++It )
	{
		UObject* Object = *It;
		if( Object->HasAnyFlags(RF_Standalone) && (Object->GetOutermost()->PackageFlags & PKG_PlayInEditor)  )
		{
			// Clear RF_Standalone flag from objects in the levels used for PIE so they get cleaned up.
			Object->ClearFlags(RF_Standalone);
		}
	}

	if( GUseFastPIE )
	{
		// cleanup refs to any duplicated streaming levels
		AWorldInfo* WorldInfo = PlayWorld->GetWorldInfo();
		if( WorldInfo )
		{
			for ( INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++ )
			{
				ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
				if( StreamingLevel &&
					StreamingLevel->LoadedLevel )
				{
					UWorld* World = Cast<UWorld>( StreamingLevel->LoadedLevel->GetOuter() );
					if( World )
					{
						World->TermWorldRBPhys();
						World->CleanupWorld();
					}
					StreamingLevel->LoadedLevel = NULL;
				}
			}
		}
	}

	// Clean up the temporary play level.
	PlayWorld->TermWorldRBPhys();
	PlayWorld->CleanupWorld();
	PlayWorld = NULL;

	// Cleanup PIE sequence object references
	for ( TObjectIterator<USequenceObject> SeqObj; SeqObj; ++SeqObj )
	{
		SeqObj->PIESequenceObject = NULL;
	}

#if WITH_SUBSTANCE_AIR
	// restore graph instances which were modified during kismet sequence
	SubstanceAir::Helpers::RestoreGraphInstances();
#endif

	// Kismet realtime debugging
	UBOOL bWereKismetBreakpointsChanged = FALSE;
	{
		if(GKismetRealtimeDebugging)
		{
			bWereKismetBreakpointsChanged = WxKismet::HasBeenMarkedDirty();
			// Clear the callstack in case it is paused on execution
			WxKismetDebugger* KismetDebugger = WxKismetDebugger::FindKismetDebuggerWindow();
			if(KismetDebugger)
			{
				KismetDebugger->Callstack->ClearCallstack();
			}
			WxKismet::CloseAllKismetDebuggerWindows();
			WxKismet::SetVisibilityOnAllKismetWindows(TRUE);
		}
		
		// Re-enable the realtime Kismet debugging button on the main editor toolbar
		if (GApp->EditorFrame && GApp->EditorFrame->GetToolBar())
		{
			GApp->EditorFrame->GetToolBar()->EnableTool(IDM_KISMET_REALTIMEDEBUGGING, true);
		}
	}
	
	GIsPlayInEditorWorld = FALSE;
	// Restore GWorld.
	GWorld = EditorWorld;

	CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

	// Make sure that all objects in the temp levels were entirely garbage collected.
	for( FObjectIterator ObjectIt; ObjectIt; ++ObjectIt )
	{
		UObject* Object = *ObjectIt;
		if( Object->GetOutermost()->PackageFlags & PKG_PlayInEditor )
		{
			UObject::StaticExec(*FString::Printf(TEXT("OBJ REFS CLASS=WORLD NAME=%s.TheWorld"), *Object->GetOutermost()->GetName()));

			TMap<UObject*,UProperty*>	Route		= FArchiveTraceRoute::FindShortestRootPath( Object, TRUE, GARBAGE_COLLECTION_KEEPFLAGS );
			FString						ErrorString	= FArchiveTraceRoute::PrintRootPath( Route, Object );
				
			// We cannot safely recover from this.
			appErrorf( LocalizeSecure(LocalizeUnrealEd("Error_PIEObjectStillReferenced"), *Object->GetFullName(), *ErrorString) );
		}
	}

	// We'll clear the "dirty for PIE" bit on this package, since it's been saved out as a PIE map!
	// It is no longer necessary to restore the dirty flag because it is not changed when saved for a test run
	UPackage* Package = GWorld->GetOutermost();
	Package->ClearDirtyForPIEFlag();


	// Spawn note actors dropped in PIE.
	if(GEngine->PendingDroppedNotes.Num() > 0)
	{
		const FScopedTransaction Transaction( TEXT("Create PIE Notes") );

		for(INT i=0; i<GEngine->PendingDroppedNotes.Num(); i++)
		{
			FDropNoteInfo& NoteInfo = GEngine->PendingDroppedNotes(i);
			ANote* NewNote = Cast<ANote>( GWorld->SpawnActor(ANote::StaticClass(), NAME_None, NoteInfo.Location, NoteInfo.Rotation) );
			if(NewNote)
			{
				NewNote->Text = NoteInfo.Comment;
				NewNote->Tag = FName(*NoteInfo.Comment);
				NewNote->SetDrawScale(2.f);
			}
		}
		Package->MarkPackageDirty(TRUE);
		GEngine->PendingDroppedNotes.Empty();
	}

	// Mark package as dirty if any Kismet breakpoints were changed
	if (GKismetRealtimeDebugging && bWereKismetBreakpointsChanged)
	{
		Package->MarkPackageDirty(TRUE);
	}

	// Restores realtime viewports that have been disabled for PIE.
	RestoreRealtimeViewports();

	GEngine->DeferredCommands.AddUniqueItem(TEXT("UpdateLandscapeSetup"));
	
	// Start the autosave timer when PIE is finished.
	GEditor->PauseAutosaveTimer( FALSE );
}

void UEditorEngine::EndPlayOnConsole()
{
	// Check if there is a current play world destination console
	if( CurrentPlayWorldDestination != INDEX_NONE )
	{
		// If there is one, exit the game
		FConsoleSupport* CurrentConsole = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(CurrentPlayWorldDestination);
		if( CurrentConsole )
		{
			CurrentConsole->ResetTargetToShell(NULL, TRUE);
		}

		// Reset the play world destination to nothing
		CurrentPlayWorldDestination = INDEX_NONE;
	}
}

/**
 * Makes a request to start a play from editor session (in editor or on a remote platform)
 * @param	StartLocation			If specified, this is the location to play from (via a Teleporter - Play From Here)
 * @param	StartRotation			If specified, this is the rotation to start playing at
 * @param	DestinationConsole		Where to play the game - -1 means in editor, 0 or more is an index into the GConsoleSupportContainer
 * @param	InPlayInViewportIndex	Viewport index to play the game in, or -1 to spawn a standalone PIE window
 * @param	bUseMobilePreview		True to enable mobile preview mode (PC platform only)
 * @param	bMovieCapture			True to start with movie capture recording (PC platform only)
 */
void UEditorEngine::PlayMap( FVector* StartLocation, FRotator* StartRotation, INT Destination, INT InPlayInViewportIndex, UBOOL bUseMobilePreview, UBOOL bMovieCapture )
{
	// queue up a Play From Here request, this way the load/save won't conflict with the TransBuffer, which doesn't like 
	// loading and saving to happen during a transaction

	// save the StartLocation if we have one
	if (StartLocation)
	{
		PlayWorldLocation = *StartLocation;
		PlayWorldRotation = StartRotation ? *StartRotation : FRotator(0, 0, 0);
		bHasPlayWorldPlacement = TRUE;
	}
	else
	{
		bHasPlayWorldPlacement = FALSE;
	}

	// remember where to send the play map request
	PlayWorldDestination = Destination;

	// Set whether or not we want to use mobile preview mode (PC platform only)
	bUseMobilePreviewForPlayWorld = bUseMobilePreview;

	// Set whether or not we want to start movie capturing immediately (PC platform only)
	bStartMovieCapture = bMovieCapture;

	// tell the editor to kick it off next Tick()
	bIsPlayWorldQueued = TRUE;

	// do we want to spectate the map
	bStartInSpectatorMode = FALSE;
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
	{
		// if control is pressed, start in spectator mode
		bStartInSpectatorMode = TRUE;
	}

	// Unless we've been asked to play in a specific viewport window, this index will be -1
	PlayInEditorViewportIndex = InPlayInViewportIndex;
}

void UEditorEngine::StartQueuedPlayMapRequest()
{
	// note that we no longer have a queued request
	bIsPlayWorldQueued = FALSE;

	if (PlayWorldDestination == -1)
	{
		PlayInEditor();
	}
	else
	{
		PlayOnConsole(PlayWorldDestination, bUseMobilePreviewForPlayWorld, bStartMovieCapture);
	}

	// Clear the spectate flag
	bStartInSpectatorMode = FALSE;
}

// @todo DB: appSaveAllWorlds declared this way because they're implemented in UnrealEd -- fix!

/**
 * Save all packages containing UWorld objects with the option to override their path and also
 * apply a prefix.
 *
 * @param	OverridePath	Path override, can be NULL
 * @param	Prefix			Optional prefix for base filename, can be NULL
 * @param	bIncludeGWorld	If TRUE, save GWorld along with other worlds.
 * @param	bCheckDirty		If TRUE, don't save level packages that aren't dirty.
 * @param	bSaveOnlyWritable	If true, only save files that are writable on disk.
 * @param	bPIESaving		Should be set to TRUE if saving for PIE; passed to UWorld::SaveWorld.
 * @return					TRUE if at least one level was saved.
 */
extern UBOOL appSaveAllWorlds(const TCHAR* OverridePath, const TCHAR* Prefix, UBOOL bIncludeGWorld, UBOOL bCheckDirty, UBOOL bSaveOnlyWritable, UBOOL bPIESaving, FEditorFileUtils::EGarbageCollectionOption Option);

/**
 * Save all packages corresponding to the specified UWorlds, with the option to override their path and also
 * apply a prefix.
 *
 * @param	WorldsArray		The set of UWorlds to save.
 * @param	OverridePath	Path override, can be NULL
 * @param	Prefix			Optional prefix for base filename, can be NULL
 * @param	bIncludeGWorld	If TRUE, save GWorld along with other worlds.
 * @param	bCheckDirty		If TRUE, don't save level packages that aren't dirty.
 * @param	bPIESaving		Should be set to TRUE if saving for PIE; passed to UWorld::SaveWorld.
 * @return					TRUE if at least one level was saved.
 */
extern UBOOL appSaveWorlds(const TArray<UWorld*>& WorldsArray, const TCHAR* OverridePath, const TCHAR* Prefix, UBOOL bIncludeGWorld, UBOOL bCheckDirty, UBOOL bPIESaving);

/**
 * Assembles a list of worlds whose PIE packages need resaving.
 *
 * @param	FilenamePrefix				The PIE filename prefix.
 * @param	OutWorldsNeedingPIESave		[out] The list worlds that need to be saved for PIE.
 */
void UEditorEngine::GetWorldsNeedingPIESave(const TCHAR* FilenamePrefix, TArray<UWorld*>& OutWorldsNeedingPIESave) const
{
	OutWorldsNeedingPIESave.Empty();

	// Check to see if the user has asked for only currently-visible levels to be loaded in PIE.  If so
	// we'll also only bother saving out levels that we'll need to load!
	const UBOOL bOnlyEditorVisible = OnlyLoadEditorVisibleLevelsInPIE();

	// Clean up any old worlds.
	UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

	// Get the set of all reference worlds.
	TArray<UWorld*> Worlds;
	FLevelUtils::GetWorlds( Worlds, TRUE, bOnlyEditorVisible );

	// Assemble a list of worlds that need to be PIE-saved.
	for ( INT WorldIndex = 0 ; WorldIndex < Worlds.Num() ; ++WorldIndex )
	{
		UWorld*		World = Worlds(WorldIndex);
		UPackage*	Package = Cast<UPackage>( World->GetOuter() );

		if ( Package )
		{
			UBOOL bNeedsPIESave = FALSE;

			// Has the package been made dirty since the last time we generated a PIE level for it?
			if ( Package->IsDirtyForPIE() )
			{
				// A PIE save is needed if the level is dirty.
				bNeedsPIESave = TRUE;
			}
			else
			{
				FString PackageName = Package->GetName();

				// Does a file already exist for this world package?
				FFilename ExistingFilename;
				const UBOOL bPackageExists = GPackageFileCache->FindPackageFile( *PackageName, NULL, ExistingFilename );

				if ( !bPackageExists )
				{
					// The world hasn't been saved before, and so it needs a PIE save.
					bNeedsPIESave = TRUE;
				}
				else
				{
					// Build a PIE filename from the existing filename.
					const FString	PIEFilename = MakePIEFileName( FilenamePrefix, ExistingFilename );

					const DOUBLE	PIEPackageAgeInSeconds = GFileManager->GetFileAgeSeconds(*PIEFilename);
					if ( PIEPackageAgeInSeconds < 0.0 )
					{
						// The world hasn't been PIE-saved before.
						bNeedsPIESave = TRUE;
					}
					else
					{
						// Compare the PIE package age against the existing package age.  This is really to catch the case
						// where the original map file has been modified externally and we need to refresh our PIE file.
						const DOUBLE ExisingPackageAgeInSeconds	= GFileManager->GetFileAgeSeconds(*ExistingFilename);
						if ( ExisingPackageAgeInSeconds < PIEPackageAgeInSeconds )
						{
							// The level has been saved since the last PIE session.
							bNeedsPIESave = TRUE;
						}
					}
					
				}
			}

			if ( bNeedsPIESave )
			{
				OutWorldsNeedingPIESave.AddItem( World );
			}
		}
	}
}

/**
 * Saves play in editor levels and also fixes up references in AWorldInfo to other levels.
 *
 * @param	Prefix				Prefix used to save files to disk.
 * @param	bPlayOnConsole		For PIE we only save the map, for POC (console), we save all packages so they can be baked, etc
 *
 * @return	False if the save failed and the user wants to abort what they were doing
 */
UBOOL UEditorEngine::SavePlayWorldPackages( const TCHAR* Prefix, UBOOL bPlayOnConsole )
{
	// if we are going to an offline destination (say Xenon), then we should save all packages so the latest content
	// is copied over to the destination
	if (bPlayOnConsole)
	{
		// if this returns false, it means we should stop what we're doing and return to the editor
		UBOOL bPromptUserToSave = TRUE;
		UBOOL bSaveMapPackages = FALSE;
		UBOOL bSaveContentPackages = TRUE;
		if (!FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages))
		{
			return FALSE;
		}
	}

	// Update cull distance volumes before saving.
	GWorld->UpdateCullDistanceVolumes();

	// generate the proper path to put this into the Autosave directory
	FString	FullAutoSaveDir = FString(appBaseDir()) * AutoSaveDir;
	GFileManager->MakeDirectory(*FullAutoSaveDir, 1);

	UBOOL bSaved = TRUE;

	// save the world and levels out to a temporary package (kind of like autosave) and keep track of package names
	if (bPlayOnConsole)
	{
		const UBOOL bSaveGWorld = TRUE;
		const UBOOL bCheckDirty = FALSE;
		const UBOOL bSaveOnlyWritable = FALSE;
		const UBOOL bPIESaving = TRUE;
		bSaved = appSaveAllWorlds(*FullAutoSaveDir, Prefix, bSaveGWorld, bCheckDirty, bSaveOnlyWritable, bPIESaving, FEditorFileUtils::GCO_CollectGarbage );
	}
	else
	{
		TArray<UWorld*> WorldsNeedingPIESave;
		GetWorldsNeedingPIESave( Prefix, WorldsNeedingPIESave );

		debugf(NAME_Log, LINE_TERMINATOR TEXT("==== Worlds needing PIE Save:") );
		for ( INT i = 0 ; i < WorldsNeedingPIESave.Num() ; ++i )
		{
			UWorld*			World = WorldsNeedingPIESave(i);
			UPackage*		Package = Cast<UPackage>( World->GetOuter() );
			FString			PackageName = Package->GetName();
			debugf(NAME_Log, TEXT("%s"), *PackageName);
		}
		debugf(NAME_Log, TEXT("==== %i total"), WorldsNeedingPIESave.Num());

		if (WorldsNeedingPIESave.Num() > 0)
		{
			bSaved = appSaveWorlds( WorldsNeedingPIESave, *FullAutoSaveDir, Prefix, TRUE, FALSE, TRUE );
		}

		// Clear dirty flags for PIE-saved worlds.
		for ( INT WorldIndex = 0 ; WorldIndex < WorldsNeedingPIESave.Num() ; ++WorldIndex )
		{
			UWorld*		World = WorldsNeedingPIESave( WorldIndex );
			UPackage*	Package = Cast<UPackage>( World->GetOuter() );
			if ( Package )
			{
				// We'll clear the "dirty for PIE" bit on this package, since it's been saved out as a PIE map!
				// It is no longer necessary to restore the dirty flag because it is not changed when saved for a test run
				Package->ClearDirtyForPIEFlag();
			}
		}

		// remove any PIE ImportGuids entries in the package for all levels
		// Note: No need to perform a GC before getting the world since 
		// GetWorldsNeedingPIESave() performs one!
		TArray<UWorld*> Worlds;
		FLevelUtils::GetWorlds( Worlds, TRUE );
		for(INT WorldIndex=0; WorldIndex < Worlds.Num(); WorldIndex++)
		{
			UWorld*		World = Worlds( WorldIndex );
			UPackage*	Package = Cast<UPackage>( World->GetOuter() );

			for (INT ImportGuidIndex = 0; ImportGuidIndex < Package->ImportGuids.Num(); ImportGuidIndex++)
			{
				FLevelGuids& LevelGuids = Package->ImportGuids(ImportGuidIndex);
				FString LevelName = LevelGuids.LevelName.ToString();
				if (LevelName.StartsWith(Prefix))
				{
					Package->ImportGuids.Remove(ImportGuidIndex);
					ImportGuidIndex--;
				}
			}
		}
	}

	return bSaved;
}

/**
 * Builds a URL for game spawned by the editor (not including map name!). Has stuff like the teleporter, spectating, etc.
 * @param	MapName			The name of the map to put into the URL
 * @param	bSpecatorMode	If true, the player starts in spectator mode
 *
 * @return	The URL for the game
 */
FString UEditorEngine::BuildPlayWorldURL(const TCHAR* MapName, UBOOL bSpectatorMode)
{
	// the URL we are building up
	FString URL(MapName);

	if (bHasPlayWorldPlacement)
	{
		// tell the player to start in at the teleporter
		URL += TEXT("#PlayWorldStart");
	}

	// If we hold down control, start in spectating mode
	if (bSpectatorMode)
	{
		// Start in spectator mode
		URL += TEXT("?SpectatorOnly=1");
	}

	// Add any game-specific options set in the .ini file
	URL += InEditorGameURLOptions;

	return URL;
}


/**
 * Spawns a teleporter in the given world
 * @param	World	The World to spawn in (for PIE this may not be GWorld)
 *
 * @return	The spawned teleporter actor
 */
UBOOL UEditorEngine::SpawnPlayFromHereTeleporter(UWorld* World, ATeleporter*& PlayerStart)
{
	// null it out in case we don't need to spawn one, and the caller relies on us setting it
	PlayerStart = NULL;

	// are we doing Play From here?
	if (bHasPlayWorldPlacement)
	{
		UBOOL bPathsWereBuilt = World->GetWorldInfo()->bPathsRebuilt;
		// spawn the Teleporter in the given world
		PlayerStart = Cast<ATeleporter>(World->SpawnActor(ATeleporter::StaticClass(), NAME_None, PlayWorldLocation, PlayWorldRotation));
		// reset bPathsRebuilt as it might get unset due to spawning of teleporter
		World->GetWorldInfo()->bPathsRebuilt = bPathsWereBuilt;
		// make sure we were able to spawn the teleporter there
		if(!PlayerStart)
		{
			appMsgf(AMT_OK, *LocalizeUnrealEd("Prompt_22"));
			return false;
		}
		// add the teleporter to the nav list
		World->PersistentLevel->AddToNavList(PlayerStart);
		// name the teleporter
		PlayerStart->Tag = TEXT("PlayWorldStart");
	}
	// true means we didn't need to spawn, or we succeeded
	return true;
}

void UEditorEngine::PlayInEditor()
{
	DOUBLE PIEStartTime = appSeconds();

	// Prompt the user that Matinee must be closed before PIE can occur.
	if( GEditorModeTools().IsModeActive(EM_InterpEdit) )
	{
		const UBOOL bContinuePIE = appMsgf( AMT_YesNo, *LocalizeUnrealEd(TEXT("PIENeedsToCloseMatineeQ")) );
		if ( !bContinuePIE )
		{
			return;
		}
		GEditorModeTools().DeactivateMode( EM_InterpEdit );
	}

	const FScopedBusyCursor BusyCursor;

	// pause any object propagation
	GObjectPropagator->Pause();

	// If there's level already being played, close it.
	if(PlayWorld)
	{
		// immediately end the playworld
		EndPlayMap();
	}

	// Flush all audio sources from the editor world
	if( Client && Client->GetAudioDevice() )
	{
		Client->GetAudioDevice()->Flush( GWorld->Scene );
	}

	// Check if the scene has any external references before launching PIE
	if( PackageUsingExternalObjects(GWorld->PersistentLevel) && !appMsgf(AMT_YesNo, *LocalizeUnrealEd("Warning_UsingExternalPackage")) )
	{
		return; 
	}

	// Set whether or not to use "instant PIE" mode
	GUseFastPIE = ParseParam( appCmdLine(), TEXT("USEFASTPIE") ); // (GetAsyncKeyState(VK_SHIFT) & 0x8000);

	// remember old GWorld
	UWorld* OldGWorld = GWorld;
	FString Prefix = PLAYWORLD_PACKAGE_PREFIX;
	FString PlayWorldMapName = Prefix + *GWorld->GetOutermost()->GetName();

	// Kismet realtime debugging
	{
		// Clear out the engine breakpoint queue and hide existing Kismet windows
		if(GKismetRealtimeDebugging && OldGWorld->GetGameSequence())
		{
			UBOOL bIsKismetWindowOpen = GApp->KismetWindows.Num() > 0;
			GEditor->KismetDebuggerBreakpointQueue.Empty();
			WxKismet::SetVisibilityOnAllKismetWindows(FALSE);
			if (bIsKismetWindowOpen)
			{
				WxKismet::OpenKismetDebugger(OldGWorld->GetGameSequence(), NULL, NULL, FALSE);
			}
		}

		// Disable the realtime Kismet debugging button on the main editor toolbar
		if (GApp->EditorFrame && GApp->EditorFrame->GetToolBar())
		{
			GApp->EditorFrame->GetToolBar()->EnableTool(IDM_KISMET_REALTIMEDEBUGGING, false);
		}
	}

	if( GUseFastPIE )
	{
		// copy the world
		UPackage* PlayWorldPackage = CastChecked<UPackage>(CreatePackage(NULL,*PlayWorldMapName));
		PlayWorldPackage->PackageFlags |= PKG_PlayInEditor;

		GWorld = CastChecked<UWorld>(StaticDuplicateObject(OldGWorld,OldGWorld,PlayWorldPackage,TEXT("TheWorld")));
		GWorld->Init();

		debugf(TEXT("PIE: Copying PIE world level from %s to %s"),
			*OldGWorld->GetPathName(),
			*GWorld->GetPathName());

		// fixup level names with pie prefix
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		if( WorldInfo )
		{
			for ( INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++ )
			{
				ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
				if ( StreamingLevel )
				{
					// Make sure we don't use the level loaded from the main world 
					// since PIE levels are duplicated on demand when updating streaming
					StreamingLevel->LoadedLevel = NULL;

					// Apply prefix so this level references the soon to be saved other world packages.
					if( StreamingLevel->PackageName != NAME_None )
					{
						StreamingLevel->PackageName = FName( *(FString(PLAYWORLD_PACKAGE_PREFIX) + StreamingLevel->PackageName.ToString()) );
					}
				}
			}
		}
	}
	else
	{
		// save out the map to disk and get it's filename
		if (!SavePlayWorldPackages(PLAYWORLD_PACKAGE_PREFIX, FALSE))
		{
			// false from this function means to stop what we're doing and return to the editor
			return;
		}

		FString	FullAutoSaveDir = FString::Printf(TEXT("%s%s\\"), appBaseDir(), *this->AutoSaveDir);
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		FString StreamingLevels;
		if(WorldInfo->StreamingLevels.Num() > 0)
		{
			for(INT LevelIndex=0; LevelIndex < WorldInfo->StreamingLevels.Num(); ++LevelIndex)
			{
				ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
				if ( StreamingLevel )
				{
					// Apply prefix so this level references the soon to be saved other world packages.
					if( StreamingLevel->PackageName != NAME_None )
					{
						FString StreamingLevelName = FString::Printf(TEXT("%s%s.%s;"), *FullAutoSaveDir, *(Prefix + StreamingLevel->PackageName.ToString()), *FURL::DefaultMapExt);
						StreamingLevels += StreamingLevelName;
					}
				}
			}
		}


	    // start using the alternate cross level manager to not use any editor objects in the PIE world
	    FCrossLevelReferenceManager::SwitchToPIEManager();

		// clear out GWorld so that no loading code uses the Editor world while we are loading the PIE world
		GWorld = NULL;

		// Before loading the map, we need to set these flags to true so that postload will work properly
		GIsPlayInEditorWorld = TRUE;
		GIsGame = (!GIsEditor || GIsPlayInEditorWorld);

		// load up a new UWorld but keep the old one
		if(StreamingLevels.Len() == 0)
		{
			Exec(*FString::Printf( TEXT("MAP LOAD PLAYWORLD=1 FILE=\"%s%s.%s\""), *FullAutoSaveDir, *PlayWorldMapName, *FURL::DefaultMapExt));
		}
		else
		{
			// Have to call it explicitly because the internal temp buffer in Exec() is too small to hold the list of streaming maps and their fully qualified path's
			Map_Load(*FString::Printf( TEXT("PLAYWORLD=1 FILE=\"%s%s.%s\" STREAMLVL=\"%s\""), *FullAutoSaveDir, *PlayWorldMapName, *FURL::DefaultMapExt, *StreamingLevels), *GLog);
		}

		// After loading the map, reset these so that things continue as normal
		GIsPlayInEditorWorld = FALSE;
		GIsGame = (!GIsEditor || GIsPlayInEditorWorld);

	

	    // go back to standard manager, it will go to PIE manager again when needed
	    FCrossLevelReferenceManager::SwitchToStandardManager();
	}


	if (!GWorld)
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("Error_FailedCreateEditorPreviewWorld"));
		// The load failed, so restore GWorld.
		RestoreEditorWorld( OldGWorld );
		return;
	}

	// remember what we just loaded
	PlayWorld = GWorld;
	// make sure we can clean up this world!
	PlayWorld->ClearFlags(RF_Standalone);

	// If a start location is specified, spawn a temporary Teleporter actor at the start location and use it as the portal.
	ATeleporter* PlayerStart = NULL;
	if (SpawnPlayFromHereTeleporter(PlayWorld, PlayerStart) == FALSE)
	{
		// go back to using the real world as GWorld
		GWorld = OldGWorld;
		EndPlayMap();
		return;
	}

	SetPlayInEditorWorld( PlayWorld );

	// make a URL
	FURL URL;
	
	// If the user wants to start in spectator mode, do not use the custom play world for now
	if( UserEditedPlayWorldURL.Len() > 0 && !bStartInSpectatorMode )
	{
		// If the user edited the play world url. Verify that the map name is the same as the currently loaded map.
		URL = FURL(NULL, *UserEditedPlayWorldURL, TRAVEL_Absolute);
		if( URL.Map != PlayWorldMapName )
		{
			// Ensure the URL map name is the same as the generated play world map name.
			URL.Map = PlayWorldMapName;
		}
	}
	else
	{
		// The user did not edit the url, just build one from scratch.
		URL = FURL(NULL, *BuildPlayWorldURL( *PlayWorldMapName, bStartInSpectatorMode ), TRAVEL_Absolute);
	}

	PlayWorld->SetGameInfo(URL);

	// Initialize the viewport client.
	UGameViewportClient* ViewportClient = NULL;
	if(Client)
	{
		ViewportClient = ConstructObject<UGameViewportClient>(GameViewportClientClass,this);
		GameViewport = ViewportClient;
		GameViewport->bIsPlayInEditorViewport = TRUE;
		FString Error;
		if(!ViewportClient->eventInit(Error))
		{
			appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_CouldntSpawnPlayer"),*LocalizePropertyPath(*Error)));
			// go back to using the real world as GWorld
			RestoreEditorWorld( OldGWorld );
			EndPlayMap();
			return;
		}
	}

	// Disable the screensaver when PIE is running.
	GEngine->EnableScreenSaver( FALSE );

	// Stop the autosave timer when PIE is running.
	GEditor->PauseAutosaveTimer( TRUE );

	PlayWorld->BeginPlay(URL);


	// Stop all audio and remove references to temp level.
	if( Client && Client->GetAudioDevice() )
	{
		Client->GetAudioDevice()->ResetInterpolation();
	}

#if USING_REMOTECONTROL
	if ( RemoteControlExec )
	{
		// Notify RemoteControl that PIE is beginning
		RemoteControlExec->OnPlayInEditor( PlayWorld );
	}
#endif

	// Open initial Viewport.
	if( Client )
	{
		DisableRealtimeViewports();

		// Create a viewport for the local player.

		// Note that we are naming this with a hardcoded value. This is how the PreWindowMessage callback (used in FCallbackDeviceEditor)
		// knows that the viewport receiving input is the playworld viewport and to switch the GWorld to the PlayWorld.
		// If the name changes to something else, the code in "FCallbackDeviceEditor::Send(ECallbackType, FViewport*, UINT)"
		// will need to be updated as well.

		// Note again that we are no longer supporting fullscreen for in-editor playing, so we pass in 0 as the last parameter.
		// As well, we use the hardcoded name in the Viewport code to make sure we can't go fullscreen. If the name changes,
		// the code in FWindowsViewport::Resize() will have to change.

		UBOOL bCreatedViewport = FALSE;
		if( PlayInEditorViewportIndex != -1 )
		{
			// Try to create an embedded PIE viewport window
			if( CreateEmbeddedPIEViewport( ViewportClient, PlayInEditorViewportIndex ) )
			{
				bCreatedViewport = TRUE;
			}
		}

		if( !bCreatedViewport )
		{
#if _WIN64
			const FString PlatformBitsString( TEXT( "64" ) );
#else
			const FString PlatformBitsString( TEXT( "32" ) );
#endif

			// Only display the RHI version in non-shipping builds
			const FString GameName = GConfig->GetStr(TEXT("URL"), TEXT("GameName"), GEngineIni);
			FString RHIName = ShaderPlatformToText( GRHIShaderPlatform, TRUE, TRUE );

			const FString ViewportName = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "PlayInEditor_RHI_F" ), *GameName, *PlatformBitsString, *RHIName ) );
			
			// Create floating PIE viewport
			FViewportFrame* ViewportFrame = Client->CreateViewportFrame(
				ViewportClient,
				*ViewportName,
				GSystemSettings.ResX,
				GSystemSettings.ResY,
				FALSE
				);
			ViewportClient->SetViewportFrame( ViewportFrame );

			// If this application is in the foreground or has focus, then
			// Intentionally capture the mouse to prevent mis-clicks while PIE is launching from
			// minimizing the PIE window
			DWORD WindowProcessID = 0;
			HWND ForegroundWindowHandle = GetForegroundWindow();
			GetWindowThreadProcessId( ForegroundWindowHandle, &WindowProcessID );

			// Compare the foreground window's process ID vs. the editor's process ID
			if ( WindowProcessID == GetCurrentProcessId() )
			{
				// Don't lock the mouse to the window if we're using mobile input emulation
				if( !ViewportFrame->GetViewport()->IsFakeMobileTouchesEnabled() )
				{
					ViewportFrame->GetViewport()->LockMouseToWindow( TRUE );
					ViewportFrame->GetViewport()->CaptureMouse( TRUE );
				}
			}
		}
	}

	// Set the game viewport that was just created as a pie viewport.
	GameViewport->Viewport->SetPlayInEditorViewport();

	// Spawn the player's actor.
	for(FLocalPlayerIterator It(this);It;++It)
	{
		FString Error;
		if(!It->SpawnPlayActor(URL.String(1),Error))
		{
			appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_CouldntSpawnPlayer"),*LocalizePropertyPath(*Error)));
			// go back to using the real world as GWorld
			RestoreEditorWorld( OldGWorld );
			EndPlayMap();
			return;
		}

		// mimic the game booting up from scratch
		It->Actor->eventOnEngineInitialTick();
	}
	//
	// Check if we want to force PIE to start in exact place, suppressing kismet. It forces all levels to be streamed in, skips all level begin events and sets all matinees to be skipable.
	if (bForcePlayFromHere)
	{		
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		if (WorldInfo->StreamingLevels.Num() > 0)
		{
			for (INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++)
			{
				ULevelStreaming* CurrentLevelStreamingObject = WorldInfo->StreamingLevels(LevelIndex);
				if (CurrentLevelStreamingObject != NULL )
				{
					CurrentLevelStreamingObject->bShouldBeLoaded		= TRUE;
					CurrentLevelStreamingObject->bShouldBeVisible		= TRUE;
					CurrentLevelStreamingObject->bShouldBlockOnLoad		= TRUE;
					CurrentLevelStreamingObject->bHasLoadRequestPending	= TRUE;

				}
				GWorld->UpdateLevelStreaming();
			}

			GWorld->UpdateLevelStreaming();
		}
		USequence* Seq = GWorld->GetGameSequence(GWorld->CurrentLevel);
		if (Seq)
		{
			USequence* RootSeq = Seq->GetRootSequence();
			if (RootSeq)
			{
				// Remove all distracting sequences
				for( INT i = 0 ; i < RootSeq->SequenceObjects.Num() ; ++i )
				{
					USequenceObject* SeqObj = RootSeq->SequenceObjects(i);
					USeqEvent_LevelLoaded* LevelBeginingAndLoaded = Cast<USeqEvent_LevelLoaded>(SeqObj);
					if( LevelBeginingAndLoaded )
					{
						RootSeq->RemoveObject(LevelBeginingAndLoaded);
					}
				}
			}
			
			// make all matinees skipable
			for (INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++)
			{
				ULevelStreaming* CurrentLevelStreamingObject = WorldInfo->StreamingLevels(LevelIndex);
				
				if (CurrentLevelStreamingObject && CurrentLevelStreamingObject->LoadedLevel)
				{
					ULevel * SeqLevel = CurrentLevelStreamingObject->LoadedLevel;
					for( INT i = 0 ; i < SeqLevel->GameSequences.Num() ; ++i )
					{
						USequence * Seq  =  SeqLevel->GameSequences(i);
						if (Seq)
						{
							for( INT j = 0 ; j < Seq->SequenceObjects.Num() ; ++j )
							{
								USequenceObject* SeqObj = Seq->SequenceObjects(i);
								USeqAct_Interp* Matinee = Cast<USeqAct_Interp>(SeqObj);
								if (Matinee)
									Matinee->bIsSkippable = TRUE;
							}
						}
					}
				}
			}
		}
		bForcePlayFromHere =FALSE;
	}
	// go back to using the real world as GWorld
	RestoreEditorWorld( OldGWorld );

	// unpause any object propagation
	GObjectPropagator->Unpause();

	debugf(NAME_Log, TEXT("PIE: play in editor start time for %s %.3f"), *PlayWorldMapName, appSeconds() - PIEStartTime );

	// Track time spent loading map.
	GTaskPerfTracker->AddTask( TEXT("PIE startup"), *GWorld->GetOutermost()->GetName(), appSeconds() - PIEStartTime );
}


void UEditorEngine::PlayOnConsole(INT ConsoleIndex, UBOOL bUseMobilePreview, UBOOL bStartMovieCapture)
{
	// check if PersistentLevel has any external references
	if( PackageUsingExternalObjects(GWorld->PersistentLevel) && !appMsgf(AMT_YesNo, *LocalizeUnrealEd("Warning_UsingExternalPackage")) )
	{
		return;
	}

	// pause any object propagation
	GObjectPropagator->Pause();

	// End any current play on console map
	EndPlayOnConsole();

	// get the platform that we want to play on
	FConsoleSupport* Console = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(ConsoleIndex);

	// allow for a teleporter to make the player appear at the "Play Form Here" location
	ATeleporter* PlayerStart = NULL;
	// if it failed abort early
	if (SpawnPlayFromHereTeleporter(GWorld, PlayerStart) == FALSE)
	{
		return;
	}
	else if (!PlayerStart && bStartMovieCapture)
	{
		// Make sure we have a spawn point for the player
		UBOOL bValidSpawn = FALSE;
		for (FActorIterator It; It; ++It)
		{
			if (It->IsA(APlayerStart::StaticClass()))
			{
				bValidSpawn = TRUE;
				break;
			}
		}
		if ( !bValidSpawn )
		{
			appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_CouldntSpawnPlayer"),*LocalizeProperty(TEXT("GameMessage"),TEXT("FailedPlaceMessage"),TEXT("Engine"))));
			return;
		}
	}

	// make a per-platform name for the map
	FString ConsoleName = FString(Console->GetPlatformName());
	// we do this to keep the PlayOnConsole names all the same size.  Everywhere else uses Xenon (too big to change at this time)
	if( ConsoleName == CONSOLESUPPORT_NAME_360 )
	{
		ConsoleName = FString( CONSOLESUPPORT_NAME_360_SHORT );
	}
	else if( ConsoleName == CONSOLESUPPORT_NAME_IPHONE )
	{
		ConsoleName = TEXT( "IOS" );
	}
	FString Prefix	= FString(PLAYWORLD_CONSOLE_BASE_PACKAGE_PREFIX) + ConsoleName;
	FString MapName	= Prefix + *GWorld->GetOutermost()->GetName();

	// save out all open packages to be synced to xenon
	if (!SavePlayWorldPackages(*Prefix, TRUE))
	{
		// false from this function means to stop what we're doing and return to the editor
		// remove the teleporter from the world
		if (PlayerStart)
		{
			GWorld->DestroyActor(PlayerStart);
		}
		return;
	}

	// remove the teleporter from the world
	if (PlayerStart)
	{
		GWorld->DestroyActor(PlayerStart);
	}

	// let the platform do what it wants to run the map
	TCHAR OutputConsoleCommand[1024] = TEXT("\0");
	FString	FullAutoSaveDir = FString::Printf(TEXT("%s%s\\"), appBaseDir(), *this->AutoSaveDir);
//	FString LevelNames = FString::Printf(TEXT("\"%s%s.%s\""), *FullAutoSaveDir, *MapName, *FURL::DefaultMapExt);
	// There is no reason to use the full path to the level...
	FString LevelNames = FString::Printf(TEXT("\"%s\\%s.%s\""), *AutoSaveDir, *MapName, *FURL::DefaultMapExt);

	if ( ConsoleName != CONSOLESUPPORT_NAME_PC )
	{
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		for(INT LevelIndex=0; LevelIndex < WorldInfo->StreamingLevels.Num(); ++LevelIndex)
		{
			ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
			if ( StreamingLevel )
			{
				// Apply prefix so this level references the soon to be saved other world packages.
				if( StreamingLevel->PackageName != NAME_None )
				{
					FString StreamingLevelName = FString::Printf(TEXT(" \"%s%s.%s\""), *FullAutoSaveDir, *(Prefix + StreamingLevel->PackageName.ToString()), *FURL::DefaultMapExt);
					LevelNames += StreamingLevelName;
				}
			}
		}
	}

	int NumTargets = Console->GetNumMenuItems();
	TARGETHANDLE *TargetList = new TARGETHANDLE[NumTargets];

	Console->GetMenuSelectedTargets(TargetList, NumTargets);

	if(NumTargets > 0 || ConsoleName == CONSOLESUPPORT_NAME_PC)
	{
		FString PlayURL;
		// If the user wants to start in spectator mode, do not use the custom play world for now
		if( UserEditedPlayWorldURL.Len() > 0 && !bStartInSpectatorMode )
		{
			// If the user edited the play world url. Verify that the map name is the same as the currently loaded map.
			FURL UserURL( NULL, *UserEditedPlayWorldURL, TRAVEL_Absolute );
			if( UserURL.Map != MapName)
			{
				// Ensure the url map name is the same as the generated map name
				UserURL.Map = MapName;
			}
			PlayURL = UserURL.String();
		}
		else
		{
			// The user did not edit the play world URL, build one from scratch.
			PlayURL = BuildPlayWorldURL(*MapName, bStartInSpectatorMode);
		}
		
		// disable secure connections for object propagation
		if( ConsoleName == CONSOLESUPPORT_NAME_360_SHORT )
		{
			PlayURL += TEXT(" -DEVCON");
		}
		else if ( ConsoleName == CONSOLESUPPORT_NAME_PC )
		{
			// Make sure that the PlayURL switches are copied to the LevelNames string as the PlayURL string is ignored on PC, start by extracting the map name
			const INT iMap = PlayURL.InStr( TEXT("?") );
			if ( iMap != INDEX_NONE)
			{
				// Verify that the map name is the same as the level name, just to be safe, NOTE: we can only do this on PC because LevelNames doesn't include streaming levels (above)
				const FString sMap = PlayURL.Left( iMap );
				if ( LevelNames.InStr( sMap, FALSE, TRUE ) != INDEX_NONE )
				{
					// Definitely the right level, copy the URL params across, they have to be encapsulated by the quotes, if present, too!
					const FString sURL = PlayURL.RightChop( iMap );
					FString sLeft, sRight;
					if ( LevelNames.Split( TEXT( "\"" ), &sLeft, &sRight, TRUE ) )
					{
						LevelNames = sLeft + sURL + TEXT( "\"" ) + sRight;
					}
					else
					{
						LevelNames += sURL;	// In case the above code should change, support no quotes too!
					}
				}
			}

			// this parameter tells UGameEngine to add the auto-save dir to the paths array and repopulate the package file cache
			// this is needed in order to support streaming levels as the streaming level packages will be loaded only when needed (thus
			// their package names need to be findable by the package file caching system)
			// (we add to LevelNames because the URL is ignored by WindowsTools)
			LevelNames += TEXT(" -PIEVIACONSOLE");

			// Disable splash screen
			LevelNames += TEXT( " -NoSplash" );

			// If we're running in mobile preview mode, then append the argument for that
			if( bUseMobilePreview )
			{
				LevelNames += TEXT( " -simmobile" );
				LevelNames += TEXT( " -WINDOWED" );

				//set res options
				LevelNames += FString::Printf(TEXT(" ResX=%d"), GEditor->PlayInEditorWidth);
				LevelNames += FString::Printf(TEXT(" ResY=%d"), GEditor->PlayInEditorHeight);

				//features
				if (GEditor->BuildPlayDevice != BPD_DEFAULT)
				{
					LevelNames += FString::Printf(TEXT(" -SystemSettings=%s"), *GEditor->GetMobileDeviceSystemSettingsSection());
				}

				//features
				if (GEditor->bMobilePreviewPortrait)
				{
					LevelNames += TEXT(" -Portrait");
				}
			}

			//set render mode
			if (GForcedRenderMode != RENDER_MODE_NONE)
			{
				if (GForcedRenderMode == RENDER_MODE_DX11)
				{
					LevelNames += FString::Printf(TEXT(" -d3d11"));
				}
				else
				{
					LevelNames += FString::Printf(TEXT(" -d3d9"));
				}
			}

			// if we want to start movie capturing right away, then append the argument for that
			if (bStartMovieCapture)
			{
				//disable movies
				LevelNames += FString::Printf(TEXT(" -nomovie"), GEditor->MatineeCaptureResolutionX);

				//set res options
				LevelNames += FString::Printf(TEXT(" -ResX=%d"), GEditor->MatineeCaptureResolutionX);
				LevelNames += FString::Printf(TEXT(" -ResY=%d"), GEditor->MatineeCaptureResolutionY);
				
				//set fps
				LevelNames += FString::Printf(TEXT(" -BENCHMARK -FPS=%d"), GEditor->MatineeCaptureFPS);

				if (GEditor->MatineeCaptureType == 1)
				{
					LevelNames += FString::Printf(TEXT(" -MATINEESSCAPTURE=%s"), *GEngine->MatineeCaptureName);//*GEditor->MatineeNameForRecording);
				}
				else
				{
					LevelNames += FString::Printf(TEXT(" -MATINEEAVICAPTURE=%s"), *GEngine->MatineeCaptureName);//*GEditor->MatineeNameForRecording);
				}
				
				LevelNames += FString::Printf(TEXT(" -MATINEEPACKAGE=%s"), *GEngine->MatineePackageCaptureName);//*GEditor->MatineePackageNameForRecording);
				LevelNames += FString::Printf(TEXT(" -VISIBLEPACKAGES=%s"), *GEngine->VisibleLevelsForMatineeCapture);

				if (GEditor->bCompressMatineeCapture == 1)
				{
					LevelNames += TEXT(" -CompressCapture");
				}
			}
		}

#if UDK
		// Hack to force reversion to 32 bit version for UDK
		if( ConsoleName == CONSOLESUPPORT_NAME_PC )
		{
			NumTargets = -1;
		}
#endif
		Console->RunGame(TargetList, NumTargets, *LevelNames, *PlayURL, OutputConsoleCommand, sizeof(OutputConsoleCommand));

		// if the RunGame command needs to exec a command, do it now
		if (OutputConsoleCommand[0] != 0)
		{
			FString Temp(OutputConsoleCommand);
			GEngine->Exec(*Temp, *GLog);
		}
	}

	// Set the active console being used
	CurrentPlayWorldDestination = ConsoleIndex;

	delete [] TargetList;
	TargetList = NULL;

	// resume object propagation
	GObjectPropagator->Unpause();
}



/**
 * Creates a fully qualified PIE package file name, given an original package file name
 */
FString UEditorEngine::MakePIEFileName( const TCHAR* FilenamePrefix, const FFilename& PackageFileName ) const
{
	// Build a PIE filename from the existing filename.
	const FFilename	CleanFileName = PackageFileName.GetCleanFilename();
	FString	FullAutoSaveDir = FString(appBaseDir()) * AutoSaveDir;
	const FString	PIEFileName = FullAutoSaveDir + PATH_SEPARATOR + FString( FilenamePrefix ) + CleanFileName;

	return PIEFileName;
}



/**
 * Checks to see if we need to delete any PIE files from disk, usually because we're closing a map (or shutting
 * down) and the in memory map data has been modified since PIE packages were generated last
 */
void UEditorEngine::PurgePIEDataForDirtyPackagesIfNeeded()
{
	// We don't bother doing this if we're currently still in a PIE world
	if( !GIsPlayInEditorWorld )
	{
		// Iterate over all packages
		for( TObjectIterator< UPackage > CurPackageIter; CurPackageIter != NULL ; ++CurPackageIter )
		{
			UPackage* CurPackage = *CurPackageIter;
			if( CurPackage != NULL )
			{
				// Only look at non-transient root packages.
				if( CurPackage->GetOuter() == NULL && CurPackage != UObject::GetTransientPackage() )
				{
					// Is this a map package, and if so, is it unsaved?  If so, then we need to check for PIE data that may
					// be more recent
					if( CurPackage->IsDirty() && CurPackage->ContainsMap() )
					{
						UBOOL bShouldDeletePIEFile = FALSE;

						// Does a file already exist for this world package?
						FFilename ExistingFilename;
						const UBOOL bPackageExists = GPackageFileCache->FindPackageFile( *CurPackage->GetName(), NULL, ExistingFilename );
						if( !bPackageExists )
						{
							// File didn't exist on disk, so we'll need to make up a file name for the PIE data
							ExistingFilename = CurPackage->GetName() + TEXT(".") + FURL::DefaultMapExt;
						}

						// Build a PIE filename from the existing filename.
						const FString	PIEFilename = MakePIEFileName( PLAYWORLD_PACKAGE_PREFIX, ExistingFilename );

						if( bPackageExists )
						{
							// Make sure the PIE data is on disk
							const DOUBLE PIEPackageAgeInSeconds = GFileManager->GetFileAgeSeconds(*PIEFilename);
							if( PIEPackageAgeInSeconds >= 0.0 )
							{
								// Is the PIE data on disk newer than the saved map data?  If so, then we'll need to purge the
								// PIE data.
								const DOUBLE ExistingPackageAgeInSeconds	= GFileManager->GetFileAgeSeconds( *ExistingFilename );
								if( ExistingPackageAgeInSeconds > PIEPackageAgeInSeconds )
								{
									bShouldDeletePIEFile = TRUE;
								}
								else
								{
									// Data on disk is newer than the PIE data, so PIE data will be likely be regenerated on demand the next
									// time the user Plays In Editor
								}
							}
							else
							{
								// PIE file doesn't even exist on disk, no need to consider deleting it
							}
						}
						else
						{
							// No original map file exists on disk, so we definitely want to delete a PIE file if there is one!
							bShouldDeletePIEFile = TRUE;
						}


						// OK, do we even have a PIE file to delete on disk?
						if( GFileManager->GetFileAgeSeconds( *PIEFilename ) >= 0.0 )
						{
							// Make sure the async IO manager doesn't have a lock on this file.  It tends to keep file handles
							// open for extended periods of time.
							GIOManager->Flush();

							// Purge it, so that it will be forcibly regenerated next time the user tries to Play In Editor
							if( !GFileManager->Delete( *PIEFilename ) )
							{
								// Hrm, unable to delete the file.  We may still have it open for some reason.  Oh well.
							}
						}
						else
						{
							// Package isn't modified in memory, so no need to worry about it
						}
					}
					else
					{
						// Can't find the package
					}
				}
			}
		}
	}
}



/**
 * FCallbackEventDevice interface
 */
void UEditorEngine::Send( ECallbackEventType Event )
{
	if( Event == CALLBACK_PreEngineShutdown )
	{
		// The engine is shutting down, so we'll check to see if we should purge any PIE data
		PurgePIEDataForDirtyPackagesIfNeeded();
	}

	if( Event == CALLBACK_MapChange || Event == CALLBACK_WorldChange )
	{
		SetMobileRenderingEmulation( GEmulateMobileRendering, GWorld->GetWorldInfo()->bUseGammaCorrection );
	}
}




/**
 * FCallbackEventDevice interface
 */
void UEditorEngine::Send( ECallbackEventType Event, DWORD Param )
{
	if( Event == CALLBACK_MapChange )
	{
		if( PreviewMeshComp )
		{
			// Detach the preview mesh component when changing 
			// maps because it's attached to the current world. 
			PreviewMeshComp->ConditionalDetach();
		}

		// Okay, a map is being unloaded (or some other major change has happened), so we'll check to see if we
		// should purge any PIE data
		PurgePIEDataForDirtyPackagesIfNeeded();
	}
}

UBOOL UEditorEngine::PackageUsingExternalObjects( ULevel* LevelToCheck, UBOOL bAddForMapCheck )
{
	check(LevelToCheck);
	UBOOL bFoundExternal = FALSE;
	TArray<UObject*> ExternalObjects;
	if(PackageTools::CheckForReferencesToExternalPackages(NULL, NULL, LevelToCheck, &ExternalObjects ))
	{
		for(INT ObjectIndex = 0; ObjectIndex < ExternalObjects.Num(); ++ObjectIndex)
		{
			// If the object in question has external references and is not pending deletion, add it to the log and tell the user about it below
			UObject* ExternalObject = ExternalObjects(ObjectIndex);

			if(!ExternalObject->HasAnyFlags(RF_PendingKill))
			{
				bFoundExternal = TRUE;
				if( bAddForMapCheck ) 
				{
					GWarn->MapCheck_Add( MCTYPE_WARNING, ExternalObject, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_UsingExternalObject" ), *ExternalObject->GetFullName() ) ), TEXT( "UsingExternalObject" ) );
				}
			}
		}
	}
	return bFoundExternal;
}
