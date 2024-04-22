/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


#if WITH_UE3_NETWORKING
	/**
	 * Control channel messages
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
	virtual void OnClientAuthRequest(UNetConnection* Connection, QWORD ServerUID, DWORD PublicServerIP, INT PublicServerPort, UBOOL bSecure);

	/**
	 * Control message sent from client to server, requesting an auth session from the server
	 */
	virtual void OnServerAuthRequest(UNetConnection* Connection);

	/**
	 * Control channel message sent from one client to another (relayed by server), requesting an auth session with that client
	 *
	 * @param Connection		The NetConnection the message came from
	 * @param RemoteUID		The UID of the client that sent the request
	 */
	virtual void OnAuthRequestPeer(UNetConnection* Connection, QWORD RemoteUID)
	{
	}

	/**
	 * Control message sent both from client to server, and server to client, containing auth ticket data for auth verification
	 *
	 * @param Connection		The NetConnection the message came from
	 * @param BlobChunk		The current chunk/blob of auth ticket data
	 * @param Current		The current sequence of the blob/chunk
	 * @param Num			The total number of blobs/chunks being received
	 */
	virtual void OnAuthBlob(UNetConnection* Connection, const FString& BlobChunk, BYTE Current, BYTE Num);

	/**
	 * Control message sent from one client to another (relayed by server), containing auth ticket data for auth verification
	 *
	 * @param Connection		The NetConnection the message came from
	 * @param RemoteUID		The UID of the client that sent the blob data
	 * @param BlobChunk		The current chunk/blob of auth ticket data
	 * @param Current		The current sequence of the blob/chunk
	 * @param Num			The total number of blobs/chunks being received
	 */
	virtual void OnAuthBlobPeer(UNetConnection* Connection, QWORD RemoteUID, const FString& BlobChunk, BYTE Current, BYTE Num)
	{
	}

	/**
	 * Control message sent both from the server to client, for ending an active auth session
	 *
	 * @param Connection		The NetConnection the message came from
	 */
	virtual void OnClientAuthEndSessionRequest(UNetConnection* Connection)
	{
	}

	/**
	 * Control message sent from one client to another (relayed by server), for ending an active auth session
	 *
	 * @param Connection		The NetConnection the message came from
	 * @param RemoteUID		The UID of the client that's ending the auth session
	 */
	virtual void OnAuthKillPeer(UNetConnection* Connection, QWORD RemoteUID)
	{
	}

	/**
	 * Control message sent from client to server, requesting an auth retry
	 *
	 * @param Connection		The NetConnection the message came from
	 */
	virtual void OnAuthRetry(UNetConnection* Connection)
	{
	}


	/**
	 * Auth/Connection events
	 */

	/**
	 * Called on both client and server, when a net connection is closing
	 *
	 * @param Connection		The NetConnection that is closing
	 */
	virtual void OnAuthConnectionClose(UNetConnection* Connection);


	/**
	 * Control channel send/receive
	 */

	/**
	 * Sends an auth ticket over the specified net connection
	 *
	 * @param InConnection		The current connection
	 * @param AuthTicketUID		The UID of the auth ticket to send
	 */
	virtual UBOOL SendAuthTicket(UNetConnection* InConnection, INT AuthTicketUID);

	/**
	 * With client auth, checks if all blobs/pieces of the clients auth ticket have been received, and if so, auths the client
	 * NOTE: Serverside only
	 *
	 * @param Connection		The current connection
	 * @param ClientSession		The client auth session data
	 * @param TicketData		The current auth ticket data
	 */
	virtual void ProcessClientAuth(UNetConnection* Connection, FAuthSession* ClientSession, FAuthTicketData* TicketData);

	/**
	 * With server auth, checks if all blobs/pieces of the servers auth ticket have been received, and if so, auths the server
	 * NOTE: Clientside only
	 *
	 * @param Connection		The current connection
	 * @param ServerSession		The server auth session data
	 * @param TicketData		The current auth ticket data
	 */
	virtual void ProcessServerAuth(UNetConnection* Connection, FAuthSession* ServerSession, FAuthTicketData* TicketData);

	/**
	 * Processes a net connections accumulated auth blob data, and returns the final auth ticket
	 * NOTE: Auth data is cleared after calling, so can only be called once
	 *
	 * @param InTicketData		The current auth ticket data
	 * @param OutAuthTicket		The final output auth ticket
	 * @return			Returns TRUE if there was no error (does not mean an auth ticket was returned), and FALSE if not
	 */
	virtual UBOOL ProcessAuthTicket(FAuthTicketData* InTicketData, TArray<BYTE>** OutAuthTicket);

	/**
	 * Client auth functions
	 */

	/**
	 * Internal platform-specific implementation of EndLocalClientAuthSession
	 * (Ends the clientside half of a client auth session)
	 *
	 * @param LocalClientSession	The local client session to end
	 */
	virtual void InternalEndLocalClientAuthSession(FLocalAuthSession& LocalClientSession)
	{
	}

	/**
	 * Internal platform-specific implementation of EndRemoteClientAuthSession
	 * (Ends the serverside half of a client auth session)
	 *
	 * @param ClientSession		The client session to end
	 */
	virtual void InternalEndRemoteClientAuthSession(FAuthSession& ClientSession)
	{
	}


	/**
	 * Server auth functions
	 */

	/**
	 * Internal platform-specific implementation of EndLocalServerAuthSession
	 * (Ends the serverside half of a server auth session)
	 *
	 * @param LocalServerSession	The local server session to end
	 */
	virtual void InternalEndLocalServerAuthSession(FLocalAuthSession& LocalServerSession)
	{
	}

	/**
	 * Internal platform-specific implementation of EndRemoteServerAuthSession
	 * (Ends the clientside half of a server auth session)
	 *
	 * @param ServerSession		The server session to end
	 */
	virtual void InternalEndRemoteServerAuthSession(FAuthSession& ServerSession)
	{
	}

	/**
	 * Session access functions
	 */

	/**
	 * Matches a UNetConnection to a ClientAuthSession entry
	 *
	 * @param InConn	The UNetConnection to match with
	 * @return		The ClientAuthSession entry, or NULL
	 */
	FAuthSession* GetClientAuthSession(UNetConnection* InConn);

	/**
	 * Matches a  UNetConnection to a LocalClientAuthSession entry
	 *
	 * @param InConn	The UNetConnection to match with
	 * @return		The LocalClientAuthSession entry, or NULL
	 */
	FLocalAuthSession* GetLocalClientAuthSession(UNetConnection* InConn);

	/**
	 * Matches a UNetConnection to a ServerAuthSession entry
	 *
	 * @param InConn	The UNetConnection to match with
	 * @return		The ServerAuthSession entry, or NULL
	 */
	FAuthSession* GetServerAuthSession(UNetConnection* InConn);

	/**
	 * Matches a UNetConnection to a LocalServerAuthSession entry
	 *
	 * @param InConn	The UNetConnection to match with
	 * @return		The LocalServerAuthSession entry, or NULL
	 */
	FLocalAuthSession* GetLocalServerAuthSession(UNetConnection* InConn);

#endif	// WITH_UE3_NETWORKING





