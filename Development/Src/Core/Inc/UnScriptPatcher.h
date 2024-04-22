/*=============================================================================
	UnScriptPatcher.h: Script patcher and helper class declarations 
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNSCRIPTPATCHER_H__
#define __UNSCRIPTPATCHER_H__

#if SUPPORTS_SCRIPTPATCH_LOADING

/**
 * Contains info about a UObject patch that needs to be applied to the image at runtime.
 */
struct FPatchData
{
	/** the name associated with this data */
	FString	DataName;

	/** raw binary data for this patch - might be UObject data or bytecode */
	TArray<BYTE> Data;

	/** Destructor */
	virtual ~FPatchData() {}

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FPatchData& Patch );
};

/**
 * This data structure is used for script function bytecode patches.  Provides quick access to the name of the class that owns the function
 * so that linkers can quickly and easily determine whether a script patch should be applied. 
 */
struct FScriptPatchData : public FPatchData
{
	FName StructName;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FScriptPatchData& Patch );
};

struct FEnumPatchData
{
	/** the name of the enum being patched; used for quickly eliminating enums which do not need to be patched */
	FName EnumName;

	/** the full pathname of the enum */
	FString EnumPathName;

	/** the list of names to replace this enum with */
	TArray<FName> EnumValues;

	/** Constructor */
	FEnumPatchData()
	: EnumName(NAME_None)
	{}

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FEnumPatchData& EnumData);
};

/**
* Archive for reading 
*/
class FPatchBinaryReader : public FMemoryReader
{
public:
	/**
	* Constructor
	*/
	FPatchBinaryReader(TArray<BYTE>& PatchMemory);

	/**
	* Override FName serializer to use strings
	*/
	virtual FArchive& operator<<(FName& N);
};

/**
 * Archive for reading 
 */
class FPatchBinaryWriter : public FMemoryWriter
{
public:
	/**
	 * Constructor
	 */
	FPatchBinaryWriter(TArray<BYTE>& PatchMemory);

	/**
	 * Override FName serializer to use strings
	 */
	virtual FArchive& operator<<(FName& N);
};

/**
 * Simple archive for serializing UObject data from memory
 */
class FPatchReader : public FArchive
{
public:
	/** Constructors */
	FPatchReader()
	: FArchive(), Offset(0), Loader(NULL)
	{
		ArIsLoading = ArIsTransacting = 1;
	}
	FPatchReader( FPatchData& Patch )
	: FArchive(), Offset(0), Loader(NULL)
	{
		ArIsLoading = ArIsTransacting = 1;

		AddPatch(Patch);
	}

	/**
	 * Adds a new source patch to this archive
	 * 
	 * @param	Patch	the patch to add to this archive
	 */
	void AddPatch( const FPatchData& Patch )
	{
		Bytes += Patch.Data;
	}

	void SetLoader( ULinkerLoad* inLoader )
	{
		Loader = inLoader;
	}

	/** FArchive interface */
	INT TotalSize()
	{
		return Bytes.Num();
	}
	void Seek( INT InPos )
	{
		Offset = InPos;
	}
	INT Tell()
	{
		return Offset;
	}

	// these are used to grab data from the IN MEMORY Loader instead of the "on the disk" loader
	FArchive& operator<<( class FName& Name )
	{
		check(Loader);

		NAME_INDEX NameIndex;
		INT Number;
		*this << NameIndex;
		*this << Number;

		FName Temporary = Loader->IndexToName( NameIndex, Number );
		appMemcpy(&Name, &Temporary, sizeof(FName));

		return *this;
	}

	FArchive& operator<<(class UObject*& Obj)
	{
		check(Loader);

		INT Index;
		*this << Index;

		UObject* Temporary = Loader->IndexToObject( Index );
		appMemcpy(&Obj, &Temporary, sizeof(UObject*));

		return *this;
	}

	// Dunno why this is needed (since FArchive already has one for INT), but get build errors if it isn't here.
	FArchive& operator<<( INT& I )
	{
		ByteOrderSerialize( &I, sizeof(I) );
		return *this;
	}

	void Preload( UObject* Object )
	{
		if ( Loader != NULL )
			Loader->Preload(Object);
	}

private:
	void Serialize( void* Data, INT Num )
	{
		checkSlow(Offset+Num<=Bytes.Num());
		appMemcpy( Data, &Bytes(Offset), Num );
		Offset += Num;
	}

	/** the raw data contained by this archive */
	TArray<BYTE>	Bytes;

	/** the current position of the reader */
	INT				Offset;

	/** the linker that is currently using this reader for serialization */
	ULinkerLoad*	Loader;

};

struct FLinkerPatchData
{
	/** the name of the package that this patch is used for */
	FName					PackageName;

	/**
	 * list of names that will be added to the linker's NameMap
	 */
	TArray<FName>			Names;

	/**
	 * the list of exports that will be added to the linker's ExportMap
	 */
	TArray<FObjectExport>	Exports;

	/**
	 * The list of imports that will be added to the linker's ImportMap.
	 */
	TArray<FObjectImport>	Imports;

	/**
	 * The list of raw object data associated with newly added exports 
	 */
	TArray<FPatchData>		NewObjects;

	/**
	 * An array of updated default property values; each element corresponds to a different CDO that has been changed.
	 */
	TArray<FPatchData>		ModifiedClassDefaultObjects;

	/**
	 * An array of enums contained in this package which have had their list of values patched
	 */
	TArray<FEnumPatchData>	ModifiedEnums;

	/**
	 * The list of updated bytecodes for all functions which were modified in this package.
	 */
	TArray<FScriptPatchData>	ScriptPatches;

	//@msewTodo  fill out the BuildExport
	static FObjectExport BuildExport( FName ObjectName, EObjectFlags ObjectFlags, EExportFlags ExportFlags, PACKAGE_INDEX ClassIndex, PACKAGE_INDEX SuperIndex, PACKAGE_INDEX OuterIndex, PACKAGE_INDEX ArchetypeIndex )
	{
		FObjectExport Export;

		Export.ClassIndex = ClassIndex;
		Export.SuperIndex = SuperIndex;

		// msew added
		Export.ArchetypeIndex = ArchetypeIndex;

		Export.OuterIndex = OuterIndex;
		Export.ObjectName = ObjectName;

		Export.ObjectFlags = ObjectFlags;
		Export.ExportFlags = ExportFlags;

		Export._iHashNext = 0;
		Export.SerialOffset = 0;
		Export.SerialSize = 0;
		Export._Object = NULL;

		//@todo ronp - do we need these?
		/*
		GenerationNetObjectCount
		PackageGuid
		SerialSize
		ComponentMap
		*/

		return Export;
	}
	//@msewTodo  fill out the BuildImport
	static FObjectImport BuildImport( FName ObjectName, FName ClassName, FName ClassPackage, PACKAGE_INDEX OuterIndex, PACKAGE_INDEX SourceIndex )
	{
		FObjectImport Import;

		Import.ObjectName = ObjectName;
		Import.ClassName = ClassName;
		Import.ClassPackage = ClassPackage;
		Import.OuterIndex = OuterIndex;
		Import.SourceIndex = SourceIndex;

		Import.XObject = NULL;
		Import.SourceIndex = INDEX_NONE;
		Import.SourceLinker = NULL;

		return Import;
	}

	~FLinkerPatchData()
	{
		ScriptPatches.Empty();
		NewObjects.Empty();
		ModifiedClassDefaultObjects.Empty();
		ModifiedEnums.Empty();
	}

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FLinkerPatchData& LinkerData );
};

/**
 * Class for replacing in-memory bytecode and supplying property data for new exports.
 */
struct FScriptPatcher
{
public:

	/**
	 * Constructor
	 */
	FScriptPatcher();
	~FScriptPatcher();

	/**
	 * Retrieves the patch data for the specified package.
	 *
	 * @param	PackageName			the package name to retrieve a patch for
	 * @param	out_LinkerPatch		receives the patch associated with the specified package
	 *
	 * @return	TRUE if the script patcher contains a patch for the specified package.
	 */
	UBOOL GetLinkerPatch( const FName& PackageName, FLinkerPatchData*& out_LinkerPatch );

	/**
	 * Frees the data associated with the patch data, the linker is done with it
	 * @param	PackageName			the package name to free the patch data for
	 */
	void FreeLinkerPatch( const FName& PackageName );

private:
	TArray<FLinkerPatchData*>	PackageUpdates;
};

#endif // SUPPORTS_SCRIPTPATCH_LOADING

static inline UBOOL IsPackageCookedForConsole(FArchive& Ar)
{
	return Ar.GetLinker() && (Ar.GetLinker()->LinkerRoot->PackageFlags & PKG_Cooked) && (GPatchingTarget & UE3::PLATFORM_Console); 
}

#endif // __UNSCRIPTPATCHER_H__
