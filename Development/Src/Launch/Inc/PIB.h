/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __HEADER_PIB_H
#define __HEADER_PIB_H

/**
 * Can't use Unreal friendly types in this file as it is included in places other than the engine
 */

class FPIB
{
public:
	virtual ~FPIB( void ) 
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
	virtual bool			Init( HINSTANCE Module, HWND Hwnd, const wchar_t* CommandLine ) = 0;

	/**
	 * Shuts the application down
	 *
	 * Calls PIBShutdown
	 */
	virtual void			Shutdown( void ) = 0;

	/**
	 * Ticks the engine loop
	 */
	virtual void __stdcall	Tick( void ) = 0;
};

typedef class FPIB* ( __cdecl *TPIBGetInterface )( DWORD );

/**
 * The single exported function to the outside world via the def file
 * 
 * Returns a pointer to an FPIB implementation
 */
FPIB* __cdecl PIBGetInterface( DWORD Version );

#if _WINDLL
/** 
 * The parent window supplied by the browser (or other external program)
 */
extern HWND GPIBParentWindow;

/**
 * The child window created by UE3 to handle all input and rendering
 */
extern HWND GPIBChildWindow;

INT PIBInit( HINSTANCE hInInstance, const TCHAR* CommandLine );
void EngineTick( void );
void PIBShutdown( void );
#endif // _WINDLL

#endif