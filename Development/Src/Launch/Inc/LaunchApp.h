/*=============================================================================
	LaunchApp.h: Unreal wxApp
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel
=============================================================================*/

#ifndef __HEADER_LAUNCHAPP_H
#define __HEADER_LAUNCHAPP_H

/**
 * Base wxApp implemenation used for the game and UnrealEdApp to inherit from.	
 */
class WxLaunchApp : public wxApp
{
public:
	/**
	 * Gets called on initialization from within wxEntry.	
	 */
	virtual bool OnInit();
	/** 
	 * Gets called after leaving main loop before wxWindows is shut down.
	 */
	virtual int OnExit();
	/**
	 * Mix of wxWindows and our main loop so we can share messages.
	 */
	virtual int MainLoop();
};

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
