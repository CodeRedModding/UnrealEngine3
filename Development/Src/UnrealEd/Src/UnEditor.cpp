/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "Factories.h"
#include "BSPOps.h"

// needed for the RemotePropagator
#include "UnIpDrv.h"
#include "EnginePrefabClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineDecalClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSoundClasses.h"
#include "EngineFluidClasses.h"

#include "UnConsoleSupportContainer.h"
#include "UnAudioCompress.h"
#include "Database.h"
#include "CrossLevelReferences.h"
#include "../Debugger/UnDebuggerCore.h"

#include "SourceControl.h"
#include "LevelUtils.h"
#include "UnLinkedObjDrawUtils.h"

#include "Kismet.h"

#if WITH_MANAGED_CODE
	#include "ColorPickerShared.h"
	#include "FileSystemNotificationShared.h"
#endif

// For WAVEFORMATEXTENSIBLE
#pragma pack(push,8)
#include <mmreg.h>
#pragma pack(pop)

extern UBOOL GKismetRealtimeDebugging;

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

UEditorEngine*	GEditor;

static inline USelection*& PrivateGetSelectedActors()
{
	static USelection* SSelectedActors = NULL;
	return SSelectedActors;
};

static inline USelection*& PrivateGetSelectedObjects()
{
	static USelection* SSelectedObjects = NULL;
	return SSelectedObjects;
};

static void PrivateInitSelectedSets()
{
	PrivateGetSelectedActors() = new( UObject::GetTransientPackage(), TEXT("SelectedActors"), RF_Transactional ) USelection;
	PrivateGetSelectedActors()->AddToRoot();

	PrivateGetSelectedObjects() = new( UObject::GetTransientPackage(), TEXT("SelectedObjects"), RF_Transactional ) USelection;
	PrivateGetSelectedObjects()->AddToRoot();
}

static void PrivateDestroySelectedSets()
{
#if 0
	PrivateGetSelectedActors()->RemoveFromRoot();
	PrivateGetSelectedActors() = NULL;
	PrivateGetSelectedObjects()->RemoveFromRoot();
	PrivateGetSelectedObjects() = NULL;
#endif
}

/*-----------------------------------------------------------------------------
	UEditorEngine.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UEditorEngine);
IMPLEMENT_CLASS(ULightingChannelsObject);
IMPLEMENT_CLASS(UEditorUserSettings);

/**
 * Returns the number of currently selected actors.
 *
 */
UBOOL UEditorEngine::GetSelectedActorCount() const
{
	int NumSelectedActors = 0;
	for(FSelectionIterator It(GetSelectedActorIterator()); It; ++It)
	{
		++NumSelectedActors;
	}

	return NumSelectedActors;
}

/**
 * Returns the set of selected actors.
 */
USelection* UEditorEngine::GetSelectedActors() const
{
	return PrivateGetSelectedActors();
}

/**
 * Returns an FSelectionIterator that iterates over the set of selected actors.
 */
FSelectionIterator UEditorEngine::GetSelectedActorIterator() const
{
	return FSelectionIterator( *GetSelectedActors() );
};

/**
 * Returns the set of selected non-actor objects.
 */
USelection* UEditorEngine::GetSelectedObjects() const
{
	return PrivateGetSelectedObjects();
}

/**
 * Returns the appropriate selection set for the specified object class.
 */
USelection* UEditorEngine::GetSelectedSet( const UClass* Class ) const
{
	USelection* SelectedSet = GetSelectedActors();
	if ( Class->IsChildOf( AActor::StaticClass() ) )
	{
		return SelectedSet;
	}
	else
	{
		//make sure this actor isn't derived off of an interface class
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* TestActor = static_cast<AActor*>( *It );
			if (TestActor->GetClass()->ImplementsInterface(Class))
			{
				return SelectedSet;
			}
		}

		//no actor matched the interface class
		return GetSelectedObjects();
	}
}

/**
 * Create a hierarchical menu of sound classes
 */
INT UEditorEngine::RecursiveAddSoundClass( UAudioDevice* AudioDevice, wxMenu* Parent, USoundClass* SoundClass, FName ClassName, INT wxID )
{
	if( SoundClass->ChildClassNames.Num() > 0 )
	{
		// If we have children, create a new menu and add the children
		wxMenu*	SoundClassMenu = new wxMenu();
		SoundClass->MenuID = wxID++;
		Parent->Append( SoundClass->MenuID, *ClassName.ToString(), SoundClassMenu );

		for( INT Index = 0; Index < SoundClass->ChildClassNames.Num(); Index++ )
		{
			USoundClass* ChildSoundClass = AudioDevice->GetSoundClass( SoundClass->ChildClassNames( Index ) );
			if( ChildSoundClass )
			{
				wxID = RecursiveAddSoundClass( AudioDevice, SoundClassMenu, ChildSoundClass, ChildSoundClass->GetFName(), wxID );
			}
		}
	}
	else
	{
		// Leaf node - add the sound class
		SoundClass->MenuID = wxID++;
		Parent->Append( SoundClass->MenuID, *SoundClass->GetFName().ToString() );
	}

	return( wxID );
}

void UEditorEngine::CreateSoundClassMenu( wxMenu* Parent )
{
	UAudioDevice* AudioDevice = Client ? Client->GetAudioDevice() : NULL;
	if( AudioDevice )
	{
		USoundClass* Master = AudioDevice->GetSoundClass( ( FName )NAME_Master );
		if( Master )
		{
			RecursiveAddSoundClass( AudioDevice, Parent, Master, FName( TEXT( "Sound Classes: Master" ) ), IDMN_ObjectContext_SoundCue_SoundClasses_START );
		}
	}
}

/**
 * Create a hierarchical menu of sound classes
 */
INT UEditorEngine::RecursiveAddSoundClassForContentBrowser( UAudioDevice* AudioDevice, TArray<FObjectSupportedCommandType>& OutCommands, INT ParentIndex, USoundClass* SoundClass, FName ClassName, INT wxID )
{
	if( SoundClass->ChildClassNames.Num() > 0 )
	{
		// If we have children, create a new menu and add the children
		SoundClass->MenuID = wxID++;
		OutCommands.AddItem( FObjectSupportedCommandType( SoundClass->MenuID, *ClassName.ToString(), TRUE, ParentIndex ) );
		ParentIndex = OutCommands.Num() - 1;

		// Add the option again as its own child item so that it can be selected from the menu
		OutCommands.AddItem( FObjectSupportedCommandType( SoundClass->MenuID, *FString::Printf( TEXT("%s (%s)"), *SoundClass->GetFName().ToString(), *LocalizeUnrealEd("Parent") ), TRUE, ParentIndex ) );
		
		for( INT Index = 0; Index < SoundClass->ChildClassNames.Num(); Index++ )
		{
			USoundClass* ChildSoundClass = AudioDevice->GetSoundClass( SoundClass->ChildClassNames( Index ) );
			if( ChildSoundClass )
			{
				wxID = RecursiveAddSoundClassForContentBrowser( AudioDevice, OutCommands, ParentIndex, ChildSoundClass, ChildSoundClass->GetFName(), wxID );
			}
		}
	}
	else
	{
		// Leaf node - add the sound class
		SoundClass->MenuID = wxID++;
		OutCommands.AddItem( FObjectSupportedCommandType( SoundClass->MenuID, *SoundClass->GetFName().ToString(), TRUE, ParentIndex ) );
	}

	return( wxID );
}

/** Create SoundClass menu commands for each sound class
 * 
 * @param OutCommands	The returned list of commands generated
 * @param ParentIndex	OptionalParameter to set the entire command structure to a different parent
 */
void UEditorEngine::CreateSoundClassMenuForContentBrowser( TArray<FObjectSupportedCommandType>& OutCommands, INT ParentIndex )
{
	UAudioDevice* AudioDevice = Client ? Client->GetAudioDevice() : NULL;
	if( AudioDevice )
	{
		USoundClass* Master = AudioDevice->GetSoundClass( ( FName )NAME_Master );
		if( Master )
		{
			RecursiveAddSoundClassForContentBrowser( AudioDevice, OutCommands, ParentIndex, Master, FName( TEXT( "Sound Classes: Master" ) ), IDMN_ObjectContext_SoundCue_SoundClasses_START );
		}
	}
}

/*-----------------------------------------------------------------------------
	Init & Exit.
-----------------------------------------------------------------------------*/

//
// Construct the UEditorEngine class.
//
void UEditorEngine::StaticConstructor()
{
}

//
// Editor early startup.
//
void UEditorEngine::InitEditor()
{
	// Call base.
	UEngine::Init();

	// Create selection sets.
	if( !GIsUCCMake )
	{
		PrivateInitSelectedSets();
	}

	// Make sure properties match up.
	VERIFY_CLASS_OFFSET(AActor,Actor,Owner);

	// Allocate temporary model.
	TempModel = new UModel( NULL, 1 );
	ConversionTempModel = new UModel( NULL, 1 );

	// Settings.
	FBSPOps::GFastRebuild	= 0;
	Bootstrapping			= 0;

	// Setup up particle count clamping values...
	if (GEngine)
	{
		GEngine->MaxParticleSpriteCount = GEngine->MaxParticleVertexMemory / (4 * sizeof(FParticleSpriteVertex));
		GEngine->MaxParticleSubUVCount = GEngine->MaxParticleVertexMemory / (4 * sizeof(FParticleSpriteSubUVVertex));
	}
	else
	{
		warnf(NAME_Warning, TEXT("Failed to set GEngine particle counts!"));
	}

	if( GWorld && !GIsUCC )
	{
		// Start up the PhysX scene for Landscape collision.
		GWorld->InitWorldRBPhys();
	}

	if (GEmulateMobileRendering == TRUE)
	{
		FMobileEmulationMaterialManager::GetManager()->UpdateCachedMaterials(FALSE, FALSE);
	}
}

UBOOL UEditorEngine::ShouldDrawBrushWireframe( AActor* InActor )
{
	UBOOL bResult = TRUE;

	bResult = GEditorModeTools().ShouldDrawBrushWireframe( InActor );
	
	return bResult;
}

// Used for sorting ActorFactory classes.
IMPLEMENT_COMPARE_POINTER( UActorFactory, UnEditor,
{
	INT CompareResult = B->MenuPriority - A->MenuPriority;
	if ( CompareResult == 0 )
	{
		if ( A->GetClass() != UActorFactory::StaticClass() && B->IsA(A->GetClass()) )
		{
			CompareResult = 1;
		}
		else if ( B->GetClass() != UActorFactory::StaticClass() && A->IsA(B->GetClass()) )
		{
			CompareResult = -1;
		}
		else
		{
			CompareResult = appStricmp(*A->GetClass()->GetName(), *B->GetClass()->GetName());
		}
	}
	return CompareResult;
})

//
// Init the editor.
//
/**
 * Handle special cases for:
 *	- OnlineSubsystem* packages - OnslineSubsystem packages are platform-dependent, but need to 
 *	  compiled by the PC. They are also licensee-access (and UDK/runtime) dependent, so if there 
 *    are OS* packages specified in EditPackages but the source code doesn't exist, then we 
 *    remove it from the list of packages to be processed
 *	- IpDrv for WITH_UE3_NETWORKING==0
 *
 * @param PackageList Array of package names to cull missing OnlineSubsystems from
 * @param ScriptSourcePath Directory to look in for script files
 */
void StripUnusedPackagesFromList(TArray<FString>& PackageList, const FString& ScriptSourcePath)
{
	for (INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++)
	{
		if (PackageList(PackageIndex).StartsWith(TEXT("OnlineSubsystem")))
		{
#if WITH_UE3_NETWORKING
			// make sure there are .uc files in the directory
			const FString SourceFiles = ScriptSourcePath * PackageList(PackageIndex) * TEXT("Classes\\*.uc");
			TArray<FString> ClassesFiles;
			GFileManager->FindFiles(ClassesFiles, *SourceFiles, TRUE, FALSE);

			// if not, then we remove this OS package from the list
			if (ClassesFiles.Num() == 0)
#endif
			{
				PackageList.Remove(PackageIndex--);
			}
		}
#if !WITH_UE3_NETWORKING
		else if (PackageList(PackageIndex) == TEXT("IpDrv"))
		{
			PackageList.Remove(PackageIndex--);
		}
#endif

#if !WITH_SUBSTANCE_AIR
		if (PackageList(PackageIndex).StartsWith(TEXT("Substance")))
		{
			PackageList.Remove(PackageIndex--);
		}
#endif
	}
}


void UEditorEngine::Init()
{
	check(!HasAnyFlags(RF_ClassDefaultObject));

	// Register for 'packaged dirtied' events.  We'll need these for some PIE stuff.  We also need to listen
	// for the 'Map Change' event so we can purge PIE data when needed.
	GCallbackEvent->Register( CALLBACK_MapChange, this );
	GCallbackEvent->Register( CALLBACK_WorldChange, this );
	GCallbackEvent->Register( CALLBACK_PreEngineShutdown, this );

	InitializeMobileSettings();

	// Init editor.
	GEditor = this;
	InitEditor();

#if !WITH_UE3_NETWORKING
	FString DummyPath;
	StripUnusedPackagesFromList(EditPackages, DummyPath);
#endif

	//Init the class hierarchy
	EditorClassHierarchy = new FEditorClassHierarchy;
	EditorClassHierarchy->Init();

	// Init transactioning.
	Trans = CreateTrans();

	/* The force loading of packages is no longer needed in the editor.  The "make" commandlet will compile all script and generate a manifest
	 * that fully expresses all classes.  The editor will use this manifest rather than needing to load all specified scripts before they are needed.
	 */
	// MT->this breaks AIFactory being able to load content pawn classes (and probably other stuff) disabling until 
	//     proper fix is checked in 
	//if (!GIsEditor)
	{
	// Load classes for editing.
	BeginLoad();

	for( INT i=0; i<EditPackages.Num(); i++ )
	{
		if( !LoadPackage( NULL, *EditPackages(i), LOAD_NoWarn ) )
		{
			debugf( LocalizeSecure(LocalizeUnrealEd("Error_CantFindEditPackage"), *EditPackages(i)) );
		}
	}

	// get the list of extra mod packages from the .ini (for UDK)
	TArray<FString> ModEditPackages;
	GConfig->GetArray(TEXT("UnrealEd.EditorEngine"), TEXT("ModEditPackages"), ModEditPackages, GEngineIni);
	for( INT i=0; i<ModEditPackages.Num(); i++ )
	{
		if( !LoadPackage( NULL, *ModEditPackages(i), LOAD_NoWarn ) )
		{
			debugf( LocalizeSecure(LocalizeUnrealEd("Error_CantFindEditPackage"), *ModEditPackages(i)) );
		}
	}

	// Automatically load mod classes unless -nomodautoload is specified.
	if ( !ParseParam(appCmdLine(), TEXT("nomodautoload")) )
	{
		TArray<FString> ModScriptPackageNames;
		FConfigSection* ModSec = GConfig->GetSectionPrivate( TEXT("ModPackages"), 0, 1, GEditorIni );
		ModSec->MultiFind( FName(TEXT("ModPackages")), ModScriptPackageNames );

		for( INT PackageIndex=0; PackageIndex<ModScriptPackageNames.Num(); ++PackageIndex )
		{
			if( !LoadPackage( NULL, *ModScriptPackageNames(PackageIndex), LOAD_NoWarn ) )
			{
				debugf( LocalizeSecure(LocalizeUnrealEd("Error_CantFindEditPackage"), *ModScriptPackageNames(PackageIndex)) );
			}
		}
	}

	EndLoad( *LocalizeProgress(TEXT("EditorPackages"), TEXT("Core")) );
	}
	// Init the client.
	UClass* ClientClass = StaticLoadClass( UClient::StaticClass(), NULL, TEXT("engine-ini:UnrealEd.EditorEngine.Client"), NULL, LOAD_None, NULL );
	Client = (UClient*)StaticConstructObject( ClientClass );
	Client->Init( this );
	check(Client);

	// Objects.
	Results  = new( GetTransientPackage(), TEXT("Results") ) UTextBuffer(TEXT(""));

	// Create array of ActorFactory instances.
	TArray<INT> FactoryClassIndexArray;
	EditorClassHierarchy->GetFactoryClasses(FactoryClassIndexArray);
	for (INT i = 0; i < FactoryClassIndexArray.Num(); ++i)
	{
		INT ClassIndex = FactoryClassIndexArray(i);
		UClass* FactoryClass = EditorClassHierarchy->GetClass(ClassIndex);
		check(FactoryClass);
		if(!HiddenActorFactoryNames.ContainsItem(FactoryClass->GetFName()) )
		{
			UActorFactory* NewFactory = ConstructObject<UActorFactory>( FactoryClass );
			ActorFactories.AddItem(NewFactory);
		}
	}

	// Init fonts used for editor drawing
	FLinkedObjDrawUtils::InitFonts(this->EditorFont);

	// Sort by menu priority.
	Sort<USE_COMPARE_POINTER(UActorFactory,UnEditor)>( &ActorFactories(0), ActorFactories.Num() );

	// Purge garbage.
	Cleanse( FALSE, 0, TEXT("startup") );

	// Subsystem init messsage.
	debugf( NAME_Init, TEXT("Editor engine initialized") );

	// create the possible propagators
	InEditorPropagator = new FEdObjectPropagator;
#if WITH_UE3_NETWORKING
	RemotePropagator = new FRemotePropagator;
#endif	//#if WITH_UE3_NETWORKING

	// script debugging support in the editor
	if (ParseParam(appCmdLine(), TEXT("VADEBUG")))
	{
		if (!GDebugger)
		{
			debugf(TEXT("Attaching script debugger (Visual Studio interface)"));
			UDebuggerCore::InitializeDebugger();
		}
	}
	LastCameraAlignTarget = NULL;
};

/**
 * Constructs a default cube builder brush, this function MUST be called at the AFTER UEditorEngine::Init in order to guarantee builder brush and other required subsystems exist.
 */
void UEditorEngine::InitBuilderBrush()
{
	const UBOOL bOldDirtyState = GWorld->CurrentLevel->GetOutermost()->IsDirty();

	// For additive geometry mode, make the builder brush a small 256x256x256 cube so its visible.
	const INT CubeSize = 256;
	UCubeBuilder* CubeBuilder = ConstructObject<UCubeBuilder>( UCubeBuilder::StaticClass() );
	CubeBuilder->X = CubeSize;
	CubeBuilder->Y = CubeSize;
	CubeBuilder->Z = CubeSize;
	CubeBuilder->eventBuild();

	// Restore the level's dirty state, so that setting a builder brush won't mark the map as dirty.
	GWorld->CurrentLevel->MarkPackageDirty( bOldDirtyState );
}

void UEditorEngine::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// this needs to be already cleaned up
		check(PlayWorld == NULL);

		// Unregister events
		GCallbackEvent->Unregister( CALLBACK_MapChange, this );
		GCallbackEvent->Unregister( CALLBACK_PreEngineShutdown, this );


		// free the propagators
		delete InEditorPropagator;
		delete RemotePropagator;

		// GWorld == NULL if we're compiling script.
		if( !GIsUCCMake )
		{
			ClearComponents();
			if( GWorld != NULL )
			{
				GWorld->CleanupWorld();
			}
		}

		// Shut down transaction tracking system.
		if( Trans )
		{
			if( GUndo )
			{
				debugf( NAME_Warning, TEXT("Warning: A transaction is active") );
			}
			ResetTransaction( TEXT("shutdown") );
		}

		// Destroy selection sets.
		PrivateDestroySelectedSets();

		// Remove editor array from root.
		debugf( NAME_Exit, TEXT("Editor shut down") );
	}

	Super::FinishDestroy();
}
void UEditorEngine::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	if(!Ar.IsLoading() && !Ar.IsSaving())
	{
		// Serialize viewport clients.

		for(UINT ViewportIndex = 0;ViewportIndex < (UINT)ViewportClients.Num();ViewportIndex++)
			Ar << *ViewportClients(ViewportIndex);

		// Serialize ActorFactories
		Ar << ActorFactories;

		// Serialize components used in UnrealEd modes

		GEditorModeTools().Serialize( Ar );
	}
}

/*-----------------------------------------------------------------------------
	Tick.
-----------------------------------------------------------------------------*/

//
// Time passes...
//
void UEditorEngine::Tick( FLOAT DeltaSeconds )
{
	check( GWorld );
	check( GWorld != PlayWorld );

	// was there a reattach requested last frame?
	if (bHasPendingGlobalReattach)
	{
		// make sure outstanding deletion has completed before the reattach
		UObject::CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

		//Only reattach actors whose replacement primitive is a child of the global reattach list
		FGlobalComponentReattachContext Reattach(ActorsForGlobalReattach);
		ActorsForGlobalReattach.Empty();

		bHasPendingGlobalReattach = FALSE;
	}

	// Tick client code.
	if( Client )
	{
		Client->Tick(DeltaSeconds);
	}

	// Clean up the game viewports that have been closed.
	CleanupGameViewport();

	// If all viewports closed, close the current play level.
	if( GameViewport == NULL && PlayWorld )
	{
		EndPlayMap();
	}

	// Set the editor perspective viewport positions to match those of Play in Viewport if required
	if( PlayInEditorViewportIndex != -1 && GEditor->AccessUserSettings().bEnableViewportCameraToUpdateFromPIV  && GEngine->GamePlayers.Num() > 0 )
	{
		FVCD_Viewport* Viewport = &GApp->EditorFrame->ViewportConfigData->AccessViewport( PlayInEditorViewportIndex );
		if( Viewport->ViewportWindow->ViewportType == LVT_Perspective )
		{
			// If not in pie world, set it now
			UWorld* OldWorld = GWorld;
			if( !GIsPlayInEditorWorld )
			{
				SetPlayInEditorWorld( PlayWorld );
			}

			ULocalPlayer* Player = GEngine->GamePlayers( 0 );
			// Only do this if the player viewport was recently shutdown
			if( Player != NULL && Player->ViewportClient && !Player->ViewportClient->Viewport )
			{
				FVector ViewLocation;
				FRotator ViewRotation;
				Player->Actor->eventGetPlayerViewPoint( ViewLocation, ViewRotation );

				Viewport->ViewportWindow->ViewLocation = ViewLocation;
				Viewport->ViewportWindow->ViewRotation = ViewRotation;
			}

			// if in pie world, restore the editor world
			if( GIsPlayInEditorWorld )
			{
				RestoreEditorWorld(OldWorld);
			}
		}
	}

	// Potentially rebuilds the streaming data.
	ULevel::ConditionallyBuildStreamingData();

	// Update subsystems.
	{
		// This assumes that UObject::StaticTick only calls ProcessAsyncLoading.	
		UObject::StaticTick( DeltaSeconds );
	}

	// Look for realtime flags.
	UBOOL IsRealtime = FALSE;

	// True if a viewport has realtime audio	// If any realtime audio is enabled in the editor
	UBOOL bAudioIsRealtime = GEditor->AccessUserSettings().bEnableRealTimeAudio;;
	// True if there is any viewport overriding audio settings
	UBOOL bViewportIsOverridingAudio = FALSE;

	// Find realtime settings on all viewport clients
	for( INT i = 0; i < ViewportClients.Num(); i++ )
	{
		FEditorLevelViewportClient* ViewportClient = ViewportClients( i );

		if( ViewportClient->bAudioRealtimeOverride )
		{
			bAudioIsRealtime = ViewportClient->bWantAudioRealtime;
			bViewportIsOverridingAudio = TRUE;
		}

		if( ViewportClient->GetScene() == GWorld->Scene )
		{
			if( ViewportClient->IsRealtime() )
			{
				IsRealtime = TRUE;
			}

			if( bAudioIsRealtime && // Realtime audio in the editor is enabled
				!ViewportClient->IsRealtime() && // The viewport does not update in realtime (visual)
				ViewportClient->ViewportType == LVT_Perspective && // The viewport is perspective
				ViewportClient->Viewport->GetSizeX() > 0 && ViewportClient->Viewport->GetSizeY() > 0	// The viewport is a valid visible size
				&& GameViewport == NULL )		// if there is a game viewport, it gets to update the listener
			{
				// Update the audio listener position for the perspective viewport if the allows realtime and realtime audio is enabled.
				// Note we only do this when the viewport itself is not realtime for visual purposes.  If it is, the following calculations will be done in the draw code.

				// Create a scene view to use when calculating listener position.
				FSceneViewFamilyContext ViewFamily(
					ViewportClient->Viewport, 
					ViewportClient->GetScene(),
					ViewportClient->ShowFlags,
					GWorld->GetTimeSeconds(),
					GWorld->GetDeltaSeconds(),
					GWorld->GetRealTimeSeconds(),
					ViewportClient->IsRealtime()
					);
				const FSceneView& View = *ViewportClient->CalcSceneView( &ViewFamily );
				ViewportClient->UpdateAudioListener( View );
			}
		}
	}
	
	// Find out if the editor has focus. Audio should only play if the editor has focus.
	DWORD ForegroundProcess;
	GetWindowThreadProcessId(GetForegroundWindow(), &ForegroundProcess);
	const UBOOL bHasFocus = ForegroundProcess == GetCurrentProcessId();
	
	if( bHasFocus && !PlayWorld && !bViewportIsOverridingAudio )
	{
		// Adjust the global volume multiplier if the window has focus and there is no pie world or no viewport overriding audio.
		GVolumeMultiplier = GEditor->AccessUserSettings().EditorVolumeLevel;
	}
	else if( PlayWorld || bViewportIsOverridingAudio )
	{
		// If there is currently a pie world a viewport is overriding audio settings do not adjust the volume.
		GVolumeMultiplier = 1.0f;
	}


	// Tick level.
	GWorld->Tick( IsRealtime ? LEVELTICK_ViewportsOnly : LEVELTICK_TimeOnly, DeltaSeconds );

	// Perform editor level streaming previs if no PIE session is currently in progress.
	if( !PlayWorld )
	{
		FEditorLevelViewportClient* PerspectiveViewportClient = NULL;
		for ( INT i = 0 ; i < ViewportClients.Num() ; ++i )
		{
			FEditorLevelViewportClient* ViewportClient = ViewportClients(i);

			// Previs level streaming volumes in the Editor.
			if ( ViewportClient->bLevelStreamingVolumePrevis )
			{
				UBOOL bProcessViewer = FALSE;
				const FVector& ViewLocation = ViewportClient->ViewLocation;

				// Iterate over streaming levels and compute whether the ViewLocation is in their associated volumes.
				TMap<ALevelStreamingVolume*, UBOOL> VolumeMap;

				AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
				for( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
				{
					ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
					if( StreamingLevel )
					{
						// Assume the streaming level is invisible until we find otherwise.
						UBOOL bStreamingLevelShouldBeVisible = FALSE;

						// We're not going to change level visibility unless we encounter at least one
						// volume associated with the level.
						UBOOL bFoundValidVolume = FALSE;

						// For each streaming volume associated with this level . . .
						for ( INT i = 0 ; i < StreamingLevel->EditorStreamingVolumes.Num() ; ++i )
						{
							ALevelStreamingVolume* StreamingVolume = StreamingLevel->EditorStreamingVolumes(i);
							if ( StreamingVolume && !StreamingVolume->bDisabled )
							{
								bFoundValidVolume = TRUE;

								UBOOL bViewpointInVolume;
								UBOOL* bResult = VolumeMap.Find(StreamingVolume);
								if ( bResult )
								{
									// This volume has already been considered for another level.
									bViewpointInVolume = *bResult;
								}
								else
								{
									// Compute whether the viewpoint is inside the volume and cache the result.
									bViewpointInVolume = StreamingVolume->Encompasses( ViewLocation );							

									// If not inside, see if we should test distance to volume
									if(!bViewpointInVolume && StreamingVolume->bTestDistanceToVolume)
									{
										// we need a brush component..
										if(StreamingVolume->BrushComponent)
										{
											// Then use GJK to find closest point on volume to view
											FVector PointA, PointB;
											FVector Extent(0,0,0);
											StreamingVolume->BrushComponent->ClosestPointOnComponentToPoint( ViewLocation, Extent, PointA, PointB );

											// See how far closest point is
											FLOAT DistSqr = (PointA - PointB).SizeSquared();
											if(DistSqr < Square(StreamingVolume->TestVolumeDistance))
											{
												bViewpointInVolume = TRUE;
											}
										}
									}									
									
									VolumeMap.Set( StreamingVolume, bViewpointInVolume );
								}

								// Halt when we find a volume associated with the level that the viewpoint is in.
								if ( bViewpointInVolume )
								{
									bStreamingLevelShouldBeVisible = TRUE;
									break;
								}
							}
						}


						// Does this level have a grid volume associated with it?  If so then we'll process that now
						if( StreamingLevel->EditorGridVolume != NULL )
						{
							bFoundValidVolume = TRUE;

							ALevelGridVolume* LevelGridVolume = StreamingLevel->EditorGridVolume;

							// Grab the bounds of the volume's grid cell that's associated with the current level
							FLevelGridCellCoordinate GridCell;
							GridCell.X = StreamingLevel->GridPosition[ 0 ];
							GridCell.Y = StreamingLevel->GridPosition[ 1 ];
							GridCell.Z = StreamingLevel->GridPosition[ 2 ];

							const UBOOL bIsLevelAlreadyLoaded =
								( StreamingLevel->LoadedLevel != NULL ) &&
								StreamingLevel->bShouldBeVisibleInEditor;
							const UBOOL bShouldBeLoaded = LevelGridVolume->ShouldLevelBeLoaded( GridCell, ViewLocation, bIsLevelAlreadyLoaded );

							if( bShouldBeLoaded )
							{
								bStreamingLevelShouldBeVisible = TRUE;
							}
						}

						// Set the streaming level visibility status if we encountered at least one volume.
						if ( bFoundValidVolume && StreamingLevel->bShouldBeVisibleInEditor != bStreamingLevelShouldBeVisible )
						{
							StreamingLevel->bShouldBeVisibleInEditor = bStreamingLevelShouldBeVisible;
							bProcessViewer = TRUE;
						}
					}
				}

				// Call UpdateLevelStreaming if the visibility of any streaming levels was modified.
				if ( bProcessViewer )
				{
					GWorld->UpdateLevelStreaming();
					GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );
					GCallbackEvent->Send( CALLBACK_RefreshEditor_PrimitiveStatsBrowser );
				}
				break;
			}
		}
	}

	// kick off a "Play From Here" if we got one
	if (bIsPlayWorldQueued)
	{
		StartQueuedPlayMapRequest();
	}

	// if we have the side-by-side world for "Play From Here", tick it!
	if(PlayWorld)
	{
		// Use the PlayWorld as the GWorld, because who knows what will happen in the Tick.
		UWorld* OldGWorld = SetPlayInEditorWorld( PlayWorld );

		// Release mouse if the game is paused. The low level input code might ignore the request when e.g. in fullscreen mode.
		if ( GameViewport != NULL && GameViewport->Viewport != NULL )
		{
			// Decide whether to drop high detail because of frame rate
			GameViewport->SetDropDetail(DeltaSeconds);
		}

		// Update the level.
		GameCycles=0;
		CLOCK_CYCLES(GameCycles);

		{
			// So that hierarchical stats work in PIE
			SCOPE_CYCLE_COUNTER(STAT_FrameTime);
			// tick the level
			PlayWorld->Tick( LEVELTICK_All, DeltaSeconds );
		}

		UNCLOCK_CYCLES(GameCycles);

		// Tick the viewports.
		if ( GameViewport != NULL )
		{
			GameViewport->Tick(DeltaSeconds);
			// Tick the play in editor viewport now that the play world is active
			// so that we can properly process events that are only allowed if world has begun play 
			Client->TickPlayInEditorViewport(DeltaSeconds);
		}

//#if defined(WITH_FACEFX) && defined(FX_TRACK_MEMORY_STATS)
#if OLD_STATS
		if( GFaceFXStats.Enabled )
		{
			GFaceFXStats.NumCurrentAllocations.Value  += OC3Ent::Face::FxGetCurrentNumAllocations();
			GFaceFXStats.CurrentAllocationsSize.Value += OC3Ent::Face::FxGetCurrentBytesAllocated() / 1024.0f;
			GFaceFXStats.PeakAllocationsSize.Value    += OC3Ent::Face::FxGetMaxBytesAllocated() / 1024.0f;
		}
#endif // WITH_FACEFX && FX_TRACK_MEMORY_STATS

		// Pop the world
		RestoreEditorWorld( OldGWorld );
	}

	// If a request has been made to open a new Kismet debugger window while the PlayInEditor world was active, open one now
	if (bIsKismetDebuggerRequested)
	{
		// Attempt to register the breakpoints with the Kismet debugger
		UBOOL bQueuedBreakpointsWereAdded = TRUE;
		for (INT i = 0; i < KismetDebuggerBreakpointQueue.Num(); ++i)
		{
			UBOOL bWasBreakpointAdded = WxKismet::AddSequenceBreakpointToQueue(*KismetDebuggerBreakpointQueue(i));
			bQueuedBreakpointsWereAdded &= bWasBreakpointAdded;
			if (bWasBreakpointAdded)
			{
				KismetDebuggerBreakpointQueue.Remove(i);
				--i;
			}
		}
		// If all the breakpoints were resgistered, bIsKismetDebuggerRequested will be set to false
		bIsKismetDebuggerRequested = !bQueuedBreakpointsWereAdded; 
		
		WxKismet::OpenKismetDebugger(GWorld->GetGameSequence(), NULL, NULL, TRUE);
	}

	// Clean up any game viewports that may have been closed during the level tick (eg by Kismet).
	CleanupGameViewport();

	// If all viewports closed, close the current play level.
	if( GameViewport == NULL && PlayWorld )
	{
		EndPlayMap();
	}

	// Handle decal update requests.
	if ( bDecalUpdateRequested )
	{
		bDecalUpdateRequested = FALSE;
		for( FActorIterator It; It; ++It )
		{
			ADecalActorBase* DecalActor = Cast<ADecalActorBase>( *It );
			if ( DecalActor && DecalActor->Decal )
			{
				FComponentReattachContext ReattachContext( DecalActor->Decal );
			}
		}
	}


	// Update viewports.
	UBOOL bIsMouseOverAnyLevelViewport = FALSE;
	for(INT ViewportIndex = 0;ViewportIndex < ViewportClients.Num();ViewportIndex++)
	{
		FEditorLevelViewportClient* ViewportClient = ViewportClients( ViewportIndex );
		ViewportClient->Tick(DeltaSeconds);

		// Keep track of whether the mouse cursor is over any level viewports
		if( ViewportClient->Viewport != NULL && ViewportClient->IsEditorFrameClient() )
		{
			const INT MouseX = ViewportClient->Viewport->GetMouseX();
			const INT MouseY = ViewportClient->Viewport->GetMouseY();
			if( MouseX >= 0 && MouseY >= 0 && MouseX < (INT)ViewportClient->Viewport->GetSizeX() && MouseY < (INT)ViewportClient->Viewport->GetSizeY() )
			{
				bIsMouseOverAnyLevelViewport = TRUE;
			}
		}
	}

	// If the cursor is outside all level viewports, then clear the hover effect
	if( !bIsMouseOverAnyLevelViewport )
	{
		FEditorLevelViewportClient::ClearHoverFromObjects();
	}


	// Commit changes to the BSP model.
	GWorld->CommitModelSurfaces();

	
	UBOOL bUpdateLinkedOrthoViewports = FALSE;
	/////////////////////////////
	// Redraw viewports.

	// Render view parents, then view children.
	UBOOL bEditorFrameNonRealtimeViewportDrawn = FALSE;
	if (GCurrentLevelEditingViewportClient && GCurrentLevelEditingViewportClient->IsVisible())
	{
		UBOOL bAllowNonRealtimeViewports = TRUE;
		UBOOL bWasNonRealtimeViewportDraw = UpdateSingleViewportClient(GCurrentLevelEditingViewportClient, bAllowNonRealtimeViewports, bUpdateLinkedOrthoViewports );
		if (GCurrentLevelEditingViewportClient->IsEditorFrameClient())
		{
			bEditorFrameNonRealtimeViewportDrawn |= bWasNonRealtimeViewportDraw;
		}
	}
	for(UBOOL bRenderingChildren = 0;bRenderingChildren < 2;bRenderingChildren++)
	{
		for(INT ViewportIndex=0; ViewportIndex<ViewportClients.Num(); ViewportIndex++ )
		{
			FEditorLevelViewportClient* ViewportClient = ViewportClients(ViewportIndex);
			if (ViewportClient == GCurrentLevelEditingViewportClient)
			{
				//already given this window a chance to update
				continue;
			}

			if( !ViewportClient->IsEditorFrameClient() || ViewportClient->IsVisible() )
			{
				// Only update ortho viewports if that mode is turned on, the viewport client we are about to update is orthographic and the current editing viewport is orthographic and tracking mouse movement.
				bUpdateLinkedOrthoViewports = GEditor->GetUserSettings().bUseLinkedOrthographicViewports && ViewportClient->IsOrtho() && GCurrentLevelEditingViewportClient && GCurrentLevelEditingViewportClient->IsOrtho() && GCurrentLevelEditingViewportClient->bIsTracking;
				
				const UBOOL bIsViewParent = ViewportClient->ViewState->IsViewParent();
				if(	(bRenderingChildren && !bIsViewParent) ||
					(!bRenderingChildren && bIsViewParent) || bUpdateLinkedOrthoViewports )
				{
					//if we haven't drawn a non-realtime viewport OR not one of the main viewports
					UBOOL bAllowNonRealtimeViewports = (!bEditorFrameNonRealtimeViewportDrawn) || !(ViewportClient->IsEditorFrameClient());
					UBOOL bWasNonRealtimeViewportDrawn = UpdateSingleViewportClient(ViewportClient, bAllowNonRealtimeViewports, bUpdateLinkedOrthoViewports );
					if (ViewportClient->IsEditorFrameClient())
					{
						bEditorFrameNonRealtimeViewportDrawn |= bWasNonRealtimeViewportDrawn;
					}
				}
			}
		}
	}

#if HAVE_SCC
	FSourceControl::Tick();
#endif

	// Tick any open managed color picker windows
#if WITH_MANAGED_CODE
	TickColorPickerWPF();
	TickFileSystemNotifications();
#endif

	// Render playworld. This needs to happen after the other viewports for screenshots to work correctly in PIE.
	if(PlayWorld)
	{
		// Use the PlayWorld as the GWorld, because who knows what will happen in the Tick.
		UWorld* OldGWorld = SetPlayInEditorWorld( PlayWorld );
		
		// Render everything.
		if ( GameViewport != NULL )
		{
			GameViewport->eventLayoutPlayers();
			check(GameViewport->Viewport);
			GameViewport->Viewport->Draw();
		}

		// Pop the world
		RestoreEditorWorld( OldGWorld );
	}

	// Update resource streaming after both regular Editor viewports and PIE had a chance to add viewers.
	GStreamingManager->Tick( DeltaSeconds );

	// Update Audio. This needs to occur after rendering as the rendering code updates the listener position.
	if( Client && Client->GetAudioDevice() )
	{
		UWorld* OldGWorld = NULL;
		if( PlayWorld )
		{
			// Use the PlayWorld as the GWorld if we're using PIE.
			OldGWorld = SetPlayInEditorWorld( PlayWorld );
		}

		// Update audio device.
		Client->GetAudioDevice()->Update( (!PlayWorld && bAudioIsRealtime) || ( PlayWorld && !PlayWorld->IsPaused() ) );

		if( PlayWorld )
		{
			// Pop the world.
			RestoreEditorWorld( OldGWorld );
		}
	}

	AI_PROFILER( FAIProfiler::GetInstance().Tick(); )

	// Update constraints if dirtied.
	UpdateConstraintActors();

	// Tick the GRenderingRealtimeClock, unless it's paused
	if ( GPauseRenderingRealtimeClock == FALSE )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			TickRenderingTimer,
			FTimer*, Timer, &GRenderingRealtimeClock,
			FLOAT, DeltaTime, DeltaSeconds,
		{
			Timer->Tick(DeltaTime);
		});
	}

	// Track Editor startup time.
	static UBOOL bIsFirstTick = TRUE;
	if( bIsFirstTick )
	{
		GTaskPerfTracker->AddTask( TEXT("Editor startup"), TEXT(""), appSeconds() - GStartTime );
		bIsFirstTick = FALSE;
	}
}

/** Get tick rate limitor. */
FLOAT UEditorEngine::GetMaxTickRate( FLOAT DeltaTime, UBOOL bAllowFrameRateSmoothing )
{
	const FLOAT SuperMaxTickRate = Super::GetMaxTickRate( DeltaTime, bAllowFrameRateSmoothing );
	if( SuperMaxTickRate != 0.0f )
	{
		return SuperMaxTickRate;
	}

	FLOAT MaxTickRate = 0.0f;

	// Clamp editor frame rate, even if smoothing is disabled
	if( !bSmoothFrameRate && GIsEditor && !GIsPlayInEditorWorld )
	{
		MaxTickRate = Clamp<FLOAT>( 1.0f / DeltaTime, MinSmoothedFrameRate, MaxSmoothedFrameRate );
	}

	return MaxTickRate;
}

/**
 * Updates a single viewport
 * @param Viewport - the viewport that we're trying to draw
 * @param bInAllowNonRealtimeViewportToDraw - whether or not to allow non-realtime viewports to update
 * @return - Whether a NON-realtime viewport has updated in this call.  Used to help time-slice canvas redraws
 */
UBOOL UEditorEngine::UpdateSingleViewportClient(FEditorLevelViewportClient* InViewportClient, const UBOOL bInAllowNonRealtimeViewportToDraw, UBOOL bLinkedOrthoMovement )
{
	UBOOL bUpdatedNonRealtimeViewport = FALSE;
	// Add view information for perspective viewports.
	if( InViewportClient->ViewportType == LVT_Perspective )
	{
		GStreamingManager->AddViewInformation( InViewportClient->ViewLocation, InViewportClient->Viewport->GetSizeX(), InViewportClient->Viewport->GetSizeX() / appTan(InViewportClient->ViewFOV) );
	}
	// Redraw the viewport if it's realtime.
	if( InViewportClient->IsRealtime() )
	{
		InViewportClient->Viewport->Draw();
	}
	// Redraw any linked ortho viewports that need to be updated this frame.
	else if( InViewportClient->IsOrtho() && bLinkedOrthoMovement && InViewportClient->IsVisible() )
	{
		if( InViewportClient->bNeedsLinkedRedraw  )
		{
			// Redraw this viewport
			InViewportClient->Viewport->Draw();
			InViewportClient->bNeedsLinkedRedraw = FALSE;
		}
		else
		{
			// This viewport doesnt need to be redrawn.  Skip this frame and increment the number of frames we skipped.
			InViewportClient->FramesSinceLastDraw++;
		}
	}
	// Redraw the viewport if there are pending redraw, and we haven't already drawn one viewport this frame.
	else if ((InViewportClient->NumPendingRedraws > 0) && (bInAllowNonRealtimeViewportToDraw))
	{
		InViewportClient->Viewport->Draw();

		//NOTE - this invalidate hit proxy call is here to ensure that drag and drop leaves a correct hit proxy after the operation.
		//In the example of dragging a proc building rule set onto a proc building volume, the hit proxy is updated early by UWindowsClient::ProcessDeferredMessages trying to 
		//update the cursor.  However, the proc building is not done being regenerated yet and the hit proxy will not be correct.
		//This is not the case for changing things via the property window.  In that case, the render happens (after the proc building is properly regenerated) and the next time an
		//update cursor is called, the hit proxy will be correct.
		InViewportClient->Viewport->InvalidateHitProxy();
		
		InViewportClient->NumPendingRedraws--;
		bUpdatedNonRealtimeViewport = TRUE;
	}
	return bUpdatedNonRealtimeViewport;
}

/**
* Callback for when a editor property changed.
*
* @param	PropertyThatChanged	Property that changed and initiated the callback.
*/
void UEditorEngine::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged )
	{
		// Check to see if the FOVAngle property changed, if so loop through all viewports and update their fovs.
		const UBOOL bFOVAngleChanged = (appStricmp( *PropertyThatChanged->GetName(), TEXT("FOVAngle") ) == 0) ? TRUE : FALSE;
		
		if( bFOVAngleChanged )
		{
			// Clamp the FOV value to valid ranges.
			GEditor->FOVAngle = Clamp<FLOAT>( GEditor->FOVAngle, 1.0f, 179.0f );

			// Loop through all viewports and update their FOV angles and invalidate them, forcing them to redraw.
			for( INT ViewportIndex = 0 ; ViewportIndex < ViewportClients.Num() ; ++ViewportIndex )
			{
				if (ViewportClients(ViewportIndex) && ViewportClients(ViewportIndex)->Viewport)
				{
					ViewportClients( ViewportIndex )->ViewFOV = GEditor->FOVAngle;
					ViewportClients( ViewportIndex )->Invalidate();
				}
			}
		}
	}

	// Propagate the callback up to the superclass.
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FViewport* UEditorEngine::GetAViewport()
{
	if(GameViewport && GameViewport->Viewport)
	{
		return GameViewport->Viewport;
	}

	for(INT i=0; i<ViewportClients.Num(); i++)
	{
		if(ViewportClients(i) && ViewportClients(i)->Viewport)
		{
			return ViewportClients(i)->Viewport;
		}
	}

	return NULL;
}

/*-----------------------------------------------------------------------------
	Cleanup.
-----------------------------------------------------------------------------*/

/**
 * Cleans up after major events like e.g. map changes.
 *
 * @param	ClearSelection	Whether to clear selection
 * @param	Redraw			Whether to redraw viewports
 * @param	TransReset		Human readable reason for resetting the transaction system
 */
void UEditorEngine::Cleanse( UBOOL ClearSelection, UBOOL Redraw, const TCHAR* TransReset )
{
	check(TransReset);
	if( GIsRunning && !Bootstrapping )
	{
		if( ClearSelection )
		{
			// Clear selection sets.
			GetSelectedActors()->DeselectAll();
			GetSelectedObjects()->DeselectAll();
		}

		// Reset the transaction tracking system.
		ResetTransaction( TransReset );

		// Invalidate hit proxies as they can retain references to objects over a few frames
		GCallbackEvent->Send( CALLBACK_CleanseEditor );

		// Redraw the levels.
		if( Redraw )
		{
			RedrawLevelEditingViewports();
		}

		// Collect garbage.
		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
	}
}

/*---------------------------------------------------------------------------------------
	Components.
---------------------------------------------------------------------------------------*/

void UEditorEngine::ClearComponents()
{
	GEditorModeTools().ClearComponents();

	if( GWorld != NULL )
	{
		GWorld->ClearComponents();
	}
}

void UEditorEngine::UpdateComponents()
{
	GEditorModeTools().UpdateComponents();

	GWorld->UpdateComponents( FALSE );
}

/** 
 * Returns an audio component linked to the current scene that it is safe to play a sound on
 *
 * @param	SoundCue	A sound cue to attach to the audio component
 * @param	SoundNode	A sound node that is attached to the audio component when the sound cue is NULL
 */
UAudioComponent* UEditorEngine::GetPreviewAudioComponent( USoundCue* SoundCue, USoundNode* SoundNode )
{
	if( Client && Client->GetAudioDevice() )
	{
		if( PreviewAudioComponent == NULL )
		{
			PreviewSoundCue = ConstructObject<USoundCue>( USoundCue::StaticClass() );
			PreviewAudioComponent = Client->GetAudioDevice()->CreateComponent( PreviewSoundCue, GWorld->Scene, NULL, FALSE );
		}

		check( PreviewAudioComponent );
		// Mark as a preview component so the distance calculations can be ignored
		PreviewAudioComponent->bPreviewComponent = TRUE;

		if( SoundNode != NULL )
		{
			PreviewSoundCue->FirstNode = SoundNode;
			PreviewAudioComponent->SoundCue = PreviewSoundCue;
		}
		else
		{
			PreviewAudioComponent->SoundCue = SoundCue;
		}
	}

	return( PreviewAudioComponent );
}

/** 
 * Stop any sounds playing on the preview audio component and allowed it to be garbage collected
 */
void UEditorEngine::ClearPreviewAudioComponents( void )
{
	if( PreviewAudioComponent )
	{
		PreviewAudioComponent->Stop();

		// Just null out so they get GC'd
		PreviewSoundCue->FirstNode = NULL;
		PreviewSoundCue = NULL;
		PreviewAudioComponent->SoundCue = NULL;
		PreviewAudioComponent = NULL;
	}
}

/*---------------------------------------------------------------------------------------
	Misc.
---------------------------------------------------------------------------------------*/

/**
 * Issued by code reuqesting that decals be reattached.
 */
void UEditorEngine::IssueDecalUpdateRequest()
{
	bDecalUpdateRequested = TRUE;
}

void UEditorEngine::SetObjectPropagationDestination(INT Destination)
{
	// Layout of the ComboBox:
	//   No Propagation
	//   Local Standalone (a game running, but not In Editor)
	//   Console 0
	//   Console 1
	//   ...

	// remember out selection for when we restart the editor
	GConfig->SetInt(TEXT("ObjectPropagation"), TEXT("Destination"), Destination, GEditorIni);

	// first one is no propagation
	if (Destination == OPD_None)
	{
		FObjectPropagator::ClearPropagator();
	}
	else
	{
		// the rest are network propagations
		FObjectPropagator::SetPropagator(RemotePropagator);
		GObjectPropagator->ClearTargets();

		// the first one of these is a local standalone game (so we use 127.0.0.1)
		if (Destination == OPD_LocalStandalone)
		{
			GObjectPropagator->AddTarget(INVALID_TARGETHANDLE, 0x7F000001, TRUE); // 127.0.0.1
		}
		else
		{
			FConsoleSupport *CurPlatform = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(Destination - OPD_ConsoleStart);

			INT NumTargets = CurPlatform->GetTargets(NULL);
			TArray<TARGETHANDLE> SelectedMenuItems(NumTargets);

			CurPlatform->GetMenuSelectedTargets(&SelectedMenuItems(0), NumTargets);

			for(INT CurTargetIndex = 0; CurTargetIndex < NumTargets; ++CurTargetIndex)
			{
				GObjectPropagator->AddTarget(SelectedMenuItems(CurTargetIndex), htonl(CurPlatform->GetIPAddress(SelectedMenuItems(CurTargetIndex))), CurPlatform->GetIntelByteOrder());
			}
		}
	}
}

/**
 *	Returns pointer to a temporary render target.
 *	If it has not already been created, does so here.
 */
UTextureRenderTarget2D* UEditorEngine::GetScratchRenderTarget( UINT MinSize )
{
	UTextureRenderTargetFactoryNew* NewFactory = CastChecked<UTextureRenderTargetFactoryNew>( StaticConstructObject(UTextureRenderTargetFactoryNew::StaticClass()) );

	UTextureRenderTarget2D* ScratchRenderTarget = NULL;

	// We never allow render targets greater than 2048
	check( MinSize <= 2048 );

	// 256x256
	if( MinSize <= 256 )
	{
		if( ScratchRenderTarget256 == NULL )
		{
			NewFactory->Width = 256;
			NewFactory->Height = 256;
			UObject* NewObj = NewFactory->FactoryCreateNew( UTextureRenderTarget2D::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Transient, NULL, GWarn );
			ScratchRenderTarget256 = CastChecked<UTextureRenderTarget2D>(NewObj);
		}
		ScratchRenderTarget = ScratchRenderTarget256;
	}
	// 512x512
	else if( MinSize <= 512 )
	{
		if( ScratchRenderTarget512 == NULL )
		{
			NewFactory->Width = 512;
			NewFactory->Height = 512;
			UObject* NewObj = NewFactory->FactoryCreateNew( UTextureRenderTarget2D::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Transient, NULL, GWarn );
			ScratchRenderTarget512 = CastChecked<UTextureRenderTarget2D>(NewObj);
		}
		ScratchRenderTarget = ScratchRenderTarget512;
	}
	// 1024x1024
	else if( MinSize <= 1024 )
	{
		if( ScratchRenderTarget1024 == NULL )
		{
			NewFactory->Width = 1024;
			NewFactory->Height = 1024;
			UObject* NewObj = NewFactory->FactoryCreateNew( UTextureRenderTarget2D::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Transient, NULL, GWarn );
			ScratchRenderTarget1024 = CastChecked<UTextureRenderTarget2D>(NewObj);
		}
		ScratchRenderTarget = ScratchRenderTarget1024;
	}
	// 2048x2048
	else if( MinSize <= 2048 )
	{
		if( ScratchRenderTarget2048 == NULL )
		{
			NewFactory->Width = 2048;
			NewFactory->Height = 2048;
			UObject* NewObj = NewFactory->FactoryCreateNew( UTextureRenderTarget2D::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Transient, NULL, GWarn );
			ScratchRenderTarget2048 = CastChecked<UTextureRenderTarget2D>(NewObj);
		}
		ScratchRenderTarget = ScratchRenderTarget2048;
	}

	check( ScratchRenderTarget != NULL );
	return ScratchRenderTarget;
}

/**
 * Check for any PrefabInstances which are out of date.  For any PrefabInstances which have a TemplateVersion less than its PrefabTemplate's
 * PrefabVersion, propagates the changes to the source Prefab to the PrefabInstance.
 */
void UEditorEngine::UpdatePrefabs()
{
	TArray<UPrefab*> UpdatedPrefabs;
	TArray<APrefabInstance*> RemovedPrefabs;
	UBOOL bResetInvalidPrefab = FALSE;

	USelection* SelectedActors = GetSelectedActors();
	for( FActorIterator It; It; ++It )
	{
		APrefabInstance* PrefabInst = Cast<APrefabInstance>(*It);

		// If this is a valid PrefabInstance
		if(	PrefabInst && !PrefabInst->bDeleteMe && !PrefabInst->IsPendingKill() )
		{
			// first, verify that this PrefabInstance is still bound to a valid prefab
			UPrefab* SourcePrefab = PrefabInst->TemplatePrefab;
			if ( SourcePrefab != NULL )
			{
				// first, verify that all archetypes in the prefab's ArchetypeToInstanceMap exist.
				if ( !PrefabInst->VerifyMemberArchetypes() )
				{
					bResetInvalidPrefab = TRUE;
				}

				// If the PrefabInstance's version number is less than the source Prefab's version (ie there is 
				// a newer version of the Prefab), update it now.
				if ( PrefabInst->TemplateVersion < SourcePrefab->PrefabVersion )
				{
					PrefabInst->UpdatePrefabInstance( GetSelectedActors() );

					// Mark the level package as dirty, so we are prompted to save the map.
					PrefabInst->MarkPackageDirty();

					// Add prefab to list of ones that we needed to update instances of.
					UpdatedPrefabs.AddUniqueItem(SourcePrefab);
				}
				else if ( PrefabInst->TemplateVersion > SourcePrefab->PrefabVersion )
				{
					bResetInvalidPrefab = TRUE;
					warnf(NAME_Warning, TEXT("PrefabInstance '%s' has a version number that is higher than the source Prefab's version.  Resetting existing PrefabInstance from source: '%s'"), *PrefabInst->GetPathName(), *SourcePrefab->GetPathName());

					// this PrefabInstance's version number is higher than the source Prefab's version number,
					// this is normally the result of updating a prefab, then saving the map but not the package containing
					// the prefab.  If this has occurred, we'll need to replace the existing PrefabInstance with a new copy 
					// of the older version of the Prefab, but we must warn the user when we do this!
					PrefabInst->DestroyPrefab(SelectedActors);
					PrefabInst->InstancePrefab(SourcePrefab);
				}
			}
			else
			{
				// if the PrefabInstance's TemplatePrefab is NULL, it probably means that the user created a prefab,
				// then reloaded the map containing the prefab instance without saving the package containing the prefab.
				PrefabInst->DestroyPrefab(SelectedActors);
				GWorld->DestroyActor(PrefabInst);
				SelectedActors->Deselect(PrefabInst);

				RemovedPrefabs.AddItem(PrefabInst);
			}
		}
	}

	// If we updated some prefab instances, display it in a dialog.
	if(UpdatedPrefabs.Num() > 0)
	{
		FString UpdatedPrefabList;
		for(INT i=0; i<UpdatedPrefabs.Num(); i++)
		{
			// Add name to list of updated Prefabs.
			UpdatedPrefabList += FString::Printf( TEXT("%s\n"), *(UpdatedPrefabs(i)->GetPathName()) );
		}

		appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("Prefab_OldPrefabInstancesUpdated"), *UpdatedPrefabList) );
	}

	if ( RemovedPrefabs.Num() )
	{
		FString RemovedPrefabList;
		for(INT i=0; i<RemovedPrefabs.Num(); i++)
		{
			// Add name to list of updated Prefabs.
			RemovedPrefabList += FString::Printf( TEXT("%s") LINE_TERMINATOR, *RemovedPrefabs(i)->GetPathName());
		}

		appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("Prefab_MissingSourcePrefabs"), *RemovedPrefabList) );
	}

	if ( bResetInvalidPrefab )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Prefab_ResetInvalidPrefab") );
	}
}


/** 
 *	Create a new instance of a prefab in the level. 
 *
 *	@param	Prefab		The prefab to create an instance of.
 *	@param	Location	Location to create the new prefab at.
 *	@param	Rotation	Rotation to create the new prefab at.
 *	@return				Pointer to new PrefabInstance actor in the level, or NULL if it fails.
 */
APrefabInstance* UEditorEngine::Prefab_InstancePrefab(UPrefab* Prefab, const FVector& Location, const FRotator& Rotation) const
{
	// First, create a new PrefabInstance actor.
	APrefabInstance* NewInstance = CastChecked<APrefabInstance>( GWorld->SpawnActor(APrefabInstance::StaticClass(), NAME_None, Location, Rotation) );
	if(NewInstance)
	{
		NewInstance->InstancePrefab(Prefab);
		return NewInstance;
	}
	else
	{
		return NULL;
	}
}

/** Util that looks for and fixes any incorrect ParentSequence pointers in Kismet objects in memory. */
void UEditorEngine::FixKismetParentSequences()
{
	TArray<FString> ParentSequenceFixed;

	for( TObjectIterator<USequenceObject> It; It; ++It )
	{
		USequenceObject* SeqObj = *It;

		// if this sequence object is a subobject template, skip it as it won't be initialized (no ParentSequence, etc.)
		if ( SeqObj->IsTemplate() )
		{
			continue;
		}

		// if there is no parent sequence look for a sequence that contains this obj
		if (SeqObj->ParentSequence == NULL)
		{
			UBOOL bFoundSeq = FALSE;
			for (TObjectIterator<USequence> SeqIt; SeqIt; ++SeqIt)
			{
				if (SeqIt->SequenceObjects.ContainsItem(SeqObj))
				{
					warnf(TEXT("Found parent sequence for object %s"),*SeqObj->GetPathName());
					bFoundSeq = TRUE;
					SeqObj->ParentSequence = *SeqIt;
					SeqObj->MarkPackageDirty( TRUE );
					ParentSequenceFixed.AddItem( SeqObj->GetPathName() );
					break;
				}
			}
			if (!bFoundSeq && !SeqObj->IsA(USequence::StaticClass()))
			{
				USequence *Seq = Cast<USequence>(SeqObj->GetOuter());
				if (Seq != NULL)
				{
					warnf(TEXT("No containing sequence for object %s, placing in outer"),*SeqObj->GetPathName());
					Seq->SequenceObjects.AddItem(SeqObj);
					SeqObj->ParentSequence = Seq;
				}
				else
				{
					warnf(TEXT("No parent for object %s, placing in the root"),*SeqObj->GetPathName());
					Seq = GWorld->GetGameSequence();
					Seq->SequenceObjects.AddItem(SeqObj);
					SeqObj->ParentSequence = Seq;
				}

				SeqObj->MarkPackageDirty( TRUE );
				ParentSequenceFixed.AddItem( SeqObj->GetPathName() );
			}
		}

		USequence* RootSeq = SeqObj->GetRootSequence();
		// check it for general errors
		SeqObj->CheckForErrors();
		
		// If we have a ParentSequence, but it does not contain me - thats a bug. Fix it.
		if( SeqObj->ParentSequence && 
			!SeqObj->ParentSequence->SequenceObjects.ContainsItem(SeqObj) )
		{
			UBOOL bFoundCorrectParent = FALSE;

			// Find the sequence that _does_ contain SeqObj
			for( FObjectIterator TestIt; TestIt; ++TestIt )
			{
				UObject* TestObj = *TestIt;
				USequence* TestSeq = Cast<USequence>(TestObj);
				if(TestSeq && TestSeq->SequenceObjects.ContainsItem(SeqObj))
				{
					// If we have already found a sequence that contains SeqObj - warn about that too!
					if(bFoundCorrectParent)
					{
						warnf( TEXT("Multiple Sequences Contain '%s'"), *SeqObj->GetName() );
					}
					else
					{
						SeqObj->MarkPackageDirty( TRUE );
						ParentSequenceFixed.AddItem( SeqObj->GetPathName() );

						// Change ParentSequence pointer to correct sequence
						SeqObj->ParentSequence = TestSeq;

						// Mark package as dirty
						SeqObj->MarkPackageDirty();
						bFoundCorrectParent = TRUE;
					}
				}
			}

			// It's also bad if we couldn't find the correct parent!
			if(!bFoundCorrectParent)
			{
				debugf( TEXT("No correct parent found for '%s'.  Try entering 'OBJ REFS class=%s name=%s' into the command window to determine how this sequence object is being referenced."),
					*SeqObj->GetName(), *SeqObj->GetClass()->GetName(), *SeqObj->GetPathName() );
			}
		}
	}

	// If we corrected some things - tell the user
	if(ParentSequenceFixed.Num() > 0)
	{	
		appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("FixedKismetParentSequences"), ParentSequenceFixed.Num()));
	}
}

/**
*	Runs ConvertObject() and UpdateObject() on all out-of-date SequenceObjects
*/
void UEditorEngine::UpdateKismetObjects()
{
	// look for kismet sequences
	for( TObjectIterator<USequence> It; It; ++It )
	{
		TArray<USequenceObject*>& Objects = It->SequenceObjects;

		// Collect the data
		for( INT ObjIndex = 0; ObjIndex < Objects.Num(); ++ObjIndex )
		{
			USequenceObject* CurObj = Objects(ObjIndex);
			USequenceObject* NewObj = NULL;

			// Purposely exclude subsequences from being updated as sequences don't currently
			// need "versioning"
			if( CurObj && !CurObj->IsA( USequence::StaticClass() ) )
			{
				NewObj = CurObj->ConvertObject();
				if( NewObj )
				{
					warnf(TEXT("Converting Kismet Object %s"), *CurObj->GetName());
					NewObj->UpdateObject();
					Objects(ObjIndex) = NewObj;
					NewObj->MarkPackageDirty();
				}
				else if( CurObj->eventGetObjClassVersion() != CurObj->ObjInstanceVersion)
				{
					warnf(TEXT("Updating Kismet Object %s"), *CurObj->GetName());
					CurObj->UpdateObject();
					CurObj->MarkPackageDirty();
				}
			}
		}
	}
}

/** Since the Kismet window cannot be modified while PlayInEditor world is active, this method can be used to defer a call to WxKismet::OpenKismetDebugger() when the editor world is active */
void UEditorEngine::RequestKismetDebuggerOpen(const TCHAR* SequenceName)
{
	check(SequenceName);
	if (GKismetRealtimeDebugging)
	{
		KismetDebuggerBreakpointQueue.AddItem(FString(SequenceName));
		bIsKismetDebuggerRequested = TRUE;
	}
}

/**
 * Warns the user of any hidden levels, and prompts them with a Yes/No dialog
 * for whether they wish to continue with the operation.  No dialog is presented if all
 * levels are visible.  The return value is TRUE if no levels are hidden or
 * the user selects "Yes", or FALSE if the user selects "No".
 *
 * @param	bIncludePersistentLvl	If TRUE, the persistent level will also be checked for visibility
 * @param	AdditionalMessage		An additional message to include in the dialog.  Can be NULL.
 * @return							FALSE if the user selects "No", TRUE otherwise.
 */
UBOOL UEditorEngine::WarnAboutHiddenLevels(UBOOL bIncludePersistentLvl, const TCHAR* AdditionalMessage) const
{
	UBOOL bResult = TRUE;

	const UBOOL bPersistentLvlHidden = !FLevelUtils::IsLevelVisible( GWorld->PersistentLevel );

	// Make a list of all hidden streaming levels.
	AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
	TArray< ULevelStreaming* > HiddenLevels;
	for( INT LevelIndex = 0 ; LevelIndex< WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
	{
		ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels( LevelIndex );
		if( StreamingLevel && !FLevelUtils::IsLevelVisible( StreamingLevel ) )
		{
			HiddenLevels.AddItem( StreamingLevel );
		}
	}

	// Warn the user that some levels are hidden and prompt for continue.
	if ( ( bIncludePersistentLvl && bPersistentLvlHidden ) || HiddenLevels.Num() > 0 )
	{
		FString ContinueMessage( bIncludePersistentLvl ? LocalizeUnrealEd("TheFollowingLevelsAreHidden") : LocalizeUnrealEd("TheFollowingStreamingLevelsAreHidden") );
		if ( bIncludePersistentLvl && bPersistentLvlHidden )
		{
			ContinueMessage += FString::Printf( TEXT("\n    %s"), *LocalizeUnrealEd("PersistentLevel") );
		}
		for ( INT LevelIndex = 0 ; LevelIndex < HiddenLevels.Num() ; ++LevelIndex )
		{
			ContinueMessage += FString::Printf( TEXT("\n    %s"), *HiddenLevels(LevelIndex)->PackageName.ToString() );
		}
		if ( AdditionalMessage )
		{
			ContinueMessage += FString::Printf( TEXT("\n%s"), AdditionalMessage );
		}
		else
		{
			ContinueMessage += FString::Printf( TEXT("\n%s"), *LocalizeUnrealEd(TEXT("ContinueQ")) );
		}

		// return code for the choice dialog if the user presses make all visible.
		const INT MakeAllVisible = 2;

		// Create and show the user the dialog.
		WxChoiceDialog ChoiceDlg(
			ContinueMessage,
			LocalizeUnrealEd("HiddenLevelDialogTitle"),
			WxChoiceDialogBase::Choice( MakeAllVisible, LocalizeUnrealEd("HiddenLevelDialogMakeVisible"),  WxChoiceDialogBase::DCT_DefaultAffirmative ),
			WxChoiceDialogBase::Choice( ART_Yes, LocalizeUnrealEd("Yes") ),
			WxChoiceDialogBase::Choice( ART_No, LocalizeUnrealEd("No"), WxChoiceDialogBase::DCT_DefaultCancel ) );

		ChoiceDlg.SetWindowStyle( ChoiceDlg.GetWindowStyle() | wxSTAY_ON_TOP );
		ChoiceDlg.ShowModal();

		const INT Choice = ChoiceDlg.GetChoice().ReturnCode;

		if( Choice == MakeAllVisible )
		{
			if ( bIncludePersistentLvl && bPersistentLvlHidden )
			{
				FLevelUtils::SetLevelVisibility( NULL, GWorld->PersistentLevel, TRUE, FALSE );
			}

			// The code below should technically also make use of FLevelUtils::SetLevelVisibility, but doing
			// so would be much more inefficient, resulting in several calls to UpdateLevelStreaming
			for( INT HiddenLevelIdx = 0; HiddenLevelIdx < HiddenLevels.Num(); ++HiddenLevelIdx )
			{
				HiddenLevels( HiddenLevelIdx )->bShouldBeVisibleInEditor = TRUE;
			}

			GWorld->UpdateLevelStreaming();
			GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
		}

		// return true if the user pressed make all visible or yes.
		bResult = (Choice != ART_No);
	}

	return bResult;
}

/**
 *	Sets the texture to use for displaying StreamingBounds.
 *
 *	@param	InTexture	The source texture for displaying StreamingBounds.
 *						Pass in NULL to disable displaying them.
 */
void UEditorEngine::SetStreamingBoundsTexture(UTexture2D* InTexture)
{
	if (StreamingBoundsTexture != InTexture)
	{
		// Clear the currently stored streaming bounds information

		// Set the new texture
		StreamingBoundsTexture = InTexture;
		if (StreamingBoundsTexture != NULL)
		{
			// Fill in the new streaming bounds info
			ULevel::BuildStreamingData(NULL, NULL, InTexture);

			// Turn on the StreamingBounds show flag
			for(UINT ViewportIndex = 0; ViewportIndex < (UINT)ViewportClients.Num(); ViewportIndex++)
			{
				FEditorLevelViewportClient* ViewportClient = ViewportClients(ViewportIndex);
				if (ViewportClient)
				{
					ViewportClient->ShowFlags |= SHOW_StreamingBounds;
					ViewportClient->Invalidate(FALSE,TRUE);
				}
			}
		}
		else
		{
			// Clear the streaming bounds info
			// Turn off the StreamingBounds show flag
			for(UINT ViewportIndex = 0; ViewportIndex < (UINT)ViewportClients.Num(); ViewportIndex++)
			{
				FEditorLevelViewportClient* ViewportClient = ViewportClients(ViewportIndex);
				if (ViewportClient)
				{
					ViewportClient->ShowFlags &= ~SHOW_StreamingBounds;
					ViewportClient->Invalidate(FALSE,TRUE);
				}
			}
		}
	}
}

void UEditorEngine::ApplyDeltaToActor(AActor* InActor,
									  UBOOL bDelta,
									  const FVector* InTrans,
									  const FRotator* InRot,
									  const FVector* InScale,
									  UBOOL bAltDown,
									  UBOOL bShiftDown,
									  UBOOL bControlDown) const
{
	InActor->Modify();
	if( InActor->IsBrush() )
	{
		ABrush* Brush = (ABrush*)InActor;
		if( Brush->BrushComponent && Brush->BrushComponent->Brush )
		{
			Brush->BrushComponent->Brush->Polys->Element.ModifyAllItems();
		}
	}

	///////////////////
	// Rotation

	// Unfortunately this can't be moved into ABrush::EditorApplyRotation, as that would
	// create a dependence in Engine on Editor.
	if ( InRot )
	{
		const FRotator& InDeltaRot = *InRot;
		const UBOOL bRotatingActor = !bDelta || !InDeltaRot.IsZero();
		if( bRotatingActor )
		{
			if( InActor->IsBrush() )
			{
				FBSPOps::RotateBrushVerts( (ABrush*)InActor, InDeltaRot, TRUE );
			}
			else
			{
				if ( bDelta )
				{
					InActor->EditorApplyRotation( InDeltaRot, bAltDown, bShiftDown, bControlDown );
				}
				else
				{
					InActor->SetRotation( InDeltaRot );
				}
			}

			if ( bDelta )
			{
				FVector NewActorLocation = InActor->Location;
				NewActorLocation -= GEditorModeTools().PivotLocation;
				NewActorLocation = FRotationMatrix( InDeltaRot ).TransformFVector( NewActorLocation );
				NewActorLocation += GEditorModeTools().PivotLocation;
				InActor->Location = NewActorLocation;
			}
		}
	}

	///////////////////
	// Translation
	if ( InTrans )
	{
		if ( bDelta )
		{
			InActor->EditorApplyTranslation( *InTrans, bAltDown, bShiftDown, bControlDown );
		}
		else
		{
			InActor->SetLocation( *InTrans );
		}
	}

	///////////////////
	// Scaling
	if ( InScale )
	{
		const FVector& InDeltaScale = *InScale;
		const UBOOL bScalingActor = !bDelta || !InDeltaScale.IsNearlyZero(0.000001f);
		if( bScalingActor )
		{
			// If the actor is a brush, update the vertices.
			if( InActor->IsBrush() )
			{
				ABrush* Brush = (ABrush*)InActor;
				FVector ModifiedScale = InDeltaScale;
				UBOOL bModifiedScaleOK[3] = { TRUE, TRUE, TRUE };

				// Get Box Extents
				const FBox BrushBox = InActor->GetComponentsBoundingBox( TRUE );
				const FVector BrushExtents = BrushBox.GetExtent();

				// Make sure brushes are clamped to a minimum size.
				FLOAT MinThreshold = 1.0f;

				for (INT Idx=0; Idx<3; Idx++)
				{
					const UBOOL bBelowAllowableScaleThreshold = ((InDeltaScale[Idx] + 1.0f) * BrushExtents[Idx]) < MinThreshold;

					if(bBelowAllowableScaleThreshold)
					{
						// Guard against divide by zero
						if ( BrushExtents[Idx] != 0.0f )
						{
							ModifiedScale[Idx] = (MinThreshold / BrushExtents[Idx]) - 1.0f;
						}
						else
						{
							ModifiedScale[Idx] = MinThreshold;
							bModifiedScaleOK[Idx] = FALSE;	// Mark this value as not to be used when working out the smallest scale
						}
					}
				}

				// If we are uniformly scaling, make sure that the modified scale is always the same for all 3 axis.
				if(GEditorModeTools().GetWidgetMode() == FWidget::WM_Scale)
				{
					INT Min = 0;
					for(INT Idx=1; Idx < 3; Idx++)
					{
						if(bModifiedScaleOK[Idx] && Abs(ModifiedScale[Idx]) < Abs(ModifiedScale[Min]))
						{
							Min=Idx;
						}
					}

					for(INT Idx=0; Idx < 3; Idx++)
					{
						if(Min != Idx)
						{
							ModifiedScale[Idx] = ModifiedScale[Min];
						}
					}
				}

				// Scale all of the polygons of the brush.
				const FScaleMatrix matrix( FVector( ModifiedScale.X , ModifiedScale.Y, ModifiedScale.Z ) );
				
				if(Brush->BrushComponent->Brush && Brush->BrushComponent->Brush->Polys)
				{
					for( INT poly = 0 ; poly < Brush->BrushComponent->Brush->Polys->Element.Num() ; poly++ )
					{
						FPoly* Poly = &(Brush->BrushComponent->Brush->Polys->Element(poly));

						FBox bboxBefore(0);
						for( INT vertex = 0 ; vertex < Poly->Vertices.Num() ; vertex++ )
						{
							bboxBefore += Brush->LocalToWorld().TransformFVector( Poly->Vertices(vertex) );
						}

						// Scale the vertices

						for( INT vertex = 0 ; vertex < Poly->Vertices.Num() ; vertex++ )
						{
							FVector Wk = Brush->LocalToWorld().TransformFVector( Poly->Vertices(vertex) );
							Wk -= GEditorModeTools().PivotLocation;
							Wk += matrix.TransformFVector( Wk );
							Wk += GEditorModeTools().PivotLocation;
							Poly->Vertices(vertex) = Brush->WorldToLocal().TransformFVector( Wk );
						}

						FBox bboxAfter(0);
						for( INT vertex = 0 ; vertex < Poly->Vertices.Num() ; vertex++ )
						{
							bboxAfter += Brush->LocalToWorld().TransformFVector( Poly->Vertices(vertex) );
						}

						FVector Wk = Brush->LocalToWorld().TransformFVector( Poly->Base );
						Wk -= GEditorModeTools().PivotLocation;
						Wk += matrix.TransformFVector( Wk );
						Wk += GEditorModeTools().PivotLocation;
						Poly->Base = Brush->WorldToLocal().TransformFVector( Wk );

						// Scale the texture vectors

						for( INT a = 0 ; a < 3 ; ++a )
						{
							const FLOAT Before = bboxBefore.GetExtent()[a];
							const FLOAT After = bboxAfter.GetExtent()[a];

							if( After != 0.0 )
							{
								const FLOAT Pct = Before / After;

								if( Pct != 0.0 )
								{
									Poly->TextureU[a] *= Pct;
									Poly->TextureV[a] *= Pct;
								}
							}
						}

						// Recalc the normal for the poly

						Poly->Normal = FVector(0,0,0);
						Poly->Finalize((ABrush*)InActor,0);
					}

					Brush->BrushComponent->Brush->BuildBound();

					if( !Brush->IsStaticBrush() )
					{
						FBSPOps::csgPrepMovingBrush( Brush );
					}
				}
			}
			else
			{
				if ( bDelta )
				{
					const FScaleMatrix matrix( FVector( InDeltaScale.X , InDeltaScale.Y, InDeltaScale.Z ) );
					InActor->EditorApplyScale( InDeltaScale,
												matrix,
												&GEditorModeTools().PivotLocation,
												bAltDown,
												bShiftDown,
												bControlDown );
				}
				else
				{
					InActor->DrawScale3D = InDeltaScale;
				}
			}

			// We need to invalidate the lighting cache BEFORE clearing the components!
			InActor->InvalidateLightingCache();
			InActor->ClearComponents();
		}
	}

	// Update the actor before leaving.
	InActor->MarkPackageDirty();
	InActor->InvalidateLightingCache();
	InActor->PostEditMove( FALSE );
	InActor->ForceUpdateComponents();
}

/**
 * Handles freezing/unfreezing of rendering
 */
void UEditorEngine::ProcessToggleFreezeCommand()
{
	if (GIsPlayInEditorWorld)
	{
		GamePlayers(0)->ViewportClient->Viewport->ProcessToggleFreezeCommand();
	}
	else
	{
		// pass along the freeze command to all perspective viewports
		for(INT ViewportIndex = 0; ViewportIndex < ViewportClients.Num(); ViewportIndex++)
		{
			if (ViewportClients(ViewportIndex)->ViewportType == LVT_Perspective)
			{
				ViewportClients(ViewportIndex)->Viewport->ProcessToggleFreezeCommand();
			}
		}
	}

	// tell editor to update views
	RedrawAllViewports();
}

/**
 * Handles frezing/unfreezing of streaming
 */
void UEditorEngine::ProcessToggleFreezeStreamingCommand()
{
	// freeze vis in PIE
	if (GIsPlayInEditorWorld)
	{
		PlayWorld->bIsLevelStreamingFrozen = !PlayWorld->bIsLevelStreamingFrozen;
	}
}

/*-----------------------------------------------------------------------------
	Reimporting.
-----------------------------------------------------------------------------*/

/** 
* Singleton function
* @return Singleton instance of this manager
*/
FReimportManager* FReimportManager::Instance()
{
	static FReimportManager Inst;
	return &Inst;
}

/**
 * Register a reimport handler with the manager
 *
 * @param	InHandler	Handler to register with the manager
 */
void FReimportManager::RegisterHandler( FReimportHandler& InHandler )
{
	Handlers.AddUniqueItem( &InHandler );
}

/**
 * Unregister a reimport handler from the manager
 *
 * @param	InHandler	Handler to unregister from the manager
 */
void FReimportManager::UnregisterHandler( FReimportHandler& InHandler )
{
	Handlers.RemoveItem( &InHandler );
}

/**
 * Attempt to reimport the specified object from its source by giving registered reimport
 * handlers a chance to try to reimport the object
 *
 * @param	Obj	Object to try reimporting
 *
 * @return	TRUE if the object was handled by one of the reimport handlers; FALSE otherwise
 */
UBOOL FReimportManager::Reimport( UObject* Obj )
{
	for( INT HandlerIndex = 0; HandlerIndex < Handlers.Num(); ++HandlerIndex )
	{
		if( Handlers( HandlerIndex )->Reimport( Obj ) )
		{
			return TRUE;
		}
	}
	return FALSE;
}


/** Constructor */
FReimportManager::FReimportManager()
{
	// Create reimport handler for textures
	// NOTE: New factories can be created anywhere, inside or outside of editor
	// This is just here for convenience
	UReimportTextureFactory::StaticClass();

	// Create reimport handler for static meshes
	UReimportStaticMeshFactory::StaticClass();

	// Create reimport handler for FBX static meshes
	UReimportFbxStaticMeshFactory::StaticClass();

	// Create reimport handler for skeletal meshes
	UReimportSkeletalMeshFactory::StaticClass();

	// Create reimport handler for FBX skeletal meshes
	UReimportFbxSkeletalMeshFactory::StaticClass();

	// Create reimport handler for sound node waves
	UReimportSoundFactory::StaticClass();
	// Create reimport handler for Apex Assets
	UReimportApexGenericAssetFactory::StaticClass();
}

/** Destructor */
FReimportManager::~FReimportManager()
{
	Handlers.Empty();
}

/*-----------------------------------------------------------------------------
	PIE helpers.
-----------------------------------------------------------------------------*/

/**
 * Sets GWorld to the passed in PlayWorld and sets a global flag indicating that we are playing
 * in the Editor.
 *
 * @param	PlayInEditorWorld		PlayWorld
 * @return	the original GWorld
 */
UWorld* SetPlayInEditorWorld( UWorld* PlayInEditorWorld )
{
	check(!GIsPlayInEditorWorld);
	UWorld* SavedWorld = GWorld;
	GIsPlayInEditorWorld = TRUE;
	GIsGame = (!GIsEditor || GIsPlayInEditorWorld);
	GWorld = PlayInEditorWorld;

	// switch to using the PIE cross level reference manager so any operations while the PIE world is active are contained to PIE objects
	FCrossLevelReferenceManager::SwitchToPIEManager();

	return SavedWorld;
}

/**
 * Restores GWorld to the passed in one and reset the global flag indicating whether we are a PIE
 * world or not.
 *
 * @param EditorWorld	original world being edited
 */
void RestoreEditorWorld( UWorld* EditorWorld )
{
	check(GIsPlayInEditorWorld);
	GIsPlayInEditorWorld = FALSE;
	GIsGame = (!GIsEditor || GIsPlayInEditorWorld);
	GWorld = EditorWorld;

	// restore the global cross level ref manager to the default manager
	FCrossLevelReferenceManager::SwitchToStandardManager();
}


/*-----------------------------------------------------------------------------
	Cooking helpers.
-----------------------------------------------------------------------------*/

/*
 * Create a struct to pass to tools from the parsed wave data
 */
void CreateWaveFormat( FWaveModInfo& WaveInfo, WAVEFORMATEXTENSIBLE& WaveFormat )
{
	appMemzero( &WaveFormat, sizeof( WAVEFORMATEXTENSIBLE ) );

	WaveFormat.Format.nAvgBytesPerSec = *WaveInfo.pAvgBytesPerSec;
	WaveFormat.Format.nBlockAlign = *WaveInfo.pBlockAlign;
	WaveFormat.Format.nChannels = *WaveInfo.pChannels;
	WaveFormat.Format.nSamplesPerSec = *WaveInfo.pSamplesPerSec;
	WaveFormat.Format.wBitsPerSample = *WaveInfo.pBitsPerSample;
	WaveFormat.Format.wFormatTag = WAVE_FORMAT_PCM;
}

/**
 * Read a wave file header from bulkdata
 */
UBOOL ReadWaveHeader( FWaveModInfo& WaveInfo, BYTE* RawWaveData, INT Size, INT Offset )
{
	if( Size == 0 )
	{
		return( FALSE );
	}

	// Parse wave info.
	if( !WaveInfo.ReadWaveInfo( RawWaveData + Offset, Size ) )
	{
		return( FALSE );
	}

	// Validate the info
	if( ( *WaveInfo.pChannels != 1 && *WaveInfo.pChannels != 2 ) || *WaveInfo.pBitsPerSample != 16 )
	{
		return( FALSE );
	}

	return( TRUE );
}

/**
 * Cook a simple mono or stereo wave
 */
void CookSimpleWave( USoundNodeWave* SoundNodeWave, FConsoleSoundCooker* SoundCooker, FByteBulkData& DestinationData )
{
	WAVEFORMATEXTENSIBLE	WaveFormat = { 0 };						

	// did we lock existing bulk data?
	UBOOL bWasRawDataLocked = FALSE;

	// did we allocate some memory for PCM data?
	UBOOL bWasRawDataAlloced = FALSE;

	// do we have good wave data and WaveInfo?
	UBOOL bHasGoodWaveData = FALSE;

	// sound params
	void* SampleDataStart = NULL;
	INT SampleDataSize = 0;

	// check if there is any raw sound data
	if( SoundNodeWave->RawData.GetBulkDataSize() > 0 )
	{
		// Lock raw wave data.
		BYTE* RawWaveData = ( BYTE * )SoundNodeWave->RawData.Lock( LOCK_READ_ONLY );
		INT RawDataSize = SoundNodeWave->RawData.GetBulkDataSize();

		// mark that we locked the data
		bWasRawDataLocked = TRUE;

		FWaveModInfo WaveInfo;
	
		// parse the wave data
		if( !ReadWaveHeader( WaveInfo, RawWaveData, RawDataSize, 0 ) )
		{
			warnf( TEXT( "Only mono or stereo 16 bit waves allowed: %s (%d bytes)" ), *SoundNodeWave->GetFullName(), RawDataSize );
		}
		else
		{
			// Create wave format structure for encoder. Set up like a regular WAVEFORMATEX.
			CreateWaveFormat( WaveInfo, WaveFormat );

			// copy out some values
			SampleDataStart = WaveInfo.SampleDataStart;
			SampleDataSize = WaveInfo.SampleDataSize;

			// mark that we succeeded
			bHasGoodWaveData = TRUE;
		}
	}
	
	// if the raw data didn't exist, try to uncompress the PC compressed data
	// @todo ship: Duplicate this for surround sound!
	if( !bHasGoodWaveData )
	{
		if( SoundNodeWave->CompressedPCData.GetBulkDataSize() > 0 )
		{
			SoundNodeWave->RemoveAudioResource();
			SoundNodeWave->InitAudioResource( SoundNodeWave->CompressedPCData );

			// should not have had a valid pointer at this point
			check( !SoundNodeWave->VorbisDecompressor );
			// Create a worker to decompress the vorbis data
			FAsyncVorbisDecompress TempDecompress( SoundNodeWave );
			TempDecompress.StartSynchronousTask();

			// Mark that the data needs to be freed
			bWasRawDataAlloced = TRUE;

			// use the PCM Data as the sample data to compress
			SampleDataSize = SoundNodeWave->RawPCMDataSize;
			SampleDataStart = SoundNodeWave->RawPCMData;

			// fill out wave info
			WaveFormat.Format.nAvgBytesPerSec = SoundNodeWave->NumChannels * SoundNodeWave->SampleRate * sizeof( SWORD );
			WaveFormat.Format.nBlockAlign = SoundNodeWave->NumChannels * sizeof( SWORD );
			WaveFormat.Format.nChannels = SoundNodeWave->NumChannels;
			WaveFormat.Format.nSamplesPerSec = SoundNodeWave->SampleRate;
			WaveFormat.Format.wBitsPerSample = 16;
			WaveFormat.Format.wFormatTag = WAVE_FORMAT_PCM;

			// mark that we succeeded
			bHasGoodWaveData = TRUE;
		}
	}

	// Compress the audio if we have good data
	if( !bHasGoodWaveData )
	{
		warnf( TEXT( "Can't cook %s because there is no source compressed or uncompressed PC sound data" ), *SoundNodeWave->GetFullName() );
	}
	else
	{
		FSoundQualityInfo QualityInfo = { 0 };

		QualityInfo.Quality = SoundNodeWave->CompressionQuality;
		QualityInfo.NumChannels = WaveFormat.Format.nChannels;
		QualityInfo.SampleRate = WaveFormat.Format.nSamplesPerSec;
		QualityInfo.SampleDataSize = SampleDataSize;
		QualityInfo.bForceRealTimeDecompression = SoundNodeWave->bForceRealTimeDecompression;
		QualityInfo.bLoopingSound = SoundNodeWave->bLoopingSound;
		appStrcpy( QualityInfo.Name, 128, *SoundNodeWave->GetFullName() );

		// Cook the data.
		if( SoundCooker->Cook( ( const BYTE* )SampleDataStart, &QualityInfo ) == TRUE ) 
		{
			// Make place for cooked data.
			DestinationData.Lock( LOCK_READ_WRITE );
			BYTE* RawCompressedData = ( BYTE* )DestinationData.Realloc( SoundCooker->GetCookedDataSize() );

			// Retrieve cooked data.
			SoundCooker->GetCookedData( RawCompressedData );
			DestinationData.Unlock();

			SoundNodeWave->SampleRate = WaveFormat.Format.nSamplesPerSec;
			SoundNodeWave->NumChannels = WaveFormat.Format.nChannels;
			SoundNodeWave->RawPCMDataSize = SampleDataSize;
			SoundNodeWave->Duration = ( FLOAT )SoundNodeWave->RawPCMDataSize / ( SoundNodeWave->SampleRate * sizeof( SWORD ) * SoundNodeWave->NumChannels );
		}
		else
		{
			FString ErrorMessages = SoundCooker->GetCookErrorMessages();
			if( ErrorMessages.Len() > 0 )
			{
				warnf( NAME_Warning, TEXT( "Cooking simple sound failed: %s\nReason: %s\n" ),
					*SoundNodeWave->GetPathName(), *ErrorMessages );
			}
			else
			{
				warnf( NAME_Warning, TEXT( "Cooking simple sound failed (unknown reason): %s\n" ),
					*SoundNodeWave->GetPathName() );
			}

			// Empty data and set invalid format token.
			DestinationData.RemoveBulkData();
		}
	}

	// handle freeing/unlocking temp buffers
	if( bWasRawDataLocked )
	{
		// Unlock source as we no longer need the data
		SoundNodeWave->RawData.Unlock();
	}

	if( bWasRawDataAlloced )
	{
		// free the allocated memory
		appFree( SoundNodeWave->RawPCMData );
	}
}

/**
 * Cook a multistream (normally 5.1) wave
 */
void CookSurroundWave( USoundNodeWave* SoundNodeWave, FConsoleSoundCooker* SoundCooker, FByteBulkData& DestinationData )
{
	INT						i, ChannelCount;
	DWORD					SampleDataSize;
	FWaveModInfo			WaveInfo;
	WAVEFORMATEXTENSIBLE	WaveFormat;						
	short*					SourceBuffers[SPEAKER_Count] = { NULL };

	BYTE* RawWaveData = ( BYTE * )SoundNodeWave->RawData.Lock( LOCK_READ_ONLY );

	// Front left channel is the master
	ChannelCount = 1;
	if( ReadWaveHeader( WaveInfo, RawWaveData, SoundNodeWave->ChannelSizes( SPEAKER_FrontLeft ), SoundNodeWave->ChannelOffsets( SPEAKER_FrontLeft ) ) )
	{
		SampleDataSize = WaveInfo.SampleDataSize;
		SourceBuffers[SPEAKER_FrontLeft] = ( short* )WaveInfo.SampleDataStart;

		// Create wave format structure for encoder. Set up like a regular WAVEFORMATEX.
		CreateWaveFormat( WaveInfo, WaveFormat );

		// Extract all the info for the other channels (may be blank)
		for( i = 1; i < SPEAKER_Count; i++ )
		{
			if( ReadWaveHeader( WaveInfo, RawWaveData, SoundNodeWave->ChannelSizes( i ), SoundNodeWave->ChannelOffsets( i ) ) )
			{
				// Only mono files allowed
				if( *WaveInfo.pChannels == 1 )
				{
					SourceBuffers[i] = ( short * )WaveInfo.SampleDataStart;
					ChannelCount++;

					// Truncating to the shortest sound
					if( WaveInfo.SampleDataSize < SampleDataSize )
					{
						SampleDataSize = WaveInfo.SampleDataSize;
					}
				}
			}
		}
	
		// Only allow the formats that can be played back through
		if( ChannelCount == 4 || ChannelCount == 6 || ChannelCount == 7 || ChannelCount == 8 )
		{
			debugf( TEXT( "Cooking %d channels for: %s" ), ChannelCount, *SoundNodeWave->GetFullName() );

			FSoundQualityInfo QualityInfo = { 0 };

			QualityInfo.Quality = SoundNodeWave->CompressionQuality;
			QualityInfo.NumChannels = WaveFormat.Format.nChannels;
			QualityInfo.SampleRate = WaveFormat.Format.nSamplesPerSec;
			QualityInfo.SampleDataSize = SampleDataSize;
			QualityInfo.bForceRealTimeDecompression = SoundNodeWave->bForceRealTimeDecompression;
			QualityInfo.bLoopingSound = SoundNodeWave->bLoopingSound;
			appStrcpy( QualityInfo.Name, 128, *SoundNodeWave->GetFullName() );

			if( SoundCooker->CookSurround( ( const BYTE** )SourceBuffers, &QualityInfo ) == TRUE ) 
			{
				// Make place for cooked data.
				DestinationData.Lock( LOCK_READ_WRITE );
				BYTE* RawCompressedData = ( BYTE * )DestinationData.Realloc( SoundCooker->GetCookedDataSize() );

				// Retrieve cooked data.
				SoundCooker->GetCookedData( RawCompressedData );
				DestinationData.Unlock();

				SoundNodeWave->SampleRate = *WaveInfo.pSamplesPerSec;
				SoundNodeWave->NumChannels = ChannelCount;
				SoundNodeWave->RawPCMDataSize = SampleDataSize * ChannelCount;
				SoundNodeWave->Duration = ( FLOAT )SampleDataSize / ( SoundNodeWave->SampleRate * sizeof( SWORD ) );
			}
			else
			{
				FString ErrorMessages = SoundCooker->GetCookErrorMessages();
				if( ErrorMessages.Len() > 0 )
				{
					warnf( NAME_Warning, TEXT( "Cooking surround sound failed: %s\nReason: %s\n" ),
						*SoundNodeWave->GetPathName(), *ErrorMessages );
				}
				else
				{
					warnf( NAME_Warning, TEXT( "Cooking surround sound failed (unknown reason): %s\n" ),
						*SoundNodeWave->GetPathName() );
				}

				// Empty data and set invalid format token.
				DestinationData.RemoveBulkData();
			}
		}
		else
		{
			warnf( NAME_Warning, TEXT( "No format available for a %d channel surround sound: %s" ), ChannelCount, *SoundNodeWave->GetFullName() );
		}
	}
	else
	{
		warnf( NAME_Warning, TEXT( "Cooking surround sound failed: %s" ), *SoundNodeWave->GetPathName() );
	}

	SoundNodeWave->RawData.Unlock();
}

/**
 * Cooks SoundNodeWave to a specific platform
 *
 * @param	SoundNodeWave			Wave file to cook
 * @param	SoundCooker				Platform specific cooker object to cook with
 * @param	DestinationData			Destination bulk data
 */
UBOOL CookSoundNodeWave( USoundNodeWave* SoundNodeWave, FConsoleSoundCooker* SoundCooker, FByteBulkData& DestinationData )
{
	check( SoundCooker );

	if( DestinationData.GetBulkDataSize() > 0 && SoundNodeWave->NumChannels > 0 )
	{
		// Already cooked for this platform
		return( FALSE );
	}

	// Compress the sound using the platform specific cooker compression (if not already compressed)
	if( SoundNodeWave->ChannelSizes.Num() == 0 )
	{
		check( SoundNodeWave->ChannelOffsets.Num() == 0 );
		check( SoundNodeWave->ChannelSizes.Num() == 0 );
		CookSimpleWave( SoundNodeWave, SoundCooker, DestinationData );
	}
	else if( SoundNodeWave->ChannelSizes.Num() > 0 )
	{
		check( SoundNodeWave->ChannelOffsets.Num() == SPEAKER_Count );
		check( SoundNodeWave->ChannelSizes.Num() == SPEAKER_Count );
		CookSurroundWave( SoundNodeWave, SoundCooker, DestinationData );
	}

	return( TRUE );
}

/**
 * Cooks SoundNodeWave to all available platforms
 *
 * @param	SoundNodeWave			Wave file to cook
 * @param	Platform				Platform to cook for - PLATFORM_Unknown for all
 */
UBOOL CookSoundNodeWave( USoundNodeWave* SoundNodeWave, UE3::EPlatformType Platform )
{
	UBOOL bDirty = FALSE;

	if( Platform == UE3::PLATFORM_Unknown || Platform == UE3::PLATFORM_Xbox360 )
	{
		FConsoleSupport* XenonSupport = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport( CONSOLESUPPORT_NAME_360 );
		if( XenonSupport )
		{
			bDirty |= CookSoundNodeWave( SoundNodeWave, XenonSupport->GetGlobalSoundCooker(), SoundNodeWave->CompressedXbox360Data );
		}
	}

	if( Platform == UE3::PLATFORM_Unknown || Platform == UE3::PLATFORM_PS3 )
	{
		FConsoleSupport* PS3Support = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport( CONSOLESUPPORT_NAME_PS3 );
		if( PS3Support )
		{
			bDirty |= CookSoundNodeWave( SoundNodeWave, PS3Support->GetGlobalSoundCooker(), SoundNodeWave->CompressedPS3Data );
		}
	}

	if( Platform == UE3::PLATFORM_Unknown || Platform == UE3::PLATFORM_WiiU )
	{
		FConsoleSupport* WiiUSupport = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport( CONSOLESUPPORT_NAME_WIIU );
		if( WiiUSupport )
		{
			bDirty |= CookSoundNodeWave( SoundNodeWave, WiiUSupport->GetGlobalSoundCooker(), SoundNodeWave->CompressedWiiUData );
		}
	}

	if( Platform == UE3::PLATFORM_Unknown || Platform == UE3::PLATFORM_IPhone )
	{
		FConsoleSupport* IPhoneSupport = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport( CONSOLESUPPORT_NAME_IPHONE );
		if( IPhoneSupport )
		{
			bDirty |= CookSoundNodeWave( SoundNodeWave, IPhoneSupport->GetGlobalSoundCooker(), SoundNodeWave->CompressedIPhoneData );
		}
	}

	if( Platform == UE3::PLATFORM_Unknown || Platform == UE3::PLATFORM_Flash )
	{
		FConsoleSupport* FlashSupport = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport( CONSOLESUPPORT_NAME_FLASH );
		if( FlashSupport )
		{
			bDirty |= CookSoundNodeWave( SoundNodeWave, FlashSupport->GetGlobalSoundCooker(), SoundNodeWave->CompressedFlashData );
		}
	}

	//Platform == UE3::PLATFORM_WindowsServer intentionally skipped
	if( Platform == UE3::PLATFORM_Unknown || Platform == UE3::PLATFORM_Windows || Platform == UE3::PLATFORM_WindowsConsole || Platform == UE3::PLATFORM_MacOSX )
	{
		bDirty |= CookSoundNodeWave( SoundNodeWave, GetPCSoundCooker(), SoundNodeWave->CompressedPCData );
	}

	return( bDirty );
}

/**
 * Compresses SoundNodeWave for all available platforms, and then decompresses to PCM 
 *
 * @param	SoundNodeWave			Wave file to compress
 * @param	PreviewInfo				Compressed stats and decompressed data
 */
void SoundNodeWaveQualityPreview( USoundNodeWave* SoundNode, FPreviewInfo* PreviewInfo )
{
	FWaveModInfo WaveInfo;
	FSoundQualityInfo QualityInfo = { 0 };

	// Extract the info from the wave header
	if( !ReadWaveHeader( WaveInfo, ( BYTE* )SoundNode->ResourceData, SoundNode->ResourceSize, 0 ) )
	{
		return;
	}

	SoundNode->NumChannels = *WaveInfo.pChannels;
	SoundNode->RawPCMDataSize = WaveInfo.SampleDataSize;

	// Extract the stats
	PreviewInfo->OriginalSize = SoundNode->ResourceSize;
	PreviewInfo->OggVorbisSize = 0;
	PreviewInfo->PS3Size = 0;
	PreviewInfo->XMASize = 0;

	QualityInfo.Quality = PreviewInfo->QualitySetting;
	QualityInfo.NumChannels = *WaveInfo.pChannels;
	QualityInfo.SampleRate = SoundNode->SampleRate;
	QualityInfo.SampleDataSize = WaveInfo.SampleDataSize;
	QualityInfo.bForceRealTimeDecompression = SoundNode->bForceRealTimeDecompression;
	QualityInfo.bLoopingSound = SoundNode->bLoopingSound;
	appStrcpy( QualityInfo.Name, 128, *SoundNode->GetFullName() );

	// PCM -> Vorbis -> PCM 
	PreviewInfo->DecompressedOggVorbis = ( BYTE* )appMalloc( QualityInfo.SampleDataSize );
	PreviewInfo->OggVorbisSize = GetPCSoundCooker()->Recompress( WaveInfo.SampleDataStart, PreviewInfo->DecompressedOggVorbis, &QualityInfo );
	if( PreviewInfo->OggVorbisSize < 0 )
	{
		debugfSuppressed( NAME_DevAudio, TEXT( "PC decompression failed" ) );
		appFree( PreviewInfo->DecompressedOggVorbis );
		PreviewInfo->DecompressedOggVorbis = NULL;
		PreviewInfo->OggVorbisSize = 0;
	}
	
	FConsoleSupport* XenonSupport = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport( CONSOLESUPPORT_NAME_360 );
	if( XenonSupport )
	{
		// PCM -> XMA -> PCM 
		PreviewInfo->DecompressedXMA = ( BYTE* )appMalloc( QualityInfo.SampleDataSize );
		PreviewInfo->XMASize = XenonSupport->GetGlobalSoundCooker()->Recompress( WaveInfo.SampleDataStart, PreviewInfo->DecompressedXMA, &QualityInfo );
		if( PreviewInfo->XMASize < 0 )
		{
			debugfSuppressed( NAME_DevAudio, TEXT( "Xenon decompression failed" ) );
			appFree( PreviewInfo->DecompressedXMA );
			PreviewInfo->DecompressedXMA = NULL;
			PreviewInfo->XMASize = 0;
		}
	}

	FConsoleSupport* PS3Support = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport( CONSOLESUPPORT_NAME_PS3 );
	if( PS3Support )
	{
		// PCM -> PS3 -> PCM 
		PreviewInfo->DecompressedPS3 = ( BYTE* )appMalloc( QualityInfo.SampleDataSize );
		PreviewInfo->PS3Size = PS3Support->GetGlobalSoundCooker()->Recompress( WaveInfo.SampleDataStart, PreviewInfo->DecompressedPS3, &QualityInfo );
		if( PreviewInfo->PS3Size < 0 )
		{
			debugfSuppressed( NAME_DevAudio, TEXT( "PS3 decompression failed" ) );
			appFree( PreviewInfo->DecompressedPS3 );
			PreviewInfo->DecompressedPS3 = NULL;
			PreviewInfo->PS3Size = 0;
		}
	}
}

/** 
 * Makes sure ogg vorbis data is available for this sound node by converting on demand
 */
UBOOL USoundNodeWave::ValidateData( void )
{
	return( CookSoundNodeWave( this, GetPCSoundCooker(), CompressedPCData ) );
}

/** 
 * Makes sure ogg vorbis data is available for all sound nodes in this cue by converting on demand
 */
UBOOL USoundCue::ValidateData( void )
{
	TArray<USoundNodeWave*> Waves;
	RecursiveFindNode<USoundNodeWave>( FirstNode, Waves );

	UBOOL Converted = FALSE;
	for( INT WaveIndex = 0; WaveIndex < Waves.Num(); ++WaveIndex )
	{
		USoundNodeWave* Sound = Waves( WaveIndex );
		if( !Sound->bUseTTS )
		{
			Converted |= Sound->ValidateData();
		}
	}

	return( Converted );
}

/**
 *	Check the InCmdParams for "MAPINISECTION=<name of section>".
 *	If found, fill in OutMapList with the proper map names.
 *
 *	@param	InCmdParams		The cmd line parameters for the application
 *	@param	OutMapList		The list of maps from the ini section, empty if not found
 */
void UEditorEngine::ParseMapSectionIni(const TCHAR* InCmdParams, TArray<FString>& OutMapList)
{
	FString SectionStr;
	if (Parse(InCmdParams, TEXT("MAPINISECTION="), SectionStr))
	{
		if (SectionStr.InStr(TEXT("+")))
		{
			TArray<FString> Sections;
			SectionStr.ParseIntoArray(&Sections,TEXT("+"),TRUE);
			for (INT Index = 0; Index < Sections.Num(); Index++)
			{
				LoadMapListFromIni(Sections(Index), OutMapList);
			}
		}
		else
		{
			LoadMapListFromIni(SectionStr, OutMapList);
		}
	}
}

/**
 *	Load the list of maps from the given section of the Editor.ini file
 *	Supports sections contains other sections - but DOES NOT HANDLE CIRCULAR REFERENCES!!!
 *
 *	@param	InSectionName		The name of the section to load
 *	@param	OutMapList			The list of maps from that section
 */
void UEditorEngine::LoadMapListFromIni(const FString& InSectionName, TArray<FString>& OutMapList)
{
	// 
	FConfigSection* MapListList = GConfig->GetSectionPrivate(*InSectionName, FALSE, TRUE, GEditorIni);
	if (MapListList)
	{
		for (FConfigSectionMap::TConstIterator It(*MapListList) ; It ; ++It)
		{
			FName EntryType = It.Key();
			const FString& EntryValue = It.Value();

			if (EntryType == NAME_Map)
			{
				// Add it to the list
				OutMapList.AddUniqueItem(EntryValue);
			}
			else if (EntryType == FName(TEXT("Section")))
			{
				// Recurse...
				LoadMapListFromIni(EntryValue, OutMapList);
			}
			else
			{
				warnf(NAME_Warning, TEXT("Invalid entry in map ini list: %s, %s=%s"),
					*InSectionName, *(EntryType.ToString()), *EntryValue);
			}
		}
	}
}
