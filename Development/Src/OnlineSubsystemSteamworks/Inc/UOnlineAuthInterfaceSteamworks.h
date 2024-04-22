/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#if WITH_UE3_NETWORKING && WITH_STEAMWORKS
	/**
	 * Interface initialization
	 *
	 * @param InSubsystem	Reference to the initializing subsystem
	 */
	void InitInterface(UOnlineSubsystemSteamworks* InSubsystem);

	/**
	 * Cleanup
	 */
	virtual void FinishDestroy();

	/**
	 * Control channel messages
	 */

	/**
	 * Control channel message sent from one client to another (relayed by server), requesting an auth session with that client
	 *
	 * @param Connection		The NetConnection the message came from
	 * @param RemoteUID		The UID of the client that sent the request
	 */
	void OnAuthRequestPeer(UNetConnection* Connection, QWORD RemoteUID);

	/**
	 * Control message sent from one client to another (relayed by server), containing auth ticket data for auth verification
	 *
	 * @param Connection		The NetConnection the message came from
	 * @param RemoteUID		The UID of the client that sent the blob data
	 * @param BlobChunk		The current chunk/blob of auth ticket data
	 * @param Current		The current sequence of the blob/chunk
	 * @param Num			The total number of blobs/chunks being received
	 */
	void OnAuthBlobPeer(UNetConnection* Connection, QWORD RemoteUID, const FString& BlobChunk, BYTE Current, BYTE Num);

	/**
	 * Control message sent from the server to client, for ending an active auth session
	 *
	 * @param Connection		The NetConnection the message came from
	 */
	void OnClientAuthEndSessionRequest(UNetConnection* Connection);

	/**
	 * Control message sent from one client to another (relayed by server), for ending an active auth session
	 *
	 * @param Connection		The NetConnection the message came from
	 * @param RemoteUID		The UID of the client that's ending the auth session
	 */
	void OnAuthKillPeer(UNetConnection* Connection, QWORD RemoteUID);

	/**
	 * Control message sent from client to server, requesting an auth retry
	 *
	 * @param Connection		The NetConnection the message came from
	 */
	void OnAuthRetry(UNetConnection* Connection);


	/**
	 * Steam callbacks
	 */

	/**
	 * Called when GSteamGameServer is fully setup and ready to authenticate players
	 */
	void NotifyGameServerAuthReady();

	/**
	 * Handles client authentication success/fails
	 * @todo Steam: Copy from cpp
	 */
	void ClientAuthComplete(UBOOL bSuccess, const QWORD SteamId, const FString ExtraInfo);


	/**
	 * Client auth functions
	 */

	/**
	 * Internal platform-specific implementation of EndLocalClientAuthSession
	 * (Ends the clientside half of a client auth session)
	 *
	 * @param LocalClientSession	The local client session to end
	 */
	void InternalEndLocalClientAuthSession(FLocalAuthSession& LocalClientSession);

	/**
	 * Internal platform-specific implementation of EndRemoteClientAuthSession
	 * (Ends the serverside half of a client auth session)
	 *
	 * @param ClientSession		The client session to end
	 */
	void InternalEndRemoteClientAuthSession(FAuthSession& ClientSession);


	/**
	 * Server auth functions
	 */

	/**
	 * Internal platform-specific implementation of EndLocalServerAuthSession
	 * (Ends the serverside half of a server auth session)
	 *
	 * @param LocalServerSession	The local server session to end
	 */
	void InternalEndLocalServerAuthSession(FLocalAuthSession& LocalServerSession);

	/**
	 * Internal platform-specific implementation of EndRemoteServerAuthSession
	 * (Ends the clientside half of a server auth session)
	 *
	 * @param ServerSession		The server session to end
	 */
	void InternalEndRemoteServerAuthSession(FAuthSession& ServerSession);

#endif	// WITH_UE3_NETWORKING && WITH_STEAMWORKS


