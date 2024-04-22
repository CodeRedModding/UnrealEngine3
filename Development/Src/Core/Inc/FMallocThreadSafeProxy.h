/*=============================================================================
	FMallocThreadSafeProxy.h: FMalloc proxy used to render any FMalloc thread
							  safe.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FMALLOCTHREADSAFEPROXY_H__
#define __FMALLOCTHREADSAFEPROXY_H__

/**
 * FMalloc proxy that synchronizes access, making the used malloc thread safe.
 */
class FMallocThreadSafeProxy : public FMalloc
{
private:
	/** Malloc we're based on, aka using under the hood							*/
	FMalloc*			UsedMalloc;
	/** Object used for synchronization via a scoped lock						*/
	FCriticalSection	SynchronizationObject;

public:
	/**
	 * Constructor for thread safe proxy malloc that takes a malloc to be used and a
	 * synchronization object used via FScopeLock as a parameter.
	 * 
	 * @param	InMalloc					FMalloc that is going to be used for actual allocations
	 */
	FMallocThreadSafeProxy( FMalloc* InMalloc)
	:	UsedMalloc( InMalloc )
	{}

	/** 
	 * QuantizeSize returns the actual size of allocation request likely to be returned
	 * so for the template containers that use slack, they can more wisely pick
	 * appropriate sizes to grow and shrink to.
	 *
	 * CAUTION: QuantizeSize is a special case and is NOT guarded by a thread lock, so must be intrinsically thread safe!
	 *
	 * @param Size			The size of a hypothetical allocation request
	 * @param Alignment		The alignment of a hypothetical allocation request
	 * @return				Returns the usable size that the allocation request would return. In other words you can ask for this greater amount without using any more actual memory.
	 */
	virtual DWORD QuantizeSize( DWORD Size, DWORD Alignment )
	{
		return UsedMalloc->QuantizeSize(Size,Alignment); 
	}
	/** 
	 * Malloc
	 */
	void* Malloc( DWORD Size, DWORD Alignment )
	{
		FScopeLock ScopeLock( &SynchronizationObject );
		STAT(TotalMallocCalls++);
		return UsedMalloc->Malloc( Size, Alignment );
	}

	/** 
	 * Realloc
	 */
	void* Realloc( void* Ptr, DWORD NewSize, DWORD Alignment )
	{
		FScopeLock ScopeLock( &SynchronizationObject );
		STAT(TotalReallocCalls++);
		return UsedMalloc->Realloc( Ptr, NewSize, Alignment );
	}

	/** 
	 * Free
	 */
	void Free( void* Ptr )
	{
		if( Ptr )
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			STAT(TotalFreeCalls++);
			UsedMalloc->Free( Ptr );
		}
	}

	/** 
	 * Physical alloc
	 */
	void* PhysicalAlloc( DWORD Size, ECacheBehaviour InCacheBehaviour )
	{
		FScopeLock ScopeLock( &SynchronizationObject );
		STAT(TotalPhysicalAllocCalls++);
		return UsedMalloc->PhysicalAlloc( Size, InCacheBehaviour );
	}

	/** 
	 * Physical free
	 */
	void PhysicalFree( void* Ptr )
	{
		if( Ptr )
		{
			FScopeLock ScopeLock( &SynchronizationObject );
			STAT(TotalPhysicalFreeCalls++);
			UsedMalloc->PhysicalFree( Ptr );
		}
	}

	/**
	 * Passes request for gathering memory allocations for both virtual and physical allocations
	 * on to used memory manager.
	 *
	 * @param FMemoryAllocationStats	[out] structure containing information about the size of allocations
	 */
	void GetAllocationInfo( FMemoryAllocationStats& MemStats )
	{
		FScopeLock ScopeLock( &SynchronizationObject );
		UsedMalloc->GetAllocationInfo( MemStats );
	}

	/**
	*Dumps details about allocation deltas to an output device
	*@param OutputDevice - Output Device
	*@param startingMemStats - The previously recorded allocation stats for which to calculate deltas against
	*/
#if USE_SCOPED_MEM_STATS
	virtual void DumpAllocationsDeltas (FOutputDevice& OutputDevice , FMemoryAllocationStats& StartingMemStats)
	{
		FScopeLock Lock( &SynchronizationObject );
		UsedMalloc->DumpAllocationsDeltas(OutputDevice, StartingMemStats);
	}
#endif

	/**
	 * Dumps details about all allocations to an output device
	 *
	 * @param Ar	[in] Output device
	 */
	virtual void DumpAllocations( class FOutputDevice& Ar ) 
	{
		FScopeLock Lock( &SynchronizationObject );
		UsedMalloc->DumpAllocations( Ar );
	}

	/**
	 * Validates the allocator's heap
	 */
	virtual UBOOL ValidateHeap()
	{
		FScopeLock Lock( &SynchronizationObject );
		return( UsedMalloc->ValidateHeap() );
	}

	/**
	 * Keeps trying to allocate memory until we fail
	 *
	 * @param Ar Device to send output to
	 */
	void CheckMemoryFragmentationLevel( class FOutputDevice& Ar )
	{
		FScopeLock ScopeLock( &SynchronizationObject );
		UsedMalloc->CheckMemoryFragmentationLevel( Ar );
	}


	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar )
	{
		FScopeLock ScopeLock( &SynchronizationObject );
		return UsedMalloc->Exec(Cmd, Ar);
	}


	/** Called every game thread tick */
	void Tick( FLOAT DeltaTime )
	{
		FScopeLock ScopeLock( &SynchronizationObject );
		return UsedMalloc->Tick( DeltaTime );
	}

	/**
	 * If possible give memory back to the os from unused segments
	 *
	 * @param ReservePad - amount of space to reserve when trimming
	 * @param bShowStats - log stats about how much memory was actually trimmed. Disable this for perf
	 * @return TRUE if succeeded
	 */
	UBOOL TrimMemory(SIZE_T ReservePad,UBOOL bShowStats=FALSE)
	{
		FScopeLock ScopeLock( &SynchronizationObject );
		return UsedMalloc->TrimMemory(ReservePad,bShowStats);
	}

	/**
	* If possible determine the size of the memory allocated at the given address
	*
	* @param Original - Pointer to memory we are checking the size of
	* @param SizeOut - If possible, this value is set to the size of the passed in pointer
	* @return TRUE if succeeded
	*/
	UBOOL GetAllocationSize(void *Original, DWORD &SizeOut)
	{
		FScopeLock ScopeLock( &SynchronizationObject );
		return UsedMalloc->GetAllocationSize(Original,SizeOut);
	}

#if USE_DETAILED_IPHONE_MEM_TRACKING
		/**
	*Decrements the tracked OpenGL memory that is externally
	*allocated
	*@param Size - The size in bytes of the deallocation to track
	*/
	virtual void IncTrackedOpenGLMemory(UINT Size, OpenGLBufferTrackingType trackingType)
	{
		FScopeLock ScopeLock( &SynchronizationObject );
		UsedMalloc->IncTrackedOpenGLMemory(Size, trackingType);
	}

	/**
	*Decrements the tracked OpenGL memory that is externally
	*allocated
	*@param Size - The size in bytes of the deallocation to track
	*/
	virtual void DecTrackedOpenGLMemory(UINT Size, OpenGLBufferTrackingType trackingType) 
	{ 
		FScopeLock ScopeLock( &SynchronizationObject );
		UsedMalloc->DecTrackedOpenGLMemory(Size, trackingType);
	}
#endif

};

#endif
