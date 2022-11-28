/*=============================================================================
	LaunchEngineLoop.h: Unreal launcher.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel
=============================================================================*/

#ifndef __HEADER_LAUNCHENGINELOOP_H
#define __HEADER_LAUNCHENGINELOOP_H

/**
 * This class implementes the main engine loop.	
 */
class FEngineLoop
{
protected:
	/** Dynamically expanding array of frame times in milliseconds (if GIsBenchmarking is set)*/
	TArray<FLOAT>	FrameTimes;

	//@documentation 
	DOUBLE			OldRealTime;
	DOUBLE			OldTime;

	INT				FrameCounter;
	INT				MaxFrameCounter;
	DWORD			LastFrameCycles;

public:
	/**
	 * Initialize the main loop.
	 *
	 * @param	CmdLine	command line
	 * @return	Returns the error level, 0 if successful and > 0 if there were errors
	 */ 
	INT Init( const TCHAR* CmdLine );
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

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
