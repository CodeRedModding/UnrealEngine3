/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemSteamworks.h"

#if WITH_UE3_NETWORKING && WITH_STEAMWORKS

// @todo Steam: A good goal, would be to move all of the auth session tracking and setup code back into IpDrv (most of this is done, but there are
//		some loose ends)


/**
 * Async events/tasks
 */

/**
 * Notification event from Steam that requests that the client disconnect from a server
 */
class FOnlineAsyncEventSteamClientGameServerDeny : public FOnlineAsyncEventSteam<ClientGameServerDeny_t, UOnlineAuthInterfaceSteamworks>
{
private:
	/** The IP of the game server to disconnect from */
	INT	ServerIP;

	/** The port of the server to disconnect from */
	INT	ServerPort;

	/** Whether or not the server has anticheat enabled */
	UBOOL	bSecure;

	/** The SteamAPI error code giving the disconnect reason */
	DWORD	DisconnectReason;


	/** Hidden constructor */
	FOnlineAsyncEventSteamClientGameServerDeny()
		: ServerIP(0)
		, ServerPort(0)
		, bSecure(FALSE)
		, DisconnectReason(0)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InAuthInterface	The auth interface object this event is linked to
	 */
	FOnlineAsyncEventSteamClientGameServerDeny(UOnlineAuthInterfaceSteamworks* InAuthInterface)
		: FOnlineAsyncEventSteam(InAuthInterface)
		, ServerIP(0)
		, ServerPort(0)
		, bSecure(FALSE)
		, DisconnectReason(0)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamClientGameServerDeny()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamClientGameServerDeny completed bSecure: %i, DisconectReason: %d"), bSecure,
					DisconnectReason);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(ClientGameServerDeny_t* CallbackData)
	{
		UBOOL bSuccess = FALSE;

		ServerIP = CallbackData->m_unGameServerIP;
		ServerPort = CallbackData->m_usGameServerPort;
		bSecure = CallbackData->m_bSecure;
		DisconnectReason = CallbackData->m_uReason;

		if (GIsClient && CallbackData->m_uAppID == GSteamAppID)
		{
			bSuccess = TRUE;
		}
		else if (!GIsClient)
		{
			debugf(NAME_DevOnline, TEXT("Received 'ClientGameServerDeny' callback while not a client; ignoring"));
		}
		else if (CallbackData->m_uAppID != GSteamAppID)
		{
			debugf(NAME_DevOnline, TEXT("Received 'ClientGameServerDeny' for wrong appid (%d); ignoring"), CallbackData->m_uAppID);
		}

		return bSuccess;
	}


	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();

		UNetDriver* NetDriver = GetActiveNetDriver();
		UBOOL bTriggerDisconnect = FALSE;

		if (NetDriver != NULL && NetDriver->ServerConnection != NULL)
		{
			FAuthSession* ServerSession = CallbackInterface->GetServerAuthSession(NetDriver->ServerConnection);

			if ((NetDriver->ServerConnection->GetAddrAsInt() == ServerIP && NetDriver->ServerConnection->GetAddrPort() == ServerPort) ||
				(ServerSession != NULL && ServerSession->EndPointIP == ServerIP && ServerSession->EndPointPort == ServerPort))
			{
				bTriggerDisconnect = TRUE;
			}
		}


		if (bTriggerDisconnect)
		{
			debugf(NAME_DevOnline, TEXT("ClientGameServerDeny telling us to disconnect (secure=%d, reason=%d)"), bSecure, DisconnectReason);

			GEngine->Exec(TEXT("DISCONNECT"));
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("ClientGameServerDeny for server we don't appear to be on. Ignoring."));
		}
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamClientGameServerDeny);


inline const TCHAR* AuthResponseString(EAuthSessionResponse Response)
{
	TCHAR* Result;

	switch(Response)
	{
	case k_EAuthSessionResponseOK:
		Result = TEXT("Success");
		break;

	case k_EAuthSessionResponseUserNotConnectedToSteam:
		Result = TEXT("User not connected to Steam");
		break;

	case k_EAuthSessionResponseNoLicenseOrExpired:
		Result = TEXT("No license or expired");
		break;

	case k_EAuthSessionResponseVACBanned:
		Result = TEXT("VAC banned");
		break;

	case k_EAuthSessionResponseLoggedInElseWhere:
		Result = TEXT("Logged in elsewhere");
		break;

	case k_EAuthSessionResponseVACCheckTimedOut:
		Result = TEXT("VAC check timed out");
		break;

	case k_EAuthSessionResponseAuthTicketCanceled:
		Result = TEXT("Auth ticket cancelled");
		break;

	case k_EAuthSessionResponseAuthTicketInvalidAlreadyUsed:
		Result = TEXT("Auth ticket already used");
		break;

	case k_EAuthSessionResponseAuthTicketInvalid:
		Result = TEXT("Auth ticket invalid");
		break;

	default:
		Result = TEXT("Bad response value");
		break;
	}

	return Result;
}

/**
 * Notification event from Steam that returns the final result of a server or peer auth attempt
 */
class FOnlineAsyncEventSteamClientValidateAuth : public FOnlineAsyncEventSteam<ValidateAuthTicketResponse_t, UOnlineAuthInterfaceSteamworks>
{
private:
	/** The player UID this auth result is for */
	QWORD			PlayerUID;

	/** The result of the authentication attempt */
	EAuthSessionResponse	AuthResult;


	/** Hidden constructor */
	FOnlineAsyncEventSteamClientValidateAuth()
		: PlayerUID(0)
		, AuthResult(k_EAuthSessionResponseAuthTicketInvalid)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InAuthInterface	The auth interface object this event is linked to
	 */
	FOnlineAsyncEventSteamClientValidateAuth(UOnlineAuthInterfaceSteamworks* InAuthInterface)
		: FOnlineAsyncEventSteam(InAuthInterface)
		, PlayerUID(0)
		, AuthResult(k_EAuthSessionResponseAuthTicketInvalid)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamClientValidateAuth()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamClientValidateAuth completed PlayerUID: ") I64_FORMAT_TAG
					TEXT(", AuthResult: %s"), PlayerUID, AuthResponseString(AuthResult));
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(ValidateAuthTicketResponse_t* CallbackData)
	{
		PlayerUID = CallbackData->m_SteamID.ConvertToUint64();
		AuthResult = CallbackData->m_eAuthSessionResponse;

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


		debugf(NAME_DevOnline, TEXT("ClientValidateAuth: UID: ") I64_FORMAT_TAG TEXT(", Response: %s"), PlayerUID,
			AuthResponseString(AuthResult));

		UNetDriver* NetDriver = GetActiveNetDriver();
		UNetConnection* ServerConn = (NetDriver != NULL ? NetDriver->ServerConnection : NULL);

		// Check that this result is for the server
		FAuthSession* ServerSession = CallbackInterface->GetServerAuthSession(ServerConn);

		if (ServerSession != NULL)
		{
			UBOOL bSuccess = AuthResult == k_EAuthSessionResponseOK;

			if (ServerSession->EndPointUID.Uid == PlayerUID)
			{
				if (ServerSession->AuthStatus == AUS_Pending)
				{
					ServerSession->AuthStatus = (bSuccess ? AUS_Authenticated : AUS_Failed);

					// Trigger UScript delegates
					OnlineAuthInterfaceImpl_eventOnServerAuthComplete_Parms Parms(EC_EventParm);
					Parms.bSuccess = bSuccess;
					Parms.ServerUID = ServerSession->EndPointUID;
					Parms.ServerConnection = ServerConn;

					if (bSuccess)
					{
						Parms.ExtraInfo = TEXT("Success");
					}
					else
					{
						Parms.ExtraInfo = AuthResponseString(AuthResult);
					}

					TriggerOnlineDelegates(CallbackInterface, CallbackInterface->ServerAuthCompleteDelegates, &Parms);
				}
				else
				{
					debugf(NAME_DevOnline,
						TEXT("ClientValidateAuth: Received server auth result when AuthStatus was not AUS_Pending"));
				}
			}
		}
		// NOTE: It's normal to receive a 'ticket cancelled' response, when there is no server session; that is the server ending the session
		else if (AuthResult != k_EAuthSessionResponseAuthTicketCanceled)
		{
			debugf(NAME_DevOnline, TEXT("ClientValidateAuth: Received auth result without a ServerSession; UID: ") I64_FORMAT_TAG,
				PlayerUID);
		}
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamClientValidateAuth);

inline const TCHAR* DenyReasonString(const EDenyReason Reason)
{
	switch (Reason)
	{
		case k_EDenyInvalid: return TEXT("Invalid");
		case k_EDenyInvalidVersion: return TEXT("Invalid version");
		case k_EDenyGeneric: return TEXT("Generic denial");
		case k_EDenyNotLoggedOn: return TEXT("Not logged on");
		case k_EDenyNoLicense: return TEXT("No license");
		case k_EDenyCheater: return TEXT("VAC banned");
		case k_EDenyLoggedInElseWhere: return TEXT("Logged in elsewhere");
		case k_EDenyUnknownText: return TEXT("Unknown text");
		case k_EDenyIncompatibleAnticheat: return TEXT("Incompatible anti-cheat");
		case k_EDenyMemoryCorruption: return TEXT("Memory corruption detected on client");
		case k_EDenyIncompatibleSoftware: return TEXT("Incompatible software");
		case k_EDenySteamConnectionLost: return TEXT("Steam connection lost");
		case k_EDenySteamConnectionError: return TEXT("Steam connection error");
		case k_EDenySteamResponseTimedOut: return TEXT("Steam response timed out");
		case k_EDenySteamValidationStalled: return TEXT("Steam validation stalled");
		case k_EDenySteamOwnerLeftGuestUser: return TEXT("Steam owner left guest user");
	}

	return TEXT("???");
}

#if WITH_STEAMWORKS_P2PAUTH
/**
 * Notification event from Steam that returns the final result of a client auth attempt to the game server
 */
class FOnlineAsyncEventSteamServerClientValidateAuth : public FOnlineAsyncEventSteam<ValidateAuthTicketResponse_t, UOnlineAuthInterfaceSteamworks>
{
private:
	/** The player UID this auth result is for */
	QWORD			PlayerUID;

	/** The result of the authentication attempt */
	EAuthSessionResponse	AuthResult;


	/** Hidden constructor */
	FOnlineAsyncEventSteamServerClientValidateAuth()
		: PlayerUID(0)
		, AuthResult(k_EAuthSessionResponseAuthTicketInvalid)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InAuthInterface	The auth interface object this event is linked to
	 */
	FOnlineAsyncEventSteamServerClientValidateAuth(UOnlineAuthInterfaceSteamworks* InAuthInterface)
		: FOnlineAsyncEventSteam(InAuthInterface)
		, PlayerUID(0)
		, AuthResult(k_EAuthSessionResponseAuthTicketInvalid)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamServerClientValidateAuth()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServerClientValidateAuth completed PlayerUID: ") I64_FORMAT_TAG
					TEXT(", AuthResult: %s"), PlayerUID, AuthResponseString(AuthResult));
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(ValidateAuthTicketResponse_t* CallbackData)
	{
		PlayerUID = CallbackData->m_SteamID.ConvertToUint64();
		AuthResult = CallbackData->m_eAuthSessionResponse;

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

		FString ExtraInfo = AuthResponseString(AuthResult);
		CallbackInterface->ClientAuthComplete(AuthResult == k_EAuthSessionResponseOK, PlayerUID, ExtraInfo);
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServerClientValidateAuth);

#else
/**
 * Notification event from Steam that returns the game server auth approval for a client
 */
class FOnlineAsyncEventSteamServerClientApprove : public FOnlineAsyncEventSteam<GSClientApprove_t, UOnlineAuthInterfaceSteamworks>
{
private:
	/** The player UID this auth result is for */
	QWORD	PlayerUID;


	/** Hidden constructor */
	FOnlineAsyncEventSteamServerClientApprove()
		: PlayerUID(0)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InAuthInterface	The auth interface object this event is linked to
	 */
	FOnlineAsyncEventSteamServerClientApprove(UOnlineAuthInterfaceSteamworks* InAuthInterface)
		: FOnlineAsyncEventSteam(InAuthInterface)
		, PlayerUID(0)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamServerClientApprove()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServerClientApprove completed PlayerUID: ") I64_FORMAT_TAG, PlayerUID);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(GSClientApprove_t* CallbackData)
	{
		PlayerUID = CallbackData->m_SteamID.ConvertToUint64();

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

		FString ExtraInfo(TEXT(""));
		CallbackInterface->ClientAuthComplete(TRUE, PlayerUID, ExtraInfo);
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServerClientApprove);

/**
 * Notification event from Steam that returns the game server auth denial for a client
 */
class FOnlineAsyncEventSteamServerClientDeny : public FOnlineAsyncEventSteam<GSClientDeny_t, UOnlineAuthInterfaceSteamworks>
{
private:
	/** The player UID this auth result is for */
	QWORD		PlayerUID;

	/** The reason the player was denied */
	EDenyReason	DenyReason;

	/** Extra information about the auth result */
	FString		OptionalInfo;


	/** Hidden constructor */
	FOnlineAsyncEventSteamServerClientDeny()
		: PlayerUID(0)
		, DenyReason(k_EDenyInvalid)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InAuthInterface	The auth interface object this event is linked to
	 */
	FOnlineAsyncEventSteamServerClientDeny(UOnlineAuthInterfaceSteamworks* InAuthInterface)
		: FOnlineAsyncEventSteam(InAuthInterface)
		, PlayerUID(0)
		, DenyReason(k_EDenyInvalid)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamServerClientDeny()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServerClientDeny completed PlayerUID: ") I64_FORMAT_TAG
					TEXT(", DenyReason: %s, OptionalInfo: %s"), PlayerUID, DenyReasonString(DenyReason), *OptionalInfo);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(GSClientDeny_t* CallbackData)
	{
		PlayerUID = CallbackData->m_SteamID.ConvertToUint64();
		DenyReason = CallbackData->m_eDenyReason;
		OptionalInfo = UTF8_TO_TCHAR(CallbackData->m_rgchOptionalText);

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

		FString ExtraInfo = FString::Printf(TEXT("%s - %s"), DenyReasonString(DenyReason), *OptionalInfo);
		CallbackInterface->ClientAuthComplete(FALSE, PlayerUID, ExtraInfo);
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServerClientDeny);

/**
 * Notification event from Steam that requests the game server kick a client
 */
class FOnlineAsyncEventSteamServerClientKick : public FOnlineAsyncEventSteam<GSClientKick_t, UOnlineAuthInterfaceSteamworks>
{
private:
	/** The player UID to be kicked */
	QWORD			PlayerUID;

	/** The reason the player is being kicked */
	EDenyReason		KickReason;


	/** Hidden constructor */
	FOnlineAsyncEventSteamServerClientKick()
		: PlayerUID(0)
		, KickReason(k_EDenyInvalid)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InAuthInterface	The auth interface object this event is linked to
	 */
	FOnlineAsyncEventSteamServerClientKick(UOnlineAuthInterfaceSteamworks* InAuthInterface)
		: FOnlineAsyncEventSteam(InAuthInterface)
		, PlayerUID(0)
		, KickReason(k_EDenyInvalid)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamServerClientKick()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServerClientKick completed PlayerUID: ") I64_FORMAT_TAG
					TEXT(", KickReason: %s"), PlayerUID, DenyReasonString(KickReason));
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(GSClientKick_t* CallbackData)
	{
		PlayerUID = CallbackData->m_SteamID.ConvertToUint64();
		KickReason = CallbackData->m_eDenyReason;

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

		if (IsServer())
		{
			AGameInfo* GameInfo = NULL;

			if (GWorld != NULL && GWorld->GetWorldInfo())
			{
				GameInfo = GWorld->GetWorldInfo()->Game;
			}

			if (GameInfo != NULL)
			{
				FUniqueNetId UniqueId;
				UniqueId.Uid = PlayerUID;

				APlayerController* Controller = FindPlayerControllerForUniqueId(UniqueId);

				if (Controller != NULL)
				{
					UOnlineSubsystemSteamworks* OnlineSub = Cast<UOnlineSubsystemSteamworks>(CallbackInterface->OwningSubsystem);
					FString KickReasonStr = DenyReasonString(KickReason);

					debugf(TEXT("Kicking player at Steam's request. PlayerUID: ") I64_FORMAT_TAG
						TEXT(" ('%s'), KickReason: %s"), PlayerUID, *Controller->PlayerReplicationInfo->PlayerName,
						*KickReasonStr);

					GameInfo->eventForceKickPlayer(Controller, KickReasonStr);

					if (GIsClient && PlayerUID == OnlineSub->LoggedInPlayerId.Uid)
					{
						debugf(TEXT("We're a listen server and we were kicked from our own game!"));
						GEngine->Exec(TEXT("DISCONNECT"));
					}
					// Refresh the game settings
					else if (OnlineSub->CachedGameInt != NULL)
					{
						OnlineSub->CachedGameInt->RefreshPublishedGameSettings();
					}
				}
				else
				{
					debugf(TEXT("Received Steam 'ClientKick' event but could not find player; PlayerUID ") I64_FORMAT_TAG
						TEXT(", KickReason: %s"), PlayerUID, DenyReasonString(KickReason));
				}
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("Received Steam 'ClientKick' event when GameInfo == NULL; PlayerUID: ") I64_FORMAT_TAG
					TEXT(", KickReason: %s"), PlayerUID, DenyReasonString(KickReason));
			}
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("Received Steam 'ClientKick' event when not server; PlayerUID: ") I64_FORMAT_TAG
				TEXT(", KickReason: %s"), PlayerUID, DenyReasonString(KickReason));
		}
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServerClientKick);
#endif


/**
 * Helper functions
 */


/**
 * Retrives a net connection based upon an auth sessions IP address
 * NOTE: Does not check ServerConnection
 *
 * @param InIP		The IP of the auth session
 * @param InPort	The port of the auth session
 * @return		The net connection matching the specified IP and Port
 */
FORCEINLINE UNetConnection* GetNetConnectionFromIP(const INT InIP, const int InPort)
{
	UNetDriver* NetDriver = (GWorld != NULL ? GWorld->GetNetDriver() : NULL);

	if (NetDriver != NULL)
	{
		for (INT i=0; i<NetDriver->ClientConnections.Num(); i++)
		{
			UNetConnection* CurConn = NetDriver->ClientConnections(i);

			if (CurConn->GetAddrAsInt() == InIP && CurConn->GetAddrPort() == InPort)
			{
				return CurConn;
			}
		}
	}

	return NULL;
}


/**
 * UOnlineAuthInterfaceSteamworks implementation
 */

/**
 * Interface initialization
 *
 * @param InSubsystem	Reference to the initializing subsystem
 */
void UOnlineAuthInterfaceSteamworks::InitInterface(UOnlineSubsystemSteamworks* InSubsystem)
{
	OwningSubsystem = InSubsystem;

	GSteamAsyncTaskManager->RegisterInterface(this);

	// By default, the auth interface is always ready to perform auth; it is only marked as >not< ready, when setting up game server auth
	bAuthReady = TRUE;
}

/**
 * Cleanup
 */
void UOnlineAuthInterfaceSteamworks::FinishDestroy()
{
	if (GSteamAsyncTaskManager != NULL)
	{
		GSteamAsyncTaskManager->UnregisterInterface(this);
	}

	Super::FinishDestroy();
}


/**
 * Utility functions
 */

/**
 * Converts a Steam API authentication result, into a human-readable string, and then logs it
 *
 * @param Result	The Steam API auth result
 */
void LogAuthResult(EBeginAuthSessionResult Result)
{
	FString ResultStr = TEXT("");

	switch (Result)
	{
	case k_EBeginAuthSessionResultOK:
		ResultStr = TEXT("Success");
		break;

	case k_EBeginAuthSessionResultInvalidTicket:
		ResultStr = TEXT("Invalid Ticket");
		break;

	case k_EBeginAuthSessionResultDuplicateRequest:
		ResultStr = TEXT("A ticket has already been submitted for this UID");
		break;

	case k_EBeginAuthSessionResultInvalidVersion:
		ResultStr = TEXT("Ticket is from an incompatible interface version");
		break;

	case k_EBeginAuthSessionResultGameMismatch:
		ResultStr = TEXT("Ticket is not for this game");
		break;

	case k_EBeginAuthSessionResultExpiredTicket:
		ResultStr = TEXT("Ticket has expired");
		break;

	default:
		ResultStr = TEXT("Unknown (invalid result)");
		break;
	}

	debugf(NAME_DevOnline, TEXT("BeginAuthSession result: %s"), *ResultStr);
}


/**
 * Control channel messages
 */

/**
 * Control channel message sent from one client to another (relayed by server), requesting an auth session with that client
 *
 * @param Connection		The NetConnection the message came from
 * @param RemoteUID		The UID of the client that sent the request
 */
void UOnlineAuthInterfaceSteamworks::OnAuthRequestPeer(UNetConnection* Connection, QWORD RemoteUID)
{
	if (IsSteamClientAvailable() || IsSteamServerAvailable())
	{
		// @todo Steam: IMPORTANT: Make sure to autodetect when a redirect is necessary, but also when to BLOCK a redirect
		//		Also, if we are a listen host and the RemoteUID is ours, handle the call here as it's directed to host
		//		NOTE: When redirecting, replace RemoteUID (which will be of the client to send to), with the sending connections UID

		// @todo Steam
	}
}

/**
 * Control message sent from one client to another (relayed by server), containing auth ticket data for auth verification
 *
 * @param Connection		The NetConnection the message came from
 * @param RemoteUID		The UID of the client that sent the blob data
 * @param BlobChunk		The current chunk/blob of auth ticket data
 * @param Current		The current sequence of the blob/chunk
 * @param Num			The total number of blobs/chunks being received
 */
void UOnlineAuthInterfaceSteamworks:: OnAuthBlobPeer(UNetConnection* Connection, QWORD RemoteUID, const FString& BlobChunk, BYTE Current, BYTE Num)
{
	if (IsSteamClientAvailable() || IsSteamServerAvailable())
	{
		// @todo Steam: IMPORTANT: Need same redirect detection as OnAuthRequestPeer

		// @todo Steam
	}
}

/**
 * Control message sent from the server to client, for ending an active auth session
 *
 * @param Connection		The NetConnection the message came from
 */
void UOnlineAuthInterfaceSteamworks::OnClientAuthEndSessionRequest(UNetConnection* Connection)
{
	if (IsSteamClientAvailable())
	{
		// Trigger UScript delegates
		OnlineAuthInterfaceImpl_eventOnClientAuthEndSessionRequest_Parms Parms(EC_EventParm);
		Parms.ServerConnection = Connection;

		TriggerOnlineDelegates(this, ClientAuthEndSessionRequestDelegates, &Parms);
	}
}

/**
 * Control message sent from one client to another (relayed by server), for ending an active auth session
 *
 * @param Connection		The NetConnection the message came from
 * @param RemoteUID		The UID of the client that's ending the auth session
 */
void UOnlineAuthInterfaceSteamworks::OnAuthKillPeer(UNetConnection* Connection, QWORD RemoteUID)
{
	if (IsSteamClientAvailable() || IsSteamServerAvailable())
	{
		// @todo Steam: IMPORTANT: Need same redirect detection as OnAuthRequestPeer

		// @todo Steam
	}
}

/**
 * Control message sent from client to server, requesting an auth retry
 *
 * @param Connection		The NetConnection the message came from
 */
void UOnlineAuthInterfaceSteamworks::OnAuthRetry(UNetConnection* Connection)
{
	if (IsSteamClientAvailable() || IsSteamServerAvailable())
	{
		// Trigger UScript delegates
		if (Connection->Driver->ServerConnection == Connection)
		{
			// @todo Steam: Decide whether or not you want to implement this for clients (not necessary so far)
		}
		else
		{
			OnlineAuthInterfaceImpl_eventOnServerAuthRetryRequest_Parms Parms(EC_EventParm);
			Parms.ClientConnection = Connection;

			TriggerOnlineDelegates(this, ServerAuthRetryRequestDelegates, &Parms);
		}
	}
}


/**
 * Control channel send/receive
 */

/**
 * Sends a client auth request to the specified client
 * NOTE: It is important to specify the ClientUID from PreLogin
 *
 * @param ClientConnection	The NetConnection of the client to send the request to
 * @param ClientUID		The UID of the client (as taken from PreLogin)
 * @return			whether or not the request kicked off successfully
 */
UBOOL UOnlineAuthInterfaceSteamworks::SendClientAuthRequest(UPlayer* ClientConnection, FUniqueNetId ClientUID)
{
	UNetDriver* NetDriver = (GWorld != NULL ? GWorld->GetNetDriver() : NULL);
	UNetConnection* ClientConn = NULL;

	// Verify that the client connection is valid (needs more verification than a simple cast)
	if (NetDriver != NULL)
	{
		for (INT i=0; i<NetDriver->ClientConnections.Num(); i++)
		{
			if (NetDriver->ClientConnections(i) == ClientConnection)
			{
				ClientConn = NetDriver->ClientConnections(i);
				break;
			}
		}
	}

	if (ClientConn == NULL)
	{
		debugf(NAME_DevOnline, TEXT("SendClientAuthRequest: Failed to match ClientConnection to NetDriver->ClientConnections list"));
		return FALSE;
	}

	if (GSteamGameServer == NULL)
	{
		debugf(NAME_DevOnline, TEXT("SendClientAuthRequest: GSteamGameServer is not set, failed to retrieve game server info"));
		return FALSE;
	}


	FAuthSession* ClientSession = GetClientAuthSession(ClientConn);

	// If there wasn't an existing session, setup a new one
	if (ClientSession == NULL)
	{
		// SparseArray version of AddZeroed
		FSparseArrayAllocationInfo ElementInfo = ClientAuthSessions.Add();
		appMemzero(ElementInfo.Pointer, sizeof(FAuthSession));

		ClientSession = &ClientAuthSessions(ElementInfo.Index);

		ClientSession->EndPointIP = ClientConn->GetAddrAsInt();
		ClientSession->EndPointPort = ClientConn->GetAddrPort();
		ClientSession->EndPointUID = ClientUID;
	}
	// If there >was< an existing session, cleanup any auth ticket data (otherwise receiving of future auth tickets is blocked)
	else if (ClientSession->AuthTicketUID != 0)
	{
		AuthTicketMap.Remove(ClientSession->AuthTicketUID);
		ClientSession->AuthTicketUID = 0;
	}

	ClientSession->AuthStatus = AUS_NotStarted;

	// @todo Steam: Could have above code in the common IpDrv implementation, and the code below in a platform-specific 'Internal*FuncName*'
	//		function? The drawback of that though, is the common implementation wont exactly be 'complete' (i.e. won't send the
	//		net message itself)

	// Now send the control message
	QWORD ServerID = (QWORD)SteamGameServer_GetSteamID();
	BYTE bServerAntiCheatSecured = (BYTE)(SteamGameServer_BSecure() ? 1 : 0);
	DWORD PublicServerIP = GSteamGameServer->GetPublicIP();
	INT PublicServerPort = 0;

	FString ListenAddr = NetDriver->LowLevelGetNetworkNumber(TRUE);
	INT i = ListenAddr.InStr(TEXT(":"));

	if (i != INDEX_NONE)
	{
		PublicServerPort = appAtoi(*ListenAddr.Mid(i + 1));
	}

	UBOOL bSecure = (UBOOL)bServerAntiCheatSecured;
	FNetControlMessage<NMT_ClientAuthRequest>::Send(ClientConn, ServerID, PublicServerIP, PublicServerPort, bSecure);

	ClientConn->FlushNet();
	return TRUE;
}

/**
 * Sends a server auth request to the server
 *
 * @param ServerUID		The UID of the server
 * @return			whether or not the request kicked off successfully
 */
UBOOL UOnlineAuthInterfaceSteamworks::SendServerAuthRequest(FUniqueNetId ServerUID)
{
	// @todo Steam: IMPORTANT: If you implement this function, you should model it closely on SendServerAuthRetryRequest

	return FALSE;
}


/**
 * Client auth functions
 */

/**
 * Creates a client auth session with the server; the session doesn't start until the auth ticket is verified by the server
 * NOTE: This must be called clientside
 *
 * @param ServerUID		The UID of the server
 * @param ServerIP		The external/public IP address of the server
 * @param ServerPort		The port of the server
 * @param bSecure		whether or not the server has cheat protection enabled
 * @param OutAuthTicketUID	Outputs the UID of the auth data, which is used to verify the auth session on the server
 * @return			whether or not the local half of the auth session was kicked off successfully
 */
UBOOL UOnlineAuthInterfaceSteamworks::CreateClientAuthSession(FUniqueNetId ServerUID, INT ServerIP, INT ServerPort, UBOOL bSecure,
								INT& OutAuthTicketUID)
{
#if AUTH_DEBUG_LOG
	FInternetIpAddr LogAddr;
	LogAddr.SetIp(ServerIP);
	LogAddr.SetPort(ServerPort);

	debugf(NAME_DevOnline, TEXT("CreateClientAuthSession: ServerAddr: %s, ServerUID: ") I64_FORMAT_TAG TEXT(", bSecure: %i"),
		*LogAddr.ToString(TRUE), ServerUID.Uid, (INT)bSecure);
#endif

	UBOOL bSuccess = FALSE;

	checkAtCompileTime(sizeof(DWORD) == sizeof(HAuthTicket), Steamworks_SDK_HAuthTicket_isnt_a_DWORD_anymore);

	if (IsSteamClientAvailable() && bAuthReady)
	{
		// Auth session tracking
		FLocalAuthSession* LocalSession = NULL;

		for (INT i=0; i<LocalClientAuthSessions.GetMaxIndex(); i++)
		{
			if (LocalClientAuthSessions.IsAllocated(i) && LocalClientAuthSessions(i).EndPointIP == ServerIP &&
				LocalClientAuthSessions(i).EndPointPort == ServerPort)
			{
				LocalSession = &LocalClientAuthSessions(i);
				break;
			}
		}

		if (LocalSession == NULL)
		{
			// SparseArray version of AddZeroed
			FSparseArrayAllocationInfo ElementInfo = LocalClientAuthSessions.Add();
			appMemzero(ElementInfo.Pointer, sizeof(FLocalAuthSession));

			LocalSession = &LocalClientAuthSessions(ElementInfo.Index);

			// Store the server address and UID
			LocalSession->EndPointIP = ServerIP;
			LocalSession->EndPointPort = ServerPort;
			LocalSession->EndPointUID = ServerUID;
		}

		// Steam interaction
		FAuthTicketData* CurAuthTicket = CreateAuthTicket(OutAuthTicketUID);
		CurAuthTicket->bComplete = TRUE;

		INT TicketSize = 0;
		CurAuthTicket->FinalAuthTicket.Init(2048);

#if WITH_STEAMWORKS_P2PAUTH
		uint32 ActualSize = 0;

		LocalSession->SessionUID = (DWORD)GSteamUser->GetAuthSessionTicket(CurAuthTicket->FinalAuthTicket.GetData(),
											CurAuthTicket->FinalAuthTicket.Num(), &ActualSize);
		TicketSize = ActualSize;
#else
		LocalSession->SessionUID = 0;

		TicketSize = GSteamUser->InitiateGameConnection(CurAuthTicket->FinalAuthTicket.GetData(), CurAuthTicket->FinalAuthTicket.Num(),
								CSteamID(ServerUID.Uid), ServerIP, ServerPort, (bSecure == TRUE));
#endif	// WITH_STEAMWORKS_P2PAUTH

		if (TicketSize > 0)
		{
			CurAuthTicket->FinalAuthTicket.Remove(TicketSize, 2048 - TicketSize);
			bSuccess = TRUE;
		}
		else
		{
			AuthTicketMap.Remove(OutAuthTicketUID);
			OutAuthTicketUID = 0;
		}

#if AUTH_DEBUG_LOG
		if (bSuccess)
		{
			debugf(NAME_DevOnline, TEXT("CreateClientAuthSession: CurAuthTicket->FinalAuthTicket.Num(): %i, SessionUID: %u"),
				CurAuthTicket->FinalAuthTicket.Num(), LocalSession->SessionUID);

			if (CurAuthTicket->FinalAuthTicket.Num() > 0)
			{
				FString TicketStr = appBlobToString((BYTE*)CurAuthTicket->FinalAuthTicket.GetData(),
									CurAuthTicket->FinalAuthTicket.Num());

				debugf(NAME_DevOnline, TEXT("CreateClientAuthSession: Ticket CRC: %08X"), appStrCrcCaps(*TicketStr));
			}
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("CreateClientAuthSession: Failure"));
		}
#endif


		// This is the first place we learn the servers UID (or IP/Port, if using steam sockets);
		//	set the game join info here, for invites etc.
		UOnlineSubsystemSteamworks* OnlineSub = Cast<UOnlineSubsystemSteamworks>(OwningSubsystem);
		AWorldInfo* WI = (GWorld != NULL ? GWorld->GetWorldInfo() : NULL);

		if (OnlineSub != NULL && WI != NULL && WI->NetMode != NM_ListenServer)
		{
			UBOOL bSteamSockets = IsSteamSocketsClient();

			OnlineSub->SetGameJoinInfo(ServerIP, ServerPort, ServerUID.Uid, bSteamSockets);
		}
	}
	else if (!IsSteamClientAvailable())
	{
		debugf(NAME_DevOnline, TEXT("CreateClientAuthSession: Call to 'CreateClientAuthSession' while IsSteamClientAvailable() is FALSE"));
	}
	else if (!bAuthReady)
	{
		debugf(NAME_DevOnline, TEXT("CreateClientAuthSession: Call to 'CreateClientAuthSession' while bAuthReady is FALSE"));
	}

	return bSuccess;
}

/**
 * Kicks off asynchronous verification and setup of a client auth session, on the server;
 * auth success/failure is returned through OnClientAuthComplete
 *
 * @param ClientUID		The UID of the client
 * @param ClientIP		The IP address of the client
 * @param ClientPort		The port the client is on
 * @param AuthTicketUID		The UID for the auth data sent by the client (as obtained through OnClientAuthResponse)
 * @return			whether or not asynchronous verification was kicked off successfully
 */
UBOOL UOnlineAuthInterfaceSteamworks::VerifyClientAuthSession(FUniqueNetId ClientUID, INT ClientIP, INT ClientPort, INT AuthTicketUID)
{
	UBOOL bSuccess = FALSE;

	FAuthTicketData* CurAuthTicket = AuthTicketMap.Find(AuthTicketUID);

#if AUTH_DEBUG_LOG
	if (CurAuthTicket != NULL && CurAuthTicket->FinalAuthTicket.Num() > 0)
	{
		FInternetIpAddr LogAddr;
		LogAddr.SetIp(ClientIP);
		FString TicketStr = appBlobToString((BYTE*)CurAuthTicket->FinalAuthTicket.GetData(), CurAuthTicket->FinalAuthTicket.Num());

		debugf(NAME_DevOnline, TEXT("VerifyClientAuthSession: ClientIP: %s, ClientUID: ") I64_FORMAT_TAG TEXT(", Ticket CRC: %08X"),
			*LogAddr.ToString(FALSE), ClientUID.Uid, appStrCrcCaps(*TicketStr));
	}
#endif

	if (IsSteamServerAvailable() && bAuthReady && CurAuthTicket != NULL && CurAuthTicket->FinalAuthTicket.Num() > 0)
	{
		if (GSteamGameServer != NULL)
		{
#if WITH_STEAMWORKS_P2PAUTH
			const CSteamID SteamId((uint64)ClientUID.Uid);
			EBeginAuthSessionResult AuthResult = GSteamGameServer->BeginAuthSession(CurAuthTicket->FinalAuthTicket.GetData(),
											CurAuthTicket->FinalAuthTicket.Num(), SteamId);

			if (AuthResult == k_EBeginAuthSessionResultOK)
			{
				bSuccess = TRUE;
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("VerifyClientAuthSession: BeginAuthSession (") I64_FORMAT_TAG
					TEXT(") failed; result:"), ClientUID.Uid);

				LogAuthResult(AuthResult);
			}

#else
			CSteamID SteamId;

			bSuccess = GSteamGameServer->SendUserConnectAndAuthenticate(ClientIP, CurAuthTicket->FinalAuthTicket.GetData(),
									CurAuthTicket->FinalAuthTicket.Num(), &SteamId);

			// May happen if wrong auth ticket UID is passed to function; can also happen if client tries to spoof someones UID,
			//	or use someone elses auth ticket (can't ban their UID though, as it won't be authenticated)
			if (bSuccess && SteamId.ConvertToUint64() != ClientUID.Uid)
			{
				debugf(TEXT("WARNING!!! Client UID does not match auth data UID. Further info:"));
				debugf(TEXT("UID client sent to server: ") I64_FORMAT_TAG TEXT(" (not authenticated), UID in auth data: ")
					I64_FORMAT_TAG TEXT(" (not authenticated)"), ClientUID.Uid, SteamId.ConvertToUint64());

				bSuccess = FALSE;
			}
#endif
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("VerifyClientAuthSession: GSteamGameServer is NULL"));
		}

		if (bSuccess)
		{
			INT ClientSessionIdx = INDEX_NONE;

			for (INT i=0; i<ClientAuthSessions.GetMaxIndex(); i++)
			{
				if (ClientAuthSessions.IsAllocated(i) && ClientAuthSessions(i).EndPointIP == ClientIP &&
					ClientAuthSessions(i).EndPointUID == ClientUID)
				{
					ClientSessionIdx = i;
					break;
				}
			}

			if (ClientSessionIdx == INDEX_NONE)
			{
				// SparseArray version of AddZeroed
				FSparseArrayAllocationInfo ElementInfo = ClientAuthSessions.Add();
				appMemzero(ElementInfo.Pointer, sizeof(FAuthSession));

				ClientSessionIdx = ElementInfo.Index;
				FAuthSession* ClientSession = &ClientAuthSessions(ClientSessionIdx);

				ClientSession->EndPointIP = ClientIP;
				ClientSession->EndPointPort = ClientPort;
				ClientSession->EndPointUID = ClientUID;


				// It's only expected that listen servers should create a new ClientAuthSessions entry here,
				//	so spit out a warning if this is not a listen server
				AWorldInfo* WI = (GWorld != NULL ? GWorld->GetWorldInfo() : NULL);

				if (WI != NULL && WI->NetMode != NM_ListenServer)
				{
					debugf(TEXT("WARNING!!! VerifyClientAuthSession called without existing ClientAuthSessions entry"));
				}
			}

			ClientAuthSessions(ClientSessionIdx).AuthStatus = AUS_Pending;
		}
	}
	else if (!IsSteamServerAvailable())
	{
		debugf(NAME_DevOnline, TEXT("VerifyClientAuthSession: Call to 'VerifyClientAuthSession' while IsSteamServerAvailable() is FALSE"));
	}
	else if (!bAuthReady)
	{
		debugf(NAME_DevOnline, TEXT("VerifyClientAuthSession: Call to 'VerifyClientAuthSession' while bAuthReady is FALSE"));
	}
	else if (CurAuthTicket == NULL)
	{
		debugf(NAME_DevOnline, TEXT("VerifyClientAuthSession: Failed to find auth ticket specified by AuthTicketUID '%i'"), AuthTicketUID);
	}
	else if (CurAuthTicket->FinalAuthTicket.Num() == 0)
	{
		debugf(NAME_DevOnline, TEXT("VerifyClientAuthSession: Auth ticket specified by AuthTicketUID is empty, or pending fill"));
	}

	return bSuccess;
}

/**
 * Internal platform-specific implementation of EndLocalClientAuthSession
 * (Ends the clientside half of a client auth session)
 *
 * @param LocalClientSession	The local client session to end
 */
void UOnlineAuthInterfaceSteamworks::InternalEndLocalClientAuthSession(FLocalAuthSession& LocalClientSession)
{
	checkAtCompileTime(sizeof(DWORD) == sizeof(HAuthTicket), Steamworks_SDK_HAuthTicket_isnt_a_DWORD_anymore);

	if (IsSteamClientAvailable() && bAuthReady)
	{
#if AUTH_DEBUG_LOG
		FInternetIpAddr LogAddr;
		LogAddr.SetIp(LocalClientSession.EndPointIP);
		LogAddr.SetPort(LocalClientSession.EndPointPort);

		debugf(NAME_DevOnline, TEXT("EndLocalClientAuthSession: ServerAddr: %s, ServerUID: ") I64_FORMAT_TAG, *LogAddr.ToString(TRUE),
			LocalClientSession.EndPointUID.Uid);
#endif

		if (GSteamUser != NULL)
		{
#if WITH_STEAMWORKS_P2PAUTH
			GSteamUser->CancelAuthTicket((HAuthTicket)LocalClientSession.SessionUID);
#else
			GSteamUser->TerminateGameConnection(LocalClientSession.EndPointIP, LocalClientSession.EndPointPort);
#endif
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("EndLocalClientAuthSession: Failed to end auth session, GSteamUser == NULL"));
		}


		// Do any post-end-auth actions needed for clients and listen hosts

		// Clear the game join info (relates to invites etc.)
		// @todo Steam: Like in CreateClientAuthSession, look into separating this from the auth code, if there is a neat way of doing so
		UOnlineSubsystemSteamworks* OnlineSub = Cast<UOnlineSubsystemSteamworks>(OwningSubsystem);

		if (OnlineSub != NULL)
		{
			OnlineSub->ClearGameJoinInfo();
		}
	}
	else if (!IsSteamClientAvailable())
	{
		debugf(NAME_DevOnline, TEXT("EndLocalClientAuthSession: Call to 'EndLocalClientAuthSession' while IsSteamClientAvailable() is FALSE"));
	}
	else if (!bAuthReady)
	{
		debugf(NAME_DevOnline, TEXT("EndLocalClientAuthSession: Call to 'EndLocalClientAuthSession' while bAuthReady is FALSE"));
	}
}

/**
 * Internal platform-specific implementation of EndRemoteClientAuthSession
 * (Ends the serverside half of a client auth session)
 *
 * @param ClientSession		The client session to end
 */
void UOnlineAuthInterfaceSteamworks::InternalEndRemoteClientAuthSession(FAuthSession& ClientSession)
{
#if AUTH_DEBUG_LOG
	FInternetIpAddr LogAddr;
	LogAddr.SetIp(ClientSession.EndPointIP);
	LogAddr.SetPort(ClientSession.EndPointPort);

	debugf(NAME_DevOnline, TEXT("EndRemoteClientAuthSession: ClientAddr: %s, ClientUID: ") I64_FORMAT_TAG, *LogAddr.ToString(TRUE),
		ClientSession.EndPointUID.Uid);
#endif

	if (IsSteamServerAvailable() && bAuthReady)
	{
		const CSteamID SteamId(ClientSession.EndPointUID.Uid);

		if (GSteamGameServer != NULL)
		{
#if WITH_STEAMWORKS_P2PAUTH
			GSteamGameServer->EndAuthSession(SteamId);
#else
			GSteamGameServer->SendUserDisconnect(SteamId);
#endif
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("EndRemoteClientAuthSession: GSteamGameServer is NULL"));
		}
	}
	else if (!IsSteamServerAvailable())
	{
		debugf(NAME_DevOnline,
			TEXT("EndRemoteClientAuthSession: Call to 'EndRemoteClientAuthSession' while IsSteamServerAvailable() is FALSE"));
	}
	else if (!bAuthReady)
	{
		debugf(NAME_DevOnline, TEXT("EndRemoteClientAuthSession: Call to 'EndRemoteClientAuthSession' while bAuthReady is FALSE"));
	}
}


/**
 * Server auth functions
 */

/**
 * Creates a server auth session with a client; the session doesn't start until the auth ticket is verified by the client
 * NOTE: This must be called serverside; if using server auth, the server should create a server auth session for every client
 *
 * @param ClientUID		The UID of the client
 * @param ClientIP		The IP address of the client
 * @param ClientPort		The port of the client
 * @param OutAuthTicketUID	Outputs the UID of the auth data, which is used to verify the auth session on the client
 * @return			whether or not the local half of the auth session was kicked off successfully
 */
UBOOL UOnlineAuthInterfaceSteamworks::CreateServerAuthSession(FUniqueNetId ClientUID, INT ClientIP, INT ClientPort, INT& OutAuthTicketUID)
{
#if AUTH_DEBUG_LOG
	FInternetIpAddr LogAddr;
	LogAddr.SetIp(ClientIP);

	debugf(NAME_DevOnline, TEXT("CreateServerAuthSession: ClientAddr: %s, ClientUID: ") I64_FORMAT_TAG, *LogAddr.ToString(FALSE), ClientUID.Uid);
#endif

	UBOOL bSuccess = FALSE;

	checkAtCompileTime(sizeof(DWORD) == sizeof(HAuthTicket), Steamworks_SDK_HAuthTicket_isnt_a_DWORD_anymore);

	if (IsSteamServerAvailable() && bAuthReady)
	{
		// Auth session tracking
		FLocalAuthSession* LocalServerSession = NULL;

		for (INT i=0; i<LocalServerAuthSessions.GetMaxIndex(); i++)
		{
			if (LocalServerAuthSessions.IsAllocated(i) && LocalServerAuthSessions(i).EndPointUID == ClientUID)
			{
				LocalServerSession = &LocalServerAuthSessions(i);
				break;
			}
		}

		if (LocalServerSession == NULL)
		{
			// SparseArray version of AddZeroed
			FSparseArrayAllocationInfo ElementInfo = LocalServerAuthSessions.Add();
			appMemzero(ElementInfo.Pointer, sizeof(FLocalAuthSession));

			LocalServerSession = &LocalServerAuthSessions(ElementInfo.Index);

			// Store IP/UID
			LocalServerSession->EndPointUID = ClientUID;
			LocalServerSession->EndPointIP = ClientIP;
			LocalServerSession->EndPointPort = ClientPort;
		}


		// Steam interaction
		if (GSteamGameServer != NULL)
		{
			FAuthTicketData* CurAuthTicket = CreateAuthTicket(OutAuthTicketUID);
			CurAuthTicket->bComplete = TRUE;

			uint32 ActualSize = 0;
			CurAuthTicket->FinalAuthTicket.Init(2048);

			LocalServerSession->SessionUID = (DWORD)GSteamGameServer->GetAuthSessionTicket(CurAuthTicket->FinalAuthTicket.GetData(),
								CurAuthTicket->FinalAuthTicket.Num(), &ActualSize);


			if (ActualSize > 0)
			{
				CurAuthTicket->FinalAuthTicket.Remove(ActualSize, 2048 - ActualSize);
				bSuccess = TRUE;
			}
			else
			{
				AuthTicketMap.Remove(OutAuthTicketUID);
				OutAuthTicketUID = 0;
			}

#if AUTH_DEBUG_LOG
			if (bSuccess)
			{
				debugf(NAME_DevOnline, TEXT("CreateServerAuthSession: CurAuthTicket->FinalAuthTicket.Num(): %i, SessionUID: %u"),
					CurAuthTicket->FinalAuthTicket.Num(), LocalServerSession->SessionUID);

				if (CurAuthTicket->FinalAuthTicket.Num() > 0)
				{
					FString TicketStr = appBlobToString((BYTE*)CurAuthTicket->FinalAuthTicket.GetData(),
										CurAuthTicket->FinalAuthTicket.Num());

					debugf(NAME_DevOnline, TEXT("CreateServerAuthSession: Ticket CRC: %08X"), appStrCrcCaps(*TicketStr));
				}
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("CreateServerAuthSession: Failure"));
			}
#endif
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("CreateServerAuthSession: GSteamGameServer is NULL"));
		}
	}
	else if (!IsSteamServerAvailable())
	{
		debugf(NAME_DevOnline,
			TEXT("CreateServerAuthSession: Call to 'CreateServerAuthSession' while IsSteamServerAvailable() is FALSE"));
	}
	else if (!bAuthReady)
	{
		debugf(NAME_DevOnline, TEXT("CreateServerAuthSession: Call to 'CreateServerAuthSession' while bAuthReady is FALSE"));
	}

	return bSuccess;
}

/**
 * Kicks off asynchronous verification and setup of a server auth session, on the client;
 * auth success/failure is returned through OnServerAuthComplete
 *
 * @param ServerUID		The UID of the server
 * @param ServerIP		The external/public IP address of the server
 * @param AuthTicketUID		The UID of the auth data sent by the server (as obtained through OnServerAuthResponse)
 * @return			whether or not asynchronous verification was kicked off successfully
 */
UBOOL UOnlineAuthInterfaceSteamworks::VerifyServerAuthSession(FUniqueNetId ServerUID, INT ServerIP, INT AuthTicketUID)
{
	UBOOL bSuccess = FALSE;
	FAuthTicketData* CurAuthTicket = AuthTicketMap.Find(AuthTicketUID);

#if AUTH_DEBUG_LOG
	if (CurAuthTicket != NULL && CurAuthTicket->FinalAuthTicket.Num() > 0)
	{
		FString TicketStr = appBlobToString((BYTE*)CurAuthTicket->FinalAuthTicket.GetData(), CurAuthTicket->FinalAuthTicket.Num());
		debugf(NAME_DevOnline, TEXT("VerifyServerAuthSession: Ticket CRC: %08X"), appStrCrcCaps(*TicketStr));
	}
#endif

	if (IsSteamClientAvailable() && bAuthReady && CurAuthTicket != NULL && CurAuthTicket->FinalAuthTicket.Num() > 0)
	{
		EBeginAuthSessionResult AuthResult = GSteamUser->BeginAuthSession(CurAuthTicket->FinalAuthTicket.GetData(),
										CurAuthTicket->FinalAuthTicket.Num(), CSteamID(ServerUID.Uid));

		if (AuthResult == k_EBeginAuthSessionResultOK)
		{
			bSuccess = TRUE;
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("VerifyServerAuthSession: BeginAuthSession (") I64_FORMAT_TAG TEXT(") failed; result:"),
				ServerUID.Uid);
			LogAuthResult(AuthResult);
		}

#if AUTH_DEBUG_LOG
		FInternetIpAddr LogAddr;
		LogAddr.SetIp(ServerIP);

		debugf(NAME_DevOnline, TEXT("VerifyServerAuthSession: ServerAddr: %s, ServerUID: ") I64_FORMAT_TAG, *LogAddr.ToString(FALSE),
			ServerUID.Uid);
#endif
	}
	else if (!IsSteamClientAvailable())
	{
		debugf(NAME_DevOnline, TEXT("VerifyServerAuthSession: Call to 'VerifyServerAuthSession' while IsSteamClientAvailable() is FALSE"));
	}
	else if (!bAuthReady)
	{
		debugf(NAME_DevOnline, TEXT("VerifyServerAuthSession: Call to 'VerifyServerAuthSession' while bAuthReady is FALSE"));
	}
	else if (CurAuthTicket == NULL)
	{
		debugf(NAME_DevOnline, TEXT("VerifyServerAuthSession: Failed to find auth ticket specified by AuthTicketUID '%i'"), AuthTicketUID);
	}
	else if (CurAuthTicket->FinalAuthTicket.Num() == 0)
	{
		debugf(NAME_DevOnline, TEXT("VerifyServerAuthSession: Auth ticket specified by AuthTicketUID is empty, or pending fill"));
	}

	return bSuccess;
}

/**
 * Internal platform-specific implementation of EndLocalServerAuthSession
 * (Ends the serverside half of a server auth session)
 *
 * @param LocalServerSession	The local server session to end
 */
void UOnlineAuthInterfaceSteamworks::InternalEndLocalServerAuthSession(FLocalAuthSession& LocalServerSession)
{
	checkAtCompileTime(sizeof(DWORD) == sizeof(HAuthTicket), Steamworks_SDK_HAuthTicket_isnt_a_DWORD_anymore);

#if AUTH_DEBUG_LOG
	FInternetIpAddr LogAddr;
	LogAddr.SetIp(LocalServerSession.EndPointIP);

	debugf(NAME_DevOnline, TEXT("EndLocalServerAuthSession: ClientAddr: %s, ClientUID: ") I64_FORMAT_TAG, *LogAddr.ToString(FALSE),
		LocalServerSession.EndPointUID.Uid);
#endif

	if (IsSteamServerAvailable() && GSteamGameServer != NULL && bAuthReady)
	{
#if AUTH_DEBUG_LOG
		debugf(NAME_DevOnline, TEXT("EndLocalServerAuthSession: SessionUID: %d"), LocalServerSession.SessionUID);
#endif

		GSteamGameServer->CancelAuthTicket((HAuthTicket)LocalServerSession.SessionUID);
	}
	else if (!IsSteamServerAvailable())
	{
		debugf(NAME_DevOnline,
			TEXT("EndLocalServerAuthSession: Call to 'EndLocalServerAuthSession' while IsSteamServerAvailable() is FALSE"));
	}
	else if (GSteamGameServer == NULL)
	{
		debugf(NAME_DevOnline, TEXT("EndLocalServerAuthSession: Call to 'EndLocalServerAuthSession' while GSteamGameServer is NULL"));
	}
	else if (!bAuthReady)
	{
		debugf(NAME_DevOnline, TEXT("EndLocalServerAuthSession: Call to 'EndLocalServerAuthSession' while bAuthReady is FALSE"));
	}
}

/**
 * Internal platform-specific implementation of EndRemoteServerAuthSession
 * (Ends the clientside half of a server auth session)
 *
 * @param ServerSession		The server session to end
 */
void UOnlineAuthInterfaceSteamworks::InternalEndRemoteServerAuthSession(FAuthSession& ServerSession)
{
#if AUTH_DEBUG_LOG
	FInternetIpAddr LogAddr;
	LogAddr.SetIp(ServerSession.EndPointIP);
	LogAddr.SetPort(ServerSession.EndPointPort);

	debugf(NAME_DevOnline, TEXT("EndRemoteServerAuthSession: ServerAddr: %s, ServerUID: ") I64_FORMAT_TAG, *LogAddr.ToString(TRUE),
		ServerSession.EndPointUID.Uid);
#endif

	if (IsSteamClientAvailable() && bAuthReady)
	{
		GSteamUser->EndAuthSession(CSteamID(ServerSession.EndPointUID.Uid));
	}
	else if (!IsSteamClientAvailable())
	{
		debugf(NAME_DevOnline, TEXT("EndRemoteServerAuthSession: Call to 'EndRemoteServerAuthSession' while IsSteamClientAvailable() is FALSE"));
	}
	else if (!bAuthReady)
	{
		debugf(NAME_DevOnline, TEXT("EndRemoteServerAuthSession: Call to 'EndRemoteServerAuthSession' while bAuthReady is FALSE"));
	}
}


/**
 * Peer auth functions
 */

// @todo Steam: Implement these eventually; they need to be updated to fit the new auth system layout

#if 0
/**
 * Creates a client auth session with another client; the session doesn't start until the auth ticket is verified by the remote client
 *
 * @param RemoteAddr		The IP address of the remote client
 * @param RemoteUID		The UID of the remote client
 * @param OutSessionUID		Outputs the UID for this auth session; used locally to end the auth session later
 * @param OutAuthTicket		Outputs the auth data used to verify the auth session on the remote client
 * @return			whether or not the local half of the auth session was kicked off successfully
 */
UBOOL UOnlineAuthInterfaceSteamworks::CreatePeerAuthSession(FInternetIpAddr RemoteAddr, QWORD RemoteUID, DWORD& OutSessionUID,
								TArray<BYTE>& OutAuthTicket)
{
#if AUTH_DEBUG_LOG
	debugf(NAME_DevOnline, TEXT("CreatePeerAuthSession: RemoteAddr: %s, RemoteUID: ") I64_FORMAT_TAG, *RemoteAddr.ToString(TRUE), RemoteUID);
#endif

	UBOOL bSuccess = FALSE;

	checkAtCompileTime(sizeof(DWORD) == sizeof(HAuthTicket), Steamworks_SDK_HAuthTicket_isnt_a_DWORD_anymore);

	if (IsSteamClientAvailable() && bAuthReady)
	{
		uint32 ActualSize = 0;
		OutAuthTicket.Init(2048);

		OutSessionUID = (DWORD)GSteamUser->GetAuthSessionTicket(OutAuthTicket.GetData(), OutAuthTicket.Num(), &ActualSize);

		if (ActualSize > 0)
		{
			OutAuthTicket.Remove(ActualSize, 2048 - ActualSize);
			bSuccess = TRUE;
		}

#if AUTH_DEBUG_LOG
		debugf(NAME_DevOnline, TEXT("CreatePeerAuthSession: OutAuthTicket.Num(): %i, OutSessionUID: %u"), OutAuthTicket.Num(), OutSessionUID);

		if (OutAuthTicket.Num() > 0)
		{
			FString TicketStr = appBlobToString((BYTE*)OutAuthTicket.GetData(), OutAuthTicket.Num());
			debugf(NAME_DevOnline, TEXT("CreatePeerAuthSession: Ticket CRC: %08X"), appStrCrcCaps(*TicketStr));
		}
#endif
	}

	return bSuccess;
}

/**
 * Kicks off asynchronous verification and setup of a client auth session, with another client
 *
 * @param RemoteAddr		The IP address of the remote client that created the auth session
 * @param RemoteUID		The UID of the remote client that created the auth session
 * @param AuthTicket		The auth data sent by the remote client
 * @return			whether or not the asynchronous verification was kicked off successfully
 */
UBOOL UOnlineAuthInterfaceSteamworks::VerifyPeerAuthSession(FInternetIpAddr RemoteAddr, QWORD RemoteUID, const TArray<BYTE>& AuthTicket)
{
	UBOOL bSuccess = FALSE;

#if AUTH_DEBUG_LOG
	if (AuthTicket.Num() > 0)
	{
		FString TicketStr = appBlobToString((BYTE*)AuthTicket.GetData(), AuthTicket.Num());
		debugf(NAME_DevOnline, TEXT("VerifyPeerAuthSession: Ticket CRC: %08X"), appStrCrcCaps(*TicketStr));
	}
#endif

	if (IsSteamClientAvailable() && bAuthReady && AuthTicket.Num() > 0)
	{
		EBeginAuthSessionResult AuthResult = GSteamUser->BeginAuthSession(AuthTicket.GetData(), AuthTicket.Num(), CSteamID(RemoteUID));

		if (AuthResult == k_EBeginAuthSessionResultOK)
		{
			bSuccess = TRUE;
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("VerifyPeerAuthSession: BeginAuthSession (") I64_FORMAT_TAG TEXT(") failed; result:"), RemoteUID);
			LogAuthResult(AuthResult);
		}

#if AUTH_DEBUG_LOG
		debugf(NAME_DevOnline, TEXT("VerifyPeerAuthSession: RemoteAddr: %s, RemoteUID: ") I64_FORMAT_TAG, *RemoteAddr.ToString(TRUE),
			RemoteUID);
#endif
	}

	return bSuccess;
}

/**
 * Ends the local half of a peer auth session
 * NOTE: This call must be matched on the other end, with appSteamEndRemotePeerAuthSession
 *
 * @param RemoteAddr		The IP address of the remote client
 * @param RemoteUID		The UID of the remote client
 * @param SessionUID		The UID of the auth session, as output by appSteamCreatePeerAuthSession
 */
void UOnlineAuthInterfaceSteamworks::EndLocalPeerAuthSession(FInternetIpAddr RemoteAddr, QWORD RemoteUID, DWORD SessionUID)
{
	checkAtCompileTime(sizeof(DWORD) == sizeof(HAuthTicket), Steamworks_SDK_HAuthTicket_isnt_a_DWORD_anymore);

#if AUTH_DEBUG_LOG
	debugf(NAME_DevOnline, TEXT("EndLocalPeerAuthSession: RemoteAddr: %s, RemoteUID: ") I64_FORMAT_TAG TEXT(", SessionUID: %u"),
		*RemoteAddr.ToString(TRUE), RemoteUID, SessionUID);
#endif

	if (IsSteamClientAvailable() && bAuthReady)
	{
		GSteamUser->CancelAuthTicket((HAuthTicket)SessionUID);
	}
}

/**
 * Ends a remotely created peer auth session
 * NOTE: This call must be matched on the other end, with appSteamEndLocalPeerAuthSession
 *
 * @param RemoteAddr		The IP address of the remote client
 * @param RemoteUID		The UID of the remote client
 */
void UOnlineAuthInterfaceSteamworks::EndRemotePeerAuthSession(FInternetIpAddr RemoteAddr, QWORD RemoteUID)
{
#if AUTH_DEBUG_LOG
	debugf(NAME_DevOnline, TEXT("EndRemotePeerAuthSession: RemoteAddr: %s, RemoteUID: ") I64_FORMAT_TAG, *RemoteAddr.ToString(TRUE), RemoteUID);
#endif

	if (IsSteamClientAvailable() && bAuthReady)
	{
		GSteamUser->EndAuthSession(CSteamID(RemoteUID));
	}
}
#endif


/**
 * Auth callbacks/utility functions
 */

/**
 * Called when GSteamGameServer is fully setup and ready to authenticate players
 */
void UOnlineAuthInterfaceSteamworks::NotifyGameServerAuthReady()
{
	// Handle listen host auth
	if (GSteamGameServer != NULL)
	{
		if (!bAuthReady)
		{
			bAuthReady = TRUE;

			// Notify script that the auth interface is ready
			OnlineAuthInterfaceImpl_eventOnAuthReady_Parms Parms(EC_EventParm);
			TriggerOnlineDelegates(this, AuthReadyDelegates, &Parms);
		}
	}
}

/**
 * @todo Steam: Proper function desc
// Handles client authentication success/fails
 */
void UOnlineAuthInterfaceSteamworks::ClientAuthComplete(UBOOL bSuccess, const QWORD SteamId, const FString ExtraInfo)
{
	UOnlineSubsystemSteamworks* OnlineSub = Cast<UOnlineSubsystemSteamworks>(OwningSubsystem);

	if (!IsServer())
	{
#if AUTH_DEBUG_LOG
		debugf(NAME_DevOnline, TEXT("(not-server) ClientAuthComplete: bSuccess: %i, SteamId: ") I64_FORMAT_TAG TEXT(", ExtraInfo: %s"),
			(INT)bSuccess, SteamId, *ExtraInfo);
#endif

		debugf(NAME_DevOnline, TEXT("We're not the server, not checking auth!"));
		return;
	}


#if AUTH_DEBUG_LOG
	debugf(NAME_DevOnline, TEXT("ClientAuthComplete: bSuccess: %i, SteamId: ") I64_FORMAT_TAG TEXT(", ExtraInfo: %s"),
		(INT)bSuccess, SteamId, *ExtraInfo);
#endif

	FUniqueNetId UniqueId;
	UniqueId.Uid = SteamId;

	// Find and update the client auth session for this UID
	INT ClientSessionIdx = INDEX_NONE;

	for (INT i=0; i<ClientAuthSessions.GetMaxIndex(); i++)
	{
		if (ClientAuthSessions.IsAllocated(i) && ClientAuthSessions(i).EndPointUID == UniqueId)
		{
			ClientSessionIdx = i;
			break;
		}
	}

	if (ClientSessionIdx != INDEX_NONE)
	{
		FAuthSession* ClientSession = &ClientAuthSessions(ClientSessionIdx);

		// Also need to ensure this player is still connected
		UNetConnection* ClientConn = GetNetConnectionFromIP(ClientSession->EndPointIP, ClientSession->EndPointPort);

		// If this is the result for a listen server host, continue anyway
		AWorldInfo* WI = (GWorld != NULL ? GWorld->GetWorldInfo() : NULL);

		if (ClientConn != NULL || (WI != NULL && WI->NetMode == NM_ListenServer && OnlineSub->LoggedInPlayerId == UniqueId))
		{
			// Update the auth status and trigger UScript delegates
			UBOOL bWasPending = ClientSession->AuthStatus == AUS_Pending;

			if (bWasPending)
			{
				ClientSession->AuthStatus = (bSuccess ? AUS_Authenticated : AUS_Failed);

				OnlineAuthInterfaceImpl_eventOnClientAuthComplete_Parms Parms(EC_EventParm);
				Parms.bSuccess = bSuccess;
				Parms.ClientUID = ClientSession->EndPointUID;
				Parms.ClientConnection = ClientConn;
				Parms.ExtraInfo = ExtraInfo;

				TriggerOnlineDelegates(this, ClientAuthCompleteDelegates, &Parms);
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("ClientAuthComplete: Received auth result when AuthStatus was not AUS_Pending"));
			}
		}
#if AUTH_DEBUG_LOG
		else if (ClientConn == NULL)
		{
			FInternetIpAddr LogAddr;
			LogAddr.SetIp(ClientSession->EndPointIP);
			LogAddr.SetPort(ClientSession->EndPointPort);

			debugf(NAME_DevOnline, TEXT("WARNING!!! ClientAuthComplete: ClientConn == NULL; IP: %s"), *LogAddr.ToString(TRUE));
		}
#endif
	}
#if AUTH_DEBUG_LOG
	else
	{
		debugf(NAME_DevOnline, TEXT("WARNING!!! ClientAuthComplete: Could not find ClientAuthSession matching UID: ") I64_FORMAT_TAG,
			SteamId);
	}
#endif


	// Refresh the game settings
	if (OnlineSub->CachedGameInt)
	{
		OnlineSub->CachedGameInt->RefreshPublishedGameSettings();
	}
}


/**
 * Server information
 */

/**
 * If this is a server, retrieves the platform-specific UID of the server; used for authentication (not supported on all platforms)
 * NOTE: This is primarily used serverside, for listen host authentication
 *
 * @param OutServerUID		The UID of the server
 * @return			whether or not the server UID was retrieved
 */
UBOOL UOnlineAuthInterfaceSteamworks::GetServerUniqueId(FUniqueNetId& OutServerUID)
{
	if (GSteamworksGameServerConnected && bAuthReady && GSteamGameServer != NULL)
	{
		OutServerUID.Uid = SteamGameServer_GetSteamID();
		return TRUE;
	}
	else if (!GSteamworksGameServerConnected)
	{
		debugf(NAME_DevOnline, TEXT("GetServerUniqueId: Call to 'GetServerUniqueId' while GSteamworksGameServerConnected is FALSE"));
	}
	else if (!bAuthReady)
	{
		debugf(NAME_DevOnline, TEXT("GetServerUniqueId: Call to 'GetServerUniqueId' while bAuthReady is FALSE"));
	}
	else if (GSteamGameServer == NULL)
	{
		debugf(NAME_DevOnline, TEXT("GetServerUniqueId: Call to 'GetServerUniqueId' while GSteamGameServer is NULL"));
	}

	return FALSE;
}

/**
 * If this is a server, retrieves the IP and port of the server; used for authentication
 * NOTE: This is primarily used serverside, for listen host authentication
 *
 * @param OutServerIP		The public IP of the server (or, for platforms which don't support it, the local IP)
 * @param OutServerPort		The port of the server
 */
UBOOL UOnlineAuthInterfaceSteamworks::GetServerAddr(INT& OutServerIP, INT& OutServerPort)
{
	if (GSteamworksGameServerConnected && bAuthReady && GSteamGameServer != NULL)
	{
		OutServerIP = GSteamGameServer->GetPublicIP();

		UNetDriver* NetDriver = (GWorld != NULL ? GWorld->GetNetDriver() : NULL);

		if (NetDriver != NULL)
		{
			FString ListenAddr = NetDriver->LowLevelGetNetworkNumber(TRUE);
			INT i = ListenAddr.InStr(TEXT(":"));

			if (i != INDEX_NONE)
			{
				OutServerPort = appAtoi(*ListenAddr.Mid(i + 1));
			}
		}

		return TRUE;
	}
	else if (!GSteamworksGameServerConnected)
	{
		debugf(NAME_DevOnline, TEXT("GetServerAddr: Call to 'GetServerAddr' while GSteamworksGameServerConnected is FALSE"));
	}
	else if (!bAuthReady)
	{
		debugf(NAME_DevOnline, TEXT("GetServerAddr: Call to 'GetServerAddr' while bAuthReady is FALSE"));
	}
	else if (GSteamGameServer == NULL)
	{
		debugf(NAME_DevOnline, TEXT("GetServerAddr: Call to 'GetServerAddr' while GSteamGameServer is NULL"));
	}

	return FALSE;
}


#endif	// WITH_UE3_NETWORKING && WITH_STEAMWORKS


