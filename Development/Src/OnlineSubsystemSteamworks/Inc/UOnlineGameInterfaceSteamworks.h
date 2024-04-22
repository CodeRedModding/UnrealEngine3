/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

// @todo Steam: Fix up the documentation/comments for all of the below; does not need to follow the multiline format, but must describe all functions
//			(and within the .cpp, their parameters/returns)

#if WITH_UE3_NETWORKING && WITH_STEAMWORKS
	/**
	 * Interface initialization
	 *
	 * @param InSubsystem	Reference to the initializing subsystem
	 */
	void InitInterface(UOnlineSubsystemSteamworks* InSubsystem);

	/**
	 * Cleanup stuff that happens outside of uobject's view
	 */
	virtual void FinishDestroy();


	/**
	 * Creates a new session info object that is correct for each platform
	 */
	virtual FSessionInfo* CreateSessionInfo(void)
	{
		return new FSessionInfoSteam();
	}

	/**
	 * Return the size of a session info struct
	 */
	virtual SIZE_T GetSessionInfoSize()
	{
		return sizeof(FSessionInfoSteam);
	}

	/**
	 * Updates the session info data
	 */
	void UpdateSessionInfo(DWORD ServerIP, INT ServerPort, QWORD ServerUID, UBOOL bSteamSockets);


	/**
	 * Shut down all Steam HServerQuery handles that might be in-flight
	 *
	 * @param SearchQueryState	The matchmaking query state containing the queries to be cancelled
	 */
	void CancelAllQueries(FMatchmakingQueryState* SearchQueryState);

	/** Handles updating of any async tasks that need to be performed */
	void Tick(FLOAT DeltaTime);

	/**
	 * Starts process to add a Server to the search results
	 *
	 * The server isn't actually added here, since we still need to obtain metadata about it, but
	 * this method kicks that off
	 *
	 * @param SearchQueryState	The matchmaking query state the result is from
	 * @param Server		The SteamAPI server data to be formatted into the search results
	 */
	void AddServerToSearchResults(FMatchmakingQueryState* SearchQueryState, gameserveritem_t* Server);

	/**
	 * Returns the platform specific connection information for joining the match.
	 * Call this function from the delegate of join completion
	 *
	 * @param SessionName the name of the session to resolve
	 * @param ConnectInfo the out var containing the platform specific connection information
	 *
	 * @return true if the call was successful, false otherwise
	 */
	UBOOL GetResolvedConnectString(FName SessionName,FString& ConnectInfo);

	/** Updates the server details with the new data */
	void UpdateGameSettingsData(UOnlineGameSettings* InGameSettings, const SteamRulesMap& Rules);

	/**
	 * Frees the specified online query and marks the search as done
	 *
	 * @param SearchQueryState	The matchmaking query state performing the query
	 * @param bCancel		Whether or not we are cancelling an active query, or cleaning up a finished one
	 */
	void CleanupOnlineQuery(FMatchmakingQueryState* SearchQueryState, UBOOL bCancel);

	/** Returns TRUE if the game wants stats, FALSE if not */
	UBOOL GameWantsStats();

	/** Returns TRUE if Game Server init succeeded, FALSE if not */
	UBOOL PublishSteamServer();

	/** Do some paperwork when Steam tells us the server policy. */
	void OnGSPolicyResponse(const UBOOL bIsVACSecured);

	/** overridden from superclass. */
	UBOOL FindOnlineGames(BYTE SearchingPlayerNum, UOnlineGameSearch* SearchSettings);

	/**
	 * Kicks off a search for internet games
	 *
	 * @param SearchQueryState	The matchmaking query state to kickoff the query in
	 * @param FilterKeyList		The array of keys (with matching value) to filter on the master server
	 * @param FilterValueList	The list of values associated with each key in FilterKeyList
	 * @return			Returns the status of the search (pending/failure)
	 */
	DWORD FindInternetGames(FMatchmakingQueryState* SearchQueryState, TArray<FString>& FilterKeyList, TArray<FString>& FilterValueList);

	// Shouldn't be used; use above
	DWORD FindInternetGames()
	{
		check(0);
		return E_FAIL;
	}


	UBOOL CancelFindOnlineGames();
	DWORD CancelFindInternetGames();
	DWORD CancelFindLanGames();
	void RefreshPublishedGameSettings();
	UBOOL CreateOnlineGame(BYTE HostingPlayerNum, FName SessionName, UOnlineGameSettings* NewGameSettings);
	DWORD CreateInternetGame(BYTE HostingPlayerNum);
	UBOOL JoinOnlineGame(BYTE PlayerNum, FName SessionName, const FOnlineGameSearchResult& DesiredGame);
	DWORD JoinInternetGame(BYTE PlayerNum);
	UBOOL StartOnlineGame(FName SessionName);
	DWORD StartInternetGame();
	UBOOL EndOnlineGame(FName SessionName);
	DWORD EndInternetGame();
	UBOOL DestroyOnlineGame(FName SessionName);
	DWORD DestroyInternetGame();
	void TickLanTasks(FLOAT DeltaTime);
	void TickInternetTasks(FLOAT DeltaTime);
	void SetInviteInfo(const TCHAR* LocationString);
	void RegisterLocalTalkers();
	void UnregisterLocalTalkers();
	DWORD ReadPlatformSpecificInternetSessionInfo(const FOnlineGameSearchResult& DesiredGame, BYTE PlatformSpecificInfo[80]);
	DWORD BindPlatformSpecificSessionToInternetSearch(BYTE SearchingPlayerNum, UOnlineGameSearch* SearchSettings, BYTE* PlatformSpecificInfo);

	/** Builds the key/value filter list for the server browser filtering */
	void BuildServerBrowserFilterMap(TArray<FClientFilterORClause>& OutFilterList, TMap<FString,FString>& OutMasterFilterList,
				UBOOL bDisableMasterFilters=FALSE);

	/**
	 * Kicks off a search for a server with a specific IP:Port, for processing an invite
	 *
	 * @param ServerAddress		The address of the server to search for
	 * @param ServerUID		The steam sockets address of the server
	 * @return			Whether or not the search kicked off successfully
	 */
	UBOOL FindInviteGame(FString& ServerAddress, QWORD ServerUID=0);


	friend class FOnlineAsyncTaskSteamServerRulesRequest;
#endif // WITH_UE3_NETWORKING && WITH_STEAMWORKS


