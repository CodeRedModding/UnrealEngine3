/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

enum ESourceControlState
{
	/** Don't know or don't care. */
	SCC_DontCare		= 0,

	/** File is checked out to current user. */
	SCC_CheckedOut		= 1,

	/** File is not checked out (but IS controlled by the source control system). */
	SCC_ReadOnly		= 2,

	/** File is not at the head revision - must sync the file before editing. */
	SCC_NotCurrent		= 3,

	/** File is new and not in the depot - needs to be added. */
	SCC_NotInDepot		= 4,

	/** File is checked out by another user and cannot be checked out locally. */
	SCC_CheckedOutOther	= 5,

	/** Certain packages are best ignored by the SCC system (MyLevel, Transient, etc). */
	SCC_Ignore			= 6
};


/**
 * Group of bitflag values for indicating which type of content browser update is desired.
 */
enum EContentBrowserRefreshFlag
{
	CBR_None					= 0x00000000,

	/**
	 * A change has occurred which affects the way the visual representation of this object, or this object's package
	 * has occurred.  Examples are marking the package dirty, updating a texture, fully loading a package, etc.
	 */
	CBR_UpdatePackageListUI		= 0x00000001,
	CBR_UpdateCollectionListUI	= 0x00000002,
	CBR_UpdateAssetListUI		= 0x00000004,

	/**
	 * Internal flags used by the combo flags below.  They are used solely for making checks against refresh flag
	 * values more concise.
	 * While these flags can be used on their own, using them will [probably] not result in desired behavior.
	 */
	CBR_InternalPackageUpdate	= 0x00000010,
	CBR_InternalCollectionUpdate= 0x00000020,
	CBR_InternalAssetUpdate		= 0x00000040,
	CBR_InternalQuickAssetUpdate= 0x00000080,

	/** A change has occurred which affects the SCC state of the object's package (i.e. check-in, check-out, etc.) */
	CBR_UpdateSCCState			= 0x00000100,

	/** Asset view should be updated to display only certain objects */
	CBR_SyncAssetView			= 0x00000200,

	/** a new object was created - add system tags to the local database */
	CBR_ObjectCreated			= 0x00000400,

	/** an object was deleted - remove tags from the local database */
	CBR_ObjectDeleted			= 0x00000800,

	/** an object was renamed - update system tags and existing tags/collections */
	CBR_ObjectRenamed			= 0x00001000,

	/** focus the content browser */
	CBR_FocusBrowser			= 0x00002000,

	/** Activates (i.e. opens the object editor for) the object specified in the event parameters */
	CBR_ActivateObject			= 0x00004000,

	/** Empties the content browsers selection and causes a refresh */
	CBR_EmptySelection			= 0x00008000,

	/** A package, that doesn't exist on disk yet, has been saved to disk - update the package view */
	CBR_NewPackageSaved			= 0x00010000,

	/** When used in conjunction with other flags that might cause an asset view sync operation, intentionally ignores the sync request */
	CBR_NoSync					= 0x00020000,

	/** Used to clear the current filter */
	CBR_ClearFilter				= 0x00040000,

	/** Used to validate when an asset is properly saved in the GAD database */
	CBR_ValidateObjectInGAD		= 0x00080000,

	/**
	 * A change has occurred which requires the respective list to repopulate its elements.  Examples include
	 * creating or deleting an object, creating a collection, etc.
	 */
	CBR_UpdatePackageList		= CBR_InternalPackageUpdate		| CBR_InternalQuickAssetUpdate | CBR_UpdatePackageListUI ,
	CBR_UpdateCollectionList	= CBR_InternalCollectionUpdate	| CBR_UpdateCollectionListUI,
	CBR_UpdateAssetList			= CBR_InternalAssetUpdate		| CBR_UpdateAssetListUI,

	/** All flags, for consistency */
	CBR_All						= 0xFFFFFFFF,
};

/** interface for objects that want to listen in for the addition/removal of net serializable objects and pacakges */
class FNetObjectNotify
{
public:
	/** destructor */
	virtual ~FNetObjectNotify();
	/** notification when a package is added to the NetPackages list */
	virtual void NotifyNetPackageAdded(UPackage* Package) = 0;
	/** notification when a package is removed from the NetPackages list */
	virtual void NotifyNetPackageRemoved(UPackage* Package) = 0;
	/** notification when an object is removed from a net package's NetObjects list */
	virtual void NotifyNetObjectRemoved(UObject* Object) = 0;
};


/**
 * Structure to hold information about an external packages objects used in cross-level references
 */
struct FLevelGuids
{
	/** Name of the external level */
	FName LevelName;

	/** Array of Guids possible in the other level (can be emptied out if all references are resolved after level load) */
	TArray<FGuid> Guids;
};

/**
 * A package.
 */
class UPackage : public UObject
{
private:
	DECLARE_CLASS_INTRINSIC(UPackage,UObject,0,Core)

	/** Used by the editor to determine if a package has been changed.																							*/
	UBOOL	bDirty;
	/** Used by the editor to determine if a package has changed since the last time the user Played-In-Editor */
	UBOOL bDirtyForPIE;
	/** Whether this package has been fully loaded (aka had all it's exports created) at some point.															*/
	UBOOL	bHasBeenFullyLoaded;
	/** Returns whether exports should be found in memory first before trying to load from disk from within CreateExport.										*/
	UBOOL	bShouldFindExportsInMemoryFirst;
	/** Whether the package is bound, set via SetBound.																											*/
	UBOOL	bIsBound;
	/** Indicates which folder to display this package under in the Generic Browser's list of packages. If not specified, package is added to the root level.	*/
	FName	FolderName;
	/** Time in seconds it took to fully load this package. 0 if package is either in process of being loaded or has never been fully loaded.					*/
	FLOAT LoadTime;

	/** GUID of package if it was loaded from disk; used by netcode to make sure packages match between client and server */
	FGuid Guid;
	/** size of the file for this package; if the package was not loaded from a file or was a forced export in another package, this will be zero */
	INT FileSize;
	/** array of net serializable objects currently loaded into this package (top level packages only) */
	TArray<UObject*> NetObjects;
	/** number of objects in the Objects list that currently exist (are loaded) */
	INT CurrentNumNetObjects;
	/** number of objects in the list for each generation of the package (for conforming) */
	TArray<INT> GenerationNetObjectCount;
	/** for packages that were a forced export in another package (seekfree loading), the name of that base package, otherwise NAME_None */
	FName ForcedExportBasePackageName;
	
	/** Mapping of guid to UObject of all objects that are pointed to by external objects */
	TMap<FGuid, UObject*>	ExportGuids;

	/** List of Guids of objects in other levels that are referenced by this level */
	TArray<FLevelGuids>		ImportGuids;

	/** array of UPackages that currently have NetObjects that are relevant to netplay */
	static TArray<UPackage*> NetPackages;
	/** objects that should be informed of NetPackage/NetObject changes */
	static TArray<FNetObjectNotify*> NetObjectNotifies;

	/** Package flags, serialized.																																*/
	DWORD	PackageFlags;

	/** The name of the file that this package was loaded from */
	FName	FileName;

	/** Editor only: Thumbnails stored in this package */
	TScopedPointer< FThumbnailMap > ThumbnailMap;

	/** returns a const reference to NetPackages */
	static FORCEINLINE const TArray<UPackage*>& GetNetPackages()
	{
		return NetPackages;
	}

	// Constructors.
	UPackage();

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	/** Serializer */
	virtual void Serialize( FArchive& Ar );

	// UPackage interface.

	/**
	 * @return		TRUE if the packge is bound, FALSE otherwise.
	 */
	UBOOL IsBound() const
	{
		return bIsBound;
	}

	/**
	 * Marks/unmarks the package as being bound.
	 */
	void SetBound( UBOOL bInBound )
	{
		bIsBound = bInBound;
	}

	/**
	 * Sets the time it took to load this package.
	 */
	void SetLoadTime( FLOAT InLoadTime )
	{
		LoadTime = InLoadTime;
	}

	/**
	 * Returns the time it took the last time this package was fully loaded, 0 otherwise.
	 *
	 * @return Time it took to load.
	 */
	FLOAT GetLoadTime()
	{
		return LoadTime;
	}

	/**
	 * Get the package's folder name
	 * @return		Folder name
	 */
	FName GetFolderName() const
	{
		return FolderName;
	}

	/**
	 * Set the package's folder name
	 */
	void SetFolderName (FName name)
	{
		FolderName = name;
	}

	/**
	 * Marks/Unmarks the package's bDirty flag
	 */
	void SetDirtyFlag( UBOOL bIsDirty );

	/**
	 * Returns whether the package needs to be saved.
	 *
	 * @return		TRUE if the package is dirty and needs to be saved, false otherwise.
	 */
	UBOOL IsDirty() const
	{
		return bDirty;
	}

	/**
	 * Returns whether the package has been changed since the last time we Played-In-Editor with it
	 *
	 * @return		TRUE if the package is dirty and needs to be saved for PIE, false otherwise.
	 */
	UBOOL IsDirtyForPIE() const
	{
		return bDirtyForPIE;
	}

	/**
	 * Called when the package is saved in a PIE level; clears the package's dirty bit for Play-In-Editor
	 */
	void ClearDirtyForPIEFlag()
	{
		bDirtyForPIE = FALSE;
	}

	/**
	 * Marks this package as being fully loaded.
	 */
	void MarkAsFullyLoaded()
	{
		bHasBeenFullyLoaded = TRUE;
	}

	/**
	 * Returns whether the package is fully loaded.
	 *
	 * @return TRUE if fully loaded or no file associated on disk, FALSE otherwise
	 */
	UBOOL IsFullyLoaded();

	/**
	 * Fully loads this package. Safe to call multiple times and won't clobber already loaded assets.
	 */
	void FullyLoad();

	/**
	 * Sets whether exports should be found in memory first or  not.
	 *
	 * @param bInShouldFindExportsInMemoryFirst	Whether to find in memory first or not
	 */
	void FindExportsInMemoryFirst( UBOOL bInShouldFindExportsInMemoryFirst )
	{
		bShouldFindExportsInMemoryFirst = bInShouldFindExportsInMemoryFirst;
	}

	/**
	 * Returns whether exports should be found in memory first before trying to load from disk
	 * from within CreateExport.
	 *
	 * @return TRUE if exports should be found via FindObject first, FALSE otherwise.
	 */
	UBOOL ShouldFindExportsInMemoryFirst()
	{
		return bShouldFindExportsInMemoryFirst;
	}

	/**
	 * Called to indicate that this package contains a ULevel or UWorld object.
	 */
	void ThisContainsMap() 
	{
		PackageFlags |= PKG_ContainsMap;
	}

	/**
	 * Returns whether this package contains a ULevel or UWorld object.
	 *
	 * @return		TRUE if package contains ULevel/ UWorld object, FALSE otherwise.
	 */
	UBOOL ContainsMap() const
	{
		return (PackageFlags & PKG_ContainsMap) ? TRUE : FALSE;
	}


	/** Returns true if this package has a thumbnail map */
	UBOOL HasThumbnailMap() const
	{
		return ThumbnailMap.IsValid();
	}
	
	/** Returns the thumbnail map for this package (const).  Only call this if HasThumbnailMap returns true! */
	const FThumbnailMap& GetThumbnailMap() const
	{
		check( HasThumbnailMap() );
		return *ThumbnailMap;
	}

	/** Access the thumbnail map for this package.  Only call this if HasThumbnailMap returns true! */
	FThumbnailMap& AccessThumbnailMap()
	{
		check( HasThumbnailMap() );
		return *ThumbnailMap;
	}


	/** initializes net info for this package from the ULinkerLoad associated with it
	 * @param InLinker the linker associated with this package to grab info from
	 * @param ExportIndex the index of the export in that linker's ExportMap where this package's export info can be found
	 *					INDEX_NONE indicates that this package is the LinkerRoot and we should look in the package file summary instead
	 */
	void InitNetInfo(ULinkerLoad* InLinker, INT ExportIndex);
	/** removes an object from the NetObjects list
	 * @param OutObject the object to remove
	 */
	void AddNetObject(UObject* InObject);
	/** removes an object from the NetObjects list
	 * @param OutObject the object to remove
	 */
	void RemoveNetObject(UObject* OutObject);
	/** clears NetIndex on all objects in our NetObjects list that are inside the passed in object
	 * @param OuterObject the Outer to check for
	 */
	void ClearAllNetObjectsInside(UObject* OuterObject);

	/** 
	 * This will add a number of NetObjects to the NetObjects list if the new count is 
	 * greater than the current size of the list.
	 *
	 * @param NewNumNetObjects new size the NetObjects list should be
	 *
	 * @script patcher
	 **/
	void PatchNetObjectList( INT NewNumNetObjects );


	/** returns the object at the specified index */
	FORCEINLINE UObject* GetNetObjectAtIndex(INT InIndex)
	{
		return (InIndex < NetObjects.Num() && NetObjects(InIndex) != NULL && !NetObjects(InIndex)->HasAnyFlags(RF_AsyncLoading)) ? NetObjects(InIndex) : NULL;
	}
	/** returns the number of net objects in the specified generation */
	FORCEINLINE INT GetNetObjectCount(INT Generation)
	{
		return (Generation < GenerationNetObjectCount.Num()) ? GenerationNetObjectCount(Generation) : 0;
	}
	/** returns a reference to GenerationNetObjectCount */
	FORCEINLINE const TArray<INT>& GetGenerationNetObjectCount()
	{
		return GenerationNetObjectCount;
	}
	/** returns our Guid */
	FORCEINLINE FGuid GetGuid()
	{
		return Guid;
	}
	/** makes our a new fresh Guid */
	FORCEINLINE void MakeNewGuid()
	{
		Guid = appCreateGuid();
	}
	/** creates empty (0 count) network object info
	 * for packages you want to be in the package map for downloading, etc, but shouldn't actually have any of its objects be relevant */
	FORCEINLINE void CreateEmptyNetInfo()
	{
		GenerationNetObjectCount.Empty();
		GenerationNetObjectCount.AddItem(0);
	}

	/** returns our FileSize */
	FORCEINLINE INT GetFileSize()
	{
		return FileSize;
	}
	/** returns ForcedExportBasePackageName */
	FORCEINLINE FName GetForcedExportBasePackageName()
	{
		return ForcedExportBasePackageName;
	}
	/** returns CurrentNumNetObjects */
	FORCEINLINE INT GetCurrentNumNetObjects()
	{
		return CurrentNumNetObjects;
	}

	/**
	 * Looks up all exports in the Linker's ExportGuidsAwaitingLookup map, and fills out ExportGuids
	 * 
	 * @param Linker The Linker that loaded this package (the package's _Linker may be NULL)
	 */
	void LookupAllOutstandingCrossLevelExports(ULinkerLoad* Linker);

	////////////////////////////////////////////////////////
	// MetaData 

	/**
	 * Get (after possibly creating) a metadata object for this package
	 *
	 * @return A valid UMetaData pointer for all objects in this package
	 */
	class UMetaData* GetMetaData();

protected:
	// MetaData for the editor, or NULL in the game
	class UMetaData*	MetaData;
};

/*-----------------------------------------------------------------------------
	UTextBuffer.
-----------------------------------------------------------------------------*/

/**
 * An object that holds a bunch of text.  The text is contiguous and, if
 * of nonzero length, is terminated by a NULL at the very last position.
 */
class UTextBuffer : public UObject, public FOutputDevice
{
	DECLARE_CLASS_INTRINSIC(UTextBuffer,UObject,0,Core)
	NO_DEFAULT_CONSTRUCTOR(UTextBuffer)

	// Variables.
	INT Pos, Top;
	FString Text;

	// Constructors.
	UTextBuffer( const TCHAR* Str );

	// UObject interface.
	void Serialize( FArchive& Ar );

	// FOutputDevice interface.
	void Serialize( const TCHAR* Data, EName Event );
};

/*-----------------------------------------------------------------------------
	UMetaData.
-----------------------------------------------------------------------------*/

/**
 * An object that holds a map of key/value pairs. 
 */
class UMetaData : public UObject
{
	DECLARE_CLASS_INTRINSIC(UMetaData, UObject, 0, Core)

#if 1
	// Variables.
	TMap<UObject*, TMap<FName, FString> > ObjectMetaDataMap;

	// UObject interface.
	void Serialize(FArchive& Ar);

	// MetaData utility functions
	/**
	 * Return the value for the given key in the given property
	 * @param Object the object to lookup the metadata for
	 * @param Key The key to lookup
	 * @return The value if found, otherwise an empty string
	 */
	const FString& GetValue(const UObject* Object, const TCHAR* Key);
	
	/**
	 * Return the value for the given key in the given property
	 * @param Object the object to lookup the metadata for
	 * @param Key The key to lookup
	 * @return The value if found, otherwise an empty string
	 */
	const FString& GetValue(const UObject* Object, FName Key);

	/**
	 * Return whether or not the Key is in the meta data
	 * @param Object the object to lookup the metadata for
	 * @param Key The key to query for existence
	 * @return TRUE if found
	 */
	UBOOL HasValue(const UObject* Object, const TCHAR* Key);

	/**
	 * Return whether or not the Key is in the meta data
	 * @param Object the object to lookup the metadata for
	 * @param Key The key to query for existence
	 * @return TRUE if found
	 */
	UBOOL HasValue(const UObject* Object, FName Key);

	/**
	 * Is there any metadata for this property?
	 * @param Object the object to lookup the metadata for
	 * @return TrUE if the object has any metadata at all
	 */
	UBOOL HasObjectValues(const UObject* Object);

	/**
	 * Set the key/value pair in the Property's metadata
	 * @param Object the object to set the metadata for
	 * @Values The metadata key/value pairs
	 */
	void SetObjectValues(const UObject* Object, const TMap<FName, FString>& Values);

	/**
	 * Set the key/value pair in the Property's metadata
	 * @param Object the object to set the metadata for
	 * @param Key A key to set the data for
	 * @param Value The value to set for the key
	 * @Values The metadata key/value pairs
	 */
	void SetValue(const UObject* Object, const TCHAR* Key, const FString& Value);

	/**
	 * Set the key/value pair in the Property's metadata
	 * @param Object the object to set the metadata for
	 * @param Key A key to set the data for
	 * @param Value The value to set for the key
	 * @Values The metadata key/value pairs
	 */
	void SetValue(const UObject* Object, FName Key, const FString& Value);

	/**
	 * Method to help support metadata for intrinsic classes. Attempts to find and set meta data for a given intrinsic class and subobject
	 * within an ini file. The keys in the ini file are expected to be in the format of ClassToUseName.ObjectToUseName, where all of the characters
	 * in the names are alphanumeric.
	 *
	 * @param	ClassToUse			Intrinsic class to attempt to find meta data for
	 * @param	ObjectToUse			Sub-object, such as a property or enum, of the provided intrinsic class
	 * @param	MetaDataToAddTo		Metadata object that any parsed metadata found in the ini will be added to
	 *
	 * @return	TRUE if any data was found, parsed, and added; FALSE otherwise
	 */
	static UBOOL AttemptParseIntrinsicMetaData( const UClass& ClassToUse, const UObject& ObjectToUse, UMetaData& MetaDataToAddTo );

	/**
	 * Removes any metadata entries that are to objects not inside the same package as this UMetaData object.
	 */
	void RemoveMetaDataOutsidePackage();
#else
	// Variables.
	TMap<FString, TMap<FName, FString> > Values;

	// UObject interface.
	void Serialize(FArchive& Ar);

	// MetaData utility functions
	/**
	 * Return the value for the given key in the given property
	 * @param ObjectPath The path name (GetPathName()) to the object in this package to lookup in
	 * @param Key The key to lookup
	 * @return The value if found, otherwise an empty string
	 */
	const FString& GetValue(const FString& ObjectPath, FName Key);

	/**
	 * Return the value for the given key in the given property
	 * @param ObjectPath The path name (GetPathName()) to the object in this package to lookup in
	 * @param Key The key to lookup
	 * @return The value if found, otherwise an empty string
	 */
	const FString& GetValue(const FString& ObjectPath, const TCHAR* Key);
	
	/**
	 * Return whether or not the Key is in the meta data
	 * @param ObjectPath The path name (GetPathName()) to the object in this package to lookup in
	 * @param Key The key to query for existence
	 * @return TRUE if found
	 */
	UBOOL HasValue(const FString& ObjectPath, FName Key);

	/**
	 * Return whether or not the Key is in the meta data
	 * @param ObjectPath The path name (GetPathName()) to the object in this package to lookup in
	 * @param Key The key to query for existence
	 * @return TRUE if found
	 */
	UBOOL HasValue(const FString& ObjectPath, const TCHAR* Key);

	/**
	 * Is there any metadata for this property?
	 * @param ObjectPath The path name (GetPathName()) to the object in this package to lookup in
	 * @return TrUE if the object has any metadata at all
	 */
	UBOOL HasObjectValues(const FString& ObjectPath);

	/**
	 * Set the key/value pair in the Property's metadata
	 * @param ObjectPath The path name (GetPathName()) to the object in this package to lookup in
	 * @Values The metadata key/value pairs
	 */
	void SetObjectValues(const FString& ObjectPath, const TMap<FName, FString>& Values);

	/**
	 * Set the key/value pair in the Property's metadata
	 * @param ObjectPath The path name (GetPathName()) to the object in this package to lookup in
	 * @param Key A key to set the data for
	 * @param Value The value to set for the key
	 * @Values The metadata key/value pairs
	 */
	void SetValue(const FString& ObjectPath, FName Key, const FString& Value);

	/**
	 * Set the key/value pair in the Property's metadata
	 * @param ObjectPath The path name (GetPathName()) to the object in this package to lookup in
	 * @param Key A key to set the data for
	 * @param Value The value to set for the key
	 * @Values The metadata key/value pairs
	 */
	void SetValue(const FString& ObjectPath, const TCHAR* Key, const FString& Value);
#endif
};

/*----------------------------------------------------------------------------
	USystem.
----------------------------------------------------------------------------*/

class USystem : public USubsystem
{
	DECLARE_CLASS_INTRINSIC(USystem,USubsystem,CLASS_Config|0,Core)

	// Variables.
	/** Files older than this will be deleted at startup down to MaxStaleCacheSize */
	INT StaleCacheDays;
	/** The size to clean the cache down to at startup of files older than StaleCacheDays. This will allow even old files to be cached, up to this set amount */
	INT MaxStaleCacheSize;
	/** The max size the cache can ever be */
	INT MaxOverallCacheSize;
	/** Soft limit for package size in MByte. There will be a warning when saving for larger packages. */
	INT PackageSizeSoftLimit;

	FLOAT AsyncIOBandwidthLimit;
	FString SavePath;
	FString CachePath;
	FString CacheExt;
	TArray<FString> Paths;
	/** Paths if -seekfreeloading is used on PC. */
	TArray<FString> SeekFreePCPaths;
	/** List of directories containing script packages */
	TArray<FString> ScriptPaths;
	/** List of directories containing script packages compiled with the -FINAL_RELEASE switch */
	TArray<FString> FRScriptPaths;
	TArray<FString> CutdownPaths;
	TArray<FName> Suppress;
	TArray<FString> Extensions;
	/** Extensions if -seekfreeloading is used on PC. */
	TArray<FString> SeekFreePCExtensions;
	TArray<FString> LocalizationPaths;
	FString TextureFileCacheExtension;

	// Constructors.
	void StaticConstructor();
	USystem();

	// FExec interface.
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );

	/**
	 * Performs periodic cleaning of the cache
	 *
	 * @param bForceDeleteDownloadCache If TRUE, the entire autodownload cache will be cleared
	 */
	void PerformPeriodicCacheCleanup(UBOOL bForceDeleteDownloadCache=FALSE);

	/**
	 * Cleans out the cache as necessary to free the space needed for an incoming
	 * incoming downloaded package 
	 *
	 * @param SpaceNeeded Amount of space needed to download a package
	 */
	void CleanCacheForNeededSpace(INT SpaceNeeded);

	/**
	 * Check to see if the cache contains a package with the given Guid and optional name
	 *
	 * @param Guid Guid to look for
	 * @param PackageName Optional name of the package to check against
	 * @param Filename [out] Path to the file if found
	 *
	 * @return TRUE if the package was found
	 */
	UBOOL CheckCacheForPackage(const FGuid& Guid, const TCHAR* PackageName, FString& Filename);

private:

	/**
	 * Internal helper function for cleaning the cache
	 *
	 * @param MaxSize Max size the total of the cache can be (for files older than ExpirationSeconds)
	 * @param ExpirationSeconds Only delete files older than this down to MaxSize
	 */
	void CleanCache(INT MaxSize, DOUBLE ExpirationSeconds);
};

/*-----------------------------------------------------------------------------
	UCommandlet helper defines.
-----------------------------------------------------------------------------*/

#define BEGIN_COMMANDLET(ClassName,Package) \
class U##ClassName##Commandlet : public UCommandlet \
{ \
	DECLARE_CLASS_INTRINSIC(U##ClassName##Commandlet,UCommandlet,CLASS_Transient,Package) \
	U##ClassName##Commandlet() \
	{ \
		if ( !HasAnyFlags(RF_ClassDefaultObject) ) \
		{ \
			AddToRoot(); \
		} \
	}

#define END_COMMANDLET \
	void InitializeIntrinsicPropertyValues() \
	{ \
		LogToConsole	= 0;	\
		IsClient        = 1;	\
		IsEditor        = 1;	\
		IsServer        = 1;	\
		ShowErrorCount  = 1;	\
		ThisClass::StaticInitialize();	\
	} \
	INT Main(const FString& Params); \
};

#define BEGIN_CHILD_COMMANDLET(ClassName,BaseClass,Package) \
class U##ClassName##Commandlet : public U##BaseClass##Commandlet \
{ \
	DECLARE_CLASS_INTRINSIC(U##ClassName##Commandlet,U##BaseClass##Commandlet,CLASS_Transient,Package) \
	U##ClassName##Commandlet() \
	{ \
		if ( !HasAnyFlags(RF_ClassDefaultObject) ) \
		{ \
			AddToRoot(); \
		} \
	}

#define END_CHILD_COMMANDLET \
	void InitializeIntrinsicPropertyValues() \
	{ \
		LogToConsole	= 0;	\
		IsClient        = 1;	\
		IsEditor        = 1;	\
		IsServer        = 1;	\
		ShowErrorCount  = 1;	\
		ThisClass::StaticInitialize();	\
	} \
};

