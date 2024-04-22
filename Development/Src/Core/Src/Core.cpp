/*=============================================================================
	Core.cpp: Unreal core.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"
#include "IConsoleManager.h"

/** Returns a unique Runtime ID, always >  0. */
QWORD appCreateRuntimeUID()
{
	return GRuntimeUIDCounter++;
};

/*-----------------------------------------------------------------------------
	Temporary startup objects.
-----------------------------------------------------------------------------*/

// Error file manager.
class FFileManagerError : public FFileManager
{
public:
	FArchive* CreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
		{appErrorf(TEXT("Called FFileManagerError::CreateFileReader")); return 0;}
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error, INT MaxFileSize )
		{appErrorf(TEXT("Called FFileManagerError::CreateFileWriter")); return 0;}
	INT UncompressedFileSize( const TCHAR* Filename )
		{return -1;}
	UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
		{return FALSE;}
	UBOOL IsReadOnly( const TCHAR* Filename )
		{return FALSE;}
	DWORD Copy( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0, FCopyProgress* Progress=NULL )
		{return COPY_MiscFail;}
	UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 )
		{return FALSE;}
	INT FindAvailableFilename( const TCHAR* Base, const TCHAR* Extension, FString& OutFilename, INT StartVal=-1 )
		{return -1;}
	UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )
		{return FALSE;}
	UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )
		{return FALSE;}
	void FindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories )
		{}
	DOUBLE GetFileAgeSeconds( const TCHAR* Filename )
		{return -1.0;}
	DOUBLE GetFileTimestamp( const TCHAR* Filename )
		{return -1.0;}
	UBOOL SetDefaultDirectory()
		{return FALSE;}
	UBOOL SetCurDirectory(const TCHAR* Directory)
		{return FALSE;}
	FString GetCurrentDirectory()
		{return TEXT("");}
	UBOOL GetTimestamp( const TCHAR* Filename, FTimeStamp& Timestamp )
		{ return FALSE; }
	UBOOL TouchFile(const TCHAR* Filename)
		{ return FALSE; }
	INT FileSize( const TCHAR* Filename )
		{ return INDEX_NONE; }

protected:
	virtual INT InternalFileSize(const TCHAR* Filename)
		{ return -1; }
} FileError;

// Exception thrower.
class FThrowOut : public FOutputDevice
{
public:
	void Serialize( const TCHAR* V, EName Event )
	{
#if EXCEPTIONS_DISABLED
		appDebugBreak();
#else
		throw( V );
#endif
	}
} ThrowOut;

// Null output device.
FOutputDeviceNull NullOut;

// Dummy saver.
class FArchiveDummySave : public FArchive
{
public:
	FArchiveDummySave() { ArIsSaving = 1; }
} GArchiveDummySave;

/** Global output device redirector, can be static as FOutputDeviceRedirector explicitly empties TArray on TearDown */
static FOutputDeviceRedirector LogRedirector;

FString FFileManager::ConvertToRelativePath( const TCHAR* Filename )
{
	appErrorf(TEXT("Currently not implemented for this platform."));
	return FString();
}

FString FFileManager::ConvertToAbsolutePath( const TCHAR* Filename )
{
	appErrorf(TEXT("Currently not implemented for this platform."));
	return FString();
}

FString FFileManager::ConvertAbsolutePathToUserPath(const TCHAR* AbsolutePath)
{
	appErrorf(TEXT("Currently not implemented for this platform."));
	return FString();
}

/**
 *	Threadsafely converts the platform-independent Unreal filename into platform-specific full path.
 *
 *	@param	Platform-independent Unreal filename
 *	@return	Platform-dependent full filepath
 **/
FString FFileManager::GetPlatformFilepath( const TCHAR* Filename )
{
	return FString(Filename);
}

DWORD FObjectPropagator::Paused = 0;
FObjectPropagator		NullPropagator; // a Null propagator to suck up unwanted propagations
void FObjectPropagator::SetPropagator(FObjectPropagator* InPropagator)
{
	// if we aren't passing one in for real, just clear it out
	if (!InPropagator)
	{
		ClearPropagator();
		return;
	}

	// disconnect the existing propagator
	GObjectPropagator->Disconnect();

	// let it connect if it needs
	if (!InPropagator->Connect())
	{
		// if it failed to connect, we have no propagator
		GObjectPropagator = &NullPropagator;
	}
	else
	{
		// set the global propagator to the new propagator
		GObjectPropagator = InPropagator;
	}

}
void FObjectPropagator::ClearPropagator()
{
	// disconnect the existing propagator
	GObjectPropagator->Disconnect();

	// set the propagator to the empty propagator that does nothing
	GObjectPropagator = &NullPropagator;
}

void FObjectPropagator::Pause()
{
	// we use a Puased stack for nested Pause/UnPause calls
	Paused++;
}

void FObjectPropagator::Unpause()
{
	// don't let us Unpause too many times
	if (Paused)
	{
		Paused--;
	}
}

void FNotifyHook::NotifyPreChange( void* Src, FEditPropertyChain* PropertyAboutToChange )
{
	NotifyPreChange( Src, PropertyAboutToChange != NULL && PropertyAboutToChange->Num() > 0 ? PropertyAboutToChange->GetActiveNode()->GetValue() : NULL );
}

void FNotifyHook::NotifyPostChange( void* Src, FEditPropertyChain* PropertyThatChanged )
{
	NotifyPostChange( Src, PropertyThatChanged != NULL && PropertyThatChanged->Num() > 0 ? PropertyThatChanged->GetActiveNode()->GetValue() : NULL );
}


/**
* FFileManger::timestamp implementation
*/
INT FFileManager::FTimeStamp::GetJulian( void ) const
{
	return  Day - 32075L +
		1461L*(Year  + 4800L + ((Month+1) - 14L)/12L)/4L		+ 
		367L*((Month+1) - 2L - (((Month+1) - 14L)/12L)*12L)/12L -
		3L*((Year + 4900L  + ((Month+1) - 14L)/12L)/100L)/4L;
}


/*
* @return Seconds of the day so far
*/
INT FFileManager::FTimeStamp::GetSecondOfDay( void ) const
{
	return Second + Minute*60 + Hour*60*60;
}


/*
* @return Whether this time stamp is older than Other
*/
UBOOL FFileManager::FTimeStamp::operator< ( const FFileManager::FTimeStamp& Other ) const
{
	const INT J = GetJulian();
	const INT OJ = Other.GetJulian();

	if (J<OJ)
	{
		return TRUE;
	}
	else if (J>OJ)
	{
		return FALSE;
	}
	else
	{
		// Have to compare times
		if (GetSecondOfDay() < Other.GetSecondOfDay())
		{
			return TRUE;
		}
	}

	return FALSE;
}

/*
* @return Whether this time stamp is newer than Other
*/
UBOOL FFileManager::FTimeStamp::operator> ( const FFileManager::FTimeStamp& Other ) const
{
	const INT J = GetJulian();
	const INT OJ = Other.GetJulian();

	if (J>OJ)
	{
		return TRUE;
	}
	else if (J<OJ)
	{
		return FALSE;
	}
	else
	{
		// Have to compare times
		if (GetSecondOfDay() > Other.GetSecondOfDay())
		{
			return TRUE;
		}
	}

	return FALSE;
}

/*
* @return Whether this time stamp is equal to Other
*/
UBOOL FFileManager::FTimeStamp::operator==( const FFileManager::FTimeStamp& Other ) const
{
	return ((Year  ==Other.Year  ) &&
		(Day   ==Other.Day   ) &&
		(Month ==Other.Month ) &&
		(Hour  ==Other.Hour  ) &&
		(Minute==Other.Minute) &&
		(Second==Other.Second));
}

/*
* @return Whether this time stamp is above or equal to Other
*/
UBOOL FFileManager::FTimeStamp::operator>=( const FFileManager::FTimeStamp& Other ) const
{
	if (operator ==(Other))
	{
		return TRUE;
	}

	return operator >(Other);
}

/*
* @return Whether this time stamp is below or equal to Other
*/
UBOOL FFileManager::FTimeStamp::operator<=( const FFileManager::FTimeStamp& Other ) const
{
	if (operator ==(Other))
	{
		return TRUE;
	}

	return operator <(Other);
}

/**
  * Converts a timestamp to a string.
  *
  * @param Timestamp The timestamp to convert to a string
  * @param [out] Output The FString to place the formatted timestamp in
  */
void FFileManager::FTimeStamp::TimestampToFString(const FFileManager::FTimeStamp& Timestamp, FString& Output)
{
	Output = FString::Printf(TEXT("%04d-%02d-%02d %02d:%02d:%02d"),
		Timestamp.Year,
		Timestamp.Month+1,
		Timestamp.Day,
		Timestamp.Hour,
		Timestamp.Minute,
		Timestamp.Second);
}

/**
  * Converts a string to a timestamp.
  * Note: No error checking is done and this function should only be called on the output from TimestampToFString.
  *
  * @param TimestampAsString A string containing a timestamp, created by TimestampToFString()
  * @param [out] Output	The timestamp represented by the input string
  */
void FFileManager::FTimeStamp::FStringToTimestamp(const FString& TimestampAsString, FFileManager::FTimeStamp& Output)
{
	// Pull the timestamp from the user readable string.  
	appMemzero(&Output, sizeof(Output));
	Output.Year   = appAtoi( *( TimestampAsString.Mid(0,4) ) );
	Output.Month  = appAtoi( *( TimestampAsString.Mid(5,2) ) ) - 1;
	Output.Day    = appAtoi( *( TimestampAsString.Mid(8,2) ) );
	Output.Hour   = appAtoi( *( TimestampAsString.Mid(11,2) ) );
	Output.Minute = appAtoi( *( TimestampAsString.Mid(14,2) ) );
	Output.Second = appAtoi( *( TimestampAsString.Mid(17,2) ) );    
}

/** Generate an FTimestamp that represents the current system time. */
void FFileManager::FTimeStamp::GetTimestampFromCurrentTime(FFileManager::FTimeStamp& Output)
{
	appMemzero(&Output, sizeof(Output));

	INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
	appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );

	Output.Year   = Year;
	Output.Month  = Month-1;
	Output.Day    = Day;
	Output.Hour   = Hour;
	Output.Minute = Min;
	Output.Second = Sec; 
}

/** A function that does nothing. Allows for a default behavior for callback function pointers. */
void appNoop()
{
}


/*-----------------------------------------------------------------------------
	FSelfRegisteringExec implementation.
-----------------------------------------------------------------------------*/

/** Constructor, registering this instance. */
FSelfRegisteringExec::FSelfRegisteringExec()
{
	RegisteredExecs.AddItem( this );
}

/** Destructor, unregistering this instance. */
FSelfRegisteringExec::~FSelfRegisteringExec()
{
	verify( RegisteredExecs.RemoveItem( this ) == 1 );
}

TArrayNoInit<FSelfRegisteringExec*> FSelfRegisteringExec::RegisteredExecs;

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

FMemStack				GMainThreadMemStack(65536, TRUE, FALSE);					/* Global memory stack for the game thread */
FOutputDeviceRedirectorBase* GLog						= &LogRedirector;			/* Regular logging */
FOutputDeviceError*		GError							= NULL;						/* Critical errors */
FOutputDevice*			GNull							= &NullOut;					/* Log to nowhere */
FOutputDevice*			GThrow							= &ThrowOut;				/* Exception thrower */
FFeedbackContext*		GWarn							= NULL;						/* User interaction and non critical warnings */
FConfigCacheIni*		GConfig							= NULL;						/* Configuration database cache */
FTransactionBase*		GUndo							= NULL;						/* Transaction tracker, non-NULL when a transaction is in progress */
FOutputDeviceConsole*	GLogConsole						= NULL;						/* Console log hook */
FMalloc*				GMalloc							= NULL;						/* Memory allocator */
FFileManager*			GFileManager					= NULL;						/* File manager */
FCallbackEventObserver*	GCallbackEvent					= NULL;						/* Used for making event callbacks into UnrealEd */
FCallbackQueryDevice*	GCallbackQuery					= NULL;						/* Used for making queries into UnrealEd */
USystem*				GSys							= NULL;						/* System control code */
UProperty*				GProperty						= NULL;						/* Property for UnrealScript interpretter */
/** Points to the UProperty currently being serialized									*/
UProperty*				GSerializedProperty				= NULL;
/** Points to the main UObject currently being serialized								*/
UObject*				GSerializedObject				= NULL;
/** Points to the main Linker currently being used for serialization by CreateImports()	*/
ULinkerLoad*			GSerializedImportLinker			= NULL;
/** Points to the main PackageLinker currently being serialized */
ULinkerLoad*			GSerializedPackageLinker		= NULL;
/** The main Import Index currently being used for serialization by CreateImports()		*/
INT						GSerializedImportIndex			= INDEX_NONE;
/** Points to the most recently used Linker for serialization by CreateExport() */
ULinkerLoad*			GSerializedExportLinker			= NULL;
/** The most recently used export Index for serialization by CreateExport() */
INT						GSerializedExportIndex			= INDEX_NONE;
BYTE*					GPropAddr						= NULL;						/* Property address for UnrealScript interpreter */
UObject*				GPropObject						= NULL;						/* Object with Property for UnrealScript interpreter */
DWORD					GRuntimeUCFlags					= 0;						/* Property for storing flags between calls to bytecode functions */
class UPropertyWindowManager*	GPropertyWindowManager	= NULL;						/* Manages and tracks property editing windows */
#if !CONSOLE
TCHAR					GErrorHist[16384]				= TEXT("");					/* For building call stack text dump in guard/unguard mechanism */
#endif
TCHAR					GYes[64]						= TEXT("Yes");				/* Localized "yes" text */
TCHAR					GNo[64]							= TEXT("No");				/* Localized "no" text */
TCHAR					GTrue[64]						= TEXT("True");				/* Localized "true" text */
TCHAR					GFalse[64]						= TEXT("False");			/* Localized "false" text */
TCHAR					GNone[64]						= TEXT("None");				/* Localized "none" text */
DOUBLE					GSecondsPerCycle				= 0.0;						/* Seconds per CPU cycle for this PC */
DWORD					GUglyHackFlags					= 0;						/* Flags for passing around globally hacked stuff */
#if !CONSOLE
UBOOL					GIsEditor						= FALSE;					/* Whether engine was launched for editing */
UBOOL					GIsImportingT3D					= FALSE;					/* Whether editor is importing T3D */
UBOOL					GIsUCC							= FALSE;					/* Is UCC running? */
UBOOL					GIsUCCMake						= FALSE;					/* Are we rebuilding script via ucc make? */
UBOOL					GIsWatchingEndLoad				= FALSE;					/* Whether we should fire notification events when objects are loaded (see GCallbackEvent). */
UBOOL					GIsTransacting					= FALSE;					/* TRUE if there is an undo/redo operation in progress. */
FLOAT					GVolumeMultiplier				= 1.0f;						/* Use to silence the app when it loses focus */
#endif // CONSOLE
UBOOL					GEdSelectionLock				= FALSE;					/* Are selections locked? (you can't select/deselect additional actors) */
UBOOL					GIsClient						= FALSE;					/* Whether engine was launched as a client */
UBOOL					GIsServer						= FALSE;					/* Whether engine was launched as a server, true if GIsClient */
UBOOL					GIsCriticalError				= FALSE;					/* An appError() has occured */
UBOOL					GIsStarted						= FALSE;					/* Whether execution is happening from within main()/WinMain() */
UBOOL					GIsGuarded						= FALSE;					/* Whether execution is happening within main()/WinMain()'s try/catch handler */
UBOOL					GIsRunning						= FALSE;					/* Whether execution is happening within MainLoop() */
UBOOL					GIsGarbageCollecting			= FALSE;					/* Whether we are inside garbage collection */
UBOOL					GIsReplacingObject				= FALSE;					/* Whether we are currently in-place replacing an object */
#if XBOX
UBOOL					GSetThreadNames					= FALSE;					/* On the xbox setting thread names messes up the XDK COM API that UnrealConsole uses. Have them off by default. */
#endif
/** This determines if we should pop up any dialogs.  If Yes then no popping up dialogs.					*/
UBOOL					GIsUnattended					= FALSE;
/** This specifies whether the engine was launched as a build machine process								*/
UBOOL					GIsBuildMachine					= FALSE;
/** This determines if we should output any log text.  If Yes then no log text should be emitted.			*/
UBOOL					GIsSilent						= FALSE;
/**
 * Used by non-UObject ctors/dtors of UObjects with multiple inheritance to
 * determine whether we're constructing/destructing the class default object
 */
UBOOL					GIsAffectingClassDefaultObject	= FALSE;
UBOOL					GIsSlowTask						= FALSE;					/* Whether there is a slow task in progress */
UBOOL					GSlowTaskOccurred				= FALSE;					/* Whether a slow task began last tick*/
UBOOL					GIsRequestingExit				= FALSE;					/* Indicates that MainLoop() should be exited at the end of the current iteration */
FGlobalMath				GMath;														/* Math code */
FArchive*				GDummySave						= &GArchiveDummySave;		/* No-op save archive */
/** Archive for serializing arbitrary data to and from memory												*/
FReloadObjectArc*		GMemoryArchive					= NULL;
TArray<FEdLoadError>	GEdLoadErrors;												/* For keeping track of load errors in the editor */
UDebugger*				GDebugger						= NULL;						/* Unrealscript Debugger */
UBOOL					GTreatScriptWarningsFatal		= 0;						/* Whether we want to assert on accessed none or not */
UBOOL					GScriptStackForScriptWarning	= 0;						/* Whether we want to log a script stack on warning or not */
UBOOL					GIsBenchmarking					= FALSE;						/* Whether we are in benchmark mode or not */
UBOOL					GAreScreenMessagesEnabled		= TRUE;						/* Whether onscreen warnings/messages are enabled */
UBOOL					GScreenMessagesRestoreState		= FALSE;					/* Used to restore state after a screenshot */
UBOOL					GIsDrawingStats					= FALSE;					/* Whether we are currently drawing stats to the screen */
INT						GColorGrading					= 1;						/* see "ColorGrading" console command help */
FLOAT					GBloomWeightSmall				= -1;						/* see "BloomWeightSmall" console command help */
FLOAT					GBloomWeightLarge				= -1;						/* see "GBloomWeightLarge" console command help */
UBOOL                   GDrawGFx                        = TRUE;                     /* TRUE: Render GFx, FALSE: Disable GFx rendering */
UBOOL					GIsDumpingMovie					= 0;						/* Whether we are dumping screenshots */
UBOOL					GIsHighResScreenshot			= FALSE;					/* Whether we're capturing a high resolution shot */
UBOOL					GIsDumpingTileShotMovie			= 0;						/* Whether we are dumping tiledshot screenshots */
UBOOL					GForceLogFlush					= 0;						/* Whether to force output after each line written to the log */
UBOOL					GForceSoundRecook				= 0;						/* Whether to force a recook of all loaded sounds */
QWORD					GMakeCacheIDIndex				= 0;						/* Cache ID */
#if UDK
FGuid					GModGUID;													/* GUID of the currently running mod */
#endif // UDK
TCHAR					GConfigSubDirectory[1024]		= TEXT("");					/* optional subdirectory off default config dir for loading/saving .ini files */
TCHAR					GEngineIni[1024]				= TEXT("");					/* Engine ini file */
TCHAR					GEditorIni[1024]				= TEXT("");					/* Editor ini file */
TCHAR					GEditorUserSettingsIni[1024]	= TEXT("");					/* Editor User Settings ini file */
TCHAR					GSystemSettingsIni[1024]			= TEXT("");
TCHAR					GLightmassIni[1024]				= TEXT("");					/* Lightmass settings ini file */
TCHAR					GInputIni[1024]					= TEXT("");					/* Input ini file */
TCHAR					GGameIni[1024]					= TEXT("");					/* Game ini file */
TCHAR					GUIIni[1024]					= TEXT("");
TCHAR					GDefaultEngineIni[1024]			= TEXT("");					/* Default Engine ini file */
TCHAR					GDefaultEditorIni[1024]			= TEXT("");					/* Default Editor ini file */
TCHAR					GDefaultEditorUserSettingsIni[1024]	= TEXT("");				/* Default Editor User Settings ini file */
TCHAR					GDefaultSystemSettingsIni[1024]		= TEXT("");
TCHAR					GDefaultLightmassIni[1024]		= TEXT("");					/* Default Lightmass settings ini file */
TCHAR					GDefaultInputIni[1024]			= TEXT("");					/* Default Input ini file */
TCHAR					GDefaultGameIni[1024]			= TEXT("");					/* Default Game ini file */
TCHAR					GDefaultUIIni[1024]				= TEXT("");
FLOAT					GNearClippingPlane				= 0.5f;					/* Near clipping plane */
/** Timestep if a fixed delta time is wanted.																*/
DOUBLE					GFixedDeltaTime					= 1 / 30.f;
/** Current delta time in seconds.																			*/
DOUBLE					GDeltaTime						= 1 / 30.f;
/** Current unclamped delta time in seconds.																*/
DOUBLE					GUnclampedDeltaTime				= 1 / 30.f;
/* Current time																								*/
DOUBLE					GCurrentTime					= 0;						
/* Last GCurrentTime																						*/
DOUBLE					GLastTime						= 0;						
/* Seed for appSRand																						*/
INT						GSRandSeed						= 0;						
/* Counter assigning runtime unique IDs. Initialized to 1 here for readability.								*/
QWORD					GRuntimeUIDCounter				= 1;						
UBOOL					GExitPurge						= FALSE;
/** Game name, used for base game directory and ini among other things										*/
TCHAR					GGameName[64]					= TEXT("Example");
/** The current SentinelRunID																				*/
INT                     GSentinelRunID                  = -1;
//@{
//@script patcher
/** whether we're currently generating a script patch														*/
UBOOL					GIsScriptPatcherActive			= FALSE;
/** suffix to append to top-level package names when running the script patcher								*/
FString					GScriptPatchPackageSuffix		= TEXT("");
//@}
/** Exec handler for game debugging tool, allowing commands like "editactor", ...							*/
FExec*					GDebugToolExec;
/** Whether we are currently cooking.																		*/
UBOOL					GIsCooking						= FALSE;
/** Whether we are currently cooking for a demo build.														*/
UBOOL					GIsCookingForDemo				= FALSE;
/** Which platform we're cooking for, PLATFORM_Unknown (0) if not cooking.									*/
UE3::EPlatformType		GCookingTarget					= UE3::PLATFORM_Unknown;
/** Which platform we're patching for, PLATFORM_Unknown (0) if not patching.								*/
UE3::EPlatformType		GPatchingTarget					= UE3::PLATFORM_Unknown;
/** Whether we're currently in the async loading codepath or not											*/
UBOOL					GIsAsyncLoading					= FALSE;
/** Whether the editor is currently loading a map or not													*/
UBOOL					GIsEditorLoadingMap				= FALSE;
/** The global object property propagator,with a Null version so nothing crashes							*/
FObjectPropagator*		GObjectPropagator				= &NullPropagator;
/** Whether to allow execution of Epic internal code like e.g. TTP integration, ...							*/
UBOOL					GIsEpicInternal					= FALSE;
/** Whether GWorld points to the play in editor world														*/
UBOOL					GIsPlayInEditorWorld			= FALSE;
/** TRUE if a normal or PIE game is active (basically !GIsEditor || GIsPlayInEditorWorld)					*/
UBOOL					GIsGame							= FALSE;
/** TRUE if using fast PIE world/level duplication instead of serializing out to disk						*/
UBOOL					GUseFastPIE						= FALSE;
/** TRUE if running PC w/ -simmobile																		*/
UBOOL					GIsSimMobile					= FALSE;
/** TRUE if the runtime needs textures to be powers of two													*/
UBOOL					GPlatformNeedsPowerOfTwoTextures = FALSE;
/** TRUE if we're associating a level with the world, FALSE otherwise										*/
UBOOL					GIsAssociatingLevel				= FALSE;
/** Global IO manager																						*/
FIOManager*				GIOManager						= NULL;
/** Time at which appSeconds() was first initialized (before main)											*/
DOUBLE					GStartTime						= appInitTiming();
/** System time at engine init.																				*/
FString					GSystemStartTime;
/** Whether to use the seekfree package map over the regular linker based one.								*/
UBOOL					GUseSeekFreePackageMap			= FALSE;
/** Whether to run as PLATFORM_WindowsConsole or not.  Enabled with "-seekfreeloadingpcconsole"	*/
UBOOL					GIsSeekFreePCConsole			= FALSE;
/** Whether to run as PLATFORM_WindowsServer or not.  Enabled with "-seekfreeloadingserver"	*/
UBOOL					GIsSeekFreePCServer				= FALSE;
/** Whether we are currently precaching or not.																*/
UBOOL					GIsPrecaching					= FALSE;
/** Whether we are still in the initial loading proces.														*/
UBOOL					GIsInitialLoad					= TRUE;
/** Whether we are currently purging an object in the GC purge pass.										*/
UBOOL					GIsPurgingObject				= FALSE;
/** TRUE when we are routing ConditionalPostLoad/PostLoad to objects										*/
UBOOL					GIsRoutingPostLoad				= FALSE;
/** Steadily increasing frame counter.																		*/
QWORD					GFrameCounter					= 0;
/** Incremented once per frame before the scene is being rendered. In split screen mode this is incremented once for all views (not for each view). */
UINT					GFrameNumber					= 0;
/** Render Thread copy of the frame number. */
UINT					GFrameNumberRenderThread		= 0;
#if !SHIPPING_PC_GAME
// We cannot count on this variable to be accurate in a shipped game, so make sure no code tries to use it
/** Whether we are the first instance of the game running.													*/
UBOOL					GIsFirstInstance				= TRUE;
#endif
/** Whether to always use the compression resulting in the smallest size.									*/
UBOOL					GAlwaysBiasCompressionForSize	= FALSE;
/** The number of full speed hardware threads the system has. */
UINT					GNumHardwareThreads				= 1;
/** The number of unused threads to have for SerializeCompressed tasks										*/
UINT					GNumUnusedThreads_SerializeCompressed = 1;
/** Approximate physical RAM in GB; 1 on everything except PC. Used for "course tuning", like GNumHardwareThreads.*/
UINT						GPhysicalGBRam = 1;
/** This flag signals that the rendering code should throw up an appropriate error.							*/
UBOOL					GHandleDirtyDiscError			= FALSE;
/** Whether to forcefully enable capturing of scoped script stats (if > 0).									*/
INT						GForceEnableScopedCycleStats	= 0;
/** Size to break up data into when saving compressed data													*/
INT						GSavingCompressionChunkSize		= SAVING_COMPRESSION_CHUNK_SIZE;
/** Total amount of calls to appSeconds and appCycles.														*/
QWORD					GNumTimingCodeCalls				= 0;
/** Whether we are using the seekfree/ cooked loading codepath.												*/
#if WITH_SUBSTANCE_AIR
UBOOL					GUseSubstanceInstallTimeCache			= FALSE;
#endif
#if CONSOLE
UBOOL					GUseSeekFreeLoading				= TRUE;
#else
UBOOL					GUseSeekFreeLoading				= FALSE;
#endif
/** Thread ID of the main/game thread																		*/
DWORD					GGameThreadId					= 0;
/** Has GGameThreadId been set yet?																			*/
UBOOL					GIsGameThreadIdInitialized		= FALSE;
#if ENABLE_SCRIPT_TRACING
/** Whether we are tracing script to a log file */
UBOOL					GIsUTracing						= FALSE;
#endif
/** Helper function to flush resource streaming.															*/
void					(*GFlushStreamingFunc)(void)	  = &appNoop;
#if PERF_TRACK_SERIALIZATION_PERFORMANCE || LOOKING_FOR_PERF_ISSUES
/** Tracks time spent serializing UObject data, per object type												*/
FStructEventMap*		GObjectSerializationPerfTracker	= NULL;
/** Tracks the amount of time spent in UClass serialization, per class										*/
FStructEventMap*		GClassSerializationPerfTracker	= NULL;
#endif
/** Whether to emit begin/ end draw events.																	*/
UBOOL					GEmitDrawEvents					= FALSE;
/** Whether we are using software rendering or not.															*/
UBOOL					GUseSoftwareRendering			= FALSE;
/** Whether COM is already initialized or not.																*/
UBOOL					GIsCOMInitialized				= FALSE;
/** Whether we want the rendering thread to be suspended, used e.g. for tracing.							*/
UBOOL					GShouldSuspendRenderingThread	= FALSE;
/** Whether we want the game thread to be suspended, used e.g. for tracing.									*/
UBOOL					GShouldSuspendGameThread		= FALSE;
/** Whether we want to use a fixed time step or not.														*/
UBOOL					GUseFixedTimeStep				= FALSE;
/** Determines what kind of trace should occur, NAME_None for none.											*/
FName					GCurrentTraceName				= NAME_None;
/** whether to print time since GStartTime in log output													*/
UBOOL					GPrintLogTimes					= FALSE;		
/** Global screen shot index, which is a way to make it so we don't have overwriting ScreenShots			*/
INT                     GScreenshotBitmapIndex           = -1;
/** Whether stats should emit named events for e.g. PIX.													*/
UBOOL					GCycleStatsShouldEmitNamedEvents= FALSE;
/** Whether stats should also generate data for gameplay profiler											*/
UBOOL					GCycleStatsWithGameplayProfiling = TRUE;
/** Whether or not we should log out all of the PlaySound calls.  Makes it much easier to debug what sound is playing when you have the name. */
UBOOL					GShouldLogAllPlaySoundCalls		= FALSE;	
/** Whether to convert net index errors to warnings (for fixing up redirects)								*/
UBOOL					GConvertNetIndexErrorsToWarnings = FALSE;
/** Whether or not we should log out all of the particle activate calls */
UBOOL					GShouldLogAllParticleActivateSystemCalls = FALSE;
/** Whether the title is running in lowgore mode															*/
UBOOL					GForceLowGore					= FALSE;
/** What RHI mode we're in																					*/
ERenderMode				GRenderMode						= RENDER_MODE_DX9;
/** What RHI mode we're forced into																			*/
ERenderMode				GForcedRenderMode				= RENDER_MODE_NONE;
/** True if we're emulating mobile rendering on a non-mobile renderer */
UBOOL					GEmulateMobileRendering			= FALSE;
/** Sometimes we need to temporarily disallow mobile materials (eg. in editor MeshPaint mode) - so we use this */
UBOOL					GForceDisableEmulateMobileRendering	= FALSE;
/** Whether we are forcing simple lightmaps for the purpose of mobile emulation on a non-mobile renderer. */
UBOOL					GUseSimpleLightmapsForMobileEmulation = FALSE;
/** True if gamma correction should be used when emulating mobile rendering */
UBOOL					GUseGammaCorrectionForMobileEmulation = FALSE;
/** True if we should always cache PVRTC textures and/or flatten textures for mobile when working in the editor */
UBOOL					GAlwaysOptimizeContentForMobile	= FALSE;
/** True if we're emulating mobile input on a non-mobile platform */
UBOOL					GEmulateMobileInput				= FALSE;
/** Global access point to register, find and deal with console variables.									*/
struct IConsoleManager*	GConsoleManager					= NULL;
/** Whether or not we are running OpenAutomate */
class FOpenAutomate*		GOpenAutomate						= NULL;
/** Whether or not a unit test is currently being run														*/
UBOOL					GIsUnitTesting					= FALSE;


/** Total number of calls to Malloc, if implemented by derived class. */
QWORD FMalloc::TotalMallocCalls;
/** Total number of calls to Free, if implemented by derived class. */
QWORD FMalloc::TotalFreeCalls;
/** Total number of calls to Realloc, if implemented by derived class. */
QWORD FMalloc::TotalReallocCalls;
/** Total number of calls to PhysicalAlloc, if implemented by derived class. */
QWORD FMalloc::TotalPhysicalAllocCalls;
/** Total number of calls to PhysicalFree, if implemented by derived class. */
QWORD FMalloc::TotalPhysicalFreeCalls;


/**
 * Object stats declarations
 */
DECLARE_STATS_GROUP(TEXT("Object"),STATGROUP_Object);

/** Memory stats objects 
 *
 * If you add new Stat Memory stats please update:  FMemoryChartEntry so the automated memory chart has the info
 */

DECLARE_STATS_GROUP(TEXT("Memory"),STATGROUP_Memory);
DECLARE_STATS_GROUP(TEXT("MemoryStaticMesh"),STATGROUP_MemoryStaticMesh);
DECLARE_STATS_GROUP(TEXT("XboxMemory"),STATGROUP_XboxMemory);
DECLARE_MEMORY_STAT(TEXT("Physical Memory Used"),STAT_PhysicalAllocSize,STATGROUP_Memory);
#if !PS3 // PS3 doesn't have a need for virtual memory stat
DECLARE_MEMORY_STAT(TEXT("Virtual Memory Used"),STAT_VirtualAllocSize,STATGROUP_Memory);
#endif
DECLARE_MEMORY_STAT(TEXT("Streaming Memory Used"),STAT_StreamingAllocSize,STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("Audio Memory Used"),STAT_AudioMemory,STATGROUP_Memory);
DECLARE_MEMORY_STAT2(TEXT("Rendertarget Memory"), STAT_RendertargetMemory, STATGROUP_Memory, MCR_Physical, FALSE);
DECLARE_MEMORY_STAT(TEXT("Texture Memory Used"),STAT_TextureMemory,STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("Lightmap Memory (Texture)"),STAT_TextureLightmapMemory,STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("Shadowmap Memory (Texture)"),STAT_TextureShadowmapMemory,STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("Light and Shadowmap Memory (Vertex)"),STAT_VertexLightingAndShadowingMemory,STATGROUP_Memory);
#if _WINDOWS // The texture memory stats will be what is used on Xbox...
DECLARE_MEMORY_STAT(TEXT("Texture Memory Used"),STAT_XboxTextureMemory,STATGROUP_XboxMemory);
DECLARE_MEMORY_STAT(TEXT("Lightmap Memory (Texture)"),STAT_XboxTextureLightmapMemory,STATGROUP_XboxMemory);
DECLARE_MEMORY_STAT(TEXT("Shadowmap Memory (Texture)"),STAT_XboxTextureShadowmapMemory,STATGROUP_XboxMemory);
#endif
DECLARE_MEMORY_STAT(TEXT("Novodex Allocation Size"),STAT_MemoryNovodexTotalAllocationSize,STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("Animation Memory"),STAT_AnimationMemory,STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("Precomputed Visibility Memory"),STAT_PrecomputedVisibilityMemory,STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("Precomputed Light Volume Memory"),STAT_PrecomputedLightVolumeMemory,STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("Dominant Shadow Transition Memory"),STAT_DominantShadowTransitionMemory,STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("StaticMesh Total Memory"),STAT_StaticMeshTotalMemory,STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("FracturedMesh Index Memory"),STAT_FracturedMeshIndexMemory,STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("SkeletalMesh Vertex Memory"),STAT_SkeletalMeshVertexMemory,STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("SkeletalMesh Index Memory"),STAT_SkeletalMeshIndexMemory,STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("SkeletalMesh M.BlurSkinning Memory"),STAT_SkeletalMeshMotionBlurSkinningMemory,STATGROUP_Memory);
DECLARE_MEMORY_STAT2(TEXT("Decal Vertex Memory"),STAT_DecalVertexMemory,STATGROUP_Memory, MCR_Physical, FALSE);
DECLARE_MEMORY_STAT2(TEXT("Decal Index Memory"),STAT_DecalIndexMemory,STATGROUP_Memory, MCR_Physical, FALSE);
DECLARE_MEMORY_STAT2(TEXT("Decal Interaction Memory"),STAT_DecalInteractionMemory,STATGROUP_Memory, MCR_Physical, FALSE);
DECLARE_MEMORY_STAT2(TEXT("VertexShader Memory"),STAT_VertexShaderMemory,STATGROUP_Memory, MCR_Physical, FALSE);
DECLARE_MEMORY_STAT2(TEXT("PixelShader Memory"),STAT_PixelShaderMemory,STATGROUP_Memory, MCR_Physical, FALSE);

DECLARE_MEMORY_STAT(TEXT("StaticMesh Total Memory"),STAT_StaticMeshTotalMemory2,STATGROUP_MemoryStaticMesh);
DECLARE_MEMORY_STAT(TEXT("StaticMesh kDOP Memory"),STAT_StaticMeshkDOPMemory,STATGROUP_MemoryStaticMesh);
DECLARE_MEMORY_STAT(TEXT("StaticMesh Vertex Memory"),STAT_StaticMeshVertexMemory,STATGROUP_MemoryStaticMesh);
DECLARE_MEMORY_STAT(TEXT("StaticMesh VxColor Resource Mem"),STAT_ResourceVertexColorMemory,STATGROUP_MemoryStaticMesh);
DECLARE_MEMORY_STAT(TEXT("StaticMesh VxColor Inst Mem"),STAT_InstVertexColorMemory,STATGROUP_MemoryStaticMesh);
DECLARE_MEMORY_STAT(TEXT("StaticMesh Index Memory"),STAT_StaticMeshIndexMemory,STATGROUP_MemoryStaticMesh);

#if PS3
DECLARE_MEMORY_STAT(TEXT("StaticMesh Vertex Memory (RSX)"), STAT_StaticMeshVideoMemory, STATGROUP_MemoryStaticMesh );
DECLARE_MEMORY_STAT(TEXT("StaticMesh VxColor Mem (RSX)"), STAT_ResourceVertexColorVideoMemory, STATGROUP_MemoryStaticMesh );
DECLARE_MEMORY_STAT(TEXT("StaticMesh Index Memory (RSX)"), STAT_StaticMeshIndexVideoMemory, STATGROUP_MemoryStaticMesh );
#endif // PS3

DECLARE_CYCLE_STAT(TEXT("TrimMemory cycles"),STAT_TrimMemoryTime,STATGROUP_Memory);

DECLARE_STATS_GROUP(TEXT("MemoryChurn"),STATGROUP_MemoryChurn);
DECLARE_DWORD_COUNTER_STAT(TEXT("Malloc calls"),STAT_MallocCalls,STATGROUP_MemoryChurn);
DECLARE_DWORD_COUNTER_STAT(TEXT("Realloc calls"),STAT_ReallocCalls,STATGROUP_MemoryChurn);
DECLARE_DWORD_COUNTER_STAT(TEXT("Free calls"),STAT_FreeCalls,STATGROUP_MemoryChurn);
DECLARE_DWORD_COUNTER_STAT(TEXT("PhysicalAlloc calls"),STAT_PhysicalAllocCalls,STATGROUP_MemoryChurn);
DECLARE_DWORD_COUNTER_STAT(TEXT("PhysicalFree calls"),STAT_PhysicalFreeCalls,STATGROUP_MemoryChurn);
DECLARE_DWORD_COUNTER_STAT(TEXT("Total Allocator calls"),STAT_TotalAllocatorCalls,STATGROUP_MemoryChurn);

/** Threading stats objects */
DECLARE_STATS_GROUP(TEXT("Threading"),STATGROUP_Threading);
DECLARE_CYCLE_STAT(TEXT("Rendering thread idle time"),STAT_RenderingIdleTime,STATGROUP_Threading);
DECLARE_CYCLE_STAT(TEXT("Rendering thread busy time"),STAT_RenderingBusyTime,STATGROUP_Threading);
DECLARE_CYCLE_STAT(TEXT("Game thread idle time"),STAT_GameIdleTime,STATGROUP_Threading);
DECLARE_CYCLE_STAT(TEXT("Game thread tick wait time"),STAT_GameTickWaitTime,STATGROUP_Threading);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Game thread requested wait time"),STAT_GameTickWantedWaitTime,STATGROUP_Threading);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Game thread additional wait time"),STAT_GameTickAdditionalWaitTime,STATGROUP_Threading);
#if XBOX
DECLARE_FLOAT_COUNTER_STAT(TEXT("GPU waiting on CPU time"),STAT_GPUWaitingOnCPU,STATGROUP_Threading);
DECLARE_FLOAT_COUNTER_STAT(TEXT("CPU waiting on GPU time"),STAT_CPUWaitingOnGPU,STATGROUP_Threading);
#endif

/** Stats notify providers need to be here so the linker doesn't strip them */
#include "UnStatsNotifyProviders.h"

/** List of provider factories */
DECLARE_STAT_NOTIFY_PROVIDER_FACTORY(
	FStatNotifyProvider_BinaryFileFactory,
	FStatNotifyProvider_BinaryFile,
	BinaryFileProvider );
DECLARE_STAT_NOTIFY_PROVIDER_FACTORY(FStatNotifyProvider_XMLFactory,
	FStatNotifyProvider_XML,XmlProvider);
DECLARE_STAT_NOTIFY_PROVIDER_FACTORY(FStatNotifyProvider_CSVFactory,
	FStatNotifyProvider_CSV,CsvProvider);

#if XBOX
#include "UnStatsNotifyProvidersXe.h"
/** The PIX notifier factory. Declared here because the linker optimizes it out if it's in the Xe file */
DECLARE_STAT_NOTIFY_PROVIDER_FACTORY(FStatNotifyProvider_PIXFactory,
	FStatNotifyProvider_PIX,PixProvider);
#endif

