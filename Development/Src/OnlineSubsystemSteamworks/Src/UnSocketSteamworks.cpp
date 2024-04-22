/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemSteamworks.h"

#if WITH_STEAMWORKS_SOCKETS

/**
 * Globals
 */

/** Manages tracking of active steam sockets, and handling of general socket management/events */
FSteamSocketsManager*		GSteamSocketsManager = NULL;


/**
 * Utility functions
 */

/**
 * Converts and EP2PSessionError value to a readable/descriptive string
 *
 * @param InError	The input error code
 * @return		The resulting descriptive string representing the error code
 */
FORCEINLINE FString GetP2PConnectError(INT InError)
{
	FString ReturnVal = TEXT("");

	switch (InError)
	{
	case k_EP2PSessionErrorNone:
		ReturnVal = TEXT("None");
		break;

	case k_EP2PSessionErrorNotRunningApp:
		ReturnVal = TEXT("NotRunningApp");
		break;

	case k_EP2PSessionErrorNoRightsToApp:
		ReturnVal = TEXT("NoRightsToApp");
		break;

	case k_EP2PSessionErrorDestinationNotLoggedIn:
		ReturnVal = TEXT("DestinationNotLoggedIn");
		break;

	case k_EP2PSessionErrorTimeout:
		ReturnVal = TEXT("Timeout");
		break;

	default:
		ReturnVal = TEXT("Unknown Error");
		break;
	}


	return ReturnVal;
}


/**
 * Async events/tasks
 */

/**
 * Notification event from Steam which notifies the client of a Steam sockets connect failure
 */
class FOnlineAsyncEventSteamSocketsConnectFail : public FOnlineAsyncEventSteam<P2PSessionConnectFail_t, FSteamSocketsManager>
{
private:
	/** The remote Steam UID we were connected/connecting to */
	QWORD			RemoteUID;

	/** The reason the connection failed */
	EP2PSessionError	FailReason;


	/** Hidden constructor */
	FOnlineAsyncEventSteamSocketsConnectFail()
		: RemoteUID(0)
		, FailReason(k_EP2PSessionErrorNone)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSocketsManager	The sockets manager object this event is linked to
	 */
	FOnlineAsyncEventSteamSocketsConnectFail(FSteamSocketsManager* InSocketsManager)
		: FOnlineAsyncEventSteam(InSocketsManager)
		, RemoteUID(0)
		, FailReason(k_EP2PSessionErrorNone)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamSocketsConnectFail()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamSocketsConnectFail completed RemoteUID: ") I64_FORMAT_TAG
					TEXT(", FailReason: %s"), RemoteUID, *GetP2PConnectError(FailReason));
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(P2PSessionConnectFail_t* CallbackData)
	{
		RemoteUID = CallbackData->m_steamIDRemote.ConvertToUint64();
		FailReason = (EP2PSessionError)CallbackData->m_eP2PSessionError;

		return TRUE;
	}


	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();

		FString FailReasonStr = GetP2PConnectError(FailReason);

		debugf(NAME_DevOnline, TEXT("SteamSocketsConnectFail: Connection to server failed; UID: ") I64_FORMAT_TAG
			TEXT(", Reason: %s"), RemoteUID, *FailReasonStr);


		if (GSteamNetworking != NULL && GSteamSocketsManager != NULL)
		{
			UNetDriver* NetDriver = GetActiveNetDriver();

			// Check that it was the server connection that failed, and set the socket to trigger a 'port unreachables' error, so that
			//	the higher-level engine drops the players immediately (instead of timing out)
			if (NetDriver != NULL && NetDriver->ServerConnection != NULL)
			{
				UTcpipConnection* ServerConn = Cast<UTcpipConnection>(NetDriver->ServerConnection);
				CSteamID ServerSteamID;

				if (ServerConn != NULL && IpAddrToSteamId(ServerConn->RemoteAddr, ServerSteamID) &&
					RemoteUID == ServerSteamID.ConvertToUint64())
				{
					// Check that the socket is a valid (currently tracked) steam socket
					FSocketSteamworks* ServerSocket = GSteamSocketsManager->GetTrackedSocket(ServerConn->Socket);

					// If so, set its PortUnreachables value
					if (ServerSocket != NULL)
					{
						ServerSocket->PortUnreachables.AddItem(RemoteUID);
					}
				}
			}


			GSteamSocketsManager->CloseP2PSession(RemoteUID, GSteamNetworking);
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

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamSocketsConnectFail);

/**
 * Notification event from Steam which notifies the game server of a Steam sockets connection request
 */
class FOnlineAsyncEventSteamServerSocketsConnectRequest : public FOnlineAsyncEventSteam<P2PSessionRequest_t, FSteamSocketsManager>
{
private:
	/** The remote Steam UID requesting a connection */
	QWORD			RemoteUID;


	/** Hidden constructor */
	FOnlineAsyncEventSteamServerSocketsConnectRequest()
		: RemoteUID(0)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSocketsManager	The sockets manager object this event is linked to
	 */
	FOnlineAsyncEventSteamServerSocketsConnectRequest(FSteamSocketsManager* InSocketsManager)
		: FOnlineAsyncEventSteam(InSocketsManager)
		, RemoteUID(0)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamServerSocketsConnectRequest()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServerSocketsConnectRequest completed RemoteUID: ") I64_FORMAT_TAG, RemoteUID);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(P2PSessionRequest_t* CallbackData)
	{
		RemoteUID = CallbackData->m_steamIDRemote.ConvertToUint64();

		return TRUE;
	}


	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();


		debugf(NAME_DevOnline, TEXT("SteamServerSocketsConnectRequest: Got steam sockets request from UID: ") I64_FORMAT_TAG, RemoteUID);

		// Don't accept connections from the local UID
		if (GSteamSocketsManager != NULL && GSteamGameServerNetworking != NULL && RemoteUID != SteamGameServer_GetSteamID())
		{
			CSteamID RemoteSteamId(RemoteUID);

			GSteamGameServerNetworking->AcceptP2PSessionWithUser(RemoteSteamId);
			GSteamSocketsManager->TouchP2PSession(RemoteUID);
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

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServerSocketsConnectRequest);

/**
 * Notification event from Steam which notifies the game server of a Steam sockets connect failure on a client connection
 */
class FOnlineAsyncEventSteamServerSocketsConnectFail : public FOnlineAsyncEventSteam<P2PSessionConnectFail_t, FSteamSocketsManager>
{
private:
	/** The remote Steam UID that was connected/connecting to us */
	QWORD			RemoteUID;

	/** The reason the connection failed */
	EP2PSessionError	FailReason;


	/** Hidden constructor */
	FOnlineAsyncEventSteamServerSocketsConnectFail()
		: RemoteUID(0)
		, FailReason(k_EP2PSessionErrorNone)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSocketsManager	The sockets manager object this event is linked to
	 */
	FOnlineAsyncEventSteamServerSocketsConnectFail(FSteamSocketsManager* InSocketsManager)
		: FOnlineAsyncEventSteam(InSocketsManager)
		, RemoteUID(0)
		, FailReason(k_EP2PSessionErrorNone)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamServerSocketsConnectFail()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServerSocketsConnectFail completed RemoteUID: ") I64_FORMAT_TAG
					TEXT(", FailReason: %s"), RemoteUID, *GetP2PConnectError(FailReason));
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(P2PSessionConnectFail_t* CallbackData)
	{
		RemoteUID = CallbackData->m_steamIDRemote.ConvertToUint64();
		FailReason = (EP2PSessionError)CallbackData->m_eP2PSessionError;

		return TRUE;
	}


	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();

		// NOTE: This is triggered on game servers, when client steam sockets connections fail

		FString FailReasonStr = GetP2PConnectError(FailReason);

		debugf(NAME_DevOnline, TEXT("SteamServerSocketsConnectFail: Connection to client failed; UID: ") I64_FORMAT_TAG
			TEXT(", Reason: %s"), RemoteUID, *FailReasonStr);


		if (GSteamGameServerNetworking != NULL && GSteamSocketsManager != NULL)
		{
			UNetDriver* NetDriver = GetActiveNetDriver();

			// Scan through ClientConnections to try and determine which connection has failed, and set the socket to trigger a
			//	'port unreachable' error, so that the higher-level engine drops the players immediately (instead of timing out)
			if (NetDriver != NULL)
			{
				for (INT ConnIdx=0; ConnIdx<NetDriver->ClientConnections.Num(); ConnIdx++)
				{
					UTcpipConnection* CurConn = Cast<UTcpipConnection>(NetDriver->ClientConnections(ConnIdx));
					CSteamID CurSteamID;

					if (CurConn != NULL && IpAddrToSteamId(CurConn->RemoteAddr, CurSteamID) &&
						RemoteUID == CurSteamID.ConvertToUint64())
					{
						// Check that this is a tracked steamworks socket
						FSocketSteamworks* ClientSocket = GSteamSocketsManager->GetTrackedSocket(CurConn->Socket);

						if (ClientSocket != NULL)
						{
							ClientSocket->PortUnreachables.AddItem(RemoteUID);
						}
					}
				}
			}


			GSteamSocketsManager->CloseP2PSession(RemoteUID, GSteamGameServerNetworking);
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

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServerSocketsConnectFail);


/**
 * FSteamSocketsManager implementation
 */

/**
 * Constructor stuff that needs to go in .cpp
 */
void FSteamSocketsManager::InitializeSocketsManager()
{
	if (GSteamAsyncTaskManager != NULL)
	{
		GSteamAsyncTaskManager->RegisterInterface(this);
	}
}

/**
 * Called when the Steamworks game server interface is initialized
 */
void FSteamSocketsManager::InitGameServer()
{
	// Finish initializing any server P2P sockets that were waiting upon GSteamGameServerNetworking
	if (GSteamGameServerNetworking != NULL && UninitializedServerSockets.Num() > 0)
	{
		debugf(NAME_DevOnline, TEXT("Initializing game server P2P sockets"));

		for (INT i=0; i<UninitializedServerSockets.Num(); i++)
		{
			if (UninitializedServerSockets(i) != NULL)
			{
				UninitializedServerSockets(i)->SetNetworkingInterface(GSteamGameServerNetworking);
			}
		}

		UninitializedServerSockets.Empty();

		// Disable network relay connections; otherwise, Steam relays packets through a server when NAT punching fails,
		//	adding unreasonable latency)
		GSteamGameServerNetworking->AllowP2PPacketRelay(FALSE);
	}
}


/**
 * Sockets manager tick
 *
 * @param DeltaTime	The difference in time since the last tick
 */
void FSteamSocketsManager::Tick(FLOAT DeltaTime)
{
	// Check for idle P2P sessions, closing them if they've lingered too long
	if (ActiveP2PSessions.Num() > 0)
	{
		TArray<QWORD> CloseList;

		FLOAT CurSeconds = appSeconds();
		FLOAT ConnectionTimeout = 30.0;

		// Grab the timeout value from the net driver, if applicable
		UNetDriver* NetDriver = GetActiveNetDriver();

		if (NetDriver != NULL)
		{
			ConnectionTimeout = NetDriver->ConnectionTimeout;
		}

		for (TMap<QWORD, FLOAT>::TIterator It(ActiveP2PSessions); It; ++It)
		{
			// If the connection times out, drop it
			if ((CurSeconds - It.Value()) >= ConnectionTimeout + 5.0)
			{
				CloseList.AddItem(It.Key());
			}
		}

		for (INT i=0; i<CloseList.Num(); i++)
		{
			CloseP2PSession(CloseList(i));
		}
	}


#if STEAM_SOCKETS_DEBUG
	static const INT P2PDumpInterval = 5.0;
	static FLOAT P2PDumpCounter = 0.0f;

	if ((CurSeconds - P2PDumpCounter) >= P2PDumpInterval)
	{
		P2PDumpCounter = CurSeconds;

		debugf(NAME_DevOnline, TEXT("Dumping active P2P session details:"));

		for (TMap<QWORD, FLOAT>::TIterator It(ActiveSteamP2PSessions); It; ++It)
		{
			debugf(NAME_DevOnline, TEXT("UID: ") I64_FORMAT_TAG TEXT(", IdleTime: %f"), It.Key(), (CurSeconds - It.Value()));

			debugf(NAME_DevOnline, TEXT("- Detailed P2P session info:"));

			CSteamID SessionId(It.Key());
			P2PSessionState_t SessionInfo;

			if (GSteamNetworking != NULL && GSteamNetworking->GetP2PSessionState(SessionId, &SessionInfo))
			{
				debugf(NAME_DevOnline, TEXT("-- ConnectionActive: %i, Connecting: %i, SessionError: %i, UsingRelay: %i"),
					SessionInfo.m_bConnectionActive, SessionInfo.m_bConnecting, SessionInfo.m_eP2PSessionError,
					SessionInfo.m_bUsingRelay);

				debugf(NAME_DevOnline, TEXT("-- QueuedBytes: %i, QueuedPackets: %i"), SessionInfo.m_nBytesQueuedForSend,
					SessionInfo.m_nPacketsQueuedForSend);
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("- FAILED TO RETRIEVE EXTRA DATA"));
			}
		}
	}
#endif
}


/**
 * Called by the online subsystem, when the game is shutting down (from FinishDestroy)
 */
void FSteamSocketsManager::NotifyDestroy()
{
	if (!bPendingDestroy)
	{
		bPendingDestroy = TRUE;

		// If we don't need to wait for any sockets to finish up, destroy immediately
		if (TrackedSockets.Num() == 0)
		{
			FinishDestroy();
		}
	}
}

/**
 * Finishes destruction, once all sockets are closed
 */
void FSteamSocketsManager::FinishDestroy()
{
	if (GSteamAsyncTaskManager != NULL)
	{
		GSteamAsyncTaskManager->UnregisterInterface(this);
	}

	if (GSteamSocketsManager != NULL)
	{
		delete GSteamSocketsManager;
		GSteamSocketsManager = NULL;
	}
}

/**
 * Handles game-exit shutdown of sockets
 */
void FSteamSocketsManager::PreExit(UNetDriver* NetDriver)
{
	if (SteamGameServer() != NULL)
	{
		UBOOL bSentClose = FALSE;

		// Invalidate all Steam sockets before shutdown
		for (INT SockIdx=TrackedSockets.Num()-1; SockIdx>=0; SockIdx--)
		{
			// Force the close packet (if any) to send instantly, as the socket will be shutting down momentarily
			TrackedSockets(SockIdx)->SetSteamSendMode(k_EP2PSendUnreliableNoDelay);

			// Try to match the socket up with a client connection, and close the connection first before invalidating the socket
			//	(otherwise clients just hang for 30 seconds after exit)
			// NOTE: Will usually match multiple client connections, since the server only has one socket
			for (INT ConnIdx=NetDriver->ClientConnections.Num()-1; ConnIdx>=0; ConnIdx--)
			{
				UTcpipConnection* CurConn = Cast<UTcpipConnection>(NetDriver->ClientConnections(ConnIdx));

				if (CurConn != NULL && CurConn->Socket == TrackedSockets(SockIdx))
				{
					CurConn->Close();
					bSentClose = TRUE;

					// NOTE: Removed a 'break' from here, because this should match multiple client connections
				}
			}

			TrackedSockets(SockIdx)->SetNetworkingInterface(NULL);

			RemoveSocket(TrackedSockets(SockIdx));
		}

		// If any 'Close' packets were sent, there needs to be a short delay before calling SteamGameServer_Shutdown
		//	(happens later in appSteamShutdown) or the buffered packets will not get sent
		if (bSentClose)
		{
			appSleep(0.1f);
		}
	}
}


/**
 * Steam Socket tracking
 */

/**
 * Adds a steam socket for tracking
 *
 * @param InSocket	The socket to add for tracking
 */
void FSteamSocketsManager::AddSocket(FSocketSteamworks* InSocket)
{
	TrackedSockets.AddItem(InSocket);

	// If this is a server socket, and the server has not logged in with its final UID yet,
	//	initialization of the socket must be delayed until the correct UID is available
	if (InSocket->IsServerSocket() && (GSteamGameServer == NULL || !GSteamGameServer->BLoggedOn()))
	{
		UninitializedServerSockets.AddItem(InSocket);
	}
}

/**
 * Removes a steam socket from tracking
 * NOTE: May trigger socket manager destruction, if waiting for the last socket to close
 *
 * @param InSocket	The socket to remove from tracking
 */
void FSteamSocketsManager::RemoveSocket(FSocketSteamworks* InSocket)
{
	TrackedSockets.RemoveItem(InSocket);

	// NOTE: DO NOT USE INSOCKET!!! It's safe to remove from above array, but not much else (because this is called from its destructor)
	// @todo Steam: Ideally, it would be nice to override the socket managers 'DestroySocket' function, and call this from there


	// Remove from uninitialized sockets list, without checking IsServerSocket, due to above (not save to use virtual functions here)
	UninitializedServerSockets.RemoveItem(InSocket);


	// If we were waiting for the last socket to finish up, then finish off destruction
	if (bPendingDestroy && TrackedSockets.Num() == 0)
	{
		FinishDestroy();
	}
}


/**
 * Steam P2P session tracking
 */

/**
 * Signal that a P2P session is still active, and shouldn't be cleaned up
 *
 * @param SteamId	The UID of the connection
 */
void FSteamSocketsManager::TouchP2PSession(QWORD SteamId)
{
	ActiveP2PSessions.Set(SteamId, appSeconds());
}

/**
 * Close an active P2P session
 *
 * @param SteamId	The UID of the connection
 * @param Interface	The networking interface to close the connection on
 */
void FSteamSocketsManager::CloseP2PSession(QWORD SteamId, ISteamNetworking* Interface/*=NULL*/)
{
	CSteamID RemoteId((uint64)SteamId);
	debugf(NAME_DevOnline, TEXT("Closing P2P session with ") I64_FORMAT_TAG, SteamId);

	ISteamNetworking* SteamNetworkingImpl = Interface;

	if (SteamNetworkingImpl == NULL)
	{
		SteamNetworkingImpl = GSteamGameServerNetworking ? GSteamGameServerNetworking : GSteamNetworking;
	}

	if (SteamNetworkingImpl != NULL)
	{
		SteamNetworkingImpl->CloseP2PSessionWithUser(RemoteId);
	}	

	ActiveP2PSessions.RemoveKey(SteamId);
}


/**
 * FSocketSteamworks implementation
 */

/**
 * Queries the socket to determine if there is pending data on the queue
 *
 * @param PendingDataSize out parameter indicating how much data is on the pipe for a single recv call
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketSteamworks::HasPendingData(UINT& PendingDataSize)
{
	if (SteamNetworkingImpl == NULL)
	{
		PendingDataSize = 0;
		return FALSE;
	}


	uint32 MessageSize = 0;

	if (!SteamNetworkingImpl->IsP2PPacketAvailable(&MessageSize))
	{
		PendingDataSize = 0;
		return FALSE;
	}


	PendingDataSize = (UINT)MessageSize;

	return TRUE;
}

/**
 * Sends a buffer to a network byte ordered address
 *
 * @param Data the buffer to send
 * @param Count the size of the data to send
 * @param BytesSent out param indicating how much was sent
 * @param Destination the network byte ordered address to send to
 */
UBOOL FSocketSteamworks::SendTo(const BYTE* Data, INT Count, INT& BytesSent, const FInternetIpAddr& Destination)
{
	if (SteamNetworkingImpl == NULL)
	{
		return FALSE;
	}


	CSteamID RemoteId;

	if (IpAddrToSteamId(Destination, RemoteId))
	{
		// Don't send data to our own UID
		if (RemoteId != LocalSteamId)
		{
			// Don't touch on send; Unreal keeps sending packets even after disconnect, so receive is the only reliable way of detecting
			//	timeout
			//TouchP2PSession(RemoteId.ConvertToUint64());

#if STEAM_SOCKETS_TRAFFIC_DEBUG
			debugf(NAME_DevOnline, TEXT("Sending %d bytes on P2P socket to ") I64_FORMAT_TAG TEXT("!"), Count,
				RemoteId.ConvertToUint64());
#endif

			return SteamNetworkingImpl->SendP2PPacket(RemoteId, Data, Count, SteamSendMode);
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("Blocked FSocketSteamworks::SendTo call, directed at current local UID"));
		}
	}


	return FALSE;
}

/**
 * Reads a chunk of data from the socket. Gathers the source address too
 *
 * @param Data the buffer to read into
 * @param BufferSize the max size of the buffer
 * @param BytesRead out param indicating how many bytes were read from the socket
 * @param Source out param receiving the address of the sender of the data
 */
UBOOL FSocketSteamworks::RecvFrom(BYTE* Data, INT BufferSize, INT& BytesRead, FInternetIpAddr& Source)
{
	if (SteamNetworkingImpl == NULL)
	{
		BytesRead = 0;
		return FALSE;
	}

	if (PortUnreachables.Num() > 0)
	{			
		SteamIdToIpAddr(CSteamID(PortUnreachables(0)), Source);
		GLastSocketError = SE_UDP_ERR_PORT_UNREACH;
		BytesRead = 0;
		PortUnreachables.Remove(0);

		return FALSE;
	}

	CSteamID RemoteId;
	uint32 MessageSize = 0;

	if (!SteamNetworkingImpl->ReadP2PPacket(Data, BufferSize, &MessageSize, &RemoteId))
	{
		GLastSocketError = SE_EWOULDBLOCK;
		BytesRead = 0;

		return FALSE;
	}


	if (GSteamSocketsManager != NULL)
	{
		GSteamSocketsManager->TouchP2PSession((QWORD)RemoteId.ConvertToUint64());
	}

	GLastSocketError = SE_NO_ERROR;
	BytesRead = (INT) MessageSize;

	SteamIdToIpAddr(RemoteId, Source);

#if STEAM_SOCKETS_TRAFFIC_DEBUG
	debugf(NAME_DevOnline, TEXT("Read %d bytes on P2P socket from %s!"), BytesRead, *RenderCSteamID(RemoteId));
#endif

	return TRUE;
}


#endif	// WITH_STEAMWORKS_SOCKETS


