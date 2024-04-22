/*=============================================================================
	UDatabaseCommandlets.cpp: Commandlets that derive from BaseDatabase
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

/*-----------------------------------------------------------------------------
	BaseDatabase commandlet.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UBaseDatabaseCommandlet);

/**
 *	Startup the commandlet.
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UBaseDatabaseCommandlet::Startup()
{
	// Find the Database and Catalog tokens
	const TCHAR* DatabaseTag = TEXT("DATABASE=");
	const TCHAR* CatalogTag = TEXT("CATALOG=");
	// Optional Timeout token for using a different value than default (3)
	const TCHAR* TimeoutTag = TEXT("TIMEOUT=");

	ConnectionTimeout = 3;
	for (INT SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++)
	{
		FString CurrSwitch = Switches(SwitchIdx);
		INT DatabaseIndex = CurrSwitch.InStr(DatabaseTag, FALSE, TRUE);
		INT CatalogIndex = CurrSwitch.InStr(CatalogTag, FALSE, TRUE);
		INT TimeoutIndex = CurrSwitch.InStr(TimeoutTag, FALSE, TRUE);

		if (DatabaseIndex != INDEX_NONE)
		{
			DatabaseName = CurrSwitch.Right(CurrSwitch.Len() - appStrlen(DatabaseTag));
		}
		if (CatalogIndex != INDEX_NONE)
		{
			CatalogName = CurrSwitch.Right(CurrSwitch.Len() - appStrlen(CatalogTag));
		}
		if (TimeoutIndex != INDEX_NONE)
		{
			FString ConnectionTimeoutStr = CurrSwitch.Right(CurrSwitch.Len() - appStrlen(CatalogTag));
			ConnectionTimeout = Max<INT>(3,appAtoi(*ConnectionTimeoutStr));
		}
	}

	// Verify the database and catalog were found...
	if (DatabaseName.Len() == 0)
	{
		warnf(NAME_Warning, TEXT("Use -DATABASE= to specify database."));
		return FALSE;
	}
	if (CatalogName.Len() == 0)
	{
		warnf(NAME_Warning, TEXT("Use -CATALOG= to specify catalog."));
		return FALSE;
	}

	// Look for VERBOSE switch
	INT DummyIndex;
	bVerbose = Switches.FindItem(TEXT("VERBOSE"), DummyIndex);
	if (bVerbose == TRUE)
	{
		warnf(NAME_Log, TEXT("Running in verbose mode..."));
	}

	bIgnoreColon = Switches.FindItem(TEXT("IGNORECOLON"), DummyIndex);
	if (bIgnoreColon == TRUE)
	{
		warnf(NAME_Log, TEXT("Ignoring objects w/ ':' in their name..."));
	}

	return TRUE;
}

/**
 *	Shutdown the commandlet
 */
void UBaseDatabaseCommandlet::Shutdown()
{
	// Close up the database
	CloseDatabaseConnection();
}

/**
 *	Allow the commandlet to do any post-startup initialization
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UBaseDatabaseCommandlet::Initialize()
{
	// Gather a list of map packages
	DOUBLE StartPackageVettingTime = appSeconds();
	{
		TArray<FString> FilesInPath = GPackageFileCache->GetPackageFileList();
		for (INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++)
		{
			const FFilename & Filename = FilesInPath(FileIndex);
			FString ShortName = Filename.GetBaseFilename().ToUpper();
			if (ProcessedPackages.Find(ShortName) == NULL)
			{
				// Get the package linker to see whether we have a map file. We can't go by the extension as
				// e.g. the entry level in Engine has a .upkg extension.
				UObject::BeginLoad();
				ULinkerLoad* Linker = UObject::GetPackageLinker(NULL,*Filename,LOAD_NoVerify,NULL,NULL);
				UObject::EndLoad();
				if (Linker != NULL)
				{
					if ((Linker->Summary.PackageFlags & PKG_ContainsMap) == PKG_ContainsMap)
					{
						MapPackages.Set(ShortName, TRUE);
					}
					else if ((Linker->Summary.PackageFlags & PKG_ContainsScript) == PKG_ContainsScript)
					{
						ScriptPackages.Set(ShortName, TRUE);
					}
					else
					{
						OtherPackages.Set(ShortName, TRUE);
					}
				}
				else
				{
					LinkerNotFoundPackages.Set(ShortName, TRUE);
				}

				ProcessedPackages.Set(ShortName, TRUE);
			}
		}	
	}
	DOUBLE PackageVettingTime = appSeconds() - StartPackageVettingTime;
	warnf(NAME_Log, TEXT("Took %5.3f seconds to review full package list..."), PackageVettingTime);

	return TRUE;
}

/**
 *	Open the connection to the database
 *
 *	@param	UBOOL	TRUE if database connection was opened, FALSE if not
 */
UBOOL UBaseDatabaseCommandlet::OpenDatabaseConnection(const TCHAR* InDatabase, const TCHAR* InCatalog, INT InConnectionTimeout)
{
	// Create the connection object; needs to be deleted via "delete".
	DBConnection = FDataBaseConnection::CreateObject();
	check(DBConnection);

	// Create the connection string with Windows Authentication as the way to handle permissions/ login/ security.
	FString ConnectionString = FString::Printf(TEXT("Provider=sqloledb;Data Source=%s;Initial Catalog=%s;Trusted_Connection=Yes;Connection Timeout=%d"), 
		InDatabase, InCatalog, InConnectionTimeout);

	// Try to open connection to DB - this is a synchronous operation.
	if (DBConnection->Open(*ConnectionString, NULL, NULL) == FALSE)
	{
		warnf(NAME_Error,TEXT("Connection to %s.%s FAILED"), InDatabase, InCatalog);
		delete DBConnection;
		DBConnection = NULL;
		return FALSE;
	}

	warnf(NAME_Log,TEXT("Connection to %s.%s succeeded"), InDatabase, InCatalog);
	return TRUE;
}

/**
 *	Close the connection to the database
 *
 *	@param	UBOOL	TRUE if successfully closed, FALSE if not
 */
UBOOL UBaseDatabaseCommandlet::CloseDatabaseConnection()
{
	if (DBConnection != NULL)
	{
		warnf(NAME_Log,TEXT("Closing connection to %s.%s..."), *DatabaseName, *CatalogName);
		DBConnection->Close();
		delete DBConnection;
		DBConnection = NULL;
	}
	return TRUE;
}

/**
 *	Mine the given database for the objects of interest
 *
 *	@param	UBOOL	TRUE if database connection was opened, FALSE if not
 */
UBOOL UBaseDatabaseCommandlet::MineDatabaseForObjects()
{
	// If we don't have a DB connection, we are doomed...
	if (DBConnection == NULL)
	{
		warnf(NAME_Error, TEXT("Invalid database connection..."));
		return FALSE;
	}

	// If we don't have any classes of interest, we have nothing to mine
	if (ClassesOfInterest.Num() == 0)
	{
		warnf(NAME_Error, TEXT("Empty ClassesOfInterest array..."));
		return FALSE;
	}

	if (GatherCookedAssetsInfo(ClassesOfInterest, CookedObjects, bIgnoreColon, NULL, NULL) == FALSE)
	{
		warnf(NAME_Error, TEXT("Failed to gather cooked object asset list..."));
		return FALSE;
	}
	
	return TRUE;
}

/**
 *	The main function of the commandlet...
 *	Derived commandlets should implement their functionality here
 */
UBOOL UBaseDatabaseCommandlet::ProcessMinedData()
{
	return TRUE;
}

/** Dump the results of the commandlet run */
void UBaseDatabaseCommandlet::DumpResults()
{
}

/**
 *	Generate a list of all the PMaps for the game.
 *
 *	@param	OutPMapList		The list of found PMaps
 *
 *	@return	UBOOL			TRUE if successful, FALSE if not
 */
UBOOL UBaseDatabaseCommandlet::GeneratePMapList(TArray<FString>& OutPMapList)
{
	TArray<FString> FilesInPath;

	// See if any of the tokens is a map...
	if (Tokens.Num() > 0)
	{
		ValidateMapList(Tokens, FilesInPath);
	}

	if (FilesInPath.Num() == 0)
	{
		// Check for a MAPINISECTION entry...
		FString MapIniSection = TEXT("");
		const TCHAR* MapIniTag = TEXT("MAPINISECTION=");
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
			ValidateMapList(IniMapList, FilesInPath);
		}

		if (FilesInPath.Num() == 0)
		{
			warnf(NAME_Log, TEXT("Using AllPackages as the file list!"));
			FilesInPath = GPackageFileCache->GetPackageFileList();
		}
	}

	if (FilesInPath.Num() == 0)
	{
		warnf(NAME_Error, TEXT("Failed to find any map files..."));
		return FALSE;
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

		// Add it to the list!
		LocalPackageList.AddItem(Filename);
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
				OutPMapList.AddItem(LocalFilename);
				break;
			}
		}
	}
	return TRUE;
}

/**
 *	Validate the given list of map names as real map packages
 *
 *	@param	InMapList			The list of maps to validate
 *	@param	OutValidatedList	Valid maps from the list
 */
void UBaseDatabaseCommandlet::ValidateMapList(const TArray<FString>& InMapList, TArray<FString>& OutValidatedList)
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

/**
 *	Helper function for getting an output file to dump results to...
 *	Caller is expected to close/delete the file!
 *
 *	@param	InFolderName		The name of the folder under <UE3>\<GAME>\Logs to create the file in
 *	@param	InShortFilename		The name of the file to open
 *	
 *	@return	FArchive*			The file stream if successful, NULL if not
 */
FArchive* UBaseDatabaseCommandlet::GetOutputFile(const TCHAR* InFolderName, const TCHAR* InShortFilename)
{
	// Place in the <UE3>\<GAME>\Logs\<InFolderName> folder
	FString Filename = FString::Printf(TEXT("%sLogs%s%s%s%s-%s.csv"),*appGameDir(),PATH_SEPARATOR,InFolderName,
		PATH_SEPARATOR,InShortFilename,*appSystemTimeString());

	FArchive* OutputStream = GFileManager->CreateDebugFileWriter(*Filename);
	if (OutputStream == NULL)
	{
		warnf(NAME_Warning, TEXT("Failed to create output stream %s"), *Filename);
	}
	return OutputStream;
}

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
UBOOL UBaseDatabaseCommandlet::GatherCookedAssetInfo(FString& InClassName, 
	TMap<FString,FCookedObjectInfo >& OutCookedInfoMap, UBOOL bInIgnoreObjsWithColonInName, 
	TMap<FString,FPackageCookedObjectInfo>* OutPackageToCookedObjectsMap, TMap<FString,UBOOL>* OutCookedPackages)
{
	TArray<FString> ClassNames;
	ClassNames.AddItem(InClassName);

	return GatherCookedAssetsInfo(ClassNames, OutCookedInfoMap, bInIgnoreObjsWithColonInName, OutPackageToCookedObjectsMap, OutCookedPackages);
}

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
UBOOL UBaseDatabaseCommandlet::GatherCookedAssetsInfo(TArray<FString>& InClassNames, 
	TMap<FString,FCookedObjectInfo >& OutCookedInfoMap, UBOOL bInIgnoreObjsWithColonInName, 
	TMap<FString,FPackageCookedObjectInfo>* OutPackageToCookedObjectsMap, TMap<FString,UBOOL>* OutCookedPackages)
{
	if (DBConnection == NULL)
	{
		return FALSE;
	}

	if (InClassNames.Num() == 0)
	{
		warnf(NAME_Error, TEXT("No class names specified for gathering cooked assets!"));
		return FALSE;
	}

	warnf(NAME_Log, TEXT("Gathering specified cooked assets from %s.%s..."), *DatabaseName, *CatalogName);
	for (INT ClassDumpidx = 0; ClassDumpidx < InClassNames.Num(); ClassDumpidx++)
	{
		warnf(NAME_Log, TEXT("\t%s"), *(InClassNames(ClassDumpidx)));
	}

	DOUBLE StartGatherTime = appSeconds();
	// Gather the list of objects for the given classes that are cooked
	FString PrefixCommand = TEXT("-- Region Parameters\n");

	FString TypeCommand;
	for (INT ClassIdx = 0; ClassIdx < InClassNames.Num(); ClassIdx++)
	{
		FString ClassName = InClassNames(ClassIdx);
		TypeCommand += FString::Printf(TEXT("DECLARE @p%d VarChar(%d) = '%s'\n"), ClassIdx, ClassName.Len(), *ClassName);
	}

	FString InnerCommand=	TEXT("-- EndRegion\n");
	InnerCommand +=			TEXT("SELECT [t6].[value] AS [PackageName], [t6].[value3] AS [ClassName], [t6].[value2] AS [ObjectName], [t6].[Size]\n");
	InnerCommand +=			TEXT("FROM (\n");
	InnerCommand +=			TEXT("	SELECT [t5].[PackageID], [t5].[value], [t5].[PackageID2], [t5].[Size], [t5].[value2], LTRIM(RTRIM([t5].[ClassName])) AS [value3], [t5].[ClassName], [t5].[ObjectName]\n");
	InnerCommand +=			TEXT("FROM (\n");
	InnerCommand +=			TEXT("	SELECT [t1].[PackageID], [t1].[value], [t2].[PackageID] AS [PackageID2], [t2].[Size], LTRIM(RTRIM([t3].[ObjectName])) AS [value2], [t4].[ClassName], [t3].[ObjectName]\n");
	InnerCommand +=			TEXT("FROM (\n");
	InnerCommand +=			TEXT("	SELECT [t0].[PackageID], LTRIM(RTRIM([t0].[PackageName])) AS [value]\n");
	InnerCommand +=			TEXT("FROM [Packages] AS [t0]\n");
	InnerCommand +=			TEXT(") AS [t1]\n");
	InnerCommand +=			TEXT("CROSS JOIN [Exports] AS [t2]\n");
	InnerCommand +=			TEXT("INNER JOIN [Objects] AS [t3] ON [t3].[ObjectID] = [t2].[ObjectID]\n");
	InnerCommand +=			TEXT("INNER JOIN [Classes] AS [t4] ON [t4].[ClassID] = [t3].[ClassID]\n");
	InnerCommand +=			TEXT(") AS [t5]\n");
	InnerCommand +=			TEXT(") AS [t6]\n");

	FString WhereCommand = TEXT("WHERE (");
	for (INT ClassIdx = 0; ClassIdx < InClassNames.Num(); ClassIdx++)
	{
		if (ClassIdx != 0)
		{
			WhereCommand += FString::Printf(TEXT(" OR ([t6].[ClassName] = @p%d)"), ClassIdx);
		}
		else
		{
			WhereCommand += FString::Printf(TEXT("([t6].[ClassName] = @p%d)"), ClassIdx);
		}
	}
	WhereCommand += FString::Printf(TEXT(") AND ([t6].[PackageID2] = [t6].[PackageID])\n"));
	WhereCommand += FString::Printf(TEXT("	ORDER BY [t6].[value], [t6].[ObjectName]"));

	FString SQLCommand = PrefixCommand + TypeCommand + InnerCommand + WhereCommand;

	if (ExecuteSQLQuery(SQLCommand, OutCookedInfoMap, bInIgnoreObjsWithColonInName, OutPackageToCookedObjectsMap, OutCookedPackages) == FALSE)
	{
		warnf(NAME_Warning, TEXT("Failed to execute SQL Query..."));
		return FALSE;
	}

	DOUBLE TotalGatherTime = appSeconds() - StartGatherTime;
	warnf(NAME_Log, TEXT("COMPLETED in %5.3f seconds."), TotalGatherTime);

	return TRUE;
}

/**
 *	Gather ALL cooked assets
 *
 *	@param	OutCookedInfoMap				The assets --> cookingobjectinfo map to fill in
 *	@param	bInIgnoreObjsWithColonInName	If TRUE, ignore objects with ':' in their name (properties, etc.)
 *	@param	OutPackageToCookedObjectsMap	Optional map of packages --> objects contains to fill in
 *	@param	OutCookedPackages				Optional map of cooked packages to fill in
 *
 *	@return	UBOOL					TRUE if successful, FALSE if not
 */
UBOOL UBaseDatabaseCommandlet::GatherAllCookedAssetInfo(TMap<FString,FCookedObjectInfo >& OutCookedInfoMap, 
	UBOOL bInIgnoreObjsWithColonInName, TMap<FString,FPackageCookedObjectInfo>* OutPackageToCookedObjectsMap, 
	TMap<FString,UBOOL>* OutCookedPackages)
{
	if (DBConnection == NULL)
	{
		return FALSE;
	}

	warnf(NAME_Log, TEXT("Gathering all cooked assets from %s.%s..."), *DatabaseName, *CatalogName);

	DOUBLE StartGatherTime = appSeconds();

	// Retrieve all objects [class name, serialize size, and object name] cooked into each package.
	FString SQLCommand	 = TEXT("SELECT [t6].[value] AS [PackageName], [t6].[value3] AS [ClassName], [t6].[value2] AS [ObjectName], [t6].[Size]\n");
	SQLCommand			+= TEXT("FROM (\n");
	SQLCommand			+= TEXT("	SELECT [t4].[PackageID], [t4].[value], [t4].[PackageID2], [t4].[Size], [t4].[value2], LTRIM(RTRIM([t5].[ClassName])) AS [value3], [t4].[ObjectName]\n");
	SQLCommand			+= TEXT("FROM (\n");
	SQLCommand			+= TEXT("	SELECT [t1].[PackageID], [t1].[value], [t2].[PackageID] AS [PackageID2], [t2].[Size], LTRIM(RTRIM([t3].[ObjectName])) AS [value2], [t3].[ClassID], [t3].[ObjectName]\n");
	SQLCommand			+= TEXT("FROM (\n");
	SQLCommand			+= TEXT("	SELECT [t0].[PackageID], LTRIM(RTRIM([t0].[PackageName])) AS [value]\n");
	SQLCommand			+= TEXT("FROM [Packages] AS [t0]\n");
	SQLCommand			+= TEXT(") AS [t1]\n");
	SQLCommand			+= TEXT("CROSS JOIN [Exports] AS [t2]\n");
	SQLCommand			+= TEXT("INNER JOIN [Objects] AS [t3] ON [t3].[ObjectID] = [t2].[ObjectID]\n");
	SQLCommand			+= TEXT(") AS [t4]\n");
	SQLCommand			+= TEXT("INNER JOIN [Classes] AS [t5] ON [t5].[ClassID] = [t4].[ClassID]\n");
	SQLCommand			+= TEXT(") AS [t6]\n");
	SQLCommand			+= TEXT("WHERE [t6].[PackageID2] = [t6].[PackageID]\n");
	SQLCommand			+= TEXT("ORDER BY [t6].[value], [t6].[Size], [t6].[ObjectName]");

	if (ExecuteSQLQuery(SQLCommand, OutCookedInfoMap, bInIgnoreObjsWithColonInName, OutPackageToCookedObjectsMap, OutCookedPackages) == FALSE)
	{
		warnf(NAME_Warning, TEXT("Failed to execute SQL Query..."));
		return FALSE;
	}

	DOUBLE TotalGatherTime = appSeconds() - StartGatherTime;
	warnf(NAME_Log, TEXT("COMPLETED in %5.3f seconds."), TotalGatherTime);

	return TRUE;
}

/**
 *	Gather ALL cooked assets in a set of packages
 *
 *	@param	InPackageStartsWith				The string the package must start with
 *	@param	OutCookedInfoMap				The assets --> cookingobjectinfo map to fill in
 *	@param	bInIgnoreObjsWithColonInName	If TRUE, ignore objects with ':' in their name (properties, etc.)
 *	@param	OutPackageToCookedObjectsMap	Optional map of cooked packages to objects contained in it to fill in
 *	@param	OutCookedPackages				Optional map of cooked packages to fill in
 *
 *	@return	UBOOL					TRUE if successful, FALSE if not
 */
UBOOL UBaseDatabaseCommandlet::GatherAllCookedAssetInfoForPackages(FString& InPackageStartsWith,
	TMap<FString,FCookedObjectInfo >& OutCookedInfoMap, 
	UBOOL bInIgnoreObjsWithColonInName, 
	TMap<FString,FPackageCookedObjectInfo>* OutPackageToCookedObjectsMap, 
	TMap<FString,UBOOL>* OutCookedPackages)
{
	if (DBConnection == NULL)
	{
		return FALSE;
	}

	warnf(NAME_Log, TEXT("Gathering all cooked assets in %s* from %s.%s..."), *InPackageStartsWith, *DatabaseName, *CatalogName);

	DOUBLE StartGatherTime = appSeconds();

	// Retrieve all objects [class name, serialize size, and object name] cooked into each package.

	FString RestrictName = InPackageStartsWith;
	RestrictName = RestrictName.Replace(TEXT("_"), TEXT("~_"));

	FString SQLCommand	 = TEXT("-- Region Parameters\n");
	SQLCommand			+= FString::Printf(TEXT("DECLARE @p0 VarChar(%d) = '%%%s%%'\n"), RestrictName.Len(), *RestrictName);
	SQLCommand			+= TEXT("-- EndRegion\n");
	SQLCommand			+= TEXT("	SELECT [t6].[value] AS [PackageName], [t6].[value3] AS [ClassName], [t6].[value2] AS [ObjectName], [t6].[Size]\n");
	SQLCommand			+= TEXT("FROM (\n");
	SQLCommand			+= TEXT("	SELECT [t4].[PackageID], [t4].[value], [t4].[PackageID2], [t4].[Size], [t4].[value2], LTRIM(RTRIM([t5].[ClassName])) AS [value3]\n");
	SQLCommand			+= TEXT("FROM (\n");
	SQLCommand			+= TEXT("	SELECT [t1].[PackageID], [t1].[value], [t2].[PackageID] AS [PackageID2], [t2].[Size], LTRIM(RTRIM([t3].[ObjectName])) AS [value2], [t3].[ClassID]\n");
	SQLCommand			+= TEXT("FROM (\n");
	SQLCommand			+= TEXT("	SELECT [t0].[PackageID], LTRIM(RTRIM([t0].[PackageName])) AS [value]\n");
	SQLCommand			+= TEXT("FROM [Packages] AS [t0]\n");
	SQLCommand			+= TEXT(") AS [t1]\n");
	SQLCommand			+= TEXT("CROSS JOIN [Exports] AS [t2]\n");
	SQLCommand			+= TEXT("INNER JOIN [Objects] AS [t3] ON [t3].[ObjectID] = [t2].[ObjectID]\n");
	SQLCommand			+= TEXT(") AS [t4]\n");
	SQLCommand			+= TEXT("INNER JOIN [Classes] AS [t5] ON [t5].[ClassID] = [t4].[ClassID]\n");
	SQLCommand			+= TEXT(") AS [t6]\n");
	SQLCommand			+= TEXT("WHERE ([t6].[value] LIKE @p0 ESCAPE '~') AND ([t6].[PackageID2] = [t6].[PackageID])\n");
	SQLCommand			+= TEXT("	ORDER BY [t6].[value], [t6].[value3], [t6].[value2]");

	if (ExecuteSQLQuery(SQLCommand, OutCookedInfoMap, bInIgnoreObjsWithColonInName, OutPackageToCookedObjectsMap, OutCookedPackages) == FALSE)
	{
		warnf(NAME_Warning, TEXT("Failed to execute SQL Query..."));
		return FALSE;
	}

	DOUBLE TotalGatherTime = appSeconds() - StartGatherTime;
	warnf(NAME_Log, TEXT("COMPLETED in %5.3f seconds."), TotalGatherTime);

	return TRUE;
}

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
UBOOL UBaseDatabaseCommandlet::GatherAllCookedAssetInfoInPackages(const TArray<FString>& InPackages,
	TMap<FString,FCookedObjectInfo >& OutCookedInfoMap, 
	UBOOL bInIgnoreObjsWithColonInName, 
	TMap<FString,FPackageCookedObjectInfo>* OutPackageToCookedObjectsMap, 
	TMap<FString,UBOOL>* OutCookedPackages)
{
	if (DBConnection == NULL)
	{
		return FALSE;
	}

	if (InPackages.Num() == 0)
	{
		warnf(NAME_Warning, TEXT("GatherAllCookedAssetInfoInPackages: Invalid package list"));
		return FALSE;
	}

	warnf(NAME_Log, TEXT("Gathering all cooked assets in the following from %s.%s:"), *DatabaseName, *CatalogName);
	for (INT PkgIdx = 0; PkgIdx < InPackages.Num(); PkgIdx++)
	{
		FString PkgName = InPackages(PkgIdx);
		if (PkgName.Len() > 0)
		{
			warnf(NAME_Log, TEXT("\t%s"), *PkgName);
		}
	}

	DOUBLE StartGatherTime = appSeconds();

	FString SQLCommand	 = TEXT("-- Region Parameters\n");
	INT TruePackageCount = 0;
	for (INT PkgIdx = 0; PkgIdx < InPackages.Num(); PkgIdx++)
	{
		FString PkgName = InPackages(PkgIdx);
		if (PkgName.Len() > 0)
		{
			SQLCommand	+= FString::Printf(TEXT("DECLARE @p%d VarChar(%d) = '%s'\n"), TruePackageCount, PkgName.Len(), *PkgName);
			TruePackageCount++;
		}
	}
	if (TruePackageCount == 0)
	{
		warnf(NAME_Warning, TEXT("GatherAllCookedAssetInfoInPackages: Invalid package list"));
		return FALSE;
	}
	SQLCommand			+= TEXT("-- EndRegion\n");
	SQLCommand			+= TEXT("SELECT [t6].[value] AS [PackageName], [t6].[value3] AS [ClassName], [t6].[value2] AS [ObjectName], [t6].[Size]\n");
	SQLCommand			+= TEXT("FROM (\n");
	SQLCommand			+= TEXT("	  SELECT [t4].[PackageID], [t4].[value], [t4].[PackageID2], [t4].[Size], [t4].[value2], LTRIM(RTRIM([t5].[ClassName])) AS [value3]\n");
	SQLCommand			+= TEXT("FROM (\n");
	SQLCommand			+= TEXT("	  SELECT [t1].[PackageID], [t1].[value], [t2].[PackageID] AS [PackageID2], [t2].[Size], LTRIM(RTRIM([t3].[ObjectName])) AS [value2], [t3].[ClassID]\n");
	SQLCommand			+= TEXT("FROM (\n");
	SQLCommand			+= TEXT("	  SELECT [t0].[PackageID], LTRIM(RTRIM([t0].[PackageName])) AS [value]\n");
	SQLCommand			+= TEXT("FROM [Packages] AS [t0]\n");
	SQLCommand			+= TEXT(") AS [t1]\n");
	SQLCommand			+= TEXT("CROSS JOIN [Exports] AS [t2]\n");
	SQLCommand			+= TEXT("INNER JOIN [Objects] AS [t3] ON [t3].[ObjectID] = [t2].[ObjectID]\n");
	SQLCommand			+= TEXT(") AS [t4]\n");
	SQLCommand			+= TEXT("INNER JOIN [Classes] AS [t5] ON [t5].[ClassID] = [t4].[ClassID]\n");
	SQLCommand			+= TEXT(") AS [t6]\n");
	SQLCommand			+= TEXT("WHERE (");
	for (INT PkgIdx = 0; PkgIdx < TruePackageCount; PkgIdx++)
	{
		if (PkgIdx > 0)
		{
			SQLCommand	+= TEXT(" OR ");
		}
		SQLCommand	+= FString::Printf(TEXT("([t6].[value] = @p%d)"), PkgIdx);
	}
	SQLCommand			+= TEXT(") AND ([t6].[PackageID2] = [t6].[PackageID])\n");
	SQLCommand			+= TEXT("ORDER BY [t6].[value], [t6].[value3], [t6].[value2]");

	if (ExecuteSQLQuery(SQLCommand, OutCookedInfoMap, bInIgnoreObjsWithColonInName, OutPackageToCookedObjectsMap, OutCookedPackages) == FALSE)
	{
		warnf(NAME_Warning, TEXT("Failed to execute SQL Query..."));
		return FALSE;
	}

	DOUBLE TotalGatherTime = appSeconds() - StartGatherTime;
	warnf(NAME_Log, TEXT("COMPLETED in %5.3f seconds."), TotalGatherTime);

	return TRUE;
}

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
UBOOL UBaseDatabaseCommandlet::GatherSpecifiedCookedAssetsInfo(TArray<FString>& InClassNames, 
	const TArray<FString>& InPackages,
	TMap<FString,FCookedObjectInfo >& OutCookedInfoMap, 
	UBOOL bInIgnoreObjsWithColonInName, 
	TMap<FString,FPackageCookedObjectInfo>* OutPackageToCookedObjectsMap, 
	TMap<FString,UBOOL>* OutCookedPackages)
{
	if (DBConnection == NULL)
	{
		return FALSE;
	}

	if (InPackages.Num() == 0)
	{
		warnf(NAME_Warning, TEXT("GatherAllCookedAssetInfoInPackages: Invalid package list"));
		return FALSE;
	}

	warnf(NAME_Log, TEXT("Gathering all cooked assets in the following from %s.%s:"), *DatabaseName, *CatalogName);
	for (INT PkgIdx = 0; PkgIdx < InPackages.Num(); PkgIdx++)
	{
		FString PkgName = InPackages(PkgIdx);
		if (PkgName.Len() > 0)
		{
			warnf(NAME_Log, TEXT("\t%s"), *PkgName);
		}
	}

	INT TruePackageCount = 0;

	DOUBLE StartGatherTime = appSeconds();

	FString SQLCommand	 = TEXT("-- Region Parameters\n");
	for (INT ClassIdx = 0; ClassIdx < InClassNames.Num(); ClassIdx++)
	{
		FString ClassName = InClassNames(ClassIdx);
		SQLCommand += FString::Printf(TEXT("DECLARE @p%d VarChar(%d) = '%s'\n"), ClassIdx, ClassName.Len(), *ClassName);
	}
	INT FirstPackageIndex = InClassNames.Num();
	for (INT PkgIdx = 0; PkgIdx < InPackages.Num(); PkgIdx++)
	{
		FString PkgName = InPackages(PkgIdx);
		if (PkgName.Len() > 0)
		{
			SQLCommand	+= FString::Printf(TEXT("DECLARE @p%d VarChar(%d) = '%s'\n"), FirstPackageIndex + TruePackageCount, PkgName.Len(), *PkgName);
			TruePackageCount++;
		}
	}
	if (TruePackageCount == 0)
	{
		warnf(NAME_Warning, TEXT("GatherAllCookedAssetInfoInPackages: Invalid package list"));
		return FALSE;
	}
	SQLCommand			+= TEXT("-- EndRegion\n");
	SQLCommand			+= TEXT("	SELECT [t6].[value] AS [PackageName], [t6].[value3] AS [ClassName], [t6].[value2] AS [ObjectName], [t6].[Size]\n");
	SQLCommand			+= TEXT("FROM (\n");
	SQLCommand			+= TEXT("	SELECT [t5].[PackageID], [t5].[value], [t5].[PackageID2], [t5].[Size], [t5].[value2], LTRIM(RTRIM([t5].[ClassName])) AS [value3], [t5].[ClassName], [t5].[ObjectName]\n");
	SQLCommand			+= TEXT("FROM (\n");
	SQLCommand			+= TEXT("	SELECT [t1].[PackageID], [t1].[value], [t2].[PackageID] AS [PackageID2], [t2].[Size], LTRIM(RTRIM([t3].[ObjectName])) AS [value2], [t4].[ClassName], [t3].[ObjectName]\n");
	SQLCommand			+= TEXT("FROM (\n");
	SQLCommand			+= TEXT("	SELECT [t0].[PackageID], LTRIM(RTRIM([t0].[PackageName])) AS [value]\n");
	SQLCommand			+= TEXT("FROM [Packages] AS [t0]\n");
	SQLCommand			+= TEXT(") AS [t1]\n");
	SQLCommand			+= TEXT("CROSS JOIN [Exports] AS [t2]\n");
	SQLCommand			+= TEXT("INNER JOIN [Objects] AS [t3] ON [t3].[ObjectID] = [t2].[ObjectID]\n");
	SQLCommand			+= TEXT("INNER JOIN [Classes] AS [t4] ON [t4].[ClassID] = [t3].[ClassID]\n");
	SQLCommand			+= TEXT(") AS [t5]\n");
	SQLCommand			+= TEXT(") AS [t6]\n");
	SQLCommand			+= TEXT("WHERE (");
	//([t6].[ClassName] = @p0) OR ([t6].[ClassName] = @p1)
	for (INT ClassIdx = 0; ClassIdx < InClassNames.Num(); ClassIdx++)
	{
		if (ClassIdx > 0)
		{
			SQLCommand += TEXT(" OR ");
		}
		SQLCommand += FString::Printf(TEXT("([t6].[ClassName] = @p%d)"), ClassIdx);
	}
	SQLCommand			+= TEXT(") AND (");
	//([t6].[value] = @p2) OR ([t6].[value] = @p3) OR ([t6].[value] = @p4)
	for (INT PkgIdx = 0; PkgIdx < TruePackageCount; PkgIdx++)
	{
		if (PkgIdx > 0)
		{
			SQLCommand += TEXT(" OR ");
		}
		SQLCommand += FString::Printf(TEXT("([t6].[value] = @p%d)"), FirstPackageIndex + PkgIdx);
	}
	SQLCommand			+= TEXT(") AND ([t6].[PackageID2] = [t6].[PackageID])\n");
	SQLCommand			+= TEXT("	ORDER BY [t6].[value], [t6].[ObjectName]");

	if (ExecuteSQLQuery(SQLCommand, OutCookedInfoMap, bInIgnoreObjsWithColonInName, OutPackageToCookedObjectsMap, OutCookedPackages) == FALSE)
	{
		warnf(NAME_Warning, TEXT("Failed to execute SQL Query..."));
		return FALSE;
	}

	DOUBLE TotalGatherTime = appSeconds() - StartGatherTime;
	warnf(NAME_Log, TEXT("COMPLETED in %5.3f seconds."), TotalGatherTime);

	return TRUE;
}

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
UBOOL UBaseDatabaseCommandlet::ExecuteSQLQuery(const FString& InSQLCommand,
	TMap<FString,FCookedObjectInfo >& OutCookedInfoMap, 
	UBOOL bInIgnoreObjsWithColonInName, 
	TMap<FString,FPackageCookedObjectInfo>* OutPackageToCookedObjectsMap, 
	TMap<FString,UBOOL>* OutCookedPackages)
{
	FDataBaseRecordSet* NewRecordSet = NULL;
	if (DBConnection->Execute(*InSQLCommand, NewRecordSet) == FALSE)
	{
		warnf(NAME_Error, TEXT("Failed to exec SQLCommand for..."));
		warnf(NAME_Error, *InSQLCommand);
		return FALSE;
	}

	if (bVerbose == TRUE)
	{
		warnf(NAME_Log, TEXT("Dumping found entries..."));
	}
	for (FDataBaseRecordSet::TIterator It(NewRecordSet); It; ++It)
	{
		// Retrieve the package, object and class names as well as the object serialize size
		FString PackageString = It->GetString(TEXT("PackageName"));
		PackageString.TrimTrailing();

		FString ObjectString = It->GetString(TEXT("ObjectName"));
		ObjectString.TrimTrailing();
		if (bInIgnoreObjsWithColonInName == TRUE)
		{
			if (ObjectString.InStr(TEXT(":")) != INDEX_NONE)
			{
				continue;
			}
		}

		INT ObjectSize = It->GetInt(TEXT("Size"));

		FString ClassString = It->GetString(TEXT("ClassName"));
		ClassString.TrimTrailing();

		// NULL returns as 'Unknown Column'
		if (ObjectString != TEXT("Unknown Column"))
		{
			FString FullString = FString::Printf(TEXT("%s'%s'"), *ClassString, *ObjectString);
			FCookedObjectInfo* CookedInfo = OutCookedInfoMap.Find(FullString);
			if (CookedInfo == NULL)
			{
				FCookedObjectInfo TempCookedInfo;
				OutCookedInfoMap.Set(FullString, TempCookedInfo);
				CookedInfo = OutCookedInfoMap.Find(FullString);
				CookedInfo->ClassName = ClassString;
				CookedInfo->ObjectName = ObjectString;
				CookedInfo->TotalSerializeSize = 0;
			}
			check(CookedInfo);
			CookedInfo->TotalSerializeSize += ObjectSize;
			CookedInfo->PackagesCookedInto.Set(PackageString, ObjectSize);

			if (OutPackageToCookedObjectsMap != NULL)
			{
				// Find the entry for this package...
				FPackageCookedObjectInfo* PkgCookedObjInfo = OutPackageToCookedObjectsMap->Find(PackageString);
				if (PkgCookedObjInfo == NULL)
				{
					FPackageCookedObjectInfo TempPkgCookedObjInfo;
					OutPackageToCookedObjectsMap->Set(PackageString, TempPkgCookedObjInfo);
					PkgCookedObjInfo = OutPackageToCookedObjectsMap->Find(PackageString);
				}
				PkgCookedObjInfo->ObjectsContained.Set(FullString, ObjectSize);
			}
			if (OutCookedPackages != NULL)
			{
				OutCookedPackages->Set(PackageString.ToUpper(), TRUE);
			}

			if (bVerbose == TRUE)
			{
				FString DumpLine = FString::Printf(TEXT("%s,%s,%d\n"), *PackageString, *ObjectString, ObjectSize);
				OutputDebugString(*DumpLine);
			}
		}
	}

	return TRUE;
}

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
UBOOL UBaseDatabaseCommandlet::DumpSimpleMapping(TMap<FString,UBOOL>& InMap, 
	TMap<FString, FCookedObjectInfo>* InOptCookedObjectInfoMap, 
	const TCHAR* InShortFilename, const TCHAR* InObjectClassName)
{
	if (InMap.Num() > 0)
	{
		check(InShortFilename != NULL);
		check(InObjectClassName != NULL);

		FArchive* OutputStream = GetOutputFile(TEXT("Audit"), InShortFilename);
		if (OutputStream != NULL)
		{
			warnf(NAME_Log, TEXT("Dumping '%s' results..."), InShortFilename);
			OutputStream->Logf(TEXT("%s,Cooked Packages Contained In,..."), InObjectClassName);
			for (TMap<FString,UBOOL>::TIterator DumpIt(InMap); DumpIt; ++DumpIt)
			{
				FString ObjName = DumpIt.Key();

				// Find it in the CookedParticleSystems list
				FString FullName = FString::Printf(TEXT("%s'%s'"), InObjectClassName, *ObjName);
				FCookedObjectInfo* ObjInfo = InOptCookedObjectInfoMap ? InOptCookedObjectInfoMap->Find(FullName) : NULL;
				if (ObjInfo != NULL)
				{
					FString Output = ObjName;
					for (TMap<FString,INT>::TIterator PackageIt(ObjInfo->PackagesCookedInto); PackageIt; ++PackageIt)
					{
						Output += FString::Printf(TEXT(",%s"), *(PackageIt.Key()));
					}
					OutputStream->Logf(*Output);
				}
				else
				{
					OutputStream->Logf(TEXT("%s"), *ObjName);
				}
			}

			OutputStream->Close();
			delete OutputStream;
		}
		else
		{
			return FALSE;
		}
	}
	return TRUE;
}

INT UBaseDatabaseCommandlet::Main(const FString& Params)
{
	ParseCommandLine(*Params, Tokens, Switches);

	if (Startup() == FALSE)
	{
		warnf(NAME_Error, TEXT("Startup() failed..."));
		return -1;
	}

	if (Initialize() == FALSE)
	{
		warnf(NAME_Error, TEXT("Initialize() failed..."));
		return -2;
	}

	// Open up the database
	if (OpenDatabaseConnection(*DatabaseName, *CatalogName, ConnectionTimeout) == FALSE)
	{
		warnf(NAME_Error, TEXT("Failed to open database connection to %s:%s"), *DatabaseName, *CatalogName);
		return -3;
	}

	// Mine the given database for the objects of interest
	if (MineDatabaseForObjects() == FALSE)
	{
		warnf(NAME_Error, TEXT("Failed to mine for objects in database %s:%s"), *DatabaseName, *CatalogName);
		return -4;
	}

	// Process the minded data
	ProcessMinedData();

	// Dump the results
	DumpResults();

	// Shutdown the commandlet
	Shutdown();

	return 0;
}

IMPLEMENT_COMPARE_CONSTREF(FString, UDatabaseCommandlets, { return appStricmp(*A,*B); })

//
//	UParticleSystemAuditCommandlet
//
IMPLEMENT_CLASS(UParticleSystemAuditCommandlet);

/**
 *	Startup the commandlet.
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UParticleSystemAuditCommandlet::Startup()
{
	if (Super::Startup() == TRUE)
	{
		// Gather the list of all cooked particles systems, particle system components and emitter actors
		FString ClassName = TEXT("ParticleSystem");
		ClassesOfInterest.AddItem(ClassName);
		ClassName = TEXT("ParticleSystemComponent");
		ClassesOfInterest.AddItem(ClassName);
		ClassName = TEXT("Emitter");
		ClassesOfInterest.AddItem(ClassName);
		return TRUE;
	}
	return FALSE;
}

/** Process all referenced particle systems that were found */
UBOOL UParticleSystemAuditCommandlet::ProcessReferencedParticleSystems()
{
	// Find all particle systems that are contained in map packages...
	for (TMap<FString, FCookedObjectInfo>::TIterator PSysIt(CookedParticleSystems); PSysIt; ++PSysIt)
	{
		FString FullName = PSysIt.Key();
		FCookedObjectInfo& ObjInfo = PSysIt.Value();

		UBOOL bLevelPlaced = FALSE;
		for (TMap<FString,INT>::TIterator MapIt(ObjInfo.PackagesCookedInto); MapIt; ++MapIt)
		{
			FString PackageName = MapIt.Key();

			if (MapPackages.Find(PackageName) != NULL)
			{
				bLevelPlaced = TRUE;
				PackageReferencedParticleSystems.Set(ObjInfo.ObjectName, TRUE);
				// If it's in at least one, that's all we need to know right now...
				break;
			}
		}

		if (bLevelPlaced == FALSE)
		{
			PackageReferencedParticleSystems.Set(ObjInfo.ObjectName, FALSE);
		}
	}

	// Sort the list so it is 'package friendly
	TMap<FString,UBOOL> SortedPSysList = PackageReferencedParticleSystems;
	SortedPSysList.KeySort<COMPARE_CONSTREF_CLASS(FString,UDatabaseCommandlets)>();

	// Find all level placed particle systems with:
	//	- Single LOD level
	//	- No fixed bounds
	//	- LODLevel Mismatch 
	//	- Kismet referenced & auto-activate set
	// Iterate over the list and check each system for *no* lod
	// 
	FString LastPackageName = TEXT("");
	INT PackageSwitches = 0;
	UPackage* CurrentPackage = NULL;
	for (TMap<FString,UBOOL>::TIterator It(SortedPSysList); It; ++It)
	{
		FString PSysName = It.Key();
		UBOOL bLevelPlaced = It.Value();
		INT FirstDotIdx = PSysName.InStr(TEXT("."));
		if (FirstDotIdx != INDEX_NONE)
		{
			FString PackageName = PSysName.Left(FirstDotIdx);
			if (PackageName != LastPackageName)
			{
				UPackage* Package = UObject::LoadPackage(NULL, *PackageName, LOAD_None);
				if (Package != NULL)
				{
					LastPackageName = PackageName;
					Package->FullyLoad();
					CurrentPackage = Package;
				}
				else
				{
					warnf(NAME_Warning, TEXT("Failed to load package %s processing %s"), *PackageName, *PSysName);
					CurrentPackage = NULL;
				}
			}

			FString ShorterPSysName = PSysName.Right(PSysName.Len() - FirstDotIdx - 1);
			UParticleSystem* PSys = FindObject<UParticleSystem>(CurrentPackage, *ShorterPSysName);
			if (PSys != NULL)
			{
				UBOOL bInvalidLOD = FALSE;
				UBOOL bSingleLOD = FALSE;
				UBOOL bFoundEmitter = FALSE;
				UBOOL bMissingMaterial = FALSE;
				UBOOL bHasConstantColorScaleOverLife = FALSE;
				UBOOL bHasCollisionEnabled = FALSE;
				INT ConstantColorScaleOverLifeCount = 0;
				for (INT EmitterIdx = 0; EmitterIdx < PSys->Emitters.Num(); EmitterIdx++)
				{
					UParticleEmitter* Emitter = PSys->Emitters(EmitterIdx);
					if (Emitter != NULL)
					{
						if (Emitter->LODLevels.Num() == 0)
						{
							bInvalidLOD = TRUE;
						}
						else if (Emitter->LODLevels.Num() == 1)
						{
							bSingleLOD = TRUE;
						}
						bFoundEmitter = TRUE;
						for (INT LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
						{
							UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIdx);
							if (LODLevel != NULL)
							{
								if (LODLevel->RequiredModule != NULL)
								{
									if (LODLevel->RequiredModule->Material == NULL)
									{
										bMissingMaterial = TRUE;
									}
								}

								for (INT ModuleIdx = 0; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
								{
									UParticleModule* Module = LODLevel->Modules(ModuleIdx);

									UParticleModuleColorScaleOverLife* CSOLModule = Cast<UParticleModuleColorScaleOverLife>(Module);
									if (CSOLModule != NULL)
									{
										UDistributionFloatConstant* FloatConst = Cast<UDistributionFloatConstant>(CSOLModule->AlphaScaleOverLife.Distribution);
										UDistributionVectorConstant* VectorConst = Cast<UDistributionVectorConstant>(CSOLModule->ColorScaleOverLife.Distribution);
										if ((FloatConst != NULL) && (VectorConst != NULL))
										{
											bHasConstantColorScaleOverLife = TRUE;
											ConstantColorScaleOverLifeCount++;
										}
									}
									else if (bHasCollisionEnabled == FALSE)
									{
										UParticleModuleCollision* CollisionModule = Cast<UParticleModuleCollision>(Module);
										if (CollisionModule != NULL)
										{
											if (CollisionModule->bEnabled == TRUE)
											{
												bHasCollisionEnabled = TRUE;
											}
										}
									}
								}
							}
						}
					}
				}

				// Note all PSystems w/ no emitters...
				if (PSys->Emitters.Num() == 0)
				{
					ParticleSystemsWithNoEmitters.Set(PSys->GetPathName(), bLevelPlaced);
				}

				// Note all missing material case PSystems...
				if (bMissingMaterial == TRUE)
				{
					ParticleSystemsWithMissingMaterials.Set(PSys->GetPathName(), bLevelPlaced);
				}

				// Note all PSystems that have at least one emitter w/ constant ColorScaleOverLife modules...
				if (bHasConstantColorScaleOverLife == TRUE)
				{
					ParticleSystemsWithConstantColorScaleOverLife.Set(PSys->GetPathName(), bLevelPlaced);
					ParticleSystemsWithConstantColorScaleOverLifeCounts.Set(PSys->GetPathName(), ConstantColorScaleOverLifeCount);
				}

				// Note all PSystems that have at least one emitter w/ an enabled collision module...
				if (bHasCollisionEnabled == TRUE)
				{
					ParticleSystemsWithCollisionEnabled.Set(PSys->GetPathName(), bLevelPlaced);
				}

				// Note all 0 LOD case PSystems...
				if (bInvalidLOD == TRUE)
				{
					ParticleSystemsWithNoLODs.Set(PSys->GetPathName(), bLevelPlaced);
				}
				// Note all single LOD case PSystems...
				if (bSingleLOD == TRUE)
				{
					ParticleSystemsWithSingleLOD.Set(PSys->GetPathName(), bLevelPlaced);
				}

				// Note all non-fixed bound PSystems...
				if (PSys->bUseFixedRelativeBoundingBox == FALSE)
				{
					ParticleSystemsWithoutFixedBounds.Set(PSys->GetPathName(), bLevelPlaced);
				}

				// Note all bOrientZAxisTowardCamera systems
				if (PSys->bOrientZAxisTowardCamera == TRUE)
				{
					ParticleSystemsWithOrientZAxisTowardCamera.Set(PSys->GetPathName(), bLevelPlaced);
				}

				if ((PSys->LODMethod == PARTICLESYSTEMLODMETHOD_Automatic) &&
					(bInvalidLOD == FALSE) && (bSingleLOD == FALSE) &&
					(PSys->LODDistanceCheckTime == 0.0f))
				{
					ParticleSystemsWithBadLODCheckTimes.Set(PSys->GetPathName(), bLevelPlaced);
				}

				// Find LOD mistmatches - looping & non looping, etc.
				CheckPSysForLODMismatches(PSys);

				// Find duplicate module information
				CheckPSysForDuplicateModules(PSys);

				if (LastPackageName.Len() > 0)
				{
					if (LastPackageName != PSys->GetOutermost()->GetName())
					{
						LastPackageName = PSys->GetOutermost()->GetName();
						PackageSwitches++;
					}
				}
				else
				{
					LastPackageName = PSys->GetOutermost()->GetName();
				}

				if (PackageSwitches > 10)
				{
					UObject::CollectGarbage(RF_Native);
					PackageSwitches = 0;
				}

			}
			else
			{
				warnf(NAME_Warning, TEXT("Failed to load particle system %s"), *PSysName);
			}
		}
		else
		{
			warnf(NAME_Warning, TEXT("Incomplete path name for %s"), *PSysName);
		}
	}

	// Probably don't need to do this, but just in case we have any 'hanging' packages 
	// and more processing steps are added later, let's clean up everything...
	UObject::CollectGarbage(RF_Native);

	return TRUE;
}

/** 
 *	Check the given ParticleSystem for LOD mismatch issues
 *
 *	@param	InPSys		The particle system to check
 */
void UParticleSystemAuditCommandlet::CheckPSysForLODMismatches(UParticleSystem* InPSys)
{
	if (InPSys == NULL)
	{
		return;
	}

	// Process it...
	UBOOL bHasLoopingMismatch = FALSE;
	UBOOL bHasIntraLoopingMismatch = FALSE;
	UBOOL bHasInterLoopingMismatch = FALSE;
	UBOOL bHasMultipleLODLevels = TRUE;

	TArray<INT> InterEmitterCompare;

	TArray<UBOOL> EmitterLODLevelEnabledFlags;
	FParticleSystemLODInfo* LODInfo = NULL;

	EmitterLODLevelEnabledFlags.Empty(InPSys->Emitters.Num());
	EmitterLODLevelEnabledFlags.AddZeroed(InPSys->Emitters.Num());
	for (INT EmitterIdx = 0; (EmitterIdx < InPSys->Emitters.Num()) && !bHasLoopingMismatch; EmitterIdx++)
	{
		UParticleEmitter* Emitter = InPSys->Emitters(EmitterIdx);
		if (Emitter != NULL)
		{
			INT EmitterLooping = -1;
			for (INT LODIdx = 0; (LODIdx < Emitter->LODLevels.Num()) && !bHasLoopingMismatch; LODIdx++)
			{
				UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIdx);
				if (LODLevel != NULL)
				{
					if (LODIdx == 0)
					{
						EmitterLODLevelEnabledFlags(EmitterIdx) = LODLevel->bEnabled;
					}
					else
					{
						if (EmitterLODLevelEnabledFlags(EmitterIdx) != LODLevel->bEnabled)
						{
							// MISMATCH
							if (LODInfo == NULL)
							{
								LODInfo = new FParticleSystemLODInfo();
								LODInfo->LODMethod = ParticleSystemLODMethod(InPSys->LODMethod);
							}
							LODInfo->EmittersWithDisableLODMismatch.AddUniqueItem(EmitterIdx);
						}
					}
				}
				if ((LODLevel != NULL) && (LODLevel->bEnabled == TRUE))
				{
					INT CheckEmitterLooping = Min<INT>(LODLevel->RequiredModule->EmitterLoops, 1);
					if (EmitterIdx == 0)
					{
						InterEmitterCompare.AddItem(CheckEmitterLooping);
					}
					else
					{
						if (InterEmitterCompare(LODIdx) != -1)
						{
							if (InterEmitterCompare(LODIdx) != CheckEmitterLooping)
							{
								if (LODInfo == NULL)
								{
									LODInfo = new FParticleSystemLODInfo();
									LODInfo->LODMethod = ParticleSystemLODMethod(InPSys->LODMethod);
								}
								LODInfo->bHasInterLoopingMismatch = TRUE;
							}
						}
						else
						{
							InterEmitterCompare(LODIdx) = CheckEmitterLooping;
						}
					}

					if (EmitterLooping == -1)
					{
						EmitterLooping = CheckEmitterLooping;
					}
					else
					{
						if (EmitterLooping != CheckEmitterLooping)
						{
							if (LODInfo == NULL)
							{
								LODInfo = new FParticleSystemLODInfo();
								LODInfo->LODMethod = ParticleSystemLODMethod(InPSys->LODMethod);
							}
							LODInfo->bHasIntraLoopingMismatch = TRUE;
						}
					}
				}
				else
				{
					if (EmitterIdx == 0)
					{
						InterEmitterCompare.AddItem(-1);
					}
				}
			}
		}
	}

	// If there was an LOD info mismatch, add it to the list...
	if (LODInfo != NULL)
	{
		ParticleSystemsWithLODLevelIssues.Set(InPSys->GetPathName(), *LODInfo);
		delete LODInfo;
	}
}

/**
 *	Determine the given ParticleSystems duplicate module information 
 *
 *	@param	InPSys		The particle system to check
 */
void UParticleSystemAuditCommandlet::CheckPSysForDuplicateModules(UParticleSystem* InPSys)
{
	if (InPSys == NULL)
	{
		return;
	}

	FParticleSystemDuplicateModuleInfo* DupInfo = PSysDuplicateModuleInfo.Find(InPSys->GetPathName());
	if (DupInfo == NULL)
	{
		// Compare all the particle modules in the array
		TMap<UClass*,TMap<UParticleModule*,INT> > ClassToModulesMap;
		TMap<UParticleModule*,INT> AllModulesArray;
		for (INT EmitterIdx = 0; EmitterIdx < InPSys->Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = InPSys->Emitters(EmitterIdx);
			if (Emitter != NULL)
			{
				for (INT LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
				{
					UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIdx);
					if (LODLevel != NULL)
					{
						for (INT ModuleIdx = -1; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
						{
							UParticleModule* Module = NULL;
							if (ModuleIdx == -1)
							{
								Module = LODLevel->SpawnModule;
							}
							else
							{
								Module = LODLevel->Modules(ModuleIdx);
							}
							if (Module != NULL)
							{
								if (AllModulesArray.Find(Module) == NULL)
								{
									FArchiveCountMem ModuleMemCount(Module);
									INT ModuleSize = ModuleMemCount.GetMax();
									AllModulesArray.Set(Module, ModuleSize);
								}

								TMap<UParticleModule*,INT>* ModuleList = ClassToModulesMap.Find(Module->GetClass());
								if (ModuleList == NULL)
								{
									TMap<UParticleModule*,INT> TempModuleList;
									ClassToModulesMap.Set(Module->GetClass(), TempModuleList);
									ModuleList = ClassToModulesMap.Find(Module->GetClass());
								}
								check(ModuleList);
								INT* ModuleCount = ModuleList->Find(Module);
								if (ModuleCount == NULL)
								{
									INT TempModuleCount = 0;
									ModuleList->Set(Module, TempModuleCount);
									ModuleCount = ModuleList->Find(Module);
								}
								check(ModuleCount);
								*ModuleCount++;
							}
						}
					}
				}
			}
		}

		// Now we have a list of module classes and the modules they contain
		TMap<UParticleModule*, TArray<UParticleModule*> > DuplicateModules;
		TMap<UParticleModule*,UBOOL> FoundAsADupeModules;
		for (TMap<UClass*,TMap<UParticleModule*,INT> >::TIterator ModClassIt(ClassToModulesMap); ModClassIt; ++ModClassIt)
		{
			UClass* ModuleClass = ModClassIt.Key();
			TMap<UParticleModule*,INT>& ModuleMap = ModClassIt.Value();
			if (ModuleMap.Num() > 1)
			{
				// There is more than one of this module, so see if there are dupes...
				TArray<UParticleModule*> ModuleArray;
				for (TMap<UParticleModule*,INT>::TIterator ModuleIt(ModuleMap); ModuleIt; ++ModuleIt)
				{
					ModuleArray.AddItem(ModuleIt.Key());
				}

				// For each module, see if it it a duplicate of another
				for (INT ModuleIdx = 0; ModuleIdx < ModuleArray.Num(); ModuleIdx++)
				{
					UParticleModule* SourceModule = ModuleArray(ModuleIdx);
					if (FoundAsADupeModules.Find(SourceModule) == NULL)
					{
						for (INT InnerModuleIdx = ModuleIdx + 1; InnerModuleIdx < ModuleArray.Num(); InnerModuleIdx++)
						{
							UParticleModule* CheckModule = ModuleArray(InnerModuleIdx);
							UBOOL bIsDifferent = FALSE;
							if (FoundAsADupeModules.Find(CheckModule) == NULL)
							{
								FName CascadeCategory(TEXT("Cascade"));
								// Copy non component properties from the old actor to the new actor
								for (UProperty* Property = ModuleClass->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext)
								{
									UBOOL bIsNative = (Property->PropertyFlags & CPF_Native) != 0;
									UBOOL bIsTransient = (Property->PropertyFlags & CPF_Transient) != 0;
									UBOOL bIsComponentProp = (Property->PropertyFlags & CPF_Component) != 0;
									UBOOL bIsEditorOnly = (Property->PropertyFlags & CPF_EditorOnly) != 0;
									UBOOL bIsCascade = (Property->Category == CascadeCategory);
									if (!bIsTransient && !bIsNative && !bIsEditorOnly && !bIsCascade)
									{
										UBOOL bIsIdentical = Property->Identical((BYTE*)SourceModule + Property->Offset, (BYTE*)CheckModule + Property->Offset, PPF_DeepComparison);
										if (bIsIdentical == FALSE)
										{
											bIsDifferent = TRUE;
										}
									}
									else
									{

									}
								}
							}
							if (bIsDifferent == FALSE)
							{
								TArray<UParticleModule*>* DupedModules = DuplicateModules.Find(SourceModule);
								if (DupedModules == NULL)
								{
									TArray<UParticleModule*> TempDupedModules;
									DuplicateModules.Set(SourceModule, TempDupedModules);
									DupedModules = DuplicateModules.Find(SourceModule);
								}
								check(DupedModules);
								DupedModules->AddUniqueItem(CheckModule);
								FoundAsADupeModules.Set(CheckModule, TRUE);
							}
						}
					}
				}
			}
		}

		if (DuplicateModules.Num() > 0)
		{
			FParticleSystemDuplicateModuleInfo TempDupInfo;
			PSysDuplicateModuleInfo.Set(InPSys->GetPathName(), TempDupInfo);
			DupInfo = PSysDuplicateModuleInfo.Find(InPSys->GetPathName());

			DupInfo->ModuleCount = AllModulesArray.Num();
			DupInfo->ModuleMemory = 0;
			INT DupeMemory = 0;
			for (TMap<UParticleModule*,INT>::TIterator ModuleIt(AllModulesArray); ModuleIt; ++ModuleIt)
			{
				INT ModuleSize = ModuleIt.Value();
				DupInfo->ModuleMemory += ModuleSize;
				if (FoundAsADupeModules.Find(ModuleIt.Key()) != NULL)
				{
					DupeMemory += ModuleSize;
				}
			}

			DupInfo->RemovedDuplicateCount = FoundAsADupeModules.Num();
			DupInfo->RemovedDuplicateMemory = DupeMemory;
		}
	}
	else
	{
		warnf(NAME_Warning, TEXT("Already processed PSys for duplicate module information: %s"), *(InPSys->GetPathName()));
	}
}


/**
 *	The main function of the commandlet...
 *
 */
UBOOL UParticleSystemAuditCommandlet::ProcessMinedData()
{
	// Split up into the various groups
	for (TMap<FString, FCookedObjectInfo>::TIterator AssetIt(CookedObjects); AssetIt; ++AssetIt)
	{
		FString FullName = AssetIt.Key();
		FCookedObjectInfo& ObjInfo = AssetIt.Value();

		TMap<FString, FCookedObjectInfo>* SpecificObjectMap = NULL;
		if (ObjInfo.ClassName == TEXT("ParticleSystem"))
		{
			SpecificObjectMap = &CookedParticleSystems;
		}
		else if (ObjInfo.ClassName == TEXT("ParticleSystemComponent"))
		{
			SpecificObjectMap = &CookedParticleSystemComponents;
		}
		else if (ObjInfo.ClassName == TEXT("Emitter"))
		{
			SpecificObjectMap = &CookedEmitterActors;
		}

		if (SpecificObjectMap != NULL)
		{
			FCookedObjectInfo* CheckInfo = SpecificObjectMap->Find(FullName);
			if (CheckInfo == NULL)
			{
				SpecificObjectMap->Set(FullName, ObjInfo);
			}
			else
			{
				warnf(NAME_Warning, TEXT("Duplicate object found in list: %s"), *FullName);
			}
		}
		else
		{
			warnf(NAME_Warning, TEXT("Duplicate object found in list: %s (%s)"), *ObjInfo.ClassName, *FullName);
		}
	}

	DOUBLE StartProcessParticleSystemsTime = appSeconds();
	if (ProcessReferencedParticleSystems() == FALSE)
	{
		warnf(NAME_Warning, TEXT("ProcessReferencedParticleSystems failed..."));
	}
	DOUBLE ProcessParticleSystemsTime = appSeconds() - StartProcessParticleSystemsTime;
	warnf(NAME_Log, TEXT("Took %5.3f seconds to process referenced particle systems..."), ProcessParticleSystemsTime);

	return TRUE;
}

/** Dump the results of the audit */
void UParticleSystemAuditCommandlet::DumpResults()
{
	Super::DumpResults();

	// Dump all the simple mappings...
	DumpSimplePSysMapping(ParticleSystemsWithNoLODs, TEXT("PSysNoLOD"));
	DumpSimplePSysMapping(ParticleSystemsWithSingleLOD, TEXT("PSysSingleLOD"));
	DumpSimplePSysMapping(ParticleSystemsWithoutFixedBounds, TEXT("PSysNoFixedBounds"));
	DumpSimplePSysMapping(ParticleSystemsWithBadLODCheckTimes, TEXT("PSysBadLODCheckTimes"));
	DumpSimplePSysMapping(ParticleSystemsWithMissingMaterials, TEXT("PSysMissingMaterial"));
	DumpSimplePSysMapping(ParticleSystemsWithNoEmitters, TEXT("PSysNoEmitters"));
	DumpSimplePSysMapping(ParticleSystemsWithCollisionEnabled, TEXT("PSysCollisionEnabled"));
	DumpSimplePSysMapping(ParticleSystemsWithConstantColorScaleOverLife, TEXT("PSysConstantColorScale"));
	DumpSimplePSysMapping(ParticleSystemsWithOrientZAxisTowardCamera, TEXT("PSysOrientZTowardsCamera"));

	FArchive* OutputStream;

	// Dump out the particle systems w/ disabled LOD level mismatches...
	const TCHAR* ConstColorScaleCountsName = TEXT("PSysConstColorScaleCounts");
	OutputStream = GetOutputFile(TEXT("Audit"), ConstColorScaleCountsName);
	if (OutputStream != NULL)
	{
		warnf(NAME_Log, TEXT("Dumping '%s' results..."), ConstColorScaleCountsName);
		OutputStream->Logf(TEXT("Particle System,ModuleCount"));
		for (TMap<FString,INT>::TIterator DumpIt(ParticleSystemsWithConstantColorScaleOverLifeCounts); DumpIt; ++DumpIt)
		{
			FString PSysName = DumpIt.Key();
			INT& Count = DumpIt.Value();
			OutputStream->Logf(TEXT("%s,%d"), *PSysName, Count);
		}
		OutputStream->Close();
		delete OutputStream;
	}
	else
	{
		warnf(NAME_Warning, TEXT("Failed to open ConstColorScaleCounts file %s"), ConstColorScaleCountsName);
	}

	// Dump out the particle systems w/ disabled LOD level mismatches...
	const TCHAR* LODIssuesName = TEXT("PSysLODIssues");
	OutputStream = GetOutputFile(TEXT("Audit"), LODIssuesName);
	if (OutputStream != NULL)
	{
		warnf(NAME_Log, TEXT("Dumping '%s' results..."), LODIssuesName);
		OutputStream->Logf(TEXT("Particle System,LOD Method,InterLoop,IntraLoop,Emitters with mismatched LOD enabled"));
		for (TMap<FString,FParticleSystemLODInfo>::TIterator DumpIt(ParticleSystemsWithLODLevelIssues); DumpIt; ++DumpIt)
		{
			FString PSysName = DumpIt.Key();
			FParticleSystemLODInfo& LODInfo = DumpIt.Value();

			FString Output = FString::Printf(TEXT("%s,%s,%s,%s"), *PSysName, 
				(LODInfo.LODMethod == PARTICLESYSTEMLODMETHOD_Automatic) ? TEXT("AUTO") :
				((LODInfo.LODMethod == PARTICLESYSTEMLODMETHOD_ActivateAutomatic) ? TEXT("AUTOACTIVATE") : TEXT("DIRECTSET")),
				LODInfo.bHasInterLoopingMismatch ? TEXT("Y") : TEXT("N"),
				LODInfo.bHasIntraLoopingMismatch ? TEXT("Y") : TEXT("N"));
			for (INT EmitterIdx = 0; EmitterIdx < LODInfo.EmittersWithDisableLODMismatch.Num(); EmitterIdx++)
			{
				Output += FString::Printf(TEXT(",%d"), LODInfo.EmittersWithDisableLODMismatch(EmitterIdx));
			}
			OutputStream->Logf(*Output);
		}
		OutputStream->Close();
		delete OutputStream;
	}
	else
	{
		warnf(NAME_Warning, TEXT("Failed to open LODIssues file %s"), LODIssuesName);
	}

	// Dump out the duplicate module findings...
	const TCHAR* DuplicateModulesName = TEXT("PSysDuplicateModules");
	OutputStream = GetOutputFile(TEXT("Audit"), DuplicateModulesName);
	if (OutputStream != NULL)
	{
		warnf(NAME_Log, TEXT("Dumping '%s' results..."), DuplicateModulesName);
		OutputStream->Logf(TEXT("Particle System,Module Count,Module Memory,Duplicate Count,Duplicate Memory"));
		for (TMap<FString,FParticleSystemDuplicateModuleInfo>::TIterator DupeIt(PSysDuplicateModuleInfo); DupeIt; ++DupeIt)
		{
			FString PSysName = DupeIt.Key();
			FParticleSystemDuplicateModuleInfo& DupeInfo = DupeIt.Value();

			OutputStream->Logf(TEXT("%s,%d,%d,%d,%d"), *PSysName, 
				DupeInfo.ModuleCount, DupeInfo.ModuleMemory,
				DupeInfo.RemovedDuplicateCount, DupeInfo.RemovedDuplicateMemory);
		}
		OutputStream->Close();
		delete OutputStream;
	}
	else
	{
		warnf(NAME_Warning, TEXT("Failed to open DuplicateModule file %s"), DuplicateModulesName);
	}
}

/**
 *	Dump the give list of particle systems to an audit CSV file...
 *
 *	@param	InPSysMap		The particle system map to dump
 *	@param	InFilename		The name for the output file (short name)
 *
 *	@return	UBOOL			TRUE if successful, FALSE if not
 */
UBOOL UParticleSystemAuditCommandlet::DumpSimplePSysMapping(TMap<FString,UBOOL>& InPSysMap, const TCHAR* InShortFilename)
{
	return DumpSimpleMapping(InPSysMap, &CookedParticleSystems, InShortFilename, TEXT("ParticleSystem"));
}

// Why does the ChangePrefabSequenceClass commandlet not need a main???
INT UParticleSystemAuditCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

//
//	ULensFlareAuditCommandlet
//
IMPLEMENT_CLASS(ULensFlareAuditCommandlet);
/**
 *	Startup the commandlet.
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL ULensFlareAuditCommandlet::Startup()
{
	if (Super::Startup() == TRUE)
	{
		// Gather the list of all cooked lens flares
		FString ClassName = TEXT("LensFlare");
		ClassesOfInterest.AddItem(ClassName);

		// -125..125 will be considered huge...
		FLOAT MaxBoundValue = 250.0f * 250.0f * 250.f;
		FString MaxBound(TEXT(""));
		for (INT TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++)
		{
			FString& Token = Tokens(TokenIndex);
			if (Parse(*Token, TEXT("BOUNDVOL="), MaxBound))
			{
				break;
			}
		}
		if (MaxBound.Len() > 0)
		{
			MaxBoundValue = appAtof(*MaxBound);
		}
		MaxBoundVolume = MaxBoundValue;
		return TRUE;
	}
	return FALSE;
}

/** Process all referenced LensFlares */
UBOOL ULensFlareAuditCommandlet::ProcessReferencedLensFlares()
{
	// Find all LensFlares that are contained in map packages...
	for (TMap<FString, FCookedObjectInfo>::TIterator LensFlareIt(CookedObjects); LensFlareIt; ++LensFlareIt)
	{
		FString FullName = LensFlareIt.Key();
		FCookedObjectInfo& ObjInfo = LensFlareIt.Value();

		UBOOL bLevelPlaced = FALSE;
		for (TMap<FString,INT>::TIterator MapIt(ObjInfo.PackagesCookedInto); MapIt; ++MapIt)
		{
			FString PackageName = MapIt.Key();

			if (MapPackages.Find(PackageName) != NULL)
			{
				bLevelPlaced = TRUE;
				PackageReferencedLensFlares.Set(ObjInfo.ObjectName, TRUE);
				// If it's in at least one, that's all we need to know right now...
				break;
			}
		}

		if (bLevelPlaced == FALSE)
		{
			PackageReferencedLensFlares.Set(ObjInfo.ObjectName, FALSE);
		}
	}

	// Sort the list so it is 'package friendly
	TMap<FString,UBOOL> SortedLensFlareList = PackageReferencedLensFlares;
	SortedLensFlareList.KeySort<COMPARE_CONSTREF_CLASS(FString,UDatabaseCommandlets)>();

	// Find all level placed LensFlares with:
	//	- No fixed bounds
	//	- Huge fixed bounds
	//	- Materials with no occlusion expression
	// 
	FString LastPackageName = TEXT("");
	INT PackageSwitches = 0;
	UPackage* CurrentPackage = NULL;
	for (TMap<FString,UBOOL>::TIterator It(SortedLensFlareList); It; ++It)
	{
		FString LensFlareName = It.Key();
		UBOOL bLevelPlaced = It.Value();
		INT FirstDotIdx = LensFlareName.InStr(TEXT("."));
		if (FirstDotIdx != INDEX_NONE)
		{
			FString PackageName = LensFlareName.Left(FirstDotIdx);
			if (PackageName != LastPackageName)
			{
				UPackage* Package = UObject::LoadPackage(NULL, *PackageName, LOAD_None);
				if (Package != NULL)
				{
					LastPackageName = PackageName;
					Package->FullyLoad();
					CurrentPackage = Package;
				}
				else
				{
					warnf(NAME_Warning, TEXT("Failed to load package %s processing %s"), *PackageName, *LensFlareName);
					CurrentPackage = NULL;
				}
			}

			FString ShorterLensFlareName = LensFlareName.Right(LensFlareName.Len() - FirstDotIdx - 1);
			ULensFlare* LensFlare = FindObject<ULensFlare>(CurrentPackage, *ShorterLensFlareName);
			if (LensFlare != NULL)
			{
				// Note all No fixed bounds
				if (LensFlare->bUseFixedRelativeBoundingBox == FALSE)
				{
					LensFlaresWithNoFixedBounds.Set(LensFlare->GetPathName(), bLevelPlaced);
				}
				else
				{
					// Note all Huge fixed bounds
					FLOAT BoundsVolume = LensFlare->FixedRelativeBoundingBox.GetVolume();
					if (BoundsVolume > MaxBoundVolume)
					{
						LensFlaresWithHugeFixedBounds.Set(LensFlare->GetPathName(), bLevelPlaced);
					}
				}

				// Check for bad screen percentage maps
				{
					UDistributionFloatConstantCurve* ScreenPercentageCurve = Cast<UDistributionFloatConstantCurve>(LensFlare->ScreenPercentageMap.Distribution);
					UBOOL bScreenPercentageIsGood = FALSE;
					if (ScreenPercentageCurve != NULL)
					{
						FLOAT LowValue = ScreenPercentageCurve->GetValue(0.0f);
						FLOAT HighValue = ScreenPercentageCurve->GetValue(1.0f);

						if ((LowValue <= 0.05f) && (HighValue >= 0.95f))
						{
							bScreenPercentageIsGood = TRUE;
						}
					}

					if (bScreenPercentageIsGood == FALSE)
					{
						LensFlaresWithBadScreenPercentageMaps.Set(LensFlare->GetPathName(), bLevelPlaced);
					}
				}

				// Note all materials referenced that do not use an occlusion expression
				CheckLensFlareForMaterialOcclusionIssues(LensFlare);

				if (LastPackageName.Len() > 0)
				{
					if (LastPackageName != LensFlare->GetOutermost()->GetName())
					{
						LastPackageName = LensFlare->GetOutermost()->GetName();
						PackageSwitches++;
					}
				}
				else
				{
					LastPackageName = LensFlare->GetOutermost()->GetName();
				}

				if (PackageSwitches > 10)
				{
					UObject::CollectGarbage(RF_Native);
					PackageSwitches = 0;
				}

			}
			else
			{
				warnf(NAME_Warning, TEXT("Failed to load lens flare %s"), *LensFlareName);
			}
		}
		else
		{
			warnf(NAME_Warning, TEXT("Incomplete path name for %s"), *LensFlareName);
		}
	}

	// Probably don't need to do this, but just in case we have any 'hanging' packages 
	// and more processing steps are added later, let's clean up everything...
	UObject::CollectGarbage(RF_Native);

	return TRUE;
}

/** 
 *	Check the given LensFlare for material occlusion problems
 *
 *	@param	InLensFlare		The lens flare to check
 */
void ULensFlareAuditCommandlet::CheckLensFlareForMaterialOcclusionIssues(ULensFlare* InLensFlare)
{
	if (InLensFlare != NULL)
	{
		// Check each material
		TArray<UMaterialInterface*> UsedMaterials;
		TArray<UMaterialInterface*> MaterialsMissingOcclusion;

		for (INT ElementIdx = -1; ElementIdx < InLensFlare->Reflections.Num(); ElementIdx++)
		{
			FLensFlareElement* Element = NULL;
			if (ElementIdx == -1)
			{
				Element = &(InLensFlare->SourceElement);
			}
			else
			{
				Element = &(InLensFlare->Reflections(ElementIdx));
			}

			if ((Element != NULL) && (Element->bIsEnabled == TRUE))
			{
				for (INT MtrlIdx = 0; MtrlIdx < Element->LFMaterials.Num(); MtrlIdx++)
				{
					UMaterialInterface* MtrlIntf = Element->LFMaterials(MtrlIdx);
					if (MtrlIntf != NULL)
					{
						UsedMaterials.AddUniqueItem(MtrlIntf);
					}
				}
			}
		}

		// Now check each material for an OcclusionPercentage expression
		for (INT CheckIdx = 0; CheckIdx < UsedMaterials.Num(); CheckIdx++)
		{
			UMaterialInterface* MtrlIntf = UsedMaterials(CheckIdx);
			UMaterial* Material = Cast<UMaterial>(MtrlIntf);
			UMaterialInstance* MaterialInst = Cast<UMaterialInstance>(MtrlIntf);

			if (MaterialInst != NULL)
			{
				while ((Material == NULL) && (MaterialInst != NULL))
				{
					UMaterial* ParentMaterial = Cast<UMaterial>(MaterialInst->Parent);
					UMaterialInstance* ParentMaterialInst = Cast<UMaterialInstance>(MaterialInst->Parent);

					if (ParentMaterial != NULL)
					{
						Material = ParentMaterial;
					}
					else
					{
						MaterialInst = ParentMaterialInst;
					}
				}
			}

			if (Material != NULL)
			{
				const INT NumPropsToCheck = 5;
				EMaterialProperty PropertiesToCheck[NumPropsToCheck] = 
				{
					MP_EmissiveColor,
					MP_Opacity,
					MP_OpacityMask,
					MP_DiffuseColor,
					MP_DiffusePower
				};

				UBOOL bHasOcclusionExpression = FALSE;
				for (INT PropIdx = 0; (PropIdx < NumPropsToCheck) && !bHasOcclusionExpression; PropIdx++)
				{
					TArray<UMaterialExpression*> ExpressionChain;
					if (Material->GetExpressionsInPropertyChain(PropertiesToCheck[PropIdx], ExpressionChain, NULL) == TRUE)
					{
						// See if there is an OcclusionPercentage or LensFlareOcclusion expression
						for (INT ExpIdx = 0; (ExpIdx < ExpressionChain.Num()) && !bHasOcclusionExpression; ExpIdx++)
						{
							UMaterialExpressionOcclusionPercentage* OcclusionPercentageExp = Cast<UMaterialExpressionOcclusionPercentage>(ExpressionChain(ExpIdx));
							UMaterialExpressionLensFlareOcclusion* LensFlareOcclusionExp = Cast<UMaterialExpressionLensFlareOcclusion>(ExpressionChain(ExpIdx));

							if ((OcclusionPercentageExp != NULL) || (LensFlareOcclusionExp != NULL))
							{
								bHasOcclusionExpression = TRUE;
							}
						}
					}
				}

				if (bHasOcclusionExpression == FALSE)
				{
					MaterialsMissingOcclusion.AddUniqueItem(Material);
				}
			}
		}

		if (MaterialsMissingOcclusion.Num() > 0)
		{
			FLensFlareOcclusionInfo* OcclusionInfo = LensFlaresWithOcclusionMissing.Find(InLensFlare->GetPathName());
			if (OcclusionInfo == NULL)
			{
				FLensFlareOcclusionInfo TempOcclusionInfo;
				LensFlaresWithOcclusionMissing.Set(InLensFlare->GetPathName(), TempOcclusionInfo);
				OcclusionInfo = LensFlaresWithOcclusionMissing.Find(InLensFlare->GetPathName());
			}
			check(OcclusionInfo);
			for (INT BadIdx = 0; BadIdx < MaterialsMissingOcclusion.Num(); BadIdx++)
			{
				UMaterialInterface* BadMaterial = MaterialsMissingOcclusion(BadIdx);
				if (BadMaterial != NULL)
				{
					OcclusionInfo->NoOcclusionMaterials.AddUniqueItem(BadMaterial->GetPathName());
				}
			}
		}
	}
}

/**
 *	The main function of the commandlet...
 *
 */
UBOOL ULensFlareAuditCommandlet::ProcessMinedData()
{
	DOUBLE StartProcessLensFlaresTime = appSeconds();
	if (ProcessReferencedLensFlares() == FALSE)
	{
		warnf(NAME_Warning, TEXT("ProcessReferencedLensFlares failed..."));
	}
	DOUBLE ProcessLensFlaresTime = appSeconds() - StartProcessLensFlaresTime;
	warnf(NAME_Log, TEXT("Took %5.3f seconds to process referenced LensFlares..."), ProcessLensFlaresTime);

	return TRUE;
}

/** Dump the results of the audit */
void ULensFlareAuditCommandlet::DumpResults()
{
	Super::DumpResults();

	// Dump all the simple mappings...
	DumpSimpleLensFlareMapping(LensFlaresWithNoFixedBounds, TEXT("LensFlareNoFixedBounds"));
	DumpSimpleLensFlareMapping(LensFlaresWithHugeFixedBounds, TEXT("LensFlareHugeFixedBounds"));
	DumpSimpleLensFlareMapping(LensFlaresWithBadScreenPercentageMaps, TEXT("LensFlareBadScreenPercentage"));

	FArchive* OutputStream;

	// Dump out the LensFlares w/ materials that have no occlusion expression
	const TCHAR* NoOcclusionName = TEXT("LensFlareNoOcclusion");
	OutputStream = GetOutputFile(TEXT("Audit"), NoOcclusionName);
	if (OutputStream != NULL)
	{
		warnf(NAME_Log, TEXT("Dumping '%s' results..."), NoOcclusionName);
		OutputStream->Logf(TEXT("LensFlare,Materials"));
		for (TMap<FString,FLensFlareOcclusionInfo>::TIterator DumpIt(LensFlaresWithOcclusionMissing); DumpIt; ++DumpIt)
		{
			FString LensFlareName = DumpIt.Key();
			FLensFlareOcclusionInfo& LODInfo = DumpIt.Value();

			FString Output = FString::Printf(TEXT("%s"), *LensFlareName);
			for (INT MaterialIdx = 0; MaterialIdx < LODInfo.NoOcclusionMaterials.Num(); MaterialIdx++)
			{
				Output += FString::Printf(TEXT(",%s"), *(LODInfo.NoOcclusionMaterials(MaterialIdx)));
			}
			OutputStream->Logf(*Output);
		}
		OutputStream->Close();
		delete OutputStream;
	}
	else
	{
		warnf(NAME_Warning, TEXT("Failed to open occlusion issues file %s"), NoOcclusionName);
	}
}

/**
 *	Dump the give list of LensFlares to an audit CSV file...
 *
 *	@param	InLensFlareMap		The lens flare map to dump
 *	@param	InFilename			The name for the output file (short name)
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not
 */
UBOOL ULensFlareAuditCommandlet::DumpSimpleLensFlareMapping(TMap<FString,UBOOL>& InLensFlareMap	, const TCHAR* InShortFilename)
{
	return DumpSimpleMapping(InLensFlareMap, &CookedObjects, InShortFilename, TEXT("LensFlare"));
}

INT ULensFlareAuditCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

//
//	UMaterialAuditCommandlet
//
IMPLEMENT_CLASS(UMaterialAuditCommandlet);
/**
 *	Startup the commandlet.
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UMaterialAuditCommandlet::Startup()
{
	if (Super::Startup() == TRUE)
	{
		// Gather the list of all cooked lens flares
		FString ClassName = TEXT("Material");
		ClassesOfInterest.AddItem(ClassName);
		return TRUE;
	}
	return FALSE;
}

/** Process all referenced materials */
UBOOL UMaterialAuditCommandlet::ProcessReferencedMaterials()
{
	// Find all materials that are contained into packages...
	for (TMap<FString, FCookedObjectInfo>::TIterator MaterialIt(CookedObjects); MaterialIt; ++MaterialIt)
	{
		FString FullName = MaterialIt.Key();
		FCookedObjectInfo& ObjInfo = MaterialIt.Value();

		UBOOL bLevelPlaced = FALSE;
		for (TMap<FString,INT>::TIterator MapIt(ObjInfo.PackagesCookedInto); MapIt; ++MapIt)
		{
			FString PackageName = MapIt.Key();

			if (MapPackages.Find(PackageName) != NULL)
			{
				bLevelPlaced = TRUE;
				PackageReferencedMaterials.Set(ObjInfo.ObjectName, TRUE);
				// If it's in at least one, that's all we need to know right now...
				break;
			}
		}

		if (bLevelPlaced == FALSE)
		{
			PackageReferencedMaterials.Set(ObjInfo.ObjectName, FALSE);
		}
	}

	// Sort the list so it is 'package friendly
	TMap<FString,UBOOL> SortedMaterials = PackageReferencedMaterials;
	SortedMaterials.KeySort<COMPARE_CONSTREF_CLASS(FString,UDatabaseCommandlets)>();

	FString LastPackageName = TEXT("");
	INT PackageSwitches = 0;
	UPackage* CurrentPackage = NULL;
	for (TMap<FString,UBOOL>::TIterator It(SortedMaterials); It; ++It)
	{
		FString MaterialName = It.Key();
		UBOOL bLevelPlaced = It.Value();
		INT FirstDotIdx = MaterialName.InStr(TEXT("."));
		if (FirstDotIdx != INDEX_NONE)
		{
			FString PackageName = MaterialName.Left(FirstDotIdx);
			if (PackageName != LastPackageName)
			{
				if (LastPackageName.Len() > 0)
				{
					PackageSwitches++;
				}
				if (PackageSwitches > 10)
				{
					UObject::CollectGarbage(RF_Native);
					PackageSwitches = 0;
					SaveLocalShaderCaches();
				}

				UPackage* Package = UObject::LoadPackage(NULL, *PackageName, LOAD_None);
				if (Package != NULL)
				{
					LastPackageName = PackageName;
					Package->FullyLoad();
					CurrentPackage = Package;
				}
				else
				{
					warnf(NAME_Warning, TEXT("Failed to load package %s processing %s"), *PackageName, *MaterialName);
					CurrentPackage = NULL;
				}
			}

			FString ShorterMaterialName = MaterialName.Right(MaterialName.Len() - FirstDotIdx - 1);
			UMaterial* Material = FindObject<UMaterial>(CurrentPackage, *ShorterMaterialName);
			if (Material != NULL)
			{
				UBOOL bHasObsoleteDepthBiasExpressions = FALSE;
				TArray<UMaterialExpression*> ReferencedExpressions;
				if (Material->GetAllReferencedExpressions(ReferencedExpressions, NULL) == TRUE)
				{
					// Check each one
					for (INT ExpIdx = 0; ExpIdx < ReferencedExpressions.Num(); ExpIdx++)
					{
						UMaterialExpressionDepthBiasBlend* DepthBiasBlendExpression = Cast<UMaterialExpressionDepthBiasBlend>(ReferencedExpressions(ExpIdx));
						UMaterialExpressionDepthBiasedBlend* DepthBiasedBlendExpression = Cast<UMaterialExpressionDepthBiasedBlend>(ReferencedExpressions(ExpIdx));
						if (DepthBiasBlendExpression || DepthBiasedBlendExpression)
						{
							bHasObsoleteDepthBiasExpressions = TRUE;
							break;
						}
					}
				}
				else
				{
					warnf(NAME_Warning, TEXT("Failed to retrieve expressions from material %s"), *MaterialName);
				}

				if (bHasObsoleteDepthBiasExpressions == TRUE)
				{
					MaterialsUsingObsoleteDepthBiasExpressions.Set(Material->GetPathName(), bLevelPlaced);
				}
			}
			else
			{
				warnf(NAME_Warning, TEXT("Failed to load material %s"), *MaterialName);
			}
		}
		else
		{
			warnf(NAME_Warning, TEXT("Incomplete path name for %s"), *MaterialName);
		}
	}

	// Probably don't need to do this, but just in case we have any 'hanging' packages 
	// and more processing steps are added later, let's clean up everything...
	UObject::CollectGarbage(RF_Native);

	return TRUE;
}

/**
 *	The main function of the commandlet...
 *
 */
UBOOL UMaterialAuditCommandlet::ProcessMinedData()
{
	DOUBLE StartProcessTime = appSeconds();
	if (ProcessReferencedMaterials() == FALSE)
	{
		warnf(NAME_Warning, TEXT("ProcessReferencedMaterials failed..."));
	}
	DOUBLE ProcessTime = appSeconds() - StartProcessTime;
	warnf(NAME_Log, TEXT("Took %5.3f seconds to process referenced materials..."), ProcessTime);

	return TRUE;
}

/** Dump the results of the audit */
void UMaterialAuditCommandlet::DumpResults()
{
	Super::DumpResults();

	// Dump all the simple mappings...
	DumpSimpleMapping(MaterialsUsingObsoleteDepthBiasExpressions, &CookedObjects, TEXT("MtrlWithObsoleteDepthBias"), TEXT("Material"));
}

INT UMaterialAuditCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

//
//	UAnalyzeDVDSpaceCommandlet
//
IMPLEMENT_CLASS(UAnalyzeDVDSpaceCommandlet);

/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UAnalyzeDVDSpaceCommandlet::Initialize()
{
	if (Super::Initialize() == TRUE)
	{
		INT DummyIndex;
		bIgnoreTFCEntries = Switches.FindItem(TEXT("IGNORETFC"), DummyIndex);
		if (bIgnoreTFCEntries == TRUE)
		{
			warnf(NAME_Log, TEXT("Ignoring TFC entries!!!"));
		}
		return TRUE;
	}
	return FALSE;
}

/**
 *	Mine the given database for the objects of interest
 *
 *	@param	UBOOL	TRUE if database connection was opened, FALSE if not
 */
UBOOL UAnalyzeDVDSpaceCommandlet::MineDatabaseForObjects()
{
	// If we don't have a DB connection, we are doomed...
	if (DBConnection == NULL)
	{
		return FALSE;
	}

	// Gather all the cooked asset info
	if (GatherAllCookedAssetInfo(CookedObjectInfo, bIgnoreColon, &PackageCookedObjectInfo, &UniqueCookedPackages) == FALSE)
	{
		return FALSE;
	}

	return TRUE;
}

/**
 *	The main function of the commandlet...
 *
 */
UBOOL UAnalyzeDVDSpaceCommandlet::ProcessMinedData()
{
	return TRUE;
}

/** Dump the results of the audit */
void UAnalyzeDVDSpaceCommandlet::DumpResults()
{
	Super::DumpResults();

	warnf(NAME_Log, TEXT("<-------------------------------------"));
	warnf(NAME_Log, TEXT("Found %d unique cooked packages"), UniqueCookedPackages.Num());
	warnf(NAME_Log, TEXT("<-------------------------------------"));

	DumpCookedObjectList(TEXT("AnalyzeDVDSpace"), CookedObjectInfo);
}

/**
 *	Dump the given cooked object list to the given file
 *
 *	@param	InShortFilename		The short filename to dump the info to
 *	@param	InCookedObjInfo		The map of cooked object info to dump to the file
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not
 */
UBOOL UAnalyzeDVDSpaceCommandlet::DumpCookedObjectList(const TCHAR* InShortFilename, TMap<FString,FCookedObjectInfo>& InCookedObjInfo)
{
	FArchive* OutputStream = GetOutputFile(TEXT("Audit"), InShortFilename);
	if (OutputStream == NULL)
	{
		warnf(NAME_Warning, TEXT("Failed to create output stream %s"), *InShortFilename);
		warnf(NAME_Log, TEXT("Class,Object,AvgSize,MapCount,TotalSize,Package,Size,..."));
	}
	else
	{
		OutputStream->Logf(TEXT("Class,Object,AvgSize,MapCount,TotalSize,Package,Size,..."));
	}

	for (TMap<FString,FCookedObjectInfo>::TIterator It(InCookedObjInfo); It; ++It)
	{
		FCookedObjectInfo& ObjInfo = It.Value();

		UBOOL bLogIt = TRUE;
		if (bIgnoreTFCEntries == TRUE)
		{
			if (ObjInfo.ClassName == TEXT("Texture2D"))
			{
				if (ObjInfo.ObjectName.StartsWith(TEXT("TFC_")) == TRUE)
				{
					bLogIt = FALSE;
				}
			}
		}

		if (bLogIt == TRUE)
		{
			FString Output = FString::Printf(TEXT("%s,%s,%d,%d,%d"), 
				*(ObjInfo.ClassName),
				*(ObjInfo.ObjectName),
				appCeil((FLOAT)ObjInfo.TotalSerializeSize / ObjInfo.PackagesCookedInto.Num()), 
				ObjInfo.PackagesCookedInto.Num(),
				ObjInfo.TotalSerializeSize);
			for (TMap<FString,INT>::TIterator DumpIt(ObjInfo.PackagesCookedInto); DumpIt; ++DumpIt)
			{
				Output += FString::Printf(TEXT(",%s,%d"), *(DumpIt.Key()), DumpIt.Value());
			}
			if (OutputStream != NULL)
			{
				OutputStream->Logf(*Output);
			}
			else
			{
				warnf(NAME_Log, *Output);
			}
		}
	}

	if (OutputStream != NULL)
	{
		OutputStream->Close();
		delete OutputStream;
	}

	return TRUE;
}

// Why does the ChangePrefabSequenceClass commandlet not need a main???
INT UAnalyzeDVDSpaceCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

//
//	UFindCinematicTexturesCommandlet
//
IMPLEMENT_CLASS(UFindCinematicTexturesCommandlet);

/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UFindCinematicTexturesCommandlet::Initialize()
{
	if (Super::Initialize() == TRUE)
	{
		FString ClassName = TEXT("Texture2D");
		ClassesOfInterest.AddItem(ClassName);
		return TRUE;
	}
	return FALSE;
}

/**
 *	The main function of the commandlet...
 *
 */
UBOOL UFindCinematicTexturesCommandlet::ProcessMinedData()
{
	TMap<FString,UBOOL> CompleteTexturelist;
	// Ignore the TFC_ textures...
	for (TMap<FString, FCookedObjectInfo>::TIterator AssetIt(CookedObjects); AssetIt; ++AssetIt)
	{
		FString FullName = AssetIt.Key();
		FCookedObjectInfo& ObjInfo = AssetIt.Value();

		// Skip 'tfc' textures that are in the database
		if (ObjInfo.ObjectName.StartsWith(TEXT("TFC_")) == FALSE)
		{
			// Add it to a list off textures
			CompleteTexturelist.Set(ObjInfo.ObjectName, FALSE);
		}
	}

	CompleteTexturelist.KeySort<COMPARE_CONSTREF_CLASS(FString,UDatabaseCommandlets)>();

	FString LastPackageName = TEXT("");
	INT PackageSwitches = 0;
	UPackage* CurrentPackage = NULL;
	for (TMap<FString,UBOOL>::TIterator It(CompleteTexturelist); It; ++It)
	{
		FString TextureName = It.Key();
		INT FirstDotIdx = TextureName.InStr(TEXT("."));
		if (FirstDotIdx != INDEX_NONE)
		{
			FString PackageName = TextureName.Left(FirstDotIdx);
			if (PackageName != LastPackageName)
			{
				// If we are loading a new package, and we've loaded 10 already, GC
				if (LastPackageName.Len() > 0)
				{
					LastPackageName = PackageName;
					PackageSwitches++;
				}
				else
				{
					LastPackageName = PackageName;
				}

				if (PackageSwitches > 10)
				{
					UObject::CollectGarbage(RF_Native);
					PackageSwitches = 0;
				}

				UPackage* Package = UObject::LoadPackage(NULL, *PackageName, LOAD_None);
				if (Package != NULL)
				{
					LastPackageName = PackageName;
					Package->FullyLoad();
					CurrentPackage = Package;
				}
				else
				{
					warnf(NAME_Warning, TEXT("Failed to load package %s processing %s"), *PackageName, *TextureName);
					CurrentPackage = NULL;
				}
			}

			FString ShorterTextureName = TextureName.Right(TextureName.Len() - FirstDotIdx - 1);
			UTexture2D* CurrentTexture = FindObject<UTexture2D>(CurrentPackage, *ShorterTextureName);
			if (CurrentTexture != NULL)
			{
				if (CurrentTexture->LODGroup == TEXTUREGROUP_Cinematic)
				{
					CinematicCookedTextures.Set(TextureName, TRUE);
				}
			}
			else
			{
				warnf(NAME_Warning, TEXT("Failed to load texture %s"), *TextureName);
			}
		}
		else
		{
			warnf(NAME_Warning, TEXT("Incomplete path name for %s"), *TextureName);
		}
	}

	return TRUE;
}

/** Dump the results of the audit */
void UFindCinematicTexturesCommandlet::DumpResults()
{
	if (CinematicCookedTextures.Num() > 0)
	{
		FArchive* OutputStream = GetOutputFile(TEXT("Audit"), TEXT("CinematicTextures"));
		if (OutputStream != NULL)
		{
			for (TMap<FString,UBOOL>::TIterator DumpIt(CinematicCookedTextures); DumpIt; ++DumpIt)
			{
				FString TextureName = DumpIt.Key();
				OutputStream->Logf(TEXT("Texture2D %s"), *TextureName);
			}

			OutputStream->Close();
			delete OutputStream;
		}
	}
}

INT UFindCinematicTexturesCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

/*-----------------------------------------------------------------------------
	PMapForceObjectCheck commandlet.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UPMapForceObjectCheckCommandlet);

/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UPMapForceObjectCheckCommandlet::Initialize()
{
	if (Super::Initialize() == TRUE)
	{
		SubLevelPercentage = 70.0f;
		const TCHAR* PercentageTag = TEXT("PERCENTAGE=");
		const TCHAR* SkipTag = TEXT("SKIPWILDCARD=");
		for (INT SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++)
		{
			FString CurrSwitch = Switches(SwitchIdx);
			INT PercentageIndex = CurrSwitch.InStr(PercentageTag, FALSE, TRUE);
			if (PercentageIndex != INDEX_NONE)
			{
				FString PercentageStr = CurrSwitch.Right(CurrSwitch.Len() - appStrlen(PercentageTag));
				SubLevelPercentage = appAtof(*PercentageStr) / 100.0f;
			}
			INT SkipIndex = CurrSwitch.InStr(SkipTag, FALSE, TRUE);
			if (SkipIndex != INDEX_NONE)
			{
				FString SkipStr = CurrSwitch.Right(CurrSwitch.Len() - appStrlen(SkipTag));
				INT PlusIndex = SkipStr.InStr(TEXT("+"));
				while (PlusIndex != INDEX_NONE)
				{
					FString SkipEntry = SkipStr.Left(PlusIndex);
					SkipNameFragments.AddItem(SkipEntry);
					SkipStr = SkipStr.Right(SkipStr.Len() - PlusIndex - 1);
					PlusIndex = SkipStr.InStr(TEXT("+"));
				}
				SkipNameFragments.AddItem(SkipStr);
			}
		}

		INT DummyIdx;
		bClassesOnly = Switches.FindItem(TEXT("CLASSESONLY"), DummyIdx);
		bScriptLevelsOnly = Switches.FindItem(TEXT("SCRIPTLEVELSONLY"), DummyIdx);
		bOutputObjectLists = Switches.FindItem(TEXT("DUMPOBJS"), DummyIdx);

		warnf(NAME_Log, TEXT("Sublevel percentage is %5.2f"), SubLevelPercentage * 100.0f);
		return TRUE;
	}
	return FALSE;
}

/**
 *	Mine the given database for the objects of interest
 *
 *	@param	UBOOL	TRUE if database connection was opened, FALSE if not
 */
UBOOL UPMapForceObjectCheckCommandlet::MineDatabaseForObjects()
{
	// If we don't have a DB connection, we are doomed...
	if (DBConnection == NULL)
	{
		return FALSE;
	}

	// This commandlet does its processing as it mines...
	TArray<FString> PMapList;
	if (GeneratePMapList(PMapList) == TRUE)
	{
		FArchive* HelperStream = GetOutputFile(TEXT("Audit"), TEXT("PMapForcedObjects"));
		FArchive* PercentageStream = GetOutputFile(TEXT("Audit"), TEXT("PMapForcedPercentage"));
		FArchive* OutputStream = NULL;
		if (bOutputObjectLists == TRUE)
		{
			OutputStream = GetOutputFile(TEXT("Audit"), TEXT("PMap_ObjectDump"));
			if (OutputStream != NULL)
			{
				OutputStream->Logf(TEXT("Object,Maps Cooked Into,..."));
			}
		}

		// For each PMap, gather it's assets...
		for (INT PMapIdx = 0; PMapIdx < PMapList.Num(); PMapIdx++)
		{
			FString PMapName = PMapList(PMapIdx);
			const TArray<FString>* LevelList = PersistentMapInfoHelper.GetPersistentMapContainedLevelsList(PMapName);
			if (LevelList != NULL)
			{
				// The level list does *not* include that actual PMap...
				FString ShortPMapName = FFilename(PMapName).GetBaseFilename();
				TArray<FString> PackageList;
				for (INT SubLevelIdx = 0; SubLevelIdx < LevelList->Num(); SubLevelIdx++)
				{
					FString SubLevelName = (*LevelList)(SubLevelIdx);
					UBOOL bSkipMap = FALSE;
					for (INT SkipIdx = 0; SkipIdx < SkipNameFragments.Num(); SkipIdx++)
					{
						if (SubLevelName.InStr(SkipNameFragments(SkipIdx), FALSE, TRUE) != INDEX_NONE)
						{
							bSkipMap = TRUE;
							break;
						}

						if ((bSkipMap == FALSE) && (bScriptLevelsOnly == TRUE))
						{
							// See if it ends w/ "_S"...
							if (SubLevelName.EndsWith(TEXT("_S")) == FALSE)
							{
								bSkipMap = TRUE;
								break;
							}
						}

						if (bSkipMap == FALSE)
						{
							PackageList.AddItem(SubLevelName);
						}
					}

					INT SubLevelCount = PackageList.Num();

					TMap<FString,FPackageCookedObjectInfo> LocalPackageCookedObjectInfo;
					TMap<FString,FCookedObjectInfo> CookedInfoMap;
					UBOOL bResult = FALSE;
					if (bClassesOnly == TRUE)
					{
						TArray<FString> ClassNames;
						ClassNames.AddItem(TEXT("Class"));
						bResult = GatherSpecifiedCookedAssetsInfo(ClassNames, PackageList, CookedInfoMap, TRUE, &LocalPackageCookedObjectInfo, NULL);
					}
					else
					{
						bResult = GatherAllCookedAssetInfoInPackages(PackageList, CookedInfoMap, TRUE, &LocalPackageCookedObjectInfo, NULL);
					}

					if (bResult == TRUE)
					{
						// Examine the cooked into...
						warnf(NAME_Log, TEXT("[Cooker.ForcedMapObjects.%s]"), *ShortPMapName);
						if (HelperStream != NULL)
						{
							HelperStream->Logf(TEXT("[Cooker.ForcedMapObjects.%s]"), *ShortPMapName);
						}
						if (PercentageStream != NULL)
						{
							PercentageStream->Logf(TEXT("ForcedMapObjects %s]"), *ShortPMapName);
						}
						for (TMap<FString,FCookedObjectInfo>::TIterator AssetIt(CookedInfoMap); AssetIt; ++AssetIt)
						{
							FString& ObjName = AssetIt.Key();
							FCookedObjectInfo& ObjInfo = AssetIt.Value();

							FLOAT Percentage = (FLOAT)ObjInfo.PackagesCookedInto.Num() / (FLOAT)SubLevelCount;
							if (Percentage >= SubLevelPercentage)
							{
								FString UpperClassName = ObjInfo.ClassName.ToUpper();
								if (UpperClassName != TEXT("PACKAGE"))
								{
									if (ObjInfo.ObjectName.StartsWith(TEXT("TFC_")) == FALSE)
									{
										warnf(NAME_Log, TEXT("+Object=%s (%4.3f - %2d out of %2d)"), *(ObjInfo.ObjectName), 
											Percentage, ObjInfo.PackagesCookedInto.Num(), SubLevelCount);
										if (PercentageStream != NULL)
										{
											PercentageStream->Logf(TEXT("%s,%4.3f,%2d,%2d)"), *(ObjInfo.ObjectName), 
												Percentage, ObjInfo.PackagesCookedInto.Num(), SubLevelCount);
										}
										if (HelperStream != NULL)
										{
											HelperStream->Logf(TEXT("+Object=%s"), *(ObjInfo.ObjectName));
										}
									}
								}
							}

							if (OutputStream != NULL)
							{
								FString OutputString = ObjInfo.ObjectName;
								for (TMap<FString,INT>::TIterator MapIt(ObjInfo.PackagesCookedInto); MapIt; ++MapIt)
								{
									OutputString += FString::Printf(TEXT(",%s"), *(MapIt.Key()));
								}
								OutputStream->Logf(*OutputString);
							}
						}
						warnf(NAME_Log, TEXT(""));
						if (HelperStream != NULL)
						{
							HelperStream->Logf(TEXT(""));
						}
						if (PercentageStream != NULL)
						{
							PercentageStream->Logf(TEXT(""));
						}
					}
				}
			}
		}

		if (PercentageStream != NULL)
		{
			PercentageStream->Close();
			delete PercentageStream;
		}
		if (HelperStream != NULL)
		{
			HelperStream->Close();
			delete HelperStream;
		}
		if (OutputStream != NULL)
		{
			OutputStream->Close();
			delete OutputStream;
		}
	}

	return TRUE;
}

/**
 *	The main function of the commandlet...
 *
 */
UBOOL UPMapForceObjectCheckCommandlet::ProcessMinedData()
{
	// This commandlet does its processing as it mines...
	return TRUE;
}

/** Dump the results of the audit */
void UPMapForceObjectCheckCommandlet::DumpResults()
{
	// This commandlet does its processing as it mines...
}

INT UPMapForceObjectCheckCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}
