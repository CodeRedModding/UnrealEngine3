/*=============================================================================
	Launch.cpp: Game launcher.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
	* Rewritten by Daniel Vogel.
=============================================================================*/

#define KEEP_ALLOCATION_BACKTRACE	0

#include "LaunchPrivate.h"

FEngineLoop	GEngineLoop;

/*-----------------------------------------------------------------------------
	WinMain.
-----------------------------------------------------------------------------*/

#ifndef _DEBUG
extern TCHAR MiniDumpFilenameW[64];
extern char  MiniDumpFilenameA[64];
extern INT CreateMiniDump( LPEXCEPTION_POINTERS ExceptionInfo );
#endif

INT WINAPI WinMain( HINSTANCE hInInstance, HINSTANCE hPrevInstance, char*, INT nCmdShow )
{
    appInitTimeing();
#if   GAMENAME == DEMOGAME
	appStrcpy( GGameName, TEXT("Demo") );
#elif GAMENAME == WARGAME
	appStrcpy( GGameName, TEXT("War") );
#elif GAMENAME == UTGAME
	appStrcpy( GGameName, TEXT("UT") );
#else
	#error Hook up your game name here
#endif

	INT ErrorLevel			= 0;

	GIsStarted				= 1;
	hInstance				= hInInstance;

	const TCHAR* CmdLine	= GetCommandLine();
	
#ifndef _DEBUG
	// Set up minidump filename.
	appStrcpy( MiniDumpFilenameW, TEXT("unreal-v") );
	appStrcat( MiniDumpFilenameW, appItoa( GEngineVersion ) );
	appStrcat( MiniDumpFilenameW, TEXT(".dmp") );
	strcpy( MiniDumpFilenameA, TCHAR_TO_ANSI( MiniDumpFilenameW ) );

	__try
#endif
	{
		GIsGuarded			= 1;
		ErrorLevel			= GEngineLoop.Init( CmdLine );
		UBOOL UsewxWindows	= !(GIsUCC || ParseParam(appCmdLine(),TEXT("nowxwindows")));

		if( UsewxWindows )
		{
			// UnrealEd of game with wxWindows.
			ErrorLevel = wxEntry((WXHINSTANCE) hInInstance, (WXHINSTANCE) hPrevInstance, "", nCmdShow);
		}
		else if( GIsUCC )
		{
			// UCC.

			// Either close log window manually or press CTRL-C to exit if not in "silent" or "nopause" mode.
			if( !ParseParam(appCmdLine(),TEXT("SILENT")) && !ParseParam(appCmdLine(),TEXT("NOPAUSE")) )
			{
				GLog->TearDown();
				Sleep(INFINITE);
			}
		}
		else
		{
			// Game without wxWindows.
			while( !GIsRequestingExit )
				GEngineLoop.Tick();
		}

		GEngineLoop.Exit();

		GIsGuarded = 0;
	}
#ifndef _DEBUG
	__except( CreateMiniDump( GetExceptionInformation() ) )
	{
		// Crashed.
		ErrorLevel = 1;
		GError->HandleError();
	}
#endif

	// Final shut down.
	appExit();
	GIsStarted = 0;
	return ErrorLevel;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/