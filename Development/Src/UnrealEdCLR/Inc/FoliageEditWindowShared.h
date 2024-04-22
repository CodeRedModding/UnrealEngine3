/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FoliageEditWindowShared_h__
#define __FoliageEditWindowShared_h__

#ifdef _MSC_VER
	#pragma once
#endif


// NOTE: This file is included as MANAGED for CLR .cpp files, but UNMANAGED for the rest of the app

#include "InteropShared.h"



#ifdef __cplusplus_cli


// ... MANAGED ONLY definitions go here ...
ref class MFoliageEditWindow;


#else // #ifdef __cplusplus_cli


// ... NATIVE ONLY definitions go here ...


#endif // #else


/**
 * Foliage Edit window class (shared between native and managed code)
 */
class FFoliageEditWindow
	: public FCallbackEventDevice
{

public:

	/** Static: Allocate and initialize mesh paint window */
	static FFoliageEditWindow* CreateFoliageEditWindow( class FEdModeFoliage* InFoliageEditSystem, const HWND InParentWindowHandle );


public:

	/** Constructor */
	FFoliageEditWindow();

	/** Destructor */
	virtual ~FFoliageEditWindow();

	/** Initialize the Foliage edit window */
	UBOOL InitFoliageEditWindow( FEdModeFoliage* InFoliageEditSystem, const HWND InParentWindowHandle );

	/** Refresh all properties */
	void RefreshAllProperties();

	/** Saves window settings to the Foliage edit mode's UISettings structure */
	void SaveWindowSettings();

	/** Returns true if the mouse cursor is over the mesh paint window */
	UBOOL IsMouseOverWindow();

	/** FCallbackEventDevice: Called when a parameterless global event we've registered for is fired */
	void Send( ECallbackEventType Event );

	/** Called from edit mode after painting to ensure calculated properties are up to date. */
	void RefreshMeshListProperties();
	
protected:

	/** Managed reference to the actual window control */
	AutoGCRoot( MFoliageEditWindow^ ) WindowControl;
};


#endif	// __FoliageEditWindowShared_h__