/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class OnlineSubsystemGameCenter extends OnlineSubsystemCommonImpl
	native
	implements(OnlinePlayerInterface,OnlineVoiceInterface,OnlineStatsInterface,OnlineSystemInterface,OnlineGameInterface,OnlinePlayerInterfaceEx)
	config(Engine);

/** The name of the player that is logged in */
var const string LoggedInPlayerName;

/** The unique id of the logged in player */
var const UniqueNetId LoggedInPlayerId;

/** Current read state */
var EOnlineEnumerationReadState ReadFriendsStatus;

/** List of friends retrieved from the server */
var array<OnlineFriend> CachedFriends;

/** Current game search settings and results */
var OnlineGameSearch GameSearch;

/** The current game settings used when matchmaking into server mode */
var OnlineGameSettings GameSettings;

/** The current stats read object */
var OnlineStatsRead CurrentStatsRead;

/** The string prepended to the achievement ID to generate a globally unique achievement identifier (like com.epicgames.mygame.achievement_). It needs to match what you put in iTunes Connect */
var config string UniqueAchievementPrefix;

/** The string prepended to the view ID to generate a globally unique category identifier (like com.epicgames.mygame.category_). It needs to match what you put in iTunes Connect */
var config string UniqueCategoryPrefix;

/** The directory profile data should be stored in */
var config string ProfileDataDirectory;

/** The file extension to use when saving profile data */
var config string ProfileDataExtension;

/** The array of delegates that notify read completion of profile data */
var array<delegate<OnReadProfileSettingsComplete> > ReadProfileSettingsDelegates;

/** The array of delegates that notify write completion of profile data */
var array<delegate<OnWriteProfileSettingsComplete> > WriteProfileSettingsDelegates;

/** The cached profile for the player */
var OnlineProfileSettings CachedProfile;

/** Holds the list of delegates to fire when any login changes */
var array<delegate<OnLoginChange> > LoginChangeDelegates;

/** Used for per player index notification of login status changes */
var array<delegate<OnLoginStatusChange> > PlayerLoginStatusDelegates;

/** This is the list of requested delegates to fire when a login fails to process */
var array<delegate<OnLoginFailed> > LoginFailedDelegates;

/** This is the list of requested delegates to fire when a login is cancelled */
var array<delegate<OnLoginCancelled> > LoginCancelledDelegates;

/** List of callbacks for when a game invitation was accepted */
var array<delegate<OnGameInviteAccepted> > GameInviteAcceptedDelegates;

/** List of callbacks for when a game invitation was accepted */
var array<delegate<OnUnlockAchievementComplete> > UnlockAchievementCompleteDelegates;

/** The array of delegates for notifying when an achievements list read has completed */
var array<delegate<OnReadAchievementsComplete> > ReadAchievementsCompleteDelegates;

/** List of callbacks for when a game was joined */
var array<delegate<OnJoinOnlineGameComplete> > JoinOnlineGameDelegates;

/** Array of delegates to multicast with for game destruction notification */
var array<delegate<OnDestroyOnlineGameComplete> > DestroyOnlineGameCompleteDelegates;

/** Array of delegates to multicast with for game starting notification */
var array<delegate<OnStartOnlineGameComplete> > StartOnlineGameCompleteDelegates;

/** List of callbacks for when a game was created (ie afdter Game Center matchmaking, and you are chosen as the server) */
var array<delegate<OnCreateOnlineGameComplete> > CreateOnlineGameDelegates;

/** The array of delegates that notify read completion of the friends list data */
var array<delegate<OnReadFriendsComplete> > ReadFriendsDelegates;

/** The array of delegates that notify that the friends list has changed */
var array<delegate<OnFriendsChange> > FriendsChangeDelegates;

/** The array of delegates that notify that the mute list has changed */
var array<delegate<OnMutingChange> > MutingChangeDelegates;

/** Array of delegates to multicast with for game ending notification */
var array<delegate<OnEndOnlineGameComplete> > EndOnlineGameCompleteDelegates;

/** The array of delegates that notify that the stats read operation has completed */
var array<delegate<OnReadOnlineStatsComplete> > ReadOnlineStatsCompleteDelegates;

/** Holds the list of delegates that are interested in receiving talking notifications */
var array<delegate<OnPlayerTalkingStateChange> > TalkingDelegates;

/*** Timer so we can try to fix offline achievements faster */
var transient float CheckForConnectionTime;
var transient bool bWasOnline;

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
function bool ReadTitleFile(string FileToRead);

/**
 * Adds the delegate to the list to be notified when a requested file has been read
 *
 * @param ReadTitleFileCompleteDelegate the delegate to add
 */
function AddReadTitleFileCompleteDelegate(delegate<OnReadTitleFileComplete> ReadTitleFileCompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param ReadTitleFileCompleteDelegate the delegate to remove
 */
function ClearReadTitleFileCompleteDelegate(delegate<OnReadTitleFileComplete> ReadTitleFileCompleteDelegate);

/**
 * Copies the file data into the specified buffer for the specified file
 *
 * @param FileName the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
function bool GetTitleFileContents(string FileName,out array<byte> FileContents);


/**
 * Determines the async state of the tile file read operation
 *
 * @param FileName the name of the file to check on
 *
 * @return the async state of the file read
 */
function EOnlineEnumerationReadState GetTitleFileState(string FileName);

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
function bool AutoLogin();

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
		LoginFailedDelegates.AddItem(LoginFailedDelegate);
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
function bool Logout(byte LocalUserNum);

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
function ELoginStatus GetLoginStatus(byte LocalUserNum)
{
	// we are logged in if we have a player name
	return LoggedInPlayerName != "" ? LS_LoggedIn : LS_NotLoggedIn;
}

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
function EFeaturePrivilegeLevel CanPlayOnline(byte LocalUserNum)
{
	// @todo: Check underage flag?
	return FPL_Enabled;
}

/**
 * Determines whether the player is allowed to use voice or text chat online
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
function EFeaturePrivilegeLevel CanCommunicate(byte LocalUserNum);

/**
 * Determines whether the player is allowed to download user created content
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
function EFeaturePrivilegeLevel CanDownloadUserContent(byte LocalUserNum);

/**
 * Determines whether the player is allowed to buy content online
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
function EFeaturePrivilegeLevel CanPurchaseContent(byte LocalUserNum);

/**
 * Determines whether the player is allowed to view other people's player profile
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
function EFeaturePrivilegeLevel CanViewPlayerProfiles(byte LocalUserNum);

/**
 * Determines whether the player is allowed to have their online presence
 * information shown to remote clients
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
function EFeaturePrivilegeLevel CanShowPresenceInformation(byte LocalUserNum);

/**
 * Checks that a unique player id is part of the specified user's friends list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PlayerId the id of the player being checked
 *
 * @return TRUE if a member of their friends list, FALSE otherwise
 */
function bool IsFriend(byte LocalUserNum,UniqueNetId PlayerId);

/**
 * Checks that whether a group of player ids are among the specified player's
 * friends
 *
 * @param LocalUserNum the controller number of the associated user
 * @param Query an array of players to check for being included on the friends list
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
function bool AreAnyFriends(byte LocalUserNum,out array<FriendsQuery> Query);

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
 * @param LocalUserNum whether to watch for changes on a specific slot or all slots
 */
function AddLoginChangeDelegate(delegate<OnLoginChange> LoginDelegate)
{
	// Add this delegate to the array if not already present
	if (LoginChangeDelegates.Find(LoginDelegate) == INDEX_NONE)
	{
		LoginChangeDelegates.AddItem(LoginDelegate);
	}
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param LoginDelegate the delegate to use for notifications
 * @param LocalUserNum whether to watch for changes on a specific slot or all slots
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
 * Adds a delegate to the list of delegates that are fired when a login is cancelled
 *
 * @param CancelledDelegate the delegate to add to the list
 */
function AddLoginCancelledDelegate(delegate<OnLoginCancelled> CancelledDelegate)
{
	// Add this delegate to the array if not already present
	if (LoginCancelledDelegates.Find(CancelledDelegate) == INDEX_NONE)
	{
		LoginCancelledDelegates.AddItem(CancelledDelegate);
	}
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param CancelledDelegate the delegate to remove fromt he list
 */
function ClearLoginCancelledDelegate(delegate<OnLoginCancelled> CancelledDelegate)
{
	local int RemoveIndex;

	// Remove this delegate from the array if found
	RemoveIndex = LoginCancelledDelegates.Find(CancelledDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		LoginCancelledDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Determines whether the specified user is a local (non-online) login or not
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return true if a local profile, false otherwise
 */
function bool IsLocalLogin(byte LocalUserNum);

/**
 * Determines whether the specified user is a guest login or not
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return true if a guest, false otherwise
 */
function bool IsGuestLogin(byte LocalUserNum);

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
	if (LocalUserNum == 0)
	{
		// Add this delegate to the array if not already present
		if (ReadProfileSettingsDelegates.Find(ReadProfileSettingsCompleteDelegate) == INDEX_NONE)
		{
			ReadProfileSettingsDelegates[ReadProfileSettingsDelegates.Length] = ReadProfileSettingsCompleteDelegate;
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

	if (LocalUserNum == 0)
	{
		RemoveIndex = ReadProfileSettingsDelegates.Find(ReadProfileSettingsCompleteDelegate);
		// Remove this delegate from the array if found
		if (RemoveIndex != INDEX_NONE)
		{
			ReadProfileSettingsDelegates.Remove(RemoveIndex,1);
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
function AddLoginStatusChangeDelegate(delegate<OnLoginStatusChange> LoginStatusDelegate,byte LocalUserNum)
{
	if (LocalUserNum == 0)
	{
		// Add this delegate to the array if not already present
		if (PlayerLoginStatusDelegates.Find(LoginStatusDelegate) == INDEX_NONE)
		{
			PlayerLoginStatusDelegates.AddItem(LoginStatusDelegate);
		}
	}
	else
	{
		`Warn("Invalid user index ("$LocalUserNum$") specified for AddLoginStatusChangeDelegate()");
	}
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param LoginDelegate the delegate to use for notifications
 * @param LocalUserNum the player to watch login status changes for
 */
function ClearLoginStatusChangeDelegate(delegate<OnLoginStatusChange> LoginStatusDelegate,byte LocalUserNum)
{
	local int RemoveIndex;

	if (LocalUserNum == 0)
	{
		RemoveIndex = PlayerLoginStatusDelegates.Find(LoginStatusDelegate);
		// Remove this delegate from the array if found
		if (RemoveIndex != INDEX_NONE)
		{
			PlayerLoginStatusDelegates.Remove(RemoveIndex,1);
		}
	}
	else
	{
		`Warn("Invalid user index ("$LocalUserNum$") specified for ClearLoginStatusChangeDelegate()");
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
function bool RegisterLocalTalker(byte LocalUserNum);

/**
 * Unregisters the user as a talker
 *
 * @param LocalUserNum the local player index to be removed
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
function bool UnregisterLocalTalker(byte LocalUserNum);

/**
 * Registers a remote player as a talker
 *
 * @param PlayerId the unique id of the remote player that is a talker
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
function bool RegisterRemoteTalker(UniqueNetId PlayerId);

/**
 * Unregisters a remote player as a talker
 *
 * @param PlayerId the unique id of the remote player to be removed
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
function bool UnregisterRemoteTalker(UniqueNetId PlayerId);

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
function bool IsHeadsetPresent(byte LocalUserNum);

/**
 * Sets the relative priority for a remote talker. 0 is highest
 *
 * @param LocalUserNum the user that controls the relative priority
 * @param PlayerId the remote talker that is having their priority changed for
 * @param Priority the relative priority to use (0 highest, < 0 is muted)
 *
 * @return TRUE if the function succeeds, FALSE otherwise
 */
function bool SetRemoteTalkerPriority(byte LocalUserNum,UniqueNetId PlayerId,int Priority);

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
	// Add this delegate to the array if not already present
	if (TalkingDelegates.Find(TalkerDelegate) == INDEX_NONE)
	{
		TalkingDelegates.AddItem(TalkerDelegate);
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

	// Remove this delegate from the array if found
	RemoveIndex = TalkingDelegates.Find(TalkerDelegate);
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
function bool StartSpeechRecognition(byte LocalUserNum);

/**
 * Tells the voice system to stop tracking voice data for speech recognition
 *
 * @param LocalUserNum the local user to recognize voice data for
 *
 * @return true upon success, false otherwise
 */
function bool StopSpeechRecognition(byte LocalUserNum);

/**
 * Gets the results of the voice recognition
 *
 * @param LocalUserNum the local user to read the results of
 * @param Words the set of words that were recognized by the voice analyzer
 *
 * @return true upon success, false otherwise
 */
function bool GetRecognitionResults(byte LocalUserNum,out array<SpeechRecognizedWord> Words);

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
function AddRecognitionCompleteDelegate(byte LocalUserNum,delegate<OnRecognitionComplete> RecognitionDelegate);

/**
 * Clears the speech recognition notification callback to use for the specified user
 *
 * @param LocalUserNum the local user to receive notifications for
 * @param RecognitionDelegate the delegate to call when recognition is complete
 */
function ClearRecognitionCompleteDelegate(byte LocalUserNum,delegate<OnRecognitionComplete> RecognitionDelegate);

/**
 * Changes the vocabulary id that is currently being used
 *
 * @param LocalUserNum the local user that is making the change
 * @param VocabularyId the new id to use
 *
 * @return true if successful, false otherwise
 */
function bool SelectVocabulary(byte LocalUserNum,int VocabularyId);

/**
 * Changes the object that is in use to the one specified
 *
 * @param LocalUserNum the local user that is making the change
 * @param SpeechRecogObj the new object use
 *
 * @param true if successful, false otherwise
 */
function bool SetSpeechRecognitionObject(byte LocalUserNum,SpeechRecognition SpeechRecogObj);

/**
 * Notifies the interested party that the last stats read has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnReadOnlineStatsComplete(bool bWasSuccessful);

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
 * Adds the delegate to a list used to notify the gameplay code that the stats read has completed
 *
 * @param ReadOnlineStatsCompleteDelegate the delegate to use for notifications
 */
function AddReadOnlineStatsCompleteDelegate(delegate<OnReadOnlineStatsComplete> ReadOnlineStatsCompleteDelegate)
{
	// Add this delegate to the array if not already present
	if (ReadOnlineStatsCompleteDelegates.Find(ReadOnlineStatsCompleteDelegate) == INDEX_NONE)
	{
		ReadOnlineStatsCompleteDelegates.AddItem(ReadOnlineStatsCompleteDelegate);
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

	// Remove this delegate from the array if found
	RemoveIndex = ReadOnlineStatsCompleteDelegates.Find(ReadOnlineStatsCompleteDelegate);
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
function FreeStats(OnlineStatsRead StatsRead);

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
function bool FlushOnlineStats(name SessionName);

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
function AddFlushOnlineStatsCompleteDelegate(delegate<OnFlushOnlineStatsComplete> FlushOnlineStatsCompleteDelegate);

/**
 * Clears the delegate used to notify the gameplay code that the stats flush has completed
 *
 * @param FlushOnlineStatsCompleteDelegate the delegate to use for notifications
 */
function ClearFlushOnlineStatsCompleteDelegate(delegate<OnFlushOnlineStatsComplete> FlushOnlineStatsCompleteDelegate);

/**
 * Writes the score data for the match
 *
 * @param SessionName the name of the session to write scores for
 * @param LeaderboardId the leaderboard to write the score information to
 * @param PlayerScores the list of players, teams, and scores they earned
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
function bool WriteOnlinePlayerScores(name SessionName,int LeaderboardId,const out array<OnlinePlayerScore> PlayerScores);

/**
 * Reads the host's stat guid for synching up stats. Only valid on the host.
 *
 * @return the host's stat guid
 */
function string GetHostStatGuid();

/**
 * Registers the host's stat guid with the client for verification they are part of
 * the stat. Note this is an async task for any backend communication that needs to
 * happen before the registration is deemed complete
 *
 * @param HostStatGuid the host's stat guid
 *
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
function AddRegisterHostStatGuidCompleteDelegate(delegate<OnFlushOnlineStatsComplete> RegisterHostStatGuidCompleteDelegate);

/**
 * Clears the delegate used to notify the gameplay code
 *
 * @param RegisterHostStatGuidCompleteDelegate the delegate to use for notifications
 */
function ClearRegisterHostStatGuidCompleteDelegateDelegate(delegate<OnFlushOnlineStatsComplete> RegisterHostStatGuidCompleteDelegate);

/**
 * Reads the client's stat guid that was generated by registering the host's guid
 * Used for synching up stats. Only valid on the client. Only callable after the
 * host registration has completed
 *
 * @return the client's stat guid
 */
function string GetClientStatGuid();

/**
 * Registers the client's stat guid on the host to validate that the client was in the stat.
 * Used for synching up stats. Only valid on the host.
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
 * Returns the unique id of the player for the specified index
 *
 * @param UserIndex the user to return the id of
 *
 * @return the unique id of the player at the specified index
 */
event UniqueNetId GetPlayerUniqueNetIdFromIndex(int UserIndex)
{
	local UniqueNetId Zero;

	if (UserIndex == 0)
	{
		return LoggedInPlayerId;
	}
	return Zero;
}

/**
 * Determines if the ethernet link is connected or not
 */
function bool HasLinkConnection()
{
	return true;
}

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
function AddLinkStatusChangeDelegate(delegate<OnLinkStatusChange> LinkStatusDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param LinkStatusDelegate the delegate to remove
 */
function ClearLinkStatusChangeDelegate(delegate<OnLinkStatusChange> LinkStatusDelegate);

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
function ENetworkNotificationPosition GetNetworkNotificationPosition();

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
function bool IsControllerConnected(int ControllerId)
{
	if (ControllerId == 0)
	{
		return true;
	}
	return false;
}

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
function AddConnectionStatusChangeDelegate(delegate<OnConnectionStatusChange> ConnectionStatusDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param ConnectionStatusDelegate the delegate to remove
 */
function ClearConnectionStatusChangeDelegate(delegate<OnConnectionStatusChange> ConnectionStatusDelegate);

/**
 * Determines the NAT type the player is using
 */
function ENATType GetNATType()
{
	return NAT_Open;
}

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
 * @param ConnectionStatusDelegate the delegate to remove
 */
function ClearStorageDeviceChangeDelegate(delegate<OnStorageDeviceChange> StorageDeviceChangeDelegate);


/**
 * Sets the online status information to use for the specified player. Used to
 * tell other players what the player is doing (playing, menus, away, etc.)
 *
 * @param LocalUserNum the controller number of the associated user
 * @param StatusId the status id to use (maps to strings where possible)
 * @param LocalizedStringSettings the list of localized string settings to set
 * @param Properties the list of properties to set
 */
function SetOnlineStatus(byte LocalUserNum,int StatusId,const out array<LocalizedStringSetting> LocalizedStringSettings,const out array<SettingsProperty> Properties);

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
function bool ShowKeyboardUI(byte LocalUserNum,string TitleText,string DescriptionText,
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
 * @param bWasCancelled whether the user canceled the input or not
 *
 * @return the string entered by the user. Note the string will be empty if it
 * fails validation
 */
function string GetKeyboardInputResults(out byte bWasCanceled);

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
function AddWritePlayerStorageCompleteDelegate(byte LocalUserNum,delegate<OnWritePlayerStorageComplete> WritePlayerStorageCompleteDelegate);

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
function AddReadPlayerStorageForNetIdCompleteDelegate(UniqueNetId NetId,delegate<OnReadPlayerStorageForNetIdComplete> ReadPlayerStorageForNetIdCompleteDelegate);

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
 * Searches the existing set of delegates for the one specified and removes it
 * from the list
 *
 * @param NetId the net id for the user to watch for read complete notifications
 * @param ReadPlayerStorageForNetIdCompleteDelegate the delegate to find and clear
 */
function ClearReadPlayerStorageForNetIdCompleteDelegate(UniqueNetId NetId,delegate<OnReadPlayerStorageForNetIdComplete> ReadPlayerStorageForNetIdCompleteDelegate);

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
function AddReadPlayerStorageCompleteDelegate(byte LocalUserNum,delegate<OnReadPlayerStorageComplete> ReadPlayerStorageCompleteDelegate);

/**
 * Searches the existing set of delegates for the one specified and removes it
 * from the list
 *
 * @param LocalUserNum which user to watch for read complete notifications
 * @param ReadPlayerStorageCompleteDelegate the delegate to find and clear
 */
function ClearReadPlayerStorageCompleteDelegate(byte LocalUserNum,delegate<OnReadPlayerStorageComplete> ReadPlayerStorageCompleteDelegate);

/**
 * Returns the online player storage for a given local user
 *
 * @param LocalUserNum the user that we are reading the data for
 *
 * @return the player storage object
 */
function OnlinePlayerStorage GetPlayerStorage(byte LocalUserNum)
{
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
 * Clears the delegate used to notify the gameplay code that the last write request has completed
 *
 * @param LocalUserNum which user to watch for write complete notifications
 * @param WritePlayerStorageCompleteDelegate the delegate to use for notifications
 */
function ClearWritePlayerStorageCompleteDelegate(byte LocalUserNum,delegate<OnWritePlayerStorageComplete> WritePlayerStorageCompleteDelegate);

/**
 * Sends a friend invite to the specified player
 *
 * @param LocalUserNum the user that is sending the invite
 * @param NewFriend the player to send the friend request to
 * @param Message the message to display to the recipient
 *
 * @return true if successful, false otherwise
 */
function bool AddFriend(byte LocalUserNum,UniqueNetId NewFriend,optional string Message);

/**
 * Sends a friend invite to the specified player nick
 *
 * @param LocalUserNum the user that is sending the invite
 * @param FriendName the name of the player to send the invite to
 * @param Message the message to display to the recipient
 *
 * @return true if successful, false otherwise
 */
function bool AddFriendByName(byte LocalUserNum,string FriendName,optional string Message);

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
function AddAddFriendByNameCompleteDelegate(byte LocalUserNum,delegate<OnAddFriendByNameComplete> FriendDelegate);

/**
 * Removes the delegate specified from the list
 *
 * @param LocalUserNum the user associated with the notification
 * @param FriendDelegate the delegate to use for notifications
 */
function ClearAddFriendByNameCompleteDelegate(byte LocalUserNum,delegate<OnAddFriendByNameComplete> FriendDelegate);

/**
 * Used to accept a friend invite sent to this player
 *
 * @param LocalUserNum the user the invite is for
 * @param RequestingPlayer the player the invite is from
 *
 * @param true if successful, false otherwise
 */
function bool AcceptFriendInvite(byte LocalUserNum,UniqueNetId RequestingPlayer);

/**
 * Used to deny a friend request sent to this player
 *
 * @param LocalUserNum the user the invite is for
 * @param RequestingPlayer the player the invite is from
 *
 * @param true if successful, false otherwise
 */
function bool DenyFriendInvite(byte LocalUserNum,UniqueNetId RequestingPlayer);

/**
 * Removes a friend from the player's friend list
 *
 * @param LocalUserNum the user that is removing the friend
 * @param FormerFriend the player to remove from the friend list
 *
 * @return true if successful, false otherwise
 */
function bool RemoveFriend(byte LocalUserNum,UniqueNetId FormerFriend);

/**
 * Called when a friend invite arrives for a local player
 *
 * @param LocalUserNum the user that is receiving the invite
 * @param RequestingPlayer the player sending the friend request
 * @param RequestingNick the nick of the player sending the friend request
 * @param Message the message to display to the recipient
 */
delegate OnFriendInviteReceived(byte LocalUserNum,UniqueNetId RequestingPlayer,string RequestingNick,string Message);

/**
 * Adds the delegate used to notify the gameplay code that the user has received a friend invite
 *
 * @param LocalUserNum the user associated with the notification
 * @param InviteDelegate the delegate to use for notifications
 */
function AddFriendInviteReceivedDelegate(byte LocalUserNum,delegate<OnFriendInviteReceived> InviteDelegate);

/**
 * Removes the delegate specified from the list
 *
 * @param LocalUserNum the user associated with the notification
 * @param InviteDelegate the delegate to use for notifications
 */
function ClearFriendInviteReceivedDelegate(byte LocalUserNum,delegate<OnFriendInviteReceived> InviteDelegate);

/**
 * Sends a message to a friend
 *
 * @param LocalUserNum the user that is sending the message
 * @param Friend the player to send the message to
 * @param Message the message to display to the recipient
 *
 * @return true if successful, false otherwise
 */
function bool SendMessageToFriend(byte LocalUserNum,UniqueNetId Friend,string Message);

/**
 * Sends an invitation to play in the player's current session
 *
 * @param LocalUserNum the user that is sending the invite
 * @param Friend the player to send the invite to
 * @param Text the text of the message for the invite
 *
 * @return true if successful, false otherwise
 */
function bool SendGameInviteToFriend(byte LocalUserNum,UniqueNetId Friend,optional string Text);

/**
 * Sends invitations to play in the player's current session
 *
 * @param LocalUserNum the user that is sending the invite
 * @param Friends the player to send the invite to
 * @param Text the text of the message for the invite
 *
 * @return true if successful, false otherwise
 */
function bool SendGameInviteToFriends(byte LocalUserNum,array<UniqueNetId> Friends,optional string Text);

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
function AddReceivedGameInviteDelegate(byte LocalUserNum,delegate<OnReceivedGameInvite> ReceivedGameInviteDelegate);

/**
 * Removes the delegate specified from the list
 *
 * @param LocalUserNum the user associated with the notification
 * @param ReceivedGameInviteDelegate the delegate to use for notifications
 */
function ClearReceivedGameInviteDelegate(byte LocalUserNum,delegate<OnReceivedGameInvite> ReceivedGameInviteDelegate);

/**
 * Allows the local player to follow a friend into a game
 *
 * @param LocalUserNum the local player wanting to join
 * @param Friend the player that is being followed
 *
 * @return true if the async call worked, false otherwise
 */
function bool JoinFriendGame(byte LocalUserNum,UniqueNetId Friend);

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
function AddJoinFriendGameCompleteDelegate(delegate<OnJoinFriendGameComplete> JoinFriendGameCompleteDelegate);

/**
 * Removes the delegate from the list of notifications
 *
 * @param JoinFriendGameCompleteDelegate the delegate to use for notifications
 */
function ClearJoinFriendGameCompleteDelegate(delegate<OnJoinFriendGameComplete> JoinFriendGameCompleteDelegate);

/**
 * Returns the list of messages for the specified player
 *
 * @param LocalUserNum the local player wanting to join
 * @param FriendMessages the set of messages cached locally for the player
 */
function GetFriendMessages(byte LocalUserNum,out array<OnlineFriendMessage> FriendMessages);

/**
 * Called when a friend invite arrives for a local player
 *
 * @param LocalUserNum the user that is receiving the message
 * @param SendingPlayer the player sending the message
 * @param SendingNick the nick of the player sending the message
 * @param Message the message to display to the recipient
 */
delegate OnFriendMessageReceived(byte LocalUserNum,UniqueNetId SendingPlayer,string SendingNick,string Message);

/**
 * Adds the delegate used to notify the gameplay code that the user has received a friend invite
 *
 * @param LocalUserNum the user associated with the notification
 * @param MessageDelegate the delegate to use for notifications
 */
function AddFriendMessageReceivedDelegate(byte LocalUserNum,delegate<OnFriendMessageReceived> MessageDelegate);

/**
 * Removes the delegate specified from the list
 *
 * @param LocalUserNum the user associated with the notification
 * @param MessageDelegate the delegate to use for notifications
 */
function ClearFriendMessageReceivedDelegate(byte LocalUserNum,delegate<OnFriendMessageReceived> MessageDelegate);

/**
 * Mutes all voice or all but friends
 *
 * @param LocalUserNum the local user that is making the change
 * @param bAllowFriends whether to mute everyone or allow friends
 */
function bool MuteAll(byte LocalUserNum,bool bAllowFriends);

/**
 * Allows all speakers to send voice
 *
 * @param LocalUserNum the local user that is making the change
 */
function bool UnmuteAll(byte LocalUserNum);

/**
 * Deletes a message from the list of messages
 *
 * @param LocalUserNum the user that is deleting the message
 * @param MessageIndex the index of the message to delete
 *
 * @return true if the message was deleted, false otherwise
 */
function bool DeleteMessage(byte LocalUserNum,int MessageIndex);

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
	// Add this delegate to the array if not already present
	if (UnlockAchievementCompleteDelegates.Find(UnlockAchievementCompleteDelegate) == INDEX_NONE)
	{
		UnlockAchievementCompleteDelegates.AddItem(UnlockAchievementCompleteDelegate);
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

	// Remove this delegate from the array if found
	RemoveIndex = UnlockAchievementCompleteDelegates.Find(UnlockAchievementCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		UnlockAchievementCompleteDelegates.Remove(RemoveIndex,1);
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
	// Add this delegate to the array if not already present
	if (ReadAchievementsCompleteDelegates.Find(ReadAchievementsCompleteDelegate) == INDEX_NONE)
	{
		ReadAchievementsCompleteDelegates.AddItem(ReadAchievementsCompleteDelegate);
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

	// Remove this delegate from the array if found
	RemoveIndex = ReadAchievementsCompleteDelegates.Find(ReadAchievementsCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		ReadAchievementsCompleteDelegates.Remove(RemoveIndex,1);
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
function bool CreateOnlineGame(byte HostingPlayerNum,name SessionName,OnlineGameSettings NewGameSettings);

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
	// Add this delegate to the array if not already present
	if (CreateOnlineGameDelegates.Find(CreateOnlineGameCompleteDelegate) == INDEX_NONE)
	{
		CreateOnlineGameDelegates.AddItem(CreateOnlineGameCompleteDelegate);
	}
}

/**
 * Removes the delegate from the list of notifications
 *
 * @param CreateOnlineGameCompleteDelegate the delegate to use for notifications
 */
function ClearCreateOnlineGameCompleteDelegate(delegate<OnCreateOnlineGameComplete> CreateOnlineGameCompleteDelegate)
{
	local int RemoveIndex;

	// Remove this delegate from the array if found
	RemoveIndex = CreateOnlineGameDelegates.Find(CreateOnlineGameCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		CreateOnlineGameDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Updates the localized settings/properties for the game in question
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
function AddUpdateOnlineGameCompleteDelegate(delegate<OnUpdateOnlineGameComplete> UpdateOnlineGameCompleteDelegate);

/**
 * Removes the delegate from the list of notifications
 *
 * @param UpdateOnlineGameCompleteDelegate the delegate to use for notifications
 */
function ClearUpdateOnlineGameCompleteDelegate(delegate<OnUpdateOnlineGameComplete> UpdateOnlineGameCompleteDelegate);

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
		DestroyOnlineGameCompleteDelegates.AddItem(DestroyOnlineGameCompleteDelegate);
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
function bool FindOnlineGames(byte SearchingPlayerNum,OnlineGameSearch SearchSettings);

/**
 * Delegate fired when the search for an online game has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnFindOnlineGamesComplete(bool bWasSuccessful);

/**
 * Adds the delegate used to notify the gameplay code that the search they
 * kicked off has completed
 *
 * @param FindOnlineGamesCompleteDelegate the delegate to use for notifications
 */
function AddFindOnlineGamesCompleteDelegate(delegate<OnFindOnlineGamesComplete> FindOnlineGamesCompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param FindOnlineGamesCompleteDelegate the delegate to use for notifications
 */
function ClearFindOnlineGamesCompleteDelegate(delegate<OnFindOnlineGamesComplete> FindOnlineGamesCompleteDelegate);

/**
 * Cancels the current search in progress if possible for that search type
 *
 * @return true if successful searching for sessions, false otherwise
 */
function bool CancelFindOnlineGames();

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
function AddCancelFindOnlineGamesCompleteDelegate(delegate<OnCancelFindOnlineGamesComplete> CancelFindOnlineGamesCompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param CancelFindOnlineGamesCompleteDelegate the delegate to use for notifications
 */
function ClearCancelFindOnlineGamesCompleteDelegate(delegate<OnCancelFindOnlineGamesComplete> CancelFindOnlineGamesCompleteDelegate);

/**
 * Serializes the platform specific data into the provided buffer for the specified search result
 *
 * @param DesiredGame the game to copy the platform specific data for
 * @param PlatformSpecificInfo the buffer to fill with the platform specific information
 *
 * @return true if successful reading the data, false otherwise
 */
function bool ReadPlatformSpecificSessionInfo(const out OnlineGameSearchResult DesiredGame,out byte PlatformSpecificInfo[80]);

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
 * @return true if successful serializing the data, false otherwise
 */
function bool BindPlatformSpecificSessionToSearch(byte SearchingPlayerNum,OnlineGameSearch SearchSettings,byte PlatformSpecificInfo[80]);

/**
 * Returns the currently set game search object
 */
function OnlineGameSearch GetGameSearch()
{
	return GameSearch;
}

/**
 * Cleans up any platform specific allocated data contained in the search results
 *
 * @param Search the object to free search results for
 *
 * @return true if successful, false otherwise
 */
native function bool FreeSearchResults(optional OnlineGameSearch Search);


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
function AddJoinMigratedOnlineGameCompleteDelegate(delegate<OnJoinMigratedOnlineGameComplete> JoinMigratedOnlineGameCompleteDelegate);

/**
 * Removes the delegate from the list of notifications
 *
 * @param JoinMigratedOnlineGameCompleteDelegate the delegate to use for notifications
 */
function ClearJoinMigratedOnlineGameCompleteDelegate(delegate<OnJoinMigratedOnlineGameComplete> JoinMigratedOnlineGameCompleteDelegate);

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
 * Joins the game specified
 *
 * @param PlayerNum the index of the player searching for a match
 * @param SessionName the name of the session to join
 * @param DesiredGame the desired game to join
 *
 * @return true if the call completed successfully, false otherwise
 */
function bool JoinOnlineGame(byte PlayerNum,name SessionName,const out OnlineGameSearchResult DesiredGame);

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
	// Add this delegate to the array if not already present
	if (JoinOnlineGameDelegates.Find(JoinOnlineGameCompleteDelegate) == INDEX_NONE)
	{
		JoinOnlineGameDelegates.AddItem(JoinOnlineGameCompleteDelegate);
	}
}

/**
 * Removes the delegate from the list of notifications
 *
 * @param JoinOnlineGameCompleteDelegate the delegate to use for notifications
 */
function ClearJoinOnlineGameCompleteDelegate(delegate<OnJoinOnlineGameComplete> JoinOnlineGameCompleteDelegate)
{
	local int RemoveIndex;

	// Remove this delegate from the array if found
	RemoveIndex = JoinOnlineGameDelegates.Find(JoinOnlineGameCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		JoinOnlineGameDelegates.Remove(RemoveIndex,1);
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
 * Removes the delegate from the list of notifications
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
 * Removes the delegate from the list of notifications
 *
 * @param UnregisterPlayerCompleteDelegate the delegate to use for notifications
 */
function ClearUnregisterPlayerCompleteDelegate(delegate<OnUnregisterPlayerComplete> UnregisterPlayerCompleteDelegate);

/**
* Delegate fired when the online game has transitioned to the started state
*
* @param SessionName the name of the session the that has transitioned to started
* @param bWasSuccessful true if the async action completed without error, false if there was an error
*/
delegate OnStartOnlineGameComplete(name SessionName,bool bWasSuccessful);

/**
 * Marks an online game as in progress (as opposed to being in lobby or pending)
 *
 * @param SessionName the name of the session that is being started
 *
 * @return true if the call succeeds, false otherwise
 */
function bool StartOnlineGame(name SessionName)
{
	local delegate<OnStartOnlineGameComplete> StartDelegate;

	foreach StartOnlineGameCompleteDelegates(StartDelegate)
	{
		StartDelegate(SessionName, true);
	}
	return true;
}

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
		StartOnlineGameCompleteDelegates.AddItem(StartOnlineGameCompleteDelegate);
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
	// Add this delegate to the array if not already present
	if (EndOnlineGameCompleteDelegates.Find(EndOnlineGameCompleteDelegate) == INDEX_NONE)
	{
		EndOnlineGameCompleteDelegates.AddItem(EndOnlineGameCompleteDelegate);
	}
}

/**
 * Removes the delegate from the list of notifications
 *
 * @param EndOnlineGameCompleteDelegate the delegate to use for notifications
 */
function ClearEndOnlineGameCompleteDelegate(delegate<OnEndOnlineGameComplete> EndOnlineGameCompleteDelegate)
{
	local int RemoveIndex;

	// Remove this delegate from the array if found
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
 * Removes the delegate from the list of notifications
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
function AddGameInviteAcceptedDelegate(byte LocalUserNum,delegate<OnGameInviteAccepted> GameInviteAcceptedDelegate)
{
	// Add this delegate to the array if not already present
	if (GameInviteAcceptedDelegates.Find(GameInviteAcceptedDelegate) == INDEX_NONE)
	{
		GameInviteAcceptedDelegates.AddItem(GameInviteAcceptedDelegate);
	}
}

/**
 * Removes the delegate from the list of notifications
 *
 * @param GameInviteAcceptedDelegate the delegate to use for notifications
 */
function ClearGameInviteAcceptedDelegate(byte LocalUserNum,delegate<OnGameInviteAccepted> GameInviteAcceptedDelegate)
{
	local int RemoveIndex;

	// Remove this delegate from the array if found
	RemoveIndex = GameInviteAcceptedDelegates.Find(GameInviteAcceptedDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		GameInviteAcceptedDelegates.Remove(RemoveIndex,1);
	}
}

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
 * Displays the UI that allows a player to give feedback on another player
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PlayerId the id of the player having feedback given for
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowFeedbackUI(byte LocalUserNum,UniqueNetId PlayerId);

/**
 * Displays the gamer card UI for the specified player
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PlayerId the id of the player to show the gamer card of
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowGamerCardUI(byte LocalUserNum,UniqueNetId PlayerId);

/**
 * Displays the messages UI for a player
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowMessagesUI(byte LocalUserNum);

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
function bool ShowInviteUI(byte LocalUserNum,optional string InviteText);

/**
 * Displays the marketplace UI for content
 *
 * @param LocalUserNum the local user viewing available content
 * @param CategoryMask the bitmask to use to filter content by type
 * @param OfferId a specific offer that you want shown
 */
function bool ShowContentMarketplaceUI(byte LocalUserNum,optional int CategoryMask = -1,optional int OfferId);

/**
 * Displays the marketplace UI for memberships
 *
 * @param LocalUserNum the local user viewing available memberships
 */
function bool ShowMembershipMarketplaceUI(byte LocalUserNum);

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
 * Adds the delegate used to notify the gameplay code that the user has completed
 * their device selection
 *
 * @param DeviceDelegate the delegate to use for notifications
 */
function AddDeviceSelectionDoneDelegate(byte LocalUserNum,delegate<OnDeviceSelectionComplete> DeviceDelegate);

/**
 * Removes the specified delegate from the list of callbacks
 *
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
 * Unlocks a gamer picture for the local user
 *
 * @param LocalUserNum the user to unlock the picture for
 * @param PictureId the id of the picture to unlock
 */
function bool UnlockGamerPicture(byte LocalUserNum,int PictureId);

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
function AddProfileDataChangedDelegate(byte LocalUserNum,delegate<OnProfileDataChanged> ProfileDataChangedDelegate);

/**
 * Clears the delegate used to notify the gameplay code that someone has changed their profile data externally
 *
 * @param LocalUserNum the user the delegate is interested in
 * @param ProfileDataChangedDelegate the delegate to use for notifications
 */
function ClearProfileDataChangedDelegate(byte LocalUserNum,delegate<OnProfileDataChanged> ProfileDataChangedDelegate);

/**
 * Displays the UI that shows a user's list of friends
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PlayerId the id of the player being invited
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowFriendsInviteUI(byte LocalUserNum,UniqueNetId PlayerId);

/**
 * Displays the UI that shows the player list
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowPlayersUI(byte LocalUserNum);

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
function bool ShowCustomPlayersUI(byte LocalUserNum,const out array<UniqueNetId> Players,string Title,string Description);

/**
 * Unlocks an avatar award for the local user
 *
 * @param LocalUserNum the user to unlock the avatar item for
 * @param AvatarItemId the id of the avatar item to unlock
 */
function bool UnlockAvatarAward(byte LocalUserNum,int AvatarItemId);

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
 * Unregisters a group of players with the online service as being part of the online game
 *
 * @param SessionName the name of the session the player is joining
 * @param Players the list of players to unregister with the online service
 *
 * @return true if the call succeeds, false otherwise
 */
function bool UnregisterPlayers(name SessionName,const out array<UniqueNetId> Players);

/**
 * Reads the online profile settings for a given user
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
 * Returns the online profile settings for a given user
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
 * @param NonEditableMessage the portion of the message that the user cannot edit
 * @param EditableMessage the portion of the message the user can edit
 *
 * @return true if successful, false otherwise
 */
function bool ShowCustomMessageUI(byte LocalUserNum,const out array<UniqueNetId> Recipients,string MessageTitle,string NonEditableMessage,optional string EditableMessage);	