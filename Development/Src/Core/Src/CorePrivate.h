/*=============================================================================
	CorePrivate.h: Unreal core private header file.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*----------------------------------------------------------------------------
	Core public includes.
----------------------------------------------------------------------------*/

#include "Core.h"

/*-----------------------------------------------------------------------------
	Locals functions.
-----------------------------------------------------------------------------*/

extern void appPlatformPreInit();
extern void appPlatformInit();

extern UBOOL GExitPurge;

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#include "UnLinker.h"

/*-----------------------------------------------------------------------------
	UTextBufferFactory.
-----------------------------------------------------------------------------*/

//
// Imports UTextBuffer objects.
//
class UTextBufferFactory : public UFactory
{
	DECLARE_CLASS(UTextBufferFactory,UFactory,0,Core)

	// Constructors.
	UTextBufferFactory();
	void StaticConstructor();

	// UFactory interface.
	UObject* FactoryCreateText( ULevel* InLevel, UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn );
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

