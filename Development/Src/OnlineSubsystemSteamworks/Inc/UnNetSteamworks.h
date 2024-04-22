/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _UN_NETSTEAMWORKS_H
#define _UN_NETSTEAMWORKS_H

#if WITH_UE3_NETWORKING && WITH_STEAMWORKS


// NOTE: See UnSockets.h for a general description of Steam sockets


/**
 * Extern functions
 */

/**
 * Determines if the Steam sockets net driver is enabled
 *
 * @return	Returns TRUE if the steam sockets net driver is enabled, FALSE otherwise
 */
UBOOL appSteamNetDriverEnabled();


/**
 * Utility functions
 */

/**
 * Determines if the Steam sockets net driver is enabled
 *
 * @return	Returns TRUE if the steam sockets net driver is enabled, FALSE otherwise
 */
UBOOL IsSteamNetDriverEnabled();


/**
 * Determines whether or not steam sockets is forcibly enabled, so that all layers of the online subsystem assume steam sockets is enabled.
 *
 * This primarily affects server hosting (steam sockets is always on regardless of ?steamsockets parameter) the server browser (assumes all
 * servers use steam sockets, setting their SteamServerId without checking it is enabled), and invites (invites >ALWAYS< connect to the server
 * steam sockets address, and this has 100% NAT traversal)
 *
 * @return	Whether or not steam sockets is force-enabled
 */
UBOOL appSteamSocketsOnly();

/**
 * Determines whether or not steam sockets is forcibly enabled, so that all layers of the online subsystem assume steam sockets is enabled.
 *
 * This primarily affects server hosting (steam sockets is always on regardless of ?steamsockets parameter) the server browser (assumes all
 * servers use steam sockets, setting their SteamServerId without checking it is enabled), and invites (invites >ALWAYS< connect to the server
 * steam sockets address, and this has 100% NAT traversal)
 *
 * @return	Whether or not steam sockets is force-enabled
 */
UBOOL IsSteamSocketsOnly();

/**
 * Determine whether or not the client is connected to a steam sockets server (clientside only)
 *
 * @return	Whether or not this client is connected to a steam sockets server
 */
FORCEINLINE UBOOL IsSteamSocketsClient()
{
	UBOOL bReturnVal = FALSE;

	if (IsSteamNetDriverEnabled())
	{
		UNetDriver* NetDriver = GetActiveNetDriver();
		UTcpipConnection* ServerConn = (NetDriver != NULL ? Cast<UTcpipConnection>(NetDriver->ServerConnection) : NULL);

		if (ServerConn != NULL)
		{
			CSteamID Dud;
			bReturnVal = IpAddrToSteamId(ServerConn->RemoteAddr, Dud);
		}
	}

	return bReturnVal;
}

/**
 * Determines whether or not steam sockets is enabled for this server (serverside only)
 *
 * @return	Whether or not this server is using steam sockets
 */
FORCEINLINE UBOOL IsSteamSocketsServer()
{
	UBOOL bReturnVal = FALSE;
	AWorldInfo* WI = (GWorld != NULL ? GWorld->GetWorldInfo() : NULL);

	if (WI != NULL)
	{
		if (WI->NetMode == NM_ListenServer || WI->NetMode == NM_DedicatedServer)
		{
			if (IsSteamSocketsOnly())
			{
				bReturnVal = TRUE;
			}
			else
			{
				UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);

				if (GameEngine != NULL && IsSteamNetDriverEnabled())
				{
					// Small hack: This code can sometimes be called during GameInfo::InitGame,
					//		affecting whether or not '?steamsockets' is seen in the URL; need to check for that
					//		(through bScriptInitialized) and handle accordingly
					if (WI->Game != NULL && !WI->Game->bScriptInitialized)
					{
						bReturnVal = GWorld->URL.HasOption(TEXT("steamsockets"));
					}
					// In normal cases, LastURL is checked instead
					else
					{
						bReturnVal = GameEngine->LastURL.HasOption(TEXT("steamsockets"));
					}
				}
			}
		}
		else
		{
			debugf(TEXT("Don't use 'IsSteamSocketsServer' on non-listen-server clients; crashing"));
			check(FALSE);
		}
	}

	return bReturnVal;
}


/**
 * Class definitions
 */

/**
 * Steam specific IP net driver implementation
 */
class UIpNetDriverSteamworks : public UTcpNetDriver
{
	DECLARE_CLASS_INTRINSIC(UIpNetDriverSteamworks, UTcpNetDriver, CLASS_Config|CLASS_Transient, OnlineSubsystemSteamworks)

	/**
	 * Base constructor
	 */
	UIpNetDriverSteamworks() {}

	/**
	 * Static constructor (for config values)
	 */
	void StaticConstructor();


	/**
	 * Initiates a client connection to a game server, through ServerConnection
	 *
	 * @param InNotify	The FNetworkNotify object which is to receive networking notification events
	 * @param ConnectURL	The URL the client is connecting to
	 * @param Error		If there was an error during connection, this returns the error
	 * @return		whether or not the connect kicked off successfully
	 */
	UBOOL InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error);

	/**
	 * Initiates a server into listen mode, for accepting client connections
	 *
	 * @param InNotify	The FNetworkNotify object which is to receive networking notification events
	 * @param ListenURL	The startup URL for the server
	 * @param Error		If there is an error during setup, this returns the error
	 * @return		whether or not listen mode kicked off successfully
	 */
	UBOOL InitListen(FNetworkNotify* InNotify, FURL& ListenURL, FString& Error);

	/**
	 * Called by the game engine, when the game is exiting, to allow for special game-exit cleanup
	 */
	void PreExit();

	/**
	 * Called from the Steam subsystem, when the Steamworks game server interface is initialized
	 */
	void InitGameServer();

public:
	/** Whether or not the net driver should only allow steam sockets connections (forcing it to be enabled all the time) */
	UBOOL bSteamSocketsOnly;


	/** List of IP net connections, waiting to be redirected to a Steam sockets address */
	TArray<FIpAddr> PendingRedirects;
};

/**
 * Steam specific net connection implementation
 */
class UIpNetConnectionSteamworks : public UTcpipConnection
{
	DECLARE_CLASS_INTRINSIC(UIpNetConnectionSteamworks, UTcpipConnection, CLASS_Transient|CLASS_Config, OnlineSubsystemSteamworks)

	/**
	 * Base constructor
	 */
	UIpNetConnectionSteamworks();

	/**
	 * Initializes a connection with the passed in settings (modified for Steam, to store the SteamId in the IP address)
	 *
	 * @param InDriver		The net driver associated with this connection
	 * @param InSocket		The socket associated with this connection
	 * @param InRemoteAddr		The remote address for this connection
	 * @param InState		The connection state to start with for this connection
	 * @param InOpenedLocally	Whether the connection was a client/server
	 * @param InURL			The URL to init with
	 * @param InMaxPacket		The max packet size that will be used for sending
	 * @param InPacketOverhead	The packet overhead for this connection type
	 */
	void InitConnection(UNetDriver* InDriver, FSocket* InSocket, const FInternetIpAddr& InRemoteAddr, EConnectionState InState,
					UBOOL InOpenedLocally, const FURL& InURL, INT InMaxPacket=0, INT InPacketOverhead=0);

	/**
	 * Closes the control channel, cleans up structures, and prepares for deletion; used by Steam to close the Steam sockets connection
	 */
	virtual void CleanUp();
};


#endif // WITH_UE3_NETWORKING && WITH_STEAMWORKS

#endif // _UN_NETSTEAMWORKS_H

