/*=============================================================================
	LaunchApp.h: Unreal wxApp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
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
	virtual bool	OnInit();
	/** 
	 * Gets called after leaving main loop before wxWindows is shut down.
	 */
	virtual int		OnExit();

	/**
	 * Callback from wxWindows' main loop to signal that we should tick the engine.
	 */
	virtual void	TickUnreal();

	/**
	 * Performs any required cleanup in the case of a fatal error.
	 */
	virtual void	ShutdownAfterError();

protected:
};

#endif

