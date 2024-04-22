/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#pragma once

namespace AndroidTools
{

/**
 * Class for loading Android symbols.
 */
public ref class FAndroidManagedSymbolParser
{
private:
	FCommandLineWrapper* CmdLineProcess;

	String^ ExecutablePath;

protected:
	/**
	 * Finalizer frees native memory.
	 */
	!FAndroidManagedSymbolParser();

public:
	/**
	 * Constructor.
	 */
	FAndroidManagedSymbolParser();

	/**
	 * Converted to virtual void Dispose().
	 */
	virtual ~FAndroidManagedSymbolParser();

	/**
	 * Loads symbols for an executable.
	 *
	 * @param	ExePath		The path to the executable whose symbols are going to be loaded.
	 * @param	SearchPath	Additional locations to search for the pdb in.
	 * @param	bEnhanced	Whether to do a more detailed (but slower) symbol lookup
	 * @param	Modules		The list of modules loaded by the process.
	 */
	bool LoadSymbols( String^ ExePath, String^ SearchPath, bool bEnhanced, Object^ UserData );

	/**
	 * Unloads any currently loaded symbols.
	 */
	void UnloadSymbols();

	/**
	 * Retrieves the symbol info for an address.
	 *
	 * @param	Address			The address to retrieve the symbol information for.
	 * @param	OutFileName		The file that the instruction at Address belongs to.
	 * @param	OutFunction		The name of the function that owns the instruction pointed to by Address.
	 * @param	OutLineNumber	The line number within OutFileName that is represented by Address.
	 * @return	True if the function succeeds.
	 */
	bool ResolveAddressToSymboInfo( QWORD Address, String^ %OutFileName, String^ %OutFunction, int% OutLineNumber );
};

}

using namespace AndroidTools;
