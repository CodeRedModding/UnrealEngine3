/*=============================================================================
UnPackageUtilities.cpp: Commandlets for viewing information about package files
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "UnrealEd.h"
#include "EngineMaterialClasses.h"
#include "EngineSequenceClasses.h"
#include "UnPropertyTag.h"
#include "EngineUIPrivateClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineParticleClasses.h"
#include "LensFlare.h"
#include "EngineAnimClasses.h"
#include "UnTerrain.h"
#include "EngineFoliageClasses.h"
#include "SpeedTree.h"
#include "UnTerrain.h"
#include "EnginePrefabClasses.h"
#include "Database.h"
#include "EngineSoundClasses.h"

#include "SourceControl.h"

#include "PackageHelperFunctions.h"
#include "PackageUtilityWorkers.h"

#include "PerfMem.h"
#include "AnimationEncodingFormat.h"
#include "AnimationUtils.h"
#include "AnimationCompression.h"

#if WITH_MANAGED_CODE
#include "GameAssetDatabaseShared.h"
#endif // WITH_MANAGED_CODE

#include "DiagnosticTable.h"

/*-----------------------------------------------------------------------------
	Package Helper Functions (defined in PackageHelperFunctions.h
-----------------------------------------------------------------------------*/

void SearchDirectoryRecursive( const FFilename& SearchPathMask, TArray<FString>& out_PackageNames, TArray<FFilename>& out_PackageFilenames )
{
	const FFilename SearchPath = SearchPathMask.GetPath();
	TArray<FString> PackageNames;
	GFileManager->FindFiles( PackageNames, *SearchPathMask, TRUE, FALSE );
	if ( PackageNames.Num() > 0 )
	{
		for ( INT PkgIndex = 0; PkgIndex < PackageNames.Num(); PkgIndex++ )
		{
			new(out_PackageFilenames) FFilename( SearchPath * PackageNames(PkgIndex) );
		}

		out_PackageNames += PackageNames;
	}

	// now search all subdirectories
	TArray<FString> Subdirectories;
	GFileManager->FindFiles( Subdirectories, *(SearchPath * TEXT("*")), FALSE, TRUE );
	for ( INT DirIndex = 0; DirIndex < Subdirectories.Num(); DirIndex++ )
	{
		SearchDirectoryRecursive( SearchPath * Subdirectories(DirIndex) * SearchPathMask.GetCleanFilename(), out_PackageNames, out_PackageFilenames);
	}
}

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
UBOOL NormalizePackageNames( TArray<FString> PackageNames, TArray<FFilename>& PackagePathNames, const FString& PackageWildcard, BYTE PackageFilter )
{
	if ( PackageNames.Num() == 0 )
	{
		GFileManager->FindFiles( PackageNames, *PackageWildcard, TRUE, FALSE );
	}

	if( PackageNames.Num() == 0 )
	{
		// if no files were found, it might be an unqualified path; try prepending the .u output path
		// if one were going to make it so that you could use unqualified paths for package types other
		// than ".u", here is where you would do it
		SearchDirectoryRecursive( appScriptOutputDir() * PackageWildcard, PackageNames, PackagePathNames );
		if ( PackageNames.Num() == 0 )
		{
			TArray<FString> Paths;
			if ( GConfig->GetArray( TEXT("Core.System"), TEXT("Paths"), Paths, GEngineIni ) > 0 )
			{
				for ( INT i = 0; i < Paths.Num(); i++ )
				{
					FFilename SearchWildcard = Paths(i) * PackageWildcard;
					SearchDirectoryRecursive( SearchWildcard, PackageNames, PackagePathNames );
				}
			}
		}
		else
		{
			PackagePathNames.Empty(PackageNames.Num());

			// re-add the path information so that GetPackageLinker finds the correct version of the file.
			FFilename WildcardPath = appScriptOutputDir() * PackageWildcard;
			for ( INT FileIndex = 0; FileIndex < PackageNames.Num(); FileIndex++ )
			{
				PackagePathNames.AddItem( WildcardPath.GetPath() * PackageNames(FileIndex) );
			}
		}

		// Try finding package in package file cache.
		if ( PackageNames.Num() == 0 )
		{
			FString Filename;
			if( GPackageFileCache->FindPackageFile( *PackageWildcard, NULL, Filename ) )
			{
				new(PackagePathNames) FString(Filename);
			}
		}
	}
	else
	{
		// re-add the path information so that GetPackageLinker finds the correct version of the file.
		const FString WildcardPath = FFilename(*PackageWildcard).GetPath();
		for ( INT FileIndex = 0; FileIndex < PackageNames.Num(); FileIndex++ )
		{
			PackagePathNames.AddItem(WildcardPath * PackageNames(FileIndex));
		}
	}

	if ( PackagePathNames.Num() == 0 )
	{
		warnf(TEXT("No packages found using '%s'!"), *PackageWildcard);
		return FALSE;
	}

	// now apply any filters to the list of packages
	for ( INT PackageIndex = PackagePathNames.Num() - 1; PackageIndex >= 0; PackageIndex-- )
	{
		FString PackageExtension = PackagePathNames(PackageIndex).GetExtension();
		if ( !GSys->Extensions.ContainsItem(PackageExtension) )
		{
			// not a valid package file - remove it
			PackagePathNames.Remove(PackageIndex);
		}
		else
		{
			if ( (PackageFilter&NORMALIZE_ExcludeMapPackages) != 0 )
			{
				if ( PackageExtension == FURL::DefaultMapExt )
				{
					PackagePathNames.Remove(PackageIndex);
					continue;
				}
			}
			if ( (PackageFilter&NORMALIZE_ExcludeContentPackages) != 0 )
			{
				if ( PackageExtension != FURL::DefaultMapExt )
				{
					PackagePathNames.Remove(PackageIndex);
					continue;
				}
			}
			if ( (PackageFilter&NORMALIZE_IncludeCookedPackages) == 0 )
			{
				if ( PackageExtension == TEXT("xxx") )
				{
					PackagePathNames.Remove(PackageIndex);
					continue;
				}
			}
		}
	}

	if ( (PackageFilter&NORMALIZE_ResetExistingLoaders) != 0 )
	{
		// reset the loaders for the packages we want to load so that we don't find the wrong version of the file
		for ( INT PackageIndex = 0; PackageIndex < PackageNames.Num(); PackageIndex++ )
		{
			// (otherwise, attempting to run a commandlet on e.g. Engine.xxx will always return results for Engine.u instead)
			const FString& PackageName = FPackageFileCache::PackageFromPath(*PackageNames(PackageIndex));
			UPackage* ExistingPackage = FindObject<UPackage>(NULL, *PackageName, TRUE);
			if ( ExistingPackage != NULL )
			{
				UObject::ResetLoaders(ExistingPackage);
			}
		}
	}

	return TRUE;
}


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

* @return TRUE if successful
*/
UBOOL SavePackageHelper(UPackage* Package, FString Filename, EObjectFlags KeepObjectFlags, FOutputDevice* ErrorDevice, ULinkerLoad* LinkerToConformAgainst )
{
	// look for a world object in the package (if there is one, there's a map)
	UWorld* World = FindObject<UWorld>(Package, TEXT("TheWorld"));
	UBOOL bSavedCorrectly;
	if (World)
	{	
		bSavedCorrectly = UObject::SavePackage(Package, World, 0, *Filename, ErrorDevice, LinkerToConformAgainst);
	}
	else
	{
		bSavedCorrectly = UObject::SavePackage(Package, NULL, KeepObjectFlags, *Filename, ErrorDevice, LinkerToConformAgainst);
	}

	// return success
	return bSavedCorrectly;
}

/**
 * Policy that marks Asset Sets via GAD Collection
 */
class FCollectionPolicy
{
public:
#if WITH_MANAGED_CODE
	static UBOOL CreateAssetSet( const FString& InSetName, EGADCollection::Type InType )
	{
		return FGameAssetDatabase::Get().CreateCollection(InSetName, InType);
	}

	static UBOOL DestroyAssetSet(const FString& InSetName, EGADCollection::Type InSetType )
	{
		return FGameAssetDatabase::Get().DestroyCollection(InSetName, InSetType);
	}

	static UBOOL RemoveAssetsFromSet( const FString& InSetName, EGADCollection::Type InSetType, const TArray< FString >& InAssetFullNames )
	{
		return FGameAssetDatabase::Get().RemoveAssetsFromCollection(InSetName, InSetType, InAssetFullNames);
	}

	static UBOOL AddAssetsToSet( const FString& InSetName, EGADCollection::Type InSetType, const TArray< FString >& InAssetFullNames )
	{
		return FGameAssetDatabase::Get().AddAssetsToCollection(InSetName, InSetType, InAssetFullNames);
	}

	static void QueryAssetsInSet( const FString& InSetName, EGADCollection::Type InSetType, TArray< FString >& OutAssetFullNames )
	{
		FGameAssetDatabase::Get().QueryAssetsInCollection(InSetName, InSetType, OutAssetFullNames);
	}
#endif //WITH_MANAGED_CODE 

};

/**
 * Policy that marks Asset Sets via GAD Tags
 */
class FTagPolicy
{
public:
#if WITH_MANAGED_CODE
	static UBOOL CreateAssetSet( const FString& InSetName, EGADCollection::Type InType )
	{
		return FGameAssetDatabase::Get().CreateTag(InSetName);
	}

	static UBOOL DestroyAssetSet(const FString& InSetName, EGADCollection::Type InSetType )
	{
		return FGameAssetDatabase::Get().DestroyTag(InSetName);
	}

	static UBOOL RemoveAssetsFromSet( const FString& InSetName, EGADCollection::Type InSetType, const TArray< FString >& InAssetFullNames )
	{
		return FGameAssetDatabase::Get().RemoveTagFromAssets(InAssetFullNames, InSetName);
	}

	static UBOOL AddAssetsToSet( const FString& InSetName, EGADCollection::Type InSetType, const TArray< FString >& InAssetFullNames )
	{
		return FGameAssetDatabase::Get().AddTagToAssets(InAssetFullNames, InSetName);
	}

	static void QueryAssetsInSet( const FString& InSetName, EGADCollection::Type InSetType, TArray< FString >& OutAssetFullNames )
	{
		FGameAssetDatabase::Get().QueryAssetsWithTag(InSetName, OutAssetFullNames);
	}
#endif //WITH_MANAGED_CODE
};


/**
 *	Create a new tag
 *	
 *	@param	InTagName		The name of the tag to create
 *
 *	@return	UBOOL			TRUE if successful, FALSE if failed
 */
UBOOL FGADHelper::CreateTag( const FString& InTagName )
{
#if WITH_MANAGED_CODE
	return FGameAssetDatabase::Get().CreateTag( InTagName );
#else
	return FALSE;
#endif //WITH_MANAGED_CODE
}



/** Clears the content of a Tag or Collection */
template <class AssetSetPolicy>	
UBOOL FGADHelper::ClearAssetSet( const FString& InSetName, const INT InSetType )
{
#if WITH_MANAGED_CODE
	if (bInitialized == FALSE)
	{
		warnf(NAME_Warning, *LocalizeUnrealEd("CollectionHelper_NotInitialized"));
		return FALSE;
	}

	const EGADCollection::Type CollectionType = EGADCollection::Type(InSetType);
	if ( AssetSetPolicy::DestroyAssetSet( InSetName, CollectionType ) == FALSE)
	{
		warnf(NAME_Warning, 
			*FString::Printf(LocalizeSecure(LocalizeUnrealEd("CollectionHelper_ClearCollectionFailed"), *InSetName)));
		return FALSE;
	}
#else
	warnf( NAME_Warning, TEXT( "GameAssetDatabase not available unless WITH_MANAGED_CODE is defined" ) );
#endif
	return TRUE;
}


/** Sets the contents of a Tag or Collection to be the InAssetList. Assets not mentioned in the list will be untagged. */
template <class AssetSetPolicy>	
UBOOL FGADHelper::AssignSetContent( const FString& InSetName, const INT InType, const TArray<FString>& InAssetList )
{
	UBOOL bResult = TRUE;

#if WITH_MANAGED_CODE
	if (bInitialized == FALSE)
	{
		warnf(NAME_Warning, *LocalizeUnrealEd("CollectionHelper_NotInitialized"));
		return FALSE;
	}

	const EGADCollection::Type CollectionType = EGADCollection::Type(InType);
	// We ALWAYS want to create the collection. 
	// Even when there is nothing to add, it will indicate the operation was a success. 
	// For example, if a commandlet is run and a collection isn't generated, it would
	// not be clear whether the commandlet actually completed successfully.
	if (AssetSetPolicy::CreateAssetSet(InSetName, CollectionType) == TRUE)
	{
		// If there is nothing to update, we are done.
		if (InAssetList.Num() >= 0)
		{
			UBOOL bAddCompleteInAssetList = TRUE;

			TArray<FString> AssetsInCollection;
			AssetSetPolicy::QueryAssetsInSet(InSetName, CollectionType, AssetsInCollection);
			INT CurrentAssetCount = AssetsInCollection.Num();
			if (CurrentAssetCount != 0)
			{
				// Generate the lists
				TArray<FString> TrueAddList;
				TArray<FString> TrueRemoveList;

				// See how many items are really being added/removed
				for (INT CheckIdx = 0; CheckIdx < AssetsInCollection.Num(); CheckIdx++)
				{
					FString CheckAsset = AssetsInCollection(CheckIdx);
					if (InAssetList.FindItemIndex(CheckAsset) != INDEX_NONE)
					{
						TrueAddList.AddUniqueItem(CheckAsset);
					}
					else
					{
						TrueRemoveList.AddUniqueItem(CheckAsset);
					}
				}

				if ((TrueRemoveList.Num() + TrueAddList.Num()) < CurrentAssetCount)
				{
					// Remove and add only the required assets.
					bAddCompleteInAssetList = FALSE;
					if (TrueRemoveList.Num() > 0)
					{
						if (AssetSetPolicy::RemoveAssetsFromSet(InSetName, CollectionType, TrueRemoveList) == FALSE)
						{
							warnf(NAME_Warning, 
								*FString::Printf(LocalizeSecure(LocalizeUnrealEd("CollectionHelper_RemoveAssetsFailed"), *InSetName)));
							bResult = FALSE;
						}
					}
					if (TrueAddList.Num() > 0)
					{
						if (AssetSetPolicy::AddAssetsToSet(InSetName, CollectionType, TrueAddList) == FALSE)
						{
							warnf(NAME_Warning, 
								*FString::Printf(LocalizeSecure(LocalizeUnrealEd("CollectionHelper_AddAssetsFailed"), *InSetName)));
							bResult = FALSE;
						}
					}
				}
				else
				{
					// Clear the collection and fall into the add all case
					bAddCompleteInAssetList = ClearAssetSet<AssetSetPolicy>(InSetName, InType);
					if (bAddCompleteInAssetList == FALSE)
					{
						// this is a problem!!!
						warnf(NAME_Warning, 
							*FString::Printf(LocalizeSecure(LocalizeUnrealEd("CollectionHelper_SetCollectionClearFailed"), *InSetName)));
						bResult = FALSE;
					}
				}
			}

			if (bAddCompleteInAssetList == TRUE)
			{
				// Just add 'em all...
				if (AssetSetPolicy::AddAssetsToSet(InSetName, CollectionType, InAssetList) == FALSE)
				{
					warnf(NAME_Warning, 
						*FString::Printf(LocalizeSecure(LocalizeUnrealEd("CollectionHelper_AddAssetsFailed"), *InSetName)));
					bResult = FALSE;
				}
			}
		}
	}
	else
	{
		warnf(NAME_Warning, 
			*FString::Printf(LocalizeSecure(LocalizeUnrealEd("CollectionHelper_CreateCollectionFailed"), *InSetName)));
		bResult = FALSE;
	}
#else
	bResult = TRUE;
	debugf(TEXT("Setting collection %s"), *InSetName);
	for (INT AssetIdx = 0; AssetIdx < InAssetList.Num(); AssetIdx++)
	{
		debugf(TEXT("\t\t%s"), *(InAssetList(AssetIdx)));
	}
#endif
	return bResult;
}

/** Add and remove assets for the specified Tag or Connection. Assets from InAddList are added; assets from InRemoveList are removed. */
template <class AssetSetPolicy>	
UBOOL FGADHelper::UpdateSetContent( const FString& InSetName, const INT InType, const TArray<FString>& InAddList, const TArray<FString>& InRemoveList )
{
	UBOOL bResult = TRUE;

#if WITH_MANAGED_CODE
	if (bInitialized == FALSE)
	{
		warnf(NAME_Warning, *LocalizeUnrealEd("CollectionHelper_NotInitialized"));
		return FALSE;
	}

	const EGADCollection::Type CollectionType = EGADCollection::Type(InType);
	// We ALWAYS want to create the collection. 
	// Even when there is nothing to add, it will indicate the operation was a success. 
	// For example, if a commandlet is run and a collection isn't generated, it would
	// not be clear whether the commandlet actually completed successfully.
	if (AssetSetPolicy::CreateAssetSet(InSetName, CollectionType) == TRUE)
	{
		// If there is nothing to update, we are done.
		if ((InAddList.Num() >= 0) || (InRemoveList.Num() >= 0))
		{
			TArray<FString> AssetsInCollection;
			AssetSetPolicy::QueryAssetsInSet(InSetName, CollectionType, AssetsInCollection);
			if (AssetsInCollection.Num() != 0)
			{
				// Clean up the lists
				TArray<FString> TrueAddList;
				TArray<FString> TrueRemoveList;

				// Generate the true Remove list, only removing items that are actually in the collection.
				for (INT RemoveIdx = 0; RemoveIdx < InRemoveList.Num(); RemoveIdx++)
				{
					if (AssetsInCollection.ContainsItem(InRemoveList(RemoveIdx)) == TRUE)
					{
						TrueRemoveList.AddUniqueItem(InRemoveList(RemoveIdx));
					}
				}

				if (TrueRemoveList.Num() > 0)
				{
					if (AssetSetPolicy::RemoveAssetsFromSet(InSetName, CollectionType, TrueRemoveList) == FALSE)
					{
						warnf(NAME_Warning, 
							*FString::Printf(LocalizeSecure(LocalizeUnrealEd("CollectionHelper_RemoveAssetsFailed"), *InSetName)));
						bResult = FALSE;
					}
				}

				// Generate the true Add list, only adding items that are not already in the collection.
				for (INT AddIdx = 0; AddIdx < InAddList.Num(); AddIdx++)
				{
					if (AssetsInCollection.ContainsItem(InAddList(AddIdx)) == FALSE)
					{
						TrueAddList.AddUniqueItem(InAddList(AddIdx));
					}
				}

				if (TrueAddList.Num() > 0)
				{
					if (AssetSetPolicy::AddAssetsToSet(InSetName, CollectionType, TrueAddList) == FALSE)
					{
						warnf(NAME_Warning, 
							*FString::Printf(LocalizeSecure(LocalizeUnrealEd("CollectionHelper_AddAssetsFailed"), *InSetName)));
						bResult = FALSE;
					}
				}
			}
			else
			{
				// Just add 'em all...
				if (AssetSetPolicy::AddAssetsToSet(InSetName, CollectionType, InAddList) == FALSE)
				{
					warnf(NAME_Warning, 
						*FString::Printf(LocalizeSecure(LocalizeUnrealEd("CollectionHelper_AddAssetsFailed"), *InSetName)));
					bResult = FALSE;
				}
			}
		}
	}
	else
	{
		warnf(NAME_Warning, 
			*FString::Printf(LocalizeSecure(LocalizeUnrealEd("CollectionHelper_CreateCollectionFailed"), *InSetName)));
		bResult = FALSE;
	}
#else
	bResult = TRUE;
	debugf(TEXT("Updating collection %s"), *InSetName);
	debugf(TEXT("\tAdding"));
	for (INT AddIdx = 0; AddIdx < InAddList.Num(); AddIdx++)
	{
		debugf(TEXT("\t\t%s"), *(InAddList(AddIdx)));
	}
	debugf(TEXT("\tRemoving"));
	for (INT RemoveIdx = 0; RemoveIdx < InRemoveList.Num(); RemoveIdx++)
	{
		debugf(TEXT("\t\t%s"), *(InRemoveList(RemoveIdx)));
	}
#endif
	return bResult;
}

/** Get the list of all assets in the specified Collection or Tag */
template <class AssetSetPolicy>	
UBOOL FGADHelper::QuerySetContent( const FString& InSetName, const INT InType, TArray<FString>& OutAssetFullNames )
{
	UBOOL bResult = TRUE;

#if WITH_MANAGED_CODE
	if (bInitialized == FALSE)
	{
		warnf(NAME_Warning, *LocalizeUnrealEd("CollectionHelper_NotInitialized"));
		return FALSE;
	}

	const EGADCollection::Type CollectionType = EGADCollection::Type(InType);
	// This will never fail w/ an initialized GAD
	AssetSetPolicy::QueryAssetsInSet(InSetName, CollectionType, OutAssetFullNames);
#else
#endif
	return bResult;
}


/**
 *	Initialize the Collection helper
 *	
 *	@return	UBOOL					TRUE if successful, FALSE if failed
 */
UBOOL FGADHelper::Initialize()
{
	// Startup the game asset database so we'll have access to the tags and collections
#if WITH_MANAGED_CODE
	bWeInitializedGAD = FALSE;
	if( !FGameAssetDatabase::IsInitialized() )
	{
		FGameAssetDatabaseStartupConfig StartupConfig;
		FString InitErrorMessageText;	// This is an output from the Init call.
		FGameAssetDatabase::Init(StartupConfig, InitErrorMessageText);
		if (InitErrorMessageText.Len() > 0)
		{
			// Does this inidicate a failure??
			warnf(NAME_Warning, TEXT("FGADHelper: GameAssetDatabase: %s" ), *InitErrorMessageText);
		}

		// Note that we initialized the GAD here so we can shut it down later on
		if( FGameAssetDatabase::IsInitialized() )
		{
			bWeInitializedGAD = TRUE;
		}
	}

	bInitialized = FGameAssetDatabase::IsInitialized();
#else
	warnf( NAME_Warning, TEXT( "GameAssetDatabase not available unless WITH_MANAGED_CODE is defined" ) );
#endif
	return bInitialized;
}

/**
 *	Shutdown the collection helper
 */
void FGADHelper::Shutdown()
{
	if( bInitialized == TRUE && bWeInitializedGAD )
	{
#if WITH_MANAGED_CODE
		// Shutdown the game asset database
		FGameAssetDatabase::Destroy();
#endif
	}
	bInitialized = FALSE;
	bWeInitializedGAD = FALSE;
}

/**
 *	Clear the given collection
 *	
 *	@param	InCollectionName	The name of the collection to create
 *	@param	InType				Type of collection
 *
 *	@return	UBOOL				TRUE if successful, FALSE if failed
 */
UBOOL FGADHelper::ClearCollection(const FString& InCollectionName, const INT InType)
{
	return this->ClearAssetSet<FCollectionPolicy>( InCollectionName, InType );
}


/**
 *  Clear this tag from all assets (and delete it)
 *
 *  @param  InTagName  The name of the collection to create
 *
 *  @return UBOOL      TRUE if successful, FALSE if failed
 */
UBOOL FGADHelper::ClearTagFromAssets( const FString& InTagName )
{
	return this->ClearAssetSet<FTagPolicy>( InTagName, 0 );
}

/**
 *	Fill the given collection with the given list of assets
 *
 *	@param	InCollectionName	The name of the collection to fill
 *	@param	InType				Type of collection
 *	@param	InAssetList			The list of items to fill the collection with (can be empty)
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not.
 */
UBOOL FGADHelper::SetCollection(const FString& InCollectionName, const INT InType, const TArray<FString>& InAssetList)
{
	return this->AssignSetContent<FCollectionPolicy>(InCollectionName, InType, InAssetList);
}

/**
 *	Assign this tag to the given set of assets; remove it from all other assets.
 *
 *	@param  InTagName    The name of the collection to fill
 *	@param  InAssetList  The list of items that should have this tag
 *
 *	@return UBOOL        TRUE if successful, FALSE if not.
 */
UBOOL FGADHelper::SetTaggedAssets( const FString& InTagName, const TArray<FString>& InAssetList )
{
	TArray<FString> TempStrings;
	// Make sure the tag doesn't have spaces
	if(InTagName.ParseIntoArray(&TempStrings, TEXT(" "), TRUE) != 1)
	{
		warnf(TEXT("Cannot have spaces in Tag Name"));
		return FALSE;
	}
	return this->AssignSetContent<FTagPolicy>( InTagName, 0, InAssetList );
}

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
UBOOL FGADHelper::UpdateCollection(const FString& InCollectionName, const INT InType, const TArray<FString>& InAddList, const TArray<FString>& InRemoveList)
{
	return this->UpdateSetContent<FCollectionPolicy>( InCollectionName, InType, InAddList, InRemoveList );
}

/**
 *	Update the given tag with the lists of adds/removes
 *
 *	@param InTagName     The name of the collection to update
 * 	@param InAddList     List of assets to be tagged with given tag
 *	@param InRemoveList  List of assets from which to remove this tag
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not.
 */
UBOOL FGADHelper::UpdateTaggedAssets( const FString& InTagName, const TArray<FString>& InAddList, const TArray<FString>& InRemoveList )
{
	return this->UpdateSetContent<FTagPolicy>( InTagName, 0, InAddList, InRemoveList );
}

/**
 *	Retrieve the assets contained in the given collection.
 *
 *	@param	InCollectionName	Name of collection to query
 *	@param	InType				Type of collection
 *	@param	OutAssetFullName	The assets contained in the collection
 * 
 *	@return True if collection was created successfully
 */
UBOOL FGADHelper::QueryAssetsInCollection(const FString& InCollectionName, const INT InType, TArray<FString>& OutAssetFullNames)
{
	return this->QuerySetContent<FCollectionPolicy>(InCollectionName, InType, OutAssetFullNames);
}

/**
 * Retrieve the assets tagged with the specified tag name.
 * 
 * @param InTagName          Name of tag to look up on assets.
 * @param OutAssetFullNames  Names of assets tagged with InTagName
 *
 * @return TRUE if successfully retrieved assets.
 */
UBOOL FGADHelper::QueryTaggedAssets( const FString& InTagName, TArray<FString>& OutAssetFullNames )
{
	return this->QuerySetContent<FTagPolicy>(InTagName, 0, OutAssetFullNames);
}


/**
 * Finds all of the assets with all of the specified tags
 *
 * @param	InTags				List of tags.  For an asset to be returned, it must be associated with all of these tags.
 * @param	OutAssetFullNames	[Out] List of assets found
 */
void FGADHelper::QueryAssetsWithAllTags( const TArray< FString >& InTags, TArray< FString >& OutAssetFullNames )
{
#if WITH_MANAGED_CODE
	FGameAssetDatabase::Get().QueryAssetsWithAllTags(InTags, OutAssetFullNames);
#endif //WITH_MANAGED_CODE
}

/*-----------------------------------------------------------------------------
ULoadPackageCommandlet
-----------------------------------------------------------------------------*/
/**
 *	Parse the given load list file, placing the entries in the given Tokens array.
 *
 *	@param	LoadListFilename	The name of the load list file
 *	@param	Tokens				The array to place the entries into.
 *	
 *	@return	UBOOL				TRUE if successful and non-empty, FALSE otherwise
 */
UBOOL ULoadPackageCommandlet::ParseLoadListFile(FString& LoadListFilename, TArray<FString>& Tokens)
{
	//Open file
	FString Data;
	if (appLoadFileToString(Data, *LoadListFilename) == TRUE)
	{
		const TCHAR* Ptr = *Data;
		FString StrLine;

		while (ParseLine(&Ptr, StrLine))
		{
			debugfSlow(TEXT("Read in: %s"), *StrLine);
			Tokens.AddUniqueItem(StrLine);
		}

		// debugging...
		debugfSlow(TEXT("\nPACKAGES TO LOAD:"));
		for (INT TokenIdx = 0; TokenIdx < Tokens.Num(); TokenIdx++)
		{
			debugfSlow(TEXT("\t%s"), *(Tokens(TokenIdx)));
		}
		return (Tokens.Num() > 0);
	}

	return FALSE;
}

/**
* If you pass in -ALL this will recursively load all of the packages from the
* directories listed in the .ini path entries
**/

INT ULoadPackageCommandlet::Main( const FString& Params )
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	UBOOL bLoadAllPackages = Switches.ContainsItem(TEXT("ALL"));

	// Check for a load list file...
	for (INT TokenIdx = 0; TokenIdx < Tokens.Num(); TokenIdx++)
	{
		FString LoadListFilename = TEXT("");
		if (Parse(*(Tokens(TokenIdx)), TEXT("LOADLIST="), LoadListFilename))
		{
			// Found one - this will be a list of packages to load
			debugfSlow(TEXT("LoadList in file %s"), *LoadListFilename);

			TArray<FString> TempTokens;
			if (ParseLoadListFile(LoadListFilename, TempTokens) == TRUE)
			{
				bLoadAllPackages = FALSE;

				Tokens.Empty(TempTokens.Num());
				Tokens = TempTokens;
			}
		}
	}

	TArray<FFilename> FilesInPath;
	if ( bLoadAllPackages )
	{
		TArray<FString> PackageExtensions;
		GConfig->GetArray( TEXT("Core.System"), TEXT("Extensions"), PackageExtensions, GEngineIni );

		Tokens.Empty(PackageExtensions.Num());
		for ( INT ExtensionIndex = 0; ExtensionIndex < PackageExtensions.Num(); ExtensionIndex++ )
		{
			Tokens.AddItem(FString(TEXT("*.")) + PackageExtensions(ExtensionIndex));
		}
	}

	if ( Tokens.Num() == 0 )
	{
		warnf(TEXT("You must specify a package name (multiple files can be delimited by spaces) or wild-card, or specify -all to include all registered packages"));
		return 1;
	}

	BYTE PackageFilter = NORMALIZE_DefaultFlags;
	if ( Switches.ContainsItem(TEXT("MAPSONLY")) )
	{
		PackageFilter |= NORMALIZE_ExcludeContentPackages;
	}

	// assume the first token is the map wildcard/pathname
	TArray<FString> Unused;
	for ( INT TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++ )
	{
		TArray<FFilename> TokenFiles;
		if ( !NormalizePackageNames( Unused, TokenFiles, Tokens(TokenIndex), PackageFilter) )
		{
			debugf(TEXT("No packages found for parameter %i: '%s'"), TokenIndex, *Tokens(TokenIndex));
			continue;
		}

		FilesInPath += TokenFiles;
	}

	if ( FilesInPath.Num() == 0 )
	{
		warnf(TEXT("No files found."));
		return 1;
	}

	GIsClient = !Switches.ContainsItem(TEXT("NOCLIENT"));
	GIsServer = !Switches.ContainsItem(TEXT("NOSERVER"));
	GIsEditor = !Switches.ContainsItem(TEXT("NOEDITOR"));

	for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		const FFilename& Filename = FilesInPath(FileIndex);

		// we don't care about trying to load the various shader caches so just skipz0r them
		if(	Filename.InStr( TEXT("LocalShaderCache") ) != INDEX_NONE
		|| Filename.InStr( TEXT("RefShaderCache") ) != INDEX_NONE )
		{
			continue;
		}

		warnf( NAME_Log, TEXT("Loading %s"), *Filename );

		const FString& PackageName = FPackageFileCache::PackageFromPath(*Filename);
		UPackage* Package = FindObject<UPackage>(NULL, *PackageName, TRUE);
		if ( Package != NULL && !bLoadAllPackages )
		{
			ResetLoaders(Package);
		}

		Package = UObject::LoadPackage( NULL, *Filename, LOAD_None );
		if( Package == NULL )
		{
			warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
		}

		if ( GIsEditor )
		{
			SaveLocalShaderCaches();
		}

		UObject::CollectGarbage( RF_Native );
	}
	GIsEditor = GIsServer = GIsClient = TRUE;

	return 0;
}
IMPLEMENT_CLASS(ULoadPackageCommandlet)


/*-----------------------------------------------------------------------------
UShowObjectCountCommandlet.
-----------------------------------------------------------------------------*/

void UShowObjectCountCommandlet::StaticInitialize()
{
}


IMPLEMENT_COMPARE_CONSTREF( FPackageObjectCount, UnPackageUtilities, { INT result = appStricmp(*A.ClassName, *B.ClassName); if ( result == 0 ) { result = B.Count - A.Count; } return result; } )


FObjectCountExecutionParms::FObjectCountExecutionParms( const TArray<UClass*>& InClasses, EObjectFlags InMask/*=RF_LoadForClient|RF_LoadForServer|RF_LoadForEdit*/ )
: SearchClasses(InClasses), ObjectMask(InMask), bShowObjectNames(FALSE), bIncludeCookedPackages(FALSE)
, bCookedPackagesOnly(FALSE), bIgnoreChildren(FALSE), bIgnoreCheckedOutPackages(FALSE)
, bIgnoreScriptPackages(FALSE), bIgnoreMapPackages(FALSE), bIgnoreContentPackages(FALSE)
{
}


/**
 * Searches all packages for the objects which meet the criteria specified.
 *
 * @param	Parms	specifies the parameters to use for the search
 * @param	Results	receives the results of the search
 * @param	bUnsortedResults	by default, the list of results will be sorted according to the number of objects in each package;
 *								specify TRUE to override this behavior.
 */
void UShowObjectCountCommandlet::ProcessPackages( const FObjectCountExecutionParms& Parms, TArray<FPackageObjectCount>& Results, UBOOL bUnsortedResults/*=FALSE*/ )
{
	TArray<FString> PackageFiles;

	if ( !Parms.bCookedPackagesOnly )
	{
		PackageFiles = GPackageFileCache->GetPackageFileList();
	}

	if ( Parms.bCookedPackagesOnly || Parms.bIncludeCookedPackages )
	{
		const INT StartIndex = PackageFiles.Num();
		const FString CookedPackageDirectory = appGameDir() + TEXT("CookedXenon");
		const FString CookedPackageSearchString = CookedPackageDirectory * TEXT("*.xxx");
		GFileManager->FindFiles(PackageFiles, *CookedPackageSearchString, TRUE, FALSE);

		// re-add the path information so that GetPackageLinker finds the correct version of the file.
		for ( INT FileIndex = StartIndex; FileIndex < PackageFiles.Num(); FileIndex++ )
		{
			PackageFiles(FileIndex) = CookedPackageDirectory * PackageFiles(FileIndex);
		}
	}

	INT GCIndex = 0;
	for( INT FileIndex=0; FileIndex<PackageFiles.Num(); FileIndex++ )
	{
		const FString &Filename = PackageFiles(FileIndex);

		if ( Parms.bIgnoreCheckedOutPackages && !GFileManager->IsReadOnly(*Filename) )
		{
			warnf(NAME_Progress, TEXT("Skipping '%s'..."), *Filename);
			continue;
		}

		UObject::BeginLoad();
		ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *Filename, LOAD_Quiet|LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
		UObject::EndLoad();

		if(	Linker != NULL )
		{
			const UBOOL bScriptPackage	= Linker->LinkerRoot != NULL && (Linker->LinkerRoot->PackageFlags&PKG_ContainsScript) == 0;
			const UBOOL bMapPackage		= Linker->LinkerRoot != NULL && !Linker->LinkerRoot->ContainsMap();
			const UBOOL bContentPackage	= !bScriptPackage && !bMapPackage;

			if ((!Parms.bIgnoreScriptPackages	|| bScriptPackage)
			&&	(!Parms.bIgnoreMapPackages		|| bMapPackage)
			&&	(!Parms.bIgnoreContentPackages	|| bContentPackage) )
			{
				warnf(NAME_Progress, TEXT("Checking '%s'..."), *Filename);

				TArray<INT> ObjectCounts;
				ObjectCounts.AddZeroed(Parms.SearchClasses.Num());

				TArray< TArray<FString> > PackageObjectNames;
				if ( Parms.bShowObjectNames )
				{
					PackageObjectNames.AddZeroed(Parms.SearchClasses.Num());
				}

				UBOOL bContainsObjects=FALSE;
				for ( INT i = 0; i < Linker->ExportMap.Num(); i++ )
				{
					FObjectExport& Export = Linker->ExportMap(i);
					if ( (Export.ObjectFlags&Parms.ObjectMask) != 0 )
					{
						continue;
					}

					FString ClassPathName;


					FName ClassFName = NAME_Class;
					PACKAGE_INDEX ClassPackageIndex = 0;

					// get the path name for this Export's class
					if ( IS_IMPORT_INDEX(Export.ClassIndex) )
					{
						FObjectImport& ClassImport = Linker->ImportMap(-Export.ClassIndex -1);
						ClassFName = ClassImport.ObjectName;
						ClassPackageIndex = ClassImport.OuterIndex;
					}
					else if ( Export.ClassIndex != UCLASS_INDEX )
					{
						FObjectExport& ClassExport = Linker->ExportMap(Export.ClassIndex-1);
						ClassFName = ClassExport.ObjectName;
						ClassPackageIndex = ClassExport.OuterIndex;
					}

					FName OuterName = NAME_Core;
					if ( ClassPackageIndex > 0 )
					{
						FObjectExport& OuterExport = Linker->ExportMap(ClassPackageIndex-1);
						OuterName = OuterExport.ObjectName;
					}
					else if ( ClassPackageIndex < 0 )
					{
						FObjectImport& OuterImport = Linker->ImportMap(-ClassPackageIndex-1);
						OuterName = OuterImport.ObjectName;
					}
					else if ( Export.ClassIndex != UCLASS_INDEX )
					{
						OuterName = Linker->LinkerRoot->GetFName();
					}

					ClassPathName = FString::Printf(TEXT("%s.%s"), *OuterName.ToString(), *ClassFName.ToString());
					UClass* ExportClass = FindObject<UClass>(ANY_PACKAGE, *ClassPathName);
					if ( ExportClass == NULL )
					{
						ExportClass = StaticLoadClass(UObject::StaticClass(), NULL, *ClassPathName, NULL, LOAD_NoVerify|LOAD_NoWarn|LOAD_Quiet, NULL);
					}

					if ( ExportClass == NULL )
					{
						continue;
					}

					FString ObjectName;
					for ( INT ClassIndex = 0; ClassIndex < Parms.SearchClasses.Num(); ClassIndex++ )
					{
						UClass* SearchClass = Parms.SearchClasses(ClassIndex);
						if ( Parms.bIgnoreChildren ? ExportClass == SearchClass : ExportClass->IsChildOf(SearchClass) )
						{
							bContainsObjects = TRUE;
							INT& CurrentObjectCount = ObjectCounts(ClassIndex);
							CurrentObjectCount++;

							if ( Parms.bShowObjectNames )
							{
								TArray<FString>& ClassObjectPaths = PackageObjectNames(ClassIndex);

								if ( ObjectName.Len() == 0 )
								{
									ObjectName = Linker->GetExportFullName(i);
								}
								ClassObjectPaths.AddItem(ObjectName);
							}
						}
					}
				}

				if ( bContainsObjects )
				{
					for ( INT ClassIndex = 0; ClassIndex < ObjectCounts.Num(); ClassIndex++ )
					{
						INT ClassObjectCount = ObjectCounts(ClassIndex);
						if ( ClassObjectCount > 0 )
						{
							FPackageObjectCount* ObjCount = new(Results) FPackageObjectCount(Filename, Parms.SearchClasses(ClassIndex)->GetName(), ClassObjectCount);
							if ( Parms.bShowObjectNames )
							{
								ObjCount->ObjectPathNames = PackageObjectNames(ClassIndex);
							}
						}
					}
				}
			}
			else
			{
				warnf(NAME_Progress, TEXT("Skipping '%s'..."), *Filename);
			}
		}

		// only GC every 10 packages (A LOT faster this way, and is safe, since we are not 
		// acting on objects that would need to go away or anything)
		if ((++GCIndex % 10) == 0)
		{
			UObject::CollectGarbage(RF_Native);
		}
	}

	if ( !bUnsortedResults && Results.Num() > 0 )
	{
		Sort<USE_COMPARE_CONSTREF(FPackageObjectCount,UnPackageUtilities)>( &Results(0), Results.Num() );
	}
}

INT UShowObjectCountCommandlet::Main( const FString& Params )
{
	const TCHAR* Parms = *Params;

	GIsRequestingExit			= 1;	// so CTRL-C will exit immediately
	TArray<FString> Tokens, Switches;
	ParseCommandLine(Parms, Tokens, Switches);

	if ( Tokens.Num() == 0 )
	{
		warnf(TEXT("No class specified!"));
		return 1;
	}

	const UBOOL bIncludeChildren = !Switches.ContainsItem(TEXT("ExactClass"));
	const UBOOL bIncludeCookedPackages = Switches.ContainsItem(TEXT("IncludeCooked"));
	const UBOOL bCookedPackagesOnly = Switches.ContainsItem(TEXT("CookedOnly"));

	// this flag is useful for skipping over old test packages which can cause the commandlet to crash
	const UBOOL bIgnoreCheckedOutPackages = Switches.ContainsItem(TEXT("IgnoreWriteable"));

	const UBOOL bIgnoreScriptPackages = !Switches.ContainsItem(TEXT("IncludeScript"));
	// this flag is useful when you know that the objects you are looking for are not placeable in map packages
	const UBOOL bIgnoreMaps = Switches.ContainsItem(TEXT("IgnoreMaps"));
	const UBOOL bIgnoreContentPackages = Switches.ContainsItem(TEXT("IgnoreContent"));

	const UBOOL bShowObjectNames = Switches.ContainsItem(TEXT("ObjectNames"));
		
	EObjectFlags ObjectMask = 0;
	if ( Switches.ContainsItem(TEXT("SkipClient")) )
	{
		ObjectMask |= RF_LoadForClient;
	}
	if ( Switches.ContainsItem(TEXT("SkipServer")) )
	{
		ObjectMask |= RF_LoadForServer;
	}
	if ( Switches.ContainsItem(TEXT("SkipEditor")) )
	{
		ObjectMask |= RF_LoadForEdit;
	}

	TArray<UClass*> SearchClasses;
	for ( INT TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++ )
	{
		FString& SearchClassName = Tokens(TokenIndex);
		UClass* SearchClass = LoadClass<UObject>(NULL, *SearchClassName, NULL, 0, NULL);
		if ( SearchClass == NULL )
		{
			warnf(TEXT("Failed to load class specified '%s'"), *SearchClassName);
			return 1;
		}

		SearchClasses.AddUniqueItem(SearchClass);

		// make sure it doesn't get garbage collected.
		SearchClass->AddToRoot();
	}

	FObjectCountExecutionParms ProcessParameters( SearchClasses, ObjectMask );
	ProcessParameters.bShowObjectNames			= bShowObjectNames;
	ProcessParameters.bIncludeCookedPackages	= bIncludeCookedPackages;
	ProcessParameters.bCookedPackagesOnly		= bCookedPackagesOnly;
	ProcessParameters.bIgnoreChildren			= !bIncludeChildren;
	ProcessParameters.bIgnoreCheckedOutPackages = bIgnoreCheckedOutPackages;
	ProcessParameters.bIgnoreScriptPackages		= bIgnoreScriptPackages;
	ProcessParameters.bIgnoreMapPackages		= bIgnoreMaps;
	ProcessParameters.bIgnoreContentPackages	= bIgnoreContentPackages;
	TArray<FPackageObjectCount> ClassObjectCounts;

	ProcessPackages(ProcessParameters, ClassObjectCounts);
	if( ClassObjectCounts.Num() )
	{
		INT TotalObjectCount=0;
		INT PerClassObjectCount=0;

		FString LastReportedClass;
		INT IndexPadding=0;
		for ( INT i = 0; i < ClassObjectCounts.Num(); i++ )
		{
			FPackageObjectCount& PackageObjectCount = ClassObjectCounts(i);
			if ( PackageObjectCount.ClassName != LastReportedClass )
			{
				if ( LastReportedClass.Len() > 0 )
				{
					warnf(TEXT("    Total: %i"), PerClassObjectCount);
				}

				PerClassObjectCount = 0;
				LastReportedClass = PackageObjectCount.ClassName;
				warnf(TEXT("\r\nPackages containing objects of class '%s':"), *LastReportedClass);
				IndexPadding = appItoa(PackageObjectCount.Count).Len();
			}

			warnf(TEXT("%s    Count: %*i    Package: %s"), (i > 0 && bShowObjectNames ? LINE_TERMINATOR : TEXT("")), IndexPadding, PackageObjectCount.Count, *PackageObjectCount.PackageName);
			PerClassObjectCount += PackageObjectCount.Count;
			TotalObjectCount += PackageObjectCount.Count;

			if ( bShowObjectNames )
			{
				warnf(TEXT("        Details:"));
				for ( INT NameIndex = 0; NameIndex < PackageObjectCount.ObjectPathNames.Num(); NameIndex++ )
				{
					warnf(TEXT("        %*i) %s"), IndexPadding, NameIndex, *PackageObjectCount.ObjectPathNames(NameIndex));
				}
			}
		}

		warnf(TEXT("    Total: %i"), PerClassObjectCount);
		warnf(TEXT("\r\nTotal number of object instances: %i"), TotalObjectCount);
	}
	return 0;
}

IMPLEMENT_CLASS(UShowObjectCountCommandlet);

/**
 * Initializes the singleton list of object flags.
 */
static TMap<QWORD, FString> GeneratePropertyFlagMap()
{
	TMap<QWORD, FString>	PropertyFlags;

#ifdef	DECLARE_PROPERTY_FLAG
#error DECLARE_PROPERTY_FLAG already defined
#else

#define DECLARE_PROPERTY_FLAG( PropertyFlag ) PropertyFlags.Set(CPF_##PropertyFlag, TEXT(#PropertyFlag));
	DECLARE_PROPERTY_FLAG(Edit)
	DECLARE_PROPERTY_FLAG(Const)
	DECLARE_PROPERTY_FLAG(Input)
	DECLARE_PROPERTY_FLAG(ExportObject)
	DECLARE_PROPERTY_FLAG(OptionalParm)
	DECLARE_PROPERTY_FLAG(Net)
	DECLARE_PROPERTY_FLAG(EditFixedSize)
	DECLARE_PROPERTY_FLAG(Parm)
	DECLARE_PROPERTY_FLAG(OutParm)
	DECLARE_PROPERTY_FLAG(SkipParm)
	DECLARE_PROPERTY_FLAG(ReturnParm)
	DECLARE_PROPERTY_FLAG(CoerceParm)
	DECLARE_PROPERTY_FLAG(Native)
	DECLARE_PROPERTY_FLAG(Transient)
	DECLARE_PROPERTY_FLAG(Config)
	DECLARE_PROPERTY_FLAG(Localized)
	DECLARE_PROPERTY_FLAG(EditConst)
	DECLARE_PROPERTY_FLAG(GlobalConfig)
	DECLARE_PROPERTY_FLAG(Component)
	DECLARE_PROPERTY_FLAG(AlwaysInit)
	DECLARE_PROPERTY_FLAG(DuplicateTransient)
	DECLARE_PROPERTY_FLAG(NoExport)
	DECLARE_PROPERTY_FLAG(NoImport)
	DECLARE_PROPERTY_FLAG(NoClear)
	DECLARE_PROPERTY_FLAG(EditInline)
	DECLARE_PROPERTY_FLAG(EditInlineUse)
	DECLARE_PROPERTY_FLAG(Deprecated)
	DECLARE_PROPERTY_FLAG(DataBinding)
	DECLARE_PROPERTY_FLAG(RepNotify)
	DECLARE_PROPERTY_FLAG(Interp)
	DECLARE_PROPERTY_FLAG(NonTransactional)
	DECLARE_PROPERTY_FLAG(EditorOnly)
	DECLARE_PROPERTY_FLAG(NotForConsole)
	DECLARE_PROPERTY_FLAG(RepRetry)
	DECLARE_PROPERTY_FLAG(ArchetypeProperty)
#undef DECLARE_PROPERTY_FLAG
#endif

	for ( TMap<QWORD,FString>::TIterator It(PropertyFlags); It; ++It )
	{
		const QWORD& Flag = It.Key();
		const FString& String = It.Value();

//  		warnf(TEXT("0x%016I64X: %s"), Flag, *String);
	}
	return PropertyFlags;
}

static FString GetPropertyFlagText( QWORD PropertyFlags )
{
	static TMap<QWORD, FString>	PropertyFlagMap = GeneratePropertyFlagMap();

	FString Result;

	for ( QWORD i = 0; i < 8 * sizeof(QWORD); i++ )
	{
		QWORD CheckFlag = ((QWORD)1) << i;
		if ( ((QWORD)(PropertyFlags & CheckFlag)) != (QWORD)0 )
		{
//  			warnf(TEXT("Found a match!  PropertyFlags(0x%016I64X)  CheckFlag (0x%016I64X) [%i]"), PropertyFlags, CheckFlag, (INT)i);
			FString* pFlagString = PropertyFlagMap.Find(CheckFlag);
			if ( pFlagString != NULL )
			{
				if ( Result.Len() > 0 )
				{
					Result += TEXT(", ");
				}

				Result += *pFlagString;
			}
		}
	}

	if ( Result.Len() == 0 )
	{
		Result = TEXT("None");
	}
	return Result;
}

/*-----------------------------------------------------------------------------
	UShowPropertyFlagsCommandlet.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UShowPropertyFlagsCommandlet);

INT UShowPropertyFlagsCommandlet::Main(const FString& Params)
{
	INT Result = 0;

	const TCHAR* CmdLine = *Params;
	TArray<FString> Tokens, Switches;
	ParseCommandLine(CmdLine, Tokens, Switches);

	FString ClassName, PropertyType, PropertyName;
	UClass* MetaClass = UObject::StaticClass();
	for ( INT TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++ )
	{
		FString& Token = Tokens(TokenIndex);
		if ( Parse(*Token, TEXT("CLASS="), ClassName) )
		{
			Tokens.Remove(TokenIndex--);
			continue;
		}

		if ( Parse(*Token, TEXT("TYPE="), PropertyType) )
		{
			Tokens.Remove(TokenIndex--);
			continue;
		}

		if ( Parse(*Token, TEXT("NAME="), PropertyName) )
		{
			Tokens.Remove(TokenIndex--);
			continue;
		}
//UBOOL ParseObject( const TCHAR* Stream, const TCHAR* Match, T*& Obj, UObject* Outer )
		if ( ParseObject<UClass>(*Token, TEXT("METACLASS="), MetaClass, ANY_PACKAGE) )
		{
			Tokens.Remove(TokenIndex--);
			continue;
		}
	}

	if ( ClassName.Len() > 0 )
	{
		UClass* OwnerClass = LoadClass<UObject>(NULL, *ClassName, NULL, LOAD_None, NULL);
		if ( OwnerClass != NULL )
		{
			if ( PropertyName.Len() > 0 )
			{
				UProperty* Property = FindField<UProperty>(OwnerClass, *PropertyName);
				if ( Property != NULL )
				{
					warnf(TEXT("Property flags for '%s': %s"), *Property->GetFullName(), *GetPropertyFlagText(Property->PropertyFlags));
				}
				else
				{
					warnf(NAME_Error, TEXT("Failed to find property with specified name '%s'"), *PropertyName);
					Result = 1;
				}
			}
			else
			{
				warnf(TEXT("Displaying property flags for all properties of class %s"), *OwnerClass->GetPathName());

				UClass* PropertyMetaClass = UProperty::StaticClass();
				if ( PropertyType.Len() > 0 )
				{
					PropertyMetaClass = LoadClass<UProperty>(NULL, *PropertyType, NULL, LOAD_None, NULL);
					if ( PropertyMetaClass == NULL )
					{
						warnf(NAME_Error, TEXT("Invalid property type specified: '%s'"), *PropertyType);
						Result = 1;
					}
				}

				if ( PropertyMetaClass != NULL )
				{
					UBOOL bFoundProperty = FALSE;
					INT NameLength = 0;

					TArray<UProperty*> ValidProperties;
					for ( TFieldIterator<UProperty> It(OwnerClass); It; ++It )
					{
						UProperty* Prop = *It;
						if ( Prop->IsA(PropertyMetaClass) && Prop->GetOwnerClass()->IsChildOf(MetaClass) )
						{
							ValidProperties.AddItem(Prop);
							NameLength = Max(NameLength, Prop->GetFullName(Prop->GetOutermost()).Len());
							bFoundProperty = TRUE;
						}
					}

					if ( bFoundProperty )
					{
						for ( INT PropIndex = 0; PropIndex < ValidProperties.Num(); PropIndex++ )
						{
							UProperty* Prop = ValidProperties(PropIndex);
							warnf(TEXT("%*s: %s"), NameLength, *Prop->GetFullName(Prop->GetOutermost()), *GetPropertyFlagText(Prop->PropertyFlags));
						}
					}
					else
					{
						warnf(TEXT("No properties of type '%s' found in class %s"), *PropertyMetaClass->GetName(), *OwnerClass->GetPathName());
					}
				}
			}
		}
		else
		{
			warnf(NAME_Error, TEXT("Failed to load specified class '%s'!"), *ClassName);
			Result = 1;
		}
	}
	else
	{
		warnf(NAME_Error, TEXT("You must specify a class to display property flags for!  syntax: game.exe run ShowPropertyFlags CLASS=SomePackage.SomeClass"));
		Result = 1;
	}

	return Result;
}

/*-----------------------------------------------------------------------------
UShowTaggedPropsCommandlet.
-----------------------------------------------------------------------------*/

INT UShowTaggedPropsCommandlet::Main(const FString& Params)
{
	const TCHAR* CmdLine = appCmdLine();

	TArray<FString> Tokens, Switches;
	ParseCommandLine(CmdLine, Tokens, Switches);

	FString	ClassName, PackageParm, PropertyFilter, IgnorePropertyFilter;

	for ( INT Idx = 0; Idx < Tokens.Num(); Idx++ )
	{
		FString& Token = Tokens(Idx);
		if ( Parse(*Token, TEXT("IGNOREPROPS="), IgnorePropertyFilter) )
		{
			Tokens.Remove(Idx);
			break;
		}
	}

	if ( Tokens.Num() >  0 )
	{
		PackageParm = Tokens(0);
		if ( Tokens.Num() > 1 )
		{
			ClassName = Tokens(1);
			if ( Tokens.Num() > 2 )
			{
				PropertyFilter = Tokens(2);
			}
		}
	}
	else
	{
		PackageParm = TEXT("*.upk");
	}

	DWORD FilterFlags = 0;
	if ( Switches.ContainsItem(TEXT("COOKED")) )
	{
		FilterFlags |= NORMALIZE_IncludeCookedPackages;
	}

	TArray<FString> PackageNames;
	TArray<FFilename> PackageFilenames;
	if ( !NormalizePackageNames(PackageNames, PackageFilenames, PackageParm, FilterFlags) )
	{
		return 0;
	}

	for ( INT FileIndex = 0; FileIndex < PackageFilenames.Num(); FileIndex++ )
	{
		// reset the loaders for the packages we want to load so that we don't find the wrong version of the file
		// (otherwise, attempting to run pkginfo on e.g. Engine.xxx will always return results for Engine.u instead)
		const FString& PackageName = FPackageFileCache::PackageFromPath(*PackageFilenames(FileIndex));
		UPackage* ExistingPackage = FindObject<UPackage>(NULL, *PackageName, TRUE);
		if ( ExistingPackage != NULL )
		{
			ResetLoaders(ExistingPackage);
		}

		warnf(TEXT("Loading '%s'..."), *PackageName);
		UObject* Pkg = LoadPackage(NULL, *PackageName, LOAD_None);
		UClass* SearchClass = StaticLoadClass(UObject::StaticClass(), NULL, *ClassName, NULL, LOAD_None, NULL);
		if ( SearchClass == NULL && ClassName.Len() > 0 )
		{
			warnf(NAME_Error, TEXT("Failed to load class '%s'"), *ClassName);
			return 1;
		}

		if ( PropertyFilter.Len() > 0 )
		{
			TArray<FString> PropertyNames;
			PropertyFilter.ParseIntoArray(&PropertyNames, TEXT(","), TRUE);

			for ( INT PropertyIndex = 0; PropertyIndex < PropertyNames.Num(); PropertyIndex++ )
			{
				UProperty* Property = FindField<UProperty>(SearchClass, FName(*PropertyNames(PropertyIndex)));
				if ( Property != NULL )
				{
					SearchProperties.AddItem(Property);
				}
			}
		}

		if ( IgnorePropertyFilter.Len() > 0 )
		{
			TArray<FString> PropertyNames;
			IgnorePropertyFilter.ParseIntoArray(&PropertyNames, TEXT(","), TRUE);

			for ( INT PropertyIndex = 0; PropertyIndex < PropertyNames.Num(); PropertyIndex++ )
			{
				UProperty* Property = FindField<UProperty>(SearchClass, FName(*PropertyNames(PropertyIndex)));
				if ( Property != NULL )
				{
					IgnoreProperties.AddItem(Property);
				}
			}
		}

		// this is needed in case we end up serializing a script reference which results in VerifyImport being called
		BeginLoad();
		for ( FObjectIterator It; It; ++It )
		{
			UObject* Obj = *It;
			if ( Obj->IsA(SearchClass) && Obj->IsIn(Pkg) )
			{
				ShowSavedProperties(Obj);
			}
		}
		EndLoad();
	}

	return 0;
}

void UShowTaggedPropsCommandlet::ShowSavedProperties( UObject* Object ) const
{
	check(Object);

	warnf(TEXT("Showing property data for %s"), *Object->GetFullName());
	ULinkerLoad& Ar = *Object->GetLinker();
	INT LinkerIndex = Object->GetLinkerIndex();
	checkf(LinkerIndex != INDEX_NONE,TEXT("Invalid linker index for '%s'"), *Object->GetFullName());

	const UBOOL bIsArchetypeObject = Object->IsTemplate();
	if ( bIsArchetypeObject == TRUE )
	{
		Ar.StartSerializingDefaults();
	}

	FName PropertyName(NAME_None);
	FObjectExport& Export = Ar.ExportMap(LinkerIndex);
	Ar.Loader->Seek(Export.SerialOffset);
	Ar.Loader->Precache(Export.SerialOffset,Export.SerialSize);

	if( Object->HasAnyFlags(RF_HasStack) )
	{
		FStateFrame* DummyStateFrame = new FStateFrame(Object);

		Ar << DummyStateFrame->Node << DummyStateFrame->StateNode;
		Ar << DummyStateFrame->ProbeMask;
		Ar << DummyStateFrame->LatentAction;
		Ar << DummyStateFrame->StateStack;
		if( DummyStateFrame->Node )
		{
			Ar.Preload( DummyStateFrame->Node );
			INT Offset = DummyStateFrame->Code ? DummyStateFrame->Code - &DummyStateFrame->Node->Script(0) : INDEX_NONE;
			Ar << Offset;
			if( Offset!=INDEX_NONE )
			{
				if( Offset<0 || Offset>=DummyStateFrame->Node->Script.Num() )
				{
					appErrorf( TEXT("%s: Offset mismatch: %i %i"), *GetFullName(), Offset, DummyStateFrame->Node->Script.Num() );
				}
			}
			DummyStateFrame->Code = Offset!=INDEX_NONE ? &DummyStateFrame->Node->Script(Offset) : NULL;
		}
		else 
		{
			DummyStateFrame->Code = NULL;
		}

		delete DummyStateFrame;
	}

	if ( Object->IsA(UComponent::StaticClass()) && !Object->HasAnyFlags(RF_ClassDefaultObject) )
	{
		((UComponent*)Object)->PreSerialize(Ar);
	}

	Object->SerializeNetIndex(Ar);

	INT BufferSize = 256 * 256;
	BYTE* Data = (BYTE*)appMalloc(BufferSize);

	// we need to keep a pointer to the original location of the memory we allocated in case one of the functions we call moves the Data pointer
	BYTE* StartingData = Data;

	// Load tagged properties.
	UClass* ObjClass = Object->GetClass();

	// This code assumes that properties are loaded in the same order they are saved in. This removes a n^2 search 
	// and makes it an O(n) when properties are saved in the same order as they are loaded (default case). In the 
	// case that a property was reordered the code falls back to a slower search.
	UProperty*	Property			= ObjClass->PropertyLink;
	UBOOL		AdvanceProperty		= 0;
	INT			RemainingArrayDim	= Property ? Property->ArrayDim : 0;

	UBOOL bDisplayedObjectName = FALSE;

	// Load all stored properties, potentially skipping unknown ones.
	while( 1 )
	{
		FPropertyTag Tag;
		Ar << Tag;
		if( Tag.Name == NAME_None )
			break;
		PropertyName = Tag.Name;

		// Move to the next property to be serialized
		if( AdvanceProperty && --RemainingArrayDim <= 0 )
		{
			check(Property);
			Property = Property->PropertyLinkNext;
			// Skip over properties that don't need to be serialized.
			while( Property && !Property->ShouldSerializeValue( Ar ) )
			{
				Property = Property->PropertyLinkNext;
			}
			AdvanceProperty		= 0;
			RemainingArrayDim	= Property ? Property->ArrayDim : 0;
		}

		// If this property is not the one we expect (e.g. skipped as it matches the default value), do the brute force search.
		if( Property == NULL || Property->GetFName() != Tag.Name )
		{
			UProperty* CurrentProperty = Property;
			// Search forward...
			for ( ; Property; Property=Property->PropertyLinkNext )
			{
				if( Property->GetFName() == Tag.Name )
				{
					break;
				}
			}
			// ... and then search from the beginning till we reach the current property if it's not found.
			if( Property == NULL )
			{
				for( Property = ObjClass->PropertyLink; Property && Property != CurrentProperty; Property = Property->PropertyLinkNext )
				{
					if( Property->GetFName() == Tag.Name )
					{
						break;
					}
				}

				if( Property == CurrentProperty )
				{
					// Property wasn't found.
					Property = NULL;
				}
			}

			RemainingArrayDim = Property ? Property->ArrayDim : 0;
		}

		const UBOOL bSkipPropertyValue = Property != NULL && IgnoreProperties.HasKey(Property);
		const UBOOL bShowPropertyValue = !bSkipPropertyValue
			&&	(SearchProperties.Num() == 0
			||	(Property != NULL && SearchProperties.HasKey(Property)));

		if ( bShowPropertyValue && !bDisplayedObjectName )
		{
			bDisplayedObjectName = TRUE;
			warnf(TEXT("%s:"), *Object->GetFullName());
		}

		if ( Tag.Size >= BufferSize )
		{
			BufferSize = Tag.Size + 1;
			StartingData = (BYTE*)appRealloc(Data, BufferSize);
		}

		// zero out the data so that we don't accidentally call a copy ctor with garbage data in the dest address
		Data = StartingData;
		appMemzero(Data, BufferSize);

		//@{
		//@compatibility
		// Check to see if we are loading an old InterpCurve Struct.
		UBOOL bNeedCurveFixup = FALSE;
		if( Ar.Ver() < VER_NEW_CURVE_AUTO_TANGENTS && Tag.Type == NAME_StructProperty && Cast<UStructProperty>(Property, CLASS_IsAUStructProperty) )
		{
			FName StructName = ((UStructProperty*)Property)->Struct->GetFName();
			if( StructName == NAME_InterpCurveFloat || StructName == NAME_InterpCurveVector2D ||
				StructName == NAME_InterpCurveVector || StructName == NAME_InterpCurveTwoVectors ||
				StructName == NAME_InterpCurveQuat )
			{
				bNeedCurveFixup = TRUE;
			}
		}
		//@}

		if( !Property )
		{
			//@{
			//@compatibility
			if ( Tag.Name == NAME_InitChild2StartBone )
			{
				UProperty* NewProperty = FindField<UProperty>(ObjClass, TEXT("BranchStartBoneName"));
				if (NewProperty != NULL && NewProperty->IsA(UArrayProperty::StaticClass()) && ((UArrayProperty*)NewProperty)->Inner->IsA(UNameProperty::StaticClass()))
				{
					INT OldNameIndex, OldNameInstance;
					Ar << OldNameIndex << OldNameInstance;
					AdvanceProperty = FALSE;
					continue;
				}
			}
			//@}
			debugf( NAME_Warning, TEXT("Property %s of %s not found for package:  %s"), *Tag.Name.ToString(), *ObjClass->GetFullName(), *Ar.GetArchiveName() );
		}
		else if( Tag.ArrayIndex>=Property->ArrayDim || Tag.ArrayIndex < 0 )
		{
			debugf( NAME_Warning, TEXT("Array bounds in %s of %s: %i/%i for package:  %s"), *Tag.Name.ToString(), *GetName(), Tag.ArrayIndex, Property->ArrayDim, *Ar.GetArchiveName() );
		}
		else if( Tag.Type==NAME_StrProperty && Cast<UNameProperty>(Property) != NULL )  
		{
			FString str;  
			Ar << str;
			AdvanceProperty = TRUE;

			if ( bShowPropertyValue )
			{
				FString PropertyNameText = Property->GetName();
				if ( Property->ArrayDim != 1 )
				{
					PropertyNameText += FString::Printf(TEXT("[%i]"), Tag.ArrayIndex);
				}

				warnf(TEXT("\t%s%s"), *PropertyNameText.RightPad(32), *str);
			}
			continue; 
		}
		else if ( Tag.Type == NAME_ByteProperty && Property->GetID() == NAME_IntProperty )
		{
			// this property's data was saved as a BYTE, but the property has been changed to an INT.  Since there is no loss of data
			// possible, we can auto-convert to the right type.
			BYTE PreviousValue;

			// de-serialize the previous value
			Ar << PreviousValue;

			FString PropertyNameText = *Property->GetName();
			if ( Property->ArrayDim != 1 )
			{
				PropertyNameText += FString::Printf(TEXT("[%i]"), Tag.ArrayIndex);
			}
			warnf(TEXT("\t%s%i"), *PropertyNameText.RightPad(32), PreviousValue);
			AdvanceProperty = TRUE;
			continue;
		}
		else if( Tag.Type!=Property->GetID() )
		{
			debugf( NAME_Warning, TEXT("Type mismatch in %s of %s - Previous (%s) Current(%s) for package:  %s"), *Tag.Name.ToString(), *ObjClass->GetName(), *Tag.Type.ToString(), *Property->GetID().ToString(), *Ar.GetArchiveName() );
		}
		else if( Tag.Type==NAME_StructProperty && Tag.StructName!=CastChecked<UStructProperty>(Property)->Struct->GetFName() )
		{
			debugf( NAME_Warning, TEXT("Property %s of %s struct type mismatch %s/%s for package:  %s"), *Tag.Name.ToString(), *ObjClass->GetName(), *Tag.StructName.ToString(), *CastChecked<UStructProperty>(Property)->Struct->GetName(), *Ar.GetArchiveName() );
		}
		else if( !Property->ShouldSerializeValue(Ar) )
		{
			if ( bShowPropertyValue )
			{
				debugf( NAME_Warning, TEXT("Property %s of %s is not serializable for package:  %s"), *Tag.Name.ToString(), *ObjClass->GetName(), *Ar.GetArchiveName() );
			}
		}
		else if ( bShowPropertyValue )
		{
			// This property is ok.
			Tag.SerializeTaggedProperty( Ar, Property, Data, 0, NULL );

			//@{
			//@compatibility
			// If we're fixing up interp curves, we need to set the curve method property manually.
			if( bNeedCurveFixup )
			{
				UScriptStruct* CurveStruct = Cast<UStructProperty>(Property, CLASS_IsAUStructProperty)->Struct;
				checkSlow(CurveStruct);

				UProperty *CurveMethodProperty = FindField<UByteProperty>(CurveStruct, TEXT("InterpMethod"));

				// Old packages store the interp method value one less than what it should be
				*(BYTE*)((BYTE*)Data + CurveMethodProperty->Offset) = *(BYTE*)((BYTE*)Data + CurveMethodProperty->Offset) + 1;
			}
			//@}

			FString PropertyValue;

			// if this is an array property, export each element individually so that it's easier to read
			UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
			if ( ArrayProp != NULL )
			{
				FScriptArray* Array = (FScriptArray*)Data;
				if ( Array != NULL )
				{
					UProperty* InnerProp = ArrayProp->Inner;
					const INT ElementSize = InnerProp->ElementSize;
					for( INT i=0; i<Array->Num(); i++ )
					{
						const FString PropertyNameText = FString::Printf(TEXT("%s(%i)"), *Property->GetName(), i);
						PropertyValue.Empty();

						BYTE* PropData = (BYTE*)Array->GetData() + i * ElementSize;
						InnerProp->ExportTextItem( PropertyValue, PropData, NULL, NULL, PPF_Localized );
						warnf(TEXT("\t%s%s"), *PropertyNameText.RightPad(32), *PropertyValue);
					}
				}
			}
			else
			{
				Property->ExportTextItem(PropertyValue, Data, NULL, NULL, PPF_Localized);

				FString PropertyNameText = *Property->GetName();
				if ( Property->ArrayDim != 1 )
				{
					PropertyNameText += FString::Printf(TEXT("[%i]"), Tag.ArrayIndex);
				}
				warnf(TEXT("\t%s%s"), *PropertyNameText.RightPad(32), *PropertyValue);
			}

			if ( (Property->PropertyFlags&CPF_NeedCtorLink) != 0 )
			{
				// clean up the memory
				Property->DestroyValue( StartingData );
			}

			AdvanceProperty = TRUE;
			continue;
		}

		// Skip unknown or bad property.
		if ( bShowPropertyValue )
		{
			debugf( NAME_Warning, TEXT("Skipping %i bytes of type %s for package:  %s"), Tag.Size, *Tag.Type.ToString(), *Ar.GetArchiveName() );
			AdvanceProperty = FALSE;
		}
		else 
		{
			// if we're not supposed to show the value for this property, just skip it without logging a warning
			AdvanceProperty = TRUE;
		}

		Ar.Loader->Seek(Ar.Loader->Tell() + Tag.Size);
	}

	appFree(StartingData);

	// now the native properties
	TLookupMap<FString> SearchPropertyNames;
	for ( INT PropIdx = 0; PropIdx < SearchProperties.Num(); PropIdx++ )
	{
		SearchPropertyNames.AddItem(*SearchProperties(PropIdx)->GetName());
	}
	TMap<FString,FString> NativePropertyValues;
	if ( Object->GetNativePropertyValues(NativePropertyValues, 0) )
	{
		for ( TMap<FString,FString>::TIterator It(NativePropertyValues); It; ++It )
		{
			const FString& PropertyName = It.Key();
			const FString& PropertyValue = It.Value();

			const UBOOL bShowPropertyValue = SearchPropertyNames.Num() == 0
				|| (PropertyName.Len() > 0 && SearchPropertyNames.HasKey(PropertyName));

			if ( bShowPropertyValue && !bDisplayedObjectName )
			{
				bDisplayedObjectName = TRUE;
				warnf(TEXT("%s:"), *Object->GetFullName());
			}
			if ( bShowPropertyValue )
			{
				warnf(TEXT("\t%s%s"), *PropertyName.RightPad(32), *PropertyValue);
			}
		}
	}

	if ( bDisplayedObjectName )
		warnf(TEXT(""));

	if ( bIsArchetypeObject == TRUE )
	{
		Ar.StopSerializingDefaults();
	}
}

IMPLEMENT_CLASS(UShowTaggedPropsCommandlet)

/*-----------------------------------------------------------------------------
UListPackagesReferencing commandlet.
-----------------------------------------------------------------------------*/

/**
* Contains the linker name and filename for a package which is referencing another package.
*/
struct FReferencingPackageName
{
	/** the name of the linker root (package name) */
	FName LinkerFName;

	/** the complete filename for the package */
	FString Filename;

	/** Constructor */
	FReferencingPackageName( FName InLinkerFName, const FString& InFilename )
		: LinkerFName(InLinkerFName), Filename(InFilename)
	{
	}

	/** Comparison operator */
	inline UBOOL operator==( const FReferencingPackageName& Other ) const
	{
		return LinkerFName == Other.LinkerFName;
	}
};

inline DWORD GetTypeHash( const FReferencingPackageName& ReferencingPackageStruct )
{
	return GetTypeHash(ReferencingPackageStruct.LinkerFName);
}

IMPLEMENT_COMPARE_CONSTREF(FReferencingPackageName,UnPackageUtilities,{ return appStricmp(*A.LinkerFName.ToString(),*B.LinkerFName.ToString()); });

INT UListPackagesReferencingCommandlet::Main( const FString& Params )
{
	const TCHAR* Parms = *Params;

	TArray<FString> Tokens, Switches;
	ParseCommandLine(Parms, Tokens, Switches);

	TSet<FReferencingPackageName>	ReferencingPackages;
	TArray<FString> PackageFiles = GPackageFileCache->GetPackageFileList();


	//@todo ronp - add support for searching for references to multiple packages/resources at once.

	if( Tokens.Num() > 0 )
	{
		FString SearchName = Tokens(0);

		const UBOOL bDeletedResource = Switches.ContainsItem(TEXT("MISSINGRESOURCE"));
		if ( bDeletedResource )
		{
			warnf(TEXT("Searching for missing resource '%s'"), *SearchName);
		}
		const UBOOL bForcedPackage = Switches.ContainsItem(TEXT("FORCEPACKAGE"));
		if ( bForcedPackage )
		{
			warnf(TEXT("Forcing package lookup..."));
		}

		// determine whether we're searching references to a package or a specific resource
		INT delimPos = SearchName.InStr(TEXT("."), TRUE);

		// if there's no dots in the search name, or the last part of the name is one of the registered package extensions, we're searching for a package
		const UBOOL bIsPackage = (!bDeletedResource || bForcedPackage) && (delimPos == INDEX_NONE || GSys->Extensions.FindItemIndex(SearchName.Mid(delimPos+1)) != INDEX_NONE);

		FName SearchPackageFName=NAME_None;
		if ( bIsPackage )
		{
			// remove any extensions on the package name
			SearchPackageFName = FName(*FFilename(SearchName).GetBaseFilename());
		}
		else
		{
			// validate that this resource exists
			UObject* SearchObject = StaticLoadObject(UObject::StaticClass(), NULL, *SearchName, NULL, LOAD_NoWarn, NULL);
			if ( SearchObject == NULL && !bDeletedResource )
			{
				warnf(TEXT("Unable to load specified resource: %s"), *SearchName);
				return 1;
			}

			if ( SearchObject != NULL )
			{
				// searching for a particular resource - pull off the package name
				SearchPackageFName = SearchObject->GetOutermost()->GetFName();

				// then change the SearchName to the object's actual path name, in case the name passed on the command-line wasn't a complete path name
				SearchName = SearchObject->GetPathName();

				// make sure it doesn't get GC'd
				SearchObject->AddToRoot();
			}
		}

		INT GCIndex = 0;
		for( INT FileIndex=0; FileIndex<PackageFiles.Num(); FileIndex++ )
		{
			const FString &Filename = PackageFiles(FileIndex);

			warnf(NAME_Progress, TEXT("Loading '%s'..."), *Filename);

			UObject::BeginLoad();
			ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *Filename, LOAD_Quiet|LOAD_NoWarn, NULL, NULL );
			UObject::EndLoad();

			if( Linker )
			{
				FName LinkerFName = Linker->LinkerRoot->GetFName();

				// ignore the package if it's the one we're processing
				if( LinkerFName != SearchPackageFName )
				{
					// look for the search package in this package's ImportMap.
					for( INT ImportIndex=0; ImportIndex<Linker->ImportMap.Num(); ImportIndex++ )
					{
						FObjectImport& Import = Linker->ImportMap( ImportIndex );
						UBOOL bImportReferencesSearchPackage = FALSE;

						if ( bIsPackage )
						{
							if ( Import.ClassPackage == SearchPackageFName )
							{
								// this import's class is contained in the package we're searching for references to
								bImportReferencesSearchPackage = TRUE;
							}
							else if ( Import.ObjectName == SearchPackageFName && Import.ClassName == NAME_Package && Import.ClassPackage == NAME_Core )
							{
								// this import is the package we're searching for references to
								bImportReferencesSearchPackage = TRUE;
							}
							else if ( Import.OuterIndex != ROOTPACKAGE_INDEX )
							{
								// otherwise, determine if this import's source package is the package we're searching for references to
								// Import.SourceLinker is cleared in UObject::EndLoad, so we can't use that
								PACKAGE_INDEX OutermostLinkerIndex = Import.OuterIndex;
								for ( PACKAGE_INDEX LinkerIndex = Import.OuterIndex; LinkerIndex != ROOTPACKAGE_INDEX; )
								{
									OutermostLinkerIndex = LinkerIndex;

									// this import's outer might be in the export table if the package was saved for seek-free loading
									if ( IS_IMPORT_INDEX(LinkerIndex) )
									{
										LinkerIndex = Linker->ImportMap( -LinkerIndex - 1 ).OuterIndex;
									}
									else
									{
										LinkerIndex = Linker->ExportMap( LinkerIndex - 1 ).OuterIndex;
									}
								}

								// if the OutermostLinkerIndex is ROOTPACKAGE_INDEX, this import corresponds to the root package for this linker
								if ( IS_IMPORT_INDEX(OutermostLinkerIndex) )
								{
									FObjectImport& PackageImport = Linker->ImportMap( -OutermostLinkerIndex - 1 );
									bImportReferencesSearchPackage =	PackageImport.ObjectName	== SearchPackageFName &&
										PackageImport.ClassName		== NAME_Package &&
										PackageImport.ClassPackage	== NAME_Core;
								}
								else
								{
									check(OutermostLinkerIndex != ROOTPACKAGE_INDEX);

									FObjectExport& PackageExport = Linker->ExportMap( OutermostLinkerIndex - 1 );
									bImportReferencesSearchPackage =	PackageExport.ObjectName == SearchPackageFName;
								}
							}
						}
						else
						{
							if ( bDeletedResource )
							{
								if ( Import.ObjectName == *SearchName || Import.ClassName == *SearchName )
								{
									// this is the object we're search for
									bImportReferencesSearchPackage = TRUE;
								}
							}

							if ( !bImportReferencesSearchPackage )
							{
								FString ImportPathName = Linker->GetImportPathName(ImportIndex);
								if ( SearchName == ImportPathName )
								{
									// this is the object we're search for
									bImportReferencesSearchPackage = TRUE;
								}
								else
								{
									// see if this import's class is the resource we're searching for
									FString ImportClassPathName = Import.ClassPackage.ToString() + TEXT(".") + Import.ClassName.ToString();
									if ( ImportClassPathName == SearchName )
									{
										bImportReferencesSearchPackage = TRUE;
									}
									else if ( Import.OuterIndex > ROOTPACKAGE_INDEX )
									{
										// and OuterIndex > 0 indicates that the import's Outer is in the package's export map, which would happen
										// if the package was saved for seek-free loading;
										// we need to check the Outer in this case since we are only iterating through the ImportMap
										FString OuterPathName = Linker->GetExportPathName(Import.OuterIndex - 1);
										if ( SearchName == OuterPathName )
										{
											bImportReferencesSearchPackage = TRUE;
										}
									}
								}
							}
						}

						if ( bImportReferencesSearchPackage )
						{
							ReferencingPackages.Add( FReferencingPackageName(LinkerFName, Filename) );
							break;
						}
					}
				}
			}

			// only GC every 10 packages (A LOT faster this way, and is safe, since we are not 
			// acting on objects that would need to go away or anything)
			if ((++GCIndex % 10) == 0)
			{
				UObject::CollectGarbage(RF_Native);
			}
		}

		warnf( TEXT("%i packages reference %s:"), ReferencingPackages.Num(), *SearchName );

		// calculate the amount of padding to use when listing the referencing packages
		INT Padding=appStrlen(TEXT("Package Name"));
		for(TSet<FReferencingPackageName>::TConstIterator It(ReferencingPackages);It;++It)
		{
			Padding = Max(Padding, It->LinkerFName.ToString().Len());
		}

		warnf( TEXT("  %*s  Filename"), Padding, TEXT("Package Name"));

		// KeySort shouldn't be used with TLookupMap because then the Value for each pair in the Pairs array (which is the index into the Pairs array for that pair)
		// is no longer correct.  That doesn't matter to use because we don't use the value for anything, so sort away!
		ReferencingPackages.Sort<COMPARE_CONSTREF_CLASS(FReferencingPackageName,UnPackageUtilities)>();

		// output the list of referencers
		for(TSet<FReferencingPackageName>::TConstIterator It(ReferencingPackages);It;++It)
		{
			warnf( TEXT("  %*s  %s"), Padding, *It->LinkerFName.ToString(), *It->Filename );
		}
	}

	return 0;
}
IMPLEMENT_CLASS(UListPackagesReferencingCommandlet)

/*-----------------------------------------------------------------------------
	UPkgInfo commandlet.
-----------------------------------------------------------------------------*/

struct FExportInfo
{
	FObjectExport Export;
	INT ExportIndex;
	FString PathName;
	FString OuterPathName;
	FString ArchetypePathName;

	FExportInfo( ULinkerLoad* Linker, INT InIndex )
	: Export(Linker->ExportMap(InIndex)), ExportIndex(InIndex)
	, OuterPathName(TEXT("NULL")), ArchetypePathName(TEXT("NULL"))
	{
		PathName = Linker->GetExportPathName(ExportIndex);
		SetOuterPathName(Linker);
		SetArchetypePathName(Linker);
	}

	void SetOuterPathName( ULinkerLoad* Linker )
	{
		if ( Export.OuterIndex > 0 )
		{
			OuterPathName = Linker->GetExportPathName(Export.OuterIndex - 1);
		}
		else if ( IS_IMPORT_INDEX(Export.OuterIndex) )
		{
			OuterPathName = Linker->GetImportPathName(-Export.OuterIndex-1);
		}
	}

	void SetArchetypePathName( ULinkerLoad* Linker )
	{
		if ( Export.ArchetypeIndex > 0 )
		{
			ArchetypePathName = Linker->GetExportPathName(Export.ArchetypeIndex-1);
		}
		else if ( IS_IMPORT_INDEX(Export.ArchetypeIndex) )
		{
			ArchetypePathName = Linker->GetImportPathName(-Export.ArchetypeIndex-1);
		}
	}
};

namespace
{
	enum EExportSortType
	{
		EXPORTSORT_ExportSize,
		EXPORTSORT_ExportIndex,
		EXPORTSORT_ObjectPathname,
		EXPORTSORT_OuterPathname,
		EXPORTSORT_ArchetypePathname,
		EXPORTSORT_MAX
	};

	class FObjectExport_Sorter
	{
	public:
		static EExportSortType SortPriority[EXPORTSORT_MAX];

		// Comparison method
		static inline INT Compare( const FExportInfo& A, const FExportInfo& B )
		{
			INT Result = 0;

			for ( INT PriorityType = 0; PriorityType < EXPORTSORT_MAX; PriorityType++ )
			{
				switch ( SortPriority[PriorityType] )
				{
					case EXPORTSORT_ExportSize:
						Result = B.Export.SerialSize - A.Export.SerialSize;
						break;

					case EXPORTSORT_ExportIndex:
						Result = A.ExportIndex - B.ExportIndex;
						break;

					case EXPORTSORT_ObjectPathname:
						Result = A.PathName.Len() - B.PathName.Len();
						if ( Result == 0 )
						{
							Result = appStricmp(*A.PathName, *B.PathName);
						}
						break;

					case EXPORTSORT_OuterPathname:
						Result = A.OuterPathName.Len() - B.OuterPathName.Len();
						if ( Result == 0 )
						{
							Result = appStricmp(*A.OuterPathName, *B.OuterPathName);
						}
						break;

					case EXPORTSORT_ArchetypePathname:
						Result = A.ArchetypePathName.Len() - B.ArchetypePathName.Len();
						if ( Result == 0 )
						{
							Result = appStricmp(*A.ArchetypePathName, *B.ArchetypePathName);
						}
						break;

					case EXPORTSORT_MAX:
						return Result;
				}

				if ( Result != 0 )
				{
					break;
				}
			}
			return Result;
		}
	};

	EExportSortType FObjectExport_Sorter::SortPriority[EXPORTSORT_MAX] =
	{ EXPORTSORT_ExportIndex, EXPORTSORT_ExportSize, EXPORTSORT_ArchetypePathname, EXPORTSORT_OuterPathname, EXPORTSORT_ObjectPathname };
}

/**
 * Writes information about the linker to the log.
 *
 * @param	InLinker	if specified, changes this reporter's Linker before generating the report.
 */
void FPkgInfoReporter_Log::GeneratePackageReport( ULinkerLoad* InLinker/*=NULL*/ )
{
	check(InLinker);

	if ( InLinker != NULL )
	{
		SetLinker(InLinker);
	}

	if ( PackageCount++ > 0 )
	{
		warnf(TEXT(""));
	}

	// Display information about the package.
	FName LinkerName = Linker->LinkerRoot->GetFName();

	// Display summary info.
	GWarn->Log( TEXT("********************************************") );
	GWarn->Logf( TEXT("Package '%s' Summary"), *LinkerName.ToString() );
	GWarn->Log( TEXT("--------------------------------------------") );

	GWarn->Logf( TEXT("\t         Filename: %s"), *Linker->Filename);
	GWarn->Logf( TEXT("\t     File Version: %i"), Linker->Ver() );
	GWarn->Logf( TEXT("\t   Engine Version: %d"), Linker->Summary.EngineVersion);
	GWarn->Logf( TEXT("\t   Cooker Version: %d"), Linker->Summary.CookedContentVersion);
	GWarn->Logf( TEXT("\t     PackageFlags: %X"), Linker->Summary.PackageFlags );
	GWarn->Logf( TEXT("\t        NameCount: %d"), Linker->Summary.NameCount );
	GWarn->Logf( TEXT("\t       NameOffset: %d"), Linker->Summary.NameOffset );
	GWarn->Logf( TEXT("\t      ImportCount: %d"), Linker->Summary.ImportCount );
	GWarn->Logf( TEXT("\t     ImportOffset: %d"), Linker->Summary.ImportOffset );
	GWarn->Logf( TEXT("\t      ExportCount: %d"), Linker->Summary.ExportCount );
	GWarn->Logf( TEXT("\t     ExportOffset: %d"), Linker->Summary.ExportOffset );
	GWarn->Logf( TEXT("\tCompression Flags: %X"), Linker->Summary.CompressionFlags);

	FString szGUID = Linker->Summary.Guid.String();
	GWarn->Logf( TEXT("\t             Guid: %s"), *szGUID );
	GWarn->Log ( TEXT("\t      Generations:"));
	for( INT i = 0; i < Linker->Summary.Generations.Num(); ++i )
	{
		const FGenerationInfo& generationInfo = Linker->Summary.Generations( i );
		GWarn->Logf(TEXT("\t\t\t%d) ExportCount=%d, NameCount=%d, NetObjectCount=%d"), i, generationInfo.ExportCount, generationInfo.NameCount, generationInfo.NetObjectCount);
	}


	if( (InfoFlags&PKGINFO_Chunks) != 0 )
	{
		GWarn->Log( TEXT("--------------------------------------------") );
		GWarn->Log ( TEXT("Compression Chunks"));
		GWarn->Log ( TEXT("=========="));

		for ( INT ChunkIndex = 0; ChunkIndex < Linker->Summary.CompressedChunks.Num(); ChunkIndex++ )
		{
			FCompressedChunk& Chunk = Linker->Summary.CompressedChunks(ChunkIndex);
			GWarn->Log ( TEXT("\t*************************"));
			GWarn->Logf( TEXT("\tChunk %d:"), ChunkIndex );
			GWarn->Logf( TEXT("\t\tUncompressedOffset: %d"), Chunk.UncompressedOffset);
			GWarn->Logf( TEXT("\t\t  UncompressedSize: %d"), Chunk.UncompressedSize);
			GWarn->Logf( TEXT("\t\t  CompressedOffset: %d"), Chunk.CompressedOffset);
			GWarn->Logf( TEXT("\t\t    CompressedSize: %d"), Chunk.CompressedSize);
		}
	}

	if( (InfoFlags&PKGINFO_Names) != 0 )
	{
		GWarn->Log( TEXT("--------------------------------------------") );
		GWarn->Log ( TEXT("Name Map"));
		GWarn->Log ( TEXT("========"));
		for( INT i = 0; i < Linker->NameMap.Num(); ++i )
		{
			FName& name = Linker->NameMap( i );
			GWarn->Logf( TEXT("\t%d: Name '%s' Index %d [Internal: %s, %d]"), i, *name.ToString(), name.GetIndex(), *name.GetNameString(), name.GetNumber() );
		}
	}

	// if we _only_ want name info, skip this part completely
	if ( InfoFlags != PKGINFO_Names )
	{
		if( (InfoFlags&PKGINFO_Imports) != 0 )
		{
			GWarn->Log( TEXT("--------------------------------------------") );
			GWarn->Log ( TEXT("Import Map"));
			GWarn->Log ( TEXT("=========="));
		}

		TArray<FName> DependentPackages;
		for( INT i = 0; i < Linker->ImportMap.Num(); ++i )
		{
			FObjectImport& import = Linker->ImportMap( i );

			FName PackageName = NAME_None;
			FName OuterName = NAME_None;
			if ( import.OuterIndex != ROOTPACKAGE_INDEX )
			{
				if ( IS_IMPORT_INDEX(import.OuterIndex) )
				{
					if ( (InfoFlags&PKGINFO_Paths) != 0 )
					{
						OuterName = *Linker->GetImportPathName(-import.OuterIndex - 1);
					}
					else
					{
						FObjectImport& OuterImport = Linker->ImportMap(-import.OuterIndex-1);
						OuterName = OuterImport.ObjectName;
					}
				}
				else if ( import.OuterIndex > 0 )
				{
					if ( (InfoFlags&PKGINFO_Paths) != 0 )
					{
						OuterName = *Linker->GetExportPathName(import.OuterIndex-1);
					}
					else
					{
						FObjectExport& OuterExport = Linker->ExportMap(import.OuterIndex-1);
						OuterName = OuterExport.ObjectName;
					}
				}

				// Find the package which contains this import.  import.SourceLinker is cleared in UObject::EndLoad, so we'll need to do this manually now.
				PACKAGE_INDEX OutermostLinkerIndex = import.OuterIndex;
				for ( PACKAGE_INDEX LinkerIndex = import.OuterIndex; LinkerIndex != ROOTPACKAGE_INDEX; )
				{
					OutermostLinkerIndex = LinkerIndex;

					// this import's outer might be in the export table if the package was saved for seek-free loading
					if ( IS_IMPORT_INDEX(LinkerIndex) )
					{
						LinkerIndex = Linker->ImportMap( -LinkerIndex - 1 ).OuterIndex;
					}
					else
					{
						LinkerIndex = Linker->ExportMap( LinkerIndex - 1 ).OuterIndex;
					}
				}

				// if the OutermostLinkerIndex is ROOTPACKAGE_INDEX, this import corresponds to the root package for this linker
				if ( IS_IMPORT_INDEX(OutermostLinkerIndex) )
				{
					FObjectImport& PackageImport = Linker->ImportMap( -OutermostLinkerIndex - 1 );
					PackageName = PackageImport.ObjectName;
				}
				else
				{
					check(OutermostLinkerIndex != ROOTPACKAGE_INDEX);
					FObjectExport& PackageExport = Linker->ExportMap( OutermostLinkerIndex - 1 );
					PackageName = PackageExport.ObjectName;
				}
			}

			if ( (InfoFlags&PKGINFO_Imports) != 0 )
			{
				GWarn->Log ( TEXT("\t*************************"));
				GWarn->Logf( TEXT("\tImport %d: '%s'"), i, *import.ObjectName.ToString() );
				GWarn->Logf( TEXT("\t\t       Outer: '%s' (%d)"), *OuterName.ToString(), import.OuterIndex);
				GWarn->Logf( TEXT("\t\t     Package: '%s'"), *PackageName.ToString());
				GWarn->Logf( TEXT("\t\t       Class: '%s'"), *import.ClassName.ToString() );
				GWarn->Logf( TEXT("\t\tClassPackage: '%s'"), *import.ClassPackage.ToString() );
				GWarn->Logf( TEXT("\t\t     XObject: %s"), import.XObject ? TEXT("VALID") : TEXT("NULL"));
				GWarn->Logf( TEXT("\t\t SourceIndex: %d"), import.SourceIndex );

				// dump depends info
				if (InfoFlags & PKGINFO_Depends)
				{
					GWarn->Log(TEXT("\t\t  All Depends:"));
					if (Linker->Summary.PackageFlags & PKG_Cooked)
					{
						GWarn->Logf(TEXT("\t\t\t  Skipping (Cooked package)"));
					}
					else
					{
						TSet<FDependencyRef> AllDepends;
						Linker->GatherImportDependencies(i, AllDepends);
						INT DependsIndex = 0;
						for(TSet<FDependencyRef>::TConstIterator It(AllDepends);It;++It)
						{
							const FDependencyRef& Ref = *It;
							GWarn->Logf(TEXT("\t\t\t%i) %s"), DependsIndex++, *Ref.Linker->GetExportFullName(Ref.ExportIndex));
						}
					}
				}
			}

			if ( PackageName == NAME_None && import.ClassPackage == NAME_Core && import.ClassName == NAME_Package )
			{
				PackageName = import.ObjectName;
			}

			if ( PackageName != NAME_None && PackageName != LinkerName )
			{
				DependentPackages.AddUniqueItem(PackageName);
			}

			if ( import.ClassPackage != NAME_None && import.ClassPackage != LinkerName )
			{
				DependentPackages.AddUniqueItem(import.ClassPackage);
			}
		}

		if ( DependentPackages.Num() )
		{
			GWarn->Log( TEXT("--------------------------------------------") );
			warnf(TEXT("\tPackages referenced by %s:"), *LinkerName.ToString());
			for ( INT i = 0; i < DependentPackages.Num(); i++ )
			{
				warnf(TEXT("\t\t%i) %s"), i, *DependentPackages(i).ToString());
			}
		}
	}

	if( (InfoFlags&PKGINFO_Exports) != 0 )
	{
		GWarn->Log( TEXT("--------------------------------------------") );
		GWarn->Log ( TEXT("Export Map"));
		GWarn->Log ( TEXT("=========="));

		if ( (InfoFlags&PKGINFO_Compact) == 0 )
		{
			TArray<FExportInfo> SortedExportMap;
			SortedExportMap.Empty(Linker->ExportMap.Num());
			for( INT i = 0; i < Linker->ExportMap.Num(); ++i )
			{
				new(SortedExportMap) FExportInfo(Linker, i);
			}

			FString SortingParms;
			if ( Parse(appCmdLine(), TEXT("SORT="), SortingParms) )
			{
				TArray<FString> SortValues;
				SortingParms.ParseIntoArray(&SortValues, TEXT(","), TRUE);

				for ( INT i = 0; i < EXPORTSORT_MAX; i++ )
				{
					if ( i < SortValues.Num() )
					{
						const FString Value = SortValues(i);
						if ( Value == TEXT("index") )
						{
							FObjectExport_Sorter::SortPriority[i] = EXPORTSORT_ExportIndex;
						}
						else if ( Value == TEXT("size") )
						{
							FObjectExport_Sorter::SortPriority[i] = EXPORTSORT_ExportSize;
						}
						else if ( Value == TEXT("name") )
						{
							FObjectExport_Sorter::SortPriority[i] = EXPORTSORT_ObjectPathname;
						}
						else if ( Value == TEXT("outer") )
						{
							FObjectExport_Sorter::SortPriority[i] = EXPORTSORT_OuterPathname;
						}
						else if ( Value == TEXT("archetype") )
						{
							FObjectExport_Sorter::SortPriority[i] = EXPORTSORT_ArchetypePathname;
						}
					}
					else
					{
						FObjectExport_Sorter::SortPriority[i] = EXPORTSORT_MAX;
					}
				}
			}

			Sort<FExportInfo, FObjectExport_Sorter>( &SortedExportMap(0), SortedExportMap.Num() );

			for( INT SortedIndex = 0; SortedIndex < SortedExportMap.Num(); ++SortedIndex )
			{
				GWarn->Log ( TEXT("\t*************************"));
				FExportInfo& ExportInfo = SortedExportMap(SortedIndex);

				FObjectExport& Export = ExportInfo.Export;

				UBOOL bIsForcedExportPackage=FALSE;

				// determine if this export is a forced export in a cooked package
				FString ForcedExportString;
				if ( (Linker->Summary.PackageFlags&PKG_Cooked) != 0 && Export.HasAnyFlags(EF_ForcedExport) )
				{
					// find the package object this forced export was originally contained within
					INT PackageExportIndex = ROOTPACKAGE_INDEX;
					for ( INT OuterIndex = Export.OuterIndex; OuterIndex != ROOTPACKAGE_INDEX; OuterIndex = Linker->ExportMap(OuterIndex-1).OuterIndex )
					{
						PackageExportIndex = OuterIndex - 1;
					}

					if ( PackageExportIndex == ROOTPACKAGE_INDEX )
					{
						// this export corresponds to a top-level UPackage
						bIsForcedExportPackage = TRUE;
						ForcedExportString = TEXT(" [** FORCED **]");
					}
					else
					{
						// this export was a forced export that is not a top-level UPackage
						FObjectExport& OuterExport = Linker->ExportMap(PackageExportIndex);
						checkf(OuterExport.HasAnyFlags(EF_ForcedExport), TEXT("Export %i (%s) is a forced export but its outermost export %i (%s) is not!"),
							ExportInfo.ExportIndex, *ExportInfo.PathName, PackageExportIndex, *Linker->GetExportPathName(PackageExportIndex));

						ForcedExportString = FString::Printf(TEXT(" [** FORCED: '%s' (%i)]"), *OuterExport.ObjectName.ToString(), PackageExportIndex);
					}
				}
				GWarn->Logf( TEXT("\tExport %d: '%s'%s"), ExportInfo.ExportIndex, *Export.ObjectName.ToString(), *ForcedExportString );

				// find the name of this object's class
				INT ClassIndex = Export.ClassIndex;
				FName ClassName = ClassIndex > 0 
					? Linker->ExportMap(ClassIndex-1).ObjectName
					: IS_IMPORT_INDEX(ClassIndex)
						? Linker->ImportMap(-ClassIndex-1).ObjectName
						: FName(NAME_Class);

				// find the name of this object's parent...for UClasses, this will be the parent class
				// for UFunctions, this will be the SuperFunction, if it exists, etc.
				FString ParentName;
				if ( Export.SuperIndex > 0 )
				{
					if ( (InfoFlags&PKGINFO_Paths) != 0 )
					{
						ParentName = *Linker->GetExportPathName(Export.SuperIndex-1);
					}
					else
					{
						FObjectExport& ParentExport = Linker->ExportMap(Export.SuperIndex-1);
						ParentName = ParentExport.ObjectName.ToString();
					}
				}
				else if ( IS_IMPORT_INDEX(Export.SuperIndex) )
				{
					if ( (InfoFlags&PKGINFO_Paths) != 0 )
					{
						ParentName = *Linker->GetImportPathName(-Export.SuperIndex-1);
					}
					else
					{
						FObjectImport& ParentImport = Linker->ImportMap(-Export.SuperIndex-1);
						ParentName = ParentImport.ObjectName.ToString();
					}
				}

				// find the name of this object's Outer.  For UClasses, this will generally be the
				// top-level package itself.  For properties, a UClass, etc.
				FString OuterName;
				if ( Export.OuterIndex > 0 )
				{
					if ( (InfoFlags&PKGINFO_Paths) != 0 )
					{
						OuterName = *Linker->GetExportPathName(Export.OuterIndex - 1);
					}
					else
					{
						FObjectExport& OuterExport = Linker->ExportMap(Export.OuterIndex-1);
						OuterName = OuterExport.ObjectName.ToString();
					}
				}
				else if ( IS_IMPORT_INDEX(Export.OuterIndex) )
				{
					if ( (InfoFlags&PKGINFO_Paths) != 0 )
					{
						OuterName = *Linker->GetImportPathName(-Export.OuterIndex-1);
					}
					else
					{
						FObjectImport& OuterImport = Linker->ImportMap(-Export.OuterIndex-1);
						OuterName = OuterImport.ObjectName.ToString();
					}
				}

				FString TemplateName;
				if ( Export.ArchetypeIndex > 0 )
				{
					if ( (InfoFlags&PKGINFO_Paths) != 0 )
					{
						TemplateName = *Linker->GetExportPathName(Export.ArchetypeIndex-1);
					}
					else
					{
						FObjectExport& TemplateExport = Linker->ExportMap(Export.ArchetypeIndex-1);
						TemplateName = TemplateExport.ObjectName.ToString();
					}
				}
				else if ( IS_IMPORT_INDEX(Export.ArchetypeIndex) )
				{
					if ( (InfoFlags&PKGINFO_Paths) != 0 )
					{
						TemplateName = *Linker->GetImportPathName(-Export.ArchetypeIndex-1);
					}
					else
					{
						FObjectImport& TemplateImport = Linker->ImportMap(-Export.ArchetypeIndex-1);
						TemplateName = TemplateImport.ObjectName.ToString();
					}
				}

				GWarn->Logf( TEXT("\t\t         Class: '%s' (%i)"), *ClassName.ToString(), ClassIndex );
				GWarn->Logf( TEXT("\t\t        Parent: '%s' (%d)"), *ParentName, Export.SuperIndex );
				GWarn->Logf( TEXT("\t\t         Outer: '%s' (%d)"), *OuterName, Export.OuterIndex );
				GWarn->Logf( TEXT("\t\t     Archetype: '%s' (%d)"), *TemplateName, Export.ArchetypeIndex);
				GWarn->Logf( TEXT("\t\t      Pkg Guid: %s"), *Export.PackageGuid.String());
				GWarn->Logf( TEXT("\t\t   ObjectFlags: 0x%016I64X"), Export.ObjectFlags );
				GWarn->Logf( TEXT("\t\t          Size: %d"), Export.SerialSize );
				if ( !bHideOffsets )
				{
					GWarn->Logf( TEXT("\t\t      Offset: %d"), Export.SerialOffset );
				}
				GWarn->Logf( TEXT("\t\t       _Object: %s"), Export._Object ? TEXT("VALID") : TEXT("NULL"));
				if ( !bHideOffsets )
				{
					GWarn->Logf( TEXT("\t\t    _iHashNext: %d"), Export._iHashNext );
				}
				GWarn->Logf( TEXT("\t\t   ExportFlags: %X"), Export.ExportFlags );

				if ( bIsForcedExportPackage && Export.GenerationNetObjectCount.Num() > 0 )
				{
					warnf(TEXT("\t\tNetObjectCounts: %d generations"), Export.GenerationNetObjectCount.Num());
					for ( INT GenerationIndex = 0; GenerationIndex < Export.GenerationNetObjectCount.Num(); GenerationIndex++ )
					{
						warnf(TEXT("\t\t\t%d) %d"), GenerationIndex, Export.GenerationNetObjectCount(GenerationIndex));
					}
				}

				// dump depends info
				if (InfoFlags & PKGINFO_Depends)
				{
					if (ExportInfo.ExportIndex < Linker->DependsMap.Num())
					{
						TArray<INT>& Depends = Linker->DependsMap(ExportInfo.ExportIndex);
						GWarn->Log(TEXT("\t\t  DependsMap:"));
						if (Linker->Summary.PackageFlags & PKG_Cooked)
						{
							GWarn->Logf(TEXT("\t\t\t  Skipping (Cooked package)"));
						}
						else
						{
							for (INT DependsIndex = 0; DependsIndex < Depends.Num(); DependsIndex++)
							{
								GWarn->Logf(TEXT("\t\t\t%i) %s (%i)"),
									DependsIndex, 
									IS_IMPORT_INDEX(Depends(DependsIndex))
										? *Linker->GetImportFullName(-Depends(DependsIndex) - 1)
										: *Linker->GetExportFullName(Depends(DependsIndex) - 1),
									Depends(DependsIndex)
								);
							}

							TSet<FDependencyRef> AllDepends;
							Linker->GatherExportDependencies(ExportInfo.ExportIndex, AllDepends);
							GWarn->Log(TEXT("\t\t  All Depends:"));
							INT DependsIndex = 0;
							for(TSet<FDependencyRef>::TConstIterator It(AllDepends);It;++It)
							{
								const FDependencyRef& Ref = *It;
								GWarn->Logf(TEXT("\t\t\t%i) %s (%i)"),
									DependsIndex++,
									*Ref.Linker->GetExportFullName(Ref.ExportIndex),
									Ref.ExportIndex);
							}
						}
					}
				}
			}
		}
		else
		{
			for( INT ExportIndex=0; ExportIndex<Linker->ExportMap.Num(); ExportIndex++ )
			{
				const FObjectExport& Export = Linker->ExportMap(ExportIndex);
				warnf(TEXT("  %8i %10i %32s %s"), ExportIndex, Export.SerialSize, 
					*(Linker->GetExportClassName(ExportIndex).ToString()), 
					(InfoFlags&PKGINFO_Paths) != 0 ? *Linker->GetExportPathName(ExportIndex) : *Export.ObjectName.ToString());
			}
		}
	}


	if( (InfoFlags&PKGINFO_Thumbs) != 0 )
	{
		GWarn->Log( TEXT("--------------------------------------------") );
		GWarn->Log ( TEXT("Thumbnail Data"));
		GWarn->Log ( TEXT("=========="));

		if ( Linker->SerializeThumbnails(TRUE) )
		{
			if ( Linker->LinkerRoot->HasThumbnailMap() )
			{
				FThumbnailMap& LinkerThumbnails = Linker->LinkerRoot->AccessThumbnailMap();

				INT MaxObjectNameSize = 0;
				for ( TMap<FName, FObjectThumbnail>::TIterator It(LinkerThumbnails); It; ++It )
				{
					FName& ObjectPathName = It.Key();
					MaxObjectNameSize = Max(MaxObjectNameSize, ObjectPathName.ToString().Len());
				}

				INT ThumbIdx=0;
				for ( TMap<FName, FObjectThumbnail>::TIterator It(LinkerThumbnails); It; ++It )
				{
					FName& ObjectFullName = It.Key();
					FObjectThumbnail& Thumb = It.Value();

					GWarn->Logf(TEXT("\t\t%i) %*s: %ix%i\t\tImage Data:%i bytes"), ThumbIdx++, MaxObjectNameSize, *ObjectFullName.ToString(), Thumb.GetImageWidth(), Thumb.GetImageHeight(), Thumb.GetCompressedDataSize());
				}
			}
			else
			{
				GWarn->Logf(TEXT("%s has no thumbnail map!"), *LinkerName.ToString());
			}
		}
		else
		{
			if ( Linker->Summary.ThumbnailTableOffset > 0 )
			{
				GWarn->Logf(TEXT("Failed to load thumbnails for package %s!"), *LinkerName.ToString());
			}
		}
	}

	if( (InfoFlags&PKGINFO_CrossLevel) != 0 )
	{
		GWarn->Log( TEXT("--------------------------------------------") );
		GWarn->Log ( TEXT("CrossLevel Data"));
		GWarn->Log ( TEXT("==============="));

		if (Linker->LinkerRoot->ImportGuids.Num() > 0)
		{
			GWarn->Log ( TEXT("ImportGuids"));
			GWarn->Log ( TEXT("==========="));
			for (INT LevelIndex = 0; LevelIndex < Linker->LinkerRoot->ImportGuids.Num(); LevelIndex++)
			{
				FLevelGuids& LevelGuids = Linker->LinkerRoot->ImportGuids(LevelIndex);

				GWarn->Logf(TEXT("Level: %s"), *LevelGuids.LevelName.ToString());
				for (INT GuidIndex = 0; GuidIndex < LevelGuids.Guids.Num(); GuidIndex++)
				{
					GWarn->Logf(TEXT("  %s"), *LevelGuids.Guids(GuidIndex).String());
				}
			}
		}

		if (Linker->ExportGuidsAwaitingLookup.Num() > 0)
		{
			GWarn->Log ( TEXT("ExportGuids"));
			GWarn->Log ( TEXT("==========="));
			for (TMap<FGuid, INT>::TIterator It(Linker->ExportGuidsAwaitingLookup); It; ++It)
			{
				GWarn->Logf(TEXT("  %s"), *It.Key().String());
			}
		}
	}
}

INT UPkgInfoCommandlet::Main( const FString& Params )
{
	// turn off as it makes diffing hard
	const UBOOL bOldGPrintLogTimes = GPrintLogTimes;
	GPrintLogTimes = FALSE;

	const TCHAR* Parms = *Params;

	TArray<FString> Tokens, Switches;
	ParseCommandLine(Parms, Tokens, Switches);

	// find out which type of info we're looking for
	DWORD InfoFlags = PKGINFO_None;
	if ( Switches.ContainsItem(TEXT("names")) )
	{
		InfoFlags |= PKGINFO_Names;
	}
	if ( Switches.ContainsItem(TEXT("imports")) )
	{
		InfoFlags |= PKGINFO_Imports;
	}
	if ( Switches.ContainsItem(TEXT("exports")) )
	{
		InfoFlags |= PKGINFO_Exports;
	}
	if ( Switches.ContainsItem(TEXT("simple")) )
	{
		InfoFlags |= PKGINFO_Compact;
	}
	if ( Switches.ContainsItem(TEXT("chunks")) )
	{
		InfoFlags |= PKGINFO_Chunks;
	}
	if ( Switches.ContainsItem(TEXT("depends")) )
	{
		InfoFlags |= PKGINFO_Depends;
	}
	if ( Switches.ContainsItem(TEXT("paths")) )
	{
		InfoFlags |= PKGINFO_Paths;
	}
	if ( Switches.ContainsItem(TEXT("thumbnails")) )
	{
		InfoFlags |= PKGINFO_Thumbs;
	}
	if ( Switches.ContainsItem(TEXT("crosslevel")) )
	{
		InfoFlags |= PKGINFO_CrossLevel;
	}
	if ( Switches.ContainsItem(TEXT("all")) )
	{
		InfoFlags |= PKGINFO_All;
	}

	const UBOOL bHideOffsets = Switches.ContainsItem(TEXT("HideOffsets"));

	/** What platform are we cooking for?	*/
	UE3::EPlatformType Platform = ParsePlatformType(*Params);
	if (Platform == UE3::PLATFORM_Unknown)
	{
		Platform = UE3::PLATFORM_Windows;
	}

	// Set compression method based on platform.
	if (((Platform & UE3::PLATFORM_PS3) != 0))
	{
		// Zlib uses SPU tasks on PS3.
		GBaseCompressionMethod = COMPRESS_ZLIB;
	}
	else if( Platform & UE3::PLATFORM_Xbox360 )
	{
		// LZX is best trade-off of perf/ compression size on Xbox 360
		GBaseCompressionMethod = COMPRESS_LZX;
	}
	// All other platforms will default to compression scheme that the PC uses.

	FPkgInfoReporter* Reporter = new FPkgInfoReporter_Log(InfoFlags, bHideOffsets);

	TArray<FString> FilesInPath;
	if( Switches.ContainsItem(TEXT("AllPackages")) )
	{
		FilesInPath = GPackageFileCache->GetPackageFileList();
	}
	else
	{
		for ( INT TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++ )
		{
			FString& PackageWildcard = Tokens(TokenIndex);

			TArray<FString> PerTokenFilesInPath;
			GFileManager->FindFiles( PerTokenFilesInPath, *PackageWildcard, TRUE, FALSE );

			if( PerTokenFilesInPath.Num() == 0 )
			{
				// if no files were found, it might be an unqualified path; try prepending the .u output path
				// if one were going to make it so that you could use unqualified paths for package types other
				// than ".u", here is where you would do it
				GFileManager->FindFiles( PerTokenFilesInPath, *(appScriptOutputDir() * PackageWildcard), 1, 0 );

				if ( PerTokenFilesInPath.Num() == 0 )
				{
					TArray<FString> Paths;
					if ( GConfig->GetArray( TEXT("Core.System"), TEXT("Paths"), Paths, GEngineIni ) > 0 )
					{
						for ( INT i = 0; i < Paths.Num(); i++ )
						{
							GFileManager->FindFiles( PerTokenFilesInPath, *(Paths(i) * PackageWildcard), 1, 0 );
						}
					}
				}
				else
				{
					// re-add the path information so that GetPackageLinker finds the correct version of the file.
					FFilename WildcardPath = appScriptOutputDir() * PackageWildcard;
					for ( INT FileIndex = 0; FileIndex < PerTokenFilesInPath.Num(); FileIndex++ )
					{
						PerTokenFilesInPath(FileIndex) = WildcardPath.GetPath() * PerTokenFilesInPath(FileIndex);
					}
				}

				// Try finding package in package file cache.
				if ( PerTokenFilesInPath.Num() == 0 )
				{
					FString Filename;
					if( GPackageFileCache->FindPackageFile( *PackageWildcard, NULL, Filename ) )
					{
						new(PerTokenFilesInPath)FString(Filename);
					}
				}
			}
			else
			{
				// re-add the path information so that GetPackageLinker finds the correct version of the file.
				FFilename WildcardPath = PackageWildcard;
				for ( INT FileIndex = 0; FileIndex < PerTokenFilesInPath.Num(); FileIndex++ )
				{
					PerTokenFilesInPath(FileIndex) = WildcardPath.GetPath() * PerTokenFilesInPath(FileIndex);
				}
			}

			if ( PerTokenFilesInPath.Num() == 0 )
			{
				warnf(TEXT("No packages found using '%s'!"), *PackageWildcard);
				continue;
			}

			FilesInPath += PerTokenFilesInPath;
		}
	}

	for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		const FString &Filename = FilesInPath(FileIndex);

		{
			// reset the loaders for the packages we want to load so that we don't find the wrong version of the file
			// (otherwise, attempting to run pkginfo on e.g. Engine.xxx will always return results for Engine.u instead)
			const FString& PackageName = FPackageFileCache::PackageFromPath(*Filename);
			UPackage* ExistingPackage = FindObject<UPackage>(NULL, *PackageName, TRUE);
			if ( ExistingPackage != NULL )
			{
				ResetLoaders(ExistingPackage);
			}
		}

		UObject::BeginLoad();
		ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *Filename, LOAD_NoVerify, NULL, NULL );
		UObject::EndLoad();

		if( Linker )
		{
			Reporter->GeneratePackageReport(Linker);
		}

		UObject::CollectGarbage(RF_Native);
	}

	// turn off as it makes diffing hard
	GPrintLogTimes = bOldGPrintLogTimes;

	delete Reporter;
	Reporter = NULL;
	return 0;
}
IMPLEMENT_CLASS(UPkgInfoCommandlet)


/*-----------------------------------------------------------------------------
	UListCorruptedComponentsCommandlet
-----------------------------------------------------------------------------*/

/**
 * This commandlet is designed to find (and in the future, possibly fix) content that is affected by the components bug described in
 * TTPRO #15535 and UComponentProperty::InstanceComponents()
 */
INT UListCorruptedComponentsCommandlet::Main(const FString& Params)
{
	// Parse command line args.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	const TCHAR* Parms = *Params;
	ParseCommandLine(Parms, Tokens, Switches);

	// Build package file list.
	const TArray<FString> FilesInPath( GPackageFileCache->GetPackageFileList() );
	if( FilesInPath.Num() == 0 )
	{
		warnf( NAME_Warning, TEXT("No packages found") );
		return 1;
	}

	const UBOOL bCheckVersion = Switches.ContainsItem(TEXT("CHECKVER"));

	// Iterate over all files doing stuff.
	for( INT FileIndex = 0 ; FileIndex < FilesInPath.Num() ; ++FileIndex )
	{
		const FFilename& Filename = FilesInPath(FileIndex);
		warnf( NAME_Log, TEXT("Loading %s"), *Filename );

		UObject* Package = UObject::LoadPackage( NULL, *Filename, LOAD_None );
		if( Package == NULL )
		{
			warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
		}
		else if( bCheckVersion && Package->GetLinkerVersion() != GPackageFileVersion )
		{
			warnf( NAME_Log, TEXT("Version mismatch. Package [%s] should be resaved."), *Filename );
		}

		UBOOL bInsertNewLine = FALSE;
		for ( TObjectIterator<UComponent> It; It; ++It )
		{
			if ( It->IsIn(Package) && !It->IsTemplate(RF_ClassDefaultObject) )
			{
				UComponent* Component = *It;
				UComponent* ComponentTemplate = Cast<UComponent>(Component->GetArchetype());
				UObject* Owner = Component->GetOuter();
				UObject* TemplateOwner = ComponentTemplate->GetOuter();
				if ( !ComponentTemplate->HasAnyFlags(RF_ClassDefaultObject) )
				{
					if ( TemplateOwner != Owner->GetArchetype() )
					{
						bInsertNewLine = TRUE;

						FString RealArchetypeName;
						if ( Component->TemplateName != NAME_None )
						{
							UComponent* RealArchetype = Owner->GetArchetype()->FindComponent(Component->TemplateName);
							if ( RealArchetype != NULL )
							{
								RealArchetypeName = RealArchetype->GetFullName();
							}
							else
							{
								RealArchetypeName = FString::Printf(TEXT("NULL: no matching components found in Owner Archetype %s"), *Owner->GetArchetype()->GetFullName());
							}
						}
						else
						{
							RealArchetypeName = TEXT("NULL");
						}

						warnf(TEXT("\tPossible corrupted component: '%s'	Archetype: '%s'	TemplateName: '%s'	ResolvedArchetype: '%s'"),
							*Component->GetFullName(), 
							*ComponentTemplate->GetPathName(),
							*Component->TemplateName.ToString(),
							*RealArchetypeName);
					}

				}
			}
		}

		if ( bInsertNewLine )
		{
			warnf(TEXT(""));
		}
		UObject::CollectGarbage( RF_Native );
	}

	return 0;
}

IMPLEMENT_CLASS(UListCorruptedComponentsCommandlet);

/*-----------------------------------------------------------------------------
	UAnalyzeCookedPackages commandlet.
-----------------------------------------------------------------------------*/

INT UAnalyzeCookedPackagesCommandlet::Main( const FString& Params )
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	const TCHAR* Parms = *Params;
	ParseCommandLine(Parms, Tokens, Switches);

	// Tokens on the command line are package wildcards.
	for( INT TokenIndex=0; TokenIndex<Tokens.Num(); TokenIndex++ )
	{
		// Find all files matching current wildcard.
		FFilename		Wildcard = Tokens(TokenIndex);
		TArray<FString> Filenames;
		GFileManager->FindFiles( Filenames, *Wildcard, TRUE, FALSE );

		// Iterate over all found files.
		for( INT FileIndex = 0; FileIndex < Filenames.Num(); FileIndex++ )
		{
			const FString& Filename = Wildcard.GetPath() + PATH_SEPARATOR + Filenames(FileIndex);

			UObject::BeginLoad();
			ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *Filename, LOAD_None, NULL, NULL );
			UObject::EndLoad();

			if( Linker )
			{
				check(Linker->LinkerRoot);
				check(Linker->Summary.PackageFlags & PKG_Cooked);

				// Display information about the package.
				FName LinkerName = Linker->LinkerRoot->GetFName();

				// Display summary info.
				GWarn->Logf( TEXT("********************************************") );
				GWarn->Logf( TEXT("Package '%s' Summary"), *LinkerName.ToString() );
				GWarn->Logf( TEXT("--------------------------------------------") );

				GWarn->Logf( TEXT("\t     Version: %i"), Linker->Ver() );
				GWarn->Logf( TEXT("\tPackageFlags: %x"), Linker->Summary.PackageFlags );
				GWarn->Logf( TEXT("\t   NameCount: %d"), Linker->Summary.NameCount );
				GWarn->Logf( TEXT("\t  NameOffset: %d"), Linker->Summary.NameOffset );
				GWarn->Logf( TEXT("\t ImportCount: %d"), Linker->Summary.ImportCount );
				GWarn->Logf( TEXT("\tImportOffset: %d"), Linker->Summary.ImportOffset );
				GWarn->Logf( TEXT("\t ExportCount: %d"), Linker->Summary.ExportCount );
				GWarn->Logf( TEXT("\tExportOffset: %d"), Linker->Summary.ExportOffset );

				FString szGUID = Linker->Summary.Guid.String();
				GWarn->Logf( TEXT("\t        Guid: %s"), *szGUID );
				GWarn->Logf( TEXT("\t Generations:"));
				for( INT i=0; i<Linker->Summary.Generations.Num(); i++ )
				{
					const FGenerationInfo& GenerationInfo = Linker->Summary.Generations( i );
					GWarn->Logf( TEXT("\t\t%d) ExportCount=%d, NameCount=%d"), i, GenerationInfo.ExportCount, GenerationInfo.NameCount );
				}

				GWarn->Logf( TEXT("") );
				GWarn->Logf( TEXT("Exports:") );
				GWarn->Logf( TEXT("Class Outer Name Size Offset ExportFlags ObjectFlags") );

				for( INT i = 0; i < Linker->ExportMap.Num(); ++i )
				{
					FObjectExport& Export = Linker->ExportMap( i );

					// Find the name of this object's class.
					INT ClassIndex	= Export.ClassIndex;
					FName ClassName = NAME_Class;
					if( ClassIndex > 0 )
					{
						ClassName = Linker->ExportMap(ClassIndex-1).ObjectName;
					}
					else if( ClassIndex < 0 )
					{
						Linker->ImportMap(-ClassIndex-1).ObjectName;
					}

					// Find the name of this object's Outer.  For UClasses, this will generally be the
					// top-level package itself.  For properties, a UClass, etc.
					FName OuterName = NAME_None;
					if ( Export.OuterIndex > 0 )
					{
						FObjectExport& OuterExport = Linker->ExportMap(Export.OuterIndex-1);
						OuterName = OuterExport.ObjectName;
					}
					else if ( Export.OuterIndex < 0 )
					{
						FObjectImport& OuterImport = Linker->ImportMap(-Export.OuterIndex-1);
						OuterName = OuterImport.ObjectName;
					}

					//GWarn->Logf( TEXT("Class Outer Name Size Offset ExportFlags ObjectFlags") );
					GWarn->Logf( TEXT("%s %s %s %i %i %x 0x%016I64X"), 
						*ClassName.ToString(), 
						*OuterName.ToString(), 
						*Export.ObjectName.ToString(), 
						Export.SerialSize, 
						Export.SerialOffset, 
						Export.ExportFlags, 
						Export.ObjectFlags );
				}
			}

			UObject::CollectGarbage(RF_Native);
		}
	}
	return 0;
}
IMPLEMENT_CLASS(UAnalyzeCookedPackagesCommandlet)

/*-----------------------------------------------------------------------------
	UMineCookedPackages commandlet.
-----------------------------------------------------------------------------*/

INT UMineCookedPackagesCommandlet::Main( const FString& Params )
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	const TCHAR* Parms = *Params;
	ParseCommandLine(Parms, Tokens, Switches);

	// Create the connection object; needs to be deleted via "delete".
	FDataBaseConnection* Connection = FDataBaseConnection::CreateObject();
	check(Connection);

	// Name of database to use.
	FString DataSource;
	if( !Parse( Parms, TEXT("-DATABASE="), DataSource ) )
	{
		warnf( TEXT("Use -DATABASE= to specify database.") );
		return -1;
	}

	// Name of catalog to use.
	FString Catalog;
	if( !Parse( Parms, TEXT("CATALOG="), Catalog ) )
	{
		warnf( TEXT("Use -CATALOG= to specify catalog.") );
		return -2;
	}

	// Create the connection string with Windows Authentication as the way to handle permissions/ login/ security.
	FString ConnectionString	= FString::Printf(TEXT("Provider=sqloledb;Data Source=%s;Initial Catalog=%s;Trusted_Connection=Yes;"),*DataSource,*Catalog);

	// Try to open connection to DB - this is a synchronous operation.
	if( Connection->Open( *ConnectionString, NULL, NULL ) )
	{
		warnf(NAME_DevDataBase,TEXT("Connection to %s.%s succeeded"),*DataSource,*Catalog);
	}
	// Connection failed :(
	else
	{
		warnf(NAME_DevDataBase,TEXT("Connection to %s.%s failed"),*DataSource,*Catalog);
		// Only delete object - no need to close as connection failed.
		delete Connection;
		Connection = NULL;
		// Early out on error.
		return -1;
	}

	UBOOL bClear = (Switches.FindItemIndex(TEXT("CLEAR")) != INDEX_NONE);
	UBOOL bClearOnly = (Switches.FindItemIndex(TEXT("CLEARONLY")) != INDEX_NONE);
	bClear |= bClearOnly;

	if (bClear == TRUE)
	{
		// Clear out the database...
		FString CommandString = TEXT("EXEC CLEAR");
		Connection->Execute(*CommandString);
	}

	if (bClearOnly == FALSE)
	{
		// Make sure all classes are loaded.
		TArray<FString> ScriptPackageNames;
		appGetScriptPackageNames(ScriptPackageNames, SPT_AllScript, FALSE );
		extern void LoadPackageList(const TArray<FString>& PackageNames);
		LoadPackageList( ScriptPackageNames );

		// Iterate over all classes, adding them to the DB.
		for( TObjectIterator<UClass> It; It; ++It )
		{
			UClass* Class = *It;
			UClass* Super = Class->GetSuperClass();
			TArray<UClass*> InheritanceTree;

			// Add all super classes, down to NULL class terminating chain.
			InheritanceTree.AddItem(Super);
			while( Super )
			{
				Super = Super->GetSuperClass();
				InheritanceTree.AddItem(Super);
			};

			// Add classes in reverse order so we can rely on the fact that super classes are added to the DB first. The ADDCLASS stored procedure
			// has special case handling for the None class.
			for( INT i=InheritanceTree.Num()-1; i>=0; i-- )
			{
				Super = InheritanceTree(i);

				// Add class, super, depth. It'll be multiple rows per class and we can do a select based on depth == 0 to get unique ones.
				FString ClassString = FString::Printf(TEXT("EXEC ADDCLASS @CLASSNAME='%s', @SUPERNAME='%s', @SUPERDEPTH=%i"),*Class->GetName(),*Super->GetName(),i);
				verify( Connection->Execute( *ClassString ) );
			}
		}

		// Add unknown fallback class.
		verify( Connection->Execute( TEXT("EXEC ADDCLASS @CLASSNAME='Unknown', @SUPERNAME='None', @SUPERDEPTH=0") ) );

		// Collect garbage to unload previously loaded packages before we reset loaders.
		UObject::CollectGarbage(RF_Native);

		// Reset loaders to avoid finding packages that are already loaded. This will load bulk data associated with those objects, which is why
		// we trim down the set of loaded objects via GC before.
		UObject::ResetLoaders( NULL );

		// We need to add dependencies after adding all packages as the stored procedure relies on the packages to already
		// have been added at the time of execution. We do this by keeping track of all SQL calls to execute at the end. 
		TArray<FString> PackageDependencies;

		// Filenames of packages to mine
		TArray<FString> Filenames;

		// Whether we want to allow uncooked packages.
		UBOOL bAllowUncookedPackages = FALSE;
		if( Switches.ContainsItem(TEXT("ALLOWUNCOOKED")) )
		{
			bAllowUncookedPackages = TRUE;
		}

		// Use -ALLUNCOOKED to operate on all content packages.
		if( Switches.ContainsItem(TEXT("ALLUNCOOKED")) )
		{
			bAllowUncookedPackages = TRUE;
			Filenames = GPackageFileCache->GetPackageFileList();
		}
		else
		{
			// Tokens on the command line are package wildcards.
			for( INT TokenIndex=0; TokenIndex<Tokens.Num(); TokenIndex++ )
			{
				// Find all files matching current wildcard.
				FFilename		Wildcard = Tokens(TokenIndex);
				TArray<FString>	TokenFilenames;
				GFileManager->FindFiles( TokenFilenames, *Wildcard, TRUE, FALSE );
				for( INT TokenFileIndex=0; TokenFileIndex<TokenFilenames.Num(); TokenFileIndex++ )
				{
					new(Filenames)FString(*(Wildcard.GetPath() + PATH_SEPARATOR + TokenFilenames(TokenFileIndex)));
				}
			}
		}

		// Iterate over all found files.
		for( INT FileIndex = 0; FileIndex < Filenames.Num(); FileIndex++ )
		{
			const FFilename& Filename = Filenames(FileIndex);
			debugf(TEXT("Handling %s"),*Filename);

			// Code currently doesn't handle fully compressed files.
			if( GFileManager->UncompressedFileSize( *Filename ) != -1 )
			{
				warnf(TEXT("Skipping fully compressed file %s"),*Filename);
				continue;
			}

			// Pick the right platform compression format for fully compressed files.
			if( Filename.ToUpper().InStr(TEXT("COOKEDXBOX360")) != INDEX_NONE )
			{
				GBaseCompressionMethod = COMPRESS_DefaultXbox360;
			}
			else if( Filename.ToUpper().InStr(TEXT("COOKEDPS3")) != INDEX_NONE )
			{
				GBaseCompressionMethod = COMPRESS_DefaultPS3;
			}
			else
			{
				GBaseCompressionMethod = COMPRESS_Default;
			}

			// Get the linker. ResetLoaders above guarantees that it has to be loded from disk. 
			// This won't load the package but rather just the package file summary and header.
			UObject::BeginLoad();
			ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *Filename, LOAD_None, NULL, NULL );
			UObject::EndLoad();

			if( Linker && ((Linker->Summary.PackageFlags & PKG_Cooked) || bAllowUncookedPackages) )
			{
				warnf(TEXT("Mining %s"), *Filename);

				check(Linker->LinkerRoot);

				// Add package information to DB.
				FString PackageName = Linker->LinkerRoot->GetName();
				INT CompressedSize		= -1; 
				INT UncompressedSize	= GFileManager->UncompressedFileSize( *Filename );
				INT FileSize			= GFileManager->FileSize( *Filename );
				// Regular file, not fully compressed.
				if( UncompressedSize == -1 )
				{
					UncompressedSize = FileSize;
				}
				// Fully compressed.
				else
				{
					CompressedSize = FileSize;
				}
				INT NameTableSize		= Linker->Summary.ImportOffset - Linker->Summary.NameOffset;
				INT	ImportTableSize		= Linker->Summary.ExportOffset - Linker->Summary.ImportOffset;
				INT	ExportTableSize		= Linker->Summary.DependsOffset - Linker->Summary.ExportOffset;
				UBOOL bIsMapPackage		= (Linker->Summary.PackageFlags & PKG_ContainsMap);

				FString PackageString = FString::Printf(TEXT("EXEC ADDPACKAGE @PACKAGENAME='%s', @BISMAPPACKAGE='%s', @COMPRESSEDSIZE=%i, @UNCOMPRESSEDSIZE=%i, @TOTALHEADERSIZE=%i, "),
					*PackageName,
					bIsMapPackage ? TEXT("TRUE") : TEXT("FALSE"),
					CompressedSize,
					UncompressedSize,
					Linker->Summary.TotalHeaderSize );
				PackageString += FString::Printf(TEXT("@NAMETABLESIZE=%i, @IMPORTTABLESIZE=%i, @EXPORTTABLESIZE=%i, @NAMECOUNT=%i, @IMPORTCOUNT=%i, @EXPORTCOUNT=%i"),
					NameTableSize,
					ImportTableSize,
					ExportTableSize,
					Linker->Summary.NameCount,
					Linker->Summary.ImportCount,
					Linker->Summary.ExportCount );
				verify( Connection->Execute( *PackageString ) );

				// Keep track of package dependencies to execute at the end
				for( INT DependencyIndex=0; DependencyIndex < Linker->Summary.AdditionalPackagesToCook.Num(); DependencyIndex++ )
				{
					const FString& DependencyName = Linker->Summary.AdditionalPackagesToCook(DependencyIndex);
					new(PackageDependencies) FString(*FString::Printf(TEXT("ADDPACKAGEDEPENDENCY @PACKAGENAME='%s', @DEPENDENCYNAME='%s'"),*PackageName,*DependencyName));
				}

				// Iterate over all exports, including forced ones, and add their information to the DB.
				for( INT ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ++ExportIndex )
				{
					FObjectExport& Export = Linker->ExportMap( ExportIndex );

					// Find the name of this object's class.
					FName ClassName = NAME_Class;
					if( Export.ClassIndex > 0 )
					{
						ClassName = Linker->ExportMap(Export.ClassIndex-1).ObjectName;
					}
					else if( Export.ClassIndex < 0 )
					{
						ClassName = Linker->ImportMap(-Export.ClassIndex-1).ObjectName;
					}

					// Look up the name of this export. Correctly handles forced exports.
					FString ObjectName = Linker->GetExportPathName(ExportIndex,NULL,TRUE);

					// Add export.
					FString ExportString = FString::Printf(TEXT("EXEC ADDEXPORT @PACKAGENAME='%s', @CLASSNAME='%s', @OBJECTNAME='%s', @SIZE=%i"),
						*PackageName, 
						*ClassName.ToString(), 
						*ObjectName, 
						Export.SerialSize );
					verify( Connection->Execute( *ExportString ) );

					// Special handling of texture objects
					if( ClassName == ULightMapTexture2D::StaticClass()->GetFName()
						|| ClassName == UShadowMapTexture2D::StaticClass()->GetFName() 
						|| ClassName == UTexture2D::StaticClass()->GetFName() 
						)
					{
						// Add export with TFC_ prefixed to the object name
						UTexture2D* Texture = LoadObject<UTexture2D>( NULL, *ObjectName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL );
						if( Texture )
						{
							FString ExportString = FString::Printf(TEXT("EXEC ADDEXPORT @PACKAGENAME='%s', @CLASSNAME='%s', @OBJECTNAME='TFC_%s', @SIZE=%i"),
								*PackageName, 
								*ClassName.ToString(), 
								*ObjectName, 
								Texture->CalcTextureMemorySize( TMC_AllMips ) );
							verify( Connection->Execute( *ExportString ) );
						}
					}
				}
			}
			else
			{
				warnf(TEXT("Error opening %s. Skipping."),*Filename);
			}

			UObject::CollectGarbage(RF_Native);
		}

		// Add dependency information now that all packages have been added.
		for( INT DependencyIndex=0; DependencyIndex < PackageDependencies.Num(); DependencyIndex++ )
		{
			verify( Connection->Execute( *PackageDependencies(DependencyIndex) ) );
		}
	}

	// Close & delete connection;
	Connection->Close();
	delete Connection;
	Connection = NULL;

	return 0;
}
IMPLEMENT_CLASS(UMineCookedPackagesCommandlet)

/*-----------------------------------------------------------------------------
	UCheckForSimplifiedMeshes commandlet.
-----------------------------------------------------------------------------*/

INT UCheckForSimplifiedMeshesCommandlet::Main( const FString& Params )
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;
	const TCHAR* Parms = *Params;
	ParseCommandLine(Parms, Tokens, Switches);

	// Make sure all classes are loaded.
	TArray<FString> ScriptPackageNames;
	appGetScriptPackageNames(ScriptPackageNames, SPT_AllScript, FALSE );
	extern void LoadPackageList(const TArray<FString>& PackageNames);
	LoadPackageList( ScriptPackageNames );

	// Collect garbage to unload previously loaded packages before we reset loaders.
	UObject::CollectGarbage(RF_Native);

	// Reset loaders to avoid finding packages that are already loaded. This will load bulk data associated with those objects, which is why
	// we trim down the set of loaded objects via GC before.
	UObject::ResetLoaders( NULL );

	// Filenames of packages to mine
	TArray<FString> Filenames;

	// Tokens on the command line are package wildcards.
	for( INT TokenIndex=0; TokenIndex<Tokens.Num(); TokenIndex++ )
	{
		// Find all files matching current wildcard.
		FFilename		Wildcard = Tokens(TokenIndex);
		TArray<FString>	TokenFilenames;
		GFileManager->FindFiles( TokenFilenames, *Wildcard, TRUE, FALSE );
		for( INT TokenFileIndex=0; TokenFileIndex<TokenFilenames.Num(); TokenFileIndex++ )
		{
			new(Filenames)FString(*(Wildcard.GetPath() + PATH_SEPARATOR + TokenFilenames(TokenFileIndex)));
		}
	}

	// Maintain a list of all simplified meshes that are found.
	TSet<FString> SimplifiedMeshes;

	// Iterate over all found files.
	for( INT FileIndex = 0; FileIndex < Filenames.Num(); FileIndex++ )
	{
		const FFilename& Filename = Filenames(FileIndex);
		warnf(NAME_Log, TEXT("Handling %s"),*Filename);

		// Code currently doesn't handle fully compressed files.
		if( GFileManager->UncompressedFileSize( *Filename ) != -1 )
		{
			warnf(TEXT("Skipping fully compressed file %s"),*Filename);
			continue;
		}

		// Pick the right platform compression format for fully compressed files.
		if( Filename.ToUpper().InStr(TEXT("COOKEDXBOX360")) != INDEX_NONE )
		{
			GBaseCompressionMethod = COMPRESS_DefaultXbox360;
		}
		else if( Filename.ToUpper().InStr(TEXT("COOKEDPS3")) != INDEX_NONE )
		{
			GBaseCompressionMethod = COMPRESS_DefaultPS3;
		}
		else
		{
			GBaseCompressionMethod = COMPRESS_Default;
		}

		// Get the linker. ResetLoaders above guarantees that it has to be loded from disk. 
		// This won't load the package but rather just the package file summary and header.
		UObject::BeginLoad();
		ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *Filename, LOAD_None, NULL, NULL );
		UObject::EndLoad();

		if( Linker && ((Linker->Summary.PackageFlags & PKG_Cooked)) )
		{
			check(Linker->LinkerRoot);

			// Add package information to DB.
			FString PackageName = Linker->LinkerRoot->GetName();
			INT CompressedSize		= -1; 
			INT UncompressedSize	= GFileManager->UncompressedFileSize( *Filename );
			INT FileSize			= GFileManager->FileSize( *Filename );
			// Regular file, not fully compressed.
			if( UncompressedSize == -1 )
			{
				UncompressedSize = FileSize;
			}
			// Fully compressed.
			else
			{
				CompressedSize = FileSize;
			}
			INT NameTableSize		= Linker->Summary.ImportOffset - Linker->Summary.NameOffset;
			INT	ImportTableSize		= Linker->Summary.ExportOffset - Linker->Summary.ImportOffset;
			INT	ExportTableSize		= Linker->Summary.DependsOffset - Linker->Summary.ExportOffset;
			UBOOL bIsMapPackage		= (Linker->Summary.PackageFlags & PKG_ContainsMap);

			// Iterate over all exports, including forced ones, and add their information to the DB.
			for( INT ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ++ExportIndex )
			{
				FObjectExport& Export = Linker->ExportMap( ExportIndex );

				// Find the name of this object's class.
				FName ClassName = NAME_Class;
				if( Export.ClassIndex > 0 )
				{
					ClassName = Linker->ExportMap(Export.ClassIndex-1).ObjectName;
				}
				else if( Export.ClassIndex < 0 )
				{
					ClassName = Linker->ImportMap(-Export.ClassIndex-1).ObjectName;
				}

				// Look up the name of this export. Correctly handles forced exports.
				FString ObjectName = Linker->GetExportPathName(ExportIndex,NULL,TRUE);

				// Special handling of static mesh objects.
				static FName StaticMeshClassName = UStaticMesh::StaticClass()->GetFName();
				if( ClassName == StaticMeshClassName )
				{
					// Add export with TFC_ prefixed to the object name
					UStaticMesh* StaticMesh = LoadObject<UStaticMesh>( NULL, *ObjectName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL );
					if( StaticMesh )
					{
						if ( StaticMesh->bHasBeenSimplified )
						{
							warnf( NAME_Log, TEXT("\tStaticMesh %s has been simplified."), *ObjectName );
							SimplifiedMeshes.Add( ObjectName );
						}
					}
					else
					{
						warnf( NAME_Warning, TEXT("\tUnable to load %s"), *ObjectName );
					}
				}
			}
		}
		else
		{
			warnf(NAME_Warning, TEXT("Error opening %s. Skipping."),*Filename);
		}

		UObject::CollectGarbage(RF_Native);
	}

	// Log out the list of all simplified meshes found.
	warnf( NAME_Log, TEXT("-------------------------------------------------------------") );
	warnf( NAME_Log, TEXT("Found %d simplified meshes:"), SimplifiedMeshes.Num() );
	for ( TSet<FString>::TConstIterator It(SimplifiedMeshes); It; ++It )
	{
		warnf( NAME_Log, TEXT("\t%s"), **It );
	}

	return 0;
}
IMPLEMENT_CLASS(UCheckForSimplifiedMeshesCommandlet)







struct SetTextureLODGroupFunctor
{
	void CleanUpGADTags(){}

	template< typename OBJECTYPE >
	void DoIt( UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches )
	{
		UBOOL bDirtyPackage = FALSE;

		const FName& PackageName = Package->GetFName(); 
		FString PackageFileName;
		GPackageFileCache->FindPackageFile( *PackageName.ToString(), NULL, PackageFileName );
		//warnf( NAME_Log, TEXT("  Loading2 %s"), *PackageFileName );

		/** if we should auto checkout packages that need to be saved**/
		const UBOOL bAutoCheckOut = ParseParam(appCmdLine(),TEXT("AutoCheckOutPackages"));

#if HAVE_SCC
		// Ensure source control is initialized and shut down properly
		FScopedSourceControl SourceControl;

		if ( bAutoCheckOut && FSourceControl::ForceGetStatus( PackageFileName ) == SCC_NotCurrent )
		{
			warnf( NAME_Log, TEXT("Skipping %s (Not at head source control revision)"), *PackageFileName );
			return;
		}
#endif // #if HAVE_SCC

		for( TObjectIterator<OBJECTYPE> It; It; ++It )
		{
			OBJECTYPE* Texture2D = *It;

			if( Texture2D->IsIn( Package ) == FALSE )
			{
				continue;
			}

			if( Package->GetLinker() != NULL
				&& Package->GetLinker()->Summary.EngineVersion == 2904 )
			{
				warnf( NAME_Log, TEXT( "Already 2904" ) );
				continue;
			}

			UBOOL bDirty = FALSE;
			UBOOL bIsSpec = FALSE;
			UBOOL bIsNormal = FALSE;

			const FString& TextureName = Texture2D->GetPathName();

			if( ParseParam(appCmdLine(),TEXT("VERBOSE")) == TRUE )
			{
				warnf( NAME_Log, TEXT( "TextureName: %s" ), *TextureName );
			}

			// do not set the LOD on in editor ThumbnailTextures
			if( (TextureName.Right(16)).InStr( TEXT("ThumbnailTexture" )) != INDEX_NONE )
			{
				continue;
			}

			if( (TextureName.Right(10)).InStr( TEXT("_Flattened" )) != INDEX_NONE )
			{
				continue;
			}




			const BYTE OrigGroup = Texture2D->LODGroup;
			const INT OrigLODBias = Texture2D->LODBias;

			// due to enum fiasco 2007 now we need to find which "type" it is based off the package name
			enum EPackageType
			{
				PACKAGE_CHARACTER,
				PACKAGE_EFFECTS,
				PACKAGE_SKYBOX,
				PACKAGE_UI,
				PACKAGE_VEHICLE,
				PACKAGE_WEAPON,
				PACKAGE_WORLD,
				PACKAGE_LUT,
				PACKAGE_Cinematic,
			};


			EPackageType ThePackageType = PACKAGE_WORLD;

			// DETERMINE which package type this package is

			// if not in the special effects non filtered group already
			if( ( PackageFileName.ToUpper().InStr( TEXT("EFFECTS") ) != INDEX_NONE ) )
			{
				ThePackageType = PACKAGE_EFFECTS;
			}
			else if( ( PackageName.ToString().Left(3).InStr( TEXT("CH_") ) != INDEX_NONE )
				|| ( PackageFileName.ToUpper().InStr( TEXT("CHARACTER" )) != INDEX_NONE ) 
				|| ( PackageFileName.ToUpper().InStr( TEXT("SOLDIERS" )) != INDEX_NONE ) 
				|| ( PackageFileName.ToUpper().InStr( TEXT("NPC" )) != INDEX_NONE ) 
				)
			{
				ThePackageType = PACKAGE_CHARACTER;
			}
			else if( ( ( PackageName.ToString().Left(3).InStr( TEXT("VH_") ) != INDEX_NONE )
				|| ( PackageFileName.ToUpper().InStr( TEXT("VEHICLE" )) != INDEX_NONE ) )
				&& ( PackageFileName.InStr( TEXT("GOW_Vehicles" )) == INDEX_NONE ) // things in this package are world and not vehicle
				)
			{
				ThePackageType = PACKAGE_VEHICLE;
			}
			else if( ( PackageName.ToString().Left(3).InStr( TEXT("WP_") ) != INDEX_NONE )
				|| ( PackageFileName.ToUpper().InStr( TEXT("WEAPON" )) != INDEX_NONE ) 
				)
			{
				ThePackageType = PACKAGE_WEAPON;
			}

			else if( ( PackageName.ToString().Left(3).InStr( TEXT("UI_") ) != INDEX_NONE )
				|| ( PackageFileName.ToUpper().InStr( TEXT("INTERFACE" )) != INDEX_NONE ) 
				)
			{
				ThePackageType = PACKAGE_UI;
			}
			else if( PackageFileName.InStr( TEXT("Sky") ) != INDEX_NONE )
			{
				ThePackageType = PACKAGE_SKYBOX;
			}
			else if( PackageFileName.InStr( TEXT("LUT_") ) != INDEX_NONE )
			{
				ThePackageType = PACKAGE_LUT;
			}
			else if( PackageFileName.InStr( TEXT("Cine") ) != INDEX_NONE )
			{
				ThePackageType = PACKAGE_Cinematic;
			}
			else
			{
				ThePackageType = PACKAGE_WORLD;
			}


			 //// SPECULAR
			if( ( ( TextureName.ToUpper().InStr( TEXT("SPEC" )) != INDEX_NONE )  // gears
				|| ( TextureName.ToUpper().InStr( TEXT("_ALPHA" )) != INDEX_NONE )  // gears
				|| ( TextureName.ToUpper().InStr( TEXT("_S0" )) != INDEX_NONE )  // ut
				|| ( TextureName.ToUpper().InStr( TEXT("_S_" )) != INDEX_NONE )  // ut
				|| ( (TextureName.ToUpper().Right(2)).InStr( TEXT("_S" )) != INDEX_NONE )  // ut
				|| ( (TextureName.ToUpper().Right(4)).InStr( TEXT("_SPM" )) != INDEX_NONE ) // specular mask
				|| ( (TextureName.ToUpper().Right(4)).InStr( TEXT("-SPM" )) != INDEX_NONE ) // specular mask (this is incorrect naming convention and will be expunged post haste)
				   )
				)
			{
				bIsSpec = TRUE;

				if( ( ThePackageType == PACKAGE_WORLD ) && ( Texture2D->LODGroup != TEXTUREGROUP_WorldSpecular ) )
				{
					Texture2D->LODGroup = TEXTUREGROUP_WorldSpecular; 
					bDirty = TRUE; 
				}
				else if( ( ThePackageType == PACKAGE_CHARACTER ) && ( Texture2D->LODGroup != TEXTUREGROUP_CharacterSpecular ) )
				{
					Texture2D->LODGroup = TEXTUREGROUP_CharacterSpecular; 
					bDirty = TRUE; 
				}
				else if( ( ThePackageType == PACKAGE_WEAPON ) && ( Texture2D->LODGroup != TEXTUREGROUP_WeaponSpecular ) )
				{
					Texture2D->LODGroup = TEXTUREGROUP_WeaponSpecular; 
					bDirty = TRUE; 
				}
				else if( ( ThePackageType == PACKAGE_VEHICLE ) && ( Texture2D->LODGroup != TEXTUREGROUP_VehicleSpecular ) )
				{
					Texture2D->LODGroup = TEXTUREGROUP_VehicleSpecular; 
					bDirty = TRUE; 
				}


				// So since there are normally not that many weapons / characters in a game
				// artists are able to manually look at each one and make certain that the specular settings are correct
				//  So, if the texture is in those "package groups" do NOT update it
				if( ( ThePackageType != PACKAGE_CHARACTER ) && ( ThePackageType != PACKAGE_WEAPON ) 
					)
				{
					// all spec LODBias should be 0 as we are going to be using a group now for setting it
					if( Texture2D->LODBias != 0 )
					{
						if( Texture2D->LODBias > 2 )
						{
							Texture2D->LODBias = Texture2D->LODBias;  // just keep it for now as those will really be LOD Biased down (and that is ok)
						}
						else
						{
							Texture2D->LODBias = 0; 
						}

						bDirty = TRUE; 
					}
				}
			}
		


			// this is useful for doing a HARD reset of negative LODs in your content
// 			// So since there are normally not that many weapons / characters in a game
// 			// artists are able to manually look at each one and make certain that the specular settings are correct
// 			//  So, if the texture is in those "package groups" do NOT update it
// 			if( ( ThePackageType != PACKAGE_CHARACTER ) && ( ThePackageType != PACKAGE_WEAPON ) 
// 				)
// 			{
// 				// all all negative LODBias should be 0 as we are going to be using a group now for setting it
// 				if( Texture2D->LODBias < 0 )
// 				{
// 					Texture2D->LODBias = 0; 
// 					
// 					bDirty = TRUE; 
// 				}
// 			}


			//// NORMAL MAP
			if( 
				( ( (TextureName.ToUpper().Right(3)).InStr( TEXT("_N0" )) != INDEX_NONE )  // ut
				   || ( TextureName.ToUpper().InStr( TEXT("_N_" )) != INDEX_NONE )  // ut
				   || ( (TextureName.ToUpper().Right(2)).InStr( TEXT("_N" )) != INDEX_NONE )  // ut
				   || ( TextureName.ToUpper().InStr( TEXT("_NORMAL" )) != INDEX_NONE )
				   || ( Texture2D->CompressionSettings == TC_Normalmap )
				   || ( Texture2D->CompressionSettings == TC_NormalmapAlpha ) 
				  )
				)
			{
				bIsNormal = TRUE;

				if( ( ThePackageType == PACKAGE_WORLD ) && ( Texture2D->LODGroup != TEXTUREGROUP_WorldNormalMap ) )
				{
					Texture2D->LODGroup = TEXTUREGROUP_WorldNormalMap; 
					bDirty = TRUE; 
				}
				else if( ( ThePackageType == PACKAGE_CHARACTER ) && ( Texture2D->LODGroup != TEXTUREGROUP_CharacterNormalMap ))
				{
					Texture2D->LODGroup = TEXTUREGROUP_CharacterNormalMap; 
					bDirty = TRUE; 
				}
				else if( ( ThePackageType == PACKAGE_WEAPON ) && ( Texture2D->LODGroup != TEXTUREGROUP_WeaponNormalMap ))
				{
					Texture2D->LODGroup = TEXTUREGROUP_WeaponNormalMap; 
					bDirty = TRUE; 
				}
				else if( ( ThePackageType == PACKAGE_VEHICLE ) && ( Texture2D->LODGroup != TEXTUREGROUP_VehicleNormalMap ) )
				{
					Texture2D->LODGroup = TEXTUREGROUP_VehicleNormalMap; 
					bDirty = TRUE; 
				}


				// fix up the normal map settings
				// these can not be correctly set when you import
				if( Texture2D->SRGB == TRUE )
				{					
					warnf( TEXT( "%s: Incorrect SRGB value for a NormalMap.  Setting to FALSE" ), *TextureName );
					Texture2D->SRGB = FALSE;
				}

				if( Texture2D->UnpackMin[0] != -1.0f )
				{
					warnf( TEXT( "%s: Incorrect UnpackMin[0] value for a NormalMap.  Setting to -1.0f" ), *TextureName );
					Texture2D->UnpackMin[0] = -1.0f;
					bDirty = TRUE;
				}

				if( Texture2D->UnpackMin[1] != -1.0f )
				{
					warnf( TEXT( "%s: Incorrect UnpackMin[1] value for a NormalMap.  Setting to -1.0f" ), *TextureName );
					Texture2D->UnpackMin[1] = -1.0f;
					bDirty = TRUE;
				}

				if( Texture2D->UnpackMin[2] != -1.0f )
				{
					warnf( TEXT( "%s: Incorrect UnpackMin[2] value for a NormalMap.  Setting to -1.0f" ), *TextureName );
					Texture2D->UnpackMin[2] = -1.0f;
					bDirty = TRUE;
				}

				if( Texture2D->UnpackMin[3] != 0.0f )
				{
					warnf( TEXT( "%s: Incorrect UnpackMin[3] value for a NormalMap.  Setting to 0.0f" ), *TextureName );
					Texture2D->UnpackMin[3] = 0.0f;
					bDirty = TRUE;
				}


				// here we set Compression setting for textures which have lost their settings
				if( !( ( Texture2D->CompressionSettings == TC_Normalmap ) || ( Texture2D->CompressionSettings == TC_NormalmapAlpha ) )

					// don't try to compress the "uncompressed" normal map settings
					&& ( Texture2D->CompressionSettings != TC_Grayscale ) 
					&& ( Texture2D->CompressionSettings != TC_OneBitAlpha ) 
					&& ( Texture2D->CompressionSettings != TC_NormalmapUncompressed )
					&& ( Texture2D->CompressionSettings != TC_NormalmapBC5 ) 
					)
				{
					warnf( TEXT( "%s:  Incorrect Normalmap CompressionSetting. Now setting: TC_Normalmap (was %d)" ), *TextureName, Texture2D->CompressionSettings );
					Texture2D->CompressionSettings = TC_Normalmap;
					bDirty = TRUE;
				}


				if( bDirty == TRUE )
				{
					Texture2D->Compress();
				}
			}


			//// UI
			if( ( ThePackageType == PACKAGE_UI ) && ( Texture2D->LODGroup != TEXTUREGROUP_UI ) )
			{
				Texture2D->LODGroup = TEXTUREGROUP_UI; 
				bDirty = TRUE; 
			}

			if( ( ( PackageFileName.InStr( TEXT("EngineFonts") ) != INDEX_NONE )
				|| ( PackageFileName.InStr( TEXT("UI_Fonts") ) != INDEX_NONE )
				|| ( TextureName.InStr( TEXT("HUD") ) != INDEX_NONE )
				) 
				&& ( Texture2D->LODGroup != TEXTUREGROUP_UI )
				)
			{
				Texture2D->LODGroup = TEXTUREGROUP_UI;
				bDirty = TRUE;
			}

 
			if( ( PackageFileName.InStr( TEXT("EngineFonts") ) != INDEX_NONE )
				&& ( PackageFileName.InStr( TEXT("UI_Fonts") ) != INDEX_NONE )
				&& ( TextureName.InStr( TEXT("HUD") ) != INDEX_NONE )
				&& ( ThePackageType != PACKAGE_UI )
				) 
			{
				if( Texture2D->NeverStream == TRUE )
				{
					warnf( TEXT("%s:  Resetting NeverStream to be FALSE "), *TextureName );
					Texture2D->NeverStream = FALSE;
					bDirty = TRUE;
				}
			}





			//// SKYBOX
			if( ( ThePackageType == PACKAGE_SKYBOX ) && ( Texture2D->LODGroup != TEXTUREGROUP_Skybox ) 
				)
			{
				Texture2D->LODGroup = TEXTUREGROUP_Skybox; 
				bDirty = TRUE; 
			}

			//// check for subgroups (e.g. effects, decals, etc.)

			// we need to look for the Effects string in the Texture name as there are many cases where we do <package>.effects.<texture>  in a non Effects packages (e.g. VH_)  (subgroup is <package>.<subgroups>.<texture>
			UBOOL bIsInASubGroup = FALSE;

			// keep skybox textures that are in a sub group
			if( ( TextureName.InStr( TEXT(".Sky.") ) != INDEX_NONE ) && ( Texture2D->LODGroup == TEXTUREGROUP_Skybox ) 
				)
			{
				bIsInASubGroup = TRUE;
			}


			//// EFFECTS
			if( ( ThePackageType == PACKAGE_EFFECTS )
				|| ( TextureName.InStr( TEXT("Effects") ) != INDEX_NONE ) // check to see if it is in a  <package>.effects.<texture>
				)
			{
				// and if the texture has not been specifically set to not be filtered (e.g. need high ansio filtering on it)
				if( ( Texture2D->LODGroup != TEXTUREGROUP_Effects ) && ( Texture2D->LODGroup != TEXTUREGROUP_EffectsNotFiltered ) )
				{
					Texture2D->LODGroup = TEXTUREGROUP_Effects;
					bDirty = TRUE; 
				}

				// we are in a named subgroup (i.e. effects)
				bIsInASubGroup = TRUE;
			}



			if( ( TextureName.InStr( TEXT("LUT_") ) != INDEX_NONE ) // check to see if it is in a  <package>.LUT_.<texture>
				||  ( TextureName.InStr( TEXT("_LUT") ) != INDEX_NONE ))
			{
				if( ( Texture2D->LODGroup != TEXTUREGROUP_ColorLookupTable ) )
				{
					Texture2D->LODGroup = TEXTUREGROUP_ColorLookupTable;
					bDirty = TRUE; 
				}

				// we are in a named subgroup (i.e. effects)
				bIsInASubGroup = TRUE;
			}


			// SO now if we have not already modified the texture above then we need to
			// do one more final check to see if the texture in the package is actually
			// classified correctly
			if( ( bDirty == FALSE ) && ( bIsSpec == FALSE ) && ( bIsNormal == FALSE ) && ( bIsInASubGroup == FALSE ) )
			{
				if( ( ThePackageType == PACKAGE_CHARACTER ) && ( Texture2D->LODGroup != TEXTUREGROUP_Character ) )
				{
					Texture2D->LODGroup = TEXTUREGROUP_Character; 
					bDirty = TRUE; 
				}
				else if( ( ThePackageType == PACKAGE_WEAPON ) && ( Texture2D->LODGroup != TEXTUREGROUP_Weapon ) )
				{
					Texture2D->LODGroup = TEXTUREGROUP_Weapon; 
					bDirty = TRUE; 
				}
				else if( ( ThePackageType == PACKAGE_VEHICLE ) && ( Texture2D->LODGroup != TEXTUREGROUP_Vehicle ) )
				{
					Texture2D->LODGroup = TEXTUREGROUP_Vehicle; 
					bDirty = TRUE; 
				}
				else if( ( ThePackageType == PACKAGE_WORLD ) && ( Texture2D->LODGroup != TEXTUREGROUP_World ) )
				{
					Texture2D->LODGroup = TEXTUREGROUP_World; 
					bDirty = TRUE; 
				}
			}


			if( bDirty == TRUE )
			{
				bDirtyPackage = TRUE;
				FString OrigDescription = TEXT( "" );
				FString NewDescription = TEXT( "" );

				TArray<FString> TextureGroupNames = FTextureLODSettings::GetTextureGroupNames();
				if( OrigGroup < TextureGroupNames.Num() )
				{
					OrigDescription = TextureGroupNames(OrigGroup);
				}

				if( Texture2D->LODGroup < TextureGroupNames.Num() )
				{
					NewDescription = TextureGroupNames(Texture2D->LODGroup);
				}
				
				if( OrigLODBias != Texture2D->LODBias )
				{
					warnf( TEXT("%s:  Changing LODBias from:  %d to %d "), *TextureName, OrigLODBias, Texture2D->LODBias );
				}

				if( OrigGroup != Texture2D->LODGroup )
				{
					warnf( TEXT("%s:  Changing LODGroup from:  %d (%s) to %d (%s)"), *TextureName, OrigGroup, *OrigDescription, Texture2D->LODGroup, *NewDescription );
				}

			}

			bDirty = FALSE;
			bIsNormal = FALSE;
			bIsSpec = FALSE;
		}

#if HAVE_SCC
		if( bDirtyPackage == TRUE )
		{
			// kk now we want to possible save the package
			UBOOL bIsReadOnly = GFileManager->IsReadOnly( *PackageFileName);

			// check to see if we need to check this package out
			if( bIsReadOnly == TRUE && bAutoCheckOut == TRUE )
			{
				FSourceControl::CheckOut(Package);
			}

			bIsReadOnly = GFileManager->IsReadOnly( *PackageFileName);
			if( bIsReadOnly == FALSE )
			{
				try
				{
					if( SavePackageHelper( Package, PackageFileName ) == TRUE )
					{
						warnf( NAME_Log, TEXT("Correctly saved:  [%s]."), *PackageFileName );
					}
				}
				catch( ... )
				{
					warnf( NAME_Log, TEXT("Lame Exception %s"), *PackageFileName );
				}
			}
		}
#endif // HAVE_SCC
	}
};




/*-----------------------------------------------------------------------------
SetTextureLODGroup Commandlet
-----------------------------------------------------------------------------*/
INT USetTextureLODGroupCommandlet::Main( const FString& Params )
{
	DoActionToAllPackages<UTexture2D, SetTextureLODGroupFunctor>(this, Params);

	return 0;
}
IMPLEMENT_CLASS(USetTextureLODGroupCommandlet)


/*-----------------------------------------------------------------------------
 CompressAnimations Commandlet
-----------------------------------------------------------------------------*/

struct AddAllSkeletalMeshesToListFunctor
{
	void CleanUpGADTags(){}

	template< typename OBJECTYPE >
	void DoIt( UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches )
	{
		for( TObjectIterator<OBJECTYPE> It; It; ++It )
		{
			OBJECTYPE* SkelMesh = *It;
			SkelMesh->AddToRoot();
		}
	}
};

/**
 * 
 */
struct CompressAnimationsFunctor
{
	void CleanUpGADTags(){}

	template< typename OBJECTYPE >
	void DoIt( UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches )
	{
		FLOAT LastSaveTime = appSeconds();
		UBOOL bDirtyPackage = FALSE;
		const FName& PackageName = Package->GetFName(); 
		FString PackageFileName;
		GPackageFileCache->FindPackageFile( *PackageName.ToString(), NULL, PackageFileName );

#if HAVE_SCC
		// Ensure source control is initialized and shut down properly
		FScopedSourceControl SourceControl;
#endif
		const UBOOL bSkipCinematicPackages = Switches.ContainsItem(TEXT("SKIPCINES"));
		const UBOOL bSkipLongAnimations = Switches.ContainsItem(TEXT("SKIPLONGANIMS"));
		// Reset compression, don't do incremental compression, start from scratch
		const UBOOL bResetCompression = Switches.ContainsItem(TEXT("RESETCOMPRESSION"));
		/** Clear bDoNotOverrideCompression flag in animations */
		const UBOOL bClearNoCompressionOverride = Switches.ContainsItem(TEXT("CLEARNOCOMPRESSIONOVERRIDE"));
		/** If we're analyzing, we're not actually going to recompress, so we can skip some significant work. */
		const UBOOL bAnalyze = Switches.ContainsItem(TEXT("ANALYZE"));
		// See if we can save this package. If we can't, don't bother...
		/** if we should auto checkout packages that need to be saved **/
		const UBOOL bAutoCheckOut = Switches.ContainsItem(TEXT("AUTOCHECKOUTPACKAGES"));
		// see if we should skip read only packages.
		UBOOL bIsReadOnly = GFileManager->IsReadOnly( *PackageFileName);

		// check to see if we need to check this package out
		if( !bAnalyze && bIsReadOnly )
		{
			// Read only, see if we can check it out
			if (bAutoCheckOut == TRUE)
			{
#if HAVE_SCC
				INT PackageStatus = FSourceControl::ForceGetStatus(*PackageFileName);
				// Checked out by other.. fail :(
				if( PackageStatus == SCC_CheckedOutOther )
				{
					warnf(TEXT("Package (%s) checked out by other, skipping."), *PackageFileName);
					PackagesThatCouldNotBeSavedList.AddItem( PackageFileName );
					return;
				}
				// Package not at head revision
				else if ( PackageStatus == SCC_NotCurrent )
				{
					warnf( NAME_Log, TEXT("Package (%s) is not at head revision, skipping."), *PackageFileName );
					PackagesThatCouldNotBeSavedList.AddItem( PackageFileName );
					return;
				}
#else
				warnf(TEXT("Package (%s) is read only, SCC not enabled. Skip"), *PackageFileName);
				PackagesThatCouldNotBeSavedList.AddUniqueItem( PackageFileName );
				return;
#endif
			}
			// not allowed to auto check out :(
			else
			{
				warnf(TEXT("Package (%s) is read only. Switch AUTOCHECKOUTPACKAGES not set. Skip."), *PackageFileName);
				PackagesThatCouldNotBeSavedList.AddUniqueItem( PackageFileName );
				return;
			}
		}

		if (bSkipCinematicPackages && (PackageFileName.InStr(TEXT("CINE"), FALSE, TRUE) != INDEX_NONE))
		{
			warnf(TEXT("Package (%s) name contains 'cine' and switch SKIPCINES is set. Skip."), *PackageFileName);
			PackagesThatCouldNotBeSavedList.AddUniqueItem( PackageFileName );
			return;
		}

		// Get version number. Bump this up every time you want to recompress all animations.
		INT CompressCommandletVersion = 0;
		GConfig->GetInt( TEXT("AnimationCompression"), TEXT("CompressCommandletVersion"), (INT&)CompressCommandletVersion, GEngineIni );


		// Count the number of animations to provide some limited progress indication
		INT NumAnimationsInPackage = 0;
		for (TObjectIterator<OBJECTYPE> It; It; ++It)
		{
			++NumAnimationsInPackage;
		}


		INT ActiveAnimationIndex = 0;
		for (TObjectIterator<OBJECTYPE> It; It; ++It)
		{
			OBJECTYPE* AnimSeq = *It;
			++ActiveAnimationIndex;

			if (!AnimSeq->IsIn(Package))
			{
				continue;
			}

			UAnimSet* AnimSet = AnimSeq->GetAnimSet();
			check(AnimSet != NULL);

			if (bSkipCinematicPackages && (AnimSet->GetName().InStr(TEXT("CINE"), FALSE, TRUE) != INDEX_NONE))
			{
				warnf(TEXT("AnimSet (%s) name contains 'cine' and switch SKIPCINES is set. Skip."), *AnimSet->GetName());
				continue;
			}

			// If animation hasn't been compressed, force it.
			UBOOL bForceCompression = (AnimSeq->CompressedTrackOffsets.Num() == 0);

			// If animation has already been compressed with the commandlet and version is the same. then skip.
			// We're only interested in new animations.
			if( !bAnalyze && !bForceCompression && AnimSeq->CompressCommandletVersion == CompressCommandletVersion )
			{
				warnf(TEXT("Same CompressCommandletVersion (%i) skip animation: %s (%s)"), CompressCommandletVersion, *AnimSeq->SequenceName.ToString(), *AnimSet->GetFullName());
				continue;
			}

			if( !bAnalyze && !bForceCompression && bSkipLongAnimations && (AnimSeq->NumFrames > 300) )
			{
				warnf(TEXT("Animation (%s) has more than 300 frames (%i frames) and SKIPLONGANIMS switch is set. Skipping."), *AnimSeq->SequenceName.ToString(), AnimSeq->NumFrames);
				continue;
			}

			if( bAnalyze )
			{
				static INT NumTotalAnimations = 0;
				static INT NumTotalSize = 0;
				static INT Trans96Savings = 0;
				static INT Trans48Savings = 0;
				static INT Rot96Savings = 0;
				static INT Rot48Savings = 0;
				static INT Num96TransTracks = 0;
				static INT Num96RotTracks = 0;
				static INT Num48TransTracks = 0;
				static INT Num48RotTracks = 0;
				static INT Num32TransTracks = 0;
				static INT UnknownTransTrack = 0;
				static INT UnknownRotTrack = 0;
				static INT RotationOnlySavings = 0;
				static INT RotationOnlyManyKeys = 0;

				NumTotalAnimations++;

				FArchiveCountMem CountBytesSize( AnimSeq );
				INT ResourceSize = CountBytesSize.GetNum();

				NumTotalSize += ResourceSize;
				
				// Looking for PerTrackCompression using 96bit translation compression.
				if( AnimSeq->KeyEncodingFormat == AKF_PerTrackCompression && AnimSeq->CompressedByteStream.Num() > 0 )
				{
					UBOOL bCandidate = FALSE;

					for(INT i=0; i<AnimSet->TrackBoneNames.Num(); i++)
 					{
 						const INT TrackIndex = i;

 						// Translation
						{
							// Use the CompressedTrackOffsets stream to find the data addresses
 							const INT* RESTRICT TrackData = AnimSeq->CompressedTrackOffsets.GetTypedData() + (TrackIndex * 2);
 							const INT TransKeysOffset = TrackData[0];
 							if( TransKeysOffset != INDEX_NONE )
 							{
 								const BYTE* RESTRICT TrackData = AnimSeq->CompressedByteStream.GetTypedData() + TransKeysOffset + 4;
 								const INT Header = *((INT*)(AnimSeq->CompressedByteStream.GetTypedData() + TransKeysOffset));
	 
 								INT KeyFormat;
 								INT NumKeys;
 								INT FormatFlags;
 								INT BytesPerKey;
 								INT FixedBytes;
 								FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags, /*OUT*/BytesPerKey, /*OUT*/ FixedBytes);
 								if( KeyFormat == ACF_Float96NoW )
 								{
									Num96TransTracks++;

 									// Determine which components we could let go, and bytes we could save.
									const FBox KeyBounds((FVector*)(TrackData + FixedBytes), NumKeys);
									const UBOOL bHasX = (Abs(KeyBounds.Max.X) >= 0.0002f) || (Abs(KeyBounds.Min.X) >= 0.0002f);
									const UBOOL bHasY = (Abs(KeyBounds.Max.Y) >= 0.0002f) || (Abs(KeyBounds.Min.Y) >= 0.0002f);
									const UBOOL bHasZ = (Abs(KeyBounds.Max.Z) >= 0.0002f) || (Abs(KeyBounds.Min.Z) >= 0.0002f);

									if( !bHasX )
									{
										Trans96Savings += (4 * NumKeys);
										bCandidate = TRUE;
									}
									if( !bHasY )
									{
										Trans96Savings += (4 * NumKeys);
										bCandidate = TRUE;
									}
									if( !bHasZ )
									{
										Trans96Savings += (4 * NumKeys);
										bCandidate = TRUE;
									}
								}
								// Measure savings on 48bits translations
								else if( KeyFormat == ACF_Fixed48NoW )
								{
									Num48TransTracks++;

									const INT SavedBytes = (6 - BytesPerKey) * NumKeys;
									if( SavedBytes > 0 )
									{
										bCandidate = TRUE;
										Trans48Savings += SavedBytes;
									}
								}
								else if( KeyFormat == ACF_IntervalFixed32NoW )
								{
									Num32TransTracks++;
								}
								else
								{
									UnknownTransTrack++;
								}

								// Measure how much we'd save if we used "rotation only" for compression
								FName const BoneName = AnimSet->TrackBoneNames(TrackIndex);
								USkeletalMesh* DefaultSkeletalMesh = LoadObject<USkeletalMesh>(NULL, *AnimSet->PreviewSkelMeshName.ToString(), NULL, LOAD_None, NULL);
								INT const BoneIndex = DefaultSkeletalMesh->MatchRefBone( BoneName );

								if( BoneIndex > 0 
									&& ((AnimSet->UseTranslationBoneNames.Num() > 0 && AnimSet->UseTranslationBoneNames.FindItemIndex(BoneName) == INDEX_NONE) 
										|| (AnimSet->ForceMeshTranslationBoneNames.FindItemIndex(BoneName) != INDEX_NONE)) 
									)
								{
									RotationOnlySavings += (BytesPerKey * NumKeys);
									if( NumKeys > 1 )
									{
										const BYTE* RESTRICT KeyData0 = TrackData + FixedBytes;
										FVector V0;
										FAnimationCompression_PerTrackUtils::DecompressTranslation(KeyFormat, FormatFlags, V0, TrackData, KeyData0);

										FLOAT MaxErrorFromFirst = 0.f;
										FLOAT MaxErrorFromDefault = 0.f;
										for(INT KeyIdx=0; KeyIdx<NumKeys; KeyIdx++)
										{
											const BYTE* RESTRICT KeyDataN = TrackData + FixedBytes + KeyIdx * BytesPerKey;
											FVector VN;
											FAnimationCompression_PerTrackUtils::DecompressTranslation(KeyFormat, FormatFlags, VN, TrackData, KeyDataN);

											
											MaxErrorFromDefault = ::Max(MaxErrorFromDefault, Abs(VN.X - DefaultSkeletalMesh->RefSkeleton(BoneIndex).BonePos.Position.X));
											MaxErrorFromDefault = ::Max(MaxErrorFromDefault, Abs(VN.Y - DefaultSkeletalMesh->RefSkeleton(BoneIndex).BonePos.Position.Y));
											MaxErrorFromDefault = ::Max(MaxErrorFromDefault, Abs(VN.Z - DefaultSkeletalMesh->RefSkeleton(BoneIndex).BonePos.Position.Z));

											MaxErrorFromFirst = ::Max(MaxErrorFromFirst, Abs(VN.X - V0.X));
											MaxErrorFromFirst = ::Max(MaxErrorFromFirst, Abs(VN.Y - V0.Y));
											MaxErrorFromFirst = ::Max(MaxErrorFromFirst, Abs(VN.Z - V0.Z));
										}

										warnf(TEXT("RotationOnly translation track that is animated! %s, %s (%s) NumKeys: %i, MaxErrorFromDefault: %f, MaxErrorFromFirst: %f"), 
											*BoneName.ToString(), *AnimSeq->SequenceName.ToString(), *AnimSet->GetFullName(), NumKeys, MaxErrorFromDefault, MaxErrorFromFirst);
										RotationOnlyManyKeys += (BytesPerKey * (NumKeys-1));
									}
								}
 							}
						}

						// Rotation
						{
							// Use the CompressedTrackOffsets stream to find the data addresses
							const INT* RESTRICT TrackData = AnimSeq->CompressedTrackOffsets.GetTypedData() + (TrackIndex * 2);
							const INT RotKeysOffset = TrackData[1];
							if( RotKeysOffset != INDEX_NONE )
							{
								const BYTE* RESTRICT TrackData = AnimSeq->CompressedByteStream.GetTypedData() + RotKeysOffset + 4;
								const INT Header = *((INT*)(AnimSeq->CompressedByteStream.GetTypedData() + RotKeysOffset));

								INT KeyFormat;
								INT NumKeys;
								INT FormatFlags;
								INT BytesPerKey;
								INT FixedBytes;
								FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags, /*OUT*/BytesPerKey, /*OUT*/ FixedBytes);
								if( KeyFormat == ACF_Float96NoW )
								{
									Num96RotTracks++;

									// Determine which components we could let go, and bytes we could save.
									const FBox KeyBounds((FVector*)(TrackData + FixedBytes), NumKeys);
									const UBOOL bHasX = (Abs(KeyBounds.Max.X) >= 0.0002f) || (Abs(KeyBounds.Min.X) >= 0.0002f);
									const UBOOL bHasY = (Abs(KeyBounds.Max.Y) >= 0.0002f) || (Abs(KeyBounds.Min.Y) >= 0.0002f);
									const UBOOL bHasZ = (Abs(KeyBounds.Max.Z) >= 0.0002f) || (Abs(KeyBounds.Min.Z) >= 0.0002f);

									if( !bHasX )
									{
										Rot96Savings += (4 * NumKeys);
										bCandidate = TRUE;
									}
									if( !bHasY )
									{
										Rot96Savings += (4 * NumKeys);
										bCandidate = TRUE;
									}
									if( !bHasZ )
									{
										Rot96Savings += (4 * NumKeys);
										bCandidate = TRUE;
									}
								}
								// Measure savings on 48bits rotations.
								else if( KeyFormat == ACF_Fixed48NoW )
								{
									Num48RotTracks++;

									const INT SavedBytes = (6 - BytesPerKey) * NumKeys;
									if( SavedBytes > 0 )
									{
										bCandidate = TRUE;
										Rot48Savings += SavedBytes;
									}
								}
								else
								{
									UnknownRotTrack++;
								}
							}
						}
 					}

					if( bCandidate )
					{
						++AnalyzeCompressionCandidates;
						warnf(TEXT("[%i] Animation could be recompressed: %s (%s), Trans96Savings: %i, Rot96Savings: %i, Trans48Savings: %i, Rot48Savings: %i, RotationOnlySavings: %i, RotationOnlyManyKeys: %i (bytes)"), 
							AnalyzeCompressionCandidates, *AnimSeq->SequenceName.ToString(), *AnimSet->GetFullName(), Trans96Savings, Rot96Savings, Trans48Savings, Rot48Savings, RotationOnlySavings, RotationOnlyManyKeys);
						warnf(TEXT("Translation Track Count, Num96TransTracks: %i, Num48TransTracks: %i, Num32TransTracks: %i, UnknownTransTrack: %i"), 
							Num96TransTracks, Num48TransTracks, Num32TransTracks, UnknownTransTrack);
						warnf(TEXT("Rotation Track Count, Num96RotTracks: %i, Num48RotTracks: %i, UnknownRotTrack: %i"), 
							Num96RotTracks, Num48RotTracks, UnknownRotTrack);
					}
				}

// 				if( AnimSeq->NumFrames > 1 && AnimSeq->KeyEncodingFormat != AKF_PerTrackCompression )
// 				{
// 					++AnalyzeCompressionCandidates;
// 
// 					FArchiveCountMem CountBytesSize( AnimSeq );
// 					INT ResourceSize = CountBytesSize.GetNum();
// 
// 					warnf(TEXT("[%i] Animation could be recompressed: %s (%s), frames: %i, length: %f, size: %i bytes, compression scheme: %s"), 
// 						AnalyzeCompressionCandidates, *AnimSeq->SequenceName.ToString(), *AnimSet->GetFullName(),  AnimSeq->NumFrames, AnimSeq->SequenceLength, ResourceSize, AnimSeq->CompressionScheme ? *AnimSeq->CompressionScheme->GetClass()->GetName() : TEXT("NULL"));
// 				}

				continue;
			}

			FLOAT HighestRatio = 0.f;
			USkeletalMesh*	BestSkeletalMeshMatch = NULL;

			// Test preview skeletal mesh
			USkeletalMesh* DefaultSkeletalMesh = LoadObject<USkeletalMesh>(NULL, *AnimSet->PreviewSkelMeshName.ToString(), NULL, LOAD_None, NULL);
			FLOAT DefaultMatchRatio = 0.f;
			if( DefaultSkeletalMesh )
			{
				DefaultMatchRatio = AnimSet->GetSkeletalMeshMatchRatio(DefaultSkeletalMesh);
			}

			// If our default mesh doesn't have a full match ratio, then see if we can find a better fit.
			if( DefaultMatchRatio < 1.f )
			{
				// Find the most suitable SkeletalMesh for this AnimSet
				for( TObjectIterator<USkeletalMesh> ItMesh; ItMesh; ++ItMesh )
				{
					USkeletalMesh* SkelMeshCandidate = *ItMesh;
					if( SkelMeshCandidate != DefaultSkeletalMesh )
					{
						FLOAT MatchRatio = AnimSet->GetSkeletalMeshMatchRatio(SkelMeshCandidate);
						if( MatchRatio > HighestRatio )
						{
							BestSkeletalMeshMatch = SkelMeshCandidate;
							HighestRatio = MatchRatio;

							// If we have found a perfect match, we can abort.
							if( Abs(1.f - MatchRatio) <= KINDA_SMALL_NUMBER )
							{
								break;
							}
						}
					}
				}

				// If we have found a best match
				if( BestSkeletalMeshMatch )
				{
					// if it is different than our preview mesh and his match ratio is higher
					// then replace preview mesh with this one, as it's a better match.
					if( BestSkeletalMeshMatch != DefaultSkeletalMesh && HighestRatio > DefaultMatchRatio )
					{
						warnf(TEXT("Found more suitable preview mesh for %s (%s): %s (%f) instead of %s (%f)."), 
							*AnimSeq->SequenceName.ToString(), *AnimSet->GetFullName(), *BestSkeletalMeshMatch->GetFName().ToString(), HighestRatio, *AnimSet->PreviewSkelMeshName.ToString(), DefaultMatchRatio);

						// We'll now use this one from now on as it's a better fit.
						AnimSet->PreviewSkelMeshName = FName( *BestSkeletalMeshMatch->GetPathName() );
						AnimSet->MarkPackageDirty();

						DefaultSkeletalMesh = BestSkeletalMeshMatch;
						bDirtyPackage = TRUE;
					}
				}
				else
				{
					warnf(TEXT("Could not find suitable mesh for %s (%s) !!! Default was %s"), 
							*AnimSeq->SequenceName.ToString(), *AnimSet->GetFullName(), *AnimSet->PreviewSkelMeshName.ToString());
				}
			}

			INT OldSize;
			INT NewSize;

			{
				FArchiveCountMem CountBytesSize( AnimSeq );
				OldSize = CountBytesSize.GetNum();
			}

			// Clear bDoNotOverrideCompression flag
			if( bClearNoCompressionOverride && AnimSeq->bDoNotOverrideCompression )
			{
				AnimSeq->bDoNotOverrideCompression = FALSE;
				bDirtyPackage = TRUE;
			}

			// Reset to default compressor
			if( bResetCompression )
			{
				warnf(TEXT("%s (%s) Resetting with BitwiseCompressOnly."), *AnimSeq->SequenceName.ToString(), *AnimSet->GetFullName());
				UAnimationCompressionAlgorithm* CompressionAlgorithm = ConstructObject<UAnimationCompressionAlgorithm_BitwiseCompressOnly>( UAnimationCompressionAlgorithm_BitwiseCompressOnly::StaticClass() );
				CompressionAlgorithm->RotationCompressionFormat = ACF_Float96NoW;
				CompressionAlgorithm->TranslationCompressionFormat = ACF_None;
				CompressionAlgorithm->Reduce(AnimSeq, NULL, FALSE);

				// Force an update.
				AnimSeq->CompressCommandletVersion = 0;
			}

			warnf(TEXT("Compressing animation '%s' [#%d / %d in package '%s']"),
				*AnimSeq->SequenceName.ToString(),
				ActiveAnimationIndex,
				NumAnimationsInPackage,
				*PackageFileName);

			FAnimationUtils::CompressAnimSequence(AnimSeq, DefaultSkeletalMesh, TRUE, FALSE);
			{
				FArchiveCountMem CountBytesSize( AnimSeq );
				NewSize = CountBytesSize.GetNum();
			}

			// Set version since we've checked this animation for recompression.
			if( AnimSeq->CompressCommandletVersion != CompressCommandletVersion )
			{
				AnimSeq->CompressCommandletVersion = CompressCommandletVersion;
				bDirtyPackage = TRUE;
			}

			// Only save package if size has changed.
			bDirtyPackage = (bDirtyPackage || bForceCompression || (OldSize != NewSize));

			// if Dirty, then we need to be able to write to this package. 
			// If we can't, abort, don't want to waste time!!
			if( bDirtyPackage )
			{
				// Save dirty package every 10 minutes at least, to avoid losing work in case of a crash on very large packages.
				FLOAT const CurrentTime = appSeconds();
				warnf( NAME_Log, TEXT("Time since last save: %f seconds"), (CurrentTime - LastSaveTime) );
				if( (CurrentTime - LastSaveTime) > 10.f * 60.f )
				{
					warnf( NAME_Log, TEXT("It's been over 10 minutes (%f seconds), try to save package."), (CurrentTime - LastSaveTime) );
					UBOOL bCorrectlySaved = FALSE;

					UBOOL bIsReadOnly = GFileManager->IsReadOnly( *PackageFileName);
					if( bIsReadOnly && bAutoCheckOut )
					{
#if HAVE_SCC
						FSourceControl::CheckOut(Package);
#endif // HAVE_SCC
					}

					bIsReadOnly = GFileManager->IsReadOnly( *PackageFileName);
					if( bIsReadOnly == FALSE )
					{
						try
						{
							if( SavePackageHelper( Package, PackageFileName ) == TRUE )
							{
								bCorrectlySaved = TRUE;
								warnf( NAME_Log, TEXT("Correctly saved:  [%s]."), *PackageFileName );
							}
						}
						catch( ... )
						{
							warnf( NAME_Log, TEXT("Lame Exception %s"), *PackageFileName );
						}
					}

					// Log which packages could not be saved
					if( !bCorrectlySaved )
					{
						PackagesThatCouldNotBeSavedList.AddUniqueItem( PackageFileName );
						warnf( NAME_Log, TEXT("%s couldn't be saved, so abort this package, don't waste time on it."), *PackageFileName );
						// Abort!
						return;
					}

					// Correctly saved
					LastSaveTime = CurrentTime;
					bDirtyPackage = FALSE;
				}
			}
		}

		// End of recompression
		// Does package need to be saved?
		bDirtyPackage = bDirtyPackage || Package->IsDirty();

		// If we need to save package, do so.
		if( bDirtyPackage && !bAnalyze )
		{
			UBOOL bCorrectlySaved = FALSE;

			/** if we should auto checkout packages that need to be saved**/
			const UBOOL bAutoCheckOut = Switches.ContainsItem(TEXT("AUTOCHECKOUTPACKAGES"));
			// see if we should skip read only packages.
			UBOOL bIsReadOnly = GFileManager->IsReadOnly( *PackageFileName);

			// check to see if we need to check this package out
			if( bIsReadOnly == TRUE && bAutoCheckOut == TRUE )
			{
#if HAVE_SCC
				FSourceControl::CheckOut(Package);
#endif // HAVE_SCC
			}

			bIsReadOnly = GFileManager->IsReadOnly( *PackageFileName);
			if( bIsReadOnly == FALSE )
			{
				try
				{
					if( SavePackageHelper( Package, PackageFileName ) == TRUE )
					{
						bCorrectlySaved = TRUE;
						warnf( NAME_Log, TEXT("Correctly saved:  [%s]."), *PackageFileName );
					}
				}
				catch( ... )
				{
					warnf( NAME_Log, TEXT("Lame Exception %s"), *PackageFileName );
				}
			}

			// Log which packages could not be saved
			if( !bCorrectlySaved )
			{
				PackagesThatCouldNotBeSavedList.AddUniqueItem( PackageFileName );
			}
		}
	}
};

static INT AnalyzeCompressionCandidates = 0;
static TArray<FString> PackagesThatCouldNotBeSavedList;

INT UCompressAnimationsCommandlet::Main( const FString& Params )
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	// want everything in upper case, it's a mess otherwise
	const FString ParamsUpperCase = Params.ToUpper() + FString(TEXT(" -SKIPSCRIPTPACKAGES"));
	const TCHAR* Parms = *ParamsUpperCase;
	UCommandlet::ParseCommandLine(Parms, Tokens, Switches);

	/** If we're analyzing, we're not actually going to recompress, so we can skip some significant work. */
	UBOOL bAnalyze = Switches.ContainsItem(TEXT("ANALYZE"));

	if (bAnalyze)
	{
		warnf(TEXT("Analyzing content for uncompressed animations..."));
		DoActionToAllPackages<UAnimSequence, CompressAnimationsFunctor>(this, ParamsUpperCase);

		warnf(TEXT("Done analyzing. Potential canditates: %i"), AnalyzeCompressionCandidates);
	}
	else
	{
		// First scan all Skeletal Meshes
		warnf(TEXT("Scanning for all SkeletalMeshes..."));

		// If we have SKIPREADONLY, then override this, as we need to scan all packages for skeletal meshes.
		FString SearchAllMeshesParams = ParamsUpperCase;
		SearchAllMeshesParams += FString(TEXT(" -OVERRIDEREADONLY"));
		SearchAllMeshesParams += FString(TEXT(" -OVERRIDELOADMAPS"));
		// Prevent recompression here, we'll do it after we gathered all skeletal meshes
		GDisableAnimationRecompression = TRUE;
		DoActionToAllPackages<USkeletalMesh, AddAllSkeletalMeshesToListFunctor>(this, SearchAllMeshesParams);
		GDisableAnimationRecompression = FALSE;

		INT Count = 0;
		for( TObjectIterator<USkeletalMesh> It; It; ++It )
		{
			USkeletalMesh* SkelMesh = *It;
			warnf(TEXT("[%i] %s"), Count, *SkelMesh->GetFName().ToString());
			Count++;
		}
		warnf(TEXT("%i SkeletalMeshes found!"), Count);

		// Then do the animation recompression
		warnf(TEXT("Recompressing all animations..."));
		DoActionToAllPackages<UAnimSequence, CompressAnimationsFunctor>(this, ParamsUpperCase);

		warnf(TEXT("\n*** Packages that could not be recompressed: %i"), PackagesThatCouldNotBeSavedList.Num());
		for(INT i=0; i<PackagesThatCouldNotBeSavedList.Num(); i++)
		{
			warnf(TEXT("\t%s"), *PackagesThatCouldNotBeSavedList(i));
		}
	}
	

	return 0;
}
IMPLEMENT_CLASS(UCompressAnimationsCommandlet)


/*-----------------------------------------------------------------------------
FixAdditiveReferencesCommandlet
-----------------------------------------------------------------------------*/

static INT FAR_EmptyReference = 0;
static INT FAR_OneWayReference = 0;

static INT FAR_RecoveredAnimSeqOrphan = 0;
static INT FAR_NumAdditiveAnimations = 0;
static INT FAR_MissingDefaultSkelMesh = 0;
static INT FAR_SetAnimFailed = 0;
static INT FAR_FoundTargetPose = 0;
static INT FAR_FoundBasePose = 0;
static INT FAR_BuiltFromBindPose = 0;
static INT FAR_ReferenceInDifferentPackage = 0;

static TArray<FName> FAR_MisnamedAdditives;
static TArray<FName> FAR_MissingTargetPoseList;
static TArray<FName> FAR_MissingBasePoseList;
static TArray<FName> FAR_UsingBindPoseList;
static TArray<FName> FAR_PackagesNotSaved;

#define MAXPOSDIFF		(0.0001f)
#define MAXANGLEDIFF	(0.0003f)

/**
 * 
 */
struct FixAdditiveReferencesFunctor
{
	USkeletalMeshComponent *SkelCompA, *SkelCompB;
	UAnimNodeSequence *AnimNodeSeqA, *AnimNodeSeqB;
	USkeletalMesh *DefaultSkeletalMesh;

	UBOOL bAnalyze, bVerbose, bMarkPackageDirty;

	void CleanUpGADTags(){}

	template< typename OBJECTYPE >
	void DoIt( UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches )
	{
		// If we're analyzing, we don't actually do anything, just provide stats.
		bAnalyze = Switches.ContainsItem(TEXT("ANALYZE"));
		// Verbose - Log everything we're doing.
		bVerbose = Switches.ContainsItem(TEXT("VERBOSE"));

		// Keep track if we make changes, and need to checkout/save package.
		bMarkPackageDirty = FALSE;

		const FName& PackageName = Package->GetFName(); 
		FString PackageFileName;
		GPackageFileCache->FindPackageFile( *PackageName.ToString(), NULL, PackageFileName );

		/** if we should auto checkout packages that need to be saved**/
		const UBOOL bAutoCheckOut = ParseParam(appCmdLine(),TEXT("AutoCheckOutPackages"));

#if HAVE_SCC
		// Ensure source control is initialized and shut down properly
		FScopedSourceControl SourceControl;

		if ( bAutoCheckOut && FSourceControl::ForceGetStatus( PackageFileName ) == SCC_NotCurrent )
		{
			warnf( NAME_Log, TEXT("Skipping %s (Not at head source control revision)"), *PackageFileName );
			FAR_PackagesNotSaved.AddUniqueItem( FName(*PackageFileName) );
			return;
		}
#endif

		AnimNodeSeqA = ConstructObject<UAnimNodeSequence>(UAnimNodeSequence::StaticClass());
		AnimNodeSeqB = ConstructObject<UAnimNodeSequence>(UAnimNodeSequence::StaticClass());

		AnimNodeSeqA->bEditorOnlyAddRefPoseToAdditiveAnimation = TRUE;
		AnimNodeSeqB->bEditorOnlyAddRefPoseToAdditiveAnimation = TRUE;

		SkelCompA = ConstructObject<UASVSkelComponent>(UASVSkelComponent::StaticClass());
		SkelCompA->Animations = AnimNodeSeqA;
		SkelCompA->bUseRawData = TRUE;
		AnimNodeSeqA->SkelComponent = SkelCompA;

		SkelCompB = ConstructObject<UASVSkelComponent>(UASVSkelComponent::StaticClass());
		SkelCompB->Animations = AnimNodeSeqB;
		SkelCompB->bUseRawData = TRUE;
		AnimNodeSeqB->SkelComponent = SkelCompB;

		// Check that AnimSequence belongs to AnimSet
		for( TObjectIterator<OBJECTYPE> It; It; ++It )
		{
			OBJECTYPE* Object = *It;
			if( !Object->IsIn(Package) )
			{
				continue;
			}

			// Cast so we can access member functions in intellisense...
			UAnimSequence* AnimSeq = Cast<UAnimSequence>(Object);

			// Verify that AnimSequence/AnimSet relationship is maintained.
			UAnimSet* AnimSet = AnimSeq->GetAnimSet();
			if( AnimSet->FindAnimSequence(AnimSeq->SequenceName) == NULL )
			{
				warnf(TEXT("AnimSequence %s references AnimSet: %s but does not belong to it!!"), *AnimSeq->SequenceName.ToString(), *AnimSet->GetFName().ToString());
				// See if we can find an AnimSet that actually contains that sequence...
				UBOOL bFound = FALSE;
				for( TObjectIterator<UAnimSet> ItAnimSet; ItAnimSet; ++ItAnimSet )
				{
					UAnimSet* ObjectAnimSet = *ItAnimSet;
					if( !ObjectAnimSet->IsIn(Package) )
					{
						continue;
					}

					if( ObjectAnimSet->Sequences.FindItemIndex(AnimSeq) != INDEX_NONE )
					{
						bFound = TRUE;
						warnf(TEXT("\nFound this AnimSet that contains it!!"), *ObjectAnimSet->GetFName().ToString());
						FAR_RecoveredAnimSeqOrphan++;
						if( !bAnalyze )
						{
							// @todolaurent - fixup?
						}
						break;
					}
				}

				if( !bFound )
				{
					warnf(TEXT("\tThis AnimSequence does not belong to any AnimSet... :("));
					if( !bAnalyze )
					{
						// @todolaurent - Dispose of those animations properly.
					}
				}
			}
		}

		// Go through all AnimSequences in that package
		for( TObjectIterator<OBJECTYPE> It; It; ++It )
		{
			OBJECTYPE* Object = *It;
			if( !Object->IsIn(Package) || Object->GetAnimSet()->Sequences.FindItemIndex(Object) == INDEX_NONE )
			{
				continue;
			}
			
			// Cast so we can access member functions in intellisense...
			UAnimSequence* AnimSeq = Cast<UAnimSequence>(Object);

			// Skip non additive animations.
			if( !AnimSeq->bIsAdditive )
			{
				continue;
			}

			FAR_NumAdditiveAnimations++;
			warnf(TEXT("Looking at %s in AnimSet: %s"), *AnimSeq->SequenceName.ToString(), *AnimSeq->GetAnimSet()->GetFName().ToString());

			// Try to find a SkeletalMesh to compare animation data. 
			// We need this to be able to compare tracks properly. Since AnimSets have not idea of structure and hierarchy.
			DefaultSkeletalMesh = NULL;
			if( AnimSeq->GetAnimSet()->PreviewSkelMeshName != NAME_None )
			{
				DefaultSkeletalMesh = LoadObject<USkeletalMesh>(NULL, *AnimSeq->GetAnimSet()->PreviewSkelMeshName.ToString(), NULL, LOAD_None, NULL);
				if( DefaultSkeletalMesh )
				{
					SkelCompA->SetSkeletalMesh( DefaultSkeletalMesh );
					SkelCompB->SetSkeletalMesh( DefaultSkeletalMesh );
				}
			}
			// If we couldn't find a skeletal mesh, fail :(
			if( DefaultSkeletalMesh == NULL )
			{
				FAR_MissingDefaultSkelMesh++;
				warnf(TEXT("\t\tCouldn't find default skeletalmesh named %s"), *AnimSeq->GetAnimSet()->PreviewSkelMeshName.ToString());
				continue;
			}

			// Look for Target Poses.
 			ScanForTargetPoses(AnimSeq, Package);

			// Look for Base Poses.
			ScanForBasePoses(AnimSeq, Package);
		
			// Verify Base Poses.
			VerifyBasePoses(AnimSeq, Package);

			// Verify Target Poses.
			VerifyTargetPoses(AnimSeq, Package);
		}

#if HAVE_SCC
		if( bMarkPackageDirty && !bAnalyze )
		{
			// kk now we want to possible save the package
			UBOOL bIsReadOnly = GFileManager->IsReadOnly( *PackageFileName);

			// check to see if we need to check this package out
			if( bIsReadOnly == TRUE && bAutoCheckOut == TRUE )
			{
				warnf(TEXT("\tAttempting to check out package %s"), *PackageFileName );
				FSourceControl::CheckOut(Package);
			}

			bIsReadOnly = GFileManager->IsReadOnly( *PackageFileName);
			if( bIsReadOnly == FALSE )
			{
				try
				{
					if( SavePackageHelper( Package, PackageFileName ) == TRUE )
					{
						warnf( NAME_Log, TEXT("\tCorrectly saved:  [%s]."), *PackageFileName );
					}
					else
					{
						FAR_PackagesNotSaved.AddUniqueItem( FName(*PackageFileName) );
					}
				}
				catch( ... )
				{
					warnf( NAME_Log, TEXT("\tLame Exception %s"), *PackageFileName );
					FAR_PackagesNotSaved.AddUniqueItem( FName(*PackageFileName) );
				}
			}
			else
			{
				warnf(TEXT("\tCouldn't save package %s, Read Only. :("), *PackageFileName );
				FAR_PackagesNotSaved.AddUniqueItem( FName(*PackageFileName) );
			}
		}
#endif // HAVE_SCC

	}

	void ScanForTargetPoses(UAnimSequence* AdditiveAnimSeq, UPackage* Package)
	{
		// Look at animations in current package which could be the original Target pose for this additive
		// We make the assumption that the animation was named ADD_<OriginalTargetName>. That makes our life easier.
		FString AdditiveString = AdditiveAnimSeq->SequenceName.ToString();
		INT const Position = AdditiveString.InStr(TEXT("ADD_"));
		if( Position == INDEX_NONE )
		{
			warnf(TEXT("\tName is not starting with ADD_, so we can't find the original Target Pose. :("));
			FString LogString = AdditiveAnimSeq->GetAnimSet()->GetFName().ToString() + FString(TEXT(".")) + AdditiveAnimSeq->SequenceName.ToString();
			FAR_MisnamedAdditives.AddItem( FName(*LogString) );
			return;
		}
		else
		{
			FName TargetPoseName = FName( *AdditiveString.Mid(Position + 4, AdditiveString.Len()) );
			warnf(TEXT("\tLooking for Target Pose(s) named %s in current package..."), *TargetPoseName.ToString());
			UBOOL bFoundAtLeastOne = FALSE;
			for( TObjectIterator<UAnimSequence> ItTarget; ItTarget; ++ItTarget )
			{
				UAnimSequence* TargetPoseSeq = *ItTarget;

				if( !TargetPoseSeq->IsIn(Package) || TargetPoseSeq == AdditiveAnimSeq 
					// A few assumptions to reduce the list of candidates:
					// * Target Pose cannot be an additive animation.
					// * Target Pose has to be: ADD_<TargetPose> == AdditiveAnimationName
					// * Number of frames between Additive Animation and Target Pose has to be the same
					|| TargetPoseSeq->bIsAdditive || TargetPoseSeq->SequenceName != TargetPoseName || TargetPoseSeq->NumFrames != AdditiveAnimSeq->NumFrames 
					|| TargetPoseSeq->GetAnimSet()->Sequences.FindItemIndex(TargetPoseSeq) == INDEX_NONE )
				{
					continue;
				}

				warnf(TEXT("\t\tFound potential Target Pose %s in AnimSet: %s"), *TargetPoseSeq->SequenceName.ToString(), *TargetPoseSeq->GetAnimSet()->GetFName().ToString());

				// See if that could be a fit...

				// Compare animations with each other, to see if they are the same.
				FCurveKeyArray CurveKeys;

				SkelCompA->AnimSets.Empty();
				SkelCompA->AnimSets.AddItem(AdditiveAnimSeq->GetAnimSet());
				AnimNodeSeqA->SetAnim(NAME_None);
				AnimNodeSeqA->SetAnim(AdditiveAnimSeq->SequenceName);
				if( AnimNodeSeqA->AnimSeq == NULL )
				{
					// Fail
					FAR_SetAnimFailed++;
					warnf(TEXT("\t\t\t\tSetAnim Failed!! %s"), *AdditiveAnimSeq->SequenceName.ToString());
					continue;
				}

				SkelCompB->AnimSets.Empty();
				SkelCompB->AnimSets.AddItem(TargetPoseSeq->GetAnimSet());
				AnimNodeSeqB->SetAnim(NAME_None);
				AnimNodeSeqB->SetAnim(TargetPoseSeq->SequenceName);
				if( AnimNodeSeqB->AnimSeq == NULL )
				{
					// Fail
					FAR_SetAnimFailed++;
					warnf(TEXT("\t\t\t\tSetAnim Failed!! %s"), *TargetPoseSeq->SequenceName.ToString());
					continue;
				}

				// Compare results...
				UBOOL bFailed = FALSE;
				FLOAT Error;
				for(INT i=0; i<DefaultSkeletalMesh->LODModels(0).RequiredBones.Num(); i++)
				{
					INT const BoneIndex = DefaultSkeletalMesh->LODModels(0).RequiredBones(i);

					FAnimSetMeshLinkup* AnimLinkupA = &AdditiveAnimSeq->GetAnimSet()->LinkupCache(AnimNodeSeqA->AnimLinkupIndex);
					INT	const TrackIndexA = AnimLinkupA->BoneToTrackTable(BoneIndex);

					FAnimSetMeshLinkup* AnimLinkupB = &TargetPoseSeq->GetAnimSet()->LinkupCache(AnimNodeSeqB->AnimLinkupIndex);
					INT	const TrackIndexB = AnimLinkupB->BoneToTrackTable(BoneIndex);

					if( TrackIndexA == INDEX_NONE || TrackIndexB == INDEX_NONE )
					{
						warnf(TEXT("\t\t\t\tSkipping bone (%d, %s) because no track present in animation A:%d, B: %d"), BoneIndex, *DefaultSkeletalMesh->RefSkeleton(BoneIndex).Name.ToString(), TrackIndexA, TrackIndexB);
						continue;
					}

					FRawAnimSequenceTrack& RawTrackA = AdditiveAnimSeq->RawAnimationData(TrackIndexA);
					FRawAnimSequenceTrack& RawTrackB = TargetPoseSeq->RawAnimationData(TrackIndexB);
					FRawAnimSequenceTrack& BasePoseTrack = AdditiveAnimSeq->AdditiveBasePose(TrackIndexA);

					INT const KeyIdx = RandHelper(AdditiveAnimSeq->NumFrames);
					FBoneAtom BoneAtomA, AddivePasePoseAtom;
					BoneAtomA.SetComponents(
						RawTrackA.RotKeys( KeyIdx < RawTrackA.RotKeys.Num() ? KeyIdx : 0 ),
						RawTrackA.PosKeys( KeyIdx < RawTrackA.PosKeys.Num() ? KeyIdx : 0 ));
					AddivePasePoseAtom.SetComponents(
						BasePoseTrack.RotKeys( KeyIdx < BasePoseTrack.RotKeys.Num() ? KeyIdx : 0 ),
						BasePoseTrack.PosKeys( KeyIdx < BasePoseTrack.PosKeys.Num() ? KeyIdx : 0 ));

					FBoneAtom BoneAtomB;
					BoneAtomB.SetComponents(
						RawTrackB.RotKeys( KeyIdx < RawTrackB.RotKeys.Num() ? KeyIdx : 0 ),
						RawTrackB.PosKeys( KeyIdx < RawTrackB.PosKeys.Num() ? KeyIdx : 0 ));

					if( BoneIndex > 0 )
					{
						BoneAtomA.FlipSignOfRotationW();
						AddivePasePoseAtom.FlipSignOfRotationW();
						BoneAtomB.FlipSignOfRotationW();
					}

					BoneAtomA.AddToTranslation(AddivePasePoseAtom.GetTranslationV());
					BoneAtomA.ConcatenateRotation(AddivePasePoseAtom.GetRotationV());
					// Normalize both rotations to be safe.
					BoneAtomA.NormalizeRotation();
					BoneAtomB.NormalizeRotation();

					Error = (BoneAtomA.GetTranslation() - BoneAtomB.GetTranslation()).Size();
					if( Error > MAXPOSDIFF )
					{
						warnf(TEXT("\t\t\t\tBone Translation not matching at key %d for bone %s with error %f"), KeyIdx, *DefaultSkeletalMesh->RefSkeleton(BoneIndex).Name.ToString(), Error);

						bFailed = TRUE;
						break;
					}
					Error = FQuatError(BoneAtomA.GetRotation(), BoneAtomB.GetRotation());
					if( Error > MAXANGLEDIFF )
					{
						warnf(TEXT("\t\t\t\tBone Rotation not matching at key %d for bone %s with error %f"), KeyIdx, *DefaultSkeletalMesh->RefSkeleton(BoneIndex).Name.ToString(), Error);
						bFailed = TRUE;
						break;
					}
				}

				if( bFailed )
				{
					continue;
				}

				// we found one!
				bFoundAtLeastOne = TRUE;
				warnf(TEXT("\t\t\t\tTarget Pose %s in AnimSet: %s worked!!"), *TargetPoseSeq->SequenceName.ToString(), *TargetPoseSeq->GetAnimSet()->GetFName().ToString());
				if( !bAnalyze )
				{
					// Have animations reference each other.
					INT OldSize = AdditiveAnimSeq->AdditiveTargetPoseAnimSeq.Num();
					AdditiveAnimSeq->AdditiveTargetPoseAnimSeq.AddUniqueItem(TargetPoseSeq);
					bMarkPackageDirty = bMarkPackageDirty || (OldSize != AdditiveAnimSeq->AdditiveTargetPoseAnimSeq.Num());

					OldSize = TargetPoseSeq->RelatedAdditiveAnimSeqs.Num();
					TargetPoseSeq->RelatedAdditiveAnimSeqs.AddUniqueItem(AdditiveAnimSeq);
					bMarkPackageDirty = bMarkPackageDirty || (OldSize != TargetPoseSeq->RelatedAdditiveAnimSeqs.Num());
				}
			}

			// Keep track of our success!
			if( bFoundAtLeastOne )
			{
				FAR_FoundTargetPose++;
			}
		}
	}

	void ScanForBasePoses(UAnimSequence* AdditiveAnimSeq, UPackage* Package)
	{
		// Base Pose Name we're looking for.
		FName BasePoseName = AdditiveAnimSeq->AdditiveRefName;

		// If we were built from the Bind Pose, then move along...
		if( BasePoseName == FName(TEXT("Bind Pose")) )
		{
			warnf(TEXT("\tBuilt from Bind Pose, so move along..."));
			FAR_BuiltFromBindPose++;
			return;
		}

		// Figure out how many frames we used from the base pose.
		INT BasePoseNumFrames = 0;
		for(INT i=0; i<AdditiveAnimSeq->AdditiveBasePose.Num() && BasePoseNumFrames<=1; i++)
		{
			BasePoseNumFrames = Max<INT>(BasePoseNumFrames, Max<INT>(AdditiveAnimSeq->AdditiveBasePose(i).PosKeys.Num(), AdditiveAnimSeq->AdditiveBasePose(i).RotKeys.Num()));
		}

		warnf(TEXT("\tLooking for Base Pose(s) named %s in current package... (Stored Base Pose has %d frame(s))"), *BasePoseName.ToString(), BasePoseNumFrames);
		UBOOL bFoundAtLeastOne = FALSE;
		for( TObjectIterator<UAnimSequence> ItTarget; ItTarget; ++ItTarget )
		{
			UAnimSequence* BasePoseSeq = *ItTarget;

			if( !BasePoseSeq->IsIn(Package) || BasePoseSeq == AdditiveAnimSeq 
				// A few assumptions to reduce the list of candidates:
				// * Base Pose cannot be an additive animation.
				// * Base Pose Name has to be stored 'BasePoseName' in Additive Animation
				// * Base Pose numframes can be scaled, but it can't be less than the base pose we have stored in the additive animation.
				|| BasePoseSeq->bIsAdditive || BasePoseSeq->SequenceName != BasePoseName || BasePoseSeq->NumFrames < BasePoseNumFrames
				|| BasePoseSeq->GetAnimSet()->Sequences.FindItemIndex(BasePoseSeq) == INDEX_NONE )
			{
				continue;
			}

			warnf(TEXT("\t\tFound potential Base Pose %s in AnimSet: %s, NumFrame(s): %d"), *BasePoseSeq->SequenceName.ToString(), *BasePoseSeq->GetAnimSet()->GetFName().ToString(), BasePoseSeq->NumFrames);

			// See if that could be a fit...

			// Compare animations with each other, to see if they are the same.

			SkelCompA->AnimSets.Empty();
			SkelCompA->AnimSets.AddItem(AdditiveAnimSeq->GetAnimSet());
			AnimNodeSeqA->SetAnim(NAME_None);
			AnimNodeSeqA->SetAnim(AdditiveAnimSeq->SequenceName);
			if( AnimNodeSeqA->AnimSeq == NULL )
			{
				// Fail
				FAR_SetAnimFailed++;
				warnf(TEXT("\t\t\t\tSetAnim Failed!! %s"), *AdditiveAnimSeq->SequenceName.ToString());
				continue;
			}

			SkelCompB->AnimSets.Empty();
			SkelCompB->AnimSets.AddItem(BasePoseSeq->GetAnimSet());
			AnimNodeSeqB->SetAnim(NAME_None);
			AnimNodeSeqB->SetAnim(BasePoseSeq->SequenceName);
			if( AnimNodeSeqB->AnimSeq == NULL )
			{
				// Fail
				FAR_SetAnimFailed++;
				warnf(TEXT("\t\t\t\tSetAnim Failed!! %s"), *BasePoseSeq->SequenceName.ToString());
				continue;
			}

			// Compare results...
			UBOOL bShouldScaleCandidate = (BasePoseSeq->NumFrames > 1 && BasePoseSeq->NumFrames != BasePoseNumFrames);
			UBOOL bFailed = FALSE;
			FLOAT Error;
			for(INT i=0; i<DefaultSkeletalMesh->LODModels(0).RequiredBones.Num(); i++)
			{
				INT const BoneIndex = DefaultSkeletalMesh->LODModels(0).RequiredBones(i);

				FAnimSetMeshLinkup* AnimLinkupA = &AdditiveAnimSeq->GetAnimSet()->LinkupCache(AnimNodeSeqA->AnimLinkupIndex);
				INT	const TrackIndexA = AnimLinkupA->BoneToTrackTable(BoneIndex);

				FAnimSetMeshLinkup* AnimLinkupB = &BasePoseSeq->GetAnimSet()->LinkupCache(AnimNodeSeqB->AnimLinkupIndex);
				INT	const TrackIndexB = AnimLinkupB->BoneToTrackTable(BoneIndex);

				if( TrackIndexA == INDEX_NONE || TrackIndexB == INDEX_NONE )
				{
					warnf(TEXT("\t\t\t\tSkipping bone (%d, %s) because no track present in animation A:%d, B: %d"), BoneIndex, *DefaultSkeletalMesh->RefSkeleton(BoneIndex).Name.ToString(), TrackIndexA, TrackIndexB);
					continue;
				}

				FRawAnimSequenceTrack& RawTrackB = BasePoseSeq->RawAnimationData(TrackIndexB);
				FRawAnimSequenceTrack& BasePoseTrack = AdditiveAnimSeq->AdditiveBasePose(TrackIndexA);

				// Pick a random Key to compare. 
				// Because we can use the first frame of the base pose animation, we need to clamp it.
				INT const KeyIdx = RandHelper(BasePoseNumFrames);

				FBoneAtom AddiveBasePoseAtom(
					BasePoseTrack.RotKeys( KeyIdx < BasePoseTrack.RotKeys.Num() ? KeyIdx : 0 ),
					BasePoseTrack.PosKeys( KeyIdx < BasePoseTrack.PosKeys.Num() ? KeyIdx : 0 ));

				FBoneAtom BoneAtomB;
				if( bShouldScaleCandidate && KeyIdx > 0 )
				{
					FLOAT Position = (BasePoseSeq->SequenceLength * FLOAT(KeyIdx)) / FLOAT(AdditiveAnimSeq->bAdditiveBuiltLooping ? BasePoseNumFrames : (BasePoseNumFrames-1));
					BasePoseSeq->GetBoneAtom(BoneAtomB, TrackIndexB, Position, AdditiveAnimSeq->bAdditiveBuiltLooping, TRUE);
					BoneAtomB.NormalizeRotation();
				}
				else
				{
					BoneAtomB.SetComponents(
						RawTrackB.RotKeys( KeyIdx < RawTrackB.RotKeys.Num() ? KeyIdx : 0 ),
						RawTrackB.PosKeys( KeyIdx < RawTrackB.PosKeys.Num() ? KeyIdx : 0 ));
					BoneAtomB.NormalizeRotation();
				}

				Error = (AddiveBasePoseAtom.GetTranslation() - BoneAtomB.GetTranslation()).Size();
				if( Error > MAXPOSDIFF )
				{
					warnf(TEXT("\t\t\t\tBone Translation not matching at key %d for bone %s with error %f. Additive: %s, Base: %s"), KeyIdx, *DefaultSkeletalMesh->RefSkeleton(BoneIndex).Name.ToString(), Error, *AddiveBasePoseAtom.GetTranslation().ToString(), *BoneAtomB.GetTranslation().ToString());

					bFailed = TRUE;
					break;
				}
				Error = FQuatError(AddiveBasePoseAtom.GetRotation(), BoneAtomB.GetRotation());
				if( Error > MAXANGLEDIFF )
				{
					warnf(TEXT("\t\t\t\tBone Rotation not matching at key %d for bone %s with error %f. Additive: %s, Base: %s"), KeyIdx, *DefaultSkeletalMesh->RefSkeleton(BoneIndex).Name.ToString(), Error, *AddiveBasePoseAtom.GetRotation().ToString(), *BoneAtomB.GetRotation().ToString());
					bFailed = TRUE;
					break;
				}
			}

			if( bFailed )
			{
				continue;
			}

			// we found one!
			bFoundAtLeastOne = TRUE;
			warnf(TEXT("\t\t\t\tBase Pose %s in AnimSet: %s worked!!"), *BasePoseSeq->SequenceName.ToString(), *BasePoseSeq->GetAnimSet()->GetFName().ToString());
			if( !bAnalyze )
			{
				// Have animations reference each other.

				INT OldSize = AdditiveAnimSeq->AdditiveBasePoseAnimSeq.Num();
				AdditiveAnimSeq->AdditiveBasePoseAnimSeq.AddUniqueItem(BasePoseSeq);
				bMarkPackageDirty = bMarkPackageDirty || (OldSize != AdditiveAnimSeq->AdditiveBasePoseAnimSeq.Num());

				OldSize = BasePoseSeq->RelatedAdditiveAnimSeqs.Num();
				BasePoseSeq->RelatedAdditiveAnimSeqs.AddUniqueItem(AdditiveAnimSeq);
				bMarkPackageDirty = bMarkPackageDirty || (OldSize != BasePoseSeq->RelatedAdditiveAnimSeqs.Num());
			}
		}

		// Keep track of our success!
		if( bFoundAtLeastOne )
		{
			FAR_FoundBasePose++;
		}
	}

	void VerifyBasePoses(UAnimSequence* AdditiveAnimSeq, UPackage* Package)
	{
		// Verify AdditiveBasePoseAnimSeq is valid. 
		if( AdditiveAnimSeq->AdditiveBasePoseAnimSeq.Num() == 0 )
		{
			FString LogString = AdditiveAnimSeq->GetAnimSet()->GetFName().ToString() + FString(TEXT(".")) + AdditiveAnimSeq->SequenceName.ToString();

			// If we were built from the Bind Pose, then we're not expecting to be referencing another animation.
			if( AdditiveAnimSeq->AdditiveRefName == FName(TEXT("Bind Pose")) )
			{
				FAR_UsingBindPoseList.AddItem( FName(*LogString) );
			}
			else
			{
				// No Base Pose found.
				FAR_MissingBasePoseList.AddItem( FName(*LogString) );
				warnf(TEXT("\t\tNo Base Pose for %s"), *LogString);
			}
		}
		else
		{
			// We go backwards in case we need to remove some elements.
			for(INT i=AdditiveAnimSeq->AdditiveBasePoseAnimSeq.Num()-1; i>=0; i--)
			{
				UAnimSequence* TestAnimSeq = AdditiveAnimSeq->AdditiveBasePoseAnimSeq(i);
				if( !TestAnimSeq )
				{
					// We have a problem, empty reference!!
					FAR_EmptyReference++;
					if( !bAnalyze )
					{
						AdditiveAnimSeq->AdditiveBasePoseAnimSeq.Remove(i, 1);
						bMarkPackageDirty = TRUE;
					}
					warnf(TEXT("\t\tEmpty Reference in AdditiveBasePoseAnimSeq array!"));
				}
				else
				{
					// Verify this reference, has also a reference back to us
					if( TestAnimSeq->RelatedAdditiveAnimSeqs.FindItemIndex(AdditiveAnimSeq) == INDEX_NONE )
					{
						FAR_OneWayReference++;
						warnf(TEXT("\t\tOne way reference! Base Pose is missing reference to Additive Animation."));
						if( !bAnalyze )
						{
							INT OldSize = TestAnimSeq->RelatedAdditiveAnimSeqs.Num();
							TestAnimSeq->RelatedAdditiveAnimSeqs.AddUniqueItem(AdditiveAnimSeq);
							bMarkPackageDirty = bMarkPackageDirty || (OldSize != TestAnimSeq->RelatedAdditiveAnimSeqs.Num());
						}
						if( !TestAnimSeq->IsIn(Package) )
						{
							FAR_ReferenceInDifferentPackage++;
							warnf(TEXT("\t\t\t\tReference is in a different package! %s, %s"), *TestAnimSeq->SequenceName.ToString(), *TestAnimSeq->GetAnimSet()->GetFName().ToString());
						}
					}
				}
			}
		}
	}

	void VerifyTargetPoses(UAnimSequence* AdditiveAnimSeq, UPackage* Package)
	{
		// Verify AdditiveBasePoseAnimSeq is valid. 
		if( AdditiveAnimSeq->AdditiveTargetPoseAnimSeq.Num() == 0 )
		{
			// No Base Pose found.
			FString LogString = AdditiveAnimSeq->GetAnimSet()->GetFName().ToString() + FString(TEXT(".")) + AdditiveAnimSeq->SequenceName.ToString();
			FAR_MissingTargetPoseList.AddItem( FName(*LogString) );
			warnf(TEXT("\t\tNo Target Pose for %s"), *LogString);
		}
		else
		{
			// We go backwards in case we need to remove some elements.
			for(INT i=AdditiveAnimSeq->AdditiveTargetPoseAnimSeq.Num()-1; i>=0; i--)
			{
				UAnimSequence* TestAnimSeq = AdditiveAnimSeq->AdditiveTargetPoseAnimSeq(i);
				if( !TestAnimSeq )
				{
					// We have a problem, empty reference!!
					FAR_EmptyReference++;
					if( !bAnalyze )
					{
						AdditiveAnimSeq->AdditiveTargetPoseAnimSeq.Remove(i, 1);
						bMarkPackageDirty = TRUE;
					}
					warnf(TEXT("\t\tEmpty Reference in AdditiveTargetPoseAnimSeq array!"));
				}
				else
				{
					// Verify this reference, has also a reference back to us
					if( TestAnimSeq->RelatedAdditiveAnimSeqs.FindItemIndex(AdditiveAnimSeq) == INDEX_NONE )
					{
						FAR_OneWayReference++;
						warnf(TEXT("\t\tOne way reference! Base Pose is missing reference to Additive Animation."));
						if( !bAnalyze )
						{
							INT OldSize = TestAnimSeq->RelatedAdditiveAnimSeqs.Num();
							TestAnimSeq->RelatedAdditiveAnimSeqs.AddUniqueItem(AdditiveAnimSeq);
							bMarkPackageDirty = bMarkPackageDirty || (OldSize != TestAnimSeq->RelatedAdditiveAnimSeqs.Num());
						}
						if( !TestAnimSeq->IsIn(Package) )
						{
							FAR_ReferenceInDifferentPackage++;
							warnf(TEXT("\t\t\t\tReference is in a different package! %s, %s"), *TestAnimSeq->SequenceName.ToString(), *TestAnimSeq->GetAnimSet()->GetFName().ToString());
						}
					}
				}
			}
		}
	}

};


INT UFixAdditiveReferencesCommandlet::Main( const FString& Params )
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	// want everything in upper case, it's a mess otherwise
	const FString ParamsUpperCase = Params.ToUpper();
	const TCHAR* Parms = *ParamsUpperCase;
	UCommandlet::ParseCommandLine(Parms, Tokens, Switches);

	// Call function on all packages...
	DoActionToAllPackages<UAnimSequence, FixAdditiveReferencesFunctor>(this, ParamsUpperCase);

	warnf(TEXT("\n\n====\nREPORT\n====\n\n"));

	warnf(TEXT("FAR_NumAdditiveAnimations: %d\nFAR_FoundTargetPose: %d\nFAR_FoundBasePose: %d\nFAR_MissingDefaultSkelMesh: %d\nFAR_SetAnimFailed: %d\nFAR_RecoveredAnimSeqOrphan: %d\nFAR_BuiltFromBindPose: %d\nFAR_ReferenceInDifferentPackage: %d\nFAR_OneWayReference: %d\nFAR_EmptyReference: %d\n"), 
		FAR_NumAdditiveAnimations, FAR_FoundTargetPose, FAR_FoundBasePose, FAR_MissingDefaultSkelMesh, FAR_SetAnimFailed, FAR_RecoveredAnimSeqOrphan, FAR_BuiltFromBindPose, FAR_ReferenceInDifferentPackage, FAR_OneWayReference, FAR_EmptyReference);

	warnf(TEXT("\nMisnamed Additive Animations (Name should be ADD_<TargetPoseName>):"));
	for(INT i=0; i<FAR_MisnamedAdditives.Num(); i++)
	{
		warnf(TEXT("%4d %s"), i+1, *FAR_MisnamedAdditives(i).ToString());
	}

	warnf(TEXT("\nAdditive Animations Missing a Base Pose:"));
	for(INT i=0; i<FAR_MissingBasePoseList.Num(); i++)
	{
		warnf(TEXT("%4d %s"), i+1, *FAR_MissingBasePoseList(i).ToString());
	}

	warnf(TEXT("\nAdditive Animations using the Bind Pose as a Base Pose (Should probably be changed to using a dedicated Base Pose Animation):"));
	for(INT i=0; i<FAR_UsingBindPoseList.Num(); i++)
	{
		warnf(TEXT("%4d %s"), i+1, *FAR_UsingBindPoseList(i).ToString());
	}

	warnf(TEXT("\nAdditive Animations Missing a Target Pose:"));
	for(INT i=0; i<FAR_MissingTargetPoseList.Num(); i++)
	{
		warnf(TEXT("%4d %s"), i+1, *FAR_MissingTargetPoseList(i).ToString());
	}

	warnf(TEXT("\nPackages that could not be saved:"));
	for(INT i=0; i<FAR_PackagesNotSaved.Num(); i++)
	{
		warnf(TEXT("%4d %s"), i+1, *FAR_PackagesNotSaved(i).ToString());
	}

	warnf(TEXT("\n\n====\nTHE END!\n====\n\n"));
	return 0;
}
IMPLEMENT_CLASS(UFixAdditiveReferencesCommandlet)





/*-----------------------------------------------------------------------------
UListScriptReferencedContentCommandlet.
-----------------------------------------------------------------------------*/
/**
* Processes a value found by ListReferencedContent(), possibly recursing for inline objects
*
* @param	Value			the object to be processed
* @param	Property		the property where Value was found (for a dynamic array, this is the Inner property)
* @param	PropertyDesc	string printed as the property Value was assigned to (usually *Property->GetName(), except for dynamic arrays, where it's the array name and index)
* @param	Tab				string with a number of tabs for the current tab level of the output
*/
void UListScriptReferencedContentCommandlet::ProcessObjectValue(UObject* Value, UProperty* Property, const FString& PropertyDesc, const FString& Tab)
{
	if (Value != NULL)
	{
		// if it's an inline object, recurse over its properties
		if ((Property->PropertyFlags & CPF_NeedCtorLink) || (Property->PropertyFlags & CPF_Component))
		{
			ListReferencedContent(Value->GetClass(), (BYTE*)Value, FString(*Value->GetName()), Tab + TEXT("\t"));
		}
		else
		{
			// otherwise, print it as content that's being referenced
			warnf(TEXT("%s\t%s=%s'%s'"), *Tab, *PropertyDesc, *Value->GetClass()->GetName(), *Value->GetPathName());
		}
	}
}

/**
* Lists content referenced by the given data
*
* @param	Struct		the type of the Default data
* @param	Default		the data to look for referenced objects in
* @param	HeaderName	string printed before any content references found (only if the data might contain content references)
* @param	Tab			string with a number of tabs for the current tab level of the output
*/
void UListScriptReferencedContentCommandlet::ListReferencedContent(UStruct* Struct, BYTE* Default, const FString& HeaderName, const FString& Tab)
{
	UBOOL bPrintedHeader = FALSE;

	// iterate over all its properties
	for (UProperty* Property = Struct->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext)
	{
		if ( !bPrintedHeader &&
			(Property->IsA(UObjectProperty::StaticClass()) || Property->IsA(UStructProperty::StaticClass()) || Property->IsA(UArrayProperty::StaticClass())) &&
			Property->ContainsObjectReference() )
		{
			// this class may contain content references, so print header with class/struct name
			warnf(TEXT("%s%s"), *Tab, *HeaderName);
			bPrintedHeader = TRUE;
		}
		// skip class properties and object properties of class Object
		UObjectProperty* ObjectProp = Cast<UObjectProperty>(Property);
		if (ObjectProp != NULL && ObjectProp->PropertyClass != UObject::StaticClass() && ObjectProp->PropertyClass != UClass::StaticClass())
		{
			if (ObjectProp->ArrayDim > 1)
			{
				for (INT i = 0; i < ObjectProp->ArrayDim; i++)
				{
					ProcessObjectValue(*(UObject**)(Default + Property->Offset + i * Property->ElementSize), Property, FString::Printf(TEXT("%s[%d]"), *Property->GetName(), i), Tab);
				}
			}
			else
			{
				ProcessObjectValue(*(UObject**)(Default + Property->Offset), Property, FString(*Property->GetName()), Tab);
			}
		}
		else if (Property->IsA(UStructProperty::StaticClass()))
		{
			if (Property->ArrayDim > 1)
			{
				for (INT i = 0; i < Property->ArrayDim; i++)
				{
					ListReferencedContent(((UStructProperty*)Property)->Struct, (Default + Property->Offset + i * Property->ElementSize), FString::Printf(TEXT("%s[%d]"), *Property->GetName(), i), Tab + TEXT("\t"));
				}
			}
			else
			{
				ListReferencedContent(((UStructProperty*)Property)->Struct, (Default + Property->Offset), FString(*Property->GetName()), Tab + TEXT("\t"));
			}
		}
		else if (Property->IsA(UArrayProperty::StaticClass()))
		{
			UArrayProperty* ArrayProp = (UArrayProperty*)Property;
			FScriptArray* Array = (FScriptArray*)(Default + Property->Offset);
			UObjectProperty* ObjectProp = Cast<UObjectProperty>(ArrayProp->Inner);
			if (ObjectProp != NULL && ObjectProp->PropertyClass != UObject::StaticClass() && ObjectProp->PropertyClass != UClass::StaticClass())
			{
				for (INT i = 0; i < Array->Num(); i++)
				{
					ProcessObjectValue(*(UObject**)((BYTE*)Array->GetData() + (i * ArrayProp->Inner->ElementSize)), ObjectProp, FString::Printf(TEXT("%s[%d]"), *ArrayProp->GetName(), i), Tab);
				}
			}
			else if (ArrayProp->Inner->IsA(UStructProperty::StaticClass()))
			{
				UStruct* InnerStruct = ((UStructProperty*)ArrayProp->Inner)->Struct;
				INT PropertiesSize = InnerStruct->GetPropertiesSize();
				for (INT i = 0; i < Array->Num(); i++)
				{
					ListReferencedContent(InnerStruct, (BYTE*)Array->GetData() + (i * ArrayProp->Inner->ElementSize), FString(*Property->GetName()), Tab + TEXT("\t"));
				}
			}
		}
	}
}

/** lists all content referenced in the default properties of script classes */
INT UListScriptReferencedContentCommandlet::Main(const FString& Params)
{
	warnf(TEXT("Loading EditPackages..."));
	// load all the packages in the EditPackages list
	BeginLoad();
	for (INT i = 0; i < GEditor->EditPackages.Num(); i++)
	{
		LoadPackage(NULL, *GEditor->EditPackages(i), 0);
	}
	EndLoad();

	// iterate over all classes
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (!(It->ClassFlags & CLASS_Intrinsic))
		{
			ListReferencedContent(*It, (BYTE*)It->GetDefaultObject(), FString::Printf(TEXT("%s %s"), *It->GetFullName(), It->HasAnyFlags(RF_Native) ? TEXT("(native)") : TEXT("")));
		}
	}

	return 0;
}
IMPLEMENT_CLASS(UListScriptReferencedContentCommandlet);


/*-----------------------------------------------------------------------------
UAnalyzeContent commandlet.
-----------------------------------------------------------------------------*/

/**
* Helper structure to hold level/ usage count information.
*/
struct FLevelResourceStat
{
	/** Level package.						*/
	UObject*	LevelPackage;
	/** Usage count in above level package.	*/
	INT			Count;

	/**
	* Constructor, initializing Count to 1 and setting package.
	*
	* @param InLevelPackage	Level package to use
	*/
	FLevelResourceStat( UObject* InLevelPackage )
		: LevelPackage( InLevelPackage )
		, Count( 1 )
	{}
};
/**
* Helper structure containing usage information for a single resource and multiple levels.
*/
struct FResourceStat
{
	/** Resource object.											*/
	UObject*					Resource;
	/** Total number this resource is used across all levels.		*/
	INT							TotalCount;
	/** Array of detailed per level resource usage breakdown.		*/
	TArray<FLevelResourceStat>	LevelResourceStats;

	/**
	* Constructor
	*
	* @param	InResource		Resource to use
	* @param	LevelPackage	Level package to use
	*/
	FResourceStat( UObject* InResource, UObject* LevelPackage )
		:	Resource( InResource )
		,	TotalCount( 1 )
	{
		// Create initial stat entry.
		LevelResourceStats.AddItem( FLevelResourceStat( LevelPackage ) );
	}

	/**
	* Increment usage count by one
	*
	* @param	LevelPackage	Level package using the resource.
	*/
	void IncrementUsage( UObject* LevelPackage )
	{
		// Iterate over all level resource stats to find existing entry.
		UBOOL bFoundExisting = FALSE;
		for( INT LevelIndex=0; LevelIndex<LevelResourceStats.Num(); LevelIndex++ )
		{
			FLevelResourceStat& LevelResourceStat = LevelResourceStats(LevelIndex);
			// We found a match.
			if( LevelResourceStat.LevelPackage == LevelPackage )
			{
				// Increase its count and exit loop.
				LevelResourceStat.Count++;
				bFoundExisting = TRUE;
				break;
			}
		}
		// No existing entry has been found, add new one.
		if( !bFoundExisting )
		{
			LevelResourceStats.AddItem( FLevelResourceStat( LevelPackage ) );
		}
		// Increase total count.
		TotalCount++;
	}
};
/** Compare function used by sort. Sorts in descending order. */
IMPLEMENT_COMPARE_CONSTREF( FResourceStat, UnPackageUtilities, { return B.TotalCount - A.TotalCount; } );

/**
* Class encapsulating stats functionality.
*/
class FResourceStatContainer
{
public:
	/**
	* Constructor
	*
	* @param	InDescription	Description used for dumping stats.
	*/
	FResourceStatContainer( const TCHAR* InDescription )
		:	Description( InDescription )
	{}

	/** 
	* Function called when a resource is encountered in a level to increment count.
	*
	* @param	Resource		Encountered resource.
	* @param	LevelPackage	Level package resource was encountered in.
	*/
	void EncounteredResource( UObject* Resource, UObject* LevelPackage )
	{
		FResourceStat* ResourceStatPtr = ResourceToStatMap.Find( Resource );
		// Resource has existing stat associated.
		if( ResourceStatPtr != NULL )
		{
			ResourceStatPtr->IncrementUsage( LevelPackage );
		}
		// Associate resource with new stat.
		else
		{
			FResourceStat ResourceStat( Resource, LevelPackage );
			ResourceToStatMap.Set( Resource, ResourceStat );
		}
	}

	/**
	* Dumps all the stats information sorted to the log.
	*/
	void DumpStats()
	{
		// Copy TMap data into TArray so it can be sorted.
		TArray<FResourceStat> SortedList;
		for( TMap<UObject*,FResourceStat>::TIterator It(ResourceToStatMap); It; ++It )
		{
			SortedList.AddItem( It.Value() );
		}
		// Sort the list in descending order by total count.
		Sort<USE_COMPARE_CONSTREF(FResourceStat,UnPackageUtilities)>( SortedList.GetTypedData(), SortedList.Num() );

		warnf( NAME_Log, TEXT("") ); 
		warnf( NAME_Log, TEXT("") ); 
		warnf( NAME_Log, TEXT("Stats for %s."), *Description ); 
		warnf( NAME_Log, TEXT("") ); 

		// Iterate over all entries and dump info.
		for( INT i=0; i<SortedList.Num(); i++ )
		{
			const FResourceStat& ResourceStat = SortedList(i);
			warnf( NAME_Log, TEXT("%4i use%s%4i level%s for%s   %s"), 
				ResourceStat.TotalCount,
				ResourceStat.TotalCount > 1 ? TEXT("s in") : TEXT(" in "), 
				ResourceStat.LevelResourceStats.Num(), 
				ResourceStat.LevelResourceStats.Num() > 1 ? TEXT("s") : TEXT(""),
				ResourceStat.LevelResourceStats.Num() > 1 ? TEXT("") : TEXT(" "),
				*ResourceStat.Resource->GetFullName() );

			for( INT LevelIndex=0; LevelIndex<ResourceStat.LevelResourceStats.Num(); LevelIndex++ )
			{
				const FLevelResourceStat& LevelResourceStat = ResourceStat.LevelResourceStats(LevelIndex);
				warnf( NAME_Log, TEXT("    %4i use%s: %s"), 
					LevelResourceStat.Count, 
					LevelResourceStat.Count > 1 ? TEXT("s in") : TEXT("  in"), 
					*LevelResourceStat.LevelPackage->GetName() );
			}
		}
	}
private:
	/** Map from resource to stat helper structure. */
	TMap<UObject*,FResourceStat>	ResourceToStatMap;
	/** Description used for dumping stats.			*/
	FString							Description;
};

void UAnalyzeContentCommandlet::StaticInitialize()
{
	ShowErrorCount = FALSE;
}

INT UAnalyzeContentCommandlet::Main( const FString& Params )
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	const TCHAR* Parms = *Params;
	ParseCommandLine(Parms, Tokens, Switches);

	// Retrieve all package file names and iterate over them, comparing them to tokens.
	TArray<FString> PackageFileList = GPackageFileCache->GetPackageFileList();		
	for( INT PackageIndex=0; PackageIndex<PackageFileList.Num(); PackageIndex++ )
	{
		// Tokens on the command line are package names.
		for( INT TokenIndex=0; TokenIndex<Tokens.Num(); TokenIndex++ )
		{
			// Compare the two and see whether we want to include this package in the analysis.
			FFilename PackageName = PackageFileList( PackageIndex );
			if( Tokens(TokenIndex) == PackageName.GetBaseFilename() )
			{
				UPackage* Package = UObject::LoadPackage( NULL, *PackageName, LOAD_None );
				if( Package != NULL )
				{
					warnf( NAME_Log, TEXT("Loading %s"), *PackageName );
					// Find the world and load all referenced levels.
					UWorld* World = FindObjectChecked<UWorld>( Package, TEXT("TheWorld") );
					if( World )
					{
						AWorldInfo* WorldInfo	= World->GetWorldInfo();
						// Iterate over streaming level objects loading the levels.
						for( INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++ )
						{
							ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
							if( StreamingLevel )
							{
								// Load package if found.
								FString Filename;
								if( GPackageFileCache->FindPackageFile( *StreamingLevel->PackageName.ToString(), NULL, Filename ) )
								{
									warnf(NAME_Log, TEXT("Loading sub-level %s"), *Filename);
									LoadPackage( NULL, *Filename, LOAD_None );
								}
							}
						}
					}
				}
				else
				{
					warnf( NAME_Error, TEXT("Error loading %s!"), *PackageName );
				}
			}
		}
	}

	// By now all objects are in memory.

	FResourceStatContainer StaticMeshMaterialStats( TEXT("materials applied to static meshes") );
	FResourceStatContainer StaticMeshStats( TEXT("static meshes placed in levels") );
	FResourceStatContainer BSPMaterialStats( TEXT("materials applied to BSP surfaces") );

	// Iterate over all static mesh components and add their materials and static meshes.
	for( TObjectIterator<UStaticMeshComponent> It; It; ++It )
	{
		UStaticMeshComponent*	StaticMeshComponent = *It;
		UPackage*				LevelPackage		= StaticMeshComponent->GetOutermost();

		// Only add if the outer is a map package.
		if( LevelPackage->ContainsMap() )
		{
			if( StaticMeshComponent->StaticMesh && StaticMeshComponent->StaticMesh->LODModels.Num() )
			{
				// Populate materials array, avoiding duplicate entries.
				TArray<UMaterial*> Materials;
				INT MaterialCount = StaticMeshComponent->StaticMesh->LODModels(0).Elements.Num();
				for( INT MaterialIndex=0; MaterialIndex<MaterialCount; MaterialIndex++ )
				{
					UMaterialInterface* MaterialInterface = StaticMeshComponent->GetMaterial( MaterialIndex );
					if( MaterialInterface && MaterialInterface->GetMaterial() )
					{
						Materials.AddUniqueItem( MaterialInterface->GetMaterial() );
					}
				}

				// Iterate over materials and create/ update associated stats.
				for( INT MaterialIndex=0; MaterialIndex<Materials.Num(); MaterialIndex++ )
				{
					UMaterial* Material = Materials(MaterialIndex);
					// Track materials applied to static meshes.			
					StaticMeshMaterialStats.EncounteredResource( Material, LevelPackage );
				}
			}

			// Track static meshes used by static mesh components.
			if( StaticMeshComponent->StaticMesh )
			{
				StaticMeshStats.EncounteredResource( StaticMeshComponent->StaticMesh, LevelPackage );
			}
		}
	}

	for( TObjectIterator<ABrush> It; It; ++It )
	{
		ABrush*		BrushActor		= *It;
		UPackage*	LevelPackage	= BrushActor->GetOutermost();

		// Only add if the outer is a map package.
		if( LevelPackage->ContainsMap() )
		{
			if( BrushActor->Brush && BrushActor->Brush->Polys )
			{
				UPolys* Polys = BrushActor->Brush->Polys;

				// Populate materials array, avoiding duplicate entries.
				TArray<UMaterial*> Materials;
				for( INT ElementIndex=0; ElementIndex<Polys->Element.Num(); ElementIndex++ )
				{
					const FPoly& Poly = Polys->Element(ElementIndex);
					if( Poly.Material && Poly.Material->GetMaterial() )
					{
						Materials.AddUniqueItem( Poly.Material->GetMaterial() );
					}
				}

				// Iterate over materials and create/ update associated stats.
				for( INT MaterialIndex=0; MaterialIndex<Materials.Num(); MaterialIndex++ )
				{
					UMaterial* Material = Materials(MaterialIndex);
					// Track materials applied to BSP.
					BSPMaterialStats.EncounteredResource( Material, LevelPackage );
				}
			}
		}
	}

	// Dump stat summaries.
	StaticMeshMaterialStats.DumpStats();
	StaticMeshStats.DumpStats();
	BSPMaterialStats.DumpStats();

	return 0;
}
IMPLEMENT_CLASS(UAnalyzeContentCommandlet)



IMPLEMENT_CLASS(UTestWordWrapCommandlet);
INT UTestWordWrapCommandlet::Main(const FString& Params)
{
	INT Result = 0;

	// replace any \n strings with the real character code
	FString MyParams = Params.Replace(TEXT("\\n"), TEXT("\n"));
	const TCHAR* Parms = *MyParams;

	INT WrapWidth = 0;
	FString WrapWidthString;
	ParseToken(Parms, WrapWidthString, FALSE);
	WrapWidth = appAtoi(*WrapWidthString);

	// advance past the space between the width and the test string
	Parms++;
	warnf(TEXT("WrapWidth: %i  WrapText: '%s'"), WrapWidth, Parms);
	UFont* DrawFont = GEngine->GetTinyFont();

	FTextSizingParameters Parameters(0, 0, WrapWidth, 0, DrawFont);
	TArray<FWrappedStringElement> Lines;

	UCanvas::WrapString(Parameters, 0, Parms, Lines, TEXT("\n"));

	warnf(TEXT("Result: %i lines"), Lines.Num());
	for ( INT LineIndex = 0; LineIndex < Lines.Num(); LineIndex++ )
	{
		FWrappedStringElement& Line = Lines(LineIndex);
		warnf(TEXT("Line %i): (X=%.2f,Y=%.2f) '%s'"), LineIndex, Line.LineExtent.X, Line.LineExtent.Y, *Line.Value);
	}

	return Result;
}

/** sets certain allowed package flags on the specified package(s) */
INT USetPackageFlagsCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens, Switches;

	ParseCommandLine(*Params, Tokens, Switches);

	if (Tokens.Num() < 2)
	{
		warnf(TEXT("Syntax: setpackageflags <package/wildcard> <flag1=value> <flag2=value>..."));
		warnf(TEXT("Supported flags: ServerSideOnly, ClientOptional, AllowDownload"));
		return 1;
	}

	// find all the files matching the specified filename/wildcard
	TArray<FString> FilesInPath;
	GFileManager->FindFiles(FilesInPath, *Tokens(0), 1, 0);
	if (FilesInPath.Num() == 0)
	{
		warnf(NAME_Error, TEXT("No packages found matching %s!"), *Tokens(0));
		return 2;
	}
	// get the directory part of the filename
	INT ChopPoint = Max(Tokens(0).InStr(TEXT("/"), 1) + 1, Tokens(0).InStr(TEXT("\\"), 1) + 1);
	if (ChopPoint < 0)
	{
		ChopPoint = Tokens(0).InStr( TEXT("*"), 1 );
	}
	FString PathPrefix = (ChopPoint < 0) ? TEXT("") : Tokens(0).Left(ChopPoint);

	// parse package flags
	DWORD PackageFlagsToAdd = 0, PackageFlagsToRemove = 0;
	for (INT i = 1; i < Tokens.Num(); i++)
	{
		DWORD NewFlag = 0;
		UBOOL bValue;
		if (ParseUBOOL(*Tokens(i), TEXT("ServerSideOnly="), bValue))
		{
			NewFlag = PKG_ServerSideOnly;
		}
		else if (ParseUBOOL(*Tokens(i), TEXT("ClientOptional="), bValue))
		{
			NewFlag = PKG_ClientOptional;
		}
		else if (ParseUBOOL(*Tokens(i), TEXT("AllowDownload="), bValue))
		{
			NewFlag = PKG_AllowDownload;
		}
		else
		{
			warnf(NAME_Warning, TEXT("Unknown package flag '%s' specified"), *Tokens(i));
		}
		if (NewFlag != 0)
		{
			if (bValue)
			{
				PackageFlagsToAdd |= NewFlag;
			}
			else
			{
				PackageFlagsToRemove |= NewFlag;
			}
		}
	}
	// process files
	for (INT i = 0; i < FilesInPath.Num(); i++)
	{
		const FString& PackageName = FilesInPath(i);
		const FString PackagePath = PathPrefix + PackageName;
		FString FileName;
		if( !GPackageFileCache->FindPackageFile( *PackagePath, NULL, FileName ) )
		{
			warnf(NAME_Error, TEXT("Couldn't find package '%s'"), *PackageName);
			continue;
		}

		// skip if read-only
		if (GFileManager->IsReadOnly(*FileName))
		{
			warnf(TEXT("Skipping %s (read-only)"), *FileName);
		}
		else
		{
			// load the package
			warnf(TEXT("Loading %s..."), *PackageName); 
			UPackage* Package = LoadPackage(NULL, *PackageName, LOAD_None);
			if (Package == NULL)
			{
				warnf(NAME_Error, TEXT("Failed to load package '%s'"), *PackageName);
			}
			else
			{
				// set flags
				Package->PackageFlags |= PackageFlagsToAdd;
				Package->PackageFlags &= ~PackageFlagsToRemove;
				// save the package
				warnf(TEXT("Saving %s..."), *PackageName);
				SavePackage(Package, NULL, RF_Standalone, *FileName, GWarn);
			}
			// GC the package
			warnf(TEXT("Cleaning up..."));
			CollectGarbage(RF_Native);
		}
	}
	return 0;
}
IMPLEMENT_CLASS(USetPackageFlagsCommandlet);


/* ==========================================================================================================
	UPerformMapCheckCommandlet
========================================================================================================== */
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
INT UPerformMapCheckCommandlet::InitializeMapCheck( const TArray<FString>& Tokens, const TArray<FString>& Switches, TArray<FFilename>& MapPathNames )
{
	bTestOnly = Switches.ContainsItem(TEXT("TESTONLY"));
	MapIndex = TotalMapsChecked = 0;

	TArray<FString> Unused;
	// assume the first token is the map wildcard/pathname
	FString MapWildcard = Tokens.Num() > 0 ? Tokens(0) : FString(TEXT("*.")) + FURL::DefaultMapExt;
	if ( NormalizePackageNames( Unused, MapNames, MapWildcard, NORMALIZE_ExcludeContentPackages) )
	{
		return 0;
	}

	return 1;
}

INT UPerformMapCheckCommandlet::Main( const FString& Params )
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	INT ReturnValue = InitializeMapCheck(Tokens, Switches, MapNames);
	if ( ReturnValue != 0 )
	{
		return ReturnValue;
	}

	warnf(TEXT("Found %i maps"), MapNames.Num());
	INT GCIndex=0;
	for ( MapIndex = 0; MapIndex < MapNames.Num(); MapIndex++ )
	{
		const FFilename& Filename = MapNames(MapIndex);
		warnf( TEXT("Loading  %s...  (%i / %i)"), *Filename, MapIndex, MapNames.Num() );

		UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_NoWarn );
		if ( Package == NULL )
		{
			warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
			continue;
		}

		// skip packages in the trashcan or PIE packages
		if ( (Package->PackageFlags&(PKG_Trash|PKG_PlayInEditor)) != 0 )
		{
			warnf(TEXT("Skipping %s (%s)"), *Filename, (Package->PackageFlags&PKG_Trash) != 0 ? TEXT("Trashcan Map") : TEXT("PIE Map"));
			UObject::CollectGarbage(RF_Native);
			continue;
		}

		TotalMapsChecked++;

		warnf(TEXT("Checking %s..."), *Filename);
		ReturnValue = CheckMapPackage( Package );
		if ( ReturnValue != 0 )
		{
			return ReturnValue;
		}

		// save shader caches incase we encountered some that weren't fully cached yet
		SaveLocalShaderCaches();

		// collecting garbage every 10 maps instead of every map makes the commandlet run much faster
		if( (++GCIndex % 10) == 0 )
		{
			UObject::CollectGarbage(RF_Native);
		}
	}

	ReturnValue = ProcessResults();
	return ReturnValue;
}

/**
 * The main worker method - performs the commandlets tests on the package.
 *
 * @param	MapPackage	the current package to be processed
 *
 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
 */
INT UPerformMapCheckCommandlet::CheckMapPackage( UPackage* MapPackage )
{
	for ( TObjectIterator<AActor> It; It; ++It )
	{
		AActor* Actor = *It;
		if ( Actor->IsIn(MapPackage) && !Actor->IsTemplate() )
		{
			Actor->CheckForErrors();
		}
	}

	return 0;
}
IMPLEMENT_CLASS(UPerformMapCheckCommandlet);

/* ==========================================================================================================
	UFindStaticActorsRefsCommandlet
========================================================================================================== */
INT UFindStaticActorsRefsCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}
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
INT UFindStaticActorsRefsCommandlet::InitializeMapCheck( const TArray<FString>& Tokens, const TArray<FString>& Switches, TArray<FFilename>& MapPathNames )
{
	bStaticKismetRefs = Switches.ContainsItem(TEXT("STATICREFS"));
	bShowObjectNames = Switches.ContainsItem(TEXT("OBJECTNAMES"));
	bLogObjectNames = Switches.ContainsItem(TEXT("LOGOBJECTNAMES"));
	bShowReferencers = Switches.ContainsItem(TEXT("SHOWREFERENCERS"));
	TotalStaticMeshActors = 0, TotalStaticLightActors = 0;
	TotalReferencedStaticMeshActors = 0, TotalReferencedStaticLightActors = 0;

	return Super::InitializeMapCheck(Tokens, Switches, MapPathNames);
}

/**
 * The main worker method - performs the commandlets tests on the package.
 *
 * @param	MapPackage	the current package to be processed
 *
 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
 */
INT UFindStaticActorsRefsCommandlet::CheckMapPackage( UPackage* MapPackage )
{
	INT Result = 0;

	// find all StaticMeshActors and static Light actors which are referenced by something in the map
	UWorld* World = FindObject<UWorld>( MapPackage, TEXT("TheWorld") );
	if ( World )
	{
		// make sure that the world's PersistentLevel is part of the levels array for the purpose of this test.
		World->Levels.AddUniqueItem(World->PersistentLevel);
		for ( INT LevelIndex = 0; LevelIndex < World->Levels.Num(); LevelIndex++ )
		{
			ULevel* Level = World->Levels(LevelIndex);

			// remove all StaticMeshActors from the level's Actor array so that we don't get false positives.
			TArray<AStaticMeshActor*> StaticMeshActors;
			if ( ContainsObjectOfClass<AActor>(Level->Actors, AStaticMeshActor::StaticClass(), FALSE, (TArray<AActor*>*)&StaticMeshActors) )
			{
				for ( INT i = 0; i < StaticMeshActors.Num(); i++ )
				{
					Level->Actors.RemoveItem(StaticMeshActors(i));
				}
			}

			// same for lights marked bStatic
			TArray<ALight*> Lights;
			if ( ContainsObjectOfClass<AActor>(Level->Actors, ALight::StaticClass(), FALSE, (TArray<AActor*>*)&Lights) )
			{
				for ( INT i = Lights.Num() - 1; i >= 0; i-- )
				{
					// only care about static lights - if the light is static, remove it from the level's Actors array
					// so that we don't get false positives; otherwise, remove it from the list of lights that we'll process
					if ( Lights(i)->IsStatic() )
					{
						Level->Actors.RemoveItem(Lights(i));
					}
					else
					{
						Lights.Remove(i);
					}
				}
			}

			// now use the object reference collector to find the static mesh actors that are still being referenced
			TArray<AStaticMeshActor*> ReferencedStaticMeshActors;
			TArchiveObjectReferenceCollector<AStaticMeshActor> SMACollector(&ReferencedStaticMeshActors, MapPackage, FALSE, TRUE, TRUE, TRUE);
			Level->Serialize( SMACollector );

			if ( ReferencedStaticMeshActors.Num() > 0 )
			{
				warnf(TEXT("\t%i of %i StaticMeshActors referenced"), ReferencedStaticMeshActors.Num(), StaticMeshActors.Num());
				if ( bShowReferencers )
				{
					TFindObjectReferencers<AStaticMeshActor> StaticMeshReferencers(ReferencedStaticMeshActors, MapPackage);

					for ( INT RefIndex = 0; RefIndex < ReferencedStaticMeshActors.Num(); RefIndex++ )
					{
						AStaticMeshActor* StaticMeshActor = ReferencedStaticMeshActors(RefIndex);
						debugf(TEXT("\t  %i) %s"), RefIndex, *StaticMeshActor->GetFullName());

						TArray<UObject*> Referencers;
						StaticMeshReferencers.MultiFind(StaticMeshActor, Referencers);

						INT Count=0;
						for ( INT ReferencerIndex = Referencers.Num() - 1; ReferencerIndex >= 0; ReferencerIndex-- )
						{
							if ( Referencers(ReferencerIndex) != StaticMeshActor->StaticMeshComponent )
							{
								debugf(TEXT("\t\t %i) %s"), Count, *Referencers(ReferencerIndex)->GetFullName());
								Count++;
							}
						}

						if ( Count == 0 )
						{
							debugf(TEXT("\t\t  (StaticMeshComponent referenced from external source)"));
						}

						debugf(TEXT(""));
					}

					debugf(TEXT("******"));
				}
				else if ( bShowObjectNames )
				{
					for ( INT RefIndex = 0; RefIndex < ReferencedStaticMeshActors.Num(); RefIndex++ )
					{
						warnf(TEXT("\t  %i) %s"), RefIndex, *ReferencedStaticMeshActors(RefIndex)->GetFullName());
					}

					warnf(TEXT(""));
				}
				else if ( bLogObjectNames )
				{
					for ( INT RefIndex = 0; RefIndex < ReferencedStaticMeshActors.Num(); RefIndex++ )
					{
						debugf(TEXT("\t  %i) %s"), RefIndex, *ReferencedStaticMeshActors(RefIndex)->GetFullName());
					}

					debugf(TEXT(""));
				}

				ReferencedStaticMeshActorMap.Set(MapIndex, ReferencedStaticMeshActors.Num());
				TotalReferencedStaticMeshActors += ReferencedStaticMeshActors.Num();
			}

			TArray<ALight*> ReferencedLights;
			TArchiveObjectReferenceCollector<ALight> LightCollector(&ReferencedLights, MapPackage, FALSE, TRUE, TRUE, TRUE);
			Level->Serialize( LightCollector );

			for ( INT RefIndex = ReferencedLights.Num() - 1; RefIndex >= 0; RefIndex-- )
			{
				if ( !ReferencedLights(RefIndex)->IsStatic() )
				{
					ReferencedLights.Remove(RefIndex);
				}
			}
			if ( ReferencedLights.Num() > 0 )
			{
				warnf(TEXT("\t%i of %i static Light actors referenced"), ReferencedLights.Num(), Lights.Num());
				if ( bShowReferencers )
				{
					TFindObjectReferencers<ALight> StaticLightReferencers(ReferencedLights, MapPackage);

					for ( INT RefIndex = 0; RefIndex < ReferencedLights.Num(); RefIndex++ )
					{
						ALight* StaticLightActor = ReferencedLights(RefIndex);
						debugf(TEXT("\t  %i) %s"), RefIndex, *StaticLightActor->GetFullName());

						TArray<UObject*> Referencers;
						StaticLightReferencers.MultiFind(StaticLightActor, Referencers);

						INT Count=0;
						UBOOL bShowSubobjects = FALSE;
LogLightReferencers:
						for ( INT ReferencerIndex = Referencers.Num() - 1; ReferencerIndex >= 0; ReferencerIndex-- )
						{
							if ( bShowSubobjects || !Referencers(ReferencerIndex)->IsIn(StaticLightActor) )
							{
								debugf(TEXT("\t\t %i) %s"), Count, *Referencers(ReferencerIndex)->GetFullName());
								Count++;
							}
						}

						if ( !bShowSubobjects && Count == 0 )
						{
							bShowSubobjects = TRUE;
							goto LogLightReferencers;
						}

						debugf(TEXT(""));
					}

					debugf(TEXT("******"));
				}
				else if ( bShowObjectNames )
				{
					for ( INT RefIndex = 0; RefIndex < ReferencedLights.Num(); RefIndex++ )
					{
						warnf(TEXT("\t  %i) %s"), RefIndex, *ReferencedLights(RefIndex)->GetFullName());
					}
					warnf(TEXT(""));
				}
				else if ( bLogObjectNames )
				{
					for ( INT RefIndex = 0; RefIndex < ReferencedLights.Num(); RefIndex++ )
					{
						debugf(TEXT("\t  %i) %s"), RefIndex, *ReferencedLights(RefIndex)->GetFullName());
					}

					debugf(TEXT(""));
				}

				ReferencedStaticLightActorMap.Set(MapIndex, ReferencedLights.Num());
				TotalReferencedStaticLightActors += ReferencedLights.Num();
			}

			if (!bShowObjectNames
				&&	(ReferencedStaticMeshActors.Num() > 0 || ReferencedLights.Num() > 0))
			{
				warnf(TEXT(""));
			}

			TotalStaticMeshActors += StaticMeshActors.Num();
			TotalStaticLightActors += Lights.Num();
		}
	}

	return Result;
}

IMPLEMENT_COMPARE_CONSTREF(INT,ReferencedStaticActorCount,
{
	return B - A;
})

/**
 * Called after all packages have been processed - provides commandlets with an opportunity to print out test results or
 * provide feedback.
 *
 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
 */
INT UFindStaticActorsRefsCommandlet::ProcessResults()
{
	INT Result = 0;

	// sort the list of maps by the number of referenceed static actors contained in them
	ReferencedStaticMeshActorMap.ValueSort<COMPARE_CONSTREF_CLASS(INT,ReferencedStaticActorCount)>();
	ReferencedStaticLightActorMap.ValueSort<COMPARE_CONSTREF_CLASS(INT,ReferencedStaticActorCount)>();

	warnf(LINE_TERMINATOR TEXT("Referenced StaticMeshActor Summary"));
	for ( TMap<INT,INT>::TIterator It(ReferencedStaticMeshActorMap); It; ++It )
	{
		warnf(TEXT("%-4i %s"), It.Value(), *MapNames(It.Key()));
	}

	warnf(LINE_TERMINATOR TEXT("Referenced Static Light Actor Summary"));
	for ( TMap<INT,INT>::TIterator It(ReferencedStaticLightActorMap); It; ++It )
	{
		warnf(TEXT("%-4i %s"), It.Value(), *MapNames(It.Key()));
	}

	warnf(LINE_TERMINATOR TEXT("Total static actors referenced across %i maps"), TotalMapsChecked);
	warnf(TEXT("StaticMeshActors: %i (of %i) across %i maps"), TotalReferencedStaticMeshActors, TotalStaticMeshActors, ReferencedStaticMeshActorMap.Num());
	warnf(TEXT("Static Light Actors: %i (of %i) across %i maps"), TotalReferencedStaticLightActors, TotalStaticLightActors, ReferencedStaticLightActorMap.Num());

	return Result;
}
IMPLEMENT_CLASS(UFindStaticActorsRefsCommandlet);

/* ==========================================================================================================
	UFindRenamedPrefabSequencesCommandlet
========================================================================================================== */
INT UFindRenamedPrefabSequencesCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}
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
INT UFindRenamedPrefabSequencesCommandlet::InitializeMapCheck( const TArray<FString>& Tokens, const TArray<FString>& Switches, TArray<FFilename>& MapPathNames )
{
	return Super::InitializeMapCheck(Tokens,Switches,MapPathNames);
}

/**
 * The main worker method - performs the commandlets tests on the package.
 *
 * @param	MapPackage	the current package to be processed
 *
 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
 */
INT UFindRenamedPrefabSequencesCommandlet::CheckMapPackage( UPackage* MapPackage )
{
	INT Result = 0;

	// find all StaticMeshActors and static Light actors which are referenced by something in the map
	UWorld* World = FindObject<UWorld>( MapPackage, TEXT("TheWorld") );
	if ( World )
	{
		// make sure that the world's PersistentLevel is part of the levels array for the purpose of this test.
		World->Levels.AddUniqueItem(World->PersistentLevel);
		for ( INT LevelIndex = 0; LevelIndex < World->Levels.Num(); LevelIndex++ )
		{
			ULevel* Level = World->Levels(LevelIndex);
			if ( Level != NULL )
			{
				USequence* LevelRootSequence = Level->GetGameSequence();
				if ( LevelRootSequence != NULL )
				{
					USequence* PrefabContainerSequence = LevelRootSequence->GetPrefabsSequence(FALSE);
					if ( PrefabContainerSequence != NULL )
					{
						const FString SeqPathName = PrefabContainerSequence->GetPathName();
						warnf(TEXT("Found prefab sequence container: %s"), *SeqPathName);

						if ( PrefabContainerSequence->ObjName != PREFAB_SEQCONTAINER_NAME )
						{
							RenamedPrefabSequenceContainers.AddItem(*SeqPathName);
						}
					}
				}
			}

			TArray<APrefabInstance*> PrefabInstances;
			if ( ContainsObjectOfClass<AActor>(Level->Actors, APrefabInstance::StaticClass(), FALSE, (TArray<AActor*>*)&PrefabInstances) )
			{
				for ( INT PrefIndex = 0; PrefIndex < PrefabInstances.Num(); PrefIndex++ )
				{
					APrefabInstance* Instance = PrefabInstances(PrefIndex);
					if ( Instance != NULL && Instance->SequenceInstance != NULL )
					{
						USequence* ParentSequence = Cast<USequence>(Instance->SequenceInstance->GetOuter());
						if ( ParentSequence != NULL && ParentSequence->ObjName != PREFAB_SEQCONTAINER_NAME )
						{
							const FString SeqPathName = ParentSequence->GetPathName();
							if ( !RenamedPrefabSequenceContainers.HasKey(*SeqPathName) )
							{
								warnf(TEXT("FOUND ORPHAN PREFAB SEQUENCE: %s"), *SeqPathName);
								RenamedPrefabSequenceContainers.AddItem(*SeqPathName);
							}
						}
					}
				}
			}
		}
	}

	return Result;
}

/**
 * Called after all packages have been processed - provides commandlets with an opportunity to print out test results or
 * provide feedback.
 *
 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
 */
INT UFindRenamedPrefabSequencesCommandlet::ProcessResults()
{
	INT Result = 0;

	if ( RenamedPrefabSequenceContainers.Num() > 0 )
	{
		warnf(TEXT("Found %i renamed prefab sequence containers."), RenamedPrefabSequenceContainers.Num());
		for ( INT PrefIndex = 0; PrefIndex < RenamedPrefabSequenceContainers.Num(); PrefIndex++ )
		{
			warnf(TEXT("    %i) %s"), PrefIndex, **RenamedPrefabSequenceContainers(PrefIndex));
		}
	}
	else
	{
		warnf(TEXT("No renamed prefab sequence containers found!"));
	}

	return Result;
}
IMPLEMENT_CLASS(UFindRenamedPrefabSequencesCommandlet);

/* ==========================================================================================================
	UDumpLightmapInfoCommandlet
========================================================================================================== */
INT UDumpLightmapInfoCommandlet::Main( const FString& Params )
{
	TArray<FString> Tokens, Switches, Unused;
	ParseCommandLine(*Params, Tokens, Switches);

	// assume the first token is the map wildcard/pathname
	FString MapWildcard = Tokens.Num() > 0 ? Tokens(0) : FString(TEXT("*.")) + FURL::DefaultMapExt;
	TArray<FFilename> MapNames;
	NormalizePackageNames( Unused, MapNames, MapWildcard, NORMALIZE_ExcludeContentPackages);

	INT GCIndex=0;
	INT TotalMapsChecked=0;

	// Get time as a string
	FString CurrentTime = appSystemTimeString();

	for ( INT MapIndex = 0; MapIndex < MapNames.Num(); MapIndex++ )
	{
		const FFilename& Filename = MapNames(MapIndex);
		warnf( TEXT("Loading  %s...  (%i / %i)"), *Filename, MapIndex, MapNames.Num() );

		// NOTE: This check for autosaves maps assumes that there will never be a map
		// or directory that holds a map with the 'autosaves' sub-string in it!
		if (Filename.GetPath().InStr(TEXT("Autosaves"), FALSE, TRUE) != -1)
		{
			warnf(NAME_Log, TEXT("Skipping autosave map %s"), *Filename);
			continue;
		}

		UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_NoWarn );
		if ( Package == NULL )
		{
			warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
			continue;
		}

		// skip packages in the trashcan or PIE packages
		if ( (Package->PackageFlags&(PKG_Trash|PKG_PlayInEditor)) != 0 )
		{
			warnf(TEXT("Skipping %s (%s)"), *Filename, (Package->PackageFlags&PKG_Trash) != 0 ? TEXT("Trashcan Map") : TEXT("PIE Map"));
			UObject::CollectGarbage(RF_Native);
			continue;
		}

		// Find the world and load all referenced levels.
		UWorld* World = FindObjectChecked<UWorld>( Package, TEXT("TheWorld") );
		if( World )
		{
			AWorldInfo* WorldInfo	= World->GetWorldInfo();
			// Iterate over streaming level objects loading the levels.
			for( INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++ )
			{
				ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
				if( StreamingLevel )
				{
					// Load package if found.
					FString Filename;
					if( GPackageFileCache->FindPackageFile( *StreamingLevel->PackageName.ToString(), NULL, Filename ) )
					{
						warnf(NAME_Log, TEXT("\tLoading sub-level %s"), *Filename);
						LoadPackage( NULL, *Filename, LOAD_None );
					}
				}
			}
		}
		else
		{
			warnf( NAME_Error, TEXT("Can't find TheWorld in %s!"), *Filename );
			continue;
		}

		TotalMapsChecked++;

		warnf(TEXT("Checking %s..."), *Filename);

		FString		CSVDirectory	= appGameLogDir() + TEXT("MapLightMapData") PATH_SEPARATOR;
		FString		CSVFilename		= TEXT("");
		FArchive*	CSVFile			= NULL;

		// Create CSV folder in case it doesn't exist yet.
		GFileManager->MakeDirectory( *CSVDirectory );

		// CSV: Human-readable spreadsheet format.
		CSVFilename	= FString::Printf(TEXT("%s%s-%s-%i-%s.csv"), *CSVDirectory, *(Filename.GetBaseFilename()), GGameName, GetChangeListNumberForPerfTesting(), *CurrentTime);
		CSVFile		= GFileManager->CreateFileWriter( *CSVFilename );
		if (CSVFile == NULL)
		{
			warnf(NAME_Log, TEXT("\t\tFailed to create CSV data file... check log for results!"));
		}
		else
		{
			// Write out header row.
			FString HeaderRow = FString::Printf(TEXT("Component,Resource,Lightmap Width, Lightmap Height, Lightmap (kB)%s"),LINE_TERMINATOR);
			CSVFile->Serialize( TCHAR_TO_ANSI( *HeaderRow ), HeaderRow.Len() );
		}

		// Iterate over all primitive components.
		for( TObjectIterator<UPrimitiveComponent> It; It; ++It )
		{
			UPrimitiveComponent*	PrimitiveComponent		= *It;
			UStaticMeshComponent*	StaticMeshComponent		= Cast<UStaticMeshComponent>(*It);
			UModelComponent*		ModelComponent			= Cast<UModelComponent>(*It);
			USkeletalMeshComponent*	SkeletalMeshComponent	= Cast<USkeletalMeshComponent>(*It);
			UTerrainComponent*		TerrainComponent		= Cast<UTerrainComponent>(*It);
			USpeedTreeComponent*	SpeedTreeComponent		= Cast<USpeedTreeComponent>(*It);
			UObject*				Resource				= NULL;
			AActor*					ActorOuter				= Cast<AActor>(PrimitiveComponent->GetOuter());

			// The static mesh is a static mesh component's resource.
			if( StaticMeshComponent )
			{
				Resource = StaticMeshComponent->StaticMesh;
			}
			// A model component is its own resource.
			else if( ModelComponent )			
			{
				// Make sure model component is referenced by level.
				ULevel* Level = CastChecked<ULevel>(ModelComponent->GetOuter());
				if( Level->ModelComponents.FindItemIndex( ModelComponent ) != INDEX_NONE )
				{
					Resource = ModelComponent;
				}
			}
			// The skeletal mesh of a skeletal mesh component is its resource.
			else if( SkeletalMeshComponent )
			{
				Resource = SkeletalMeshComponent->SkeletalMesh;
			}
			// A terrain component's resource is the terrain actor.
			else if( TerrainComponent )
			{
				Resource = TerrainComponent->GetTerrain();
			}
			// The speed tree actor of a speed tree component is its resource.
			else if( SpeedTreeComponent )
			{
				Resource = SpeedTreeComponent->SpeedTree;
			}

			// Dont' care about components without a resource.
			if(	!Resource )
			{
				continue;
			}

			// Require actor association for selection and to disregard mesh emitter components. The exception being model components.
			if (!(ActorOuter || ModelComponent))
			{
				continue;
			}

			// Don't list pending kill components.
			if (PrimitiveComponent->IsPendingKill())
			{
				continue;
			}

			// Figure out memory used by light and shadow maps and light/ shadow map resolution.
			INT LightMapWidth	= 0;
			INT LightMapHeight	= 0;
			PrimitiveComponent->GetLightMapResolution( LightMapWidth, LightMapHeight );
			INT LMSMResolution	= appSqrt( LightMapHeight * LightMapWidth );
			INT LightMapData	= 0;
			INT ShadowMapData	= 0;
			PrimitiveComponent->GetLightAndShadowMapMemoryUsage( LightMapData, ShadowMapData );

			if ((LightMapWidth != 0) && (LightMapHeight != 0))
			{
				warnf(TEXT("%4dx%4d - %s (%s)"),
					LightMapWidth, LightMapHeight, *(PrimitiveComponent->GetName()), *(Resource->GetFullName()));
				if( CSVFile )
				{	
					FString HeaderRow = FString::Printf(TEXT("Component,Resource,Lightmap Width, Lightmap Height, Lightmap (kB)"));
					FString OutString = FString::Printf(TEXT("%s,%s,%d,%d,%d%s"),
						*(PrimitiveComponent->GetName()), *(Resource->GetFullName()),
						LightMapWidth, LightMapHeight, LightMapData, LINE_TERMINATOR);
					CSVFile->Serialize( TCHAR_TO_ANSI( *OutString ), OutString.Len() );
				}
			}
		}

		if (CSVFile)
		{
			// Close and delete archive.
			CSVFile->Close();
			delete CSVFile;
		}

		// collecting garbage every N maps instead of every map makes the commandlet run much faster
		if( (++GCIndex % 1) == 0 )
		{
			UObject::CollectGarbage(RF_Native);
		}
	}

	return 0;
}
IMPLEMENT_CLASS(UDumpLightmapInfoCommandlet);

/* ==========================================================================================================
	UPerformTerrainMaterialDumpCommandlet
========================================================================================================== */
INT UPerformTerrainMaterialDumpCommandlet::Main( const FString& Params )
{
	TArray<FString> Tokens, Switches, Unused;
	ParseCommandLine(*Params, Tokens, Switches);

	// assume the first token is the map wildcard/pathname
	FString MapWildcard = Tokens.Num() > 0 ? Tokens(0) : FString(TEXT("*.")) + FURL::DefaultMapExt;
	TArray<FFilename> MapNames;
	NormalizePackageNames( Unused, MapNames, MapWildcard, NORMALIZE_ExcludeContentPackages);

	INT GCIndex = 0;
	INT TotalMapsChecked = 0;
	for (INT MapIndex = 0; MapIndex < MapNames.Num(); MapIndex++)
	{
		const FFilename& Filename = MapNames(MapIndex);
		warnf( TEXT("Loading  %s...  (%i / %i)"), *Filename, MapIndex, MapNames.Num() );

		UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_NoWarn );
		if ( Package == NULL )
		{
			warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
			continue;
		}

		// skip packages in the trashcan or PIE packages
		if ( (Package->PackageFlags&(PKG_Trash|PKG_PlayInEditor)) != 0 )
		{
			warnf(TEXT("Skipping %s (%s)"), *Filename, (Package->PackageFlags&PKG_Trash) != 0 ? TEXT("Trashcan Map") : TEXT("PIE Map"));
			UObject::CollectGarbage(RF_Native);
			continue;
		}

		TotalMapsChecked++;

		warnf(TEXT("Checking %s..."), *Filename);
		for (TObjectIterator<ATerrain> It; It; ++It )
		{
			ATerrain* Terrain = *It;
			if (Terrain->IsIn(Package) && !Terrain->IsTemplate())
			{
				warnf(TEXT("\tTerrain %s"), *(Terrain->GetFullName()));

				INT ComponentIndex;

				TArray<FTerrainMaterialMask> BatchMaterials;
				TArray<FTerrainMaterialMask> FullMaterials;

				warnf(TEXT("\t\tComponent Count = %d"), Terrain->TerrainComponents.Num());

				UBOOL bIsTerrainMaterialResourceInstance;
				FMaterialRenderProxy* MaterialRenderProxy;
				const FMaterial* Material;

				warnf(TEXT("\t\t\tFullBatches = %d"), FullMaterials.Num());
				for (INT FullIndex = 0; FullIndex < FullMaterials.Num(); FullIndex++)
				{
					MaterialRenderProxy = Terrain->GetCachedMaterial(FullMaterials(FullIndex), bIsTerrainMaterialResourceInstance);
					Material = MaterialRenderProxy ? MaterialRenderProxy->GetMaterial() : NULL;
					warnf(TEXT("\t\t\t\t%s"), Material ? *(Material->GetFriendlyName()) : TEXT("NO MATERIAL"));
				}

				warnf(TEXT("\t\t\tBatches = %d"), BatchMaterials.Num());
				for (INT BatchIndex = 0; BatchIndex < BatchMaterials.Num(); BatchIndex++)
				{
					MaterialRenderProxy = Terrain->GetCachedMaterial(BatchMaterials(BatchIndex), bIsTerrainMaterialResourceInstance);
					Material = MaterialRenderProxy ? MaterialRenderProxy->GetMaterial() : NULL;
					warnf(TEXT("\t\t\t\t%s"), Material ? *(Material->GetFriendlyName()) : TEXT("NO MATERIAL"));
				}

				warnf(TEXT("\t\t\tComponent Batch Usage"));
				for (ComponentIndex = 0; ComponentIndex < Terrain->TerrainComponents.Num(); ComponentIndex++)
				{
					UTerrainComponent* Comp = Terrain->TerrainComponents(ComponentIndex);
					if (Comp)
					{
						warnf(TEXT("\t\t\t\tComponent %d"), ComponentIndex);

						MaterialRenderProxy = Terrain->GetCachedMaterial(Comp->BatchMaterials(Comp->FullBatch), bIsTerrainMaterialResourceInstance);
						Material = MaterialRenderProxy ? MaterialRenderProxy->GetMaterial() : NULL;
						warnf(TEXT("\t\t\t\t\t%4d with FULL BATCH %s"), 
							Comp->GetMaxTriangleCount(), 
							Material ? *(Material->GetFriendlyName()) : TEXT("NO MATERIAL"));

					}
				}
			}
		}

		// collecting garbage every 10 maps instead of every map makes the commandlet run much faster
		if( (++GCIndex % 10) == 0 )
		{
			UObject::CollectGarbage(RF_Native);
		}
	}

	return 0;
}

IMPLEMENT_CLASS(UPerformTerrainMaterialDumpCommandlet);

/* ==========================================================================================================
	UListPSysFixedBoundSettingCommandlet
========================================================================================================== */
INT UListPSysFixedBoundSettingCommandlet::Main( const FString& Params )
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	const TCHAR* Parms = *Params;
	ParseCommandLine(Parms, Tokens, Switches);

	const UBOOL bLoadAllPackages = Switches.FindItemIndex(TEXT("ALL")) != INDEX_NONE;
	TArray<FString> FilesInPath;
	if ( bLoadAllPackages )
	{
		FilesInPath = GPackageFileCache->GetPackageFileList();
	}
	else
	{
		for ( INT i = 0; i < Tokens.Num(); i++ )
		{
			FString	PackageWildcard = Tokens(i);	

			GFileManager->FindFiles( FilesInPath, *PackageWildcard, TRUE, FALSE );
			if( FilesInPath.Num() == 0 )
			{
				// if no files were found, it might be an unqualified path; try prepending the .u output path
				// if one were going to make it so that you could use unqualified paths for package types other
				// than ".u", here is where you would do it
				GFileManager->FindFiles( FilesInPath, *(appScriptOutputDir() * PackageWildcard), 1, 0 );

				if ( FilesInPath.Num() == 0 )
				{
					TArray<FString> Paths;
					if ( GConfig->GetArray( TEXT("Core.System"), TEXT("Paths"), Paths, GEngineIni ) > 0 )
					{
						for ( INT j = 0; j < Paths.Num(); j++ )
						{
							GFileManager->FindFiles( FilesInPath, *(Paths(j) * PackageWildcard), 1, 0 );
						}
					}
				}
				else
				{
					// re-add the path information so that GetPackageLinker finds the correct version of the file.
					FFilename WildcardPath = appScriptOutputDir() * PackageWildcard;
					for ( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
					{
						FilesInPath(FileIndex) = WildcardPath.GetPath() * FilesInPath(FileIndex);
					}
				}

				// Try finding package in package file cache.
				if ( FilesInPath.Num() == 0 )
				{
					FString Filename;
					if( GPackageFileCache->FindPackageFile( *PackageWildcard, NULL, Filename ) )
					{
						new(FilesInPath)FString(Filename);
					}
				}
			}
			else
			{
				// re-add the path information so that GetPackageLinker finds the correct version of the file.
				FFilename WildcardPath = PackageWildcard;
				for ( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
				{
					FilesInPath(FileIndex) = WildcardPath.GetPath() * FilesInPath(FileIndex);
				}
			}
		}
	}

	if( FilesInPath.Num() == 0 )
	{
		warnf(NAME_Warning,TEXT("No packages found matching '%s'"), Parms);
		return 1;
	}

	INT GCIndex = 0;
	INT TotalMapsChecked = 0;
	for (INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++)
	{
		const FFilename& Filename = FilesInPath(FileIndex);

		// we don't care about trying to load the various shader caches so just skipz0r them
		if ((Filename.InStr( TEXT("LocalShaderCache") ) != INDEX_NONE) || 
			(Filename.InStr( TEXT("RefShaderCache") ) != INDEX_NONE))
		{
			continue;
		}

		warnf( TEXT("Loading  %s...  (%i / %i)"), *Filename, FileIndex, FilesInPath.Num() );

		UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_NoWarn );
		if ( Package == NULL )
		{
			warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
			continue;
		}

		// skip packages in the trashcan or PIE packages
		if ( (Package->PackageFlags&(PKG_Trash|PKG_PlayInEditor)) != 0 )
		{
			warnf(TEXT("Skipping %s (%s)"), *Filename, (Package->PackageFlags&PKG_Trash) != 0 ? TEXT("Trashcan Map") : TEXT("PIE Map"));
			UObject::CollectGarbage(RF_Native);
			continue;
		}

		TotalMapsChecked++;

		warnf(TEXT("%s"), *Filename);
		for (TObjectIterator<UParticleSystem> It; It; ++It )
		{
			UBOOL bFirstOne = TRUE;
			UParticleSystem* PSys = *It;
			if (PSys->IsIn(Package) && !PSys->IsTemplate())
			{
				warnf(TEXT("\t%s,%s,%f,%f,%f,%f,%f,%f"), *(PSys->GetFullName()), 
					PSys->bUseFixedRelativeBoundingBox ? TEXT("ENABLED") : TEXT("disabled"),
					PSys->FixedRelativeBoundingBox.Max.X,
					PSys->FixedRelativeBoundingBox.Max.Y,
					PSys->FixedRelativeBoundingBox.Max.Z,
					PSys->FixedRelativeBoundingBox.Min.X,
					PSys->FixedRelativeBoundingBox.Min.Y,
					PSys->FixedRelativeBoundingBox.Min.Z);
			}
		}

		// collecting garbage every 10 maps instead of every map makes the commandlet run much faster
		if( (++GCIndex % 10) == 0 )
		{
			UObject::CollectGarbage(RF_Native);
		}
	}

	return 0;
}

IMPLEMENT_CLASS(UListPSysFixedBoundSettingCommandlet);


/* ==========================================================================================================
	UListEmittersUsingModuleCommandlet
========================================================================================================== */
INT UListEmittersUsingModuleCommandlet::Main( const FString& Params )
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	const TCHAR* Parms = *Params;
	ParseCommandLine(Parms, Tokens, Switches);

	const UBOOL bLoadAllPackages = Switches.FindItemIndex(TEXT("ALL")) != INDEX_NONE;
	TArray<FString> FilesInPath;
	if ( bLoadAllPackages )
	{
		FilesInPath = GPackageFileCache->GetPackageFileList();
	}
	else
	{
		for ( INT i = 0; i < Tokens.Num(); i++ )
		{
			FString	PackageWildcard = Tokens(i);	

			GFileManager->FindFiles( FilesInPath, *PackageWildcard, TRUE, FALSE );
			if( FilesInPath.Num() == 0 )
			{
				// if no files were found, it might be an unqualified path; try prepending the .u output path
				// if one were going to make it so that you could use unqualified paths for package types other
				// than ".u", here is where you would do it
				GFileManager->FindFiles( FilesInPath, *(appScriptOutputDir() * PackageWildcard), 1, 0 );

				if ( FilesInPath.Num() == 0 )
				{
					TArray<FString> Paths;
					if ( GConfig->GetArray( TEXT("Core.System"), TEXT("Paths"), Paths, GEngineIni ) > 0 )
					{
						for ( INT j = 0; j < Paths.Num(); j++ )
						{
							GFileManager->FindFiles( FilesInPath, *(Paths(j) * PackageWildcard), 1, 0 );
						}
					}
				}
				else
				{
					// re-add the path information so that GetPackageLinker finds the correct version of the file.
					FFilename WildcardPath = appScriptOutputDir() * PackageWildcard;
					for ( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
					{
						FilesInPath(FileIndex) = WildcardPath.GetPath() * FilesInPath(FileIndex);
					}
				}

				// Try finding package in package file cache.
				if ( FilesInPath.Num() == 0 )
				{
					FString Filename;
					if( GPackageFileCache->FindPackageFile( *PackageWildcard, NULL, Filename ) )
					{
						new(FilesInPath)FString(Filename);
					}
				}
			}
			else
			{
				// re-add the path information so that GetPackageLinker finds the correct version of the file.
				FFilename WildcardPath = PackageWildcard;
				for ( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
				{
					FilesInPath(FileIndex) = WildcardPath.GetPath() * FilesInPath(FileIndex);
				}
			}
		}
	}

	if( FilesInPath.Num() == 0 )
	{
		warnf(NAME_Warning,TEXT("No packages found matching '%s'"), Parms);
		return 1;
	}

	FString RequestedModuleName;
	RequestedModuleName.Empty();
	for (INT ModuleNameIndex = 0; ModuleNameIndex < Switches.Num(); ModuleNameIndex++)
	{
		INT ModuleNameOffset = Switches(ModuleNameIndex).InStr(TEXT("M="));
		if (ModuleNameOffset != -1)
		{
			RequestedModuleName = Switches(ModuleNameIndex);
			RequestedModuleName = RequestedModuleName.Right(RequestedModuleName.Len() - 2);
			break;
		}
	}

	if (RequestedModuleName.Len() == 0)
	{
		return -1;
	}

	FString ModuleName(TEXT("ParticleModule"));
	ModuleName += RequestedModuleName;

	warnf( TEXT("Looking for module %s..."), *ModuleName );

	INT GCIndex = 0;
	INT TotalMapsChecked = 0;
	for (INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++)
	{
		const FFilename& Filename = FilesInPath(FileIndex);

		// we don't care about trying to load the various shader caches so just skipz0r them
		if ((Filename.InStr( TEXT("LocalShaderCache") ) != INDEX_NONE) || 
			(Filename.InStr( TEXT("RefShaderCache") ) != INDEX_NONE))
		{
			continue;
		}

		warnf( TEXT("Loading  %s...  (%i / %i)"), *Filename, FileIndex, FilesInPath.Num() );

		UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_NoWarn );
		if ( Package == NULL )
		{
			warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
			continue;
		}

		// skip packages in the trashcan or PIE packages
		if ( (Package->PackageFlags&(PKG_Trash|PKG_PlayInEditor)) != 0 )
		{
			warnf(TEXT("Skipping %s (%s)"), *Filename, (Package->PackageFlags&PKG_Trash) != 0 ? TEXT("Trashcan Map") : TEXT("PIE Map"));
			UObject::CollectGarbage(RF_Native);
			continue;
		}

		TotalMapsChecked++;

		warnf(TEXT("%s"), *Filename);
		for (TObjectIterator<UParticleSystem> It; It; ++It )
		{
			UBOOL bFirstOne = TRUE;
			UParticleSystem* PSys = *It;
			if (PSys->IsIn(Package) && !PSys->IsTemplate())
			{
				for (INT EmitterIndex = 0; EmitterIndex < PSys->Emitters.Num(); EmitterIndex++)
				{
					UParticleSpriteEmitter* SpriteEmitter = Cast<UParticleSpriteEmitter>(PSys->Emitters(EmitterIndex));
					if (SpriteEmitter)
					{
						if (SpriteEmitter->LODLevels.Num() > 0)
						{
							UParticleLODLevel* LODLevel = SpriteEmitter->LODLevels(0);
							if (LODLevel)
							{
								for (INT ModuleIndex = 0; ModuleIndex < LODLevel->Modules.Num(); ModuleIndex++)
								{
									UParticleModule* Module = LODLevel->Modules(ModuleIndex);
									if (Module && (Module->GetClass()->GetName() == ModuleName))
									{
										warnf(TEXT("\t%s,%d,"), *(PSys->GetFullName()), EmitterIndex);
									}
								}
							}
						}
					}
				}
			}
		}

		// collecting garbage every 10 maps instead of every map makes the commandlet run much faster
		if( (++GCIndex % 10) == 0 )
		{
			UObject::CollectGarbage(RF_Native);
		}
	}

	return 0;
}

IMPLEMENT_CLASS(UListEmittersUsingModuleCommandlet);




//======================================================================
// Commandlet for replacing one kind of actor with another kind of actor, copying changed properties from the most-derived common superclass
IMPLEMENT_CLASS(UReplaceActorCommandlet)

INT UReplaceActorCommandlet::Main(const FString& Params)
{
	const TCHAR* Parms = *Params;

// 	// get the specified filename/wildcard
// 	FString PackageWildcard;
// 	if (!ParseToken(Parms, PackageWildcard, 0))
// 	{
// 		warnf(TEXT("Syntax: replaceactor <file/wildcard> <Package.Class to remove> <Package.Class to replace with>"));
// 		return 1;
// 	}

	// find all the files matching the specified filename/wildcard
// 	TArray<FString> FilesInPath;
// 	GFileManager->FindFiles(FilesInPath, *PackageWildcard, 1, 0);
// 	if (FilesInPath.Num() == 0)
// 	{
// 		warnf(NAME_Error, TEXT("No packages found matching %s!"), *PackageWildcard);
// 		return 2;
// 	}

	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList;

	FString PackageWildcard;
	FString PackagePrefix;
// 	if(ParseToken(Parms,PackageWildcard,FALSE))
// 	{
// 		GFileManager->FindFiles(PackageList,*PackageWildcard,TRUE,FALSE);
// 		PackagePrefix = FFilename(PackageWildcard).GetPath() * TEXT("");
// 	}
// 	else
// 	{
		PackageList = GPackageFileCache->GetPackageFileList();
//	}
	if( !PackageList.Num() )
	{
		warnf( TEXT( "Found no packages to run UReplaceActorCommandlet on!" ) );
		return 0;
	}

	// get the directory part of the filename
	INT ChopPoint = Max(PackageWildcard.InStr(TEXT("/"), 1) + 1, PackageWildcard.InStr(TEXT("\\"), 1) + 1);
	if (ChopPoint < 0)
	{
		ChopPoint = PackageWildcard.InStr( TEXT("*"), 1 );
	}

	FString PathPrefix = (ChopPoint < 0) ? TEXT("") : PackageWildcard.Left(ChopPoint);

	// get the class to remove and the class to replace it with
	FString ClassName;
	if (!ParseToken(Parms, ClassName, 0))
	{
		warnf(TEXT("Syntax: replaceactor <file/wildcard> <Package.Class to remove> <Package.Class to replace with>"));
		return 1;
	}

	UClass* ClassToReplace = (UClass*)StaticLoadObject(UClass::StaticClass(), NULL, *ClassName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
	if (ClassToReplace == NULL)
	{
		warnf(NAME_Error, TEXT("Invalid class to remove: %s"), *ClassName);
		return 4;
	}
	else
	{
		ClassToReplace->AddToRoot();
	}

	if (!ParseToken(Parms, ClassName, 0))
	{
		warnf(TEXT("Syntax: replaceactor <file/wildcard> <Package.Class to remove> <Package.Class to replace with>"));
		return 1;
	}

	UClass* ReplaceWithClass = (UClass*)StaticLoadObject(UClass::StaticClass(), NULL, *ClassName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
	if (ReplaceWithClass == NULL)
	{
		warnf(NAME_Error, TEXT("Invalid class to replace with: %s"), *ClassName);
		return 5;
	}
	else
	{
		ReplaceWithClass->AddToRoot();
	}

	// find the most derived superclass common to both classes
	UClass* CommonSuperclass = NULL;
	for (UClass* BaseClass1 = ClassToReplace; BaseClass1 != NULL && CommonSuperclass == NULL; BaseClass1 = BaseClass1->GetSuperClass())
	{
		for (UClass* BaseClass2 = ReplaceWithClass; BaseClass2 != NULL && CommonSuperclass == NULL; BaseClass2 = BaseClass2->GetSuperClass())
		{
			if (BaseClass1 == BaseClass2)
			{
				CommonSuperclass = BaseClass1;
			}
		}
	}
	checkSlow(CommonSuperclass != NULL);

	const UBOOL bAutoCheckOut = TRUE; //ParseParam(appCmdLine(),TEXT("AutoCheckOutPackages"));
#if HAVE_SCC
	// Ensure source control is initialized and shut down properly
	FScopedSourceControl SourceControl;
#endif // HAVE_SCC

	for (INT i = 0; i < PackageList.Num(); i++)
	{

		const FString& PackageName = PackageList(i);
		// get the full path name to the file
		FFilename FileName = PathPrefix + PackageName;

		const UBOOL	bIsShaderCacheFile		= FString(*FileName).ToUpper().InStr( TEXT("SHADERCACHE") ) != INDEX_NONE;
		const UBOOL	bIsAutoSave				= FString(*FileName).ToUpper().InStr( TEXT("AUTOSAVES") ) != INDEX_NONE;

		// skip if read-only
//  		if( GFileManager->IsReadOnly(*FileName) )
//  		{
//  			warnf(TEXT("Skipping %s (read-only)"), *FileName);
//  			continue;
// 		}
// 		else 
			if( ( FileName.GetExtension() == TEXT( "u" ) )
				|| ( bIsShaderCacheFile == TRUE )
				|| ( bIsAutoSave == TRUE )
			//|| ( FileName.GetExtension() == TEXT( "upk" ) )
			)
		{
			warnf(TEXT("Skipping %s (non map)"), *FileName);
			continue;
		}
#if HAVE_SCC
		else if ( bAutoCheckOut && FSourceControl::ForceGetStatus( PackageName ) == SCC_NotCurrent )
		{
			warnf( NAME_Log, TEXT("Skipping %s (Not at head source control revision)"), *PackageName );
			continue;
		}
#endif
		else
		{
			// clean up any previous world
			if (GWorld != NULL)
			{
				GWorld->CleanupWorld();
				GWorld->RemoveFromRoot();
				GWorld = NULL;
			}

			// load the package
			warnf(TEXT("Loading %s..."), *FileName); 
			UPackage* Package = LoadPackage(NULL, *FileName, LOAD_None);

			// load the world we're interested in
			GWorld = FindObject<UWorld>(Package, TEXT("TheWorld"));

			// this is the case where .upk objects have class references (e.g. prefabs, animnodes, etc)
			if( GWorld == NULL )
			{
				warnf(TEXT("%s (not a map)"), *FileName);
				for( FObjectIterator It; It; ++It )
				{
					UObject* OldObject = *It;
					if( ( OldObject->GetOutermost() == Package )
						)
					{
						TMap<UClass*, UClass*> ReplaceMap;
						ReplaceMap.Set(ClassToReplace, ReplaceWithClass);
						FArchiveReplaceObjectRef<UClass> ReplaceAr(OldObject, ReplaceMap, FALSE, FALSE, FALSE);
						if( ReplaceAr.GetCount() > 0 )
						{
							warnf(TEXT("Replaced %i class references in an Object: %s"), ReplaceAr.GetCount(), *OldObject->GetName() );
							Package->MarkPackageDirty();
						}
					}
				}

				if( Package->IsDirty() == TRUE )
				{
					if( (GFileManager->IsReadOnly(*FileName)) && ( bAutoCheckOut == TRUE ) )
					{
#if HAVE_SCC
						FSourceControl::CheckOut(Package);
#endif // HAVE_SCC
					}

					warnf(TEXT("Saving %s..."), *FileName);
					SavePackage( Package, NULL, RF_Standalone, *FileName, GWarn );
				}
			}
			else
			{
				// need to have a bool so we dont' save every single map
				UBOOL bIsDirty = FALSE;

				// add the world to the root set so that the garbage collection to delete replaced actors doesn't garbage collect the whole world
				GWorld->AddToRoot();
				// initialize the levels in the world
				GWorld->Init();
				GWorld->GetWorldInfo()->PostEditChange();

				// iterate through all the actors in the world, looking for matches with the class to replace (must have exact match, not subclass)
				for (FActorIterator It; It; ++It)
				{
					AActor* OldActor = *It;
					if (OldActor->GetClass() == ClassToReplace)
					{
						// replace an instance of the old actor
						warnf(TEXT("Replacing actor %s"), *OldActor->GetName());
						// make sure we spawn the new actor in the same level as the old
						//@warning: this relies on the outer of an actor being the level
						GWorld->CurrentLevel = OldActor->GetLevel();
						checkSlow(GWorld->CurrentLevel != NULL);
						// spawn the new actor
						AActor* NewActor = GWorld->SpawnActor(ReplaceWithClass, NAME_None, OldActor->Location, OldActor->Rotation, NULL, TRUE);
						// copy non-native non-transient properties common to both that were modified in the old actor to the new actor
						for (UProperty* Property = CommonSuperclass->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext)
						{
							//@note: skipping properties containing components - don't have a reasonable way to deal with them and the instancing mess they create
							// to  hack around this for now you can do:
							// (!(Property->PropertyFlags & CPF_Component) || Property->GetFName() == FName(TEXT("Weapons"))) &&
							if ( !(Property->PropertyFlags & CPF_Native) && !(Property->PropertyFlags & CPF_Transient) &&
								!(Property->PropertyFlags & CPF_Component) &&
								!Property->Identical((BYTE*)OldActor + Property->Offset, (BYTE*)OldActor->GetClass()->GetDefaultObject() + Property->Offset) )
							{
								Property->CopyCompleteValue((BYTE*)NewActor + Property->Offset, (BYTE*)OldActor + Property->Offset);
								Package->MarkPackageDirty();
								bIsDirty = TRUE;
							}
						}

						// destroy the old actor
						//@warning: must do this before replacement so the new Actor doesn't get the old Actor's entry in the level's actor list (which would cause it to be in there twice)
						GWorld->DestroyActor(OldActor);
						check(OldActor->IsValid()); // make sure DestroyActor() doesn't immediately trigger GC since that'll break the reference replacement
						// check for any references to the old Actor and replace them with the new one
						TMap<AActor*, AActor*> ReplaceMap;
						ReplaceMap.Set(OldActor, NewActor);
						FArchiveReplaceObjectRef<AActor> ReplaceAr(GWorld, ReplaceMap, FALSE, FALSE, FALSE);
						if (ReplaceAr.GetCount() > 0)
						{
							warnf(TEXT("Replaced %i actor references in %s"), ReplaceAr.GetCount(), *It->GetName());
							Package->MarkPackageDirty();
							bIsDirty = TRUE;
						}
					}
					else
					{
						// check for any references to the old class and replace them with the new one
						TMap<UClass*, UClass*> ReplaceMap;
						ReplaceMap.Set(ClassToReplace, ReplaceWithClass);
						FArchiveReplaceObjectRef<UClass> ReplaceAr(*It, ReplaceMap, FALSE, FALSE, FALSE);
						if (ReplaceAr.GetCount() > 0)
						{
							warnf(TEXT("Replaced %i class references in actor %s"), ReplaceAr.GetCount(), *It->GetName());
							Package->MarkPackageDirty();
							bIsDirty = TRUE;
						}
					}
				}


				// replace Kismet references to the class
				USequence* Sequence = GWorld->GetGameSequence();
				if (Sequence != NULL)
				{
					TMap<UClass*, UClass*> ReplaceMap;
					ReplaceMap.Set(ClassToReplace, ReplaceWithClass);
					FArchiveReplaceObjectRef<UClass> ReplaceAr(Sequence, ReplaceMap, FALSE, FALSE, FALSE);
					if (ReplaceAr.GetCount() > 0)
					{
						warnf(TEXT("Replaced %i class references in Kismet"), ReplaceAr.GetCount());
						Package->MarkPackageDirty();
						bIsDirty = TRUE;
					}
				}

				// collect garbage to delete replaced actors and any objects only referenced by them (components, etc)
				GWorld->PerformGarbageCollection();

				// save the world
				if( ( Package->IsDirty() == TRUE ) && ( bIsDirty == TRUE ) )
				{
					if( (GFileManager->IsReadOnly(*FileName)) && ( bAutoCheckOut == TRUE ) )
					{
#if HAVE_SCC
						FSourceControl::CheckOut(Package);
#endif // HAVE_SCC
					}

					warnf(TEXT("Saving %s..."), *FileName);
					SavePackage(Package, GWorld, 0, *FileName, GWarn);
				}

				// clear GWorld by removing it from the root set and replacing it with a new one
				GWorld->CleanupWorld();
				GWorld->RemoveFromRoot();
				GWorld = NULL;
			}
		}

		// get rid of the loaded world
		warnf(TEXT("GCing..."));
		CollectGarbage(RF_Native);
	}

	// UEditorEngine::FinishDestroy() expects GWorld to exist
	UWorld::CreateNew();
	return 0;
}

/**
 *	ListSoundNodeWaves
 */
struct FListSoundNodeWaves_Entry
{
	FString Name;
	FString Path;
	INT Length;
};

IMPLEMENT_COMPARE_CONSTREF(FListSoundNodeWaves_Entry,UnPackageUtilities,{ return A.Length < B.Length ? 1 : -1; });

INT UListSoundNodeWavesCommandlet::Main( const FString& Params )
{
	TArray<FListSoundNodeWaves_Entry> NodeWaveList;
	INT TooLongCount = 0;

	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	const UBOOL bLoadAllPackages = Switches.FindItemIndex(TEXT("ALL")) != INDEX_NONE;
	TArray<FString> FilesInPath;
	if ( bLoadAllPackages )
	{
		FilesInPath = GPackageFileCache->GetPackageFileList();
	}
	else
	{
		for ( INT TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++ )
		{
			FString& PackageWildcard = Tokens(TokenIndex);

			GFileManager->FindFiles( FilesInPath, *PackageWildcard, TRUE, FALSE );
			if( FilesInPath.Num() == 0 )
			{
				// if no files were found, it might be an unqualified path; try prepending the .u output path
				// if one were going to make it so that you could use unqualified paths for package types other
				// than ".u", here is where you would do it
				GFileManager->FindFiles( FilesInPath, *(appScriptOutputDir() * PackageWildcard), 1, 0 );

				if ( FilesInPath.Num() == 0 )
				{
					TArray<FString> Paths;
					if ( GConfig->GetArray( TEXT("Core.System"), TEXT("Paths"), Paths, GEngineIni ) > 0 )
					{
						for ( INT i = 0; i < Paths.Num(); i++ )
						{
							GFileManager->FindFiles( FilesInPath, *(Paths(i) * PackageWildcard), 1, 0 );
						}
					}
				}
				else
				{
					// re-add the path information so that GetPackageLinker finds the correct version of the file.
					FFilename WildcardPath = appScriptOutputDir() * PackageWildcard;
					for ( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
					{
						FilesInPath(FileIndex) = WildcardPath.GetPath() * FilesInPath(FileIndex);
					}
				}

				// Try finding package in package file cache.
				if ( FilesInPath.Num() == 0 )
				{
					FString Filename;
					if( GPackageFileCache->FindPackageFile( *PackageWildcard, NULL, Filename ) )
					{
						new(FilesInPath)FString(Filename);
					}
				}
			}
			else
			{
				// re-add the path information so that GetPackageLinker finds the correct version of the file.
				FFilename WildcardPath = PackageWildcard;
				for ( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
				{
					FilesInPath(FileIndex) = WildcardPath.GetPath() * FilesInPath(FileIndex);
				}
			}

			if ( FilesInPath.Num() == 0 )
			{
				warnf(TEXT("No packages found using '%s'!"), *PackageWildcard);
				continue;
			}
		}
	}

	for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		const FString &Filename = FilesInPath(FileIndex);

		{
			// reset the loaders for the packages we want to load so that we don't find the wrong version of the file
			// (otherwise, attempting to run pkginfo on e.g. Engine.xxx will always return results for Engine.u instead)
			const FString& PackageName = FPackageFileCache::PackageFromPath(*Filename);
			UPackage* ExistingPackage = FindObject<UPackage>(NULL, *PackageName, TRUE);
			if ( ExistingPackage != NULL )
			{
				ResetLoaders(ExistingPackage);
			}
		}

		UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_NoWarn );
		if ( Package == NULL )
		{
			warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
			continue;
		}

		// skip packages in the trashcan or PIE packages
		UBOOL bIsAMapPackage = FindObject<UWorld>(Package, TEXT("TheWorld")) != NULL;
		if ( bIsAMapPackage || ((Package->PackageFlags&(PKG_Trash|PKG_PlayInEditor)) != 0 ))
		{
			warnf(TEXT("Skipping %s (%s)"), *Filename, (Package->PackageFlags&PKG_Trash) != 0 ? TEXT("Trashcan Map") : TEXT("PIE Map"));
			UObject::CollectGarbage(RF_Native);
			continue;
		}

		warnf(TEXT("%s"), *Filename);
		for (TObjectIterator<USoundNodeWave> It; It; ++It )
		{
			UBOOL bFirstOne = TRUE;
			USoundNodeWave* SndNodeWave = *It;
			if (SndNodeWave->IsIn(Package) && !SndNodeWave->IsTemplate())
			{
				GWarn->Logf( TEXT("\t%3d - %s"), SndNodeWave->GetName().Len(), *(SndNodeWave->GetName()) );

				FListSoundNodeWaves_Entry* NewEntry = new(NodeWaveList)FListSoundNodeWaves_Entry;
				check(NewEntry);
				NewEntry->Length = SndNodeWave->GetName().Len();
				NewEntry->Name = SndNodeWave->GetName();
				NewEntry->Path = SndNodeWave->GetPathName();

				if (NewEntry->Length > 27)
				{
					TooLongCount++;
				}
			}
		}

		UObject::CollectGarbage(RF_Native);
	}

	if (NodeWaveList.Num() > 0)
	{
		debugf(TEXT("Dumping all SoundNodeWaves"));
		debugf(TEXT("\t%4d out of %4d are too long!"), TooLongCount, NodeWaveList.Num());

		Sort<USE_COMPARE_CONSTREF(FListSoundNodeWaves_Entry,UnPackageUtilities)>(&(NodeWaveList(0)),NodeWaveList.Num());
		for (INT Index = 0; Index < NodeWaveList.Num(); Index++)
		{
			debugf(TEXT("\t%3d,%s,%s"), 
				NodeWaveList(Index).Length,
				*(NodeWaveList(Index).Name),
				*(NodeWaveList(Index).Path));
		}
	}
	else
	{
		warnf(TEXT("No sound node waves found!"));
	}
	return 0;
}
IMPLEMENT_CLASS(UListSoundNodeWavesCommandlet);

/*-----------------------------------------------------------------------------
	FindDarkDiffuseTextures commandlet.
-----------------------------------------------------------------------------*/
/**
 *	This will find materials/textures that have dark diffuse values...
 */
struct MaterialFindDarkDiffuseFunctor
{
	static INT PackageCount;

	void CleanUpGADTags(){}

	template< typename OBJECTYPE >
	void DoIt( UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches )
	{
		UFindDarkDiffuseTexturesCommandlet* FindDarkDiffuseCommandlet = Cast<UFindDarkDiffuseTexturesCommandlet>(Commandlet);
		check(FindDarkDiffuseCommandlet);

		// Do it...
		for( TObjectIterator<OBJECTYPE> It; It; ++It )
		{
			OBJECTYPE* Material = *It;
			if (Material->IsIn(Package) == FALSE)
			{
				continue;
			}

			if (FindDarkDiffuseCommandlet->ProcessMaterial(Material) == TRUE)
			{
				warnf(NAME_Log, TEXT("*** Found dark textures in %s"), *(Material->GetPathName()));
			}
		}

		PackageCount++;
		if (FindDarkDiffuseCommandlet->CollectionUpdateRate > 0)
		{
			if ((PackageCount % FindDarkDiffuseCommandlet->CollectionUpdateRate) == 0)
			{
				FindDarkDiffuseCommandlet->UpdateCollection();
			}
		}
	}
};

INT MaterialFindDarkDiffuseFunctor::PackageCount = 0;

/**
 *	Startup the commandlet
 */
void UFindDarkDiffuseTexturesCommandlet::Startup()
{
	DarkDiffuseCollectionName = TEXT("Dark Diffuse Textures");
	// Fetch the settings from the ini file, let the command-line override those
	MinimalBrightness = 0.40f;
	if (GConfig->GetFloat(TEXT("DarkTextures"), TEXT("MinimalBrightness"), MinimalBrightness, GEditorIni))
	{
		MinimalBrightness /= 100.0f;
	}
	// See if it was passed as a commandline parameter
	FString MinBright(TEXT(""));
	for (INT TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++)
	{
		FString& Token = Tokens(TokenIndex);
		if (Parse(*Token, TEXT("MINBRIGHT="), MinBright))
		{
			break;
		}
	}
	if (MinBright.Len() > 0)
	{
		MinimalBrightness = appAtof(*MinBright);
		MinimalBrightness /= 100.0f;
	}
	MinimalBrightness = Clamp<FLOAT>(MinimalBrightness, 0.0f, 1.0f);

	UBOOL bTemp;
	bIgnoreBlack = TRUE;
	if (GConfig->GetBool(TEXT("DarkTextures"), TEXT("bIgnoreBlack"), bTemp, GEditorIni))
	{
		bIgnoreBlack = bTemp;
	}
	if (Switches.ContainsItem(TEXT("ALLOWBLACK")) == TRUE)
	{
		bIgnoreBlack = FALSE;
	}

	bUseGrayScale = FALSE;
	if (GConfig->GetBool(TEXT("DarkTextures"), TEXT("bUseGrayScale"), bTemp, GEditorIni))
	{
		bUseGrayScale = bTemp;
	}
	if (Switches.ContainsItem(TEXT("GRAYSCALE")) == TRUE)
	{
		bUseGrayScale = TRUE;
	}

	//@todo. Currently assumes that the MinBright value is in gamma space!
	// The IsTextureDark function operates in linear space so convert!
	MinimalBrightness = appPow(MinimalBrightness, 2.2);

	// See if it was passed as a commandline parameter
	CollectionUpdateRate = 0;
	FString UpdateRate(TEXT(""));
	for (INT TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++)
	{
		FString& Token = Tokens(TokenIndex);
		if (Parse(*Token, TEXT("UPDATE="), UpdateRate))
		{
			break;
		}
	}
	if (UpdateRate.Len() > 0)
	{
		CollectionUpdateRate = appAtoi(*UpdateRate);
		warnf(NAME_Log, TEXT("Collection update rate set to %3d packages..."), CollectionUpdateRate);
	}

	// Create and setup the collection helper
	GADHelper = new FGADHelper();
	check(GADHelper);
	GADHelper->Initialize();
	if (Switches.ContainsItem(TEXT("CLEARCOLLECTION")))
	{
		GADHelper->ClearCollection(DarkDiffuseCollectionName, EGADCollection::Shared);
	}
}

/**
 *	Shutdown the commandlet
 */
void UFindDarkDiffuseTexturesCommandlet::Shutdown()
{
	delete GADHelper;
	GADHelper = NULL;
}

/**
 *	Update the collection with the current lists
 *
 *	@return	UBOOL		TRUE if successful, FALSE if not.
 */
UBOOL UFindDarkDiffuseTexturesCommandlet::UpdateCollection()
{
	if (GADHelper->UpdateCollection(DarkDiffuseCollectionName, EGADCollection::Shared, DarkTextures, NonDarkTextures) == TRUE)
	{
		// Clear the current lists...
		DarkTextures.Empty();
		NonDarkTextures.Empty();
		return TRUE;
	}
	return FALSE;
}

/**
 *	Check the given material for dark textures in the diffuse property chain
 *
 *	@param	InMaterialInterface		The material interface to check
 *
 *	@return	UBOOL					TRUE if dark textures were found, FALSE if not.
 */
UBOOL UFindDarkDiffuseTexturesCommandlet::ProcessMaterial(UMaterialInterface* InMaterialInterface)
{
	if (InMaterialInterface == NULL)
	{
		return FALSE;
	}

	UBOOL bHasDarkTextures = FALSE;
	TArray<UTexture*> TexturesInChain;
	if (InMaterialInterface->GetTexturesInPropertyChain(MP_DiffuseColor, TexturesInChain, NULL, NULL) == TRUE)
	{
		for (INT TextureIdx = 0; TextureIdx < TexturesInChain.Num(); TextureIdx++)
		{
			UTexture* CheckTexture = TexturesInChain(TextureIdx);
			if (CheckTexture != NULL)
			{
				FString CheckTextureName = CheckTexture->GetFullName();
				INT DummyIdx;
				if ((DarkTextures.FindItem(CheckTextureName, DummyIdx) == FALSE) &&
					(NonDarkTextures.FindItem(CheckTextureName, DummyIdx) == FALSE))
				{
					UTextureCube* CheckTextureCube = Cast<UTextureCube>(CheckTexture);
					if (CheckTextureCube != NULL)
					{
						FLOAT MinBrightness = 16.0f;
						FLOAT CheckBrightness = 0.0f;

#define CHECK_CUBE_FACE_MACRO(x)																					\
	if (x != NULL)																									\
	{																												\
		CheckBrightness = x->GetAverageBrightness(bIgnoreBlack, bUseGrayScale);										\
		MinBrightness = (CheckBrightness > 0.0f) ? Min<FLOAT>(MinBrightness, CheckBrightness) : MinBrightness;		\
	}
						CHECK_CUBE_FACE_MACRO(CheckTextureCube->FacePosX);
						CHECK_CUBE_FACE_MACRO(CheckTextureCube->FaceNegX);
						CHECK_CUBE_FACE_MACRO(CheckTextureCube->FacePosY);
						CHECK_CUBE_FACE_MACRO(CheckTextureCube->FaceNegY);
						CHECK_CUBE_FACE_MACRO(CheckTextureCube->FacePosZ);
						CHECK_CUBE_FACE_MACRO(CheckTextureCube->FaceNegZ);

						if (MinBrightness < MinimalBrightness)
						{
							debugfSlow(TEXT("^^^^ Texture is DARK: %5.3f for %s"), MinBrightness, *GetPathName());
							DarkTextures.AddUniqueItem(CheckTextureName);
							bHasDarkTextures = TRUE;
						}
						else
						{
							NonDarkTextures.AddUniqueItem(CheckTextureName);
						}
					}
					else
					{
						FLOAT AvgBrightness = CheckTexture->GetAverageBrightness(bIgnoreBlack, bUseGrayScale);
						if (AvgBrightness > 0.0f)
						{
							if (AvgBrightness < MinimalBrightness)
							{
								debugfSlow(TEXT("^^^^ Texture is DARK: %5.3f for %s"), AvgBrightness, *GetPathName());
								DarkTextures.AddUniqueItem(CheckTextureName);
								bHasDarkTextures = TRUE;
							}
							else
							{
								NonDarkTextures.AddUniqueItem(CheckTextureName);
							}
						}
						else
						{
							// We are skipping this texture - likely SourceArt wasn't present!
						}
					}
				}
			}
		}
	}
	return bHasDarkTextures;
}

INT UFindDarkDiffuseTexturesCommandlet::Main( const FString& Params )
{
	ParseCommandLine(*Params, Tokens, Switches);

	// Initialize the parameters
	Startup();

	// Reset the package count
	MaterialFindDarkDiffuseFunctor::PackageCount = 0;

	// Do it...
	DoActionToAllPackages<UMaterialInterface, MaterialFindDarkDiffuseFunctor>(this, Params);

	// Update the collection
	UpdateCollection();

	// Shut it down!
	Shutdown();

	return 0;
}
IMPLEMENT_CLASS(UFindDarkDiffuseTexturesCommandlet);

///////////////////////////////////
/*-----------------------------------------------------------------------------
	FindUniqueSpecularTextureMaterials commandlet.
-----------------------------------------------------------------------------*/
/**
 *	This will find materials/textures that have dark diffuse values...
 */
struct MaterialFindUniqueSpecularFunctor
{
	static INT PackageCount;

	void CleanUpGADTags(){}

	template< typename OBJECTYPE >
	void DoIt( UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches )
	{
		UFindUniqueSpecularTextureMaterialsCommandlet* FindUniqueSpecularTextureMaterials = Cast<UFindUniqueSpecularTextureMaterialsCommandlet>(Commandlet);
		check(FindUniqueSpecularTextureMaterials);

		// Do it...
		for( TObjectIterator<OBJECTYPE> It; It; ++It )
		{
			OBJECTYPE* Material = *It;
			if (Material->IsIn(Package) == FALSE)
			{
				continue;
			}

			if (FindUniqueSpecularTextureMaterials->ProcessMaterial(Material) == TRUE)
			{
				warnf(NAME_Log, TEXT("*** Found unique specular texture in %s"), *(Material->GetPathName()));
			}
		}

		PackageCount++;
		if (FindUniqueSpecularTextureMaterials->CollectionUpdateRate > 0)
		{
			if ((PackageCount % FindUniqueSpecularTextureMaterials->CollectionUpdateRate) == 0)
			{
				FindUniqueSpecularTextureMaterials->UpdateCollection();
			}
		}
	}
};

INT MaterialFindUniqueSpecularFunctor::PackageCount = 0;

/**
 *	Startup the commandlet
 */
void UFindUniqueSpecularTextureMaterialsCommandlet::Startup()
{
	// Fetch the settings from the ini file, let the command-line override those
	// See if it was passed as a commandline parameter
	CollectionUpdateRate = 0;
	bAllMaterials = FALSE;
	FString UpdateRate(TEXT(""));
	for (INT TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++)
	{
		FString& Token = Tokens(TokenIndex);
		if (Parse(*Token, TEXT("UPDATE="), UpdateRate))
		{
			break;
		}
	}
	if (UpdateRate.Len() > 0)
	{
		CollectionUpdateRate = appAtoi(*UpdateRate);
		warnf(NAME_Log, TEXT("Collection update rate set to %3d packages..."), CollectionUpdateRate);
	}

	// Check for all materials...
	if (Switches.ContainsItem(TEXT("ALLMATERIALS")) == TRUE)
	{
		bAllMaterials = TRUE;
	}
	warnf(NAME_Log, TEXT("Searching for %s materials..."), bAllMaterials ? TEXT("ALL") : TEXT("ENVIRONMENTAL"));

	UniqueSpecularTextureCollectionName = TEXT("Unique Specular Materials");
	GADHelper = new FGADHelper();
	check(GADHelper);
	GADHelper->Initialize();
	if (Switches.ContainsItem(TEXT("CLEARCOLLECTION")))
	{
		GADHelper->ClearCollection(UniqueSpecularTextureCollectionName, EGADCollection::Shared);
	}
}

/**
 *	Shutdown the commandlet
 */
void UFindUniqueSpecularTextureMaterialsCommandlet::Shutdown()
{
	delete GADHelper;
	GADHelper = NULL;
}

/**
 *	Update the collection with the current lists
 *
 *	@return	UBOOL		TRUE if successful, FALSE if not.
 */
UBOOL UFindUniqueSpecularTextureMaterialsCommandlet::UpdateCollection()
{
	check(GADHelper);
	if (GADHelper->UpdateCollection(UniqueSpecularTextureCollectionName, EGADCollection::Shared, UniqueSpecMaterials, NonUniqueSpecMaterials) == TRUE)
	{
		// Clear the current lists...
		UniqueSpecMaterials.Empty();
		NonUniqueSpecMaterials.Empty();
		return TRUE;
	}
	return FALSE;
}

/**
 *	Check the given material for dark textures in the diffuse property chain
 *
 *	@param	InMaterialInterface		The material interface to check
 *
 *	@return	UBOOL					TRUE if the material has unique specular textures, FALSE if not
 */
UBOOL UFindUniqueSpecularTextureMaterialsCommandlet::ProcessMaterial(UMaterialInterface* InMaterialInterface)
{
	if (InMaterialInterface == NULL)
	{
		return FALSE;
	}

	UBOOL bHasUniqueSpecular = FALSE;
	TArray<TArray<UTexture*>> TexturesInChains;

	TexturesInChains.Empty(MP_MAX);
	TexturesInChains.AddZeroed(MP_MAX);
	// Get the textures for ALL property chains...
	for (INT MPIdx = 0; MPIdx < MP_MAX; MPIdx++)
	{
		TArray<UTexture*>& FetchTexturesInChain = TexturesInChains(MPIdx);
		InMaterialInterface->GetTexturesInPropertyChain((EMaterialProperty)MPIdx, FetchTexturesInChain, NULL, NULL);
	}

	if (bAllMaterials == FALSE)
	{
		UBOOL bIsEnvironmental = FALSE;
		// We only want to check environmental materials in this case...
		TArray<UTexture*>& DiffuseChainTextures = TexturesInChains(MP_DiffuseColor);
		for (INT DiffuseTexIdx = 0; DiffuseTexIdx < DiffuseChainTextures.Num(); DiffuseTexIdx++)
		{
			UTexture* DiffuseTexture = DiffuseChainTextures(DiffuseTexIdx);
			if (DiffuseTexture != NULL)
			{
				if (DiffuseTexture->LODGroup == TEXTUREGROUP_World)
				{
					bIsEnvironmental = TRUE;
					break;
				}
			}
		}

		if (bIsEnvironmental == FALSE)
		{
			return FALSE;
		}
	}
	// For each texture found in the specular chain, see if it is in any other chain.
	for (INT SpecTexIdx = 0; SpecTexIdx < TexturesInChains(MP_SpecularColor).Num(); SpecTexIdx++)
	{
		UTexture* CheckSpecTexture = TexturesInChains(MP_SpecularColor)(SpecTexIdx);
		UBOOL bFoundInAnotherChain = FALSE;
		for (INT MPIdx = 0; MPIdx < MP_MAX; MPIdx++)
		{
			// Skip the specular chain...
			if (MPIdx == MP_SpecularColor)
			{
				continue;
			}

			// Check all the others
			INT DummyIdx;
			if (TexturesInChains(MPIdx).FindItem(CheckSpecTexture, DummyIdx) == TRUE)
			{
				bFoundInAnotherChain = TRUE;
			}
		}

		if (bFoundInAnotherChain == FALSE)
		{
			UBOOL bFoundInParent = TRUE;

			UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(InMaterialInterface);
			if (MaterialInstance && MaterialInstance->Parent)
			{
				UMaterialInterface* ParentInterface = MaterialInstance->Parent;
				// We need to make sure that the MI* is overriding the 
				// unique specular and not falsely reporting the parent
				TArray<UTexture*> IgnoredTextures;
				TArray<FName> CheckTextureParamNames;
				if (MaterialInstance->GetTexturesInPropertyChain(MP_SpecularColor, IgnoredTextures, &CheckTextureParamNames, NULL) == TRUE)
				{
					// Are there texture parameter names in the chain?
					if (CheckTextureParamNames.Num() > 0)
					{
						TArray<UTexture*> ParentTextures;
						// Are they actually being overriden on the MI*?
						if (ParentInterface->GetTexturesInPropertyChain(MP_SpecularColor, ParentTextures, NULL, NULL) == TRUE)
						{
							for (INT NameIdx = 0; NameIdx < CheckTextureParamNames.Num(); NameIdx++)
							{
								UTexture* CheckTexture;
								if (MaterialInstance->GetTextureParameterValue(CheckTextureParamNames(NameIdx), CheckTexture) == TRUE)
								{
									// Is the texture NOT in parent textures? 
									INT DummyIdx;
									if (ParentTextures.FindItem(CheckTexture, DummyIdx) == FALSE)
									{
										bFoundInParent = FALSE;
									}
								}
							}
						}
					}
				}
			}
			else
			{
				bFoundInParent = FALSE;
			}

			if (bFoundInParent == FALSE)
			{
				bHasUniqueSpecular = TRUE;
				UniqueSpecMaterials.AddUniqueItem(InMaterialInterface->GetFullName());
				break;
			}
		}
	}

	if (bHasUniqueSpecular == FALSE)
	{
		NonUniqueSpecMaterials.AddUniqueItem(InMaterialInterface->GetFullName());
	}

	return bHasUniqueSpecular;
}

INT UFindUniqueSpecularTextureMaterialsCommandlet::Main( const FString& Params )
{
	ParseCommandLine(*Params, Tokens, Switches);

	// Initialize the parameters
	Startup();

	// Reset the package count
	MaterialFindUniqueSpecularFunctor::PackageCount = 0;

	// Do it...
	DoActionToAllPackages<UMaterialInterface, MaterialFindUniqueSpecularFunctor>(this, Params);

	// Make sure to do a final update of the collection
	UpdateCollection();

	// Shut it down!
	Shutdown();

	return 0;
}
IMPLEMENT_CLASS(UFindUniqueSpecularTextureMaterialsCommandlet);

///////////////////////////////////
/*-----------------------------------------------------------------------------
	FindNeverStreamTextures commandlet.
-----------------------------------------------------------------------------*/
/**
 *	This will find textures tagged as NeverStream
 */
struct FindNeverStreamTexturesFunctor
{
	static INT PackageCount;

	void CleanUpGADTags(){}

	template< typename OBJECTYPE >
	void DoIt( UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches )
	{
		UFindNeverStreamTexturesCommandlet* FindNeverStreamTextures = Cast<UFindNeverStreamTexturesCommandlet>(Commandlet);
		check(FindNeverStreamTextures);

		// Do it...
		for( TObjectIterator<OBJECTYPE> It; It; ++It )
		{
			OBJECTYPE* Texture = *It;
			if (Texture->IsIn(Package) == FALSE)
			{
				continue;
			}

			FindNeverStreamTextures->ProcessTexture(Texture);
		}

		PackageCount++;
		if (FindNeverStreamTextures->CollectionUpdateRate > 0)
		{
			if ((PackageCount % FindNeverStreamTextures->CollectionUpdateRate) == 0)
			{
				FindNeverStreamTextures->UpdateCollection();
			}
		}
	}
};

INT FindNeverStreamTexturesFunctor::PackageCount = 0;

/**
 *	Startup the commandlet
 */
void UFindNeverStreamTexturesCommandlet::Startup()
{
	NeverStreamTextures.Empty();
	StreamTextures.Empty();

	// Fetch the settings from the ini file, let the command-line override those
	// See if it was passed as a commandline parameter
	CollectionUpdateRate = 10;
	FString UpdateRate(TEXT(""));
	for (INT TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++)
	{
		FString& Token = Tokens(TokenIndex);
		if (Parse(*Token, TEXT("UPDATE="), UpdateRate))
		{
			break;
		}
	}
	if (UpdateRate.Len() > 0)
	{
		CollectionUpdateRate = appAtoi(*UpdateRate);
		warnf(NAME_Log, TEXT("Collection update rate set to %3d packages..."), CollectionUpdateRate);
	}

	CollectionName = TEXT("Never Stream Textures");
	GADHelper = new FGADHelper();
	check(GADHelper);
	GADHelper->Initialize();
	if (Switches.ContainsItem(TEXT("CLEARCOLLECTION")))
	{
		GADHelper->ClearCollection(CollectionName, EGADCollection::Shared);
	}
}
/**
 *	Shutdown the commandlet
 */
void UFindNeverStreamTexturesCommandlet::Shutdown()
{
	delete GADHelper;
	GADHelper = NULL;
}

/**
 *	Update the collection with the current lists
 *
 *	@return	UBOOL		TRUE if successful, FALSE if not.
 */
UBOOL UFindNeverStreamTexturesCommandlet::UpdateCollection()
{
	check(GADHelper);
	if (GADHelper->UpdateCollection(CollectionName, EGADCollection::Shared, NeverStreamTextures, StreamTextures) == TRUE)
	{
		// Clear the current lists...
		NeverStreamTextures.Empty();
		StreamTextures.Empty();
		return TRUE;
	}
	return FALSE;
}

/**
 *	Check the texture for NeverStream
 *
 *	@param	InTexture		The texture to check
 *
 *	@return	UBOOL			TRUE if successful, FALSE if there was an error
 */
UBOOL UFindNeverStreamTexturesCommandlet::ProcessTexture(UTexture* InTexture)
{
	if (InTexture != NULL)
	{
		if (InTexture->NeverStream == TRUE)
		{
			NeverStreamTextures.AddUniqueItem(InTexture->GetFullName());
		}
		else
		{
			StreamTextures.AddUniqueItem(InTexture->GetFullName());
		}
		return TRUE;
	}

	return FALSE;
}

INT UFindNeverStreamTexturesCommandlet::Main( const FString& Params )
{
	ParseCommandLine(*Params, Tokens, Switches);

	// Initialize the parameters
	Startup();

	// Reset the package count
	FindNeverStreamTexturesFunctor::PackageCount = 0;

	// Do it...
	DoActionToAllPackages<UTexture, FindNeverStreamTexturesFunctor>(this, Params);

	// Make sure to do a final update of the collection
	UpdateCollection();

	// Shut it down!
	Shutdown();

	return 0;
}

IMPLEMENT_CLASS(UFindNeverStreamTexturesCommandlet);

/*-----------------------------------------------------------------------------
	FindDuplicateTextures commandlet.
-----------------------------------------------------------------------------*/
/**
 *	This will find materials/textures that have dark diffuse values...
 */
struct FindDuplicateTexturesFunctor
{
	void CleanUpGADTags(){}

	template< typename OBJECTYPE >
	void DoIt( UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches )
	{
		UFindDuplicateTexturesCommandlet* FindDuplicateTextures = Cast<UFindDuplicateTexturesCommandlet>(Commandlet);
		check(FindDuplicateTextures);

		// Do it...
		for (TObjectIterator<OBJECTYPE> It; It; ++It)
		{
			OBJECTYPE* Texture = *It;
			if (Texture->IsIn(Package) == FALSE)
			{
				continue;
			}

			// We are ignoring Thumbnail textures
			if (Texture->GetPathName().InStr(TEXT(":ThumbnailTexture"), FALSE, TRUE) == INDEX_NONE)
			{
				FindDuplicateTextures->ProcessTexture(Texture);
			}
		}
	}
};

/**
 *	Startup the commandlet
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UFindDuplicateTexturesCommandlet::Startup()
{
	// Available options:
	// SWITCHES:
	//	FIXUP - if specified, the duplicate textures will reduced to a single one
	//	EXACT - if specified, textures will be considered duplicate if all 
	//		settings match exactly (properties, etc.). Otherwise, only the source 
	//		art pixels and the SRGB settings are considered.
	//	CHECKOUTONLY - if specified, all packages that need to be touched will be 
	//		checked out only, but not fixed up. This is to allow the 32-bit version 
	//		to check-out needed packages, and the 64-bit version to fix them.
	//	SAVESHADERS - if specified, shader caches will be saved after each package
	//		is processed. (To save waiting for shader recompilations in the event
	//		of a crash such as out-of-memory.)
	// TOKENS:
	//	FIXUPLIST=<filename> - if specified, the fixup code will only be run on the 
	//		textures listed in the file. The format of the file must match that of 
	//		the generated CSV file that is created to report duplicates.
	//			#Textures,FirstTextureName,SecondTextureName,...
	//		When specified, the commandlet will NOT search for duplicate textures
	//		and will simply fixup those provided.
	//	PRIORITY=<filename> - if specified, a text file containing pathnames in 
	//		order of priority. This is the actual folder location on disk.
	//		For example:
	//			..\..\Engine\Content\EngineMaterials
	//			..\..\Engine\Content\Engine_MI_Shaders
	//			..\..\Engine\Content\EditorMaterials
	//			..\..\Engine\Content
	//			..\..\GearGame\Content\Environments
	//			..\..\GearGame\Content\Effects
	//			..\..\GearGame\Content\Interface
	//			..\..\GearGame\Content\COG\COG_Soldiers\COG_MarcusFenix
	//			..\..\GearGame\Content\COG
	//			..\..\GearGame\Content\Locust\Locust_Vehicles\Locust_Brumak
	//			..\..\GearGame\Content\Locust
	//	specifies EngineMaterials as the highest priority package, followed by other 
	//	specific Engine content packages and then any package found in Engine\Content.
	//	Content found in the GearGame Environments, Effects, and then Interface 
	//	folders is next in priority. Content in the specific pacakge COG_MarcusFenix
	//	followed by any COG content, etc.
	//
	//	Typical usage of this commandlet is expected to be:
	//			1. Run commandlet w/ or w/out the EXACT option to generate the CSV 
	//			   file of duplicates. Will be in the Logs\DuplicateTextures folder, 
	//			   named 
	//			       DuplicateTextures-<GAME>-<CHANGELIST>.csv 
	//			   or 
	//				   DuplicateTextures-<GAME>-<CHANGELIST>-EXACT.csv
	//			   depending on whether EXACT was specified or not.
	//			2. Edit and remove any textures you do not want fixed up from this 
	//			   file. (Typically, font textures)
	//			3. Run the commandlet specifying the edited CSV file as the 
	//			   FixupList, and specifying a Priority file to properly select the
	//			   'primary' texture.

	bFixup = (Switches.FindItemIndex(TEXT("FIXUP")) != INDEX_NONE);
	if (bFixup == TRUE)
	{
		warnf(NAME_Log, TEXT("*** Fixup mode is ENABLED!"));
	}

	bExactMatch = (Switches.FindItemIndex(TEXT("EXACT")) != INDEX_NONE);
	if (bExactMatch == TRUE)
	{
		warnf(NAME_Log, TEXT("*** Exact matching is ENABLED!"));
	}

	bCheckoutOnly = (Switches.FindItemIndex(TEXT("CHECKOUTONLY")) != INDEX_NONE);
	if (bCheckoutOnly == TRUE)
	{
		warnf(NAME_Log, TEXT("*** CHECKOUT ONLY is ENABLED!"));
	}
	
	bFixupOnly = FALSE;
	FixupFilename = TEXT("");
	for (INT TokenIdx = 0; TokenIdx < Tokens.Num(); TokenIdx++)
	{
		// Check for a fixup list file...
		if (Parse(*(Tokens(TokenIdx)), TEXT("FIXUPLIST="), FixupFilename))
		{
			// Found one - only run the fixup code on the given list
			warnf(TEXT("Fixup only on file %s"), *FixupFilename);
			bFixupOnly = TRUE;
			bFixup = TRUE;
		}

		// Check for a priority list file...
		if (Parse(*(Tokens(TokenIdx)), TEXT("PRIORITY="), PriorityFilename))
		{
			// Found one
			warnf(TEXT("Priority file name %s"), *PriorityFilename);
		}
	}

	if (bFixup == TRUE)
	{
		if (PriorityFilename.Len() == 0)
		{
			warnf(NAME_Warning, TEXT("A priority file is recommended when fixing up duplicates!"));
		}
	}

	// Setup the output directory
	CSVDirectory = appGameLogDir() + TEXT("DuplicateTextures") + PATH_SEPARATOR;

	return TRUE;
}

/**
 *	Shutdown the commandlet
 */
void UFindDuplicateTexturesCommandlet::Shutdown()
{
}

/**
 *	Process the given texture, adding it to a CRC-bucket
 *
 *	@param	InTexture		The texture to process
 */
void UFindDuplicateTexturesCommandlet::ProcessTexture(UTexture2D* InTexture)
{
	DWORD CRCValue;

	// Only process textures that have source art as that is what we compare
	// Generate a list of CRC-matching textures w/out examining any deeper
	if (InTexture->GetSourceArtCRC(CRCValue) == TRUE)
	{
		TArray<FString>* CheckMap = CRCToTextureNamesMap.Find(CRCValue);
		if (CheckMap == NULL)
		{
			TArray<FString> TempMap;
			CRCToTextureNamesMap.Set(CRCValue, TempMap);
			CheckMap = CRCToTextureNamesMap.Find(CRCValue);
		}
		check(CheckMap);
		CheckMap->AddUniqueItem(InTexture->GetPathName());
	}
}

/**
 *	Finds the true duplicates from the process textures...
 */
void UFindDuplicateTexturesCommandlet::FindTrueDuplicates()
{
	DWORD DuplicatesIndex = 0;

	// Check all the textures in each CRC-bucket for matches.
	for (TMap<DWORD,TArray<FString>>::TIterator CRCIt(CRCToTextureNamesMap); CRCIt; ++CRCIt)
	{
		DWORD CRCValue = CRCIt.Key();
		TArray<FString>& TextureNames = CRCIt.Value();

		if (TextureNames.Num() > 1)
		{
			// There are CRC-duplicates...
			debugfSlow(TEXT("CRC Value 0x%08x has %2d potential duplciates!"), CRCValue, TextureNames.Num());

			// Now we have to compare all the textures in this bucket.
			UBOOL bDone = FALSE;
			while (!bDone)
			{
				// For each CRC-bucket, check the first texture in the bucket vs. all the others.
				// Walk the list backwards, removing any duplicates of the first texture and placing them in a duplicate texture list.
				// Pop the first one out of the bucket and then repeat until the bucket is empty.
				// 
				UTexture2D* Texture1 = LoadObject<UTexture2D>(NULL, *(TextureNames(0)), NULL, LOAD_None, NULL);
				if (Texture1)
				{
					for (INT TextureIdx = TextureNames.Num() - 1; TextureIdx > 0; TextureIdx--)
					{
						UTexture2D* Texture2 = LoadObject<UTexture2D>(NULL, *(TextureNames(TextureIdx)), NULL, LOAD_None, NULL);
						if (Texture2)
						{
							{
								// First, check dimensions, SRGB setting and a pixel-by-pixel comparison of the source art
								if (Texture1->HasSameSourceArt(Texture2) == TRUE)
								{
									UBOOL bIsDuplicate = TRUE;
									if (bExactMatch == TRUE)
									{
										// If EXACT matching is enabled, then check all the relevant properties.

										// Helper macro for checking and reporting mismatches on textures...
										#define CHECK_AND_REPORT_TEXTURE_MISMATCH(val1, val2, txtr1, txtr2, str)							\
											{																								\
												if (val1 != val2)																			\
												{																							\
													FString LogOutput;																		\
													LogOutput += FString::Printf(TEXT("----- Mismatch due to %20s for %s, %s"),				\
																				str, *(txtr1->GetPathName()), *(txtr2->GetPathName()));		\
													debugf(*LogOutput);																		\
													bIsDuplicate = FALSE;																	\
												}																							\
											}
										// Check each property
										//@todo. Should just iterate over the properties here...
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->LODGroup, Texture2->LODGroup, Texture1, Texture2, TEXT("LODGroup"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->LODBias,  Texture2->LODBias, Texture1, Texture2, TEXT("LODBias"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->Filter,  Texture2->Filter, Texture1, Texture2, TEXT("Filter"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->RGBE,  Texture2->RGBE, Texture1, Texture2, TEXT("RGBE"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->CompressionNoAlpha,  Texture2->CompressionNoAlpha, Texture1, Texture2, TEXT("CompressionNoAlpha"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->CompressionNone,  Texture2->CompressionNone, Texture1, Texture2, TEXT("CompressionNone"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->CompressionFullDynamicRange,  Texture2->CompressionFullDynamicRange, Texture1, Texture2, TEXT("CompressionFullDynamicRange"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->NeverStream,  Texture2->NeverStream, Texture1, Texture2, TEXT("NeverStream"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->bDitherMipMapAlpha,  Texture2->bDitherMipMapAlpha, Texture1, Texture2, TEXT("bDitherMipMapAlpha"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->bPreserveBorderR,  Texture2->bPreserveBorderR, Texture1, Texture2, TEXT("bPreserveBorderR"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->bPreserveBorderG,  Texture2->bPreserveBorderG, Texture1, Texture2, TEXT("bPreserveBorderG"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->bPreserveBorderB,  Texture2->bPreserveBorderB, Texture1, Texture2, TEXT("bPreserveBorderB"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->bPreserveBorderA,  Texture2->bPreserveBorderA, Texture1, Texture2, TEXT("bPreserveBorderA"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->bNoTiling,  Texture2->bNoTiling, Texture1, Texture2, TEXT("bNoTiling"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->UnpackMin[0],  Texture2->UnpackMin[0], Texture1, Texture2, TEXT("UnpackMin[0]"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->UnpackMin[1],  Texture2->UnpackMin[1], Texture1, Texture2, TEXT("UnpackMin[1]"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->UnpackMin[2],  Texture2->UnpackMin[2], Texture1, Texture2, TEXT("UnpackMin[2]"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->UnpackMin[3],  Texture2->UnpackMin[3], Texture1, Texture2, TEXT("UnpackMin[3]"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->UnpackMax[0],  Texture2->UnpackMax[0], Texture1, Texture2, TEXT("UnpackMax[0]"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->UnpackMax[1],  Texture2->UnpackMax[1], Texture1, Texture2, TEXT("UnpackMax[1]"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->UnpackMax[2],  Texture2->UnpackMax[2], Texture1, Texture2, TEXT("UnpackMax[2]"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->UnpackMax[3],  Texture2->UnpackMax[3], Texture1, Texture2, TEXT("UnpackMax[3]"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->CompressionSettings,  Texture2->CompressionSettings, Texture1, Texture2, TEXT("CompressionSettings"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->AdjustBrightness,  Texture2->AdjustBrightness, Texture1, Texture2, TEXT("AdjustBrightness"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->AdjustBrightnessCurve,  Texture2->AdjustBrightnessCurve, Texture1, Texture2, TEXT("AdjustBrightnessCurve"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->AdjustVibrance,  Texture2->AdjustVibrance, Texture1, Texture2, TEXT("AdjustVibrance"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->AdjustSaturation,  Texture2->AdjustSaturation, Texture1, Texture2, TEXT("AdjustSaturation"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->AdjustRGBCurve,  Texture2->AdjustRGBCurve, Texture1, Texture2, TEXT("AdjustRGBCurve"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->AdjustHue,  Texture2->AdjustHue, Texture1, Texture2, TEXT("AdjustHue"));
										CHECK_AND_REPORT_TEXTURE_MISMATCH(Texture1->MipGenSettings,  Texture2->MipGenSettings, Texture1, Texture2, TEXT("MipGenSettings"));
									}

									if (bIsDuplicate == TRUE)
									{
										// We found a duplicate of the first texture.
										// Add it to the Duplicate list and remove it from the CRC-bucket being processed.
										TArray<FString>* DupArray = DuplicateTextures.Find(Texture1->GetPathName());
										if (DupArray == NULL)
										{
											TArray<FString> TempArray;
											DuplicateTextures.Set(Texture1->GetPathName(), TempArray);
											DupArray = DuplicateTextures.Find(Texture1->GetPathName());
										}
										check(DupArray);
										DupArray->AddUniqueItem(Texture2->GetPathName());
										// Found at least one match!
										debugf(TEXT("*****,MATCHING,%s,%s,%s,%s"), 
											*(Texture1->GetPathName()), 
											*(Texture2->GetPathName()),
											(Texture1->Format == Texture2->Format) ?		TEXT("      ") :	TEXT("Format"), 
											(Texture1->LODGroup == Texture2->LODGroup) ?	TEXT("         ") : TEXT("LODGroups"));
										TextureNames.Remove(TextureIdx);
									}
								}
							}
						}
					}
				}

				// Remove the lead texture from the bucket...
				TextureNames.Remove(0);
				if (TextureNames.Num() == 0)
				{
					bDone = TRUE;
				}
			}

			UObject::CollectGarbage(RF_Native);
		}
	}

	if (DuplicateTextures.Num() > 0)
	{
		// Log out any found duplicate textures into a CSV file.

		FArchive* CSVFile = NULL;
		// Create CSV folder in case it doesn't exist yet.
		GFileManager->MakeDirectory(*CSVDirectory);
		// CSV: Human-readable spreadsheet format.
		FString CSVFilename	= FString::Printf(TEXT("%s%s-%s-%i%s.csv"), 
			*CSVDirectory, TEXT("DuplicateTextures"), GGameName, GetChangeListNumberForPerfTesting(),
			bExactMatch ? TEXT("-EXACT") : TEXT(""));
		CSVFile	= GFileManager->CreateFileWriter(*CSVFilename);

		FString WriteString;
		if (CSVFile != NULL)
		{
			for (TMap<FString,TArray<FString>>::TIterator DupIt(DuplicateTextures); DupIt; ++DupIt)
			{
				FString& TextureName = DupIt.Key();
				TArray<FString>& Duplicates = DupIt.Value();
				WriteString = FString::Printf(TEXT("%d,"), Duplicates.Num());
				WriteString += TextureName;
				WriteString += TEXT(",");
				for (INT DupIdx = 0; DupIdx < Duplicates.Num(); DupIdx++)
				{
					WriteString += Duplicates(DupIdx);
					if (DupIdx + 1 < Duplicates.Num())
					{
						WriteString += TEXT(",");
					}
				}
				WriteString += TEXT("\n");
				CSVFile->Serialize(TCHAR_TO_ANSI(*WriteString), WriteString.Len());
			}
			WriteString += TEXT("\n");
			CSVFile->Serialize(TCHAR_TO_ANSI(*WriteString), WriteString.Len());

			// Close and delete archive.
			CSVFile->Close();
			delete CSVFile;
		}
	}
}

/**
 *	Sorting helper
 */
INT PackageUtilities_TextureReplacementSortFunction(const FTextureReplacementInfo& A, const FTextureReplacementInfo& B)
{
	// Priority values determine the winner...
	if (A.Priority > B.Priority)
	{
		return 1;
	}
	else if (A.Priority < B.Priority)
	{
		return -1; 
	}

	// They are equal priority... 
	FString TexturePathA = A.TexturePathName;
	FString TexturePathB = B.TexturePathName;

	INT PackageAndGroupNameIdx = TexturePathA.InStr(TEXT("."), TRUE);
	if (PackageAndGroupNameIdx != INDEX_NONE)
	{
		TexturePathA = TexturePathA.Left(PackageAndGroupNameIdx);
	}
	PackageAndGroupNameIdx = TexturePathB.InStr(TEXT("."), TRUE);
	if (PackageAndGroupNameIdx != INDEX_NONE)
	{
		TexturePathB = TexturePathB.Left(PackageAndGroupNameIdx);
	}

	// Is one 'internal' of the other? 
	// If so, then return the 'innermost' one (group over package)
	if (TexturePathA.InStr(TexturePathB) != INDEX_NONE)
	{
		// B is internal of A (or they are identical??)
		return -1;
	}
	if (TexturePathB.InStr(TexturePathA) != INDEX_NONE)
	{
		// A is internal of B (or they are identical??)
		return 1;
	}

	// They are not in the same 'group chain'
	// Pick the 'outermost' one in this case
	// If they are in different packages entirely, pick the B one.
	INT GroupCountA = 0;
	PackageAndGroupNameIdx = 0;
	while (PackageAndGroupNameIdx != INDEX_NONE)
	{
		PackageAndGroupNameIdx = TexturePathA.InStr(TEXT("."), FALSE);
		if (PackageAndGroupNameIdx != INDEX_NONE)
		{
			GroupCountA++;
			TexturePathA = TexturePathA.Right(TexturePathA.Len() - PackageAndGroupNameIdx - 1);
		}
	}
	INT GroupCountB = 0;
	PackageAndGroupNameIdx = 0;
	while (PackageAndGroupNameIdx != INDEX_NONE)
	{
		PackageAndGroupNameIdx = TexturePathB.InStr(TEXT("."), FALSE);
		if (PackageAndGroupNameIdx != INDEX_NONE)
		{
			GroupCountB++;
			TexturePathB = TexturePathB.Right(TexturePathB.Len() - PackageAndGroupNameIdx - 1);
		}
	}

	if ((GroupCountB == 0) && (GroupCountA > 0))
	{
		return 1;
	}
	if ((GroupCountA == 0) && (GroupCountB > 0))
	{
		return -1;
	}
	if (GroupCountA < GroupCountB)
	{
		return 1;
	}
	return -1;
}

IMPLEMENT_COMPARE_CONSTREF(
		FTextureReplacementInfo, 
		UnPackageUtilities,
		{ 
			return PackageUtilities_TextureReplacementSortFunction(A, B);
		}
	);

/**
 *	Fixup the duplicates found...
 */
void UFindDuplicateTexturesCommandlet::FixupDuplicates()
{
	// Only do so if it was requested on the commandline
	if (bFixup == TRUE)
	{
		if (DuplicateTextures.Num() > 0)
		{
#if HAVE_SCC
			// Init source control.
			FSourceControl::Init();
#endif // HAVE_SCC

			FArchive* CSVFile = NULL;
			if (bCheckoutOnly == FALSE)
			{
				// Create CSV folder in case it doesn't exist yet.
				GFileManager->MakeDirectory(*CSVDirectory);
				// CSV: Human-readable spreadsheet format.
				//	This will be the list of replaced textures...
				FString CSVFilename	= FString::Printf(TEXT("%s%s-%s-%i%s%s.csv"), 
					*CSVDirectory, TEXT("ReplacedTextures"), GGameName, GetChangeListNumberForPerfTesting(),
					(FixupFilename.Len() > 0) ? TEXT("-") : TEXT(""),
					(FixupFilename.Len() > 0) ? *FixupFilename : TEXT(""));
				CSVFile	= GFileManager->CreateFileWriter(*CSVFilename);
			}

			// Process all the duplicate textures
			for (TMap<FString,TArray<FString>>::TIterator It(DuplicateTextures); It; ++It)
			{
				TArray<FTextureReplacementInfo> PriorityOrderList;
				FString TextureName = It.Key();
				TArray<FString>& DuplicateList = It.Value();

				FTextureReplacementInfo TempInfo;

				// Load the 'key'...
				if (GetTextureReplacementInfo(TextureName, TempInfo) == TRUE)
				{
					PriorityOrderList.AddItem(TempInfo);
				}
				else
				{
					warnf(NAME_Warning, TEXT("Failed to get texture info for %s"), *TextureName);
					// In this case, the next loadable texture in the list acts as the key!
				}

				for (INT DupIdx = 0; DupIdx < DuplicateList.Num(); DupIdx++)
				{
					// Load each duplicate...
					if (GetTextureReplacementInfo(DuplicateList(DupIdx), TempInfo) == TRUE)
					{
						PriorityOrderList.AddItem(TempInfo);
					}
					else
					{
						warnf(NAME_Warning, TEXT("Failed to get texture info for %s"), *TextureName);
					}
				}

				// Sort them
				Sort<USE_COMPARE_CONSTREF(FTextureReplacementInfo,UnPackageUtilities)>(PriorityOrderList.GetTypedData(),PriorityOrderList.Num());

				debugf(TEXT("Texture (%4.2f) %s"), PriorityOrderList(0).Priority, *(PriorityOrderList(0).TexturePathName));
				for (INT DumpIdx = 1; DumpIdx < PriorityOrderList.Num(); DumpIdx++)
				{
					debugf(TEXT("\treplacing (%4.2f) %s"), PriorityOrderList(DumpIdx).Priority, *(PriorityOrderList(DumpIdx).TexturePathName));
				}

				// Write out a CSV file of replacements.
				FString WriteString;
				if (CSVFile != NULL)
				{
					WriteString = FString::Printf(TEXT("%4.2f,%s"), PriorityOrderList(0).Priority, *(PriorityOrderList(0).TexturePathName));
				}

				// Replace them
				FTextureReplacementInfo& Replacer = PriorityOrderList(0);
				for (INT ReplaceIdx = 1; ReplaceIdx < PriorityOrderList.Num(); ReplaceIdx++)
				{
					FTextureReplacementInfo& Replacee = PriorityOrderList(ReplaceIdx);

					UPackage* ReplaceePackage = UObject::LoadPackage(NULL, *(Replacee.PackageFilename), LOAD_None);
					check(ReplaceePackage == Replacee.Texture->GetOutermost());
#if HAVE_SCC
					if (GFileManager->IsReadOnly(*(Replacee.PackageFilename)) == TRUE)
					{
						FSourceControl::CheckOut(ReplaceePackage);
					}
#endif // HAVE_SCC
					// Verify it is writeable - otherwise we can't save it!
					if (GFileManager->IsReadOnly(*(Replacee.PackageFilename)) == TRUE)
					{
						warnf(NAME_Warning, TEXT("Package is read-only - skipping texture %s"), *(Replacee.TexturePathName));
						continue;
					}

					if (bCheckoutOnly == FALSE)
					{
						// Replace references in the same package.
						TMap<UTexture2D*,UTexture2D*> TextureReplacementMap;
						TextureReplacementMap.Set(Replacee.Texture, Replacer.Texture);
						for (FObjectIterator ObjIt; ObjIt; ++ObjIt)
						{
							if (ObjIt->IsIn(ReplaceePackage))
							{
								UObject* Object = *ObjIt;
								if (Object != Replacee.Texture)
								{
									FArchiveReplaceObjectRef<UTexture2D> ReplaceAr(Object, TextureReplacementMap, FALSE, FALSE, FALSE);
									if (ReplaceAr.GetCount() > 0)
									{
										warnf(TEXT("Replaced %i references of %s in %s"), ReplaceAr.GetCount(), *(Replacee.Texture->GetPathName()), *(Object->GetName()));
										ReplaceePackage->MarkPackageDirty();
									}
								}
							}
						}

						// Create a redirector with the same name as the texture we are 'replacing'
						UObjectRedirector* Redir = (UObjectRedirector*)StaticConstructObject(
							UObjectRedirector::StaticClass(), 
							Replacee.Texture->GetOuter(), 
							Replacee.Texture->GetFName(), RF_Standalone | RF_Public);
						// point the redirector object to this object
						Redir->DestinationObject = Replacer.Texture;

						// Mark it transient to force it to not be saved...
						Replacee.Texture->SetFlags(RF_Transient);
						Replacee.Texture->ClearFlags(RF_Standalone | RF_Public);

						// Update the CSV file string if present
						if (CSVFile != NULL)
						{
							WriteString += TEXT(",");
							WriteString += FString::Printf(TEXT("%4.2f,%s"), PriorityOrderList(ReplaceIdx).Priority, *(PriorityOrderList(ReplaceIdx).TexturePathName));
						}

						// Save the package
						//@todo. This could be optimized to only save the package when we
						// 'switch' to a different source package...
						warnf(TEXT("Saving %s..."), *(ReplaceePackage->GetName()));
						try
						{
							if (SavePackageHelper(ReplaceePackage, Replacee.PackageFilename) == TRUE)
							{
								warnf(NAME_Log, TEXT("\tCorrectly saved:  [%s]."), *(Replacee.PackageFilename));
								TouchedPackageList.AddUniqueItem(ReplaceePackage->GetName());
							}
						}
						catch( ... )
						{
							warnf(NAME_Log, TEXT("\tSave Exception %s"), *(Replacee.PackageFilename));
						}
					}
				}

				// Write out the CSV file line for this replacement
				if (CSVFile != NULL)
				{
					WriteString += TEXT("\n");
					CSVFile->Serialize(TCHAR_TO_ANSI(*WriteString), WriteString.Len());
				}

				UObject::CollectGarbage(RF_Native);
			}
#if HAVE_SCC
			// Init source control.
			FSourceControl::Close();
#endif // HAVE_SCC

			if (CSVFile != NULL)
			{
				// Close and delete archive.
				CSVFile->Close();
				delete CSVFile;
			}
		}
	}
}

/**
 *	Fill in the texture replacement helper structure for the given texture name.
 *
 *	@param	InTextureName		The name of the texture to fill in the info for
 *	@param	OutInfo	[out]		The structure to fill in with the information
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not.
 */
UBOOL UFindDuplicateTexturesCommandlet::GetTextureReplacementInfo(FString& InTextureName, FTextureReplacementInfo& OutInfo)
{
	// Clear the OutInfo just to be safe...
	OutInfo.Clear();

	// Try to load the texture
	UTexture2D* Texture = LoadObject<UTexture2D>(NULL, *InTextureName, NULL, LOAD_None, NULL);	
	if (Texture != NULL)
	{
		OutInfo.Texture = Texture;
		OutInfo.TexturePathName = Texture->GetPathName();
		// Grab the package it is from
		UPackage* Package = Texture->GetOutermost();
		if (Package != NULL)
		{
			// Get the package file path
			FString PackageFilename;
			if (GPackageFileCache->FindPackageFile(*(Package->GetName()), NULL, PackageFilename) == TRUE)
			{
				OutInfo.PackageFilename = PackageFilename;

				FString PackageNameNoExt;
				// Strip off the .upk
				INT ExtensionIdx = PackageFilename.InStr(TEXT(".upk"), FALSE, TRUE);
				if (ExtensionIdx != INDEX_NONE)
				{
					PackageNameNoExt = PackageFilename.Left(ExtensionIdx);
				}
				
				FString PathName;
				INT PathIdx = PackageNameNoExt.InStr(TEXT("\\"), TRUE, TRUE);
				if (PathIdx != INDEX_NONE)
				{
					PathName = PackageNameNoExt.Left(PathIdx);
				}

				// See if it contains any of the folder priority strings...
				for (INT PrioIdx = 0; PrioIdx < FolderPriority.Num(); PrioIdx++)
				{
					if ((PackageFilename == FolderPriority(PrioIdx)) || (PackageNameNoExt == FolderPriority(PrioIdx)))
					{
						// If it is an exact match, then that is the priority
						// If it is an exact match (minus the .upk), then that is the priority
						// This will allow for packages themselves to declare a priority
						// NOTE: It is assumed that packages will come BEFORE the folder they are in.
						OutInfo.Priority = PrioIdx * 1.0f;
						break;
					}
					else
					if (PathName == FolderPriority(PrioIdx))
					{
						// The path name matches exactly (it is in the folder of that priority)
						OutInfo.Priority = PrioIdx * 1.0f;
						break;
					}
					else
					{
						// If it is a sub-folder of the priority, then determine the number of sub-folders
						// and give a priority offset based on that count (up to 9)
						// Count the number of slashes in the name remaining...
						FString ChoppedPathName = PathName;
						UBOOL bFound = FALSE;
						INT SlashCount = 0;
						if (ChoppedPathName.InStr(FolderPriority(PrioIdx), FALSE, TRUE) != INDEX_NONE)
						{
							INT ChopIndex = ChoppedPathName.InStr(TEXT("\\"), TRUE, TRUE);
							while ((bFound == FALSE) && (ChopIndex != INDEX_NONE))
							{
								SlashCount++;
								ChoppedPathName = ChoppedPathName.Left(ChopIndex);
								if (ChoppedPathName == FolderPriority(PrioIdx))
								{
									bFound = TRUE;
								}
								else
								{
									ChopIndex = ChoppedPathName.InStr(TEXT("\\"), TRUE, TRUE);
								}
							}
						}

						if (bFound)
						{
							OutInfo.Priority = PrioIdx * 1.0f + 0.1f;
							break;
						}
					}
				}

				if (OutInfo.Priority == -1.0f)
				{
					OutInfo.Priority = FolderPriority.Num() * 1.0f;
				}

				return TRUE;
			}
		}
	}

	return FALSE;
}

/** 
 *	If present, parse the folder priority list. 
 *
 *	This file can contain folders and/or package names such as:
 *		..\..\Engine\Content\EngineMaterials 
 *			This is EngineMaterials.upk.
 *		..\..\Engine\Content
 *			This is any package found in Engine\Content (or a sub folder)
 *
 *	The first item in the list == priority level 0 (the highest)
 *	Each subsequent line is priority level + 1
 *
 *	Items in subfolders not specifically identified are given a priority of
 *		Folder Priority + (# of subfolders) * 0.1f
 */
void UFindDuplicateTexturesCommandlet::ParseFolderPriorityList()
{
	// Load the priority list, if present
	if (PriorityFilename.Len() > 0)
	{
		const FString PriorityName = CSVDirectory + PriorityFilename;
		debugf(TEXT("Parsing duplicate list from %s"), *PriorityName);
		FString Data;
		if (appLoadFileToString(Data, *PriorityName) == TRUE)
		{
			const TCHAR* Ptr = *Data;
			FString StrLine;

			while (ParseLine(&Ptr, StrLine))
			{
				debugfSlow(TEXT("Read in: %s"), *StrLine);
				FolderPriority.AddUniqueItem(StrLine);
			}
		}
	}
	else
	{
		// By default, Engine Content has priority over all
		FolderPriority.AddUniqueItem(TEXT("..\\..\\Engine\\Content"));
		// Add the game Content folder as well
		FString GameContentFolder = FString::Printf(TEXT("..\\..\\%s\\Content"), GGameName);
		FolderPriority.AddUniqueItem(GameContentFolder);
	}
}

/** 
 *	If present, parse the fixup file. 
 *	Entries found will be inserted into the DuplicateTextures map.
 *	This is assumed to be a csv file that was generated by this commandlet!
 */
void UFindDuplicateTexturesCommandlet::ParseFixupFile()
{
	if ((bFixupOnly == TRUE) && (FixupFilename.Len() > 0))
	{
		// Try to load the given file... and fill in the duplicate texture array
		const FString CSVFilename = CSVDirectory + FixupFilename + TEXT(".csv");

		debugf(TEXT("Parsing duplicate list from %s"), *CSVFilename);
		//Open file
		FString Data;
		if (appLoadFileToString(Data, *CSVFilename) == TRUE)
		{
			const TCHAR* Ptr = *Data;
			const TCHAR* StrPtr;
			FString StrLine;

			while (ParseLine(&Ptr, StrLine))
			{
				StrPtr = *StrLine;

				debugf(TEXT("Read in: %s"), StrPtr);

				// Parse out each texture name
				// The first value is the # of texture duplicates
				FString Parsed;

				INT TextureCount = 0;
				INT ParseIndex = StrLine.InStr(TEXT(","));
				if (ParseIndex != INDEX_NONE)
				{
					Parsed = StrLine.Left(ParseIndex);
					TextureCount = appAtoi(*Parsed) + 1;
				}

				if (TextureCount > 1)
				{
					TArray<FString> TempTextureNames;
					FString ChopString = StrLine;
					for (INT Idx = 0; Idx < TextureCount; Idx++)
					{
						ParseIndex = ChopString.InStr(TEXT(","), TRUE);
						if (ParseIndex != INDEX_NONE)
						{
							Parsed = ChopString.Right(ChopString.Len() - ParseIndex - 1);

							TempTextureNames.AddItem(Parsed);
							ChopString = ChopString.LeftChop(ChopString.Len() - ParseIndex);
						}
					}

					FString& FirstTexture = TempTextureNames(TempTextureNames.Num() - 1);
					TArray<FString>* Textures = DuplicateTextures.Find(FirstTexture);
					if (Textures == NULL)
					{
						// Add it
						TArray<FString> TempArray;
						DuplicateTextures.Set(FirstTexture, TempArray);
						Textures = DuplicateTextures.Find(FirstTexture);						
					}
					check(Textures);

					for (INT AddIdx = 0; AddIdx < TempTextureNames.Num() - 1; AddIdx++)
					{
						Textures->AddUniqueItem(TempTextureNames(AddIdx));
					}
				}
				else
				{
					warnf(NAME_Warning, TEXT("Parse error on line: %s"), *StrLine);
				}
			}
		}
	}
}

/**
 *	Write out the list of packages that were touched...
 */
void UFindDuplicateTexturesCommandlet::WriteTouchedPackageFile()
{
	if (TouchedPackageList.Num() > 0)
	{
		// This resulting file can be passed into the LoadPackages commandlet to
		// ensure there are no problems w/ resaved packages.

		// Open with the fixed up filename, if present
		FString Filename = FixupFilename;
		// Need to make up a file name
		Filename += TEXT("TouchedPackages");
		const FString CSVFilename = CSVDirectory + Filename + TEXT(".csv");

		warnf(TEXT("Writing touched packages to %s"), *CSVFilename);

		FArchive* CSVFile = NULL;
		CSVFile	= GFileManager->CreateFileWriter(*CSVFilename);
		if (CSVFile != NULL)
		{
			FString WriteString;
			for (INT PkgIdx = 0; PkgIdx < TouchedPackageList.Num(); PkgIdx++)
			{
				WriteString = TouchedPackageList(PkgIdx);
				WriteString += TEXT("\n");
				CSVFile->Serialize(TCHAR_TO_ANSI(*WriteString), WriteString.Len());
			}

			// Close and delete archive.
			CSVFile->Close();
			delete CSVFile;
		}
	}
}

INT UFindDuplicateTexturesCommandlet::Main(const FString& Params)
{
	bFixup = FALSE;
	bFixupOnly = FALSE;
	FixupFilename = TEXT("");
	PriorityFilename = TEXT("");

	ParseCommandLine(*Params, Tokens, Switches);

	// Initialize the parameters
	if (Startup() == FALSE)
	{
		return -1;
	}

	// If not fixup only, find all duplicate textures
	if (bFixupOnly == FALSE)
	{
		// Do it...
		DoActionToAllPackages<UTexture2D, FindDuplicateTexturesFunctor>(this, Params);
		FindTrueDuplicates();
	}

	if (bFixup == TRUE)
	{
		ParseFolderPriorityList();
		ParseFixupFile();
		// Fix them up, if desired...
		FixupDuplicates();
		// Write out the touched package list
		WriteTouchedPackageFile();
	}

	// Shut it down!
	Shutdown();

	return 0;
}

IMPLEMENT_CLASS(UFindDuplicateTexturesCommandlet);

/*-----------------------------------------------------------------------------
	FindStaticMeshCanBecomeDynamic commandlet.
-----------------------------------------------------------------------------*/
/**
 *	This will find static meshes that contain empty sections
 */
struct FFindStaticMeshCanBecomeDynamicFunctor
{
	void CleanUpGADTags(){}

	template< typename OBJECTYPE >
	void DoIt(UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches)
	{
		UFindStaticMeshCanBecomeDynamicCommandlet* FindStaticMeshCanBecomeDynamic = Cast<UFindStaticMeshCanBecomeDynamicCommandlet>(Commandlet);
		check(FindStaticMeshCanBecomeDynamic);

		// Do it...
		for (TObjectIterator<OBJECTYPE> It; It; ++It)
		{
			OBJECTYPE* StaticMesh = *It;
			if (StaticMesh->IsIn(Package) == FALSE)
			{
				continue;
			}
			FindStaticMeshCanBecomeDynamic->ProcessStaticMesh(StaticMesh, Package);
		}
	}
};

/**
 *	Find static meshes w/ bCanBecomeDynamic set to true
 */
/**
 *	Startup the commandlet
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UFindStaticMeshCanBecomeDynamicCommandlet::Startup()
{
	StaticMeshesThatCanBecomeDynamic.Empty();

	bSkipCollection = (Switches.FindItemIndex(TEXT("SKIPCOLLECTION")) != INDEX_NONE);
	if (bSkipCollection == TRUE)
	{
		warnf(NAME_Log, TEXT("Skipping collection update..."));
	}
	else
	{
		// Setup our GAD collections
		CollectionName = TEXT("SMDynamics");
		GADHelper = new FGADHelper();
		check(GADHelper);
		GADHelper->Initialize();
	}

	return TRUE;
}

/**
 *	Shutdown the commandlet
 */
void UFindStaticMeshCanBecomeDynamicCommandlet::Shutdown()
{
	if (bSkipCollection == FALSE)
	{
		delete GADHelper;
		GADHelper = NULL;
	}
}

/**
 *	Update the collection with the current lists
 *
 *	@return	UBOOL		TRUE if successful, FALSE if not.
 */
UBOOL UFindStaticMeshCanBecomeDynamicCommandlet::UpdateCollection()
{
	if (StaticMeshesThatCanBecomeDynamic.Num() != 0)
	{
		TArray<FString> AssetList;
		for (TMap<FString,UBOOL>::TIterator It(StaticMeshesThatCanBecomeDynamic); It; ++It)
		{
			AssetList.AddItem(It.Key());
		}

		if (bSkipCollection == FALSE)
		{
			check(GADHelper);
			if (GADHelper->SetCollection(CollectionName, EGADCollection::Shared, AssetList) == TRUE)
			{
				// Clear the current lists...
				StaticMeshesThatCanBecomeDynamic.Empty();
				return TRUE;
			}
		}
		else
		{
			// Simply log them out...
			for (INT DumpIdx = 0; DumpIdx < AssetList.Num(); DumpIdx++)
			{
				warnf(NAME_Log, TEXT("%s"), *(AssetList(DumpIdx)));
			}
		}
	}
	else
	{
		warnf(NAME_Log, TEXT("No static meshes found!"));
	}
	return TRUE;
}

/**
 *	Process the given static mesh
 *
 *	@param	InStaticMesh		The Static mesh to process
 *	@param	InPackage		The package being processed
 */
void UFindStaticMeshCanBecomeDynamicCommandlet::ProcessStaticMesh(UStaticMesh* InStaticMesh, UPackage* InPackage)
{
	if (InStaticMesh != NULL)
	{
		if (InStaticMesh->bCanBecomeDynamic == TRUE)
		{
			StaticMeshesThatCanBecomeDynamic.Set(InStaticMesh->GetFullName(), TRUE);
		}
	}
}

INT UFindStaticMeshCanBecomeDynamicCommandlet::Main(const FString& Params)
{
	ParseCommandLine(*Params, Tokens, Switches);

	// Initialize the parameters
	if (Startup() == FALSE)
	{
		return -1;
	}

	// Do it...
	DoActionToAllPackages<UStaticMesh, FFindStaticMeshCanBecomeDynamicFunctor>(this, Params);

	// Update the collection
	UpdateCollection();

	// Shut it down!
	Shutdown();

	return 0;
}

IMPLEMENT_CLASS(UFindStaticMeshCanBecomeDynamicCommandlet);

/*-----------------------------------------------------------------------------
	FindStaticMeshEmptySections commandlet.
-----------------------------------------------------------------------------*/
/**
 *	This will find static meshes that contain empty sections
 */
struct FFindStaticMeshEmptySectionsFunctor
{
	void CleanUpGADTags(){}

	template< typename OBJECTYPE >
	void DoIt(UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches)
	{
		UFindStaticMeshEmptySectionsCommandlet* FindStaticMeshEmptySections = Cast<UFindStaticMeshEmptySectionsCommandlet>(Commandlet);
		check(FindStaticMeshEmptySections);

		// Do it...
		for (TObjectIterator<OBJECTYPE> It; It; ++It)
		{
			OBJECTYPE* StaticMesh = *It;
			if (FindStaticMeshEmptySections->bOnlyLoadMaps == FALSE)
			{
				if (StaticMesh->IsIn(Package) == FALSE)
				{
					continue;
				}
			}
			FindStaticMeshEmptySections->ProcessStaticMesh(StaticMesh, Package);
		}
	}
};

/**
 *	Startup the commandlet
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UFindStaticMeshEmptySectionsCommandlet::Startup()
{
	// Available options:
	// SWITCHES:
	//	REFDONLY    - if specified, only map packages will be processed.
	//	SAVESHADERS - if specified, shader caches will be saved after each package
	//		is processed. (To save waiting for shader recompilations in the event
	//		of a crash such as out-of-memory.)
	// TOKENS:
	//

	bOnlyLoadMaps = (Switches.FindItemIndex(TEXT("ONLYLOADMAPS")) != INDEX_NONE);
	StaticMeshEmptySectionList.Empty();
	StaticMeshToPackageUsedInMap.Empty();

	// Setup our GAD collections
	CollectionName = TEXT("StaticMesh Bad Sections");
	if (bOnlyLoadMaps == TRUE)
	{
		CollectionName += TEXT(" (Ref'd)");
	}
	GADHelper = new FGADHelper();
	check(GADHelper);
	GADHelper->Initialize();
	if (Switches.ContainsItem(TEXT("CLEARCOLLECTION")))
	{
		GADHelper->ClearCollection(CollectionName, EGADCollection::Shared);
	}

	return TRUE;
}

/**
 *	Shutdown the commandlet
 */
void UFindStaticMeshEmptySectionsCommandlet::Shutdown()
{
	delete GADHelper;
	GADHelper = NULL;
}

/**
 *	Update the collection with the current lists
 *
 *	@return	UBOOL		TRUE if successful, FALSE if not.
 */
UBOOL UFindStaticMeshEmptySectionsCommandlet::UpdateCollection()
{
	if (GADHelper->UpdateCollection(CollectionName, EGADCollection::Shared, StaticMeshEmptySectionList, StaticMeshPassedList) == TRUE)
	{
		// Clear the current lists...
		StaticMeshEmptySectionList.Empty();
		StaticMeshPassedList.Empty();
		return TRUE;
	}
	return FALSE;
}

/**
 *	Process the given StaticMesh
 *
 *	@param	InStaticMesh	The static mesh to process
 *	@param	InPackage		The package being processed
 */
void UFindStaticMeshEmptySectionsCommandlet::ProcessStaticMesh(UStaticMesh* InStaticMesh, UPackage* InPackage)
{
	if (InStaticMesh != NULL)
	{
		FString MeshName = InStaticMesh->GetPathName();
		UBOOL bAdded = FALSE;
		for (INT LODIdx = 0; LODIdx < InStaticMesh->LODModels.Num() && !bAdded; LODIdx++)
		{
			FStaticMeshRenderData& LODModel = InStaticMesh->LODModels(LODIdx);
			for (INT ElementIdx = 0; ElementIdx < LODModel.Elements.Num() && !bAdded; ElementIdx++)
			{
				FStaticMeshElement& Element = LODModel.Elements(ElementIdx);
				if ((Element.NumTriangles == 0) || (Element.Material == NULL))
				{
					StaticMeshEmptySectionList.AddUniqueItem(InStaticMesh->GetFullName());

					TArray<FString>* MapList = StaticMeshToPackageUsedInMap.Find(MeshName);
					if (MapList == NULL)
					{
						TArray<FString> Temp;
						StaticMeshToPackageUsedInMap.Set(MeshName, Temp);
						MapList = StaticMeshToPackageUsedInMap.Find(MeshName);
					}
					check(MapList);
					MapList->AddUniqueItem(InPackage->GetName());
					bAdded = TRUE;
				}
			}
		}

		if (bAdded == FALSE)
		{
			StaticMeshPassedList.AddUniqueItem(InStaticMesh->GetFullName());
		}
	}
}

INT UFindStaticMeshEmptySectionsCommandlet::Main(const FString& Params)
{
	ParseCommandLine(*Params, Tokens, Switches);

	// Initialize the parameters
	if (Startup() == FALSE)
	{
		return -1;
	}

	// Do it...
	DoActionToAllPackages<UStaticMesh, FFindStaticMeshEmptySectionsFunctor>(this, Params);

	// Update the collection
	UpdateCollection();

	// Dump out the mesh --> map/package list
	debugf(TEXT("StaticMesh to Package list:"));
	for (TMap<FString,TArray<FString>>::TIterator DumpIt(StaticMeshToPackageUsedInMap); DumpIt; ++DumpIt)
	{
		FString StaticMeshName = DumpIt.Key();
		TArray<FString>& PackageList = DumpIt.Value();

		debugf(TEXT("\t%s"), *StaticMeshName);
		for (INT PkgIdx = 0; PkgIdx < PackageList.Num(); PkgIdx++)
		{
			debugf(TEXT("\t\t%s"), *(PackageList(PkgIdx)));
		}
	}

	// Shut it down!
	Shutdown();

	return 0;
}

IMPLEMENT_CLASS(UFindStaticMeshEmptySectionsCommandlet);

/*-----------------------------------------------------------------------------
	FindAssetReferencers commandlet.
-----------------------------------------------------------------------------*/
/**
 *	Startup the commandlet
 *
 *	@param	Params	The command-line parameters
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UFindAssetReferencersCommandlet::Startup(const FString& Params)
{
	ParseCommandLine(*Params, Tokens, Switches);

	bVerbose = Switches.FindItemIndex(TEXT("VERBOSE")) != INDEX_NONE;
	bExactMatch = Switches.FindItemIndex(TEXT("EXACT")) != INDEX_NONE;

	// the first token must be the name of the asset (or package) of interest
	if (Tokens.Num() == 0)
	{
		warnf(NAME_Warning, TEXT("No asset specified!"));
		ShowUsage();
		return FALSE;
	}
	AssetName = Tokens(0);
	warnf(NAME_Log, TEXT("Checking for asset %s"), *AssetName);

	// Check for SCRIPT inclusion
	bNonNativeScript = Switches.FindItemIndex(TEXT("SCRIPT")) != INDEX_NONE;
	if (bVerbose == TRUE)
	{
		warnf(NAME_Log, TEXT("Non-native game script packages WILL%s be checked!"), bNonNativeScript ? TEXT(""): TEXT(" NOT"));
	}

	// Parse the map prefixes
	MapPrefixes.Empty();
	FString MapPrefixCmdLine;
	const TCHAR* CmdParams = *Params;
	if (Parse(CmdParams, TEXT("MAPPREFIXES="), MapPrefixCmdLine, FALSE))
	{
		INT CommaIdx = MapPrefixCmdLine.InStr(TEXT(","));
		while (CommaIdx != INDEX_NONE)
		{
			FString Temp = MapPrefixCmdLine.Left(CommaIdx);
			MapPrefixes.AddItem(Temp.ToUpper());
			MapPrefixCmdLine = MapPrefixCmdLine.Right(MapPrefixCmdLine.Len() - CommaIdx - 1);
			CommaIdx = MapPrefixCmdLine.InStr(TEXT(","));
		}
		MapPrefixes.AddItem(MapPrefixCmdLine.ToUpper());
	}

	warnf(NAME_Log, TEXT("Using %d map prefixes:"), MapPrefixes.Num());
	if (bVerbose == TRUE)
	{
		for (INT DumpIdx = 0; DumpIdx < MapPrefixes.Num(); DumpIdx++)
		{
			warnf(NAME_Log, TEXT("\t%s"), *(MapPrefixes(DumpIdx)));
		}
	}
	
	return TRUE;
}

/**
 *	Shutdown the commandlet
 */
void UFindAssetReferencersCommandlet::Shutdown()
{
}

/** Show the usage string */
void UFindAssetReferencersCommandlet::ShowUsage()
{
	warnf(NAME_Log, TEXT("Usage: FindAssetReferencers <ASSET OR PACKAGE NAME> [-SCRIPT] [-MAPPREFIXES=<CSV list of map prefixes to look in>] [-VERBOSE]"));
}

/** 
 *	Generate the list of packages to process 
 *	
 *	@return UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UFindAssetReferencersCommandlet::GeneratePackageList()
{
	PackageList.Empty();
	// Script packages
	if (bNonNativeScript == TRUE)
	{
		if (bVerbose == TRUE)
		{
			warnf(NAME_Log, TEXT("Gathering non-native script packages..."));
		}
		// Add the non-native script packages to the list
		TArray<FString> NonNativeScriptPackages;
		appGetScriptPackageNames(NonNativeScriptPackages, SPT_NonNative);

		for (INT AddIdx = 0; AddIdx < NonNativeScriptPackages.Num(); AddIdx++)
		{
			PackageList.AddItem(NonNativeScriptPackages(AddIdx));
			if (bVerbose == TRUE)
			{
				warnf(NAME_Log, TEXT("\t%s"), *(NonNativeScriptPackages(AddIdx)));
			}
		}
	}

	// Map prefixes...
	if (MapPrefixes.Num() > 0)
	{
		TArray<FString> FilesInPath = GPackageFileCache->GetPackageFileList();
		INT GCIndex = 0;
		for (INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++)
		{
			const FFilename& Filename = FilesInPath(FileIndex);
			// Skip non-maps
			if (Filename.GetExtension() != FURL::DefaultMapExt)
			{
				continue;
			}
			// Skip auto-save maps
			if (FString(*Filename).ToUpper().InStr(TEXT("AUTOSAVES")) != INDEX_NONE)
			{
				continue;
			}
// 			// Skip on-hold maps
// 			if (FString(*Filename).ToUpper().InStr(TEXT("ONHOLD")) != INDEX_NONE)
// 			{
// 				warnf(NAME_Log, TEXT("Skipping on-hold map %s"), *Filename);
// 				continue;
// 			}
// 			// Skip cinematics maps
// 			if (FString(*Filename).ToUpper().InStr(TEXT("CINEMATICS")) != INDEX_NONE)
// 			{
// 				warnf(NAME_Log, TEXT("Skipping cinematic map %s"), *Filename);
// 				continue;
// 			}
// 			// Skip DLC maps
// 			if (FString(*Filename).ToUpper().InStr(TEXT("DLC")) != INDEX_NONE)
// 			{
// 				warnf(NAME_Log, TEXT("Skipping cinematic map %s"), *Filename);
// 				continue;
// 			}

			// Verify it contains one of the requested prefixes
			FString BaseName = Filename.GetBaseFilename().ToUpper();
			for (INT PrefixIdx = 0; PrefixIdx < MapPrefixes.Num(); PrefixIdx++)
			{
				if (BaseName.StartsWith(MapPrefixes(PrefixIdx)) == TRUE)
				{
					PackageList.AddUniqueItem(Filename);
					if (bVerbose == TRUE)
					{
						warnf(NAME_Log, TEXT("Adding map package to list: %s"), *BaseName);
					}
				}
			}
		}
	}
	
	return TRUE;
}

/** Process the packages
 *	
 *	@return UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UFindAssetReferencersCommandlet::ProcessPackageList()
{
	for (INT PackageIdx = 0; PackageIdx < PackageList.Num(); PackageIdx++)
	{
		const FString& PackageName = PackageList(PackageIdx);
		warnf(NAME_Log, TEXT("Loading package %4d of %4d: %s"), (PackageIdx + 1), PackageList.Num(), *PackageName);
		UPackage* Package = UObject::LoadPackage(NULL, *PackageName, LOAD_NoWarn);
		if (Package == NULL)
		{
			warnf(NAME_Error, TEXT("Error loading %s!"), *PackageName);
			continue;
		}

		// Iterate over all objects looking for the asset
		for (TObjectIterator<UObject> It; It; ++It)
		{
			UObject* Object = *It;

			if (Object->IsA(UPackage::StaticClass()) == FALSE)
			{
				FString ObjPathName = Object->GetPathName();
				if (ObjPathName.InStr(TEXT(":")) == INDEX_NONE)
				{
					// this will find substrings of the asset name
					// so if you are just looking for GameWeap_Rifle  you will get GameWeap_RifleAwesome
					//if (ObjPathName.InStr(AssetName, FALSE, TRUE) != INDEX_NONE)
					if (ObjPathName.Right(AssetName.Len()).InStr(AssetName, bExactMatch, TRUE) != INDEX_NONE)
					{
						UPackage* ObjPackage = Object->GetOutermost();
						// Found as object of interest...
						TMap<FString,UBOOL>* ReferencerList = AssetsToReferencersMap.Find(Object->GetPathName());
						if (ReferencerList == NULL)
						{
							TMap<FString,UBOOL> Temp;
							AssetsToReferencersMap.Set(Object->GetPathName(), Temp);
							ReferencerList = AssetsToReferencersMap.Find(Object->GetPathName());
						}
						check(ReferencerList);

						TArray<FReferencerInformation> OutInternalReferencers;
						TArray<FReferencerInformation> OutExternalReferencers;
						Object->RetrieveReferencers(&OutInternalReferencers, &OutExternalReferencers, FALSE);
						for (INT OIRefIdx = 0; OIRefIdx < OutInternalReferencers.Num(); OIRefIdx++)
						{
							FReferencerInformation& InternalReferencer = OutInternalReferencers(OIRefIdx);
							if (InternalReferencer.Referencer != NULL)
							{
								ProcessReferencer(InternalReferencer.Referencer, ObjPackage, ReferencerList);
							}
						}

						for (INT OERefIdx = 0; OERefIdx < OutExternalReferencers.Num(); OERefIdx++)
						{
							FReferencerInformation& ExternalReferencer = OutExternalReferencers(OERefIdx);
							if (ExternalReferencer.Referencer != NULL)
							{
								ProcessReferencer(ExternalReferencer.Referencer, ObjPackage, ReferencerList);
							}
						}
					}
				}
			}
		}

		// Have to garbage collect each run...
		UObject::CollectGarbage(RF_Native);
	}
	return TRUE;
}

/**
 *	Process the given object referencer
 *
 *	@param	InReferencer	The object referencers to process
 *	@param	InPackage		The package being processed
 *	@param	ReferencerList	The list to add the ref info to
 */
void UFindAssetReferencersCommandlet::ProcessReferencer(UObject* InReferencer, UPackage* InPackage, TMap<FString,UBOOL>* ReferencerList)
{
	// Don't add referencers that are in the same package?
	if (InReferencer->GetOutermost() != InPackage)
	{
		FString RefFullName = InReferencer->GetFullName();
		// If the referencer has a ':' in the name, it is a 'sub-object' of an object.
		// For example, a texture would be referenced by a material expression texture sample.
		// We only care about the material.
		INT ColonIdx = RefFullName.InStr(TEXT(":"));
		if (ColonIdx != INDEX_NONE)
		{
			FString ParsedName = RefFullName.Right(RefFullName.Len() - ColonIdx);
			// We know that we are interested in the outer at the very least
			INT OuterCount = 0;
			// Count the number of '.' in the parsed name to determine how much further 
			// up the outer chain we should go
			INT PeriodIdx = ParsedName.InStr(TEXT("."));
			while (PeriodIdx != INDEX_NONE)
			{
				OuterCount++;
				ParsedName = ParsedName.Right(ParsedName.Len() - PeriodIdx - 1);
				PeriodIdx = ParsedName.InStr(TEXT("."));
			}

			// Find the outer of interest
			UObject* RefOuter = InReferencer->GetOuter();
			while (OuterCount > 0)
			{
				RefOuter = RefOuter->GetOuter();
				OuterCount--;
				if (RefOuter == NULL)
				{
					checkf(0, TEXT("FOUND NULL OUTER???"));
					OuterCount = 0;
				}
			}
			RefFullName = RefOuter->GetFullName();
		}
		ReferencerList->Set(RefFullName, TRUE);
	}
}

/** Report the results of the commandlet */
void UFindAssetReferencersCommandlet::ReportResults()
{
	FArchive* CSVFile = NULL;
	// Create CSV folder in case it doesn't exist yet.
	FString CSVDirectory = appGameLogDir() + TEXT("AssetRefs") + PATH_SEPARATOR;
	GFileManager->MakeDirectory(*CSVDirectory);
	// CSV: Human-readable spreadsheet format.
	//	This will be the list of replaced textures...
	FString CSVFilename	= FString::Printf(TEXT("%s%s-%s.csv"), *CSVDirectory, GGameName, *AssetName);
	CSVFile	= GFileManager->CreateFileWriter(*CSVFilename);

	FString OutputString;

	for (TMap<FString,TMap<FString,UBOOL>>::TIterator It(AssetsToReferencersMap); It; ++It)
	{
		FString ObjectName = It.Key();
		TMap<FString,UBOOL>& ReferencerMap = It.Value();

		if (ReferencerMap.Num() > 0)
		{
			OutputString = FString::Printf(TEXT("%s"), *ObjectName);
			for (TMap<FString,UBOOL>::TIterator InnerIt(ReferencerMap); InnerIt; ++InnerIt)
			{
				FString RefName = InnerIt.Key();
				OutputString += FString::Printf(TEXT(",%s"), *RefName);
			}
			OutputString += LINE_TERMINATOR;

			if (CSVFile)
			{
				CSVFile->Serialize(TCHAR_TO_ANSI(*OutputString), OutputString.Len());
			}
			if (bVerbose == TRUE)
			{
				warnf(NAME_Log, *OutputString);
			}
		}
	}

	if (CSVFile)
	{
		CSVFile->Close();
		delete CSVFile;
	}
}

INT UFindAssetReferencersCommandlet::Main(const FString& Params)
{
	// Initialize the parameters
	if (Startup(Params) == FALSE)
	{
		warnf(NAME_Warning, TEXT("Failed to startup commandlet!"));
		return -1;
	}

	GeneratePackageList();
	ProcessPackageList();
	ReportResults();
	// Shut it down!
	Shutdown();

	return 0;
}

IMPLEMENT_CLASS(UFindAssetReferencersCommandlet);

///////////////////////////////////
/*-----------------------------------------------------------------------------
	AnalyzeParticleSystems commandlet.
-----------------------------------------------------------------------------*/
/**
 *	
 */
struct FAnalyzeParticleSystemsFunctor
{
	void CleanUpGADTags(){}

	template< typename OBJECTYPE >
	void DoIt(UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches)
	{
		UAnalyzeParticleSystemsCommandlet* AnalyzeParticleSystems = Cast<UAnalyzeParticleSystemsCommandlet>(Commandlet);
		check(AnalyzeParticleSystems);

		// Do it...
		for (TObjectIterator<OBJECTYPE> It; It; ++It)
		{
			OBJECTYPE* PSys = *It;
			if (PSys->IsIn(Package) == FALSE)
			{
				continue;
			}

			AnalyzeParticleSystems->ProcessParticleSystem(PSys);
		}
	}
};

/**
 *	Startup the commandlet
 */
void UAnalyzeParticleSystemsCommandlet::Startup()
{
	ParticleSystemCount = 0;
	EmitterCount = 0;
	DisabledEmitterCount = 0;
	LODLevelCount = 0;
	DisabledLODLevelCount = 0;
	ModuleCount = 0;
	DisabledModuleCount = 0;
}

/**
 *	Shutdown the commandlet
 */
void UAnalyzeParticleSystemsCommandlet::Shutdown()
{
}

/**
 *	Process the given particle system
 *
 *	@param	InParticleSystem	The particle system to analyze
 *
 *	@return	UBOOL				Ignored
 */
UBOOL UAnalyzeParticleSystemsCommandlet::ProcessParticleSystem(UParticleSystem* InParticleSystem)
{
	if (InParticleSystem == NULL)
	{
		return FALSE;
	}

	warnf(TEXT("\tAnalyzing %s"), *(InParticleSystem->GetPathName()));

	ParticleSystemCount++;
	// Check the particle system for
	//	- Completely disabled emitters (all LODLevels are set to bEnabled==FALSE
	//	- Completely disabled modules
	//	- Modules disabled in one LOD and enabled in the others
	for (INT EmitterIdx = 0; EmitterIdx < InParticleSystem->Emitters.Num(); EmitterIdx++)
	{
		UBOOL bEmitterCompletelyDisabled = TRUE;
		UParticleEmitter* Emitter = InParticleSystem->Emitters(EmitterIdx);
		if (Emitter != NULL)
		{
			EmitterCount++;

			warnf(TEXT("\t\tEmitter %d"), EmitterIdx);
			warnf(TEXT("\t\t\tLODLevels = %d"), Emitter->LODLevels.Num());

			for (INT LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
			{
				FString DebugDump;
				UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIdx);
				if (LODLevel != NULL)
				{
					LODLevelCount++;
					if (LODLevel->bEnabled == TRUE)
					{
						bEmitterCompletelyDisabled = FALSE;
					}
					else
					{
						DisabledLODLevelCount++;
					}

					DebugDump = FString::Printf(TEXT("\t\t\t\t%2d: "), LODIdx);
					for (INT ModuleIdx = -3; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
					{
						UParticleModule* Module = NULL;
						switch (ModuleIdx)
						{
						case -3:	Module = LODLevel->RequiredModule;			break;
						case -2:	Module = LODLevel->SpawnModule;				break;
						case -1:	Module = LODLevel->TypeDataModule;			break;
						default:	Module = LODLevel->Modules(ModuleIdx);		break;
						}

						if (Module != NULL)
						{
							ModuleCount++;
							DebugDump += (Module->bEnabled == TRUE) ? TEXT("1 ") : TEXT("0 ");
							if (Module->bEnabled == FALSE)
							{
								DisabledModuleCount++;
							}
						}
						else
						{
							DebugDump += TEXT("- ");
						}
					}
				}
				else
				{
					DebugDump = FString::Printf(TEXT("\t\t\t\t%2d: NULL"), LODIdx);
				}

				warnf(*DebugDump);
			}

			if (bEmitterCompletelyDisabled == TRUE)
			{
				DisabledEmitterCount++;
			}
		}
		else
		{
			warnf(TEXT("\t\tEmitter %d == NULL"), EmitterIdx);
		}
	}

	return TRUE;
}

INT UAnalyzeParticleSystemsCommandlet::Main( const FString& Params )
{
	ParseCommandLine(*Params, Tokens, Switches);

	// Initialize the parameters
	Startup();

	debugf(TEXT("Analyzing particle systems..."));
	debugf(TEXT("----------------------------------------------------"));

	// Do it...
	DoActionToAllPackages<UParticleSystem, FAnalyzeParticleSystemsFunctor>(this, Params);

	debugf(TEXT("----------------------------------------------------"));
	debugf(TEXT("Found:"));
	debugf(TEXT("\t%5d particle systems."), ParticleSystemCount);
	debugf(TEXT("\t\t%5d emitters  - %5d disabled (%5.3f%%)"), EmitterCount, DisabledEmitterCount, (FLOAT)DisabledEmitterCount / (FLOAT)EmitterCount);
	debugf(TEXT("\t\t%5d LODLevels - %5d disabled (%5.3f%%)"), LODLevelCount, DisabledLODLevelCount, (FLOAT)DisabledLODLevelCount / (FLOAT)LODLevelCount);
	debugf(TEXT("\t\t%5d Modules   - %5d disabled (%5.3f%%)"), ModuleCount, DisabledModuleCount, (FLOAT)DisabledModuleCount / (FLOAT)ModuleCount);
	debugf(TEXT("----------------------------------------------------"));

	// Shut it down!
	Shutdown();

	return 0;
}
IMPLEMENT_CLASS(UAnalyzeParticleSystemsCommandlet);

/** This will clear the passed in tag from all assets **/
void UpdateGADClearTags( const FString& TagName )
{
#if WITH_MANAGED_CODE
	CommandletGADHelper.ClearTagFromAssets( TagName );
#endif // WITH_MANAGED_CODE
}

/** This will set the passed in tag name on the object if it is not in the whitelist **/
void UpdateGADSetTags( const FString& ObjectName, const FString& TagName, const FString& TagNameWhitelist )
{
#if WITH_MANAGED_CODE
	// check to see that the whitelist tag exists.  If it does not then make it

	CommandletGADHelper.CreateTag( TagNameWhitelist );

	// find the objects in the whitelist
	TArray<FString> Whitelist;
	CommandletGADHelper.QueryTaggedAssets( TagNameWhitelist, Whitelist );

	// if we are not in the white list
	if( Whitelist.FindItemIndex( ObjectName ) == INDEX_NONE )
	{
		TArray<FString> ListToAdd;
		TArray<FString> ListToRemove;
		ListToAdd.AddItem( ObjectName );

		// update our tag
		//GADHelper.SetTaggedAssets( TagName, EGADCollection::Shared, ListToAdd );
		CommandletGADHelper.UpdateTaggedAssets( TagName, ListToAdd, ListToRemove );
	}
#endif // WITH_MANAGED_CODE
}

/** 
 *	This will set the passed in tag name on the objects if they are not in the whitelist
 *
 *	@param	ObjectNames			The list of object names to tag
 *	@param	TagName				The tag to apply to the objects
 *	@param	TagNameWhitelist	The tag of objects that should *not* be tagged (whitelist)
 */
void UpdateGADSetTagsForObjects(TMap<FString,UBOOL>& ObjectNames, const FString& TagName, const FString& TagNameWhitelist)
{
#if WITH_MANAGED_CODE
	TArray<FString> AssetsToTag;

	// check to see that the whitelist tag exists.  If it does not then make it
	CommandletGADHelper.CreateTag(TagNameWhitelist);

	// find the objects in the whitelist
	TArray<FString> Whitelist;
	CommandletGADHelper.QueryTaggedAssets(TagNameWhitelist, Whitelist);

	for (TMap<FString,UBOOL>::TIterator ObjIt(ObjectNames); ObjIt; ++ObjIt)
	{
		FString ObjectName = ObjIt.Key();
		// if we are not in the white list
		if (Whitelist.FindItemIndex(ObjectName) == INDEX_NONE)
		{
			AssetsToTag.AddItem(ObjectName);
		}
	}

	TArray<FString> ListToRemove;
	// update our tag
	CommandletGADHelper.UpdateTaggedAssets(TagName, AssetsToTag, ListToRemove);
#endif // WITH_MANAGED_CODE
}

/*-----------------------------------------------------------------------------
	BaseObjectCollectionGenerator commandlet.
-----------------------------------------------------------------------------*/
/**
 *	Base commandlet for generation collections of objects that meet some criteria
 */
/**
 *	Startup the commandlet.
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UBaseObjectTagGeneratorCommandlet::Startup()
{
	bSkipScriptPackages = (Switches.FindItemIndex(TEXT("SKIPSCRIPT")) != INDEX_NONE);
	if (bSkipScriptPackages)
	{
		warnf(NAME_Log, TEXT("Skipping script packages..."));
	}
	else
	{
		warnf(NAME_Log, TEXT("Processing script packages..."));
	}
	bSkipMapPackages = (Switches.FindItemIndex(TEXT("SKIPMAPS")) != INDEX_NONE);
	if (bSkipMapPackages)
	{
		warnf(NAME_Log, TEXT("Skipping map packages..."));
	}
	else
	{
		warnf(NAME_Log, TEXT("Processing map packages..."));
	}
	bMapsOnly = !bSkipMapPackages && (Switches.FindItemIndex(TEXT("MAPSONLY")) != INDEX_NONE);
	if (bMapsOnly)
	{
		warnf(NAME_Log, TEXT("Skipping non-map packages..."));
	}
	else
	{
		warnf(NAME_Log, TEXT("Processing non-map packages..."));
	}
	bSaveShaderCaches = (Switches.FindItemIndex(TEXT("SAVESHADERCACHES")) != INDEX_NONE);

	GCRate = 10;
	FString UpdateRate(TEXT(""));
	for (INT TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++)
	{
		FString& Token = Tokens(TokenIndex);
		if (Parse(*Token, TEXT("GCRate="), UpdateRate))
		{
			break;
		}
	}
	if (UpdateRate.Len() > 0)
	{
		INT CheckInt = appAtoi(*UpdateRate);
		if (CheckInt > 0)
		{
			GCRate = CheckInt;
		}
	}
	warnf(NAME_Log, TEXT("Garbage collection rate set to %3d packages..."), GCRate);

	GADHelper = NULL;
	bUpdateTagsOnly = (Switches.FindItemIndex(TEXT("UPDATETAGS")) != INDEX_NONE);
	if (bUpdateTagsOnly)
	{
		warnf(NAME_Log, TEXT("Updating tags only..."));
	}
	bSkipTags = (Switches.FindItemIndex(TEXT("SKIPTAGS")) != INDEX_NONE);
	if (bSkipTags == TRUE)
	{
		warnf(NAME_Log, TEXT("Skipping tags update..."));
	}
	bClearTags = (Switches.FindItemIndex(TEXT("CLEARTAGS")) != INDEX_NONE);
	if (bClearTags == TRUE)
	{
		warnf(NAME_Log, TEXT("Clearing tags..."));
	}

	/** The text file that contains map lists */
	FString MapFileToken(TEXT(""));
	for (INT TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++)
	{
		FString& Token = Tokens(TokenIndex);
		if (Parse(*Token, TEXT("MAPLIST="), MapFileToken))
		{
			break;
		}
	}
	if (MapFileToken.Len() > 0)
	{
		MapFileName = MapFileToken;
		warnf(NAME_Log, TEXT("Using map list %s"), *MapFileName);
	}

	/** Whether to load all sub-levels of maps as well */
	bLoadSubLevels = (Switches.FindItemIndex(TEXT("LOADSUBLEVELS")) != INDEX_NONE);
	if (bLoadSubLevels == TRUE)
	{
		warnf(NAME_Log, TEXT("Loading sub-levels..."));
	}

	return TRUE;
}

/**
 *	Shutdown the commandlet
 */
void UBaseObjectTagGeneratorCommandlet::Shutdown()
{
	delete GADHelper;
	GADHelper = NULL;
}

/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UBaseObjectTagGeneratorCommandlet::Initialize()
{
	// YOUR COMMANDLET MUST OVERRIDE THIS FUNCTION!
	return FALSE;
}

/**
 *	Generate the list of packages to process
 */
void UBaseObjectTagGeneratorCommandlet::GeneratePackageList()
{
	PackageList.Empty();

	// Form the package list
	if (MapFileName.Len() > 0)
	{
		// 
		FString Text;
		if (appLoadFileToString(Text, *MapFileName))
		{
			const TCHAR* Data = *Text;
			FString Line;
			while (ParseLine(&Data, Line))
			{
				PackageList.AddUniqueItem(Line);
				if (bLoadSubLevels == TRUE)
				{
					FString FullMapName;
					if (GPackageFileCache->FindPackageFile(*Line, NULL, FullMapName))
					{
						FArchive* CheckMapPackageFile = GFileManager->CreateFileReader(*FullMapName);
						if (CheckMapPackageFile)
						{
							// read the package summary, which has list of sub levels
							FPackageFileSummary Summary;
							(*CheckMapPackageFile) << Summary;
							// close the map
							delete CheckMapPackageFile;
							// Make sure that it is a map!
							if ((Summary.PackageFlags & PKG_ContainsMap) != 0)
							{
								// if it's an old map, then we have to load it to get the list of sublevels
								if (Summary.GetFileVersion() < VER_ADDITIONAL_COOK_PACKAGE_SUMMARY)
								{
									warnf(NAME_Log, TEXT("  Old map, SKIPPING %s"), *Line);
								}
								else
								{
									// Streaming levels present?
									for (INT AdditionalPackageIndex = 0; AdditionalPackageIndex < Summary.AdditionalPackagesToCook.Num(); AdditionalPackageIndex++)
									{
										FString ContainedLevelName;
										if (GPackageFileCache->FindPackageFile(*Summary.AdditionalPackagesToCook(AdditionalPackageIndex), NULL, ContainedLevelName))
										{
											FArchive* AdditionalPackageFile = GFileManager->CreateFileReader(*ContainedLevelName);
											if (AdditionalPackageFile)
											{
												// read the package summary, which has list of sub levels
												FPackageFileSummary AdditionalSummary;
												(*AdditionalPackageFile) << AdditionalSummary;
												// close the map
												delete AdditionalPackageFile;

												if ((AdditionalSummary.PackageFlags & PKG_ContainsMap) != 0)
												{
													PackageList.AddUniqueItem(ContainedLevelName);
												}
											}
										}
									}
								}
							}
						}
					}
					else
					{
						warnf(NAME_Log, TEXT("Failed to find package %s"), *Line);
					}
				}
			}
		}
		else
		{
			warnf(NAME_Warning, TEXT("Failed to load file %s"), *MapFileName);
		}
	}

	if (PackageList.Num() == 0)
	{
		TArray<FString> FilesInPath = GPackageFileCache->GetPackageFileList();
		INT GCIndex = 0;
		for (INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++)
		{
			const FFilename& Filename = FilesInPath(FileIndex);

			const UBOOL	bIsShaderCacheFile	= FString(*Filename).ToUpper().InStr( TEXT("SHADERCACHE") ) != INDEX_NONE;
			const UBOOL	bIsAutoSave			= FString(*Filename).ToUpper().InStr( TEXT("AUTOSAVES") ) != INDEX_NONE;
			// Skip auto-save maps & shader caches
			if (bIsShaderCacheFile || bIsAutoSave)
			{
				continue;
			}
			if (((bSkipMapPackages == TRUE) && (Filename.GetExtension() == FURL::DefaultMapExt)) ||
				((bMapsOnly == TRUE) && (Filename.GetExtension() != FURL::DefaultMapExt)))
			{
				// Skip non-maps or maps as requested
				continue;
			}
			if ((bSkipScriptPackages == TRUE) && (Filename.GetExtension().ToUpper() == TEXT("U")))
			{
				// This assumes all script ends in '.u'...
				continue;
			}

			// Add it to the list!
			PackageList.AddItem(Filename);
		}
	}
}

/**
 *	Process the list of packages
 */
void UBaseObjectTagGeneratorCommandlet::ProcessPackageList()
{
	INT GCCount = 0;

	for (INT PackageIdx = 0; PackageIdx < PackageList.Num(); PackageIdx++)
	{
		UBOOL bResavePackage = FALSE;
		const FString& PackageName = PackageList(PackageIdx);

		warnf(NAME_Log, TEXT("Loading package %4d of %4d: %s"), (PackageIdx + 1), PackageList.Num(), *PackageName);
		
		UPackage* Package = UObject::LoadPackage(NULL, *PackageName, LOAD_NoWarn);

		if (Package == NULL)
		{
			warnf(NAME_Error, TEXT("Error loading %s!"), *PackageName);
			continue;
		}

		UBOOL bIsMapPackage = Package->ContainsMap();

		// Iterate over all objects looking for the asset
		for (TObjectIterator<UObject> It; It; ++It)
		{
			UObject* Object = *It;
			UClass* ObjClass = Object->GetClass();

			UBOOL bIsClassOfInterest = (ObjectClassesOfInterest.Find(ObjClass) != NULL);

			if (bIsClassOfInterest == FALSE)
			{
				for (TMap<UClass*,UBOOL>::TIterator CheckIt(ObjectClassesOfInterest); CheckIt; ++CheckIt)
				{
					UClass* CheckClass = CheckIt.Key();

					if (ObjClass->IsChildOf(CheckClass) == TRUE)
					{
						bIsClassOfInterest = TRUE;
						break;
					}
				}
			}

			if (bIsClassOfInterest == TRUE)
			{
				bResavePackage |= ProcessObject(Object, Package);
			}
		}

		// Save package if requested
		if (bResavePackage == TRUE)
		{
			FString OutFileName;
			
			if (GPackageFileCache->FindPackageFile(*PackageName, NULL, OutFileName))
			{
				UObject::SavePackage(Package, NULL, RF_Standalone, *OutFileName, GWarn);
			}
			else
			{
				warnf(NAME_Error, TEXT("Failed to save package %s - file name not found."), *PackageName);
			}
		}

		// Save shader caches if requested...
		if (bSaveShaderCaches == TRUE)
		{
			SaveLocalShaderCaches();
		}

		// GC at the rate requested (if any)
		if (((++GCCount % GCRate) == 0) || (bIsMapPackage == TRUE))
		{
			UObject::CollectGarbage(RF_Native);

			if (bIsMapPackage == TRUE)
			{
				GCCount = 0;
			}
		}
	}
}

UBOOL UBaseObjectTagGeneratorCommandlet::ProcessObject(UObject* InObject, UPackage* InPackage)
{
	// This should be overridden to process the objects as you desire in your commandlet!
	return FALSE;
}

/**
 *	Update the tags with the current lists
 *
 *	@return	UBOOL		TRUE if successful, FALSE if not.
 */
UBOOL UBaseObjectTagGeneratorCommandlet::UpdateTags()
{
	UBOOL bResult = TRUE;
	if (ObjectsToTag.Num() != 0)
	{
		if (bSkipTags == FALSE)
		{
			// Setup our GAD collections
			GADHelper = new FGADHelper();
			check(GADHelper);
			GADHelper->Initialize();
			if (bClearTags == TRUE)
			{
				for (INT TagIdx = 0; TagIdx < TagNames.Num(); TagIdx++)
				{
					GADHelper->ClearTagFromAssets(TagNames(TagIdx));
				}
			}
		}

		for (INT TagSetIdx = 0; TagSetIdx < ObjectsToTag.Num(); TagSetIdx++)
		{
			if (TagSetIdx < TagNames.Num())
			{
				TMap<FString,UBOOL>& ObjectsToAdd = ObjectsToTag(TagSetIdx);

				if (ObjectsToAdd.Num() > 0)
				{
					TArray<FString> AssetList;
					for (TMap<FString,UBOOL>::TIterator It(ObjectsToAdd); It; ++It)
					{
						AssetList.AddItem(It.Key());
					}

					if (bSkipTags == FALSE)
					{
						if (bUpdateTagsOnly == FALSE)
						{
							if (GADHelper->SetTaggedAssets(TagNames(TagSetIdx), AssetList) == TRUE)
							{
								// Clear the current lists...
								ObjectsToAdd.Empty();
							}
							else
							{
								bResult = FALSE;
							}
						}
						else
						{
							TArray<FString> EmptyRemove;
							if (GADHelper->UpdateTaggedAssets(TagNames(TagSetIdx), AssetList, EmptyRemove) == TRUE)
							{
								// Clear the current lists...
								ObjectsToAdd.Empty();
							}
							else
							{
								bResult = FALSE;
							}
						}
					}
					else
					{
						// Simply log them out...
						warnf(NAME_Log, TEXT("%s"), *(TagNames(TagSetIdx)));
						for (INT DumpIdx = 0; DumpIdx < AssetList.Num(); DumpIdx++)
						{
							warnf(NAME_Log, TEXT("\t%s"), *(AssetList(DumpIdx)));
						}
					}
				}
			}
			else
			{
				warnf(NAME_Warning, TEXT("Too many Tags (%d), not enough tags names (%d)"),
					ObjectsToTag.Num(), TagNames.Num());
				bResult = FALSE;
			}
		}
	}
	else
	{
		warnf(NAME_Log, TEXT("No objects found!"));
	}
	return bResult;
}

INT UBaseObjectTagGeneratorCommandlet::Main(const FString& Params)
{
	ParseCommandLine(*Params, Tokens, Switches);

	// Startup the commandlet
	if (Startup() == FALSE)
	{
		warnf(NAME_Warning, TEXT("Startup() failed... exiting!"));
		return -1;
	}

	// Initialize the parameters
	if (Initialize() == FALSE)
	{
		warnf(NAME_Warning, TEXT("Initialize() failed... exiting!"));
		return -1;
	}

	GeneratePackageList();
	ProcessPackageList();

	// Update the tags
	UpdateTags();

	// Shut it down!
	Shutdown();

	return 0;
}

IMPLEMENT_CLASS(UBaseObjectTagGeneratorCommandlet);

/*-----------------------------------------------------------------------------
	FindPSysReferencesToMesh commandlet.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UFindPSysReferencesToMeshCommandlet);
/**
 *	Startup the commandlet.
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UFindPSysReferencesToMeshCommandlet::Startup()
{
	if (Super::Startup() == TRUE)
	{
		bSkipMapPackages = TRUE;
		bClearTags = TRUE;
		// skipping the tags will result in the output just going to the log!
		bSkipTags = TRUE;
		return TRUE;
	}
	return FALSE;
}

/**
 *	Shutdown the commandlet
 */
void UFindPSysReferencesToMeshCommandlet::Shutdown()
{
	Super::Shutdown();
}

/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UFindPSysReferencesToMeshCommandlet::Initialize()
{
	UBaseObjectTagGeneratorCommandlet::Initialize();
	// Setup the classes we are interested in
	ObjectClassesOfInterest.Set(UParticleSystem::StaticClass(),TRUE);

	// Parse out the list of meshes...
	for (INT TokenIdx = 0; TokenIdx < Tokens.Num(); TokenIdx++)
	{
		FString MeshPathName = Tokens(TokenIdx);
		MeshesOfInterest.Set(MeshPathName, TagNames.Num());
		TagNames.AddItem(MeshPathName);
		ObjectsToTag.AddZeroed();
	}
	return TRUE;
}

/**
 *	Process the given object
 *
 *	@param	InObject	The object to process
 *	@param	InPackage	The package being processed
 */
UBOOL UFindPSysReferencesToMeshCommandlet::ProcessObject(UObject* InObject, UPackage* InPackage)
{
	UParticleSystem* PSys = Cast<UParticleSystem>(InObject);
	if (PSys != NULL)
	{
		for (INT EmitterIdx = 0; EmitterIdx < PSys->Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = PSys->Emitters(EmitterIdx);
			if (Emitter != NULL)
			{
				UBOOL bIsMeshEmitter = TRUE;
				for (INT LODIdx = 0; (LODIdx < Emitter->LODLevels.Num()) && bIsMeshEmitter; LODIdx++)
				{
					UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIdx);
					if (LODLevel != NULL)
					{
						UParticleModuleTypeDataMesh* MeshTD = Cast<UParticleModuleTypeDataMesh>(LODLevel->TypeDataModule);
						if (MeshTD != NULL)
						{
							if (MeshTD->Mesh != NULL)
							{
								INT* AssetIdx = MeshesOfInterest.Find(MeshTD->Mesh->GetPathName());
								if (AssetIdx != NULL)
								{

									check(*AssetIdx < ObjectsToTag.Num());
									TMap<FString,UBOOL>& ObjectsToAdd = ObjectsToTag(*AssetIdx);
									ObjectsToAdd.Set(PSys->GetPathName(), TRUE);
								}
							}
						}
						else
						{
							bIsMeshEmitter = FALSE;
						}
					}
				}
			}
		}
	}
	return FALSE;
}

INT UFindPSysReferencesToMeshCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

/*-----------------------------------------------------------------------------
	FindPSysWithBadAutoActivateSetting commandlet.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleAuditCommandlet);
	/** ParticleModule to number of times it is used  */
	TMap<FString,INT> ModuleInstances;
	/** ParticleModule to number of times it is used (per-emitter) */
	TMap<FString,INT> ModuleUsages;

/**
 *	Startup the commandlet.
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UParticleModuleAuditCommandlet::Startup()
{
	if (Super::Startup() == TRUE)
	{
		bSkipMapPackages = TRUE;
		bSkipScriptPackages = TRUE;
		return TRUE;
	}
	return FALSE;
}

/**
 *	Shutdown the commandlet
 */
void UParticleModuleAuditCommandlet::Shutdown()
{
	Super::Shutdown();
}

/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UParticleModuleAuditCommandlet::Initialize()
{
	UBaseObjectTagGeneratorCommandlet::Initialize();
	// Setup the classes we are interested in
	ObjectClassesOfInterest.Set(UParticleSystem::StaticClass(),TRUE);
	TagNames.AddItem(TEXT("ParticleModuleAudit"));
	return TRUE;
}

/**
 *	Process the given object
 *
 *	@param	InObject	The object to process
 *	@param	InPackage	The package being processed
 */
UBOOL UParticleModuleAuditCommandlet::ProcessObject(UObject* InObject, UPackage* InPackage)
{
	UParticleSystem* PSys = Cast<UParticleSystem>(InObject);
	if (PSys != NULL)
	{
		ParticleSystemsProcessed++;

		TMap<UParticleModule*,UBOOL> ModulesInPSys;

		for (INT EmitterIdx = 0; EmitterIdx < PSys->Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = PSys->Emitters(EmitterIdx);
			if (Emitter != NULL)
			{
				EmittersProcessed++;
				for (INT LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
				{
					UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIdx);
					if (LODLevel != NULL)
					{
						for (INT ModuleIdx = 0; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
						{
							UParticleModule* Module = LODLevel->Modules(ModuleIdx);
							if (Module != NULL)
							{
								if (Module->bEnabled == TRUE)
								{
									FString ModuleClassName = Module->GetClass()->GetName();
									if (ModulesInPSys.Find(Module) == NULL)
									{
										INT* InstCountPtr = ModuleInstances.Find(ModuleClassName);
										if (InstCountPtr == NULL)
										{
											INT Temp = 0;
											ModuleInstances.Set(ModuleClassName, Temp);
											InstCountPtr = ModuleInstances.Find(ModuleClassName);
										}
										check(InstCountPtr != NULL);
										*InstCountPtr = *InstCountPtr + 1;

										ModulesInPSys.Set(Module, TRUE);
									}

									INT* UsageCountPtr = ModuleUsages.Find(ModuleClassName);
									if (UsageCountPtr == NULL)
									{
										INT Temp = 0;
										ModuleUsages.Set(ModuleClassName, Temp);
										UsageCountPtr = ModuleUsages.Find(ModuleClassName);
									}
									check(UsageCountPtr != NULL);
									*UsageCountPtr = *UsageCountPtr + 1;
								}
							}
						}
					}
				}
			}
		}
	}
	return FALSE;
}

/** Sorts FParticleTickStats from largest AccumTickTime to smallest. */
IMPLEMENT_COMPARE_CONSTREF(FString, UnPackageUtilities, { return appStricmp(*A,*B); })

/**
 *	Update the tags with the current lists
 *
 *	@return	UBOOL		TRUE if successful, FALSE if not.
 */
UBOOL UParticleModuleAuditCommandlet::UpdateTags()
{
//	if (Super::UpdateTags() == TRUE)
	{
		// Dump out the list of bad emitters
		if (ModuleInstances.Num() > 0)
		{
			const FString TemplateFileName = FString::Printf(TEXT("%sModuleAudit-%s.csv"),*appProfilingDir(),*appSystemTimeString());
			FDiagnosticTableWriterCSV InstancesTableWriter(GFileManager->CreateDebugFileWriter(*TemplateFileName));

			InstancesTableWriter.AddColumn(TEXT("ParticleSystems"));
			InstancesTableWriter.AddColumn(TEXT("Emitters"));
			InstancesTableWriter.CycleRow();
			InstancesTableWriter.AddColumn(TEXT("%d"), ParticleSystemsProcessed);
			InstancesTableWriter.AddColumn(TEXT("%d"), EmittersProcessed);
			InstancesTableWriter.CycleRow();
			InstancesTableWriter.CycleRow();

			InstancesTableWriter.AddColumn(TEXT("Module"));
			InstancesTableWriter.AddColumn(TEXT("Instances"));
			InstancesTableWriter.AddColumn(TEXT("Usages"));
			InstancesTableWriter.CycleRow();

			TMap<FString,INT> TempModuleInstances = ModuleInstances;
			TempModuleInstances.KeySort<COMPARE_CONSTREF_CLASS(FString,UnPackageUtilities)>();
			for (TMap<FString,INT>::TIterator It(TempModuleInstances); It; ++It)
			{
				FString ModuleName = It.Key();
				INT ModuleInstancesCount = It.Value();
				INT* InstancesCountPtr = ModuleUsages.Find(ModuleName);
				InstancesTableWriter.AddColumn(TEXT("%s"), *ModuleName);
				InstancesTableWriter.AddColumn(TEXT("%d"), ModuleInstancesCount);
				InstancesTableWriter.AddColumn(TEXT("%d"), InstancesCountPtr ? *InstancesCountPtr : 0);
				InstancesTableWriter.CycleRow();
			}

			InstancesTableWriter.Close();
		}
		if (ModuleUsages.Num() > 0)
		{
			const FString TemplateFileName = FString::Printf(TEXT("%sModuleUsages-%s.csv"),*appProfilingDir(),*appSystemTimeString());
			FDiagnosticTableWriterCSV UsagesTableWriter(GFileManager->CreateDebugFileWriter(*TemplateFileName));

			UsagesTableWriter.AddColumn(TEXT("ParticleSystems"));
			UsagesTableWriter.AddColumn(TEXT("Emitters"));
			UsagesTableWriter.CycleRow();
			UsagesTableWriter.AddColumn(TEXT("%d"), ParticleSystemsProcessed);
			UsagesTableWriter.AddColumn(TEXT("%d"), EmittersProcessed);
			UsagesTableWriter.CycleRow();
			UsagesTableWriter.CycleRow();

			UsagesTableWriter.AddColumn(TEXT("Module"));
			UsagesTableWriter.AddColumn(TEXT("Usages"));
			UsagesTableWriter.CycleRow();

			TMap<FString,INT> TempModuleUsages = ModuleUsages;
			TempModuleUsages.KeySort<COMPARE_CONSTREF_CLASS(FString,UnPackageUtilities)>();
			for (TMap<FString,INT>::TIterator It(TempModuleUsages); It; ++It)
			{
				FString ModuleName = It.Key();
				INT ModuleUsagesCount = It.Value();
				UsagesTableWriter.AddColumn(TEXT("%s"), *ModuleName);
				UsagesTableWriter.AddColumn(TEXT("%d"), ModuleUsagesCount);
				UsagesTableWriter.CycleRow();
			}

			UsagesTableWriter.Close();
		}
	}
	return TRUE;
}

INT UParticleModuleAuditCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

/*-----------------------------------------------------------------------------
	FindPSysWithBadAutoActivateSetting commandlet.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UFindPSysWithBadAutoActivateSettingCommandlet);
/**
 *	Startup the commandlet.
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UFindPSysWithBadAutoActivateSettingCommandlet::Startup()
{
	if (Super::Startup() == TRUE)
	{
		bMapsOnly = TRUE;
		bClearTags = TRUE;
		// skipping the tags will result in the output just going to the log!
		bSkipTags = TRUE;
		return TRUE;
	}
	return FALSE;
}

/**
 *	Shutdown the commandlet
 */
void UFindPSysWithBadAutoActivateSettingCommandlet::Shutdown()
{
	Super::Shutdown();
}

/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UFindPSysWithBadAutoActivateSettingCommandlet::Initialize()
{
	UBaseObjectTagGeneratorCommandlet::Initialize();
	// Setup the classes we are interested in
	ObjectClassesOfInterest.Set(AEmitter::StaticClass(),TRUE);
	TagNames.AddItem(TEXT("LevelPlacedEmittersWithBadAutoActivate"));
	return TRUE;
}

/**
 *	Generate the list of packages to process
 */
void UFindPSysWithBadAutoActivateSettingCommandlet::GeneratePackageList()
{
	TArray<FString> MapList;
	GEditor->ParseMapSectionIni(appCmdLine(), MapList);

	if (MapList.Num() > 0)
	{
		TArray<FString> FilesInPath = GPackageFileCache->GetPackageFileList();
		INT GCIndex = 0;
		for (INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++)
		{
			const FFilename& Filename = FilesInPath(FileIndex);
			FString ShortName = Filename.GetBaseFilename().ToUpper();
			for (INT CheckIdx = 0; CheckIdx < MapList.Num(); CheckIdx++)
			{
				FString UpperName = MapList(CheckIdx).ToUpper();
				if (ShortName == UpperName)
				{
					PackageList.AddItem(Filename);

					FArchive* CheckMapPackageFile = GFileManager->CreateFileReader(*Filename);
					if (CheckMapPackageFile)
					{
						// read the package summary, which has list of sub levels
						FPackageFileSummary Summary;
						(*CheckMapPackageFile) << Summary;
						// close the map
						delete CheckMapPackageFile;
						// Make sure that it is a map!
						if ((Summary.PackageFlags & PKG_ContainsMap) != 0)
						{
							// if it's an old map, then we have to load it to get the list of sublevels
							if (Summary.GetFileVersion() < VER_ADDITIONAL_COOK_PACKAGE_SUMMARY)
							{
								warnf(NAME_Log, TEXT("  Old map, SKIPPING %s"), *Filename);
							}
							else
							{
								// Streaming levels present?
								for (INT AdditionalPackageIndex = 0; AdditionalPackageIndex < Summary.AdditionalPackagesToCook.Num(); AdditionalPackageIndex++)
								{
									FString ContainedLevelName;
									if (GPackageFileCache->FindPackageFile(*Summary.AdditionalPackagesToCook(AdditionalPackageIndex), NULL, ContainedLevelName))
									{
										FArchive* AdditionalPackageFile = GFileManager->CreateFileReader(*ContainedLevelName);
										if (AdditionalPackageFile)
										{
											// read the package summary, which has list of sub levels
											FPackageFileSummary AdditionalSummary;
											(*AdditionalPackageFile) << AdditionalSummary;
											// close the map
											delete AdditionalPackageFile;

											if ((AdditionalSummary.PackageFlags & PKG_ContainsMap) != 0)
											{
												PackageList.AddUniqueItem(ContainedLevelName);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	else
	{
		Super::GeneratePackageList();
	}
}

/**
 *	Process the given object
 *
 *	@param	InObject	The object to process
 *	@param	InPackage	The package being processed
 */
UBOOL UFindPSysWithBadAutoActivateSettingCommandlet::ProcessObject(UObject* InObject, UPackage* InPackage)
{
	AEmitter* EmitterActor = Cast<AEmitter>(InObject);
	if (EmitterActor != NULL)
	{
		UBOOL bIsTriggered = FALSE;
		// See what is referencing it...
		TArray<FReferencerInformation> OutInternalReferencers;
		TArray<FReferencerInformation> OutExternalReferencers;
		EmitterActor->RetrieveReferencers(&OutInternalReferencers, &OutExternalReferencers, FALSE);
		for (INT OIRefIdx = 0; (OIRefIdx < OutInternalReferencers.Num()) && !bIsTriggered; OIRefIdx++)
		{
			FReferencerInformation& InternalReferencer = OutInternalReferencers(OIRefIdx);
			if (InternalReferencer.Referencer != NULL)
			{
				USeqVar_Object* SeqVarObject = Cast<USeqVar_Object>(InternalReferencer.Referencer);
				if (SeqVarObject != NULL)
				{
					bIsTriggered = TRUE;
				}
			}
		}

		for (INT OERefIdx = 0; (OERefIdx < OutExternalReferencers.Num()) && !bIsTriggered; OERefIdx++)
		{
			FReferencerInformation& ExternalReferencer = OutExternalReferencers(OERefIdx);
			if (ExternalReferencer.Referencer != NULL)
			{
				USeqVar_Object* SeqVarObject = Cast<USeqVar_Object>(ExternalReferencer.Referencer);
				if (SeqVarObject != NULL)
				{
					bIsTriggered = TRUE;
				}
			}
		}

		if (bIsTriggered == TRUE)
		{
			if (EmitterActor->ParticleSystemComponent &&
				EmitterActor->ParticleSystemComponent->bAutoActivate)
			{
				if (ObjectsToTag.Num() == 0)
				{
					ObjectsToTag.AddZeroed();
				}
				TMap<FString,UBOOL>& ObjList = ObjectsToTag(0);
				ObjList.Set(EmitterActor->GetFullName(), TRUE);

				if (BadAutoActivateEmitterToLevel.Find(EmitterActor->GetPathName()) == NULL)
				{
					BadAutoActivateEmitterToLevel.Set(EmitterActor->GetPathName(), InPackage->GetName());
				}
			}
		}
	}
	return FALSE;
}

/**
 *	Update the tags with the current lists
 *
 *	@return	UBOOL		TRUE if successful, FALSE if not.
 */
UBOOL UFindPSysWithBadAutoActivateSettingCommandlet::UpdateTags()
{
	if (Super::UpdateTags() == TRUE)
	{
		// Dump out the list of bad emitters
		if (BadAutoActivateEmitterToLevel.Num() > 0)
		{
			// Place in the <UE3>\<GAME>\Logs\<InFolderName> folder
			FString Filename = FString::Printf(TEXT("%sLogs%s%s%s%s-%s.csv"),*appGameDir(),PATH_SEPARATOR,TEXT("Audit"),
				PATH_SEPARATOR,TEXT("EmittersWithBadAutoActivate"),*appSystemTimeString());
			FArchive* OutputStream = GFileManager->CreateDebugFileWriter(*Filename);
			if (OutputStream != NULL)
			{
				OutputStream->Logf(TEXT("EmitterActor,Level Contained In,..."));
				for (TMap<FString,FString>::TIterator It(BadAutoActivateEmitterToLevel); It; ++It)
				{
					FString& EmitterName = It.Key();
					FString& LevelName = It.Value();
					OutputStream->Logf(TEXT("%s,%s"), *EmitterName, *LevelName);
				}

				OutputStream->Close();
				delete OutputStream;
			}
			else
			{
				warnf(NAME_Warning, TEXT("Failed to create output stream %s"), *Filename);
			}
		}
		return TRUE;
	}
	return FALSE;
}

INT UFindPSysWithBadAutoActivateSettingCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

/*-----------------------------------------------------------------------------
	UFindPSysWithZOrientTowardsCameraCommandlet commandlet.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UFindPSysWithZOrientTowardsCameraCommandlet);
/**
 *	Startup the commandlet.
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UFindPSysWithZOrientTowardsCameraCommandlet::Startup()
{
	if (Super::Startup() == TRUE)
	{
		bSkipMapPackages = TRUE;
		bClearTags = TRUE;
		// skipping the tags will result in the output just going to the log!
		bSkipTags = TRUE;
		return TRUE;
	}
	return FALSE;
}

/**
 *	Shutdown the commandlet
 */
void UFindPSysWithZOrientTowardsCameraCommandlet::Shutdown()
{
	Super::Shutdown();
}

/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UFindPSysWithZOrientTowardsCameraCommandlet::Initialize()
{
	UBaseObjectTagGeneratorCommandlet::Initialize();
	// Setup the classes we are interested in
	ObjectClassesOfInterest.Set(UParticleSystem::StaticClass(),TRUE);
	TagNames.AddItem(TEXT("PSys_OrientZTowardsCamera"));
	return TRUE;
}

/**
 *	Process the given object
 *
 *	@param	InObject	The object to process
 *	@param	InPackage	The package being processed
 */
UBOOL UFindPSysWithZOrientTowardsCameraCommandlet::ProcessObject(UObject* InObject, UPackage* InPackage)
{
	UParticleSystem* PSys = Cast<UParticleSystem>(InObject);
	if (PSys != NULL)
	{
		if (PSys->bOrientZAxisTowardCamera == TRUE)
		{
			if (ObjectsToTag.Num() == 0)
			{
				ObjectsToTag.AddZeroed();
			}
			TMap<FString,UBOOL>& ObjList = ObjectsToTag(0);
			ObjList.Set(PSys->GetFullName(), TRUE);
		}
	}
	return FALSE;
}

INT UFindPSysWithZOrientTowardsCameraCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

/*-----------------------------------------------------------------------------
	UFindPSysWithCollisionEnabledCommandlet.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UFindPSysWithCollisionEnabledCommandlet);
/**
 *	Startup the commandlet.
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UFindPSysWithCollisionEnabledCommandlet::Startup()
{
	if (Super::Startup() == TRUE)
	{
		bSkipMapPackages = TRUE;
		bClearTags = TRUE;
		// skipping the tags will result in the output just going to the log!
		bSkipTags = TRUE;
		return TRUE;
	}
	return FALSE;
}

/**
 *	Shutdown the commandlet
 */
void UFindPSysWithCollisionEnabledCommandlet::Shutdown()
{
	Super::Shutdown();
}

/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UFindPSysWithCollisionEnabledCommandlet::Initialize()
{
	UBaseObjectTagGeneratorCommandlet::Initialize();
	// Setup the classes we are interested in
	ObjectClassesOfInterest.Set(UParticleSystem::StaticClass(),TRUE);
	TagNames.AddItem(TEXT("PSys_CollisionEnabled"));
	TagNames.AddItem(TEXT("PSys_CollisionActorEnabled"));
	return TRUE;
}

/**
 *	Process the given object
 *
 *	@param	InObject	The object to process
 *	@param	InPackage	The package being processed
 */
UBOOL UFindPSysWithCollisionEnabledCommandlet::ProcessObject(UObject* InObject, UPackage* InPackage)
{
	UParticleSystem* PSys = Cast<UParticleSystem>(InObject);
	if (PSys != NULL)
	{
		UBOOL bHasCollisionEnabled = FALSE;
		UBOOL bHasCollisionActorEnabled = FALSE;
		for (INT EmitterIdx = 0; EmitterIdx < PSys->Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = PSys->Emitters(EmitterIdx);
			if (Emitter != NULL)
			{
				for (INT LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
				{
					UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIdx);
					if (LODLevel != NULL)
					{
						for (INT ModuleIdx = 0; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
						{
							UParticleModule* Module = LODLevel->Modules(ModuleIdx);
							UParticleModuleCollision* CollisionModule = Cast<UParticleModuleCollision>(Module);
							UParticleModuleCollisionActor* CollisionActorModule = Cast<UParticleModuleCollisionActor>(Module);
							if (CollisionActorModule != NULL)
							{
								bHasCollisionActorEnabled |= CollisionActorModule->bEnabled;
							}
							else if (CollisionModule != NULL)
							{
								bHasCollisionEnabled |= CollisionModule->bEnabled;
							}
						}
					}
				}
			}
		}

		if ((bHasCollisionEnabled == TRUE) || (bHasCollisionActorEnabled == TRUE))
		{
			if (ObjectsToTag.Num() == 0)
			{
				ObjectsToTag.AddZeroed(2);
			}

			if (bHasCollisionEnabled == TRUE)
			{
				TMap<FString,UBOOL>& ObjList = ObjectsToTag(0);
				ObjList.Set(PSys->GetFullName(), TRUE);
			}
			if (bHasCollisionActorEnabled == TRUE)
			{
				TMap<FString,UBOOL>& ObjList = ObjectsToTag(1);
				ObjList.Set(PSys->GetFullName(), TRUE);
			}
		}
	}
	return FALSE;
}

INT UFindPSysWithCollisionEnabledCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

/*-----------------------------------------------------------------------------
	Helper function for validating a list of maps
-----------------------------------------------------------------------------*/
/**
 *	Validate the given list of map names as real map packages
 *
 *	@param	InMapList			The list of maps to validate
 *	@param	OutValidatedList	Valid maps from the list
 */
void PackageUtilities_ValidateMapList(const TArray<FString>& InMapList, TArray<FString>& OutValidatedList)
{
	TArray<FString> AllPackages = GPackageFileCache->GetPackageFileList();
	if (InMapList.Num() > 0)
	{
		for (INT PkgIdx = 0; PkgIdx < AllPackages.Num(); PkgIdx++)
		{
			FFilename PackageName = AllPackages(PkgIdx);
			for (INT CheckIdx = 0; CheckIdx < InMapList.Num(); CheckIdx++)
			{
				if (PackageName.GetBaseFilename().ToUpper() == InMapList(CheckIdx).ToUpper())
				{
					// A token was a package... see if it's a map
					UObject::BeginLoad();
					ULinkerLoad* Linker = UObject::GetPackageLinker(NULL,*PackageName,LOAD_NoVerify,NULL,NULL);
					UObject::EndLoad();
					if (Linker != NULL)
					{
						if ((Linker->Summary.PackageFlags & PKG_ContainsMap) == PKG_ContainsMap)
						{
							OutValidatedList.AddItem(PackageName);
						}
					}
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	UFindStaticMeshCleanIssuesCommandlet commandlet.
-----------------------------------------------------------------------------*/
/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UFindStaticMeshCleanIssuesCommandlet::Initialize()
{
	UBaseObjectTagGeneratorCommandlet::Initialize();

	// Setup the classes we are interested in
	ObjectClassesOfInterest.Set(UObject::StaticClass(),TRUE);

	SkipStaticMeshCleanMeshes.Empty();
	SkipStaticMeshCleanPackages.Empty();
	CheckStaticMeshCleanClasses.Empty();

	const TCHAR* SMCleanSkipIniSection = TEXT("Cooker.CleanStaticMeshMtrlSkip");
	FConfigSection* SMCleanSkipIniList = GConfig->GetSectionPrivate(SMCleanSkipIniSection, FALSE, TRUE, GEditorIni);
	if (SMCleanSkipIniList)
	{
		// Fill in the classes, packages, and static mesh lists to skip cleaning
		for (FConfigSectionMap::TIterator It(*SMCleanSkipIniList); It; ++It)
		{
			const FName TypeString = It.Key();
			FString ValueString = It.Value();

			if (TypeString == TEXT("Class"))
			{
				// Add the class name to the list
				CheckStaticMeshCleanClasses.Set(ValueString, TRUE);
				warnf(NAME_Log, TEXT("StaticMesh CleanUp: Adding Class      : %s"), *ValueString);
			}
			else if (TypeString == TEXT("StaticMesh"))
			{
				// Add the mesh name to the list
				SkipStaticMeshCleanMeshes.Set(ValueString, TRUE);
				warnf(NAME_Log, TEXT("StaticMesh CleanUp: Adding StaticMesh : %s"), *ValueString);
			}
			else if (TypeString == TEXT("Package"))
			{
				// Add the package name to the list
				SkipStaticMeshCleanPackages.Set(ValueString, TRUE);
				warnf(NAME_Log, TEXT("StaticMesh CleanUp: Adding Package    : %s"), *ValueString);
			}
		}
	}

	return TRUE;
}

/**
 *	Generate the list of packages to process
 */
void UFindStaticMeshCleanIssuesCommandlet::GeneratePackageList()
{
	PackageList.Empty();

	TArray<FString> FilesInPath;

	// See if one of the tokens is a map...
	if (Tokens.Num() > 0)
	{
		PackageUtilities_ValidateMapList(Tokens, FilesInPath);
	}

	if (FilesInPath.Num() == 0)
	{
		FString MapIniSection = TEXT("");
		const TCHAR* MapIniTag = TEXT("MAPINI=");
		for (INT SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++)
		{
			FString CurrSwitch = Switches(SwitchIdx);
			INT MapIniIndex = CurrSwitch.InStr(MapIniTag, FALSE, TRUE);
			if (MapIniIndex != INDEX_NONE)
			{
				MapIniSection = CurrSwitch.Right(CurrSwitch.Len() - appStrlen(MapIniTag));
			}
		}

		if (MapIniSection.Len() > 0)
		{
			warnf(NAME_Log, TEXT("Using map INI section %s"), *MapIniSection);
			TArray<FString> IniMapList;
			GEditor->LoadMapListFromIni(MapIniSection, IniMapList);
			PackageUtilities_ValidateMapList(IniMapList, FilesInPath);
		}
		
		if (FilesInPath.Num() == 0)
		{
			warnf(NAME_Log, TEXT("Using AllPackages as the file list!"));
			FilesInPath = GPackageFileCache->GetPackageFileList();
		}
	}

	INT GCIndex = 0;
	TArray<FString> LocalPackageList;
	for (INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++)
	{
		const FFilename& Filename = FilesInPath(FileIndex);

		const UBOOL	bIsShaderCacheFile	= FString(*Filename).ToUpper().InStr( TEXT("SHADERCACHE") ) != INDEX_NONE;
		const UBOOL	bIsAutoSave			= FString(*Filename).ToUpper().InStr( TEXT("AUTOSAVES") ) != INDEX_NONE;
		// Skip auto-save maps & shader caches
		if (bIsShaderCacheFile || bIsAutoSave)
		{
			continue;
		}
		if (((bSkipMapPackages == TRUE) && (Filename.GetExtension() == FURL::DefaultMapExt)) ||
			((bMapsOnly == TRUE) && (Filename.GetExtension() != FURL::DefaultMapExt)))
		{
			// Skip non-maps or maps as requested
			continue;
		}
		if ((bSkipScriptPackages == TRUE) && (Filename.GetExtension().ToUpper() == TEXT("U")))
		{
			// This assumes all script ends in '.u'...
			continue;
		}

		FString FilenameOnly = Filename.GetBaseFilename();
		UBOOL bSinglePlayerMap = (FilenameOnly.StartsWith(TEXT("SP_")) == TRUE);
		UBOOL bMultiplayerMap = (FilenameOnly.StartsWith(TEXT("MP_")) == TRUE);
		if (bSinglePlayerMap || bMultiplayerMap)
		{
			debugf(TEXT("Checking map for validity: %s"), *Filename);
			if (
				(Filename.InStr(TEXT("\\DLC\\"), FALSE, TRUE) == INDEX_NONE) && 
				(Filename.InStr(TEXT("\\CINEMATICS\\"), FALSE, TRUE) == INDEX_NONE) && 
				((bSinglePlayerMap && (Filename.InStr(TEXT("\\SP_"), FALSE, TRUE) != INDEX_NONE)) ||
				(bMultiplayerMap && (Filename.InStr(TEXT("\\MP_MAPS\\"), FALSE, TRUE) != INDEX_NONE) &&
				 (Filename.InStr(TEXT("\\ONHOLD"), FALSE, TRUE) == INDEX_NONE)))
				)
			{
				// Add it to the list!
				LocalPackageList.AddItem(Filename);
			}
		}
	}

	PersistentMapInfoHelper.SetCallerInfo(this);
	PersistentMapInfoHelper.SetPersistentMapInfoGenerationVerboseLevel(FPersistentMapInfo::VL_Simple);
	PersistentMapInfoHelper.GeneratePersistentMapList(LocalPackageList, TRUE, FALSE);

	// Now, extract the PMaps only for the real package list
	TArray<FString> PMapList;
	PersistentMapInfoHelper.GetPersistentMapList(PMapList);
	for (INT LocalIdx = 0; LocalIdx < LocalPackageList.Num(); LocalIdx++)
	{
		FFilename LocalFilename = LocalPackageList(LocalIdx);
		FString LocalUpper = LocalFilename.GetBaseFilename().ToUpper();
		for (INT PMapIdx = 0; PMapIdx < PMapList.Num(); PMapIdx++)
		{
			FString PMapUpper = PMapList(PMapIdx).ToUpper();
			if (LocalUpper == PMapUpper)
			{
				PackageList.AddItem(LocalFilename);
				break;
			}
		}
	}
}

/**
 *	Process the list of packages
 */
void UFindStaticMeshCleanIssuesCommandlet::ProcessPackageList()
{
	INT GCCount = 0;
	for (INT PackageIdx = 0; PackageIdx < PackageList.Num(); PackageIdx++)
	{
		const FFilename& PackageName = PackageList(PackageIdx);

		FString ShortPMapName = PackageName.GetBaseFilename();
		TArray<FString> InnerPackageList;
		InnerPackageList.AddItem(PackageName);
		const TArray<FString>* SubLevelList = PersistentMapInfoHelper.GetPersistentMapContainedLevelsList(ShortPMapName);
		if (SubLevelList != NULL)
		{
			PackageUtilities_ValidateMapList(*SubLevelList, InnerPackageList);
		}

		TMap<FString,FStaticMeshToSubLevelInfo>* StaticMeshInfo = PMapToStaticMeshMapping.Find(ShortPMapName);
		if (StaticMeshInfo == NULL)
		{
			TMap<FString,FStaticMeshToSubLevelInfo> TempStaticMeshInfo;
			PMapToStaticMeshMapping.Set(ShortPMapName, TempStaticMeshInfo);
			StaticMeshInfo = PMapToStaticMeshMapping.Find(ShortPMapName);
		}
		else
		{
			warnf(NAME_Warning, TEXT("PMap already found in StaticMeshInfo mapping: %s"), *ShortPMapName);
		}
		check(StaticMeshInfo);

		// Grab the classes that we need to check for static meshes...
		TArray<UClass*> ClassesToCheck;
		for (TMap<FString,UBOOL>::TIterator ClassIt(CheckStaticMeshCleanClasses); ClassIt; ++ClassIt)
		{
			// Try to find the class of interest...
			UClass* CheckClass = (UClass*)(UObject::StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *(ClassIt.Key()), TRUE));
			if (CheckClass != NULL)
			{
				warnf(NAME_Log, TEXT("Found class to check for static meshes: %s"), *(CheckClass->GetPathName()));
				ClassesToCheck.AddItem(CheckClass);
			}
		}

		warnf(NAME_Log, TEXT("Processing PMap %3d of %3d: %s"), (PackageIdx + 1), PackageList.Num(), *ShortPMapName);

		// For each sub level, find the static meshes & the static meshes referenced by cleanup whitelist classes
		for (INT InnerIdx = 0; InnerIdx < InnerPackageList.Num(); InnerIdx++)
		{
			/** The dependency chain used to find the potentially spawned static meshes */
			TSet<FDependencyRef> FoundDependencies;

			const FFilename& InnerPackageName = InnerPackageList(InnerIdx);
			FString ShortInnerPackageName = InnerPackageName.GetBaseFilename();
			warnf(NAME_Log, TEXT("\tLoading package %4d of %4d: %s"), (InnerIdx + 1), InnerPackageList.Num(), *InnerPackageName);

			UObject::BeginLoad();
			ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *InnerPackageName, LOAD_Quiet|LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
			UObject::EndLoad();

			UPackage* Package = UObject::LoadPackage(NULL, *InnerPackageName, LOAD_NoWarn);
			if (Package == NULL)
			{
				warnf(NAME_Error, TEXT("Error loading %s!"), *InnerPackageName);
				continue;
			}
			UBOOL bIsMapPackage = Package->ContainsMap();

			TArray<INT> GatherDependenciesList;

			// Get the linker
			if (Linker != NULL)
			{
				FName StaticMesh_Name = FName(TEXT("StaticMesh"));
				// Walk the export/import maps looking for StaticMeshes
				for (INT ExportIdx = 0; ExportIdx < Linker->ExportMap.Num(); ExportIdx++)
				{
					FString FoundStaticMeshName;
					FObjectExport& Export = Linker->ExportMap(ExportIdx);
					if (Export.ClassIndex != 0)
					{
						FName ClassName = Linker->GetExportClassName(ExportIdx);
						if (ClassName == StaticMesh_Name)
						{
							FoundStaticMeshName = Linker->GetExportPathName(ExportIdx);
							FStaticMeshToSubLevelInfo* SM2SLInfo = StaticMeshInfo->Find(FoundStaticMeshName);
							if (SM2SLInfo == NULL)
							{
								FStaticMeshToSubLevelInfo TempSM2SLInfo;
								StaticMeshInfo->Set(FoundStaticMeshName, TempSM2SLInfo);
								SM2SLInfo = StaticMeshInfo->Find(FoundStaticMeshName);
							}
							check(SM2SLInfo);
							// By default, we set it to *not* being a possible spawner...
							SM2SLInfo->SubLevelsContainedIn.Set(ShortInnerPackageName, FALSE);
						}
						else
						{
							UClass* LoadClass = (UClass*)(UObject::StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *(ClassName.ToString()), TRUE));
							if (LoadClass != NULL)
							{
								for (INT CheckClassIdx = 0; CheckClassIdx < ClassesToCheck.Num(); CheckClassIdx++)
								{
									UClass* CheckClass = ClassesToCheck(CheckClassIdx);
									if (CheckClass != NULL)
									{
										if (LoadClass->IsChildOf(CheckClass))
										{
											GatherDependenciesList.AddUniqueItem(ExportIdx);
											break;
										}
									}
								}
							}
						}
					}
				}
				for (INT ImportIdx = 0; ImportIdx < Linker->ImportMap.Num(); ImportIdx++)
				{
					FString FoundStaticMeshName;
					FObjectImport& Import = Linker->ImportMap(ImportIdx);
					if (Import.ClassName == StaticMesh_Name)
					{
						FoundStaticMeshName = Linker->GetImportPathName(ImportIdx);
						FStaticMeshToSubLevelInfo* SM2SLInfo = StaticMeshInfo->Find(FoundStaticMeshName);
						if (SM2SLInfo == NULL)
						{
							FStaticMeshToSubLevelInfo TempSM2SLInfo;
							StaticMeshInfo->Set(FoundStaticMeshName, TempSM2SLInfo);
							SM2SLInfo = StaticMeshInfo->Find(FoundStaticMeshName);
						}
						check(SM2SLInfo);
						// By default, we set it to *not* being a possible spawner...
						SM2SLInfo->SubLevelsContainedIn.Set(ShortInnerPackageName, FALSE);
					}
				}
			}

			for (INT GatherIdx = 0; GatherIdx < GatherDependenciesList.Num(); GatherIdx++)
			{
				INT GatherExportIdx = GatherDependenciesList(GatherIdx);
				// Gather the dependecies for this object
//				warnf(NAME_Log, TEXT("Gathering ExportDependencies for %s"), *(Linker->GetExportPathName(GatherExportIdx)));
				Linker->GatherExportDependencies(GatherExportIdx, FoundDependencies);
			}

			// Check the found dependencies
			for (TSet<FDependencyRef>::TConstIterator It(FoundDependencies); It; ++It)
			{
				const FDependencyRef& Ref = *It;
				FObjectExport& DepExport = Ref.Linker->ExportMap(Ref.ExportIndex);
				if (DepExport.ClassIndex != 0)
				{
					FName ClassName = Ref.Linker->GetExportClassName(Ref.ExportIndex);
					UClass* DepClass = (UClass*)(UObject::StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *(ClassName.ToString()), TRUE));
					if (DepClass == UStaticMesh::StaticClass())
					{
						FString StaticMeshName = Ref.Linker->GetExportPathName(Ref.ExportIndex);
						FStaticMeshToSubLevelInfo* SM2SLInfo = StaticMeshInfo->Find(StaticMeshName);
						if (SM2SLInfo == NULL)
						{
							warnf(NAME_Warning, TEXT("StaticMesh ref'd but not already found?? %s"), *StaticMeshName);
							FStaticMeshToSubLevelInfo TempSM2SLInfo;
							StaticMeshInfo->Set(StaticMeshName, TempSM2SLInfo);
							SM2SLInfo = StaticMeshInfo->Find(StaticMeshName);
						}
						check(SM2SLInfo);
						// set it to being a possible spawner...
						SM2SLInfo->SubLevelsContainedIn.Set(ShortInnerPackageName, TRUE);
					}
				}
			}

			// Save shader caches if requested...
			if (bSaveShaderCaches == TRUE)
			{
				SaveLocalShaderCaches();
			}

			// GC at the rate requested (if any)
			if (((++GCCount % GCRate) == 0) || (bIsMapPackage == TRUE))
			{
				UObject::CollectGarbage(RF_Native);
				if (bIsMapPackage == TRUE)
				{
					GCCount = 0;
				}
			}
		}
	}
}

/**
 *	Process the given object
 *
 *	@param	InObject	The object to process
 *	@param	InPackage	The package being processed
 */
UBOOL UFindStaticMeshCleanIssuesCommandlet::ProcessObject(UObject* InObject, UPackage* InPackage)
{
	return FALSE;
}

/**
 *	Update the collections with the current lists
 *
 *	@return	UBOOL		TRUE if successful, FALSE if not.
 */
UBOOL UFindStaticMeshCleanIssuesCommandlet::UpdateTags()
{
	TMap<FString,UBOOL> StaticMeshesToWhitelist;

	for (TMap<FString,TMap<FString,FStaticMeshToSubLevelInfo> >::TIterator PMapIt(PMapToStaticMeshMapping); PMapIt; ++PMapIt)
	{
		FString& PMapName = PMapIt.Key();
		TMap<FString,FStaticMeshToSubLevelInfo>& StaticMeshInfo = PMapIt.Value();
		for (TMap<FString,FStaticMeshToSubLevelInfo>::TIterator SMIt(StaticMeshInfo); SMIt; ++SMIt)
		{
			const FString& MeshName = SMIt.Key();
			FStaticMeshToSubLevelInfo& SM2SLInfo = SMIt.Value();

			UBOOL bIsPotentiallySpawned = FALSE;
			UBOOL bIsPlaced = FALSE;
			for (TMap<FString,UBOOL>::TIterator SublevelIt(SM2SLInfo.SubLevelsContainedIn); SublevelIt; ++SublevelIt)
			{
				// If the field is true, the mesh is potentially spawned for the sublevel...
				bIsPotentiallySpawned |= SublevelIt.Value();
				// If false, the mesh is placed in the sublevel...
				bIsPlaced |= !(SublevelIt.Value());
			}

			// If both Placed & PotentiallySpawned, we need to whitelist it
			if (bIsPotentiallySpawned && bIsPlaced)
			{
				StaticMeshesToWhitelist.Set(MeshName, TRUE);
			}
		}
	}

	warnf(NAME_Log, TEXT("<------------------------------------------"));
	warnf(NAME_Log, TEXT("StaticMesh Whitelist (%d entries)"), StaticMeshesToWhitelist.Num());
	for (TMap<FString,UBOOL>::TIterator DumpIt(StaticMeshesToWhitelist); DumpIt; ++DumpIt)
	{
		const FString& MeshName = DumpIt.Key();
		warnf(NAME_Log, TEXT("%s"), *MeshName);
	}

	return TRUE;
}

// Why does the ChangePrefabSequenceClass commandlet not need a main???
INT UFindStaticMeshCleanIssuesCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

IMPLEMENT_CLASS(UFindStaticMeshCleanIssuesCommandlet);

/*-----------------------------------------------------------------------------
	UFixupMobileMaterialFogCommandlet 
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UFixupMobileMaterialFogCommandlet);

/**
 *	Startup the commandlet.
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UFixupMobileMaterialFogCommandlet::Startup ()
{
	if (Super::Startup() == TRUE)
	{
		bSkipMapPackages = TRUE;
		bClearTags = TRUE;
		bSkipTags = TRUE;

		return TRUE;
	}

	return FALSE;
}


/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UFixupMobileMaterialFogCommandlet::Initialize ()
{
	UBaseObjectTagGeneratorCommandlet::Initialize();
	
	// Setup the classes we are interested in
	ObjectClassesOfInterest.Set(UMaterial::StaticClass(),TRUE);
	TagNames.AddItem(TEXT("MaterialsWithFog"));
	
	ObjectClassesOfInterest.Set(UMaterialInstanceConstant::StaticClass(),TRUE);
	TagNames.AddItem(TEXT("MICsWithFog"));
	
	ObjectClassesOfInterest.Set(UMaterialInstanceTimeVarying::StaticClass(),TRUE);
	TagNames.AddItem(TEXT("MITVsWithFog"));

	return TRUE;
}


/**
 *	Process the given object
 *
 *	@param	InObject	The object to process
 *	@param	InPackage	The package being processed
 */
UBOOL UFixupMobileMaterialFogCommandlet::ProcessObject (UObject* InObject, UPackage* InPackage)
{
	UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(InObject);

	if (MaterialInterface != NULL)
	{
		const FMaterial* MaterialResource = MaterialInterface->GetMaterialResource();

		if (MaterialResource->GetLightingModel() == MLM_Unlit)
		{
			if ((MaterialResource->GetBlendMode() == BLEND_Additive) || ( MaterialResource->GetBlendMode() == BLEND_Translucent))
			{
				MaterialInterface->bMobileAllowFog = FALSE;

				return TRUE;
			}
		}
	}

	return FALSE;
}


INT UFixupMobileMaterialFogCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}



//======================================================================
// Commandlet for replacing a material with another material, or a MIC.
// Useful when replacing materials with master materials, to reduce shader count
// Usage: ReplaceMaterialCommandlet MaterialToBeReplaced ReplacementMaterial 
IMPLEMENT_CLASS(UReplaceMaterialCommandlet)

INT UReplaceMaterialCommandlet::Main(const FString& Params)
{
	// Retrieve list of all packages in .ini paths.
	TArray<FString> InputPackageList;
	TArray<FFilename> PackagePathNames;

	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	const UBOOL bAutoCheckOut = Switches.FindItemIndex(TEXT("AutoCheckOutPackages")) != INDEX_NONE;

	FString PackageWildcard(TEXT("*.*"));
	if (Tokens.Num() < 2)
	{
		warnf(TEXT("Syntax: ReplaceMaterial <Package.Class to remove> <Package.Class to replace with> <file/wildcard> <-flag1> <-flag2>"));
		warnf(TEXT("Flags: -ALL, -AutoCheckOutPackages"));
		return 1;
	} 
	else if ( (Tokens.Num() >= 3) && (Switches.FindItemIndex(TEXT("ALL")) == INDEX_NONE) )
	{
		PackageWildcard=Tokens(2);
	}

	NormalizePackageNames(InputPackageList, PackagePathNames, PackageWildcard, NORMALIZE_ExcludeMapPackages);

	// get the material to remove and the material to replace it with
	FString MaterialName=Tokens(0);
	UMaterialInterface* MaterialToBeReplaced = (UMaterialInterface*)StaticLoadObject(UMaterialInterface::StaticClass(), NULL, *MaterialName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
	if (MaterialToBeReplaced == NULL)
	{
		warnf(NAME_Error, TEXT("Invalid material to remove: %s"), *MaterialName);
		return 2;
	}
	else
	{
		MaterialToBeReplaced->AddToRoot();
	}

	FString ReplacementMaterialName=Tokens(1);
	UMaterialInterface* ReplacementMaterial = (UMaterialInterface*)StaticLoadObject(UMaterialInterface::StaticClass(), NULL, *ReplacementMaterialName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
	if (ReplacementMaterial == NULL)
	{
		warnf(NAME_Error, TEXT("Invalid material to replace with: %s"), *ReplacementMaterialName);
		return 3;
	}
	else
	{
		ReplacementMaterial->AddToRoot();
	}


#if HAVE_SCC
	// Ensure source control is initialized and shut down properly
	FScopedSourceControl SourceControl;
#endif // HAVE_SCC

	for (INT i = 0; i < PackagePathNames.Num(); i++)
	{
		const FString& PackageName = PackagePathNames(i);

		// get the full path name to the file
		FFilename FileName = PackageName;

#if HAVE_SCC
		if ( bAutoCheckOut && FSourceControl::ForceGetStatus( PackageName ) == SCC_NotCurrent )
		{
			warnf( NAME_Log, TEXT("Skipping %s (Not at head source control revision)"), *PackageName );
			continue;
		}
		else
#endif		
		{
			// clean up any previous world
			if (GWorld != NULL)
			{
				GWorld->CleanupWorld();
				GWorld->RemoveFromRoot();
				GWorld = NULL;
			}

			// load the package
			UPackage* Package = LoadPackage(NULL, *FileName, LOAD_NoWarn|LOAD_Quiet);

			// load the world we're interested in
			GWorld = FindObject<UWorld>(Package, TEXT("TheWorld"));

			// we want to change templates only, so ignore maps
			if( GWorld == NULL )
			{
				for( FObjectIterator It; It; ++It )
				{
					UObject* OldObject = *It;
					if( OldObject->GetOutermost() == Package )
					{
						// Don't mess with material internals, which sometimes contain material references
						if (OldObject->IsA(UMaterialInterface::StaticClass()) || OldObject->IsA(UMaterialExpression::StaticClass()))
						{
							continue;
						}

						TMap<UMaterialInterface*, UMaterialInterface*> ReplaceMap;
						ReplaceMap.Set(MaterialToBeReplaced, ReplacementMaterial);
						FArchiveReplaceObjectRef<UMaterialInterface> ReplaceAr(OldObject, ReplaceMap, FALSE, FALSE, FALSE);
						if( ReplaceAr.GetCount() > 0 )
						{
							UObject* OldObjOuter = OldObject->GetOuter();
							FString OldObjOuterName;
							if (OldObjOuter)
							{
								OldObjOuterName = OldObjOuter->GetName();
							}
							warnf(TEXT("Replaced %i material references in an Object: %s, in object %s"), ReplaceAr.GetCount(), *OldObject->GetName(), *OldObjOuterName );
							Package->MarkPackageDirty();
						}
					}
				}

				if( Package->IsDirty() == TRUE )
				{
					if( (GFileManager->IsReadOnly(*FileName)) && ( bAutoCheckOut == TRUE ) )
					{
#if HAVE_SCC
						FSourceControl::CheckOut(Package);
#endif // HAVE_SCC
					}

					warnf(TEXT("Saving %s..."), *FileName);
					SavePackage( Package, NULL, RF_Standalone, *FileName, GWarn );
				}
			}

		}

		// get rid of the loaded world
		CollectGarbage(RF_Native);

	}

	MaterialToBeReplaced->RemoveFromRoot();
	ReplacementMaterial->RemoveFromRoot();

	// UEditorEngine::FinishDestroy() expects GWorld to exist
	UWorld::CreateNew();
	return 0;
}
