/*=============================================================================
	FMallocIPhone.h: iPhone memory allocator declarations
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#ifndef __FMALLOCIPHONE_H__
#define __FMALLOCIPHONE_H__


/**
 * Using Doug Lea's allocator for iPhone (http://g.oswego.edu/)
 */
class FMallocIPhoneDL : public FMalloc
{
public:
	FMallocIPhoneDL();

	/**
	 * Allocates memory using dlmalloc
	 *
	 * @param	Size		Size of request in bytes
	 * @param	Alignment	Alignment of returned pointer
	 * @return	Allocated memory, aligned by Alignment
	 */
	virtual void*	Malloc( DWORD Size, DWORD Alignment );

	/**
	 * Frees passed in pointer.
	 *
	 * @param	Ptr			Allocation to free
	 */
	virtual void	Free( void* Ptr );

	/**
	 * Reallocates memory, ANSI C compliant.
	 *
	 * @param	Ptr			Pointer to reallocate
	 * @param	NewSize		Requested size of new allocation, can be 0 to free
	 * @param	Alignment	Alignment of new allocation
	 * @return	New pointer, or NULL if it either fails of NewSize is 0
	 */
	virtual void*	Realloc( void* Ptr, DWORD NewSize, DWORD Alignment );

	/**
	 * Returns stats about current memory usage.
	 *
	 * @param FMemoryAllocationStats	[out] structure containing information about the size of allocations
	 */
	virtual void	GetAllocationInfo( FMemoryAllocationStats& MemStats );




#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	/** The highest that the allocator reached, since the last time DumpAllocs was called (updated if bCheckMemoryEveryFrame) */
	DWORD				TaskResidentPeak;
	/** The highest that the allocator reached, since the last time DumpAllocs was called (updated if bCheckMemoryEveryFrame) */
	DWORD				TaskResidentRecentPeak;
	/** The time when the memory usage was highest, since the last time DumpAllocs was called (updated if bCheckMemoryEveryFrame) */
	DOUBLE				TaskResidentRecentPeakAgo;
	/** The highest that the allocator reached, since the last time DumpAllocs was called (updated if bCheckMemoryEveryFrame) */
	DWORD				TaskVirtualPeak;
	/** The highest that the allocator reached, since the last time DumpAllocs was called (updated if bCheckMemoryEveryFrame) */
	DWORD				TaskVirtualRecentPeak;
	/** The time when the memory usage was highest, since the last time DumpAllocs was called (updated if bCheckMemoryEveryFrame) */
	DOUBLE				TaskVirtualRecentPeakAgo;
	/** Should the Tick() for the allocator check the number of free pages each frame */
	UBOOL				bCheckMemoryEveryFrame;

	virtual void Tick( FLOAT DeltaTime );
#endif

	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );

    /**
	*Dumps details about allocation deltas to an output device
	*@param OutputDevice - Output Device
	*@param startingMemStats - The previously recorded allocation stats for which to calculate deltas against
	*/
#if USE_SCOPED_MEM_STATS
	virtual void DumpAllocationsDeltas (FOutputDevice& OutputDevice , FMemoryAllocationStats& StartingMemStats);
#endif

	/**
	 * Dumps details about all allocations to an output device
	 *
	 * @param Ar	[in] Output device
	 */
	virtual void DumpAllocations( class FOutputDevice& Ar );

	/**
	 * dlmalloc is not internally thread safe.
	 *
	 * @return FALSE (as we can only be called from one thread at a time)
	 */
	virtual UBOOL	IsInternallyThreadSafe() const
	{
		return FALSE;
	}

	/**
	*Calls check_malloc_state in the dl malloc implementation for IPhone.
	*Will assert if heap is in invalid state.
	*Must define DEBUG for calls to dlmalloc to work.(can quickly be done in source file at dlmalloc inl include)
	*/
	virtual UBOOL ValidateHeap();
	

#if USE_DETAILED_IPHONE_MEM_TRACKING
	/**
	*Decrements the tracked OpenGL memory that is externally
	*allocated
	*@param Size - The size in bytes of the deallocation to track
	*/
	virtual void IncTrackedOpenGLMemory(UINT Size, OpenGLBufferTrackingType trackingType)
	{
		TrackedExternalOpenGLMemory[trackingType] += Size;
		TrackedExternalOpenGLAllocCount[trackingType]++;
	}

	/**
	*Decrements the tracked OpenGL memory that is externally
	*allocated
	*@param Size - The size in bytes of the deallocation to track
	*/
	virtual void DecTrackedOpenGLMemory(UINT Size, OpenGLBufferTrackingType trackingType) 
	{ 
		TrackedExternalOpenGLMemory[trackingType] -= Size;
		TrackedExternalOpenGLAllocCount[trackingType]--;
	}


private:

	/**
	*Records statistics about the allocation
	*@param Ptr - Pointer to the memory allocated.
	*/
	void RecordAlloc( void* Ptr );

	/**
	*Records statistics about the memory being freed
	*@param Ptr - Pointer to the memory allocated.
	*/
	void RecordFree( void* Ptr );

protected:

	/** Holds the current number of allocations. */
	QWORD CurrentAllocs;

	/** Holds the total number of allocations. */
	QWORD TotalAllocs;

	/** Holds the amount of currently allocated memory (in bytes). */
	QWORD UsedCurrent;

	/** Holds the peak amount of allocated memory (in bytes). */
	QWORD UsedPeak;

	/** The tracked open GL memory that is allocated external to our malloc system*/
	UINT TrackedExternalOpenGLMemory[GLTRACKINGTYPESCOUNT];
	
	/** The tracked number of allocations through OpenGL with respect to TrackedExternalOpenGLMemory*/
	UINT TrackedExternalOpenGLAllocCount[GLTRACKINGTYPESCOUNT];

#endif
};



#endif

