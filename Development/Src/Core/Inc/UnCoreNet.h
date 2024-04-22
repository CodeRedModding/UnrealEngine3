/*=============================================================================
	UnCoreNet.h: Core networking support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

//
// Information about a field.
//
class FFieldNetCache
{
public:
	UField* Field;
	INT FieldNetIndex;
	INT ConditionIndex;
	FFieldNetCache()
	{}
	FFieldNetCache( UField* InField, INT InFieldNetIndex, INT InConditionIndex )
	: Field(InField), FieldNetIndex(InFieldNetIndex), ConditionIndex(InConditionIndex)
	{}
};

//
// Information about a class, cached for network coordination.
//
class FClassNetCache
{
	friend class UPackageMap;
public:
	FClassNetCache();
	FClassNetCache( UClass* Class );
	INT GetMaxIndex()
	{
		return FieldsBase+Fields.Num();
	}
	INT GetRepConditionCount()
	{
		return RepConditionCount;
	}
	FFieldNetCache* GetFromField( UObject* Field )
	{
		FFieldNetCache* Result=NULL;
		for( FClassNetCache* C=this; C; C=C->Super )
		{
			if( (Result=C->FieldMap.FindRef(Field))!=NULL )
			{
				break;
			}
		}
		return Result;
	}
	FFieldNetCache* GetFromIndex( INT Index )
	{
		for( FClassNetCache* C=this; C; C=C->Super )
		{
			if( Index>=C->FieldsBase && Index<C->FieldsBase+C->Fields.Num() )
			{
				return &C->Fields(Index-C->FieldsBase);
			}
		}
		return NULL;
	}
	TArray<FFieldNetCache*> RepProperties;
private:
	INT FieldsBase;
	FClassNetCache* Super;
	INT RepConditionCount;
	UClass* Class;
	TArray<FFieldNetCache> Fields;
	TMap<UObject*,FFieldNetCache*> FieldMap;
};

//
// Ordered information of linker file requirements.
//
class FPackageInfo
{
public:
	// Variables.
	FName			PackageName;		// name of the package we need to request.
	UPackage*		Parent;				// The parent package.
	FGuid			Guid;				// Package identifier.
	INT				ObjectBase;			// Net index of first object.
	INT				ObjectCount;		// Number of objects, defined by server.
	INT				LocalGeneration;	// This machine's generation of the package.
	INT				RemoteGeneration;	// Remote machine's generation of the package.
	DWORD			PackageFlags;		// Package flags.
	FName			ForcedExportBasePackageName; // for packages that were a forced export in another package (seekfree loading), the name of that base package, otherwise NAME_None
	BYTE			LoadingPhase;		// indicates if package was loaded during a seamless loading operation (e.g. seamless level change) to aid client in determining when to process it
	FString			Extension;			// Extension of the package file, used so HTTP downloading can get the package
	FName			FileName;			// Name of the file this package was loaded from if it is not equal to the PackageName

	// Functions.
	FPackageInfo(UPackage* Package = NULL);
	friend FArchive& operator<<( FArchive& Ar, FPackageInfo& I );
};

/** maximum index that may ever be used to serialize an object over the network
 * this affects the number of bits used to send object references
 */
#define MAX_OBJECT_INDEX (DWORD(1) << 31)

//
// Maps objects and names to and from indices for network communication.
//
class UPackageMap : public UObject
{
	DECLARE_CLASS_INTRINSIC(UPackageMap,UObject,CLASS_Transient|0,Core);

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );
	void FinishDestroy();
	
	// UPackageMap interface.
	virtual UBOOL CanSerializeObject( UObject* Obj );
	
	virtual UBOOL SupportsPackage( UObject* InOuter );
	virtual UBOOL SupportsObject( UObject* Obj );

	virtual UBOOL SerializeObject( FArchive& Ar, UClass* Class, UObject*& Obj );
	virtual UBOOL SerializeName( FArchive& Ar, FName& Name );

	/** get the cached field to index mappings for the given class */
	FClassNetCache* GetClassNetCache(UClass* Class);
	/** remove the cached field to index mappings for the given class (usually, because the class is being GC'ed) */
	void RemoveClassNetCache(UClass* Class);

	virtual void Compute();

	/** adds all packages from UPackage::GetNetPackages() to the package map */
	virtual void AddNetPackages();
	/** adds a single package to the package map
	 * @return Index of the entry for the package in List
	 */
	virtual INT AddPackage(UPackage* Package);

	/** removes a package from the package map
	 * @param Package the package to remove
	 * @param bAllowEntryDeletion (optional) whether to actually delete the entry in List or to leave the entry around to maintain indices
	 */
	void RemovePackage(UPackage* Package, UBOOL bAllowEntryDeletion);
	/** removes a package from the package map by GUID instead of by package reference */
	void RemovePackageByGuid(const FGuid& Guid);
	/** removes the passed in package only if both sides have completely sychronized it
	 * @param Package the package to remove
	 * @return TRUE if the package was removed or was not in the package map, FALSE if it was not removed because it has not been fully synchronized
	 */
	UBOOL RemovePackageOnlyIfSynced(UPackage* Package);
	/** adds a package info for a package that may or may not be loaded
	 * if an entry for the given package already exists, updates it
	 * @param Info the info to add
	 */
	virtual void AddPackageInfo(const FPackageInfo& Info);

	virtual void Copy( UPackageMap* Other );

	/** logs debug info (package list, etc) to the specified output device
	 * @param Ar - the device to log to
	 */
	void LogDebugInfo(FOutputDevice& Ar);

	// Variables.
	TArray<FPackageInfo> List;

protected:
	virtual INT ObjectToIndex( UObject* Object );
	virtual UObject* IndexToObject( INT InIndex, UBOOL Load );

	/** map from package names to their index in List */
	TMap<FName, INT> PackageListMap;
	TMap<UClass*, FClassNetCache*> ClassFieldIndices;
};

//
// Information for tracking retirement and retransmission of a property.
//
struct FPropertyRetirement
{
	INT			InPacketId;		// Packet received on, INDEX_NONE=none.
	INT			OutPacketId;	// Packet sent on, INDEX_NONE=none.
	BYTE		Reliable;		// Whether it was sent reliably.
	FPropertyRetirement()
	:	InPacketId	( INDEX_NONE )
	,   OutPacketId	( INDEX_NONE )

	{}
};

