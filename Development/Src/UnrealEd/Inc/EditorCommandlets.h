/*=============================================================================
	Editor.h: Unreal editor public header file.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_EDITOR_COMMANDLETS
#define _INC_EDITOR_COMMANDLETS

BEGIN_COMMANDLET(AnalyzeScript,UnrealEd)
	static UFunction* FindSuperFunction(UFunction* evilFunc);
END_COMMANDLET

BEGIN_COMMANDLET(AnalyzeContent,UnrealEd)
	void StaticInitialize();
END_COMMANDLET


/**
 * Contains stats about a single resource in a package file.
 */
struct FObjectResourceStat
{
	/** the complete path name for this resource */
	INT ResourceNameIndex;

	/** the name of the class for this resource */
	FName ClassName;

	/** the size of this resource, on disk */
	INT ResourceSize;

	/** Standard Constructor */
	FObjectResourceStat( FName InClassName, const FString& InResourceName, INT InResourceSize );

	/** Copy constructor */
	FObjectResourceStat( const FObjectResourceStat& Other )
	{
		ResourceNameIndex = Other.ResourceNameIndex;
		ClassName = Other.ClassName;
		ResourceSize = Other.ResourceSize;
	}
};

/**
 * A mapping of class name to the resource stats for objects of that class
 */
class FClassResourceMap : public TMultiMap<FName,FObjectResourceStat>
{
};

struct FPackageResourceStat
{
	/** the name of the package this struct contains resource stats for */
	FName				PackageName;

	/** the filename of the package; will be different from PackageName if this package is one of the loc packages */
	FName				PackageFilename;

	/** the map of 'Class name' to 'object resources of that class' for this package */
	FClassResourceMap	PackageResources;

	/**
	 * Constructor
	 */
	FPackageResourceStat( FName InPackageName )
	: PackageName(InPackageName)
	{ }

	/**
	 * Creates a new resource stat using the specified parameters.
	 *
	 * @param	ResourceClassName	the name of the class for the resource
	 * @param	ResourcePathName	the complete path name for the resource
	 * @param	ResourceSize		the size on disk for the resource
	 *
	 * @return	a pointer to the FObjectResourceStat that was added
	 */
	struct FObjectResourceStat* AddResourceStat( FName ResourceClassName, const FString& ResourcePathName, INT ResourceSize );
};

struct FKismetResourceStat
{
	/** the name of the kismet object this struct contains stats for */
	FName				ObjectName;

	/** the number of references to the kismet object */
	INT					ReferenceCount;

	/** array of files that reference this kismet object */
	TArray<FString>		ReferenceSources;

	FKismetResourceStat( FName InObjectName )
	: ObjectName(InObjectName), ReferenceCount(0)
	{ }

	FKismetResourceStat( FName InObjectName, INT InRefCount )
	: ObjectName(InObjectName), ReferenceCount(InRefCount)
	{ }
};
typedef TMap<FName,FKismetResourceStat> KismetResourceMap;


enum EReportOutputType
{
	/** write the results to the log only */
	OUTPUTTYPE_Log,

	/** write the results to a CSV file */
	OUTPUTTYPE_CSV,

	/** write the results to an XML file (not implemented) */
	OUTPUTTYPE_XML,
};

/**
 * Generates various types of reports for the list of resources collected by the AnalyzeCookedContent commandlet.  Each derived version of this struct
 * generates a different type of report.
 */
struct FResourceStatReporter
{
	EReportOutputType OutputType;

	/**
	 * Creates a report using the specified stats.  The type of report created depends on the reporter type.
	 *
	 * @param	ResourceStats	the list of resource stats to create a report for.
	 *
	 * @return	TRUE if the report was created successfully; FALSE otherwise.
	 */
	virtual UBOOL CreateReport( const TArray<struct FPackageResourceStat>& ResourceStats )=0;

	/** Constructor */
	FResourceStatReporter()
	: OutputType(OUTPUTTYPE_Log)
	{}

	/** Destructor */
	virtual ~FResourceStatReporter()
	{}
};

/**
 * This reporter generates a report on the disk-space taken by each asset type.
 */
struct FResourceStatReporter_TotalMemoryPerAsset : public FResourceStatReporter
{
	/**
	 * Creates a report using the specified stats.  The type of report created depends on the reporter type.
	 *
	 * @param	ResourceStats	the list of resource stats to create a report for.
	 *
	 * @return	TRUE if the report was created successfully; FALSE otherwise.
	 */
	virtual UBOOL CreateReport( const TArray<struct FPackageResourceStat>& ResourceStats );
};

/**
 * This reporter generates a report which displays objects which are duplicated into more than one package.
 */
struct FResourceStatReporter_AssetDuplication : public FResourceStatReporter
{
	/**
	 * Creates a report using the specified stats.  The type of report created depends on the reporter type.
	 *
	 * @param	ResourceStats	the list of resource stats to create a report for.
	 *
	 * @return	TRUE if the report was created successfully; FALSE otherwise.
	 */
	virtual UBOOL CreateReport( const TArray<struct FPackageResourceStat>& ResourceStats );
};

struct FResourceDiskSize
{
	FString ClassName;
	QWORD TotalSize;

	/** Default constructor */
	FResourceDiskSize( FName InClassName )
	: ClassName(InClassName.ToString()), TotalSize(0)
	{}

	/** Copy constructor */
	FResourceDiskSize( const FResourceDiskSize& Other )
	{
		ClassName = Other.ClassName;
		TotalSize = Other.TotalSize;
	}
};

BEGIN_COMMANDLET(AnalyzeCookedContent,UnrealEd)

	/**
	 * the list of packages to process
	 */
	TArray<FFilename> CookedPackageNames;

	/**
	 * the class, path name, and size on disk for all resources
	 */
	TArray<struct FPackageResourceStat> PackageResourceStats;

	/**
	 * Builds the list of package names to load
	 */
	void Init();

	/**
	 * Loads each package and adds stats for its exports to the main list of stats
	 */
	void AssembleResourceStats();

	/**
	 * Determines which report type is desired based on the command-line parameters specified and creates the appropriate reporter.
	 *
	 * @param	Params	the command-line parameters passed to Main
	 *
	 * @return	a pointer to a reporter which generates output in the desired format, or NULL if no valid report type was specified.
	 */
	struct FResourceStatReporter* CreateReporter( const FString& Params );

END_COMMANDLET

BEGIN_COMMANDLET(AnalyzeCookedPackages,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(MineCookedPackages,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(CheckForSimplifiedMeshes,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(AnalyzeFallbackMaterials,UnrealEd)
/**
* Encapsulates gathered stats for a particular UMaterial object.
*/
struct FMaterialStats
{
	/**
	* Stringifies gathered stats for generated fallbacks in CSV format.
	*
	* @return comma separated list of stats
	*/
	FString GeneratedFallbackToCSV() const;

	/**
	* Stringifies gathered stats for manually specified fallbacks with errors in CSV format.
	*
	* @return comma separated list of stats
	*/
	FString FallbackErrorsToCSV() const;

	/**
	* Returns a Generated Fallback header row for CSV
	*
	* @return comma separated header row
	*/
	static FString GetGeneratedFallbackCSVHeaderRow();

	/**
	* Returns a Fallback Error header row for CSV
	*
	* @return comma separated header row
	*/
	static FString GetFallbackErrorsCSVHeaderRow();

	/** Resource type.																*/
	FString ResourceType;
	/** Resource name.																*/
	FString ResourceName;
	/** Whether resource is referenced by script.									*/
	UBOOL bIsReferencedByScript;
	/** Compile errors from trying to cache the material's shaders.					*/
	TArray<FString> CompileErrors;
};

/**
* Retrieves/ creates material stats associated with passed in material.
*
* @warning: returns pointer into TMap, only valid till next time Set is called
*
* @param	Material	Material to retrieve/ create material stats for
* @return	pointer to material stats associated with material
*/
FMaterialStats* GetMaterialStats( UMaterial* Material );

/**
* Handles encountered object, routing to various sub handlers.
*
* @param	Object			Object to handle
* @param	LevelPackage	Currently loaded level package, can be NULL if not a level
* @param	bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleObject( UObject* Object, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

/**
* Handles gathering stats for passed in material.
*
* @param Material		Material to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleMaterial( UMaterial* Material, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

/** Mapping from a fully qualified resource string (including type) to material stats info.									*/
TMap<FString,FMaterialStats> ResourceNameToMaterialStats;
END_COMMANDLET


BEGIN_COMMANDLET(BatchExport,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(ExportLoc,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(CompareLoc,UnrealEd)
	
	/**
	 * Contains information about a single localization file, any language.
	 */
	struct FLocalizationFile
	{
	private:
		/**
		 * The filename for the FConfigFile this FLocalizationFile represents.
		 */
		FFilename LocFilename;

		/** sections that do not exist in the counterpart file. */
		TArray<FString> UnmatchedSections;

		/** properties that are missing from the corresponding section in the other file */
		TArray<FString> UnmatchedProperties;

		/** properties that have identical values in the other file */
		TArray<FString> IdenticalProperties;

		/** the FConfigFile which contains the data for this loc file */
		FConfigFile* LocFile;

	public:

		/**
		 * Standard constructor
		 */
		FLocalizationFile( const FString& InPath );

		/** Copy ctor */
		FLocalizationFile( const FLocalizationFile& Other );

		/** Dtor */
		~FLocalizationFile();

		/**
		 * Determines whether this file is the counterpart for the loc file specified
		 */
		UBOOL IsCounterpartFor( const FLocalizationFile& Other ) const;

		/**
		 * Compares the data in this loc file against the data in the specified counterpart file, placing the results in the various tracking arrays.
		 */
		void CompareToCounterpart( FLocalizationFile* Other );

		/** Accessors */
		const FString GetFullName()			const	{ return LocFilename; }
		const FString GetDirectoryName()	const	{ return LocFilename.GetPath(); }
		const FString GetFilename()			const	{ return LocFilename.GetBaseFilename(); }
		const FString GetExtension()		const	{ return LocFilename.GetExtension(); }
		class FConfigFile* GetFile()		const	{ return LocFile; }

		void GetMissingSections( TArray<FString>& out_Sections ) const;
		void GetMissingProperties( TArray<FString>& out_Properties ) const;
		void GetIdenticalProperties( TArray<FString>& out_Properties ) const;
	};

	/**
	 * Contains information about a localization file and its english counterpart.
	 */
	struct FLocalizationFilePair
	{
		FLocalizationFile* EnglishFile, *ForeignFile;

		/** Default ctor */
		FLocalizationFilePair() : EnglishFile(NULL), ForeignFile(NULL) {}
		~FLocalizationFilePair();

		/**
		 * Compares the two loc files against each other.
		 */
		void CompareFiles();

		/**
		 * Builds a list of files which exist in the english directory but don't have a counterpart in the foreign directory.
		 */
		void GetMissingLocFiles( TArray<FString>& Files );

		/**
		 * Builds a list of files which no longer exist in the english loc directories.
		 */
		void GetObsoleteLocFiles( TArray<FString>& Files );

		/**
		 * Builds a list of section names which exist in the english version of the file but don't exist in the foreign version.
		 */
		void GetMissingSections( TArray<FString>& Sections );

		/**
		 * Builds a list of section names which exist in the foreign version but no longer exist in the english version.
		 */
		void GetObsoleteSections( TArray<FString>& Sections );

		/**
		 * Builds a list of key names which exist in the english version of the file but don't exist in the foreign version.
		 */
		void GetMissingProperties( TArray<FString>& Properties );

		/**
		 * Builds a list of section names which exist in the foreign version but no longer exist in the english version.
		 */
		void GetObsoleteProperties( TArray<FString>& Properties );

		/**
		 * Builds a list of property names which have the same value in the english and localized version of the file, indicating that the value isn't translated.
		 */
		void GetUntranslatedProperties( TArray<FString>& Properties );

		/**
		 * Assigns the english version of the loc file pair.
		 */
		UBOOL SetEnglishFile( const FString& EnglishFilename );

		/**
		 * Assigns the foreign version of this loc file pair.
		 */
		UBOOL SetForeignFile( const FString& ForeignFilename );

		/** returns the filename (without path or extension info) for this file pair */
		const FString GetFilename();
		UBOOL HasEnglishFile();
		UBOOL HasForeignFile();
		UBOOL HasEnglishFile( const FString& Filename );
		UBOOL HasForeignFile( const FString& Filename );
	};

	/**
	 * Returns the index of the loc file pair that contains the english version of the specified filename, or INDEX_NONE if it isn't found
	 */
	INT FindEnglishIndex( const FString& Filename );

	/**
	 * Returns the index of the loc file pair that contains the english version of the specified filename, or INDEX_NONE if it isn't found
	 */
	INT FindForeignIndex( const FString& Filename );

	/**
	 * Adds the specified file as the english version for a loc file pair
	 */
	void AddEnglishFile( const FString& Filename );

	/**
	 * Adds the specified file as the foreign version for a loc file pair
	 */
	void AddForeignFile( const FString& Filename );

	/**
	 * Initializes the LocPairs arrays using the list of filenames provided.
	 */
	void ReadLocFiles( const TArray<FString>& EnglishFilenames, TArray<FString>& ForeignFilenames );

	FString LangExt;
	TArray<FLocalizationFilePair> LocPairs;

END_COMMANDLET

BEGIN_COMMANDLET(Conform,UnrealEd)
	void PerformAdditionalOperations( class UObject* Object );
END_COMMANDLET

BEGIN_COMMANDLET(LoadPackage,UnrealEd)
	/**
	 *	Parse the given load list file, placing the entries in the given Tokens array.
	 *
	 *	@param	LoadListFilename	The name of the load list file
	 *	@param	Tokens				The array to place the entries into.
	 *	
	 *	@return	UBOOL				TRUE if successful and non-empty, FALSE otherwise
	 */
	UBOOL ParseLoadListFile(FString& LoadListFilename, TArray<FString>& Tokens);
END_COMMANDLET

BEGIN_COMMANDLET(ConvertEmitters,UnrealEd)
END_COMMANDLET

/** Commandlet that will query the GAB for quarantined objects and attempt to delete them from packages */
BEGIN_COMMANDLET(DeleteQuarantinedContent,UnrealEd)

	/**
	 * Utility method for extracting asset name from from the GAD query data
	 *
	 * @param	AssetQueryData		The query string returned from the GAD
	 *	
	 * @return	FString				The asset name
	 */
	FString ExtractAssetName( const FString& AssetQueryData );


	/**
	 * Utility method for extracting the package name of an asset from the GAD query data
	 *
	 * @param	AssetQueryData		The query string returned from the GAD
	 *	
	 * @return	FString				The asset name
	 */
	FString ExtractPackageName( const FString& AssetQueryData );
	
	/**
	* Stores data about packages with quarantined objects
	*/
	struct FPackageHelper
	{
		FString PackageNames;
		FFilename PackageFilePath;
		TArray<FString> QuarantinedObjectNames;
	};

END_COMMANDLET

BEGIN_COMMANDLET(DumpEmitters,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(ConvertUberEmitters,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(FixupEmitters,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(FindEmitterMismatchedLODs,UnrealEd)
	void CheckPackageForMismatchedLODs( const FFilename& Filename, UBOOL bFixup );
END_COMMANDLET

BEGIN_COMMANDLET(FindEmitterModifiedLODs,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(FindEmitterModuleLODErrors,UnrealEd)
	UBOOL CheckModulesForLODErrors( INT LODIndex, INT ModuleIndex, 
		const UParticleEmitter* Emitter, const UParticleModule* CurrModule );
	void CheckPackageForModuleLODErrors( const FFilename& Filename, UBOOL bFixup );
END_COMMANDLET

BEGIN_COMMANDLET(FixupRedirects,UnrealEd)
	/**
	 * Allows commandlets to override the default behavior and create a custom engine class for the commandlet. If
	 * the commandlet implements this function, it should fully initialize the UEngine object as well.  Commandlets
	 * should indicate that they have implemented this function by assigning the custom UEngine to GEngine.
	 */
	virtual void CreateCustomEngine();
END_COMMANDLET

BEGIN_COMMANDLET(FixupSourceUVs,UnrealEd)
	virtual void FixupUVs( INT step, FStaticMeshTriangle * RawTriangleData, INT NumRawTriangles );
	virtual UBOOL FindUV( const FStaticMeshTriangle * RawTriangleData, INT UVChannel, INT NumRawTriangles, const FVector2D &UV );
	virtual UBOOL ValidateUVChannels( const FStaticMeshTriangle * RawTriangleData, INT NumRawTriangles );
	virtual UBOOL CheckFixableType( INT step, const FStaticMeshTriangle * RawTriangleData, INT NumRawTriangles );
	virtual INT CheckFixable( const FStaticMeshTriangle * RawTriangleData, INT NumRawTriangles );
	virtual UBOOL CheckUVs( FStaticMeshRenderData * StaticMeshRenderData, const FStaticMeshTriangle * RawTriangleData );
END_COMMANDLET

BEGIN_COMMANDLET(CheckLightMapUVs,UnrealEd)
	void ProcessStaticMesh( UStaticMesh* InStaticMesh, TArray< FString >& InOutAssetsWithMissingUVSets, TArray< FString >& InOutAssetsWithBadUVSets, TArray< FString >& InOutAssetsWithValidUVSets );
END_COMMANDLET

BEGIN_COMMANDLET(TagCookedReferencedAssets,UnrealEd)
	/** The tokens from the command line */
	TArray<FString> Tokens;
	/** The switches from the command line */
	TArray<FString> Switches;
	/** The collection name to use */
	FString CollectionName;
	/** The platform being run for */
	UE3::EPlatformType Platform;
	/** The platform cooked data folder to examine */
	FString CookedDataPath;
	/** The list of assets to tag as 'On DVD' */
	TMap<FString,UBOOL> AssetAddList;
	/** If specified, the commandlet will log out various information */
	UBOOL bVerbose;
	/** If specified, skip the update of the collection */
	UBOOL bSkipCollection;
	/** If specified, clear the collection */
	UBOOL bClearCollection;
	/** Classes that should be rejected */
	TArray<FName> ClassesToSkip;

	/** Name of the DLC being processed. Empty if not processing DLC */
	FString DLCName;

	/**
	 *	Initializes the commandlet
	 *
	 *	@param	Params		The command-line for the app
	 *
	 *	@return	UBOOL		TRUE if successful, FALSE if not
	 */
	UBOOL Initialize(const FString& Params);

	/**
	 *	Generate the list of referenced assets for the given platform
	 *	This will utilize the contents of the Cooked<PLATFORM> folder
	 */
	void GenerateReferencedAssetsList();

	/**
	 *	Process the given file, adding it's assets to the list
	 *
	 *	@param	InFilename		The name of the file to process
	 *
	 */
	void ProcessFile(const FFilename& InFilename);

	/**
	 *	Update the collections
	 */
	void UpdateCollections();
END_COMMANDLET

BEGIN_COMMANDLET(TagReferencedAssets,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(CreateStreamingWorld,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(ListPackagesReferencing,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(PkgInfo,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(ResavePackages,UnrealEd)
	/** only packages that have this version or higher will be resaved; a value of IGNORE_PACKAGE_VERSION indicates that there is no minimum package version */
	INT MinResaveVersion;

	/**
	 * Limits resaving to packages with this package version or lower.
	 * A value of IGNORE_PACKAGE_VERSION (default) removes this limitation.
	 */
	INT MaxResaveVersion;

	/**
	 * Limits resaving to packages with this licensee package version or lower.
	 * A value of IGNORE_PACKAGE_VERSION (default) removes this limitation.
	 */
	INT MaxResaveLicenseeVersion;

	/** 
	 * Maximum number of packages to resave to avoid having a massive sync
	 * A value of -1 (default) removes this limitation.
	 */
	INT MaxPackagesToResave;

	/** if true, updates all out-of-date Kismet objects */
	UBOOL bUpdateKismet;

	/** if true, only save packages that require sound conversion */
	UBOOL bSoundConversionOnly;
	UBOOL bSoundWasDirty;

	/** allows users to save only packages with a particular class in them (useful for fixing content) */
	TLookupMap<FName> ResaveClasses;

	/**
	 * Evalutes the command-line to determine which maps to check.  By default all maps are checked (except PIE and trash-can maps)
	 * Provides child classes with a chance to initialize any variables, parse the command line, etc.
	 *
	 * @param	Tokens			the list of tokens that were passed to the commandlet
	 * @param	Switches		the list of switches that were passed on the commandline
	 * @param	MapPathNames	receives the list of path names for the maps that will be checked.
	 *
	 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
	 */
	virtual INT InitializeResaveParameters( const TArray<FString>& Tokens, const TArray<FString>& Switches, TArray<FFilename>& MapPathNames );

	/**
	 * Allow the commandlet to perform any operations on the export/import table of the package before all objects in the package are loaded.
	 *
	 * @param	PackageLinker	the linker for the package about to be loaded
	 * @param	bSavePackage	[in]	indicates whether the package is currently going to be saved
	 *							[out]	set to TRUE to resave the package
	 */
	virtual UBOOL PerformPreloadOperations( ULinkerLoad* PackageLinker, UBOOL& bSavePackage );

	/**
	 * Allows the commandlet to perform any additional operations on the object before it is resaved.
	 *
	 * @param	Object			the object in the current package that is currently being processed
	 * @param	bSavePackage	[in]	indicates whether the package is currently going to be saved
	 *							[out]	set to TRUE to resave the package
	 */
	virtual void PerformAdditionalOperations( class UObject* Object, UBOOL& bSavePackage );

	/**
	 * Allows the commandlet to perform any additional operations on the package before it is resaved.
	 *
	 * @param	Package			the package that is currently being processed
	 * @param	bSavePackage	[in]	indicates whether the package is currently going to be saved
	 *							[out]	set to TRUE to resave the package
	 */
	void PerformAdditionalOperations( class UPackage* Package, UBOOL& bSavePackage );

	/**
	 * Removes any UClass exports from packages which aren't script packages.
	 *
	 * @param	Package			the package that is currently being processed
	 *
	 * @return	TRUE to resave the package
	 */
	UBOOL CleanClassesFromContentPackages( class UPackage* Package );

	/**
	 * Instances subobjects for any existing objects with subobject properties pointing to the default object.
	 * This is currently the case when a classes has an object property and subobject definition added to it --
	 * existing instances of such a class will see the new object property refer to the template object.
	 *
	 * @param	Package			The package that is currently being processed.
	 *
	 * @return					TRUE to resave the package.
	 */
	UBOOL InstanceMissingSubObjects(class UPackage* Package);

END_COMMANDLET

BEGIN_CHILD_COMMANDLET(ChangePrefabSequenceClass, ResavePackages, UnrealEd)

	/**
	 * Allow the commandlet to perform any operations on the export/import table of the package before all objects in the package are loaded.
	 *
	 * @param	PackageLinker	the linker for the package about to be loaded
	 * @param	bSavePackage	[in]	indicates whether the package is currently going to be saved
	 *							[out]	set to TRUE to resave the package
	 */
	virtual UBOOL PerformPreloadOperations( ULinkerLoad* PackageLinker, UBOOL& bSavePackage );

	/**
	 * Allows the commandlet to perform any additional operations on the object before it is resaved.
	 *
	 * @param	Object			the object in the current package that is currently being processed
	 * @param	bSavePackage	[in]	indicates whether the package is currently going to be saved
	 *							[out]	set to TRUE to resave the package
	 */
	virtual void PerformAdditionalOperations( class UObject* Object, UBOOL& bSavePackage );

END_CHILD_COMMANDLET

BEGIN_COMMANDLET(ScaleAudioVolume,UnrealEd)
END_COMMANDLET

/** Forward declaration of config cache class... */
class FConfigCacheIni;

// The interface for receiving notifications
class FCookerCleanupShaderCacheHandler : public FCallbackEventDevice
{
public:
	/**
	 * Constructor for the callback device
	 */
	FCookerCleanupShaderCacheHandler() : 
		FCallbackEventDevice()
	{
	}

	/**
	 * Destructor. Will unregister the callback
	 */
	~FCookerCleanupShaderCacheHandler();

	virtual void Send( ECallbackEventType InType, const FString& InString, UPackage* InPackage, UObject* InObject);
};

BEGIN_COMMANDLET(CookPackages,UnrealEd)
	/** The implementation of the PC-side support functions for the targetted platform. */
	class FConsoleSupport* ConsoleSupport;

	/** The callback handler for cleaning up the shader caches */
	FCookerCleanupShaderCacheHandler CleanupShaderCacheCallbackHandler;

	/** What platform are we cooking for?														*/
	UE3::EPlatformType				Platform;
	/** The shader type with which to compile shaders											*/
	EShaderPlatform					ShaderPlatform;
	
	/**
	 * Cooking helper classes for resources requiring platform specific cooking
	 */
	struct FConsoleTextureCooker*		TextureCooker;
	class FConsoleSoundCooker*			SoundCooker;
	struct FConsoleSkeletalMeshCooker*	SkeletalMeshCooker;
	struct FConsoleStaticMeshCooker*	StaticMeshCooker;

	/** The command-line tokens and switches */
	TArray<FString> Tokens;
	TArray<FString> Switches;

	/** Cooked data directory																	*/
	FString							CookedDir;

	/** Cooked data directory, in most cases the same as the above, but for child cook process, it will be a subdirectory															*/
	FString							CookedDirForPerProcessData;
	
	/** Whether to only cook dependencies of passed in packages/ maps							*/
	UBOOL							bOnlyCookDependencies;
	/** Whether to skip cooking maps.															*/
	UBOOL							bSkipCookingMaps;
	/** Whether to skip saving maps.															*/
	UBOOL							bSkipSavingMaps;
	/**	Whether to skip loading and saving maps not necessarily required like texture packages.	*/
	UBOOL							bSkipNotRequiredPackages;
	/** Always recook seekfree files, even if they haven't changed since cooking.				*/
	UBOOL							bForceRecookSeekfree;
	/** Only cook ini and localization files.													*/
	UBOOL							bIniFilesOnly;
	/** Generate SHA hashes.																	*/
	UBOOL							bGenerateSHAHashes;
	/** Should the cooker preassemble ini files and copy those to the Xbox						*/
	UBOOL							bShouldPreFinalizeIniFilesInCooker;
	/** TRUE to cook out static mesh actors														*/
	UBOOL							bCookOutStaticMeshActors;
	/** TRUE to cook out static light actors													*/
	UBOOL							bCookOutStaticLightActors;
	/** TRUE to bake and prune matinees that are tagged as such.								*/
	UBOOL							bBakeAndPruneDuringCook;
	/** TRUE to allow individual anim sets to override bake and prune.							*/
	UBOOL							bAllowBakeAndPruneOverride;
	/** Whether we are using the texture file cache for texture streaming.						*/
	UBOOL							bUseTextureFileCache;
	/** Alignment for bulk data stored in the texture file cache								*/
	DWORD							TextureFileCacheBulkDataAlignment;
	/** TRUE if we only want to cook maps, skipping the non-seekfree, script, startup, etc packages */
	UBOOL							bCookMapsOnly;
	/** TRUE if we want to only save the shader caches when we're done							*/
	UBOOL							bSaveShaderCacheAtEnd;
	/** Disallow map and package compression if option is set.									*/
	UBOOL							bDisallowPackageCompression;
	/** TRUE if we want to convert sRGB textures to a piecewise-linear approximation of sRGB	*/
	UBOOL							bShouldConvertPWLGamma;
	/** 
	 *	TRUE if we want to generate coalesced files for only the language being cooked for.		
	 *	Also, will not put the subtitles of other languages in the SoundNodeWaves!
	 *	Indicated by commandline option 'NOLOCCOOKING'
	 */
	UBOOL							bCookCurrentLanguageOnly;
	/** Language mask for multilanguage cooks, can't be zero for an actual multilanguage cook	*/
	DWORD							MultilanguageCookingMask;
	/** Language mask for text only multilanguage cooks
	*	Text only languages are always included in MultilanguageCookingMask, so
	*	(MultilanguageCookingMask & MultilanguageCookingMaskTextOnly) ==  MultilanguageCookingMaskTextOnly
	*/
	DWORD							MultilanguageCookingMaskTextOnly;
	/** 
	 *	If TRUE, saved cooked, non-int packages to language subdirectories
	 *  Comes from ini version: SaveLocalizedCookedPackagesInSubdirectories
	 */
	UBOOL							bSaveLocalizedCookedPackagesInSubdirectories;
	/** 
	 *	TRUE if we want to verify the texture file cache for all textures that get cooked.
	 *	Indicated by commandline option 'VERIFYTFC'
	 */
	UBOOL							bVerifyTextureFileCache;
	/** 
	* TRUE if we would like to write out a text file describing all items added to 
	texture file caches during cook
	*/
	UBOOL							bWriteTFCReport;
	/** 
	*	TRUE if we want to get analyzedreferencedcontent data during cooking.
	*	For now, we only do this for facefx only
	*	Indicated by commandline option 'ANALYZEREFERENCEDCONTENT'
	*/
	UBOOL							bAnalyzeReferencedContent;

	/**
	 * If true, brushes owned by volumes will be stripped out if they successfully generate physics data
	 */
	UBOOL bStripBrushesWithCachedPhysicsData;

	/** If TRUE, put lightmaps and shadowmaps in their own TFC */
	UBOOL bSeparateLightingTFC;
	/** If TRUE, put character textures in their own TFC */
	UBOOL bSeparateCharacterTFC;

	/** If TRUE, cleanup materials and clear unreferenced textures */
	UBOOL bCleanupMaterials;
	/** 
	 *	If TRUE, push materials onto StaticMeshComponents and clear static meshe refs to them. 
	 *	MeshEmitter ref'd StaticMeshes are left alone.
	 *	Enabled in *Editor.ini, [Cooker.GeneralOptions], bCleanStaticMeshMaterials=<true/false>
	 *
	 *	NOTE: If you spawn StaticMeshComponents dynamically, this could be a problem.
	 */
	UBOOL bCleanStaticMeshMaterials;

	/** 
	 *	If TRUE, skip the StaticSwitch inspection while cleaning materials. 
	 *	(This is intended to be temporary to help diagnose missing material-type issues during
	 *	the introduction of the new functionality)
	 *	-SKIPSTATICSWITCHCLEAN on commandline.
	 */
	UBOOL bSkipStaticSwitchClean;

	/** If TRUE, skip removing duplicate modules from ParticleSystems. -SKIPPSYSMODULES on commandline. */
	UBOOL bSkipPSysDuplicateModules;

	/** 
	 *	If TRUE, skip the identification of truly cooked startup objects.
	 *	Pass -SKIPSTARTUPOBJS on the commandline to enable.
	 */
	UBOOL bSkipStartupObjectDetermination;

	/** Flag indicating whether packages are being preloaded for dependency generation */
	UBOOL bPreloadingPackagesForDLC;

	/** 
	 *	Flag indicating that the FilterEditorOnly flag should be forced.
	 *	Intended for cooking for PCConsole that will be used by an executable
	 *	built with 
	 *	Pass -FORCEFILTEREDITORONLY on the commandline to activate this mode.
	 */
	UBOOL bForceFilterEditorOnly;

	/** Whether to remove directional lightmaps during cooking or not */
	UBOOL bRemoveDirectionalLightmaps;

	/** List of StaticMeshes that are script referenced - these will not have material cleanup performed on them */
	TMap<FString,UBOOL>				ScriptReferencedStaticMeshes;
	/** List of StaticMeshes that should be skipped when cleaning - from the ini section [Cooker.CleanStaticMeshMtrlSkip] */
	TMap<FString,UBOOL>				SkipStaticMeshCleanMeshes;
	/** List of Packages whose static meshes should be skipped when cleaning - from the ini section [Cooker.CleanStaticMeshMtrlSkip] */
	TMap<FString,UBOOL>				SkipStaticMeshCleanPackages;
	/** List of classes that should be checked for static mesh references which should be skipped when cleaning - from the ini section [Cooker.CleanStaticMeshMtrlSkip] */
	TMap<FString,UBOOL>				CheckStaticMeshCleanClasses;

	/** Array of names of editor-only packages.													*/
	TArray<FString>					EditorOnlyContentPackageNames;
	/** Detail mode of target platform.															*/
	INT								PlatformDetailMode;
	/** Archive used to write texture file cache.												*/
	TMap<FName,FArchive*>			TextureCacheNameToArMap;
	/** Filenames of texture cache Archives														*/
	TMap<FName,FString>				TextureCacheNameToFilenameMap;
	/** DLC name to cook for user-created mods and DLC											*/
	FString							DLCName;
	/** Set containing all packages that need to be cooked if bOnlyCookDependencies	== TRUE.	*/
	TMap<FString,INT>				PackageDependencies;
	/** Regular packages required to be cooked/ present.										*/
	TArray<FString>					RequiredPackages;
	/** Persistent data stored by the cooking process, e.g. used for bulk data separation.		*/
	UPersistentCookerData*			PersistentCookerData;
	/** Persistent shader data used for detecting material shader file changes. */
	FPersistentShaderData			PersistentShaderData;
	/** Guid cache saved by the cooker, used at runtime for looking up packages guids for packagemap */
	class UGuidCache*				GuidCache;
	/** LOD settings used on target platform.													*/
	FTextureLODSettings				PlatformLODSettings;
	/** Shader cache saved into seekfree packages.												*/
	UShaderCache*					ShaderCache;
	/** Set of materials that have already been put into an always loaded shader cache.			*/
 	TSet<FString>					AlreadyHandledMaterials;
	/** Set of material instances that have already been put into an always loaded shader cache. */
 	TSet<FString>					AlreadyHandledMaterialInstances;
	/** A list of files to generate SHA hash values for											*/
	TArray<FString>					FilesForSHA;
	/** Remember the name of the target platform's engine .ini file								*/
	TCHAR							PlatformEngineConfigFilename[1024];
	/** Remember the name of the target platform's systemsettings .ini file							*/
	TCHAR							PlatformSystemSettingsConfigFilename[1024];
	/** List of objects to NEVER cook into packages. */
	TMap<FString,INT>				NeverCookObjects;
	/** Maps a string representing staticmesh name / LOD index to tables containing vertex reorderings introduced when cooking for the PS3 */
	TMap<FString, TArray<INT> > StaticMeshVertexRemappings;
	/** Entries for verifying the texture file cache. */
	TMap<FString,FTextureFileCacheEntry>	TFCVerificationData;
	TArray<FTextureFileCacheEntry> TFCCheckData;
	TArray<FTextureFileCacheEntry> CharTFCCheckData;
	TArray<FTextureFileCacheEntry> LightingTFCCheckData;

	/** The tracked cooked startup objects */
	TMap<FString,UBOOL> TrackedCookedStartupObjects;
	TMap<FString,UBOOL> TrackedInitialMarkedObjects;
	TMap<FString,UBOOL> TrackedStartupPackageObjects;

	/** This is FaceFX AnimSet generator PER PERSISTENT MAP. */
	FPersistentFaceFXAnimSetGenerator		PersistentFaceFXAnimSetGenerator;

	/** Tracking Kismet-based items to cook out */
	TArray<UInterpData*> UnreferencedMatineeData;
	
	/** Data class for analyzedReferencedContent */
	FAnalyzeReferencedContentStat	ReferencedContentStat;

	/** Persistent Map Info tracking */
	FPersistentMapInfo PersistentMapInfoHelper;

	/** Archive for writing out script SHA hashes while cooking DLC/mods */
	FArchive* UserModeSHAWriter;

	//////
	/** If TRUE, we are doing a multithreaded cook and this is the master process */
	UBOOL bIsMTMaster;

	/** If TRUE, we are doing a multithreaded cook and this is a child process */
	UBOOL bIsMTChild;

	/** If TRUE, we skip cooking startup packages...this can leave some junk in maps, but not much */
	UBOOL bQuickMode;

	/** For child processes, this is a string of the form Process_XXXX where the xxx is the process id in hex */
	FFilename MTChildString; 
	
	/************************************************************************/
	/* FChildProcess														*/
	/* Structure the parent process maintains for each child process		*/	
	/************************************************************************/
	struct FChildProcess
	{
		/**  Childs temp working directory, relative to appBaseDir() */
		FFilename	Directory;
		/**  Childs log filename, absolute path */
		FFilename	LogFilename;
		/**  Filename of text file used to pass commands to this child process */
		FFilename	CommandFile;
		/**  Last command sent to this child process */
		FString		LastCommand;
		/**  Time that this command was sent to this child process */
		DOUBLE		StartTime;
		/**  Number of jobs this child has completed...used to determine if the child has completely started up*/
		INT			JobsCompleted;
		/**  If TRUE,, stop command has been sent to this child process*/
		UBOOL		bStopped;
		/**  If TRUE, this child has stopped and the results (TFC, GPCD, shader caches) have been merged */
		UBOOL		bMergedResults;
		/**  TFCTextureSaved the last time we synced with child; used to determine optional syncs */
		QWORD		LastTFCTextureSaved;
		/**  Process information returned from appCreateProc */
		void*		ProcessHandle;
		/**  Process ID returned from appCreateProc */
		DWORD		ProcessId;
	/**
	 * FChildProcess constructor
	 * just clears POD types to sensible values
	 *
	 */
		FChildProcess() :
			StartTime(0.0),
			JobsCompleted(0),
			bStopped(FALSE),
			bMergedResults(FALSE),
			LastTFCTextureSaved(0)
		{
		}

	};
	/**  Only used if bIsMTMaster is TRUE. Tracks information about child processes */
	TArray<FChildProcess> ChildProcesses;

	/** What format to cook for with Android */
	DWORD AndroidTextureFormat;

	/** 
	 *	For mobile platforms, the lightmap type to utilize.
	 *	Can be set per level via the WorldInfo.LightmapType property
	 */
	EPreferredLightmapType		PreferredLightmapType;

	/**
	 * On a heavily loaded machine, sometimes, files don't want to delete right away and we need to 
	 * make sure they are deleted for integrity, so this routine calls GFilemanger->Delete until it 
	 * succeeds or MTTimeout time is spent...if the timeout expires, we call appError
	 * During this time, it calls CheckForCrashedChildren repeatedly if we are the master process
	 *
	 * @param	Filename file to delete
	 */
	void WaitToDeleteFile(const TCHAR *Filename);

	/**
	 * A robust version of appSaveStringToFile
	 * we save to a temp file, then rename. This is important for synchronization, which is what this is used for
	 *
	 * @param	string to save
	 * @param	Filename file to save
	 */
	void RobustSaveStringToFile(const FString& String,const TCHAR *Filename);

	/**
	 * Save a persistent cooker data structure to a specific filename in a bullet proof manner
	 * calls reset loaders
	 * attempts to delete the destination
	 * leave the internal filename to Where
	 *
	 * @param	Who		UPersistentCookerData to save
	 * @param	Where	Filename to save to
	 */
	void BulletProofPCDSave(UPersistentCookerData* Who,const TCHAR *Where);

	/**
	 * Load a persistent cooker data structure from a specific filename in a bullet proof manner
	 * verifies the source file exists
	 *
	 * @param	Where	Filename to save to
	 * @return	Newly loaded PCD check()'d to be non-null
	 */
	UPersistentCookerData* BulletProofPCDLoad(const TCHAR *Where);

	/**
	 * Start all child processes for MT cooks
	 *  Only used if bIsMTMaster is TRUE. 
	 *
	 * @param	NumFiles		estimate of the number of files we have to process
	 * @return FALSE if there are not enough cores or jobs to bother with MT cooking
	 */
	UBOOL StartChildren(INT NumFiles);

	/**
	 * Sends stop commands to all chiuld processes that are idle but have not yet been stopped
	 *  Only used if bIsMTMaster is TRUE. 
	 *
	 * @return	TRUE if all children have been sent stop commands
	 */
	UBOOL SendStopCommands();

	/**
	 * Merges texture file caches, shader caches, guid caches and global persistent cooker data for a given child process
	 *  Only used if bIsMTMaster is TRUE. 
	 *
	 * @param	ProcessIndex index of the child process to merge
	 */
	void MergeChildProducts(INT ProcessIndex);

	/**
	 * Deletes all temp files for a given child process...essentially removes the process_XXX directory
	 *  Only used if bIsMTMaster is TRUE. 
	 *
	 * @param	ProcessIndex index of the child process to clean
	 */
	void CleanChild(INT ProcessIndex);

	/**
	 * Verifies that all child processes that have not been stoppped are still running, appErrors otherwise
	 *  Only used if bIsMTMaster is TRUE. 
	 *
	 */
	void CheckForCrashedChildren();

	/**
	 * Merges the logs from all of the children into my log
	 *  Only used if bIsMTMaster is TRUE. 
	 *
	 * @param CriticalReason if non-null, returns all critical errors found in the child logs
	 */
	void MergeLogs(FString *CriticalReason=NULL);

	/**
	 * Waits until all children are stopped, merges results and cleans the directories
	 *  Only used if bIsMTMaster is TRUE. 
	 *
	 */
	void StopChildren();

	/**
	 * Waits until a child process is available, then gives it the given job
	 *  Only used if bIsMTMaster is TRUE. 
	 *
	 */
	void StartChildJob(const FFilename &Job);

	/**
	 * Determine if a given child process is idle or not
	 *  Only used if bIsMTMaster is TRUE. 
	 *
	 * @param Job String representing the filename of the file to cook on the child process
	 * @return TRUE if the given child process is idle
	 */
	UBOOL ChildIsIdle(INT ProcessIndex);

	/**
	 * Saves our PCD data for a child process
	 *  Only used if bIsMTMaster is TRUE. 
	 *
	 */
	void SavePersistentCookerDataForChild(INT ProcessIndex);

	/**
	 * Checks to see if a child process is waiting for TFC synchronization, and if so, synchronizes the TFC with the child
	 *  Only used if bIsMTMaster is TRUE. 
	 * @return TRUE if the TFC was synchronized
	 */
	UBOOL CheckForTFCSync(INT ProcessIndex);

	/**
	 * Low level routine to send a given command string to a child
	 * Caller must verify that child is idle
	 *  Only used if bIsMTMaster is TRUE. 
	 *
	 * @param ProcessIndex	Index of child to send command to
	 * @param Command		String to send to child
	 */
	void SendChildCommand(INT ProcessIndex, const TCHAR *Command);

	/**
	 * Called by a child process to wait for the next command and return it
	 *  Only used if bIsMTChild is TRUE. 
	 *
	 * @return	The next job. Either a package name to cook or the empty filename if this is the stop command
	 */
	FFilename GetMyNextJob();

	/**
	 * Called by a child process to load the PCD the parent saved for us
	 * Done both when syncronizing TFCs and before each job
	 */
	void LoadParentPersistentCookerData();

	/**
	 * Called by a child process prior to saving a cooked package
	 *  Writes out the global persistent cooker data
	 *  Waits for the master process to digest the TFC infor and product a new PCD
	 *  Loads new PCD and fixes up texture offsets
	 *  Only used if bIsMTChild is TRUE. 
	 *
	 */
	void SynchronizeTFCs();

	/**
	 * Helper function for SynchronizeTFCs to synchronize the bulk data for a set of mips
	 *
	 * @param	Texture2D		the Texture to operate on
	 * @param	Mips			Reference to the actual collection of generated mips
	 * @param	TextureFormat	Which format is the mip data is in
	 */
	void SynchronizeTFCMips( UTexture2D* Texture2D, TIndirectArray<FTexture2DMipMap> &Mips, ETextureFormatSupport TextureFormat = TEXSUPPORT_DXT );

	/**
	 * Called by a child process to signal is has completed a job (or the stop command)
	 *  Only used if bIsMTChild is TRUE. 
	 *
	 */
	void JobCompleted();

	//////

	/**
	 * Cooks passed in object if it hasn't been already.
	 *
	 * @param	Package						Package going to be saved
	 * @param	 Object		Object to cook
	 * @param	bIsSavedInSeekFreePackage	Whether object is going to be saved into a seekfree package
	 */
	void CookObject( UPackage* Package, UObject* Object, UBOOL bIsSavedInSeekFreePackage );
	/**
	 * Helper function used by CookObject - performs level specific cooking.
	 *
	 * @param	Level						Level object to process
	 */
	void CookLevel( ULevel* Level );
	/**
	 * Helper function used by CookObject - performs texture specific cooking.
	 *
	 * @param	Package						Package going to be saved
	 * @param	Texture2D	Texture to cook
	 * @param	bIsSavedInSeekFreePackage	Whether object is going to be saved into a seekfree package
	 */
	void CookTexture( UPackage* Package, UTexture2D* Texture2D, UBOOL bIsSavedInSeekFreePackage );
	/**
	* Helper function used by CookObject - performs movie specific cooking.
	*
	* @param	TextureMovie	Movie texture to cook
	*/
	void CookMovieTexture( UTextureMovie* TextureMovie );

	/**
	 * Helper function used by CookObject - performs brush component specific cooking.
	 *
	 * @param	Package						Package going to be saved
	 * @param	BrushComponent				Brush component to cook
	 * @param	bIsSavedInSeekFreePackage	Whether object is going to be saved into a seekfree package
	 */
	void CookBrushComponent( UPackage* Package, UBrushComponent* BrushComponent, UBOOL bIsSavedInSeekFreePackage );

	/**
	 * Helper function used by CookObject - performs ParticleSystem specific cooking.
	 *
	 * @param	ParticleSystem	ParticleSystem to cook
	 */
	void CookParticleSystem(UParticleSystem* ParticleSystem);

	/**
	 * Helper function used by CookObject - performs SkeletalMesh specific cooking.
	 *
	 * @param	SkeletalMesh	SkeletalMesh to cook
	 */
	void CookSkeletalMesh(USkeletalMesh* SkeletalMesh);

	/**
	 * Helper function used by CookObject - performs StaticMesh specific cooking.
	 *
	 * @param	StaticMesh	StaticMesh to cook
	 */
	void CookStaticMesh(UStaticMesh* StaticMesh);

	/**
	 * Cooks out all static mesh actors in the specified package by re-attaching their StaticMeshComponents to
	 * a StaticMeshCollectionActor referenced by the world.
	 *
	 * @param	Package		the package being cooked
	 */
	void CookStaticMeshActors( UPackage* Package );

	/**
	 * Cooks out all static Light actors in the specified package by re-attaching their LightComponents to a 
	 * StaticLightCollectionActor referenced by the world.
	 */
	void CookStaticLightActors( UPackage* Package );

	/**
	 * Performs Landscape component specific cooking. This happens before any textures are cooked as it needs the heightmap data.
	 *
	 * @param	LandscapeComponent	LandscapeComponent to cook
	 */
	void CookLandscapeComponent(ULandscapeComponent* LandscapeComponent);

	/**
	 *	Clean up the kismet for the given level...
	 *	Remove 'danglers' - sequences that don't actually hook up to anything, etc.
	 *
	 *	@param	Package		The map being cooked
	 */
	void CleanupKismet(UPackage* Package);

	/** 
	 *	Get the referenced texture param list for the given material instance.
	 *
	 *	@param	MatInst					The material instance of interest
	 *	@param	RefdTextureParamsMap	Map to fill in w/ texture name-texture pairs
	 */
	void GetMaterialInstanceReferencedTextureParams(UMaterialInstance* MatInst, TMap<FName,UTexture*>& RefdTextureParamsMap);

	/**
	 *	Clean up the materials
	 *
	 *	@param	Package		The map being cooked
	 */
	void CleanupMaterials(UPackage* Package);

	/**
	 *	Bake and prune all matinee sequences that are tagged as such.
	 */
	void BakeAndPruneMatinee( UPackage* Package );

	/**
	 * Prepares object for saving into package. Called once for each object being saved 
	 * into a new package.
	 *
	 * @param	Package						Package going to be saved
	 * @param	Object						Object to prepare
	 * @param	bIsSavedInSeekFreePackage	Whether object is going to be saved into a seekfree package
	 * @param	bIsTextureOnlyFile			Whether file is only going to contain texture mips
	 */
	void PrepareForSaving(UPackage* Package, UObject* Object, UBOOL bIsSavedInSeekFreePackage, UBOOL bIsTextureOnlyFile);
	
	/**
	 * Called by PrepareForSaving to process a texture specifically
	 *
	 * @param	Package						Package going to be saved
	 * @param	Texture2D					Textire to prepare
	 * @param	bIsSavedInSeekFreePackage	Whether object is going to be saved into a seekfree package
	 * @param	bIsTextureOnlyFile			Whether file is only going to contain texture mips
	 * @param	bIsStreamingTexture			Whether the texture is streaming
	 * @param	bAllowStoreInSeparateFile	Whether or not the texture can be stored in a separate file
	 * @param	bHasMatchingGuidTFC			Whether or not the texture's GUID matches the one in the TFC
	 * @param	TextureFormat				Which texture format to prepare the texture for
	 */
	void PrepareTextureForSaving(UPackage* Package, UTexture2D* Texture2D, FCookedTextureFileCacheInfo* TextureFileCacheInfo, UBOOL bIsSavedInSeekFreePackage, UBOOL bIsTextureOnlyFile, UBOOL bIsDLC, UBOOL bIsStreamingTexture, const UBOOL bAllowStoreInSeparateFile, UBOOL bHasMatchingGuidTFC, ETextureFormatSupport TextureFromat = TEXSUPPORT_DXT );

	/**
	 * Helper function used by CookObject - performs sound cue specific cooking.
	 */
	void CookSoundCue( USoundCue* SoundCue );

	/**
	 * Helper function used by CookObject - performs sound specific cooking.
	 */
	void CookSoundNodeWave( USoundNodeWave* SoundNodeWave );

	/**
	* Helper function used by CookObject - performs FaceFX InterpTrack cooking.
	*/
	void CookFaceFXInterpTrack(UInterpTrackFaceFX* FaceFXInterpTrack);

	/**
	* Helper function used by CookObject - performs FaceFX Animset cooking
	*/
	void CookSeqActPlayFaceFXAnim(USeqAct_PlayFaceFXAnim* SeqAct_PlayFaceFX);

	/**
	* Helper function used by CookObject - performs FaceFX cooking.
	*/
	void LogFaceFXAnimSet(UFaceFXAnimSet* FaceFXAnimSet, UPackage* LevelPackage);

	/**
	 * Helper function used by CookSoundNodeWave - localises sound
	 */
	void LocSoundNodeWave( USoundNodeWave* SoundNodeWave );

	/**
	 * Make sure materials are compiled for Xbox 360 and add them to the shader cache embedded into seekfree packages.
	 * @param Material - Material to process
	 */
	void CompileMaterialShaders(UMaterial* Material);

	/**
	* Make sure material instances are compiled and add them to the shader cache embedded into seekfree packages.
	* @param MaterialInterface - MaterialInterface to process
	*/
	void CompileMaterialInstanceShaders(UMaterialInstance* MaterialInterface);

	/**
	 * Setup the commandlet's platform setting based on commandlet params
	 * @param Params The commandline parameters to the commandlet - should include "platform=xxx"
	 *
	 * @return TRUE if a good known platform was found in Params
	 */
	UBOOL SetPlatform(const FString& Params);

	/**
	 * Tried to load the DLLs and bind entry points.
	 *
	 * @return	TRUE if successful, FALSE otherwise
	 */
	UBOOL BindDLLs();

	/**
	* Update all game .ini files from defaults
	*
	* @param IniPrefix	prefix for ini filename in case we want to write to cooked path
	*/
	void UpdateGameIniFilesFromDefaults(const TCHAR* IniPrefix);

	/**
	 * Precreate all the .ini files that the platform will use at runtime
	 * @param bAddForHashing - TRUE if running with -sha and ini files should be added to list of hashed files
	 */
	void CreateIniFiles(UBOOL bAddForHashing);

	/**
	 *	Prep the given package for Object cooking.
	 *	@param	InPackage						The package to prep
	 *	@param	bInShouldBeSeekFree				Whether the package will be seekfree
	 *	@param	bInShouldBeFullyCompressed		Whether the package will be fully compressed
	 *	@param	bInIsNativeScriptFile			Whether the package is a native script file
	 *	@param	bInIsCombinedStartupPackage		Whether the package is a combined startup package
	 *	@param	bInStripEverythingButTextures	Whether the package should have everything but textures stripped
	 *	@param	bInProcessShaderCaches			Whether the shader caches should be processed
	 */
	void PrepPackageForObjectCooking(UPackage* InPackage, UBOOL bInShouldBeSeekFree, UBOOL bInShouldBeFullyCompressed,
		UBOOL bInIsNativeScriptFile, UBOOL bInIsCombinedStartupPackage, UBOOL bInStripEverythingButTextures, UBOOL bInProcessShaderCaches);

	/**
	* If -sha is specified then iterate over all FilesForSHA and generate their hashes
	* The results are written to Hashes.sha
	*/
	void GenerateSHAHashes();

	/** 
	 * Prepares shader files for the given platform to make sure they can be used for compiling
	 *
	 *	@return			TRUE on success, FALSE on failure.
	 */
	UBOOL PrepareShaderFiles();

	/** 
	 * Cleans up shader files for the given platform 
	 */
	void CleanupShaderFiles();

	/**
	 * Warns the user if the map they are cooking has references to editor content (EditorMeshes, etc)
	 *
	 * @param Package Package that has been loaded by the cooker
	 */
	void WarnAboutEditorContentReferences(UPackage* Package);

	/**
	 * Loads a package that will be used for cooking. This will cache the source file time
	 * and add the package to the Guid Cache
	 *
	 * @param Filename Name of package to load
	 *
	 * @return Package that was loaded and cached
	 */
	UPackage* LoadPackageForCooking(const TCHAR* Filename);

	/**
	 * Force load a package and emit some useful info
	 * 
	 * @param PackageName Name of package, could be different from filename due to localization
	 * @param bRequireServerSideOnly If TRUE, the loaded packages are required to have the PKG_ServerSideOnly flag set for this function to succeed
	 *
	 * @return TRUE if successful
	 */
	UBOOL ForceLoadPackage(const FString& PackageName, UBOOL bRequireServerSideOnly=FALSE);

	/**
	 * Load all packages in a specified ini section with the Package= key
	 * @param SectionName Name of the .ini section ([Engine.PackagesToAlwaysCook])
	 * @param PackageNames Paths of the loaded packages
	 * @param KeyName Optional name for the key of the list to load (defaults to "Package")
	 * @param bShouldSkipLoading If TRUE, this function will only fill out PackageNames, and not load the package
	 * @param bRequireServerSideOnly If TRUE, the loaded packages are required to have the PKG_ServerSideOnly flag set for this function to succeed
	 * @return if loading was required, whether we successfully loaded all the packages; otherwise, always TRUE
	 */
	UBOOL LoadSectionPackages(const TCHAR* SectionName, TArray<FString>& PackageNames, const TCHAR* KeyName=TEXT("Package"), UBOOL bShouldSkipLoading=FALSE, UBOOL bRequireServerSideOnly=FALSE);

	/**
	 * We use the CreateCustomEngine call to set some flags which will allow SerializeTaggedProperties to have the correct settings
	 * such that editoronly and notforconsole data can correctly NOT be loaded from startup packages (e.g. engine.u)
	 *
	 **/
	virtual void CreateCustomEngine();

	/**
	 * Performs command line and engine specific initialization.
	 *
	 * @param	Params	command line
	 * @param	bQuitAfterInit [out] If TRUE, the caller will quit the commandlet, even if the Init function returns TRUE
	 * @return	TRUE if successful, FALSE otherwise
	 */
	UBOOL Init( const TCHAR* Params, UBOOL& bQuitAfterInit );

	/**
	 *	Initializes the cooker for cleaning up materials
	 */
	virtual void InitializeMaterialCleanupSettings();

	/**
	 * Check if any dependencies of a seekfree package are newer than the cooked seekfree package.
	 * If they are, the package needs to be recooked.
	 *
	 * @param SrcLinker Optional source package linker to check dependencies for when no src file is available
	 * @param SrcFilename Name of the source of the seekfree package
	 * @param DstTimestamp Timestamp of the cooked version of this package
	 *
	 * @return TRUE if any dependencies are newer, meaning to recook the package
	 */
	UBOOL AreSeekfreeDependenciesNewer(ULinkerLoad* SrcLinker, const FString& SrcFilename, DOUBLE DstTimestamp);

	/**
	 * Generates list of src/ dst filename mappings of packages that need to be cooked after taking the command
	 * line options into account.
	 *
	 * @param [out] FirstStartupIndex		index of first startup package in returned array, untouched if there are none
	 * @param [out]	FirstScriptIndex		index of first script package in returned array, untouched if there are none
	 * @param [out] FirstGameScriptIndex	index of first game script package in returned array, untouched if there are none
	 * @param [out] FirstMapIndex			index of first map package in returned array, untouched if there are none
	 *
	 * @return	array of src/ dst filename mappings for packages that need to be cooked
	 */
	TArray<FPackageCookerInfo> GeneratePackageList( INT& FirstStartupIndex, INT& FirstScriptIndex, INT& FirstGameScriptIndex, INT& FirstMapIndex );

	/**
	 * Cleans up DLL handles and destroys cookers
	 */
	void Cleanup();

	/**
	 * Collects garbage and verifies all maps have been garbage collected.
	 */
	static void CollectGarbageAndVerify();

	/**
	 * Handles duplicating cubemap faces that are about to be saved with the passed in package.
	 *
	 * @param	Package	 Package for which cubemaps that are going to be saved with it need to be handled.
	 */
	void HandleCubemaps( UPackage* Package );

	/**
	 * Adds the mip data payload for the given texture and mip index to the texture file cache.
	 * If an entry exists it will try to replace it if the mip is <= the existing entry or
	 * the mip data will be appended to the end of the TFC file.
	 * Also updates the bulk data entry for the texture mip with the saved size/offset.
	 *
	 * @param Package - Package for texture that is going to be saved
	 * @param Texture - 2D texture with mips to be saved
	 * @param MipIndex - index of mip entry in the texture that needs to be saved
	 * @param TextureFormat - which format of compressed texture to use
	 */
	void SaveMipToTextureFileCache( UPackage* Package, UTexture2D* Texture, INT MipIndex, ETextureFormatSupport TextureFromat = TEXSUPPORT_DXT );

	/**
	 * Saves the passed in package, gathers and stores bulk data info and keeps track of time spent.
	 *
	 * @param	Package						Package to save
	 * @param	Base						Base/ root object passed to SavePackage, can be NULL
	 * @param	TopLevelFlags				Top level "keep"/ save flags, passed onto SavePackage
	 * @param	DstFilename					Filename to save under
	 * @param	bStripEverythingButTextures	Whether to strip everything but textures
	 * @param	bCleanupAfterSave			Whether or not objects should have certain object flags cleared and remember if objects were saved
	 * @param	bRememberSavedObjects		TRUE if objects should be marked to not be saved again, as well as materials (if bRemeberSavedObjects is set)
	 * @param	OutSavedObjects				Optional map to fill in with the objects that were saved
	 */
	void SaveCookedPackage( UPackage* Package, UObject* Base, EObjectFlags TopLevelFlags, const TCHAR* DstFilename, 
		UBOOL bStripEverythingButTextures, UBOOL bCleanupAfterSave, UBOOL bRememberSavedObjects, 
		TMap<FString,FString>* OutSavedObjects );

	/**
	 * Returns whether there are any localized resources that need to be handled.
	 *
	 * @param Package			Current package that is going to be saved
	 * @param TopLevelFlags		TopLevelFlags that are going to be passed to SavePackage
	 * 
	 * @return TRUE if there are any localized resources pending save, FALSE otherwise
	 */
	UBOOL AreThereLocalizedResourcesPendingSave( UPackage* Package, EObjectFlags TopLevelFlags );

	/**
	 * Merges the uncooked local and reference shader caches for the given platform and saves the merged cache as the
	 * cooked reference shader cache.
	 * @param	CookShaderPlatform	The platform whose ref shader cache will be copied.
	 */
	void CookReferenceShaderCache(EShaderPlatform CookShaderPlatform);

	/**
	 * Saves the global shader cache for the given platform in the cooked content folder with appropriate byte-order.
	 * @param	CookShaderPlatform	The platform whose ref shader cache will be copied.
	 */
	void CookGlobalShaderCache(EShaderPlatform CookShaderPlatform);

	/**
	 * @return The name of the output cooked data directory - regardless of user mode, etc.
	 */
	FString GetTrueCookedDirectory();

	/**
	 * @return The name of the output cooked data directory
	 */
	FString GetCookedDirectory();

	/**
	 * @return The name of the android texture format
	 */
	static const TCHAR* GetAndroidTextureFormatName(ETextureFormatSupport TextureFormat);

	/**
	 * @param The texture format to create the TFC file for
	 */
	void CreateAndroidTFCFile(ETextureFormatSupport TextureFormat);

	/**
	 * @return TRUE if the destination platform expects pre-byteswapped data (packages, coalesced ini files, etc)
	 */
	UBOOL ShouldByteSwapData();

	/**
	 * @return The name of the bulk data container file to use
	 */
	FString GetBulkDataContainerFilename();

	/** Gets the name of the persistent shader data file. */
	FString GetShaderDataContainerFilename() const;

	/**
	 *	Get the extension of a cooked package on the given platform
	 *
	 *	@param	Platform		The platform of interest
	 *
	 *	@return	FString			The extension
	 */
	FString GetCookedPackageExtension(UE3::EPlatformType Platform);

	/**
	 * Get the destination filename for the given source package file based on platform
	 *
	 * @param SrcFilename - source package file path
	 * @return cooked filename destination path
	 */
	FFilename GetCookedPackageFilename( const FFilename& SrcFilename );

	/*
	 * @return The name of the guid cache file to use
	 */
	FString GetGuidCacheFilename();

	/**
	 * Determines the name of the texture cache to use for the specified texture.
	 * @param Texture - The texture to determine the texture cache name for.
	 * @return The name of the texture cache file to use.
	 */
	FString GetTextureCacheName(UTexture* Texture);

	/**
	 * Returns the archive which contains the texture file cache with the specified name.
	 * @param TextureCacheName - The name of the texture cache to access.
	 * @return A pointer to the archive; will always be non-NULL.
	 */
	FArchive* GetTextureCacheArchive(const FName& TextureCacheName);

	/**
	 *	Verifies that the data in the texture file cache for the given texture
	 *	is a valid 'header' packet...
	 *
	 *	@param	Package		The package the texture was cooked into.
	 *	@param	Texture2D	The texture that was cooked.
	 *	@param	bIsSaved...	If TRUE, the texture was saved in a seekfree pacakge
	 *
	 *	@return	UBOOL		TRUE if the texture cache entry was valid, FALSE if not.
	 */
	UBOOL VerifyTextureFileCacheEntry();

	/** 
	 * Indirects the map list through a text file - 1 map per line of file
	 * 
	 * @param Params		The command line passed into the commandlet
	 *
	 * @return UBOOL		FALSE if processing failed
	 */
	UBOOL ParseMapList( const TCHAR* Params );

	UBOOL AddVerificationTextureFileCacheEntry(UPackage* Package, UTexture2D* Texture2D, UBOOL bIsSavedInSeekFreePackage);

	FTextureFileCacheEntry* FindTFCEntryOverlap(FTextureFileCacheEntry& InEntry);

	/** @return UBOOL  Whether or not this texture is a streaming texture and should be set as NeverStream. **/
	UBOOL ShouldBeANeverStreamTexture( UTexture2D* Texture2D, UBOOL bIsSavedInSeekFreePackage ) const;

	/**
	 *  Make sure data is (and remains) loaded for (and after) saving.
	 *
	 * @param	Texture2D					The texture to operate on
	 * @param	Mips						The Mips array to operate on (should differ for each compressed texture format)
	 * @param	FirstMipIndex				The first mip to operate on
	 * @param	bIsSavedInSeekFreePackage	Whether or not the package is seek free
	 * @param	bIsStreamingTexture			Whether or not the texture is streaming
	 */
	void EnsureMipsLoaded (UTexture2D* Texture2D, TIndirectArray<FTexture2DMipMap>& Mips, INT FirstMipIndex, UBOOL bIsSavedInSeekFreePackage, UBOOL bIsStreamingTExture);
	/**
	 * Helper function used by CookTexture - strips mips from UI Textures 
	 */
	void StripUIMips(UTexture2D* Texture2D, TIndirectArray<FTexture2DMipMap> &Mips, INT FirstMipIndex);

	/**
	 *	Get the list of packages that will always be loaded
	 *
	 *	@return	UBOOL		TRUE if successful, FALSE if failed
	 */
	UBOOL GetAlwaysLoadedPackageList(TArray<FString>& OutAlwaysLoaded, TMap<FString,TArray<FString> >& OutAlwaysLoadedLoc);

	/**
	 *	Mark known cooked startup objects
	 */
	virtual void MarkCookedStartupObjects(UBOOL bClear);

	/**
	 *	Tag known cooked startup objects
	 *	Called once at the start of the second processing pass
	 */
	virtual void TagCookedStartupObjects();

	/** 
	 * Add a mobile shader group at cook time
	 */
	void AddDynamicMobileShaderGroup(const FName& ShaderGroupName, const FName& PackageName);

END_COMMANDLET

BEGIN_COMMANDLET(PrecompileShaders,UnrealEd)
	FString ForceName;
	TArray<EShaderPlatform> ShaderPlatforms;
	void ProcessMaterial(UMaterial* Material, UBOOL bIsEditorOnlyPackage);
	void ProcessMaterialInstance(UMaterialInstance* MaterialInstance, UBOOL bIsEditorOnlyPackage);
	void ProcessTerrain(class ATerrain* Terrain);
END_COMMANDLET

BEGIN_COMMANDLET(DumpShaders, UnrealEd)
	TArray<EShaderPlatform> ShaderPlatforms;
END_COMMANDLET

BEGIN_COMMANDLET(TestCompression,UnrealEd)
	/**
	 * Run a compression/decompress test with the given package and compression options
	 *
	 * @param PackageName		The package to compress/decompress
	 * @param Flags				The options for compression
	 * @param UncompressedSize	The size of the uncompressed package file
	 * @param CompressedSize	The options for compressed package file
	 */
	void RunTest(const FFilename& PackageName, ECompressionFlags Flags, DWORD& UncompressedSize, DWORD& CompressedSize);
END_COMMANDLET

BEGIN_COMMANDLET(StripSource,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(MergePackages,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(MergeConflictingPackages,UnrealEd)
	void MergeConflicting(UObject* From, UObject* To, TMap<UObject*, UObject*>& Matches);
END_COMMANDLET

//====================================================================
// UDiffPackagesCommandlet and helper structs
//====================================================================

/**
 * The different types of comparison differences that can exist between packages.
 */
enum EObjectDiff
{
	/** no difference */
	OD_None,

	/** the object exist in the first package only */
	OD_AOnly,

	/** the object exists in the second package only */
	OD_BOnly,

	/** (three-way merges) the value has been changed from the ancestor package, but the new value is identical in the two packages being compared */
	OD_ABSame,

	/** @todo */
	OD_ABConflict,

	/** @todo */
	OD_Invalid,
};

/**
 * Contains an object and the object's path name.
 */
struct FObjectReference
{
	UObject* Object;
	FString ObjectPathName;

	FObjectReference( UObject* InObject )
	: Object(InObject)
	{
		if ( Object != NULL )
		{
			ObjectPathName = Object->GetPathName();
		}
	}
};


/**
 * Represents a single top-level object along with all its subobjects.
 */
struct FObjectGraph
{
	/**
	 * The list of objects in this object graph.  The first element is always the root object.
	 */
	TArray<struct FObjectReference> Objects;

	/**
	 * Constructor
	 *
	 * @param	RootObject			the top-level object for this object graph
	 * @param	PackageIndex		the index [into the Packages array] for the package that this object graph belongs to
	 * @param	ObjectsToIgnore		optional list of objects to not include in this object graph, even if they are contained within RootObject
	 */
	FObjectGraph( UObject* RootObject, INT PackageIndex, TArray<struct FObjectComparison>* ObjectsToIgnore=NULL);

	/**
	 * Returns the root of this object graph.
	 */
	inline UObject* GetRootObject() const { return Objects(0).Object; }
};

/**
 * Contains the natively serialized property data for a single UObject.
 */
struct FNativePropertyData 
{
	/** the object that this property data is for */
	UObject*				Object;

	/** the raw bytes corresponding to this object's natively serialized property data */
	TArray<BYTE>			PropertyData;

	/** the property names and textual representations of this object's natively serialized data */
	TMap<FString,FString>	PropertyText;

	/** Constructor */
	FNativePropertyData( UObject* InObject );

	/**
	 * Changes the UObject associated with this native property data container and re-initializes the
	 * PropertyData and PropertyText members
	 */
	void SetObject( UObject* NewObject );

	/** Comparison operators */
	inline UBOOL operator==( const FNativePropertyData& Other ) const
	{
		return ((Object == NULL) == (Other.Object == NULL)) && PropertyData == Other.PropertyData && LegacyCompareEqual(PropertyText,Other.PropertyText);
	}
	inline UBOOL operator!=( const FNativePropertyData& Other ) const
	{
		return ((Object == NULL) != (Other.Object == NULL)) || PropertyData != Other.PropertyData || LegacyCompareNotEqual(PropertyText,Other.PropertyText);
	}

	/** bool operator */
	inline operator UBOOL() const
	{
		return PropertyData.Num() || PropertyText.Num();
	}
};

BEGIN_COMMANDLET(DiffPackages,UnrealEd)

	/**
	 * Parses the command-line and loads the packages being compared.
	 *
	 * @param	Parms	the full command-line used to invoke this commandlet
	 *
	 * @return	TRUE if all parameters were parsed successfully; FALSE if any of the specified packages couldn't be loaded
	 *			or the parameters were otherwise invalid.
	 */
	UBOOL Initialize( const TCHAR* Parms );

	/**
	 * Generates object graphs for the specified object and its corresponding objects in all packages being diffed.
	 *
	 * @param	RootObject			the object to generate the object comparison for
	 * @param	out_Comparison		the object graphs for the specified object for each package being diffed
	 * @param	ObjectsToIgnore		if specified, this list will be passed to the FObjectGraphs created for this comparison; this will prevent those object graphs from containing
	 *								these objects.  Useful when generating an object comparison for package-root type objects, such as levels, worlds, etc. to prevent their comparison's
	 *								object graphs from containing all objects in the level/world
	 *
	 * @return	TRUE if RootObject was found in any of the packages being compared.
	 */
	UBOOL GenerateObjectComparison( UObject* RootObject, struct FObjectComparison& out_Comparison, TArray<struct FObjectComparison>* ObjectsToIgnore=NULL );
	UBOOL ProcessDiff(struct FObjectComparison& Diff);

	EObjectDiff DiffObjects(UObject* ObjA, UObject* ObjB, UObject* ObjAncestor, struct FObjectComparison& PropDiffs);

	/**
	 * Copies the raw property values for the natively serialized properties of the specified object into the output var.
	 *
	 * @param	Object	the object to load values for
	 * @param	out_NativePropertyData	receives the raw bytes corresponding to Object's natively serialized property values.
	 */
	static void LoadNativePropertyData( UObject* Object, TArray<BYTE>& out_NativePropertyData );

	/**
	 * Compares the natively serialized property values for the specified objects by comparing the non-script serialized portion of each object's data as it
	 * is on disk.  If a different is detected, gives each object the chance to generate a textual representation of its natively serialized property values
	 * that will be displayed to the user in the final comparison report.
	 *
	 * @param	ObjA		the object from the first package being compared.  Can be NULL if both ObjB and ObjAncestor are valid, which indicates that this object
	 *						doesn't exist in the first package.
	 * @param	ObjB		the object from the second package being compared.  Can be NULL if both ObjA and ObjAncestor are valid, which indicates that this object
	 *						doesn't exist in the second package.
	 * @param	ObjAncestor	the object from the optional common base package.  Can only be NULL if both ObjA and ObjB are valid, which indicates that this is either
	 *						a two-comparison (if NumPackages == 2) or the object was added to both packages (if NumPackages == 3)
	 * @param	PropertyValueComparisons	contains the results for all property values that were different so far; for any native property values which are determined
	 *										to be different, new entries will be added to the ObjectComparison's list of PropDiffs.
	 *
	 * @return	The cumulative comparison result type for a comparison of all natively serialized property values.
	 */
	EObjectDiff CompareNativePropertyValues( UObject* ObjA, UObject* ObjB, UObject* ObjAncestor, struct FObjectComparison& PropertyValueComparisons );

	UBOOL bDiffNonEditProps;
	UBOOL bDiffAllProps;

	UPackage* Packages[3];
	FString PackageFilenames[3];
	INT NumPackages;
END_COMMANDLET

BEGIN_COMMANDLET(Make,UnrealEd)
	/**
	 * Allows commandlets to override the default behavior and create a custom engine class for the commandlet. If
	 * the commandlet implements this function, it should fully initialize the UEngine object as well.  Commandlets
	 * should indicate that they have implemented this function by assigning the custom UEngine to GEngine.
	 */
	virtual void CreateCustomEngine();
	/**
	 * Builds a text manifest of the class hierarchy to be used instead of loading all editor packages at start up
	 */
	UBOOL BuildManifest(void);

	/**
	 * Deletes all dependent .u files.  Given an index into the EditPackages array, deletes the .u files corresponding to
	 * that index, as well as the .u files corresponding to subsequent members of the EditPackages array.
	 *
	 * @param	ScriptOutputDir		output directory for script packages.
	 * @param	PackageList			list of package names to delete
	 * @param	StartIndex			index to start deleting packages
	 * @param	Count				number of packages to delete - defaults to all
	 */
	void DeleteEditPackages( const FString& ScriptOutputDir, const TArray<FString>& PackageList, INT StartIndex, INT Count=INDEX_NONE ) const;
END_COMMANDLET

BEGIN_COMMANDLET(ShowTaggedProps,UnrealEd)

	/**
	 * Optional list of properties to display values for.  If this array is empty, all serialized property values are logged.
	 */
	TLookupMap<UProperty*> SearchProperties;

	/**
	 * Optional list of properties to ignore when logging values.
	 */
	TLookupMap<UProperty*> IgnoreProperties;
	void ShowSavedProperties( UObject* Object ) const;
END_COMMANDLET

BEGIN_COMMANDLET(ShowPropertyFlags, UnrealEd)
END_COMMANDLET

/**
 * Container for parameters of the UShowObjectCount::ProcessPackages.
 */
struct FObjectCountExecutionParms
{
	/** the list of classes to search for */
	TArray<UClass*> SearchClasses;

	/** Bitmask of flags that are required in order for a match to be listed in the results */
	EObjectFlags ObjectMask;

	/** TRUE to print out the names of all objects found */
	UBOOL bShowObjectNames:1;

	/** TRUE to include cooked packages in the list of packages to check */	
	UBOOL bIncludeCookedPackages:1;
	
	/** TRUE to check cooked packages only */
	UBOOL bCookedPackagesOnly:1;
	
	/** TRUE to ignore objects of derived classes */
	UBOOL bIgnoreChildren:1;
	
	/** TRUE to ignore packages which are writeable (useful for skipping test packages) */
	UBOOL bIgnoreCheckedOutPackages:1;
	
	/** TRUE to skip checking script packages */
	UBOOL bIgnoreScriptPackages:1;
	
	/** TRUE to skip checking map packages */
	UBOOL bIgnoreMapPackages:1;
	
	/** TRUE to skip checking content (non-map, non-script) packages */
	UBOOL bIgnoreContentPackages:1;
	
	/** Constructor */
	FObjectCountExecutionParms( const TArray<UClass*>& InClasses, EObjectFlags InMask=RF_LoadForClient|RF_LoadForServer|RF_LoadForEdit );
};

struct FPackageObjectCount
{
	/** the number of objects for a single class in a single package */
	INT				Count;
	/** the name of the package */
	FString			PackageName;
	/** the name of the object class this count is for */
	FString			ClassName;
	/** the path names for the objects found */
	TArray<FString>	ObjectPathNames;

	/** Constructors */
	FPackageObjectCount()
	: Count(0)
	{
	}
	FPackageObjectCount( const FString& inPackageName, const FString& inClassName, INT InCount=0 )
	: Count(InCount), PackageName(inPackageName), ClassName(inClassName)
	{
	}
};

BEGIN_COMMANDLET(ShowObjectCount,UnrealEd)
	void StaticInitialize();

	/**
	 * Searches all packages for the objects which meet the criteria specified.
	 *
	 * @param	Parms				specifies the parameters to use for the search
	 * @param	Results				receives the results of the search
	 * @param	bUnsortedResults	by default, the list of results will be sorted according to the number of objects in each package;
	 *								specify TRUE to override this behavior.
	 */
	void ProcessPackages( const FObjectCountExecutionParms& Parms, TArray<FPackageObjectCount>& Results, UBOOL bUnsortedResults=FALSE );
END_COMMANDLET

BEGIN_COMMANDLET(CreateDefaultStyle,UnrealEd)
	class UUISkin* DefaultSkin;

	class UUIStyle_Text* CreateTextStyle( const TCHAR* StyleName=TEXT("DefaultTextStyle"), FLinearColor StyleColor=FLinearColor(1.f,1.f,1.f,1.f) ) const;
	class UUIStyle_Image* CreateImageStyle( const TCHAR* StyleName=TEXT("DefaultImageStyle"), FLinearColor StyleColor=FLinearColor(1.f,1.f,1.f,1.f) ) const;
	class UUIStyle_Combo* CreateComboStyle( UUIStyle_Text* TextStyle, UUIStyle_Image* ImageStyle, const TCHAR* StyleName=TEXT("DefaultComboStyle") ) const;

	void CreateAdditionalStyles() const;

	void CreateConsoleStyles() const;

	void CreateMouseCursors() const;
END_COMMANDLET

BEGIN_COMMANDLET(ShowStyles,UnrealEd)
	void DisplayStyleInfo( class UUIStyle* Style );

	INT GetStyleDataIndent( class UUIStyle* Style );
END_COMMANDLET

BEGIN_COMMANDLET(ExamineOuters,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(ListCorruptedComponents,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(FindSoundCuesWithMissingGroups,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(FindTexturesWithMissingPhysicalMaterials,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(FindQuestionableTextures,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(BreakApartPackages,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(ListPhysAssetsNumBodies,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(FindDuplicateKismetObjects,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(AnalyzeKismet,UnrealEd)
	KismetResourceMap ResourceMap;
	BOOL bShowLevelRefs;
	void CreateReport(const FString& FileName) const;
END_COMMANDLET

BEGIN_COMMANDLET(UpdateKismet,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(SetTextureLODGroup,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(CompressAnimations,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(FixAdditiveReferences,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(WrangleContent,UnrealEd)
END_COMMANDLET


/** lists all content referenced in the default properties of script classes */
BEGIN_COMMANDLET(ListScriptReferencedContent,UnrealEd)
	/** processes a value found by ListReferencedContent(), possibly recursing for inline objects
	 * @param Value the object to be processed
	 * @param Property the property where Value was found (for a dynamic array, this is the Inner property)
	 * @param PropertyDesc string printed as the property Value was assigned to (usually *Property->GetName(), except for arrays, where it's the array name and index)
	 * @param Tab string with a number of tabs for the current tab level of the output
	 */
	void ProcessObjectValue(UObject* Value, UProperty* Property, const FString& PropertyDesc, const FString& Tab);
	/** lists content referenced by the given data
	 * @param Struct the type of the Default data
	 * @param Default the data to look for referenced objects in
	 * @param HeaderName header string printed before any content references found (only if the data might contain content references)
	 * @param Tab string with a number of tabs for the current tab level of the output
	 */
	void ListReferencedContent(UStruct* Struct, BYTE* Default, const FString& HeaderName, const FString& Tab = TEXT(""));
END_COMMANDLET

BEGIN_COMMANDLET(FixAmbiguousMaterialParameters,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(TestWordWrap,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(SetMaterialUsage,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(SetPackageFlags,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(PIEToNormal,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(UT3MapStats,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(OutputAuditSummary,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(PerformMapCheck,UnrealEd)
	/** the list of filenames for the maps that will be checked */
	TArray<FFilename> MapNames;

	/** the index of map currently being processed */
	INT MapIndex;

	/**
	 * The number of maps processed so far.
	 */
	INT TotalMapsChecked;

	/**
	 * unused for now.
	 */
	UBOOL bTestOnly;

	/**
	 * Evalutes the command-line to determine which maps to check.  By default all maps are checked (except PIE and trash-can maps)
	 * Provides child classes with a chance to initialize any variables, parse the command line, etc.
	 *
	 * @param	Tokens			the list of tokens that were passed to the commandlet
	 * @param	Switches		the list of switches that were passed on the commandline
	 * @param	MapPathNames	receives the list of path names for the maps that will be checked.
	 *
	 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
	 */
	virtual INT InitializeMapCheck( const TArray<FString>& Tokens, const TArray<FString>& Switches, TArray<FFilename>& MapPathNames );

	/**
	 * The main worker method - performs the commandlets tests on the package.
	 *
	 * @param	MapPackage	the current package to be processed
	 *
	 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
	 */
	virtual INT CheckMapPackage( UPackage* MapPackage );

	/**
	 * Called after all packages have been processed - provides commandlets with an opportunity to print out test results or
	 * provide feedback.
	 *
	 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
	 */
	virtual INT ProcessResults() { return 0; }
END_COMMANDLET

BEGIN_CHILD_COMMANDLET(FindStaticActorsRefs,PerformMapCheck,UnrealEd)

	UBOOL bStaticKismetRefs;
	UBOOL bShowObjectNames;
	UBOOL bLogObjectNames;
	UBOOL bShowReferencers;
	UBOOL bFixPrefabSequences;

	INT TotalStaticMeshActors;
	INT TotalStaticLightActors;
	INT TotalReferencedStaticMeshActors;
	INT TotalReferencedStaticLightActors;
	INT TotalMapsChecked;
	TMap<INT, INT> ReferencedStaticMeshActorMap;
	TMap<INT, INT> ReferencedStaticLightActorMap;

	/**
	 * Evalutes the command-line to determine which maps to check.  By default all maps are checked (except PIE and trash-can maps)
	 * Provides child classes with a chance to initialize any variables, parse the command line, etc.
	 *
	 * @param	Tokens			the list of tokens that were passed to the commandlet
	 * @param	Switches		the list of switches that were passed on the commandline
	 * @param	MapPathNames	receives the list of path names for the maps that will be checked.
	 *
	 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
	 */
	virtual INT InitializeMapCheck( const TArray<FString>& Tokens, const TArray<FString>& Switches, TArray<FFilename>& MapPathNames );

	/**
	 * The main worker method - performs the commandlets tests on the package.
	 *
	 * @param	MapPackage	the current package to be processed
	 *
	 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
	 */
	virtual INT CheckMapPackage( UPackage* MapPackage );


	/**
	 * Called after all packages have been processed - provides commandlets with an opportunity to print out test results or
	 * provide feedback.
	 *
	 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
	 */
	virtual INT ProcessResults();
END_COMMANDLET

BEGIN_CHILD_COMMANDLET(FindRenamedPrefabSequences,PerformMapCheck,UnrealEd)
	TLookupMap<FString>	RenamedPrefabSequenceContainers;

	/**
	 * Evalutes the command-line to determine which maps to check.  By default all maps are checked (except PIE and trash-can maps)
	 * Provides child classes with a chance to initialize any variables, parse the command line, etc.
	 *
	 * @param	Tokens			the list of tokens that were passed to the commandlet
	 * @param	Switches		the list of switches that were passed on the commandline
	 * @param	MapPathNames	receives the list of path names for the maps that will be checked.
	 *
	 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
	 */
	virtual INT InitializeMapCheck( const TArray<FString>& Tokens, const TArray<FString>& Switches, TArray<FFilename>& MapPathNames );

	/**
	 * The main worker method - performs the commandlets tests on the package.
	 *
	 * @param	MapPackage	the current package to be processed
	 *
	 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
	 */
	virtual INT CheckMapPackage( UPackage* MapPackage );


	/**
	 * Called after all packages have been processed - provides commandlets with an opportunity to print out test results or
	 * provide feedback.
	 *
	 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
	 */
	virtual INT ProcessResults();
END_COMMANDLET

BEGIN_COMMANDLET(DumpLightmapInfo,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(PerformTerrainMaterialDump,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(ListLoopingEmitters,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(SoundClassInfo,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(ListDistanceCrossFadeNodes,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(LocSoundInfo,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(DumpProperties,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(SoundCueAudit,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(ContentAudit,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(ContentComparison,UnrealEd)


	struct TypesToLookForDatum
	{
		FString ClassName;
		UClass* Class;
		INT TotalSize;

		TypesToLookForDatum( const FString& InClassName, UClass* InClass ):
		ClassName(InClassName), Class(InClass), TotalSize(0)
		{
		}

		/**
		 * @return whether or not we added the passed in object.  (used to "break;" in our while loop
		 **/
		UBOOL PossiblyAddObject( UObject* TheObject )
		{
			if( TheObject->IsA( Class ) == TRUE )
			{
				warnf( TEXT("\tAdding: %s %d "), *TheObject->GetFullName(), TheObject->GetResourceSize()  );
				TotalSize += TheObject->GetResourceSize();

				return TRUE;
			}
			return FALSE;
		}
	};


	TArray<TypesToLookForDatum> TypesToLookFor; 
	TArray<FString> ClassesToCompare; 
	TMap<FString,TArray<FString>> ClassToDerivedClassesBeingCompared;

	class FDiagnosticTableViewer* Table;

	struct FDependentAssetInfo
	{
		/** The Full name of the asset */
		FString AssetName;
		/** The resource size of the asset */
		INT ResourceSize;
		/** The list of classes that depend on this asset */
		TMap<FString,UBOOL> ClassesThatDependOnAsset;

		FDependentAssetInfo()
		{
			appMemzero(this, sizeof(FDependentAssetInfo));
		}

		inline UBOOL operator==(const FDependentAssetInfo& Other) const
		{
			if (
				(AssetName == Other.AssetName) &&
				(ResourceSize == Other.ResourceSize)
				)
			{
				// See if the maps are equal
				if (ClassesThatDependOnAsset.Num() == Other.ClassesThatDependOnAsset.Num())
				{
					TMap<FString,UBOOL>::TConstIterator It(ClassesThatDependOnAsset);
					TMap<FString,UBOOL>::TConstIterator OtherIt(Other.ClassesThatDependOnAsset);

					while (It)
					{
						if (OtherIt == NULL)
						{
							return FALSE;
						}

						if ((It.Key() != OtherIt.Key()) ||
							(It.Value() != OtherIt.Value()))
						{
							return FALSE;
						}

						++It;
						++OtherIt;
					}
					return TRUE;
				}
			}
			return FALSE;
		}

		inline FDependentAssetInfo& operator=(const FDependentAssetInfo& Other)
		{
			AssetName = Other.AssetName;
			ResourceSize = Other.ResourceSize;
			ClassesThatDependOnAsset.Empty();
			ClassesThatDependOnAsset = Other.ClassesThatDependOnAsset;
			return *this;
		}
	};
	UBOOL bGenerateAssetGrid;
	UBOOL bGenerateFullAssetGrid;
	UBOOL bSkipTagging;
	UBOOL bSkipAssetCSV;
	TArray<FString> ClassesTaggedInDependentAssetsList;
	TArray<FString> ClassesTaggedInDependentAssetsList_Tags;
	TMap<FString, FDependentAssetInfo> DependentAssets;

	class FGADHelper* GADHelper;

	virtual void CreateCustomEngine();

// 	void StaticInitialize()
// 	{
// 		IsClient = FALSE;
// 		IsEditor = TRUE; // weak
// 		IsServer = FALSE;
// 		LogToConsole = FALSE;
// 	}


	void InitClassesToGatherDataOn( const FString& InTableName );

	void GatherClassBasedData( ULinkerLoad* Linker, const UClass* const InClassToGatherDataOn );

	void ReportComparisonData( const FString& ClassToCompare );

	void ReportClassAssetDependencies(const FString& InTableName);

	FString CurrentTime;

END_COMMANDLET


BEGIN_COMMANDLET(ListPSysFixedBoundSetting,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(ListEmittersUsingModule,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(ReplaceActor,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(ListSoundNodeWaves,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(CheckpointGameAssetDatabase,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(ListCustomMaterialExpressions,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(AnalyzeShaderCaches,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(AnalyzeCookedTextureUsage,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(AnalyzeCookedTextureSingleUsage,UnrealEd)
	/** The list of textures to add to the 'Single Use Textures' collection */
	TArray<FString> TextureAddList;
	/** The list of referencers of SUTs to add to the 'Single Use Textures Referencers' collection */
	TArray<FString> ReferencerAddList;
	/** Map of packages to single use textures they reference/contain */
	TMap<FString,TArray<FString>> PackageToSingleTexturesMap;
	/** Map of single use textures to their referencer(s) */
	TMap<FString,TArray<FString>> SingleTextureToReferencersMap;
	/** Map of SUT referencer referencers */
	TMap<FString,TArray<FString>> FullReferencerMap;
	// If specified, this will generate a csv file of the referencers of single use texture refernecers. 
	// (Ie, what references a material that references a single use texture)
	UBOOL bFullRefCsv;
	// If specified, this will only examine single use textures that have references in map packages.
	UBOOL bMapsOnly;
	// If specified, the commandlet will log out various information
	UBOOL bVerbose;
	// If specified, do NOT clear the collections before updating them
	UBOOL bRetainCollections;
	// If specified, do NOT update the collections at all.
	// This is intended for generating just the RefCsv file, or for simply logging out the info (bVerbose)
	UBOOL bSkipCollections;


	/**
	 *	Generate the list of single use textures for the given CookerData
	 *
	 *	@param	CookerData		The cooker data to generate the list from
	 */
	void GenerateSingleUseTextureList(const UPersistentCookerData* CookerData);

	/**
	 *	Process the single use texture list that was generated from the cooker data
	 */
	void ProcessSingleUseTextureList();

	/**
	 *	Process the given single use texture referencer
	 *
	 *	@param	InReferencer		The referencer object
	 *	@param	ReferencerList		The SUT-->Referencer list for the current SUT being processed
	 */
	void ProcessReferencer(UObject* InReferencer, TArray<FString>* ReferencerList);

	/**
	 *	Process the given list of referencers of the given referencer of a 
	 *	single use texture.
	 *
	 *	@param	InRefName			The name of the single use texture referencer
	 *	@param	InReferencersList	The list of referencers of that reference.
	 */
	void ProcessReferencerReferencerList(const FString& InRefName, const TArray<FReferencerInformation>& InReferencersList);

	/**
	 *	Update the collections
	 */
	void UpdateCollections();

	/**
	 *	Generate the CSV file of single use texture referencer referencers
	 */
	void GenerateCSVFile();
END_COMMANDLET

BEGIN_COMMANDLET(AnalyzeCookedTextureDXT5Usage,UnrealEd)
	/** Packages to DXT5 textures they contain (to find 'white alpha' textures) */
	TMap<FString,TArray<FString>> PackageToDXT5TexturesMap;
	/** The list of textures found that are DXT5 */
	TArray<FString> DXT5TextureList;
	/** The list of textures to add to the 'DXT5 Textures' collection */
	TArray<FString> TextureAddList;
	/** The list of textures to add to the 'DXT5 WhiteAlpha Textures' collection */
	TArray<FString> WhiteAlphaAddList;
	/** If specified, generate collection of textures that have a white alpha channel */
	UBOOL bWhiteAlpha;
	/** If specified, this will only examine single use textures that have references in map packages. */
	UBOOL bMapsOnly;
	/** If specified, the commandlet will log out various information */
	UBOOL bVerbose;
	/** If specified, do NOT clear the collections before updating them */
	UBOOL bRetainCollections;
	/** If specified, do NOT update the collections at all.
	    This is intended for simply logging out the info (bVerbose) */
	UBOOL bSkipCollections;
	/** The Root Mean Square Deviation check value */
	DOUBLE AllowedRMSDForWhite;

	/**
	 *	Generate the list of DXT5 textures for the given CookerData
	 *
	 *	@param	CookerData		The cooker data to generate the list from
	 */
	void GenerateDXT5TextureList(const UPersistentCookerData* CookerData);

	/**
	 *	Process the DXT5 texture list that was generated from the cooker data
	 */
	void ProcessDXT5TextureList();

	/**
	 *	Update the collections
	 */
	void UpdateCollections();
END_COMMANDLET

BEGIN_COMMANDLET(TestTextureCompression,UnrealEd)
END_COMMANDLET

BEGIN_COMMANDLET(FindDarkDiffuseTextures,UnrealEd)
	TArray<FString> Tokens;
	TArray<FString> Switches;
	FLOAT MinimalBrightness;
	UBOOL bIgnoreBlack;
	UBOOL bUseGrayScale;
	INT CollectionUpdateRate;
	TArray<FString>	DarkTextures;
	TArray<FString>	NonDarkTextures;

	FString DarkDiffuseCollectionName;

	class FGADHelper* GADHelper;

	/**
	 *	Startup the commandlet
	 */
	void Startup();

	/**
	 *	Shutdown the commandlet
	 */
	void Shutdown();

	/**
	 *	Update the collection with the current lists
	 *
	 *	@return	UBOOL		TRUE if successful, FALSE if not.
	 */
	UBOOL UpdateCollection();

	/**
	 *	Check the given material for dark textures in the diffuse property chain
	 *
	 *	@param	InMaterialInterface		The material interface to check
	 *
	 *	@return	UBOOL					TRUE if dark textures were found, FALSE if not.
	 */
	UBOOL ProcessMaterial(UMaterialInterface* InMaterialInterface);

END_COMMANDLET

BEGIN_COMMANDLET(FindUniqueSpecularTextureMaterials,UnrealEd)
	TArray<FString> Tokens;
	TArray<FString> Switches;
	/** 
	 *	If FALSE, then check environmental materials only (diffuse chain has texture(s) tagged for the WORLD group)
	 *	If TRUE, check ALL materials
	 */
	UBOOL bAllMaterials;
	INT CollectionUpdateRate;
	TArray<FString>	UniqueSpecMaterials;
	TArray<FString>	NonUniqueSpecMaterials;

	FString UniqueSpecularTextureCollectionName;

	class FGADHelper* GADHelper;

	/**
	 *	Startup the commandlet
	 */
	void Startup();

	/**
	 *	Shutdown the commandlet
	 */
	void Shutdown();

	/**
	 *	Update the collection with the current lists
	 *
	 *	@return	UBOOL		TRUE if successful, FALSE if not.
	 */
	UBOOL UpdateCollection();

	/**
	 *	Check the given material for dark textures in the diffuse property chain
	 *
	 *	@param	InMaterialInterface		The material interface to check
	 *
	 *	@return	UBOOL					TRUE if dark textures were found, FALSE if not.
	 */
	UBOOL ProcessMaterial(UMaterialInterface* InMaterialInterface);

END_COMMANDLET

BEGIN_COMMANDLET(FindNeverStreamTextures,UnrealEd)
	TArray<FString> Tokens;
	TArray<FString> Switches;
	INT CollectionUpdateRate;
	TArray<FString>	NeverStreamTextures;
	TArray<FString>	StreamTextures;

	FString CollectionName;
	FGADHelper* GADHelper;

	/**
	 *	Startup the commandlet
	 */
	void Startup();

	/**
	 *	Shutdown the commandlet
	 */
	void Shutdown();

	/**
	 *	Update the collection with the current lists
	 *
	 *	@return	UBOOL		TRUE if successful, FALSE if not.
	 */
	UBOOL UpdateCollection();

	/**
	 *	Check the texture for NeverStream
	 *
	 *	@param	InTexture		The texture to check
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if there was an error
	 */
	UBOOL ProcessTexture(UTexture* InTexture);
END_COMMANDLET

/** Data structure containing texture info for determining replacement priority */
struct FTextureReplacementInfo
{
	/** The texture... */
	UTexture2D* Texture;
	/** The path name of the texture... */
	FString TexturePathName;
	/** The filename of the package that contains the texture... */
	FString PackageFilename;
	/** The priority of the texture - 0 = highest... */
	FLOAT Priority;

	void Clear()
	{
		Texture = NULL;
		TexturePathName = TEXT("");
		PackageFilename = TEXT("");
		Priority = -1.0f;
	}
};

BEGIN_COMMANDLET(FindDuplicateTextures,UnrealEd)
	TArray<FString> Tokens;
	TArray<FString> Switches;

	UBOOL bFixup;
	UBOOL bFixupOnly;
	UBOOL bExactMatch;
	UBOOL bCheckoutOnly;

	FString FixupFilename;
	FString PriorityFilename;
	FString CSVDirectory;

	TArray<FString> FolderPriority;

	TMap<DWORD,TArray<FString>>	CRCToTextureNamesMap;
	TMap<DWORD,TArray<FString>>	DuplicateTexturesMap;
	TMap<FString,TArray<FString>> DuplicateTextures;

	TArray<FString> TouchedPackageList;

	/**
	 *	Startup the commandlet
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	UBOOL Startup();

	/**
	 *	Shutdown the commandlet
	 */
	void Shutdown();

	/**
	 *	Update the collection with the current lists
	 *
	 *	@return	UBOOL		TRUE if successful, FALSE if not.
	 */
	UBOOL UpdateCollection();

	/**
	 *	Process the given texture, adding it to a CRC-bucket
	 *
	 *	@param	InTexture		The texture to process
	 */
	void ProcessTexture(UTexture2D* InTexture);

	/**
	 *	Finds the true duplicates from the process textures...
	 */
	void FindTrueDuplicates();

	/**
	 *	Fixup the duplicates found...
	 */
	void FixupDuplicates();

	/**
	 *	Fill in the texture replacement helper structure for the given texture name.
	 *
	 *	@param	InTextureName		The name of the texture to fill in the info for
	 *	@param	OutInfo	[out]		The structure to fill in with the information
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE if not.
	 */
	UBOOL GetTextureReplacementInfo(FString& InTextureName, FTextureReplacementInfo& OutInfo);

	/** If present, parse the folder priority list. */
	void ParseFolderPriorityList();

	/** 
	 *	If present, parse the fixup file. 
	 *	Entries found will be inserted into the DuplicateTextures map.
	 */
	void ParseFixupFile();

	/**
	 *	Write out the list of packages that were touched...
	 */
	void WriteTouchedPackageFile();

END_COMMANDLET

/**
 *	Find static meshes w/ bCanBecomeDynamic set to true
 */
BEGIN_COMMANDLET(FindStaticMeshCanBecomeDynamic,UnrealEd)
	TArray<FString> Tokens;
	TArray<FString> Switches;

	TMap<FString,UBOOL> StaticMeshesThatCanBecomeDynamic;

	UBOOL bSkipCollection;
	FString CollectionName;
	FGADHelper* GADHelper;

	/**
	 *	Startup the commandlet
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	UBOOL Startup();

	/**
	 *	Shutdown the commandlet
	 */
	void Shutdown();

	/**
	 *	Update the collection with the current lists
	 *
	 *	@return	UBOOL		TRUE if successful, FALSE if not.
	 */
	UBOOL UpdateCollection();

	/**
	 *	Process the given static mesh
	 *
	 *	@param	InStaticMesh		The Static mesh to process
	 *	@param	InPackage		The package being processed
	 */
	void ProcessStaticMesh(UStaticMesh* InStaticMesh, UPackage* InPackage);
END_COMMANDLET

/**
 *	Find static meshes w/ sections that have 0 triangles.
 */
BEGIN_COMMANDLET(FindStaticMeshEmptySections,UnrealEd)
	TArray<FString> Tokens;
	TArray<FString> Switches;

	TArray<FString> StaticMeshEmptySectionList;
	TArray<FString> StaticMeshPassedList;
	TMap<FString,TArray<FString>> StaticMeshToPackageUsedInMap;

	UBOOL bOnlyLoadMaps;
	FString CollectionName;
	FGADHelper* GADHelper;

	/**
	 *	Startup the commandlet
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	UBOOL Startup();

	/**
	 *	Shutdown the commandlet
	 */
	void Shutdown();

	/**
	 *	Update the collection with the current lists
	 *
	 *	@return	UBOOL		TRUE if successful, FALSE if not.
	 */
	UBOOL UpdateCollection();

	/**
	 *	Process the given static mesh
	 *
	 *	@param	InStaticMesh		The Static mesh to process
	 *	@param	InPackage		The package being processed
	 */
	void ProcessStaticMesh(UStaticMesh* InStaticMesh, UPackage* InPackage);

END_COMMANDLET

/**
 *	Find asset referencers
 *	Will find all referencers of a given set of assets in the maps with the given prefixes.
 *	Usage:
 *	FindAssetReferencers <ASSET OR PACKAGE NAME> <-SCRIPT> <-MAPPREFIXES=<CSV list of map prefixes to look in>>
 *
 *	The ASSET OR PACKAGE NAME can either be:
 *		- A direct name of an asset (Package.Group.ObjectName)
 *		- A package or sub-package name (Package or Package.Group)
 *		  In this case, all the assets of the package or group will be looked up
 *
 *	The SCRIPT option allows for looking in non-native script packages 
 *
 *	The MAPPREFIXES options allows you to specificy prefix names for maps to look in.
 *	'*' will look in all maps that are found.
 */
BEGIN_COMMANDLET(FindAssetReferencers,UnrealEd)
	TArray<FString> Tokens;
	TArray<FString> Switches;

	UBOOL bVerbose;
	UBOOL bExactMatch;
	TArray<FString> MapPrefixes;
	UBOOL bNonNativeScript;

	TArray<FString> PackageList;

	FString AssetName;
	TArray<FString> AssetsOfInterestList;
	TMap<FString,TMap<FString,UBOOL>> AssetsToReferencersMap;

	INT NumMapsAssetFoundIn;

	/**
	 *	Startup the commandlet
	 *
	 *	@param	Params	The command-line parameters
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	UBOOL Startup(const FString& Params);

	/** Shutdown the commandlet */
	void Shutdown();

	/** Show the usage string */
	void ShowUsage();

	/** 
	 *	Generate the list of packages to process 
	 *	
	 *	@return UBOOL	TRUE if successful, FALSE if not
	 */
	UBOOL GeneratePackageList();

	/** Process the packages
	 *	
	 *	@return UBOOL	TRUE if successful, FALSE if not
	 */
	UBOOL ProcessPackageList();

	/**
	 *	Process the given object referencer
	 *
	 *	@param	InReferencer	The object referencers to process
	 *	@param	InPackage		The package being processed
	 *	@param	ReferencerList	The list to add the ref info to
	 */
	void ProcessReferencer(UObject* InReferencer, UPackage* InPackage, TMap<FString,UBOOL>* ReferencerList);

	/** Report the results of the commandlet */
	void ReportResults();

END_COMMANDLET

/** used by the FindUnreferencedFunctions commandlet to read function references out of the script bytecode */
class UByteCodeSerializer : public UStruct
{
public:
	DECLARE_CLASS_INTRINSIC(UByteCodeSerializer,UStruct,0|CLASS_Transient,UnrealEd);
	NO_DEFAULT_CONSTRUCTOR(UByteCodeSerializer);

	/**
	 * Entry point for processing a single struct's bytecode array.  Calls SerializeExpr to process the struct's bytecode.
	 *
	 * @param	InParentStruct	the struct containing the bytecode to be processed.
	 */
	void ProcessStructBytecode( UStruct* InParentStruct );

	/**
	 * Determines the correct class context based on the specified property.
	 *
	 * @param	ContextProperty		a property which potentially changes the current lookup context, such as object or interface property
	 *
	 * @return	if ContextProperty corresponds to an object, interface, or delegate property, result is a pointer to the class associated
	 *			with the property (e.g. for UObjectProperty, would be PropertyClass); NULL if no context switch will happen as a result of
	 *			processing ContextProperty
	 */
	UClass* DetermineCurrentContext( UProperty* ContextProperty ) const;

	/**
	 * Sets the value of CurrentContext based on the results of calling DetermineCurrentContext()
	 *
	 * @param	ContextProperty		a property which potentially changes the current lookup context, such as object or interface property
	 */
	void SetCurrentContext( UProperty* ContextProperty=GProperty );

	/**
	 * Marks the specified function as referenced by finding the UFunction corresponding to its original declaration.
	 *
	 * @param	ReferencedFunction	the function that should be marked referenced
	 */
	static void MarkFunctionAsReferenced( UFunction* ReferencedFunction );

	/**
	 * Wrapper for checking whether the specified function is referenced.  Uses the UFunction corresponding to
	 * the function's original declaration.
	 */
	static UBOOL IsFunctionReferenced( UFunction* FunctionToCheck );

	/**
	 * Searches for the UFunction corresponding to the original declaration of the specified function.
	 *
	 * @param	FunctionToCheck		the function to lookup
	 *
	 * @return	a pointer to the original declaration of the specified function.
	 */
	static UFunction* FindOriginalDeclaration( UFunction* FunctionToCheck );

	/**
	 * Find the function associated with the native index specified.
	 *
	 * @param	iNative		the native function index to search for
	 *
	 * @return	a pointer to the UFunction using the specified native function index.
	 */
	UFunction* GetIndexedNative( INT iNative );

	/** === UStruct interface === */
	/**
	 * Processes compiled bytecode for UStructs, marking any UFunctions encountered as referenced.
	 *
	 * @param	iCode	the current position in the struct's bytecode array.
	 * @param	Ar		the archive used for serializing the bytecode; not really necessary for this purpose.
	 *
	 * @return	the token that was parsed from the bytecode stream.
	 */
	virtual EExprToken SerializeExpr( INT& iCode, FArchive& Ar );

protected:
	/**
	 * The struct (function, state, class, etc.) containing the bytecode currently being processed.
	 */
	class UStruct* ParentStruct;

	/**
	 * The class associated with the symbol that was most recently parsed.  Used for function lookups to ensure that the correct class is being searched.
	 */
	class UClass* CurrentContext;

	/**
	 * Caches the results of GetIndexedNative, for faster lookup.
	 */
	TMap<INT,UFunction*>	NativeFunctionIndexMap;
};

BEGIN_COMMANDLET(AnalyzeParticleSystems,UnrealEd)
	TArray<FString> Tokens;
	TArray<FString> Switches;

	INT ParticleSystemCount;
	INT EmitterCount;
	INT DisabledEmitterCount;
	INT LODLevelCount;
	INT DisabledLODLevelCount;
	INT ModuleCount;
	INT DisabledModuleCount;

	/**
	 *	Startup the commandlet
	 */
	void Startup();

	/**
	 *	Shutdown the commandlet
	 */
	void Shutdown();

	/**
	 *	Process the given particle system
	 *
	 *	@param	InParticleSystem	The particle system to analyze
	 *
	 *	@return	UBOOL				Ignored
	 */
	UBOOL ProcessParticleSystem(UParticleSystem* InParticleSystem);

END_COMMANDLET

/** Commandlet that opens and parses a gamestats file then outputs a stats report */
BEGIN_COMMANDLET(WriteGameStatsReport, UnrealEd)

	/** Actual implementation, shared with editor */
	class FGameStatsReportWorkerFile* Worker;

END_COMMANDLET

//
//
//
/**
 *	Base commandlet for generation collections of objects that meet some criteria
 */
BEGIN_COMMANDLET(BaseObjectTagGenerator,UnrealEd)
	/** The tokens passed via the commandline */
	TArray<FString> Tokens;
	/** The switches passed via the commandline */
	TArray<FString> Switches;

	/** Platform being processed */
	UE3::EPlatformType PlatformType;

	/** 
	 *	The Rate to GC at... default to every 10 packages.
	 *	Specified via -GCRATE=##
	 *	NOTE: Will GC after any map package is processed regardless of this setting!
	 */
	INT GCRate;

	/** If TRUE, skip all script packages. '-SKIPSCRIPT' */
	UBOOL bSkipScriptPackages;
	/** If TRUE, skip map packages. '-SKIPMAPS' */
	UBOOL bSkipMapPackages;
	/** If TRUE, only load map packages. '-MAPSONLY' */
	UBOOL bMapsOnly;
	/** If TRUE, save shader caches every N packages. '-SAVESHADERCACHES' */
	UBOOL bSaveShaderCaches;

	/** 
	 *	If TRUE, clear all tags before starting.
	 *	Specified via '-CLEARTAGS' on the commandline.
	 */
	UBOOL bClearTags;
	/** 
	 *	If TRUE, do not update the tags - simply log to the window.
	 *	Specified via '-SKIPTAGS' on the commandline.
	 */
	UBOOL bSkipTags;
	/** 
	 *	If TRUE, just update the tags (ie only add objects found).
	 *	Otherwise, it will SET the tags on the objects found, clearing any existing.
	 *	Specified via '-TAGUPDATE' on the commandline.
	 *	(This allows for processing a sub-group of packages rather than all)
	 */
	UBOOL bUpdateTagsOnly;

	/** The name of the tag(s) to update */
	TArray<FString> TagNames;

	/** The GAD helper class for updating Tags */
	FGADHelper* GADHelper;

	/** 
	 *	Map of static classes (Object->StaticClass()) of objects the commandlet should process 
	 *	For every object encountered, it will see if Object->StaticClass is in this map.
	 *	If it is, ProcessObject will be called.
	 */
	TMap<UClass*,UBOOL> ObjectClassesOfInterest;

	/** 
	 *	The list(s) of objects to add to the collection(s)
	 *	Indexing will be the same of the entries in TagNames
	 */
	TArray<TMap<FString,UBOOL>>	ObjectsToTag;

	/** The text file that contains map lists */
	FString MapFileName;
	/** Whether to load all sub-levels of maps as well */
	UBOOL bLoadSubLevels;

	/**
	 *	The list of packages to process 
	 */
	TArray<FString> PackageList;

	/**
	 *	Startup the commandlet.
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	virtual UBOOL Startup();

	/**
	 *	Shutdown the commandlet
	 */
	virtual void Shutdown();

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Generate the list of packages to process
	 */
	virtual void GeneratePackageList();

	/**
	 *	Process the list of packages
	 */
	virtual void ProcessPackageList();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 *
	 *	@param	UBOOL		TRUE if the package should be re-saved, FALSE if not.
	 */
	virtual UBOOL ProcessObject(UObject* InObject, UPackage* InPackage);

	/**
	 *	Update the tags with the current lists
	 *
	 *	@return	UBOOL		TRUE if successful, FALSE if not.
	 */
	virtual UBOOL UpdateTags();
END_COMMANDLET

/**
 *	Base commandlet for generation collections of objects that meet some criteria
 */
BEGIN_CHILD_COMMANDLET(FindPSysWithZOrientTowardsCamera,BaseObjectTagGenerator,UnrealEd)
	/**
	 *	Startup the commandlet.
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	virtual UBOOL Startup();

	/**
	 *	Shutdown the commandlet
	 */
	virtual void Shutdown();

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 *
	 *	@param	UBOOL		TRUE if the package should be re-saved, FALSE if not.
	 */
	virtual UBOOL ProcessObject(UObject* InObject, UPackage* InPackage);
END_COMMANDLET

/**
 *	Find particle systems w/ collision enabled.
 */
BEGIN_CHILD_COMMANDLET(FindPSysWithCollisionEnabled,BaseObjectTagGenerator,UnrealEd)
	/**
	 *	Startup the commandlet.
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	virtual UBOOL Startup();

	/**
	 *	Shutdown the commandlet
	 */
	virtual void Shutdown();

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 *
	 *	@param	UBOOL		TRUE if the package should be re-saved, FALSE if not.
	 */
	virtual UBOOL ProcessObject(UObject* InObject, UPackage* InPackage);
END_COMMANDLET

/**
 *	Find particle systems that reference a specific static mesh
 */
BEGIN_CHILD_COMMANDLET(FindPSysReferencesToMesh,BaseObjectTagGenerator,UnrealEd)
	/** The list of meshes that are of interest - specified via the commandline */
	TMap<FString,INT> MeshesOfInterest;

	/**
	 *	Startup the commandlet.
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	virtual UBOOL Startup();

	/**
	 *	Shutdown the commandlet
	 */
	virtual void Shutdown();

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 *
	 *	@param	UBOOL		TRUE if the package should be re-saved, FALSE if not.
	 */
	virtual UBOOL ProcessObject(UObject* InObject, UPackage* InPackage);
END_COMMANDLET

/**
 *	Audit particle module usage...
 */
BEGIN_CHILD_COMMANDLET(ParticleModuleAudit,BaseObjectTagGenerator,UnrealEd)
	/** Total particle system count */
	INT ParticleSystemsProcessed;
	/** Total emitter count */
	INT EmittersProcessed;
	/** ParticleModule to number of times it is used  */
	TMap<FString,INT> ModuleInstances;
	/** ParticleModule to number of times it is used (per-emitter) */
	TMap<FString,INT> ModuleUsages;

	/**
	 *	Startup the commandlet.
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	virtual UBOOL Startup();

	/**
	 *	Shutdown the commandlet
	 */
	virtual void Shutdown();

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 *
	 *	@param	UBOOL		TRUE if the package should be re-saved, FALSE if not.
	 */
	virtual UBOOL ProcessObject(UObject* InObject, UPackage* InPackage);

		/**
	 *	Update the tags with the current lists
	 *
	 *	@return	UBOOL		TRUE if successful, FALSE if not.
	 */
	virtual UBOOL UpdateTags();
END_COMMANDLET

/**
 */
BEGIN_CHILD_COMMANDLET(FindPSysWithBadAutoActivateSetting,BaseObjectTagGenerator,UnrealEd)
	/** The emitter actors w/ bad auto activate settings and the level they are in */
	TMap<FString,FString> BadAutoActivateEmitterToLevel;

	/**
	 *	Startup the commandlet.
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	virtual UBOOL Startup();

	/**
	 *	Shutdown the commandlet
	 */
	virtual void Shutdown();

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Generate the list of packages to process
	 */
	virtual void GeneratePackageList();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 *
	 *	@param	UBOOL		TRUE if the package should be re-saved, FALSE if not.
	 */
	virtual UBOOL ProcessObject(UObject* InObject, UPackage* InPackage);

	/**
	 *	Update the tags with the current lists
	 *
	 *	@return	UBOOL		TRUE if successful, FALSE if not.
	 */
	virtual UBOOL UpdateTags();
END_COMMANDLET

/**
 *	Check PMaps and sub-levels for cross-level StaticMesh clean problems
 */
BEGIN_CHILD_COMMANDLET(FindStaticMeshCleanIssues,BaseObjectTagGenerator,UnrealEd)
	/** Helper structure for tracking StaticMesh --> SubLevel information */
	struct FStaticMeshToSubLevelInfo
	{
	public:
		/** Map of sublevels the mesh is in and whether they are referenced by potential spawners or not */
		TMap<FString,UBOOL> SubLevelsContainedIn;

		FStaticMeshToSubLevelInfo()
		{
		}

		inline UBOOL operator==(const FStaticMeshToSubLevelInfo& Src)
		{
			if (SubLevelsContainedIn.Num() != Src.SubLevelsContainedIn.Num())
			{
				return FALSE;
			}

			for (TMap<FString,UBOOL>::TIterator It(SubLevelsContainedIn); It; ++It)
			{
				FString CheckString = It.Key();
				UBOOL& CheckValue = It.Value();

				const UBOOL* SrcValue = Src.SubLevelsContainedIn.Find(CheckString);
				if (SrcValue == NULL)
				{
					return FALSE;
				}
				if (*SrcValue != CheckValue)
				{
					return FALSE;
				}
			}
			return TRUE;
		}

		inline FStaticMeshToSubLevelInfo& operator=(const FStaticMeshToSubLevelInfo& Src)
		{
			SubLevelsContainedIn = Src.SubLevelsContainedIn;
			return *this;
		}
	};

	/** Persistent map to the static meshes it (and its sublevels) contain */
	TMap<FString,TMap<FString,FStaticMeshToSubLevelInfo> > PMapToStaticMeshMapping;

	TArray<UClass*> ClassesToCheck;
	TMap<FString,UBOOL> SkipStaticMeshCleanMeshes;
	TMap<FString,UBOOL> SkipStaticMeshCleanPackages;
	TMap<FString,UBOOL> CheckStaticMeshCleanClasses;

	/** Persistent Map Info tracking */
	FPersistentMapInfo PersistentMapInfoHelper;

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Generate the list of packages to process
	 */
	virtual void GeneratePackageList();

	/**
	 *	Process the list of packages
	 */
	virtual void ProcessPackageList();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 *
	 *	@param	UBOOL		TRUE if the package should be re-saved, FALSE if not.
	 */
	virtual UBOOL ProcessObject(UObject* InObject, UPackage* InPackage);

	/**
	 *	Update the collections with the current lists
	 *
	 *	@return	UBOOL		TRUE if successful, FALSE if not.
	 */
	virtual UBOOL UpdateTags();
END_COMMANDLET

// BaseDatabase commandlet
BEGIN_COMMANDLET(BaseDatabase,UnrealEd)
	/** The tokens passed via the commandline */
	TArray<FString> Tokens;
	/** The switches passed via the commandline */
	TArray<FString> Switches;
	/** Persistent map info if needed */
	FPersistentMapInfo PersistentMapInfoHelper;
	/** The connection to the database */
	class FDataBaseConnection* DBConnection;
	/** The name of the database to use */
	FString DatabaseName;
	/** The name of the catalog to use */
	FString CatalogName;
	/** The connection timout to use */
	INT ConnectionTimeout;
	/** If TRUE, run in verbose mode */
	UBOOL bVerbose;
	/** If TRUE, ignore objects w/ ':' in their name */
	UBOOL bIgnoreColon;

	/** Uncooked package information... */
	TMap<FString,UBOOL> MapPackages;
	TMap<FString,UBOOL> ScriptPackages;
	TMap<FString,UBOOL> OtherPackages;
	TMap<FString,UBOOL> LinkerNotFoundPackages;
	TMap<FString,UBOOL> ProcessedPackages;

	/** Helper struct for tracking cooked objects */
	struct FCookedObjectInfo
	{
	public:
		/** The class of the object */
		FString ClassName;
		/** The name of the object */
		FString ObjectName;
		/** The total serialized size of the object (in all packages it's contained in) */
		INT TotalSerializeSize;
		/** The packages the object is cooked into, and the serialize size in said package */
		TMap<FString,INT> PackagesCookedInto;

		FCookedObjectInfo()
		{
		}

		inline UBOOL operator==(const FCookedObjectInfo& Src)
		{
			if ((ClassName != Src.ClassName) || 
				(ObjectName != Src.ObjectName) || 
				(TotalSerializeSize != Src.TotalSerializeSize))
			{
				return FALSE;
			}

			if (PackagesCookedInto.Num() != Src.PackagesCookedInto.Num())
			{
				return FALSE;
			}

			for (TMap<FString,INT>::TIterator It(PackagesCookedInto); It; ++It)
			{
				FString CheckString = It.Key();
				INT& CheckValue = It.Value();

				const INT* SrcValue = Src.PackagesCookedInto.Find(CheckString);
				if (SrcValue == NULL)
				{
					return FALSE;
				}
				if (*SrcValue != CheckValue)
				{
					return FALSE;
				}
			}
			return TRUE;
		}

		inline FCookedObjectInfo& operator=(const FCookedObjectInfo& Src)
		{
			ClassName = Src.ClassName;
			ObjectName = Src.ObjectName;
			TotalSerializeSize = Src.TotalSerializeSize;
			PackagesCookedInto = Src.PackagesCookedInto;
			return *this;
		}
	};

	/** Helper struct for tracking package-->cooked object information */
	struct FPackageCookedObjectInfo
	{
	public:
		/** The name of the package */
		FString PackageName;
		/** The objects contained in the map, and their size */
		TMap<FString,INT>	ObjectsContained;

		FPackageCookedObjectInfo()
		{
		}

		inline UBOOL operator==(const FPackageCookedObjectInfo& Src)
		{
			if (PackageName != Src.PackageName)
			{
				return FALSE;
			}

			if (ObjectsContained.Num() != Src.ObjectsContained.Num())
			{
				return FALSE;
			}

			for (TMap<FString,INT>::TIterator It(ObjectsContained); It; ++It)
			{
				FString CheckString = It.Key();
				INT& CheckValue = It.Value();

				const INT* SrcValue = Src.ObjectsContained.Find(CheckString);
				if (SrcValue == NULL)
				{
					return FALSE;
				}
				if (*SrcValue != CheckValue)
				{
					return FALSE;
				}
			}
			return TRUE;
		}

		inline FPackageCookedObjectInfo& operator=(const FPackageCookedObjectInfo& Src)
		{
			PackageName = Src.PackageName;
			ObjectsContained = Src.ObjectsContained;
			return *this;
		}
	};

	/** The class(es) to find in the cooked assets */
	TArray<FString> ClassesOfInterest;
	/** All cooked objects of the classes of interest, and the package(s) they are cooked into */
	TMap<FString, FCookedObjectInfo> CookedObjects;

	/**
	 *	Startup the commandlet.
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	virtual UBOOL Startup();

	/**
	 *	Shutdown the commandlet
	 */
	virtual void Shutdown();

	/**
	 *	Allow the commandlet to do any post-startup initialization
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	virtual UBOOL Initialize();

	/**
	 *	Open the connection to the database
	 *
	 *	@param	UBOOL	TRUE if database connection was opened, FALSE if not
	 */
	virtual UBOOL OpenDatabaseConnection(const TCHAR* InDatabase, const TCHAR* InCatalog, INT InConnectionTimeout);

	/**
	 *	Close the connection to the database
	 *
	 *	@param	UBOOL	TRUE if successfully closed, FALSE if not
	 */
	virtual UBOOL CloseDatabaseConnection();

	/**
	 *	Mine the given database for the objects of interest
	 *
	 *	@param	UBOOL	TRUE if database connection was opened, FALSE if not
	 */
	virtual UBOOL MineDatabaseForObjects();

	/**
	 *	The main function of the commandlet...
	 *
	 */
	virtual UBOOL ProcessMinedData();

	/** Dump the results of the audit */
	virtual void DumpResults();

	/**
	 *	Generate a list of all the PMaps for the game.
	 *
	 *	@param	OutPMapList		The list of found PMaps
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if not
	 */
	virtual UBOOL GeneratePMapList(TArray<FString>& OutPMapList);

	/**
	 *	Validate the given list of map names as real map packages
	 *
	 *	@param	InMapList			The list of maps to validate
	 *	@param	OutValidatedList	Valid maps from the list
	 */
	virtual void ValidateMapList(const TArray<FString>& InMapList, TArray<FString>& OutValidatedList);

	/**
	 *	Helper function for getting an output file to dump results to...
	 *	Caller is expected to close/delete the file!
	 *
	 *	@param	InFolderName		The name of the folder under <UE3>\<GAME>\Logs to create the file in
	 *	@param	InShortFilename		The name of the file to open
	 *	
	 *	@return	FArchive*			The file stream if successful, NULL if not
	 */
	virtual FArchive* GetOutputFile(const TCHAR* InFolderName, const TCHAR* InShortFilename);

	/**
	 *	Gather the cooked assets of interest
	 *
	 *	@param	InClassName						The name of the class to find the cooked assets for
	 *	@param	OutCookedInfoMap				The assets --> cookingobjectinfo map to fill in
	 *	@param	bInIgnoreObjsWithColonInName	If TRUE, ignore objects with ':' in their name (properties, etc.)
	 *	@param	OutPackageToCookedObjectsMap	Optional map of cooked packages to objects contained in it to fill in
	 *	@param	OutCookedPackages				Optional map of cooked packages to fill in
	 *
	 *	@return	UBOOL							TRUE if successful, FALSE if not
	 */
	virtual UBOOL GatherCookedAssetInfo(FString& InClassName, 
		TMap<FString,FCookedObjectInfo >& OutCookedInfoMap, 
		UBOOL bInIgnoreObjsWithColonInName, 
		TMap<FString,FPackageCookedObjectInfo>* OutPackageToCookedObjectsMap, 
		TMap<FString,UBOOL>* OutCookedPackages);

	/**
	 *	Gather the cooked assets of interest
	 *
	 *	@param	InClassNames					An array of the names of the classes to find the cooked assets for
	 *	@param	OutCookedInfoMap				The assets --> cookingobjectinfo map to fill in
	 *	@param	bInIgnoreObjsWithColonInName	If TRUE, ignore objects with ':' in their name (properties, etc.)
	 *	@param	OutPackageToCookedObjectsMap	Optional map of cooked packages to objects contained in it to fill in
	 *	@param	OutCookedPackages				Optional map of cooked packages to fill in
	 *
	 *	@return	UBOOL							TRUE if successful, FALSE if not
	 */
	virtual UBOOL GatherCookedAssetsInfo(TArray<FString>& InClassNames, 
		TMap<FString,FCookedObjectInfo >& OutCookedInfoMap, 
		UBOOL bInIgnoreObjsWithColonInName, 
		TMap<FString,FPackageCookedObjectInfo>* OutPackageToCookedObjectsMap, 
		TMap<FString,UBOOL>* OutCookedPackages);

	/**
	 *	Gather ALL cooked assets
	 *
	 *	@param	OutCookedInfoMap				The assets --> cookingobjectinfo map to fill in
	 *	@param	bInIgnoreObjsWithColonInName	If TRUE, ignore objects with ':' in their name (properties, etc.)
	 *	@param	OutPackageToCookedObjectsMap	Optional map of cooked packages to objects contained in it to fill in
	 *	@param	OutCookedPackages				Optional map of cooked packages to fill in
	 *
	 *	@return	UBOOL							TRUE if successful, FALSE if not
	 */
	virtual UBOOL GatherAllCookedAssetInfo(TMap<FString,FCookedObjectInfo >& OutCookedInfoMap, 
		UBOOL bInIgnoreObjsWithColonInName, 
		TMap<FString,FPackageCookedObjectInfo>* OutPackageToCookedObjectsMap, 
		TMap<FString,UBOOL>* OutCookedPackages);

	/**
	 *	Gather ALL cooked assets in a set of packages
	 *
	 *	@param	InPackageStartsWith				The string the package must start with
	 *	@param	OutCookedInfoMap				The assets --> cookingobjectinfo map to fill in
	 *	@param	bInIgnoreObjsWithColonInName	If TRUE, ignore objects with ':' in their name (properties, etc.)
	 *	@param	OutPackageToCookedObjectsMap	Optional map of cooked packages to objects contained in it to fill in
	 *	@param	OutCookedPackages				Optional map of cooked packages to fill in
	 *
	 *	@return	UBOOL							TRUE if successful, FALSE if not
	 */
	virtual UBOOL GatherAllCookedAssetInfoForPackages(FString& InPackageStartsWith,
		TMap<FString,FCookedObjectInfo >& OutCookedInfoMap, 
		UBOOL bInIgnoreObjsWithColonInName, 
		TMap<FString,FPackageCookedObjectInfo>* OutPackageToCookedObjectsMap, 
		TMap<FString,UBOOL>* OutCookedPackages);

	/**
	 *	Gather ALL cooked assets in the given set of packages
	 *
	 *	@param	InPackages						The array of packages to allow
	 *	@param	OutCookedInfoMap				The assets --> cookingobjectinfo map to fill in
	 *	@param	bInIgnoreObjsWithColonInName	If TRUE, ignore objects with ':' in their name (properties, etc.)
	 *	@param	OutPackageToCookedObjectsMap	Optional map of cooked packages to objects contained in it to fill in
	 *	@param	OutCookedPackages				Optional map of cooked packages to fill in
	 *
	 *	@return	UBOOL					TRUE if successful, FALSE if not
	 */
	virtual UBOOL GatherAllCookedAssetInfoInPackages(const TArray<FString>& InPackages,
		TMap<FString,FCookedObjectInfo >& OutCookedInfoMap, 
		UBOOL bInIgnoreObjsWithColonInName, 
		TMap<FString,FPackageCookedObjectInfo>* OutPackageToCookedObjectsMap, 
		TMap<FString,UBOOL>* OutCookedPackages);

	/**
	 *	Gather the cooked assets of interest in the given set of packages
	 *
	 *	@param	InClassNames					An array of the names of the classes to find the cooked assets for
	 *	@param	InPackages						The array of packages to allow
	 *	@param	OutCookedInfoMap				The assets --> cookingobjectinfo map to fill in
	 *	@param	bInIgnoreObjsWithColonInName	If TRUE, ignore objects with ':' in their name (properties, etc.)
	 *	@param	OutPackageToCookedObjectsMap	Optional map of cooked packages to objects contained in it to fill in
	 *	@param	OutCookedPackages				Optional map of cooked packages to fill in
	 *
	 *	@return	UBOOL							TRUE if successful, FALSE if not
	 */
	virtual UBOOL GatherSpecifiedCookedAssetsInfo(TArray<FString>& InClassNames, 
		const TArray<FString>& InPackages,
		TMap<FString,FCookedObjectInfo >& OutCookedInfoMap, 
		UBOOL bInIgnoreObjsWithColonInName, 
		TMap<FString,FPackageCookedObjectInfo>* OutPackageToCookedObjectsMap, 
		TMap<FString,UBOOL>* OutCookedPackages);

	/**
	 *	Execute the given SQL query and fill in the cooked object info
	 *
	 *	@param	InSQLCommand					The query to execute
	 *	@param	OutCookedInfoMap				The assets --> cookingobjectinfo map to fill in
	 *	@param	bInIgnoreObjsWithColonInName	If TRUE, ignore objects with ':' in their name (properties, etc.)
	 *	@param	OutPackageToCookedObjectsMap	Optional map of cooked packages to objects contained in it to fill in
	 *	@param	OutCookedPackages				Optional map of cooked packages to fill in
	 *
	 *	@return	UBOOL					TRUE if successful, FALSE if not
	 */
	virtual UBOOL ExecuteSQLQuery(const FString& InSQLCommand,
		TMap<FString,FCookedObjectInfo >& OutCookedInfoMap, 
		UBOOL bInIgnoreObjsWithColonInName, 
		TMap<FString,FPackageCookedObjectInfo>* OutPackageToCookedObjectsMap, 
		TMap<FString,UBOOL>* OutCookedPackages);

	/**
	 *	Helper function for dumping simple lists to files.
	 *
	 *	@param	InMap						The map of object names to dump out
	 *	@param	InOptCookedObjectInfoMap	Optional map of CookedObjects --> CookedObjectInfo
	 *	@param	InShortFilename				The name for the output file (short name)
	 *	@param	InObjectClassName			The name of the object class
	 *
	 *	@return	UBOOL						TRUE if successful, FALSE if not
	 */
	virtual UBOOL DumpSimpleMapping(TMap<FString,UBOOL>& InMap, 
		TMap<FString, FCookedObjectInfo>* InOptCookedObjectInfoMap,
		const TCHAR* InShortFilename, const TCHAR* InObjectClassName);

END_COMMANDLET

#include "EngineParticleClasses.h"

BEGIN_CHILD_COMMANDLET(ParticleSystemAudit,BaseDatabase,UnrealEd)
	/** Helper struct for tracking disabled LOD level issues */
	struct FParticleSystemLODInfo
	{
	public:
		/** The method of LOD updating on the particle system */
		ParticleSystemLODMethod LODMethod;
		/** Indices of emitters with disabled LODLevel mismatches */
		TArray<INT>	EmittersWithDisableLODMismatch;
		/** TRUE if the PSys has emitters w/ inter looping mismatches */
		UBOOL bHasInterLoopingMismatch;
		/** TRUE if the PSys has emitters w/ intra looping mismatches */
		UBOOL bHasIntraLoopingMismatch;

		FParticleSystemLODInfo() :
			  bHasInterLoopingMismatch(FALSE)
			, bHasIntraLoopingMismatch(FALSE)
		{
		}

		inline UBOOL operator==(const FParticleSystemLODInfo& Src)
		{
			return (
				(LODMethod == Src.LODMethod) && 
				(EmittersWithDisableLODMismatch == Src.EmittersWithDisableLODMismatch) && 
				(bHasInterLoopingMismatch == Src.bHasInterLoopingMismatch) && 
				(bHasIntraLoopingMismatch == Src.bHasIntraLoopingMismatch)
				);
		}

		inline FParticleSystemLODInfo& operator=(const FParticleSystemLODInfo& Src)
		{
			LODMethod = Src.LODMethod;
			EmittersWithDisableLODMismatch.Empty();
			EmittersWithDisableLODMismatch = Src.EmittersWithDisableLODMismatch;
			bHasInterLoopingMismatch = Src.bHasInterLoopingMismatch;
			bHasIntraLoopingMismatch = Src.bHasIntraLoopingMismatch;
			return *this;
		}
	};

	/** Helper struct for tracking duplicate module info */
	struct FParticleSystemDuplicateModuleInfo
	{
	public:
		/** The number of modules the particle system has */
		INT ModuleCount;
		/** The amount of memory used by those modules */
		INT ModuleMemory;
		/** The number of modules the particle system *could* have w/ duplicates removed */
		INT RemovedDuplicateCount;
		/** The amount of memory there would be w/ duplicates removed */
		INT RemovedDuplicateMemory;

		FParticleSystemDuplicateModuleInfo() :
		ModuleCount(0)
			, ModuleMemory(0)
			, RemovedDuplicateCount(0)
			, RemovedDuplicateMemory(0)
		{
		}

		inline UBOOL operator==(const FParticleSystemDuplicateModuleInfo& Src)
		{
			return (
				(ModuleCount == Src.ModuleCount) &&
				(ModuleMemory == Src.ModuleMemory) &&
				(RemovedDuplicateCount == Src.RemovedDuplicateCount) &&
				(RemovedDuplicateMemory == Src.RemovedDuplicateMemory)
				);
		}

		inline FParticleSystemDuplicateModuleInfo& operator=(const FParticleSystemDuplicateModuleInfo& Src)
		{
			ModuleCount = Src.ModuleCount;
			ModuleMemory = Src.ModuleMemory;
			RemovedDuplicateCount = Src.RemovedDuplicateCount;
			RemovedDuplicateMemory = Src.RemovedDuplicateMemory;
			return *this;
		}
	};

	/** All cooked particle systems, and the package(s) they are cooked into */
	TMap<FString, FCookedObjectInfo> CookedParticleSystems;
	/** All cooked particle system components, and the package(s) they are cooked into */
	TMap<FString, FCookedObjectInfo> CookedParticleSystemComponents;
	/** All cooked emitter actors, and the package(s) they are cooked into */
	TMap<FString, FCookedObjectInfo> CookedEmitterActors;

	/** All cooked particle systems - TRUE if in a map, FALSE if not */
	TMap<FString,UBOOL> PackageReferencedParticleSystems;

	/** All particle systems w/ a NO LOD levels */
	TMap<FString,UBOOL> ParticleSystemsWithNoLODs;
	/** All particle systems w/ a single LOD level */
	TMap<FString,UBOOL> ParticleSystemsWithSingleLOD;
	/** All particle systems w/out fixed bounds set */
	TMap<FString,UBOOL> ParticleSystemsWithoutFixedBounds;
	/** All particle systems w/ LOD Method of Automatic & a check time of 0.0 */
	TMap<FString,UBOOL> ParticleSystemsWithBadLODCheckTimes;
	/** All particle systems w/ bOrientZAxisTowardCamera enabled */
	TMap<FString,UBOOL> ParticleSystemsWithOrientZAxisTowardCamera;
	/** All particle systems w/ missing materials */
	TMap<FString,UBOOL> ParticleSystemsWithMissingMaterials;
	/** All particle systems w/ no emitters */
	TMap<FString,UBOOL> ParticleSystemsWithNoEmitters;
	/** All particle systems w/ collision on in at least one emitter */
	TMap<FString,UBOOL> ParticleSystemsWithCollisionEnabled;
	/** All particle systems w/ a color scale over life that is a constant */
	TMap<FString,UBOOL> ParticleSystemsWithConstantColorScaleOverLife;
	/** All particle systems w/ a color scale over life that is a constant */
	TMap<FString,INT> ParticleSystemsWithConstantColorScaleOverLifeCounts;
	/** Particle systems w/ disabled LOD level matches */
	TMap<FString,FParticleSystemLODInfo> ParticleSystemsWithLODLevelIssues;
	/** Particle system duplicate module information */
	TMap<FString,FParticleSystemDuplicateModuleInfo> PSysDuplicateModuleInfo;

	/**
	 *	Startup the commandlet.
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	virtual UBOOL Startup();

	/** Process all referenced particle systems */
	UBOOL ProcessReferencedParticleSystems();

	/** 
	 *	Check the given ParticleSystem for LOD mismatch issues
	 *
	 *	@param	InPSys		The particle system to check
	 */
	void CheckPSysForLODMismatches(UParticleSystem* InPSys);

	/**
	 *	Determine the given ParticleSystems duplicate module information 
	 *
	 *	@param	InPSys		The particle system to check
	 */
	void CheckPSysForDuplicateModules(UParticleSystem* InPSys);

	/**
	 *	The main function of the commandlet...
	 *
	 */
	virtual UBOOL ProcessMinedData();

	/** Dump the results of the audit */
	virtual void DumpResults();

	/**
	 *	Dump the give list of particle systems to an audit CSV file...
	 *
	 *	@param	InPSysMap		The particle system map to dump
	 *	@param	InFilename		The name for the output file (short name)
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if not
	 */
	UBOOL DumpSimplePSysMapping(TMap<FString,UBOOL>& InPSysMap, const TCHAR* InShortFilename);

END_COMMANDLET

BEGIN_CHILD_COMMANDLET(LensFlareAudit,BaseDatabase,UnrealEd)
	/** Helper struct for tracking occlusion in material info */
	struct FLensFlareOcclusionInfo
	{
	public:
		/** The referneced materials that do *NOT* have occlusion percentage in them */
		TArray<FString> NoOcclusionMaterials;

		FLensFlareOcclusionInfo()
		{
		}

		inline UBOOL operator==(const FLensFlareOcclusionInfo& Src)
		{
			return (
				(NoOcclusionMaterials == Src.NoOcclusionMaterials)
				);
		}

		inline FLensFlareOcclusionInfo& operator=(const FLensFlareOcclusionInfo& Src)
		{
			NoOcclusionMaterials = Src.NoOcclusionMaterials;
			return *this;
		}
	};

	/**
	 *	The volume of the lens flare bound that is considered 'huge'.
	 *	Specified via BOUNDVOL=## on the commandline
	 */
	FLOAT MaxBoundVolume;

	/** All cooked lens flares - TRUE if in a map, FALSE if not */
	TMap<FString,UBOOL> PackageReferencedLensFlares;

	/** All LensFlares with no fixed bounds */
	TMap<FString,UBOOL> LensFlaresWithNoFixedBounds;
	/** All LensFlares with HUGE fixed bounds */
	TMap<FString,UBOOL> LensFlaresWithHugeFixedBounds;
	/** All LensFlares with bad ScreenPercentage maps */
	TMap<FString,UBOOL> LensFlaresWithBadScreenPercentageMaps;
	/** All LensFlares w/ no occlusion percentage in their materials */
	TMap<FString,FLensFlareOcclusionInfo> LensFlaresWithOcclusionMissing;

	/**
	 *	Startup the commandlet.
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	virtual UBOOL Startup();

	/** Process all referenced LensFlares */
	UBOOL ProcessReferencedLensFlares();

	/** 
	 *	Check the given LensFlare for material occlusion problems
	 *
	 *	@param	InLensFlare		The lens flare to check
	 */
	void CheckLensFlareForMaterialOcclusionIssues(ULensFlare* InLensFlare);

	/**
	 *	The main function of the commandlet...
	 *
	 */
	virtual UBOOL ProcessMinedData();

	/** Dump the results of the audit */
	virtual void DumpResults();

	/**
	 *	Dump the give list of LensFlares to an audit CSV file...
	 *
	 *	@param	InLensFlareMap		The lens flare map to dump
	 *	@param	InFilename			The name for the output file (short name)
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE if not
	 */
	UBOOL DumpSimpleLensFlareMapping(TMap<FString,UBOOL>& InLensFlareMap	, const TCHAR* InShortFilename);

END_COMMANDLET

BEGIN_CHILD_COMMANDLET(MaterialAudit,BaseDatabase,UnrealEd)
	/** All cooked materials - TRUE if in a map, FALSE if not */
	TMap<FString,UBOOL> PackageReferencedMaterials;

	/** All materials using obsolete DepthBias* expressions */
	TMap<FString,UBOOL> MaterialsUsingObsoleteDepthBiasExpressions;

	/**
	 *	Startup the commandlet.
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	virtual UBOOL Startup();

	/** Process all referenced materials */
	UBOOL ProcessReferencedMaterials();

	/**
	 *	The main function of the commandlet...
	 *
	 */
	virtual UBOOL ProcessMinedData();

	/** Dump the results of the audit */
	virtual void DumpResults();

END_COMMANDLET

BEGIN_CHILD_COMMANDLET(AnalyzeDVDSpace,BaseDatabase,UnrealEd)
	/** The cooked objects found in the database... */
	TMap<FString,FCookedObjectInfo> CookedObjectInfo;
	/** Package to cooked objects contained in it mapping */
	TMap<FString,FPackageCookedObjectInfo> PackageCookedObjectInfo;
	/** List of unique cooked packages */
	TMap<FString,UBOOL> UniqueCookedPackages;

	/** If TRUE, ignore 'faked' TextureFileCache objects */
	UBOOL bIgnoreTFCEntries;

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Mine the given database for the objects of interest
	 *
	 *	@param	UBOOL	TRUE if database connection was opened, FALSE if not
	 */
	virtual UBOOL MineDatabaseForObjects();

	/**
	 *	The main function of the commandlet...
	 *
	 */
	virtual UBOOL ProcessMinedData();

	/** Dump the results of the audit */
	virtual void DumpResults();

	/**
	 *	Dump the given cooked object list to the given file
	 *
	 *	@param	InShortFilename		The short filename to dump the info to
	 *	@param	InCookedObjInfo		The map of cooked object info to dump to the file
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE if not
	 */
	UBOOL DumpCookedObjectList(const TCHAR* InShortFilename, TMap<FString,FCookedObjectInfo>& InCookedObjInfo);

END_COMMANDLET

BEGIN_CHILD_COMMANDLET(FindCinematicTextures,BaseDatabase,UnrealEd)
	/** List of textures that are in TEXTUREGROUP_Cinematic */
	TMap<FString,UBOOL> CinematicCookedTextures;

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	The main function of the commandlet...
	 *
	 */
	virtual UBOOL ProcessMinedData();

	/** Dump the results of the audit */
	virtual void DumpResults();

END_COMMANDLET

BEGIN_CHILD_COMMANDLET(PMapForceObjectCheck,BaseDatabase,UnrealEd)
	/** The percentage of sublevels an object has to be in to qualify for forced PMap status */
	FLOAT SubLevelPercentage;
	/** If TRUE, gather classes only. -CLASSESONLY on commandline */
	UBOOL bClassesOnly;
	/** If TRUE, gather from script levels only. -SCRIPTLEVELSONLY on commandline */
	UBOOL bScriptLevelsOnly;
	/** If TRUE, write out the cooked objects. -DUMPOBJS on commandline */
	UBOOL bOutputObjectLists;

	/** Skips files containing any of these. */
	TArray<FString> SkipNameFragments;

	/** Package to cooked objects contained in it mapping */
	TMap<FString,FPackageCookedObjectInfo> PackageCookedObjectInfo;

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Mine the given database for the objects of interest
	 *
	 *	@param	UBOOL	TRUE if database connection was opened, FALSE if not
	 */
	virtual UBOOL MineDatabaseForObjects();

	/**
	 *	The main function of the commandlet...
	 *
	 */
	virtual UBOOL ProcessMinedData();

	/** Dump the results of the audit */
	virtual void DumpResults();

END_COMMANDLET

//
//
//
/**
 *	Base commandlet for processing objects contained in a collection.
 *	Default is to use the one generated by the TagCookedReferencedAssets 
 *	commandlet (ie, object that are known to be 'OnDVD')
 */
BEGIN_COMMANDLET(BaseCollectionProcessing,UnrealEd)
	/** The tokens passed via the commandline */
	TArray<FString> Tokens;
	/** The switches passed via the commandline */
	TArray<FString> Switches;

	/** Platform being processed */
	UE3::EPlatformType PlatformType;

	/** The name of the collection to process */
	FString CollectionName;

	/** If TRUE, the collection is private, FALSE it is public (the default) */
	UBOOL bPrivateCollection;
	/** If TRUE, the collection is local, FALSE it is public (the default) */
	UBOOL bLocalCollection;

	/** 
	 *	The Rate to GC at... default to every 10 packages.
	 *	Specified via -GCRATE=##
	 *	NOTE: Will GC after any map package is processed regardless of this setting!
	 */
	INT GCRate;

	/** If TRUE, save shader caches every N packages. '-SAVESHADERCACHES' */
	UBOOL bSaveShaderCaches;

	/** The GAD helper class for updating Tags */
	class FGADHelper* GADHelper;

	/** 
	 *	List of the class names that the commandlet should process.
	 *	For every object in the collection, it will see if the class is in this map.
	 *	If it is, ProcessObject will be called on that object.
	 */
	TMap<FString,UBOOL> ObjectClassesOfInterest;

	/** 
	 *	The list of objects found that were of the ObjectClassesOfInterest.
	 *	These are the objects that will be processed by the commandlet.
	 */
	TArray<FString> FoundObjectsOfInterest;

	/**
	 *	Startup the commandlet.
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	virtual UBOOL Startup();

	/**
	 *	Shutdown the commandlet
	 */
	virtual void Shutdown();

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Gather the list of assets in the collection
	 */
	virtual UBOOL GatherAssetNamesFromCollection();

	/**
	 *	Process the list of objects
	 */
	virtual void ProcessFoundObjectList();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 */
	virtual void ProcessObject(UObject* InObject, UPackage* InPackage);

	/**
	 *	Dump the results of the commandlet
	 */
	virtual void DumpResults();

	/**
	 *	Helper function for getting a diagnostic table writer
	 *	CALLER MUST SHUT IT DOWN/DELETE IT PROPERLY!!!!!
	 *
	 *	@param	InFilename						The name of the file to write the table to
	 *	
	 *	@return	FDiagnosticTableWriterCSV*		The diagnostic table writer if successful, NULL if not.
	 */
	virtual class FDiagnosticTableWriterCSV* GetTableWriter(const TCHAR* InFilename);
END_COMMANDLET

/**
 *	Find particle systems w/ collision enabled.
 */
BEGIN_CHILD_COMMANDLET(FindOnDVDPSysWithCollisionEnabled,BaseCollectionProcessing,UnrealEd)
	/** All particle systems w/ collision on in at least one emitter */
	TMap<FString,UBOOL> ParticleSystemsWithCollisionEnabled;

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 */
	virtual void ProcessObject(UObject* InObject, UPackage* InPackage);

	/**
	 *	Dump the results of the commandlet
	 */
	virtual void DumpResults();
END_COMMANDLET

/**
 *	Find particle systems w/ MediumDetailSpawnRate set to something other than 1.0.
 */
BEGIN_CHILD_COMMANDLET(FindOnDVDPSysWithMedDetailSpawnRate,BaseCollectionProcessing,UnrealEd)
	/** All particle systems w/ MediumDetailSpawnRateScale != 1.0 */
	TMap<FString,TArray<FLOAT> > ParticleSystemsWithMedDetailSpawnRate;

	/** 
	 *	If TRUE, tag emitters that have a MediumDetailSpawnRateScale that is != 1.0 or 0.0.
	 *	Specified by passing in -PARTIAL on the commandline
	 *	Default (FALSE) will tag emitters that have a MediumDetailSpawnRateScale that is != 1.0.
	 */
	UBOOL bTagPartialValuesOnly;

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 */
	virtual void ProcessObject(UObject* InObject, UPackage* InPackage);

	/**
	 *	Dump the results of the commandlet
	 */
	virtual void DumpResults();
END_COMMANDLET

/**
 *	Find particle systems w/ MediumDetailSpawnRate set to something other than 1.0.
 */
BEGIN_CHILD_COMMANDLET(FindOnDVDPSystems,BaseCollectionProcessing,UnrealEd)
	/** All particle systems found in collection */
	TMap<FString,UBOOL> ParticleSystemsOnDVD;

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 */
	virtual void ProcessObject(UObject* InObject, UPackage* InPackage);

	/**
	 *	Dump the results of the commandlet
	 */
	virtual void DumpResults();
END_COMMANDLET

/**
 *	Find particle systems w/ no fixed bounds set.
 */
BEGIN_CHILD_COMMANDLET(FindOnDVDPSysWithNoFixedBounds,BaseCollectionProcessing,UnrealEd)
	/** All particle systems found in collection with no fixed bounds set*/
	TMap<FString,UBOOL> ParticleSystemsOnDVDWithNoFixedBounds;

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 */
	virtual void ProcessObject(UObject* InObject, UPackage* InPackage);

	/**
	 *	Dump the results of the commandlet
	 */
	virtual void DumpResults();
END_COMMANDLET

/**
 *	particle systems DynamicParameter audit
 */
BEGIN_CHILD_COMMANDLET(FindOnDVDPSysDynamicParameter,BaseCollectionProcessing,UnrealEd)
	/** Helper struct w/ dynamic parameter info */
	struct FDynParamInfoHelper
	{
	public:
		/** If this emitter uses dynamic parameter or not */
		UBOOL	bUsesDynamicParameter;
		/** The name of the material used with this emitter */
		FString	MaterialName;
		/** The way the material uses dynamic parmaeter */
		INT		MtrlUsageFlags;
		/** The SpawnTimeOnly settings for the module */
		INT		SpawnTimeOnlyFlags;
		/** The velocity settings for the module */
		INT		VelocityUsageFlags;
		/** The Constant value (vs curve, etc) settings for the module */
		INT		ConstantValueFlags;

		FDynParamInfoHelper()
		{
		}

		inline UBOOL operator==(const FDynParamInfoHelper& Other) const
		{
			return (
				(bUsesDynamicParameter == Other.bUsesDynamicParameter) &&
				(MaterialName == Other.MaterialName) &&
				(MtrlUsageFlags == Other.MtrlUsageFlags) &&
				(SpawnTimeOnlyFlags == Other.SpawnTimeOnlyFlags) &&
				(ConstantValueFlags == Other.ConstantValueFlags)
				);
		}
		inline FDynParamInfoHelper& operator=(const FDynParamInfoHelper& Other)
		{
			bUsesDynamicParameter = Other.bUsesDynamicParameter;
			MaterialName = Other.MaterialName;
			MtrlUsageFlags = Other.MtrlUsageFlags;
			SpawnTimeOnlyFlags = Other.SpawnTimeOnlyFlags;
			ConstantValueFlags = Other.ConstantValueFlags;
			return *this;
		}
	};

	/** All particle systems found with a dynamic parameter module */
	TMap<FString,TArray<FDynParamInfoHelper> > ParticleSystemsOnDVDDynamicParameterUsage;
	/**
	 *	If TRUE, only list ones that are not used in the material.
	 *	Specified by commandline option -NOMATERIALUSAGE
	 */
	UBOOL bNoMaterialUsageOnly;

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 */
	virtual void ProcessObject(UObject* InObject, UPackage* InPackage);

	/**
	 *	Dump the results of the commandlet
	 */
	virtual void DumpResults();
END_COMMANDLET

//
//
//
/**
 *	Find AnimNotify instances that do not have the AnimSeqeunce that 'owns' them as their outer.
 */
BEGIN_CHILD_COMMANDLET(FindBadAnimNotify,BaseCollectionProcessing,UnrealEd)
	/** All Bad Animnotifier map with animsequence:animnotifier*/
	TMap<FString,FString> BadAnimNotifiers;

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 */
	virtual void ProcessObject(UObject* InObject, UPackage* InPackage);

	/**
	 *	Dump the results of the commandlet
	 */
	virtual void DumpResults();
END_COMMANDLET

BEGIN_CHILD_COMMANDLET(FindOnDVDCinematicTextures,BaseCollectionProcessing,UnrealEd)
	/** List of textures that are in TEXTUREGROUP_Cinematic */
	TMap<FString,UBOOL> CinematicCookedTextures;

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 */
	virtual void ProcessObject(UObject* InObject, UPackage* InPackage);

	/**
	 *	Dump the results of the commandlet
	 */
	virtual void DumpResults();
END_COMMANDLET

/** Commandlet that allows unit tests to be run */
BEGIN_COMMANDLET(UnitTest, UnrealEd)
END_COMMANDLET

BEGIN_CHILD_COMMANDLET(FixupMobileMaterialFog,BaseObjectTagGenerator,UnrealEd)
	/**
	 *	Startup the commandlet.
	 *
	 *	@return	UBOOL	TRUE if successful, FALSE if not
	 */
	virtual UBOOL Startup();

	/**
	 *	Fill in the collection names and classes of interest
	 */
	virtual UBOOL Initialize();

	/**
	 *	Process the given object
	 *
	 *	@param	InObject	The object to process
	 *	@param	InPackage	The package being processed
	 *
	 *	@param	UBOOL		TRUE if the package should be re-saved, FALSE if not.
	 */
	virtual UBOOL ProcessObject(UObject* InObject, UPackage* InPackage);
END_COMMANDLET

BEGIN_COMMANDLET(ReplaceMaterial,UnrealEd)
END_COMMANDLET

#endif
