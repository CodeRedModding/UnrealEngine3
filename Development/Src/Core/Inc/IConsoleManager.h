/*=============================================================================
IConsoleManager.h: console command interface
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ICONSOLEMANAGER_H__
#define __ICONSOLEMANAGER_H__

/**
 * Console variable usage guide:
 *
 * The variable should be creates early in the initialization but not before (not in global variable construction).
 * Choose the right variable type, consider using a console command if more functionality is needed (see Exec()).
 * Available types: int, float, int&, float&, string
 * There is no bool type as the int type provides enough functionality (0=false, 1=true) and can be extended easily (e.g. 2=auto, -1=debug)
 * Always provide a good help text, other should be able to understand the function of the console variable by reading this help.
 * The help length should be limited to a reasonable width in order to work well for low res screen resolutions.
 *
 * Usage in the game console:
 *   <COMMAND> ?				print the HELP
 *   <COMMAND>	 				print the current state of the console variable
 *   <COMMAND> x 				set and print the new state of the console variable
 *
 * The ECVF_Changed flag can be used to detect if the state was changes through the Set() method (float& and int& can change outside of this).
 * 
 * All variables support auto completion. The single line help that can show up there is currently not connected to the help as the help text
 * is expected to be multi line.
 * The former Exec() system can be used to access the console variables.
 * Use console variables only in main thread.
 * The state of console variables is not network synchronized or serialized (load/save). The plan is to allow to set the state in external files (game/platform/engige/local).
 */

/**
 * Bitmask 0x1, 0x2, 0x4, ..
 */
enum EConsoleVariableFlags
{
	/**
	 * Default, no flags are set
	 */
	ECVF_Default = 0x0,
	/**
	 * Console variables marked with this flag behave differently in a final release build.
	 * Then they are are hidden in the console and cannot be changed by the user.
	 */
	ECVF_Cheat = 0x1,
	/**
	 * By default, after creation this flag is not set.
	 * Whenever the value was changes through the Set functions the flag is set.
	 * To detect changes:
	 * if(CVar->TestFlags(ECVF_Changed))
	 * {
	 *     CVar->ClearFlags(ECVF_Changed);
	 *     ... 
	 * }
	 * The old value is not stored as this can be easily done by hand and only when needed.
	 */
	ECVF_Changed = 0x2,
	/**
	 * Console variables cannot be changed by the user (from console or from ini file).
	 * Changing from C++ is still possible.
	 */
	ECVF_ReadOnly = 0x4,
	/**
	 * UnregisterConsoleVariable() was called on this one.
	 * If the variable is registered again with the same type this object is reactivated. This is good for DLL unloading.
	 */
	ECVF_Unregistered = 0x8,
	/**
	 * This flag is set by the ini loading code when the variable wasn't registered yet.
	 * Once the variable is registered later the value is copied over and the variable is destructed.
	 */
	ECVF_CreatedFromIni = 0x10,
	/**
	 * Prefer printout of the value in hex (only for Int)
	 */
	ECVF_HexValue = 0x20,
};

/**
 * Interface for console variables
 */
struct IConsoleVariable
{
	virtual ~IConsoleVariable()
	{

	}

	/**
	 * Set the internal value from the specified int.
	 */
	virtual void Set(INT InValue) = 0;
	/**
	 * Set the internal value from the specified float.
	 */
	virtual void Set(FLOAT InValue) = 0;
	/**
	 * Set the internal value from the specified string.
	 */
	virtual void Set(const TCHAR* InValue) = 0;
	/**
	 * Get the internal value as int (should not be used on strings).
	 * @return value is not rounded (simple cast)
	 */
	virtual INT GetInt() const = 0;
	/**
	 * Get the internal value as float (should not be used on strings).
	 */
	virtual FLOAT GetFloat() const = 0;
	/**
	 * Get the internal value as string (works on all types).
	 */
	virtual FString GetString() const = 0;
	/**
	 *  @return never 0, can be multi line ('\n')
	 */
	virtual const TCHAR* GetHelp() const = 0;
	/**
	 *  @return never 0, can be multi line ('\n')
	 */
	virtual void SetHelp(const TCHAR* Value) = 0;
	/**
	 * Get the internal state of the flags.
	 */
	virtual EConsoleVariableFlags GetFlags() const = 0;
	/**
	 * Sets the internal flag state to the specified value.
	 */
	virtual void SetFlags(const EConsoleVariableFlags Value) = 0;

	// Convenience methods -------------------------------------

	/**
	 * Removes the specified flags in the internal state.
	 */
	void ClearFlags(const EConsoleVariableFlags Value)
	{
		UINT New = (UINT)GetFlags() & (UINT)Value;
	
		SetFlags((EConsoleVariableFlags)New);
	}
	/**
	 * Test is any of the specified flags is set in the internal state.
	 */
	UBOOL TestFlags(const EConsoleVariableFlags Value) const
	{
		return ((UINT)GetFlags() & (UINT)Value) != 0;
	}


private: // -----------------------------------------


	/**
	 *  should only be called by the manager, needs to be implemented for each instance
	 */
	virtual void Release() = 0;

	friend class FConsoleManager;
};

/**
 * Allows to iterate through all console variables
 */
struct IConsoleVariableVisitor
{
	/**
	 *  @param Name must not be 0
	 *  @param CVar must not be 0
	 */
	virtual void OnConsoleVariable(const TCHAR *Name, IConsoleVariable* CVar) = 0;
};

/**
 * handles console commands and variables, registered console variables are released on destruction
 */
struct IConsoleManager
{
	virtual ~IConsoleManager()
	{

	}

	/**
	 * Create a int console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	virtual IConsoleVariable* RegisterConsoleVariable(const TCHAR* Name, INT DefaultValue, const TCHAR* Help, UINT Flags = ECVF_Default) = 0;
	/**
	 * Create a float console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	virtual IConsoleVariable* RegisterConsoleVariable(const TCHAR* Name, FLOAT DefaultValue, const TCHAR* Help, UINT Flags = ECVF_Default) = 0;
	/**
	 * Create a string console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	virtual IConsoleVariable* RegisterConsoleVariable(const TCHAR* Name, const TCHAR* DefaultValue, const TCHAR* Help, UINT Flags = ECVF_Default) = 0;
	/**
	 * Create a reference to a int console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	virtual IConsoleVariable* RegisterConsoleVariableRef(const TCHAR* Name, INT& RefValue, const TCHAR* Help, UINT Flags = ECVF_Default) = 0;
	/**
	 * Create a reference to a float console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	virtual IConsoleVariable* RegisterConsoleVariableRef(const TCHAR* Name, FLOAT& RefValue, const TCHAR* Help, UINT Flags = ECVF_Default) = 0;
	/**
	 * Deactivated the use of the console variable but not actually removes it as some pointers might still reference it.
	 * If the variable is registered again with the same type this object is reactivated. This is good for DLL unloading.
	 * @param CVar can be 0 then the function does nothing
	 */
	virtual void UnregisterConsoleVariable(IConsoleVariable* CVar) = 0;
	/**
	 * Find a console variable
	 * @param Name must not be 0
	 * @param bCaseSensitive TRUE:fast, FALSE:slow but convenient
	 * @return 0 if the variable wasn't found
	 */
	virtual IConsoleVariable* FindConsoleVariable(const TCHAR* Name, UBOOL bCaseSensitive = TRUE) const = 0;
	/**
	 *  Iterate in O(n), not case sensitive, does not guarantee that UnregisterConsoleVariable() will work in the loop
	 *  @param Visitor must not be 0
	 *  @param ThatStartsWith must not be 0 
	 */
	virtual void ForEachConsoleVariable(IConsoleVariableVisitor* Visitor, const TCHAR* ThatStartsWith = TEXT("")) const = 0;
	/**
	 * Process user input
	 *  e.g.
	 *  "MyCVar" to get the current value of the console variable
	 *  "MyCVar -5.2" to set the value to -5.2
	 *  "MyCVar ?" to get the help text
	 *  @param Input must not be 0
	 *  @return TRUE if the command was recognized
	 */
	virtual UBOOL ProcessUserConsoleInput(const TCHAR* Input, FOutputDevice& Ar) const = 0;
};

#endif // __ICONSOLEMANAGER_H__