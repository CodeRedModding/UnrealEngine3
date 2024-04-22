/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _ANDROIDSUPPORT_H_
#define _ANDROIDSUPPORT_H_

#include "AndroidNetworkManager.h"
#include "ConsoleFileManifest.h"

/**
 * Target helper struct, for each device plugged in
 */
struct FAndroidTarget
{
	/**
	 * Constructor that takes an ANSI name
	 */
	FAndroidTarget(const char* InName);

	/**
	 * Terminates any child processes that the target has started
	 */
	void KillChildProcess();

	/**
	 * Handle sending a string to TTY listeners (ie UnrealConsole)
	 */
	void OnTTY(const wchar_t *Txt);

	/**
	 * Handle sending a string to TTY listeners (ie UnrealConsole)
	 */
	void OnTTY(const string &Txt);

	/**
	 * Process a line of text, looking for crashes
	 */
	void ParseLine(const string &Line);

	/**
	 * Parse a callstack of address to text
	 */
	bool GenerateCallstack(std::vector<DWORD> &CallstackAddresses);

	/**
	 * Look for any incoming stdout from adb logcat, and process it
	 */
	void ProcessTextFromChildProcess();

	/** Name of the target */
	wstring TargetName;

	/** Are we currently 'connected' to the device to send console commands, etc? */
	bool bIsConnected;

	/** Function to call when we get any TTY output from the game */
	TTYEventCallbackPtr TxtCallback;

	/** The callback to be called when a crash has occured. */
	CrashCallbackPtr CrashCallback;

	/** Handle for logcat to write to */
	HANDLE TxtHandleWrite;

	/** Handle for reading output from logcat */
	HANDLE TxtHandleRead;

	/** Process */
	PROCESS_INFORMATION TxtProcess;

	/** This is the buffer for TTY output as only complete lines are outputted. */
	string TTYBuffer;

	/** Holds the addresses of a callstack */
	vector<DWORD> Callstack;

	/** Whether the port forwarding has been setup yet */
	bool bHasSetupSocket;

	/** The UDP socket to send console commands over */
	FAndroidSocket CommandSocket;
};


class FAndroidSupport : public FConsoleSupport
{
public:
	FAndroidSupport(void* Module);

	virtual ~FAndroidSupport();

	/** Initialize the DLL with some information about the game/editor
	 *
	 * @param	GameName		The name of the current game ("ExampleGame", "UTGame", etc)
	 * @param	Configuration	The name of the configuration to run ("Debug", "Release", etc)
	 */
	virtual void Initialize(const wchar_t* InGameName, const wchar_t* InConfiguration);

	/**
	 * Return a void* (actually an HMODULE) to the loaded DLL instance
	 *
	 * @return	The HMODULE of the DLL
	 */
	virtual void* GetModule()
	{
		return Module;
	}

	/**
	 * Return a string name descriptor for this game (required to implement)
	 *
	 * @return	The name of the game
	 */
	virtual const wchar_t* GetGameName()
	{
		return GameName.c_str();
	}

	/**
	 * Return a string name descriptor for this configuration (required to implement)
	 *
	 * @return	The name of the configuration
	 */
	virtual const wchar_t* GetConfiguration()
	{
		return Configuration.c_str();
	}

	/**
	 * Return a string name descriptor for this platform (required to implement)
	 *
	 * @return	The name of the platform
	 */
	virtual const wchar_t* GetPlatformName()
	{
		return CONSOLESUPPORT_NAME_ANDROID;
	}

	/**
	 * Returns the platform of the specified target.
	 */
	virtual FConsoleSupport::EPlatformType GetPlatformType()
	{
		return EPlatformType_Android;
	}

	/**
	 * @return true if this platform needs to have files copied from PC->target (required to implement)
	 */
	virtual bool PlatformNeedsToCopyFiles()
	{
		return true;
	}

	/**
	 * Return whether or not this console Intel byte order (required to implement)
	 *
	 * @return	True if the console is Intel byte order
	 */
	virtual bool GetIntelByteOrder()
	{
		return true;
	}

	/**
	 * Retrieves the handle of the default console.
	 */
	virtual TARGETHANDLE GetDefaultTarget();

	/**
	 * Starts the (potentially async) process of enumerating all available targets
	 */
	virtual void EnumerateAvailableTargets();

	/**
	 * Retrieves a handle to each available target.
	 *
	 * @param	OutTargetList			An array to copy all of the target handles into.
	 */
	virtual int GetTargets(TARGETHANDLE *OutTargetList);

	/**
	 * Returns the type of the specified target.
	 */
	virtual FConsoleSupport::ETargetType GetTargetType(TARGETHANDLE Handle);

	/**
	 * Get the name of the specified target
	 *
	 * @param	Handle The handle of the console to retrieve the information from.
	 * @return Name of the target, or NULL if the Index is out of bounds
	 */
	virtual const wchar_t* GetTargetName(TARGETHANDLE Handle);

	/**
	 * Return the default IP address to use when sending data into the game for object propagation
	 * Note that this is probably different than the IP address used to debug (reboot, run executable, etc)
	 * the console. (required to implement)
	 *
	 * @param	Handle The handle of the console to retrieve the information from.
	 *
	 * @return	The in-game IP address of the console, in an Intel byte order 32 bit integer
	 */
	virtual unsigned int GetIPAddress(TARGETHANDLE Handle);

	/**
	 * Sets the callback function for handling crashes.
	 *
	 * @param	Callback	Pointer to a callback function or NULL if handling crashes is to be disabled.
	 * @param	Handle		The handle to the target that will register the callback.
	 */
	virtual void SetCrashCallback(TARGETHANDLE Handle, CrashCallbackPtr Callback);

	/**
	 * Sets the callback function for TTY output.
	 *
	 * @param	Callback	Pointer to a callback function or NULL if TTY output is to be disabled.
	 * @param	Handle		The handle to the target that will register the callback.
	 */
	virtual void SetTTYCallback(TARGETHANDLE Handle, TTYEventCallbackPtr Callback);

	/**
	 * Retrieve the state of the console (running, not running, crashed, etc)
	 *
	 * @param Handle The handle of the console to retrieve the information from.
	 *
	 * @return the current state
	 */
	virtual FConsoleSupport::ETargetState GetTargetState(TARGETHANDLE Handle);

	/**
	 * Send a text command to the target
	 * 
	 * @param Handle The handle of the console to retrieve the information from.
	 * @param Command Command to send to the target
	 */
	virtual void SendConsoleCommand(TARGETHANDLE Handle, const wchar_t* Command);

	/**
	 * Allow for target to perform periodic operations
	 */
	virtual void Heartbeat(TARGETHANDLE Handle);

	/**
	 * Open an internal connection to a target.
	 *
	 * @param Handle The handle of the console to connect to.
	 *
	 * @return false for failure.
	 */
	virtual bool ConnectToTarget(TARGETHANDLE Handle);

	/**
	 * Called after an atomic operation to close any open connections
	 *
	 * @param Handle The handle of the console to disconnect.
	 */
	virtual void DisconnectFromTarget(TARGETHANDLE Handle);

	/**
	 * Exits the current instance of the game and reboots the target if appropriate. Must be implemented
	 *
	 * @param Handle The handle of the console to reset
	 * 
	 * @return true if successful
	 */
	virtual bool ResetTargetToShell(TARGETHANDLE Handle, bool bWaitForReset);

	/**
	 * Copies a single file from PC to target
	 *
	 * @param Handle		The handle of the console to retrieve the information from.
	 * @param SourceFilename Path of the source file on PC
	 * @param DestFilename Platform-independent destination filename (ie, no xe:\\ for Xbox, etc)
	 *
	 * @return true if successful
	 */
	virtual bool CopyFileToTarget(TARGETHANDLE Handle, const wchar_t* SourceFilename, const wchar_t* DestFilename);

	/**
	 *	Runs an instance of the selected game on the target console
	 *
	 *  @param	Handle					The handle of the console to retrieve the information from.
	 *	@param	GameName				The root game name (e.g. Gear)
	 *	@param	Configuration			The configuration to run (e.g. Release)
	 *	@param	URL						The command line to pass to the running instance
	 *	@param	BaseDirectory			The base directory to run from on the console
	 *	
	 *	@return	bool					true if successful, false otherwise
	 */
	virtual bool RunGameOnTarget(TARGETHANDLE Handle, const wchar_t* GameName, const wchar_t* Configuration, const wchar_t* URL, const wchar_t* BaseDirectory);

	/**
	 * Returns the global sound cooker object.
	 *
	 * @return global sound cooker object, or NULL if none exists
	 */
	virtual FConsoleSoundCooker* GetGlobalSoundCooker();

	/**
	 * Returns the global texture cooker object.
	 *
	 * @return global sound cooker object, or NULL if none exists
	 */
	virtual FConsoleTextureCooker* GetGlobalTextureCooker();

	/**
	 * Returns the global skeletal mesh cooker object.
	 *
	 * @return global skeletal mesh cooker object, or NULL if none exists
	 */
	virtual FConsoleSkeletalMeshCooker* GetGlobalSkeletalMeshCooker();

	/**
	 * Returns the global static mesh cooker object.
	 *
	 * @return global static mesh cooker object, or NULL if none exists
	 */
	virtual FConsoleStaticMeshCooker* GetGlobalStaticMeshCooker();

	/**
	 * Returns the global shader precompiler object.
	 *
	 * @return global shader precompiler object, or NULL if none exists.
	 */
	virtual FConsoleShaderPrecompiler* GetGlobalShaderPrecompiler();

	/**
	 * Returns the global shader precompiler object.
	 *
	 * @return global symbol parser object, or NULL if none exists.
	 */
	virtual FConsoleSymbolParser* GetGlobalSymbolParser()
	{
		return NULL;
	}

protected:
	/** Handle to the dll */
	void* Module;

	/** Helper object that tracks network connections to all iPhones */
	FAndroidNetworkManager NetworkManager;

	/** Cache the gamename coming from the editor */
	wstring GameName;

	/** Cache the configuration (debug/release) of the editor */
	wstring Configuration;

	/** Relative path from current working directory to the binaries directory */
	wstring RelativeBinariesDir;

	/** Discovered targets */
	std::vector<FAndroidTarget*> AndroidTargets;

	/** Which target is selected in the menu */
	int SelectedTargetIndex;

	/** Location of android tools (usually C:\Android) */
	string AndroidRoot;

public:

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
	virtual bool RunGame(TARGETHANDLE *TargetList, int NumTargets, const wchar_t* MapName, const wchar_t* URL, wchar_t* OutputConsoleCommand, int CommandBufferSize);

	/**
	 * Retrieves a target with the specified handle.
	 *
	 * @param	Handle	The handle of the target to retrieve.
	 */
	FAndroidTarget* GetTarget(TARGETHANDLE Handle);

	/** Returns a string with the relative path to the binaries folder prepended */
	wstring GetRelativePathString( wchar_t* InWideString );

	int GetIniInt(const wchar_t* Key, int Default);

	void GetIniString(const wchar_t* Key, const wchar_t* Default, wchar_t* Value, int ValueSize);

	bool GetIniBool(const wchar_t* Key, bool Default=false);

	void SetIniInt(const wchar_t* Key, int Value);

	void SetIniString(const wchar_t* Key, const wchar_t* Value);

	void SetIniBool(const wchar_t* Key, bool Value);

	const wchar_t* GetTargetComputerName(TARGETHANDLE Handle);

	const wchar_t* GetTargetGameName(TARGETHANDLE /*Handle*/);

	const wchar_t* GetTargetGameType(TARGETHANDLE Handle);

	void CopyFileWhenNewer (FConsoleFileManifest& Manifest, const wchar_t* SourceFilename, const wchar_t* DestFilename, bool bDisplayMessageBox, ColoredTTYEventCallbackPtr OutputCallback);

	/**
	 * Asynchronously copies a set of files to a set of targets.
	 *
	 * @param	Handles						An array of targets to receive the files.
	 * @param	HandlesSize					The size of the array pointed to by Handles.
	 * @param	SrcFilesToSync				An array of source files to copy. This must be the same size as DestFilesToSync.
	 * @param	DestFilesToSync				An array of destination files to copy to. This must be the same size as SrcFilesToSync.
	 * @param	FilesToSyncSize				The size of the SrcFilesToSync and DestFilesToSync arrays.
	 * @param	DirectoriesToCreate			An array of directories to be created on the targets.
	 * @param	DirectoriesToCreateSize		The size of the DirectoriesToCreate array.
	 * @param	OutputCallback				TTY callback for receiving colored output.
	 */
	virtual bool SyncFiles(TARGETHANDLE * /*Handles*/, int /*HandlesSize*/, const wchar_t **SrcFilesToSync, const wchar_t **DestFilesToSync, int FilesToSyncSize, const wchar_t ** /*DirectoriesToCreate*/, int /*DirectoriesToCreateSize*/, ColoredTTYEventCallbackPtr OutputCallback);

	/**
	 * Return the number of console-specific menu items this platform wants to add to the main
	 * menu in UnrealEd.
	 *
	 * @return	The number of menu items for this console
	 */
	int GetNumMenuItems();

	/**
	 * Return the string label for the requested menu item
	 * @param	Index		The requested menu item
	 * @param	bIsChecked	Is this menu item checked (or selected if radio menu item)
	 * @param	bIsRadio	Is this menu item part of a radio group?
	 * @param	OutHandle	Receives the handle of the target associated with the menu item.
	 *
	 * @return	Label for the requested menu item
	 */
	const wchar_t* GetMenuItem(int Index, bool& bIsChecked, bool& bIsRadio, TARGETHANDLE& OutHandle);

	/**
	 * Gets a list of targets that have been selected via menu's in UnrealEd.
	 *
	 * @param	OutTargetList			The list to be filled with target handles.
	 * @param	InOutTargetListSize		Contains the size of OutTargetList. When the function returns it contains the # of target handles copied over.
	 */
	void GetMenuSelectedTargets(TARGETHANDLE* OutTargetList, int &InOutTargetListSize);

	/**
	 * Turn an address into a symbol for callstack dumping
	 * 
	 * @param Address Code address to be looked up
	 * @param OutString Function name/symbol of the given address
	 * @param OutLength Size of the OutString buffer
	 */
	virtual void ResolveAddressToString(unsigned int /*Address*/, wchar_t* OutString, int OutLength);
};

#endif
