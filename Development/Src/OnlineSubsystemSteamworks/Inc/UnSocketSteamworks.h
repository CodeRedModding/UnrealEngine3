/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _UN_SOCKET_STEAMWORKS_H
#define _UN_SOCKET_STEAMWORKS_H

/**
 * Steam Sockets implementation
 *
 * Steam sockets use the P2P sockets in the SteamAPI's ISteamNetworking interface, to make connections between servers and clients;
 * Steam handles NAT-punching behind the scenes for this, which allows seamless connections to a server even from behind a router/firewall.
 *
 * This is implemented in three primary layers:
 *	- Steam Sockets:
 *		FSteamworksSocket integrates Steam P2P sockets with the FSocket interface, which is what UE3 uses for basic socket communication;
 *		all net traffic with Steam sockets occurs through FSteamworksSocket and its subclasses (one for client and one for server).
 *
 *	- Steam Sockets Manager:
 *		Since the SteamAPI networking code requires accepting and closing of connections, whereas UE3 requires straight-through UDP
 *		connections for game sockets, FSteamSocketsManager is used to track all steam sockets, and to transparently manage the connections
 *		at the socket level (including other miscellaneous socket tasks).
 *
 *	- Steam Net Connection and Steam Net Driver:
 *		UIpNetDriverSteamworks is used to hook UE3 at the net driver level, which is between the sockets and higher level game networking.
 *
 *		When the game connections are created, it uses UIpNetConnectionSteamworks for the net connections, which link to FSocketSteamworks,
 *		instead of the usual IP-socket linked net connections.
 *
 * SteamId's:
 *	SteamId's are 64bit unique identifiers, which are used as connection addresses for Steam sockets (both within UE3 and in the SteamAPI).
 *	When a game server starts up, it is automatically assigned a SteamId by the Steam backend, and clients SteamId's are tied to their account.
 *
 *	Since SteamId's are 64bit and IP addresses (what UE3 uses within the internal netcode) are generally 32bit, the Steam sockets code does some
 *	special packing, fitting the SteamId into the IP address structs 64bit 'sin_zero' field, so it can be passed around transparently within UE3.
 *
 *	The game servers UID is advertised in the server browser using the 'SteamServerId' value, and can be retrieved through OnlineAuthInterface
 *	using 'GetServerUniqueId'.
 *
 *	To connect to game servers using Steam sockets, the Steam net driver looks for the special 'Steam.#' connection address in URL's (with #
 *	being the server SteamId number); an example connect string: "open steam.90085002618533890"
 *
 * Setup:
 *	To setup your game to use Steam sockets, you need to change the 'NetworkDevice' setting under [Engine.Engine] in *Engine.ini to:
 *		NetworkDevice=OnlineSubsystemSteamworks.IpNetDriverSteamworks
 *	This must be done for both server and client.
 *
 *	After that, you need to start your server with '?steamsockets' in the commandline to tell it to use Steam sockets, e.g:
 *		"UDKGame server dm-deck?steamsockets"
 *	It will use normal IP sockets without that specified on the commandline.
 *
 *	After that, clients can connect to the server using its SteamId; clients can also connect to the servers IP, and will be automatically
 *	redirected to the servers SteamId.
 *
 * See the UDN page for OnlineSubsystemSteamworks for more information:
 *	https://udn.epicgames.com/Three/OnlineSubsystemSteamworks#Steam%20sockets
 */


// Forward declarations
class FSteamSocketsManager;
class FSocketSteamworks;


/**
 * Debug defines
 */

// Enables/disables extra debug logging for P2P sockets
// @todo Steam: Remove at some stage
#define STEAM_SOCKETS_DEBUG 0
#define STEAM_SOCKETS_TRAFFIC_DEBUG 0


/**
 * Globals
 */

/** Manages tracking of active steam sockets, and handling of general socket management/events */
extern FSteamSocketsManager*		GSteamSocketsManager;


/**
 * Utility functions
 */

/**
 * Retrieves the SteamId stored within the specified IP address, returning TRUE if successful
 *
 * @param Addr		The IP address containing the SteamId
 * @param OutSteamId	The output SteamId, where the result is stored
 * @return		whether or not a SteamId was successfully retrieved
 */
FORCEINLINE UBOOL IpAddrToSteamId(const FInternetIpAddr& Addr, CSteamID& OutSteamId)
{
	UBOOL bSuccess = FALSE;

	const SOCKADDR* SockAddr = (const SOCKADDR*)Addr;
	const SOCKADDR_IN* SockAddrIn = (const SOCKADDR_IN*)SockAddr;
	const uint64 IdBits = *((const uint64*)SockAddrIn->sin_zero);
	CSteamID RetVal(IdBits);

	checkAtCompileTime(sizeof(SockAddrIn->sin_zero) >= 8, FInternetIpAddr_Padding_Cant_Hold_CSteamID);

	// Sanity check against our (mostly) mapped data in the IP/Port fields.
	// If these don't match, we either got corrupted data somewhere, or this FInternetAddr was a real IP address and not built from a Steam ID.
	DWORD Ip = 0;

	Addr.GetIp(Ip);

	DWORD WantedIp = (DWORD)RetVal.GetAccountID();

	if (Ip == WantedIp)
	{
		const INT Port = Addr.GetPort();
		const INT WantedPort = 
			( (((INT)RetVal.GetEUniverse()) & 0xFF) << 0 ) | 
			( (((INT)RetVal.GetEAccountType()) & 0xF) << 8 ) |
			( (((INT)RetVal.GetUnAccountInstance()) & 0xF) << 12 );

		if (Port == WantedPort)
		{
			OutSteamId = RetVal;
			bSuccess = TRUE;
		}
	}

	return bSuccess;
}

/**
 * Takes a SteamId and stores it within the specified IP address
 *
 * @param SteamId	The SteamId to be stored
 * @param Addr		The IP address to store the SteamId in
 */
FORCEINLINE void SteamIdToIpAddr(const CSteamID& SteamId, FInternetIpAddr& Addr)
{
	// try to make these values as unique as possible, in case the caller wants to compare addresses.
	Addr.SetIp((DWORD) SteamId.GetAccountID());

	// we strip most of the bits from the instance field. Oh well.  :/
	Addr.SetPort(
		( (((INT)SteamId.GetEUniverse()) & 0xFF) << 0 ) | 
		( (((INT)SteamId.GetEAccountType()) & 0xF) << 8 ) |
		( (((INT)SteamId.GetUnAccountInstance()) & 0xF) << 12 )
	);

	SOCKADDR* SockAddr = (SOCKADDR*)Addr;
	SOCKADDR_IN* SockAddrIn = (SOCKADDR_IN*)SockAddr;
	uint64* IdBits = (uint64*)SockAddrIn->sin_zero;
	*IdBits = SteamId.ConvertToUint64();
}


/**
 * FSteamSocketsManager - Tracks active sockets and handles passing events to them
 */
class FSteamSocketsManager : public FTickableObject
{
public:
	/**
	 * Base constructor
	 */
	FSteamSocketsManager()
		: bPendingDestroy(FALSE)
	{
	}

	/**
	 * Constructor stuff that needs to go in .cpp
	 */
	void InitializeSocketsManager();

	/**
	 * Called from the Steam net driver, when the Steamworks game server interface is initialized
	 */
	void InitGameServer();


	/**
	 * Sockets manager tick
	 *
	 * @param DeltaTime	The difference in time since the last tick
	 */
	void Tick(FLOAT DeltaTime);

	/**
	 * Whether or not this object should currently tick at all
	 */
	UBOOL IsTickable() const
	{
		return TRUE;
	}

	/**
	 * Whether or not this object should tick when the game is paused
	 */
	UBOOL IsTickableWhenPaused() const
	{
		return TRUE;
	}


	/**
	 * Called by the online subsystem, when the game is shutting down (from FinishDestroy)
	 */
	void NotifyDestroy();

	/**
	 * Finishes destruction, once all sockets are closed
	 */
	void FinishDestroy();

	/**
	 * Handles game-exit shutdown of sockets
	 */
	void PreExit(UNetDriver* NetDriver);


	/**
	 * Steam Socket tracking
	 */

	/**
	 * Adds a steam socket for tracking
	 *
	 * @param InSocket	The socket to add for tracking
	 */
	void AddSocket(FSocketSteamworks* InSocket);

	/**
	 * Removes a steam socket from tracking
	 * NOTE: May trigger socket manager destruction, if waiting for the last socket to close
	 *
	 * @param InSocket	The socket to remove from tracking
	 */
	void RemoveSocket(FSocketSteamworks* InSocket);

	/**
	 * Determines if the input socket is an active steam socket that is currently being tracked
	 *
	 * @param Insocket	The socket to check
	 * @return		Returns the socket cast to FSocketSteamworks, or NULL if it is not an actively tracked socket
	 */
	FORCEINLINE FSocketSteamworks* GetTrackedSocket(FSocket* InSocket)
	{
		FSocketSteamworks* ReturnVal = NULL;

		for (INT i=0; i<TrackedSockets.Num(); i++)
		{
			if (InSocket == (FSocket*)TrackedSockets(i))
			{
				ReturnVal = TrackedSockets(i);
				break;
			}
		}

		return ReturnVal;
	}


	/**
	 * Steam P2P session tracking
	 */

	/**
	 * Signal that a P2P session is still active, and shouldn't be cleaned up
	 *
	 * @param SteamId	The UID of the connection
	 */
	void TouchP2PSession(QWORD SteamId);

	/**
	 * Close an active P2P session
	 *
	 * @param SteamId	The UID of the connection
	 * @param Interface	The networking interface to close the connection on
	 */
	void CloseP2PSession(QWORD SteamId, ISteamNetworking* Interface=NULL);


private:
	/** List of currently tracked Steam sockets */
	TArray<FSocketSteamworks*>	TrackedSockets;

	/** List of server sockets awaiting initialization (usually, these are sockets created before game server interface has fully initialized) */
	TArray<FSocketSteamworks*>	UninitializedServerSockets;


	/** Map of currently active Steam P2P sessions (maps UID against time last active) */
	TMap<QWORD, FLOAT>		ActiveP2PSessions;


	/** Used to delay destruction of the sockets manager, when we're waiting for active sockets to close */
	UBOOL				bPendingDestroy;
};


/**
 * FSocketSteamworks
 */
class FSocketSteamworks : public FSocket
{
public:
	/**
	 * Base constructor
	 *
	 * @param NetworkingImpl	The SteamAPI networking interface this socket should use
	 * @param InSteamId		The local SteamId (address) of this socket
	 * @param InSocketDescription	The human-readable description of this socket
	 */
	FSocketSteamworks(ISteamNetworking* NetworkingImpl, QWORD InSteamId, const FString& InSocketDescription)
		: FSocket(SOCKTYPE_Datagram, InSocketDescription)
		, LocalSteamId(InSteamId)
		, SteamSendMode(k_EP2PSendUnreliable)
		, SteamNetworkingImpl(NetworkingImpl)
	{
#if STEAM_SOCKETS_DEBUG
		debugf(NAME_DevOnline, TEXT("FSocketSteamworks::Constructor"));
#endif
	}

	/**
	 * Initializes the socket after construction; moved from the constructor, as using 'this' is unsafe in the constructor
	 * NOTE: Must be called after constructing a socket
	 */
	virtual void InitializeSocket()
	{
		// Allow sockets to initialize the sockets manager, in case a sockets is created before subsystem init
		if (GSteamSocketsManager == NULL)
		{
			GSteamSocketsManager = new FSteamSocketsManager();
			GSteamSocketsManager->InitializeSocketsManager();
		}

		GSteamSocketsManager->AddSocket(this);
	}

	/**
	 * Virtual destructor
	 */
	virtual ~FSocketSteamworks(void)
	{
		// NOTE: Removed call to 'Close' from here; redundant

		if (GSteamSocketsManager != NULL)
		{
			GSteamSocketsManager->RemoveSocket(this);
		}
	}


	/**
	 * Sets the Steam networking interface that this socket should use
	 *
	 * @param InInterface	The interface to use
	 */
	void SetNetworkingInterface(ISteamNetworking* InInterface)
	{
		SteamNetworkingImpl = InInterface;

		if (SteamNetworkingImpl != NULL && SteamNetworkingImpl == GSteamGameServerNetworking)
		{
			LocalSteamId = SteamGameServer_GetSteamID();
		}
	}

	/**
	 * Changes the Steam send mode; usually used by endgame exit code, to force a non-buffered send before socket shutdown
	 *
	 * @param NewSendMode	The new Steam send mode to use
	 */
	void SetSteamSendMode(EP2PSend NewSendMode)
	{
		SteamSendMode = NewSendMode;
	}

	/**
	 * Whether or not this socket is a game server socket (required additional tracking)
	 *
	 * @return	Whether or not this is a game server socket
	 */
	virtual UBOOL IsServerSocket()
	{
		return FALSE;
	}


	/**
	 * FSocket implementation
	 */

	/**
	 * Closes the socket
	 *
	 * @param TRUE if it closes without errors, FALSE otherwise
	 */
	virtual UBOOL Close(void)
	{
#if STEAM_SOCKETS_DEBUG
		debugf(NAME_DevOnline, TEXT("FSocketSteamworks::Close"));
#endif

		return TRUE;
	}

	/**
	 * Binds a socket to a network byte ordered address
	 *
	 * @param Addr the address to bind to
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL Bind(const FInternetIpAddr& Addr)
	{
		// We ignore the address. Steam figures out the NAT problems for us, etc. furthermore, we lie in GetAddress() and GetPort();
		// we map these to our Steam ID.
		return TRUE;
	}

	/**
	 * Connects a socket to a network byte ordered address
	 *
	 * @param Addr the address to connect to
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL Connect(const FInternetIpAddr& Addr)
	{
		// Connectionless only!
		return FALSE;
	}

	/**
	 * Places the socket into a state to listen for incoming connections
	 *
	 * @param MaxBacklog the number of connections to queue before refusing them
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL Listen(INT MaxBacklog)
	{
		// Connectionless only!
		return FALSE;
	}

	/**
	 * Queries the socket to determine if there is a pending connection
	 *
	 * @param bHasPendingConnection out parameter indicating whether a connection is pending or not
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL HasPendingConnection(UBOOL& bHasPendingConnection)
	{
		// Connectionless only!
		return FALSE;
	}

	/**
	* Queries the socket to determine if there is pending data on the queue
	*
	* @param PendingDataSize out parameter indicating how much data is on the pipe for a single recv call
	*
	* @return TRUE if successful, FALSE otherwise
	*/
	virtual UBOOL HasPendingData(UINT& PendingDataSize);

	/**
	 * Accepts a connection that is pending
	 *
	 * @param		SocketDescription debug description of socket
	 * @return		The new (heap-allocated) socket, or NULL if unsuccessful.
	 */
	virtual FSocket* Accept(const FString& SocketDescription)
	{
		// Connectionless only!
		return NULL;
	}

	/**
	 * Accepts a connection that is pending
	 *
	 * @param OutAddr the address of the connection
	 * @param		SocketDescription debug description of socket
	 *
	 * @return		The new (heap-allocated) socket, or NULL if unsuccessful.
	 */
	virtual FSocket* Accept(FInternetIpAddr& OutAddr, const FString& SocketDescription)
	{
		// Connectionless only!
		return NULL;
	}

	/**
	 * Sends a buffer to a network byte ordered address
	 *
	 * @param Data the buffer to send
	 * @param Count the size of the data to send
	 * @param BytesSent out param indicating how much was sent
	 * @param Destination the network byte ordered address to send to
	 */
	virtual UBOOL SendTo(const BYTE* Data, INT Count, INT& BytesSent, const FInternetIpAddr& Destination);

	/**
	 * Sends a buffer on a connected socket
	 *
	 * @param Data the buffer to send
	 * @param Count the size of the data to send
	 * @param BytesSent out param indicating how much was sent
	 */
	virtual UBOOL Send(const BYTE* Data, INT Count, INT& BytesSent)
	{
		// Connectionless only!
		BytesSent = 0;
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
	virtual UBOOL RecvFrom(BYTE* Data, INT BufferSize, INT& BytesRead, FInternetIpAddr& Source);

	/**
	 * Reads a chunk of data from a connected socket
	 *
	 * @param Data the buffer to read into
	 * @param BufferSize the max size of the buffer
	 * @param BytesRead out param indicating how many bytes were read from the socket
	 */
	virtual UBOOL Recv(BYTE* Data, INT BufferSize, INT& BytesRead)
	{
		// Connectionless only!
		BytesRead = 0;
		return FALSE;
	}

	/**
	 * Determines the connection state of the socket
	 */
	virtual ESocketConnectionState GetConnectionState(void)
	{
		return SCS_NotConnected;
	}

	/**
	 * Reads the address the socket is bound to and returns it
	 */
	virtual FInternetIpAddr GetAddress(void)
	{
		FInternetIpAddr RetVal;
		SteamIdToIpAddr(LocalSteamId, RetVal);

		return RetVal;
	}

	/**
	 * Sets this socket into non-blocking mode
	 *
	 * @param bIsNonBlocking whether to enable blocking or not
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL SetNonBlocking(UBOOL bIsNonBlocking=TRUE)
	{
		// We ignore this
		return TRUE;
	}

	/**
	 * Sets a socket into broadcast mode (UDP only)
	 *
	 * @param bAllowBroadcast whether to enable broadcast or not
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL SetBroadcast(UBOOL bAllowBroadcast=TRUE)
	{
		// We ignore this
		return TRUE;
	}

	/**
	 * Sets whether a socket can be bound to an address in use
	 *
	 * @param bAllowReuse whether to allow reuse or not
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetReuseAddr(UBOOL bAllowReuse=TRUE)
	{
		// We ignore this
		return TRUE;
	}

	/**
	 * Sets whether and how long a socket will linger after closing
	 *
	 * @param bShouldLinger whether to have the socket remain open for a time period after closing or not
	 * @param Timeout the amount of time to linger before closing
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetLinger(UBOOL bShouldLinger=TRUE, INT Timeout=0)
	{
		// We ignore this
		return TRUE;
	}

	/**
	 * Enables error queue support for the socket
	 *
	 * @param bUseErrorQueue whether to enable error queuing or not
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetRecvErr(UBOOL bUseErrorQueue=TRUE)
	{
		// We ignore this
		return TRUE;
	}

	/**
	 * Sets the size of the send buffer to use
	 *
	 * @param Size the size to change it to
	 * @param NewSize the out value returning the size that was set (in case OS can't set that)
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetSendBufferSize(INT Size, INT& NewSize)
	{
		// We ignore this
		NewSize = Size;
		return TRUE;
	}

	/**
	 * Sets the size of the receive buffer to use
	 *
	 * @param Size the size to change it to
	 * @param NewSize the out value returning the size that was set (in case OS can't set that)
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetReceiveBufferSize(INT Size, INT& NewSize)
	{
		// We ignore this
		NewSize = Size;
		return TRUE;
	}

	/**
	 * Reads the port this socket is bound to.
	 */ 
	virtual INT GetPortNo(void)
	{
		FInternetIpAddr RetVal;
		SteamIdToIpAddr(LocalSteamId, RetVal);

		return RetVal.GetPort();
	}

	/**
	 * Fetches the IP address that generated the error
	 *
	 * @param FromAddr the out param getting the address
	 *
	 * @return TRUE if succeeded, FALSE otherwise
	 */
	virtual UBOOL GetErrorOriginatingAddress(FInternetIpAddr& FromAddr)
	{
		// This is unimplemented everywhere (and only called from a piece of #if 0'd code that was meant for Linux servers)
		// Everyone else returns TRUE (incorrectly!)
		return TRUE;
	}


public:
	/** Stores UIDS linked to this socket, where the connection has failed (used to trigger a socket error in RecvFrom) */
	TArray<QWORD>		PortUnreachables;

protected:
	/** The SteamId this socket is bound to */
	CSteamID		LocalSteamId;

	/** The internal Steam sockets send mode this socket is currently set at */
	EP2PSend		SteamSendMode;

	/** A reference to the Steam networking interface this socket is bound to */
	ISteamNetworking*	SteamNetworkingImpl;
};


// The SteamAPI has a separate interface for the client and game server sockets, so we need to define classes to split them accordingly

/**
 * FSocketSteamworksServer
 */
class FSocketSteamworksServer : public FSocketSteamworks
{
public:
	/**
	 * Base constructor
	 *
	 * @param InSocketDescription	The human-readable description of this socket
	 */
	FSocketSteamworksServer(const FString& InSocketDescription)
		: FSocketSteamworks(GSteamGameServerNetworking, (GSteamGameServerNetworking != NULL ? SteamGameServer_GetSteamID() : 0),
					InSocketDescription)
	{
#if STEAM_SOCKETS_DEBUG
		debugf(NAME_DevOnline, TEXT("FSocketSteamworksServer::Constructor"));
#endif
	}

#if STEAM_SOCKETS_DEBUG
	/**
	 * Base destructor
	 */
	virtual ~FSocketSteamworksServer()
	{
		debugf(NAME_DevOnline, TEXT("FSocketSteamworksServer::~Destructor"));
	}
#endif


	/**
	 * Whether or not this socket is a game server socket (required additional tracking)
	 *
	 * @return	Whether or not this is a game server socket
	 */
	virtual UBOOL IsServerSocket()
	{
		return TRUE;
	}
};

/**
 * FSocketSteamworksClient
 */
class FSocketSteamworksClient : public FSocketSteamworks
{
public:
	/**
	 * Base constructor
	 *
	 * @param InSocketDescription	The human-readable description of this socket
	 */
	FSocketSteamworksClient(const FString& InSocketDescription)
		: FSocketSteamworks(GSteamNetworking, (GSteamUser != NULL ? (QWORD)GSteamUser->GetSteamID().ConvertToUint64() : 0),
					InSocketDescription)
	{
#if STEAM_SOCKETS_DEBUG
		debugf(NAME_DevOnline, TEXT("FSocketSteamworksClient::Constructor"));
#endif
	}

#if STEAM_SOCKETS_DEBUG
	/**
	 * Base destructor
	 */
	virtual ~FSocketSteamworksClient()
	{
		debugf(NAME_DevOnline, TEXT("FSocketSteamworksClient::~Destructor"));
	}
#endif
};


#endif _UN_SOCKET_STEAMWORKS_H

