/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class is responsible for mapping properties in an OnlineGameSettings
 * object to something that the UI system can consume.
 */
class UIDataProvider_OnlinePlayerStorage extends UIDataProvider_OnlinePlayerDataBase
	native(inherit)
	config(Game)
	dependson(OnlineSubsystem)
	transient;

/** The storage settings that are used to load/save with the online subsystem */
var OnlinePlayerStorage Profile;

/** For displaying in the provider tree */
var const name ProviderName;

/**
 * If there was an error, it was possible the read was already in progress. This
 * indicates to re-read upon a good completion
 */
var bool bWasErrorLastRead;

/** Keeps a list of providers for each storage settings id */
struct native PlayerStorageArrayProvider
{
	/** The storage settings id that this provider is for */
	var int PlayerStorageId;
	/** The provider object to expose the data with */
	var UIDataProvider_OnlinePlayerStorageArray Provider;
};

/** The list of mappings from settings id to their provider */
var array<PlayerStorageArrayProvider> PlayerStorageArrayProviders;

/** The amount of storage needed for this game */
var config int DeviceStorageSizeNeeded;

/** Whether the UI external to the game is open or not */
var bool bIsExternalUIOpen;

/** Whether we need to refresh our data upon external UI closing or not */
var bool bNeedsDeferredRefresh;

/**
 * Reads the data
 *
 * @param PlayerInterface is the OnlinePlayerInterface used
 * @param LocalUserNum the user that we are reading the data for
 * @param DeviceId device for local read of player data (-1 for no device)
 * @param PlayerStorage the object to copy the results to and contains the list of items to read
 *
 * @return true if the call succeeds, false otherwise
 */
function bool ReadData(OnlinePlayerInterface PlayerInterface, byte LocalUserNum, int DeviceId, OnlinePlayerStorage PlayerStorage)
{
	return PlayerInterface.ReadPlayerStorage(LocalUserNum, PlayerStorage, DeviceId);
}

/**
 * Writes the online  data for a given local user to the online data store
 *
 * @param PlayerInterface is the OnlinePlayerInterface used
 * @param LocalUserNum the user that we are writing the data for
 * @param DeviceId device for local write of player data (-1 for no device)
 * @param PlayerStorage the object that contains the list of items to write
 *
 * @return true if the call succeeds, false otherwise
 */
function bool WriteData(OnlinePlayerInterface PlayerInterface, byte LocalUserNum, int DeviceId, OnlinePlayerStorage PlayerStorage)
{
	return PlayerInterface.WritePlayerStorage(LocalUserNum,PlayerStorage, DeviceId);
}

/**
 * Fetches the requested object from the online layer's cache
 *
 * @param PlayerInterface is the OnlinePlayerInterface used
 * @param LocalUserNum the user that we are writing the data for
 *
 * @return true if the call succeeds, false otherwise
 */
function bool GetData(OnlinePlayerInterface PlayerInterface,byte LocalUserNum)
{
	local OnlinePlayerStorage CachedStorage;

	CachedStorage = PlayerInterface.GetPlayerStorage(LocalUserNum);
	if (CachedStorage != None)
	{
		// Use the existing object instead of the new one
		Profile = CachedStorage;
		// This read will return immediately
		PlayerInterface.ReadPlayerStorage(LocalUserNum,Profile);
		return true;
	}
	return false;
}

/**
 * Sets the delegate used to notify the gameplay code that the last read request has completed 
 *
 * @param PlayerInterface is the OnlinePlayerInterface used
 * @param LocalUserNum which user to watch for read complete notifications
 */
function AddReadCompleteDelegate(OnlinePlayerInterface PlayerInterface, byte LocalUserNum)
{
	PlayerInterface.AddReadPlayerStorageCompleteDelegate(LocalUserNum,OnReadStorageComplete);
}

/**
 * Clears the delegate used to notify the gameplay code that the last read request has completed 
 *
 * @param PlayerInterface is the OnlinePlayerInterface used
 * @param LocalUserNum which user to stop watching for read complete notifications
 */
function ClearReadCompleteDelegate(OnlinePlayerInterface PlayerInterface, byte LocalUserNum)
{
	PlayerInterface.ClearReadPlayerStorageCompleteDelegate(LocalUserNum,OnReadStorageComplete);
}

/**
 * Binds the player to this provider. Starts the async friends list gathering
 *
 * @param InPlayer the player that we are retrieving friends for
 */
event OnRegister(LocalPlayer InPlayer)
{
	local OnlineSubsystem OnlineSub;
	local OnlinePlayerInterface PlayerInterface;

	Super.OnRegister(InPlayer);

	// If the player is None, we are in the editor
	if (PlayerControllerId != -1)
	{
		// Figure out if we have an online subsystem registered
		OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
		if (OnlineSub != None)
		{
			// Grab the player interface to verify the subsystem supports it
			PlayerInterface = OnlineSub.PlayerInterface;
			if (PlayerInterface != None)
			{
				// Register that we are interested in any sign in change for this player
				PlayerInterface.AddLoginChangeDelegate(OnLoginChange);
				// Set our callback function per player
				AddReadCompleteDelegate(PlayerInterface,PlayerControllerId);
				// Swap our object if this is one cached for this user
				GetData(PlayerInterface,PlayerControllerId);
				// Refresh our data
				RefreshStorageData();
			}
		}
		// Request notifications of device removal
		if (OnlineSub.SystemInterface != None)
		{
			OnlineSub.SystemInterface.AddStorageDeviceChangeDelegate(OnStorageDeviceChange);
			OnlineSub.SystemInterface.AddExternalUIChangeDelegate(OnExternalUIChange);
		}
	}
}

/**
 * Clears our delegate for getting login change notifications
 */
event OnUnregister()
{
	local OnlineSubsystem OnlineSub;
	local OnlinePlayerInterface PlayerInterface;
	
	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		// Grab the player interface to verify the subsystem supports it
		PlayerInterface = OnlineSub.PlayerInterface;
		if (PlayerInterface != None)
		{
			// Clear our delegate
			PlayerInterface.ClearLoginChangeDelegate(OnLoginChange);
			ClearReadCompleteDelegate(PlayerInterface,PlayerControllerId);
		}
		// Request notifications of device removal
		if (OnlineSub.SystemInterface != None)
		{
			OnlineSub.SystemInterface.ClearStorageDeviceChangeDelegate(OnStorageDeviceChange);
			OnlineSub.SystemInterface.ClearExternalUIChangeDelegate(OnExternalUIChange);
		}
	}
	Super.OnUnregister();
}

/**
 * Handles the notification that the async read of the storage data is done
 *
 * @param bWasSuccessful whether the call succeeded or not
 */
function OnReadStorageComplete(byte LocalUserNum,bool bWasSuccessful)
{
	local OnlineSubsystem OnlineSub;
	local OnlinePlayerInterface PlayerInterface;

	if (bWasSuccessful == true)
	{
		if (!bWasErrorLastRead)
		{
		}
		else
		{
			// Figure out if we have an online subsystem registered
			OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
			if (OnlineSub != None)
			{
				// Grab the player interface to verify the subsystem supports it
				PlayerInterface = OnlineSub.PlayerInterface;
				if (PlayerInterface != None)
				{
					bWasErrorLastRead = false;
					// Read again to copy any data from a read in progress
					if (ReadData(PlayerInterface,PlayerControllerId,Profile.DeviceId,Profile) == false)
					{
						bWasErrorLastRead = true;
					}
				}
			}
		}
	}
	else
	{
		bWasErrorLastRead = true;
		`Log("Failed to read online storage data",,'DevOnline');
	}
}

/**
 * Executes a refetching of the storage data when the login for this player changes
 *
 * @param LocalUserNum the player that logged in/out
 */
function OnLoginChange(byte LocalUserNum)
{
	local OnlineSubsystem OnlineSub;
	local OnlinePlayerInterface PlayerInterface;
	local ELoginStatus LoginStatus;
	local UniqueNetId NetId;

	if (LocalUserNum == PlayerControllerId)
	{
		// Figure out if we have an online subsystem registered
		OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
		if (OnlineSub != None)
		{
			// Grab the player interface to verify the subsystem supports it
			PlayerInterface = OnlineSub.PlayerInterface;
			if (PlayerInterface != None)
			{
				LoginStatus = PlayerInterface.GetLoginStatus(PlayerControllerId);
				PlayerInterface.GetUniquePlayerId(PlayerControllerId,NetId);
				if (LoginStatus == LS_NotLoggedIn)
				{
					// Reset the profile only when they've signed out
					Profile.SetToDefaults();
				}
			}
		}
		RefreshStorageData();
	}
}

/**
 * Reads this user's storage data from the online subsystem.
 */
function RefreshStorageData()
{
	local OnlineSubsystem OnlineSub;
	local bool bFoundCachedData;

	if (!bIsExternalUIOpen)
	{
		// Figure out if we have an online subsystem registered
		OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
		if (OnlineSub != None &&
			OnlineSub.PlayerInterface != None &&
			OnlineSub.PlayerInterfaceEx != None)
		{
			// Determine if the cached data is present
			bFoundCachedData = GetData(OnlineSub.PlayerInterface,PlayerControllerId);
			if (!bFoundCachedData ||
				// If they have cached data and that device is valid, skip the prompt
				(bFoundCachedData &&
				// If they already have a valid device, don't prompt
				!OnlineSub.PlayerInterfaceEx.IsDeviceValid(Profile.DeviceId,DeviceStorageSizeNeeded)))
			{
				ShowDeviceSelection();
			}
		}
	}
	else
	{
		// Do the refresh when the UI closes
		bNeedsDeferredRefresh = true;
	}
}

/**
 * Shows the device selection UI if possible
 */
function ShowDeviceSelection()
{
	local OnlineSubsystem OnlineSub;

	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None &&
		OnlineSub.PlayerInterface != None &&
		OnlineSub.PlayerInterfaceEx != None)
	{
		OnlineSub.PlayerInterfaceEx.AddDeviceSelectionDoneDelegate(PlayerControllerId,OnDeviceSelectionComplete);
		// Get the device that their data may be stored on locally
		OnlineSub.PlayerInterfaceEx.ShowDeviceSelectionUI(PlayerControllerId,DeviceStorageSizeNeeded);
	}
}

/**
 * Called once the user has selected their device
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
function OnDeviceSelectionComplete(bool bWasSuccessful)
{
	local OnlineSubsystem OnlineSub;
	local string Ignored;

	// We know we have one, because this event was called
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	OnlineSub.PlayerInterfaceEx.ClearDeviceSelectionDoneDelegate(PlayerControllerId,OnDeviceSelectionComplete);
	// Get the latest device id if this worked
	if (bWasSuccessful)
	{
		// Get the device that was selected
		Profile.DeviceId = OnlineSub.PlayerInterfaceEx.GetDeviceSelectionResults(PlayerControllerId,Ignored);
		`Log("OnDeviceSelectionComplete("$bWasSuccessful$") for ControllerId ("$PlayerControllerId$") with DeviceId ("$Profile.DeviceId$")",,'DevOnline');
		// Start the async task
		if (ReadData(OnlineSub.PlayerInterface,PlayerControllerId,Profile.DeviceId,Profile) == false)
		{
		}
	}
	else
	{
		// Failed, so default to no storage
		Profile.DeviceId = -1;
	}
}

/**
 * Verifies that the device for all of the installed DLC is still valid and reboots the game if not
 */
function OnStorageDeviceChange()
{
	local OnlineSubsystem OnlineSub;

	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None &&
		OnlineSub.SystemInterface != None)
	{
		// If our current device is no longer valid, re-request one
		if (!OnlineSub.PlayerInterfaceEx.IsDeviceValid(Profile.DeviceId,DeviceStorageSizeNeeded))
		{
			Profile.DeviceId = -1;
			RefreshStorageData();
		}
	}
}

/**
 * Used to check for an external UI being open when attempting to show other UI. That will fail
 * so this allows the code to call the show upon closing
 *
 * @param bIsOpening whether the external UI is opening or closing
 */
function OnExternalUIChange(bool bIsOpening)
{
	bIsExternalUIOpen = bIsOpening;
	// If we have a deferred update pending, kick it off now that the external UI is gone
	if (!bIsOpening && bNeedsDeferredRefresh)
	{
		RefreshStorageData();
	}
}

defaultproperties
{
	ProviderName=PlayerStorageData
}
