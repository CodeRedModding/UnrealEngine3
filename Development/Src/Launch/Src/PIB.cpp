/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#if _WINDOWS && !MOBILE
#pragma pack( push, 8 )
#include <Windows.h>
#pragma pack( pop )

#include "PIB.h"
#endif // _WINDOWS && !MOBILE

#ifdef _WINDLL

/** 
 * The parent window supplied by the browser (or other external program)
 */
HWND GPIBParentWindow = NULL;

/**
 * The child window created by UE3 to handle all input and rendering
 */
HWND GPIBChildWindow = NULL;

/** 
 * An implementation of the PIB abstract class
 */
class FPIBImplementation : public FPIB
{
public:
	FPIBImplementation( void ) 
	{
	}

	virtual ~FPIBImplementation( void ) 
	{
	}

	/**
	 * The equivalent of WinMain when the app is called as a dll
	 *
	 * @Param Module		HMODULE of loaded dll
	 * @Param Hwnd			HWND of parent window that a child window will be created from
	 * @Param CommandLine	wchar_t* of the commandline of command line
	 *
	 * This converts the parameters to Unreal friendly types, and passes on to PIBInit
	 */
	virtual bool			Init( HINSTANCE Module, HWND Hwnd, const wchar_t* CommandLine );

	/**
	 * Shuts the application down
	 *
	 * Calls PIBShutdown
	 */
	virtual void			Shutdown( void );

	/**
	 * Ticks the engine loop
	 */
	virtual void __stdcall	Tick( void );
};

/**
 * The single exported function to the outside world via the def file
 * 
 * Returns a pointer to an FPIB implementation
 */
FPIB* PIBGetInterface( DWORD )
{
	FPIB* PIB = new FPIBImplementation();
	return( PIB );
}

/** 
 * Inits the loaded DLL version of UE3
 */
bool FPIBImplementation::Init( HINSTANCE Module, HWND Hwnd, const wchar_t* CommandLine )
{
	GPIBParentWindow = Hwnd;

	if( PIBInit( Module, ( const TCHAR* )CommandLine ) != 0 )
	{
		return( false );
	}

	return( true );
}

/**
 * Shuts the loaded DLL version of UE3 down
 */
void FPIBImplementation::Shutdown( void )
{
	PIBShutdown();
}

/**
 * Ticks the engine loop and potentially moves the client window if it has moved (IE only)
 */
void FPIBImplementation::Tick( void )
{
	EngineTick();
}

#else

#if _WINDOWS && !MOBILE
FPIB* PIBGetInterface( DWORD )
{
	return( NULL );
}
#endif // _WINDOWS && !MOBILE

#endif // _WINDLL

// end