/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class is responsible for mapping properties in an OnlineGameSettings
 * object to something that the UI system can consume.
 */
class UIDataProvider_OnlineProfileSettings extends UIDataProvider_OnlinePlayerStorage
	config(Game)
	native(inherit)
	dependson(OnlineSubsystem)
	transient;

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
 	return PlayerInterface.ReadProfileSettings(LocalUserNum, OnlineProfileSettings(PlayerStorage));
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
 	return PlayerInterface.WriteProfileSettings(LocalUserNum,OnlineProfileSettings(PlayerStorage));
 }
 
/**
 * Fetches the requested object from the online layer's cache
 *
 * @param PlayerInterface is the OnlinePlayerInterface used
 * @param LocalUserNum the user that we are writing the data for
 * @param PlayerStorage the object that contains the list of items to write
 *
 * @return true if the call succeeds, false otherwise
 */
function bool GetData(OnlinePlayerInterface PlayerInterface, byte LocalUserNum)
{
	local OnlineProfileSettings CachedProfile;

	CachedProfile = PlayerInterface.GetProfileSettings(LocalUserNum);
	if (CachedProfile != None)
	{
		Profile = CachedProfile;
		// If there is one in the cache, the read will return immediately
		return PlayerInterface.ReadProfileSettings(LocalUserNum,OnlineProfileSettings(Profile));
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
	PlayerInterface.AddReadProfileSettingsCompleteDelegate(LocalUserNum,OnReadStorageComplete);
}
 
/**
 * Clears the delegate used to notify the gameplay code that the last read request has completed 
 *
 * @param PlayerInterface is the OnlinePlayerInterface used
 * @param LocalUserNum which user to stop watching for read complete notifications
 */
function ClearReadCompleteDelegate(OnlinePlayerInterface PlayerInterface, byte LocalUserNum)
{
	PlayerInterface.ClearReadProfileSettingsCompleteDelegate(LocalUserNum,OnReadStorageComplete);
}
 
/**
 * Reads this user's storage data from the online subsystem.
 */
function RefreshStorageData()
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
			// Start the async task
			if (!ReadData(PlayerInterface,PlayerControllerId,Profile.DeviceId,Profile))
			{
			}
		}
	}
}

/**
 * Profile doesn't care, so ignores it
 */
function OnStorageDeviceChange()
{
}

defaultproperties
{
	ProviderName=ProfileData
}
