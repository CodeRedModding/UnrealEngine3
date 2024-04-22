/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class is responsible for retrieving the friends list from the online
 * subsystem and populating the UI with that data.
 */
class UIDataProvider_OnlineFriends extends UIDataProvider_OnlinePlayerDataBase
	native(inherit)
	dependson(OnlineSubsystem)
	transient;

/** Gets a copy of the friends data from the online subsystem */
var array<OnlineFriend> FriendsList;

/** The column name to display in the UI */
var localized string NickNameCol;

/** The column name to display in the UI */
var localized string PresenceInfoCol;

/** The column name to display in the UI */
var localized string FriendStateCol;

/** The column name to display in the UI */
var localized string bIsOnlineCol;

/** The column name to display in the UI */
var localized string bIsPlayingCol;

/** The column name to display in the UI */
var localized string bIsPlayingThisGameCol;

/** The column name to display in the UI */
var localized string bIsJoinableCol;

/** The column name to display in the UI */
var localized string bHasVoiceSupportCol;

/** The column name to display in the UI */
var localized string bHaveInvitedCol;

/** The column name to display in the UI */
var localized string bHasInvitedYouCol;

/** The text to use when offline */
var localized string OfflineText;

/** The text to use when online */
var localized string OnlineText;

/** The text to use when away */
var localized string AwayText;

/** The text to use when busy */
var localized string BusyText;

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
				PlayerInterface.AddReadFriendsCompleteDelegate(PlayerControllerId,OnFriendsReadComplete);
				// Don't read people that aren't signed in or are guest accounts
				if (PlayerInterface.GetLoginStatus(PlayerControllerId) > LS_NotLoggedIn &&
					!PlayerInterface.IsGuestLogin(PlayerControllerId))
				{
					// Start the async task
					PlayerInterface.ReadFriendsList(PlayerControllerId);
				}
			}
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
			// Set our callback function per player
			PlayerInterface.ClearReadFriendsCompleteDelegate(PlayerControllerId,OnFriendsReadComplete);
			// Clear our delegate
			PlayerInterface.ClearLoginChangeDelegate(OnLoginChange);
		}
	}
	Super.OnUnregister();
}

/**
 * Handles the notification that the async read of the friends data is done
 *
 * @param bWasSuccessful whether the call completed ok or not
 */
function OnFriendsReadComplete(bool bWasSuccessful)
{
	local OnlineSubsystem OnlineSub;
	local OnlinePlayerInterface PlayerInterface;

	if (bWasSuccessful == true)
	{
		// Figure out if we have an online subsystem registered
		OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
		if (OnlineSub != None)
		{
			// Grab the player interface to verify the subsystem supports it
			PlayerInterface = OnlineSub.PlayerInterface;
			if (PlayerInterface != None)
			{
				// Make a copy of the friends data for the UI
				PlayerInterface.GetFriendsList(PlayerControllerId,FriendsList);
			}
		}
	}
	else
	{
		`Log("Failed to read friends list",,'DevOnline');
	}
}

/**
 * Executes a refetching of the friends data when the login for this player
 * changes
 *
 * @param LocalUserNum the player that logged in/out
 */
function OnLoginChange(byte LocalUserNum)
{
	local OnlineSubsystem OnlineSub;
	local OnlinePlayerInterface PlayerInterface;

	FriendsList.Length = 0;
	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		// Grab the player interface to verify the subsystem supports it
		PlayerInterface = OnlineSub.PlayerInterface;
		if (PlayerInterface != None &&
			PlayerInterface.GetLoginStatus(PlayerControllerId) > LS_NotLoggedIn &&
			!PlayerInterface.IsGuestLogin(PlayerControllerId))
		{
			// Start the async task
			PlayerInterface.ReadFriendsList(PlayerControllerId);
		}
	}
}

/** Re-reads the friends list to freshen any cached data */
event RefreshFriendsList()
{
	local OnlineSubsystem OnlineSub;
	local OnlinePlayerInterface PlayerInterface;

	// If the player is None, we are in the editor
	if (PlayerControllerId != -1)
	{
		// Figure out if we have an online subsystem registered
		OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
		if (OnlineSub != None)
		{
			// Grab the player interface to verify the subsystem supports it
			PlayerInterface = OnlineSub.PlayerInterface;
			if (PlayerInterface != None &&
				PlayerInterface.GetLoginStatus(PlayerControllerId) > LS_NotLoggedIn &&
				!PlayerInterface.IsGuestLogin(PlayerControllerId))
			{
				// Start the async task
				PlayerInterface.ReadFriendsList(PlayerControllerId);
				`log("Refreshing friends list",,'DevOnline');
			}
		}
	}
}