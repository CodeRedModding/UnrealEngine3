/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class OnlineSubsystemSteamworks extends OnlineSubsystemCommonImpl
	native
	implements(OnlinePlayerInterface, OnlinePlayerInterfaceEx, OnlineVoiceInterface, OnlineStatsInterface, OnlineSystemInterface,
			UserCloudFileInterface, SharedCloudFileInterface)
	config(Engine);


/** True if we're storing an achievement we unlocked, false if just storing stats. */
var bool bStoringAchievement;

/** Whether or not a client stats stores is pending */
var bool bClientStatsStorePending;

/** Counts number of outstanding server-side stats stores. */
var int TotalGSStatsStoresPending;

/** Starts as true, changes to false if pending server-side stats stores fail. */
var bool bGSStatsStoresSuccess;

/** Sets when Steam's UserStatsReceived callback triggers. */
var EOnlineEnumerationReadState UserStatsReceivedState;

/** Pointer to the object that handles the game interface */
var const OnlineGameInterfaceSteamworks CachedGameInt;

/** The name to use for local profiles */
var const localized string LocalProfileName;

/** The name of the player that is logged in */
var const string LoggedInPlayerName;

/** The unique id of the logged in player */
var const UniqueNetId LoggedInPlayerId;

/** The number of the player that called the login function */
var const int LoggedInPlayerNum;

/** The current login status for the player */
var const ELoginStatus LoggedInStatus;

/** The array of delegates that notify write completion of profile data */
var array<delegate<OnWriteProfileSettingsComplete> > WriteProfileSettingsDelegates;

/** The cached profile for the player */
var OnlineProfileSettings CachedProfile;

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

/** Holds the list of delegates that are interested in receiving talking notifications */
var array<delegate<OnPlayerTalkingStateChange> > TalkingDelegates;

/** This is the list of delegates requesting notification when a stats read finishes */
var array<delegate<OnReadOnlineStatsComplete> > ReadOnlineStatsCompleteDelegates;

/** The list of delegates to notify when the stats flush is complete */
var array<delegate<OnFlushOnlineStatsComplete> > FlushOnlineStatsDelegates;

/** This is the list of delegates requesting notification Steamworks's connection state changes */
var array<delegate<OnConnectionStatusChange> > ConnectionStatusChangeDelegates;

/** This is the list of delegates requesting notification of network link status changes */
var array<delegate<OnLinkStatusChange> > LinkStatusDelegates;

/** The list of delegates to notify when a network platform file is read */
var array<delegate<OnReadTitleFileComplete> > ReadTitleFileCompleteDelegates;

/** The array of delegates for notifying when an achievement write has completed */
var array<delegate<OnUnlockAchievementComplete> > AchievementDelegates;

/** The array of delegates for notifying when an achievements list read has completed */
var array<delegate<OnReadAchievementsComplete> > AchievementReadDelegates;

/** The array of delegates for notifying when user file enumeration has completed */
var array<delegate<OnEnumerateUserFilesComplete> > EnumerateUserFilesCompleteDelegates;

/** The array of delegates for notifying when a user file read has completed */
var array<delegate<OnReadUserFileComplete> > ReadUserFileCompleteDelegates;

/** The array of delegates for notifying when a user file write has completed */
var array<delegate<OnWriteUserFileComplete> > WriteUserFileCompleteDelegates;

/** The array of delegates for notifying when a user file delete has completed */
var array<delegate<OnDeleteUserFileComplete> > DeleteUserFileCompleteDelegates;

/** The array of delegates for notifying when a shared read has completed */
var array<delegate<OnReadSharedFileComplete> > SharedFileReadCompleteDelegates;

/** The array of delegates for notifying when a shared write has completed */
var array<delegate<OnWriteSharedFileComplete> > SharedFileWriteCompleteDelegates;

/** The types of global muting we support */
enum EMuteType
{
	MUTE_None,
	MUTE_AllButFriends,
	MUTE_All
};

/** Adds to the local talker definition so we can support muting */
struct native LocalTalkerSteam extends LocalTalker
{
	var EMuteType MuteType;
};

/** Holds the local talker information for the single signed in player */
var LocalTalkerSteam CurrentLocalTalker;

/** This is the list of remote talkers */
var array<RemoteTalker> RemoteTalkers;

/** Identifies the Steamworks game */
var const int AppId;

/** The currently outstanding stats read request */
var const OnlineStatsRead CurrentStatsRead;

/** This holds a single stat waiting to be written out */
struct native PlayerStat
{
	/** The view for this stat */
	var int ViewId;

	/** The column for this stat */
	var int ColumnId;

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

/** This is the list of requested delegates to fire when a friend invite is received */
var array<delegate<OnFriendInviteReceived> > FriendInviteReceivedDelegates;

/** This is the list of requested delegates to fire when a friend message is received */
var array<delegate<OnFriendMessageReceived> > FriendMessageReceivedDelegates;

/** This is the list of requested delegates to fire when a friend by name invite has completed*/
var array<delegate<OnAddFriendByNameComplete> > AddFriendByNameCompleteDelegates;

/** Holds the cached state of the profile for a single player */
struct native ProfileSettingsCache
{
	/** The profile for the player */
	var OnlineProfileSettings Profile;

	/** Used for per player index notification of profile reads completing */
	var array<delegate<OnReadProfileSettingsComplete> > ReadDelegates;

	/** Used for per player index notification of profile writes completing */
	var array<delegate<OnWriteProfileSettingsComplete> > WriteDelegates;

	/** Used to notify subscribers when the player changes their (non-game) profile */
	var array<delegate<OnProfileDataChanged> > ProfileDataChangedDelegates;
};

/** Holds the per player profile data */
var ProfileSettingsCache ProfileCache;

/** Holds the per player online storage data (only for local players) */
var OnlinePlayerStorage PlayerStorageCache;

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

/** Holds the list of delegates that are interested in receiving GetNumberOfCurrentPlayers completions */
var array<delegate<OnGetNumberOfCurrentPlayersComplete> > GetNumberOfCurrentPlayersCompleteDelegates;

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

/** Whether the last frame has connection status or not */
var const bool bLastHasConnection;

/** The amount of time to elapse before checking for connection status change */
var float ConnectionPresenceTimeInterval;

/** Used to check when to verify connection status */
var const float ConnectionPresenceElapsedTime;

/** Whether the stats session is ok to add stats to etc */
var bool bIsStatsSessionOk;

/** Holds the set of people that are muted by the currently logged in player */
var const array<UniqueNetId> MuteList;

/** Whether to use MCP for news or not */
var const config bool bShouldUseMcp;

/**  Where Steamworks notifications will be displayed on the screen */
var config ENetworkNotificationPosition CurrentNotificationPosition;

/** Struct that matches one download object per file for parallel downloading */
struct native SteamUserCloud
{
	/** Owning user for these files */
	var string UserId;

	/** File cache */
	var array<TitleFile> UserCloudFileData;
};

/** Array of files in the cloud for a given user */
var array<SteamUserCloud> UserCloudFiles;

struct native SteamUserCloudMetadata
{
	/** Owning user for these files */
	var string UserId;

	/** File metadata */
	var array<EmsFile> UserCloudMetadata;
};

/** Array of the files in the cloud for a given user */
var array<SteamUserCloudMetadata> UserCloudMetaData;

/** Array of files not owned by this user, retrieved and cached on request until cleared */
var array<TitleFile> SharedFileCache;

/** Struct to hold pending avatar requests */
struct native QueuedAvatarRequest
{
	/** Ticks elapsed since request was made */
	var const float CheckTime;

	/** Number of times we've (re)requested this avatar. */
	var const int NumberOfAttempts;

	/** Steam ID of player to get avatar for. */
	var const UniqueNetId PlayerNetId;

	/** Size, in pixels, of desired avatar image (width and height are equal). */
	var const int Size;

	/** delegate to trigger when we have a result. */
	var const delegate<OnReadOnlineAvatarComplete> ReadOnlineAvatarCompleteDelegate;
};

/** Pending avatar lookups. */
var const array<QueuedAvatarRequest> QueuedAvatarRequests;

/**
 * Maps achievement ids to backend achievement names, stats to achievements, and sets up automatic achievement unlocks/progress-toasts
 *
 * NOTE: If ProgressCount or bAutoUnlock are set, both ViewId and MaxProgress must be set for them to work; the achievement will display
 *		progress/unlock when achievement stats are uploaded through Write/FlushOnlineStats
 * 		(use DisplayAchievementProgress and UnlockAchievement for more fine control of progress-toasts/unlocks)
 *
 * NOTE: If you have achievements linked to stats on the backend, and have progress set there, the backend will automatically unlock
 *		achievements for you. HOWEVER, Steam has a bug which causes this to break for Listen servers; bAutoUnlock always works
 */
struct native AchievementMappingInfo
{
	/** The id of the achievement, as used by UnlockAchievement */
	var int AchievementId;

	/** The name of the achievement, as specified on the Steam backend */
	var name AchievementName;

	// Optional settings

	/** If the achievement is linked to a stats entry, this is the ViewId for the stats entry (ColumnId is determined by AchievementId) */
	var int ViewId;

	/** Pops up an achievement toast every time the achievement stats value increases by this amount (0 = disabled) */
	var int ProgressCount;

	/** Specifies the number of steps required for an unlock */
	var int MaxProgress;

	/** If True, achievements are automatically unlocked when the achievements progress hits/exceeds MaxProgress */
	var bool bAutoUnlock;
};

/**
 * Maps achievement names (as set on the Steam backend) to their AchievementId value, as taken by UnlockAchievement, and sets up other
 * achievement values
 *
 * If not specified, achievements are loaded/unlocked based on the pattern 'Achievement_#', where # is the AchievementId value.
 * Achievements will be loaded starting from 0, and will keep on being loaded until there are no more achievements.
 * NOTE: Achievements must have a picture associated with them, and there must not be a gap in number between achievement names
 */
var config array<AchievementMappingInfo> AchievementMappings;


/** Stores an achievement progress update (internal) */
struct native AchievementProgressStat
{
	var int AchievementId;
	var int Progress;
	var int MaxProgress;
	var bool bUnlock;
};

/** Achievement progress toast updates which are put together in WriteOnlineStats and displayed by FlushOnlineStats (internal) */
var const array<AchievementProgressStat> PendingAchievementProgress;


/** When calling the internal 'ReadLeaderboardEntries' function, this is used to specify what entries to return (internal, native only) */
enum ELeaderboardRequestType
{
	/** Returns entries starting at the top of the leaderboard list (e.g. for getting top 10) */
	LRT_Global,

	/** Returns entries starting at the players entry in the leaderboard */
	LRT_Player,

	/** Only returns entries which the player is friends with on Steam */
	LRT_Friends
};


/** Leaderboard entry sort types (internal) */
enum ELeaderboardSortType
{
	LST_Ascending,
	LST_Descending
};

/** Leaderboard display format on the steam community website (internal) */
enum ELeaderboardFormat
{
	/** A raw number */
	LF_Number,

	/** Time, in seconds */
	LF_Seconds,

	/** Time, in milliseconds */
	LF_Milliseconds
};

/** How to upload leaderboard score updates (configurable, see LeaderboardTemplate) */
enum ELeaderboardUpdateType
{
	/** If current leaderboard score is better than the uploaded one, keep the current one */
	LUT_KeepBest,

	/** Leaderboard score is always replaced with uploaded value */
	LUT_Force
};

/** Internally used dud struct */
struct {SteamLeaderboard_t} LeaderboardHandle
{
	var private const qword Dud;
};

/** Struct describing a leaderboard */
struct native LeaderboardTemplate
{
	/** The name of the leaderboard on the backend */
	var string			LeaderboardName;

	/** How the leaderboard handles score updates (configurable, affects FlushOnlineStats) */
	var ELeaderboardUpdateType	UpdateType;

	// Leaderboard information

	/** The number of entries in the leaderboard (updated after every read/write request for this leaderboard) */
	var const int			LeaderboardSize;

	/** The method used to sort the leaderboard, as defined on backend */
	var const ELeaderboardSortType	SortType;

	/** How the leaderboard should be formatted on the backend */
	var const ELeaderboardFormat	DisplayFormat;

	// Internal; do not modify, do not copy if copying struct entries (copy individual elements if copying the struct instead)

	/** Handle to the Steamworks leaderboard reference */
	var const LeaderboardHandle	LeaderboardRef;

	/** Whether or not initialization is in progress for this leaderboard */
	var const bool			bLeaderboardInitializing;

	/** Whether or not the leaderboard reference has been initiated */
	var const bool			bLeaderboardInitiated;
};

/** Stores a deferred leaderboard read request (internal) */
struct native DeferredLeaderboardRead
{
	var string	LeaderboardName;
	var byte	RequestType;
	var int		Start;
	var int		End;

	var array<UniqueNetId>	PlayerList;
};

/** Stores a deferred leaderboard write request (internal) */
struct native DeferredLeaderboardWrite
{
	var string	LeaderboardName;
	var int		Score;

	var array<int>		LeaderboardData;
};

/** Struct representing an individual leaderboard entry (internal) */
struct native LeaderboardEntry
{
	/** UID of the player this leaderboard entry represents */
	var UniqueNetId			PlayerUID;

	/** Global rank of the player this entry represents */
	var int				Rank;

	/** Leaderboard score */
	var int				Score;

	/** Leaderboard stats data */
	var array<int>			LeaderboardData;
};

/**
 * List of active leaderboards (internal, but it's safe to add and modify entries to specify UpdateType)
 * NOTE: Leaderboards are added as they are used, through ReadOnlineStatsByRank* and WriteOnlineStats;
 *		leaderboard information is populated by the time ReadOnlineStatsComplete/FlushOnlineStatsComplete returns
 */
var array<LeaderboardTemplate> LeaderboardList;

/** If a leaderboard read request needs to first initialize a leaderboard, store the request until initialization completes (internal) */
var const array<DeferredLeaderboardRead> DeferredLeaderboardReads;

/** If a leaderboard write request needs to first initialize a leaderboard, store the request until initialization completes (internal) */
var const array<DeferredLeaderboardWrite> DeferredLeaderboardWrites;

/** Leaderboard stats updates which are put together in WriteOnlineStats, and written by FlushOnlineStats (internal) */
var const array<DeferredLeaderboardWrite> PendingLeaderboardStats;


/** Maps a ViewId (as used by OnlineStats* classes) to a Steam leaderboard name */
struct native ViewIdToLeaderboardName
{
	/** The id of the view */
	var int ViewId;

	/** The leaderboard name */
	var string LeaderboardName;
};

/** Mappings of ViewId's to LeaderboardName's; this >must< be setup for leaderboards to work */
var config array<ViewIdToLeaderboardName> LeaderboardNameMappings;

/** Mapping of ViewId's to game server stats; stats ViewId's which are not in this list, are always written as client stats */
var config array<int> GameServerStatsMappings;


/* This is used with GetSteamClanData(), which is Steamworks-specific, and subject to change! */
struct native SteamPlayerClanData
{
	/** The formal name of the clan */
	var const string ClanName;

	/** The clan's tag */
	var const string ClanTag;
};


/** Pointer to the object that handles the auth interface */
var const OnlineAuthInterfaceSteamworks CachedAuthInt;

`if(`STEAM_MATCHMAKING_LOBBY)

/** The interface to use for creating, searching for and interacting with online lobbies */
var OnlineLobbyInterfaceSteamworks LobbyInterface;

`endif


/**
 * Called from engine start up code to allow the subsystem to initialize
 *
 * @return TRUE if the initialization was successful, FALSE otherwise
 */
native event bool Init();

/**
 * Called from the engine shutdown code, to allow the online subsystem to cleanup
 */
native event Exit();

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
function bool ShowLoginUI(optional bool bShowOnlineOnly = false);

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
function bool Logout(byte LocalUserNum)
{
	return false;
}

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
function AddLogoutCompletedDelegate(byte LocalUserNum,delegate<OnLogoutCompleted> LogoutDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param LogoutDelegate the delegate to use for notifications
 */
function ClearLogoutCompletedDelegate(byte LocalUserNum,delegate<OnLogoutCompleted> LogoutDelegate);

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
		if (ProfileCache.ReadDelegates.Find(ReadProfileSettingsCompleteDelegate) == INDEX_NONE)
		{
			ProfileCache.ReadDelegates[ProfileCache.ReadDelegates.Length] = ReadProfileSettingsCompleteDelegate;
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
// !!! FIXME: all of these should check for logged in user only.
	if (LocalUserNum >= 0 && LocalUserNum < 4)
	{
		RemoveIndex = ProfileCache.ReadDelegates.Find(ReadProfileSettingsCompleteDelegate);
		// Remove this delegate from the array if found
		if (RemoveIndex != INDEX_NONE)
		{
			ProfileCache.ReadDelegates.Remove(RemoveIndex,1);
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
		return PlayerStorageCache;
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
	local int AddIndex;
	// Add this delegate to the array if not already present
	if (TalkingDelegates.Find(TalkerDelegate) == INDEX_NONE)
	{
		AddIndex = TalkingDelegates.Length;
		TalkingDelegates.Length = TalkingDelegates.Length + 1;
		TalkingDelegates[AddIndex] = TalkerDelegate;
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
	RemoveIndex = TalkingDelegates.Find(TalkerDelegate);
	// Only remove if found
	if (RemoveIndex != INDEX_NONE)
	{
		TalkingDelegates.Remove(RemoveIndex,1);
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
	return CurrentNotificationPosition;
}

/**
 * Sets a new position for the network notification icons/images
 *
 * @param NewPos the new location to use
 */
native function SetNetworkNotificationPosition(ENetworkNotificationPosition NewPos);

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
function AddControllerChangeDelegate(delegate<OnControllerChange> ControllerChangeDelegate);

/**
 * Removes the delegate used to notify the gameplay code that the controller state changed
 *
 * @param ControllerChangeDelegate the delegate to remove
 */
function ClearControllerChangeDelegate(delegate<OnControllerChange> ControllerChangeDelegate);

/**
 * Determines if the specified controller is connected or not
 *
 * @param ControllerId the controller to query
 *
 * @return true if connected, false otherwise
 */
function bool IsControllerConnected(int ControllerId);

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
function bool ShowKeyboardUI(byte LocalUserNum,string TitleText,string DescriptionText, optional bool bIsPassword = false,
	optional bool bShouldValidate = true, optional string DefaultText, optional int MaxResultLength = 256);

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
function AddKeyboardInputDoneDelegate(delegate<OnKeyboardInputComplete> InputDelegate);

/**
 * Clears the delegate used to notify the gameplay code that the user has completed
 * their keyboard input
 *
 * @param InputDelegate the delegate to use for notifications
 */
function ClearKeyboardInputDoneDelegate(delegate<OnKeyboardInputComplete> InputDelegate);

/**
 * Fetches the results of the input
 *
 * @param bWasCanceled whether the user cancelled the input or not
 *
 * @return the string entered by the user. Note the string will be empty if it
 * fails validation
 */
function string GetKeyboardInputResults(out byte bWasCanceled);

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
 * NOTE: Not applicable to Steam
 *
 * @return the host's stat guid
 */
function string GetHostStatGuid();

/**
 * Registers the host's stat guid with the client for verification they are part of
 * the stat. Note this is an async task for any backend communication that needs to
 * happen before the registration is deemed complete
 * NOTE: Not applicable to Steam
 *
 * @param HostStatGuid the host's stat guid
 * @return TRUE if the call is successful, FALSE otherwise
 */
function bool RegisterHostStatGuid(const out string HostStatGuid);

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
function AddRegisterHostStatGuidCompleteDelegate(delegate<OnRegisterHostStatGuidComplete> RegisterHostStatGuidCompleteDelegate);

/**
 * Clears the delegate used to notify the gameplay code
 *
 * @param RegisterHostStatGuidCompleteDelegate the delegate to use for notifications
 */
function ClearRegisterHostStatGuidCompleteDelegateDelegate(delegate<OnRegisterHostStatGuidComplete> RegisterHostStatGuidCompleteDelegate);

/**
 * Reads the client's stat guid that was generated by registering the host's guid
 * Used for synching up stats. Only valid on the client. Only callable after the
 * host registration has completed
 * NOTE: Not applicable to Steam
 *
 * @return the client's stat guid
 */
function string GetClientStatGuid();

/**
 * Registers the client's stat guid on the host to validate that the client was in the stat.
 * Used for synching up stats. Only valid on the host.
 * NOTE: Not applicable to Steam
 *
 * @param PlayerId the client's unique net id
 * @param ClientStatGuid the client's stat guid
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
function bool RegisterStatGuid(UniqueNetId PlayerId,const out string ClientStatGuid);

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


// OnlinePlayerInterfaceEx implementation...

/**
 * Displays the UI that allows a player to give feedback on another player
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PlayerId the id of the player having feedback given for
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
native function bool ShowFeedbackUI(byte LocalUserNum,UniqueNetId PlayerId);

/**
 * Displays the gamer card UI for the specified player
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PlayerId the id of the player to show the gamer card of
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
native function bool ShowGamerCardUI(byte LocalUserNum,UniqueNetId PlayerId);

/**
 * Displays the messages UI for a player
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
native function bool ShowMessagesUI(byte LocalUserNum);

/**
 * Displays the achievements UI for a player
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
native function bool ShowAchievementsUI(byte LocalUserNum);

/**
 * Displays the invite ui
 *
 * @param LocalUserNum the local user sending the invite
 * @param InviteText the string to prefill the UI with
 */
native function bool ShowInviteUI(byte LocalUserNum,optional string InviteText);

/**
 * Displays the marketplace UI for content
 *
 * @param LocalUserNum the local user viewing available content
 * @param CategoryMask the bitmask to use to filter content by type
 * @param OfferId a specific offer that you want shown
 */
native function bool ShowContentMarketplaceUI(byte LocalUserNum,optional int CategoryMask = -1,optional int OfferId);

/**
 * Displays the marketplace UI for memberships
 *
 * @param LocalUserNum the local user viewing available memberships
 */
native function bool ShowMembershipMarketplaceUI(byte LocalUserNum);

/**
 * Displays the UI that allows the user to choose which device to save content to
 *
 * @param LocalUserNum the controller number of the associated user
 * @param SizeNeeded the size of the data to be saved in bytes
 * @param bManageStorage whether to allow the user to manage their storage or not
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowDeviceSelectionUI(byte LocalUserNum,int SizeNeeded,optional bool bManageStorage);

/**
 * Delegate used when the device selection request has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnDeviceSelectionComplete(bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the user has completed
 * their device selection
 *
 * @param LocalUserNum the controller number of the associated user
 * @param DeviceDelegate the delegate to use for notifications
 */
function AddDeviceSelectionDoneDelegate(byte LocalUserNum,delegate<OnDeviceSelectionComplete> DeviceDelegate);

/**
 * Removes the specified delegate from the list of callbacks
 *
 * @param LocalUserNum the controller number of the associated user
 * @param DeviceDelegate the delegate to use for notifications
 */
function ClearDeviceSelectionDoneDelegate(byte LocalUserNum,delegate<OnDeviceSelectionComplete> DeviceDelegate);

/**
 * Fetches the results of the device selection
 *
 * @param LocalUserNum the player to check the results for
 * @param DeviceName out param that gets a copy of the string
 *
 * @return the ID of the device that was selected
 * NOTE: Zero means the user hasn't selected one
 */
function int GetDeviceSelectionResults(byte LocalUserNum,out string DeviceName);

/**
 * Checks the device id to determine if it is still valid (could be removed) and/or
 * if there is enough space on the specified device
 *
 * @param DeviceId the device to check
 * @param SizeNeeded the amount of space requested
 *
 * @return true if valid, false otherwise
 */
function bool IsDeviceValid(int DeviceId,optional int SizeNeeded);

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
	// Make sure the user is valid
	if (LocalUserNum >= 0 && LocalUserNum < 4)
	{
		if (AchievementDelegates.Find(UnlockAchievementCompleteDelegate) == INDEX_NONE)
		{
			AchievementDelegates.AddItem(UnlockAchievementCompleteDelegate);
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

	// Make sure the user is valid
	if (LocalUserNum >= 0 && LocalUserNum < 4)
	{
		RemoveIndex = AchievementDelegates.Find(UnlockAchievementCompleteDelegate);
		if (RemoveIndex != INDEX_NONE)
		{
			AchievementDelegates.Remove(RemoveIndex,1);
		}
	}
	else
	{
		`warn("Invalid index ("$LocalUserNum$") passed to ClearUnlockAchievementCompleteDelegate()");
	}
}

/**
 * Unlocks a gamer picture for the local user
 * NOTE: Not applicable to Steam
 *
 * @param LocalUserNum the user to unlock the picture for
 * @param PictureId the id of the picture to unlock
 */
function bool UnlockGamerPicture(byte LocalUserNum, int PictureId);

/**
 * Called when an external change to player profile data has occured
 */
delegate OnProfileDataChanged();

/**
 * Sets the delegate used to notify the gameplay code that someone has changed their profile data externally
 *
 * @param LocalUserNum the user the delegate is interested in
 * @param ProfileDataChangedDelegate the delegate to use for notifications
 */
function AddProfileDataChangedDelegate(byte LocalUserNum,delegate<OnProfileDataChanged> ProfileDataChangedDelegate)
{
	if ( LocalUserNum == 0 )
	{
		if (ProfileCache.ProfileDataChangedDelegates.Find(ProfileDataChangedDelegate) == INDEX_NONE)
		{
			ProfileCache.ProfileDataChangedDelegates.AddItem(ProfileDataChangedDelegate);
		}
	}
	else
	{
		`Log("Invalid user id ("$LocalUserNum$") specified for AddProfileDataChangedDelegate()");
	}
}

/**
 * Clears the delegate used to notify the gameplay code that someone has changed their profile data externally
 *
 * @param LocalUserNum the user the delegate is interested in
 * @param ProfileDataChangedDelegate the delegate to use for notifications
 */
function ClearProfileDataChangedDelegate(byte LocalUserNum,delegate<OnProfileDataChanged> ProfileDataChangedDelegate)
{
	local int RemoveIndex;

	if (LocalUserNum == LoggedInPlayerNum)
	{
		RemoveIndex = ProfileCache.ProfileDataChangedDelegates.Find(ProfileDataChangedDelegate);
		if (RemoveIndex != INDEX_NONE)
		{
			ProfileCache.ProfileDataChangedDelegates.Remove(RemoveIndex,1);
		}
	}
	else
	{
		`Log("Invalid user id ("$LocalUserNum$") specified for ClearProfileDataChangedDelegate()");
	}
}

/**
 * Displays the UI that shows a user's list of friends
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
native function bool ShowFriendsUI(byte LocalUserNum);

/**
 * Displays the UI that shows a user's list of friends
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PlayerId the id of the player being invited
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
native function bool ShowFriendsInviteUI(byte LocalUserNum,UniqueNetId PlayerId);

/**
 * Displays the UI that shows the player list
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
native function bool ShowPlayersUI(byte LocalUserNum);

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
 * @param TitleId the title id that the read was for (0 means current title)
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
	// Make sure the user is valid
	if (LocalUserNum == LoggedInPlayerNum)
	{
		if (AchievementReadDelegates.Find(ReadAchievementsCompleteDelegate) == INDEX_NONE)
		{
			AchievementReadDelegates.AddItem(ReadAchievementsCompleteDelegate);
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

	// Make sure the user is valid
	if (LocalUserNum == LoggedInPlayerNum)
	{
		RemoveIndex = AchievementReadDelegates.Find(ReadAchievementsCompleteDelegate);
		if (RemoveIndex != INDEX_NONE)
		{
			AchievementReadDelegates.Remove(RemoveIndex,1);
		}
	}
	else
	{
		`warn("Invalid index ("$LocalUserNum$") passed to ClearReadAchievementsCompleteDelegate()");
	}
}

/**
 * Copies the list of achievements for the specified player and title id
 * NOTE: Achievement pictures are not guaranteed to be set, you will need to repeatedly call this in order to load missing pictures
 *		Check the 'Images' value for all entries in the returned achievements list, to detect missing images
 *
 * @param LocalUserNum the user to read the friends list of
 * @param Achievements the out array that receives the copied data
 * @param TitleId the title id of the game that these were read for
 *
 * @return OERS_Done if the read has completed, otherwise one of the other states
 */
native function EOnlineEnumerationReadState GetAchievements(byte LocalUserNum,out array<AchievementDetails> Achievements,optional int TitleId = 0);

/**
 * Shows a custom players UI for the specified list of players
 *
 * @param LocalUserNum the controller number of the associated user
 * @param Players the list of players to show in the custom UI
 * @param Title the title to use for the UI
 * @param Description the text to show at the top of the UI
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
native function bool ShowCustomPlayersUI(byte LocalUserNum,const out array<UniqueNetId> Players,string Title,string Description);

/**
 * Notifies the interested party that the avatar read has completed
 *
 * @param PlayerNetId Id of the Player whose avatar this is.
 * @param Avatar the avatar texture. None on error or no avatar available.
 */
delegate OnReadOnlineAvatarComplete(const UniqueNetId PlayerNetId, Texture2D Avatar);

/**
 * Reads an avatar images for the specified player. Results are delivered via OnReadOnlineAvatarComplete delegates.
 *
 * @param PlayerNetId the unique id to read avatar for
 * @param Size The width, in pixels, of the avatar image to read (image will be square, so height==width). The system
 *              will use the closest match available. As this is eventually returned as a Texture2D, you can scale it
 *              however you like, but this parameter dictates image quality, texture memory, and bandwidth used. Steam
 *              currently offers images in 32, 64, and 184 pixels. Other OnlineSubsystems may vary.
 * @param ReadOnlineAvatarCompleteDelegate The delegate to call with results.
 */
native function ReadOnlineAvatar(const UniqueNetId PlayerNetId, int Size, delegate<OnReadOnlineAvatarComplete> ReadOnlineAvatarCompleteDelegate);

/**
 * Starts an async query for the total players. This is the amount of players the system thinks is playing right now, globally,
 *  not just on a specific server.
 *
 * @return TRUE if async call started, FALSE otherwise.
 */
native function bool GetNumberOfCurrentPlayers();

/**
 * Called when the async player count has completed
 *
 * @param TotalPlayers Count of players. -1 if unknown or error.
 */
delegate OnGetNumberOfCurrentPlayersComplete(int TotalPlayers);

/**
 * Sets the delegate used to notify the gameplay code that the player count request has completed
 *
 * @param GetNumberOfCurrentPlayersCompleteDelegate the delegate to use for notifications
 */
function AddGetNumberOfCurrentPlayersCompleteDelegate(delegate<OnGetNumberOfCurrentPlayersComplete> GetNumberOfCurrentPlayersCompleteDelegate)
{
	if (GetNumberOfCurrentPlayersCompleteDelegates.Find(GetNumberOfCurrentPlayersCompleteDelegate) == INDEX_NONE)
	{
		GetNumberOfCurrentPlayersCompleteDelegates.AddItem(GetNumberOfCurrentPlayersCompleteDelegate);
	}
}

/**
 * Clears the delegate used to notify the gameplay code that the player count read request has completed
 *
 * @param GetNumberOfCurrentPlayersCompleteDelegate the delegate to use for notifications
 */
function ClearGetNumberOfCurrentPlayersCompleteDelegate(delegate<OnGetNumberOfCurrentPlayersComplete> GetNumberOfCurrentPlayersCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = GetNumberOfCurrentPlayersCompleteDelegates.Find(GetNumberOfCurrentPlayersCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		GetNumberOfCurrentPlayersCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Get the Clan tags for the current user.
 *
 * This functionality is currently OnlineSubsystemSteamworks specific, and the API will change to be
 *  more general if it is moved into the parent class.
 */
native function GetSteamClanData(out array<SteamPlayerClanData> Results);

/**
 * Unlocks an avatar award for the local user
 *
 * @param LocalUserNum the user to unlock the avatar item for
 * @param AvatarItemId the id of the avatar item to unlock
 */
function bool UnlockAvatarAward(byte LocalUserNum,int AvatarItemId);

/**
 * Reads the online profile settings for a given user and title id
 *
 * @param LocalUserNum the user that we are reading the data for
 * @param TitleId the title that the profile settings are being read for
 * @param ProfileSettings the object to copy the results to and contains the list of items to read
 *
 * @return true if the call succeeds, false otherwise
 */
function bool ReadCrossTitleProfileSettings(byte LocalUserNum,int TitleId,OnlineProfileSettings ProfileSettings);

/**
 * Delegate used when the last read profile settings request has completed
 *
 * @param LocalUserNum the controller index of the player who's read just completed
 * @param TitleId the title that the profile settings were read for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnReadCrossTitleProfileSettingsComplete(byte LocalUserNum,int TitleId,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the last read request has completed
 *
 * @param LocalUserNum which user to watch for read complete notifications
 * @param ReadProfileSettingsCompleteDelegate the delegate to use for notifications
 */
function AddReadCrossTitleProfileSettingsCompleteDelegate(byte LocalUserNum,delegate<OnReadCrossTitleProfileSettingsComplete> ReadProfileSettingsCompleteDelegate);

/**
 * Searches the existing set of delegates for the one specified and removes it
 * from the list
 *
 * @param LocalUserNum which user to watch for read complete notifications
 * @param ReadProfileSettingsCompleteDelegate the delegate to find and clear
 */
function ClearReadCrossTitleProfileSettingsCompleteDelegate(byte LocalUserNum,delegate<OnReadCrossTitleProfileSettingsComplete> ReadProfileSettingsCompleteDelegate);

/**
 * Returns the online profile settings for a given user and title id
 *
 * @param LocalUserNum the user that we are reading the data for
 * @param TitleId the title that the profile settings are being read for
 *
 * @return the profile settings object
 */
function OnlineProfileSettings GetCrossTitleProfileSettings(byte LocalUserNum,int TitleId);

/**
 * Removes a cached entry of a profile for the specified title id
 *
 * @param LocalUserNum the user that we are reading the data for
 * @param TitleId the title that the profile settings are being read for
 */
function ClearCrossTitleProfileSettings(byte LocalUserNum,int TitleId);

/**
 * Shows a dialog with the message pre-populated in it
 *
 * @param LocalUserNum the user sending the message
 * @param Recipients the list of people to send the message to
 * @param MessageTitle the title of the message being sent
 * @param NonEditableMessage the portion of the messge that the user cannot edit
 * @param EditableMessage the portion of the message the user can edit
 *
 * @return true if successful, false otherwise
 */
function bool ShowCustomMessageUI(byte LocalUserNum,const out array<UniqueNetId> Recipients,string MessageTitle,string NonEditableMessage,optional string EditableMessage);


/**
 * Resets the players stats (and achievements, if specified)
 *
 * @param bResetAchievements	If true, also resets player achievements
 * @return			TRUE if successful, FALSE otherwise
 */
native function bool ResetStats(bool bResetAchievements);


/**
 * Creates the specified leaderboard on the Steamworks backend
 * NOTE: It's best to use this for game/mod development purposes only, not for release usage
 *
 * @param LeaderboardName	The name to give the leaderboard (NOTE: This will be the human-readable name displayed on the backend and stats page)
 * @param SortType		The sorting to use for the leaderboard
 * @param DisplayFormat		The way to display leaderboard data
 * @return			Returns True if the leaderboard is being created, False otherwise
 */
native function bool CreateLeaderboard(string LeaderboardName, ELeaderboardSortType SortType, ELeaderboardFormat DisplayFormat);

/**
 * Pops up the Steam toast dialog, notifying the player of their progress with an achievement (does not unlock achievements)
 *
 * @param AchievementId		The id of the achievment which will have its progress displayed
 * @param ProgressCount		The number of completed steps for this achievement
 * @param MaxProgress		The total number of required steps for this achievement, before it will be unlocked
 * @return			TRUE if successful, FALSE otherwise
 */
native function bool DisplayAchievementProgress(int AchievementId, int ProgressCount, int MaxProgress);


/**
 * Converts the specified UID, into the players Steam Community name
 *
 * @param UID		The players UID
 * @return		The username of the player, as stored on the Steam backend
 */
native function string UniqueNetIdToPlayerName(const out UniqueNetId UID);

/**
 * Shows the current (or specified) players Steam profile page, with an optional sub-URL (e.g. for displaying leaderboards)
 *
 * @param LocalUserNum		The controller number of the associated user
 * @param SubURL		An optional sub-URL within the players main profile URL
 * @param PlayerUID		If you want to show the profile of a specific player, pass their UID in here
 */
native function bool ShowProfileUI(byte LocalUserNum, optional string SubURL, optional UniqueNetId PlayerUID);

/**
 * Internal function, for relaying VOIP AudioComponent 'Stop' events to the native code (so references are cleaned up properly)
 *
 * @param AC	The VOIP AudioComponent which has finished playing
 */
function OnVOIPPlaybackFinished(AudioComponent AC)
{
	NotifyVOIPPlaybackFinished(AC);
}

/**
 * Internal function, for relaying VOIP AudioComponent 'Stop' events to the native code (so references are cleaned up properly)
 *
 * @param VOIPAudioComponent	The VOIP AudioComponent which has finished playing
 */
native function NotifyVOIPPlaybackFinished(AudioComponent VOIPAudioComponent);


`if(`STEAM_MATCHMAKING_LOBBY)

/**
 * Called from native code to assign the lobby interface
 *
 * @param NewInterface	The object to assign as providing the lobby interface
 * @return		Returns True if the interface is valid, False otherwise
 */
event bool SetLobbyInterface(object NewInterface)
{
	// @todo Steam: Restore this line, and remove the one below, if OnlineLobbyInterface becomes an actual interface (not sure it matters)
	//LobbyInterface = NewInterface;

	LobbyInterface = OnlineLobbyInterfaceSteamworks(NewInterface);

	// Will return false if the interface isn't supported
	return LobbyInterface != none;
}

`endif

/**
 * Converts the specified UID, into a string representing a 64bit int
 * NOTE: Primarily for use with 'open Steam.#', when P2P sockets are enabled
 *
 * @param UID		The players UID
 * @result		The string representation of the 64bit integer, made from the UID
 */
native function string UniqueNetIdToInt64(const out UniqueNetId UID);

/**
 * Converts the specified string (representing a 64bit int), into a UID
 *
 * @param UIDString	The string representing a 64bit int
 * @param OutUID	The returned UID
 * @return		whether or not the conversion was successful
 */
native function bool Int64ToUniqueNetId(string UIDString, out UniqueNetId OutUID);

/**
 * If the game was launched by a 'join friend' request in Steam, this function retrieves the server info from the commandline
 *
 * @param bMarkAsJoined	If True, future calls to this function return False (but still output the URL/UID)
 * @param ServerURL	The URL (IP:Port) of the server
 * @param ServerUID	The SteamId of the server
 * @return		Returns True if there is data available and the server needs to be joined
 */
native function bool GetCommandlineJoinURL(bool bMarkAsJoined, out string ServerURL, out string ServerUID);

/**
 * Retrives the URL/UID of the server a friend is currently in
 *
 * @param FriendUID	The UID of the friend
 * @param ServerURL	The URL (IP:Port) of the server
 * @param ServerUID	The SteamId of the server (if this is set, the server should be joined using steam sockets)
 * @return		Returns True if there is information available
 */
native function bool GetFriendJoinURL(UniqueNetId FriendUID, out string ServerURL, out string ServerUID);


/**
 * **INTERNAL**
 * Starts an asynchronous write of the specified user file to the network platform's file store
 *
 * @param UserId User owning the storage
 * @param FileToWrite the name of the file to write
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the calls starts successfully, false otherwise
 */
private native function bool WriteUserFileInternal(string UserId,string FileName,const out array<byte> FileContents);

/**
 * Copies the file data into the specified buffer for the specified file
 *
 * @param UserId User owning the storage
 * @param FileName the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
native function bool GetFileContents(string UserId,string FileName,out array<byte> FileContents);

/**
 * Empties the set of downloaded files if possible (no async tasks outstanding)
 *
 * @param UserId User owning the storage
 *
 * @return true if they could be deleted, false if they could not
 */
native function bool ClearFiles(string UserId);

/**
 * Empties the cached data for this file if it is not being downloaded currently
 *
 * @param UserId User owning the storage
 * @param FileName the name of the file to remove from the cache
 *
 * @return true if it could be deleted, false if it could not
 */
native function bool ClearFile(string UserId,string FileName);

/**
 * Delegate fired when the list of files has been returned from the network store
 *
 * @param bWasSuccessful whether the file list was successful or not
 * @param UserId User owning the storage
 *
 */
delegate OnEnumerateUserFilesComplete(bool bWasSuccessful,string UserId);

/**
 * Requests a list of available User files from the network store
 *
 * @param UserId User owning the storage
 *
 */
native function EnumerateUserFiles(string UserId);

/**
 * Adds the delegate to the list to be notified when all files have been enumerated
 *
 * @param EnumerateUserFileCompleteDelegate the delegate to add
 */
function AddEnumerateUserFileCompleteDelegate(delegate<OnEnumerateUserFilesComplete> EnumerateUserFileCompleteDelegate)
{
	// Add this delegate to the array if not already present
	if (EnumerateUserFilesCompleteDelegates.Find(EnumerateUserFileCompleteDelegate) == INDEX_NONE)
	{
		EnumerateUserFilesCompleteDelegates[EnumerateUserFilesCompleteDelegates.Length] = EnumerateUserFileCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param EnumerateUserFileCompleteDelegate the delegate to remove
 */
function ClearEnumerateUserFileCompleteDelegate(delegate<OnEnumerateUserFilesComplete> EnumerateUserFileCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = EnumerateUserFilesCompleteDelegates.Find(EnumerateUserFileCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		EnumerateUserFilesCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Returns the list of User files that was returned by the network store
 *
 * @param UserId User owning the storage
 * @param UserFiles out array of file metadata
 *
 */
native function GetUserFileList(string UserId,out array<EmsFile> UserFiles);

/**
 * Delegate fired when a user file read from the network platform's storage is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param UserId User owning the storage
 * @param FileName the name of the file this was for
 *
 */
delegate OnReadUserFileComplete(bool bWasSuccessful,string UserId,string FileName);

/**
 * Starts an asynchronous read of the specified user file from the network platform's file store
 *
 * @param UserId User owning the storage
 * @param FileToRead the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
native function bool ReadUserFile(string UserId,string FileName);

/**
 * Adds the delegate to the list to be notified when a requested file has been read
 *
 * @param ReadUserFileCompleteDelegate the delegate to add
 */
function AddReadUserFileCompleteDelegate(delegate<OnReadUserFileComplete> ReadUserFileCompleteDelegate)
{
	// Add this delegate to the array if not already present
	if (ReadUserFileCompleteDelegates.Find(ReadUserFileCompleteDelegate) == INDEX_NONE)
	{
		ReadUserFileCompleteDelegates[ReadUserFileCompleteDelegates.Length] = ReadUserFileCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param ReadUserFileCompleteDelegate the delegate to remove
 */
function ClearReadUserFileCompleteDelegate(delegate<OnReadUserFileComplete> ReadUserFileCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = ReadUserFileCompleteDelegates.Find(ReadUserFileCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		ReadUserFileCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Delegate fired when a user file write to the network platform's storage is complete
 *
 * @param bWasSuccessful whether the file Write was successful or not
 * @param UserId User owning the storage
 * @param FileName the name of the file this was for
 *
 */
delegate OnWriteUserFileComplete(bool bWasSuccessful,string UserId,string FileName);

/**
 * Starts an asynchronous write of the specified user file to the network platform's file store
 *
 * @param UserId User owning the storage
 * @param FileToWrite the name of the file to write
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the calls starts successfully, false otherwise
 */
native function bool WriteUserFile(string UserId,string FileName,const out array<byte> FileContents);

/**
 * Adds the delegate to the list to be notified when a requested file has been written
 *
 * @param WriteUserFileCompleteDelegate the delegate to add
 */
function AddWriteUserFileCompleteDelegate(delegate<OnWriteUserFileComplete> WriteUserFileCompleteDelegate)
{
	// Add this delegate to the array if not already present
	if (WriteUserFileCompleteDelegates.Find(WriteUserFileCompleteDelegate) == INDEX_NONE)
	{
		WriteUserFileCompleteDelegates[WriteUserFileCompleteDelegates.Length] = WriteUserFileCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param WriteUserFileCompleteDelegate the delegate to remove
 */
function ClearWriteUserFileCompleteDelegate(delegate<OnWriteUserFileComplete> WriteUserFileCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = WriteUserFileCompleteDelegates.Find(WriteUserFileCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		WriteUserFileCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Delegate fired when a user file delete from the network platform's storage is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param UserId User owning the storage
 * @param FileName the name of the file this was for
 */
delegate OnDeleteUserFileComplete(bool bWasSuccessful,string UserId,string FileName);

/**
 * Starts an asynchronous delete of the specified user file from the network platform's file store
 *
 * @param UserId User owning the storage
 * @param FileToRead the name of the file to read
 * @param bShouldCloudDelete whether to delete the file from the cloud
 * @param bShouldLocallyDelete whether to delete the file locally
 *
 * @return true if the calls starts successfully, false otherwise
 */
native function bool DeleteUserFile(string UserId,string FileName,bool bShouldCloudDelete,bool bShouldLocallyDelete);

/**
 * Adds the delegate to the list to be notified when a requested file has been deleted
 *
 * @param DeleteUserFileCompleteDelegate the delegate to add
 */
function AddDeleteUserFileCompleteDelegate(delegate<OnDeleteUserFileComplete> DeleteUserFileCompleteDelegate)
{
	// Add this delegate to the array if not already present
	if (DeleteUserFileCompleteDelegates.Find(DeleteUserFileCompleteDelegate) == INDEX_NONE)
	{
		DeleteUserFileCompleteDelegates[DeleteUserFileCompleteDelegates.Length] = DeleteUserFileCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param DeleteUserFileCompleteDelegate the delegate to remove
 */
function ClearDeleteUserFileCompleteDelegate(delegate<OnDeleteUserFileComplete> DeleteUserFileCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = DeleteUserFileCompleteDelegates.Find(DeleteUserFileCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		DeleteUserFileCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Copies the shared data into the specified buffer for the specified file
 *
 * @param SharedHandle the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
native function bool GetSharedFileContents(string SharedHandle,out array<byte> FileContents);

/**
 * Empties the set of all downloaded files if possible (no async tasks outstanding)
 *
 * @return true if they could be deleted, false if they could not
 */
native function bool ClearSharedFiles();

/**
 * Empties the cached data for this file if it is not being downloaded currently
 *
 * @param SharedHandle the name of the file to read
 *
 * @return true if it could be deleted, false if it could not
 */
native function bool ClearSharedFile(string SharedHandle);

/**
 * Delegate fired when a shared file read from the network platform's storage is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param FileName the name of the file this was for
 */
delegate OnReadSharedFileComplete(bool bWasSuccessful,string SharedHandle);

/**
 * Starts an asynchronous read of the specified shared file from the network platform's file store
 *
 * @param FileToRead the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
native function bool ReadSharedFile(string SharedHandle);

/**
 * Adds the delegate to the list to be notified when a requested file has been read
 *
 * @param ReadSharedFileCompleteDelegate the delegate to add
 */
function AddReadSharedFileCompleteDelegate(delegate<OnReadSharedFileComplete> ReadSharedFileCompleteDelegate)
{
	// Add this delegate to the array if not already present
	if (SharedFileReadCompleteDelegates.Find(ReadSharedFileCompleteDelegate) == INDEX_NONE)
	{
		SharedFileReadCompleteDelegates[SharedFileReadCompleteDelegates.Length] = ReadSharedFileCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param ReadSharedFileCompleteDelegate the delegate to remove
 */
function ClearReadSharedFileCompleteDelegate(delegate<OnReadSharedFileComplete> ReadSharedFileCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = SharedFileReadCompleteDelegates.Find(ReadSharedFileCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		SharedFileReadCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Delegate fired when a shared file write to the network platform's storage is complete
 *
 * @param bWasSuccessful whether the file Write was successful or not
 * @param UserId User owning the storage
 * @param FileName the name of the file this was for
 * @param SharedHandle the handle to the shared file, may be platform dependent
 */
delegate OnWriteSharedFileComplete(bool bWasSuccessful,string UserId,string FileName,string SharedHandle);

/**
 * Starts an asynchronous write of the specified shared file to the network platform's file store
 *
 * @param UserId User owning the storage
 * @param FileName the name of the file to write
 * @param Contents data to write to the file
 *
 * @return true if the calls starts successfully, false otherwise
 */
native function bool WriteSharedFile(string UserId,string FileName,const out array<byte> Contents);

/**
 * Adds the delegate to the list to be notified when a requested file has been written
 *
 * @param WriteSharedFileCompleteDelegate the delegate to add
 */
function AddWriteSharedFileCompleteDelegate(delegate<OnWriteSharedFileComplete> WriteSharedFileCompleteDelegate)
{
	// Add this delegate to the array if not already present
	if (SharedFileWriteCompleteDelegates.Find(WriteSharedFileCompleteDelegate) == INDEX_NONE)
	{
		SharedFileWriteCompleteDelegates[SharedFileWriteCompleteDelegates.Length] = WriteSharedFileCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param WriteSharedFileCompleteDelegate the delegate to remove
 */
function ClearWriteSharedFileCompleteDelegate(delegate<OnWriteSharedFileComplete> WriteSharedFileCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = SharedFileWriteCompleteDelegates.Find(WriteSharedFileCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		SharedFileWriteCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/** clears all delegates for e.g. end of level cleanup */
function ClearAllDelegates()
{
	EnumerateUserFilesCompleteDelegates.length = 0;
	ReadUserFileCompleteDelegates.length = 0;
	WriteUserFileCompleteDelegates.length = 0;
	DeleteUserFileCompleteDelegates.length = 0;
}

defaultproperties
{
	LoggedInPlayerName="Local Profile"
	ConnectionPresenceTimeInterval=0.5
}
