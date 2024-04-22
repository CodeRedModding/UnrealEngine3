/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


#ifndef BEST_FIT_ALLOCATOR_H
#define BEST_FIT_ALLOCATOR_H


#define LOG_EVERY_ALLOCATION			0
#define DUMP_ALLOC_FREQUENCY			0 // 100


/*-----------------------------------------------------------------------------
	Custom fixed size pool best fit texture memory allocator
-----------------------------------------------------------------------------*/

// Forward declaration
class FAsyncReallocationRequest;

/**
 * Simple best fit allocator, splitting and coalescing whenever/ wherever possible.
 * NOT THREAD-SAFE.
 *
 * - uses TMap to find memory chunk given a pointer (potentially colliding with malloc/ free from main thread)
 * - uses separate linked list for free allocations, assuming that relatively few free chunks due to coalescing
 */
struct FBestFitAllocator
{
	typedef TDoubleLinkedList<FAsyncReallocationRequest*> FRequestList;
	typedef TDoubleLinkedList<FAsyncReallocationRequest*>::TDoubleLinkedListNode FRequestNode;

	/**
	 * Container for allocator settings.
	 */
	struct FSettings
	{
		FSettings()
		:	bEnableAsyncDefrag(FALSE)
		,	bEnableAsyncReallocation(FALSE)
		,	MaxDefragRelocations(128*1024)
		,	MaxDefragDownShift(32*1024)
		{
		}

		/** Whether to enable async defrag. */
		UBOOL	bEnableAsyncDefrag;
		/** Whether to allow async reallocation requests. Requires 'bEnableAsyncDefrag' to be TRUE. */
		UBOOL	bEnableAsyncReallocation;
		/** Maximum number of bytes to relocate, in total, during a partial defrag. */
		INT		MaxDefragRelocations;
		/** Maximum number of bytes to relocate during a partial defrag by brute-force downshifting. */
		INT		MaxDefragDownShift;
	};

	enum EMemoryElementType
	{
		MET_Allocated,
		MET_Free,
		MET_Locked,
		MET_Relocating,
		MET_Resizing,
		MET_Resized,
		MET_Max
	};

	struct FMemoryLayoutElement
	{
		FMemoryLayoutElement()
		:	Size( 0 )
		,	Type( MET_Allocated )
		{
		}
		FMemoryLayoutElement( INT InSize, EMemoryElementType InType )
		:	Size( InSize )
		,	Type( InType )
		{
		}
		INT					Size;
		EMemoryElementType	Type;

		friend FArchive& operator<<( FArchive& Ar, FMemoryLayoutElement& Element )
		{
			Ar << Element.Size;
			DWORD TypeDWORD = Element.Type;
			Ar << TypeDWORD;
			Element.Type = EMemoryElementType(TypeDWORD);
			return Ar;
		}
	};

	/**
	 * Container for allocator relocation stats.
	 */
	struct FRelocationStats
	{
		FRelocationStats()
		:	NumBytesRelocated(0)
		,	NumBytesDownShifted(0)
		,	NumRelocations(0)
		,	LargestHoleSize(0)
		,	NumHoles(0)
		{
		}

		/** Number of bytes relocated, in total. */
		INT NumBytesRelocated;
		/** Number of bytes relocated by brute-force downshifting. */
		INT NumBytesDownShifted;
		/** Number of relocations initiated. */
		INT NumRelocations;
		/** Size of the largest free consecutive memory region, before any relocations were made. */
		INT LargestHoleSize;
		/** Number of disjoint free memory regions, before any relocations were made. */
		INT NumHoles;
	};

	/**
	 * Contains information of a single allocation or free block.
	 */
	class FMemoryChunk
	{
	public:
		/**
		 * Private constructor.
		 *
		 * @param	InBase					Pointer to base of chunk
		 * @param	InSize					Size of chunk
		 * @param	ChunkToInsertAfter		Chunk to insert this after.
		 * @param	FirstFreeChunk			Reference to first free chunk pointer.
		 */
		FMemoryChunk( BYTE* InBase, INT InSize, FBestFitAllocator& InBestFitAllocator, FMemoryChunk*& ChunkToInsertAfter, /*FMemoryChunk*& InFirstChunk, FMemoryChunk*& InFirstFreeChunk,*/ UBOOL bSortedFreeList )
		:	Base(InBase)
		,	Size(InSize)
		,	bIsAvailable(FALSE)
		,	bLocked(FALSE)
		,	DefragCounter(0)
		,	BestFitAllocator(InBestFitAllocator)
		,	SyncIndex(0)
		,	SyncSize(0)
		,	UserPayload(0)
		,	ReallocationRequestNode(NULL)
		{
			Link( ChunkToInsertAfter );
			// This is going to change bIsAvailable.
			LinkFree( bSortedFreeList, ChunkToInsertAfter );
		}
		
		/**
		 * Unlinks/ removes the chunk from the linked lists it belongs to.
		 */
		~FMemoryChunk()
		{
			// Remove from linked lists.
			Unlink();
			UnlinkFree();
		}

		/**
		 * Inserts this chunk after the passed in one.
		 *
		 * @param	ChunkToInsertAfter	Chunk to insert after
		 */
		void	Link( FMemoryChunk*& ChunkToInsertAfter )
		{
			if( ChunkToInsertAfter )
			{
				NextChunk		= ChunkToInsertAfter->NextChunk;
				PreviousChunk	= ChunkToInsertAfter;
				ChunkToInsertAfter->NextChunk = this;
				if( NextChunk )
				{
					NextChunk->PreviousChunk = this;
				}
				else
				{
					BestFitAllocator.LastChunk = this;
				}
			}
			else
			{
				PreviousChunk		= NULL;
				NextChunk			= NULL;
				ChunkToInsertAfter	= this;
				BestFitAllocator.LastChunk = this;
			}
		}

		/**
		 * Inserts this chunk at the head of the free chunk list.
		 */
		void	LinkFree( UBOOL bMaintainSortOrder, FMemoryChunk* FirstFreeChunkToSearch );

		/**
		 * Removes itself for linked list.
		 */
		void	Unlink()
		{
			if( PreviousChunk )
			{
				PreviousChunk->NextChunk = NextChunk;
			}
			else
			{
				BestFitAllocator.FirstChunk = NextChunk;
			}
			
			if( NextChunk )
			{
				NextChunk->PreviousChunk = PreviousChunk;
			}
			else
			{
				BestFitAllocator.LastChunk = PreviousChunk;
			}

			PreviousChunk	= NULL;
			NextChunk		= NULL;
		}

		/**
		 * Removes itself for linked "free" list. Maintains the free-list order.
		 */
		void	UnlinkFree()
		{
			check(bIsAvailable);
			bIsAvailable = FALSE;

			if( PreviousFreeChunk )
			{
				PreviousFreeChunk->NextFreeChunk = NextFreeChunk;
			}
			else
			{
				BestFitAllocator.FirstFreeChunk = NextFreeChunk;
			}

			if( NextFreeChunk )
			{
				NextFreeChunk->PreviousFreeChunk = PreviousFreeChunk;
			}

			PreviousFreeChunk	= NULL;
			NextFreeChunk		= NULL;
		}

		/**
		 *	Returns TRUE if the Chunk has an associated reallocation request.
		 */
		UBOOL	HasReallocationRequest() const
		{
			return ReallocationRequestNode != NULL;
		}

		/**
		 *	Returns TRUE if the Chunk is being asynchronously relocated due to reallocation or defrag.
		 */
		UBOOL	IsRelocating() const
		{
			return SyncIndex > BestFitAllocator.CompletedSyncIndex;
		}

		/**
		 * Returns the number of bytes that can be allocated from this chunk.
		 */
		INT		GetAvailableSize( ) const
		{
			if ( bIsAvailable )
			{
				return IsRelocating() ? (Size - SyncSize) : Size;
			}
			else
			{
				return 0;
			}
		}

		/**
		 *	Returns the current size (in bytes), or the final size if it has a reallocating request.
		 */
		INT		GetFinalSize() const;

		/**
		 * Sets the relocation sync index.
		 * @param InSyncIndex	GPU synchronization identifier that can be compared with BestFitAllocator::CompletedSyncIndex
		 * @param InSyncSize	Number of bytes that require GPU synchronization (starting from the beginning of the chunk)
		 */
		void	SetSyncIndex( DWORD InSyncIndex, INT InSyncSize )
		{
			SyncIndex = InSyncIndex;
			SyncSize = InSyncSize;
		}

		/**
		 *	Returns the relocation sync index.
		 */
		DWORD	GetSyncIndex() const
		{
			return SyncIndex;
		}

		/**
		 *	Comparison function for Sort(), etc, based on increasing base address.
		 */
		static INT Compare( const FMemoryChunk* A, const FMemoryChunk* B )
		{
			return PTRINT(B->Base) - PTRINT(A->Base);
		}

#if USE_ALLOCATORFIXEDSIZEFREELIST
		/** Custom new/delete */
		void* operator new(size_t Size);
		void operator delete(void *RawMemory);
#endif

		/** Base of chunk.								*/
		BYTE*					Base;
		/** Size of chunk.								*/
		INT						Size;
		/** Whether the chunk is available.				*/
		DWORD					bIsAvailable : 1;
		/** Whether the chunk has been locked.			*/
		DWORD					bLocked : 1;
		/** Defrag counter. If this chunk failed to defrag, it won't try it again until the counter is 0. */
		DWORD					DefragCounter : 10;

		/** Allows access to FBestFitAllocator members such as FirstChunk, FirstFreeChunk and LastChunk. */
		FBestFitAllocator&		BestFitAllocator;
		/** Pointer to previous chunk.					*/
		FMemoryChunk*			PreviousChunk;
		/** Pointer to next chunk.						*/
		FMemoryChunk*			NextChunk;

		/** Pointer to previous free chunk.				*/
		FMemoryChunk*			PreviousFreeChunk;
		/** Pointer to next free chunk.					*/
		FMemoryChunk*			NextFreeChunk;
		/** SyncIndex that must be exceeded before accessing the data within this chunk. */
		DWORD					SyncIndex;
		/** Number of bytes covered by the SyncIndex (starting from the beginning of the chunk). */
		INT						SyncSize;
		/** User payload, e.g. platform-specific texture pointer. Only chunks with payload can be relocated. */
		PTRINT					UserPayload;

		/**
		 * Reallocation request for this chunk, or NULL.
		 * Points straight to a node in FBestFitAllocator::ReallocationRequests or FBestFitAllocator::ReallocationRequestsInProgress.
		 */
		FRequestNode*			ReallocationRequestNode;
	};

	/** Constructor, zero initializing all member variables */
	FBestFitAllocator()
	:	MemorySize(0)
	,	MemoryBase(NULL)
	,	AllocationAlignment(0)
	,	FirstChunk(NULL)
	,	LastChunk(NULL)
	,	FirstFreeChunk(NULL)
	,	TimeSpentInAllocator(0.0)
	,	AllocatedMemorySize(0)
	,	AvailableMemorySize(0)
	,	PendingMemoryAdjustment(0)
	,	CurrentSyncIndex(1)
	,	CompletedSyncIndex(0)
	,	NumRelocationsInProgress(0)
	,	PlatformSyncFence(0)
	,	TotalNumRelocations(0)
	,	TotalNumBytesRelocated(0)
	,	MaxNumHoles(0)
	,	MinLargestHole(MAXINT)
	,	NumFinishedAsyncReallocations(0)
	,	NumFinishedAsyncAllocations(0)
	,	NumCanceledAsyncRequests(0)
	,	BlockedCycles(0)
	,	bBenchmarkMode(FALSE)
	{}

	/**
	 * Initialize this allocator with a preallocated block of memory.
	 *
	 * @param InMemoryBase			Base address for the block of memory
	 * @param InMemorySize			Size of the block of memory, in bytes
	 * @param InAllocationAlignment	Alignment for all allocations, in bytes
	 */
	void	Initialize( BYTE* InMemoryBase, INT InMemorySize, INT InAllocationAlignment )
	{
		// Update size, pointer and alignment.
		MemoryBase			= InMemoryBase;
		MemorySize			= InMemorySize;
		AllocationAlignment	= InAllocationAlignment;
		check( Align( MemoryBase, AllocationAlignment ) == MemoryBase );

		// Update stats in a thread safe way.
		appInterlockedExchange( &AvailableMemorySize, MemorySize );
		// Allocate initial chunk.
		FirstChunk			= new FMemoryChunk( MemoryBase, MemorySize, *this, FirstChunk, /*FirstChunk, FirstFreeChunk,*/ FALSE );
		LastChunk			= FirstChunk;
	}

	/**
	 * Returns the current allocator settings.
	 *
	 * @param OutSettings	[out] Current allocator settings
	 */
	void	GetSettings( FSettings& OutSettings )
	{
		OutSettings = Settings;
	}

	/**
	 * Sets new allocator settings.
	 *
	 * @param InSettings	New allocator settings to replace the old ones.
	 */
	void	SetSettings( const FSettings& InSettings )
	{
		Settings = InSettings;

		// Only enable async reallocations if async defrag is also enabled.
		Settings.bEnableAsyncReallocation = InSettings.bEnableAsyncReallocation && InSettings.bEnableAsyncDefrag;
	}

	/**
	 * Returns whether allocator has been initialized.
	 */
	UBOOL	IsInitialized()
	{
		return MemoryBase != NULL;
	}

	/**
	 * Allocate physical memory.
	 *
	 * @param	AllocationSize	Size of allocation
	 * @param	bAllowFailure	Whether to allow allocation failure or not
	 * @return	Pointer to allocated memory
	 */
	void*	Allocate( INT AllocationSize, UBOOL bAllowFailure );

	/**
	 * Frees allocation associated with the specified pointer.
	 *
	 * @param Pointer		Pointer to free.
	 */
	void	Free( void* Pointer );

	/**
	 * Locks an FMemoryChunk
	 *
	 * @param Pointer		Pointer indicating which chunk to lock
	 */
	void	Lock( const void* Pointer );

	/**
	 * Unlocks an FMemoryChunk
	 *
	 * @param Pointer		Pointer indicating which chunk to unlock
	 */
	void	Unlock( const void* Pointer );

	/**
	 * Sets the user payload for an FMemoryChunk
	 *
	 * @param Pointer		Pointer indicating a chunk
	 * @param UserPayload	User payload to set
	 */
	void	SetUserPayload( const void* Pointer, PTRINT UserPayload );

	/**
	 * Returns the user payload for an FMemoryChunk
	 *
	 * @param Pointer		Pointer indicating a chunk
	 * return				The chunk's user payload
	 */
	PTRINT	GetUserPayload( const void* Pointer );

	/**
	 * Returns the amount of memory allocated for the specified address.
	 *
	 * @param Pointer		Pointer to check.
	 * @return				Number of bytes allocated
	 */
	INT		GetAllocatedSize( void* Pointer );

	/**
	 * Tries to reallocate texture memory in-place (without relocating),
	 * by adjusting the base address of the allocation but keeping the end address the same.
	 * Note: Newly freed memory due to shrinking won't be available for allocation right away (need GPU sync).
	 *
	 * @param	OldBaseAddress	Pointer to the original allocation
	 * @return	New base address if it succeeded, otherwise NULL
	 **/
	void*	Reallocate( void* OldBaseAddress, INT NewSize );

	/**
	 * Requests an async allocation or reallocation.
	 * The caller must hold on to the request until it has been completed or canceled.
	 *
	 * @param ReallocationRequest	The request
	 * @param bForceRequest			If TRUE, the request will be accepted even if there's currently not enough free space
	 * @return						TRUE if the request was accepted
	 */
	UBOOL	AsyncReallocate( FAsyncReallocationRequest* ReallocationRequest, UBOOL bForceRequest );

	/**
	 * Dump allocation information.
	 */
	void	DumpAllocs( FOutputDevice& Ar=*GLog );

	/**
	 * Retrieves allocation stats.
	 *
	 * @param	OutAllocatedMemorySize		[out] Size of allocated memory
	 * @param	OutAvailableMemorySize		[out] Size of available memory
	 * @param	OutPendingMemoryAdjustment	[out] Size of pending allocation change (due to async reallocation)
	 */
	void	GetMemoryStats( INT& OutAllocatedMemorySize, INT& OutAvailableMemorySize, INT& OutPendingMemoryAdjustment )
	{
		OutAllocatedMemorySize = AllocatedMemorySize;
		OutAvailableMemorySize = AvailableMemorySize;
		OutPendingMemoryAdjustment = PendingMemoryAdjustment;
	}

	/**
	 * Scans the free chunks and returns the largest size you can allocate.
	 *
	 * @param OutNumFreeChunks	Upon return, contains the total number of free chunks. May be NULL.
	 * @return					The largest size of all free chunks.
	 */
	INT		GetLargestAvailableAllocation( INT* OutNumFreeChunks=NULL );

	/**
	 * Returns the amount of time blocked by a platform fence since the beginning of the last call to Tick(), in appCycles.
	 */
	DWORD	GetBlockedCycles() const
	{
		return BlockedCycles;
	}

	/**
	 * Returns whether we're in benchmark mode or not.
	 */
	UBOOL	InBenchmarkMode() const
	{
		return bBenchmarkMode;
	}

	/**
	 * Fills a texture with to visualize the texture pool memory.
	 *
	 * @param	TextureData		Start address
	 * @param	SizeX			Number of pixels along X
	 * @param	SizeY			Number of pixels along Y
	 * @param	Pitch			Number of bytes between each row
	 * @param	PixelSize		Number of bytes each pixel represents
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	UBOOL	GetTextureMemoryVisualizeData( FColor* TextureData, INT SizeX, INT SizeY, INT Pitch, const INT PixelSize );

	void	GetMemoryLayout( TArray<FMemoryLayoutElement>& MemoryLayout );

	EMemoryElementType	GetChunkType( FMemoryChunk* Chunk ) const;

	/**
	 * Fully defragments the memory and blocks until it's done.
	 *
	 * @param Stats			[out] Stats
	 */
	void	DefragmentMemory( FRelocationStats& Stats );

	/**
	 * Partially defragments the memory and tries to process all async reallocation requests at the same time.
	 * Call this once per frame.
	 *
	 * @param Stats			[out] Stats
	 * @param bPanicDefrag	If TRUE, performs a full defrag and ignores all reallocation requests
	 */
	INT		Tick( FRelocationStats& Stats, UBOOL bPanicDefrag );

	/**
	 * Blocks the calling thread until all relocations and reallocations that were initiated by Tick() have completed.
	 *
	 * @return		TRUE if there were any relocations in progress before this call
	 */
	UBOOL	FinishAllRelocations();

	/**
	 * Blocks the calling thread until the specified request has been completed.
	 *
	 * @param Request	Request to wait for. Must be a valid request.
	 */
	void	BlockOnAsyncReallocation( FAsyncReallocationRequest* Request );

	/**
	 * Cancels the specified reallocation request.
	 * Note that the allocator doesn't keep track of requests after it's been completed,
	 * so the user must provide the current base address. This may not match any of the
	 * addresses in the (old) request since the memory may have been relocated since then.
	 *
	 * @param Request				Request to cancel. Must be a valid request.
	 * @param CurrentBaseAddress	Current baseaddress used by the allocation.
	 */
	void	CancelAsyncReallocation( FAsyncReallocationRequest* Request, const void* CurrentBaseAddress );

	/**
	 * Performs a benchmark of the allocator and outputs the result to the log.
	 *
	 * @param MinChunkSize	Minimum number of bytes per random chunk
	 * @param MaxChunkSize	Maximum number of bytes per random chunk
	 * @param FreeRatio		Free 0.0-1.0 of the memory before benchmarking
	 * @param LockRatio		Lock 0.0-1.0 % of the memory before benchmarking
	 * @param bFullDefrag	Whether to test full defrag (TRUE) or continuous defrag (FALSE)
	 * @param bSaveImages	Whether to save before/after images to hard disk (TexturePoolBenchmark-*.bmp)
	 * @param Filename		[opt] Filename to a previously saved memory layout to use for benchmarking, or NULL
	 */
	void	Benchmark( INT MinChunkSize, INT MaxChunkSize, FLOAT FreeRatio, FLOAT LockRatio, UBOOL bFullDefrag, UBOOL bSaveImages, const TCHAR* Filename );

protected:

	/**
	 * Copy memory from one location to another. If it returns FALSE, the defragmentation
	 * process will assume the memory is not relocatable and keep it in place.
	 * Note: Source and destination may overlap.
	 *
	 * @param Dest			Destination memory start address
	 * @param Source		Source memory start address
	 * @param Size			Number of bytes to copy
	 * @param UserPayload	User payload for this allocation
	 */
	virtual void	PlatformRelocate( void* Dest, const void* Source, INT Size, PTRINT UserPayload ) = 0;

	/**
	 * Inserts a fence to synchronize relocations.
	 * The fence can be blocked on at a later time to ensure that all relocations initiated
	 * so far has been fully completed.
	 *
	 * @return		New fence value
	 */
	virtual QWORD	PlatformInsertFence() = 0;

	/**
	 * Blocks the calling thread until all relocations initiated before the fence
	 * was added has been fully completed.
	 *
	 * @param Fence		Fence to block on
	 */
	virtual void	PlatformBlockOnFence( QWORD Fence ) = 0;

	/**
	 * Allows each platform to decide whether an allocation can be relocated at this time.
	 *
	 * @param Source		Base address of the allocation
	 * @param UserPayload	User payload for the allocation
	 * @return				TRUE if the allocation can be relocated at this time
	 */
	virtual UBOOL	PlatformCanRelocate( const void* Source, PTRINT UserPayload ) const = 0;

	/**
	 * Notifies the platform that an async reallocation request has been completed.
	 *
	 * @param FinishedRequest	The request that got completed
	 * @param UserPayload		User payload for the allocation
	 */
	virtual void	PlatformNotifyReallocationFinished( FAsyncReallocationRequest* FinishedRequest, PTRINT UserPayload ) = 0;

	/**
	 * Copy memory from one location to another. If it returns FALSE, the defragmentation
	 * process will assume the memory is not relocatable and keep it in place.
	 * Note: Source and destination may overlap.
	 *
	 * @param Stats			[out] Stats
	 * @param Dest			Destination memory chunk
	 * @param DestOffset	Destination offset, counted from the base address of the destination memory chunk, in bytes
	 * @param Source		Base address of the source memory
	 * @param Size			Number of bytes to copy
	 * @param UserPayload	User payload for the allocation
	 */
	void			Relocate( FRelocationStats& Stats, FMemoryChunk* Dest, INT DestOffset, const void* Source, INT Size, PTRINT UserPayload  )
	{
		if ( !bBenchmarkMode )
		{
			PlatformRelocate( Dest->Base + DestOffset, Source, Size, UserPayload );
		}
		Dest->UserPayload = UserPayload;
		Stats.NumBytesRelocated += Size;
		Stats.NumRelocations++;
	}

	/**
	 * Returns the sync index to be completed by the next call to FinishAllRelocations().
	 */
	DWORD			GetCurrentSyncIndex() const
	{
		return CurrentSyncIndex;
	}

	/**
	 * Performs a partial defrag while trying to process any pending async reallocation requests.
	 *
	 * @param Stats			[out] Stats
	 * @param StartTime		Start time, used for limiting the Tick() time
	 */
	void			PartialDefragmentation( FRelocationStats& Stats, DOUBLE StartTime );

	/**
	 * Performs a partial defrag by shifting down memory to fill holes, in a brute-force manner.
	 * Takes consideration to async reallocations, but processes the all memory in order.
	 *
	 * @param Stats			[out] Stats
	 * @param StartTime		Start time, used for limiting the Tick() time
	 */
	void			PartialDefragmentationDownshift( FRelocationStats& Stats, DOUBLE StartTime );

	/**
	 * Performs a full defrag and ignores all reallocation requests.
	 *
	 * @param Stats			[out] Stats
	 */
	void			FullDefragmentation( FRelocationStats& Stats );

	/**
	 * Tries to immediately grow a memory chunk by moving the base address, without relocating any memory.
	 *
	 * @param Chunk			Chunk to grow
	 * @param GrowAmount	Number of bytes to grow by
	 * @return				NULL if it failed, otherwise the new grown chunk
	 */
	FMemoryChunk*	Grow( FMemoryChunk* Chunk, INT GrowAmount );

	/**
	 * Immediately shrinks a memory chunk by moving the base address, without relocating any memory.
	 * Always succeeds.
	 *
	 * @param Chunk			Chunk to shrink
	 * @param ShrinkAmount	Number of bytes to shrink by
	 * @return				The new shrunken chunk
	 */
	FMemoryChunk*	Shrink( FMemoryChunk* Chunk, INT ShrinkAmount );

	/**
	 * Checks the internal state for errors. (Slow)
	 *
	 * @param bCheckSortedFreeList	If TRUE, also checks that the freelist is sorted
	 */
	void			CheckForErrors( UBOOL bCheckSortedFreeList );

	/**
	 * Returns TRUE if the specified chunk is allowed to relocate at this time.
	 * Will also call PlatformCanRelocate().
	 *
	 * @param Chunk		Chunk to check
	 * @return			TRUE if the allocation can be relocated at this time
	 */
	UBOOL			CanRelocate( const FMemoryChunk* Chunk ) const;

	/**
	 * Inserts a platform fence and updates the allocator sync index to match.
	 */
	void			InsertFence();

	/**
	 * Blocks the calling thread until the current sync fence has been completed.
	 */
	void			BlockOnFence();

	/**
	 * Blocks the calling thread until the specified sync index has been completed.
	 *
	 * @param SyncIndex		Sync index to wait for
	 */
	void			BlockOnSyncIndex( DWORD SyncIndex );

	/**
	 * Split allocation into two, first chunk being used and second being available.
	 * Maintains the free-list order if bSortedFreeList is TRUE.
	 *
	 * @param BaseChunk			Chunk to split
	 * @param FirstSize			New size of first chunk
	 * @param bSortedFreeList	If TRUE, maintains the free-list order
	 */
	void			Split( FMemoryChunk* BaseChunk, INT FirstSize, UBOOL bSortedFreeList )
	{
		check( BaseChunk );
		check( FirstSize < BaseChunk->Size );
		check( FirstSize > 0 );

		// Don't make any assumptions on the following chunk. Because Reallocate() will make the 1st chunk free
		// and the 2nd chunk used, so it's ok to have the following chunk free. Note, this only happens when Reallocate()
		// is splitting the very first chunk in the pool.
		// Don't make any assumptions for the previous chunk either...
// 		check( !BaseChunk->NextChunk || !BaseChunk->NextChunk->bIsAvailable );
// 		check( !BaseChunk->PreviousChunk || !BaseChunk->PreviousChunk->bIsAvailable || !BaseChunk->bIsAvailable );

		// Calculate size of second chunk...
		INT SecondSize = BaseChunk->Size - FirstSize;
		// ... and create it.
		FMemoryChunk* NewFreeChunk = new FMemoryChunk( BaseChunk->Base + FirstSize, SecondSize, *this, BaseChunk, /*FirstChunk, FirstFreeChunk,*/ bSortedFreeList );

		// Keep the original sync index for the new chunk if necessary.
		if ( BaseChunk->IsRelocating() && BaseChunk->SyncSize > FirstSize )
		{
			INT SecondSyncSize = BaseChunk->SyncSize - FirstSize;
			NewFreeChunk->SetSyncIndex( BaseChunk->SyncIndex, SecondSyncSize );
		}

		// Resize base chunk.
		BaseChunk->Size = FirstSize;
	}

	/**
	 * Marks the specified chunk as 'allocated' and updates tracking variables.
	 * Splits the chunk if only a portion of it is allocated.
	 *
	 * @param FreeChunk			Chunk to allocate
	 * @param AllocationSize	Number of bytes to allocate
	 * @param bAsync			If TRUE, allows allocating from relocating chunks and maintains the free-list sort order.
	 * @return					The memory chunk that was allocated (the original chunk could've been split).
	 */
	FMemoryChunk*	AllocateChunk( FMemoryChunk* FreeChunk, INT AllocationSize, UBOOL bAsync );

	/**
	 * Marks the specified chunk as 'free' and updates tracking variables.
	 * Calls LinkFreeChunk() to coalesce adjacent free memory.
	 *
	 * @param Chunk						Chunk to free
	 * @param bMaintainSortedFreelist	If TRUE, maintains the free-list sort order
	 */
	void			FreeChunk( FMemoryChunk* Chunk, UBOOL bMaintainSortedFreelist );

	/**
	 * Frees the passed in chunk and coalesces adjacent free chunks into 'Chunk' if possible.
	 * Maintains the free-list order if bSortedFreeList is TRUE.
	 *
	 * @param Chunk				Chunk to mark as available. 
	 * @param bSortedFreeList	If TRUE, maintains the free-list sort order
	 */
	void			LinkFreeChunk( FMemoryChunk* Chunk, UBOOL bSortedFreeList )
	{
		check(Chunk);
		// Mark chunk as available.
		Chunk->LinkFree( bSortedFreeList, NULL );
		// Kick of merge pass.
		Coalesce( Chunk );
	}

	/**
	 * Merges any adjacent free chunks into the specified free chunk.
	 * Doesn't affect the free-list sort order.
	 *
	 * @param FreedChunk	Chunk that just became available.
	 */
	void			Coalesce( FMemoryChunk* FreedChunk );

	/**
	 * Sorts the freelist based on increasing base address.
	 *
	 * @param NumFreeChunks		[out] Number of free chunks
	 * @param LargestFreeChunk	[out] Size of the largest free chunk, in bytes
	 */
	void			SortFreeList( INT& NumFreeChunks, INT& LargestFreeChunk );

	/**
	 * Defrag helper function. Checks if the specified allocation fits within
	 * the adjacent free chunk(s), accounting for potential reallocation request.
	 *
	 * @param UsedChunk		Allocated chunk to check for a fit
	 * @param bAnyChunkType	If FALSE, only succeeds if 'UsedChunk' has a reallocation request and fits
	 * @return				Returns 'UsedChunk' if it fits the criteria, otherwise NULL
	 */
	FMemoryChunk*	FindAdjacent( FMemoryChunk* UsedChunk, UBOOL bAnyChunkType );

	/**
	 * Searches for a reallocation request that would fit within the specified free chunk.
	 *
	 * @param FreeChunk		Free chunk we're trying to fill up
	 * @return				First request that could fit, or NULL
	 */
	FRequestNode*	FindAnyReallocation( FMemoryChunk* FreeChunk );

	/**
	 * Searches for an allocated chunk that would fit within the specified free chunk.
	 * The allocated chunk must be adjacent to a free chunk and have a larger
	 * base address than 'FreeChunk'.
	 * Starts searching from the end of the texture pool.
	 *
	 * @param FreeChunk		Free chunk we're trying to fill up
	 * @return				Pointer to a suitable chunk, or NULL
	 */
	FMemoryChunk*	FindAdjacentToHole( FMemoryChunk* FreeChunk );

	/**
	 * Searches for an allocated chunk that would fit within the specified free chunk.
	 * Any chunk that fits and has a larger base address than 'FreeChunk' is accepted.
	 * Starts searching from the end of the texture pool.
	 *
	 * @param FreeChunk		Free chunk we're trying to fill up
	 * @return				Pointer to a suitable chunk, or NULL
	 */
	FMemoryChunk*	FindAny( FMemoryChunk* FreeChunk );

	/**
	 * Initiates an async relocation of an allocated chunk into a free chunk.
	 * Takes potential reallocation request into account.
	 *
	 * @param Stats			[out] Stats
	 * @param FreeChunk		Destination chunk (free memory)
	 * @param UsedChunk		Source chunk (allocated memory)
	 * @return				Next Free chunk to try to fill up
	 */
	FMemoryChunk*	RelocateIntoFreeChunk( FRelocationStats& Stats, FMemoryChunk* FreeChunk, FMemoryChunk* UsedChunk );

	/**
	 * Allocates memory from the specified free chunk, servicing an async allocation request.
	 *
	 * @param Stats			[out] Stats
	 * @param FreeChunk		Chunk to allocate memory from
	 * @param RequestNode	List node to the allocation request
	 * @return				Next Free chunk to try to fill up
	 */
	FMemoryChunk*	AllocateIntoFreeChunk( FRelocationStats& Stats, FMemoryChunk* FreeChunk, FRequestNode* RequestNode );

	/** Total size of memory pool, in bytes.						*/
	INT				MemorySize;
	/** Base of memory pool.										*/
	BYTE*			MemoryBase;
	/** Allocation alignment requirements.							*/
	INT				AllocationAlignment;
	/** Head of linked list of chunks. Sorted by memory address.	*/
	FMemoryChunk*	FirstChunk;
	/** Last chunk in the linked list of chunks (see FirstChunk).	*/
	FMemoryChunk*	LastChunk;
	/** Head of linked list of free chunks.	Unsorted.				*/
	FMemoryChunk*	FirstFreeChunk;
	/** Cumulative time spent in allocator.							*/
	DOUBLE			TimeSpentInAllocator;
	/** Allocated memory in bytes.									*/
	volatile INT	AllocatedMemorySize;
	/** Available memory in bytes.									*/
	volatile INT	AvailableMemorySize;
	/** Adjustment to allocated memory, pending all reallocations.	*/
	volatile INT	PendingMemoryAdjustment;
	/** Mapping from pointer to chunk for fast removal.				*/
	TMap<PTRINT,FMemoryChunk*>	PointerToChunkMap;

	/** Allocator settings that affect its behavior.				*/
	FSettings		Settings;

	/** Ever-increasing index to synchronize all relocations initiated by Tick(). */
	DWORD			CurrentSyncIndex;
	/** Sync index that has been completed, so far.					*/
	DWORD			CompletedSyncIndex;

	/** Number of async relocations that are currently in progress.	*/
	INT				NumRelocationsInProgress;
	/** Platform-specific (GPU) fence, used for synchronizing the Sync Index. */
	QWORD			PlatformSyncFence;

	/** Asynchronous reallocations requested by user.				*/
	FRequestList	ReallocationRequests;

	/** Asynchronous reallocations currently being processed.		*/
	FRequestList	ReallocationRequestsInProgress;

	/** Chunks that couldn't be freed immediately because they were being relocated. */
	TDoubleLinkedList<FMemoryChunk*>	PendingFreeChunks;

	// Stats
	/** Total number of relocations performed so far.				*/
	QWORD			TotalNumRelocations;
	/** Total number of bytes relocated so far.						*/
	QWORD			TotalNumBytesRelocated;
	/** Maximum number of disjoint free memory regions we've had.	*/
	INT				MaxNumHoles;
	/** Smallest consecutive free memory region we've had.			*/
	INT				MinLargestHole;
	/** Total number of async reallocations successfully completed so far.	*/
	INT				NumFinishedAsyncReallocations;
	/** Total number of async allocations successfully completed so far.	*/
	INT				NumFinishedAsyncAllocations;
	/** Total number of async requests that has been canceled so far.		*/
	INT				NumCanceledAsyncRequests;
	/** Amount of time blocked by a platform fence since the beginning of the last call to Tick(), in appCycles.	*/
	DWORD			BlockedCycles;

	/** When in benchmark mode, don't call any Platform functions.	*/
	UBOOL			bBenchmarkMode;
};


/**
 * Wrapper class around a best fit allocator that handles running out of memory without
 * returning NULL, but instead returns a preset pointer and marks itself as "corrupt".
 * THREAD-SAFE.
 */
struct FPresizedMemoryPool : private FBestFitAllocator
{
	/**
	 * Constructor, initializes the BestFitAllocator (will allocate physical memory!)
	 * 
	 * @param PoolSize Size of the memory pool
	 * @param Alignment Default alignment for each allocation
	 */
	FPresizedMemoryPool(INT PoolSize, INT Alignment)
	:	bIsCorrupted(FALSE)
	,	AllocationFailurePointer(NULL)
	,	PhysicalMemoryBase(NULL)
	,	PhysicalMemorySize(0)
	,	TickCycles(0)
	{
		// Don't add any extra bytes when allocating, so we don't cause unnecessary fragmentation.
		AllocationFailurePointer = (BYTE*)appPhysicalAlloc(PoolSize, CACHE_WriteCombine);

		// Initialize the pool slightly in, so we can distinguish between a real and a failed allocation when we are freeing memory.
		BYTE* PoolMemory = AllocationFailurePointer + Alignment;
		FBestFitAllocator::Initialize( PoolMemory, PoolSize - Alignment, Alignment );
	}

	/**
	 * Constructor, initializes the BestFitAllocator with already allocated memory
	 * 
	 * @param PoolSize Size of the memory pool
	 * @param Alignment Default alignment for each allocation
	 */
	FPresizedMemoryPool(BYTE* PoolMemory, BYTE* FailedAllocationMemory, INT PoolSize, INT Alignment)
	:	bIsCorrupted(FALSE)
	,	AllocationFailurePointer(FailedAllocationMemory)
	,	PhysicalMemoryBase(NULL)
	,	PhysicalMemorySize(0)
	,	TickCycles(0)
	{
		// Initialize allocator.
		FBestFitAllocator::Initialize(PoolMemory, PoolSize, Alignment );
	}

	/**
	 * Default constructor, to initialize the BestFitAllocator at a later time
	 * 
	 * @param PoolSize Size of the memory pool
	 * @param Alignment Default alignment for each allocation
	 */
	FPresizedMemoryPool()
	:	bIsCorrupted(FALSE)
	,	AllocationFailurePointer(NULL)
	,	PhysicalMemoryBase(NULL)
	,	PhysicalMemorySize(0)
	,	TickCycles(0)
	{
	}

	/**
	 * Returns whether allocator has been initialized or not.
	 */
	UBOOL	IsInitialized()
	{
		return FBestFitAllocator::IsInitialized();
	}

	/**
	 * Returns the current allocator settings.
	 *
	 * @param OutSettings	[out] Current allocator settings
	 */
	void	GetSettings( FSettings& OutSettings )
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		FBestFitAllocator::GetSettings( OutSettings );
	}

	/**
	 * Sets new allocator settings.
	 *
	 * @param InSettings	New allocator settings to replace the old ones.
	 */
	void	SetSettings( const FSettings& InSettings )
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		FBestFitAllocator::SetSettings( InSettings );
	}

	/**
	 * Initializes the BestFitAllocator (will allocate physical memory!)
	 * 
	 * @param PoolSize Size of the memory pool
	 * @param Alignment Default alignment for each allocation
	 * @param TailSlack Unused space to leave at the end of the pool for potential overfetch
	 * @return TRUE if the initialization succeeded or FALSE if it was already initialized.
	 */
	UBOOL	Initialize(INT PoolSize, INT Alignment, INT TailSlack = 0)
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		if ( !IsInitialized() )
		{
			// Don't add any extra bytes when allocating, so we don't cause unnecessary fragmentation.
			PhysicalMemorySize = PoolSize;
			PhysicalMemoryBase = appPhysicalAlloc(PhysicalMemorySize, CACHE_WriteCombine);

			AllocationFailurePointer = (BYTE*) PhysicalMemoryBase;

			// Initialize the pool slightly in, so we can distinguish between a real and a failed allocation when we are freeing memory.
			BYTE* PoolMemory = AllocationFailurePointer + Alignment;
			FBestFitAllocator::Initialize( PoolMemory, PoolSize - Alignment - TailSlack, Alignment );
			return TRUE;
		}
		return FALSE;
	}

	/**
	 * Initializes the BestFitAllocator with already allocated memory
	 * 
	 * @param PoolSize Size of the memory pool
	 * @param Alignment Default alignment for each allocation
	 * @return TRUE if the initialization succeeded or FALSE if it was already initialized.
	 */
	UBOOL	Initialize(BYTE* PoolMemory, BYTE* FailedAllocationMemory, INT PoolSize, INT Alignment)
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		if ( !IsInitialized() )
		{
			AllocationFailurePointer = FailedAllocationMemory;
			FBestFitAllocator::Initialize( PoolMemory, PoolSize, Alignment );
			return TRUE;
		}
		return FALSE;
	}

	/**
	 * Determines whether this pointer resides within the texture memory pool.
	 *
	 * @param	Pointer		Pointer to check
	 * @return				TRUE if the pointer resides within the texture memory pool, otherwise FALSE.
	 */
	UBOOL IsTextureMemory( const void* Pointer )
	{
		UBOOL bIsWithinTexturePool = ( PTRINT(Pointer) >= PTRINT(MemoryBase) && PTRINT(Pointer) < (PTRINT(MemoryBase) + PTRINT(MemorySize)) ) || Pointer == AllocationFailurePointer;
		return bIsWithinTexturePool;
	}

	/**
	 * Checks whether the pointer represents valid texture data.
	 *
	 * @param	Pointer		Baseaddress of the texture data to check
	 * @return				TRUE if it points to valid texture data
	 */
	UBOOL	IsValidTextureData( const void* Pointer )
	{
		return Pointer && Pointer != AllocationFailurePointer && IsTextureMemory(Pointer);
	}

	/**
	 * Allocates texture memory.
	 *
	 * @param	Size			Size of allocation
	 * @param	bAllowFailure	Whether to allow allocation failure or not
	 * @returns					Pointer to allocated memory
	 */
	void*	Allocate(DWORD Size, UBOOL bAllowFailure);

	/**
	 * Frees texture memory allocated via Allocate
	 *
	 * @param	Pointer		Allocation to free
	 */
	void	Free(void* Pointer);

	/**
	 * Locks an FMemoryChunk
	 *
	 * @param Pointer		Pointer indicating which chunk to lock
	 */
	void	Lock( const void* Pointer )
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		if ( IsValidTextureData(Pointer) )
		{
			FBestFitAllocator::Lock( Pointer );
		}
	}

	/**
	 * Unlocks an FMemoryChunk
	 *
	 * @param Pointer		Pointer indicating which chunk to unlock
	 */
	void	Unlock( const void* Pointer )
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		if ( IsValidTextureData(Pointer) )
		{
			FBestFitAllocator::Unlock( Pointer );
		}
	}

	/**
	 * Sets the user payload for an FMemoryChunk
	 *
	 * @param Pointer		Pointer indicating a chunk
	 * @param UserPayload	User payload to set
	 */
	void	SetUserPayload( const void* Pointer, PTRINT UserPayload )
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		FBestFitAllocator::SetUserPayload( Pointer, UserPayload );
	}

	/**
	 * Returns the user payload for an FMemoryChunk
	 *
	 * @param Pointer		Pointer indicating a chunk
	 * return				The chunk's user payload
	 */
	PTRINT	GetUserPayload( const void* Pointer )
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		return FBestFitAllocator::GetUserPayload( Pointer );
	}

	/**
	 * Returns the amount of memory allocated for the specified address.
	 *
	 * @param	Pointer		Pointer to check.
	 * @return				Number of bytes allocated
	 */
	INT		GetAllocatedSize( void* Pointer );

	/**
	 * Tries to reallocate texture memory in-place (without relocating),
	 * by adjusting the base address of the allocation but keeping the end address the same.
	 *
	 * @param	OldBaseAddress	Pointer to the original allocation
	 * @returns	New base address if it succeeded, otherwise NULL
	 **/
	void*	Reallocate( void* OldBaseAddress, INT NewSize );

	/**
	 * Requests an async allocation or reallocation.
	 * The caller must hold on to the request until it has been completed or canceled.
	 *
	 * @param ReallocationRequest	The request
	 * @param bForceRequest			If TRUE, the request will be accepted even if there's currently not enough free space
	 * @return						TRUE if the request was accepted
	 */
	UBOOL	AsyncReallocate( FAsyncReallocationRequest* ReallocationRequest, UBOOL bForceRequest );

	/**
	 * Partially defragments the memory and tries to process all async reallocation requests at the same time.
	 * Call this once per frame.
	 *
	 * @param Stats			[out] Stats
	 */
	INT		Tick( FRelocationStats& Stats );

	/**
	 * Blocks until all relocations and reallocations that were initiated by Tick() have completed.
	 *
	 * @return		TRUE if there were any relocations in progress before this call
	 */
	UBOOL	FinishAllRelocations()
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		return FBestFitAllocator::FinishAllRelocations();
	}

	/**
	 * Blocks until the GPU is idle and all relocations and reallocations are done.
	 */
	void	ForceSync()
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		InsertFence();
		BlockOnFence();
		FinishAllRelocations();
	}

	/**
	 * Blocks the calling thread until the specified request has been completed.
	 *
	 * @param Request	Request to wait for. Must be a valid request.
	 */
	void	BlockOnAsyncReallocation( FAsyncReallocationRequest* Request )
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		return FBestFitAllocator::BlockOnAsyncReallocation( Request );
	}

	/**
	 * Cancels the specified reallocation request.
	 * Note that the allocator doesn't keep track of requests after it's been completed,
	 * so the user must provide the current base address. This may not match any of the
	 * addresses in the (old) request since the memory may have been relocated since then.
	 *
	 * @param Request				Request to cancel. Must be a valid request.
	 * @param CurrentBaseAddress	Current baseaddress used by the allocation.
	 */
	void	CancelAsyncReallocation( FAsyncReallocationRequest* Request, const void* CurrentBaseAddress )
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		return FBestFitAllocator::CancelAsyncReallocation( Request, CurrentBaseAddress );
	}

	/**
	 * Retrieves allocation stats.
	 *
	 * @param	OutAllocatedMemorySize		[out] Size of allocated memory
	 * @param	OutAvailableMemorySize		[out] Size of available memory
	 * @param	OutPendingMemoryAdjustment	[out] Size of pending allocation change (due to async reallocation)
	 */
	void	GetMemoryStats( INT& OutAllocatedMemorySize, INT& OutAvailableMemorySize, INT& OutPendingMemoryAdjustment )
	{
		FBestFitAllocator::GetMemoryStats(OutAllocatedMemorySize, OutAvailableMemorySize, OutPendingMemoryAdjustment);
	}

	/**
	 * Fills a texture with to visualize the texture pool memory.
	 *
	 * @param	TextureData		Start address
	 * @param	SizeX			Number of pixels along X
	 * @param	SizeY			Number of pixels along Y
	 * @param	Pitch			Number of bytes between each row
	 * @param	PixelSize		Number of bytes each pixel represents
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	UBOOL	GetTextureMemoryVisualizeData( FColor* TextureData, INT SizeX, INT SizeY, INT Pitch, INT PixelSize )
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		return FBestFitAllocator::GetTextureMemoryVisualizeData( TextureData, SizeX, SizeY, Pitch, PixelSize );
	}

	void	GetMemoryLayout( TArray<FMemoryLayoutElement>& MemoryLayout )
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		return FBestFitAllocator::GetMemoryLayout( MemoryLayout );
	}

	/**
	 * Scans the free chunks and returns the largest size you can allocate.
	 *
	 * @param OutNumFreeChunks	Upon return, contains the total number of free chunks. May be NULL.
	 * @return					The largest size of all free chunks.
	 */
	INT		GetLargestAvailableAllocation( INT* OutNumFreeChunks=NULL )
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		return FBestFitAllocator::GetLargestAvailableAllocation( OutNumFreeChunks );
	}

	/**
	 * Returns the amount of time spent in the last call to Tick(), in appCycles.
	 */
	DWORD	GetTickCycles() const
	{
		return TickCycles;
	}

	/**
	 * Returns the amount of time blocked by a platform fence since the beginning of the last call to Tick(), in appCycles.
	 */
	DWORD	GetBlockedCycles() const
	{
		return FBestFitAllocator::GetBlockedCycles();
	}

	/**
	 * Returns whether we're in benchmark mode or not.
	 */
	UBOOL	InBenchmarkMode() const
	{
		return FBestFitAllocator::InBenchmarkMode();
	}

	/**
	 * Fully defragments the memory and blocks until it's done.
	 *
	 * @param Stats			[out] Stats
	 */
	void	DefragmentMemory( FRelocationStats& Stats )
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		FBestFitAllocator::DefragmentMemory( Stats );
	}

	/**
	 * Performs a benchmark of the allocator and outputs the result to the log.
	 *
	 * @param MinChunkSize	Minimum number of bytes per random chunk
	 * @param MaxChunkSize	Maximum number of bytes per random chunk
	 * @param FreeRatio		Free 0.0-1.0 of the memory before benchmarking
	 * @param LockRatio		Lock 0.0-1.0 % of the memory before benchmarking
	 * @param bFullDefrag	Whether to test full defrag (TRUE) or continuous defrag (FALSE)
	 * @param bSaveImages	Whether to save before/after images to hard disk (TexturePoolBenchmark-*.bmp)
	 * @param Filename		[opt] Filename to a previously saved memory layout to use for benchmarking, or NULL
	 */
	void	Benchmark( INT MinChunkSize, INT MaxChunkSize, FLOAT FreeRatio, FLOAT LockRatio, UBOOL bFullDefrag, UBOOL bSaveImages, const TCHAR* Filename )
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		FBestFitAllocator::Benchmark( MinChunkSize, MaxChunkSize, FreeRatio, LockRatio, bFullDefrag, bSaveImages, Filename );
	}

	/** Object used for synchronization via a scoped lock */
	FCriticalSection	SynchronizationObject;

	/** TRUE if we have run out of memory in the pool (and therefore returned AllocationFailurePointer) */
	UBOOL		bIsCorrupted;

	/** Single pointer to return when an allocation fails */
	BYTE*		AllocationFailurePointer;

	/** Base address of the physical memory */
	void*		PhysicalMemoryBase;

	/** Size of the physical memory */
	DWORD		PhysicalMemorySize;

	/** Time spent in the last call to Tick(), in appCycles. */
	DWORD		TickCycles;

	// Propagate these data types.
	typedef FBestFitAllocator::FSettings FSettings;
	typedef FBestFitAllocator::FRelocationStats FRelocationStats;
	typedef FBestFitAllocator::EMemoryElementType EMemoryElementType;
	typedef FBestFitAllocator::FMemoryLayoutElement FMemoryLayoutElement;
};

/**
 * Asynchronous reallocation request.
 * Requests are created and deleted by the user, but it must stick around until the allocator is done with it.
 * Requests may be fulfilled immediately, check HasCompleted() after making the request.
 */
class FAsyncReallocationRequest
{
public:
	/**
	 * Creates a new reallocation request.
	 *
	 * @param InCurrentBaseAddress	Current base address
	 * @param InNewSize				Requested new size, in bytes
	 * @param InRequestStatus		Will be decremented by one when the request has been completed. Can be NULL.
	 */
	FAsyncReallocationRequest( void* InCurrentBaseAddress, INT InNewSize, FThreadSafeCounter* InRequestStatus )
	:	OldAddress(InCurrentBaseAddress)
	,	NewAddress(NULL)
	,	OldSize(0)	// Set by AsyncReallocate()
	,	NewSize(InNewSize)
	,	InternalRequestStatus(1)
	,	ExternalRequestStatus(InRequestStatus)
	,	bIsCanceled(FALSE)
	,	MemoryChunk(NULL)
	{
	}

	/** Destructor. */
	~FAsyncReallocationRequest()
	{
		check( !HasStarted() || IsCanceled() || HasCompleted() );
	}

	/** Returns TRUE if the request is for a new allocation. */
	UBOOL	IsAllocation() const
	{
		return OldAddress == NULL && OldSize == 0;
	}

	/** Returns TRUE if the request is for a reallocation. */
	UBOOL	IsReallocation() const
	{
		return OldAddress != NULL;
	}

	/** Returns TRUE if the request has been canceled. */
	UBOOL	IsCanceled() const
	{
		return bIsCanceled;
	}

	/** Returns TRUE if the request has been completed. */
	UBOOL	HasCompleted() const
	{
		UBOOL bHasCompleted = InternalRequestStatus.GetValue() == 0;
		check( !bHasCompleted || NewAddress || bIsCanceled );
		return bHasCompleted;
	}

	/** Returns TRUE if the allocator has started processing the request (TRUE for completed requests as well). */
	UBOOL	HasStarted() const
	{
		return NewAddress ? TRUE : FALSE;
	}

	/** Returns the original base address. */
	void*	GetOldBaseAddress() const
	{
		return OldAddress;
	}

	/** Returns the new base address, or NULL if the request hasn't started yet. */
	void*	GetNewBaseAddress() const
	{
		return NewAddress;
	}

	/** Returns the requested new memory size (in bytes). */
	INT		GetNewSize() const
	{
		return NewSize;
	}

private:
	// Hidden on purpose since outside usage isn't necessarily thread-safe.
	FAsyncReallocationRequest( const FAsyncReallocationRequest& Other )	{ appMemcpy( this, &Other, sizeof(FAsyncReallocationRequest) ); }
	void operator=( const FAsyncReallocationRequest& Other  ) { appMemcpy( this, &Other, sizeof(FAsyncReallocationRequest) ); }

	/**
	 * Marks the request as completed. Also decrements the external request status, if it wasn't NULL.
	 */
	void	MarkCompleted()
	{
		check( InternalRequestStatus.GetValue() == 1 );
		InternalRequestStatus.Decrement();
		if ( ExternalRequestStatus )
		{
			ExternalRequestStatus->Decrement();
		}
	}

	/** Original base address. */
	void*				OldAddress;
	/** New base address, or NULL if the request hasn't started yet. */
	void*				NewAddress;
	/** Original memory size, in bytes. Set by AsyncReallocate(). */
	INT					OldSize;
	/** Requested new memory size, in bytes. */
	INT					NewSize;
	/** Thread-safe counter that will be decremented by one when the request has been completed. */
	FThreadSafeCounter	InternalRequestStatus;
	/** External counter that will be decremented by one when the request has been completed. */
	FThreadSafeCounter*	ExternalRequestStatus;
	/** TRUE if the request has been canceled. */
	DWORD				bIsCanceled : 1;

	/**
	 * Corresponding memory chunk. Starts out as the chunk that contains the original memory block,
	 * but is changed to the destination chunk once the allocator starts processing the request.
	 */
	class FBestFitAllocator::FMemoryChunk*		MemoryChunk;

	friend struct FBestFitAllocator;
};


/**
 *	Returns the current size (in bytes), or the final size if it has a reallocating request.
 */
FORCEINLINE INT FBestFitAllocator::FMemoryChunk::GetFinalSize() const
{
	return ReallocationRequestNode ? ReallocationRequestNode->GetValue()->GetNewSize() : Size;
}

/**
 * Returns TRUE if the specified chunk is allowed to relocate at this time.
 * Will also call PlatformCanRelocate().
 *
 * @param Chunk		Chunk to check
 * @return			TRUE if the allocation can be relocated at this time
 */
FORCEINLINE UBOOL FBestFitAllocator::CanRelocate( const FMemoryChunk* Chunk ) const
{
	// During reallocation, the new texture keeps the request around until it's been processed higher up.
	// Can't relocate a request that has started until it's fully gone from the system.
	const FRequestNode* RequestNode = Chunk->ReallocationRequestNode;
	if ( RequestNode && RequestNode->GetValue()->HasStarted() )
	{
		return FALSE;
	}

	if ( Chunk->bLocked )
	{
		return FALSE;
	}

	if ( !bBenchmarkMode )
	{
		return PlatformCanRelocate( Chunk->Base, Chunk->UserPayload );
	}
	else
	{
		return TRUE;
	}
}

/**
 * Blocks the calling thread until the specified request has been completed.
 *
 * @param Request	Request to wait for. Must be a valid request.
 */
FORCEINLINE void FBestFitAllocator::BlockOnAsyncReallocation( FAsyncReallocationRequest* Request )
{
	check( Request->HasStarted() );
	if ( !Request->HasCompleted() )
	{
		BlockOnSyncIndex( Request->MemoryChunk->SyncIndex );
	}
}

#endif
