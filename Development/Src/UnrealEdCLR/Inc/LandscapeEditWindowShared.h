/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LandscapeEditWindowShared_h__
#define __LandscapeEditWindowShared_h__

#ifdef _MSC_VER
	#pragma once
#endif


// NOTE: This file is included as MANAGED for CLR .cpp files, but UNMANAGED for the rest of the app

#include "InteropShared.h"



#ifdef __cplusplus_cli


// ... MANAGED ONLY definitions go here ...
ref class MLandscapeEditWindow;


#else // #ifdef __cplusplus_cli


// ... NATIVE ONLY definitions go here ...


#endif // #else


/**
 * Landscape Edit window class (shared between native and managed code)
 */
class FLandscapeEditWindow
	: public FCallbackEventDevice
{

public:

	/** Static: Allocate and initialize mesh paint window */
	static FLandscapeEditWindow* CreateLandscapeEditWindow( class FEdModeLandscape* InLandscapeEditSystem, const HWND InParentWindowHandle );


public:

	/** Constructor */
	FLandscapeEditWindow();

	/** Destructor */
	virtual ~FLandscapeEditWindow();

	/** Initialize the landscape edit window */
	UBOOL InitLandscapeEditWindow( FEdModeLandscape* InLandscapeEditSystem, const HWND InParentWindowHandle );

	/** Refresh all properties */
	void RefreshAllProperties();

	/** Saves window settings to the landscape edit mode's UISettings structure */
	void SaveWindowSettings();

	/** Returns true if the mouse cursor is over the mesh paint window */
	UBOOL IsMouseOverWindow();

	/** FCallbackEventDevice: Called when a parameterless global event we've registered for is fired */
	void Send( ECallbackEventType Event );

	/* User changed the current tool - update the button state */
	void NotifyCurrentToolChanged( const FString& ToolSetName );

	void NotifyMaskEnableChanged( UBOOL bEnabled );

	void NotifyBrushSizeChanged( FLOAT Radius );
	void NotifyBrushComponentSizeChanged( INT Size );

protected:

	/** Managed reference to the actual window control */
	AutoGCRoot( MLandscapeEditWindow^ ) WindowControl;
};


#endif	// __LandscapeEditWindowShared_h__