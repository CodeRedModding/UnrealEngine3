/*=============================================================================
	CorePrivate.h: Unreal core private header file.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*----------------------------------------------------------------------------
	Core public includes.
----------------------------------------------------------------------------*/

#ifndef _INC_COREPRIVATE
#define _INC_COREPRIVATE

#include "Core.h"

/*-----------------------------------------------------------------------------
	Locals functions.
-----------------------------------------------------------------------------*/

extern void appPlatformPreInit();
extern void appPlatformInit();
extern void appPlatformPostInit();

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------
	UTextBufferFactory.
-----------------------------------------------------------------------------*/

//
// Imports UTextBuffer objects.
//
class UTextBufferFactory : public UFactory
{
	DECLARE_CLASS_INTRINSIC(UTextBufferFactory,UFactory,0,Core)

	// Constructors.
	UTextBufferFactory();

	// UFactory interface.
	UObject* FactoryCreateText( UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn );
};

#endif

