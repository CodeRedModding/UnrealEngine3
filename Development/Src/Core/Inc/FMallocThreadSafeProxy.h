/*=============================================================================
	FMallocThreadSafeProxy.h: FMalloc proxy used to render any FMalloc thread
							  safe.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

/**
 * FMalloc proxy that synchronizes access, making the used malloc thread safe.
 */
class FMallocThreadSafeProxy : public FMalloc
{
private:
	/** Malloc we're based on, aka using under the hood							*/
	FMalloc*		UsedMalloc;
	/** Object used for synchronization via a scoped lock						*/
	FSynchronize*	SynchronizationObject;

public:
	/**
	 * Constructor for thread safe proxy malloc that takes a malloc to be used and a
	 * synchronization object used via FScopeLock as a parameter.
	 * 
	 * @param	InMalloc					FMalloc that is going to be used for actual allocations
	 * @param	InSynchronizationObject		Synchronization object used to ensure thread safety
	 */
	FMallocThreadSafeProxy( FMalloc* InMalloc, FSynchronize* InSynchronizationObject )
	:	UsedMalloc( InMalloc ),
		SynchronizationObject( InSynchronizationObject )
	{}

	// FMalloc interface.

	void* Malloc( DWORD Size )
	{
		FScopeLock ScopeLock( SynchronizationObject );
		return UsedMalloc->Malloc( Size );
	}
	void* Realloc( void* Ptr, DWORD NewSize )
	{
		FScopeLock ScopeLock( SynchronizationObject );
		return UsedMalloc->Realloc( Ptr, NewSize );
	}
	void Free( void* Ptr )
	{
		FScopeLock ScopeLock( SynchronizationObject );
		UsedMalloc->Free( Ptr );
	}
	void* PhysicalAlloc( DWORD Size, ECacheBehaviour InCacheBehaviour )
	{
		FScopeLock ScopeLock( SynchronizationObject );
		return UsedMalloc->PhysicalAlloc( Size, InCacheBehaviour );
	}
	void PhysicalFree( void* Ptr )
	{
		FScopeLock ScopeLock( SynchronizationObject );
		UsedMalloc->PhysicalFree( Ptr );
	}
	/**
	 * Passes request for gathering memory allocations for both virtual and physical allocations
	 * on to used memory manager.
	 *
	 * @param Virtual	[out] size of virtual allocations
	 * @param Physical	[out] size of physical allocations	
	 */
	void GetAllocationInfo( SIZE_T& Virtual, SIZE_T& Physical )
	{
		FScopeLock ScopeLock( SynchronizationObject );
		UsedMalloc->GetAllocationInfo( Virtual, Physical );
	}
	void DumpAllocs()
	{
		FScopeLock ScopeLock( SynchronizationObject );
		UsedMalloc->DumpAllocs();
	}
	void HeapCheck()
	{
		FScopeLock ScopeLock( SynchronizationObject );
		UsedMalloc->HeapCheck();
	}
	void Init( UBOOL Reset )
	{
		FScopeLock ScopeLock( SynchronizationObject );
		UsedMalloc->Init( Reset );
	}
	void DumpMemoryImage()
	{
		FScopeLock ScopeLock( SynchronizationObject );
		UsedMalloc->DumpMemoryImage();
	}
	void Exit()
	{
		FScopeLock ScopeLock( SynchronizationObject );
		UsedMalloc->Exit();
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
