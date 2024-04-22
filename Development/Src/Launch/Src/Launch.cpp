/*=============================================================================
	Launch.cpp: Game launcher.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "LaunchPrivate.h"
#include "NvApexManager.h"

#if PLATFORM_DESKTOP && !CONSOLE

#include "SplashScreen.h"
#if WITH_OPEN_AUTOMATE
#include "OpenAutomate.h"
#endif

#if HAVE_WXWIDGETS
// use wxWidgets as a DLL
#include <wx/evtloop.h>  // has our base callback class
#endif

#if _WINDOWS && defined(_DEBUG)
#include <crtdbg.h>
#endif

FEngineLoop	GEngineLoop;
/** Whether to use wxWindows when running the game */
UBOOL		GUsewxWindows = FALSE;

/** Whether we should pause before exiting. used by UCC */
UBOOL		GShouldPauseBeforeExit;

/** Whether we should generate crash reports even if the debugger is attached. */
UBOOL		GAlwaysReportCrash = FALSE;

extern "C" int test_main(int argc, char ** argp)
{
	return 0;
}

/*-----------------------------------------------------------------------------
	WinMain.
-----------------------------------------------------------------------------*/

#if _WINDOWS
extern TCHAR MiniDumpFilenameW[1024];
extern INT CreateMiniDump( LPEXCEPTION_POINTERS ExceptionInfo );

// use wxWidgets as a DLL
extern bool IsUnrealWindowHandle( HWND hWnd );
#endif



/**
 * Performs any required cleanup in the case of a fatal error.
 */
static void StaticShutdownAfterError()
{
	// Make sure Novodex is correctly torn down.
	DestroyGameRBPhys();

#if WITH_FACEFX
	// Make sure FaceFX is shutdown.
	UnShutdownFaceFX();
#endif // WITH_FACEFX

#if HAVE_WXWIDGETS
	// Unbind DLLs (e.g. SCC integration)
	WxLaunchApp* LaunchApp = (WxLaunchApp*) wxTheApp;
	if( LaunchApp )
	{
		LaunchApp->ShutdownAfterError();
	}
#endif
}



#if HAVE_WXWIDGETS
class WxUnrealCallbacks : public wxUnrealCallbacks
{
public:

	virtual bool IsUnrealWindowHandle(HWND hwnd) const
	{
		return ::IsUnrealWindowHandle(hwnd);
	}

	virtual bool IsRequestingExit() const 
	{ 
		return GIsRequestingExit ? true : false; 
	}

	virtual void SetRequestingExit( bool bRequestingExit ) 
	{ 
		GIsRequestingExit = bRequestingExit ? true : false; 
	}


	/** Called by WxWidgets if it catches an SEH exception within a WndProc */
	virtual INT WndProcExceptionFilter( LPEXCEPTION_POINTERS ExceptionInfo )
	{
		if( IsDebuggerPresent() )
		{
			// Break on exception thrown from WxWidgets as WndProc will sometimes swallow the
			// exception and we otherwise wouldn't get a chance to break.  Note that this is
			// likely a fatal error and if you opt to continue execution, state may be corrupt!
			appDebugBreak();
			return EXCEPTION_CONTINUE_SEARCH;
		}

		return CreateMiniDump( ExceptionInfo );
	}



	/** Called for unhandled exceptions from WxWidgets WndProc functions */
	virtual void WndProcUnhandledExceptionCallback()
	{
		GError->HandleError();
		StaticShutdownAfterError();
	}

};

static WxUnrealCallbacks s_UnrealCallbacks;
#endif //HAVE_WXWIDGETS



/**
 * Sets global to TRUE if the app should pause infinitely before exit.
 * Currently used by UCC.
 */
void SetShouldPauseBeforeExit(INT ErrorLevel)
{
	// If we are UCC, determine 
	if( GIsUCC )
	{
		// UCC.
		UBOOL bInheritConsole = FALSE;

#if !CONSOLE
		if(NULL != GLogConsole)
		{
			// if we're running from a console we inherited, do not sleep indefinitely
			bInheritConsole = GLogConsole->IsInherited();
		}
#endif

		// Either close log window manually or press CTRL-C to exit if not in "silent" or "nopause" mode.
		GShouldPauseBeforeExit = !bInheritConsole && !GIsSilent && !ParseParam(appCmdLine(),TEXT("NOPAUSE"));
		// if it was specified to not pause if successful, then check that here
		if (ParseParam(appCmdLine(),TEXT("NOPAUSEONSUCCESS")) && ErrorLevel == 0)
		{
			// we succeeded, so don't pause 
			GShouldPauseBeforeExit = FALSE;
		}
	}
}

/** 
 * PreInits the engine loop 
 */
INT EnginePreInit( const TCHAR* CmdLine )
{
	INT ErrorLevel = GEngineLoop.PreInit( CmdLine );

	SetShouldPauseBeforeExit( ErrorLevel );

	return( ErrorLevel );
}

/** 
 * Inits the engine loop 
 */
INT EngineInit( const TCHAR* SplashName )
{
	appShowSplash( SplashName );

	INT ErrorLevel = GEngineLoop.Init();

	if ( !GIsGame )
	{
		appHideSplash();
	}

	return( ErrorLevel );
}

/** 
 * Ticks the engine loop 
 */
void EngineTick( void )
{
	GEngineLoop.Tick();
}

/**
 * Shuts down the engine
 */
void EngineExit( void )
{
	// Make sure this is set
	GIsRequestingExit = TRUE;

	GEngineLoop.Exit();
}

/**
 * Static guarded main function. Rolled into own function so we can have error handling for debug/ release builds depending
 * on whether a debugger is attached or not.
 */
INT GuardedMain( const TCHAR* CmdLine, HINSTANCE hInInstance, HINSTANCE hPrevInstance, INT nCmdShow )
{
	// For unix based OS's, it is >essential< that this is called as early on in the process as possible;
	//	it determines the base directory by caching the current working directory (and the working directory is changed regularly later).
	appBaseDir();

	// make sure GEngineLoop::Exit() is always called.
	struct EngineLoopCleanupGuard 
	{ 
		~EngineLoopCleanupGuard()
		{
			EngineExit();
		}
	} CleanupGuard;

	// Set up minidump filename. We cannot do this directly inside main as we use an FString that requires 
	// destruction and main uses SEH.
	// These names will be updated as soon as the Filemanager is set up so we can write to the log file.
	// That will also use the user folder for installed builds so we don't write into program files or whatever.
#if _WINDOWS
	appStrcpy( MiniDumpFilenameW, *FString::Printf( TEXT("unreal-v%i-%s.dmp"), GEngineVersion, *appSystemTimeString() ) );

	CmdLine = RemoveExeName(CmdLine);
#endif

#if DEDICATED_SERVER
	//Made to match size of GCmdLine in UnMisc.cpp
	TCHAR ServerCmdLine[16384];
	ServerCmdLine[0] = '\0';
	//Inject server commandlet onto commandline
	appStrncat( ServerCmdLine, TEXT("SERVER "), ARRAY_COUNT(ServerCmdLine) );
	appStrncat( ServerCmdLine, CmdLine, ARRAY_COUNT(ServerCmdLine) );
	CmdLine = ServerCmdLine;
#endif

#if WITH_STEAMWORKS
	extern void appSteamHandleCmdLine(const TCHAR** CmdLine);
	appSteamHandleCmdLine(&CmdLine);
#endif

#if WITH_OPEN_AUTOMATE
	if( ParseParam( CmdLine, TEXT( "OPENAUTOMATE"), TRUE ) )
	{
		GOpenAutomate = new FOpenAutomate();
	}
#endif // WITH_OPEN_AUTOMATE

	INT ErrorLevel = EnginePreInit( CmdLine );

	GUsewxWindows = 0;
#if HAVE_WXWIDGETS
	GUsewxWindows	= GIsEditor || ParseParam(appCmdLine(),TEXT("WXWINDOWS")) || ParseParam(appCmdLine(),TEXT("REMOTECONTROL"));
#endif

	// exit if PreInit failed.
	if ( ErrorLevel != 0 || GIsRequestingExit )
	{
		return ErrorLevel;
	}

	if( GUsewxWindows )
	{
#if HAVE_WXWIDGETS
		// use wxWidgets as a DLL
		// set the call back class here
		SetUnrealCallbacks( &s_UnrealCallbacks );

		// UnrealEd of game with wxWindows.
		ErrorLevel = wxEntry( hInInstance, hPrevInstance, "", nCmdShow);
#endif
	}
#if WITH_OPEN_AUTOMATE
	else if( ( GOpenAutomate != NULL ) && !GIsEditor )
	{
		ErrorLevel = EngineInit( TEXT( "PC\\Splash.bmp" ) );

		if( GOpenAutomate->Init( CmdLine ) )
		{
			GIsRequestingExit = GOpenAutomate->ProcessLoop();

			while( !GIsRequestingExit )
			{
				EngineTick();
			}
		}

		delete GOpenAutomate;
		GOpenAutomate = NULL;
	}
#endif // WITH_OPEN_AUTOMATE
	else
	{
#if PLATFORM_MACOSX
		ErrorLevel = EngineInit( TEXT("Mac\\Splash.bmp") );
#else
		// Game without wxWindows.
		ErrorLevel = EngineInit( GIsEditor ? TEXT("PC\\EdSplash.bmp") : TEXT("PC\\Splash.bmp") );
#endif

		while( !GIsRequestingExit )
		{
			EngineTick();
		}
	}
	return ErrorLevel;
}

#if _WINDOWS
/**
 * Maintain a named mutex to detect whether we are the first instance of this game
 */
HANDLE GNamedMutex = NULL;

void ReleaseNamedMutex( void )
{
	if( GNamedMutex )
	{
		ReleaseMutex( GNamedMutex );
		GNamedMutex = NULL;
	}
}

UBOOL MakeNamedMutex( const TCHAR* CmdLine )
{
	UBOOL bIsFirstInstance = FALSE;

	TCHAR MutexName[MAX_SPRINTF] = TEXT( "" );
	appSprintf( MutexName, TEXT( "UnrealEngine3_%d" ), GAMENAME );

	GNamedMutex = CreateMutex( NULL, TRUE, MutexName );

	if( GNamedMutex	&& GetLastError() != ERROR_ALREADY_EXISTS && !ParseParam( CmdLine, TEXT( "NEVERFIRST" ) ) )
	{
		// We're the first instance!
		bIsFirstInstance = TRUE;
	}
	else
	{
		// Still need to release it in this case, because it gave us a valid copy
		ReleaseNamedMutex();
		// There is already another instance of the game running.
		bIsFirstInstance = FALSE;
	}

	return( bIsFirstInstance );
}

/**
 * Handler for CRT parameter validation. Triggers error
 *
 * @param Expression - the expression that failed crt validation
 * @param Function - function which failed crt validation
 * @param File - file where failure occured
 * @param Line - line number of failure
 * @param Reserved - not used
 */
void InvalidParameterHandler(const TCHAR* Expression,
							 const TCHAR* Function, 
							 const TCHAR* File, 
							 UINT Line, 
							 uintptr_t Reserved)
{
	appErrorf(TEXT("SECURE CRT: Invalid parameter detected.\nExpression: %s Function: %s. File: %s Line: %d\n"), 
		Expression ? Expression : TEXT("Unknown"), 
		Function ? Function : TEXT("Unknown"), 
		File ? File : TEXT("Unknown"), 
		Line );
}

/**
 * Setup the common debug settings 
 */
void SetupWindowsEnvironment( void )
{
	// all crt validation should trigger the callback
	_set_invalid_parameter_handler(InvalidParameterHandler);

#ifdef _DEBUG
	// Disable the message box for assertions and just write to debugout instead
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_DEBUG );
	// don't fill buffers with 0xfd as we make assumptions for FNames st we only use a fraction of the entire buffer
	_CrtSetDebugFillThreshold( 0 );
#endif
}

#if WITH_MANAGED_CODE
// Implemented in ManagedCodeSupportCLR.cpp
extern INT ManagedGuardedMain( const TCHAR* CmdLine, HINSTANCE hInInstance, HINSTANCE hPrevInstance, INT nCmdShow );
#endif

/**
 * The inner exception handler catches crashes/asserts in native C++ code and is the only way to get the correct callstack
 * when running a 64-bit executable. However, XAudio2 doesn't always like this and it may result in no sound.
 */
#if _WIN64
	UBOOL GEnableInnerException = TRUE;
#else
	UBOOL GEnableInnerException = FALSE;
#endif

/**
 * Called from Managed code.
 * The inner exception handler catches crashes/asserts in native C++ code and is the only way to get the correct callstack
 * when running a 64-bit executable. However, XAudio2 doesn't like this and it may result in no sound.
 */
INT GuardedMainWrapper( const TCHAR* CmdLine, HINSTANCE hInInstance, HINSTANCE hPrevInstance, INT nCmdShow )
{
	INT ErrorLevel = 0;
	if ( GEnableInnerException )
	{
	 	__try
		{
			// Run the guarded code.
			ErrorLevel = GuardedMain( CmdLine, hInInstance, hPrevInstance, nCmdShow );
		}
		__except( CreateMiniDump( GetExceptionInformation() ), EXCEPTION_CONTINUE_SEARCH )
		{
		}
	}
	else
	{
		// Run the guarded code.
		ErrorLevel = GuardedMain( CmdLine, hInInstance, hPrevInstance, nCmdShow );
	}
	return ErrorLevel;
}

INT WINAPI WinMain( HINSTANCE hInInstance, HINSTANCE hPrevInstance, char*, INT nCmdShow )
{
	// Setup common Windows settings
	SetupWindowsEnvironment();

	// default to no game
	appStrcpy(GGameName, TEXT("None"));

	INT ErrorLevel			= 0;
	GIsStarted				= 1;
	hInstance				= hInInstance;
	const TCHAR* CmdLine	= GetCommandLine();
	
#if !SHIPPING_PC_GAME && !CONSOLE
	// Named mutex we use to figure out whether we are the first instance of the game running. This is needed to e.g.
	// make sure there is no contention when trying to save the shader cache.
	GIsFirstInstance = MakeNamedMutex( CmdLine );

	if ( ParseParam( CmdLine,TEXT("crashreports") ) )
	{
		GAlwaysReportCrash = TRUE;
	}
#endif

	// Using the -noinnerexception parameter will disable the exception handler within native C++, which is call from managed code,
	// which is called from this function.
	// The default case is to have three wrapped exception handlers (note that wxWidgets also use an exception handler, which would be a 4th one):
	// Native: WinMain() -> Managed: ManagedGuardedMain() -> Native: GuardedMainWrapper().
	// The inner exception handler in GuardedMainWrapper() catches crashes/asserts in native C++ code and is the only way to get the
	// correct callstack when running a 64-bit executable. However, XAudio2 sometimes (?) don't like this and it may result in no sound.
#if _WIN64
	if ( ParseParam(CmdLine,TEXT("noinnerexception")) || GIsBenchmarking )
	{
		GEnableInnerException = FALSE;
	}
#endif

#if defined( _DEBUG )
	if( TRUE && !GAlwaysReportCrash )
#else
	if( appIsDebuggerPresent() && !GAlwaysReportCrash )
#endif
	{
		// Don't use exception handling when a debugger is attached to exactly trap the crash. This does NOT check
		// whether we are the first instance or not!
		ErrorLevel = GuardedMain( CmdLine, hInInstance, hPrevInstance, nCmdShow );
	}
	else
	{
		// Use structured exception handling to trap any crashes, walk the the stack and display a crash dialog box.
 		__try
 		{
			GIsGuarded = 1;
			// Run the guarded code.
#if WITH_MANAGED_CODE
			ErrorLevel = ManagedGuardedMain( CmdLine, hInInstance, hPrevInstance, nCmdShow );
#else
			ErrorLevel = GuardedMainWrapper( CmdLine, hInInstance, hPrevInstance, nCmdShow );
#endif
			GIsGuarded = 0;
		}
		__except( GEnableInnerException ? EXCEPTION_EXECUTE_HANDLER : CreateMiniDump( GetExceptionInformation() ) )
		{
#if !SHIPPING_PC_GAME && !CONSOLE
			// Release the mutex in the error case to ensure subsequent runs don't find it.
			ReleaseNamedMutex();
#endif
			// Crashed.
			ErrorLevel = 1;
			GError->HandleError();
			StaticShutdownAfterError();
			appRequestExit( TRUE );
		}
	}

	// Final shut down.
	appExit();

#if !SHIPPING_PC_GAME && !CONSOLE
	// Release the named mutex again now that we are done.
	ReleaseNamedMutex();
#endif

	// pause if we should
	if (GShouldPauseBeforeExit)
	{
		Sleep(INFINITE);
	}
	
	GIsStarted = 0;
	return ErrorLevel;
}

#ifdef _WINDLL

INT PIBGuardedMainInit( const TCHAR* CmdLine )
{
	INT ErrorLevel = EnginePreInit( CmdLine );

	// exit if PreInit failed.
	if ( ErrorLevel != 0 || GIsRequestingExit )
	{
		return( 1 );
	}

	ErrorLevel = EngineInit( TEXT("PCConsole\\Splash.bmp") );

	return( ErrorLevel );
}

/** 
 * Inits the loaded DLL version of UE3
 */
INT PIBInit( HINSTANCE hInInstance, const TCHAR* CmdLine )
{
	SetupWindowsEnvironment();

	// default to no game
	appStrcpy(GGameName, TEXT("None"));

	INT ErrorLevel = 0;
	GIsStarted = 1;
	hInstance = hInInstance;

#ifdef _DEBUG
	if( TRUE )
#else
	if( IsDebuggerPresent() )
#endif
	{
		// Don't use exception handling when a debugger is attached to exactly trap the crash. This does NOT check
		// whether we are the first instance or not!
		ErrorLevel = PIBGuardedMainInit( CmdLine );
	}
	else
	{
		// Use structured exception handling to trap any crashes, walk the the stack and display a crash dialog box.
		__try
		{
			GIsGuarded = 1;
			ErrorLevel = PIBGuardedMainInit( CmdLine );
			GIsGuarded = 0;
		}
		__except( CreateMiniDump( GetExceptionInformation() ) )
		{
			// Crashed.
			ReleaseNamedMutex();

			ErrorLevel = 1;
			GError->HandleError();
			StaticShutdownAfterError();
			appRequestExit( TRUE );
		}
	}

	return( ErrorLevel );
}

/**
 * Shuts the loaded DLL version of UE3 down
 */
void PIBShutdown( void )
{
	EngineExit();

	appExit();

	GIsStarted = 0;
}

#endif // _WINDLL

#endif
#endif
