/*=============================================================================
	UnGameCookerHelper.cpp: Game specific cooking helper class implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnGameCookerHelper.h"

#if WITH_SUBSTANCE_AIR == 1
#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirInstanceFactoryClasses.h"
#include "SubstanceAirHelpers.h"
#include "SubstanceAirEdPreset.h"
#include "SubstanceAirTextureClasses.h"
#include "framework\details\detailslinkdata.h"
#include "framework\imageinput.h"

#pragma pack ( push, 8 )
#include <substance/texture.h>
#pragma pack ( pop )

#include <atc_api.h>
class atc_api::Writer* SubstanceAirTextureCacheWriter=NULL;
#endif // WITH_SUBSTANCE_AIR == 1

//-----------------------------------------------------------------------------
//	GenerateGameContentSeekfree
//-----------------------------------------------------------------------------
#define GAME_CONTENT_PKG_PREFIX TEXT("MPContent")

static DOUBLE		GenerateGameContentTime;
static DOUBLE		GenerateGameContentInitTime;
static DOUBLE		GenerateGameContentCommonInitTime;
static DOUBLE		GenerateGameContentListGenerationTime;
static DOUBLE		GenerateGameContentCommonGenerationTime;
static DOUBLE		GenerateGameContentPackageGenerationTime;
static DOUBLE		GenerateGameContentCommonPackageGenerationTime;

static DOUBLE		ForcedContentTime;

/**
 *	Helper funtion for loading the object or class of the given name.
 *	If supplied, it will also add it to the given object referencer.
 *
 *	@param	Commandlet		The commandlet being run
 *	@param	ObjectName		The name of the object/class to attempt to load
 *	@param	ObjReferencer	If not NULL, the object referencer to add the loaded object/class to
 *
 *	@return	UObject*		The loaded object/class, NULL if not found
 */
UObject* FGameContentCookerHelper::LoadObjectOrClass(class UCookPackagesCommandlet* Commandlet, const FString& ObjectName, UObjectReferencer* ObjRefeferencer)
{
	if (ObjectName.InStr(TEXT(":")) != INDEX_NONE)
	{
		// skip these objects under the assumption that their outer will have been loaded
		return NULL;
	}

	// load the requested content object (typically a content class) 
	UObject* ContentObj = LoadObject<UObject>(NULL, *ObjectName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
	if (!ContentObj)
	{
		ContentObj = LoadObject<UClass>(NULL, *ObjectName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
	}

	if (ContentObj)
	{
		if (ObjRefeferencer != NULL)
		{
			ObjRefeferencer->ReferencedObjects.AddUniqueItem(ContentObj);
		}
	}

	return ContentObj;
}


/**
 *	Mark the given object as cooked
 *
 *	@param	InObject	The object to mark as cooked.
 */
void FGameContentCookerHelper::MarkObjectAsCooked(UObject* InObject)
{
	InObject->ClearFlags(RF_ForceTagExp);
	InObject->SetFlags(RF_MarkedByCooker);
	UObject* MarkOuter = InObject->GetOuter();
	while (MarkOuter)
	{
		MarkOuter->ClearFlags(RF_ForceTagExp);
		MarkOuter->SetFlags(RF_MarkedByCooker);
		MarkOuter = MarkOuter->GetOuter();
	}
}

/**
 *	Attempt to load each of the objects in the given list.
 *	If loaded, iterate over all objects that get loaded along with it and add them to
 *	the given OutLoadedObjectMap - incrementing the count of objects found.
 *
 *	@param	InObjectsToLoad			The list of objects to load
 *	@param	OutLoadedObjectMap		All objects loaded, and the number of times they were loaded
 *	
 *	@return	INT						The number of object in the InObjectsToLoad list that successfully loaded
 */
INT FGameContentCookerHelper::GenerateLoadedObjectList(class UCookPackagesCommandlet* Commandlet, const TArray<FString>& InObjectsToLoad, TMap<FString,INT>& OutLoadedObjectMap)
{
	INT FoundObjectCount = 0;
	for (INT LoadIdx = 0; LoadIdx < InObjectsToLoad.Num(); LoadIdx++)
	{
		FString ObjectStr = InObjectsToLoad(LoadIdx);
		UObject* ContentObj = LoadObjectOrClass(Commandlet, ObjectStr, NULL);
		if (ContentObj != NULL)
		{
			FoundObjectCount++;

			// Mark any already cooked startup objects...
			Commandlet->MarkCookedStartupObjects(TRUE);

			// USE THIS TO GATHER THE DEPENDENCIES FOR THE CONTENT OBJECT!!!!
			TSet<FDependencyRef> ContentObjDependencies;
			TMap<FString,UBOOL> ContentObjReferencedList;
			ULinkerLoad* Linker = ContentObj->GetLinker();
			if (Linker != NULL)
			{
				Linker->GatherExportDependencies(ContentObj->GetLinkerIndex(), ContentObjDependencies, FALSE);

				for (TSet<FDependencyRef>::TConstIterator It(ContentObjDependencies); It; ++It)
				{
					const FDependencyRef& Ref = *It;
					FString FullName = Ref.Linker->GetExportFullName(Ref.ExportIndex);
					ContentObjReferencedList.Set(FullName, TRUE);
				}
			}
			else
			{
				warnf(NAME_Warning, TEXT("Failed to get linker for %s"), *(ContentObj->GetPathName()));
			}

			// Now we have a list of objects we would serialize (remove any marked by startup)
			// So clean as usual... then iterate over and any objects !Marked are still valid
			// This is so that we don't pull in base textures, etc.

			// Create a temporary package to import the content objects
			// This is required so that we can 'process' the package for cooking, 
			// clearing out materials, etc.
			FString TempPackageName = ObjectStr + TEXT("_TmpPkg");
			UPackage* TempPackage = UObject::CreatePackage(NULL, *TempPackageName);
			if (TempPackage != NULL)
			{
				// create a referencer to keep track of the content objects added in the package
				UObjectReferencer* ObjRefeferencer = ConstructObject<UObjectReferencer>(UObjectReferencer::StaticClass(), TempPackage);
				ObjRefeferencer->SetFlags(RF_Standalone);
				ObjRefeferencer->ReferencedObjects.AddUniqueItem(ContentObj);
				// prep the temp package for cooking - cleanup materials, etc.
				Commandlet->PrepPackageForObjectCooking(TempPackage, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE);
			}

			// Prevent disabled emitters from pulling in materials...
			//@todo. This needs to be done for *all* objects...
			ContentObj->AddToRoot();
			for (TObjectIterator<UObject> ObjIt; ObjIt; ++ObjIt)
			{
				UParticleSystem* ParticleSystem = Cast<UParticleSystem>(*ObjIt);
				if (ParticleSystem != NULL)
				{
					Commandlet->CookParticleSystem(ParticleSystem);
				}
			}
			UObject::CollectGarbage(RF_Native);
			ContentObj->RemoveFromRoot();
			
			// Iterate over all objects that are loaded and not marked by cooker, adding them to the list
			for (TObjectIterator<UObject> ObjIt; ObjIt; ++ObjIt)
			{
				UObject* CheckObject = *ObjIt;

				if (CheckObject->GetOuter() != NULL)
				{
					if (//(CheckObject->IsA(UPackage::StaticClass()) == FALSE) &&
						(CheckObject->IsA(UShaderCache::StaticClass()) == FALSE) &&
						(CheckObject->IsA(UObjectReferencer::StaticClass()) == FALSE))
					{
						if (!CheckObject->HasAnyFlags(RF_Transient) && !CheckObject->IsIn(UObject::GetTransientPackage()))
						{
							if ((CheckObject->HasAnyFlags(RF_MarkedByCooker|RF_CookedStartupObject) == FALSE) && 
								(CheckObject->IsIn(TempPackage) || CheckObject->HasAnyFlags(RF_ForceTagExp)))
							{
								FString ExportPathName = CheckObject->GetPathName();
								FString ExportFullName = CheckObject->GetFullName();
								if (ContentObjReferencedList.Find(ExportFullName) != NULL)
								{
									ExportPathName = ExportPathName.ToUpper();
									INT* EntryPtr = OutLoadedObjectMap.Find(ExportPathName);
									if (EntryPtr == NULL)
									{
										INT Temp = 0;
										OutLoadedObjectMap.Set(ExportPathName, Temp);
										EntryPtr = OutLoadedObjectMap.Find(ExportPathName);
									}
									check(EntryPtr);
									*EntryPtr = *EntryPtr + 1;
								}
							}
						}
					}
				}
			}
			UObject::CollectGarbage(RF_Native);
		}
	}

	return FoundObjectCount;
}

/**
 * Helper for adding game content standalone packages for cooking
 */

/**
 * Initialize options from command line options
 *
 *	@param	Tokens			Command line tokens parsed from app
 *	@param	Switches		Command line switches parsed from app
 */
void FGenerateGameContentSeekfree::InitOptions(const TArray<FString>& Tokens, const TArray<FString>& Switches)
{
	SCOPE_SECONDS_COUNTER(GenerateGameContentTime);
	// Check for flag to recook seekfree game type content
	bForceRecookSeekfreeGameTypes = Switches.ContainsItem(TEXT("RECOOKSEEKFREEGAMETYPES"));
	if (Switches.FindItemIndex(TEXT("MTCHILD")) != INDEX_NONE )
	{
		// child will only cook this if asked, so add them all to the cook list
		bForceRecookSeekfreeGameTypes = TRUE; 
	}
}

/**
 *	Check if the given object or any of its dependencies are newer than the given time
 *
 *	@param	Commandlet		The commandlet being run
 *	@param	ContentStr		The name of the object in question
 *	@param	CheckTime		The time to check against
 *	@param	bIsSeekfreeFileDependencyNewerMap	A mapping of base package states to prevent repeated examination of the same content
 *
 *	@return	UBOOL			TRUE if the content (or any of its dependencies) is newer than the specified time
 */
UBOOL FGenerateGameContentSeekfree::CheckIfContentIsNewer(
	class UCookPackagesCommandlet* Commandlet,
	const FString& ContentStr, 
	DOUBLE CheckTime,
	TMap<FString,UBOOL>& bIsSeekfreeFileDependencyNewerMap
	)
{
	UBOOL bIsSeekfreeFileDependencyNewer = FALSE;

	FString SrcContentPackageFileBase = ContentStr;
	// strip off anything after the base package name
	INT FoundIdx = ContentStr.InStr(TEXT("."));
	if( FoundIdx != INDEX_NONE )
	{
		SrcContentPackageFileBase = ContentStr.LeftChop(ContentStr.Len() - FoundIdx);
	}

	FString SrcContentPackageFilePath;
	// find existing content package
	if (GPackageFileCache->FindPackageFile(*SrcContentPackageFileBase, NULL, SrcContentPackageFilePath))
	{
		// check if we've already done the (slow) dependency check for this package
		UBOOL* bExistingEntryPtr = bIsSeekfreeFileDependencyNewerMap.Find(SrcContentPackageFileBase);					
		if( bExistingEntryPtr )
		{
			// use existing entry to trigger update
			bIsSeekfreeFileDependencyNewer |= *bExistingEntryPtr;
		}
		else
		{
			// do dependency check for packages coming from exports of the current content package
			UBOOL bHasNewer = Commandlet->AreSeekfreeDependenciesNewer(NULL, *SrcContentPackageFilePath, CheckTime);
			bIsSeekfreeFileDependencyNewer |= bHasNewer;
			// keep track of this content package entry to check for duplicates
			bIsSeekfreeFileDependencyNewerMap.Set(SrcContentPackageFileBase,bHasNewer);
		}
	}

	return bIsSeekfreeFileDependencyNewer;
}

/**
 *	Add the standalone seekfree entries to the package list for the given ContentMap
 *
 *	@param	InContentMap		The content map being processed for this call
 *	@param	Commandlet			The commandlet being run
 *	@param	Platform			The platform being cooked for
 *	@param	ShaderPlatform		The shader platform being cooked for
 *	@param	*Pairs				The filename pair lists
 *
 *	@return TRUE if succeeded
 */
UBOOL FGenerateGameContentSeekfree::GeneratePackageListForContentMap( 
	TMap<FString,FGameContentEntry>& InContentMap,
	class UCookPackagesCommandlet* Commandlet, 
	UE3::EPlatformType Platform,
	EShaderPlatform ShaderPlatform,
	TArray<FPackageCookerInfo>& NotRequiredFilenamePairs,
	TArray<FPackageCookerInfo>& RegularFilenamePairs,
	TArray<FPackageCookerInfo>& MapFilenamePairs,
	TArray<FPackageCookerInfo>& ScriptFilenamePairs,
	TArray<FPackageCookerInfo>& StartupFilenamePairs,
	TArray<FPackageCookerInfo>& StandaloneSeekfreeFilenamePairs)
{
	UBOOL bSuccess=TRUE;

	FString OutDir = appGameDir() + PATH_SEPARATOR + FString(GAME_CONTENT_PKG_PREFIX) + PATH_SEPARATOR;
	TMap<FString,UBOOL> bIsSeekfreeFileDependencyNewerMap;

	for (TMap<FString,FGameContentEntry>::TIterator GTIt(InContentMap); GTIt; ++GTIt)
	{
		const FString& SourceName = GTIt.Key();
		FGameContentEntry& ContentEntry = GTIt.Value();

		if (ContentEntry.ContentList.Num() > 0)
		{
			// Source filename of temp package to be cooked
			ContentEntry.PackageCookerInfo.SrcFilename = OutDir + SourceName + TEXT(".upk");
			// Destination filename for cooked seekfree package
			ContentEntry.PackageCookerInfo.DstFilename = Commandlet->GetCookedPackageFilename(ContentEntry.PackageCookerInfo.SrcFilename);

			// setup flags to cook as seekfree standalone package
			ContentEntry.PackageCookerInfo.bShouldBeSeekFree = TRUE;
			ContentEntry.PackageCookerInfo.bIsNativeScriptFile = FALSE;
			ContentEntry.PackageCookerInfo.bIsCombinedStartupPackage = FALSE;
			ContentEntry.PackageCookerInfo.bIsStandaloneSeekFreePackage = TRUE;
			ContentEntry.PackageCookerInfo.bShouldOnlyLoad = FALSE;

			// check to see if this seekfree package's dependencies require it to be recooked
			UBOOL bIsSeekfreeFileDependencyNewer = FALSE;
			if (bForceRecookSeekfreeGameTypes)
			{
				// force recook
				bIsSeekfreeFileDependencyNewer = TRUE;
			}
			else
			{
				FString ActualDstName = ContentEntry.PackageCookerInfo.DstFilename.GetBaseFilename(FALSE);
				ActualDstName += STANDALONE_SEEKFREE_SUFFIX;
				ActualDstName += FString(TEXT(".")) + ContentEntry.PackageCookerInfo.DstFilename.GetExtension();
				// get dest cooked file timestamp
				DOUBLE Time = GFileManager->GetFileTimestamp(*ActualDstName);
				// iterate over source content that needs to be cooked for this game type
				for( INT ContentIdx=0; (ContentIdx < ContentEntry.ContentList.Num()) && !bIsSeekfreeFileDependencyNewer; ContentIdx++ )
				{
					const FString& ContentStr = ContentEntry.ContentList(ContentIdx);
					bIsSeekfreeFileDependencyNewer |= CheckIfContentIsNewer(Commandlet, ContentStr, Time, bIsSeekfreeFileDependencyNewerMap);
				}

				for (INT ForcedIdx = 0; (ForcedIdx < ContentEntry.ForcedContentList.Num()) && !bIsSeekfreeFileDependencyNewer; ForcedIdx++)
				{
					const FString& ContentStr = ContentEntry.ForcedContentList(ForcedIdx);
					bIsSeekfreeFileDependencyNewer |= CheckIfContentIsNewer(Commandlet, ContentStr, Time, bIsSeekfreeFileDependencyNewerMap);
				}
			}

			//@todo. Check if the language version being cooked for is present as well...
			if( !bIsSeekfreeFileDependencyNewer )
			{
			}

			if( !bIsSeekfreeFileDependencyNewer )
			{
				debugf(NAME_Log, TEXT("GamePreloadContent: standalone seekfree package for %s is UpToDate, skipping"), *SourceName);
			}
			else
			{
				// add the entry to the standalone seekfree list
				StandaloneSeekfreeFilenamePairs.AddItem(ContentEntry.PackageCookerInfo);

				debugf(NAME_Log, TEXT("GamePreloadContent: Adding standalone seekfree package for %s"), *SourceName);
			}
		}
		else
		{
			debugf(NAME_Log, TEXT("GeneratePackageListForContentMap: Empty content list for %s"), *SourceName);
		}
	}

	return bSuccess;
}

/**
 * Adds one standalone seekfree entry to the package list for each game type specified from ini.  
 *
 * @return TRUE if succeeded
 */
UBOOL FGenerateGameContentSeekfree::GeneratePackageList( 
	class UCookPackagesCommandlet* Commandlet, 
	UE3::EPlatformType Platform,
	EShaderPlatform ShaderPlatform,
	TArray<FPackageCookerInfo>& NotRequiredFilenamePairs,
	TArray<FPackageCookerInfo>& RegularFilenamePairs,
	TArray<FPackageCookerInfo>& MapFilenamePairs,
	TArray<FPackageCookerInfo>& ScriptFilenamePairs,
	TArray<FPackageCookerInfo>& StartupFilenamePairs,
	TArray<FPackageCookerInfo>& StandaloneSeekfreeFilenamePairs)
{
	SCOPE_SECONDS_COUNTER(GenerateGameContentTime);

	if (Commandlet->DLCName.Len())
	{
		// Only allow specific gametype cooking for DLC
		FString CommandLineGameType;
		Parse( appCmdLine(), TEXT("MPGAMETYPE="), CommandLineGameType );

		if (CommandLineGameType.Len() == 0)
		{
			return TRUE;
		}
	}

	UBOOL bSuccess=TRUE;

	if (Commandlet->DLCName.Len() == 0)
	{
		// Generate the common package list
		SCOPE_SECONDS_COUNTER(GenerateGameContentCommonGenerationTime);
		if (GeneratePackageListForContentMap(
			GameTypeCommonContentMap, 
			Commandlet, 
			Platform,
			ShaderPlatform,
			NotRequiredFilenamePairs,
			RegularFilenamePairs,
			MapFilenamePairs,
			ScriptFilenamePairs,
			StartupFilenamePairs,
			StandaloneSeekfreeFilenamePairs) == FALSE)
		{
			bSuccess = FALSE;
		}
	}

	{
		// update the package cooker info for each content entry
		SCOPE_SECONDS_COUNTER(GenerateGameContentListGenerationTime);
		if (GeneratePackageListForContentMap(
			GameContentMap, 
			Commandlet, 
			Platform,
			ShaderPlatform,
			NotRequiredFilenamePairs,
			RegularFilenamePairs,
			MapFilenamePairs,
			ScriptFilenamePairs,
			StartupFilenamePairs,
			StandaloneSeekfreeFilenamePairs) == FALSE)
		{
			bSuccess = FALSE;
		}
	}

	return bSuccess;
}

/**
 * Cooks passed in object if it hasn't been already.
 *
 *	@param	Commandlet					The cookpackages commandlet being run
 *	@param	Package						Package going to be saved
 *	@param	Object						Object to cook
 *	@param	bIsSavedInSeekFreePackage	Whether object is going to be saved into a seekfree package
 *
 *	@return	UBOOL						TRUE if the object should continue the 'normal' cooking operations.
 *										FALSE if the object should not be processed any further.
 */
UBOOL FGenerateGameContentSeekfree::CookObject(class UCookPackagesCommandlet* Commandlet, UPackage* Package, UObject* Object, UBOOL bIsSavedInSeekFreePackage)
{
	if (CurrentCommonPackageInfo == NULL)
	{
		return TRUE;
	}

	// Check for the object in the current list.
	FString FindName = Object->GetPathName();
	FindName = FindName.ToUpper();
	if (CurrentCommonPackageInfo->CookedContentList.Find(FindName) != NULL)
	{
		MarkObjectAsCooked(Object);
		// Return FALSE to indicate no further processing of this object should be done
		return FALSE;
	}
	return TRUE;
}

/** 
 *	PreLoadPackageForCookingCallback
 *	This function will be called in LoadPackageForCooking, prior to the actual loading of the package.
 *	It is intended for pre-loading any required packages, etc.
 *
 *	@param	Commandlet		The cookpackages commandlet being run
 *	@param	Filename		The name of the package to load.
 *
 *	@return	UBOOL		TRUE if the package should be processed further.
 *						FALSE if the cook of this package should be aborted.
 */
UBOOL FGenerateGameContentSeekfree::PreLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, const TCHAR* Filename)
{
	// Is it a map?
	FString CheckMapExt = TEXT(".");
	CheckMapExt += FURL::DefaultMapExt;
	if (appStristr(Filename, *CheckMapExt) == NULL)
	{
		CurrentCommonPackageInfo = NULL;
		// Not a map - let it go
		return TRUE;
	}

	FFilename TempFilename(Filename);
	AGameInfo* GameInfoDefaultObject = NULL;
	UClass* GameInfoClass = FindObject<UClass>(ANY_PACKAGE, TEXT("GameInfo"));
	if(GameInfoClass != NULL)
	{
		GameInfoDefaultObject = GameInfoClass->GetDefaultObject<AGameInfo>();
	}

	if (GameInfoDefaultObject != NULL)
	{
		// The intention here is to 'load' the prefix-common package prior to loading
		// the map. This is to prevent cooking objects in the common pkg into the map.

		FString CheckName = TempFilename.GetBaseFilename();
		FGameTypePrefix GTPrefix;
		appMemzero(&GTPrefix, sizeof(FGameTypePrefix));
		if (GameInfoDefaultObject->GetSupportedGameTypes(CheckName, GTPrefix) == FALSE)
		{
			// Didn't find a prefix type??
			CurrentCommonPackageInfo = NULL;
			warnf(NAME_Log, TEXT("Failed to find supported game type(s) for %s"), *CheckName);
		}
		else 
		{
			if (GTPrefix.bUsesCommonPackage == TRUE)
			{
				// Only preload the common package
				FString CommonName = GTPrefix.Prefix + TEXT("_Common");
				CommonName = CommonName.ToUpper();

				FForceCookedInfo* CheckCommonInfo = Commandlet->PersistentCookerData->GetCookedPrefixCommonInfo(CommonName, FALSE);
				if (!CheckCommonInfo) 
				{
					FGameContentEntry* GameContentEntry = GameTypeCommonContentMap.Find(CommonName);
					if (GameContentEntry)
					{
						debugf(TEXT("Generating %s '%s' on the fly"),*CommonName,*GameContentEntry->PackageCookerInfo.SrcFilename);
						GenerateCommonPackageData(Commandlet,*GameContentEntry->PackageCookerInfo.SrcFilename);

						// reload it; may have been created
						CheckCommonInfo = Commandlet->PersistentCookerData->GetCookedPrefixCommonInfo(CommonName, FALSE);
						if (!CheckCommonInfo)
						{
							warnf(TEXT("Failed to generate %s '%s' on the fly."),*CommonName,*GameContentEntry->PackageCookerInfo.SrcFilename);
							// force an entry so we don't keep trying
							CheckCommonInfo = Commandlet->PersistentCookerData->GetCookedPrefixCommonInfo(CommonName, TRUE);
						}
					}
				}
				if (CurrentCommonPackageInfo != CheckCommonInfo)
				{
					warnf(NAME_Log, TEXT("Setting map prefix GameType to %s"), *CommonName);
					CurrentCommonPackageInfo = CheckCommonInfo;
				}
			}
			else
			{
				if (CurrentCommonPackageInfo != NULL)
				{
					warnf(NAME_Log, TEXT("Clearing map prefix GameType"));
				}
				CurrentCommonPackageInfo = NULL;
			}
		}
 	}
	else
	{
		warnf(NAME_Log, TEXT("Failed to find gameinfo object."));
		CurrentCommonPackageInfo = NULL;
	}

	return TRUE;
}

/**
 *	Create the package for the given filename if it is one that is generated
 *	
 *	@param	Commandlet		The cookpackages commandlet being run
 *	@param	InContentMap	The content map to check
 *	@param	Filename		Current filename that needs to be loaded for cooking
 *	
 *	@return	UPakcage*		The create package; NULL if failed or invalid
 */
UPackage* FGenerateGameContentSeekfree::CreateContentPackageForCooking(class UCookPackagesCommandlet* Commandlet, TMap<FString,FGameContentEntry>& InContentMap, const TCHAR* Filename)
{
	UPackage* Result=NULL;
	FString CheckFilename = FString(Filename).ToUpper();
	for (TMap<FString,FGameContentEntry>::TIterator GTIt(InContentMap); GTIt; ++GTIt)
	{
		const FString& CommonName = GTIt.Key();
		FGameContentEntry& ContentEntry = GTIt.Value();

		if (ContentEntry.PackageCookerInfo.SrcFilename == CheckFilename)
		{
			if ((ContentEntry.ContentList.Num() > 0) || (ContentEntry.ForcedContentList.Num() > 0))
			{
				// create a temporary package to import the content objects
				Result = UObject::CreatePackage(NULL, *ContentEntry.PackageCookerInfo.SrcFilename);
				if (Result == NULL)
				{
					warnf(NAME_Warning, TEXT("GamePreloadContent: Couldn't generate package %s"), *ContentEntry.PackageCookerInfo.SrcFilename);
				}
				else
				{
					// create a referencer to keep track of the content objects added in the package
					UObjectReferencer* ObjRefeferencer = ConstructObject<UObjectReferencer>(UObjectReferencer::StaticClass(),Result);
					ObjRefeferencer->SetFlags( RF_Standalone );

					// load all content objects and add them to the package
					for (INT ContentIdx = 0; ContentIdx < ContentEntry.ContentList.Num(); ContentIdx++)
					{
						const FString& ContentStr = ContentEntry.ContentList(ContentIdx);
						// load the requested content object (typically a content class) 
						LoadObjectOrClass(Commandlet, ContentStr, ObjRefeferencer);
					}

					for (INT ForcedIdx = 0; ForcedIdx < ContentEntry.ForcedContentList.Num(); ForcedIdx++)
					{
						const FString& ContentStr = ContentEntry.ForcedContentList(ForcedIdx);
						// load the requested content object (typically a content class) 
						LoadObjectOrClass(Commandlet, ContentStr, ObjRefeferencer);
					}

					// Mark any already cooked startup objects...
					Commandlet->MarkCookedStartupObjects(FALSE);

					// need to fully load before saving 
					Result->FullyLoad();
				}
			}
		}
	}
	return Result;
}

/**
 * Match game content package filename to the current filename being cooked and 
 * create a temporary package for it instead of loading a package from file. This
 * will also load all the game content needed and add an entry to the object
 * referencer in the package.
 * 
 * @param Commandlet - commandlet that is calling back
 * @param Filename - current filename that needs to be loaded for cooking
 *
 * @return TRUE if succeeded
 */
UPackage* FGenerateGameContentSeekfree::LoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, const TCHAR* Filename)
{
	SCOPE_SECONDS_COUNTER(GenerateGameContentTime);

	UPackage* Result=NULL;

	// Is it a prefix common package?
	{
		SCOPE_SECONDS_COUNTER(GenerateGameContentCommonPackageGenerationTime);

		if (GenerateCommonPackageData(Commandlet, Filename) == TRUE)
		{
			Result = CreateContentPackageForCooking(Commandlet, GameTypeCommonContentMap, Filename);
		}
	}

	// find package that needs to be generated by looking up the filename
	if (Result == NULL)
	{
		SCOPE_SECONDS_COUNTER(GenerateGameContentPackageGenerationTime);
		Result = CreateContentPackageForCooking(Commandlet, GameContentMap, Filename);
	}

	return Result;
}

/**
 *	Dump out stats specific to the game cooker helper.
 */
void FGenerateGameContentSeekfree::DumpStats()
{
	warnf( NAME_Log, TEXT("") );
	warnf( NAME_Log, TEXT("  GameContent Stats:") );
	warnf( NAME_Log, TEXT("    Total time                             %7.2f seconds"), GenerateGameContentTime );
	warnf( NAME_Log, TEXT("    Init time                              %7.2f seconds"), GenerateGameContentInitTime );
	warnf( NAME_Log, TEXT("    Common init time                       %7.2f seconds"), GenerateGameContentCommonInitTime );
	warnf( NAME_Log, TEXT("    List generation time                   %7.2f seconds"), GenerateGameContentListGenerationTime );
	warnf( NAME_Log, TEXT("    Common generation time                 %7.2f seconds"), GenerateGameContentCommonGenerationTime );
	warnf( NAME_Log, TEXT("    Package generation time                %7.2f seconds"), GenerateGameContentPackageGenerationTime );
	warnf( NAME_Log, TEXT("    Common package generation time         %7.2f seconds"), GenerateGameContentCommonPackageGenerationTime );
}

/**
 * Initializes the list of game content packages that need to generated. 
 * Game types along with the content to be loaded for each is loaded from ini.
 * Set in [Cooker.MPGameContentCookStandalone] section
 */
void FGenerateGameContentSeekfree::Init()
{
	SCOPE_SECONDS_COUNTER(GenerateGameContentTime);
	SCOPE_SECONDS_COUNTER(GenerateGameContentInitTime);

	bForceRecookSeekfreeGameTypes=FALSE;
	GameContentMap.Empty();

	// check to see if gametype content should be cooked into its own SF packages
	// enabling this flag will remove the hard game references from the map files
	if (GEngine->bCookSeparateSharedMPGameContent)
	{
		debugf(NAME_Log, TEXT("Saw bCookSeparateSharedMPGameContent flag."));
		// Allow user to specify a single game type on command line.
		FString CommandLineGameType;
		Parse( appCmdLine(), TEXT("MPGAMETYPE="), CommandLineGameType );

		// find the game strings and content to be loaded for each
		TMap<FString, TArray<FString> > MPGameContentStrings;
		GConfig->Parse1ToNSectionOfStrings(TEXT("Cooker.MPGameContentCookStandalone"), TEXT("GameType"), TEXT("Content"), MPGameContentStrings, GEditorIni);
		for( TMap<FString,TArray<FString> >::TConstIterator It(MPGameContentStrings); It; ++It )
		{
			const FString& GameTypeStr = It.Key();
			const TArray<FString>& GameContent = It.Value();
			for( INT Idx=0; Idx < GameContent.Num(); Idx++ )
			{
				const FString& GameContentStr = GameContent(Idx);
				// Only add game type if it matches command line or no command line override was specified.
				if( CommandLineGameType.Len() == 0 || CommandLineGameType == GameTypeStr )
				{
					AddGameContentEntry(GameContentMap, *GameTypeStr, *GameContentStr, FALSE);
				}
			}
		}

		// Setup the common prefix pakages
		InitializeCommonPrefixPackages();
	}
}

/**
 *	Checks to see if any of the map prefix --> gametypes use a common package.
 *	If so, it will store off the information required to create it.
 */
void FGenerateGameContentSeekfree::InitializeCommonPrefixPackages()
{
	SCOPE_SECONDS_COUNTER(GenerateGameContentCommonInitTime);

	if (GEngine->bCookSeparateSharedMPGameContent)
	{
		GameTypeCommonContentMap.Empty();

		// Grab the GameInfo class
		AGameInfo* GameInfoDefaultObject = NULL;
		UClass* GameInfoClass = FindObject<UClass>(ANY_PACKAGE, TEXT("GameInfo"));
		if(GameInfoClass != NULL)
		{
			GameInfoDefaultObject = GameInfoClass->GetDefaultObject<AGameInfo>();
		}

		// Iterate over the prefix mappings
		if (GameInfoDefaultObject != NULL)
		{
			INT DefaultCount = GameInfoDefaultObject->DefaultMapPrefixes.Num();
			INT CustomCount = GameInfoDefaultObject->CustomMapPrefixes.Num();
			INT TotalCount = DefaultCount + CustomCount;

			TMap<FString, TSet<FDependencyRef>> GameTypeToObjectDependenciesMap;
			TMap<FString, TArray<FString>> GameTypeToObjectLoadedMap;

			for (INT MapPrefixIdx = 0; MapPrefixIdx < TotalCount; MapPrefixIdx++)
			{
				FGameTypePrefix* GTPrefix = NULL;
				if (MapPrefixIdx < DefaultCount)
				{
					GTPrefix = &(GameInfoDefaultObject->DefaultMapPrefixes(MapPrefixIdx));
				}
				else
				{
					INT TempIdx = MapPrefixIdx - DefaultCount;
					GTPrefix = &(GameInfoDefaultObject->CustomMapPrefixes(TempIdx));
				}

				if (GTPrefix && (GTPrefix->bUsesCommonPackage == TRUE))
				{
					// Make the name
					FString CommonPackageName = GTPrefix->Prefix;
					CommonPackageName += TEXT("_Common");

					// Add each of the supported gameplay types as 'content'
					for (INT GameTypeIdx = -1; GameTypeIdx < GTPrefix->AdditionalGameTypes.Num(); GameTypeIdx++)
					{
						FString GameContentStr;
						if (GameTypeIdx == -1)
						{
							GameContentStr = GTPrefix->GameType;
						}
						else
						{
							GameContentStr = GTPrefix->AdditionalGameTypes(GameTypeIdx);
						}

						AddGameContentEntry(GameTypeCommonContentMap, *CommonPackageName, *GameContentStr, FALSE);
					}

					for (INT ForcedIdx = 0; ForcedIdx < GTPrefix->ForcedObjects.Num(); ForcedIdx++)
					{
						AddGameContentEntry(GameTypeCommonContentMap, *CommonPackageName, *GTPrefix->ForcedObjects(ForcedIdx), TRUE);
					}
				}
			}
		}
	}
}

/**
 *	Generates the list of common content for the given filename.
 *	This will be stored in both the GameTypeCommonContentMap content list for the prefix
 *	as well as the persistent cooker data list (for retrieval and usage in subsequent cooks)
 *
 *	@param	Commandlet		commandlet that is calling back
 *	@param	Filename		current filename that needs to be loaded for cooking
 *
 *	@return UBOOL			TRUE if successful (and the package should be created and cooked)
 *							FALSE if not (and the package should not be created and cooked)
 */
UBOOL FGenerateGameContentSeekfree::GenerateCommonPackageData(class UCookPackagesCommandlet* Commandlet, const TCHAR* Filename)
{
	UBOOL bSuccessful = FALSE;

	TArray<FString> GameTypesToLoad;
	TArray<FString> ForcedObjectsToLoad;

	FGameContentEntry* CurrContentEntry = NULL;
	FForceCookedInfo* CurrCookedPrefixCommonInfo = NULL;

	FString CheckFilename = FString(Filename).ToUpper();
	for (TMap<FString,FGameContentEntry>::TIterator GTIt(GameTypeCommonContentMap); GTIt; ++GTIt)
	{
		FGameContentEntry& ContentEntry = GTIt.Value();
		if (ContentEntry.PackageCookerInfo.SrcFilename == CheckFilename)
		{
			const FString& CommonName = GTIt.Key();

			CurrCookedPrefixCommonInfo = Commandlet->PersistentCookerData->GetCookedPrefixCommonInfo(CommonName, TRUE);
			CurrContentEntry = &ContentEntry;
			// We've found the entry that will be used to mark objects cooked...
			// Regenerate the lists here
			AGameInfo* GameInfoDefaultObject = NULL;
			UClass* GameInfoClass = FindObject<UClass>(ANY_PACKAGE, TEXT("GameInfo"));
			if(GameInfoClass != NULL)
			{
				GameInfoDefaultObject = GameInfoClass->GetDefaultObject<AGameInfo>();
			}

			// Iterate over the prefix mappings
			if (GameInfoDefaultObject != NULL)
			{
				INT DefaultCount = GameInfoDefaultObject->DefaultMapPrefixes.Num();
				INT CustomCount = GameInfoDefaultObject->CustomMapPrefixes.Num();
				INT TotalCount = DefaultCount + CustomCount;

				TMap<FString, TSet<FDependencyRef>> GameTypeToObjectDependenciesMap;
				TMap<FString, TArray<FString>> GameTypeToObjectLoadedMap;

				for (INT MapPrefixIdx = 0; MapPrefixIdx < TotalCount; MapPrefixIdx++)
				{
					FGameTypePrefix* GTPrefix = NULL;
					if (MapPrefixIdx < DefaultCount)
					{
						GTPrefix = &(GameInfoDefaultObject->DefaultMapPrefixes(MapPrefixIdx));
					}
					else
					{
						INT TempIdx = MapPrefixIdx - DefaultCount;
						GTPrefix = &(GameInfoDefaultObject->CustomMapPrefixes(TempIdx));
					}
					FString CheckPrefix = TEXT("\\");
					CheckPrefix += GTPrefix->Prefix;
					CheckPrefix += TEXT("_COMMON");
					if ((GTPrefix->bUsesCommonPackage == TRUE) && (CheckFilename.InStr(CheckPrefix, FALSE, TRUE) != INDEX_NONE))
					{
						GameTypesToLoad.AddUniqueItem(GTPrefix->GameType);
						for (INT AdditionalIdx = 0; AdditionalIdx < GTPrefix->AdditionalGameTypes.Num(); AdditionalIdx++)
						{
							GameTypesToLoad.AddUniqueItem(GTPrefix->AdditionalGameTypes(AdditionalIdx));
						}

						for (INT ForcedIdx = 0; ForcedIdx < GTPrefix->ForcedObjects.Num(); ForcedIdx++)
						{
							ForcedObjectsToLoad.AddUniqueItem(GTPrefix->ForcedObjects(ForcedIdx));
						}
						break;
					}
				}
			}
		}
	}

	// If there are any objects common to all the supported gametypes
	// or if there are any forced objects, but them into the prefix
	// common package.
	if ((GameTypesToLoad.Num() > 0) || (ForcedObjectsToLoad.Num() > 0))
	{
		if (CurrContentEntry != NULL)
		{
			TMap<FString,INT> CommonContentMap;
			TMap<FString,INT> ForcedContentMap;

			INT FoundGameTypeCount = GenerateLoadedObjectList(Commandlet, GameTypesToLoad, CommonContentMap);
			GenerateLoadedObjectList(Commandlet, ForcedObjectsToLoad, ForcedContentMap);

			// Reset the CurrContentEntry content list (ie remove the gametypes) and 
			// fill it with the actual content used to generate the package to cook...
			// Also, fill in the info stored in the GPCD for subsequent cooks
			CurrContentEntry->ContentList.Empty();
			CurrCookedPrefixCommonInfo->CookedContentList.Empty();


			// For 'common' objects, we only want ones that are in ALL of the loaded types
			for (TMap<FString,INT>::TIterator ObjIt(CommonContentMap); ObjIt; ++ObjIt)
			{
				FString ObjName = ObjIt.Key();
				INT ObjCount = ObjIt.Value();
				if (ObjCount == FoundGameTypeCount)
				{
					// This is a common object!!!
					CurrContentEntry->ContentList.AddItem(ObjName);
					ObjName = ObjName.ToUpper();
					UBOOL bFound = TRUE;
					CurrCookedPrefixCommonInfo->CookedContentList.Set(ObjName, bFound);
				}
			}

			// For forced objects, we want all of them
			for (TMap<FString,INT>::TIterator ForcedIt(ForcedContentMap); ForcedIt; ++ForcedIt)
			{
				FString ObjName = ForcedIt.Key();
				CurrContentEntry->ContentList.AddItem(ObjName);
				UBOOL bFound = TRUE;
				CurrCookedPrefixCommonInfo->CookedContentList.Set(ObjName, bFound);
			}

			bSuccessful = TRUE;
		}
	}

	return bSuccessful;
}

/** 
*	Returns true if a package is a GameContent package
*
*	@param InPackage			Package to check	
*/
UBOOL FGenerateGameContentSeekfree::IsPackageGameContent(UPackage* InPackage)
{
	FString ChoppedPackageName = InPackage->GetName();
	INT SFIndex = ChoppedPackageName.InStr(TEXT("_SF"));
	if (SFIndex != -1)
	{
		ChoppedPackageName = ChoppedPackageName.Left(SFIndex);
	}

	return GameContentMap.Find(ChoppedPackageName) != NULL;
}

/** 
*	Returns true if a package is a GameContent Common package, that is content shared between several GameContent packages
*
*	@param InPackage			Package to check
*/
UBOOL FGenerateGameContentSeekfree::IsPackageGameCommonContent(UPackage* InPackage)
{
	FString ChoppedPackageName = InPackage->GetName();
	INT SFIndex = ChoppedPackageName.InStr(TEXT("_SF"));
	if (SFIndex != -1)
	{
		ChoppedPackageName = ChoppedPackageName.Left(SFIndex);
	}

	return GameTypeCommonContentMap.Find(ChoppedPackageName) != NULL;
}

/**
 * Adds a unique entry for the given game type
 * Adds the content string for each game type given
 *
 * @param InGameStr - game string used as base for package filename
 * @param InContentStr - content to be loaded for the game type package
 */
void FGenerateGameContentSeekfree::AddGameContentEntry(TMap<FString,FGameContentEntry>& InContentMap, const TCHAR* InGameStr,const TCHAR* InContentStr, UBOOL bForced)
{
	if (!InGameStr || !InContentStr)
	{
		return;
	}

	FString GameStr = FString(InGameStr).ToUpper();
	FGameContentEntry* GameContentEntryPtr = InContentMap.Find(GameStr);
	if( GameContentEntryPtr == NULL)
	{
		FGameContentEntry GameContentEntry;
		InContentMap.Set(GameStr,GameContentEntry);
		GameContentEntryPtr = InContentMap.Find(GameStr);
	}
	check(GameContentEntryPtr);
	if (bForced == FALSE)
	{
		GameContentEntryPtr->ContentList.AddUniqueItem(FString(InContentStr).ToUpper());
	}
	else
	{
		GameContentEntryPtr->ForcedContentList.AddUniqueItem(FString(InContentStr).ToUpper());
	}
}

/**
 * @return FGenerateGameContentSeekfree	Singleton instance for game content seekfree helper class
 */
class FGenerateGameContentSeekfree* FGameCookerHelper::GetGameContentSeekfreeHelper()
{
	static FGenerateGameContentSeekfree MapPreloadHelper;
	return &MapPreloadHelper;
}

//-----------------------------------------------------------------------------
//	ForcedContentHelper
//-----------------------------------------------------------------------------
/**
 * Initialize options from command line options
 *
 *	@param	Tokens			Command line tokens parsed from app
 *	@param	Switches		Command line switches parsed from app
 */
void FForcedContentHelper::InitOptions(const TArray<FString>& Tokens, const TArray<FString>& Switches)
{
}

/**
 *	Attempt to load each of the objects in the given list.
 *	If loaded, iterate over all objects that get loaded along with it and add them to
 *	the given OutLoadedObjectMap - incrementing the count of objects found.
 *
 *	@param	InObjectsToLoad			The list of objects to load
 *	@param	OutLoadedObjectMap		All objects loaded, and the number of times they were loaded
 *	
 *	@return	INT						The number of object in the InObjectsToLoad list that successfully loaded
 */
INT FForcedContentHelper::GenerateLoadedObjectList(class UCookPackagesCommandlet* Commandlet, const FString& InPMapName, 
	const TArray<FString>& InObjectsToLoad, TMap<FString,INT>& OutLoadedObjectMap)
{
	INT FoundObjectCount = 0;
	TMap<FString,UBOOL> ContentObjReferencedList;

	// Create a temporary package to import the content objects
	// This is required so that we can 'process' the package for cooking, 
	// clearing out materials, etc.
	FString TempPackageName = FString::Printf(TEXT("%s_Forced_TmpPkg"), *InPMapName);
	UPackage* TempPackage = UObject::CreatePackage(NULL, *TempPackageName);
	check(TempPackage != NULL);
	// create a referencer to keep track of the content objects added in the package
	UObjectReferencer* ObjRefeferencer = ConstructObject<UObjectReferencer>(UObjectReferencer::StaticClass(), TempPackage);
	ObjRefeferencer->SetFlags(RF_Standalone);

	for (INT LoadIdx = 0; LoadIdx < InObjectsToLoad.Num(); LoadIdx++)
	{
		FString ObjectStr = InObjectsToLoad(LoadIdx);
		UObject* ContentObj = LoadObjectOrClass(Commandlet, ObjectStr, NULL);
		if (ContentObj != NULL)
		{
			FoundObjectCount++;

			FString ContentObjUpperName = ContentObj->GetPathName().ToUpper();
			OutLoadedObjectMap.Set(ContentObjUpperName, TRUE);
			ObjRefeferencer->ReferencedObjects.AddUniqueItem(ContentObj);
		}
	}

	if (FoundObjectCount > 0)
	{
		// Mark any already cooked startup objects...
		Commandlet->MarkCookedStartupObjects(TRUE);
		// Now we have a list of objects we would serialize (remove any marked by startup)
		// So clean as usual... then iterate over and any objects !Marked are still valid
		// This is so that we don't pull in base textures, etc.
		// prep the temp package for cooking - cleanup materials, etc.
		Commandlet->PrepPackageForObjectCooking(TempPackage, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE);

		// Prevent disabled emitters from pulling in materials...
		//@todo. This needs to be done for *all* objects...
		ObjRefeferencer->AddToRoot();
		for (TObjectIterator<UObject> ObjIt; ObjIt; ++ObjIt)
		{
			UParticleSystem* ParticleSystem = Cast<UParticleSystem>(*ObjIt);
			if (ParticleSystem != NULL)
			{
				Commandlet->CookParticleSystem(ParticleSystem);
			}
		}
		UObject::CollectGarbage(RF_Native);
		ObjRefeferencer->RemoveFromRoot();

		// Iterate over all objects that are loaded and not marked by cooker, adding them to the list
		for (TObjectIterator<UObject> ObjIt; ObjIt; ++ObjIt)
		{
			UObject* CheckObject = *ObjIt;
			if (CheckObject->GetOuter() != NULL)
			{
				if ((CheckObject->IsA(UPersistentCookerData::StaticClass()) == FALSE) &&
					(CheckObject->IsA(UShaderCache::StaticClass()) == FALSE) &&
					(CheckObject->IsA(UObjectReferencer::StaticClass()) == FALSE))
				{
					if (!CheckObject->HasAnyFlags(RF_Transient) && !CheckObject->IsIn(UObject::GetTransientPackage()))
					{
						if ((CheckObject->HasAnyFlags(RF_MarkedByCooker|RF_CookedStartupObject) == FALSE) && 
							(CheckObject->IsIn(TempPackage) || CheckObject->HasAnyFlags(RF_ForceTagExp)))
						{
							FString ExportPathName = CheckObject->GetPathName();
							ExportPathName = ExportPathName.ToUpper();
							OutLoadedObjectMap.Set(ExportPathName, 1);
						}
					}
				}
			}
		}
	}
	UObject::CollectGarbage(RF_Native);
	return FoundObjectCount;
}

/**
 * Cooks passed in object if it hasn't been already.
 *
 *	@param	Commandlet					The cookpackages commandlet being run
 *	@param	Package						Package going to be saved
 *	@param	Object						Object to cook
 *	@param	bIsSavedInSeekFreePackage	Whether object is going to be saved into a seekfree package
 *
 *	@return	UBOOL						TRUE if the object should continue the 'normal' cooking operations.
 *										FALSE if the object should not be processed any further.
 */
UBOOL FForcedContentHelper::CookObject(class UCookPackagesCommandlet* Commandlet, UPackage* Package, UObject* Object, UBOOL bIsSavedInSeekFreePackage)
{
	SCOPE_SECONDS_COUNTER(ForcedContentTime);
	if ((bPMapForcedObjectsEnabled == TRUE) && (bPMapBeingProcessed == FALSE) && (CurrentPMapForcedObjectsList != NULL))
	{
		// See if the object being cooked is in the forced PMap list
		FString ObjectName = Object->GetPathName().ToUpper();
		if (CurrentPMapForcedObjectsList->CookedContentList.Find(ObjectName) != NULL)
		{
			MarkObjectAsCooked(Object);
			return FALSE;
		}
	}
	return TRUE;
}

/** 
 *	PreLoadPackageForCookingCallback
 *	This function will be called in LoadPackageForCooking, prior to the actual loading of the package.
 *	It is intended for pre-loading any required packages, etc.
 *
 *	@param	Commandlet		The cookpackages commandlet being run
 *	@param	Filename		The name of the package to load.
 *
 *	@return	UBOOL		TRUE if the package should be processed further.
 *						FALSE if the cook of this package should be aborted.
 */
UBOOL FForcedContentHelper::PreLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, const TCHAR* Filename)
{
	SCOPE_SECONDS_COUNTER(ForcedContentTime);
	if (bPMapForcedObjectsEnabled == FALSE)
	{
		return TRUE;
	}

	FFilename LookupFilename = Filename;
	FString CheckName = LookupFilename.GetBaseFilename().ToUpper();
	FString PMapSource;
	if (Commandlet->PersistentMapInfoHelper.GetPersistentMapAlias(CheckName, PMapSource) == TRUE)
	{
		// Copy the alias in
		CheckName = PMapSource;
	}
	PMapBeingProcessed = TEXT("");
	if (Commandlet->PersistentMapInfoHelper.GetPersistentMapForLevel(CheckName, PMapSource) == TRUE)
	{
		// This will generate it the first time the PMap-list is requested
		FForceCookedInfo* ForceCookedInfo = GetPMapForcedObjectList(Commandlet, PMapSource, TRUE);
		if (ForceCookedInfo != NULL)
		{
			// We are cooking a map that has a PMap
			CurrentPMapForcedObjectsList = ForceCookedInfo;
			if (CheckName == PMapSource)
			{
				// It is the PMap itself!
				bPMapBeingProcessed = TRUE;
				PMapBeingProcessed = PMapSource;
			}
			else
			{
				bPMapBeingProcessed = FALSE;
			}
		}
		else
		{
			bPMapBeingProcessed = FALSE;
			CurrentPMapForcedObjectsList = NULL;
		}
	}
	else
	{
		bPMapBeingProcessed = FALSE;
		CurrentPMapForcedObjectsList = NULL;
	}

	return TRUE;
}

/**
 */
UBOOL FForcedContentHelper::PostLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, UPackage* InPackage)
{
	SCOPE_SECONDS_COUNTER(ForcedContentTime);
	if ((bPMapForcedObjectsEnabled == TRUE) && (bPMapBeingProcessed == TRUE) && (CurrentPMapForcedObjectsList != NULL))
	{
		// Find the world info and add the object referencer!
		for (TObjectIterator<AWorldInfo> It; It; ++It)
		{
			AWorldInfo* WorldInfo = *It;
			if (WorldInfo->GetOutermost()->GetName().InStr(PMapBeingProcessed, FALSE, TRUE) != INDEX_NONE)
			{
				// This is the one we want
				UObjectReferencer* ObjReferencer = ConstructObject<UObjectReferencer>(UObjectReferencer::StaticClass(), WorldInfo);
				check(ObjReferencer);
				ObjReferencer->SetFlags(RF_Standalone);
				// Load each object in the forced list and add it to the object referencer.
				for (TMap<FString,UBOOL>::TIterator ContentIt(CurrentPMapForcedObjectsList->CookedContentList); ContentIt; ++ContentIt)
				{
					FString ContentStr = ContentIt.Key();
					// load the requested content object (typically a content class) 
					LoadObjectOrClass(Commandlet, ContentStr, ObjReferencer);
				}
				WorldInfo->PersistentMapForcedObjects = ObjReferencer;
				break;
			}
		}
	}
	return TRUE;
}

/**
 *	Called when the FPersistentMapInfo class has been initialized and filled in.
 *	Can be used to do any special setup for PMaps the cooker may require.
 *
 *	@param	Commandlet		The commandlet that is being run.
 */
void FForcedContentHelper::PersistentMapInfoGeneratedCallback(class UCookPackagesCommandlet* Commandlet)
{
	SCOPE_SECONDS_COUNTER(ForcedContentTime);
	// Force Map Objects
	if (bPMapForcedObjectsEnabled == TRUE)
	{
		// If object are being forced into the PMaps, then setup the lists here!
		TArray<FString> PMapList;
		Commandlet->PersistentMapInfoHelper.GetPersistentMapList(PMapList);

		// Iterate thru the list and 
		for (INT PMapIdx = 0; PMapIdx < PMapList.Num(); PMapIdx++)
		{
			FString PMapName = PMapList(PMapIdx);
			// find the game strings and content to be loaded for each
			FString PMapForcedIniSection = FString::Printf(TEXT("Cooker.ForcedMapObjects.%s"), *PMapName);
			FConfigSection* PMapForceObjectsMap = GConfig->GetSectionPrivate(*PMapForcedIniSection, FALSE, TRUE, GEditorIni);
			if (PMapForceObjectsMap != NULL)
			{
				// Split up the remotes and the pawns...
				TArray<FString> ForcedObjectsList;
				for (FConfigSectionMap::TIterator It(*PMapForceObjectsMap); It; ++It)
				{
					FString ObjectName = It.Value();
					ForcedObjectsList.AddUniqueItem(ObjectName);
				}

				if (ForcedObjectsList.Num() > 0)
				{
					TMap<FString,UBOOL>* ForcedObjectsMap = PMapForcedObjectsMap.Find(PMapName);
					if (ForcedObjectsMap == NULL)
					{
						TMap<FString,UBOOL> Temp;
						PMapForcedObjectsMap.Set(PMapName, Temp);
						ForcedObjectsMap = PMapForcedObjectsMap.Find(PMapName);
					}
					check(ForcedObjectsMap);
				
					for (INT ForcedIdx = 0; ForcedIdx < ForcedObjectsList.Num(); ForcedIdx++)
					{
						FString ForcedUpper = ForcedObjectsList(ForcedIdx).ToUpper();
						ForcedObjectsMap->Set(ForcedUpper, TRUE);
					}
				}
			}
		}
	}

}

/**
 *	Dump out stats specific to the game cooker helper.
 */
void FForcedContentHelper::DumpStats()
{
	warnf( NAME_Log, TEXT("") );
	warnf( NAME_Log, TEXT("  ForcedContent Stats:") );
	warnf( NAME_Log, TEXT("    Total time                             %7.2f seconds"), ForcedContentTime );
}

/**
 */
void FForcedContentHelper::Init()
{
	SCOPE_SECONDS_COUNTER(ForcedContentTime);
	GConfig->GetBool(TEXT("Cooker.ForcedMapObjects"), TEXT("bAllowPMapForcedObjects"), bPMapForcedObjectsEnabled, GEditorIni);
}

/**
 *	Retrieves the list of forced objects for the given PMap.
 *
 *	@param	Commandlet			commandlet that is calling back
 *	@param	InPMapName			the PMap to get the forced object list for
 *	@param	bCreateIfNotFound	if TRUE, generate the list for the given PMap
 *
 *	@return FForceCookedInfo*	The force cooked info for the given PMap
 *								NULL if not found (or not a valid PMap)
 */
FForceCookedInfo* FForcedContentHelper::GetPMapForcedObjectList(class UCookPackagesCommandlet* Commandlet, const FString& InPMapName, UBOOL bCreateIfNotFound)
{
	FForceCookedInfo* Result = NULL;
	// Generate the information if the PMap is a valid one
	FString UpperPMapName = InPMapName.ToUpper();
	TMap<FString,UBOOL>* ForcedObjectsMap = PMapForcedObjectsMap.Find(UpperPMapName);
	if (ForcedObjectsMap != NULL)
	{
		// We have a valid PMap w/ forced objects
		//@todo. We need to handle detecting that the object list stored is different than the current forced list.
		// This can be done by using the UBOOL flags stored in the ForceCokedInfo list, setting it to TRUE for
		// objects that are in the ForcedObject list, and FALSE to ones that are loaded as a side-effect.
		// For now we will simply assume that the list it correct if it is found.
		Result = Commandlet->PersistentCookerData->GetPMapForcedObjectInfo(InPMapName, FALSE);
		if ((Result == NULL) && (bCreateIfNotFound == TRUE))
		{
			Result = Commandlet->PersistentCookerData->GetPMapForcedObjectInfo(UpperPMapName, TRUE);

			// Create a list of objects to load (ie, the forced objects)
			TArray<FString> ForceObjectList;
			for (TMap<FString,UBOOL>::TIterator ForceObjIt(*ForcedObjectsMap); ForceObjIt; ++ForceObjIt)
			{
				ForceObjectList.AddUniqueItem(ForceObjIt.Key());
			}

			TMap<FString,INT> LoadedObjectMap;
			INT LoadedObjCount = GenerateLoadedObjectList(Commandlet, InPMapName, ForceObjectList, LoadedObjectMap);
			if (LoadedObjCount > 0)
			{
				// Add these to the PCD list
				for (TMap<FString,INT>::TIterator AddIt(LoadedObjectMap); AddIt; ++AddIt)
				{
					// We don't care about the counts for forced objects
					FString ObjectName = AddIt.Key();
					ObjectName = ObjectName.ToUpper();
					Result->CookedContentList.Set(ObjectName, TRUE);
				}
			}
		}
	}
	return Result;
}

/**
 */
class FForcedContentHelper* FGameCookerHelper::GetForcedContentHelper()
{
	static FForcedContentHelper ForcedContentHelper;
	return &ForcedContentHelper;
}

//-----------------------------------------------------------------------------
//	GameStreamingLevelCookerHelper
//-----------------------------------------------------------------------------
static DOUBLE	StreamingLevelHelper_TotalTime;
static DOUBLE	StreamingLevelHelper_PreLoadTime;
static DOUBLE	StreamingLevelHelper_PostLoadTime;
static DOUBLE	StreamingLevelHelper_CookObjectTime;

/**
 * Initialize the cooker helper and process any command line params
 *
 *	@param	Commandlet		The cookpackages commandlet being run
 *	@param	Tokens			Command line tokens parsed from app
 *	@param	Switches		Command line switches parsed from app
 */
void FGameStreamingLevelCookerHelper::Init(class UCookPackagesCommandlet* Commandlet, const TArray<FString>& Tokens, const TArray<FString>& Switches)
{
	GConfig->GetBool(TEXT("Cooker.GeneralOptions"), TEXT("bDisablePMapObjectChecking"), bDisablePMapObjectChecking, GEditorIni);
	// Check for flag to skip this entirely...
	if ((Switches.ContainsItem(TEXT("SKIPPMAPOBJS")) == TRUE) || (Switches.ContainsItem(TEXT("FASTCOOK")) == TRUE))
	{
		bDisablePMapObjectChecking = TRUE;
	}
	if ((Commandlet->Platform & UE3::PLATFORM_Stripped) == 0)
	{
		// Disable PMap object optimization for non-console platforms
		bDisablePMapObjectChecking = TRUE;
	}
	warnf(NAME_Log, TEXT("Running w/ PMap Object size optimization %s."), bDisablePMapObjectChecking ? TEXT("DISABLED") : TEXT("ENABLED"));
	bDumpPMapObjectList = Switches.ContainsItem(TEXT("LOGPMAPOBJS"));
	if (bDumpPMapObjectList == TRUE)
	{
		warnf(NAME_Log, TEXT("PMap Object dumping is ENABLED."));
	}
}

struct FStreamingLevelCooker_PMapSwapInfo
{
public:
	INT PMapIndex;
	INT FirstSublevelIndex;

	FStreamingLevelCooker_PMapSwapInfo() :
	PMapIndex(-1)
		, FirstSublevelIndex(-1)
	{
	}

	inline UBOOL operator==(const FStreamingLevelCooker_PMapSwapInfo& Src)
	{
		return (
			(PMapIndex == Src.PMapIndex) &&
			(FirstSublevelIndex == Src.FirstSublevelIndex)
			);
	}

	inline FStreamingLevelCooker_PMapSwapInfo& operator=(const FStreamingLevelCooker_PMapSwapInfo& Src)
	{
		PMapIndex = Src.PMapIndex;
		FirstSublevelIndex = Src.FirstSublevelIndex;
		return *this;
	}
};

/** 
 *	Generate the package list for audio-related files.
 *
 *	@param	Commandlet							The cookpackages commandlet being run
 *	@param	Platform							The platform being cooked for
 *	@param	ShaderPlatform						The shader platform being cooked for
 *	@param	NotRequiredFilenamePairs			The package lists being filled in...
 *	@param	RegularFilenamePairs				""
 *	@param	MapFilenamePairs					""
 *	@param	ScriptFilenamePairs					""
 *	@param	StartupFilenamePairs				""
 *	@param	StandaloneSeekfreeFilenamePairs		""
 *	
 *	@return	UBOOL		TRUE if successfull, FALSE is something went wrong.
 */
UBOOL FGameStreamingLevelCookerHelper::GeneratePackageList( 
	class UCookPackagesCommandlet* Commandlet, 
	UE3::EPlatformType Platform,
	EShaderPlatform ShaderPlatform,
	TArray<FPackageCookerInfo>& NotRequiredFilenamePairs,
	TArray<FPackageCookerInfo>& RegularFilenamePairs,
	TArray<FPackageCookerInfo>& MapFilenamePairs,
	TArray<FPackageCookerInfo>& ScriptFilenamePairs,
	TArray<FPackageCookerInfo>& StartupFilenamePairs,
	TArray<FPackageCookerInfo>& StandaloneSeekfreeFilenamePairs)
{
	if (bDisablePMapObjectChecking == FALSE)
	{
		// Check the map list for PMaps and make sure they are first in the list...
		TMap<FString,FStreamingLevelCooker_PMapSwapInfo> PMapToFirstSublevelMapping;
		for (INT MapIdx = 0; MapIdx < MapFilenamePairs.Num(); MapIdx++)
		{
			FPackageCookerInfo& MapInfo = MapFilenamePairs(MapIdx);
			FString ShortMapFilename = MapInfo.SrcFilename.GetBaseFilename();
			FString PMapForLevel;
			if (Commandlet->PersistentMapInfoHelper.GetPersistentMapForLevel(ShortMapFilename, PMapForLevel) == TRUE)
			{
				FStreamingLevelCooker_PMapSwapInfo* PMapInfo = PMapToFirstSublevelMapping.Find(PMapForLevel);
				if (PMapInfo == NULL)
				{
					// First one...
					FStreamingLevelCooker_PMapSwapInfo TempInfo;
					PMapToFirstSublevelMapping.Set(PMapForLevel, TempInfo);
					PMapInfo = PMapToFirstSublevelMapping.Find(PMapForLevel);
					PMapInfo->FirstSublevelIndex = MapIdx;
				}
				const TArray<FString>* SublevelList = Commandlet->PersistentMapInfoHelper.GetPersistentMapContainedLevelsList(ShortMapFilename);
				if (SublevelList != NULL)
				{
					// It's the pmap... we need to swap it for the first sublevel in it's list
					PMapInfo->PMapIndex = MapIdx;
				}
			}
		}

		// Swap all PMaps to be before their sublevels...
		for (TMap<FString,FStreamingLevelCooker_PMapSwapInfo>::TIterator PMapIt(PMapToFirstSublevelMapping); PMapIt; ++PMapIt)
		{
			FStreamingLevelCooker_PMapSwapInfo& PMapSwapInfo = PMapIt.Value();
			if ((PMapSwapInfo.FirstSublevelIndex != -1) && (PMapSwapInfo.PMapIndex != -1))
			{
				if (PMapSwapInfo.FirstSublevelIndex != PMapSwapInfo.PMapIndex)
				{
					FPackageCookerInfo TempPMapInfo = MapFilenamePairs(PMapSwapInfo.PMapIndex);
					MapFilenamePairs.Remove(PMapSwapInfo.PMapIndex);
					MapFilenamePairs.InsertItem(TempPMapInfo, PMapSwapInfo.FirstSublevelIndex);
				}
			}
		}
	}
	return TRUE;
}

/** 
 *	PreLoadPackageForCookingCallback
 *	This function will be called in LoadPackageForCooking, prior to the actual loading of the package.
 *	It is intended for pre-loading any required packages, etc.
 *
 *	@param	Commandlet		The cookpackages commandlet being run
 *	@param	Filename		The name of the package to load.
 *
 *	@return	UBOOL		TRUE if the package should be processed further.
 *						FALSE if the cook of this package should be aborted.
 */
UBOOL FGameStreamingLevelCookerHelper::PreLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, const TCHAR* Filename)
{
	if ((bDisablePMapObjectChecking == FALSE) && (Commandlet->bPreloadingPackagesForDLC == FALSE))
	{
		check(Commandlet);

		SCOPE_SECONDS_COUNTER(StreamingLevelHelper_TotalTime);
		SCOPE_SECONDS_COUNTER(StreamingLevelHelper_PreLoadTime);

		FFilename TempFilename(Filename);
		FString ShortMapName = TempFilename.GetBaseFilename();
		FString CheckPMapName;
		if (Commandlet->PersistentMapInfoHelper.GetPersistentMapForLevel(ShortMapName, CheckPMapName) == TRUE)
		{
			if (CheckPMapName != ShortMapName)
			{
				if (CheckPMapName != CurrentPersistentMap)
				{
					if (bDumpPMapObjectList == TRUE)
					{
						debugf(TEXT("Filling in PMap information for %s..."), *CheckPMapName);
					}
					// Clear the previous PMap obj list...
					ObjectsInPMap.Empty();

					TArray<FString> NativeScriptPackages;
					appGetScriptPackageNames(NativeScriptPackages, SPT_Native);
					TMap<FString,UBOOL> NativeScriptMap;
					for (INT NativeIdx = 0; NativeIdx < NativeScriptPackages.Num(); NativeIdx++)
					{
						NativeScriptMap.Set(NativeScriptPackages(NativeIdx), TRUE);
					}
					// Load the pmap... less than ideal, but works to determine what will be cooked.
					UPackage* PMapPackage = UObject::LoadPackage(NULL, *CheckPMapName, LOAD_NoWarn);
					if (PMapPackage != NULL)
					{
						// Mark any already cooked startup objects...
						Commandlet->MarkCookedStartupObjects(FALSE);

						// Since it is a PMap, we know that:
						//	bInShouldBeSeekFree				This is TRUE, as we only perform this for cooked & stripped platforms
						//	bInShouldBeFullyCompressed		This is FALSE as maps are not fully compressed
						//	bInIsNativeScriptFile			This is FALSE as it is a map
						//	bInIsCombinedStartupPackage		This is FALSE as it is a map
						//	bInStripEverythingButTextures	This is FALSE as it is a map
						//	bInProcessShaderCaches			This is FALSE as we are not actually saving the package
						Commandlet->PrepPackageForObjectCooking(PMapPackage, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE);

						// Process particle systems as they can also result in unreferened materials...
						for (FObjectIterator ObjIt; ObjIt; ++ObjIt)
						{
							UParticleSystem* ParticleSystem = Cast<UParticleSystem>(*ObjIt);
							if (ParticleSystem != NULL)
							{
								Commandlet->CookParticleSystem(ParticleSystem);
							}
						}

						
						// When cooking for mobile, clear out material references from the graph network,
						// as well as any texture references from the level objects (used for streaming.)
						// We need to do this here because PrepPackageForObjectCooking() is currently unable
						// to do this stripping, because of how cook-time texture flattening for mobile works.
						if( GCookingTarget & UE3::PLATFORM_Mobile )
						{
							for( TObjectIterator<UMaterialInstance> It; It; ++It )
							{
								UMaterialInstance* MaterialInstance = *It;
								if (!MaterialInstance->HasAnyFlags(RF_ClassDefaultObject|RF_Transient))
								{
									MaterialInstance->ClearParameterValues();

									for (INT Quality = 0; Quality < MSQ_MAX; Quality++)
									{
										if (MaterialInstance->StaticPermutationResources[Quality])
										{
											MaterialInstance->StaticPermutationResources[Quality]->RemoveUniformExpressionTextures();
										}
									}

									MaterialInstance->ReferencedTextureGuids.Empty();
								}
							}

							for( TObjectIterator<UMaterial> It; It; ++It )
							{
								UMaterial* Material = *It;
								if (Material && !Material->HasAnyFlags(RF_Transient))
								{
									Material->RemoveExpressions(TRUE);
									Material->ReferencedTextureGuids.Empty();
								}
							}

							for( TObjectIterator<ULevel> It; It; ++It )
							{
								ULevel* Level = *It;
								if (Level)
								{
									Level->TextureToInstancesMap.Empty();
									Level->DynamicTextureInstances.Empty();
								}
							}
						}


						// Get the world and add it to the root to prevent if from getting GC'd
						UWorld* PMapWorld = FindObjectChecked<UWorld>(PMapPackage, TEXT("TheWorld"));
						check(PMapWorld);
						PMapWorld->AddToRoot();

						// GC to remove all loaded stuff that wasn't really needed...
						UObject::CollectGarbage(RF_Native);

						// Iterate over all objects that are loaded and not marked by cooker, adding them to the list
						for (TObjectIterator<UObject> ObjIt; ObjIt; ++ObjIt)
						{
							UObject* CheckObject = *ObjIt;
							if (CheckObject->GetOuter() != NULL)
							{
								// Let packages go... we don't ever want to mark them
								if (CheckObject->IsA(UPackage::StaticClass()) == FALSE)
								{
									if (!CheckObject->HasAnyFlags(RF_Transient) && !CheckObject->IsIn(UObject::GetTransientPackage()))
									{
										if (CheckObject->HasAnyFlags(RF_MarkedByCooker) == FALSE)
										{
											FString ObjectName = CheckObject->GetPathName();
											UBOOL bInMapObject = FALSE;
											UBOOL bInNativePackage = FALSE;
											INT FirstDotIdx = ObjectName.InStr(TEXT("."));
											if (FirstDotIdx != INDEX_NONE)
											{
												FString PackageName = ObjectName.Left(FirstDotIdx);
												if (PackageName == CheckPMapName)
												{
													bInMapObject = TRUE;
												}
												bInNativePackage = (NativeScriptMap.Find(PackageName) != NULL);
											}
											if (bInNativePackage == FALSE)
											{
												if (bInMapObject == FALSE)
												{
													ObjectsInPMap.Set(ObjectName, TRUE);
												}
											}
										}
									}
								}
							}
						}
						// Remove the PMap world from the root so it gets cleaned up.
						PMapWorld->RemoveFromRoot();
						// Clean up the package
						UObject::CollectGarbage(RF_Native);
					}
					else
					{
						warnf(NAME_Warning, TEXT("Failed to load PMap package %s"), *CheckPMapName);
					}

					CurrentPersistentMap = CheckPMapName;

					if (bDumpPMapObjectList == TRUE)
					{
						debugf(TEXT("<---------------------------------------------------"));
						debugf(TEXT("Objects in PMap %s"), *CheckPMapName);
						for (TMap<FString,UBOOL>::TIterator DumpIt(ObjectsInPMap); DumpIt; ++DumpIt)
						{
							FString& DumpObjName = DumpIt.Key();
							debugf(TEXT("\t%s"), *DumpObjName);
						}
					}
				}
				else
				{
					// Same PMap... nothing to do as we have already filled in the PMapObject list
				}
			}
			else
			{
				CurrentPersistentMap = TEXT("");
				ObjectsInPMap.Empty();
			}
		}
		else
		{
			CurrentPersistentMap = TEXT("");
			ObjectsInPMap.Empty();
		}
	}

	// Always continue processing...
	return TRUE;
}

/** 
 *	PostLoadPackageForCookingCallback
 *	This function will be called in LoadPackageForCooking, prior to any
 *	operations occurring on the contents...
 *
 *	@param	Commandlet	The cookpackages commandlet being run
 *	@param	Package		The package just loaded.
 *
 *	@return	UBOOL		TRUE if the package should be processed further.
 *						FALSE if the cook of this package should be aborted.
 */
UBOOL FGameStreamingLevelCookerHelper::PostLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, UPackage* InPackage)
{
	if ((bDisablePMapObjectChecking == FALSE) && (Commandlet->bPreloadingPackagesForDLC == FALSE))
	{
		SCOPE_SECONDS_COUNTER(StreamingLevelHelper_TotalTime);
		SCOPE_SECONDS_COUNTER(StreamingLevelHelper_PostLoadTime);

		// If we have a PMap object list, then clear any objects found in it.
		if (ObjectsInPMap.Num() > 0)
		{
			for (FObjectIterator ObjIt; ObjIt; ++ObjIt)
			{
				UObject* Object = *ObjIt;
				// Just in case, skip packages again here...
				if (Object->IsA(UPackage::StaticClass()) == FALSE)
				{
					// We are cooking a sublevel if there are entries in the ObjectsInPMap
					if (ObjectsInPMap.Find(Object->GetPathName()) != NULL)
					{
						// It's in the PMap... so don't cook it into the streaming level
						Object->ClearFlags(RF_ForceTagExp);
						Object->SetFlags(RF_MarkedByCooker);
						debugfSlow(TEXT("%32s: Clearing object %s"), *(InPackage->GetName()), *(Object->GetPathName()));
					}
				}
			}
		}
	}
	return TRUE;
}

/**
 * Cooks passed in object if it hasn't been already.
 *
 *	@param	Commandlet					The cookpackages commandlet being run
 *	@param	Package						Package going to be saved
 *	@param	Object						Object to cook
 *	@param	bIsSavedInSeekFreePackage	Whether object is going to be saved into a seekfree package
 *
 *	@return	UBOOL						TRUE if the object should continue the 'normal' cooking operations.
 *										FALSE if the object should not be processed any further.
 */
UBOOL FGameStreamingLevelCookerHelper::CookObject(class UCookPackagesCommandlet* Commandlet, UPackage* Package, UObject* Object, UBOOL bIsSavedInSeekFreePackage)
{
	if (bDisablePMapObjectChecking == FALSE)
	{
		SCOPE_SECONDS_COUNTER(StreamingLevelHelper_TotalTime);
		SCOPE_SECONDS_COUNTER(StreamingLevelHelper_CookObjectTime);

		// We are cooking a sublevel if there are entries in the ObjectsInPMap
		if (ObjectsInPMap.Find(Object->GetPathName()) != NULL)
		{
			// It's in the PMap... so don't cook it into the streaming level
			Object->ClearFlags(RF_ForceTagExp);
			Object->SetFlags(RF_MarkedByCooker);
			debugfSlow(TEXT("%32s: Clearing object %s"), *(Package->GetName()), *(Object->GetPathName()));
			return FALSE;
		}
	}

	return TRUE;
}

/**
 *	Dump out stats specific to the game cooker helper.
 */
void FGameStreamingLevelCookerHelper::DumpStats()
{
	warnf(NAME_Log, TEXT(""));
	warnf(NAME_Log, TEXT("  StreamingLevel CookerHelper Stats:"));
	warnf(NAME_Log, TEXT("    Total time                             %7.2f seconds"), StreamingLevelHelper_TotalTime);
	warnf(NAME_Log, TEXT("    PreLoad time                           %7.2f seconds"), StreamingLevelHelper_PreLoadTime);
	warnf(NAME_Log, TEXT("    PostLoad time                          %7.2f seconds"), StreamingLevelHelper_PostLoadTime);
	warnf(NAME_Log, TEXT("    CookObject time                        %7.2f seconds"), StreamingLevelHelper_CookObjectTime);
}

/**
 */
class FGameStreamingLevelCookerHelper* FGameCookerHelper::GetStreamingLevelCookerHelper()
{
	static FGameStreamingLevelCookerHelper StreamingLevelHelper;
	return &StreamingLevelHelper;
}

//-----------------------------------------------------------------------------
//	GameCookerHelper
//-----------------------------------------------------------------------------

/**
* Initialize the cooker helpr and process any command line params
*
*	@param	Commandlet		The cookpackages commandlet being run
*	@param	Tokens			Command line tokens parsed from app
*	@param	Switches		Command line switches parsed from app
*/
void FGameCookerHelper::Init(
	class UCookPackagesCommandlet* Commandlet, 
	const TArray<FString>& Tokens, 
	const TArray<FString>& Switches )
{
	GetGameContentSeekfreeHelper()->InitOptions(Tokens,Switches);
	GetForcedContentHelper()->InitOptions(Tokens,Switches);
	GetStreamingLevelCookerHelper()->Init(Commandlet, Tokens, Switches);

#if WITH_SUBSTANCE_AIR == 1
	GConfig->GetBool(
		TEXT("SubstanceAir"),
		TEXT("bInstallTimeGeneration"),
		GUseSubstanceInstallTimeCache,
		GEngineIni);

	SubstanceAirHelper_sbsasrTotal_Kb = 0;
	SubstanceAirHelper_mipsRemoved_Kb = 0;
	SubstanceAirHelper_mipsKept_Kb = 0;

	if (GUseSubstanceInstallTimeCache)
	{
		CurrentPackageContainsMap = FALSE;

		FString cacheDir = appGameDir() + TEXT("SubstanceAir");
		if (!appDirectoryExists(*cacheDir))
		{
			GFileManager->MakeDirectory(*cacheDir, TRUE);
		}

		// If in MT cooking, each process gets an ID to avoid TFC filename collision,
		// use it to avoid filename collision for SubstanceAir Texture caches
		// (which is a kind of TFC)
		FString childProcessNumber;
		Parse(appCmdLine(), TEXT("TFCSUFFIX="), childProcessNumber);

		FString cacheNameW =
			appGameDir() + TEXT("SubstanceAir") + PATH_SEPARATOR +
			TEXT("substance.cache") + childProcessNumber;

		SubstanceAirTextureCacheWriter = new atc_api::Writer((const wchar_t *)*cacheNameW, false);
	}
#endif
}

/**
 *	Create an instance of the persistent cooker data given a filename. 
 *	First try to load from disk and if not found will construct object and store the 
 *	filename for later use during saving.
 *
 *	The cooker will call this first, and if it returns NULL, it will use the standard
 *		UPersistentCookerData::CreateInstance function. 
 *	(They are static hence the need for this)
 *
 * @param	Filename					Filename to use for serialization
 * @param	bCreateIfNotFoundOnDisk		If FALSE, don't create if couldn't be found; return NULL.
 * @return								instance of the container associated with the filename
 */
UPersistentCookerData* FGameCookerHelper::CreateInstance( const TCHAR* Filename, UBOOL bCreateIfNotFoundOnDisk )
{
	return FGameCookerHelperBase::CreateInstance(Filename,bCreateIfNotFoundOnDisk);
}

/** 
 *	Generate the package list that is specific for the game being cooked.
 *
 *	@param	Commandlet							The cookpackages commandlet being run
 *	@param	Platform							The platform being cooked for
 *	@param	ShaderPlatform						The shader platform being cooked for
 *	@param	NotRequiredFilenamePairs			The package lists being filled in...
 *	@param	RegularFilenamePairs				""
 *	@param	MapFilenamePairs					""
 *	@param	ScriptFilenamePairs					""
 *	@param	StartupFilenamePairs				""
 *	@param	StandaloneSeekfreeFilenamePairs		""
 *	
 *	@return	UBOOL		TRUE if successful, FALSE is something went wrong.
 */
UBOOL FGameCookerHelper::GeneratePackageList( 
	class UCookPackagesCommandlet* Commandlet, 
	UE3::EPlatformType Platform,
	EShaderPlatform ShaderPlatform,
	TArray<FPackageCookerInfo>& NotRequiredFilenamePairs,
	TArray<FPackageCookerInfo>& RegularFilenamePairs,
	TArray<FPackageCookerInfo>& MapFilenamePairs,
	TArray<FPackageCookerInfo>& ScriptFilenamePairs,
	TArray<FPackageCookerInfo>& StartupFilenamePairs,
	TArray<FPackageCookerInfo>& StandaloneSeekfreeFilenamePairs)
{
	UBOOL bSuccess=TRUE;

	// Add to list of seekfree packages needed for standalone game content
	if (GetGameContentSeekfreeHelper()->GeneratePackageList(
		Commandlet,
		Platform,
		ShaderPlatform,
		NotRequiredFilenamePairs,
		RegularFilenamePairs,
		MapFilenamePairs,
		ScriptFilenamePairs,
		StartupFilenamePairs,
		StandaloneSeekfreeFilenamePairs
		) == FALSE)
	{
		bSuccess = FALSE;
	}

	if (GetStreamingLevelCookerHelper()->GeneratePackageList(
		Commandlet,
		Platform,
		ShaderPlatform,
		NotRequiredFilenamePairs,
		RegularFilenamePairs,
		MapFilenamePairs,
		ScriptFilenamePairs,
		StartupFilenamePairs,
		StandaloneSeekfreeFilenamePairs
		) == FALSE)
	{
		bSuccess = FALSE;
	}
	return bSuccess;
}

/**
 * Cooks passed in object if it hasn't been already.
 *
 *	@param	Commandlet					The cookpackages commandlet being run
 *	@param	Package						Package going to be saved
 *	@param	Object						Object to cook
 *	@param	bIsSavedInSeekFreePackage	Whether object is going to be saved into a seekfree package
 *
 *	@return	UBOOL						TRUE if the object should continue the 'normal' cooking operations.
 *										FALSE if the object should not be processed any further.
 */
UBOOL FGameCookerHelper::CookObject(class UCookPackagesCommandlet* Commandlet, UPackage* Package, UObject* Object, UBOOL bIsSavedInSeekFreePackage)
{
	UBOOL bContinueProcessing = TRUE;

	// pre process loaded package for standalone seekfree game content
	if (GetGameContentSeekfreeHelper()->CookObject(Commandlet,Package,Object,bIsSavedInSeekFreePackage) == FALSE)
	{
		bContinueProcessing = FALSE;
	}

	if (bContinueProcessing && (GetForcedContentHelper()->CookObject(Commandlet,Package,Object,bIsSavedInSeekFreePackage) == FALSE))
	{
		bContinueProcessing = FALSE;
	}

	if (bContinueProcessing && (GetStreamingLevelCookerHelper()->CookObject(Commandlet, Package, Object, bIsSavedInSeekFreePackage) == FALSE))
	{
		bContinueProcessing = FALSE;
	}

	if (bContinueProcessing)
	{
#if WITH_SUBSTANCE_AIR	
		if( (GCookingTarget & UE3::PLATFORM_AnyWindows) != 0 )
		{
			USubstanceAirTexture2D* Texture = Cast<USubstanceAirTexture2D>(Object);
			if (Texture)
			{
				CookSubstanceTexture(Texture);

				// do not return FALSE as we do not actually process
				// the texture, but just gather some information
				return TRUE;
			}
			else
			{
				USubstanceAirGraphInstance* Instance = Cast<USubstanceAirGraphInstance>(Object);
				if (Instance)
				{
					CookSubstanceAirGraphInstance(
						Commandlet, Package, Instance, bIsSavedInSeekFreePackage);
					return FALSE;
				}
				else
				{
					USubstanceAirInstanceFactory* InstanceFactory = 
						Cast<USubstanceAirInstanceFactory>(Object);

					if (InstanceFactory)
					{
						CookSubstanceAirInstanceFactory(
							Commandlet, Package, InstanceFactory, bIsSavedInSeekFreePackage);
						return FALSE;
					}
				}
			}
		}
		else
		{
			if( Object->GetOutermost()->GetName() == TEXT("SubstanceAir") || Object->GetClass()->GetOutermost()->GetName() == TEXT("SubstanceAir") )
			{
				Object->ClearFlags(RF_ForceTagExp);
				Object->SetFlags(RF_MarkedByCooker|RF_Transient);
				UObject* MarkOuter = Object->GetOuter();
				while (MarkOuter)
				{
					MarkOuter->ClearFlags(RF_ForceTagExp);
					MarkOuter->SetFlags(RF_MarkedByCooker);
					MarkOuter = MarkOuter->GetOuter();
				}

				warnf( TEXT("Substance asset (%s) is not supported on this platform"), *Object->GetPathName() );
				return FALSE;
			}
		}
#endif
	}

	return bContinueProcessing;
}

#if WITH_SUBSTANCE_AIR == 1
void FGameCookerHelper::CookSubstanceTexture(USubstanceAirTexture2D* Texture)
{
	if (GUseSubstanceInstallTimeCache)
	{
		USubstanceAirTexture2D* AirTexture = Cast<USubstanceAirTexture2D>(Texture);	
		USubstanceAirGraphInstance* Instance = Cast<USubstanceAirGraphInstance>(AirTexture->ParentInstance);

		FString TextureName;
		AirTexture->GetFullName().ToLower().Split(TEXT(" "), NULL, &TextureName);
		debugf(TEXT("Texture:\t%s"), *TextureName);

		if (!Instance)
		{
			debugf(TEXT("Invalid texture, no parent graph instance available, skipping."));
			return;
		}

		FString InstanceName;
		Instance->GetFullName().ToLower().Split(" ", NULL, &InstanceName);

		FString PackageName;
		Instance->Parent->GetFullName().ToLower().Split(" ", NULL, &PackageName);

		debugf(TEXT("\t\t\t\t%s"), *PackageName);
		debugf(TEXT("\t\t\t\t%s"), *InstanceName);

		FTextureLODSettings PlatformLODSettings = GSystemSettings.TextureLODSettings;

		// Take texture LOD settings into account, avoiding cooking 
		// and keeping memory around for unused miplevels.
		INT FirstMipIndex = Clamp<INT>( 
			PlatformLODSettings.CalculateLODBias( Texture ) - Texture->NumCinematicMipLevels,
			0,
			Texture->Mips.Num()-1 );

		// make sure we load at least the first packed mip level
		FirstMipIndex = Min(FirstMipIndex, Texture->MipTailBaseIdx);

		if (FirstMipIndex < 0)
		{
			// issue with mip will be handled by cooker and an error message displayed
			return;
		}

		atc_api::MipDataArray mipDataArray;
		for (INT MipIndex=FirstMipIndex; MipIndex<Texture->Mips.Num(); MipIndex++)
		{
			FTexture2DMipMap& Mip = Texture->Mips(MipIndex);
			SIZE_T dataSize = Mip.Data.GetBulkDataSize();
			void * dataPtr = Mip.Data.Lock(LOCK_READ_WRITE);
			mipDataArray.push_back(atc_api::MipData(dataPtr, dataSize));
		}

		// Write texture
		bool textureAdded = SubstanceAirTextureCacheWriter->addTexture(
			TCHAR_TO_UTF8(*TextureName),
			AirTexture->OutputCopy->Uid,
			Texture->Mips(FirstMipIndex).SizeX,
			Texture->Mips(FirstMipIndex).SizeY,
			(atc_api::TextureFormat)Texture->Format,
			mipDataArray,
			TCHAR_TO_UTF8(*InstanceName),
			TCHAR_TO_UTF8(*PackageName));

		if (textureAdded)
		{
			for (INT MipIndex=FirstMipIndex ; MipIndex < Texture->Mips.Num() ; MipIndex++)
			{
				appMemzero((void *)mipDataArray[MipIndex].data(), mipDataArray[MipIndex].size());
			}
		}

		// Unlock memory
		for (INT MipIndex=FirstMipIndex; MipIndex<Texture->Mips.Num(); MipIndex++)
		{
			Texture->Mips(MipIndex).Data.Unlock();
		}
	}
	else
	{
		UBOOL bForceTextureBaking = FALSE;
		GConfig->GetBool(TEXT("SubstanceAir"), TEXT("bForceTextureBaking"), bForceTextureBaking, GEngineIni);

		if (FALSE == bForceTextureBaking)
		{
			// Get the number of mips to keep from the engine
			// configuration file, in the SubstanceAir section

			INT MipsToKeep = SBS_DFT_COOKED_MIPS_NB;
			GConfig->GetInt(TEXT("SubstanceAir"), TEXT("MipCountAfterCooking"), MipsToKeep, GEngineIni);

			INT RemovedSize_bytes = 0;
			INT KeptSize_bytes = 0;

			for (INT Idx = 0 ; Idx < Texture->Mips.Num() ; ++Idx)
			{
				if (Idx < Texture->Mips.Num()-MipsToKeep)
				{
					RemovedSize_bytes += Texture->Mips(Idx).Data.GetBulkDataSizeOnDisk();
				}
				else
				{
					KeptSize_bytes += Texture->Mips(Idx).Data.GetBulkDataSizeOnDisk();
				}
			}

			SubstanceAirHelper_mipsRemoved_Kb += RemovedSize_bytes / 1024.0f;
			SubstanceAirHelper_mipsKept_Kb += KeptSize_bytes / 1024.0f;
		}
		else if (Texture->OutputCopy)
		{
			Texture->OutputCopy->GetParentGraphInstance()->bIsBaked = TRUE;
		}
	}
}


void FGameCookerHelper::CookSubstanceAirInstanceFactory(
	class UCookPackagesCommandlet* Commandlet,
	UPackage* Package,
	USubstanceAirInstanceFactory* Factory,
	UBOOL bIsSavedInSeekFreePackage)
{
	if (GUseSubstanceInstallTimeCache)
	{
		FString FactoryName;
		Factory->GetFullName().ToLower().Split(" ", NULL, &FactoryName);

		SubstanceAir::Details::LinkDataAssembly *LinkData = 
			static_cast<SubstanceAir::Details::LinkDataAssembly*>(
			Factory->SubstancePackage->LinkData.get());

		debugf(TEXT("Package:\t%s"), *FactoryName);

		UBOOL res = SubstanceAirTextureCacheWriter->addSubstancePackage(
			TCHAR_TO_UTF8(*FactoryName), 
			&LinkData->getAssembly()[0],
			LinkData->getAssembly().size()) ? TRUE : FALSE;

		// the sbsar is now stored in the texture cache, we can erase it.
		Factory->SubstancePackage->LinkData->clear();
	}
	else
	{
		if (Factory->SubstancePackage)
		{
			SubstanceAirHelper_sbsasrTotal_Kb +=
				Factory->SubstancePackage->LinkData->getSize() / 1024.0f;
		}

		UBOOL bNoRuntimeGeneration = FALSE;
		GConfig->GetBool(TEXT("SubstanceAir"), TEXT("bNoRuntimeGeneration"), bNoRuntimeGeneration, GEngineIni);

		if (bNoRuntimeGeneration && Factory && Factory->SubstancePackage)
		{
			Factory->SubstancePackage->LinkData->clear();
		}
	}	
}

void FGameCookerHelper::CookSubstanceAirGraphInstance(
	class UCookPackagesCommandlet* Commandlet,
	UPackage* Package,
	class USubstanceAirGraphInstance* Instance,
	UBOOL bIsSavedInSeekFreePackage )
{
	if (GUseSubstanceInstallTimeCache)
	{
		preset_t Preset;
		Preset.ReadFrom(Instance->Instance);

		FString PresetContent;
		SubstanceAir::WritePreset(Preset, PresetContent);

		FString InstanceName;
		Instance->GetFullName().ToLower().Split(" ", NULL, &InstanceName);

		FString PackageName;
		Instance->Parent->GetFullName().ToLower().Split(" ", NULL, &PackageName);

		UBOOL res = SubstanceAirTextureCacheWriter->addSubstanceInstance(
			TCHAR_TO_UTF8(*InstanceName),
			TCHAR_TO_UTF8(*PackageName),
			TCHAR_TO_UTF8(*PresetContent)) ? TRUE : FALSE;

		SubstanceAir::List<std::tr1::shared_ptr<input_inst_t>>::TIterator ItInputs(Instance->Instance->Inputs.itfront());

		for (; ItInputs ; ++ItInputs)
		{
			if ((*ItInputs)->IsNumerical() == FALSE)
			{
				UObject* ImageSource = static_cast<img_input_inst_t*>((*ItInputs).get())->ImageSource;

				if (ImageSource)
				{
					ImageSource->ConditionalPostLoad();

					std::tr1::shared_ptr<SubstanceAir::ImageInput> ImageInputContent = 
						SubstanceAir::Helpers::PrepareImageInput(
						ImageSource,
						static_cast<img_input_inst_t*>((*ItInputs).get()), 
						Instance->Instance);

					if (ImageInputContent.get())
					{
						FString ImageSourceName;
						ImageSource->GetFullName().ToLower().Split(" ", NULL, &ImageSourceName);

						SubstanceAir::ImageInput::ScopedAccess SA(ImageInputContent);

						UBOOL res = SubstanceAirTextureCacheWriter->addSubstanceImageInput(
							TCHAR_TO_UTF8(*ImageSourceName),
							atc_api::MipData(
							SA->buffer,
							SA.getSize()));
					}
				}
			}
		}
	}
}
#endif // WITH_SUBSTANCE_AIR == 1


/** 
 *	PreLoadPackageForCookingCallback
 *	This function will be called in LoadPackageForCooking, prior to the actual loading of the package.
 *	It is intended for pre-loading any required packages, etc.
 *
 *	@param	Commandlet		The cookpackages commandlet being run
 *	@param	Filename		The name of the package to load.
 *
 *	@return	UBOOL		TRUE if the package should be processed further.
 *						FALSE if the cook of this package should be aborted.
 */
UBOOL FGameCookerHelper::PreLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, const TCHAR* Filename)
{
	UBOOL bContinueProcessing = TRUE;

	// pre process loaded package for standalone seekfree game content
	if (GetGameContentSeekfreeHelper()->PreLoadPackageForCookingCallback(Commandlet,Filename) == FALSE)
	{
		bContinueProcessing = FALSE;
	}

	if (bContinueProcessing && (GetForcedContentHelper()->PreLoadPackageForCookingCallback(Commandlet,Filename) == FALSE))
	{
		bContinueProcessing = FALSE;
	}

	if (bContinueProcessing && (GetStreamingLevelCookerHelper()->PreLoadPackageForCookingCallback(Commandlet, Filename) == FALSE))
	{
		bContinueProcessing = FALSE;
	}

	return bContinueProcessing;
}

/** 
 *	LoadPackageForCookingCallback
 *	This function will be called in LoadPackageForCooking, allowing the cooker
 *	helper to handle the package creation as they wish.
 *
 *	@param	Commandlet		The cookpackages commandlet being run
 *	@param	Filename		The name of the package to load.
 *
 *	@return	UPackage*		The package generated/loaded
 *							NULL if the commandlet should load the package normally.
 */
UPackage* FGameCookerHelper::LoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, const TCHAR* Filename)
{
	UPackage* Result=NULL;
	
	// load package for standalone seekfree game content for the given filename
	Result = GetGameContentSeekfreeHelper()->LoadPackageForCookingCallback(Commandlet,Filename);

	return Result;
}

/** 
 *	PostLoadPackageForCookingCallback
 *	This function will be called in LoadPackageForCooking, prior to any
 *	operations occurring on the contents...
 *
 *	@param	Commandlet	The cookpackages commandlet being run
 *	@param	Package		The package just loaded.
 *
 *	@return	UBOOL		TRUE if the package should be processed further.
 *						FALSE if the cook of this package should be aborted.
 */
UBOOL FGameCookerHelper::PostLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, UPackage* InPackage)
{
	UBOOL bContinueProcessing = TRUE;

	// post process loaded package for standalone seekfree game content
	if (GetGameContentSeekfreeHelper()->PostLoadPackageForCookingCallback(Commandlet,InPackage) == FALSE)
	{
		bContinueProcessing = FALSE;
	}

	if (bContinueProcessing && (GetForcedContentHelper()->PostLoadPackageForCookingCallback(Commandlet,InPackage) == FALSE))
	{
		bContinueProcessing = FALSE;
	}

	if (bContinueProcessing && (GetStreamingLevelCookerHelper()->PostLoadPackageForCookingCallback(Commandlet, InPackage) == FALSE))
	{
		bContinueProcessing = FALSE;
	}
#if WITH_SUBSTANCE_AIR == 1
	CurrentPackageContainsMap = InPackage->ContainsMap();
#endif

	return bContinueProcessing;
}

/**
 *	Clean up the kismet for the given level...
 *	Remove 'danglers' - sequences that don't actually hook up to anything, etc.
 *
 *	@param	Commandlet	The cookpackages commandlet being run
 *	@param	Package		The package being cooked.
 */
void FGameCookerHelper::CleanupKismet(class UCookPackagesCommandlet* Commandlet, UPackage* InPackage)
{
}

/**
 *	Return TRUE if the sound cue should be ignored when generating persistent FaceFX list.
 *
 *	@param	Commandlet		The commandlet being run
 *	@param	InSoundCue		The sound cue of interest
 *
 *	@return	UBOOL			TRUE if the sound cue should be ignored, FALSE if not
 */
UBOOL FGameCookerHelper::ShouldSoundCueBeIgnoredForPersistentFaceFX(class UCookPackagesCommandlet* Commandlet, const USoundCue* InSoundCue)
{
	return FALSE;
}

/**
 *	Called when the FPersistentMapInfo class has been initialized and filled in.
 *	Can be used to do any special setup for PMaps the cooker may require.
 *
 *	@param	Commandlet		The commandlet that is being run.
 */
void FGameCookerHelper::PersistentMapInfoGeneratedCallback(class UCookPackagesCommandlet* Commandlet)
{
	// Allow the seekfree helper to do what it needs
	GetGameContentSeekfreeHelper()->PersistentMapInfoGeneratedCallback(Commandlet);
	GetForcedContentHelper()->PersistentMapInfoGeneratedCallback(Commandlet);
}

/**
 *	Initialize any special information for a cooked shader cache, such as priority
 *
 *	@param	Commandlet			The cookpackages commandlet being run
 *	@param	Package				The package being cooked.
 *	@param	ShaderCache			Shader cache object that is being initialize
 *	@param	IsStartupPackage	If true, this is a package that is loaded on startup
 */
void FGameCookerHelper::InitializeShaderCache(class UCookPackagesCommandlet* Commandlet, UPackage* InPackage, UShaderCache *InShaderCache, UBOOL bIsStartupPackage)
{	
	int Priority = -1;

	if (bIsStartupPackage)
	{
		// Startup packages get best priority
		Priority = 0;
	}

	if (Priority == -1)
	{
		// Check if we're a game content common package
		if (GetGameContentSeekfreeHelper()->IsPackageGameCommonContent(InPackage))
		{
			Priority = 1;
		}
	}

	if (Priority == -1)
	{
		// Check if we're a game content common package
		if (GetGameContentSeekfreeHelper()->IsPackageGameContent(InPackage))
		{
			Priority = 2;
		}
	}

	if (Priority == -1)
	{	
		FString CheckName = InPackage->GetName();
		INT SFIndex = CheckName.InStr(TEXT("_SF"));
		if (SFIndex != -1)
		{
			CheckName = CheckName.Left(SFIndex);
		}
		
		FString PMapSource;
		if (Commandlet->PersistentMapInfoHelper.GetPersistentMapAlias(CheckName, PMapSource) == TRUE)
		{
			// Copy the alias in
			CheckName = PMapSource;
		}	
		if (Commandlet->PersistentMapInfoHelper.GetPersistentMapForLevel(CheckName, PMapSource) == TRUE)
		{
			if (CheckName == PMapSource)
			{
				// We're a persistent map
				Priority = 3;
			}
		}
	}

	if (Priority == -1)
	{
		// Default priority of 10
		Priority = 10;
	}
	
	InShaderCache->SetPriority(Priority);
}

/**
 *	Tag any cooked startup objects that should not go into subsequent packages.
 *	
 *	@param	Commandlet			The cookpackages commandlet being run
 */
void FGameCookerHelper::TagCookedStartupObjects(class UCookPackagesCommandlet* Commandlet)
{
}

/**
 *	Dump out stats specific to the game cooker helper.
 */
void FGameCookerHelper::DumpStats()
{
	warnf( NAME_Log, TEXT("") );
	warnf( NAME_Log, TEXT("Game-specific Cooking Stats:") );
	GetGameContentSeekfreeHelper()->DumpStats();
	GetForcedContentHelper()->DumpStats();
	GetStreamingLevelCookerHelper()->DumpStats();
}

#if WITH_SUBSTANCE_AIR == 1
void FGameCookerHelper::Cleanup()
{
	if (GUseSubstanceInstallTimeCache)
	{
		check(SubstanceAirTextureCacheWriter);
		delete SubstanceAirTextureCacheWriter;
		SubstanceAirTextureCacheWriter = NULL;
	}
}
#endif
