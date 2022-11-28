/*=============================================================================
	UnDebuggerInterface.h: Debugger Interface interface
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Lucas Alonso, Demiurge Studios
=============================================================================*/

#ifndef __UNDEBUGGERINTERFACE_H__
#define __UNDEBUGGERINTERFACE_H__

enum EUserAction
{
	UA_None,
	UA_StepInto, 
	UA_StepOver,
	UA_StepOverStack,
	UA_StepOut, 
	UA_RunToCursor,
	UA_Go,
	UA_Exit,
	UA_ReloadPackages,
	UA_RefreshClassTree,
	UA_MAX,
};

class UDebuggerInterface
{
public:
	/**
	 * Called when the debugger is loaded
	 */
	virtual UBOOL Initialize( class UDebuggerCore* DebuggerOwner )=0;


	/**
	 * Should close the debugger window
	 */
	virtual void Close()=0;


	/**
	 * Bring the debugger window to the foreground
	 */
	virtual void Show()=0;


	/**
	 * Force the debugger window to the background
	 */
	virtual void Hide()=0;


	/**
	 * Add a line to the log window
	 */
	virtual void AddToLog( const TCHAR* Line )=0;


	/**
	 * Send the debugger to the specified class and line number
	 */
	virtual void Update( const TCHAR* ClassName, const TCHAR* PackageName, INT LineNumber, const TCHAR* OpcodeName, const TCHAR* objectName )=0;


	/**
	 * Change the contents of the call stack window
	 */
	virtual void UpdateCallStack( TArray<FString>& StackNames )=0;


	/**
	 * Set a breakpoint at the specified location
	 */
	virtual void SetBreakpoint( const TCHAR* ClassName, INT Line )=0;


	/**
	 * Remove the specified breakpoint
	 */
	virtual void RemoveBreakpoint( const TCHAR* ClassName, INT Line )=0;


	/**
	 * Force the class tree to refresh. Should probably be called only once at the start of the program
	 */
	virtual void UpdateClassTree()=0;


	/**
	 * Prepare a watch for editing
	 */
	virtual void LockWatch(int watch) = 0;


	/**
	 * Complete watch window edits
	 */
	virtual void UnlockWatch(int watch) = 0;


	/**
	 * Add a line to a watch window (Put between LockWatch/UnlockWatch)
	 */
	virtual int AddAWatch(int watch, int ParentIndex, const TCHAR* ObjectName, const TCHAR* Data) = 0;


	/**
	 * Clears the contents of a watch window
	 */
	virtual void ClearAWatch(int watch) = 0;

	// Whether the interface dll is currently loaded
	virtual UBOOL IsLoaded() const = 0;


	/**
	 * The three watch windows. Used as parameters to LockWatch, UnlockWatch, ClearWatch and AddAWatch
	 */
	const int GLOBAL_WATCH;
	const int LOCAL_WATCH;
	const int WATCH_WATCH;

	UDebuggerInterface() : GLOBAL_WATCH(1), LOCAL_WATCH(0), WATCH_WATCH(2) {}

protected:
	class UDebuggerCore* Debugger;
	INT   LoadCount;
};

#endif // __UNDEBUGGERINTERFACE_H__