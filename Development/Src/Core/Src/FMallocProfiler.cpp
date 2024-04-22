/*=============================================================================
	FMallocProfiler.cpp: Memory profiling support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#include "FMallocProfiler.h"
#include "ScriptCallstackDecoder.h"
#include "ProfilingHelpers.h"

#if USE_MALLOC_PROFILER

FMallocProfiler* GMallocProfiler;

#if PS3
#include "FMallocPS3.h"
#endif

/**
 * Maximum depth of stack backtrace.
 * Reducing this will sometimes truncate the amount of callstack info you get but will also reduce
 * the number of over all unique call stacks as some of the script call stacks are REALLY REALLY
 * deep and end up eating a lot of memory which will OOM you on consoles. A good value for consoles 
 * when doing long runs is 50.
 */
#define	MEMORY_PROFILER_MAX_BACKTRACE_DEPTH			75
/** Number of backtrace entries to skip											*/
#define MEMORY_PROFILER_SKIP_NUM_BACKTRACE_ENTRIES	1

/** Magic value, determining that file is a memory profiler file.				*/
#define MEMORY_PROFILER_MAGIC						0xDA15F7D8
/** Version of memory profiler.													*/
#define MEMORY_PROFILER_VERSION						3

/** Whether we are performing symbol lookup at runtime or not.					*/
#if !CONSOLE
#define SERIALIZE_SYMBOL_INFO 1
#else
#define SERIALIZE_SYMBOL_INFO 0
#endif

// Register for GC callbacks to log when GC occurs.
IMPLEMENT_PRE_GARBAGE_COLLECTION_CALLBACK(AutoMemorySnapshotGCStart, FMallocProfiler::SnapshotMemoryGCStart, GCCB_PRE_AutoMemorySnapshot);
IMPLEMENT_POST_GARBAGE_COLLECTION_CALLBACK(AutoMemorySnapshotGCEnd, FMallocProfiler::SnapshotMemoryGCEnd, GCCB_POST_AutoMemorySnapshot);

/*=============================================================================
	Profiler header.
=============================================================================*/

struct FProfilerHeader
{
	/** Magic to ensure we're opening the right file.	*/
	DWORD	Magic;
	/** Version number to detect version mismatches.	*/
	DWORD	Version;
	/** Platform that was captured.						*/
	DWORD	Platform;
	/** Whether symbol information is being serialized. */
	DWORD	bShouldSerializeSymbolInfo;
	/** Name of executable, used for finding symbols.	*/
	FString ExecutableName;

	/** Offset in file for name table.					*/
	DWORD	NameTableOffset;
	/** Number of name table entries.					*/
	DWORD	NameTableEntries;

	/** Offset in file for callstack address table.		*/
	DWORD	CallStackAddressTableOffset;
	/** Number of callstack address entries.			*/
	DWORD	CallStackAddressTableEntries;

	/** Offset in file for callstack table.				*/
	DWORD	CallStackTableOffset;
	/** Number of callstack entries.					*/
	DWORD	CallStackTableEntries;
	/** The file offset for module information.			*/
	DWORD	ModulesOffset;
	/** The number of module entries.					*/
	DWORD	ModuleEntries;
	/** Number of data files total.						*/
	DWORD	NumDataFiles;

	/*-----------------------------------------------------------------------------
		Version 3
	-----------------------------------------------------------------------------*/

	/** Offset in file for stript callstack table.		*/
	DWORD	ScriptCallstackTableOffset;

	/** Offset in file for script name table.			*/
	DWORD	ScriptNameTableOffset;

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Header		Header to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FProfilerHeader Header )
	{
		Ar	<< Header.Magic
			<< Header.Version
			<< Header.Platform
			<< Header.bShouldSerializeSymbolInfo
			<< Header.NameTableOffset
			<< Header.NameTableEntries
			<< Header.CallStackAddressTableOffset
			<< Header.CallStackAddressTableEntries
			<< Header.CallStackTableOffset
			<< Header.CallStackTableEntries
			<< Header.ModulesOffset
			<< Header.ModuleEntries
			<< Header.NumDataFiles
			<< Header.ScriptCallstackTableOffset
			<< Header.ScriptNameTableOffset;

		check( Ar.IsSaving() );
		SerializeStringAsANSICharArray( Header.ExecutableName, Ar, 255 );
		return Ar;
	}
};

/*=============================================================================
	CallStack address information.
=============================================================================*/

/**
 * Helper structure encapsulating a callstack.
 */
struct FCallStackInfo
{
	/** CRC of program counters for this callstack.				*/
	DWORD	CRC;
	/** Array of indices into callstack address info array.		*/
	INT		AddressIndices[MEMORY_PROFILER_MAX_BACKTRACE_DEPTH];

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	AllocInfo	Callstack info to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FCallStackInfo CallStackInfo )
	{
		Ar << CallStackInfo.CRC;
		// Serialize valid callstack indices.
		INT i=0;
		for( ; i<ARRAY_COUNT(CallStackInfo.AddressIndices) && CallStackInfo.AddressIndices[i]!=-1; i++ )
		{
			Ar << CallStackInfo.AddressIndices[i];
		}
		// Terminate list of address indices with -1 if we have a normal callstack.
		INT Stopper = -1;
		// And terminate with -2 if the callstack was truncated.
		if( i== ARRAY_COUNT(CallStackInfo.AddressIndices) )
		{
			Stopper = -2;
		}

		Ar << Stopper;
		return Ar;
	}
};

/*=============================================================================
	Allocation infos.
=============================================================================*/

/**
 * Relevant information for a single malloc operation.
 *
 * 16 bytes
 */
struct FProfilerAllocInfo
{
	/** Pointer of allocation, lower two bits are used for payload type.	*/
	QWORD Pointer;
	/** Index of callstack.													*/
	INT CallStackIndex;
	/** Size of allocation.													*/
	DWORD Size;

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	AllocInfo	Allocation info to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FProfilerAllocInfo AllocInfo )
	{
		Ar	<< AllocInfo.Pointer
			<< AllocInfo.CallStackIndex
			<< AllocInfo.Size;
		return Ar;
	}
};

/**
 * Relevant information for a single free operation.
 *
 * 8 bytes
 */
struct FProfilerFreeInfo
{
	/** Free'd pointer, lower two bits are used for payload type.			*/
	QWORD Pointer;
	
	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	FreeInfo	Free info to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FProfilerFreeInfo FreeInfo )
	{
		Ar	<< FreeInfo.Pointer;
		return Ar;
	}
};

/**
 * Relevant information for a single realloc operation.
 *
 * 24 bytes
 */
struct FProfilerReallocInfo
{
	/** Old pointer, lower two bits are used for payload type.				*/
	QWORD OldPointer;
	/** New pointer, might be identical to old.								*/
	QWORD NewPointer;
	/** Index of callstack.													*/
	INT CallStackIndex;
	/** Size of allocation.													*/
	DWORD Size;

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	ReallocInfo	Realloc info to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FProfilerReallocInfo ReallocInfo )
	{
		Ar	<< ReallocInfo.OldPointer
			<< ReallocInfo.NewPointer
			<< ReallocInfo.CallStackIndex
			<< ReallocInfo.Size;
		return Ar;
	}
};

/**
 * Helper structure for misc data like e.g. end of stream marker.
 *
 * 12 bytes (assuming 32 bit pointers)
 */
struct FProfilerOtherInfo
{
	/** Dummy pointer as all tokens start with a pointer (TYPE_Other)		*/
	QWORD	DummyPointer;
	/** Subtype.															*/
	INT		SubType;
	/** Subtype specific payload.											*/
	DWORD	Payload;

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	OtherInfo	Info to serialize
 	 * @return	Passed in archive
 	 */
	friend FArchive& operator << ( FArchive& Ar, FProfilerOtherInfo OtherInfo )
	{
		Ar	<< OtherInfo.DummyPointer
			<< OtherInfo.SubType
			<< OtherInfo.Payload;
		return Ar;
	}
};

/*=============================================================================
	FMallocProfiler implementation.
=============================================================================*/


/**
 * Constructor, initializing all member variables and potentially loading symbols.
 *
 * @param	InMalloc	The allocator wrapped by FMallocProfiler that will actually do the allocs/deallocs.
 */
FMallocProfiler::FMallocProfiler(FMalloc* InMalloc)
:	UsedMalloc( InMalloc )
,   bEndProfilingHasBeenCalled( FALSE )
,	CallStackInfoBuffer( 512 * 1024, COMPRESS_ZLIB )
,	bOutputFileClosed(FALSE)
,	SyncObjectLockCount(0)
,	MemoryOperationCount( 0 )
{
#if LOAD_SYMBOLS_FOR_STACK_WALKING
	// Initialize symbols.
	appInitStackWalking();
#endif
	StartTime = appSeconds();
}

/**
 * Tracks malloc operation.
 *
 * @param	Ptr		Allocated pointer 
 * @param	Size	Size of allocated pointer
 */
void FMallocProfiler::TrackMalloc( void* Ptr, DWORD Size )
{	
	// Avoid tracking operations caused by tracking!
	if( !bEndProfilingHasBeenCalled && IsOutsideTrackingFunction() )
	{
		FScopedMallocProfilerLock MallocProfilerLock;

		// Gather information about operation.
		FProfilerAllocInfo AllocInfo;
		AllocInfo.Pointer			= (QWORD)(UPTRINT) Ptr | TYPE_Malloc;
		AllocInfo.CallStackIndex	= GetCallStackIndex();
		AllocInfo.Size				= Size;

		// Serialize to HDD.
		BufferedFileWriter << AllocInfo;
		MALLOC_PROFILER_DECODE_SCRIPT( CallStackDecoder->SerializeScriptCallstack(BufferedFileWriter); )

		// Re-enable allocation tracking again.
		PossiblySplitMprof();
	}
}

/**
 * Tracks free operation
 *
 * @param	Ptr		Freed pointer
 */
void FMallocProfiler::TrackFree( void* Ptr )
{
	// Avoid tracking operations caused by tracking!
	if( !bEndProfilingHasBeenCalled && Ptr && IsOutsideTrackingFunction() )
	{
		FScopedMallocProfilerLock MallocProfilerLock;

		// Gather information about operation.
		FProfilerFreeInfo FreeInfo;
		FreeInfo.Pointer = (QWORD)(UPTRINT) Ptr | TYPE_Free;

		// Serialize to HDD.
		BufferedFileWriter << FreeInfo;

		// Re-enable allocation tracking again.
		PossiblySplitMprof();
	}
}

/**
 * Tracks realloc operation
 *
 * @param	OldPtr	Previous pointer
 * @param	NewPtr	New pointer
 * @param	NewSize	New size of allocation
 */
void FMallocProfiler::TrackRealloc( void* OldPtr, void* NewPtr, DWORD NewSize )
{
	// Avoid tracking operations caused by tracking!
	if( !bEndProfilingHasBeenCalled && (OldPtr || NewPtr) && IsOutsideTrackingFunction() )
	{
		FScopedMallocProfilerLock MallocProfilerLock;

		// Gather information about operation.
		FProfilerReallocInfo ReallocInfo;
		ReallocInfo.OldPointer		= (QWORD)(UPTRINT) OldPtr | TYPE_Realloc;
		ReallocInfo.NewPointer		= (QWORD)(UPTRINT) NewPtr;
		ReallocInfo.CallStackIndex	= GetCallStackIndex();
		ReallocInfo.Size			= NewSize;

		// Serialize to HDD.
		BufferedFileWriter << ReallocInfo;
		MALLOC_PROFILER_DECODE_SCRIPT( CallStackDecoder->SerializeScriptCallstack(BufferedFileWriter); )

		// Re-enable allocation tracking again.
		PossiblySplitMprof();
	}
}

/**
 * Tracks memory allocations stats every 1024 memory operations. Used for time line view in memory profiler app.
 */
void FMallocProfiler::TrackSpecialMemory()
{
	if (!bEndProfilingHasBeenCalled && ((MemoryOperationCount++ & 0x3FF) == 0))
	{
		// Avoid tracking operations caused by tracking!
		if (SyncObjectLockCount == 0)
		{
			// Write marker snapshot to stream.
			FProfilerOtherInfo SnapshotMarker;
			SnapshotMarker.DummyPointer	= TYPE_Other;
			SnapshotMarker.SubType = SUBTYPE_MemoryAllocationStats;
			SnapshotMarker.Payload = 0;
			BufferedFileWriter << SnapshotMarker;

			WriteMemoryAllocationStats();
		}
	}
}

/**
 * Begins profiling operation and opens file.
 */
void FMallocProfiler::BeginProfiling()
{
	FScopedMallocProfilerLock MallocProfilerLock;

	// Create a new script callstack decoder.
	MALLOC_PROFILER_DECODE_SCRIPT( CallStackDecoder = new FScriptCallStackDecoder(); )

	// Serialize dummy header, overwritten in EndProfiling.
	FProfilerHeader DummyHeader;
	appMemzero( &DummyHeader, sizeof(DummyHeader) );
	BufferedFileWriter << DummyHeader;
}

/** 
	Added for untracked memory calculation
	Note that this includes all the memory used by dependent malloc profilers, such as FMallocGcmProfiler,
	so they don't need their own version of this function. 
*/
INT FMallocProfiler::CalculateMemoryProfilingOverhead()
{
	return (INT)(ProgramCounterToIndexMap.GetAllocatedSize()
			+ CallStackAddressInfoArray.GetAllocatedSize()
			+ CRCToCallStackIndexMap.GetAllocatedSize()
			+ CallStackInfoBuffer.GetAllocatedSize()
			+ NameToNameTableIndexMap.GetAllocatedSize()
			+ NameArray.GetAllocatedSize()
#if USE_MALLOC_PROFILER_DECODE_SCRIPT
			+ CallStackDecoder->GetAllocatedSize()
#endif
			+ BufferedFileWriter.GetAllocatedSize());
}

/** 
 * For externing in files where you can't include FMallocProfiler.h. 
 */
void FMallocProfiler_PanicDump(INT FailedOperation, void* Ptr1, void* Ptr2)
{
	if( GMallocProfiler )
	{
		GMallocProfiler->PanicDump((EProfilingPayloadType)FailedOperation, Ptr1, Ptr2);
	}
}

/** 
 * This function is intended to be called when a fatal error has occurred inside the allocator and
 * you want to dump the current mprof before crashing, so that it can be used to help debug the error.
 * IMPORTANT: This function assumes that this thread already has the allocator mutex.. 
 */
void FMallocProfiler::PanicDump(EProfilingPayloadType FailedOperation, void* Ptr1, void* Ptr2)
{
	FString OperationString = TEXT("Invalid");
	switch (FailedOperation)
	{
		case TYPE_Malloc:
			OperationString = TEXT("Malloc");
			break;

		case TYPE_Free:
			OperationString = TEXT("Free");
			break;

		case TYPE_Realloc:
			OperationString = TEXT("Realloc");
			break;
	}

	appOutputDebugStringf(TEXT("FMallocProfiler::PanicDump called! Failed operation: %s, Ptr1: %08x, Ptr2: %08x"), *OperationString, Ptr1, Ptr2);

	CriticalSection.Unlock();
	EndProfiling();
	CriticalSection.Lock();
}

/**
 * Ends profiling operation and closes file.
 */
void FMallocProfiler::EndProfiling()
{
	debugf(TEXT("FMallocProfiler: dumping file [%s]"),*BufferedFileWriter.FullFilepath);
	{
		FScopeLock Lock( &CriticalSection );
		FScopedMallocProfilerLock MallocProfilerLock;

		bEndProfilingHasBeenCalled = TRUE;

		// Write end of stream marker.
		FProfilerOtherInfo EndOfStream;
		EndOfStream.DummyPointer	= TYPE_Other;
		EndOfStream.SubType			= SUBTYPE_EndOfStreamMarker;
		EndOfStream.Payload			= 0;
		BufferedFileWriter << EndOfStream;

		WriteAdditionalSnapshotMemoryStats();

#if SERIALIZE_SYMBOL_INFO
		// Look up symbols on platforms supporting it at runtime.
		for( INT AddressIndex=0; AddressIndex<CallStackAddressInfoArray.Num(); AddressIndex++ )
		{
			FCallStackAddressInfo&	AddressInfo = CallStackAddressInfoArray(AddressIndex);
			// Look up symbols.
			const FProgramCounterSymbolInfo SymbolInfo	= appProgramCounterToSymbolInfo( AddressInfo.ProgramCounter );

			// Convert to strings, and clean up in the process.
			const FString ModulName		= FFilename(SymbolInfo.ModuleName).GetCleanFilename();
			const FString FileName		= FString(SymbolInfo.Filename);
			const FString FunctionName	= FString(SymbolInfo.FunctionName);

			// Propagate to our own struct, also populating name table.
			AddressInfo.FilenameNameTableIndex	= GetNameTableIndex( FileName );
			AddressInfo.FunctionNameTableIndex	= GetNameTableIndex( FunctionName );
			AddressInfo.LineNumber				= SymbolInfo.LineNumber;
		}
#endif // SERIALIZE_SYMBO_INFO

		// Archive used to write out symbol information. This will always be written to the first file, which
		// in the case of multiple files won't be pointed to by BufferedFileWriter.
		FArchive* SymbolFileWriter = NULL;
		// Use the BufferedFileWriter if we're still on the first file.
		if( BufferedFileWriter.FileNumber == 0 )
		{
			SymbolFileWriter = &BufferedFileWriter;
		}
		else
		{
			// Close the last file and transfer it to the PC
			BufferedFileWriter.Close();
			warnf(TEXT("FMallocProfiler: done writing file [%s]"), *(BufferedFileWriter.FullFilepath) );

			FString PartFilename = BufferedFileWriter.BaseFilePath + FString::Printf(TEXT(".m%i"), BufferedFileWriter.FileNumber);
			SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!MEMORY:"), *PartFilename );

			// Create a file writer appending to the first file.
			BufferedFileWriter.FullFilepath = BufferedFileWriter.BaseFilePath + TEXT(".mprof");
			SymbolFileWriter = GFileManager->CreateFileWriter( *BufferedFileWriter.FullFilepath, FILEWRITE_Append );

			// Seek to the end of the first file.
			SymbolFileWriter->Seek( SymbolFileWriter->Tell() );
		}

		// Real header, written at start of the file but written out right before we close the file.
		FProfilerHeader Header;
		Header.Magic				= MEMORY_PROFILER_MAGIC;
		Header.Version				= MEMORY_PROFILER_VERSION;
		Header.Platform				= appGetPlatformType();
		Header.bShouldSerializeSymbolInfo = SERIALIZE_SYMBOL_INFO ? 1 : 0;
#if XBOX || PS3 || _WINDOWS || IPHONE
		Header.ExecutableName		= appExecutableName();
#endif // XBOX || PS3 || _WINDOWS
		Header.NumDataFiles			= BufferedFileWriter.FileNumber + 1;

		// Write out name table and update header with offset and count.
		Header.NameTableOffset	= SymbolFileWriter->Tell();
		Header.NameTableEntries	= NameArray.Num();
		for( INT NameIndex=0; NameIndex<NameArray.Num(); NameIndex++ )
		{
			SerializeStringAsANSICharArray( NameArray(NameIndex), *SymbolFileWriter );
		}

		// Write out callstack address infos and update header with offset and count.
		Header.CallStackAddressTableOffset	= SymbolFileWriter->Tell();
		Header.CallStackAddressTableEntries	= CallStackAddressInfoArray.Num();
		for( INT CallStackAddressIndex=0; CallStackAddressIndex<CallStackAddressInfoArray.Num(); CallStackAddressIndex++ )
		{
			(*SymbolFileWriter) << CallStackAddressInfoArray(CallStackAddressIndex);
		}

		// Write out callstack infos and update header with offset and count.
		Header.CallStackTableOffset			= SymbolFileWriter->Tell();
		Header.CallStackTableEntries		= CallStackInfoBuffer.Num();

		CallStackInfoBuffer.Lock();
		for( INT CallStackIndex=0; CallStackIndex<CallStackInfoBuffer.Num(); CallStackIndex++ )
		{
			FCallStackInfo* CallStackInfo = (FCallStackInfo*) CallStackInfoBuffer.Access( CallStackIndex * sizeof(FCallStackInfo) );
			(*SymbolFileWriter) << (*CallStackInfo);
		}
		CallStackInfoBuffer.Unlock();

		Header.ModulesOffset				= SymbolFileWriter->Tell();
		Header.ModuleEntries				= appGetProcessModuleCount();

		TArray<FModuleInfo> ProcModules(Header.ModuleEntries);

		Header.ModuleEntries = appGetProcessModuleSignatures(&ProcModules(0), ProcModules.Num());

		for(DWORD ModuleIndex = 0; ModuleIndex < Header.ModuleEntries; ++ModuleIndex)
		{
			FModuleInfo &CurModule = ProcModules(ModuleIndex);

			(*SymbolFileWriter) << CurModule.BaseOfImage
								<< CurModule.ImageSize
								<< CurModule.TimeDateStamp
								<< CurModule.PdbSig
								<< CurModule.PdbAge
								<< CurModule.PdbSig70.Data1
								<< CurModule.PdbSig70.Data2
								<< CurModule.PdbSig70.Data3
								<< *((DWORD*)&CurModule.PdbSig70.Data4[0])
								<< *((DWORD*)&CurModule.PdbSig70.Data4[4]);

			SerializeStringAsANSICharArray(CurModule.ModuleName, (*SymbolFileWriter));
			SerializeStringAsANSICharArray(CurModule.ImageName, (*SymbolFileWriter));
			SerializeStringAsANSICharArray(CurModule.LoadedImageName, (*SymbolFileWriter));
		}

#if USE_MALLOC_PROFILER_DECODE_SCRIPT
		Header.ScriptCallstackTableOffset = SymbolFileWriter->Tell();
		CallStackDecoder->SerializeCallstackTable(*SymbolFileWriter);

		Header.ScriptNameTableOffset = SymbolFileWriter->Tell();
		CallStackDecoder->SerializeNameTable(*SymbolFileWriter);
#else
		// Invalid value to indicate that there are no decoded script callstacks in the malloc stream.
		Header.ScriptCallstackTableOffset = (DWORD)-1;
		Header.ScriptNameTableOffset = (DWORD)-1;
#endif

		// Seek to the beginning of the file and write out proper header.
		SymbolFileWriter->Seek( 0 );
		(*SymbolFileWriter) << Header;

		// Close file writers.
		SymbolFileWriter->Close();
		if( SymbolFileWriter != &BufferedFileWriter )
		{
			BufferedFileWriter.Close();
		}

		bOutputFileClosed = TRUE;
	}

	warnf(TEXT("FMallocProfiler: done writing file [%s]"), *(BufferedFileWriter.FullFilepath) );

	// Send the final part
	SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!MEMORY:"), *(BufferedFileWriter.FullFilepath) );
}

/**
 * Returns index of passed in program counter into program counter array. If not found, adds it.
 *
 * @param	ProgramCounter	Program counter to find index for
 * @return	Index of passed in program counter if it's not NULL, INDEX_NONE otherwise
 */
INT FMallocProfiler::GetProgramCounterIndex( QWORD ProgramCounter )
{
	INT	Index = INDEX_NONE;

	// Look up index in unique array of program counter infos, if we have a valid program counter.
	if( ProgramCounter )
	{
		// Use index if found.
		INT* IndexPtr = ProgramCounterToIndexMap.Find( ProgramCounter );
		if( IndexPtr )
		{
			Index = *IndexPtr;
		}
		// Encountered new program counter, add to array and set index mapping.
		else
		{
			// Add to aray and set mapping for future use.
			Index = CallStackAddressInfoArray.AddZeroed();
			ProgramCounterToIndexMap.Set( ProgramCounter, Index );

			// Only initialize program counter, rest will be filled in at the end, once symbols are loaded.
			FCallStackAddressInfo& CallStackAddressInfo = CallStackAddressInfoArray(Index);
			CallStackAddressInfo.ProgramCounter = ProgramCounter;
		}
		check(Index!=INDEX_NONE);
	}

	return Index;
}

/** 
 * Returns index of callstack, captured by this function into array of callstack infos. If not found, adds it.
 *
 * @return index into callstack array
 */
INT FMallocProfiler::GetCallStackIndex()
{
	// Index of callstack in callstack info array.
	INT Index = INDEX_NONE;

	// Capture callstack and create CRC.
	QWORD FullBackTrace[MEMORY_PROFILER_MAX_BACKTRACE_DEPTH + MEMORY_PROFILER_SKIP_NUM_BACKTRACE_ENTRIES];
	appCaptureStackBackTrace( FullBackTrace, MEMORY_PROFILER_MAX_BACKTRACE_DEPTH + MEMORY_PROFILER_SKIP_NUM_BACKTRACE_ENTRIES );
	// Skip first 5 entries as they are inside the allocator.
	QWORD* BackTrace = &FullBackTrace[MEMORY_PROFILER_SKIP_NUM_BACKTRACE_ENTRIES];
	DWORD CRC = appMemCrc( BackTrace, MEMORY_PROFILER_MAX_BACKTRACE_DEPTH * sizeof(QWORD), 0 );

	// Use index if found
	INT* IndexPtr = CRCToCallStackIndexMap.Find( CRC );
	if( IndexPtr )
	{
		Index = *IndexPtr;
	}
	// Encountered new call stack, add to array and set index mapping.
	else
	{
		// Set mapping for future use.
		Index = CallStackInfoBuffer.Num();
		CRCToCallStackIndexMap.Set( CRC, Index );

		// Set up callstack info with captured call stack, translating program counters
		// to indices in program counter array (unique entries).
		FCallStackInfo CallStackInfo;
		CallStackInfo.CRC = CRC;
		for( INT i=0; i<MEMORY_PROFILER_MAX_BACKTRACE_DEPTH; i++ )
		{
			CallStackInfo.AddressIndices[i] = GetProgramCounterIndex( BackTrace[i] );
		}

		// Append to compressed buffer.
		CallStackInfoBuffer.Append( &CallStackInfo, sizeof(FCallStackInfo) );
	}

	check(Index!=INDEX_NONE);
	return Index;
}	

/**
 * Returns index of passed in name into name array. If not found, adds it.
 *
 * @param	Name	Name to find index for
 * @return	Index of passed in name
 */
INT FMallocProfiler::GetNameTableIndex( const FString& Name )
{
	// Index of name in name table.
	INT Index = INDEX_NONE;

	// Use index if found.
	INT* IndexPtr = NameToNameTableIndexMap.Find( Name );
	if( IndexPtr )
	{
		Index = *IndexPtr;
	}
	// Encountered new name, add to array and set index mapping.
	else
	{
		Index = NameArray.Num();
		new(NameArray)FString(Name);
		NameToNameTableIndexMap.Set(*Name,Index);
	}

	check(Index!=INDEX_NONE);
	return Index;
}

/**
 * Exec handler. Parses command and returns TRUE if handled.
 *
 * @param	Cmd		Command to parse
 * @param	Ar		Output device to use for logging
 * @return	TRUE if handled, FALSE otherwise
 */
UBOOL FMallocProfiler::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// End profiling.
	if (ParseCommand(&Cmd, TEXT("MPROF")))
	{
		if (ParseCommand(&Cmd, TEXT("START")))
		{
			if (bEndProfilingHasBeenCalled)
			{
				warnf(TEXT("FMallocProfiler: Memory recording has already been stopped and cannot be restarted."));
			}
			else
			{
				warnf(TEXT("FMallocProfiler: Memory recording is automatically started when the game is run and is still running."));
			}
		}
		else if (ParseCommand(&Cmd, TEXT("STOP")))
		{
			if (bEndProfilingHasBeenCalled)
			{
				warnf(TEXT("FMallocProfiler: Memory recording has already been stopped."));
			}
			else
			{
				warnf(TEXT("FMallocProfiler: Stopping recording."));
				EndProfiling();
			}
		}
		else if (ParseCommand(&Cmd, TEXT("MARK")) || ParseCommand(&Cmd, TEXT("SNAPSHOT")))
		{
			if (bEndProfilingHasBeenCalled == TRUE)
			{
				warnf(TEXT("FMallocProfiler: Memory recording has already been stopped.  Markers have no meaning at this point."));
			}
			else
			{
				FString SnapshotName;
				ParseToken(Cmd, SnapshotName, TRUE);

				Ar.Logf(TEXT("FMallocProfiler: Recording a snapshot marker %s"), *SnapshotName);
				SnapshotMemory(SUBTYPE_SnapshotMarker, SnapshotName);
			}
		}
		else
		{
			if (bEndProfilingHasBeenCalled)
			{
				warnf(TEXT("FMallocProfiler: Status: Memory recording has been stopped."));
			}
			else
			{
				warnf(TEXT("FMallocProfiler: Status: Memory recording is ongoing."));
				warnf(TEXT("  Use MPROF MARK [FriendlyName] to insert a marker."));
				warnf(TEXT("  Use MPROF STOP to stop recording and write the recording to disk."));
			}
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("DUMPALLOCSTOFILE")) )
	{
		if( bEndProfilingHasBeenCalled == TRUE )
		{
			warnf(TEXT("FMallocProfiler: EndProfiling() has been called further actions will not be recorded please restart memory tracking process"));
			return TRUE;
		}

		warnf(TEXT("FMallocProfiler: DUMPALLOCSTOFILE"));
		EndProfiling();
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("SNAPSHOTMEMORY")) )
	{
		if( bEndProfilingHasBeenCalled == TRUE )
		{
			warnf(TEXT("FMallocProfiler: EndProfiling() has been called further actions will not be recorded please restart memory tracking process"));
			return TRUE;
		}

		FString SnapshotName;
		ParseToken(Cmd, SnapshotName, TRUE);

		Ar.Logf(TEXT("FMallocProfiler: SNAPSHOTMEMORY %s"), *SnapshotName);
		SnapshotMemory(SUBTYPE_SnapshotMarker, SnapshotName);
		return TRUE;
	}
	// Do not use this.
	else if( ParseCommand(&Cmd,TEXT("SNAPSHOTMEMORYFRAME")) )
	{
		if (!bEndProfilingHasBeenCalled)
		{
			EmbedFloatMarker(SUBTYPE_FrameTimeMarker, (FLOAT)GDeltaTime);
		}
		return TRUE;
	}

	return UsedMalloc->Exec(Cmd, Ar);
}

/**
 * Embeds token into stream to snapshot memory at this point.
 */
void FMallocProfiler::SnapshotMemory(EProfilingPayloadSubType SubType, const FString& MarkerName)
{
	FScopeLock Lock( &CriticalSection );
	FScopedMallocProfilerLock MallocProfilerLock;

	// Write snapshot marker to stream.
	FProfilerOtherInfo SnapshotMarker;
	SnapshotMarker.DummyPointer	= TYPE_Other;
	SnapshotMarker.SubType = SubType;
	SnapshotMarker.Payload = GetNameTableIndex(MarkerName);
	BufferedFileWriter << SnapshotMarker;

	WriteAdditionalSnapshotMemoryStats();
}

/**
 * Embeds token into stream to snapshot memory at this point.
 */
void FMallocProfiler::EmbedFloatMarker(EProfilingPayloadSubType SubType, FLOAT DeltaTime)
{
	FScopeLock Lock( &CriticalSection );
	FScopedMallocProfilerLock MallocProfilerLock;

	union { FLOAT f; DWORD ui; } TimePacker;
	TimePacker.f = DeltaTime;

	// Write marker snapshot to stream.
	FProfilerOtherInfo SnapshotMarker;
	SnapshotMarker.DummyPointer	= TYPE_Other;
	SnapshotMarker.SubType = SubType;
	SnapshotMarker.Payload = TimePacker.ui;
	BufferedFileWriter << SnapshotMarker;
}

/**
 * Embeds token into stream to snapshot memory at this point.
 */
void FMallocProfiler::EmbedDwordMarker(EProfilingPayloadSubType SubType, DWORD Info)
{
	if (Info != 0)
	{
		FScopeLock Lock( &CriticalSection );
		FScopedMallocProfilerLock MallocProfilerLock;

		// Write marker snapshot to stream.
		FProfilerOtherInfo SnapshotMarker;
		SnapshotMarker.DummyPointer	= TYPE_Other;
		SnapshotMarker.SubType = SubType;
		SnapshotMarker.Payload = Info;
		BufferedFileWriter << SnapshotMarker;
	}
}

/** Writes memory allocations stats. */
void FMallocProfiler::WriteMemoryAllocationStats()
{
	FMemoryAllocationStats MemStats;
	UsedMalloc->GetAllocationInfo( MemStats );
	GetTexturePoolSize( MemStats );

	BYTE StatsCount = FMemoryAllocationStats::GetStatsNum();
	BufferedFileWriter << StatsCount;

	// Convert memory allocations stats into more reliable format. Size of SIZE_T is platform-dependent.
	for( INT StatIndex = 0; StatIndex < StatsCount; StatIndex++ )
	{
		// Serialize as SQWORDs.
		SQWORD Value = (SQWORD)(*( (SIZE_T*)&MemStats + StatIndex ));
		BufferedFileWriter << Value;
	}
}

/** 
 * Writes additional memory stats for a snapshot like memory allocations stats, list of all loaded levels and platform dependent memory metrics.
 */
void FMallocProfiler::WriteAdditionalSnapshotMemoryStats()
{
	WriteMemoryAllocationStats();

	// Write "stat levels" information.
	WriteLoadedLevels();

	// Write memory metrics, need to be redone. It could be used to store platform dependent memory metrics.
	FMemoryAllocationStats MemStats;
	UsedMalloc->GetAllocationInfo( MemStats );

	SQWORD MemoryProfilingOverhead = CalculateMemoryProfilingOverhead();

	SQWORD AllocatedFromOS = 0;
	SQWORD MaxAllocatedFromOS = 0;
	SQWORD TotalUsedChunks = 0;
	SQWORD AllocateFromGame = 0;

#if PS3 && USE_FMALLOC_DL
	SIZE_T TempAllocatedFromOS, TempMaxAllocatedFromOS, TempAllocatedFromGame;
	((FMallocPS3DL*)UsedMalloc)->GetDLMallocStats( TempAllocatedFromOS, TempMaxAllocatedFromOS, TempAllocatedFromGame, NULL );
	AllocatedFromOS = TempAllocatedFromOS;
	MaxAllocatedFromOS = TempMaxAllocatedFromOS;
	AllocateFromGame = TempAllocatedFromGame;

	INT TempNumSegments = 0;
	INT TempTotalUsedChunks = 0;
	((FMallocPS3DL*)UsedMalloc)->GetCustomStats(TempNumSegments, TempTotalUsedChunks);
	TotalUsedChunks = TempTotalUsedChunks;
#endif

	SQWORD TextureLightmapMemory = 0;
	SQWORD TextureShadowmapMemory = 0;
#if STATS
	TextureLightmapMemory = GStatManager.GetStatValueDWORD(STAT_TextureLightmapMemory);
	TextureShadowmapMemory = GStatManager.GetStatValueDWORD(STAT_TextureShadowmapMemory);
#endif

	// IMPORTANT: All metrics MUST be serialized as SQWORDs!
	BYTE NumMetrics = 7;
	BufferedFileWriter << NumMetrics
						<< MemoryProfilingOverhead
						<< AllocatedFromOS
						<< MaxAllocatedFromOS
						<< AllocateFromGame
						<< TotalUsedChunks
						<< TextureLightmapMemory
						<< TextureShadowmapMemory;
}

/** 
 * Checks whether file is too big and will create new files with different extension but same base name.
 */
void FMallocProfiler::PossiblySplitMprof()
{
	// Nothing to do if we haven't created a file write yet. This happens at startup as GFileManager is initialized after
	// quite a few allocations.
	if( !bEndProfilingHasBeenCalled && !BufferedFileWriter.IsBufferingToMemory() )
	{
		const INT CurrentSize = BufferedFileWriter.Tell();

		// Create a new file if current one exceeds 1 GByte.
		#define SIZE_OF_DATA_FILE 1 * 1024 * 1024 * 1024

		if (CurrentSize > SIZE_OF_DATA_FILE) 
		{
			warnf( TEXT("FMallocProfiler: Splitting recording into a new file.") );
			BufferedFileWriter.Split();
		}
	}
}

/** Snapshot taken when engine has started the cleaning process before loading a new level. */
void FMallocProfiler::SnapshotMemoryLoadMapStart(const FString& Tag)
{
	if (GMallocProfiler && !GMallocProfiler->bEndProfilingHasBeenCalled)
	{
		GMallocProfiler->SnapshotMemory(SUBTYPE_SnapshotMarker_LoadMap_Start, Tag);
	}
}

/** Snapshot taken when a new level has started loading. */
void FMallocProfiler::SnapshotMemoryLoadMapMid(const FString& Tag)
{
	if (GMallocProfiler && !GMallocProfiler->bEndProfilingHasBeenCalled)
	{
		GMallocProfiler->SnapshotMemory(SUBTYPE_SnapshotMarker_LoadMap_Mid, Tag);
	}
}

/** Snapshot taken when a new level has been loaded. */
void FMallocProfiler::SnapshotMemoryLoadMapEnd(const FString& Tag)
{
	if (GMallocProfiler && !GMallocProfiler->bEndProfilingHasBeenCalled)
	{
		GMallocProfiler->SnapshotMemory(SUBTYPE_SnapshotMarker_LoadMap_End, Tag);
	}
}

/** Snapshot taken when garbage collection has started. */
void FMallocProfiler::SnapshotMemoryGCStart()
{
	if (GMallocProfiler && !GMallocProfiler->bEndProfilingHasBeenCalled)
	{
		GMallocProfiler->SnapshotMemory(SUBTYPE_SnapshotMarker_GC_Start, FString());
	}
}

/** Snapshot taken when garbage collection has finished. */
void FMallocProfiler::SnapshotMemoryGCEnd()
{
	if (GMallocProfiler && !GMallocProfiler->bEndProfilingHasBeenCalled)
	{
		GMallocProfiler->SnapshotMemory(SUBTYPE_SnapshotMarker_GC_End, FString());
	}
}

/** Snapshot taken when a new streaming level has been requested to load. */
void FMallocProfiler::SnapshotMemoryLevelStreamStart(const FString& Tag)
{
	if (GMallocProfiler && !GMallocProfiler->bEndProfilingHasBeenCalled)
	{
		GMallocProfiler->SnapshotMemory(SUBTYPE_SnapshotMarker_LevelStream_Start, Tag);
	}
}

/** Snapshot taken when a previously  streamed level has been made visible. */
void FMallocProfiler::SnapshotMemoryLevelStreamEnd(const FString& Tag)
{
	if (GMallocProfiler && !GMallocProfiler->bEndProfilingHasBeenCalled)
	{
		GMallocProfiler->SnapshotMemory(SUBTYPE_SnapshotMarker_LevelStream_End, Tag);
	}
}

/** 
 * Writes names of currently loaded levels.
 */
void FMallocProfiler::WriteLoadedLevels()
{
	// Write a 0 count for loaded levels.
	WORD NumLoadedLevels = 0;
	BufferedFileWriter << NumLoadedLevels;
}

/** 
 * Gather texture memory stats. 
 */
void FMallocProfiler::GetTexturePoolSize( FMemoryAllocationStats& MemoryStats )
{
	MemoryStats.AllocatedTextureMemorySize = 0;
	MemoryStats.AvailableTextureMemorySize = 0;
}

/*=============================================================================
	FMallocProfilerBufferedFileWriter implementation
=============================================================================*/

/**
 * Constructor. Called before GMalloc is initialized!!!
 */
FMallocProfilerBufferedFileWriter::FMallocProfilerBufferedFileWriter()
:	FileWriter( NULL )
,	FileNumber( 0 )
{
	ArIsSaving		= TRUE;
	ArIsPersistent	= TRUE;
	BaseFilePath = TEXT("");
}

/**
 * Destructor, cleaning up FileWriter.
 */
FMallocProfilerBufferedFileWriter::~FMallocProfilerBufferedFileWriter()
{
	if( FileWriter )
	{
		delete FileWriter;
		FileWriter = NULL;
	}
}

/**
 * Whether we are currently buffering to memory or directly writing to file.
 *
 * @return	TRUE if buffering to memory, FALSE otherwise
 */
UBOOL FMallocProfilerBufferedFileWriter::IsBufferingToMemory()
{
	return FileWriter == NULL;
}

/**
 * Splits the file and creates a new one with different extension.
 */
void FMallocProfilerBufferedFileWriter::Split()
{
	// Increase file number.
	FileNumber++;

	// Delete existing file writer. A new one will be automatically generated
	// the first time further data is being serialized.
	if( FileWriter )
	{
		// Serialize the end-of-file marker
		FProfilerOtherInfo EndOfFileToken;
		EndOfFileToken.DummyPointer	= TYPE_Other;
		EndOfFileToken.SubType = SUBTYPE_EndOfFileMarker;
		EndOfFileToken.Payload = 0;
		Serialize(&EndOfFileToken, sizeof(EndOfFileToken));

		FullFilepath = "";
		delete FileWriter;
		FileWriter = NULL;

		// Copy the file over to the PC if it's not the first file
		// (the first one gets copied over when the recording is done)
		if (FileNumber > 1)
		{
			FString PartFilename = BaseFilePath + FString::Printf(TEXT(".m%i"), FileNumber - 1);
			SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!MEMORY:"), *PartFilename );
		}
	}
}

// FArchive interface.

void FMallocProfilerBufferedFileWriter::Serialize( void* V, INT Length )
{
#if ALLOW_DEBUG_FILES
	// Copy to buffered memory array if file manager hasn't been set up yet.
	if( GFileManager == NULL || !GFileManager->IsInitialized() )
	{
		const INT Index = BufferedData.Add( Length );
		appMemcpy( &BufferedData(Index), V, Length );
	} 
	// File manager is set up but we haven't created file writer yet, do it now.
	else if( (FileWriter == NULL) && !GMallocProfiler->bOutputFileClosed )
	{
		// Get the base path (only used once to prevent the system time from changing for multi-file-captures
		if (BaseFilePath == TEXT(""))
		{
			const FString SysTime = appSystemTimeString();
			BaseFilePath = appProfilingDir() + GGameName + TEXT("-") + appPlatformTypeToString(appGetPlatformType()) + TEXT("-") + SysTime + PATH_SEPARATOR + GGameName;
		}
			
		// Create file writer to serialize data to HDD.
		if( FullFilepath.GetBaseFilename() == TEXT("") )
		{
			// Use .mprof extension for first file.
			if( FileNumber == 0 )
			{
				FullFilepath = BaseFilePath + TEXT(".mprof");
			}
			// Use .mX extension for subsequent files.
			else
			{
				FullFilepath = BaseFilePath + FString::Printf(TEXT(".m%i"),FileNumber);
			}
		}

		GFileManager->MakeDirectory( *BaseFilePath );
		FileWriter = GFileManager->CreateFileWriter( *FullFilepath, FILEWRITE_NoFail );
		checkf( FileWriter );

		// Serialize existing buffered data and empty array.
		FileWriter->Serialize( BufferedData.GetData(), BufferedData.Num() );
		BufferedData.Empty();
	}

	// Serialize data to HDD via FileWriter if it already has been created.
	if( FileWriter && ((GMallocProfiler == NULL) || !GMallocProfiler->bOutputFileClosed))
	{
		FileWriter->Serialize( V, Length );
	}
#endif
}

void FMallocProfilerBufferedFileWriter::Seek( INT InPos )
{
	check( FileWriter );
	FileWriter->Seek( InPos );
}

UBOOL FMallocProfilerBufferedFileWriter::Close()
{
	check( FileWriter );

	UBOOL bResult = FileWriter->Close();

	delete FileWriter;
	FileWriter = NULL;

	return bResult;
}

INT FMallocProfilerBufferedFileWriter::Tell()
{
	check( FileWriter );
	return FileWriter->Tell();
}

DWORD FMallocProfilerBufferedFileWriter::GetAllocatedSize()
{
	//@TODO: Currently there is no way to request the full size of a platform specfic archiver, these are just
	// approximate sizes based on the buffer size in each platform
	DWORD FileWriterSize = sizeof(FArchive);
#if PS3
	FileWriterSize += 8192;
#elif _WINDOWS
	FileWriterSize += 1024;
#else
	FileWriterSize += 4096;
#endif

	return FileWriterSize + BufferedData.GetAllocatedSize();
}

/*-----------------------------------------------------------------------------
	FScopedMallocProfilerLock
-----------------------------------------------------------------------------*/

/** Constructor that performs a lock on the malloc profiler tracking methods. */
FScopedMallocProfilerLock::FScopedMallocProfilerLock()
{
	check( GMallocProfiler );

	GMallocProfiler->SyncObject.Lock();
	GMallocProfiler->ThreadId = appGetCurrentThreadId();
	GMallocProfiler->SyncObjectLockCount++;
}

/** Destructor that performs a release on the malloc profiler tracking methods. */
FScopedMallocProfilerLock::~FScopedMallocProfilerLock()
{
	GMallocProfiler->SyncObjectLockCount--;
	GMallocProfiler->SyncObject.Unlock();
}

#else //USE_MALLOC_PROFILER

// Suppress linker warning "warning LNK4221: no public symbols found; archive member will be inaccessible"
INT FMallocProfilerLinkerHelper;

#endif //
