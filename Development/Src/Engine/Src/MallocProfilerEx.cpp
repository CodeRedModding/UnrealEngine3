/*=============================================================================
	MallocProfilerEx.cpp: Extended memory profiling support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#include "FMallocProfiler.h"
#include "MallocProfilerEx.h"

#if USE_MALLOC_PROFILER

// These functions are here because FMallocProfiler is in the Core
// project, and therefore can't access most of the classes needed by these functions.

/**
 * Constructor, initializing all member variables and potentially loading symbols.
 *
 * @param	InMalloc	The allocator wrapped by FMallocProfiler that will actually do the allocs/deallocs.
 */
FMallocProfilerEx::FMallocProfilerEx( FMalloc* InMalloc )
	: FMallocProfiler( InMalloc )
{}

/** 
 * Writes names of currently loaded levels. 
 * Only to be called from within the mutex / scope lock of the FMallocProfiler.
 */
void FMallocProfilerEx::WriteLoadedLevels()
{
	WORD NumLoadedLevels = 0;
	INT NumLoadedLevelsPosition = BufferedFileWriter.Tell();
	BufferedFileWriter << NumLoadedLevels;

	if (GWorld)
	{
		// Write the name of the map.
		const FString MapName = GWorld->CurrentLevel->GetOutermost()->GetName();
		INT MapNameIndex = GetNameTableIndex( MapName );
		NumLoadedLevels ++;

		BufferedFileWriter << MapNameIndex;

		// Write out all of the fully loaded levels.
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		for (INT LevelIndex = 0; LevelIndex < WorldInfo->StreamingLevels.Num(); LevelIndex++)
		{
			ULevelStreaming* LevelStreaming = WorldInfo->StreamingLevels(LevelIndex);
			if ((LevelStreaming != NULL)
				&& (LevelStreaming->PackageName != NAME_None)
				&& (LevelStreaming->PackageName != GWorld->GetOutermost()->GetFName())
				&& (LevelStreaming->LoadedLevel != NULL))
			{
				NumLoadedLevels++;

				INT LevelPackageIndex = GetNameTableIndex(LevelStreaming->PackageName);

				BufferedFileWriter << LevelPackageIndex;
			}
		}

		// Patch up the count.
		if (NumLoadedLevels > 0)
		{
			INT EndPosition = BufferedFileWriter.Tell();
			BufferedFileWriter.Seek(NumLoadedLevelsPosition);
			BufferedFileWriter << NumLoadedLevels;
			BufferedFileWriter.Seek(EndPosition);
		}
	}
}

/** 
 * Gather texture memory stats. 
 */
void FMallocProfilerEx::GetTexturePoolSize( FMemoryAllocationStats& MemoryStats )
{
	INT AllocatedTextureMemorySize = 0;
	INT AvailableTextureMemorySize = 0;
	INT PendingDeletionSize = 0;

	if( GIsRHIInitialized )
	{
		RHIGetTextureMemoryStats( AllocatedTextureMemorySize, AvailableTextureMemorySize, PendingDeletionSize );
	}

	MemoryStats.AllocatedTextureMemorySize = AllocatedTextureMemorySize;
	MemoryStats.AvailableTextureMemorySize = AvailableTextureMemorySize;
}

#endif // USE_MALLOC_PROFILER