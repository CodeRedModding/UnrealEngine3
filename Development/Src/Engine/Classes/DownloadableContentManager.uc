/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This object is responsible for the installation and removal of
 * downloadable content
 */
class DownloadableContentManager extends Object
	native(GameEngine);

/** The list of DLC config cache changes made by the installed DLC */
var native const transient array<pointer> DLCConfigCacheChanges{struct FDLCConfigCacheChanges};

/** The list of installed DLC bundles */
var const transient array<string> InstalledDLC;

/** Maps the file name to the full path to its location */
var	const native transient Map_Mirror NonPackageFilePathMap{TMap<FName,FString>};

/** The list of classes that need reloading due to a config change */
var const native transient array<class> ClassesToReload;

/** The list of per object config objects that need updating due to a config change */
var const native transient array<object> ObjectsToReload;

/** The list of ini files we need to check for always loaded packages after initialization */
var const transient array<string> QueuedFullyLoadPackageInis;

/** The owning game engine */
var const transient GameEngine GameEngine;

/**
 * Installs a DLC bundle
 *
 * @param DLCBundle the bundle that is to be installed
 *
 * @return true if the bundle could be installed correctly, false if it failed
 */
native function bool InstallDLC(const out OnlineContent DLCBundle);

/**
 * Installs a set of DLC bundles
 *
 * @param DLCBundles the set of bundles that are to be installed
 */
native function InstallDLCs(const out array<OnlineContent> DLCBundles);

/**
 * Clears the DLC cache and restores the config cache to its pre-DLC state
 */
native function ClearDLC();

/**
 * Determines the full path of the file in DLC
 *
 * @param NonPackageFileName the bundle that is to be installed
 * @param Path receives the full path to the file
 *
 * @return true if the file was found, false if it wasn't
 */
native function bool GetDLCNonPackageFilePath(name NonPackageFileName,out string Path);

/**
 * Adds the specified section to the classes to update list or to the per object config
 * objects to update depending on whether they are found
 *
 * @param Section the section name being reloaded
 */
native function AddSectionToObjectList(string Section);

/**
 * Looks to see if the section is a per object config section that was removed so the
 * object also needs to be destroyed
 *
 * @param Section the section name being unloaded
 */
native function MarkPerObjectConfigPendingKill(string Section);

/**
 * Reloads config and localization on both of the object lists and empties them
 */
native function UpdateObjectLists();

/**
 * Installs the list of packages for the DLC
 *
 * @param DLCBundle the bundle that is being installed
 */
native protected function InstallPackages(const out OnlineContent DLCBundle);

/**
 * Installs the list of non-packages (ini, loc, sha, etc.) for the DLC
 *
 * @param DLCBundle the bundle that is being installed
 */
native protected function InstallNonPackageFiles(const out OnlineContent DLCBundle);

/**
 * Registers the callback events with the online subsystem that it cares about
 */
event Init()
{
	local OnlineSubsystem OnlineSub;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		if (OnlineSub.PlayerInterface != None)
		{
			OnlineSub.PlayerInterface.AddLoginChangeDelegate(OnLoginChange);
		}
		if (OnlineSub.SystemInterface != None)
		{
			OnlineSub.SystemInterface.AddStorageDeviceChangeDelegate(OnStorageDeviceChange);
		}
		if (OnlineSub.ContentInterface != None)
		{
			OnlineSub.ContentInterface.AddContentChangeDelegate(OnContentChange);
		}
	}
}

/**
 * Delegate called when a player logs in/out, so we can clear/refresh DLC
 *
 * @param LocalUserNum the player that logged in/out
 */
function OnLoginChange(byte LocalUserNum)
{
	RefreshDLC();
}

/**
 * Called when a storage device is inserted/removed. Here so game specific implementations can easily overload
 */
function OnStorageDeviceChange()
{
	RefreshDLC();
}

/**
 * Called when new DLC is installed. Here so game specific implementations can easily overload
 */
function OnContentChange()
{
	RefreshDLC();
}

/**
 * Delegate used when content is changed (add or deletion) for any user or
 * when any storage devices are changed
 */
function RefreshDLC()
{
	local DownloadableContentEnumerator DLCEnum;

	ClearDLC();
	DLCEnum = class'GameEngine'.static.GetDLCEnumerator();
	if (DLCEnum != None)
	{
		DLCEnum.AddFindDLCDelegate(RefreshDLCEnumComplete);
		DLCEnum.FindDLC();
	}
}

/**
 * Installs the DLC when the enumeration is complete
 */
function RefreshDLCEnumComplete()
{
	local DownloadableContentEnumerator DLCEnum;

	DLCEnum = class'GameEngine'.static.GetDLCEnumerator();
	if (DLCEnum != None)
	{
		DLCEnum.ClearFindDLCDelegate(RefreshDLCEnumComplete);
		DLCEnum.InstallAllDLC();
	}
	OnRefreshComplete();
}

/**
 * Parses the specified section for the key/value set to use for fully loading packages
 *
 * @param FileName the file name to parse the information from
 */
native function AddPackagesToFullyLoad(string FileName);

/** Called once the refresh is complete */
delegate OnRefreshComplete();

cpptext
{
	/**
	 * Creates a map of all the non package files contained in the DLC
	 *
	 * @param DLCBundle the DLC bundle being processed
	 */
	void InstallNonPackages(const FOnlineContent& DLCBundle);

#if PS3 // PS3 DLC has to be signed
	/**
	 * Adds any SHA hashes to the file hashes for this bundle
	 *
	 * @param DLCBundle the DLC bundle being processed
	 */
	void InstallSHAFiles(const FOnlineContent& DLCBundle);
#endif

	/**
	 * Process all INI files and our current loc language
	 *
	 * @param DLCBundle the DLC bundle being processed
	 */
	void InstallIniLocFiles(const FOnlineContent& DLCBundle);

	/**
	 * Parses the specified section for the key/value set to use for fully loading
	 *
	 * @param LoadType the type of package loading to use
	 * @param Section the INI section to parse
	 * @param KeyOne the key to parse
	 * @param KeyN the sub keys to parse
	 * @param FileName the file name to parse the information from
	 */
	void AddPackagesToFullyLoad(EFullyLoadPackageType LoadType,const TCHAR* Section,const TCHAR* KeyOne,const TCHAR* KeyN,const TCHAR* FileName);

	/**
	 * For a given DLC ini/loc file, it determines the list of sections in the INI/loc file that are touched
	 * so that it can determine which ones to replace during unloading and which to remove altogether
	 *
	 * @param FileName the name of the DLC ini/loc file to parse for section changes
	 * @param OutSectionsIncluded gets the list of modified/added sections
	 */
	void GetListOfSectionNames(const TCHAR* FileName,TArray<FString>& OutSectionsIncluded);

	/**
	 * Builds the set of changes needed to undo this DLC's config changes/additions
	 *
	 * @param ConfigFileName the name of the DLC ini/loc file to build the undo set for
	 * @param SectionsIncluded the list of modified/added sections
	 */
	void BuildDLCConfigCacheUndo(const TCHAR* ConfigFileName,const TArray<FString>& SectionsIncluded);
}