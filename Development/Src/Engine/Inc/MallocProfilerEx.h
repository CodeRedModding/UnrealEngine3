/*=============================================================================
	MallocProfilerEx.h: Extended memory profiling support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef INC_MALLOCPROFILEREX_H
#define INC_MALLOCPROFILEREX_H

#if USE_MALLOC_PROFILER

/** 
 * Extended version of malloc profiler, implements engine side functions that are not available in the core
 */
class FMallocProfilerEx : public FMallocProfiler
{
public:
	/**
	 * Constructor, initializing all member variables and potentially loading symbols.
	 *
	 * @param	InMalloc	The allocator wrapped by FMallocProfiler that will actually do the allocs/deallocs.
	 */
	FMallocProfilerEx( FMalloc* InMalloc );

	/** 
	 * Writes names of currently loaded levels. 
	 * Only to be called from within the mutex / scope lock of the FMallocProfiler.
	 */
	virtual void WriteLoadedLevels();

	/** 
	 * Gather texture memory stats. 
	 */
	virtual void GetTexturePoolSize( FMemoryAllocationStats& MemoryStats );
};

#endif // USE_MALLOC_PROFILER

#endif // INC_MALLOCPROFILEREX_H
