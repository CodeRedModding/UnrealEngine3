/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemSteamworks.h"

#if WITH_UE3_NETWORKING && WITH_STEAMWORKS

// Extra debug logging for server filters
#define FILTER_DUMP 0


/**
 * Async events/tasks
 */

/**
 * Notification event from Steam, for handling individual server list entries (tied to FOnlineAsyncTaskSteamServerListRequest)
 */
class FOnlineAsyncEventSteamServerListResponse : public FOnlineAsyncEvent
{
protected:
	/** The game interface which callbacks are passed to */
	UOnlineGameInterfaceSteamworks*	CallbackInterface;

	/** Reference to the matchmaking query state in the CallbackInterface (game interface), that this search result is tied to */
	FMatchmakingQueryState*		SearchQueryState;

	/** The server list request this result is associated with */
	HServerListRequest		Request;

	/** The index of the server which has (or has failed to) return response results */
	INT				ServerIndex;

	/** Whether or not the server has responded successfully */
	UBOOL				bResponseSuccess;


	/** Hidden constructor */
	FOnlineAsyncEventSteamServerListResponse()
		: CallbackInterface(NULL)
		, SearchQueryState(NULL)
		, Request(NULL)
		, ServerIndex(INDEX_NONE)
		, bResponseSuccess(FALSE)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InGameInterface	The game interface object this task is linked to
	 * @param InSearchQueryState		The matchmaking query state within the game interface, this search will be tied to
	 * @param InRequest		The server list request this result is associated with
	 * @param InServerIndex		The index of the server which has (or has failed to) return response results
	 * @param bInResponseSuccess	Whether or not the server has responded successfully
	 */
	FOnlineAsyncEventSteamServerListResponse(UOnlineGameInterfaceSteamworks* InGameInterface, FMatchmakingQueryState* InSearchQueryState,
							HServerListRequest InRequest, INT InServerIndex, UBOOL bInResponseSuccess)
		: CallbackInterface(InGameInterface)
		, SearchQueryState(InSearchQueryState)
		, Request(InRequest)
		, ServerIndex(InServerIndex)
		, bResponseSuccess(bInResponseSuccess)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamServerListResponse()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServerListResponse completed ServerIndex: %i, bResponseSuccess: %i"), ServerIndex,
					bResponseSuccess);
	}

	/**
	 * Whether or not the task manager should execute this item when it returns to the game thread
	 * Used as a hook-in to check that the object the item is sending results to, is still valid
	 *
	 * @return	Whether or not the execute Finalize and TriggerDelegates
	 */
	virtual UBOOL CanExecute()
	{
		return GSteamAsyncTaskManager->IsRegisteredInterface(CallbackInterface);
	}


	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEvent::Finalize();


		SearchQueryState->LastActivityTimestamp = appSeconds();

		if (SearchQueryState->CurrentMatchmakingType != SMT_Invalid && SearchQueryState->CurrentMatchmakingQuery != NULL &&
			Request == SearchQueryState->CurrentMatchmakingQuery)
		{
			gameserveritem_t* Server = GSteamMatchmakingServers->GetServerDetails(SearchQueryState->CurrentMatchmakingQuery,
								ServerIndex);

			if (bResponseSuccess)
			{
				debugf(NAME_DevOnline, TEXT("ServerListResponse: Got server details (index: %i)"), ServerIndex);


				if (Server != NULL)
				{
					debugf(NAME_DevOnline, TEXT("Ping: %i, bHadSuccessfulResponse: %i, bDoNotRefresh: %i, GameDir: %s"),
							Server->m_nPing, (INT)Server->m_bHadSuccessfulResponse, (INT)Server->m_bDoNotRefresh,
							UTF8_TO_TCHAR(Server->m_szGameDir));

					debugf(NAME_DevOnline,
						TEXT("Map: %s, GameDescription: %s, AppId: %i, Players: %i, MaxPlayers: %i, BotPlayers: %i"),
						UTF8_TO_TCHAR(Server->m_szMap), UTF8_TO_TCHAR(Server->m_szGameDescription), Server->m_nAppID,
						Server->m_nPlayers, Server->m_nMaxPlayers, Server->m_nBotPlayers);

					debugf(NAME_DevOnline,
						TEXT("bPassword: %i, bSecure: %i, LastPlayed: %i, Version: %i, ServerName: %s, GameTags: %s"),
						(INT)Server->m_bPassword, (INT)Server->m_bSecure, Server->m_ulTimeLastPlayed, Server->m_nServerVersion,
						UTF8_TO_TCHAR(Server->GetName()), UTF8_TO_TCHAR(Server->m_szGameTags));

					CallbackInterface->AddServerToSearchResults(SearchQueryState, Server);
				}
				else
				{
					debugf(NAME_DevOnline, TEXT("ServerListResponse: Warning!!! 'GetServerDetails' returned NULL"));
				}
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("ServerListResponse: Server '%i' failed to respond"), ServerIndex);

				// Just dump the server by doing nothing
				if (Server != NULL)
				{
					FInternetIpAddr ServerIP;

					ServerIP.SetIp((DWORD)Server->m_NetAdr.GetIP());
					ServerIP.SetPort((DWORD)Server->m_NetAdr.GetQueryPort());

					debugf(NAME_DevOnline, TEXT("ServerListResponse: Server '%i' Address: %s"), ServerIndex,
						*ServerIP.ToString(TRUE));
				}
				else
				{
					debugf(NAME_DevOnline, TEXT("ServerListResponse: No extra server details available"));
				}
			}
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("ServerListResponse: Got server list response without matching query"));
		}
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEvent::TriggerDelegates();
	}
};

/**
 * Asynchronous task for Steam, for requesting and receiving server list entries
 */
class FOnlineAsyncTaskSteamServerListRequest
	: public FOnlineAsyncTaskSteamBase<UOnlineGameInterfaceSteamworks>
	, public ISteamMatchmakingServerListResponse
{
private:
	/** Reference to the matchmaking query state in the CallbackInterface (game interface), that this search is tied to */
	FMatchmakingQueryState*		SearchQueryState;

	/** The server list request this result is associated with */
	HServerListRequest		ListRequest;

	/** The overall result of the server list request */
	EMatchMakingServerResponse	FinalResponse;


	/** Whether or not the SteamAPI is done with this object, and it is ready for deletion */
	UBOOL				bMarkedForDelete;

	/** Whether or not the async task manager is done with this item */
	UBOOL				bTaskComplete;


	/** Hidden constructor */
	FOnlineAsyncTaskSteamServerListRequest()
		: SearchQueryState(NULL)
		, ListRequest(NULL)
		, FinalResponse(eNoServersListedOnMasterServer)
		, bMarkedForDelete(FALSE)
		, bTaskComplete(FALSE)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InGameInterface		The game interface object this task is linked to
	 * @param InSearchQueryState		The matchmaking query state within the game interface, this search will be tied to
	 */
	FOnlineAsyncTaskSteamServerListRequest(UOnlineGameInterfaceSteamworks* InGameInterface, FMatchmakingQueryState* InSearchQueryState)
		: FOnlineAsyncTaskSteamBase(InGameInterface)
		, SearchQueryState(InSearchQueryState)
		, ListRequest(NULL)
		, FinalResponse(eNoServersListedOnMasterServer)
		, bMarkedForDelete(FALSE)
		, bTaskComplete(FALSE)
	{
	}

	/**
	 * Base virtual destructor
	 */
	virtual ~FOnlineAsyncTaskSteamServerListRequest()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		// @todo Steam: Create an inline function to stringize FinalResponse eventually
		return FString::Printf(TEXT("FOnlineAsyncTaskSteamServerListRequest completed FinalResponse: %i"), (INT)FinalResponse);
	}

	/**
	 * Whether or not the task manager should delete this item once done with it
	 *
	 * @return	Whether or not the async task manager is responsible for deleting this item
	 */
	virtual UBOOL CanDelete()
	{
		return bMarkedForDelete;
	}

	/**
	 * Whether or not this item should block other items from returning until it has completed
	 *
	 * @return	Whether or not the item should block
	 */
	virtual UBOOL IsBlocking()
	{
		return FALSE;
	}

	/**
	 * Called by the server browser when it and the SteamAPI is done with this object
	 */
	void MarkForDelete()
	{
		// The async task manager is already done with this object, delete immediately
		if (bTaskComplete)
		{
			delete this;
		}
		// The async task manager is still processing the object, let the async task manager handle deletion
		else
		{
			bMarkedForDelete = TRUE;
		}
	}


	/**
	 * Called by the SteamAPI when a server has successfully responded
	 * NOTE: Called on online thread
	 */
	void ServerResponded(HServerListRequest Request, int iServer)
	{
		// Create a separate callback result for each response
		FOnlineAsyncEventSteamServerListResponse* NewEvent =
			new FOnlineAsyncEventSteamServerListResponse(CallbackInterface, SearchQueryState, Request, iServer, TRUE);

		GSteamAsyncTaskManager->AddToOutQueue(NewEvent);
	}

	/**
	 * Called by the SteamAPI when a server has failed to respond
	 * NOTE: Called on online thread
	 */
	void ServerFailedToRespond(HServerListRequest Request, int iServer)
	{
		// Create a separate callback result for each response
		FOnlineAsyncEventSteamServerListResponse* NewEvent =
			new FOnlineAsyncEventSteamServerListResponse(CallbackInterface, SearchQueryState, Request, iServer, FALSE);

		GSteamAsyncTaskManager->AddToOutQueue(NewEvent);
	}

	/**
	 * Called by the SteamAPI when all server requests for the list have completed
	 * NOTE: Called on online thread
	 */
	void RefreshComplete(HServerListRequest Request, EMatchMakingServerResponse Response)
	{
		ListRequest = Request;
		FinalResponse = Response;

		bWasSuccessful = Response != eServerFailedToRespond;
		bIsComplete = TRUE;
	}


	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncTaskSteamBase::Finalize();


		if (!SearchQueryState->bIgnoreRefreshComplete)
		{
			if (SearchQueryState->CurrentMatchmakingType != SMT_Invalid && SearchQueryState->CurrentMatchmakingQuery != NULL &&
				SearchQueryState->CurrentMatchmakingQuery == ListRequest)
			{
				debugf(NAME_DevOnline, TEXT("ServerListRefreshComplete: Got 'refresh complete' callback for current query"));
				CallbackInterface->CleanupOnlineQuery(SearchQueryState, FALSE);

				// We'll signal the app about being complete in game interface Tick(), since this callback only means we've
				// got the whole server list; we may still be waiting on metadata queries for some of them
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("ServerListRefreshComplete: Got server list response without matching query"));
			}
		}
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncTaskSteamBase::TriggerDelegates();

		bTaskComplete = TRUE;
	}
};


// Forward declaration
class FOnlineAsyncTaskSteamServerRulesRequest;

/**
 * Notification event from Steam, for handling individual server rules entries (tied to FOnlineAsyncTaskSteamServerRulesRequest)
 */
class FOnlineAsyncEventSteamServerRulesResponse : public FOnlineAsyncEvent
{
protected:
	/** The game interface which callbacks are passed to */
	UOnlineGameInterfaceSteamworks*			CallbackInterface;

	/** The SteamAPI-derived query object this response is tied to */
	FOnlineAsyncTaskSteamServerRulesRequest*	QueryObj;


	/** The rule key that has been received for the server */
	FString						Rule;

	/** The value of the rule that has been received for the server */
	FString						Value;

	/** Whether or not the server has responded successfully */
	UBOOL						bResponseSuccess;


	/** Hidden constructor */
	FOnlineAsyncEventSteamServerRulesResponse()
		: CallbackInterface(NULL)
		, QueryObj(NULL)
		, bResponseSuccess(FALSE)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InGameInterface	The game interface object this task is linked to
	 * @param InQueryObj		The query object this result is associated with
	 * @param bInResponseSuccess	Whether or not the response was successful
	 * @param InRule		The rule key received for the server
	 * @param InValue		The value of the rule received for the server
	 */
	FOnlineAsyncEventSteamServerRulesResponse(UOnlineGameInterfaceSteamworks* InGameInterface, FOnlineAsyncTaskSteamServerRulesRequest* InQueryObj,
							UBOOL bInResponseSuccess, const char* InRule=NULL, const char* InValue=NULL)
		: CallbackInterface(InGameInterface)
		, QueryObj(InQueryObj)
		, bResponseSuccess(bInResponseSuccess)
	{
		if (InRule != NULL)
		{
			Rule = FString(UTF8_TO_TCHAR(InRule));
		}

		if (InValue != NULL)
		{
			Value = FString(UTF8_TO_TCHAR(InValue));
		}
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamServerRulesResponse()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServerRulesResponse completed Rule: %s, Value: %s, bResponseSuccess: %i"),
					*Rule, *Value, (INT)bResponseSuccess);
	}

	/**
	 * Whether or not the task manager should execute this item when it returns to the game thread
	 * Used as a hook-in to check that the object the item is sending results to, is still valid
	 *
	 * @return	Whether or not the execute Finalize and TriggerDelegates
	 */
	virtual UBOOL CanExecute()
	{
		return GSteamAsyncTaskManager->IsRegisteredInterface(CallbackInterface);
	}


	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 *
	 * NOTE: Implemented further below due to circular dependancy with below class
	 */
	virtual void Finalize();

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEvent::TriggerDelegates();
	}
};

/**
 * Asynchronous task for Steam, for requesting and receiving server rules entries
 */
class FOnlineAsyncTaskSteamServerRulesRequest
	: public FOnlineAsyncTaskSteamBase<UOnlineGameInterfaceSteamworks>
	, public ISteamMatchmakingRulesResponse
{
private:
	/** Whether or not the SteamAPI is done with this object, and it is ready for deletion */
	UBOOL				bMarkedForDelete;

	/** Whether or not the async task manager is done with this item */
	UBOOL				bTaskComplete;


	/** Map used to store the retrieved rules for the server */
	SteamRulesMap			Rules;

	/** The cached game settings for the server */
	UOnlineGameSettings*		Settings;

	/** The cached IP address of the server */
	DWORD				Addr;

	/** The cached Port for the server */
	INT				Port;

	/** The cached SteamId for the server */
	QWORD				SteamId;

public:
	// @todo Steam: Un-fudge this when you do the server browser overhaul (two delete variables is confusing)

	/** Used to flag for future deletion, since this can't deleted during callback (NOTE: Keep it this way, despite threaded callbacks) */
	/** NOTE: This is part of the server browser cleanup code "this object is waiting for cleanup", whereas bMarkedForDelete handles the
			actual deletion of the object itself */
	UBOOL				bDeleteMe;


private:
	/** Hidden constructor */
	FOnlineAsyncTaskSteamServerRulesRequest()
		: bMarkedForDelete(FALSE)
		, bTaskComplete(FALSE)
		, Settings(NULL)
		, Addr(0)
		, Port(-1)
		, SteamId(0)
		, bDeleteMe(FALSE)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InGameInterface		The game interface object this task is linked to
	 * @param InSettings			Tracks the settings object associated with the server, for storing rules and server details
	 * @param InAddr			The address of the server
	 * @param InPort			The server port
	 * @param InSteamId			The SteamID of the server
	 */
	FOnlineAsyncTaskSteamServerRulesRequest(UOnlineGameInterfaceSteamworks* InGameInterface, UOnlineGameSettings* InSettings, DWORD InAddr,
						INT InPort, QWORD InSteamId)
		: FOnlineAsyncTaskSteamBase(InGameInterface)
		, bMarkedForDelete(FALSE)
		, bTaskComplete(FALSE)
		, Settings(InSettings)
		, Addr(InAddr)
		, Port(InPort)
		, SteamId(InSteamId)
		, bDeleteMe(FALSE)
	{
		if (CallbackInterface != NULL)
		{
			CallbackInterface->ServerBrowserSearchQuery.PendingRulesSearchSettings.AddUniqueItem(InSettings);
		}
	}

	/**
	 * Base virtual destructor
	 */
	virtual ~FOnlineAsyncTaskSteamServerRulesRequest()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskSteamServerRulesRequest completed"));
	}

	/**
	 * Whether or not the task manager should delete this item once done with it
	 *
	 * @return	Whether or not the async task manager is responsible for deleting this item
	 */
	virtual UBOOL CanDelete()
	{
		return bMarkedForDelete;
	}

	/**
	 * Whether or not this item should block other items from returning until it has completed
	 *
	 * @return	Whether or not the item should block
	 */
	virtual UBOOL IsBlocking()
	{
		return FALSE;
	}

	/**
	 * Called by the server browser when it and the SteamAPI is done with this object
	 */
	void MarkForDelete()
	{
		// The async task manager is already done with this object, delete immediately
		if (bTaskComplete)
		{
			delete this;
		}
		// The async task manager is still processing the object, let the async task manager handle deletion
		else
		{
			bMarkedForDelete = TRUE;
		}
	}


	/**
	 * Called by the SteamAPI when the server has responded with an individual server rule
	 * NOTE: Called on online thread
	 */
	void RulesResponded(const char* pchRule, const char* pchValue)
	{
		// Create a separate callback result for each response
		FOnlineAsyncEventSteamServerRulesResponse* NewEvent =
			new FOnlineAsyncEventSteamServerRulesResponse(CallbackInterface, this, TRUE, pchRule, pchValue);

		GSteamAsyncTaskManager->AddToOutQueue(NewEvent);
	}

	/**
	 * Called by the SteamAPI when the server has failed to respond
	 * NOTE: Called on online thread
	 */
	void RulesFailedToRespond()
	{
		// Create a separate callback result for each response
		FOnlineAsyncEventSteamServerRulesResponse* NewEvent =
			new FOnlineAsyncEventSteamServerRulesResponse(CallbackInterface, this, FALSE);

		GSteamAsyncTaskManager->AddToOutQueue(NewEvent);
	}

	/**
	 * Called by the SteamAPI when the grabbing of rules for this server has completed
	 * NOTE: Called on online thread
	 */
	void RulesRefreshComplete()
	{
		bWasSuccessful = TRUE;
		bIsComplete = TRUE;
	}


	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncTaskSteamBase::Finalize();

		CallbackInterface->ServerBrowserSearchQuery.LastActivityTimestamp = appSeconds();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncTaskSteamBase::TriggerDelegates();


		debugf(NAME_DevOnline, TEXT("ServerRulesRequest: Rules refresh complete"));

		if (!bDeleteMe)
		{
			if (Settings != NULL)
			{
				UBOOL bPassedFilters = TRUE;

				// If clientside filters are set, do the comparison now; iterate the AND clauses
				for (INT i=0; i<CallbackInterface->ServerBrowserSearchQuery.ActiveClientsideFilters.Num(); ++i)
				{
					FilterMap& CurFilter = CallbackInterface->ServerBrowserSearchQuery.ActiveClientsideFilters(i).OrParams;

					if (CurFilter.Num() == 0)
					{
						continue;
					}


					UBOOL bCurORResult = FALSE;

#if FILTER_FAIL_LOG
					debugf(NAME_DevOnline, TEXT("Beginning check of new AND filter set"));
#endif

					// Iterate the OR clauses (the final results of all OR clauses are 'AND'ed together)
					// NOTE: There can be multiple filters with the same key
					for (FilterMap::TIterator It(CurFilter); It; ++It)
					{
						FString* RuleValuePtr = Rules.Find(It.Key());
						FSearchFilterValue FilterValue = It.Value();

						if (RuleValuePtr == NULL)
						{
							// Be smart about this, and detect != "non-empty" cases
							if (FilterValue.Operator == OGSCT_NotEquals && !FilterValue.Value.IsEmpty())
							{
								bCurORResult = TRUE;
								break;
							}

							continue;
						}


						FString& RuleValue = *RuleValuePtr;

						UBOOL bCurFilterResult = FALSE;

						switch (FilterValue.Operator)
						{
						case OGSCT_Equals:
							bCurFilterResult = FilterValue.Value == RuleValue;
							break;

						case OGSCT_NotEquals:
							bCurFilterResult = FilterValue.Value != RuleValue;
							break;

						case OGSCT_GreaterThan:
							bCurFilterResult = FilterValue.Value > RuleValue;
							break;

						case OGSCT_GreaterThanEquals:
							bCurFilterResult = FilterValue.Value >= RuleValue;
							break;

						case OGSCT_LessThan:
							bCurFilterResult = FilterValue.Value < RuleValue;
							break;

						case OGSCT_LessThanEquals:
							bCurFilterResult = FilterValue.Value <= RuleValue;
							break;

						default:
							bCurFilterResult = FALSE;
							break;
						}

#if FILTER_FAIL_LOG
						if (!bCurFilterResult)
						{
							debugf(NAME_DevOnline, TEXT("Non-fatal filter fail: FilterValue: %s, ServerValue: %s, Key: %s"),
								*FilterValue.Value, *RuleValue, *It.Key());
						}
#endif


						bCurORResult = bCurORResult || bCurFilterResult;

						// Break if any OR clauses succeed early
						if (bCurORResult)
						{
							break;
						}
					}

					bPassedFilters = bPassedFilters && bCurORResult;

					// Break if any AND clauses fail early
					if (!bPassedFilters)
					{
#if FILTER_FAIL_LOG
						debugf(NAME_DevOnline, TEXT("Fatal filter failure in current filter set"));
#endif

						break;
					}
				}


				if (bPassedFilters)
				{
					// Finish filling in the game settings...
					CallbackInterface->UpdateGameSettingsData(Settings, Rules);

					// ...and finally add it to the search results.
					if (CallbackInterface->ServerBrowserSearchQuery.GameSearch != NULL)
					{
						FSessionInfoSteam* SessInfo = (FSessionInfoSteam*)CallbackInterface->CreateSessionInfo();

						SessInfo->HostAddr.SetIp(Addr);
						SessInfo->HostAddr.SetPort(Port);

						FString* ServerUIDPtr = Rules.Find(TEXT("SteamServerId"));

						if (ServerUIDPtr != NULL)
						{
							SessInfo->ServerUID = appAtoi64(**ServerUIDPtr);
							SessInfo->bSteamSockets = TRUE;
						}
						// If the server is not advertising a SteamServerId value, but the net driver has bSteamSocketsOnly
						//	set, then assume the server is using steam sockets anyway, and grab the cached SteamServerId
						//	value
						else if (IsSteamSocketsOnly())
						{
							SessInfo->ServerUID = SteamId;
							SessInfo->bSteamSockets = TRUE;
						}

						const INT Position = CallbackInterface->ServerBrowserSearchQuery.GameSearch->Results.AddZeroed();
						FOnlineGameSearchResult& Result =
							CallbackInterface->ServerBrowserSearchQuery.GameSearch->Results(Position);

						Result.PlatformData = SessInfo;
						Result.GameSettings = Settings;
					}
				}


				// GameSpy calls this for every server update (and everything ignores the result).
				FAsyncTaskDelegateResults Param(S_OK);
				TriggerOnlineDelegates(CallbackInterface, CallbackInterface->FindOnlineGamesCompleteDelegates, &Param);
			}


			// Done with this query, mark for deletion
			bDeleteMe = TRUE;
		}


		bTaskComplete = TRUE;
	}


	// @todo Steam: Are any member access functions required anymore, now that you handle the callback here? (think only GetSettings)

	/**
	 * Member access for the rules map (allows writing)
	 *
	 * @return	Returns a reference to the rules map
	 */
	SteamRulesMap& GetRules()
	{
		return Rules;
	}

	/**
	 * Other member access functions
	 */

	UOnlineGameSettings* GetSettings() const
	{
		return Settings;
	}

	DWORD GetAddr() const
	{
		return Addr;
	}

	INT GetPort() const
	{
		return Port;
	}

	QWORD GetSteamId() const
	{
		return SteamId;
	}
};


/**
 * Give the async task a chance to marshal its data back to the game thread
 * Can only be called on the game thread by the async task manager
 * NOTE: Implemented here due to circular dependancy with above class
 */
void FOnlineAsyncEventSteamServerRulesResponse::Finalize()
{
	FOnlineAsyncEvent::Finalize();

	CallbackInterface->ServerBrowserSearchQuery.LastActivityTimestamp = appSeconds();

	if (bResponseSuccess)
	{
		debugf(NAME_DevOnline, TEXT("ServerRulesResponse: Retrieved server rule: %s=%s"), *Rule, *Value);

		if (!QueryObj->bDeleteMe)
		{
			UBOOL bAddServer = TRUE;

			// Don't list this server if it's for a different engine version; otherwise UDK servers for different releases won't work
			// Licensees that are conforming their packages between releases may want to change this.
			if (CallbackInterface->bFilterEngineBuild && Rule == TEXT("SteamEngineVersion"))
			{
				if (appAtoi64(*Value) != GEngineVersion)
				{
					debugf(NAME_DevOnline, TEXT("This server is for a different engine version (%s), rejecting."), *Value);
					QueryObj->bDeleteMe = TRUE;

					for (INT i=0; i<CallbackInterface->ServerBrowserSearchQuery.QueryToRulesResponseMap.Num(); i++)
					{
						if (CallbackInterface->ServerBrowserSearchQuery.QueryToRulesResponseMap(i).Response == QueryObj)
						{
							GSteamMatchmakingServers->CancelServerQuery(
									CallbackInterface->ServerBrowserSearchQuery.QueryToRulesResponseMap(i).Query);
							break;
						}
					}
				}

				// Don't add this rule, ever; it's only meant to eliminate incompatible servers at this level
				// @todo Steam: Perhaps it would be useful or informative to have added though?
				bAddServer = FALSE;
			}


			if (bAddServer)
			{
				QueryObj->GetRules().Set(Rule, Value);
			}
		}
	}
	else
	{
		FInternetIpAddr FailAddr;

		FailAddr.SetIp((DWORD)QueryObj->GetAddr());
		FailAddr.SetPort((DWORD)QueryObj->GetPort());

		// @todo Steam: Need to test this and see that ip/port/uid are always properly set for server
		//			(uid not getting set sometimes is normal)
		debugf(NAME_DevOnline, TEXT("ServerRulesResponse: Rules response failed for server; IP: %s, UID: ") I64_FORMAT_TAG,
			*FailAddr.ToString(TRUE), QueryObj->GetSteamId());


		// Mark for deletion now we are done
		QueryObj->bDeleteMe = TRUE;
	}
}


/**
 * Asynchronous task for Steam, for receiving server ping responses
 */
class FOnlineAsyncTaskSteamServerPingRequest
	: public FOnlineAsyncTaskSteamBase<UOnlineGameInterfaceSteamworks>
	, public ISteamMatchmakingPingResponse
{
private:
	// @todo Steam: Perhaps migrate this same formatting to the server list response code (grabbing all these details in the online thread)

	/** The name of the server */
	FString				ServerName;

	/** The IP address of the server */
	DWORD				ServerIP;

	/** The port of the server */
	INT				ServerPort;

	/** The query port of the server */
	INT				ServerQueryPort;

	/** The returned ping result */
	INT				ServerPing;

	/** Whether or not the server previously responded successfully in the past */
	UBOOL				bPastResponseSuccess;

	/** Server is not responding and shouldn't be refreshed in future queries */
	UBOOL				bNotResponding;

	/** The game dir of the server (SteamAPI identification of the game/mod the server is based on) */
	FString				ServerGameDir;

	/** The map the server is on */
	FString				ServerMap;

	/** The server game description (?) */
	FString				ServerGameDescription;

	/** The appid of the game the server is running */
	DWORD				ServerAppID;

	/** The number of players on the server */
	INT				PlayerCount;

	/** The maximum number of players that the server can hold */
	INT				MaxPlayerCount;

	/** The number of bots on the server */
	INT				BotCount;

	/** Whether or not the server is passworded */
	UBOOL				bPassworded;

	/** Whether or not VAC is enabled on the server */
	UBOOL				bSecure;

	/** Last time (in unix time) the server was played on (only valid for favourite/history servers) */
	DWORD				LastPlayedTime;

	/** The server version (as determined by SteamAPI matchmaking version field) */
	INT				ServerVersion;

	/** The game tags set by the server */
	FString				ServerTags;

	/** The SteamID of the game server (may be invalid) */
	QWORD				ServerUID;


	/** Whether or not the SteamAPI is done with this object, and it is ready for deletion */
	UBOOL				bMarkedForDelete;

	/** Whether or not the async task manager is done with this item */
	UBOOL				bTaskComplete;


	/** The cached game settings for the server */
	UOnlineGameSettings*		Settings;


public:
	// @todo Steam: Un-fudge this when you do the server browser overhaul (two delete variables is confusing)

	/** Used to flag for future deletion, since this can't deleted during callback (NOTE: Keep it this way, despite threaded callbacks) */
	/** NOTE: This is part of the server browser cleanup code "this object is waiting for cleanup", whereas bMarkedForDelete handles the
			actual deletion of the object itself */
	UBOOL				bDeleteMe;


private:
	/** Hidden constructor */
	FOnlineAsyncTaskSteamServerPingRequest()
		: ServerIP(0)
		, ServerPort(0)
		, ServerQueryPort(0)
		, ServerPing(0)
		, bPastResponseSuccess(FALSE)
		, bNotResponding(FALSE)
		, ServerAppID(0)
		, PlayerCount(0)
		, MaxPlayerCount(0)
		, BotCount(0)
		, bPassworded(FALSE)
		, bSecure(FALSE)
		, LastPlayedTime(0)
		, ServerVersion(0)
		, ServerUID(0)
		, bMarkedForDelete(FALSE)
		, bTaskComplete(FALSE)
		, Settings(NULL)
		, bDeleteMe(FALSE)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InGameInterface		The game interface object this task is linked to
	 * @param InSettings			Tracks the settings object associated with the server, for storing rules and server details
	 */
	FOnlineAsyncTaskSteamServerPingRequest(UOnlineGameInterfaceSteamworks* InGameInterface, UOnlineGameSettings* InSettings)
		: FOnlineAsyncTaskSteamBase(InGameInterface)
		, ServerIP(0)
		, ServerPort(0)
		, ServerQueryPort(0)
		, ServerPing(0)
		, bPastResponseSuccess(FALSE)
		, bNotResponding(FALSE)
		, ServerAppID(0)
		, PlayerCount(0)
		, MaxPlayerCount(0)
		, BotCount(0)
		, bPassworded(FALSE)
		, bSecure(FALSE)
		, LastPlayedTime(0)
		, ServerVersion(0)
		, ServerUID(0)
		, bMarkedForDelete(FALSE)
		, bTaskComplete(FALSE)
		, Settings(InSettings)
		, bDeleteMe(FALSE)
	{
		if (CallbackInterface != NULL)
		{
			CallbackInterface->ServerBrowserSearchQuery.PendingPingSearchSettings.AddUniqueItem(InSettings);
		}
	}

	/**
	 * Base virtual destructor
	 */
	virtual ~FOnlineAsyncTaskSteamServerPingRequest()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskSteamServerPingRequest completed"));
	}

	/**
	 * Whether or not the task manager should delete this item once done with it
	 *
	 * @return	Whether or not the async task manager is responsible for deleting this item
	 */
	virtual UBOOL CanDelete()
	{
		return bMarkedForDelete;
	}

	/**
	 * Whether or not this item should block other items from returning until it has completed
	 *
	 * @return	Whether or not the item should block
	 */
	virtual UBOOL IsBlocking()
	{
		return FALSE;
	}

	/**
	 * Called by the server browser when it and the SteamAPI is done with this object
	 */
	void MarkForDelete()
	{
		// The async task manager is already done with this object, delete immediately
		if (bTaskComplete)
		{
			delete this;
		}
		// The async task manager is still processing the object, let the async task manager handle deletion
		else
		{
			bMarkedForDelete = TRUE;
		}
	}


	/**
	 * Called by the SteamAPI when the server has responded to the ping request
	 * NOTE: Called on online thread
	 */
	void ServerResponded(gameserveritem_t& server)
	{
		ServerName = UTF8_TO_TCHAR(server.GetName());
		ServerIP = server.m_NetAdr.GetIP();
		ServerPort = server.m_NetAdr.GetConnectionPort();
		ServerQueryPort = server.m_NetAdr.GetQueryPort();
		ServerPing = server.m_nPing;
		bPastResponseSuccess = server.m_bHadSuccessfulResponse;
		bNotResponding = server.m_bDoNotRefresh;
		ServerGameDir = UTF8_TO_TCHAR(server.m_szGameDir);
		ServerMap = UTF8_TO_TCHAR(server.m_szMap);
		ServerGameDescription = UTF8_TO_TCHAR(server.m_szGameDescription);
		ServerAppID = server.m_nAppID;
		PlayerCount = server.m_nPlayers;
		MaxPlayerCount = server.m_nMaxPlayers;
		BotCount = server.m_nBotPlayers;
		bPassworded = server.m_bPassword;
		bSecure = server.m_bSecure;
		LastPlayedTime = server.m_ulTimeLastPlayed;
		ServerVersion = server.m_nServerVersion;
		ServerTags = UTF8_TO_TCHAR(server.m_szGameTags);
		ServerUID = server.m_steamID.ConvertToUint64();

		bWasSuccessful = TRUE;
		bIsComplete = TRUE;
	}

	/**
	 * Called by the SteamAPI when the server fails to respond to the ping request
	 * NOTE: Called on online thread
	 */
	void ServerFailedToRespond()
	{
		bWasSuccessful = FALSE;
		bIsComplete = TRUE;
	}


	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncTaskSteamBase::Finalize();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncTaskSteamBase::TriggerDelegates();


		INT Ping = 0;

		if (bWasSuccessful)
		{
			Ping = ServerPing;

			// Log the ping result
			FInternetIpAddr ServerAddr;
			INT ServerQueryPort = 0;

			ServerAddr.SetIp(ServerIP);
			ServerAddr.SetPort(ServerPort);

			debugf(NAME_DevOnline, TEXT("ServerPingRequest: Got server ping response. Server: %s (QueryPort: %i), Ping: %i"),
				*ServerAddr.ToString(TRUE), ServerQueryPort, Ping);
		}
		else
		{
			Ping = MAX_QUERY_MSEC;

			debugf(NAME_DevOnline, TEXT("ServerPingRequest: Failed to get server ping response"));
		}


		if (Settings != NULL)
		{
			Settings->PingInMs = Clamp(Ping, 0, MAX_QUERY_MSEC);

			// GameSpy calls this for every server update (and everything ignores the result).
			FAsyncTaskDelegateResults Param(S_OK);
			TriggerOnlineDelegates(CallbackInterface, CallbackInterface->FindOnlineGamesCompleteDelegates, &Param);
		}

		// Done querying server, mark for deletion
		bDeleteMe = TRUE;


		bTaskComplete = TRUE;
	}


	/**
	 * Member access functions
	 */

	UOnlineGameSettings* GetSettings() const
	{
		return Settings;
	}
};


/**
 * UOnlineGameInterfaceSteamworks implementation
 */

/**
 * Interface initialization
 *
 * @param InSubsystem	Reference to the initializing subsystem
 */
void UOnlineGameInterfaceSteamworks::InitInterface(UOnlineSubsystemSteamworks* InSubsystem)
{
	UBOOL bInFilterEngineBuild = FALSE;

	// Load config values
	GConfig->GetBool(TEXT("OnlineSubsystemSteamworks.OnlineSubsystemSteamworks"), TEXT("bFilterEngineBuild"), bInFilterEngineBuild,
				GEngineIni);

	bFilterEngineBuild = bInFilterEngineBuild;

	GConfig->GetFloat(TEXT("OnlineSubsystemSteamworks.OnlineSubsystemSteamworks"), TEXT("ServerBrowserTimeout"), ServerBrowserTimeout,
				GEngineIni);

	GConfig->GetFloat(TEXT("OnlineSubsystemSteamworks.OnlineSubsystemSteamworks"), TEXT("InviteTimeout"), InviteTimeout,
				GEngineIni);

	OwningSubsystem = InSubsystem;

	if (GSteamworksClientInitialized)
	{
		GSteamAsyncTaskManager->RegisterInterface(this);
	}
}

/**
 * Cleanup
 */
void UOnlineGameInterfaceSteamworks::FinishDestroy()
{
	CancelAllQueries(&ServerBrowserSearchQuery);
	CancelAllQueries(&InviteSearchQuery);

	if (GSteamAsyncTaskManager != NULL)
	{
		GSteamAsyncTaskManager->UnregisterInterface(this);
	}

	Super::FinishDestroy();
}


/**
 * Online session management (Create/Start/End/Destroy-OnlineGame)
 */

/**
 * Marks an online game as in progress (as opposed to being in lobby)
 *
 * @param SessionName the name of the session that is being started
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineGameInterfaceSteamworks::StartOnlineGame(FName SessionName)
{
	UBOOL Result = FALSE;

	if (IsSteamServerAvailable())
	{
		DWORD Return = E_FAIL;

		if (GameSettings != NULL && SessionInfo != NULL)
		{
			// Lan matches don't report starting to external services
			if (GameSettings->bIsLanMatch == FALSE)
			{
				// Can't start a match multiple times
				if (GameSettings->GameState == OGS_Pending || GameSettings->GameState == OGS_Ended)
				{
					Return = StartInternetGame();
				}
				else
				{
					debugf(NAME_Error, TEXT("Can't start an online game in state %i"), GameSettings->GameState);
				}
			}
			else
			{
				// If this lan match has join in progress disabled, shut down the beacon
				if (GameSettings->bAllowJoinInProgress == FALSE)
				{
					StopLanBeacon();
				}

				Return = S_OK;
			}

			// Update the game state if successful
			if (Return == S_OK)
			{
				GameSettings->GameState = OGS_InProgress;
			}
		}
		else
		{
			debugf(NAME_Error, TEXT("Can't start an online game that hasn't been created"));
		}

		// Indicate that the start completed (NOTE: There is no asynchronous return for StartInternetGame, it is immediate)
		FAsyncTaskDelegateResultsNamedSession Params(SessionName, Return);
		TriggerOnlineDelegates(this, StartOnlineGameCompleteDelegates, &Params);

		Result = (Return == S_OK);
	}
	else
	{
		Result = Super::StartOnlineGame(SessionName);
	}

	return Result;
}

/**
 * Marks an online game as having been ended
 *
 * @param SessionName the name of the session being ended
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineGameInterfaceSteamworks::EndOnlineGame(FName SessionName)
{
	UBOOL Result = FALSE;

	if (IsSteamServerAvailable())
	{
		DWORD Return = E_FAIL;

		if (GameSettings != NULL && SessionInfo != NULL)
		{
			if (GameSettings->bIsLanMatch == FALSE)
			{
				// Can't end a match that isn't in progress
				if (GameSettings->GameState == OGS_InProgress)
				{
					Return = EndInternetGame();
				}
				else
				{
					debugf(NAME_DevOnline, TEXT("Can't end an online game in state %i"), GameSettings->GameState);
				}
			}
			else
			{
				Return = S_OK;

				// If the session should be advertised and the lan beacon was destroyed, recreate
				if (GameSettings->bShouldAdvertise && LanBeacon == NULL)
				{
					// Recreate the beacon
					Return = StartLanBeacon();
				}
			}

			GameSettings->GameState = OGS_Ended;
		}
		else
		{
			debugf(NAME_Error, TEXT("Can't end an online game that hasn't been created"));
		}

		// Trigger the delegates (NOTE: There is no asynchronous return for EndOnlineGame, it is immediate)
		FAsyncTaskDelegateResultsNamedSession Params(SessionName, Return);
		TriggerOnlineDelegates(this, EndOnlineGameCompleteDelegates, &Params);

		Result = (Return == S_OK);
	}
	else
	{
		Result = Super::EndOnlineGame(SessionName);
	}

	return Result;
}

// @todo Steam: we should probably just make CreateInternetGame and CreateLanGame virtual in the superclass.
/**
 * Creates an online game based upon the settings object specified.
 * NOTE: online game registration is an async process and does not complete
 * until the OnCreateOnlineGameComplete delegate is called.
 *
 * @param HostingPlayerNum the index of the player hosting the match
 * @param SessionName the name of the session being created
 * @param NewGameSettings the settings to use for the new game session
 *
 * @return true if successful creating the session, false otherwsie
 */
UBOOL UOnlineGameInterfaceSteamworks::CreateOnlineGame(BYTE HostingPlayerNum, FName SessionName, UOnlineGameSettings* NewGameSettings)
{
	UBOOL Result = FALSE;

	check(OwningSubsystem && "Was this object created and initialized properly?");

	DWORD Return = E_FAIL;

	// Don't set if we already have a session going
	if (GameSettings == NULL)
	{
		GameSettings = NewGameSettings;

		if (GameSettings != NULL)
		{
			check(SessionInfo == NULL);

			// Allow for per platform override of the session info
			SessionInfo = CreateSessionInfo();

			// Init the game settings counts so the host can use them later
			GameSettings->NumOpenPrivateConnections = GameSettings->NumPrivateConnections;
			GameSettings->NumOpenPublicConnections = GameSettings->NumPublicConnections;

			// Copy the unique id of the owning player
			GameSettings->OwningPlayerId = OwningSubsystem->eventGetPlayerUniqueNetIdFromIndex(HostingPlayerNum);

			// Copy the name of the owning player
			GameSettings->OwningPlayerName =
					AGameReplicationInfo::StaticClass()->GetDefaultObject<AGameReplicationInfo>()->ServerName;

			if (GameSettings->OwningPlayerName.Len() == 0)
			{
				debugf(TEXT("Warning!!! ServerName is not set in .ini file!"));
				GameSettings->OwningPlayerName = OwningSubsystem->eventGetPlayerNicknameFromIndex(HostingPlayerNum);
			}

			if (GameSettings->OwningPlayerName.Len() == 0)
			{
				debugf(TEXT("Warning!!! Backup server name not available, set ServerName in .ini file!"));
				GameSettings->OwningPlayerName = TEXT("Server name not set");
			}

			// Determine if we are registering a session on our master server or via lan
			if (!GameSettings->bIsLanMatch)
			{
				Return = CreateInternetGame(HostingPlayerNum);
			}
			else
			{
				// Lan match so do any beacon creation
				Return = CreateLanGame(HostingPlayerNum);
			}

			// Update the game state if successful
			if (Return == S_OK)
			{
				GameSettings->GameState = OGS_Pending;
			}
		}
		else
		{
			debugf(NAME_Error, TEXT("Can't create an online session with null game settings"));
		}
	}
	else
	{
		debugf(NAME_Error, TEXT("Can't create a new online session when one is in progress: %s"), *(GameSettings->GetPathName()));
	}

	// Trigger the delegates (NOTE: CreateOnlineGame is not asynchronous, it returns immediately)
	FAsyncTaskDelegateResultsNamedSession Params(SessionName, Return);
	TriggerOnlineDelegates(this, CreateOnlineGameCompleteDelegates, &Params);

	Result = (Return == S_OK);

	return Result;
}

/**
 * Creates a new Steamworks enabled game and registers it with the backend
 *
 * @param HostingPlayerNum the player hosting the game
 *
 * @return S_OK if it succeeded, otherwise an Error code
 */
DWORD UOnlineGameInterfaceSteamworks::CreateInternetGame(BYTE HostingPlayerNum)
{
	check(SessionInfo);

	DWORD Return = E_FAIL;

	// Don't try to create the session if the network device is broken
	if (GSocketSubsystem->HasNetworkDevice())
	{
		if (GameSettings != NULL)
		{
			// If launching a server as a client, only advertise on Steam if the Steam Client is running
			if ((!GIsClient || IsSteamClientAvailable()) && IsSteamServerAvailable() && GameSettings->bShouldAdvertise &&
				GameSettings->NumPublicConnections > 0)
			{
				UBOOL bUseVAC = FALSE;
				GConfig->GetBool(TEXT("OnlineSubsystemSteamworks.OnlineSubsystemSteamworks"), TEXT("bUseVAC"), bUseVAC, GEngineIni);

				debugf(NAME_DevOnline, TEXT("Steam server wants VAC: %i"), GameSettings->bAntiCheatProtected);

				// Since SteamGameServer_Init happens so early on, and can only init once, VAC status can't be modified
				if (bUseVAC != GameSettings->bAntiCheatProtected)
				{
					debugf(NAME_DevOnline,
						TEXT("WARNING!!! VAC mode set through bUseVac in .ini; can't be modified ingame. VAC status: %i"),
						bUseVAC);
				}


				if (PublishSteamServer())
				{
					Return = S_OK;
				}
			}
			else
			{
				// Creating a private match so indicate ok
				debugf(NAME_DevOnline, TEXT("Creating a private match (not registered with Steam)"));
				Return = S_OK;
			}

			if (Return == S_OK)
			{
				GameSettings->GameState = OGS_Pending;
			}
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("CreateInternetGame: GameSettings == NULL"));
		}
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("Can't create an Internet game without a network connection"));
	}

	if (Return == S_OK)
	{
		// Register all local talkers
		RegisterLocalTalkers();
	}

	return Return;
}

/**
 * Starts the specified internet enabled game
 *
 * @return S_OK if it succeeded, otherwise an Error code
 */
DWORD UOnlineGameInterfaceSteamworks::StartInternetGame()
{
	DWORD Return = E_FAIL;

	// Don't try to search if the network device is broken
	if (GSocketSubsystem->HasNetworkDevice())
	{
		// Enable the stats session
		UOnlineSubsystemSteamworks* OnlineSub = CastChecked<UOnlineSubsystemSteamworks>(OwningSubsystem);
		OnlineSub->bIsStatsSessionOk = TRUE;

		GameSettings->GameState = OGS_InProgress;
		Return = S_OK;
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Can't start an internet game without a network connection"));
	}

	return Return;
}

/**
 * Ends the specified internet enabled game
 *
 * @return S_OK if it succeeded, otherwise an Error code
 */
DWORD UOnlineGameInterfaceSteamworks::EndInternetGame()
{
	DWORD Return = E_FAIL;

	// Don't try to search if the network device is broken
	if (GSocketSubsystem->HasNetworkDevice())
	{
		GameSettings->GameState = OGS_Ended;
		CastChecked<UOnlineSubsystemSteamworks>(OwningSubsystem)->FlushOnlineStats(FName(TEXT("Game")));
		Return = S_OK;
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Can't end an internet game without a network connection"));
	}

	return Return;
}

// @todo Steam: Just make Destroy*Game() virtual in the superclass.
/**
 * Destroys the current online game
 * NOTE: online game de-registration is an async process and does not complete
 * until the OnDestroyOnlineGameComplete delegate is called.
 *
 * @param SessionName the name of the session being destroyed
 *
 * @return true if successful destroying the session, false otherwise
 */
UBOOL UOnlineGameInterfaceSteamworks::DestroyOnlineGame(FName SessionName)
{
	UBOOL Result = FALSE;

	if (IsSteamServerAvailable())
	{
		DWORD Return = E_FAIL;

		// Don't shut down if it isn't valid
		if (GameSettings != NULL && SessionInfo != NULL)
		{
			// Stop all local talkers (avoids a debug runtime warning)
			UnregisterLocalTalkers();

			// Stop all remote voice before ending the session
			RemoveAllRemoteTalkers();

			// Determine if this is a lan match or our master server
			if (GameSettings->bIsLanMatch == FALSE)
			{
				Return = DestroyInternetGame();
			}
			else
			{
				Return = DestroyLanGame();
			}

			// NOTE: GameState is not updated here, as GameSettings is NULL'd in DestroyInternetGame
		}
		else
		{
			debugf(NAME_Error,TEXT("Can't destroy a null online session"));
		}


		// Fire the delegates off (NOTE: There is no asynchronous return for this with Steam, it is immediate)
		FAsyncTaskDelegateResultsNamedSession Params(SessionName, Return);
		TriggerOnlineDelegates(this, DestroyOnlineGameCompleteDelegates, &Params);

		Result = (Return == S_OK);
	}
	else
	{
		Result = Super::DestroyOnlineGame(SessionName);
	}

	return Result;
}

/**
 * Terminates a Steamworks session, removing it from the Steamworks backend
 *
 * @return an Error/success code
 */
DWORD UOnlineGameInterfaceSteamworks::DestroyInternetGame()
{
	check(SessionInfo);

	UOnlineSubsystemSteamworks* OnlineSub = CastChecked<UOnlineSubsystemSteamworks>(OwningSubsystem);

	// Update the stats flag if present
	OnlineSub->bIsStatsSessionOk = FALSE;

	if (GSteamGameServer != NULL)
	{
		GSteamGameServer->EnableHeartbeats(false);

		// Removed during transition to 1.17 SteamSDK; do not think this needs to be replaced with LogOff,
		//	as the server still appears to be correctly delisted after calling DestroyOnlineGame
		//GSteamGameServer->NotifyShutdown();
	}

	// Clean up before firing the delegate
	delete (FSessionInfoSteam*)SessionInfo;
	SessionInfo = NULL;

	// Null out the no longer valid game settings
	GameSettings = NULL;

	// NOTE: Removed SteamGameServer_Shutdown from here, as that interface is valid for the lifetime of the process;
	//		it doesn't need to be reinitialized each game session

	// NOTE: No GSteamGameServer* interfaces are NULL'd anymore, once initialized

	return S_OK;
}

/**
 * Sets up the Steam 'game server' interfaces, and begins advertisement of the online game
 * NOTE: This has crossover between advertisement and online session management
 */
UBOOL UOnlineGameInterfaceSteamworks::PublishSteamServer()
{
	UBOOL bReturnVal = FALSE;

	// NOTE: The game server interfaces are now persistent through the life of the process, and only master server advertising is enabled/disabled
	//		by online session management; a lot of code for on-demand setup of game server interfaces has been removed here
	if (IsSteamServerAvailable())
	{
		debugf(TEXT("Initializing Steam game server"));

		FString ProductName, GameDir;

		// Dev's with source access should hard-code the product name in OnlineSubsystemSteamworks.h; UDK devs must set it in DefaultEngineUDK.ini
#if UDK
		GConfig->GetString(TEXT("OnlineSubsystemSteamworks.OnlineSubsystemSteamworks"), TEXT("ProductName"), ProductName, GEngineIni);
#else
		ProductName = STEAM_PRODUCT_NAME;
#endif

		GConfig->GetString(TEXT("OnlineSubsystemSteamworks.OnlineSubsystemSteamworks"), TEXT("GameDir"), GameDir, GEngineIni);


		// Don't spam the ProductName notification
		static UBOOL bProductError = FALSE;

		if (!bProductError)
		{
			bProductError = TRUE;

			if (ProductName.Len() == 0)
			{
				debugf(TEXT("Game doesn't have a ProductName set. To advertise on master server, game needs a product name from Valve"));
			}

			if (GameDir.Len() == 0)
			{
				debugf(TEXT("[OnlineSubsystemSteamworks.OnlineSubsystemSteamworks].GameDir is not set. Server advertising will fail"));
				return FALSE;
			}
		}


		GSteamGameServer->SetProduct(TCHAR_TO_UTF8(*ProductName));
		GSteamGameServer->SetModDir(TCHAR_TO_UTF8(*GameDir));

		UOnlineSubsystemSteamworks* OnlineSub = Cast<UOnlineSubsystemSteamworks>(OwningSubsystem);
		UBOOL bServerLoggedOn = GSteamGameServer->BLoggedOn();

		// Notify the auth interface
		if (OnlineSub->CachedAuthInt != NULL)
		{
			// We need to notify the auth interface that the server is ready to authenticate players;
			//	normally this happens in FOnlineAsyncEventSteamServersConnectedGameServer,
			//	but if GSteamGameServer is already initialized, that is not called
			if (bServerLoggedOn)
			{
				OnlineSub->CachedAuthInt->NotifyGameServerAuthReady();
			}
			// If awaiting initialization, mark the auth interface as not ready
			else
			{
				OnlineSub->CachedAuthInt->bAuthReady = FALSE;
			}
		}

		// NOTE: Removed call to FSteamSocketsManager::InitGameServer, as that's handled in FOnlineAsyncEventSteamServersConnectedGameServer

		RefreshPublishedGameSettings();


		if (!bServerLoggedOn)
		{
			// @todo Steam: Add .ini settings for logging on with named account

			// Login the server with Steam (must happen after settings are updated in RefreshPublishedGameSettings)
			GSteamGameServer->LogOnAnonymous();


			// Setup advertisement and force the initial update
			GSteamGameServer->SetHeartbeatInterval(-1);
			GSteamGameServer->EnableHeartbeats(true);

			GSteamGameServer->ForceHeartbeat();
		}
		else
		{
			// If we are already logged in, update the session info IP and SteamId
			UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
			UBOOL bSteamSockets = IsSteamSocketsServer();
			AWorldInfo* WI = (GWorld != NULL ? GWorld->GetWorldInfo() : NULL);

			// If this is a listen server, set the session info indirectly while setting the invite info
			if (WI != NULL && WI->NetMode == NM_ListenServer)
			{
				OnlineSub->SetGameJoinInfo(GSteamGameServer->GetPublicIP(), SessionInfo->HostAddr.GetPort(),
								SteamGameServer_GetSteamID(), bSteamSockets);
			}
			else
			{
				UpdateSessionInfo(GSteamGameServer->GetPublicIP(), SessionInfo->HostAddr.GetPort(), SteamGameServer_GetSteamID(),
							bSteamSockets);
			}


			// Setup advertisement
			GSteamGameServer->SetHeartbeatInterval(-1);
			GSteamGameServer->EnableHeartbeats(true);
		}


		bReturnVal = TRUE;
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("Failed to initialize game server with Steam!"));
	}

	return bReturnVal;
}

/**
 * Updates the session info data
 *
 * @param ServerIP	The server IP to set
 * @param ServerPort	The server port to set
 * @param ServerUID	The server UID to set
 * @param bSteamSockets	Whether or not the server is using steam sockets
 */
void UOnlineGameInterfaceSteamworks::UpdateSessionInfo(DWORD ServerIP, INT ServerPort, QWORD ServerUID, UBOOL bSteamSockets)
{
	FSessionInfoSteam* CurSessionInfo = (FSessionInfoSteam*)SessionInfo;

	if (CurSessionInfo != NULL)
	{
		CurSessionInfo->HostAddr.SetIp(ServerIP);
		CurSessionInfo->HostAddr.SetPort(ServerPort);
		CurSessionInfo->ServerUID = ServerUID;
		CurSessionInfo->bSteamSockets = bSteamSockets;

		debugf(NAME_DevOnline, TEXT("Updating session info, address: %s, ServerUID: ") I64_FORMAT_TAG TEXT(", bSteamSockets: %i"),
			*CurSessionInfo->HostAddr.ToString(TRUE), ServerUID, (INT)bSteamSockets);
	}
}


/**
 * Server advertisement
 */

/**
 * Determines if a settings object's UProperty should be advertised via Steamworks.
 *
 * @param PropertyName the property to check
 */
static inline UBOOL ShouldAdvertiseUProperty(FName PropertyName)
{
	if (FName(TEXT("NumPublicConnections"), FNAME_Find) == PropertyName)
	{
		return TRUE;
	}

	if (FName(TEXT("bUsesStats"), FNAME_Find) == PropertyName)
	{
		return TRUE;
	}

	if (FName(TEXT("bIsDedicated"), FNAME_Find) == PropertyName)
	{
		return TRUE;
	}

	if (FName(TEXT("OwningPlayerName"), FNAME_Find) == PropertyName)
	{
		return TRUE;
	}

	return FALSE;
}

static inline void SetRule(const TCHAR *Key, const TCHAR *Value)
{
	debugf(NAME_DevOnline,TEXT("Advertising: %s=%s"), Key, Value);
	GSteamGameServer->SetKeyValue(TCHAR_TO_UTF8(Key), TCHAR_TO_UTF8(Value));
}

/**
 * Refreshes values advertised to Steam, that may change during the course of a game
 */
void UOnlineGameInterfaceSteamworks::RefreshPublishedGameSettings()
{
	if (GSteamGameServer == NULL)
	{
		return;
	}


	debugf(NAME_DevOnline, TEXT("Refreshing published game settings..."));

	FString Region, GameName;

	GConfig->GetString(TEXT("OnlineSubsystemSteamworks.OnlineSubsystemSteamworks"), TEXT("Region"), Region, GEngineIni);
	GConfig->GetString(TEXT("URL"), TEXT("GameName"), GameName, GEngineIni);

	if (Region.Len() == 0)
	{
		Region = TEXT("255");
	}

	INT IsLocked = 0;

	UBOOL bPassworded = false;
	UBOOL bDedicated = false;
	INT Slots = 0;
	INT NumPlayers = 0;
	INT NumBots = 0;
	FString ServerName = TEXT("");
	FString MapName = TEXT("");
	FString GameType = TEXT("");

	if (GameSettings != NULL)
	{
		// @todo Steam: Is this correct for passwords? Implement it properly, so it doesn't break later
		//	(perhaps use 'FilterKeyToSteamKeyMap', or an analagous array?)
		bPassworded = GameSettings->GetStringSettingValue(7, IsLocked) && IsLocked;  // hack from GameSpy code.
		bDedicated = GameSettings->bIsDedicated;
		Slots = GameSettings->NumPublicConnections + GameSettings->NumPrivateConnections;
		NumPlayers = Slots - (GameSettings->NumOpenPublicConnections + GameSettings->NumOpenPrivateConnections);
		ServerName = GameSettings->OwningPlayerName;


		// Grab the server name from the game settings, if specified
		static UBOOL bMustFindServerDescId = TRUE;
		static INT ServerDescId = -1;

		if (bMustFindServerDescId)
		{
			bMustFindServerDescId = FALSE;
			const FName DescName(TEXT("ServerDescription"), FNAME_Find);

			for(INT i=0; i<GameSettings->PropertyMappings.Num(); i++)
			{
				if (GameSettings->PropertyMappings(i).Name == DescName)
				{
					ServerDescId = GameSettings->PropertyMappings(i).Id;
					break;
				}
			}
		}

		if (ServerDescId != -1)
		{
			// Try to use the ServerDescription here instead of the OwningPlayerName
			for(INT i=0; i<GameSettings->Properties.Num(); i++)
			{
				const INT PropertyId = GameSettings->Properties(i).PropertyId;

				if (PropertyId == ServerDescId)
				{
					const FString ServerDesc = GameSettings->Properties(i).Data.ToString();

					if (ServerDesc.Len() > 0)
					{
						ServerName = ServerDesc;
					}

					break;
				}
			}
		}
	}

	if (GWorld != NULL)
	{
		if (GWorld->CurrentLevel != NULL)
		{
			MapName = GWorld->CurrentLevel->GetOutermost()->GetName();
		}

		AGameInfo* CurGameInfo = GWorld->GetGameInfo();

		if (CurGameInfo != NULL)
		{
			NumBots = CurGameInfo->NumBots;
			GameType = CurGameInfo->GetClass()->GetName();
		}


		// Report player info.
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		AGameReplicationInfo* GRI = (WorldInfo != NULL ? WorldInfo->GRI : NULL);

		if (GRI != NULL)
		{
			for (INT i=0; i<GRI->PRIArray.Num(); i++)
			{
				APlayerReplicationInfo* PRI = GRI->PRIArray(i);

				if (PRI != NULL)
				{
					GSteamGameServer->BUpdateUserData(CSteamID((uint64)PRI->UniqueId.Uid), TCHAR_TO_UTF8(*PRI->PlayerName),
										(uint32)PRI->Score);
				}
			}
		}
	}


	INT Version = GEngineMinNetVersion;


	debugf(NAME_DevOnline,
		TEXT("Server data: Ver: %i, Ded: %i, Region: %s, Slots: %i, Pass: %i, Server: %s, Map: %s, Players: %i Bots: %i, Game: %s"),
		Version, bDedicated, *Region, Slots, bPassworded, *ServerName, *MapName, NumPlayers, NumBots, *GameType);

	GSteamGameServer->SetDedicatedServer(bDedicated == TRUE);

	// @todo Steam: Test/verify this (supposed to adjust gametype in steam server browser column); can't atm though, as no changeable game in browser
	GSteamGameServer->SetGameDescription(TCHAR_TO_UTF8(*GameType));

	GSteamGameServer->SetGameTags(TCHAR_TO_UTF8(*GameType));
	GSteamGameServer->SetMaxPlayerCount(Slots);
	GSteamGameServer->SetBotPlayerCount(NumBots);
	GSteamGameServer->SetServerName(TCHAR_TO_UTF8(*ServerName));
	GSteamGameServer->SetMapName(TCHAR_TO_UTF8(*MapName));
	GSteamGameServer->SetPasswordProtected(bPassworded == TRUE);
	GSteamGameServer->SetRegion(TCHAR_TO_UTF8(*Region));

	// @todo Steam: You should check 'SetGameData' and implement that, as it seems useful


	// Set the advertised filter keys (these can not be filtered at master-server level, only be clientside filters)
	GSteamGameServer->ClearAllKeyValues();


	SetRule(TEXT("SteamEngineVersion"), *FString::Printf(TEXT("%i"), GEngineVersion));

#if WITH_STEAMWORKS_SOCKETS
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);

	// If this server is using steam sockets, advertise the server steamid
	if (GSteamGameServer != NULL && IsSteamSocketsServer())
	{
		SetRule(TEXT("SteamServerId"), *FString::Printf(I64_FORMAT_TAG, SteamGameServer_GetSteamID()));
	}
#endif

	if (GameSettings != NULL)
	{
		SetRule(TEXT("OwningPlayerId"), *FString::Printf(I64_FORMAT_TAG, GameSettings->OwningPlayerId.Uid));


		// Go through all game settings values, and add them to the filter key map

		// Databindable properties
		for (UProperty* Prop=GameSettings->GetClass()->PropertyLink; Prop!=NULL; Prop=Prop->PropertyLinkNext)
		{
			// Skip object properties
			if ((Prop->PropertyFlags & CPF_DataBinding) != 0 &&
				Cast<UObjectProperty>(Prop, CLASS_IsAUObjectProperty) == NULL &&
				ShouldAdvertiseUProperty(Prop->GetFName()))
			{
				const BYTE* ValueAddress = (BYTE*)GameSettings + Prop->Offset;
				FString StringValue;

				Prop->ExportTextItem(StringValue, ValueAddress, NULL, GameSettings, Prop->PropertyFlags & PPF_Localized);
				SetRule(*Prop->GetName(), *StringValue);
			}
		}

		// Mapped settings
		for (INT i=0; i<GameSettings->LocalizedSettings.Num(); i++)
		{
			const FString SettingName(FString::Printf(TEXT("s%i"), GameSettings->LocalizedSettings(i).Id));
			const FString SettingValue(FString::Printf(TEXT("%i"), GameSettings->LocalizedSettings(i).ValueIndex));

			SetRule(*SettingName, *SettingValue);
		}

		// Mapped properties
		for (INT i=0; i<GameSettings->Properties.Num(); i++)
		{
			const FString PropertyName(FString::Printf(TEXT("p%i"), GameSettings->Properties(i).PropertyId));
			const FString PropertyValue(GameSettings->Properties(i).Data.ToString());

			SetRule(*PropertyName, *PropertyValue);
		}
	}


	// Force a master server heartbeat to update server values on the master server (important for master server filtering)
	if (GSteamGameServer->BLoggedOn())
	{
		GSteamGameServer->ForceHeartbeat();
	}
}

void UOnlineGameInterfaceSteamworks::OnGSPolicyResponse(const UBOOL bIsVACSecured)
{
	if (GameSettings != NULL)
	{
		GameSettings->bAntiCheatProtected = bIsVACSecured;
	}

	RefreshPublishedGameSettings();
}

/**
 * Updates the localized settings/properties for the game in question
 *
 * @param SessionName the name of the session to update
 * @param UpdatedGameSettings the object to update the game settings with
 * @param ignored Steam always needs to be updated
 *
 * @return true if successful creating the session, false otherwsie
 */
UBOOL UOnlineGameInterfaceSteamworks::UpdateOnlineGame(FName SessionName,UOnlineGameSettings* UpdatedGameSettings,UBOOL Ignored)
{
	// Don't try to without a network device
	if (GSocketSubsystem->HasNetworkDevice())
	{
		if (UpdatedGameSettings != NULL)
		{
			GameSettings = UpdatedGameSettings;
			RefreshPublishedGameSettings();
		}
		else
		{
			debugf(NAME_Error,TEXT("Can't update game settings with a NULL object"));
		}
	}
	FAsyncTaskDelegateResultsNamedSession Results(SessionName,0);
	TriggerOnlineDelegates(this,UpdateOnlineGameCompleteDelegates,&Results);
	return TRUE;
}


/**
 * Server browser querying (FindOnlineGames etc.)
 */

/**
 * Shut down all Steam HServerQuery handles that might be in-flight
 *
 * @param SearchQueryState	The matchmaking query state containing the queries to be cancelled
 */
void UOnlineGameInterfaceSteamworks::CancelAllQueries(FMatchmakingQueryState* SearchQueryState)
{
	if (GSteamMatchmakingServers == NULL)
	{
		return;
	}


	// Stop any pending queries, so we don't callback into a destroyed object
	if (SearchQueryState->CurrentMatchmakingQuery != NULL)
	{
		// Need to temporarily ignore callbacks triggered by CancelQuery
		SearchQueryState->bIgnoreRefreshComplete = TRUE;

		GSteamMatchmakingServers->CancelQuery(SearchQueryState->CurrentMatchmakingQuery);
		GSteamMatchmakingServers->ReleaseRequest(SearchQueryState->CurrentMatchmakingQuery);
		SearchQueryState->CurrentMatchmakingType = SMT_Invalid;
		SearchQueryState->CurrentMatchmakingQuery = NULL;

		SearchQueryState->bIgnoreRefreshComplete = FALSE;
	}

	if (SearchQueryState->ServerListResponse != NULL)
	{
		SearchQueryState->ServerListResponse->MarkForDelete();
		SearchQueryState->ServerListResponse = NULL;
	}


	for (INT i=0; i<SearchQueryState->QueryToRulesResponseMap.Num(); i++)
	{
		HServerQuery Query = (HServerQuery)SearchQueryState->QueryToRulesResponseMap(i).Query;

		FOnlineAsyncTaskSteamServerRulesRequest* Response = SearchQueryState->QueryToRulesResponseMap(i).Response;

		GSteamMatchmakingServers->CancelServerQuery(Query);
		Response->MarkForDelete();
	}

	SearchQueryState->QueryToRulesResponseMap.Empty();

	for (INT i=0; i<SearchQueryState->QueryToPingResponseMap.Num(); i++)
	{
		HServerQuery Query = (HServerQuery)SearchQueryState->QueryToPingResponseMap(i).Query;

		FOnlineAsyncTaskSteamServerPingRequest* Response = SearchQueryState->QueryToPingResponseMap(i).Response;

		GSteamMatchmakingServers->CancelServerQuery(Query);
		Response->MarkForDelete();
	}

	SearchQueryState->QueryToPingResponseMap.Empty();


	if (SearchQueryState->GameSearch != NULL)
	{
		SearchQueryState->GameSearch->bIsSearchInProgress = FALSE;
	}

	SearchQueryState->PendingRulesSearchSettings.Empty();
	SearchQueryState->PendingPingSearchSettings.Empty();
}

/**
 * Starts process to add a Server to the search results
 *
 * The server isn't actually added here, since we still need to obtain metadata about it, but
 * this method kicks that off
 *
 * @param SearchQueryState	The matchmaking query state the result is from
 * @param Server		The SteamAPI server data to be formatted into the search results
 */
void UOnlineGameInterfaceSteamworks::AddServerToSearchResults(FMatchmakingQueryState* SearchQueryState, gameserveritem_t* Server)
{
	if (SearchQueryState->GameSearch->Results.Num() >= SearchQueryState->GameSearch->MaxSearchResults)
	{
		debugf(NAME_DevOnline, TEXT("Got enough search results, stopping query."));

		if (SearchQueryState->CurrentMatchmakingQuery != NULL)
		{
			GSteamMatchmakingServers->CancelQuery(SearchQueryState->CurrentMatchmakingQuery);
			GSteamMatchmakingServers->ReleaseRequest(SearchQueryState->CurrentMatchmakingQuery);
			SearchQueryState->CurrentMatchmakingQuery = NULL;
		}

		return;
	}

	if (Server->m_bDoNotRefresh)
	{
		return;
	}

	// Not this game
	if (Server->m_nAppID != GSteamAppID)
	{
		debugf(NAME_DevOnline, TEXT("Got server with mismatched appid '%i' in search results"), Server->m_nAppID);
		return;
	}

	const uint32 IpAddr = Server->m_NetAdr.GetIP();
	const uint16 ConnectionPort = Server->m_NetAdr.GetConnectionPort();
	const uint16 QueryPort = Server->m_NetAdr.GetQueryPort();

	// Make sure we haven't processed it
	// @todo Steam: Have a closer look at this eventually; if the log you added below is never triggered, remove this code
	for (INT i=0; i<SearchQueryState->GameSearch->Results.Num(); i++)
	{
		const FOnlineGameSearchResult& Result = SearchQueryState->GameSearch->Results(i);
		const FInternetIpAddr& HostAddr = ((FSessionInfoSteam*)Result.PlatformData)->HostAddr;
		DWORD Addr;

		HostAddr.GetIp(Addr);

		if (Addr == IpAddr && HostAddr.GetPort() == ConnectionPort)
		{
			debugf(TEXT("Found duplicate server in search results (index '%i' address '%s'), returning"), i, *HostAddr.ToString(TRUE));
			return;
		}
	}

	// Create an object that we'll copy the data to
	UOnlineGameSettings* NewServer = ConstructObject<UOnlineGameSettings>(SearchQueryState->GameSearch->GameSettingsClass);

	if (NewServer != NULL)
	{
		NewServer->PingInMs = Clamp(Server->m_nPing,0,MAX_QUERY_MSEC);
		NewServer->bAntiCheatProtected = Server->m_bSecure ? 1 : 0;
		NewServer->NumPublicConnections = Server->m_nMaxPlayers;
		NewServer->NumOpenPublicConnections = Server->m_nMaxPlayers - Server->m_nPlayers;
		NewServer->bIsLanMatch = (SearchQueryState->CurrentMatchmakingType == SMT_LAN);
		NewServer->OwningPlayerName = UTF8_TO_TCHAR(Server->GetName());

		// We set this later by rule response. Server->m_steamID is the SERVER id, not the owner!
		NewServer->OwningPlayerId.Uid = 0;

		// If bSteamSocketsOnly is set in the net driver, assume the server uses steam sockets and set the SteamServerId value
		if (IsSteamSocketsOnly())
		{
			// If the class has a 'SteamServerId' databinding property, assign the value to that
			UProperty* SteamServerIdProp = FindField<UProperty>(NewServer->GetClass(), TEXT("SteamServerId"));

			if (SteamServerIdProp != NULL && (SteamServerIdProp->PropertyFlags & CPF_DataBinding) != 0 &&
				Cast<UObjectProperty>(SteamServerIdProp, CLASS_IsAUObjectProperty) == NULL)
			{
				BYTE* ValueAddress = (BYTE*)NewServer + SteamServerIdProp->Offset;
				FString ServerUID = FString::Printf(I64_FORMAT_TAG, Server->m_steamID.ConvertToUint64());

				SteamServerIdProp->ImportText(*ServerUID, ValueAddress, PPF_Localized, NewServer);
			}
		}


		// Only kickoff a rules search, if this is a server browser query
		if (SearchQueryState == &ServerBrowserSearchQuery)
		{
			TCHAR* ServerTypeString = (SearchQueryState->CurrentMatchmakingType == SMT_LAN) ? TEXT("LAN") : TEXT("Internet");

			debugf(NAME_DevOnline, TEXT("Querying rules data for %s server %s"), ServerTypeString,
				ANSI_TO_TCHAR(Server->m_NetAdr.GetQueryAddressString()));


			FOnlineAsyncTaskSteamServerRulesRequest* RulesResponse =
				new FOnlineAsyncTaskSteamServerRulesRequest(this, NewServer, IpAddr, ConnectionPort,
									(QWORD)Server->m_steamID.ConvertToUint64());

			GSteamAsyncTaskManager->AddToInQueue(RulesResponse);


			const HServerQuery RulesQuery = GSteamMatchmakingServers->ServerRules(IpAddr, QueryPort, RulesResponse);
			const INT MapPosition = SearchQueryState->QueryToRulesResponseMap.AddZeroed();
			FServerQueryToRulesResponseMapping& RulesMapping = SearchQueryState->QueryToRulesResponseMap(MapPosition);

			RulesMapping.Query = (INT)RulesQuery;
			RulesMapping.Response = RulesResponse;

			// We now wait for RulesResponse to fire callbacks to continue processing this server
		}
		// If this was an invite search result, we are ready to immediately switch to the server; cancel the remaining queries
		else if (SearchQueryState == &InviteSearchQuery)
		{
			FInternetIpAddr ResultAddr;

			ResultAddr.SetIp(IpAddr);
			ResultAddr.SetPort(ConnectionPort);

			if (InviteServerUID.Uid == 0 || InviteServerUID.Uid == Server->m_steamID.ConvertToUint64())
			{
				debugf(NAME_DevOnline, TEXT("Successful invite search query result; address: %s, SteamId: ") I64_FORMAT_TAG,
					*ResultAddr.ToString(TRUE), Server->m_steamID.ConvertToUint64());


				// Add the server to the results list (it will be the only one there)
				FSessionInfoSteam* ServerSessionInfo = (FSessionInfoSteam*)CreateSessionInfo();

				ServerSessionInfo->HostAddr.SetIp(IpAddr);
				ServerSessionInfo->HostAddr.SetPort(ConnectionPort);
				ServerSessionInfo->ServerUID = Server->m_steamID.ConvertToUint64();

				// NOTE: No way to set ServerSessionInfo->bSteamSockets, as you need to query the SteamServerId value to know this.
				//		Invite queries do not query rules, and there is no benefit to making them do this either
				//		(ports need to be forwarded in order to query rules)

				// However, if the net driver has bSteamSocketsOnly set, then assume it is a steam sockets server
				if (IsSteamSocketsOnly())
				{
					ServerSessionInfo->bSteamSockets = TRUE;

					if (InviteServerUID.Uid == 0)
					{
						InviteServerUID.Uid = Server->m_steamID.ConvertToUint64();
					}
				}

				INT ResultIndex = InviteGameSearch->Results.AddZeroed();

				if (ResultIndex > 0)
				{
					debugf(NAME_DevOnline, TEXT("Warning!!! More than one server added to invite search query result list"));
				}

				FOnlineGameSearchResult& ResultRef = InviteGameSearch->Results(ResultIndex);

				ResultRef.PlatformData = ServerSessionInfo;
				ResultRef.GameSettings = NewServer;


				// Now trigger the 'OnGameInviteAccepted' delegates
				OnlineGameInterfaceImpl_eventOnGameInviteAccepted_Parms Parms(EC_EventParm);
				Parms.InviteResult = ResultRef;

				/**
				 * NOTE: IMPORTANT: Because this invite search skips querying the server rules, you >CAN NOT< determine if the
				 * server is using steam sockets from here; this means, only invites using 'SendGameInviteToFriend' (which cause
				 * 'InviteServerUID' to be set) will directly use steam sockets, whereas Steam UI invites will use IP instead
				 * (however, Steam UI invites can only be sent by friends, so the higher-level code uses friend rich presence
				 * info to get the server steam sockets address).
				 *
				 * Also (again very important), there is >NO BENEFIT< to making the invite search query the server
				 * rules to try and determine if steam sockets are in use; connecting by IP will redirect to steam
				 * sockets anyway, and if you can't connect directly by IP it will fail the rules query anyway.
				 */
				TriggerOnlineDelegates(this, GameInviteAcceptedDelegates, &Parms);


				// Finally, end the current search query
				CleanupOnlineQuery(SearchQueryState, TRUE);
			}
			else
			{
				debugf(TEXT("Received invite search result with correct IP (%s), but wrong UID: ") I64_FORMAT_TAG
					TEXT(" (expected '") I64_FORMAT_TAG TEXT("')"), *ResultAddr.ToString(TRUE),
					Server->m_steamID.ConvertToUint64(), InviteServerUID.Uid);
			}
		}
	}
	else
	{
		debugf(NAME_Error, TEXT("Failed to create new online game settings object"));
	}
}

/**
 * Returns the platform specific connection information for joining the match.
 * Call this function from the delegate of join completion
 *
 * @param SessionName the name of the session to resolve
 * @param ConnectInfo the out var containing the platform specific connection information
 *
 * @return true if the call was successful, false otherwise
 */
UBOOL UOnlineGameInterfaceSteamworks::GetResolvedConnectString(FName SessionName, FString& ConnectInfo)
{
	UBOOL bSuccess = FALSE;

	// Since 'SessionName' is not used by Steam, have fudged it a bit and use a session name of 'invite' to signify invite handling code
	if (SessionName == FName(TEXT("Invite")))
	{
		if (InviteGameSearch != NULL && InviteGameSearch->Results.Num() > 0)
		{
			// Steam sockets server, give back the UID instead of the IP
			if (InviteServerUID.Uid > 0)
			{
				ConnectInfo = FString::Printf(TEXT("steam.") I64_FORMAT_TAG, InviteServerUID.Uid);
				bSuccess = TRUE;
			}
			// Not a steam sockets server, give back the IP
			else if (InviteGameSearch->Results(0).PlatformData != NULL)
			{
				FSessionInfoSteam* ServerSessionInfo = (FSessionInfoSteam*)InviteGameSearch->Results(0).PlatformData;

				ConnectInfo = ServerSessionInfo->HostAddr.ToString(TRUE);
				bSuccess = TRUE;
			}
		}
	}
	else
	{
		bSuccess = Super::GetResolvedConnectString(SessionName, ConnectInfo);
	}

	return bSuccess;
}

/**
 * Updates the server details with the new data
 *
 * @param InGameSettings the game settings to update
 * @param Server the Steamworks data to update with
 */
void UOnlineGameInterfaceSteamworks::UpdateGameSettingsData(UOnlineGameSettings* InGameSettings, const SteamRulesMap& Rules)
{
	if (InGameSettings != NULL)
	{
		// Read the owning player id
		const FString* PlayerIdValue = Rules.Find(TEXT("OwningPlayerId"));

		if (PlayerIdValue != NULL)
		{
			InGameSettings->OwningPlayerId.Uid = appAtoi64(*(*PlayerIdValue));
		}

		// Read the data bindable properties
		for (UProperty* Prop=InGameSettings->GetClass()->PropertyLink; Prop!=NULL; Prop=Prop->PropertyLinkNext)
		{
			// If the property is databindable and is not an object, we'll check for it
			if ((Prop->PropertyFlags & CPF_DataBinding) != 0 && Cast<UObjectProperty>(Prop, CLASS_IsAUObjectProperty) == NULL)
			{
				BYTE* ValueAddress = (BYTE*)InGameSettings + Prop->Offset;
				const FString& DataBindableName = Prop->GetName();
				const FString* DataBindableValue = Rules.Find(DataBindableName);

				if (DataBindableValue != NULL)
				{
					Prop->ImportText(*(*DataBindableValue), ValueAddress, PPF_Localized, InGameSettings);
				}
			}
		}

		// Read the settings
		for (INT i=0; i<InGameSettings->LocalizedSettings.Num(); i++)
		{
			INT SettingId = InGameSettings->LocalizedSettings(i).Id;
			FString SettingName(TEXT("s"));
			SettingName += appItoa(SettingId);

			const FString* SettingValue = Rules.Find(SettingName);

			if (SettingValue != NULL)
			{
				InGameSettings->LocalizedSettings(i).ValueIndex = appAtoi(*(*SettingValue));
			}
		}

		// Read the properties
		for (INT i=0; i<InGameSettings->Properties.Num(); i++)
		{
			INT PropertyId = InGameSettings->Properties(i).PropertyId;
			FString PropertyName(TEXT("p"));
			PropertyName += appItoa(PropertyId);

			const FString* PropertyValue = Rules.Find(PropertyName);

			if (PropertyValue != NULL)
			{
				InGameSettings->Properties(i).Data.FromString(*PropertyValue);
			}
		}
	}
}


// @todo Steam: we should probably just make CreateInternetGame and CreateLanGame virtual in the superclass.
/**
 * Searches for games matching the settings specified
 *
 * @param SearchingPlayerNum the index of the player searching for a match
 * @param SearchSettings the desired settings that the returned sessions will have
 *
 * @return true if successful searching for sessions, false otherwise
 */
UBOOL UOnlineGameInterfaceSteamworks::FindOnlineGames(BYTE SearchingPlayerNum, UOnlineGameSearch* SearchSettings)
{
	UBOOL Result = FALSE;

	if (IsSteamClientAvailable())
	{
		DWORD Return = E_FAIL;

		// Verify that we have valid search settings
		if (SearchSettings != NULL)
		{
			// Don't start another while in progress or multiple entries for the same server will show up in the server list
			if ((ServerBrowserSearchQuery.GameSearch != NULL && ServerBrowserSearchQuery.GameSearch->bIsSearchInProgress == FALSE) ||
				ServerBrowserSearchQuery.GameSearch == NULL)
			{
				// Free up previous results, if present
				if (SearchSettings->Results.Num())
				{
					FreeSearchResults(SearchSettings);
				}

				GameSearch = SearchSettings;
				ServerBrowserSearchQuery.GameSearch = SearchSettings;

				// Check for master server or lan query
				if (SearchSettings->bIsLanQuery == FALSE)
				{
					// Generate the filter mapping for clientside and master server filters
					TMap<FString,FString> ActiveMasterServerFilters;

					BuildServerBrowserFilterMap(ServerBrowserSearchQuery.ActiveClientsideFilters, ActiveMasterServerFilters);

#if FILTER_DUMP
					debugf(NAME_DevOnline, TEXT("Master server filters: (num: %i)"), ActiveMasterServerFilters.Num());

					for (TMap<FString,FString>::TIterator It(ActiveMasterServerFilters); It; ++It)
					{
						debugf(NAME_DevOnline, TEXT("- Key: %s, Value: %s"), *It.Key(), *It.Value());
					}
#endif

					TArray<FString> FilterKeys;
					TArray<FString> FilterValues;

					ActiveMasterServerFilters.GenerateKeyArray(FilterKeys);
					ActiveMasterServerFilters.GenerateValueArray(FilterValues);


					Return = FindInternetGames(&ServerBrowserSearchQuery, FilterKeys, FilterValues);
				}
				else
				{
					Return = FindLanGames();
				}
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("Ignoring game search request while one is pending"));
				Return = ERROR_IO_PENDING;
			}
		}
		else
		{
			debugf(NAME_Error, TEXT("Can't search with null criteria"));
		}

		if (Return != ERROR_IO_PENDING)
		{
			// Fire the delegate off immediately
			FAsyncTaskDelegateResults Params(Return);
			TriggerOnlineDelegates(this, FindOnlineGamesCompleteDelegates, &Params);
		}

		Result = (Return == S_OK || Return == ERROR_IO_PENDING);
	}
	else
	{
		Result = Super::FindOnlineGames(SearchingPlayerNum, SearchSettings);
	}

	return Result;
}

/**
 * Builds the key/value filter list for the server browser filtering
 *
 * @param OutFilterList		The destination filter list
 * @param OutMasterFilterList	The filter list for storing master-server evaluated filters
 * @param bDisableMasterFilters	whether or not to disable master-server filters (including the master server filters in the regular filter list)
 */
void UOnlineGameInterfaceSteamworks::BuildServerBrowserFilterMap(TArray<FClientFilterORClause>& OutFilterList, TMap<FString,FString>& OutMasterFilterList,
							UBOOL bDisableMasterFilters/*=FALSE*/)
{
	OutFilterList.Empty();

	// @todo Steam: At some stage, FilterQueries in OnlineGameSearch should be redone, as FilterQueries is extremely limited and complicated to
	//		implement/workaround at the moment

	// Cache the number of properties and settings
	INT NumProperties = ServerBrowserSearchQuery.GameSearch->Properties.Num();
	INT NumSettings = ServerBrowserSearchQuery.GameSearch->LocalizedSettings.Num();

	// Loop through the clauses
	INT NumClauses = ServerBrowserSearchQuery.GameSearch->FilterQuery.OrClauses.Num();

	for (INT ClauseIndex=0; ClauseIndex<NumClauses; ClauseIndex++)
	{
		FOnlineGameSearchORClause* Clause = &ServerBrowserSearchQuery.GameSearch->FilterQuery.OrClauses(ClauseIndex);
		INT NumSearchParameters = Clause->OrParams.Num();

		if (NumSearchParameters <= 0)
		{
			continue;
		}


		INT FilterIndex = OutFilterList.AddZeroed(1);

		if (FilterIndex == INDEX_NONE)
		{
			continue;
		}


		FClientFilterORClause& OutFilterClause = OutFilterList(FilterIndex);

		// Loop through the params
		for (INT ParameterIndex=0; ParameterIndex<NumSearchParameters; ParameterIndex++)
		{
			FOnlineGameSearchParameter* Parameter = &Clause->OrParams(ParameterIndex);

			FString ParameterName;
			FString ParameterValue;

			// Determine the parameter type so we can get it from the right place
			switch (Parameter->EntryType)
			{
				case OGSET_Property:
				{
					ParameterName = TEXT("p");
					ParameterName += appItoa(Parameter->EntryId);

					FSettingsProperty* Property = NULL;

					for (INT Index=0; Index<NumProperties; Index++)
					{
						if (ServerBrowserSearchQuery.GameSearch->Properties(Index).PropertyId == Parameter->EntryId)
						{
							Property = &ServerBrowserSearchQuery.GameSearch->Properties(Index);
							break;
						}
					}

					if (Property == NULL)
					{
						continue;
					}


					ParameterValue += Property->Data.ToString();

					break;
				}

				case OGSET_LocalizedSetting:
				{
					if (ServerBrowserSearchQuery.GameSearch->IsWildcardStringSetting(Parameter->EntryId))
					{
						continue;
					}

					ParameterName = TEXT("s");
					ParameterName += appItoa(Parameter->EntryId);

					FLocalizedStringSetting* Setting = NULL;

					for (INT Index=0; Index<NumSettings; Index++)
					{
						if (ServerBrowserSearchQuery.GameSearch->LocalizedSettings(Index).Id == Parameter->EntryId)
						{
							Setting = &ServerBrowserSearchQuery.GameSearch->LocalizedSettings(Index);
							break;
						}
					}

					if (Setting == NULL)
					{
						continue;
					}


					ParameterValue = appItoa(Setting->ValueIndex);

					break;
				}

				case OGSET_ObjectProperty:
				{
					ParameterName = Parameter->ObjectPropertyName.ToString();

					// Search through the properties so we can find the corresponding value
					for (INT Index=0; Index<ServerBrowserSearchQuery.GameSearch->NamedProperties.Num(); Index++)
					{
						if (ServerBrowserSearchQuery.GameSearch->NamedProperties(Index).ObjectPropertyName ==
							Parameter->ObjectPropertyName)
						{
							ParameterValue = ServerBrowserSearchQuery.GameSearch->NamedProperties(Index).ObjectPropertyValue;
							break;
						}
					}

					break;
				}
			}


			// Determine whether or not this is a master-server filter
			FString MasterKeyName = TEXT("");
			UBOOL bReverseFilter = FALSE;
			FString IgnoreValue;

			// Only the OGSCT_Equals operator and lone AND (&&) filters are supported with master-server filters
			if (!bDisableMasterFilters && Parameter->ComparisonType == OGSCT_Equals && NumSearchParameters == 1)
			{
				for (INT KeyMapIndex=0; KeyMapIndex<FilterKeyToSteamKeyMap.Num(); KeyMapIndex++)
				{
					FFilterKeyToSteamKeyMapping& CurKeyMap = FilterKeyToSteamKeyMap(KeyMapIndex);

					// If this isn't a master server filter, continue
					if (CurKeyMap.KeyType == OGSET_ObjectProperty)
					{
						if (CurKeyMap.RawKey != Parameter->ObjectPropertyName.ToString())
						{
							continue;
						}
					}
					else if (CurKeyMap.RawKey.IsEmpty())
					{
						if (CurKeyMap.KeyId != Parameter->EntryId)
						{
							continue;
						}
					}
					else
					{
						if (CurKeyMap.RawKey != ParameterName)
						{
							continue;
						}
					}

					// Passed the checks, this is a master server filter
					MasterKeyName = CurKeyMap.SteamKey;
					bReverseFilter = CurKeyMap.bReverseFilter;
					IgnoreValue = CurKeyMap.IgnoreValue;
				}
			}

			// Enter the filter into the mapping list
			if (!MasterKeyName.IsEmpty())
			{
				// Treats the filter as a bool, and flips the comparison value
				if (bReverseFilter)
				{
					ParameterValue = FString::Printf(TEXT("%i"), (INT)(ParameterValue.Trim() == TEXT("0")));
				}


				if (ParameterValue.Trim() != IgnoreValue.Trim())
				{
					OutMasterFilterList.Set(*MasterKeyName, ParameterValue);
				}
			}
			else
			{
				FSearchFilterValue CurValue(ParameterValue, Parameter->ComparisonType);
				OutFilterClause.OrParams.Add(ParameterName, CurValue);
			}
		}
	}


	// If additional criteria are set, add them
	if (!ServerBrowserSearchQuery.GameSearch->AdditionalSearchCriteria.IsEmpty())
	{
		// Parse the 'FilterKeyToSteamKeyMap' keys into raw strings, as this needs to be done for comparison here (can't compare KeyId's)
		// @todo Steam: Consolidate these into a TMultiMap, or use a TMap of the actual script struct which defines the master filters
		TMap<FString, FString> MasterKeyMap;
		TMap<FString, UBOOL> MasterKeyReverseMap;
		TMap<FString, FString> MasterKeyIgnoreMap;

		if (!bDisableMasterFilters)
		{
			for (INT KeyMapIndex=0; KeyMapIndex<FilterKeyToSteamKeyMap.Num(); KeyMapIndex++)
			{
				FFilterKeyToSteamKeyMapping& CurKeyMap = FilterKeyToSteamKeyMap(KeyMapIndex);

				if (!CurKeyMap.RawKey.IsEmpty())
				{
					MasterKeyMap.Set(CurKeyMap.RawKey, CurKeyMap.SteamKey);
					MasterKeyReverseMap.Set(CurKeyMap.RawKey, CurKeyMap.bReverseFilter);
					MasterKeyIgnoreMap.Set(CurKeyMap.RawKey, CurKeyMap.IgnoreValue);
				}
				else
				{
					FString CurKey = TEXT("");

					switch (CurKeyMap.KeyType)
					{
					case OGSET_Property:
						CurKey = TEXT("p");
						CurKey += appItoa(CurKeyMap.KeyId);

						break;

					case OGSET_LocalizedSetting:
						CurKey = TEXT("s");
						CurKey += appItoa(CurKeyMap.KeyId);

						break;

					case OGSET_ObjectProperty:
						CurKey = CurKeyMap.RawKey;
						break;

					default:
						break;
					}

					MasterKeyMap.Set(CurKey, CurKeyMap.SteamKey);
					MasterKeyReverseMap.Set(CurKey, CurKeyMap.bReverseFilter);
					MasterKeyIgnoreMap.Set(CurKey, CurKeyMap.IgnoreValue);
				}
			}
		}


		// Format of AdditionalSearchCriteria:
		//	(OrParameters)&&(OrParameters)&&(OrParameters)
		//	NOTE: All OR parameters, MUST be encased with brackets; you can only use && outside of brackets
		// Format of OrParameters:
		//	Key1==Value1||Key2!=Value2 etc.
		//	NOTE: There must be no brackets; you can only use || operators inside the brackets
		// Full example:
		//	(Key1==Value1||Key2>Value2)&&(Key3!=Value3||Key4<Value4)

		// Parse and iterate the AND pairs
		TArray<FString> ANDPairs;
		ServerBrowserSearchQuery.GameSearch->AdditionalSearchCriteria.ParseIntoArray(&ANDPairs, TEXT("&&"), TRUE);

		for (INT ANDIdx=0; ANDIdx<ANDPairs.Num(); ANDIdx++)
		{
			// Clean out any brackets/whitespace (if any)
			ANDPairs(ANDIdx) = ANDPairs(ANDIdx).Replace(TEXT(" "), TEXT("")).Replace(TEXT("("), TEXT("")).Replace(TEXT(")"), TEXT(""));

			// Parse and iterate the OR pairs
			TArray<FString> ORPairs;
			ANDPairs(ANDIdx).ParseIntoArray(&ORPairs, TEXT("||"), TRUE);

			if (ORPairs.Num() == 0)
			{
				continue;
			}


			INT FilterIndex = OutFilterList.AddZeroed(1);

			if (FilterIndex == INDEX_NONE)
			{
				continue;
			}


			FClientFilterORClause& OutFilterClause = OutFilterList(FilterIndex);

			for (INT ORIdx=0; ORIdx<ORPairs.Num(); ORIdx++)
			{
				const FString& CurPairStr = ORPairs(ORIdx);
				FSearchFilterValue CurValue;
				FString CurDelim;
				TArray<FString> CurPair;

				// Determine the operator
				if (CurPairStr.InStr(TEXT("==")) != INDEX_NONE)
				{
					CurValue.Operator = OGSCT_Equals;
					CurDelim = TEXT("==");
				}
				else if (CurPairStr.InStr(TEXT("!=")) != INDEX_NONE)
				{
					CurValue.Operator = OGSCT_NotEquals;
					CurDelim = TEXT("!=");
				}
				else if (CurPairStr.InStr(TEXT(">")) != INDEX_NONE)
				{
					CurValue.Operator = OGSCT_GreaterThan;
					CurDelim = TEXT(">");
				}
				else if (CurPairStr.InStr(TEXT(">=")) != INDEX_NONE)
				{
					CurValue.Operator = OGSCT_GreaterThanEquals;
					CurDelim = TEXT(">=");
				}
				else if (CurPairStr.InStr(TEXT("<")) != INDEX_NONE)
				{
					CurValue.Operator = OGSCT_LessThan;
					CurDelim = TEXT("<");
				}
				else if (CurPairStr.InStr(TEXT("<=")) != INDEX_NONE)
				{
					CurValue.Operator = OGSCT_LessThanEquals;
					CurDelim = TEXT("<=");
				}
				else
				{
					debugf(TEXT("Bad Key/Value comparison in AdditionalSearchCriteria: %s"), *CurPairStr);
					continue;
				}


				// Parse and validate the key/value
				CurPairStr.ParseIntoArray(&CurPair, *CurDelim, TRUE);

				if (CurPair.Num() != 2)
				{
					debugf(TEXT("Bad Key/Value comparison in AdditionalSearchCriteria: %s, OR pair list: %s"), *CurPairStr,
							*ANDPairs(ANDIdx));
					continue;
				}


				// Determine whether or not this is a master server filter
				FString* MasterKeyName = NULL;

				// Only the OGSCT_Equals operator and lone AND (&&) filters are supported with master-server filters
				if (!bDisableMasterFilters && ORPairs.Num() == 1 && CurValue.Operator == OGSCT_Equals)
				{
					MasterKeyName = MasterKeyMap.Find(CurPair(0));
				}


				// Enter the result into the mapping list
				if (MasterKeyName != NULL)
				{
					FString ParameterValue = CurPair(1);

					UBOOL* ReverseMap = MasterKeyReverseMap.Find(CurPair(0));
					UBOOL bReverseFilter = ReverseMap != NULL && *ReverseMap;

					FString* IgnoreMap = MasterKeyIgnoreMap.Find(CurPair(0));
					FString IgnoreValue = (IgnoreMap != NULL ? *IgnoreMap : TEXT(""));

					// Treats the filter as a bool, and flips the comparison value
					if (bReverseFilter)
					{
						ParameterValue = FString::Printf(TEXT("%i"), (INT)(ParameterValue.Trim() == TEXT("0")));
					}


					if (ParameterValue.Trim() != IgnoreValue.Trim())
					{
						OutMasterFilterList.Set(*MasterKeyName, ParameterValue);
					}
				}
				else
				{
					CurValue.Value = CurPair(1);
					OutFilterClause.OrParams.Add(CurPair(0), CurValue);
				}
			}
		}
	}


#if FILTER_DUMP
	debugf(NAME_DevOnline, TEXT("Query Filter (count: %i) AND list:"), OutFilterList.Num());

	for (INT FilterIndex=0; FilterIndex<OutFilterList.Num(); FilterIndex++)
	{
		FClientFilterORClause& FilterClause = OutFilterList(FilterIndex);

		debugf(NAME_DevOnline, TEXT("Query filter AND entry '%i' (count: %i) OR list:"), FilterIndex, FilterClause.OrParams.Num());

		FSearchFilterValue CurValue;
		FString CurOperator;
		INT i=0;

		for (FilterMap::TIterator It(FilterClause.OrParams); It; ++It)
		{
			CurValue = It.Value();

			switch (CurValue.Operator)
			{
			case OGSCT_Equals:
				CurOperator = TEXT("==");
				break;
			case OGSCT_NotEquals:
				CurOperator = TEXT("!=");
				break;
			case OGSCT_GreaterThan:
				CurOperator = TEXT(">");
				break;
			case OGSCT_GreaterThanEquals:
				CurOperator = TEXT(">=");
				break;
			case OGSCT_LessThan:
				CurOperator = TEXT("<");
				break;
			case OGSCT_LessThanEquals:
				CurOperator = TEXT("<=");
				break;
			default:
				CurOperator = TEXT("'Unknown operator'");
				break;
			}

			debugf(NAME_DevOnline, TEXT("- %i: %s %s %s"), i, *It.Key(), *CurOperator, *CurValue.Value);

			i++;
		}
	}
#endif
}

/**
 * Kicks off a search for a server with a specific IP:Port, for processing an invite
 *
 * @param ServerAddress		The address of the server to search for
 * @param ServerUID		The steam sockets address of the server
 * @return			Whether or not the search kicked off successfully
 */
UBOOL UOnlineGameInterfaceSteamworks::FindInviteGame(FString& ServerAddress, QWORD ServerUID/*=0*/)
{
	UBOOL bSuccess = FALSE;

	if (IsSteamClientAvailable())
	{
		// Don't start another search while one is in progress
		if ((InviteSearchQuery.GameSearch != NULL && InviteSearchQuery.GameSearch->bIsSearchInProgress == FALSE) ||
			InviteSearchQuery.GameSearch == NULL)
		{
			if (InviteGameSearch == NULL)
			{
				InviteGameSearch = ConstructObject<UOnlineGameSearch>(UOnlineGameSearch::StaticClass());
			}
			// Free up previous results, if present
			else if (InviteGameSearch->Results.Num())
			{
				FreeSearchResults(InviteGameSearch);
			}

			InviteSearchQuery.GameSearch = InviteGameSearch;

			TArray<FString> FilterKeys;
			TArray<FString> FilterValues;

			// Set a filter to find the precise server address
			FilterKeys.AddItem(TEXT("gameaddr"));
			FilterValues.AddItem(ServerAddress);


			// Now kickoff the search
			debugf(NAME_DevOnline, TEXT("Kicking off a search for an invite game at address: %s"), *ServerAddress);

			InviteServerUID.Uid = ServerUID;

			DWORD FindResult = FindInternetGames(&InviteSearchQuery, FilterKeys, FilterValues);

			if (FindResult != E_FAIL)
			{
				bSuccess = TRUE;
			}
			else
			{
				InviteServerUID.Uid = 0;
			}
		}
	}

	return bSuccess;
}

/**
 * Kicks off a search for internet games
 *
 * @param SearchQueryState	The matchmaking query state to kickoff the query in
 * @param FilterKeyList		The array of keys (with matching value) to filter on the master server
 * @param FilterValueList	The list of values associated with each key in FilterKeyList
 * @return			Returns the status of the search (pending/failure)
 */
DWORD UOnlineGameInterfaceSteamworks::FindInternetGames(FMatchmakingQueryState* SearchQueryState, TArray<FString>& FilterKeyList,
							TArray<FString>& FilterValueList)
{
	DWORD Return = E_FAIL;

	// Don't try to search if the network device is broken
	if (GSocketSubsystem->HasNetworkDevice())
	{
		// Make sure they are logged in to play online
		if (CastChecked<UOnlineSubsystemSteamworks>(OwningSubsystem)->LoggedInStatus == LS_LoggedIn)
		{
			debugf(NAME_DevOnline, TEXT("Starting search for Internet games..."));

			// JohnB: Filters I currently know of:
			/*
				gamedir		- a string identifying the current game or mod

				map		- Filters by map name
				dedicated	- Show only dedicated servers
				secure		- Show only VAC-enabled servers
				full		- Exclude full servers
				empty		- Exclude empty servers
				noplayers	- Show empty servers
				proxy		- Show only spectate-relay servers (not applicable to UE3)
			*/

			// @todo Steam: Add an optional hook in for game tag based searching, even if it can't be made compatible with the current
			//		online subsystem classes; needed to speedup queries

			// NOTE: If you update this code, update the code in FindLANGames as well


			// Setup the Steam filter list (the +1 is for the GameDir filter)
			INT NumFilters = FilterKeyList.Num() + 1;
			MatchMakingKeyValuePair_t* Filters = new MatchMakingKeyValuePair_t[NumFilters];

			const INT KeySize = sizeof(Filters[0].m_szKey);
			const INT ValueSize = sizeof(Filters[0].m_szValue);

			appMemzero(Filters, sizeof(MatchMakingKeyValuePair_t) * NumFilters);


			// Add the filters (starting with GameDir)
			INT FilterIndex=0;

			// Set the GameDir filter (NOTE: You must specify GameDir, as Steamworks has a bug, where just specifying the appid
			//	returns >all< servers, of all games, i.e. 20,000+)
			const TCHAR* GameDirKey = TEXT("gamedir");
			FString GameDirValue;
			GConfig->GetString(TEXT("OnlineSubsystemSteamworks.OnlineSubsystemSteamworks"), TEXT("GameDir"), GameDirValue, GEngineIni);

			appStrcpy(Filters[FilterIndex].m_szKey, KeySize, TCHAR_TO_ANSI(GameDirKey));
			appStrcpy(Filters[FilterIndex].m_szValue, ValueSize, TCHAR_TO_ANSI(*GameDirValue));

			FilterIndex++;


			for (INT CurKeyIdx=0; CurKeyIdx<FilterKeyList.Num(); CurKeyIdx++)
			{
				appStrcpy(Filters[FilterIndex].m_szKey, KeySize, TCHAR_TO_ANSI(*FilterKeyList(CurKeyIdx)));
				appStrcpy(Filters[FilterIndex].m_szValue, ValueSize, TCHAR_TO_ANSI(*FilterValueList(CurKeyIdx)));

				FilterIndex++;
			}

			// Kickoff the query
			CancelAllQueries(SearchQueryState);

			SearchQueryState->LastActivityTimestamp = appSeconds();

			SearchQueryState->ServerListResponse = new FOnlineAsyncTaskSteamServerListRequest(this, SearchQueryState);
			GSteamAsyncTaskManager->AddToInQueue(SearchQueryState->ServerListResponse);


			SearchQueryState->CurrentMatchmakingType = SMT_Internet;
			SearchQueryState->GameSearch->bIsSearchInProgress = TRUE;

			MatchMakingKeyValuePair_t** FilterPtrs = new MatchMakingKeyValuePair_t*[NumFilters];

			for (INT i=0; i<NumFilters; i++)
			{
				FilterPtrs[i] = &Filters[i];
			}

			SearchQueryState->CurrentMatchmakingQuery = GSteamMatchmakingServers->RequestInternetServerList(
										GSteamAppID, FilterPtrs, NumFilters,
										SearchQueryState->ServerListResponse);

			// Clear the Steam filter arrays
			delete[] Filters;
			delete[] FilterPtrs;

			Return = ERROR_IO_PENDING;
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("You must be logged in to an online profile to search for internet games"));
		}
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("Can't search for an internet game without a network connection"));
	}

	return Return;
}


// @todo Steam: should probably just make CancelFind*Games() virtual in the superclass.
/**
 * Cancels the current search in progress if possible for that search type
 *
 * @return true if successful searching for sessions, false otherwise
 */
UBOOL UOnlineGameInterfaceSteamworks::CancelFindOnlineGames()
{
	UBOOL Result = FALSE;

	if (IsSteamClientAvailable())
	{
		DWORD Return = E_FAIL;

		if (ServerBrowserSearchQuery.GameSearch != NULL && ServerBrowserSearchQuery.GameSearch->bIsSearchInProgress)
		{
			// Lan and internet are handled differently
			if (ServerBrowserSearchQuery.GameSearch->bIsLanQuery)
			{
				Return = CancelFindLanGames();
			}
			else
			{
				Return = CancelFindInternetGames();
			}
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("Can't cancel a search that isn't in progress"));
		}

		// Trigger the delegates (NOTE: CancelFindOnlineGames is not asynchronous, it returns immediately)
		FAsyncTaskDelegateResults Results(Return);
		TriggerOnlineDelegates(this, CancelFindOnlineGamesCompleteDelegates, &Results);

		Result = Return == S_OK;
	}
	else
	{
		Result = Super::CancelFindOnlineGames();
	}

	return Result;
}

/**
 * Attempts to cancel an internet game search
 *
 * @return an error/success code
 */
DWORD UOnlineGameInterfaceSteamworks::CancelFindInternetGames(void)
{
	debugf(NAME_DevOnline, TEXT("Canceling internet game search"));

	CleanupOnlineQuery(&ServerBrowserSearchQuery, TRUE);

	return S_OK;
}

/**
 * Attempts to cancel a LAN game search
 *
 * @return an error/success code
 */
DWORD UOnlineGameInterfaceSteamworks::CancelFindLanGames(void)
{
	debugf(NAME_DevOnline, TEXT("Canceling LAN game search"));

	StopLanBeacon();
	ServerBrowserSearchQuery.GameSearch->bIsSearchInProgress = FALSE;

	return S_OK;
}

/**
 * Frees the specified online query and marks the search as done
 *
 * @param SearchQueryState	The matchmaking query state performing the query
 * @param bCancel		Whether or not we are cancelling an active query, or cleaning up a finished one
 */
void UOnlineGameInterfaceSteamworks::CleanupOnlineQuery(FMatchmakingQueryState* SearchQueryState, UBOOL bCancel)
{
	if (bCancel)
	{
		if (SearchQueryState->GameSearch != NULL)
		{
			SearchQueryState->GameSearch->bIsSearchInProgress = FALSE;
		}

		CancelAllQueries(SearchQueryState);

		SearchQueryState->ActiveClientsideFilters.Empty();
	}
	else
	{
		if (SearchQueryState->CurrentMatchmakingQuery != NULL)
		{
			// Need to temporarily ignore callbacks triggered by CancelQuery, to avoid infinite recursion with 'RefreshComplete'
			SearchQueryState->bIgnoreRefreshComplete = TRUE;

			GSteamMatchmakingServers->CancelQuery(SearchQueryState->CurrentMatchmakingQuery);
			GSteamMatchmakingServers->ReleaseRequest(SearchQueryState->CurrentMatchmakingQuery);
			SearchQueryState->CurrentMatchmakingQuery = NULL;

			SearchQueryState->bIgnoreRefreshComplete = FALSE;
		}

		if (SearchQueryState->ServerListResponse != NULL)
		{
			SearchQueryState->ServerListResponse->MarkForDelete();
			SearchQueryState->ServerListResponse = NULL;
		}
	}
}


/**
 * Online session join
 */

// @todo Steam: just make the superclass Join*Game() virtual.
/**
 * Joins the game specified
 *
 * @param PlayerNum the index of the player searching for a match
 * @param SessionName the name of the session being joined
 * @param DesiredGame the desired game to join
 *
 * @return true if the call completed successfully, false otherwise
 */
UBOOL UOnlineGameInterfaceSteamworks::JoinOnlineGame(BYTE PlayerNum, FName SessionName, const FOnlineGameSearchResult& DesiredGame)
{
	UBOOL Result = FALSE;

	if (IsSteamClientAvailable())
	{
		DWORD Return = E_FAIL;

		// Don't join a session if already in one or hosting one
		if (SessionInfo == NULL)
		{
			// Make the selected game our current game settings
			GameSettings = DesiredGame.GameSettings;

			if (GameSettings != NULL)
			{
				Return = S_OK;

				// Create an empty session and fill it based upon game type
				SessionInfo = CreateSessionInfo();

				// Copy the session info over
				appMemcpy(SessionInfo, DesiredGame.PlatformData, GetSessionInfoSize());

				// The session info is created/filled differently depending on type
				if (GameSettings->bIsLanMatch == FALSE)
				{
					Return = JoinInternetGame(PlayerNum);
				}
				else
				{
					// Register all local talkers for voice
					RegisterLocalTalkers();

					// @todo Steam: Verify this is working correctly
					UOnlineSubsystemSteamworks* OnlineSub = CastChecked<UOnlineSubsystemSteamworks>(OwningSubsystem);
					OnlineSub->bIsStatsSessionOk = GameWantsStats();
					Return = S_OK;

					// @todo Steam: Remove this duplicate call, after the code has been tested thoroughly
#if 0
					FAsyncTaskDelegateResultsNamedSession Params(SessionName,S_OK);
					TriggerOnlineDelegates(this, JoinOnlineGameCompleteDelegates, &Params);
#endif
				}

				// Update the game state if successful (so that StartOnlineGame can work)
				if (Return == S_OK)
				{
					GameSettings->GameState = OGS_Pending;
				}
			}

			// Handle clean up in one place
			if (Return != S_OK)
			{
				// Clean up the session info so we don't get into a confused state
				delete (FSessionInfoSteam*)SessionInfo;
				SessionInfo = NULL;
				GameSettings = NULL;
			}
		}


		// Trigger the delegates (NOTE: JoinOnlineGame is not asynchronous, it returns immediately)
		FAsyncTaskDelegateResultsNamedSession Params(SessionName, Return);
		TriggerOnlineDelegates(this, JoinOnlineGameCompleteDelegates, &Params);

		debugf(NAME_DevOnline, TEXT("JoinOnlineGame  Return:0x%08X"), Return);

		Result = (Return == S_OK);
	}
	else
	{
		Result = Super::JoinOnlineGame(PlayerNum, SessionName, DesiredGame);
	}

	return Result;
}

/**
 * Joins the specified internet enabled game
 *
 * @param PlayerNum the player joining the game
 * @return S_OK if it succeeded, otherwise an Error code
 */
DWORD UOnlineGameInterfaceSteamworks::JoinInternetGame(BYTE PlayerNum)
{
	DWORD Return = E_FAIL;

	// Don't try to without a network device
	if (GSocketSubsystem->HasNetworkDevice())
	{
		UOnlineSubsystemSteamworks* OnlineSub = CastChecked<UOnlineSubsystemSteamworks>(OwningSubsystem);

		// Make sure they are logged in to play online
		if (OnlineSub->LoggedInStatus == LS_LoggedIn)
		{
			// Register all local talkers
			RegisterLocalTalkers();

			// Update the current game state
			GameSettings->GameState = OGS_Pending;


			// Update the players invite info (also happens in auth code upon connect, but doing it here allows it to work without auth)
			FSessionInfoSteam* CurSessionInfo = (FSessionInfoSteam*)SessionInfo;
			DWORD ServerAddr = 0;

			CurSessionInfo->HostAddr.GetIp(ServerAddr);

			OnlineSub->SetGameJoinInfo(ServerAddr, CurSessionInfo->HostAddr.GetPort(), CurSessionInfo->ServerUID,
							CurSessionInfo->bSteamSockets);


			// Update the stats flag if present
			OnlineSub->bIsStatsSessionOk = GameWantsStats();

			Return = S_OK;
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Can't join an internet game without being logged into an online profile"));
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Can't join an internet game without a network connection"));
	}

	return Return;
}


/**
 * General/misc OnlineGameInterface functions
 */


/**
 * Returns TRUE if the game wants stats, FALSE if not
 */
UBOOL UOnlineGameInterfaceSteamworks::GameWantsStats()
{
	UBOOL bWantsStats = FALSE;

	if (GameSettings != NULL)
	{
		if (GameSettings->bIsLanMatch == FALSE && GameSettings->bUsesStats)
		{
			bWantsStats = TRUE;
		}
	}

	return bWantsStats;
}

void UOnlineGameInterfaceSteamworks::Tick(FLOAT DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsSteamServerAvailable())
	{
		static FLOAT RefreshPublishedGameSettingsTime = 0.0f;
		RefreshPublishedGameSettingsTime += DeltaTime;

		if (IsServer() && GameSettings != NULL && RefreshPublishedGameSettingsTime >= 60.0f)
		{
			BYTE CurGameState = GameSettings->GameState;

			if (CurGameState == OGS_Pending || CurGameState == OGS_Starting || CurGameState == OGS_InProgress)
			{
				RefreshPublishedGameSettings();
			}

			RefreshPublishedGameSettingsTime = 0.0f;
		}
	}

	if (IsSteamClientAvailable())
	{
		TArray<FMatchmakingQueryState*> SearchQueries;

		SearchQueries.AddItem(&ServerBrowserSearchQuery);
		SearchQueries.AddItem(&InviteSearchQuery);


		for (INT SearchQueryIdx=0; SearchQueryIdx<SearchQueries.Num(); SearchQueryIdx++)
		{
			FMatchmakingQueryState& CurSearchQuery = *SearchQueries(SearchQueryIdx);

			// delete any response objects we're done with. They can't be deleted sooner, since the Steam SDK touches them after
			//	the callback runs
			for (INT i=0; i<CurSearchQuery.QueryToRulesResponseMap.Num(); i++)
			{
				FOnlineAsyncTaskSteamServerRulesRequest* Response = CurSearchQuery.QueryToRulesResponseMap(i).Response;

				if (Response->bDeleteMe)
				{
					if (CurSearchQuery.PendingRulesSearchSettings.RemoveItem(Response->GetSettings()) == 0)
					{
						debugf(NAME_DevOnline, TEXT("Rules cleanup: Could not find GameSettings in ")
							TEXT("PendingRulesSearchSettings"));
					}

					CurSearchQuery.QueryToRulesResponseMap.Remove(i, 1);
					i--;

					Response->MarkForDelete();
				}
			}

			for (INT i=0; i<CurSearchQuery.QueryToPingResponseMap.Num(); i++)
			{
				FOnlineAsyncTaskSteamServerPingRequest* Response = CurSearchQuery.QueryToPingResponseMap(i).Response;

				if (Response->bDeleteMe)
				{
					if (CurSearchQuery.PendingPingSearchSettings.RemoveItem(Response->GetSettings()) == 0)
					{
						debugf(NAME_DevOnline, TEXT("Ping cleanup: Could not find GameSettings in ")
							TEXT("PendingPingSearchSettings"));
					}

					CurSearchQuery.QueryToPingResponseMap.Remove(i, 1);
					i--;

					Response->MarkForDelete();
				}
			}

			// Server list request is done, but we're still waiting for rules responses for some servers;
			//	see if remaining rules responses came in
			if (CurSearchQuery.GameSearch != NULL && CurSearchQuery.GameSearch->bIsSearchInProgress &&
				!CurSearchQuery.GameSearch->bIsLanQuery && CurSearchQuery.ServerListResponse == NULL &&
				CurSearchQuery.QueryToRulesResponseMap.Num() == 0)
			{
				CurSearchQuery.GameSearch->bIsSearchInProgress = FALSE;

				// Only the server browser queries should trigger the FindOnlineGames callbacks
				if (CurSearchQuery.GameSearch == ServerBrowserSearchQuery.GameSearch)
				{
					FAsyncTaskDelegateResults Param(S_OK);
					TriggerOnlineDelegates(this, FindOnlineGamesCompleteDelegates, &Param);
				}
			}
		}


		// Check for timed out queries
		if (ServerBrowserSearchQuery.GameSearch != NULL && ServerBrowserSearchQuery.GameSearch->bIsSearchInProgress &&
			!ServerBrowserSearchQuery.GameSearch->bIsLanQuery)
		{
			if (appSeconds() - ServerBrowserSearchQuery.LastActivityTimestamp > ServerBrowserTimeout)
			{
				debugf(NAME_DevOnline, TEXT("Server browser query timed out after '%f' seconds"), ServerBrowserTimeout);

				CleanupOnlineQuery(&ServerBrowserSearchQuery, TRUE);

				// Notify script the query has completed
				FAsyncTaskDelegateResults Param(S_OK);
				TriggerOnlineDelegates(this, FindOnlineGamesCompleteDelegates, &Param);
			}
		}

		if (InviteSearchQuery.GameSearch != NULL && InviteSearchQuery.GameSearch->bIsSearchInProgress)
		{
			if (appSeconds() - InviteSearchQuery.LastActivityTimestamp > InviteTimeout)
			{
				debugf(NAME_DevOnline, TEXT("Invite search query timed out after '%f' seconds"), InviteTimeout);

				CleanupOnlineQuery(&InviteSearchQuery, TRUE);
				InviteServerUID.Uid = 0;
			}
		}
	}
}


/**
 * Updates any pending lan tasks and fires event notifications as needed
 *
 * @param DeltaTime the amount of time that has elapsed since the last call
 */
void UOnlineGameInterfaceSteamworks::TickLanTasks(FLOAT DeltaTime)
{
	Super::TickLanTasks(DeltaTime);  // ...and recheck...
}

/**
 * Updates any pending internet tasks and fires event notifications as needed
 *
 * @param DeltaTime the amount of time that has elapsed since the last call
 */
void UOnlineGameInterfaceSteamworks::TickInternetTasks(FLOAT DeltaTime)
{
	// no-op: we run pending Steam callbacks elsewhere.
	Super::TickInternetTasks(DeltaTime);
}

/**
 * Tells the online subsystem to accept the game invite that is currently pending
 *
 * @param LocalUserNum the local user accepting the invite
 * @param SessionName the name of the session this invite is to be known as
 */
UBOOL UOnlineGameInterfaceSteamworks::AcceptGameInvite(BYTE LocalUserNum, FName SessionName)
{
	if (InviteGameSearch && InviteGameSearch->Results.Num() > 0)
	{
		// If there is an invite pending, make it our game
		if (JoinOnlineGame(LocalUserNum, FName(TEXT("Invite")), InviteGameSearch->Results(0)) == FALSE)
		{
			debugf(NAME_Error, TEXT("Failed to join the invite game, aborting"));
		}

		// Clean up the invite data
		delete (FSessionInfoSteam*)InviteGameSearch->Results(0).PlatformData;

		InviteGameSearch->Results(0).PlatformData = NULL;
		InviteGameSearch = NULL;

		return TRUE;
	}

	return FALSE;
}

/**
 * Creates a game settings object populated with the information from the location string
 *
 * @param LocationString the string to parse
 */
void UOnlineGameInterfaceSteamworks::SetInviteInfo(const TCHAR* LocationString)
{
	// @todo Steam: Is this deprecated? Remove it? (NOTE: This wouldn't work with steam sockets anyway)

	// Get rid of the old stuff if applicable
	if (InviteGameSearch && InviteGameSearch->Results.Num() > 0)
	{
		FOnlineGameSearchResult& Result = InviteGameSearch->Results(0);
		FSessionInfoSteam* InviteSessionInfo = (FSessionInfoSteam*)Result.PlatformData;

		// Free the old first
		delete (FSessionInfoSteam*)InviteSessionInfo;

		Result.PlatformData = NULL;
		InviteGameSearch->Results.Empty();
	}

	InviteGameSearch = NULL;
	InviteLocationUrl.Empty();

	if (LocationString != NULL)
	{
		InviteLocationUrl = LocationString;
		InviteGameSearch = ConstructObject<UOnlineGameSearch>(UOnlineGameSearch::StaticClass());

		INT AddIndex = InviteGameSearch->Results.AddZeroed();
		FOnlineGameSearchResult& Result = InviteGameSearch->Results(AddIndex);

		Result.GameSettings = ConstructObject<UOnlineGameSettings>(UOnlineGameSettings::StaticClass());


		// Parse the URL to get the server and options
		FURL Url(NULL,LocationString,TRAVEL_Absolute);

		// Get the numbers from the location string
		INT MaxPub = appAtoi(Url.GetOption(TEXT("MaxPub="),TEXT("4")));
		INT MaxPri = appAtoi(Url.GetOption(TEXT("MaxPri="),TEXT("4")));

		// Set the number of slots and the hardcoded values
		Result.GameSettings->NumOpenPublicConnections = MaxPub;
		Result.GameSettings->NumOpenPrivateConnections = MaxPri;
		Result.GameSettings->NumPublicConnections = MaxPub;
		Result.GameSettings->NumPrivateConnections = MaxPri;
		Result.GameSettings->bIsLanMatch = FALSE;
		Result.GameSettings->bWasFromInvite = TRUE;


		// Create the session info and stash on the object
		FSessionInfoSteam* SessInfo = (FSessionInfoSteam*)CreateSessionInfo();
		UBOOL bIsValid;

		SessInfo->HostAddr.SetIp(*Url.Host, bIsValid);
		SessInfo->HostAddr.SetPort(Url.Port);

		// Store the session data
		Result.PlatformData = SessInfo;
	}
}

/** Registers all of the local talkers with the voice engine */
void UOnlineGameInterfaceSteamworks::RegisterLocalTalkers()
{
	UOnlineSubsystemSteamworks* Subsystem = Cast<UOnlineSubsystemSteamworks>(OwningSubsystem);

	if (Subsystem != NULL)
	{
		Subsystem->RegisterLocalTalkers();
	}
}

/** Unregisters all of the local talkers from the voice engine */
void UOnlineGameInterfaceSteamworks::UnregisterLocalTalkers()
{
	UOnlineSubsystemSteamworks* Subsystem = Cast<UOnlineSubsystemSteamworks>(OwningSubsystem);

	if (Subsystem != NULL)
	{
		Subsystem->UnregisterLocalTalkers();
	}
}

/**
 * Passes the new player to the subsystem so that voice is registered for this player
 *
 * @param SessionName the name of the session that the player is being added to
 * @param UniquePlayerId the player to register with the online service
 * @param bWasInvited whether the player was invited to the game or searched for it
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineGameInterfaceSteamworks::RegisterPlayer(FName SessionName, FUniqueNetId PlayerID, UBOOL bWasInvited)
{
	UOnlineSubsystemSteamworks* Subsystem = Cast<UOnlineSubsystemSteamworks>(OwningSubsystem);

	if (Subsystem != NULL)
	{
		Subsystem->RegisterRemoteTalker(PlayerID);


		CSteamID SteamPlayerId((uint64)PlayerID.Uid);

		// Retrieve the players stats (otherwise you can't update them later)
		if (IsServer() && GSteamGameServerStats != NULL)
		{
			SteamAPICall_t ApiCall = GSteamGameServerStats->RequestUserStats(SteamPlayerId);
			GSteamAsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskSteamServerUserStatsReceived(Subsystem, ApiCall));
		}

		if (GSteamUserStats != NULL)
		{
			SteamAPICall_t ApiCall = GSteamUserStats->RequestUserStats(SteamPlayerId);
			GSteamAsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskSteamUserStatsReceived(Subsystem, ApiCall));
		}
	}

	// Trigger delegates
	OnlineGameInterfaceImpl_eventOnRegisterPlayerComplete_Parms Results(EC_EventParm);
	Results.SessionName = SessionName;
	Results.PlayerID = PlayerID;
	Results.bWasSuccessful = FIRST_BITFIELD;

	TriggerOnlineDelegates(this, RegisterPlayerCompleteDelegates, &Results);

	return TRUE;
}

/**
 * Passes the removed player to the subsystem so that voice is unregistered for this player
 *
 * @param SessionName the name of the session to remove the player from
 * @param PlayerId the player to unregister with the online service
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineGameInterfaceSteamworks::UnregisterPlayer(FName SessionName, FUniqueNetId PlayerID)
{
	UOnlineSubsystemSteamworks* Subsystem = Cast<UOnlineSubsystemSteamworks>(OwningSubsystem);

	if (Subsystem != NULL)
	{
		Subsystem->UnregisterRemoteTalker(PlayerID);
	}


	// Trigger delegates
	OnlineGameInterfaceImpl_eventOnUnregisterPlayerComplete_Parms Results(EC_EventParm);
	Results.SessionName = SessionName;
	Results.PlayerID = PlayerID;
	Results.bWasSuccessful = FIRST_BITFIELD;

	TriggerOnlineDelegates(this, UnregisterPlayerCompleteDelegates, &Results);

	return TRUE;
}

/**
 * Serializes the platform specific data into the provided buffer for the specified search result
 *
 * @param DesiredGame the game to copy the platform specific data for
 * @param PlatformSpecificInfo the buffer to fill with the platform specific information
 *
 * @return true if successful serializing the data, false otherwise
 */
DWORD UOnlineGameInterfaceSteamworks::ReadPlatformSpecificInternetSessionInfo(const FOnlineGameSearchResult& DesiredGame, BYTE PlatformSpecificInfo[80])
{
	DWORD Return = E_FAIL;

	FNboSerializeToBuffer Buffer(80);
	FSessionInfoSteam* CurSessionInfo = (FSessionInfoSteam*)DesiredGame.PlatformData;

	// Write the connection data
	Buffer << CurSessionInfo->HostAddr;
	Buffer << CurSessionInfo->ServerUID;
	Buffer << (BYTE&)CurSessionInfo->bSteamSockets;

	if (Buffer.GetByteCount() <= 80)
	{
		// Copy the built up data
		appMemcpy(PlatformSpecificInfo, Buffer.GetRawBuffer(0), Buffer.GetByteCount());
		Return = S_OK;
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("Platform data is larger (%d) than the supplied buffer (80)"), Buffer.GetByteCount());
	}

	return Return;
}

/**
 * Creates a search result out of the platform specific data and adds that to the specified search object
 *
 * @param SearchingPlayerNum the index of the player searching for a match
 * @param SearchSettings the desired search to bind the session to
 * @param PlatformSpecificInfo the platform specific information to convert to a server object
 *
 * @return the result code for the operation
 */
DWORD UOnlineGameInterfaceSteamworks::BindPlatformSpecificSessionToInternetSearch(BYTE SearchingPlayerNum, UOnlineGameSearch* SearchSettings,
											BYTE* PlatformSpecificInfo)
{
	DWORD Return = E_FAIL;

	// Create an object that we'll copy the data to
	UOnlineGameSettings* NewServer = ConstructObject<UOnlineGameSettings>(ServerBrowserSearchQuery.GameSearch->GameSettingsClass);

	if (NewServer != NULL)
	{
		// Add space in the search results array
		INT NewSearch = ServerBrowserSearchQuery.GameSearch->Results.Add();
		FOnlineGameSearchResult& Result = ServerBrowserSearchQuery.GameSearch->Results(NewSearch);

		// Link the settings to this result
		Result.GameSettings = NewServer;

		// Allocate and read the session data
		FSessionInfoSteam* SessInfo = (FSessionInfoSteam*)CreateSessionInfo();

		// Read the serialized data from the buffer
		FNboSerializeFromBuffer Packet(PlatformSpecificInfo, 80);

		// Read the connection data
		Packet >> SessInfo->HostAddr;
		Packet >> SessInfo->ServerUID;
		Packet >> (BYTE&)SessInfo->bSteamSockets;


		// Store this in the results
		Result.PlatformData = SessInfo;
		Return = S_OK;
	}

	return Return;
}

#endif

