/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Class that implements the Steamworks-specific lobby functionality
 */
Class OnlineLobbyInterfaceSteamworks extends Object within OnlineSubsystemCommonImpl
	config(Engine)
	native;


// This lobby interface is disabled by default, until it can be generalized into an interface usable by all online subsystems.
//	Licensee's can enable and use this code, but expect significant changes and binary compatability breakage until it has
//	been generalized into the main engine code

// NOTE: To enable the lobby interface, set STEAM_MATCHMAKING_LOBBY to 1 in Globals.uci
`if(`STEAM_MATCHMAKING_LOBBY)

cpptext
{
	// Enables the C++ side of the lobby interface
	#define STEAM_MATCHMAKING_LOBBY 1

	// NOTE: If the 'STEAM_MATCHMAKING_LOBBY' define is removed, this cpptext can be removed (script compiler will automatically include this .h)
	#include "UOnlineLobbyInterfaceSteamworks.h"
}


/**
 * The visibility/connectivity type of a lobby
 */
enum ELobbyVisibility
{
	LV_Public,		// Lobby is visible to everyone
	LV_Friends,		// Visible to friends and invited players only
	LV_Private,		// Can only join by invite
	LV_Invisible		// @todo Steam: Figure out what this does exactly
};

/**
 * A basic key/value pair for holding various lobby settings
 */
struct native LobbyMetaData
{
	var string	Key;	// The key identifying the setting
	var string	Value;	// The value of the setting
};

/** Stores Settings passed into CreateLobby, until they can be set */
var array<LobbyMetaData> CreateLobbySettings;

/** Stores the UID of the created lobby, while initial lobby settings are being set asynchronously */
var const UniqueNetId PendingCreateLobbyResult;

/** The list of delegates fired when 'CreateLobby' completes */
var array<delegate<OnCreateLobbyComplete> > CreateLobbyCompleteDelegates;


/**
 * Used to specify the desired geographical distance when searching for lobbies
 */
enum ELobbyDistance
{
	LD_Best,	// Prefers close lobbies, but will search farther if none available (default)
	LD_Close,	// Only lobbies in the same general region (low latency)
	LD_Far,		// Returns lobbies from half way across the globe (high latency) (@todo Steam: see if >only< returns far)
	LD_Any		// No distance filtering; returns lobbies from anywhere
};

/**
 * Used for specifying filters based on key/value pairs, when searching for lobbies
 * NOTE: Max size of a key is 255
 */
struct native LobbyFilter
{
	var string				Key;		// The key to be filtered
	var string				Value;		// The value to filter against
	var EOnlineGameSearchComparisonType	Operator;	// The operator to use for comparison

	var bool				bNumeric;	// whether or not this filter is numeric (treated as string otherwise)
};

/**
 * Used for specifying the order in which lobby search results should be sorted
 */
struct native LobbySortFilter
{
	var string	Key;		// The key to use for sorting
	var int		TargetValue;	// The value to sort upon (the closer a key is to this value, the higher up in the results)
};

/**
 * Struct describing basic information about a lobby, typically from a lobby search result
 */
struct native BasicLobbyInfo
{
	var const UniqueNetId		LobbyUID;	// The unique id of the lobby
	var const array<LobbyMetaData>	LobbySettings;	// The list of settings read for this lobby
};


/** whether or not a lobby search is in progress */
var const bool bLobbySearchInProgress;

/** Stores the most recent results from 'FindLobbies'; the 'LobbyList' parameter for 'OnFindLobbiesComplete' directly references this property */
var const array<BasicLobbyInfo> CachedFindLobbyResults;

/** The list of delegates fired when 'FindLobbies' completes */
var array<delegate<OnFindLobbiesComplete> > FindLobbiesCompleteDelegates;


/**
 * Describes information about a member of a lobby we are active in
 */
struct native LobbyMember
{
	var const UniqueNetId		PlayerUID;	// The unique id of the player
	var const array<LobbyMetaData>	PlayerSettings;	// The list of settings read for this player

	// @todo Steam: Decide whether or not to put extra fields here, e.g. avatar/community-name etc. (may be best to leave that up
	//	to the actual ingame implementation of lobbies)
};

/**
 * Describes all available information about a lobby we are active in
 */
struct native ActiveLobbyInfo extends BasicLobbyInfo
{
	var const array<LobbyMember>	Members;	// The list of members of a lobby we are in
};


/** Lobbies the player is currently connected to; many callback delegates have out parameters directly referencing this property */
var const array<ActiveLobbyInfo> ActiveLobbies;

/** The list of delegates fired when 'JoinLobby' completes */
var array<delegate<OnJoinLobbyComplete> > JoinLobbyCompleteDelegates;

/** Determines the set of keys that should be read from lobby members */
var config array<string> LobbyMemberKeys;

/** The list of delegates fired when a lobbies settings are updated */
var array<delegate<OnLobbySettingsUpdate> > LobbySettingsUpdateDelegates;

/** The list of delegates fired when a lobby members settings are updated */
var array<delegate<OnLobbyMemberSettingsUpdate> > LobbyMemberSettingsUpdateDelegates;

/** The list of delegates fired when a lobby members status has changed */
var array<delegate<OnLobbyMemberStatusUpdate> > LobbyMemberStatusUpdateDelegates;

/** The list of delegates fired when a message is received from a lobby */
var array<delegate<OnLobbyReceiveMessage> > LobbyReceiveMessageDelegates;

/** The list of delegates fired when binary data is received from a lobby */
var array<delegate<OnLobbyReceiveBinaryData> > LobbyReceiveBinaryDataDelegates;

/** Stores the most recently received binary from a lobby; the 'Data' parameter for 'OnLobbyReceiveBinaryData' directly references this property */
var const array<byte> CachedBinaryData;

/** The list of delegates fired when a lobby directs players to join a server */
var array<delegate<OnLobbyJoinGame> > LobbyJoinGameDelegates;

/** The list of delegates fired when the player receives or accepts a lobby invite */
var array<delegate<OnLobbyInvite> > LobbyInviteDelegates;


// @todo Steam: Add bools here, which locks the arrays being passed as out parameters, and spits out a log warning if anything tries to modify them
//		while they are being accessed like this (this is important, even though you are not doing a compiler hack to pass array elements)


// General interface functions

/**
 * Creates a lobby, joins it, and optionally assigns its initial settings, triggering callbacks when done
 *
 * @param MaxPlayers		The maximum number of lobby members
 * @param Type			The type of lobby to setup (public/private/etc.)
 * @param InitialSettings	The list of settings to apply to the lobby upon creation
 * @return			Returns True if successful, False otherwise
 */
native function bool CreateLobby(int MaxPlayers, optional ELobbyVisibility Type, optional array<LobbyMetaData> InitialSettings);

/**
 * Called when 'CreateLobby' completes, returning success/failure, and (if successful) the new lobby UID
 *
 * @param bWasSuccessful	whether or not 'CreateLobby' was successful
 * @param LobbyId		If successful, the UID of the new lobby
 * @param Error			If lobby creation failed, returns the error type
 */
delegate OnCreateLobbyComplete(bool bWasSuccessful, UniqueNetId LobbyId, string Error);

/**
 * Sets the delegate used to notify when a call to 'CreateLobby' has completed
 *
 * @param CreateLobbyCompleteDelegate	The delegate to use for notifications
 */
function AddCreateLobbyCompleteDelegate(delegate<OnCreateLobbyComplete> CreateLobbyCompleteDelegate)
{
	if (CreateLobbyCompleteDelegates.Find(CreateLobbyCompleteDelegate) == INDEX_None)
	{
		CreateLobbyCompleteDelegates[CreateLobbyCompleteDelegates.Length] = CreateLobbyCompleteDelegate;
	}
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param CreateLobbyCompleteDelegate	The delegate to remove from the list
 */
function ClearCreateLobbyCompleteDelegate(delegate<OnCreateLobbyComplete> CreateLobbyCompleteDelegate)
{
	local int i;

	i = CreateLobbyCompleteDelegates.Find(CreateLobbyCompleteDelegate);

	if (i != INDEX_None)
	{
		CreateLobbyCompleteDelegates.Remove(i, 1);
	}
}


/**
 * Kicks off a search for available lobbies, matching the specified filters, triggering callbacks when done
 * @todo Steam: Clean up the parameter list here, it's quite messy
 * @todo Steam: It may be best to roll up the 'distance' filter, as a hardcoded filter key in the 'Filters' list; this standardizes
 *		the lobby implementation a bit more, allowing other subsystems to use it
 *
 * @param MaxResults	The maximum number of results to return
 * @param Filters	Filters used for restricting returned lobbies
 * @param SortFilters	Influences sorting of the returned lobby list, with the first filter influencing the most
 * @param MinSlots	Minimum required number of open slots (@todo Steam: Test to see this doesn't list >exact< number of slots)
 * @param Distance	The desired geographical distance of returned lobbies
 * @return		Returns True if successful, False otherwise
 */
native function bool FindLobbies(optional int MaxResults=32, optional array<LobbyFilter> Filters, optional array<LobbySortFilter> SortFilters,
					optional int MinSlots, optional ELobbyDistance Distance=LD_Best);

/**
 * Updates the lobby settings for all current lobby search results, and removes lobbies if they have become invalid
 * NOTE: Triggers OnFindLobbiesComplete when done
 * @todo Steam: See if you really want this (could be used to update lobby search results, as well as grab more info for specific lobbies a lobby
 *		when passing bUIDOnly to FindLobbies
 * @todo Steam: Should this really trigger OnFindLobbiesComplete when done? That returns the entire lobby list
 *
 * @param LobbyId	Allows you to specify the id of one particular lobby you want to update
 * @return		Returns True if successful, False otherwise
 */
native function bool UpdateFoundLobbies(optional UniqueNetId LobbyId);

/**
 * Called when 'FindLobbies' completes, returning success/failure, and (if successful) the final lobby list
 *
 * @param bWasSuccessful	whether or not 'FindLobbies' was successful
 * @param LobbyList		The list of returned lobbies
 */
delegate OnFindLobbiesComplete(bool bWasSuccessful, const out array<BasicLobbyInfo> LobbyList);

/**
 * Triggers all 'OnFindLobbiesComplete' delegates; done from UScript, as C++ can't pass CachedFindLobbyResults as an out parameter, without copying
 *
 * @param bWasSuccessful	whether or not 'FindLobbies' was successful
 */
event TriggerFindLobbiesCompleteDelegates(bool bWasSuccessful)
{
	local array<delegate<OnFindLobbiesComplete> > DelList;
	local delegate<OnFindLobbiesComplete> CurDel;

	DelList = FindLobbiesCompleteDelegates;

	foreach DelList(CurDel)
	{
		CurDel(bWasSuccessful, CachedFindLobbyResults);
	}
}

/**
 * Sets the delegate used to notify when a call to 'FindLobbies' has completed
 *
 * @param FindLobbiesCompleteDelegate	The delegate to use for notifications
 */
function AddFindLobbiesCompleteDelegate(delegate<OnFindLobbiesComplete> FindLobbiesCompleteDelegate)
{
	if (FindLobbiesCompleteDelegates.Find(FindLobbiesCompleteDelegate) == INDEX_None)
		FindLobbiesCompleteDelegates[FindLobbiesCompleteDelegates.Length] = FindLobbiesCompleteDelegate;
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param FindLobbiesCompleteDelegate	The delegate to remove from the list
 */
function ClearFindLobbiesCompleteDelegate(delegate<OnFindLobbiesComplete> FindLobbiesCompleteDelegate)
{
	local int i;

	i = FindLobbiesCompleteDelegates.Find(FindLobbiesCompleteDelegate);

	if (i != INDEX_None)
		FindLobbiesCompleteDelegates.Remove(i, 1);
}


/**
 * Joins the specified lobby, triggering callbacks when done
 *
 * @param LobbyId	The unique id of the lobby to join
 * @return		Returns True if successful, False otherwise
 */
native function bool JoinLobby(UniqueNetId LobbyId);

/**
 * Called when 'JoinLobby' completes, returning success/failure, and (if successful) the full lobby info
 *
 * @param bWasSuccessful	whether or not 'JoinLobby' was successful
 * @param LobbyList		The list of active lobbies
 * @param LobbyIndex		The index of the lobby we joined
 * @param LobbyUID		The UID of the lobby (for when joining failed, and there is no valid LobbyIndex)
 * @param Error			If 'JoinLobby' failed, returns the error type
 */
delegate OnJoinLobbyComplete(bool bWasSuccessful, const out array<ActiveLobbyInfo> LobbyList, int LobbyIndex, UniqueNetId LobbyUID, string Error);

/**
 * Triggers all 'OnJoinLobbyComplete' delegates; done from UScript, as C++ can't pass ActiveLobbies as an out parameter, without copying it
 *
 * @param bWasSuccessful	whether or not 'JoinLobby' was successful
 * @param LobbyIndex		The index of the lobby we joined
 * @param LobbyUID		The UID of the lobby (for when joining failed, and there is no valid LobbyIndex)
 * @param Error			If 'JoinLobby' failed, returns the error type
 */
event TriggerJoinLobbyCompleteDelegates(bool bWasSuccessful, int LobbyIndex, UniqueNetId LobbyUID, string Error)
{
	local array<delegate<OnJoinLobbyComplete> > DelList;
	local delegate<OnJoinLobbyComplete> CurDel;

	DelList = JoinLobbyCompleteDelegates;

	foreach DelList(CurDel)
	{
		CurDel(bWasSuccessful, ActiveLobbies, LobbyIndex, LobbyUID, Error);
	}
}

/**
 * Sets the delegate used to notify when a call to 'JoinLobby' has completed
 *
 * @param JoinLobbyCompleteDelegate	The delegate to use for notifications
 */
function AddJoinLobbyCompleteDelegate(delegate<OnJoinLobbyComplete> JoinLobbyCompleteDelegate)
{
	if (JoinLobbyCompleteDelegates.Find(JoinLobbyCompleteDelegate) == INDEX_None)
		JoinLobbyCompleteDelegates[JoinLobbyCompleteDelegates.Length] = JoinLobbyCompleteDelegate;
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param JoinLobbyCompleteDelegate	The delegate to remove from the list
 */
function ClearJoinLobbyCompleteDelegate(delegate<OnJoinLobbyComplete> JoinLobbyCompleteDelegate)
{
	local int i;

	i = JoinLobbyCompleteDelegates.Find(JoinLobbyCompleteDelegate);

	if (i != INDEX_None)
		JoinLobbyCompleteDelegates.Remove(i, 1);
}

/**
 * Exits the specified lobby; always returns True, and has no callbacks
 * @todo Steam: Is the corresponding callback for this 'OnLobbyKicked', since it says that is triggered upon disconnect as well?
 *
 * @param LobbyId	The UID of the lobby to exit
 * @return		Returns True if successful, False otherwise
 */
native function bool LeaveLobby(UniqueNetId LobbyId);


/**
 * Changes the value of a setting for the local user in the specified lobby
 * NOTE: You should specify any keys you set, in the 'LobbyMemberKeys' config array; otherwise they aren't read
 *
 * @param LobbyId	The UID of the lobby where the change is to be applied
 * @param Key		The name of the setting to change
 * @param Value		The new value of the setting
 */
native function bool SetLobbyUserSetting(UniqueNetId LobbyId, string Key, string Value);

/**
 * Sends a chat message to the specified lobby
 *
 * @param LobbyId	The UID of the lobby where the message should be sent
 * @param Message	The message to send to the lobby
 * @return		Returns True if the message was sent successfully, False otherwise
 */
native function bool SendLobbyMessage(UniqueNetId LobbyId, string Message);

/**
 * Sends binary data to the specified lobby
 *
 * @param LobbyId	The UID of the lobby where the data should be sent
 * @param Data		The binary data which should be sent to the lobby (limit of around 2048 bytes)
 */
native function bool SendLobbyBinaryData(UniqueNetId LobbyId, const out array<byte> Data);

/**
 * Called when lobby settings have been updated
 *
 * @param LobbyList		The list of active lobbies
 * @param LobbyIndex		The index of the lobby whose settings have been updated
 */
delegate OnLobbySettingsUpdate(const out array<ActiveLobbyInfo> LobbyList, int LobbyIndex);

/**
 * Triggers all 'OnLobbySettingsUpdate' delegates; done from UScript, as C++ can't pass ActiveLobbies as an out parameter, without copying it
 *
 * @param LobbyIndex		The index of the lobby whose settings have been updated
 */
event TriggerLobbySettingsUpdateDelegates(int LobbyIndex)
{
	local array<delegate<OnLobbySettingsUpdate> > DelList;
	local delegate<OnLobbySettingsUpdate> CurDel;

	DelList = LobbySettingsUpdateDelegates;

	foreach DelList(CurDel)
	{
		CurDel(ActiveLobbies, LobbyIndex);
	}
}

/**
 * Sets the delegate used to notify when a lobbies settings have updated
 *
 * @param LobbySettingsUpdateDelegate	The delegate to use for notifications
 */
function AddLobbySettingsUpdateDelegate(delegate<OnLobbySettingsUpdate> LobbySettingsUpdateDelegate)
{
	if (LobbySettingsUpdateDelegates.Find(LobbySettingsUpdateDelegate) == INDEX_None)
		LobbySettingsUpdateDelegates[LobbySettingsUpdateDelegates.Length] = LobbySettingsUpdateDelegate;
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param LobbySettingsUpdateDelegate	The delegate to remove from the list
 */
function ClearLobbySettingsUpdateDelegate(delegate<OnLobbySettingsUpdate> LobbySettingsUpdateDelegate)
{
	local int i;

	i = LobbySettingsUpdateDelegates.Find(LobbySettingsUpdateDelegate);

	if (i != INDEX_None)
		LobbySettingsUpdateDelegates.Remove(i, 1);
}

/**
 * Called when the settings of a specific lobby member have been updated
 *
 * @param LobbyList		The list of active lobbies
 * @param LobbyIndex		The index of the lobby
 * @param MemberIndex		The index of the member whose settings have been updated
 */
delegate OnLobbyMemberSettingsUpdate(const out array<ActiveLobbyInfo> LobbyList, int LobbyIndex, int MemberIndex);

/**
 * Triggers all 'OnLobbyMemberSettingsUpdate' delegates; done from UScript, as C++ can't pass ActiveLobbies as an out parameter, without copying it
 * @param LobbyIndex		The index of the lobby
 * @param MemberIndex		The index of the member whose settings have been updated
 */
event TriggerLobbyMemberSettingsUpdateDelegates(int LobbyIndex, int MemberIndex)
{
	local array<delegate<OnLobbyMemberSettingsUpdate> > DelList;
	local delegate<OnLobbyMemberSettingsUpdate> CurDel;

	DelList = LobbyMemberSettingsUpdateDelegates;

	foreach DelList(CurDel)
	{
		CurDel(ActiveLobbies, LobbyIndex, MemberIndex);
	}
}

/**
 * Sets the delegate used to notify when a lobby members settings have been updated
 *
 * @param LobbyMemberSettingsUpdateDelegate		The delegate to use for notifications
 */
function AddLobbyMemberSettingsUpdateDelegate(delegate<OnLobbyMemberSettingsUpdate> LobbyMemberSettingsUpdateDelegate)
{
	if (LobbyMemberSettingsUpdateDelegates.Find(LobbyMemberSettingsUpdateDelegate) == INDEX_None)
		LobbyMemberSettingsUpdateDelegates[LobbyMemberSettingsUpdateDelegates.Length] = LobbyMemberSettingsUpdateDelegate;
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param LobbyMemberSettingsUpdateDelegate 	The delegate to remove from the list
 */
function ClearLobbyMemberSettingsUpdateDelegate(delegate<OnLobbyMemberSettingsUpdate> LobbyMemberSettingsUpdateDelegate)
{
	local int i;

	i = LobbyMemberSettingsUpdateDelegates.Find(LobbyMemberSettingsUpdateDelegate);

	if (i != INDEX_None)
		LobbyMemberSettingsUpdateDelegates.Remove(i, 1);
}

/**
 * Called when the status of a lobby member changes (e.g. entering/leaving)
 * NOTE: If the lobby member was kicked/banned, InstigatorIndex is set to the lobby member who kicked/banned the player
 *
 * @param LobbyList		The list of active lobbies
 * @param LobbyIndex		The index of the lobby
 * @param MemberIndex		The index of the member whose status has changed
 * @param InstigatorIndex	The index of the member (probably admin) who changed the other members status (may be INDEX_None)
 * @param Status		The new status of the member
 */
delegate OnLobbyMemberStatusUpdate(const out array<ActiveLobbyInfo> LobbyList, int LobbyIndex, int MemberIndex, int InstigatorIndex,
					string Status);

/**
 * Triggers all 'OnLobbyMemberStatusUpdate' delegates; done from UScript, as C++ can't pass ActiveLobbies as an out parameter, without copying it
 *
 * @param LobbyIndex		The index of the lobby
 * @param MemberIndex		The index of the member whose status has changed
 * @param InstigatorIndex	The index of the member (probably admin) who changed the other members status (may be INDEX_None)
 * @param Status		The new status of the member
 */
event TriggerLobbyMemberStatusUpdateDelegates(int LobbyIndex, int MemberIndex, int InstigatorIndex, string Status)
{
	local array<delegate<OnLobbyMemberStatusUpdate> > DelList;
	local delegate<OnLobbyMemberStatusUpdate> CurDel;

	DelList = LobbyMemberStatusUpdateDelegates;

	foreach DelList(CurDel)
	{
		CurDel(ActiveLobbies, LobbyIndex, MemberIndex, InstigatorIndex, Status);
	}
}

/**
 * Sets the delegate used to notify when a lobby members status has changed
 *
 * @param LobbyMemberStatusUpdateDelegate	The delegate to use for notifications
 */
function AddLobbyMemberStatusUpdateDelegate(delegate<OnLobbyMemberStatusUpdate> LobbyMemberStatusUpdateDelegate)
{
	if (LobbyMemberStatusUpdateDelegates.Find(LobbyMemberStatusUpdateDelegate) == INDEX_None)
		LobbyMemberStatusUpdateDelegates[LobbyMemberStatusUpdateDelegates.Length] = LobbyMemberStatusUpdateDelegate;
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param LobbyMemberStatusUpdateDelegate	The delegate to remove from the list
 */
function ClearLobbyMemberStatusUpdateDelegate(delegate<OnLobbyMemberStatusUpdate> LobbyMemberStatusUpdateDelegate)
{
	local int i;

	i = LobbyMemberStatusUpdateDelegates.Find(LobbyMemberStatusUpdateDelegate);

	if (i != INDEX_None)
		LobbyMemberStatusUpdateDelegates.Remove(i, 1);
}


/**
 * Called when a chat message has been received from the lobby
 * @todo Steam: Remove 'Type' if it's not used by Steam
 *
 * @param LobbyList	The list of active lobbies
 * @param LobbyIndex	The index of the lobby the message came from
 * @param MemberIndex	The index of the member the message is from
 * @param Type		The type of message (chat/is-typing/game-starting/etc.)
 * @param Message	The actual message
 */
delegate OnLobbyReceiveMessage(const out array<ActiveLobbyInfo> LobbyList, int LobbyIndex, int MemberIndex, string Type, string Message);

/**
 * Triggers all 'OnLobbyReceiveMessage' delegates; done from UScript, as C++ can't pass ActiveLobbies as an out parameter, without copying it
 *
 * @param LobbyIndex	The index of the lobby the message came from
 * @param MemberIndex	The index of the member the message is from
 * @param Type		The type of message (chat/is-typing/game-starting/etc.)
 * @param Message	The actual message
 */
event TriggerLobbyReceiveMessageDelegates(int LobbyIndex, int MemberIndex, string Type, string Message)
{
	local array<delegate<OnLobbyReceiveMessage> > DelList;
	local delegate<OnLobbyReceiveMessage> CurDel;

	DelList = LobbyReceiveMessageDelegates;

	foreach DelList(CurDel)
	{
		CurDel(ActiveLobbies, LobbyIndex, MemberIndex, Type, Message);
	}
}

/**
 * Sets the delegate used to notify when a lobby message is received
 *
 * @param LobbyReceiveMessageDelegate		The delegate to use for notifications
 */
function AddLobbyReceiveMessageDelegate(delegate<OnLobbyReceiveMessage> LobbyReceiveMessageDelegate)
{
	if (LobbyReceiveMessageDelegates.Find(LobbyReceiveMessageDelegate) == INDEX_None)
		LobbyReceiveMessageDelegates[LobbyReceiveMessageDelegates.Length] = LobbyReceiveMessageDelegate;
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param LobbyReceiveMessageDelegate		The delegate to remove from the list
 */
function ClearLobbyReceiveMessageDelegate(delegate<OnLobbyReceiveMessage> LobbyReceiveMessageDelegate)
{
	local int i;

	i = LobbyReceiveMessageDelegates.Find(LobbyReceiveMessageDelegate);

	if (i != INDEX_None)
		LobbyReceiveMessageDelegates.Remove(i, 1);
}

/**
 * Called when a binary message has been received from the lobby
 *
 * @param LobbyList	The list of active lobbies
 * @param LobbyIndex	The index of the lobby the message came from
 * @param MemberIndex	The index of the member the message is from
 * @param Data		The received binary data
 */
delegate OnLobbyReceiveBinaryData(const out array<ActiveLobbyInfo> LobbyList, int LobbyIndex, int MemberIndex, const out array<byte> Data);

/**
 * Triggers all 'OnLobbyReceiveBinaryData' delegates; done from UScript, as C++ can't pass ActiveLobbies/CachedBinaryData as out parameters
 *
 * @param LobbyIndex	The index of the lobby the message came from
 * @param MemberIndex	The index of the member the message is from
 */
event TriggerLobbyReceiveBinaryDataDelegates(int LobbyIndex, int MemberIndex)
{
	local array<delegate<OnLobbyReceiveBinaryData> > DelList;
	local delegate<OnLobbyReceiveBinaryData> CurDel;

	DelList = LobbyReceiveBinaryDataDelegates;

	foreach DelList(CurDel)
	{
		CurDel(ActiveLobbies, LobbyIndex, MemberIndex, CachedBinaryData);
	}
}

/**
 * Sets the delegate used to notify when binary data is received from a lobby
 *
 * @param LobbyReceiveMessageDelegate		The delegate to use for notifications
 */
function AddLobbyReceiveBinaryDataDelegate(delegate<OnLobbyReceiveBinaryData> LobbyReceiveBinaryDataDelegate)
{
	if (LobbyReceiveBinaryDataDelegates.Find(LobbyReceiveBinaryDataDelegate) == INDEX_None)
		LobbyReceiveBinaryDataDelegates[LobbyReceiveBinaryDataDelegates.Length] = LobbyReceiveBinaryDataDelegate;
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param LobbyReceiveBinaryDataDelegate	The delegate to remove from the list
 */
function ClearLobbyReceiveBinaryDataDelegate(delegate<OnLobbyReceiveBinaryData> LobbyReceiveBinaryDataDelegate)
{
	local int i;

	i = LobbyReceiveBinaryDataDelegates.Find(LobbyReceiveBinaryDataDelegate);

	if (i != INDEX_None)
		LobbyReceiveBinaryDataDelegates.Remove(i, 1);
}

/**
 * Called when the lobby activity has completed, and the player is directed towards a server
 * NOTE: Player does not automatically leave the lobby when this is triggered
 *
 * @param LobbyList	The list of active lobbies
 * @param LobbyIndex	The index of the lobby making the join-game request
 * @param ServerId	The UID of the server to join
 * @param ServerIP	The IP of the server to join
 */
delegate OnLobbyJoinGame(const out array<ActiveLobbyInfo> LobbyList, int LobbyIndex, UniqueNetId ServerId, string ServerIP);

/**
 * Triggers all 'OnLobbyJoinGame' delegates; done from UScript, as C++ can't pass ActiveLobbies as an out parameter, without copying it
 *
 * @param LobbyIndex	The index of the lobby making the join-game request
 * @param ServerId	The UID of the server to join
 * @param ServerIP	The IP of the server to join
 */
event TriggerLobbyJoinGameDelegates(int LobbyIndex, UniqueNetId ServerId, string ServerIP)
{
	local array<delegate<OnLobbyJoinGame> > DelList;
	local delegate<OnLobbyJoinGame> CurDel;

	DelList = LobbyJoinGameDelegates;

	foreach DelList(CurDel)
	{
		CurDel(ActiveLobbies, LobbyIndex, ServerId, ServerIP);
	}
}

/**
 * Sets the delegate used to notify when a lobby directs the player towards a server
 *
 * @param LobbyJoinGameDelegate		The delegate to use for notifications
 */
function AddLobbyJoinGameDelegate(delegate<OnLobbyJoinGame> LobbyJoinGameDelegate)
{
	if (LobbyJoinGameDelegates.Find(LobbyJoinGameDelegate) == INDEX_None)
		LobbyJoinGameDelegates[LobbyJoinGameDelegates.Length] = LobbyJoinGameDelegate;
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param LobbyJoinGameDelegate		The delegate to remove from the list
 */
function ClearLobbyJoinGameDelegate(delegate<OnLobbyJoinGame> LobbyJoinGameDelegate)
{
	local int i;

	i = LobbyJoinGameDelegates.Find(LobbyJoinGameDelegate);

	if (i != INDEX_None)
		LobbyJoinGameDelegates.Remove(i, 1);
}

/**
 * Called when the
 * @todo Steam: IMPORTANT: It is suggested by searching the Steam partner documentation, that this may not even be implemented;
 *		remove it if you can't test it, and don't implement this callback until you have tested the native part
 * @todo Steam: Should this be named OnLobbyDisconnect? Does this get called when the player leaves a lobby through LeaveLobby?
 * @todo Steam: Update this to take a const out array, with an index
 */
delegate OnLobbyKicked(const out array<ActiveLobbyInfo> LobbyList, int LobbyIndex, int AdminIndex);


/**
 * Returns the UID of the person who is admin of the specified lobby
 *
 * @param LobbyId	The UID of the lobby to check
 * @param AdminId	Outputs the UID of the lobby admin
 * @return		Returns True if successful, False otherwise
 */
native function bool GetLobbyAdmin(UniqueNetId LobbyId, out UniqueNetId AdminId);

/**
 * Sets the value of a specified setting, in the specified lobby
 * NOTE: Admin-only
 *
 * @param LobbyId	The UID of the lobby where the setting should be changed
 * @param Key		The name of the setting to change
 * @param Value		The new value for the setting
 * @return		Returns True if successful, False otherwise
 */
native function bool SetLobbySetting(UniqueNetId LobbyId, string Key, string Value);

/**
 * Removes the specified setting, from the specified lobby
 * NOTE: Admin-only
 *
 * @param LobbyId	The UID of the lobby where the setting should be removed
 * @param Key		The name of the setting to remove
 * @return		Returns True if successful, False otherwise
 */
native function bool RemoveLobbySetting(UniqueNetId LobbyId, string Key);

/**
 * Sets the game server to be joined for the specified lobby
 * NOTE: Admin-only
 *
 * @param LobbyId	The UID of the lobby where the game server should be set
 * @param ServerUID	The UID of the game server
 * @param ServerIP	The IP address of the game server
 * @return		Returns True if successful, False otherwise
 */
native function bool SetLobbyServer(UniqueNetId LobbyId, UniqueNetId ServerUID, string ServerIP);

/**
 * Changes the visibility/connectivity type for the specified lobby
 * NOTE: Admin-only
 *
 * @param LobbyId	The UID of the lobby where the change should be made
 * @param Type		The new visibility/connectivity type for the lobby
 * @return		Returns True if successful, False otherwise
 */
native function bool SetLobbyType(UniqueNetId LobbyId, ELobbyVisibility Type);

/**
 * Locks/unlocks the specified lobby (i.e. sets whether or not people can join it, regardless of friend/invite status)
 * NOTE: Admin-only
 *
 * @param LobbyId	The UID of the lobby to be locked/unlocked
 * @param bLocked	whether to lock or unlock the lobby
 * @return		Returns True if successful, False otherwise
 */
native function bool SetLobbyLock(UniqueNetId LobbyId, bool bLocked);

/**
 * Changes the owner of the specfied lobby
 * NOTE: Admin-only
 *
 * @param LobbyId	The UID of the lobby where ownership should be changed
 * @param NewOwner	The UID of the new lobby owner (must be present in lobby)
 */
native function bool SetLobbyOwner(UniqueNetId LobbyId, UniqueNetId NewOwner);

/**
 * Invites a player to the specified lobby
 *
 * @param LobbyId	The UID of the lobby to invite the player to
 * @param PlayerId	The UID of the player to invite
 * @return		Returns True if the invitation was sent successfully, False otherwise
 */
native function bool InviteToLobby(UniqueNetId LobbyId, UniqueNetId PlayerId);

/**
 * Called when the user receives or accepts a lobby invite
 *
 * @param LobbyId	The UID of the lobby the player was invited to
 * @param FriendId	The UID of the player who invited the user to the lobby (may be invalid, if not invited directly by a friend)
 * @param bAccepted	whether or not the player has already accepted the invite
 */
delegate OnLobbyInvite(UniqueNetId LobbyId, UniqueNetId FriendId, bool bAccepted);

/**
 * Sets the delegate used to notify when the player receives or accepts a lobby invite
 *
 * @param AcceptLobbyInviteDelegate	The delegate to use for notifications
 */
function AddLobbyInviteDelegate(delegate<OnLobbyInvite> LobbyInviteDelegate)
{
	if (LobbyInviteDelegates.Find(LobbyInviteDelegate) == INDEX_None)
		LobbyInviteDelegates[LobbyInviteDelegates.Length] = LobbyInviteDelegate;
}

/**
 * Removes the specifed delegate from the notification list
 *
 * @param LobbyInviteDelegate	The delegate to remove from the list
 */
function ClearLobbyInviteDelegate(delegate<OnLobbyInvite> LobbyInviteDelegate)
{
	local int i;

	i = LobbyInviteDelegates.Find(LobbyInviteDelegate);

	if (i != INDEX_None)
		LobbyInviteDelegates.Remove(i, 1);
}

/**
 * If the player accepted a lobby invite from outside of the game, this grabs the lobby UID from the commandline
 *
 * @param LobbyId		Outputs the UID of the lobby to be joined
 * @param bMarkAsJoined		Set this when the lobby is joined; future calls to this function will return False, but will still output the UID
 * @return			Returns True if a lobby UID is on the commandline, but returns False if it has been joined
 */
native function bool GetLobbyFromCommandline(out UniqueNetId LobbyId, optional bool bMarkAsJoined=True);


`endif



