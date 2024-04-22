/*=============================================================================
	WindowsTools/WindowsSupport.cpp: Windows platform support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "WindowsSupport.h"

/**
 * Windows callback to close a window for a target process
 */
BOOL CALLBACK CloseWindowFunc( HWND hWnd, LPARAM lParam )
{
	DWORD TargetProcessId = (DWORD)lParam;
	DWORD CurrentProcessId;
	GetWindowThreadProcessId( hWnd, &CurrentProcessId );

	if( CurrentProcessId == TargetProcessId )
	{
		PostMessage( hWnd, WM_CLOSE, 0, 0 );
	}

	return TRUE;
}

#include "WindowsSupport.h"

CWindowsSupport::CWindowsSupport(void* InModule)
{
	Module = InModule;
}

CWindowsSupport::~CWindowsSupport()
{
}

/** Initialize the DLL with some information about the game/editor
 *
 * @param	GameName		The name of the current game ("ExampleGame", "UTGame", etc)
 * @param	Configuration	The name of the configuration to run ("Debug", "Release", etc)
 */
void CWindowsSupport::Initialize(const wchar_t* GameName, const wchar_t* Configuration)
{
	// cache the parameters
	this->GameName = GameName;
	this->Configuration = Configuration;
}

/**
 * Returns the type of the specified target.
 */
FConsoleSupport::ETargetType CWindowsSupport::GetTargetType(TARGETHANDLE Handle)
{
	ETargetType RetVal = TART_Unknown;
	TargetPtr Target = NetworkManager.GetTarget(Handle);

	if(Target)
	{
		RetVal = TART_Remote;
	}

	return RetVal;
}

/**
 * Return the default IP address to use when sending data into the game for object propagation
 * Note that this is probably different than the IP address used to debug (reboot, run executable, etc)
 * the console. (required to implement)
 *
 * @param	Handle The handle of the console to retrieve the information from.
 *
 * @return	The in-game IP address of the console, in an Intel byte order 32 bit integer
 */
unsigned int CWindowsSupport::GetIPAddress(TARGETHANDLE Handle)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);
	if (!Target)
	{
		return 0;
	}

	return Target->GetRemoteAddress().sin_addr.s_addr;
}

/**
 * Get the name of the specified target
 *
 * @param	Handle The handle of the console to retrieve the information from.
 * @return Name of the target, or NULL if the Index is out of bounds
 */
const wchar_t* CWindowsSupport::GetTargetName(TARGETHANDLE Handle)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);
	
	if(!Target)
	{
		return NULL;
	}

	return Target->Name.c_str();
}

const wchar_t* CWindowsSupport::GetTargetGameName(TARGETHANDLE Handle)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);
	
	if(!Target)
	{
		return NULL;
	}

	return Target->GameName.c_str();
}

const wchar_t* CWindowsSupport::GetTargetGameType(TARGETHANDLE Handle)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);
	
	if(!Target)
	{
		return NULL;
	}

	return Target->GameTypeName.c_str();
}

/**
 * Open an internal connection to a target. This is used so that each operation is "atomic" in 
 * that connection failures can quickly be 'remembered' for one "sync", etc operation, so
 * that a connection is attempted for each file in the sync operation...
 * For the next operation, the connection can try to be opened again.
 *
 * @param Handle The handle of the console to connect to.
 *
 * @return false for failure.
 */
bool CWindowsSupport::ConnectToTarget(TARGETHANDLE Handle)
{
	return NetworkManager.ConnectToTarget(Handle);
}

/**
 * Called after an atomic operation to close any open connections
 *
 * @param Handle The handle of the console to disconnect.
 */
void CWindowsSupport::DisconnectFromTarget(TARGETHANDLE Handle)
{
	NetworkManager.DisconnectTarget(Handle);
}

/**
 * Reboots the target console. Must be implemented
 *
 * @param Handle			The handle of the console to retrieve the information from.
 * @param Configuration		Build type to run (Debug, Release, RelaseLTCG, etc)
 * @param BaseDirectory		Location of the build on the console (can be empty for platforms that don't copy to the console)
 * @param GameName			Name of the game to run (Example, UT, etc)
 * @param URL				Optional URL to pass to the executable
 * @param bForceGameName	Forces the name of the executable to be only what's in GameName instead of being auto-generated
 * 
 * @return true if successful
 */
bool CWindowsSupport::RunGameOnTarget(TARGETHANDLE, const wchar_t*, const wchar_t*, const wchar_t*, const wchar_t*)
{
	return false;
}

/**
 * Run the game on the target console (required to implement)
 *
 * @param	TargetList				The list of handles of consoles to run the game on.
 * @param	NumTargets				The number of handles in TargetList.
 * @param	MapName					The name of the map that is going to be loaded.
 * @param	URL						The map name and options to run the game with
 * @param	OutputConsoleCommand	A buffer that the menu item can fill out with a console command to be run by the Editor on return from this function
 * @param	CommandBufferSize		The size of the buffer pointed to by OutputConsoleCommand
 *
 * @return	Returns true if the run was successful
 */
bool CWindowsSupport::RunGame(TARGETHANDLE* /*TargetList*/, int NumTargets, const wchar_t* MapName, const wchar_t* /*URL*/, wchar_t* /*OutputConsoleCommand*/, int /*CommandBufferSize*/)
{
	const INT MaxCmdLine = 4096;
	WCHAR ModuleNameString[MaxCmdLine];

	// Just get the executable name, for UDK purposes
	GetModuleFileNameW( NULL, ModuleNameString, MaxCmdLine );
	_wcslwr( ModuleNameString );
	wstring ModuleName = ModuleNameString;

	// Special hack for UDK
	if( NumTargets < 0 )
	{
		size_t Position = ModuleName.find( L"win64" );
		if( Position != wstring::npos )
		{
			ModuleName.replace( Position, 5, L"win32" );
		}
	}

	// ... add in the commandline
	ModuleName += L" ";
	ModuleName += MapName;

	// Copy back to a modifiable location
	wcscpy_s( ModuleNameString, MaxCmdLine, ModuleName.c_str() );

	// Minimal startup info 
	STARTUPINFOW Startupinfo = { 0 };
	Startupinfo.cb = sizeof( STARTUPINFO );
	Startupinfo.wShowWindow = SW_HIDE;
	Startupinfo.dwFlags = STARTF_USESHOWWINDOW;

	// Return value (handles and threads)
	ZeroMemory(&ProcessInfo, sizeof(ProcessInfo) );

	if( CreateProcessW( NULL, ModuleNameString, NULL, NULL, FALSE, CREATE_NEW_PROCESS_GROUP, NULL, NULL, &Startupinfo, &ProcessInfo ) == 0 )
	{
		// Failed to execute
		WCHAR MessageBuffer[MaxCmdLine] = { 0 };

		DWORD ErrorCode = GetLastError();
		FormatMessageW( FORMAT_MESSAGE_FROM_SYSTEM, NULL, ErrorCode, 0, MessageBuffer, MaxCmdLine, NULL );

		MessageBoxW( NULL, MessageBuffer, L"Failed to Launch Game!", MB_ICONERROR );
		return false;
	}
	return true;
}

/**
 * Exit the game on the target console
 *
 * @param	TargetList				The list of handles of consoles to run the game on.
 * @param	NumTargets				The number of handles in TargetList.
 * @param	WaitForShutdown			TRUE if this function waits for the game to shutdown, FALSE if it should return immediately
 *
 * @return	Returns true if the function was successful
 */

bool CWindowsSupport::ResetTargetToShell(TARGETHANDLE /*TargetList*/, bool WaitForShutdown)
{
	// Send a close message to all windows belonging to the game's process.
	EnumWindows( (WNDENUMPROC)CloseWindowFunc, (LPARAM)ProcessInfo.dwProcessId );

	// Wait for the game to shutdown if requested
	if( WaitForShutdown )
	{
		// Amount of time to wait for the game to exit
		const DWORD WaitTime = 10000;
		if( WaitForSingleObject( ProcessInfo.hProcess, WaitTime ) != WAIT_OBJECT_0 )
		{
			// We timed out waiting for the process to shut down nicely.  Force it to shutdown now
			TerminateProcess( ProcessInfo.hProcess, 0 );
		}
	}

	return true;
}

/**
 * Gets a list of targets that have been selected via menu's in UnrealEd.
 *
 * @param	OutTargetList			The list to be filled with target handles.
 * @param	InOutTargetListSize		Contains the size of OutTargetList. When the function returns it contains the # of target handles copied over.
 */
void CWindowsSupport::GetMenuSelectedTargets(TARGETHANDLE* /*OutTargetList*/, int &InOutTargetListSize)
{
	InOutTargetListSize = 0;
}


/**
 * Retrieve the state of the console (running, not running, crashed, etc)
 *
 * @param Handle The handle of the console to retrieve the information from.
 *
 * @return the current state
 */
FConsoleSupport::ETargetState CWindowsSupport::GetTargetState(TARGETHANDLE Handle)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);

	if(!Target)
	{
		return TS_Unconnected;
	}

	return TS_Running;
}

/**
 * Allow for target to perform periodic operations
 */
void CWindowsSupport::Heartbeat(TARGETHANDLE Handle)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);
	if (Target)
	{
		Target->Tick();
	}
}

/**
 * Turn an address into a symbol for callstack dumping
 * 
 * @param Address Code address to be looked up
 * @param OutString Function name/symbol of the given address
 * @param OutLength Size of the OutString buffer
 */
void CWindowsSupport::ResolveAddressToString(unsigned int Address, wchar_t* OutString, int OutLength)
{
	swprintf_s(OutString, OutLength, L"%s", ToWString(inet_ntoa(*(in_addr*) &Address)).c_str());
}

/**
 * Send a text command to the target
 * 
 * @param Handle The handle of the console to retrieve the information from.
 * @param Command Command to send to the target
 */
void CWindowsSupport::SendConsoleCommand(TARGETHANDLE Handle, const wchar_t* Command)
{
	if(!Command || wcslen(Command) == 0)
	{
		return;
	}

	NetworkManager.SendToConsole(Handle, Command);
}

/**
 * Retrieves the handle of the default console.
 */
TARGETHANDLE CWindowsSupport::GetDefaultTarget()
{
	TargetPtr Target = NetworkManager.GetDefaultTarget();

	if(Target)
	{
		return Target.GetHandle();
	}

	return INVALID_TARGETHANDLE;
}

/**
 * Retrieves a handle to each available target.
 *
 * @param	OutTargetList			An array to copy all of the target handles into.
 */
int CWindowsSupport::GetTargets(TARGETHANDLE *OutTargetList)
{
	return NetworkManager.GetTargets(OutTargetList);
}

/**
 * Sets the callback function for TTY output.
 *
 * @param	Callback	Pointer to a callback function or NULL if TTY output is to be disabled.
 * @param	Handle		The handle to the target that will register the callback.
 */
void CWindowsSupport::SetTTYCallback(TARGETHANDLE Handle, TTYEventCallbackPtr Callback)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);

	if(Target)
	{
		Target->TxtCallback = Callback;
	}
}

/**
 * Starts the (potentially async) process of enumerating all available targets
 */
void CWindowsSupport::EnumerateAvailableTargets()
{
	// Search for available targets
	NetworkManager.Initialize();
	NetworkManager.DetermineTargets();
}


CONSOLETOOLS_API FConsoleSupport* GetConsoleSupport(void* Module)
{
	static CWindowsSupport* WindowsSupport = NULL;
	if( WindowsSupport == NULL )
	{
		try
		{
			WindowsSupport = new CWindowsSupport(Module);
		}
		catch( ... )
		{
			WindowsSupport = NULL;
		}
	}

	return WindowsSupport;
}