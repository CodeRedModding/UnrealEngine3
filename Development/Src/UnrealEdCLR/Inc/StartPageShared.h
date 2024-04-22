/*=============================================================================
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __StartPageCLR_h__
#define __StartPageCLR_h__

#ifdef _MSC_VER
#pragma once
#endif

// NOTE: This file is included as MANAGED for CLR .cpp files, but UNMANAGED for the rest of the app

#include "InteropShared.h"

#ifdef __cplusplus_cli

// ... MANAGED ONLY definitions go here ...

ref class MStartPageControl;

#else // #ifdef __cplusplus_cli

// ... NATIVE ONLY definitions go here ...

#endif // #else

/**
 * Start Page class (shared between native and managed code)
 */
class FStartPage
{

public:

	/** Static: Allocate and initialize start page*/
	static FStartPage* CreateStartPage( class WxStartPageHost* InParent, const HWND InParentWindowHandle );

	/** Static: Returns true if one start page has been allocated */
	static UBOOL IsInitialized( const INT InstanceIndex = 0 )
	{
		return ( StartPageInstances.Num() > InstanceIndex );
	}

	/** Static: Access instance of start page that the user focused last */
	static FStartPage& GetActiveInstance( )
	{
		// In case we do not find an appropriate instance of start page, return the default one.
		check( StartPageInstances.Num() > 0 );
		return *StartPageInstances( 0 );
	}

protected:
	/** Static: Access an global instance of the specified start page */
	static FStartPage& GetInstance( const INT InstanceIndex = 0 )
	{
		check( StartPageInstances.Num() > InstanceIndex );
		return *StartPageInstances( InstanceIndex );
	}


public:
	/** Destructor */
	virtual ~FStartPage();

	/** Resize the window */
	void Resize(HWND hWndParent, int x, int y, int Width, int Height);

	/**
	 * Propagates focus from the wxWidgets framework to the WPF framework.
	 */
	void SetFocus();

protected:
	/** Constructor */
	FStartPage();

	/**
	 * Initialize the start page
	 *
	 * @param	InParent				Parent window (or NULL if we're not parented.)
	 * @param	InParentWindowHandle	Parent window handle
	 *
	 * @return	TRUE if successful
	 */
	UBOOL InitStartPage( WxStartPageHost* InParent, const HWND InParentWindowHandle );


protected:
	/** Managed reference to the actual window control */
	GCRoot( MStartPageControl^ ) WindowControl;

	/** Static: List of currently-active Start Page instances */
	static TArray< FStartPage* > StartPageInstances;

};



#endif	// __StartPageCLR_h__

