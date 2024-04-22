/*=============================================================================
	BestFitAllocator.cpp: Unreal memory best fit allocator
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"
#include "BestFitAllocator.h"


DECLARE_STATS_GROUP(TEXT("TexturePool"),STATGROUP_TexturePool);

DECLARE_CYCLE_STAT(TEXT("Defragmentation"),STAT_TexturePool_DefragTime,STATGROUP_TexturePool);
DECLARE_CYCLE_STAT(TEXT("Blocked By GPU Relocation"),STAT_TexturePool_Blocked,STATGROUP_TexturePool);
DECLARE_MEMORY_STAT2(TEXT("Allocated"),STAT_TexturePool_Allocated,STATGROUP_TexturePool,MCR_TexturePool1,TRUE);
DECLARE_MEMORY_STAT2(TEXT("Free"),STAT_TexturePool_Free,STATGROUP_TexturePool,MCR_TexturePool1,TRUE);
DECLARE_MEMORY_STAT2_FAST(TEXT("Largest Hole"),STAT_TexturePool_LargestHole,STATGROUP_TexturePool,MCR_TexturePool1,TRUE);
DECLARE_MEMORY_STAT2(TEXT("Relocating Memory"),STAT_TexturePool_RelocatedSize,STATGROUP_TexturePool,MCR_TexturePool1,TRUE);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Relocations"),STAT_TexturePool_NumRelocations,STATGROUP_TexturePool);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Holes"),STAT_TexturePool_NumHoles,STATGROUP_TexturePool);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Async Reallocs"),STAT_TexturePool_TotalAsyncReallocations,STATGROUP_TexturePool);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Async Allocs"),STAT_TexturePool_TotalAsyncAllocations,STATGROUP_TexturePool);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Async Cancels"),STAT_TexturePool_TotalAsyncCancellations,STATGROUP_TexturePool);

#ifdef _DEBUG
	#define PARTIALDEFRAG_TIMELIMIT		(4.0/1000.0)	// 4 ms
#else
	#define PARTIALDEFRAG_TIMELIMIT		(2.0/1000.0)	// 2 ms
#endif

// Number of bytes when a chunk is considered "small", for defragmentation retry purposes.
#define DEFRAG_SMALL_CHUNK_SIZE			(16*1024-1)		// ~16 KB
// Number of defrags before trying a small chunk again (about 5-10 seconds at 30 fps). Must fit in FMemoryChunk::DefragCounter.
#define DEFRAG_SMALL_CHUNK_COUNTER_MIN	(5*30)
#define DEFRAG_SMALL_CHUNK_COUNTER_MAX	(10*30)
// Number of defrags before trying a chunk again (about 1-2 seconds at 30 fps).  Must fit in FMemoryChunk::DefragCounter.
#define DEFRAG_CHUNK_COUNTER_MIN		(20)
#define DEFRAG_CHUNK_COUNTER_MAX		(80)


/*-----------------------------------------------------------------------------
FBestFitAllocator::FMemoryChunk implementation.
-----------------------------------------------------------------------------*/

/**
 * Inserts this chunk at the head of the free chunk list.
 * If bMaintainSortOrder is TRUE, insert-sort this chunk into the free chunk list.
 */
void FBestFitAllocator::FMemoryChunk::LinkFree( UBOOL bMaintainSortOrder, FMemoryChunk* FirstFreeChunkToSearch )
{
	check(!bIsAvailable);
	bIsAvailable = TRUE;
	DefragCounter = 0;
	UserPayload = 0;

	if ( bMaintainSortOrder == FALSE )
	{
		if( BestFitAllocator.FirstFreeChunk )
		{
			NextFreeChunk		= BestFitAllocator.FirstFreeChunk;
			PreviousFreeChunk	= NULL;
			BestFitAllocator.FirstFreeChunk->PreviousFreeChunk = this;
			BestFitAllocator.FirstFreeChunk		= this;
		}
		else
		{
			PreviousFreeChunk	= NULL;
			NextFreeChunk		= NULL;
			BestFitAllocator.FirstFreeChunk		= this;
		}
	}
	else
	{
		if( BestFitAllocator.FirstFreeChunk )
		{
			FMemoryChunk* InsertBefore = (FirstFreeChunkToSearch && FirstFreeChunkToSearch->bIsAvailable) ? FirstFreeChunkToSearch : BestFitAllocator.FirstFreeChunk;
			while ( Base > InsertBefore->Base && InsertBefore->NextFreeChunk )
			{
				InsertBefore = InsertBefore->NextFreeChunk;
			}
			NextFreeChunk = InsertBefore;
			PreviousFreeChunk = InsertBefore->PreviousFreeChunk;
			if ( InsertBefore->PreviousFreeChunk )
			{
				InsertBefore->PreviousFreeChunk->NextFreeChunk = this;
			}
			else
			{
				BestFitAllocator.FirstFreeChunk = this;
			}
			InsertBefore->PreviousFreeChunk = this;
		}
		else
		{
			PreviousFreeChunk	= NULL;
			NextFreeChunk		= NULL;
			BestFitAllocator.FirstFreeChunk		= this;
		}
	}
}

/*-----------------------------------------------------------------------------
	FBestFitAllocator implementation.
-----------------------------------------------------------------------------*/

/**
 * Allocate physical memory.
 *
 * @param	AllocationSize	Size of allocation
 * @param	bAllowFailure	Whether to allow allocation failure or not
 * @return	Pointer to allocated memory
 */
void* FBestFitAllocator::Allocate( INT AllocationSize, UBOOL bAllowFailure )
{
	SCOPE_SECONDS_COUNTER(TimeSpentInAllocator);
	check( FirstChunk );

	// Make sure everything is appropriately aligned.
	AllocationSize = Align( AllocationSize, AllocationAlignment );

	// Perform a "best fit" search, returning first perfect fit if there is one.
	FMemoryChunk* CurrentChunk	= FirstFreeChunk;
	FMemoryChunk* BestChunk		= NULL;
	INT BestSize = MAXINT;
	INT Iteration = 0;
	do 
	{
		while ( CurrentChunk )
		{
			// Check whether chunk is available and large enough to hold allocation.
			check( CurrentChunk->bIsAvailable );
			INT AvailableSize = CurrentChunk->GetAvailableSize();
			if ( AvailableSize >= AllocationSize )
			{
				// Tighter fits are preferred.
				if ( AvailableSize < BestSize )
				{
					BestSize = AvailableSize;
					BestChunk = CurrentChunk;
				}

				// We have a perfect fit, no need to iterate further.
				if ( AvailableSize == AllocationSize )
				{
					break;
				}
			}
			CurrentChunk = CurrentChunk->NextFreeChunk;
		}

		// If we didn't find any chunk to allocate, and we're currently doing some async defragmentation...
		if ( !BestChunk && NumRelocationsInProgress > 0 && !bAllowFailure )
		{
			// Wait for all relocations to finish and try again.
			FinishAllRelocations();
			CurrentChunk = FirstFreeChunk;
		}
	} while ( !BestChunk && CurrentChunk );

	// Dump allocation info and return NULL if we weren't able to satisfy allocation request.
	if( !BestChunk )
	{
		if ( !bAllowFailure )
		{
#if !FINAL_RELEASE
			DumpAllocs();
			debugf(TEXT("Ran out of memory for allocation in best-fit allocator of size %i KByte"), AllocationSize / 1024);
			GLog->FlushThreadedLogs();
#endif
		}
		return NULL;
	}

	FMemoryChunk* AllocatedChunk = AllocateChunk( BestChunk, AllocationSize, FALSE );
	return AllocatedChunk->Base;
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
FBestFitAllocator::FMemoryChunk* FBestFitAllocator::AllocateChunk( FMemoryChunk* FreeChunk, INT AllocationSize, UBOOL bAsync )
{
	check( FreeChunk );
	check( FreeChunk->GetAvailableSize() >= AllocationSize );

	// If this is an immediate allocation (i.e. the CPU will start accessing the memory right away)
	// and the beginning of the chunk is currently being relocated by the GPU, split that part off and allocate from the rest.
	if ( !bAsync && FreeChunk->IsRelocating() && FreeChunk->SyncSize > 0 && FreeChunk->SyncSize < FreeChunk->Size )
	{
		Split( FreeChunk, FreeChunk->SyncSize, bAsync );
		FreeChunk = FreeChunk->NextChunk;
	}

	// Mark as being in use.
	FreeChunk->UnlinkFree();

	// Split chunk to avoid waste.
	if( FreeChunk->Size > AllocationSize )
	{
		Split( FreeChunk, AllocationSize, bAsync );
	}

	// Ensure that everything's in range.
	check( (FreeChunk->Base + FreeChunk->Size) <= (MemoryBase + MemorySize) );
	check( FreeChunk->Base >= MemoryBase );

	// Update usage stats in a thread safe way.
	appInterlockedAdd( &AllocatedMemorySize, +FreeChunk->Size );
	appInterlockedAdd( &AvailableMemorySize, -FreeChunk->Size );

	// Keep track of mapping and return pointer.
	PointerToChunkMap.Set( (PTRINT) FreeChunk->Base, FreeChunk );
	return FreeChunk;
}

/**
 * Marks the specified chunk as 'free' and updates tracking variables.
 * Calls LinkFreeChunk() to coalesce adjacent free memory.
 *
 * @param Chunk						Chunk to free
 * @param bMaintainSortedFreelist	If TRUE, maintains the free-list sort order
 */
void FBestFitAllocator::FreeChunk( FMemoryChunk* Chunk, UBOOL bMaintainSortedFreelist )
{
	// Remove the entry
	PointerToChunkMap.Remove((PTRINT) Chunk->Base);

	// Update usage stats in a thread safe way.
	appInterlockedAdd( &AllocatedMemorySize, -Chunk->Size );
	appInterlockedAdd( &AvailableMemorySize, +Chunk->Size );

	// Free the chunk.
	LinkFreeChunk(Chunk, bMaintainSortedFreelist);
}

#if !FINAL_RELEASE
	/** For debugging minidumps and release builds. */
	void* GBestFitAllocatorFreePointer = NULL;
#endif

/**
 * Frees allocation associated with passed in pointer.
 *
 * @param	Pointer		Pointer to free.
 */
void FBestFitAllocator::Free( void* Pointer )
{
	SCOPE_SECONDS_COUNTER(TimeSpentInAllocator);

#if !FINAL_RELEASE
	GBestFitAllocatorFreePointer = Pointer;
#endif

	// Look up pointer in TMap.
	FMemoryChunk* MatchingChunk = PointerToChunkMap.FindRef( (PTRINT) Pointer );
	check( MatchingChunk );

	// Is this chunk is currently being relocated asynchronously (by the GPU)?
	if ( MatchingChunk->IsRelocating() )
	{
		PendingFreeChunks.AddTail( MatchingChunk );
	}
	else
	{
		// Free the chunk.
		FreeChunk(MatchingChunk, FALSE);
	}
}

/**
 * Locks an FMemoryChunk
 *
 * @param Pointer		Pointer indicating which chunk to lock
 */
void FBestFitAllocator::Lock( const void* Pointer )
{
	FMemoryChunk* MatchingChunk = PointerToChunkMap.FindRef( (PTRINT) Pointer );
	check( MatchingChunk );

	// Is this chunk is currently being relocated asynchronously (by the GPU)?
	if ( MatchingChunk->IsRelocating() )
	{
		// Wait for it to finish.
		FinishAllRelocations();
	}

	MatchingChunk->bLocked = TRUE;
}

/**
 * Unlocks an FMemoryChunk
 *
 * @param Pointer		Pointer indicating which chunk to unlock
 */
void FBestFitAllocator::Unlock( const void* Pointer )
{
	FMemoryChunk* MatchingChunk = PointerToChunkMap.FindRef( (PTRINT) Pointer );
	check( MatchingChunk && MatchingChunk->IsRelocating() == FALSE );

	MatchingChunk->bLocked = FALSE;
}

/**
 * Sets the user payload for an FMemoryChunk
 *
 * @param Pointer		Pointer indicating a chunk
 * @param UserPayload	User payload to set
 */
void FBestFitAllocator::SetUserPayload( const void* Pointer, PTRINT UserPayload )
{
	FMemoryChunk* MatchingChunk = PointerToChunkMap.FindRef( (PTRINT) Pointer );
	checkf( MatchingChunk, TEXT("Couldn't find matching chunk for %x"), Pointer );
	if ( MatchingChunk )
	{
		MatchingChunk->UserPayload = UserPayload;
	}
}

/**
 * Returns the user payload for an FMemoryChunk
 *
 * @param Pointer		Pointer indicating a chunk
 * return				The chunk's user payload
 */
PTRINT FBestFitAllocator::GetUserPayload( const void* Pointer )
{
	FMemoryChunk* MatchingChunk = PointerToChunkMap.FindRef( (PTRINT) Pointer );
	check( MatchingChunk );
	if ( MatchingChunk )
	{
		return MatchingChunk->UserPayload;
	}
	return 0;
}

/**
 * Returns the amount of memory allocated for the specified address.
 *
 * @param	Pointer		Pointer to check.
 * @return				Number of bytes allocated
 */
INT FBestFitAllocator::GetAllocatedSize( void* Pointer )
{
	FMemoryChunk* MatchingChunk = PointerToChunkMap.FindRef( (PTRINT) Pointer );
	return MatchingChunk ? MatchingChunk->Size : 0;
}

/**
 * Tries to reallocate texture memory in-place (without relocating),
 * by adjusting the base address of the allocation but keeping the end address the same.
 *
 * @param	OldBaseAddress	Pointer to the original allocation
 * @returns	New base address if it succeeded, otherwise NULL
 **/
void* FBestFitAllocator::Reallocate( void* OldBaseAddress, INT NewSize )
{
	SCOPE_SECONDS_COUNTER(TimeSpentInAllocator);

	// Look up pointer in TMap.
	FMemoryChunk* MatchingChunk = PointerToChunkMap.FindRef( PTRINT(OldBaseAddress) );
	check( MatchingChunk && PTRINT(OldBaseAddress) == PTRINT(MatchingChunk->Base) );

	// Can't modify a chunk that is currently being relocated.
	// Actually, yes we can, since immediate reallocation doesn't move any memory.
// 	if ( MatchingChunk->IsRelocating() )
// 	{
// 		return NULL;
// 	}

	INT AlignedNewSize = Align( NewSize, AllocationAlignment );
	FMemoryChunk* NewChunk = NULL;
	INT MemoryAdjustment = Abs<INT>(AlignedNewSize - MatchingChunk->Size);

	// Are we growing the allocation?
	if ( AlignedNewSize > MatchingChunk->Size )
	{
		NewChunk = Grow( MatchingChunk, MemoryAdjustment );
	}
	else
	{
		NewChunk = Shrink( MatchingChunk, MemoryAdjustment );
	}
	return NewChunk ? NewChunk->Base : NULL;
}

/**
 * Tries to immediately grow a memory chunk by moving the base address, without relocating any memory.
 *
 * @param Chunk			Chunk to grow
 * @param GrowAmount	Number of bytes to grow by
 * @return				NULL if it failed, otherwise the new grown chunk
 */
FBestFitAllocator::FMemoryChunk* FBestFitAllocator::Grow( FMemoryChunk* Chunk, INT GrowAmount )
{
	// Is there enough free memory immediately before this chunk?
	FMemoryChunk* PrevChunk = Chunk->PreviousChunk;
	if ( PrevChunk && PrevChunk->bIsAvailable && PrevChunk->Size >= GrowAmount )
	{
		void* OldBaseAddress = Chunk->Base;
		PTRINT UserPayload = Chunk->UserPayload;
		PointerToChunkMap.Remove( PTRINT(OldBaseAddress) );

		// Shrink the previous and grow the current chunk.
		PrevChunk->Size -= GrowAmount;
		Chunk->Base -= GrowAmount;
		Chunk->Size += GrowAmount;

		PointerToChunkMap.Set( PTRINT(Chunk->Base), Chunk );

		if ( PrevChunk->Size == 0 )
		{
			delete PrevChunk;
		}

		Chunk->UserPayload = UserPayload;

		// Update usage stats in a thread safe way.
		appInterlockedAdd( &AllocatedMemorySize, +GrowAmount );
		appInterlockedAdd( &AvailableMemorySize, -GrowAmount );
		return Chunk;
	}
	return NULL;
}

/**
 * Immediately shrinks a memory chunk by moving the base address, without relocating any memory.
 * Always succeeds.
 *
 * @param Chunk			Chunk to shrink
 * @param ShrinkAmount	Number of bytes to shrink by
 * @return				The new shrunken chunk
 */
FBestFitAllocator::FMemoryChunk* FBestFitAllocator::Shrink( FMemoryChunk* Chunk, INT ShrinkAmount )
{
	// We're shrinking the allocation.
	check( ShrinkAmount <= Chunk->Size );
	void* OldBaseAddress = Chunk->Base;
	PTRINT UserPayload = Chunk->UserPayload;

	FMemoryChunk* NewFreeChunk = Chunk->PreviousChunk;
	if ( NewFreeChunk )
	{
		// Shrink the current chunk.
		Chunk->Base += ShrinkAmount;
		Chunk->Size -= ShrinkAmount;

		// Grow the previous chunk.
		INT OriginalPrevSize = NewFreeChunk->Size;
		NewFreeChunk->Size += ShrinkAmount;

		// If the previous chunk was "in use", split it and insert a 2nd free chunk.
		if ( !NewFreeChunk->bIsAvailable )
		{
			Split( NewFreeChunk, OriginalPrevSize, FALSE );
			NewFreeChunk = NewFreeChunk->NextChunk;
		}
	}
	else
	{
		// This was the first chunk, split it.
		Split( Chunk, ShrinkAmount, FALSE );

		// We're going to use the new chunk. Mark it as "used memory".
		Chunk = Chunk->NextChunk;
		Chunk->UnlinkFree();

		// Make the original chunk "free memory".
		NewFreeChunk = Chunk->PreviousChunk;
		LinkFreeChunk( NewFreeChunk, FALSE );
	}

	// Mark the newly freed memory as "being relocated" and require GPU sync.
	// (The GPU may still be rendering with the old, larger size.)
	NewFreeChunk->SetSyncIndex( GetCurrentSyncIndex(), NewFreeChunk->Size );

	PointerToChunkMap.Remove( PTRINT(OldBaseAddress) );
	PointerToChunkMap.Set( PTRINT(Chunk->Base), Chunk );
	Chunk->UserPayload = UserPayload;

	// Update usage stats in a thread safe way.
	appInterlockedAdd( &AllocatedMemorySize, -ShrinkAmount );
	appInterlockedAdd( &AvailableMemorySize, +ShrinkAmount );
	return Chunk;
}

/**
 * Requests an async allocation or reallocation.
 * The caller must hold on to the request until it has been completed or canceled.
 *
 * @param ReallocationRequest	The request
 * @param bForceRequest			If TRUE, the request will be accepted even if there's currently not enough free space
 * @return						TRUE if the request was accepted
 */
UBOOL FBestFitAllocator::AsyncReallocate( FAsyncReallocationRequest* Request, UBOOL bForceRequest )
{
	// Make sure everything is appropriately aligned.
	Request->NewSize = Align( Request->NewSize, AllocationAlignment );

	if ( Request->IsReallocation() )
	{
		// Look up pointer in TMap.
		Request->MemoryChunk	= PointerToChunkMap.FindRef( PTRINT(Request->OldAddress) );
		check( Request->MemoryChunk );
		Request->OldSize		= Request->MemoryChunk->Size;
	}

	// OOM?
	INT MemoryAdjustment		= Request->NewSize - Request->OldSize;
	if ( !bForceRequest && MemoryAdjustment > 0 && MemoryAdjustment > AvailableMemorySize )
	{
		return FALSE;
	}

	if ( Request->IsReallocation() )
	{
		// Already being reallocated?
		if ( Request->MemoryChunk->HasReallocationRequest() )
		{
			//@TODO: Perhaps flush or cancel previous reallocation and try again
			return FALSE;
		}

		// Try an immediate in-place reallocation (just changing the base address of the existing chunk).
		Request->NewAddress = Reallocate( Request->OldAddress, Request->NewSize );
		if ( Request->NewAddress )
		{
			// Note: The caller should call PlatformNotifyReallocationFinished().
			Request->MarkCompleted();
			return TRUE;
		}
	}

	if ( Settings.bEnableAsyncDefrag && Settings.bEnableAsyncReallocation )
	{
		appInterlockedAdd( &PendingMemoryAdjustment, +MemoryAdjustment );

		if ( Request->IsReallocation() )
		{
			ReallocationRequests.AddTail( Request );
			Request->MemoryChunk->ReallocationRequestNode = ReallocationRequests.GetTail();
		}
		else
		{
			// Allocations takes priority over reallocations.
			ReallocationRequests.AddHead( Request );
		}
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/**
 * Sorts the freelist based on increasing base address.
 *
 * @param NumFreeChunks		[out] Number of free chunks
 * @param LargestFreeChunk	[out] Size of the largest free chunk, in bytes
 */
void FBestFitAllocator::SortFreeList( INT& NumFreeChunks, INT& LargestFreeChunk )
{
	NumFreeChunks = 0;
	LargestFreeChunk = 0;

	if ( FirstFreeChunk )
	{
		NumFreeChunks++;
		LargestFreeChunk = FirstFreeChunk->Size;
		FMemoryChunk* LastSortedChunk = FirstFreeChunk;
		FMemoryChunk* ChunkToSort = FirstFreeChunk->NextFreeChunk;
		while ( ChunkToSort )
		{
			LargestFreeChunk = Max( LargestFreeChunk, ChunkToSort->Size );

			// Out of order?
			if ( ChunkToSort->Base < LastSortedChunk->Base )
			{
				FMemoryChunk* InsertBefore = FirstFreeChunk;
				while ( ChunkToSort->Base > InsertBefore->Base )
				{
					InsertBefore = InsertBefore->NextFreeChunk;
				}
				ChunkToSort->UnlinkFree();
				ChunkToSort->bIsAvailable = TRUE;	// Set it back to 'free'
				ChunkToSort->PreviousFreeChunk = InsertBefore->PreviousFreeChunk;
				ChunkToSort->NextFreeChunk = InsertBefore;
				if ( InsertBefore->PreviousFreeChunk )
				{
					InsertBefore->PreviousFreeChunk->NextFreeChunk = ChunkToSort;
				}
				InsertBefore->PreviousFreeChunk = ChunkToSort;
				if ( InsertBefore == FirstFreeChunk )
				{
					FirstFreeChunk = ChunkToSort;
				}
			}
			LastSortedChunk = ChunkToSort;
			ChunkToSort = ChunkToSort->NextFreeChunk;
			NumFreeChunks++;
		}
	}
}

/**
 * Defrag helper function. Checks if the specified allocation fits within
 * the adjacent free chunk(s), accounting for potential reallocation request.
 *
 * @param UsedChunk		Allocated chunk to check for a fit
 * @param bAnyChunkType	If FALSE, only succeeds if 'UsedChunk' has a reallocation request and fits
 * @return				Returns 'UsedChunk' if it fits the criteria, otherwise NULL
 */
FBestFitAllocator::FMemoryChunk* FBestFitAllocator::FindAdjacent( FMemoryChunk* UsedChunk, UBOOL bAnyChunkType )
{
	if ( UsedChunk && !UsedChunk->IsRelocating() && (bAnyChunkType || UsedChunk->HasReallocationRequest()) )
	{
		FMemoryChunk* FreeChunkLeft = UsedChunk->PreviousChunk;
		FMemoryChunk* FreeChunkRight = UsedChunk->NextChunk;

		INT AvailableSize = UsedChunk->Size;
		if ( FreeChunkLeft && FreeChunkLeft->bIsAvailable )
		{
			AvailableSize += FreeChunkLeft->Size;
		}
		if ( FreeChunkRight && FreeChunkRight->bIsAvailable )
		{
			AvailableSize += FreeChunkRight->Size;
		}

		// Does it fit?
		INT FinalSize = UsedChunk->GetFinalSize();
		if ( FinalSize <= AvailableSize && CanRelocate( UsedChunk ) )
		{
			return UsedChunk;
		}
	}
	return NULL;
}

/**
 * Searches for a reallocation request that would fit within the specified free chunk.
 * Prefers allocation requests over reallocation requests.
 *
 * @param FreeChunk		Free chunk we're trying to fill up
 * @return				First reallocating chunk that could fit, or NULL
 */
FBestFitAllocator::FRequestNode* FBestFitAllocator::FindAnyReallocation( FMemoryChunk* FreeChunk )
{
	FRequestNode* BestRequest	= NULL;
	INT BestFit					= MAXINT;
	UBOOL bFoundAllocation		= FALSE;

	for ( FRequestList::TIterator It(ReallocationRequests.GetHead()); It; ++It )
	{
		FRequestNode* RequestNode			= It.GetNode();
		FAsyncReallocationRequest* Request	= RequestNode->GetValue();
		FMemoryChunk* CurrentChunk			= Request->MemoryChunk;
		INT CurrentFit						= FreeChunk->Size - Request->GetNewSize();

		// Have we started getting into reallocations, but we've already found a suitable allocation?
		if ( bFoundAllocation && Request->IsReallocation() )
		{
			// Use the allocation first.
			break;
		}

		// Better fit than previously?
		if ( CurrentFit >= 0 && CurrentFit < BestFit && (Request->IsAllocation() || CanRelocate( CurrentChunk )) )
		{
			BestFit		= CurrentFit;
			BestRequest	= RequestNode;
			bFoundAllocation = Request->IsAllocation();

			// Perfect fit?
			if ( CurrentFit == 0 )
			{
				break;
			}
		}
	}
	return BestRequest;
}

/**
 * Searches for an allocated chunk that would fit within the specified free chunk.
 * The allocated chunk must be adjacent to a free chunk and have a larger
 * base address than 'FreeChunk'.
 * Starts searching from the end of the texture pool.
 *
 * @param FreeChunk		Free chunk we're trying to fill up
 * @return				Pointer to a suitable chunk, or NULL
 */
FBestFitAllocator::FMemoryChunk* FBestFitAllocator::FindAdjacentToHole( FMemoryChunk* FreeChunk )
{
//@TODO: Maintain LastFreeChunk for speed
	FMemoryChunk* LastFreeChunk = LastChunk;
	while ( LastFreeChunk && !LastFreeChunk->bIsAvailable )
	{
		LastFreeChunk = LastFreeChunk->PreviousChunk;
	}

	FMemoryChunk* Chunk = LastFreeChunk;
	while ( Chunk && Chunk->Base > FreeChunk->Base )
	{
		// Check Right
		const FMemoryChunk* Right = Chunk->NextChunk;
		if ( Right && !Right->bIsAvailable && Right->GetFinalSize() < FreeChunk->Size && CanRelocate( Right ) )
		{
			return Chunk->NextChunk;
		}
		// Check Left
		const FMemoryChunk* Left = Chunk->PreviousChunk;
		if ( Left && !Left->bIsAvailable && Left->GetFinalSize() < FreeChunk->Size && CanRelocate( Left ) )
		{
			return Chunk->PreviousChunk;
		}
		Chunk = Chunk->NextFreeChunk;
	}
	return NULL;
}

/**
 * Searches for an allocated chunk that would fit within the specified free chunk.
 * Any chunk that fits and has a larger base address than 'FreeChunk' is accepted.
 * Starts searching from the end of the texture pool.
 *
 * @param FreeChunk		Free chunk we're trying to fill up
 * @return				Pointer to a suitable chunk, or NULL
 */
FBestFitAllocator::FMemoryChunk* FBestFitAllocator::FindAny( FMemoryChunk* FreeChunk )
{
	//@TODO: Stop the search at some reasonable threshold (FreeChunk->Size < THRESHOLD || NumChunksSearched > THRESHOLD).
	FMemoryChunk* BestChunk = NULL;
	INT BestFit = MAXINT;
	FMemoryChunk* CurrentChunk = LastChunk;
	while ( CurrentChunk && CurrentChunk->Base > FreeChunk->Base )
	{
		if ( !CurrentChunk->bIsAvailable )
		{
			INT CurrentFit = FreeChunk->Size - CurrentChunk->GetFinalSize();

			// Better fit than previously?
			if ( CurrentFit >= 0 && CurrentFit < BestFit && CanRelocate( CurrentChunk ) )
			{
				BestFit = CurrentFit;
				BestChunk = CurrentChunk;

				// Perfect fit?
				if ( CurrentFit == 0 )
				{
					break;
				}
			}
		}
		CurrentChunk = CurrentChunk->PreviousChunk;
	}

	return BestChunk;
}

/**
 * Checks the internal state for errors. (Slow)
 *
 * @param bCheckSortedFreeList	If TRUE, also checks that the freelist is sorted
 */
void FBestFitAllocator::CheckForErrors( UBOOL bCheckSortedFreeList )
{
	if ( FirstFreeChunk == NULL )
	{
		return;
	}

	if ( bCheckSortedFreeList )
	{
		FMemoryChunk* Chunk = FirstFreeChunk;
		INT TotalFreeMem = Chunk->Size;
		while ( Chunk->NextFreeChunk )
		{
			check( Chunk->bIsAvailable );
			check( Chunk->Base < Chunk->NextFreeChunk->Base );
			check( !Chunk->NextChunk->bIsAvailable );
			check( Chunk->PreviousChunk == NULL || !Chunk->PreviousChunk->bIsAvailable );
			Chunk = Chunk->NextFreeChunk;
			TotalFreeMem += Chunk->Size;
		}
		check( TotalFreeMem == AvailableMemorySize );
	}

	INT TotalUsedMem = 0;
	INT TotalFreeMem = 0;
	FMemoryChunk* Chunk = FirstChunk;
	while ( Chunk )
	{
		if ( Chunk->bIsAvailable )
		{
			TotalFreeMem += Chunk->Size;
		}
		else
		{
			TotalUsedMem += Chunk->Size;
		}
		Chunk = Chunk->NextChunk;
	}
	check( TotalUsedMem == AllocatedMemorySize );
	check( TotalFreeMem == AvailableMemorySize );
}

/**
 * Initiates an async relocation of an allocated chunk into a free chunk.
 * Takes potential reallocation request into account.
 *
 * @param Stats			[out] Stats
 * @param FreeChunk		Destination chunk (free memory)
 * @param SourceChunk	Source chunk (allocated memory)
 * @return				Next Free chunk to try to fill up
 */
FBestFitAllocator::FMemoryChunk* FBestFitAllocator::RelocateIntoFreeChunk( FRelocationStats& Stats, FMemoryChunk* FreeChunk, FMemoryChunk* SourceChunk )
{
	FRequestNode* ReallocationRequestNode = SourceChunk->ReallocationRequestNode;

	// Save off important data from 'SourceChunk', since it will get modified by the call to LinkFreeChunk().
	BYTE* OldBaseAddress	= SourceChunk->Base;
	PTRINT UserPayload		= SourceChunk->UserPayload;
	INT OldSize				= SourceChunk->Size;
	const INT NewSize		= SourceChunk->GetFinalSize();
	const INT UsedSize		= Min(NewSize, OldSize);

	// Are we relocating into adjacent free chunk?
	UBOOL bAdjacentRelocation = SourceChunk->PreviousChunk == FreeChunk || SourceChunk->NextChunk == FreeChunk;

// Enable for debugging:
// CheckForErrors( TRUE );

	// Merge adjacent free chunks into SourceChunk to make a single free chunk.
	LinkFreeChunk( SourceChunk, TRUE );

	FMemoryChunk* DestinationChunk;
	if ( bAdjacentRelocation )
	{
		DestinationChunk = SourceChunk;
	}
	else
	{
		DestinationChunk = FreeChunk;
	}
	// 'FreeChunk' was deleted if it was adjacent to SourceChunk. Set to NULL to avoid using it by mistake.
	FreeChunk = NULL;

	// Leave room for new mips to stream in.
	INT DestinationOffset = Max(NewSize - OldSize, 0);

	// Relocate the memory if needed
	BYTE* NewBaseAddress = DestinationChunk->Base;
	if ( OldBaseAddress != (NewBaseAddress + DestinationOffset) )
	{
		Relocate( Stats, DestinationChunk, DestinationOffset, OldBaseAddress, UsedSize, UserPayload );
	}
	// Make sure the destination chunk keeps the UserPayload, no matter what. :)
	DestinationChunk->UserPayload = UserPayload;

	// Update our book-keeping.
	PointerToChunkMap.Remove((PTRINT) OldBaseAddress);
	PointerToChunkMap.Set((PTRINT) NewBaseAddress, DestinationChunk);

	// Move the reallocation request into the InProgress list.
	if ( ReallocationRequestNode )
	{
		FAsyncReallocationRequest* ReallocationRequest = ReallocationRequestNode->GetValue();
		check( SourceChunk == ReallocationRequest->MemoryChunk );

		// Create a new node in the 'InProgress' list.
		ReallocationRequestsInProgress.AddHead( ReallocationRequest );
		FRequestNode* NewNode = ReallocationRequestsInProgress.GetHead();

		// Swap chunk and node pointers.
		SourceChunk->ReallocationRequestNode = NULL;
		DestinationChunk->ReallocationRequestNode = NewNode;
		ReallocationRequest->MemoryChunk = DestinationChunk;
		ReallocationRequest->NewAddress = NewBaseAddress;

		// Remove from current request list.
		ReallocationRequests.RemoveNode( ReallocationRequestNode );
	}

	// Is there free space left over at the end of DestinationChunk?
	FMemoryChunk* NextFreeChunk;
	if ( DestinationChunk->Size > NewSize )
	{
		// Split the DestinationChunk into a used chunk and a free chunk.
		Split( DestinationChunk, NewSize, TRUE );
		NextFreeChunk = DestinationChunk->NextChunk;
	}
	else
	{
		// The whole DestinationChunk is now allocated.
		check( DestinationChunk->Size == NewSize );
		NextFreeChunk = DestinationChunk->NextFreeChunk;
	}
	DestinationChunk->UnlinkFree();

	// Mark both chunks as "in use" during the current sync step.
	// Note: This sync index will propagate if these chunks are involved in any merge/split in the future.
	SourceChunk->SetSyncIndex( GetCurrentSyncIndex(), OldSize );
	DestinationChunk->SetSyncIndex( GetCurrentSyncIndex(), NewSize );

	if ( NewSize != OldSize )
	{
		INT MemoryAdjustment = NewSize - OldSize;
		appInterlockedAdd( &AllocatedMemorySize, +MemoryAdjustment );
		appInterlockedAdd( &AvailableMemorySize, -MemoryAdjustment );
		appInterlockedAdd( &PendingMemoryAdjustment, -MemoryAdjustment );
	}

// Enable for debugging:
// CheckForErrors( TRUE );

	// Did we free up a chunk to the left of NextFreeChunk?
	if ( !bAdjacentRelocation && OldBaseAddress < NewBaseAddress )
	{
		// Use that one for the next defrag step!
		return SourceChunk;
	}
	return NextFreeChunk;
}

/**
 * Allocates memory from the specified free chunk, servicing an async allocation request.
 *
 * @param Stats			[out] Stats
 * @param FreeChunk		Chunk to allocate memory from
 * @param RequestNode	List node to the allocation request
 * @return				Next Free chunk to try to fill up
 */
FBestFitAllocator::FMemoryChunk* FBestFitAllocator::AllocateIntoFreeChunk( FRelocationStats& Stats, FMemoryChunk* FreeChunk, FRequestNode* RequestNode )
{
	FMemoryChunk* NextFreeChunk = FreeChunk->NextFreeChunk;
	FAsyncReallocationRequest* Request = RequestNode->GetValue();
	check( Request->IsAllocation() );

	// Note: AllocateChunk() may split 'FreeChunk' and return a new chunk.
	FMemoryChunk* AllocatedChunk = AllocateChunk( FreeChunk, Request->GetNewSize(), TRUE );

	// Create a new node in the 'InProgress' list.
	ReallocationRequestsInProgress.AddHead( Request );
	FRequestNode* NewNode = ReallocationRequestsInProgress.GetHead();

	// Setup chunk and node variables.
	AllocatedChunk->ReallocationRequestNode	= NewNode;
	Request->MemoryChunk				= AllocatedChunk;
	Request->NewAddress					= AllocatedChunk->Base;

	// Mark the chunk as "in use" during the current sync step.
	// Note: This sync index will propagate if these chunks are involved in any merge/split in the future.
	AllocatedChunk->SetSyncIndex( GetCurrentSyncIndex(), AllocatedChunk->Size );

	// Remove from current request list.
	ReallocationRequests.RemoveNode( RequestNode );

	appInterlockedAdd( &PendingMemoryAdjustment, -Request->GetNewSize() );

	if ( AllocatedChunk->NextChunk && AllocatedChunk->NextChunk->bIsAvailable )
	{
		return AllocatedChunk->NextChunk;
	}
	else
	{
		return NextFreeChunk;
	}
}

/**
 * Blocks the calling thread until all relocations and reallocations that were initiated by Tick() have completed.
 *
 * @return		TRUE if there were any relocations in progress before this call
 */
UBOOL FBestFitAllocator::FinishAllRelocations()
{
	UBOOL bWasAnyRelocationsInProgress = NumRelocationsInProgress > 0;

	if ( bWasAnyRelocationsInProgress )
	{
		BlockOnFence();
	}

	// All reallocation requests have now completed.
	INT TotalMemoryAdjustment = 0;
	for ( FRequestList::TIterator It(ReallocationRequestsInProgress.GetHead()); It; ++It )
	{
		FAsyncReallocationRequest* FinishedRequest = *It;

		FMemoryChunk* FinishedChunk = FinishedRequest->MemoryChunk;
		if ( !FinishedRequest->IsCanceled() )
		{
			// Mark it complete.
			FinishedRequest->MarkCompleted();

			if ( FinishedRequest->IsReallocation() )
			{
				NumFinishedAsyncReallocations++;
			}
			else
			{
				NumFinishedAsyncAllocations++;
			}

			if ( !bBenchmarkMode )
			{
				PlatformNotifyReallocationFinished( FinishedRequest, FinishedChunk->UserPayload );
			}
			TotalMemoryAdjustment += FinishedRequest->NewSize - FinishedRequest->OldSize;
			FinishedRequest->MemoryChunk = NULL;
		}
		else
		{
			// Delete the request, since it's our copy. The user has deleted the original already.
			delete FinishedRequest;
		}
		FinishedChunk->ReallocationRequestNode = NULL;
	}
	ReallocationRequestsInProgress.Clear();

	check( ReallocationRequests.Num() > 0 || PendingMemoryAdjustment == 0 );

	// Take the opportunity to free all chunks that couldn't be freed immediately before.
	for ( TDoubleLinkedList<FMemoryChunk*>::TIterator It(PendingFreeChunks.GetHead()); It; ++It )
	{
		FMemoryChunk* Chunk = *It;
		FreeChunk( Chunk, FALSE );
	}
	PendingFreeChunks.Clear();

	NumRelocationsInProgress = 0;

	return bWasAnyRelocationsInProgress;
}

/**
 * Inserts a platform fence and updates the allocator sync index to match.
 */
void FBestFitAllocator::InsertFence()
{
	if ( !bBenchmarkMode )
	{
		PlatformSyncFence = PlatformInsertFence();
	}
	CurrentSyncIndex++;
}

/**
 * Blocks the calling thread until the current sync fence has been completed.
 */
void FBestFitAllocator::BlockOnFence()
{
	if ( CompletedSyncIndex < (CurrentSyncIndex - 1) )
	{
		DWORD StartTime = appCycles();
		if ( !bBenchmarkMode )
		{
			PlatformBlockOnFence( PlatformSyncFence );
		}
		CompletedSyncIndex = CurrentSyncIndex - 1;
		BlockedCycles += appCycles() - StartTime;
	}
}

/**
 * Blocks the calling thread until the specified sync index has been completed.
 *
 * @param SyncIndex		Sync index to wait for
 */
void FBestFitAllocator::BlockOnSyncIndex( DWORD SyncIndex )
{
	// Not completed yet?
	if ( SyncIndex > CompletedSyncIndex )
	{
		FinishAllRelocations();

		// Still not completed?
		if ( SyncIndex > CompletedSyncIndex )
		{
			InsertFence();
			BlockOnFence();
			FinishAllRelocations();
		}
	}
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
void FBestFitAllocator::CancelAsyncReallocation( FAsyncReallocationRequest* Request, const void* CurrentBaseAddress )
{
	check( Request && !Request->IsCanceled() );
	NumCanceledAsyncRequests++;

	INT MemoryAdjustment = Request->NewSize - Request->OldSize;

	// We cannot undo shrinking reallocations. The memory will be lost and gone forever.
	check( MemoryAdjustment > 0 );

	// Mark it canceled.
	Request->bIsCanceled = TRUE;

	// Make sure it's marked 'completed'.
	UBOOL bHasStarted = Request->HasStarted();
	UBOOL bHasCompleted = Request->HasCompleted();
	if ( !bHasCompleted )
	{
		// This will also prevent it from further relocation until it's gone completely from the system.
		Request->MarkCompleted();
	}

	// Has it not started yet?
	if ( !bHasStarted )
	{
		if ( Request->IsReallocation() )
		{
			// If it hasn't started yet, just remove the request and flag it completed.
			FMemoryChunk* MatchingChunk = Request->MemoryChunk;
			check( MatchingChunk && CurrentBaseAddress == NULL );
			FRequestNode* RequestNode = MatchingChunk->ReallocationRequestNode;
			check( RequestNode );

			// Remove it from our end of the system.
			ReallocationRequests.RemoveNode( RequestNode );
			MatchingChunk->ReallocationRequestNode = NULL;
		}
		else
		{
			FRequestNode* RequestNode = ReallocationRequests.FindNode( Request );
			check( RequestNode );
			ReallocationRequests.RemoveNode( RequestNode );
		}
		appInterlockedAdd( &PendingMemoryAdjustment, -MemoryAdjustment );
	}
	else
	{
		// Is it still in progress?
		if ( !bHasCompleted )
		{
			FMemoryChunk* MatchingChunk = Request->MemoryChunk;
			check( MatchingChunk && CurrentBaseAddress == NULL );

			// Make a copy of the request, since the current Request will be deleted by the user.
			FAsyncReallocationRequest* RequestCopy = new FAsyncReallocationRequest( *Request );
			FRequestNode* RequestNode = MatchingChunk->ReallocationRequestNode;
			MatchingChunk->ReallocationRequestNode = NULL;

			FMemoryChunk* NewChunk;
			if ( Request->IsReallocation() )
			{
				// Undo the 'Grow' by immediately shrinking it back to the old size (adjusting baseaddress).
				// Allow the GPU to finish relocating into the used portion. (Any new relocations will be pipelined.)
				NewChunk = Shrink( MatchingChunk, MemoryAdjustment );
			}
			else
			{
				// Undo the allocation by freeing it.
				FreeChunk( MatchingChunk, FALSE );
				NewChunk = MatchingChunk;
			}

			// Fixup all request pointers.
			RequestNode->GetValue() = RequestCopy;
			NewChunk->ReallocationRequestNode = RequestNode;
			RequestCopy->MemoryChunk = NewChunk;
		}
		// Is it already completed?
		else
		{
			// When it's completed already, the allocator doesn't keep track of it anymore in any lists.
			if ( Request->IsReallocation() )
			{
				FMemoryChunk* MatchingChunk = PointerToChunkMap.FindRef( (PTRINT) CurrentBaseAddress );
				check( MatchingChunk && MatchingChunk->ReallocationRequestNode == NULL );

				// Undo the 'Grow' by immediately shrinking it back to the old size (adjusting baseaddress).
				FMemoryChunk* NewChunk = Shrink( MatchingChunk, MemoryAdjustment );
			}
			else
			{
				FMemoryChunk* MatchingChunk = PointerToChunkMap.FindRef( (PTRINT) Request->GetNewBaseAddress() );
				check( MatchingChunk && MatchingChunk->ReallocationRequestNode == NULL );

				// Undo the allocation by freeing it.
				FreeChunk( MatchingChunk, FALSE );
			}
		}
	}
}

/**
 * Performs a partial defrag while trying to process any pending async reallocation requests.
 *
 * @param Stats			[out] Stats
 * @param StartTime		Start time, used for limiting the Tick() time
 */
void FBestFitAllocator::PartialDefragmentation( FRelocationStats& Stats, DOUBLE StartTime )
{
	FMemoryChunk* FreeChunk = FirstFreeChunk;
	while ( FreeChunk /*&& ReallocationRequests.Num() > 0*/ && Stats.NumBytesRelocated < Settings.MaxDefragRelocations )
	{
		FRequestNode* BestRequestNode = NULL;
		FMemoryChunk* BestChunk = NULL;

		if ( FreeChunk->DefragCounter )
		{
			FreeChunk->DefragCounter--;
		}
		else
		{
			// 1. Merge with Left, if it has a Reallocation request and fits
			BestChunk = FindAdjacent( FreeChunk->PreviousChunk, FALSE );
			if ( !BestChunk )
			{
				// 2. Merge with Right, if it has an async request and fits
				BestChunk = FindAdjacent( FreeChunk->NextChunk, FALSE );

				if ( !BestChunk )
				{
					// 3. Merge with any async request (best-fitting)
					BestRequestNode = FindAnyReallocation( FreeChunk );

					if ( !BestRequestNode )
					{
						// 4. Merge with a used chunk adjacent to hole (to make that hole larger).
						BestChunk = FindAdjacentToHole( FreeChunk );

						if ( !BestChunk )
						{
							// 5. Merge with chunk from the end of the pool (well-fitting)
							BestChunk = FindAny( FreeChunk );
						}
					}
				}
			}
		}

		if ( BestChunk )
		{
			FreeChunk = RelocateIntoFreeChunk( Stats, FreeChunk, BestChunk );
		}
		else if ( BestRequestNode )
		{
			FAsyncReallocationRequest* Request = BestRequestNode->GetValue();
			if ( Request->IsReallocation() )
			{
				FreeChunk = RelocateIntoFreeChunk( Stats, FreeChunk, Request->MemoryChunk );
			}
			else
			{
				FreeChunk = AllocateIntoFreeChunk( Stats, FreeChunk, BestRequestNode );
			}
		}
		else
		{
			// Did the free chunk fail to defrag?
			if ( FreeChunk->DefragCounter == 0 && (FreeChunk->NextFreeChunk || ReallocationRequests.Num() > 0) )
			{
				// Don't try it again for a while.
				if ( FreeChunk->Size < DEFRAG_SMALL_CHUNK_SIZE )
				{
					FreeChunk->DefragCounter = DEFRAG_SMALL_CHUNK_COUNTER_MIN + RandHelper(DEFRAG_SMALL_CHUNK_COUNTER_MAX - DEFRAG_SMALL_CHUNK_COUNTER_MIN);
				}
				else
				{
					FreeChunk->DefragCounter = DEFRAG_CHUNK_COUNTER_MIN + RandHelper(DEFRAG_CHUNK_COUNTER_MAX - DEFRAG_CHUNK_COUNTER_MIN);
				}
			}

			FreeChunk = FreeChunk->NextFreeChunk;
		}

		// Limit time spent
		DOUBLE TimeSpent = appSeconds() - StartTime;
		if ( TimeSpent > PARTIALDEFRAG_TIMELIMIT )
		{
			break;
		}
	}
}

/**
 * Performs a partial defrag by shifting down memory to fill holes, in a brute-force manner.
 * Takes consideration to async reallocations, but processes the all memory in order.
 *
 * @param Stats			[out] Stats
 * @param StartTime		Start time, used for limiting the Tick() time
 */
void FBestFitAllocator::PartialDefragmentationDownshift( FRelocationStats& Stats, DOUBLE StartTime )
{
	FMemoryChunk* FreeChunk = FirstFreeChunk;
	while ( FreeChunk && Stats.NumBytesRelocated < Settings.MaxDefragRelocations && Stats.NumBytesDownShifted < Settings.MaxDefragDownShift )
	{
		FMemoryChunk* BestChunk = NULL;

		// 6. Merge with Right, if it fits
		BestChunk = FindAdjacent( FreeChunk->NextChunk, TRUE );

		if ( BestChunk )
		{
			Stats.NumBytesDownShifted += BestChunk->Size;
			FreeChunk = RelocateIntoFreeChunk( Stats, FreeChunk, BestChunk );
		}
		else
		{
			FreeChunk = FreeChunk->NextFreeChunk;
		}

		// Limit time spent
		DOUBLE TimeSpent = appSeconds() - StartTime;
		if ( TimeSpent > PARTIALDEFRAG_TIMELIMIT )
		{
			break;
		}
	}
}

/**
 * Performs a full defrag and ignores all reallocation requests.
 *
 * @param Stats			[out] Stats
 */
void FBestFitAllocator::FullDefragmentation( FRelocationStats& Stats )
{
	FMemoryChunk* FreeChunk = FirstFreeChunk;
	while ( FreeChunk )
	{
		FMemoryChunk* BestChunk = NULL;
		if ( !BestChunk )
		{
			// Try merging with a used chunk adjacent to hole (to make that hole larger).
			BestChunk = FindAdjacentToHole( FreeChunk );

			if ( !BestChunk )
			{
				// Try merging with chunk from the end of the pool (well-fitting)
				BestChunk = FindAny( FreeChunk );

				if ( !BestChunk )
				{
					// Try merging with Right, if it fits (brute-force downshifting)
					BestChunk = FindAdjacent( FreeChunk->NextChunk, TRUE );
					if ( BestChunk )
					{
						Stats.NumBytesDownShifted += BestChunk->Size;
					}
				}
			}
		}
		if ( BestChunk )
		{
			FreeChunk = RelocateIntoFreeChunk( Stats, FreeChunk, BestChunk );
		}
		else
		{
			FreeChunk = FreeChunk->NextFreeChunk;
		}
	}
}

/**
 * Partially defragments the memory and tries to process all async reallocation requests at the same time.
 * Call this once per frame.
 *
 * @param Stats			[out] Stats
 * @param bPanicDefrag	If TRUE, performs a full defrag and ignores all reallocation requests
 */
INT FBestFitAllocator::Tick( FRelocationStats& Stats, UBOOL bPanicDefrag )
{
	SET_CYCLE_COUNTER( STAT_TexturePool_Blocked, BlockedCycles, 1 );
	DOUBLE StartTime = appSeconds();
	BlockedCycles = 0;

	// Block until all relocations that were kicked of from last call have been completed.
	// There may still be chunks being flagged as 'IsRelocating' due to immediate shrinks between calls.
	FinishAllRelocations();

	// Sort the Free chunks.
	SortFreeList( Stats.NumHoles, Stats.LargestHoleSize );

	if ( Settings.bEnableAsyncDefrag || ReallocationRequests.Num() || ReallocationRequestsInProgress.Num() || bPanicDefrag )
	{
		if ( !bPanicDefrag )
		{
			// Smart defrag
			PartialDefragmentation( Stats, StartTime );

			// Brute-force defrag
			PartialDefragmentationDownshift( Stats, StartTime );
		}
		else
		{
			FullDefragmentation( Stats );
		}
	}

	NumRelocationsInProgress = Stats.NumRelocations;

	// Start a new sync point.
	InsertFence();

	TotalNumRelocations += Stats.NumRelocations;
	TotalNumBytesRelocated += Stats.NumBytesRelocated;
	MaxNumHoles = Max( MaxNumHoles, Stats.NumHoles );
	MinLargestHole = Min( MinLargestHole, Stats.LargestHoleSize );

	return Stats.NumBytesRelocated;
}

/**
 * Dump allocation information.
 */
void FBestFitAllocator::DumpAllocs( FOutputDevice& Ar/*=*GLog*/ )
{		
	// Memory usage stats.
	INT				UsedSize		= 0;
	INT				FreeSize		= 0;
	INT				NumUsedChunks	= 0;
	INT				NumFreeChunks	= 0;
	
	// Fragmentation and allocation size visualization.
	INT				NumBlocks		= MemorySize / AllocationAlignment;
	INT				Dimension		= 1 + NumBlocks / appTrunc(appSqrt(NumBlocks));			
	TArray<FColor>	AllocationVisualization;
	AllocationVisualization.AddZeroed( Dimension * Dimension );
	INT				VisIndex		= 0;

	// Traverse linked list and gather allocation information.
	FMemoryChunk*	CurrentChunk	= FirstChunk;	
	while( CurrentChunk )
	{
		FColor VisColor;
		// Free chunk.
		if( CurrentChunk->bIsAvailable )
		{
			NumFreeChunks++;
			FreeSize += CurrentChunk->Size;
			VisColor = FColor(0,255,0);
		}
		// Allocated chunk.
		else
		{
			NumUsedChunks++;
			UsedSize += CurrentChunk->Size;
			
			// Slightly alternate coloration to also visualize allocation sizes.
			if( NumUsedChunks % 2 == 0 )
			{
				VisColor = FColor(255,0,0);
			}
			else
			{
				VisColor = FColor(192,0,0);
			}
		}

		for( INT i=0; i<(CurrentChunk->Size/AllocationAlignment); i++ )
		{
			AllocationVisualization(VisIndex++) = VisColor;
		}

		CurrentChunk = CurrentChunk->NextChunk;
	}

	check(UsedSize == AllocatedMemorySize);
	check(FreeSize == AvailableMemorySize);

	// Write out bitmap for visualization of fragmentation and allocation patterns.
	appCreateBitmap( TEXT("..\\..\\Binaries\\TextureMemory"), Dimension, Dimension, AllocationVisualization.GetTypedData() );
	Ar.Logf( TEXT("BestFitAllocator: Allocated %i KByte in %i chunks, leaving %i KByte in %i chunks."), UsedSize / 1024, NumUsedChunks, FreeSize / 1024, NumFreeChunks );
	Ar.Logf( TEXT("BestFitAllocator: %5.2f ms in allocator"), TimeSpentInAllocator * 1000 );
}


/**
 * Helper function to fill in one gradient bar in the texture, for memory visualization purposes.
 */
void FillVizualizeData( FColor* TextureData, INT& X, INT& Y, INT& NumBytes, const FColor& Color1, const FColor& Color2, const INT SizeX, const INT SizeY, const INT Pitch, const INT PixelSize )
{
	// Fill pixels with a color gradient that represents the current allocation type.
	INT MaxPixelIndex = Max( (NumBytes - 1) / PixelSize, 1 );
	INT PixelIndex = 0;
	while ( NumBytes > 0 )
	{
		FColor& PixelColor = TextureData[ Y*Pitch + X ];
		PixelColor.R = (INT(Color1.R) * PixelIndex + INT(Color2.R) * (MaxPixelIndex - PixelIndex)) / MaxPixelIndex;
		PixelColor.G = (INT(Color1.G) * PixelIndex + INT(Color2.G) * (MaxPixelIndex - PixelIndex)) / MaxPixelIndex;
		PixelColor.B = (INT(Color1.B) * PixelIndex + INT(Color2.B) * (MaxPixelIndex - PixelIndex)) / MaxPixelIndex;
		PixelColor.A = 255;
		if ( ++X >= SizeX )
		{
			X = 0;
			if ( ++Y >= SizeY )
			{
				break;
			}
		}
		PixelIndex++;
		NumBytes -= PixelSize;
	}
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
UBOOL FBestFitAllocator::GetTextureMemoryVisualizeData( FColor* TextureData, INT SizeX, INT SizeY, INT Pitch, const INT PixelSize )
{
	check( Align(Pitch,sizeof(FColor)) == Pitch );
	Pitch /= sizeof(FColor);
	FColor TypeColor[2][6] =
	//  Allocated:           Free:             Locked:          Relocating:        Resizing:		Resized:
	{ { FColor(220,220,220), FColor(50,50,50), FColor(220,0,0), FColor(220,220,0), FColor(0,220,0), FColor(0,140,0) },
	  { FColor(180,180,180), FColor(50,50,50), FColor(180,0,0), FColor(180,180,0), FColor(0,180,0), FColor(0,50,0) } };
	INT X = 0;
	INT Y = 0;
	INT NumBytes = 0;
	EMemoryElementType CurrentType = MET_Allocated;
	FMemoryChunk* Chunk = FirstChunk;
	FMemoryChunk* CurrentChunk = NULL;
	UBOOL bIsColoringUsableRelocatingMemory = FALSE;
	while ( Chunk && Y < SizeY )
	{
		EMemoryElementType ChunkType = GetChunkType( Chunk );

		// Fill pixels with a color gradient that represents the current allocation type.
		FColor Color1 = TypeColor[0][ CurrentType ];
		FColor Color2 = TypeColor[1][ CurrentType ];

		// Special case for relocating chunks, to show it in two color gradient bars.
		if ( CurrentType == MET_Relocating )
		{
			// First, color the FMemoryChunk::SyncSize part of the chunk.
			INT UsableMemorySize = CurrentChunk->Size - CurrentChunk->SyncSize;
			NumBytes -= UsableMemorySize;
			FillVizualizeData( TextureData, X, Y, NumBytes, Color1, Color2, SizeX, SizeY, Pitch, PixelSize );

			// Second, color the rest (immediately usable) part of the chunk.
			NumBytes += UsableMemorySize;
			Color1 = TypeColor[0][ MET_Relocating ];
			Color2 = TypeColor[1][ MET_Resized ];
		}

		FillVizualizeData( TextureData, X, Y, NumBytes, Color1, Color2, SizeX, SizeY, Pitch, PixelSize );

		CurrentType = ChunkType;
		CurrentChunk = Chunk;
		NumBytes += Chunk->Size;
		Chunk = Chunk ? Chunk->NextChunk : NULL;
	}

	// Fill rest of pixels with black.
	INT NumRemainingPixels = SizeY * Pitch - (Y*Pitch + X);
	if ( NumRemainingPixels > 0 )
	{
		appMemzero( &TextureData[ Y*Pitch + X ], NumRemainingPixels * sizeof(FColor) );
	}

	return TRUE;
}

void FBestFitAllocator::GetMemoryLayout( TArray<FMemoryLayoutElement>& MemoryLayout )
{
	FMemoryChunk* Chunk = FirstChunk;
	MemoryLayout.Empty( 512 );
	while ( Chunk )
	{
		EMemoryElementType ChunkType = GetChunkType( Chunk );
		new (MemoryLayout) FMemoryLayoutElement( Chunk->Size, ChunkType );
		Chunk = Chunk->NextChunk;
	}
}

FBestFitAllocator::EMemoryElementType FBestFitAllocator::GetChunkType( FMemoryChunk* Chunk ) const
{
	EMemoryElementType ChunkType;
	if ( Chunk == NULL )
	{
		ChunkType = MET_Max;			// End-of-memory (n/a)
	}
	else if ( Chunk->IsRelocating() )
	{
		ChunkType = MET_Relocating;		// Currently being relocated (yellow)
	}
	else if ( Chunk->bIsAvailable )
	{
		ChunkType = MET_Free;			// Free (dark grey)
	}
	else if ( Chunk->HasReallocationRequest() )
	{
		if ( Chunk->ReallocationRequestNode->GetValue()->HasCompleted() )
		{
			ChunkType = MET_Resized;	// Has been resized but not finalized yet (dark green)
		}
		else
		{
			ChunkType = MET_Resizing;	// Allocated but wants a resize (green)
		}
	}
	else if ( CanRelocate( Chunk ) == FALSE )
	{
		ChunkType = MET_Locked;			// Allocated but can't me relocated at this time (locked) (red)
	}
	else
	{
		ChunkType = MET_Allocated;
	}
	return ChunkType;
}

/**
 * Scans the free chunks and returns the largest size you can allocate.
 *
 * @param OutNumFreeChunks	Upon return, contains the total number of free chunks. May be NULL.
 * @return					The largest size of all free chunks.
 */
INT FBestFitAllocator::GetLargestAvailableAllocation( INT* OutNumFreeChunks/*=NULL*/ )
{
	INT NumFreeChunks = 0;
	INT LargestChunkSize = 0;
	FMemoryChunk* FreeChunk = FirstFreeChunk;
	while (FreeChunk)
	{
		NumFreeChunks++;
		LargestChunkSize = Max<INT>( LargestChunkSize, FreeChunk->Size );
		FreeChunk = FreeChunk->NextFreeChunk;
	}
	if ( OutNumFreeChunks )
	{
		*OutNumFreeChunks = NumFreeChunks;
	}
	return LargestChunkSize;
}

/**
 * Fully defragments the memory and blocks until it's done.
 *
 * @param Stats			[out] Stats
 */
void FBestFitAllocator::DefragmentMemory( FRelocationStats& Stats )
{
	DOUBLE StartTime		= appSeconds();

	Tick( Stats, TRUE );

	DOUBLE MidTime			= appSeconds();

	if ( Stats.NumRelocations > 0 )
	{
		BlockOnFence();
	}

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	INT NumHolesBefore		= Stats.NumHoles;
	INT NumHolesAfter		= 0;
	INT LargestHoleBefore	= Stats.LargestHoleSize;
	INT LargestHoleAfter	= GetLargestAvailableAllocation(&NumHolesAfter);
	DOUBLE EndTime			= appSeconds();
	DOUBLE TotalDuration	= EndTime - StartTime;
	DOUBLE GPUDuration		= EndTime - MidTime;
	LargestHoleAfter		= GetLargestAvailableAllocation( &NumHolesAfter );
	debugf( NAME_DevMemory, TEXT("DEFRAG: %.1f ms (GPU %.1f ms), Available: %.3f MB, NumRelocations: %d, Relocated: %.3f MB, NumHolesBefore: %d, NumHolesAfter: %d, LargestHoleBefore: %.3f MB, LargestHoleAfter: %.3f MB"),
		TotalDuration*1000.0, GPUDuration*1000.0, AvailableMemorySize/1024.0f/1024.0f, Stats.NumRelocations, FLOAT(Stats.NumBytesRelocated)/1024.0f/1024.0f, NumHolesBefore, NumHolesAfter, FLOAT(LargestHoleBefore)/1024.f/1024.0f, FLOAT(LargestHoleAfter)/1024.0f/1024.0f );
#endif
}

/**
 * Merges any adjacent free chunks into the specified free chunk.
 * Doesn't affect the free-list sort order.
 *
 * @param	FreedChunk	Chunk that just became available.
 */
void FBestFitAllocator::Coalesce( FMemoryChunk* FreedChunk )
{
	check( FreedChunk );

	DWORD LatestSyncIndex = 0;
	INT LatestSyncSize = 0;
	INT LeftSize = 0;
	INT RightSize = 0;

	// Check if the previous chunk is available.
	FMemoryChunk* LeftChunk = FreedChunk->PreviousChunk;
	if ( LeftChunk && LeftChunk->bIsAvailable )
	{
		LeftSize = LeftChunk->Size;

		// Update relocation data for the left chunk.
		if ( LeftChunk->IsRelocating() )
		{
			LatestSyncIndex = LeftChunk->SyncIndex;
			LatestSyncSize = LeftChunk->SyncSize;
		}

		// Deletion will unlink.
		delete LeftChunk;
	}

	// Update relocation data for the middle chunk.
	if ( FreedChunk->IsRelocating() )
	{
		LatestSyncIndex = Max(LatestSyncIndex, FreedChunk->SyncIndex);
		LatestSyncSize = LeftSize + FreedChunk->SyncSize;
	}

	// Check if the next chunk is available.
	FMemoryChunk* RightChunk = FreedChunk->NextChunk;
	if ( RightChunk && RightChunk->bIsAvailable )
	{
		RightSize = RightChunk->Size;

		// Update relocation data for the right chunk.
		if ( RightChunk->IsRelocating() )
		{
			LatestSyncIndex = Max(LatestSyncIndex, RightChunk->SyncIndex);
			LatestSyncSize = LeftSize + FreedChunk->Size + RightChunk->SyncSize;
		}

		// Deletion will unlink.
		delete RightChunk;
	}

	// Merge.
	FreedChunk->Base -= LeftSize;
	FreedChunk->Size += LeftSize + RightSize;
	FreedChunk->SetSyncIndex( LatestSyncIndex, LatestSyncSize );
}

/**
 * Performs a benchmark of the allocator and outputs the result to the log.
 *
 * @param MinChunkSize	Minimum number of bytes per random chunk
 * @param MaxChunkSize	Maximum number of bytes per random chunk
 * @param FreeRatio		Free 0.0-1.0 % of the memory before benchmarking
 * @param LockRatio		Lock 0.0-1.0 % of the memory before benchmarking
 * @param bFullDefrag	Whether to test full defrag (TRUE) or continuous defrag (FALSE)
 * @param bSaveImages	Whether to save before/after images to hard disk (TexturePoolBenchmark-*.bmp)
 * @param Filename		[opt] Filename to a previously saved memory layout to use for benchmarking, or NULL
 */
void FBestFitAllocator::Benchmark( INT MinChunkSize, INT MaxChunkSize, FLOAT FreeRatio, FLOAT LockRatio, UBOOL bFullDefrag, UBOOL bSaveImages, const TCHAR* Filename )
{
#if !FINAL_RELEASE
	INT OriginalAllocatedMemorySize = AllocatedMemorySize;
	INT OriginalAvailableMemorySize = AvailableMemorySize;
	FRandomStream Rand( 0x1ee7c0de );
	TArray<void*> Chunks;
	TArray<FMemoryChunk*> OriginalChunks;
	TArray<FMemoryChunk*> TempLockedChunks;
	Chunks.Reserve(512);
	OriginalChunks.Reserve(512);
	TempLockedChunks.Reserve(512);

	// Lock existing chunks (that aren't already locked) and save them off.
	FMemoryChunk* OriginalChunk = FirstChunk;
	while ( OriginalChunk )
	{
		if ( !OriginalChunk->bIsAvailable && !OriginalChunk->bLocked )
		{
			OriginalChunk->bLocked = TRUE;
			OriginalChunks.AddItem( OriginalChunk );
		}
		OriginalChunk = OriginalChunk->NextChunk;
	}

	TArray<FMemoryLayoutElement> MemoryLayout;
	if ( GFileManager && Filename )
	{
		FArchive* Ar = GFileManager->CreateFileReader( Filename );
		if ( Ar )
		{
			*Ar << MemoryLayout;
			delete Ar;
		}
	}

	if ( MemoryLayout.Num() > 0 )
	{
		// Try to recreate the memory layout.
		for ( INT LayoutIndex=0; LayoutIndex < MemoryLayout.Num(); ++LayoutIndex )
		{
			FMemoryLayoutElement& LayoutElement = MemoryLayout( LayoutIndex );
			void* Ptr = Allocate( LayoutElement.Size, TRUE );
			if ( Ptr )
			{
				Chunks.AddItem( Ptr );
			}
			else
			{
				break;
			}
		}

		// Set the type for the elements
		for ( INT LayoutIndex=0; LayoutIndex < Chunks.Num(); ++LayoutIndex )
		{
			FMemoryLayoutElement& LayoutElement = MemoryLayout( LayoutIndex );
			switch ( LayoutElement.Type )
			{
				case MET_Free:
				{
					Free( Chunks( LayoutIndex ) );
					break;
				}
				case MET_Relocating:
				case MET_Resized:
				case MET_Locked:
				{
					FMemoryChunk* Chunk = PointerToChunkMap.FindRef( (PTRINT) Chunks( LayoutIndex ) );
					Chunk->bLocked = TRUE;
					TempLockedChunks.AddItem( Chunk );
					break;
				}
			}
		}
	}
	else
	{
		// Fill memory with random 32-1024 KB chunks.
		void* Ptr = NULL;
		do
		{
			INT ChunkSize = Align((appTrunc((MaxChunkSize - MinChunkSize)*Rand.GetFraction()) + MinChunkSize), 4096);
			Ptr = Allocate( ChunkSize, TRUE );
			if ( Ptr )
			{
				Chunks.AddItem( Ptr );
			}
		} while ( Ptr );

		// Free some of the pool to create random holes.
		INT SizeToFree = appTrunc(FreeRatio * MemorySize);
		while ( SizeToFree > 0 && Chunks.Num() > 0 )
		{
			INT ChunkIndex = appTrunc(Chunks.Num() * Rand.GetFraction());
			void* Ptr = Chunks( ChunkIndex );
			INT ChunkSize = GetAllocatedSize( Ptr );
			Free( Ptr );
			Chunks.RemoveSwap( ChunkIndex );
			SizeToFree -= ChunkSize;
		}

		// Lock some random chunks.
		INT SizeToLock = appTrunc(LockRatio * MemorySize);
		while ( SizeToLock > 0 && Chunks.Num() > 0 )
		{
			INT ChunkIndex = appTrunc(Chunks.Num() * Rand.GetFraction());
			void* ChunkPtr = Chunks( ChunkIndex );
			FMemoryChunk* Chunk = PointerToChunkMap.FindRef( (PTRINT) ChunkPtr );
			Chunk->bLocked = TRUE;
			TempLockedChunks.AddItem( Chunk );
			SizeToLock -= Chunk->Size;
		}
	}

	bBenchmarkMode = TRUE;

	// Save a "before" image
	TArray<FColor> Bitmap;
	if ( bSaveImages )
	{
		Bitmap.Add( 256*256 );
		GetTextureMemoryVisualizeData( Bitmap.GetTypedData(), 256, 256, 256*sizeof(FColor), 4096 );
		appCreateBitmap( TEXT("TexturePoolBenchmark-Before.bmp"), 256, 256, Bitmap.GetTypedData() );
	}

	// Keep Ticking until there are no more holes.
	INT NumTicks = 0;
	INT NumBytesRelocated = 0;
	TArray<INT> LargestHole;
	TArray<DOUBLE> Timers;
	LargestHole.Reserve( 4096 );
	Timers.Reserve( 4096 );
	INT Result = 1;
	DOUBLE StartTime = appSeconds();
	while ( Result && FirstFreeChunk && FirstFreeChunk->NextFreeChunk != NULL )
	{
		FBestFitAllocator::FRelocationStats Stats;
		Result = Tick( Stats, bFullDefrag );
		LargestHole.AddItem( Stats.LargestHoleSize );
		Timers.AddItem( appSeconds() );
		NumBytesRelocated += Stats.NumBytesRelocated;
		NumTicks++;

		// If benchmarking a specific memory layout, only iterate one time.
		if ( Filename )
		{
			Result = 0;
		}
	}
	DOUBLE Duration = appSeconds() - StartTime;
	FinishAllRelocations();

	// Time two "empty" runs.
	DOUBLE StartTime2 = appSeconds();
	for ( INT ExtraTiming=0; ExtraTiming < 2; ++ExtraTiming )
	{
		FBestFitAllocator::FRelocationStats Stats;
		Result = Tick( Stats, FALSE );
	}
	DOUBLE Duration2 = (appSeconds() - StartTime2) / 2;

	// Save an "after" image
	if ( bSaveImages )
	{
		GetTextureMemoryVisualizeData( Bitmap.GetTypedData(), 256, 256, 256*sizeof(FColor), 4096 );
		appCreateBitmap( TEXT("TexturePoolBenchmark-After.bmp"), 256, 256, Bitmap.GetTypedData() );
	}

	bBenchmarkMode = FALSE;

	// What's the defragmentation after 0%, 25%, 50%, 75%, 100% of the Ticks?
	FLOAT DefragmentationRatios[5];
	DefragmentationRatios[0] = LargestHole( appTrunc(LargestHole.Num() * 0.00f) ) / FLOAT(AvailableMemorySize);
	DefragmentationRatios[1] = LargestHole( appTrunc(LargestHole.Num() * 0.25f) ) / FLOAT(AvailableMemorySize);
	DefragmentationRatios[2] = LargestHole( appTrunc(LargestHole.Num() * 0.50f) ) / FLOAT(AvailableMemorySize);
	DefragmentationRatios[3] = LargestHole( appTrunc(LargestHole.Num() * 0.75f) ) / FLOAT(AvailableMemorySize);
	DefragmentationRatios[4] = LargestHole( appTrunc(LargestHole.Num() - 1) ) / FLOAT(AvailableMemorySize);
	FLOAT Durations[5];
	Durations[0] = Timers( appTrunc(Timers.Num() * 0.00f) ) - StartTime;
	Durations[1] = Timers( appTrunc(Timers.Num() * 0.25f) ) - StartTime;
	Durations[2] = Timers( appTrunc(Timers.Num() * 0.50f) ) - StartTime;
	Durations[3] = Timers( appTrunc(Timers.Num() * 0.75f) ) - StartTime;
	Durations[4] = Timers( appTrunc(Timers.Num() - 1) ) - StartTime;

	// Unlock our temp chunks we locked before.
	for ( INT ChunkIndex=0; ChunkIndex < TempLockedChunks.Num(); ++ChunkIndex )
	{
		FMemoryChunk* TempChunk = TempLockedChunks( ChunkIndex );
		TempChunk->bLocked = FALSE;
	}
	// Free all unlocked chunks (our temp chunks).
	FMemoryChunk* Chunk = FirstChunk;
	while ( Chunk )
	{
		if ( !Chunk->bIsAvailable && !Chunk->bLocked )
		{
			FreeChunk(Chunk, FALSE);
		}
		Chunk = Chunk->NextChunk;
	}
	// Unlock the original chunks we locked before.
	for ( INT ChunkIndex=0; ChunkIndex < OriginalChunks.Num(); ++ChunkIndex )
	{
		FMemoryChunk* OriginalChunk = OriginalChunks( ChunkIndex );
		OriginalChunk->bLocked = FALSE;
	}

	debugf( TEXT("Defrag benchmark: %.1f ms, %d ticks, additional %.1f ms/tick, %.1f MB relocated, defragmentation: %.1f%% (%.1f ms), %.1f%% (%.1f ms), %.1f%% (%.1f ms), %.1f%% (%.1f ms), %.1f%% (%.1f ms)"),
		Duration*1000.0,
		NumTicks,
		Duration2*1000.0,
		NumBytesRelocated/1024.0f/1024.0f,
		DefragmentationRatios[0]*100.0f,
		Durations[0]*1000.0,
		DefragmentationRatios[1]*100.0f,
		Durations[1]*1000.0,
		DefragmentationRatios[2]*100.0f,
		Durations[2]*1000.0,
		DefragmentationRatios[3]*100.0f,
		Durations[3]*1000.0,
		DefragmentationRatios[4]*100.0f,
		Durations[4]*1000.0 );
	GLog->Flush();

	check( OriginalAllocatedMemorySize == AllocatedMemorySize && OriginalAvailableMemorySize == AvailableMemorySize );
#endif
}


/*-----------------------------------------------------------------------------
	FBestFitAllocator implementation.
-----------------------------------------------------------------------------*/

/**
 * Allocates texture memory.
 *
 * @param	Size			Size of allocation
 * @param	bAllowFailure	Whether to allow allocation failure or not
 * @returns					Pointer to allocated memory
 */
void* FPresizedMemoryPool::Allocate(DWORD Size, UBOOL bAllowFailure)
{
	FScopeLock ScopeLock(&SynchronizationObject);

#if DUMP_ALLOC_FREQUENCY
	static INT AllocationCounter;
	if( ++AllocationCounter % DUMP_ALLOC_FREQUENCY == 0 )
	{
		FBestFitAllocator::DumpAllocs();
	}
#endif

	// Initialize allocator if it hasn't already.
	if (!FBestFitAllocator::IsInitialized())
	{
	}

	// actually do the allocation
	void* Pointer = FBestFitAllocator::Allocate(Size, bAllowFailure);

	// We ran out of memory. Instead of crashing rather corrupt the content and display an error message.
	if (Pointer == NULL)
	{
		if ( !bAllowFailure )
		{
			// Mark texture memory as having been corrupted.
			bIsCorrupted = TRUE;
		}

		// Use special pointer, which is being identified by free.
		Pointer = AllocationFailurePointer;
	}

#if LOG_EVERY_ALLOCATION
	INT AllocSize, AvailSize, PendingMemoryAdjustment;
	FBestFitAllocator::GetMemoryStats( AllocSize, AvailSize, PendingMemoryAdjustment );
	debugf(TEXT("Texture Alloc: %p  Size: %6i     Alloc: %8i Avail: %8i Pending: %8i"), Pointer, Size, AllocSize, AvailSize, PendingMemoryAdjustment );
#endif
	return Pointer;
}


/**
 * Frees texture memory allocated via Allocate
 *
 * @param	Pointer		Allocation to free
 */
void FPresizedMemoryPool::Free(void* Pointer)
{
	FScopeLock ScopeLock(&SynchronizationObject);

#if LOG_EVERY_ALLOCATION
	INT AllocSize, AvailSize, PendingMemoryAdjustment;
	FBestFitAllocator::GetMemoryStats( AllocSize, AvailSize, PendingMemoryAdjustment );
	debugf(TEXT("Texture Free : %p   Before free     Alloc: %8i Avail: %8i Pending: %8i"), Pointer, AllocSize, AvailSize, PendingMemoryAdjustment );
#endif
	// we never want to free the special pointer
	if (Pointer != AllocationFailurePointer)
	{
		// do the free
		FBestFitAllocator::Free(Pointer);
	}
}

/**
 * Returns the amount of memory allocated for the specified address.
 *
 * @param	Pointer		Pointer to check.
 * @return				Number of bytes allocated
 */
INT FPresizedMemoryPool::GetAllocatedSize( void* Pointer )
{
	return FBestFitAllocator::GetAllocatedSize( Pointer );
}

/**
 * Tries to reallocate texture memory in-place (without relocating),
 * by adjusting the base address of the allocation but keeping the end address the same.
 *
 * @param	OldBaseAddress	Pointer to the original allocation
 * @returns	New base address if it succeeded, otherwise NULL
 **/
void* FPresizedMemoryPool::Reallocate( void* OldBaseAddress, INT NewSize )
{
	FScopeLock ScopeLock(&SynchronizationObject);

	// Initialize allocator if it hasn't already.
	if (!FBestFitAllocator::IsInitialized())
	{
	}

	// we never want to reallocate the special pointer
	if ( IsValidTextureData( OldBaseAddress ) )
	{
		// Actually try to do the reallocation.
		return FBestFitAllocator::Reallocate(OldBaseAddress, NewSize);
	}
	else
	{
		return NULL;
	}
}

/**
 * Requests an async allocation or reallocation.
 * The caller must hold on to the request until it has been completed or canceled.
 *
 * @param ReallocationRequest	The request
 * @param bForceRequest			If TRUE, the request will be accepted even if there's currently not enough free space
 * @return						TRUE if the request was accepted
 */
UBOOL FPresizedMemoryPool::AsyncReallocate( FAsyncReallocationRequest* ReallocationRequest, UBOOL bForceRequest )
{
	FScopeLock ScopeLock(&SynchronizationObject);

	// Initialize allocator if it hasn't already.
	if (!FBestFitAllocator::IsInitialized())
	{
	}

	// we never want to reallocate the special pointer
	if (ReallocationRequest->GetOldBaseAddress() != AllocationFailurePointer)
	{
		// Actually try to do the reallocation.
		return FBestFitAllocator::AsyncReallocate( ReallocationRequest, bForceRequest );
	}
	else
	{
		return FALSE;
	}
}

/**
 * Partially defragments the memory and tries to process all async reallocation requests at the same time.
 * Call this once per frame.
 *
 * @param Stats			[out] Stats
 * @param bPanicDefrag	If TRUE, performs a full defrag and ignores all reallocation requests
 */
INT FPresizedMemoryPool::Tick( FRelocationStats& Stats )
{
	DWORD StartTime = appCycles();

	FScopeLock ScopeLock(&SynchronizationObject);

	// Initialize allocator if it hasn't already.
	if (!FBestFitAllocator::IsInitialized())
	{
	}

	INT Result = FBestFitAllocator::Tick( Stats, FALSE );

	TickCycles = appCycles() - StartTime - BlockedCycles;

	SET_DWORD_STAT( STAT_TexturePool_RelocatedSize, Stats.NumBytesRelocated );
	SET_DWORD_STAT( STAT_TexturePool_NumRelocations, Stats.NumRelocations );
	SET_DWORD_STAT( STAT_TexturePool_Allocated, AllocatedMemorySize );
	SET_DWORD_STAT( STAT_TexturePool_Free, AvailableMemorySize );
	SET_DWORD_STAT_FAST( STAT_TexturePool_LargestHole, Stats.LargestHoleSize );
	SET_DWORD_STAT( STAT_TexturePool_NumHoles, Stats.NumHoles );

	SET_DWORD_STAT( STAT_TexturePool_TotalAsyncReallocations, NumFinishedAsyncReallocations );
	SET_DWORD_STAT( STAT_TexturePool_TotalAsyncAllocations, NumFinishedAsyncAllocations );
	SET_DWORD_STAT( STAT_TexturePool_TotalAsyncCancellations, NumCanceledAsyncRequests );
	SET_CYCLE_COUNTER( STAT_TexturePool_DefragTime, TickCycles, 1 );

	return Result;
}
