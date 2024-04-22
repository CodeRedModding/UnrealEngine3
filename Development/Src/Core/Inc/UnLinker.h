/*=============================================================================
	UnLinker.h: Unreal object linker.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// should really be a typedef, but that requires lots of code to be changed

/**
 * Index into a ULnker's ImportMap or ExportMap.
 * Values greater than zero indicate that this is an index into the ExportMap.  The
 * actual array index will be (PACKAGE_INDEX - 1).
 *
 * Values less than zero indicate that this is an index into the ImportMap. The actual
 * array index will be (-PACKAGE_INDEX - 1)
 */
#define PACKAGE_INDEX	INT

/**
 * A PACKAGE_INDEX of 0 for an FObjectResource's OuterIndex indicates a
 * top-level UPackage which will be the LinkerRoot for a ULinkerLoad
 */
#define ROOTPACKAGE_INDEX 0

/**
 * A PACKAGE_INDEX of 0 for an FObjectExport's ClassIndex indicates that the
 * export is a UClass object
 */
#define UCLASS_INDEX 0

/**
 * A PACKAGE_INDEX of 0 for an FObjectExport's SuperIndex indicates that this
 * object does not have a parent struct, either because it is not a UStruct-derived
 * object, or it does not extend any other UStruct
 */
#define NULLSUPER_INDEX 0

/**
 * A PACKAGE_INDEX of 0 for an FObjectExport's ArchetypeIndex indicate that the
 * ObjectArchetype for this export's object is the class default object.
 */
#define CLASSDEFAULTS_INDEX 0

/**
 * Clarify checks for import indexes
 */
#define IS_IMPORT_INDEX(index) (index < 0)

/**
 * Base class for UObject resource types.  FObjectResources are used to store UObjects on disk
 * via ULinker's ImportMap (for resources contained in other packages) and ExportMap (for resources
 * contained within the same package)
 */
struct FObjectResource
{
	/**
	 * The name of the UObject represented by this resource.
	 * Serialized
	 */
	FName			ObjectName;

	/**
	 * Location of the resource for this resource's Outer.  Values of 0 indicate that this resource
	 * represents a top-level UPackage object (the linker's LinkerRoot).
	 * Serialized
	 */
	PACKAGE_INDEX	OuterIndex;

	FObjectResource();
	FObjectResource( UObject* InObject );
};

/*-----------------------------------------------------------------------------
	FObjectExport.
-----------------------------------------------------------------------------*/

typedef DWORD EExportFlags;
/** No flags																*/
#define EF_None						0x00000000		
/** Whether the export was forced into the export table via RF_ForceTagExp.	*/
#define	EF_ForcedExport				0x00000001

//@{
//@script patcher
/** indicates that this export was added by the script patcher, so this object's data will come from memory, not disk */
#define EF_ScriptPatcherExport		0x00000002
/** indicates that this export is a UStruct which will be patched with additional member fields by the script patcher */
#define EF_MemberFieldPatchPending	0x00000004
//@}
/** All flags																*/
#define EF_AllFlags					0xFFFFFFFF

//@script patcher
// Forward declarations
struct FScriptPatcher;
struct FLinkerPatchData;
struct FPatchData;
struct FScriptPatchData;
struct FEnumPatchData;
class FPatchReader;

/**
 * UObject resource type for objects that are contained within this package and can
 * be referenced by other packages.
 */
struct FObjectExport : public FObjectResource
{
	/**
	 * Location of the resource for this export's class (if non-zero).  A value of zero
	 * indicates that this export represents a UClass object; there is no resource for
	 * this export's class object
	 * Serialized
	 */
	PACKAGE_INDEX  	ClassIndex;

	/**
	 * Location of the resource for this export's SuperField (parent).  Only valid if
	 * this export represents a UStruct object. A value of zero indicates that the object
	 * represented by this export isn't a UStruct-derived object.
	 * Serialized
	 */
	PACKAGE_INDEX 	SuperIndex;

	/**
	 * Location of the resource for this resource's template.  Values of 0 indicate that this object's
	 * template is the class default object.
	 */
	PACKAGE_INDEX	ArchetypeIndex;

	/**
	 * The object flags for the UObject represented by this resource.  Only flags that
	 * match the RF_Load combination mask will be loaded from disk and applied to the UObject.
	 * Serialized
	 */
	EObjectFlags	ObjectFlags;

	/**
	 * The number of bytes to serialize when saving/loading this export's UObject.
	 * Serialized
	 */
	INT         	SerialSize;

	/**
	 * The location (into the ULinker's underlying file reader archive) of the beginning of the
	 * data for this export's UObject.  Used for verification only.
	 * Serialized
	 */
	INT         	SerialOffset;

	/**
	 * The location (into the ULinker's underlying file reader archive) of the beginning of the
	 * portion of this export's data that is serialized using script serialization.
	 * Transient
	 */
	INT				ScriptSerializationStartOffset;

	/**
	 * The location (into the ULinker's underlying file reader archive) of the end of the
	 * portion of this export's data that is serialized using script serialization.
	 * Transient
	 */
	INT				ScriptSerializationEndOffset;

	/**
	 * The UObject represented by this export.  Assigned the first time CreateExport is called for this export.
	 * Transient
	 */
	UObject*		_Object;

	/**
	 * The index into the ULinker's ExportMap for the next export in the linker's export hash table.
	 * Transient
	 */
	INT				_iHashNext;

	/**
	 * Set of flags/ attributes, e.g. including information whether the export was forced into the export 
	 * table via RF_ForceTagExp.
	 * Serialized
	 */
	EExportFlags	ExportFlags;

	/** If this object is a top level package (which must have been forced into the export table via RF_ForceTagExp)
	 * this contains the number of net serializable objects in each generation of that package
	 * Serialized
	 */
	TArray<INT>		GenerationNetObjectCount;

	/** If this object is a top level package (which must have been forced into the export table via RF_ForceTagExp)
	 * this is the GUID for the original package file
	 * Serialized
	 */
	FGuid			PackageGuid;

	/** If this object is a top level package (which must have been forced into the export table via RF_ForceTagExp)
	 * this is the package flags for the original package file
	 * Serialized
	 */
	DWORD			PackageFlags;

	/**
	 * Constructors
	 */
	FObjectExport();
	FObjectExport( UObject* InObject );
	
	/** I/O function */
	friend FArchive& operator<<( FArchive& Ar, FObjectExport& E );

	/**
	 * Retrieves export flags.
	 * @return Current export flags.
	 */
	FORCEINLINE EExportFlags GetFlags() const
	{
		return ExportFlags;
	}
	/**
	 * Sets the passed in flags
	 * @param NewFlags		Flags to set
	 */
	FORCEINLINE void SetFlags( EExportFlags NewFlags )
	{
		ExportFlags |= NewFlags;
	}
	/**
	 * Clears the passed in export flags.
	 * @param FlagsToClear	Flags to clear
	 */
	FORCEINLINE void ClearFlags( EExportFlags FlagsToClear )
	{
		ExportFlags &= ~FlagsToClear;
	}
	/**
	 * Used to safely check whether any of the passed in flags are set. This is required
	 * in case we extend EExportFlags to a 64 bit data type and keep UBOOL a 32 bit data 
	 * type so simply using GetFlags() & RF_MyFlagBiggerThanMaxInt won't work correctly 
	 * when assigned directly to an UBOOL.
	 *
	 * @param FlagsToCheck	Export flags to check for.
	 * @return				TRUE if any of the passed in flags are set, FALSE otherwise  (including no flags passed in).
	 */
	FORCEINLINE UBOOL HasAnyFlags( EExportFlags FlagsToCheck ) const
	{
		return (ExportFlags & FlagsToCheck) != 0 || FlagsToCheck == EF_AllFlags;
	}
	/**
	 * Used to safely check whether all of the passed in flags are set. This is required
	 * in case we extend EExportFlags to a 64 bit data type and keep UBOOL a 32 bit data 
	 * type so simply using GetFlags() & RF_MyFlagBiggerThanMaxInt won't work correctly 
	 * when assigned directly to an UBOOL.
	 *
	 * @param FlagsToCheck	Object flags to check for
	 * @return TRUE if all of the passed in flags are set (including no flags passed in), FALSE otherwise
	 */
	FORCEINLINE UBOOL HasAllFlags( EExportFlags FlagsToCheck ) const
	{
		return ((ExportFlags & FlagsToCheck) == FlagsToCheck);
	}
	/**
	 * Returns object flags that are both in the mask and set on the object.
	 *
	 * @param Mask	Mask to mask object flags with
	 * @return Objects flags that are set in both the object and the mask
	 */
	FORCEINLINE EExportFlags GetMaskedFlags( EExportFlags Mask ) const
	{
		return ExportFlags & Mask;
	}
};

/*-----------------------------------------------------------------------------
	FObjectImport.
-----------------------------------------------------------------------------*/

/**
 * UObject resource type for objects that are referenced by this package, but contained
 * within another package.
 */
struct FObjectImport : public FObjectResource
{
	/**
	 * The name of the package that contains the class of the UObject represented by this resource.
	 * Serialized
	 */
	FName			ClassPackage;

	/**
	 * The name of the class for the UObject represented by this resource.
	 * Serialized
	 */
	FName			ClassName;

	/**
	 * The UObject represented by this resource.  Assigned the first time CreateImport is called for this import.
	 * Transient
	 */
	UObject*		XObject;

	/**
	 * The linker that contains the original FObjectExport resource associated with this import.
	 * Transient
	 */
	ULinkerLoad*	SourceLinker;

	/**
	 * Index into SourceLinker's ExportMap for the export associated with this import's UObject.
	 * Transient
	 */
	INT             SourceIndex;

	/**
	 * Constructors
	 */
	FObjectImport();
	FObjectImport( UObject* InObject );
	
	/** I/O function */
	friend FArchive& operator<<( FArchive& Ar, FObjectImport& I );
};

/**
 * Information about a compressed chunk in a file.
 */
struct FCompressedChunk
{
	/** Default constructor, zero initializing all members. */
	FCompressedChunk();

	/** Original offset in uncompressed file.	*/
	INT		UncompressedOffset;
	/** Uncompressed size in bytes.				*/
	INT		UncompressedSize;
	/** Offset in compressed file.				*/
	INT		CompressedOffset;
	/** Compressed size in bytes.				*/
	INT		CompressedSize;

	/** I/O function */
	friend FArchive& operator<<(FArchive& Ar,FCompressedChunk& Chunk);
};

/*----------------------------------------------------------------------------
	Items stored in Unrealfiles.
----------------------------------------------------------------------------*/

/**
 * Revision data for an Unreal package file.
 */
//@todo: shouldn't need ExportCount/NameCount with the linker free package map; if so, clean this stuff up
struct FGenerationInfo
{
	/**
	 * Number of exports in the linker's ExportMap for this generation.
	 */
	INT ExportCount;

	/**
	 * Number of names in the linker's NameMap for this generation.
	 */
	INT NameCount;

	/** number of net serializable objects in the package for this generation */
	INT NetObjectCount;

	/** Constructor */
	FGenerationInfo( INT InExportCount, INT InNameCount, INT InNetObjectCount );

	/** I/O function
	 * we use a function instead of operator<< so we can pass in the package file summary for version tests, since Ar.Ver() hasn't been set yet
	 */
	void Serialize(FArchive& Ar, const struct FPackageFileSummary& Summary);
};

/**
 * Information about the textures stored in the package.
 */
struct FTextureAllocations
{
	/**
	 * Stores an export index for each texture of a certain type (size, format, etc).
	 */
	struct FTextureType
	{
		FTextureType();
		FTextureType( INT SizeX, INT SizeY, INT NumMips, DWORD Format, DWORD TexCreateFlags );

		/** Width of the largest mip-level stored in the package. */
		INT				SizeX;
		/** Height of the largest mip-level stored in the package. */
		INT				SizeY;
		/** Number of mips */
		INT				NumMips;
		/** Texture format */
		DWORD			Format;
		/** ETextureCreateFlags bit flags */
		DWORD			TexCreateFlags;
		/** Index into the package ExportMap. */
		TArray<INT>		ExportIndices;

		/** Not serialized. ResourceMems are constructed on load. */
		TArray<FTexture2DResourceMem*>	Allocations;
		/** Note serialized. Number of ExportIndices processed during load. */
		INT				NumExportIndicesProcessed;

		/**
		 * Checks whether all potential allocations for this TextureType have been considered yet (during load).
		 *
		 * @return TRUE if all allocations have been started
		 */
		UBOOL	HaveAllAllocationsBeenConsidered() const
		{
			return NumExportIndicesProcessed == ExportIndices.Num();
		}

		/**
		 * Serializes an FTextureType
		 */
		friend FArchive& operator<<( FArchive& Ar, FTextureAllocations::FTextureType& TextureType );
	};

	FTextureAllocations()
	:	PendingAllocationSize(0)
	,	NumTextureTypesConsidered(0)
	{
	}

	/** Array of texture types in the package. */
	TArray<FTextureType>	TextureTypes;
	/** Number of allocations that haven't been completed yet. */
	FThreadSafeCounter		PendingAllocationCount;
	/** Number of allocated bytes that has yet to be claimed by UTexture2D serialization. */
	INT						PendingAllocationSize;
	/** Number of TextureTypes that have been considered for potential allocations so far, during load. */
	INT						NumTextureTypesConsidered;

	/**
	 * Finds a suitable ResourceMem allocation, removes it from this container and returns it to the user.
	 *
	 * @param SizeX				Width of texture
	 * @param SizeY				Height of texture
	 * @param NumMips			Number of mips
	 * @param Format			Texture format (EPixelFormat)
	 * @param TexCreateFlags	ETextureCreateFlags bit flags
	 **/
	FTexture2DResourceMem*	FindAndRemove( INT SizeX, INT SizeY, INT NumMips, DWORD Format, DWORD TexCreateFlags );

	/**
	 * Cancels any pending ResourceMem allocation that hasn't been claimed by a texture yet,
	 * just in case there are any mismatches at run-time.
	 *
	 * @param bCancelEverything		If TRUE, cancels all allocations. If FALSE, only cancels allocations that haven't been completed yet.
	 */
	void	CancelRemainingAllocations( UBOOL bCancelEverything );

	/**
	 * Checks if all allocations that should be started have been started (during load).
	 *
	 * @return TRUE if all allocations have been started
	 */
	UBOOL	HaveAllAllocationsBeenConsidered() const
	{
		return NumTextureTypesConsidered == TextureTypes.Num();
	}

	/**
	 * Checks if all ResourceMem allocations has completed.
	 *
	 * @return TRUE if all ResourceMem allocations has completed
	 */
	UBOOL	HasCompleted() const
	{
		return PendingAllocationCount.GetValue() == 0;
	}

	/**
	 * Checks if all ResourceMem allocations have been claimed by a texture.
	 *
	 * @return TRUE if there are no more pending ResourceMem allocations waiting for a texture to claim it
	 */
	UBOOL	HasBeenFullyClaimed() const
	{
		return PendingAllocationSize == 0;
	}

	/**
	 * Serializes an FTextureType
	 */
	friend FArchive& operator<<( FArchive& Ar, FTextureAllocations::FTextureType& TextureAllocationType );

	/**
	 * Serializes an FTextureAllocations struct
	 */
	friend FArchive& operator<<( FArchive& Ar, FTextureAllocations& TextureAllocations );

	FTextureAllocations( const FTextureAllocations& Other );
	void operator=(const FTextureAllocations& Other);

private:
	/**
	 * Finds a texture type that matches the given specifications.
	 *
	 * @param SizeX				Width of the largest mip-level stored in the package
	 * @param SizeY				Height of the largest mip-level stored in the package
	 * @param NumMips			Number of mips
	 * @param Format			Texture format (EPixelFormat)
	 * @param TexCreateFlags	ETextureCreateFlags bit flags
	 * @return					Matching texture type, or NULL if none was found
	 */
	FTextureType*	FindTextureType( INT SizeX, INT SizeY, INT NumMips, DWORD Format, DWORD TexCreateFlags );

	/**
	 * Adds a dummy export index (-1) for a specified texture type.
	 * Creates the texture type entry if needed.
	 *
	 * @param SizeX				Width of the largest mip-level stored in the package
	 * @param SizeY				Height of the largest mip-level stored in the package
	 * @param NumMips			Number of mips
	 * @param Format			Texture format (EPixelFormat)
	 * @param TexCreateFlags	ETextureCreateFlags bit flags
	 */
	void			AddResourceMemInfo( INT SizeX, INT SizeY, INT NumMips, DWORD Format, DWORD TexCreateFlags );
};

/**
 * A "table of contents" for an Unreal package file.  Stored at the top of the file.
 */
struct FPackageFileSummary
{
	/**
	 * Magic tag compared against PACKAGE_FILE_TAG to ensure that package is an Unreal package.
	 */
	INT		Tag;

protected:
	/**
	 * The package file version number when this package was saved.
	 *
	 * Lower 16 bits stores the main engine version
	 * Upper 16 bits stores the licensee version
	 */
	INT		FileVersion;

public:
	/**
	 * Total size of all information that needs to be read in to create a ULinkerLoad. This includes
	 * the package file summary, name table and import & export maps.
	 */
	INT		TotalHeaderSize;

	/**
	 * The flags for the package
	 */
	DWORD	PackageFlags;

	/**
	 * The Generic Browser folder name that this package lives in
	 */
	FString	FolderName;

	/**
	 * Number of names used in this package
	 */
	INT		NameCount;

	/**
	 * Location into the file on disk for the name data
	 */
	INT 	NameOffset;

	/**
	 * Number of exports contained in this package
	 */
	INT		ExportCount;

	/**
	 * Location into the file on disk for the ExportMap data
	 */
	INT		ExportOffset;

	/**
	 * Number of imports contained in this package
	 */
	INT     ImportCount;

	/**
	 * Location into the file on disk for the ImportMap data
	 */
	INT		ImportOffset;

	/**
	* Location into the file on disk for the DependsMap data
	*/
	INT		DependsOffset;

	/**
	 * Location in the file on disk of the ImportGuids/ExportGuids data
	 */
	INT		ImportExportGuidsOffset;

	/**
	 * Number of ImportGuids entries (number FLevelGuids structs)
	 */
	INT		ImportGuidsCount;

	/**
	 * Number of ExportGuids entries (Guid/ExportIndex pairs)
	 */
	INT		ExportGuidsCount;

	/**
	 * Thumbnail table offset
	 */
	INT		ThumbnailTableOffset;

	/**
	 * Current id for this package
	 */
	FGuid	Guid;

	/**
	 * Data about previous versions of this package
	 */
	TArray<FGenerationInfo> Generations;

	/**
	 * Engine version this package was saved with.
	 */
	INT		EngineVersion;

	/**
	 * CookedContent version this package was saved with.
	 *
	 * This is used to make certain that the content in the Cooked dir is the correct cooked
	 * version.  So we can just auto cook content and it will do the correct thing.
	 */
	INT     CookedContentVersion;

	/**
	 * Flags used to compress the file on save and uncompress on load.
	 */
	DWORD	CompressionFlags;

	/**
	 * Value that is used to determine if the package was saved by Epic (or licensee) or by a modder, etc
	 */
	DWORD	PackageSource;

	/**
	 * Array of compressed chunks in case this package was stored compressed.
	 */
	TArray<FCompressedChunk> CompressedChunks;

	/**
	 * List of additional packages that are needed to be cooked for this package (ie streaming levels)
	 */
	TArray<FString>	AdditionalPackagesToCook;

	/**
	 * Information about the textures stored in the package.
	 */
	FTextureAllocations	TextureAllocations;

	/** Constructor */
	FPackageFileSummary();

	INT GetFileVersion() const
	{
		// This code is mirrored in the << operator for FPackageFileSummary found in UnLinker.cpp 
		return (FileVersion & 0xffff); 
	}

	INT GetFileVersionLicensee() const
	{
		return ((FileVersion >> 16) & 0xffff);
	}

	INT GetCookedContentVersion() const
	{
		return CookedContentVersion;
	}

	void SetFileVersions(INT Epic, INT Licensee)
	{
		FileVersion = ((Licensee << 16) | Epic);
	}

	/** I/O function */
	friend FArchive& operator<<( FArchive& Ar, FPackageFileSummary& Sum );
};

/*----------------------------------------------------------------------------
	ULinker.
----------------------------------------------------------------------------*/

/**
 * Manages the data associated with an Unreal package.  Acts as the bridge between
 * the file on disk and the UPackage object in memory for all Unreal package types.
 */
class ULinker : public UObject
{
	DECLARE_CLASS_INTRINSIC(ULinker,UObject,CLASS_Transient|0,Core)
	NO_DEFAULT_CONSTRUCTOR(ULinker)

	/** The top-level UPackage object for the package associated with this linker */
	UPackage*				LinkerRoot;

	/** Table of contents for this package's file */
	FPackageFileSummary		Summary;

	/** Names used by objects contained within this package */
	TArray<FName>			NameMap;

	/** Resources for all UObjects referenced by this package */
	TArray<FObjectImport>	ImportMap;

	/** Resources for all UObjects contained within this package */
	TArray<FObjectExport>	ExportMap;

	/** Mapping of exports to all imports they need (not in the ExportMap so it can be easily skipped over in seekfree packages) */
	TArray<TArray<INT> >	DependsMap;

	/** Mapping of guid to ExportMap index of all objects that are pointed to by external objects (when this is loaded, the objects dont exist yet) */
	// @todo: Lookup all objects when the Linker is about to be destroyed (on consoles this happens?)
	TMap<FGuid, INT>		ExportGuidsAwaitingLookup;

	/** The name of the file for this package */
	FString					Filename;

	/** Used to filter out exports that should not be created */
	EObjectFlags			_ContextFlags;

	/** The SHA1 key generator for this package, if active */
	FSHA1*					ScriptSHA;

#if PERF_TRACK_SERIALIZATION_PERFORMANCE || LOOKING_FOR_PERF_ISSUES
	/** Per linker serialization performance data */
	class FStructEventMap	SerializationPerfTracker;
#endif

	/** Constructor. */
	ULinker( UPackage* InRoot, const TCHAR* InFilename );

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	/**
	 * I/O function
	 * 
	 * @param	Ar	the archive to read/write into
	 */
	void Serialize( FArchive& Ar );

	/**
	 * Return the path name of the UObject represented by the specified import. 
	 * (can be used with StaticFindObject)
	 * 
	 * @param	ImportIndex	index into the ImportMap for the resource to get the name for
	 *
	 * @return	the path name of the UObject represented by the resource at ImportIndex
	 */
	FString GetImportPathName(INT ImportIndex);

	/**
	 * Return the path name of the UObject represented by the specified export.
	 * (can be used with StaticFindObject)
	 * 
	 * @param	ExportIndex				index into the ExportMap for the resource to get the name for
	 * @param	FakeRoot				Optional name to replace use as the root package of this object instead of the linker
	 * @param	bResolveForcedExports	if TRUE, the package name part of the return value will be the export's original package,
	 *									not the name of the package it's currently contained within.
	 *
	 * @return	the path name of the UObject represented by the resource at ExportIndex
	 */
	FString GetExportPathName(INT ExportIndex, const TCHAR* FakeRoot=NULL,UBOOL bResolveForcedExports=FALSE);

	/**
	 * Return the full name of the UObject represented by the specified import.
	 * 
	 * @param	ImportIndex	index into the ImportMap for the resource to get the name for
	 *
	 * @return	the full name of the UObject represented by the resource at ImportIndex
	 */
	FString GetImportFullName(INT ImportIndex);

	/**
	 * Return the full name of the UObject represented by the specified export.
	 * 
	 * @param	ExportIndex				index into the ExportMap for the resource to get the name for
	 * @param	FakeRoot				Optional name to replace use as the root package of this object instead of the linker
	 * @param	bResolveForcedExports	if TRUE, the package name part of the return value will be the export's original package,
	 *									not the name of the package it's currently contained within.
	 *
	 * @return	the full name of the UObject represented by the resource at ExportIndex
	 */
	FString GetExportFullName(INT ExportIndex, const TCHAR* FakeRoot=NULL,UBOOL bResolveForcedExports=FALSE);

	/** 
	 * This will return the name given an index.
	 *
	 * @script patcher
	 **/
	FName IndexToName( NAME_INDEX Index, INT Number ) const;

	/**
	 * Returns the index [into the ExportMap array] for the first export which has the EF_ScriptPatcherExport flag, or INDEX_NONE if there are none.
	 *
	 * @script patcher
	 */
	INT FindFirstPatchedExportIndex() const;

	/**
	 * Tell this linker to start SHA calculations
	 */
	void StartScriptSHAGeneration();

	/**
	 * If generating a script SHA key, update the key with this script code
	 *
	 * @param ScriptCode Code to SHAify
	 */
	void UpdateScriptSHAKey(const TArray<BYTE>& ScriptCode);

	/**
	 * After generating the SHA key for all of the 
	 *
	 * @param OutKey Storage for the key bytes (20 bytes)
	 */
	void GetScriptSHAKey(BYTE* OutKey);

	// UObject interface.
	void BeginDestroy();
};

/*----------------------------------------------------------------------------
	ULinkerLoad.
----------------------------------------------------------------------------*/

/**
 * Helper struct to keep track of all objects needed by an export (recursive dependency caching)
 */
struct FDependencyRef
{
	/** The Linker the export lives in */
	ULinkerLoad* Linker;

	/** Index into Linker's ExportMap for this object */
	INT ExportIndex;

	/**
	 * Comparison operator
	 */
	UBOOL operator==(const FDependencyRef& Other) const
	{
		return Linker == Other.Linker && ExportIndex == Other.ExportIndex;
	}

	/**
	 * Type hash implementation. Export indices are usually less than 100k, so are linker indices.
	 *
	 * @param	Ref		Reference to hash
	 * @return	hash value
	 */
	friend DWORD GetTypeHash( const FDependencyRef& Ref  );
};

/**
 * Handles loading Unreal package files, including reading UObject data from disk.
 */
class ULinkerLoad : public ULinker, public FArchive
{
	DECLARE_CLASS_INTRINSIC(ULinkerLoad,ULinker,CLASS_Transient|0,Core)
	NO_DEFAULT_CONSTRUCTOR(ULinkerLoad)

	// Friends.
	friend class UObject;
	friend class UPackageMap;
	friend struct FAsyncPackage;

	//@script patcher
	friend class UStruct;
	friend class FPatchReader;
	friend struct FLinkerPatchData;

	// Variables.
public:
	/** Flags determining loading behavior.																					*/
	DWORD					LoadFlags;
	/** Indicates whether the imports for this loader have been verified													*/
	UBOOL					bHaveImportsBeenVerified;
	/** Hash table for exports.																								*/
	INT						ExportHash[256];
	/** Bulk data that does not need to be loaded when the linker is loaded.												*/
	TArray<FUntypedBulkData*> BulkDataLoaders;
	/** The archive that actually reads the raw data from disk.																*/
	FArchive*				Loader;

#if !FINAL_RELEASE
	/** OldClassName to NewClassName for ImportMap */
	static TMap<FName, FName> ObjectNameRedirects;
	/** OldClassName to NewClassName for ExportMap */
	static TMap<FName, FName> ObjectNameRedirectsInstanceOnly;
	/** Object name to NewClassName for export map */
	static TMap<FName, FName> ObjectNameRedirectsObjectOnly;	
#endif

private:
	// Variables used during async linker creation.

	/** Current index into name map, used by async linker creation for spreading out serializing name entries.				*/
	INT						NameMapIndex;
	/** Current index into import map, used by async linker creation for spreading out serializing importmap entries.		*/	
	INT						ImportMapIndex;
	/** Current index into export map, used by async linker creation for spreading out serializing exportmap entries.		*/
	INT						ExportMapIndex;
	/** Current index into depends map, used by async linker creation for spreading out serializing dependsmap entries.		*/
	INT						DependsMapIndex;
	/** Current index into export hash map, used by async linker creation for spreading out hashing exports.				*/
	INT						ExportHashIndex;


	/** Whether we already serialized the package file summary.																*/
	UBOOL					bHasSerializedPackageFileSummary;
	/** Whether we already fixed up import map.																				*/
	UBOOL					bHasFixedUpImportMap;
	/** Whether we already matched up existing exports.																		*/
	UBOOL					bHasFoundExistingExports;
	/** Whether we are already fully initialized.																			*/
	UBOOL					bHasFinishedInitialization;
	/** Whether we we have remapped package names for ML cooks																*/
	UBOOL					bHasRemappedExternalPackageReferencesForMultilanguageCooks;

	//@{
	//@script patcher
	/** Whether this linker has already integrated any new names from a script patch */
	UBOOL					bHasIntegratedNamePatches;
	/** Whether this linker has already integrated any new imports from a script patch */
	UBOOL					bHasIntegratedImportPatches;
	/** Whether this linker has already integrated any new exports from a script patch */
	UBOOL					bHasIntegratedExportPatches;
	/** Whether this linker has already integrated any script bytecode patches from a script patch */
	UBOOL					bHasIntegratedScriptPatches;
	/** Whether this linker has already integrated any modified class defaults from a script patch */
	UBOOL					bHasIntegratedDefaultsPatches;
	/** Whether this linker has already integrated its enum value patches */
	UBOOL					bHasIntegratedEnumPatches;
	/** Whether this linker has already remapped its external package references for the script patcher */
	UBOOL					bHasRemappedExternalPackageReferences;

	/** Whether we are gathering dependencies, can be used to streamline VerifyImports, etc									*/
	UBOOL					bIsGatheringDependencies;
	//@}

	/** Whether time limit is/ has been exceeded in current/ last tick.														*/
	UBOOL					bTimeLimitExceeded;
	/** Call count of IsTimeLimitExceeded.																					*/
	INT						IsTimeLimitExceededCallCount;
	/** Whether to use a time limit for async linker creation.																*/
	UBOOL					bUseTimeLimit;
	/** Current time limit to use if bUseTimeLimit is TRUE.																	*/
	FLOAT					TimeLimit;
	/** Time at begin of Tick function. Used for time limit determination.													*/
	DOUBLE					TickStartTime;

	/** Used for ActiveClassRedirects functionality */
	UBOOL	bFixupExportMapDone;

	//@{
	//@script patcher
	/**
	 * pointer to the shared script patcher;  created the first time it's requested, never deleted
	 */
	static FScriptPatcher*	ScriptPatcher;

	/**
	 * Pointer to an archive which contains UObject data for any new exports which were added to this linker by the script patcher
	 * Only created if the script patcher adds exports to this linker.
	 */
	FPatchReader*			PatchDataAr;
protected:
	/** used to restore back to the original file loader from the patch loader */
	FArchive*				OriginalLoader;
private:
	/**
	 * Map of indexes [into the ExportMap array] to replacement bytecode for functions contained in this linker which are being patched.
	 */
	TMap<INT,FScriptPatchData*>	FunctionsToPatch;

	/**
	 * Map of indexes [into the ExportMap array] to replacement bytecode for default objects contained in this linker which are being patched.
	 */
	TMap<INT,FPatchData*> DefaultsToPatch;

	/**
	 * Map of indexes [into the ExportMap array] of enums which need their values patched.
	 */
	TMap<INT,FEnumPatchData*> EnumsToPatch;
	//@}

	/**
	 * Helper struct to keep track of background file reads
	 */
	struct FPackagePrecacheInfo
	{
		/** Synchronization object used to wait for completion of async read. Pointer so it can be copied around, etc */
		FThreadSafeCounter* SynchronizationObject;

		/** Memory that contains the package data read off disk */
		void* PackageData;

		/** Size of the buffer pointed to by PackageData */
		INT PackageDataSize;

		/**
		 * Basic constructor
		 */
		FPackagePrecacheInfo()
		: SynchronizationObject(NULL)
		, PackageData(NULL)
		, PackageDataSize(0)
		{
		}
		/**
		 * Destructor that will free the sync object
		 */
		~FPackagePrecacheInfo()
		{
			delete SynchronizationObject;
		}
	};

	/** Map that keeps track of any precached full package reads															*/
	static TMap<FString, FPackagePrecacheInfo> PackagePrecacheMap;


	/** Owner of a potential cross level reference - needed for setting the pointer after the level is loaded */
	UObject* PotentialCrossLevelOwner;

	/** Property in PotentialCrossLevelOwner for cross level reference - needed for setting the pointer after the level is loaded */
	const UProperty* PotentialCrossLevelProperty;

public:

#if !FINAL_RELEASE
	/**
	 * Add redirects to ULinkerLoad static map
	 */
	void CreateActiveRedirectsMap(const TCHAR* GEngineIniName);
#endif
	/**
	 * Locates the class adjusted index and its package adjusted index for a given class name in the import map
	 */
	UBOOL FindImportClassAndPackage( FName ClassName, PACKAGE_INDEX& ClassIdx, PACKAGE_INDEX& PackageIdx );
	
	/**
	 * Attempts to find the index for the given class object in the import list and adds it + its package if it does not exist
	 */
	UBOOL CreateImportClassAndPackage( FName ClassName, FName PackageName, PACKAGE_INDEX& ClassIdx, PACKAGE_INDEX& PackageIdx );

	/**
	 * Allows object instances to be converted to other classes upon loading a package
	 */
	UBOOL FixupExportMap();


	/**
 	 * Fills in the passed in TArray with the packages that are in its PrecacheMap
	 *
	 * @param TArray<FString> to be populated
	 */
	static void GetListOfPackagesInPackagePrecacheMap( TArray<FString>& ListOfPackages );

	/**
	 * Returns whether linker has finished (potentially) async initialization.
	 *
	 * @return TRUE if initialized, FALSE if pending.
	 */
	UBOOL HasFinishedInitializtion() const
	{
        return bHasFinishedInitialization;
	}

	/**
	 * If this archive is a ULinkerLoad or ULinkerSave, returns a pointer to the ULinker portion.
	 */
	virtual ULinker* GetLinker() { return this; }

	/**
	 * Creates and returns a ULinkerLoad object.
	 *
	 * @param	Parent		Parent object to load into, can be NULL (most likely case)
	 * @param	Filename	Name of file on disk to load
	 * @param	LoadFlags	Load flags determining behavior
	 *
	 * @return	new ULinkerLoad object for Parent/ Filename
	 */
	static ULinkerLoad* CreateLinker( UPackage* Parent, const TCHAR* Filename, DWORD LoadFlags );

	void Verify();

	FName GetExportClassPackage( INT i );
	FName GetExportClassName( INT i );
	virtual FString GetArchiveName() const;

	/**
	 * Recursively gathers the dependencies of a given export (the recursive chain of imports
	 * and their imports, and so on)

	 * @param ExportIndex Index into the linker's ExportMap that we are checking dependencies
	 * @param Dependencies Set of all dependencies needed
	 * @param bSkipLoadedObjects Whether to skip already loaded objects when gathering dependencies
	 */
	void GatherExportDependencies(INT ExportIndex, TSet<FDependencyRef>& Dependencies, UBOOL bSkipLoadedObjects=TRUE);

	/**
	 * Recursively gathers the dependencies of a given import (the recursive chain of imports
	 * and their imports, and so on)

	 * @param ImportIndex Index into the linker's ImportMap that we are checking dependencies
	 * @param Dependencies Set of all dependencies needed
	 * @param bSkipLoadedObjects Whether to skip already loaded objects when gathering dependencies
	 */
	void GatherImportDependencies(INT ImportIndex, TSet<FDependencyRef>& Dependencies, UBOOL bSkipLoadedObjects=TRUE);

	/**
	 * A wrapper around VerifyImportInner. If the VerifyImportInner (previously VerifyImport) fails, this function
	 * will look for a UObjectRedirector that will point to the real location of the object. You will see this if
	 * an object was renamed to a different package or group, but something that was referencing the object was not
	 * not currently open. (Rename fixes up references of all loaded objects, but naturally not for ones that aren't
	 * loaded).
	 *
	 * @param	i	The index into this package's ImportMap to verify
	 */
	void VerifyImport( INT i );
	
	/**
	 * Loads all objects in package.
	 *
	 * @param bForcePreload	Whether to explicitly call Preload (serialize) right away instead of being
	 *						called from EndLoad()
	 */
	void LoadAllObjects( UBOOL bForcePreload = FALSE );

	/**
	 * Returns the ObjectName associated with the resource indicated.
	 * 
	 * @param	ResourceIndex	location of the object resource
	 *
	 * @return	ObjectName for the FObjectResource at ResourceIndex, or NAME_None if not found
	 */
	FName ResolveResourceName( PACKAGE_INDEX ResourceIndex );
	
	/**
	 * Changes all references to external packages in the ImportMap and ExportMap to contain the current script patcher
	 * package suffix (e.g. _OriginalVer or _LatestVer).
	 *
	 * @script patcher
	 */
	void RemapLinkerPackageNames();

	/**
	 * Changes all references to external localized packages ExportMap to strip the language extension, i.e. _FRA
	 *
	 */
	void RemapLinkerPackageNamesForMultilanguageCooks();

	INT FindExportIndex( FName ClassName, FName ClassPackage, FName ObjectName, INT ExportOuterIndex );
	
	/**
	 * Function to create the instance of, or verify the presence of, an object as found in this Linker.
	 *
	 * @param ObjectClass	The class of the object
	 * @param ObjectName	The name of the object
	 * @param Outer			Optional outer that this object must be in (for finding objects in a specific group when there are multiple groups with the same name)
	 * @param LoadFlags		Flags used to determine if the object is being verified or should be created
	 * @param Checked		Whether or not a failure will throw an error
	 * @return The created object, or (UObject*)-1 if this is just verifying
	 */
	UObject* Create( UClass* ObjectClass, FName ObjectName, UObject* Outer, DWORD LoadFlags, UBOOL Checked );

	/** version of Create() using export/linker index for Outer instead of an object reference
	  * @param ObjectClass	The class of the object
	  * @param ObjectName	The name of the object
	  * @param OuterIndex	export/linker index of the Outer for this object (ROOTPACKAGE_INDEX if Outer is LinkerRoot)
	  * @param LoadFlags	Flags used to determine if the object is being verified or should be created
	  * @param Checked		Whether or not a failure will throw an error
	  */
	UObject* CreateByOuterIndex(UClass* ObjectClass, FName ObjectName, INT OuterIndex, DWORD LoadFlags, UBOOL Checked);

	/**
	 * Serialize the object data for the specified object from the unreal package file.  Loads any
	 * additional resources required for the object to be in a valid state to receive the loaded
	 * data, such as the object's Outer, Class, or ObjectArchetype.
	 *
	 * When this function exits, Object is guaranteed to contain the data stored that was stored on disk.
	 *
	 * @param	Object	The object to load data for.  If the data for this object isn't stored in this
	 *					ULinkerLoad, routes the call to the appropriate linker.  Data serialization is 
	 *					skipped if the object has already been loaded (as indicated by the RF_NeedLoad flag
	 *					not set for the object), so safe to call on objects that have already been loaded.
	 *					Note that this function assumes that Object has already been initialized against
	 *					its template object.
	 *					If Object is a UClass and the class default object has already been created, calls
	 *					Preload for the class default object as well.
	 */
	void Preload( UObject* Object );

	/**
	 * Before loading a persistent object from disk, this function can be used to discover
	 * the object in memory. This could happen in the editor when you save a package (which
	 * destroys the linker) and then play PIE, which would cause the Linker to be
	 * recreated. However, the objects are still in memory, so there is no need to reload
	 * them.
	 *
	 * @param ExportIndex	The index of the export to hunt down
	 * @return The object that was found, or NULL if it wasn't found
	 */
	UObject* FindExistingExport(INT ExportIndex);

	/**
	 * Builds a string containing the full path for a resource in the export table.
	 *
	 * @param OutPathName		[out] Will contain the full path for the resource
	 * @param ResourceIndex		Index of a resource in the export table
	 */
	void BuildPathName( FString& OutPathName, INT ExportIndex ) const;

	/**
	 * Checks if the specified export should be loaded or not.
	 * Performs similar checks as CreateExport().
	 *
	 * @param ExportIndex	Index of the export to check
	 * @return				TRUE of the export should be loaded
	 */
	UBOOL WillTextureBeLoaded( UClass* Class, INT ExportIndex );

	/**
	 * Kick off an async load of a package file into memory
	 * 
	 * @param PackageName Name of package to read in. Must be the same name as passed into LoadPackage/CreateLinker
	 */
	static void AsyncPreloadPackage(const TCHAR* PackageName);

	//@{
	//@script patcher
	/**
	 * Returns the archive that contains the object patch data for this linker.
	 */
	FPatchReader* GetScriptPatchArchive() const { return PatchDataAr; }

	/**
	 * Returns a bytecode patch given an index into the ExportMap.
	 *
	 * @param	ExportIndex		an index [into the ExportMap] for the patch to find
	 *
	 * @return	a pointer to the bytecode patch for the specified export, or NULL if that export doesn't have a bytecode patch
	 */
	FScriptPatchData* FindBytecodePatch( INT ExportIndex ) const;

	/**
	 * Returns an object data patch given an index into the ExportMap.
	 *
	 * @param	ExportIndex		an index [into the ExportMap] for the patch to find
	 *
	 * @return	a pointer to the object data patch for the specified export, or NULL if that export doesn't have a patch
	 */
	FPatchData* FindDefaultsPatch( INT ExportIndex ) const;

	/**
	 * Returns an enum patch given an index into the ExportMap
	 *
	 * @param	ExportIndex		an index [into the ExportMap] for the patch to find
	 *
	 * @return	a pointer to the enum value patch for the specified export, or NULL if that export doesn't have a patch
	 */
	FEnumPatchData* FindEnumPatch( INT ExportIndex ) const;
	//@}

	/**
	 * Called when an object begins serializing property data using script serialization.
	 */
	virtual void MarkScriptSerializationStart( const UObject* Obj );

	/**
	 * Called when an object stops serializing property data using script serialization.
	 */
	virtual void MarkScriptSerializationEnd( const UObject* Obj );

	/**
	 * Let the archive handle an object that could be a cross level reference.
	 *
	 * @param PropertyOwner The object that owns the property/pointer that is being serialized, not the destination object
	 * @param Property The property that describes the data being serialized
	 */
	virtual void WillSerializePotentialCrossLevelPointer(UObject* PropertyOwner, const UProperty* Property)
	{
		PotentialCrossLevelOwner = PropertyOwner;
		PotentialCrossLevelProperty = Property;
	}

	/**
	 * Resolves a cross level reference (as specified by the serialized LevelIndex and GuidIndex pair)
	 * into a UObject. If it fails, then the destination level probably hasn't been loaded yet, the
	 * the reference will need to be filled in later, when the level has been loaded
	 *
	 * @param LevelIndex Index of level in the package/linker's ImportGuids array
	 * @param GuidIndex Index of level in the level's Guids array (in ImportGuids(LevelIndex))
	 * @param PropertyOwner The object that owns the property/pointer that is being serialized, not the destination object
	 * @param Property The property that describes the data being serialized
	 *
	 * @return Object in another level, or NULL if invalid input or the other level isn't loaded yet
	 */
	UObject* ResolveCrossLevelReference(INT LevelIndex, INT GuidIndex, UObject* PropertyOwner, const UProperty* Property);

	/**
	 * Looks for an existing linker for the given package, without trying to make one if it doesn't exist
	 */
	static ULinkerLoad* FindExistingLinkerForPackage(UPackage* Package);

private:

	//@{
	/**
	 * Returns a pointer to the shared script patcher object.
	 */
	FScriptPatcher* GetScriptPatcher();

	/**
	 * Returns a pointer to the shared script patcher object if it already exists, NULL otherwise
	 */
	FScriptPatcher* GetExistingScriptPatcher();

	/**
	 * Creates a patch archive for this linker if one doesn't already exist.
	 */
	void CreatePatchReader();

	//@patchComment: added for patching
	/**
	 * Adds the specified names to the linker's NameMap
	 *
	 * @param	NewNames	the names to add
	 */
	void AppendNames( const TArray<FName>& NewNames );

	/**
	 * Adds the specified imports to the linker's ImportMap; won't add duplicates
	 *
	 * @param	Imports		the imports to add
	 */
	void AppendImports( const TArray<FObjectImport>& Imports );

	/**
	 * Adds the specified exports to the linker's ExportMap; won't add duplicates
	 *
	 * @param	Exports		the exports to add
	 */
	void AppendExports( const TArray<FObjectExport>& Exports, const TArray<struct FPatchData>& ExportPatchData );
	//@}

	UObject* CreateExport( INT Index );
	UObject* CreateImport( INT Index );
	UObject* IndexToObject( PACKAGE_INDEX Index );

	void DetachExport( INT i );

	// UObject interface.
	void BeginDestroy();

	// FArchive interface.
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
	virtual UBOOL Precache( INT PrecacheOffset, INT PrecacheSize );
	
	/**
	 * Attaches/ associates the passed in bulk data object with the linker.
	 *
	 * @param	Owner		UObject owning the bulk data
	 * @param	BulkData	Bulk data object to associate
	 */
	virtual void AttachBulkData( UObject* Owner, FUntypedBulkData* BulkData );
	/**
	 * Detaches the passed in bulk data object from the linker.
	 *
	 * @param	BulkData	Bulk data object to detach
	 * @param	bEnsureBulkDataIsLoaded	Whether to ensure that the bulk data is loaded before detaching
	 */
	virtual void DetachBulkData( FUntypedBulkData* BulkData, UBOOL bEnsureBulkDataIsLoaded );
	/**
	 * Detaches all attached bulk  data objects.
	 *
	 * @param	bEnsureBulkDataIsLoaded	Whether to ensure that the bulk data is loaded before detaching
	 */
	virtual void DetachAllBulkData( UBOOL bEnsureBulkDataIsLoaded );

	/**
	 * Detaches linker from bulk data/ exports and removes itself from array of loaders.
	 *
	 * @param	bEnsureAllBulkDataIsLoaded	Whether to load all bulk data first before detaching.
	 */
	virtual void Detach( UBOOL bEnsureAllBulkDataIsLoaded );


	void Seek( INT InPos );
	INT Tell();
	INT TotalSize();
	void Serialize( void* V, INT Length );
	virtual FArchive& operator<<( UObject*& Object );
	virtual FArchive& operator<<( FName& Name );

	/**
	 * Safely verify that an import in the ImportMap points to a good object. This decides whether or not
	 * a failure to load the object redirector in the wrapper is a fatal error or not (return value)
	 *
	 * @param	i				The index into this packages ImportMap to verify
	 * @param	WarningSuffix	[out] additional information about the load failure that should be appended to
	 *							the name of the object in the load failure dialog.
	 *
	 * @return TRUE if the wrapper should crash if it can't find a good object redirector to load
	 */
	UBOOL VerifyImportInner(INT i, FString& WarningSuffix);

	//
	// ULinkerLoad creation helpers BEGIN
	//

	/**
	 * Creates a ULinkerLoad object for async creation. Tick has to be called manually till it returns
	 * TRUE in which case the returned linker object has finished the async creation process.
	 *
	 * @param	Parent		Parent object to load into, can be NULL (most likely case)
	 * @param	Filename	Name of file on disk to load
	 * @param	LoadFlags	Load flags determining behavior
	 *
	 * @return	new ULinkerLoad object for Parent/ Filename
	 */
	static ULinkerLoad* CreateLinkerAsync( UPackage* Parent, const TCHAR* Filename, DWORD LoadFlags );
protected:
	/**
	 * Ticks an in-flight linker and spends InTimeLimit seconds on creation. This is a soft time limit used
	 * if bInUseTimeLimit is TRUE.
	 *
	 * @param	InTimeLimit		Soft time limit to use if bInUseTimeLimit is TRUE
	 * @param	bInUseTimeLimit	Whether to use a (soft) timelimit
	 * 
	 * @return	TRUE if linker has finished creation, FALSE if it is still in flight
	 */
	UBOOL Tick( FLOAT InTimeLimit, UBOOL bInUseTimeLimit );
	/**
	 * Private constructor, passing arguments through from CreateLinker.
	 *
	 * @param	Parent		Parent object to load into, can be NULL (most likely case)
	 * @param	Filename	Name of file on disk to load
	 * @param	LoadFlags	Load flags determining behavior
	 */
	ULinkerLoad( UPackage* InParent, const TCHAR* InFilename, DWORD InLoadFlags );
private:
	/**
	 * Returns whether the time limit allotted has been exceeded, if enabled.
	 *
	 * @param CurrentTask	description of current task performed for logging spilling over time limit
	 * @param Granularity	Granularity on which to check timing, useful in cases where appSeconds is slow (e.g. PC)
	 *
	 * @return TRUE if time limit has been exceeded (and is enabled), FALSE otherwise (including if time limit is disabled)
	 */
	UBOOL IsTimeLimitExceeded( const TCHAR* CurrentTask, INT Granularity = 1 );

	/**
	 * Creates loader used to serialize content.
	 */
	UBOOL CreateLoader();

	/**
	 * Serializes the package file summary.
	 */
	UBOOL SerializePackageFileSummary();

	/**
	 * Serializes the name map.
	 */
	UBOOL SerializeNameMap();

	/**
	 * Serializes the import map.
	 */
	UBOOL SerializeImportMap();

	/**
	 * Fixes up the import map, performing remapping for backward compatibility and such.
	 */
	UBOOL FixupImportMap();

	/**
	 * Changes classes for imports / exports before the objects are created.  The new class MUST be a child of the current class.
	 */
	UBOOL RemapClasses();

	/**
	 * Serializes the export map.
	 */
	UBOOL SerializeExportMap();

	/**
	 * Kicks off async memory allocations for all textures that will be loaded from this package.
	 */
	UBOOL StartTextureAllocation();

	/**
	 * Integrates any new names, imports and exports from the shared script patcher object
	 *
	 * @script patcher
	 */
	UBOOL IntegrateScriptPatches();

	/**
	 * Serializes the import map.
	 */
	UBOOL SerializeDependsMap();

	/**
	 * Serializes the Import/ExportGuids maps
	 */
	UBOOL SerializeGuidMaps();

public:
	/**
	 * Serializes thumbnails
	 */
	UBOOL SerializeThumbnails( UBOOL bForceEnableForCommandlet=FALSE );

private:
	/** 
	 * Creates the export hash.
	 */
	UBOOL CreateExportHash();

	/**
	 * Finds existing exports in memory and matches them up with this linker. This is required for PIE to work correctly
	 * and also for script compilation as saving a package will reset its linker and loading will reload/ replace existing
	 * objects without a linker.
	 */
	UBOOL FindExistingExports();

	/**
	 * Finalizes linker creation, adding linker to loaders array and potentially verifying imports.
	 */
	UBOOL FinalizeCreation();

	//
	// ULinkerLoad creation helpers END
	//
};

/*----------------------------------------------------------------------------
	ULinkerSave.
----------------------------------------------------------------------------*/

/**
 * Handles saving Unreal package files.
 */
class ULinkerSave : public ULinker, public FArchive
{
	DECLARE_CLASS_INTRINSIC(ULinkerSave,ULinker,CLASS_Transient|0,Core);
	NO_DEFAULT_CONSTRUCTOR(ULinkerSave);

	// Variables.
	/** The archive that actually writes the data to disk. */
	FArchive* Saver;

	/** Index array - location of the resource for a UObject is stored in the ObjectIndices array using the UObject's Index */
	TArray<PACKAGE_INDEX> ObjectIndices;

	/** Index array - location of the name in the NameMap array for each FName is stored in the NameIndices array using the FName's Index */
	TArray<INT> NameIndices;

	/** This will be set to TRUE right before an operator<< is called with a potential cross level reference */
	UBOOL bIsNextObjectSerializePotentialCrossLevelRef;

	/** A mapping of package name to generated script SHA keys */
	static TMap<FString, TArray<BYTE> > PackagesToScriptSHAMap;

	/** Constructor */
	ULinkerSave( UPackage* InParent, const TCHAR* InFilename, UBOOL bForceByteSwapping );
	/** Constructor for memory writer */
	ULinkerSave( UPackage* InParent, UBOOL bForceByteSwapping );
	void BeginDestroy();

	// FArchive interface.
	virtual INT MapName( const FName* Name ) const;
	virtual INT MapObject( const UObject* Object ) const;
	FArchive& operator<<( FName& InName )
	{
		INT Save = NameIndices(InName.GetIndex());
		INT Number = InName.GetNumber();
		FArchive& Ar = *this;
		return Ar << Save << Number;
	}
	FArchive& operator<<( UObject*& Obj );

	/**
	 * If this archive is a ULinkerLoad or ULinkerSave, returns a pointer to the ULinker portion.
	 */
	virtual ULinker* GetLinker() { return this; }

	void Seek( INT InPos );
	INT Tell();
	void Serialize( void* V, INT Length );

	/**
	 * Detaches file saver and hence file handle.
	 */
	void Detach();

	/*
	 * Let the archive handle an object that could be a cross level reference.
	 *
	 * @param PropertyOwner The object that owns the property/pointer that is being serialized, not the destination object
	 * @param Property The property that describes the data being serialized
	 */
	virtual void WillSerializePotentialCrossLevelPointer(UObject* PropertyOwner, const UProperty* Property)
	{
		bIsNextObjectSerializePotentialCrossLevelRef = TRUE;
	}
};


/*-----------------------------------------------------------------------------
	Lazy loading.
-----------------------------------------------------------------------------*/

/**
 * This will log out that an LazyArray::Load has occurred IFF 
 * #if defined(PERF_LOG_LAZY_ARRAY_LOADS) || LOOKING_FOR_PERF_ISSUES
 * is true.
 *
 **/
void LogLazyArrayPerfIssue();

/**
 * Flags serialized with the lazy loader.
 */
typedef DWORD ELazyLoaderFlags;

/**
 * Empty flag set.
 */
#define LLF_None					0x00000000
/**
 * If set, payload is [going to be] stored in separate file		
 */
#define	LLF_PayloadInSeparateFile	0x00000001
/**
 * If set, payload should be [un]compressed during serialization. Only bulk data that doesn't require 
 * any special serialization or endian conversion can be compressed! The code will simply serialize a 
 * block from disk and use the byte order agnostic Serialize( Data, Length ) function to fill the memory.
 */
#define	LLF_SerializeCompressed		0x00000002
/**
 * Mask of all flags
 */
#define	LLF_AllFlags				0xFFFFFFFF

