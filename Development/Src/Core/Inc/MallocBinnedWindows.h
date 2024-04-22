/*=============================================================================
	MallocBinnedWindows.h: Windows-specifics for the binner allocator
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MALLOCBINNEDWINDOWS_H__
#define __MALLOCBINNEDWINDOWS_H__

#include <psapi.h>
#pragma comment(lib, "psapi.lib")

/**
 * @return the page size the OS uses
 */
FORCEINLINE UINT BinnedGetPageSize()
{
	// Get OS page size.
	SYSTEM_INFO SI;
	GetSystemInfo( &SI );
	return SI.dwPageSize;
}

/**
 * Allocate from the OS, must be at least 64k aligned
 *
 * @param Size Size to allocate, not necessarily aligned
 * @param bIsPhysical If TRUE, this is allocating via GMalloc->PhysicalAlloc
 *
 * @return OS allocated pointer for use by binned allocator
 */
FORCEINLINE void* BinnedAllocFromOS(UINT Size, UBOOL bIsPhysical=FALSE)
{
	return VirtualAlloc(NULL, Size, MEM_COMMIT, PAGE_READWRITE);
}

/**
 * Return a pointer allocated by BinnedAllocFromOS to the OS
 *
 * @param A pointer previously returned from BinnedAllocFromOS
 * @param bIsPhysical If TRUE, this is allocating via GMalloc->PhysicalFree
 */
FORCEINLINE void BinnedFreeToOS(void* Ptr, UBOOL bIsPhysical=FALSE)
{
	verify(VirtualFree(Ptr, 0, MEM_RELEASE) != 0);
}

/**
 * @return the amount of memory allocated from the OS
 */
FORCEINLINE UINT BinnedGetTotalMemoryAllocatedFromOS()
{
	// Just get memory information for the process and report the working set instead
	PROCESS_MEMORY_COUNTERS Counters;
	GetProcessMemoryInfo(GetCurrentProcess(), &Counters, sizeof(Counters));

	return Counters.WorkingSetSize;
}

FORCEINLINE UINT BinnedGetAvailableMemoryFromOS()
{
	PERFORMANCE_INFORMATION PerfInfo;
	GetPerformanceInfo(&PerfInfo, sizeof(PerfInfo));

	return PerfInfo.PhysicalAvailable;
}

#endif