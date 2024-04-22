/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This interface deals with multiplayer authentication between players and servers
 *
 * NOTE: It is not a good idea to store 'Player' object references outside of Actor classes (and even then, you must be careful);
 *		instead, Player/NetConnection's should be tracked by IP and Port, where possible (see 'AllClientConnections' in WorldInfo)
 */
interface OnlineAuthInterface
	dependson(OnlineSubsystem)
	native;


/** Status of an auth session */
enum EAuthStatus
{
	/** The authentication process has not started yet */
	AUS_NotStarted,

	/** Authentication in progress, or about to be started */
	AUS_Pending,

	/** Successfully authenticated */
	AUS_Authenticated,

	/** Authentication has failed */
	AUS_Failed
};


/**
 * Base auth session information, common to local and remote auth session tracking
 */
struct native BaseAuthSession
{
	var const int		EndPointIP;		// The IP of the client/server on the other end of the auth session
	var const int		EndPointPort;		// The Port of the client/server is on
	var const UniqueNetId	EndPointUID;		// The UID of the client/server on the other end of the auth session
							//	(NOTE: Not verified for AuthSession until AuthStatus is AUS_Authenticated)
};

/**
 * Contains information for an auth session created locally, where we (the local client or server) are being authenticated
 */
struct native LocalAuthSession extends BaseAuthSession
{
	var const int		SessionUID;		// Platform-specific UID for the session; used for ending the local half of the session
};

/**
 * Contains information for an auth session created remotely, where we are authenticating the remote client/server
 */
struct native AuthSession extends BaseAuthSession
{
	var const EAuthStatus	AuthStatus;		// The current status of the auth session
	var const int		AuthTicketUID;		// The UID for tracking platform-specific auth data needed to perform authentication
};


/**
 * Used to check if the auth interface is ready to perform authentication
 *
 * @return	whether or not the auth interface is ready
 */
function bool IsReady();

/**
 * Called when the auth interface is ready to perform authentication
 */
delegate OnAuthReady();

/**
 * Sets the delegate used to notify when the auth interface is ready to perform authentication
 *
 * @param AuthReadyDelegate	The delegate to use for notification
 */
function AddAuthReadyDelegate(delegate<OnAuthReady> AuthReadyDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param AuthReadyDelegate	The delegate to remove from the list
 */
function ClearAuthReadyDelegate(delegate<OnAuthReady> AuthReadyDelegate);

/**
 * Called when the client receives a message from the server, requesting a client auth session
 *
 * @param ServerUID		The UID of the game server
 * @param ServerIP		The public (external) IP of the game server
 * @param ServerPort	The port of the game server
 * @param bSecure		whether or not the server has anticheat enabled (relevant to OnlineSubsystemSteamworks and VAC)
 */
delegate OnClientAuthRequest(UniqueNetId ServerUID, int ServerIP, int ServerPort, bool bSecure);

/**
 * Sets the delegate used to notify when the client receives a message from the server, requesting a client auth session
 *
 * @param ClientAuthRequestDelegate	The delegate to use for notifications
 */
function AddClientAuthRequestDelegate(delegate<OnClientAuthRequest> ClientAuthRequestDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param ClientAuthRequestDelegate	The delegate to remove from the list
 */
function ClearClientAuthRequestDelegate(delegate<OnClientAuthRequest> ClientAuthRequestDelegate);

/**
 * Called when the server receives a message from a client, requesting a server auth session
 *
 * @param ClientConnection	The NetConnection of the client the request came from
 * @param ClientUID		The UID of the client making the request
 * @param ClientIP		The IP of the client making the request
 * @param ClientPort		The port the client is on
 */
delegate OnServerAuthRequest(Player ClientConnection, UniqueNetId ClientUID, int ClientIP, int ClientPort);

/**
 * Sets the delegate used to notify when the server receives a message from a client, requesting a server auth session
 *
 * @param ServerAuthRequestDelegate	The delegate to use for notifications
 */
function AddServerAuthRequestDelegate(delegate<OnServerAuthRequest> ServerAuthRequestDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param ServerAuthRequestDelegate	The delegate to remove from the list
 */
function ClearServerAuthRequestDelegate(delegate<OnServerAuthRequest> ServerAuthRequestDelegate);

/**
 * Called when the server receives auth data from a client, needed for authentication
 *
 * @param ClientUID		The UID of the client
 * @param ClientIP		The IP of the client
 * @param AuthTicketUID		The UID used to reference the auth data
 */
delegate OnClientAuthResponse(UniqueNetId ClientUID, int ClientIP, int AuthTicketUID);

/**
 * Sets the delegate used to notify when the server receives a auth data from a client
 *
 * @param ClientAuthResponseDelegate	The delegate to use for notifications
 */
function AddClientAuthResponseDelegate(delegate<OnClientAuthResponse> ClientAuthResponseDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param ClientAuthResponseDelegate	The delegate to remove from the list
 */
function ClearClientAuthResponseDelegate(delegate<OnClientAuthResponse> ClientAuthResponseDelegate);

/**
 * Called when the client receives auth data from the server, needed for authentication
 *
 * @param ServerUID		The UID of the server
 * @param ServerIP		The IP of the server
 * @param AuthTicketUID		The UID used to reference the auth data
 */
delegate OnServerAuthResponse(UniqueNetId ServerUID, int ServerIP, int AuthTicketUID);

/**
 * Sets the delegate used to notify when the client receives a auth data from the server
 *
 * @param ServerAuthResponseDelegate	The delegate to use for notifications
 */
function AddServerAuthResponseDelegate(delegate<OnServerAuthResponse> ServerAuthResponseDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param ServerAuthResponseDelegate	The delegate to remove from the list
 */
function ClearServerAuthResponseDelegate(delegate<OnServerAuthResponse> ServerAuthResponseDelegate);

/**
 * Called on the server, when the authentication result for a client auth session has returned
 * NOTE: This is the first place, where a clients UID is verified as valid
 *
 * @param bSuccess		whether or not authentication was successful
 * @param ClientUID		The UID of the client
 * @param ClientConnection	The connection associated with the client (for retrieving auth session data)
 * @param ExtraInfo		Extra information about authentication, e.g. failure reasons
 */
delegate OnClientAuthComplete(bool bSuccess, UniqueNetId ClientUID, Player ClientConnection, string ExtraInfo);

/**
 * Sets the delegate used to notify when the server receives the authentication result for a client
 *
 * @param ClientAuthCompleteDelegate	The delegate to use for notifications
 */
function AddClientAuthCompleteDelegate(delegate<OnClientAuthComplete> ClientAuthCompleteDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param ClientAuthCompleteDelegate	The delegate to remove from the list
 */
function ClearClientAuthCompleteDelegate(delegate<OnClientAuthComplete> ClientAuthCompleteDelegate);

/**
 * Called on the client, when the authentication result for the server has returned
 *
 * @param bSuccess		whether or not authentication was successful
 * @param ServerUID		The UID of the server
 * @param ServerConnection	The connection associated with the server (for retrieving auth session data)
 * @param ExtraInfo		Extra information about authentication, e.g. failure reasons
 */
delegate OnServerAuthComplete(bool bSuccess, UniqueNetId ServerUID, Player ServerConnection, string ExtraInfo);

/**
 * Sets the delegate used to notify when the client receives the authentication result for the server
 *
 * @param ServerAuthCompleteDelegate	The delegate to use for notifications
 */
function AddServerAuthCompleteDelegate(delegate<OnServerAuthComplete> ServerAuthCompleteDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param ServerAuthCompleteDelegate	The delegate to remove from the list
 */
function ClearServerAuthCompleteDelegate(delegate<OnServerAuthComplete> ServerAuthCompleteDelegate);

/**
 * Called when the client receives a request from the server, to end an active auth session
 *
 * @param ServerConnection	The server NetConnection
 */
delegate OnClientAuthEndSessionRequest(Player ServerConnection);

/**
 * Sets the delegate used to notify when the client receives a request from the server, to end an active auth session
 *
 * @param ClientAuthEndSessionRequestDelegate	The delegate to use for notifications
 */
function AddClientAuthEndSessionRequestDelegate(delegate<OnClientAuthEndSessionRequest> ClientAuthEndSessionRequestDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param ClientAuthEndSessionRequestDelegate	The delegate to remove from the list
 */
function ClearClientAuthEndSessionRequestDelegate(delegate<OnClientAuthEndSessionRequest> ClientAuthEndSessionRequestDelegate);

/**
 * Called when the server receives a server auth retry request from a client
 *
 * @param ClientConnection	The client NetConnection
 */
delegate OnServerAuthRetryRequest(Player ClientConnection);

/**
 * Sets the delegate used to notify when the server receives a request from the client, to retry server auth
 *
 * @param ServerAuthRetryRequestDelegate	The delegate to use for notifications
 */
function AddServerAuthRetryRequestDelegate(delegate<OnServerAuthRetryRequest> ServerAuthRetryRequestDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param ServerAuthRetryRequestDelegate	The delegate to remove from the list
 */
function ClearServerAuthRetryRequestDelegate(delegate<OnServerAuthRetryRequest> ServerAuthRetryRequestDelegate);

/**
 * Called on the server when a clients net connection is closing (so auth sessions can be ended)
 *
 * @param ClientConnection	The client NetConnection that is closing
 */
delegate OnClientConnectionClose(Player ClientConnection);

/**
 * Sets the delegate used to notify when the a client net connection is closing
 *
 * @param ClientConnectionCloseDelegate		The delegate to use for notifications
 */
function AddClientConnectionCloseDelegate(delegate<OnClientConnectionClose> ClientConnectionCloseDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param ClientConnectionCloseDelegate		The delegate to remove from the list
 */
function ClearClientConnectionCloseDelegate(delegate<OnClientConnectionClose> ClientConnectionCloseDelegate);

/**
 * Called on the client when a server net connection is closing (so auth sessions can be ended)
 *
 * @param ServerConnection	The server NetConnection that is closing
 */
delegate OnServerConnectionClose(Player ServerConnection);

/**
 * Sets the delegate used to notify when the a server net connection is closing
 *
 * @param ServerConnectionCloseDelegate		The delegate to use for notifications
 */
function AddServerConnectionCloseDelegate(delegate<OnServerConnectionClose> ServerConnectionCloseDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param ServerConnectionCloseDelegate		The delegate to remove from the list
 */
function ClearServerConnectionCloseDelegate(delegate<OnServerConnectionClose> ServerConnectionCloseDelegate);


/**
 * Sends a client auth request to the specified client
 * NOTE: It is important to specify the ClientUID from PreLogin
 *
 * @param ClientConnection	The NetConnection of the client to send the request to
 * @param ClientUID		The UID of the client (as taken from PreLogin)
 * @return			whether or not the request kicked off successfully
 */
function bool SendClientAuthRequest(Player ClientConnection, UniqueNetId ClientUID);

/**
 * Sends a server auth request to the server
 *
 * @param ServerUID		The UID of the server
 * @return			whether or not the request kicked off successfully
 */
function bool SendServerAuthRequest(UniqueNetId ServerUID);

/**
 * Sends the specified auth ticket from the client to the server
 *
 * @param AuthTicketUID		The UID of the auth ticket, as retrieved by CreateClientAuthSession
 * @return			whether or not the auth ticket was sent successfully
 */
function bool SendClientAuthResponse(int AuthTicketUID);

/**
 * Sends the specified auth ticket from the server to the client
 *
 * @param ClientConnection	The NetConnection of the client to send the auth ticket to
 * @param AuthTicketUID		The UID of the auth ticket, as retrieved by CreateServerAuthSession
 * @return			whether or not the auth ticket was sent successfully
 */
function bool SendServerAuthResponse(Player ClientConnection, int AuthTicketUID);

/**
 * Sends an auth kill request to the specified client
 *
 * @param ClientConnection	The NetConnection of the client to send the request to
 * @return			whether or not the request was sent successfully
 */
function bool SendClientAuthEndSessionRequest(Player ClientConnection);

/**
 * Sends a server auth retry request to the server
 *
 * @return			whether or not the request was sent successfully
 */
function bool SendServerAuthRetryRequest();


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
function bool CreateClientAuthSession(UniqueNetId ServerUID, int ServerIP, int ServerPort, bool bSecure, out int OutAuthTicketUID);

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
function bool VerifyClientAuthSession(UniqueNetId ClientUID, int ClientIP, int ClientPort, int AuthTicketUID);

/**
 * Ends the clientside half of a client auth session
 * NOTE: This call must be matched on the server, with EndRemoteClientAuthSession
 *
 * @param ServerUID		The UID of the server
 * @param ServerIP		The external (public) IP address of the server
 * @param ServerPort		The port of the server
 */
function EndLocalClientAuthSession(UniqueNetId ServerUID, int ServerIP, int ServerPort);

/**
 * Ends the serverside half of a client auth session
 * NOTE: This call must be matched on the client, with EndLocalClientAuthSession
 *
 * @param ClientUID		The UID of the client
 * @param ClientIP		The IP address of the client
 */
function EndRemoteClientAuthSession(UniqueNetId ClientUID, int ClientIP);

/**
 * Ends the clientside halves of all client auth sessions
 * NOTE: This is the same as iterating AllLocalClientAuthSessions and ending each session with EndLocalClientAuthSession
 */
function EndAllLocalClientAuthSessions();

/**
 * Ends the serverside halves of all client auth sessions
 * NOTE: This is the same as iterating AllClientAuthSessions and ending each session with EndRemoteClientAuthSession
 */
function EndAllRemoteClientAuthSessions();

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
function bool CreateServerAuthSession(UniqueNetId ClientUID, int ClientIP, int ClientPort, out int OutAuthTicketUID);

/**
 * Kicks off asynchronous verification and setup of a server auth session, on the client;
 * auth success/failure is returned through OnServerAuthComplete
 *
 * @param ServerUID		The UID of the server
 * @param ServerIP		The external/public IP address of the server
 * @param AuthTicketUID		The UID of the auth data sent by the server (as obtained through OnServerAuthResponse)
 * @return			whether or not asynchronous verification was kicked off successfully
 */
function bool VerifyServerAuthSession(UniqueNetId ServerUID, int ServerIP, int AuthTicketUID);

/**
 * Ends the serverside half of a server auth session
 * NOTE: This call must be matched on the other end, with EndRemoteServerAuthSession
 *
 * @param ClientUID		The UID of the client
 * @param ClientIP		The IP address of the client
 */
function EndLocalServerAuthSession(UniqueNetId ClientUID, int ClientIP);

/**
 * Ends the clientside half of a server auth session
 * NOTE: This call must be matched on the other end, with EndLocalServerAuthSession
 *
 * @param ServerUID		The UID of the server
 * @param ServerIP		The external/public IP address of the server
 */
function EndRemoteServerAuthSession(UniqueNetId ServerUID, int ServerIP);

/**
 * Ends the serverside halves of all server auth sessions
 * NOTE: This is the same as iterating AllLocalServerAuthSessions and ending each session with EndLocalServerAuthSession
 */
function EndAllLocalServerAuthSessions();

/**
 * Ends the clientside halves of all server auth sessions
 * NOTE: This is the same as iterating AllServerAuthSessions and ending each session with EndRemoteServerAuthSession
 */
function EndAllRemoteServerAuthSessions();


/**
 * Auth info access functions
 */

/**
 * On a server, iterates all auth sessions for clients connected to the server
 * NOTE: This iterator is remove-safe; ending a client auth session from within this iterator will not mess up the order of iteration
 *
 * @param OutSessionInfo	Outputs the currently iterated auth session
 */
native function iterator AllClientAuthSessions(out AuthSession OutSessionInfo);

/**
 * On a client, iterates all auth sessions we created for a server
 * NOTE: This iterator is remove-safe; ending a local client auth session from within this iterator will not mess up the order of iteration
 *
 * @param OutSessionInfo	Outputs the currently iterated auth session
 */
native function iterator AllLocalClientAuthSessions(out LocalAuthSession OutSessionInfo);

/**
 * On a client, iterates all auth sessions for servers we are connecting/connected to
 * NOTE: This iterator is remove-safe; ending a server auth session from within this iterator will not mess up the order of iteration
 *
 * @param OutSessionInfo	Outputs the currently iterated auth session
 */
native function iterator AllServerAuthSessions(out AuthSession OutSessionInfo);

/**
 * On a server, iterates all auth sessions we created for clients
 * NOTE: This iterator is remove-safe; ending a local server auth session from within this iterator will not mess up the order of iteration
 *
 * @param OutSessionInfo	Outputs the currently iterated auth session
 */
native function iterator AllLocalServerAuthSessions(out LocalAuthSession OutSessionInfo);


/**
 * Finds the active/pending client auth session, for the client associated with the specified NetConnection
 *
 * @param ClientConnection	The NetConnection associated with the client
 * @param OutSessionInfo	Outputs the auth session info for the client
 * @return			Returns TRUE if a session was found for the client, FALSE otherwise
 */
function bool FindClientAuthSession(Player ClientConnection, out AuthSession OutSessionInfo);

/**
 * Finds the clientside half of an active/pending client auth session
 *
 * @param ServerConnection	The NetConnection associated with the server
 * @param OutSessionInfo	Outputs the auth session info for the client
 * @return			Returns TRUE if a session was found for the client, FALSE otherwise
 */
function bool FindLocalClientAuthSession(Player ServerConnection, out LocalAuthSession OutSessionInfo);

/**
 * Finds the active/pending server auth session, for the specified server connection
 *
 * @param ServerConnection	The NetConnection associated with the server
 * @param OutSessionInfo	Outputs the auth session info for the server
 * @return			Returns TRUE if a session was found for the server, FALSE otherwise
 */
function bool FindServerAuthSession(Player ServerConnection, out AuthSession OutSessionInfo);

/**
 * Finds the serverside half of an active/pending server auth session
 *
 * @param ClientConnection	The NetConnection associated with the client
 * @param OutSessionInfo	Outputs the auth session info for the server
 * @return			Returns TRUE if a session was found for the server, FALSE otherwise
 */
function bool FindLocalServerAuthSession(Player ClientConnection, out LocalAuthSession OutSessionInfo);


/**
 * Platform specific server information
 */

/**
 * If this is a server, retrieves the platform-specific UID of the server; used for authentication (not supported on all platforms)
 * NOTE: This is primarily used serverside, for listen host authentication
 *
 * @param OutServerUID		The UID of the server
 * @return			whether or not the server UID was retrieved
 */
function bool GetServerUniqueId(out UniqueNetId OutServerUID);

/**
 * If this is a server, retrieves the platform-specific IP and port of the server; used for authentication
 * NOTE: This is primarily used serverside, for listen host authentication
 *
 * @param OutServerIP		The public IP of the server (or, for platforms which don't support it, the local IP)
 * @param OutServerPort		The port of the server
 */
function bool GetServerAddr(out int OutServerIP, out int OutServerPort);







