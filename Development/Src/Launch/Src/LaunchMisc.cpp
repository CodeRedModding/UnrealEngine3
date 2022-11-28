#include "LaunchPrivate.h"

//
// Minidump support/ exception handling.
//

#pragma pack(push,8)
#include <DbgHelp.h>
#pragma pack(pop)

TCHAR MiniDumpFilenameW[64] = TEXT("");
char  MiniDumpFilenameA[64] = "";		// Don't use alloca before writing out minidump.

#define MAX_SYMBOL_NAME_LENGTH 1024

//@todo: investigate whether alloca usage of ANSI_TO_TCHAR below is safe
INT CreateMiniDump( LPEXCEPTION_POINTERS ExceptionInfo )
{
	// Try to create file for minidump.
	HANDLE FileHandle	= TCHAR_CALL_OS( 
								CreateFileW( MiniDumpFilenameW, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL ), 
								CreateFileA( MiniDumpFilenameA, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL ) 
						);

	// Write a minidump.

	if( FileHandle )
	{
		MINIDUMP_EXCEPTION_INFORMATION DumpExceptionInfo;
	
		DumpExceptionInfo.ThreadId			= GetCurrentThreadId();
		DumpExceptionInfo.ExceptionPointers	= ExceptionInfo;
		DumpExceptionInfo.ClientPointers	= true;

		MiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), FileHandle, MiniDumpNormal, &DumpExceptionInfo, NULL, NULL );
		CloseHandle( FileHandle );
	}

	// Walk the stack.

	// Variables for stack walking.
	HANDLE				Process		= GetCurrentProcess(),
						Thread		= GetCurrentThread();
	DWORD64				Offset64	= 0;
	DWORD				Offset		= 0,
						SymOptions	= SymGetOptions();
	SYMBOL_INFO*		Symbol		= (SYMBOL_INFO*) appSystemMalloc( sizeof(SYMBOL_INFO) + MAX_SYMBOL_NAME_LENGTH );
	IMAGEHLP_LINE64		Line;
	STACKFRAME64		StackFrame;

	// Initialize stack frame.
	memset( &StackFrame, 0, sizeof(StackFrame) );
	StackFrame.AddrPC.Offset		= ExceptionInfo->ContextRecord->Eip;
	StackFrame.AddrPC.Mode			= AddrModeFlat;
	StackFrame.AddrFrame.Offset		= ExceptionInfo->ContextRecord->Ebp;
	StackFrame.AddrFrame.Mode		= AddrModeFlat;
	StackFrame.AddrStack.Offset		= ExceptionInfo->ContextRecord->Esp;
	StackFrame.AddrStack.Mode		= AddrModeFlat;
	StackFrame.AddrBStore.Mode		= AddrModeFlat;
	StackFrame.AddrReturn.Mode		= AddrModeFlat;

	// Set symbol options.
	SymOptions						|= SYMOPT_LOAD_LINES;
	SymOptions						|= SYMOPT_UNDNAME;
	SymOptions						|= SYMOPT_EXACT_SYMBOLS;
	SymSetOptions( SymOptions );

	// Initialize symbol.
	memset( Symbol, 0, sizeof(SYMBOL_INFO) + MAX_SYMBOL_NAME_LENGTH );
	Symbol->SizeOfStruct			= sizeof(SYMBOL_INFO);
	Symbol->MaxNameLen				= MAX_SYMBOL_NAME_LENGTH;

	// Initialize line number info.
	memset( &Line, 0, sizeof(Line) );
	Line.SizeOfStruct				= sizeof(Line);

	// Load symbols.
	SymInitialize( GetCurrentProcess(), ".", 1 );

	while( 1 )
	{
		if( !StackWalk64( IMAGE_FILE_MACHINE_I386, Process, Thread, &StackFrame, ExceptionInfo->ContextRecord, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL ) )
			break;

		// Warning - ANSI_TO_TCHAR uses alloca which might not be safe during an exception handler - INVESTIGATE!
		if( SymFromAddr( Process, StackFrame.AddrPC.Offset, &Offset64, Symbol ) && SymGetLineFromAddr64( Process, StackFrame.AddrPC.Offset, &Offset, &Line ) )
		{
			appStrncat( GErrorHist, *FString::Printf(TEXT("%s [Line %i] <- "),ANSI_TO_TCHAR(Symbol->Name), Line.LineNumber), ARRAY_COUNT(GErrorHist) );
			debugf(TEXT("%45s   Line %4i of %s"), ANSI_TO_TCHAR(Symbol->Name), Line.LineNumber, ANSI_TO_TCHAR(Line.FileName) );
		}
	}

	SymCleanup( Process );
	appSystemFree( Symbol );
	
	return EXCEPTION_EXECUTE_HANDLER;
}

//
//	WxLaunchApp implementation.
//

/**
 * Gets called on initialization from within wxEntry.	
 */
bool WxLaunchApp::OnInit()
{
	wxApp::OnInit();

	// Initialize XML resources
	wxXmlResource::Get()->InitAllHandlers();
	wxXmlResource::Get()->Load( TEXT("wxRC/UnrealEd*.xrc") );

	// Init subsystems
	InitPropertySubSystem();

	return 1;
}

/** 
 * Gets called after leaving main loop before wxWindows is shut down.
 */
int WxLaunchApp::OnExit()
{
	return wxApp::OnExit();
}

/**
 * Mix of wxWindows and our main loop so we can share messages.
 */
int WxLaunchApp::MainLoop()
{
	m_keepGoing = TRUE;
	while ( m_keepGoing && !GIsRequestingExit )
	{
		// a message came or no more idle processing to do
		DoMessage();

		while( !Pending() && m_keepGoing && !GIsRequestingExit )
		{
			ProcessIdle();

#if wxUSE_THREADS
			wxMutexGuiLeaveOrEnter();
#endif

			GEngineLoop.Tick();

#if wxUSE_THREADS
			wxMutexGuiLeaveOrEnter();
#endif
		}
	}
	return 1;
}


/**
 * The below is a manual expansion of wxWindows's IMPLEMENT_APP to allow multiple wxApps.
 *
 * @warning: when upgrading wxWindows, make sure that the below is how IMPLEMENT_APP looks like
 */
wxApp *wxCreateApp()
{
	wxApp::CheckBuildOptions(wxBuildOptions());
	return GIsEditor ? new WxUnrealEdApp : new WxLaunchApp;
}
wxAppInitializer wxTheAppInitializer((wxAppInitializerFunction) wxCreateApp);
WxLaunchApp& wxGetApp() { return *(WxLaunchApp *)wxTheApp; }


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
