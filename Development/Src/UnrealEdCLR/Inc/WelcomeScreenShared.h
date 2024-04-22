/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#ifndef __WELCOME_SCREEN_H__
#define __WELCOME_SCREEN_H__

#include "InteropShared.h"

#ifdef __cplusplus_cli

// Forward declarations
ref class MWPFFrame;
ref class MWelcomeScreenPanel;

#endif // #ifdef __cplusplus_cli

/** Non-managed singleton wrapper around the welcome screen */
class FWelcomeScreen : public FCallbackEventDevice
{
public:
	/**
	 * Returns whether or not the welcome screen should be displayed at startup (based on user preference)
	 *
	 * @return	TRUE if the welcome screen should be displayed at startup; FALSE otherwise
	 */
	static UBOOL ShouldDisplayWelcomeScreenAtStartup();

	/** Display the welcome screen */
	static void DisplayWelcomeScreen();

	/** Shut down the welcome screen singleton */
	static void Shutdown();

	/** Close the Welcome Screen window */
	static void CloseWindow();

protected:
	/** Override from FCallbackEventDevice to handle events */
	virtual void Send( ECallbackEventType Event );

private:
	/** Constructor */
	FWelcomeScreen();

	/** Destructor */
	~FWelcomeScreen();

	// Copy constructor and assignment operator intentionally left unimplemented
	FWelcomeScreen( const FWelcomeScreen& );
	FWelcomeScreen& operator=( const FWelcomeScreen& );

	/**
	 * Return internal singleton instance of the class
	 *
	 * @return	Reference to the internal singleton instance of the class
	 */
	static FWelcomeScreen& GetInternalInstance();

	/** Frame used for the welcome screen */
	GCRoot( MWPFFrame^ ) WelcomeScreenFrame;

	/** Panel used for the welcome screen */
	GCRoot( MWelcomeScreenPanel^ ) WelcomeScreenPanel;

	/** Singleton instance of the class */
	static FWelcomeScreen* Instance;
};

#endif // #ifndef __WELCOME_SCREEN_H__
