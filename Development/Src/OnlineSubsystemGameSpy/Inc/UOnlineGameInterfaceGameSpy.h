/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#if WITH_UE3_NETWORKING

public:
	/**
	 * Called when a Server key needs to be reported
	 *
	 * @param KeyId the key that is being requested
	 * @param OutBuf the buffer to append to
	 */
	void ServerKeyCallback(INT KeyId, qr2_buffer_t OutBuf);

	/**
	 * Called when a player key needs to be reported
	 *
	 * @param KeyId the key that is being requested
	 * @param Index the index of the player being queried
	 * @param OutBuf the buffer to append to
	 */
	void PlayerKeyCallback(INT KeyId, INT Index, qr2_buffer_t OutBuf);

	/**
	 * Called when a team key needs to be reported
	 *
	 * @param KeyId the key that is being requested
	 * @param Index the index of the team being queried
	 * @param OutBuf the buffer to append to
	 */
	void TeamKeyCallback(INT KeyId, INT Index, qr2_buffer_t OutBuf);

	/**
	 * Called when we need to report the list of keys we report values for
	 *
	 * @param KeyId the key that is being requested
	 * @param KeyBuffer the buffer to append to
	 */
	void KeyListCallback(qr2_key_type KeyType, qr2_keybuffer_t KeyBuffer);

	/**
	 * Called when we need to report the number of players and teams
	 *
	 * @param KeyType the type of object that needs the count reported for
	 *
	 * @return the number of items for this key type
	 */
	INT CountCallback(qr2_key_type KeyType);

	/**
	 * Called if our registration with the GameSpy master Server failed
	 *
	 * @param Error the error code that occured
	 * @param ErrorMessage message form of the error
	 */
	void AddErrorCallback(qr2_error_t Error, gsi_char *ErrorMessage);

	/**
	 * Called with updates from the ServerBrowsing SDK (Server adds, details ready, etc)
	 *
	 * @param SB the GameSpy server browser object
	 * @param Reason the reason the callback happened
	 * @param Server the server object that was updated
	 */
	void SBCallback(ServerBrowser SB, SBCallbackReason Reason, SBServer Server);

	/**
	 * Registers all the properties & localized settings as QR2 objects
	 */
	void QR2SetupCustomKeys(void);

	/** Returns TRUE if the game wants stats, FALSE if not */
	inline UBOOL GameWantsStats(void)
	{
		return GameSettings != NULL &&
			GameSettings->bIsLanMatch == FALSE &&
			GameSettings->bUsesStats == TRUE;
	}

	/**
	 * Creates a game settings object populated with the information from the location string
	 *
	 * @param LocationString the string to parse
	 *
	 * @return the object that was created
	 */
	void SetInviteInfo(const TCHAR* LocationString);

protected:
	/**
	 * Adds a Server to the search results
	 */
	void AddServerToSearchResults(SBServer Server);

	/**
	 * Updates the server details with the new data
	 *
	 * @param GameSettings the game settings to update
	 * @param Server the GameSpy data to update with
	 */
	void UpdateGameSettingsData(UOnlineGameSettings* GameSettings,SBServer Server);

	/**
	 * Builds a filter string based on the current GameSearch
	 */
	void BuildFilter(FString& Filter);

	/**
	 * Builds a GameSpy query and submits it to the GameSpy backend
	 *
	 * @return an Error/success code
	 */
	virtual DWORD FindInternetGames(void);

	/**
	 * Attempts to cancel an internet game search
	 *
	 * @return an error/success code
	 */
	virtual DWORD CancelFindInternetGames(void);

	/**
	 * Serializes the platform specific data into the provided buffer for the specified search result
	 *
	 * @param DesiredGame the game to copy the platform specific data for
	 * @param PlatformSpecificInfo the buffer to fill with the platform specific information
	 *
	 * @return true if successful serializing the data, false otherwise
	 */
	virtual DWORD ReadPlatformSpecificInternetSessionInfo(const FOnlineGameSearchResult& DesiredGame,BYTE PlatformSpecificInfo[80]);

	/**
	 * Builds a search result using the platform specific data specified
	 *
	 * @param SearchingPlayerNum the index of the player searching for a match
	 * @param SearchSettings the desired search to bind the session to
	 * @param PlatformSpecificInfo the platform specific information to convert to a server object
	 *
	 * @return an error/success code
	 */
	virtual DWORD BindPlatformSpecificSessionToInternetSearch(BYTE SearchingPlayerNum,UOnlineGameSearch* SearchSettings,BYTE* PlatformSpecificInfo);

	/**
	 * Creates a new GameSpy enabled game and registers it with the backend
	 *
	 * @param HostingPlayerNum the player hosting the game
	 *
	 * @return S_OK if it succeeded, otherwise an Error code
	 */
	virtual DWORD CreateInternetGame(BYTE HostingPlayerNum);

	/**
	 * Joins the specified internet enabled game
	 *
	 * @param PlayerNum the player joining the game
	 *
	 * @return S_OK if it succeeded, otherwise an Error code
	 */
	virtual DWORD JoinInternetGame(BYTE PlayerNum);

	/**
	 * Starts the specified internet enabled game
	 *
	 * @return S_OK if it succeeded, otherwise an Error code
	 */
	virtual DWORD StartInternetGame(void);

	/**
	 * Ends the specified internet enabled game
	 *
	 * @return S_OK if it succeeded, otherwise an Error code
	 */
	virtual DWORD EndInternetGame(void);

	/**
	 * Terminates a GameSpy session, removing it from the GameSpy backend
	 *
	 * @return an Error/success code
	 */
	virtual DWORD DestroyInternetGame(void);

	/**
	 * Updates any pending internet tasks and fires event notifications as needed
	 *
	 * @param DeltaTime the amount of time that has elapsed since the last call
	 */
	virtual void TickInternetTasks(FLOAT DeltaTime);

	/** Registers all of the local talkers with the voice engine */
	virtual void RegisterLocalTalkers(void);

	/** Unregisters all of the local talkers from the voice engine */
	virtual void UnregisterLocalTalkers(void);

	/** Frees the current server browser query and marks the search as done */
	void CleanupServerBrowserQuery(void);

	/**
	 * Marks a server in the server list as unreachable
	 *
	 * @param Addr the IP addr of the server to update
	 */
	void MarkServerAsUnreachable(const FInternetIpAddr& Addr);

	/**
	 * Creates a new session info object that is correct for each platform.
	 * Creates a specialized version for the PS3 if desired
	 */
	virtual FSessionInfo* CreateSessionInfo(void)
	{
#if PS3 && WANTS_NAT_TRAVERSAL
		return new FSessionInfoPS3();
#else
		return new FSessionInfo();
#endif
	}

	/**
	 *	Return the size of a session info struct
	 */
	virtual SIZE_T GetSessionInfoSize()
	{
#if PS3 && WANTS_NAT_TRAVERSAL
		return sizeof(FSessionInfoPS3);
#else
		return sizeof(FSessionInfo);
#endif
	}

#endif	//#if WITH_UE3_NETWORKING
