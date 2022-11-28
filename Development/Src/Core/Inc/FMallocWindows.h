/*=============================================================================
	FMallocWindows.h: Windows optimized memory allocator.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
		* 16 byte memory alignment changes by Daniel Vogel
=============================================================================*/

#define MEM_TIME(st)

//
// Optimized Windows virtual memory allocator.
//
class FMallocWindows : public FMalloc
{
private:
	// Counts.
	enum {POOL_COUNT = 43     };
	enum {POOL_MAX   = 32768+1};

	// Forward declares.
	struct FFreeMem;
	struct FPoolTable;

	// Memory pool info. 32 bytes.
	struct FPoolInfo
	{
		DWORD	    Bytes;		// Bytes allocated for pool.
		DWORD		OsBytes;	// Bytes aligned to page size.
		DWORD       Taken;      // Number of allocated elements in this pool, when counts down to zero can free the entire pool.
		BYTE*       Mem;		// Memory base.
		FPoolTable* Table;		// Index of pool.
		FFreeMem*   FirstMem;   // Pointer to first free memory in this pool.
		FPoolInfo*	Next;
		FPoolInfo**	PrevLink;

		void Link( FPoolInfo*& Before )
		{
			if( Before )
				Before->PrevLink = &Next;
			Next     = Before;
			PrevLink = &Before;
			Before   = this;
		}
		void Unlink()
		{
			if( Next )
				Next->PrevLink = PrevLink;
			*PrevLink = Next;
		}
	};

	// Information about a piece of free memory. 8 bytes.
	struct FFreeMem
	{
		FFreeMem*	Next;		// Next or MemLastPool[], always in order by pool.
		DWORD		Blocks;		// Number of consecutive free blocks here, at least 1.
		FPoolInfo* GetPool()
		{
			return (FPoolInfo*)((INT)this & 0xffff0000);
		}
	};

	// Pool table.
	struct FPoolTable
	{
		FPoolInfo* FirstPool;
		FPoolInfo* ExaustedPool;
		DWORD      BlockSize;
	};

	// Variables.
	FPoolTable  PoolTable[POOL_COUNT], OsTable;
	FPoolInfo*	PoolIndirect[256];
	FPoolTable* MemSizeToPoolTable[POOL_MAX];
	INT         MemInit;
	INT			OsCurrent,OsPeak,UsedCurrent,UsedPeak,CurrentAllocs,TotalAllocs;
	DOUBLE		MemTime;

	// Implementation.
	void OutOfMemory()
	{
		appErrorf( *LocalizeError("OutOfMemory",TEXT("Core")) );
	}
	FPoolInfo* CreateIndirect()
	{
		FPoolInfo* Indirect = (FPoolInfo*)VirtualAlloc( NULL, 256*sizeof(FPoolInfo), MEM_COMMIT, PAGE_READWRITE );
		if( !Indirect )
			OutOfMemory();
		return Indirect;
	}

public:
	// FMalloc interface.
	FMallocWindows()
	:	MemInit( 0 )
	,	OsCurrent		( 0 )
	,	OsPeak			( 0 )
	,	UsedCurrent		( 0 )
	,	UsedPeak		( 0 )
	,	CurrentAllocs	( 0 )
	,	TotalAllocs		( 0 )
	,	MemTime			( 0.0 )
	{}
	void* Malloc( DWORD Size )
	{
		checkSlow(Size>=0);
		checkSlow(MemInit);
		MEM_TIME(MemTime -= appSeconds());
		STAT(CurrentAllocs++);
		STAT(TotalAllocs++);
		FFreeMem* Free;
		if( Size<POOL_MAX )
		{
			// Allocate from pool.
			FPoolTable* Table = MemSizeToPoolTable[Size];
			checkSlow(Size<=Table->BlockSize);
			FPoolInfo* Pool = Table->FirstPool;
			if( !Pool )
			{
				// Must create a new pool.
				DWORD Blocks  = 65536 / Table->BlockSize;
				DWORD Bytes   = Blocks * Table->BlockSize;
				checkSlow(Blocks>=1);
				checkSlow(Blocks*Table->BlockSize<=Bytes);

				// Allocate memory.
				Free = (FFreeMem*)VirtualAlloc( NULL, Bytes, MEM_COMMIT, PAGE_READWRITE );
				if( !Free )
					OutOfMemory();

				// Create pool in the indirect table.
				FPoolInfo*& Indirect = PoolIndirect[((DWORD)Free>>24)];
				if( !Indirect )
					Indirect = CreateIndirect();
				Pool = &Indirect[((DWORD)Free>>16)&255];

				// Init pool.
				Pool->Link( Table->FirstPool );
				Pool->Mem            = (BYTE*)Free;
				Pool->Bytes		     = Bytes;
				Pool->OsBytes		 = Align(Bytes,GPageSize);
				STAT(OsPeak = Max(OsPeak,OsCurrent+=Pool->OsBytes));
				Pool->Table		     = Table;
				Pool->Taken			 = 0;
				Pool->FirstMem       = Free;

				// Create first free item.
				Free->Blocks         = Blocks;
				Free->Next           = NULL;
			}

			// Pick first available block and unlink it.
			Pool->Taken++;
			checkSlow(Pool->FirstMem);
			checkSlow(Pool->FirstMem->Blocks>0);
			Free = (FFreeMem*)((BYTE*)Pool->FirstMem + --Pool->FirstMem->Blocks * Table->BlockSize);
			if( Pool->FirstMem->Blocks==0 )
			{
				Pool->FirstMem = Pool->FirstMem->Next;
				if( !Pool->FirstMem )
				{
					// Move to exausted list.
					Pool->Unlink();
					Pool->Link( Table->ExaustedPool );
				}
			}
			STAT(UsedPeak = Max(UsedPeak,UsedCurrent+=Table->BlockSize));
		}
		else
		{
			// Use OS for large allocations.
			INT AlignedSize = Align(Size,GPageSize);
			Free = (FFreeMem*)VirtualAlloc( NULL, AlignedSize, MEM_COMMIT, PAGE_READWRITE );
			if( !Free )
				OutOfMemory();
			checkSlow(!((SIZE_T)Free&65535));

			// Create indirect.
			FPoolInfo*& Indirect = PoolIndirect[((DWORD)Free>>24)];
			if( !Indirect )
				Indirect = CreateIndirect();

			// Init pool.
			FPoolInfo* Pool = &Indirect[((DWORD)Free>>16)&255];
			Pool->Mem		= (BYTE*)Free;
			Pool->Bytes		= Size;
			Pool->OsBytes	= AlignedSize;
			Pool->Table		= &OsTable;
			STAT(OsPeak   = Max(OsPeak,  OsCurrent+=AlignedSize));
			STAT(UsedPeak = Max(UsedPeak,UsedCurrent+=Size));
		}
		MEM_TIME(MemTime += appSeconds());
		return Free;
	}
	void* Realloc( void* Ptr, DWORD NewSize )
	{
		checkSlow(MemInit);
		MEM_TIME(MemTime -= appSeconds());
		check(NewSize>=0);
		void* NewPtr = Ptr;
		if( Ptr && NewSize )
		{
			checkSlow(MemInit);
			FPoolInfo* Pool = &PoolIndirect[(DWORD)Ptr>>24][((DWORD)Ptr>>16)&255];
			if( Pool->Table!=&OsTable )
			{
				// Allocated from pool, so grow or shrink if necessary.
				if( NewSize>Pool->Table->BlockSize || MemSizeToPoolTable[NewSize]!=Pool->Table )
				{
					NewPtr = Malloc( NewSize );
					appMemcpy( NewPtr, Ptr, Min(NewSize,Pool->Table->BlockSize) );
					Free( Ptr );
				}
			}
			else
			{
				// Allocated from OS.
				checkSlow(!((INT)Ptr&65535));
				if( NewSize>Pool->OsBytes || NewSize*3<Pool->OsBytes*2 )
				{
					// Grow or shrink.
					NewPtr = Malloc( NewSize );
					appMemcpy( NewPtr, Ptr, Min(NewSize,Pool->Bytes) );
					Free( Ptr );
				}
				else
				{
					// Keep as-is, reallocation isn't worth the overhead.
					Pool->Bytes = NewSize;
				}
			}
		}
		else if( Ptr == NULL )
		{
			NewPtr = Malloc( NewSize );
		}
		else
		{
			Free( Ptr );
			NewPtr = NULL;
		}
		MEM_TIME(MemTime += appSeconds());
		return NewPtr;
	}
	void Free( void* Ptr )
	{
		if( !Ptr )
			return;
		checkSlow(MemInit);
		MEM_TIME(MemTime -= appSeconds());
		STAT(CurrentAllocs--);

		// Windows version.
		FPoolInfo* Pool = &PoolIndirect[(DWORD)Ptr>>24][((DWORD)Ptr>>16)&255];
		checkSlow(Pool->Bytes!=0);
		if( Pool->Table!=&OsTable )
		{
			// If this pool was exausted, move to available list.
			if( !Pool->FirstMem )
			{
				Pool->Unlink();
				Pool->Link( Pool->Table->FirstPool );
			}

			// Free a pooled allocation.
			FFreeMem* Free		= (FFreeMem *)Ptr;
			Free->Blocks		= 1;
			Free->Next			= Pool->FirstMem;
			Pool->FirstMem		= Free;
			STAT(UsedCurrent   -= Pool->Table->BlockSize);

			// Free this pool.
			checkSlow(Pool->Taken>=1);
			if( --Pool->Taken == 0 )
			{
				// Free the OS memory.
				Pool->Unlink();
				verify( VirtualFree( Pool->Mem, 0, MEM_RELEASE )!=0 );
				STAT(OsCurrent -= Pool->OsBytes);
			}
		}
		else
		{
			// Free an OS allocation.
			checkSlow(!((INT)Ptr&65535));
			STAT(UsedCurrent -= Pool->Bytes);
			STAT(OsCurrent   -= Pool->OsBytes);
			verify( VirtualFree( Ptr, 0, MEM_RELEASE )!=0 );
		}
		MEM_TIME(MemTime += appSeconds());
	}
	/**
	 * Gathers memory allocations for both virtual and physical allocations.
	 *
	 * @param Virtual	[out] size of virtual allocations
	 * @param Physical	[out] size of physical allocations	
	 */
	void GetAllocationInfo( SIZE_T& Virtual, SIZE_T& Physical )
	{
		Virtual		= OsCurrent;
		Physical	= 0;
	}
	void DumpAllocs()
	{
		FMallocWindows::HeapCheck();

		STAT(debugf( TEXT("Memory Allocation Status") ));
		STAT(debugf( TEXT("Curr Memory % 5.3fM / % 5.3fM"), UsedCurrent/1024.0/1024.0, OsCurrent/1024.0/1024.0 ));
		STAT(debugf( TEXT("Peak Memory % 5.3fM / % 5.3fM"), UsedPeak   /1024.0/1024.0, OsPeak   /1024.0/1024.0 ));
		STAT(debugf( TEXT("Allocs      % 6i Current / % 6i Total"), CurrentAllocs, TotalAllocs ));
		MEM_TIME(debugf( "Seconds     % 5.3f", MemTime ));
		MEM_TIME(debugf( "MSec/Allc   % 5.5f", 1000.0 * MemTime / MemAllocs ));

#if STATS
		if( ParseParam(appCmdLine(), TEXT("MEMSTAT")) )
		{
			debugf( TEXT("Block Size Num Pools Cur Allocs Total Allocs Mem Used Mem Waste Efficiency") );
			debugf( TEXT("---------- --------- ---------- ------------ -------- --------- ----------") );
			INT TotalPoolCount  = 0;
			INT TotalAllocCount = 0;
			INT TotalMemUsed    = 0;
			INT TotalMemWaste   = 0;
			for( INT i=0; i<POOL_COUNT; i++ )
			{
				FPoolTable* Table = &PoolTable[i];
				INT PoolCount=0;
				INT AllocCount=0;
				INT MemUsed=0;
				for( INT i=0; i<2; i++ )
				{
					for( FPoolInfo* Pool=(i?Table->FirstPool:Table->ExaustedPool); Pool; Pool=Pool->Next )
					{
						PoolCount++;
						AllocCount += Pool->Taken;
						MemUsed += Pool->Bytes;
						INT FreeCount=0;
						for( FFreeMem* Free=Pool->FirstMem; Free; Free=Free->Next )
							FreeCount += Free->Blocks;
					}
				}
				INT MemWaste = MemUsed - AllocCount*Table->BlockSize;
				debugf
				(
					TEXT("% 10i % 9i % 10i % 11iK % 7iK % 8iK % 9.2f%%"),
					Table->BlockSize,
					PoolCount,
					AllocCount,
					0,
					MemUsed /1024,
					MemWaste/1024,
					MemUsed ? 100.0 * MemUsed / (MemUsed+MemWaste) : 100.0
				);
				TotalPoolCount  += PoolCount;
				TotalAllocCount += AllocCount;
				TotalMemUsed    += MemUsed;
				TotalMemWaste   += MemWaste;
			}
			debugf
			(
				TEXT("BlkOverall % 9i % 10i % 11iK % 7iK % 8iK % 9.2f%%"),
				TotalPoolCount,
				TotalAllocCount,
				0,
				TotalMemUsed /1024,
				TotalMemWaste/1024,
				TotalMemUsed ? 100.0 * TotalMemUsed / (TotalMemUsed+TotalMemWaste) : 100.0
			);
		}
#endif
	}
	void HeapCheck()
	{
		for( INT i=0; i<POOL_COUNT; i++ )
		{
			FPoolTable* Table = &PoolTable[i];
			for( FPoolInfo** PoolPtr=&Table->FirstPool; *PoolPtr; PoolPtr=&(*PoolPtr)->Next )
			{
				FPoolInfo* Pool=*PoolPtr;
				check(Pool->PrevLink==PoolPtr);
				check(Pool->FirstMem);
				for( FFreeMem* Free=Pool->FirstMem; Free; Free=Free->Next )
					check(Free->Blocks>0);
			}
			for( FPoolInfo** PoolPtr=&Table->ExaustedPool; *PoolPtr; PoolPtr=&(*PoolPtr)->Next )
			{
				FPoolInfo* Pool=*PoolPtr;
				check(Pool->PrevLink==PoolPtr);
				check(!Pool->FirstMem);
			}
		}
	}
	void Init( UBOOL Reset )
	{
		check(!MemInit);
		MemInit = 1;

		// Get OS page size.
#ifndef XBOX
		SYSTEM_INFO SI;
		GetSystemInfo( &SI );
		GPageSize = SI.dwPageSize;
		check(!(GPageSize&(GPageSize-1)));
#else
		GPageSize = 4096;
#endif

		// Init tables.
		OsTable.FirstPool    = NULL;
		OsTable.ExaustedPool = NULL;
		OsTable.BlockSize    = 0;

		PoolTable[0].FirstPool    = NULL;
		PoolTable[0].ExaustedPool = NULL;
		PoolTable[0].BlockSize    = 8;
		for( DWORD i=1; i<6; i++ )
		{
			PoolTable[i].FirstPool    = NULL;
			PoolTable[i].ExaustedPool = NULL;
			PoolTable[i].BlockSize    = (8<<(i>>2)) + (2<<(i-1));
		}
		for( DWORD i=6; i<POOL_COUNT; i++ )
		{
			PoolTable[i].FirstPool    = NULL;
			PoolTable[i].ExaustedPool = NULL;
			PoolTable[i].BlockSize    = (4+((i+6)&3)) << (1+((i+6)>>2));
		}
		for( DWORD i=0; i<POOL_MAX; i++ )
		{
			DWORD Index;
			for( Index=0; PoolTable[Index].BlockSize<i; Index++ );
			checkSlow(Index<POOL_COUNT);
			MemSizeToPoolTable[i] = &PoolTable[Index];
		}
		for( DWORD i=0; i<256; i++ )
		{
			PoolIndirect[i] = NULL;
		}
		check(POOL_MAX-1==PoolTable[POOL_COUNT-1].BlockSize);
	}
	void Exit()
	{
		check(MemInit);
		MemInit = 0;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

