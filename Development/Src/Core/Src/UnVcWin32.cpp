/*=============================================================================
	UnVcWin32.cpp: Visual C++ Windows 32-bit core.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#if _WINDOWS && !CONSOLE

// Core includes.
#include "UnThreadingWindows.h"		// For getting thread context in the stack walker.
#include "../../IpDrv/Inc/UnIpDrv.h"		// For GDebugChannel
#include "ChartCreation.h"
#ifdef _WINDLL
#include "PIB.h"
#endif // _WINDLL

#pragma pack(push,8)
#include <stdio.h>
#include <float.h>
#include <time.h>
#include <io.h>
#include <direct.h>
#include <errno.h>
#include <sys/stat.h>

#include <string.h>
#include <ShellAPI.h>
#include <MMSystem.h>

#include <rpcsal.h>					// from DXSDK
#include <gameux.h>					// from DXSDK For IGameExplorer
#include <shlobj.h>
#include <intshcut.h>

// For getting the MAC address
#include <WinSock.h>
#include <Iphlpapi.h>

#include <wincrypt.h>
#pragma pack(pop)

// Resource includes.
#include "../../Launch/Resources/Resource.h"

#include "PreWindowsApi.h"

#if WITH_FIREWALL_SUPPORT
// For firewall integration - obtained from the latest Windows/Platform SDK
#include <netfw.h>
#endif

#include "PostWindowsApi.h"

#if !CONSOLE
#include "../../WinDrv/Inc/WinDrv.h"
#endif
#include "Database.h"

#if (WITH_IME && WITH_GFx && WITH_GFx_IME)
#include "EngineUserInterfaceClasses.h"
#include "GFxUIClasses.h"
#include "ScaleformEngine.h"
#if WITH_GFx_IME
    #pragma pack(push,8)
    #include "GFx/IME/GFx_IMEManager.h"
    #pragma pack(pop)
#endif
#endif

// Link with the Wintrust.lib file.
#pragma comment( lib,"Crypt32.lib" )
#pragma comment( lib, "Iphlpapi.lib" )

/** Whether we should be checking for device lost every drawcall. */
UBOOL GParanoidDeviceLostChecking = TRUE;

/** Resource ID of icon to use for Window */
extern INT GGameIcon;

/** Handle for the initial game Window */
extern HWND GGameWindow;

/** Whether the game is using the startup window procedure */
extern UBOOL GGameWindowUsingStartupWindowProc;

/** Width of the primary monitor, in pixels. */
extern INT GPrimaryMonitorWidth;
/** Height of the primary monitor, in pixels. */
extern INT GPrimaryMonitorHeight;
/** Rectangle of the work area on the primary monitor (excluding taskbar, etc) in "virtual screen coordinates" (pixels). */
extern RECT GPrimaryMonitorWorkRect;
/** Virtual screen rectangle including all monitors. */
extern RECT GVirtualScreenRect;

/** Whether we should generate crash reports even if the debugger is attached. */
extern UBOOL GAlwaysReportCrash;

/*----------------------------------------------------------------------------
	Hotkeys for the 'Yes/No to All' dialog
----------------------------------------------------------------------------*/

#define HOTKEY_YES			100
#define HOTKEY_NO			101
#define HOTKEY_CANCEL		102

/*----------------------------------------------------------------------------
	Misc functions.
----------------------------------------------------------------------------*/

/**
 * Displays extended message box allowing for YesAll/NoAll
 * @return 3 - YesAll, 4 - NoAll, -1 for Fail
 */
int MessageBoxExt( EAppMsgType MsgType, HWND HandleWnd, const TCHAR* Text, const TCHAR* Caption );

/**
 * Callback for MessageBoxExt dialog (allowing for Yes to all / No to all and Cancel )
 * @return		One of ART_Yes, ART_Yesall, ART_No, ART_NoAll, ART_Cancel.
 */
PTRINT CALLBACK MessageBoxDlgProc( HWND HandleWnd, UINT Message, WPARAM WParam, LPARAM LParam );

/**
 * Handles IO failure by ending gameplay.
 *
 * @param Filename	If not NULL, name of the file the I/O error occured with
 */
void appHandleIOFailure( const TCHAR* Filename )
{
	appErrorf(TEXT("I/O failure operating on '%s'"), Filename ? Filename : TEXT("Unknown file"));
}

/**
 * Platform specific function for adding a named event that can be viewed in PIX
 */
void appBeginNamedEvent(const FColor& Color,const TCHAR* Text)
{
}

/**
 * Platform specific function for closing a named event that can be viewed in PIX
 */
void appEndNamedEvent()
{
}

/**
 * Rebuild the commandline if needed
 *
 * @param NewCommandLine The commandline to fill out
 *
 * @return TRUE if NewCommandLine should be pushed to GCmdLine
 */
UBOOL appResetCommandLine(TCHAR NewCommandLine[16384])
{
	return FALSE;
}

/*-----------------------------------------------------------------------------
	FOutputDeviceWindowsError.
-----------------------------------------------------------------------------*/

//
// Sends the message to the debugging output.
//
void appOutputDebugString( const TCHAR *Message )
{
	OutputDebugString( Message );
#if WITH_UE3_NETWORKING && !SHIPPING_PC_GAME
	if ( GDebugChannel )
	{
		GDebugChannel->SendText( Message );
	}
#endif	//#if WITH_UE3_NETWORKING
}

/** Sends a message to a remote tool. */
void appSendNotificationString( const ANSICHAR *Message )
{
	FANSIToTCHAR MsgWide(Message);
	OutputDebugString( MsgWide );
#if WITH_UE3_NETWORKING && !SHIPPING_PC_GAME
	if ( GDebugChannel )
	{
		GDebugChannel->SendText( MsgWide );
	}
#endif	//#if WITH_UE3_NETWORKING
}

/** Sends a message to a remote tool. */
void appSendNotificationString( const TCHAR* Message )
{
	OutputDebugString( Message );
#if WITH_UE3_NETWORKING && !SHIPPING_PC_GAME
	if ( GDebugChannel )
	{
		GDebugChannel->SendText( Message );
	}
#endif	//#if WITH_UE3_NETWORKING
}

//
// Immediate exit.
//
void appRequestExit( UBOOL Force )
{
	debugf( TEXT("appRequestExit(%i)"), Force );
	if( Force )
	{
		// Force immediate exit.
		// Dangerous because config code isn't flushed, global destructors aren't called, etc.
		// Suppress abort message and MS reports.
		_set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
		abort();
	}
	else
	{
		// Tell the platform specific code we want to exit cleanly from the main loop.
		PostQuitMessage( 0 );
		GIsRequestingExit = 1;
	}
}

/**
 * Returns the last system error code in string form.  NOTE: Only one return value is valid at a time!
 *
 * @param OutBuffer the buffer to be filled with the error message
 * @param BufferLength the size of the buffer in character count
 * @param Error the error code to convert to string form
 */
const TCHAR* appGetSystemErrorMessage(TCHAR* OutBuffer,INT BufferCount,INT Error)
{
	check(OutBuffer && BufferCount);
	*OutBuffer = TEXT('\0');
	if (Error == 0)
	{
		Error = GetLastError();
	}
	FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, Error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), OutBuffer, BufferCount, NULL );
	TCHAR* Found = appStrchr(OutBuffer,TEXT('\r'));
	if (Found)
	{
		*Found = TEXT('\0');
	}
	Found = appStrchr(OutBuffer,TEXT('\n'));
	if (Found)
	{
		*Found = TEXT('\0');
	}
	return OutBuffer;
}



/** Submits a crash report to a central server (release builds only) */
void appSubmitErrorReport( const TCHAR* InErrorHist, EErrorReportMode::Type InMode )
{
#if !CONSOLE

#if defined(_DEBUG)
	if ( GAlwaysReportCrash )
#else
	if ( !appIsDebuggerPresent() || GAlwaysReportCrash )
#endif
	{
		TCHAR ReportDumpVersion[] = TEXT("3");

		TCHAR ReportDumpPath[ MAX_SPRINTF ];
		{
			const TCHAR ReportDumpFilename[] = TEXT("UE3AutoReportDump");
			appCreateTempFilename( *appGameLogDir(), ReportDumpFilename, TEXT( ".txt" ), ReportDumpPath, MAX_SPRINTF );
		}

		TCHAR IniDumpPath[ MAX_SPRINTF ];
		{
			const TCHAR IniDumpFilename[] = TEXT("UE3AutoReportIniDump");
			appCreateTempFilename( *appGameLogDir(), IniDumpFilename, TEXT( ".txt" ), IniDumpPath, MAX_SPRINTF );
		}

		TCHAR AutoReportExe[] = TEXT("..\\AutoReporter.exe");

		//build the ini dump
		FOutputDeviceFile AutoReportIniFile(IniDumpPath);
		GConfig->Dump(AutoReportIniFile);
		AutoReportIniFile.Flush();
		AutoReportIniFile.TearDown();
		
		FArchive * AutoReportFile = GFileManager->CreateFileWriter(ReportDumpPath, FILEWRITE_EvenIfReadOnly);
		if (AutoReportFile != NULL)
		{
			TCHAR CompName[256];
			appStrcpy(CompName, appComputerName());
			TCHAR UserName[256];
			appStrcpy(UserName, appUserName());
			TCHAR GameName[256];
			appStrcpy(GameName, appGetGameName());
			TCHAR PlatformName[32];
#if _WIN64
			appStrcpy(PlatformName, TEXT("PC 64-bit"));
#else
			appStrcpy(PlatformName, TEXT("PC 32-bit"));
#endif
			TCHAR LangExt[10];
			appStrcpy(LangExt, *appGetLanguageExt());
			TCHAR SystemTime[256];
			appStrcpy(SystemTime, *appSystemTimeString());
			TCHAR EngineVersionStr[32];
			appStrcpy(EngineVersionStr, *appItoa(GEngineVersion));

			TCHAR ChangelistVersionStr[32];
			INT ChangelistFromCommandLine = 0;
			const UBOOL bFoundAutomatedBenchMarkingChangelist = Parse( appCmdLine(), TEXT("-gABC="), ChangelistFromCommandLine );
			if( bFoundAutomatedBenchMarkingChangelist == TRUE )
			{
				appStrcpy(ChangelistVersionStr, *appItoa(ChangelistFromCommandLine));
			}
			// we are not passing in the changelist to use so use the one that was stored in the UnObjVer
			else
			{
				appStrcpy(ChangelistVersionStr, *appItoa(GBuiltFromChangeList));
			}

			TCHAR CmdLine[2048];
			appStrcpy(CmdLine, appCmdLine());
			TCHAR BaseDir[260];
			appStrcpy(BaseDir, appBaseDir());
			TCHAR separator = 0;

			TCHAR EngineMode[64];
			if( GIsUCC )
			{
				appStrcpy(EngineMode, TEXT("Commandlet"));
			}
			else if( GIsEditor )
			{
				appStrcpy(EngineMode, TEXT("Editor"));
			}
			else if( GIsServer && !GIsClient )
			{
				appStrcpy(EngineMode, TEXT("Server"));
			}
			else
			{
				appStrcpy(EngineMode, TEXT("Game"));
			}
			
			//build the report dump file
			AutoReportFile->Serialize(ReportDumpVersion, appStrlen(ReportDumpVersion) * sizeof(TCHAR));
			AutoReportFile->Serialize(&separator, sizeof(TCHAR));
			AutoReportFile->Serialize(CompName, appStrlen(CompName) * sizeof(TCHAR));
			AutoReportFile->Serialize(&separator, sizeof(TCHAR));
			AutoReportFile->Serialize(UserName, appStrlen(UserName) * sizeof(TCHAR));
			AutoReportFile->Serialize(&separator, sizeof(TCHAR));
			AutoReportFile->Serialize(GameName, appStrlen(GameName) * sizeof(TCHAR));
			AutoReportFile->Serialize(&separator, sizeof(TCHAR));
			AutoReportFile->Serialize(PlatformName, appStrlen(PlatformName) * sizeof(TCHAR));
			AutoReportFile->Serialize(&separator, sizeof(TCHAR));
			AutoReportFile->Serialize(LangExt, appStrlen(LangExt) * sizeof(TCHAR));
			AutoReportFile->Serialize(&separator, sizeof(TCHAR));
			AutoReportFile->Serialize(SystemTime, appStrlen(SystemTime) * sizeof(TCHAR));
			AutoReportFile->Serialize(&separator, sizeof(TCHAR));
			AutoReportFile->Serialize(EngineVersionStr, appStrlen(EngineVersionStr) * sizeof(TCHAR));
			AutoReportFile->Serialize(&separator, sizeof(TCHAR));
			AutoReportFile->Serialize(ChangelistVersionStr, appStrlen(ChangelistVersionStr) * sizeof(TCHAR));
			AutoReportFile->Serialize(&separator, sizeof(TCHAR));
			AutoReportFile->Serialize(CmdLine, appStrlen(CmdLine) * sizeof(TCHAR));
			AutoReportFile->Serialize(&separator, sizeof(TCHAR));
			AutoReportFile->Serialize(BaseDir, appStrlen(BaseDir) * sizeof(TCHAR));
			AutoReportFile->Serialize(&separator, sizeof(TCHAR));

			TCHAR* NonConstErrorHist = const_cast< TCHAR* >( InErrorHist );
			AutoReportFile->Serialize(NonConstErrorHist, appStrlen(NonConstErrorHist) * sizeof(TCHAR));

			AutoReportFile->Serialize(&separator, sizeof(TCHAR));
			AutoReportFile->Serialize(EngineMode, appStrlen(EngineMode) * sizeof(TCHAR));
			AutoReportFile->Serialize(&separator, sizeof(TCHAR));
			AutoReportFile->Close();

			//get the paths that the files will actually have been saved to
			FString UserIniDumpPath = GFileManager->ConvertAbsolutePathToUserPath(*GFileManager->ConvertToAbsolutePath(IniDumpPath));
			FString LogDirectory = appGameLogDir();
			TCHAR CommandlineLogFile[MAX_SPRINTF]=TEXT("");

			//use the log file specified on the commandline if there is one
			if (Parse(appCmdLine(), TEXT("LOG="), CommandlineLogFile, ARRAY_COUNT(CommandlineLogFile)))
			{
				LogDirectory += CommandlineLogFile;
			}
			else
			{
				LogDirectory += TEXT("Launch.log");
			}

			FString UserLogFile = GFileManager->ConvertAbsolutePathToUserPath(*GFileManager->ConvertToAbsolutePath(*LogDirectory));
			FString UserReportDumpPath = GFileManager->ConvertAbsolutePathToUserPath(*GFileManager->ConvertToAbsolutePath(ReportDumpPath));

			extern TCHAR MiniDumpFilenameW[1024];
			//start up the auto reporting app, passing the report dump file path, the games' log file, the ini dump path and the minidump path
			//protect against spaces in paths breaking them up on the commandline
			FString CallingCommandLine = FString::Printf(TEXT("\"%s\" \"%s\" \"%s\" \"%s\""), *UserReportDumpPath, *UserLogFile, *UserIniDumpPath, MiniDumpFilenameW);

			switch( InMode )
			{
				case EErrorReportMode::Unattended:
					CallingCommandLine += TEXT( " -unattended" );
					break;

				case EErrorReportMode::Balloon:
					CallingCommandLine += TEXT( " -balloon" );
					break;
			}

			if (appCreateProc(AutoReportExe, *CallingCommandLine) == NULL)
			{
				warnf(TEXT("Couldn't start up the Auto Reporting process!"));
				appMsgf( AMT_OK, TEXT("%s"), InErrorHist );
			}

			// so here we need to see if we are doing AutomatedPerfTesting and we are -unattended
			// if we are then we have crashed in some terrible way and we need to make certain we can 
			// kill -9 the devenv process / vsjitdebugger.exe  and any other processes that are still running
			INT FromCommandLine = 0;
			Parse( appCmdLine(), TEXT("AutomatedPerfTesting="), FromCommandLine );
			if(( GIsUnattended == TRUE ) && ( FromCommandLine != 0 ) && ( ParseParam(appCmdLine(), TEXT("KillAllPopUpBlockingWindows")) == TRUE ))
			{

				warnf(TEXT("Attempting to run KillAllPopUpBlockingWindows"));

				TCHAR KillAllBlockingWindows[] = TEXT("KillAllPopUpBlockingWindows.bat");
				// .bat files never seem to launch correctly with appCreateProc so we just use the appLaunchURL which will call ShellExecute
				// we don't really care about the return code in this case 
				appLaunchURL( TEXT("KillAllPopUpBlockingWindows.bat"), NULL, NULL );

				// check to see if we are doing sentinel
				if( ( FString(appCmdLine()).InStr( TEXT( "DoingASentinelRun=1" ), FALSE, TRUE ) != INDEX_NONE ) 
					|| ( FString(appCmdLine()).InStr( TEXT( "gDASR=1" ), FALSE, TRUE ) != INDEX_NONE ) 
					)
				{
					const FString EndRun = FString::Printf(TEXT("EXEC EndRun @RunID=%i, @ResultDescription='%s'")
						, GSentinelRunID
						, *PerfMemRunResultStrings[ARR_Crashed] 
					);

					//warnf( TEXT("%s"), *EndRun );
					GTaskPerfMemDatabase->SendExecCommand( *EndRun );
				}
			}
		}
	}
#endif
}





/*-----------------------------------------------------------------------------
	Clipboard.
-----------------------------------------------------------------------------*/

// Disabling optimizations helps to reduce the frequency of OpenClipboard failing with error code 0. It still happens
// though only with really large text buffers and we worked around this by changing the editor to use an intermediate
// text buffer for internal operations.
PRAGMA_DISABLE_OPTIMIZATION 

//
// Copy text to clipboard.
//
void appClipboardCopy( const TCHAR* Str )
{
	if( OpenClipboard(GetActiveWindow()) )
	{
		verify(EmptyClipboard());
		HGLOBAL GlobalMem;
		INT StrLen = appStrlen(Str);
		GlobalMem = GlobalAlloc( GMEM_MOVEABLE, sizeof(TCHAR)*(StrLen+1) );
		check(GlobalMem);
		TCHAR* Data = (TCHAR*) GlobalLock( GlobalMem );
		appStrcpy( Data, (StrLen+1), Str );
		GlobalUnlock( GlobalMem );
		if( SetClipboardData( CF_UNICODETEXT, GlobalMem ) == NULL )
			appErrorf(TEXT("SetClipboardData failed with error code %i"), GetLastError() );
		verify(CloseClipboard());
	}
}

//
// Paste text from clipboard.
//
FString appClipboardPaste()
{
	FString Result;
	if( OpenClipboard(GetActiveWindow()) )
	{
		HGLOBAL GlobalMem = NULL;
		UBOOL Unicode = 0;
		GlobalMem = GetClipboardData( CF_UNICODETEXT );
		Unicode = 1;
		if( !GlobalMem )
		{
			GlobalMem = GetClipboardData( CF_TEXT );
			Unicode = 0;
		}
		if( !GlobalMem )
		{
			Result = TEXT("");
		}
		else
		{
			void* Data = GlobalLock( GlobalMem );
			check( Data );	
			if( Unicode )
				Result = (TCHAR*) Data;
			else
			{
				ANSICHAR* ACh = (ANSICHAR*) Data;
				INT i;
				for( i=0; ACh[i]; i++ );
				TArray<TCHAR> Ch(i+1);
				for( i=0; i<Ch.Num(); i++ )
					Ch(i)=FromAnsi(ACh[i]);
				Result = &Ch(0);
			}
			GlobalUnlock( GlobalMem );
		}
		verify(CloseClipboard());
	}
	else 
	{
		Result=TEXT("");
	}
	return Result;
}

PRAGMA_ENABLE_OPTIMIZATION 

/*-----------------------------------------------------------------------------
	DLLs.
-----------------------------------------------------------------------------*/

void* appGetDllHandle( const TCHAR* Filename )
{
	check(Filename);	
	return LoadLibraryW(Filename);
}

//
// Free a DLL.
//
void appFreeDllHandle( void* DllHandle )
{
	check(DllHandle);
	FreeLibrary( (HMODULE)DllHandle );
}

//
// Lookup the address of a DLL function.
//
void* appGetDllExport( void* DllHandle, const TCHAR* ProcName )
{
	check(DllHandle);
	check(ProcName);
	return (void*)GetProcAddress( (HMODULE)DllHandle, TCHAR_TO_ANSI(ProcName) );
}

/*-----------------------------------------------------------------------------
	Formatted printing and messages.
-----------------------------------------------------------------------------*/

/**
* Helper function to write formatted output using an argument list
*
* @param Dest - destination string buffer
* @param DestSize - size of destination buffer
* @param Count - number of characters to write (not including null terminating character)
* @param Fmt - string to print
* @param Args - argument list
* @return number of characters written or -1 if truncated
*/
INT appGetVarArgs( TCHAR* Dest, SIZE_T DestSize, INT Count, const TCHAR*& Fmt, va_list ArgPtr )
{
#if USE_SECURE_CRT
	INT Result = _vsntprintf_s(Dest,DestSize,Count/*_TRUNCATE*/,Fmt,ArgPtr);
#else
	INT Result = _vsntprintf(Dest,Count,Fmt,ArgPtr);
#endif
	va_end( ArgPtr );
	return Result;
}

/**
* Helper function to write formatted output using an argument list
* ASCII version
*
* @param Dest - destination string buffer
* @param DestSize - size of destination buffer
* @param Count - number of characters to write (not including null terminating character)
* @param Fmt - string to print
* @param Args - argument list
* @return number of characters written or -1 if truncated
*/
INT appGetVarArgsAnsi( ANSICHAR* Dest, SIZE_T DestSize, INT Count, const ANSICHAR*& Fmt, va_list ArgPtr )
{
#if USE_SECURE_CRT
	INT Result = _vsnprintf_s(Dest,DestSize,Count/*_TRUNCATE*/,Fmt,ArgPtr);
#else
	INT Result = _vsnprintf(Dest,Count,Fmt,ArgPtr);
#endif
	va_end( ArgPtr );
	return Result;
}

void appDebugMessagef( const TCHAR* Fmt, ... )
{
	TCHAR TempStr[4096]=TEXT("");
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );
	if( GIsUnattended == TRUE )
	{
		debugf(TempStr);
	}
	else
	{
		if (GIsEditor)
		{
			GCallbackQuery->Query( CALLBACK_ModalErrorMessage, TempStr, AMT_OK );
		}
		else
		{
			MessageBox(NULL, TempStr, TEXT("appDebugMessagef"),MB_OK|MB_SYSTEMMODAL);
		}
	}
}

VARARG_BODY( UBOOL, appMsgf, const TCHAR*, VARARG_EXTRA(EAppMsgType Type) )
{
	TCHAR TempStr[16384]=TEXT("");
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );
	if( GIsUnattended == TRUE )
	{
		if (GWarn)
		{
			warnf(TempStr);
		}

		switch(Type)
		{
		case AMT_YesNo:
			return 0; // No
		case AMT_OKCancel:
			return 1; // Cancel
		case AMT_YesNoCancel:
			return 2; // Cancel
		case AMT_CancelRetryContinue:
			return 0; // Cancel
		case AMT_YesNoYesAllNoAll:
			return ART_No; // No
		default:
			return 1;
		}
	}
	else
	{
		if (GIsEditor && !GIsUCC)
		{
			FString ErrorString(TempStr);
			return GCallbackQuery->Query( CALLBACK_ModalErrorMessage, TempStr, Type );
		}
		else
		{
			HWND ParentWindow = GWarn ? (HWND)GWarn->hWndEditorFrame : (HWND)NULL;
			switch( Type )
			{
				case AMT_YesNo:
					return MessageBox( ParentWindow, TempStr, TEXT("Message"), MB_YESNO|MB_SYSTEMMODAL ) == IDYES;
					break;
				case AMT_OKCancel:
					return MessageBox( ParentWindow, TempStr, TEXT("Message"), MB_OKCANCEL|MB_SYSTEMMODAL ) == IDOK;
					break;
				case AMT_YesNoCancel:
					{
						INT Return = MessageBox(ParentWindow, TempStr, TEXT("Message"), MB_YESNOCANCEL | MB_ICONQUESTION | MB_SYSTEMMODAL);
						// return 0 for Yes, 1 for No, 2 for Cancel
						return Return == IDYES ? 0 : (Return == IDNO ? 1 : 2);
					}
				case AMT_CancelRetryContinue:
					{
						INT Return = MessageBox(ParentWindow, TempStr, TEXT("Message"), MB_CANCELTRYCONTINUE | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_SYSTEMMODAL);
						// return 0 for Cancel, 1 for Retry, 2 for Continue
						return Return == IDCANCEL ? 0 : (Return == IDTRYAGAIN ? 1 : 2);
					}
					break;
				case AMT_YesNoYesAllNoAll:
					return MessageBoxExt( AMT_YesNoYesAllNoAll, ParentWindow, TempStr, TEXT("Message") );
					// return 0 for No, 1 for Yes, 2 for YesToAll, 3 for NoToAll
					break;
				case AMT_YesNoYesAllNoAllCancel:
					return MessageBoxExt( AMT_YesNoYesAllNoAllCancel, ParentWindow, TempStr, TEXT("Message") );
					// return 0 for No, 1 for Yes, 2 for YesToAll, 3 for NoToAll, 4 for Cancel
					break;
				default:
					MessageBox( ParentWindow, TempStr, TEXT("Message"), MB_OK|MB_SYSTEMMODAL );
					break;
			}
		}
	}
	return 1;
}

VARARG_BODY( UBOOL, appMsgExf, const TCHAR*, VARARG_EXTRA(EAppMsgType Type) VARARG_EXTRA(UBOOL bDefaultValue) VARARG_EXTRA(UBOOL bSilence))
{
	TCHAR TempStr[16384]=TEXT("");
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );

	if (bSilence)
	{
		// print question and default value
		debugf(TEXT("%s (%d)"), TempStr, bDefaultValue);

		return bDefaultValue;
	}

	return appMsgf(Type, TempStr);
}

void appGetLastError( void )
{
	TCHAR TempStr[MAX_SPRINTF]=TEXT("");
	TCHAR ErrorBuffer[1024];
	appSprintf( TempStr, TEXT("GetLastError : %d\n\n%s"), GetLastError(), appGetSystemErrorMessage(ErrorBuffer,1024) );
	if( GIsUnattended == TRUE )
	{
		appErrorf(TempStr);
	}
	else
	{
		MessageBox( NULL, TempStr, TEXT("System Error"), MB_OK|MB_SYSTEMMODAL );
	}
}

// Interface for recording loading errors in the editor
void EdClearLoadErrors()
{
	GEdLoadErrors.Empty();
}

VARARG_BODY( void VARARGS, EdLoadErrorf, const TCHAR*, VARARG_EXTRA(INT Type) )
{
	TCHAR TempStr[4096]=TEXT("");
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );

	// Check to see if this error already exists ... if so, don't add it.
	// NOTE : for some reason, I can't use AddUniqueItem here or it crashes
	for( INT x = 0 ; x < GEdLoadErrors.Num() ; ++x )
		if( GEdLoadErrors(x) == FEdLoadError( Type, TempStr ) )
			return;

	new( GEdLoadErrors )FEdLoadError( Type, TempStr );
}


/*-----------------------------------------------------------------------------
	Timing.
-----------------------------------------------------------------------------*/

//
// Sleep this thread for Seconds, 0.0 means release the current
// timeslice to let other threads get some attention.
//
void appSleep( FLOAT Seconds )
{
	Sleep( (DWORD)(Seconds * 1000.0) );
}

/**
 * Sleeps forever. This function does not return!
 */
void appSleepInfinite()
{
	Sleep(INFINITE);
}

/*
 *   @return UTC offset from machine local time in minutes
 */
INT appUTCOffset()
{
	struct tm UTCTime, LocalTime;
	time_t now;
	time(&now);
	gmtime_s(&UTCTime, &now);
	UTCTime.tm_isdst = -1;
	localtime_s(&LocalTime, &now);
	LocalTime.tm_isdst = -1;
	time_t UTCTimeT = mktime(&UTCTime);
	time_t LocalTimeT = mktime(&LocalTime);

	// Convert sec to min
	return (INT)((LocalTimeT - UTCTimeT) / 60);
}

/*
 * Convert a UTC timestamp string into seconds
 * @param DateString - a date string formatted same as appTimeStamp 
 * @return number of seconds since epoch
 */
time_t appStrToSeconds(const TCHAR* DateString)
{
	time_t Seconds = 0;
	struct tm TimeStruct;
	appMemzero(&TimeStruct, sizeof(struct tm));

	INT NumParsed = appSSCANF(DateString, TEXT("%d.%d.%d-%d.%d.%d"), 
							  &TimeStruct.tm_year,
							  &TimeStruct.tm_mon,
							  &TimeStruct.tm_mday,
							  &TimeStruct.tm_hour,
							  &TimeStruct.tm_min,
							  &TimeStruct.tm_sec);
	if (NumParsed == 6)
	{
		// Convert proper ranges
		TimeStruct.tm_year -= 1900;
		TimeStruct.tm_mon -= 1;
		TimeStruct.tm_isdst = -1;
		Seconds = mktime(&TimeStruct);
	}

	return Seconds;
}

/**
 * @return the local time values given a time structure.
 */
void appSecondsToLocalTime( time_t Time, INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec )
{
	struct tm TimeStruct;
	localtime_s(&TimeStruct, &Time);

	Year		= TimeStruct.tm_year + 1900;
	Month		= TimeStruct.tm_mon + 1;
	DayOfWeek	= TimeStruct.tm_wday;
	Day			= TimeStruct.tm_mday;
	Hour		= TimeStruct.tm_hour;
	Min			= TimeStruct.tm_min;
	Sec			= TimeStruct.tm_sec;
}

//
// Return the system time.
//
void appSystemTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	SYSTEMTIME st;
	GetLocalTime( &st );

	Year		= st.wYear;
	Month		= st.wMonth;
	DayOfWeek	= st.wDayOfWeek;
	Day			= st.wDay;
	Hour		= st.wHour;
	Min			= st.wMinute;
	Sec			= st.wSecond;
	MSec		= st.wMilliseconds;
}

//
// Return the UTC time.
//
void appUtcTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	SYSTEMTIME st;
	GetSystemTime( &st );

	Year		= st.wYear;
	Month		= st.wMonth;
	DayOfWeek	= st.wDayOfWeek;
	Day			= st.wDay;
	Hour		= st.wHour;
	Min			= st.wMinute;
	Sec			= st.wSecond;
	MSec		= st.wMilliseconds;
}

/*-----------------------------------------------------------------------------
	Link functions.
-----------------------------------------------------------------------------*/

//
// Launch a uniform resource locator (i.e. http://www.epicgames.com/unreal).
// This is expected to return immediately as the URL is launched by another
// task.
//
void appLaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error )
{
	debugf( NAME_Log, TEXT("LaunchURL %s %s"), URL, Parms?Parms:TEXT("") );
	HINSTANCE Code = ShellExecuteW(NULL,TEXT("open"),URL,Parms?Parms:TEXT(""),TEXT(""),SW_SHOWNORMAL);
	if( Error )
	{
		*Error = ( (PTRINT)Code <= 32 ) ? LocalizeError(TEXT("UrlFailed"),TEXT("Core")) : TEXT("");
	}
}

/**
 * Attempt to launch the provided file name in its default external application. Similar to appLaunchURL,
 * with the exception that if a default application isn't found for the file, the user will be prompted with
 * an "Open With..." dialog.
 *
 * @param	FileName	Name of the file to attempt to launch in its default external application
 * @param	Parms		Optional parameters to the default application
 */
void appLaunchFileInDefaultExternalApplication( const TCHAR* FileName, const TCHAR* Parms /*= NULL*/ )
{
	// First attempt to open the file in its default application
	debugf( NAME_Log, TEXT("LaunchFileInExternalEditor %s %s"), FileName, Parms ? Parms : TEXT("") );
	HINSTANCE Code = ShellExecuteW( NULL, TEXT("open"), FileName, Parms ? Parms : TEXT(""), TEXT(""), SW_SHOWNORMAL );
	
	// If opening the file in the default application failed, check to see if it's because the file's extension does not have
	// a default application associated with it. If so, prompt the user with the Windows "Open With..." dialog to allow them to specify
	// an application to use.
	if ( (PTRINT)Code == SE_ERR_NOASSOC || (PTRINT)Code == SE_ERR_ASSOCINCOMPLETE )
	{
		ShellExecuteW( NULL, TEXT("open"), TEXT("RUNDLL32.EXE"), *FString::Printf( TEXT("shell32.dll,OpenAs_RunDLL %s"), FileName ), TEXT(""), SW_SHOWNORMAL );
	}
}

/**
 * Attempt to "explore" the folder specified by the provided file path
 *
 * @param	FilePath	File path specifying a folder to explore
 */
void appExploreFolder( const TCHAR* FilePath )
{
	// Explore the folder
	ShellExecuteW( NULL, TEXT("explore"), FilePath, NULL, NULL, SW_SHOWNORMAL );
}

/**
 * Creates a new process and its primary thread. The new process runs the
 * specified executable file in the security context of the calling process.
 * @param URL					executable name
 * @param Parms					command line arguments
 * @param bLaunchDetached		if TRUE, the new process will have its own window
 * @param bLaunchHidded			if TRUE, the new process will be minimized in the task bar
 * @param bLaunchReallyHidden	if TRUE, the new process will not have a window or be in the task bar
 * @param OutProcessId			if non-NULL, this will be filled in with the ProcessId
 * @param PriorityModifier		-2 idle, -1 low, 0 normal, 1 high, 2 higher
 * @return	The process handle for use in other process functions
 */
void *appCreateProc( const TCHAR* URL, const TCHAR* Parms, UBOOL bLaunchDetached, UBOOL bLaunchHidden, UBOOL bLaunchReallyHidden, DWORD* OutProcessID, INT PriorityModifier )
{
	debugf( NAME_Dev, TEXT("CreateProc %s %s"), URL, Parms );

	FString CommandLine = FString::Printf(TEXT("%s %s"), URL, Parms);

	PROCESS_INFORMATION ProcInfo;
	SECURITY_ATTRIBUTES Attr;
	Attr.nLength = sizeof(SECURITY_ATTRIBUTES);
	Attr.lpSecurityDescriptor = NULL;
	Attr.bInheritHandle = TRUE;

	DWORD CreateFlags = NORMAL_PRIORITY_CLASS;
	if (PriorityModifier < 0)
	{
		if (PriorityModifier == -1)
		{
			CreateFlags = BELOW_NORMAL_PRIORITY_CLASS;
		}
		else
		{
			CreateFlags = IDLE_PRIORITY_CLASS;
		}
	}
	else if (PriorityModifier > 0)
	{
		if (PriorityModifier == 1)
		{
			CreateFlags = ABOVE_NORMAL_PRIORITY_CLASS;
		}
		else
		{
			CreateFlags = HIGH_PRIORITY_CLASS;
		}
	}
	if (bLaunchDetached)
	{
		CreateFlags |= DETACHED_PROCESS;
	}
	DWORD dwFlags = NULL;
	WORD ShowWindowFlags = SW_HIDE;
	if (bLaunchReallyHidden)
	{
		dwFlags = STARTF_USESHOWWINDOW;
	}
	else if (bLaunchHidden)
	{
		dwFlags = STARTF_USESHOWWINDOW;
		ShowWindowFlags = SW_SHOWMINNOACTIVE;
	}
	STARTUPINFO StartupInfo = { sizeof(STARTUPINFO), NULL, NULL, NULL,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, NULL, dwFlags, ShowWindowFlags, NULL, NULL,
		NULL, NULL, NULL };
	if( !CreateProcess( NULL, CommandLine.GetCharArray().GetTypedData(), &Attr, &Attr, TRUE, CreateFlags,
		NULL, NULL, &StartupInfo, &ProcInfo ) )
	{
		if (OutProcessID)
		{
			*OutProcessID = 0;
		}
		return NULL;
	}
	if (OutProcessID)
	{
		*OutProcessID = ProcInfo.dwProcessId;
	}
	return (void*)ProcInfo.hProcess;
}

/** Returns TRUE if the specified process is running 
*
* @param ProcessHandle handle returned from appCreateProc
* @return TRUE if the process is still running
*/
UBOOL appIsProcRunning( void* ProcessHandle )
{
	UBOOL bApplicationRunning = TRUE;
	DWORD WaitResult = WaitForSingleObject((HANDLE)ProcessHandle, 0);
	if (WaitResult != WAIT_TIMEOUT)
	{
		bApplicationRunning = FALSE;
	}
	return bApplicationRunning;
}

/** Waits for a process to stop
*
* @param ProcessHandle handle returned from appCreateProc
*/
void appWaitForProc( void* ProcessHandle )
{
	WaitForSingleObject((HANDLE)ProcessHandle, INFINITE);
}

/** Terminates a process
*
* @param ProcessHandle handle returned from appCreateProc
*/
void appTerminateProc( void* ProcessHandle )
{
	TerminateProcess((HANDLE)ProcessHandle,0);
}

/** Retrieves the ProcessId of this process
*
* @return the ProcessId of this process
*/
DWORD appGetCurrentProcessId()
{
	return GetCurrentProcessId();
}

//
// Retrieves the termination status of the specified process.
//
UBOOL appGetProcReturnCode( void* ProcHandle, INT* ReturnCode )
{
	return GetExitCodeProcess( (HANDLE)ProcHandle, (DWORD*)ReturnCode ) && *((DWORD*)ReturnCode) != STILL_ACTIVE;
}

/** Returns TRUE if the specified application is running */
UBOOL appIsApplicationRunning( DWORD ProcessId )
{
	UBOOL bApplicationRunning = TRUE;
	HANDLE ProcessHandle = OpenProcess(SYNCHRONIZE, false, ProcessId);
	if (ProcessHandle == NULL)
	{
		bApplicationRunning = FALSE;
	}
	else
	{
		DWORD WaitResult = WaitForSingleObject(ProcessHandle, 0);
		if (WaitResult != WAIT_TIMEOUT)
		{
			bApplicationRunning = FALSE;
		}
		CloseHandle(ProcessHandle);
	}
	return bApplicationRunning;
}

/*-----------------------------------------------------------------------------
	File finding.
-----------------------------------------------------------------------------*/

//
// Deletes 1) all temporary files; 2) all cache files that are no longer wanted.
//
void appCleanFileCache()
{
	// do standard cache cleanup
	GSys->PerformPeriodicCacheCleanup();

	// make sure the shaders copied over from user-mode PS3 shader compiling are cleaned up

	// get shader path, and convert it to the userdirectory
	FString ShaderDir = FString(appBaseDir()) * appShaderDir();
	FString UserShaderDir = GFileManager->ConvertAbsolutePathToUserPath(*GFileManager->ConvertToAbsolutePath(*ShaderDir));

	// make sure we don't delete from the source directory
	if (GFileManager->ConvertToAbsolutePath(*ShaderDir) != UserShaderDir)
	{
		// get all the shaders we copied
		TArray<FString> Results;
		appFindFilesInDirectory(Results, *UserShaderDir, FALSE, TRUE);

		// delete each shader
		for (INT ShaderIndex = 0; ShaderIndex < Results.Num(); ShaderIndex++)
		{
			GFileManager->Delete(*Results(ShaderIndex), FALSE, FALSE);
		}
	}

	UBOOL bShouldCleanShaderWorkingDirectory = TRUE;
#if !SHIPPING_PC_GAME
	// Only clean the shader working directory if we are the first instance, to avoid deleting files in use by other instances
	//@todo - check if any other instances are running right now
	bShouldCleanShaderWorkingDirectory = GIsFirstInstance;
#endif

	if (ParseParam(appCmdLine(), TEXT("MTCHILD")))
	{
		// Don't want parallel cooker instances stomping on each other
		bShouldCleanShaderWorkingDirectory = FALSE;
	}

	if (bShouldCleanShaderWorkingDirectory)
	{
		// Path to the working directory where files are written for multi-threaded compilation
		FString ShaderWorkingDirectory = FString( appShaderDir() ) * TEXT("WorkingDirectory") PATH_SEPARATOR;
		// Only clean for processes of this same game
		ShaderWorkingDirectory += appGetGameName();

		TArray<FString> FilesToDelete;
		// Delete all files first, only empty directories can be deleted
		appFindFilesInDirectory(FilesToDelete, *ShaderWorkingDirectory, FALSE, TRUE);
		for (INT FileIndex = 0; FileIndex < FilesToDelete.Num(); FileIndex++)
		{
			// Don't try to delete the placeholder file
			if (FilesToDelete(FileIndex).InStr(TEXT("DO_NOT_DELETE")) == INDEX_NONE)
			{
				GFileManager->Delete(*FilesToDelete(FileIndex), FALSE, FALSE);
			}
		}

		FString Wildcard = ShaderWorkingDirectory * TEXT("*.*");
		TArray<FString> FoldersToDelete; 
		// Find bottom level directories
		GFileManager->FindFiles(FoldersToDelete, *Wildcard, FALSE, TRUE);
		for (INT FolderIndex = 0; FolderIndex < FoldersToDelete.Num(); FolderIndex++)
		{
			TArray<FString> FoldersToDeleteNested; 
			const FString SearchString = ShaderWorkingDirectory * FoldersToDelete(FolderIndex) * TEXT("*.*");
			// Find directories that are nested one level, no further nesting should exist
			GFileManager->FindFiles(FoldersToDeleteNested, *SearchString, FALSE, TRUE);
			// Do a depth-first traversal and delete directories
			for (INT NestedFolderIndex = 0; NestedFolderIndex < FoldersToDeleteNested.Num(); NestedFolderIndex++)
			{
				GFileManager->DeleteDirectory(*(ShaderWorkingDirectory * FoldersToDelete(FolderIndex) * FoldersToDeleteNested(NestedFolderIndex)), FALSE, FALSE);
			}
			GFileManager->DeleteDirectory(*(ShaderWorkingDirectory * FoldersToDelete(FolderIndex)), FALSE, FALSE);
		}
	}
}

/*-----------------------------------------------------------------------------
	Guids.
-----------------------------------------------------------------------------*/

//
// Create a new globally unique identifier.
//
FGuid appCreateGuid()
{
	FGuid Result(0,0,0,0);
	verify( CoCreateGuid( (GUID*)&Result )==S_OK );
	return Result;
}

/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

/** Returns TRUE if the directory exists */
UBOOL appDirectoryExists( const TCHAR* DirectoryName )
{
	FString AbsolutePath = appConvertRelativePathToFull( DirectoryName );
	DWORD FileAttrib = GetFileAttributes( *AbsolutePath );
	return( FileAttrib != INVALID_FILE_ATTRIBUTES );   
}

// Get startup directory.  NOTE: Only one return value is valid at a time!
const TCHAR* appBaseDir()
{
	static TCHAR Result[512]=TEXT("");
	if( !Result[0] )
	{
		// Get directory this executable was launched from.
		GetModuleFileName( hInstance, Result, ARRAY_COUNT(Result) );
		INT StringLength = appStrlen(Result);

		if(StringLength > 0)
		{
			--StringLength;
			for(; StringLength > 0; StringLength-- )
			{
				if( Result[StringLength - 1] == PATH_SEPARATOR[0] || Result[StringLength - 1] == '/' )
				{
					break;
				}
			}
		}
		Result[StringLength] = 0;
	}
	return Result;
}

// Get computer name.  NOTE: Only one return value is valid at a time!
const TCHAR* appComputerName()
{
	static TCHAR Result[256]=TEXT("");
	if( !Result[0] )
	{
		DWORD Size=ARRAY_COUNT(Result);
		GetComputerName( Result, &Size );
	}
	return Result;
}

// Get user name.  NOTE: Only one return value is valid at a time!
const TCHAR* appUserName()
{
	static TCHAR Result[256]=TEXT("");
	if( !Result[0] )
	{
		DWORD Size=ARRAY_COUNT(Result);
		GetUserName( Result, &Size );
		TCHAR *c, *d;
		for( c=Result, d=Result; *c!=0; c++ )
			if( appIsAlnum(*c) )
				*d++ = *c;
		*d++ = 0;
	}
	return Result;
}

// shader dir relative to appBaseDir
static TCHAR ShaderDir[1024] = TEXT("..") PATH_SEPARATOR TEXT("..") PATH_SEPARATOR TEXT("Engine") PATH_SEPARATOR TEXT("Shaders");
const TCHAR* appShaderDir()
{
	return ShaderDir;
}

void appSetShaderDir(const TCHAR*Where)
{
	appStrncpy(ShaderDir,Where,1024);
}

/**
 * Return the name of the currently running executable
 *
 * @return Name of the currently running executable
 */
const TCHAR* appExecutableName()
{
	static TCHAR Result[512]=TEXT("");
	if( !Result[0] )
	{
		// Get complete path for the executable
		if ( GetModuleFileName( hInstance, Result, ARRAY_COUNT(Result) ) != 0 )
		{
			// Remove all of the path information by finding the base filename
			FFilename FileName = Result;
			appStrncpy( Result, *( FileName.GetBaseFilename() ), ARRAY_COUNT(Result) );
		}
		// If the call failed, zero out the memory to be safe
		else
		{
			appMemzero( Result, sizeof( Result ) );
		}
	}
	return Result;
}

/**
 * Retrieve a environment variable from the system
 *
 * @param VariableName The name of the variable (ie "Path")
 * @param Result The string to copy the value of the variable into
 * @param ResultLength The size of the Result string
 */
void appGetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, INT ResultLength)
{
	DWORD Error = GetEnvironmentVariable(VariableName, Result, ResultLength);
	if (Error <= 0)
	{
		// on error, just return an empty string
		*Result = 0;
	}
}

/*-----------------------------------------------------------------------------
	App init/exit.
-----------------------------------------------------------------------------*/

/**
 * Does per platform initialization of timing information and returns the current time. This function is
 * called before the execution of main as GStartTime is statically initialized by it. The function also
 * internally sets GSecondsPerCycle, which is safe to do as static initialization order enforces complex
 * initialization after the initial 0 initialization of the value.
 *
 * @return	current time
 */
DOUBLE appInitTiming(void)
{
	LARGE_INTEGER Frequency;
	verify( QueryPerformanceFrequency(&Frequency) );
	GSecondsPerCycle = 1.0 / Frequency.QuadPart;
	return appSeconds();
}

/**
 * Handles Game Explorer operations (installing/uninstalling for testing, checking parental controls, etc)
 * @param CmdLine Commandline to the app
 *
 * @returns FALSE if the game cannot continue.
 */
UBOOL HandleGameExplorerIntegration(const TCHAR* CmdLine)
{
#if !CONSOLE
	TCHAR AppPath[MAX_PATH];
	GetModuleFileName(NULL, AppPath, MAX_PATH - 1);

	// Initialize COM. We only want to do this once and not override settings of previous calls.
	if( !GIsCOMInitialized )
	{
		CoInitialize( NULL );
		GIsCOMInitialized = TRUE;
	}

	// check to make sure we are able to run, based on parental rights
	IGameExplorer* GameExp;
	HRESULT hr = CoCreateInstance(__uuidof(GameExplorer), NULL, CLSCTX_INPROC_SERVER, __uuidof(IGameExplorer), (void**) &GameExp);

	BOOL bHasAccess = 1;
	BSTR AppPathBSTR = SysAllocString(AppPath);

	// @todo: This will allow access if the CoCreateInstance fails, but it should probaly disallow 
	// access if OS is Vista and it fails, succeed for XP
	if (SUCCEEDED(hr) && GameExp)
	{
		GameExp->VerifyAccess(AppPathBSTR, &bHasAccess);
	}


	// Guid for testing GE (un)installation
	static const GUID GEGuid = 
	{ 0x7089dd1d, 0xfe97, 0x4cc8, { 0x8a, 0xac, 0x26, 0x3e, 0x44, 0x1f, 0x3c, 0x42 } };

	// add the game to the game explorer if desired
	if (ParseParam( CmdLine, TEXT("installge")))
	{
		if (bHasAccess && GameExp)
		{
			BSTR AppDirBSTR = SysAllocString(appBaseDir());
			GUID Guid = GEGuid;
			hr = GameExp->AddGame(AppPathBSTR, AppDirBSTR, ParseParam( CmdLine, TEXT("allusers")) ? GIS_ALL_USERS : GIS_CURRENT_USER, &Guid);

			UBOOL bWasSuccessful = FALSE;
			// if successful
			if (SUCCEEDED(hr))
			{
				// get location of app local dir
				TCHAR UserPath[MAX_PATH];
				SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, UserPath);

				// convert guid to a string
				TCHAR GuidDir[MAX_PATH];
				StringFromGUID2(GEGuid, GuidDir, MAX_PATH - 1);

				// make the base path for all tasks
				FString BaseTaskDirectory = FString(UserPath) + TEXT("\\Microsoft\\Windows\\GameExplorer\\") + GuidDir;

				// make full paths for play and support tasks
				FString PlayTaskDirectory = BaseTaskDirectory + TEXT("\\PlayTasks");
				FString SupportTaskDirectory = BaseTaskDirectory + TEXT("\\SupportTasks");
				
				// make sure they exist
				GFileManager->MakeDirectory(*PlayTaskDirectory, TRUE);
				GFileManager->MakeDirectory(*SupportTaskDirectory, TRUE);

				// interface for creating a shortcut
				IShellLink* Link;
				hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,	IID_IShellLink, (void**)&Link);

				// get the persistent file interface of the link
				IPersistFile* LinkFile;
				Link->QueryInterface(IID_IPersistFile, (void**)&LinkFile);

				Link->SetPath(AppPath);

				// create all of our tasks

				// first is just the game
				Link->SetArguments(TEXT(""));
				Link->SetDescription(TEXT("Play"));
				GFileManager->MakeDirectory(*(PlayTaskDirectory + TEXT("\\0")), TRUE);
				LinkFile->Save(*(PlayTaskDirectory + TEXT("\\0\\Play.lnk")), TRUE);

				Link->SetArguments(TEXT("editor"));
				Link->SetDescription(TEXT("Editor"));
				GFileManager->MakeDirectory(*(PlayTaskDirectory + TEXT("\\1")), TRUE);
				LinkFile->Save(*(PlayTaskDirectory + TEXT("\\1\\Editor.lnk")), TRUE);

				LinkFile->Release();
				Link->Release();

				IUniformResourceLocator* InternetLink;
				CoCreateInstance (CLSID_InternetShortcut, NULL, 
					CLSCTX_INPROC_SERVER, IID_IUniformResourceLocator, (LPVOID*) &InternetLink);

				InternetLink->QueryInterface(IID_IPersistFile, (void**)&LinkFile);

				// make an internet shortcut
				InternetLink->SetURL(TEXT("http://www.unrealtournament3.com/"), 0);
				GFileManager->MakeDirectory(*(SupportTaskDirectory + TEXT("\\0")), TRUE);
				LinkFile->Save(*(SupportTaskDirectory + TEXT("\\0\\UT3.url")), TRUE);

				LinkFile->Release();
				InternetLink->Release();
			}
			appMsgf(AMT_OK, TEXT("GameExplorer installation was %s, quitting now."), SUCCEEDED(hr) ? TEXT("successful") : TEXT("a failure"));

			SysFreeString(AppDirBSTR);
		}
		else
		{
			appMsgf(AMT_OK, TEXT("GameExplorer installation failed because you don't have access (check parental control levels and that you are running XP). You should not need Admin access"));
		}

		// free the string and shutdown COM
		SysFreeString(AppPathBSTR);
		SAFE_RELEASE(GameExp);
		CoUninitialize();
		GIsCOMInitialized = FALSE;

		return FALSE;
	}
	else if (ParseParam( CmdLine, TEXT("uninstallge")))
	{
		if (GameExp)
		{
			hr = GameExp->RemoveGame(GEGuid);
			appMsgf(AMT_OK, TEXT("GameExplorer uninstallation was %s, quitting now."), SUCCEEDED(hr) ? TEXT("successful") : TEXT("a failure"));
		}
		else
		{
			appMsgf(AMT_OK, TEXT("GameExplorer uninstallation failed because you are probably not running Vista."));
		}

		// free the string and shutdown COM
		SysFreeString(AppPathBSTR);
		SAFE_RELEASE(GameExp);
		CoUninitialize();
		GIsCOMInitialized = FALSE;

		return FALSE;
	}

	// free the string and shutdown COM
	SysFreeString(AppPathBSTR);
	SAFE_RELEASE(GameExp);
	CoUninitialize();
	GIsCOMInitialized = FALSE;

	// if we don't have access, we must quit ASAP after showing a message
	if (!bHasAccess)
	{
		appMsgf(AMT_OK, *Localize(TEXT("Errors"), TEXT("Error_ParentalControls"), TEXT("Launch")));
		return FALSE;
	}
#endif
	return TRUE;
}

#if WITH_FIREWALL_SUPPORT
/** 
 * Get the INetFwProfile interface for current profile
 */
INetFwProfile* GetFirewallProfile( void )
{
	HRESULT hr;
	INetFwMgr* pFwMgr = NULL;
	INetFwPolicy* pFwPolicy = NULL;
	INetFwProfile* pFwProfile = NULL;

	// Create an instance of the Firewall settings manager
	hr = CoCreateInstance( __uuidof( NetFwMgr ), NULL, CLSCTX_INPROC_SERVER, __uuidof( INetFwMgr ), ( void** )&pFwMgr );
	if( SUCCEEDED( hr ) )
	{
		hr = pFwMgr->get_LocalPolicy( &pFwPolicy );
		if( SUCCEEDED( hr ) )
		{
			pFwPolicy->get_CurrentProfile( &pFwProfile );
		}
	}

	// Cleanup
	if( pFwPolicy )
	{
		pFwPolicy->Release();
	}
	if( pFwMgr )
	{
		pFwMgr->Release();
	}

	return( pFwProfile );
}

/**
 * Handles firewall operations
 * @param CmdLine Commandline to the app
 */
void HandleFirewallIntegration( const TCHAR* CmdLine, const TCHAR* AppPath, const TCHAR* AppFriendlyName )
{
	BSTR bstrGameExeFullPath = SysAllocString( AppPath );
	BSTR bstrFriendlyAppName = SysAllocString( AppFriendlyName );

	if( bstrGameExeFullPath && bstrFriendlyAppName )
	{
		HRESULT hr = S_OK;

		// Initialize COM. We only want to do this once and not override settings of previous calls.
		if( !GIsCOMInitialized )
		{
			hr = CoInitialize( NULL );
			GIsCOMInitialized = TRUE;
		}

		if( SUCCEEDED( hr ) )
		{
			INetFwProfile* pFwProfile = GetFirewallProfile();
			if( pFwProfile )
			{
				INetFwAuthorizedApplications* pFwApps = NULL;

				hr = pFwProfile->get_AuthorizedApplications( &pFwApps );
				if( SUCCEEDED( hr ) && pFwApps ) 
				{
					// Add/Enable firewall exception
					if( ParseParam( CmdLine, TEXT( "installfw" ) ) )
					{
						INetFwAuthorizedApplication* pFwApp = NULL;
						
						// Check to see if the app is in the exception list
						hr = pFwApps->Item(bstrGameExeFullPath, &pFwApp);
						if(SUCCEEDED(hr) && pFwApp)
						{
							// We have an existing app exception, make sure the exception is enabled
							pFwApp->put_Enabled(VARIANT_TRUE);
							pFwApp->Release();
						}
						else
						{
							// Create an instance of an authorized application.
							hr = CoCreateInstance( __uuidof( NetFwAuthorizedApplication ), NULL, CLSCTX_INPROC_SERVER, __uuidof( INetFwAuthorizedApplication ), ( void** )&pFwApp );
							if( SUCCEEDED( hr ) && pFwApp )
							{
								// Set the process image file name.
								hr = pFwApp->put_ProcessImageFileName( bstrGameExeFullPath );
								if( SUCCEEDED( hr ) )
								{
									// Set the application friendly name.
									hr = pFwApp->put_Name( bstrFriendlyAppName );
									if( SUCCEEDED( hr ) )
									{
										// Add the application to the collection.
										hr = pFwApps->Add( pFwApp );
									}
								}

								pFwApp->Release();

							}
						}

					}
					else if( ParseParam( CmdLine, TEXT( "uninstallfw" ) ) )
					{
						// Remove the application from the collection.
						hr = pFwApps->Remove( bstrGameExeFullPath );
					}

					pFwApps->Release();
				}

				pFwProfile->Release();
			}

			CoUninitialize();
			GIsCOMInitialized = FALSE;
		}

		SysFreeString( bstrFriendlyAppName );
		SysFreeString( bstrGameExeFullPath );
	}
}
#endif // WITH_FIREWALL_SUPPORT

// fwd decl
void InitSHAHashes();

/**
* Called during appInit() after cmd line setup
*/
void appPlatformPreInit()
{
	// Check Windows version.
	OSVERSIONINFOEX OsVersionInfo = { 0 };
	OsVersionInfo.dwOSVersionInfoSize = sizeof( OSVERSIONINFOEX );
	GetVersionEx( ( LPOSVERSIONINFO )&OsVersionInfo );

	// Get the total screen size of the primary monitor.
	GPrimaryMonitorWidth = ::GetSystemMetrics( SM_CXSCREEN );
	GPrimaryMonitorHeight = ::GetSystemMetrics( SM_CYSCREEN );
	GVirtualScreenRect.left = ::GetSystemMetrics( SM_XVIRTUALSCREEN );
	GVirtualScreenRect.top = ::GetSystemMetrics( SM_YVIRTUALSCREEN );
	GVirtualScreenRect.right = ::GetSystemMetrics( SM_CXVIRTUALSCREEN );
	GVirtualScreenRect.bottom = ::GetSystemMetrics( SM_CYVIRTUALSCREEN );

	// Get the screen rect of the primary monitor, exclusing taskbar etc.
	SystemParametersInfo( SPI_GETWORKAREA, 0, &GPrimaryMonitorWorkRect, 0 );

	// initialize the file SHA hash mapping
	InitSHAHashes();
}

/**
 * Temporary window procedure for the game window during startup.
 * It gets replaced later on with SetWindowLong().
 */
LRESULT CALLBACK StartupWindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
		// Prevent power management
		case WM_POWERBROADCAST:
		{
			switch( wParam )
			{
				case PBT_APMQUERYSUSPEND:
				case PBT_APMQUERYSTANDBY:
					return BROADCAST_QUERY_DENY;
			}
		}
	}

	return DefWindowProc(hWnd, Message, wParam, lParam);
}

void appPlatformInit()
{
	// Randomize.
    if( GIsBenchmarking )
	{
		srand( 0 );
	}
    else
	{
		srand( (unsigned)time( NULL ) );
	}

	// Set granularity of sleep and such to 1 ms.
	timeBeginPeriod( 1 );

	// Identity.
	debugf( NAME_Init, TEXT("Computer: %s"), appComputerName() );
	debugf( NAME_Init, TEXT("User: %s"), appUserName() );

	// Get CPU info.
	SYSTEM_INFO SI;
	GetSystemInfo(&SI);
	debugf( NAME_Init, TEXT("CPU Page size=%i, Processors=%i"), SI.dwPageSize, SI.dwNumberOfProcessors );
	GNumHardwareThreads = SI.dwNumberOfProcessors;

	// Timer resolution.
	debugf( NAME_Init, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / GSecondsPerCycle );

	// Get memory.
	MEMORYSTATUSEX M;
	M.dwLength = sizeof(MEMORYSTATUSEX);
	GlobalMemoryStatusEx(&M);
	GPhysicalGBRam = UINT(FLOAT(M.ullTotalPhys/1024.0/1024.0/1024.0)+ .1f);
	debugf( NAME_Init, TEXT("Memory total: Physical=%.1fGB (%dGB approx) Pagefile=%.1fGB Virtual=%.1fGB"), FLOAT(M.ullTotalPhys/1024.0/1024.0/1024.0),GPhysicalGBRam, FLOAT(M.ullTotalPageFile/1024.0/1024.0/1024.0), FLOAT(M.ullTotalVirtual/1024.0/1024.0/1024.0) );
#if STATS
	// set our max physical memory available
	DWORD MaxPhysicalMemory = (DWORD) Min<DWORDLONG>( M.ullTotalPhys, DWORDLONG(MAXDWORD) );
	GStatManager.SetAvailableMemory(MCR_Physical, MaxPhysicalMemory);
#endif

	// Whether we should be calling TestCooperativeLevel every drawcall
	GConfig->GetBool( TEXT("WinDrv.WindowsClient"), TEXT("ParanoidDeviceLostChecking"), GParanoidDeviceLostChecking, GEngineIni );
}


void appPlatformPostInit()
{
	if ( GIsGame )
	{
		// Register the window class
		FString WindowClassName = FString(GPackage) + TEXT("Unreal") + TEXT("UWindowsClient");
		WNDCLASSEXW Cls;
		appMemzero( &Cls, sizeof(Cls) );
		Cls.cbSize			= sizeof(Cls);
		// disable dbl-click messages in the game as the dbl-click event is sent instead of the key pressed event, which causes issues with e.g. rapid firing
		Cls.style			= GIsGame ? (CS_OWNDC) : (CS_DBLCLKS|CS_OWNDC);
		Cls.lpfnWndProc		= StartupWindowProc;
		Cls.hInstance		= hInstance;
		LPTSTR IntResource = MAKEINTRESOURCEW(GGameIcon);
		Cls.hIcon			= LoadIcon(hInstance,MAKEINTRESOURCEW(GGameIcon));
		if (Cls.hIcon == 0)
		{
			DWORD Error = GetLastError();
			Cls.hIcon = LoadIcon(hInstance, TEXT("UDK.ico"));
		}
		Cls.lpszClassName	= *WindowClassName;
		Cls.hIconSm			= LoadIcon(hInstance,MAKEINTRESOURCEW(GGameIcon));
		verify(RegisterClassExW( &Cls ));
		GGameWindowUsingStartupWindowProc = TRUE;

#if _WINDLL
		// Don't create a Window if an external program (e.g. browser) has already supplied one
		if( !GPIBParentWindow )
#endif // _WINDLL
		{
			// Create a minimized game window
			DWORD WindowStyle, WindowStyleEx;
			WindowStyle = WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_BORDER | WS_CAPTION;
			WindowStyleEx = WS_EX_APPWINDOW;

			// Obtain width and height of primary monitor.
			INT ScreenWidth  = GPrimaryMonitorWidth;
			INT ScreenHeight = GPrimaryMonitorHeight;
			INT WindowWidth  = ScreenWidth / 2;
			INT WindowHeight = ScreenHeight / 2;
			INT WindowPosX = (ScreenWidth - WindowWidth ) / 2;
			INT WindowPosY = (ScreenHeight - WindowHeight ) / 2;
#if _WIN64
			const FString PlatformBitsString( TEXT( "64" ) );
#else
			const FString PlatformBitsString( TEXT( "32" ) );
#endif
			const FString GameName = GConfig->GetStr(TEXT("URL"), TEXT("GameName"), GEngineIni);
			const FString RHIName = ShaderPlatformToText( GRHIShaderPlatform, TRUE, TRUE );
			const FString Name = FString::Printf( TEXT( "%s (%s-bit, %s)" ), *GameName, *PlatformBitsString, *RHIName );

			// Create the window
			GGameWindow = CreateWindowEx( WindowStyleEx, *WindowClassName, *Name, WindowStyle, WindowPosX, WindowPosY, WindowWidth, WindowHeight, NULL, NULL, hInstance, NULL );
			verify( GGameWindow );
			ShowWindow( GGameWindow, SW_SHOWMINIMIZED );
		}
	}
}

/**
 *	Pumps Windows messages.
 */
void appWinPumpMessages()
{
#if _WINDLL
	// The external program owns and handles the message pump
	if( !GPIBParentWindow )
#endif // _WINDLL
	{
		MSG Msg;
		while( PeekMessage(&Msg,NULL,0,0,PM_REMOVE) )
		{
#if (WITH_IME && WITH_GFx && WITH_GFx_IME)
			// This is necessary to hide the composition string pop up window on windows Vista. 
			HWND hWndIME = ImmGetDefaultIMEWnd(Msg.hwnd);
			ShowOwnedPopups(hWndIME, false);

			// Preprocess some keyboard messages before TranslateMessage
			if (GEngine && GEngine->GameViewport && 
				(Msg.message == WM_KEYDOWN || Msg.message == WM_KEYUP || Msg.message == WM_LBUTTONDOWN || Msg.message == WM_LBUTTONUP ||
				 ImmIsUIMessage(NULL, Msg.message, Msg.wParam, Msg.lParam)))
			{
				UGameViewportClient* GameViewportClient = Cast<UGameViewportClient>(GEngine->GameViewport->GetUObject());
				if (GameViewportClient && GameViewportClient->ScaleformInteraction)
				{
					UGFxMoviePlayer* Movie = GameViewportClient->ScaleformInteraction->GetFocusMovie(0);
					if (Movie && Movie->pMovie && Movie->pMovie->pView)
					{
						GFx::IMEWin32Event ev(GFx::IMEWin32Event::IME_PreProcessKeyboard,
							(UPInt)Msg.hwnd, Msg.message, Msg.wParam, Msg.lParam, false);
						Movie->pMovie->pView->HandleEvent(ev);
					}
				}
			}
#endif

#if WITH_PANORAMA
			extern UBOOL appPanoramaInputTranslateMessage(LPMSG);
			// If not handled by Live, handle it ourselves
			if (appPanoramaInputTranslateMessage(&Msg))
			{
				TranslateMessage( &Msg );
				DispatchMessage( &Msg );
			}
#else
			TranslateMessage( &Msg );
			DispatchMessage( &Msg );
#endif
		}
	}
}

/**
 *	Processes sent window messages only.
 */
void appWinPumpSentMessages()
{
	MSG Msg;
	PeekMessage(&Msg,NULL,0,0,PM_NOREMOVE | PM_QS_SENDMESSAGE);
}

/*
 *	Shows the intial game window in the proper position and size.
 *	It also changes the window proc from StartupWindowProc to
 *	UWindowsClient::StaticWndProc.
 *	This function doesn't have any effect if called a second time.
 */
void appShowGameWindow()
{
#if !CONSOLE
#ifdef _WINDLL
	// Set the message processor for our child window to handle all the incoming messages
	if( GPIBParentWindow )
	{
		SetWindowLongPtr( GPIBChildWindow, GWLP_WNDPROC, ( LONG_PTR )UWindowsClient::StaticWndProc );
		GGameWindowUsingStartupWindowProc = FALSE;
	}
	else 
#endif // _WINDLL
	if ( GGameWindow && GGameWindowUsingStartupWindowProc )
	{
		extern DWORD GGameWindowStyle;
		extern INT GGameWindowPosX;
		extern INT GGameWindowPosY;
		extern INT GGameWindowWidth;
		extern INT GGameWindowHeight;

		// Convert position from screen coordinates to workspace coordinates.
		HMONITOR Monitor = MonitorFromWindow(GGameWindow, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO MonitorInfo;
        MonitorInfo.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo( Monitor, &MonitorInfo );
        INT PosX = GGameWindowPosX - MonitorInfo.rcWork.left;
        INT PosY = GGameWindowPosY - MonitorInfo.rcWork.top;

		// Clear out old messages using the old StartupWindowProc
		appWinPumpMessages();

		SetWindowLong(GGameWindow, GWL_STYLE, GGameWindowStyle);
		SetWindowLongPtr(GGameWindow, GWLP_WNDPROC, (LONG_PTR)UWindowsClient::StaticWndProc);
		GGameWindowUsingStartupWindowProc = FALSE;

		// Restore the minimized window to the correct position, size and styles.
		WINDOWPLACEMENT Placement;
		appMemzero(&Placement, sizeof(WINDOWPLACEMENT));
		Placement.length					= sizeof(WINDOWPLACEMENT);
		GetWindowPlacement(GGameWindow, &Placement);
		Placement.flags						= 0;
		Placement.showCmd					= SW_SHOWNORMAL;	// Restores the minimized window (SW_SHOW won't do that)
		Placement.rcNormalPosition.left		= PosX;
		Placement.rcNormalPosition.right	= PosX + GGameWindowWidth;
		Placement.rcNormalPosition.top		= PosY;
		Placement.rcNormalPosition.bottom	= PosY + GGameWindowHeight;
		SetWindowPlacement(GGameWindow, &Placement);
		UpdateWindow(GGameWindow);

		// Pump the messages using the new (correct) WindowProc
		appWinPumpMessages();
	}
#endif
}

/** Returns the language setting that is configured for the platform */
FString appGetLanguageExt(void)
{
	static FString LangExt = TEXT("");
	if (LangExt.Len())
	{
		return LangExt;
	}

	// Retrieve the setting from the ini file
	FString Temp;
	GConfig->GetString( TEXT("Engine.Engine"), TEXT("Language"), Temp, GEngineIni );
	if (Temp.Len() == 0)
	{
		Temp = TEXT("INT");
	}
	LangExt = Temp;

	// Allow for overriding the language settings via the commandline
	FString CmdLineLang;
	if (Parse(appCmdLine(), TEXT("LANGUAGE="), CmdLineLang))
	{
		warnf(NAME_Log, TEXT("Overriding lang %s w/ command-line option of %s"), *LangExt, *CmdLineLang);
		LangExt = CmdLineLang;
	}
	LangExt = LangExt.ToUpper();

	// make sure the language is one that is known (GKnownLanguages)
	if (appIsKnownLanguageExt(LangExt) == FALSE)
	{
		// default back to INT if the language wasn't known
		warnf(NAME_Warning, TEXT("Unknown language extension %s. Defaulting to INT"), *LangExt);
		LangExt = TEXT("INT");
	}
	return LangExt;
}


/*-----------------------------------------------------------------------------
	SHA-1 functions.
-----------------------------------------------------------------------------*/

/**
* Get the hash values out of the executable hash section
*
* NOTE: hash keys are stored in the executable, you will need to put a line like the
*		 following into your PCLaunch.rc settings:
*			ID_HASHFILE RCDATA "..\\..\\..\\..\\GameName\\Build\\Hashes.sha"
*
*		 Then, use the -sha option to the cooker (must be from commandline, not
*       frontend) to generate the hashes for .ini, loc, startup packages, and .usf shader files
*
*		 You probably will want to make and checkin an empty file called Hashses.sha
*		 into your source control to avoid linker warnings. Then for testing or final
*		 final build ONLY, use the -sha command and relink your executable to put the
*		 hashes for the current files into the executable.
*/
void InitSHAHashes()
{
	DWORD SectionSize = 0;
	void* SectionData = NULL;
	// find the resource for the file hash in the exe by ID
	HRSRC HashFileFindResH = FindResource(NULL,MAKEINTRESOURCE(ID_HASHFILE),RT_RCDATA);
	if( HashFileFindResH )
	{
		// load it
		HGLOBAL HashFileLoadResH = LoadResource(NULL,HashFileFindResH);
		if( !HashFileLoadResH )
		{
			appGetLastError();
		}
		else
		{
			// get size
			SectionSize = SizeofResource(NULL,HashFileFindResH);
			// get the data. no need to unlock it
			SectionData = (BYTE*)LockResource(HashFileLoadResH);
		}
	}

	// there may be a dummy byte for platforms that can't handle empty files for linking
	if (SectionSize <= 1)
	{
		return;
	}

	// look for the hash section
	if( SectionData )
	{
		FSHA1::InitializeFileHashesFromBuffer((BYTE*)SectionData, SectionSize);
	}
}


/**
 * Callback that is called if the asynchronous SHA verification fails
 * This will be called from a pooled thread.
 *
 * @param FailedPathname Pathname of file that failed to verify
 * @param bFailedDueToMissingHash TRUE if the reason for the failure was that the hash was missing, and that was set as being an error condition
 */

/* *** NEVER CHECK THE BELOW IN SET TO 1!!! *** */
#define DISABLE_AUTHENTICATION_FOR_DEV 0
/* *** NEVER CHECK THE ABOVE IN SET TO 1!!! *** */

void appOnFailSHAVerification(const TCHAR* FailedPathname, UBOOL bFailedDueToMissingHash)
{
#if !DISABLE_AUTHENTICATION_FOR_DEV
	appErrorf(TEXT("SHA Verification failed for '%s'. Reason: %s"), 
		FailedPathname ? FailedPathname : TEXT("Unknown file"),
		bFailedDueToMissingHash ? TEXT("Missing hash") : TEXT("Bad hash"));
#else
	debugfSuppressed(NAME_DevSHA, TEXT("SHA Verification failed for '%s'. Reason: %s"), 
		FailedPathname ? FailedPathname : TEXT("Unknown file"),
		bFailedDueToMissingHash ? TEXT("Missing hash") : TEXT("Bad hash"));
#endif
}

/*----------------------------------------------------------------------------
	Misc functions.
----------------------------------------------------------------------------*/

/**
 * Helper global variables, used in MessageBoxDlgProc for set message text.
 */
static TCHAR* GMessageBoxText = NULL;
static TCHAR* GMessageBoxCaption = NULL;
/**
 * Used by MessageBoxDlgProc to indicate whether a 'Cancel' button is present and
 * thus 'Esc should be accepted as a hotkey.
 */
static UBOOL GCancelButtonEnabled = FALSE;

/**
 * Displays extended message box allowing for YesAll/NoAll
 * @return 3 - YesAll, 4 - NoAll, -1 for Fail
 */
int MessageBoxExt( EAppMsgType MsgType, HWND HandleWnd, const TCHAR* Text, const TCHAR* Caption )
{
	GMessageBoxText = (TCHAR *) Text;
	GMessageBoxCaption = (TCHAR *) Caption;

	if( MsgType == AMT_YesNoYesAllNoAll )
	{
		GCancelButtonEnabled = FALSE;
		return DialogBox( GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_YESNO2ALL), HandleWnd, MessageBoxDlgProc );
	}
	else if( MsgType == AMT_YesNoYesAllNoAllCancel )
	{
		GCancelButtonEnabled = TRUE;
		return DialogBox( GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_YESNO2ALLCANCEL), HandleWnd, MessageBoxDlgProc );
	}

	return -1;
}


/**
 * Calculates button position and size, localize button text.
 * @param HandleWnd handle to dialog window
 * @param Text button text to localize
 * @param DlgItemId dialog item id
 * @param PositionX current button position (x coord)
 * @param PositionY current button position (y coord)
 * @return TRUE if succeeded
 */
static UBOOL SetDlgItem( HWND HandleWnd, const TCHAR* Text, INT DlgItemId, INT* PositionX, INT* PositionY )
{
	SIZE SizeButton;
		
	HDC DC = CreateCompatibleDC( NULL );
	GetTextExtentPoint32( DC, Text, wcslen(Text), &SizeButton );
	DeleteDC(DC);
	DC = NULL;

	SizeButton.cx += 14;
	SizeButton.cy += 8;

	HWND Handle = GetDlgItem( HandleWnd, DlgItemId );
	if( Handle )
	{
		*PositionX -= ( SizeButton.cx + 5 );
		SetWindowPos( Handle, HWND_TOP, *PositionX, *PositionY - SizeButton.cy, SizeButton.cx, SizeButton.cy, 0 );
		SetDlgItemText( HandleWnd, DlgItemId, Text );
		
		return TRUE;
	}

	return FALSE;
}

/**
 * Callback for MessageBoxExt dialog (allowing for Yes to all / No to all )
 * @return		One of ART_Yes, ART_Yesall, ART_No, ART_NoAll, ART_Cancel.
 */
PTRINT CALLBACK MessageBoxDlgProc( HWND HandleWnd, UINT Message, WPARAM WParam, LPARAM LParam )
{
    switch(Message)
    {
        case WM_INITDIALOG:
			{
				// Sets most bottom and most right position to begin button placement
				RECT Rect;
				POINT Point;
				
				GetWindowRect( HandleWnd, &Rect );
				Point.x = Rect.right;
				Point.y = Rect.bottom;
				ScreenToClient( HandleWnd, &Point );
				
				INT PositionX = Point.x - 8;
				INT PositionY = Point.y - 10;

				// Localize dialog buttons, sets position and size.
				FString CancelString;
				FString NoToAllString;
				FString NoString;
				FString YesToAllString;
				FString YesString;

				// The Localize* functions will return the Key if a dialog is presented before the config system is initialized.
				// Instead, we use hard-coded strings if config is not yet initialized.
				if( !GIsStarted || !GConfig || !GSys )
				{
					CancelString = TEXT("Cancel");
					NoToAllString = TEXT("No to All");
					NoString = TEXT("No");
					YesToAllString = TEXT("Yes to All");
					YesString = TEXT("Yes");
				}
				else
				{
					CancelString = LocalizeUnrealEd("Cancel");
					NoToAllString = LocalizeUnrealEd("NoToAll");
					NoString = LocalizeUnrealEd("No");
					YesToAllString = LocalizeUnrealEd("YesToAll");
					YesString = LocalizeUnrealEd("Yes");
				}
				SetDlgItem( HandleWnd, *CancelString, IDC_CANCEL, &PositionX, &PositionY );
				SetDlgItem( HandleWnd, *NoToAllString, IDC_NOTOALL, &PositionX, &PositionY );
				SetDlgItem( HandleWnd, *NoString, IDC_NO_B, &PositionX, &PositionY );
				SetDlgItem( HandleWnd, *YesToAllString, IDC_YESTOALL, &PositionX, &PositionY );
				SetDlgItem( HandleWnd, *YesString, IDC_YES, &PositionX, &PositionY );

				SetDlgItemText( HandleWnd, IDC_MESSAGE, GMessageBoxText );
				SetWindowText( HandleWnd, GMessageBoxCaption );

				// If parent window exist, get it handle and make it foreground.
				HWND ParentWindow = GetTopWindow( HandleWnd );
				if( ParentWindow )
				{
					SetWindowPos( ParentWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
				}

				SetForegroundWindow( HandleWnd );
				SetWindowPos( HandleWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );

				RegisterHotKey( HandleWnd, HOTKEY_YES, 0, 'Y' );
				RegisterHotKey( HandleWnd, HOTKEY_NO, 0, 'N' );
				if ( GCancelButtonEnabled )
				{
					RegisterHotKey( HandleWnd, HOTKEY_CANCEL, 0, VK_ESCAPE );
				}

				// Windows are foreground, make them not top most.
				SetWindowPos( HandleWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
				if( ParentWindow )
				{
					SetWindowPos( ParentWindow, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
				}

				return TRUE;
			}
		case WM_DESTROY:
			{
				UnregisterHotKey( HandleWnd, HOTKEY_YES );
				UnregisterHotKey( HandleWnd, HOTKEY_NO );
				if ( GCancelButtonEnabled )
				{
					UnregisterHotKey( HandleWnd, HOTKEY_CANCEL );
				}
				return TRUE;
			}
		case WM_COMMAND:
            switch( LOWORD( WParam ) )
            {
                case IDC_YES:
                    EndDialog( HandleWnd, ART_Yes );
					break;
                case IDC_YESTOALL:
                    EndDialog( HandleWnd, ART_YesAll );
					break;
				case IDC_NO_B:
                    EndDialog( HandleWnd, ART_No );
					break;
				case IDC_NOTOALL:
                    EndDialog( HandleWnd, ART_NoAll );
					break;
				case IDC_CANCEL:
					if ( GCancelButtonEnabled )
					{
						EndDialog( HandleWnd, ART_Cancel );
					}
					break;
            }
			break;
		case WM_HOTKEY:
			switch( WParam )
			{
			case HOTKEY_YES:
				EndDialog( HandleWnd, ART_Yes );
				break;
			case HOTKEY_NO:
				EndDialog( HandleWnd, ART_No );
				break;
			case HOTKEY_CANCEL:
				if ( GCancelButtonEnabled )
				{
					EndDialog( HandleWnd, ART_Cancel );
				}
				break;
			}
			break;
        default:
            return FALSE;
    }
    return TRUE;
}


/** 
 * This will update the passed in FMemoryChartEntry with the platform specific data
 *
 * @param FMemoryChartEntry the struct to fill in
 **/
void appUpdateMemoryChartStats( struct FMemoryChartEntry& MemoryEntry )
{
#if DO_CHARTING
	//@todo fill in these values

	// set the memory chart data
	MemoryEntry.GPUMemUsed = 0;

	MemoryEntry.NumAllocations = 0;
	MemoryEntry.AllocationOverhead = 0;
	MemoryEntry.AllignmentWaste = 0;
#endif // DO_CHARTING
}

#if !CONSOLE

/**
 * Get a compatibility level for the app
 */
FCompatibilityLevelInfo appGetCompatibilityLevel()
{
#if DEDICATED_SERVER
	static FCompatibilityLevelInfo DummyStruct(0,0,0);
	return DummyStruct;
#else
	// extern the function in from D3D9Drv. Shady, but better than calling a D3D9Drv function directly from where we shouldn't.
	extern FCompatibilityLevelInfo GetCompatibilityLevelWindows();

	return GetCompatibilityLevelWindows();
#endif
}

/**
 * Set compatibility level for the app.
 * 
 * @param	Level		compatibility level to set
 * @praram  bWriteToIni whether to write the new settings to the ini file
 * 
 * @return	UBOOL		TRUE if setting was successful
 */
UBOOL appSetCompatibilityLevel(FCompatibilityLevelInfo Level, UBOOL bWriteToIni)
{
#if DEDICATED_SERVER
	return TRUE;
#else
	// extern the function in from D3D9Drv. Shady, but better than calling a D3D9Drv function directly from where we shouldn't.
	extern UBOOL SetCompatibilityLevelWindows(FCompatibilityLevelInfo, UBOOL);

	return SetCompatibilityLevelWindows(Level, bWriteToIni);
#endif
}

#endif // !CONSOLE

/**
 * Reads the mac address for the computer
 *
 * @param MacAddr the buffer that receives the mac address
 * @param MacAddrLen (in) the size of the dest buffer, (out) the size of the data that was written
 *
 * @return TRUE if the address was read, FALSE if it failed to get the address
 */
UBOOL appGetMacAddress(BYTE* MacAddr,DWORD& MacAddrLen)
{
	UBOOL bCopiedOne = FALSE;
	IP_ADAPTER_INFO IpAddresses[16];
	DWORD OutBufferLength = sizeof(IP_ADAPTER_INFO) * 16;
	// Read the adapters
    DWORD RetVal = GetAdaptersInfo(IpAddresses,&OutBufferLength);
	if (RetVal == NO_ERROR)
	{
		PIP_ADAPTER_INFO AdapterList = IpAddresses;
		// Walk the set of addresses copying each one
		while (AdapterList && !bCopiedOne)
		{
			// If the destination buffer is large enough
			if (AdapterList->AddressLength <= MacAddrLen)
			{
				// Copy the data and say we did
				appMemcpy(MacAddr,AdapterList->Address,AdapterList->AddressLength);
				MacAddrLen = AdapterList->AddressLength;
				bCopiedOne = TRUE;
			}
			AdapterList = AdapterList->Next;
		}
	}
	else
	{
		appMemzero(MacAddr,MacAddrLen);
	}
	return bCopiedOne;
}

/**
 * Reads the mac address for the computer
 *
 * @return the MAC address as a string (or empty if MAC address could not be read).
 */
FString appGetMacAddress()
{
	// Have to assume some knowledge of how long the MAC address is so we don't risk sending a buffer too small.
	BYTE MacAddress[MAX_ADAPTER_ADDRESS_LENGTH];
	DWORD MacAddressLength = MAX_ADAPTER_ADDRESS_LENGTH;
	if (appGetMacAddress(MacAddress, MacAddressLength))
	{
		FString Result;
		for (DWORD Ndx = 0; Ndx < MacAddressLength; ++Ndx)
		{
			Result += FString::Printf(TEXT("%02x%s"),MacAddress[Ndx], Ndx != MacAddressLength-1 ? TEXT(":") : TEXT(""));
		}
		return Result;
	}
	return TEXT("");
}


/**
* Validates and converts a physical adapter MAC address string to an array of bytes.
*
* @param OutAddress Buffer to hold the resulting address.
* @param OutAddressLen Input length of OutAddress buffer.  Output size of OutAddress buffer that was used.
* @param MacAddressStr "-" delimited string representing the physical adapter address. This is a hex string
* @return TRUE if the MacAddressStr was valid and the conversion succeeded.
*/
static UBOOL ParseAdapterAddress(BYTE* OutAddress, DWORD& OutAddressLen, const TCHAR* MacAddressStr)
{
	check(OutAddressLen >= MAX_ADAPTER_ADDRESS_LENGTH && OutAddress != NULL);
	OutAddressLen = 0;
	// split string into numbers using "-" as delimiter
	TArray<FString> AddressNums;
	FString(MacAddressStr).Trim().TrimTrailing().ParseIntoArray(&AddressNums,TEXT("-"),TRUE);	
	for (INT Idx=0; Idx < AddressNums.Num(); Idx++)
	{
		// MAC address should be specified as a hex string
		INT HexNum = appStrtoi(*AddressNums(Idx),NULL,16);
		if (HexNum < 0 || HexNum > 255)
		{
			break;
		}
		OutAddress[OutAddressLen++] = (BYTE)HexNum;
	}
	// MAC address should have at least 6 numbers
	return (OutAddressLen >= 6 && OutAddressLen <= MAX_ADAPTER_ADDRESS_LENGTH);
}

/**
* Finds the adapter name for the given physical MAC address of the adapter.
*
* @param OutAdapterName Buffer to hold the resulting adapter name string.
* @param OutAdapterNameLen Input length of OutAdapterName buffer.  Output size of OutAdapterName buffer that was used.
* @param MacAddressStr "-" delimited string representing the physical adapter address. This is a hex string
* @return TRUE if the MacAddressStr was valid and the conversion succeeded.
*/
UBOOL appGetAdapterName(ANSICHAR* OutAdapterName, DWORD& OutAdapterNameLen, const TCHAR* MacAddressStr)
{
	// parse MAC address into BYTE array
	DWORD AddressLen = MAX_ADAPTER_ADDRESS_LENGTH;
	BYTE Address[MAX_ADAPTER_ADDRESS_LENGTH];
	ParseAdapterAddress(Address,AddressLen,MacAddressStr);

	IP_ADAPTER_INFO IpAddresses[16];
	DWORD OutBufferLength = sizeof(IP_ADAPTER_INFO) * 16;
	// Read the adapters
	DWORD RetVal = GetAdaptersInfo(IpAddresses,&OutBufferLength);
	if (RetVal == NO_ERROR)
	{
		PIP_ADAPTER_INFO AdapterList = IpAddresses;
		// Walk the set of addresses and find the one with the matching MAC address
		while (AdapterList )
		{
			if (AddressLen == AdapterList->AddressLength)
			{
				const DWORD AdapterNameLen = appStrlen(AdapterList->AdapterName);				
				if (appMemcmp(AdapterList->Address,Address,AddressLen) == 0 && 
					AdapterNameLen < OutAdapterNameLen)
				{	
					appStrcpyANSI(OutAdapterName,OutAdapterNameLen,AdapterList->AdapterName);
					OutAdapterNameLen = AdapterNameLen + 1;
					return TRUE;
				}
			}
			AdapterList = AdapterList->Next;
		}
	}
	return FALSE;
}

/**
 * Encrypts a buffer using the crypto API
 *
 * @param SrcBuffer the source data being encrypted
 * @param SrcLen the size of the buffer in bytes
 * @param DestBuffer (out) chunk of memory that is written to
 * @param DestLen (in) the size of the dest buffer, (out) the size of the encrypted data
 *
 * @return TRUE if the encryption worked, FALSE otherwise
 */
UBOOL appEncryptBuffer(const BYTE* SrcBuffer,const DWORD SrcLen,BYTE* DestBuffer,DWORD& DestLen)
{
	UBOOL bEncryptedOk = FALSE;
	DATA_BLOB SourceBlob, EntropyBlob, FinalBlob;
	// Set up the datablob to encrypt
	SourceBlob.cbData = SrcLen;
	SourceBlob.pbData = (BYTE*)SrcBuffer;
	// Get the mac address for mixing into the entropy (ties the encryption to a location)
	ULONGLONG MacAddress = 0;
	DWORD AddressSize = sizeof(ULONGLONG);
	appGetMacAddress((BYTE*)&MacAddress,AddressSize);
	// Set up the entropy blob (changing this breaks all previous encrypted buffers!)
	ULONGLONG Entropy = 5148284414757334885ui64;
	Entropy ^= MacAddress;
	EntropyBlob.cbData = sizeof(ULONGLONG);
	EntropyBlob.pbData = (BYTE*)&Entropy;
	// Zero the output data
	appMemzero(&FinalBlob,sizeof(DATA_BLOB));
	// Now encrypt the data
	if (CryptProtectData(&SourceBlob,
		NULL,
		&EntropyBlob,
		NULL,
		NULL,
		CRYPTPROTECT_UI_FORBIDDEN,
		&FinalBlob))
	{
		if (FinalBlob.cbData <= DestLen)
		{
			// Copy the final results
			DestLen = FinalBlob.cbData;
			appMemcpy(DestBuffer,FinalBlob.pbData,DestLen);
			bEncryptedOk = TRUE;
		}
		// Free the encryption buffer
		LocalFree(FinalBlob.pbData);
	}
	else
	{
		DWORD Error = GetLastError();
		debugf(TEXT("CryptProtectData failed w/ 0x%08x"), Error);
	}
	return bEncryptedOk;
}

/**
 * Decrypts a buffer using the crypto API
 *
 * @param SrcBuffer the source data being decrypted
 * @param SrcLen the size of the buffer in bytes
 * @param DestBuffer (out) chunk of memory that is written to
 * @param DestLen (in) the size of the dest buffer, (out) the size of the encrypted data
 *
 * @return TRUE if the decryption worked, FALSE otherwise
 */
UBOOL appDecryptBuffer(const BYTE* SrcBuffer,const DWORD SrcLen,BYTE* DestBuffer,DWORD& DestLen)
{
	UBOOL bDecryptedOk = FALSE;
	DATA_BLOB SourceBlob, EntropyBlob, FinalBlob;
	// Set up the datablob to encrypt
	SourceBlob.cbData = SrcLen;
	SourceBlob.pbData = (BYTE*)SrcBuffer;
	// Get the mac address for mixing into the entropy (ties the encryption to a location)
	ULONGLONG MacAddress = 0;
	DWORD AddressSize = sizeof(ULONGLONG);
	appGetMacAddress((BYTE*)&MacAddress,AddressSize);
	// Set up the entropy blob
	ULONGLONG Entropy = 5148284414757334885ui64;
	Entropy ^= MacAddress;
	EntropyBlob.cbData = sizeof(ULONGLONG);
	EntropyBlob.pbData = (BYTE*)&Entropy;
	// Zero the output data
	appMemzero(&FinalBlob,sizeof(DATA_BLOB));
	// Now encrypt the data
	if (CryptUnprotectData(&SourceBlob,
		NULL,
		&EntropyBlob,
		NULL,
		NULL,
		CRYPTPROTECT_UI_FORBIDDEN,
		&FinalBlob))
	{
		if (FinalBlob.cbData <= DestLen)
		{
			// Copy the final results
			DestLen = FinalBlob.cbData;
			appMemcpy(DestBuffer,FinalBlob.pbData,DestLen);
			bDecryptedOk = TRUE;
		}
		// Free the decryption buffer
		LocalFree(FinalBlob.pbData);
	}
	return bDecryptedOk;
}


/** 
 * Clamps each level to a 1..5 range. 0 normally means unsupported, 
 * but gets clamped to 1 if the user chooses to continue anyway. 
 */
FCompatibilityLevelInfo FCompatibilityLevelInfo::ClampToValidRange() const
{
	return FCompatibilityLevelInfo(
		Clamp(CompositeLevel, 1u, 5u),
		Clamp(CPULevel, 1u, 5u),
		Clamp(GPULevel, 1u, 5u));
}


/**
 * Prevents screen-saver from kicking in by moving the mouse by 0 pixels. This works even on
 * Vista in the presence of a group policy for password protected screen saver.
 */
void appPreventScreenSaver()
{
	INPUT Input = { 0 };
	Input.type = INPUT_MOUSE;
	Input.mi.dx = 0;
	Input.mi.dy = 0;	
	Input.mi.mouseData = 0;
	Input.mi.dwFlags = MOUSEEVENTF_MOVE;
	Input.mi.time = 0;
	Input.mi.dwExtraInfo = 0; 	
	SendInput(1,&Input,sizeof(INPUT));
}


#endif // _WINDOWS && !CONSOLE