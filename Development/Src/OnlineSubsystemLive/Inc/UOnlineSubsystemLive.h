/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#if WITH_UE3_NETWORKING

protected:
	/** Processes any invites that couldn't be handled because there were no player controllers */
	inline void TickDelayedInvites(void)
	{
		if (DelayedInviteUserMask)
		{
			// Check each delayed user flag
			for (DWORD UserIndex = 0; UserIndex < 4; UserIndex++)
			{
				if (((1 << UserIndex) & DelayedInviteUserMask) &&
					ContentCache[UserIndex].ReadState > OERS_InProgress)
				{
					// Attempt to delive the invite and requeue if can't
					ProcessGameInvite(UserIndex);
				}
			}
		}
	}

	/**
	 * Processes a notification that was returned during polling
	 *
	 * @param Notification the notification event that was fired
	 * @param Data the notification specifc data
	 */
	virtual void ProcessNotification(DWORD Notification,ULONG_PTR Data);

	/**
	 * Handles any sign in change processing (firing delegates, etc)
	 *
	 * @param Data the mask of changed sign ins
	 */
	void ProcessSignInNotification(ULONG_PTR Data);

	/**
	 * Checks for new Live notifications and passes them out to registered delegates
	 *
	 * @param DeltaTime the amount of time that has passed since the last tick
	 */
	virtual void Tick(FLOAT DeltaTime);

	/**
	 * Creates the session flags value from the game settings object
	 *
	 * @param GameSettings the game settings of the new session
	 *
	 * @return the flags needed to set up the session
	 */
	DWORD BuildSessionFlags(UOnlineGameSettings* GameSettings);

	/**
	 * Sets the contexts and properties for this game settings object
	 *
	 * @param HostingPlayerNum the index of the player hosting the match
	 * @param GameSettings the game settings of the new session
	 */
	void SetContextsAndProperties(BYTE HostingPlayerNum,UOnlineGameSettings* GameSettings);

	/**
	 * Sets the list contexts for the player
	 *
	 * @param PlayerNum the index of the player hosting the match
	 * @param Contexts the list of contexts to set
	 */
	void SetContexts(BYTE PlayerNum,const TArray<FLocalizedStringSetting>& Contexts);

	/**
	 * Sets the list properties for the player
	 *
	 * @param PlayerNum the index of the player hosting the match
	 * @param Properties the list of properties to set
	 */
	void SetProperties(BYTE PlayerNum,const TArray<FSettingsProperty>& Properties);

	/**
	 * Reads the contexts and properties from the Live search data and populates the
	 * game settings object with them
	 *
	 * @param SearchResult the data that was returned from Live
	 * @param GameSettings the game settings that we are setting the data on
	 */
	void ParseContextsAndProperties(XSESSION_SEARCHRESULT& SearchResult,UOnlineGameSettings* GameSettings);

	/**
	 * Allocates the space/structure needed for holding the search results plus
	 * any resources needed for async support
	 *
	 * @param SearchingPlayerNum the index of the player searching for the match
	 * @param QueryNum the unique id of the query to be run
	 * @param MaxSearchResults the maximum number of search results we want
	 * @param NumBytes the out param indicating the size that was allocated
	 *
	 * @return The data allocated for the search (space plus overlapped)
	 */
	FLiveAsyncTaskDataSearch* AllocateSearch(BYTE SearchingPlayerNum,DWORD QueryNum,DWORD MaxSearchResults,DWORD& NumBytes);

	/**
	 * Copies the Epic structures into the Live equivalent
	 *
	 * @param DestProps the destination properties
	 * @param SourceProps the source properties
	 *
	 * @return the number of items copied
	 */
	DWORD CopyPropertiesForSearch(PXUSER_PROPERTY DestProps,const TArray<FSettingsProperty>& SourceProps);

	/**
	 * Copies the Epic structures into the Live equivalent
	 *
	 * @param Search the object to use when determining
	 * @param DestContexts the destination contexts
	 * @param SourceContexts the source contexts
	 *
	 * @return the number of items copied (handles skipping for wildcards)
	 */
	DWORD CopyContextsForSearch(UOnlineGameSearch* Search,PXUSER_CONTEXT DestContexts,const TArray<FLocalizedStringSetting>& SourceContexts);

	/**
	 * Copies Unreal data to Live structures for the Live property writes
	 *
	 * @param Profile the profile object to copy the data from
	 * @param LiveData the Live data structures to copy the data to
	 */
	void CopyLiveProfileSettings(UOnlineProfileSettings* Profile,PXUSER_PROFILE_SETTING LiveData);

	/**
	 * Copies the stats data from our Epic form to something Live can handle
	 *
	 * @param Stats the destination buffer the stats are written to
	 * @param Properties the Epic structures that need copying over
	 * @param RatingId the id to set as the rating for this leaderboard set
	 */
	void CopyStatsToProperties(XUSER_PROPERTY* Stats,const TArray<FSettingsProperty>& Properties,const DWORD RatingId);

	/**
	 * Builds the data that we want to read into the Live specific format. Live
	 * uses WORDs which script doesn't support, so we can't map directly to it
	 *
	 * @param DestSpecs the destination stat specs to fill in
	 * @param ViewId the view id that is to be used
	 * @param Columns the columns that are being requested
	 */
	void BuildStatsSpecs(XUSER_STATS_SPEC* DestSpecs,INT ViewId,const TArrayNoInit<INT>& Columns);

	/**
	 * Creates a new Live enabled game for the requesting player using the
	 * settings specified in the game settings object
	 *
	 * @param HostingPlayerNum the player hosting the game
	 * @param Session the named session for this online game
	 *
	 * @return The result from the Live APIs
	 */
	DWORD CreateLiveGame(BYTE HostingPlayerNum,FNamedSession* Session);

	/**
	 * Migrates an existing Live enabled game for the requesting player using the
	 * existing settings specified in the game settings object
	 *
	 * @param PlayerNum the new player hosting or joining the game
	 * @param Session the named session for this online game
	 * @param bIsHost whether migration is occurring on the new host or client joining
	 *
	 * @return The result from the Live APIs
	 */
	DWORD MigrateLiveGame(BYTE PlayerNum,FNamedSession* Session,UBOOL bIsHost);

	/**
	 * Tells the QoS to respond with a "go away" packet and includes our custom
	 * data. Prevents bandwidth from going to QoS probes
	 *
	 * @param Session the named session that is having qos disabled
	 *
	 * @return The success/error code of the operation
	 */
	DWORD DisableQoS(FNamedSession* Session);

	/**
	 * Tells the QoS thread to stop its listening process
	 *
	 * @param Session the named session that is being removed from qos
	 *
	 * @return The success/error code of the operation
	 */
	DWORD UnregisterQoS(FNamedSession* Session);

	/**
	 * Builds a Live game query and submits it to Live for processing
	 *
	 * @param SearchingPlayerNum the player searching for games
	 * @param SearchSettings the settings to search with
	 *
	 * @return The result from the Live APIs
	 */
	DWORD FindLiveGames(BYTE SearchingPlayerNum,UOnlineGameSearch* SearchSettings);

	/**
	 * Joins a Live game by creating the session without hosting it
	 *
	 * @param PlayerNum the player joining the game
	 * @param Session the named session the join is for
	 * @param bIsFromInvite whether this join is from a search or from an invite
	 *
	 * @return The result from the Live APIs
	 */
	DWORD JoinLiveGame(BYTE PlayerNum,FNamedSession* Session,UBOOL bIsFromInvite = FALSE);

	/**
	 * Terminates a Live session
	 *
	 * @param Session the named session that is being destroyed
	 *
	 * @return The result from the Live APIs
	 */
	DWORD DestroyLiveGame(FNamedSession* Session);

	/**
	 * Ticks voice subsystem for reading/submitting any voice data
	 *
	 * @param DeltaTime the time since the last tick
	 */
	void TickVoice(FLOAT DeltaTime);

	/**
	 * Reads any data that is currently queued in XHV
	 */
	void ProcessLocalVoicePackets(void);

	/**
	 * Submits network packets to XHV for playback
	 */
	void ProcessRemoteVoicePackets(void);

	/**
	 * Processes any talking delegates that need to be fired off
	 *
	 * @param DeltaTime the amount of time that has elapsed since the last tick
	 */
	void ProcessTalkingDelegates(FLOAT DeltaTime);

	/**
	 * Processes any speech recognition delegates that need to be fired off
	 */
	void ProcessSpeechRecognitionDelegates(void);

	/**
	 * Finds a remote talker in the cached list
	 *
	 * @param UniqueId the XUID of the player to search for
	 *
	 * @return pointer to the remote talker or NULL if not found
	 */
	inline FLiveRemoteTalker* FindRemoteTalker(FUniqueNetId UniqueId)
	{
		for (INT Index = 0; Index < RemoteTalkers.Num(); Index++)
		{
			FLiveRemoteTalker& Talker = RemoteTalkers(Index);
			// Compare XUIDs to see if they match
			if ((XUID&)Talker.TalkerId == (XUID&)UniqueId)
			{
				return &RemoteTalkers(Index);
			}
		}
		return NULL;
	}

	/**
	 * Re-evaluates the muting list for all local talkers
	 */
	void ProcessMuteChangeNotification(void);

	/**
	 * Registers/unregisters local talkers based upon login changes
	 */
	void UpdateVoiceFromLoginChange(void);

	/**
	 * Iterates the current remote talker list unregistering them with XHV
	 * and our internal state
	 */
	void RemoveAllRemoteTalkers(void);

	/**
	 * Registers all signed in local talkers
	 */
	void RegisterLocalTalkers(void);

	/**
	 * Unregisters all signed in local talkers
	 */
	void UnregisterLocalTalkers(void);

	/**
	 * Checks to see if a XUID is a local player or not. This is to avoid
	 * the extra processing that happens for remote players
	 *
	 * @param Player the player to check as being local
	 *
	 * @return TRUE if the player is local, FALSE otherwise
	 */
	inline UBOOL IsLocalPlayer(FUniqueNetId Player)
	{
		XUID Xuid;
		// Loop through the signins checking
		for (INT Index = 0; Index < 4; Index++)
		{
			// If the index has a XUID and it matches the talker, then they
			// are local
			if (GetUserXuid(Index,&Xuid) == ERROR_SUCCESS &&
				Xuid == (XUID&)Player)
			{
				return TRUE;
			}
		}
		// Remote talker
		return FALSE;
	}

	/**
	 * Handles accepting a game invite for the specified user
	 *
	 * @param UserNum the user that accepted the game invite
	 */
	void ProcessGameInvite(DWORD UserNum);

	/**
	 * Common method for joining a session by session id
	 *
	 * @param UserNum the user that is performing the search
	 * @param Inviter the user that is sending the invite (or following)
	 * @param SessionId the session id to join (from invite or friend presence)
	 */
	UBOOL HandleJoinBySessionId(DWORD UserNum,QWORD Inviter,QWORD SessionId);

	/**
	 * Handles notifying interested parties when a signin is cancelled
	 */
	void ProcessSignInCancelledNotification(void);

	/**
	 * Handles notifying interested parties when installed content changes
	 */
	void ProcessContentChangeNotification(void);

	/**
	 * Handles external UI change notifications
	 *
	 * @param bIsOpening whether the UI is opening or closing
	 */
	void ProcessExternalUINotification(UBOOL bIsOpening);

	/**
	 * Handles controller connection state changes
	 */
	void ProcessControllerNotification(void);

	/**
	 * Handles notifying interested parties when the Live connection status
	 * has changed
	 *
	 * @param Status the type of change that has happened
	 */
	void ProcessConnectionStatusNotification(HRESULT Status);

	/**
	 * Handles notifying interested parties when the link state changes
	 *
	 * @param bIsConnected whether the link has a connection or not
	 */
	void ProcessLinkStateNotification(UBOOL bIsConnected);

	/**
	 * Figures out which remote talkers need to be muted for a given local talker
	 *
	 * @param TalkerIndex the talker that needs the mute list checked for
	 * @param PlayerController the player controller associated with this talker
	 */
	void UpdateMuteListForLocalTalker(INT TalkerIndex,APlayerController* PlayerController);

	/**
	 * Handles notifying interested parties when the player changes profile data
	 *
	 * @param ChangeStatus bit flags indicating which user just changed status
	 */
	void ProcessProfileDataNotification(DWORD ChangeStatus);

	/**
	 * Processes a system link packet. For a host, responds to discovery
	 * requests. For a client, parses the discovery response and places
	 * the resultant data in the current search's search results array
	 *
	 * @param PacketData the packet data to parse
	 * @param PacketLength the amount of data that was received
	*/
	virtual void ProcessLanPacket(BYTE* PacketData,INT PacketLength);

	/**
	 * Creates a new system link enabled game. Registers the keys/nonce needed
	 * for secure communication
	 *
	 * @param HostingPlayerNum the player hosting the game
	 * @param Session the named session for this lan game
	 *
	 * @return The result code from the nonce/key APIs
	 */
	DWORD CreateLanGame(BYTE HostingPlayerNum,FNamedSession* Session);

	/**
	 * Terminates a LAN session. Unregisters the key for the secure connection
	 *
	 * @return an error/success code
	 */
	DWORD DestroyLanGame(void);

	/**
	 * Ticks the lan beacon for lan support
	 *
	 * @param DeltaTime the time since the last tick
	 */
	virtual void TickLanTasks(FLOAT DeltaTime);

	/**
	 * Determines if the packet header is valid or not
	 *
	 * @param Packet the packet data to check
	 * @param Length the size of the packet buffer
	 * @param ClientNonce out param that reads the client's nonce
	 *
	 * @return true if the header is valid, false otherwise
	 */
	UBOOL IsValidLanQueryPacket(const BYTE* Packet,DWORD Length,QWORD& ClientNonce);

	/**
	 * Determines if the packet header is valid or not
	 *
	 * @param Packet the packet data to check
	 * @param Length the size of the packet buffer
	 *
	 * @return true if the header is valid, false otherwise
	 */
	UBOOL IsValidLanResponsePacket(const BYTE* Packet,DWORD Length);

	/** Stops the lan beacon from accepting broadcasts */
	inline void StopLanBeacon(void)
	{
		// Don't poll anymore since we are shutting it down
		LanBeaconState = LANB_NotUsingLanBeacon;
		// Unbind the lan beacon object
		delete LanBeacon;
		LanBeacon = NULL;
	}

	/**
	 * Adds the game settings data to the packet that is sent by the host
	 * in reponse to a server query
	 *
	 * @param Packet the writer object that will encode the data
	 * @param GameSettings the game settings to add to the packet
	 */
	void AppendGameSettingsToPacket(FNboSerializeToBuffer& Packet,UOnlineGameSettings* GameSettings);

	/**
	 * Reads the game settings data from the packet and applies it to the
	 * specified object
	 *
	 * @param Packet the reader object that will read the data
	 * @param GameSettings the game settings to copy the data to
	 */
	void ReadGameSettingsFromPacket(FNboSerializeFromBuffer& Packet,UOnlineGameSettings* NewServer);

	/**
	 * Builds a LAN query and broadcasts it
	 *
	 * @return an error/success code
	 */
	DWORD FindLanGames(void);

#if !CONSOLE
	/**
	 * Initializes the G4W Live specific features
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	UBOOL InitG4WLive(void);
#endif

	/**
	 * Initializes the various sign in state and DLC state
	 */
	void InitLoginState(void);

	/**
	 * Reads the signin state for all players
	 *
	 * @param LoginState login state structs to fill in with the current state
	 */
	void ReadLoginState(FCachedLoginState LoginState[4]);

public:

	/**
	 * Parses the search results into something the game play code can handle
	 *
	 * @param Search the Unreal search object to fill in
	 * @param SearchResults the buffer filled by Live
	 */
	void ParseSearchResults(UOnlineGameSearch* Search,PXSESSION_SEARCHRESULT_HEADER SearchResults);

	/**
	 * Parses the read profile results into something the game play code can handle
	 *
	 * @param PlayerNum the number of the user being processed
	 * @param ReadResults the buffer filled by Live
	 */
	void ParseReadProfileResults(BYTE PlayerNum,PXUSER_READ_PROFILE_SETTING_RESULT ReadResults);

	/**
	 * Parses the arbitration results into something the game play code can handle
	 *
	 * @param SessionName the session that arbitration happened for
	 * @param ArbitrationResults the buffer filled by Live
	 */
	void ParseArbitrationResults(FName SessionName,PXSESSION_REGISTRATION_RESULTS ArbitrationResults);

	/**
	 * Tells the QoS thread to start its listening process. Builds the packet
	 * of custom data to send back to clients in the query.
	 *
	 * @param Session the named session being registered
	 *
	 * @return The success/error code of the operation
	 */
	DWORD RegisterQoS(FNamedSession* Session);

	/**
	 * Kicks off the list of returned servers' QoS queries
	 *
	 * @param AsyncData the object that holds the async QoS data
	 *
	 * @return TRUE if the call worked and the results should be polled for,
	 *		   FALSE otherwise
	 */
	UBOOL CheckServersQoS(FLiveAsyncTaskDataSearch* AsyncData);

	/**
	 * Parses the results from the QoS queries and places those results in the
	 * corresponding search results info
	 *
	 * @param QosData the data to parse the results of
	 */
	void ParseQoSResults(XNQOS* QosData);

	/**
	 * Registers all local players with the current session
	 *
	 * @param Session the session that they are registering in
	 * @param bIsFromInvite whether we are from an invite or not
	 */
	void RegisterLocalPlayers(FNamedSession* Session,UBOOL bIsFromInvite = FALSE);

	/**
	 * Parses the friends results into something the game play code can handle
	 *
	 * @param PlayerIndex the index of the player that this read was for
	 * @param LiveFriends the buffer filled by Live
	 * @param NumReturned the number of friends returned by live
	 */
	void ParseFriendsResults(DWORD PlayerIndex,PXONLINE_PRESENCE LiveFriends,DWORD NumReturned);

	/**
	 * Parses the friends results into something the game play code can handle
	 *
	 * @param PlayerIndex the index of the player that this read was for
	 * @param LiveFriends the buffer filled by Live
	 * @param NumReturned the number of friends returned by live
	 */
	void ParseFriendsResults(DWORD PlayerIndex,PXONLINE_FRIEND LiveFriends,DWORD NumReturned);

	/**
	 * Adds one setting to the users profile results
	 *
	 * @param Profile the profile object to copy the data from
	 * @param LiveData the Live data structures to copy the data to
	 */
	void AppendProfileSetting(BYTE PlayerNum,const FOnlineProfileSetting& Setting);

	/**
	 * Determines whether the specified settings should come from the game
	 * default settings. If so, the defaults are copied into the players
	 * profile results and removed from the settings list
	 *
	 * @param PlayerNum the id of the player
	 * @param SettingsIds the set of ids to filter against the game defaults
	 */
	void ProcessProfileDefaults(BYTE PlayerNum,TArray<DWORD>& SettingsId);

	/**
	 * Parses the read results and copies them to the stats read object
	 *
	 * @param ReadResults the data to add to the stats object
	 */
	void ParseStatsReadResults(XUSER_STATS_READ_RESULTS* ReadResults);

	/**
	 * Updates the flags and number of public/private slots that are available
	 *
	 * @param Session the session to modify/update
	 * @param ScriptDelegates the set of delegates to fire when the modify complete
	 *
	 * @return the success/error code of the modify action
	 */
	DWORD ModifySession(FNamedSession* Session,TArray<FScriptDelegate>* ScriptDelegates);

	/**
	 * Shrinks the session to the number of arbitrated registrants
	 *
	 * @param Session the session to modify/update
	 */
	void ShrinkToArbitratedRegistrantSize(FNamedSession* Session);

	/**
	 * Finishes creating the online game, including creating lan beacons and/or
	 * list play sessions
	 *
	 * @param HostingPlayerNum the player starting the session
	 * @param SessionName the name of the session that is being created
	 * @param CreateResult the result code from the async create operation
	 * @param bIsFromInvite whether this is from an invite or not
	 */
	void FinishCreateOnlineGame(DWORD HostingPlayerNum,FName SessionName,DWORD CreateResult,UBOOL bIsFromInvite);

	/**
	 * Finishes joining the online game
	 *
	 * @param HostingPlayerNum the player starting the session
	 * @param SessionName the name of the session that is being joined
	 * @param CreateResult the result code from the async create operation
	 * @param bIsFromInvite whether this is from an invite or not
	 */
	void FinishJoinOnlineGame(DWORD HostingPlayerNum,FName SessionName,DWORD CreateResult,UBOOL bIsFromInvite);

	/**
	 * Finishes migrating the online game
	 *
	 * @param PlayerNum the local player index migrating the session
	 * @param SessionName the name of the session that is being migrated
	 * @param MigrateResult the result code from the async create operation
	 * @param bIsHost TRUE if migrated session is for new host, FALSE if client
	 */
	void FinishMigrateOnlineGame(DWORD PlayerNum,FName SessionName,DWORD MigrateResult,UBOOL bIsHost);

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
				// So we can look at this in the debugger
				FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
				// Release platform specific version session info
				delete SessionInfo;
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

	/**
	 * Searches the named session array for the specified session id
	 *
	 * @param SessionId the id to find
	 *
	 * @return TRUE if the session exists, FALSE otherwise
	 */
	inline UBOOL IsInSession(QWORD& SessionId)
	{
		for (INT SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
		{
			FSecureSessionInfo* SessionInfo = GetSessionInfo(&Sessions(SearchIndex));
			if ((QWORD&)SessionInfo->XSessionInfo.sessionID == SessionId)
			{
				return TRUE;
			}
		}
		return FALSE;
	}

	/**
	 * @return Live specific id for a session
	 */
	virtual QWORD GetSessionId(FName SessionName)
	{
		FNamedSession* Session = GetNamedSession(SessionName);
		if (Session != NULL)
		{
			FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
			return (QWORD&)SessionInfo->XSessionInfo.sessionID;
		}
		return 0;
	}

	/**
	 * @return platform specific id for a session from a search result
	 */
	virtual QWORD GetSearchResultSessionId(const struct FOnlineGameSearchResult* SearchResult)
	{
		if (SearchResult != NULL && 
			SearchResult->PlatformData != NULL)
		{
			const XSESSION_INFO* SessionInfo = (const XSESSION_INFO*)SearchResult->PlatformData;
			return (QWORD&)SessionInfo->sessionID;
		}
		return 0;
	}

	/**
	 * Finds the cached achievements for this player and title id. If not found,
	 * it creates an empty one with the state to OERS_NotStarted
	 *
	 * @param PlayerNum the number of the player that the achievements are cached for
	 * @param TitleId the title that these are being read for
	 *
	 * @return The set of cached achievements for the player and title id
	 */
	inline FCachedAchievements& GetCachedAchievements(DWORD PlayerNum,DWORD TitleId)
	{
		// Search for one that matches the player and title id
		for (INT Index = 0; Index < AchievementList.Num(); Index++)
		{
			FCachedAchievements& Cached = AchievementList(Index);
			if (Cached.PlayerNum == PlayerNum && Cached.TitleId == TitleId)
			{
				return Cached;
			}
		}
		// Wasn't found so create
		INT AddIndex = AchievementList.AddZeroed();
		FCachedAchievements& Cached = AchievementList(AddIndex);
		Cached.PlayerNum = PlayerNum;
		Cached.TitleId = TitleId;
		return Cached;
	}

	/**
	 * Empties all of the cached achievement data for the specified player
	 *
	 * @param PlayerNum the number of the player that the achievements are cached for
	 */
	inline void ClearCachedAchievements(DWORD PlayerNum)
	{
		// Search for one that matches the player
		for (INT Index = 0; Index < AchievementList.Num(); Index++)
		{
			FCachedAchievements* Cached = &AchievementList(Index);
			if (Cached->PlayerNum == PlayerNum)
			{
				// Remove it and decrement the index so we don't skip any
				AchievementList.Remove(Index);
				Index--;
			}
		}
	}

	/**
	 * Empties the cached achievement data for the specified player and specified title
	 *
	 * @param PlayerNum the number of the player that the achievements are cached for
	 * @param TitleId the title that these are being read for
	 */
	inline void ClearCachedAchievements(DWORD PlayerNum,DWORD TitleId)
	{
		// Search for one that matches the player and title id
		for (INT Index = 0; Index < AchievementList.Num(); Index++)
		{
			FCachedAchievements* Cached = &AchievementList(Index);
			if (Cached->PlayerNum == PlayerNum && Cached->TitleId == TitleId)
			{
				AchievementList.Remove(Index);
				return;
			}
		}
	}

	/**
	 * Kicks off an async read of the skill information for the list of players in the
	 * search object
	 *
	 * @param SearchingPlayerNum the player executing the search
	 * @param SearchSettings the object that has the list of players to use in the read
	 *
	 * @return the error/success code from the skill search
	 */
	DWORD ReadSkillForSearch(BYTE SearchingPlayerNum,UOnlineGameSearch* SearchSettings);

	/**
	 * Returns the skill for the last search if the search manually specified a search value
	 * otherwise it uses the default skill rating
	 *
	 * @param OutMu has the skill rating set
	 * @param OutSigma has the skill certainty set
	 * @param OutCount has the number of contributing players in it
	 */
	void GetLocalSkills(DOUBLE& OutMu,DOUBLE& OutSigma,DOUBLE& OutCount);

	/**
	 * Determines how good of a skill match this session is for the local players
	 *
	 * @param Mu the skill rating of the local player(s)
	 * @param Sigma the certainty of that rating
	 * @param PlayerCount the number of players contributing to the skill
	 * @param GameSettings the game session to calculate the match quality for
	 */
	void CalculateMatchQuality(DOUBLE Mu,DOUBLE Sigma,DOUBLE PlayerCount,UOnlineGameSettings* GameSettings);

	/**
	 * Takes the manual skill override data and places that in the properties array
	 *
	 * @param SearchSettings the search object to update
	 */
	void AppendSkillProperties(UOnlineGameSearch* SearchSettings);

	/**
	 * Serializes this object
	 *
	 * @param Ar the archive to serialize to/from
	 */
	virtual void Serialize(FArchive& Ar);

	/**
	 * Finds the specified save game
	 *
	 * @param LocalUser the user that owns the data
	 * @param DeviceId the device to search for
	 * @param FriendlyName the friendly name of the save game data
	 * @param FileName the file name of the save game data
	 *
	 * @return a pointer to the data or NULL if not found
	 */
	FOnlineSaveGame* FindSaveGame(BYTE LocalUser,INT DeviceId,const FString& FriendlyName,const FString& FileName);

	/**
	 * Adds the specified save game
	 *
	 * @param LocalUser the user that owns the data
	 * @param DeviceId the device to search for
	 * @param FriendlyName the friendly name of the save game data
	 * @param FileName the file name of the save game data
	 *
	 * @return a pointer to the data or NULL if not found
	 */
	FOnlineSaveGame* AddSaveGame(BYTE LocalUser,INT DeviceId,const FString& FriendlyName,const FString& FileName);

	/**
	 * Checks the save games for a given player to see if any have async tasks outstanding
	 *
	 * @param LocalUser the user that owns the data
	 *
	 * @return true if a save game has an async task in progress, false otherwise
	 */
	UBOOL AreAnySaveGamesInProgress(BYTE LocalUser) const;

	/**
	 * Finds the specified save game
	 *
	 * @param LocalUser the user that owns the data
	 * @param DeviceId the device to search for
	 * @param TitleId the title id the save game is from
	 * @param FriendlyName the friendly name of the save game data
	 * @param FileName the file name of the save game data
	 *
	 * @return a pointer to the data or NULL if not found
	 */
	FOnlineCrossTitleSaveGame* FindCrossTitleSaveGame(BYTE LocalUser,INT DeviceId,INT TitleId,const FString& FriendlyName,const FString& FileName);

	/**
	 * Adds the specified save game
	 *
	 * @param LocalUser the user that owns the data
	 * @param DeviceId the device to search for
	 * @param TitleId the title id the save game is from
	 * @param FriendlyName the friendly name of the save game data
	 * @param FileName the file name of the save game data
	 *
	 * @return a pointer to the data or NULL if not found
	 */
	FOnlineCrossTitleSaveGame* AddCrossTitleSaveGame(BYTE LocalUser,INT DeviceId,INT TitleId,const FString& FriendlyName,const FString& FileName);

	/**
	 * Checks the save games for a given player to see if any have async tasks outstanding
	 *
	 * @param LocalUser the user that owns the data
	 *
	 * @return true if a save game has an async task in progress, false otherwise
	 */
	UBOOL AreAnyCrossTitleSaveGamesInProgress(BYTE LocalUser) const;

	/**
	 * Ticks the secure address cache so that it can unregister if needed
	 *
	 * @param DeltaTime the amount of time since the last tick
	 */
	void TickSecureAddressCache(FLOAT DeltaTime);

	/**
	 * Kicks off the async LSP resolves if there are LSPs to be used with this title
	 */
	void InitLspResolves(void);
#endif
