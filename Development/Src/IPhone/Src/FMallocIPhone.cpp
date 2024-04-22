/*=============================================================================
 FMallocIPhone.cpp: iPhone memory allocator definitions
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "CorePrivate.h"
#include "FMallocIPhone.h"
#include "UnMallocDLIPhone.inl" 
#include "IPhoneObjCWrapper.h" 
#include "Core.h"

#if USE_DETAILED_IPHONE_MEM_TRACKING
#define RECORD_ALLOC(Ptr) RecordAlloc(Ptr)
#define RECORD_FREE(Ptr) RecordFree(Ptr)
#else
#define RECORD_ALLOC(Ptr)
#define RECORD_FREE(Ptr)
#endif

/**
*Utility function that prints information about the current state of allocated memory
*/
#if USE_DETAILED_IPHONE_MEM_TRACKING
void PrintAllocationStats ( class FOutputDevice& Ar, QWORD AllocSize, QWORD AllocCount, QWORD AllocPeak, UINT* OpenGLAllocSize, UINT* OpenGLAllocCount)
{
	uint64_t TaskResident, TaskVirtual;
	uint64_t PhysicalFreeMem, PhysicalUsedMem;
	size_t   Footprint = dlmalloc_footprint();

	IPhoneGetTaskMemoryInfo(TaskResident, TaskVirtual);
	IPhoneGetPhysicalMemoryInfo(PhysicalFreeMem, PhysicalUsedMem);

	Ar.Logf(TEXT("Allocations:"));
	Ar.Logf(TEXT("	Tracked:              %4.2f MB (%u)"), static_cast<UINT>(AllocSize / 1024) / 1024.f, static_cast<UINT>(AllocCount));
	Ar.Logf(TEXT("	Peak:                 %4.2f MB"), static_cast<UINT>(AllocPeak / 1024) / 1024.f);
	Ar.Logf(TEXT("	GLBufferTracked:      %4.2f MB (%u)"), static_cast<UINT>(OpenGLAllocSize[FMalloc::GLBUFFER] / 1024) / 1024.f, static_cast<UINT>(OpenGLAllocCount[FMalloc::GLBUFFER]));
	Ar.Logf(TEXT("	GLTextureTracked:     %4.2f MB (%u)"), static_cast<UINT>(OpenGLAllocSize[FMalloc::GLTEXTURE] / 1024) / 1024.f, static_cast<UINT>(OpenGLAllocCount[FMalloc::GLTEXTURE]));
	Ar.Logf(TEXT("	GLSurfaceTracked:     %4.2f MB (%u)"), static_cast<UINT>(OpenGLAllocSize[FMalloc::GLSURFACE] / 1024) / 1024.f, static_cast<UINT>(OpenGLAllocCount[FMalloc::GLSURFACE]));
	Ar.Logf(TEXT("	GLBackBufferTracked:  %4.2f MB (%u)"), static_cast<UINT>(OpenGLAllocSize[FMalloc::GLBACKBUFFER] / 1024) / 1024.f, static_cast<UINT>(OpenGLAllocCount[FMalloc::GLBACKBUFFER]));
	Ar.Logf(TEXT("	DLMallocFootprint:    %4.2f MB"), static_cast<UINT>(Footprint / 1024) / 1024.f);

	QWORD totalAllocSize= OpenGLAllocSize[FMalloc::GLBUFFER] + OpenGLAllocSize[FMalloc::GLTEXTURE] + OpenGLAllocSize[FMalloc::GLSURFACE] + OpenGLAllocSize[FMalloc::GLBACKBUFFER] +Footprint;
	QWORD totalAllocCount = OpenGLAllocCount[FMalloc::GLBUFFER] + OpenGLAllocCount[FMalloc::GLTEXTURE] + OpenGLAllocCount[FMalloc::GLSURFACE] + OpenGLAllocCount[FMalloc::GLBACKBUFFER] + AllocCount;
	Ar.Logf(TEXT("	TotalTracked:         %4.2f MB (%u)"), static_cast<UINT>((totalAllocSize) / 1024) / 1024.f, static_cast<UINT>(totalAllocCount));
}
#endif


FMallocIPhoneDL::FMallocIPhoneDL() 
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	: TaskResidentPeak(0)
	, TaskResidentRecentPeak(0)
	, TaskResidentRecentPeakAgo(0.0)
	, TaskVirtualPeak(0)
	, TaskVirtualRecentPeak(0)
	, TaskVirtualRecentPeakAgo(0)
	, bCheckMemoryEveryFrame(FALSE)
#endif
#if USE_DETAILED_IPHONE_MEM_TRACKING
#if !(!FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE)
	:
#else
	, 
#endif	  
	  CurrentAllocs(0)
	, TotalAllocs(0)
	, UsedCurrent(0)
	, UsedPeak(0)
#endif
{
#if USE_DETAILED_IPHONE_MEM_TRACKING
	appMemset(TrackedExternalOpenGLMemory, 0, sizeof(UINT)* GLTRACKINGTYPESCOUNT);
	appMemset(TrackedExternalOpenGLAllocCount, 0, sizeof(UINT)* GLTRACKINGTYPESCOUNT);
#endif
}

/**
 * Allocates memory using dlmalloc
 *
 * @param	Size		Size of request in bytes
 * @param	Alignment	Alignment of returned pointer
 * @return	Allocated memory, aligned by Alignment
 */	
void* FMallocIPhoneDL::Malloc( DWORD Size, DWORD Alignment )
{
	void* Ptr = NULL;

	if( Alignment != DEFAULT_ALIGNMENT )
	{
		Ptr = dlmemalign( Alignment, Size );
	}
	else
	{
		Ptr = dlmalloc( Size );
	}

	if( !Ptr )
	{
		appOutputDebugStringf(TEXT("Ran out of memory allocating %d bytes") LINE_TERMINATOR, Size);
	}

	RECORD_ALLOC(Ptr);

	return Ptr;
}

/**
 * Frees passed in pointer.
 *
 * @param	Ptr			Allocation to free
 */
void FMallocIPhoneDL::Free( void* Ptr )
{
	RECORD_FREE( Ptr );
	dlfree( Ptr );
}

/**
 * Reallocates memory, ANSI C compliant.
 *
 * @param	Ptr			Pointer to reallocate
 * @param	NewSize		Requested size of new allocation, can be 0 to free
 * @param	Alignment	Alignment of new allocation
 * @return	New pointer, or NULL if it either fails of NewSize is 0
 */
void* FMallocIPhoneDL::Realloc( void* Ptr, DWORD NewSize, DWORD Alignment )
{
	void* NewPtr = NULL;

	if( Ptr && NewSize > 0 )
	{
		if (Alignment != DEFAULT_ALIGNMENT)
		{
			NewPtr = Malloc( NewSize, Alignment );
			DWORD PtrSize = dlmalloc_usable_size(Ptr);
			appMemcpy( NewPtr, Ptr, Min<DWORD>(PtrSize,NewSize));
			Free( Ptr );
		}
		else
		{
			RECORD_FREE(Ptr);
			NewPtr = dlrealloc(Ptr, NewSize);
			RECORD_ALLOC(NewPtr);
		}
	}
	else if( !Ptr )
	{
		NewPtr = Malloc( NewSize, Alignment );
	}
	else
	{
		Free( Ptr );
	}

	return NewPtr;
}
/**
 * Returns stats about current memory usage.
 *
 * @param FMemoryAllocationStats	[out] structure containing information about the size of allocations
 */
void FMallocIPhoneDL::GetAllocationInfo( FMemoryAllocationStats& MemStats )
{
	uint64_t TaskResident, TaskVirtual;

	IPhoneGetTaskMemoryInfo(TaskResident, TaskVirtual);

	// The virtual memory number isn't very important in general, as the physical number is what drives eviction, etc...
	// but the amount of additional paged out or never-loaded virtual memory is (TaskVirtual - TaskResident) if needed
	MemStats.TotalUsed = TaskResident;
	MemStats.OSReportedUsed = TaskResident;
	MemStats.CPUUsed = TaskVirtual;

	// This shows up as "Physical Memory Used" in STAT MEMORY. It's the amount of memory that we're actually using.
	MemStats.GPUUsed = TaskResident;
}

/**
*Dumps details about allocation deltas to an output device 
*@param OutputDevice - Output Device
*@param StartingMemStats - The previously recorded allocation stats for which to calculate deltas against
*/
#if USE_SCOPED_MEM_STATS
void FMallocIPhoneDL::DumpAllocationsDeltas (FOutputDevice& OutputDevice, FMemoryAllocationStats& StartingMemStats )
{
	//Determine the task memory deltas
	FMemoryAllocationStats CurrentMemStats;
	GetAllocationInfo(CurrentMemStats);
	INT TaskResidentDiff = static_cast<INT>(CurrentMemStats.TotalUsed) - static_cast<INT>(StartingMemStats.TotalUsed);
	INT TaskVirtualDiff = static_cast<INT>(CurrentMemStats.CPUUsed) - static_cast<INT>(StartingMemStats.CPUUsed);

	OutputDevice.Logf(TEXT("Task Memory Deltas: "));
	OutputDevice.Logf(TEXT("	TaskResisdentDelta:    %4.4f MB (%4.6f KB)"), static_cast<INT>(TaskResidentDiff / 1024) / 1024.f, static_cast<INT>(TaskResidentDiff) / 1024.f);
	OutputDevice.Logf(TEXT("	TaskVirtualDelta:      %4.4f MB (%4.6f KB)"), static_cast<INT>(TaskVirtualDiff / 1024) / 1024.f , static_cast<INT>(TaskVirtualDiff) / 1024.f);
}    
#endif

/**
 * Dumps details about all allocations to an output device
 *
 * @param Ar	[in] Output device
 */
void FMallocIPhoneDL::DumpAllocations( class FOutputDevice& Ar )
{

#if USE_DETAILED_IPHONE_MEM_TRACKING
	PrintAllocationStats(Ar, UsedCurrent, CurrentAllocs, UsedPeak, TrackedExternalOpenGLMemory, TrackedExternalOpenGLAllocCount);
#endif

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE

	//NOTE - the output format iOS_* is required by the memleakcheck difer tool.  If you need to make
	//any changes to any of the following loged information, then you need to update that tool as well. 
	uint64_t TaskResident, TaskVirtual;
	uint64_t PhysicalFreeMem, PhysicalUsedMem;

	IPhoneGetTaskMemoryInfo(TaskResident, TaskVirtual);
	IPhoneGetPhysicalMemoryInfo(PhysicalFreeMem, PhysicalUsedMem);

	Ar.Logf(TEXT("Memory Allocation Status"));
	Ar.Logf(TEXT("dlmalloc_footprint()        %lld"), (uint64_t)dlmalloc_footprint());
	Ar.Logf(TEXT("iOS_PhysicalFree            %lld"), PhysicalFreeMem);
	Ar.Logf(TEXT("iOS_PhysicalUsed            %lld"), PhysicalUsedMem);
	Ar.Logf(TEXT("iOS_TaskResident            %lld"), TaskResident);
	Ar.Logf(TEXT("iOS_TaskVirtual             %lld"), TaskVirtual);

	if (bCheckMemoryEveryFrame == TRUE)
	{
		Ar.Logf(TEXT("iOS_TaskResidentPeak        %i"), TaskResidentPeak);

		if (TaskResidentRecentPeak > 0)
		{
			Ar.Logf(TEXT("iOS_TaskResidentRecentPeak    %i"), TaskResidentRecentPeak);
			Ar.Logf(TEXT("iOS_TaskResidentRecentPeakAgo %f"), appSeconds() - TaskResidentRecentPeakAgo);
		}

		Ar.Logf(TEXT("iOS_TaskVirtualPeak          %i"), TaskVirtualPeak);

		if (TaskVirtualRecentPeak > 0)
		{
			Ar.Logf(TEXT("iOS_TaskVirtualRecentPeak    %i"), TaskVirtualRecentPeak);
			Ar.Logf(TEXT("iOS_TaskVirtualRecentPeakAgo %f"), appSeconds() - TaskVirtualRecentPeakAgo);
		}

		// Reset the recent lowest, so it gets recalculated by the next interval
		TaskResidentRecentPeak = 0;
		TaskVirtualRecentPeak = 0;
	}

	// Blank lines for parser
	Ar.Log(TEXT(""));
	Ar.Log(TEXT(""));
#endif
}

/**
*Calls check_malloc_state in the dl malloc implementation for IPhone. 
*Will assert if heap is in invalid state.
*Must define DEBUG for calls to dlmalloc to work.(can quickly be done in source file at dlmalloc inl include)
*/
UBOOL FMallocIPhoneDL::ValidateHeap()
{
	check_malloc_state(&_gm_);
	return TRUE;
}

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
void FMallocIPhoneDL::Tick( FLOAT DeltaTime )
{
	if (bCheckMemoryEveryFrame == TRUE)
	{
		uint64_t TaskResident, TaskVirtual;
		IPhoneGetTaskMemoryInfo(TaskResident, TaskVirtual);

		// track resident peaks
		TaskResidentPeak = Max<INT>(TaskResidentPeak, TaskResident);

		if (TaskResident > TaskResidentRecentPeak)
		{
			TaskResidentRecentPeak = TaskResident;
			TaskResidentRecentPeakAgo = appSeconds();
		}

		// track virtual peaks
		TaskVirtualPeak = Max<INT>(TaskVirtualPeak, TaskVirtual);

		if (TaskVirtual > TaskVirtualRecentPeak)
		{
			TaskVirtualRecentPeak = TaskVirtual;
			TaskVirtualRecentPeakAgo = appSeconds();
		}
	}

	FMalloc::Tick(DeltaTime);
}
#endif

UBOOL FMallocIPhoneDL::Exec( const TCHAR* Cmd, FOutputDevice& Ar ) 
{ 
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	if( ParseCommand(&Cmd,TEXT("TRACKLOWESTMEMORY")) )
	{
		// We are actually tracking highest memory since we don't know what the 
		// total memory available to the run actually is...

		// Parse the argument
		UBOOL bNewValue = bCheckMemoryEveryFrame;
		FString Parameter0(ParseToken(Cmd, 0));

		if(Parameter0.Len())
		{
			bNewValue = appAtoi(*Parameter0) != 0;
		}

		// Reset the variables if just enabled (as they're currently stale) or disabled (as they'll no longer be valid)
		if (bCheckMemoryEveryFrame != bNewValue)
		{
			bCheckMemoryEveryFrame = bNewValue;

			TaskResidentRecentPeakAgo = 0.0;
			TaskResidentRecentPeak = 0;
			TaskResidentPeak = 0;

			TaskVirtualRecentPeakAgo = 0.0;
			TaskVirtualRecentPeak = 0;
			TaskVirtualPeak = 0;
		}

		// Remind the user what the current state is
		Ar.Logf(TEXT("TrackLowestMemory: Per-frame tracking is %s."), bCheckMemoryEveryFrame ? TEXT("Enabled") : TEXT("Disabled"));

		return TRUE;
	}
#endif

	return FALSE;
}

#if USE_DETAILED_IPHONE_MEM_TRACKING
/**
*Records statistics about the allocation
*@param Ptr - Pointer to the memory allocated.
*/
void FMallocIPhoneDL::RecordAlloc( void* Ptr )
{
	if ( Ptr )
	{
		size_t MemSize = dlmalloc_usable_size(Ptr);
		UsedCurrent += MemSize;		
		UsedPeak = Max(UsedPeak, UsedCurrent);
		CurrentAllocs++;
		TotalAllocs++;
	}
}

/**
*Records statistics about the memory being freed
*@param Ptr - Pointer to the memory allocated.
*/
void FMallocIPhoneDL::RecordFree( void* Ptr )
{
	if ( Ptr )
	{
		size_t MemSize = dlmalloc_usable_size(Ptr);
		UsedCurrent -= MemSize;		
		CurrentAllocs--;
	}
}
#endif