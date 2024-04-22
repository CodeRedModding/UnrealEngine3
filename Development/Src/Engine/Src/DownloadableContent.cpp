/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"
#include "EngineUserInterfaceClasses.h"
#include "EngineUIPrivateClasses.h"
#include "DownloadableContent.h"
#include "FConfigCacheIni.h"

IMPLEMENT_CLASS(UDownloadableContentManager);
IMPLEMENT_CLASS(UDownloadableContentEnumerator);

/**
 * Looks for DLC and populates the DLC bundles with the information
 */
void UDownloadableContentEnumerator::FindDLC(void)
{
	debugf(NAME_DevDlc,TEXT("Looking for DLC..."));
	// look for any DLC installed
	TArray<FString> DLCDirectories;
	GFileManager->FindFiles(DLCDirectories,*(DLCRootDir + TEXT("*")),FALSE,TRUE);

	// Add a bundle entry for each directory found
	DLCBundles.Empty(DLCDirectories.Num());
	DLCBundles.AddZeroed(DLCDirectories.Num());
	// the DLCDirectories should now contain a list of all DLCs
	for (INT DLCIndex = 0; DLCIndex < DLCDirectories.Num(); DLCIndex++)
	{
		debugf(NAME_DevDlc,TEXT("Found DLC dir %s"),*DLCDirectories(DLCIndex));
		FOnlineContent& Bundle = DLCBundles(DLCIndex);
		Bundle.ContentPath = DLCRootDir + DLCDirectories(DLCIndex);
		// Name the DLC based off of its dir
		Bundle.FriendlyName = DLCDirectories(DLCIndex);

		// find all the packages in the content
		appFindFilesInDirectory(Bundle.ContentPackages,*Bundle.ContentPath,TRUE,FALSE);
		debugf(NAME_DevDlc,TEXT("\tFound %d package files"),Bundle.ContentPackages.Num());

		// find all the non-packages in the content
		appFindFilesInDirectory(Bundle.ContentFiles,*Bundle.ContentPath,FALSE,TRUE);
		debugf(NAME_DevDlc,TEXT("\tFound %d content files"),Bundle.ContentFiles.Num());
	}
	// Let any listeners know the enumeration is done
	TriggerFindDLCDelegates();
}

/**
 * Triggers the FindDLC delegates
 */
void UDownloadableContentEnumerator::TriggerFindDLCDelegates(void)
{
	const TArray<FScriptDelegate> Delegates = FindDLCDelegates;
	DownloadableContentEnumerator_eventOnFindDLCComplete_Parms Parms(EC_EventParm);
	// Iterate through the delegate list
	for (INT Index = 0; Index < Delegates.Num(); Index++)
	{
		// Make sure the pointer if valid before processing
		const FScriptDelegate* ScriptDelegate = &Delegates(Index);
		if (ScriptDelegate != NULL)
		{
			// Send the notification of completion
			ProcessDelegate(NAME_None,ScriptDelegate,&Parms);
		}
	}
}

/**
 * Deletes a single DLC from the system (physically removes it, not just uninstalls it)
 *
 * @param DLCName Name of the DLC to delete
 */
void UDownloadableContentEnumerator::DeleteDLC(const FString& DLCName)
{
	// delete the entire directory, just like that :)
	GFileManager->DeleteDirectory(*(DLCRootDir + DLCName), FALSE, TRUE);
}

/**
 * Installs the named DLC via the DLC manager
 *
 * @param DLCName the name of the DLC bundle to install
 */
void UDownloadableContentEnumerator::InstallDLC(const FString& DLCName)
{
	UDownloadableContentManager* DLCManager = UGameEngine::GetDLCManager();
	if (DLCManager != NULL)
	{
		for (INT Index = 0; Index < DLCBundles.Num(); Index++)
		{
			if (DLCBundles(Index).FriendlyName == DLCName)
			{
				DLCManager->InstallDLC(DLCBundles(Index));
				break;
			}
		}
	}
}

/**
 * Installs a DLC bundle
 *
 * @param DLCBundle the bundle that is to be installed
 *
 * @return true if the bundle could be installed correctly, false if it failed
 */
UBOOL UDownloadableContentManager::InstallDLC(const FOnlineContent& DLCBundle)
{
	if (GameEngine == NULL)
	{
		GameEngine = Cast<UGameEngine>(GEngine);
	}
	if (GameEngine != NULL)
	{
		// Skip any corrupt DLC packages
		if (DLCBundle.bIsCorrupt == FALSE)
		{
			InstallPackages(DLCBundle);
			InstallNonPackageFiles(DLCBundle);
			// Add this to the list of installed DLC
			InstalledDLC.AddItem(DLCBundle.FriendlyName);
			return TRUE;
		}
		else
		{
			debugf(NAME_DevDlc,
				TEXT("Skipping DLC (%s) since it is corrupt"),
				*DLCBundle.FriendlyName);
		}
	}
	return FALSE;
}

/**
 * Installs a set of DLC bundles
 *
 * @param DLCBundles the set of bundles that are to be installed
 */
void UDownloadableContentManager::InstallDLCs(const TArray<FOnlineContent>& DLCBundles)
{
	for (INT Index = 0; Index < DLCBundles.Num(); Index++)
	{
		if (InstallDLC(DLCBundles(Index)) == FALSE)
		{
			debugf(NAME_DevDlc,TEXT("Failed to install DLC %s"),*DLCBundles(Index).FriendlyName);
		}
	}
	// Fully load all queued up packages to avoid multiple reloads of the same package
	for (INT Index = 0; Index < QueuedFullyLoadPackageInis.Num(); Index++)
	{
		AddPackagesToFullyLoad(QueuedFullyLoadPackageInis(Index));
	}
	QueuedFullyLoadPackageInis.Empty();
}

/** Clears the DLC cache and restores the config cache to its pre-DLC state */
void UDownloadableContentManager::ClearDLC(void)
{
	// Clear the package file cache
	GPackageFileCache->ClearDownloadedPackages();
	// Remove any sections that were added and replace any that were changed
	while (DLCConfigCacheChanges.Num())
	{
		// Work backwards through the list of config files to keep from thrashing memory
		// And to overwrite any changes with the oldest (closest to original files)
		INT Index = DLCConfigCacheChanges.Num() - 1;
		FDLCConfigCacheChanges* CacheChanges = DLCConfigCacheChanges(Index);
		if (CacheChanges != NULL)
		{
			// Look up which config file we are modifying
			FConfigFile* ConfigFile = GConfig->FindConfigFile(*CacheChanges->ConfigFileName);
			if (ConfigFile != NULL)
			{
				// Replace any sections that were modified
				for (TMap<FString,FConfigSection>::TIterator It(CacheChanges->SectionsToReplace); It; ++It)
				{
					// Replace the section and flag any objects that were affected
					ConfigFile->Set(It.Key(),It.Value());
					// Flag any objects for reloading later
					AddSectionToObjectList(It.Key());
				}
				// Remove any sections that were added
				for (INT RemoveIndex = 0; RemoveIndex < CacheChanges->SectionsToRemove.Num(); RemoveIndex++)
				{
					ConfigFile->Remove(*CacheChanges->SectionsToRemove(RemoveIndex));
					// Mark the object as destroyed if needed
					MarkPerObjectConfigPendingKill(CacheChanges->SectionsToRemove(RemoveIndex));
				}
			}
			// Release the memory
			delete CacheChanges;
			DLCConfigCacheChanges.Remove(Index);
		}
	}
	// Clear the always loaded package list
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (GameEngine)
	{
		// Cleanup all of the fully loaded packages for maps (may not free memory until next GC)
		GameEngine->CleanupAllPackagesToFullyLoad();
	}
	// Reload config/loc for any objects affected
	UpdateObjectLists();
	// Empty the DLC list
	InstalledDLC.Empty();
	NonPackageFilePathMap.Empty();
}

/**
 * Determines the texture cache file path associated with the name
 *
 * @param DLCBundle the bundle that is to be installed
 *
 * @return true if the bundle could be installed correctly, false if it failed
 */
UBOOL UDownloadableContentManager::GetDLCNonPackageFilePath(FName NonPackageFileName,FString& Path)
{
    FString* Result = NonPackageFilePathMap.Find(NonPackageFileName);
	if (Result != NULL)
	{
		Path = *Result;
		return TRUE;
	}
	return FALSE;
}

/**
 * Adds the specified section to the classes to update list or to the per object config
 * objects to update depending on whether they are found
 *
 * @param Section the section name being reloaded
 */
void UDownloadableContentManager::AddSectionToObjectList(const FString& Section)
{
	// See if this section is related to a class
	UClass* Class = FindObject<UClass>(NULL,*Section,TRUE);
	if (Class)
	{
		// Add this to the list to check against
		ClassesToReload.AddUniqueItem(Class);
	}
	else
	{
		// See if this section is for a per object config
		INT PerObjectNameIndex = Section.InStr(TEXT(" "));
		if (PerObjectNameIndex != INDEX_NONE)
		{
			const FString PerObjectName = Section.Left(PerObjectNameIndex);
			// Explicitly search the transient package (won't update non-transient objects)
			UObject* PerObject = FindObject<UObject>(ANY_PACKAGE,*PerObjectName,FALSE);
			if (PerObject)
			{
				ObjectsToReload.AddUniqueItem(PerObject);
			}
		}
	}
}

/**
 * Looks to see if the section is a per object config section that was removed so the
 * object also needs to be destroyed
 *
 * @param Section the section name being unloaded
 */
void UDownloadableContentManager::MarkPerObjectConfigPendingKill(const FString& Section)
{
	// See if this section is for a per object config
	INT PerObjectNameIndex = Section.InStr(TEXT(" "));
	if (PerObjectNameIndex != INDEX_NONE)
	{
		const FString PerObjectName = Section.Left(PerObjectNameIndex);
		// Explicitly search the transient package (won't update non-transient objects)
		UObject* PerObject = FindObject<UObject>(ANY_PACKAGE,*PerObjectName,FALSE);
		if (PerObject)
		{
			// Have GC clean it up next time around
			PerObject->MarkPendingKill();
		}
	}
}

/**
 * Reloads config and localization on both of the object lists and empties them
 */
void UDownloadableContentManager::UpdateObjectLists(void)
{
	// If there is a list of classes to update, iterate and update
	if (ClassesToReload.Num())
	{
		for (FObjectIterator It; It; ++It)
		{
			UClass* Class = It->GetClass();
			// Don't do anything for non-config/localized classes
			if (Class->HasAnyClassFlags(CLASS_Config | CLASS_Localized) &&
				// Per object config is handled later
				!Class->HasAnyClassFlags(CLASS_PerObjectConfig))
			{
				// Check to see if this class is in our list
				for (INT ClassIndex = 0; ClassIndex < ClassesToReload.Num(); ClassIndex++)
				{
					if (It->IsA(ClassesToReload(ClassIndex)))
					{
						// Force a reload of the config vars
						It->ReloadConfig();
						// Force a reload of the localized vars
						It->ReloadLocalized();
					}
				}
			}
		}
	}
	// Do all the per object config updates
	for (INT Index = 0; Index < ObjectsToReload.Num(); Index++)
	{
		// Force a reload of the config vars
		ObjectsToReload(Index)->ReloadConfig();
		// Force a reload of the localized vars
		ObjectsToReload(Index)->ReloadLocalized();
	}
	// Clear out the lists so we don't confuse GC
	ObjectsToReload.Empty();
	ClassesToReload.Empty();
}

/**
 * Installs the list of packages for the DLC
 *
 * @param DLCBundle the bundle that is being installed
 */
void UDownloadableContentManager::InstallPackages(const FOnlineContent& DLCBundle)
{
	TArray<FName> GuidCaches;
	// Add each package to the package file cache
	for (INT PackageIndex = 0; PackageIndex < DLCBundle.ContentPackages.Num(); PackageIndex++)
	{
		debugfSlow(TEXT("Caching package %s"),*DLCBundle.ContentPackages(PackageIndex));
		FFilename BaseName = FFilename(DLCBundle.ContentPackages(PackageIndex)).GetBaseFilename();
		// See if this is a guid cache that needs to be always loaded
		if (BaseName.StartsWith(TEXT("GuidCache_")))
		{
			GuidCaches.AddItem(FName(*BaseName));
			debugf(TEXT("Loading guid cache %s"),*BaseName);
		}
		GPackageFileCache->CacheDownloadedPackage(*DLCBundle.ContentPackages(PackageIndex),0);
	}
	// Load any guid caches that were found
	if (GuidCaches.Num())
	{
		GameEngine->AddPackagesToFullyLoad(FULLYLOAD_Always,TEXT(""),GuidCaches,TRUE);
	}
}

/**
 * Installs the list of non-packages (ini, loc, sha, etc.) for the DLC
 *
 * @param DLCBundle the bundle that is being installed
 */
void UDownloadableContentManager::InstallNonPackageFiles(const FOnlineContent& DLCBundle)
{
	// Create a map to store all the non package files
	InstallNonPackages(DLCBundle);

#if PS3 // PS3 DLC has to be signed
	// Add any SHA files
	InstallSHAFiles(DLCBundle);
#endif
	// Process all INI files and our current loc language
	InstallIniLocFiles(DLCBundle);
}

/**
 * Updates the TFC map with all TFC entries in this DLC bundle
 *
 * @param DLCBundle the DLC bundle being processed
 */
void UDownloadableContentManager::InstallNonPackages(const FOnlineContent& DLCBundle)
{
	// Loop through processing TFC files in the non-package list
	for (INT FileIndex = 0; FileIndex < DLCBundle.ContentFiles.Num(); FileIndex++)
	{
		if (DLCBundle.ContentFiles(FileIndex).Right(4) != TEXT(".xxx"))
		{
			const FFilename FullFilename = FFilename(DLCBundle.ContentFiles(FileIndex));
			debugf(NAME_DevDlc,TEXT("Caching file: %s/%s"),*FullFilename.GetBaseFilename(),*FullFilename);
			// Add the file to our map
			NonPackageFilePathMap.Set(FName(*FullFilename.GetBaseFilename()),FullFilename);
		}
	}
}

#if PS3
/**
 * Adds any SHA hashes to the file hashes for this bundle
 *
 * @param DLCBundle the DLC bundle being processed
 */
void UDownloadableContentManager::InstallSHAFiles(const FOnlineContent& DLCBundle)
{
	TArray<BYTE> HashContents;
	// Loop through processing TFC files in the non-package list
	for (INT FileIndex = 0; FileIndex < DLCBundle.ContentFiles.Num(); FileIndex++)
	{
		if (DLCBundle.ContentFiles(FileIndex).Right(4) == TEXT(".sha"))
		{
			const FFilename FullFilename = FFilename(DLCBundle.ContentFiles(FileIndex));
			debugf(NAME_DevDlc,TEXT("Adding SHA file: %s/%s"),*FullFilename.GetBaseFilename(),*FullFilename);
			// Load the SHA file into the buffer
			appLoadFileToArray(HashContents,*DLCBundle.ContentFiles(FileIndex));
			// Add the data to the SHA code
			FSHA1::InitializeFileHashesFromBuffer(HashContents.GetData(),HashContents.Num(),TRUE);
		}
	}
}
#endif

/**
 * Process all INI files and our current loc language
 *
 * @param DLCBundle the DLC bundle being processed
 */
void UDownloadableContentManager::InstallIniLocFiles(const FOnlineContent& DLCBundle)
{
	// Loop through processing INI/loc files in the non-package list
	for (INT FileIndex = 0; FileIndex < DLCBundle.ContentFiles.Num(); FileIndex++)
	{
		const FFilename FullFileName = FFilename(DLCBundle.ContentFiles(FileIndex));
		const FFilename ContentFile = FullFileName.GetCleanFilename();
		// Get filename extension so we skip any file that isn't a INI/loc file
		const FString Ext = ContentFile.GetExtension();
		const UBOOL bIsINI = Ext == TEXT("ini");
		// We need to handle ini files, loc for this language, and fallback loc files
		if (bIsINI ||
			Ext == appGetLanguageExt() ||
			Ext == TEXT("int"))
		{
			TArray<FString> Sections;
			// Get the list of section names for this ini/loc file
			GetListOfSectionNames(*DLCBundle.ContentFiles(FileIndex),Sections);
			FString ConfigFileName;
			// Create a file name that matches what the game expects in terms of full path
			if (bIsINI)
			{
				ConfigFileName = appGameConfigDir() + ContentFile;
			}
			else
			{
				// Put this into any localization directory in the proper language sub-directory (..\ExampleGame\Localization\fra\DLCMap.fra)
				for (INT LocIdx=GSys->LocalizationPaths.Num()-1; LocIdx >= 0; LocIdx--)
				{
					ConfigFileName = GSys->LocalizationPaths(LocIdx) * Ext * ContentFile;
					if (GConfig->FindConfigFile(*ConfigFileName) != NULL)
					{
						break;
					}
				}
			}
			// Build our undo sets before we modify the config cache
			BuildDLCConfigCacheUndo(*ConfigFileName,Sections);
			// Search for the config file
			FConfigFile* ConfigFile = GConfig->FindConfigFile(*ConfigFileName);
			// If not found, add this file to the config cache
			if (ConfigFile == NULL)
			{
				ConfigFile = &GConfig->Set(*ConfigFileName,FConfigFile());
			}
			check(ConfigFile);
			// Merge the DLC ini file into the existing one
			if (ConfigFile->Combine(*DLCBundle.ContentFiles(FileIndex)))
			{
				debugf(NAME_DevDlc,TEXT("Merged DLC config file (%s) into existing config file (%s)"),*DLCBundle.ContentFiles(FileIndex),*ConfigFileName);
			}
			else
			{
				debugf(NAME_DevDlc,TEXT("Failed to merge DLC config file (%s) into existing config file (%s)"),*DLCBundle.ContentFiles(FileIndex),*ConfigFileName);
			}
			// Add all of the sections to objects that need to be updated after merging INIs
			for (INT Index = 0; Index < Sections.Num(); Index++)
			{
				AddSectionToObjectList(Sections(Index));
			}
			if (bIsINI)
			{
				// Add any packages that need fully loading
				QueuedFullyLoadPackageInis.AddUniqueItem(ConfigFileName);				
			}
		}
	}
	// Reload config/loc for any objects affected
	UpdateObjectLists();
}

/**
 * Parses the specified section for the key/value set to use for fully loading packages
 *
 * @param FileName the file name to parse the information from
 */
void UDownloadableContentManager::AddPackagesToFullyLoad(const FString& FileName)
{
	// Get the set of packages to load for a given map
	AddPackagesToFullyLoad(FULLYLOAD_Map,TEXT("Engine.PackagesToFullyLoadForDLC"),TEXT("MapName"),TEXT("Package"),*FileName);
	// Get the set of packages to pre load for a game type
	AddPackagesToFullyLoad(FULLYLOAD_Game_PreLoadClass,TEXT("Engine.PackagesToFullyLoadForDLC"),TEXT("GameType_PreLoadClass"),TEXT("Package"),*FileName);
	// Get the set of packages to post load for a game type
	AddPackagesToFullyLoad(FULLYLOAD_Game_PostLoadClass,TEXT("Engine.PackagesToFullyLoadForDLC"),TEXT("GameType_PostLoadClass"),TEXT("Package"),*FileName);
	// Get the set of packages to post load for all game types
	AddPackagesToFullyLoad(FULLYLOAD_Game_PostLoadClass,TEXT("Engine.PackagesToFullyLoadForDLC"),TEXT("LoadForAllGameTypes"),TEXT("Package"),*FileName);
}

/**
 * Parses the specified section for the key/value set to use for fully loading
 *
 * @param LoadType the type of package loading to use
 * @param Section the INI section to parse
 * @param KeyOne the key to parse
 * @param KeyN the sub keys to parse
 * @param FileName the file name to parse the information from
 */
void UDownloadableContentManager::AddPackagesToFullyLoad(EFullyLoadPackageType LoadType,const TCHAR* Section,const TCHAR* KeyOne,const TCHAR* KeyN,const TCHAR* FileName)
{
	TMap<FName,TArray<FName> > MapMappings;
	// Read the specified section and keys
	GConfig->Parse1ToNSectionOfNames(Section,KeyOne,KeyN,MapMappings,FileName);
	// And update the game engine with each of the parsed key sets
	for(TMap<FName,TArray<FName> >::TIterator It(MapMappings); It; ++It)
	{
		TArray<FName> UniqueNames;
		for (INT i = 0; i < It.Value().Num(); i++)
		{
			UniqueNames.AddUniqueItem(It.Value()(i));
		}
		GameEngine->AddPackagesToFullyLoad(LoadType,It.Key().ToString(),UniqueNames,TRUE);
	}
}

/**
 * For a given DLC ini/loc file, it determines the list of sections in the INI/loc file that are touched
 * so that it can determine which ones to replace during unloading and which to remove altogether
 *
 * @param FileName the name of the DLC ini/loc file to parse for section changes
 * @param OutSectionsIncluded gets the list of modified/added sections
 */
void UDownloadableContentManager::GetListOfSectionNames(const TCHAR* FileName,TArray<FString>& OutSectionsIncluded)
{
	OutSectionsIncluded.Empty();
	FString IniLocData;
	// Load the file so we can parse the sections
	if(appLoadFileToString(IniLocData,FileName))
	{
		INT StartIndex = 0;
		INT EndIndex = 0;
		// Find the set of object classes that were affected
		while (StartIndex >= 0 && StartIndex < IniLocData.Len())
		{
			// Find the next section header
			StartIndex = IniLocData.InStr(TEXT("["),FALSE,FALSE,StartIndex);
			if (StartIndex > -1)
			{
				// Find the ending section identifier
				EndIndex = IniLocData.InStr(TEXT("]"),FALSE,FALSE,StartIndex);
				if (EndIndex > StartIndex)
				{
					// Snip the text out and add that section to our list
					OutSectionsIncluded.AddItem(IniLocData.Mid(StartIndex + 1,EndIndex - StartIndex - 1));
					StartIndex = EndIndex;
				}
			}
		}
	}
	else
	{
		debugf(NAME_DevDlc,TEXT("Failed to load ini file (%s) for GetListOfSectionNames()"),FileName);
	}
}

/**
 * Builds the set of changes needed to undo this DLC's config changes/additions
 *
 * @param ConfigFileName the name of the DLC ini/loc file to build the undo set for
 * @param SectionsIncluded the list of modified/added sections
 */
void UDownloadableContentManager::BuildDLCConfigCacheUndo(const TCHAR* ConfigFileName,const TArray<FString>& SectionsIncluded)
{
	// Allocate our undo object and add it to our list to undo at uninstall time
	FDLCConfigCacheChanges* CacheChanges = new FDLCConfigCacheChanges();
	DLCConfigCacheChanges.AddItem(CacheChanges);
	// Store off the config file name
	CacheChanges->ConfigFileName = ConfigFileName;
	// See if the config file exists or not
	FConfigFile* ConfigFile = GConfig->FindConfigFile(ConfigFileName);
	if (ConfigFile)
	{
		// Iterate through the sections determining whether they are changes or additions
		for (INT Index = 0; Index < SectionsIncluded.Num(); Index++)
		{
			FConfigSection* Section = ConfigFile->Find(SectionsIncluded(Index));
			if (Section)
			{
				// This section was changed so store it off so we can restore later
				CacheChanges->SectionsToReplace.Set(SectionsIncluded(Index),*Section);
			}
			else
			{
				// This will be a new section
				CacheChanges->SectionsToRemove.AddItem(SectionsIncluded(Index));
			}
		}
	}
	else
	{
		// Brand new file so remove all of the sections
		CacheChanges->SectionsToRemove += SectionsIncluded;
	}
}
