/*=============================================================================
	UnFile.h: General-purpose file utilities.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNFILE_H__
#define __UNFILE_H__

#if USE_SECURE_CRT
#pragma warning(default : 4996)	// enable deprecation warnings
#endif

/*-----------------------------------------------------------------------------
	Structures
-----------------------------------------------------------------------------*/

/**
* This is used to capture all of the module information needed to load pdb's.
*/
struct FModuleInfo
{
	QWORD BaseOfImage;
	DWORD ImageSize;
	DWORD TimeDateStamp;
	TCHAR ModuleName[32];
	TCHAR ImageName[256];
	TCHAR LoadedImageName[256];
	DWORD PdbSig;
	DWORD PdbAge;
#if defined(GUID_DEFINED)
	GUID PdbSig70;
#else
	struct
	{
		unsigned long  Data1;
		unsigned short Data2;
		unsigned short Data3;
		unsigned char  Data4[8];
	} PdbSig70;
#endif
};

/*-----------------------------------------------------------------------------
	Defines.
-----------------------------------------------------------------------------*/

// Might be overridde on a per platform basis.
#ifndef DVD_ECC_BLOCK_SIZE
/** Default DVD sector size in bytes.						*/
#define DVD_SECTOR_SIZE		(2048)
/** Default DVD ECC block size for read offset alignment.	*/
#define DVD_ECC_BLOCK_SIZE	(32 * 1024)
/** Default minimum read size for read size alignment.		*/
#define DVD_MIN_READ_SIZE	(128 * 1024)
#endif

#if PS3
// Immediate exit if requested
#define SHUTDOWN_IF_EXIT_REQUESTED \
	if (GIsRequestingExit) \
	{ \
		appPS3QuitToSystem(); \
	}

// Simply return from function if we need to shutdown.
#define RETURN_IF_EXIT_REQUESTED \
	if (GIsRequestingExit) \
	{ \
		return; \
	}

// Simply return from function if we need to shutdown.
#define RETURN_VAL_IF_EXIT_REQUESTED(x) \
	if (GIsRequestingExit) \
	{ \
		return x; \
	}

#else
#define SHUTDOWN_IF_EXIT_REQUESTED
#define RETURN_IF_EXIT_REQUESTED
#define RETURN_VAL_IF_EXIT_REQUESTED(x)
#endif

#if PS3
	#define MAX_FILEPATH_LENGTH		CELL_FS_MAX_FS_PATH_LENGTH
#elif NGP
	#define MAX_FILEPATH_LENGTH		255 // from docs, couldn't find a #define
#elif WIIU
	#define MAX_FILEPATH_LENGTH		FSA_MAX_LOCALPATH_SIZE
#elif XBOX
	#define MAX_FILEPATH_LENGTH		XCONTENT_MAX_FILENAME_LENGTH
#elif defined (_WINDOWS)
	#define MAX_FILEPATH_LENGTH		MAX_PATH
#elif PLATFORM_UNIX
	#define MAX_FILEPATH_LENGTH		128
#else
	#error Define your platform here!
#endif

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

// Global variables.
extern DWORD GCRCTable[];

/*----------------------------------------------------------------------------
	Byte order conversion.
----------------------------------------------------------------------------*/

// These macros are not safe to use unless data is UNSIGNED!
#define BYTESWAP_ORDER16_unsigned(x)   ((((x)>>8)&0xff)+ (((x)<<8)&0xff00))
#define BYTESWAP_ORDER32_unsigned(x)   (((x)>>24) + (((x)>>8)&0xff00) + (((x)<<8)&0xff0000) + ((x)<<24))

static inline WORD BYTESWAP_ORDER16(WORD val)
{
#if PS3
	return __lhbrx(&val);
#else
	return(BYTESWAP_ORDER16_unsigned(val));
#endif
}

static inline SWORD BYTESWAP_ORDER16(SWORD val)
{
	WORD uval = *((WORD *) &val);
	uval = BYTESWAP_ORDER16_unsigned(uval);
	return( *((SWORD *) &uval) );
}

static inline DWORD BYTESWAP_ORDER32(DWORD val)
{
#if PS3
	return __lwbrx(&val);
#else
	return(BYTESWAP_ORDER32_unsigned(val));
#endif
}

static inline INT BYTESWAP_ORDER32(INT val)
{
	DWORD uval = *((DWORD *) &val);
	uval = BYTESWAP_ORDER32_unsigned(uval);
	return( *((INT *) &uval) );
}

static inline FLOAT BYTESWAP_ORDERF(FLOAT val)
{
	DWORD uval = *((DWORD *) &val);
	uval = BYTESWAP_ORDER32_unsigned(uval);
	return( *((FLOAT *) &uval) );
}

static inline QWORD BYTESWAP_ORDER64(QWORD Value)
{
	QWORD Swapped = 0;
	BYTE* Forward = (BYTE*)&Value;
	BYTE* Reverse = (BYTE*)&Swapped + 7;
	for( INT i=0; i<8; i++ )
	{
		*(Reverse--) = *(Forward++); // copy into Swapped
	}
	return Swapped;
}

static inline void BYTESWAP_ORDER_TCHARARRAY(TCHAR* str)
{
	for (TCHAR* c = str; *c; ++c)
	{
		*c = BYTESWAP_ORDER16_unsigned(*c);
	}
}

// Bitfields.
#ifndef NEXT_BITFIELD
	#if __INTEL_BYTE_ORDER__
		#define NEXT_BITFIELD(b) ((b)<<1)
		#define FIRST_BITFIELD   (1)
	#else
		#define NEXT_BITFIELD(b) ((b)>>1)
		#define FIRST_BITFIELD   (0x80000000)
	#endif
#endif

/**
 * This special tag is a standard C++ way of forcing alignment of the members after this tag
 * to be aligned to sizeof(BITFIELD). It allows gcc (and others) to mimic the alignment that
 * Unreal Script expects. For instance, "BITFIELD x:1; BYTE y;" needs to have y aligned to 4 
 * bytes, but gcc would stick y into the 4-byte memory of x. This tag between x and y will 
 * cause y to be aligned to 4 bytes. It works on all platforms, so we always define it, no 
 * matter the platform.
 */
#if IPHONE
#define SCRIPT_ALIGN	BITFIELD: 0 __attribute__((aligned(4)));
#else
#define SCRIPT_ALIGN	BITFIELD: 0;
#endif

/**
 * We leave this defined so that licensees' headers will compile the first time after syncing 
 * to the QA build with this defined. We also make it an error with not Windows so that 
 * they will make sure to change any local GCC_BITFIELD_MAGIC's they may have manually used
 */
#ifdef _WINDOWS
#define GCC_BITFIELD_MAGIC 
#else
#define GCC_BITFIELD_MAGIC Compile error, please see UnFile.h and SCRIPT_ALIGN
#endif

// General byte swapping.
#if __INTEL_BYTE_ORDER__
	#define INTEL_ORDER16(x)   (x)
	#define INTEL_ORDER32(x)   (x)
	#define INTEL_ORDERF(x)    (x)
	#define INTEL_ORDER64(x)   (x)
	#define INTEL_ORDER_TCHARARRAY(x)
#else
	#define INTEL_ORDER16(x)			BYTESWAP_ORDER16(x)
	#define INTEL_ORDER32(x)			BYTESWAP_ORDER32(x)
	#define INTEL_ORDERF(x)				BYTESWAP_ORDERF(x)
	#define INTEL_ORDER64(x)			BYTESWAP_ORDER64(x)
	#define INTEL_ORDER_TCHARARRAY(x)	BYTESWAP_ORDER_TCHARARRAY(x)
#endif

/*-----------------------------------------------------------------------------
	Stats.
-----------------------------------------------------------------------------*/

#if STATS
	#define STAT(x) x
#else
	#define STAT(x)
#endif

#if STATS_SLOW
	#define STAT_SLOW(x) x
#else
	#define STAT_SLOW(x)
#endif

#if STATS && _WINDOWS
	#define STATWIN(x) x
#else
	#define STATWIN(x)
#endif

/*-----------------------------------------------------------------------------
	Global init and exit.
-----------------------------------------------------------------------------*/

/** @name Global init and exit */
//@{
/** General initialization.  Called from within guarded code at the beginning of engine initialization. */
void appInit( const TCHAR* InCmdLine, FOutputDevice* InLog, FOutputDeviceConsole* InLogConsole, FOutputDeviceError* InError, FFeedbackContext* InWarn, FFileManager* InFileManager, FCallbackEventObserver* InCallbackEventDevice, FCallbackQueryDevice* InCallbackQueryDevice, FConfigCacheIni*(*ConfigFactory)() );
/** Pre-shutdown.  Called from within guarded exit code, only during non-error exits.*/
void appPreExit();
/** Shutdown.  Called outside guarded exit code, during all exits (including error exits). */
void appExit();
/**
 * Starts up the socket subsystem 
 *
 * @param bIsEarlyInit If TRUE, this function is being called before GCmdLine, GFileManager, GConfig, etc are setup. If FALSE, they have been initialized properly
 */
void appSocketInit(UBOOL bIsEarlyInit);
/** Shuts down the socket subsystem */
void appSocketExit(void);

/**
 * Sets GCmdLine to the string given
 */
void appSetCommandline(TCHAR* NewCommandLine);

/**
 * Rebuild the commandline if needed
 *
 * @param NewCommandLine The commandline to fill out
 *
 * @return TRUE if NewCommandLine should be pushed to GCmdLine
 */
UBOOL appResetCommandLine(TCHAR NewCommandLine[16384]);

/** Returns the language setting that is configured for the platform */
#if PS3
// * @param bDisallowDiskAccess If we need the language before we've initialized the file manager, we can't allow disk access
FString appGetLanguageExt(UBOOL bDisallowDiskAccess=FALSE);
#else
FString appGetLanguageExt(void);
#endif

/** 
 * Returns a list of known language extensions.
 *
 * @return The array of known language extensions found in the Engine.ini config file
 */
const TArray<FString>& appGetKnownLanguageExtensions();

/** 
 *	Returns whether the given language setting is known or not
 *
 *	@param	InLangExt		The language extension to check for validity
 *
 *	@return	UBOOL			TRUE if valid, FALSE if not
 */
UBOOL appIsKnownLanguageExt(const FString& InLangExt);
/** Starts up performance counters on dedicated servers*/
void appPerfCountersInit(void);
/** Chance for performance counters to update on dedicated servers */
void appPerfCountersUpdate(void);
/** Shuts down performance counters on dedicated servers */
void appPerfCountersCleanup(void);

//@}

/** @name Interface for recording loading errors in the editor */
//@{
void EdClearLoadErrors();
VARARG_DECL( void, static void, VARARG_NONE, EdLoadErrorf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(INT Type), VARARG_EXTRA(Type) );

/** Passed to appMsgf to specify the type of message dialog box. */
enum EAppMsgType
{
	AMT_OK,
	AMT_YesNo,
	AMT_OKCancel,
	AMT_YesNoCancel,
	AMT_CancelRetryContinue,
	AMT_YesNoYesAllNoAll,
	AMT_YesNoYesAllNoAllCancel,
#if IPHONE
	AMT_Crash,
#endif
};

/** Returned from appMsgf to specify the type of chosen option. */
enum EAppReturnType
{
	ART_No			= 0,
	ART_Yes,
	ART_YesAll,
	ART_NoAll,
	ART_Cancel,
};

/**
* Pops up a message dialog box containing the input string.  Return value depends on the type of dialog.
*
* @param	Type	Specifies the type of message dialog.
* @return			1 if user selected OK/YES, or 0 if user selected CANCEL/NO.
*/
VARARG_DECL( UBOOL, static UBOOL, return, appMsgf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(EAppMsgType Type), VARARG_EXTRA(Type) );
//@}

/**
* Pops up a message dialog box containing the input string.  Return value depends on the type of dialog.
*
* @param	Type	Specifies the type of message dialog.
* @param	DefaultValue Default Value if Silence
* @param	bSilence	Silence? Or message box?
* @return			1 if user selected OK/YES, or 0 if user selected CANCEL/NO.
*/
VARARG_DECL( UBOOL, static UBOOL, return, appMsgExf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(EAppMsgType Type) VARARG_EXTRA(UBOOL bDefaultValue) VARARG_EXTRA(UBOOL bSilence), VARARG_EXTRA(Type)  VARARG_EXTRA(bDefaultValue) VARARG_EXTRA(bSilence));
//@}
/*-----------------------------------------------------------------------------
	Misc.
-----------------------------------------------------------------------------*/

/** 
 * Return the current configuration of this binary (Debug, Release, Shipping or Test)
 */
FString appGetConfigurationString();

/**
 * Returns the number of modules loaded by the currently running process.
 */
INT appGetProcessModuleCount();

/**
 * Gets the signature for every module loaded by the currently running process.
 *
 * @param	ModuleSignatures		An array to retrieve the module signatures.
 * @param	ModuleSignaturesSize	The size of the array pointed to by ModuleSignatures.
 *
 * @return	The number of modules copied into ModuleSignatures
 */
INT appGetProcessModuleSignatures(struct FModuleInfo *ModuleSignatures, const INT ModuleSignaturesSize);

/** @name DLL access */
//@{
void* appGetDllHandle( const TCHAR* DllName );
/** Frees a DLL. */
void appFreeDllHandle( void* DllHandle );
/** Looks up the address of a DLL function. */
void* appGetDllExport( void* DllHandle, const TCHAR* ExportName );
//@}

/**
 * Does per platform initialization of timing information and returns the current time. This function is
 * called before the execution of main as GStartTime is statically initialized by it. The function also
 * internally sets GSecondsPerCycle, which is safe to do as static initialization order enforces complex
 * initialization after the initial 0 initialization of the value.
 *
 * @return	current time
 */
DOUBLE appInitTiming();

/**
 * Launches a uniform resource locator (i.e. http://www.epicgames.com/unreal).
 * This is expected to return immediately as the URL is launched by another task.
 */
void appLaunchURL( const TCHAR* URL, const TCHAR* Parms=NULL, FString* Error=NULL );

/**
 * Attempt to launch the provided file name in its default external application. Similar to appLaunchURL,
 * with the exception that if a default application isn't found for the file, the user will be prompted with
 * an "Open With..." dialog.
 *
 * @param	FileName	Name of the file to attempt to launch in its default external application
 * @param	Parms		Optional parameters to the default application
 */
void appLaunchFileInDefaultExternalApplication( const TCHAR* FileName, const TCHAR* Parms = NULL );

/**
 * Attempt to "explore" the folder specified by the provided file path
 *
 * @param	FilePath	File path specifying a folder to explore
 */
void appExploreFolder( const TCHAR* FilePath );

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
void* appCreateProc( const TCHAR* URL, const TCHAR* Parms, UBOOL bLaunchDetached=TRUE, UBOOL bLaunchHidden=FALSE, UBOOL bLaunchReallyHidden = FALSE, DWORD* OutProcessID = NULL, INT PriorityModifier=0 );

/** Returns TRUE if the specified process is running 
*
* @param ProcessHandle handle returned from appCreateProc
* @return TRUE if the process is still running
*/
UBOOL appIsProcRunning( void* ProcessHandle );

/** Waits for a process to stop
*
* @param ProcessHandle handle returned from appCreateProc
*/
void appWaitForProc( void* ProcessHandle );

/** Terminates a process
*
* @param ProcessHandle handle returned from appCreateProc
*/
void appTerminateProc( void* ProcessHandle );

/** Retrieves the termination status of the specified process. */
UBOOL appGetProcReturnCode( void* ProcHandle, INT* ReturnCode );
/** Returns TRUE if the specified application is running */
UBOOL appIsApplicationRunning( DWORD ProcessId );

/** Retrieves the ProcessId of this process
*
* @return the ProcessId of this process
*/
DWORD appGetCurrentProcessId();

/** Creates a new globally unique identifier. */
class FGuid appCreateGuid();

/** 
* Creates a temporary filename (extension = '.tmp')
*
* @param Path - file pathname
* @param Result1024 - destination buffer to store results of unique path (@warning must be >= MAX_SPRINTF size)
*/
void appCreateTempFilename( const TCHAR* Path, TCHAR* Result, SIZE_T ResultSize );

/** 
* Creates a temporary filename with the specified prefix 
*
* @param Path - file pathname
* @param Prefix - file prefix
* @param Extension - file extension ('.' required!)
* @param Result1024 - destination buffer to store results of unique path (@warning must be >= MAX_SPRINTF size)
*/
void appCreateTempFilename( const TCHAR* Path, const TCHAR* Prefix, const TCHAR* Extension, TCHAR* Result, SIZE_T ResultSize );


/** 
* Removes the executable name from a commandline, denoted by parentheses.
*/
const TCHAR* RemoveExeName(const TCHAR* CmdLine);

/**
 * Cleans the package download cache and any other platform specific cleaning 
 */
void appCleanFileCache();


/**
* Saves a 24Bit BMP file to disk
* 
* @param Pattern filename with path, must not be 0, if with "bmp" extension (e.g. "out.bmp") the filename stays like this, if without (e.g. "out") automatic index numbers are addended (e.g. "out00002.bmp")
* @param Width >0
* @param Height >0
* @param Data must not be 0
* @param FileManager must not be 0
*
* @return TRUE if success
*/
UBOOL appCreateBitmap( const TCHAR* Pattern, INT Width, INT Height, class FColor* Data, FFileManager* FileManager = GFileManager );

/**
 * Finds a usable splash pathname for the given filename
 * 
 * @param SplashFilename Name of the desired splash name ("Splash.bmp")
 * @param OutPath String containing the path to the file, if this function returns TRUE
 *
 * @return TRUE if a splash screen was found
 */
UBOOL appGetSplashPath(const TCHAR* SplashFilename, FString& OutPath);

/** deletes log files older than a number of days specified in the Engine ini file */
void appDeleteOldLogs();

/**
 * Figure out which native OnlineSubsystem package to add to native package list (this is what is requested, if it 
 * doesn't exist, then OSSPC will be attempted as a fallback, and failing that, nothing will be used)
 *
 * NOTE: If this changes, you should also change the corresponding GetDesiredOnlineSubsystem function in UBT
 * 
 * @return The name of the OnlineSubystem package 
 */
const TCHAR* appGetOSSPackageName();


/*-----------------------------------------------------------------------------
	Ini files.
-----------------------------------------------------------------------------*/

/**
 * This will load up two .ini files and then determine if the Generated one is outdated.
 * Outdatedness is determined by the following mechanic:
 *
 * When a generated .ini is written out it will store the timestamps of the files it was generated
 * from.  This way whenever the Default__.inis are modified the Generated .ini will view itself as
 * outdated and regenerate it self.
 *
 * Outdatedness also can be affected by commandline params which allow one to delete all .ini, have
 * automated build system etc.
 *
 * Additionally, this function will save the previous generation of the .ini to the Config dir with
 * a datestamp.
 *
 * Finally, this function will load the Generated .ini into the global ConfigManager.
 *
 * @param DefaultIniFile			The Default .ini file (e.g. DefaultEngine.ini )
 * @param GeneratedIniFile			The Generated .ini file (e.g. FooGameEngine.ini )
 * @param bTryToPreserveContents	If set, only properties that don't exist in the generatedn file will be added
 *										from the default file.  Used for editor user preferences that we don't want
 *										ever want to blow away.
 * @param YesNoToAll				[out] Receives the user's selection if an .ini was out of date.
 * @param bForceReload				Forces a reload of the resulting .ini file.
 */
void appCheckIniForOutdatedness( const TCHAR* GeneratedIniFile, const TCHAR* DefaultIniFile, const UBOOL bTryToPreserveContents, UINT& YesNoToAll, UBOOL bForceReload=FALSE );


/**
* This will create the .ini filenames for the Default and the Game based off the passed in values.
* (e.g. DefaultEditor.ini, MyLeetGameEditor.ini  and the respective relative paths to those files )
*
* @param GeneratedIniName				The Global TCHAR[MAX_SPRINTF] that unreal uses ( e.g. GEngineIni )
* @param GeneratedDefaultIniName		The Global TCHAR[MAX_SPRINTF] that unreal uses for the default ini ( e.g. GDefaultEngineIni )
* @param CommandLineDefaultIniToken	The token to look for on the command line to parse the passed in Default<Type>Ini
* @param CommandLineIniToken			The token to look for on the command line to parse the passed in <Type>Ini
* @param IniFileName					The IniFile's name  (e.g. Engine.ini Editor.ini )
* @param DefaultIniPrefix				What the prefix for the Default .inis should be  ( e.g. Default )
* @param IniPrefix						What the prefix for the Game's .inis should be  ( generally empty )
*/
void appCreateIniNames( TCHAR* GeneratedIniName, TCHAR* GeneratedDefaultIniName, const TCHAR* CommandLineDefaultIniName, const TCHAR* CommandLineIniName, const TCHAR* IniFileName, const TCHAR* DefaultIniPrefix, const TCHAR* IniPrefix );


/**
* This will completely load an .ini file into the passed in FConfigFile.  This means that it will 
* recurse up the BasedOn hierarchy loading each of those .ini.  The passed in FConfigFile will then
* have the data after combining all of those .ini 
*
* @param FilenameToLoad - this is the path to the file to 
* @param ConfigFile - This is the FConfigFile which will have the contents of the .ini loaded into and Combined()
* @param bUpdateIniTimeStamps - whether to update the timestamps array.  Only for Default___.ini should this be set to TRUE.  The generated .inis already have the timestamps.
*
**/
void LoadAnIniFile( const TCHAR* FilenameToLoad, FConfigFile& ConfigFile, UBOOL bUpdateIniTimeStamps );

#ifndef SUPPORT_NAMES_ONLY
#define SUPPORT_NAMES_ONLY
#endif
// provide the engine with access to the names used by the console support .dlls
#include "../../Engine/Inc/UnConsoleTools.h"
#undef SUPPORT_NAMES_ONLY

#define XENON_DEFAULT_INI_PREFIX TEXT("Xbox360\\Xbox360")
#define XENON_INI_PREFIX TEXT("Xbox360-")

#define PS3_DEFAULT_INI_PREFIX TEXT("PS3\\PS3")
#define PS3_INI_PREFIX TEXT("PS3-")

#define PCSERVER_DEFAULT_INI_PREFIX TEXT("PCServer\\PCServer")
#define PCSERVER_INI_PREFIX TEXT("PCServer-")

#define SIMMOBILE_DEFAULT_INI_PREFIX TEXT("Mobile\\Mobile")
#define SIMMOBILE_INI_PREFIX TEXT("Mobile-")

#define NGP_DEFAULT_INI_PREFIX TEXT("NGP\\NGP")
#define NGP_INI_PREFIX TEXT("NGP-")

#define IPHONE_DEFAULT_INI_PREFIX TEXT("IPhone\\IPhone")
#define IPHONE_INI_PREFIX TEXT("IPhone-")

#define ANDROID_DEFAULT_INI_PREFIX TEXT("Android\\Android")
#define ANDROID_INI_PREFIX TEXT("Android-")

#define WIIU_DEFAULT_INI_PREFIX TEXT("WiiU\\WiiU")
#define WIIU_INI_PREFIX TEXT("WiiU-")

#define PC_DEFAULT_INI_PREFIX TEXT("Default")
#define PC_INI_PREFIX TEXT("")

#define FLASH_DEFAULT_INI_PREFIX TEXT("Flash\\Flash")
#define FLASH_INI_PREFIX TEXT("Flash-")

#define MACOSX_DEFAULT_INI_PREFIX TEXT("Mac\\Mac")
#define MACOSX_INI_PREFIX TEXT("Mac-")

// Default ini prefix.
#if XBOX
    #define DEFAULT_INI_PREFIX XENON_DEFAULT_INI_PREFIX
    #define INI_PREFIX XENON_INI_PREFIX
#elif PS3
	#define DEFAULT_INI_PREFIX PS3_DEFAULT_INI_PREFIX
	#define INI_PREFIX PS3_INI_PREFIX
#elif IPHONE
	#define DEFAULT_INI_PREFIX IPHONE_DEFAULT_INI_PREFIX
	#define INI_PREFIX IPHONE_INI_PREFIX
#elif FLASH
    #define DEFAULT_INI_PREFIX FLASH_DEFAULT_INI_PREFIX
    #define INI_PREFIX FLASH_INI_PREFIX
#elif ANDROID
	#define DEFAULT_INI_PREFIX ANDROID_DEFAULT_INI_PREFIX
	#define INI_PREFIX ANDROID_INI_PREFIX
#elif DEDICATED_SERVER
	#define DEFAULT_INI_PREFIX PCSERVER_DEFAULT_INI_PREFIX
	#define INI_PREFIX PCSERVER_INI_PREFIX
#elif PLATFORM_MACOSX
	#define DEFAULT_INI_PREFIX MACOSX_DEFAULT_INI_PREFIX
	#define INI_PREFIX MACOSX_INI_PREFIX
#elif defined (_WINDOWS) || PLATFORM_UNIX
    #define DEFAULT_INI_PREFIX PC_DEFAULT_INI_PREFIX
    #define INI_PREFIX PC_INI_PREFIX
#elif NGP
	#define DEFAULT_INI_PREFIX NGP_DEFAULT_INI_PREFIX
	#define INI_PREFIX NGP_INI_PREFIX
#elif WIIU
	#define DEFAULT_INI_PREFIX WIIU_DEFAULT_INI_PREFIX
	#define INI_PREFIX WIIU_INI_PREFIX
#else
	#error define your platform here
#endif


/*-----------------------------------------------------------------------------
	User created content.
-----------------------------------------------------------------------------*/

/** 
 * @return TRUE if any user created content was loaded since boot or since a call to appResetUserCreatedContentLoaded()
 */
UBOOL appHasAnyUserCreatedContentLoaded();

/**
 * Marks that some user created content was loaded
 */
void appSetUserCreatedContentLoaded();

/**
 * Resets the flag that tracks if any user created content was loaded (say, on return to main menu, etc)
 */
void appResetUserCreatedContentLoaded();


/*-----------------------------------------------------------------------------
	Package file cache.
-----------------------------------------------------------------------------*/

/**
 * This will recurse over a directory structure looking for files.
 * 
 * @param Result The output array that is filled out with a file paths
 * @param RootDirectory The root of the directory structure to recurse through
 * @param bFindPackages Should this function add package files to the Resuls
 * @param bFindNonPackages Should this function add non-package files to the Results
 */
void appFindFilesInDirectory(TArray<FString>& Results, const TCHAR* RootDirectory, UBOOL bFindPackages, UBOOL bFindNonPackages);

#define NO_USER_SPECIFIED -1

/** @name Package file cache */
//@{
struct FPackageFileCache
{
	/**
	 * Strips all path and extension information from a relative or fully qualified file name.
	 *
	 * @param	InPathName	a relative or fully qualified file name
	 *
	 * @return	the passed in string, stripped of path and extensions
	 */
	static FString PackageFromPath( const TCHAR* InPathName );

	/**
	 * Parses a fully qualified or relative filename into its components (filename, path, extension).
	 *
	 * @param	InPathName	the filename to parse
	 * @param	Path		[out] receives the value of the path portion of the input string
	 * @param	Filename	[out] receives the value of the filename portion of the input string
	 * @param	Extension	[out] receives the value of the extension portion of the input string
	 */
	static void SplitPath( const TCHAR* InPathName, FString& Path, FString& Filename, FString& Extension );

	/**
	 * Replaces all slashes and backslashes in the string with the correct path separator character for the current platform.
	 *
	 * @param	InFilename	a string representing a filename.
	 */
	static void NormalizePathSeparators( FString& InFilename );

	/** 
	 * Cache all packages found in the engine's configured paths directories, recursively.
	 */
	virtual void CachePaths()=0;

	/**
	 * Adds the package name specified to the runtime lookup map.  The stripped package name (minus extension or path info) will be mapped
	 * to the fully qualified or relative filename specified.
	 *
	 * @param	InPathName		a fully qualified or relative [to Binaries] path name for an Unreal package file.
	 * @param	InOverrideDupe	specify TRUE to replace existing mapping with the new path info
	 * @param	WarnIfExists	specify TRUE to write a warning to the log if there is an existing entry for this package name
	 *
	 * @return	TRUE if the specified path name was successfully added to the lookup table; FALSE if an entry already existed for this package
	 */
	virtual UBOOL CachePackage( const TCHAR* InPathName, UBOOL InOverrideDupe=FALSE, UBOOL WarnIfExists=TRUE)=0;

	/**
	 * Finds the fully qualified or relative pathname for the package specified.
	 *
	 * @param	InName			a string representing the filename of an Unreal package; may include path and/or extension.
	 * @param	Guid			if specified, searches the directory containing files downloaded from other servers (cache directory) for the specified package.
	 * @param	OutFilename		receives the full [or relative] path that was originally registered for the package specified.
	 * @param	Language		Language version to retrieve if overridden for that particular language
	 *
	 * @return	TRUE if the package was successfully found.
	 */
	virtual UBOOL FindPackageFile( const TCHAR* InName, const FGuid* Guid, FString& OutFileName, const TCHAR* Language=NULL)=0;

	/**
	 * Sets the source control status for a package
	 *
	 * @param	InName			a string representing the filename of an Unreal package; may include path and/or extension.
	 * @param	InState			the new source control state
	 */
	virtual UBOOL SetSourceControlState ( const TCHAR* InName, INT InNewState)=0;
	/**
	 * Gets the source control status for a package
	 *
	 * @param	InName			a string representing the filename of an Unreal package; may include path and/or extension.
	 */
	virtual INT GetSourceControlState ( const TCHAR* InName)=0;

	/**
	 * Returns the list of fully qualified or relative pathnames for all registered packages.
	 */
	virtual TArray<FString> GetPackageFileList() = 0;

	/**
	 * Add a downloaded content package to the list of known packages.
	 * Can be assigned to a particular ID for removing with ClearDownloadadPackages.
	 *
	 * @param InPlatformPathName The platform-specific full path name to a package (will be passed directly to CreateFileReader)
	 * @param UserIndex Optional user to associate with the package so that it can be flushed later
	 *
	 * @return TRUE if successful
	 */
	virtual UBOOL CacheDownloadedPackage(const TCHAR* InPlatformPathName, INT UserIndex=NO_USER_SPECIFIED)=0;

	/**
	 * Clears all entries from the package file cache.
	 *
	 * @script patcher
	 */
	virtual void ClearPackageCache()=0;

	/**
	 * Remove all downloaded packages from the package file cache.
	 */
	virtual void ClearDownloadedPackages()=0;
};

extern FPackageFileCache* GPackageFileCache;
//@}

/** Converts a relative path name to a fully qualified name */
FString appConvertRelativePathToFull(const FString& InString);

/**
 * Takes a fully pathed string and eliminates relative pathing (eg: annihilates ".." with the adjacent directory).
 * Assumes all slashes have been converted to PATH_SEPARATOR[0].
 * For example, takes the string:
 *	BaseDirectory/SomeDirectory/../SomeOtherDirectory/Filename.ext
 * and converts it to:
 *	BaseDirectory/SomeOtherDirectory/Filename.ext
 *
 * @param	InString	a pathname potentially containing relative pathing
 */
FString appCollapseRelativeDirectories(const FString& InString);

/*-----------------------------------------------------------------------------
	Clipboard.
-----------------------------------------------------------------------------*/

/** @name Clipboard */
//@{
/** Copies text to the operating system clipboard. */
void appClipboardCopy( const TCHAR* Str );
/** Pastes in text from the operating system clipboard. */
FString appClipboardPaste();
//@}

/*-----------------------------------------------------------------------------
	Exception handling.
-----------------------------------------------------------------------------*/

/** @name Exception handling */
//@{
/** For throwing string-exceptions which safely propagate through guard/unguard. */
VARARG_DECL( void VARARGS, static void, VARARG_NONE, appThrowf, VARARG_NONE, const TCHAR*, VARARG_NONE, VARARG_NONE );
//@}

/**
 * Raises an OS exception. Normally used for critical errors that forces UE3 to shutdown.
 * These can be caught by __try/__except in MS unmanaged C++ environments, and
 * try/catch(Exception) in MS managed C++ environments.
 *
 * @ExceptionCode	Application-specific code that is passed to the exception handler.
 */
void appRaiseException( DWORD ExceptionCode );

/*-----------------------------------------------------------------------------
	Check macros for assertions.
-----------------------------------------------------------------------------*/

#if XBOX 
// Using !! to work around error in handling of expressions.
#define ANALYSIS_ASSUME(expr) __analysis_assume(!!(expr))
#elif COMPILER_SUPPORTS_NOOP
#define ANALYSIS_ASSUME       __noop
#elif SUPPORTS_VARIADIC_MACROS
#define ANALYSIS_ASSUME(...)
#else
#define ANALYSIS_ASSUME(expr)
#endif

//
// "check" expressions are only evaluated if enabled.
// "verify" expressions are always evaluated, but only cause an error if enabled.
//
/** @name Assertion macros */
//@{

inline void NullAssertFunc(...)		{}

#if DO_CHECK
	#define checkCode( Code )		do { Code } while ( false );
	#define checkMsg(expr,msg)		{ if(!(expr)) {      appFailAssert( #expr " : " #msg , __FILE__, __LINE__ ); } ANALYSIS_ASSUME(expr); }
    #define checkFunc(expr,func)	{ if(!(expr)) {func; appFailAssert( #expr, __FILE__, __LINE__ ); } ANALYSIS_ASSUME(expr); }
	#define verify(expr)			{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ ); ANALYSIS_ASSUME(expr); }
	#define check(expr)				{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ ); ANALYSIS_ASSUME(expr); }

	/**
	 * Denotes codepaths that should never be reached.
	 */
	#define checkNoEntry()       { appFailAssert( "Enclosing block should never be called", __FILE__, __LINE__ ); }

	/**
	 * Denotes codepaths that should not be executed more than once.
	 */
	#define checkNoReentry()     { static bool s_beenHere##__LINE__ = false;                                         \
	                               checkMsg( !s_beenHere##__LINE__, Enclosing block was called more than once );   \
								   s_beenHere##__LINE__ = true; }

	class FRecursionScopeMarker
	{
	public: 
		FRecursionScopeMarker(WORD &InCounter) : Counter( InCounter ) { ++Counter; }
		~FRecursionScopeMarker() { --Counter; }
	private:
		WORD& Counter;
	};

	/**
	 * Denotes codepaths that should never be called recursively.
	 */
	#define checkNoRecursion()  static WORD RecursionCounter##__LINE__ = 0;                                            \
	                            checkMsg( RecursionCounter##__LINE__ == 0, Enclosing block was entered recursively );  \
	                            const FRecursionScopeMarker ScopeMarker##__LINE__( RecursionCounter##__LINE__ )

#if SUPPORTS_VARIADIC_MACROS
	/**
	* verifyf, checkf: Same as verify, check but with printf style additional parameters
	* Read about __VA_ARGS__ (variadic macros) on http://gcc.gnu.org/onlinedocs/gcc-3.4.4/cpp.pdf.
	*/
#if __INTEL_COMPILER
	#define verifyf(expr, ...)				{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__, __VA_ARGS__ ); }
	#define checkf(expr, ...)				{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__, __VA_ARGS__ ); }
#else
	#define verifyf(expr, ...)				{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__, ##__VA_ARGS__ ); ANALYSIS_ASSUME(expr); }
	#define checkf(expr, ...)				{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__, ##__VA_ARGS__ ); ANALYSIS_ASSUME(expr); }
#endif
#elif _MSC_VER >= 1300
	#define verifyf(expr,a,b,c,d,e,f,g,h)	{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__, VARG(a),VARG(b),VARG(c),VARG(d),VARG(e),VARG(f),VARG(g),VARG(h) ); ANALYSIS_ASSUME(expr); }
	#define checkf(expr,a,b,c,d,e,f,g,h)	{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__, VARG(a),VARG(b),VARG(c),VARG(d),VARG(e),VARG(f),VARG(g),VARG(h) ); ANALYSIS_ASSUME(expr); }
#endif

#else
#if COMPILER_SUPPORTS_NOOP
	// MS compilers support noop which discards everything inside the parens
	#define checkCode(Code)			{}
	#define check					__noop
	#define checkf					__noop
	#define checkMsg				__noop
	#define checkFunc				__noop
	#define checkNoEntry			__noop
	#define checkNoReentry			__noop
	#define checkNoRecursion		__noop
#elif SUPPORTS_VARIADIC_MACROS
	#define checkCode(...)
	#define check(...)
	#define checkf(...)
	#define checkMsg(...)
	#define checkFunc(...)    
	#define checkNoEntry(...)
	#define checkNoReentry(...)
	#define checkNoRecursion(...)
#else
	#define checkCode(Code)				{}
	#define check						{}
	#define checkf						NullAssertFunc
    #define checkMsg(expr,msg)			{}
    #define checkFunc(expr,func)		{}    
	#define checkNoEntry()				{}
	#define checkNoReentry()			{}
	#define checkNoRecursion()			{}
#endif
	#define verify(expr)						{ if(!(expr)){} }
	#if SUPPORTS_VARIADIC_MACROS
		#define verifyf(expr, ...)				{ if(!(expr)){} }
	#elif _MSC_VER >= 1300
		#define verifyf(expr,a,b,c,d,e,f,g,h)	{ if(!(expr)){} }
	#endif
#endif

//
// Check for development only.
//
#if DO_GUARD_SLOW
    #if SUPPORTS_VARIADIC_MACROS
        #define checkSlow(expr, ...)				{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ ); ANALYSIS_ASSUME(expr); }
		#define checkfSlow(expr, ...)				{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__, __VA_ARGS__ ); ANALYSIS_ASSUME(expr); }
    #elif _MSC_VER >= 1300
        #define checkSlow(expr,a,b,c,d,e,f,g,h)		{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ ); ANALYSIS_ASSUME(expr); }
		#define checkfSlow(expr,a,b,c,d,e,f,g,h)	{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__, VARG(a),VARG(b),VARG(c),VARG(d),VARG(e),VARG(f),VARG(g),VARG(h) ); ANALYSIS_ASSUME(expr); }
    #endif

	#define verifySlow(expr)  						{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ ); ANALYSIS_ASSUME(expr); }
#else
#if COMPILER_SUPPORTS_NOOP
	// MS compilers support noop which discards everything inside the parens
	#define checkSlow         __noop
	#define checkfSlow        __noop
#else
	#define checkSlow(expr)   {}
	#define checkfSlow		  NullAssertFunc
#endif
	#define verifySlow(expr)  if(expr){}
#endif

// 
// Intermediate check for debug + ReleasePC, but not release console
//
#if DO_GUARD_SLOWISH
	#if SUPPORTS_VARIADIC_MACROS
		#define checkSlowish(expr, ...)				{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ ); ANALYSIS_ASSUME(expr); }
	#if __INTEL_COMPILER
		#define checkfSlowish(expr, ...)			{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__, __VA_ARGS__ ); ANALYSIS_ASSUME(expr); }
	#else
		#define checkfSlowish(expr, ...)			{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__, ##__VA_ARGS__ ); ANALYSIS_ASSUME(expr); }
	#endif
	#elif _MSC_VER >= 1300
		#define checkSlowish(expr,a,b,c,d,e,f,g,h)	{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ ); ANALYSIS_ASSUME(expr); }
		#define checkfSlowish(expr,a,b,c,d,e,f,g,h)	{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__, VARG(a),VARG(b),VARG(c),VARG(d),VARG(e),VARG(f),VARG(g),VARG(h) ); ANALYSIS_ASSUME(expr); }
	#endif
#else
	#if COMPILER_SUPPORTS_NOOP
		// MS compilers support noop which discards everything inside the parens
		#define checkSlowish         __noop
		#define checkfSlowish        __noop
	#elif SUPPORTS_VARIADIC_MACROS
		#define checkSlowish(...)
		#define checkfSlowish(...)
	#else
		#define checkSlowish(expr)   {}
		#define checkfSlowish		  NullAssertFunc
	#endif
#endif



/**
 * ensure() can be used to test for *non-fatal* errors at runtime
 *
 * Rather than crashing, an error report (with a full call stack) will be logged and submitted to the crash server. 
 * This is useful when you want runtime code verification but you're handling the error case anyway.
 *
 * Note: ensure() can be nested within conditionals!
 *
 * Example:
 *
 *		if( ensure( InObject != NULL ) )
 *		{
 *			InObject->Modify();
 *		}
 *
 * This code is safe to execute as the pointer dereference is wrapped in a non-NULL conditional block, but
 * you still want to find out if this ever happens so you can avoid side effects.  Using ensure() here will
 * force a crash report to be generated without crashing the application (and potentially causing editor
 * users to lose unsaved work.)
 *
 * ensure() resolves to a regular assertion (crash) on consoles and in debug, release or shipping builds.
 */

#if SUPPORTS_VARIADIC_MACROS

#if DO_CHECK

	#define ensure( InExpression ) \
		appEnsureNotFalse( ( ( InExpression ) != 0 ), #InExpression, __FILE__, __LINE__ )

	#define ensureMsg( InExpression, InMsg ) \
		appEnsureNotFalse( ( ( InExpression ) != 0 ), #InExpression, __FILE__, __LINE__, InMsg )

	#if __INTEL_COMPILER
		#define ensureMsgf( InExpression, ... ) \
			appEnsureNotFalseFormatted( ( ( InExpression ) != 0 ), #InExpression, __FILE__, __LINE__, __VA_ARGS__ )
	#else
		#define ensureMsgf( InExpression, ... ) \
			appEnsureNotFalseFormatted( ( ( InExpression ) != 0 ), #InExpression, __FILE__, __LINE__, ##__VA_ARGS__ )
	#endif

#else	// DO_CHECK

	#define ensure( InExpression ) ( ( InExpression ) != 0 )

	#define ensureMsg( InExpression, InMsg ) ( ( InExpression ) != 0 )

	#define ensureMsgf( InExpression, ... ) ( ( InExpression ) != 0 )

#endif	// DO_CHECK

#endif	// SUPPORTS_VARIADIC_MACROS


//@}


/*-----------------------------------------------------------------------------
	Localization.
-----------------------------------------------------------------------------*/

/** @name Localization */
//@{
FString Localize( const TCHAR* Section, const TCHAR* Key, const TCHAR* Package=GPackage, const TCHAR* LangExt=NULL, UBOOL Optional=0 );
FString LocalizeLabel( const TCHAR* Section, const TCHAR* Key, const TCHAR* Package=GPackage, const TCHAR* LangExt=NULL, UBOOL Optional=0 );
FString LocalizeError( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
FString LocalizeProgress( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
FString LocalizeQuery( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
FString LocalizeGeneral( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
FString LocalizeUnrealEd( const TCHAR* Key, const TCHAR* Package=TEXT("UnrealEd"), const TCHAR* LangExt=NULL );
FString LocalizeProperty( const TCHAR* Section, const TCHAR* Key, const TCHAR* Package=TEXT("Properties"), const TCHAR* LangExt=NULL );
FString LocalizePropertyPath( const TCHAR* PackageSectionKey, const TCHAR* LangExt=NULL );

#if !TCHAR_IS_1_BYTE
FString Localize( const ANSICHAR* Section, const ANSICHAR* Key, const TCHAR* Package=GPackage, const TCHAR* LangExt=NULL, UBOOL Optional=0 );
FString LocalizeLabel( const ANSICHAR* Section, const ANSICHAR* Key, const TCHAR* Package=GPackage, const TCHAR* LangExt=NULL, UBOOL Optional=0 );
FString LocalizeError( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
FString LocalizeProgress( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
FString LocalizeQuery( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
FString LocalizeGeneral( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
FString LocalizeUnrealEd( const ANSICHAR* Key, const TCHAR* Package=TEXT("UnrealEd"), const TCHAR* LangExt=NULL );
FString LocalizeProperty( const ANSICHAR* Section, const ANSICHAR* Key, const TCHAR* Package=TEXT("Properties"), const TCHAR* LangExt=NULL );
FString LocalizePropertyPath( const ANSICHAR* PackageSectionKey, const TCHAR* LangExt=NULL );
#endif
//@}

/*-----------------------------------------------------------------------------
	OS functions.
-----------------------------------------------------------------------------*/

/** Returns the executable command line. */
const TCHAR* appCmdLine();
/** Returns TRUE if the directory exists */
UBOOL appDirectoryExists( const TCHAR* DirectoryName );
/**
 * Returns the startup directory (the directory this executable was launched from)
 *
 * NOTE: Only one return value is valid at a time!
 * NOTE: On Mac, this completely breaks the standard "Binaries\*Platform*" relative directory structure, due to the way .apps are structured
 *	(do not depend upon the .app being located here, use appMacAppDir instead; this is platform-specific unfortunately)
 */
const TCHAR* appBaseDir();
/** Returns the computer name.  NOTE: Only one return value is valid at a time! */
const TCHAR* appComputerName();
/** Returns the user name.  NOTE: Only one return value is valid at a time! */
const TCHAR* appUserName();
/** Returns shader dir relative to appBaseDir */
const TCHAR* appShaderDir();
/** Returns name of currently running executable (NOTE: On Mac, this returns the .app name, not the actual binary name) */
const TCHAR* appExecutableName();

#if PLATFORM_MACOSX
/** Returns the directory of the Mac .app */
const TCHAR* appMacAppDir();
/** Returns the directory of the Mac binary (within the .app) */
const TCHAR* appMacBinaryDir();
/** Returns the name of the current running Mac binary */
const TCHAR* appMacBinaryName();
#endif

namespace UE3
{
	/**
	 * The platform that this is running on.  This mask is also used by UFunction::PlatformFlags to determine which platforms
	 * a native function can be bound for.
	 * NOTE: Be sure to check hardcoded/mirrored usage found in LanPacketPlatformMask
	 *												            WindowsTools.dll WindowsTarget.h 
	 *												            MemoryProfiler2 ProfileDataHeader.cs
	 * NOTE: Only append to the bottom of the enum to preserve existing values
	 */
	enum EPlatformType
	{
		PLATFORM_Unknown			=	0x00000000,
		PLATFORM_Windows			=	0x00000001,
		PLATFORM_WindowsServer		=	0x00000002,		// Windows platform dedicated server mode ("lean and mean" cooked as console without editor support)
		PLATFORM_Xbox360			=	0x00000004,
		PLATFORM_PS3				=	0x00000008,
		PLATFORM_Linux				=	0x00000010,
		PLATFORM_MacOSX				=	0x00000020,
		PLATFORM_WindowsConsole		=	0x00000040,     // Windows platform cooked as console without editor support
		PLATFORM_IPhone				=	0x00000080,
		PLATFORM_NGP				=	0x00000100,
		PLATFORM_Android			=	0x00000200,
		PLATFORM_WiiU				=	0x00000400,
		PLATFORM_Flash				=	0x00000800,

		// Combination Masks
		/** PC platform types */
		PLATFORM_PC					=	PLATFORM_Windows|PLATFORM_WindowsServer|PLATFORM_WindowsConsole|PLATFORM_Linux|PLATFORM_MacOSX,

		/** Windows platform types */
		PLATFORM_AnyWindows			=	PLATFORM_Windows|PLATFORM_WindowsServer|PLATFORM_WindowsConsole,

		/** Console platform types */
		PLATFORM_Console			=	PLATFORM_Xbox360|PLATFORM_PS3|PLATFORM_IPhone|PLATFORM_Android|PLATFORM_NGP|PLATFORM_WiiU|PLATFORM_Flash,

		/** Mobile platform types */
		PLATFORM_Mobile				=	PLATFORM_IPhone|PLATFORM_Android|PLATFORM_NGP|PLATFORM_Flash,

		/** 
		 *	Platforms with editor only data that has been stripped during cooking 
		 *	These are platforms that are COMPILED with the WITH_EDITOR_
		 */
		PLATFORM_FilterEditorOnly	=	PLATFORM_Console|PLATFORM_WindowsServer,

		/** Platforms with data that has been stripped during cooking */
		PLATFORM_Stripped			=	PLATFORM_FilterEditorOnly|PLATFORM_WindowsConsole,

		/** Platforms who's vertex data can't be packed into 16-bit floats */
		PLATFORM_OpenGLES2			=	PLATFORM_IPhone|PLATFORM_Android|PLATFORM_Flash,

		/** Platforms that support DLC */
		PLATFORM_DLCSupported		=	PLATFORM_PS3|PLATFORM_Xbox360|PLATFORM_WindowsConsole|PLATFORM_WindowsServer|PLATFORM_IPhone
	};
}


/**
 * Returns the string name of the given platform 
 *
 * @param InPlatform The platform of interest (UE3::PlatformType)
 *
 * @return The name of the platform, "" if not found
 */
FString appPlatformTypeToString(UE3::EPlatformType Platform);

/**
 * Returns the string name of the given platform 
 *
 * @param InPlatform The platform of interest (UE3::PlatformType)
 *
 * @return The name of the platform, "" if not found
 */
FString appPlatformTypeToStringEx(UE3::EPlatformType Platform);

/** 
 * Returns the list of valid platforms, in <platform 1>|<platform 2>|...|<platform N> style.
 *
 * @return The list of valid platforms
 */
FString appValidPlatformsString();

/** 
 * Returns the enumeration value for the given platform 
 *
 * @param InPlatform The platform of interest
 *
 * @return The platform type, or PLATFORM_Unknown if bad input
 */
UE3::EPlatformType appPlatformStringToType(const FString& PlatformStr);

/** 
 * @return Enumerated type for the current, compiled platform 
 */
UE3::EPlatformType appGetPlatformType();

/** 
 * @return the string name of the current, compiled platform
 */
FString appGetPlatformString();

/** 
 * @return the string name of the current, compiled platform
 */
FString appGetPlatformStringEx();

/** 
 *	Returns the path to the cooked data for the given platform
 *	
 *	@param	InPlatform		The platform of interest (UE3::PlatformType)
 *	@param	OutPath			The path to the cooked content for that platform
 *
 *	@return	UBOOL			TRUE if successful, FALSE if not
 */
UBOOL appGetCookedContentPath(UE3::EPlatformType InPlatform, FString& OutPath);

/**
 *	Get the cooked content path for the given platform.
 *
 *	@param	InPlatform		The platform of interest
 *	@param	InDLCName		The name of the DLC being cooked; empty if none
 *	@param	OutPath			Will be filled in with the cooked content path 
 *
 *	@return	UBOOL			TRUE if successful, FALSE if not.
 */
UBOOL appCookedContentPath(UE3::EPlatformType InPlatform, FString& InDLCName, FString& OutPath);

/** Which platform we're cooking for, PLATFORM_Unknown (0) if not cooking. Defined in Core.cpp. */
extern UE3::EPlatformType	GCookingTarget;

/** Which platform we're patching for, PLATFORM_Unknown (0) if not patching. Defined in Core.cpp. */
extern UE3::EPlatformType	GPatchingTarget;

/**
 * Detects whether we're running in a 64-bit operating system.
 *
 * @return	TRUE if we're running in a 64-bit operating system
 */
UBOOL appIs64bitOperatingSystem();

/*-----------------------------------------------------------------------------
	Game/ mod specific directories.
-----------------------------------------------------------------------------*/
/** @name Game or mod specific directories */
//@{
/** 
 * Returns the base directory of the "core" engine that can be shared across
 * several games or across games & mods. Shaders and base localization files
 * e.g. reside in the engine directory.
 *
 * @return engine directory
 */
FString appEngineDir();

/**
 * Returns the directory the root configuration files are located.
 *
 * @return root config directory
 */
FString appEngineConfigDir();

/** @return the root directory of the engine directory tree */
FString appRootDir();

/**
 * Returns the base directory of the current game by looking at the global
 * GGameName variable. This is usually a subdirectory of the installation
 * root directory and can be overridden on the command line to allow self
 * contained mod support.
 *
 * @return base directory
 */
FString appGameDir();

/**
 * Returns the directory the engine uses to look for the leaf ini files. This
 * can't be an .ini variable for obvious reasons.
 *
 * @return config directory
 */
FString appGameConfigDir();

/**
 * Returns the directory the engine uses to output profiling files.
 *
 * @return log directory
 */
FString appProfilingDir();

/**
 * Returns the directory the engine uses to output screenshot files.
 *
 * @return screenshot directory
 */
FString appScreenShotDir();

/**
 * Returns the directory the engine uses to output logs. This currently can't 
 * be an .ini setting as the game starts logging before it can read from .ini
 * files.
 *
 * @return log directory
 */
FString appGameLogDir();

/**
 * Returns the directory the engine should save compiled script packages to.
 */
FString appScriptOutputDir();

/**
 * Returns the file used for the script class manifest
 */
FString appScriptManifestFile();

/**
 * @return The directory for local files used in cloud emulation or support
 */
FString appCloudDir();
/**
 * @return The directory for local files that are cached
 */
FString appCacheDir();

/**
 * Returns the pathnames for the directories which contain script packages.
 *
 * @param	ScriptPackagePaths	receives the list of directory paths to use for loading script packages 
 */
void appGetScriptPackageDirectories( TArray<FString>& ScriptPackagePaths );

/**
 * Enum for the types of script packages to return from appGetScriptPackageNames
 */
enum EScriptPackageTypes
{
	/** Shared engine native packages - usually you want to specify SPT_Native */
	SPT_EngineNative	= 1,
	/** Game-specific native packages - usually you want to specify SPT_Native */
	SPT_GameNative		= 2,
	/** Engine and game editor packages */
	SPT_Editor			= 4,
	/** Game-specific non-native script packages (the kind that are cooked into seekfree maps, etc */
	SPT_NonNative		= 8,
	/** The localized portions of seekfree native packages (ie GearGame_LOC_INT) */
	SPT_SeekfreeLoc		= 16,

	/** Engine and game native packages (the usual way to get native packages) */
	SPT_Native			= SPT_EngineNative | SPT_GameNative,

	/** All script packages, but no seekfree loc */
	SPT_AllScript		= SPT_Native | SPT_Editor | SPT_NonNative,
};

/**
 * A single function to get the list of the script packages that are used by 
 * the current game (as specified by the GAMENAME #define). This is not the same as
 * EditPackages, which can be used to compile more script packages that this
 * function will return.
 *
 * @param PackageNames			The output array that will contain the package names for this game (with no extension)
 * @param ScriptTypes			The set of script packages to return - returned in the order shown in EScriptPackageTypes (except SeekfreeLoc which is put in before the corresponding package)
 * @param EngineConfigFilename	Optional name of an engine ini file (this is used when cooking for another platform)
 */
void appGetScriptPackageNames(TArray<FString>& PackageNames, UINT ScriptTypes, const TCHAR* EngineConfigFilename=NULL);

/**
 * Get a list of all packages that may be needed at startup, and could be
 * loaded async in the background when doing seek free loading
 *
 * @param PackageNames The output list of package names
 * @param EngineConfigFilename Optional engine config filename to use to lookup the package settings
 */
void appGetAllPotentialStartupPackageNames(TArray<FString>& PackageNames, const TCHAR* EngineConfigFilename=NULL, UBOOL bIsCreatingHashes=FALSE);

/**
* Generate a list of shader source files that engine needs to load
*
* @param ShaderSourceFiles - [out] list of shader source files to add to
*/
void appGetAllShaderSourceFiles( TArray<FString>& ShaderSourceFiles );

/*-----------------------------------------------------------------------------
	Timing functions.
-----------------------------------------------------------------------------*/

/** @name Timing */
//@{


//@see UnPS3.h for this being defined for PS3 land
#if !DEFINED_appCycles && !defined(PS3)
/** Return number of CPU cycles passed. Origin is arbitrary. */
DWORD appCycles();
#endif

#if STATS
#define CLOCK_CYCLES(Timer)   {Timer -= appCycles();}
#define UNCLOCK_CYCLES(Timer) {Timer += appCycles();}
#else
#define CLOCK_CYCLES(Timer)
#define UNCLOCK_CYCLES(Timer)
#endif

#if !DEFINED_appSeconds
/** Get time in seconds. Origin is arbitrary. */
DOUBLE appSeconds();
#endif

/*
 *   @return UTC offset (in minutes) from machine local time 
 */
INT appUTCOffset();

#if WITH_EDITOR
/*
 * Convert a timestamp string into seconds
 * @param DateString - a date string formatted same as appTimeStamp 
 * @return number of seconds since epoch
 */
time_t appStrToSeconds(const TCHAR* DateString);
#endif
/** Return the local time values given a time structure. */
void appSecondsToLocalTime( time_t Time, INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec );
/** Returns the system time. */
void appSystemTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec );
/** Returns a string of system time. */
FString appSystemTimeString( void );
/** Returns the UTC time. */
void appUtcTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec );
/** Returns a string of UTC time. */
FString appUtcTimeString( void );
/** Returns string timestamp.  NOTE: Only one return value is valid at a time! */
const TCHAR* appTimestamp();
/** Sleep this thread for Seconds.  0.0 means release the current timeslice to let other threads get some attention. */
void appSleep( FLOAT Seconds );
/** Sleep this thread infinitely. */
void appSleepInfinite();

//@}

/*-----------------------------------------------------------------------------
	Character type functions.
-----------------------------------------------------------------------------*/
#define UPPER_LOWER_DIFF	32

/** @name Character functions */
//@{
inline TCHAR appToUpper( TCHAR c )
{
	// compiler generates incorrect code if we try to use TEXT('char') instead of the numeric values directly
	//@hack - ideally, this would be data driven or use some sort of lookup table
	// some special cases
	switch (UNICHAR(c))
	{
		// these special chars are not 32 apart
		case 255: return 159; // diaeresis y
		case 156: return 140; // digraph ae
		case 337: return ( TCHAR )336; // Hungarian 0

		// characters within the 192 - 255 range which have no uppercase/lowercase equivalents
		case 240:
		case 208:
		case 223:
		case 247:
			return c;
	}

	if ( (c >= TEXT('a') && c <= TEXT('z')) || (c > 223 && c < 255) )
	{
		return c - UPPER_LOWER_DIFF;
	}

	// no uppercase equivalent
	return c;
}
inline TCHAR appToLower( TCHAR c )
{
	// compiler generates incorrect code if we try to use TEXT('char') instead of the numeric values directly
	// some special cases
	switch (UNICHAR(c))
	{
		// these are not 32 apart
		case 159: return 255; // diaeresis y
		case 140: return 156; // digraph ae
		case 336: return ( TCHAR )337; // Hungarian 0

		// characters within the 192 - 255 range which have no uppercase/lowercase equivalents
		case 240:
		case 208:
		case 223:
		case 247: 
			return c;
	}

	if ( (c >= 192 && c < 223) || (c >= TEXT('A') && c <= TEXT('Z')) )
	{
		return c + UPPER_LOWER_DIFF;
	}

	// no lowercase equivalent
	return c;
}
inline UBOOL appIsUpper( TCHAR cc )
{
	UNICHAR c(cc);
	// compiler generates incorrect code if we try to use TEXT('char') instead of the numeric values directly
	return (c == 159) || (c == 140)	// these are outside the standard range
		|| (c == 240) || (c == 247)	// these have no lowercase equivalents
		|| (c >= TEXT('A') && c <= TEXT('Z')) || (c >= 192 && c <= 223);
}
inline UBOOL appIsLower( TCHAR cc )
{
	UNICHAR c(cc);
	// compiler generates incorrect code if we try to use TEXT('char') instead of the numeric values directly
	return (c == 156) 								// outside the standard range
		|| (c == 215) || (c == 208) || (c== 223)	// these have no lower-case equivalents
		|| (c >= TEXT('a') && c <= TEXT('z')) || (c >= 224 && c <= 255);
}

inline UBOOL appIsAlpha( TCHAR cc )
{
	UNICHAR c(cc);
	// compiler generates incorrect code if we try to use TEXT('char') instead of the numeric values directly
	return (c >= TEXT('A') && c <= TEXT('Z')) 
		|| (c >= 192 && c <= 255)
		|| (c >= TEXT('a') && c <= TEXT('z')) 
		|| (c == 159) || (c == 140) || (c == 156);	// these are outside the standard range
}
inline UBOOL appIsDigit( TCHAR c )
{
	return c>=TEXT('0') && c<=TEXT('9');
}
inline UBOOL appIsAlnum( TCHAR c )
{
	return appIsAlpha(c) || appIsDigit(c);
}
inline UBOOL appIsWhitespace( TCHAR c )
{
	return c == TEXT(' ') || c == TEXT('\t');
}
inline UBOOL appIsLinebreak( TCHAR c )
{
	//@todo - support for language-specific line break characters
	return c == TEXT('\n');
}
inline UBOOL appIsUnderscore( TCHAR c )
{
	return c == TEXT('_');
}

/** Returns nonzero if character is a space character. */
inline UBOOL appIsSpace( TCHAR c )
{
    return( iswspace(c) != 0 );
}

inline UBOOL appIsPunct( TCHAR c )
{
	return( iswpunct( c ) != 0 );
}
//@}

/*-----------------------------------------------------------------------------
	String functions.
-----------------------------------------------------------------------------*/
/** @name String functions */
//@{

/** Returns whether the string is pure ANSI. */
UBOOL appIsPureAnsi( const TCHAR* Str );

#if !TCHAR_IS_1_BYTE
/**
 * strcpy wrapper
 *
 * @param Dest - destination string to copy to
 * @param Destcount - size of Dest in characters
 * @param Src - source string
 * @return destination string
 */
inline ANSICHAR* appStrcpy( ANSICHAR* Dest, SIZE_T DestCount, const ANSICHAR* Src )
{
#if USE_SECURE_CRT
	strcpy_s( Dest, DestCount, Src );
	return Dest;
#else
	return (ANSICHAR*)strcpy( Dest, Src );
#endif
}
#endif

/**
 * strcpy wrapper
 *
 * @param Dest - destination string to copy to
 * @param Destcount - size of Dest in characters
 * @param Src - source string
 * @return destination string
 */
inline TCHAR* appStrcpy( TCHAR* Dest, SIZE_T DestCount, const TCHAR* Src )
{
#if USE_SECURE_CRT
	_tcscpy_s( Dest, DestCount, Src );
	return Dest;
#else
	return (TCHAR*)_tcscpy( Dest, Src );
#endif
}

/**
* strcpy wrapper
* (templated version to automatically handle static destination array case)
*
* @param Dest - destination string to copy to
* @param Src - source string
* @return destination string
*/
template<SIZE_T DestCount>
inline TCHAR* appStrcpy( TCHAR (&Dest)[DestCount], const TCHAR* Src ) 
{
	return appStrcpy( Dest, DestCount, Src );
}

#if !TCHAR_IS_1_BYTE
/**
* strcpy wrapper
* (templated version to automatically handle static destination array case)
*
* @param Dest - destination string to copy to
* @param Src - source string
* @return destination string
*/
template<SIZE_T DestCount>
inline ANSICHAR* appStrcpy( ANSICHAR (&Dest)[DestCount], const ANSICHAR* Src ) 
{
	return appStrcpy( Dest, DestCount, Src );
}
#endif

/**
* strcat wrapper
*
* @param Dest - destination string to copy to
* @param Destcount - size of Dest in characters
* @param Src - source string
* @return destination string
*/
inline TCHAR* appStrcat( TCHAR* Dest, SIZE_T DestCount, const TCHAR* Src ) 
{ 
#if USE_SECURE_CRT
	_tcscat_s( Dest, DestCount, Src );
	return Dest;
#else
	return (TCHAR*)_tcscat( Dest, Src );
#endif
}

/**
* strcat wrapper
* (templated version to automatically handle static destination array case)
*
* @param Dest - destination string to copy to
* @param Src - source string
* @return destination string
*/
template<SIZE_T DestCount>
inline TCHAR* appStrcat( TCHAR (&Dest)[DestCount], const TCHAR* Src ) 
{ 
	return appStrcat( Dest, DestCount, Src );
}

/**
* strupr wrapper
*
* @param Dest - destination string to convert
* @param Destcount - size of Dest in characters
* @return destination string
*/
inline TCHAR* appStrupr( TCHAR* Dest, SIZE_T DestCount ) 
{
#if USE_SECURE_CRT
	_tcsupr_s( Dest, DestCount );
	return Dest;
#else
	return (TCHAR*)_tcsupr( Dest );
#endif
}

/**
* strupr wrapper
* (templated version to automatically handle static destination array case)
*
* @param Dest - destination string to convert
* @return destination string
*/
template<SIZE_T DestCount>
inline TCHAR* appStrupr( TCHAR (&Dest)[DestCount] ) 
{
	return appStrupr( Dest, DestCount );
}

// ANSI character versions of string manipulation functions

/**
* strcpy wrapper (ANSI version)
*
* @param Dest - destination string to copy to
* @param Destcount - size of Dest in characters
* @param Src - source string
* @return destination string
*/
inline INT appStrcmpANSI( const ANSICHAR* String1, const ANSICHAR* String2 ) 
{ 
	return strcmp(String1, String2);
}

/**
* strcpy wrapper (ANSI version)
*
* @param Dest - destination string to copy to
* @param Destcount - size of Dest in characters
* @param Src - source string
* @return destination string
*/
inline ANSICHAR* appStrcpyANSI( ANSICHAR* Dest, SIZE_T DestCount, const ANSICHAR* Src ) 
{ 
#if USE_SECURE_CRT
	strcpy_s( Dest, DestCount, Src );
	return Dest;
#else
	return (ANSICHAR*)strcpy( Dest, Src );
#endif
}

/**
* strcpy wrapper (ANSI version)
* (templated version to automatically handle static destination array case)
*
* @param Dest - destination string to copy to
* @param Src - source string
* @return destination string
*/
template<SIZE_T DestCount>
inline ANSICHAR* appStrcpyANSI( ANSICHAR (&Dest)[DestCount], const ANSICHAR* Src ) 
{ 
	return appStrcpyANSI( Dest, DestCount, Src );
}

/**
* strcat wrapper (ANSI version)
*
* @param Dest - destination string to copy to
* @param Destcount - size of Dest in characters
* @param Src - source string
* @return destination string
*/
inline ANSICHAR* appStrcatANSI( ANSICHAR* Dest, SIZE_T DestCount, const ANSICHAR* Src ) 
{ 
#if USE_SECURE_CRT
	strcat_s( Dest, DestCount, Src );
	return Dest;
#else
	return (ANSICHAR*)strcat( Dest, Src );
#endif
}

/**
* strcat wrapper (ANSI version)
*
* @param Dest - destination string to copy to
* @param Destcount - size of Dest in characters
* @param Src - source string
* @return destination string
*/
template<SIZE_T DestCount>
inline ANSICHAR* appStrcatANSI( ANSICHAR (&Dest)[DestCount], const ANSICHAR* Src ) 
{ 
	return appStrcatANSI( Dest, DestCount, Src );
}

#if !TCHAR_IS_1_BYTE
inline INT appStrlen( const ANSICHAR* String ) { return strlen( String ); }
inline INT appStricmp( const ANSICHAR* String1, const ANSICHAR* String2 )  { return _stricmp( String1, String2 ); }
#endif

inline INT appStrlen( const TCHAR* String ) { return _tcslen( String ); }
inline TCHAR* appStrstr( const TCHAR* String, const TCHAR* Find ) { return (TCHAR*)_tcsstr( String, Find ); }
inline TCHAR* appStrchr( const TCHAR* String, WORD c ) { return (TCHAR*)_tcschr( String, c ); }
inline TCHAR* appStrrchr( const TCHAR* String, WORD c ) { return (TCHAR*)_tcsrchr( String, c ); }
inline INT appStrcmp( const TCHAR* String1, const TCHAR* String2 ) { return _tcscmp( String1, String2 ); }
inline INT appStricmp( const TCHAR* String1, const TCHAR* String2 )  { return _tcsicmp( String1, String2 ); }
inline INT appStrncmp( const TCHAR* String1, const TCHAR* String2, PTRINT Count ) { return _tcsncmp( String1, String2, Count ); }
inline INT appAtoi( const TCHAR* String ) { return _tstoi( String ); }
inline SQWORD appAtoi64( const TCHAR* String ) { return _tstoi64( String ); }
inline FLOAT appAtof( const TCHAR* String ) { return _tstof( String ); }
inline DOUBLE appAtod( const TCHAR* String ) { return _tcstod( String, NULL ); }
inline INT appStrtoi( const TCHAR* Start, TCHAR** End, INT Base ) { return _tcstoul( Start, End, Base ); }
inline QWORD appStrtoi64( const TCHAR* Start, TCHAR** End, INT Base ) { return _tcstoui64( Start, End, Base ); }
inline INT appStrnicmp( const TCHAR* A, const TCHAR* B, PTRINT Count ) { return _tcsnicmp( A, B, Count ); }

/**
 * Returns a static string that is full of a variable number of characters
 * Since it is static, only one return value from a call is valid at a time.
 *
 * @param NumCharacters Number of characters to put into the string, max of 255
 * @param Char Character to put into the string
 * 
 * @return The string of NumCharacters characters.
 */
const TCHAR* appSpc( INT NumCharacters, BYTE Char );

/**
 * Returns a static string that is full of a variable number of spaces
 * that can be used to space things out, or calculate string widths
 *
 * @param NumSpaces Number of spaces to put into the string, max of 255
 * 
 * @return The string of NumSpaces spaces
 */
const TCHAR* appSpc( INT NumSpaces );

/** 
* Copy a string with length checking. Behavior differs from strncpy in that last character is zeroed. 
*
* @param Dest - destination buffer to copy to
* @param Src - source buffer to copy from
* @param MaxLen - max length of the buffer (including null-terminator)
* @return pointer to resulting string buffer
*/
TCHAR* appStrncpy( TCHAR* Dest, const TCHAR* Src, INT MaxLen );

/** 
* Concatenate a string with length checking.
*
* @param Dest - destination buffer to append to
* @param Src - source buffer to copy from
* @param MaxLen - max length of the buffer
* @return pointer to resulting string buffer
*/
TCHAR* appStrncat( TCHAR* Dest, const TCHAR* Src, INT MaxLen );

/** 
* Copy a string with length checking. Behavior differs from strncpy in that last character is zeroed. 
* (ANSICHAR version) 
*
* @param Dest - destination char buffer to copy to
* @param Src - source char buffer to copy from
* @param MaxLen - max length of the buffer (including null-terminator)
* @return pointer to resulting string buffer
*/
ANSICHAR* appStrncpyANSI( ANSICHAR* Dest, const ANSICHAR* Src, INT MaxLen );

/** Finds string in string, case insensitive, requires non-alphanumeric lead-in. */
const TCHAR* appStrfind(const TCHAR* Str, const TCHAR* Find);

/** 
 * Finds string in string, case insensitive 
 * @param Str The string to look through
 * @param Find The string to find inside Str
 * @return Position in Str if Find was found, otherwise, NULL
 */
const TCHAR* appStristr(const TCHAR* Str, const TCHAR* Find);
TCHAR* appStristr(TCHAR* Str, const TCHAR* Find);

/** String CRC. */
DWORD appStrCrc( const TCHAR* Data );
/** String CRC, case insensitive. */
DWORD appStrCrcCaps( const TCHAR* Data );
/** Ansi String CRC. */
DWORD appAnsiStrCrc( const char* Data );
/** Ansi String CRC, case insensitive. */
DWORD appAnsiStrCrcCaps( const char* Data );

/** Converts an integer to a string. */
FString appItoa( INT Num );
void appItoaAppend( INT InNum,FString &NumberString );


/** 
* Standard string formatted print. 
* @warning: make sure code using appSprintf allocates enough (>= MAX_SPRINTF) memory for the destination buffer
*/
#define MAX_SPRINTF 1024
VARARG_DECL( INT, static INT, return, appSprintf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(TCHAR* Dest), VARARG_EXTRA(Dest) );
/**
* Standard string formatted print (ANSI version).
* @warning: make sure code using appSprintf allocates enough (>= MAX_SPRINTF) memory for the destination buffer
*/
VARARG_DECL( INT, static INT, return, appSprintfANSI, VARARG_NONE, const ANSICHAR*, VARARG_EXTRA(ANSICHAR* Dest), VARARG_EXTRA(Dest) );

/** Trims spaces from an ascii string by zeroing them. */
void appTrimSpaces( ANSICHAR* String);

/**
 * Returns a pretty-string for a time given in seconds. (I.e. "4:31 min", "2:16:30 hours", etc)
 * @param Seconds	Time in seconds
 * @return			Time in a pretty formatted string
 */
FString appPrettyTime( DOUBLE Seconds );

#if USE_SECURE_CRT
#define appSSCANF	_stscanf_s
#else
#define appSSCANF	_stscanf
#endif

typedef int QSORT_RETURN;
typedef QSORT_RETURN(CDECL* QSORT_COMPARE)( const void* A, const void* B );
/** Quick sort. */
void appQsort( void* Base, INT Num, INT Width, QSORT_COMPARE Compare );

/** Case insensitive string hash function. */
inline DWORD appStrihash( const TCHAR* Data )
{
	DWORD Hash=0;
	while( *Data )
	{
		TCHAR Ch = appToUpper(*Data++);
		WORD  B  = Ch;
		Hash     = ((Hash >> 8) & 0x00FFFFFF) ^ GCRCTable[(Hash ^ B) & 0x000000FF];
		B        = Ch>>8;
		Hash     = ((Hash >> 8) & 0x00FFFFFF) ^ GCRCTable[(Hash ^ B) & 0x000000FF];
	}
	return Hash;
}

#if !TCHAR_IS_1_BYTE
/** Case insensitive string hash function. */
inline DWORD appStrihash( const ANSICHAR* Data )
{
	DWORD Hash=0;
	while( *Data )
	{
		TCHAR Ch = appToUpper(*Data++);
		WORD  B  = Ch;
		Hash     = ((Hash >> 8) & 0x00FFFFFF) ^ GCRCTable[(Hash ^ B) & 0x000000FF];
	}
	return Hash;
}
#endif
//@}

/*-----------------------------------------------------------------------------
	Parsing functions.
-----------------------------------------------------------------------------*/
/** @name Parsing functions */
//@{
/**
 * Sees if Stream starts with the named command.  If it does,
 * skips through the command and blanks past it.  Returns TRUE of match.
 * @param bParseMightTriggerExecution TRUE: Caller guarantees this is only part of parsing and no execution happens without further parsing (good for "DumpConsoleCommands").
 */
UBOOL ParseCommand( const TCHAR** Stream, const TCHAR* Match, UBOOL bParseMightTriggerExecution = TRUE );
/** Parses a name. */
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, class FName& Name );
/** Parses a DWORD. */
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, DWORD& Value );
/** Parses a globally unique identifier. */
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, class FGuid& Guid );
/** Parses a string from a text string. */
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, TCHAR* Value, INT MaxLen, UBOOL bShouldStopOnComma=TRUE );
/** Parses a byte. */
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, BYTE& Value );
/** Parses a signed byte. */
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, SBYTE& Value );
/** Parses a WORD. */
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, WORD& Value );
/** Parses a signed word. */
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, SWORD& Value );
/** Parses a floating-point value. */
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, FLOAT& Value );
/** Parses a signed double word. */
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, INT& Value );
/** Parses a string. */
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, FString& Value, UBOOL bShouldStopOnComma=TRUE );
/** Parses a quadword. */
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, QWORD& Value );
/** Parses a signed quadword. */
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, SQWORD& Value );
/** Parses a boolean value. */
UBOOL ParseUBOOL( const TCHAR* Stream, const TCHAR* Match, UBOOL& OnOff );
/** Parses an object from a text stream, returning 1 on success */
UBOOL ParseObject( const TCHAR* Stream, const TCHAR* Match, class UClass* Type, class UObject*& DestRes, class UObject* InParent );
/** Get a line of Stream (everything up to, but not including, CR/LF. Returns 0 if ok, nonzero if at end of stream and returned 0-length string. */
UBOOL ParseLine( const TCHAR** Stream, TCHAR* Result, INT MaxLen, UBOOL Exact=0 );
/** Get a line of Stream (everything up to, but not including, CR/LF. Returns 0 if ok, nonzero if at end of stream and returned 0-length string. */
UBOOL ParseLine( const TCHAR** Stream, FString& Resultd, UBOOL Exact=0 );
/** Grabs the next space-delimited string from the input stream. If quoted, gets entire quoted string. */
UBOOL ParseToken( const TCHAR*& Str, TCHAR* Result, INT MaxLen, UBOOL UseEscape );
/** Grabs the next space-delimited string from the input stream. If quoted, gets entire quoted string. */
UBOOL ParseToken( const TCHAR*& Str, FString& Arg, UBOOL UseEscape );
/** Grabs the next space-delimited string from the input stream. If quoted, gets entire quoted string. */
FString ParseToken( const TCHAR*& Str, UBOOL UseEscape );
/** Get next command.  Skips past comments and cr's. */
void ParseNext( const TCHAR** Stream );
/** Checks if a command-line parameter exists in the stream. */
UBOOL ParseParam( const TCHAR* Stream, const TCHAR* Param, UBOOL bAllowQuoted = FALSE );
#if !FINAL_RELEASE
/** Needed for the console command "DumpConsoleCommands" */
void ConsoleCommandLibrary_DumpLibrary(FExec& SubSystem, const FString& Pattern, FOutputDevice& Ar);
#endif // !FINAL_RELEASE

/** 
 * Determines which, if any, platform was specified in a string (usually command line) 
 * 
 * @param String String to look in for platform
 *
 * @return Platform specified, or PLATFORM_Unknown if not specified
 */
UE3::EPlatformType ParsePlatformType(const TCHAR* String);

//@}

//
// Parse a hex digit.
//
FORCEINLINE INT ParseHexDigit(TCHAR c)
{
	INT Result = 0;

	if (c >= '0' && c <= '9')
	{
		Result = c - '0';
	}
	else if (c >= 'a' && c <= 'f')
	{
		Result = c + 10 - 'a';
	}
	else if (c >= 'A' && c <= 'F')
	{
		Result = c + 10 - 'A';
	}
	else
	{
		Result = 0;
	}

	return Result;
}
/*-----------------------------------------------------------------------------
	Array functions.
-----------------------------------------------------------------------------*/

/** @name Array functions
 * Core functions depending on TArray and FString.
 */
//@{
void appBufferToString( FString& Result, const BYTE* Buffer, INT Size );
UBOOL appLoadFileToArray( TArray<BYTE>& Result, const TCHAR* Filename, FFileManager* FileManager=GFileManager,DWORD Flags = 0 );

enum ELoadFileHashOptions
{ 
	/** Enable the async task for verifying the hash for the file being loaded */
	LoadFileHash_EnableVerify		=1<<0,
	/** A missing hash entry should trigger an error */
	LoadFileHash_ErrorMissingHash	=1<<1
};
UBOOL appLoadFileToString( FString& Result, const TCHAR* Filename, FFileManager* FileManager=GFileManager, DWORD VerifyFlags=0, DWORD ReadFlags=0 );

#if WITH_EDITOR
/**
 *	Load the given ANSI text file to an array of strings - one FString per line of the file.
 *	Intended for use in simple text parsing actions
 *
 *	@param	InFilename			The text file to read, full path
 *	@param	InFileManager		The filemanager to use - NULL will use GFileManager
 *	@param	OutStrings			The array of FStrings to fill in
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not
 */
UBOOL appLoadANSITextFileToStrings(const TCHAR* InFilename, FFileManager* InFileManager, TArray<FString>& OutStrings);
#endif

UBOOL appSaveArrayToFile( const TArray<BYTE>& Array, const TCHAR* Filename, FFileManager* FileManager=GFileManager );
UBOOL appSaveStringToFile( const FString& String, const TCHAR* Filename, UBOOL bAlwaysSaveAsAnsi=FALSE, FFileManager* FileManager=GFileManager );
UBOOL appAppendStringToFile( const FString& String, const TCHAR* Filename, UBOOL bAlwaysSaveAsAnsi=FALSE, FFileManager* FileManager=GFileManager );
UBOOL appWriteStringToFileInternal (const FString& String, const TCHAR* Filename, UBOOL bAlwaysSaveAsAnsi=FALSE, FFileManager* FileManager=GFileManager, UBOOL Append=FALSE);
//@}

// These absolutely have to be forced inline (even when FORCEINLINE isn't forceful enough!)
//  on Mac OS X and maybe other Unixes, or the dynamic loader will resolve to our allocators
//  for the system libraries, even when Unreal isn't prepared.
#if __GNUC__ && PLATFORM_UNIX
#define OPERATOR_NEW_INLINE inline __attribute__((always_inline))
#else
#define OPERATOR_NEW_INLINE inline
#endif

//
// C++ style memory allocation.
//
OPERATOR_NEW_INLINE void* operator new( size_t Size )
#if __GNUC__ && !PS3 && !IPHONE && !ANDROID && !FLASH
throw (std::bad_alloc)
#endif
{
	return appMalloc( Size );
}
OPERATOR_NEW_INLINE void operator delete( void* Ptr )
{
	appFree( Ptr );
}

OPERATOR_NEW_INLINE void* operator new[]( size_t Size )
{
	return appMalloc( Size );
}
OPERATOR_NEW_INLINE void operator delete[]( void* Ptr )
{
	appFree( Ptr );
}

#if PS3

OPERATOR_NEW_INLINE void* operator new( size_t Size, size_t Alignment )
{
	return appMalloc( Size, Alignment );
}
OPERATOR_NEW_INLINE void* operator new[]( size_t Size, size_t Alignment )
{
	return appMalloc( Size, Alignment );
}

#endif
//@}

/** 
 * This will update the passed in FMemoryChartEntry with the platform specific data
 *
 * @param FMemoryChartEntry the struct to fill in
 **/
void appUpdateMemoryChartStats( struct FMemoryChartEntry& MemoryEntry );


/*-----------------------------------------------------------------------------
	Math.
-----------------------------------------------------------------------------*/

/**
 * Returns smallest N such that (1<<N)>=Arg.
 * Note: appCeilLogTwo(0)=0 because (1<<0)=1 >= 0.
 */
FORCEINLINE DWORD appCeilLogTwo( DWORD Arg )
{
	INT Bitmask = ((INT)(appCountLeadingZeros(Arg) << 26)) >> 31;
	return (32 - appCountLeadingZeros(Arg - 1)) & (~Bitmask);
}

/** @return Rounds the given number up to the next highest power of two. */
FORCEINLINE UINT appRoundUpToPowerOfTwo(UINT Arg)
{
	return 1 << appCeilLogTwo(Arg);
}

/**
 * Returns value based on comparand. The main purpose of this function is to avoid
 * branching based on floating point comparison which can be avoided via compiler
 * intrinsics.
 *
 * Please note that we don't define what happens in the case of NaNs as there might
 * be platform specific differences.
 *
 * @param	Comparand		Comparand the results are based on
 * @param	ValueGEZero		Return value if Comparand >= 0
 * @param	ValueLTZero		Return value if Comparand < 0
 *
 * @return	ValueGEZero if Comparand >= 0, ValueLTZero otherwise
 */
FORCEINLINE FLOAT appFloatSelect( FLOAT Comparand, FLOAT ValueGEZero, FLOAT ValueLTZero )
{
#if PS3
	return __fsels( Comparand, ValueGEZero, ValueLTZero );
#elif XBOX
	return __fself( Comparand, ValueGEZero, ValueLTZero );
#else
	return Comparand >= 0.f ? ValueGEZero : ValueLTZero;
#endif
}

/**
 * Returns value based on comparand. The main purpose of this function is to avoid
 * branching based on floating point comparison which can be avoided via compiler
 * intrinsics.
 *
 * Please note that we don't define what happens in the case of NaNs as there might
 * be platform specific differences.
 *
 * @param	Comparand		Comparand the results are based on
 * @param	ValueGEZero		Return value if Comparand >= 0
 * @param	ValueLTZero		Return value if Comparand < 0
 *
 * @return	ValueGEZero if Comparand >= 0, ValueLTZero otherwise
 */
FORCEINLINE DOUBLE appFloatSelect( DOUBLE Comparand, DOUBLE ValueGEZero, DOUBLE ValueLTZero )
{
#if PS3
	return __fsel( Comparand, ValueGEZero, ValueLTZero );
#elif XBOX
	return __fsel( Comparand, ValueGEZero, ValueLTZero );
#else
	return Comparand >= 0.f ? ValueGEZero : ValueLTZero;
#endif
}

/*-----------------------------------------------------------------------------
	MD5 functions.
-----------------------------------------------------------------------------*/

/** @name MD5 functions */
//@{
//
// MD5 Context.
//
struct FMD5Context
{
	DWORD state[4];
	DWORD count[2];
	BYTE buffer[64];
};

//
// MD5 functions.
//!!it would be cool if these were implemented as subclasses of
// FArchive.
//
void appMD5Init( FMD5Context* context );
void appMD5Update( FMD5Context* context, const BYTE* input, INT inputLen );
void appMD5Final( BYTE* digest, FMD5Context* context );
void appMD5Transform( DWORD* state, const BYTE* block );
void appMD5Encode( BYTE* output, const DWORD* input, INT len );
void appMD5Decode( DWORD* output, const BYTE* input, INT len );
//@}


/*-----------------------------------------------------------------------------
	SHA-1 functions.
-----------------------------------------------------------------------------*/

/*
 *	NOTE:
 *	100% free public domain implementation of the SHA-1 algorithm
 *	by Dominik Reichl <dominik.reichl@t-online.de>
 *	Web: http://www.dominik-reichl.de/
 */


typedef union
{
	BYTE  c[64];
	DWORD l[16];
} SHA1_WORKSPACE_BLOCK;

/** This divider string is beween full file hashes and script hashes */
#define HASHES_SHA_DIVIDER "+++"

/** Stores an SHA hash generated by FSHA1. */
class FSHAHash
{
public:
	BYTE Hash[20];

	friend UBOOL operator==(const FSHAHash& X, const FSHAHash& Y)
	{
		return appMemcmp(&X.Hash, &Y.Hash, sizeof(X.Hash)) == 0;
	}

	friend UBOOL operator!=(const FSHAHash& X, const FSHAHash& Y)
	{
		return appMemcmp(&X.Hash, &Y.Hash, sizeof(X.Hash)) != 0;
	}

	friend FArchive& operator<<( FArchive& Ar, FSHAHash& G );
};

class FSHA1
{
public:

	// Constructor and Destructor
	FSHA1();
	~FSHA1();

	DWORD m_state[5];
	DWORD m_count[2];
	DWORD __reserved1[1];
	BYTE  m_buffer[64];
	BYTE  m_digest[20];
	DWORD __reserved2[3];

	void Reset();

	// Update the hash value
	void Update(const BYTE *data, DWORD len);

	// Finalize hash and report
	void Final();

	// Report functions: as pre-formatted and raw data
	void GetHash(BYTE *puDest);

	/**
	 * Calculate the hash on a single block and return it
	 *
	 * @param Data Input data to hash
	 * @param DataSize Size of the Data block
	 * @param OutHash Resulting hash value (20 byte buffer)
	 */
	static void HashBuffer(const void* Data, DWORD DataSize, BYTE* OutHash);

	/**
	 * Shared hashes.sha reading code (each platform gets a buffer to the data,
	 * then passes it to this function for processing)
	 *
	 * @param Buffer Contents of hashes.sha (probably loaded from an a section in the executable)
	 * @param BufferSize Size of Buffer
	 * @param bDuplicateKeyMemory If Buffer is not always loaded, pass TRUE so that the 20 byte hashes are duplicated 
	 */
	static void InitializeFileHashesFromBuffer(BYTE* Buffer, INT BufferSize, UBOOL bDuplicateKeyMemory=FALSE);

	/**
	 * Gets the stored SHA hash from the platform, if it exists. This function
	 * must be able to be called from any thread.
	 *
	 * @param Pathname Pathname to the file to get the SHA for
	 * @param Hash 20 byte array that receives the hash
	 * @param bIsFullPackageHash TRUE if we are looking for a full package hash, instead of a script code only hash
	 *
	 * @return TRUE if the hash was found, FALSE otherwise
	 */
	static UBOOL GetFileSHAHash(const TCHAR* Pathname, BYTE Hash[20], UBOOL bIsFullPackageHash=TRUE);

private:
	// Private SHA-1 transformation
	void Transform(DWORD *state, const BYTE *buffer);

	// Member variables
	BYTE m_workspace[64];
	SHA1_WORKSPACE_BLOCK *m_block; // SHA1 pointer to the byte array above

	/** Global map of filename to hash value, filled out in InitializeFileHashesFromBuffer */
	static TMap<FString, BYTE*> FullFileSHAHashMap;

	/** Global map of filename to hash value, but for script-only SHA hashes */
	static TMap<FString, BYTE*> ScriptSHAHashMap;
};

/**
 * Callback that is called if the asynchronous SHA verification fails
 * This will be called from a pooled thread.
 *
 * NOTE: Each platform is expected to implement this!
 *
 * @param FailedPathname Pathname of file that failed to verify
 * @param bFailedDueToMissingHash TRUE if the reason for the failure was that the hash was missing, and that was set as being an error condition
 */
void appOnFailSHAVerification(const TCHAR* FailedPathname, UBOOL bFailedDueToMissingHash);

/*-----------------------------------------------------------------------------
	Compression.
-----------------------------------------------------------------------------*/

/**
 * Flags controlling [de]compression
 */
enum ECompressionFlags
{
	/** No compression																*/
	COMPRESS_None					= 0x00,
	/** Compress with ZLIB															*/
	COMPRESS_ZLIB 					= 0x01,
	/** Compress with LZO															*/
	COMPRESS_LZO 					= 0x02,
	/** Compress with LZX															*/
	COMPRESS_LZX					= 0x04,
	/** Prefer compression that compresses smaller (ONLY VALID FOR COMPRESSION)		*/
	COMPRESS_BiasMemory 			= 0x10,
	/** Prefer compression that compresses faster (ONLY VALID FOR COMPRESSION)		*/
	COMPRESS_BiasSpeed				= 0x20,
	/** If this flag is present, decompression will not happen on the SPUs.			*/
	COMPRESS_ForcePPUDecompressZLib	= 0x80
};

// Defines for default compression on platform.
#define COMPRESS_DefaultXbox360		COMPRESS_LZX
#define COMPRESS_DefaultPS3			COMPRESS_ZLIB
#if WITH_LZO
#define COMPRESS_DefaultPC			COMPRESS_LZO
#else
#define COMPRESS_DefaultPC			COMPRESS_ZLIB
#endif

// Define global current platform default to current platform.
#if XBOX
#define COMPRESS_Default			COMPRESS_DefaultXbox360
#elif PS3
#define COMPRESS_Default			COMPRESS_ZLIB
#else
#define COMPRESS_Default			COMPRESS_DefaultPC
#endif

/** Base compression method to use. Fixed on console but cooker needs to change this depending on target. */
extern ECompressionFlags GBaseCompressionMethod;

/** Compression Flag Masks **/
/** mask out compression type flags */
#define COMPRESSION_FLAGS_TYPE_MASK		0x0F
/** mask out compression type */
#define COMPRESSION_FLAGS_OPTIONS_MASK	0xF0


/**
 * Chunk size serialization code splits data into. The loading value CANNOT be changed without resaving all
 * compressed data which is why they are split into two separate defines.
 */
#define LOADING_COMPRESSION_CHUNK_SIZE_PRE_369  32768
#define LOADING_COMPRESSION_CHUNK_SIZE			131072
#define SAVING_COMPRESSION_CHUNK_SIZE			LOADING_COMPRESSION_CHUNK_SIZE

/** Maximum allowed size of an uncompressed buffer passed to appCompressMemory or appUncompressMemory. */
const static UINT MaxUncompressedSize = 256 * 1024;

/**
 * Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
 * buffer. Updates CompressedSize with size of compressed data. Compression controlled by the passed in flags.
 *
 * @param	Flags						Flags to control what method to use and optionally control memory vs speed
 * @param	CompressedBuffer			Buffer compressed data is going to be written to
 * @param	CompressedSize	[in/out]	Size of CompressedBuffer, at exit will be size of compressed data
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @return TRUE if compression succeeds, FALSE if it fails because CompressedBuffer was too small or other reasons
 */
UBOOL appCompressMemory( ECompressionFlags Flags, void* CompressedBuffer, INT& CompressedSize, const void* UncompressedBuffer, INT UncompressedSize );

/**
 * Thread-safe abstract decompression routine. Uncompresses memory from compressed buffer and writes it to uncompressed
 * buffer. UncompressedSize is expected to be the exact size of the data after decompression.
 *
 * @param	Flags						Flags to control what method to use to decompress
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @param	CompressedBuffer			Buffer compressed data is going to be read from
 * @param	CompressedSize				Size of CompressedBuffer data in bytes
 * @param	bIsSourcePadded		Whether the source memory is padded with a full cache line at the end
 * @return TRUE if compression succeeds, FALSE if it fails because CompressedBuffer was too small or other reasons
 */
UBOOL appUncompressMemory( ECompressionFlags Flags, void* UncompressedBuffer, INT UncompressedSize, const void* CompressedBuffer, INT CompressedSize, UBOOL bIsSourcePadded = FALSE );

/*-----------------------------------------------------------------------------
	Game Name.
-----------------------------------------------------------------------------*/

const TCHAR* appGetGameName();

#if WITH_GAMESPY
/**
 * Returns the game name to use with GameSpy
 */
const TCHAR* appGetGameSpyGameName(void);

/**
 * Returns the secret key used by this game
 */
const TCHAR* appGetGameSpySecretKey(void);
#endif

/** Returns the title id of this game */
DWORD appGetTitleId(void);

#if WITH_STEAMWORKS
/**
 * Gets the Steam appid for this game
 *
 * @return	The appid of the game, or INDEX_NONE if it cannot be retrieved
 */
INT appGetSteamworksAppId();
#endif


/*-----------------------------------------------------------------------------
	Misc.
-----------------------------------------------------------------------------*/

/**
 * Converts a buffer to a string by hex-ifying the elements
 *
 * @param SrcBuffer the buffer to stringify
 * @param SrcSize the number of bytes to convert
 *
 * @return the blob in string form
 */
FString appBlobToString(const BYTE* SrcBuffer,const DWORD SrcSize);

/**
 * Converts a string into a buffer
 *
 * @param DestBuffer the buffer to fill with the string data
 * @param DestSize the size of the buffer in bytes (must be at least string len / 2)
 *
 * @return TRUE if the conversion happened, FALSE otherwise
 */
UBOOL appStringToBlob(const FString& Source,BYTE* DestBuffer,const DWORD DestSize);

/**
 * Platform specific function for adding a named event that can be viewed in PIX
 */
void appBeginNamedEvent(const FColor& Color,const TCHAR* Text);

/**
 * Platform specific function for closing a named event that can be viewed in PIX
 */
void appEndNamedEvent();

#endif // __UNFILE_H__
