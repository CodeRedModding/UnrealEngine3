/*=============================================================================
	LaunchEngineLoop.h: Unreal launcher.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __HEADER_LAUNCHENGINELOOP_H
#define __HEADER_LAUNCHENGINELOOP_H

/** Helper structure for tracking incremental loading of startup packages */
struct FLoadStartupInfo
{
public:
	/** The time the startup load was started */
	DOUBLE StartTime;
	/** should script packages load from memory? */
	UBOOL bSerializeStartupPackagesFromMemory;
	/** List  of native script package names. */
	TArray<FString> NativeScriptPackages;
	/** Flag to indicate NativeScriptPackages loading has completed */
	UBOOL bLoadedNativeScriptPackages;
	/** List of non-native startup packages. */
	TArray<FString> NonNativeStartupPackages;
	/** Flag to indicate NativeScriptPackages loading has completed */
	UBOOL bLoadedNonNativeScriptPackages;
	/** The last package loaded */
	INT LastPackageLoaded;

	/**
	 *	Constructor.
	 *
	 *	@param	bInDoIncrementalLoad		Whether to do incremental loading of startup packages
	 */
	FLoadStartupInfo(UBOOL bInDoIncrementalLoad = FALSE);
};

/**
 * This class implementes the main engine loop.	
 */
class FEngineLoop
{
protected:
	/**	Flag indicating initialization has completed */
	UBOOL			bInitialized;
	/** Dynamically expanding array of frame times in milliseconds (if GIsBenchmarking is set)*/
	TArray<FLOAT>	FrameTimes;
	/** Total of time spent ticking engine.  */
	DOUBLE			TotalTickTime;
	/** Maximum number of seconds engine should be ticked. */
	DOUBLE			MaxTickTime;
	QWORD			MaxFrameCounter;
	DWORD			LastFrameCycles;

	/** The objects which need to be cleaned up when the rendering thread finishes the previous frame. */
	FPendingCleanupObjects* PendingCleanupObjects;

	/** 
	 *	Helper structure for ticked initialization
	 *	This is used by platforms that are single-thread and cannot block for extended periods.
	 */
	struct FTickedInitInfo
	{
	public:
		/** The load startup packages info */
		FLoadStartupInfo LoadStartupInfo;
		/** TRUE if we have completed the load of all startup packages. */
		UBOOL bLoadedStartupPackages;

		/** The time the init was started */
		DOUBLE StartTime;

		FTickedInitInfo();
	};

	FTickedInitInfo* TickedInitInfo;

public:
	/**
	* Pre-Initialize the main loop - parse command line, sets up GIsEditor, etc.
	*
	* @param	CmdLine	command line
	* @return	Returns the error level, 0 if successful and > 0 if there were errors
	*/ 
	INT PreInit( const TCHAR* CmdLine );

	/**
	 *	Initialization that occurs during the Tick.
	 *	On platforms that require it, this function should minimize the time
	 *	spent during init. It will be called in the Tick function until it
	 *	returns TRUE, at which point, normally ticking will be processed.
	 *
	 *	@return	UBOOL		TRUE when initialization is complete.
	 */
	UBOOL TickedInit();

	/**
	 * Initialize the main loop - the rest of the initialization.
	 *
	 * @return	Returns the error level, 0 if successful and > 0 if there were errors
	 */ 
	INT Init( );
	/** 
	 * Performs shut down 
	 */
	void Exit();
	/**
	 * Advances main loop 
	 */
	void Tick();
};

/**
 * Global engine loop object. This is needed so wxWindows can access it.
 */
extern FEngineLoop GEngineLoop;

#endif

