/*=============================================================================
	FMallocProxySimpleTrack.h: Simple named-section tracking for allocations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FMALLOCPROXYSIMPLETRACK_H__
#define __FMALLOCPROXYSIMPLETRACK_H__

#include "TrackAllocSections.h"


class FMallocProxySimpleTrack : public FMalloc
{
protected:

	/** Maximum string size for a section name */
	const static UINT MaxSectionNameSize = 96;

	/** Amount of historic entries to track when dumping allocation info over time */
	const static UINT MaxStatsOverTime = 300;

	/** We store an FSimpleAlloc for every single outstanding memory allocation */
	struct FSimpleAlloc
	{
		/** The section ID this allocation belongs to (INDEX_NONE is usually 'Unknown') */
		UINT SectionID;

		/** Size of the allocation in bytes */
		UINT Size;
	};

	/** Allocator we are actually passing requests to. */
	FMalloc*							UsedMalloc;

	/** Map from allocation pointer to size. */
	TMap< PTRINT, FSimpleAlloc >		AllocToSizeMap;

	/** Total size of current allocations in bytes. */
	SIZE_T								TotalAllocSize;

	/** Total number of allocations. */
	SIZE_T								NumAllocs;

	/** Used to avoid re-entrancy (i.e. when doing map allocations) */
	UBOOL								bIsTracking;


	/** Contains information about allocations for a specific memory section */
	struct FMemSectionInfo
	{
		/** Section ID (this is also usually the hash Key from the container map) */
		UINT SectionID;

		/** Original name for this section */
		FString Name;

		/** Friendly name for the allocation count stat, for PIX */
		ANSICHAR AllocationCountStatName[ MaxSectionNameSize ];

		/** Friendly name for the memory allocated stat, for PIX */
		ANSICHAR MemAllocatedStatName[ MaxSectionNameSize ];

		/** Number of allocations outstanding */
		UINT AllocationCount;

		/** Number of bytes currently allocated (outstanding allocation) */
		UINT BytesAllocated;
	};

	IMPLEMENT_COMPARE_CONSTREF( FMemSectionInfo, FMallocProxySimpleTrack, { return appStrnicmp(*A.Name,*B.Name,A.Name.Len() + 1) >= 0 ? 1 : -1; } )


	/** Map of memory sections to their statistics */
	typedef TMap< INT, FMemSectionInfo > FMemSectionMap;
	FMemSectionMap MemSections;

	/** Stores the collated allocation stats for a single memory section over time */
	struct FSimpleAllocOverTime
	{
		/** The section ID for this allocation */
		UINT SectionID;

		/** Name of this allocation section, for sorting */
		FString Name;

		/** Sizes of section allocs for each dump */
		TArray< UINT > Sizes;

		/** Allocation counts of each section for each dump */
		TArray< UINT > AllocationCounts;

		FSimpleAllocOverTime()
		{
			SectionID = (UINT)INDEX_NONE;
			Sizes.Reserve( MaxStatsOverTime * 2 );
			AllocationCounts.Reserve( MaxStatsOverTime * 2 );
		}
	};

	IMPLEMENT_COMPARE_CONSTREF( FSimpleAllocOverTime, FMallocProxySimpleTrack, { return appStrnicmp(*A.Name,*B.Name,A.Name.Len() + 1) >= 0 ? 1 : -1; } )


	/** Array of cached allocation stats, for use with 'DUMPALLOCS' */
	TArray< FSimpleAllocOverTime > AllocationsOverTime;

public:

	FMallocProxySimpleTrack(FMalloc* InMalloc)
		: UsedMalloc( InMalloc ),
		  MemSections()
	{
		TotalAllocSize = 0;
		NumAllocs = 0;
		bIsTracking = FALSE;

		AllocationsOverTime.Reserve( MaxStatsOverTime );
	}


	virtual ~FMallocProxySimpleTrack(void)
	{
	}

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
		void* Pointer = UsedMalloc->Malloc(Size, Alignment);
		AddAllocation( Pointer, Size );
		return Pointer;
	}

	/** 
	 * Realloc
	 */
	void* Realloc( void* Ptr, DWORD NewSize, DWORD Alignment )
	{
		RemoveAllocation( Ptr );
		void* Pointer = UsedMalloc->Realloc(Ptr, NewSize, Alignment);
		AddAllocation( Pointer, NewSize );
		return Pointer;
	}

	/** 
	 * Free
	 */
	void Free( void* Ptr )
	{
		RemoveAllocation( Ptr );
		UsedMalloc->Free(Ptr);
	}

	/** 
	 * Physical alloc
	 */
	void* PhysicalAlloc( DWORD Size, ECacheBehaviour InCacheBehaviour )
	{
		void* Pointer = UsedMalloc->PhysicalAlloc(Size, InCacheBehaviour);
		AddAllocation( Pointer, Size );
		return Pointer;
	}

	/** 
	 * Physical free
	 */
	void PhysicalFree( void* Ptr )
	{
		RemoveAllocation( Ptr );
		UsedMalloc->PhysicalFree(Ptr);
	}

	/**
	 * Passes request for gathering memory allocations for both virtual and physical allocations
	 * on to used memory manager.
	 *
	 * @param FMemoryAllocationStats	[out] structure containing information about the size of allocations
	 */
	void GetAllocationInfo( FMemoryAllocationStats& MemStats )
	{
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
		// Copy the section list.  We don't want this to be modified out from underneath us.  Plus,
		// we'll be adding some 'fake' entries to the list.
		TArray< FMemSectionInfo > SectionList;
		SectionList.Reserve( MemSections.Num() );
		for( FMemSectionMap::TIterator SectionIt( MemSections ); SectionIt; ++SectionIt )
		{
			const FMemSectionInfo& CurSection = SectionIt.Value();
			SectionList.AddItem( CurSection );
		}

		FMemoryAllocationStats MemStats;
		GMalloc->GetAllocationInfo( MemStats );

		// always add the current Mem of the system to the list
		FMemSectionInfo AllocVM;
		{
			AllocVM.SectionID = (UINT)INDEX_NONE;
			AllocVM.Name = TEXT( "Virtual Memory" );
			appStrncpyANSI( AllocVM.AllocationCountStatName, "Mem Virtual Memory Allocs", MaxSectionNameSize );
			appStrncpyANSI( AllocVM.MemAllocatedStatName, "Mem Virtual Memory MB", MaxSectionNameSize );
			AllocVM.BytesAllocated = MemStats.CPUUsed;
			AllocVM.AllocationCount = 1;
		}
		SectionList.AddItem( AllocVM );

#if PS3
		FMemSectionInfo AllocGPU;
		{
			AllocGPU.SectionID = (UINT)INDEX_NONE;
			AllocGPU.Name = TEXT( "GPU Memory" );
			appStrncpyANSI( AllocGPU.AllocationCountStatName, "Mem GPU Memory Allocs", MaxSectionNameSize );
			appStrncpyANSI( AllocGPU.MemAllocatedStatName, "Mem GPU Memory MB", MaxSectionNameSize );
			AllocGPU.BytesAllocated = MemStats.GPUUsed;
			AllocGPU.AllocationCount = 1;
		}
		SectionList.AddItem( AllocGPU );
#endif

		Sort<USE_COMPARE_CONSTREF(FMemSectionInfo,FMallocProxySimpleTrack)>(&SectionList(0),SectionList.Num());

		// Now print out amount allocated for each allocating location.
		Ar.Logf( TEXT("===== Dumping stat allocations [Section],[Size],[NumAllocs]") );			
		for(INT AllocIndex = 0; AllocIndex < SectionList.Num(); AllocIndex++)
		{
			const FMemSectionInfo& Alloc = SectionList(AllocIndex);
			Ar.Logf( TEXT("%s,%d,%d"), *Alloc.Name, Alloc.BytesAllocated, Alloc.AllocationCount );
		}

		// this is O(N^2) as we currently have a TArray with the allocation data
		for( INT AllocIndex = 0; AllocIndex < SectionList.Num(); ++AllocIndex )
		{
			const FMemSectionInfo& Alloc = SectionList(AllocIndex);
			UBOOL bFound = FALSE;
			for( INT OverTimeIndex = 0; OverTimeIndex < AllocationsOverTime.Num(); ++OverTimeIndex )
			{
				if( AllocationsOverTime(OverTimeIndex).SectionID == Alloc.SectionID )
				{
					//warnf( TEXT( "  MATCH!") );
					FSimpleAllocOverTime& AllocOverTime = AllocationsOverTime(OverTimeIndex);
					AllocOverTime.Sizes.AddItem( Alloc.BytesAllocated );
					AllocOverTime.AllocationCounts.AddItem( Alloc.AllocationCount );
					bFound = TRUE;
					break;
				}
			}
			// we need to add this stat to the overtime
			if( bFound == FALSE )
			{
				//warnf( TEXT( " NEW! %s"), Alloc.Section );
				FSimpleAllocOverTime NewDatum;
				NewDatum.SectionID = Alloc.SectionID;
				NewDatum.Name = Alloc.Name;
				NewDatum.Sizes.AddItem( Alloc.BytesAllocated );
				NewDatum.AllocationCounts.AddItem( Alloc.AllocationCount );

				AllocationsOverTime.AddItem( NewDatum );
			}
		}

		Sort<USE_COMPARE_CONSTREF(FSimpleAllocOverTime,FMallocProxySimpleTrack)>(&AllocationsOverTime(0),AllocationsOverTime.Num());

		Ar.Logf( TEXT("===== Dumping stat allocations [Section],[Size]") );		
		for( INT OverTimeIndex = 0; OverTimeIndex < AllocationsOverTime.Num(); ++OverTimeIndex )
		{
			const FSimpleAllocOverTime& AllocOverTime = AllocationsOverTime(OverTimeIndex);

			FString Tmp;
			Tmp += FString::Printf( TEXT("%s,"), *AllocOverTime.Name );
			for( INT Idx = 0; Idx < AllocOverTime.Sizes.Num(); ++Idx )
			{
				Tmp += FString::Printf( TEXT("%d,"), AllocOverTime.Sizes(Idx) );
			}
			Ar.Logf( TEXT("%s"), *Tmp );
		}

		Ar.Logf( TEXT("===== Dumping stat allocations [Section],[NumAllocs]") );	
		for( INT OverTimeIndex = 0; OverTimeIndex < AllocationsOverTime.Num(); ++OverTimeIndex )
		{
			const FSimpleAllocOverTime& AllocOverTime = AllocationsOverTime(OverTimeIndex);

			FString Tmp;
			Tmp += FString::Printf( TEXT("%s,"), *AllocOverTime.Name );
			for( INT Idx = 0; Idx < AllocOverTime.AllocationCounts.Num(); ++Idx )
			{
				Tmp += FString::Printf( TEXT("%d,"), AllocOverTime.AllocationCounts(Idx) );
			}
			Ar.Logf( TEXT("%s"), *Tmp );
		}
	}

protected:

	/**
	 * Add allocation to keep track of.
	 *
	 * @param	Pointer		Allocation
	 * @param	Size		Allocation size in bytes
	 */
	void AddAllocation( void* Pointer, SIZE_T Size )
	{
		if(Pointer != NULL && !GExitPurge && !bIsTracking)
		{
			const UINT SectionID = GAllocSectionState.GetCurrentSectionID();
			
			bIsTracking = TRUE;

			NumAllocs++;
			TotalAllocSize += Size;

			FSimpleAlloc Alloc;
			Alloc.SectionID = SectionID;
			Alloc.Size = Size;

			AllocToSizeMap.Set( (PTRINT) Pointer, Alloc );


			// Update section stats
			FMemSectionInfo* SectionInfo = MemSections.Find( SectionID );
			if( SectionInfo == NULL )
			{
				// Add new section
				FMemSectionInfo NewSectionInfo;

				NewSectionInfo.SectionID = SectionID;
				NewSectionInfo.Name = GAllocSectionState.GetCurrentSectionName();

				appMemzero( NewSectionInfo.AllocationCountStatName, sizeof( NewSectionInfo.AllocationCountStatName ) );
				appMemzero( NewSectionInfo.MemAllocatedStatName, sizeof( NewSectionInfo.MemAllocatedStatName ) );
				{
					// Trim whitespace
					FString FriendlyName = NewSectionInfo.Name;
					FriendlyName.Trim();

					// Strip 'STAT_' prefix if we find it
					if( FriendlyName.StartsWith( TEXT( "STAT_" ) ) )	
					{
						FriendlyName = FriendlyName.Right( FriendlyName.Len() - 5 );
					}

					// Add 'Mem_' prefix
					FriendlyName = FString( TEXT( "Mem " ) ) + FriendlyName;

					// Limit max string length
					FriendlyName = FriendlyName.Left( MaxSectionNameSize - 1 );	

					// Convert to ANSI chars (for PIX)
					appStrncpyANSI( NewSectionInfo.AllocationCountStatName, TCHAR_TO_ANSI( *( FriendlyName + TEXT( " Allocs" ) ) ), MaxSectionNameSize );
					appStrncpyANSI( NewSectionInfo.MemAllocatedStatName, TCHAR_TO_ANSI( *( FriendlyName + TEXT( " MB" ) ) ), MaxSectionNameSize );
				}

				NewSectionInfo.AllocationCount = 0;
				NewSectionInfo.BytesAllocated = 0;

				SectionInfo = &MemSections.Set( SectionID, NewSectionInfo );
			}
			

			// Update stats for this section
			SectionInfo->AllocationCount += 1;
			SectionInfo->BytesAllocated += Size;


			bIsTracking = FALSE;
		}
	}
	
	/**
	 * Remove allocation from list to track.
	 *
	 * @param	Pointer		Allocation
	 */
	void RemoveAllocation( void* Pointer )
	{
		if(!GExitPurge && !bIsTracking)
		{
			if(Pointer)
			{
				bIsTracking = TRUE;


				FSimpleAlloc* AllocPtr = AllocToSizeMap.Find( (PTRINT) Pointer );
				check(AllocPtr);


				// Update section stats
				FMemSectionInfo* SectionInfo = MemSections.Find( AllocPtr->SectionID );
				if( SectionInfo != NULL )
				{
					// Update stats for this section
					SectionInfo->AllocationCount -= 1;
					SectionInfo->BytesAllocated -= AllocPtr->Size;
				}
				else
				{
					// Unknown section, we'll just ignore it
				}


				NumAllocs--;
				TotalAllocSize -= AllocPtr->Size;

				AllocToSizeMap.Remove( (PTRINT) Pointer );


				bIsTracking = FALSE;
			}
		}
	}

	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar )
	{
		return UsedMalloc->Exec(Cmd, Ar);
	}

	/**
	 * Validates the allocator's heap
	 */
	UBOOL ValidateHeap()
	{
		return UsedMalloc->ValidateHeap();
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

	/** Called every game thread tick */
	void Tick( FLOAT DeltaTime )
	{
#if EXPORT_MEM_STATS_TO_PIX
		// Iterate over all sections
		for( FMemSectionMap::TIterator SectionIt( MemSections ); SectionIt != NULL; ++SectionIt )
		{
			const FMemSectionInfo& CurSection = SectionIt.Value();

			{
				FString ExportedCounterName( CurSection.AllocationCountStatName );
				appSetCounterValue( *ExportedCounterName, ( FLOAT )CurSection.AllocationCount );
			}
			{
				FString ExportedCounterName( CurSection.MemAllocatedStatName );
				appSetCounterValue( *ExportedCounterName, ( FLOAT )( CurSection.BytesAllocated / ( 1024.0f * 1024.0f ) ) );
			}
		}


		// Also add stat totals
		{
			appSetCounterValue( TEXT( "Mem <Total Tracked> Allocs" ), ( FLOAT )NumAllocs );
		}
		{
			appSetCounterValue( TEXT( "Mem <Total Tracked> MB" ), ( FLOAT )( TotalAllocSize / ( 1024.0f * 1024.0f ) ) );
		}
#endif
	}

	
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
	virtual void DecTrackedOpenGLMemory(UINT Size, OpenGLBufferTrackingType trackingType) 
	{ 
		UsedMalloc->DecTrackedOpenGLMemory(Size, trackingType);
	}
#endif

};

#endif
