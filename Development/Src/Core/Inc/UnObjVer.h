/*=============================================================================
	UnObjVer.h: Unreal object version.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Prevents incorrect files from being loaded.

#define PACKAGE_FILE_TAG 0x9E2A83C1

// Version access.

extern INT			GEngineVersion,					// Engine build number, for displaying to end users.
					GEngineMinINIVersion,			// Earliest build that doesn't need to have ini's scrapped.
					GEngineMinNetVersion,			// Earliest engine build that is network compatible with this one.
					GEngineNegotiationVersion,		// Base protocol version to negotiate in network play.
					GPackageFileVersion,			// The current Unrealfile version.
					GPackageFileMinVersion,			// The earliest file version that can be loaded with complete backward compatibility.
					GPackageFileLicenseeVersion;	// Licensee Version Number.

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

