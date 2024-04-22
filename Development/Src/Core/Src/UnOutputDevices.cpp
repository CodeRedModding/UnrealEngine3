/*=============================================================================
	UnOutputDevices.cpp: Collection of FOutputDevice subclasses
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#include <stdio.h>
#if !CONSOLE && _MSC_VER
#pragma warning (push)
#pragma warning (disable : 4548) // needed as xlocale does not compile cleanly
#include <iostream>
#pragma warning (pop)
#endif

/*-----------------------------------------------------------------------------
	FOutputDeviceRedirector.
-----------------------------------------------------------------------------*/

/** Initialization constructor. */
FOutputDeviceRedirector::FOutputDeviceRedirector()
:	MasterThreadID(appGetCurrentThreadId())
,	bEnableBacklog(FALSE)
{
}

/**
 * Adds an output device to the chain of redirections.	
 *
 * @param OutputDevice	output device to add
 */
void FOutputDeviceRedirector::AddOutputDevice( FOutputDevice* OutputDevice )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	if( OutputDevice )
	{
		OutputDevices.AddUniqueItem( OutputDevice );
	}
}

/**
 * Removes an output device from the chain of redirections.	
 *
 * @param OutputDevice	output device to remove
 */
void FOutputDeviceRedirector::RemoveOutputDevice( FOutputDevice* OutputDevice )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	OutputDevices.RemoveItem( OutputDevice );
}

/**
 * Returns whether an output device is currently in the list of redirectors.
 *
 * @param	OutputDevice	output device to check the list against
 * @return	TRUE if messages are currently redirected to the the passed in output device, FALSE otherwise
 */
UBOOL FOutputDeviceRedirector::IsRedirectingTo( FOutputDevice* OutputDevice )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	return OutputDevices.FindItemIndex( OutputDevice ) == INDEX_NONE ? FALSE : TRUE;
}

/**
 * The unsynchronized version of FlushThreadedLogs.
 * Assumes that the caller holds a lock on SynchronizationObject.
 */
void FOutputDeviceRedirector::UnsynchronizedFlushThreadedLogs()
{
	for(INT LineIndex = 0;LineIndex < BufferedLines.Num();LineIndex++)
	{
		const FBufferedLine& BufferedLine = BufferedLines(LineIndex);

		for( INT OutputDeviceIndex=0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++ )
		{
			OutputDevices(OutputDeviceIndex)->Serialize( *BufferedLine.Data, BufferedLine.Event );
		}
	}

	BufferedLines.Empty();
}

/**
 * Flushes lines buffered by secondary threads.
 */
void FOutputDeviceRedirector::FlushThreadedLogs()
{
	// Acquire a lock on SynchronizationObject and call the unsynchronized worker function.
	FScopeLock ScopeLock( &SynchronizationObject );
	UnsynchronizedFlushThreadedLogs();
}

/**
 * Serializes the current backlog to the specified output device.
 * @param OutputDevice	- Output device that will receive the current backlog
 */
void FOutputDeviceRedirector::SerializeBacklog( FOutputDevice* OutputDevice )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	for (INT LineIndex = 0; LineIndex < BacklogLines.Num(); LineIndex++)
	{
		const FBufferedLine& BacklogLine = BacklogLines( LineIndex );
		OutputDevice->Serialize( *BacklogLine.Data, BacklogLine.Event );
	}
}

/**
 * Enables or disables the backlog.
 * @param bEnable	- Starts saving a backlog if TRUE, disables and discards any backlog if FALSE
 */
void FOutputDeviceRedirector::EnableBacklog( UBOOL bEnable )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	bEnableBacklog = bEnable;
	if ( bEnableBacklog == FALSE )
	{
		BacklogLines.Empty();
	}
}

/**
 * Sets the current thread to be the master thread that prints directly
 * (isn't queued up)
 */
void FOutputDeviceRedirector::SetCurrentThreadAsMasterThread()
{
	// make sure anything queued up is flushed out
	FlushThreadedLogs();

	// set the current thread as the master thread
	MasterThreadID = appGetCurrentThreadId();
}

/**
 * Serializes the passed in data via all current output devices.
 *
 * @param	Data	Text to log
 * @param	Event	Event name used for suppression purposes
 */
void FOutputDeviceRedirector::Serialize( const TCHAR* Data, enum EName Event )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	if ( bEnableBacklog )
	{
		new(BacklogLines) FBufferedLine(Data,Event);
	}

	if(appGetCurrentThreadId() != MasterThreadID || OutputDevices.Num() == 0)
	{
		new(BufferedLines) FBufferedLine(Data,Event);
	}
	else
	{
		// Flush previously buffered lines from secondary threads.
		// Since we already hold a lock on SynchronizationObject, call the unsynchronized version.
		UnsynchronizedFlushThreadedLogs();

		for( INT OutputDeviceIndex=0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++ )
		{
			OutputDevices(OutputDeviceIndex)->Serialize( Data, Event );
		}
	}
}

/**
 * Passes on the flush request to all current output devices.
 */
void FOutputDeviceRedirector::Flush()
{
	if(appGetCurrentThreadId() == MasterThreadID)
	{
		FScopeLock ScopeLock( &SynchronizationObject );

		// Flush previously buffered lines from secondary threads.
		// Since we already hold a lock on SynchronizationObject, call the unsynchronized version.
		UnsynchronizedFlushThreadedLogs();

		for( INT OutputDeviceIndex=0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++ )
		{
			OutputDevices(OutputDeviceIndex)->Flush();
		}
	}
}

/**
 * Closes output device and cleans up. This can't happen in the destructor
 * as we might have to call "delete" which cannot be done for static/ global
 * objects.
 */
void FOutputDeviceRedirector::TearDown()
{
	check(appGetCurrentThreadId() == MasterThreadID);

	FScopeLock ScopeLock( &SynchronizationObject );

	// Flush previously buffered lines from secondary threads.
	// Since we already hold a lock on SynchronizationObject, call the unsynchronized version.
	UnsynchronizedFlushThreadedLogs();

	for( INT OutputDeviceIndex=0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++ )
	{
		OutputDevices(OutputDeviceIndex)->TearDown();
	}
	OutputDevices.Empty();
}


/*-----------------------------------------------------------------------------
	FOutputDevice subclasses.
-----------------------------------------------------------------------------*/

/** 
 * Constructor, initializing member variables.
 *
 * @param InFilename		Filename to use, can be NULL
 * @param bInDisableBackup	If TRUE, existing files will not be backed up
 */
FOutputDeviceFile::FOutputDeviceFile( const TCHAR* InFilename, UBOOL bInDisableBackup, UBOOL bInRespectAllowDebugFilesDefine )
:	LogAr( NULL ),
	Opened( 0 ),
	Dead( 0 ),
	bDisableBackup(bInDisableBackup),
	bRespectAllowDebugFilesDefine(bInRespectAllowDebugFilesDefine)
{
	if( InFilename )
	{
		appStrncpy( Filename, InFilename, ARRAY_COUNT(Filename) );
	}
	else
	{
		Filename[0]	= 0;
	}
}

/**
 * Closes output device and cleans up. This can't happen in the destructor
 * as we have to call "delete" which cannot be done for static/ global
 * objects.
 */
void FOutputDeviceFile::TearDown()
{
	if( LogAr )
	{
		if (!bSuppressEventTag)
		{
			Logf( NAME_Log, TEXT("Log file closed, %s"), appTimestamp() );
		}
		delete LogAr;
		LogAr = NULL;
	}
}

/**
 * Flush the write cache so the file isn't truncated in case we crash right
 * after calling this function.
 */
void FOutputDeviceFile::Flush()
{
	if( LogAr )
	{
		LogAr->Flush();
	}
}

/** if the passed in file exists, makes a timestamped backup copy
 * @param Filename the name of the file to check
 */
static void CreateBackupCopy(TCHAR* Filename)
{
	if (GFileManager->FileSize(Filename) > 0)
	{
		FString SystemTime = appSystemTimeString();
		FString Name, Extension;
		FString(Filename).Split(TEXT("."), &Name, &Extension, TRUE);
		FString BackupFilename = FString::Printf(TEXT("%s%s%s.%s"), *Name, BACKUP_LOG_FILENAME_POSTFIX, *SystemTime, *Extension);
		GFileManager->Copy(*BackupFilename, Filename, FALSE);
	}
}

/**
 * Serializes the passed in data unless the current event is suppressed.
 *
 * @param	Data	Text to log
 * @param	Event	Event name used for suppression purposes
 */
void FOutputDeviceFile::Serialize( const TCHAR* Data, enum EName Event )
{
	UBOOL bShouldLog = TRUE;
	#if !ALLOW_DEBUG_FILES
	if (bRespectAllowDebugFilesDefine)
	{
		bShouldLog = FALSE;
	}
	#endif

	if (!bShouldLog)
	{
		return;
	}

	static UBOOL Entry=0;
	if( !GIsCriticalError || Entry )
	{
		if( !FName::SafeSuppressed(Event) )
		{
			if( !LogAr && !Dead )
			{
				// Make log filename.
				if( !Filename[0] )
				{
					// The Editor requires a fully qualified directory to not end up putting the log in various directories.
					appStrcpy( Filename, appBaseDir() );
					appStrcat( Filename, *appGameLogDir() );

					if(	!Parse(appCmdLine(), TEXT("LOG="), Filename+appStrlen(Filename), ARRAY_COUNT(Filename)-appStrlen(Filename) )
					&&	!Parse(appCmdLine(), TEXT("ABSLOG="), Filename, ARRAY_COUNT(Filename) ) )
					{
						appStrcat( Filename, GPackage );
						appStrcat( Filename, TEXT(".log") );
					}
				}

				// if the file already exists, create a backup as we are going to overwrite it
#if !PS3 && !MOBILE && !WIIU
				if (!bDisableBackup && !Opened)
				{
					CreateBackupCopy(Filename);
				}
#endif

				// Open log file.
#if ALLOW_DEBUG_FILES
				LogAr = GFileManager->CreateDebugFileWriter( Filename, FILEWRITE_AllowRead|(Opened?FILEWRITE_Append:0));
#endif

				// If that failed, append an _2 and try again (unless we don't want extra copies). This 
				// happens in the case of running a server and client on same computer for example.
				if(!bDisableBackup && !LogAr)
				{				  
					INT FileIndex = 2;
					TCHAR ExtBuffer[16];
					appStrcpy(ExtBuffer, TEXT(".log"));
					do 
					{
                        //Continue to increment indices until a valid filename is found
						Filename[ appStrlen(Filename) - appStrlen(ExtBuffer) ] = 0;
						appSprintf(ExtBuffer, TEXT("_%d.log"), FileIndex++);
						appStrcat( Filename, ExtBuffer );
						if (!Opened)
						{
							CreateBackupCopy(Filename);
						}
						LogAr = GFileManager->CreateFileWriter( Filename, FILEWRITE_AllowRead|(Opened?FILEWRITE_Append:0));
					}
					while(!LogAr && FileIndex < 32);
				}

				if( LogAr )
				{
					Opened = 1;
// Unix will write the log file in UTF-8, so no BOM needed!
#if !FORCE_ANSI_LOG && !PLATFORM_UNIX
					WORD UnicodeBOM = UNICODE_BOM;
					LogAr->Serialize( &UnicodeBOM, 2 );
#endif
					if (!bSuppressEventTag)
					{
						Logf( NAME_Log, TEXT("Log file open, %s"), appTimestamp() );
					}
				}
				else 
				{
					Dead = TRUE;
				}
			}

			if( LogAr && Event!=NAME_Title && Event != NAME_Color )
			{
#if FORCE_ANSI_LOG
				INT i = 0;
				ANSICHAR ACh[MAX_SPRINTF];
				if (!bSuppressEventTag)
				{
					TCHAR Ch[MAX_SPRINTF] = TEXT("");

					// Log the timestamp.
					if(GPrintLogTimes)
					{
						INT Len = appSprintfANSI(ACh, "[%07.2f] ", appSeconds() - GStartTime);
						LogAr->Serialize( ACh, Len );
					}

					appStrcat(Ch,*FName::SafeString(Event));
					appStrcat(Ch,TEXT(": "));
					for( i=0; Ch[i]; i++ )
					{
						ACh[i] = ToAnsi(Ch[i] );
					}
					LogAr->Serialize( ACh, i );
				}

				INT DataOffset = 0;
				while(Data[DataOffset])
				{
					for(i = 0;i < ARRAY_COUNT(ACh) && Data[DataOffset];i++,DataOffset++)
					{
						ACh[i] = Data[DataOffset];
					}
					LogAr->Serialize(ACh,i);
				};

				if (bAutoEmitLineTerminator)
				{
					for(i = 0;LINE_TERMINATOR[i];i++)
					{
						ACh[i] = LINE_TERMINATOR[i];
					}
					LogAr->Serialize(ACh,i);
				}
#else
				if (!bSuppressEventTag)
				{
					WriteRaw( *FName::SafeString(Event) );
					WriteRaw( TEXT(": ") );
				}
				WriteRaw( Data );
				if (bAutoEmitLineTerminator)
				{
					WriteRaw( LINE_TERMINATOR );
				}
#endif

				if( GForceLogFlush )
				{
					LogAr->Flush();
				}
			}
		}
	}
	else
	{
		Entry=1;
#if !EXCEPTIONS_DISABLED
		try
#endif
		{
			// Ignore errors to prevent infinite-recursive exception reporting.
			Serialize( Data, Event );
		}
#if !EXCEPTIONS_DISABLED
		catch( ... )
		{}
#endif
		Entry=0;
	}
}


#if PLATFORM_UNIX
static inline void WriteUTF8( FArchive *Ar, const char* C )
{
	Ar->Serialize( const_cast<char*>(C), strlen(C)*sizeof(char) );
}
#endif

void FOutputDeviceFile::WriteRaw( const TCHAR* C )
{
#if PLATFORM_UNIX
    // push through another function so TCHAR_TO_UTF8 pointer stays valid.
	WriteUTF8(LogAr, TCHAR_TO_UTF8(C));
#else
	LogAr->Serialize( const_cast<TCHAR*>(C), appStrlen(C)*sizeof(TCHAR) );
#endif
}

/**
 * Serializes the passed in data unless the current event is suppressed.
 *
 * @param	Data	Text to log
 * @param	Event	Event name used for suppression purposes
 */
void FOutputDeviceDebug::Serialize( const TCHAR* Data, enum EName Event )
{
	static UBOOL Entry=0;
	if( !GIsCriticalError || Entry )
	{
		if( !FName::SafeSuppressed(Event) )
		{
			if (Event == NAME_Color)
			{
#if PS3
#define ESCCHAR     "\x1b"

#define TTY_BLACK   ESCCHAR"\x1e"
#define TTY_RED     ESCCHAR"\x1f"
#define TTY_GREEN   ESCCHAR"\x20"
#define TTY_YELLOW  ESCCHAR"\x21"
#define TTY_BLUE    ESCCHAR"\x22"
#define TTY_MAGENTA ESCCHAR"\x23"
#define TTY_CYAN    ESCCHAR"\x24"
#define TTY_LTGREY  ESCCHAR"\x25"

#define TTY_BG_BLACK    ESCCHAR"\x28"
#define TTY_BG_RED      ESCCHAR"\x29"
#define TTY_BG_GREEN    ESCCHAR"\x2A"
#define TTY_BG_DKYELLOW ESCCHAR"\x2B"
#define TTY_BG_BLUE     ESCCHAR"\x2C"
#define TTY_BG_MAGENTA  ESCCHAR"\x2D"
#define TTY_BG_CYAN     ESCCHAR"\x2E"
#define TTY_BG_BKGRND   ESCCHAR"\x2F"

				if (appStricmp(Data, TEXT("")) == 0)
				{
					printf(TTY_BLACK);
				}
				else
				{
					printf(TTY_GREEN);
				}
#endif
			}
			else
			if( Event!=NAME_Title)// && Event != NAME_Color )
			{
				if(GPrintLogTimes)
				{
					appOutputDebugStringf(TEXT("[%07.2f] %s: %s%s"), appSeconds() - GStartTime, *FName::SafeString(Event), Data, LINE_TERMINATOR);
				}
				else
				{
					appOutputDebugStringf(TEXT("%s: %s%s"),*FName::SafeString(Event), Data, LINE_TERMINATOR);
				}
			}
		}
	}
	else
	{
		Entry=1;
#if !EXCEPTIONS_DISABLED
		try
#endif
		{
			// Ignore errors to prevent infinite-recursive exception reporting.
			Serialize( Data, Event );
		}
#if !EXCEPTIONS_DISABLED
		catch( ... )
		{}
#endif
		Entry=0;
	}
}

/*-----------------------------------------------------------------------------
	FOutputDeviceError subclasses.
-----------------------------------------------------------------------------*/

/** Constructor, initializing member variables */
FOutputDeviceAnsiError::FOutputDeviceAnsiError()
:	ErrorPos(0),
	ErrorType(NAME_None)
{}

/**
 * Serializes the passed in data unless the current event is suppressed.
 *
 * @param	Data	Text to log
 * @param	Event	Event name used for suppression purposes
 */
void FOutputDeviceAnsiError::Serialize( const TCHAR* Msg, enum EName Event )
{
	// Display the error and exit.
  	LocalPrint( TEXT("\nappError called: \n") );
	LocalPrint( Msg );
  	LocalPrint( TEXT("\n") );

	if( !GIsCriticalError )
	{
		// First appError.
		GIsCriticalError = 1;
		ErrorType        = Event;
		debugf( NAME_Critical, TEXT("appError called: %s"), Msg );
#if !CONSOLE
		appStrncpy( GErrorHist, Msg, ARRAY_COUNT(GErrorHist) - 5 );
		appStrncat( GErrorHist, TEXT("\r\n\r\n"), ARRAY_COUNT(GErrorHist) - 1 );
		ErrorPos = appStrlen(GErrorHist);
#endif
	}
	else
	{
		debugf( NAME_Critical, TEXT("Error reentered: %s"), Msg );
	}

	appDebugBreak();

	if( GIsGuarded )
	{
		// Propagate error so structured exception handler can perform necessary work.
#if EXCEPTIONS_DISABLED
		appDebugBreak();
#else
		appRaiseException( 1 );
#endif
	}
	else
	{
		// We crashed outside the guarded code (e.g. appExit).
		HandleError();
		// pop up a crash window if we are not in unattended mode
		if( GIsUnattended == FALSE )
		{
			appRequestExit( TRUE );
		}
		else
		{
			warnf( NAME_Critical, TEXT("%s"), Msg );
		}		
	}
}

/**
 * Error handling function that is being called from within the system wide global
 * error handler, e.g. using structured exception handling on the PC.
 */
void FOutputDeviceAnsiError::HandleError()
{
#if !EXCEPTIONS_DISABLED
	try
#endif
	{
		GIsGuarded			= 0;
		GIsRunning			= 0;
		GIsCriticalError	= 1;
		GLogConsole			= NULL;
#if !CONSOLE
		GErrorHist[ErrorType==NAME_FriendlyError ? ErrorPos : ARRAY_COUNT(GErrorHist)-1]=0;
		LocalPrint( GErrorHist );
#endif
		LocalPrint( TEXT("\n\nExiting due to error\n") );

		UObject::StaticShutdownAfterError();
	}
#if !EXCEPTIONS_DISABLED
	catch( ... )
	{}
#endif
}

void FOutputDeviceAnsiError::LocalPrint( const TCHAR* Str )
{
#if PS3 || PLATFORM_MACOSX
	printf("%s", TCHAR_TO_ANSI(Str));
#else
	wprintf(Str);
#endif
}

#if !CONSOLE && defined(_MSC_VER)

/** Constructor, initializing member variables */
FOutputDeviceWindowsError::FOutputDeviceWindowsError()
:	ErrorPos(0),
	ErrorType(NAME_None)
{}

/**
 * Serializes the passed in data unless the current event is suppressed.
 *
 * @param	Data	Text to log
 * @param	Event	Event name used for suppression purposes
 */
void FOutputDeviceWindowsError::Serialize( const TCHAR* Msg, enum EName Event )
{
	appDebugBreak();
   
	INT Error = GetLastError();
	if( !GIsCriticalError )
	{   
		// First appError.
		GIsCriticalError = 1;
		ErrorType        = Event;
#if !NO_LOGGING
		TCHAR ErrorBuffer[1024];
		// pop up a crash window if we are not in unattended mode
		if( GIsUnattended == FALSE )
		{ 
			debugf( NAME_Critical, TEXT("appError called: %s"), Msg );

			// Windows error.
			debugf( NAME_Critical, TEXT("Windows GetLastError: %s (%i)"), appGetSystemErrorMessage(ErrorBuffer,1024,Error), Error );
		} 
		// log the warnings to the log
		else
		{
			warnf( NAME_Critical, TEXT("appError called: %s"), Msg );

			// Windows error.
			warnf( NAME_Critical, TEXT("Windows GetLastError: %s (%i)"), appGetSystemErrorMessage(ErrorBuffer,1024,Error), Error );
		}
#endif
		appStrncpy( GErrorHist, Msg, ARRAY_COUNT(GErrorHist) - 5 );
		appStrncat( GErrorHist, TEXT("\r\n\r\n"), ARRAY_COUNT(GErrorHist) - 1  );
		ErrorPos = appStrlen(GErrorHist);
	}
	else
	{
		debugf( NAME_Critical, TEXT("Error reentered: %s"), Msg );
	}

	if( GIsGuarded )
	{
		// Propagate error so structured exception handler can perform necessary work.
#if EXCEPTIONS_DISABLED
		appDebugBreak();
#endif
		appRaiseException( 1 );
	}
	else
	{
		// We crashed outside the guarded code (e.g. appExit).
		HandleError();
		appRequestExit( TRUE );
	}
}

/**
 * Error handling function that is being called from within the system wide global
 * error handler, e.g. using structured exception handling on the PC.
 */
static UBOOL GHandleErrorAlreadyCalled = FALSE;
void FOutputDeviceWindowsError::HandleError()
{
	// make sure we don't report errors twice
	static INT CallCount = 0;
	INT NewCallCount = appInterlockedIncrement(&CallCount);
	if (NewCallCount != 1)
	{
		debugf( NAME_Critical, TEXT("HandleError re-entered.") );
		return;
	}

	try
	{
		GIsGuarded				= 0;
		GIsRunning				= 0;
		GIsCriticalError		= 1;
		GLogConsole				= NULL;
		GErrorHist[ErrorType==NAME_FriendlyError ? ErrorPos : ARRAY_COUNT(GErrorHist)-1]=0;
		// Dump the error and flush the log.
		debugf(TEXT("=== Critical error: ===") LINE_TERMINATOR TEXT("%s"), GErrorHist);
		GLog->Flush();

		// Unhide the mouse.
		while( ::ShowCursor(TRUE)<0 );
		// Release capture.
		::ReleaseCapture();
		// Allow mouse to freely roam around.
		::ClipCursor(NULL);

		appClipboardCopy(GErrorHist);

		if( GIsEpicInternal )
		{
			appSubmitErrorReport( GErrorHist, EErrorReportMode::Interactive );
		}
#if !SHIPPING_PC_GAME && (!FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE)
		else
		{
			appMsgf( AMT_OK, TEXT("%s"), GErrorHist );
		}
#endif


		UObject::StaticShutdownAfterError();
	}
	catch( ... )
	{}
}

#endif

/*-----------------------------------------------------------------------------
	FOutputDeviceConsole subclasses.
-----------------------------------------------------------------------------*/

#if !CONSOLE && defined(_MSC_VER)

/** 
 * Constructor, setting console control handler.
 */
FOutputDeviceConsoleWindowsInherited::FOutputDeviceConsoleWindowsInherited(FOutputDeviceConsole &forward)
: ForwardConsole(forward)
{
	ConsoleHandle = INVALID_HANDLE_VALUE;
	Shown = FALSE;
}

/**
 * Destructor, closing handle.
 */
FOutputDeviceConsoleWindowsInherited::~FOutputDeviceConsoleWindowsInherited()
{
	if(INVALID_HANDLE_VALUE != ConsoleHandle)
	{
		FlushFileBuffers(ConsoleHandle);
		CloseHandle(ConsoleHandle);
		ConsoleHandle = INVALID_HANDLE_VALUE;
	}
}

/**
 * Attempt to connect to the pipes set up by our .com launcher
 *
 * @retval TRUE if connection was successful, FALSE otherwise
 */
UBOOL FOutputDeviceConsoleWindowsInherited::Connect()
{
	if(INVALID_HANDLE_VALUE == ConsoleHandle)
	{
		// This code was lifted from this article on CodeGuru
		// http://www.codeguru.com/Cpp/W-D/console/redirection/article.php/c3955/
		//
		// It allows us to connect to a console IF we were launched from the command line
		// it requires the CONSOLE.COM helper app included
		TCHAR szOutputPipeName[MAX_SPRINTF]=TEXT("");

		// Construct named pipe names...
		appSprintf( szOutputPipeName, TEXT("\\\\.\\pipe\\%dcout"), GetCurrentProcessId() );

		// ... and see if we can connect.
		ConsoleHandle = CreateFile(szOutputPipeName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

		SetStdHandle(STD_OUTPUT_HANDLE, ConsoleHandle);
	}
	return INVALID_HANDLE_VALUE != ConsoleHandle;
}

/**
 * Shows or hides the console window. 
 *
 * @param ShowWindow	Whether to show (TRUE) or hide (FALSE) the console window.
 */
void FOutputDeviceConsoleWindowsInherited::Show( UBOOL ShowWindow )
{
	if(INVALID_HANDLE_VALUE != ConsoleHandle)
	{
		// keep track of "shown" status
		Shown = !Shown;
	}
	else
	{
		ForwardConsole.Show(ShowWindow);
	}
}

/** 
 * Returns whether console is currently shown or not.
 *
 * @return TRUE if console is shown, FALSE otherwise
 */
UBOOL FOutputDeviceConsoleWindowsInherited::IsShown()
{
	if(INVALID_HANDLE_VALUE != ConsoleHandle)
	{
		return Shown;
	}
	else
	{
		return ForwardConsole.IsShown();
	}
}

/**
 * Returns whether the console has been inherited (TRUE) or created (FALSE).
 *
 * @return TRUE if console is inherited, FALSE if it was created
 */
UBOOL FOutputDeviceConsoleWindowsInherited::IsInherited() const
{
	if(INVALID_HANDLE_VALUE != ConsoleHandle)
	{
		return TRUE;
	}
	else
	{
		return ForwardConsole.IsInherited();
	}
}

/**
 * Disconnect an inherited console. Default does nothing.
 */
void FOutputDeviceConsoleWindowsInherited::DisconnectInherited()
{
	if(INVALID_HANDLE_VALUE != ConsoleHandle)
	{
		// Try to tell console handler to die.
		TCHAR szDieEvent[MAX_SPRINTF]=TEXT("");

		// Construct die event name.
		appSprintf( szDieEvent, TEXT("dualmode_die_event%d"), GetCurrentProcessId() );

		HANDLE hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, szDieEvent);

		if(NULL != hEvent)
		{
			// Die Consle die.
			SetEvent(hEvent);
			CloseHandle(hEvent);
		}
		CloseHandle(ConsoleHandle);
		ConsoleHandle = INVALID_HANDLE_VALUE;

		if(Shown)
		{
			ForwardConsole.Show(Shown);
		}
	}
}

/**
 * Displays text on the console and scrolls if necessary.
 *
 * @param Data	Text to display
 * @param Event	Event type, used for filtering/ suppression
 */
void FOutputDeviceConsoleWindowsInherited::Serialize( const TCHAR* Data, enum EName Event )
{
	if(INVALID_HANDLE_VALUE != ConsoleHandle )
	{
		if(Shown)
		{
			static UBOOL Entry=0;
			if( !GIsCriticalError || Entry )
			{
				if( !FName::SafeSuppressed(Event) )
				{
					if( Event == NAME_Title )
					{
						// do nothing
					}
					else
					{
						FString OutputString;

						if (Event == NAME_Color)
						{

							#define UNI_COLOR_MAGIC TEXT("`~[~`")
							OutputString = UNI_COLOR_MAGIC;
							if (appStricmp(Data, TEXT("")) == 0)
							{
								OutputString += TEXT("Reset");
							}
							else
							{
								OutputString += Data;
							}
							OutputString += LINE_TERMINATOR;
						}
						else 
						{
							if( Event != NAME_None )
							{
								OutputString = *FName::SafeString(Event);
								OutputString += TEXT(": ");
							}
							OutputString += Data;
							OutputString += LINE_TERMINATOR;
						}

						DWORD toWrite = OutputString.Len();
						DWORD written;
						WriteFile(ConsoleHandle, *OutputString, toWrite*sizeof(TCHAR), &written, NULL);
						FlushFileBuffers(ConsoleHandle);
					}
				}
			}
			else
			{
				Entry=1;
				try
				{
					// Ignore errors to prevent infinite-recursive exception reporting.
					Serialize( Data, Event );
				}
				catch( ... )
				{}
				Entry=0;
			}
		}
	}
	else
	{
		ForwardConsole.Serialize(Data, Event);
	}
}

/**
 * Handler called for console events like closure, CTRL-C, ...
 *
 * @param Type	unused
 */
static BOOL WINAPI ConsoleCtrlHandler( DWORD /*Type*/ )
{
	// make sure as much data is written to disk as possible
	if (GLog)
	{
		GLog->Flush();
	}
	if (GWarn)
	{
		GWarn->Flush();
	}
	if (GError)
	{
		GError->Flush();
	}

	if( !GIsRequestingExit )
	{
		PostQuitMessage( 0 );
		GIsRequestingExit = 1;
	}
	else
	{
		ExitProcess(0);
	}
	return TRUE;
}


/** 
 * Constructor
 */
FOutputDeviceConsoleWindows::FOutputDeviceConsoleWindows()
{
}

/** 
* Destructor
*/
FOutputDeviceConsoleWindows::~FOutputDeviceConsoleWindows()
{
	SaveToINI();

	// WRH - 2007/08/23 - This causes the process to take a LONG time to shut down when clicking the "close window"
	// button on the top-right of the console window.
	//FreeConsole();
}

/**
 * Saves the console window's position and size to the game .ini
 */
void FOutputDeviceConsoleWindows::SaveToINI()
{
	if(GConfig)
	{
		RECT WindowRect;
		::GetWindowRect( GetConsoleWindow(), &WindowRect );

		CONSOLE_SCREEN_BUFFER_INFO Info;
		GetConsoleScreenBufferInfo(ConsoleHandle, &Info);

		GConfig->SetInt(TEXT("DebugWindows"), TEXT("ConsoleWidth"), Info.dwSize.X, GGameIni);
		GConfig->SetInt(TEXT("DebugWindows"), TEXT("ConsoleHeight"), Info.dwSize.Y, GGameIni);
		GConfig->SetInt(TEXT("DebugWindows"), TEXT("ConsoleX"), WindowRect.left, GGameIni);
		GConfig->SetInt(TEXT("DebugWindows"), TEXT("ConsoleY"), WindowRect.top, GGameIni);
	}
}

/**
 * Shows or hides the console window. 
 *
 * @param ShowWindow	Whether to show (TRUE) or hide (FALSE) the console window.
 */
void FOutputDeviceConsoleWindows::Show( UBOOL ShowWindow )
{
	if( ShowWindow )
	{
		if( !ConsoleHandle )
		{
			AllocConsole();
			ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

			if( ConsoleHandle )
			{
				COORD Size;
				Size.X = 160;
				Size.Y = 4000;

				INT ConsoleWidth = 160;
				INT ConsoleHeight = 4000;
				INT ConsolePosX = 0;
				INT ConsolePosY = 0;
				UBOOL bHasX = FALSE;
				UBOOL bHasY = FALSE;

				if(GConfig)
				{
					GConfig->GetInt(TEXT("DebugWindows"), TEXT("ConsoleWidth"), ConsoleWidth, GGameIni);
					GConfig->GetInt(TEXT("DebugWindows"), TEXT("ConsoleHeight"), ConsoleHeight, GGameIni);
					bHasX = GConfig->GetInt(TEXT("DebugWindows"), TEXT("ConsoleX"), ConsolePosX, GGameIni);
					bHasY = GConfig->GetInt(TEXT("DebugWindows"), TEXT("ConsoleY"), ConsolePosY, GGameIni);

					Size.X = (SHORT)ConsoleWidth;
					Size.Y = (SHORT)ConsoleHeight;
				}

				SetConsoleScreenBufferSize( ConsoleHandle, Size );

				RECT WindowRect;
				::GetWindowRect( GetConsoleWindow(), &WindowRect );

				if (!Parse(appCmdLine(), TEXT("ConsolePosX="), ConsolePosX) && !bHasX)
				{
					ConsolePosX = WindowRect.left;
				}

				if (!Parse(appCmdLine(), TEXT("ConsolePosY="), ConsolePosY) && !bHasY)
				{
					ConsolePosY = WindowRect.top;
				}

				::SetWindowPos( GetConsoleWindow(), HWND_TOP, ConsolePosX, ConsolePosY, 0, 0, SWP_NOSIZE | SWP_NOSENDCHANGING | SWP_NOZORDER );
				
				// set the control-c, etc handler
				appSetConsoleInterruptHandler();
			}
		}
	}
	else if( ConsoleHandle )
	{
		SaveToINI();

		ConsoleHandle = NULL;
		FreeConsole();
	}
}

/** 
 * Returns whether console is currently shown or not
 *
 * @return TRUE if console is shown, FALSE otherwise
 */
UBOOL FOutputDeviceConsoleWindows::IsShown()
{
	return ConsoleHandle != NULL;
}

/**
 * Displays text on the console and scrolls if necessary.
 *
 * @param Data	Text to display
 * @param Event	Event type, used for filtering/ suppression
 */
void FOutputDeviceConsoleWindows::Serialize( const TCHAR* Data, enum EName Event )
{
	if( ConsoleHandle )
	{
		static UBOOL Entry=0;
		if( !GIsCriticalError || Entry )
		{
			if( !FName::SafeSuppressed(Event) )
			{
				if( Event == NAME_Title )
				{
					SetConsoleTitle( Data );
				}
				// here we can change the color of the text to display, it's in the format:
				// ForegroundRed | ForegroundGreen | ForegroundBlue | ForegroundBright | BackgroundRed | BackgroundGreen | BackgroundBlue | BackgroundBright
				// where each value is either 0 or 1 (can leave off trailing 0's), so 
				// blue on bright yellow is "00101101" and red on black is "1"
				// An empty string reverts to the normal gray on black
				else if (Event == NAME_Color)
				{
					if (appStricmp(Data, TEXT("")) == 0)
					{
						SetConsoleTextAttribute(ConsoleHandle, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
					}
					else
					{
						// turn the string into a bunch of 0's and 1's
						TCHAR String[9];
						appMemset(String, 0, sizeof(TCHAR) * ARRAY_COUNT(String));
						appStrncpy(String, Data, ARRAY_COUNT(String));
						for (TCHAR* S = String; *S; S++)
						{
							*S -= '0';
						}
						// make the color
						SetConsoleTextAttribute(ConsoleHandle, 
							(String[0] ? FOREGROUND_RED			: 0) | 
							(String[1] ? FOREGROUND_GREEN		: 0) | 
							(String[2] ? FOREGROUND_BLUE		: 0) | 
							(String[3] ? FOREGROUND_INTENSITY	: 0) | 
							(String[4] ? BACKGROUND_RED			: 0) | 
							(String[5] ? BACKGROUND_GREEN		: 0) | 
							(String[6] ? BACKGROUND_BLUE		: 0) | 
							(String[7] ? BACKGROUND_INTENSITY	: 0) );
					}
				}
				else
				{
					TCHAR OutputString[MAX_SPRINTF]=TEXT(""); //@warning: this is safe as appSprintf only use 1024 characters max
					if( Event == NAME_None )
					{
						if (GPrintLogTimes)
						{
							appSprintf(OutputString,TEXT("[%07.2f] %s%s"),appSeconds() - GStartTime,Data,LINE_TERMINATOR);
						}
						else
						{
							appSprintf(OutputString,TEXT("%s%s"),Data,LINE_TERMINATOR);
						}
					}
					else
					{
						if (GPrintLogTimes)
						{
							appSprintf(OutputString,TEXT("[%07.2f] %s: %s%s"),appSeconds() - GStartTime,*FName::SafeString(Event),Data,LINE_TERMINATOR);
						}
						else
						{
							appSprintf(OutputString,TEXT("%s: %s%s"),*FName::SafeString(Event),Data,LINE_TERMINATOR);
						}
					}

					DWORD Written;
					WriteConsole( ConsoleHandle, OutputString, appStrlen(OutputString), &Written, NULL );
				}
			}
		}
		else
		{
			Entry=1;
			try
			{
				// Ignore errors to prevent infinite-recursive exception reporting.
				Serialize( Data, Event );
			}
			catch( ... )
			{}
			Entry=0;
		}
	}
}

#endif // CONSOLE

#ifdef _MSC_VER

/**
 * Set/restore the Console Interrupt (Control-C, Control-Break, Close) handler
 */
void appSetConsoleInterruptHandler()
{
#if !CONSOLE
	// Set console control handler so we can exit if requested.
	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
#endif
}

#endif // _MSC_VER

#if WANTS_WINDOWS_EVENT_LOGGING
/**
 * Constructor, initializing member variables
 */
FOutputDeviceEventLog::FOutputDeviceEventLog(void) :
	EventLog(NULL)
{
	FString InstanceName;
	FString ServerName;
	// Build a name to uniquely identify this instance
	if (Parse(appCmdLine(),TEXT("-Login="),ServerName))
	{
		InstanceName = appGetGameName();
		InstanceName += ServerName;
	}
	else
	{
		DWORD ProcID = GetCurrentProcessId();
		InstanceName = FString::Printf(TEXT("%s-PID%d"),appGetGameName(),ProcID);
	}
	// Open the event log using the name built above
	EventLog = RegisterEventSource(NULL,*InstanceName);
	if (EventLog == NULL)
	{
		debugf(NAME_Error,TEXT("Failed to open the Windows Event Log for writing (%d)"),GetLastError());
	}
}

/** Destructor that cleans up any remaining resources */
FOutputDeviceEventLog::~FOutputDeviceEventLog(void)
{
	TearDown();
}

/**
 * Closes any event log handles that are open
 */
void FOutputDeviceEventLog::TearDown(void)
{
	if (EventLog != NULL)
	{
		DeregisterEventSource(EventLog);
		EventLog = NULL;
	}
}

/**
 * Writes a buffer to the event log
 *
 * @param Buffer the text to log
 * @param Event the FName attributed to the log entry
 */
void FOutputDeviceEventLog::Serialize(const TCHAR* Buffer,EName Event)
{
	if (EventLog != NULL)
	{
		// Only forward errors and warnings to the event log
		switch (Event)
		{
			case NAME_Critical:
			case NAME_Error:
			{
				ReportEvent(EventLog,
					EVENTLOG_ERROR_TYPE,
					NULL,
					0xC0000001L,
					NULL,
					1,
					0,
					&Buffer,
					NULL);
				break;
			}
			case NAME_Warning:
			{
				ReportEvent(EventLog,
					EVENTLOG_WARNING_TYPE,
					NULL,
					0x80000002L,
					NULL,
					1,
					0,
					&Buffer,
					NULL);
				break;
			}
		}
	}
}

#endif
