/*=============================================================================
	LaunchEngineLoop.cpp: Main engine loop.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "LaunchPrivate.h"
#include "UnConsoleSupportContainer.h"
#include "Database.h"
#include "ConsoleManager.h"
#include "FScopedMemoryStats.h"  
#if IPHONE
#include "IPhoneObjCWrapper.h"
#endif



#if _WINDOWS && !CONSOLE

#if UDK
#include "tinyxml.h"
#endif

#if WITH_EDITOR

#if GAMENAME == GEARGAME
#include "GearEditorCookerHelper.h"
#elif GAMENAME == EXOGAME
#include "ExoEditorCookerHelper.h"
#elif GAMENAME == FORTRESSGAME
#include "FortressEditorCookerHelper.h"
#elif GAMENAME == SWORDGAME
#include "SwordCookerHelper.h"
#endif  

#endif //WITH_EDITOR

#endif	//#if _WINDOWS && !CONSOLE

#if XBOX
	#include "XeViewport.h"
#endif

#if PS3
	#include "PS3Viewport.h"
#endif

#if WITH_REALD
	#include "RealD/RealD.h"
#endif
 
#include "GFxUI.h"
#if WITH_GFx
	#include "ScaleformEngine.h"
	#include "ScaleformFullscreenMovie.h"
#endif

#include "StreamingPauseRendering.h"

#if WITH_MANAGED_CODE
	#include "InteropShared.h"
#endif

#if WITH_SUBSTANCE_AIR == 1
#include "SubstanceAirTypedefs.h"
#include "SubstanceAirHelpers.h"
#include "SubstanceAirOutput.h"
#include "SubstanceAirInput.h"

#if !CONSOLE
#include <atc_api.h>
extern atc_api::Reader * GSubstanceAirTextureReader;

#endif // !CONSOLE && WITH_EDITOR

#endif // WITH_SUBSTANCE_AIR

/*-----------------------------------------------------------------------------
	Defines.
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
	Initializing registrants.
-----------------------------------------------------------------------------*/

// Static linking support forward declaration.
void InitializeRegistrantsAndRegisterNames();

// Please note that not all of these apply to every game/platform, but it's
// safe to declare them here and never define or reference them.

//	AutoInitializeRegistrants* declarations.
extern void AutoInitializeRegistrantsCore( INT& Lookup );
extern void AutoInitializeRegistrantsEngine( INT& Lookup );
extern void AutoInitializeRegistrantsGameFramework( INT& Lookup );
extern void AutoInitializeRegistrantsIpDrv( INT& Lookup );
extern void AutoInitializeRegistrantsXAudio2( INT& Lookup );
extern void AutoInitializeRegistrantsALAudio( INT& Lookup );
extern void AutoInitializeRegistrantsCoreAudio( INT& Lookup );
extern void AutoInitializeRegistrantsXeDrv( INT& Lookup );
extern void AutoInitializeRegistrantsPS3Drv( INT& Lookup );
extern void AutoInitializeRegistrantsNGPDrv( INT& Lookup );
extern void AutoInitializeRegistrantsWiiUDrv( INT& Lookup );
extern void AutoInitializeRegistrantsIPhoneDrv( INT& Lookup );
extern void AutoInitializeRegistrantsMacDrv( INT& Lookup );
extern void AutoInitializeRegistrantsKdDrv( INT& Lookup );
extern void AutoInitializeRegistrantsAndroidDrv( INT& Lookup );
extern void AutoInitializeRegistrantsFlashDrv( INT& Lookup );
extern void AutoInitializeRegistrantsOpenSLAudio( INT& Lookup );
extern void AutoInitializeRegistrantsUnrealEd( INT& Lookup );
extern void AutoInitializeRegistrantsWinDrv( INT& Lookup );
extern void AutoInitializeRegistrantsOnlineSubsystemLive( INT& Lookup );
extern void AutoInitializeRegistrantsOnlineSubsystemGameSpy( INT& Lookup );
extern void AutoInitializeRegistrantsOnlineSubsystemPC( INT& Lookup );
extern void AutoInitializeRegistrantsOnlineSubsystemSteamworks( INT& Lookup );
extern void AutoInitializeRegistrantsOnlineSubsystemGameCenter( INT& Lookup );

extern void appInitShowFlags();

#if GAMENAME == GEARGAME
extern void AutoInitializeRegistrantsGearGame( INT& Lookup );
extern void AutoInitializeRegistrantsGearEditor( INT& Lookup );

#elif GAMENAME == FORTRESSGAME

extern void AutoInitializeRegistrantsFortressBase( INT& Lookup );
extern void AutoInitializeRegistrantsFortress( INT& Lookup );
extern void AutoInitializeRegistrantsFortressEditor( INT& Lookup );

#elif GAMENAME == UTGAME
extern void AutoInitializeRegistrantsUDKBase( INT& Lookup );
extern void AutoInitializeRegistrantsUTEditor( INT& Lookup );

#elif GAMENAME == DUKEGAME
extern void AutoInitializeRegistrantsDukeGame( INT& Lookup );
extern void AutoInitializeRegistrantsDukeEditor( INT& Lookup );

#elif GAMENAME == EXOGAME
extern void AutoInitializeRegistrantsExoGame( INT& Lookup );
extern void AutoInitializeRegistrantsExoEditor( INT& Lookup );

#elif GAMENAME == SWORDGAME
extern void AutoInitializeRegistrantsSwordGame( INT& Lookup );
extern void AutoInitializeRegistrantsSwordEditor( INT& Lookup );
#endif

extern void AutoInitializeRegistrantsGFxUI( INT& Lookup );
#if WITH_GFx
	extern void AutoInitializeRegistrantsGFxUIEditor( INT& Lookup );
#endif // WITH_GFx


#if WITH_SUBSTANCE_AIR
extern void AutoInitializeRegistrantsSubstanceAir( INT& Lookup );
extern void AutoInitializeRegistrantsSubstanceAirEd( INT& Lookup );
#endif

//	AutoGenerateNames* declarations.
extern void AutoGenerateNamesCore();
extern void AutoGenerateNamesEngine();
extern void AutoGenerateNamesGameFramework();
extern void AutoGenerateNamesUnrealEd();
extern void AutoGenerateNamesIpDrv();
extern void AutoGenerateNamesWinDrv();
extern void AutoGenerateNamesOnlineSubsystemLive();
extern void AutoGenerateNamesOnlineSubsystemGameSpy();
extern void AutoGenerateNamesOnlineSubsystemPC();
extern void AutoGenerateNamesOnlineSubsystemSteamworks();
extern void AutoGenerateNamesOnlineSubsystemGameCenter();

#if GAMENAME == GEARGAME
extern void AutoGenerateNamesGearGame();
extern void AutoGenerateNamesGearEditor();

extern void AutoGenerateNamesFortress();
extern void AutoGenerateNamesFortressEditor();

#elif GAMENAME == FORTRESSGAME

extern void AutoGenerateNamesFortressBase();
extern void AutoGenerateNamesFortress();
extern void AutoGenerateNamesFortressEditor();

#elif GAMENAME == UTGAME
extern void AutoGenerateNamesUDKBase();
extern void AutoGenerateNamesUTEditor();

#elif GAMENAME == DUKEGAME
extern void AutoGenerateNamesDukeGame();
extern void AutoGenerateNamesDukeEditor();

#elif GAMENAME == EXOGAME
extern void AutoGenerateNamesExoGame();
extern void AutoGenerateNamesExoEditor();

#elif GAMENAME == SWORDGAME
extern void AutoGenerateNamesSwordGame();
extern void AutoGenerateNamesSwordEditor();
#endif

#if WITH_GFx
	extern void AutoGenerateNamesGFxUI();
	extern void AutoGenerateNamesGFxUIEditor();
#endif // WITH_GFx

#if WITH_SUBSTANCE_AIR
extern void AutoGenerateNamesSubstanceAir();

#if WITH_EDITOR == 1
extern void AutoGenerateNamesSubstanceAirEd();
#endif // WITH_EDITOR == 1
#endif

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

#define SPAWN_CHILD_PROCESS_TO_COMPILE 0

// General.
#if _WINDOWS && !CONSOLE
extern "C" {HINSTANCE hInstance;}
#endif

#if PS3
// we use a different GPackage here so that PS3 will use PS3Launch.log to not conflict with PC's
// Launch.log when using -writetohost
extern "C" {TCHAR GPackage[64]=TEXT("PS3Launch");}
#elif NGP
extern "C" {TCHAR GPackage[64]=TEXT("NGPLaunch");}
#elif WIIU
extern "C" {TCHAR GPackage[64]=TEXT("WiiULaunch");}
#else
extern "C" {TCHAR GPackage[64]=TEXT("Launch");}
#endif

/** Critical section used by MallocThreadSafeProxy for synchronization										*/

#if XBOX
/** Remote debug command																					*/
static CHAR							RemoteDebugCommand[1024];		
/** Critical section for accessing remote debug command														*/
static FCriticalSection				RemoteDebugCriticalSection;	
#endif



/** Helper function called on first allocation to create and initialize GMalloc */
void GCreateMalloc()
{
#if PS3
	#include "FMallocPS3.h"

	// give malloc a buffer to use instead of calling malloc internally
	static char buf[0x8000];
	setvbuf(stdout, buf, _IOLBF, sizeof(buf));

#if USE_FMALLOC_POOLED
	FMallocPS3Pooled::StaticInit();
	static FMallocPS3Pooled MallocPS3;
#endif

#if USE_FMALLOC_DL
	FMallocPS3DL::StaticInit();
	static FMallocPS3DL MallocPS3;
#endif

	// point to the static object
	GMalloc = &MallocPS3;

#elif defined(XBOX)
	GMalloc = new FMallocXenonPooled();
#elif _WIN64
	GMalloc = new FMallocTBB();
#elif IPHONE
	GMalloc = new FMallocIPhoneDL();
#elif ANDROID
	GMalloc = new FMallocAnsi();
#elif FLASH
	GMalloc = new FMallocAnsi();
#elif NGP
//	#include "FMallocNGP.h"
	GMalloc = new FMallocNGPDL();
#elif PLATFORM_MACOSX
	GMalloc = new FMallocAnsi();
#elif PLATFORM_UNIX
	GMalloc = new FMallocAnsi();
#elif WIIU
	GMalloc = new FMallocBinned();
#elif _DEBUG && !USE_MALLOC_PROFILER
	GMalloc = new FMallocDebug();
#elif _WINDOWS
	GMalloc = new FMallocBinned();
#else
	#error Please define your platform.
#endif

// so now check to see if we are using a Mem Profiler which wraps the GMalloc
#if TRACK_MEM_USING_TAGS
#if USE_MALLOC_PROFILER || TRACK_MEM_USING_STAT_SECTIONS
	#error Only a single memory allocator should be used at once.  There are multiple defined
#endif	//#if USE_MALLOC_PROFILER || TRACK_MEM_USING_STAT_SECTIONS
	GMalloc = new FMallocProxySimpleTag( GMalloc );
#elif TRACK_MEM_USING_STAT_SECTIONS
#if USE_MALLOC_PROFILER
	warnf(TEXT("Running with multiple allocators defined!"));
#endif	//#if USE_MALLOC_PROFILER
	GMalloc = new FMallocProxySimpleTrack( GMalloc );
#elif USE_MALLOC_PROFILER
	#if (_WINDOWS && !(defined(_DEBUG)) && !LOAD_SYMBOLS_FOR_STACK_WALKING)
		#pragma message("**** MallocProfiler not supported in this configuration!**** ")
	#endif
	GMallocProfiler = new FMallocProfilerEx( GMalloc );
	GMallocProfiler->BeginProfiling();
	GMalloc = GMallocProfiler;
#endif

	// if the allocator is already thread safe, there is no need for the thread safe proxy
	if (!GMalloc->IsInternallyThreadSafe())
	{
		GMalloc = new FMallocThreadSafeProxy( GMalloc );
	}
}

#if defined(XBOX)
static FOutputDeviceFile					Log;
static FOutputDeviceAnsiError				Error;
static FFeedbackContextAnsi					GameWarn;
static FFileManagerXenon					FileManager;
#elif PS3
#include "PS3DownloadableContent.h"
static FSynchronizeFactoryPU				SynchronizeFactory;
static FThreadFactoryPU						ThreadFactory;
static FOutputDeviceFile					Log;
static FOutputDeviceAnsiError				Error;
static FFeedbackContextAnsi					GameWarn;
static FFileManagerPS3						FileManager;
static FQueuedThreadPoolPS3					ThreadPool;
#elif NGP
static FNGPSynchronizeFactory				SynchronizeFactory;
static FNGPThreadFactory						ThreadFactory;
static FOutputDeviceFile					Log;
static FOutputDeviceAnsiError				Error;
static FFeedbackContextAnsi					GameWarn;
static FFileManagerNGP						FileManager;
static FNGPQueuedThreadPool					ThreadPool;
#elif IPHONE
static FOutputDeviceFile					Log;
static FOutputDeviceAnsiError				Error;
static FFeedbackContextAnsi					GameWarn;
#if WITH_UE3_NETWORKING && (!FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE)
#include "FFileManagerNetwork.h"
static FFileManagerIPhone					FileManagerBase;
static FFileManagerNetwork					FileManager(&FileManagerBase);
#else
static FFileManagerIPhone					FileManager;
#endif
static FSynchronizeFactoryIPhone			SynchronizeFactory;
static FThreadFactoryIPhone					ThreadFactory;
static FQueuedThreadPoolIPhone				ThreadPool;
#elif ANDROID
static FOutputDeviceFile					Log;
static FOutputDeviceAnsiError				Error;
static FFeedbackContextAnsi					GameWarn;
static FFileManagerAndroid					FileManager;
static FSynchronizeFactoryAndroid			SynchronizeFactory;
static FThreadFactoryAndroid				ThreadFactory;
static FQueuedThreadPoolAndroid				ThreadPool;
#elif PLATFORM_MACOSX
static FOutputDeviceFile					Log;
static FOutputDeviceAnsiError				Error;
static FFeedbackContextAnsi					GameWarn;
static FFileManagerMac						FileManager;
static FSynchronizeFactoryMac				SynchronizeFactory;
static FThreadFactoryMac					ThreadFactory;
static FQueuedThreadPoolMac					ThreadPool;
#elif FLASH
static FOutputDeviceFile					Log;
static FOutputDeviceAnsiError				Error;
static FFeedbackContextAnsi					GameWarn;
static FFileManagerFlash					FileManager;
static FSynchronizeFactoryFlash				SynchronizeFactory;
static FThreadFactoryFlash					ThreadFactory;
static FQueuedThreadPoolFlash				ThreadPool;
#elif WIIU
static FOutputDeviceFile					Log;
static FOutputDeviceAnsiError				Error;
static FFeedbackContextAnsi					GameWarn;
static FFileManagerWiiU						FileManager;
static FSynchronizeFactoryWiiU				SynchronizeFactory;
static FThreadFactoryWiiU					ThreadFactory;
static FQueuedThreadPoolWiiU				ThreadPool;
#else

#if WITH_EDITOR
#include "FFeedbackContextEditor.h"
static FFeedbackContextWindows				GameWarn;
static FFeedbackContextEditor				UnrealEdWarn;
#else
static FFeedbackContextAnsi					GameWarn; 
#endif  //WITH_EDITOR
static FOutputDeviceFile					Log;
static FOutputDeviceWindowsError			Error;
static FFileManagerWindows					FileManager;
static FOutputDeviceConsoleWindows			LogConsole;
static FOutputDeviceConsoleWindowsInherited InheritedLogConsole(LogConsole);
static FSynchronizeFactoryWin				SynchronizeFactory;
static FThreadFactoryWin					ThreadFactory;
static FQueuedThreadPoolWin					ThreadPool;
static FQueuedThreadPoolWin					HiPriThreadPool;
#endif

#if !FINAL_RELEASE
	#if WITH_UE3_NETWORKING
		// this will allow the game to receive object propagation commands from the network
		FListenPropagator					ListenPropagator;
	#endif	//#if WITH_UE3_NETWORKING
#endif

extern	TCHAR								GCmdLine[16384];
/** Whether we are using wxWindows or not */
extern	UBOOL								GUsewxWindows;

/** Whether to log all asynchronous IO requests out */
extern UBOOL GbLogAsyncLoading;

/** Thread used for async IO manager */
FRunnableThread*							AsyncIOThread = NULL;

/** 
* if TRUE then FDeferredUpdateResource::UpdateResources needs to be called 
* (should only be set on the rendering thread)
*/
UBOOL FDeferredUpdateResource::bNeedsUpdate = TRUE;

/** Whether force feedback should be enabled or disabled. */
UBOOL GEnableForceFeedback = TRUE;

#if _WINDOWS
#include "..\..\Launch\Resources\resource.h"
/** Resource ID of icon to use for Window */
#if UDK
INT			GGameIcon	= IDICON_UDKGame;
INT			GEditorIcon	= IDICON_UDKEditor;
#elif GAMENAME == GEARGAME
INT			GGameIcon	= IDICON_GoW;
INT			GEditorIcon	= IDICON_GoWEditor;
#elif GAMENAME == FORTRESSGAME
INT			GGameIcon	= IDICON_FortressGame;
INT			GEditorIcon	= IDICON_FortressGameEditor;
#elif GAMENAME == UTGAME
INT			GGameIcon	= IDICON_UTGame;
INT			GEditorIcon	= IDICON_UTEditor;
#elif GAMENAME == DUKEGAME
INT			GGameIcon	= IDICON_DemoGame;
INT			GEditorIcon	= IDICON_DemoEditor;
#elif GAMENAME == EXOGAME
INT			GGameIcon	= IDICON_DemoGame;
INT			GEditorIcon	= IDICON_DemoEditor;
#elif GAMENAME == SWORDGAME
INT			GGameIcon	= IDICON_DemoGame;
INT			GEditorIcon	= IDICON_DemoEditor;
#else
	#error Hook up your game name here
#endif

#include "../Debugger/UnDebuggerCore.h"
#endif

#if USE_BINK_CODEC
#include "../Bink/Src/FullScreenMovieBink.h"
#endif

/** global for full screen movie player */
FFullScreenMovieSupport* GFullScreenMovie = NULL;

/** Globally shared viewport for fullscreen movie playback. */
FViewport* GFullScreenMovieViewport = NULL;

/** Initialize the global full screen movie player */
void appInitFullScreenMoviePlayer()
{
	UBOOL bUseSound = !(ParseParam(appCmdLine(),TEXT("nosound")) || GIsBenchmarking);

#if !IPHONE
	// GFullScreenMovie is already initialized much earlier on iOS, 
	// so we can't disable loading movies at this point
	check( GFullScreenMovie == NULL );

	// check for disabling of movies from ini file
	UBOOL bForceNoMovies = FALSE;
	if (GConfig != NULL)
	{
		GConfig->GetBool(TEXT("FullScreenMovie"), TEXT("bForceNoMovies"), bForceNoMovies, GEngineIni);
	}
	// handle disabling of movies
	if( appStrfind(GCmdLine, TEXT("nomovie")) != NULL || GIsEditor || !GIsGame || bForceNoMovies || ParseParam( appCmdLine(), TEXT("es2") )|| ParseParam( appCmdLine(), TEXT("simmobile") ) )
	{
		GFullScreenMovie = FFullScreenMovieFallback::StaticInitialize(bUseSound);
	}
	else
#endif
	{
#if USE_BINK_CODEC
		GFullScreenMovie = FFullScreenMovieBink::StaticInitialize(bUseSound);
	#if XBOX
		GFullScreenMovieViewport = new FXenonViewport(NULL,NULL,TEXT("Movie"),GScreenWidth,GScreenHeight,TRUE);	
	#elif PS3
		GFullScreenMovieViewport = new FPS3Viewport(NULL,NULL,GScreenWidth,GScreenHeight);
	#endif
#elif IPHONE
		if (GFullScreenMovie == NULL)
		{
			GFullScreenMovie = FFullScreenMovieIPhone::StaticInitialize(bUseSound);
		}
#elif WITH_GFx_FULLSCREEN_MOVIE
		GFullScreenMovie = FFullScreenMovieGFx::StaticInitialize(bUseSound);
#elif ANDROID
		if (GFullScreenMovie == NULL)
		{
			GFullScreenMovie = FAndroidFullScreenMovie::StaticInitialize(bUseSound);
		}
#else		
		GFullScreenMovie = FFullScreenMovieFallback::StaticInitialize(bUseSound);
#endif
	}
	check( GFullScreenMovie != NULL );
}

#if WITH_EDITOR
// From UMakeCommandlet.cpp - See if scripts need rebuilding at runtime
extern UBOOL AreScriptPackagesOutOfDate();
#endif

// Lets each platform perform any post appInit processing
extern void appPlatformPostInit();

#if GAMENAME != GEARGAME && GAMENAME != FORTRESSGAME
/** Starts up performance counters */
void appPerfCountersInit()
{
}

/** Chance for performance counters to update on dedicated servers */
void appPerfCountersUpdate()
{
}

/** Shuts down performance counters */
void appPerfCountersCleanup()
{
}
#endif

#if WITH_PANORAMA || _XBOX
/**
 * Returns the title id for this game
 * Licensees need to add their own game(s) here!
 */
DWORD appGetTitleId(void)
{
#if GAMENAME == EXAMPLEGAME
	return 0;
#elif GAMENAME == FORTRESSGAME
	return 0;
#elif GAMENAME == UTGAME
	return 0x4D5707DB;
#elif GAMENAME == GEARGAME
	return 0x4D5308AB;
#elif GAMENAME == EXOGAME
	return 0;
#elif GAMENAME == SWORDGAME
	return 0;
#else
	#error Hook up the title id for your game here
#endif
}
#endif

#if WITH_GAMESPY
/**
 * Returns the game name to use with GameSpy
 * Licensees need to add their own game(s) here!
 */
const TCHAR* appGetGameSpyGameName(void)
{
#if GAMENAME == UTGAME
#if PS3
	static TCHAR GSGameName[7];
	GSGameName[0] = TEXT('u');
	GSGameName[1] = TEXT('t');
	GSGameName[2] = TEXT('3');
	GSGameName[3] = TEXT('p');
	GSGameName[4] = TEXT('s');
	GSGameName[5] = TEXT('3');
	GSGameName[6] = 0;
	return GSGameName;
#else
	static TCHAR GSGameName[7];
	GSGameName[0] = TEXT('u');
	GSGameName[1] = TEXT('t');
	GSGameName[2] = TEXT('3');
	GSGameName[3] = TEXT('p');
	GSGameName[4] = TEXT('c');
	GSGameName[5] = 0;
	return GSGameName;
#endif
#elif GAMENAME == GEARGAME
#if WITH_GAMESPY
	static TCHAR GSGameName[7];
	GSGameName[0] = TEXT('g');
	GSGameName[1] = TEXT('m');
	GSGameName[2] = TEXT('t');
	GSGameName[3] = TEXT('e');
	GSGameName[4] = TEXT('s');
	GSGameName[5] = TEXT('t');
	GSGameName[6] = 0;
//debugf(TEXT("appGetGameSpyGameName> returning %s"), GSGameName);
	return GSGameName;
#else
	return NULL;
#endif
#elif GAMENAME == DUKEGAME || GAMENAME == SWORDGAME
	static TCHAR GSGameName[7];
	GSGameName[0] = TEXT('g');
	GSGameName[1] = TEXT('m');
	GSGameName[2] = TEXT('t');
	GSGameName[3] = TEXT('e');
	GSGameName[4] = TEXT('s');
	GSGameName[5] = TEXT('t');
	GSGameName[6] = 0;
	//debugf(TEXT("appGetGameSpyGameName> returning %s"), GSGameName);
	return GSGameName;
#elif GAMENAME == EXOGAME
	static TCHAR GSGameName[7];
	GSGameName[0] = TEXT('g');
	GSGameName[1] = TEXT('m');
	GSGameName[2] = TEXT('t');
	GSGameName[3] = TEXT('e');
	GSGameName[4] = TEXT('s');
	GSGameName[5] = TEXT('t');
	GSGameName[6] = 0;
	//debugf(TEXT("appGetGameSpyGameName> returning %s"), GSGameName);
	return GSGameName;
#else
	#error Hook up your GameSpy game name here
#endif
}

/**
 * Returns the secret key used by this game
 * Licensees need to add their own game(s) here!
 */
const TCHAR* appGetGameSpySecretKey(void)
{
#if GAMENAME == UTGAME
	static TCHAR GSSecretKey[7];
	GSSecretKey[0] = TEXT('n');
	GSSecretKey[1] = TEXT('T');
	GSSecretKey[2] = TEXT('2');
	GSSecretKey[3] = TEXT('M');
	GSSecretKey[4] = TEXT('t');
	GSSecretKey[5] = TEXT('z');
	GSSecretKey[6] = 0;
	return GSSecretKey;
#elif GAMENAME == GEARGAME
#if PS3
	static TCHAR GSSecretKey[7];
	GSSecretKey[0] = TEXT('H');
	GSSecretKey[1] = TEXT('A');
	GSSecretKey[2] = TEXT('6');
	GSSecretKey[3] = TEXT('z');
	GSSecretKey[4] = TEXT('k');
	GSSecretKey[5] = TEXT('S');
	GSSecretKey[6] = 0;
debugf(TEXT("appGetGameSpySecretKey> returning %s"), GSSecretKey);
	return GSSecretKey;
#else
	return NULL;
#endif
#elif GAMENAME == DUKEGAME || GAMENAME == SWORDGAME
	return NULL;
#elif GAMENAME == EXOGAME
	return NULL;
#else
	#error Hook up the secret key for your game here
#endif
}
#endif

/**
 * The single function that sets the gamename based on the #define GAMENAME
 * Licensees need to add their own game(s) here!
 */
void appSetGameName()
{
	// Initialize game name
#if   GAMENAME == DUKEGAME
	appStrcpy(GGameName, TEXT("Duke"));
#elif GAMENAME == GEARGAME
	appStrcpy(GGameName, TEXT("Gear"));
#elif GAMENAME == UTGAME
	appStrcpy(GGameName, TEXT("UDK"));
#elif GAMENAME == EXOGAME
	appStrcpy(GGameName, TEXT("Exo"));
#elif GAMENAME == FORTRESSGAME
	appStrcpy(GGameName, TEXT("Fortress"));
#elif GAMENAME == SWORDGAME
	appStrcpy(GGameName, TEXT("Sword"));
#else
	#error Hook up your game name here
#endif
}

/**
 * Figure out which native OnlineSubsystem package to add to native package list (this is what is requested, if it 
 * doesn't exist, then OSSPC will be attempted as a fallback, and failing that, nothing will be used)
 *
 * NOTE: If this changes, you should also change the corresponding GetDesiredOnlineSubsystem function in UBT
 * 
 * @return The name of the OnlineSubystem package 
 */
const TCHAR* appGetOSSPackageName()
{
#if XBOX
	
	// xbox is always live
	return TEXT("Live");

#elif TEGRA || NGP

	// basic OSS for mobiles at the moment
	return TEXT("PC");

#elif PS3

	// is it compiled with GameSpy on?
	#if WITH_GAMESPY
		return TEXT("GameSpy");
	#else
		return TEXT("PC");
	#endif

#elif IPHONE 
	
	#if WITH_GAMECENTER
		// make sure the runtime supports GC, no independent of how it was compiled
		if (IPhoneDynamicCheckForGameCenter())
		{
			return TEXT("GameCenter");
		}
	#endif
		return NULL;

#elif ANDROID || NGP || FLASH

	// basic OSS for others at the moment
	return TEXT("PC");

#elif PLATFORM_MACOSX

	#if WITH_STEAMWORKS
		return TEXT("Steamworks");
	#elif WITH_GAMESPY
		return TEXT("GameSpy");
	#else
		return TEXT("PC");
	#endif

#elif _WINDOWS 

	// handle special cases for cooking to consoles to cook appropriate target OSS, 
	// we only do consoles because cooking for Windows uses the same executable as
	// the runtime, so it uses the WITH_* checks below this if
	if (GIsCooking && (GCookingTarget & UE3::PLATFORM_Console))
	{
		switch (GCookingTarget)
		{
			case UE3::PLATFORM_Xbox360:
				// xbox always uses Live
				return TEXT("Live");

			case UE3::PLATFORM_PS3:
				#if GAMENAME == UTGAME
					// UT uses GameSpy, others use PC
					return TEXT("GameSpy");
				#else
					return TEXT("PC");
				#endif

			case UE3::PLATFORM_IPhone:
				// always cook GC, but if it's not compiled in or not allowed, it won't be used at runtime
				return TEXT("GameCenter");

			case UE3::PLATFORM_Android:
			case UE3::PLATFORM_NGP:
			case UE3::PLATFORM_WiiU:
			case UE3::PLATFORM_Flash:
				// only basic OSS for others
				return TEXT("PC");

			default:
				appErrorf(TEXT("Specify the console target's OnlineSubsystem"));
		}
	}
	else if (GIsCooking && (GCookingTarget & UE3::PLATFORM_WindowsServer))
	{
		#if GAMENAME == GEARGAME
			return TEXT("Live");
		#endif
	}

	// if we are running the game, or cooking for Windows, we check for how the code was compiled
	#if _WINDLL
		// PIB has no OSS
		return NULL;
	#elif WITH_STEAMWORKS
		return TEXT("Steamworks");
	#elif WITH_PANORAMA
		// allow for a commandline option to disable all OSS entirely
		if (ParseParam(appCmdLine(), TEXT("NOLIVE")))
		{
			return NULL;
		}
		return TEXT("Live");
	#elif WITH_GAMESPY
		return TEXT("GameSpy");
	#else
		return TEXT("PC");
	#endif

#endif // WINDOWS

	return NULL;
}

/**
 * A single function to get the list of the script packages that are used by 
 * the current game (as specified by the GAMENAME #define). This is not the same as
 * EditPackages, which can be used to compile more script packages that this
 * function will return.
 *
 * @param PackageNames					The output array that will contain the package names for this game (with no extension)
 * @param ScriptTypes			The set of script packages to return - returned in the order shown in EScriptPackageTypes (except SeekfreeLoc which is put in before the corresponding package)
 * @param EngineConfigFilename	Optional name of an engine ini file (this is used when cooking for another platform)
 */
void appGetScriptPackageNames(TArray<FString>& PackageNames, UINT ScriptTypes, const TCHAR* EngineConfigFilename)
{
	checkf(GConfig, TEXT("appGetGameNativeScriptPackageNames() has been called before GConfig has been set up. This is not allowed"));

#if CONSOLE || PLATFORM_UNIX || !WITH_EDITOR
	// consoles and unix NEVER want to load editor only packages
	ScriptTypes = ScriptTypes & ~SPT_Editor;
#endif

	// if not specified, use the default
	if (EngineConfigFilename == NULL)
	{
		EngineConfigFilename = GEngineIni;
	}

	// get the list of the base engine native packages
	if (ScriptTypes & SPT_EngineNative)
	{
		TArray<FString> EngineNativePackages;
		GConfig->GetArray(TEXT("Engine.ScriptPackages"), TEXT("EngineNativePackages"), EngineNativePackages, EngineConfigFilename);
		PackageNames.Append(EngineNativePackages);

#if WITH_UE3_NETWORKING
		// only include this if we are not checking native class sizes.  Currently, the native classes which reside on specific console
		// platforms will cause native class size checks to fail even tho class sizes are hopefully correct on the target platform as the PC 
		// doesn't have access to that native class.
		if( ParseParam(appCmdLine(),TEXT("CHECK_NATIVE_CLASS_SIZES")) == FALSE )
		{
			// figure out the desired package name
			const TCHAR* OSSName = appGetOSSPackageName();

			// OSSName is allowed to be NULL, which means to load no OSS class (or package)
			if (OSSName)
			{
				FString OSSPackage = FString(TEXT("OnlineSubsystem")) + OSSName;

				// make sure the package exists on disk
				FString PackagePath;
				if (GPackageFileCache->FindPackageFile(*OSSPackage, NULL, PackagePath))
				{
					PackageNames.AddItem(OSSPackage);
				}
			}
		}
#else
		// get the list of packages needed for networking (note, this does not include the OSS script package
		// because that cannot be ini driven, it needs to come from appGetOSSPackageName(), below)
		TArray<FString> NetworkingPackages;
		GConfig->GetArray(TEXT("Engine.ScriptPackages"), TEXT("NetNativePackages"), NetworkingPackages, EngineConfigFilename);

		for (INT PackageIndex = 0; PackageIndex < NetworkingPackages.Num(); PackageIndex++)
		{
			PackageNames.RemoveItem(NetworkingPackages(PackageIndex));
		}
#endif
	}

	// now add the game native packages, these need to come after IpDrv, etc above during cooking, so
	// they are split up from the engine native packages
	if (ScriptTypes & SPT_GameNative)
	{
		TArray<FString> GameNativePackages;
		GConfig->GetArray(TEXT("Engine.ScriptPackages"), TEXT("NativePackages"), GameNativePackages, EngineConfigFilename);
		PackageNames.Append(GameNativePackages);

		// insert any localization for Seek free packages if requested
		if (ScriptTypes & SPT_SeekfreeLoc)
		{
			for( INT PackageIndex=0; PackageIndex<PackageNames.Num(); PackageIndex++ )
			{
				// only insert localized package if it exists
				FString LocalizedPackageName = PackageNames(PackageIndex) + LOCALIZED_SEEKFREE_SUFFIX;
				FString LocalizedFileName;
				if( !GUseSeekFreeLoading || GPackageFileCache->FindPackageFile( *LocalizedPackageName, NULL, LocalizedFileName ) )
				{
					PackageNames.InsertItem(*LocalizedPackageName, PackageIndex);
					// skip over the package that we just localized
					PackageIndex++;
				}
			}
		}
	}

	// get the list of editor-only packages
	if (ScriptTypes & SPT_Editor)
	{
		TArray<FString> EditorPackages;
		GConfig->GetArray(TEXT("Engine.ScriptPackages"), TEXT("EditorPackages"), EditorPackages, EngineConfigFilename);
		PackageNames.Append(EditorPackages);

#if WITH_GFx
		TArray<FString> ScaleformEditorPackages;
		GConfig->GetArray(TEXT("Engine.ScriptPackages"), TEXT("ScaleformEditorPackages"), ScaleformEditorPackages, EngineConfigFilename);
		PackageNames.Append(ScaleformEditorPackages);
#endif
	}

	if (ScriptTypes & SPT_NonNative)
	{
		// get the list of non-native packages for this game (these will never be base engine packages)
		TArray<FString> NonNativePackages;
		GConfig->GetArray(TEXT("Engine.ScriptPackages"), TEXT("NonNativePackages"), NonNativePackages, EngineConfigFilename);
		PackageNames.Append(NonNativePackages);

		// get the list of extra mod packages from the .ini (for UDK)
		TArray<FString> ModEditPackages;
		GConfig->GetArray(TEXT("UnrealEd.EditorEngine"), TEXT("ModEditPackages"), ModEditPackages, EngineConfigFilename);
		PackageNames.Append(ModEditPackages);
	}

#if !WITH_SUBSTANCE_AIR
	for (INT PackageIndex = 0; PackageIndex < PackageNames.Num(); PackageIndex++)
	{
		if (PackageNames(PackageIndex).StartsWith(TEXT("Substance")))
		{
			PackageNames.Remove(PackageIndex--);
		}
	}
#endif
}


/**
 * Gets the list of packages that are precached at startup for seek free loading
 *
 * @param PackageNames The output list of package names
 * @param EngineConfigFilename Optional engine config filename to use to lookup the package settings
 */
void GetNonNativeStartupPackageNames(TArray<FString>& PackageNames, const TCHAR* EngineConfigFilename=NULL, UBOOL bIsCreatingHashes=FALSE)
{
	// if we aren't cooking, we actually just want to use the cooked startup package as the only startup package
	if (bIsCreatingHashes || (!GIsUCC && !GIsEditor && GUseSeekFreeLoading))
	{
		// make sure the startup package exists
		PackageNames.AddItem(FString(TEXT("Startup_LOC")));
		PackageNames.AddItem(FString(TEXT("Startup")));
	}
	else
	{
		// look for any packages that we want to force preload at startup
		FConfigSection* PackagesToPreload = GConfig->GetSectionPrivate( TEXT("Engine.StartupPackages"), 0, 1, 
			EngineConfigFilename ? EngineConfigFilename : GEngineIni );
		if (PackagesToPreload)
		{
			// go through list and add to the array
			for( FConfigSectionMap::TIterator It(*PackagesToPreload); It; ++It )
			{
				if (It.Key() == TEXT("Package"))
				{
					// add this package to the list to be fully loaded later
					PackageNames.AddItem(*(It.Value()));
				}
			}
		}
	}
}

/**
 * Kicks off a list of packages to be read in asynchronously in the background by the
 * async file manager. The package will be serialized from RAM later.
 * 
 * @param PackageNames The list of package names to async preload
 */
void AsyncPreloadPackageList(const TArray<FString>& PackageNames)
{
	// Iterate over all native script packages and preload them.
	for (INT PackageIndex=0; PackageIndex<PackageNames.Num(); PackageIndex++)
	{
		// let ULinkerLoad class manage preloading
		ULinkerLoad::AsyncPreloadPackage(*PackageNames(PackageIndex));
	}
}

/**
 * Fully loads a list of packages.
 * 
 * @param PackageNames The list of package names to load
 */
void LoadPackageList(const TArray<FString>& PackageNames)
{
	// Iterate over all native script packages and fully load them.
	for( INT PackageIndex=0; PackageIndex<PackageNames.Num(); PackageIndex++ )
	{
		UObject* Package = UObject::LoadPackage(NULL, *PackageNames(PackageIndex), LOAD_None);
	}
}

/**
 * This will load up all of the various "core" .u packages.
 * 
 * We do this as an optimization for load times.  We also do this such that we can be assured that 
 * all of the .u classes are loaded so we can then verify them.
 */
void LoadAllNativeScriptPackages()
{
	TArray<FString> PackageNames;

	// call the shared function to get all native script package names
	appGetScriptPackageNames(PackageNames, SPT_Native | (!GUseSeekFreeLoading ? SPT_Editor : 0));

	// load them
	LoadPackageList(PackageNames);
}

/**
 * This function will look at the given command line and see if we have passed in a map to load.
 * If we have then use that.
 * If we have not then use the DefaultLocalMap which is stored in the Engine.ini
 * 
 * @see UGameEngine::Init() for this method of getting the correct start up map
 *
 * @param CommandLine Commandline to use to get startup map (NULL or "" will return default startup map)
 *
 * @return Name of the startup map without an extension (so can be used as a package name)
 */
FString GetStartupMap(const TCHAR* CommandLine)
{
	FURL DefaultURL;
	DefaultURL.LoadURLConfig( TEXT("DefaultPlayer"), GGameIni );

	// convert commandline to a URL
	TCHAR Parm[4096]=TEXT("");
	const TCHAR* Tmp = CommandLine ? CommandLine : TEXT("");
	/*** CHAIR CHANGE for IB2, Always load into the default map, which will then load into the specified map */
	if (!GIsEditor || !ParseToken(Tmp, Parm, ARRAY_COUNT(Parm), 0) || Parm[0] == '-')
	{
		appStrcpy(Parm, *(FURL::DefaultLocalMap + FURL::DefaultLocalOptions));
	}
	FURL URL(&DefaultURL, Parm, TRAVEL_Partial);

	// strip off extension of the map if there is one
	return FFilename(URL.Map).GetBaseFilename();
}

/**
 * Load all startup packages. If desired preload async followed by serialization from memory.
 * Only native script packages are loaded from memory if we're not using the GUseSeekFreeLoading
 * codepath as we need to reset the loader on those packages and don't want to keep the bulk
 * data around. Native script packages don't have any bulk data so this doesn't matter.
 *
 * The list of additional packages can be found at Engine.StartupPackages and if seekfree loading
 * is enabled, the startup map is going to be preloaded as well.
 */
void LoadStartupPackages()
{
	DOUBLE StartTime = appSeconds();

	// should script packages load from memory?
	UBOOL bSerializeStartupPackagesFromMemory = FALSE;
	GConfig->GetBool(TEXT("Engine.StartupPackages"), TEXT("bSerializeStartupPackagesFromMemory"), bSerializeStartupPackagesFromMemory, GEngineIni);

	// Get all native script package names.
	TArray<FString> NativeScriptPackages;
	appGetScriptPackageNames(NativeScriptPackages, SPT_Native | (!GUseSeekFreeLoading ? SPT_Editor : SPT_SeekfreeLoc));

	// Get list of non-native startup packages.
	TArray<FString> NonNativeStartupPackages;
	
	// if the user wants to skip loading these, then don't (can be helpful for deleting objects in startup packages in the editor, etc)
	if (!ParseParam(appCmdLine(), TEXT("NoLoadStartupPackages")))
	{
		GetNonNativeStartupPackageNames(NonNativeStartupPackages);
	}

	if( bSerializeStartupPackagesFromMemory )
	{
		// start preloading them
		AsyncPreloadPackageList(NativeScriptPackages);

		if( GUseSeekFreeLoading )
		{
			// kick them off to be preloaded
			AsyncPreloadPackageList(NonNativeStartupPackages);
		}
	}

	// Load the native script packages.
	LoadPackageList(NativeScriptPackages);

	if( !GUseSeekFreeLoading && !GIsEditor )
	{
		// Reset loaders on native script packages as they are always fully loaded. This ensures the memory
		// for the async preloading process can be reclaimed.
		for( INT PackageIndex=0; PackageIndex<NativeScriptPackages.Num(); PackageIndex++ )
		{
			const FString& PackageName = NativeScriptPackages(PackageIndex);
			UPackage* Package = FindObjectChecked<UPackage>(NULL,*PackageName,TRUE);
			UObject::ResetLoaders( Package );
		}
	}
#if !CONSOLE
	// with PC seekfree, the shadercaches are loaded specially, they aren't cooked in, so at 
	// this point, their linkers are still in memory and should be reset
	else
	{
		for (TObjectIterator<UShaderCache> It; It; ++It)
		{
			UObject::ResetLoaders(It->GetOutermost());
		}
	}
#endif

	// Load the other startup packages.
	LoadPackageList(NonNativeStartupPackages);

	debugf(NAME_Init,TEXT("Finished loading startup packages in %.2f seconds"), appSeconds() - StartTime);
}

/**
 * Get a list of all packages that may be needed at startup, and could be
 * loaded async in the background when doing seek free loading
 *
 * @param PackageNames The output list of package names
 * @param EngineConfigFilename Optional engine config filename to use to lookup the package settings
 */
void appGetAllPotentialStartupPackageNames(TArray<FString>& PackageNames, const TCHAR* EngineConfigFilename, UBOOL bIsCreatingHashes)
{
	// native script packages
	appGetScriptPackageNames(PackageNames, SPT_Native | (!GUseSeekFreeLoading ? SPT_Editor : 0) | SPT_SeekfreeLoc);
	// startup packages from .ini
	GetNonNativeStartupPackageNames(PackageNames, EngineConfigFilename, bIsCreatingHashes);
	// add the startup map
	PackageNames.AddItem(*GetStartupMap(NULL));

	// Get a list of known language extensions
	const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();

	// go through and add localized versions of each package for all known languages
	// since they could be used at runtime depending on the language at runtime
	INT NumPackages = PackageNames.Num();
	for (INT PackageIndex = 0; PackageIndex < NumPackages; PackageIndex++)
	{
		FString PkgName = PackageNames(PackageIndex);
		if (PkgName.EndsWith(LOCALIZED_SEEKFREE_SUFFIX))
		{
			// add the packagename with _XXX language extension
			for (INT LangIndex = 0; LangIndex < KnownLanguageExtensions.Num(); LangIndex++)
			{
				if (LangIndex == 0)
				{
					// Inline replace the PACKAGE_LOC case
					PackageNames(PackageIndex) = PkgName + TEXT("_") + KnownLanguageExtensions(LangIndex);
				}
				else
				{
					// Append the new loc package to the end of the list
					PackageNames.AddItem(*(PkgName + TEXT("_") + KnownLanguageExtensions(LangIndex)));
				}
			}
		}
	}
}

/**
* Add a new entry to the list of shader source files
* Only unique entries which can be loaded are added as well as their #include files
*
* @param ShaderSourceFiles - [out] list of shader source files to add to
* @param ShaderFilename - shader file to add
*/
static void AddShaderSourceFileEntry( TArray<FString>& ShaderSourceFiles, const FString& ShaderFilename)
{
	FString ShaderFilenameBase( FFilename(ShaderFilename).GetBaseFilename() );

	// get the filename for the the vertex factory type
	if( !ShaderSourceFiles.ContainsItem(ShaderFilenameBase) )
	{
		ShaderSourceFiles.AddItem(ShaderFilenameBase);

		TArray<FString> ShaderIncludes;
		GetShaderIncludes(*ShaderFilenameBase,ShaderIncludes);
		for( INT IncludeIdx=0; IncludeIdx < ShaderIncludes.Num(); IncludeIdx++ )
		{
			ShaderSourceFiles.AddUniqueItem(ShaderIncludes(IncludeIdx));
		}
	}
}

/**
* Generate a list of shader source files that engine needs to load
*
* @param ShaderSourceFiles - [out] list of shader source files to add to
*/
void appGetAllShaderSourceFiles( TArray<FString>& ShaderSourceFiles )
{
	// add all shader source files for hashing
	for( TLinkedList<FVertexFactoryType*>::TIterator FactoryIt(FVertexFactoryType::GetTypeList()); FactoryIt; FactoryIt.Next() )
	{
		FVertexFactoryType* VertexFactoryType = *FactoryIt;
		if( VertexFactoryType )
		{
			FString ShaderFilename(VertexFactoryType->GetShaderFilename());
			if (ShaderFilename.InStr(TEXT("CommonDepth"), FALSE, TRUE) != INDEX_NONE)
			{
				int dummy = 0;
				dummy++;
			}
			AddShaderSourceFileEntry(ShaderSourceFiles,ShaderFilename);
		}
	}
	for( TLinkedList<FShaderType*>::TIterator ShaderIt(FShaderType::GetTypeList()); ShaderIt; ShaderIt.Next() )
	{
		FShaderType* ShaderType = *ShaderIt;
		// don't process NGP global shaders
		if( ShaderType && ShaderType->GetNGPGlobalShaderType() == NULL )
		{
			FString ShaderFilename(ShaderType->GetShaderFilename());
			if (ShaderFilename.InStr(TEXT("CommonDepth"), FALSE, TRUE) != INDEX_NONE)
			{
				int dummy = 0;
				dummy++;
			}
			AddShaderSourceFileEntry(ShaderSourceFiles,ShaderFilename);
		}
	}
	// also always add the MaterialTemplate.usf shader file
	AddShaderSourceFileEntry(ShaderSourceFiles,FString(TEXT("MaterialTemplate")));
	AddShaderSourceFileEntry(ShaderSourceFiles,FString(TEXT("Common")));
	AddShaderSourceFileEntry(ShaderSourceFiles,FString(TEXT("Definitions")));	
}

/**
* Kick off SHA verification for all shader source files
*/
void VerifyShaderSourceFiles()
{
	// get the list of shader files that can be used
	TArray<FString> ShaderSourceFiles;
	appGetAllShaderSourceFiles(ShaderSourceFiles);
	for( INT ShaderFileIdx=0; ShaderFileIdx < ShaderSourceFiles.Num(); ShaderFileIdx++ )
	{
		// load each shader source file. This will cache the shader source data after it has been verified
		LoadShaderSourceFile(*ShaderSourceFiles(ShaderFileIdx));
	}
}

/**
 * Checks for native class script/ C++ mismatch of class size and member variable
 * offset. Note that only the first and last member variable of each class in addition
 * to all member variables of noexport classes are verified to work around a compiler
 * limitation. The code is disabled by default as it has quite an effect on compile
 * time though is an invaluable tool for sanity checking and bringing up new
 * platforms.
 */
void CheckNativeClassSizes()
{
#if CHECK_NATIVE_CLASS_SIZES  // pass in via /DCHECK_NATIVE_CLASS_SIZES or set CL=/DCHECK_NATIVE_CLASS_SIZES and then this will be activated.  Good for setting up on your continuous integration machine
	debugf(TEXT("CheckNativeClassSizes..."));
	UBOOL Mismatch = FALSE;

	extern void AutoCheckNativeClassSizesCore( UBOOL& Mismatch );
	AutoCheckNativeClassSizesCore( Mismatch );

	extern void AutoCheckNativeClassSizesEngine( UBOOL& Mismatch );
	AutoCheckNativeClassSizesEngine( Mismatch );

	extern void AutoCheckNativeClassSizesGameFramework( UBOOL& Mismatch );
	AutoCheckNativeClassSizesGameFramework( Mismatch );

#if WITH_GFx
	extern void AutoCheckNativeClassSizesGFxUI( UBOOL& Mismatch );
	AutoCheckNativeClassSizesGFxUI( Mismatch );

	#if !CONSOLE && !PLATFORM_MACOSX
		extern void AutoCheckNativeClassSizesGFxUIEditor( UBOOL& Mismatch );
		AutoCheckNativeClassSizesGFxUIEditor( Mismatch );
	#endif

#endif

#if !CONSOLE && !PLATFORM_MACOSX
	extern void AutoCheckNativeClassSizesUnrealEd( UBOOL& Mismatch );
	AutoCheckNativeClassSizesUnrealEd( Mismatch );
#endif

#if WITH_UE3_NETWORKING
	extern void AutoCheckNativeClassSizesIpDrv( UBOOL& Mismatch );
	AutoCheckNativeClassSizesIpDrv( Mismatch );

	extern void AutoCheckNativeClassSizesWinDrv( UBOOL& Mismatch );
	AutoCheckNativeClassSizesWinDrv( Mismatch );
#endif	//#if WITH_UE3_NETWORKING

#if GAMENAME == GEARGAME
	extern void AutoCheckNativeClassSizesGearGame( UBOOL& Mismatch );
	AutoCheckNativeClassSizesGearGame( Mismatch );

#if !CONSOLE && !PLATFORM_MACOSX
	extern void AutoCheckNativeClassSizesGearEditor( UBOOL& Mismatch );
	AutoCheckNativeClassSizesGearEditor( Mismatch );
#endif



#elif GAMENAME == FORTRESSGAME

	extern void AutoCheckNativeClassSizesFortressBase( UBOOL& Mismatch );
	AutoCheckNativeClassSizesFortressBase( Mismatch );

	extern void AutoCheckNativeClassSizesFortress( UBOOL& Mismatch );
	AutoCheckNativeClassSizesFortress( Mismatch );

#if !CONSOLE && !PLATFORM_MACOSX
	extern void AutoCheckNativeClassSizesFortressEditor( UBOOL& Mismatch );
	AutoCheckNativeClassSizesFortressEditor( Mismatch );
#endif



#elif GAMENAME == UTGAME

	extern void AutoCheckNativeClassSizesUDKBase( UBOOL& Mismatch );
	AutoCheckNativeClassSizesUDKBase( Mismatch );

#if !CONSOLE && !PLATFORM_MACOSX
	extern void AutoCheckNativeClassSizesUTEditor( UBOOL& Mismatch );
	AutoCheckNativeClassSizesUTEditor( Mismatch );
#endif

#elif GAMENAME == DUKEGAME
	extern void AutoCheckNativeClassSizesDukeGame( UBOOL& Mismatch );
	AutoCheckNativeClassSizesDukeGame( Mismatch );

#if !CONSOLE && !PLATFORM_MACOSX
	extern void AutoCheckNativeClassSizesExampleEditor( UBOOL& Mismatch );
	AutoCheckNativeClassSizesExampleEditor( Mismatch );
#endif

#elif GAMENAME == EXOGAME
	extern void AutoCheckNativeClassSizesExoGame( UBOOL& Mismatch );
	AutoCheckNativeClassSizesExoGame( Mismatch );

#if !CONSOLE && !PLATFORM_MACOSX
	extern void AutoCheckNativeClassSizesExoEditor( UBOOL& Mismatch );
	AutoCheckNativeClassSizesExoEditor( Mismatch );
#endif

#elif GAMENAME == SWORDGAME
	extern void AutoCheckNativeClassSizesSwordGame( UBOOL& Mismatch );
	AutoCheckNativeClassSizesSwordGame( Mismatch );

#if !CONSOLE && !PLATFORM_MACOSX
	extern void AutoCheckNativeClassSizesSwordEditor( UBOOL& Mismatch );
	AutoCheckNativeClassSizesSwordEditor( Mismatch );
#endif
#else
	#error Hook up your game name here
#endif


	if( ( Mismatch == TRUE ) && ( GIsUnattended == TRUE ) )
	{
		appErrorf( NAME_Error, *LocalizeUnrealEd("Error_ScriptClassSizeMismatch") );
	}
	else if( Mismatch == TRUE )
	{
		appErrorf( NAME_FriendlyError, *LocalizeUnrealEd("Error_ScriptClassSizeMismatch") );
	}
	else
	{
		debugf(TEXT("CheckNativeClassSizes completed with no errors"));
	}
#endif  // #if CHECK_NATIVE_CLASS_SIZES 
}


/**
 * Update GCurrentTime/ GDeltaTime while taking into account max tick rate.
 */
void appUpdateTimeAndHandleMaxTickRate()
{
	// start at now minus a bit so we don't get a zero delta.
	static DOUBLE LastTime = appSeconds() - 0.0001;
	static UBOOL bIsUsingFixedTimeStep = FALSE;

	// Figure out whether we want to use real or fixed time step.
	const UBOOL bUseFixedTimeStep = GIsBenchmarking || GUseFixedTimeStep;

	GLastTime = GCurrentTime;

	// Calculate delta time and update time.
	if( bUseFixedTimeStep )
	{
		bIsUsingFixedTimeStep = TRUE;

		GDeltaTime		= GFixedDeltaTime;
		LastTime		= GCurrentTime;
		GCurrentTime	+= GDeltaTime;
	}
	else
	{
		GCurrentTime = appSeconds();
#if LOG_WAIT_TIME_STATS
		DOUBLE OldCurrentTime = GCurrentTime;
#endif
		// Did we just switch from a fixed time step to real-time?  If so, then we'll update our
		// cached 'last time' so our current interval isn't huge (or negative!)
		if( bIsUsingFixedTimeStep )
		{
			LastTime = GCurrentTime - GDeltaTime;
			bIsUsingFixedTimeStep = FALSE;
		}

		// Calculate delta time.
		FLOAT DeltaTime = GCurrentTime - LastTime;

		// Negative delta time means something is wrong with the system. Error out so user can address issue.
		if( DeltaTime < 0 )
		{
#if MOBILE
			warnf(TEXT("Detected negative delta time: %f"), DeltaTime);
#else
			// AMD dual-core systems are a known issue that require AMD CPU drivers to be installed. Installer will take care of this for shipping.
			appErrorf(TEXT("Detected negative delta time - on AMD systems please install http://files.aoaforums.com/I3199-setup.zip.html"));
#endif
			DeltaTime = 0.01;
		}

		// Get max tick rate based on network settings and current delta time.
		const FLOAT MaxTickRate	= GEngine->GetMaxTickRate( DeltaTime );
		FLOAT WaitTime		= 0;
		// Convert from max FPS to wait time.
		if( MaxTickRate > 0 )
		{
			WaitTime = Max( 1.f / MaxTickRate - DeltaTime, 0.f );
		}

		// Enforce maximum framerate and smooth framerate by waiting.
		STAT( DOUBLE ActualWaitTime = 0.f ); 
		const DWORD IdleStart = appCycles();
		if( WaitTime > 0 )
		{
			DOUBLE WaitEndTime = GCurrentTime + WaitTime;
			SCOPE_SECONDS_COUNTER(ActualWaitTime);
			SCOPE_CYCLE_COUNTER(STAT_GameTickWaitTime);
			SCOPE_CYCLE_COUNTER(STAT_GameIdleTime);

#if !FLASH
			if (GSystemSettings.bAllowPerFrameSleep)
			{
				if (GIsHighPrecisionThreadingEnabled)
				{
					appSleep( WaitTime );
				}
				else
				{
					// Sleep if we're waiting more than 5 ms. We set the scheduler granularity to 1 ms
					// at startup on PC. We reserve 2 ms of slack time which we will wait for by giving
					// up our timeslice.
					if( WaitTime > 5 / 1000.f )
					{
						appSleep( WaitTime - 0.002f );
					}

					// Give up timeslice for remainder of wait time.
					while( appSeconds() < WaitEndTime )
					{
						appSleep( 0 );
					}
				}
			}
			else if (GSystemSettings.bAllowPerFrameYield)
			{
				appSleep( 0 );
			}

#endif
			GCurrentTime = appSeconds();
		}

#if LOG_WAIT_TIME_STATS
		FLOAT UpdatedDeltaTime = GCurrentTime - LastTime;
		FLOAT RealWaitTime = GCurrentTime - OldCurrentTime;
		appOutputDebugStringf(TEXT("New Delta: %5.2f, Actual Wait: %5.2f, Org Delta: %5.2f, Req Wait: %5.2f, "),UpdatedDeltaTime*1000,RealWaitTime*1000,DeltaTime * 1000, WaitTime * 1000);
#endif

		// Update game thread idle time even when stats are disabled so it works for Test builds.
		extern DWORD GGameThreadIdle;
		GGameThreadIdle += appCycles() - IdleStart;

		SET_FLOAT_STAT(STAT_GameTickWantedWaitTime,WaitTime * 1000.f);
		SET_FLOAT_STAT(STAT_GameTickAdditionalWaitTime,Max<FLOAT>((ActualWaitTime-WaitTime)*1000.f,0.f));

		GDeltaTime			= GCurrentTime - LastTime;
		// Negative delta time means something is wrong with the system. Error out so user can address issue.
		if( GDeltaTime < 0 )
		{
#if MOBILE
			warnf(TEXT("Detected negative delta time: %f"), GDeltaTime);
#else
			// AMD dual-core systems are a known issue that require AMD CPU drivers to be installed. Installer will take care of this for shipping.
			appErrorf(TEXT("Detected negative delta time - on AMD systems please install http://files.aoaforums.com/I3199-setup.zip.html"));
#endif
			GDeltaTime = 0.01;
		}
		GUnclampedDeltaTime = GDeltaTime;
		LastTime			= GCurrentTime;

		// Enforce a maximum delta time if wanted.
		const FLOAT MaxDeltaTime = Cast<UGameEngine>(GEngine) ? (CastChecked<UGameEngine>(GEngine))->MaxDeltaTime : 0.f;
		if( MaxDeltaTime > 0.f )
		{
			// We don't want to modify delta time if we are dealing with network clients as either host or client.
			if( GWorld != NULL
			// allow demo playback "clients"
			&& ( (GWorld->DemoRecDriver != NULL && GWorld->DemoRecDriver->ServerConnection != NULL) ||
				// Not having a game info implies being a client.
				( GWorld->GetWorldInfo()->Game != NULL
				// NumPlayers and GamePlayer only match in standalone game types and handles the case of splitscreen.
				&&	GWorld->GetWorldInfo()->Game->NumPlayers == GEngine->GamePlayers.Num() ) ) )
			{
				// Happy clamping!
				GDeltaTime = Min<DOUBLE>( GDeltaTime, MaxDeltaTime );
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	Frame end sync object implementation.
-----------------------------------------------------------------------------*/

/**
 * Special helper class for frame end sync. It respects a passed in option to allow one frame
 * of lag between the game and the render thread by using two events in round robin fashion.
 */
class FFrameEndSync
{
public:
	/** Events used to sync to. If frame lag is allowed we will sync to the previous event. */
	FEvent* Event[2];
	/** Current index into events array. */
	INT EventIndex;

	/**
	 * Constructor, initializing all member variables and creating events.
	 */
	FFrameEndSync()
	{
		check( GSynchronizeFactory );
		
		// Create sync events.
		Event[0] = GSynchronizeFactory->CreateSynchEvent();
		Event[1] = GSynchronizeFactory->CreateSynchEvent();

		// Trigger event 0 as we're waiting on its completion when we first start up in the
		// one frame lag case.
		Event[0]->Trigger();
		// Set event index to start at 1 as we've just triggered 0.
		EventIndex = 1;
	}

	/**
	 * Destructor, destroying the event objects.
	 */
	~FFrameEndSync()
	{
		check( GSynchronizeFactory );
		
		// Destroy events.
		GSynchronizeFactory->Destroy( Event[0] );
		GSynchronizeFactory->Destroy( Event[1] );
		
		// NULL out to aid debugging.
		Event[0] = NULL;
		Event[1] = NULL;
	}

	/**
	 * Syncs the game thread with the render thread. Depending on passed in bool this will be a total
	 * sync or a one frame lag.
	 */
	void Sync( UBOOL bAllowOneFrameThreadLag )
	{
		check(IsInGameThread());			

		// Reset the previously triggered event at this point. The render thread will set it.
		Event[EventIndex]->Reset();

		// Enqueue command to trigger event once the render thread executes it.
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FenceCommand,
			FEvent*,EventToTrigger,Event[EventIndex],
			{
				// Trigger event to signal game thread.
				EventToTrigger->Trigger();
			});

		// Use two events if we allow a one frame lag.
		if( bAllowOneFrameThreadLag )
		{
			EventIndex = (EventIndex + 1) % 2;
		}

		SCOPE_CYCLE_COUNTER(STAT_GameIdleTime);
		DWORD IdleStart = appCycles();

		// Wait for completion of fence. Either the previous or the current one depending on whether
		// one frame lag is enabled or not.
		while(TRUE)
		{
			// Wait with a timeout for the event from the rendering thread; use a timeout to ensure that this doesn't deadlock
			// if the rendering thread crashes.
			if(Event[EventIndex]->Wait(100))
			{
				break;
			}

			// Break out of the loop with a fatal error if the rendering thread has crashed.
			CheckRenderingThreadHealth();
		}

		extern DWORD GGameThreadIdle;
		GGameThreadIdle += appCycles() - IdleStart;
	}
};
	

/*-----------------------------------------------------------------------------
	Debugging.
-----------------------------------------------------------------------------*/

#if _MSC_VER
/** Original C- Runtime pure virtual call handler that is being called in the (highly likely) case of a double fault */
_purecall_handler DefaultPureCallHandler;

/**
* Our own pure virtual function call handler, set by appPlatformPreInit. Falls back
* to using the default C- Runtime handler in case of double faulting.
*/
static void PureCallHandler()
{
	static UBOOL bHasAlreadyBeenCalled = FALSE;
	appDebugBreak();
	if( bHasAlreadyBeenCalled )
	{
		// Call system handler if we're double faulting.
		if( DefaultPureCallHandler )
		{
			DefaultPureCallHandler();
		}
	}
	else
	{
		bHasAlreadyBeenCalled = TRUE;
		if( GIsRunning )
		{
			appMsgf( AMT_OK, TEXT("Pure virtual function being called while application was running (GIsRunning == 1).") );
		}
		appErrorf(TEXT("Pure virtual function being called") );
	}
}
#endif

/*-----------------------------------------------------------------------------
	FEngineLoop implementation.
-----------------------------------------------------------------------------*/
#if !CONSOLE

#if WITH_EDITOR
FGameCookerHelper* GGameCookerHelper = NULL;
#endif

/**
 * Helper function to set GIsUCCMake and other hacks required for compiling script.
 */
static void SwitchToScriptCompilationMode()
{
	GIsUCCMake = TRUE;
	// allow ConditionalLink() to call Link() for non-intrinsic classes during make
	GUglyHackFlags |= HACK_ClassLoadingDisabled;
}
#endif	//#if !CONSOLE

/** Ensures the hardware survey thread is shutdown. Waits up to 30 seconds from application start for it to finish before killing it. */
void ShutdownHardwareSurveyThread()
{
	// shut down the hardware survey thread
	extern FRunnableThread* GUDKHardwareSurveyThread;
	if (GUDKHardwareSurveyThread != NULL)
	{
		// give the thread 30 seconds to finish from app startup time.
		DOUBLE AppTimeoutTime = GStartTime + 30.0;
		extern volatile INT GUDKHardwareSurveySucceeded;
		extern volatile INT GUDKHardwareSurveyThreadRunning;
		// give the thread some time to finish before signaling a forced shutdown, which would abort the upload.
		while (GUDKHardwareSurveyThreadRunning && appSeconds() < AppTimeoutTime)
		{
			appSleep(0.1f);
		}

		// kill the thread, give it one second to be nice, but we've already waited nicely above.
		// A successful upload is when the thread shut down properly and set the succeeded flag.
		UBOOL bSurveySuccessful = GUDKHardwareSurveyThread->Kill(TRUE) && GUDKHardwareSurveySucceeded;
		GThreadFactory->Destroy(GUDKHardwareSurveyThread);
		GUDKHardwareSurveyThread = NULL;

		// update the config to keep track of survey success rate.
		if (GConfig)
		{
			const TCHAR* HardwareSurveySectionStr = TEXT("HardwareSurvey");
			const TCHAR* HardwareSurveyAttemptedStr = TEXT("SurveysAttempted");
			const TCHAR* HardwareSurveyFailedStr = TEXT("SurveysFailed");
			INT NumSurveysAttempted=0, NumSurveysFailed=0;
			GConfig->GetInt(HardwareSurveySectionStr, HardwareSurveyFailedStr, NumSurveysFailed, GEngineIni);
			GConfig->GetInt(HardwareSurveySectionStr, HardwareSurveyAttemptedStr, NumSurveysAttempted, GEngineIni);
			++NumSurveysAttempted;
			NumSurveysFailed += bSurveySuccessful ? 0 : 1;
			GConfig->SetInt(HardwareSurveySectionStr, HardwareSurveyFailedStr, NumSurveysFailed, GEngineIni);
			GConfig->SetInt(HardwareSurveySectionStr, HardwareSurveyAttemptedStr, NumSurveysAttempted, GEngineIni);
			GConfig->Flush(FALSE, GEngineIni);
		}
	}
}

/* 
 *   Iterate over all objects considered part of the root to setup GC optimizations
 */
void MarkObjectsToDisregardForGC()
{
	// Iterate over all class objects and force the default objects to be created. Additionally also
	// assembles the token reference stream at this point. This is required for class objects that are
	// not taken into account for garbage collection but have instances that are.
	for( TObjectIterator<UClass> It; It; ++It )
	{
		UClass* Class = *It;
		// Force the default object to be created.
		Class->GetDefaultObject( TRUE );
		// Assemble reference token stream for garbage collection/ RTGC.
		Class->AssembleReferenceTokenStream();
	}

	// Iterate over all objects and mark them to be part of root set.
	INT NumAlwaysLoadedObjects = 0;
	for( FObjectIterator It; It; ++It )
	{
		UObject* Object = *It;
		ULinkerLoad* LinkerLoad = Cast<ULinkerLoad>(Object);

		// Exclude linkers from root set if we're using seekfree loading
		if( !GUseSeekFreeLoading || (LinkerLoad == NULL || LinkerLoad->HasAnyFlags(RF_ClassDefaultObject)) )
		{
			Object->AddToRoot();
			NumAlwaysLoadedObjects++;
		}
	}

	debugf(TEXT("%i objects as part of root set at end of initial load."), NumAlwaysLoadedObjects);
	debugf(TEXT("%i out of %i bytes used by permanent object pool."), UObject::GPermanentObjectPoolTail - UObject::GPermanentObjectPool, UObject::GPermanentObjectPoolSize );
}					

#if _WINDOWS && !CONSOLE && UDK
/** 
 * Load in the current Mod GUID from Binaries\UnSetup.Game.xml
 */
FGuid LoadModGUID( void )
{
	FGuid GameUniqueID( 0, 0, 0, 0 );
	TiXmlDocument UnSetupGame( "..\\UnSetup.Game.xml" );

	if (UnSetupGame.LoadFile())
	{
		// First child element is 'GameOptions'
		TiXmlHandle RootNode = UnSetupGame.FirstChildElement();
		// Find the 'GameUniqueID' element
		TiXmlHandle GameUniqueIDElement = RootNode.FirstChildElement( "GameUniqueID" );
		TiXmlText* ModGUIDText = GameUniqueIDElement.FirstChild().ToText();
		if( ModGUIDText )
		{
			FString ModGuidString( UTF8_TO_TCHAR( ModGUIDText->Value() ) );
			ModGuidString.ReplaceInline( TEXT( "-" ), TEXT( "" ) );
			ModGuidString = FString::Printf( TEXT( "Guid=%s" ), *ModGuidString );
			Parse( *ModGuidString, TEXT( "Guid=" ), GameUniqueID );
		}
	}

	return( GameUniqueID );
}
#endif // UDK


// can be move to a later point or restructured for each module
void CreateConsoleVariables()
{
	check(GConsoleManager);

#if !FINAL_RELEASE
	GConsoleManager->RegisterConsoleVariable(TEXT("TonemapperType"),
		-1,
		TEXT("Allows to override which tone mapper function (during the post processing stage to transform HDR to LDR colors) is used:\n")
		TEXT("-1: use what is specified elsewhere (default)\n")
		TEXT(" 0: off (no tone mapper)\n")
		TEXT(" 1: filmic approximation (s-curve, contrast enhances, changes saturation and hue)\n")
		TEXT(" 2: neutral soft white clamp (experimental)"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("MotionBlurTimeScaleMin"),
		0.0f,
		TEXT("Allows to specify the minimum amount of motion blur to control how frame time and hitches are affecting the look.\n")
		TEXT("Value is in seconds (default 0.0, meaning no clamp)"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("MotionBlurTimeScaleMax"),
		1000.0f,
		TEXT("Allows to specify the maximum amount of motion blur to control how frame time and hitches are affecting the look.\n")
		TEXT("Value is in seconds (default 1000.0, basically no clamp)"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("AmbientOcclusionQuality"),
		-1,
		TEXT("Allows to override the SSAO quality.\n")
		TEXT("-1: use what is specified elsewhere (default)\n")
		TEXT(" 0: Low (fastest)\n")
		TEXT(" 1: Medium\n")
		TEXT(" 2: High (full resolution)"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("MotionBlurSoftEdge"),
		-1.0f,
		TEXT("Allows to override the motion blur soft edge amount.\n")
		TEXT("<0: use post process settings (default: -1)\n")
		TEXT(" 0: override post process settings, feature is off\n")
		TEXT(">0: override post process settings by the given value"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("MotionBlurMaxVelocity"),
		-1.0f,
		TEXT("Allows to override the motion blur max velocity post process setting.\n")
		TEXT("<0: use post process settings (default: -1)\n")
		TEXT(" 0: override post process settings, feature is off\n")
		TEXT(">0: override post process settings by the given value"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("MotionBlurAmount"),
		-1.0f,
		TEXT("Allows to override the motion blur amount post process setting.\n")
		TEXT("<0: use post process settings (default: -1)\n")
		TEXT(" 0: override post process settings, feature is off\n")
		TEXT(">0: override post process settings by the given value"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("ImageGrain"),
		-1.0f,
		TEXT("Allows to override the image grain post process setting.\n")
		TEXT("<0: use post process settings (default: -1)\n")
		TEXT(" 0: override post process settings, feature is off\n")
		TEXT(">0: override post process settings by the given value"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("BloomSize"),
		-1.0f,
		TEXT("Allows to override the bloom size post process setting.\n")
		TEXT("<0: use post process settings (default: -1)\n")
		TEXT(">=0: override post process settings by the given value"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("BloomThreshold"),
		-1.0f,
		TEXT("Allows to override the bloom threshold post process setting.\n")
		TEXT("<0: use post process settings (default: -1)\n")
		TEXT(">=0: override post process settings by the given value"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("BloomQuality"),
		-1,
		TEXT("Allows to override the bloom quality post process setting.\n")
		TEXT("<0: use default (default: -1)\n")
		TEXT(" 0: Bloom down sampling is done with a 2x2 kernel in 4 lookups (fast)\n")
		TEXT(" 1: Bloom down sampling is done with a 4x4 kernel in 4 lookups (softer)"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("BloomType"),
		-1,
		TEXT("Allows to override which bloom method is used\n")
		TEXT("-1: use what is specified elsewhere\n")
		TEXT(" 0: Separatable Gaussian (like 3 but limited radius and radius can be chosen at any level)\n")
		TEXT(" 1: Separatable Gaussian with additional inner/narrow weight\n")
		TEXT(" 2: Separatable Gaussian with additional inner/narrow and outer/broad weight\n")
		TEXT(" 3: Separatable Gaussian (unlimited radius through multipass, can get slow, radius change every 4 units)"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("BloomScale"),
		-1.0f,
		TEXT("Allows to override the bloom scale post process setting.\n")
		TEXT(" <0: use post process settings (default: -1)\n")
		TEXT(">=0: override post process settings by the given value"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("PreViewTranslation"),
		1,
		TEXT("To limit issues with float world space positions we offset the world by the\n")
		TEXT("PreViewTranslation vector. This command allows to disable updating this vector.\n")
		TEXT(" 0: disable update\n")
		TEXT(" 1: update the offset is each frame (default)"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("DOFOcclusionTweak"),
		-1.0f,
		TEXT("Allows to override the Depth of Field occlusion setting.\n")
		TEXT(" <0: use default settings (default: -1)\n")
		TEXT(">=0: override settings by the given value"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("FogDensity"),
		-1.0f,
		TEXT("Allows to override the FogDensity setting (needs ExponentialFog in the level).\n")
		TEXT("Using a strong value allows to quickly see which pixel are affected by fog.\n")
		TEXT("Using a start distance allows to cull pixels are can speed up rendering.\n")
		TEXT(" <0: use default settings (default: -1)\n")
		TEXT(">=0: override settings by the given value (0:off, 1=very dense fog)"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("FogStartDistance"),
		-1.0f,
		TEXT("Allows to override the FogStartDistance setting (needs ExponentialFog in the level).\n")
		TEXT(" <0: use default settings (default: -1)\n")
		TEXT(">=0: override settings by the given value (in world units)"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("RenderTimeFrozen"),
		0,
		TEXT("Allows to freeze time based effects in order to provide more deterministic render profiling.\n")
		TEXT(" 0: off\n")
		TEXT(" 1: on"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("FreezeAtPosition"),
		TEXT(""),	// default value is empty
		TEXT("This console variable stores the position and rotation for the FreezeAt command which allows\n")
		TEXT("to lock the camera in order to provide more deterministic render profiling.\n")
		TEXT("The FreezeAtPosition can be set in the ConsoleVariables.ini (start the map with MAPNAME?bTourist=1).\n")
		TEXT("Also see the FreezeAt command console command.\n")
		TEXT("The number syntax if the same as the one used by the BugIt command:\n")
		TEXT(" The first three values define the position, the next three define the rotation.\n")
		TEXT("Example:\n")
		TEXT(" FreezeAtPosition 2819.5520 416.2633 75.1500 65378 -25879 0"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("PostprocessAAType"),
		-1,
		TEXT("Allows to override the post process anti aliasing type.\n")
		TEXT(" <0: use post process settings (default: -1)\n")
		TEXT("  0: off\n")
		TEXT("1-6: FXAA Preset 0:low quality but fast .. 5:very high quality but slow (1 pass)\n")
		TEXT("  7: MLAA (requires extra render targets (requires bAllowPostprocessMLAA=True in .ini), 3 passes)"),
		ECVF_Cheat);

	GConsoleManager->RegisterConsoleVariable(TEXT("ProfileEx"),
		0,
		TEXT("Profile experiment: to profile features only by looking at stat unit (alternating it on/off)\n")
		TEXT("This is useful on platforms that don't support any other profiling functionality.\n")
		TEXT("Make sure VSYNC is off and ideally you are in \"pause\".\n")
		TEXT("\"Stat unit\" and \"StableFrameTime\" are activated with the feature.\n")
		TEXT("FeatureMask in hex (add to combine, 0 is off, e.g. 0x1a = 0x10 0x08 0x02):\n")
		TEXT(" 0x001: Deferred shadow projection\n")
		TEXT(" 0x002: Depth of Field\n")
		TEXT(" 0x004: Bloom\n")
		TEXT(" 0x008: Shadows (shadow map generation and projection)\n")
		TEXT(" 0x010: Depth resolve (mobile only)\n")
		TEXT(" 0x020: Bloom/DOF blur (mobile only)\n")
		TEXT(" 0x040: Force Gaussian to blur with 4 samples at max\n")
		TEXT(" 0x080: Fog (mobile only)\n")
		TEXT(" 0x100: Translucency\n")
		TEXT(" 0x200: Skeletal meshes\n")
		TEXT(" 0x400: StaticMeshes\n")
		TEXT(" 0x800: Particles (only toggles rendering)"),
		ECVF_Cheat | ECVF_HexValue);

	GConsoleManager->RegisterConsoleVariable(TEXT("StableFrameTime"),
		0,
		TEXT("\"Stat Unit\" will display a more readable stable frame time (experimental)\n")
		TEXT("  0: off, old mode (default)\n")
		TEXT("  1: on (no rolling average/smoothing)"),
		ECVF_Cheat);

#endif // !FINAL_RELEASE
}

INT FEngineLoop::PreInit( const TCHAR* CmdLine )
{
	bInitialized = FALSE;
#if _MSC_VER
	// Use our own handler for pure virtuals being called.
	DefaultPureCallHandler = _set_purecall_handler( PureCallHandler );
#endif

	// remember thread id of the main thread
	GGameThreadId = appGetCurrentThreadId();
	GIsGameThreadIdInitialized = TRUE;

	// Init console variable manager
	GConsoleManager = new FConsoleManager;

	// can be move to a later point or restructured for each module
	CreateConsoleVariables();

	// setup the streaming resource flush function pointer
	GFlushStreamingFunc = &FlushResourceStreaming;

	// Set the game name.
	appSetGameName();

	// Figure out whether we want to override the package map with the seekfree version. Needs to happen before the first call
	// to UClass::Link!
	GUseSeekFreePackageMap	= GUseSeekFreePackageMap || ParseParam( CmdLine, TEXT("SEEKFREEPACKAGEMAP") );
	// Figure out whether we are cooking for the demo or not.
	GIsCookingForDemo		= ParseParam( CmdLine, TEXT("COOKFORDEMO"));

	// look early for the editor token
	UBOOL bHasEditorToken = FALSE;
#if !CONSOLE
	// Figure out whether we're the editor, ucc or the game.
	const SIZE_T CommandLineSize = appStrlen(CmdLine)+1;
	TCHAR* CommandLineCopy			= new TCHAR[ CommandLineSize ];
	appStrcpy( CommandLineCopy, CommandLineSize, CmdLine );
	const TCHAR* ParsedCmdLine	= CommandLineCopy;
	FString Token				= ParseToken( ParsedCmdLine, 0);

	// trim any whitespace at edges of string - this can happen if the token was quoted with leading or trailing whitespace
	// VC++ tends to do this in its "external tools" config
	Token = Token.Trim();

	bHasEditorToken = Token == TEXT("EDITOR");

	// set the seek free loading flag if it's given if we are running a commandlet or not
#if SHIPPING_PC_GAME && !UDK
	// shipping PC game implies seekfreeloading for non-commandlets/editor
	GUseSeekFreeLoading = !bHasEditorToken;

	if (ParseParam(CmdLine, TEXT("NOSEEKFREELOADING")))
	{
		GUseSeekFreeLoading = FALSE;
	}

#else
	GUseSeekFreeLoading = ParseParam(CmdLine, TEXT("SEEKFREELOADING")) && !bHasEditorToken;
	// If there is no game content folder, presume we're running cooked
	FString ContentFolder = appGameDir() + TEXT( "Content" );
	if( !appDirectoryExists( *ContentFolder ) )
	{
		GUseSeekFreeLoading = TRUE;
	}
#endif

	// PC Server mode is "lean and mean", does not support editor, and only runs with cooked data
	GIsSeekFreePCServer = ParseParam(CmdLine, TEXT("SEEKFREELOADINGSERVER")) && !bHasEditorToken;
	// PC console mode does not support editor and only runs with cooked data
	GIsSeekFreePCConsole = ParseParam(CmdLine, TEXT("SEEKFREELOADINGPCCONSOLE")) && !bHasEditorToken;
	GUseSeekFreeLoading |= (GIsSeekFreePCServer | GIsSeekFreePCConsole);

	if( Token == TEXT("MAKE") || Token == TEXT("MAKECOMMANDLET") )
	{
		// Rebuilding script requires some hacks in the engine so we flag that.
		SwitchToScriptCompilationMode();
	}
#endif

	GIsSimMobile = ParseParam(CmdLine, TEXT("simmobile")) && !bHasEditorToken;

	// Force a log flush after each line
	GForceLogFlush = ParseParam( CmdLine, TEXT("FORCELOGFLUSH") );

	// Force a complete recook of all sounds
	GForceSoundRecook = ParseParam( CmdLine, TEXT("FORCESOUNDRECOOK") );

	// Override compression settings wrt size.
	GAlwaysBiasCompressionForSize = ParseParam( CmdLine, TEXT("BIASCOMPRESSIONFORSIZE") );

	// Allows release builds to override not verifying GC assumptions. Useful for profiling as it's hitchy.
	extern UBOOL GShouldVerifyGCAssumptions;
	if( ParseParam( CmdLine, TEXT("VERIFYGC") ) )
	{
		GShouldVerifyGCAssumptions = TRUE;
	}
	if( ParseParam( CmdLine, TEXT("NOVERIFYGC") ) )
	{
		GShouldVerifyGCAssumptions = FALSE;
	}

	// Please Trace FX
	extern UBOOL GShouldTraceFaceFX;
	GShouldTraceFaceFX = ParseParam ( CmdLine, TEXT("DEBUGFACEFX") );

	extern UBOOL GShouldTraceAnimationUsage;
	GShouldTraceAnimationUsage = ParseParam( CmdLine, TEXT("TRACEANIMUSAGE") );

#if XBOX
	appInit( CmdLine, &::Log, NULL       , &Error, &GameWarn, &FileManager, new FCallbackEventObserver(), new FCallbackQueryDevice(), FConfigCacheIni::Factory );
#else  // XBOX
	// This is done in appXenonInit on Xenon.
	GSynchronizeFactory			= &SynchronizeFactory;
	GThreadFactory				= &ThreadFactory;
	GThreadPool					= &ThreadPool;
	INT NumThreadsInThreadPool	= 1;
#if _WINDOWS
	// This is limited to 8 worker threads, because currently we can't keep more than this busy and
	// dedicated servers suffer when using just processor number
	NumThreadsInThreadPool = 8;
	SYSTEM_INFO SI;
	GetSystemInfo(&SI);
	NumThreadsInThreadPool = Min<INT>(NumThreadsInThreadPool,SI.dwNumberOfProcessors - 1);
	// Ensure always one thread in the pool
	NumThreadsInThreadPool = Max<INT>(1,NumThreadsInThreadPool);

	// create the hi pri thread pool.
	GHiPriThreadPool = &HiPriThreadPool;
	GHiPriThreadPoolNumThreads = NumThreadsInThreadPool;
	verify(GHiPriThreadPool->Create(NumThreadsInThreadPool));
#endif

#if !FLASH
	verify(GThreadPool->Create(NumThreadsInThreadPool));
#endif

	FOutputDeviceConsole *LogConsolePtr = NULL;
#if _WINDOWS && !CONSOLE
	// see if we were launched from our .com command line launcher
	InheritedLogConsole.Connect();
	LogConsolePtr = &InheritedLogConsole;

#if WITH_FIREWALL_SUPPORT
	extern void HandleFirewallIntegration(const TCHAR* CmdLine, const TCHAR* AppPath, const TCHAR* AppFriendlyName);
	if( ParseParam( CmdLine, TEXT( "installfw" ) ) || ParseParam( CmdLine, TEXT( "uninstallfw" ) ) )
	{
		TCHAR AppPath[MAX_PATH];
		GetModuleFileName( NULL, AppPath, MAX_PATH - 1 );

		FString AppFullPath = AppPath;
		AppFullPath = AppFullPath.ToLower();
		
		// Grab the user friendly display name passed in on the cmdline, if none is provided we set a default
		FString AppFriendlyName;
		if(!Parse(CmdLine, TEXT("-namefw="), AppFriendlyName))
		{
			AppFriendlyName = TEXT("Unreal Development Kit");
		}

		HandleFirewallIntegration( CmdLine, *AppFullPath, *AppFriendlyName );
		
		// Figure out if we are running the 32/64 bit executable and then add firewall support for the opposite executable as well
		if(AppFullPath.InStr( TEXT("win64") ) != INDEX_NONE )
		{
			HandleFirewallIntegration( CmdLine, *AppFullPath.Replace( TEXT("win64"), TEXT("win32") ), *AppFriendlyName);
		}
		else if( AppFullPath.InStr( TEXT("win32") ) != INDEX_NONE )
		{
			HandleFirewallIntegration( CmdLine, *AppFullPath.Replace(TEXT("win32"), TEXT("win64") ), *AppFriendlyName);
		}

		// Request exit of the app, this exit is delayed and additional flags in the commandline will be processed (ex. -firstinstall)
		appRequestExit( FALSE );
	}
#endif // WITH_FIREWALL_SUPPORT
#endif // _WINDOWS && !CONSOLE

	appInit( CmdLine, &::Log, LogConsolePtr, &Error, &GameWarn, &FileManager, new FCallbackEventObserver(), new FCallbackQueryDevice(), FConfigCacheIni::Factory );
#endif	// XBOX 

#if WITH_SUBSTANCE_AIR
	GUseSubstanceInstallTimeCache = FALSE;
	if (GUseSeekFreeLoading)
	{
		GConfig->GetBool(
			TEXT("SubstanceAir"),
			TEXT("bInstallTimeGeneration"),
			GUseSubstanceInstallTimeCache,
			GEngineIni);
	}
#endif

	if (ParseParam( CmdLine, TEXT("firstinstall")))
	{
		// If system setting overrides were passed in on the cmdline on first install, 
		//  we will run system setting init here so that those values can be saved out to ini files
		FString Settings;
		if (Parse(appCmdLine(), TEXT("-SS:"), Settings, FALSE))
		{
			GSystemSettings.Initialize( bHasEditorToken );
		}

#if _WINDOWS && !USE_NULL_RHI && !CONSOLE
		// @TODO: Move this to an appropriate place
		extern VOID SetDefaultResolutionForDevice();
		SetDefaultResolutionForDevice();
		GLog->Flush();
#endif

#if UDK
		// wait for hw survey thread to finish or time out
		ShutdownHardwareSurveyThread();
#endif

		// Flush config to ensure language changes are written to disk.
		GConfig->Flush(FALSE);

		appRequestExit(TRUE);
	}

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	FString EatMemAmount;
	// eat up some memory at startup if desired
	if (Parse(CmdLine, TEXT("EATMEM="), EatMemAmount))
	{
		INT EatMemSize = appAtoi(*EatMemAmount);
		debugf(TEXT("Eating up %d megs of memory, in 1 meg chunks"), EatMemSize);
		for (INT Index = 0; Index < EatMemSize; Index++)
		{
			void* Ptr = appMalloc(1 * 1024 * 1024);
			if (Ptr)
			{
				// make sure the memory is written to, in case there's any "allocate on write" happening
				appMemzero(Ptr, 1 * 1024 * 1024);
			}
		}
	}
#endif

#if CHECK_PUREVIRTUALS
	appMsgf(AMT_OK, *LocalizeError(TEXT("Error_PureVirtualsEnabled"), TEXT("Launch")));
	appRequestExit(FALSE);
#endif

	// Create the object used to track task performance and report the data to a database. This relies on GConfig already being initialized, which
	// means that it needs to occur after appInit.
#if !CONSOLE
	GTaskPerfTracker = new FTaskPerfTracker();
#endif
	// Only create DB connection if we're running Sentinel
	if( ( FString(appCmdLine()).InStr( TEXT( "DoingASentinelRun=1" ), FALSE, TRUE ) != INDEX_NONE )
		|| ( FString(appCmdLine()).InStr( TEXT( "gDASR=1" ), FALSE, TRUE ) != INDEX_NONE ) 
		)
	{
	GTaskPerfMemDatabase = new FTaskPerfMemDatabase();
	}

#if !FINAL_RELEASE
	//Initialize the class redirect map before we load any packages
	// This is needed so that any class renames will load correctly
	//ULinkerLoad::CreateActiveRedirectsMap(GEngineIni);
#endif

#if _WINDOWS && !CONSOLE
	const INT MinResolution[] = {640,480};
	if ( GetSystemMetrics(SM_CXSCREEN) < MinResolution[0] || GetSystemMetrics(SM_CYSCREEN) < MinResolution[1] )
	{
		appMsgf(AMT_OK, *LocalizeError(TEXT("Error_ResolutionTooLow"), TEXT("Launch")));
		appRequestExit( FALSE );
	}

	extern UBOOL HandleGameExplorerIntegration(const TCHAR* CmdLine);
	// allow for game explorer processing (including parental controls)
	if (HandleGameExplorerIntegration(appCmdLine()) == FALSE)
	{
		appRequestExit(FALSE);
	}

#endif // _WINDOWS

	// Initialize system settings before anyone tries to use it...
	GSystemSettings.Initialize( bHasEditorToken );
	
	// Initialize global engine settings.
	GConfig->GetBool( TEXT("Engine.Engine"), TEXT("bUseTextureStreaming"), GUseTextureStreaming, GEngineIni );
	GConfig->GetBool( TEXT("Engine.Engine"), TEXT("AllowScreenDoorFade"), GAllowScreenDoorFade, GEngineIni );
	GConfig->GetBool( TEXT("Engine.Engine"), TEXT("AllowNvidiaStereo3d"), GAllowNvidiaStereo3d, GEngineIni );

#if WITH_STREAMING_PAUSE
	GConfig->GetBool( TEXT("Engine.Engine"), TEXT("bUseStreamingPause"), GUseStreamingPause, GEngineIni );
#endif // WITH_STREAMING_PAUSE

	// Get the render mode. 
	INT RenderModeMode;
	GRenderMode = RENDER_MODE_DX9;
	if( GConfig->GetInt( TEXT( "Startup" ), TEXT( "RenderMode" ), RenderModeMode, GEditorUserSettingsIni ) )
	{
		if( RenderModeMode == RENDER_MODE_DX11 )
		{
			GRenderMode = RENDER_MODE_DX11;
		}
	}

#if _WINDOWS && !CONSOLE && UDK
	// Load in the mod guid (if any)
	GModGUID = LoadModGUID();
#endif // UDK

#if _WINDOWS && !CONSOLE && !DEDICATED_SERVER
	// see if we are running a remote desktop session
	UBOOL bRunningRemoteDesktop = GetSystemMetrics(SM_REMOTESESSION);

	// set compatibility setting the first time the game is run
	// Don't set the CompatLevel if we're running remote desktop or in the editor.
	if ( !bRunningRemoteDesktop && !bHasEditorToken && !GIsUCCMake )
	{
		// get compatibility level
		const FCompatibilityLevelInfo MachineCompatLevel = appGetCompatibilityLevel();
		GConfig->SetInt( TEXT("AppCompat"), TEXT("CompatLevelComposite"), MachineCompatLevel.CompositeLevel, GEngineIni );
		GConfig->Flush( FALSE, GEngineIni );

		appSetCompatibilityLevel( MachineCompatLevel, TRUE );
	}
#endif // _WINDOWS && !DEDICATED_SERVER

	// Initialize the RHI.
	ValidatePixelFormats();
	RHIInit( bHasEditorToken );

	// is not using NAME_Init to make sure this is never suppressed
	debugf(TEXT("Shader platform (RHI): %s"), ShaderPlatformToText(GRHIShaderPlatform));

#if !CONSOLE
	// Generate the binary shader files if we need to.
	GenerateBinaryShaderFiles();
#endif
	
	check(!GShaderCompilingThreadManager);
	GShaderCompilingThreadManager = new FShaderCompilingThreadManager();
	
	// Run optional tests
#if ENABLE_VECTORINTRINSICS_TEST
	if ( ParseParam(appCmdLine(),TEXT("vectortest")) )
	{
		debugf(TEXT("Running vector math tests..."));
		RunVectorRegisterAbstractionTest();
	}
#endif


#if WITH_EDITOR
#if SPAWN_CHILD_PROCESS_TO_COMPILE
	// If the scripts need rebuilding, ask the user if they'd like to rebuild them
	//@script patcher
	if( !GIsUCCMake && Token != TEXT("PatchScript"))
	{	
		UBOOL bIsDone = FALSE;
		while (!bIsDone && AreScriptPackagesOutOfDate())
		{
			// figure out if we should compile scripts or not
			UBOOL bShouldCompileScripts = GIsUnattended;
			if( ( GIsUnattended == FALSE ) && ( GUseSeekFreeLoading == FALSE ) )
			{
				INT Result = appMsgf(AMT_YesNoCancel,TEXT("Scripts are outdated. Would you like to rebuild now?"));

				// check for Cancel
				if (Result == 2)
				{
					exit(1);
				}
				else
				{
					// 0 is Yes, 1 is No (unlike AMT_YesNo, where 1 is Yes)
					bShouldCompileScripts = Result == 0;
				}
			}

			if (bShouldCompileScripts)
			{
#ifdef _MSC_VER
				// get executable name
				TCHAR ExeName[MAX_PATH];
				GetModuleFileName(NULL, ExeName, ARRAY_COUNT(ExeName));

				// create the new process, with params as follows:
				//  - if we are running unattended, pass on the unattended flag
				//  - if not unattended, then tell the subprocess to pause when there is an error compiling
				//  - if we are running silently, pass on the flag
				void* ProcHandle = appCreateProc(ExeName, *FString::Printf(TEXT("make %s %s"), 
					GIsUnattended ? TEXT("-unattended") : TEXT("-nopauseonsuccess"), 
					GIsSilent ? TEXT("-silent") : TEXT("")));
				
				INT ReturnCode;
				// wait for it to finish and get return code
				while (!appGetProcReturnCode(ProcHandle, &ReturnCode))
				{
					appSleep(0);
				}

				// if we are running unattended, we can't run forever, so only allow one pass of compiling script code
				if (GIsUnattended)
				{
					bIsDone = TRUE;
				}
#else // _MSC_VER
				// if we can't spawn childprocess, just make within this process
				Token = TEXT("MAKE");
				bHasEditorToken = false;
				bIsDone = TRUE;
#endif
			}
			else
			{
				bIsDone = TRUE;
			}
		}

		// reload the package cache, as the child process may have created script packages!
		GPackageFileCache->CachePaths();
	}
#else // SPAWN_CHILD_PROCESS_TO_COMPILE

	// If the scripts need rebuilding, ask the user if they'd like to rebuild them
	//@script patcher
	if( !GIsUCCMake && Token != TEXT("PATCHSCRIPT") && AreScriptPackagesOutOfDate())
	{
		// If we are unattended, don't popup the dialog, but emit and error as scripts need to be updated!
		// Default response in unattended mode for appMsgf is cancel - which exits immediately. It should default to 'No' in this case
		if( ( GIsUnattended == FALSE ) && ( GUseSeekFreeLoading == FALSE ) )
		{
			switch ( appMsgf(AMT_YesNoCancel, TEXT("Scripts are outdated. Would you like to rebuild now?")) )
			{
				case 0:		// Yes - compile
					Token			= TEXT("MAKE");
					SwitchToScriptCompilationMode();
					bHasEditorToken = false;
					break;

				case 1:		// No - do nothing
					break;

				case 2:		// Cancel - exit
					appRequestExit( TRUE );
					break;

				default:
					warnf( TEXT( "Scripts are outdated. Failed to find valid case to switch on!" ) );
					break;
			}
		}
		else if( ( GIsUnattended == TRUE ) && ( GUseSeekFreeLoading == FALSE ) )
		{
			warnf( NAME_Error, TEXT( "Scripts are outdated. Please build them.") );
			appRequestExit( FALSE );
		}
	}

#endif
#endif	// WITH_EDITOR

	// Deal with static linking.
	InitializeRegistrantsAndRegisterNames();

	UBOOL bLoadStartupPackagesForCommandlet = FALSE;
#if !CONSOLE
	// if we are running a commandlet, disable GUseSeekfreeLoading at this point, so
	// look for special tokens

	// the server can handle seekfree loading, so don't disable it
	if (Token == TEXT("SERVER") || Token == TEXT("SERVERCOMMANDLET"))
	{
		// do nothing
	}
	// make wants seekfree loading off
	else if (GIsUCCMake || Token == TEXT("RUN"))
	{
		GUseSeekFreeLoading = FALSE;
		bLoadStartupPackagesForCommandlet = TRUE;
	}
	// otherwise, look if first token is a native commandlet
	else if( Token.Len() && appStrnicmp(*Token, TEXT("-"), 1) != 0)
	{
		// look for the commandlet by name
		UClass* Class = FindObject<UClass>(ANY_PACKAGE, *Token, FALSE);
		
		// tack on "Commandlet" and try again if not found
		if (!Class)
		{
			Class = FindObject<UClass>(ANY_PACKAGE, *(Token + TEXT("Commandlet")), FALSE);
		}

		// if a commandlet was specified, disable seekfree loading
		if (Class && Class->IsChildOf(UCommandlet::StaticClass()))
		{
			GUseSeekFreeLoading = FALSE;
			bLoadStartupPackagesForCommandlet = TRUE;
		}
	}
#endif

	// create global debug communications object (for talking to UnrealConsole)
	// @todo iphone keyboard: Remove the iPhone FINAL_RELEASE override once we have an on-screen keyboard
#if !NGP // @todo ngp 940: Investigate this again with SDK 940
#if (!FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE) && !SHIPPING_PC_GAME && WITH_UE3_NETWORKING
	GDebugChannel = new FDebugServer();
#if !DEDICATED_SERVER
	GDebugChannel->Init();
#endif	//DEDICATED_SERVER
#endif
#endif

	// Create the streaming manager and add the default streamers.
	FStreamingManagerTexture* TextureStreamingManager = new FStreamingManagerTexture();
	TextureStreamingManager->AddTextureStreamingHandler( new FStreamingHandlerTextureStatic() );
	TextureStreamingManager->AddTextureStreamingHandler( new FStreamingHandlerTextureLevelForced() );
	GStreamingManager = new FStreamingManagerCollection();
	GStreamingManager->AddStreamingManager( TextureStreamingManager );
	
	GIOManager = new FIOManager();

	// Create the async IO manager.
#if XBOX
	FAsyncIOSystemXenon*	AsyncIOSystem = new FAsyncIOSystemXenon();
#elif PS3
	FAsyncIOSystemPS3*		AsyncIOSystem = new FAsyncIOSystemPS3();
#elif _MSC_VER
	FAsyncIOSystemWindows*	AsyncIOSystem = new FAsyncIOSystemWindows();
#elif ANDROID
	FAsyncIOSystemAndroid*	AsyncIOSystem = new FAsyncIOSystemAndroid();
#elif FLASH
	FAsyncIOSystemFlash*	AsyncIOSystem = new FAsyncIOSystemFlash();
	AsyncIOSystem->Init();
#elif IPHONE
	FAsyncIOSystemIPhone*	AsyncIOSystem = new FAsyncIOSystemIPhone();
#elif PLATFORM_MACOSX
	FAsyncIOSystemMac*		AsyncIOSystem = new FAsyncIOSystemMac();
#elif NGP
	FAsyncIOSystemNGP*		AsyncIOSystem = new FAsyncIOSystemNGP();
#elif WIIU
	FAsyncIOSystemWiiU*		AsyncIOSystem = new FAsyncIOSystemWiiU();
#else
	#error implement async io manager for platform
#endif

#if !FLASH // No threads yet
	// This will auto- register itself with GIOMananger and be cleaned up when the manager is destroyed.
	AsyncIOThread = GThreadFactory->CreateThread( AsyncIOSystem, TEXT("AsyncIOSystem"), 0, 0, 16384, 
#if NGP //|| WIIU
		// @todo ngp 940: Investigate this again with SDK 940
		TPri_AboveNormal );
#else
		TPri_BelowNormal );
#endif
	check(AsyncIOThread);
#endif
#if XBOX
	// See UnXenon.h
	AsyncIOThread->SetProcessorAffinity(ASYNCIO_HWTHREAD);
#endif

	// Init physics engine before loading anything, in case we want to do things like cook during post-load.
	InitGameRBPhys();

#if WITH_FACEFX
	// Make sure FaceFX is initialized.
	UnInitFaceFX();
#endif // WITH_FACEFX

	// create the game specific DLC manager here
#if MOBILE
	// We currently do not support downloadable content on iDevices 
#else
#if GAMENAME == GEARGAME
	GGamePatchHelper = new FGamePatchHelper;
#else
	GGamePatchHelper = new FGamePatchHelper;
#endif

	// create the platform specific DLC helper here
#if PS3
	GDownloadableContent = new FDownloadableContent;
	GPlatformDownloadableContent = new FPS3DownloadableContent;
#endif
#endif	//!MOBILE

	// Delete temporary files in cache.
	appCleanFileCache();

#if !CONSOLE
	if( bHasEditorToken )
	{	
#if DEMOVERSION
		appErrorf(TEXT("Editor not supported in demo version."));
#endif

#if WITH_EDITOR
		// release our .com launcher -- this is to mimic previous behavior of detaching the console we launched from
		InheritedLogConsole.DisconnectInherited();

		if( !GIsCOMInitialized )
		{
			// here we start COM (used by some console support DLLs)
			CoInitialize(NULL);
			GIsCOMInitialized = TRUE;
		}

		// We're the editor.
		GIsClient	= 1;
		GIsServer	= 1;
		GIsEditor	= 1;
		GIsUCC		= 0;
		GGameIcon	= GEditorIcon;

		// UnrealEd requires a special callback handler and feedback context.		
		GCallbackEvent	= new FCallbackEventDeviceEditor();
		GCallbackQuery	= new FCallbackQueryDeviceEditor();
		GWarn		= &UnrealEdWarn;

		// Remove "EDITOR" from command line.
		const TCHAR* pCmdLine = GCmdLine;
		const FString Unused = ParseToken(pCmdLine, TRUE);
		if ( Unused.Len() > 0 )
		{
			const FString CommandLineWithoutToken(pCmdLine);

			pCmdLine = 0;
			appStrcpy(GCmdLine, *CommandLineWithoutToken);
		}

		// Set UnrealEd as the current package (used for e.g. log and localization files).
		appStrcpy( GPackage, TEXT("UnrealEd") );
#else	
		appMsgf(AMT_OK, TEXT("Editor not supported in this mode."));
		appRequestExit(FALSE);
		return 1;
#endif //WITH_EDITOR
	}
	else
	{
		// See whether the first token on the command line is a commandlet.

		//@hack: We need to set these before calling StaticLoadClass so all required data gets loaded for the commandlets.
		GIsClient	= 1;
		GIsServer	= 1;
		GIsEditor	= 1;
		GIsUCC		= 1;
		UBOOL bIsSeekFreeDedicatedServer = FALSE;

		UClass* Class = NULL;

#if SHIPPING_PC_GAME && !UDK && !DEDICATED_SERVER
		const UBOOL bIsRunningAsUser = TRUE;
#else
		const UBOOL bIsRunningAsUser = ParseParam(appCmdLine(), TEXT("user"));
#endif
		if ( bIsRunningAsUser )
		{
			GIsCooking = (appStristr(appCmdLine(), TEXT("CookPackages")) != NULL);

			// we need to set the GCookingTarget early so that editoronly objects are not loaded
			// (see UStruct::SerializeTaggedProperties)
			if (GIsCooking)
			{
				GCookingTarget = ParsePlatformType(appCmdLine());
				GCookingShaderPlatform = ShaderPlatformFromUE3Platform(GCookingTarget);
			}

			const UBOOL bIsMaking = GIsUCCMake;
			if ( GIsCooking || bIsMaking || bLoadStartupPackagesForCommandlet )
			{
				// disable this flag to make package loading work as expected internally
				GIsUCC = FALSE;

				// load up the seekfree startup packages
				LoadStartupPackages();

				// restore the flag
				GIsUCC = TRUE;
			}
		}

		// We need to disregard the empty token as we try finding Token + "Commandlet" which would result in finding the
		// UCommandlet class if Token is empty.
		if( Token.Len() && appStrnicmp(*Token,TEXT("-"),1) != 0 )
		{
			DWORD CommandletLoadFlags = LOAD_NoWarn|LOAD_Quiet;
			UBOOL bNoFail = FALSE;
			UBOOL bIsUsingRunOption = Token == TEXT("RUN");

			if ( bIsUsingRunOption )
			{
				Token = ParseToken( ParsedCmdLine, 0);
				if ( Token.Len() == 0 )
				{
					warnf(TEXT("You must specify a commandlet to run, e.g. game.exe run test.samplecommandlet"));
					appRequestExit(FALSE);
					return 1;
				}
				bNoFail = TRUE;
				if ( Token == TEXT("MAKE") || Token == TEXT("MAKECOMMANDLET") )
				{
#if WITH_EDITOR
					// We can't bind to .u files if we want to build them via the make commandlet, hence the LOAD_DisallowFiles.
					CommandletLoadFlags |= LOAD_DisallowFiles;

					// allow the make commandlet to be invoked without requiring the package name
					Token = TEXT("UnrealEd.MakeCommandlet");
#else	
					appMsgf(AMT_OK, TEXT("Make is not supported in this mode."));
					appRequestExit(FALSE);
					return 1;
#endif //WITH_EDITOR

				}

				if ( Token == TEXT("SERVER") || Token == TEXT("SERVERCOMMANDLET") )
				{
					// allow the server commandlet to be invoked without requiring the package name
					Token = TEXT("Engine.ServerCommandlet");
					bIsSeekFreeDedicatedServer = GUseSeekFreeLoading;
				}

				Class = LoadClass<UCommandlet>(NULL, *Token, NULL, CommandletLoadFlags, NULL );
				if ( Class == NULL && Token.InStr(TEXT("Commandlet")) == INDEX_NONE )
				{
					Class = LoadClass<UCommandlet>(NULL, *(Token+TEXT("Commandlet")), NULL, CommandletLoadFlags, NULL );
				}
			}
			else
			{
				// allow these commandlets to be invoked without requiring the package name
				if ( Token == TEXT("MAKE") || Token == TEXT("MAKECOMMANDLET") )
				{
					// We can't bind to .u files if we want to build them via the make commandlet, hence the LOAD_DisallowFiles.
					CommandletLoadFlags |= LOAD_DisallowFiles;

					// allow the make commandlet to be invoked without requiring the package name
					Token = TEXT("UnrealEd.MakeCommandlet");
				}

				else if ( Token == TEXT("SERVER") || Token == TEXT("SERVERCOMMANDLET") )
				{
					// allow the server commandlet to be invoked without requiring the package name
					Token = TEXT("Engine.ServerCommandlet");
					bIsSeekFreeDedicatedServer = GUseSeekFreeLoading;
				}
			}

			if (bIsSeekFreeDedicatedServer)
			{
				// when starting up a seekfree dedicated server, we act like we're the game starting up with the seekfree
				GIsEditor = FALSE;
				GIsUCC = FALSE;
				GIsGame = TRUE;

				// load up the seekfree startup packages
				LoadStartupPackages();

				// Setup GC optimizations
				MarkObjectsToDisregardForGC(); 
			}

			// See whether we're trying to run a commandlet. @warning: only works for native commandlets
			 
			//  Try various common name mangling approaches to find the commandlet class...
			if( !Class )
			{
				Class = FindObject<UClass>(ANY_PACKAGE,*Token,FALSE);
			}
			if( !Class )
			{
				Class = FindObject<UClass>(ANY_PACKAGE,*(Token+TEXT("Commandlet")),FALSE);
			}

			// Only handle '.' if we're using RUN commandlet option so we can specify maps including the extension.
			if( bIsUsingRunOption )
			{
				if( !Class && Token.InStr(TEXT(".")) != -1)
				{
					Class = LoadObject<UClass>(NULL,*Token,NULL,LOAD_NoWarn,NULL);
				}
				if( !Class && Token.InStr(TEXT(".")) != -1 )
				{
					Class = LoadObject<UClass>(NULL,*(Token+TEXT("Commandlet")),NULL,LOAD_NoWarn,NULL);
				}
			}

			// If the 'cookpackages' commandlet is being run, then create the game-specific helper...
#if !CONSOLE && WITH_EDITOR
			FString UpperCaseToken = Token.ToUpper();
			if (UpperCaseToken.InStr(TEXT("COOKPACKAGES")) != INDEX_NONE )
			{
#if GAMENAME == GEARGAME
				GGameCookerHelper = new FGearGameCookerHelper();
#elif GAMENAME == EXOGAME
				GGameCookerHelper = new FExoGameCookerHelper();
#elif GAMENAME == FORTRESSGAME
				GGameCookerHelper = new FFortEditorCookerHelper();
#elif GAMENAME == SWORDGAME
				GGameCookerHelper = new FSwordGameCookerHelper();
#else
				GGameCookerHelper = new FGameCookerHelper();
#endif
			}
#endif	//!CONSOLE && WITH_EDITOR

			// Ignore found class if it is not a commandlet!
			// This can happen when a map is named the same as a class...
			if (Class && (Class->IsChildOf(UCommandlet::StaticClass()) == FALSE))
			{
				Class = NULL;
			}

			// ... and if successful actually load it.
			if( Class )
			{
				if ( Class->HasAnyClassFlags(CLASS_Intrinsic) )
				{
					// if this commandlet is native-only, we'll need to manually load its parent classes to ensure that it has
					// correct values in its PropertyLink array after it has been linked
					TArray<UClass*> ClassHierarchy;
					for ( UClass* ClassToLoad = Class->GetSuperClass(); ClassToLoad != NULL; ClassToLoad = ClassToLoad->GetSuperClass() )
					{
						ClassHierarchy.AddItem(ClassToLoad);
					}
					for ( INT i = ClassHierarchy.Num() - 1; i >= 0; i-- )
					{
						UClass* LoadedClass = UObject::StaticLoadClass( UObject::StaticClass(), NULL, *(ClassHierarchy(i)->GetPathName()), NULL, CommandletLoadFlags, NULL );
						check(LoadedClass);
					}
				}
				Class = UObject::StaticLoadClass( UCommandlet::StaticClass(), NULL, *Class->GetPathName(), NULL, CommandletLoadFlags, NULL );
			}
			
			if ( Class == NULL && bNoFail == TRUE )
			{
				appMsgf(AMT_OK, TEXT("Failed to find a commandlet named '%s'"), *Token);
				appRequestExit(FALSE);
				return 1;
			}
		}

		if( Class != NULL )
		{
#if DEMOVERSION
			if( !Class->IsChildOf(UServerCommandlet::StaticClass()) )
			{
				appErrorf(TEXT("Only supporting SERVER commandlet in demo version."));
			}
#endif
			//@todo - make this block a separate function

			// The first token was a commandlet so execute it.
	
			// Remove commandlet name from command line.
			const TCHAR* pCmdLine = GCmdLine;
			const FString FirstToken = ParseToken(pCmdLine, TRUE);
			if ( FirstToken.Len() > 0 )
			{
				// if the commandlet was executed via "run", there are two tokens to remove
				if (FirstToken == TEXT("RUN"))
			{
					ParseToken(pCmdLine, TRUE);
				}
				const FString CommandLineWithoutToken(pCmdLine);

				pCmdLine = 0;
				appStrcpy(GCmdLine, *CommandLineWithoutToken);
			}

			// Set UCC as the current package (used for e.g. log and localization files).
			appStrcpy( GPackage, TEXT("UCC") );

			// Bring up console unless we're a silent build.
			if( GLogConsole && !GIsSilent )
			{
				GLogConsole->Show( TRUE );
			}

			// log out the engine meta data when running a commandlet
			debugf( NAME_Init, TEXT("Version: %i"), GEngineVersion );
			debugf( NAME_Init, TEXT("Epic Internal: %i"), GIsEpicInternal );
#if PLATFORM_64BITS
			debugf( NAME_Init, TEXT("Compiled (64-bit): %s %s"), ANSI_TO_TCHAR(__DATE__), ANSI_TO_TCHAR(__TIME__) );
#else
			debugf( NAME_Init, TEXT("Compiled (32-bit): %s %s"), ANSI_TO_TCHAR(__DATE__), ANSI_TO_TCHAR(__TIME__) );
#endif
			debugf( NAME_Init, TEXT("Command line: %s"), appCmdLine() );
			debugf( NAME_Init, TEXT("Base directory: %s"), appBaseDir() );
			debugf( NAME_Init, TEXT("Character set: %s"), sizeof(TCHAR)==1 ? TEXT("ANSI") : TEXT("Unicode") );
			debugf( TEXT("Executing %s"), *Class->GetFullName() );

			// Allow commandlets to individually override those settings.
			UCommandlet* Default = CastChecked<UCommandlet>(Class->GetDefaultObject(TRUE));
			Class->ConditionalLink();

			if ( GIsRequestingExit )
			{
				// commandlet set GIsRequestingExit in StaticInitialize
				return 1;
			}

			GIsClient	= Default->IsClient;
			GIsServer	= Default->IsServer;
			GIsEditor	= Default->IsEditor;
			GIsGame		= !GIsEditor;
			GIsUCC		= TRUE;

			// Reset aux log if we don't want to log to the console window.
			if( !Default->LogToConsole )
			{
				GLog->RemoveOutputDevice( GLogConsole );
			}

#if _WINDOWS
			if( ParseParam(appCmdLine(),TEXT("AUTODEBUG")) )
			{
				debugf(TEXT("Attaching UnrealScript Debugger and breaking at first bytecode."));
				UDebuggerCore::InitializeDebugger();

				// we want the UDebugger to break on the first bytecode it processes
				((UDebuggerCore*)GDebugger)->SetBreakASAP(TRUE);

				// we need to allow the debugger to process debug opcodes prior to the first tick, so enable the debugger.
				((UDebuggerCore*)GDebugger)->EnableDebuggerProcessing(TRUE);
			}
			else if ( ParseParam(appCmdLine(),TEXT("VADEBUG")) )
			{
				if (!GDebugger)
				{
					debugf(TEXT("Attaching UnrealScript Debugger"));

					// otherwise, if we are running the script debugger from within VS.NET, we need to create the UDebugger
					// so that we can receive log messages from the game and user input events (e.g. set breakpoint) from VS.NET
					UDebuggerCore::InitializeDebugger();
				}
			}
#endif

			// Commandlets can't use disregard for GC optimizations as FEngineLoop::Init is not being called which does all 
			// the required fixup of creating defaults and marking dependencies as part of the root set.
			if (!bIsSeekFreeDedicatedServer)
			{
				UObject::GObjFirstGCIndex = 0;
			}

			GIsInitialLoad		= FALSE;

			//@hack: we don't want GIsRequestingExit=TRUE here for the server commandlet because it will break networking
			// as that code checks for this and doesn't add net packages to the package map when it is true
			// (probably should be checking something different, or maybe not checking it at all, or commandlets should be setting this flag themselves...
			// but this is the safer fix for now
			if (!Class->IsChildOf(UServerCommandlet::StaticClass()))
			{
				GIsRequestingExit	= TRUE;	// so CTRL-C will exit immediately
			}

			// allow the commandlet the opportunity to create a custom engine
			Class->GetDefaultObject<UCommandlet>()->CreateCustomEngine();
			if ( GEngine == NULL )
			{
#if !_WINDOWS || !WITH_EDITOR  // !!! FIXME
				//STUBBED("GIsEditor");
#else
				if ( GIsEditor )
				{
					UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_None, NULL );

					// must do this here so that the engine object that we create on the next line receives the correct property values
					EditorEngineClass->GetDefaultObject(TRUE);
					EditorEngineClass->ConditionalLink();
					GEngine = GEditor			= ConstructObject<UEditorEngine>( EditorEngineClass );
					debugf(TEXT("Initializing Editor Engine..."));
					GEditor->InitEditor();
					debugf(TEXT("Initializing Editor Engine Completed"));
				}
				else
#endif //!_WINDOWS || !WITH_EDITOR
				{
					// pretend we're the game (with no client) again while loading/initializing the engine from seekfree 
					if (bIsSeekFreeDedicatedServer)
					{
						GIsEditor = FALSE;
						GIsGame = TRUE;
						GIsUCC = FALSE;
					}
					UClass* EngineClass = UObject::StaticLoadClass( UEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.GameEngine"), NULL, LOAD_None, NULL );
					// must do this here so that the engine object that we create on the next line receives the correct property values
					EngineClass->GetDefaultObject(TRUE);
					EngineClass->ConditionalLink();
					GEngine = ConstructObject<UEngine>( EngineClass );

					// Setup up particle count clamping values...
					GEngine->MaxParticleSpriteCount = GEngine->MaxParticleVertexMemory / (4 * sizeof(FParticleSpriteVertex));
					GEngine->MaxParticleSubUVCount = GEngine->MaxParticleVertexMemory / (4 * sizeof(FParticleSpriteSubUVVertex));

					debugf(TEXT("Initializing Game Engine..."));
					GEngine->Init();
					debugf(TEXT("Initializing Game Engine Completed"));

					// reset the flags if needed
					if (bIsSeekFreeDedicatedServer)
					{
						GIsEditor	= Default->IsEditor;
						GIsGame		= !GIsEditor;
						GIsUCC		= FALSE;
					}
				}
			}
	

#if !CONSOLE && _WINDOWS
			if( !GIsUCCMake && !GIsCooking)
			{
				// Load in the console support dlls so commandlets can convert data
				FConsoleSupportContainer::GetConsoleSupportContainer()->LoadAllConsoleSupportModules();
			}
#endif //!CONSOLE && _WINDOWS

			// check to see if we are supposed to do minimal shader compilation for this commandlet

			UBOOL bMinimalShaders = FALSE;
			GConfig->GetBool(TEXT("CommandletsToForceMinimalShaderCompilation"), *Class->GetName(), bMinimalShaders, GEditorIni);
			if (bMinimalShaders)
			{
				GForceMinimalShaderCompilation = TRUE;
				warnf(NAME_Log, TEXT("*** Forcing Minimal Shader Compiling"));
			}

			UCommandlet* Commandlet = ConstructObject<UCommandlet>( Class );
			check(Commandlet);

#if WITH_MANAGED_CODE
			InteropTools::AddResolveHandler();
#endif

			// Execute the commandlet.
			DOUBLE CommandletExecutionStartTime = appSeconds();

			Commandlet->InitExecution();
			Commandlet->ParseParms( appCmdLine() );
			INT ErrorLevel = Commandlet->Main( appCmdLine() );

			// Log warning/ error summary.
			if( Commandlet->ShowErrorCount )
			{
				TLookupMap<FString> UniqueErrors;
				TLookupMap<FString> UniqueWarnings;
				UniqueErrors.Empty(GWarn->Errors.Num());
				UniqueWarnings.Empty(GWarn->Warnings.Num());
				if( GWarn->Errors.Num() || GWarn->Warnings.Num() )
				{
					SET_WARN_COLOR(COLOR_WHITE);
					warnf(TEXT(""));
					warnf(TEXT("Warning/Error Summary"));
					warnf(TEXT("---------------------"));

					SET_WARN_COLOR(COLOR_RED);
					for(INT I = 0; I < GWarn->Errors.Num(); I++)
					{
						UniqueErrors.AddItem(*GWarn->Errors(I));
					}
					for(INT I = 0; I < Min(50, UniqueErrors.Num()); I++)
					{
						GWarn->Log(UniqueErrors(I));
					}
					if (UniqueErrors.Num() > 50)
					{
						SET_WARN_COLOR(COLOR_WHITE);
						warnf(TEXT("NOTE: Only first 50 errors displayed."));
					}

					SET_WARN_COLOR(COLOR_YELLOW);
					UniqueWarnings.Empty(GWarn->Warnings.Num());
					for(INT I = 0; I < GWarn->Warnings.Num(); I++)
					{
						UniqueWarnings.AddItem(*GWarn->Warnings(I));
					}
					for(INT I = 0; I < Min(50, UniqueWarnings.Num()); I++)
					{
						GWarn->Log(UniqueWarnings(I));
					}
					if (UniqueWarnings.Num() > 50)
					{
						SET_WARN_COLOR(COLOR_WHITE);
						warnf(TEXT("NOTE: Only first 50 warnings displayed."));
					}
				}

				warnf(TEXT(""));

				if( ErrorLevel != 0 )
				{
					SET_WARN_COLOR(COLOR_RED);
					warnf( TEXT("Commandlet->Main return this error code: %d"), ErrorLevel );
					warnf( TEXT("With %d error(s), %d warning(s)"), UniqueErrors.Num(), UniqueWarnings.Num() );
				}
				else if( ( UniqueErrors.Num() == 0 ) )
				{
					SET_WARN_COLOR(UniqueWarnings.Num() ? COLOR_YELLOW : COLOR_GREEN);
					warnf( TEXT("Success - %d error(s), %d warning(s)"), UniqueErrors.Num(), UniqueWarnings.Num() );
				}
				else
				{
					SET_WARN_COLOR(COLOR_RED);
					warnf( TEXT("Failure - %d error(s), %d warning(s)"), UniqueErrors.Num(), UniqueWarnings.Num() );
					ErrorLevel = 1;
				}
				CLEAR_WARN_COLOR();
			}
			else
			{
				warnf( TEXT("Finished.") );
			}
		
			DOUBLE CommandletExecutionTime = appSeconds() - CommandletExecutionStartTime;
			warnf( LINE_TERMINATOR TEXT( "Execution of commandlet took:  %.2f seconds"), CommandletExecutionTime );
			GTaskPerfTracker->AddTask( *Class->GetName(), TEXT(""), CommandletExecutionTime );

			// We're ready to exit!
			return ErrorLevel;
		}
		else
#else
	{
#endif //!CONSOLE
		{
#if _WINDOWS && !CONSOLE
			// Load in the console support dlls so commandlets can convert data and
			// 360 texture stats work...
//			FConsoleSupportContainer::GetConsoleSupportContainer()->LoadAllConsoleSupportModules();
#endif

			// We're a regular client.
			GIsClient		= 1;
			GIsServer		= 0;
#if !CONSOLE
			GIsEditor		= 0;
			GIsUCC			= 0;
#endif

// handle launching dedicated server on console
#if CONSOLE
			// copy command line
			TCHAR* CommandLine	= new TCHAR[ appStrlen(appCmdLine())+1 ];
			appStrcpy( CommandLine, appStrlen(appCmdLine())+1, appCmdLine() );	
			// parse first token
			const TCHAR* ParsedCmdLine = CommandLine;
			FString Token = ParseToken( ParsedCmdLine, 0);
			Token = Token.Trim();
			// dedicated server command line option
			if( Token == TEXT("SERVER") )
			{
				GIsClient = 0;
				GIsServer = 1;

				// Pretend RHI hasn't been initialized to mimick PC behavior.
				GIsRHIInitialized = FALSE;

				// Remove commandlet name from command line
				appStrncpy( GCmdLine, ParsedCmdLine, ARRAY_COUNT(GCmdLine) );
			}	

			delete [] CommandLine;
#endif //CONSOLE

#if !FINAL_RELEASE
	#if WITH_UE3_NETWORKING
			// regular clients can listen for propagation requests
			FObjectPropagator::SetPropagator(&ListenPropagator);
	#endif	//#if WITH_UE3_NETWORKING
#endif
		}
	}

	// at this point, GIsGame is always not GIsEditor, because the only other way to set GIsGame is to be in a PIE world
	GIsGame = !GIsEditor;

#if !PS3
	// Exit if wanted.
	if( GIsRequestingExit )
	{
		if ( GEngine != NULL )
		{
			GEngine->PreExit();
		}
		appPreExit();
		// appExit is called outside guarded block.
		return 1;
	}
#endif

	// Movie recording.
	GIsDumpingTileShotMovie = Parse( appCmdLine(), TEXT("DUMPMOVIE_TILEDSHOT="), GScreenshotResolutionMultiplier );
	GIsTiledScreenshot = GIsDumpingTileShotMovie;

	FString MatineeName;
	// GIsDumpingMovie is mutually exclusive with GIsDumpingtileShotMovie, we only want one or the other
	GIsDumpingMovie	= !GIsDumpingTileShotMovie && (ParseParam(appCmdLine(),TEXT("DUMPMOVIE")) || Parse(appCmdLine(), TEXT("-MATINEESSCAPTURE="), MatineeName));
	// If dumping movie then we do NOT want on-screen messages
	GAreScreenMessagesEnabled = (!GIsDumpingMovie && !GIsTiledScreenshot);

	if (ParseParam(appCmdLine(),TEXT("NOSCREENMESSAGES")))
	{
		GAreScreenMessagesEnabled = FALSE;
	}

	// Disable force feedback
	GEnableForceFeedback = GEnableForceFeedback && !ParseParam(appCmdLine(),TEXT("NOFORCEFEEDBACK"));

	// Benchmarking.
	GIsBenchmarking	= ParseParam(appCmdLine(),TEXT("BENCHMARK"));

	// Capturing FPS chart info.
	extern UBOOL GIsCapturingFPSChartInfo;
	GIsCapturingFPSChartInfo = ParseParam( appCmdLine(), TEXT("CaptureFPSChartInfo") ) || ParseParam( appCmdLine(), TEXT("gCFPSCI") );

	// Script debugging tools
	GTreatScriptWarningsFatal = ParseParam(appCmdLine(),TEXT("FATALSCRIPTWARNINGS"));
	GScriptStackForScriptWarning = ParseParam(appCmdLine(),TEXT("SCRIPTSTACKONWARNINGS"));

	// create the global full screen movie player. 
	// This needs to happen before the rendering thread starts since it adds a rendering thread tickable object
	appInitFullScreenMoviePlayer();

	appInitShowFlags();

#if _WINDOWS && !CONSOLE
	if( !GIsBenchmarking && !ParseParam( appCmdLine(), TEXT("LOG") ) )
	{
		// release our .com launcher -- this is to mimic previous behavior of detaching the console we launched from
		InheritedLogConsole.DisconnectInherited();
	}

#endif

	// Don't update ini files if benchmarking or -noini
	if( GIsBenchmarking || ParseParam(appCmdLine(),TEXT("NOINI")))
	{
		GConfig->Detach( GEngineIni );
		GConfig->Detach( GInputIni );
		GConfig->Detach( GGameIni );
		GConfig->Detach( GEditorIni );
		GConfig->Detach( GUIIni );
	}

	// do any post appInit processing, before the renderthread is started.
	appPlatformPostInit();

	// Force single-threaded even on multiple processors
	UBOOL bOneThread = ParseParam(appCmdLine(),TEXT("ONETHREAD"));

	// Force multi-threaded even on single processors
	UBOOL bMultiThread = ParseParam(appCmdLine(), TEXT("FORCEMULTITHREAD"));

	// Make fluids simulate on the current thread if -onethread is supplied on the command line.
	if ( bOneThread )
	{
		extern UBOOL GThreadedFluidSimulation;
		GThreadedFluidSimulation = FALSE;
	}

	// Read texture fade settings
	{
		for ( INT MipFadeSetting=0; MipFadeSetting < MipFade_NumSettings; ++MipFadeSetting )
		{
			FString Key;
			Key = FString::Printf( TEXT("MipFadeInSpeed%d"), MipFadeSetting );
			GConfig->GetFloat( TEXT("Engine.Engine"), *Key, GMipFadeSettings[MipFadeSetting].FadeInSpeed, GEngineIni );
			Key = FString::Printf( TEXT("MipFadeOutSpeed%d"), MipFadeSetting );
			GConfig->GetFloat( TEXT("Engine.Engine"), *Key, GMipFadeSettings[MipFadeSetting].FadeOutSpeed, GEngineIni );
		}
	}

	// -onethread will disable renderer thread
	if (GIsClient && !bOneThread)
	{
		// Create the rendering thread.  Note that commandlets don't make it to this point.
		if(GNumHardwareThreads > 1 || bMultiThread)
		{
			GUseThreadedRendering = TRUE;
			StartRenderingThread();
		}
	}

#if !CONSOLE
	// Clean up allocation
	delete [] CommandLineCopy;
#endif

	// initialize the pointer, as it is delete'd before being assigned in the first frame
	PendingCleanupObjects = NULL;

#if WITH_REALD
	RealD::EngineLoopPreInit(CmdLine);
#endif
 
	return 0;
}


#if UDK && !CONSOLE
#if PLATFORM_MACOSX
extern UBOOL IsEULAAccepted (void);
#else // PLATFORM_MACOSX
/**
 * Checks EULA acceptance in UDK
 * return - TRUE if user has accepted the EULA and it is stored in the registry
 */
#pragma pack(push,8)
#include <shellapi.h>
#pragma pack(pop)
UBOOL IsEULAAccepted (void)
{
#if SHIPPING_PC_GAME
	// make sure they've run the EULA at least once at startup
	GFileManager->SetDefaultDirectory();

	//check registry setting by hand
	UBOOL bRegistryHasEULAAccepted = FALSE;
	UBOOL bSuccessfullyReadGUID = FALSE;

	// Get GUID out of XML file
	FString GUIDFileName = TEXT("..\\InstallInfo.xml");
	FString Text;
	if( appLoadFileToString( Text, *GUIDFileName, GFileManager, 0 ) )
	{
		FString GUIDStartString = TEXT("<InstallGuidString>");
		FString GUIDEndString = TEXT("<");

		INT StartIndex = Text.InStr(GUIDStartString);
		if (StartIndex != INDEX_NONE)
		{
			// skip to the end of the tag
			StartIndex += GUIDStartString.Len();
			INT EndIndex = Text.InStr(GUIDEndString, FALSE, FALSE, StartIndex);
			if (EndIndex != INDEX_NONE)
			{
				// read value out
				FString GUIDString = Text.Mid(StartIndex, EndIndex - StartIndex);
				FString RegistryKeyName = FString::Printf(TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\UDK-%s"), *GUIDString);

				// FString RegistryKeyName = FString::Printf(TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WIC"), *GUIDString);
				FString RegistryVariableName = TEXT("EULAAccepted");

				for (INT RegistryIndex = 0; RegistryIndex < 2; ++RegistryIndex)
				{
					HKEY Key = 0;
					char *Buffer = NULL;

					DWORD RegFlags = (RegistryIndex == 0) ? KEY_WOW64_32KEY : KEY_WOW64_64KEY;
					HRESULT OpenKeyResult = RegOpenKeyEx( HKEY_LOCAL_MACHINE, *RegistryKeyName, 0, KEY_READ | RegFlags, &Key );
					if (OpenKeyResult == ERROR_SUCCESS)
					{
						DWORD Size = 0;
						// First, we'll call RegQueryValueEx to find out how large of a buffer we need
						if ((RegQueryValueEx( Key, *RegistryVariableName, NULL, NULL, NULL, &Size ) == ERROR_SUCCESS) && Size)
						{
							// Allocate a buffer to hold the value and call the function again to get the data
							Buffer = new char[Size];
							if (RegQueryValueEx( Key, *RegistryVariableName, NULL, NULL, (LPBYTE)Buffer, &Size ) == ERROR_SUCCESS)
							{
								// run through all characters and see if any are valid (in case this is an INT)
								for (DWORD i = 0; i < Size; ++i)
								{
									bRegistryHasEULAAccepted |= Buffer[i];
								}
								// bust out of the loop if we found the entry in the registry already.  otherwise proceed to the user 
								RegistryIndex = 2;
							}
						}
						RegCloseKey( Key );
					}
				}
			}
		}
	}

	// if the EULA wasn't already accepted, try and run UnSetup to present the EULA
	if (!bRegistryHasEULAAccepted)
	{
		// Current Directory Storage
		FString CurrentDirectory = GFileManager->GetCurrentDirectory() + TEXT("..\\");
		// Setup Executable String
		FString EULAExecutableString = CurrentDirectory + TEXT("UnSetup.exe");
		// Setup Parameters for EULA
		FString EULACommandParams = TEXT("/EULA");

		SHELLEXECUTEINFO Info = { sizeof(SHELLEXECUTEINFO), 0 };

		Info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_UNICODE;
		Info.lpVerb = TEXT("runas");
		Info.lpFile = *EULAExecutableString;
		Info.lpParameters = *EULACommandParams;
		Info.lpDirectory = *CurrentDirectory;

		Info.nShow = SW_SHOWNORMAL;

		ShellExecuteEx(&Info);

		// execute unsetup.exe to run the EULA if it hasn't already been run
		if (!Info.hProcess)
		{
			debugf(TEXT("Failed to launch UnSetup.exe.  Please reinstall."));
			// Failure
			return FALSE;
		}
		// wait for it to finish and get return code
		INT ReturnCode;
		while (!appGetProcReturnCode(Info.hProcess, &ReturnCode))
		{
			appSleep(0);
		}
		// if we failed to accept the eula, quit
		// Note, 0 = success, (!0) = Failure
		if (!ReturnCode)
		{
			bRegistryHasEULAAccepted = TRUE;
		}
	}

	//EULA was accepted
	return bRegistryHasEULAAccepted;
#else
	return TRUE;
#endif // SHIPPING_PC_GAME
}
#endif // PLATFORM_MACOSX
#endif // UDK && !CONSOLE

INT FEngineLoop::Init()
{
#if UDK && !CONSOLE
	UBOOL bIsEULAAccepted = IsEULAAccepted();
	//if we failed to accept the eula, quit
	if (!bIsEULAAccepted)
	{
		GIsRequestingExit = TRUE;
		//error value
		return 1;
	}
#endif
#if USE_SCOPED_MEM_STATS && STATS
	if ( ParseParam(appCmdLine(), TEXT("scopedmemstats")))
	{
		FScopedMemoryStats::IsScopedMemStatsActive = TRUE;
	}
#endif

	SCOPED_MEM_STATS

#if USE_DETAILED_IPHONE_MEM_TRACKING
		//Track back buffer allocated on obj-c code 
		GMalloc->IncTrackedOpenGLMemory(GetIPhoneOpenGLBackBufferSize(), FMalloc::GLBACKBUFFER);
#endif

	if ( ParseParam(appCmdLine(), TEXT("logasync")))
	{
		GbLogAsyncLoading = TRUE;
		warnf(NAME_Warning, TEXT("*** ASYNC LOGGING IS ENABLED"));
	}
#if !CONSOLE
	if ( ParseParam(appCmdLine(), TEXT("ForceMinimalShaderCompilation")))
	{
		GForceMinimalShaderCompilation = TRUE;
		warnf(NAME_Log, TEXT("*** Forcing Minimal Shader Compiling"));
	}
#endif
#if !CONSOLE && !UE3_LEAN_AND_MEAN
	// verify that all shader source files are intact
	VerifyShaderSourceFiles();
#endif

	// Load the global shaders.
	GetGlobalShaderMap();

#if XBOX || PS3 || IPHONE // The movie playing can only create the special startup movie viewport on XBOX and PS3.
	if (GFullScreenMovie && !GIsEditor)
	{
		// play a looping movie from RAM while booting up
		GFullScreenMovie->GameThreadInitiateStartupSequence();
	}
#endif

#if WITH_SUBSTANCE_AIR == 1 
	if (GUseSubstanceInstallTimeCache)
	{
		FString CacheNameW = appGameDir() + TEXT("SubstanceAir") + PATH_SEPARATOR + TEXT("texture.cache");
		GSubstanceAirTextureReader = new atc_api::Reader(*CacheNameW);
	}
#endif

	// Load all startup packages, always including all native script packages. This is a superset of 
	// LoadAllNativeScriptPackages, which is done by the cooker, etc.
	LoadStartupPackages();

	if( !GUseSeekFreeLoading )
	{
		// Load the shader cache for the current shader platform if it hasn't already been loaded.
		GetLocalShaderCache( GRHIShaderPlatform );
	}

	// Setup GC optimizations
	MarkObjectsToDisregardForGC();

	GIsInitialLoad = FALSE;

	// Figure out which UEngine variant to use.
	UClass* EngineClass = NULL;
	if( !GIsEditor )
	{
		// We're the game.
		EngineClass = UObject::StaticLoadClass( UGameEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.GameEngine"), NULL, LOAD_None, NULL );
		GEngine = ConstructObject<UEngine>( EngineClass );
	}
	else
	{
#if WITH_EDITOR && !PLATFORM_UNIX
		// We're UnrealEd.
		EngineClass = UObject::StaticLoadClass( UUnrealEdEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.UnrealEdEngine"), NULL, LOAD_None, NULL );
		GEngine = GEditor = GUnrealEd = ConstructObject<UUnrealEdEngine>( EngineClass );

		// we don't want any of the Live Update/Play On * functionality in a shipping pc editor
#if !SHIPPING_PC_GAME || UDK
		// load any Console support modules that exist
		FConsoleSupportContainer::GetConsoleSupportContainer()->LoadAllConsoleSupportModules();

		debugf(TEXT("Supported Consoles:"));
		for (FConsoleSupportIterator It; It; ++It)
		{
			debugf(TEXT("  %s"), It->GetPlatformName());
		}
#endif

#else
		check(0);
#endif //WITH_EDITOR && !PLATFORM_UNIX
	}

	check( GEngine );

	// if we should use all available cores then we want to compress with all
	if( ParseParam(appCmdLine(), TEXT("USEALLAVAILABLECORES")) == TRUE )
	{
		GNumUnusedThreads_SerializeCompressed = 0;
	}

	// If the -nosound or -benchmark parameters are used, disable sound.
	if(ParseParam(appCmdLine(),TEXT("nosound")) || GIsBenchmarking)
	{
		GEngine->bUseSound = FALSE;
	}

	// Disable texture streaming if that was requested
	if( ParseParam( appCmdLine(), TEXT( "NoTextureStreaming" ) ) )
	{
		GUseTextureStreaming = FALSE;
	}

	if( ParseParam( appCmdLine(), TEXT("noailogging")) )
	{
		GEngine->bDisableAILogging = TRUE;
	}

	if( !GIsEditor && ParseParam( appCmdLine(), TEXT("aiproftool")) )
	{
		GEngine->Exec(TEXT("AIPROFILER START"));
	}

	if( ParseParam( appCmdLine(), TEXT("enableailogging")) )
	{
		GEngine->bDisableAILogging = FALSE;
	}

	// Setup up particle count clamping values...
	GEngine->MaxParticleSpriteCount = GEngine->MaxParticleVertexMemory / (4 * sizeof(FParticleSpriteVertex));
	GEngine->MaxParticleSubUVCount = GEngine->MaxParticleVertexMemory / (4 * sizeof(FParticleSpriteSubUVVertex));

	GEngine->bStartWithMatineeCapture = FALSE;
	GEngine->bCompressMatineeCapture = FALSE;
#if WITH_EDITOR
	// If doing a 'Play on XXX' from the editor, add the auto-save directory to the package search path, so streamed sub-levels can be found
	if ( !GIsEditor && ParseParam(appCmdLine(), TEXT("PIEVIACONSOLE")) )
	{
		GSys->Paths.AddUniqueItem(UEditorEngine::StaticClass()->GetDefaultObject<UEditorEngine>()->AutoSaveDir);
		GPackageFileCache->CachePaths();
	}

	if (!GIsEditor && Parse(appCmdLine(), TEXT("-MATINEEAVICAPTURE="), GEngine->MatineeCaptureName))
	{
		GEngine->MatineeCaptureType = 0; // AVI
		GEngine->bStartWithMatineeCapture = TRUE;
	}
	else if (!GIsEditor && Parse(appCmdLine(), TEXT("-MATINEESSCAPTURE="), GEngine->MatineeCaptureName))
	{
		GEngine->MatineeCaptureType = 1; // Screen Shots
		GEngine->bStartWithMatineeCapture = TRUE;
	}

	if (GEngine->bStartWithMatineeCapture)
	{
		Parse(appCmdLine(), TEXT("-MATINEEPACKAGE="), GEngine->MatineePackageCaptureName);
		Parse(appCmdLine(), TEXT("-VISIBLEPACKAGES="), GEngine->VisibleLevelsForMatineeCapture);
	}

	if ( !GIsEditor && ParseParam(appCmdLine(), TEXT("COMPRESSCAPTURE")) )
	{
		GEngine->bCompressMatineeCapture = TRUE;
	}


#endif

	// Init variables used for benchmarking and ticking.
	GCurrentTime				= appSeconds();
	MaxFrameCounter				= 0;
	MaxTickTime					= 0;
	TotalTickTime				= 0;
	LastFrameCycles				= appCycles();

	FLOAT FloatMaxTickTime		= 0;
	Parse(appCmdLine(),TEXT("SECONDS="),FloatMaxTickTime);
	MaxTickTime					= FloatMaxTickTime;

	// look of a version of seconds that only is applied if GIsBenchmarking is set. This makes it easier on
	// say, iOS, where we have a toggle setting to enable benchmarking, but don't want to have to make user
	// also disable the seconds setting as well. -seconds= will exit the app after time even if benchmarking
	// is not enabled
	// NOTE: This will override -seconds= if it's specified
	if (GIsBenchmarking)
	{
		if (Parse(appCmdLine(),TEXT("BENCHMARKSECONDS="),FloatMaxTickTime) && FloatMaxTickTime)
		{
			MaxTickTime			= FloatMaxTickTime;
		}
	}

	// Use -FPS=X to override fixed tick rate if e.g. -BENCHMARK is used.
	FLOAT FixedFPS = 0;
	Parse(appCmdLine(),TEXT("FPS="),FixedFPS);
	if( FixedFPS > 0 )
	{
		GEngine->MatineeCaptureFPS = (INT)FixedFPS;
		GFixedDeltaTime = 1 / FixedFPS;
	}
	else
	{
		GEngine->MatineeCaptureFPS = 30;
	}

	// convert FloatMaxTickTime into number of frames (using 1 / GFixedDeltaTime to convert fps to seconds )
	MaxFrameCounter				= appTrunc( MaxTickTime / GFixedDeltaTime );


	debugf(TEXT("Initializing Engine..."));
	GEngine->Init();
	debugf(TEXT("Initializing Engine Completed"));

	// Verify native class sizes and member offsets.
	CheckNativeClassSizes();

	// Optionally Exec an exec file
	FString Temp;
	if( Parse(appCmdLine(), TEXT("EXEC="), Temp) )
	{
		Temp = FString(TEXT("exec ")) + Temp;
		UGameEngine* Engine = Cast<UGameEngine>(GEngine);
		if ( Engine != NULL && Engine->GamePlayers.Num() && Engine->GamePlayers(0) )
		{
			Engine->GamePlayers(0)->Exec( *Temp, *GLog );
		}
	}

	GIsRunning		= TRUE;

	// let the propagator do it's thing now that we are done initializing
	GObjectPropagator->Unpause();

	// let the game script code run any special code for initial bootup (this is a one time call ever)
	if (GWorld != NULL && GWorld->GetGameInfo())
	{
		GWorld->GetGameInfo()->eventOnEngineHasLoaded();
	}

#if (IPHONE || ANDROID) && WITH_MOBILE_RHI
	// allow for EpicCitadel to skip on a button press
	if (GWorld->GetOutermost()->GetName() == TEXT("EpicCitadel"))
	{
		// Scale the "Start" text based on screen sizes.
		const FLOAT ScreenRatio = (FLOAT)GScreenHeight / 1024;
		const FLOAT ScaleStartTextFactor = ( 7.0f * ScreenRatio ) / GSystemSettings.MobileContentScaleFactor;

		GFullScreenMovie->GameThreadAddOverlay(GEngine->LargeFont, TEXT("START"), 0.0f, 0.8f, 1.0f, ScaleStartTextFactor, TRUE, FALSE, 0.0f);
		GFullScreenMovie->GameThreadToggleInputProcessing(TRUE);
	}
#endif

	// stop the initial startup movies now. 
	// movies won't actually stop until startup sequence has finished
	// @todo ib2merge: Chair had this line commented out
	GFullScreenMovie->GameThreadStopMovie();

#if FLASH
	 // @todo hack: Remove this if Android gets a FullscreenMovie class
	GEngine->Exec(TEXT("mobile stoploading"));
#endif

	if( !GIsEditor)
	{
		// hide a couple frames worth of rendering
		// @todo ib2merge: Chair had this line commented out
		FViewport::SetGameRenderingEnabled(TRUE, 3);
	}

	debugf(TEXT(">>>>>>>>>>>>>> Initial startup: %.2fs <<<<<<<<<<<<<<<"), appSeconds() - GStartTime);

	// handle test movie
	if( appStrfind(GCmdLine, TEXT("movietest")) != NULL )
	{
		// make sure language is correct for localized center channel
		UObject::SetLanguage(*appGetLanguageExt());
		// get the optional moviename from the command line (-movietest=Test.sfd)
		FString TestMovieName;
		Parse(GCmdLine, TEXT("movietest="), TestMovieName);
		// use default if not specified
		if( TestMovieName.Len() > 0 )
		{
			// play movie and wait for it to be done
			GFullScreenMovie->GameThreadPlayMovie(MM_PlayOnceFromStream, *TestMovieName);
			GFullScreenMovie->GameThreadWaitForMovie();			
		}
	}

#if !FINAL_RELEASE
	{
		// if the console variable FreezeAtPosition is set, we right away freeze at the given position
		static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("FreezeAtPosition")); 

		if(!CVar->GetString().IsEmpty())
		{
			new(GEngine->DeferredCommands) FString(TEXT("FreezeAt"));
		}
	}
#endif // !FINAL_RELEASE

	return 0;
}

void FEngineLoop::Exit()
{
	GIsRunning	= 0;
	GLogConsole	= NULL;

#if PERF_TRACK_FILEIO_STATS
	if( ParseParam( appCmdLine(), TEXT("DUMPFILEIOSTATS") ) )
	{
		GetFileIOStats()->DumpStats();
	}
#endif

	if( !GIsEditor && ParseParam( appCmdLine(), TEXT("aiproftool")) )
	{
		GEngine->Exec(TEXT("AIPROFILER STOP"));
	}

#if WITH_GFx
	if (GGFxEngine)
	{
		FlushRenderingCommands();

		delete GGFxEngine;      // which queues FFullScreenMovieGFx for deletion on the render thread
		GGFxEngine = NULL;
#if !USE_BINK_CODEC
#if WITH_GFx_FULLSCREEN_MOVIE
		GFullScreenMovie = NULL;
#endif
#endif
	}
	while (GGFxGCManager)
	{
		GGFxGCManager->Release();
	}

	if (!GIsUCCMake)
	{
		UObject::CollectGarbage(0);
	}
#endif

#if !CONSOLE
	// Save the local shader cache if it has changed. This avoids loss of shader compilation work due to crashing.
	// GEMINI_TODO: Revisit whether this is worth thet slowdown in material editing once the engine has stabilized.
	SaveLocalShaderCaches();
#endif

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
		appLoadFileToString( OutputString, *(appGameLogDir() + TEXT("benchmark.log")) );
		OutputString += FString::Printf(TEXT("min= %6.2f   avg= %6.2f   max= %6.2f%s"), 1000.f / MaxFrameTime, 1000.f / AvgFrameTime, 1000.f / MinFrameTime, LINE_TERMINATOR );
		appSaveStringToFile( OutputString, *(appGameLogDir() + TEXT("benchmark.log")) );

		FrameTimes.Empty();
	}

#if PS3 || ANDROID
	// the caller will shutdown, now that the benchmarking info is written out
	return;
#endif
#if XBOX
	XLaunchNewImage(XLAUNCH_KEYWORD_DEFAULT_APP, 0);
#endif

	// Make sure we're not in the middle of loading something.
	UObject::FlushAsyncLoading();
	// Block till all outstanding resource streaming requests are fulfilled.
	if (GStreamingManager != NULL)
	{
		UTexture2D::CancelPendingTextureStreaming();
		GStreamingManager->BlockTillAllRequestsFinished();
	}

	// shutdown debug communications
#if !FINAL_RELEASE && !SHIPPING_PC_GAME && WITH_UE3_NETWORKING
	GDebugChannel->Destroy();
	delete GDebugChannel;
	GDebugChannel = NULL;
#endif

	MALLOC_PROFILER( GEngine->Exec( TEXT( "MPROF STOP" ) ); )

	if ( GEngine != NULL )
	{
		GEngine->PreExit();
	}
	appPreExit();
	DestroyGameRBPhys();
	ParticleVertexFactoryPool_FreePool();

#if WITH_FACEFX
	// Make sure FaceFX is shutdown.
	UnShutdownFaceFX();
#endif // WITH_FACEFX

	// Stop the rendering thread.
	StopRenderingThread();

	delete GStreamingManager;
	GStreamingManager	= NULL;

	if (AsyncIOThread != NULL)
	{
		AsyncIOThread->Kill(TRUE);
		check(GThreadFactory);
		GThreadFactory->Destroy( AsyncIOThread );
	}
	delete GIOManager;
	GIOManager			= NULL;

#if STATS
	// Shutdown stats
	GStatManager.Destroy();
#endif
#if WITH_UE3_NETWORKING
	// Shutdown sockets layer
	appSocketExit();
#endif	//#if WITH_UE3_NETWORKING

	// Delete the task perf tracking object used to upload stats to a DB.
	delete GTaskPerfTracker;
	GTaskPerfTracker = NULL;

	// Delete the task perfmem database object used to upload perfmem stats to a DB.
	delete GTaskPerfMemDatabase;
	GTaskPerfMemDatabase = NULL;

	// remove all registered console variables
	delete GConsoleManager; GConsoleManager = 0;

	// make sure the hardware survey thread is shut down.
	ShutdownHardwareSurveyThread();

#if WITH_SUBSTANCE_AIR
	SubstanceAir::Helpers::TearDownSubstanceAir();
#endif // WITH_SUBSTANCE_AIR
}


void FEngineLoop::Tick()
{
#if STATS
	{
		// Extra Scope for FrameTime stat to be correct
		SCOPE_CYCLE_COUNTER( STAT_FrameTime );	
#endif
		// Suspend game thread while we're trace dumping to avoid running out of memory due to holding
		// an IRQ for too long.
		while( GShouldSuspendGameThread )
		{
			appSleep( 1 );
		}

		if (GHandleDirtyDiscError)
		{
			appSleepInfinite();
		}

		// Flush debug output which has been buffered by other threads.
		GLog->FlushThreadedLogs();

#if !CONSOLE
		// If the local shader cache has changed, save it.
		static UBOOL bIsFirstTick = TRUE;
		if ((!GIsEditor && !UObject::IsAsyncLoading()) || bIsFirstTick)
		{
			SaveLocalShaderCaches();
			bIsFirstTick = FALSE;
		}
#endif

		if( GDebugger )
		{
			GDebugger->NotifyBeginTick();
		}

		// Exit if frame limit is reached in benchmark mode.
		if( (GIsBenchmarking && MaxFrameCounter && (GFrameCounter > MaxFrameCounter))
			// or timelimt is reached if set.
			|| (MaxTickTime && (TotalTickTime > MaxTickTime)) )
		{
#if DO_CHARTING
			GEngine->DumpFPSChart();
#endif // DO_CHARTING
			appRequestExit(0);
		}

		// Set GCurrentTime, GDeltaTime and potentially wait to enforce max tick rate.
		appUpdateTimeAndHandleMaxTickRate();

		// Our frame is now over, so we can tick various things that keep track of frame time

#if DO_CHARTING
		// Ticks the FPS chart.
		GEngine->TickFPSChart( GDeltaTime );

		// Ticks the Memory chart.
		GEngine->TickMemoryChart( GDeltaTime );
#endif // DO_CHARTING

#if STATS		
	} // Close extra scope for FrameTime stat

	// Write all stats for this frame out
	GStatManager.AdvanceFrame();
	
	if( GIsThreadedRendering )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND( AdvanceFrame, { RenderingThreadAdvanceFrame(); } );
	}

	{ // Extra Scope for FrameTime stat to be correct
		SCOPE_CYCLE_COUNTER( STAT_FrameTime );

		extern void BeginOneFrameParticleStats();
		BeginOneFrameParticleStats();

		GFPSCounter.Update( appSeconds() );
#endif

	// Calculates average FPS/MS (outside STATS on purpose)
	CalculateFPSTimings();

	// Tick gameplay profiler here so it has accurate last frame stats
	GAMEPLAY_PROFILER( FGameplayProfiler::Tick() );

	// Note the start of a new frame
	MALLOC_PROFILER(GMalloc->Exec(*FString::Printf(TEXT("SNAPSHOTMEMORYFRAME")),*GLog));

	// handle some per-frame tasks on the rendering thread
	ENQUEUE_UNIQUE_RENDER_COMMAND(
		ResetDeferredUpdatesAndTickTickables,
	{
		FDeferredUpdateResource::ResetNeedsUpdate();

		// make sure that rendering thread tickables get a change to tick, even if the game thread
		// is keeping the rendering queue always full
		TickRenderingTickables();
	});

	// Update.
	GEngine->Tick( GDeltaTime );

	// Update RHI.
	{
		SCOPE_CYCLE_COUNTER(STAT_RHITickTime);
		RHITick( GDeltaTime );
	}

#if WITH_SUBSTANCE_AIR
	// Tick the SubstanceAir system
	SubstanceAir::Helpers::Tick();
#endif // WITH_SUBSTANCE_AIR

	// Increment global frame counter. Once for each engine tick.
	GFrameCounter++;

	// Disregard first few ticks for total tick time as it includes loading and such.
	if( GFrameCounter > 5 )
	{
		TotalTickTime+=GDeltaTime;
	}
	// after the first frame, 
	else if( GFrameCounter == 1 )
	{
		for (INT PlayerIndex = 0; PlayerIndex < GEngine->GamePlayers.Num(); PlayerIndex++)
		{
			GEngine->GamePlayers(PlayerIndex)->Actor->eventOnEngineInitialTick();
		}
	}

	// Find the objects which need to be cleaned up the next frame.
	FPendingCleanupObjects* PreviousPendingCleanupObjects = PendingCleanupObjects;
	PendingCleanupObjects = GetPendingCleanupObjects();

	// Sync game and render thread. Either total sync or allowing one frame lag.
	static FFrameEndSync FrameEndSync;
	FrameEndSync.Sync( GSystemSettings.bAllowOneFrameThreadLag );

	// Delete the objects which were enqueued for deferred cleanup before the previous frame.
	delete PreviousPendingCleanupObjects;

#if !CONSOLE && !PLATFORM_UNIX
	// Handle all incoming messages if we're not using wxWindows in which case this is done by their
	// message loop.
	if( !GUsewxWindows )
	{
		appWinPumpMessages();
	}

	// check to see if the window in the foreground was made by this process (ie, does this app
	// have focus)
	DWORD ForegroundProcess;
	GetWindowThreadProcessId(GetForegroundWindow(), &ForegroundProcess);
	UBOOL HasFocus = ForegroundProcess == GetCurrentProcessId();

	// If editor thread doesn't have the focus, don't suck up too much CPU time.
	if( GIsEditor )
	{
		static UBOOL HadFocus=1;
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
			appSleep(0.005f);
		}
		HadFocus = HasFocus;
	}

	// if its our window, allow sound, otherwise silence it
	GVolumeMultiplier = HasFocus ? 1.0f : 0.0f;

#elif PLATFORM_MACOSX
	appMacPumpMessages();
#endif

	if( GIsBenchmarking )
	{
#if STATS
		FrameTimes.AddItem( GFPSCounter.GetFrameTime() * 1000.f );
#endif
	}

#if XBOX
	// Handle remote debugging commands.
	{
		FScopeLock ScopeLock(&RemoteDebugCriticalSection);
		if( RemoteDebugCommand[0] != '\0' )
		{
			new(GEngine->DeferredCommands) FString(ANSI_TO_TCHAR(RemoteDebugCommand));
		}
		RemoteDebugCommand[0] = '\0';
	}
#endif

#if WITH_UE3_NETWORKING && !SHIPPING_PC_GAME
	if ( GDebugChannel )
	{
		GDebugChannel->Tick();
	}
#endif	//#if WITH_UE3_NETWORKING

	// Execute all currently queued deferred commands (allows commands to be queued up for next frame).
	const INT DeferredCommandsCount = GEngine->DeferredCommands.Num();
	for( INT DeferredCommandsIndex=0; DeferredCommandsIndex<DeferredCommandsCount; DeferredCommandsIndex++ )
	{
		// Use LocalPlayer if available...
		if( GEngine->GamePlayers.Num() && GEngine->GamePlayers(0) )
		{
			ULocalPlayer* Player = GEngine->GamePlayers(0);
			{
				Player->Exec( *GEngine->DeferredCommands(DeferredCommandsIndex), *GLog );
			}
		}
		// and fall back to UEngine otherwise.
		else
		{
			GEngine->Exec( *GEngine->DeferredCommands(DeferredCommandsIndex), *GLog );
		}
	}
	GEngine->DeferredCommands.Remove(0, DeferredCommandsCount);

#if STATS
		// update some engine-specific stats
		SET_MEMORY_STAT(STAT_LandscapeHeightmapTextureMem, GET_MEMORY_STAT(STAT_TEXTUREGROUP_Terrain_Heightmap));
		SET_MEMORY_STAT(STAT_LandscapeWeightmapTextureMem, GET_MEMORY_STAT(STAT_TEXTUREGROUP_Terrain_Weightmap));

		extern void FinishOneFrameParticleStats();
		FinishOneFrameParticleStats();
	} // Close extra scope for FrameTime stat
#endif
}


/*-----------------------------------------------------------------------------
	Static linking support.
-----------------------------------------------------------------------------*/

/**
 * Initializes all registrants and names for static linking support.
 */
void InitializeRegistrantsAndRegisterNames()
{
	GUglyHackFlags |= HACK_SkipCopyDuringRegistration;

	// Static linking.
	INT Lookup = 0;
	// Auto-generated lookups and statics
	AutoInitializeRegistrantsCore( Lookup );
	AutoInitializeRegistrantsEngine( Lookup );
	AutoInitializeRegistrantsGameFramework( Lookup );
#if WITH_UE3_NETWORKING
	AutoInitializeRegistrantsIpDrv( Lookup );
#endif	//#if WITH_UE3_NETWORKING
#if !PS3 && !MOBILE && !PLATFORM_MACOSX && !WIIU
	AutoInitializeRegistrantsXAudio2( Lookup );
#endif
#if PLATFORM_MACOSX
	AutoInitializeRegistrantsCoreAudio( Lookup );
#endif

	AutoInitializeRegistrantsGFxUI( Lookup );

#if defined(XBOX)
	AutoInitializeRegistrantsXeDrv( Lookup );
#elif PS3
	AutoInitializeRegistrantsPS3Drv( Lookup );
#elif NGP
	AutoInitializeRegistrantsNGPDrv( Lookup );
#elif WIIU
	AutoInitializeRegistrantsWiiUDrv( Lookup );
#elif IPHONE
	AutoInitializeRegistrantsIPhoneDrv( Lookup );
#elif ANDROID
	AutoInitializeRegistrantsAndroidDrv( Lookup );
	AutoInitializeRegistrantsOpenSLAudio( Lookup );
#elif FLASH
	AutoInitializeRegistrantsFlashDrv( Lookup );
#elif PLATFORM_MACOSX
	AutoInitializeRegistrantsMacDrv( Lookup );
#elif _WINDOWS  // !!! FIXME: some of these will be Unix, too!

#if WITH_EDITOR
	AutoInitializeRegistrantsUnrealEd( Lookup );
	#if WITH_GFx
		AutoInitializeRegistrantsGFxUIEditor( Lookup );
	#endif // WITH_GFx
#endif

#if WITH_UE3_NETWORKING
	AutoInitializeRegistrantsWinDrv( Lookup );
#endif

#endif

#if WITH_UE3_NETWORKING
	// these control what functions are called and therefore linked, so we have to use the OSS
	// that we compiled with (based on WITH_* style flags), not runtime checks with GetOSSPacakgeName
	#if XBOX || WITH_PANORAMA
		AutoInitializeRegistrantsOnlineSubsystemLive( Lookup );
		AutoGenerateNamesOnlineSubsystemLive();
	#elif WITH_STEAMWORKS
		AutoInitializeRegistrantsOnlineSubsystemSteamworks( Lookup );
		AutoGenerateNamesOnlineSubsystemSteamworks();
	#elif WITH_GAMESPY
		AutoInitializeRegistrantsOnlineSubsystemGameSpy( Lookup );
		AutoGenerateNamesOnlineSubsystemGameSpy();
	#elif WITH_GAMECENTER
		AutoInitializeRegistrantsOnlineSubsystemGameCenter( Lookup );
		AutoGenerateNamesOnlineSubsystemGameCenter();
	#else
		AutoInitializeRegistrantsOnlineSubsystemPC( Lookup );
		AutoGenerateNamesOnlineSubsystemPC();
	#endif
#endif

#if GAMENAME == GEARGAME
	AutoInitializeRegistrantsGearGame( Lookup );
#if _WINDOWS && WITH_EDITOR
	AutoInitializeRegistrantsGearEditor( Lookup );
#endif

#elif GAMENAME == FORTRESSGAME

	AutoInitializeRegistrantsFortressBase( Lookup );
	AutoInitializeRegistrantsFortress( Lookup );

#if _WINDOWS && WITH_EDITOR
	AutoInitializeRegistrantsFortressEditor( Lookup );
#endif

#elif GAMENAME == UTGAME
	AutoInitializeRegistrantsUDKBase( Lookup );
#if _WINDOWS && WITH_EDITOR
	AutoInitializeRegistrantsUTEditor( Lookup );
#endif

#elif GAMENAME == DUKEGAME
	AutoInitializeRegistrantsDukeGame( Lookup );
#if _WINDOWS && WITH_EDITOR
	AutoInitializeRegistrantsDukeEditor( Lookup );
#endif

#elif GAMENAME == EXOGAME
		AutoInitializeRegistrantsExoGame( Lookup );
#if _WINDOWS && WITH_EDITOR
		AutoInitializeRegistrantsExoEditor( Lookup );
#endif

#elif GAMENAME == SWORDGAME
	AutoInitializeRegistrantsSwordGame( Lookup );
#if _WINDOWS && WITH_EDITOR
	AutoInitializeRegistrantsSwordEditor( Lookup );
#endif

#else
	#error Hook up your game name here
#endif

#if WITH_SUBSTANCE_AIR
	AutoInitializeRegistrantsSubstanceAir( Lookup );
#if _WINDOWS && WITH_EDITOR
	AutoInitializeRegistrantsSubstanceAirEd( Lookup );
#endif
#endif

//	AutoGenerateNames* declarations.
	AutoGenerateNamesCore();
	AutoGenerateNamesEngine();
	AutoGenerateNamesGameFramework();
#if WITH_GFx
	AutoGenerateNamesGFxUI();
#endif // WITH_GFx
#if _WINDOWS && WITH_EDITOR
	AutoGenerateNamesUnrealEd();
	#if WITH_GFx
		AutoGenerateNamesGFxUIEditor();
	#endif // WITH_GFx
#endif
#if WITH_UE3_NETWORKING
	AutoGenerateNamesIpDrv();
#if _WINDOWS
	AutoGenerateNamesWinDrv();
#endif
#endif	//#if WITH_UE3_NETWORKING

#if GAMENAME == GEARGAME
	AutoGenerateNamesGearGame();
	#if _WINDOWS && WITH_EDITOR
		AutoGenerateNamesGearEditor();
	#endif


#elif GAMENAME == FORTRESSGAME
	AutoGenerateNamesFortressBase();
	AutoGenerateNamesFortress();
#if _WINDOWS && WITH_EDITOR
	AutoGenerateNamesFortressEditor();
#endif


#elif GAMENAME == UTGAME
	AutoGenerateNamesUDKBase();
	#if _WINDOWS && WITH_EDITOR
		AutoGenerateNamesUTEditor();
	#endif

#elif GAMENAME == DUKEGAME
	AutoGenerateNamesDukeGame();
	#if _WINDOWS && WITH_EDITOR
		AutoGenerateNamesDukeEditor();
	#endif

#elif GAMENAME == EXOGAME
	AutoGenerateNamesExoGame();
	#if _WINDOWS && WITH_EDITOR
		AutoGenerateNamesExoEditor();
	#endif

#elif GAMENAME == SWORDGAME
	AutoGenerateNamesSwordGame();
	#if _WINDOWS && WITH_EDITOR
		AutoGenerateNamesSwordEditor();
	#endif

#else
	#error Hook up your game name here
#endif

#if WITH_SUBSTANCE_AIR
	AutoGenerateNamesSubstanceAir();
#if _WINDOWS && WITH_EDITOR
		AutoGenerateNamesSubstanceAirEd();
#endif
#endif

	GUglyHackFlags &= ~HACK_SkipCopyDuringRegistration;
}


/** 
* Returns whether the line can be broken between these two characters
*/
UBOOL appCanBreakLineAt( TCHAR Previous, TCHAR Current )
{
#if XBOX
	// Use the word-wrapping code from the XDK sample source
	extern bool WordWrap_CanBreakLineAt( WCHAR prev, WCHAR curr );
	return WordWrap_CanBreakLineAt(Previous, Current);
#else
	return (appIsPunct(Previous) && Previous != TEXT('\'')) || appIsWhitespace(Current);
#endif
}

/**
 * @return TRUE if game supports listening for tilt pusher network messages (as set in UBT)
 */
UBOOL appGameSupportsTiltPusher()
{
#if SUPPORTS_TILT_PUSHER
	return TRUE;
#endif
	return FALSE;
}

/*-----------------------------------------------------------------------------
	Remote debug channel support.
-----------------------------------------------------------------------------*/

#if (defined XBOX) && ALLOW_NON_APPROVED_FOR_SHIPPING_LIB

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

