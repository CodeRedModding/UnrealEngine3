/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

// Includes
#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

/**
 * Globals
 */
TMap<DWORD, FAuthTicketData>	AuthTicketMap;
DWORD				NextAuthTicketUID = 1;

#endif // WITH_UE3_NETWORKING


/**
 * Extern functions
 */

/**
 * Relays NMT_ClientAuthRequest control messages to the OnlineSubsystem auth interface
 */
void appHandleClientAuthRequest(UNetConnection* Connection, QWORD ServerUID, DWORD PublicServerIP, INT PublicServerPort, UBOOL bSecure)
{
#if WITH_UE3_NETWORKING
	// Need to store the servers UID in the net connection, so the auth interface can properly match up server connections to auth data
	Connection->PlayerId.Uid = ServerUID;

	UOnlineSubsystemCommonImpl* OnlineSub = Cast<UOnlineSubsystemCommonImpl>(UGameEngine::GetOnlineSubsystem());

	if (OnlineSub != NULL && OnlineSub->AuthInterfaceImpl != NULL)
	{
		OnlineSub->AuthInterfaceImpl->OnClientAuthRequest(Connection, ServerUID, PublicServerIP, PublicServerPort, bSecure);
	}
#endif // WITH_UE3_NETWORKING
}

/**
 * Relays NMT_ServerAuthRequest control messages to the OnlineSubsystem auth interface
 */
void appHandleServerAuthRequest(UNetConnection* Connection)
{
#if WITH_UE3_NETWORKING
	UOnlineSubsystemCommonImpl* OnlineSub = Cast<UOnlineSubsystemCommonImpl>(UGameEngine::GetOnlineSubsystem());

	if (OnlineSub != NULL && OnlineSub->AuthInterfaceImpl != NULL)
	{
		OnlineSub->AuthInterfaceImpl->OnServerAuthRequest(Connection);
	}
#endif // WITH_UE3_NETWORKING
}

/**
 * Relays NMT_AuthRequestPeer control messages to the OnlineSubsystem auth interface
 */
void appHandleAuthRequestPeer(UNetConnection* Connection, QWORD RemoteUID)
{
#if WITH_UE3_NETWORKING
	UOnlineSubsystemCommonImpl* OnlineSub = Cast<UOnlineSubsystemCommonImpl>(UGameEngine::GetOnlineSubsystem());

	if (OnlineSub != NULL && OnlineSub->AuthInterfaceImpl != NULL)
	{
		OnlineSub->AuthInterfaceImpl->OnAuthRequestPeer(Connection, RemoteUID);
	}
#endif // WITH_UE3_NETWORKING
}

/**
 * Relays NMT_AuthBlob control messages to the OnlineSubsystem auth interface
 */
void appAuthBlob(UNetConnection* Connection, const FString& BlobChunk, BYTE Current, BYTE Num)
{
#if WITH_UE3_NETWORKING
	UOnlineSubsystemCommonImpl* OnlineSub = Cast<UOnlineSubsystemCommonImpl>(UGameEngine::GetOnlineSubsystem());

	if (OnlineSub != NULL && OnlineSub->AuthInterfaceImpl != NULL)
	{
		OnlineSub->AuthInterfaceImpl->OnAuthBlob(Connection, BlobChunk, Current, Num);
	}
#endif // WITH_UE3_NETWORKING
}

/**
 * Relays NMT_AuthBlobPeer control messages to the OnlineSubsystem auth interface
 */
void appAuthBlobPeer(UNetConnection* Connection, QWORD RemoteUID, const FString& BlobChunk, BYTE Current, BYTE Num)
{
#if WITH_UE3_NETWORKING
	UOnlineSubsystemCommonImpl* OnlineSub = Cast<UOnlineSubsystemCommonImpl>(UGameEngine::GetOnlineSubsystem());

	if (OnlineSub != NULL && OnlineSub->AuthInterfaceImpl != NULL)
	{
		OnlineSub->AuthInterfaceImpl->OnAuthBlobPeer(Connection, RemoteUID, BlobChunk, Current, Num);
	}
#endif // WITH_UE3_NETWORKING
}

/**
 * Relays NMT_ClientAuthEndSessionRequest control messages to the OnlineSubsystem auth interface
 */
void appClientAuthEndSessionRequest(UNetConnection* Connection)
{
#if WITH_UE3_NETWORKING
	UOnlineSubsystemCommonImpl* OnlineSub = Cast<UOnlineSubsystemCommonImpl>(UGameEngine::GetOnlineSubsystem());

	if (OnlineSub != NULL && OnlineSub->AuthInterfaceImpl != NULL)
	{
		OnlineSub->AuthInterfaceImpl->OnClientAuthEndSessionRequest(Connection);
	}
#endif // WITH_UE3_NETWORKING
}

/**
 * Relays NMT_AuthKillPeer control messages to the OnlineSubsystem auth interface
 */
void appAuthKillPeer(UNetConnection* Connection, QWORD RemoteUID)
{
#if WITH_UE3_NETWORKING
	UOnlineSubsystemCommonImpl* OnlineSub = Cast<UOnlineSubsystemCommonImpl>(UGameEngine::GetOnlineSubsystem());

	if (OnlineSub != NULL && OnlineSub->AuthInterfaceImpl != NULL)
	{
		OnlineSub->AuthInterfaceImpl->OnAuthKillPeer(Connection, RemoteUID);
	}
#endif // WITH_UE3_NETWORKING
}

/**
 * Relays NMT_AuthRetry control messages to the OnlineSubsystem auth interface
 */
void appAuthRetry(UNetConnection* Connection)
{
#if WITH_UE3_NETWORKING
	UOnlineSubsystemCommonImpl* OnlineSub = Cast<UOnlineSubsystemCommonImpl>(UGameEngine::GetOnlineSubsystem());

	if (OnlineSub != NULL && OnlineSub->AuthInterfaceImpl != NULL)
	{
		OnlineSub->AuthInterfaceImpl->OnAuthRetry(Connection);
	}
#endif // WITH_UE3_NETWORKING
}

/**
 * Relays UNetConnection::Close events to the OnlineSubsystem auth interface
 */
void appAuthConnectionClose(UNetConnection* Connection)
{
#if WITH_UE3_NETWORKING
	UOnlineSubsystemCommonImpl* OnlineSub = Cast<UOnlineSubsystemCommonImpl>(UGameEngine::GetOnlineSubsystem());

	if (OnlineSub != NULL && OnlineSub->AuthInterfaceImpl != NULL)
	{
		OnlineSub->AuthInterfaceImpl->OnAuthConnectionClose(Connection);
	}
#endif // WITH_UE3_NETWORKING
}


#if WITH_UE3_NETWORKING

/**
 * OnlineAuthInterfaceImpl implementation
 */

/**
 * Auth/Connection events
 */

/**
 * Control message sent from server to client, requesting an auth session from the client
 *
 * @param Connection		The NetConnection the message came from
 * @param ServerUID		The UID of the game server
 * @param PublicServerIP	The public (external) IP of the game server
 * @param PublicServerPort	The port of the game server
 * @param bSecure		whether or not the server has anticheat enabled (relevant to OnlineSubsystemSteamworks and VAC)
 */
void UOnlineAuthInterfaceImpl::OnClientAuthRequest(UNetConnection* Connection, QWORD ServerUID, DWORD PublicServerIP, INT PublicServerPort,
								UBOOL bSecure)
{
	// Setup server auth session tracking here, regardless of whether or not the code is going to use it at all;
	//	this is the primary place where we get the servers public IP etc., so it must be setup here
	FAuthSession* ServerSession = GetServerAuthSession(Connection);

	// If there wasn't an existing session, setup a new one
	if (ServerSession == NULL)
	{
		// SparseArray version of AddZeroed
		FSparseArrayAllocationInfo ElementInfo = ServerAuthSessions.Add();
		appMemzero(ElementInfo.Pointer, sizeof(FAuthSession));

		ServerSession = &ServerAuthSessions(ElementInfo.Index);

		ServerSession->EndPointIP = PublicServerIP;
		ServerSession->EndPointPort = PublicServerPort;
		ServerSession->EndPointUID.Uid = ServerUID;
	}
	// If there >was< an existing session, cleanup any auth ticket data (otherwise receiving of future auth tickets is blocked)
	else if (ServerSession->AuthTicketUID != 0)
	{
		AuthTicketMap.Remove(ServerSession->AuthTicketUID);
		ServerSession->AuthTicketUID = 0;
	}

	ServerSession->AuthStatus = AUS_NotStarted;


	// Trigger UScript delegates
	OnlineAuthInterfaceImpl_eventOnClientAuthRequest_Parms Parms(EC_EventParm);
	Parms.ServerUID.Uid = ServerUID;
	Parms.ServerIP = PublicServerIP;
	Parms.ServerPort = PublicServerPort;
	Parms.bSecure = bSecure;

	TriggerOnlineDelegates(this, ClientAuthRequestDelegates, &Parms);
}

/**
 * Control message sent from client to server, requesting an auth session from the server
 *
 * @param Connection		The NetConnection the message came from
 */
void UOnlineAuthInterfaceImpl::OnServerAuthRequest(UNetConnection* Connection)
{
	FAuthSession* ClientSession = GetClientAuthSession(Connection);

	// Trigger UScript delegates
	if (ClientSession != NULL && ClientSession->AuthStatus == AUS_Authenticated)
	{
		OnlineAuthInterfaceImpl_eventOnServerAuthRequest_Parms Parms(EC_EventParm);
		Parms.ClientConnection = Connection;
		Parms.ClientUID = ClientSession->EndPointUID;
		Parms.ClientIP = ClientSession->EndPointIP;
		Parms.ClientPort = ClientSession->EndPointPort;

		TriggerOnlineDelegates(this, ServerAuthRequestDelegates, &Parms);
	}
	else
	{
		// Don't want to allow clients to trigger server auth with a potentially faked UID
		debugf(NAME_DevOnline, TEXT("OnServerAuthRequest: Need a fully authenticated client auth session before doing server auth"));
	}
}

/**
 * Control message sent both from client to server, and server to client, containing auth ticket data for auth verification
 *
 * @param Connection		The NetConnection the message came from
 * @param BlobChunk		The current chunk/blob of auth ticket data
 * @param Current		The current sequence of the blob/chunk
 * @param Num			The total number of blobs/chunks being received
 */
void UOnlineAuthInterfaceImpl::OnAuthBlob(UNetConnection* Connection, const FString& BlobChunk, BYTE Current, BYTE Num)
{
	UBOOL bServerBlob = (Connection == Connection->Driver->ServerConnection);

	FAuthSession* CurSession = NULL;

	if (bServerBlob)
	{
		CurSession = GetServerAuthSession(Connection);
	}
	else
	{
		CurSession = GetClientAuthSession(Connection);
	}

	FAuthTicketData* CurTicket = NULL;

	if (CurSession != NULL)
	{
		CurTicket = GetAuthTicket(CurSession, TRUE);
	}


	if (CurTicket != NULL && !CurTicket->bComplete)
	{
		// Preallocate space for all the strings
		if (Num > 0 && Num <= 8 && CurTicket->IncomingBlobs.Num() == 0)
		{
			CurTicket->IncomingBlobs.AddZeroed(Num);
		}

		// They all need to match
		if (Num == 0 || CurTicket->IncomingBlobs.Num() != Num || Current >= Num ||
			CurTicket->IncomingBlobs(Current).Len() != 0)
		{
			debugf(NAME_DevOnline, TEXT("Received bad auth blob data (Blob num: %i-%i)"), Current, Num);
			return;
		}

		// Cache the string from the message
		CurTicket->IncomingBlobs(Current) = BlobChunk;

		// Handle checking server auth
		if (bServerBlob)
		{
			ProcessServerAuth(Connection, CurSession, CurTicket);
		}
		else
		{
			ProcessClientAuth(Connection, CurSession, CurTicket);
		}
	}
	else if (CurTicket == NULL)
	{
		debugf(NAME_DevOnline, TEXT("OnAuthBlob: Failed to find auth session and store auth ticket data"));
	}
	else if (CurTicket->bComplete)
	{
		debugf(NAME_DevOnline, TEXT("OnAuthBlob: Received auth blob data when auth ticket receive has completed; ticket UID: %d"),
			CurSession->AuthTicketUID);
	}
}

/**
 * Called on both client and server, when a net connection is closing
 *
 * @param Connection		The NetConnection that is closing
 */
void UOnlineAuthInterfaceImpl::OnAuthConnectionClose(UNetConnection* Connection)
{
	UNetConnection* ServerConn = Connection->Driver->ServerConnection;

	// Trigger UScript delegates
	if (ServerConn != NULL)
	{
		OnlineAuthInterfaceImpl_eventOnServerConnectionClose_Parms Parms(EC_EventParm);
		Parms.ServerConnection = ServerConn;

		TriggerOnlineDelegates(this, ServerConnectionCloseDelegates, &Parms);
	}
	else
	{
		OnlineAuthInterfaceImpl_eventOnClientConnectionClose_Parms Parms(EC_EventParm);
		Parms.ClientConnection = Connection;

		TriggerOnlineDelegates(this, ClientConnectionCloseDelegates, &Parms);
	}
}


/**
 * Control channel send/receive
 */

/**
 * Sends the specified auth ticket from the client to the server
 *
 * @param AuthTicketUID		The UID of the auth ticket, as retrieved by CreateClientAuthSession
 * @return			whether or not the auth ticket was sent successfully
 */
UBOOL UOnlineAuthInterfaceImpl::SendClientAuthResponse(INT AuthTicketUID)
{
	UBOOL bSuccess = FALSE;
	UNetDriver* NetDriver = (GWorld != NULL ? GWorld->GetNetDriver() : NULL);

	// If the main level does not have a net driver, check for pending level net driver
	if (NetDriver == NULL && Cast<UGameEngine>(GEngine) != NULL && ((UGameEngine*)GEngine)->GPendingLevel != NULL)
	{
		NetDriver = ((UGameEngine*)GEngine)->GPendingLevel->NetDriver;
	}

	if (NetDriver != NULL && NetDriver->ServerConnection != NULL)
	{
		bSuccess = SendAuthTicket(NetDriver->ServerConnection, AuthTicketUID);
	}
	else if (NetDriver == NULL)
	{
		debugf(NAME_DevOnline, TEXT("SendClientAuthResponse: NetDriver is NULL"));
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("SendClientAuthResponse: NetDriver->ServerConnection is NULL"));
	}

	return bSuccess;
}

/**
 * Sends the specified auth ticket from the server to the client
 *
 * @param ClientConnection	The NetConnection of the client to send the auth ticket to
 * @param AuthTicketUID		The UID of the auth ticket, as retrieved by CreateServerAuthSession
 * @return			whether or not the auth ticket was sent successfully
 */
UBOOL UOnlineAuthInterfaceImpl::SendServerAuthResponse(UPlayer* ClientConnection, INT AuthTicketUID)
{
	UBOOL bSuccess = FALSE;

	// Verify the ClientConnection is valid (needs more than a simple cast)
	UNetDriver* NetDriver = (GWorld != NULL ? GWorld->GetNetDriver() : NULL);
	UNetConnection* ClientConn = NULL;

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


	if (ClientConn != NULL)
	{
		bSuccess = SendAuthTicket(ClientConn, AuthTicketUID);
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("SendServerAuthResponse: ClientConnection is not in NetDriver->ClientConnections"));
	}


	return bSuccess;
}

/**
 * Sends an auth kill request to the specified client
 *
 * @param ClientConnection	The NetConnection of the client to send the request to
 * @return			whether or not the request was sent successfully
 */
UBOOL UOnlineAuthInterfaceImpl::SendClientAuthEndSessionRequest(UPlayer* ClientConnection)
{
	UBOOL bSuccess = FALSE;

	// Verify the ClientConnection is valid (needs more than a simple cast)
	UNetDriver* NetDriver = (GWorld != NULL ? GWorld->GetNetDriver() : NULL);
	UNetConnection* ClientConn = NULL;

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

	if (ClientConn != NULL)
	{
		FNetControlMessage<NMT_ClientAuthEndSessionRequest>::Send(ClientConn);
		ClientConn->FlushNet();

		bSuccess = TRUE;
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("SendClientAuthEndSessionRequest: ClientConnection is not in NetDriver->ClientConnections"));
	}

	return bSuccess;
}

/**
 * Sends a server auth retry request to the server
 *
 * @return			whether or not the request was sent successfully
 */
UBOOL UOnlineAuthInterfaceImpl::SendServerAuthRetryRequest()
{
	UBOOL bSuccess = FALSE;
	UNetDriver* NetDriver = (GWorld != NULL ? GWorld->GetNetDriver() : NULL);

	// If the main level does not have a net driver, check for pending level net driver
	if (NetDriver == NULL && Cast<UGameEngine>(GEngine) != NULL && ((UGameEngine*)GEngine)->GPendingLevel != NULL)
	{
		NetDriver = ((UGameEngine*)GEngine)->GPendingLevel->NetDriver;
	}

	if (NetDriver != NULL && NetDriver->ServerConnection != NULL)
	{
		FAuthSession* ServerSession = GetServerAuthSession(NetDriver->ServerConnection);

		// If there wasn't an existing session, setup a new one, based on the existing client auth session data
		if (ServerSession == NULL)
		{
			FLocalAuthSession* LocalClientSession = GetLocalClientAuthSession(NetDriver->ServerConnection);

			if (LocalClientSession != NULL)
			{
				// SparseArray version of AddZeroed
				FSparseArrayAllocationInfo ElementInfo = ServerAuthSessions.Add();
				appMemzero(ElementInfo.Pointer, sizeof(FAuthSession));

				ServerSession = &ServerAuthSessions(ElementInfo.Index);

				ServerSession->EndPointIP = LocalClientSession->EndPointIP;
				ServerSession->EndPointPort = LocalClientSession->EndPointPort;
				ServerSession->EndPointUID = LocalClientSession->EndPointUID;
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("SendServerAuthRetryRequest: Need a client auth session to create a server auth session"));
			}
		}
		// If there >was< an existing session, cleanup any auth ticket data (otherwise receiving of future auth tickets is blocked)
		else if (ServerSession->AuthTicketUID != 0)
		{
			AuthTicketMap.Remove(ServerSession->AuthTicketUID);
			ServerSession->AuthTicketUID = 0;
		}

		if (ServerSession != NULL)
		{
			ServerSession->AuthStatus = AUS_NotStarted;

			FNetControlMessage<NMT_AuthRetry>::Send(NetDriver->ServerConnection);
			NetDriver->ServerConnection->FlushNet();

			bSuccess = TRUE;
		}
	}
	else if (NetDriver == NULL)
	{
		debugf(NAME_DevOnline, TEXT("SendServerAuthRetryRequest: NetDriver is NULL"));
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("SendServerAuthRetryRequest: NetDriver->ServerConnection is NULL"));
	}

	return bSuccess;
}

/**
 * Sends an auth ticket over the specified net connection
 *
 * @param InConnection		The current connection
 * @param AuthTicketUID		The UID of the auth ticket to send
 */
UBOOL UOnlineAuthInterfaceImpl::SendAuthTicket(UNetConnection* InConnection, INT AuthTicketUID)
{
	UBOOL bSuccess = FALSE;

	FAuthTicketData* CurAuthTicket = AuthTicketMap.Find(AuthTicketUID);

	if (CurAuthTicket != NULL && CurAuthTicket->bComplete)
	{
		// Break up the ticket into chunks/blobs small enough to fit in a packet
		const INT MaxSubBlobSize = (InConnection->MaxPacket - 32) / 4;
		BYTE NumSubBlobs = (BYTE)((CurAuthTicket->FinalAuthTicket.Num() + (MaxSubBlobSize - 1)) / MaxSubBlobSize);
		INT Offset = 0;

		for (BYTE SubBlob=0; SubBlob<NumSubBlobs; SubBlob++)
		{
			// Calc size of the next one to go
			const INT SubBlobSize = Min(MaxSubBlobSize, CurAuthTicket->FinalAuthTicket.Num() - Offset);

			// Send it
			FString AuthBlobString = appBlobToString(((BYTE*)CurAuthTicket->FinalAuthTicket.GetData()) + Offset, SubBlobSize);
			FNetControlMessage<NMT_AuthBlob>::Send(InConnection, AuthBlobString, SubBlob, NumSubBlobs);
			InConnection->FlushNet();

			// Move up in the world
			Offset += SubBlobSize;
		}

		bSuccess = TRUE;
	}
	else if (CurAuthTicket == NULL)
	{
		debugf(NAME_DevOnline, TEXT("SendAuthTicket: Invalid AuthTicketUID '%i'"), AuthTicketUID);
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("SendAuthTicket: Auth ticket '%i' not complete"), AuthTicketUID);
	}

	return bSuccess;
}

/**
 * With client auth, checks if all blobs/pieces of the clients auth ticket have been received, and if so, auths the client
 * NOTE: Serverside only
 *
 * @param Connection		The current connection
 * @param ClientSession		The client auth session data
 * @param TicketData		The current auth ticket data
 */
void UOnlineAuthInterfaceImpl::ProcessClientAuth(UNetConnection* Connection, FAuthSession* ClientSession, FAuthTicketData* TicketData)
{
	TArray<BYTE>* ClientAuthTicket = NULL;

	if (ProcessAuthTicket(TicketData, &ClientAuthTicket) && ClientAuthTicket != NULL && ClientAuthTicket->Num() > 0)
	{
		// Trigger UScript delegates
		OnlineAuthInterfaceImpl_eventOnClientAuthResponse_Parms Parms(EC_EventParm);
		Parms.ClientUID = ClientSession->EndPointUID;
		Parms.ClientIP = ClientSession->EndPointIP;
		Parms.AuthTicketUID = ClientSession->AuthTicketUID;

		TriggerOnlineDelegates(this, ClientAuthResponseDelegates, &Parms);
	}
	// If ProcessAuthTicket failed, and wiped IncomingBlobs, there was a fatal error
	// @todo JohnB: Redo this eventually; a fudgy way to pass on failure
	else if (TicketData->IncomingBlobs.Num() == 0)
	{
		debugf(NAME_DevOnline, TEXT("ProcessClientAuth: WARNING!!! Error processing auth blob data"));
	}
}

/**
 * With server auth, checks if all blobs/pieces of the servers auth ticket have been received, and if so, auths the server
 * NOTE: Clientside only
 *
 * @param Connection		The current connection
 * @param ServerSession		The server auth session data
 * @param TicketData		The current auth ticket data
 */
void UOnlineAuthInterfaceImpl::ProcessServerAuth(UNetConnection* Connection, FAuthSession* ServerSession, FAuthTicketData* TicketData)
{
	UBOOL bAuthFailure = FALSE;
	TArray<BYTE>* ServerAuthTicket = NULL;

	if (ProcessAuthTicket(TicketData, &ServerAuthTicket) && ServerAuthTicket != NULL && ServerAuthTicket->Num() > 0)
	{
		// If server auth session tracking is still in the 'AUS_NotStarted' status, mark it as pending now
		if (ServerSession->AuthStatus == AUS_NotStarted)
		{
			debugf(NAME_DevOnline, TEXT("Marking server auth session status as pending"));

			ServerSession->AuthStatus = AUS_Pending;
		}

		// Triger UScript delegates
		OnlineAuthInterfaceImpl_eventOnServerAuthResponse_Parms Parms(EC_EventParm);
		Parms.ServerUID = ServerSession->EndPointUID;
		Parms.ServerIP = ServerSession->EndPointIP;
		Parms.AuthTicketUID = ServerSession->AuthTicketUID;

		TriggerOnlineDelegates(this, ServerAuthResponseDelegates, &Parms);
	}
	// If ProcessAuthTicket failed, and wiped IncomingBlobs, there was a fatal error
	else if (TicketData->IncomingBlobs.Num() == 0)
	{
		debugf(NAME_DevOnline, TEXT("ProcessServerAuth: WARNING!!! Error processing auth blob data"));
	}
}

/**
 * Processes a net connections accumulated auth blob data, and returns the final auth ticket
 * NOTE: Auth data is cleared after calling, so can only be called once
 *
 * @param InTicketData		The current auth ticket data
 * @param OutAuthTicket		The final output auth ticket
 * @return			Returns TRUE if there was no error (does not mean an auth ticket was returned), and FALSE if not
 */
UBOOL UOnlineAuthInterfaceImpl::ProcessAuthTicket(FAuthTicketData* InTicketData, TArray<BYTE>** OutAuthTicket)
{
	*OutAuthTicket = NULL;

	const INT NumAuthBlobs = InTicketData->IncomingBlobs.Num();
	FString FullAuthString;

	// Make sure we got all of the auth blobs
	for (INT SubBlob=0; SubBlob<NumAuthBlobs; SubBlob++)
	{
		// No string should be empty
		if (InTicketData->IncomingBlobs(SubBlob).Len() == 0)
		{
			return FALSE;
		}

		// Put all the strings together
		FullAuthString += InTicketData->IncomingBlobs(SubBlob);
	}

	// Toss temp strings
	InTicketData->IncomingBlobs.Empty();
	InTicketData->bComplete = TRUE;

	InTicketData->FinalAuthTicket.Init(FullAuthString.Len() / 3);

	// The final ticket must be exactly divisible by 3 (due to how appBlobToString etc. works)
	if ((FullAuthString.Len() % 3) != 0)
	{
		debugf(NAME_DevOnline, TEXT("Invalid auth ticket"));
		return FALSE;
	}

	if (InTicketData->FinalAuthTicket.Num() > 0)
	{
		appStringToBlob(FullAuthString, (BYTE*)InTicketData->FinalAuthTicket.GetData(), InTicketData->FinalAuthTicket.Num());
		*OutAuthTicket = &InTicketData->FinalAuthTicket;
	}

	return TRUE;
}

/**
 * Client auth functions
 */

/**
 * Ends the clientside half of a client auth session
 * NOTE: This call must be matched on the server, with EndRemoteClientAuthSession
 *
 * @param ServerUID		The UID of the server
 * @param ServerIP		The external (public) IP address of the server
 * @param ServerPort		The port of the server
 */
void UOnlineAuthInterfaceImpl::EndLocalClientAuthSession(FUniqueNetId ServerUID, INT ServerIP, INT ServerPort)
{
	INT LocalClientSessionIdx = INDEX_NONE;

	for (INT i=0; i<LocalClientAuthSessions.GetMaxIndex(); i++)
	{
		if (LocalClientAuthSessions.IsAllocated(i) && LocalClientAuthSessions(i).EndPointUID == ServerUID &&
			LocalClientAuthSessions(i).EndPointIP == ServerIP && LocalClientAuthSessions(i).EndPointPort == ServerPort)
		{
			LocalClientSessionIdx = i;
			break;
		}
	}

	if (LocalClientSessionIdx != INDEX_NONE)
	{
		// NOTE: Only the internal version of this function accepts an FLocalSessionInfo as input, because passing around
		//	copied FLocalSessionInfo's in script, can lead to out of date struct data (except for the UID, IP and Port values)
		InternalEndLocalClientAuthSession(LocalClientAuthSessions(LocalClientSessionIdx));

		// Remove from tracking
		LocalClientAuthSessions.Remove(LocalClientSessionIdx, 1);
	}
	else
	{
		// @todo JohnB: unix compatible int64 logging
		debugf(NAME_DevOnline, TEXT("EndLocalClientAuthSession: WARNING! Could not find entry in LocalClientAuthSessions! (ServerUID: %I64u)"),
			ServerUID.Uid);
	}
}

/**
 * Ends the serverside half of a client auth session
 * NOTE: This call must be matched on the client, with EndLocalClientAuthSession
 *
 * @param ClientUID		The UID of the client
 * @param ClientIP		The IP address of the client
 */
void UOnlineAuthInterfaceImpl::EndRemoteClientAuthSession(FUniqueNetId ClientUID, INT ClientIP)
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

	if (ClientSessionIdx != INDEX_NONE)
	{
		FAuthSession& ClientSession = ClientAuthSessions(ClientSessionIdx);

		if (ClientSession.AuthStatus == AUS_Pending || ClientSession.AuthStatus == AUS_Authenticated)
		{
			// NOTE: Only the internal version of this function accepts an FSessionInfo as input, because passing around copied
			//	FSessionInfo's in script, can lead to out of date struct data (Except for the UID, IP and Port values)
			InternalEndRemoteClientAuthSession(ClientSession);
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("EndRemoteClientAuthSession: Auth session failed or not started, removing from list (AuthStatus: %i)"),
				ClientSession.AuthStatus);
		}

		// Remove from tracking
		if (ClientSession.AuthTicketUID != 0)
		{
			AuthTicketMap.Remove(ClientSession.AuthTicketUID);
		}

		ClientAuthSessions.Remove(ClientSessionIdx, 1);
	}
	else
	{
		// @todo JohnB: unix compatible int64 logging
		debugf(NAME_DevOnline, TEXT("EndRemoteClientAuthSession: WARNING! Could not find entry in ClientAuthSessions! (ClientUID: %I64u)"),
			ClientUID.Uid);
	}
}


/**
 * Ends the clientside halves of all client auth sessions
 * NOTE: This is the same as iterating AllLocalClientAuthSessions and ending each session with EndLocalClientAuthSession
 */
void UOnlineAuthInterfaceImpl::EndAllLocalClientAuthSessions()
{
	if (LocalClientAuthSessions.Num() > 0)
	{
		for (TSparseArray<FLocalAuthSession>::TIterator It(LocalClientAuthSessions); It; ++It)
		{
			InternalEndLocalClientAuthSession(*It);
		}
	}
}

/**
 * Ends the serverside halves of all client auth sessions
 * NOTE: This is the same as iterating AllClientAuthSessions and ending each session with EndRemoteClientAuthSession
 */
void UOnlineAuthInterfaceImpl::EndAllRemoteClientAuthSessions()
{
	if (ClientAuthSessions.Num() > 0)
	{
		for (TSparseArray<FAuthSession>::TIterator It(ClientAuthSessions); It; ++It)
		{
			if (It->AuthStatus == AUS_Pending || It->AuthStatus == AUS_Authenticated)
			{
				InternalEndRemoteClientAuthSession(*It);
			}
		}
	}
}


/**
 * Server auth functions
 */

/**
 * Ends the serverside half of a server auth session
 * NOTE: This call must be matched on the other end, with EndRemoteServerAuthSession
 *
 * @param ClientUID		The UID of the client
 * @param ClientIP		The IP address of the client
 */
void UOnlineAuthInterfaceImpl::EndLocalServerAuthSession(FUniqueNetId ClientUID, INT ClientIP)
{
	INT LocalServerSessionIdx = INDEX_NONE;

	for (INT i=0; i<LocalServerAuthSessions.GetMaxIndex(); i++)
	{
		if (LocalServerAuthSessions.IsAllocated(i) && LocalServerAuthSessions(i).EndPointUID == ClientUID &&
			LocalServerAuthSessions(i).EndPointIP == ClientIP)
		{
			LocalServerSessionIdx = i;
			break;
		}
	}

	if (LocalServerSessionIdx != INDEX_NONE)
	{
		// NOTE: Only the internal version of this function accepts an FLocalSessionInfo as input, because passing around
		//	copied FLocalSessionInfo's in script, can lead to out of date struct data (except for the UID, IP and Port values)
		InternalEndLocalServerAuthSession(LocalServerAuthSessions(LocalServerSessionIdx));

		// Remove server auth from tracking
		if (LocalServerSessionIdx != INDEX_NONE)
		{
			LocalServerAuthSessions.Remove(LocalServerSessionIdx, 1);
		}
	}
	else
	{
		// @todo JohnB: unix compatible int64 logging
		debugf(NAME_DevOnline, TEXT("EndLocalServerAuthSession: WARNING! Could not find entry in LocalServerAuthSessions! (ClientUID: %I64u)"),
			ClientUID.Uid);
	}
}

/**
 * Ends the clientside half of a server auth session
 * NOTE: This call must be matched on the other end, with EndLocalServerAuthSession
 *
 * @param ServerUID		The UID of the server
 * @param ServerIP		The external/public IP address of the server
 */
void UOnlineAuthInterfaceImpl::EndRemoteServerAuthSession(FUniqueNetId ServerUID, INT ServerIP)
{
	INT ServerSessionIdx = INDEX_NONE;

	for (INT i=0; i<ServerAuthSessions.GetMaxIndex(); i++)
	{
		if (ServerAuthSessions.IsAllocated(i) && ServerAuthSessions(i).EndPointUID == ServerUID &&
			ServerAuthSessions(i).EndPointIP == ServerIP)
		{
			ServerSessionIdx = i;
			break;
		}
	}

	if (ServerSessionIdx != INDEX_NONE)
	{
		FAuthSession& ServerSession = ServerAuthSessions(ServerSessionIdx);

		if (ServerSession.AuthStatus == AUS_Pending || ServerSession.AuthStatus == AUS_Authenticated)
		{
			// NOTE: Only the internal version of this function accepts an FSessionInfo as input, because passing around copied
			//	FSessionInfo's in script, can lead to out of date struct data (Except for the UID, IP and Port values)
			InternalEndRemoteServerAuthSession(ServerSession);
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("EndRemoteServerAuthSession: Auth session failed or not started, removing from list (AuthStatus: %i)"),
				ServerSession.AuthStatus);
		}

		// Remove from tracking
		if (ServerSession.AuthTicketUID != 0)
		{
			AuthTicketMap.Remove(ServerSession.AuthTicketUID);
		}

		ServerAuthSessions.Remove(ServerSessionIdx, 1);
	}
	else
	{
		// @todo JohnB: unix compatible int64 logging
		debugf(NAME_DevOnline, TEXT("EndRemoteServerAuthSession: WARNING! Could not find entry in ServerAuthSessions! (ServerUID: %I64u)"),
			ServerUID.Uid);
	}
}


/**
 * Ends the serverside halves of all server auth sessions
 * NOTE: This is the same as iterating AllLocalServerAuthSessions and ending each session with EndLocalServerAuthSession
 */
void UOnlineAuthInterfaceImpl::EndAllLocalServerAuthSessions()
{
	if (LocalServerAuthSessions.Num() > 0)
	{
		for (TSparseArray<FLocalAuthSession>::TIterator It(LocalServerAuthSessions); It; ++It)
		{
			InternalEndLocalServerAuthSession(*It);
		}
	}
}

/**
 * Ends the clientside halves of all server auth sessions
 * NOTE: This is the same as iterating AllServerAuthSessions and ending each session with EndRemoteServerAuthSession
 */
void UOnlineAuthInterfaceImpl::EndAllRemoteServerAuthSessions()
{
	if (ServerAuthSessions.Num() > 0)
	{
		for (TSparseArray<FAuthSession>::TIterator It(ServerAuthSessions); It; ++It)
		{
			if (It->AuthStatus == AUS_Pending || It->AuthStatus == AUS_Authenticated)
			{
				InternalEndRemoteServerAuthSession(*It);
			}
		}
	}
}


/**
 * Session access functions
 */


/**
 * On a server, iterates all auth sessions for clients connected to the server
 * NOTE: This iterator is remove-safe; ending a client auth session from within this iterator will not mess up the order of iteration
 *
 * @param OutSessionInfo	Outputs the currently iterated auth session
 */
void UOnlineAuthInterfaceImpl::execAllClientAuthSessions(FFrame& Stack, RESULT_DECL)
{
	P_GET_STRUCT_REF(FAuthSession, OutSessionInfo);
	P_FINISH;

	if (ClientAuthSessions.Num() == 0)
	{
		SKIP_ITERATOR;
	}
	else
	{
		TSparseArray<FAuthSession>::TIterator It(ClientAuthSessions);

		PRE_ITERATOR;
			if (It)
			{
				OutSessionInfo = *It;
				++It;
			}
			else
			{
				EXIT_ITERATOR;
				break;
			}
		POST_ITERATOR;
	}
}

/**
 * On a client, iterates all auth sessions we created for a server
 * NOTE: This iterator is remove-safe; ending a local client auth session from within this iterator will not mess up the order of iteration
 *
 * @param OutSessionInfo	Outputs the currently iterated auth session
 */
void UOnlineAuthInterfaceImpl::execAllLocalClientAuthSessions(FFrame& Stack, RESULT_DECL)
{
	P_GET_STRUCT_REF(FLocalAuthSession, OutSessionInfo);
	P_FINISH;

	if (LocalClientAuthSessions.Num() == 0)
	{
		SKIP_ITERATOR;
	}
	else
	{
		TSparseArray<FLocalAuthSession>::TIterator It(LocalClientAuthSessions);

		PRE_ITERATOR;
			if (It)
			{
				OutSessionInfo = *It;
				++It;
			}
			else
			{
				EXIT_ITERATOR;
				break;
			}
		POST_ITERATOR;
	}
}

/**
 * On a client, iterates all auth sessions for servers we are connecting/connected to
 * NOTE: This iterator is remove-safe; ending a server auth session from within this iterator will not mess up the order of iteration
 *
 * @param OutSessionInfo	Outputs the currently iterated auth session
 */
void UOnlineAuthInterfaceImpl::execAllServerAuthSessions(FFrame& Stack, RESULT_DECL)
{
	P_GET_STRUCT_REF(FAuthSession, OutSessionInfo);
	P_FINISH;

	if (ServerAuthSessions.Num() == 0)
	{
		SKIP_ITERATOR;
	}
	else
	{
		TSparseArray<FAuthSession>::TIterator It(ServerAuthSessions);

		PRE_ITERATOR;
			if (It)
			{
				OutSessionInfo = *It;
				++It;
			}
			else
			{
				EXIT_ITERATOR;
				break;
			}
		POST_ITERATOR;
	}
}

/**
 * On a server, iterates all auth sessions we created for clients
 * NOTE: This iterator is remove-safe; ending a local server auth session from within this iterator will not mess up the order of iteration
 *
 * @param OutSessionInfo	Outputs the currently iterated auth session
 */
void UOnlineAuthInterfaceImpl::execAllLocalServerAuthSessions(FFrame& Stack, RESULT_DECL)
{
	P_GET_STRUCT_REF(FLocalAuthSession, OutSessionInfo);
	P_FINISH;

	if (LocalServerAuthSessions.Num() == 0)
	{
		SKIP_ITERATOR;
	}
	else
	{
		TSparseArray<FLocalAuthSession>::TIterator It(LocalServerAuthSessions);

		PRE_ITERATOR;
			if (It)
			{
				OutSessionInfo = *It;
				++It;
			}
			else
			{
				EXIT_ITERATOR;
				break;
			}
		POST_ITERATOR;
	}
}


/**
 * Finds the active/pending client auth session, for the client associated with the specified NetConnection
 *
 * @param ClientConnection	The NetConnection associated with the client
 * @param OutSessionInfo	Outputs the auth session info for the client
 * @return			Returns TRUE if a session was found for the client, FALSE otherwise
 */
UBOOL UOnlineAuthInterfaceImpl::FindClientAuthSession(class UPlayer* ClientConnection, FAuthSession& OutSessionInfo)
{
	FAuthSession* ReturnVal = GetClientAuthSession(Cast<UNetConnection>(ClientConnection));

	if (ReturnVal != NULL)
	{
		OutSessionInfo = *ReturnVal;
		return TRUE;
	}

	return FALSE;
}

/**
 * Finds the clientside half of an active/pending client auth session
 *
 * @param ServerConnection	The NetConnection associated with the server
 * @param OutSessionInfo	Outputs the auth session info for the client
 * @return			Returns TRUE if a session was found for the client, FALSE otherwise
 */
UBOOL UOnlineAuthInterfaceImpl::FindLocalClientAuthSession(class UPlayer* ServerConnection, FLocalAuthSession& OutSessionInfo)
{
	FLocalAuthSession* ReturnVal = GetLocalClientAuthSession(Cast<UNetConnection>(ServerConnection));

	if (ReturnVal != NULL)
	{
		OutSessionInfo = *ReturnVal;
		return TRUE;
	}

	return FALSE;
}

/**
 * Finds the active/pending server auth session, for the specified server connection
 *
 * @param ServerConnection	The NetConnection associated with the server
 * @param OutSessionInfo	Outputs the auth session info for the server
 * @return			Returns TRUE if a session was found for the server, FALSE otherwise
 */
UBOOL UOnlineAuthInterfaceImpl::FindServerAuthSession(class UPlayer* ServerConnection, FAuthSession& OutSessionInfo)
{
	FAuthSession* ReturnVal = GetServerAuthSession(Cast<UNetConnection>(ServerConnection));

	if (ReturnVal != NULL)
	{
		OutSessionInfo = *ReturnVal;
		return TRUE;
	}

	return FALSE;
}

/**
 * Finds the serverside half of an active/pending server auth session
 *
 * @param ClientConnection	The NetConnection associated with the client
 * @param OutSessionInfo	Outputs the auth session info for the server
 * @return			Returns TRUE if a session was found for the server, FALSE otherwise
 */
UBOOL UOnlineAuthInterfaceImpl::FindLocalServerAuthSession(class UPlayer* ClientConnection, FLocalAuthSession& OutSessionInfo)
{
	FLocalAuthSession* ReturnVal = GetLocalServerAuthSession(Cast<UNetConnection>(ClientConnection));

	if (ReturnVal != NULL)
	{
		OutSessionInfo = *ReturnVal;
		return TRUE;
	}

	return FALSE;
}


/**
 * Matches a UNetConnection to a ClientAuthSession entry
 *
 * @param InConn	The UNetConnection to match with
 * @param OutIndex	Outputs the index into ClientAuthSession
 * @return		The ClientAuthSession entry, or NULL
 */
FAuthSession* UOnlineAuthInterfaceImpl::GetClientAuthSession(UNetConnection* InConn)
{
	FAuthSession* ReturnVal = NULL;

	if (InConn != NULL)
	{
		DWORD ConnAddr = InConn->GetAddrAsInt();
		DWORD ConnPort = InConn->GetAddrPort();

		for (TSparseArray<FAuthSession>::TIterator It(ClientAuthSessions); It; ++It)
		{
			if (It->EndPointIP == ConnAddr && It->EndPointPort == ConnPort)
			{
				ReturnVal = &*It;
				break;
			}
		}
	}

	return ReturnVal;
}

/**
 * Matches a  UNetConnection to a LocalClientAuthSession entry
 *
 * @param InConn	The UNetConnection to match with
 * @param OutIndex	Outputs the index into LocalClientAuthSession
 * @return		The LocalClientAuthSession entry, or NULL
 */
FLocalAuthSession* UOnlineAuthInterfaceImpl::GetLocalClientAuthSession(UNetConnection* InConn)
{
	FLocalAuthSession* ReturnVal = NULL;

	if (InConn != NULL)
	{
		DWORD ConnAddr = InConn->GetAddrAsInt();
		DWORD ConnPort = InConn->GetAddrPort();
		QWORD ConnUID = InConn->PlayerId.Uid;

		for (TSparseArray<FLocalAuthSession>::TIterator It(LocalClientAuthSessions); It; ++It)
		{
			// NOTE: IP alone isn't a reliable way to match server connections, hence preferring UID matches, with IP as a fallback
			if (It->EndPointUID.Uid == ConnUID || (It->EndPointIP == ConnAddr && It->EndPointPort == ConnPort))
			{
				ReturnVal = &*It;
				break;
			}
		}
	}

	return ReturnVal;
}

/**
 * Matches a UNetConnection to a ServerAuthSession entry
 *
 * @param InConn	The UNetConnection to match with
 * @param OutIndex	Outputs the index into ServerAuthSession
 * @return		The ServerAuthSession entry, or NULL
 */
FAuthSession* UOnlineAuthInterfaceImpl::GetServerAuthSession(UNetConnection* InConn)
{
	FAuthSession* ReturnVal = NULL;

	if (InConn != NULL)
	{
		DWORD ConnAddr = InConn->GetAddrAsInt();
		DWORD ConnPort = InConn->GetAddrPort();
		QWORD ConnUID = InConn->PlayerId.Uid;

		// NOTE: IP alone isn't a reliable way to match server connections, hence preferring UID matches, with IP as a fallback
		for (TSparseArray<FAuthSession>::TIterator It(ServerAuthSessions); It; ++It)
		{
			if (It->EndPointUID == ConnUID || (It->EndPointIP == ConnAddr && It->EndPointPort == ConnPort))
			{
				ReturnVal = &*It;
				break;
			}
		}
	}

	return ReturnVal;
}

/**
 * Matches a UNetConnection to a LocalServerAuthSession entry
 *
 * @param InConn	The UNetConnection to match with
 * @param OutIndex	Outputs the index into LocalServerAuthSession
 * @return		The LocalServerAuthSession entry, or NULL
 */
FLocalAuthSession* UOnlineAuthInterfaceImpl::GetLocalServerAuthSession(UNetConnection* InConn)
{
	FLocalAuthSession* ReturnVal = NULL;

	if (InConn != NULL)
	{
		DWORD ConnAddr = InConn->GetAddrAsInt();
		DWORD ConnPort = InConn->GetAddrPort();

		for (TSparseArray<FLocalAuthSession>::TIterator It(LocalServerAuthSessions); It; ++It)
		{
			if (It->EndPointIP == ConnAddr && It->EndPointPort == ConnPort)
			{
				ReturnVal = &*It;
				break;
			}
		}
	}

	return ReturnVal;
}


IMPLEMENT_CLASS(UOnlineAuthInterfaceImpl);


#endif	// WITH_UE3_NETWORKING

