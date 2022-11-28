/*=============================================================================
	DemoGame.cpp
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Includes.
#include "DemoGame.h"

/*-----------------------------------------------------------------------------
	The following must be done once per package.
-----------------------------------------------------------------------------*/

#define STATIC_LINKING_MOJO 1

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName DEMOGAME_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "DemoGameClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "DemoGameClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVES_ONLY
#undef NAMES_ONLY

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
