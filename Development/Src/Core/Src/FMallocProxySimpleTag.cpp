/*=============================================================================
	FMallocProxySimpleTag.cpp: Simple tag based allocation tracker.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#include "FMallocProxySimpleTag.h"

/** Current active tag.	*/
INT	FMallocProxySimpleTag::CurrentTag;

/**
 * Add allocation to keep track of.
 *
 * @param	Pointer		Allocation
 * @param	Size		Allocation size in bytes
 * @param	Tag			Tag to use for original tag
 */
void FMallocProxySimpleTag::AddAllocation( void* Pointer, SIZE_T Size, INT OriginalTag )
{
	if( !GExitPurge && !bIsTracking && Pointer )
	{
		bIsTracking = TRUE;

		// Create active allocation for tracking and add it to map.
		FAllocInfo AllocInfo;
		AllocInfo.Size			= Size;
		AllocInfo.OriginalTag	= OriginalTag;
		AllocInfo.CurrentTag	= CurrentTag;
		AllocInfo.Count			= 1;
		AllocToInfoMap.Set( (PTRINT) Pointer, AllocInfo );

		bIsTracking = FALSE;
	}
}

/**
 * Remove allocation from list to track.
 *
 * @param	Pointer		Allocation
 * @return	Original tag of allocation
 */
INT FMallocProxySimpleTag::RemoveAllocation( void* Pointer )
{
	INT OriginalTag = 0;

	if( !GExitPurge && !bIsTracking && Pointer )
	{
		bIsTracking = TRUE;

		// Find existing entry to return it's original tag.
		FAllocInfo* AllocPtr = AllocToInfoMap.Find( (PTRINT) Pointer );
		check(AllocPtr);
		OriginalTag = AllocPtr->OriginalTag;
		// Remove from active allocation map.
		AllocToInfoMap.Remove( (PTRINT) Pointer );

		bIsTracking = FALSE;
	}

	return OriginalTag;
}



// FMalloc interface.

FMallocProxySimpleTag::FMallocProxySimpleTag(FMalloc* InMalloc)
:	UsedMalloc( InMalloc )
{
	TotalAllocSize	= 0;
	TotalAllocCount = 0;
	bIsTracking		= FALSE;
}

void* FMallocProxySimpleTag::Malloc( DWORD Size, DWORD Alignment )
{
	void* Pointer = UsedMalloc->Malloc(Size, Alignment);
	AddAllocation( Pointer, Size, CurrentTag );
	return Pointer;
}

void* FMallocProxySimpleTag::Realloc( void* Ptr, DWORD NewSize, DWORD Alignment )
{
	INT OriginalTag = RemoveAllocation( Ptr );
	void* Pointer = UsedMalloc->Realloc(Ptr, NewSize, Alignment);
	AddAllocation( Pointer, NewSize, OriginalTag );
	return Pointer;
}

void FMallocProxySimpleTag::Free( void* Ptr )
{
	RemoveAllocation( Ptr );
	UsedMalloc->Free(Ptr);
}

void* FMallocProxySimpleTag::PhysicalAlloc( DWORD Size, ECacheBehaviour InCacheBehaviour )
{
	void* Pointer = UsedMalloc->PhysicalAlloc(Size, InCacheBehaviour);
	AddAllocation( Pointer, Size, CurrentTag );
	return Pointer;
}

void FMallocProxySimpleTag::PhysicalFree( void* Ptr )
{
	RemoveAllocation( Ptr );
	UsedMalloc->PhysicalFree(Ptr);
}

/**
 * Dumps details about all allocations to an output device
 *
 * @param Ar	[in] Output device
 */
void FMallocProxySimpleTag::DumpAllocations( FOutputDevice& Ar )
{
	// Group allocations by tags.
	TArray<FAllocInfo> GroupedAllocs;

	// Iterate over all allocations
	for ( TMap<PTRINT,FAllocInfo>::TIterator It(AllocToInfoMap); It; ++It )
	{
		FAllocInfo AllocInfo = It.Value();

		// See if we have a matching group.
		INT MatchingGroupIndex = INDEX_NONE;
		for( INT GroupIndex=0; GroupIndex<GroupedAllocs.Num(); GroupIndex++ )
		{
			// Compare both current and original tag for grouping.
			if( AllocInfo.CurrentTag  == GroupedAllocs(GroupIndex).CurrentTag
				&&	AllocInfo.OriginalTag == GroupedAllocs(GroupIndex).OriginalTag )
			{
				MatchingGroupIndex = GroupIndex;
				break;
			}
		}

		// If we found it - update size and count.
		if( MatchingGroupIndex != INDEX_NONE )
		{
			FAllocInfo& FoundAlloc = GroupedAllocs(MatchingGroupIndex);
			FoundAlloc.Size += AllocInfo.Size;
			FoundAlloc.Count++;
		}
		// If we didn't add to array.
		else
		{
			GroupedAllocs.AddItem(AllocInfo);
		}
	}

	// Now print out amount allocated for each group.
	Ar.Logf( TEXT(",OriginalTag,CurrentTag,Size,Count") );			
	for( INT GroupIndex=0; GroupIndex<GroupedAllocs.Num(); GroupIndex++ )
	{
		FAllocInfo& AllocInfo = GroupedAllocs(GroupIndex);
		Ar.Logf( TEXT(",%i,%i,%i,%i"), AllocInfo.OriginalTag, AllocInfo.CurrentTag, AllocInfo.Size, AllocInfo.Count );
	}
}

UBOOL FMallocProxySimpleTag::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	return UsedMalloc->Exec(Cmd, Ar);
}


