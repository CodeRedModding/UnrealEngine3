/*=============================================================================
	UnArc.h: Unreal archive class.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Forward declarations.
-----------------------------------------------------------------------------*/

struct FUntypedBulkData;

/*-----------------------------------------------------------------------------
	Archive.
-----------------------------------------------------------------------------*/

/**
 * Define this to 1 if you are willing to sacrifice the performance of
 * (one branch plus one virtual call) in order to allow PC clients to
 * connect to a Xbox server.
 */
#define WANTS_XBOX_BYTE_SWAPPING 0

#if NO_BYTE_ORDER_SERIALIZE && !WANTS_XBOX_BYTE_SWAPPING
// The data cooker takes care of endian conversion on consoles.
#define ByteOrderSerialize	Serialize
#endif

//
// Archive class. Used for loading, saving, and garbage collecting
// in a byte order neutral way.
//
class FArchive
{
public:
	// FArchive interface.
	virtual ~FArchive()
	{}
	virtual void Serialize( void* V, INT Length )
	{}
	virtual void SerializeBits( void* V, INT LengthBits )
	{
		Serialize( V, (LengthBits+7)/8 );
		if( IsLoading() )
		{
			((BYTE*)V)[LengthBits/8] &= ((1<<(LengthBits&7))-1);
		}
	}
	virtual void SerializeInt( DWORD& Value, DWORD Max )
	{
		ByteOrderSerialize( &Value, sizeof(Value) );
	}
	virtual void Preload( UObject* Object )
	{}
	virtual void CountBytes( SIZE_T InNum, SIZE_T InMax )
	{}
	virtual FArchive& operator<<( class FName& N )
	{
		return *this;
	}
	virtual FArchive& operator<<( class UObject*& Res )
	{
		return *this;
	}

	/**
  	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const;

	/**
	 * If this archive is a ULinkerLoad or ULinkerSave, returns a pointer to the ULinker portion.
	 */
	virtual class ULinker* GetLinker() { return NULL; }

	virtual INT Tell()
	{
		return INDEX_NONE;
	}
	virtual INT TotalSize()
	{
		return INDEX_NONE;
	}
	virtual UBOOL AtEnd()
	{
		INT Pos = Tell();
		return Pos!=INDEX_NONE && Pos>=TotalSize();
	}
	virtual void Seek( INT InPos )
	{}
	/**
	 * Attaches/ associates the passed in bulk data object with the linker.
	 *
	 * @param	Owner		UObject owning the bulk data
	 * @param	BulkData	Bulk data object to associate
	 */
	virtual void AttachBulkData( UObject* Owner, FUntypedBulkData* BulkData )
	{}
	/**
	 * Detaches the passed in bulk data object from the linker.
	 *
	 * @param	BulkData	Bulk data object to detach
	 * @param	bEnsureBulkDataIsLoaded	Whether to ensure that the bulk data is loaded before detaching
	 */
	virtual void DetachBulkData( FUntypedBulkData* BulkData, UBOOL bEnsureBulkDataIsLoaded )
	{}
	/**
	 * Hint the archive that the region starting at passed in offset and spanning the passed in size
	 * is going to be read soon and should be precached.
	 *
	 * The function returns whether the precache operation has completed or not which is an important
	 * hint for code knowing that it deals with potential async I/O. The archive is free to either not 
	 * implement this function or only partially precache so it is required that given sufficient time
	 * the function will return TRUE. Archives not based on async I/O should always return TRUE.
	 *
	 * This function will not change the current archive position.
	 *
	 * @param	PrecacheOffset	Offset at which to begin precaching.
	 * @param	PrecacheSize	Number of bytes to precache
	 * @return	FALSE if precache operation is still pending, TRUE otherwise
	 */
	virtual UBOOL Precache( INT PrecacheOffset, INT PrecacheSize )
	{
		return TRUE;
	}
	/**
	 * Flushes cache and frees internal data.
	 */
	virtual void FlushCache()
	{}
	/**
	 * Sets mapping from offsets/ sizes that are going to be used for seeking and serialization to what
	 * is actually stored on disk. If the archive supports dealing with compression in this way it is 
	 * going to return TRUE.
	 *
	 * @param	CompressedChunks	Pointer to array containing information about [un]compressed chunks
	 * @param	CompressionFlags	Flags determining compression format associated with mapping
	 *
	 * @return TRUE if archive supports translating offsets & uncompressing on read, FALSE otherwise
	 */
	virtual UBOOL SetCompressionMap( TArray<struct FCompressedChunk>* CompressedChunks, ECompressionFlags CompressionFlags )
	{
		return FALSE;
	}
	virtual void Flush()
	{}
	virtual UBOOL Close()
	{
		return !ArIsError;
	}
	virtual UBOOL GetError()
	{
		return ArIsError;
	}

	/**
	 * Serializes and compresses/ uncompresses data. This is a shared helper function for compression
	 * support. The data is saved in a way compatible with FIOSystem::LoadCompressedData.
	 *
	 * @param	V		Data pointer to serialize data from/ to
	 * @param	Length	Length of source data if we're saving, unused otherwise
	 * @param	Flags	Flags to control what method to use for [de]compression and optionally control memory vs speed when compressing
	 * @param	bTreatBufferAsFileReader TRUE if V is actually an FArchive, which is used when saving to read data - helps to avoid single huge allocations of source data
	 */
	void SerializeCompressed( void* V, INT Length, ECompressionFlags Flags, UBOOL bTreatBufferAsFileReader=FALSE );

#if !NO_BYTE_ORDER_SERIALIZE || WANTS_XBOX_BYTE_SWAPPING
	// Hardcoded datatype routines that may not be overridden.
	FArchive& ByteOrderSerialize( void* V, INT Length )
	{
#if __INTEL_BYTE_ORDER__ || WANTS_XBOX_BYTE_SWAPPING
		UBOOL SwapBytes = ArForceByteSwapping;
#else
		UBOOL SwapBytes = ArIsPersistent;
#endif
		if( SwapBytes )
		{
			// Transferring between memory and file, so flip the byte order.
			for( INT i=Length-1; i>=0; i-- )
				Serialize( (BYTE*)V + i, 1 );
		}
		else
		{
			// Transferring around within memory, so keep the byte order.
			Serialize( V, Length );
		}
		return *this;
	}
#endif

	void ThisContainsCode()	{ArContainsCode=true;}	// Sets that this package contains code

	/**
	 * Indicate that this archive contains a ULevel or UWorld object.
	 */
	void ThisContainsMap() 
	{
		ArContainsMap=true;
	}

	/**
	 * Indicates that this archive contains cooked data.
	 */
	void ThisContainsCookedData()
	{
		ArContainsCookedData=TRUE;
	}

	/**
	 * Indicate that this archive is currently serializing class/struct defaults
	 */
	void StartSerializingDefaults() 
	{
		ArSerializingDefaults++;
	}

	/**
	 * Indicate that this archive is no longer serializing class/struct defaults
	 */
	void StopSerializingDefaults() 
	{
		ArSerializingDefaults--;
	}

	/**
	 * Called when an object begins serializing property data using script serialization.
	 */
	virtual void MarkScriptSerializationStart( const UObject* Obj ) {}

	/**
	 * Called when an object stops serializing property data using script serialization.
	 */
	virtual void MarkScriptSerializationEnd( const UObject* Obj ) {}

	/**
	 * Whether to allow serialization code to potentially remove references. Note that this
	 * is increasing/ decreasing an internal counter and hence supporting nesting so elimination
	 * might not be enabled after calling it. 
	 *
	 * @param	Allow Whether to increase or decrease counter determining whether to allow elimination
	 * @return	TRUE if reference elimination is enabled, FALSE otherwise
	 */
	UBOOL AllowEliminatingReferences( UBOOL Allow )
	{
		ArAllowEliminatingReferences += Allow ? 1 : -1;
		return IsAllowingReferenceElimination();
	}

	/**
	 * Let the archive handle an object that could be a cross level reference. By default, do nothing.
	 *
	 * @param PropertyOwner The object that owns the property/pointer that is being serialized, not the destination object
	 * @param Property The property that describes the data being serialized
	 */
	virtual void WillSerializePotentialCrossLevelPointer(UObject* PropertyOwner, const UProperty* Property) {}

	// Logf implementation for convenience.
	VARARG_DECL( void, void, {}, Logf, VARARG_NONE, const TCHAR*, VARARG_NONE, VARARG_NONE );

	// Constructor.
	FArchive()
	{
		Reset();
	}

	// Status accessors.
	FORCEINLINE INT Ver()									const	{return ArVer;}
	FORCEINLINE INT NetVer()								const	{return ArNetVer&0x7fffffff;}
	FORCEINLINE INT LicenseeVer()							const	{return ArLicenseeVer;}
	FORCEINLINE UBOOL IsLoading()							const	{return ArIsLoading;}
	FORCEINLINE UBOOL IsSaving()							const	{return ArIsSaving;}
	FORCEINLINE UBOOL IsSaveGame()							const	{return ArIsSaveGame;}
#if !CONSOLE
	FORCEINLINE UBOOL IsTransacting()						const	{return ArIsTransacting;}
#else
	FORCEINLINE UBOOL IsTransacting()						const	{return FALSE;}
#endif
	FORCEINLINE UBOOL WantBinaryPropertySerialization()		const	{return ArWantBinaryPropertySerialization;}
	FORCEINLINE UBOOL IsForcingUnicode()					const	{return ArForceUnicode;}
	FORCEINLINE UBOOL IsNet()								const	{return (ArNetVer&0x80000000)!=0;}
	FORCEINLINE UBOOL IsPersistent()						const	{return ArIsPersistent;}
	FORCEINLINE UBOOL IsError()								const	{return ArIsError;}
	FORCEINLINE UBOOL IsCriticalError()						const	{return ArIsCriticalError;}
	FORCEINLINE UBOOL ContainsCookedData()					const	{return ArContainsCookedData;}
	FORCEINLINE UBOOL ForEdit()								const	{return ArForEdit;}
	FORCEINLINE UBOOL ForClient()							const	{return ArForClient;}
	FORCEINLINE UBOOL ForServer()							const	{return ArForServer;}
	FORCEINLINE UBOOL ContainsCode()						const	{return ArContainsCode;}
	FORCEINLINE UBOOL ContainsMap()							const	{return ArContainsMap;}
	FORCEINLINE UBOOL ForceByteSwapping()					const	{return ArForceByteSwapping;}
	FORCEINLINE UBOOL IsSerializingDefaults()				const	{return ArSerializingDefaults > 0 ? TRUE : FALSE;}
	FORCEINLINE UBOOL IsIgnoringArchetypeRef()				const	{return ArIgnoreArchetypeRef;}
	FORCEINLINE UBOOL IsIgnoringOuterRef()					const	{return ArIgnoreOuterRef;}
	FORCEINLINE UBOOL IsIgnoringClassRef()					const	{return ArIgnoreClassRef;}
	FORCEINLINE UBOOL IsAllowingReferenceElimination()		const	{return ArAllowEliminatingReferences > 0 ? TRUE : FALSE;}
	FORCEINLINE UBOOL IsAllowingLazyLoading()				const	{return ArAllowLazyLoading;}
	FORCEINLINE UBOOL IsObjectReferenceCollector()			const	{return ArIsObjectReferenceCollector;}
	FORCEINLINE UBOOL IsCountingMemory()					const	{return ArIsCountingMemory;}
	FORCEINLINE DWORD GetPortFlags()						const	{return ArPortFlags;}
	FORCEINLINE UBOOL HasAnyPortFlags( DWORD Flags )		const	{return (ArPortFlags&Flags) != 0;}
	FORCEINLINE UBOOL HasAllPortFlags( DWORD Flags )		const	{return (ArPortFlags&Flags) == Flags;}
	FORCEINLINE UBOOL ShouldSkipBulkData()					const	{return ArShouldSkipBulkData;}
	FORCEINLINE UBOOL IsFinalPackageSave()					const	{return ArIsFinalPackageSave;}
	FORCEINLINE INT GetMaxSerializeSize()					const	{return ArMaxSerializeSize;}

	/**
	 * Sets the archive version number. Used by the code that makes sure that ULinkerLoad's internal 
	 * archive versions match the file reader it creates.
	 *
	 * @param Ver	new version number
	 */
	void SetVer(INT InVer)			{ ArVer = InVer; }
	/**
	 * Sets the archive licensee version number. Used by the code that makes sure that ULinkerLoad's 
	 * internal archive versions match the file reader it creates.
	 *
	 * @param Ver	new version number
	 */
	void SetLicenseeVer(INT InVer)	{ ArLicenseeVer = InVer; }

	/**
	 * Toggle saving as Unicode. This is needed when we need to make sure ANSI strings are saved as Unicode
	 *
	 * @param Enabled	set to true to force saving as Unicode
	 */
	void SetForceUnicode( UBOOL Enabled )	{ ArForceUnicode = Enabled; }

	/**
	 * Toggle byte order swapping. This is needed in rare cases when we already know that the data
	 * swapping has already occured or if we know that it will be handled later.
	 *
	 * @param Enabled	set to true to enable byte order swapping
	 */
	void SetByteSwapping( UBOOL Enabled )	{ ArForceByteSwapping = Enabled; }

	/**
	 * Sets the archive's property serialization modifier flags
	 *
	 * @param	InPortFlags		the new flags to use for property serialization
	 */
	void SetPortFlags( DWORD InPortFlags )	{ ArPortFlags = InPortFlags; }

	/**
	  * Sets the archive is a SaveGame
	  *
	  * @param  IsSaveGame		new flag
	  */
	void SetIsSaveGame( UBOOL IsSaveGame ) { ArIsSaveGame = IsSaveGame; }

	/**
	 * Returns if an async close operation has finished or not, as well as if there was an error
	 *
	 * @param bHasError TRUE if there was an error
	 *
	 * @return TRUE if the close operation is complete (or if Close is not an async operation) 
	 */
	virtual UBOOL IsCloseComplete(UBOOL& bHasError) { bHasError = FALSE; return TRUE; }

	// Friend archivers.
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, ANSICHAR& C )
	{
		Ar.Serialize( &C, 1 );
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, BYTE& B )
	{
		Ar.Serialize( &B, 1 );
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, SBYTE& B )
	{
		Ar.Serialize( &B, 1 );
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, WORD& W )
	{
		Ar.ByteOrderSerialize( &W, sizeof(W) );
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, SWORD& S )
	{
		Ar.ByteOrderSerialize( &S, sizeof(S) );
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, DWORD& D )
	{
		Ar.ByteOrderSerialize( &D, sizeof(D) );
		return Ar;
	}
#if !PLATFORM_MACOSX && !FLASH
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, UINT& D )
	{
		Ar.ByteOrderSerialize( &D, sizeof(D) );
		return Ar;
	}
#endif
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, INT& I )
	{
		Ar.ByteOrderSerialize( &I, sizeof(I) );
		return Ar;
	}
	// Various compilers and archs need this, others don't.
#if (!PS3 && !_MSC_VER && !WIIU) && (PLATFORM_MACOSX || ((defined(__GNUC__) && (__GNUC__ < 4))))
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, PTRINT& P )
	{
		Ar.ByteOrderSerialize( &P, sizeof(P) );
		return Ar;
	}
#endif
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, FLOAT& F )
	{
		Ar.ByteOrderSerialize( &F, sizeof(F) );
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, DOUBLE& F )
	{
		Ar.ByteOrderSerialize( &F, sizeof(F) );
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<( FArchive &Ar, QWORD& Q )
	{
		Ar.ByteOrderSerialize( &Q, sizeof(Q) );
		return Ar;
	}
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, SQWORD& S )
	{
		Ar.ByteOrderSerialize( &S, sizeof(S) );
		return Ar;
	}
	friend FArchive& operator<<( FArchive& Ar, FString& A );

	/**
	 * Indicates whether this archive is filtering editor-only on save or contains data that had editor-only content stripped.
	 */
	virtual UBOOL IsFilterEditorOnly()
	{
		return ArIsFilterEditorOnly;
	}

	/**
	 * Flags that this archive needs handle filtering editor-only content.
	 */
	virtual void SetFilterEditorOnly( UBOOL InFilterEditorOnly )
	{
		ArIsFilterEditorOnly = InFilterEditorOnly;
	}

protected:
	// Status variables.
	INT ArVer;
	INT ArNetVer;
	INT ArLicenseeVer;
	UBOOL ArIsLoading;
	UBOOL ArIsSaving;
	UBOOL ArIsTransacting;
	/** Whether this archive wants properties to be serialized in binary form instead of tagged.			*/
	UBOOL ArWantBinaryPropertySerialization;
	/** Whether this archive wants to always save strings in unicode format									*/
	UBOOL ArForceUnicode;
	UBOOL ArIsPersistent;
	UBOOL ArForEdit;
	UBOOL ArForClient;
	UBOOL ArForServer;
	UBOOL ArIsError;
	UBOOL ArIsCriticalError;
	/** If TRUE, archive contains cooked data.																*/
	UBOOL ArContainsCookedData;
	/** Quickly tell if an archive contains script code.													*/
	UBOOL ArContainsCode;
	/** Used to determine whether FArchive contains a level or world.										*/
	UBOOL ArContainsMap;
	/** Whether we should forcefully swap bytes.															*/
	UBOOL ArForceByteSwapping;
	/** Whether we are currently serializing defaults. > 0 means yes, <= 0 means no							*/
	INT ArSerializingDefaults;
	/** If true, we will not serialize the ObjectArchetype reference in UObject.							*/
	UBOOL ArIgnoreArchetypeRef;
	/** If true, we will not serialize the Outer reference in UObject.										*/
	UBOOL ArIgnoreOuterRef;
	/** If true, UObject::Serialize will skip serialization of the Class property							*/
	UBOOL ArIgnoreClassRef;
	/** Whether we are allowed to potentially NULL references to UObjects, > 0 means yes, <= 0 means no		*/
	INT ArAllowEliminatingReferences;
	/** Whether to allow lazy loading.																		*/
	UBOOL ArAllowLazyLoading;
	/** Whether this archive only cares about serializing object references.								*/
	UBOOL ArIsObjectReferenceCollector;
	/** Whether this archive is counting memory and therefore wants e.g. TMaps to be serialized.			*/
	UBOOL ArIsCountingMemory;
	/** Modifier flags that be used when serializing UProperties											*/
	DWORD ArPortFlags;
	/** Whether bulk data serialization should be skipped or not.											*/
	UBOOL ArShouldSkipBulkData;
	/** Set to indentity this archive as savegame file **/
	UBOOL ArIsSaveGame;
	/** 
	 * Indicates whether this is the final call to UObject::Serialize with ArIsSaving == TRUE during cooking.
	 * This is useful for knowing when to do slow tasks on save while cooking since UObject::Serialize is called multiple times on each object.
	 */
	UBOOL ArIsFinalPackageSave;
	/** Max size of data that this archive is allowed to serialize											*/
	INT ArMaxSerializeSize;
	/** Whether editor only properties are being filtered from the archive (or has been filtered).			*/
	UBOOL ArIsFilterEditorOnly;

	/**
	 * Resets all of the base archive members
	 */
	void Reset(void)
	{
		ArVer								= GPackageFileVersion;
		ArNetVer							= GEngineNegotiationVersion;
		ArLicenseeVer						= GPackageFileLicenseeVersion;
		ArIsLoading							= FALSE;
		ArIsSaving							= FALSE;
		ArIsTransacting						= FALSE;
		ArWantBinaryPropertySerialization	= FALSE;
		ArForceUnicode						= FALSE;
		ArIsPersistent						= FALSE;
		ArIsError							= FALSE;
		ArIsCriticalError					= FALSE;
		ArForEdit							= TRUE;
		ArForClient							= TRUE;
		ArForServer							= TRUE;
		ArContainsCookedData				= FALSE;
		ArContainsCode						= FALSE;
		ArContainsMap						= FALSE;
		ArForceByteSwapping					= FALSE;
		ArSerializingDefaults				= FALSE;
		ArIgnoreArchetypeRef				= FALSE;
		ArIgnoreOuterRef					= FALSE;
		ArIgnoreClassRef					= FALSE;
		ArAllowEliminatingReferences		= TRUE;
		ArAllowLazyLoading					= FALSE;
		ArIsObjectReferenceCollector		= FALSE;
		ArIsCountingMemory					= FALSE;
		ArPortFlags							= 0;
		ArShouldSkipBulkData				= FALSE;
		ArIsSaveGame						= FALSE;
		ArIsFinalPackageSave				= FALSE;
		ArMaxSerializeSize					= 0;
		ArIsFilterEditorOnly				= FALSE;
	}
};

/**
 * Archive constructor.
 */
template <class T> T Arctor( FArchive& Ar )
{
	T Tmp;
	Ar << Tmp;
	return Tmp;
}

/** The base type of archive proxies; archive types that modify the behavior of another archive type. */
class FArchiveProxy : public FArchive
{
public:

	/** Initialization constructor. */
	FArchiveProxy(FArchive& InInnerArchive)
	:	InnerArchive(InInnerArchive)
	{
		ArVer								= InnerArchive.Ver();
		ArNetVer							= InnerArchive.NetVer();
		ArLicenseeVer						= InnerArchive.LicenseeVer();
		ArIsLoading							= InnerArchive.IsLoading();
		ArIsSaving							= InnerArchive.IsSaving();
		ArIsTransacting						= InnerArchive.IsTransacting();
		ArWantBinaryPropertySerialization	= InnerArchive.WantBinaryPropertySerialization();
		ArForceUnicode						= InnerArchive.IsForcingUnicode();
		ArIsPersistent						= InnerArchive.IsPersistent();
		ArIsError							= InnerArchive.IsError();
		ArIsCriticalError					= InnerArchive.IsCriticalError();
		ArForEdit							= InnerArchive.ForEdit();
		ArForClient							= InnerArchive.ForClient();
		ArForServer							= InnerArchive.ForServer();
		ArContainsCode						= InnerArchive.ContainsCode();
		ArContainsMap						= InnerArchive.ContainsMap();
		ArForceByteSwapping					= InnerArchive.ForceByteSwapping();
		ArSerializingDefaults				= InnerArchive.IsSerializingDefaults();
		ArIgnoreArchetypeRef				= InnerArchive.IsIgnoringArchetypeRef();
		ArIgnoreOuterRef					= InnerArchive.IsIgnoringOuterRef();
		ArIgnoreClassRef					= InnerArchive.IsIgnoringClassRef();
		ArAllowEliminatingReferences		= InnerArchive.IsAllowingReferenceElimination();
		ArAllowLazyLoading					= InnerArchive.IsAllowingLazyLoading();
		ArIsObjectReferenceCollector		= InnerArchive.IsObjectReferenceCollector();
		ArIsCountingMemory					= InnerArchive.IsCountingMemory();
		ArPortFlags							= InnerArchive.GetPortFlags();
		ArShouldSkipBulkData				= InnerArchive.ShouldSkipBulkData();
		ArIsSaveGame						= InnerArchive.IsSaveGame();
		ArIsFinalPackageSave				= InnerArchive.IsFinalPackageSave();
		ArMaxSerializeSize					= InnerArchive.GetMaxSerializeSize();
		ArIsFilterEditorOnly				= FALSE;
	}

	// FArchive interface.
	virtual void Serialize( void* V, INT Length )
	{
		InnerArchive.Serialize(V,Length);
	}
	virtual void SerializeBits( void* V, INT LengthBits )
	{
		InnerArchive.SerializeBits(V,LengthBits);
	}
	virtual void SerializeInt( DWORD& Value, DWORD Max )
	{
		InnerArchive.SerializeInt(Value,Max);
	}
	virtual void Preload( UObject* Object )
	{
		InnerArchive.Preload(Object);
	}
	virtual void CountBytes( SIZE_T InNum, SIZE_T InMax )
	{
		InnerArchive.CountBytes(InNum,InMax);
	}
	virtual FArchive& operator<<( class FName& N )
	{
		return InnerArchive << N;
	}
	virtual FArchive& operator<<( class UObject*& Res )
	{
		return InnerArchive << Res;
	}
	virtual FString GetArchiveName() const;
	virtual class ULinker* GetLinker()
	{
		return InnerArchive.GetLinker();
	}
	virtual INT Tell()
	{
		return InnerArchive.Tell();
	}
	virtual INT TotalSize()
	{
		return InnerArchive.TotalSize();
	}
	virtual UBOOL AtEnd()
	{
		return InnerArchive.AtEnd();
	}
	virtual void Seek( INT InPos )
	{
		InnerArchive.Seek(InPos);
	}
	virtual void AttachBulkData( UObject* Owner, FUntypedBulkData* BulkData )
	{
		InnerArchive.AttachBulkData(Owner,BulkData);
	}
	virtual void DetachBulkData( FUntypedBulkData* BulkData, UBOOL bEnsureBulkDataIsLoaded )
	{
		InnerArchive.DetachBulkData(BulkData,bEnsureBulkDataIsLoaded);
	}
	virtual UBOOL Precache( INT PrecacheOffset, INT PrecacheSize )
	{
		return InnerArchive.Precache(PrecacheOffset,PrecacheSize);
	}
	virtual UBOOL SetCompressionMap( TArray<struct FCompressedChunk>* CompressedChunks, ECompressionFlags CompressionFlags )
	{
		return InnerArchive.SetCompressionMap(CompressedChunks,CompressionFlags);
	}
	virtual void Flush()
	{
		InnerArchive.Flush();
	}
	virtual UBOOL Close()
	{
		return InnerArchive.Close();
	}
	virtual UBOOL GetError()
	{
		return InnerArchive.GetError();
	}
	virtual void MarkScriptSerializationStart( const UObject* Obj )
	{
		InnerArchive.MarkScriptSerializationStart(Obj);
	}
	virtual void MarkScriptSerializationEnd( const UObject* Obj )
	{
		InnerArchive.MarkScriptSerializationEnd(Obj);
	}
	virtual UBOOL IsCloseComplete(UBOOL& bHasError)
	{
		return InnerArchive.IsCloseComplete(bHasError);
	}

protected:

	/** The archive that this archive is a proxy to. */
	FArchive& InnerArchive;
};

/**
 * A proxy archive that serializes FNames as string data
 */
 struct FNameAsStringProxyArchive : public FArchiveProxy
 {
	 FNameAsStringProxyArchive(FArchive& InInnerArchive)
		 :	FArchiveProxy(InInnerArchive)
	 {
	 }

	 /**
	  * Serialize the given FName as an FString
	  */
	 virtual FArchive& operator<<( class FName& N );
 };

/**
 * A proxy archive that serializes UObjects and FNames as string data. Expected
 * use is:
 *    FArchive* SomeAr = CreateAnAr();
 *    FObjectAndNameAsStringProxyArchive Ar(*SomeAr);
 *    SomeObject->Serialize(Ar);
 *    FinalizeAr(SomeAr);
 * 
 * @param InInnerArchive The actual FArchive object to serialize normal data types (FStrings, INTs, etc)
 */
 struct FObjectAndNameAsStringProxyArchive : public FNameAsStringProxyArchive
 {
	FObjectAndNameAsStringProxyArchive(FArchive& InInnerArchive)
		:	FNameAsStringProxyArchive(InInnerArchive)
	{
	}

	/**
	 * Serialize the given UObject* as an FString
	 */
	virtual FArchive& operator<<(class UObject*& Obj);
 };


/*-----------------------------------------------------------------------------
	Compression helper.
-----------------------------------------------------------------------------*/

/**
 * Helper structure for compression support, containing information on compressed
 * and uncompressed size of a chunk of data.
 */
struct FCompressedChunkInfo
{
	/** Compressed size of data.	*/
	INT	CompressedSize;
	/** Uncompresses size of data.	*/
	INT UncompressedSize;
};

/** 
 * FCompressedChunkInfo serialize operator.
 */
FArchive& operator<<( FArchive& Ar, FCompressedChunkInfo& Chunk );

