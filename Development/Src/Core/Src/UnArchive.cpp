/*=============================================================================
	UnArchive.cpp: Core archive classes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	FArchive implementation.
-----------------------------------------------------------------------------*/

/**
 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
 * is in when a loading error occurs.
 *
 * This is overridden for the specific Archive Types
 **/
FString FArchive::GetArchiveName() const
{
	return TEXT("FArchive");
}

FString FArchiveProxy::GetArchiveName() const
{
	return InnerArchive.GetArchiveName();
}

/**
 * Serialize the given FName as an FString
 */
FArchive& FNameAsStringProxyArchive::operator<<( class FName& N )
{
	if (IsLoading())
	{
		FString LoadedString;
		InnerArchive << LoadedString;
		N = FName(*LoadedString);
		return InnerArchive;
	}
	else
	{
		FString SavedString(N.ToString());
		return InnerArchive << SavedString;
	}
}


 /**
  * Serialize the given UObject* as an FString
  */
FArchive& FObjectAndNameAsStringProxyArchive::operator<<(class UObject*& Obj)
{
	if (IsLoading())
	{
		// load the path name to the object
		FString LoadedString;
		InnerArchive << LoadedString;
		// look up the object by fully qualified pathname
		Obj = FindObject<UObject>(NULL, *LoadedString, FALSE);
		return InnerArchive;
	}
	else
	{
		// save out the fully qualified object name
		FString SavedString(Obj->GetPathName());
		return InnerArchive << SavedString;
	}
}



/** 
 * FCompressedChunkInfo serialize operator.
 */
FArchive& operator<<( FArchive& Ar, FCompressedChunkInfo& Chunk )
{
	// The order of serialization needs to be identical to the memory layout as the async IO code is reading it in bulk.
	// The size of the structure also needs to match what is being serialized.
	Ar << Chunk.CompressedSize;
	Ar << Chunk.UncompressedSize;
	return Ar;
}

/** Accumulative time spent in IsSaving portion of SerializeCompressed. */
DOUBLE GArchiveSerializedCompressedSavingTime = 0;

// MT compression disabled on console due to memory impact and lack of beneficial usage case.
#define WITH_MULTI_THREADED_COMPRESSION (!CONSOLE)
#if WITH_MULTI_THREADED_COMPRESSION
// Helper structure to keep information about async chunks that are in-flight.
class FAsyncCompressionChunk : public FNonAbandonableTask
{
public:
	/** Pointer to source (uncompressed) memory.								*/
	void* UncompressedBuffer;
	/** Pointer to destination (compressed) memory.								*/
	void* CompressedBuffer;
	/** Compressed size in bytes as passed to/ returned from compressor.		*/
	INT CompressedSize;
	/** Uncompressed size in bytes as passed to compressor.						*/
	INT UncompressedSize;
	/** Flags to control compression											*/
	ECompressionFlags Flags;

	/**
	 * Constructor, zeros everything
	 */
	FAsyncCompressionChunk()
		: UncompressedBuffer(0)
		, CompressedBuffer(0)
		, CompressedSize(0)
		, UncompressedSize(0)
		, Flags(ECompressionFlags(0))
	{
	}
	/**
	 * Performs the async compression
	 */
	void DoWork()
	{
		// Compress from memory to memory.
		verify( appCompressMemory( Flags, CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize ) );
	}

	/** Give the name for external event viewers
	* @return	the name to display in external event viewers
	*/
	static const TCHAR *Name()
	{
		return TEXT("FAsyncCompressionChunks");
	}
};
#endif

/**
 * Serializes and compresses/ uncompresses data. This is a shared helper function for compression
 * support. The data is saved in a way compatible with FIOSystem::LoadCompressedData.
 *
 * @note: the way this code works needs to be in line with FIOSystem::LoadCompressedData implementations
 * @note: the way this code works needs to be in line with FAsyncIOSystemBase::FulfillCompressedRead
 *
 * @param	V		Data pointer to serialize data from/to, or a FileReader if bTreatBufferAsFileReader is TRUE
 * @param	Length	Length of source data if we're saving, unused otherwise
 * @param	Flags	Flags to control what method to use for [de]compression and optionally control memory vs speed when compressing
 * @param	bTreatBufferAsFileReader TRUE if V is actually an FArchive, which is used when saving to read data - helps to avoid single huge allocations of source data
 */
void FArchive::SerializeCompressed( void* V, INT Length, ECompressionFlags Flags, UBOOL bTreatBufferAsFileReader )
{
	if( IsLoading() )
	{
		// Serialize package file tag used to determine endianess.
		FCompressedChunkInfo PackageFileTag;
		PackageFileTag.CompressedSize	= 0;
		PackageFileTag.UncompressedSize	= 0;
		*this << PackageFileTag;
		UBOOL bWasByteSwapped = PackageFileTag.CompressedSize != PACKAGE_FILE_TAG;

		// Read in base summary.
		FCompressedChunkInfo Summary;
		*this << Summary;

		if (bWasByteSwapped)
		{
			check( PackageFileTag.CompressedSize   == PACKAGE_FILE_TAG_SWAPPED );
			Summary.CompressedSize = BYTESWAP_ORDER32(Summary.CompressedSize);
			Summary.UncompressedSize = BYTESWAP_ORDER32(Summary.UncompressedSize);
			PackageFileTag.UncompressedSize = BYTESWAP_ORDER32(PackageFileTag.UncompressedSize);
		}
		else
		{
			check( PackageFileTag.CompressedSize   == PACKAGE_FILE_TAG );
		}

		// Handle change in compression chunk size in backward compatible way.
		INT LoadingCompressionChunkSize = PackageFileTag.UncompressedSize;
		if (LoadingCompressionChunkSize == PACKAGE_FILE_TAG)
		{
			LoadingCompressionChunkSize = LOADING_COMPRESSION_CHUNK_SIZE;
		}

		// Figure out how many chunks there are going to be based on uncompressed size and compression chunk size.
		INT	TotalChunkCount	= (Summary.UncompressedSize + LoadingCompressionChunkSize - 1) / LoadingCompressionChunkSize;
		
		// Allocate compression chunk infos and serialize them, keeping track of max size of compression chunks used.
		FCompressedChunkInfo*	CompressionChunks	= new FCompressedChunkInfo[TotalChunkCount];
		INT						MaxCompressedSize	= 0;
		for( INT ChunkIndex=0; ChunkIndex<TotalChunkCount; ChunkIndex++ )
		{
			*this << CompressionChunks[ChunkIndex];
			if (bWasByteSwapped)
			{
				CompressionChunks[ChunkIndex].CompressedSize	= BYTESWAP_ORDER32( CompressionChunks[ChunkIndex].CompressedSize );
				CompressionChunks[ChunkIndex].UncompressedSize	= BYTESWAP_ORDER32( CompressionChunks[ChunkIndex].UncompressedSize );
			}
			MaxCompressedSize = Max( CompressionChunks[ChunkIndex].CompressedSize, MaxCompressedSize );
		}

		INT Padding = 0;
#if PS3
		// This makes sure that there is a full valid cache line (and DMA read alignment) after the end
		// of the compressed data, so that the decompression function can safely over-read the source data.
		Padding = 128;
#endif

		// Set up destination pointer and allocate memory for compressed chunk[s] (one at a time).
		BYTE*	Dest				= (BYTE*) V;
		void*	CompressedBuffer	= appMalloc( MaxCompressedSize + Padding );

		// Iterate over all chunks, serialize them into memory and decompress them directly into the destination pointer
		for( INT ChunkIndex=0; ChunkIndex<TotalChunkCount; ChunkIndex++ )
		{
			const FCompressedChunkInfo& Chunk = CompressionChunks[ChunkIndex];
			// Read compressed data.
			Serialize( CompressedBuffer, Chunk.CompressedSize );
			// Decompress into dest pointer directly.
			verify( appUncompressMemory( Flags, Dest, Chunk.UncompressedSize, CompressedBuffer, Chunk.CompressedSize, (Padding > 0) ? TRUE : FALSE ) );
			// And advance it by read amount.
			Dest += Chunk.UncompressedSize;
		}

		// Free up allocated memory.
		appFree( CompressedBuffer );
		delete [] CompressionChunks;
	}
	else if( IsSaving() )
	{	
		SCOPE_SECONDS_COUNTER(GArchiveSerializedCompressedSavingTime);
		check( Length > 0 );

		// Serialize package file tag used to determine endianess in LoadCompressedData.
		FCompressedChunkInfo PackageFileTag;
		PackageFileTag.CompressedSize	= PACKAGE_FILE_TAG;
		PackageFileTag.UncompressedSize	= GSavingCompressionChunkSize;
		*this << PackageFileTag;

		// Figure out how many chunks there are going to be based on uncompressed size and compression chunk size.
		INT	TotalChunkCount	= (Length + GSavingCompressionChunkSize - 1) / GSavingCompressionChunkSize + 1;
		
		// Keep track of current position so we can later seek back and overwrite stub compression chunk infos.
		INT StartPosition = Tell();

		// Allocate compression chunk infos and serialize them so we can later overwrite the data.
		FCompressedChunkInfo* CompressionChunks	= new FCompressedChunkInfo[TotalChunkCount];
		for( INT ChunkIndex=0; ChunkIndex<TotalChunkCount; ChunkIndex++ )
		{
			*this << CompressionChunks[ChunkIndex];
		}

		// The uncompressd size is equal to the passed in length.
		CompressionChunks[0].UncompressedSize	= Length;
		// Zero initialize compressed size so we can update it during chunk compression.
		CompressionChunks[0].CompressedSize		= 0;

#if WITH_MULTI_THREADED_COMPRESSION

#define MAX_COMPRESSION_JOBS (16)
		// Don't scale more than 16x to avoid going overboard wrt temporary memory.
		FAsyncTask<FAsyncCompressionChunk> AsyncChunks[MAX_COMPRESSION_JOBS];

		// used to keep track of which job is the next one we need to retire
		INT AsyncChunkIndex[MAX_COMPRESSION_JOBS]={0};

		// Maximum number of concurrent async tasks we're going to kick off. This is based on the number of processors
		// available in the system.
		INT MaxConcurrentAsyncChunks = Clamp<INT>( GNumHardwareThreads - GNumUnusedThreads_SerializeCompressed, 1, MAX_COMPRESSION_JOBS );
		if (ParseParam(appCmdLine(), TEXT("MTCHILD")))
		{
			// throttle this back when doing MT cooks
			MaxConcurrentAsyncChunks = Min<INT>( MaxConcurrentAsyncChunks,4 );
		}

		// LZO compressor is only pseudo-thread safe via critical section.
		if( Flags & COMPRESS_LZO )
		{
			MaxConcurrentAsyncChunks = 1;
		}

		// Number of chunks left to finalize.
		INT NumChunksLeftToFinalize	= (Length + GSavingCompressionChunkSize - 1) / GSavingCompressionChunkSize;
		// Number of chunks left to kick off
		INT NumChunksLeftToKickOff	= NumChunksLeftToFinalize;
		// Start at index 1 as first chunk info is summary.
		INT	CurrentChunkIndex		= 1;
		// Start at index 1 as first chunk info is summary.
		INT	RetireChunkIndex		= 1;
	
		// Number of bytes remaining to kick off compression for.
		INT BytesRemainingToKickOff	= Length;
		// Pointer to src data if buffer is memory pointer, NULL if it's a FArchive.
		BYTE* SrcBuffer = bTreatBufferAsFileReader ? NULL : (BYTE*)V;

		check(!bTreatBufferAsFileReader || ((FArchive*)V)->IsLoading());
		check(NumChunksLeftToFinalize);

		// Loop while there is work left to do based on whether we have finalized all chunks yet.
		while( NumChunksLeftToFinalize )
		{
			// If TRUE we are waiting for async tasks to complete and should wait to complete some
			// if there are no async tasks finishing this iteration.
			UBOOL bNeedToWaitForAsyncTask = FALSE;

			// Try to kick off async tasks if there are chunks left to kick off.
			if( NumChunksLeftToKickOff )
			{
				// Find free index based on looking at uncompressed size. We can't use the thread counter
				// for this as that might be a chunk ready for finalization.
				INT FreeIndex = INDEX_NONE;
				for( INT i=0; i<MaxConcurrentAsyncChunks; i++ )
				{
					if( !AsyncChunkIndex[i] )
					{
						FreeIndex = i;
						check(AsyncChunks[FreeIndex].IsIdle()); // this is not supposed to be in use
						break;
					}
				}

				// Kick off async compression task if we found a chunk for it.
				if( FreeIndex != INDEX_NONE )
				{
					FAsyncCompressionChunk& NewChunk = AsyncChunks[FreeIndex].GetTask();
					// 2 times the uncompressed size should be more than enough; the compressed data shouldn't be that much larger
					NewChunk.CompressedSize	= 2 * GSavingCompressionChunkSize;
					// Allocate compressed buffer placeholder on first use.
					if( NewChunk.CompressedBuffer == NULL )
					{
						NewChunk.CompressedBuffer = appMalloc( NewChunk.CompressedSize	);
					}

					// By default everything is chunked up into GSavingCompressionChunkSize chunks.
					NewChunk.UncompressedSize	= Min( BytesRemainingToKickOff, GSavingCompressionChunkSize );
					check(NewChunk.UncompressedSize>0);

					// Need to serialize source data if passed in pointer is an FArchive.
					if( bTreatBufferAsFileReader )
					{
						// Allocate memory on first use. We allocate the maximum amount to allow reuse.
						if( !NewChunk.UncompressedBuffer )
						{
							NewChunk.UncompressedBuffer = appMalloc(GSavingCompressionChunkSize);
						}
						((FArchive*)V)->Serialize(NewChunk.UncompressedBuffer, NewChunk.UncompressedSize);
					}
					// Advance src pointer by amount to be compressed.
					else
					{
						NewChunk.UncompressedBuffer = SrcBuffer;
						SrcBuffer += NewChunk.UncompressedSize;
					}

					// Update status variables for tracking how much work is left, what to do next.
					BytesRemainingToKickOff -= NewChunk.UncompressedSize;
					AsyncChunkIndex[FreeIndex] = CurrentChunkIndex++;
					NewChunk.Flags = Flags;
					NumChunksLeftToKickOff--;

					AsyncChunks[FreeIndex].StartBackgroundTask();
				}
				// No chunks were available to use, complete some
				else
				{
					bNeedToWaitForAsyncTask = TRUE;
				}
			}

			// Index of oldest chunk, needed as we need to serialize in order.
			INT OldestAsyncChunkIndex = INDEX_NONE;
			for( INT i=0; i<MaxConcurrentAsyncChunks; i++ )
			{
				check(AsyncChunkIndex[i] == 0 || AsyncChunkIndex[i] >= RetireChunkIndex);
				check(AsyncChunkIndex[i] < RetireChunkIndex + MaxConcurrentAsyncChunks);
				if (AsyncChunkIndex[i] == RetireChunkIndex)
				{
					OldestAsyncChunkIndex = i;
				}
			}
			check(OldestAsyncChunkIndex != INDEX_NONE);  // the retire chunk better be outstanding


			UBOOL ChunkReady;
			if (bNeedToWaitForAsyncTask)
			{
				// This guarantees that the async work has finished, doing it on this thread if it hasn't been started
				AsyncChunks[OldestAsyncChunkIndex].EnsureCompletion();
				ChunkReady = TRUE;
			}
			else
			{
				ChunkReady = AsyncChunks[OldestAsyncChunkIndex].IsDone();
			}
			if (ChunkReady)
			{
				FAsyncCompressionChunk& DoneChunk = AsyncChunks[OldestAsyncChunkIndex].GetTask();
				// Serialize the data via archive.
				Serialize( DoneChunk.CompressedBuffer, DoneChunk.CompressedSize );

				// Update associated chunk.
				INT CompressionChunkIndex = RetireChunkIndex++;
				check(CompressionChunkIndex<TotalChunkCount);
				CompressionChunks[CompressionChunkIndex].CompressedSize		= DoneChunk.CompressedSize;
				CompressionChunks[CompressionChunkIndex].UncompressedSize	= DoneChunk.UncompressedSize;

				// Keep track of total compressed size, stored in first chunk.
				CompressionChunks[0].CompressedSize	+= DoneChunk.CompressedSize;

				// Clean up chunk. Src and dst buffer are not touched as the contain allocations we keep till the end.
				AsyncChunkIndex[OldestAsyncChunkIndex] = 0;
				DoneChunk.CompressedSize	= 0;
				DoneChunk.UncompressedSize = 0;

				// Finalized one :)
				NumChunksLeftToFinalize--;
				bNeedToWaitForAsyncTask = FALSE;
			}
		}

		// Free intermediate buffer storage.
		for( INT i=0; i<MaxConcurrentAsyncChunks; i++ )
		{
			// Free temporary compressed buffer storage.
			appFree( AsyncChunks[i].GetTask().CompressedBuffer );
			AsyncChunks[i].GetTask().CompressedBuffer = NULL;
			// Free temporary uncompressed buffer storage if data was serialized in.
			if( bTreatBufferAsFileReader )
			{
				appFree( AsyncChunks[i].GetTask().UncompressedBuffer );
				AsyncChunks[i].GetTask().UncompressedBuffer = NULL;
			}
		}

#else
		// Set up source pointer amount of data to copy (in bytes)
		BYTE*	Src;
		// allocate memory to read into
		if (bTreatBufferAsFileReader)
		{
			Src = (BYTE*)appMalloc(GSavingCompressionChunkSize);
			check(((FArchive*)V)->IsLoading());
		}
		else
		{
			Src = (BYTE*) V;
		}
		INT		BytesRemaining			= Length;
		// Start at index 1 as first chunk info is summary.
		INT		CurrentChunkIndex		= 1;
		// 2 times the uncompressed size should be more than enough; the compressed data shouldn't be that much larger
		INT		CompressedBufferSize	= 2 * GSavingCompressionChunkSize;
		void*	CompressedBuffer		= appMalloc( CompressedBufferSize );

		while( BytesRemaining > 0 )
		{
			INT BytesToCompress = Min( BytesRemaining, GSavingCompressionChunkSize );
			INT CompressedSize	= CompressedBufferSize;

			// read in the next chunk from the reader
			if (bTreatBufferAsFileReader)
			{
				((FArchive*)V)->Serialize(Src, BytesToCompress);
			}

			verify( appCompressMemory( Flags, CompressedBuffer, CompressedSize, Src, BytesToCompress ) );
			// move to next chunk if not reading from file
			if (!bTreatBufferAsFileReader)
			{
				Src += BytesToCompress;
			}
			Serialize( CompressedBuffer, CompressedSize );
			// Keep track of total compressed size, stored in first chunk.
			CompressionChunks[0].CompressedSize	+= CompressedSize;

			// Update current chunk.
			check(CurrentChunkIndex<TotalChunkCount);
			CompressionChunks[CurrentChunkIndex].CompressedSize		= CompressedSize;
			CompressionChunks[CurrentChunkIndex].UncompressedSize	= BytesToCompress;
			CurrentChunkIndex++;
			
			BytesRemaining -= GSavingCompressionChunkSize;
		}

		// free the buffer we read into
		if (bTreatBufferAsFileReader)
		{
			appFree(Src);
		}

		// Free allocated memory.
		appFree( CompressedBuffer );
#endif

		// Overrwrite chunk infos by seeking to the beginning, serializing the data and then
		// seeking back to the end.
		INT EndPosition = Tell();
		// Seek to the beginning.
		Seek( StartPosition );
		// Serialize chunk infos.
		for( INT ChunkIndex=0; ChunkIndex<TotalChunkCount; ChunkIndex++ )
		{
			*this << CompressionChunks[ChunkIndex];
		}
		// Seek back to end.
		Seek( EndPosition );

		// Free intermediate data.
		delete [] CompressionChunks;
	}
}

VARARG_BODY( void, FArchive::Logf, const TCHAR*, VARARG_NONE )
{
	// We need to use malloc here directly as GMalloc might not be safe, e.g. if called from within GMalloc!
	INT		BufferSize	= 1024;
	TCHAR*	Buffer		= NULL;
	INT		Result		= -1;

	while(Result == -1)
	{
		appSystemFree(Buffer);
		Buffer = (TCHAR*) appSystemMalloc( BufferSize * sizeof(TCHAR) );
		GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );
		BufferSize *= 2;
	};
	Buffer[Result] = 0;

	// Convert to ANSI and serialize as ANSI char.
	for( INT i=0; i<Result; i++ )
	{
		ANSICHAR Char = ToAnsi( Buffer[i] );
		Serialize( &Char, 1 );
	}

	// Write out line terminator.
	for( INT i=0; LINE_TERMINATOR[i]; i++ )
	{
		ANSICHAR Char = LINE_TERMINATOR[i];
		Serialize( &Char, 1 );
	}

	// Free temporary buffers.
	appSystemFree( Buffer );
}

/*-----------------------------------------------------------------------------
	UObject in-memory archivers.
-----------------------------------------------------------------------------*/
/** Constructor */
FReloadObjectArc::FReloadObjectArc()
: FArchive(), Reader(Bytes), Writer(Bytes), RootObject(NULL), InstanceGraph(NULL)
, bAllowTransientObjects(TRUE), bInstanceSubobjectsOnLoad(TRUE)
{
}

/** Destructor */
FReloadObjectArc::~FReloadObjectArc()
{
	if ( InstanceGraph != NULL )
	{
		delete InstanceGraph;
		InstanceGraph = NULL;
	}
}

/**
 * Sets the root object for this memory archive.
 * 
 * @param	NewRoot		the UObject that should be the new root
 */
void FReloadObjectArc::SetRootObject( UObject* NewRoot )
{
	if ( NewRoot != NULL && InstanceGraph == NULL )
	{
		// if we are setting the top-level root object and we don't yet have an instance graph, create one
		InstanceGraph = new FObjectInstancingGraph(NewRoot);
		if ( IsLoading() )
		{
			// if we're reloading data onto objects, pre-initialize the instance graph with the objects instances that were serialized
			for ( INT ObjectIndex = 0; ObjectIndex < CompleteObjects.Num(); ObjectIndex++ )
			{
				UObject* InnerObject = CompleteObjects(ObjectIndex);

				// ignore previously saved objects that aren't contained within the current root object
				if ( InnerObject->IsIn(InstanceGraph->GetDestinationRoot()) )
				{
					UComponent* InnerComponent = Cast<UComponent>(InnerObject);
					if ( InnerComponent != NULL )
					{
						InstanceGraph->AddComponentPair(InnerComponent->GetArchetype<UComponent>(), InnerComponent);
					}
					else
					{
						InstanceGraph->AddObjectPair(InnerObject);
					}
				}
			}
		}
	}

	RootObject = NewRoot;
	if ( RootObject == NULL && InstanceGraph != NULL )
	{
		// if we have cleared the top-level root object and we have an instance graph, delete it
		delete InstanceGraph;
		InstanceGraph = NULL;
	}
}

/**
 * Begin serializing a UObject into the memory archive.  Note that this archive does not use
 * object flags (RF_TagExp|RF_TagImp) to prevent recursion, as do most other archives that perform
 * similar functionality.  This is because this archive is used in operations that modify those
 * object flags for other purposes.
 *
 * @param	Obj		the object to serialize
 */
void FReloadObjectArc::SerializeObject( UObject* Obj )
{
	if ( Obj != NULL )
	{
		TLookupMap<UObject*>& ObjectList = IsLoading()
			? LoadedObjects
			: SavedObjects;

		// only process this top-level object once
		if ( !ObjectList.HasKey(Obj) )
		{
			ObjectList.AddItem(Obj);

			// store the current value of RootObject, in case our caller set it before calling SerializeObject
			UObject* PreviousRoot = RootObject;

			DWORD PreviousHackFlags = GUglyHackFlags;
			GUglyHackFlags |= HACK_IsReloadObjArc;

			// set the root object to this object so that we load complete object data
			// for any objects contained within the top-level object (such as components)
			SetRootObject(Obj);

			// set this to prevent recursion in serialization
			if ( IsLoading() )
			{
				// InitProperties will call CopyCompleteValue for any instanced object properties, so disable object instancing
				// because we probably already have an object that will be copied over that value; for any instanced object properties which
				// did not have a value when we were saving object data, we'll call InstanceSubobjects to instance those.  Also disable component
				// instancing as this object may not have the correct values for its component properties until its serialized, which result in
				// possibly creating lots of unnecessary components that will be thrown away anyway
				InstanceGraph->EnableObjectInstancing(FALSE);
				InstanceGraph->EnableComponentInstancing(FALSE);
				if ( Obj->GetClass() != UPackage::StaticClass() )
				{
					Obj->InitializeProperties(NULL, InstanceGraph);
				}
			}

			if ( Obj->HasAnyFlags(RF_ClassDefaultObject) )
			{
				Obj->GetClass()->SerializeDefaultObject(Obj, *this);
			}
			else
			{
				// save / load the data for this object that is different from its class
				Obj->Serialize(*this);
			}

			if ( IsLoading() )
			{
				if ( InstanceGraph != NULL )
				{
					InstanceGraph->EnableObjectInstancing(TRUE);
					InstanceGraph->EnableComponentInstancing(TRUE);

					if ( bInstanceSubobjectsOnLoad )
					{
						// serializing the stored data for this object should have replaced all of its original instanced object references
						// but there might have been new subobjects added to the object's archetype in the meantime (in the case of updating 
						// an prefab from a prefab instance, for example), so enable subobject instancing and instance those now.
						Obj->InstanceSubobjectTemplates(InstanceGraph);

						// components which were identical to their archetypes weren't stored into this archive's object data, so re-instance those components now
						Obj->InstanceComponentTemplates(InstanceGraph);
					}
				}

				if ( !Obj->HasAnyFlags(RF_ClassDefaultObject) )
				{
					// allow the object to perform any cleanup after re-initialization
					Obj->PostLoad();
				}
			}

			// restore the RootObject - we're done with it.
			SetRootObject(PreviousRoot);
			GUglyHackFlags = PreviousHackFlags;
		}
	}
}

/**
 * Resets the archive so that it can be loaded from again from scratch
 * as if it was never serialized as a Reader
 */
void FReloadObjectArc::Reset()
{
	// empty the list of objects that were loaded, so we can load again
	LoadedObjects.Empty();
	// reset our location in the buffer
	Seek(0);
}


/**
 * I/O function for FName
 */
FArchive& FReloadObjectArc::operator<<( class FName& Name )
{
	NAME_INDEX NameIndex;
	INT NameInstance;
	if ( IsLoading() )
	{
		Reader << NameIndex << NameInstance;

		// recreate the FName using the serialized index and instance number
		Name = FName((EName)NameIndex, NameInstance);
	}
	else if ( IsSaving() )
	{
		NameIndex = Name.GetIndex();
		NameInstance = Name.GetNumber();

		Writer << NameIndex << NameInstance;
	}
	return *this;
}

/**
 * I/O function for UObject references
 */
FArchive& FReloadObjectArc::operator<<( class UObject*& Obj )
{
	if ( IsLoading() )
	{
		PACKAGE_INDEX Index = 0;
		Reader << Index;

		// An index of 0 indicates that the value of the object was NULL
		if ( Index == 0 )
		{
			Obj = NULL;
		}
		else if ( Index < 0 )
		{
			// An index less than zero indicates an object for which we only stored the object pointer
			Obj = ReferencedObjects(-Index-1);
		}
		else if ( Index > 0 )
		{
			// otherwise, the memory archive contains the entire UObject data for this UObject, so load
			// it from the archive
			Obj = CompleteObjects(Index-1);

			// Ensure we don't load it more than once.
			if ( !LoadedObjects.HasKey(Obj) )
			{
				LoadedObjects.AddItem(Obj);

				// find the location for this UObject's data in the memory archive
				INT* ObjectOffset = ObjectMap.Find(Obj);
				checkf(ObjectOffset,TEXT("%s wasn't not found in ObjectMap for %s - double-check that %s (and objects it references) saves the same amount of data it loads if using custom serialization"),
					*Obj->GetFullName(), *GetArchiveName(), *RootObject->GetFullName());

				// move the reader to that position
				Reader.Seek(*ObjectOffset);

				DWORD PreviousHackFlags = GUglyHackFlags;
				GUglyHackFlags |= HACK_IsReloadObjArc;

				// make sure object instancing is disabled before calling InitializeProperties; otherwise new copies of objects will be
				// created for any instanced object properties owned by this object and then immediately overwritten when its serialized
				InstanceGraph->EnableObjectInstancing(FALSE);
				InstanceGraph->EnableComponentInstancing(FALSE);

				// Call InitializeProperties to propagate base change to 'within' objects (like Components).
				Obj->InitializeProperties( NULL, InstanceGraph );

				// read in the data for this UObject
				Obj->Serialize(*this);

				// we should never have RootObject serialized as an object contained by the root object
				checkSlow(Obj != RootObject);

				// serializing the stored data for this object should have replaced all of its original instanced object references
				// but there might have been new subobjects added to the object's archetype in the meantime (in the case of updating 
				// an prefab from a prefab instance, for example), so enable subobject instancing and instance those now.
				InstanceGraph->EnableObjectInstancing(TRUE);
				InstanceGraph->EnableComponentInstancing(TRUE);

				if ( bInstanceSubobjectsOnLoad )
				{
					// we just called InitializeProperties, so any instanced components which were identical to their template weren't serialized into the
					// object data, thus those properties are now pointing to the component contained by the archetype.  So now we need to reinstance those components
					Obj->InstanceSubobjectTemplates(InstanceGraph);

					// components which were identical to their archetypes weren't stored into this archive's object data, so re-instance those components now
					Obj->InstanceComponentTemplates(InstanceGraph);
				}

				// normally we'd never have CDOs in the list of CompleteObjects (CDOs can't be contained within other objects)
				// but in some cases this operator is invoked directly (prefabs)
				if ( !Obj->HasAnyFlags(RF_ClassDefaultObject) )
				{
					Obj->PostLoad();
				}

				GUglyHackFlags = PreviousHackFlags;
			}
		}
	}
	else if ( IsSaving() )
	{
		// Don't save references to transient or deleted objects.
		if ( Obj == NULL || (Obj->HasAnyFlags(RF_Transient) && !bAllowTransientObjects) || Obj->IsPendingKill() )
		{
			// null objects are stored as 0 indexes
			PACKAGE_INDEX Index = 0;
			Writer << Index;
			return *this;
		}

		// See if we have already written this object out.
		PACKAGE_INDEX CompleteIndex = CompleteObjects.HasKey(Obj) ? CompleteObjects.FindRef(Obj) : INDEX_NONE;
		PACKAGE_INDEX ReferencedIndex = ReferencedObjects.HasKey(Obj) ? ReferencedObjects.FindRef(Obj) : INDEX_NONE;

		// The same object can't be in both arrays.
		check( !(CompleteIndex != INDEX_NONE && ReferencedIndex != INDEX_NONE) );

		if(CompleteIndex != INDEX_NONE)
		{
			PACKAGE_INDEX Index = CompleteIndex + 1;
			Writer << Index;
		}
		else if(ReferencedIndex != INDEX_NONE)
		{
			PACKAGE_INDEX Index = -ReferencedIndex - 1;
			Writer << Index;
		}
		// New object - was not already saved.
		// if the object is in the SavedObjects array, it means that the object was serialized into this memory archive as a root object
		// in this case, just serialize the object as a simple reference
		else if ( Obj->IsIn(RootObject) && !SavedObjects.HasKey(Obj) )
		{
			SavedObjects.AddItem(Obj);

			// we need to store the UObject data for this object
			check(ObjectMap.Find(Obj) == NULL);

			// only the location of the UObject in the CompleteObjects
			// array is stored in the memory archive, using PACKAGE_INDEX
			// notation
			PACKAGE_INDEX Index = CompleteObjects.AddItem(Obj) + 1;
			Writer << Index;

			// remember the location of the beginning of this UObject's data
			ObjectMap.Set(Obj,Writer.Tell());

			DWORD PreviousHackFlags = GUglyHackFlags;
			GUglyHackFlags |= HACK_IsReloadObjArc;

			Obj->Serialize(*this);

			GUglyHackFlags = PreviousHackFlags;
		}
		else
		{
#if 0
			// this code is for finding references from actors in PrefabInstances to actors in other levels
			if ( !Obj->HasAnyFlags(RF_Public) && RootObject->GetOutermost() != Obj->GetOutermost() )
			{
				debugf(TEXT("FReloadObjectArc: Encountered reference to external private object %s while serializing references for %s   (prop:%s)"), *Obj->GetFullName(), *RootObject->GetFullName(), GSerializedProperty ? *GSerializedProperty->GetPathName() : TEXT("NULL"));
			}
#endif

			// Referenced objects will be indicated by negative indexes
			PACKAGE_INDEX Index = -ReferencedObjects.AddItem(Obj) - 1;
			Writer << Index;
		}
	}
	return *this;
}

/*----------------------------------------------------------------------------
	FArchetypePropagationArc.
----------------------------------------------------------------------------*/
/** Constructor */
FArchetypePropagationArc::FArchetypePropagationArc()
: FReloadObjectArc()
{
	// enable "write" mode for this archive - objects will serialize their data into the archive
	ActivateWriter();

	// don't wipe out transient objects
	bAllowTransientObjects = TRUE;

	// setting this flag indicates that component references should only be serialized into this archive if there the component has different values that its archetype,
	// and only when the component is being compared to its archetype (as opposed to another component instance, for example)
	SetPortFlags(PPF_DeepCompareInstances);
}

/*----------------------------------------------------------------------------
	FArchiveReplaceArchetype.
----------------------------------------------------------------------------*/
FArchiveReplaceArchetype::FArchiveReplaceArchetype()
: FReloadObjectArc()
{
	// don't wipe out transient objects
	bAllowTransientObjects = TRUE;

	// setting this flag indicates that component references should only be serialized into this archive if there the component has different values that its archetype,
	// and only when the component is being compared to its archetype (as opposed to another component instance, for example)
	SetPortFlags(PPF_DeepCompareInstances);
}

/*----------------------------------------------------------------------------
	FArchiveShowReferences.
----------------------------------------------------------------------------*/
/**
 * Constructor
 *
 * @param	inOutputAr		archive to use for logging results
 * @param	LimitOuter		only consider objects that have this object as its Outer
 * @param	inTarget		object to show referencers to
 * @param	inExclude		list of objects that should be ignored if encountered while serializing Target
 */
FArchiveShowReferences::FArchiveShowReferences( FOutputDevice& inOutputAr, UObject* inOuter, UObject* inSource, TArray<UObject*>& inExclude )
: SourceObject(inSource)
, SourceOuter(inOuter)
, OutputAr(inOutputAr)
, Exclude(inExclude)
, DidRef(FALSE)
{
	ArIsObjectReferenceCollector = TRUE;

	// there are several types of objects we don't want listed, for different reasons.
	// Prevent them from being logged by adding them to our Found list before we start
	// serialization, so that they won't be listed

	// quick sanity check
	check(SourceObject);
	check(SourceObject->IsValid());

	// every object we serialize obviously references our package
	Found.AddUniqueItem(SourceOuter);

	// every object we serialize obviously references its linker
	Found.AddUniqueItem(SourceObject->GetLinker());

	// every object we serialize obviously references its class and its class's parent classes
	for ( UClass* ObjectClass = SourceObject->GetClass(); ObjectClass; ObjectClass = ObjectClass->GetSuperClass() )
	{
		Found.AddUniqueItem( ObjectClass );
	}

	// similarly, if the object is a class, they all obviously reference their parent classes
	if ( SourceObject->IsA(UClass::StaticClass()) )
	{
		for ( UClass* ParentClass = Cast<UClass>(SourceObject)->GetSuperClass(); ParentClass; ParentClass = ParentClass->GetSuperClass() )
		{
			Found.AddUniqueItem( ParentClass );
		}
	}

	// OK, now we're all set to go - let's see what Target is referencing.
	SourceObject->Serialize( *this );
}

FArchive& FArchiveShowReferences::operator<<( UObject*& Obj )
{
	if( Obj && Obj->GetOuter() != SourceOuter )
	{
		INT i;
		for( i=0; i<Exclude.Num(); i++ )
		{
			if( Exclude(i) == Obj->GetOuter() )
			{
				break;
			}
		}

		if( i==Exclude.Num() )
		{
			if( !DidRef )
			{
				OutputAr.Logf( TEXT("   %s references:"), *Obj->GetFullName() );
			}

			OutputAr.Logf( TEXT("      %s"), *Obj->GetFullName() );

			DidRef=1;
		}
	}
	return *this;
}

DWORD GetTypeHash( const FObjectGraphNode* Node )
{
	return PointerHash(Node->NodeObject, PointerHash(Node));
}

// This is from FArchiveTraceRoute -This only creates object graph of all objects 
// This can be used by other classes such as FTraceReferences - trace references of one object
FArchiveObjectGraph::FArchiveObjectGraph(UBOOL IncludeTransients, EObjectFlags	KeepFlags)
:	CurrentReferencer(NULL),
	bIncludeTransients(IncludeTransients), 
	RequiredFlags(KeepFlags)
{
	ArIsObjectReferenceCollector = TRUE;

	// ALL objects reference their outers...it's just log spam here
	//ArIgnoreOuterRef = TRUE;

	TArray<UObject*> RootObjects;

	// allocate enough memory for all objects
	ObjectGraph.Empty(UObject::GetObjectArrayNum());
	RootObjects.Empty(UObject::GetObjectArrayNum());

	// search for objects that have the right flags and add them to the list of objects that we're going to start with
	// all other objects need to be tagged so that we can tell whether they've been serialized or not.
	for( FObjectIterator It; It; ++It )
	{
		UObject* CurrentObject = *It;
		if ( CurrentObject->HasAnyFlags(RequiredFlags) )
		{
			// make sure it isn't tagged
			// ASKRON: WHY do we need this?
			CurrentObject->ClearFlags(RF_TagExp);
			RootObjects.AddItem(CurrentObject);
			ObjectGraph.Set(CurrentObject, new FObjectGraphNode(CurrentObject));
		}
		else
		{
			// ASKRON: WHY do we need this?
			CurrentObject->SetFlags( RF_TagExp );
		}
	}

	// Populate the ObjectGraph - this serializes our root set to map out the relationships between all rooted objects
	GenerateObjectGraph(RootObjects);

	// we won't be adding any additional objects for the arrays and graphs, so free up any memory not being used.
	RootObjects.Shrink();
	ObjectGraph.Shrink();

	// we're done with serialization; clear the tags so that we don't interfere with anything else
	for( FObjectIterator It; It; ++It )
	{
		It->ClearFlags( RF_TagExp );
	}
}

FArchiveObjectGraph::~FArchiveObjectGraph()
{
	for ( TMap<UObject*, FObjectGraphNode*>::TIterator It(ObjectGraph); It; ++It )
	{
		delete It.Value();
		It.Value() = NULL;
	}
}

	/** Handles serialization of UObject references */
FArchive& FArchiveObjectGraph::operator<<( class UObject*& Obj )
{
	if ( Obj != NULL
		&&	(bIncludeTransients || !Obj->HasAnyFlags(RF_Transient)) )
	{
		// grab the object graph node for this object and its referencer, creating them if necessary
		FObjectGraphNode* CurrentObjectNode = ObjectGraph.FindRef(Obj);
		if ( CurrentObjectNode == NULL )
		{
			CurrentObjectNode = ObjectGraph.Set(Obj, new FObjectGraphNode(Obj));
		}
		FObjectGraphNode* ReferencerNode = ObjectGraph.FindRef(CurrentReferencer);
		if ( ReferencerNode == NULL )
		{
			ReferencerNode = ObjectGraph.Set(CurrentReferencer, new FObjectGraphNode(CurrentReferencer));
		}

		if ( Obj != CurrentReferencer )
		{
			FTraceRouteRecord * Record = ReferencerNode->ReferencedObjects.Find(Obj);
			// now record the references between this object and the one referencing it
			if ( !Record )
			{
				ReferencerNode->ReferencedObjects.Set(Obj, FTraceRouteRecord(CurrentObjectNode));
			}
			else
			{
				Record->Add();
			}

			Record = CurrentObjectNode->ReferencerRecords.Find(CurrentReferencer);
			if ( !Record )
			{
				CurrentObjectNode->ReferencerRecords.Set(CurrentReferencer, FTraceRouteRecord(ReferencerNode));
			}
			else
			{
				Record->Add();
			}
		}

		// if this object is still tagged for serialization, add it to the list
		if ( Obj->HasAnyFlags(RF_TagExp) )
		{
			Obj->ClearFlags(RF_TagExp);
			ObjectsToSerialize.AddItem(Obj);
		}
	}
	return *this;
}

void FArchiveObjectGraph::GenerateObjectGraph( TArray<UObject*>& Objects )
{
	const INT LastRootObjectIndex = Objects.Num();

	for ( INT ObjIndex = 0; ObjIndex < Objects.Num(); ObjIndex++ )
	{
		CurrentReferencer = Objects(ObjIndex);
		CurrentReferencer->ClearFlags(RF_TagExp);

		// Serialize this object
		if ( CurrentReferencer->HasAnyFlags(RF_ClassDefaultObject) )
		{
			CurrentReferencer->GetClass()->SerializeDefaultObject(CurrentReferencer, *this);
		}
		else
		{
			CurrentReferencer->Serialize( *this );
		}

		// ObjectsToSerialize will contain only those objects which were encountered while serializing CurrentReferencer
		// that weren't already in the list of objects to be serialized.
		if ( ObjectsToSerialize.Num() > 0 )
		{
			// add to objects, so that we make sure ObjectToSerialize are serialized
			Objects += ObjectsToSerialize;
			ObjectsToSerialize.Empty();
		}
	}

	Objects.Remove(LastRootObjectIndex, Objects.Num() - LastRootObjectIndex);
}

void FArchiveObjectGraph::ClearSearchFlags()
{
	for ( TMap<UObject*, FObjectGraphNode*>::TIterator Iter(ObjectGraph); Iter; ++Iter )
	{
		FObjectGraphNode * GraphNode = Iter.Value();
		if ( GraphNode )
		{
			GraphNode->Visited = 0;
			GraphNode->ReferenceDepth = MAXINT;
			GraphNode->ReferencerProperties.Empty();
		}
	}
}

// This traces referenced/referencer of an object using FArchiveObjectGraph 
FTraceReferences::FTraceReferences( UBOOL bIncludeTransients, EObjectFlags KeepFlags )
: ArchiveObjectGraph ( bIncludeTransients, KeepFlags )
{}

FString FTraceReferences::GetReferencerString( UObject* Object, INT Depth)
{
	TArray<FObjectGraphNode*> Referencers;
	FString OutString;

	if ( GetReferencer( Object, Referencers, FALSE, Depth ) > 0 )
	{
		INT CurrentDepth = 0, NumPrinted=0;

		do 
		{
			NumPrinted = 0;
			for (INT RefId=0; RefId<Referencers.Num(); ++RefId)
			{
				FObjectGraphNode * Node = Referencers(RefId);
				if (CurrentDepth == Node->ReferenceDepth)
				{
					++NumPrinted;
					OutString += FString::Printf(TEXT("(%d) %s%s"), CurrentDepth, *Node->NodeObject->GetPathName(), LINE_TERMINATOR);

					for (INT Id=0; Id<Node->ReferencerProperties.Num(); ++Id)
					{
						OutString += FString::Printf(TEXT("\t(%d) %s%s"), Id+1, *Node->ReferencerProperties(Id)->GetName(), LINE_TERMINATOR);
					}
				}
			}

			++CurrentDepth;
		} while( NumPrinted > 0 || CurrentDepth == 0);
	}

	return OutString;
}

FString FTraceReferences::GetReferencedString( UObject* Object, INT Depth)
{
	TArray<FObjectGraphNode*> Referenced;
	FString OutString;

	if ( GetReferenced( Object, Referenced, FALSE, Depth ) > 0 )
	{
		INT CurrentDepth = 0, NumPrinted=0;

		do 
		{
			NumPrinted = 0;
			for (INT RefId=0; RefId<Referenced.Num(); ++RefId)
			{
				FObjectGraphNode * Node = Referenced(RefId);
				if (CurrentDepth == Node->ReferenceDepth)
				{
					++NumPrinted;
					OutString += FString::Printf(TEXT("(%d) %s%s"), CurrentDepth, *Node->NodeObject->GetPathName(), LINE_TERMINATOR);

					for (INT Id=0; Id<Node->ReferencerProperties.Num(); ++Id)
					{
						OutString += FString::Printf(TEXT("\t(%d) %s%s"), Id+1, *Node->ReferencerProperties(Id)->GetName(), LINE_TERMINATOR);
					}
				}
			}

			++CurrentDepth;
		} while( NumPrinted > 0 || CurrentDepth == 0);
	}

	return OutString;
}

INT		FTraceReferences::GetReferencer( UObject * Object, TArray<FObjectGraphNode*> &Referencer, UBOOL bExcludeSelf, INT Depth)
{
	ArchiveObjectGraph.ClearSearchFlags();
	Referencer.Empty();

	GetReferencerInternal( Object, Referencer, 0, Depth );

	if ( bExcludeSelf )
	{
		// remove head
		Referencer.Remove(0);
	}

	return Referencer.Num();
}

void	FTraceReferences::GetReferencerInternal( UObject * CurrentObject, TArray<FObjectGraphNode*> &OutReferencer, INT CurrentDepth, INT TargetDepth )
{
	if (TargetDepth >= CurrentDepth)
	{
		FObjectGraphNode * CurrentTarget = ArchiveObjectGraph.ObjectGraph.FindRef(CurrentObject);

		if ( CurrentTarget && !CurrentTarget->Visited && CurrentTarget->ReferencerRecords.Num() > 0 )
		{
			// set Depth and add to outerReferencer
			CurrentTarget->Visited = TRUE;
			CurrentTarget->ReferenceDepth = CurrentDepth;
			OutReferencer.AddItem(CurrentTarget);

			// go through all referncers and call referencerinternal
			for (TMap<UObject*, FTraceRouteRecord>::TIterator Iter(CurrentTarget->ReferencerRecords); Iter; ++Iter )
			{
				UBOOL HasValidProperty=FALSE;
				FTraceRouteRecord& Referencer = Iter.Value();

				for ( INT Idx=0; Idx<Referencer.ReferencerProperties.Num(); ++Idx )
				{
					if (Referencer.ReferencerProperties(Idx))
					{
						CurrentTarget->ReferencerProperties.AddItem(Referencer.ReferencerProperties(Idx));
						HasValidProperty = TRUE;
					}
				}

				if ( HasValidProperty )
				{
					GetReferencerInternal( Referencer.GraphNode->NodeObject, OutReferencer, CurrentDepth + 1, TargetDepth );
				}
			}
		}
	}
}

INT		FTraceReferences::GetReferenced( UObject * Object, TArray<FObjectGraphNode*> &Referenced, UBOOL bExcludeSelf, INT Depth)
{
	ArchiveObjectGraph.ClearSearchFlags();
	Referenced.Empty();

	GetReferencedInternal( Object, Referenced, 0, Depth );

	if ( bExcludeSelf )
	{
		// remove head
		Referenced.Remove(0);
	}

	return Referenced.Num();
}

void FTraceReferences::GetReferencedInternal( UObject * CurrentObject, TArray<FObjectGraphNode*> &OutReferenced, INT CurrentDepth, INT TargetDepth )
{
	if (TargetDepth >= CurrentDepth)
	{
		FObjectGraphNode * CurrentTarget = ArchiveObjectGraph.ObjectGraph.FindRef(CurrentObject);

		if ( CurrentTarget && !CurrentTarget->Visited && CurrentTarget->ReferencedObjects.Num() > 0 )
		{
			// set Depth and add to outerReferencer
			CurrentTarget->Visited = TRUE;
			CurrentTarget->ReferenceDepth = CurrentDepth;
			OutReferenced.AddItem(CurrentTarget);

			// go through all refernced and call referencedinternal
			for (TMap<UObject*, FTraceRouteRecord>::TIterator Iter(CurrentTarget->ReferencedObjects); Iter; ++Iter )
			{
				UBOOL HasValidProperty=FALSE;
				FTraceRouteRecord& Referenced = Iter.Value();

				for ( INT Idx=0; Idx<Referenced.ReferencerProperties.Num(); ++Idx )
				{
					if (Referenced.ReferencerProperties(Idx))
					{
						// For referenced, it makes sense if we add property to the referenced, instead of target
						Referenced.GraphNode->ReferencerProperties.AddItem(Referenced.ReferencerProperties(Idx));
						HasValidProperty = TRUE;
						break;
					}
				}

				if ( HasValidProperty )
				{
					// it has property set up, call getreferenced again
					GetReferencedInternal( Referenced.GraphNode->NodeObject, OutReferenced, CurrentDepth + 1, TargetDepth );
				}
			}
		}
	}
}
/*----------------------------------------------------------------------------
	FArchiveTraceRoute
----------------------------------------------------------------------------*/

TMap<UObject*,UProperty*> FArchiveTraceRoute::FindShortestRootPath( UObject* Obj, UBOOL bIncludeTransients, EObjectFlags KeepFlags )
{
	// Take snapshot of object flags that will be restored once marker goes out of scope.
	FScopedObjectFlagMarker ObjectFlagMarker;

	TMap<UObject*,FTraceRouteRecord> Routes;
	FArchiveTraceRoute Rt( Obj, Routes, bIncludeTransients, KeepFlags );

	TMap<UObject*,UProperty*> Result;

	// No routes are reported if the object wasn't rooted.
	if ( Routes.Num() > 0 || Obj->HasAnyFlags(KeepFlags) )
	{
		TArray<FTraceRouteRecord> Records;
		Routes.GenerateValueArray(Records);

		// the target object is NOT included in the result, so add it first.  Then iterate over the route
		// backwards, following the trail from the target object to the root object.
		Result.Set(Obj, NULL);
		for ( INT RecordIndex = Records.Num() - 1; RecordIndex >= 0; RecordIndex-- )
		{
			FTraceRouteRecord& Record = Records(RecordIndex);
			// To keep same behavior as previous, it will set last one for now until Ron comes back
			for ( INT ReferenceIndex = 0; ReferenceIndex<Record.ReferencerProperties.Num(); ++ReferenceIndex )
			{
				if (Record.ReferencerProperties(ReferenceIndex))
				{
					Result.Set( Record.GraphNode->NodeObject, Record.ReferencerProperties(ReferenceIndex));
					break;
				}
			}
		}
	}

#if 0
	// eventually we'll merge this archive with the FArchiveFindCulprit, since we have all the same information already
	// but before we can do so, we'll need to change the TraceRouteRecord's ReferencerProperty to a TArray so that we
	// can report when an object has multiple references to another object
	FObjectGraphNode* ObjNode = Rt.ObjectGraph.FindRef(Obj);
	if ( ObjNode != NULL && ObjNode->ReferencerRecords.Num() > 0 )
	{
		debugf( TEXT("Referencers of %s (from FArchiveTraceRoute):"), *Obj->GetFullName() );
		INT RefIndex=0;
		for ( TMap<UObject*,FTraceRouteRecord>::TIterator It(ObjNode->ReferencerRecords); It; ++It )
		{
			UObject* Object = It.Key();
			FTraceRouteRecord& Record = It.Value();
			debugf(TEXT("      %i) %s  (%s)"), RefIndex++, *Object->GetFullName(), *Record.ReferencerProperty->GetFullName());
		}
	}
#endif

	return Result;
}

/**
 * Retuns path to root created by e.g. FindShortestRootPath via a string.
 *
 * @param TargetObject	object marking one end of the route
 * @param Route			route to print to log.
 * @param String of root path
 */
FString FArchiveTraceRoute::PrintRootPath( const TMap<UObject*,UProperty*>& Route, const UObject* TargetObject )
{
	FString RouteString;
	for( TMap<UObject*,UProperty*>::TConstIterator MapIt(Route); MapIt; ++MapIt )
	{
		UObject*	Object		= MapIt.Key();
		UProperty*	Property	= MapIt.Value();

		FString	ObjectReachability;
		
		if( Object == TargetObject )
		{
			ObjectReachability = TEXT(" [target]");
		}
		
		if( Object->HasAnyFlags(RF_RootSet) )
		{
			ObjectReachability += TEXT(" (root)");
		}
		
		if( Object->HasAnyFlags(RF_Native) )
		{
			ObjectReachability += TEXT(" (native)");
		}
		
		if( Object->HasAnyFlags(RF_Standalone) )
		{
			ObjectReachability += TEXT(" (standalone)");
		}
		
		if( ObjectReachability == TEXT("") )
		{
			ObjectReachability = TEXT(" ");
		}
			
		FString ReferenceSource;
		if( Property != NULL )
		{
			ReferenceSource = FString::Printf(TEXT("%s (%s)"), *ObjectReachability, *Property->GetFullName());
		}
		else
		{
			ReferenceSource = ObjectReachability;
		}

		RouteString += FString::Printf(TEXT("   %s%s%s"), *Object->GetFullName(), *ReferenceSource, LINE_TERMINATOR );
	}

	if( !Route.Num() )
	{
		RouteString = TEXT("   (Object is not currently rooted)\r\n");
	}
	return RouteString;
}

FArchiveTraceRoute::FArchiveTraceRoute( UObject* TargetObject, TMap<UObject*,FTraceRouteRecord>& InRoutes, UBOOL bShouldIncludeTransients, EObjectFlags KeepFlags )
:	CurrentReferencer(NULL)
,	Depth(0)
,	bIncludeTransients(bShouldIncludeTransients)
,	RequiredFlags((KeepFlags|RF_RootSet) & ~RF_TagExp)
{
	// this object is part of the root set; don't have to do anything
	if ( TargetObject == NULL || TargetObject->HasAnyFlags(KeepFlags) )
	{
		return;
	}

	ArIsObjectReferenceCollector = TRUE;
	
	TSparseArray<UObject*> RootObjects;

	// allocate enough memory for all objects
	ObjectGraph.Empty(UObject::GetObjectArrayNum());
	RootObjects.Empty(UObject::GetObjectArrayNum() / 2);

	// search for objects that have the right flags and add them to the list of objects that we're going to start with
	// all other objects need to be tagged so that we can tell whether they've been serialized or not.
	for( FObjectIterator It; It; ++It )
	{
		UObject* CurrentObject = *It;
		if ( CurrentObject->HasAnyFlags(RequiredFlags) )
		{
			// make sure it isn't tagged
			CurrentObject->ClearFlags(RF_TagExp);
			RootObjects.AddItem(CurrentObject);
			ObjectGraph.Set(CurrentObject, new FObjectGraphNode(CurrentObject));
		}
		else
		{
			CurrentObject->SetFlags( RF_TagExp );
		}
	}

	// Populate the ObjectGraph - this serializes our root set to map out the relationships between all rooted objects
	GenerateObjectGraph(RootObjects);

	// we won't be adding any additional objects for the arrays and graphs, so free up any memory not being used.
	RootObjects.Shrink();
	ObjectGraph.Shrink();

	// we're done with serialization; clear the tags so that we don't interfere with anything else
	for( FObjectIterator It; It; ++It )
	{
		It->ClearFlags( RF_TagExp );
	}

	// Now we calculate the shortest path from the target object to a rooted object; if the target object isn't
	// in the object graph, it means it isn't rooted.
	FObjectGraphNode* ObjectNode = ObjectGraph.FindRef(TargetObject);
	if ( ObjectNode )
	{
		ObjectNode->ReferenceDepth = 0;

		// This method sets up the ReferenceDepth values for all relevant nodes
		CalculateReferenceDepthsForNode(ObjectNode);

		INT LowestDepth = MAXINT;
		FRouteLink ClosestLink;

		// Next, we find the root object that has the lowest Depth value
		//@fixme - we might have multiple root objects which have the same depth value
//	 	TArray<TDoubleLinkedList<FObjectGraphNode*> > ShortestRoutes;
		TDoubleLinkedList<FObjectGraphNode*> ShortestRoute;
		for ( INT RootObjectIndex = 0; RootObjectIndex < RootObjects.Num(); RootObjectIndex++ )
		{
			FObjectGraphNode* RootObjectNode = ObjectGraph.FindRef(RootObjects(RootObjectIndex));
			FindClosestLink(RootObjectNode, LowestDepth, ClosestLink);
		}

		// At this point, we should know which root object has the shortest depth from the target object.  Push that
		// link into our linked list and recurse into the links to navigate our way through the reference chain to the
		// target object.
		ShortestRoute.AddHead(ClosestLink.LinkParent);
		if ( ClosestLink.LinkChild != NULL )
		{
			ShortestRoute.AddTail(ClosestLink.LinkChild);
			while ( FindClosestLink(ClosestLink.LinkChild, LowestDepth, ClosestLink)/* || LowestDepth > 0*/ )
			{
				// at this point, LinkChild will be different than it was when we last evaluated the conditional
				ShortestRoute.AddTail(ClosestLink.LinkChild);
			}
		}

		// since we know that the target object is rooted, there should be at least one route to a root object;
		// therefore LowestDepth should always be zero or there is a bug somewhere...
		check(LowestDepth==0);

		// Finally, fill in the output value - it will start with the rooted object and follow the chain of object references
		// to the target object.  However, the node for the target object itself is NOT included in this result.
		for (TDoubleLinkedList<FObjectGraphNode*>::TIterator It(ShortestRoute.GetHead()); It; ++It )
		{
			FObjectGraphNode* CurrentNode = *It;
			InRoutes.Set(CurrentNode->NodeObject, FTraceRouteRecord(CurrentNode, CurrentNode->ReferencerProperties));
		}
	}
}

/**
 * Searches through the objects referenced by CurrentNode for a record with a Depth lower than LowestDepth.
 *
 * @param	CurrentNode		the node containing the list of referenced objects that will be searched.
 * @param	LowestDepth		the current number of links we are from the target object.
 * @param	ClosestLink		if a trace route record is found with a lower depth value than LowestDepth, the link is saved to this value.
 *
 * @return	TRUE if a closer link was discovered; FALSE if no links were closer than lowest depth, or if we've reached the target object.
 */
UBOOL FArchiveTraceRoute::FindClosestLink( FObjectGraphNode* CurrentNode, INT& LowestDepth, FRouteLink& ClosestLink )
{
	UBOOL bResult = FALSE;

	if ( CurrentNode != NULL )
	{
		for ( TMap<UObject*, FTraceRouteRecord>::TIterator RefIt(CurrentNode->ReferencedObjects); RefIt; ++RefIt )
		{
			FTraceRouteRecord& Record = RefIt.Value();

			// a ReferenceDepth of MAXINT means that this object was not part of the target object's reference graph
			if ( Record.GraphNode->ReferenceDepth < MAXINT )
			{
				if ( Record.GraphNode->ReferenceDepth == 0 )
				{
					// found the target
					if ( CurrentNode->ReferenceDepth < LowestDepth )
					{
						// the target object is referenced directly by a rooted object
						ClosestLink = FRouteLink(CurrentNode, NULL);
					}
					LowestDepth = CurrentNode->ReferenceDepth - 1;
					bResult = FALSE;
					break;
				}
				else if ( Record.GraphNode->ReferenceDepth < LowestDepth )
				{
					LowestDepth = Record.GraphNode->ReferenceDepth;
					ClosestLink = FRouteLink(CurrentNode, Record.GraphNode);
					bResult = TRUE;
				}
				else if ( Record.GraphNode->ReferenceDepth == LowestDepth )
				{
					// once we've changed this to an array, push this link on
				}
			}
		}
	}

	return bResult;
}

/**
 * Destructor.  Deletes all FObjectGraphNodes created by this archive.
 */
FArchiveTraceRoute::~FArchiveTraceRoute()
{
	for ( TMap<UObject*, FObjectGraphNode*>::TIterator It(ObjectGraph); It; ++It )
	{
		delete It.Value();
		It.Value() = NULL;
	}
}

FArchive& FArchiveTraceRoute::operator<<( class UObject*& Obj )
{
	if ( Obj != NULL
	&&	(bIncludeTransients || !Obj->HasAnyFlags(RF_Transient)) )
	{
		// grab the object graph node for this object and its referencer, creating them if necessary
		FObjectGraphNode* CurrentObjectNode = ObjectGraph.FindRef(Obj);
		if ( CurrentObjectNode == NULL )
		{
			CurrentObjectNode = ObjectGraph.Set(Obj, new FObjectGraphNode(Obj));
		}
		FObjectGraphNode* ReferencerNode = ObjectGraph.FindRef(CurrentReferencer);
		if ( ReferencerNode == NULL )
		{
			ReferencerNode = ObjectGraph.Set(CurrentReferencer, new FObjectGraphNode(CurrentReferencer));
		}

		if ( Obj != CurrentReferencer )
		{
			FTraceRouteRecord * Record = ReferencerNode->ReferencedObjects.Find(Obj);
			// now record the references between this object and the one referencing it
			if ( !Record )
			{
				ReferencerNode->ReferencedObjects.Set(Obj, FTraceRouteRecord(CurrentObjectNode));
			}
			else
			{
				Record->Add();
			}

			Record = CurrentObjectNode->ReferencerRecords.Find(CurrentReferencer);
			if ( !Record )
			{
				CurrentObjectNode->ReferencerRecords.Set(CurrentReferencer, FTraceRouteRecord(ReferencerNode));
			}
			else
			{
				Record->Add();
			}
		}

		// if this object is still tagged for serialization, add it to the list
		if ( Obj->HasAnyFlags(RF_TagExp) )
		{
			Obj->ClearFlags(RF_TagExp);
			ObjectsToSerialize.AddItem(Obj);
		}
	}
	return *this;
}

/**
 * Serializes the objects in the specified set; any objects encountered during serialization
 * of an object are added to the object set and processed until no new objects are added.
 *
 * @param	Objects		the original set of objects to serialize; the original set will be preserved.
 */
void FArchiveTraceRoute::GenerateObjectGraph( TSparseArray<UObject*>& Objects )
{
	const INT LastRootObjectIndex = Objects.Num();

	for ( INT ObjIndex = 0; ObjIndex < Objects.Num(); ObjIndex++ )
	{
		CurrentReferencer = Objects(ObjIndex);
		CurrentReferencer->ClearFlags(RF_TagExp);

		// Serialize this object
		if ( CurrentReferencer->HasAnyFlags(RF_ClassDefaultObject) )
		{
			CurrentReferencer->GetClass()->SerializeDefaultObject(CurrentReferencer, *this);
		}
		else
		{
			CurrentReferencer->Serialize( *this );
		}
		
		// ObjectsToSerialize will contain only those objects which were encountered while serializing CurrentReferencer
		// that weren't already in the list of objects to be serialized.
		if ( ObjectsToSerialize.Num() > 0 )
		{
			Objects += ObjectsToSerialize;
			ObjectsToSerialize.Empty();
		}
	}

	Objects.Remove(LastRootObjectIndex, Objects.Num() - LastRootObjectIndex);
}

/**
 * Recursively iterates over the referencing objects for the specified node, marking each with
 * the current Depth value.  Stops once it reaches a route root.
 *
 * @param	ObjectNode	the node to evaluate.
 */
void FArchiveTraceRoute::CalculateReferenceDepthsForNode( FObjectGraphNode* ObjectNode )
{
	check(ObjectNode);

	Depth++;

	TSparseArray<FObjectGraphNode*> RecurseRecords;
	// for each referencer, set the current depth.  Do this before recursing into this object's
	// referencers to minimize the number of times we have to revisit nodes
	for ( TMap<UObject*, FTraceRouteRecord>::TIterator It(ObjectNode->ReferencerRecords); It; ++It )
	{
		FTraceRouteRecord& Record = It.Value();

		checkSlow(Record.GraphNode);
		if ( Record.GraphNode->ReferenceDepth > Depth )
		{
			Record.GraphNode->ReferenceDepth = Depth;
			Record.GraphNode->ReferencerProperties.Append(Record.ReferencerProperties);
			RecurseRecords.AddItem(Record.GraphNode);
		}
	}

	for ( TSparseArray<FObjectGraphNode*>::TIterator It(RecurseRecords); It; ++It )
	{
		FObjectGraphNode* CurrentNode = *It;
		It.RemoveCurrent();

		// this record may have been encountered by processing another node; if that resulted in a shorter
		// route for this record, just skip it.
		if ( CurrentNode->ReferenceDepth == Depth )
		{
			// if the object from this node has one of the required flags, don't process this object's referencers
			// as it's considered a "root" for the route
			if ( !CurrentNode->NodeObject->HasAnyFlags(RequiredFlags) )
			{
				CalculateReferenceDepthsForNode(CurrentNode);
			}
		}
	}

	Depth--;
}

/*----------------------------------------------------------------------------
	FFindReferencersArchive.
----------------------------------------------------------------------------*/
/**
 * Constructor
 *
 * @param	PotentialReferencer		the object to serialize which may contain references to our target objects
 * @param	InTargetObjects			array of objects to search for references to
 */
FFindReferencersArchive::FFindReferencersArchive( UObject* PotentialReferencer, TArray<UObject*> InTargetObjects )
{
	// use the optimized RefLink to skip over properties which don't contain object references
	ArIsObjectReferenceCollector = TRUE;

	// ALL objects reference their outers...it's just log spam here
	ArIgnoreOuterRef = TRUE;

	// initialize the map
	for ( INT ObjIndex = 0; ObjIndex < InTargetObjects.Num(); ObjIndex++ )
	{
		UObject* TargetObject = InTargetObjects(ObjIndex);
		if ( TargetObject != NULL && TargetObject != PotentialReferencer )
		{
			TargetObjects.Set(TargetObject, 0);
		}
	}

	// now start the search
	PotentialReferencer->Serialize( *this );
}

/**
 * Retrieves the number of references from PotentialReferencer to the object specified.
 *
 * @param	TargetObject	the object to might be referenced
 * @param	out_ReferencingProperties
 *							receives the list of properties which were holding references to TargetObject
 *
 * @return	the number of references to TargetObject which were encountered when PotentialReferencer
 *			was serialized.
 */
INT FFindReferencersArchive::GetReferenceCount( UObject* TargetObject, TArray<UProperty*>* out_ReferencingProperties/*=NULL*/ ) const
{
	INT Result = 0;
	if ( TargetObject != NULL )
	{
		const INT* pCount = TargetObjects.Find(TargetObject);
		if ( pCount != NULL && (*pCount) > 0 )
		{
			Result = *pCount;
			if ( out_ReferencingProperties != NULL )
			{
				TArray<UProperty*> PropertiesReferencingObj;
				ReferenceMap.MultiFind(TargetObject, PropertiesReferencingObj);

				out_ReferencingProperties->Empty(PropertiesReferencingObj.Num());
				for ( INT PropIndex = PropertiesReferencingObj.Num() - 1; PropIndex >= 0; PropIndex-- )
				{
					out_ReferencingProperties->AddItem(PropertiesReferencingObj(PropIndex));
				}
			}
		}
	}

	return Result;
}

/**
 * Retrieves the number of references from PotentialReferencer list of TargetObjects
 *
 * @param	out_ReferenceCounts		receives the number of references to each of the TargetObjects
 *
 * @return	the number of objects which were referenced by PotentialReferencer.
 */
INT FFindReferencersArchive::GetReferenceCounts( TMap<UObject*, INT>& out_ReferenceCounts ) const
{
	out_ReferenceCounts.Empty();
	for ( TMap<UObject*,INT>::TConstIterator It(TargetObjects); It; ++It )
	{
		if ( It.Value() > 0 )
		{
			out_ReferenceCounts.Set(It.Key(), It.Value());
		}
	}

	return out_ReferenceCounts.Num();
}

/**
 * Retrieves the number of references from PotentialReferencer list of TargetObjects
 *
 * @param	out_ReferenceCounts			receives the number of references to each of the TargetObjects
 * @param	out_ReferencingProperties	receives the map of properties holding references to each referenced object.
 *
 * @return	the number of objects which were referenced by PotentialReferencer.
 */
INT FFindReferencersArchive::GetReferenceCounts( TMap<UObject*, INT>& out_ReferenceCounts, TMultiMap<UObject*,UProperty*>& out_ReferencingProperties ) const
{
	GetReferenceCounts(out_ReferenceCounts);
	if ( out_ReferenceCounts.Num() > 0 )
	{
		out_ReferencingProperties.Empty();
		for ( TMap<UObject*,INT>::TIterator It(out_ReferenceCounts); It; ++It )
		{
			UObject* Object = It.Key();

			TArray<UProperty*> PropertiesReferencingObj;
			ReferenceMap.MultiFind(Object, PropertiesReferencingObj);

			for ( INT PropIndex = PropertiesReferencingObj.Num() - 1; PropIndex >= 0; PropIndex-- )
			{
				out_ReferencingProperties.Add(Object, PropertiesReferencingObj(PropIndex));
			}
		}
	}

	return out_ReferenceCounts.Num();
}

/**
 * Serializer - if Obj is one of the objects we're looking for, increments the reference count for that object
 */
FArchive& FFindReferencersArchive::operator<<( UObject*& Obj )
{
	if ( Obj != NULL )
	{
		INT* pReferenceCount = TargetObjects.Find(Obj);
		if ( pReferenceCount != NULL )
		{
			// if this object was serialized via a UProperty, add it to the list
			if ( GSerializedProperty != NULL )
			{
				ReferenceMap.AddUnique(Obj, GSerializedProperty);
			}

			// now increment the reference count for this target object
			(*pReferenceCount)++;
		}
	}

	return *this;
}

/*----------------------------------------------------------------------------
	FArchiveFindCulprit.
----------------------------------------------------------------------------*/
/**
 * Constructor
 *
 * @param	InFind				the object that we'll be searching for references to
 * @param	Src					the object to serialize which may contain a reference to InFind
 * @param	InPretendSaving		if TRUE, marks the archive as saving and persistent, so that a different serialization codepath is followed
 */
FArchiveFindCulprit::FArchiveFindCulprit( UObject* InFind, UObject* Src, UBOOL InPretendSaving )
: Find(InFind), Count(0), PretendSaving(InPretendSaving)
{
	// use the optimized RefLink to skip over properties which don't contain object references
	ArIsObjectReferenceCollector = TRUE;

	// ALL objects reference their outers...it's just log spam here
	ArIgnoreOuterRef = TRUE;

	if( PretendSaving )
	{
		ArIsSaving		= TRUE;
		ArIsPersistent	= TRUE;
	}

	GSerializedProperty = NULL;
	Src->Serialize( *this );
}

FArchive& FArchiveFindCulprit::operator<<( UObject*& Obj )
{
	if( Obj==Find )
	{
		if ( GSerializedProperty != NULL )
		{
			Referencers.AddUniqueItem(GSerializedProperty);
		}
		Count++;
	}

	if( PretendSaving && Obj && !Obj->IsPendingKill() )
	{
		if( (!Obj->HasAnyFlags(RF_Transient) || Obj->HasAnyFlags(RF_Public)) && !Obj->HasAnyFlags(RF_TagExp) )
		{
			if ( Obj->HasAnyFlags(RF_Native|RF_Standalone|RF_RootSet) )
			{
				// serialize the object's Outer if this object could potentially be rooting the object we're attempting to find references to
				// otherwise, it's just spam
				*this << Obj->Outer;
			}

			// serialize the object's ObjectArchetype
			*this << Obj->ObjectArchetype;
		}
	}
	return *this;
}

/*----------------------------------------------------------------------------
	FDuplicateDataReader.
----------------------------------------------------------------------------*/
/**
 * Constructor
 *
 * @param	InDuplicatedObjects		map of original object to copy of that object
 * @param	InObjectData			object data to read from
 */
FDuplicateDataReader::FDuplicateDataReader(const TMap<UObject*,FDuplicatedObjectInfo*>& InDuplicatedObjects,const TArray<BYTE>& InObjectData)
: DuplicatedObjects(InDuplicatedObjects)
, ObjectData(InObjectData)
, Offset(0)
{
	ArIsLoading			= TRUE;
	ArIsPersistent		= TRUE;
	ArPortFlags |= PPF_Duplicate;
}

FArchive& FDuplicateDataReader::operator<<( UObject*& Object )
{
	UObject*	SourceObject = Object;
	Serialize(&SourceObject,sizeof(UObject*));
	const FDuplicatedObjectInfo*	ObjectInfo = DuplicatedObjects.FindRef(SourceObject);
	if(ObjectInfo)
	{
		Object = ObjectInfo->DupObject;
	}
	else
	{
		Object = SourceObject;
	}

	return *this;
}

/*----------------------------------------------------------------------------
	FDuplicateDataWriter.
----------------------------------------------------------------------------*/
/**
 * Constructor
 *
 * @param	InDuplicatedObjects		will contain the original object -> copy mappings
 * @param	InObjectData			will store the serialized data
 * @param	SourceObject			the object to copy
 * @param	DestObject				the object to copy to
 * @param	InFlagMask				the flags that should be copied when the object is duplicated
 * @param	InApplyFlags			the flags that should always be set on the duplicated objects (regardless of whether they're set on the source)
 * @param	InInstanceGraph			the instancing graph to use when creating the duplicate objects.
 */
FDuplicateDataWriter::FDuplicateDataWriter(TMap<UObject*,FDuplicatedObjectInfo*>& InDuplicatedObjects,TArray<BYTE>& InObjectData,UObject* SourceObject,
										   UObject* DestObject,EObjectFlags InFlagMask, EObjectFlags InApplyFlags, FObjectInstancingGraph* InInstanceGraph)
: DuplicatedObjects(InDuplicatedObjects)
, ObjectData(InObjectData)
, Offset(0)
, FlagMask(InFlagMask)
, ApplyFlags(InApplyFlags)
, InstanceGraph(InInstanceGraph)
{
	ArIsSaving			= TRUE;
	ArIsPersistent		= TRUE;
	ArAllowLazyLoading	= FALSE;
	ArPortFlags |= PPF_Duplicate;

	AddDuplicate(SourceObject,DestObject);
}

/**
 * I/O function
 */
FArchive& FDuplicateDataWriter::operator<<(UObject*& Object)
{
	GetDuplicatedObject(Object);

	// store the pointer to this object
	Serialize(&Object,sizeof(UObject*));
	return *this;
}

/**
 * Places a new duplicate in the DuplicatedObjects map as well as the UnserializedObjects list
 *
 * @param	SourceObject		the original version of the object
 * @param	DuplicateObject		the copy of the object
 *
 * @return	a pointer to the copy of the object
 */
UObject* FDuplicateDataWriter::AddDuplicate(UObject* SourceObject,UObject* DupObject)
{
	// Check for an existing duplicate of the object; if found, use that one instead of creating a new one.
	FDuplicatedObjectInfo* Info = DuplicatedObjects.FindRef(SourceObject);
	if ( Info == NULL )
	{
		Info = DuplicatedObjects.Set(SourceObject,new FDuplicatedObjectInfo());
	}
	Info->DupObject = DupObject;

	TMap<FName,UComponent*>	ComponentInstanceMap;
	SourceObject->CollectComponents(ComponentInstanceMap, FALSE);

	for(TMap<FName,UComponent*>::TIterator It(ComponentInstanceMap);It;++It)
	{
		UComponent*& Component = It.Value();

		UObject* DuplicatedComponent = GetDuplicatedObject(Component);
		Info->ComponentInstanceMap.Set( Component, Cast<UComponent>(DuplicatedComponent) );
	}

	UnserializedObjects.AddItem(SourceObject);
	return DupObject;
}

/**
 * Returns a pointer to the duplicate of a given object, creating the duplicate object if necessary.
 *
 * @param	Object	the object to find a duplicate for
 *
 * @return	a pointer to the duplicate of the specified object
 */
UObject* FDuplicateDataWriter::GetDuplicatedObject( UObject* Object )
{
	UObject* Result = NULL;
	if( Object != NULL )
	{
		// Check for an existing duplicate of the object.
		FDuplicatedObjectInfo*	DupObjectInfo = DuplicatedObjects.FindRef(Object);
		if( DupObjectInfo != NULL )
		{
			Result = DupObjectInfo->DupObject;
		}
		else
		{
			// Check to see if the object's outer is being duplicated.
			UObject*	DupOuter = GetDuplicatedObject(Object->GetOuter());
			if(DupOuter != NULL)
			{
				// The object's outer is being duplicated, create a duplicate of this object.
				Result = AddDuplicate(Object,UObject::StaticConstructObject(Object->GetClass(),DupOuter,*Object->GetName(),ApplyFlags|Object->GetMaskedFlags(FlagMask),
					Object->GetArchetype(),GError,INVALID_OBJECT,InstanceGraph));
			}
		}
	}

	return Result;
}

/*----------------------------------------------------------------------------
	Saving Packages.
----------------------------------------------------------------------------*/
/**
 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
 * is in when a loading error occurs.
 *
 * This is overridden for the specific Archive Types
 **/
FString FArchiveSaveTagExports::GetArchiveName() const
{
	return Outer != NULL
		? *FString::Printf(TEXT("SaveTagExports (%s)"), *Outer->GetName())
		: TEXT("SaveTagExports");
}

FArchive& FArchiveSaveTagExports::operator<<( UObject*& Obj )
{
	if( Obj && (Obj->IsIn(Outer) || Obj->HasAnyFlags(RF_ForceTagExp)) && !Obj->HasAnyFlags(RF_Transient|RF_TagExp) )
	{
#if 0
		// the following line should be used to track down the cause behind
		// "Load flags don't match result of *" errors.  The most common reason
		// is a child component template is modifying the load flags of it parent
		// component template (that is, the component template that it is overriding
		// from a parent class).
		// @MISMATCHERROR
		if ( Obj->GetFName() == TEXT("BrushComponent0") && Obj->GetOuter()->GetFName() == TEXT("Default__Volume") )
		{
			debugf(TEXT(""));
		}
#endif
		// Object is in outer so we don't need RF_ForceTagExp.
		if( Obj->IsIn(Outer) )
		{
			Obj->ClearFlags(RF_ForceTagExp);
		}

		// Set flags.
		Obj->SetFlags(RF_TagExp);

		// first, serialize this object's archetype so that if the archetype's load flags are set correctly if this object
		// is encountered by the SaveTagExports archive before its archetype.  This is necessary for the code below which verifies
		// that the load flags for the archetype and the load flags for the object match
		UObject* Template = Obj->GetArchetype();
		*this << Template;

		if ( Obj->HasAnyFlags(RF_ClassDefaultObject) )
		{
			if ( Obj->GetClass()->HasAnyClassFlags(CLASS_Intrinsic) )
			{
				// if this class is an intrinsic class, its class default object
				// shouldn't be saved, as it does not contain any data that should be saved
				Obj->ClearFlags(RF_TagExp);
			}
			else
			{
				// class default objects should always be loaded
				Obj->SetFlags(RF_LoadContextFlags);
			}
		}
		else
		{
			if( Obj->NeedsLoadForEdit() )
			{
				Obj->SetFlags(RF_LoadForEdit);
			}

			if( Obj->NeedsLoadForClient() )
			{
				Obj->SetFlags(RF_LoadForClient);
			}

			if( Obj->NeedsLoadForServer() )
			{
				Obj->SetFlags(RF_LoadForServer);
			}

			// skip these checks if the Template object is the CDO for an intrinsic class, as they will never have any load flags set.
			if ( Template != NULL 
			&& (!Template->GetClass()->HasAnyClassFlags(CLASS_Intrinsic) || !Template->HasAnyFlags(RF_ClassDefaultObject)) )
			{
				EObjectFlags PropagateFlags = Obj->GetMaskedFlags(RF_LoadContextFlags);

				// if the object's Template is not in the same package, we can't adjust its load flags to ensure that this object can be loaded
				// therefore make this a critical error so that we don't discover later that we won't be able to load this object.
				if ( !Template->IsIn(Obj->GetOutermost())

				// if the template or the object have the RF_ForceTagExp flag, then they'll be saved into the same cooked package even if they don't
				// share the same Outermost...
				&& !Template->HasAnyFlags(RF_ForceTagExp)

				// if the object has the forcetagexp flag, no need to worry about mismatched load flags unless its template is not going to be saved into this package
				&& (!Obj->HasAnyFlags(RF_ForceTagExp) || !Template->IsIn(Outer)) )
				{
					// if the component's archetype is not in the same package, we won't be able
					// to propagate the load flags to the archetype, so make sure that the
					// derived template's load flags are <= the parent's load flags.
					FString LoadFlagsString;
					if ( Obj->HasAnyFlags(RF_LoadForEdit) && !Template->NeedsLoadForEdit() )
					{
						LoadFlagsString = TEXT("RF_LoadForEdit");
					}
					if ( Obj->HasAnyFlags(RF_LoadForClient) && !Template->NeedsLoadForClient() )
					{
						if ( LoadFlagsString.Len() > 0 )
						{
							LoadFlagsString += TEXT(",");
						}
						LoadFlagsString += TEXT("RF_LoadForClient");
					}
					if ( Obj->HasAnyFlags(RF_LoadForServer) && !Template->NeedsLoadForServer() )
					{
						if ( LoadFlagsString.Len() > 0 )
						{
							LoadFlagsString += TEXT(",");
						}
						LoadFlagsString += TEXT("RF_LoadForServer");
					}

					if ( LoadFlagsString.Len() > 0 )
					{
						if ( Obj->IsA(UComponent::StaticClass()) && Template->IsTemplate() )
						{
							//@todo localize
							appErrorf(TEXT("Mismatched load flag/s (%s) on component template parent from different package.  '%s' cannot be derived from '%s'"),
								*LoadFlagsString, *Obj->GetPathName(), *Template->GetPathName());
						}
						else
						{
							//@todo localize
							appErrorf(TEXT("Mismatched load flag/s (%s) on object archetype from different package.  Loading '%s' would fail because its archetype '%s' wouldn't be created."),
								*LoadFlagsString, *Obj->GetPathName(), *Template->GetPathName());
						}
					}
				}

				// this is a normal object - it's template MUST be loaded anytime
				// the object is loaded, so make sure the load flags match the object's
				// load flags (note that it's OK for the template itself to be loaded
				// in situations where the object is not, but vice-versa is not OK)
				Template->SetFlags( PropagateFlags );
			}
		}

		// Recurse with this object's class and package.
		UObject* Class  = Obj->GetClass();
		UObject* Parent = Obj->GetOuter();
		*this << Class << Parent;

		TaggedObjects.AddItem(Obj);
	}
	return *this;
}

/**
 * Serializes the specified object, tagging all objects it references.
 *
 * @param	BaseObject	the object that should be serialized; usually the package root or
 *						[in the case of a map package] the map's UWorld object.
 */
void FArchiveSaveTagExports::ProcessBaseObject(UObject* BaseObject )
{
	(*this) << BaseObject;
	ProcessTaggedObjects();
}

/**
 * Iterates over all objects which were encountered during serialization of the root object, serializing each one in turn.
 * Objects encountered during that serialization are then added to the array and iteration continues until no new objects are
 * added to the array.
 */
void FArchiveSaveTagExports::ProcessTaggedObjects()
{
	TArray<UObject*> CurrentlyTaggedObjects;
	CurrentlyTaggedObjects.Empty(UObject::GetObjectArrayNum());
	while ( TaggedObjects.Num() )
	{
		CurrentlyTaggedObjects += TaggedObjects;
		TaggedObjects.Empty();

		for ( INT ObjIndex = 0; ObjIndex < CurrentlyTaggedObjects.Num(); ObjIndex++ )
		{
			UObject* Obj = CurrentlyTaggedObjects(ObjIndex);

			// Recurse with this object's children.
			if ( Obj->HasAnyFlags(RF_ClassDefaultObject) )
			{
				Obj->GetClass()->SerializeDefaultObject(Obj, *this);
			}
			else
			{
				Obj->Serialize( *this );
			}
		}

		CurrentlyTaggedObjects.Empty(UObject::GetObjectArrayNum());
	}
}

/**
 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
 * is in when a loading error occurs.
 *
 * This is overridden for the specific Archive Types
 **/
FString FArchiveSaveTagImports::GetArchiveName() const
{
	if ( Linker != NULL && Linker->LinkerRoot )
	{
		return FString::Printf(TEXT("SaveTagImports (%s)"), *Linker->LinkerRoot->GetName());
	}

	return TEXT("SaveTagImports");
}

FArchive& FArchiveSaveTagImports::operator<<( UObject*& Obj )
{
	// handle potential cross level pointers
	if (bIsNextObjectSerializePotentialCrossLevelRef)
	{
		// reset the flag
		bIsNextObjectSerializePotentialCrossLevelRef = FALSE;

		// if it's NULL, then it could be an cross level import to an unloaded level's object, but we can skip it
		// if it exists, and isn't an export, than it's an import, and therefore in another package, so we treat it 
		if (Obj && !Obj->HasAnyFlags(RF_TagExp))
		{
			// this is a cross level reference that doesn't need to be tagged below
			return *this;
		}
	}

	if( Obj && !Obj->IsPendingKill() )
	{
		if( !Obj->HasAnyFlags(RF_Transient) || Obj->HasAllFlags(RF_Native) )
		{
			// remember it as a dependency, unless it's a top level pacakge or native
			UBOOL bIsTopLevelPackage = Obj->GetOuter() == NULL && Obj->IsA(UPackage::StaticClass());
			UBOOL bIsNative = Obj->HasAnyFlags(RF_Native);
			UObject* Outer = Obj->GetOuter();
			
			// go up looking for native classes
			while (!bIsNative && Outer)
			{
				if (Outer->IsA(UClass::StaticClass()) && Outer->HasAnyFlags(RF_Native))
				{
					bIsNative = true;
				}
				Outer = Outer->GetOuter();
			}

			// only add valid objects
			if (!bIsTopLevelPackage && !bIsNative)
			{
				Dependencies.AddUniqueItem(Obj);
			}

			if( !Obj->HasAnyFlags(RF_TagExp) )
			{
				// mark this object as an import
				Obj->SetFlags( RF_TagImp );

				if ( Obj->HasAnyFlags(RF_ClassDefaultObject) )
				{
					Obj->SetFlags(RF_LoadContextFlags);
				}
				else
				{
					if( !Obj->HasAnyFlags( RF_NotForEdit  ) )
					{
						Obj->SetFlags(RF_LoadForEdit);
					}

					if( !Obj->HasAnyFlags( RF_NotForClient) )
					{
						Obj->SetFlags(RF_LoadForClient);
					}

					if( !Obj->HasAnyFlags( RF_NotForServer) )
					{
						Obj->SetFlags(RF_LoadForServer);
					}
				}

				UObject* Parent = Obj->GetOuter();
				if( Parent )
				{
					*this << Parent;
				}
			}
		}
	}
	return *this;
}

/*----------------------------------------------------------------------------
	FArchiveObjectReferenceCollector.
----------------------------------------------------------------------------*/

/** 
 * UObject serialize operator implementation
 *
 * @param Object	reference to Object reference
 * @return reference to instance of this class
 */
FArchive& FArchiveObjectReferenceCollector::operator<<( UObject*& Object )
{
	// Avoid duplicate entries.
	if ( Object != NULL )
	{
		if ( (LimitOuter == NULL || (Object->GetOuter() == LimitOuter || (!bRequireDirectOuter && Object->IsIn(LimitOuter)))) )
		{
			if ( !ObjectArray->ContainsItem(Object) )
			{
				check( Object->IsValid() );
				ObjectArray->AddItem( Object );
			}

			// check this object for any potential object references
			if ( bSerializeRecursively && !SerializedObjects.Find(Object) )
			{
				SerializedObjects.Add(Object);
				Object->Serialize(*this);
			}
		}
	}
	return *this;
}

/*----------------------------------------------------------------------------
	Transparent compression/ decompression archives.
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
	FArchiveSaveCompressedProxy
----------------------------------------------------------------------------*/

/** 
 * Constructor, initializing all member variables and allocating temp memory.
 *
 * @param	InCompressedData [ref]	Array of bytes that is going to hold compressed data
 * @param	InCompressionFlags		Compression flags to use for compressing data
 */
FArchiveSaveCompressedProxy::FArchiveSaveCompressedProxy( TArray<BYTE>& InCompressedData, ECompressionFlags InCompressionFlags )
:	CompressedData(InCompressedData)
,	CompressionFlags(InCompressionFlags)
{
	ArIsSaving							= TRUE;
	ArIsPersistent						= TRUE;
	ArWantBinaryPropertySerialization	= TRUE;
	bShouldSerializeToArray				= FALSE;
	RawBytesSerialized					= 0;
	CurrentIndex						= 0;

	// Allocate temporary memory.
	TmpDataStart	= (BYTE*) appMalloc(LOADING_COMPRESSION_CHUNK_SIZE);
	TmpDataEnd		= TmpDataStart + LOADING_COMPRESSION_CHUNK_SIZE;
	TmpData			= TmpDataStart;
}

/** Destructor, flushing array if needed. Also frees temporary memory. */
FArchiveSaveCompressedProxy::~FArchiveSaveCompressedProxy()
{
	// Flush is required to write out remaining tmp data to array.
	Flush();
	// Free temporary memory allocated.
	appFree( TmpDataStart );
	TmpDataStart	= NULL;
	TmpDataEnd		= NULL;
	TmpData			= NULL;
}

/**
 * Flushes tmp data to array.
 */
void FArchiveSaveCompressedProxy::Flush()
{
	if( TmpData - TmpDataStart > 0 )
	{
		// This will call Serialize so we need to indicate that we want to serialize to array.
		bShouldSerializeToArray = TRUE;
		SerializeCompressed( TmpDataStart, TmpData - TmpDataStart, CompressionFlags );
		bShouldSerializeToArray = FALSE;
		// Buffer is drained, reset.
		TmpData	= TmpDataStart;
	}
}

/**
 * Serializes data to archive. This function is called recursively and determines where to serialize
 * to and how to do so based on internal state.
 *
 * @param	Data	Pointer to serialize to
 * @param	Count	Number of bytes to read
 */
void FArchiveSaveCompressedProxy::Serialize( void* InData, INT Count )
{
	BYTE* SrcData = (BYTE*) InData;
	// If counter > 1 it means we're calling recursively and therefore need to write to compressed data.
	if( bShouldSerializeToArray )
	{
		// Add space in array if needed and copy data there.
		INT BytesToAdd = CurrentIndex + Count - CompressedData.Num();
		if( BytesToAdd > 0 )
		{
			CompressedData.Add(BytesToAdd);
		}
		// Copy memory to array.
		appMemcpy( &CompressedData(CurrentIndex), SrcData, Count );
		CurrentIndex += Count;
	}
	// Regular call to serialize, queue for compression.
	else
	{
		while( Count )
		{
			INT BytesToCopy = Min<INT>( Count, (INT)(TmpDataEnd - TmpData) );
			// Enough room in buffer to copy some data.
			if( BytesToCopy )
			{
				appMemcpy( TmpData, SrcData, BytesToCopy );
				Count -= BytesToCopy;
				TmpData += BytesToCopy;
				SrcData += BytesToCopy;
				RawBytesSerialized += BytesToCopy;
			}
			// Tmp buffer fully exhausted, compress it.
			else
			{
				// Flush existing data to array after compressing it. This will call Serialize again 
				// so we need to handle recursion.
				Flush();
			}
		}
	}
}

/**
 * Seeking is only implemented internally for writing out compressed data and asserts otherwise.
 * 
 * @param	InPos	Position to seek to
 */
void FArchiveSaveCompressedProxy::Seek( INT InPos )
{
	// Support setting position in array.
	if( bShouldSerializeToArray )
	{
		CurrentIndex = InPos;
	}
	else
	{
		appErrorf(TEXT("Seeking not supported with FArchiveSaveCompressedProxy"));
	}
}

/**
 * @return current position in uncompressed stream in bytes
 */
INT FArchiveSaveCompressedProxy::Tell()
{
	// If we're serializing to array, return position in array.
	if( bShouldSerializeToArray )
	{
		return CurrentIndex;
	}
	// Return global position in raw uncompressed stream.
	else
	{
		return RawBytesSerialized;
	}
}


/*----------------------------------------------------------------------------
	FArchiveLoadCompressedProxy
----------------------------------------------------------------------------*/

/** 
 * Constructor, initializing all member variables and allocating temp memory.
 *
 * @param	InCompressedData	Array of bytes that is holding compressed data
 * @param	InCompressionFlags	Compression flags that were used to compress data
 */
FArchiveLoadCompressedProxy::FArchiveLoadCompressedProxy( const TArray<BYTE>& InCompressedData, ECompressionFlags InCompressionFlags )
:	CompressedData(InCompressedData)
,	CompressionFlags(InCompressionFlags)
{
	ArIsLoading							= TRUE;
	ArIsPersistent						= TRUE;
	ArWantBinaryPropertySerialization	= TRUE;
	bShouldSerializeFromArray			= FALSE;
	RawBytesSerialized					= 0;
	CurrentIndex						= 0;

	// Allocate temporary memory.
	TmpDataStart	= (BYTE*) appMalloc(LOADING_COMPRESSION_CHUNK_SIZE);
	TmpDataEnd		= TmpDataStart + LOADING_COMPRESSION_CHUNK_SIZE;
	TmpData			= TmpDataEnd;
}

/** Destructor, freeing temporary memory. */
FArchiveLoadCompressedProxy::~FArchiveLoadCompressedProxy()
{
	// Free temporary memory allocated.
	appFree( TmpDataStart );
	TmpDataStart	= NULL;
	TmpDataEnd		= NULL;
	TmpData			= NULL;
}

/**
 * Flushes tmp data to array.
 */
void FArchiveLoadCompressedProxy::DecompressMoreData()
{
	// This will call Serialize so we need to indicate that we want to serialize from array.
	bShouldSerializeFromArray = TRUE;
	SerializeCompressed( TmpDataStart, LOADING_COMPRESSION_CHUNK_SIZE /** it's ignored, but that's how much we serialize */, CompressionFlags );
	bShouldSerializeFromArray = FALSE;
	// Buffer is filled again, reset.
	TmpData = TmpDataStart;
}

/**
 * Serializes data from archive. This function is called recursively and determines where to serialize
 * from and how to do so based on internal state.
 *
 * @param	InData	Pointer to serialize to
 * @param	Count	Number of bytes to read
 */
void FArchiveLoadCompressedProxy::Serialize( void* InData, INT Count )
{
	BYTE* DstData = (BYTE*) InData;
	// If counter > 1 it means we're calling recursively and therefore need to write to compressed data.
	if( bShouldSerializeFromArray )
	{
		// Add space in array and copy data there.
		check(CurrentIndex+Count<=CompressedData.Num());
		appMemcpy( DstData, &CompressedData(CurrentIndex), Count );
		CurrentIndex += Count;
	}
	// Regular call to serialize, read from temp buffer
	else
	{	
		while( Count )
		{
			INT BytesToCopy = Min<INT>( Count, (INT)(TmpDataEnd - TmpData) );
			// Enough room in buffer to copy some data.
			if( BytesToCopy )
			{
				// We pass in a NULL pointer when forward seeking. In that case we don't want
				// to copy the data but only care about pointing to the proper spot.
				if( DstData )
				{
					appMemcpy( DstData, TmpData, BytesToCopy );
					DstData += BytesToCopy;
				}
				Count -= BytesToCopy;
				TmpData += BytesToCopy;
				RawBytesSerialized += BytesToCopy;
			}
			// Tmp buffer fully exhausted, decompress new one.
			else
			{
				// Decompress more data. This will call Serialize again so we need to handle recursion.
				DecompressMoreData();
			}
		}
	}
}

/**
 * Seeks to the passed in position in the stream. This archive only supports forward seeking
 * and implements it by serializing data till it reaches the position.
 */
void FArchiveLoadCompressedProxy::Seek( INT InPos )
{
	INT CurrentPos = Tell();
	INT Difference = InPos - CurrentPos;
	// We only support forward seeking.
	check(Difference>=0);
	// Seek by serializing data, albeit with NULL destination so it's just decompressing data.
	Serialize( NULL, Difference );
}

/**
 * @return current position in uncompressed stream in bytes.
 */
INT FArchiveLoadCompressedProxy::Tell()
{
	return RawBytesSerialized;
}

/*----------------------------------------------------------------------------
	The End
----------------------------------------------------------------------------*/






