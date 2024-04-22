/*================================================================================
	SplashScreen.h: Splash screen for game/editor startup
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/


#ifndef __SplashScreen_h__
#define __SplashScreen_h__

#ifdef _MSC_VER
	#pragma once
#endif

#if !CONSOLE

/**
 * SplashTextType defines the types of text on the splash screen
 */
namespace SplashTextType
{
	enum Type
	{
		/** Startup progress text */
		StartupProgress	= 0,

		/** Version information text line 1 */
		VersionInfo1,

		/** Copyright information text */
		CopyrightInfo,

		/** Third party copyright information text */
		ThirdPartyCopyrightInfo,

		// ...

		/** Number of text types (must be final enum value) */
		NumTextTypes
	};
}


/**
 * Displays a splash window with the specified image.  The function does not use wxWidgets.  The splash
 * screen variables are stored in static global variables (SplashScreen*).  If the image file doesn't exist,
 * the function does nothing.
 *
 * @param	InSplashName	Name of splash screen file (and relative path if needed)
 */
void appShowSplash( const TCHAR* InSplashName );


/**
 * Destroys the splash window that was previously shown by appShowSplash(). If the splash screen isn't active,
 * it will do nothing.
 */
void appHideSplash();


/**
 * Sets the text displayed on the splash screen (for startup/loading progress)
 *
 * @param	InType		Type of text to change
 * @param	InText		Text to display
 */
void appSetSplashText( const SplashTextType::Type InType, const TCHAR* InText );


#endif

#endif	// __SplashScreen_h__