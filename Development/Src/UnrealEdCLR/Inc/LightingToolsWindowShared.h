/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LightingToolsWindowShared_h__
#define __LightingToolsWindowShared_h__

#ifdef _MSC_VER
	#pragma once
#endif


// NOTE: This file is included as MANAGED for CLR .cpp files, but UNMANAGED for the rest of the app

#include "InteropShared.h"



#ifdef __cplusplus_cli


// ... MANAGED ONLY definitions go here ...

ref class MLightingToolsWindow;


#else // #ifdef __cplusplus_cli


// ... NATIVE ONLY definitions go here ...


#endif // #else




/**
 * Light map res window class (shared between native and managed code)
 */
class FLightingToolsWindow
	: public FCallbackEventDevice
{

public:

	/** Static: Allocate and initialize window */
	static FLightingToolsWindow* CreateLightingToolsWindow( const HWND InParentWindowHandle );


public:

	/** Constructor */
	FLightingToolsWindow();

	/** Destructor */
	virtual ~FLightingToolsWindow();

	/** Initialize the window */
	UBOOL InitLightingToolsWindow( const HWND InParentWindowHandle );

	/** 
	 *	Show the window
	 *
	 *	@param	bShow		If TRUE, show the window
	 *						If FALSE, hide it
	 */
	void ShowWindow(UBOOL bShow);

	/** Refresh all properties */
	void RefreshAllProperties();

	/** Saves window settings to the settings structure */
	void SaveWindowSettings();

	/** Returns true if the mouse cursor is over the window */
	UBOOL IsMouseOverWindow();

	/** FCallbackEventDevice: Called when a parameterless global event we've registered for is fired */
	void Send( ECallbackEventType Event );

protected:

	/** Managed reference to the actual window control */
	AutoGCRoot( MLightingToolsWindow^ ) WindowControl;

};



#endif	// __LightingToolsWindowShared_h__
