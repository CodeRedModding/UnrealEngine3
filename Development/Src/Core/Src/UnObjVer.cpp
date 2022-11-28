/*=============================================================================
	UnObjVer.cpp: Unreal version definitions.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "CorePrivate.h"

// Defined separately so the build script can get to it easily.
#define ENGINE_VERSION	469

extern INT			GEngineVersion				= ENGINE_VERSION,
					GEngineMinINIVersion		= 430,
					GEngineMinNetVersion		= 73,
					GEngineNegotiationVersion	= 73,
					GPackageFileVersion			= 186,
					GPackageFileMinVersion		= 178,
					GPackageFileLicenseeVersion = 0;

//
//	Package file version log:
//
//	178:
//	-	Deprecated FCompactIndex
//	179:
//	-	Change InterpData to being a subclass of SequenceVariable attached to SeqAct_Interps
//	180:
//	-	Removed UPrimitive
//	181:
//	-	Store precomputed streaming data in ULevel
//	182: 
//	-	Removed UStatickMesh::StreamingTextureFactor
//	183:
//	-	Changed static mesh lightmaps to be a UTexture2D and create mipmaps based on coverage
//	184:
//	-	Cleaned up the mess I created when I clobbered Scott's change in the first "183"
//	185:
//  -   Added "AutoExpandCategories" array to UClass
//	186:
//	-	Removed FDependency
