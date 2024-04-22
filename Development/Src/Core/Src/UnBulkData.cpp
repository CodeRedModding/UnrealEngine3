/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	Constructors and operators
-----------------------------------------------------------------------------*/

/** Whether to track information of how bulk data is being used */
#define TRACK_BULKDATA_USE !CONSOLE


#if TRACK_BULKDATA_USE

/** Map from bulk data pointer to object it is contained by */
TMap<FUntypedBulkData*,UObject*> BulkDataToObjectMap;

/**
 * Helper structure associating an object and a size for sorting purposes.
 */
struct FObjectAndSize
{
	FObjectAndSize( UObject* InObject, INT InSize )
	:	Object( InObject )
	,	Size( InSize )
	{}

	/** Object associated with size. */
	UObject*	Object;
	/** Size associated with object. */
	INT			Size;
};

/** Hash function required for TMap support */
DWORD GetTypeHash( const FUntypedBulkData* BulkData )
{
	return PointerHash(BulkData);
}

/** Compare operator, sorting by size in descending order */
IMPLEMENT_COMPARE_CONSTREF(FObjectAndSize,UnBulkData,{ return B.Size - A.Size; });

#endif


/**
 * Constructor, initializing all member variables.
 */
FUntypedBulkData::FUntypedBulkData()
{
	InitializeMemberVariables();
}

/**
 * Copy constructor. Use the common routine to perform the copy.
 *
 * @param Other the source array to copy
 */
FUntypedBulkData::FUntypedBulkData( const FUntypedBulkData& Other )
{
	InitializeMemberVariables();

	// Prepare bulk data pointer. Can't call any functions that would call virtual GetElementSize on "this" as
	// we're in the constructor of the base class and would hence call a pure virtual.
	ElementCount	= Other.ElementCount;
	check(bShouldFreeOnEmpty);
	BulkData		= appRealloc( BulkData, Other.GetBulkDataSize() );

	// Copy data over.
	Copy( Other );

#if TRACK_BULKDATA_USE
	BulkDataToObjectMap.Set( this, NULL );
#endif
}

/**
 * Virtual destructor, free'ing allocated memory.
 */
FUntypedBulkData::~FUntypedBulkData()
{
	check( LockStatus == LOCKSTATUS_Unlocked );
	// Free memory.
	if( bShouldFreeOnEmpty )
	{
		appFree( BulkData );
	}
	BulkData = NULL;
	// Detach from archive.
	if( AttachedAr )
	{
		AttachedAr->DetachBulkData( this, FALSE );
		check( AttachedAr == NULL );
	}
#if TRACK_BULKDATA_USE
	BulkDataToObjectMap.Remove( this );
#endif
}

/**
 * Copies the source array into this one after detaching from archive.
 *
 * @param Other the source array to copy
 */
FUntypedBulkData& FUntypedBulkData::operator=( const FUntypedBulkData& Other )
{
	// Remove bulk data, avoiding potential load in Lock call.
	RemoveBulkData();
	
	// Reallocate to size of src.
	Lock(LOCK_READ_WRITE);
	Realloc(Other.GetElementCount());

	// Copy data over.
	Copy( Other );

	// Unlock.
	Unlock();

	return *this;
}


/*-----------------------------------------------------------------------------
	Static functions.
-----------------------------------------------------------------------------*/

/**
 * Dumps detailed information of bulk data usage.
 *
 * @param Log FOutputDevice to use for logging
 */
void FUntypedBulkData::DumpBulkDataUsage( FOutputDevice& Log )
{
#if TRACK_BULKDATA_USE
	// Arrays about to hold per object and per class size information.
	TArray<FObjectAndSize> PerObjectSizeArray;
	TArray<FObjectAndSize> PerClassSizeArray;

	// Iterate over all "live" bulk data and add size to arrays if it is loaded.
	for( TMap<FUntypedBulkData*,UObject*>::TIterator It(BulkDataToObjectMap); It; ++It )
	{
		FUntypedBulkData*	BulkData	= It.Key();
		UObject*			Owner		= It.Value();
		// Only add bulk data that is consuming memory to array.
		if( BulkData->IsBulkDataLoaded() && BulkData->GetBulkDataSize() > 0 )
		{
			// Per object stats.
			PerObjectSizeArray.AddItem( FObjectAndSize( Owner, BulkData->GetBulkDataSize() ) );
			
			// Per class stats.
			UBOOL bFoundExistingPerClassSize = FALSE;
			// Iterate over array, trying to find existing entry.
			for( INT PerClassIndex=0; PerClassIndex<PerClassSizeArray.Num(); PerClassIndex++ )
			{
				FObjectAndSize& PerClassSize = PerClassSizeArray( PerClassIndex );
				// Add to existing entry if found.
				if( PerClassSize.Object == Owner->GetClass() )
				{
					PerClassSize.Size += BulkData->GetBulkDataSize();
					bFoundExistingPerClassSize = TRUE;
					break;
				}
			}
			// Add new entry if we didn't find an existing one.
			if( !bFoundExistingPerClassSize )
			{
				PerClassSizeArray.AddItem( FObjectAndSize( Owner->GetClass(), BulkData->GetBulkDataSize() ) );
			}
		}
	}

	// Sort by size.
	Sort<USE_COMPARE_CONSTREF(FObjectAndSize,UnBulkData)>( PerObjectSizeArray.GetTypedData(), PerObjectSizeArray.Num() );
	Sort<USE_COMPARE_CONSTREF(FObjectAndSize,UnBulkData)>( PerClassSizeArray.GetTypedData(), PerClassSizeArray.Num() );

	// Log information.
	debugf(TEXT(""));
	debugf(TEXT("Per class summary of bulk data use:"));
	for( INT PerClassIndex=0; PerClassIndex<PerClassSizeArray.Num(); PerClassIndex++ )
	{
		const FObjectAndSize& PerClassSize = PerClassSizeArray( PerClassIndex );
		Log.Logf( TEXT("  %5d KByte of bulk data for Class %s"), PerClassSize.Size / 1024, *PerClassSize.Object->GetPathName() );
	}
	debugf(TEXT(""));
	debugf(TEXT("Detailed per object stats of bulk data use:"));
	for( INT PerObjectIndex=0; PerObjectIndex<PerObjectSizeArray.Num(); PerObjectIndex++ )
	{
		const FObjectAndSize& PerObjectSize = PerObjectSizeArray( PerObjectIndex );
		Log.Logf( TEXT("  %5d KByte of bulk data for %s"), PerObjectSize.Size / 1024, *PerObjectSize.Object->GetFullName() );
	}
	debugf(TEXT(""));
#else
	debugf(TEXT("Please recompiled with TRACK_BULKDATA_USE set to 1 in UnBulkData.cpp."));
#endif
}


/*-----------------------------------------------------------------------------
	Accessors.
-----------------------------------------------------------------------------*/

/**
 * Returns the number of elements in this bulk data array.
 *
 * @return Number of elements in this bulk data array
 */
INT FUntypedBulkData::GetElementCount() const
{
	return ElementCount;
}
/**
 * Returns the size of the bulk data in bytes.
 *
 * @return Size of the bulk data in bytes
 */
INT FUntypedBulkData::GetBulkDataSize() const
{
	return GetElementCount() * GetElementSize();
}
/**
 * Returns the size of the bulk data on disk. This can differ from GetBulkDataSize if
 * BULKDATA_SerializeCompressed is set.
 *
 * @return Size of the bulk data on disk or INDEX_NONE in case there's no association
 */
INT FUntypedBulkData::GetBulkDataSizeOnDisk() const
{
	return BulkDataSizeOnDisk;
}
/**
 * Returns the offset into the file the bulk data is located at.
 *
 * @return Offset into the file or INDEX_NONE in case there is no association
 */
INT FUntypedBulkData::GetBulkDataOffsetInFile() const
{
	return BulkDataOffsetInFile;
}
/**
 * Returns whether the bulk data is stored compressed on disk.
 *
 * @return TRUE if data is compressed on disk, FALSE otherwise
 */
UBOOL FUntypedBulkData::IsStoredCompressedOnDisk() const
{
	return (BulkDataFlags & BULKDATA_SerializeCompressed) ? TRUE : FALSE;
}

/**
 * Returns flags usable to decompress the bulk data
 * 
 * @return COMPRESS_NONE if the data was not compressed on disk, or valid flags to pass to appUncompressMemory for this data
 */
ECompressionFlags FUntypedBulkData::GetDecompressionFlags() const
{
	return
		(BulkDataFlags & BULKDATA_SerializeCompressedZLIB) ? COMPRESS_ZLIB : 
		(BulkDataFlags & BULKDATA_SerializeCompressedLZX) ? COMPRESS_LZX : 
		(BulkDataFlags & BULKDATA_SerializeCompressedLZO) ? COMPRESS_LZO : 
		COMPRESS_None;
}

/**
 * Returns whether the bulk data is stored in a separate file and henceforth cannot
 * be "locked"/ read.
 *
 * @return TRUE if data is stored in separate file, FALSE otherwise
 */
UBOOL FUntypedBulkData::IsStoredInSeparateFile() const
{
	return (BulkDataFlags & BULKDATA_StoreInSeparateFile) ? TRUE : FALSE;
}
/**
 * Returns whether the bulk data is currently loaded and resident in memory.
 *
 * @return TRUE if bulk data is loaded, FALSE otherwise
 */
UBOOL FUntypedBulkData::IsBulkDataLoaded() const
{
	return BulkData != NULL;
}

/**
* Returns whether this bulk data is used
* @return TRUE if BULKDATA_Unused is not set
*/
UBOOL FUntypedBulkData::IsAvailableForUse() const
{
	return (BulkDataFlags & BULKDATA_Unused) ? FALSE : TRUE;
}

/**
 * Returns the last saved bulk data flags.
 *
 * @param Last saved bulk data flags.
 */
DWORD FUntypedBulkData::GetSavedBulkDataFlags() const
{
	return SavedBulkDataFlags;
}
/**
 * Returns the last saved number of elements in this bulk data array.
 *
 * @return Last saved number of elements in this bulk data array
 */	
INT FUntypedBulkData::GetSavedElementCount() const
{
	return SavedElementCount;
}
/**
 * Returns the last saved offset into the file the bulk data is located at.
 *
 * @return Last saved Offset into the file or INDEX_NONE in case there is no association
 */
INT FUntypedBulkData::GetSavedBulkDataOffsetInFile() const
{
	return SavedBulkDataOffsetInFile;
}
/**
 * Returns the last saved size of the bulk data on disk. This can differ from GetBulkDataSize 
 * if BULKDATA_SerializeCompressed is set.
 *
 * @return Size of the last saved bulk data on disk or INDEX_NONE in case there's no association
 */
INT FUntypedBulkData::GetSavedBulkDataSizeOnDisk() const
{
	return SavedBulkDataSizeOnDisk;
}

/*-----------------------------------------------------------------------------
	Data retrieval and manipulation.
-----------------------------------------------------------------------------*/

/**
 * Retrieves a copy of the bulk data.
 *
 * @param Dest [in/out] Pointer to pointer going to hold copy, can point to NULL pointer in which case memory is allocated
 * @param bDiscardInternalCopy Whether to discard/ free the potentially internally allocated copy of the data
 */
void FUntypedBulkData::GetCopy( void** Dest, UBOOL bDiscardInternalCopy )
{
	check( LockStatus == LOCKSTATUS_Unlocked );
	check( !(BulkDataFlags & BULKDATA_StoreInSeparateFile) );
	check( Dest );

	// Passed in memory is going to be used.
	if( *Dest )
	{
		// The data is already loaded so we can simply use a mempcy.
		if( BulkData )
		{
			// Copy data into destination memory.
			appMemcpy( *Dest, BulkData, GetBulkDataSize() );
			// Discard internal copy if wanted and we're still attached to an archive or if we're
			// single use bulk data.
			if( bDiscardInternalCopy && (AttachedAr || (BulkDataFlags & BULKDATA_SingleUse)) )
			{
				if( bShouldFreeOnEmpty )
				{
					appFree( BulkData );
				}
				BulkData = NULL;
			}
		}
		// Data isn't currently loaded so we need to load it from disk.
		else
		{
			LoadDataIntoMemory( *Dest );
		}
	}
	// Passed in memory is NULL so we need to allocate some.
	else
	{
		// The data is already loaded so we can simply use a mempcy.
		if( BulkData )
		{
			// If the internal copy should be discarded and we are still attached to an archive we can
			// simply "return" the already existing copy and NULL out the internal reference. We can
			// also do this if the data is single use like e.g. when uploading texture data.
			if( bDiscardInternalCopy && (AttachedAr || (BulkDataFlags & BULKDATA_SingleUse)) )
			{
				*Dest = BulkData;
				BulkData = NULL;
			}
			// Can't/ Don't discard so we need to allocate and copy.
			else
			{
				// Allocate enough memory for data...
				*Dest = appMalloc( GetBulkDataSize() );
				// ... and copy it into memory now pointed to by out parameter.
				appMemcpy( *Dest, BulkData, GetBulkDataSize() );
			}
		}
		// Data isn't currently loaded so we need to load it from disk.
		else
		{
			// Allocate enougn memory for data...
			*Dest = appMalloc( GetBulkDataSize() );
			// ... and directly load into it.
			LoadDataIntoMemory( *Dest );
		}
	}
}

/**
 * Locks the bulk data and returns a pointer to it.
 *
 * @param	LockFlags	Flags determining lock behavior
 */
void* FUntypedBulkData::Lock( DWORD LockFlags )
{
	check( LockStatus == LOCKSTATUS_Unlocked );
	check( !(BulkDataFlags & BULKDATA_StoreInSeparateFile) );

	// Make sure bulk data is loaded.
	MakeSureBulkDataIsLoaded();
		
	// Read-write operations are allowed on returned memory.
	if( LockFlags & LOCK_READ_WRITE )
	{
		LockStatus = LOCKSTATUS_ReadWriteLock;

		// We need to detach from the archive to not be able to clobber changes by serializing
		// over them.
		if( AttachedAr )
		{
			// Detach bulk data. This will call DetachFromArchive which in turn will clear AttachedAr.
			AttachedAr->DetachBulkData( this, FALSE );
			check( AttachedAr == NULL );
		}
	}
	// Only read operations are allowed on returned memory.
	else if( LockFlags & LOCK_READ_ONLY )
	{
		LockStatus = LOCKSTATUS_ReadOnlyLock;
	}
	else
	{
		appErrorf(TEXT("Unknown lock flag %i"),LockFlags);
	}

	check( BulkData );
	return BulkData;
}

/**
 * Change size of locked bulk data. Only valid if locked via read-write lock.
 *
 * @param InElementCount	Number of elements array should be resized to
 */
void* FUntypedBulkData::Realloc( INT InElementCount )
{
	check( LockStatus == LOCKSTATUS_ReadWriteLock );
	// Progate element count and reallocate data based on new size.
	ElementCount	= InElementCount;
	check(bShouldFreeOnEmpty);
	BulkData		= appRealloc( BulkData, GetBulkDataSize() );
	return BulkData;
}

/** 
 * Unlocks bulk data after which point the pointer returned by Lock no longer is valid.
 */
void FUntypedBulkData::Unlock()
{
	check( LockStatus != LOCKSTATUS_Unlocked );
	LockStatus = LOCKSTATUS_Unlocked;
	// Free pointer if we're still attached to an archive and therefore can load the data again
	// if required later on. Also free if we're guaranteed to only to access the data once.
	if( AttachedAr || (BulkDataFlags & BULKDATA_SingleUse) )
	{
		if( bShouldFreeOnEmpty )
		{
			appFree( BulkData );
		}
		BulkData = NULL;
	}
}

/**
 * Clears/ removes the bulk data and resets element count to 0.
 */
void FUntypedBulkData::RemoveBulkData()
{
	check( LockStatus == LOCKSTATUS_Unlocked );
	// Detach from archive without loading first.
	if( AttachedAr )
	{
		AttachedAr->DetachBulkData( this, FALSE );
		check( AttachedAr == NULL );
	}
	// Resize to 0 elements.
	ElementCount	= 0;
	if( ShouldFreeOnEmpty() )
	{
		appFree( BulkData );
	}
	BulkData		= NULL;
}

/**
 * Forces the bulk data to be resident in memory and detaches the archive.
 */
void FUntypedBulkData::ForceBulkDataResident()
{
	// Make sure bulk data is loaded.
	MakeSureBulkDataIsLoaded();

	// Detach from the archive 
	if( AttachedAr )
	{
		// Detach bulk data. This will call DetachFromArchive which in turn will clear AttachedAr.
		AttachedAr->DetachBulkData( this, FALSE );
		check( AttachedAr == NULL );
	}
}

/**
 * Sets the passed in bulk data flags.
 *
 * @param BulkDataFlagsToSet	Bulk data flags to set
 */
void FUntypedBulkData::SetBulkDataFlags( DWORD BulkDataFlagsToSet )
{
	BulkDataFlags |= BulkDataFlagsToSet;
}

/**
* Gets the current bulk data flags.
*
* @return Bulk data flags currently set
*/
DWORD FUntypedBulkData::GetBulkDataFlags() const
{
	return BulkDataFlags;
}

/**
 * Clears the passed in bulk data flags.
 *
 * @param BulkDataFlagsToClear	Bulk data flags to clear
 */
void FUntypedBulkData::ClearBulkDataFlags( DWORD BulkDataFlagsToClear )
{
	BulkDataFlags &= ~BulkDataFlagsToClear;
}

/**
 * BulkData memory allocated from a resource should only be freed by the resource
 *
 * @return TRUE if bulk data should free allocated memory
 */
UBOOL FUntypedBulkData::ShouldFreeOnEmpty() const
{
	return bShouldFreeOnEmpty;
}


/*-----------------------------------------------------------------------------
	Serialization.
-----------------------------------------------------------------------------*/

/**
* Serialize function used to serialize this bulk data structure.
*
* @param Ar	Archive to serialize with
* @param Owner	Object owning the bulk data
* @param Idx	Index of bulk data item being serialized
* @param bSkipLoad	if set don't load data but rather seek over it and say we're empty
*					(used when load time check determines this platform/detailmode/etc should use an empty resource)
*/
void FUntypedBulkData::Serialize(FArchive& Ar, UObject* Owner, INT Idx, UBOOL bSkipLoad)
{
	check( LockStatus == LOCKSTATUS_Unlocked );

	if(Ar.IsTransacting())
	{
		// Special case for transacting bulk data arrays.

		// Flags for bulk data.
		Ar << BulkDataFlags;
		// Number of elements in array.
		Ar << ElementCount;

		if(Ar.IsLoading())
		{
			// Allocate bulk data.
			check(bShouldFreeOnEmpty);
			BulkData = appRealloc( BulkData, GetBulkDataSize() );

			// Deserialize bulk data.
			SerializeBulkData( Ar, BulkData );
		}
		else if(Ar.IsSaving())
		{
			// Make sure bulk data is loaded.
			MakeSureBulkDataIsLoaded();

			// Serialize bulk data.
			SerializeBulkData( Ar, BulkData );
		}
	}
	else if( Ar.IsPersistent() && !Ar.IsObjectReferenceCollector() && !Ar.ShouldSkipBulkData() )
	{
#if TRACK_BULKDATA_USE
		BulkDataToObjectMap.Set( this, Owner );
#endif
		// Keep track of first serialized item to be able to overwrite it.
		INT BulkDataFlagsPos = Ar.Tell();

		// Whether we only want to serialize payload.
		UBOOL bOnlySerializePayload = Ar.IsSaving() && (BulkDataFlags & BULKDATA_StoreOnlyPayload);

		// Don't serialize status information if we only care about payload; only used when saving.
		if( !bOnlySerializePayload )
		{
			// Flags for bulk data.
			Ar << BulkDataFlags;

			// Number of elements in array.
			Ar << ElementCount;
		}

		// We're loading from the persistent archive.
		if( Ar.IsLoading() )
		{
			check(!bOnlySerializePayload);

			if( GUseSeekFreeLoading )
			{
				// Bulk data that is being serialized via seekfree loadingis single use only. This allows us 
				// to free the memory as e.g. the bulk data won't be attached to an archive in the case of
				// seek free loading.
				BulkDataFlags |= BULKDATA_SingleUse;
			}

			// Size on disk, which in the case of compression is != GetBulkDataSize()
			Ar << BulkDataSizeOnDisk;
			// Offset in file.
			Ar << BulkDataOffsetInFile;

			// Skip serialization of bulk data if it's stored in a separate file
			if( !(BulkDataFlags & BULKDATA_StoreInSeparateFile) )					
			{
				checkf( BulkDataOffsetInFile == Ar.Tell(), TEXT("Bad offset for %s"), *Owner->GetFullName() );

				// skip over data if caller requested we do so
				if (bSkipLoad)
				{
					Ar.Seek(Ar.Tell() + BulkDataSizeOnDisk);
					ElementCount = 0;
				}
				// We're allowing defered serialization.
				else if( Ar.IsAllowingLazyLoading() )
				{
					Ar.AttachBulkData( Owner, this );
					AttachedAr = &Ar;
					// Seek over the bulk data we skipped serializing.
					Ar.Seek( Ar.Tell() + BulkDataSizeOnDisk );
				}
				// Serialize the bulk data right away.
				else
				{
					// memory for bulk data can come from preallocated GPU-accessible resource memory or default to system memory
					BulkData = GetBulkDataResourceMemory(Owner,Idx);
					if( !BulkData )
					{
						BulkData = appRealloc( BulkData, GetBulkDataSize() );
					}
					SerializeBulkData( Ar, BulkData );
				}
			}
		}
		// We're saving to the persistent archive.
		else if( Ar.IsSaving() )
		{
			// Remove single element serialization requirement before saving out bulk data flags.
			BulkDataFlags &= ~BULKDATA_ForceSingleElementSerialization;

			// Store the bulk data in a separate file but the status information in this.
			if( BulkDataFlags & BULKDATA_StoreInSeparateFile )
			{
				check( !bOnlySerializePayload );

				// Seek back to the beginning and overwrite data.
				Ar.Seek( BulkDataFlagsPos );
				// Serialize only 'status' information.
				Ar << SavedBulkDataFlags;
				Ar << SavedElementCount;
				Ar << SavedBulkDataSizeOnDisk;
				Ar << SavedBulkDataOffsetInFile;
			}
			// Regular serialization.
			else
			{
				// Make sure bulk data is loaded.
				MakeSureBulkDataIsLoaded();

				// Keep track of last saved values.
				SavedBulkDataFlags	= BulkDataFlags;
				SavedElementCount	= ElementCount;

				// Only serialize status information if wanted.
				INT SavedBulkDataSizeOnDiskPos		= INDEX_NONE;
				INT SavedBulkDataOffsetInFilePos	= INDEX_NONE;
				if( !bOnlySerializePayload )
				{
					// Keep track of position we are going to serialize placeholder BulkDataSizeOnDisk.
					SavedBulkDataSizeOnDiskPos = Ar.Tell();
					SavedBulkDataSizeOnDisk = INDEX_NONE;
					// And serialize the placeholder which is going to be overwritten later.
					Ar << SavedBulkDataSizeOnDisk;

					// Keep track of position we are going to serialize placeholder BulkDataOffsetInFile.
					SavedBulkDataOffsetInFilePos = Ar.Tell();
					SavedBulkDataOffsetInFile = INDEX_NONE;
					// And serialize the placeholder which is going to be overwritten later.
					Ar << SavedBulkDataOffsetInFile;
				}

				// Keep track of bulk data start and end position so we can calculate the size on disk.
				INT SavedBulkDataStartPos = Ar.Tell();
				// Serialize bulk data.
				SerializeBulkData( Ar, BulkData );
				INT SavedBulkDataEndPos = Ar.Tell();

				// Calculate size of bulk data on disk and offset in file. The former might be different 
				// from GetBulkDataSize in the case of compression.
				SavedBulkDataSizeOnDisk		= SavedBulkDataEndPos - SavedBulkDataStartPos;
				SavedBulkDataOffsetInFile	= SavedBulkDataStartPos;

				// Only serialize status information if wanted.
				if( !bOnlySerializePayload )
				{
					// Seek back and overwrite placeholder for BulkDataSizeOnDisk
					Ar.Seek( SavedBulkDataSizeOnDiskPos );
					Ar << SavedBulkDataSizeOnDisk;

					// Seek back and overwrite placeholder for BulkDataOffsetInFile
					Ar.Seek( SavedBulkDataOffsetInFilePos );
					Ar << SavedBulkDataOffsetInFile;

					// Seek to the end of written data so we don't clobber any data in subsequent write 
					// operations
					Ar.Seek( SavedBulkDataEndPos );
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	Class specific virtuals.
-----------------------------------------------------------------------------*/

/**
 * Returns whether single element serialization is required given an archive. This e.g.
 * can be the case if the serialization for an element changes and the single element
 * serialization code handles backward compatibility.
 */
UBOOL FUntypedBulkData::RequiresSingleElementSerialization( FArchive& Ar )
{
	return FALSE;
}

/*-----------------------------------------------------------------------------
	Accessors for friend classes ULinkerLoad and content cookers.
-----------------------------------------------------------------------------*/

/**
 * Detaches the bulk data from the passed in archive. Needs to match the archive we are currently
 * attached to.
 *
 * @param Ar						Archive to detach from
 * @param bEnsureBulkDataIsLoaded	whether to ensure that bulk data is load before detaching from archive
 */
void FUntypedBulkData::DetachFromArchive( FArchive* Ar, UBOOL bEnsureBulkDataIsLoaded )
{
	check( Ar );
	check( Ar == AttachedAr );

	// Make sure bulk data is loaded.
	if( bEnsureBulkDataIsLoaded )
	{
		MakeSureBulkDataIsLoaded();
	}

	// Detach from archive.
	AttachedAr = NULL;
}

/**
 * Sets whether we should store only the status information in the file or serialize everything.
 *
 * @param bShouldStoreInSeparateFile	Whether to store in separate file or serialize everything
 * @param InSavedBulkDataFlags			if stored in separate file, status information for other file
 * @param InSavedElementCount			if stored in separate file, status information for other file
 * @param InSavedBUlkDataOffsetInFile	if stored in separate file, status information for other file
 * @param InSavedBulkDataSizeOnDisk		if stored in separate file, status information for other file
 */
void FUntypedBulkData::StoreInSeparateFile( 
	UBOOL bShouldStoreInSeparateFile, 
	INT InSavedBulkDataFlags, 
	INT InSavedElementCount, 
	INT	InSavedBulkDataOffsetInFile,
	INT InSavedBulkDataSizeOnDisk )
{
	// Set flag to store bulk data in separate file.
	if( bShouldStoreInSeparateFile )
	{
		BulkDataFlags |= BULKDATA_StoreInSeparateFile;
	}
	// Clear flag, allowing normal serialization.
	else
	{
		BulkDataFlags &= ~(BULKDATA_StoreInSeparateFile | BULKDATA_Unused);
	}
	
	// Propagate passed in variables to state used by code used when data is serialized in a separate file.
	SavedBulkDataFlags			= InSavedBulkDataFlags | (bShouldStoreInSeparateFile ? BULKDATA_StoreInSeparateFile : 0);
	SavedElementCount			= InSavedElementCount;
	SavedBulkDataOffsetInFile	= InSavedBulkDataOffsetInFile;
	SavedBulkDataSizeOnDisk		= InSavedBulkDataSizeOnDisk;
}

/**
 * Sets whether we should store the data compressed on disk.
 *
 * @param CompressionFlags	Flags to use for compressing the data. Use COMPRESS_NONE for no compression, or something like COMPRESS_LZO to compress the data
 */
void FUntypedBulkData::StoreCompressedOnDisk( ECompressionFlags CompressionFlags )
{
	if( CompressionFlags == COMPRESS_None )
	{
		// clear all compression settings
		BulkDataFlags &= ~BULKDATA_SerializeCompressed;
	}
	else
	{
		// make sure a valid compression format was specified
		check(CompressionFlags & (COMPRESS_ZLIB | COMPRESS_LZO | COMPRESS_LZX));
		BulkDataFlags |=	(CompressionFlags & COMPRESS_ZLIB) ? BULKDATA_SerializeCompressedZLIB : 
							(CompressionFlags & COMPRESS_LZX) ? BULKDATA_SerializeCompressedLZX :
							(CompressionFlags & COMPRESS_LZO) ? BULKDATA_SerializeCompressedLZO :
							 BULKDATA_None;
	}
}


/*-----------------------------------------------------------------------------
	Internal helpers
-----------------------------------------------------------------------------*/

/**
 * Copies bulk data from passed in structure.
 *
 * @param	Other	Bulk data object to copy from.
 */
void FUntypedBulkData::Copy( const FUntypedBulkData& Other )
{
	// Only copy if there is something to copy.
	if( Other.GetElementCount() )
	{
		// Make sure src is loaded without calling Lock as the object is const.
		check(Other.BulkData);
		check(BulkData);
		check(ElementCount == Other.GetElementCount() );
		// Copy from src to dest.
		appMemcpy( BulkData, Other.BulkData, Other.GetBulkDataSize() );
	}
}

/**
 * Helper function initializing all member variables.
 */
void FUntypedBulkData::InitializeMemberVariables()
{
	BulkDataFlags				= BULKDATA_None;
	ElementCount				= 0;
	BulkDataOffsetInFile		= INDEX_NONE;
	BulkDataSizeOnDisk			= INDEX_NONE;
	SavedBulkDataFlags			= BULKDATA_None;
	SavedElementCount			= INDEX_NONE;
	SavedBulkDataOffsetInFile	= INDEX_NONE;
	SavedBulkDataSizeOnDisk		= INDEX_NONE;
	BulkData					= NULL;
	LockStatus					= LOCKSTATUS_Unlocked;
	AttachedAr					= NULL;
	bShouldFreeOnEmpty			= TRUE;
}

/**
 * Serialize just the bulk data portion to/ from the passed in memory.
 *
 * @param	Ar					Archive to serialize with
 * @param	Data				Memory to serialize either to or from
 */
void FUntypedBulkData::SerializeBulkData( FArchive& Ar, void* Data )
{
	// skip serializing of unused data
	if( BulkDataFlags & BULKDATA_Unused )
	{
		return;
	}

	// Allow backward compatible serialization by forcing bulk serialization off if required. Saving also always uses single
	// element serialization so errors or oversight when changing serialization code is recoverable.
	UBOOL bSerializeInBulk = TRUE;
	if( RequiresSingleElementSerialization( Ar ) 
	// Set when serialized like a lazy array.
	|| (BulkDataFlags & BULKDATA_ForceSingleElementSerialization) 
	// We use bulk serialization even when saving 1 byte types (texture & sound bulk data) as an optimization for those.
	|| (Ar.IsSaving() && (GetElementSize() > 1) ) )
	{
		bSerializeInBulk = FALSE;
	}

	// Raw serialize the bulk data without any possiblity for potential endian conversion.
	if( bSerializeInBulk )
	{
		// Serialize data compressed.
		if( BulkDataFlags & BULKDATA_SerializeCompressed )
		{
			Ar.SerializeCompressed( Data, GetBulkDataSize(), GetDecompressionFlags());
		}
		// Uncompressed/ regular serialization.
		else
		{
			Ar.Serialize( Data, GetBulkDataSize() );
		}
	}
	// Serialize an element at a time via the virtual SerializeElement function potentialy allowing and dealing with 
	// endian conversion. Dealing with compression makes this a bit more complex as SerializeCompressed expects the 
	// full data to be compresed en block and not piecewise.
	else
	{
		// Serialize data compressed.
		if( BulkDataFlags & BULKDATA_SerializeCompressed )
		{
			// Placeholder for to be serialized data.
			TArray<BYTE> SerializedData;
			
			// Loading, data is compressed in archive and needs to be decompressed.
			if( Ar.IsLoading() )
			{
				// Create space for uncompressed data.
				SerializedData.Empty( GetBulkDataSize() );
				SerializedData.Add( GetBulkDataSize() );

				// Serialize data with passed in archive and compress.
				Ar.SerializeCompressed( SerializedData.GetData(), SerializedData.Num(), GetDecompressionFlags());
				
				// Initialize memory reader with uncompressed data array and propagate forced byte swapping
				FMemoryReader MemoryReader( SerializedData, TRUE );
				MemoryReader.SetByteSwapping( Ar.ForceByteSwapping() );

				// Serialize each element individually via memory reader.				
				for( INT ElementIndex=0; ElementIndex<ElementCount; ElementIndex++ )
				{
					SerializeElement( MemoryReader, Data, ElementIndex );
				}
			}
			// Saving, data is uncompressed in memory and needs to be compressed.
			else if( Ar.IsSaving() )
			{			
				// Initialize memory writer with blank data array and propagate forced byte swapping
				FMemoryWriter MemoryWriter( SerializedData, TRUE );
				MemoryWriter.SetByteSwapping( Ar.ForceByteSwapping() );

				// Serialize each element individually via memory writer.				
				for( INT ElementIndex=0; ElementIndex<ElementCount; ElementIndex++ )
				{
					SerializeElement( MemoryWriter, Data, ElementIndex );
				}

				// Serialize data with passed in archive and compress.
				Ar.SerializeCompressed( SerializedData.GetData(), SerializedData.Num(), GetDecompressionFlags() );
			}
		}
		// Uncompressed/ regular serialization.
		else
		{
			// We can use the passed in archive if we're not compressing the data.
			for( INT ElementIndex=0; ElementIndex<ElementCount; ElementIndex++ )
			{
				SerializeElement( Ar, Data, ElementIndex );
			}
		}
	}
}

/**
 * Loads the bulk data if it is not already loaded.
 */
void FUntypedBulkData::MakeSureBulkDataIsLoaded()
{
	// Nothing to do if data is already loaded.
	if( !BulkData )
	{
		// Allocate memory for bulk data.
		BulkData = appMalloc( GetBulkDataSize() );

		// Only load if there is something to load. E.g. we might have just created the bulk data array
		// in which case it starts out with a size of zero.
		if( GetBulkDataSize() > 0 )
		{
			LoadDataIntoMemory( BulkData );
		}
	}
}

/**
 * Loads the data from disk into the specified memory block. This requires us still being attached to an
 * archive we can use for serialization.
 *
 * @param Dest Memory to serialize data into
 */
void FUntypedBulkData::LoadDataIntoMemory( void* Dest )
{
	checkf( AttachedAr, TEXT( "Attempted to load bulk data without an attached archive. Most likely the bulk data was loaded twice on console, which is not supported" ) );

	// Keep track of current position in file so we can restore it later.
	INT PushedPos = AttachedAr->Tell();
	// Seek to the beginning of the bulk data in the file.
	AttachedAr->Seek( BulkDataOffsetInFile );
		
	SerializeBulkData( *AttachedAr, Dest );

	// Restore file pointer.
	AttachedAr->Seek( PushedPos );
}



/*-----------------------------------------------------------------------------
	BYTE version of bulk data.
-----------------------------------------------------------------------------*/

/**
 * Returns size in bytes of single element.
 *
 * @return Size in bytes of single element
 */
INT FByteBulkData::GetElementSize() const
{
	return sizeof(BYTE);
}

/**
 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
 * 
 * @param Ar			Archive to serialize with
 * @param Data			Base pointer to data
 * @param ElementIndex	Element index to serialize
 */
void FByteBulkData::SerializeElement( FArchive& Ar, void* Data, INT ElementIndex )
{
	BYTE& ByteData = *((BYTE*)Data + ElementIndex);
	Ar << ByteData;
}

/*-----------------------------------------------------------------------------
	WORD version of bulk data.
-----------------------------------------------------------------------------*/

/**
 * Returns size in bytes of single element.
 *
 * @return Size in bytes of single element
 */
INT FWordBulkData::GetElementSize() const
{
	return sizeof(WORD);
}

/**
 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
 * 
 * @param Ar			Archive to serialize with
 * @param Data			Base pointer to data
 * @param ElementIndex	Element index to serialize
 */
void FWordBulkData::SerializeElement( FArchive& Ar, void* Data, INT ElementIndex )
{
	WORD& WordData = *((WORD*)Data + ElementIndex);
	Ar << WordData;
}

/*-----------------------------------------------------------------------------
	INT version of bulk data.
-----------------------------------------------------------------------------*/

/**
 * Returns size in bytes of single element.
 *
 * @return Size in bytes of single element
 */
INT FIntBulkData::GetElementSize() const
{
	return sizeof(INT);
}

/**
 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
 * 
 * @param Ar			Archive to serialize with
 * @param Data			Base pointer to data
 * @param ElementIndex	Element index to serialize
 */
void FIntBulkData::SerializeElement( FArchive& Ar, void* Data, INT ElementIndex )
{
	INT& IntData = *((INT*)Data + ElementIndex);
	Ar << IntData;
}

/*-----------------------------------------------------------------------------
	FLOAT version of bulk data.
-----------------------------------------------------------------------------*/

/**
 * Returns size in bytes of single element.
 *
 * @return Size in bytes of single element
 */
INT FFloatBulkData::GetElementSize() const
{
	return sizeof(FLOAT);
}

/**
 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
 * 
 * @param Ar			Archive to serialize with
 * @param Data			Base pointer to data
 * @param ElementIndex	Element index to serialize
 */
void FFloatBulkData::SerializeElement( FArchive& Ar, void* Data, INT ElementIndex )
{
	FLOAT& FloatData = *((FLOAT*)Data + ElementIndex);
	Ar << FloatData;
}
























