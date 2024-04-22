/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class OnlineSubsystemGameSpy extends OnlineSubsystemCommonImpl
	native
	implements(OnlinePlayerInterface,OnlineVoiceInterface,OnlineStatsInterface,OnlineSystemInterface,OnlineAccountInterface)
	config(Engine);

/** Pointer to the object that handles the game interface */
var const OnlineGameInterfaceGameSpy CachedGameInt;

/** The name to use for local profiles */
var const localized string LocalProfileName;

/** The name of the player that is logged in */
var const string LoggedInPlayerName;

/** The unique id of the logged in player */
var const UniqueNetId LoggedInPlayerId;

/** Whether a login is in progress or not */
var const bool bIsLoginInProcess;

/**
 * Store the password temporarily during the login attempt for the login certificate request
 * Clear it as soon as it is used
 */
var const string LoggedInPlayerPassword;

/** The number of the player that called the login function */
var const int LoggedInPlayerNum;

/** The current login status for the player */
var const ELoginStatus LoggedInStatus;

/** The auth token (when doing remote auth) */
var const string RemoteAuthToken;

/** The partner challenge (when doing remote auth) */
var const string RemoteAuthPartnerChallenge;

/**
 * This is the array of pending async tasks. Each tick these tasks are checked
 * for completion. If complete, the delegate associated with them is called
 */
var native const array<pointer> AsyncTasks{FOnlineAsyncTaskGameSpy};

/** The directory profile data should be stored in */
var config string ProfileDataDirectory;

/** The file extension to use when saving profile data */
var config string ProfileDataExtension;

struct native PerUserProfileDelegates
{
	/** The array of delegates that notify read completion of profile data */
	var array<delegate<OnReadProfileSettingsComplete> > Delegates;
};

/** Holds callbacks for upto 4 splitscreen players */
var PerUserProfileDelegates PerUserReadProfileSettings[4];

/** The array of delegates that notify write completion of profile data */
var array<delegate<OnWriteProfileSettingsComplete> > WriteProfileSettingsDelegates;

/** The cached profile for the player */
var OnlineProfileSettings CachedProfile;

/** Holds the per player online storage data (only for local players) */
var OnlinePlayerStorage PlayerStorageCache[4];

/** Used for notification of player storage reads completing for local players */
var array<delegate<OnReadPlayerStorageComplete> > LocalPlayerStorageReadDelegates;
/** Used for notification of player storage writes completing for local players */
var array<delegate<OnWritePlayerStorageComplete> > LocalPlayerStorageWriteDelegates;
/** Used for notification of player storage reads completing for remote players */
var array<delegate<OnReadPlayerStorageForNetIdComplete> > RemotePlayerStorageReadDelegates;

/** List of callbacks to notify when speech recognition is complete */
var array<delegate<OnRecognitionComplete> > SpeechRecognitionCompleteDelegates;

/** The array of delegates that notify read completion of the friends list data */
var array<delegate<OnReadFriendsComplete> > ReadFriendsDelegates;

/** The array of delegates that notify that the friends list has changed */
var array<delegate<OnFriendsChange> > FriendsChangeDelegates;

/** The array of delegates that notify that the mute list has changed */
var array<delegate<OnMutingChange> > MutingChangeDelegates;

/** This is the list of requested delegates to fire when a login fails to process */
var array<delegate<OnLoginChange> > LoginChangeDelegates;

/** This is the list of requested delegates to fire when a login fails to process */
var array<delegate<OnLoginFailed> > LoginFailedDelegates;

/** This is the list of requested delegates to fire when a logout completes */
var array<delegate<OnLogoutCompleted> > LogoutCompletedDelegates;

/** This is the list of requested delegates to fire when an account create completes */
var array<delegate<OnCreateOnlineAccountCompleted> > AccountCreateDelegates;

/** This list is used to notify the game when a player is talking */
var array<delegate<OnPlayerTalkingStateChange> > PlayerTalkingDelegates;

/** This is the list of delegates requesting notification when a stats read finishes */
var array<delegate<OnReadOnlineStatsComplete> > ReadOnlineStatsCompleteDelegates;

/** The list of delegates to notify when the stats flush is complete */
var array<delegate<OnFlushOnlineStatsComplete> > FlushOnlineStatsDelegates;

/** This is the list of delegates requesting notification GameSpy's connection state changes */
var array<delegate<OnConnectionStatusChange> > ConnectionStatusChangeDelegates;

/** This is the list of delegates requesting notification of controller status changes */
var array<delegate<OnControllerChange> > ControllerChangeDelegates;

/** This is the list of delegates requesting notification of network link status changes */
var array<delegate<OnLinkStatusChange> > LinkStatusDelegates;

/** The list of delegates to notify when a network platform file is read */
var array<delegate<OnReadTitleFileComplete> > ReadTitleFileCompleteDelegates;

/** The list of delegate to notify when the achievements have been read */
var array<delegate<OnReadAchievementsComplete> > ReadAchievementsCompleteDelegates;

/** The list of delegate to notify when an achievement has been unlocked */
var array<delegate<OnUnlockAchievementComplete> > UnlockAchievementCompleteDelegates;

/** The types of global muting we support */
enum EMuteType
{
	MUTE_None,
	MUTE_AllButFriends,
	MUTE_All
};

/** Adds to the local talker definition so we can support muting */
struct native LocalTalkerGS extends LocalTalker
{
	var EMuteType MuteType;
};

/** Holds the local talker information for the single signed in player */
var LocalTalkerGS CurrentLocalTalker;

/** This is the list of remote talkers */
var array<RemoteTalker> RemoteTalkers;

/** Stores a handle to the GP instance */
var native const transient private pointer GPHandle{void};

/** Stores a handle to the Sake instance */
var native const transient private pointer SakeHandle{struct SAKEInternal};

/** Stores a handle to the SC (stats & competition) SDK */
var native const transient private pointer SCHandle{void};

/** Stores a login certificate */
var native const transient private pointer LoginCertificate{GSLoginCertificate};

/** Stores the login private data */
var native const transient private pointer LoginPrivateData{GSLoginPrivateData};

/** Stores the Sake recordid associated with the player's profile */
var const int SakeProfileRecordID;

/** Identifies the GameSpy game*/
var const config int GameID;

/** Identifies the GameSpy product */
var const config int ProductID;

/** Identifies the login namespace */
var const config int NamespaceID;

/** Identifies the login partner */
var const config int PartnerID;

/** The currently outstanding stats read request */
var const OnlineStatsRead CurrentStatsRead;

/** This should match the version configured through the GameSpy stats admin site */
var config int StatsVersion;

/** The stats key id for the nickname */
var config int NickStatsKeyId;

/** The stats key id for the player's place in the match */
var config int PlaceStatsKeyId;

/**
 * Maps a view and property to a gamespy stats key
 * If only PropertyId is 0, then this is the KeyId for the View itself
 */
struct native ViewPropertyToKeyId
{
	/** The id of the view */
	var int ViewId;
	/** The id of the property */
	var int PropertyId;
	/** The id of the gamespy stats key */
	var int KeyId;
};

/** Mappings of views and properties to gamespy stats keys */
var config array<ViewPropertyToKeyId> StatsKeyMappings;

/** This holds a single stat waiting to be written out */
struct native PlayerStat
{
	/** The GameSpy key for this stat */
	var int KeyId;
	/** The stat's value */
	var const SettingsData Data;
};

/** This stores the stats for a single player before being written out to the backend */
struct native PendingPlayerStats
{
	/** The player for which stats are being written */
	var const UniqueNetId Player;
	/** The name of the player to report with */
	var const string PlayerName;
	/** This is a per-player guid that needs to be passed to the backend */
	var const string StatGuid;
	/** The stats for this player */
	var const array<PlayerStat> Stats;
	/** The score for this player */
	var const OnlinePlayerScore Score;
	/** This player's place when sorted against the other players.  Calculated at reporting time */
	var const string Place;
};

/** Stats are stored in this array while waiting for FlushOnlineStats() */
var const array<PendingPlayerStats> PendingStats;

/** Holds the results of async keyboard input */
var const string KeyboardResultsString;

/** Whether the user canceled keyboard input or not */
var const byte bWasKeyboardInputCanceled;

/** Whether the keyboard needs to be ticked */
var const bool bNeedsKeyboardTicking;

/** This is the list of requested delegates to fire when keyboard UI has completed */
var array<delegate<OnKeyboardInputComplete> > KeyboardInputDelegates;

/** This is the list of requested delegates to fire when a friend invite is received */
var array<delegate<OnFriendInviteReceived> > FriendInviteReceivedDelegates;

/** This is the list of requested delegates to fire when a friend message is received */
var array<delegate<OnFriendMessageReceived> > FriendMessageReceivedDelegates;

/** This is the list of requested delegates to fire when a friend by name invite has completed*/
var array<delegate<OnAddFriendByNameComplete> > AddFriendByNameCompleteDelegates;

/** Used by the async add friend by name function */
var const string CachedFriendMessage;

/**
 * The list of location strings that are ok to accept invites for. Used mostly
 * the different platform skus use different location strings.
 */
var const config array<string> LocationUrlsForInvites;

/** The URL to send as the location string */
var const config string LocationUrl;

/** The list of subscribers for game invite events */
var array<delegate<OnReceivedGameInvite> > ReceivedGameInviteDelegates;

/** Holds the list of delegates that are interested in receiving join friend completions */
var array<delegate<OnJoinFriendGameComplete> > JoinFriendGameCompleteDelegates;

/** This is the list of requested delegates to fire when a host registration is complete */
var array<delegate<OnRegisterHostStatGuidComplete> > RegisterHostStatGuidCompleteDelegates;

/** The list of friend messages received while the game was running */
var array<OnlineFriendMessage> CachedFriendMessages;

/** Holds the items used to map an online status string to its format string */
struct native OnlineStatusMapping
{
	/** The id of the status string */
	var int StatusId;
	/** The format string to use to apply the passed in properties/strings */
	var localized string StatusString;
};

/** Holds the set of status strings for the specified game */
var const config array<OnlineStatusMapping> StatusMappings;

/** This is the default online status to use in status updates */
var const localized string DefaultStatus;

/** The message to use for game invites */
var const localized string GameInviteMessage;

/** Pointer to the PS3 specific data needed by GameSpy for single sign on */
var const native transient pointer NpData{FNpData};

/** Struct to hold current and previous frame's game state */
struct native ControllerConnectionState
{
	/** Whether the controller is connected or not */
	var const int bIsControllerConnected;
	/** Last frame's version of the above */
	var const int bLastIsControllerConnected;
};

/** Upto 4 player split screen support */
var ControllerConnectionState ControllerStates[4];

/** Whether the last frame has connection status or not */
var bool bLastHasConnection;

/** The amount of time to elapse before checking for connection status change */
var float ConnectionPresenceTimeInterval;

/** Used to check when to verify connection status */
var float ConnectionPresenceElapsedTime;

/** Whether the stats session is ok to add stats to etc */
var bool bIsStatsSessionOk;

/** Holds the product key in its encrypted form */
var private const config string EncryptedProductKey;

/** Whether the user has created a GameSpy account or not */
var private const config bool bHasGameSpyAccount;

/** An ever incrementing number assigned to auth requests */
var const transient int NextAuthId;

/** Holds the server auth challenge */
var const string ServerChallenge;

/** Holds the server auth response */
var const string ServerResponse;

/** Holds the server auth local id */
var const int ServerLocalId;

/** Holds the set of people that are muted by the currently logged in player */
var const array<UniqueNetId> MuteList;

/** Whether to use MCP for news or not */
var const config bool bShouldUseMcp;

/** Whether to request total view counts.  This requires an extra round trip to the server. */
var config bool bShouldStatsReadsRequestTotals;

/**
 * Called from engine start up code to allow the subsystem to initialize
 *
 * @return TRUE if the initialization was successful, FALSE otherwise
 */
native event bool Init();

/**
 * Delegate used in login notifications
 *
 * @param LocalUserNum the player that had the login change
 */
delegate OnLoginChange(byte LocalUserNum);

/**
 * Delegate used to notify when a login request was cancelled by the user
 */
delegate OnLoginCancelled();

/**
 * Delegate used in mute list change notifications
 */
delegate OnMutingChange();

/**
 * Delegate used in friends list change notifications
 */
delegate OnFriendsChange();

/**
 * Displays the UI that prompts the user for their login credentials. Each
 * platform handles the authentication of the user's data.
 *
 * @param bShowOnlineOnly whether to only display online enabled profiles or not
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
native function bool ShowLoginUI(optional bool bShowOnlineOnly = false);

/**
 * Logs the player into the online service. If this fails, it generates a
 * OnLoginFailed notification
 *
 * @param LocalUserNum the controller number of the associated user
 * @param LoginName the unique identifier for the player
 * @param Password the password for this account
 * @param bWantsLocalOnly whether the player wants to sign in locally only or not
 *
 * @return true if the async call started ok, false otherwise
 */
native function bool Login(byte LocalUserNum,string LoginName,string Password,optional bool bWantsLocalOnly);

/**
 * Logs the player into the online service using parameters passed on the
 * command line. Expects -Login=<UserName> -Password=<password>. If either
 * are missing, the function returns false and doesn't start the login
 * process
 *
 * @return true if the async call started ok, false otherwise
 */
native function bool AutoLogin();

/**
 * Delegate used in notifying the UI/game that the manual login failed
 *
 * @param LocalUserNum the controller number of the associated user
 * @param ErrorCode the async error code that occurred
 */
delegate OnLoginFailed(byte LocalUserNum,EOnlineServerConnectionStatus ErrorCode);

/**
 * Sets the delegate used to notify the gameplay code that a login failed
 *
 * @param LocalUserNum the controller number of the associated user
 * @param LoginDelegate the delegate to use for notifications
 */
function AddLoginFailedDelegate(byte LocalUserNum,delegate<OnLoginFailed> LoginFailedDelegate)
{
	// Add this delegate to the array if not already present
	if (LoginFailedDelegates.Find(LoginFailedDelegate) == INDEX_NONE)
	{
		LoginFailedDelegates[LoginFailedDelegates.Length] = LoginFailedDelegate;
	}
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param LoginDelegate the delegate to use for notifications
 */
function ClearLoginFailedDelegate(byte LocalUserNum,delegate<OnLoginFailed> LoginFailedDelegate)
{
	local int RemoveIndex;

	// Remove this delegate from the array if found
	RemoveIndex = LoginFailedDelegates.Find(LoginFailedDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		LoginFailedDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Signs the player out of the online service
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
native function bool Logout(byte LocalUserNum);

/**
 * Delegate used in notifying the UI/game that the manual logout completed
 *
 * @param bWasSuccessful whether the async call completed properly or not
 */
delegate OnLogoutCompleted(bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that a logout completed
 *
 * @param LocalUserNum the controller number of the associated user
 * @param LogoutDelegate the delegate to use for notifications
 */
function AddLogoutCompletedDelegate(byte LocalUserNum,delegate<OnLogoutCompleted> LogoutDelegate)
{
	// Add this delegate to the array if not already present
	if (LogoutCompletedDelegates.Find(LogoutDelegate) == INDEX_NONE)
	{
		LogoutCompletedDelegates[LogoutCompletedDelegates.Length] = LogoutDelegate;
	}
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param LogoutDelegate the delegate to use for notifications
 */
function ClearLogoutCompletedDelegate(byte LocalUserNum,delegate<OnLogoutCompleted> LogoutDelegate)
{
	local int RemoveIndex;

	// Remove this delegate from the array if found
	RemoveIndex = LogoutCompletedDelegates.Find(LogoutDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		LogoutCompletedDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Fetches the login status for a given player
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the enum value of their status
 */
native function ELoginStatus GetLoginStatus(byte LocalUserNum);

/**
 * Determines whether the specified user is a guest login or not
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return true if a guest, false otherwise
 */
function bool IsGuestLogin(byte LocalUserNum);

/**
 * Determines whether the specified user is a local (non-online) login or not
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return true if a local profile, false otherwise
 */
function bool IsLocalLogin(byte LocalUserNum);

/**
 * Gets the platform specific unique id for the specified player
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PlayerId the byte array that will receive the id
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
function bool GetUniquePlayerId(byte LocalUserNum,out UniqueNetId PlayerId)
{
	PlayerId = LoggedInPlayerId;
	return true;
}

/**
 * Reads the player's nick name from the online service
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return a string containing the players nick name
 */
function string GetPlayerNickname(byte LocalUserNum)
{
	return LoggedInPlayerName;
}

/**
 * Determines whether the player is allowed to play online
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
native function EFeaturePrivilegeLevel CanPlayOnline(byte LocalUserNum);

/**
 * Determines whether the player is allowed to use voice or text chat online
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
native function EFeaturePrivilegeLevel CanCommunicate(byte LocalUserNum);

/**
 * Determines whether the player is allowed to download user created content
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
function EFeaturePrivilegeLevel CanDownloadUserContent(byte LocalUserNum)
{
	return CanPlayOnline(LocalUserNum);
}

/**
 * Determines whether the player is allowed to buy content online
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
function EFeaturePrivilegeLevel CanPurchaseContent(byte LocalUserNum)
{
	return FPL_Enabled;
}

/**
 * Determines whether the player is allowed to view other people's player profile
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
function EFeaturePrivilegeLevel CanViewPlayerProfiles(byte LocalUserNum)
{
	return FPL_Enabled;
}

/**
 * Determines whether the player is allowed to have their online presence
 * information shown to remote clients
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
function EFeaturePrivilegeLevel CanShowPresenceInformation(byte LocalUserNum)
{
	return FPL_Enabled;
}

/**
 * Checks that a unique player id is part of the specified user's friends list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PlayerId the id of the player being checked
 *
 * @return TRUE if a member of their friends list, FALSE otherwise
 */
native function bool IsFriend(byte LocalUserNum,UniqueNetId PlayerId);

/**
 * Checks that whether a group of player ids are among the specified player's
 * friends
 *
 * @param LocalUserNum the controller number of the associated user
 * @param Query an array of players to check for being included on the friends list
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
native function bool AreAnyFriends(byte LocalUserNum,out array<FriendsQuery> Query);

/**
 * Checks that a unique player id is on the specified user's mute list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PlayerId the id of the player being checked
 *
 * @return TRUE if the player should be muted, FALSE otherwise
 */
function bool IsMuted(byte LocalUserNum,UniqueNetId PlayerId);

/**
 * Displays the UI that shows a user's list of friends
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowFriendsUI(byte LocalUserNum);

/**
 * Sets the delegate used to notify the gameplay code that a login changed
 *
 * @param LoginDelegate the delegate to use for notifications
 */
function AddLoginChangeDelegate(delegate<OnLoginChange> LoginDelegate)
{
	// Add this delegate to the array if not already present
	if (LoginChangeDelegates.Find(LoginDelegate) == INDEX_NONE)
	{
		LoginChangeDelegates[LoginChangeDelegates.Length] = LoginDelegate;
	}
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param LoginDelegate the delegate to use for notifications
 */
function ClearLoginChangeDelegate(delegate<OnLoginChange> LoginDelegate)
{
	local int RemoveIndex;

	// Remove this delegate from the array if found
	RemoveIndex = LoginChangeDelegates.Find(LoginDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		LoginChangeDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Delegate called when a player's status changes but doesn't change profiles
 *
 * @param NewStatus the new login status for the user
 * @param NewId the new id to associate with the user
 */
delegate OnLoginStatusChange(ELoginStatus NewStatus,UniqueNetId NewId);

/**
 * Sets the delegate used to notify the gameplay code that a login status has changed
 *
 * @param LoginDelegate the delegate to use for notifications
 * @param LocalUserNum the player to watch login status changes for
 */
function AddLoginStatusChangeDelegate(delegate<OnLoginStatusChange> LoginStatusDelegate,byte LocalUserNum);

/**
 * Removes the specified delegate from the notification list
 *
 * @param LoginDelegate the delegate to use for notifications
 * @param LocalUserNum the player to watch login status changes for
 */
function ClearLoginStatusChangeDelegate(delegate<OnLoginStatusChange> LoginStatusDelegate,byte LocalUserNum);

/**
 * Adds a delegate to the list of delegates that are fired when a login is cancelled
 *
 * @param CancelledDelegate the delegate to add to the list
 */
function AddLoginCancelledDelegate(delegate<OnLoginCancelled> CancelledDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param CancelledDelegate the delegate to remove fromt he list
 */
function ClearLoginCancelledDelegate(delegate<OnLoginCancelled> CancelledDelegate);

/**
 * Sets the delegate used to notify the gameplay code that a muting list changed
 *
 * @param MutingDelegate the delegate to use for notifications
 */
function AddMutingChangeDelegate(delegate<OnMutingChange> MutingDelegate)
{
	// Add this delegate to the array if not already present
	if (MutingChangeDelegates.Find(MutingDelegate) == INDEX_NONE)
	{
		MutingChangeDelegates[MutingChangeDelegates.Length] = MutingDelegate;
	}
}

/**
 * Searches the existing set of delegates for the one specified and removes it
 * from the list
 *
 * @param FriendsDelegate the delegate to use for notifications
 */
function ClearMutingChangeDelegate(delegate<OnFriendsChange> MutingDelegate)
{
	local int RemoveIndex;

	RemoveIndex = MutingChangeDelegates.Find(MutingDelegate);
	// Remove this delegate from the array if found
	if (RemoveIndex != INDEX_NONE)
	{
		MutingChangeDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Sets the delegate used to notify the gameplay code that a friends list changed
 *
 * @param LocalUserNum the user to read the friends list of
 * @param FriendsDelegate the delegate to use for notifications
 */
function AddFriendsChangeDelegate(byte LocalUserNum,delegate<OnFriendsChange> FriendsDelegate)
{
	if (LocalUserNum == 0)
	{
		// Add this delegate to the array if not already present
		if (FriendsChangeDelegates.Find(FriendsDelegate) == INDEX_NONE)
		{
			FriendsChangeDelegates[FriendsChangeDelegates.Length] = FriendsDelegate;
		}
	}
	else
	{
		`Warn("Invalid user index ("$LocalUserNum$") specified for ClearFriendsChangeDelegate()");
	}
}

/**
 * Searches the existing set of delegates for the one specified and removes it
 * from the list
 *
 * @param LocalUserNum the user to read the friends list of
 * @param FriendsDelegate the delegate to use for notifications
 */
function ClearFriendsChangeDelegate(byte LocalUserNum,delegate<OnFriendsChange> FriendsDelegate)
{
	local int RemoveIndex;

	if (LocalUserNum == 0)
	{
		RemoveIndex = FriendsChangeDelegates.Find(FriendsDelegate);
		// Remove this delegate from the array if found
		if (RemoveIndex != INDEX_NONE)
		{
			FriendsChangeDelegates.Remove(RemoveIndex,1);
		}
	}
	else
	{
		`Warn("Invalid user index ("$LocalUserNum$") specified for ClearFriendsChangeDelegate()");
	}
}

/**
 * Reads the online profile settings for a given user
 *
 * @param LocalUserNum the user that we are reading the data for
 * @param ProfileSettings the object to copy the results to and contains the list of items to read
 *
 * @return true if the call succeeds, false otherwise
 */
native function bool ReadProfileSettings(byte LocalUserNum,OnlineProfileSettings ProfileSettings);

/**
 * Delegate used when the last read profile settings request has completed
 *
 * @param LocalUserNum the controller index of the player who's read just completed
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnReadProfileSettingsComplete(byte LocalUserNum,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the last read request has completed
 *
 * @param LocalUserNum which user to watch for read complete notifications
 * @param ReadProfileSettingsCompleteDelegate the delegate to use for notifications
 */
function AddReadProfileSettingsCompleteDelegate(byte LocalUserNum,delegate<OnReadProfileSettingsComplete> ReadProfileSettingsCompleteDelegate)
{
	if (LocalUserNum >= 0 && LocalUserNum < 4)
	{
		// Add this delegate to the array if not already present
		if (PerUserReadProfileSettings[LocalUserNum].Delegates.Find(ReadProfileSettingsCompleteDelegate) == INDEX_NONE)
		{
			PerUserReadProfileSettings[LocalUserNum].Delegates[PerUserReadProfileSettings[LocalUserNum].Delegates.Length] = ReadProfileSettingsCompleteDelegate;
		}
	}
	else
	{
		`Warn("Invalid user index ("$LocalUserNum$") specified for AddReadProfileSettingsCompleteDelegate()");
	}
}

/**
 * Searches the existing set of delegates for the one specified and removes it
 * from the list
 *
 * @param LocalUserNum which user to watch for read complete notifications
 * @param ReadProfileSettingsCompleteDelegate the delegate to find and clear
 */
function ClearReadProfileSettingsCompleteDelegate(byte LocalUserNum,delegate<OnReadProfileSettingsComplete> ReadProfileSettingsCompleteDelegate)
{
	local int RemoveIndex;

	if (LocalUserNum >= 0 && LocalUserNum < 4)
	{
		RemoveIndex = PerUserReadProfileSettings[LocalUserNum].Delegates.Find(ReadProfileSettingsCompleteDelegate);
		// Remove this delegate from the array if found
		if (RemoveIndex != INDEX_NONE)
		{
			PerUserReadProfileSettings[LocalUserNum].Delegates.Remove(RemoveIndex,1);
		}
	}
	else
	{
		`Warn("Invalid user index ("$LocalUserNum$") specified for ClearReadProfileSettingsCompleteDelegate()");
	}
}

/**
 * Returns the online profile settings for a given user
 *
 * @param LocalUserNum the user that we are reading the data for
 *
 * @return the profile settings object
 */
function OnlineProfileSettings GetProfileSettings(byte LocalUserNum)
{
	if (LocalUserNum == 0)
	{
		return CachedProfile;
	}
	return None;
}

/**
 * Writes the online profile settings for a given user to the online data store
 *
 * @param LocalUserNum the user that we are writing the data for
 * @param ProfileSettings the list of settings to write out
 *
 * @return true if the call succeeds, false otherwise
 */
native function bool WriteProfileSettings(byte LocalUserNum,OnlineProfileSettings ProfileSettings);

/**
 * Delegate used when the last write profile settings request has completed
 *
 * @param LocalUserNum the controller index of the player who's write just completed
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnWriteProfileSettingsComplete(byte LocalUserNum,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the last read request has completed
 *
 * @param LocalUserNum which user to watch for read complete notifications
 * @param ReadProfileSettingsCompleteDelegate the delegate to use for notifications
 */
function AddWriteProfileSettingsCompleteDelegate(byte LocalUserNum,delegate<OnWriteProfileSettingsComplete> WriteProfileSettingsCompleteDelegate)
{
	if (LocalUserNum == 0)
	{
		// Add this delegate to the array if not already present
		if (WriteProfileSettingsDelegates.Find(WriteProfileSettingsCompleteDelegate) == INDEX_NONE)
		{
			WriteProfileSettingsDelegates[WriteProfileSettingsDelegates.Length] = WriteProfileSettingsCompleteDelegate;
		}
	}
	else
	{
		`Warn("Invalid user index ("$LocalUserNum$") specified for AddWriteProfileSettingsCompleteDelegate()");
	}
}

/**
 * Searches the existing set of delegates for the one specified and removes it
 * from the list
 *
 * @param LocalUserNum which user to watch for read complete notifications
 * @param ReadProfileSettingsCompleteDelegate the delegate to find and clear
 */
function ClearWriteProfileSettingsCompleteDelegate(byte LocalUserNum,delegate<OnWriteProfileSettingsComplete> WriteProfileSettingsCompleteDelegate)
{
	local int RemoveIndex;

	if (LocalUserNum == 0)
	{
		RemoveIndex = WriteProfileSettingsDelegates.Find(WriteProfileSettingsCompleteDelegate);
		// Remove this delegate from the array if found
		if (RemoveIndex != INDEX_NONE)
		{
			WriteProfileSettingsDelegates.Remove(RemoveIndex,1);
		}
	}
	else
	{
		`Warn("Invalid user index ("$LocalUserNum$") specified for ClearWriteProfileSettingsCompleteDelegate()");
	}
}

/**
 * Reads the online player storage data for a given local user
 * If a valid storage device ID is specified then data is also read from that device and the newer version is kept.
 *
 * @param LocalUserNum the user that we are reading the data for
 * @param PlayerStorage the object to copy the results to and contains the list of items to read
 * @param DeviceId optional ID for connected device to read from. -1 for no device
 *
 * @return true if the call succeeds, false otherwise
 */
function bool ReadPlayerStorage(byte LocalUserNum,OnlinePlayerStorage PlayerStorage,optional int DeviceId = -1);

/**
 * Delegate used when the last read of online player storage data request has completed
 *
 * @param LocalUserNum the controller index of the player who's read just completed
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnReadPlayerStorageComplete(byte LocalUserNum,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the last read request has completed 
 *
 * @param LocalUserNum which user to watch for read complete notifications
 * @param ReadPlayerStorageCompleteDelegate the delegate to use for notifications
 */
function AddReadPlayerStorageCompleteDelegate(byte LocalUserNum,delegate<OnReadPlayerStorageComplete> ReadPlayerStorageCompleteDelegate)
{
	// Add this delegate to the array if not already present
	if (LocalPlayerStorageReadDelegates.Find(ReadPlayerStorageCompleteDelegate) == INDEX_NONE)
	{
		LocalPlayerStorageReadDelegates.AddItem(ReadPlayerStorageCompleteDelegate);
	}
}

/**
 * Searches the existing set of delegates for the one specified and removes it
 * from the list
 *
 * @param LocalUserNum which user to watch for read complete notifications
 * @param ReadPlayerStorageCompleteDelegate the delegate to find and clear
 */
function ClearReadPlayerStorageCompleteDelegate(byte LocalUserNum,delegate<OnReadPlayerStorageComplete> ReadPlayerStorageCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = LocalPlayerStorageReadDelegates.Find(ReadPlayerStorageCompleteDelegate);
	// Remove this delegate from the array if found
	if (RemoveIndex != INDEX_NONE)
	{
		LocalPlayerStorageReadDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Reads the online player storage data for a given net user
 *
 * @param LocalUserNum the local user that is initiating the read
 * @param NetId the net user that we are reading the data for
 * @param PlayerStorage the object to copy the results to and contains the list of items to read
 *
 * @return true if the call succeeds, false otherwise
 */
function bool ReadPlayerStorageForNetId(byte LocalUserNum,UniqueNetId NetId,OnlinePlayerStorage PlayerStorage);

/**
 * Delegate used when the last read of online player storage data request has completed
 *
 * @param NetId the net id for the user who's read just completed
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnReadPlayerStorageForNetIdComplete(UniqueNetId NetId,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the last read request has completed
 *
 * @param NetId the net id for the user to watch for read complete notifications
 * @param ReadPlayerStorageForNetIdCompleteDelegate the delegate to use for notifications
 */
function AddReadPlayerStorageForNetIdCompleteDelegate(UniqueNetId NetId,delegate<OnReadPlayerStorageForNetIdComplete> ReadPlayerStorageForNetIdCompleteDelegate)
{
	// Add this delegate to the array if not already present
	if (RemotePlayerStorageReadDelegates.Find(ReadPlayerStorageForNetIdCompleteDelegate) == INDEX_NONE)
	{
		RemotePlayerStorageReadDelegates.AddItem(ReadPlayerStorageForNetIdCompleteDelegate);
	}
}

/**
 * Searches the existing set of delegates for the one specified and removes it
 * from the list
 *
 * @param NetId the net id for the user to watch for read complete notifications
 * @param ReadPlayerStorageForNetIdCompleteDelegate the delegate to find and clear
 */
function ClearReadPlayerStorageForNetIdCompleteDelegate(UniqueNetId NetId,delegate<OnReadPlayerStorageForNetIdComplete> ReadPlayerStorageForNetIdCompleteDelegate)
{
	local int RemoveIndex;
	
	RemoveIndex = RemotePlayerStorageReadDelegates.Find(ReadPlayerStorageForNetIdCompleteDelegate);
	// Remove this delegate from the array if found
	if (RemoveIndex != INDEX_NONE)
	{
		RemotePlayerStorageReadDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Returns the online player storage for a given local user
 *
 * @param LocalUserNum the user that we are reading the data for
 *
 * @return the player storage object
 */
function OnlinePlayerStorage GetPlayerStorage(byte LocalUserNum)
{
	if (LocalUserNum >= 0 && LocalUserNum < 4)
	{
		return PlayerStorageCache[LocalUserNum];
	}
	return None;
}

/**
 * Writes the online player storage data for a given local user to the online data store
 * If a valid storage device ID is specified then data is also written to that device.
 *
 * @param LocalUserNum the user that we are writing the data for
 * @param PlayerStorage the object that contains the list of items to write
 * @param DeviceId optional ID for connected device to write to. -1 for no device
 *
 * @return true if the call succeeds, false otherwise
 */
function bool WritePlayerStorage(byte LocalUserNum,OnlinePlayerStorage PlayerStorage,optional int DeviceId = -1);

/**
 * Delegate used when the last write online player storage request has completed
 *
 * @param LocalUserNum the controller index of the player who's write just completed
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnWritePlayerStorageComplete(byte LocalUserNum,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the last write request has completed
 *
 * @param LocalUserNum which user to watch for write complete notifications
 * @param WritePlayerStorageCompleteDelegate the delegate to use for notifications
 */
function AddWritePlayerStorageCompleteDelegate(byte LocalUserNum,delegate<OnWritePlayerStorageComplete> WritePlayerStorageCompleteDelegate)
{
	// Add this delegate to the array if not already present
	if (LocalPlayerStorageWriteDelegates.Find(WritePlayerStorageCompleteDelegate) == INDEX_NONE)
	{
		LocalPlayerStorageWriteDelegates.AddItem(WritePlayerStorageCompleteDelegate);
	}
}

/**
 * Clears the delegate used to notify the gameplay code that the last write request has completed
 *
 * @param LocalUserNum which user to watch for write complete notifications
 * @param WritePlayerStorageCompleteDelegate the delegate to use for notifications
 */
function ClearWritePlayerStorageCompleteDelegate(byte LocalUserNum,delegate<OnWritePlayerStorageComplete> WritePlayerStorageCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = LocalPlayerStorageWriteDelegates.Find(WritePlayerStorageCompleteDelegate);
	// Remove this delegate from the array if found
	if (RemoveIndex != INDEX_NONE)
	{
		LocalPlayerStorageWriteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Starts an async task that retrieves the list of friends for the player from the
 * online service. The list can be retrieved in whole or in part.
 *
 * @param LocalUserNum the user to read the friends list of
 * @param Count the number of friends to read or zero for all
 * @param StartingAt the index of the friends list to start at (for pulling partial lists)
 *
 * @return true if the read request was issued successfully, false otherwise
 */
native function bool ReadFriendsList(byte LocalUserNum,optional int Count,optional int StartingAt);

/**
 * Delegate used when the friends read request has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnReadFriendsComplete(bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the friends read request has completed
 *
 * @param LocalUserNum the user to read the friends list of
 * @param ReadFriendsCompleteDelegate the delegate to use for notifications
 */
function AddReadFriendsCompleteDelegate(byte LocalUserNum,delegate<OnReadFriendsComplete> ReadFriendsCompleteDelegate)
{
	if (LocalUserNum == 0)
	{
		// Add this delegate to the array if not already present
		if (ReadFriendsDelegates.Find(ReadFriendsCompleteDelegate) == INDEX_NONE)
		{
			ReadFriendsDelegates[ReadFriendsDelegates.Length] = ReadFriendsCompleteDelegate;
		}
	}
	else
	{
		`Warn("Invalid user index ("$LocalUserNum$") specified for AddReadFriendsCompleteDelegate()");
	}
}

/**
 * Searches the existing set of delegates for the one specified and removes it
 * from the list
 *
 * @param LocalUserNum which user to watch for read complete notifications
 * @param ReadFriendsCompleteDelegate the delegate to find and clear
 */
function ClearReadFriendsCompleteDelegate(byte LocalUserNum,delegate<OnReadFriendsComplete> ReadFriendsCompleteDelegate)
{
	local int RemoveIndex;

	if (LocalUserNum == 0)
	{
		RemoveIndex = ReadFriendsDelegates.Find(ReadFriendsCompleteDelegate);
		// Remove this delegate from the array if found
		if (RemoveIndex != INDEX_NONE)
		{
			ReadFriendsDelegates.Remove(RemoveIndex,1);
		}
	}
	else
	{
		`Warn("Invalid user index ("$LocalUserNum$") specified for ClearReadFriendsCompleteDelegate()");
	}
}

/**
 * Copies the list of friends for the player previously retrieved from the online
 * service. The list can be retrieved in whole or in part.
 *
 * @param LocalUserNum the user to read the friends list of
 * @param Friends the out array that receives the copied data
 * @param Count the number of friends to read or zero for all
 * @param StartingAt the index of the friends list to start at (for pulling partial lists)
 *
 * @return OERS_Done if the read has completed, otherwise one of the other states
 */
native function EOnlineEnumerationReadState GetFriendsList(byte LocalUserNum,out array<OnlineFriend> Friends,optional int Count,optional int StartingAt);

/**
 * Registers the user as a talker
 *
 * @param LocalUserNum the local player index that is a talker
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
native function bool RegisterLocalTalker(byte LocalUserNum);

/**
 * Unregisters the user as a talker
 *
 * @param LocalUserNum the local player index to be removed
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
native function bool UnregisterLocalTalker(byte LocalUserNum);

/**
 * Registers a remote player as a talker
 *
 * @param PlayerId the unique id of the remote player that is a talker
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
native function bool RegisterRemoteTalker(UniqueNetId PlayerId);

/**
 * Unregisters a remote player as a talker
 *
 * @param PlayerId the unique id of the remote player to be removed
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
native function bool UnregisterRemoteTalker(UniqueNetId PlayerId);

/**
 * Determines if the specified player is actively talking into the mic
 *
 * @param LocalUserNum the local player index being queried
 *
 * @return TRUE if the player is talking, FALSE otherwise
 */
native function bool IsLocalPlayerTalking(byte LocalUserNum);

/**
 * Determines if the specified remote player is actively talking into the mic
 * NOTE: Network latencies will make this not 100% accurate
 *
 * @param PlayerId the unique id of the remote player being queried
 *
 * @return TRUE if the player is talking, FALSE otherwise
 */
native function bool IsRemotePlayerTalking(UniqueNetId PlayerId);

/**
 * Determines if the specified player has a headset connected
 *
 * @param LocalUserNum the local player index being queried
 *
 * @return TRUE if the player has a headset plugged in, FALSE otherwise
 */
native function bool IsHeadsetPresent(byte LocalUserNum);

/**
 * Sets the relative priority for a remote talker. 0 is highest
 *
 * @param LocalUserNum the user that controls the relative priority
 * @param PlayerId the remote talker that is having their priority changed for
 * @param Priority the relative priority to use (0 highest, < 0 is muted)
 *
 * @return TRUE if the function succeeds, FALSE otherwise
 */
native function bool SetRemoteTalkerPriority(byte LocalUserNum,UniqueNetId PlayerId,int Priority);

/**
 * Mutes a remote talker for the specified local player. NOTE: This only mutes them in the
 * game unless the bIsSystemWide flag is true, which attempts to mute them globally
 *
 * @param LocalUserNum the user that is muting the remote talker
 * @param PlayerId the remote talker that is being muted
 * @param bIsSystemWide whether to try to mute them globally or not
 *
 * @return TRUE if the function succeeds, FALSE otherwise
 */
native function bool MuteRemoteTalker(byte LocalUserNum,UniqueNetId PlayerId,optional bool bIsSystemWide);

/**
 * Allows a remote talker to talk to the specified local player. NOTE: This only unmutes them in the
 * game unless the bIsSystemWide flag is true, which attempts to unmute them globally
 *
 * @param LocalUserNum the user that is allowing the remote talker to talk
 * @param PlayerId the remote talker that is being restored to talking
 * @param bIsSystemWide whether to try to unmute them globally or not
 *
 * @return TRUE if the function succeeds, FALSE otherwise
 */
native function bool UnmuteRemoteTalker(byte LocalUserNum,UniqueNetId PlayerId,optional bool bIsSystemWide);

/**
 * Called when a player is talking either locally or remote. This will be called
 * once for each active talker each frame.
 *
 * @param Player the player that is talking
 * @param bIsTalking if true, the player is now talking, if false they are no longer talking
 */
delegate OnPlayerTalkingStateChange(UniqueNetId Player,bool bIsTalking);

/**
 * Adds a talker delegate to the list of notifications
 *
 * @param TalkerDelegate the delegate to call when a player is talking
 */
function AddPlayerTalkingDelegate(delegate<OnPlayerTalkingStateChange> TalkerDelegate)
{
	if (PlayerTalkingDelegates.Find(TalkerDelegate) == INDEX_NONE)
	{
		PlayerTalkingDelegates[PlayerTalkingDelegates.Length] = TalkerDelegate;
	}
}

/**
 * Removes a talker delegate to the list of notifications
 *
 * @param TalkerDelegate the delegate to remove from the notification list
 */
function ClearPlayerTalkingDelegate(delegate<OnPlayerTalkingStateChange> TalkerDelegate)
{
	local int RemoveIndex;

	RemoveIndex = PlayerTalkingDelegates.Find(TalkerDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		PlayerTalkingDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Tells the voice layer that networked processing of the voice data is allowed
 * for the specified player. This allows for push-to-talk style voice communication
 *
 * @param LocalUserNum the local user to allow network transimission for
 */
native function StartNetworkedVoice(byte LocalUserNum);

/**
 * Tells the voice layer to stop processing networked voice support for the
 * specified player. This allows for push-to-talk style voice communication
 *
 * @param LocalUserNum the local user to disallow network transimission for
 */
native function StopNetworkedVoice(byte LocalUserNum);

/**
 * Tells the voice system to start tracking voice data for speech recognition
 *
 * @param LocalUserNum the local user to recognize voice data for
 *
 * @return true upon success, false otherwise
 */
native function bool StartSpeechRecognition(byte LocalUserNum);

/**
 * Tells the voice system to stop tracking voice data for speech recognition
 *
 * @param LocalUserNum the local user to recognize voice data for
 *
 * @return true upon success, false otherwise
 */
native function bool StopSpeechRecognition(byte LocalUserNum);

/**
 * Gets the results of the voice recognition
 *
 * @param LocalUserNum the local user to read the results of
 * @param Words the set of words that were recognized by the voice analyzer
 *
 * @return true upon success, false otherwise
 */
native function bool GetRecognitionResults(byte LocalUserNum,out array<SpeechRecognizedWord> Words);

/**
 * Called when speech recognition for a given player has completed. The
 * consumer of the notification can call GetRecognitionResults() to get the
 * words that were recognized
 */
delegate OnRecognitionComplete();

/**
 * Sets the speech recognition notification callback to use for the specified user
 *
 * @param LocalUserNum the local user to receive notifications for
 * @param RecognitionDelegate the delegate to call when recognition is complete
 */
function AddRecognitionCompleteDelegate(byte LocalUserNum,delegate<OnRecognitionComplete> RecognitionDelegate)
{
	if (SpeechRecognitionCompleteDelegates.Find(RecognitionDelegate) == INDEX_NONE)
	{
		SpeechRecognitionCompleteDelegates[SpeechRecognitionCompleteDelegates.Length] = RecognitionDelegate;
	}
}

/**
 * Clears the speech recognition notification callback to use for the specified user
 *
 * @param LocalUserNum the local user to receive notifications for
 * @param RecognitionDelegate the delegate to call when recognition is complete
 */
function ClearRecognitionCompleteDelegate(byte LocalUserNum,delegate<OnRecognitionComplete> RecognitionDelegate)
{
	local int RemoveIndex;

	RemoveIndex = SpeechRecognitionCompleteDelegates.Find(RecognitionDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		SpeechRecognitionCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Changes the vocabulary id that is currently being used
 *
 * @param LocalUserNum the local user that is making the change
 * @param VocabularyId the new id to use
 *
 * @return true if successful, false otherwise
 */
native function bool SelectVocabulary(byte LocalUserNum,int VocabularyId);

/**
 * Changes the object that is in use to the one specified
 *
 * @param LocalUserNum the local user that is making the change
 * @param SpeechRecogObj the new object use
 *
 * @param true if successful, false otherwise
 */
native function bool SetSpeechRecognitionObject(byte LocalUserNum,SpeechRecognition SpeechRecogObj);

/**
 * Reads a set of stats for the specified list of players
 *
 * @param Players the array of unique ids to read stats for
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
native function bool ReadOnlineStats(const out array<UniqueNetId> Players,OnlineStatsRead StatsRead);

/**
 * Reads a player's stats and all of that player's friends stats for the
 * specified set of stat views. This allows you to easily compare a player's
 * stats to their friends.
 *
 * @param LocalUserNum the local player having their stats and friend's stats read for
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
native function bool ReadOnlineStatsForFriends(byte LocalUserNum,OnlineStatsRead StatsRead);

/**
 * Reads stats by ranking. This grabs the rows starting at StartIndex through
 * NumToRead and places them in the StatsRead object.
 *
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 * @param StartIndex the starting rank to begin reads at (1 for top)
 * @param NumToRead the number of rows to read (clamped at 100 underneath)
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
native function bool ReadOnlineStatsByRank(OnlineStatsRead StatsRead,optional int StartIndex = 1,optional int NumToRead = 100);

/**
 * Reads stats by ranking centered around a player. This grabs a set of rows
 * above and below the player's current rank
 *
 * @param LocalUserNum the local player having their stats being centered upon
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 * @param NumRows the number of rows to read above and below the player's rank
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
native function bool ReadOnlineStatsByRankAroundPlayer(byte LocalUserNum,OnlineStatsRead StatsRead,optional int NumRows = 10);

/**
 * Notifies the interested party that the last stats read has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnReadOnlineStatsComplete(bool bWasSuccessful);

/**
 * Adds the delegate to a list used to notify the gameplay code that the stats read has completed
 *
 * @param ReadOnlineStatsCompleteDelegate the delegate to use for notifications
 */
function AddReadOnlineStatsCompleteDelegate(delegate<OnReadOnlineStatsComplete> ReadOnlineStatsCompleteDelegate)
{
	if (ReadOnlineStatsCompleteDelegates.Find(ReadOnlineStatsCompleteDelegate) == INDEX_NONE)
	{
		ReadOnlineStatsCompleteDelegates[ReadOnlineStatsCompleteDelegates.Length] = ReadOnlineStatsCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param ReadOnlineStatsCompleteDelegate the delegate to use for notifications
 */
function ClearReadOnlineStatsCompleteDelegate(delegate<OnReadOnlineStatsComplete> ReadOnlineStatsCompleteDelegate)
{
	local int RemoveIndex;
	// Find it in the list
	RemoveIndex = ReadOnlineStatsCompleteDelegates.Find(ReadOnlineStatsCompleteDelegate);
	// Only remove if found
	if (RemoveIndex != INDEX_NONE)
	{
		ReadOnlineStatsCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Cleans up any platform specific allocated data contained in the stats data
 *
 * @param StatsRead the object to handle per platform clean up on
 */
native function FreeStats(OnlineStatsRead StatsRead);

/**
 * Writes out the stats contained within the stats write object to the online
 * subsystem's cache of stats data. Note the new data replaces the old. It does
 * not write the data to the permanent storage until a FlushOnlineStats() call
 * or a session ends. Stats cannot be written without a session or the write
 * request is ignored. No more than 5 stats views can be written to at a time
 * or the write request is ignored.
 *
 * @param SessionName the name of the session to write stats for
 * @param Player the player to write stats for
 * @param StatsWrite the object containing the information to write
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
native function bool WriteOnlineStats(name SessionName,UniqueNetId Player,OnlineStatsWrite StatsWrite);

/**
 * Commits any changes in the online stats cache to the permanent storage
 *
 * @param SessionName the name of the session having stats flushed
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
native function bool FlushOnlineStats(name SessionName);

/**
 * Delegate called when the stats flush operation has completed
 *
 * @param SessionName the name of the session having stats flushed
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnFlushOnlineStatsComplete(name SessionName,bool bWasSuccessful);

/**
 * Adds the delegate used to notify the gameplay code that the stats flush has completed
 *
 * @param FlushOnlineStatsCompleteDelegate the delegate to use for notifications
 */
function AddFlushOnlineStatsCompleteDelegate(delegate<OnFlushOnlineStatsComplete> FlushOnlineStatsCompleteDelegate)
{
	if (FlushOnlineStatsDelegates.Find(FlushOnlineStatsCompleteDelegate) == INDEX_NONE)
	{
		FlushOnlineStatsDelegates[FlushOnlineStatsDelegates.Length] = FlushOnlineStatsCompleteDelegate;
	}
}

/**
 * Clears the delegate used to notify the gameplay code that the stats flush has completed
 *
 * @param FlushOnlineStatsCompleteDelegate the delegate to use for notifications
 */
function ClearFlushOnlineStatsCompleteDelegate(delegate<OnFlushOnlineStatsComplete> FlushOnlineStatsCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = FlushOnlineStatsDelegates.Find(FlushOnlineStatsCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		FlushOnlineStatsDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Writes the score data for the match
 *
 * @param SessionName the name of the session to write scores for
 * @param LeaderboardId the leaderboard to write the score information to
 * @param PlayerScores the list of players, teams, and scores they earned
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
native function bool WriteOnlinePlayerScores(name SessionName,int LeaderboardId,const out array<OnlinePlayerScore> PlayerScores);

/**
 * Returns the name of the player for the specified index
 *
 * @param UserIndex the user to return the name of
 *
 * @return the name of the player at the specified index
 */
event string GetPlayerNicknameFromIndex(int UserIndex)
{
	if (UserIndex == 0)
	{
		return LoggedInPlayerName;
	}
	return "";
}

/**
 * Determines if the ethernet link is connected or not
 */
native function bool HasLinkConnection();

/**
 * Delegate fired when the network link status changes
 *
 * @param bIsConnected whether the link is currently connected or not
 */
delegate OnLinkStatusChange(bool bIsConnected);

/**
 * Adds the delegate used to notify the gameplay code that link status changed
 *
 * @param LinkStatusDelegate the delegate to use for notifications
 */
function AddLinkStatusChangeDelegate(delegate<OnLinkStatusChange> LinkStatusDelegate)
{
	// Only add to the list once
	if (LinkStatusDelegates.Find(LinkStatusDelegate) == INDEX_NONE)
	{
		LinkStatusDelegates[LinkStatusDelegates.Length] = LinkStatusDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param LinkStatusDelegate the delegate to remove
 */
function ClearLinkStatusChangeDelegate(delegate<OnLinkStatusChange> LinkStatusDelegate)
{
	local int RemoveIndex;
	// See if the specified delegate is in the list
	RemoveIndex = LinkStatusDelegates.Find(LinkStatusDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		LinkStatusDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Delegate fired when an external UI display state changes (opening/closing)
 *
 * @param bIsOpening whether the external UI is opening or closing
 */
delegate OnExternalUIChange(bool bIsOpening);

/**
 * Sets the delegate used to notify the gameplay code that external UI state
 * changed (opened/closed)
 *
 * @param ExternalUIDelegate the delegate to use for notifications
 */
function AddExternalUIChangeDelegate(delegate<OnExternalUIChange> ExternalUIDelegate);

/**
 * Removes the delegate from the notification list
 *
 * @param ExternalUIDelegate the delegate to remove
 */
function ClearExternalUIChangeDelegate(delegate<OnExternalUIChange> ExternalUIDelegate);

/**
 * Determines the current notification position setting
 */
function ENetworkNotificationPosition GetNetworkNotificationPosition()
{
//@todo joeg -- hook up properly
	return NNP_BottomCenter;
}

/**
 * Sets a new position for the network notification icons/images
 *
 * @param NewPos the new location to use
 */
function SetNetworkNotificationPosition(ENetworkNotificationPosition NewPos);

/**
 * Delegate fired when the controller becomes dis/connected
 *
 * @param ControllerId the id of the controller that changed connection state
 * @param bIsConnected whether the controller connected (true) or disconnected (false)
 */
delegate OnControllerChange(int ControllerId,bool bIsConnected);

/**
 * Sets the delegate used to notify the gameplay code that the controller state changed
 *
 * @param ControllerChangeDelegate the delegate to use for notifications
 */
function AddControllerChangeDelegate(delegate<OnControllerChange> ControllerChangeDelegate)
{
	// Only add to the list once
	if (ControllerChangeDelegates.Find(ControllerChangeDelegate) == INDEX_NONE)
	{
		ControllerChangeDelegates[ControllerChangeDelegates.Length] = ControllerChangeDelegate;
	}
}

/**
 * Removes the delegate used to notify the gameplay code that the controller state changed
 *
 * @param ControllerChangeDelegate the delegate to remove
 */
function ClearControllerChangeDelegate(delegate<OnControllerChange> ControllerChangeDelegate)
{
	local int RemoveIndex;
	// See if the specified delegate is in the list
	RemoveIndex = ControllerChangeDelegates.Find(ControllerChangeDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		ControllerChangeDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Determines if the specified controller is connected or not
 *
 * @param ControllerId the controller to query
 *
 * @return true if connected, false otherwise
 */
native function bool IsControllerConnected(int ControllerId);

/**
 * Delegate fire when the online server connection state changes
 *
 * @param ConnectionStatus the new connection status
 */
delegate OnConnectionStatusChange(EOnlineServerConnectionStatus ConnectionStatus);

/**
 * Adds the delegate to the list to be notified when the connection status changes
 *
 * @param ConnectionStatusDelegate the delegate to add
 */
function AddConnectionStatusChangeDelegate(delegate<OnConnectionStatusChange> ConnectionStatusDelegate)
{
	// Only add to the list once
	if (ConnectionStatusChangeDelegates.Find(ConnectionStatusDelegate) == INDEX_NONE)
	{
		ConnectionStatusChangeDelegates[ConnectionStatusChangeDelegates.Length] = ConnectionStatusDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param ConnectionStatusDelegate the delegate to remove
 */
function ClearConnectionStatusChangeDelegate(delegate<OnConnectionStatusChange> ConnectionStatusDelegate)
{
	local int RemoveIndex;
	// See if the specified delegate is in the list
	RemoveIndex = ConnectionStatusChangeDelegates.Find(ConnectionStatusDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		ConnectionStatusChangeDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Determines the NAT type the player is using
 */
native function ENATType GetNATType();

/**
 * Determine the locale (country code) for the player
 */
function int GetLocale()
{
	return 0;
}

/**
 * Delegate fired when a storage device change is detected
 */
delegate OnStorageDeviceChange();

/**
 * Adds the delegate to the list to be notified when a storage device changes
 *
 * @param StorageDeviceChangeDelegate the delegate to add
 */
function AddStorageDeviceChangeDelegate(delegate<OnStorageDeviceChange> StorageDeviceChangeDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param StorageDeviceChangeDelegate the delegate to remove
 */
function ClearStorageDeviceChangeDelegate(delegate<OnStorageDeviceChange> StorageDeviceChangeDelegate);


/**
 * Delegate fired when a file read from the network platform's title specific storage is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param FileName the name of the file this was for
 */
delegate OnReadTitleFileComplete(bool bWasSuccessful,string FileName);

/**
 * Starts an asynchronous read of the specified file from the network platform's
 * title specific file store
 *
 * @param FileToRead the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
native function bool ReadTitleFile(string FileToRead);

/**
 * Adds the delegate to the list to be notified when a requested file has been read
 *
 * @param ReadTitleFileCompleteDelegate the delegate to add
 */
function AddReadTitleFileCompleteDelegate(delegate<OnReadTitleFileComplete> ReadTitleFileCompleteDelegate)
{
	if (ReadTitleFileCompleteDelegates.Find(ReadTitleFileCompleteDelegate) == INDEX_NONE)
	{
		ReadTitleFileCompleteDelegates[ReadTitleFileCompleteDelegates.Length] = ReadTitleFileCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param ReadTitleFileCompleteDelegate the delegate to remove
 */
function ClearReadTitleFileCompleteDelegate(delegate<OnReadTitleFileComplete> ReadTitleFileCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = ReadTitleFileCompleteDelegates.Find(ReadTitleFileCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		ReadTitleFileCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Copies the file data into the specified buffer for the specified file
 *
 * @param FileName the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
native function bool GetTitleFileContents(string FileName,out array<byte> FileContents);

/**
 * Determines the async state of the tile file read operation
 *
 * @param FileName the name of the file to check on
 *
 * @return the async state of the file read
 */
function EOnlineEnumerationReadState GetTitleFileState(string FileName);

/**
 * Creates a network enabled account on the online service
 *
 * @param UserName the unique nickname of the account
 * @param Password the password securing the account
 * @param EmailAddress the address used to send password hints to
 * @param ProductKey
 */
native function bool CreateOnlineAccount(string UserName,string Password,string EmailAddress,optional string ProductKey);

/**
 * Delegate used in notifying the UI/game that the account creation completed
 *
 * @param ErrorStatus whether the account created successfully or not
 */
delegate OnCreateOnlineAccountCompleted(EOnlineAccountCreateStatus ErrorStatus);

/**
 * Sets the delegate used to notify the gameplay code that account creation completed
 *
 * @param AccountCreateDelegate the delegate to use for notifications
 */
function AddCreateOnlineAccountCompletedDelegate(delegate<OnCreateOnlineAccountCompleted> AccountCreateDelegate)
{
	if (AccountCreateDelegates.Find(AccountCreateDelegate) == INDEX_NONE)
	{
		AccountCreateDelegates[AccountCreateDelegates.Length] = AccountCreateDelegate;
	}
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param AccountCreateDelegate the delegate to use for notifications
 */
function ClearCreateOnlineAccountCompletedDelegate(delegate<OnCreateOnlineAccountCompleted> AccountCreateDelegate)
{
	local int RemoveIndex;

	RemoveIndex = AccountCreateDelegates.Find(AccountCreateDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		AccountCreateDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Creates a non-networked account on the local system. Password is only used
 * when supplied. Otherwise the account is not secured.
 *
 * @param UserName the unique nickname of the account
 * @param Password the password securing the account
 *
 * @return true if the account was created, false otherwise
 */
function bool CreateLocalAccount(string UserName,optional string Password);

/**
 * Changes the name of a local account
 *
 * @param UserName the unique nickname of the account
 * @param Password the password securing the account
 *
 * @return true if the account was renamed, false otherwise
 */
function bool RenameLocalAccount(string NewUserName,string OldUserName,optional string Password);

/**
 * Deletes a local account if the password matches
 *
 * @param UserName the unique nickname of the account
 * @param Password the password securing the account
 *
 * @return true if the account was deleted, false otherwise
 */
function bool DeleteLocalAccount(string UserName,optional string Password);

/**
 * Fetches a list of local accounts
 *
 * @param Accounts the array that is populated with the accounts
 *
 * @return true if the list was read, false otherwise
 */
function bool GetLocalAccountNames(out array<string> Accounts);

/**
 * Sets the online status information to use for the specified player. Used to
 * tell other players what the player is doing (playing, menus, away, etc.)
 *
 * @param LocalUserNum the controller number of the associated user
 * @param StatusId the status id to use (maps to strings where possible)
 * @param LocalizedStringSettings the list of localized string settings to set
 * @param Properties the list of properties to set
 */
native function SetOnlineStatus(byte LocalUserNum,int StatusId,const out array<LocalizedStringSetting> LocalizedStringSettings,const out array<SettingsProperty> Properties);

/**
 * Displays the UI that shows the keyboard for inputing text
 *
 * @param LocalUserNum the controller number of the associated user
 * @param TitleText the title to display to the user
 * @param DescriptionText the text telling the user what to input
 * @param bIsPassword whether the item being entered is a password or not
 * @param bShouldValidate whether to apply the string validation API after input or not
 * @param DefaultText the default string to display
 * @param MaxResultLength the maximum length string expected to be filled in
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
native function bool ShowKeyboardUI(byte LocalUserNum,string TitleText,string DescriptionText,
	optional bool bIsPassword = false,
	optional bool bShouldValidate = true,
	optional string DefaultText,
	optional int MaxResultLength = 256);

/**
 * Delegate used when the keyboard input request has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnKeyboardInputComplete(bool bWasSuccessful);

/**
 * Adds the delegate used to notify the gameplay code that the user has completed
 * their keyboard input
 *
 * @param InputDelegate the delegate to use for notifications
 */
function AddKeyboardInputDoneDelegate(delegate<OnKeyboardInputComplete> InputDelegate)
{
	// Add this delegate to the array if not already present
	if (KeyboardInputDelegates.Find(InputDelegate) == INDEX_NONE)
	{
		KeyboardInputDelegates[KeyboardInputDelegates.Length] = InputDelegate;
	}
}

/**
 * Clears the delegate used to notify the gameplay code that the user has completed
 * their keyboard input
 *
 * @param InputDelegate the delegate to use for notifications
 */
function ClearKeyboardInputDoneDelegate(delegate<OnKeyboardInputComplete> InputDelegate)
{
	local int RemoveIndex;

	RemoveIndex = KeyboardInputDelegates.Find(InputDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		KeyboardInputDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Fetches the results of the input
 *
 * @param bWasCanceled whether the user cancelled the input or not
 *
 * @return the string entered by the user. Note the string will be empty if it
 * fails validation
 */
function string GetKeyboardInputResults(out byte bWasCanceled)
{
	bWasCanceled = bWasKeyboardInputCanceled;
	return KeyboardResultsString;
}

/**
 * Sends a friend invite to the specified player
 *
 * @param LocalUserNum the user that is sending the invite
 * @param NewFriend the player to send the friend request to
 * @param Message the message to display to the recipient
 *
 * @return true if successful, false otherwise
 */
native function bool AddFriend(byte LocalUserNum,UniqueNetId NewFriend,optional string Message);

/**
 * Sends a friend invite to the specified player nick
 *
 * @param LocalUserNum the user that is sending the invite
 * @param FriendName the name of the player to send the invite to
 * @param Message the message to display to the recipient
 *
 * @return true if successful, false otherwise
 */
native function bool AddFriendByName(byte LocalUserNum,string FriendName,optional string Message);

/**
 * Called when a friend invite arrives for a local player
 *
 * @param bWasSuccessful true if successfully added, false if not found or failed
 */
delegate OnAddFriendByNameComplete(bool bWasSuccessful);

/**
 * Adds the delegate used to notify the gameplay code that the user has received a friend invite
 *
 * @param LocalUserNum the user associated with the notification
 * @param FriendDelegate the delegate to use for notifications
 */
function AddAddFriendByNameCompleteDelegate(byte LocalUserNum,delegate<OnAddFriendByNameComplete> FriendDelegate)
{
	if (LocalUserNum == LoggedInPlayerNum)
	{
		if (AddFriendByNameCompleteDelegates.Find(FriendDelegate) == INDEX_NONE)
		{
			AddFriendByNameCompleteDelegates[AddFriendByNameCompleteDelegates.Length] = FriendDelegate;
		}
	}
}

/**
 * Removes the delegate specified from the list
 *
 * @param LocalUserNum the user associated with the notification
 * @param FriendDelegate the delegate to use for notifications
 */
function ClearAddFriendByNameCompleteDelegate(byte LocalUserNum,delegate<OnAddFriendByNameComplete> FriendDelegate)
{
	local int RemoveIndex;

	if (LocalUserNum == LoggedInPlayerNum)
	{
		RemoveIndex = AddFriendByNameCompleteDelegates.Find(FriendDelegate);
		if (RemoveIndex != INDEX_NONE)
		{
			AddFriendByNameCompleteDelegates.Remove(RemoveIndex,1);
		}
	}
}

/**
 * Removes a friend from the player's friend list
 *
 * @param LocalUserNum the user that is removing the friend
 * @param FormerFriend the player to remove from the friend list
 *
 * @return true if successful, false otherwise
 */
native function bool RemoveFriend(byte LocalUserNum,UniqueNetId FormerFriend);

/**
 * Used to accept a friend invite sent to this player
 *
 * @param LocalUserNum the user the invite is for
 * @param RequestingPlayer the player the invite is from
 *
 * @param true if successful, false otherwise
 */
native function bool AcceptFriendInvite(byte LocalUserNum,UniqueNetId RequestingPlayer);

/**
 * Used to deny a friend request sent to this player
 *
 * @param LocalUserNum the user the invite is for
 * @param RequestingPlayer the player the invite is from
 *
 * @param true if successful, false otherwise
 */
native function bool DenyFriendInvite(byte LocalUserNum,UniqueNetId RequestingPlayer);

/**
 * Called when a friend invite arrives for a local player
 *
 * @param LocalUserNum the user that is receiving the invite
 * @param RequestingPlayer the player sending the friend request
 * @param RequestingNick the nick of the player sending the friend request
 * @param Message the message to display to the recipient
 *
 * @return true if successful, false otherwise
 */
delegate OnFriendInviteReceived(byte LocalUserNum,UniqueNetId RequestingPlayer,string RequestingNick,string Message);

/**
 * Adds the delegate used to notify the gameplay code that the user has received a friend invite
 *
 * @param LocalUserNum the user associated with the notification
 * @param InviteDelegate the delegate to use for notifications
 */
function AddFriendInviteReceivedDelegate(byte LocalUserNum,delegate<OnFriendInviteReceived> InviteDelegate)
{
	if (LocalUserNum == LoggedInPlayerNum)
	{
		if (FriendInviteReceivedDelegates.Find(InviteDelegate) == INDEX_NONE)
		{
			FriendInviteReceivedDelegates[FriendInviteReceivedDelegates.Length] = InviteDelegate;
		}
	}
}

/**
 * Removes the delegate specified from the list
 *
 * @param LocalUserNum the user associated with the notification
 * @param InviteDelegate the delegate to use for notifications
 */
function ClearFriendInviteReceivedDelegate(byte LocalUserNum,delegate<OnFriendInviteReceived> InviteDelegate)
{
	local int RemoveIndex;

	if (LocalUserNum == LoggedInPlayerNum)
	{
		RemoveIndex = FriendInviteReceivedDelegates.Find(InviteDelegate);
		if (RemoveIndex != INDEX_NONE)
		{
			FriendInviteReceivedDelegates.Remove(RemoveIndex,1);
		}
	}
}

/**
 * Sends a message to a friend
 *
 * @param LocalUserNum the user that is sending the message
 * @param Friend the player to send the message to
 * @param Message the message to display to the recipient
 *
 * @return true if successful, false otherwise
 */
native function bool SendMessageToFriend(byte LocalUserNum,UniqueNetId Friend,string Message);

/**
 * Sends an invitation to play in the player's current session
 *
 * @param LocalUserNum the user that is sending the invite
 * @param Friend the player to send the invite to
 * @param Text the text of the message for the invite
 *
 * @return true if successful, false otherwise
 */
native function bool SendGameInviteToFriend(byte LocalUserNum,UniqueNetId Friend,optional string Text);

/**
 * Sends invitations to play in the player's current session
 *
 * @param LocalUserNum the user that is sending the invite
 * @param Friends the player to send the invite to
 * @param Text the text of the message for the invite
 *
 * @return true if successful, false otherwise
 */
native function bool SendGameInviteToFriends(byte LocalUserNum,array<UniqueNetId> Friends,optional string Text);

/**
 * Called when the online system receives a game invite that needs handling
 *
 * @param LocalUserNum the user that is receiving the invite
 * @param InviterName the nick name of the person sending the invite
 */
delegate OnReceivedGameInvite(byte LocalUserNum,string InviterName);

/**
 * Adds the delegate used to notify the gameplay code that the user has received a game invite
 *
 * @param LocalUserNum the user associated with the notification
 * @param ReceivedGameInviteDelegate the delegate to use for notifications
 */
function AddReceivedGameInviteDelegate(byte LocalUserNum,delegate<OnReceivedGameInvite> ReceivedGameInviteDelegate)
{
	if (LocalUserNum == LoggedInPlayerNum)
	{
		if (ReceivedGameInviteDelegates.Find(ReceivedGameInviteDelegate) == INDEX_NONE)
		{
			ReceivedGameInviteDelegates[ReceivedGameInviteDelegates.Length] = ReceivedGameInviteDelegate;
		}
	}
}

/**
 * Removes the delegate specified from the list
 *
 * @param LocalUserNum the user associated with the notification
 * @param ReceivedGameInviteDelegate the delegate to use for notifications
 */
function ClearReceivedGameInviteDelegate(byte LocalUserNum,delegate<OnReceivedGameInvite> ReceivedGameInviteDelegate)
{
	local int RemoveIndex;

	if (LocalUserNum == LoggedInPlayerNum)
	{
		RemoveIndex = ReceivedGameInviteDelegates.Find(ReceivedGameInviteDelegate);
		if (RemoveIndex != INDEX_NONE)
		{
			ReceivedGameInviteDelegates.Remove(RemoveIndex,1);
		}
	}
}

/**
 * Allows the local player to follow a friend into a game
 *
 * @param LocalUserNum the local player wanting to join
 * @param Friend the player that is being followed
 *
 * @return true if the async call worked, false otherwise
 */
native function bool JoinFriendGame(byte LocalUserNum,UniqueNetId Friend);

/**
 * Called once the join task has completed
 *
 * @param bWasSuccessful the session was found and is joinable, false otherwise
 */
delegate OnJoinFriendGameComplete(bool bWasSuccessful);

/**
 * Sets the delegate used to notify when the join friend is complete
 *
 * @param JoinFriendGameCompleteDelegate the delegate to use for notifications
 */
function AddJoinFriendGameCompleteDelegate(delegate<OnJoinFriendGameComplete> JoinFriendGameCompleteDelegate)
{
	if (JoinFriendGameCompleteDelegates.Find(JoinFriendGameCompleteDelegate) == INDEX_NONE)
	{
		JoinFriendGameCompleteDelegates[JoinFriendGameCompleteDelegates.Length] = JoinFriendGameCompleteDelegate;
	}
}

/**
 * Removes the delegate from the list of notifications
 *
 * @param JoinFriendGameCompleteDelegate the delegate to use for notifications
 */
function ClearJoinFriendGameCompleteDelegate(delegate<OnJoinFriendGameComplete> JoinFriendGameCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = JoinFriendGameCompleteDelegates.Find(JoinFriendGameCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		JoinFriendGameCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Returns the list of messages for the specified player
 *
 * @param LocalUserNum the local player wanting to join
 * @param FriendMessages the set of messages cached locally for the player
 */
function GetFriendMessages(byte LocalUserNum,out array<OnlineFriendMessage> FriendMessages)
{
	if (LocalUserNum == 0)
	{
		FriendMessages = CachedFriendMessages;
	}
}

/**
 * Called when a friend invite arrives for a local player
 *
 * @param LocalUserNum the user that is receiving the invite
 * @param SendingPlayer the player sending the friend request
 * @param SendingNick the nick of the player sending the friend request
 * @param Message the message to display to the recipient
 *
 * @return true if successful, false otherwise
 */
delegate OnFriendMessageReceived(byte LocalUserNum,UniqueNetId SendingPlayer,string SendingNick,string Message);

/**
 * Adds the delegate used to notify the gameplay code that the user has received a friend invite
 *
 * @param LocalUserNum the user associated with the notification
 * @param MessageDelegate the delegate to use for notifications
 */
function AddFriendMessageReceivedDelegate(byte LocalUserNum,delegate<OnFriendMessageReceived> MessageDelegate)
{
	if (LocalUserNum == LoggedInPlayerNum)
	{
		if (FriendMessageReceivedDelegates.Find(MessageDelegate) == INDEX_NONE)
		{
			FriendMessageReceivedDelegates[FriendMessageReceivedDelegates.Length] = MessageDelegate;
		}
	}
}

/**
 * Removes the delegate specified from the list
 *
 * @param LocalUserNum the user associated with the notification
 * @param MessageDelegate the delegate to use for notifications
 */
function ClearFriendMessageReceivedDelegate(byte LocalUserNum,delegate<OnFriendMessageReceived> MessageDelegate)
{
	local int RemoveIndex;

	if (LocalUserNum == LoggedInPlayerNum)
	{
		RemoveIndex = FriendMessageReceivedDelegates.Find(MessageDelegate);
		if (RemoveIndex != INDEX_NONE)
		{
			FriendMessageReceivedDelegates.Remove(RemoveIndex,1);
		}
	}
}

/**
 * Reads the host's stat guid for synching up stats. Only valid on the host.
 *
 * @return the host's stat guid
 */
native function string GetHostStatGuid();

/**
 * Registers the host's stat guid with the client for verification they are part of
 * the stat. Note this is an async task for any backend communication that needs to
 * happen before the registration is deemed complete
 *
 * @param HostStatGuid the host's stat guid
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
native function bool RegisterHostStatGuid(const out string HostStatGuid);

/**
 * Called when the host stat guid registration is complete
 *
 * @param bWasSuccessful whether the registration has completed or not
 */
delegate OnRegisterHostStatGuidComplete(bool bWasSuccessful);

/**
 * Adds the delegate for notifying when the host guid registration is done
 *
 * @param RegisterHostStatGuidCompleteDelegate the delegate to use for notifications
 */
function AddRegisterHostStatGuidCompleteDelegate(delegate<OnRegisterHostStatGuidComplete> RegisterHostStatGuidCompleteDelegate)
{
	if (RegisterHostStatGuidCompleteDelegates.Find(RegisterHostStatGuidCompleteDelegate) == INDEX_NONE)
	{
		RegisterHostStatGuidCompleteDelegates[RegisterHostStatGuidCompleteDelegates.Length] = RegisterHostStatGuidCompleteDelegate;
	}
}

/**
 * Clears the delegate used to notify the gameplay code
 *
 * @param RegisterHostStatGuidCompleteDelegate the delegate to use for notifications
 */
function ClearRegisterHostStatGuidCompleteDelegateDelegate(delegate<OnRegisterHostStatGuidComplete> RegisterHostStatGuidCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = RegisterHostStatGuidCompleteDelegates.Find(RegisterHostStatGuidCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		RegisterHostStatGuidCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Reads the client's stat guid that was generated by registering the host's guid
 * Used for synching up stats. Only valid on the client. Only callable after the
 * host registration has completed
 *
 * @return the client's stat guid
 */
native function string GetClientStatGuid();

/**
 * Registers the client's stat guid on the host to validate that the client was in the stat.
 * Used for synching up stats. Only valid on the host.
 *
 * @param PlayerId the client's unique net id
 * @param ClientStatGuid the client's stat guid
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
native function bool RegisterStatGuid(UniqueNetId PlayerId,const out string ClientStatGuid);

/**
 * Calculates the aggregate skill from an array of skills. 
 * 
 * @param Mus - array that holds the mu values 
 * @param Sigmas - array that holds the sigma values 
 * @param OutAggregateMu - aggregate Mu
 * @param OutAggregateSigma - aggregate Sigma
 */
function CalcAggregateSkill(array<double> Mus, array<double> Sigmas, out double OutAggregateMu, out double OutAggregateSigma);

/**
 * Mutes all voice or all but friends
 *
 * @param LocalUserNum the local user that is making the change
 * @param bAllowFriends whether to mute everyone or allow friends
 */
function bool MuteAll(byte LocalUserNum,bool bAllowFriends)
{
	if (LocalUserNum == LoggedInPlayerNum)
	{
		CurrentLocalTalker.MuteType = bAllowFriends ? MUTE_AllButFriends : MUTE_All;
		return true;
	}
	return false;
}

/**
 * Allows all speakers to send voice
 *
 * @param LocalUserNum the local user that is making the change
 */
function bool UnmuteAll(byte LocalUserNum)
{
	if (LocalUserNum == LoggedInPlayerNum)
	{
		CurrentLocalTalker.MuteType = MUTE_None;
		return true;
	}
	return false;
}

/**
 * Deletes a message from the list of messages
 *
 * @param LocalUserNum the user that is deleting the message
 * @param MessageIndex the index of the message to delete
 *
 * @return true if the message was deleted, false otherwise
 */
function bool DeleteMessage(byte LocalUserNum,int MessageIndex)
{
	if (LocalUserNum == LoggedInPlayerNum)
	{
		// If it's safe to access, remove it
		if (MessageIndex >= 0 && MessageIndex < CachedFriendMessages.Length)
		{
			CachedFriendMessages.Remove(MessageIndex,1);
			return true;
		}
	}
	return false;
}

/**
* Unlocks the specified achievement for the specified user
*
* @param LocalUserNum the controller number of the associated user
* @param AchievementId the id of the achievement to unlock
*
* @return TRUE if the call worked, FALSE otherwise
*/
native function bool UnlockAchievement(byte LocalUserNum,int AchievementId,float PercentComplete=100.0);

/**
* Delegate used when the achievement unlocking has completed
*
* @param bWasSuccessful true if the async action completed without error, false if there was an error
*/
delegate OnUnlockAchievementComplete(bool bWasSuccessful);

/**
* Adds the delegate used to notify the gameplay code that the achievement unlocking has completed
*
* @param LocalUserNum which user to watch for read complete notifications
* @param UnlockAchievementCompleteDelegate the delegate to use for notifications
*/
function AddUnlockAchievementCompleteDelegate(byte LocalUserNum,delegate<OnUnlockAchievementComplete> UnlockAchievementCompleteDelegate)
{
	if (LocalUserNum == LoggedInPlayerNum)
	{
		if (UnlockAchievementCompleteDelegates.Find(UnlockAchievementCompleteDelegate) == INDEX_NONE)
		{
			UnlockAchievementCompleteDelegates.AddItem(UnlockAchievementCompleteDelegate);
		}
	}
	else
	{
		`warn("Invalid index ("$LocalUserNum$") passed to AddUnlockAchievementCompleteDelegate()");
	}
}

/**
* Clears the delegate used to notify the gameplay code that the achievement unlocking has completed
*
* @param LocalUserNum which user to watch for read complete notifications
* @param UnlockAchievementCompleteDelegate the delegate to use for notifications
*/
function ClearUnlockAchievementCompleteDelegate(byte LocalUserNum,delegate<OnUnlockAchievementComplete> UnlockAchievementCompleteDelegate)
{
	local int RemoveIndex;

	if (LocalUserNum == LoggedInPlayerNum)
	{
		RemoveIndex = UnlockAchievementCompleteDelegates.Find(UnlockAchievementCompleteDelegate);
		if (RemoveIndex != INDEX_NONE)
		{
			UnlockAchievementCompleteDelegates.Remove(RemoveIndex,1);
		}
	}
	else
	{
		`warn("Invalid index ("$LocalUserNum$") passed to ClearUnlockAchievementCompleteDelegate()");
	}
}


/**
 * Starts an async read for the achievement list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param TitleId the title id of the game the achievements are to be read for
 * @param bShouldReadText whether to fetch the text strings or not
 * @param bShouldReadImages whether to fetch the image data or not
 *
 * @return TRUE if the task starts, FALSE if it failed
 */
native function bool ReadAchievements(byte LocalUserNum,optional int TitleId = 0,optional bool bShouldReadText = true,optional bool bShouldReadImages = false);

/**
 * Called when the async achievements read has completed
 *
 * @param TitleId the title id that the read was for
 */
delegate OnReadAchievementsComplete(int TitleId);

/**
 * Sets the delegate used to notify the gameplay code that the achievements read request has completed
 *
 * @param LocalUserNum the user to read the achievements list for
 * @param ReadAchievementsCompleteDelegate the delegate to use for notifications
 */
function AddReadAchievementsCompleteDelegate(byte LocalUserNum,delegate<OnReadAchievementsComplete> ReadAchievementsCompleteDelegate)
{
	if (LocalUserNum == LoggedInPlayerNum)
	{
		if (ReadAchievementsCompleteDelegates.Find(ReadAchievementsCompleteDelegate) == INDEX_NONE)
		{
			ReadAchievementsCompleteDelegates.AddItem(ReadAchievementsCompleteDelegate);
		}
	}
	else
	{
		`warn("Invalid index ("$LocalUserNum$") passed to AddReadAchievementsComplete()");
	}
}

/**
 * Clears the delegate used to notify the gameplay code that the achievements read request has completed
 *
 * @param LocalUserNum the user to read the achievements list for
 * @param ReadAchievementsCompleteDelegate the delegate to use for notifications
 */
function ClearReadAchievementsCompleteDelegate(byte LocalUserNum,delegate<OnReadAchievementsComplete> ReadAchievementsCompleteDelegate)
{
	local int RemoveIndex;

	if (LocalUserNum == LoggedInPlayerNum)
	{
		RemoveIndex = ReadAchievementsCompleteDelegates.Find(ReadAchievementsCompleteDelegate);
		if (RemoveIndex != INDEX_NONE)
		{
			ReadAchievementsCompleteDelegates.Remove(RemoveIndex,1);
		}
	}
	else
	{
		`warn("Invalid index ("$LocalUserNum$") passed to ClearReadAchievementsCompleteDelegate()");
	}
}

/**
 * Copies the list of achievements for the specified player and title id
 *
 * @param LocalUserNum the user to read the friends list of
 * @param Achievements the out array that receives the copied data
 * @param TitleId the title id of the game that these were read for
 *
 * @return OERS_Done if the read has completed, otherwise one of the other states
 */
native function EOnlineEnumerationReadState GetAchievements(byte LocalUserNum,out array<AchievementDetails> Achievements,optional int TitleId = 0);

defaultproperties
{
	LoggedInPlayerName="Local Profile"
	ConnectionPresenceTimeInterval=0.5
}
