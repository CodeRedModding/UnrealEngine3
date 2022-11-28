/*=============================================================================
	Core.cpp: Unreal core.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	Temporary startup objects.
-----------------------------------------------------------------------------*/

// Error malloc.
class FMallocError : public FMalloc
{
	void Called( const TCHAR* Str )
		{appErrorf( TEXT("Called %s before memory init"), Str );check(0);}
	void* Malloc( DWORD Count )
		{Called(TEXT("appMalloc"));return NULL;}
	void* Realloc( void* Original, DWORD Count )
		{Called(TEXT("appRealloc"));return NULL;}
	void Free( void* Original )
		{Called(TEXT("appFree"));}
	void* PhysicalMalloc( DWORD Count )
		{Called(TEXT("PhysicalMalloc"));return NULL;}
	void PhysicalFree( void* Original )
		{Called(TEXT("PhysicalFree"));}
	void DumpAllocs()
		{Called(TEXT("appDumpAllocs"));}
	void HeapCheck()
		{Called(TEXT("appHeapCheck"));}
	void Init( UBOOL Reset )
		{Called(TEXT("FMallocError::Init"));}
	void Exit()
		{Called(TEXT("FMallocError::Exit"));}
	void DumpMemoryImage()
		{Called(TEXT("FMallocError::DumpMemoryImage"));}
	void GetAllocationInfo( SIZE_T& Virtual, SIZE_T& Physical )
		{Called(TEXT("FMallocError::GetAllocationInfo"));}
} MallocError;

// Error file manager.
class FFileManagerError : public FFileManager
{
public:
	FArchive* CreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
		{appErrorf(TEXT("Called FFileManagerError::CreateFileReader")); return 0;}
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
		{appErrorf(TEXT("Called FFileManagerError::CreateFileWriter")); return 0;}
	INT FileSize( const TCHAR* Filename )
		{return -1;}
	UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
		{return 0;}
	UBOOL IsReadOnly( const TCHAR* Filename )
		{return 0;}
	DWORD Copy( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0, DWORD Compress=FILECOPY_Normal, FCopyProgress* Progress=NULL )
		{return COPY_MiscFail;}
	UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 )
		{return 0;}
	UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )
		{return 0;}
	UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )
		{return 0;}
	void FindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories )
		{}
	DOUBLE GetFileAgeSeconds( const TCHAR* Filename )
		{return -1.0;}
	UBOOL SetDefaultDirectory()
		{return 0;}
	FString GetCurrentDirectory()
		{return TEXT("");}
} FileError;

// Exception thrower.
class FThrowOut : public FOutputDevice
{
public:
	void Serialize( const TCHAR* V, EName Event )
	{
		throw( V );
	}
} ThrowOut;

// Null output device.
class FNullOut : public FOutputDevice
{
public:
	void Serialize( const TCHAR* V, enum EName Event )
	{}
} NullOut;

// Dummy saver.
class FArchiveDummySave : public FArchive
{
public:
	FArchiveDummySave() { ArIsSaving = 1; }
} GArchiveDummySave;

/** Glogal output device redirector, can be static as FOutputDeviceRedirector explicitely empties TArray on TearDown */
static FOutputDeviceRedirector LogRedirector;

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

FMemStack				GMem;							/* Global memory stack */
FOutputDeviceRedirectorBase* GLog						= &LogRedirector;			/* Regular logging */
FOutputDeviceError*		GError							= NULL;						/* Critical errors */
FOutputDevice*			GNull							= &NullOut;					/* Log to nowhere */
FOutputDevice*			GThrow							= &ThrowOut;				/* Exception thrower */
FFeedbackContext*		GWarn							= NULL;						/* User interaction and non critical warnings */
FConfigCache*			GConfig							= NULL;						/* Configuration database cache */
FTransactionBase*		GUndo							= NULL;						/* Transaction tracker, non-NULL when a transaction is in progress */
FOutputDeviceConsole*	GLogConsole						= NULL;						/* Console log hook */
FMalloc*				GMalloc							= &MallocError;				/* Memory allocator */
FFileManager*			GFileManager					= &FileError;				/* File manager */
FCallbackDevice*		GCallback						= NULL;						/* Used for making callbacks into UnrealEd */
USystem*				GSys							= NULL;						/* System control code */
UProperty*				GProperty						= NULL;						/* Property for UnrealScript interpretter */
BYTE*					GPropAddr						= NULL;						/* Property address for UnrealScript interpreter */
UObject*				GPropObject						= NULL;						/* Object with Property for UnrealScript interpreter */
DWORD					GRuntimeUCFlags					= 0;						/* Property for storing flags between calls to bytecode functions */
USubsystem*				GWindowManager					= NULL;						/* Window update routine called once per tick */
class UPropertyWindowManager*	GPropertyWindowManager	= NULL;						/* Manages and tracks property editing windows */
TCHAR					GErrorHist[4096]				= TEXT("");					/* For building call stack text dump in guard/unguard mechanism */
TCHAR					GYes[64]						= TEXT("Yes");				/* Localized "yes" text */
TCHAR					GNo[64]							= TEXT("No");				/* Localized "no" text */
TCHAR					GTrue[64]						= TEXT("True");				/* Localized "true" text */
TCHAR					GFalse[64]						= TEXT("False");			/* Localized "false" text */
TCHAR					GNone[64]						= TEXT("None");				/* Localized "none" text */
DOUBLE					GSecondsPerCycle				= 1.0;						/* Seconds per CPU cycle for this PC */
INT						GScriptCycles					= 0;						/* Times script execution CPU cycles per tick */
DWORD					GPageSize						= 4096;						/* Operating system page size */
DWORD					GUglyHackFlags					= 0;						/* Flags for passing around globally hacked stuff */
UBOOL					GIsEditor						= 0;						/* Whether engine was launched for editing */
UBOOL					GIsUCC							= 0;						/* Is UCC running? */
UBOOL					GIsUCCMake						= 0;						/* Are we rebuilding script via ucc make? */
UBOOL					GEdSelectionLock				= 0;						/* Are selections locked? (you can't select/deselect additional actors) */
UBOOL					GIsClient						= 0;						/* Whether engine was launched as a client */
UBOOL					GIsServer						= 0;						/* Whether engine was launched as a server, true if GIsClient */
UBOOL					GIsCriticalError				= 0;						/* An appError() has occured */
UBOOL					GIsStarted						= 0;						/* Whether execution is happening from within main()/WinMain() */
UBOOL					GIsGuarded						= 0;						/* Whether execution is happening within main()/WinMain()'s try/catch handler */
UBOOL					GIsRunning						= 0;						/* Whether execution is happening within MainLoop() */
UBOOL					GIsGarbageCollecting			= 0;						/* Whether we are inside garbage collection */
UBOOL					GIsSlowTask						= 0;						/* Whether there is a slow task in progress */
UBOOL					GIsRequestingExit				= 0;						/* Indicates that MainLoop() should be exited at the end of the current iteration */
UBOOL					GLazyLoad						= 1;						/* Whether TLazyLoad arrays should be lazy-loaded or not */
FGlobalMath				GMath;														/* Math code */
FArchive*				GDummySave						= &GArchiveDummySave;		/* No-op save archive */
TArray<FEdLoadError>	GEdLoadErrors;												/* For keeping track of load errors in the editor */
UDebugger*				GDebugger						= NULL;						/* Unrealscript Debugger */
UBOOL					GIsBenchmarking					= 0;						/* Whether we are in benchmark mode or not */
QWORD					GMakeCacheIDIndex				= 0;						/* Cache ID */
TCHAR					GEngineIni[1024]				= TEXT("");					/* Engine ini file */
TCHAR					GEditorIni[1024]				= TEXT("");					/* Editor ini file */
TCHAR					GInputIni[1024]					= TEXT("");					/* Input ini file */
TCHAR					GGameIni[1024]					= TEXT("");					/* Game ini file */
FLOAT					NEAR_CLIPPING_PLANE				= 10.0f;					/* Near clipping plane */
DOUBLE					GCurrentTime					= 0;						/* Current time */
INT						GSRandSeed						= 0;						/* Seed for appSRand */
#ifdef XBOX
TCHAR					GMemoryImageName[256]			= TEXT("");					/* Memory image name */
#endif
#if UNICODE
/** Whether we're using Unicode or not																		*/
UBOOL					GUnicode						= 1;						
/** Whether the OS supports Unicode or not																	*/
UBOOL					GUnicodeOS						= 0;						
#else
/** Whether we're using Unicode or not																		*/
UBOOL					GUnicode						= 0;						
/** Whether the OS supports Unicode or not																	*/
UBOOL					GUnicodeOS						= 0;						
#endif
UBOOL					GExitPurge						= 0;
/** Game name, used for base game directory and ini among other things										*/
TCHAR					GGameName[64]					= TEXT("Demo");				
/** Exec handler for game debugging tool, allowing commands like "editactor", ...							*/
FExec*					GDebugToolExec;
/** Whether we're currently cooking for console																*/
UBOOL					GIsConsoleCooking				= 0;

#ifndef XBOX
FCriticalSection*		GHostByNameCriticalSection;
#else
// The below won't be clobbered by the memory image code.
__declspec(allocate(XENON_NOCLOBBER_SEG)) FCriticalSection*		GHostByNameCriticalSection;
/** Whether we are saving or loading a memory image															*/
__declspec(allocate(XENON_NOCLOBBER_SEG)) DWORD					GMemoryImageFlags = MEMORYIMAGE_NotUsed;
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

