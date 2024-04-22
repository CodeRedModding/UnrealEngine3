/*=============================================================================
 	Android.cpp: Android main platform glue definitions
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "Core.h"
#include "FMallocAnsi.h"
#include "FFileManagerAndroid.h"
#include "UnIpDrv.h"
#include "ChartCreation.h"
#include "SystemSettings.h"

#include <android/log.h>
#include <jni.h>
#include <AndroidJNI.h>

#include "ES2RHIPrivate.h"

// From https://hg.mozilla.org/mozilla-central/rev/35fb1400f0f7
/*
 * To work around http://code.google.com/p/android/issues/detail?id=23203
 * we don't link with the crt objects. In some configurations, this means
 * a lack of the __dso_handle symbol because it is defined there, and
 * depending on the android platform and ndk versions used, it may or may
 * not be defined in libc.so. In the latter case, we fail to link. Defining
 * it here as weak makes us provide the symbol when it's not provided by
 * the crt objects, making the change transparent for future NDKs that
 * would fix the original problem. On older NDKs, it is not a problem
 * either because the way __dso_handle was used was already broken (and
 * the custom linker works around it).
 */
__attribute__((weak)) void *__dso_handle;

// Global for tracking the device model
FString		GAndroidDeviceModel = FString(TEXT(""));

// Global for performance level to enable/disable features
EAndroidPerformanceLevel GAndroidPerformanceLevel = ANDROID_Performance_1;

// Global for memory cutoff level to enable/disable features
EAndroidMemoryLevel GAndroidMemoryLevel = Android_Memory_Low;

// Global for resolution scale
FLOAT GAndroidResolutionScale = 1.0;

// Global for informing main thread of a feature level change
UBOOL GFeatureLevelChangeNeeded = FALSE;

// Global for informing engine of eglSurface recreation
UBOOL GEGLSurfaceRecreated = FALSE;

// The total device memory in bytes
UINT GAndroidDeviceMemory = 0;

void appOutputDebugString( const TCHAR *Message )
{
#if !NO_LOGGING
	__android_log_print(ANDROID_LOG_DEBUG, "UE3",  "%s", TCHAR_TO_ANSI(Message)); 
#endif
}

/** Sends a message to a remote tool. */
void appSendNotificationString( const ANSICHAR *Message )
{
    __android_log_print(ANDROID_LOG_DEBUG, "UE3",  "%s", TCHAR_TO_ANSI(Message)); 
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
    // we use gettimeofday() instead of rdtsc, so it's 1000000 "cycles" per second on this faked CPU.
	GSecondsPerCycle = 1.0f / 1000000.0f;
	return appSeconds();
}

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
	// wide printf routines want %ls instead of %s for wide string params. It assumes %s is a ANSICHAR*
	INT Result = vswprintf(Dest, Count, *FString(Fmt).Replace(TEXT("%s"), TEXT("%ls")), ArgPtr);
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

const TCHAR* appBaseDir() 
{
	return TEXT("");
}

const TCHAR* appShaderDir() 
{
	return TEXT("");
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

void appPlatformPreInit()
{
}

void appPlatformInit()
{
	appOutputDebugStringf("Configuring for %d hardware threads", GNumHardwareThreads);
}

void appPlatformPostInit()
{
}

void appHandleIOFailure( const TCHAR* Filename) 
{
	appOutputDebugStringf("I/O failure operating on '%s'", Filename ? Filename : "Unknown file");
}

FString GAndroidLocale(TEXT("en"));

/** Returns the language setting that is configured for the platform */
FString appGetLanguageExt(void)
{
	static FString UserLanguage;
	if (UserLanguage.Len() == 0)
	{	
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

		// now translate the Android local into a UE3 language
		for (INT LookupIndex = 0; LookupIndex < ARRAY_COUNT(LanguageRemap); LookupIndex += 2)
		{
			// if we match the first column, use the second column string
			if (appStrcmpANSI(LanguageRemap[LookupIndex], *GAndroidLocale) == 0)
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

void appProgramCounterToHumanReadableString( QWORD ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, EVerbosityFlags VerbosityFlags)
{
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

	// get the name of this device
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
	return TEXT("AndroidUser");
}

DWORD appCaptureStackBackTrace(unsigned long long*, unsigned long, unsigned long*)
{
	return 0;
}

void appBeginNamedEvent(FColor const&, TCHAR const*) 
{
}

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
	// load the latest UE3CommandLine.txt
	FString CmdLine;
	appLoadFileToString(CmdLine, *FString::Printf(TEXT("%s%s\\UE3CommandLine.txt"), *appGameDir(), TEXT("CookedAndroid")));

	// put it into the output
	appStrncpy(NewCommandLine, *CmdLine, 16384);

	return TRUE;
}

void appCleanFileCache()
{
	GSys->PerformPeriodicCacheCleanup();
}

void appUpdateMemoryChartStats(FMemoryChartEntry& MemoryEntry)
{
#if DO_CHARTING
	// TODO: track what memory info we have
#endif // DO_CHARTING
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
	SET_WARN_COLOR(COLOR_RED);
	warnf(TEXT("MESSAGE: %s"), TempStr);
	CLEAR_WARN_COLOR();
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
 * @param PriorityModifier		-2 idle, -1 low, 0 normal, 1 high, 2 higher
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

UBOOL appIsApplicationRunning( DWORD ProcessId )
{
	appErrorf(TEXT("appIsApplicationRunning not implemented."));
	return FALSE;
}


/*----------------------------------------------------------------------------
	Extras
 ----------------------------------------------------------------------------*/

/**
 * Return the system settings section name to use for the current running platform
 */
const TCHAR* appGetMobileSystemSettingsSectionName()
{
	static FString SectionName;
	SectionName = FString(TEXT("SystemSettingsAndroid"));

	switch (GAndroidPerformanceLevel)
	{
		case ANDROID_Performance_2:
			SectionName += TEXT("_Performance2");
			break;
		case ANDROID_Performance_1:
		default:
			SectionName += TEXT("_Performance1");
			break;
	}

	switch (GAndroidMemoryLevel)
	{
		case Android_Memory_1024:
			SectionName += TEXT("_Memory1024");
			break;
		case Android_Memory_Low:
		default:
			SectionName += TEXT("_MemoryLow");
			break;
	}

	return *SectionName;
}

/**
 * Sets up feature levels based on queried device metrics
 */ 
void appDetermineDeviceFeatureLevels()
{
	INT PerfLevel = CallJava_GetPerformanceLevel();
	
	if ( PerfLevel > -1 )
	{
		switch (PerfLevel)
		{
			case 0:
				GAndroidPerformanceLevel = ANDROID_Performance_1;
				break;
			case 1:
				GAndroidPerformanceLevel = ANDROID_Performance_2;
				break;
		}
	}
	else
	{
		// Currently, just determine performance level by GL_MAX_FRAGMENT_UNIFORM_VECTORS
		GLint MaxFragmentUniformVectors, MaxVertexUniformVectors;
		glGetError();	// Reset the error code.
		glGetIntegerv( GL_MAX_FRAGMENT_UNIFORM_VECTORS, &MaxFragmentUniformVectors );
		glGetIntegerv( GL_MAX_VERTEX_UNIFORM_VECTORS, &MaxVertexUniformVectors );
		
		if (glGetError() != GL_NO_ERROR || MaxFragmentUniformVectors < 256)
		{
			GAndroidPerformanceLevel = ANDROID_Performance_1;
		}
		else
		{
			GAndroidPerformanceLevel = ANDROID_Performance_2;
		}

		// Override Razr i to Performance 2 (no good way to identify atom processor as superior, and GPU is still only an SGX 540)
		if (appStrcmp(*GAndroidDeviceModel, TEXT("XT890")) == 0)
		{
			GAndroidPerformanceLevel = ANDROID_Performance_2;
		}
	}

	// Determine memory feature level
	UINT AndroidDeviceMB = GAndroidDeviceMemory / 1024 / 1024;
	if (AndroidDeviceMB > 1024)
	{
		GAndroidMemoryLevel = Android_Memory_1024;
	}
	else
	{
		GAndroidMemoryLevel = Android_Memory_Low;
	}

	// Check for Resolution Scale override
	GAndroidResolutionScale = CallJava_GetResolutionScale();
}

/*
 * Rechecks feature levels and adjusts accordingly 
 */
void appHandleFeatureLevelChange(int PerformanceLevel, float ResolutionScale)
{
	// Identify android feature levels
	if (GAndroidPerformanceLevel != (EAndroidPerformanceLevel) PerformanceLevel || GAndroidResolutionScale != ResolutionScale)
	{
		GAndroidPerformanceLevel = (EAndroidPerformanceLevel) PerformanceLevel;
		GAndroidResolutionScale = ResolutionScale;

		GFeatureLevelChangeNeeded = TRUE;
	}
}

void appUpdateFeatureLevelChangeFromMainThread()
{
	check(IsInGameThread());

	if (!GFeatureLevelChangeNeeded)
	{
		// Hide reloader if its visible
		CallJava_HideReloader();

		return;
	}
	GFeatureLevelChangeNeeded = FALSE;

	FlushRenderingCommands();
	GEngine->Exec(TEXT("SCALE RESET"));

	// Manually recompile ES2 shaders
	FlushRenderingCommands();
	ENQUEUE_UNIQUE_RENDER_COMMAND (
		RecompileCommand,
	{
		// Explicitly reset some settings not handled by SCALE RESET
#if WITH_ES2_RHI
		GShaderManager.ResetPlatformFeatures();
#endif

		// Apply resolution scale
		GSystemSettings.ScreenPercentage = GAndroidResolutionScale * 100.0f;

		// Update render targets
		FSystemSettings::UpdateSceneRenderTargetsRHI();

		// Recompile all shaders
#if WITH_ES2_RHI
		extern void RecompileES2Shaders();
		RecompileES2Shaders();
#endif
	});
	FlushRenderingCommands();
	
	// Hide reloader if its visible
	CallJava_HideReloader();
}

void appRecompilePreprocessedShaders()
{
	check(IsInGameThread());

	// Manually recompile ES2 shaders
	FlushRenderingCommands();
	ENQUEUE_UNIQUE_RENDER_COMMAND (
		RecompileCommand,
	{
#if WITH_ES2_RHI
		extern void RecompileES2Shaders();
		RecompileES2Shaders();
#endif
	});
	FlushRenderingCommands();
}

/**
 * Super early Android initialization
 */
void appAndroidInit(int argc, char* argv[])
{
	// Get the device model
	GAndroidDeviceModel = CallJava_GetDeviceModel();

	// Identify android feature levels
	appDetermineDeviceFeatureLevels();

	FFileManagerAndroid::StaticInit();
	
	extern TCHAR GCmdLine[16384];
	GCmdLine[0] = 0;

	// read in the command line text file (coming from UnrealFrontend) if it exists
	FString CookedDir = TEXT("CookedAndroid");
	// make sure GGameName is set
	extern void appSetGameName();
	appSetGameName();

	appStrcpy(GCmdLine, *CallJava_GetAppCommandLine());

	// append any commandline options coming from command line
	for (INT Option = 1; Option < argc; Option++)
	{
		appStrcat(GCmdLine, TEXT(" "));
		appStrcat(GCmdLine, ANSI_TO_TCHAR(argv[Option]));
	}

	appOutputDebugStringf(TEXT("Combined Android Commandline: %s\n"), GCmdLine);
}


const TCHAR* appGetAndroidPhoneHomeURL()
{
	return PHONE_HOME_URL;
}


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


// wide string madness

bool iswspace( TCHAR c ) {
	return isspace( char( c ) );
}

bool iswpunct( TCHAR c ) {
	return ispunct( char( c ) );
}

TCHAR towupper( TCHAR c ) {
	return toupper( c );
}

TCHAR * wide_upr( TCHAR *str ) {
	TCHAR *s = str;
	while( *s ) {
		*s = toupper( *s );
		s++;
	}
	return str;
}

TCHAR *wide_cpy( TCHAR *dst, const TCHAR *src ) {
	return strcpy( dst, src );
}

TCHAR *wide_ncpy( TCHAR *dst, const TCHAR *src, int n ) {
	return strncpy( dst, src, n );
}

size_t wide_len( const TCHAR *dst ) {
	return strlen( dst );
}

TCHAR *wide_cat( TCHAR *dst, const TCHAR *src ) {
	return strcat( dst, src );
}

TCHAR *wide_chr( const TCHAR *s, TCHAR c ) {
	return strchr( s, c );
}

TCHAR *wide_rchr( const TCHAR *s, TCHAR c ) {
	return strrchr( s, c );
}

TCHAR *wide_str( const TCHAR *big, const TCHAR *little ) {
	return strstr( big, little );
}

int wide_cmp( const TCHAR *a, const TCHAR *b ) {
	return strcmp( a, b );
}

int wide_ncmp( const TCHAR *a, const TCHAR *b, int n ) {
	return strncmp( a, b, n );
}

int wide_toul( const TCHAR * wstr, TCHAR **end, int base ) {
	return strtoul( wstr, end, base );
}

unsigned long long wide_toull( const TCHAR * wstr, TCHAR **end, int base ) {
	return strtoull( wstr, end, base );
}

double wide_tod( const TCHAR * wstr ) {
	return strtod( wstr, 0 );
}

// This won't handle TCHAR * replacement; need to scan the fmt for %s and do surgery on
// the arg list. Ugh.
int vswprintf( TCHAR *dst, int count, const TCHAR *fmt, va_list arg ) {
	return vsnprintf( dst, count, fmt, arg );
}

// This won't handle TCHAR * replacement; need to scan the fmt for %s and do surgery on
// the arg list. Ugh.
int wprintf( const TCHAR *fmt, ... ) {
	va_list args;
	va_start( args, fmt );
	int ret = vprintf( fmt, args );
	va_end( args );
	return ret;
}

int swscanf( const TCHAR *buffer, const TCHAR *fmt, ... ) {
	va_list args;
	va_start( args, fmt );
	int ret = vsscanf( buffer, fmt, args );
	va_end( args );
	return ret;
}
