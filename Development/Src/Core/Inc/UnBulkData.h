/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _UNBULKDATA_H
#define _UNBULKDATA_H

/**
 * Flags serialized with the bulk data.
 */
enum EBulkDataFlags
{
	/** Empty flag set.																*/
	BULKDATA_None								= 0,
	/** If set, payload is [going to be] stored in separate file.					*/
	BULKDATA_StoreInSeparateFile				= 1<<0,
	/** If set, payload should be [un]compressed using ZLIB during serialization.	*/
	BULKDATA_SerializeCompressedZLIB			= 1<<1,
	/** Force usage of SerializeElement over bulk serialization.					*/
	BULKDATA_ForceSingleElementSerialization	= 1<<2,
	/** Bulk data is only used once at runtime in the game.							*/
	BULKDATA_SingleUse							= 1<<3,
	/** If set, payload should be [un]compressed using LZO during serialization.	*/
	BULKDATA_SerializeCompressedLZO				= 1<<4,
	/** Bulk data won't be used and doesn't need to be loaded						*/
	BULKDATA_Unused								= 1<<5,
	/** If specified, only payload data will be written to archive.					*/
	BULKDATA_StoreOnlyPayload					= 1<<6,
	/** If set, payload should be [un]compressed using LZX during serialization.	*/
	BULKDATA_SerializeCompressedLZX				= 1<<7,
	/** Flag to check if either compression mode is specified						*/
	BULKDATA_SerializeCompressed				= (BULKDATA_SerializeCompressedZLIB | BULKDATA_SerializeCompressedLZO | BULKDATA_SerializeCompressedLZX),

};

/**
 * Enumeration for bulk data lock status.
 */
enum EBulkDataLockStatus
{
	/** Unlocked array													*/
	LOCKSTATUS_Unlocked							= 0,
	/** Locked read-only												*/
	LOCKSTATUS_ReadOnlyLock						= 1,
	/** Locked read-write-realloc										*/
	LOCKSTATUS_ReadWriteLock					= 2,
};

/**
 * Enumeration for bulk data lock behavior
 */
enum EBulkDataLockFlags
{
	LOCK_READ_ONLY								= 1,
	LOCK_READ_WRITE								= 2,
};

/*-----------------------------------------------------------------------------
	Base version of untyped bulk data.
-----------------------------------------------------------------------------*/

/**
 * @documentation @todo documentation
 */
struct FUntypedBulkData
{
	friend class ULinkerLoad;
	friend class UPersistentCookerData;
	friend class UCookPackagesCommandlet;

	/*-----------------------------------------------------------------------------
		Constructors and operators
	-----------------------------------------------------------------------------*/

	/**
	 * Constructor, initializing all member variables.
	 */
	FUntypedBulkData();

	/**
	 * Copy constructor. Use the common routine to perform the copy.
	 *
	 * @param Other the source array to copy
	 */
	FUntypedBulkData( const FUntypedBulkData& Other );

	/**
	 * Virtual destructor, free'ing allocated memory.
	 */
	virtual ~FUntypedBulkData();

	/**
	* Get resource memory preallocated for serializing bulk data into
	* This is typically GPU accessible memory to avoid multiple allocations copies from system memory
	* If NULL is returned then default to allocating from system memory
	*
	* @param Owner	object with bulk data being serialized
	* @param Idx	entry when serializing out of an array
	* @return pointer to resource memory or NULL
	*/
	virtual void* GetBulkDataResourceMemory(UObject* Owner,INT Idx) 
	{ 
		return NULL; 
	}

	/**
	 * Copies the source array into this one after detaching from archive.
	 *
	 * @param Other the source array to copy
	 */
	FUntypedBulkData& operator=( const FUntypedBulkData& Other );

	/*-----------------------------------------------------------------------------
		Static functions.
	-----------------------------------------------------------------------------*/

	/**
	 * Dumps detailed information of bulk data usage.
	 *
	 * @param Log FOutputDevice to use for logging
	 */
	static void DumpBulkDataUsage( FOutputDevice& Log );

	/*-----------------------------------------------------------------------------
		Accessors
	-----------------------------------------------------------------------------*/

	/**
	 * Returns the number of elements in this bulk data array.
	 *
	 * @return Number of elements in this bulk data array
	 */
	INT GetElementCount() const;
	/**
	 * Returns size in bytes of single element.
	 *
	 * Pure virtual that needs to be overloaded in derived classes.
	 *
	 * @return Size in bytes of single element
	 */
	virtual INT GetElementSize() const = 0;
	/**
	 * Returns the size of the bulk data in bytes.
	 *
	 * @return Size of the bulk data in bytes
	 */
	INT GetBulkDataSize() const;
	/**
	 * Returns the size of the bulk data on disk. This can differ from GetBulkDataSize if
	 * BULKDATA_SerializeCompressed is set.
	 *
	 * @return Size of the bulk data on disk or INDEX_NONE in case there's no association
	 */
	INT GetBulkDataSizeOnDisk() const;
	/**
	 * Returns the offset into the file the bulk data is located at.
	 *
	 * @return Offset into the file or INDEX_NONE in case there is no association
	 */
	INT GetBulkDataOffsetInFile() const;
	/**
	 * Returns whether the bulk data is stored compressed on disk.
	 *
	 * @return TRUE if data is compressed on disk, FALSE otherwise
	 */
	UBOOL IsStoredCompressedOnDisk() const;

	/**
	 * Returns flags usable to decompress the bulk data
	 * 
	 * @return COMPRESS_NONE if the data was not compressed on disk, or valid flags to pass to appUncompressMemory for this data
	 */
	ECompressionFlags GetDecompressionFlags() const;

	/**
	 * Returns whether the bulk data is stored in a separate file and henceforth cannot
	 * be "locked"/ read.
	 *
	 * @return TRUE if data is stored in separate file, FALSE otherwise
	 */
	UBOOL IsStoredInSeparateFile() const;
	/**
	 * Returns whether the bulk data is currently loaded and resident in memory.
	 *
	 * @return TRUE if bulk data is loaded, FALSE otherwise
	 */
	UBOOL IsBulkDataLoaded() const;

	/**
	* Returns whether this bulk data is used
	* @return TRUE if BULKDATA_Unused is not set
	*/
	UBOOL IsAvailableForUse() const;

	/**
	 * Sets the passed in bulk data flags.
	 *
	 * @param BulkDataFlagsToSet	Bulk data flags to set
	 */
	void SetBulkDataFlags( DWORD BulkDataFlagsToSet );

	/**
	* Gets the current bulk data flags.
	*
	* @return Bulk data flags currently set
	*/
	DWORD GetBulkDataFlags() const;

	/**
	 * Clears the passed in bulk data flags.
	 *
	 * @param BulkDataFlagsToClear	Bulk data flags to clear
	 */
	void ClearBulkDataFlags( DWORD BulkDataFlagsToClear );

	/**
	 * BulkData memory allocated from a resource should only be freed by the resource
	 *
	 * @return TRUE if bulk data should free allocated memory
	 */
	UBOOL ShouldFreeOnEmpty() const;

#if WITH_SUBSTANCE_AIR
	void* GetBulkData()
	{
		return BulkData;
	}
#endif

private:
	/**
	 * Returns the last saved bulk data flags.
	 *
	 * @param Last saved bulk data flags.
	 */
	DWORD GetSavedBulkDataFlags() const;
	/**
	 * Returns the last saved number of elements in this bulk data array.
	 *
	 * @return Last saved number of elements in this bulk data array
	 */	
	INT GetSavedElementCount() const;
	/**
	 * Returns the last saved offset into the file the bulk data is located at.
	 *
	 * @return Last saved Offset into the file or INDEX_NONE in case there is no association
	 */
	INT GetSavedBulkDataOffsetInFile() const;
	/**
	 * Sets last saved offset into the file the bulk data is located at.
	 *
	 * @param NewOffset offset to save
	 */
	void SetSavedBulkDataOffsetInFile(INT NewOffset)
	{
		SavedBulkDataOffsetInFile = NewOffset;
	}
	/**
	 * Returns the last saved size of the bulk data on disk. This can differ from GetBulkDataSize 
	 * if BULKDATA_SerializeCompressed is set.
	 *
	 * @return Size of the last saved bulk data on disk or INDEX_NONE in case there's no association
	 */
	INT GetSavedBulkDataSizeOnDisk() const;
	/**
	 * Sets last saved compressed size of the bulk data.
	 *
	 * @param NewSize size to save
	 */
	void SetSavedBulkDataSizeOnDisk(INT NewSize)
	{
		SavedBulkDataSizeOnDisk = NewSize;
	}
public:

	/*-----------------------------------------------------------------------------
		Data retrieval and manipulation.
	-----------------------------------------------------------------------------*/

	/**
	 * Retrieves a copy of the bulk data.
	 *
	 * @param Dest [in/out] Pointer to pointer going to hold copy, can point to NULL pointer in which case memory is allocated
	 * @param bDiscardInternalCopy Whether to discard/ free the potentially internally allocated copy of the data
	 */
	void GetCopy( void** Dest, UBOOL bDiscardInternalCopy = TRUE );

	/**
	 * Locks the bulk data and returns a pointer to it.
	 *
	 * @param	LockFlags	Flags determining lock behavior
	 */
	void* Lock( DWORD LockFlags );

	/**
	 * Change size of locked bulk data. Only valid if locked via read-write lock.
	 *
	 * @param InElementCount	Number of elements array should be resized to
	 */
	void* Realloc( INT InElementCount );

	/** 
	 * Unlocks bulk data after which point the pointer returned by Lock no longer is valid.
	 */
	void Unlock();

	/**
	 * Clears/ removes the bulk data and resets element count to 0.
	 */
	void RemoveBulkData();

	/**
	 * Forces the bulk data to be resident in memory and detaches the archive.
	 */
	void ForceBulkDataResident();


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
	void Serialize(FArchive& Ar, UObject* Owner, INT Idx = INDEX_NONE, UBOOL bSkipLoad = FALSE);

	/**
	 * Serialize (loading only) bulk data that was original saved via the TLazyArray.
	 *
	 * @deprecate with VER_REPLACED_LAZY_ARRAY_WITH_UNTYPED_BULK_DATA
	 *
	 * @param Ar	Archive to serialize with
	 * @param Owner	Object owning the bulk data
	 */
	void SerializeLikeLazyArray( FArchive &Ar, UObject* Owner );

	/*-----------------------------------------------------------------------------
		Class specific virtuals.
	-----------------------------------------------------------------------------*/

protected:
	/**
	 * Serializes a single element at a time, allowing backward compatible serialization
	 * and endian swapping to be performed. Needs to be overloaded by derived classes.
	 *
	 * @param Ar			Archive to serialize with
	 * @param Data			Base pointer to data
	 * @param ElementIndex	Index of element to serialize
	 */
	virtual void SerializeElement( FArchive& Ar, void* Data, INT ElementIndex ) = 0;

	/**
	 * Returns whether single element serialization is required given an archive. This e.g.
	 * can be the case if the serialization for an element changes and the single element
	 * serialization code handles backward compatibility.
	 */
	virtual UBOOL RequiresSingleElementSerialization( FArchive& Ar );

private:
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
	void DetachFromArchive( FArchive* Ar, UBOOL bEnsureBulkDataIsLoaded );

	/**
	 * Sets whether we should store only the status information in the file or serialize everything.
	 *
	 * @param bShouldStoreInSeparateFile	Whether to store in separate file or serialize everything
	 * @param InSavedBulkDataFlags			if stored in separate file, status information for other file
	 * @param InSavedElementCount			if stored in separate file, status information for other file
	 * @param InSavedBUlkDataOffsetInFile	if stored in separate file, status information for other file
	 * @param InSavedBulkDataSizeOnDisk		if stored in separate file, status information for other file
	 */
	void StoreInSeparateFile( 
		UBOOL bShouldStoreInSeparateFile, 
		INT InSavedBulkDataFlags		= 0, 
		INT InSavedElementCount			= INDEX_NONE, 
		INT	InSavedBulkDataOffsetInFile	= INDEX_NONE,
		INT InSavedBulkDataSizeOnDisk	= INDEX_NONE );

	/**
	 * Sets whether we should store the data compressed on disk.
	 *
	 * @param CompressionFlags	Flags to use for compressing the data. Use COMPRESS_NONE for no compression, or something like COMPRESS_LZO to compress the data
	 */
	void StoreCompressedOnDisk( ECompressionFlags CompressionFlags );

	/*-----------------------------------------------------------------------------
		Internal helpers
	-----------------------------------------------------------------------------*/

	/**
	 * Copies bulk data from passed in structure.
	 *
	 * @param	Other	Bulk data object to copy from.
	 */
	void Copy( const FUntypedBulkData& Other );

	/**
	 * Helper function initializing all member variables.
	 */
	void InitializeMemberVariables();

	/**
	 * Serialize just the bulk data portion to/ from the passed in memory.
	 *
	 * @param	Ar					Archive to serialize with
	 * @param	Data				Memory to serialize either to or from
	 */
	void SerializeBulkData( FArchive& Ar, void* Data );

	/**
	 * Loads the bulk data if it is not already loaded.
	 */
	void MakeSureBulkDataIsLoaded();

	/**
	 * Loads the data from disk into the specified memory block. This requires us still being attached to an
	 * archive we can use for serialization.
	 *
	 * @param Dest Memory to serialize data into
	 */
	void LoadDataIntoMemory( void* Dest );

	/*-----------------------------------------------------------------------------
		Member variables.
	-----------------------------------------------------------------------------*/

	/** Serialized flags for bulk data																					*/
	DWORD				BulkDataFlags;
	/** Number of elements in bulk data array																			*/
	INT					ElementCount;
	/** Offset of bulk data into file or INDEX_NONE if no association													*/
	INT					BulkDataOffsetInFile;
	/** Size of bulk data on disk or INDEX_NONE if no association														*/
	INT					BulkDataSizeOnDisk;

	/** From last saving or StoreInSeparateFile call: Serialized flags for bulk data									*/
	DWORD				SavedBulkDataFlags;
	/** From last saving or StoreInSeparateFile call: Number of elements in bulk data array								*/
	INT					SavedElementCount;
	/** From last saving or StoreInSeparateFile call: Offset of bulk data into file or INDEX_NONE if no association		*/
	INT					SavedBulkDataOffsetInFile;
	/** From last saving or StoreInSeparateFile call: Size of bulk data on disk or INDEX_NONE if no association			*/
	INT					SavedBulkDataSizeOnDisk;

	/** Pointer to cached bulk data																						*/
	void*				BulkData;
	/** Current lock status																								*/
	DWORD				LockStatus;
	/** Archive associated with bulk data for serialization																*/
	FArchive*			AttachedAr;

protected:
	/** TRUE when data has been allocated internally by the bulk data and does not come from a preallocated resource	*/
	UBOOL				bShouldFreeOnEmpty;
};


/*-----------------------------------------------------------------------------
	BYTE version of bulk data.
-----------------------------------------------------------------------------*/

struct FByteBulkData : public FUntypedBulkData
{
	/**
	 * Returns size in bytes of single element.
	 *
	 * @return Size in bytes of single element
	 */
	virtual INT GetElementSize() const;

	/**
	 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
	 * 
	 * @param Ar			Archive to serialize with
	 * @param Data			Base pointer to data
	 * @param ElementIndex	Element index to serialize
	 */
	virtual void SerializeElement( FArchive& Ar, void* Data, INT ElementIndex );
};

/*-----------------------------------------------------------------------------
	BYTE version of bulk data used for texture mips
-----------------------------------------------------------------------------*/

/** 
* Bulk data used to load byte data for texture mips
*/
struct FTextureMipBulkData : public FByteBulkData
{
	/**
	* Get resource memory preallocated for serializing bulk data into
	* This is typically GPU accessible memory to avoid multiple allocations copies from system memory
	* If NULL is returned then default to allocating from system memory
	*
	* @param Owner	object with bulk data being serialized
	* @param Idx	entry when serializing out of an array
	* @return pointer to resource memory or NULL
	*/
	virtual void* GetBulkDataResourceMemory(UObject* Owner,INT Idx);
};


/*-----------------------------------------------------------------------------
	WORD version of bulk data.
-----------------------------------------------------------------------------*/

struct FWordBulkData : public FUntypedBulkData
{
	/**
	 * Returns size in bytes of single element.
	 *
	 * @return Size in bytes of single element
	 */
	virtual INT GetElementSize() const;

	/**
	 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
	 * 
	 * @param Ar			Archive to serialize with
	 * @param Data			Base pointer to data
	 * @param ElementIndex	Element index to serialize
	 */
	virtual void SerializeElement( FArchive& Ar, void* Data, INT ElementIndex );
};

/*-----------------------------------------------------------------------------
	INT version of bulk data.
-----------------------------------------------------------------------------*/

struct FIntBulkData : public FUntypedBulkData
{
	/**
	 * Returns size in bytes of single element.
	 *
	 * @return Size in bytes of single element
	 */
	virtual INT GetElementSize() const;

	/**
	 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
	 * 
	 * @param Ar			Archive to serialize with
	 * @param Data			Base pointer to data
	 * @param ElementIndex	Element index to serialize
	 */
	virtual void SerializeElement( FArchive& Ar, void* Data, INT ElementIndex );
};

/*-----------------------------------------------------------------------------
	FLOAT version of bulk data.
-----------------------------------------------------------------------------*/

struct FFloatBulkData : public FUntypedBulkData
{
	/**
	 * Returns size in bytes of single element.
	 *
	 * @return Size in bytes of single element
	 */
	virtual INT GetElementSize() const;

	/**
	 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
	 * 
	 * @param Ar			Archive to serialize with
	 * @param Data			Base pointer to data
	 * @param ElementIndex	Element index to serialize
	 */
	virtual void SerializeElement( FArchive& Ar, void* Data, INT ElementIndex );
};


#endif


