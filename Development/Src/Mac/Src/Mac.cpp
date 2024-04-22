/*=============================================================================
	Mac.mm: Unreal main platform glue definitions
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Core.h"
#include "UnIpDrv.h"
#include "ChartCreation.h"
#include "MacObjCWrapper.h"

#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_host.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysctl.h>

/** Save the commandline, so we can use it when calling GuardedMain */
FString GSavedCommandLine;

void appOutputDebugString( const TCHAR *Message )
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	printf("%s", TCHAR_TO_ANSI(Message));
#endif
#if WITH_UE3_NETWORKING
	if (GDebugChannel)
	{
		GDebugChannel->SendText(Message);
	}
#endif
}

/** Sends a message to a remote tool. */
void appSendNotificationString( const ANSICHAR *Message )
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	printf("%s", Message);
#endif
#if WITH_UE3_NETWORKING
	if (GDebugChannel)
	{
		GDebugChannel->SendText(ANSI_TO_TCHAR(Message));
	}
#endif
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
		
//
// Return the system time.
//
void appSystemTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	// query for calendar time
    struct timeval Time;
    gettimeofday(&Time, NULL);

	// convert it to local time
    struct tm LocalTime;
    localtime_r(&Time.tv_sec, &LocalTime);
	
	// pull out data/time
	Year		= LocalTime.tm_year + 1900;
	Month		= LocalTime.tm_mon + 1;
	DayOfWeek	= LocalTime.tm_wday;
	Day			= LocalTime.tm_mday;
	Hour		= LocalTime.tm_hour;
	Min			= LocalTime.tm_min;
	Sec			= LocalTime.tm_sec;
	MSec		= Time.tv_usec / 1000;
}

//
// Return the UTC time.
//
void appUtcTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	// query for calendar time
	struct timeval Time;
	gettimeofday(&Time, NULL);

	// convert it to UTC
	struct tm LocalTime;
	gmtime_r(&Time.tv_sec, &LocalTime);

	// pull out data/time
	Year		= LocalTime.tm_year + 1900;
	Month		= LocalTime.tm_mon + 1;
	DayOfWeek	= LocalTime.tm_wday;
	Day			= LocalTime.tm_mday;
	Hour		= LocalTime.tm_hour;
	Min			= LocalTime.tm_min;
	Sec			= LocalTime.tm_sec;
	MSec		= Time.tv_usec / 1000;
}

/**
 * Does per platform initialization of timing information and returns the current time. This function is
 * called before the execution of main as GStartTime is statically initialized by it. The function also
 * internally sets GSecondsPerCycle, which is safe to do as static initialization order enforces complex
 * initialization after the initial 0 initialization of the value.
 *
 * @return	current time
 */
DOUBLE appInitTiming()
{
	// Time base is in nano seconds.
	mach_timebase_info_data_t Info;
	verify( mach_timebase_info( &Info ) == 0 );
	GSecondsPerCycle = 1e-9 * (DOUBLE) Info.numer / (DOUBLE) Info.denom;
	return appSeconds();
}

#if TCHAR_IS_4_BYTES
/**
 * Wide stricmp replacement
 */
int wgccstrcasecmp(const TCHAR *StrA, const TCHAR *StrB)
{
	// walk the strings, comparing them case insensitively
	for (; *StrA || *StrB; StrA++, StrB++)
	{
		TCHAR A = towupper(*StrA), B = towupper(*StrB);
		if (A != B)
		{
			return A - B;
		}
	}
	return 0;
}

/**
 * Wide strnicmp replacement
 */
int wgccstrncasecmp(const TCHAR *StrA, const TCHAR *StrB, size_t Size)
{
	// walk the strings, comparing them case insensitively, up to a max size
	for (; (*StrA || *StrB) && Size; StrA++, StrB++, Size--)
	{
		TCHAR A = towupper(*StrA), B = towupper(*StrB);
		if (A != B)
		{
			return A - B;
		}
	}
	return 0;
}
#endif

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
#if UNICODE
#if !TCHAR_IS_4_BYTES
	// CFStringCreateWithFormatAndArguments wants %S instead of %s for UTF16 string params. It assumes %s is a ANSICHAR*
	CFStringRef CFFormat = CFStringCreateWithCharacters(NULL, (const UniChar *)*FString(Fmt).Replace(TEXT("%s"), TEXT("%S")), _tcslen(Fmt));
	CFStringRef CFString = CFStringCreateWithFormatAndArguments(NULL, NULL, CFFormat, ArgPtr);
	INT Result = CFStringGetLength(CFString);
	if (Result <= Count)
	{
		CFStringGetCharacters(CFString, CFRangeMake(0, Result), (UniChar *)Dest);
	}
	else
	{
		Result = -1;
	}
	CFRelease(CFFormat);
	CFRelease(CFString);
#else
	// wide printf routines want %ls instead of %s for wide string params. It assumes %s is a ANSICHAR*
	INT Result = vswprintf(Dest, Count, *FString(Fmt).Replace(TEXT("%s"), TEXT("%ls")), ArgPtr);
#endif // TCHAR_IS_4_BYTES
#else
	INT Result = vsnprintf(Dest,Count,Fmt,ArgPtr);
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
	INT Result = vsnprintf(Dest,Count,Fmt,ArgPtr);
	va_end( ArgPtr );
	return Result;
}



void appSleep( FLOAT Seconds )
{
	const INT usec = appTrunc(Seconds * 1000000.0f);
	if (usec > 0)
	{
		usleep(usec);
	}
	else
	{
		sched_yield();
	}
}

void appSleepInfinite()
{
	// stop this thread forever
	pause();
}

void appOnFailSHAVerification(const TCHAR* FailedPathname, UBOOL bFailedDueToMissingHash)
{
}

static TCHAR CachedBaseDir[2048] = TEXT("");
static FString CachedRootDir;

static TCHAR CachedAppDir[2048] = TEXT("");
static TCHAR CachedAppName[512] = TEXT("");

static TCHAR CachedMacBinaryDir[2048] = TEXT("");
static TCHAR CachedMacBinaryName[512] = TEXT("");

/**
 * Takes the path of the .app resources, .app itself, and the path of the binary within the .app, from the Obj-C launcher
 *
 * @param ResourcePath		The absolute path of the .app resources
 * @param AppPathAndName	The absolute path of the .app
 * @param BinaryPathAndName	The absolute path of the binary within the .app
 * @param bResourcesBasePath	Whether or not the game content resides within ResourcePath
 */
void appMacSaveAppPaths(const char* ResourcesPath, const char* AppPathAndName, const char* BinaryPathAndName, bool bResourcesBasePath)
{
	FString FullResourcesPath = ANSI_TO_TCHAR(ResourcesPath);
	const FString FullAppPath = ANSI_TO_TCHAR(AppPathAndName);
	const FString FullBinaryPath = ANSI_TO_TCHAR(BinaryPathAndName);

	// Make sure the resource path is capped off
	if (!FullResourcesPath.IsEmpty() && !FullResourcesPath.EndsWith(PATH_SEPARATOR))
	{
		FullResourcesPath += PATH_SEPARATOR;
	}


	// Split the app name/path
	INT AppDirDelim = FullAppPath.InStr(PATH_SEPARATOR, TRUE);
	FString AppDir;

	if (AppDirDelim != INDEX_NONE)
	{
		AppDir = *FullAppPath.Left(AppDirDelim+1);

		appStrcpy(CachedAppDir, 2048, *FullAppPath.Left(AppDirDelim+1));
		appStrcpy(CachedAppName, 512, *FullAppPath.Mid(AppDirDelim+1));
	}
	else
	{
		// Need to use appMsgf instead of appErrorf, as this is too early for appErrorf logging
		appMsgf(AMT_OK, TEXT("Failed to parse Mac app path and name"));
		exit(1);
		return;
	}


	// Set the root directory
	FString RootPath;

	if (bResourcesBasePath)
	{
		RootPath = FullResourcesPath;
	}
	else if (!AppDir.IsEmpty())
	{
		INT BinDelim = AppDir.InStr(TEXT("Binaries"), TRUE, TRUE);

		if (BinDelim != INDEX_NONE)
		{
			RootPath = AppDir.Left(BinDelim);
		}
	}

	if (!RootPath.IsEmpty())
	{
		CachedRootDir = RootPath;
	}
	else
	{
		appMsgf(AMT_OK, TEXT("Failed to parse Mac root directory"));
		exit(1);
		return;
	}


	// Set the base directory
	FString BasePath;

	if (!RootPath.IsEmpty())
	{
		// For Mac, the base directory has to (unfortunately) start within Engine\Config, because the directory structure for .app's
		// produced by UnrealFrontend, does not have any Binaries directory (and *Game is in a completely separate 'resource' directory)
		BasePath = FString::Printf(TEXT("%s%s"), *RootPath, TEXT("Engine") PATH_SEPARATOR TEXT("Config") PATH_SEPARATOR);
		appStrcpy(CachedBaseDir, 2048, *BasePath);
	}
	else
	{
		appMsgf(AMT_OK, TEXT("Failed to parse Mac base directory"));
		exit(1);
		return;
	}


	// Split the binary name/path
	INT BinaryDirDelim = FullBinaryPath.InStr(PATH_SEPARATOR, TRUE);

	if (BinaryDirDelim != INDEX_NONE)
	{
		appStrcpy(CachedMacBinaryDir, 2048, *FullBinaryPath.Left(BinaryDirDelim+1));
		appStrcpy(CachedMacBinaryName, 512, *FullBinaryPath.Mid(BinaryDirDelim+1));
	}
	else
	{
		appMsgf(AMT_OK, TEXT("Failed to parse Mac binary path and name"));
		exit(1);
		return;
	}
}

// NOTE: For Mac, this always points to Engine\Config, as the standard UE3 directory structure can not be supported for Mac .app bundles
const TCHAR* appBaseDir()
{
	return CachedBaseDir;
}

FString appRootDir()
{
	return CachedRootDir;
}

// NOTE: For Mac, this returns the .app name, not the binary name
const TCHAR* appExecutableName()
{
	return CachedAppName;
}

/** Returns the directory of the Mac .app */
const TCHAR* appMacAppDir()
{
	return CachedAppDir;
}

/** Returns the directory of the Mac binary (within the .app) */
const TCHAR* appMacBinaryDir()
{
	return CachedMacBinaryDir;
}

/** Returns the name of the current running Mac binary */
const TCHAR* appMacBinaryName()
{
	return CachedMacBinaryName;
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

void appRequestExit( UBOOL bForceExit )
{
	debugf( TEXT("appRequestExit(%i)"), bForceExit );
	if( bForceExit )
	{
		// Abort allows signal handler to know we aborted.
		abort();
	}
	else
	{
		// Tell the platform specific code we want to exit cleanly from the main loop.
		GIsRequestingExit = 1;
	}
}

// This version is just a link to Cocoa code so it can call this method
void MacAppRequestExit( int ForceExit )
{
	appRequestExit( ForceExit?TRUE:FALSE );
}

void appPlatformPreInit()
{
}

void appPlatformInit()
{
	int NumCPUs = 1;
	size_t Size = sizeof(int);
	if (sysctlbyname("hw.ncpu", &NumCPUs, &Size, NULL, 0) == 0)
	{
		GNumHardwareThreads = NumCPUs;
	}
	else
	{
		GNumHardwareThreads = 1;
	}

	GMacOSXVersion = MacGetOSXVersion();
}

void appPlatformPostInit()
{
}

void appHandleIOFailure( const TCHAR* ) 
{
}

/** Returns the language setting that is configured for the platform */
FString appGetLanguageExt(void)
{
	static FString UserLanguage;
	if (UserLanguage.Len() == 0)
	{
		// ask Obj-C lang for the user's language
		ANSICHAR ObjCLanguage[128];
		MacGetUserLanguage(ObjCLanguage, ARRAY_COUNT(ObjCLanguage));

		// lookup table for two letter code returned by iOS to UE3 extension
		static const ANSICHAR* LanguageRemap[] = 
		{
			"en", "INT",
			"ja", "JPN",
			"de", "DEU",
			"fr", "FRA",
			"es", "ESN",
			"??", "ESM",
			"it", "ITA",
			"ko", "KOR",
			"zh", "CHN",
			"ru", "RUS",
			"pl", "POL",
			"hu", "HUN",
			"cs", "CZE",
			"sk", "SLO",
		};

		// now translate the Apple language into a UE3 language
		for (INT LookupIndex = 0; LookupIndex < ARRAY_COUNT(LanguageRemap); LookupIndex += 2)
		{
			// if we match the first column, use the second column string
			if (appStrcmpANSI(LanguageRemap[LookupIndex], ObjCLanguage) == 0)
			{
				UserLanguage = LanguageRemap[LookupIndex + 1];
				break;
			}
		}

		// default to INT if the language wasn't found
		if (UserLanguage.Len() == 0)
		{
			UserLanguage = TEXT("INT");
		}

		// Allow for overriding the language settings via the commandline
		FString CmdLineLang;
		if (Parse(appCmdLine(), TEXT("LANGUAGE="), CmdLineLang))
		{
			warnf(NAME_Log, TEXT("Overriding lang %s w/ command-line option of %s"), *UserLanguage, *CmdLineLang);
			UserLanguage = CmdLineLang;
		}
		UserLanguage = UserLanguage.ToUpper();

		// make sure the language is one that is known (GKnownLanguages)
		if (appIsKnownLanguageExt(UserLanguage) == FALSE)
		{
			// default back to INT if the language wasn't known
			warnf(NAME_Warning, TEXT("Unknown language extension %s. Defaulting to INT"), *UserLanguage);
			UserLanguage = TEXT("INT");
		}
	}

	return UserLanguage;
}


// Create a new globally unique identifier.
FGuid appCreateGuid()
{
	INT Year=0, Month=0, DayOfWeek=0, Day=0, Hour=0, Min=0, Sec=0, MSec=0;
	appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );
	
	FGuid   GUID;
	GUID.A = Day | (Hour << 16);
	GUID.B = Month | (Sec << 16);
	GUID.C = MSec | (Min << 16);
	GUID.D = Year ^ appCycles();
	return GUID;
}

// Get computer name.
const TCHAR* appComputerName()
{
	static TCHAR CompName[256] = { 0 };

	// get the name of this iphone
	if (CompName[0] == 0)
	{
		ANSICHAR AnsiCompName[ARRAY_COUNT(CompName)];
		gethostname(AnsiCompName, ARRAY_COUNT(CompName));
		appStrcpy(CompName, ANSI_TO_TCHAR(AnsiCompName));
	}

	return CompName;
}

// Get user name.
const TCHAR* appUserName()
{
	return TEXT("MacUser");
}

/**
 * Converts the passed in program counter address to a human readable string and appends it to the passed in one.
 * @warning: The code assumes that HumanReadableString is large enough to contain the information.
 *
 * @param	ProgramCounter			Address to look symbol information up for
 * @param	HumanReadableString		String to concatenate information with
 * @param	HumanReadableStringSize size of string in characters
 * @param	VerbosityFlags			Bit field of requested data for output. -1 for all output.
 */ 
void appProgramCounterToHumanReadableString( QWORD ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, EVerbosityFlags VerbosityFlags )
{
}

/**
 * Capture a stack backtrace and optionally use the passed in exception pointers.
 *
 * @param	BackTrace			[out] Pointer to array to take backtrace
 * @param	MaxDepth			Entries in BackTrace array
 * @return	Number of function pointers captured
 */
DWORD appCaptureStackBackTrace( QWORD* BackTrace, DWORD MaxDepth )
{
	void* OsBackTrace[128];
	MaxDepth = Min<DWORD>( ARRAY_COUNT(OsBackTrace), MaxDepth );
	INT CapturedDepth = backtrace(OsBackTrace, MaxDepth);

	// Convert 32 bit pointers 64 bit QWORD
	for( INT i=0; i<CapturedDepth; i++ )
	{
		BackTrace[i] = (QWORD) OsBackTrace[i];
	}

	return CapturedDepth;
}

/**
 * Returns the number of modules loaded by the currently running process.
 */
INT appGetProcessModuleCount()
{
	return 0;
}

/**
 * Gets the signature for every module loaded by the currently running process.
 *
 * @param	ModuleSignatures		An array to retrieve the module signatures.
 * @param	ModuleSignaturesSize	The size of the array pointed to by ModuleSignatures.
 *
 * @return	The number of modules copied into ModuleSignatures
 */
INT appGetProcessModuleSignatures(FModuleInfo *ModuleSignatures, const INT ModuleSignaturesSize)
{
	return 0;
}

void appBeginNamedEvent(FColor const&, wchar_t const*) 
{
}

void appEndNamedEvent() 
{
}

void appCleanFileCache()
{
	GSys->PerformPeriodicCacheCleanup();
}

void appUpdateMemoryChartStats(FMemoryChartEntry& MemoryEntry)
{
#if DO_CHARTING
	// track what memory info we hav

	vm_statistics_t Stats;
	mach_msg_type_number_t Size = sizeof(Stats);
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&Stats, &Size);

	vm_size_t PageSize;
	host_page_size(mach_host_self(), &PageSize);

	uint64_t FreeMem = Stats->free_count * PageSize;
	uint64_t UsedMem = (Stats->active_count + Stats->inactive_count + Stats->wire_count) * PageSize;

	MemoryEntry.PhysicalMemUsed = UsedMem;
	MemoryEntry.PhysicalTotal = FreeMem + UsedMem;
#endif // DO_CHARTING
}

extern wchar_t* GMacSplashBitmapPath;
extern wchar_t* GMacSplashStartupProgress;
extern wchar_t* GMacSplashVersionInfo;
extern wchar_t* GMacSplashCopyrightInfo;

extern void appShowSplashMac( void );
extern void appHideSplashMac( void );

/**
 * Displays a splash window with the specified image.  The function does not use wxWidgets.  The splash
 * screen variables are stored in static global variables (SplashScreen*).  If the image file doesn't exist,
 * the function does nothing.
 *
 * @param	InSplashName	Name of splash screen file (and relative path if needed)
 */
void appShowSplash(const TCHAR* InSplashName)
{
	if (ParseParam(appCmdLine(),TEXT("NOSPLASH")) == TRUE)
		return;

	// make sure a splash was found
	FString SplashPath;
	if (appGetSplashPath(InSplashName, SplashPath) != TRUE)
		return;

	check( !GMacSplashBitmapPath );

	// Get all interesting info and save it in local variables
	{
		const FString Filepath = FString( appBaseDir() ) * SplashPath;
		GMacSplashBitmapPath = (TCHAR*) appMalloc(MAX_PATH*sizeof(TCHAR));
		appStrncpy(GMacSplashBitmapPath, *Filepath, MAX_PATH);

		// In the editor, we'll display loading info
		if( GIsEditor )
		{
			const TCHAR* StartupProgressText = *LocalizeUnrealEd( TEXT( "SplashScreen_InitialStartupProgress" ) );
			int StartupProgressTextLength = _tcslen( StartupProgressText ) + 1;
			GMacSplashStartupProgress = (TCHAR*) appMalloc(StartupProgressTextLength*sizeof(TCHAR));
			appStrcpy(GMacSplashStartupProgress, StartupProgressTextLength, StartupProgressText);

			// Set version info
			{
#ifdef __LP64__
				const FString PlatformBitsString( TEXT( "64" ) );
#else
				const FString PlatformBitsString( TEXT( "32" ) );
#endif

#if UDK
				const FString AppName = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "UDKTitle_F" ), *PlatformBitsString ) );
#else
				const FString GameName = GConfig->GetStr(TEXT("URL"), TEXT("GameName"), GEngineIni);
				const FString AppName = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "UnrealEdTitle_F" ), *GameName, *PlatformBitsString ) );
#endif
				const FString VersionInfo1 = FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "SplashScreen_VersionInfo1_F" ) ), *AppName, GEngineVersion, GBuiltFromChangeList ) );

				int VersionInfoTextLength = _tcslen( *VersionInfo1 ) + 1;
				GMacSplashVersionInfo = (TCHAR*) appMalloc(VersionInfoTextLength*sizeof(TCHAR));
				appStrcpy(GMacSplashVersionInfo, VersionInfoTextLength, *VersionInfo1);
			}
		}
		
		// Display copyright information in both editor and game splash screens
		{
			const FString CopyrightInfo = LocalizeGeneral( TEXT( "SplashScreen_CopyrightInfo" ), TEXT("Engine") );
			int CopyrightInfoTextLength = _tcslen( *CopyrightInfo ) + 1;
			GMacSplashCopyrightInfo = (TCHAR*) appMalloc(CopyrightInfoTextLength*sizeof(TCHAR));
			appStrcpy(GMacSplashCopyrightInfo, CopyrightInfoTextLength, *CopyrightInfo);
		}
	}

	appShowSplashMac();
}

/**
 * Destroys the splash window that was previously shown by appShowSplash(). If the splash screen isn't active,
 * it will do nothing.
 */
void appHideSplash()
{
	appHideSplashMac();

	if( GMacSplashBitmapPath )
	{
		appFree( GMacSplashBitmapPath );
		GMacSplashBitmapPath = 0;
	}

	if( GMacSplashStartupProgress )
	{
		appFree( GMacSplashStartupProgress );
		GMacSplashStartupProgress = 0;
	}

	if( GMacSplashVersionInfo )
	{
		appFree( GMacSplashVersionInfo );
		GMacSplashVersionInfo = 0;
	}

	if( GMacSplashCopyrightInfo )
	{
		appFree( GMacSplashCopyrightInfo );
		GMacSplashCopyrightInfo = 0;
	}
}

/*-----------------------------------------------------------------------------
 Unimplemented dummies.
 -----------------------------------------------------------------------------*/

void appClipboardCopy( const TCHAR* Str )
{
}
FString appClipboardPaste()
{
	return TEXT("");
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
	check(OutBuffer && BufferCount >= MAX_SPRINTF);
	appSprintf(OutBuffer,TEXT("appGetSystemErrorMessage not supported.  Error: %d"),Error);
	warnf(OutBuffer);
	return OutBuffer;
}

void* appGetDllHandle( const TCHAR* Filename )
{
	return NULL;
}
void appFreeDllHandle( void* DllHandle )
{
}
void* appGetDllExport( void* DllHandle, const TCHAR* ProcName )
{
	return NULL;
}
void appDebugMessagef( const TCHAR* Fmt, ... )
{
	appErrorf(TEXT("Not supported"));
}
VARARG_BODY( UBOOL, appMsgf, const TCHAR*, VARARG_EXTRA(EAppMsgType Type) )
{
	TCHAR TempStr[4096]=TEXT("");
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );

	switch (Type)
	{
		case AMT_OK:
			return MacShowBlockingAlert("Message", TCHAR_TO_ANSI(TempStr), "OK");
		case AMT_YesNo:
			return MacShowBlockingAlert("Message", TCHAR_TO_ANSI(TempStr), "No", "Yes");
		case AMT_OKCancel:
			return MacShowBlockingAlert("Message", TCHAR_TO_ANSI(TempStr), "Cancel", "OK");
		case AMT_YesNoCancel:
			return MacShowBlockingAlert("Message", TCHAR_TO_ANSI(TempStr), "No", "Yes", "Cancel");
		default:
			warnf(TEXT("Unsupported appMsgf type [%d]: %s"), TempStr);
	}

	return 1;
}
void appGetLastError( void )
{
	appErrorf(TEXT("Not supported"));
}
void EdClearLoadErrors()
{
	appErrorf(TEXT("Not supported"));
}
VARARG_BODY( void VARARGS, EdLoadErrorf, const TCHAR*, VARARG_EXTRA(INT Type) )
{
	appErrorf(TEXT("Not supported"));
}
void appLaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error )
{
	appErrorf(TEXT("appLaunchURL not supported"));
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
 * @return	The process handle for use in other process functions
 */
void *appCreateProc( const TCHAR* URL, const TCHAR* Parms, UBOOL bLaunchDetached, UBOOL bLaunchHidden, UBOOL bLaunchReallyHidden, DWORD* OutProcessID, INT PriorityModifier )
{
	appErrorf(TEXT("appCreateProc not supported"));
	return NULL;
}

/** Returns TRUE if the specified process is running 
*
* @param ProcessHandle handle returned from appCreateProc
* @return TRUE if the process is still running
*/
UBOOL appIsProcRunning( void* )
{
	appErrorf(TEXT("appIsProcRunning not supported"));
	return FALSE;
}

/** Waits for a process to stop
*
* @param ProcessHandle handle returned from appCreateProc
*/
void appWaitForProc( void* )
{
	appErrorf(TEXT("appWaitForProc not supported"));
}

/** Terminates a process
*
* @param ProcessHandle handle returned from appCreateProc
*/
void appTerminateProc( void* )
{
	appErrorf(TEXT("appTerminateProc not supported"));
}

/** Retrieves the ProcessId of this process
*
* @return the ProcessId of this process
*/
DWORD appGetCurrentProcessId()
{
	appErrorf(TEXT("appGetCurrentProcessId not supported"));
	return 0;
}

UBOOL appGetProcReturnCode( void* ProcHandle, INT* ReturnCode )
{
	appErrorf(TEXT("appGetProcReturnCode not supported"));
	return false;
}
/** Returns TRUE if the specified application is running */
UBOOL appIsApplicationRunning( DWORD ProcessId )
{
	appErrorf(TEXT("appIsApplicationRunning not implemented."));
	return FALSE;
}

void appMacSaveCommandLine(int argc, char* argv[])
{
	GSavedCommandLine.Empty();

	for (INT Option = 1; Option < argc; Option++)
	{
		GSavedCommandLine += TEXT(" ");
		GSavedCommandLine += ANSI_TO_TCHAR(argv[Option]);
	}
}

extern INT GuardedMain(const TCHAR *CmdLine, HINSTANCE hInInstance, HINSTANCE hPrevInstance, INT nCmdShow);

int appMacCallGuardedMain()
{
	return GuardedMain(*GSavedCommandLine, NULL, NULL, 0);
}

#if !USE_NULL_RHI
void appBeginDrawEvent(const FColor& Color,const TCHAR* Text)
{
}

/**
 * Ends the current PIX event
 */
void appEndDrawEvent(void)
{
}

/**
 * Platform specific function for setting the value of a counter that can be
 * viewed in PIX.
 */
void appSetCounterValue(const TCHAR* CounterName, FLOAT Value)
{
}
#endif

/**
 * Prevents screen-saver from kicking in by moving the mouse by 0 pixels. This works even on
 * Vista in the presence of a group policy for password protected screen saver.
 */
void appPreventScreenSaver()
{
	MacIssueMouseEventDoingNothing();
}

/*
 *	Shows the intial game window in the proper position and size.
 *	This function doesn't have any effect if called a second time.
 */
void appShowGameWindow()
{
	// @todo mac: currently window is created already visible
}

/** Returns TRUE if the directory exists */
UBOOL appDirectoryExists( const TCHAR* DirectoryName )
{
	FString AbsolutePath = appConvertRelativePathToFull( DirectoryName );
	struct stat Stat;
	return (stat(TCHAR_TO_ANSI(*AbsolutePath), &Stat) == 0);
}

extern bool MacIsEULAAccepted(const char *RTFPath);
UBOOL IsEULAAccepted(void)
{
	FString LangExt(appGetLanguageExt());
	FString RTFPath = appGameDir() + TEXT("../Engine/Localization/EULA.UDK.") + LangExt + TEXT(".rtf");
	FString AbsolutePath = appConvertRelativePathToFull(RTFPath);
	return MacIsEULAAccepted(TCHAR_TO_ANSI(*AbsolutePath));
}

/*----------------------------------------------------------------------------
	Extras
 ----------------------------------------------------------------------------*/

/**
 * Allow debugger to show FStrings
 */
const char* __FString(const FString& Input)
{
	return TCHAR_TO_ANSI(*Input);
}

/**
 * Allow debugger to show TCHAR* strings
 */
const char* __Str(TCHAR* Input)
{
	return TCHAR_TO_ANSI(Input);
}

/**
 * Allow debugger to show FNames
 */
const char* __FName(FName Input)
{
	return TCHAR_TO_ANSI(*Input.ToString());
}

/**
 * Allow debugger to show UObject names
 */
const char* __Obj(const UObject* Input)
{
	return TCHAR_TO_ANSI(*Input->GetFullName());
}
