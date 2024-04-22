/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class OnlineSubsystemPC extends OnlineSubsystemCommonImpl
	native
	implements(OnlinePlayerInterface,OnlineVoiceInterface,OnlineStatsInterface,OnlineSystemInterface)
	config(Engine);

/** The name of the player that is logged in */
var const string LoggedInPlayerName;

/** The unique id of the logged in player */
var const UniqueNetId LoggedInPlayerId;

/**
 * This is the array of pending async tasks. Each tick these tasks are checked
 * for completion. If complete, the delegate associated with them is called
 */
var native const array<pointer> AsyncTasks{FOnlineAsyncTaskPC};

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

/** List of callbacks to notify when speech recognition is complete */
var array<delegate<OnRecognitionComplete> > SpeechRecognitionCompleteDelegates;

/** The array of delegates that notify read completion of the friends list data */
var array<delegate<OnReadFriendsComplete> > ReadFriendsDelegates;

/** The array of delegates that notify that the friends list has changed */
var array<delegate<OnFriendsChange> > FriendsChangeDelegates;

/** The array of delegates that notify that the mute list has changed */
var array<delegate<OnMutingChange> > MutingChangeDelegates;

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
function AddPlayerTalkingDelegate(delegate<OnPlayerTalkingStateChange> TalkerDelegate);

/**
 * Removes a talker delegate to the list of notifications
 *
 * @param TalkerDelegate the delegate to remove from the notification list
 */
function ClearPlayerTalkingDelegate(delegate<OnPlayerTalkingStateChange> TalkerDelegate);

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
function bool Login(byte LocalUserNum,string LoginName,string Password,optional bool bWantsLocalOnly);

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
function AddLoginFailedDelegate(byte LocalUserNum,delegate<OnLoginFailed> LoginDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param LoginDelegate the delegate to use for notifications
 */
function ClearLoginFailedDelegate(byte LocalUserNum,delegate<OnLoginFailed> LoginDelegate);

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
	return 	LS_NotLoggedIn;
;
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
function EFeaturePrivilegeLevel CanPlayOnline(byte LocalUserNum);

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
function AddLoginChangeDelegate(delegate<OnLoginChange> LoginDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param LoginDelegate the delegate to use for notifications
 * @param LocalUserNum whether to watch for changes on a specific slot or all slots
 */
function ClearLoginChangeDelegate(delegate<OnLoginChange> LoginDelegate);

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
function AddLoginStatusChangeDelegate(delegate<OnLoginStatusChange> LoginStatusDelegate,byte LocalUserNum);

/**
 * Removes the specified delegate from the notification list
 *
 * @param LoginDelegate the delegate to use for notifications
 * @param LocalUserNum the player to watch login status changes for
 */
function ClearLoginStatusChangeDelegate(delegate<OnLoginStatusChange> LoginStatusDelegate,byte LocalUserNum);

/**
 * Delegate used when the friends read request has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnReadFriendsComplete(bool bWasSuccessful);

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
function bool ReadFriendsList(byte LocalUserNum,optional int Count,optional int StartingAt)
{
	local int Index;
	local delegate<OnReadFriendsComplete> CallDelegate;

	// must always call the delegate
	for (Index=0; Index < ReadFriendsDelegates.Length; Index++)
	{
		CallDelegate = ReadFriendsDelegates[Index];
		if (CallDelegate != None)
		{
			CallDelegate(false);
		}
	}
	return false;
}

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
function EOnlineEnumerationReadState GetFriendsList(byte LocalUserNum,out array<OnlineFriend> Friends,optional int Count,optional int StartingAt);

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
function bool IsLocalPlayerTalking(byte LocalUserNum);

/**
 * Determines if the specified remote player is actively talking into the mic
 * NOTE: Network latencies will make this not 100% accurate
 *
 * @param PlayerId the unique id of the remote player being queried
 *
 * @return TRUE if the player is talking, FALSE otherwise
 */
function bool IsRemotePlayerTalking(UniqueNetId PlayerId);

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
function bool MuteRemoteTalker(byte LocalUserNum,UniqueNetId PlayerId,optional bool bIsSystemWide);

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
function bool UnmuteRemoteTalker(byte LocalUserNum,UniqueNetId PlayerId,optional bool bIsSystemWide);

/**
 * Tells the voice layer that networked processing of the voice data is allowed
 * for the specified player. This allows for push-to-talk style voice communication
 *
 * @param LocalUserNum the local user to allow network transimission for
 */
function StartNetworkedVoice(byte LocalUserNum);

/**
 * Tells the voice layer to stop processing networked voice support for the
 * specified player. This allows for push-to-talk style voice communication
 *
 * @param LocalUserNum the local user to disallow network transimission for
 */
function StopNetworkedVoice(byte LocalUserNum);

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
function bool ReadOnlineStats(const out array<UniqueNetId> Players,OnlineStatsRead StatsRead);

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
function bool ReadOnlineStatsForFriends(byte LocalUserNum,OnlineStatsRead StatsRead);

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
function bool ReadOnlineStatsByRank(OnlineStatsRead StatsRead,optional int StartIndex = 1,optional int NumToRead = 100);

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
function bool ReadOnlineStatsByRankAroundPlayer(byte LocalUserNum,OnlineStatsRead StatsRead,optional int NumRows = 10);

/**
 * Adds the delegate to a list used to notify the gameplay code that the stats read has completed
 *
 * @param ReadOnlineStatsCompleteDelegate the delegate to use for notifications
 */
function AddReadOnlineStatsCompleteDelegate(delegate<OnReadOnlineStatsComplete> ReadOnlineStatsCompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param ReadOnlineStatsCompleteDelegate the delegate to use for notifications
 */
function ClearReadOnlineStatsCompleteDelegate(delegate<OnReadOnlineStatsComplete> ReadOnlineStatsCompleteDelegate);

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
function bool WriteOnlineStats(name SessionName,UniqueNetId Player,OnlineStatsWrite StatsWrite);

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
function bool UnlockAchievement(byte LocalUserNum,int AchievementId,float PercentComplete=100.0);

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
function AddUnlockAchievementCompleteDelegate(byte LocalUserNum,delegate<OnUnlockAchievementComplete> UnlockAchievementCompleteDelegate);

/**
 * Clears the delegate used to notify the gameplay code that the achievement unlocking has completed
 *
 * @param LocalUserNum which user to watch for read complete notifications
 * @param UnlockAchievementCompleteDelegate the delegate to use for notifications
 */
function ClearUnlockAchievementCompleteDelegate(byte LocalUserNum,delegate<OnUnlockAchievementComplete> UnlockAchievementCompleteDelegate);

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
function bool ReadAchievements(byte LocalUserNum,optional int TitleId = 0,optional bool bShouldReadText = true,optional bool bShouldReadImages = false);

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
function AddReadAchievementsCompleteDelegate(byte LocalUserNum,delegate<OnReadAchievementsComplete> ReadAchievementsCompleteDelegate);

/**
 * Clears the delegate used to notify the gameplay code that the achievements read request has completed
 *
 * @param LocalUserNum the user to read the achievements list for
 * @param ReadAchievementsCompleteDelegate the delegate to use for notifications
 */
function ClearReadAchievementsCompleteDelegate(byte LocalUserNum,delegate<OnReadAchievementsComplete> ReadAchievementsCompleteDelegate);

/**
 * Copies the list of achievements for the specified player and title id
 *
 * @param LocalUserNum the user to read the friends list of
 * @param Achievements the out array that receives the copied data
 * @param TitleId the title id of the game that these were read for
 *
 * @return OERS_Done if the read has completed, otherwise one of the other states
 */
function EOnlineEnumerationReadState GetAchievements(byte LocalUserNum,out array<AchievementDetails> Achievements,optional int TitleId = 0);

defaultproperties
{
	LoggedInPlayerName="Player1"
}