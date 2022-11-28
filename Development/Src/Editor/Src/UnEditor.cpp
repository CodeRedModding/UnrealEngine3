/*=============================================================================
	UnEditor.cpp: Unreal editor main file
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

class UEditorEngine* GEditor;

/*-----------------------------------------------------------------------------
	UEditorEngine.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UEditorEngine);

/*-----------------------------------------------------------------------------
	Init & Exit.
-----------------------------------------------------------------------------*/

//
// Construct the UEditorEngine class.
//
void UEditorEngine::StaticConstructor()
{
							new(GetClass(), TEXT("EditPackagesInPath"),		RF_Public) UStrProperty		( CPP_PROPERTY(EditPackagesInPath),		TEXT("Advanced"), CPF_Config );
							new(GetClass(), TEXT("EditPackagesOutPath"),	RF_Public) UStrProperty		( CPP_PROPERTY(EditPackagesOutPath),	TEXT("Advanced"), CPF_Config );
	UArrayProperty* A	=	new(GetClass(), TEXT("EditPackages"),			RF_Public) UArrayProperty	( CPP_PROPERTY(EditPackages),			TEXT("Advanced"), CPF_Config );
	A->Inner			=	new(A,			TEXT("StrProperty0"),			RF_Public) UStrProperty;
}

//
// Construct the editor.
//
UEditorEngine::UEditorEngine()
:	EditPackages( E_NoInit )
{}

//
// Editor early startup.
//
void UEditorEngine::InitEditor()
{
	// Call base.
	UEngine::Init();

	// Topics.
	GTopics.Init();

	// Make sure properties match up.
	VERIFY_CLASS_OFFSET(A,Actor,Owner);

	// Allocate temporary model.
	TempModel = new UModel( NULL, 1 );

	// Settings.
	MovementSpeed	= 4.0;
	FastRebuild		= 0;
	Bootstrapping	= 0;
}

UBOOL UEditorEngine::ShouldDrawBrushWireframe( AActor* InActor )
{
	return GEditorModeTools.GetCurrentMode()->ShouldDrawBrushWireframe( InActor );
}

// Used for sorting ActorFactory classes.
IMPLEMENT_COMPARE_POINTER( UActorFactory, UnEditor, { return B->MenuPriority - A->MenuPriority; } )

//
// Init the editor.
//
void UEditorEngine::Init()
{
	// Init editor.
	GEditor = this;
	InitEditor();

	// Init transactioning.
	Trans = CreateTrans();

	// Load classes for editing.
	BeginLoad();
	for( INT i=0; i<EditPackages.Num(); i++ )
		if( !LoadPackage( NULL, *EditPackages(i), LOAD_NoWarn ) )
				appErrorf( TEXT("Can't find edit package '%s'"), *EditPackages(i) );
	EndLoad();

	// Init the client.
	UClass* ClientClass = StaticLoadClass( UClient::StaticClass(), NULL, TEXT("engine-ini:Editor.EditorEngine.Client"), NULL, LOAD_NoFail, NULL );
	Client = (UClient*)StaticConstructObject( ClientClass );
	Client->Init( this );
	check(Client);

	// Verify classes.
	UBOOL Mismatch = false;
	#define VERIFY_CLASS_SIZES
	#define NAMES_ONLY
	#define AUTOGENERATE_NAME(name)
	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "EditorClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef AUTOGENERATE_NAME
	#undef NAMES_ONLY
	#undef VERIFY_CLASS_SIZES
	if( Mismatch )
		appErrorf(TEXT("C++/ Script class size mismatch"));

	VERIFY_CLASS_OFFSET(U,EditorEngine,ParentContext);

	InitGameRBPhys();

	// Info.
	UPackage* LevelPkg = CreatePackage( NULL, TEXT("MyLevel") );
	Level = new( LevelPkg, TEXT("MyLevel") )ULevel( this, 1 );

	// Objects.
	Results  = new( GetTransientPackage(), TEXT("Results") )UTextBuffer;

	// Create array of ActorFactory instances.
	for(TObjectIterator<UClass> It; It; ++It)
	{
		if(It->IsChildOf(UActorFactory::StaticClass()) &&
		   !(It->ClassFlags & CLASS_Abstract) &&
		   ((UActorFactory*)It->GetDefaultObject())->bPlaceable)
		{
			UClass* ActorFactoryClass = *It;
			UActorFactory* NewFactory = ConstructObject<UActorFactory>( ActorFactoryClass );
			ActorFactories.AddItem(NewFactory);
		}
	}

	// Sort by menu priority.
	Sort<USE_COMPARE_POINTER(UActorFactory,UnEditor)>( &ActorFactories(0), ActorFactories.Num() );

	// Purge garbage.
	Cleanse( 0, TEXT("startup") );

	// Subsystem init messsage.
	debugf( NAME_Init, TEXT("Editor engine initialized") );
};
void UEditorEngine::Destroy()
{
	if(Level)
	{
		ClearComponents();
		Level->CleanupLevel();
		Level = NULL;
	}

	// Shut down transaction tracking system.
	if( Trans )
	{
		if( GUndo )
			debugf( NAME_Warning, TEXT("Warning: A transaction is active") );
		Trans->Reset( TEXT("shutdown") );
	}

	// Topics.
	GTopics.Exit();

	// Remove editor array from root.
	debugf( NAME_Exit, TEXT("Editor shut down") );

	Super::Destroy();
}
void UEditorEngine::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	Ar << Tools;
	if(!Ar.IsLoading() && !Ar.IsSaving())
	{
		// Serialize viewport clients.

		for(UINT ViewportIndex = 0;ViewportIndex < (UINT)ViewportClients.Num();ViewportIndex++)
			Ar << *ViewportClients(ViewportIndex);

		// Serialize ActorFactories
		Ar << ActorFactories;

		// Serialize components used in UnrealEd modes

		GEditorModeTools.Serialize( Ar );
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
	// Tick client code.
	if( Client )
		Client->Tick(DeltaSeconds);

	// Clean up the viewports that have been closed.
	for(FPlayerIterator It(this);It;++It)
		if(!It->Viewport)
			It.RemoveCurrent();

	// If all viewports closed, close the current play level.
	if(!Players.Num() && PlayLevel)
		EndPlayMap();

	// Update subsystems.
	StaticTick();

	// Look for realtime flags.
	UBOOL IsRealtime		= false;
	UBOOL IsRealtimeAudio	= false;
	for( INT i=0; i<ViewportClients.Num(); i++ )
	{
		// If this viewport is onto the main, edited level (not a level inside a tool like PhAT or Cascade).
		if(ViewportClients(i)->GetLevel() == GEditor->Level)
		{
			if( ViewportClients(i)->Realtime )
			{
				IsRealtime = true;
			}
			if( ViewportClients(i)->RealtimeAudio )
			{
				IsRealtimeAudio = true;
			}
		}
	}

	// Tick level.
	if( Level )
	{
		Level->Tick( IsRealtime ? LEVELTICK_ViewportsOnly : LEVELTICK_TimeOnly, DeltaSeconds );
	}

	if(PlayLevel)
	{
		// Release mouse if the game is paused. The low level input code might ignore the request when e.g. in fullscreen mode.
		for( INT i=0; i<Players.Num(); i++ )
			if( Players(i)->MasterViewport )
				Players(i)->MasterViewport->CaptureMouseInput( !PlayLevel->IsPaused() );

		// Update the level.
		GameCycles=0;
		clock(GameCycles);
		if( PlayLevel )
		{
			// Decide whether to drop high detail because of frame rate
			if ( Client )
			{
				PlayLevel->GetLevelInfo()->bDropDetail = (DeltaSeconds > 1.f/Clamp(Client->MinDesiredFrameRate,1.f,100.f)) && !GIsBenchmarking;
				PlayLevel->GetLevelInfo()->bAggressiveLOD = (DeltaSeconds > 1.f/Clamp(Client->MinDesiredFrameRate - 5.f,1.f,100.f)) && !GIsBenchmarking;
			}
			// tick the level
			PlayLevel->Tick( LEVELTICK_All, DeltaSeconds );
		}
		unclock(GameCycles);
	}

	// Update Audio.
	if( Client && Client->GetAudioDevice() )
		Client->GetAudioDevice()->Update( IsRealtimeAudio || (PlayLevel && !PlayLevel->IsPaused()) );

	if(PlayLevel)
	{
		// Render everything.
		for(FPlayerIterator It(this);It;++It)
			It->Viewport->Draw(1);
	}

	// Update viewports.
	for(INT ViewportIndex = 0;ViewportIndex < ViewportClients.Num();ViewportIndex++)
	{
		ViewportClients(ViewportIndex)->Tick(DeltaSeconds);
		if(ViewportClients(ViewportIndex)->Realtime || ViewportClients(ViewportIndex)->Redraw)
		{
			ViewportClients(ViewportIndex)->Viewport->Draw(1);
			ViewportClients(ViewportIndex)->Redraw = 0;
		}
	}
}

/*-----------------------------------------------------------------------------
	Garbage collection.
-----------------------------------------------------------------------------*/

//
// Clean up after a major event like loading a file.
//
void UEditorEngine::Cleanse( UBOOL Redraw, const TCHAR* TransReset )
{
	check(TransReset);
	if( GIsRunning && !Bootstrapping )
	{
		// Collect garbage.
		CollectGarbage( RF_Native | RF_Standalone );

		// Reset the transaction tracking system if desired.
		Trans->Reset( TransReset );

		// Flush the cache.
		Flush();

		// Redraw the levels.
		if( Redraw )
			RedrawLevel( Level );

		// Entirely load packages.
		UObject::LoadEverything();
	}
}

/*---------------------------------------------------------------------------------------
	Topics.
---------------------------------------------------------------------------------------*/
void UEditorEngine::Get( const TCHAR* Topic, const TCHAR* Item, FOutputDevice& Ar )
{
	GTopics.Get( Level, Topic, Item, Ar );
}
void UEditorEngine::Set( const TCHAR* Topic, const TCHAR* Item, const TCHAR* Value )
{
	GTopics.Set( Level, Topic, Item, Value );
}

/*---------------------------------------------------------------------------------------
	Misc.
---------------------------------------------------------------------------------------*/

void UEditorEngine::ClearComponents()
{
	check(Level);

	FEdMode* CurrentMode = GEditorModeTools.GetCurrentMode();
	if(CurrentMode)
	{
		CurrentMode->ClearComponent();
	}

	Level->ClearComponents();
}

void UEditorEngine::UpdateComponents()
{
	check(Level);

	FEdMode* CurrentMode = GEditorModeTools.GetCurrentMode();
	if(CurrentMode)
	{
		CurrentMode->UpdateComponent();
	}

	Level->UpdateComponents();
}

/*---------------------------------------------------------------------------------------
	The End.
---------------------------------------------------------------------------------------*/

