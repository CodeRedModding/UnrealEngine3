/*=============================================================================
	StackWalkingWindows.cpp: Windows stack walking implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"


#if _WINDOWS && !CONSOLE

#pragma pack(push,8)
#include <DbgHelp.h>				
#include <TlHelp32.h>		
#include <psapi.h>
#pragma pack(pop)

/*-----------------------------------------------------------------------------
	Stack walking.
-----------------------------------------------------------------------------*/

/** Whether appInitStackWalking() has been called successfully or not. */
static UBOOL GStackWalkingInitialized = FALSE;

// NOTE: Make sure to enable Stack Frame pointers: bOmitFramePointers = false, or /Oy-
// If GStackWalkingInitialized is TRUE, traces will work anyway but will be much slower.
#define USE_FAST_STACKTRACE 1

typedef BOOL  (WINAPI *TFEnumProcesses)( DWORD * lpidProcess, DWORD cb, DWORD * cbNeeded);
typedef BOOL  (WINAPI *TFEnumProcessModules)(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded);
typedef DWORD (WINAPI *TFGetModuleBaseName)(HANDLE hProcess, HMODULE hModule, LPSTR lpBaseName, DWORD nSize);
typedef DWORD (WINAPI *TFGetModuleFileNameEx)(HANDLE hProcess, HMODULE hModule, LPSTR lpFilename, DWORD nSize);
typedef BOOL  (WINAPI *TFGetModuleInformation)(HANDLE hProcess, HMODULE hModule, LPMODULEINFO lpmodinfo, DWORD cb);

static TFEnumProcesses			FEnumProcesses;
static TFEnumProcessModules		FEnumProcessModules;
static TFGetModuleBaseName		FGetModuleBaseName;
static TFGetModuleFileNameEx	FGetModuleFileNameEx;
static TFGetModuleInformation	FGetModuleInformation;

/**
 * Helper function performing the actual stack walk. This code relies on the symbols being loaded for best results
 * walking the stack albeit at a significant performance penalty.
 *
 * This helper function is designed to be called within a structured exception handler.
 *
 * @param	BackTrace			Array to write backtrace to
 * @param	MaxDepth			Maxium depth to walk - needs to be less than or equal to array size
 * @param	Context				Thread context information
 * @return	EXCEPTION_EXECUTE_HANDLER
 */
static INT CaptureStackTraceHelper( QWORD *BackTrace, DWORD MaxDepth, CONTEXT* Context )
{
	STACKFRAME64		StackFrame64;
	HANDLE				ProcessHandle;
	HANDLE				ThreadHandle;
	unsigned long		LastError;
	UBOOL				bStackWalkSucceeded	= TRUE;
	DWORD				CurrentDepth		= 0;
	DWORD				MachineType			= IMAGE_FILE_MACHINE_I386;
	CONTEXT				ContextCopy = *Context;

	__try
	{
		// Get context, process and thread information.
		ProcessHandle	= GetCurrentProcess();
		ThreadHandle	= GetCurrentThread();

		// Zero out stack frame.
		memset( &StackFrame64, 0, sizeof(StackFrame64) );

		// Initialize the STACKFRAME structure.
		StackFrame64.AddrPC.Mode         = AddrModeFlat;
		StackFrame64.AddrStack.Mode      = AddrModeFlat;
		StackFrame64.AddrFrame.Mode      = AddrModeFlat;
#ifdef _WIN64
		StackFrame64.AddrPC.Offset       = Context->Rip;
		StackFrame64.AddrStack.Offset    = Context->Rsp;
		StackFrame64.AddrFrame.Offset    = Context->Rbp;
		MachineType                      = IMAGE_FILE_MACHINE_AMD64;
#else
		StackFrame64.AddrPC.Offset       = Context->Eip;
		StackFrame64.AddrStack.Offset    = Context->Esp;
		StackFrame64.AddrFrame.Offset    = Context->Ebp;
#endif

		// Walk the stack one frame at a time.
		while( bStackWalkSucceeded && (CurrentDepth < MaxDepth) )
		{
			bStackWalkSucceeded = StackWalk64(  MachineType, 
												ProcessHandle, 
												ThreadHandle, 
												&StackFrame64,
												&ContextCopy,
												NULL,
												SymFunctionTableAccess64,
												SymGetModuleBase64,
												NULL );

			BackTrace[CurrentDepth++] = StackFrame64.AddrPC.Offset;

			if( !bStackWalkSucceeded  )
			{
				// StackWalk failed! give up.
				LastError = GetLastError( );
				break;
			}

			// Stop if the frame pointer or address is NULL.
			if( StackFrame64.AddrFrame.Offset == 0 || StackFrame64.AddrPC.Offset == 0 )
			{
				break;
			}
		}
	} 
	__except ( EXCEPTION_EXECUTE_HANDLER )
	{
		// We need to catch any exceptions within this function so they don't get sent to 
		// the engine's error handler, hence causing an infinite loop.
		return EXCEPTION_EXECUTE_HANDLER;
	} 

	// NULL out remaining entries.
	for ( ; CurrentDepth<MaxDepth; CurrentDepth++ )
	{
		BackTrace[CurrentDepth] = NULL;
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

PRAGMA_DISABLE_OPTIMIZATION // Work around "flow in or out of inline asm code suppresses global optimization" warning C4740.

#if USE_FAST_STACKTRACE
NTSYSAPI WORD NTAPI RtlCaptureStackBackTrace(
	__in DWORD FramesToSkip,
	__in DWORD FramesToCapture,
	__out_ecount(FramesToCapture) PVOID *BackTrace,
	__out_opt PDWORD BackTraceHash
	);

/** Maximum callstack depth that is supported by the current OS. */
ULONG GMaxCallstackDepth = 62;

/** Whether DetermineMaxCallstackDepth() has been called or not. */
UBOOL GMaxCallstackDepthInitialized = FALSE;

/** Maximum callstack depth we support, no matter what OS we're running on. */
#define MAX_CALLSTACK_DEPTH 128

/** Checks the current OS version and sets up the GMaxCallstackDepth variable. */
void DetermineMaxCallstackDepth()
{
	// Check that we're running on Vista or newer (version 6.0+).
	OSVERSIONINFOEX Version;
	Version.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	Version.dwMajorVersion = 6;
	ULONGLONG ConditionMask = 0;
	BOOL bVerificationPassed = VerifyVersionInfo( &Version, VER_MAJORVERSION, VerSetConditionMask(ConditionMask,VER_MAJORVERSION,VER_GREATER_EQUAL) );
	if ( bVerificationPassed )
	{
		GMaxCallstackDepth = MAX_CALLSTACK_DEPTH;
	}
	else
	{
		GMaxCallstackDepth = Min<ULONG>(62,MAX_CALLSTACK_DEPTH);
	}
	GMaxCallstackDepthInitialized = TRUE;
}

#endif

/**
 * Capture a stack backtrace and optionally use the passed in exception pointers.
 *
 * @param	BackTrace			[out] Pointer to array to take backtrace
 * @param	MaxDepth			Entries in BackTrace array
 * @param	Context				Optional thread context information
 */
void appCaptureStackBackTrace( QWORD* BackTrace, DWORD MaxDepth, CONTEXT* Context )
{
	// Make sure we have place to store the information before we go through the process of raising
	// an exception and handling it.
	if( BackTrace == NULL || MaxDepth == 0 )
	{
		return;
	}

	if( Context )
	{
		CaptureStackTraceHelper( BackTrace, MaxDepth, Context );
	}
	else
	{
#if USE_FAST_STACKTRACE
		// NOTE: Make sure to enable Stack Frame pointers: bOmitFramePointers = false, or /Oy-
		// If GStackWalkingInitialized is TRUE, traces will work anyway but will be much slower.
		if ( GStackWalkingInitialized )
		{
			CONTEXT HelperContext;
			RtlCaptureContext( &HelperContext );

			// Capture the back trace.
			CaptureStackTraceHelper( BackTrace, MaxDepth, &HelperContext );
		}
		else
		{
			if ( !GMaxCallstackDepthInitialized )
			{
				DetermineMaxCallstackDepth();
			}
			PVOID WinBackTrace[MAX_CALLSTACK_DEPTH];
			USHORT NumFrames = RtlCaptureStackBackTrace( 0, Min<ULONG>(GMaxCallstackDepth,MaxDepth), WinBackTrace, NULL );
			for ( USHORT FrameIndex=0; FrameIndex < NumFrames; ++FrameIndex )
			{
				BackTrace[ FrameIndex ] = (QWORD) WinBackTrace[ FrameIndex ];
			}
			while ( NumFrames < MaxDepth )
			{
				BackTrace[ NumFrames++ ] = NULL;
			}
		}
#elif defined(_WIN64)
		// Raise an exception so CaptureStackBackTraceHelper has access to context record.
		__try
		{
			RaiseException(	0,			// Application-defined exception code.
							0,			// Zero indicates continuable exception.
							0,			// Number of arguments in args array (ignored if args is NULL)
							NULL );		// Array of arguments
			}
		// Capture the back trace.
		__except( CaptureStackTraceHelper( BackTrace, MaxDepth, (GetExceptionInformation())->ContextRecord ) )
		{
		}
#elif 1
		// Use a bit of inline assembly to capture the information relevant to stack walking which is
		// basically EIP and EBP.
		CONTEXT HelperContext;
		memset( &HelperContext, 0, sizeof(CONTEXT) );
		HelperContext.ContextFlags = CONTEXT_FULL;

		// Use a fake function call to pop the return address and retrieve EIP.
		__asm
		{
			call FakeFunctionCall
		FakeFunctionCall: 
			pop eax
			mov HelperContext.Eip, eax
			mov HelperContext.Ebp, ebp
			mov HelperContext.Esp, esp
		}

		// Capture the back trace.
		CaptureStackTraceHelper( BackTrace, MaxDepth, &HelperContext );
#else
		CONTEXT HelperContext;
		// RtlCaptureContext relies on EBP being untouched so if the below crashes it is because frame pointer
		// omission is enabled. It is implied by /Ox or /O2 and needs to be manually disabled via /Oy-
		RtlCaptureContext( HelperContext );

		// Capture the back trace.
		CaptureStackTraceHelper( BackTrace, MaxDepth, &HelperContext );
#endif
	}
}

PRAGMA_ENABLE_OPTIMIZATION

/**
 * Walks the stack and appends the human readable string to the passed in one.
 * @warning: The code assumes that HumanReadableString is large enough to contain the information.
 *
 * @param	HumanReadableString	String to concatenate information with
 * @param	HumanReadableStringSize size of string in characters
 * @param	IgnoreCount			Number of stack entries to ignore (some are guaranteed to be in the stack walking code)
 * @param	Context				Optional thread context information
 */ 
void appStackWalkAndDump( ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, INT IgnoreCount, CONTEXT* Context )
{	
	// Initialize stack walking... loads up symbol information.
	appInitStackWalking();

	// Temporary memory holding the stack trace.
	const UINT MaxDepth = 100;
	DWORD64 StackTrace[MaxDepth];
	memset( StackTrace, 0, sizeof(StackTrace) );

	// Capture stack backtrace.
	appCaptureStackBackTrace( StackTrace, MaxDepth, Context );

	// Skip the first two entries as they are inside the stack walking code.
	INT CurrentDepth = IgnoreCount;
	// Allow the first entry to be NULL as the crash could have been caused by a call to a NULL function pointer,
	// which would mean the top of the callstack is NULL.
	while( StackTrace[CurrentDepth] || ( CurrentDepth == IgnoreCount ) )
	{
		appProgramCounterToHumanReadableString( StackTrace[CurrentDepth], HumanReadableString, HumanReadableStringSize );
		appStrcatANSI( HumanReadableString, HumanReadableStringSize, "\r\n" );
		CurrentDepth++;
	}
}


/** 
 * Dump current function call stack to log file.
 */
void appDumpCallStackToLog(INT IgnoreCount/* =2 */)
{
	// Initialize stack walking... loads up symbol information.
	appInitStackWalking();

	// Capture C++ callstack
	const UINT MaxDepth = 100;
	QWORD FullBackTrace[MaxDepth];
	memset( FullBackTrace, 0, sizeof(FullBackTrace) );
	appCaptureStackBackTrace( FullBackTrace, MaxDepth );
	FString CallStackString;

	// Iterate over all addresses in the callstack to look up symbol name. Skipping IgnoreCount first entries.
	for( INT AddressIndex=IgnoreCount; AddressIndex<ARRAY_COUNT(FullBackTrace) && FullBackTrace[AddressIndex]; AddressIndex++ )
	{
		ANSICHAR AddressInformation[512];
		AddressInformation[0] = 0;
		appProgramCounterToHumanReadableString( FullBackTrace[AddressIndex], AddressInformation, ARRAY_COUNT(AddressInformation)-1, VF_DISPLAY_FILENAME );
		CallStackString = CallStackString + TEXT("\t") + FString(AddressInformation) + TEXT("\r\n");
	}

	// Finally log callstack
	debugf(TEXT("%s \r\n\r\n"),*CallStackString);
}


/**
 * Converts the passed in program counter address to a human readable string and appends it to the passed in one.
 * @warning: The code assumes that HumanReadableString is large enough to contain the information.
 *
 * @param	ProgramCounter			Address to look symbol information up for
 * @param	HumanReadableString		String to concatenate information with
 * @param	HumanReadableStringSize size of string in characters
 * @param	VerbosityFlags			Bit field of requested data for output.
 * @return	TRUE if the symbol was found, otherwise FALSE
 */ 
UBOOL appProgramCounterToHumanReadableString( QWORD ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, EVerbosityFlags VerbosityFlags )
{
	ANSICHAR			SymbolBuffer[sizeof(IMAGEHLP_SYMBOL64) + 512];
	PIMAGEHLP_SYMBOL64	Symbol;
	DWORD				SymbolDisplacement		= 0;
	DWORD64				SymbolDisplacement64	= 0;
	DWORD				LastError;
	UBOOL				bSymbolFound = FALSE;

	HANDLE				ProcessHandle = GetCurrentProcess();

	// Initialize stack walking as it loads up symbol information which we require.
	appInitStackWalking();

	// Initialize symbol.
	Symbol					= (PIMAGEHLP_SYMBOL64) SymbolBuffer;
	Symbol->SizeOfStruct	= sizeof(SymbolBuffer);
	Symbol->MaxNameLength	= 512;

	// Get symbol from address.
	if( SymGetSymFromAddr64( ProcessHandle, ProgramCounter, &SymbolDisplacement64, Symbol ) )
	{
		ANSICHAR			FunctionName[MAX_SPRINTF];

		// Skip any funky chars in the beginning of a function name.
		INT Offset = 0;
		while( Symbol->Name[Offset] < 32 || Symbol->Name[Offset] > 127 )
		{
			Offset++;
		}

		// Write out function name if there is sufficient space.
		appSprintfANSI( FunctionName,  ("%s() "), (const ANSICHAR*)(Symbol->Name + Offset) );
		appStrcatANSI( HumanReadableString, HumanReadableStringSize, FunctionName );
		bSymbolFound = TRUE;
	}
	else
	{
		// No symbol found for this address.
		LastError = GetLastError( );
	}

	if( VerbosityFlags & VF_DISPLAY_FILENAME )
	{
		IMAGEHLP_LINE64		ImageHelpLine;
		ANSICHAR			FileNameLine[MAX_SPRINTF];

		// Get Line from address
		ImageHelpLine.SizeOfStruct = sizeof( ImageHelpLine );
		if( SymGetLineFromAddr64( ProcessHandle, ProgramCounter, &SymbolDisplacement, &ImageHelpLine) )
		{
			appSprintfANSI( FileNameLine, ("0x%-8x + %d bytes [File=%s:%d] "), (DWORD) ProgramCounter, SymbolDisplacement, (const ANSICHAR*)(ImageHelpLine.FileName), ImageHelpLine.LineNumber );
		}
		else    
		{
			// No line number found.  Print out the logical address instead.
			appSprintfANSI( FileNameLine, "Address = 0x%-8x (filename not found) ", (DWORD) ProgramCounter );
		}
		appStrcatANSI( HumanReadableString, HumanReadableStringSize, FileNameLine );
	}

	if( VerbosityFlags & VF_DISPLAY_MODULE )
	{
		IMAGEHLP_MODULE64	ImageHelpModule;
		ANSICHAR			ModuleName[MAX_SPRINTF];

		// Get module information from address.
		ImageHelpModule.SizeOfStruct = sizeof( ImageHelpModule );
		if( SymGetModuleInfo64( ProcessHandle, ProgramCounter, &ImageHelpModule) )
		{
			// Write out Module information if there is sufficient space.
			appSprintfANSI( ModuleName, "[in %s]", (const ANSICHAR*)(ImageHelpModule.ImageName) );
			appStrcatANSI( HumanReadableString, HumanReadableStringSize, ModuleName );
		}
		else
		{
			LastError = GetLastError( );
		}
	}

	return bSymbolFound;
}

/**
 * Converts the passed in program counter address to a symbol info struct, filling in module and filename, line number and displacement.
 * @warning: The code assumes that the destination strings are big enough
 *
 * @param	ProgramCounter			Address to look symbol information up for
 * @return	symbol information associated with program counter
 */
FProgramCounterSymbolInfo appProgramCounterToSymbolInfo( QWORD ProgramCounter )
{
	// Create zeroed out return value.
	FProgramCounterSymbolInfo	SymbolInfo;
	appMemzero( &SymbolInfo, sizeof(SymbolInfo) );

	ANSICHAR			SymbolBuffer[sizeof(IMAGEHLP_SYMBOL64) + 512];
	PIMAGEHLP_SYMBOL64	Symbol;
	DWORD				SymbolDisplacement		= 0;
	DWORD64				SymbolDisplacement64	= 0;
	DWORD				LastError;
	
	HANDLE				ProcessHandle = GetCurrentProcess();

	// Initialize stack walking as it loads up symbol information which we require.
	appInitStackWalking();

	// Initialize symbol.
	Symbol					= (PIMAGEHLP_SYMBOL64) SymbolBuffer;
	Symbol->SizeOfStruct	= sizeof(SymbolBuffer);
	Symbol->MaxNameLength	= 512;

	// Get symbol from address.
	if( SymGetSymFromAddr64( ProcessHandle, ProgramCounter, &SymbolDisplacement64, Symbol ) )
	{
		// Skip any funky chars in the beginning of a function name.
		INT Offset = 0;
		while( Symbol->Name[Offset] < 32 || Symbol->Name[Offset] > 127 )
		{
			Offset++;
		}

		// Write out function name.
		appStrcpyANSI( SymbolInfo.FunctionName, Symbol->Name + Offset );
	}
	else
	{
		// No symbol found for this address.
		LastError = GetLastError( );
	}

	// Get Line from address
	IMAGEHLP_LINE64	ImageHelpLine;
	ImageHelpLine.SizeOfStruct = sizeof( ImageHelpLine );
	if( SymGetLineFromAddr64( ProcessHandle, ProgramCounter, &SymbolDisplacement, &ImageHelpLine) )
	{
		appStrcpyANSI( SymbolInfo.Filename, ImageHelpLine.FileName );
		SymbolInfo.LineNumber			= ImageHelpLine.LineNumber;
		SymbolInfo.SymbolDisplacement	= SymbolDisplacement;
	}
	else    
	{
		// No line number found.
		appStrcatANSI( SymbolInfo.Filename, "Unknown" );
		SymbolInfo.LineNumber			= 0;
		SymbolDisplacement				= 0;
	}

	// Get module information from address.
	IMAGEHLP_MODULE64 ImageHelpModule;
	ImageHelpModule.SizeOfStruct = sizeof( ImageHelpModule );
	if( SymGetModuleInfo64( ProcessHandle, ProgramCounter, &ImageHelpModule) )
	{
		// Write out Module information.
		appStrcpyANSI( SymbolInfo.ModuleName, ImageHelpModule.ImageName );
	}
	else
	{
		LastError = GetLastError( );
	}

	return SymbolInfo;
}

/**
 * Loads modules for current process.
 */ 
static void LoadProcessModules()
{
	INT			ErrorCode = 0;	
	HANDLE		ProcessHandle = GetCurrentProcess(); 
	const INT	MAX_MOD_HANDLES = 1024;
	HMODULE		ModuleHandleArray[MAX_MOD_HANDLES];
	HMODULE*	ModuleHandlePointer = ModuleHandleArray;
	DWORD		BytesRequired;
	MODULEINFO	ModuleInfo;

	// Enumerate process modules.
	UBOOL bEnumProcessModulesSucceeded = FEnumProcessModules( ProcessHandle, ModuleHandleArray, sizeof(ModuleHandleArray), &BytesRequired );
	if( !bEnumProcessModulesSucceeded )
	{
		ErrorCode = GetLastError();
		return;
	}

	// Static array isn't sufficient so we dynamically allocate one.
	UBOOL bNeedToFreeModuleHandlePointer = FALSE;
	if( BytesRequired > sizeof( ModuleHandleArray ) )
	{
		// Keep track of the fact that we need to free it again.
		bNeedToFreeModuleHandlePointer = TRUE;
		ModuleHandlePointer = (HMODULE*) appMalloc( BytesRequired );
		FEnumProcessModules( ProcessHandle, ModuleHandlePointer, sizeof(ModuleHandleArray), &BytesRequired );
	}

	// Find out how many modules we need to load modules for.
	INT	ModuleCount = BytesRequired / sizeof( HMODULE );

	// Load the modules.
	for( INT ModuleIndex=0; ModuleIndex<ModuleCount; ModuleIndex++ )
	{
		ANSICHAR ModuleName[1024];
		ANSICHAR ImageName[1024];
		FGetModuleInformation( ProcessHandle, ModuleHandleArray[ModuleIndex], &ModuleInfo,sizeof( ModuleInfo ) );
		FGetModuleFileNameEx( ProcessHandle, ModuleHandleArray[ModuleIndex], ImageName, 1024 );
		FGetModuleBaseName( ProcessHandle, ModuleHandleArray[ModuleIndex], ModuleName, 1024 );

		// Set the search path to find PDBs in the same folder as the DLL.
		ANSICHAR SearchPath[1024];
		ANSICHAR* FileName = NULL;
		GetFullPathNameA( ImageName, 1024, SearchPath, &FileName );
		*FileName = 0;
		SymSetSearchPath( GetCurrentProcess(), SearchPath );

		// Load module.
		DWORD64 BaseAddress = SymLoadModule64( ProcessHandle, ModuleHandleArray[ModuleIndex], ImageName, ModuleName, (DWORD64) ModuleInfo.lpBaseOfDll, (DWORD) ModuleInfo.SizeOfImage );
		if( !BaseAddress )
		{
			ErrorCode = GetLastError();
		}
	} 

	// Free the module handle pointer allocated in case the static array was insufficient.
	if( bNeedToFreeModuleHandlePointer )
	{
		appFree( ModuleHandlePointer );
	}
}

/**
 * Returns the number of modules loaded by the currently running process.
 */
INT appGetProcessModuleCount()
{
	appInitStackWalking();

	HANDLE		ProcessHandle = GetCurrentProcess(); 
	const INT	MAX_MOD_HANDLES = 1024;
	HMODULE		ModuleHandleArray[MAX_MOD_HANDLES];
	HMODULE*	ModuleHandlePointer = ModuleHandleArray;
	DWORD		BytesRequired;

	// Enumerate process modules.
	UBOOL bEnumProcessModulesSucceeded = FEnumProcessModules( ProcessHandle, ModuleHandleArray, sizeof(ModuleHandleArray), &BytesRequired );
	if( !bEnumProcessModulesSucceeded )
	{
		return 0;
	}

	// Static array isn't sufficient so we dynamically allocate one.
	UBOOL bNeedToFreeModuleHandlePointer = FALSE;
	if( BytesRequired > sizeof( ModuleHandleArray ) )
	{
		// Keep track of the fact that we need to free it again.
		bNeedToFreeModuleHandlePointer = TRUE;
		ModuleHandlePointer = (HMODULE*) appMalloc( BytesRequired );
		FEnumProcessModules( ProcessHandle, ModuleHandlePointer, sizeof(ModuleHandleArray), &BytesRequired );
	}

	// Find out how many modules we need to load modules for.
	INT	ModuleCount = BytesRequired / sizeof( HMODULE );

	// Free the module handle pointer allocated in case the static array was insufficient.
	if( bNeedToFreeModuleHandlePointer )
	{
		appFree( ModuleHandlePointer );
	}

	return ModuleCount;
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
	appInitStackWalking();

	HANDLE		ProcessHandle = GetCurrentProcess(); 
	const INT	MAX_MOD_HANDLES = 1024;
	HMODULE		ModuleHandleArray[MAX_MOD_HANDLES];
	HMODULE*	ModuleHandlePointer = ModuleHandleArray;
	DWORD		BytesRequired;
	MODULEINFO	ModuleInfo;

	// Enumerate process modules.
	UBOOL bEnumProcessModulesSucceeded = FEnumProcessModules( ProcessHandle, ModuleHandleArray, sizeof(ModuleHandleArray), &BytesRequired );
	if( !bEnumProcessModulesSucceeded )
	{
		return 0;
	}

	// Static array isn't sufficient so we dynamically allocate one.
	UBOOL bNeedToFreeModuleHandlePointer = FALSE;
	if( BytesRequired > sizeof( ModuleHandleArray ) )
	{
		// Keep track of the fact that we need to free it again.
		bNeedToFreeModuleHandlePointer = TRUE;
		ModuleHandlePointer = (HMODULE*) appMalloc( BytesRequired );
		FEnumProcessModules( ProcessHandle, ModuleHandlePointer, sizeof(ModuleHandleArray), &BytesRequired );
	}

	// Find out how many modules we need to load modules for.
	INT	ModuleCount = BytesRequired / sizeof( HMODULE );
	IMAGEHLP_MODULEW64 Img;
	Img.SizeOfStruct = sizeof(Img);

	INT SignatureIndex = 0;

	// Load the modules.
	for( INT ModuleIndex = 0; ModuleIndex < ModuleCount && SignatureIndex < ModuleSignaturesSize; ModuleIndex++ )
	{
		ANSICHAR ModuleName[1024];
		ANSICHAR ImageName[1024];
		FGetModuleInformation( ProcessHandle, ModuleHandleArray[ModuleIndex], &ModuleInfo,sizeof( ModuleInfo ) );
		FGetModuleFileNameEx( ProcessHandle, ModuleHandleArray[ModuleIndex], ImageName, 1024 );
		FGetModuleBaseName( ProcessHandle, ModuleHandleArray[ModuleIndex], ModuleName, 1024 );

		// Load module.
		if(SymGetModuleInfoW64(ProcessHandle, (DWORD64)ModuleInfo.lpBaseOfDll, &Img))
		{
			FModuleInfo Info;
			Info.BaseOfImage = Img.BaseOfImage;
			appStrcpy(Info.ImageName, Img.ImageName);
			Info.ImageSize = Img.ImageSize;
			appStrcpy(Info.LoadedImageName, Img.LoadedImageName);
			appStrcpy(Info.ModuleName, Img.ModuleName);
			Info.PdbAge = Img.PdbAge;
			Info.PdbSig = Img.PdbSig;
			Info.PdbSig70 = Img.PdbSig70;
			Info.TimeDateStamp = Img.TimeDateStamp;

			ModuleSignatures[SignatureIndex] = Info;
			++SignatureIndex;
		}
	}

	// Free the module handle pointer allocated in case the static array was insufficient.
	if( bNeedToFreeModuleHandlePointer )
	{
		appFree( ModuleHandlePointer );
	}

	return SignatureIndex;
}

/**
 * Initializes the symbol engine if needed.
 */ 
UBOOL appInitStackWalking()
{
	// Only initialize once.
	if( !GStackWalkingInitialized )
	{
		void* DllHandle = appGetDllHandle( TEXT("PSAPI.DLL") );
		if( DllHandle == NULL )
		{
			return FALSE;
		}

		// Load dynamically linked PSAPI routines.
		FEnumProcesses			= (TFEnumProcesses)			appGetDllExport( DllHandle,TEXT("EnumProcesses"));
		FEnumProcessModules		= (TFEnumProcessModules)	appGetDllExport( DllHandle,TEXT("EnumProcessModules"));
		FGetModuleFileNameEx	= (TFGetModuleFileNameEx)	appGetDllExport( DllHandle,TEXT("GetModuleFileNameExA"));
		FGetModuleBaseName		= (TFGetModuleBaseName)		appGetDllExport( DllHandle,TEXT("GetModuleBaseNameA"));
		FGetModuleInformation	= (TFGetModuleInformation)	appGetDllExport( DllHandle,TEXT("GetModuleInformation"));

		// Abort if we can't look up the functions.
		if( !FEnumProcesses || !FEnumProcessModules || !FGetModuleFileNameEx || !FGetModuleBaseName || !FGetModuleInformation )
		{
			return FALSE;
		}

		// Set up the symbol engine.
		DWORD SymOpts = SymGetOptions();
		SymOpts |= SYMOPT_LOAD_LINES;
		SymOpts |= SYMOPT_DEBUG;
		SymOpts |= SYMOPT_UNDNAME;
		SymOpts |= SYMOPT_LOAD_LINES;
		SymOpts |= SYMOPT_FAIL_CRITICAL_ERRORS;
		SymOpts |= SYMOPT_DEFERRED_LOADS;
		SymOpts |= SYMOPT_ALLOW_ABSOLUTE_SYMBOLS;
		SymOpts |= SYMOPT_EXACT_SYMBOLS;
		SymOpts |= SYMOPT_CASE_INSENSITIVE;
		SymSetOptions ( SymOpts );

		// Initialize the symbol engine.
		SymInitialize ( GetCurrentProcess(), NULL, TRUE );
		LoadProcessModules();

		GStackWalkingInitialized = TRUE;
	}
	return GStackWalkingInitialized;
}

#endif


