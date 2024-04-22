/*=============================================================================
	FMallocProxySimpleTag.h: Simple tag based allocation tracker.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FMALLOCPROXYSIMPLETAG_H__
#define __FMALLOCPROXYSIMPLETAG_H__

class FMallocProxySimpleTag : public FMalloc
{
	/**
	 * Helper structure containing size and tag of allocation.
	 */
	struct FAllocInfo
	{
		/** Size of allocation.	*/
		INT Size;
		/** Tag at time of first allocation. Maintained for realloc. */ 
		INT OriginalTag;
		/** Tag at time of allocation. */
		INT CurrentTag;
		/** Allocation count, used for grouping during output. */
		INT Count;
	};

	/** Allocator we are actually passing requests to. */
	FMalloc*							UsedMalloc;
	/** Map from allocation pointer to size. */
	TMap<PTRINT,FAllocInfo>				AllocToInfoMap;
	/** Total size of current allocations in bytes. */
	SIZE_T								TotalAllocSize;
	/** Number of allocations. */
	SIZE_T								TotalAllocCount;
	/** Used to avoid re-entrancy (i.e. when doing map allocations)	*/
	UBOOL								bIsTracking;

	/**
	 * Add allocation to keep track of.
	 *
	 * @param	Pointer		Allocation
	 * @param	Size		Allocation size in bytes
	 * @param	Tag			Tag to use for original tag
	 */
	void AddAllocation( void* Pointer, SIZE_T Size, INT OriginalTag );
	
	/**
	 * Remove allocation from list to track.
	 *
	 * @param	Pointer		Allocation
	 * @return	Original tag of allocation
	 */
	INT RemoveAllocation( void* Pointer );

public:
	/** Current active tag.	*/
	static INT							CurrentTag;

	/**
	 * Constructor, intializing member variables and underlying allocator to use.
	 *
	 * @param	InMalloc	Allocator to use for allocations.
	 */
	FMallocProxySimpleTag(FMalloc* InMalloc);

	/** 
	 * Malloc
	 */
	virtual void* Malloc( DWORD Count, DWORD Alignment=DEFAULT_ALIGNMENT );

	/** 
	 * Realloc
	 */
	virtual void* Realloc( void* Original, DWORD Count, DWORD Alignment=DEFAULT_ALIGNMENT );

	/** 
	 * Free
	 */
	virtual void Free( void* Original );

	/** 
	 * Physical alloc
	 */
	virtual void* PhysicalAlloc( DWORD Count, ECacheBehaviour CacheBehaviour = CACHE_WriteCombine );

	/** 
	 * Physical free
	 */
	virtual void PhysicalFree( void* Original );

	/**
	 * Passes request for gathering memory allocations for both virtual and physical allocations
	 * on to used memory manager.
	 *
	 * @param FMemoryAllocationStats	[out] structure containing information about the size of allocations
	 */
	virtual void GetAllocationInfo( FMemoryAllocationStats& MemStats )
	{
		UsedMalloc->GetAllocationInfo( MemStats );
	}

	/**
	* If possible determine the size of the memory allocated at the given address
	*
	* @param Original - Pointer to memory we are checking the size of
	* @param SizeOut - If possible, this value is set to the size of the passed in pointer
	* @return TRUE if succeeded
	*/
	virtual UBOOL GetAllocationSize(void *Original, DWORD &SizeOut)
	{		
		return UsedMalloc->GetAllocationSize(Original,SizeOut);
	}

    /**
	*Dumps details about allocation deltas to an output device
	*@param OutputDevice - Output Device
	*@param startingMemStats - The previously recorded allocation stats for which to calculate deltas against
	*/
#if USE_SCOPED_MEM_STATS
	virtual void DumpAllocationsDeltas (FOutputDevice& OutputDevice , FMemoryAllocationStats& StartingMemStats)
	{
		UsedMalloc->DumpAllocationsDeltas(OutputDevice, StartingMemStats);
	}
#endif

	/**
	 * Dumps details about all allocations to an output device
	 *
	 * @param Ar	[in] Output device
	 */
	virtual void DumpAllocations( class FOutputDevice& Ar );

	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );

	
#if USE_DETAILED_IPHONE_MEM_TRACKING
	/**
	*Decrements the tracked OpenGL memory that is externally
	*allocated
	*@param Size - The size in bytes of the deallocation to track
	*/
	virtual void IncTrackedOpenGLMemory(UINT Size, OpenGLBufferTrackingType trackingType)
	{
		UsedMalloc->IncTrackedOpenGLMemory(Size, trackingType);
	}

	/**
	*Decrements the tracked OpenGL memory that is externally
	*allocated
	*@param Size - The size in bytes of the deallocation to track
	*/
	virtual void DecTrackedOpenGLBufferMemory(UINT Size, OpenGLBufferTrackingType trackingType) 
	{ 
		UsedMalloc->DecTrackedOpenGLMemory(Size, trackingType);
	}
#endif
};

#endif
