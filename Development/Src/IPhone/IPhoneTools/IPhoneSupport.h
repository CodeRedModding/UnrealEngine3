/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

#ifndef _IPHONESUPPORT_H_
#define _IPHONESUPPORT_H_

#include "IPhoneNetworkManager.h"

class FIPhoneSupport : public FConsoleSupport
{
public:
	FIPhoneSupport(void* Module);

	virtual ~FIPhoneSupport()
	{
	}

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
		return CONSOLESUPPORT_NAME_IPHONE;
	}

	/**
	 * Returns the platform of the specified target.
	 */
	virtual FConsoleSupport::EPlatformType GetPlatformType()
	{
		return EPlatformType_IPhone;
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
	FIPhoneNetworkManager NetworkManager;

	/** Cache the gamename coming from the editor */
	wstring GameName;

	/** Cache the configuration (debug/release) of the editor */
	wstring Configuration;

	/** Relative path from current working directory to the binaries directory */
	wstring RelativeBinariesDir;

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

	const wchar_t* GetTargetGameName(TARGETHANDLE Handle);

	const wchar_t* GetTargetGameType(TARGETHANDLE Handle);

	/**
	 * This function is run on a separate thread for cooking, copying, and running an autosaved level.
	 *
	 * @param	Data	A pointer to data passed into the thread providing it with extra data needed to do its job.
	 */
	static void __cdecl RunGameThread(void* Data);

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
	const wchar_t* GetMenuItem(int /*Index*/, bool& /*bIsChecked*/, bool& /*bIsRadio*/, TARGETHANDLE& /*OutHandle*/);

	/**
	 * Gets a list of targets that have been selected via menus in UnrealEd.
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

	/**
	 * Forces a stub target to be created to await connection
	 *
	 * @Param TargetAddress IP of target to add
	 *
	 * @returns Handle of new stub target
	 */
	virtual TARGETHANDLE ForceAddTarget( const wchar_t* TargetAddress );

};

#endif
