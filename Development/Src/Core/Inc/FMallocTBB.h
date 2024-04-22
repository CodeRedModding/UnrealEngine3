/*=============================================================================
	FMallocTBB.h: Intel-TBB 64-bit scalable memory allocator.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FMALLOCTBB_H__
#define __FMALLOCTBB_H__


#ifdef _WIN64


// always link TBB Release lib
#ifdef _DEBUG
	#define UNDEFD_DEBUG
	#undef _DEBUG
#endif

#include <tbb/scalable_allocator.h>

#ifdef UNDEFD_DEBUG
	#define _DEBUG
#endif


#pragma pack(push,8) 
#include <Psapi.h>
#pragma pack(pop) 
#pragma comment(lib, "psapi.lib")


#define MEM_TIME(st)


/**
 * Implements a memory allocator using the Intel Threading Building Blocks library.
 *
 * This allocator can only be used on 64-bit Windows. Note that scalable_malloc()
 * and scalable_realloc() may return more memory than requested.
 */
class FMallocTBB : public FMalloc
{
public:

	/**
	 * Default constructor.
	 */
	FMallocTBB () :
#if STATS
		CurrentAllocs(0),
		TotalAllocs(0),
		UsedCurrent(0),
		UsedPeak(0),
#endif
		MemTime(0.0)
	{
	}
	

public:

	/** 
	 * Inherited from FMalloc. Allocates a new block of memory.
	 *
	 * @param Size Desired size of the memory to allocate (in bytes).
	 * @param Alignment Desired memory alignment (in bytes).
	 */
	virtual void* Malloc (DWORD Size, DWORD Alignment)
	{
		check(Alignment == DEFAULT_ALIGNMENT && "Alignment is currently unsupported in Windows");

		MEM_TIME(MemTime -= appSeconds());
		void* NewPtr = scalable_malloc(Size);
		MEM_TIME(MemTime += appSeconds());

		if (NewPtr == NULL)
		{
			OutOfMemory();
		}

		STAT(UsedCurrent += scalable_msize(NewPtr));
		STAT(UsedPeak = Max(UsedPeak, UsedCurrent));
		STAT(CurrentAllocs++);
		STAT(TotalAllocs++);

		return NewPtr;
	}
	
	/** 
	 * Inherited from FMalloc. Reallocates previously allocated memory.
	 *
	 * @param Ptr Pointer to allocated memory (if NULL, new memory will be allocated).
	 * @param NewSize Desired size of the reallocated memory in bytes (if 0, the memory will be freed).
	 * @param Alignment Desired memory alignment in bytes.
	 */
	virtual void* Realloc (void* Ptr, DWORD NewSize, DWORD Alignment)
	{
		check(Alignment == DEFAULT_ALIGNMENT && "Alignment is currently unsupported in Windows");

		void* NewPtr = NULL;

		if (NewSize == 0)
		{
			Free(Ptr);
		}
		else if (Ptr == NULL)
		{
			NewPtr = Malloc(NewSize, Alignment);
		}
		else
		{
			STAT(DWORD OldSize = scalable_msize(Ptr));

			MEM_TIME(MemTime -= appSeconds());
			NewPtr = scalable_realloc(Ptr, NewSize);
			MEM_TIME(MemTime += appSeconds());

			STAT(checkSlow(UsedCurrent >= OldSize));

			STAT(UsedCurrent -= OldSize);
			STAT(UsedCurrent += (NewPtr != NULL) ? scalable_msize(NewPtr) : 0);
			STAT(UsedPeak = Max(UsedPeak, UsedCurrent));
		}

		return NewPtr;
	}
	
	/** 
	 * Inherited from FMalloc. Frees previously allocated memory.
	 *
	 * @param Ptr Pointer to allocated memory.
	 */
	virtual void Free (void* Ptr)
	{
		if (Ptr == NULL)
		{
			return;
		}

		STAT(DWORD OldSize = scalable_msize(Ptr));

		MEM_TIME(MemTime -= appSeconds());
		scalable_free(Ptr);
		MEM_TIME(MemTime += appSeconds());

		STAT(checkSlow(UsedCurrent >= OldSize));
		STAT(checkSlow(CurrentAllocs > 0));

		STAT(UsedCurrent -= OldSize);
		STAT(CurrentAllocs--);
	}

	/**
	 * Inherited from FMalloc. Dumps details about all allocations to an output device
	 *
	 * @param Ar [in] Output device
	 */
	virtual void DumpAllocations (FOutputDevice& Ar)
	{
		STAT(Ar.Logf(TEXT("Memory Allocation Status")));
		STAT(Ar.Logf(TEXT("Current Memory %.2f MB used, plus 0.00 MB waste"), UsedCurrent / (1024.0f * 1024.0f)));
		STAT(Ar.Logf(TEXT("Peak Memory %.2f MB used, plus 0.00 MB waste"), UsedPeak / (1024.0f * 1024.0f)));
		STAT(Ar.Logf(TEXT("Allocs %i Current / %i Total"), CurrentAllocs, TotalAllocs));
		
		MEM_TIME(Ar.Logf("Seconds % 5.3f", MemTime ));

		STAT_FAST(Ar.Logf(TEXT("GlobalMemoryStatus")));
		STAT_FAST(Ar.Logf(TEXT("TBB_AllocatedFromOS        %6.2f MB"), GetOSUsedMemory() / (1024.0f * 1024.0f)));
	}

	/**
	 * Inherited from FMalloc. Gathers all current memory stats.
	 *
	 * @param MemStats [out] Structure containing information about the size of allocations
	 */
	virtual void FMallocTBB::GetAllocationInfo (FMemoryAllocationStats& MemStats)
	{
		SIZE_T OSUsedMemory = GetOSUsedMemory();

		MemStats.TotalUsed = OSUsedMemory;
		MemStats.TotalAllocated = OSUsedMemory;

#if STATS
		MemStats.CPUUsed = UsedCurrent;
#endif
		MemStats.CPUWaste = 0;
		MemStats.CPUSlack = 0;

		MemStats.OSReportedUsed = OSUsedMemory;
		MemStats.OSReportedFree = GetOSAvailableMemory();
	}


protected:

	/** Holds the time in seconds spend on allocations. */
	DOUBLE MemTime;

#if STATS
	/** Holds the current number of allocations. */
	QWORD CurrentAllocs;

	/** Holds the total number of allocations. */
	QWORD TotalAllocs;

	/** Holds the amount of currently allocated memory (in bytes). */
	QWORD UsedCurrent;

	/** Holds the peak amount of allocated memory (in bytes). */
	QWORD UsedPeak;
#endif


protected:

	/**
	 * Queries the operating system for the amount of memory available.
	 *
	 * @return The amount of available memory in bytes.
	 */
	SIZE_T GetOSAvailableMemory () const
	{
		PROCESS_MEMORY_COUNTERS MemCounters;
		GetProcessMemoryInfo(GetCurrentProcess(), &MemCounters, sizeof(MemCounters));

		return MemCounters.WorkingSetSize;
	}

	/**
	 * Queries the operating system for the amount of memory used by this process.
	 *
	 * @return The amount of used memory in bytes.
	 */
	SIZE_T GetOSUsedMemory () const
	{
		PERFORMANCE_INFORMATION PerfInfo;
		GetPerformanceInfo(&PerfInfo, sizeof(PerfInfo));

		return PerfInfo.PhysicalAvailable;
	}

	/**
	 * Handles out-of-memory errors.
	 */
	void OutOfMemory () const
	{
		appErrorf(*LocalizeError("OutOfMemory",TEXT("Core")));
	}
};


#endif	// #ifdef _WIN64

#endif	// #ifndef __FMALLOCTBB_H__