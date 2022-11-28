/*=============================================================================
	LaunchEngineLoop.cpp: Main engine loop.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel
=============================================================================*/

#include "LaunchPrivate.h"
#include "..\..\Engine\Debugger\UnDebuggerCore.h"

#define STATIC_LINKING_MOJO 1

#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "EngineClasses.h"
#include "IpDrvClasses.h"
#ifndef XBOX
#include "EditorClasses.h"
#include "UnrealEdClasses.h"
#endif
#if   GAMENAME == WARGAME
#include "WarfareGame.h"
#elif GAMENAME == DEMOGAME
#include "DemoGameClasses.h"
#elif GAMENAME == UTGAME
#include "UTGameClasses.h"
#else
	#error Hook up your game name here
#endif
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

// General.
#ifndef XBOX
extern "C" {HINSTANCE hInstance;}					//@hack: the global HINSTANCE should be removed when we remove WWindow.
#endif
extern "C" {TCHAR GPackage[64]=TEXT("Launch");}

#ifdef XBOX
FMallocMemoryImageXenon				Malloc;
#elif _DEBUG
static FMallocDebug					Malloc;
#else
static FMallocWindows				Malloc;
#endif

#if KEEP_ALLOCATION_BACKTRACE
static FMallocDebugProxyWindows		MallocDebugProxy( &Malloc );
#endif

#ifndef XBOX
/** Critical section used by MallocThreadSafeProxy for synchronization										*/
static FCriticalSectionWin			MallocCriticalSection;
#else
/** Remote debug command																					*/
static __declspec(allocate(XENON_NOCLOBBER_SEG)) CHAR					RemoteDebugCommand[1024];		
/** Critical section for accessing remote debug command														*/
static __declspec(allocate(XENON_NOCLOBBER_SEG)) FCriticalSectionWin	RemoteDebugCriticalSection;	
/** Critical section used by MallocThreadSafeProxy for synchronization										*/
static __declspec(allocate(XENON_NOCLOBBER_SEG)) FCriticalSectionWin	MallocCriticalSection;
#endif

#if KEEP_ALLOCATION_BACKTRACE
/** Thread safe malloc proxy, rendering any FMalloc thread safe												*/
static FMallocThreadSafeProxy		MallocThreadSafeProxy( &MallocDebugProxy, &MallocCriticalSection );
#else
/** Thread safe malloc proxy, rendering any FMalloc thread safe												*/
static FMallocThreadSafeProxy		MallocThreadSafeProxy( &Malloc, &MallocCriticalSection );
#endif

#ifndef XBOX
static FOutputDeviceFile			Log;
static FOutputDeviceWindowsError	Error;
static FFeedbackContextWindows		GameWarn;
static FFeedbackContextEditor		UnrealEdWarn;
static FFileManagerWindows			FileManager;
static FCallbackDevice				GameCallback;
static FCallbackDeviceEditor		UnrealEdCallback;
static FOutputDeviceConsoleWindows	LogConsole;
static FSynchronizeFactoryWin		SynchronizeFactory;
static FThreadFactoryWin			ThreadFactory;
#else
//@todo xenon: fixup appSeconds()
static FOutputDeviceDebug			Log;
static FOutputDeviceAnsiError		Error;
static FFeedbackContextAnsi			GameWarn;
static FFileManagerXenon			FileManager;
static FCallbackDevice				GameCallback;
#endif

extern	TCHAR						GCmdLine[4096];

/** Thread used for async IO manager */
FRunnableThread*					AsyncIOThread;

#ifndef XBOX
#include "..\..\Launch\Resources\resource.h"
/** Resource ID of icon to use for Window */
INT									GGameIcon	= IDICON_Game;
#endif

/*-----------------------------------------------------------------------------
	FEngineLoop implementation.
-----------------------------------------------------------------------------*/

INT FEngineLoop::Init( const TCHAR* CmdLine )
{
#ifndef XBOX
	// This is done in appXenonInit on Xenon.
	GSynchronizeFactory = &SynchronizeFactory;
	GThreadFactory		= &ThreadFactory;

	appInit( CmdLine, &MallocThreadSafeProxy, &Log, &LogConsole, &Error, &GameWarn, &FileManager, &GameCallback, FConfigCacheIni::Factory );
#else
	appInit( CmdLine, &MallocThreadSafeProxy, &Log, NULL       , &Error, &GameWarn, &FileManager, &GameCallback, FConfigCacheIni::Factory );
#endif

	// Static linking.
	for( INT k=0; k<ARRAY_COUNT(GNativeLookupFuncs); k++ )
		GNativeLookupFuncs[k] = NULL;
	INT Lookup = 0;
	// Core natives.
	GNativeLookupFuncs[Lookup++] = &FindCoreUObjectNative;
	GNativeLookupFuncs[Lookup++] = &FindCoreUCommandletNative;
	// auto-generated lookups and statics
	AUTO_INITIALIZE_REGISTRANTS_ENGINE;
	AUTO_INITIALIZE_REGISTRANTS_D3DDRV;
	AUTO_INITIALIZE_REGISTRANTS_IPDRV;
#ifndef XBOX
	AUTO_INITIALIZE_REGISTRANTS_EDITOR;
	AUTO_INITIALIZE_REGISTRANTS_UNREALED;
	AUTO_INITIALIZE_REGISTRANTS_ALAUDIO;
	AUTO_INITIALIZE_REGISTRANTS_WINDRV;
#else
	AUTO_INITIALIZE_REGISTRANTS_XEAUDIO;
	AUTO_INITIALIZE_REGISTRANTS_XEDRV;
#endif
#if   GAMENAME == WARGAME
	AUTO_INITIALIZE_REGISTRANTS_WARFAREGAME;
#elif GAMENAME == DEMOGAME
	AUTO_INITIALIZE_REGISTRANTS_DEMOGAME;
#elif GAMENAME == UTGAME
	AUTO_INITIALIZE_REGISTRANTS_UTGAME;
#else
	#error Hook up your game name here
#endif
	check( Lookup < ARRAY_COUNT(GNativeLookupFuncs) );

	// Initialize names and verify class sizes & property offsets (for noexport classes).
	UBOOL Mismatch = false;
	// Uncomment the below line for Script/ C++ mismatch detection.
	//#define VERIFY_CLASS_SIZES	
	#define NAMES_ONLY
	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#define AUTOGENERATE_NAME(name) ENGINE_##name = FName(TEXT(#name),FNAME_Intrinsic);
	#include "EngineClasses.h"
	#include "EngineAIClasses.h"
	#include "EngineMaterialClasses.h"
	#include "EngineTerrainClasses.h"
	#include "EnginePhysicsClasses.h"
	#include "EngineSequenceClasses.h"
	#include "EngineSoundClasses.h"
	#include "EngineInterpolationClasses.h"
	#include "EngineParticleClasses.h"
	#include "EngineAnimClasses.h"
	#undef AUTOGENERATE_NAME
#ifndef XBOX	
	#define AUTOGENERATE_NAME(name) EDITOR_##name=FName(TEXT(#name),FNAME_Intrinsic);
	#include "EditorClasses.h"
	#undef AUTOGENERATE_NAME
	#define AUTOGENERATE_NAME(name) UNREALED_##name=FName(TEXT(#name),FNAME_Intrinsic);
	#include "UnrealEdClasses.h"
#endif
	#undef AUTOGENERATE_NAME
	#define AUTOGENERATE_NAME(name) IPDRV_##name=FName(TEXT(#name),FNAME_Intrinsic);
	#include "IpDrvClasses.h"
	#undef AUTOGENERATE_NAME
#if   GAMENAME == WARGAME
	#define AUTOGENERATE_NAME(name) WARFAREGAME_##name=FName(TEXT(#name),FNAME_Intrinsic);
	#include "WarfareGameClasses.h"
#elif GAMENAME == DEMOGAME
	#define AUTOGENERATE_NAME(name) DEMOGAME_##name=FName(TEXT(#name),FNAME_Intrinsic);
	#include "DemoGameClasses.h"
#elif GAMENAME == UTGAME
	#define AUTOGENERATE_NAME(name) UTGAME_##name=FName(TEXT(#name),FNAME_Intrinsic);
	#include "UTGameClasses.h"
#else
	#error Hook up your game name here
#endif
	#undef AUTOGENERATE_FUNCTION
	#undef AUTOGENERATE_NAME
	#undef NAMES_ONLY
	#undef VERIFY_CLASS_SIZES
	if( Mismatch )
		appErrorf(TEXT("C++/ Script class size mismatch"));

	// Create a new resource manager. Needs to happen before commandlet execution.
	GResourceManager = new FResourceManager();

	// Create the streaming manager and add the default static texture streamer.
	GStreamingManager = new FStreamingManager();
	GStreamingManager->AddStreamer( new FStaticTextureStreamer() );
	
	// Create the resource loaded and add the defaul background loader.
	GResourceLoader = new FResourceLoader();
	GResourceLoader->AddLoader(  new FBackgroundLoaderUnreal() );
	
	// Create the async IO manager and have it run on CPU 1.
	GAsyncIOManager = new FAsyncIOManagerWindows();
	AsyncIOThread = GThreadFactory->CreateThread( GAsyncIOManager );
	check(AsyncIOThread);
	AsyncIOThread->SetProcessorAffinity( 1 );

#ifndef XBOX
	// Set the game icon used by Window.cpp/ WinClient.cpp.
#if GAMENAME == WARGAME
	GGameIcon = IDICON_Game;
#endif

	// Figure out whether we're the editor, ucc or the game.
	TCHAR* ParsedCmdLine	= new TCHAR[ appStrlen(appCmdLine())+1 ];
	appStrcpy( ParsedCmdLine, appCmdLine() );
	FString Token			= ParseToken( (const TCHAR *&)ParsedCmdLine, static_cast<UBOOL>( 0 ) );

	if( Token == TEXT("EDITOR") )
	{		
		// We're the editor.
		GIsClient	= 1;
		GIsServer	= 1;
		GIsEditor	= 1;
		GIsUCC		= 0;
		GGameIcon	= IDICON_Editor;

		// UnrealEd requires a special callback handler and feedback context.
		GCallback	= &UnrealEdCallback;
		GWarn		= &UnrealEdWarn;

		// Remove "EDITOR" from command line.
		appStrcpy( GCmdLine, ParsedCmdLine );

		// Set UnrealEd as the current package (used for e.g. log and localization files).
		appStrcpy( GPackage, TEXT("UnrealEd") );
	}
	else 
	{
		// See whether the first token on the command line is a commandlet.

		//@hack: We need to set these before calling StaticLoadClass so all required data gets loaded for the commandlets.
		//@todo static linking: we should use something along the lines of FindObject to see whether the current token is a commandlet or not.
		GIsClient	= 1;
		GIsServer	= 1;
		GIsEditor	= 1;
		GIsUCC		= 1;

		UClass* Class = NULL;

		if( Token == TEXT("MAKE") )
		{
			// We can't bind to .u files if we want to build them via the make commandlet, hence the LOAD_DisallowFiles.
			Class = UObject::StaticLoadClass( UCommandlet::StaticClass(), NULL, TEXT("editor.makecommandlet"), NULL, LOAD_NoWarn | LOAD_Quiet | LOAD_DisallowFiles, NULL );
		}
		else if( Token == TEXT("SERVER") )
		{
			//@todo static linking: this should be replaced with a less hardcoded solution
			Class = UObject::StaticLoadClass( UCommandlet::StaticClass(), NULL, TEXT("engine.servercommandlet"), NULL, LOAD_NoWarn | LOAD_Quiet, NULL );
		}
		else
		{
			// Try loading the class name.
			Class = UObject::StaticLoadClass( UCommandlet::StaticClass(), NULL, *Token, NULL, LOAD_NoWarn | LOAD_Quiet, NULL );
			if( !Class )
				Class = UObject::StaticLoadClass( UCommandlet::StaticClass(), NULL, *(Token+TEXT("Commandlet")), NULL, LOAD_NoWarn | LOAD_Quiet, NULL );
		}

		if( Class )
		{
			// The first token was a commandlet so execute it.
	
			// Remove commandlet name from command line.
			appStrcpy( GCmdLine, ParsedCmdLine );

			// Set UCC as the current package (used for e.g. log and localization files).
			appStrcpy( GPackage, TEXT("UCC") );

			// Bring up console unless we're a silent build.
			if( GLogConsole && !ParseParam(appCmdLine(),TEXT("Silent")) )
				GLogConsole->Show( TRUE );

			UObject::SetLanguage(TEXT("INT"));
			debugf( TEXT("Executing %s"), *Class->GetFullName() );
	
			// Allow commandlets to individually override those settings.
			UCommandlet* Default = CastChecked<UCommandlet>(Class->GetDefaultObject());
			GIsClient	= Default->IsClient;
			GIsServer	= Default->IsServer;
			GIsEditor	= Default->IsEditor;
			GLazyLoad	= Default->LazyLoad;
			GIsUCC		= 1;

			// Rebuilding script requires some hacks in the engine so we flag that.
			if( Token == TEXT("MAKE") )
				GIsUCCMake = 1;

			// Reset aux log if we don't want to log to the console window.
			if( !Default->LogToConsole )
				GLog->RemoveOutputDevice( GLogConsole );

			UCommandlet* Commandlet = ConstructObject<UCommandlet>( Class );
			check(Commandlet);

			// Execute the commandlet.
			Commandlet->InitExecution();
			Commandlet->ParseParms( appCmdLine() );
			Commandlet->Main( appCmdLine() );

			INT ErrorLevel = 0;

			// Log warning/ error summary.
			if( Commandlet->ShowErrorCount )
			{
				if ( GWarn->ErrorCount == 0)
				{
					warnf( TEXT("Success - %d error(s), %d warning(s)"), GWarn->ErrorCount, GWarn->WarningCount );
				}
				else
				{
					warnf( TEXT("Failure - %d error(s), %d warning(s)"), GWarn->ErrorCount, GWarn->WarningCount );
					ErrorLevel = 1;
				}
			}
			else
			{
				warnf( TEXT("Finished...") );
			}
		
			// We're ready to exit!
			return ErrorLevel;
		}
		else
#else
	{
#endif
		{
			// We're a regular client.
			GIsClient		= 1;
			GIsServer		= 0;
			GIsEditor		= 0;
			GIsUCC			= 0;
		}
	}

	// Exit if wanted.
	if( GIsRequestingExit )
	{
		appPreExit();
		// appExit is called outside guarded block.
		return 1;
	}
#ifndef XBOX
	else
	{
		// Display the splash screen.
		InitSplash( TEXT("Splash\\Logo.bmp") );
	}

	// Init windowing.
	InitWindowing();
#endif

	// Benchmarking.
	GIsBenchmarking	= ParseParam(appCmdLine(),TEXT("BENCHMARK"));
	
	// Don't update ini files if benchmarking or -noini
	if( GIsBenchmarking || ParseParam(appCmdLine(),TEXT("NOINI")))
	{
		GConfig->Detach( GEngineIni );
		GConfig->Detach( GInputIni );
		GConfig->Detach( GGameIni );
		GConfig->Detach( GEditorIni );
	}

#ifndef XBOX
	// Create mutex so installer knows we're running.
	CreateMutexX( NULL, 0, TEXT("UnrealIsRunning"));
	UBOOL AlreadyRunning;
	AlreadyRunning = (GetLastError()==ERROR_ALREADY_EXISTS);
#endif

	// Figure out which UEngine variant to use.
	UClass* EngineClass = NULL;
	if( !GIsEditor )
	{
		// We're the game.
		EngineClass = UObject::StaticLoadClass( UGameEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.GameEngine"), NULL, LOAD_NoFail, NULL );
		GEngine = ConstructObject<UEngine>( EngineClass );
	}
	else
	{
#ifndef XBOX
		// We're UnrealEd.
		EngineClass = UObject::StaticLoadClass( UUnrealEdEngine::StaticClass(), NULL, TEXT("UnrealEd.UnrealEdEngine"), NULL, LOAD_NoFail, NULL );
		GEngine = GEditor = GUnrealEd = ConstructObject<UUnrealEdEngine>( EngineClass );
#else
		check(0);
#endif
	}

	if ( ParseParam( appCmdLine(), TEXT("AUTODEBUG") ) )
	{
		if ( GDebugger == NULL )
		{
			GDebugger = new UDebuggerCore();
		}
	}

	check( GEngine );
	GEngine->Init();

#ifndef XBOX
	// Hide splash screen.
	ExitSplash();
#endif

	// Init variables used for benchmarking and ticking.
	OldRealTime					= appSeconds(),
	OldTime						= GIsBenchmarking ? 0 : OldRealTime;
	FrameCounter				= 0,
	MaxFrameCounter				= 0;
	LastFrameCycles				= appCycles();

	Parse(appCmdLine(),TEXT("SECONDS="),MaxFrameCounter);
	MaxFrameCounter						*= 30;

	// Optionally Exec an exec file
	FString Temp;
	if( Parse(appCmdLine(), TEXT("EXEC="), Temp) )
	{
		Temp = FString(TEXT("exec ")) + Temp;
		if( Cast<UGameEngine>(GEngine) && Cast<UGameEngine>(GEngine)->Players.Num() && Cast<UGameEngine>(GEngine)->Players(0) )
			Cast<UGameEngine>(GEngine)->Players(0)->Exec( *Temp, *GLog );
	}

	GIsRunning = 1;

	return 0;
}

void FEngineLoop::Exit()
{
	GIsRunning	= 0;
	GLogConsole	= NULL;

	// Output benchmarking data.
	if( GIsBenchmarking )
	{
		FLOAT	MinFrameTime = 1000.f,
				MaxFrameTime = 0.f,
				AvgFrameTime = 0;

		// Determine min/ max framerate (discarding first 10 frames).
		for( INT i=10; i<FrameTimes.Num(); i++ )
		{		
			MinFrameTime = Min( MinFrameTime, FrameTimes(i) );
			MaxFrameTime = Max( MaxFrameTime, FrameTimes(i) );
			AvgFrameTime += FrameTimes(i);
		}
		AvgFrameTime /= FrameTimes.Num() - 10;

		// Output results to Benchmark/benchmark.log
		FString OutputString = TEXT("");
		appLoadFileToString( OutputString, TEXT("..\\Benchmark\\benchmark.log") );
		OutputString += FString::Printf(TEXT("min= %6.2f   avg= %6.2f   max= %6.2f%s"), 1000.f / MaxFrameTime, 1000.f / AvgFrameTime, 1000.f / MinFrameTime, LINE_TERMINATOR );
		appSaveStringToFile( OutputString, TEXT("..\\Benchmark\\benchmark.log") );

		FrameTimes.Empty();
	}

	GStreamingManager->Flush( FALSE );

	delete GEngine;
	GEngine				= NULL;
	
	appPreExit();
	DestroyGameRBPhys();

	delete GResourceManager;
	GResourceManager	= NULL;

	delete GStreamingManager;
	GStreamingManager	= NULL;

	delete GResourceLoader;
	GResourceLoader		= NULL;

	GThreadFactory->Destroy( AsyncIOThread );
}

void FEngineLoop::Tick()
{
	if( GDebugger )
		GDebugger->NotifyBeginTick();

	for(FStatGroup* Group = GFirstStatGroup;Group;Group = Group->NextGroup)
		Group->AdvanceFrame();

	DOUBLE	CurrentRealTime = appSeconds();

	if( GIsBenchmarking && MaxFrameCounter && (FrameCounter > MaxFrameCounter) )
		appRequestExit(0);

	if( GIsBenchmarking )
		GCurrentTime += 1.0f / 30.0f;
	else
		GCurrentTime = CurrentRealTime;

	FLOAT DeltaTime	= Clamp<FLOAT>( GCurrentTime - OldTime, 0.0001f, 1.f );

	//Update.
	GEngine->Tick( DeltaTime );
	if( GWindowManager )
		GWindowManager->Tick( DeltaTime );

	FrameCounter++;
	OldTime		= GCurrentTime;
	OldRealTime = CurrentRealTime;
	
	// Enforce optional maximum tick rate.
	if( !GIsBenchmarking )
	{		
		FLOAT MaxTickRate = GEngine->GetMaxTickRate();
		if( MaxTickRate>0.0 )
		{
			FLOAT Delta = (1.0/MaxTickRate) - (appSeconds()-OldTime);
			appSleep( Max(0.f,Delta) );
		}
	}

#ifndef XBOX
	// Handle all incoming messages.
	MSG Msg;
	while( PeekMessageX(&Msg,NULL,0,0,PM_REMOVE) )
	{
		// When closing down the editor, check to see if there are any unsaved dirty packages.
		if( GIsEditor && Msg.message == WM_QUIT )
		{
			GEngine->SaveDirtyPackages();
			GIsRequestingExit = 1;
		}

		TranslateMessage( &Msg );
		DispatchMessageX( &Msg );
	}

	// If editor thread doesn't have the focus, don't suck up too much CPU time.
	if( GIsEditor )
	{
		static UBOOL HadFocus=1;
		UBOOL HasFocus = (GetWindowThreadProcessId(GetForegroundWindow(),NULL) == GetCurrentThreadId() );
		if( HadFocus && !HasFocus )
		{
			// Drop our priority to speed up whatever is in the foreground.
			SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL );
		}
		else if( HasFocus && !HadFocus )
		{
			// Boost our priority back to normal.
			SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_NORMAL );
		}
		if( !HasFocus )
		{
			// Sleep for a bit to not eat up all CPU time.
			Sleep(5);
		}
		HadFocus = HasFocus;
	}
#endif

	DWORD	CurrentFrameCycles		= appCycles();
	GEngineStats.FrameTime.Value   += CurrentFrameCycles - LastFrameCycles;
	LastFrameCycles					= CurrentFrameCycles;

	GEngineStats.UnrealScriptTime.Value += GScriptCycles;
	GScriptCycles = 0;

	if( GIsBenchmarking )
		FrameTimes.AddItem( GEngineStats.FrameTime.Value * GSecondsPerCycle * 1000.f );

#ifdef XBOX
	// Handle remote debugging commands.
	FString RemoteDebugString;
	{
		FScopeLock ScopeLock(&RemoteDebugCriticalSection);
		RemoteDebugString = FString(ANSI_TO_TCHAR(RemoteDebugCommand));
		RemoteDebugCommand[0] = '\0';
	}
	if( RemoteDebugString.Len() && Cast<UGameEngine>(GEngine) && Cast<UGameEngine>(GEngine)->Players.Num() && Cast<UGameEngine>(GEngine)->Players(0) )
		Cast<UGameEngine>(GEngine)->Players(0)->Exec( *RemoteDebugString, *GLog );
#endif
}

/*-----------------------------------------------------------------------------
	Remote debug channel support.
-----------------------------------------------------------------------------*/

#ifdef XBOX

static int dbgstrlen( const CHAR* str )
{
    const CHAR* strEnd = str;
    while( *strEnd )
        strEnd++;
    return strEnd - str;
}
static inline CHAR dbgtolower( CHAR ch )
{
    if( ch >= 'A' && ch <= 'Z' )
        return ch - ( 'A' - 'a' );
    else
        return ch;
}
static INT dbgstrnicmp( const CHAR* str1, const CHAR* str2, int n )
{
    while( n > 0 )
    {
        if( dbgtolower( *str1 ) != dbgtolower( *str2 ) )
            return *str1 - *str2;
        --n;
        ++str1;
        ++str2;
    }
    return 0;
}
static void dbgstrcpy( CHAR* strDest, const CHAR* strSrc )
{
    while( ( *strDest++ = *strSrc++ ) != 0 );
}

HRESULT __stdcall DebugConsoleCmdProcessor( const CHAR* Command, CHAR* Response, DWORD ResponseLen, PDM_CMDCONT pdmcc )
{
	// Skip over the command prefix and the exclamation mark.
	Command += dbgstrlen("UNREAL") + 1;

	// Check if this is the initial connect signal
	if( dbgstrnicmp( Command, "__connect__", 11 ) == 0 )
	{
		// If so, respond that we're connected
		lstrcpynA( Response, "Connected.", ResponseLen );
		return XBDM_NOERR;
	}

	{
		FScopeLock ScopeLock(&RemoteDebugCriticalSection);
		if( RemoteDebugCommand[0] )
		{
			// This means the application has probably stopped polling for debug commands
			dbgstrcpy( Response, "Cannot execute - previous command still pending." );
		}
		else
		{
			dbgstrcpy( RemoteDebugCommand, Command );
		}
	}

	return XBDM_NOERR;
}

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
