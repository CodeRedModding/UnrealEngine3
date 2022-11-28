/*=============================================================================
	UnGame.cpp: Unreal game engine.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "..\Debugger\UnDebuggerCore.h"

//
//	Object class implementation.
//

IMPLEMENT_CLASS(UGameEngine);

/*-----------------------------------------------------------------------------
	cleanup!!
-----------------------------------------------------------------------------*/

void UGameEngine::PaintProgress()
{
	for(FPlayerIterator It(Cast<UGameEngine>(GEngine));It;++It)
		It->Viewport->Draw(1);

}

INT UGameEngine::ChallengeResponse( INT Challenge )
{
	return (Challenge*237) ^ (0x93fe92Ce) ^ (Challenge>>16) ^ (Challenge<<16);
}

void UGameEngine::UpdateConnectingMessage()
{
	if(GPendingLevel && Players.Num() && Players(0)->Actor)
	{
		if(Players(0)->Actor->ProgressTimeOut < Players(0)->Actor->Level->TimeSeconds)
		{
			TCHAR Msg1[256], Msg2[256];
			appSprintf( Msg1, *LocalizeProgress(TEXT("ConnectingText"),TEXT("Engine")) );
			appSprintf( Msg2, *LocalizeProgress(TEXT("ConnectingURL"),TEXT("Engine")), *GPendingLevel->URL.Host, *GPendingLevel->URL.Map );
			SetProgress( Msg1, Msg2, 60.f );
		}
	}
}
void UGameEngine::BuildServerMasterMap( UNetDriver* NetDriver, ULevel* InLevel )
{
	check(NetDriver);
	check(InLevel);
	BeginLoad();
	{
		// Init LinkerMap.
		check(InLevel->GetLinker());
		NetDriver->MasterMap->AddLinker( InLevel->GetLinker() );

		// Load server-required packages.
		for( INT i=0; i<ServerPackages.Num(); i++ )
		{
			debugf( NAME_DevNet, TEXT("Server Package: %s"), *ServerPackages(i) );
			ULinkerLoad* Linker = GetPackageLinker( NULL, *ServerPackages(i), LOAD_NoFail, NULL, NULL );
			if( NetDriver->MasterMap->AddLinker( Linker )==INDEX_NONE )
				debugf( NAME_DevNet, TEXT("   (server-side only)") );
		}

		// Add GameInfo's package to map.
		check(InLevel->GetLevelInfo());
		check(InLevel->GetLevelInfo()->Game);
		check(InLevel->GetLevelInfo()->Game->GetClass()->GetLinker());
		NetDriver->MasterMap->AddLinker( InLevel->GetLevelInfo()->Game->GetClass()->GetLinker() );

		// Precompute linker info.
		NetDriver->MasterMap->Compute();
	}
	EndLoad();
}

/*-----------------------------------------------------------------------------
	Game init and exit.
-----------------------------------------------------------------------------*/

//
// Construct the game engine.
//
UGameEngine::UGameEngine()
: LastURL(TEXT(""))
{}

//
// Initialize the game engine.
//
void UGameEngine::Init()
{
	check(sizeof(*this)==GetClass()->GetPropertiesSize());

	// Call base.
	UEngine::Init();

	// Init variables.
	GLevel = NULL;

	// Delete temporary files in cache.
	appCleanFileCache();

	// If not a dedicated server.
	if( GIsClient )
	{	
		// Init client.
		UClass* ClientClass = StaticLoadClass( UClient::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.Client"), NULL, LOAD_NoFail, NULL );
		Client = ConstructObject<UClient>( ClientClass );
		Client->Init( this );
	}

	InitGameRBPhys();

	// Load the entry level.
	FString Error;
	
	// For Later
	if( Client )
	{
		if( !LoadMap( FURL(TEXT("Entry")), NULL, NULL, Error ) )
			appErrorf( *LocalizeError(TEXT("FailedBrowse"),TEXT("Engine")), TEXT("Entry"), *Error );
		Exchange( GLevel, GEntry );
	}

	// Create default URL.
	FURL DefaultURL;
	DefaultURL.LoadURLConfig( TEXT("DefaultPlayer"), TEXT("User") );

	// Create the local player.

	ULocalPlayer*	LocalPlayer = new(this) ULocalPlayer;
	Players.AddItem(LocalPlayer);

	// Enter initial world.
	TCHAR Parm[4096]=TEXT("");
	const TCHAR* Tmp = appCmdLine();
	if(	!ParseToken( Tmp, Parm, ARRAY_COUNT(Parm), 0 ) || Parm[0]=='-' )
		appStrcpy( Parm, *FURL::DefaultLocalMap );
	FURL URL( &DefaultURL, Parm, TRAVEL_Partial );
	if( !URL.Valid )
		appErrorf( *LocalizeError(TEXT("InvalidUrl"),TEXT("Engine")), Parm );
	UBOOL Success = Browse( URL, NULL, Error );

	// If waiting for a network connection, go into the starting level.
	if( !Success && Error==TEXT("") && appStricmp( Parm, *FURL::DefaultLocalMap )!=0 )
		Success = Browse( FURL(&DefaultURL,*FURL::DefaultLocalMap,TRAVEL_Partial), NULL, Error );

	// Handle failure.
	if( !Success )
		appErrorf( *LocalizeError(TEXT("FailedBrowse"),TEXT("Engine")), Parm, *Error );

	// Open initial Viewport.
	if( Client )
	{
		// Create a viewport for the local player.
		LocalPlayer->Viewport = LocalPlayer->MasterViewport = Client->CreateViewport(
			LocalPlayer,
			*LocalizeGeneral("Product",GPackage),
			Client->StartupResolutionX,
			Client->StartupResolutionY,
			Client->StartupFullscreen && !ParseParam(appCmdLine(),TEXT("WINDOWED"))
			);
		LocalPlayer->Viewport->CaptureMouseInput(1);
	}

	debugf( NAME_Init, TEXT("Game engine initialized") );

}

//
// Game exit.
//
void UGameEngine::Destroy()
{
	// Game exit.
	if( GPendingLevel )
		CancelPending();

	if(GEntry && GEntry != GLevel)
	{
		GEntry->CleanupLevel();
	}
	GEntry = NULL;

	if(GLevel)
	{
		// Clean up the current level's components.
		GLevel->CleanupLevel();
		GLevel = NULL;
	}

	debugf( NAME_Exit, TEXT("Game engine shut down") );

	// Cleanup players.
	for(INT PlayerIndex = 0;PlayerIndex < Players.Num();PlayerIndex++)
		delete Players(PlayerIndex);
	Players.Empty();

	// Close client.
	if(Client)
	{
		delete Client;
		Client = NULL;
	}

	Super::Destroy();
}

//
// Progress text.
//
void UGameEngine::SetProgress( const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds )
{
	if( Players.Num() && Players(0)->Actor )
	{
/* FIXMESTEVE
		if( Seconds==-1.f )
		{
			// Upgrade message.
			if ( Players(0)->Actor->myHUD )
				Players(0)->Actor->myHUD->eventShowUpgradeMenu();
		}
*/
		Players(0)->Actor->eventSetProgressMessage(0, Str1, FColor(255,255,255));
		Players(0)->Actor->eventSetProgressMessage(1, Str2, FColor(255,255,255));
		Players(0)->Actor->eventSetProgressTime(Seconds);
	}
}

/*-----------------------------------------------------------------------------
	Command line executor.
-----------------------------------------------------------------------------*/

//
// This always going to be the last exec handler in the chain. It
// handles passing the command to all other global handlers.
//
UBOOL UGameEngine::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	const TCHAR* Str=Cmd;
	if( ParseCommand( &Str, TEXT("OPEN") ) )
	{
		SetClientTravel( Str, 0, TRAVEL_Partial );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("START") ) )
	{
		SetClientTravel( Str, 0, TRAVEL_Absolute );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("SERVERTRAVEL") ) && (GIsServer && !GIsClient) )
	{
		GLevel->GetLevelInfo()->eventServerTravel(Str,0);
		return 1;
	}
	else if( (GIsServer && !GIsClient) && ParseCommand( &Str, TEXT("SAY") ) )
	{
		GLevel->GetLevelInfo()->Game->eventBroadcast(NULL, Str, NAME_None);
		return 1;
	}
	else if( ParseCommand(&Str, TEXT("DISCONNECT")) )
	{
		// Remove ?Listen parameter, if it exists
		// FIXMESTEVE LastURL.RemoveOption( TEXT("Listen") );
		//LastURL.RemoveOption( TEXT("LAN") ) ;

		if( GLevel && GLevel->NetDriver && GLevel->NetDriver->ServerConnection && GLevel->NetDriver->ServerConnection->Channels[0] )
		{
			GLevel->NetDriver->ServerConnection->Channels[0]->Close();
			GLevel->NetDriver->ServerConnection->FlushNet();
		}
		if( GPendingLevel && GPendingLevel->NetDriver && GPendingLevel->NetDriver->ServerConnection && GPendingLevel->NetDriver->ServerConnection->Channels[0] )
		{
			GPendingLevel->NetDriver->ServerConnection->Channels[0]->Close();
			GPendingLevel->NetDriver->ServerConnection->FlushNet();
		}
		SetClientTravel( TEXT("?failed"), 0, TRAVEL_Absolute );

		return 1;
	}
	else if( ParseCommand(&Str, TEXT("RECONNECT")) )
	{
		SetClientTravel( *LastURL.String(), 0, TRAVEL_Absolute );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("EXIT")) || ParseCommand(&Cmd,TEXT("QUIT")))
	{
		if( GLevel && GLevel->NetDriver && GLevel->NetDriver->ServerConnection && GLevel->NetDriver->ServerConnection->Channels[0] )
		{
			GLevel->NetDriver->ServerConnection->Channels[0]->Close();
			GLevel->NetDriver->ServerConnection->FlushNet();
		}
		if( GPendingLevel && GPendingLevel->NetDriver && GPendingLevel->NetDriver->ServerConnection && GPendingLevel->NetDriver->ServerConnection->Channels[0] )
		{
			GPendingLevel->NetDriver->ServerConnection->Channels[0]->Close();
			GPendingLevel->NetDriver->ServerConnection->FlushNet();
		}
		Ar.Log( TEXT("Closing by request") );
		appRequestExit( 0 );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("GETMAXTICKRATE") ) )
	{
		Ar.Logf( TEXT("%f"), GetMaxTickRate() );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SAVEGAME")) )
	{
		if( appIsDigit(Str[0]) )
			SaveGame( appAtoi(Str) );
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("CANCEL") ) )
	{
		static UBOOL InCancel = 0;
		if( !InCancel )	
		{
			//!!Hack for broken Input subsystem.  JP.
			//!!Inside LoadMap(), ResetInput() is called,
			//!!which can retrigger an Exec call.
			InCancel = 1;
			if( GPendingLevel )
			{
				if( GPendingLevel->TrySkipFile() )
				{
					InCancel = 0;
					return 1;
				}
				SetProgress( *LocalizeProgress(TEXT("CancelledConnect"),TEXT("Engine")), TEXT(""), 2.f );
			}
			else
				SetProgress( TEXT(""), TEXT(""), 0.f );
			CancelPending();
			InCancel = 0;
		}
		return 1;
	}
#ifndef XBOX
	else if( ParseCommand( &Cmd, TEXT("TOGGLEDEBUGGER") ) )
	{
		if( GDebugger )
		{
			delete GDebugger;
			GDebugger = NULL;
		}
		else
		{
			GDebugger = new UDebuggerCore();
		}
		return 1;
	}
#endif
	else if( GLevel && GLevel->Exec( Cmd, Ar ) )
	{
		return 1;
	}
	else if( GLevel && GLevel->GetLevelInfo()->Game && GLevel->GetLevelInfo()->Game->ScriptConsoleExec(Cmd,Ar,NULL) )
	{
		return 1;
	}
	else
	{
		UBOOL bAllowCmd = true;
		if ( GLevel && (GLevel->GetLevelInfo()->NetMode == NM_Client) )
		{
			// disallow set of actor properties if network game
			if ( ParseCommand(&Str,TEXT("SET")) )
			{
				const TCHAR *Str = Cmd;
				TCHAR ClassName[256];
				UClass* Class;
				if(	ParseToken( Str, ClassName, ARRAY_COUNT(ClassName), 1 )
					&& ((Class=FindObject<UClass>( ANY_PACKAGE, ClassName))!=NULL)
					&& Class->IsChildOf(AActor::StaticClass())
					&& !Class->IsChildOf(AGameInfo::StaticClass()) )
				{
					bAllowCmd = false;
				}
			}
		}
		if( bAllowCmd && UEngine::Exec( Cmd, Ar ) )
			return 1;
		else
			return 0;
	}
}

/*-----------------------------------------------------------------------------
	Game entering.
-----------------------------------------------------------------------------*/

//
// Cancel pending level.
//
void UGameEngine::CancelPending()
{
	if( GPendingLevel )
	{
		if( GPendingLevel->NetDriver && GPendingLevel->NetDriver->ServerConnection && GPendingLevel->NetDriver->ServerConnection->Channels[0] )
		{
			GPendingLevel->NetDriver->ServerConnection->Channels[0]->Close();
			GPendingLevel->NetDriver->ServerConnection->FlushNet();
		}
		delete GPendingLevel;
		GPendingLevel = NULL;
	}
}

//
// Match Viewports to actors.
//
static void MatchViewportsToActors(UGameEngine* Engine,ULevel* Level,const FURL& URL)
{
	for(FPlayerIterator It(Engine);It;++It)
	{
		FString Error;
		It->Actor = Level->SpawnPlayActor(*It,ROLE_SimulatedProxy,URL,Error);
		if(!It->Actor)
			appErrorf(TEXT("%s"),*Error);
	}
}

//
// Browse to a specified URL, relative to the current one.
//
UBOOL UGameEngine::Browse( FURL URL, const TMap<FString,FString>* TravelInfo, FString& Error )
{
	Error = TEXT("");
	const TCHAR* Option;

	// Convert .unreal link files.
	const TCHAR* LinkStr = TEXT(".unreal");//!!
	if( appStrstr(*URL.Map,LinkStr)-*URL.Map==appStrlen(*URL.Map)-appStrlen(LinkStr) )
	{
		debugf( TEXT("Link: %s"), *URL.Map );
		FString NewUrlString;
		if( GConfig->GetString( TEXT("Link")/*!!*/, TEXT("Server"), NewUrlString, *URL.Map ) )
		{
			// Go to link.
			URL = FURL( NULL, *NewUrlString, TRAVEL_Absolute );//!!
		}
		else
		{
			// Invalid link.
			Error = FString::Printf( *LocalizeError(TEXT("InvalidLink"),TEXT("Engine")), *URL.Map );
			return 0;
		}
	}

	// Crack the URL.
	debugf( TEXT("Browse: %s"), *URL.String() );

	// Handle it.
	if( !URL.Valid )
	{
		// Unknown URL.
		Error = FString::Printf( *LocalizeError(TEXT("InvalidUrl"),TEXT("Engine")), *URL.String() );
		return 0;
	}
	else if( URL.HasOption(TEXT("failed")) || URL.HasOption(TEXT("entry")) )
	{
		// Handle failure URL.
		debugf( NAME_Log, *LocalizeError(TEXT("AbortToEntry"),TEXT("Engine")) );
		if( GLevel && GLevel!=GEntry )
			ResetLoaders( GLevel->GetOuter(), 1, 0 );
		NotifyLevelChange();
		GLevel = GEntry;
		GLevel->GetLevelInfo()->LevelAction = LEVACT_None;
		//CollectGarbage( RF_Native ); // Causes texture corruption unless you flush.
		if( URL.HasOption(TEXT("failed")) )
		{
			if( !GPendingLevel )
				SetProgress( *LocalizeError(TEXT("ConnectionFailed"),TEXT("Engine")), TEXT(""), 6.f );
		}
		return 1;
	}
	else if( URL.HasOption(TEXT("restart")) )
	{
		// Handle restarting.
		URL = LastURL;
	}
	else if( (Option=URL.GetOption(TEXT("load="),NULL))!=NULL )
	{
		// Handle loadgame.
		FString Error, Temp=FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("Save%i.usa?load"), *GSys->SavePath, appAtoi(Option) );
		debugf(TEXT("Loading save game %s"),*Temp);
		if( LoadMap(FURL(&LastURL,*Temp,TRAVEL_Partial),NULL,NULL,Error) )
		{
			LastURL = GLevel->URL;
			return 1;
		}
		else return 0;
	}

	// Handle normal URL's.
	if( URL.IsLocalInternal() )
	{
		// Local map file.
		UBOOL MapRet = LoadMap( URL, NULL, TravelInfo, Error )!=NULL;
		return MapRet;
	}
	else if( URL.IsInternal() && GIsClient )
	{
		// Network URL.
		if( GPendingLevel )
			CancelPending();
		GPendingLevel = new UNetPendingLevel( this, URL );
		if( !GPendingLevel->NetDriver )
		{
			SetProgress( TEXT("Networking Failed"), *GPendingLevel->Error, 6.f );
			delete GPendingLevel;
			GPendingLevel = NULL;
		}
		return 0;
	}
	else if( URL.IsInternal() )
	{
		// Invalid.
		Error = LocalizeError(TEXT("ServerOpen"),TEXT("Engine"));
		return 0;
	}
	else
	{
		// External URL.
		//Client->Viewports(0)->Exec(TEXT("ENDFULLSCREEN"));
		appLaunchURL( *URL.String(), TEXT(""), &Error );
		return 0;
	}
}

//
// Notify that level is changing
//
void UGameEngine::NotifyLevelChange()
{
}

//
// Load a map.
//
ULevel* UGameEngine::LoadMap( const FURL& URL, UPendingLevel* Pending, const TMap<FString,FString>* TravelInfo, FString& Error )
{
	DOUBLE	StartTime = appSeconds();

	Error = TEXT("");
	debugf( NAME_Log, TEXT("LoadMap: %s"), *URL.String() );
	GInitRunaway();

	// Get network package map.
	UPackageMap* PackageMap = NULL;
	if( Pending )
		PackageMap = Pending->GetDriver()->ServerConnection->PackageMap;

	// Verify that we can load all packages we need.
	UObject* MapParent = NULL;
	try
	{
		BeginLoad();
		if( Pending )
		{
			// Verify that we can load everything needed for client in this network level.
			for( INT i=0; i<PackageMap->List.Num(); i++ )
				PackageMap->List(i).Linker = GetPackageLinker
				(
					PackageMap->List(i).Parent,
					NULL,
					LOAD_Verify | LOAD_Throw | LOAD_NoWarn | LOAD_NoVerify,
					NULL,
					&PackageMap->List(i).Guid
				);
			for( INT i=0; i<PackageMap->List.Num(); i++ )
				VerifyLinker( PackageMap->List(i).Linker );
			if( PackageMap->List.Num() )
				MapParent = PackageMap->List(0).Parent;
		}
		LoadObject<ULevel>( MapParent, TEXT("MyLevel"), *URL.Map, LOAD_Verify | LOAD_Throw | LOAD_NoWarn, NULL );
		EndLoad();
	}
	#if UNICODE
	catch( TCHAR* CatchError )
	#else
	catch( char* CatchError )
	#endif
	{
		// Safely failed loading.
		EndLoad();
		Error = CatchError;
        #if UNICODE
        TCHAR *e = CatchError;
        #else
        TCHAR *e = ANSI_TO_TCHAR(CatchError);
        #endif
		SetProgress( *LocalizeError(TEXT("UrlFailed"),TEXT("Core")), e, 6.f );
		return NULL;
	}
	// Display loading screen.
	if( GLevel && !URL.HasOption(TEXT("quiet")) )
	{
		GLevel->GetLevelInfo()->LevelAction = LEVACT_Loading;
		GLevel->GetLevelInfo()->Pauser = NULL;
		PaintProgress();
		GLevel->GetLevelInfo()->LevelAction = LEVACT_None;
	}
	// Notify of the level change, before we dissociate Viewport actors
	if( GLevel )
		NotifyLevelChange();
	// Clean up game state.
	if( GLevel )
	{
		// Shut down.
		ResetLoaders( GLevel->GetOuter(), 1, 0 );
		if( GLevel->NetDriver )
		{
			delete GLevel->NetDriver;
			GLevel->NetDriver = NULL;
		}
		GLevel->CleanupLevel();
		GLevel = NULL;
	}
	// Dissociate Viewport actors.
	for(FPlayerIterator It(this);It;++It)
	{
		if(It->Actor)
		{
			ULevel*	Level = It->Actor->GetLevel();
			if(It->Actor->Pawn)
				Level->DestroyActor(It->Actor->Pawn);
			Level->DestroyActor(It->Actor);
			It->Actor = NULL;
		}
	}
	// Load the level and all objects under it, using the proper Guid.
	GLevel = LoadObject<ULevel>( MapParent, TEXT("MyLevel"), *URL.Map, LOAD_NoFail, NULL );
	// If pending network level.
	if( Pending )
	{
		// If playing this network level alone, ditch the pending level.
		if( Pending && Pending->LonePlayer )
			Pending = NULL;

		// Setup network package info.
		PackageMap->Compute();
		for( INT i=0; i<PackageMap->List.Num(); i++ )
			if( PackageMap->List(i).LocalGeneration!=PackageMap->List(i).RemoteGeneration )
				Pending->GetDriver()->ServerConnection->Logf( TEXT("HAVE GUID=%s GEN=%i"), *PackageMap->List(i).Guid.String(), PackageMap->List(i).LocalGeneration );
	}

	VERIFY_CLASS_OFFSET( A, Actor, Owner );
	VERIFY_CLASS_OFFSET( A, PlayerController,	ViewTarget );
	VERIFY_CLASS_OFFSET( A, Pawn, Health );
    VERIFY_CLASS_SIZE( UEngine );
    VERIFY_CLASS_SIZE( UGameEngine );
	
	VERIFY_CLASS_SIZE( UTexture );
	VERIFY_CLASS_OFFSET( U, Texture, UnpackMax );

	VERIFY_CLASS_SIZE( UTexture2D );
	VERIFY_CLASS_OFFSET( U, Texture2D, ResourceIndex );
	VERIFY_CLASS_OFFSET( U, Texture2D, Dynamic );
	VERIFY_CLASS_OFFSET( U, Texture2D, SizeX );
	VERIFY_CLASS_OFFSET( U, Texture2D, SizeY );
	VERIFY_CLASS_OFFSET( U, Texture2D, Format );
	VERIFY_CLASS_OFFSET( U, Texture2D, NumMips );
	VERIFY_CLASS_OFFSET( U, Texture2D, Mips );
	VERIFY_CLASS_OFFSET( U, Texture2D, SourceArt );

	VERIFY_CLASS_SIZE( UTextureCube );

	VERIFY_CLASS_SIZE( USkeletalMeshComponent );
	VERIFY_CLASS_OFFSET( U, SkeletalMeshComponent, SkeletalMesh );

	VERIFY_CLASS_SIZE( USkeletalMesh );
	VERIFY_CLASS_OFFSET( U, SkeletalMesh, RefBasesInvMatrix );

	VERIFY_CLASS_SIZE(UPlayer);
	VERIFY_CLASS_SIZE(ULocalPlayer);
	VERIFY_CLASS_SIZE(UAudioComponent);

	debugf(TEXT("Object size: %u"),sizeof(UObject));
	debugf(TEXT("Actor size: %u"),sizeof(AActor));
	debugf(TEXT("ActorComponent size: %u"),sizeof(UActorComponent));
	debugf(TEXT("PrimitiveComponent size: %u"),sizeof(UPrimitiveComponent));

	// Handle pending level.
	if( Pending )
	{
		check(Pending==GPendingLevel);

		// Hook network driver up to level.
		GLevel->NetDriver = Pending->NetDriver;
		if( GLevel->NetDriver )
			GLevel->NetDriver->Notify = GLevel;

		// Setup level.
		GLevel->GetLevelInfo()->NetMode = NM_Client;
	}
	else check(!GLevel->NetDriver);

	// Purge unused objects and flush caches.
	CollectGarbage( RF_Native );
	Flush();

	// Initialize gameplay for the level.
	GLevel->BeginPlay(URL,PackageMap);

	// Listen for clients.
	if( !Client || URL.HasOption(TEXT("Listen")) )
	{
		if( GPendingLevel )
		{
			check(!Pending);
			delete GPendingLevel;
			GPendingLevel = NULL;
		}
		FString Error;
		if( !GLevel->Listen( Error ) )
			appErrorf( *LocalizeError(TEXT("ServerListen"),TEXT("Engine")), *Error );
	}
	// Client init.
	if( Client )
	{
		// Match Viewports to actors.
		MatchViewportsToActors( this, GLevel->IsServer() ? GLevel : GEntry, URL );
	}
	// Remember the URL.
	LastURL = URL;
	// Remember DefaultPlayer options.
	if( GIsClient )
	{
		URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Name" ), TEXT("User") );
		URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Team" ), TEXT("User") );
		URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Class"), TEXT("User") );
		URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Skin" ), TEXT("User") );
		URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Face" ), TEXT("User") );
		URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Voice" ), TEXT("User") );
		URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("OverrideClass" ), TEXT("User") );
	}

	debugf(TEXT("########### Finished loading level: %f seconds"),appSeconds() - StartTime);

	// Successfully started local level.
	return GLevel;
}

static void ExportTravel( FOutputDevice& Out, AActor* Actor )
{
	debugf( TEXT("Exporting travelling actor of class %s"), *Actor->GetClass()->GetPathName() );//!!xyzzy
	check(Actor);
	if( !Actor->bTravel )
		return;
	Out.Logf( TEXT("Class=%s Name=%s\r\n{\r\n"), *Actor->GetClass()->GetPathName(), Actor->GetName() );
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Actor->GetClass()); It; ++It )
	{
		for( INT Index=0; Index<It->ArrayDim; Index++ )
		{
			FString	Value;
			if
			(	(It->PropertyFlags & CPF_Travel)
			&&	It->ExportText( Index, Value, (BYTE*)Actor, &Actor->GetClass()->Defaults(0), Actor, 0 ) )
			{
				Out.Log( It->GetName() );
				if( It->ArrayDim!=1 )
					Out.Logf( TEXT("[%i]"), Index );
				Out.Log( TEXT("=") );
				UObjectProperty* Ref = Cast<UObjectProperty>( *It );
				if( Ref && Ref->PropertyClass->IsChildOf(AActor::StaticClass()) )
				{
					UObject* Obj = *(UObject**)( (BYTE*)Actor + It->Offset + Index*It->ElementSize );
					Out.Logf( TEXT("%s\r\n"), Obj ? Obj->GetName() : TEXT("None") );
				}
				Out.Logf( TEXT("%s\r\n"), *Value );
			}
		}
	}
	Out.Logf( TEXT("}\r\n") );
}

//
// Jumping viewport.
//
void UGameEngine::SetClientTravel( const TCHAR* NextURL, UBOOL bItems, ETravelType InTravelType )
{
	TravelURL    = NextURL;
	TravelType   = InTravelType;
	bTravelItems = bItems;

	// Prevent crashing the game by attempting to connect to own listen server
	//FIXMESTEVE if ( LastURL.HasOption(TEXT("Listen")) )
	//	LastURL.RemoveOption(TEXT("Listen"));
}

/*-----------------------------------------------------------------------------
	Tick.
-----------------------------------------------------------------------------*/

//
// Get tick rate limitor.
//
FLOAT UGameEngine::GetMaxTickRate()
{
	static UBOOL LanPlay = ParseParam(appCmdLine(),TEXT("lanplay"));
	if( GLevel && GLevel->NetDriver && !GIsClient )
	{
		// We're a dedicated server, use the LAN or Net tick rate.
		return Clamp( GLevel->NetDriver->NetServerMaxTickRate, 10, 120 );
	}
	else
	if( GLevel && GLevel->NetDriver && GLevel->NetDriver->ServerConnection )
	{
		FLOAT MaxClientFrameRate = (GLevel->NetDriver->ServerConnection->CurrentNetSpeed > 10000) ? GLevel->GetLevelInfo()->MaxClientFrameRate : 90.f;
		return Clamp((GLevel->NetDriver->ServerConnection->CurrentNetSpeed /*FIXMESTEVE- GLevel->NetDriver->ServerConnection->CurrentVoiceBandwidth*/)/GLevel->GetLevelInfo()->MoveRepSize, 10.f, MaxClientFrameRate);
	}
	else
		return 0;
}

//
// Update everything.
//
void UGameEngine::Tick( FLOAT DeltaSeconds )
{
	INT LocalTickCycles=0;
	clock(LocalTickCycles);

    if( DeltaSeconds < 0.0f )
        appErrorf(TEXT("Negative delta time!"));

	// Tick the client code.
	INT LocalClientCycles=0;
	if( Client )
	{
		clock(LocalClientCycles);
		Client->Tick( DeltaSeconds );
		unclock(LocalClientCycles);
	}
	ClientCycles=LocalClientCycles;

	// Clean up the viewports that have been closed.
	for(FPlayerIterator It(this);It;++It)
		if(!It->Viewport)
			It.RemoveCurrent();

	// If all viewports closed, time to exit.
	if(GIsClient && !Players.Num())
	{
		debugf( TEXT("All Windows Closed") );
		appRequestExit( 0 );
		return;
	}

	// Release mouse if the game is paused. The low level input code might ignore the request when e.g. in fullscreen mode.
	for( INT i=0; i<Players.Num(); i++ )
		if( Players(i)->MasterViewport && GLevel )
			Players(i)->MasterViewport->CaptureMouseInput( !GLevel->IsPaused() );

	// Update subsystems.
	UObject::StaticTick();				

	// Update the level.
	GameCycles=0;
	clock(GameCycles);
	if( GLevel )
	{
		// Decide whether to drop high detail because of frame rate.
		if ( Client )
		{
			GLevel->GetLevelInfo()->bDropDetail = (DeltaSeconds > 1.f/Clamp(Client->MinDesiredFrameRate,1.f,100.f)) && !GIsBenchmarking;
			GLevel->GetLevelInfo()->bAggressiveLOD = (DeltaSeconds > 1.f/Clamp(Client->MinDesiredFrameRate - 5.f,1.f,100.f)) && !GIsBenchmarking;
		}
		// Tick the level.
		GLevel->Tick( LEVELTICK_All, DeltaSeconds );
	
		// Update Audio.
		if( Client && Client->GetAudioDevice() )
		{
			// Don't play audio on first tick as audio device gets ticked before render device to avoid latency though
			// render device will precache on first tick which can take a significant amount of time.
			UBOOL FirstTick = GLevel->TimeSeconds == DeltaSeconds;
			Client->GetAudioDevice()->Update( FirstTick ? 0 : !GLevel->IsPaused() );
		}

		if( GEntry && GEntry!=GLevel )
		{
			// Tick any TCP links in the entry level
			FLOAT EntryDelta = DeltaSeconds;

			// Update entry clock
			EntryDelta *= GEntry->GetLevelInfo()->TimeDilation;

			GEntry->TimeSeconds += EntryDelta;
			GEntry->GetLevelInfo()->TimeSeconds = GEntry->TimeSeconds;

			// Go through ticking any TCP links
			for( INT iActor=GEntry->iFirstDynamicActor; iActor<GEntry->Actors.Num(); iActor++ )
			{
				AActor* Actor = GEntry->Actors( iActor );
				if( Actor && !Actor->bDeleteMe && Actor->ShouldTickInEntry() )
					Actor->Tick(EntryDelta, LEVELTICK_All);
			}
		}
	}
	unclock(GameCycles);

	// Handle server travelling.
	if( GLevel && GLevel->GetLevelInfo()->NextURL!=TEXT("") )
	{
		if( (GLevel->GetLevelInfo()->NextSwitchCountdown-=DeltaSeconds) <= 0.f )
		{
			// Travel to new level, and exit.
			TMap<FString,FString> TravelInfo;
			if( GLevel->GetLevelInfo()->NextURL==TEXT("?RESTART") )
			{
				TravelInfo = GLevel->TravelInfo;
			}
			else if( GLevel->GetLevelInfo()->bNextItems )
			{
				TravelInfo = GLevel->TravelInfo;
				for( INT i=0; i<GLevel->Actors.Num(); i++ )
				{
					APlayerController* P = Cast<APlayerController>( GLevel->Actors(i) );
					if( P && P->Player && P->Pawn )
					{
						// Export items and self.
						FStringOutputDevice PlayerTravelInfo;
						ExportTravel( PlayerTravelInfo, P->Pawn );
						for( AInventory* Inv=P->Pawn->InvManager->InventoryChain; Inv; Inv=Inv->Inventory )
							ExportTravel( PlayerTravelInfo, Inv );
						TravelInfo.Set( *P->PlayerReplicationInfo->PlayerName, *PlayerTravelInfo );

						// Prevent local ClientTravel from taking place, since it will happen automatically.
						TravelURL = TEXT("");
					}
				}
			}
			debugf( TEXT("Server switch level: %s"), *GLevel->GetLevelInfo()->NextURL );
			FString Error;
			Browse( FURL(&LastURL,*GLevel->GetLevelInfo()->NextURL,TRAVEL_Relative), &TravelInfo, Error );
			GLevel->GetLevelInfo()->NextURL = TEXT("");
			return;
		}
	}

	// Handle client travelling.
	if( TravelURL != TEXT("") )
	{
		// Travel to new level, and exit.
		TMap<FString,FString> TravelInfo;

        if( GLevel && GLevel->GetLevelInfo() && GLevel->GetLevelInfo()->Game )
            GLevel->GetLevelInfo()->Game->eventGameEnding(); 

		// Export items.
		if( appStricmp(*TravelURL,TEXT("?RESTART"))==0 )
		{
			TravelInfo = GLevel->TravelInfo;
		}
		else if( bTravelItems )
		{
			for(UINT PlayerIndex = 0;PlayerIndex < (UINT)Players.Num();PlayerIndex++)
			{
				debugf( TEXT("Export travel for: %s"), *Players(PlayerIndex)->Actor->PlayerReplicationInfo->PlayerName );
				FStringOutputDevice PlayerTravelInfo;
				ExportTravel( PlayerTravelInfo, Players(PlayerIndex)->Actor->Pawn );
				for( AInventory* Inv=Players(PlayerIndex)->Actor->Pawn->InvManager->InventoryChain; Inv; Inv=Inv->Inventory )
					ExportTravel( PlayerTravelInfo, Inv );
				TravelInfo.Set( *Players(PlayerIndex)->Actor->PlayerReplicationInfo->PlayerName, *PlayerTravelInfo );
			}
		}
		FString Error;
		Browse( FURL(&LastURL,*TravelURL,TravelType), &TravelInfo, Error );
		TravelURL=TEXT("");

		return;
	}

	// Update the pending level.
	if( GPendingLevel )
	{
		GPendingLevel->Tick( DeltaSeconds );
		if( GPendingLevel->Error!=TEXT("") )
		{
			// Pending connect failed.
			SetProgress( *LocalizeError(TEXT("ConnectionFailed"),TEXT("Engine")), *GPendingLevel->Error, 4.f );
			debugf( NAME_Log, *LocalizeError(TEXT("Pending"),TEXT("Engine")), *GPendingLevel->URL.String(), *GPendingLevel->Error );
			delete GPendingLevel;
			GPendingLevel = NULL;
		}
		else if( GPendingLevel->Success && !GPendingLevel->FilesNeeded && !GPendingLevel->SentJoin )
		{
			// Attempt to load the map.
			FString Error;
			LoadMap( GPendingLevel->URL, GPendingLevel, NULL, Error );
			if( Error!=TEXT("") )
			{
				SetProgress( *LocalizeError(TEXT("ConnectionFailed"),TEXT("Engine")), *Error, 4.f );
			}
			else if( !GPendingLevel->LonePlayer )
			{
				check(GLevel);
				check(GEntry);
				// Show connecting message, cause precaching to occur.
				GLevel->GetLevelInfo()->LevelAction = LEVACT_Connecting;
				GEntry->GetLevelInfo()->LevelAction = LEVACT_Connecting;
				if( Client )
					Client->Tick( DeltaSeconds );

				// Send join.
				GPendingLevel->SendJoin();
				GPendingLevel->NetDriver = NULL;
			}
			// Kill the pending level.
			delete GPendingLevel;
			GPendingLevel = NULL;
		}
	}

	// Update memory stats.
	if( GMemoryStats.Enabled )
	{
		SIZE_T Virtual, Physical;
		GMalloc->GetAllocationInfo( Virtual, Physical );

		GMemoryStats.VirtualAllocations.Value	+= Virtual / 1024.f / 1024.f;
		GMemoryStats.PhysicalAllocations.Value	+= Physical / 1024.f / 1024.f;
	}

	// Placed right before draw in order to delay stalling the GPU for as long as possible.
	GStreamingManager->Tick();

	// Render everything.
	for(FPlayerIterator It(this);It;++It)
		It->Viewport->Draw(!GIsBenchmarking);
	
	unclock(LocalTickCycles);
	TickCycles=LocalTickCycles;

	if( GScriptCallGraph )
		GScriptCallGraph->Tick();
}

/*-----------------------------------------------------------------------------
	Saving the game.
-----------------------------------------------------------------------------*/

//
// Save the current game state to a file.
//
void UGameEngine::SaveGame( INT Position )
{
	debugf(TEXT("SAVE GAME!"));
	GLevel->GetLevelInfo()->Game->bIsSaveGame = true;  // for next time the level is loaded
	TCHAR Filename[256];
	GFileManager->MakeDirectory( *GSys->SavePath, 0 );
	appSprintf( Filename, TEXT("%s") PATH_SEPARATOR TEXT("Save%i.usa"), *GSys->SavePath, Position );
	GLevel->GetLevelInfo()->LevelAction=LEVACT_Saving;
	PaintProgress();
	GWarn->BeginSlowTask( *LocalizeProgress(TEXT("Saving"),TEXT("Engine")), 1);
	GLevel->CleanupDestroyed( 1 );
	SavePackage( GLevel->GetOuter(), GLevel, 0, Filename, GLog );
	GWarn->EndSlowTask();
	GLevel->GetLevelInfo()->LevelAction=LEVACT_None;
	
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
