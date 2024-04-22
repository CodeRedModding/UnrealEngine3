/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class is responsible for retrieving the party chat member list from the online
 * subsystem and populating the UI with that data.
 */
class UIDataProvider_OnlinePartyChatList extends UIDataProvider_OnlinePlayerDataBase
	native(inherit)
	dependson(OnlineSubsystem)
	transient;

/** Gets a copy of the friends data from the online subsystem */
var array<OnlinePartyMember> PartyMembersList;

/** The text to use for nat types */
var localized array<string> NatTypes;

/** The column name to display in the UI */
var localized string NickNameCol;

/** The column name to display in the UI */
var localized string NatTypeCol;

/** The column name to display in the UI */
var localized string IsLocalCol;

/** The column name to display in the UI */
var localized string IsInPartyVoiceCol;

/** The column name to display in the UI */
var localized string IsTalkingCol;

/** The column name to display in the UI */
var localized string IsInGameSessionCol;

/** The column name to display in the UI */
var localized string IsPlayingThisGameCol;

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
				// Start the async task
//				PlayerInterface.ReadFriendsList(Player.ControllerId);
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
			// Clear our delegate
			PlayerInterface.ClearLoginChangeDelegate(OnLoginChange);
		}
	}
	Super.OnUnregister();
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

	PartyMembersList.Length = 0;
	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		// Grab the player interface to verify the subsystem supports it
		PlayerInterface = OnlineSub.PlayerInterface;
		if (PlayerInterface != None &&
			PlayerInterface.GetLoginStatus(PlayerControllerId) > LS_NotLoggedIn)
		{
			// Start the async task
//			PlayerInterface.ReadFriendsList(Player.ControllerId);
		}
	}
}

/** Re-reads the friends list to freshen any cached data */
event RefreshMembersList()
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
			if (PlayerInterface != None)
			{
				// Start the async task
//				PlayerInterface.ReadFriendsList(Player.ControllerId);
				`log("Refreshing friends list");
			}
		}
	}
}