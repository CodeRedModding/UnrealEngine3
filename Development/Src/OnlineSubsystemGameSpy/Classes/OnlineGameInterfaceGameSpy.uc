/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Class that implements the GameSpy specific functionality
 */
class OnlineGameInterfaceGameSpy extends OnlineGameInterfaceImpl within OnlineSubsystemCommonImpl
	native
	config(Engine);

/** stores a handle to the QR2 instance */
var native const transient private pointer QR2Handle{struct qr2_implementation_s};

/** stores a handle to the ServerBrowsing instance */
var native const transient private pointer SBHandle{struct _ServerBrowser};

/** The QR2 key id for the player id */
var const int QR2OwningPlayerIdKeyId;

/** The first databinding property key id in the list */
var const int QR2FirstDataBindableKeyId;

/** The number of databinding property keys */
var const int QR2NumDataBindableKeys;

/** The list of databindable properties that are registered as keys */
var const array<name> QR2DataBindableKeyNames;

/** The first localized setting key id in the list */
var const int QR2FirstSettingKeyId;

/** The number of localized settings keys */
var const int QR2NumSettingKeys;

/** The first property key id in the list */
var const int QR2FirstPropertyKeyId;

/** The number of registered property keys */
var const int QR2NumPropertyKeys;

/** The QR2 key id for the np id */
var const int QR2NpIdKeyId;

/** The string form of the np id */
var const string QR2NpIdString;

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

struct native SBServer
{
	var native const transient private pointer Server{_SBServer};
};

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
native function bool QueryNonAdvertisedData(int StartAt,int NumberToQuery);
