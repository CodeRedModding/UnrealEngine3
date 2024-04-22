/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This object is responsible for the enumeration of downloadable content bundles
 */
class DownloadableContentEnumerator extends Object
	dependson(OnlineSubsystem)
	native(GameEngine);

/** The set of DLC this enumerator is aware of */
var protectedwrite array<OnlineContent> DLCBundles;

/** The root directory to look for DLC in */
var string DLCRootDir;

/** List of listeners for the find DLC event */
var protected array<delegate<OnFindDLCComplete> > FindDLCDelegates;

/**
 * Looks for DLC and populates the DLC bundles with the information
 */
native function FindDLC();

/**
 * Called once the DLC enumeration is complete
 */
delegate OnFindDLCComplete();

/**
 * Adds a delegate to the list of listeners
 *
 * @param InDelegate the delegate to use for notifications
 */
function AddFindDLCDelegate(delegate<OnFindDLCComplete> InDelegate)
{
	// Add this delegate to the array if not already present
	if (FindDLCDelegates.Find(InDelegate) == INDEX_NONE)
	{
		FindDLCDelegates.AddItem(InDelegate);
	}
}

/**
 * Removes a delegate from the list of listeners
 *
 * @param InDelegate the delegate to use for notifications
 */
function ClearFindDLCDelegate(delegate<OnFindDLCComplete> InDelegate)
{
	local int RemoveIndex;

	// Remove this delegate from the array if found
	RemoveIndex = FindDLCDelegates.Find(InDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		FindDLCDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Removes a DLC bundle from the local machine. This is not an uninstall, so choose wisely
 *
 * @param DLCName the name of the DLC bundle to delete
 */
native function DeleteDLC(string DLCName);

/**
 * Adds the list of DLC bundles to the DLC manager
 */
function InstallAllDLC()
{
	local DownloadableContentManager DLCManager;

	DLCManager = class'GameEngine'.static.GetDLCManager();
	if (DLCManager != None)
	{
		DLCManager.InstallDLCs(DLCBundles);
	}
}

/**
 * Installs the named DLC via the DLC manager
 *
 * @param DLCName the name of the DLC bundle to install
 */
native function InstallDLC(string DLCName);

/**
 * Triggers the FindDLC delegates
 */
native function TriggerFindDLCDelegates();

defaultproperties
{
	DLCRootDir="../../DLC/"
}
