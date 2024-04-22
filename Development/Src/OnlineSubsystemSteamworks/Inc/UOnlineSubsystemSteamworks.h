/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

// @todo Steam: Fix up the documentation/comments for all of the below; does not need to follow the multiline format, but must describe all functions
//			(and within the .cpp, their parameters/returns)

#if WITH_UE3_NETWORKING && WITH_STEAMWORKS

	/** Cleanup stuff that happens outside of uobject's view. */
	virtual void FinishDestroy();

	/** Initializes Steamworks */
	UBOOL InitSteamworks();

	/** Notification sent to OnlineSubsystem, that pre-travel cleanup is occuring */
	void NotifyCleanupWorld(UBOOL bSessionEnded);

	/** Handles updating of any async tasks that need to be performed */
	void Tick(FLOAT DeltaTime);

	/** Ticks the connection checking code */
	void TickConnectionStatusChange(FLOAT DeltaTime);

	/** Allows the Steamworks code to perform their Think operations */
	void TickSteamworksTasks(FLOAT DeltaTime);

	/** Logs the player into the default account */
	void SignInLocally();

	/** Registers all of the local talkers with the voice engine */
	void RegisterLocalTalkers();

	/** Unregisters all of the local talkers from the voice engine */
	void UnregisterLocalTalkers();

	/** Just empties the mute list and remote talker list */
	void UnregisterRemoteTalkers()
	{
		RemoteTalkers.Empty();
		MuteList.Empty();
	}

	/** Finds a remote talker in the cached list */
	FRemoteTalker* FindRemoteTalker(FUniqueNetId UniqueId);

	/** Handles updating of any async tasks that need to be performed */
	void TickVoice(FLOAT DeltaTime);

	/** Reads any data that is currently queued in the voice interface */
	void ProcessLocalVoicePackets();

	/** Submits network packets to the voice interface for playback */
	void ProcessRemoteVoicePackets();

	/** Processes any talking delegates that need to be fired off */
	void ProcessTalkingDelegates();

	/** Processes any speech recognition delegates that need to be fired off */
	void ProcessSpeechRecognitionDelegates();

	/** Clears the various data that is associated with a player to prevent the data being used across logins */
	void ClearPlayerInfo();

	/** Determines whether the user's profile file exists or not */	
	UBOOL DoesProfileExist();

	/** Return Steam cloud filename for player profile. */
	FString CreateProfileName();

	/** 
     * **INTERNAL**
     * Get the metadata related to a given user's file on Steam
     * This information is only available after calling EnumerateUserFiles
     *
     * @param UserId the UserId owning the file to search for
     * @param Filename the file to get metadata about
     * @return the struct with the metadata about the requested file, NULL otherwise
     *
     */
	FEmsFile* GetUserCloudMetadataFile(const FString& UserId, const FString& Filename);

    /** 
     * **INTERNAL**
     * Get the metadata related to a given user
     * This information is only available after calling EnumerateUserFiles
     *
     * @param UserId the UserId to search for
     * @return the struct with the metadata about the requested user, will always return a valid struct, creating one if necessary
     *
     */
	FSteamUserCloudMetadata* GetUserCloudMetadata(const FString& UserId);

	/** 
     * **INTERNAL**
     * Clear the metadata related to a given user's file on Steam
     * This information is only available after calling EnumerateUserFiles
     * It doesn't actually delete any of the actual data on disk
     *
     * @param UserId the UserId for the file to search for
     * @param Filename the file to get metadata about
     * @return the true if the delete was successful, false otherwise
     *
     */
	UBOOL ClearUserCloudMetadata(const FString& UserId, const FString& Filename);

	/**
     * **INTERNAL**
	 * Get physical/logical file information for a given user's cloud file
     *
	 * @param UserID the UserId owning the file to search for
	 * @param Filename the file to search for
	 * @return the file details, NULL otherwise
     *
	 */
	FTitleFile* GetUserCloudDataFile(const FString& UserId, const FString& Filename);

    /** 
     * **INTERNAL**
	 * Get physical/logical file information for all given user's cloud files
     * This information is only available after calling EnumerateUserFiles
     *
     * @param UserId the UserId for the file to search for
     * @return the struct with the metadata about the requested user, will always return a valid struct, creating one if necessary
     *
     */
	FSteamUserCloud* GetUserCloudData(const FString& UserId);

	/** 
     * **INTERNAL**
     * Get the file entry related to a shared download
     *
     * @param SharedHandle the handle to search for
     * @return the struct with the metadata about the requested shared data, will always return a valid struct, creating one if necessary
     *
     */
	FTitleFile* GetSharedCloudFile(const FString& SharedHandle);

	/** Searches for a player's pending stats, returning them if they exist, or adding them if they don't */
	FPendingPlayerStats& FindOrAddPendingPlayerStats(const FUniqueNetId& Player);

	/** Get the Steamworks stat field string for a given view/column */
	FString GetStatsFieldName(INT ViewId, INT ColumnId);

	/** Refresh data in pending stats */
	void AddOrUpdatePlayerStat(TArray<FPlayerStat>& PlayerStats, INT ViewId, INT ColumnId, const FSettingsData& Data);

	/** Called when ready to submit all collected stats */
	UBOOL CreateAndSubmitStatsReport();

	/** Returns TRUE if stats are enabled for this session, FALSE otherwise */
	inline UBOOL SessionHasStats(void)
	{
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

		// Clients can always write stats, they are not locked to an active game session (they are, however, locked to client-only stats)
		if (WorldInfo != NULL && (WorldInfo->NetMode == NM_Standalone || WorldInfo->NetMode == NM_Client))
		{
			return TRUE;
		}

		return CachedGameInt->GameWantsStats() && bIsStatsSessionOk;
	}

	/** Handle actual downloading of avatars from Steam. */
	UBOOL GetOnlineAvatar(const struct FUniqueNetId PlayerNetId,const INT Size,FScriptDelegate &ReadOnlineAvatarCompleteDelegate,const UBOOL bTriggerOnFailure);

	/** Sets up the specified leaderboard, so that read/write calls can be performed on it */
	UBOOL InitiateLeaderboard(const FString& LeaderboardName);

	/** Reads entries from the specified leaderboard */
	UBOOL ReadLeaderboardEntries(const FString& LeaderboardName, BYTE RequestType=LRT_Global, INT Start=0, INT End=0,
								 const TArray<FUniqueNetId>* PlayerList=NULL);

	/** Writes out the leaderboard score, for the currently logged in player */
	UBOOL WriteLeaderboardScore(const FString& LeaderboardName, INT Score, const TArray<INT>& LeaderboardData);

	/** Takes a stats ViewId and matches it up to a leaderboard name */
	FString LeaderboardNameLookup(INT ViewId);

	/** Refresh pending achievement progress updates */
	void AddOrUpdateAchievementStat(const FAchievementMappingInfo& Ach, const FSettingsData& Data);

	/** Refresh pending leaderboard stats */
	void AddOrUpdateLeaderboardStat(FString LeaderboardName, INT ColumnId, const FSettingsData& Data);

	/** Refresh pending leaderboard rank */
	void AddOrUpdateLeaderboardRank(FString LeaderboardName, const FSettingsData& Data);

	/** Wipe pending leaderboard data */
	void ClearPendingLeaderboardData(FString LeaderboardName);

	/** Called internally by clients and listen hosts, to advertise the current server IP/UID so friends can join */
	UBOOL SetGameJoinInfo(DWORD ServerIP, INT ServerPort, QWORD ServerUID, UBOOL bSteamSockets);

	/** Called internally by clients and listen hosts, to clear advertising the current server info */
	UBOOL ClearGameJoinInfo();

	/** Tries to match up the IP on a game invite, to a friend UID and the steam sockets address of the server (if applicable) */
	UBOOL GetInviteFriend(FInternetIpAddr ServerAddr, QWORD& OutFriendUID, QWORD& OutSteamSocketsAddr);
#endif	// WITH_UE3_NETWORKING && WITH_STEAMWORKS


