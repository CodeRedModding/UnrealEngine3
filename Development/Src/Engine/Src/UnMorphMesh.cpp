/*=============================================================================
	UnMorphMesh.cpp: Unreal morph target mesh and blending implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.	
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"

/**
* serialize members
*/
void UMorphTarget::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << MorphLODModels;
}

/**
 * Used by various commandlets to purge editor only and platform-specific data from various objects
 * 
 * @param PlatformsToKeep Platforms for which to keep platform-specific data
 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
 */
void UMorphTarget::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData); 

#if WITH_EDITORONLY_DATA
	//Morph target data not necessary for dedicated server, so
	// toss the data if we are only keeping WindowsServer
	if (!(PlatformsToKeep & ~UE3::PLATFORM_WindowsServer))
	{
		MorphLODModels.Empty();
	}
#endif // WITH_EDITORONLY_DATA
}

/**
* called when object is loaded
*/
void UMorphTarget::PostLoad()
{
	Super::PostLoad();
}

INT UMorphTarget::GetResourceSize()
{
	INT Retval = 0;

	if (!GExclusiveResourceSizeMode)
	{
		FArchiveCountMem CountBytesSize( this );
		Retval = CountBytesSize.GetNum();
	}

	return Retval;
}