/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Class that implements the Steamworks specific functionality
 */
class OnlineGameInterfaceSteamworks extends OnlineGameInterfaceImpl within OnlineSubsystemCommonImpl
	native
	config(Engine);


/** Maps a Steam HServerQuery to a Steam server rules callback object. */
struct native ServerQueryToRulesResponseMapping
{
	/** The Steam query handle */
	var int Query;
	/** The Steam callback object */
	var native pointer Response{FOnlineAsyncTaskSteamServerRulesRequest};
};

/** Maps a Steam HServerQuery to a Steam server rules callback object. */
struct native ServerQueryToPingResponseMapping
{
	/** The Steam query handle */
	var int Query;
	/** The Steam callback object */
	var native pointer Response{FOnlineAsyncTaskSteamServerPingRequest};
};

/** The type of server search we're doing at the moment. */
enum ESteamMatchmakingType
{
	SMT_Invalid,
	SMT_LAN,
	SMT_Internet
};

/** Struct representing a bunch of clientside filters which are combined with the OR '||' operator */
struct native ClientFilterORClause
{
	var transient native MultiMap_Mirror OrParams{FilterMap};
};


/**
 * Encapsulates all the variables required for tracking an online game search
 */
struct native MatchmakingQueryState
{
	/** The current game search object in use (NOTE: For ServerBrowserSearchQuery, this matches OnlineGameInterfaceSteamworks.GameSearch) */
	var const OnlineGameSearch				GameSearch;


	/** Stores in-progress Steam query handles */
	var array<ServerQueryToRulesResponseMapping>		QueryToRulesResponseMap;

	/** Stores in-progress Steam query handles */
	var array<ServerQueryToPingResponseMapping>		QueryToPingResponseMap;

	/** Provides callbacks when there are master server results */
	var native pointer					ServerListResponse{FOnlineAsyncTaskSteamServerListRequest};

	/** The kind of server search in progress */
	var ESteamMatchmakingType				CurrentMatchmakingType;

	/** Handle to in-progress Steam server query */
	var native pointer					CurrentMatchmakingQuery{void};


	/** An array of clientside search filters, (combined with the AND '&&' operator) for the active browser query */
	var transient native array<ClientFilterORClause>	ActiveClientsideFilters;


	/** Holds OnlineGameSettings objects bound to Steam queries, so that they don't get garbage collected in rare circumstances */
	var array<OnlineGameSettings>				PendingRulesSearchSettings;

	/** As above, but for ping queries */
	var array<OnlineGameSettings>				PendingPingSearchSettings;

	/** Used internally to ignore a callback */
	var bool						bIgnoreRefreshComplete;


	/** The last time there was activity on this game search (for implementing timeouts) */
	var float						LastActivityTimestamp;
};

/** Matchmaking query state for tracking server browser queries */
var const MatchmakingQueryState ServerBrowserSearchQuery;

/** Matchmaking query state for tracking invite 'find-server' queries */
var const MatchmakingQueryState InviteSearchQuery;

/** The length of time before server browser queries should time out (loaded from OnlineSubsystemSteamworks config) */
var float ServerBrowserTimeout;

/** The length of time before invite search queries should time out (loaded from OnlineSubsystemSteamworks config) */
var float InviteTimeout;


/** If we were invited to a steam sockets server, this stores the UID for filtering during the invite search, and connecting once complete */
var const UniqueNetId InviteServerUID;


/** The list of delegates to notify when a game invite is accepted */
var array<delegate<OnGameInviteAccepted> > GameInviteAcceptedDelegates;

/** Game game settings associated with this invite */
var const private OnlineGameSearch InviteGameSearch;

/** The last invite's URL information */
var const private string InviteLocationUrl;

/** This is the list of requested delegates to fire when complete */
var array<delegate<OnRegisterPlayerComplete> > RegisterPlayerCompleteDelegates;

/** This is the list of requested delegates to fire when complete */
var array<delegate<OnUnregisterPlayerComplete> > UnregisterPlayerCompleteDelegates;


/** whether or not to filter out servers with a mismatched engine server build (loaded from OnlineSubsystemSteamworks config) */
var bool bFilterEngineBuild;

/**
 * Maps an OnlineGameSearch filter, to a hard coded Steam filter. These filters are evaluated by the Valve master server, instead of clientside.
 * NOTE: Also used to map server-advertised keys/rules, to the hard-coded Steam filters
 * NOTE: If these filters are used in OR (||) operations, or use operators other than OGSCT_Equals, they are evaluted clientside
 * NOTE: For KeyType 'OGSET_ObjectProperty' use 'RawKey' to specify the object property name
 *
 * Useable Steam filter keys:
 *	map		- Mapname
 *	dedicated	- Dedicated servers
 *	secure		- whether or not VAC is enabled
 *	full		- Filter-out full servers
 *	empty		- Filter-out empty servers
 *	noplayers	- Show empty servers
 *	proxy		- Spectate-relay servers (for viewing games through a spectator proxy); not applicable to UE3
 */
struct native FilterKeyToSteamKeyMapping
{
	/** The id of the filter key, as used by OnlineGameSearch.OnlineGameSearchParameter (e.g. CONTEXT_MAPNAME i.e. 1) */
	var int				KeyId;
	/** The filter key type, as used by OnlineGameSearch.OnlineGameSearchParameter */
	var EOnlineGameSearchEntryType	KeyType;

	/** Optionally, the raw filter key can be specified (if set, the above values are ignored) */
	var string			RawKey;

	/** The Steam filter key, which the OnlineGameSearch key should be mapped to */
	var string			SteamKey;


	/** Treats the filter as a bool, and sets the opposite value for the Steam filter (if the final value is 'false', the filter is ignored) */
	var bool			bReverseFilter;

	/** If the filter value is equal to this, the filter is ignored (not passed on to Steam) */
	var string			IgnoreValue;
};

/** Maps OnlineGameSearch filters, to hard-coded Valve filters (speeds up the server browser, as Valve filters are evaluated on the master server) */
var config array<FilterKeyToSteamKeyMapping> FilterKeyToSteamKeyMap;


/**
 * Updates the localized settings/properties for the game in question
 *
 * @param SessionName the name of the session to update
 * @param UpdatedGameSettings the object to update the game settings with
 * @param bShouldRefreshOnlineData whether to submit the data to the backend or not
 *
 * @return true if successful creating the session, false otherwsie
 */
native function bool UpdateOnlineGame(name SessionName,OnlineGameSettings UpdatedGameSettings,optional bool bShouldRefreshOnlineData = false);

/**
 * Sets the delegate used to notify the gameplay code when a game invite has been accepted
 *
 * @param LocalUserNum the user to request notification for
 * @param GameInviteAcceptedDelegate the delegate to use for notifications
 */
function AddGameInviteAcceptedDelegate(byte LocalUserNum,delegate<OnGameInviteAccepted> GameInviteAcceptedDelegate)
{
	if (GameInviteAcceptedDelegates.Find(GameInviteAcceptedDelegate) == INDEX_NONE)
	{
		GameInviteAcceptedDelegates[GameInviteAcceptedDelegates.Length] = GameInviteAcceptedDelegate;
	}
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param LocalUserNum the user to request notification for
 * @param GameInviteAcceptedDelegate the delegate to use for notifications
 */
function ClearGameInviteAcceptedDelegate(byte LocalUserNum,delegate<OnGameInviteAccepted> GameInviteAcceptedDelegate)
{
	local int RemoveIndex;

	RemoveIndex = GameInviteAcceptedDelegates.Find(GameInviteAcceptedDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		GameInviteAcceptedDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Called when a user accepts a game invitation. Allows the gameplay code a chance
 * to clean up any existing state before accepting the invite. The invite must be
 * accepted by calling AcceptGameInvite() on the OnlineGameInterface after clean up
 * has completed
 *
 * @param InviteResult the search/settings for the game we're joining via invite
 */
delegate OnGameInviteAccepted(const out OnlineGameSearchResult InviteResult);

/**
 * Tells the online subsystem to accept the game invite that is currently pending
 *
 * @param LocalUserNum the local user accepting the invite
 * @param SessionName the name of the session this invite is to be known as
 *
 * @return true if the game invite was able to be accepted, false otherwise
 */
native function bool AcceptGameInvite(byte LocalUserNum,name SessionName);

/**
 * Registers a player with the online service as being part of the online game
 *
 * @param SessionName the name of the session the player is joining
 * @param UniquePlayerId the player to register with the online service
 * @param bWasInvited whether the player was invited to the game or searched for it
 *
 * @return true if the call succeeds, false otherwise
 */
native function bool RegisterPlayer(name SessionName,UniqueNetId PlayerId,bool bWasInvited);

/**
 * Delegate fired when the registration process has completed
 *
 * @param SessionName the name of the session the player joined or not
 * @param PlayerId the player that was unregistered from the online service
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnRegisterPlayerComplete(name SessionName,UniqueNetId PlayerId,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the player
 * registration request they submitted has completed
 *
 * @param RegisterPlayerCompleteDelegate the delegate to use for notifications
 */
function AddRegisterPlayerCompleteDelegate(delegate<OnRegisterPlayerComplete> RegisterPlayerCompleteDelegate)
{
	if (RegisterPlayerCompleteDelegates.Find(RegisterPlayerCompleteDelegate) == INDEX_NONE)
	{
		RegisterPlayerCompleteDelegates[RegisterPlayerCompleteDelegates.Length] = RegisterPlayerCompleteDelegate;
	}
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param RegisterPlayerCompleteDelegate the delegate to use for notifications
 */
function ClearRegisterPlayerCompleteDelegate(delegate<OnRegisterPlayerComplete> RegisterPlayerCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = RegisterPlayerCompleteDelegates.Find(RegisterPlayerCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		RegisterPlayerCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Unregisters a player with the online service as being part of the online game
 *
 * @param SessionName the name of the session the player is leaving
 * @param PlayerId the player to unregister with the online service
 *
 * @return true if the call succeeds, false otherwise
 */
native function bool UnregisterPlayer(name SessionName,UniqueNetId PlayerId);

/**
 * Delegate fired when the unregistration process has completed
 *
 * @param SessionName the name of the session the player left
 * @param PlayerId the player that was unregistered from the online service
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnUnregisterPlayerComplete(name SessionName,UniqueNetId PlayerId,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the player
 * Unregistration request they submitted has completed
 *
 * @param UnregisterPlayerCompleteDelegate the delegate to use for notifications
 */
function AddUnregisterPlayerCompleteDelegate(delegate<OnUnregisterPlayerComplete> UnregisterPlayerCompleteDelegate)
{
	if (UnregisterPlayerCompleteDelegates.Find(UnregisterPlayerCompleteDelegate) == INDEX_NONE)
	{
		UnregisterPlayerCompleteDelegates[UnregisterPlayerCompleteDelegates.Length] = UnregisterPlayerCompleteDelegate;
	}
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param UnregisterPlayerCompleteDelegate the delegate to use for notifications
 */
function ClearUnregisterPlayerCompleteDelegate(delegate<OnUnregisterPlayerComplete> UnregisterPlayerCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = UnregisterPlayerCompleteDelegates.Find(UnregisterPlayerCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		UnregisterPlayerCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Fetches the additional data a session exposes outside of the online service.
 * NOTE: notifications will come from the OnFindOnlineGamesComplete delegate
 *
 * @param StartAt the search result index to start gathering the extra information for
 * @param NumberToQuery the number of additional search results to get the data for
 *
 * @return true if the query was started, false otherwise
 */
function bool QueryNonAdvertisedData(int StartAt,int NumberToQuery)
{
	`Log("Ignored on Steamworks");  // ignored on Live, too.
	return false;
}


