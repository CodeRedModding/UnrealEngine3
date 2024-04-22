/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#if WITH_UE3_NETWORKING

public:
	/**
	 * Called when there was an error with the GP SDK
	 *
	 * @param Arg the error info
	 */
	void GPErrorCallback(GPErrorArg * Arg);

	/**
	 * Called when a buddy request is received
	 *
	 * @param Arg the request info
	 */
	void GPRecvBuddyRequestCallback(GPRecvBuddyRequestArg * Arg);

	/**
	 * Called when a buddy's status is received
	 *
	 * @param Arg the status info
	 */
	void GPRecvBuddyStatusCallback(GPRecvBuddyStatusArg * Arg);

	/**
	 * Called when a buddy message is received
	 *
	 * @param Arg the message info
	 */
	void GPRecvBuddyMessageCallback(GPRecvBuddyMessageArg * Arg);

	/**
	 * Called when a buddy request has been authorized
	 *
	 * @param Arg the auth info
	 */
	void GPRecvBuddyAuthCallback(GPRecvBuddyAuthArg * Arg);

	/**
	 * Called when a buddy has revoked your friendship
	 *
	 * @param Arg the revoke info
	 */
	void GPRecvBuddyRevokeCallback(GPRecvBuddyRevokeArg * Arg);

	/**
	 * Called at the end of a connection attempt
	 *
	 * @param Arg the connection result
	 */
	void GPConnectCallback(GPConnectResponseArg* Arg);

	/**
	 * Called at the end of a new user attempt
	 *
	 * @param Arg the new user result
	 */
	void GPNewUserCallback(GPNewUserResponseArg* Arg);

	/**
	 * Called in response to a request for a remote profile's info
	 *
	 * @param Arg the profile's info
	 */
	void GPGetInfoCallback(GPGetInfoResponseArg* Arg);

	/**
	 * Called in response to a request for a stats player's nick
	 *
	 * @param Arg the player's info
	 */
	void GPGetStatsNickCallback(GPGetInfoResponseArg* Arg);

	/**
	 * Called after requesting the profile from the Sake DB
	 *
	 * @param Request a handle to the request
	 * @param Result the result of the request
	 * @param Input the data that was sent along with the request
	 * @param Output the data that was returned with the response
	 */
	void SakeRequestProfileCallback(SAKERequest Request, SAKERequestResult Result, SAKEGetMyRecordsInput* Input, SAKEGetMyRecordsOutput* Output);

	/**
	 * Called after creating a profile record in the Sake DB
	 *
	 * @param Request a handle to the request
	 * @param Result the result of the request
	 * @param Input the data that was sent along with the request
	 * @param Output the data that was returned with the response
	 */
	void SakeCreateProfileCallback(SAKERequest Request, SAKERequestResult Result, SAKECreateRecordInput* Input, SAKECreateRecordOutput* Output);

	/**
	 * Called after updating the profile stored in the Sake DB
	 *
	 * @param Request a handle to the request
	 * @param Result the result of the request
	 * @param Input the data that was sent along with the request
	 */
	void SakeUpdateProfileCallback(SAKERequest Request, SAKERequestResult Result, SAKEUpdateRecordInput* Input);

	/**
	 * Called after searching for records in the Sake DB
	 *
	 * @param Request a handle to the request
	 * @param Result the result of the request
	 * @param Input the data that was sent along with the request
	 * @param Output the data that was returned with the response
	 */
	void SakeSearchForRecordsCallback(SAKERequest Request, SAKERequestResult Result, SAKESearchForRecordsInput* Input, SAKESearchForRecordsOutput* Output);

	/**
	 * Called after requesting a Sake table record count
	 *
	 * @param Request a handle to the request
	 * @param Result the result of the request
	 * @param Input the data that was sent along with the request
	 * @param Output the data that was returned with the response
	 */
	void SakeGetRecordCountCallback(SAKERequest Request, SAKERequestResult Result, SAKEGetRecordCountInput* Input, SAKEGetRecordCountOutput* Output);

	/**
	 * Called after a login certificate request
	 *
	 * @param HttpResult the result of the http request
	 * @param Response the response to the request
	 */
	void LoginCertificateCallback(GHTTPResult HttpResult, WSLoginResponse * Response);

	/**
	 * Called after a PS3 login certificate request
	 *
	 * @param HttpResult the result of the http request
	 * @param Response the response to the request
	 */
	void PS3LoginCertificateCallback(GHTTPResult HttpResult, WSLoginPs3CertResponse * Response);

	/**
	 * Called after an attempt to create a session
	 *
	 * @param HttpResult the result of the http request
	 * @param Result the result of the create session attempt
	 */
	void CreateStatsSessionCallback(GHTTPResult HttpResult, SCResult Result);

	/**
	 * Called after the host sets his report intention
	 *
	 * @param HttpResult the result of the http request
	 * @param Result the result of the set report intention
	 */
	void HostSetStatsReportIntentionCallback(GHTTPResult HttpResult, SCResult Result);

	/**
	 * Called after the client sets his report intention
	 *
	 * @param HttpResult the result of the http request
	 * @param Result the result of the set report intention
	 */
	void ClientSetStatsReportIntentionCallback(GHTTPResult HttpResult, SCResult Result);

	/**
	 * Called after the host submits a report
	 *
	 * @param HttpResult the result of the http request
	 * @param Result the result of the report submission
	 */
	void SubmitStatsReportCallback(GHTTPResult HttpResult, SCResult Result);

	/**
	 * Called at the end of a profile search
	 *
	 * @param Arg the search result
	 */
	void GPProfileSearchCallback(GPProfileSearchResponseArg* Arg);

	/**
	 * Initiates a stats session
	 * Only called by the host
	 * Will result in the CreateStatsSessionCallback being called
	 */
	void CreateStatsSession(void);

	/**
	 * Called when a game invite was received
	 *
	 * @param Arg the game invite data
	 */
	void GPRecvGameInviteCallback(GPRecvGameInviteArg* Arg);

	/**
	 * Called when GameSpy has determined if a player is authorized to play
	 *
	 * @param GameId the game id that the auth request was for
	 * @param LocalId the id the game assigned the user when requesting auth
	 * @param Authenticated whether the player was authenticated or should be rejected
	 * @param ErrorMsg a string describing the error if there was one
	 */
	void CDKeyAuthCallBack(INT GameId,INT LocalId,INT Authenticated,const TCHAR* ErrorMsg);

	/**
	 * Called when GameSpy has wants verify if a player is still playing on a host
	 *
	 * @param GameId the game id that the auth request was for
	 * @param LocalId the id the game assigned the user when requesting auth
	 * @param Hint the session id that should be passed to the reauth call
	 * @param Challenge a string describing the error if there was one
	 */
	void RefreshCDKeyAuthCallBack(INT GameId,INT LocalId,INT Hint,const TCHAR* Challenge);

	/** Returns TRUE if stats are enabled for this session, FALSE otherwise */
	inline UBOOL SessionHasStats(void)
	{
		return CachedGameInt->GameWantsStats() &&
			LoginCertificate != NULL &&
			LoginPrivateData != NULL &&
			SCHandle != NULL &&
			bIsStatsSessionOk;
	}

	/** Registers all of the local talkers with the voice engine */
	void RegisterLocalTalkers(void);

	/** Unregisters all of the local talkers from the voice engine */
	void UnregisterLocalTalkers(void);

	/**
	 * Based upon logged in status, decides if it needs to write to Sake or not.
	 * Called after local writing has completed. If writing remote, it starts the
	 * write. Otherwise it fires the delegates as if it is done
	 *
	 * @param Result the result code from the write
	 * @param Profile the profile data to write
	 * @param Length the number of bytes of data to write
	 */
	void ConditionallyWriteProfileToSake(DWORD Result,const BYTE* Profile,DWORD Length);

	/**
	 * Calculates a response string based off of the server's challenge
	 *
	 * @param Connection the connection that we are generating the response for
	 * @param bIsReauth whether we are processing a reauth or initial auth request
	 */
	void GetChallengeResponse(UNetConnection* Connection,UBOOL bIsReauth);

	/**
	 * Sends an authorization request to GameSpy's backend
	 *
	 * @param Connection the connection that we are authorizing
	 */
	void SubmitAuthRequest(UNetConnection* Connection);

	/**
	 * Used to respond to reauth a request from GameSpy. Used to see if a user is still
	 * using a given CD key
	 *
	 * @param Connection the connection to authenticate
	 * @param Hint the hint that GameSpy sent us
	 */
	void SubmitReauthResponse(UNetConnection* Connection,INT Hint);

	/** Initiates an auth request for the host of the session */
	UBOOL CheckServerProductKey(void);

	/** Just empties the mute list and remote talker list */
	void UnregisterRemoteTalkers(void)
	{
		RemoteTalkers.Empty();
		MuteList.Empty();
	}

protected:
	/** Initializes GameSpy */
	UBOOL InitGameSpy(void);

	/**
	 * Handles updating of any async tasks that need to be performed
	 *
	 * @param DeltaTime the amount of time that has passed since the last tick
	 */
	virtual void Tick(FLOAT DeltaTime);

	/**
	 * Checks any queued async tasks for status, allows the task a change
	 * to process the results, and fires off their delegates if the task is
	 * complete
	 *
	 * @param DeltaTime the amount of time that has passed since the last tick
	 */
	void TickAsyncTasks(FLOAT DeltaTime);

	/** Allows the GameSpy code to perform their Think operations */
	void TickGameSpyTasks(void);

	/**
	 * Ticks the connection checking code
	 *
	 * @param DeltaTime the amount of time that has elapsed since the list check
	 */
	void TickConnectionStatusChange(FLOAT DeltaTime);

#if PS3
	/** Updates the controller state and fires any delegates */
	void TickControllers(void);
#endif

	/**
	 * Handles updating of any async tasks that need to be performed
	 *
	 * @param DeltaTime the amount of time that has passed since the last tick
	 */
	void TickVoice(FLOAT DeltaTime);

	/** Ticks the keyboard UI task if open */
	void TickKeyboardUI(void);

	/**
	 * Determines whether the user's profile file exists or not
	 */
	UBOOL DoesProfileExist(void);

	/**
	 * Builds a file name for the user's profile data
	 */
	inline FString CreateProfileName(void)
	{
#if PS3
		return TEXT("PROFILE\\Profile.bin");
#endif
		FString FilteredName = LoggedInPlayerName;
		// Iterate through and replace bad characters
		for (INT Index = 0; Index < FilteredName.Len(); Index++)
		{
			if (appIsAlnum(FilteredName[Index]) == FALSE)
			{
				FilteredName.GetCharArray()(Index) = TEXT('_');
			}
		}
		// Use the player nick name to generate a profile name
		return ProfileDataDirectory * FilteredName + ProfileDataExtension;
	}

	/**
	 * Sends off a request for the profile from Sake
	 * This will result in the SakeRequestProfileCallback being called
	 */
	void SakeRequestProfile(void);

	/**
	 * Creates a profile in Sake
	 *
	 * @param Buffer a serialized version of the profile
	 * @param Length the number of bytes in the buffer
	 */
	void SakeCreateProfile(const BYTE* Buffer, DWORD Length);

	/**
	 * Updates a profile in Sake
	 *
	 * @param Buffer a serialized version of the profile
	 * @param Length the number of bytes in the buffer
	 *
	 * @return TRUE if the update request happened ok, FALSE otherwise
	 */
	UBOOL SakeUpdateProfile(const BYTE* Buffer, DWORD Length);

	/**
	 * Checks if we have received all the info from a ReadOnlineStats request
	 * If so, it calls the delegate, then cleans up the request
	 */
	void TryToCompleteReadOnlineStatsRequest(void);

	/**
	 * Does the common setup for a ReadOnlineStats request
	 *
	 * @param StatsRead holds the definitions of the tables to read the data from and
	 *		  results are copied into the specified object
	 * @param SearchInput gets allocated internally, then filled with the common request info
	 *
	 * @return TRUE if the call is successful, FALSE otherwise
	 */
	UBOOL SetupReadOnlineStatsRequest(UOnlineStatsRead* StatsRead, SAKESearchForRecordsInput*& SearchInput);

	/**
	 * Sends a ReadOnlineStats request
	 *
	 * @param SearchInput points to the request input
	 *
	 * @return TRUE if the call is successful, FALSE otherwise
	 */
	UBOOL SendReadOnlineStatsRequest(SAKESearchForRecordsInput* SearchInput);

	/**
	 * Called by the host after creating a stats session
	 * Will result in the HostSetStatsReportIntentionCallback being called
	 */
	UBOOL HostSetStatsReportIntention(void);

	/**
	 * Called by the client after receiving the host's session id
	 * Will result in the ClientSetStatsReportIntentionCallback being called
	 *
	 * @param SessionId the session id received from the host
	 */
	UBOOL ClientSetStatsReportIntention(const gsi_u8 SessionId[SC_SESSION_GUID_SIZE]);

	/**
	 * For each player in the pending stats array, it adds them and their stats to
	 * the report
	 *
	 * @param ReportHandle the stats report handle to write to
	 * @param bNoTeams whether there are teams or not
	 *
	 * @return the success/error code
	 */
	DWORD AppendPlayersToStatsReport(SCReportPtr ReportHandle,UBOOL bNoTeams);

	/**
	 * Appends each stat in a players stats array to the report
	 *
	 * @param ReportHandle the stats report handle to write to
	 * @param Stats the stats to append to the report
	 */
	void AppendPlayerStatsToStatsReport(SCReportPtr ReportHandle,const TArrayNoInit<FPlayerStat>& Stats);

	/**
	 * Called when ready to submit all collected stats
	 * Only called by the host
	 * The host needs to have received the ConnectionIds from all the players before submitting the report
	 *
	 * @param Players the players for which to submit stats
	 * @param StatsWrites the stats to submit for the players
	 * @param PlayerScores the scores for the players
	 */
	UBOOL CreateAndSubmitStatsReport(void);

	/**
	 * Sorts players by their score and updates their "place" (1st, 2nd, etc.)
	 */
	void PreprocessPlayersByScore(void);

	/**
	 * Counts the number of teams that players were on
	 */
	INT GetTeamCount(void);

	/**
	 * Called to get the GameSpy stats key id based on a view and property
	 *
	 * @param ViewId the stat view
	 * @param PropertyId the stat property
	 *
	 * @return the gamespy stat key id
	 */
	INT StatKeyLookup(INT ViewId, INT PropertyId);

	void AddOrUpdatePlayerStat(TArray<FPlayerStat>& PlayerStats, INT KeyId, const FSettingsData& Data);

	/**
	 * Searches for a player's pending stats, returning them if they exist, or adding them if they don't
	 *
	 * @param Player the player to find/add stats for
	 *
	 * @return the existing/new stats for the player
	 */
	FPendingPlayerStats& FindOrAddPendingPlayerStats(const FUniqueNetId& Player);

	/**
	 * Clears the various data that is associated with a player to prevent the
	 * data being used across logins
	 */
	inline void ClearPlayerInfo(void)
	{
#if !PS3
		CachedProfile = NULL;
#endif
		LoggedInPlayerId = (QWORD)0;
		LoggedInStatus = LS_NotLoggedIn;
		LoggedInPlayerName.Empty();
		CachedFriendMessages.Empty();
		CachedGameInt->SetInviteInfo(NULL);
//@todo joeg -- add more as they come up
	}

	/**
	 * Performs a default sign in for non-networked profiles
	 */
	void SignInLocally(void);

	/**
	 * Finds a remote talker in the cached list
	 *
	 * @param UniqueId the net id of the player to search for
	 *
	 * @return pointer to the remote talker or NULL if not found
	 */
	inline FRemoteTalker* FindRemoteTalker(FUniqueNetId UniqueId)
	{
		for (INT Index = 0; Index < RemoteTalkers.Num(); Index++)
		{
			FRemoteTalker& Talker = RemoteTalkers(Index);
			// Compare net ids to see if they match
			if ((QWORD&)Talker.TalkerId == (QWORD&)UniqueId)
			{
				return &RemoteTalkers(Index);
			}
		}
		return NULL;
	}

	/**
	 * Processes any talking delegates that need to be fired off
	 */
	void ProcessTalkingDelegates(void);

	/**
	 * Processes any speech recognition delegates that need to be fired off
	 */
	void ProcessSpeechRecognitionDelegates(void);

	/** Builds the GameSpy location string from the game the player is connected to */
	FString GetServerLocation(void) const;

	/**
	 * Searches the cached invite array looking for the sender
	 *
	 * @param Sender the inviter to find
	 *
	 * @return the friend invite if found, NULL if not found
	 */
	inline FOnlineFriendMessage* FindFriendInvite(FUniqueNetId Sender)
	{
		for (INT Index = 0; Index < CachedFriendMessages.Num(); Index++)
		{
			if ((QWORD&)CachedFriendMessages(Index).SendingPlayerId == (QWORD&)Sender)
			{
				return &CachedFriendMessages(Index);
			}
		}
		return NULL;
	}

	/**
	 * Determines if the location string is joinable for this game
	 *
	 * @param LocationString the string that is being parsed
	 *
	 * @return TRUE if it is joinable, FALSE otherwise
	 */
	inline UBOOL IsJoinableLocationString(const TCHAR* LocationString) const
	{
		// There can be multiple URLs because of different platforms, so check all
		for (INT Index = 0; Index < LocationUrlsForInvites.Num(); Index++)
		{
			const FString& JoinableUrl = LocationUrlsForInvites(Index);
			// If this invite came from a game we can join
			if (appStrnicmp(LocationString,*JoinableUrl,JoinableUrl.Len()) == 0)
			{
				return TRUE;
			}
		}
		return FALSE;
	}

	/**
	 * Reads any data that is currently queued in the voice interface
	 */
	void ProcessLocalVoicePackets(void);

	/**
	 * Submits network packets to the voice interface for playback
	 */
	void ProcessRemoteVoicePackets(void);

#if PS3
	/** Initializes the NP libs */
	void InitNp(void);

	/** Ticks the NP libs */
	void TickNp(FLOAT DeltaTime);
#endif

	/** Returns the partner id to use */
	inline INT GetPartnerId(void)
	{
#if PS3
	#if FINAL_RELEASE && USE_PRODUCTION_PS3_SERVERS
		return 19;
	#else
		return 33;
	#endif
#else
	#if SHIPPING_PC_GAME
		return PartnerID;
	#else
		return 0;
	#endif
#endif
	}

	/** Returns the namespace id to use */
	inline INT GetNamespaceId(void)
	{
#if PS3
	#if FINAL_RELEASE && USE_PRODUCTION_PS3_SERVERS
		return 28;
	#else
		return 40;
	#endif
#else
	#if SHIPPING_PC_GAME
		return NamespaceID;
	#else
		return 1;
	#endif
#endif
	}

	/**
	 * Decrypts the product key and places it in the specified buffer
	 *
	 * @param Buffer the output buffer that holds the key
	 * @param BufferLen the size of the buffer to fill
	 *
	 * @return TRUE if the decyption was successful, FALSE otherwise
	 */
	UBOOL DecryptProductKey(BYTE* Buffer,DWORD BufferLen);

	/**
	 * Searches the named session array for the specified session and removes it
	 *
	 * @param SessionName the name to search for
	 */
	virtual void RemoveNamedSession(FName SessionName)
	{
		for (INT SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
		{
			if (Sessions(SearchIndex).SessionName == SessionName)
			{
				FNamedSession* Session = &Sessions(SearchIndex);
#if PS3
				// Release platform specific version session info
				FSessionInfoPS3* PS3SessionInfo = (FSessionInfoPS3*) Session->SessionInfo;
				delete PS3SessionInfo;
#else
				// Release platform specific version session info
				delete Session->SessionInfo;
#endif
				if (Session->GameSettings)
				{
					// Mark the session as not valid
					Session->GameSettings->GameState = OGS_NoSession;
				}
				Sessions.Remove(SearchIndex);
				return;
			}
		}
	}

public:

#endif	//#if WITH_UE3_NETWORKING
