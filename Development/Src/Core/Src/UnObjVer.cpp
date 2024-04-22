/*=============================================================================
	UnObjVer.cpp: Unreal version definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

// Used by the build system to set the Windows version number - it is defined as MAJOR.MINOR.ENGINE.PRIVATE
#define MAJOR_VERSION			1
#define MINOR_VERSION			0
#define PRIVATE_VERSION			131

// Defined separately so the build script can get to it easily (DO NOT CHANGE THIS MANUALLY)
#define	ENGINE_VERSION	10897

#define	BUILT_FROM_CHANGELIST	1532151


INT	GEngineVersion				= ENGINE_VERSION;
INT	GBuiltFromChangeList		= BUILT_FROM_CHANGELIST;

#if _XBOX
	// Prevent patched and unpatched network clients from seeing each other in system link
	// NOTE: This is not a Live problem, because you must patch to play on Live
	INT	GEngineMinNetVersion		= ENGINE_VERSION;
#else
	INT	GEngineMinNetVersion		= 9188;
#endif
INT	GEngineNegotiationVersion	= 3077;

// @see UnObjVer.h for the list of changes/defines
INT	GPackageFileVersion			= VER_LATEST_ENGINE;
INT	GPackageFileMinVersion		= 491;
INT	GPackageFileLicenseeVersion = VER_LATEST_ENGINE_LICENSEE;
INT GPackageFileCookedContentVersion = VER_LATEST_COOKED_PACKAGE | (VER_LATEST_COOKED_PACKAGE_LICENSEE << 16);

