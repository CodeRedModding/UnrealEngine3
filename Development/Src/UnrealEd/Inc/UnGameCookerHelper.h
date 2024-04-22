/*=============================================================================
	UnGameCookerHelper.h: Game specific editor cooking helper class declarations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __GAMEEDITORCOOKERHELPER_H__
#define __GAMEEDITORCOOKERHELPER_H__

class FGameContentCookerHelper
{
public:
	/**
	 *	Load the object or class of the given name.
	 *
	 *	@param	Commandlet		The commandlet being run
	 *	@param	ObjectName		The name of the object/class to attempt to load
	 *	@param	ObjReferencer	If not NULL, the object referencer to add the loaded object/class to
	 *
	 *	@return	UObject*		The loaded object/class, NULL if not found
	 */
	UObject* LoadObjectOrClass(class UCookPackagesCommandlet* Commandlet, const FString& ObjectName, UObjectReferencer* ObjRefeferencer);

	/**
	 *	Mark the given object as cooked
	 *
	 *	@param	InObject	The object to mark as cooked.
	 */
	void MarkObjectAsCooked(UObject* InObject);

	/**
	 *	Called when the FPersistentMapInfo class has been initialized and filled in.
	 *	Can be used to do any special setup for PMaps the cooker may require.
	 *
	 *	@param	Commandlet		The commandlet that is being run.
	 */
	virtual void PersistentMapInfoGeneratedCallback(class UCookPackagesCommandlet* Commandlet) { };

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
	INT GenerateLoadedObjectList(class UCookPackagesCommandlet* Commandlet, const TArray<FString>& InObjectsToLoad, TMap<FString,INT>& OutLoadedObjectMap);
};

/**
* Helper for adding game content standalone packages for cooking
*/
class FGenerateGameContentSeekfree : public FGameContentCookerHelper
{
public:
	/**
	 * Game content package entry
	 */
	struct FGameContentEntry
	{
		FGameContentEntry()
			: PackageCookerInfo(NULL,NULL,FALSE,FALSE,FALSE,FALSE,FALSE)
		{
		}
		/** list of content strings that are going to be loaded */
		TArray<FString> ContentList;
		/** list of content strings that are forced into every package */
		TArray<FString> ForcedContentList;
		/** package list entry generated for the game type */
		FPackageCookerInfo PackageCookerInfo;
	};

	/**
	 * Constructor
	 */
	FGenerateGameContentSeekfree()
		: bForceRecookSeekfreeGameTypes(FALSE)
	{
		Init();
	}

	/**
	 * Initialize options from command line options
	 *
	 *	@param	Tokens			Command line tokens parsed from app
	 *	@param	Switches		Command line switches parsed from app
	 */
	void InitOptions(const TArray<FString>& Tokens, const TArray<FString>& Switches);

	/**
	 *	Check if the given object or any of its dependencies are newer than the given time
	 *
	 *	@param	Commandlet		The commandlet being run
	 *	@param	ContentStr		The name of the object in question
	 *	@param	CheckTime		The time to check against
	 *	@param	bIsSeekfreeFileDependencyNewerMap	A mapping of base package times to prevent repeated reloads of the same content
	 *
	 *	@return	UBOOL			TRUE if the content (or any of its dependencies) is newer than the specified time
	 */
	UBOOL CheckIfContentIsNewer(
		class UCookPackagesCommandlet* Commandlet,
		const FString& ContentStr, 
		DOUBLE CheckTime,
		TMap<FString,UBOOL>& bIsSeekfreeFileDependencyNewerMap);

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
	UBOOL GeneratePackageListForContentMap( 
		TMap<FString,FGameContentEntry>& InContentMap,
		class UCookPackagesCommandlet* Commandlet, 
		UE3::EPlatformType Platform,
		EShaderPlatform ShaderPlatform,
		TArray<FPackageCookerInfo>& NotRequiredFilenamePairs,
		TArray<FPackageCookerInfo>& RegularFilenamePairs,
		TArray<FPackageCookerInfo>& MapFilenamePairs,
		TArray<FPackageCookerInfo>& ScriptFilenamePairs,
		TArray<FPackageCookerInfo>& StartupFilenamePairs,
		TArray<FPackageCookerInfo>& StandaloneSeekfreeFilenamePairs);

	/**
	 * Adds required standalone seekfree entries to the package list 
	 *
	 * @return TRUE if succeeded
	 */
	UBOOL GeneratePackageList( 
		class UCookPackagesCommandlet* Commandlet, 
		UE3::EPlatformType Platform,
		EShaderPlatform ShaderPlatform,
		TArray<FPackageCookerInfo>& NotRequiredFilenamePairs,
		TArray<FPackageCookerInfo>& RegularFilenamePairs,
		TArray<FPackageCookerInfo>& MapFilenamePairs,
		TArray<FPackageCookerInfo>& ScriptFilenamePairs,
		TArray<FPackageCookerInfo>& StartupFilenamePairs,
		TArray<FPackageCookerInfo>& StandaloneSeekfreeFilenamePairs);

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
	virtual UBOOL CookObject(class UCookPackagesCommandlet* Commandlet, UPackage* Package, UObject* Object, UBOOL bIsSavedInSeekFreePackage);

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
	UBOOL PreLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, const TCHAR* Filename);

	/**
	 *	Create the package for the given filename if it is one that is generated
	 *	
	 *	@param	Commandlet		The cookpackages commandlet being run
	 *	@param	InContentMap	The content map to check
	 *	@param	Filename		Current filename that needs to be loaded for cooking
	 *	
	 *	@return	UPakcage*		The create package; NULL if failed or invalid
	 */
	UPackage* CreateContentPackageForCooking(class UCookPackagesCommandlet* Commandlet, TMap<FString,FGameContentEntry>& InContentMap, const TCHAR* Filename);

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
	UPackage* LoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, const TCHAR* Filename);

	/**
	* Not used
	*/
	UBOOL PostLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, UPackage* InPackage)
	{
		return TRUE;
	}

	/**
	 *	Dump out stats specific to the game cooker helper.
	 */
	virtual void DumpStats();

	/** 
	 *	Returns true if a package is a GameContent package
	 *
	 *	@param InPackage			Package to check	
	 */
	UBOOL IsPackageGameContent(UPackage* InPackage);

	/** 
	 *	Returns true if a package is a GameContent Common package, that is content shared between several GameContent packages
	 *
	 *	@param InPackage			Package to check
	 */
	UBOOL IsPackageGameCommonContent(UPackage* InPackage);

private:

	/**
	* Initializes the list of game content packages that need to generated. 
	* Game types along with the content to be loaded for each is loaded from ini.
	* Set in [Cooker.MPGameContentCookStandalone] section
	*/
	void Init();
	/**
	 *	Checks to see if any of the map prefix --> gametypes use a common package.
	 *	If so, it will store off the information required to create it.
	 */
	void InitializeCommonPrefixPackages();

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
	UBOOL GenerateCommonPackageData(class UCookPackagesCommandlet* Commandlet, const TCHAR* Filename);

	/** if TRUE then dependency checking is disabled when cooking the game content packages */
	UBOOL bForceRecookSeekfreeGameTypes;

protected:
	/** game string mapping to content entries for generating packages */
	TMap<FString,FGameContentEntry> GameContentMap;
	/** game common string mapping to content entries for generating packages */
	TMap<FString,FGameContentEntry> GameTypeCommonContentMap;

	/** The current common content for the map being cooked... */
	FForceCookedInfo* CurrentCommonPackageInfo;

	/**
	* Adds a unique entry for the given game type
	* Adds the content string for each game type given
	*
	*	@param	InGameStr		game string used as base for package filename
	*	@param	InContentStr	content to be loaded for the game type package
	*	@param	bForced			If TRUE, put the item in the forced content list, otherwise the contentlist
	*/
	void AddGameContentEntry(TMap<FString,FGameContentEntry>& InContentMap, const TCHAR* InGameStr,const TCHAR* InContentStr, UBOOL bForced);
};

/**
 *	Helper for forcing content into maps/packages
 */
class FForcedContentHelper : public FGameContentCookerHelper
{
public:
	/**
	 * Constructor
	 */
	FForcedContentHelper() : 
		  bPMapForcedObjectsEnabled(FALSE)
		, CurrentPMapForcedObjectsList(NULL)
		, bPMapBeingProcessed(FALSE)
	{
		Init();
	}

	/**
	 * Initialize options from command line options
	 *
	 *	@param	Tokens			Command line tokens parsed from app
	 *	@param	Switches		Command line switches parsed from app
	 */
	void InitOptions(const TArray<FString>& Tokens, const TArray<FString>& Switches);

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
	INT GenerateLoadedObjectList(class UCookPackagesCommandlet* Commandlet, const FString& InPMapName, 
		const TArray<FString>& InObjectsToLoad, TMap<FString,INT>& OutLoadedObjectMap);

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
	virtual UBOOL CookObject(class UCookPackagesCommandlet* Commandlet, UPackage* Package, UObject* Object, UBOOL bIsSavedInSeekFreePackage);

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
	virtual UBOOL PreLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, const TCHAR* Filename);

	/**
	 */
	virtual UBOOL PostLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, UPackage* InPackage);

	/**
	 *	Called when the FPersistentMapInfo class has been initialized and filled in.
	 *	Can be used to do any special setup for PMaps the cooker may require.
	 *
	 *	@param	Commandlet		The commandlet that is being run.
	 */
	virtual void PersistentMapInfoGeneratedCallback(class UCookPackagesCommandlet* Commandlet);

	/**
	 *	Dump out stats specific to the game cooker helper.
	 */
	virtual void DumpStats();

private:

	/**
	 */
	void Init();

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
	FForceCookedInfo* GetPMapForcedObjectList(class UCookPackagesCommandlet* Commandlet, const FString& InPMapName, UBOOL bCreateIfNotFound);


	/** if TRUE then PMaps can have objects forced into them */
	UBOOL bPMapForcedObjectsEnabled;

	/** Mapping of PMaps to forced objects */
	TMap<FString,TMap<FString,UBOOL>> PMapForcedObjectsMap;

	/** The current PMap forced object list */
	FForceCookedInfo* CurrentPMapForcedObjectsList;
	/** If TRUE, the current map being loaded IS the PMap */
	UBOOL bPMapBeingProcessed;
	/** The current PMap name that is being processed */
	FString PMapBeingProcessed;
};

/** 
 *	Helper class for cooking sublevels of PMaps
 */
class FGameStreamingLevelCookerHelper
{
public:
	FGameStreamingLevelCookerHelper()
	{
	}

	/**
	 * Initialize the cooker helper and process any command line params
	 *
	 *	@param	Commandlet		The cookpackages commandlet being run
	 *	@param	Tokens			Command line tokens parsed from app
	 *	@param	Switches		Command line switches parsed from app
	 */
	virtual void Init(
		class UCookPackagesCommandlet* Commandlet, 
		const TArray<FString>& Tokens, 
		const TArray<FString>& Switches);

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
	virtual UBOOL GeneratePackageList( 
		class UCookPackagesCommandlet* Commandlet, 
		UE3::EPlatformType Platform,
		EShaderPlatform ShaderPlatform,
		TArray<FPackageCookerInfo>& NotRequiredFilenamePairs,
		TArray<FPackageCookerInfo>& RegularFilenamePairs,
		TArray<FPackageCookerInfo>& MapFilenamePairs,
		TArray<FPackageCookerInfo>& ScriptFilenamePairs,
		TArray<FPackageCookerInfo>& StartupFilenamePairs,
		TArray<FPackageCookerInfo>& StandaloneSeekfreeFilenamePairs);

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
	virtual UBOOL PreLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, const TCHAR* Filename);

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
	virtual UBOOL PostLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, UPackage* InPackage);

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
	virtual UBOOL CookObject(class UCookPackagesCommandlet* Commandlet, UPackage* Package, UObject* Object, UBOOL bIsSavedInSeekFreePackage);

	/**
	 *	Dump out stats specific to the game cooker helper.
	 */
	virtual void DumpStats();

protected:
	/** If TRUE, the users has opted to disable this feature... */
	UBOOL bDisablePMapObjectChecking;
	/** If TRUE, log out the PMap object lists */
	UBOOL bDumpPMapObjectList;
	/** The current PMap that is 'active' */
	FString CurrentPersistentMap;
	/** The objects that will be cooked into that PMap */
	TMap<FString,UBOOL>	ObjectsInPMap;
};

/**
 *	Helper class to handle game-specific cooking. Each game can 
 *	subclass this to handle special-case cooking needs.
 */
class FGameCookerHelper : public FGameCookerHelperBase
{
public:

	/**
	 *	Initialize the cooker helper and process any command line params
	 *
	 *	@param	Commandlet		The cookpackages commandlet being run
	 *	@param	Tokens			Command line tokens parsed from app
	 *	@param	Switches		Command line switches parsed from app
	 */
	virtual void Init(
		class UCookPackagesCommandlet* Commandlet, 
		const TArray<FString>& Tokens, 
		const TArray<FString>& Switches );

#if WITH_SUBSTANCE_AIR
	/**
	 * Clean up the cooker helper.
	 */
	void Cleanup();
#endif // WITH_SUBSTANCE_AIR == 1

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
	virtual UPersistentCookerData* CreateInstance( const TCHAR* Filename, UBOOL bCreateIfNotFoundOnDisk );

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
	 *	@return	UBOOL		TRUE if successfull, FALSE is something went wrong.
	 */
	virtual UBOOL GeneratePackageList( 
		class UCookPackagesCommandlet* Commandlet, 
		UE3::EPlatformType Platform,
		EShaderPlatform ShaderPlatform,
		TArray<FPackageCookerInfo>& NotRequiredFilenamePairs,
		TArray<FPackageCookerInfo>& RegularFilenamePairs,
		TArray<FPackageCookerInfo>& MapFilenamePairs,
		TArray<FPackageCookerInfo>& ScriptFilenamePairs,
		TArray<FPackageCookerInfo>& StartupFilenamePairs,
		TArray<FPackageCookerInfo>& StandaloneSeekfreeFilenamePairs);

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
	virtual UBOOL CookObject(class UCookPackagesCommandlet* Commandlet, UPackage* Package, UObject* Object, UBOOL bIsSavedInSeekFreePackage);

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
	virtual UBOOL PreLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, const TCHAR* Filename);

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
	virtual UPackage* LoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, const TCHAR* Filename);

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
	virtual UBOOL PostLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, UPackage* InPackage);

	/**
	 *	Clean up the kismet for the given level...
	 *	Remove 'danglers' - sequences that don't actually hook up to anything, etc.
	 *
	 *	@param	Commandlet	The cookpackages commandlet being run
	 *	@param	Package		The package being cooked.
	 */
	virtual void CleanupKismet(class UCookPackagesCommandlet* Commandlet, UPackage* InPackage);

	/**
	 *	Return TRUE if the sound cue should be ignored when generating persistent FaceFX list.
	 *
	 *	@param	Commandlet		The commandlet being run
	 *	@param	InSoundCue		The sound cue of interest
	 *
	 *	@return	UBOOL			TRUE if the sound cue should be ignored, FALSE if not
	 */
	virtual UBOOL ShouldSoundCueBeIgnoredForPersistentFaceFX(class UCookPackagesCommandlet* Commandlet, const USoundCue* InSoundCue);

	/**
	 *	Called when the FPersistentMapInfo class has been initialized and filled in.
	 *	Can be used to do any special setup for PMaps the cooker may require.
	 *
	 *	@param	Commandlet		The commandlet that is being run.
	 */
	virtual void PersistentMapInfoGeneratedCallback(class UCookPackagesCommandlet* Commandlet);

	/**
	 *	Initialize any special information for a cooked shader cache, such as priority
	 *
	 *	@param	Commandlet			The cookpackages commandlet being run
	 *	@param	Package				The package being cooked.
	 *	@param	ShaderCache			Shader cache object that is being initialize
	 *	@param	IsStartupPackage	If true, this is a package that is loaded on startup
	 */
	virtual void InitializeShaderCache(class UCookPackagesCommandlet* Commandlet, UPackage* InPackage, UShaderCache *InShaderCache, UBOOL bIsStartupPackage);

	/**
	 *	Tag any cooked startup objects that should not go into subsequent packages.
	 *	
	 *	@param	Commandlet			The cookpackages commandlet being run
	 */
	virtual void TagCookedStartupObjects(class UCookPackagesCommandlet* Commandlet);

	/**
	 *	Dump out stats specific to the game cooker helper.
	 */
	virtual void DumpStats();

	/**
	 *	Retrieve the instance of the GenerateGameContentSeekfree helper
	 */
	static class FGenerateGameContentSeekfree* GetGameContentSeekfreeHelper();

	/**
	 *	Retrieve the instance of the ForcedContentHelper
	 */
	static class FForcedContentHelper* GetForcedContentHelper();

	/**
	 *	Retrieve the instance of the GameStreamingLevelCookerHelper
	 */
	static class FGameStreamingLevelCookerHelper* GetStreamingLevelCookerHelper();

#if WITH_SUBSTANCE_AIR == 1
protected:

	void CookSubstanceTexture(class USubstanceAirTexture2D* Texture);

	void CookSubstanceAirInstanceFactory(
		class UCookPackagesCommandlet* Commandlet, 
		UPackage* Package, 
		class USubstanceAirInstanceFactory* Factory, 
		UBOOL bIsSavedInSeekFreePackage);

	void CookSubstanceAirGraphInstance(
		class UCookPackagesCommandlet* Commandlet, 
		UPackage* Package, 
		class USubstanceAirGraphInstance* Factory, 
		UBOOL bIsSavedInSeekFreePackage);
	
	UBOOL CurrentPackageContainsMap;

	//! @brief Various size information collected during cooking
	//! @note stored in Kb for precision
	FLOAT SubstanceAirHelper_sbsasrTotal_Kb;
	FLOAT SubstanceAirHelper_mipsRemoved_Kb;
	FLOAT SubstanceAirHelper_mipsKept_Kb;

#endif // WITH_SUBSTANCE_AIR == 1
};

// global objects
extern FGameCookerHelper* GGameCookerHelper;

#endif //__GAMEEDITORCOOKERHELPER_H__
