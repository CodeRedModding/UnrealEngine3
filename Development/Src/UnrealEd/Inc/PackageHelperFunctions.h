/**
 * File to hold common package helper functions.
 *
 * 	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _PACKAGE_HELPER_FUNCTIONS_H_
#define _PACKAGE_HELPER_FUNCTIONS_H_


/**
 * Flags which modify the way that NormalizePackageNames works.
 */
enum EPackageNormalizationFlags
{
	/** reset the linker for any packages currently in memory that are part of the output list */
	NORMALIZE_ResetExistingLoaders		= 0x01,
	/** do not include map packages in the result array; only relevant if the input array is empty */
	NORMALIZE_ExcludeMapPackages		= 0x02,
	/** do not include content packages in the result array; only relevant if the input array is empty */
	NORMALIZE_ExcludeContentPackages	= 0x04,
	/** include cooked packages in the result array; only relevant if the input array is empty */
	NORMALIZE_IncludeCookedPackages 	= 0x10,
	
	/** Combo flags */
	NORMALIZE_DefaultFlags				= NORMALIZE_ResetExistingLoaders,
};

void SearchDirectoryRecursive( const FFilename& SearchPathMask, TArray<FString>& out_PackageNames, TArray<FFilename>& out_PackageFilenames );

/**
 * Takes an array of package names (in any format) and converts them into relative pathnames for each package.
 *
 * @param	PackageNames		the array of package names to normalize.  If this array is empty, the complete package list will be used.
 * @param	PackagePathNames	will be filled with the complete relative path name for each package name in the input array
 * @param	PackageWildcard		if specified, allows the caller to specify a wildcard to use for finding package files
 * @param	PackageFilter		allows the caller to limit the types of packages returned.
 *
 * @return	TRUE if packages were found successfully, FALSE otherwise.
 */
UBOOL NormalizePackageNames( TArray<FString> PackageNames, TArray<FFilename>& PackagePathNames, const FString& PackageWildcard=FString(TEXT("*.*")), BYTE PackageFilter=NORMALIZE_DefaultFlags );


/** 
 * Helper function to save a package that may or may not be a map package
 *
 * @param	Package		The package to save
 * @param	Filename	The location to save the package to
 * @param	KeepObjectFlags	Objects with any these flags will be kept when saving even if unreferenced.
 * @param	ErrorDevice	the output device to use for warning and error messages
 * @param	LinkerToConformAgainst
 * @param				optional linker to use as a base when saving Package; if specified, all common names, imports and exports
 *						in Package will be sorted in the same order as the corresponding entries in the LinkerToConformAgainst
 *
 * @return TRUE if successful
 */
UBOOL SavePackageHelper(UPackage* Package, FString Filename,  EObjectFlags KeepObjectFlags = RF_Standalone, FOutputDevice* ErrorDevice=GWarn, ULinkerLoad* LinkerToConformAgainst=NULL);


/**
 *	Collection helper
 *	Used to create and update ContentBrowser collections
 *
 */
class FGADHelper
{
public:
	FGADHelper() :
		bInitialized(FALSE),
		bWeInitializedGAD(FALSE)
	{
	}

	~FGADHelper()
	{
		if (bInitialized == TRUE)
		{
			Shutdown();
		}
	}

	/**
	 *	Initialize the Collection helper
	 *	
	 *	@return	UBOOL					TRUE if successful, FALSE if failed
	 */
	UBOOL Initialize();

	/**
	 *	Shutdown the collection helper
	 */
	void Shutdown();
	
	/**
	 *	Create a new tag
	 *	
	 *	@param	InTagName		The name of the tag to create
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	UBOOL CreateTag( const FString& InTagName );

	/**
	 *	Clear the given collection
	 *	
	 *	@param	InCollectionName		The name of the collection to clear
	 *	@param	InType					Type of collection
	 *
	 *	@return	UBOOL					TRUE if successful, FALSE if failed
	 */
	UBOOL ClearCollection(const FString& InCollectionName, const INT InType);

	/**
	 *  Clear this tag from all assets (and delete it)
	 *
	 *  @param  InTagName  The name of the collection to create
	 *
	 *  @return UBOOL      TRUE if successful, FALSE if failed
	 */
	UBOOL ClearTagFromAssets( const FString& InTagName );

	/**
	 *	Fill the given collection with the given list of assets
	 *
	 *	@param	InCollectionName	The name of the collection to fill
	 *	@param	InType				Type of collection
	 *	@param	InAssetList			The list of items to fill the collection with (can be empty)
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE if not.
	 */
	UBOOL SetCollection(const FString& InCollectionName, const INT InType, const TArray<FString>& InAssetList);


	/**
	 *	Assign this tag to the given set of assets; remove it from all other assets.
	 *
	 *	@param  InTagName    The name of the collection to fill
	 *	@param  InAssetList  The list of items that should have this tag
	 *
	 *	@return UBOOL        TRUE if successful, FALSE if not.
	 */
	UBOOL SetTaggedAssets( const FString& InTagName, const TArray<FString>& InAssetList );

	/**
	 *	Update the given collection with the lists of adds/removes
	 *
	 *	@param	InCollectionName	The name of the collection to update
	 *	@param	InType				Type of collection
	 *	@param	InAddList			The list of items to ADD to the collection (can be empty)
	 *	@param	InRemoveList		The list of items to REMOVE from the collection (can be empty)
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE if not.
	 */
	UBOOL UpdateCollection(const FString& InCollectionName, const INT InType, const TArray<FString>& InAddList, const TArray<FString>& InRemoveList);


	/**
	 *	Update the given tag with the lists of adds/removes
	 *
	 *	@param InTagName     The name of the collection to update
	 * 	@param InAddList     List of assets to be tagged with given tag
	 *	@param InRemoveList  List of assets from which to remove this tag
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE if not.
	 */
	UBOOL UpdateTaggedAssets( const FString& InTagName, const TArray<FString>& InAddList, const TArray<FString>& InRemoveList );

	/**
	 *	Retrieve the assets contained in the given collection.
	 *
	 *	@param	InCollectionName	Name of collection to query
	 *	@param	InType				Type of collection
	 *	@param	OutAssetFullName	The assets contained in the collection
	 * 
	 *	@return True if collection was created successfully
	 */
	UBOOL QueryAssetsInCollection(const FString& InCollectionName, const INT InType, TArray<FString>& OutAssetFullNames);

	/**
	 * Retrieve the assets tagged with the specified tag name.
	 * 
	 * @param InTagName          Name of tag to look up on assets.
	 * @param OutAssetFullNames  Names of assets tagged with InTagName
	 *
	 * @return TRUE if successfully retrieved assets.
	 */
	UBOOL QueryTaggedAssets( const FString& InTagName, TArray<FString>& OutAssetFullNames );


	/**
	 * Finds all of the assets with all of the specified tags
	 *
	 * @param	InTags				List of tags.  For an asset to be returned, it must be associated with all of these tags.
	 * @param	OutAssetFullNames	[Out] List of assets found
	 */
	void QueryAssetsWithAllTags( const TArray< FString >& InTags, TArray< FString >& OutAssetFullNames );


protected:
	/** Clears the content of a Tag or Collection */
	template <class AssetSetPolicy>	UBOOL ClearAssetSet ( const FString& InSetName, const INT InSetType );
	
	/** Sets the contents of a Tag or Collection to be the InAssetList. Assets not mentioned in the list will be untagged. */
	template <class AssetSetPolicy>	UBOOL AssignSetContent( const FString& InSetName, const INT InType, const TArray<FString>& InAssetList );
	
	/** Add and remove assets for the specified Tag or Connection. Assets from InAddList are added; assets from InRemoveList are removed. */
	template <class AssetSetPolicy>	UBOOL UpdateSetContent( const FString& InSetName, const INT InType, const TArray<FString>& InAddList, const TArray<FString>& InRemoveList );
	
	/** Get the list of all assets in the specified Collection or Tag */
	template <class AssetSetPolicy>	UBOOL QuerySetContent( const FString& InCollectionName, const INT InType, TArray<FString>& OutAssetFullNames );

	UBOOL bInitialized;

	/** True if we initialized the game asset database ourselves.  Otherwise the GAD was already initialized and
	    we'll assume it will stay initialized until our GAD operations complete. */
	UBOOL bWeInitializedGAD;
};

#if CONSOLE
template< typename OBJECTYPE >
void DoIt( UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches ){}
#endif



#if WITH_MANAGED_CODE
extern FGADHelper CommandletGADHelper;
#endif // WITH_MANAGED_CODE


/**  This will clear the passed in tag from all asssets **/
void UpdateGADClearTags( const FString& TagName );

/** This will set the passed in tag name on the object if it is not in the whitelist **/
void UpdateGADSetTags( const FString& ObjectName, const FString& TagName, const FString& TagNameWhitelist );

/** 
 *	This will set the passed in tag name on the objects if they are not in the whitelist
 *
 *	@param	ObjectNames			The list of object names to tag
 *	@param	TagName				The tag to apply to the objects
 *	@param	TagNameWhitelist	The tag of objects that should *not* be tagged (whitelist)
 */
void UpdateGADSetTagsForObjects(TMap<FString,UBOOL>& ObjectNames, const FString& TagName, const FString& TagNameWhitelist);

/**
 * This is our Functional "Do an Action to all Packages" Template.  Basically it handles all
 * of the boilerplate code which normally gets copy pasted around.  So now we just pass in
 * the OBJECTYPE  (e.g. Texture2D ) and then the Functor which will do the actual work.
 *
 * @see UFindMissingPhysicalMaterialsCommandlet
 * @see UFindTexturesWhichLackLODBiasOfTwoCommandlet
 **/
template< typename OBJECTYPE, typename FUNCTOR >
void DoActionToAllPackages( UCommandlet* Commandlet, const FString& Params )
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	warnf(TEXT("%s"), *Params);

	const TCHAR* Parms = *Params;
	UCommandlet::ParseCommandLine(Parms, Tokens, Switches);

	const UBOOL bVerbose = Switches.ContainsItem(TEXT("VERBOSE"));
	const UBOOL bLoadMaps = Switches.ContainsItem(TEXT("LOADMAPS"));
	const UBOOL bOverrideLoadMaps = Switches.ContainsItem(TEXT("OVERRIDELOADMAPS"));
	const UBOOL bOnlyLoadMaps = Switches.ContainsItem(TEXT("ONLYLOADMAPS"));
	const UBOOL bSkipReadOnly = Switches.ContainsItem(TEXT("SKIPREADONLY"));
	const UBOOL bOverrideSkipOnly = Switches.ContainsItem(TEXT("OVERRIDEREADONLY"));
	const UBOOL bGCEveryPackage = Switches.ContainsItem(TEXT("GCEVERYPACKAGE"));
	const UBOOL bSaveShaderCaches = Switches.ContainsItem(TEXT("SAVESHADERS"));
	const UBOOL bSkipScriptPackages = Switches.ContainsItem(TEXT("SKIPSCRIPTPACKAGES"));

	TArray<FString> FilesInPath;
	FilesInPath = GPackageFileCache->GetPackageFileList();


#if WITH_MANAGED_CODE
	CommandletGADHelper.Initialize();
#endif // WITH_MANAGED_CODE

	{
		FUNCTOR TheFunctor;
		TheFunctor.CleanUpGADTags();
	}


	INT GCIndex = 0;
	for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		const FFilename& Filename = FilesInPath(FileIndex);

		const UBOOL	bIsShaderCacheFile		= FString(*Filename).ToUpper().InStr( TEXT("SHADERCACHE") ) != INDEX_NONE;
		const UBOOL	bIsAutoSave				= FString(*Filename).ToUpper().InStr( TEXT("AUTOSAVES") ) != INDEX_NONE;
		const UBOOL bIsReadOnly             = GFileManager->IsReadOnly( *Filename );

		// we don't care about trying to wrangle the various shader caches so just skipz0r them
		if(	Filename.GetBaseFilename().InStr( TEXT("LocalShaderCache") )	!= INDEX_NONE
			||	Filename.GetBaseFilename().InStr( TEXT("RefShaderCache") )		!= INDEX_NONE
			|| ( bIsAutoSave == TRUE )
			)
		{
			continue;
		}

		

		// Skip over script packages
		if( bSkipScriptPackages && Filename.GetExtension() == TEXT("u") )
		{
			continue;
		}

		// See if we should skip read only packages
		if( bSkipReadOnly && !bOverrideSkipOnly )
		{
			const UBOOL bIsReadOnly = GFileManager->IsReadOnly( *Filename );
			if( bIsReadOnly )
			{
				warnf(TEXT("Skipping %s (read-only)"), *Filename);			
				continue;
			}
		}

		// if we don't want to load maps for this
		if( ((!bLoadMaps && !bOnlyLoadMaps) || bOverrideLoadMaps) && ( Filename.GetExtension() == FURL::DefaultMapExt ) )
		{
			continue;
		}

		// if we only want to load maps for this
		if( ( bOnlyLoadMaps == TRUE ) && ( Filename.GetExtension() != FURL::DefaultMapExt ) )
		{
			continue;
		}

		if( bVerbose == TRUE )
		{
			warnf( NAME_Log, TEXT("Loading %s"), *Filename );
		}

		// don't die out when we have a few bad packages, just keep on going so we get most of the data
		try
		{
			UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_None );
			if( Package != NULL )
			{
				FUNCTOR TheFunctor;
				TheFunctor.DoIt<OBJECTYPE>( Commandlet, Package, Tokens, Switches );
			}
			else
			{
				warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
			}
		}
		catch ( ... )
		{
			warnf( NAME_Log, TEXT("Exception %s"), *Filename.GetBaseFilename() );
		}

		if( ( (++GCIndex % 10) == 0 ) || ( bGCEveryPackage == TRUE ) )
		{
			UObject::CollectGarbage(RF_Native);
		}

		if (bSaveShaderCaches == TRUE)
		{
			SaveLocalShaderCaches();
		}
	}

#if WITH_MANAGED_CODE
	CommandletGADHelper.Shutdown();
#endif // WITH_MANAGED_CODE
}

#endif // _PACKAGE_HELPER_FUNCTIONS_H_
