/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Class that implements a cross platform version of the game interface
 */
class OnlineGameInterfaceImpl extends Object within OnlineSubsystemCommonImpl
	native
	implements(OnlineGameInterface)
	config(Engine);

/** The owning subsystem that this object is providing an implementation for */
var OnlineSubsystemCommonImpl OwningSubsystem;

/** The current game settings object in use */
var const OnlineGameSettings GameSettings;

/** The current game search object in use */
var const OnlineGameSearch GameSearch;

/** Array of delegates to multicast with for game creation notification */
var array<delegate<OnCreateOnlineGameComplete> > CreateOnlineGameCompleteDelegates;

/** Array of delegates to multicast with for game update notification */
var array<delegate<OnUpdateOnlineGameComplete> > UpdateOnlineGameCompleteDelegates;

/** Array of delegates to multicast with for game destruction notification */
var array<delegate<OnDestroyOnlineGameComplete> > DestroyOnlineGameCompleteDelegates;

/** Array of delegates to multicast with for game join notification */
var array<delegate<OnJoinOnlineGameComplete> > JoinOnlineGameCompleteDelegates;

/** Array of delegates to multicast with for game migration notification */
var array<delegate<OnMigrateOnlineGameComplete> > MigrateOnlineGameCompleteDelegates;

/** Array of delegates to multicast with for migrated game join notification */
var array<delegate<OnJoinMigratedOnlineGameComplete> > JoinMigratedOnlineGameCompleteDelegates;

/** Array of delegates to multicast with for skill rating update notification */
var array<delegate<OnRecalculateSkillRatingComplete> > RecalculateSkillRatingCompleteDelegates;

/** Array of delegates to multicast with for game starting notification */
var array<delegate<OnStartOnlineGameComplete> > StartOnlineGameCompleteDelegates;

/** Array of delegates to multicast with for game ending notification */
var array<delegate<OnEndOnlineGameComplete> > EndOnlineGameCompleteDelegates;

/** Array of delegates to multicast with for game search notification */
var array<delegate<OnFindOnlineGamesComplete> > FindOnlineGamesCompleteDelegates;

/** Array of delegates to multicast with for game search notification */
var array<delegate<OnCancelFindOnlineGamesComplete> > CancelFindOnlineGamesCompleteDelegates;

/** The current state the lan beacon is in */
var const ELanBeaconState LanBeaconState;

/** Port to listen on for LAN queries/responses */
var const config int LanAnnouncePort;

/** Unique id to keep UE3 games from seeing each others' lan packets */
var const config int LanGameUniqueId;

/** Used by a client to uniquely identify itself during lan match discovery */
var const byte LanNonce[8];

/** Mask containing which platforms can cross communicate */
var const config int LanPacketPlatformMask;

/** The amount of time before the lan query is considered done */
var float LanQueryTimeLeft;

/** The amount of time to wait before timing out a lan query request */
var config float LanQueryTimeout;

/** LAN announcement socket used to send/receive discovery packets */
var const native transient pointer LanBeacon{FLanBeacon};

/** The session information used to connect to a host */
var native const transient private pointer SessionInfo{FSessionInfo};

/**
 * Delegate fired when the search for an online game has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnFindOnlineGamesComplete(bool bWasSuccessful);

/**
 * Returns the game settings object for the session with a matching name
 *
 * @param SessionName the name of the session to return
 *
 * @return the game settings for this session name
 */
function OnlineGameSettings GetGameSettings(name SessionName)
{
	return GameSettings;
}

/** Returns the currently set game search object */
function OnlineGameSearch GetGameSearch()
{
	return GameSearch;
}

/**
 * Creates an online game based upon the settings object specified.
 * NOTE: online game registration is an async process and does not complete
 * until the OnCreateOnlineGameComplete delegate is called.
 *
 * @param HostingPlayerNum the index of the player hosting the match
 * @param SessionName the name to use for this session so that multiple sessions can exist at the same time
 * @param NewGameSettings the settings to use for the new game session
 *
 * @return true if successful creating the session, false otherwise
 */
native function bool CreateOnlineGame(byte HostingPlayerNum,name SessionName,OnlineGameSettings NewGameSettings);

/**
 * Delegate fired when a create request has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnCreateOnlineGameComplete(name SessionName,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the online game they
 * created has completed the creation process
 *
 * @param CreateOnlineGameCompleteDelegate the delegate to use for notifications
 */
function AddCreateOnlineGameCompleteDelegate(delegate<OnCreateOnlineGameComplete> CreateOnlineGameCompleteDelegate)
{
	if (CreateOnlineGameCompleteDelegates.Find(CreateOnlineGameCompleteDelegate) == INDEX_NONE)
	{
		CreateOnlineGameCompleteDelegates[CreateOnlineGameCompleteDelegates.Length] = CreateOnlineGameCompleteDelegate;
	}
}

/**
 * Sets the delegate used to notify the gameplay code that the online game they
 * created has completed the creation process
 *
 * @param CreateOnlineGameCompleteDelegate the delegate to use for notifications
 */
function ClearCreateOnlineGameCompleteDelegate(delegate<OnCreateOnlineGameComplete> CreateOnlineGameCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = CreateOnlineGameCompleteDelegates.Find(CreateOnlineGameCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		CreateOnlineGameCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Updates the localized settings/properties for the game in question. Updates
 * the QoS packet if needed (starting & restarting QoS).
 *
 * @param SessionName the name of the session to update
 * @param UpdatedGameSettings the object to update the game settings with
 * @param bShouldRefreshOnlineData whether to submit the data to the backend or not
 *
 * @return true if successful creating the session, false otherwsie
 */
function bool UpdateOnlineGame(name SessionName,OnlineGameSettings UpdatedGameSettings,optional bool bShouldRefreshOnlineData = false);

/**
 * Delegate fired when a update request has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnUpdateOnlineGameComplete(name SessionName,bool bWasSuccessful);

/**
 * Adds a delegate to the list of objects that want to be notified
 *
 * @param UpdateOnlineGameCompleteDelegate the delegate to use for notifications
 */
function AddUpdateOnlineGameCompleteDelegate(delegate<OnUpdateOnlineGameComplete> UpdateOnlineGameCompleteDelegate)
{
	if (UpdateOnlineGameCompleteDelegates.Find(UpdateOnlineGameCompleteDelegate) == INDEX_NONE)
	{
		UpdateOnlineGameCompleteDelegates[UpdateOnlineGameCompleteDelegates.Length] = UpdateOnlineGameCompleteDelegate;
	}
}

/**
 * Removes a delegate from the list of objects that want to be notified
 *
 * @param UpdateOnlineGameCompleteDelegate the delegate to use for notifications
 */
function ClearUpdateOnlineGameCompleteDelegate(delegate<OnUpdateOnlineGameComplete> UpdateOnlineGameCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = UpdateOnlineGameCompleteDelegates.Find(UpdateOnlineGameCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		UpdateOnlineGameCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Destroys the current online game
 * NOTE: online game de-registration is an async process and does not complete
 * until the OnDestroyOnlineGameComplete delegate is called.
 *
 * @param SessionName the name of the session to delete
 *
 * @return true if successful destroying the session, false otherwsie
 */
native function bool DestroyOnlineGame(name SessionName);

/**
 * Delegate fired when a destroying an online game has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnDestroyOnlineGameComplete(name SessionName,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the online game they
 * destroyed has completed the destruction process
 *
 * @param DestroyOnlineGameCompleteDelegate the delegate to use for notifications
 */
function AddDestroyOnlineGameCompleteDelegate(delegate<OnDestroyOnlineGameComplete> DestroyOnlineGameCompleteDelegate)
{
	if (DestroyOnlineGameCompleteDelegates.Find(DestroyOnlineGameCompleteDelegate) == INDEX_NONE)
	{
		DestroyOnlineGameCompleteDelegates[DestroyOnlineGameCompleteDelegates.Length] = DestroyOnlineGameCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notification list
 *
 * @param DestroyOnlineGameCompleteDelegate the delegate to use for notifications
 */
function ClearDestroyOnlineGameCompleteDelegate(delegate<OnDestroyOnlineGameComplete> DestroyOnlineGameCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = DestroyOnlineGameCompleteDelegates.Find(DestroyOnlineGameCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		DestroyOnlineGameCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Searches for games matching the settings specified
 *
 * @param SearchingPlayerNum the index of the player searching for a match
 * @param SearchSettings the desired settings that the returned sessions will have
 *
 * @return true if successful searching for sessions, false otherwise
 */
native function bool FindOnlineGames(byte SearchingPlayerNum,OnlineGameSearch SearchSettings);

/**
 * Adds the delegate used to notify the gameplay code that the search they
 * kicked off has completed
 *
 * @param FindOnlineGamesCompleteDelegate the delegate to use for notifications
 */
function AddFindOnlineGamesCompleteDelegate(delegate<OnFindOnlineGamesComplete> FindOnlineGamesCompleteDelegate)
{
	// Only add to the list once
	if (FindOnlineGamesCompleteDelegates.Find(FindOnlineGamesCompleteDelegate) == INDEX_NONE)
	{
		FindOnlineGamesCompleteDelegates[FindOnlineGamesCompleteDelegates.Length] = FindOnlineGamesCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param FindOnlineGamesCompleteDelegate the delegate to use for notifications
 */
function ClearFindOnlineGamesCompleteDelegate(delegate<OnFindOnlineGamesComplete> FindOnlineGamesCompleteDelegate)
{
	local int RemoveIndex;
	// Find it in the list
	RemoveIndex = FindOnlineGamesCompleteDelegates.Find(FindOnlineGamesCompleteDelegate);
	// Only remove if found
	if (RemoveIndex != INDEX_NONE)
	{
		FindOnlineGamesCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Cancels the current search in progress if possible for that search type
 *
 * @return true if successful searching for sessions, false otherwise
 */
native function bool CancelFindOnlineGames();

/**
 * Delegate fired when the cancellation of a search for an online game has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnCancelFindOnlineGamesComplete(bool bWasSuccessful);

/**
 * Adds the delegate to the list to notify with
 *
 * @param CancelFindOnlineGamesCompleteDelegate the delegate to use for notifications
 */
function AddCancelFindOnlineGamesCompleteDelegate(delegate<OnCancelFindOnlineGamesComplete> CancelFindOnlineGamesCompleteDelegate)
{
	// Only add to the list once
	if (CancelFindOnlineGamesCompleteDelegates.Find(CancelFindOnlineGamesCompleteDelegate) == INDEX_NONE)
	{
		CancelFindOnlineGamesCompleteDelegates[CancelFindOnlineGamesCompleteDelegates.Length] = CancelFindOnlineGamesCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param CancelFindOnlineGamesCompleteDelegate the delegate to use for notifications
 */
function ClearCancelFindOnlineGamesCompleteDelegate(delegate<OnCancelFindOnlineGamesComplete> CancelFindOnlineGamesCompleteDelegate)
{
	local int RemoveIndex;
	// Find it in the list
	RemoveIndex = CancelFindOnlineGamesCompleteDelegates.Find(CancelFindOnlineGamesCompleteDelegate);
	// Only remove if found
	if (RemoveIndex != INDEX_NONE)
	{
		CancelFindOnlineGamesCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Cleans up any platform specific allocated data contained in the search results
 *
 * @param Search the object to free search results for
 *
 * @return true if successful, false otherwise
 */
native function bool FreeSearchResults(OnlineGameSearch Search);

/**
 * Joins the game specified
 *
 * @param PlayerNum the index of the player searching for a match
 * @param SessionName the name of the session to join
 * @param DesiredGame the desired game to join
 *
 * @return true if the call completed successfully, false otherwise
 */
native function bool JoinOnlineGame(byte PlayerNum,name SessionName,const out OnlineGameSearchResult DesiredGame);

/**
 * Delegate fired when the joing process for an online game has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnJoinOnlineGameComplete(name SessionName,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the join request they
 * kicked off has completed
 *
 * @param JoinOnlineGameCompleteDelegate the delegate to use for notifications
 */
function AddJoinOnlineGameCompleteDelegate(delegate<OnJoinOnlineGameComplete> JoinOnlineGameCompleteDelegate)
{
	if (JoinOnlineGameCompleteDelegates.Find(JoinOnlineGameCompleteDelegate) == INDEX_NONE)
	{
		JoinOnlineGameCompleteDelegates[JoinOnlineGameCompleteDelegates.Length] = JoinOnlineGameCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param JoinOnlineGameCompleteDelegate the delegate to use for notifications
 */
function ClearJoinOnlineGameCompleteDelegate(delegate<OnJoinOnlineGameComplete> JoinOnlineGameCompleteDelegate)
{
	local int RemoveIndex;
	// Find it in the list
	RemoveIndex = JoinOnlineGameCompleteDelegates.Find(JoinOnlineGameCompleteDelegate);
	// Only remove if found
	if (RemoveIndex != INDEX_NONE)
	{
		JoinOnlineGameCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Returns the platform specific connection information for joining the match.
 * Call this function from the delegate of join completion
 *
 * @param SessionName the name of the session to fetch the connection information for
 * @param ConnectInfo the out var containing the platform specific connection information
 *
 * @return true if the call was successful, false otherwise
 */
native function bool GetResolvedConnectString(name SessionName,out string ConnectInfo);

/**
 * Registers a player with the online service as being part of the online game
 *
 * @param SessionName the name of the session the player is joining
 * @param UniquePlayerId the player to register with the online service
 * @param bWasInvited whether the player was invited to the game or searched for it
 *
 * @return true if the call succeeds, false otherwise
 */
function bool RegisterPlayer(name SessionName,UniqueNetId PlayerId,bool bWasInvited);

/**
 * Registers a group of players with the online service as being part of the online game
 *
 * @param SessionName the name of the session the player is joining
 * @param Players the list of players to register with the online service
 *
 * @return true if the call succeeds, false otherwise
 */
function bool RegisterPlayers(name SessionName,const out array<UniqueNetId> Players);

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
function AddRegisterPlayerCompleteDelegate(delegate<OnRegisterPlayerComplete> RegisterPlayerCompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param RegisterPlayerCompleteDelegate the delegate to use for notifications
 */
function ClearRegisterPlayerCompleteDelegate(delegate<OnRegisterPlayerComplete> RegisterPlayerCompleteDelegate);

/**
 * Unregisters a player with the online service as being part of the online game
 *
 * @param SessionName the name of the session the player is leaving
 * @param PlayerId the player to unregister with the online service
 *
 * @return true if the call succeeds, false otherwise
 */
function bool UnregisterPlayer(name SessionName,UniqueNetId PlayerId);

/**
 * Unregisters a group of players with the online service as being part of the online game
 *
 * @param SessionName the name of the session the player is joining
 * @param Players the list of players to unregister with the online service
 *
 * @return true if the call succeeds, false otherwise
 */
function bool UnregisterPlayers(name SessionName,const out array<UniqueNetId> Players);

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
 * unregistration request they submitted has completed
 *
 * @param UnregisterPlayerCompleteDelegate the delegate to use for notifications
 */
function AddUnregisterPlayerCompleteDelegate(delegate<OnUnregisterPlayerComplete> UnregisterPlayerCompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param UnregisterPlayerCompleteDelegate the delegate to use for notifications
 */
function ClearUnregisterPlayerCompleteDelegate(delegate<OnUnregisterPlayerComplete> UnregisterPlayerCompleteDelegate);

/**
 * Marks an online game as in progress (as opposed to being in lobby or pending)
 *
 * @param SessionName the name of the session that is being started
 *
 * @return true if the call succeeds, false otherwise
 */
native function bool StartOnlineGame(name SessionName);

/**
 * Delegate fired when the online game has transitioned to the started state
 *
 * @param SessionName the name of the session the that has transitioned to started
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnStartOnlineGameComplete(name SessionName,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the online game has
 * transitioned to the started state.
 *
 * @param StartOnlineGameCompleteDelegate the delegate to use for notifications
 */
function AddStartOnlineGameCompleteDelegate(delegate<OnStartOnlineGameComplete> StartOnlineGameCompleteDelegate)
{
	if (StartOnlineGameCompleteDelegates.Find(StartOnlineGameCompleteDelegate) == INDEX_NONE)
	{
		StartOnlineGameCompleteDelegates[StartOnlineGameCompleteDelegates.Length] = StartOnlineGameCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param StartOnlineGameCompleteDelegate the delegate to use for notifications
 */
function ClearStartOnlineGameCompleteDelegate(delegate<OnStartOnlineGameComplete> StartOnlineGameCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = StartOnlineGameCompleteDelegates.Find(StartOnlineGameCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		StartOnlineGameCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Marks an online game as having been ended
 *
 * @param SessionName the name of the session the to end
 *
 * @return true if the call succeeds, false otherwise
 */
native function bool EndOnlineGame(name SessionName);

/**
 * Delegate fired when the online game has transitioned to the ending game state
 *
 * @param SessionName the name of the session the that was ended
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnEndOnlineGameComplete(name SessionName,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the online game has
 * transitioned to the ending state.
 *
 * @param EndOnlineGameCompleteDelegate the delegate to use for notifications
 */
function AddEndOnlineGameCompleteDelegate(delegate<OnEndOnlineGameComplete> EndOnlineGameCompleteDelegate)
{
	if (EndOnlineGameCompleteDelegates.Find(EndOnlineGameCompleteDelegate) == INDEX_NONE)
	{
		EndOnlineGameCompleteDelegates[EndOnlineGameCompleteDelegates.Length] = EndOnlineGameCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param EndOnlineGameCompleteDelegate the delegate to use for notifications
 */
function ClearEndOnlineGameCompleteDelegate(delegate<OnEndOnlineGameComplete> EndOnlineGameCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = EndOnlineGameCompleteDelegates.Find(EndOnlineGameCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		EndOnlineGameCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Tells the game to register with the underlying arbitration server if available
 *
 * @param SessionName the name of the session to register for arbitration with
 */
function bool RegisterForArbitration(name SessionName);

/**
 * Delegate fired when the online game has completed registration for arbitration
 *
 * @param SessionName the name of the session the that had arbitration pending
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnArbitrationRegistrationComplete(name SessionName,bool bWasSuccessful);

/**
 * Sets the notification callback to use when arbitration registration has completed
 *
 * @param ArbitrationRegistrationCompleteDelegate the delegate to use for notifications
 */
function AddArbitrationRegistrationCompleteDelegate(delegate<OnArbitrationRegistrationComplete> ArbitrationRegistrationCompleteDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param ArbitrationRegistrationCompleteDelegate the delegate to use for notifications
 */
function ClearArbitrationRegistrationCompleteDelegate(delegate<OnArbitrationRegistrationComplete> ArbitrationRegistrationCompleteDelegate);

/**
 * Returns the list of arbitrated players for the arbitrated session
 *
 * @param SessionName the name of the session to get the arbitration results for
 *
 * @return the list of players that are registered for this session
 */
function array<OnlineArbitrationRegistrant> GetArbitratedPlayers(name SessionName);

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
 * Sets the delegate used to notify the gameplay code when a game invite has been accepted
 *
 * @param LocalUserNum the user to request notification for
 * @param GameInviteAcceptedDelegate the delegate to use for notifications
 */
function AddGameInviteAcceptedDelegate(byte LocalUserNum,delegate<OnGameInviteAccepted> GameInviteAcceptedDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param LocalUserNum the user to request notification for
 * @param GameInviteAcceptedDelegate the delegate to use for notifications
 */
function ClearGameInviteAcceptedDelegate(byte LocalUserNum,delegate<OnGameInviteAccepted> GameInviteAcceptedDelegate);

/**
 * Tells the online subsystem to accept the game invite that is currently pending
 *
 * @param LocalUserNum the local user accepting the invite
 * @param SessionName the name of the session this invite is to be known as
 *
 * @return true if the game invite was able to be accepted, false otherwise
 */
function bool AcceptGameInvite(byte LocalUserNum,name SessionName);

/**
 * Updates the current session's skill rating using the list of players' skills
 *
 * @param SessionName the name of the session to update the skill rating for
 * @param Players the set of players to use in the skill calculation
 *
 * @return true if the update succeeded, false otherwise
 */
function bool RecalculateSkillRating(name SessionName,const out array<UniqueNetId> Players);

/**
 * Delegate fired when a skill rating request has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnRecalculateSkillRatingComplete(name SessionName,bool bWasSuccessful);

/**
 * Adds a delegate to the list of objects that want to be notified
 *
 * @param RecalculateSkillRatingCompleteDelegate the delegate to use for notifications
 */
function AddRecalculateSkillRatingCompleteDelegate(delegate<OnRecalculateSkillRatingComplete> RecalculateSkillRatingCompleteDelegate);

/**
 * Removes a delegate from the list of objects that want to be notified
 *
 * @param RecalculateSkillRatingCompleteDelegate the delegate to use for notifications
 */
function ClearRecalculateSkillRatingCompleteDelegate(delegate<OnRecalculateSkillRatingComplete> RecalculateSkillRatingGameCompleteDelegate);

/**
 * Migrates an existing online game on the host.
 * NOTE: online game migration is an async process and does not complete
 * until the OnMigrateOnlineGameComplete delegate is called.
 *
 * @param HostingPlayerNum the index of the player now hosting the match
 * @param SessionName the name of the existing session to migrate
 *
 * @return true if successful migrating the session, false otherwise
 */
function bool MigrateOnlineGame(byte HostingPlayerNum,name SessionName);

/**
 * Delegate fired when a create request has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnMigrateOnlineGameComplete(name SessionName,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code when the session migration completes
 *
 * @param MigrateOnlineGameCompleteDelegate the delegate to use for notifications
 */
function AddMigrateOnlineGameCompleteDelegate(delegate<OnMigrateOnlineGameComplete> MigrateOnlineGameCompleteDelegate);

/**
 * Removes the delegate from the list of notifications
 *
 * @param MigrateOnlineGameCompleteDelegate the delegate to use for notifications
 */
function ClearMigrateOnlineGameCompleteDelegate(delegate<OnMigrateOnlineGameComplete> MigrateOnlineGameCompleteDelegate);

/**
 * Joins the migrated game specified
 *
 * @param PlayerNum the index of the player about to join a match
 * @param SessionName the name of the migrated session to join
 * @param DesiredGame the desired migrated game to join
 *
 * @return true if the call completed successfully, false otherwise
 */
function bool JoinMigratedOnlineGame(byte PlayerNum,name SessionName,const out OnlineGameSearchResult DesiredGame);

/**
 * Delegate fired when the joing process for a migrated online game has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnJoinMigratedOnlineGameComplete(name SessionName,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the join request for a migrated session they
 * kicked off has completed
 *
 * @param JoinMigratedOnlineGameCompleteDelegate the delegate to use for notifications
 */
function AddJoinMigratedOnlineGameCompleteDelegate(delegate<OnJoinMigratedOnlineGameComplete> JoinMigratedOnlineGameCompleteDelegate)
{
	if (JoinMigratedOnlineGameCompleteDelegates.Find(JoinMigratedOnlineGameCompleteDelegate) == INDEX_NONE)
	{
		JoinMigratedOnlineGameCompleteDelegates.AddItem(JoinMigratedOnlineGameCompleteDelegate);
	}
}

/**
 * Removes the delegate from the list of notifications
 *
 * @param JoinMigratedOnlineGameCompleteDelegate the delegate to use for notifications
 */
function ClearJoinMigratedOnlineGameCompleteDelegate(delegate<OnJoinMigratedOnlineGameComplete> JoinMigratedOnlineGameCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = JoinMigratedOnlineGameCompleteDelegates.Find(JoinMigratedOnlineGameCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		JoinMigratedOnlineGameCompleteDelegates.Remove(RemoveIndex,1);
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
function bool QueryNonAdvertisedData(int StartAt,int NumberToQuery);

/**
 * Serializes the platform specific data into the provided buffer for the specified search result
 *
 * @param DesiredGame the game to copy the platform specific data for
 * @param PlatformSpecificInfo the buffer to fill with the platform specific information
 *
 * @return true if successful serializing the data, false otherwise
 */
native function bool ReadPlatformSpecificSessionInfo(const out OnlineGameSearchResult DesiredGame,out byte PlatformSpecificInfo[80]);

/**
 * Serializes the platform specific data into the provided buffer for the specified settings object.
 * NOTE: This can only be done for a session that is bound to the online system
 *
 * @param GameSettings the game to copy the platform specific data for
 * @param PlatformSpecificInfo the buffer to fill with the platform specific information
 *
 * @return true if successful reading the data for the session, false otherwise
 */
function bool ReadPlatformSpecificSessionInfoBySessionName(name SessionName,out byte PlatformSpecificInfo[80]);

/**
 * Creates a search result out of the platform specific data and adds that to the specified search object
 *
 * @param SearchingPlayerNum the index of the player searching for a match
 * @param SearchSettings the desired search to bind the session to
 * @param PlatformSpecificInfo the platform specific information to convert to a server object
 *
 * @return true if successful searching for sessions, false otherwise
 */
native function bool BindPlatformSpecificSessionToSearch(byte SearchingPlayerNum,OnlineGameSearch SearchSettings,byte PlatformSpecificInfo[80]);

/**
 * Delegate fired when QoS status has changed for a given search
 *
 * @param NumComplete the number completed thus far
 * @param NumTotal the number of QoS requests total
 */
delegate OnQosStatusChanged(int NumComplete,int NumTotal);

/**
 * Adds a delegate to the list of objects that want to be notified
 *
 * @param QosStatusChangedDelegate the delegate to use for notifications
 */
function AddQosStatusChangedDelegate(delegate<OnQosStatusChanged> QosStatusChangedDelegate);

/**
 * Removes the delegate from the list of notifications
 *
 * @param QosStatusChangedDelegate the delegate to use for notifications
 */
function ClearQosStatusChangedDelegate(delegate<OnQosStatusChanged> QosStatusChangedDelegate);
