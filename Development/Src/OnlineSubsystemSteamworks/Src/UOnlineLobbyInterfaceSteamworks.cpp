/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemSteamworks.h"

// @todo Steam: Go through all function header comments when finalized, and make sure they match script function comments

// @todo Steam: You need to properly remove lobbies from ActiveLobbies when exiting them; you do this in LeaveLobby, but you don't catch cases
//		where you are otherwise disconnected from the lobby; how do you catch those cases?
//		NOTE: Move all lobby-exit-handling from LeaveLobby to a separate function, if you need multiple calls

// @todo Steam: Test whether 'GetLobbyMemberLimit' can work with found lobbies, or if it only works with joined lobbies, and then either add a
//		MaxMembers/MaxPlayers value to BasicLobbyInfo, or to ActiveLobbyInfo, depending on the results
//		NOTE: Perhaps also hook 'GetLobbyMemberLimit' into the lobby data update native code, to make sure the structs are up to date
//		NOTE: Also add a 'SetLobbyLimit' function, for calling 'SetLobbyMemberLimit' when admin

// @todo Steam: IMPORTANT: Document >all< return codes (all of them are controlled by switch statements, which make them easy to find);
//		you must document them in the UScript function comments primarily

// @todo Steam: Test the size limits of the lobby message functions (both string and binary)

// @todo Steam: Add utility functions to consolidate all the LobbyIndex/MemberIndex searching

#if WITH_UE3_NETWORKING && WITH_STEAMWORKS

#if STEAM_MATCHMAKING_LOBBY

/**
 * Async events/tasks
 */

/**
 * Notification event from Steam notifying the client of a lobby creation result
 */
class FOnlineAsyncEventSteamLobbyCreated : public FOnlineAsyncEventSteam<LobbyCreated_t, UOnlineLobbyInterfaceSteamworks>
{
private:
	/** The UID of the created lobby, or 0 if creation failed */
	QWORD	LobbyUID;

	/** The result of the lobby creation attempt */
	EResult	CreateResult;


	/** Hidden constructor */
	FOnlineAsyncEventSteamLobbyCreated()
		: LobbyUID(0)
		, CreateResult(k_EResultOK)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InLobbyInterface	The lobby interface object this event is linked to
	 */
	FOnlineAsyncEventSteamLobbyCreated(UOnlineLobbyInterfaceSteamworks* InLobbyInterface)
		: FOnlineAsyncEventSteam(InLobbyInterface)
		, LobbyUID(0)
		, CreateResult(k_EResultOK)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamLobbyCreated()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamLobbyCreated completed LobbyUID: ") I64_FORMAT_TAG
					TEXT(", CreateResult: %s"), LobbyUID, *SteamResultString(CreateResult));
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(LobbyCreated_t* CallbackData)
	{
		LobbyUID = CallbackData->m_ulSteamIDLobby;
		CreateResult = CallbackData->m_eResult;

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();

	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();


		debugf(NAME_DevOnline, TEXT("SteamLobbyCreated: Result: %s, LobbyId: ") I64_FORMAT_TAG, *SteamResultString(CreateResult), LobbyUID);

		// @todo Steam: Document all possible error returns for the delegates

		if (CreateResult == k_EResultOK)
		{
			// If there are pending lobby settings, set them now (may trigger new asynchronous calls),
			//	or trigger success delegates immediately
			if (CallbackInterface->CreateLobbySettings.Num() > 0)
			{
				CSteamID LobbySteamId(LobbyUID);
				UBOOL bSetFailure = FALSE;

				for (INT SettingIdx=0; SettingIdx<CallbackInterface->CreateLobbySettings.Num(); SettingIdx++)
				{
					FLobbyMetaData& CurSettings = CallbackInterface->CreateLobbySettings(SettingIdx);

					if (!GSteamMatchmaking->SetLobbyData(LobbySteamId, TCHAR_TO_UTF8(*CurSettings.Key),
						TCHAR_TO_UTF8(*CurSettings.Value)))
					{
						bSetFailure = TRUE;
						break;
					}
				}


				// If we failed to set the lobby settings, we need to request the lobby settings list first
				// @todo Steam: Test that it is actually possible for this to fail at all;
				//		you may only need to request settings to grab, not set
				if (bSetFailure)
				{
					CallbackInterface->PendingCreateLobbyResult.Uid = LobbyUID;

					// If we failed to request a settings update, trigger success, but note the settings error
					if (!GSteamMatchmaking->RequestLobbyData(LobbySteamId))
					{
						debugf(NAME_DevOnline, TEXT("Created lobby, but failed to request/set initial lobby settings"));

						CallbackInterface->PendingCreateLobbyResult.Uid = 0;

						OnlineLobbyInterfaceSteamworks_eventOnCreateLobbyComplete_Parms CreateLobbyParms(EC_EventParm);
						CreateLobbyParms.bWasSuccessful = TRUE;
						CreateLobbyParms.Error = TEXT("FailedInitialSettings");
						CreateLobbyParms.LobbyId.Uid = LobbyUID;

						TriggerOnlineDelegates(CallbackInterface, CallbackInterface->CreateLobbyCompleteDelegates,
									&CreateLobbyParms);
					}
				}
				else
				{
					// Success; return immediately
					OnlineLobbyInterfaceSteamworks_eventOnCreateLobbyComplete_Parms CreateLobbyParms(EC_EventParm);
					CreateLobbyParms.bWasSuccessful = TRUE;
					CreateLobbyParms.LobbyId.Uid = LobbyUID;

					TriggerOnlineDelegates(CallbackInterface, CallbackInterface->CreateLobbyCompleteDelegates, &CreateLobbyParms);
				}
			}
			else
			{
				// Success; return immediately
				OnlineLobbyInterfaceSteamworks_eventOnCreateLobbyComplete_Parms CreateLobbyParms(EC_EventParm);
				CreateLobbyParms.bWasSuccessful = TRUE;
				CreateLobbyParms.LobbyId.Uid = LobbyUID;

				TriggerOnlineDelegates(CallbackInterface, CallbackInterface->CreateLobbyCompleteDelegates, &CreateLobbyParms);
			}
		}
		else
		{
			FString ErrorString = TEXT("");

			switch (CreateResult)
			{
			case k_EResultNoConnection:
				ErrorString = TEXT("NoConnection");
				break;

			case k_EResultTimeout:
				ErrorString = TEXT("Timeout");
				break;

			case k_EResultFail:
				ErrorString = TEXT("InternalError");
				break;

			case k_EResultAccessDenied:
				ErrorString = TEXT("AccessDenied");
				break;

			case k_EResultLimitExceeded:
				ErrorString = TEXT("LimitExceeded");
				break;

			default:
				ErrorString = FString::Printf(TEXT("Unknown error: %s"), *SteamResultString(CreateResult));
				break;
			}

			// Wipe the pending settings list, and trigger the callback delegate with a failure notice
			CallbackInterface->CreateLobbySettings.Empty();

			OnlineLobbyInterfaceSteamworks_eventOnCreateLobbyComplete_Parms CreateLobbyParms(EC_EventParm);
			CreateLobbyParms.bWasSuccessful = FALSE;
			CreateLobbyParms.Error = ErrorString;

			TriggerOnlineDelegates(CallbackInterface, CallbackInterface->CreateLobbyCompleteDelegates, &CreateLobbyParms);
		}
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyCreated);

/**
 * Notification event from Steam notifying the client of a lobby enter result
 */
class FOnlineAsyncEventSteamLobbyEnterResponse : public FOnlineAsyncEventSteam<LobbyEnter_t, UOnlineLobbyInterfaceSteamworks>
{
private:
	/** The UID of the lobby we attempted to enter */
	QWORD			LobbyUID;

	/** The result of the lobby enter attempt */
	EChatRoomEnterResponse	EnterResult;

	/** User permissions within the lobby */
	DWORD			UserPermissions;

	/** Whether or not the lobby is invite-only */
	UBOOL			bInviteOnly;


	/** Hidden constructor */
	FOnlineAsyncEventSteamLobbyEnterResponse()
		: LobbyUID(0)
		, EnterResult(k_EChatRoomEnterResponseNotAllowed)
		, UserPermissions(0)
		, bInviteOnly(FALSE)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InLobbyInterface	The lobby interface object this event is linked to
	 */
	FOnlineAsyncEventSteamLobbyEnterResponse(UOnlineLobbyInterfaceSteamworks* InLobbyInterface)
		: FOnlineAsyncEventSteam(InLobbyInterface)
		, LobbyUID(0)
		, EnterResult(k_EChatRoomEnterResponseNotAllowed)
		, UserPermissions(0)
		, bInviteOnly(FALSE)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamLobbyEnterResponse()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		// @todo Steam: Split out the EnterResult stringizing into an inline function, and use it here
		return FString::Printf(TEXT("FOnlineAsyncEventSteamLobbyEnterResponse completed LobbyUID: ") I64_FORMAT_TAG
			TEXT(", EnterResult: %i, UserPermissions: %i, bInviteOnly: %i"), LobbyUID, (INT)EnterResult, UserPermissions,
			(INT)bInviteOnly);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(LobbyEnter_t* CallbackData)
	{
		LobbyUID = CallbackData->m_ulSteamIDLobby;
		EnterResult = (EChatRoomEnterResponse)CallbackData->m_EChatRoomEnterResponse;
		UserPermissions = CallbackData->m_rgfChatPermissions;
		bInviteOnly = CallbackData->m_bLocked;

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();

	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();

		// @todo Steam: Like 'ToString', split out and strinize EnterResult
		debugf(NAME_DevOnline, TEXT("SteamLobbyEnterResponse: LobbyUID: ") I64_FORMAT_TAG
			TEXT(", EnterResult: %i, UserPermissions: %i, bInviteOnly: %i"), LobbyUID, (INT)EnterResult, UserPermissions,
			(INT)bInviteOnly);

		FUniqueNetId LobbyNetId;
		LobbyNetId.Uid = LobbyUID;

		if (EnterResult == k_EChatRoomEnterResponseSuccess)
		{
			CSteamID LobbySteamId(LobbyUID);

			// Add this lobby to the active lobbies list
			INT LobbyIndex = CallbackInterface->ActiveLobbies.AddZeroed();
			FActiveLobbyInfo& CurActiveLobby = CallbackInterface->ActiveLobbies(LobbyIndex);

			CurActiveLobby.LobbyUID = LobbyNetId;


			// Attempt to parse the lobby settings
			if (!CallbackInterface->FillLobbySettings(CurActiveLobby.LobbySettings, LobbyNetId))
			{
				// NOTE: This should never fail for joined lobbies, judging by Steam documentation
				debugf(NAME_DevOnline, TEXT("SteamLobbyEnterResponse: Failed to grab lobby settings; LobbyUID: ") I64_FORMAT_TAG,
					LobbyUID);
			}


			// Attempt to parse the lobby members
			INT LobbyMemberCount = GSteamMatchmaking->GetNumLobbyMembers(LobbySteamId);

			for (INT i=0; i<LobbyMemberCount; i++)
			{
				CSteamID MemberSteamId = GSteamMatchmaking->GetLobbyMemberByIndex(LobbySteamId, i);

				INT MemberIndex = CurActiveLobby.Members.AddZeroed();
				FLobbyMember& CurMember = CurActiveLobby.Members(MemberIndex);

				CurMember.PlayerUID.Uid = MemberSteamId.ConvertToUint64();


				// Attempt to parse the member settings
				if (!CallbackInterface->FillMemberSettings(CurMember.PlayerSettings, LobbyNetId, CurMember.PlayerUID))
				{
					debugf(NAME_DevOnline, TEXT("SteamLobbyEnterResponse: Failed to grab lobby member settings; LobbyUID: ")
						I64_FORMAT_TAG TEXT(", MemberId: ") I64_FORMAT_TAG, LobbyUID, CurMember.PlayerUID.Uid);

					// @todo Steam: Are fallbacks possible here?
				}
			}


			// Success; trigger callbacks
			CallbackInterface->eventTriggerJoinLobbyCompleteDelegates(TRUE, LobbyIndex, LobbyNetId, "");
		}
		else
		{
			// @todo Steam: Split into an inline and stringize other log references to EnterResult
			FString ErrorString = TEXT("");

			switch (EnterResult)
			{
			case k_EChatRoomEnterResponseDoesntExist:
				ErrorString = TEXT("DoesntExist");
				break;

			case k_EChatRoomEnterResponseNotAllowed:
				ErrorString = TEXT("NotAllowed");
				break;

			case k_EChatRoomEnterResponseFull:
				ErrorString = TEXT("Full");
				break;

			case k_EChatRoomEnterResponseError:
				ErrorString = TEXT("InternalError");
				break;

			case k_EChatRoomEnterResponseBanned:
				ErrorString = TEXT("Banned");
				break;

			case k_EChatRoomEnterResponseLimited:
				ErrorString = TEXT("LimitedUser");
				break;

			case k_EChatRoomEnterResponseClanDisabled:
				ErrorString = TEXT("ClanDisabled");
				break;

			case k_EChatRoomEnterResponseCommunityBan:
				ErrorString = TEXT("CommunityBan");
				break;

			default:
				debugf(NAME_DevOnline, TEXT("SteamLobbyEnterResponse: Join attempt returned unknown error: %i"), (INT)EnterResult);
				ErrorString = FString::Printf(TEXT("UnknownError:%i"), (INT)EnterResult);

				break;
			}


			// Trigger callback delegate with failure notice
			CallbackInterface->eventTriggerJoinLobbyCompleteDelegates(FALSE, INDEX_NONE, LobbyNetId, ErrorString);
		}
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyEnterResponse);

/**
 * Notification event from Steam notifying the client that data/info for a lobby has changed
 */
class FOnlineAsyncEventSteamLobbyDataUpdate : public FOnlineAsyncEventSteam<LobbyDataUpdate_t, UOnlineLobbyInterfaceSteamworks>
{
private:
	/** The UID of the lobby whose data has updated */
	QWORD			LobbyUID;

	/** The UID of the lobby member whose data has updated (or LobbyUID if the lobby itself updated) */
	QWORD			MemberUID;

	/** Whether or not the data update was successful (only relevant when using RequestLobbyData) */
	UBOOL			bUpdateSuccess;


	/** Hidden constructor */
	FOnlineAsyncEventSteamLobbyDataUpdate()
		: LobbyUID(0)
		, MemberUID(0)
		, bUpdateSuccess(FALSE)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InLobbyInterface	The lobby interface object this event is linked to
	 */
	FOnlineAsyncEventSteamLobbyDataUpdate(UOnlineLobbyInterfaceSteamworks* InLobbyInterface)
		: FOnlineAsyncEventSteam(InLobbyInterface)
		, LobbyUID(0)
		, MemberUID(0)
		, bUpdateSuccess(FALSE)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamLobbyDataUpdate()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamLobbyDataUpdate completed LobbyUID: ") I64_FORMAT_TAG
					TEXT(", MemberUID: ") I64_FORMAT_TAG TEXT(", bUpdateSuccess: %i"), LobbyUID, MemberUID,
					(INT)bUpdateSuccess);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(LobbyDataUpdate_t* CallbackData)
	{
		LobbyUID = CallbackData->m_ulSteamIDLobby;
		MemberUID = CallbackData->m_ulSteamIDMember;
		bUpdateSuccess = CallbackData->m_bSuccess;

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();


		// @todo Steam: Comment out this log, if it ends up spamming info
		debugf(NAME_DevOnline, TEXT("SteamLobbyDataUpdate: LobbyUID: ") I64_FORMAT_TAG TEXT(", MemberUID: ") I64_FORMAT_TAG
			TEXT(", bUpdateSuccess: %i"), LobbyUID, MemberUID, (INT)bUpdateSuccess);


		// Capture initial lobby data update, to set initial lobby settings and trigger 'CreateLobbyComplete' delegates
		// @todo Steam: Make sure you don't also call the 'data update' delegates, if grabbing initial lobby setting notification)
		if (CallbackInterface->PendingCreateLobbyResult.Uid != 0 && LobbyUID == CallbackInterface->PendingCreateLobbyResult.Uid &&
			MemberUID == CallbackInterface->PendingCreateLobbyResult.Uid)
		{
			CallbackInterface->PendingCreateLobbyResult.Uid = 0;
			UBOOL bSetFailure = FALSE;

			if (bUpdateSuccess)
			{
				CSteamID LobbySteamId(LobbyUID);

				for (INT SettingIdx=0; SettingIdx<CallbackInterface->CreateLobbySettings.Num(); SettingIdx++)
				{
					FLobbyMetaData& CurSettings = CallbackInterface->CreateLobbySettings(SettingIdx);

					if (CurSettings.Key.IsEmpty() || CurSettings.Value.IsEmpty())
					{
						continue;
					}


					if (!GSteamMatchmaking->SetLobbyData(LobbySteamId, TCHAR_TO_UTF8(*CurSettings.Key),
										TCHAR_TO_UTF8(*CurSettings.Value)))
					{
						bSetFailure = TRUE;
						break;
					}
				}
			}
			else
			{
				bSetFailure = TRUE;
			}

			if (!bSetFailure)
			{
				// Success; trigger delegates
				OnlineLobbyInterfaceSteamworks_eventOnCreateLobbyComplete_Parms CreateLobbyParms(EC_EventParm);
				CreateLobbyParms.bWasSuccessful = TRUE;
				CreateLobbyParms.LobbyId.Uid = LobbyUID;

				TriggerOnlineDelegates(CallbackInterface, CallbackInterface->CreateLobbyCompleteDelegates, &CreateLobbyParms);
			}
			else
			{
				// If we failed to set the initial settings, trigger success, but note the settings error
				debugf(NAME_DevOnline, TEXT("Created lobby, but failed to set initial lobby settings after data update"));

				OnlineLobbyInterfaceSteamworks_eventOnCreateLobbyComplete_Parms CreateLobbyParms(EC_EventParm);
				CreateLobbyParms.bWasSuccessful = TRUE;
				CreateLobbyParms.Error = TEXT("FailedInitialDataUpdateSettings");
				CreateLobbyParms.LobbyId.Uid = LobbyUID;

				TriggerOnlineDelegates(CallbackInterface, CallbackInterface->CreateLobbyCompleteDelegates, &CreateLobbyParms);
			}
		}
		else
		{
			INT ActiveLobbyIndex = INDEX_NONE;

			for (INT i=0; i<CallbackInterface->ActiveLobbies.Num(); i++)
			{
				if (CallbackInterface->ActiveLobbies(i).LobbyUID.Uid == LobbyUID)
				{
					ActiveLobbyIndex = i;
					break;
				}
			}

			if (ActiveLobbyIndex != INDEX_NONE)
			{
				FActiveLobbyInfo& CurActiveLobby = CallbackInterface->ActiveLobbies(ActiveLobbyIndex);

				// Lobby data update
				if (LobbyUID == MemberUID)
				{
					if (CallbackInterface->FillLobbySettings(CurActiveLobby.LobbySettings, CurActiveLobby.LobbyUID))
					{
						CallbackInterface->eventTriggerLobbySettingsUpdateDelegates(ActiveLobbyIndex);
					}
					else
					{
						debugf(NAME_DevOnline, TEXT("SteamLobbyDataUpdate: Failed to grab lobby settings, LobbyUID: ")
							I64_FORMAT_TAG, LobbyUID);
					}
				}
				// Lobby member data update
				else
				{
					INT MemberIndex = INDEX_NONE;

					for (INT i=0; i<CurActiveLobby.Members.Num(); i++)
					{
						if (CurActiveLobby.Members(i).PlayerUID.Uid == MemberUID)
						{
							MemberIndex = i;
							break;
						}
					}


					// New member (shouldn't happen)
					if (MemberIndex == INDEX_NONE)
					{
						MemberIndex = CurActiveLobby.Members.AddZeroed();
						CurActiveLobby.Members(MemberIndex).PlayerUID.Uid = MemberUID;

						// @todo Steam: If this is possible, you may need to add extra checks and consolidate
						//		join/exit code in one place
						debugf(NAME_DevOnline, TEXT("SteamLobbyDataUpdate: Unexpected new lobby member, LobbyUID: ")
							I64_FORMAT_TAG TEXT(", MemberUID: ") I64_FORMAT_TAG, LobbyUID, MemberUID);
					}


					FLobbyMember& CurMember = CurActiveLobby.Members(MemberIndex);

					if (CallbackInterface->FillMemberSettings(CurMember.PlayerSettings, LobbyUID, MemberUID))
					{
						CallbackInterface->eventTriggerLobbyMemberSettingsUpdateDelegates(ActiveLobbyIndex, MemberIndex);
					}
					else
					{
						debugf(NAME_DevOnline, TEXT("SteamLobbyDataUpdate: Failed to grab member settings, LobbyUID: ")
							I64_FORMAT_TAG TEXT(", MemberUID: ") I64_FORMAT_TAG, LobbyUID, MemberUID);
					}
				}
			}
		}
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyDataUpdate);

/**
 * Notification event from Steam notifying the client that the status of a lobby member has changed
 */
class FOnlineAsyncEventSteamLobbyMemberUpdate : public FOnlineAsyncEventSteam<LobbyChatUpdate_t, UOnlineLobbyInterfaceSteamworks>
{
private:
	/** The UID of the lobby the member is in */
	QWORD			LobbyUID;

	/** The UID of the lobby member whose status has changed */
	QWORD			MemberUID;

	/** The UID of the lobby member that instigated the change in status for the member (e.g. admin UID if kicking) */
	QWORD			InstigatorUID;

	/** How the members status changed */
	EChatMemberStateChange	UpdateEvent;


	/** Hidden constructor */
	FOnlineAsyncEventSteamLobbyMemberUpdate()
		: LobbyUID(0)
		, MemberUID(0)
		, InstigatorUID(0)
		, UpdateEvent(k_EChatMemberStateChangeLeft)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InLobbyInterface	The lobby interface object this event is linked to
	 */
	FOnlineAsyncEventSteamLobbyMemberUpdate(UOnlineLobbyInterfaceSteamworks* InLobbyInterface)
		: FOnlineAsyncEventSteam(InLobbyInterface)
		, LobbyUID(0)
		, MemberUID(0)
		, InstigatorUID(0)
		, UpdateEvent(k_EChatMemberStateChangeLeft)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamLobbyMemberUpdate()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		// @todo Steam: Stringize UpdateEvent?
		return FString::Printf(TEXT("FOnlineAsyncEventSteamLobbyMemberUpdate completed LobbyUID: ") I64_FORMAT_TAG TEXT(", MemberUID: ")
			I64_FORMAT_TAG TEXT(", InstigatorUID: ") I64_FORMAT_TAG TEXT(", UpdateEvent: %i"),
			LobbyUID, MemberUID, InstigatorUID, (INT)UpdateEvent);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(LobbyChatUpdate_t* CallbackData)
	{
		LobbyUID = CallbackData->m_ulSteamIDLobby;
		MemberUID = CallbackData->m_ulSteamIDUserChanged;
		InstigatorUID = CallbackData->m_ulSteamIDMakingChange;
		UpdateEvent = (EChatMemberStateChange)CallbackData->m_rgfChatMemberStateChange;

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();

	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();


		// @todo Steam: Stringize status
		debugf(NAME_DevOnline, TEXT("SteamLobbyMemberUpdate: LobbyUID: ") I64_FORMAT_TAG TEXT(", MemberUID: ") I64_FORMAT_TAG
			TEXT(", InstigatorUID: ") I64_FORMAT_TAG TEXT(", UpdateEvent: %d"), LobbyUID, MemberUID, InstigatorUID, (INT)UpdateEvent);


		// Search for the lobby
		INT LobbyIndex = INDEX_NONE;

		for (INT i=0; i<CallbackInterface->ActiveLobbies.Num(); i++)
		{
			if (CallbackInterface->ActiveLobbies(i).LobbyUID.Uid == LobbyUID)
			{
				LobbyIndex = i;
				break;
			}
		}

		if (LobbyIndex == INDEX_NONE)
		{
			debugf(NAME_DevOnline, TEXT("SteamLobbyMemberUpdate: Received status update for a lobby we are not in; LobbyId: ")
				I64_FORMAT_TAG, LobbyUID);

			return;
		}

		FActiveLobbyInfo& CurActiveLobby = CallbackInterface->ActiveLobbies(LobbyIndex);

		// Member/Instigator index
		INT MemberIndex = INDEX_NONE;
		INT InstigatorIndex = INDEX_NONE;

		if (LobbyIndex != INDEX_NONE)
		{
			for (INT i=0; i<CurActiveLobby.Members.Num(); i++)
			{
				const QWORD CurUID = CurActiveLobby.Members(i).PlayerUID.Uid;

				if (CurUID == MemberUID)
				{
					MemberIndex = i;
				}

				if (CurUID == InstigatorUID)
				{
					InstigatorIndex = i;
				}

				if (MemberIndex != INDEX_NONE && InstigatorIndex != INDEX_NONE)
				{
					break;
				}
			}
		}


		// Determine the type of status update
		FString Status = TEXT("");
		UBOOL bJoinEvent = FALSE;
		UBOOL bExitEvent = FALSE;

		switch (UpdateEvent)
		{
		case k_EChatMemberStateChangeEntered:
			bJoinEvent = TRUE;
			Status = TEXT("Joined");
			break;

		case k_EChatMemberStateChangeLeft:
			bExitEvent = TRUE;
			Status = TEXT("Left");
			break;

		case k_EChatMemberStateChangeDisconnected:
			bExitEvent = TRUE;
			Status = TEXT("Disconnected");
			break;

		case k_EChatMemberStateChangeKicked:
			bExitEvent = TRUE;
			Status = TEXT("Kicked");
			break;

		case k_EChatMemberStateChangeBanned:
			bExitEvent = TRUE;
			Status = TEXT("Banned");
			break;

		default:
			debugf(NAME_DevOnline, TEXT("SteamLobbyMemberUpdate: Unknown status value: %d"), (INT)UpdateEvent);
			Status = FString::Printf(TEXT("UnknownStatus:%i"), (INT)UpdateEvent);
			break;
		}

		// Handle joined player data, >before< triggering delegates
		if (bJoinEvent)
		{
			if (MemberIndex == INDEX_NONE)
			{
				MemberIndex = CurActiveLobby.Members.AddZeroed();

				FLobbyMember& CurMember = CurActiveLobby.Members(MemberIndex);
				CurMember.PlayerUID.Uid = MemberUID;

				// @todo Steam: Should there be a separate join event delegate?
				// @todo Steam: You need to retest this eventually, as you were originally passing in wrong second/third parameters
				if (!CallbackInterface->FillMemberSettings(CurMember.PlayerSettings, LobbyUID, MemberUID))
				{
					debugf(NAME_DevOnline, TEXT("SteamLobbyMemberUpdate: Failed to fill joining members settings; LobbyUID: ")
						I64_FORMAT_TAG TEXT(", MemberUID: ") I64_FORMAT_TAG, LobbyUID, MemberUID);
				}
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("SteamLobbyMemberUpdate: Received lobby join, when member already in lobby; LobbyUID: ")
					I64_FORMAT_TAG TEXT(", MemberUID: ") I64_FORMAT_TAG, LobbyUID, MemberUID);
			}
		}


		// Trigger the callback delegates
		if (MemberIndex != INDEX_NONE)
		{
			CallbackInterface->eventTriggerLobbyMemberStatusUpdateDelegates(LobbyIndex, MemberIndex, InstigatorIndex, Status);
		}


		// Handle removal of exiting players data, >after< triggering delegates
		if (bExitEvent)
		{
			if (MemberIndex != INDEX_NONE)
			{
				// @todo Steam: Should there be a serpate exit event delegate?

				// Remove the players data from the lobby
				CurActiveLobby.Members.Remove(MemberIndex, 1);
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("SteamLobbyMemberUpdate: Received lobby exit, when member is not in lobby; LobbyUID: ")
					I64_FORMAT_TAG TEXT(", MemberUID: ") I64_FORMAT_TAG, LobbyUID, MemberUID);
			}
		}
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyMemberUpdate);

/**
 * Notification event from Steam notifying the client that a lobby game is created and ready to join
 */
class FOnlineAsyncEventSteamLobbyGameCreated : public FOnlineAsyncEventSteam<LobbyGameCreated_t, UOnlineLobbyInterfaceSteamworks>
{
private:
	/** The UID of the lobby the member is in */
	QWORD			LobbyUID;

	/** The UID of the lobby game server to be joined */
	QWORD			ServerUID;

	/** The IP of the game server */
	DWORD			ServerIP;

	/** The port of the game server */
	INT			ServerPort;


	/** Hidden constructor */
	FOnlineAsyncEventSteamLobbyGameCreated()
		: LobbyUID(0)
		, ServerUID(0)
		, ServerIP(0)
		, ServerPort(0)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InLobbyInterface	The lobby interface object this event is linked to
	 */
	FOnlineAsyncEventSteamLobbyGameCreated(UOnlineLobbyInterfaceSteamworks* InLobbyInterface)
		: FOnlineAsyncEventSteam(InLobbyInterface)
		, LobbyUID(0)
		, ServerUID(0)
		, ServerIP(0)
		, ServerPort(0)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamLobbyGameCreated()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		FInternetIpAddr ServerAddr;
		ServerAddr.SetIp(ServerIP);

		return FString::Printf(TEXT("FOnlineAsyncEventSteamLobbyGameCreated completed LobbyUID: ") I64_FORMAT_TAG TEXT(", ServerUID: ")
					I64_FORMAT_TAG TEXT(", ServerIP: %s, ServerPort: %i"), LobbyUID, ServerUID, *ServerAddr.ToString(FALSE),
					ServerPort);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(LobbyGameCreated_t* CallbackData)
	{
		LobbyUID = CallbackData->m_ulSteamIDLobby;
		ServerUID = CallbackData->m_ulSteamIDGameServer;
		ServerIP = CallbackData->m_unIP;
		ServerPort = CallbackData->m_usPort;

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();

		FInternetIpAddr ServerAddr;

		ServerAddr.SetIp(ServerIP);
		ServerAddr.SetPort(ServerPort);

		debugf(NAME_DevOnline, TEXT("SteamLobbyGameCreated: LobbyUID: ") I64_FORMAT_TAG TEXT(", ServerUID: ") I64_FORMAT_TAG
			TEXT(", IP: %s"), LobbyUID, ServerUID, *ServerAddr.ToString(TRUE));


		INT LobbyIndex = INDEX_NONE;

		for (INT i=0; i<CallbackInterface->ActiveLobbies.Num(); i++)
		{
			if (CallbackInterface->ActiveLobbies(i).LobbyUID.Uid == LobbyUID)
			{
				LobbyIndex = i;
				break;
			}
		}


		if (LobbyIndex != INDEX_NONE)
		{
			// Kickoff the callback delegates
			FUniqueNetId ServerId;
			ServerId.Uid = ServerUID;

			CallbackInterface->eventTriggerLobbyJoinGameDelegates(LobbyIndex, ServerId, *ServerAddr.ToString(TRUE));
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("SteamLobbyGameCreated: Received join event for lobby we are not in; LobbyUID: ")
				I64_FORMAT_TAG TEXT(", ServerUID: ") I64_FORMAT_TAG TEXT(", ServerIP: %s"),
				LobbyUID, ServerUID, *ServerAddr.ToString(TRUE));
		}
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyGameCreated);

/**
 * Notification event from Steam notifying the client that a chat message in a lobby has been received
 */
class FOnlineAsyncEventSteamLobbyChatMessage : public FOnlineAsyncEventSteam<LobbyChatMsg_t, UOnlineLobbyInterfaceSteamworks>
{
private:
	/** The UID of the lobby the member is in */
	QWORD			LobbyUID;

	/** The UID of the member who sent the chat message */
	QWORD			MemberUID;

	/** The type of chat message received */
	EChatEntryType		MessageType;

	/** The index of the chat entry (for use with SteamAPI functions) */
	INT			ChatIndex;


	/** Hidden constructor */
	FOnlineAsyncEventSteamLobbyChatMessage()
		: LobbyUID(0)
		, MemberUID(0)
		, MessageType(k_EChatEntryTypeInvalid)
		, ChatIndex(0)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InLobbyInterface	The lobby interface object this event is linked to
	 */
	FOnlineAsyncEventSteamLobbyChatMessage(UOnlineLobbyInterfaceSteamworks* InLobbyInterface)
		: FOnlineAsyncEventSteam(InLobbyInterface)
		, LobbyUID(0)
		, MemberUID(0)
		, MessageType(k_EChatEntryTypeInvalid)
		, ChatIndex(0)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamLobbyChatMessage()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		// @todo: Stringize MessageType
		return FString::Printf(TEXT("FOnlineAsyncEventSteamLobbyChatMessage completed LobbyUID: ") I64_FORMAT_TAG
					TEXT(", MemberUID: ") I64_FORMAT_TAG TEXT(", MessageType: %i, ChatIndex: %i"),
					LobbyUID, MemberUID, (INT)MessageType, ChatIndex);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(LobbyChatMsg_t* CallbackData)
	{
		LobbyUID = CallbackData->m_ulSteamIDLobby;
		MemberUID = CallbackData->m_ulSteamIDUser;
		MessageType = (EChatEntryType)CallbackData->m_eChatEntryType;
		ChatIndex = CallbackData->m_iChatID;

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();


		// If you always provide this as slack when emptying, the array will never be reallocated, which is more efficient
		CallbackInterface->CachedBinaryData.Empty(MAX_LOBBY_CHAT_LENGTH);

		// @todo Steam: Should you do anything with the gamestart type here, or is that handled elsewhere?


		// Parse the message type
		FString Type;

		switch(MessageType)
		{
		case k_EChatEntryTypeInvalid:
			Type = TEXT("Invalid");
			break;

		case k_EChatEntryTypeChatMsg:
			Type = TEXT("Chat");
			break;

		case k_EChatEntryTypeTyping:
			Type = TEXT("Typing");
			break;

		case k_EChatEntryTypeInviteGame:
			Type = TEXT("Invite");
			break;

		case k_EChatEntryTypeEmote:
			Type = TEXT("Emote");
			break;

		// Removed in a Steam SDK update
/*
		case k_EChatEntryTypeLobbyGameStart:
			Type = TEXT("GameStart");
			break;
*/

		case k_EChatEntryTypeLeftConversation:
			Type = TEXT("LeftConversation");
			break;

		default:
			Type = FString::Printf(TEXT("Unknown:%i"), (INT)MessageType);
			break;
		}


		FUniqueNetId LobbyNetId;
		FUniqueNetId MemberNetId;

		LobbyNetId.Uid = LobbyUID;
		MemberNetId.Uid = MemberUID;

		CSteamID LobbySteamId(LobbyUID);
		CSteamID MemberSteamId(MemberUID);


		// Parse the actual message
		TArray<BYTE> ReceivedData;
		ReceivedData.Add(MAX_LOBBY_CHAT_LENGTH + 256);

		EChatEntryType DudType;
		INT ReceivedSize = GSteamMatchmaking->GetLobbyChatEntry(LobbySteamId, ChatIndex, &MemberSteamId, ReceivedData.GetData(),
									ReceivedData.Num(), &DudType);

		// Reject the message if it goes out of bounds
		if (ReceivedSize <= 1 || ReceivedSize > MAX_LOBBY_CHAT_LENGTH)
		{
			debugf(NAME_DevOnline, TEXT("SteamLobbyChatMessage: Bad lobby message entry/size; ReceivedSize: %i, LobbyUID: ")
				I64_FORMAT_TAG TEXT(", MemberUID: ") I64_FORMAT_TAG TEXT(", ChatIndex: %i"),
				ReceivedSize, LobbyUID, MemberUID, ChatIndex);

			return;
		}


		FLobbyChatReader ReceivedReader(ReceivedData);
		BYTE MessageFormat;
		FString Message;

		ReceivedReader << MessageFormat;

		if (MessageFormat == LOBBY_CHAT_TYPE_STRING)
		{
			ReceivedReader << Message;
		}
		else if (MessageFormat == LOBBY_CHAT_TYPE_BINARY)
		{
			CallbackInterface->CachedBinaryData.Add(ReceivedSize-1);
			ReceivedReader.Serialize(CallbackInterface->CachedBinaryData.GetData(), ReceivedSize-1);
		}
		else
		{
			// Read the data anyway, so it's script-accessible, but don't trigger any events
			CallbackInterface->CachedBinaryData.Add(ReceivedSize-1);
			ReceivedReader.Serialize(CallbackInterface->CachedBinaryData.GetData(), ReceivedSize-1);

			debugf(NAME_DevOnline, TEXT("SteamLobbyChatMessage: Unknown message format: %i; LobbyUID: ") I64_FORMAT_TAG
				TEXT(", MemberUID: ") I64_FORMAT_TAG TEXT(", ChatIndex: %i"), LobbyUID, MemberUID, ChatIndex);
		}


		// Success; search for the correct lobby/member, and trigger the callback delegate
		if (!ReceivedReader.IsError())
		{
			INT LobbyIndex = INDEX_NONE;

			for (INT i=0; i<CallbackInterface->ActiveLobbies.Num(); i++)
			{
				if (CallbackInterface->ActiveLobbies(i).LobbyUID == LobbyUID)
				{
					LobbyIndex = i;
					break;
				}
			}

			INT MemberIndex = INDEX_NONE;

			if (LobbyIndex != INDEX_NONE)
			{
				FActiveLobbyInfo& CurActiveLobby = CallbackInterface->ActiveLobbies(LobbyIndex);

				for (INT i=0; i<CurActiveLobby.Members.Num(); i++)
				{
					if (CurActiveLobby.Members(i).PlayerUID == MemberUID)
					{
						MemberIndex = i;
						break;
					}
				}


				if (MessageFormat == LOBBY_CHAT_TYPE_STRING)
				{
					CallbackInterface->eventTriggerLobbyReceiveMessageDelegates(LobbyIndex, MemberIndex, Type, Message);
				}
				else if (MessageFormat == LOBBY_CHAT_TYPE_BINARY)
				{
					CallbackInterface->eventTriggerLobbyReceiveBinaryDataDelegates(LobbyIndex, MemberIndex);
				}
			}
			else
			{
				debugf(NAME_DevOnline,
					TEXT("SteamLobbyChatMessage: Received lobby message for lobby we are not a member of; LobbyUID: ")
					I64_FORMAT_TAG TEXT(", ChatIndex: %i"), LobbyUID, ChatIndex);
			}
		}
		else
		{
			CallbackInterface->CachedBinaryData.Empty(MAX_LOBBY_CHAT_LENGTH);

			debugf(NAME_DevOnline, TEXT("SteamLobbyChatMessage: Error reading lobby message; ReceivedSize: %i, LobbyUID: ")
				I64_FORMAT_TAG TEXT(", MemberUID: ") I64_FORMAT_TAG TEXT(", ChatIndex: %i"),
				ReceivedSize, LobbyUID, MemberUID, ChatIndex);
		}
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyChatMessage);

/**
 * Notification event from Steam notifying the client of lobby search results
 */
class FOnlineAsyncEventSteamLobbyMatchList : public FOnlineAsyncEventSteam<LobbyMatchList_t, UOnlineLobbyInterfaceSteamworks>
{
private:
	/** The number of lobbies returned */
	INT			NumLobbies;


	/** Hidden constructor */
	FOnlineAsyncEventSteamLobbyMatchList()
		: NumLobbies(0)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InLobbyInterface	The lobby interface object this event is linked to
	 */
	FOnlineAsyncEventSteamLobbyMatchList(UOnlineLobbyInterfaceSteamworks* InLobbyInterface)
		: FOnlineAsyncEventSteam(InLobbyInterface)
		, NumLobbies(0)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamLobbyMatchList()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamLobbyMatchList completed NumLobbies: %i"), NumLobbies);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(LobbyMatchList_t* CallbackData)
	{
		NumLobbies = CallbackData->m_nLobbiesMatching;

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();


		debugf(NAME_DevOnline, TEXT("SteamLobbyMatchList: Num Found Lobbies: %i"), NumLobbies);

		CallbackInterface->CachedFindLobbyResults.Empty();

		for (INT i=0; i<NumLobbies; i++)
		{
			CSteamID LobbySteamId = GSteamMatchmaking->GetLobbyByIndex(i);
			INT ResultIndex = CallbackInterface->CachedFindLobbyResults.AddZeroed();
			FBasicLobbyInfo& CurResult = CallbackInterface->CachedFindLobbyResults(ResultIndex);

			CurResult.LobbyUID.Uid = LobbySteamId.ConvertToUint64();


			// Grab the lobby settings
			if (!CallbackInterface->FillLobbySettings(CurResult.LobbySettings, CurResult.LobbyUID))
			{
				// @todo Steam: Add fallback code (kickoff a request and wait) if it's possible for this to fail (may be slow?)
				debugf(NAME_DevOnline, TEXT("SteamLobbyMatchList: Failed to grab lobby settings; LobbyUID: ") I64_FORMAT_TAG,
					CurResult.LobbyUID.Uid);
			}
		}

		CallbackInterface->bLobbySearchInProgress = FALSE;


		// Kickoff the callback delegate
		CallbackInterface->eventTriggerFindLobbiesCompleteDelegates(TRUE);
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyMatchList);

/**
 * Notification event from Steam notifying the client of a lobby invite
 */
class FOnlineAsyncEventSteamLobbyInvite : public FOnlineAsyncEventSteam<LobbyInvite_t, UOnlineLobbyInterfaceSteamworks>
{
private:
	/** The UID of the friend sending the invite */
	QWORD			FriendUID;

	/** The UID of the lobby we're being invited to */
	QWORD			LobbyUID;


	/** Hidden constructor */
	FOnlineAsyncEventSteamLobbyInvite()
		: FriendUID(0)
		, LobbyUID(0)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InLobbyInterface	The lobby interface object this event is linked to
	 */
	FOnlineAsyncEventSteamLobbyInvite(UOnlineLobbyInterfaceSteamworks* InLobbyInterface)
		: FOnlineAsyncEventSteam(InLobbyInterface)
		, FriendUID(0)
		, LobbyUID(0)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamLobbyInvite()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamLobbyInvite completed FriendUID: ") I64_FORMAT_TAG TEXT(", LobbyUID: ")
					I64_FORMAT_TAG, FriendUID, LobbyUID);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(LobbyInvite_t* CallbackData)
	{
		FriendUID = CallbackData->m_ulSteamIDUser;
		LobbyUID = CallbackData->m_ulSteamIDLobby;

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();


		OnlineLobbyInterfaceSteamworks_eventOnLobbyInvite_Parms LobbyInviteParms(EC_EventParm);

		LobbyInviteParms.LobbyId.Uid = LobbyUID;
		LobbyInviteParms.FriendId.Uid = FriendUID;
		LobbyInviteParms.bAccepted = FALSE;

		TriggerOnlineDelegates(CallbackInterface, CallbackInterface->LobbyInviteDelegates, &LobbyInviteParms);
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyInvite);

/**
 * Notification event from Steam notifying the client of an accepted (through overlay) lobby invite
 */
class FOnlineAsyncEventSteamLobbyInviteAccepted : public FOnlineAsyncEventSteam<GameLobbyJoinRequested_t, UOnlineLobbyInterfaceSteamworks>
{
private:
	/** The UID of the friend sending the invite */
	QWORD			FriendUID;

	/** The UID of the lobby we're being invited to */
	QWORD			LobbyUID;


	/** Hidden constructor */
	FOnlineAsyncEventSteamLobbyInviteAccepted()
		: FriendUID(0)
		, LobbyUID(0)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InLobbyInterface	The lobby interface object this event is linked to
	 */
	FOnlineAsyncEventSteamLobbyInviteAccepted(UOnlineLobbyInterfaceSteamworks* InLobbyInterface)
		: FOnlineAsyncEventSteam(InLobbyInterface)
		, FriendUID(0)
		, LobbyUID(0)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamLobbyInviteAccepted()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamLobbyInviteAccepted completed FriendUID: ") I64_FORMAT_TAG TEXT(", LobbyUID: ")
					I64_FORMAT_TAG /** << Part of printf string, not a parameter */,
					FriendUID, LobbyUID);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(GameLobbyJoinRequested_t* CallbackData)
	{
		FriendUID = CallbackData->m_steamIDFriend.ConvertToUint64();
		LobbyUID = CallbackData->m_steamIDLobby.ConvertToUint64();

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();


		OnlineLobbyInterfaceSteamworks_eventOnLobbyInvite_Parms LobbyInviteParms(EC_EventParm);

		LobbyInviteParms.LobbyId.Uid = LobbyUID;
		LobbyInviteParms.FriendId.Uid = FriendUID;
		LobbyInviteParms.bAccepted = TRUE;

		TriggerOnlineDelegates(CallbackInterface, CallbackInterface->LobbyInviteDelegates, &LobbyInviteParms);
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyInviteAccepted);


/**
 * UOnlineLobbyInterfaceSteamworks
 */

// General functions

/**
 * Interface initialization
 *
 * @param InSubsystem	Reference to the initializing subsystem
 */
void UOnlineLobbyInterfaceSteamworks::InitInterface(UOnlineSubsystemSteamworks* InSubsystem)
{
	if (GSteamworksClientInitialized)
	{
		GSteamAsyncTaskManager->RegisterInterface(this);
	}
}

/**
 * Cleanup
 */
void UOnlineLobbyInterfaceSteamworks::FinishDestroy()
{
	if (GSteamAsyncTaskManager != NULL)
	{
		GSteamAsyncTaskManager->UnregisterInterface(this);
	}

	Super::FinishDestroy();
}


// Native function implementation

/**
 * Creates a lobby, joins it, and optionally assigns its initial settings, triggering callbacks when done
 *
 * @param MaxPlayers		The maximum number of lobby members
 * @param Type			The type of lobby to setup (public/private/etc.)
 * @param InitialSettings	The list of settings to apply to the lobby upon creation
 * @return			Returns True if successful, False otherwise
 */
UBOOL UOnlineLobbyInterfaceSteamworks::CreateLobby(INT MaxPlayers, BYTE Type, const TArray<FLobbyMetaData>& InitialSettings)
{
	UBOOL bReturnVal = FALSE;

	// @todo Steam: Add code to detect multiple calls to this function, while lobby creation is already in progress;
	//		decide whether you want to block (could block >ALL< further CreateLobby calls), or spit out a warning
	//		(could also add handling for multiple calls, but I don't think it's possible to individually identify
	//		created lobbies; maybe through playercount or something? low priority in any case)

	CreateLobbySettings.Empty();

	if (GSteamMatchmaking != NULL)
	{
		ELobbyType SteamLobbyType;

		// NOTE: If you update this, update SetLobbyType as well
		switch (Type)
		{
		case LV_Public:
			SteamLobbyType = k_ELobbyTypePublic;
			break;

		case LV_Friends:
			SteamLobbyType = k_ELobbyTypeFriendsOnly;
			break;

		case LV_Private:
			SteamLobbyType = k_ELobbyTypePrivate;
			break;

		case LV_Invisible:
			SteamLobbyType = k_ELobbyTypeInvisible;
			break;

		default:
			SteamLobbyType = k_ELobbyTypePublic;
			debugf(NAME_DevOnline, TEXT("CreateLobby: Bad Type parameter '%i', defaulting to LV_Public"), Type);

			break;
		}

		// @todo Steam: Check that 0 is valid input
		if (MaxPlayers < 0)
		{
			MaxPlayers = 0;
		}

		debugf(NAME_DevOnline, TEXT("Creating lobby of type %i with a maximum of %i members"), Type, MaxPlayers);

		CreateLobbySettings = InitialSettings;
		GSteamMatchmaking->CreateLobby(SteamLobbyType, MaxPlayers);

		bReturnVal = TRUE;
	}

	return bReturnVal;
}

/**
 * Kicks off a search for available lobbies, matching the specified filters, triggering callbacks when done
 *
 * @param MaxResults	The maximum number of results to return
 * @param Filters	Filters used for restricting returned lobbies
 * @param SortFilters	Influences sorting of the returned lobby list, with the first filter influencing the most
 * @param MinSlots	Minimum required number of open slots (@todo Steam: Test to see this doesn't list >exact< number of slots)
 * @param Distance	The desired geographical distance of returned lobbies
 * @return		Returns True if successful, False otherwise
 */
UBOOL UOnlineLobbyInterfaceSteamworks::FindLobbies(INT MaxResults, const TArray<FLobbyFilter>& Filters,
			const TArray<FLobbySortFilter>& SortFilters, INT MinSlots, BYTE Distance)
{
	UBOOL bReturnVal = FALSE;

	CachedFindLobbyResults.Empty();

	if (GSteamMatchmaking != NULL && !bLobbySearchInProgress)
	{
		bLobbySearchInProgress = TRUE;

		// Setup filters
		if (MaxResults > 0)
		{
			GSteamMatchmaking->AddRequestLobbyListResultCountFilter(MaxResults);
		}

		if (MinSlots > 0)
		{
			GSteamMatchmaking->AddRequestLobbyListFilterSlotsAvailable(MinSlots);
		}


		// Distance filter
		ELobbyDistanceFilter SteamDistanceFilter;

		switch (Distance)
		{
		case LD_Best:
			SteamDistanceFilter = k_ELobbyDistanceFilterDefault;
			break;

		case LD_Close:
			SteamDistanceFilter = k_ELobbyDistanceFilterClose;
			break;

		case LD_Far:
			SteamDistanceFilter = k_ELobbyDistanceFilterFar;
			break;

		case LD_Any:
			SteamDistanceFilter = k_ELobbyDistanceFilterWorldwide;
			break;

		default:
			SteamDistanceFilter = k_ELobbyDistanceFilterDefault;
			debugf(NAME_DevOnline, TEXT("FindLobbies: Bad Distance parameter '%i', defaulting to LD_Best"), Distance);
			break;
		}

		if (SteamDistanceFilter != k_ELobbyDistanceFilterDefault)
		{
			GSteamMatchmaking->AddRequestLobbyListDistanceFilter(SteamDistanceFilter);
		}


		// Key/Value filters
		for (INT i=0; i<Filters.Num(); i++)
		{
			const FLobbyFilter& CurFilter = Filters(i);

			if (CurFilter.Key.IsEmpty() || CurFilter.Value.IsEmpty())
			{
				continue;
			}


			ELobbyComparison Operator;

			switch (CurFilter.Operator)
			{
			case OGSCT_Equals:
				Operator = k_ELobbyComparisonEqual;
				break;

			case OGSCT_NotEquals:
				Operator = k_ELobbyComparisonNotEqual;
				break;

			case OGSCT_GreaterThan:
				Operator = k_ELobbyComparisonGreaterThan;
				break;

			case OGSCT_GreaterThanEquals:
				Operator = k_ELobbyComparisonEqualToOrGreaterThan;
				break;

			case OGSCT_LessThan:
				Operator = k_ELobbyComparisonLessThan;
				break;

			case OGSCT_LessThanEquals:
				Operator = k_ELobbyComparisonEqualToOrLessThan;
				break;

			default:
				debugf(NAME_DevOnline, TEXT("FindLobbies: Bad filter operator '%i', defaulting to OGSCT_Equals"),
					CurFilter.Operator);
				Operator = k_ELobbyComparisonEqual;

				break;
			}


			// Add the filter
			if (CurFilter.bNumeric)
			{
				GSteamMatchmaking->AddRequestLobbyListNumericalFilter(TCHAR_TO_UTF8(*CurFilter.Key), appAtoi(*CurFilter.Value),
											Operator);
			}
			else
			{
				GSteamMatchmaking->AddRequestLobbyListStringFilter(TCHAR_TO_UTF8(*CurFilter.Key), TCHAR_TO_UTF8(*CurFilter.Value),
										Operator);
			}
		}


		// Sorting filters
		for (INT i=0; i<SortFilters.Num(); i++)
		{
			const FLobbySortFilter& CurFilter = SortFilters(i);

			if (CurFilter.Key.IsEmpty())
			{
				continue;
			}


			GSteamMatchmaking->AddRequestLobbyListNearValueFilter(TCHAR_TO_UTF8(*CurFilter.Key), CurFilter.TargetValue);
		}


		// Kickoff the search
		GSteamMatchmaking->RequestLobbyList();

		bReturnVal = TRUE;
	}

	return bReturnVal;
}

/**
 * Updates the lobby settings for all current lobby search results, and removes lobbies if they have become invalid
 * NOTE: Triggers OnFindLobbiesComplete when done
 *
 * @param LobbyId	Allows you to specify the id of one particular lobby you want to update
 * @return		Returns True if successful, False otherwise
 */
UBOOL UOnlineLobbyInterfaceSteamworks::UpdateFoundLobbies(FUniqueNetId LobbyId)
{
	// @todo Steam: IMPORTANT: You can probably leave this as a stub, because lobby data updates should be 100% automatic now;
	//		keep it as a stub though, for when you start generalizing the code for all online subsystems

	// @todo Steam
	return FALSE;
}

/**
 * Joins the specified lobby, triggering callbacks when done
 *
 * @param LobbyId	The unique id of the lobby to join
 * @return		Returns True if successful, False otherwise
 */
UBOOL UOnlineLobbyInterfaceSteamworks::JoinLobby(FUniqueNetId LobbyId)
{
	UBOOL bReturnVal = FALSE;

	if (GSteamMatchmaking != NULL)
	{
		UBOOL bAlreadyJoined = FALSE;

		for (INT i=0; i<ActiveLobbies.Num(); i++)
		{
			if (ActiveLobbies(i).LobbyUID == LobbyId)
			{
				bAlreadyJoined = TRUE;
				break;
			}
		}

		if (!bAlreadyJoined)
		{
			CSteamID SteamLobbyId(LobbyId.Uid);

			GSteamMatchmaking->JoinLobby(SteamLobbyId);
			bReturnVal = TRUE;
		}
	}

	return bReturnVal;
}

/**
 * Exits the specified lobby; always returns True, and has no callbacks
 *
 * @param LobbyId	The UID of the lobby to exit
 * @return		Returns True if successful, False otherwise
 */
UBOOL UOnlineLobbyInterfaceSteamworks::LeaveLobby(FUniqueNetId LobbyId)
{
	UBOOL bReturnVal = FALSE;

	for (INT i=0; i<ActiveLobbies.Num(); i++)
	{
		if (ActiveLobbies(i).LobbyUID == LobbyId)
		{
			ActiveLobbies.Remove(i, 1);
			break;
		}
	}

	if (GSteamMatchmaking != NULL)
	{
		CSteamID SteamLobbyId(LobbyId.Uid);
		GSteamMatchmaking->LeaveLobby(SteamLobbyId);
		bReturnVal = TRUE;
	}

	return bReturnVal;
}

/**
 * Changes the value of a setting for the local user in the specified lobby
 * NOTE: You should specify any keys you set, in the 'LobbyMemberKeys' config array; otherwise they aren't read
 *
 * @param LobbyId	The UID of the lobby where the change is to be applied
 * @param Key		The name of the setting to change
 * @param Value		The new value of the setting
 */
UBOOL UOnlineLobbyInterfaceSteamworks::SetLobbyUserSetting(FUniqueNetId LobbyId, const FString& Key, const FString& Value)
{
	UBOOL bReturnVal = FALSE;

	if (GSteamMatchmaking != NULL)
	{
		if (!LobbyMemberKeys.ContainsItem(Key))
		{
			debugf(NAME_DevOnline, TEXT("SetLobbyUserSetting: Warning, key '%s' is not in LobbyMemberKeys, thus is not read"), *Key);
		}

		CSteamID SteamLobbyId(LobbyId.Uid);
		GSteamMatchmaking->SetLobbyMemberData(SteamLobbyId, TCHAR_TO_UTF8(*Key), TCHAR_TO_UTF8(*Value));
		bReturnVal = TRUE;
	}

	return bReturnVal;
}

/**
 * Sends a chat message to the specified lobby
 *
 * @param LobbyId	The UID of the lobby where the message should be sent
 * @param Message	The message to send to the lobby
 * @return		Returns True if the message was sent successfully, False otherwise
 */
UBOOL UOnlineLobbyInterfaceSteamworks::SendLobbyMessage(FUniqueNetId LobbyId, const FString& Message)
{
	UBOOL bReturnVal = FALSE;

	if (GSteamMatchmaking != NULL)
	{
		CSteamID SteamLobbyId(LobbyId.Uid);
		TArray<BYTE> SendData;
		BYTE DataType = LOBBY_CHAT_TYPE_STRING;

		// Special archive for serializing to SendData, but triggers an error if it hits the size limit
		FLobbyChatWriter SendWriter(SendData);

		SendWriter << DataType;
		SendWriter << (FString&)Message;

		if (!SendWriter.IsError())
		{
			if (GSteamMatchmaking->SendLobbyChatMsg(SteamLobbyId, SendData.GetData(), SendData.Num()))
			{
				bReturnVal = TRUE;
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("SendLobbyMessage: Failed to send lobby chat message; LobbyId: ") I64_FORMAT_TAG
					TEXT(", Message: %s"), LobbyId.Uid, *Message);
			}
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("SendLobbyMessage: Error serialization chat message; message length: %i"), Message.Len());
		}
	}

	return bReturnVal;
}

/**
 * Sends binary data to the specified lobby
 *
 * @param LobbyId	The UID of the lobby where the data should be sent
 * @param Data		The binary data which should be sent to the lobby (limit of around 2048 bytes)
 */
UBOOL UOnlineLobbyInterfaceSteamworks::SendLobbyBinaryData(FUniqueNetId LobbyId, const TArray<BYTE>& Data)
{
	UBOOL bReturnVal = FALSE;

	if (GSteamMatchmaking != NULL)
	{
		CSteamID SteamLobbyId(LobbyId.Uid);
		TArray<BYTE> SendData;
		BYTE DataType = LOBBY_CHAT_TYPE_BINARY;

		// Special archive for serializing to SendData, but triggers an error if it hits the size limit
		FLobbyChatWriter SendWriter(SendData);

		SendWriter << DataType;
		SendWriter.Serialize((void*)Data.GetData(), Data.Num());

		if (!SendWriter.IsError())
		{
			if (GSteamMatchmaking->SendLobbyChatMsg(SteamLobbyId, SendData.GetData(), SendData.Num()))
			{
				bReturnVal = TRUE;
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("SendLobbyBinaryData: Failed to send binary data to lobby; LobbyId: ") I64_FORMAT_TAG
					TEXT(", DataLen: %i"), LobbyId.Uid, Data.Num());
			}
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("SendLobbyBinaryData: Error serialization binary data; DataLen: %i"), Data.Num());
		}
	}

	return bReturnVal;
}

/**
 * Returns the UID of the person who is admin of the specified lobby
 *
 * @param LobbyId	The UID of the lobby to check
 * @param AdminId	Outputs the UID of the lobby admin
 * @return		Returns True if successful, False otherwise
 */
UBOOL UOnlineLobbyInterfaceSteamworks::GetLobbyAdmin(FUniqueNetId LobbyId, FUniqueNetId& AdminId)
{
	UBOOL bReturnVal = FALSE;

	if (GSteamMatchmaking != NULL)
	{
		CSteamID SteamLobbyId(LobbyId.Uid);
		CSteamID SteamAdminId = GSteamMatchmaking->GetLobbyOwner(SteamLobbyId);

		AdminId.Uid = SteamAdminId.ConvertToUint64();
		bReturnVal = TRUE;
	}

	return bReturnVal;
}

/**
 * Sets the value of a specified setting, in the specified lobby
 * NOTE: Admin-only
 *
 * @param LobbyId	The UID of the lobby where the setting should be changed
 * @param Key		The name of the setting to change
 * @param Value		The new value for the setting
 * @return		Returns True if successful, False otherwise
 */
UBOOL UOnlineLobbyInterfaceSteamworks::SetLobbySetting(FUniqueNetId LobbyId, const FString& Key, const FString& Value)
{
	UBOOL bReturnVal = FALSE;

	if (GSteamMatchmaking != NULL)
	{
		CSteamID SteamLobbyId(LobbyId.Uid);
		bReturnVal = GSteamMatchmaking->SetLobbyData(SteamLobbyId, TCHAR_TO_UTF8(*Key), TCHAR_TO_UTF8(*Value));
	}

	return bReturnVal;
}

/**
 * Removes the specified setting, from the specified lobby
 * NOTE: Admin-only
 *
 * @param LobbyId	The UID of the lobby where the setting should be removed
 * @param Key		The name of the setting to remove
 * @return		Returns True if successful, False otherwise
 */
UBOOL UOnlineLobbyInterfaceSteamworks::RemoveLobbySetting(FUniqueNetId LobbyId, const FString& Key)
{
	UBOOL bReturnVal = FALSE;

	if (GSteamMatchmaking != NULL)
	{
		CSteamID SteamLobbyId(LobbyId.Uid);
		bReturnVal = GSteamMatchmaking->DeleteLobbyData(SteamLobbyId, TCHAR_TO_UTF8(*Key));
	}

	return bReturnVal;
}

/**
 * Sets the game server to be joined for the specified lobby, triggering OnLobbyJoinGame for all lobby members
 * NOTE: Admin-only
 *
 * @param LobbyId	The UID of the lobby where the game server should be set
 * @param ServerUID	The UID of the game server
 * @param ServerIP	The IP address of the game server
 * @return		Returns True if successful, False otherwise
 */
UBOOL UOnlineLobbyInterfaceSteamworks::SetLobbyServer(FUniqueNetId LobbyId, FUniqueNetId ServerUID, const FString& ServerIP)
{
	UBOOL bReturnVal = FALSE;

	if (GSteamMatchmaking != NULL)
	{
		CSteamID SteamLobbyId(LobbyId.Uid);
		CSteamID SteamServerId(ServerUID.Uid);
		FInternetIpAddr ServerAddr;
		DWORD SteamAddr = 0;

		UBOOL bValidIP = FALSE;
		INT PortDelim = ServerIP.InStr(TEXT(":"));

		if (PortDelim != INDEX_NONE)
		{
			ServerAddr.SetIp(*ServerIP.Left(PortDelim), bValidIP);
			ServerAddr.SetPort(appAtoi(*ServerIP.Mid(PortDelim+1)));
		}
		else
		{
			ServerAddr.SetIp(*ServerIP, bValidIP);
		}

		if (bValidIP)
		{
			ServerAddr.GetIp(SteamAddr);
			GSteamMatchmaking->SetLobbyGameServer(SteamLobbyId, SteamAddr, ServerAddr.GetPort(), SteamServerId);
			bReturnVal = TRUE;
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("SetLobbyServer: Bad IP: %s"), *ServerIP);
		}
	}

	return bReturnVal;
}

/**
 * Changes the visibility/connectivity type for the specified lobby
 * NOTE: Admin-only
 *
 * @param LobbyId	The UID of the lobby where the change should be made
 * @param Type		The new visibility/connectivity type for the lobby
 * @return		Returns True if successful, False otherwise
 */
UBOOL UOnlineLobbyInterfaceSteamworks::SetLobbyType(FUniqueNetId LobbyId, BYTE Type)
{
	UBOOL bReturnVal = FALSE;

	if (GSteamMatchmaking != NULL)
	{
		CSteamID SteamLobbyId(LobbyId.Uid);
		ELobbyType SteamLobbyType;

		// NOTE: If you update this, update CreateLobby as well
		switch (Type)
		{
		case LV_Public:
			SteamLobbyType = k_ELobbyTypePublic;
			break;

		case LV_Friends:
			SteamLobbyType = k_ELobbyTypeFriendsOnly;
			break;

		case LV_Private:
			SteamLobbyType = k_ELobbyTypePrivate;
			break;

		case LV_Invisible:
			SteamLobbyType = k_ELobbyTypeInvisible;
			break;

		default:
			SteamLobbyType = k_ELobbyTypePublic;
			debugf(NAME_DevOnline, TEXT("SetLobbyType: Bad Type parameter '%i', defaulting to LV_Public"), Type);

			break;
		}


		bReturnVal = GSteamMatchmaking->SetLobbyType(SteamLobbyId, SteamLobbyType);
	}

	return bReturnVal;
}

/**
 * Locks/unlocks the specified lobby (i.e. sets whether or not people can join it, regardless of friend/invite status)
 * NOTE: Admin-only
 *
 * @param LobbyId	The UID of the lobby to be locked/unlocked
 * @param bLocked	whether to lock or unlock the lobby
 * @return		Returns True if successful, False otherwise
 */
UBOOL UOnlineLobbyInterfaceSteamworks::SetLobbyLock(FUniqueNetId LobbyId, UBOOL bLocked)
{
	UBOOL bReturnVal = FALSE;

	if (GSteamMatchmaking != NULL)
	{
		CSteamID SteamLobbyId(LobbyId.Uid);
		bReturnVal = GSteamMatchmaking->SetLobbyJoinable(SteamLobbyId, !bLocked);
	}

	return bReturnVal;
}

/**
 * Changes the owner of the specfied lobby
 * NOTE: Admin-only
 *
 * @param LobbyId	The UID of the lobby where ownership should be changed
 * @param NewOwner	The UID of the new lobby owner (must be present in lobby)
 */
UBOOL UOnlineLobbyInterfaceSteamworks::SetLobbyOwner(FUniqueNetId LobbyId, FUniqueNetId NewOwner)
{
	UBOOL bReturnVal = FALSE;

	if (GSteamMatchmaking != NULL)
	{
		CSteamID SteamLobbyId(LobbyId.Uid);
		CSteamID SteamMemberId(NewOwner.Uid);

		bReturnVal = GSteamMatchmaking->SetLobbyOwner(SteamLobbyId, SteamMemberId);
	}

	return bReturnVal;
}

/**
 * Invites a player to the specified lobby
 *
 * @param LobbyId	The UID of the lobby to invite the player to
 * @param PlayerId	The UID of the player to invite
 * @return		Returns True if the invitation was sent successfully, False otherwise
 */
UBOOL UOnlineLobbyInterfaceSteamworks::InviteToLobby(FUniqueNetId LobbyId, FUniqueNetId PlayerID)
{
	UBOOL bReturnVal = FALSE;

	if (GSteamMatchmaking != NULL)
	{
		CSteamID SteamLobbyId(LobbyId.Uid);
		CSteamID SteamPlayerId(PlayerID.Uid);

		bReturnVal = GSteamMatchmaking->InviteUserToLobby(SteamLobbyId, SteamPlayerId);
	}

	return bReturnVal;
}

/**
 * If the player accepted a lobby invite from outside of the game, this grabs the lobby UID from the commandline
 *
 * @param LobbyId		Outputs the UID of the lobby to be joined
 * @param bMarkAsJoined		Set this when the lobby is joined; future calls to this function will return False, but will still output the UID
 * @return			Returns True if a lobby UID is on the commandline, but returns False if it has been joined
 */
UBOOL UOnlineLobbyInterfaceSteamworks::GetLobbyFromCommandline(FUniqueNetId& LobbyId, UBOOL bMarkAsJoined/*=TRUE*/)
{
	// @todo Steam: IMPORTANT: Currently this cannot be implemented, because Steam uses this on the commandline: +connect_lobby 109775241718538587
	//		Unreal interprets this as a mapname because of the '+', so you will either need to modify how Unreal handles parameters
	//		on the commandline, or get Valve to update Steam (the former is preferable I think, unless there is a good reason not to)

	// @todo Steam
	return FALSE;
}


// General utility functions

/**
 * Attempts to parse the specified lobbies settings into the target array, returning FALSE if the data needs to be requested first
 *
 * @param TargetArray	The destination array which is to be filled with the lobby settings
 * @param LobbyId	The UID of the lobby to grab settings for
 * @return		Returns TRUE if successful, FALSE if data needs to be requested first
 */
UBOOL UOnlineLobbyInterfaceSteamworks::FillLobbySettings(TArray<FLobbyMetaData>& TargetArray, FUniqueNetId LobbyId)
{
	UBOOL bReturnVal = FALSE;

	TargetArray.Empty();

	if (GSteamMatchmaking != NULL)
	{
		CSteamID SteamLobbyId(LobbyId.Uid);
		INT DataCount = GSteamMatchmaking->GetLobbyDataCount(SteamLobbyId);

		for (INT DataIndex=0; DataIndex<DataCount; DataIndex++)
		{
			static ANSICHAR Key[1024];
			static ANSICHAR Value[1024];

			if (GSteamMatchmaking->GetLobbyDataByIndex(SteamLobbyId, DataIndex, Key, sizeof(Key), Value, sizeof(Value)))
			{
				INT SettingIndex = TargetArray.AddZeroed();
				FLobbyMetaData& CurSetting = TargetArray(SettingIndex);

				CurSetting.Key = UTF8_TO_TCHAR(Key);
				CurSetting.Value = UTF8_TO_TCHAR(Value);

				// Any values returning is a success
				bReturnVal = TRUE;
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("Failed to grab lobby setting: LobbyId: ") I64_FORMAT_TAG TEXT(", DataIndex: %i"),
					LobbyId.Uid, DataIndex);
			}
		}

		if (DataCount == 0)
		{
			bReturnVal = TRUE;
		}
	}

	return bReturnVal;
}

/**
 * Attempts to parse the settings of a member in the specified lobby, into the target array
 * NOTE: Determines what settings to parse based on the LobbyMemberKeys config list
 *
 * @param TargetArray	The destination array which is to be filled with the lobby member settings
 * @param LobbyId	The UID of the lobby the member is in
 * @param MemberId	The UID of the member to grab settings for
 * @return		Returns TRUE if successful, FALSE otherwise
 */
UBOOL UOnlineLobbyInterfaceSteamworks::FillMemberSettings(TArray<FLobbyMetaData>& TargetArray, FUniqueNetId LobbyId, FUniqueNetId MemberId)
{
	UBOOL bReturnVal = FALSE;

	TargetArray.Empty();

	if (GSteamMatchmaking != NULL)
	{
		CSteamID SteamLobbyId(LobbyId.Uid);
		CSteamID SteamMemberId(MemberId.Uid);

		for (INT i=0; i<LobbyMemberKeys.Num(); i++)
		{
			const TCHAR* CurKey = *LobbyMemberKeys(i);
			const char* CurSteamValue = GSteamMatchmaking->GetLobbyMemberData(SteamLobbyId, SteamMemberId, TCHAR_TO_UTF8(CurKey));

			if (CurSteamValue != NULL)
			{
				INT SettingIndex = TargetArray.AddZeroed();
				FLobbyMetaData& CurSetting = TargetArray(SettingIndex);

				CurSetting.Key = CurKey;
				CurSetting.Value = UTF8_TO_TCHAR(CurSteamValue);

				// Any values returning is a success
				bReturnVal = TRUE;
			}
			else
			{
				// @todo Steam: This fails under normal circumstances sometimes, e.g. when a lobby member first joins; account for this,
				//		and perhaps mark this debug log for later removal

				// NOTE: This is a valid failure, as even if a member key doesn't exist, it still returns an empty string, not NULL
				debugf(NAME_DevOnline, TEXT("Failed to grab lobby member setting: LobbyId: ") I64_FORMAT_TAG TEXT(", MemberId: ")
					I64_FORMAT_TAG TEXT(", Key: %s"), LobbyId.Uid, MemberId.Uid, CurKey);
			}
		}

		if (LobbyMemberKeys.Num() == 0)
		{
			debugf(NAME_DevOnline, TEXT("LobbyMemberKeys does not have any values set, no lobby member settings retrieved"));
			bReturnVal = TRUE;
		}
	}

	return bReturnVal;
}


#endif

#endif


