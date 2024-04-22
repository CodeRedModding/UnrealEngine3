/*=============================================================================
	UnPatchCommandlets.h:	Class declarations for unrealscript bytecode patch
							generation commandlet and helpers
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _PATCH_COMMANDLETS_H_
#define _PATCH_COMMANDLETS_H_

const TCHAR OriginalPackageSuffix[] = TEXT("_OriginalVer");
const TCHAR LatestPackageSuffix[] = TEXT("_LatestVer");

/**
 * Tracks two versions of the same object
 */
struct FObjectPair
{
	/** the name of the object represented by this pair */
	FString						ObjectName;

	/** the version of this object from the original file */
	UObject*					OriginalObject;

	/** the version of this object from the current file */
	UObject*					CurrentObject;

	/**
	 * the number of properties which were different between the two objects
	 * set from the result of an FObjectPairComparison
	 */
	INT							DifferenceCount;

	/**
	 * The index into the ExportMap for the ULinkerLoad which contains the CurrentObject.
	 * Only valid once the object's data has been exported (@see ExportModifiedDefaults)
	 */
	INT							CurrentLinkerIndex;

	UBOOL ShouldComparePropertyValue( UProperty* Property, UStruct* CurrentOwnerStruct, UStruct* OriginalOwnerStruct, UProperty** MatchingProperty=NULL );
	UBOOL PerformStructPropertyComparison( UStruct* OriginalStruct, UStruct* CurrentStruct, BYTE* OriginalValue, BYTE* CurrentValue );
	void PerformComparison();
	void DisplayDifferences( INT Indent = 0 );

	/** constructors */
	FObjectPair( const TCHAR* inObjectName )
	: ObjectName(inObjectName), OriginalObject(NULL), CurrentObject(NULL), DifferenceCount(0), CurrentLinkerIndex(INDEX_NONE)
	{}

	FObjectPair( UObject* inOriginal, UObject* inCurrent, INT inDifferenceCount=0 )
	: OriginalObject(inOriginal), CurrentObject(inCurrent), DifferenceCount(inDifferenceCount), CurrentLinkerIndex(INDEX_NONE)
	{
		check(OriginalObject);
		check(CurrentObject);

		ObjectName = OriginalObject->GetName();
		check(ObjectName == CurrentObject->GetName());
	}

	/**
	 * Copy constructor
	 */
	FObjectPair( const FObjectPair& Other )
	{
		ObjectName = Other.ObjectName;
		OriginalObject = Other.OriginalObject;
		CurrentObject = Other.CurrentObject;
		DifferenceCount = Other.DifferenceCount;
		CurrentLinkerIndex = Other.CurrentLinkerIndex;
	}
};

/**
 * Keeps track of two versions of the same struct.
 */
struct FStructPair
{
	/** the name of the struct represented by this pair */
	FString						StructName;

	/** the version of the struct from the original file */
	UStruct*					OriginalStruct;

	/** the version of the struct from the current file */
	UStruct*					CurrentStruct;

	/** 
	 * the child structs of this struct - discrepancies in the child structs between the original
	 * version and the new version are not supported
	 **/
	TScopedPointer<TMap<FString,FStructPair> >	ChildMap;

	/**
	 * Iterates over all structs contained within this one, placing them in the ChildMap.
	 */
	void CompileChildMap();

	/**
	 * Compares the fields and data values for the two structs.
	 *
	 * @param	Result	receives the results of the comparison.
	 */
	void PerformComparison( struct FComparisonResults& Result);
	void DisplayDifferences( INT Indent = 0 );

	/** Constructor */
	FStructPair( const TCHAR* inStructName )
	: StructName(inStructName), OriginalStruct(NULL), CurrentStruct(NULL), ChildMap(new TMap<FString,FStructPair>())
	{}

	/** Copy constructor */
	FStructPair( const FStructPair& Other ): ChildMap(new TMap<FString,FStructPair>())
	{
		StructName = Other.StructName;
		OriginalStruct = Other.OriginalStruct;
		CurrentStruct = Other.CurrentStruct;
	}

	/** Assignment operator */
	FStructPair& operator=(const FStructPair& Other)
	{
		StructName = Other.StructName;
		OriginalStruct = Other.OriginalStruct;
		CurrentStruct = Other.CurrentStruct;
		return *this;
	}

	virtual ~FStructPair()
	{ }
};

/**
 * Holds a reference to two versions of the same class.
 */
struct FClassPair : public FStructPair
{
	/** Constructor */
	FClassPair( const TCHAR* inClassName )
	: FStructPair(inClassName)
	{}
};

/**
 * Since we must unload the linker associated with the current version of the script package,
 * this data structure stores the information we needed from the linker.
 */
struct FPackageData
{
	/** this is the bare name of the package (e.g. Engine) */
	FString							PackageName;

	/** this is the filename of the package, including any path info (e.g. ../System/OriginalScript/Engine.u) */
	FString							FileName;

	/** these are copied directly from the linker */
	FPackageFileSummary				Summary;

	TArray<FName>					Names;
	TArray<FObjectImport>			Imports;
	TArray<FObjectExport>			Exports;

	/** Constructor */
	FPackageData()
	{}

	FPackageData( ULinkerLoad* Linker, const FString& InPackageName  )
	{
		Summary = Linker->Summary;

		PackageName = InPackageName;
		FileName = Linker->Filename;

		Names = Linker->NameMap;
		Imports = Linker->ImportMap;
		Exports = Linker->ExportMap;
	}
};

class FScriptPatchExporter : public FStringOutputDevice
{
public:
	TArray<FString> PatchClassNames;
	TArray<FString>	LoaderPatchClassNames;

	/** List of all package patch data for exporting to binary */
	TArray<FLinkerPatchData> AllPackagePatches;

	UBOOL bNonIntelByteOrder;

	void SaveToFile();

	FScriptPatchExporter( UBOOL bShouldByteSwapResults=FALSE )
	: bNonIntelByteOrder(bShouldByteSwapResults)
	{
	}
};

struct FComparisonResults
{
	FString					PackageName;
	TArray<FStructPair>		StructsAdded;
	TArray<FStructPair>		StructsRemoved;
	TArray<FStructPair>		StructsModified;
	TArray<FObjectPair>		DefaultObjectsModified;
	TArray<UEnum*>			ModifiedEnums;

	TArray<FName>			NamesAdded;
	
	TArray<FObjectImport>	ImportsAdded;
	TArray<FObjectExport>	ExportsAdded;

	INT						OriginalExportCount;
	ULinkerLoad*			PackageLinker;

	operator UBOOL()
	{
		return 
			StructsAdded.Num() || StructsRemoved.Num() || StructsModified.Num()
			|| NamesAdded.Num() || ImportsAdded.Num() || ExportsAdded.Num()
			|| DefaultObjectsModified.Num() || ModifiedEnums.Num();
	}

	void SetPackageName( const TCHAR* inPackageName )
	{
		PackageName = inPackageName;
	}

	void DisplayResults( INT Indent=0 );
	void ExportResults(FScriptPatchExporter& Writer);

	/**
	 * Add the results for this package to the given array
	 *
	 * AllPackagePatches [out] Array to add this package's patch data to
	 */
	void BuildStructureForBinary(TArray<FLinkerPatchData>& AllPackagePatches, UBOOL bNonIntelByteCode);
	/**
	 * Fill out an FPatchDataObject
	 *
	 * @todo comment
	 */
	void BuildObjectDataForBinary(FPatchData* PatchObject, INT ExportMapIndex);
	/**
	 * @todo comment
	 */
	void BuildScriptDataForBinary(FScriptPatchData* ScriptPatch, INT StructIndex, UBOOL bNonIntelByteCode);
	/**
	 * @todo comment
	 */
	void BuildEnumDataForBinary(FEnumPatchData* NewEnum, INT EnumIndex);

private:
	void ExportNewExports( FScriptPatchExporter& Writer );
	void ExportModifiedDefaults( FScriptPatchExporter& Writer );
	void ExportModifiedEnums( FScriptPatchExporter& Writer );
	void ExportObjectData( FScriptPatchExporter& Writer, INT ExportMapIndex );
	void ExportModifiedScript(FScriptPatchExporter& Writer);
	void ExportPackageData(FScriptPatchExporter& Writer);
};

/**
 * Compares two versions of a package and stores a list of the differences between them.
 */
struct FPackageComparison
{
	/** Information about the latest version of the package */
	FPackageData CurrentPackage;

	/** Information about the original version of the package */
	FPackageData OriginalPackage;

	/** Stores the differences between the original and current versions of the package */
	FComparisonResults	ComparisonResults;

	/** this is the bare name of the package (e.g. Engine) */
	FString PackageName;

	/**
	 * Builds a list of class pairs contained by this package and its corresponding original version.
	 * 
	 * @param	Classes		[out] map of classname to classes instances
	 */
	void CompileClassMap( TMap<FString,FClassPair>& Classes );

	/**
	 * Performs a comparison of the names, imports, exports, and function bytecode for two versions of a package.  Any differences are added to the comparison's
	 * internal lists which are used for generating the script patch header file.
	 */
	void ComparePackages();

	/**
	 * Displays a summary of the comparison results for this package to the console window.
	 */
	void DisplayComparisonResults();

	/**
	 * Writes the patch data (if any) for this package to the specified output device.
	 *
	 * @param	Writer	the output device to pipe the results to
	 *
	 * @return	TRUE if patch data was written to the output device.
	 */
	UBOOL OutputComparisonResults( FScriptPatchExporter& Writer );

	/**
	 * Displays a summary of contents of the specified package, such as number of imports, names, exports, etc.
	 */
	void DisplayPackageSummary( FPackageData& Package, FOutputDevice* OutputDevice );
	void DisplaySummaries( FOutputDevice* OutputDevice )
	{
		DisplayPackageSummary(CurrentPackage, OutputDevice);
		DisplayPackageSummary(OriginalPackage, OutputDevice);
	}

	/** Constructors */
	FPackageComparison( const FString& InPackageName )
	: PackageName(InPackageName)
	{}
	FPackageComparison( FPackageData& Current, FPackageData& Original )
	: CurrentPackage(Current)
	, OriginalPackage(Original)
	, PackageName(Original.PackageName)
	{
	}
};

/**
 * Helper class for the UPatchScriptCommandlet class; this class actually performs all the work of the commandlet.
 */
class FScriptPatchWorker
{
public:
	/** Parsed from the command-line */
	TArray<FString>		Tokens, Switches;

	/** the names of the native packages which will never be unloaded */ 
	TArray<FString>		NativeScriptPackageNames;

	/** the names script packages which contain no native classes; the classes in these packages will always be baked into the maps they're used by */ 
	TArray<FString>		NonNativeScriptPackageNames;

	/** the list of package names that will be patched; only those packages found in both locations will be patched */
	TArray<FString>		PackageNamesToPatch;

	/** an array of pathnames to the packages being patched */
	TArray<FFilename>	LatestPackagePathnames, OriginalPackagePathnames;

	/**
	 * Because of the interdependencies between script packages, comparisons for native packages are generated and exported
	 * in a single pass, rather than per-package
	 */
	TArray<struct FPackageComparison*>	NativePackageComparisons;

	/** quick lookup for intrinsic classes, per package */
	TMultiMap<FName,UClass*>			IntrinsicClassMap;

	/** Remember the name of the target platform's engine .ini file */
	TCHAR				PlatformEngineConfigFilename[1024];

	/** Remember the name of the target platform's systemsettings .ini file	*/
	TCHAR				PlatformSystemSettingsConfigFilename[1024];

	/** indicates that we're generating a script patch for a non-intel based platform */
	UBOOL				bNonIntelScriptPatch;

	/** indicated that we're generating a script patch for a stripped platform */
	UBOOL				bStrippedScriptPatch;

	/** Constructor */
	FScriptPatchWorker();

	/**
	 * Main entry point.
	 */
	INT Main( const FString& Params );

	/**
	 * Parses the specified array of tokens to determine the operational parameters to use for running the commandlet.
	 *
	 * @return	TRUE if all parameters were parsed successfully and the specified paths and package name/wildcard corresponded to valid files
	 */
	UBOOL InitCommandlet();

	/**
	 *  Sets the platform that we are generating the patch for
	 *  @Params commandline parameters to parse for the platform specified 
	 */
	UBOOL SetPlatform(const FString& Params);

	/**
	 * Basically a wrapper for GFileManager->FindFiles(), which re-adds the path information to the resulting list of files. 
	 */
	UBOOL GetPackageList( const FString& Path, TArray<FFilename>& Filenames );

	/**
	 * Returns the index [into the NativePackageComparisons array] for the comparison of the specified package, or INDEX_NONE if it isn't found.
	 */
	INT FindNativePackageComparison( const FString PackageName ) const;

	void UpdatePackageCache( const TArray<FFilename>& PackagePathnames );
	void RemapLoadingToOriginalFiles();
	void RemapLoadingToLatestFiles();
	void ResetPackageRemapping();
	void MoveIntrinsicClasses( FName PackageName=NAME_None, UPackage* TargetOuter=NULL );

	void GenerateNativePackageComparisons( FScriptPatchExporter& Writer );
	void GenerateNonNativePackageComparisons( FScriptPatchExporter& Writer );

	/**
	 * Determines whether the specified package is contained in the NativeScriptPackageNames array.
	 *
	 * @param	PackageName		the pathname for the package to check; all path and extension information will be removed before the search.
	 */
	UBOOL IsNativeScriptPackage( FFilename PackageName ) const;

	/**
	 * Determines whether the specified package is contained in the NonNativeScriptPackageNames array.
	 *
	 * @param	PackageName		the pathname for the package to check; all path and extension information will be removed before the search.
	 */
	UBOOL IsNonNativeScriptPackage( FFilename PackageName ) const;
};


#endif // _PATCH_COMMANDLETS_H_

