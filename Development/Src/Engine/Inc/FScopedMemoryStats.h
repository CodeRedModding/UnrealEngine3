/*=============================================================================
	ScopedMemoryStats.h scoped memory stats tracking.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/
#ifndef _FSCOPEDMEMORYSTATS_H_
#define _FSCOPEDMEMORYSTATS_H_

#include "Core.h"
#include "Array.h"
#include "UnString.h"

#if USE_SCOPED_MEM_STATS && STATS
class FScopedMemoryStats
{

public:
	/**
	*Constructs the class and marks memory state
	*@param outputDevice - The output device to log deltas to upon destruction
	*@param label - The label to use when logging deltas
	*/
	FScopedMemoryStats (FOutputDevice& outputDevice, const FString& label);
	~FScopedMemoryStats ();

	/**The starting state of the STATGROUP_Memory stats used for finding deltas*/
	TArray<INT>				StartingMemStats;

	//**The starting state of memory stats used for finding deltas*/
	FMemoryAllocationStats	StartingAllocationStats;

	//**The output device to log deltas to*/
	FOutputDevice&			OutputDevice;

	//**The label used when logging deltas to the OutputDevice*/
	FString					Label;

	//**Used to prevent the logging logic from running when correct flag is not specified*/
	static UBOOL IsScopedMemStatsActive;
}; 

#define SCOPED_MEM_STATS FScopedMemoryStats ScopedMemStats(*GLog, FString( __FUNCTION__ ));
#define SCOPED_MEM_STATS_LABEL(label) FScopedMemoryStats ScopedMemStats(*GLog, FString(TEXT(label)));

#else
#define SCOPED_MEM_STATS
#define SCOPED_MEM_STATS_LABEL(label)
#endif

#endif