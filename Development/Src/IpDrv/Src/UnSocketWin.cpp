/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

/** Allows sockets to set custom errors, without overriding the entire socket subsystem */
INT GLastSocketError = SE_NO_ERROR;

#include "NetworkProfiler.h"

#if _WINDOWS && !WITH_PANORAMA // PC socket subsystem creation/destruction support

FSocketSubsystemWindows SocketSubsystem;

/**
 * Starts up the socket subsystem 
 *
 * @param bIsEarlyInit If TRUE, this function is being called before GCmdLine, GFileManager, GConfig, etc are setup. If FALSE, they have been initialized properly
 */
void appSocketInit(UBOOL bIsEarlyInit)
{
	// we can initialize at early init time, so do nothing at the later init time
	if (!bIsEarlyInit)
	{
		return;
	}

	GSocketSubsystem = &SocketSubsystem;
	GSocketSubsystemDebug = &SocketSubsystem;
	FString Error;
	if (GSocketSubsystem->Initialize(Error) == FALSE)
	{
		debugf(NAME_Init,TEXT("Failed to initialize socket subsystem: (%s)"),*Error);
	}
}

#endif

#if XBOX || _WINDOWS

/** Shuts down the socket subsystem */
void appSocketExit(void)
{
	if (GSocketSubsystem != NULL)
	{
		GSocketSubsystem->Destroy();
	}

	if (GSocketSubsystemDebug != NULL && GSocketSubsystemDebug != GSocketSubsystem)
	{
		GSocketSubsystemDebug->Destroy();
	}
}

/**
 * Closes the socket
 *
 * @param TRUE if it closes without errors, FALSE otherwise
 */
UBOOL FSocketWin::Close(void)
{
	return closesocket(Socket) == 0;
}

/**
 * Binds a socket to a network byte ordered address
 *
 * @param Addr the address to bind to
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketWin::Bind(const FInternetIpAddr& Addr)
{
	return bind(Socket,Addr,sizeof(SOCKADDR_IN)) == 0;
}

/**
 * Connects a socket to a network byte ordered address
 *
 * @param Addr the address to connect to
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketWin::Connect(const FInternetIpAddr& Addr)
{
	INT Return = connect(Socket,Addr,sizeof(SOCKADDR_IN));
	return Return == 0 ||
		Return == WSAEWOULDBLOCK ||
		(Return == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK);
}

/**
 * Places the socket into a state to listen for incoming connections
 *
 * @param MaxBacklog the number of connections to queue before refusing them
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketWin::Listen(INT MaxBacklog)
{
	return listen(Socket,MaxBacklog) == 0;
}

/**
 * Queries the socket to determine if there is a pending connection
 *
 * @param bHasPendingConnection out parameter indicating whether a connection is pending or not
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketWin::HasPendingConnection(UBOOL& bHasPendingConnection)
{
	UBOOL bHasSucceeded = FALSE;
	bHasPendingConnection = FALSE;
	// Check and return without waiting
	TIMEVAL Time = {0,0};
	fd_set SocketSet;
	// Set up the socket sets we are interested in (just this one)
	FD_ZERO(&SocketSet);
	FD_SET(Socket,&SocketSet);
	// Check the status of the bits. First check for errors
	INT SelectStatus = select(Socket + 1,NULL,NULL,&SocketSet,&Time);
	if (SelectStatus == 0)
	{
		FD_ZERO(&SocketSet);
		FD_SET(Socket,&SocketSet);
		// Now check to see if it has a pending connection
		SelectStatus = select(Socket + 1,&SocketSet,NULL,NULL,&Time);
		// One or more means there is a pending connection
		bHasPendingConnection = SelectStatus > 0;
		// Non negative means it worked
		bHasSucceeded = SelectStatus >= 0;
	}
	return bHasSucceeded;
}

/**
* Queries the socket to determine if there is pending data on the queue
*
* @param bHasPendingData out parameter indicating whether a connection has pending data or not
*
* @return TRUE if successful, FALSE otherwise
*/
UBOOL FSocketWin::HasPendingData(UINT& PendingDataSize)
{
	UBOOL bHasSucceeded = FALSE;
	PendingDataSize = 0;
	// Check and return without waiting
	TIMEVAL Time = {0,0};
	fd_set SocketSet;
	// Set up the socket sets we are interested in (just this one)
	FD_ZERO(&SocketSet);
	FD_SET(Socket,&SocketSet);
	// Check the status of the read socket.
	INT SelectStatus = select(Socket + 1,&SocketSet,NULL,NULL,&Time);
	if (SelectStatus > 0)
	{
		// See if there is any pending data on the read socket
		if (ioctlsocket( Socket, FIONREAD, (ULONG*)&PendingDataSize ) == 0)
		{
			bHasSucceeded = TRUE;
		}
	}
	return bHasSucceeded;
}

/**
 * Accepts a connection that is pending
 *
 * @param		SocketDescription debug description of socket
 *
 * @return		The new (heap-allocated) socket, or NULL if unsuccessful.
 */
FSocket* FSocketWin::Accept(const FString& SocketDescription)
{
	SOCKET NewSocket = accept(Socket,NULL,NULL);
	if ( NewSocket != INVALID_SOCKET )
	{
		return new FSocketWin( NewSocket, SocketType, SocketDescription );
	}
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
FSocket* FSocketWin::Accept(FInternetIpAddr& OutAddr,const FString& SocketDescription)
{
	INT SizeOf = sizeof(SOCKADDR_IN);
	SOCKET NewSocket = accept(Socket,OutAddr,&SizeOf);
	if ( NewSocket != INVALID_SOCKET )
	{
		return new FSocketWin( NewSocket, SocketType, SocketDescription );
	}
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
UBOOL FSocketWin::SendTo(const BYTE* Data,INT Count,INT& BytesSent,
	const FInternetIpAddr& Destination)
{
	// Write the data and see how much was written
	BytesSent = sendto(Socket,(const char*)Data,Count,0,Destination,sizeof(SOCKADDR_IN));
	NETWORK_PROFILER(FSocket::SendTo(Data,Count,BytesSent,Destination));
	return BytesSent >= 0;
}

/**
 * Sends a buffer on a connected socket
 *
 * @param Data the buffer to send
 * @param Count the size of the data to send
 * @param BytesSent out param indicating how much was sent
 */
UBOOL FSocketWin::Send(const BYTE* Data,INT Count,INT& BytesSent)
{
	do 
	{
		BytesSent = send(Socket,(const char*)Data,Count,0);
	} while ( BytesSent == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK );

	NETWORK_PROFILER(FSocket::Send(Data,Count,BytesSent));
	return BytesSent != SOCKET_ERROR;
}

/**
 * Reads a chunk of data from the socket. Gathers the source address too
 *
 * @param Data the buffer to read into
 * @param BufferSize the max size of the buffer
 * @param BytesRead out param indicating how many bytes were read from the socket
 * @param Source out param receiving the address of the sender of the data
 */
UBOOL FSocketWin::RecvFrom(BYTE* Data,INT BufferSize,INT& BytesRead,
	FInternetIpAddr& Source)
{
	INT Size = sizeof(SOCKADDR_IN);
	// Read into the buffer and set the source address
	BytesRead = recvfrom(Socket,(char*)Data,BufferSize,0,Source,&Size);
	NETWORK_PROFILER(FSocket::RecvFrom(Data,BufferSize,BytesRead,Source));
	return BytesRead >= 0;
}

/**
 * Reads a chunk of data from a connected socket
 *
 * @param Data the buffer to read into
 * @param BufferSize the max size of the buffer
 * @param BytesRead out param indicating how many bytes were read from the socket
 */
UBOOL FSocketWin::Recv(BYTE* Data,INT BufferSize,INT& BytesRead)
{
	BytesRead = recv(Socket,(char*)Data,BufferSize,0);
	NETWORK_PROFILER(FSocket::Recv(Data,BufferSize,BytesRead));
	return BytesRead >= 0;
}

/**
 * Determines the connection state of the socket
 */
ESocketConnectionState FSocketWin::GetConnectionState(void)
{
	ESocketConnectionState CurrentState = SCS_ConnectionError;
	// Check and return without waiting
	TIMEVAL Time = {0,0};
	fd_set SocketSet;
	// Set up the socket sets we are interested in (just this one)
	FD_ZERO(&SocketSet);
	FD_SET(Socket,&SocketSet);
	// Check the status of the bits. First check for errors
	INT SelectStatus = select(0,NULL,NULL,&SocketSet,&Time);
	if (SelectStatus == 0)
	{
		FD_ZERO(&SocketSet);
		FD_SET(Socket,&SocketSet);
		// Now check to see if it's connected (writable means connected)
		SelectStatus = select(0,NULL,&SocketSet,NULL,&Time);
		if (SelectStatus > 0)
		{
			CurrentState = SCS_Connected;
		}
		// Zero means it is still pending
		if (SelectStatus == 0)
		{
			CurrentState = SCS_NotConnected;
		}
	}
	return CurrentState;
}

/**
 * Reads the address the socket is bound to and returns it
 */
FInternetIpAddr FSocketWin::GetAddress(void)
{
	FInternetIpAddr Addr;
	SOCKLEN Size = sizeof(SOCKADDR_IN);
	// Figure out what ip/port we are bound to
	UBOOL bOk = getsockname(Socket,Addr,&Size) == 0;
	if (bOk == FALSE)
	{
		debugf(NAME_Error,TEXT("Failed to read address for socket (%s)"),
			GSocketSubsystem->GetSocketError());
	}
	return Addr;
}

/**
 * Sets this socket into non-blocking mode
 *
 * @param bIsNonBlocking whether to enable blocking or not
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketWin::SetNonBlocking(UBOOL bIsNonBlocking)
{
	DWORD Value = bIsNonBlocking ? TRUE : FALSE;
	return ioctlsocket(Socket,FIONBIO,&Value) == 0;
}

/**
 * Sets a socket into broadcast mode (UDP only)
 *
 * @param bAllowBroadcast whether to enable broadcast or not
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketWin::SetBroadcast(UBOOL bAllowBroadcast)
{
	return setsockopt(Socket,SOL_SOCKET,SO_BROADCAST,(char*)&bAllowBroadcast,sizeof(UBOOL)) == 0;
}

/**
 * Sets whether a socket can be bound to an address in use
 *
 * @param bAllowReuse whether to allow reuse or not
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL FSocketWin::SetReuseAddr(UBOOL bAllowReuse)
{
	return setsockopt(Socket,SOL_SOCKET,SO_REUSEADDR,(char*)&bAllowReuse,sizeof(UBOOL)) == 0;
}

/**
 * Sets whether and how long a socket will linger after closing
 *
 * @param bShouldLinger whether to have the socket remain open for a time period after closing or not
 * @param Timeout the amount of time to linger before closing
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL FSocketWin::SetLinger(UBOOL bShouldLinger,INT Timeout)
{
	LINGER ling;
	ling.l_onoff = bShouldLinger;
	ling.l_linger = Timeout;
	return setsockopt(Socket,SOL_SOCKET,SO_LINGER,(char*)&ling,sizeof(ling)) == 0;
}

/**
 * Enables error queue support for the socket
 *
 * @param bUseErrorQueue whether to enable error queueing or not
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL FSocketWin::SetRecvErr(UBOOL bUseErrorQueue)
{
	// Not supported, but return true to avoid spurious log messages
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
UBOOL FSocketWin::SetSendBufferSize(INT Size,INT& NewSize)
{
	INT SizeSize = sizeof(INT);
	UBOOL bOk = setsockopt(Socket,SOL_SOCKET,SO_SNDBUF,(char*)&Size,sizeof(INT)) == 0;
	// Read the value back in case the size was modified
	getsockopt(Socket,SOL_SOCKET,SO_SNDBUF,(char*)&NewSize,GCC_OPT_INT_CAST &SizeSize);
	return bOk;
}

/**
 * Sets the size of the receive buffer to use
 *
 * @param Size the size to change it to
 * @param NewSize the out value returning the size that was set (in case OS can't set that)
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL FSocketWin::SetReceiveBufferSize(INT Size,INT& NewSize)
{
	INT SizeSize = sizeof(INT);
	UBOOL bOk = setsockopt(Socket,SOL_SOCKET,SO_RCVBUF,(char*)&Size,sizeof(INT)) == 0;
	// Read the value back in case the size was modified
	getsockopt(Socket,SOL_SOCKET,SO_RCVBUF,(char*)&NewSize,GCC_OPT_INT_CAST &SizeSize);
	return bOk;
}

/**
 * Reads the port this socket is bound to
 */ 
INT FSocketWin::GetPortNo(void)
{
	const FInternetIpAddr& Addr = GetAddress();
	// Read the port number
	return Addr.GetPort();
}

/**
 * Does Windows platform initialization of the sockets library
 *
 * @param Error a string that is filled with error information
 *
 * @return TRUE if initialized ok, FALSE otherwise
 */
UBOOL FSocketSubsystemWindows::Initialize(FString& Error)
{
	if (bTriedToInit == FALSE)
	{
		bTriedToInit = TRUE;
		WSADATA WSAData;
		// Init WSA
		INT Code = WSAStartup(0x0101,&WSAData);
		if (Code == 0)
		{
			// We need a critical section to protect the shared data
			GIpDrvInitialized = TRUE;
			debugf(NAME_Init,
				TEXT("WinSock: version %i.%i (%i.%i), MaxSocks=%i, MaxUdp=%i"),
				WSAData.wVersion >> 8,WSAData.wVersion & 0xFF,
				WSAData.wHighVersion >> 8,WSAData.wHighVersion & 0xFF,
				WSAData.iMaxSockets,WSAData.iMaxUdpDg);
		}
		else
		{
			Error = FString::Printf(TEXT("WSAStartup failed (%s)"),
				GSocketSubsystem->GetSocketError(Code));
		}
	}
	return GIpDrvInitialized;
}

/**
 * Performs Windows specific socket clean up
 */
void FSocketSubsystemWindows::Destroy(void)
{
	WSACleanup();
}

/**
 * Creates a data gram (UDP) socket
 *
 * @param SocketDescription debug description
 * @param ignored
 *
 * @return the new socket or NULL if failed
 */
FSocket* FSocketSubsystemWindows::CreateDGramSocket(const FString& SocketDescription,UBOOL)
{
	SOCKET Socket = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	return Socket != INVALID_SOCKET ? new FSocketWin(Socket,SOCKTYPE_Datagram,SocketDescription) : NULL;
}

/**
 * Creates a stream (TCP) socket
 *
 * @param SocketDescription debug description
 *
 * @return the new socket or NULL if failed
 */
FSocket* FSocketSubsystemWindows::CreateStreamSocket(const FString& SocketDescription)
{
	SOCKET Socket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	return Socket != INVALID_SOCKET ? new FSocketWin(Socket,SOCKTYPE_Streaming,SocketDescription) : NULL;
}

/**
 * Cleans up a socket class
 *
 * @param Socket the socket object to destroy
 */
void FSocketSubsystemWindows::DestroySocket(FSocket* Socket)
{
	delete Socket;
}

/**
 * Returns a human readable string from an error code
 *
 * @param Code the error code to check
 */
const TCHAR* FSocketSubsystemWindows::GetSocketError(INT Code)
{
#if !FINAL_RELEASE
	if (Code == -1)
	{
		Code = GSocketSubsystem->GetLastErrorCode();
	}
	switch (Code)
	{
		case SE_NO_ERROR: return TEXT("SE_NO_ERROR");
		case SE_EINTR: return TEXT("SE_EINTR");
		case SE_EBADF: return TEXT("SE_EBADF");
		case SE_EACCES: return TEXT("SE_EACCES");
		case SE_EFAULT: return TEXT("SE_EFAULT");
		case SE_EINVAL: return TEXT("SE_EINVAL");
		case SE_EMFILE: return TEXT("SE_EMFILE");
		case SE_EWOULDBLOCK: return TEXT("SE_EWOULDBLOCK");
		case SE_EINPROGRESS: return TEXT("SE_EINPROGRESS");
		case SE_EALREADY: return TEXT("SE_EALREADY");
		case SE_ENOTSOCK: return TEXT("SE_ENOTSOCK");
		case SE_EDESTADDRREQ: return TEXT("SE_EDESTADDRREQ");
		case SE_EMSGSIZE: return TEXT("SE_EMSGSIZE");
		case SE_EPROTOTYPE: return TEXT("SE_EPROTOTYPE");
		case SE_ENOPROTOOPT: return TEXT("SE_ENOPROTOOPT");
		case SE_EPROTONOSUPPORT: return TEXT("SE_EPROTONOSUPPORT");
		case SE_ESOCKTNOSUPPORT: return TEXT("SE_ESOCKTNOSUPPORT");
		case SE_EOPNOTSUPP: return TEXT("SE_EOPNOTSUPP");
		case SE_EPFNOSUPPORT: return TEXT("SE_EPFNOSUPPORT");
		case SE_EAFNOSUPPORT: return TEXT("SE_EAFNOSUPPORT");
		case SE_EADDRINUSE: return TEXT("SE_EADDRINUSE");
		case SE_EADDRNOTAVAIL: return TEXT("SE_EADDRNOTAVAIL");
		case SE_ENETDOWN: return TEXT("SE_ENETDOWN");
		case SE_ENETUNREACH: return TEXT("SE_ENETUNREACH");
		case SE_ENETRESET: return TEXT("SE_ENETRESET");
		case SE_ECONNABORTED: return TEXT("SE_ECONNABORTED");
		case SE_ECONNRESET: return TEXT("SE_ECONNRESET");
		case SE_ENOBUFS: return TEXT("SE_ENOBUFS");
		case SE_EISCONN: return TEXT("SE_EISCONN");
		case SE_ENOTCONN: return TEXT("SE_ENOTCONN");
		case SE_ESHUTDOWN: return TEXT("SE_ESHUTDOWN");
		case SE_ETOOMANYREFS: return TEXT("SE_ETOOMANYREFS");
		case SE_ETIMEDOUT: return TEXT("SE_ETIMEDOUT");
		case SE_ECONNREFUSED: return TEXT("SE_ECONNREFUSED");
		case SE_ELOOP: return TEXT("SE_ELOOP");
		case SE_ENAMETOOLONG: return TEXT("SE_ENAMETOOLONG");
		case SE_EHOSTDOWN: return TEXT("SE_EHOSTDOWN");
		case SE_EHOSTUNREACH: return TEXT("SE_EHOSTUNREACH");
		case SE_ENOTEMPTY: return TEXT("SE_ENOTEMPTY");
		case SE_EPROCLIM: return TEXT("SE_EPROCLIM");
		case SE_EUSERS: return TEXT("SE_EUSERS");
		case SE_EDQUOT: return TEXT("SE_EDQUOT");
		case SE_ESTALE: return TEXT("SE_ESTALE");
		case SE_EREMOTE: return TEXT("SE_EREMOTE");
		case SE_EDISCON: return TEXT("SE_EDISCON");
		case SE_SYSNOTREADY: return TEXT("SE_SYSNOTREADY");
		case SE_VERNOTSUPPORTED: return TEXT("SE_VERNOTSUPPORTED");
		case SE_NOTINITIALISED: return TEXT("SE_NOTINITIALISED");
		case SE_HOST_NOT_FOUND: return TEXT("SE_HOST_NOT_FOUND");
		case SE_TRY_AGAIN: return TEXT("SE_TRY_AGAIN");
		case SE_NO_RECOVERY: return TEXT("SE_NO_RECOVERY");
		case SE_NO_DATA: return TEXT("SE_NO_DATA");
		default: return TEXT("Unknown Error");
	};
#else
	return TEXT("");
#endif
}

#endif

#if _WINDOWS // Xbox has it's own version of this
/**
 * Does a DNS look up of a host name
 *
 * @param HostName the name of the host to look up
 * @param Addr the address to copy the IP address to
 */
INT FSocketSubsystemWindows::GetHostByName(ANSICHAR* HostName,FInternetIpAddr& Addr)
{
	INT ErrorCode = 0;
	// gethostbyname() touches a static object so lock for thread safety
	FScopeLock ScopeLock(&HostByNameSynch);
	HOSTENT* HostEnt = gethostbyname(HostName);
	if (HostEnt != NULL)
	{
		// Make sure it's a valid type
		if (HostEnt->h_addrtype == PF_INET)
		{
			// Copy the data before letting go of the lock. This is safe only
			// for the copy locally. If another thread is reading this while
			// we are copying they will get munged data. This relies on the
			// consumer of this class to call the resolved() accessor before
			// attempting to read this data
			Addr.SetIp(*(in_addr*)(*HostEnt->h_addr_list));
		}
		else
		{
			ErrorCode = SE_HOST_NOT_FOUND;
		}
	}
	else
	{
		ErrorCode = WSAGetLastError();
	}
	return ErrorCode;
}

/**
 * Determines the name of the local machine
 *
 * @param HostName the string that receives the data
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketSubsystemWindows::GetHostName(FString& HostName)
{
	ANSICHAR Buffer[256];
	UBOOL bRead = gethostname(Buffer,256) == 0;
	if (bRead == TRUE)
	{
		HostName = Buffer;
	}
	return bRead;
}

/**
 * Uses the socket libs to look up the host address
 *
 * @param Out the output device to log messages to
 * @param HostAddr the out param receiving the host address
 *
 * @return TRUE if it can bind all ports, FALSE otherwise
 */
UBOOL FSocketSubsystemWindows::GetLocalHostAddr(FOutputDevice& Out,
	FInternetIpAddr& HostAddr)
{
	UBOOL CanBindAll = FALSE;
	HostAddr.SetAnyAddress();
	TCHAR Home[256]=TEXT("");
	FString HostName;
	if (GSocketSubsystem->GetHostName(HostName) == FALSE)
	{
		Out.Logf(TEXT("%s: gethostname failed (%s)"),SOCKET_API,
			GSocketSubsystem->GetSocketError());
	}
	if (Parse(appCmdLine(),TEXT("MULTIHOME="),Home,ARRAY_COUNT(Home)))
	{
		UBOOL bIsValid = FALSE;
		HostAddr.SetIp(Home,bIsValid);
		if (Home == NULL || bIsValid == FALSE)
		{
			Out.Logf( TEXT("Invalid multihome IP address %s"), Home );
		}
	}
	else
	{
		if (GSocketSubsystem->GetHostByName(TCHAR_TO_ANSI(*HostName),HostAddr) == 0)
		{
			if( !ParseParam(appCmdLine(),TEXT("PRIMARYNET")) )
			{
				CanBindAll = TRUE;
			}
			static UBOOL First;
			if( !First )
			{
				First = TRUE;
				debugf( NAME_Init, TEXT("%s: I am %s (%s)"), SOCKET_API, *HostName, *HostAddr.ToString(TRUE) );
			}
		}
		else
		{
			Out.Logf(TEXT("gethostbyname failed (%s)"),GSocketSubsystem->GetSocketError());
		}
	}
	return CanBindAll;
}
#endif

#endif	//#if WITH_UE3_NETWORKING
