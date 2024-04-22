/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#ifndef __ABOUT_SCREEN_H__
#define __ABOUT_SCREEN_H__

#include "InteropShared.h"

#ifdef __cplusplus_cli

// Forward declarations
ref class MWPFFrame;
ref class MAboutScreenPanel;

#endif // #ifdef __cplusplus_cli

/** Non-managed singleton wrapper around the about screen */
class FAboutScreen : public FCallbackEventDevice
{
public:
	/** Display the about screen */
	static void DisplayAboutScreen();

	/** Shut down the about screen singleton */
	static void Shutdown();

	/** Close the About Screen window */
	static void CloseWindow();

protected:
	/** Override from FCallbackEventDevice to handle events */
	virtual void Send( ECallbackEventType Event );

private:
	/** Constructor */
	FAboutScreen();

	/** Destructor */
	~FAboutScreen();

	// Copy constructor and assignment operator intentionally left unimplemented
	FAboutScreen( const FAboutScreen& );
	FAboutScreen& operator=( const FAboutScreen& );

	/**
	 * Return internal singleton instance of the class
	 *
	 * @return	Reference to the internal singleton instance of the class
	 */
	static FAboutScreen& GetInternalInstance();

	/** Frame used for the about screen */
	GCRoot( MWPFFrame^ ) AboutScreenFrame;

	/** Panel used for the about screen */
	GCRoot( MAboutScreenPanel^ ) AboutScreenPanel;

	/** Singleton instance of the class */
	static FAboutScreen* Instance;
};

#endif // #ifndef __ABOUT_SCREEN_H__
