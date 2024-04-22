/*=============================================================================
	FMallocProfiler.h: Memory profiling support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef FMALLOC_PROFILER_H
#define FMALLOC_PROFILER_H

#include "UMemoryDefines.h"

#if USE_MALLOC_PROFILER

/*=============================================================================
	Malloc profiler enumerations
=============================================================================*/

/**
 * The lower 2 bits of a pointer are piggy-bagged to store what kind of data follows it. This enum lists
 * the possible types.
 */
enum EProfilingPayloadType
{
	TYPE_Malloc  = 0,
	TYPE_Free	 = 1,
	TYPE_Realloc = 2,
	TYPE_Other   = 3,
	// Don't add more than 4 values - we only have 2 bits to store this.
};

/**
 *  The the case of TYPE_Other, this enum determines the subtype of the token.
 */
enum EProfilingPayloadSubType
{
	// Core marker types

	/** Marker used to determine when malloc profiler stream has ended. */
	SUBTYPE_EndOfStreamMarker					= 0,

	/** Marker used to determine when we need to read data from the next file. */
	SUBTYPE_EndOfFileMarker						= 1,

	/** Marker used to determine when a snapshot has been added. */
	SUBTYPE_SnapshotMarker						= 2,

	/** Marker used to determine when a new frame has started. */
	SUBTYPE_FrameTimeMarker						= 3,

	/** Not used. Only for backward compatibility. Use a new snapshot marker instead. */
	SUBTYPE_TextMarker							= 4,

	// Marker types for periodic non-GMalloc memory status updates. Only for backward compatibility, replaced with SUBTYPE_MemoryAllocationStats

	/** Marker used to store the total amount of memory used by the game. */
	SUBTYPE_TotalUsed							= 5,

	/** Marker used to store the total amount of memory allocated from the OS. */
	SUBTYPE_TotalAllocated						= 6,

	/** Marker used to store the allocated in use by the application virtual memory. */
	SUBTYPE_CPUUsed								= 7,

	/** Marker used to store the allocated from the OS/allocator, but not used by the application. */
	SUBTYPE_CPUSlack							= 8,

	/** Marker used to store the alignment waste from a pooled allocator plus book keeping overhead. */
	SUBTYPE_CPUWaste							= 9,

	/** Marker used to store the allocated in use by the application physical memory. */
	SUBTYPE_GPUUsed								= 10,

	/** Marker used to store the allocated from the OS, but not used by the application. */
	SUBTYPE_GPUSlack							= 11,

	/** Marker used to store the alignment waste from a pooled allocator plus book keeping overhead. */
	SUBTYPE_GPUWaste							= 12,

	/** Marker used to store the overhead of the operating system. */
	SUBTYPE_OSOverhead							= 13,

	/** Marker used to store the size of loaded executable, stack, static, and global object size. */
	SUBTYPE_ImageSize							= 14,

	/// Version 3
	// Marker types for automatic snapshots.

	/** Marker used to determine when engine has started the cleaning process before loading a new level. */
	SUBTYPE_SnapshotMarker_LoadMap_Start		= 21,

	/** Marker used to determine when a new level has started loading. */
	SUBTYPE_SnapshotMarker_LoadMap_Mid			= 22,

	/** Marker used to determine when a new level has been loaded. */
	SUBTYPE_SnapshotMarker_LoadMap_End			= 23,

	/** Marker used to determine when garbage collection has started. */
    SUBTYPE_SnapshotMarker_GC_Start				= 24,

	/** Marker used to determine when garbage collection has finished. */
    SUBTYPE_SnapshotMarker_GC_End		        = 25,

	/** Marker used to determine when a new streaming level has been requested to load. */
	SUBTYPE_SnapshotMarker_LevelStream_Start	= 26,

	/** Marker used to determine when a previously streamed level has been made visible. */
	SUBTYPE_SnapshotMarker_LevelStream_End		= 27,

	/** Marker used to store a generic malloc statistics. @see FMemoryAllocationStats. */
	SUBTYPE_MemoryAllocationStats				= 31,

	/** Start licensee-specific subtypes from here. */
	SUBTYPE_LicenseeBase						= 50,

	/** Unknown the subtype of the token. */
	SUBTYPE_Unknown,
};

/** Whether we are performing symbol lookup at runtime or not.					*/
#if !CONSOLE
#define SERIALIZE_SYMBOL_INFO 1
#else
#define SERIALIZE_SYMBOL_INFO 0
#endif

/*=============================================================================
	CallStack address information.
=============================================================================*/

/**
 * Helper structure encapsulating a single address/ point in a callstack
 */
struct FCallStackAddressInfo
{
	/** Program counter address of entry.			*/
	QWORD	ProgramCounter;
#if SERIALIZE_SYMBOL_INFO
	/** Index into name table for filename.			*/
	INT		FilenameNameTableIndex;
	/** Index into name table for function name.	*/
	INT		FunctionNameTableIndex;
	/** Line number in file.						*/
	INT		LineNumber;
#endif

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	AddressInfo	AddressInfo to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FCallStackAddressInfo AddressInfo )
	{
		Ar	<< AddressInfo.ProgramCounter
#if SERIALIZE_SYMBOL_INFO
			<< AddressInfo.FilenameNameTableIndex
			<< AddressInfo.FunctionNameTableIndex
			<< AddressInfo.LineNumber
#endif
			;
		return Ar;
	}
};

/*=============================================================================
	FMallocProfilerBufferedFileWriter
=============================================================================*/

/**
 * Special buffered file writer, used to serialize data before GFileManager is initialized.
 */
class FMallocProfilerBufferedFileWriter : public FArchive
{
public:
	/** Internal file writer used to serialize to HDD. */
	FArchive*		FileWriter;
	/** Buffered data being serialized before GFileManager has been set up.	*/
	TArray<BYTE>	BufferedData;
	/** Timestamped filename with path.	*/
	FFilename		FullFilepath;
	/** Timestamped file path for the memory captures, just add extension. */
	FString			BaseFilePath;
	/** File number. Index 0 is the base .mprof file. */
	INT				FileNumber;

	/**
	 * Constructor. Called before GMalloc is initialized!!!
	 */
	FMallocProfilerBufferedFileWriter();

	/**
	 * Destructor, cleaning up FileWriter.
	 */
	virtual ~FMallocProfilerBufferedFileWriter();

	/**
	 * Whether we are currently buffering to memory or directly writing to file.
	 *
	 * @return	TRUE if buffering to memory, FALSE otherwise
	 */
	UBOOL IsBufferingToMemory();

	/**
	 * Splits the file and creates a new one with different extension.
	 */
	void Split();

	// FArchive interface.
	virtual void Serialize( void* V, INT Length );
	virtual void Seek( INT InPos );
	virtual UBOOL Close();
	virtual INT Tell();

	/** Returns the allocated size for use in untracked memory calculations. */
	DWORD GetAllocatedSize();
};

/*=============================================================================
	FMallocProfiler
=============================================================================*/

/** This is an utility class that handles specific malloc profiler mutex locking. */
class FScopedMallocProfilerLock
{	
	/** Copy constructor hidden on purpose. */
	FScopedMallocProfilerLock(FScopedMallocProfilerLock* InScopeLock);

	/** Assignment operator hidden on purpose. */
	FScopedMallocProfilerLock& operator=(FScopedMallocProfilerLock& InScopedMutexLock) { return *this; }

public:
	/** Constructor that performs a lock on the malloc profiler tracking methods. */
	FScopedMallocProfilerLock();

	/** Destructor that performs a release on the malloc profiler tracking methods. */
	~FScopedMallocProfilerLock();
};

/**
 * Memory profiling malloc, routing allocations to real malloc and writing information on all 
 * operations to a file for analysis by a standalone tool.
 */
class FMallocProfiler : public FMalloc
{
	friend struct	FScriptCallstackFrame;
	friend class	FMallocGcmProfiler;
	friend class	FScopedMallocProfilerLock;
	friend class	FMallocProfilerBufferedFileWriter;

protected:
	/** Malloc we're based on, aka using under the hood												*/
	FMalloc*								UsedMalloc;
	/** Whether or not EndProfiling()  has been Ended.  Once it has been ended it has ended most operations are no longer valid **/
	UBOOL									bEndProfilingHasBeenCalled;
	/** Time malloc profiler was created. Used to convert arbitrary DOUBLE time to relative FLOAT.	*/
	DOUBLE									StartTime;
	/** File writer used to serialize data. Safe to call before GFileManager has been initialized.	*/
	FMallocProfilerBufferedFileWriter		BufferedFileWriter;

	/** Critical section to sequence tracking.														*/
	FCriticalSection						CriticalSection;

	/** Mapping from program counter to index in program counter array.								*/
	TMap<QWORD,INT>							ProgramCounterToIndexMap;
	/** Array of unique call stack address infos.													*/
	TArray<struct FCallStackAddressInfo>	CallStackAddressInfoArray;

	/** Mapping from callstack CRC to offset in call stack info buffer.								*/
	TMap<DWORD,INT>							CRCToCallStackIndexMap;
	/** Buffer of unique call stack infos.															*/
	FCompressedGrowableBuffer				CallStackInfoBuffer;

	/** Mapping from name to index in name array.													*/
	TMap<FString,INT>						NameToNameTableIndexMap;
	/** Array of unique names.																		*/
	TArray<FString>							NameArray;

	/** Whether the output file has been closed. */
	UBOOL									bOutputFileClosed;

	/** Critical section used to detect when malloc profiler is inside one of the tracking functions. */
	FCriticalSection						SyncObject;

	/** Whether operations should be tracked. FALSE e.g. in tracking internal functions.			*/
	INT										SyncObjectLockCount;

	/** The currently executing thread's id. */
	DWORD									ThreadId;

	/** Simple count of memory operations															*/
	QWORD									MemoryOperationCount;

#if USE_MALLOC_PROFILER_DECODE_SCRIPT
	/** An instance of script callstack decoder.													*/
	class FScriptCallStackDecoder*			CallStackDecoder;

public:
	/** Returns an instance of script callstack decoder.											*/
	class FScriptCallStackDecoder* GetScriptCallStackDecoder() 
	{ 
		return CallStackDecoder; 
	} 
protected:
#endif // USE_MALLOC_PROFILER_DECODE_SCRIPT

	/** Returns TRUE if malloc profiler is outside one of the tracking methods, returns FALSE otherwise. */
	UBOOL IsOutsideTrackingFunction() const
	{
		const DWORD CurrentThreadId = appGetCurrentThreadId();
		return (SyncObjectLockCount == 0) || (ThreadId != CurrentThreadId);
	}

	/** 
	 * Returns index of callstack, captured by this function into array of callstack infos. If not found, adds it.
	 *
	 * @return index into callstack array
	 */
	INT GetCallStackIndex();

	/**
	 * Returns index of passed in program counter into program counter array. If not found, adds it.
	 *
	 * @param	ProgramCounter	Program counter to find index for
	 * @return	Index of passed in program counter if it's not NULL, INDEX_NONE otherwise
	 */
	INT GetProgramCounterIndex( QWORD ProgramCounter );

	/**
	 * Returns index of passed in name into name array. If not found, adds it.
	 *
	 * @param	Name	Name to find index for
	 * @return	Index of passed in name
	 */
	INT GetNameTableIndex( const FString& Name );

	/**
	 * Returns index of passed in name into name array. If not found, adds it.
	 *
	 * @param	Name	Name to find index for
	 * @return	Index of passed in name
	 */
	INT GetNameTableIndex( const FName& Name )
	{
		return GetNameTableIndex(Name.ToString());
	}

	/**
	 * Tracks malloc operation.
	 *
	 * @param	Ptr		Allocated pointer 
	 * @param	Size	Size of allocated pointer
	 */
	void TrackMalloc( void* Ptr, DWORD Size );
	
	/**
	 * Tracks free operation
	 *
	 * @param	Ptr		Freed pointer
	 */
	void TrackFree( void* Ptr );
	
	/**
	 * Tracks realloc operation
	 *
	 * @param	OldPtr	Previous pointer
	 * @param	NewPtr	New pointer
	 * @param	NewSize	New size of allocation
	 */
	void TrackRealloc( void* OldPtr, void* NewPtr, DWORD NewSize );

	/**
	 * Tracks memory allocations stats every 1024 memory operations. Used for time line view in memory profiler app.
	 * Expects to be inside of the critical section
	 */
	void TrackSpecialMemory();

	/**
	 * Ends profiling operation and closes file.
	 */
	void EndProfiling();

	/**
	 * Embeds token into stream to snapshot memory at this point.
	 */
	void SnapshotMemory(EProfilingPayloadSubType SubType, const FString& MarkerName);

	/**
	 * Embeds float token into stream (e.g. delta time).
	 */
	void EmbedFloatMarker(EProfilingPayloadSubType SubType, FLOAT DeltaTime);

	/**
	 * Embeds token into stream to snapshot memory at this point.
	 */
	void EmbedDwordMarker(EProfilingPayloadSubType SubType, DWORD Info);

	/** 
	 * Checks whether file is too big and will create new files with different extension but same base name.
	 */
	void PossiblySplitMprof();

	/** 
	 * Writes additional memory stats for a snapshot like memory allocations stats, list of all loaded levels and platform dependent memory metrics.
	 */
	void WriteAdditionalSnapshotMemoryStats();

	/** 
	 * Writes memory allocations stats. 
	 */
	void WriteMemoryAllocationStats();

	/** 
	 * Writes names of currently loaded levels. 
	 * Only to be called from within the mutex / scope lock of the FMallocProifler.
	 */
	virtual void WriteLoadedLevels();

	/** 
	 * Gather texture memory stats. 
	 */
	virtual void GetTexturePoolSize( FMemoryAllocationStats& MemoryStats );

	/** 
		Added for untracked memory calculation
		Note that this includes all the memory used by dependent malloc profilers, such as FMallocGcmProfiler,
		so they don't need their own version of this function. 
	*/
	INT CalculateMemoryProfilingOverhead();

public:
	/** Snapshot taken when engine has started the cleaning process before loading a new level. */
	static void SnapshotMemoryLoadMapStart(const FString& MapName);

	/** Snapshot taken when a new level has started loading. */
	static void SnapshotMemoryLoadMapMid(const FString& MapName);

	/** Snapshot taken when a new level has been loaded. */
	static void SnapshotMemoryLoadMapEnd(const FString& MapName);

	/** Snapshot taken when garbage collection has started. */
	static void SnapshotMemoryGCStart();

	/** Snapshot taken when garbage collection has finished. */
	static void SnapshotMemoryGCEnd();

	/** Snapshot taken when a new streaming level has been requested to load. */
	static void SnapshotMemoryLevelStreamStart(const FString& LevelName);

	/** Snapshot taken when a previously  streamed level has been made visible. */
	static void SnapshotMemoryLevelStreamEnd(const FString& LevelName);

	/** Returns malloc we're based on. */
	virtual FMalloc* GetInnermostMalloc()
	{ 
		return UsedMalloc;
	}

	/** 
	 * This function is intended to be called when a fatal error has occurred inside the allocator and
	 * you want to dump the current mprof before crashing, so that it can be used to help debug the error.
	 * IMPORTANT: This function assumes that this thread already has the allocator mutex.. 
	 */
	void PanicDump(EProfilingPayloadType FailedOperation, void* Ptr1, void* Ptr2);

	/**
	 * Constructor, initializing all member variables and potentially loading symbols.
	 *
	 * @param	InMalloc	The allocator wrapped by FMallocProfiler that will actually do the allocs/deallocs.
	 */
	FMallocProfiler(FMalloc* InMalloc);

	/**
	 * Begins profiling operation and opens file.
	 */
	void BeginProfiling();

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
	virtual void* Malloc( DWORD Size, DWORD Alignment )
	{
		FScopeLock Lock( &CriticalSection );
		void* Ptr = UsedMalloc->Malloc( Size, Alignment );
		TrackMalloc( Ptr, Size );
		TrackSpecialMemory();
		return Ptr;
	}

	/** 
	 * Realloc
	 */
	virtual void* Realloc( void* OldPtr, DWORD NewSize, DWORD Alignment )
	{
		FScopeLock Lock( &CriticalSection );
		void* NewPtr = UsedMalloc->Realloc( OldPtr, NewSize, Alignment );
		TrackRealloc( OldPtr, NewPtr, NewSize );
		TrackSpecialMemory();
		return NewPtr;
	}

	/** 
	 * Free
	 */
	virtual void Free( void* Ptr )
	{
		FScopeLock Lock( &CriticalSection );
		UsedMalloc->Free( Ptr );
		TrackFree( Ptr );
		TrackSpecialMemory();
	}

	/** 
	 * Physical alloc
	 */
	virtual void* PhysicalAlloc( DWORD Size, ECacheBehaviour InCacheBehaviour )
	{
		FScopeLock Lock( &CriticalSection );
		void* Ptr = UsedMalloc->PhysicalAlloc( Size, InCacheBehaviour );
		TrackMalloc( Ptr, Size );
		TrackSpecialMemory();
		return Ptr;
	}

	/** 
	 * Physical free
	 */
	virtual void PhysicalFree( void* Ptr )
	{
		FScopeLock Lock( &CriticalSection );
		UsedMalloc->PhysicalFree( Ptr );
		TrackFree( Ptr );
		TrackSpecialMemory();
	}

	/**
	 * Returns if the allocator is guaranteed to be thread-safe and therefore
	 * doesn't need a unnecessary thread-safety wrapper around it.
	 */
	virtual UBOOL IsInternallyThreadSafe() const 
	{ 
		return TRUE; 
	}

	/**
	 * Passes request for gathering memory allocations for both virtual and physical allocations
	 * on to used memory manager.
	 *
	 * @param FMemoryAllocationStats	[out] structure containing information about the size of allocations
	 */
	virtual void GetAllocationInfo( FMemoryAllocationStats& MemStats )
	{
		FScopeLock Lock( &CriticalSection );
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
		FScopeLock Lock( &CriticalSection );
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
		FScopeLock Lock( &CriticalSection );
		UsedMalloc->DumpAllocations( Ar );
	}

	/**
	 * Validates the allocator's heap
	 */
	virtual UBOOL ValidateHeap()
	{
		FScopeLock Lock( &CriticalSection );
		return( UsedMalloc->ValidateHeap() );
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
		FScopeLock Lock( &CriticalSection );
		return UsedMalloc->GetAllocationSize(Original,SizeOut);
	}

	/**
	 * Exec handler. Parses command and returns TRUE if handled.
	 *
	 * @param	Cmd		Command to parse
	 * @param	Ar		Output device to use for logging
	 * @return	TRUE if handled, FALSE otherwise
	 */
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );

	/** Called every game thread tick */
	virtual void Tick( FLOAT DeltaTime ) 
	{ 
		FScopeLock Lock( &CriticalSection );
		UsedMalloc->Tick(DeltaTime);
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

#endif //USE_MALLOC_PROFILER

#endif	//#ifndef FMALLOC_PROFILER_H
