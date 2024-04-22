/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Class that implements the Steamworks specific auth functionality
 */
Class OnlineAuthInterfaceSteamworks extends OnlineAuthInterfaceImpl within OnlineSubsystemCommonImpl
	native;


/**
 * Sends a client auth request to the specified client
 * NOTE: It is important to specify the ClientUID from PreLogin
 *
 * @param ClientConnection	The NetConnection of the client to send the request to
 * @param ClientUID		The UID of the client (as taken from PreLogin)
 * @return			whether or not the request kicked off successfully
 */
native function bool SendClientAuthRequest(Player ClientConnection, UniqueNetId ClientUID);

/**
 * Sends a server auth request to the server
 *
 * @param ServerUID		The UID of the server
 * @return			whether or not the request kicked off successfully
 */
native function bool SendServerAuthRequest(UniqueNetId ServerUID);


/**
 * Client auth functions, for authenticating clients with a game server
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
native function bool CreateClientAuthSession(UniqueNetId ServerUID, int ServerIP, int ServerPort, bool bSecure, out int OutAuthTicketUID);

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
native function bool VerifyClientAuthSession(UniqueNetId ClientUID, int ClientIP, int ClientPort, int AuthTicketUID);


/**
 * Server auth functions, for authenticating the server with clients
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
native function bool CreateServerAuthSession(UniqueNetId ClientUID, int ClientIP, int ClientPort, out int OutAuthTicketUID);

/**
 * Kicks off asynchronous verification and setup of a server auth session, on the client;
 * auth success/failure is returned through OnServerAuthComplete
 *
 * @param ServerUID		The UID of the server
 * @param ServerIP		The external/public IP address of the server
 * @param AuthTicketUID		The UID of the auth data sent by the server (as obtained through OnServerAuthResponse)
 * @return			whether or not asynchronous verification was kicked off successfully
 */
native function bool VerifyServerAuthSession(UniqueNetId ServerUID, int ServerIP, int AuthTicketUID);


/**
 * Platform-specific server information
 */

/**
 * If this is a server, retrieves the platform-specific UID of the server; used for authentication (not supported on all platforms)
 * NOTE: This is primarily used serverside, for listen host authentication
 *
 * @param OutServerUID		The UID of the server
 * @return			whether or not the server UID was retrieved
 */
native function bool GetServerUniqueId(out UniqueNetId OutServerUID);

/**
 * If this is a server, retrieves the platform-specific IP and port of the server; used for authentication
 * NOTE: This is primarily used serverside, for listen host authentication
 *
 * @param OutServerIP		The public IP of the server (or, for platforms which don't support it, the local IP)
 * @param OutServerPort		The port of the server
 */
native function bool GetServerAddr(out int OutServerIP, out int OutServerPort);







