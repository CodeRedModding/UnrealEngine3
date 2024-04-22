/*=============================================================================
	MemoryBase.h: Base memory management definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MEMORYBASE_H__
#define __MEMORYBASE_H__

/** The global memory allocator. */
extern class FMalloc* GMalloc;

/** Global FMallocProfiler variable to allow multiple malloc profilers to communicate. */
MALLOC_PROFILER( extern class FMallocProfiler* GMallocProfiler; )

// Memory allocator.

enum ECacheBehaviour
{
	CACHE_Normal		= 0,
	CACHE_WriteCombine	= 1,
	CACHE_None			= 2,
	CACHE_Virtual		= 3,
	CACHE_MAX			// needs to be last entry
};

/** 
 * Struct used to hold memory allocation statistics. 
 * NOTE: Be sure to check hardcoded/mirrored usage found in MemoryProfiler2 StreamToken.cs
 */
struct FMemoryAllocationStats
{
	SIZE_T	TotalUsed;			/** The total amount of memory used by the game. */	
	SIZE_T	TotalAllocated;		/** The total amount of memory allocated from the OS. */

	// Virtual memory for Xbox and PC / Main memory for PS3 (tracked in the allocators)
	SIZE_T	CPUUsed;			/** The allocated in use by the application virtual memory. */
	SIZE_T	CPUSlack;			/** The allocated from the OS/allocator, but not used by the application. */
	SIZE_T	CPUWaste;			/** Alignment waste from a pooled allocator plus book keeping overhead. */
	SIZE_T	CPUAvailable;		/** The amount of free memory before the first malloc has been done. (PS3 only) */

	// Physical memory for Xbox and PC / Local memory for PS3 (tracked in the allocators)
	SIZE_T	GPUUsed;			/** The allocated in use by the application physical memory. */
	SIZE_T	GPUSlack;			/** The allocated from the OS, but not used by the application. */
	SIZE_T	GPUWaste;			/** Alignment waste from a pooled allocator plus book keeping overhead. */
	SIZE_T	GPUAvailable;		/** The total amount of memory available for the allocator. (PS3 only) */

	SIZE_T	OSReportedUsed;		/** Used memory as reported by the operating system. (Xbox360, PS3) */
	SIZE_T	OSReportedFree;		/** Free memory as reported by the operating system. (Xbox360, PS3) */
	SIZE_T	OSOverhead;			/** The overhead of the operating system. (Xbox360 only) */
	SIZE_T	ImageSize;			/** Size of loaded executable, stack, static, and global object size. (Xbox360, PS3) */

	SIZE_T	HostUsed;			/** Host memory in use by the application. (PS3 only) */
	SIZE_T	HostSlack;			/** Host memory allocated, but not used by the application. (PS3 only) */
	SIZE_T	HostWaste;			/** Host memory wasted due to allocations' alignment. Aproximation. (PS3 only) */
	SIZE_T	HostAvailable;		/** The total amount of host memory that has been allocated. (PS3 only) */

	SIZE_T	AllocatedTextureMemorySize; /** Size of allocated memory in the texture pool. */
	SIZE_T	AvailableTextureMemorySize; /** Size of available memory in the texture pool. */

	/** Returns statistics count. */
	static BYTE GetStatsNum()
	{
		const BYTE ItemsCount = (BYTE)(sizeof(FMemoryAllocationStats) / sizeof(SIZE_T));
		return ItemsCount;
	}

	/** 
	 * Constructor. 
	 */
	FMemoryAllocationStats()
	{
		TotalUsed = 0;
		TotalAllocated = 0;

		CPUUsed = 0;
		CPUSlack = 0;
		CPUWaste = 0;
		CPUAvailable = 0;

		GPUUsed = 0;
		GPUSlack = 0;
		GPUWaste = 0;
		GPUAvailable = 0;

		HostUsed = 0;
		HostSlack = 0;
		HostWaste = 0;
		HostAvailable = 0;

		OSReportedUsed = 0;	
		OSReportedFree = 0;	
		OSOverhead = 0;	
		ImageSize = 0;

		AllocatedTextureMemorySize = 0;
		AvailableTextureMemorySize = 0;
	}
};

//
// C style memory allocation stubs that fall back to C runtime
//
#ifndef appSystemMalloc
#define appSystemMalloc		malloc
#endif
#ifndef appSystemFree
#define appSystemFree		free
#endif

/**
 * Inherit from FUseSystemMallocForNew if you want your objects to be placed in memory
 * alloced by the system malloc routines, bypassing GMalloc. This is e.g. used by FMalloc
 * itself.
 */
class FUseSystemMallocForNew
{
public:
	/**
	 * Overloaded new operator using the system allocator.
	 *
	 * @param	Size	Amount of memory to allocate (in bytes)
	 * @return			A pointer to a block of memory with size Size or NULL
	 */
	void* operator new( size_t Size )
	{
		return appSystemMalloc( Size );
	}

	/**
	 * Overloaded delete operator using the system allocator
	 *
	 * @param	Ptr		Pointer to delete
	 */
	void operator delete( void* Ptr )
	{
		appSystemFree( Ptr );
	}

	/**
	 * Overloaded array new operator using the system allocator.
	 *
	 * @param	Size	Amount of memory to allocate (in bytes)
	 * @return			A pointer to a block of memory with size Size or NULL
	 */
	void* operator new[]( size_t Size )
	{
		return appSystemMalloc( Size );
	}

	/**
	 * Overloaded array delete operator using the system allocator
	 *
	 * @param	Ptr		Pointer to delete
	 */
	void operator delete[]( void* Ptr )
	{
		appSystemFree( Ptr );
	}
};

/** The global memory allocator's interface. */
class FMalloc  : 
	public FUseSystemMallocForNew,
	public FExec
{
public:
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
		return Size; // implementations where this is not possible need not implement it.
	}

	/** 
	 * Malloc
	 */
	virtual void* Malloc( DWORD Count, DWORD Alignment=DEFAULT_ALIGNMENT ) = 0;

	/** 
	 * Realloc
	 */
	virtual void* Realloc( void* Original, DWORD Count, DWORD Alignment=DEFAULT_ALIGNMENT ) = 0;

	/** 
	 * Free
	 */
	virtual void Free( void* Original ) = 0;

	/** 
	 * Physical alloc
	 */
	virtual void* PhysicalAlloc( DWORD Count, ECacheBehaviour CacheBehaviour = CACHE_WriteCombine ) 
	{ 
		return NULL;
	}

	/** 
	 * Physical free
	 */
	virtual void PhysicalFree( void* Original )
	{
	}
		
	/** 
	 * Handles any commands passed in on the command line
	 */
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar ) 
	{ 
		return FALSE; 
	}
	
	/** 
	 * Called every game thread tick 
	 */
	virtual void Tick( FLOAT DeltaTime ) 
	{ 
	}

	/**
	 * Returns if the allocator is guaranteed to be thread-safe and therefore
	 * doesn't need a unnecessary thread-safety wrapper around it.
	 */
	virtual UBOOL IsInternallyThreadSafe() const 
	{ 
		return FALSE; 
	}

	/**
	 * Gathers all current memory stats
	 *
	 * @param FMemoryAllocationStats	[out] structure containing information about the size of allocations
	 */
	virtual void GetAllocationInfo( FMemoryAllocationStats& MemStats ) 
	{
	}

	/**
	*Dumps details about allocation deltas to an output device
	*@param OutputDevice - Output Device
	*@param startingMemStats - The previously recorded allocation stats for which to calculate deltas against
	*/
#if USE_SCOPED_MEM_STATS
	virtual void DumpAllocationsDeltas (class FOutputDevice& OutputDevice , FMemoryAllocationStats& StartingMemStats )
	{
		OutputDevice.Logf( TEXT( "DumpAllocationsDeltas not implemented" ) );
	}
#endif

	/**
	 * Dumps details about all allocations to an output device
	 *
	 * @param Ar	[in] Output device
	 */
	virtual void DumpAllocations( class FOutputDevice& Ar ) 
	{
		Ar.Logf( TEXT( "DumpAllocations not implemented" ) );
	}

	/**
	 * Validates the allocator's heap
	 */
	virtual UBOOL ValidateHeap()
	{
		return( TRUE );
	}

	/**
	 * Keeps trying to allocate memory until we fail
	 *
	 * @param Ar Device to send output to
	 */
	virtual void CheckMemoryFragmentationLevel( class FOutputDevice& Ar ) 
	{ 
		Ar.Log( TEXT("CheckMemoryFragmentationLevel not implemented") ); 
	}

	/**
	 * If possible give memory back to the os from unused segments
	 *
	 * @param ReservePad - amount of space to reserve when trimming
	 * @param bShowStats - log stats about how much memory was actually trimmed. Disable this for perf
	 * @return TRUE if succeeded
	 */
	virtual UBOOL TrimMemory(SIZE_T /*ReservePad*/,UBOOL bShowStats=FALSE) 
	{ 
		return FALSE; 
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
		return FALSE; // Default implementation has no way of determining this
	}

#if USE_DETAILED_IPHONE_MEM_TRACKING

	enum OpenGLBufferTrackingType
	{
		GLBUFFER = 0,
		GLTEXTURE,
		GLSURFACE,
		GLBACKBUFFER,
		GLTRACKINGTYPESCOUNT 
	};

	/**
	*Increments the tracked OpenGL buffer memory that is externally 
	*allocated.  
	*@Param Size - The size in bytes of the allocation to track
	*/
	virtual void IncTrackedOpenGLMemory(UINT Size, OpenGLBufferTrackingType trackingType) 
	{ 
		
	}

	/**
	*Decrements the tracked OpenGL buffer memory that is externally
	*allocated
	*@param Size - The size in bytes of the deallocation to track
	*/
	virtual void DecTrackedOpenGLMemory(UINT Size, OpenGLBufferTrackingType trackingType) 
	{ 
		
	}
#endif


	/** Total number of calls Malloc, if implemented by derived class. */
	static QWORD TotalMallocCalls;
	/** Total number of calls Malloc, if implemented by derived class. */
	static QWORD TotalFreeCalls;
	/** Total number of calls Malloc, if implemented by derived class. */
	static QWORD TotalReallocCalls;
	/** Total number of calls to PhysicalAlloc, if implemented by derived class. */
	static QWORD TotalPhysicalAllocCalls;
	/** Total number of calls to PhysicalFree, if implemented by derived class. */
	static QWORD TotalPhysicalFreeCalls;
};

/**
*defines to easily utilize tracking OpenGL buffer data without the need to wrap all calls
*/
#if USE_DETAILED_IPHONE_MEM_TRACKING
#define INC_TRACKED_OPEN_GL_BUFFER_MEM(Size) GMalloc->IncTrackedOpenGLMemory(Size, FMalloc::GLBUFFER);
#define DEC_TRACKED_OPEN_GL_BUFFER_MEM(Size) GMalloc->DecTrackedOpenGLMemory(Size, FMalloc::GLBUFFER);

#define INC_TRACKED_OPEN_GL_TEXTURE_MEM(Size) GMalloc->IncTrackedOpenGLMemory(Size, FMalloc::GLTEXTURE);
#define DEC_TRACKED_OPEN_GL_TEXTURE_MEM(Size) GMalloc->DecTrackedOpenGLMemory(Size, FMalloc::GLTEXTURE);

#define INC_TRACKED_OPEN_GL_SURFACE_MEM(Size) GMalloc->IncTrackedOpenGLMemory(Size, FMalloc::GLSURFACE);
#define DEC_TRACKED_OPEN_GL_SURFACE_MEM(Size) GMalloc->DecTrackedOpenGLMemory(Size, FMalloc::GLSURFACE);

#else
#define INC_TRACKED_OPEN_GL_BUFFER_MEM(Size)
#define DEC_TRACKED_OPEN_GL_BUFFER_MEM(Size)
#define INC_TRACKED_OPEN_GL_TEXTURE_MEM(Size)
#define INC_TRACKED_OPEN_GL_TEXTURE_MEM(Size) 
#define INC_TRACKED_OPEN_GL_SURFACE_MEM(Size) 
#define DEC_TRACKED_OPEN_GL_SURFACE_MEM(Size) 
#endif

/*-----------------------------------------------------------------------------
	Memory functions.
-----------------------------------------------------------------------------*/

/** @name Memory functions */
//@{
/** Copies count bytes of characters from Src to Dest. If some regions of the source
 * area and the destination overlap, memmove ensures that the original source bytes
 * in the overlapping region are copied before being overwritten.  NOTE: make sure
 * that the destination buffer is the same size or larger than the source buffer!
 */
#ifndef DEFINED_appMemmove
	#define appMemmove( Dest, Src, Count )	memmove( Dest, Src, Count )
#endif

inline INT appMemcmp( const void* Buf1, const void* Buf2, INT Count )
{
	return memcmp( Buf1, Buf2, Count );
}

UBOOL appMemIsZero( const void* V, int Count );
DWORD appMemCrc( const void* Data, INT Length, DWORD CRC=0 );

/**
 * Sets the first Count chars of Dest to the character C.
 */
#define appMemset( Dest, C, Count )			memset( Dest, C, Count )

#ifndef DEFINED_appMemcpy
	#define appMemcpy( Dest, Src, Count )	memcpy( Dest, Src, Count )
	/** On some platforms memcpy optimized for big blocks is available */
	#define appBigBlockMemcpy( Dest, Src, Count )	memcpy( Dest, Src, Count )
	/** On some platforms memcpy optimized for big blocks that avoid L2 cache pollution are available */
	#define appStreamingMemcpy( Dest, Src, Count )	memcpy( Dest, Src, Count )
#endif

#ifndef DEFINED_appMemzero
	#define appMemzero( Dest, Count )		memset( Dest, 0, Count )
#endif

inline void appMemswap( void* Ptr1, void* Ptr2, DWORD Size )
{
	void* Temp = appAlloca(Size);
	appMemcpy( Temp, Ptr1, Size );
	appMemcpy( Ptr1, Ptr2, Size );
	appMemcpy( Ptr2, Temp, Size );
}

/** Templated version of appMemset. */
template< class T > inline void appMemSet( T& Src, INT I )
{
	appMemset( &Src, I, sizeof( T ) );
}

/** Templated version of appMemzero. */
template< class T > inline void appMemZero( T& Src )
{
	appMemset( &Src, 0, sizeof( T ) );
}

/** Templated version of appMemcopy. */
template< class T > inline void appMemCopy( T& Dest, const T& Src )
{
	appMemcpy( &Dest, &Src, sizeof( T ) );
}

//
// C style memory allocation stubs.
//
/** 
 * appMallocQuantizeSize returns the actual size of allocation request likely to be returned
 * so for the template containers that use slack, they can more wisely pick
 * appropriate sizes to grow and shrink to.
 *
 * @param Size			The size of a hypothetical allocation request
 * @param Alignment		The alignment of a hypothetical allocation request
 * @return				Returns the usable size that the allocation request would return. In other words you can ask for this greater amount without using any more actual memory.
 */
extern DWORD appMallocQuantizeSize( DWORD Size, DWORD Alignment=DEFAULT_ALIGNMENT );
extern void* appMalloc( DWORD Count, DWORD Alignment=DEFAULT_ALIGNMENT );
extern void* appRealloc( void* Original, DWORD Count, DWORD Alignment=DEFAULT_ALIGNMENT );
extern void appFree( void* Original );
extern void* appPhysicalAlloc( DWORD Count, ECacheBehaviour CacheBehaviour = CACHE_WriteCombine );
extern void appPhysicalFree( void* Original );

#endif
