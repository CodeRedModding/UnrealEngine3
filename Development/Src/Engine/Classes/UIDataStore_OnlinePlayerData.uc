/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class is responsible for retrieving the friends list from the online
 * subsystem and populating the UI with that data.
 */
class UIDataStore_OnlinePlayerData extends UIDataStore_Remote
	native(inherit)
	config(Engine)
	transient;

/** Provides access to the player's online friends list */
var UIDataProvider_OnlineFriends FriendsProvider;

/** Holds the player controller that this data store is associated with */
var int PlayerControllerId;

/** The online nick name for the player */
var string PlayerNick;

/** The name of the OnlineProfileSettings class to use as the default */
var config string ProfileSettingsClassName;

/** The class that should be created when a player is bound to this data store */
var class<OnlineProfileSettings> ProfileSettingsClass;

/** Provides access to the player's profile data */
var UIDataProvider_OnlineProfileSettings ProfileProvider;

/** The name of the data provider class to use as the default for player storage data */
var config string ProfileProviderClassName;

/** The class that should be created when a player is bound to this data store */
var class<UIDataProvider_OnlineProfileSettings> ProfileProviderClass;

/** The name of the OnlinePlayerStorage class to use as the default */
var config string PlayerStorageClassName;

/** The class that should be created when a player is bound to this data store */
var class<OnlinePlayerStorage> PlayerStorageClass;

/** Provides access to the player's storage data */
var UIDataProvider_OnlinePlayerStorage StorageProvider;

/** The name of the data provider class to use as the default for player storage data */
var config string StorageProviderClassName;

/** The class that should be created when a player is bound to this data store */
var class<UIDataProvider_OnlinePlayerStorage> StorageProviderClass;

/** Provides access to any friend messages */
var UIDataProvider_OnlineFriendMessages FriendMessagesProvider;

/** Provides access to the list of achievements for this player */
var	UIDataProvider_PlayerAchievements AchievementsProvider;

/** The name of the data provider class to use as the default for friends */
var config string FriendsProviderClassName;

/** The class that should be created when a player is bound to this data store */
var class<UIDataProvider_OnlineFriends> FriendsProviderClass;

/** The name of the data provider class to use as the default for messages */
var config string FriendMessagesProviderClassName;

/** The class that should be created when a player is bound to this data store */
var class<UIDataProvider_OnlineFriendMessages> FriendMessagesProviderClass;

/** The name of the data provider class to use as the default for enumerating achievements */
var config string AchievementsProviderClassName;

/** The class that should be created when a player is bound to this data store for providing achievements data to the UI */
var class<UIDataProvider_PlayerAchievements> AchievementsProviderClass;

/** The name of the data provider class to use as the default for party chat members */
var config string PartyChatProviderClassName;

/** The class that should be created when a player is bound to this data store */
var class<UIDataProvider_OnlinePartyChatList> PartyChatProviderClass;

/** The provider instance for the party chat data */
var UIDataProvider_OnlinePartyChatList PartyChatProvider;

cpptext
{
/* === UIDataStore interface === */

	/**
	 * Loads the game specific OnlineProfileSettings class
	 */
	virtual void LoadDependentClasses(void);

	/**
	 * Creates the data providers exposed by this data store
	 */
	virtual void InitializeDataStore(void);

	/**
	 * Forwards the calls to the data providers so they can do their start up
	 *
	 * @param Player the player that will be associated with this DataStore
	 */
	virtual void OnRegister(ULocalPlayer* Player);

	/**
	 * Tells all of the child providers to clear their player data
	 *
	 * @param Player ignored
	 */
	virtual void OnUnregister(ULocalPlayer*);

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

	if (InPlayer != None)
	{
		PlayerControllerId = InPlayer.ControllerId;
		// Figure out if we have an online subsystem registered
		OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
		if (OnlineSub != None)
		{
			// Grab the player interface to verify the subsystem supports it
			PlayerInterface = OnlineSub.PlayerInterface;
			if (PlayerInterface != None)
			{
				// We need to know when the player's login changes
				PlayerInterface.AddLoginChangeDelegate(OnLoginChange);
			}
			if (OnlineSub.PlayerInterfaceEx != None)
			{
				// We need to know when the player changes data (change nick name, etc)
				OnlineSub.PlayerInterfaceEx.AddProfileDataChangedDelegate(PlayerControllerId,OnPlayerDataChange);
			}
		}
		//If we do not have an online subsystem, nor any settings, then we want the default settings.
		else if (ProfileProvider != none && ProfileProvider.Profile != none)
		{
			 ProfileProvider.Profile.SetToDefaults();
		}

		RegisterDelegates();

		// Force a refresh
		OnLoginChange(PlayerControllerId);
	}
}

/**
 * Clears our delegate for getting login change notifications
 */
event OnUnregister()
{
	local OnlineSubsystem OnlineSub;
	local OnlinePlayerInterface PlayerInterface;

	if (PlayerControllerId != -1)
	{
		ClearDelegates();

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
			}
			if (OnlineSub.PlayerInterfaceEx != None)
			{
				// Clear for GC reasons
				OnlineSub.PlayerInterfaceEx.ClearProfileDataChangedDelegate(PlayerControllerId,OnPlayerDataChange);
			}
		}
	}
}

/**
 * Refetches the player's nick name from the online subsystem
 *
 * @param LocalUserNum the player that logged in/out
 */
function OnLoginChange(byte LocalUserNum)
{
	local OnlineSubsystem OnlineSub;
	local OnlinePlayerInterface PlayerInterface;

	if (LocalUserNum == PlayerControllerId)
	{
		// Figure out if we have an online subsystem registered
		OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
		if (OnlineSub != None)
		{
			// Grab the player interface to verify the subsystem supports it
			PlayerInterface = OnlineSub.PlayerInterface;
			if (PlayerInterface != None &&
				PlayerInterface.GetLoginStatus(PlayerControllerId) > LS_NotLoggedIn)
			{
				// Get the name and force a refresh
				PlayerNick = PlayerInterface.GetPlayerNickname(PlayerControllerId);
			}
			else
			{
				PlayerNick = "";
				ProfileProvider.Profile.SetToDefaults();
				StorageProvider.Profile.SetToDefaults();
			}
		}
		RefreshSubscribers();
	}
}

/**
 * Refetches the player's nick name from the online subsystem
 */
function OnPlayerDataChange()
{
	local OnlineSubsystem OnlineSub;

	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		if (OnlineSub.PlayerInterface != None)
		{
			// Get the name and force a refresh
			PlayerNick = OnlineSub.PlayerInterface.GetPlayerNickname(PlayerControllerId);
			RefreshSubscribers();
		}
	}
}

/**
 * Registers the delegates with the providers so we can know when async data changes
 */
function RegisterDelegates()
{

}

function ClearDelegates()
{

}

/**
 * Retrieves a player profile which has been cached by the online subsystem.
 *
 * @param	ControllerId	the controller ID for the player to retrieve the profile for.
 *
 * @return	a player profile which was previously created and cached by the online subsystem for
 *			the specified controller id.
 */
event OnlineProfileSettings GetCachedPlayerProfile( int ControllerId )
{
	local OnlineSubsystem OnlineSub;
	local OnlinePlayerInterface PlayerInterface;
	local OnlineProfileSettings Result;

	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		// Grab the player interface to verify the subsystem supports it
		PlayerInterface = OnlineSub.PlayerInterface;
		if (PlayerInterface != None)
		{
			Result = PlayerInterface.GetProfileSettings(ControllerId);
		}
	}
	return Result;
}

/**
 * Retrieves a player storage which has been cached by the online subsystem.
 *
 * @param	ControllerId	the controller ID for the player to retrieve the profile for.
 *
 * @return	a player storage which was previously created and cached by the online subsystem for
 *			the specified controller id.
 */
event OnlinePlayerStorage GetCachedPlayerStorage( int ControllerId )
{
	local OnlineSubsystem OnlineSub;
	local OnlinePlayerInterface PlayerInterface;
	local OnlinePlayerStorage Result;

	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		// Grab the player interface to verify the subsystem supports it
		PlayerInterface = OnlineSub.PlayerInterface;
		if (PlayerInterface != None)
		{
			Result = PlayerInterface.GetPlayerStorage(ControllerId);
		}
	}
	return Result;
}

defaultproperties
{
	Tag=OnlinePlayerData
	// So something shows up in the editor
	PlayerNick="PlayerNickNameHere"
	PlayerControllerId=-1
}
