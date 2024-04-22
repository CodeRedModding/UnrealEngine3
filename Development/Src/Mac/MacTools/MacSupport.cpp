/*=============================================================================
	MacTools/MacSupport.cpp: Mac platform support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "MacSupport.h"

#include "..\..\Engine\Classes\PixelFormatEnum.uci"

#include <ddraw.h>

#define COMMON_BUFFER_SIZE	(16 * 1024)

#define RGBA_ONLY 0

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

/**
 * Runs a child process without spawning a command prompt window for each one
 *
 * @param CommandLine The commandline of the child process to run
 * @param Errors Output buffer for any errors
 * @param ErrorsSize Size of the Errors buffer
 * @param ProcessReturnValue Optional out value that the process returned
 *
 * @return TRUE if the process was run (even if the process failed)
 */
bool RunChildProcess(const wchar_t* CommandLine, const wchar_t* WorkingDirectory, char* Errors, int /*ErrorsSize*/, DWORD* ProcessReturnValue)
{
	// run the command (and avoid a window popping up)
// 	SECURITY_ATTRIBUTES SecurityAttr; 
// 	SecurityAttr.nLength = sizeof(SecurityAttr); 
// 	SecurityAttr.bInheritHandle = TRUE; 
// 	SecurityAttr.lpSecurityDescriptor = NULL; 
// 
// 	HANDLE StdOutRead, StdOutWrite;
// 	CreatePipe(&StdOutRead, &StdOutWrite, &SecurityAttr, 0);
// 	SetHandleInformation(StdOutRead, HANDLE_FLAG_INHERIT, 0);

	// set up process spawn structures
	STARTUPINFO StartupInfo;
	memset(&StartupInfo, 0, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
// 	StartupInfo.dwFlags = STARTF_USESTDHANDLES;
// 	StartupInfo.hStdOutput = StdOutWrite;
// 	StartupInfo.hStdError = StdOutWrite;
	PROCESS_INFORMATION ProcInfo;

	wchar_t FinalCommandLine[COMMON_BUFFER_SIZE];
	swprintf(FinalCommandLine, ARRAY_COUNT(FinalCommandLine), L"cmd /C %s", CommandLine);

	// kick off the child process
	if (!CreateProcessW(NULL, (LPWSTR)FinalCommandLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, (LPWSTR)WorkingDirectory, &StartupInfo, &ProcInfo))
	{
		sprintf(Errors, "\nFailed to start process '%S'\n", CommandLine);
		return false;
	}

	bool bProcessComplete = false;
	// read up to 64k of output at once (that would be crazy amount of error, but whatever)
// 	char Buffer[1024 * 64];
// 	DWORD BufferSize = sizeof(Buffer);
	Errors[0] = 0;

	// wait until the process is finished
	while (!bProcessComplete)
	{
		DWORD Reason = WaitForSingleObject(ProcInfo.hProcess, 1000);

// 		// See if we have some data to read
// 		DWORD SizeToRead;
// 		PeekNamedPipe(StdOutRead, NULL, 0, NULL, &SizeToRead, NULL);
// 		while (SizeToRead > 0)
// 		{
// 			// read some output
// 			DWORD SizeRead;
// 			ReadFile(StdOutRead, &Buffer, min(SizeToRead, BufferSize - 1), &SizeRead, NULL);
// 			Buffer[SizeRead] = 0;
// 
// 			// decrease how much we need to read
// 			SizeToRead -= SizeRead;
// 
// 			// append the output to the log
// 			strncpy(Errors, Buffer, ErrorsSize - 1);
// 		}

		// when the process signals, its done
		if (Reason != WAIT_TIMEOUT)
		{
			// break out of the loop
			bProcessComplete = true;
		}
	}

	// Get the return value
	DWORD ErrorCode;
	GetExitCodeProcess(ProcInfo.hProcess, &ErrorCode);

	// fill in the error code is needed
	if (ErrorCode != 0)
	{
		sprintf(Errors, "\nFailed while running '%S'\n", CommandLine);
	}

	// pass back the return code if desired
	if (ProcessReturnValue)
	{
		*ProcessReturnValue = ErrorCode;
	}

	// Close process and thread handles. 
	CloseHandle(ProcInfo.hProcess);
	CloseHandle(ProcInfo.hThread);
// 	CloseHandle(StdOutRead);
// 	CloseHandle(StdOutWrite);

	return true;
}

/**
 * Runs a child process and spawns a progress bar wrapper (with a details button to see the actual output spew if desired)
 *
 * @param CommandLine The commandline of the child process to run
 * @param WorkingDirectory The working directory of the child process to run
 * @param RelativeBinariesDir The binaries directory of Unreal
 * @param Errors Output buffer for any errors
 * @param ErrorsSize Size of the Errors buffer
 * @param ProcessReturnValue Optional out value that the process returned
 *
 * @return TRUE if the process was run (even if the process failed)
 */
bool RunChildProcessWithProgressBar(const wchar_t* CommandLine, const wchar_t* WorkingDirectory, const wchar_t* RelativeBinariesDir, char* Errors, int ErrorsSize, DWORD* ProcessReturnValue)
{
	wchar_t NewCommandLine[COMMON_BUFFER_SIZE];
	wchar_t CurrentWorkingDirectory[COMMON_BUFFER_SIZE];

	// Make sure the working directory is a real string
	GetCurrentDirectoryW(COMMON_BUFFER_SIZE, CurrentWorkingDirectory);
	if (WorkingDirectory == NULL)
	{
		WorkingDirectory = CurrentWorkingDirectory;
	}

	// Construct a new command line to run the UnrealConsole progress bar wrapper
	swprintf(
		NewCommandLine,
		ARRAY_COUNT(NewCommandLine), 
		L"UnrealConsole.exe -wrapexe -pwd %s -cwd %s %s",
		CurrentWorkingDirectory,
		WorkingDirectory,
		CommandLine
		);

	// Run UnrealConsole with it's working directory in the binaries directory
	// (it will use the value from -cwd as the working directory for the true target process)
	return RunChildProcess(NewCommandLine, RelativeBinariesDir, Errors, ErrorsSize, ProcessReturnValue);
}


/**
 * Maps Unreal formats to various settings about a pixel
 */
struct FPixelFormat
{
	UINT BlockSizeX, BlockSizeY, BlockSizeZ, BlockBytes;
	DWORD DDSFourCC;
	bool NeedToSwizzle;
};


struct FMacSkeletalMeshCooker : public FConsoleSkeletalMeshCooker
{
	void Init(void)
	{

	}

	virtual void CookMeshElement(const FSkeletalMeshFragmentCookInfo& ElementInfo, FSkeletalMeshFragmentCookOutputInfo& OutInfo)
	{
		// no optimization needed, just copy the data over
		memcpy(OutInfo.NewIndices, ElementInfo.Indices, ElementInfo.NumTriangles * 3 * sizeof(WORD) );
	}
};

/**
* Mac version of static mesh cooker
*/
struct FMacStaticMeshCooker : public FConsoleStaticMeshCooker
{
	void Init(void)
	{

	}

	/**
	* Cooks a mesh element.
	* @param ElementInfo - Information about the element being cooked
	* @param OutIndices - Upon return, contains the optimized index buffer.
	* @param OutPartitionSizes - Upon return, points to a list of partition sizes in number of triangles.
	* @param OutNumPartitions - Upon return, contains the number of partitions.
	* @param OutVertexIndexRemap - Upon return, points to a list of vertex indices which maps from output vertex index to input vertex index.
	* @param OutNumVertices - Upon return, contains the number of vertices indexed by OutVertexIndexRemap.
	*/
	virtual void CookMeshElement(FMeshFragmentCookInfo& ElementInfo, FMeshFragmentCookOutputInfo& OutInfo)
	{
		// no optimization needed, just copy the data over
		memcpy(OutInfo.NewIndices, ElementInfo.Indices, ElementInfo.NumTriangles * 3 * sizeof(WORD) );
	}
};

/** Mac platform shader precompiler */
struct FMacShaderPrecompiler : public FConsoleShaderPrecompiler
{
	/**
	 * Precompile the shader with the given name. Must be implemented
	 *
	 * @param ShaderPath			Pathname to the source shader file ("..\Engine\Shaders\BasePassPixelShader.usf")
	 * @param EntryFunction			Name of the startup function ("pixelShader")
	 * @param bIsVertexShader		True if the vertex shader is being precompiled
	 * @param CompileFlags			Default is 0, otherwise members of the D3DXSHADER enumeration
	 * @param Definitions			Space separated string that contains shader defines ("FRAGMENT_SHADER=1 VERTEX_LIGHTING=0")
	 * @param bDumpingShaderPDBs	True if shader PDB's should be saved to ShaderPDBPath
	 * @param ShaderPDBPath			Path to save shader PDB's, can be on the local machine if not using runtime compiling.
	 * @param BytecodeBuffer		Block of memory to fill in with the compiled bytecode
	 * @param BytecodeSize			Size of the returned bytecode
	 * @param ConstantBuffer		String buffer to return the shader definitions with [Name,RegisterIndex,RegisterCount] ("WorldToLocal,100,4 Scale,101,1"), NULL = Error case
	 * @param Errors				String buffer any output/errors
	 * 
	 * @return true if successful
	 */
	virtual bool PrecompileShader(
		const char* /*ShaderPath*/, 
		const char* /*EntryFunction*/, 
		bool /*bIsVertexShader*/, 
		DWORD /*CompileFlags*/, 
		const char* /*Definitions*/, 
		const char* /*IncludeDirectory*/,
		char* const* /*IncludeFileNames*/,
		char* const* /*IncludeFileContents*/,
		int /*NumIncludes*/,
		bool /*bDumpingShaderPDBs*/,
		const char* /*ShaderPDBPath*/,
		unsigned char* /*BytecodeBufer*/, 
		int& /*BytecodeSize*/, 
		char* /*ConstantBuffer*/, 
		char* /*Errors*/
		)
	{
		return false;
	}

	/**
	 * Preprocess the shader with the given name. Must be implemented
	 *
	 * @param ShaderPath		Pathname to the source shader file ("..\Engine\Shaders\BasePassPixelShader.usf")
	 * @param Definitions		Space separated string that contains shader defines ("FRAGMENT_SHADER=1 VERTEX_LIGHTING=0")
	 * @param ShaderText		Block of memory to fill in with the preprocessed shader output
	 * @param ShaderTextSize	Size of the returned text
	 * @param Errors			String buffer any output/errors
	 * 
	 * @return true if successful
	 */
	virtual bool PreprocessShader(
		const char* /*ShaderPath*/, 
		const char* /*Definitions*/, 
		const char* /*IncludeDirectory*/,
		char* const* /*IncludeFileNames*/,
		char* const* /*IncludeFileContents*/,
		int /*NumIncludes*/,
		unsigned char* /*ShaderText*/, 
		int& /*ShaderTextSize*/, 
		char* /*Errors*/ 
		)
	{
		return false;
	}

	/**
	* Disassemble the shader with the given byte code. Must be implemented
	*
	* @param ShaderByteCode	The null terminated shader byte code to be disassembled
	* @param ShaderText		Block of memory to fill in with the preprocessed shader output
	* @param ShaderTextSize	Size of the returned text
	* 
	* @return true if successful
	*/
	virtual bool DisassembleShader(
		const DWORD* /*ShaderByteCode*/, 
		unsigned char* /*ShaderText*/, 
		int& /*ShaderTextSize*/)
	{
		return false;
	}

	/**
	* Create a command line to compile the shader with the given parameters. Must be implemented
	*
	* @param ShaderPath		Pathname to the source shader file ("..\Engine\Shaders\BasePassPixelShader.usf")
	* @param IncludePath		Pathname to extra include directory (can be NULL)
	* @param EntryFunction		Name of the startup function ("Main") (can be NULL)
	* @param bIsVertexShader	True if the vertex shader is being precompiled
	* @param CompileFlags		Default is 0, otherwise members of the D3DXSHADER enumeration
	* @param Definitions		Space separated string that contains shader defines ("FRAGMENT_SHADER=1 VERTEX_LIGHTING=0") (can be NULL)
	* @param CommandStr		Block of memory to fill in with the null terminated command line string
	* 
	* @return true if successful
	*/
	virtual bool CreateShaderCompileCommandLine(
		const char* /*ShaderPath*/, 
		const char* /*IncludePath*/, 
		const char* /*EntryFunction*/, 
		bool /*bIsVertexShader*/, 
		DWORD /*CompileFlags*/,
		const char* /*Definitions*/, 
		char* /*CommandStr*/,
		bool /*bPreprocessed*/
		)
	{
		return false;
	}
};

struct FRunGameThreadData
{
	class FMacSupport* SupportObject;
	wstring URL;
	wstring MapName;
	wstring Configuration;
	wstring GameName;
};

#include "MacSupport.h"

FMacSupport::FMacSupport(void* InModule)
{
	Module = InModule;
}

/** Initialize the DLL with some information about the game/editor
 *
 * @param	GameName		The name of the current game ("ExampleGame", "UTGame", etc)
 * @param	Configuration	The name of the configuration to run ("Debug", "Release", etc)
 */
void FMacSupport::Initialize(const wchar_t* InGameName, const wchar_t* InConfiguration)
{
	// cache the parameters
	GameName = InGameName;
	Configuration = InConfiguration;

	RelativeBinariesDir = L"";

	wchar_t CurDir[COMMON_BUFFER_SIZE];
	GetCurrentDirectoryW(COMMON_BUFFER_SIZE, CurDir);
	if (_wcsicmp(CurDir + wcslen(CurDir) - 8, L"Binaries") != 0)
	{
		RelativeBinariesDir = L"..\\";
	}
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
unsigned int FMacSupport::GetIPAddress(TARGETHANDLE Handle)
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
const wchar_t* FMacSupport::GetTargetName(TARGETHANDLE Handle)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);
	
	if(!Target)
	{
		return NULL;
	}

	return Target->Name.c_str();
}

const wchar_t* FMacSupport::GetTargetGameName(TARGETHANDLE Handle)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);
	
	if(!Target)
	{
		return NULL;
	}

	return Target->GameName.c_str();
}

const wchar_t* FMacSupport::GetTargetGameType(TARGETHANDLE Handle)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);
	
	if(!Target)
	{
		return NULL;
	}

	return Target->GameTypeName.c_str();
}

/**
 * Returns the type of the specified target.
 */
FConsoleSupport::ETargetType FMacSupport::GetTargetType(TARGETHANDLE Handle)
{
	CMacTarget* Target = NetworkManager.ConvertTarget(Handle);

	if( Target)
	{
		if( !_wcsicmp( Target->OSVersion.c_str(), L"v3.2" ) )
		{
			return TART_IOS32;
		}
		else if( !_wcsnicmp( Target->OSVersion.c_str(), L"v3.", 3 ) )
		{
			return TART_IOS3x;
		}
		else if( !_wcsicmp( Target->OSVersion.c_str(), L"v4.0" ) )
		{
			return TART_IOS40;
		}
		else if( !_wcsicmp( Target->OSVersion.c_str(), L"v4.1" ) )
		{
			return TART_IOS41;
		}
		else if( !_wcsicmp( Target->OSVersion.c_str(), L"<remote>" ) )
		{
			return TART_Remote;
		}
	}

	return TART_Unknown ;
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
bool FMacSupport::ConnectToTarget(TARGETHANDLE Handle)
{
	return NetworkManager.ConnectToTarget(Handle);
}

/**
 * Called after an atomic operation to close any open connections
 *
 * @param Handle The handle of the console to disconnect.
 */
void FMacSupport::DisconnectFromTarget(TARGETHANDLE Handle)
{
	NetworkManager.DisconnectTarget(Handle);
}

/**
 * Exits the current instance of the game and reboots the target if appropriate. Must be implemented
 *
 * @param Handle The handle of the console to reset
 * 
 * @return true if successful
 */
bool FMacSupport::ResetTargetToShell(TARGETHANDLE, bool)
{
	return false;
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
bool FMacSupport::RunGameOnTarget(TARGETHANDLE, const wchar_t* GameName, const wchar_t* Configuration, const wchar_t*, const wchar_t*)
{
	wchar_t CommandLine[COMMON_BUFFER_SIZE];
	wchar_t WorkingDir[COMMON_BUFFER_SIZE];
	char ErrorsRaw[COMMON_BUFFER_SIZE];
	DWORD ReturnValue;

	swprintf(
		CommandLine,
		ARRAY_COUNT(CommandLine), 
		L"%s\\Mac\\MacPackager.exe deploy %s %s -interactive",
		RelativeBinariesDir.c_str(),
		GameName,
		Configuration
	);
	swprintf(
		WorkingDir,
		ARRAY_COUNT(WorkingDir), 
		L"%s\\Mac",
		RelativeBinariesDir.c_str()
	);

	// IPP expects the working directory to be it's own directory
	ErrorsRaw[0] = 0;
	RunChildProcessWithProgressBar(CommandLine, WorkingDir, RelativeBinariesDir.c_str(), ErrorsRaw, sizeof(ErrorsRaw) - 1, &ReturnValue);
	if (ReturnValue == 0)
	{
		MessageBox(NULL, L"Deployed successfully, you can now start the application on your iOS device", L"Finished installing on iOS device", MB_ICONINFORMATION | MB_SETFOREGROUND);
	}

	return (ReturnValue == 0);
}

/**
 * This function is run on a separate thread for cooking, copying, and running an autosaved level.
 *
 * @param	Data	A pointer to data passed into the thread providing it with extra data needed to do its job.
 */
void __cdecl FMacSupport::RunGameThread(void* Data)
{
	FRunGameThreadData *ThreadData = (FRunGameThreadData*)Data;

	wchar_t* URLCopy = _wcsdup(ThreadData->URL.c_str());

	bool bUseDebugExecutable = false;

	wchar_t CommandLine[COMMON_BUFFER_SIZE];
	wchar_t WorkingDir[COMMON_BUFFER_SIZE];
	wchar_t Errors[COMMON_BUFFER_SIZE];
	char ErrorsRaw[COMMON_BUFFER_SIZE];
	DWORD ReturnValue;

	// Cook the map
	{
		// A special workaround for UDKM (similar to the one in WindowsSupport for UDK)
		// Use the module name (the running exe of the Editor) as the binary to use for cooking
		WCHAR ModuleNameString[MAX_PATH];

		// Just get the executable name, for UDK purposes
		GetModuleFileNameW( NULL, ModuleNameString, MAX_PATH );
		_wcslwr( ModuleNameString );
		wstring ModuleName = ModuleNameString;

		// Make sure we launch the command line friendlier shell
		size_t Position = ModuleName.find( L".exe" );
		if( Position != wstring::npos )
		{
			ModuleName.replace( Position, 4, L".com" );
		}

		swprintf(
			CommandLine,
			ARRAY_COUNT(CommandLine), 
			L"%s cookpackages %s -full -platform=mac",
			ModuleName.c_str(),
			ThreadData->MapName.c_str()
		);

		// Use the working directory of the current process for this task
		ErrorsRaw[0] = 0;
		RunChildProcessWithProgressBar(CommandLine, NULL, ThreadData->SupportObject->RelativeBinariesDir.c_str(), ErrorsRaw, sizeof(ErrorsRaw) - 1, &ReturnValue);
		if (ReturnValue != 0)
		{
			swprintf(Errors, ARRAY_COUNT(Errors), L"%S", ErrorsRaw);
			MessageBox(NULL, Errors, L"Cooking failed", MB_ICONSTOP | MB_SETFOREGROUND);
			return;
		}

	}

	// Update the TOC
	{
		swprintf(
			CommandLine,
			ARRAY_COUNT(CommandLine), 
			L"%sCookerSync %s -p Mac -nd",
			ThreadData->SupportObject->RelativeBinariesDir.c_str(),
			ThreadData->GameName.c_str()
		);

		// Use the working directory of the current process for this task
		ErrorsRaw[0] = 0;
		RunChildProcessWithProgressBar(CommandLine, NULL, ThreadData->SupportObject->RelativeBinariesDir.c_str(), ErrorsRaw, sizeof(ErrorsRaw) - 1, &ReturnValue);
		if (ReturnValue != 0)
		{
			swprintf(Errors, ARRAY_COUNT(Errors), L"%S", ErrorsRaw);
			MessageBox(NULL, Errors, L"CookerSync failed", MB_ICONSTOP | MB_SETFOREGROUND);
			return;
		}
	}

	// Save out the command line
	{
		wchar_t CommandLineFilePath[COMMON_BUFFER_SIZE];
		swprintf(
			CommandLineFilePath,
			ARRAY_COUNT(CommandLineFilePath),
			L"%s..\\%s\\CookedMac\\UE3CommandLine.txt",
			ThreadData->SupportObject->RelativeBinariesDir.c_str(),
			ThreadData->GameName.c_str()
		);

		// Open the command line file
		FILE* CommandLineFile = _wfopen(CommandLineFilePath, L"w");

		// Write out the URL to the command line
		fwprintf(CommandLineFile, URLCopy);
		fclose(CommandLineFile);
	}

	// Package up the application
	{
		swprintf(
			CommandLine,
			ARRAY_COUNT(CommandLine), 
			L"%s\\Mac\\MacPackager.exe RepackageIPA %s %s -interactive -sign -compress=none",
			ThreadData->SupportObject->RelativeBinariesDir.c_str(),
			ThreadData->GameName.c_str(),
			ThreadData->Configuration.c_str()
		);
		swprintf(
			WorkingDir,
			ARRAY_COUNT(WorkingDir), 
			L"%s\\Mac",
			ThreadData->SupportObject->RelativeBinariesDir.c_str()
		);

		// ITP expects the working directory to be it's own directory
		ErrorsRaw[0] = 0;
		RunChildProcessWithProgressBar(CommandLine, WorkingDir, ThreadData->SupportObject->RelativeBinariesDir.c_str(), ErrorsRaw, sizeof(ErrorsRaw) - 1, &ReturnValue);
		if (ReturnValue != 0)
		{
			swprintf(Errors, ARRAY_COUNT(Errors), L"%S", ErrorsRaw);
			MessageBox(NULL, Errors, L"Packaging failed", MB_ICONSTOP | MB_SETFOREGROUND);
			return;
		}
	}

	{
		ThreadData->SupportObject->RunGameOnTarget(0, ThreadData->GameName.c_str(), bUseDebugExecutable ? L"Debug" : L"Release", URLCopy, L"");
	}

	free(URLCopy);
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
bool FMacSupport::RunGame(TARGETHANDLE* /*TargetList*/, int /*NumTargets*/, const wchar_t* MapName, const wchar_t* URL, wchar_t* /*OutputConsoleCommand*/, int /*CommandBufferSize*/)
{
	FRunGameThreadData *Data = new FRunGameThreadData();

	Data->SupportObject = this;
	Data->MapName = MapName;
	Data->GameName = GameName;
	Data->Configuration = Configuration;
	Data->URL = URL;

	// Do all cooking, copying, and running on a separate thread so the UI doesn't hang.
	_beginthread(&FMacSupport::RunGameThread, 0, Data);

	return true;
}

/**
 * Return the number of console-specific menu items this platform wants to add to the main
 * menu in UnrealEd.
 *
 * @return	The number of menu items for this console
 */
int FMacSupport::GetNumMenuItems()
{
	return 1;
}

/**
 * Return the string label for the requested menu item
 * @param	Index		The requested menu item
 * @param	bIsChecked	Is this menu item checked (or selected if radio menu item)
 * @param	bIsRadio	Is this menu item part of a radio group?
 * @param	OutHandle	Receives the handle of the target associated with the menu item.
 *
 * @return	Label for the requested menu item
 */
const wchar_t* FMacSupport::GetMenuItem(int /*Index*/, bool& /*bIsChecked*/, bool& /*bIsRadio*/, TARGETHANDLE& /*OutHandle*/)
{
// 		// Just have a default target for now, always checked
// 		assert(Index == 0);
// 		bIsChecked = true;
// 		bIsRadio = false;
// 
// 		// Get the first target
// 		INT NumHandles = 1;
// 		NetworkManager.GetTargets(&OutHandle, &NumHandles);
// 
// 		TargetPtr Target = NetworkManager.GetTarget(OutHandle);
// 		return Target->Name.c_str();

	// Until we have control over real targets, simply say we have one
	return L"iOS Device";
}
/**
 * Gets a list of targets that have been selected via menus in UnrealEd.
 *
 * @param	OutTargetList			The list to be filled with target handles.
 * @param	InOutTargetListSize		Contains the size of OutTargetList. When the function returns it contains the # of target handles copied over.
 */
void FMacSupport::GetMenuSelectedTargets(TARGETHANDLE* OutTargetList, int &InOutTargetListSize)
{
// 		TARGETHANDLE Handles[1];
// 		INT NumHandles = 1;
// 		
// 		// Get the first target
// 		NetworkManager.GetTargets(Handles);
// 		
// 		InOutTargetListSize = 1;
// 		OutTargetList[0] = Handles[0];

	// Until we have control over real targets, simply say we have one
	InOutTargetListSize = 1;
	OutTargetList[0] = NULL;
}


/**
 * Retrieve the state of the console (running, not running, crashed, etc)
 *
 * @param Handle The handle of the console to retrieve the information from.
 *
 * @return the current state
 */
FConsoleSupport::ETargetState FMacSupport::GetTargetState(TARGETHANDLE Handle)
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
void FMacSupport::Heartbeat(TARGETHANDLE Handle)
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
void FMacSupport::ResolveAddressToString(unsigned int /*Address*/, wchar_t* OutString, int OutLength)
{
	OutString[0] = 0;
	OutLength = 0;
}

/**
 * Send a text command to the target
 * 
 * @param Handle The handle of the console to retrieve the information from.
 * @param Command Command to send to the target
 */
void FMacSupport::SendConsoleCommand(TARGETHANDLE Handle, const wchar_t* Command)
{
	if(!Command || wcslen(Command) == 0)
	{
		return;
	}

	NetworkManager.SendToConsole(Handle, Command);
}

/**
 * Returns the global sound cooker object.
 *
 * @return global sound cooker object, or NULL if none exists
 */
FConsoleSoundCooker* FMacSupport::GetGlobalSoundCooker()
{
	return NULL;
}

/**
 * Returns the global texture cooker object.
 *
 * @return global sound cooker object, or NULL if none exists
 */
FConsoleTextureCooker* FMacSupport::GetGlobalTextureCooker()
{
	return NULL;
}

/**
 * Returns the global skeletal mesh cooker object.
 *
 * @return global skeletal mesh cooker object, or NULL if none exists
 */
FConsoleSkeletalMeshCooker* FMacSupport::GetGlobalSkeletalMeshCooker() 
{ 
	static FMacSkeletalMeshCooker* GlobalSkeletalMeshCooker = NULL;
	if( !GlobalSkeletalMeshCooker )
	{
		GlobalSkeletalMeshCooker = new FMacSkeletalMeshCooker();
	}
	return GlobalSkeletalMeshCooker;
}

/**
 * Returns the global static mesh cooker object.
 *
 * @return global static mesh cooker object, or NULL if none exists
 */
FConsoleStaticMeshCooker* FMacSupport::GetGlobalStaticMeshCooker() 
{ 
	static FMacStaticMeshCooker* GlobalStaticMeshCooker = NULL;
	if( !GlobalStaticMeshCooker )
	{
		GlobalStaticMeshCooker = new FMacStaticMeshCooker();
	}
	return GlobalStaticMeshCooker;
}

FConsoleShaderPrecompiler* FMacSupport::GetGlobalShaderPrecompiler()
{
	static FMacShaderPrecompiler* GlobalShaderPrecompiler = NULL;
	if(!GlobalShaderPrecompiler)
	{
		GlobalShaderPrecompiler = new FMacShaderPrecompiler();
	}
	return GlobalShaderPrecompiler;
}


/**
 * Retrieves the handle of the default console.
 */
TARGETHANDLE FMacSupport::GetDefaultTarget()
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
int FMacSupport::GetTargets(TARGETHANDLE* OutTargetList)
{
	return NetworkManager.GetTargets(OutTargetList);
}

/**
 * Sets the callback function for TTY output.
 *
 * @param	Callback	Pointer to a callback function or NULL if TTY output is to be disabled.
 * @param	Handle		The handle to the target that will register the callback.
 */
void FMacSupport::SetTTYCallback(TARGETHANDLE Handle, TTYEventCallbackPtr Callback)
{
	TargetPtr Target = NetworkManager.GetTarget(Handle);

	if(Target)
	{
		Target->TxtCallback = Callback;
	}
}

/**
* Sets the callback function for handling crashes.
*
* @param	Callback	Pointer to a callback function or NULL if handling crashes is to be disabled.
* @param	Handle		The handle to the target that will register the callback.
*/
void FMacSupport::SetCrashCallback(TARGETHANDLE Handle, CrashCallbackPtr Callback)
{
	CMacTarget* Target = NetworkManager.ConvertTarget(Handle);
	if (Target)
	{
		Target->CrashCallback = Callback;
	}
}

/**
 * Starts the (potentially async) process of enumerating all available targets
 */
void FMacSupport::EnumerateAvailableTargets()
{
	// Search for available targets
	NetworkManager.Initialize();
	NetworkManager.DetermineTargets();
}

/**
 * Forces a stub target to be created to await connection
 *
 * @Param TargetAddress IP of target to add
 *
 * @returns Handle of new stub target
 */
TARGETHANDLE FMacSupport::ForceAddTarget( const wchar_t* TargetAddress )
{
	return( NetworkManager.ForceAddTarget( TargetAddress ) );
}


CONSOLETOOLS_API FConsoleSupport* GetConsoleSupport(void* Module)
{
	static FMacSupport* MacSupport = NULL;
	if( MacSupport == NULL )
	{
		try
		{
			MacSupport = new FMacSupport(Module);
		}
		catch( ... )
		{
			MacSupport = NULL;
		}
	}

	return MacSupport;
}
