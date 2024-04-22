/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"
#include <signal.h>

#if WITH_UE3_NETWORKING

#if PLATFORM_MACOSX
#include <net/if.h>
#include <ifaddrs.h>
#endif

#if PLATFORM_UNIX

FSocketSubsystemBSD SocketSubsystem;

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

/** Shuts down the socket subsystem */
void appSocketExit(void)
{
	GSocketSubsystem->Destroy();

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
UBOOL FSocketBSD::Close(void)
{
	UBOOL bSucceeded = close(Socket) == 0;
	Socket = -1;
	return bSucceeded;
}

/**
 * Binds a socket to a network byte ordered address
 *
 * @param Addr the address to bind to
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketBSD::Bind(const FInternetIpAddr& Addr)
{
	return bind(Socket,Addr,sizeof(struct sockaddr_in)) == 0;
}

/**
 * Connects a socket to a network byte ordered address
 *
 * @param Addr the address to connect to
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketBSD::Connect(const FInternetIpAddr& Addr)
{
	INT Err = connect(Socket,Addr,sizeof(struct sockaddr_in));
	if (Err == 0)
	{
		return TRUE;
	}
	Err = GSocketSubsystem->GetLastErrorCode();
	INT Return = FALSE;
	switch (Err)
	{
		case 0:
		case EAGAIN:
		case EINPROGRESS:
		case EINTR:
			Return = TRUE;
			break;
	}
	return Return;
}

/**
 * Places the socket into a state to listen for incoming connections
 *
 * @param MaxBacklog the number of connections to queue before refusing them
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketBSD::Listen(INT MaxBacklog)
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
UBOOL FSocketBSD::HasPendingConnection(UBOOL& bHasPendingConnection)
{
	UBOOL bHasSucceeded = FALSE;
	bHasPendingConnection = FALSE;
	// Check and return without waiting
	struct timeval Time = {0,0};
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
UBOOL FSocketBSD::HasPendingData(UINT& PendingDataSize)
{
	UBOOL bHasSucceeded = FALSE;
	PendingDataSize = 0;
	// Check and return without waiting
	struct timeval Time = {0,0};
	fd_set SocketSet;
	// Set up the socket sets we are interested in (just this one)
	FD_ZERO(&SocketSet);
	FD_SET(Socket,&SocketSet);
	// Check the status of the read socket.
	INT SelectStatus = select(Socket + 1,&SocketSet,NULL,NULL,&Time);
	if (SelectStatus > 0)
	{
		// See if there is any pending data on the read socket
        int Pending = 0;
		if (ioctl( Socket, FIONREAD, &Pending ) == 0)
		{
			PendingDataSize = (UINT) Pending;
			bHasSucceeded = TRUE;
		}
	}
	return bHasSucceeded;
}

/**
 * Accepts a connection that is pending
 *
 * @return		The new (heap-allocated) socket, or NULL if unsuccessful.
 */
FSocket* FSocketBSD::Accept(const FString& SocketDescription)
{
	SOCKET NewSocket = accept(Socket,NULL,NULL);
	if ( NewSocket != INVALID_SOCKET )
	{
		return new FSocketBSD( NewSocket, SocketType, SocketDescription );
	}
	return NULL;
}

/**
 * Accepts a connection that is pending
 *
 * @param OutAddr the address of the connection
 *
 * @return		The new (heap-allocated) socket, or NULL if unsuccessful.
 */
FSocket* FSocketBSD::Accept(FInternetIpAddr& OutAddr, const FString& SocketDescription)
{
	socklen_t SizeOf = sizeof(SOCKADDR_IN);
	SOCKET NewSocket = accept(Socket,OutAddr,&SizeOf);
	if ( NewSocket != INVALID_SOCKET )
	{
		return new FSocketBSD( NewSocket, SocketType, SocketDescription );
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
UBOOL FSocketBSD::SendTo(const BYTE* Data,INT Count,INT& BytesSent,
	const FInternetIpAddr& Destination)
{
	// Write the data and see how much was written
	BytesSent = sendto(Socket,(const char*)Data,Count,0,Destination,sizeof(struct sockaddr_in));
	return BytesSent >= 0;
}

/**
 * Sends a buffer on a connected socket
 *
 * @param Data the buffer to send
 * @param Count the size of the data to send
 * @param BytesSent out param indicating how much was sent
 */
UBOOL FSocketBSD::Send(const BYTE* Data,INT Count,INT& BytesSent)
{
	BytesSent = send(Socket,(const char*)Data,Count,0);
	
	// if we are writing to a dead socket, then close it
	if (BytesSent == -1 && errno == EPIPE)
	{
		Close();
	}
	return BytesSent >= 0;
}

/**
 * Reads a chunk of data from the socket. Gathers the source address too
 *
 * @param Data the buffer to read into
 * @param BufferSize the max size of the buffer
 * @param BytesRead out param indicating how many bytes were read from the socket
 * @param Source out param receiving the address of the sender of the data
 */
UBOOL FSocketBSD::RecvFrom(BYTE* Data,INT BufferSize,INT& BytesRead,
	FInternetIpAddr& Source)
{
	socklen_t Size = sizeof(struct sockaddr_in);
	// Read into the buffer and set the source address
	BytesRead = recvfrom(Socket,(char*)Data,BufferSize,0,Source,&Size);
	return BytesRead >= 0;
}

/**
 * Reads a chunk of data from a connected socket
 *
 * @param Data the buffer to read into
 * @param BufferSize the max size of the buffer
 * @param BytesRead out param indicating how many bytes were read from the socket
 */
UBOOL FSocketBSD::Recv(BYTE* Data,INT BufferSize,INT& BytesRead)
{
	BytesRead = recv(Socket,(char*)Data,BufferSize,0);
	return BytesRead >= 0;
}

/**
 * Determines the connection state of the socket
 */
ESocketConnectionState FSocketBSD::GetConnectionState(void)
{
	ESocketConnectionState CurrentState = SCS_ConnectionError;
	if (Socket != -1)
	{
		// Check and return without waiting
		struct timeval Time = {0,0};
		fd_set SocketSet;
		// Set up the socket sets we are interested in (just this one)
		FD_ZERO(&SocketSet);
		FD_SET(Socket,&SocketSet);
		// Check the status of the bits. First check for errors
		INT SelectStatus = select(Socket+1,NULL,NULL,&SocketSet,&Time);
		if (SelectStatus == 0)
		{
			struct timeval Time2 = {0,0};  // Linux may modify this struct, make a new one here...
			FD_ZERO(&SocketSet);
			FD_SET(Socket,&SocketSet);
			// Now check to see if it's connected (writable means connected)
			SelectStatus = select(Socket+1,NULL,&SocketSet,NULL,&Time2);
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
	}
	return CurrentState;
}

/**
 * Reads the address the socket is bound to and returns it
 */
FInternetIpAddr FSocketBSD::GetAddress(void)
{
	FInternetIpAddr Addr;
	socklen_t Size = sizeof(struct sockaddr_in);
	// Figure out what ip/port we are bound to
	UBOOL bOk = getsockname(Socket,Addr,&Size) == 0;
	if (bOk == FALSE)
	{
		debugf(NAME_Error,TEXT("Failed to read address for socket (%d)"),
			GSocketSubsystem->GetSocketError());
	}
	return Addr;
}

/**
 * Sets this socket into non-blocking mode
 *
 * @param bIsNonBlocking whether to enable broadcast or not
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketBSD::SetNonBlocking(UBOOL bIsNonBlocking)
{
	int Flags;
	Flags = fcntl( Socket, F_GETFL, 0 );
	if (bIsNonBlocking)
		Flags |= O_NONBLOCK;
	else
		Flags &= ~O_NONBLOCK;
	return fcntl( Socket, F_SETFL, Flags ) == 0;
}

/**
 * Sets a socket into broadcast mode (UDP only)
 *
 * @param bAllowBroadcast whether to enable broadcast or not
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketBSD::SetBroadcast(UBOOL bAllowBroadcast)
{
	const int bAllow = (int) bAllowBroadcast;
	return setsockopt(Socket,SOL_SOCKET,SO_BROADCAST,&bAllow,sizeof(bAllow)) == 0;
}

/**
 * Sets whether a socket can be bound to an address in use
 *
 * @param bAllowReuse whether to allow reuse or not
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL FSocketBSD::SetReuseAddr(UBOOL bAllowReuse)
{
	const int bAllow = (int) bAllowReuse;
	return setsockopt(Socket,SOL_SOCKET,SO_REUSEADDR,&bAllow,sizeof(bAllow)) == 0;
}

/**
 * Sets whether and how long a socket will linger after closing
 *
 * @param bShouldLinger whether to have the socket remain open for a time period after closing or not
 * @param Timeout the amount of time to linger before closing
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL FSocketBSD::SetLinger(UBOOL bShouldLinger,INT Timeout)
{
	struct linger ling;
	ling.l_onoff = bShouldLinger ? 1 : 0;
	ling.l_linger = Timeout;
	return setsockopt(Socket,SOL_SOCKET,SO_LINGER,&ling,sizeof(ling)) == 0;
}

/**
 * Enables error queue support for the socket
 *
 * @param bUseErrorQueue whether to enable error queueing or not
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL FSocketBSD::SetRecvErr(UBOOL bUseErrorQueue)
{
#if PLATFORM_LINUX
	const int bUse = (int) bUseErrorQueue;
	return setsockopt(Socket,SOL_IP,IP_RECVERR,&bUse,sizeof (bUse)) == 0;
#else
	return TRUE;
#endif
}

/**
 * Sets the size of the send buffer to use
 *
 * @param Size the size to change it to
 * @param NewSize the out value returning the size that was set (in case OS can't set that)
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL FSocketBSD::SetSendBufferSize(INT Size,INT& NewSize)
{
	int iSize = NewSize;
	INT Error = setsockopt(Socket,SOL_SOCKET,SO_SNDBUF,&iSize,sizeof(iSize));
	UBOOL bOk = Error == 0;

	socklen_t SizeSize = sizeof(iSize);
	// Read the value back in case the size was modified
	if (getsockopt(Socket,SOL_SOCKET,SO_SNDBUF,&iSize,&SizeSize) != -1)
	{
		NewSize = iSize;
	}

	// even if the code fails to set the size properly (ie too big for the platform), then
	// the socket will still operate properly, so we attempt to set the size, but return 
	// success anyway
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
UBOOL FSocketBSD::SetReceiveBufferSize(INT Size,INT& NewSize)
{
	int iSize = NewSize;
	UBOOL bOk = setsockopt(Socket,SOL_SOCKET,SO_RCVBUF,&iSize,sizeof(iSize)) == 0;
	socklen_t SizeSize = sizeof(iSize);
	// Read the value back in case the size was modified
	if (getsockopt(Socket,SOL_SOCKET,SO_RCVBUF,&iSize,&SizeSize) != -1)
		NewSize = iSize;
	return bOk;
}

/**
 * Reads the port this socket is bound to
 */ 
INT FSocketBSD::GetPortNo(void)
{
	const FInternetIpAddr& Addr = GetAddress();
	// Read the port number
	return Addr.GetPort();
}

/**
 * Does BSD Sockets platform initialization of the sockets library
 *
 * @param Error a string that is filled with error information
 *
 * @return TRUE if initialized ok, FALSE otherwise
 */
UBOOL FSocketSubsystemBSD::Initialize(FString& Error)
{
	if (bTriedToInit == FALSE)
	{
		bTriedToInit = TRUE;
		// We need a critical section to protect the shared data
		GIpDrvInitialized = TRUE;
		debugf(NAME_Init, TEXT("BSD Sockets initialized"));
	}
	return GIpDrvInitialized;
}

/**
 * Performs BSD Sockets specific socket clean up
 */
void FSocketSubsystemBSD::Destroy(void)
{
	bTriedToInit = FALSE;
}

/**
 * Creates a data gram (UDP) socket
 *
 * @param ignored
 *
 * @return the new socket or NULL if failed
 */
FSocket* FSocketSubsystemBSD::CreateDGramSocket(const FString& SocketDescription, UBOOL)
{
	SOCKET Socket = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	if (Socket == INVALID_SOCKET)
	{
		return NULL;
	}

	int bAllow = 1;
	setsockopt(Socket, SOL_SOCKET, SO_NOSIGPIPE, &bAllow, sizeof(bAllow));
	return new FSocketBSD(Socket,SOCKTYPE_Datagram,SocketDescription);
}

/**
 * Creates a stream (TCP) socket
 *
 * @return the new socket or NULL if failed
 */
FSocket* FSocketSubsystemBSD::CreateStreamSocket(const FString& SocketDescription)
{
	SOCKET Socket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if (Socket == INVALID_SOCKET)
	{
		return NULL;
	}
	
	int bAllow = 1;
	setsockopt(Socket, SOL_SOCKET, SO_NOSIGPIPE, &bAllow, sizeof(bAllow));
	return new FSocketBSD(Socket,SOCKTYPE_Streaming,SocketDescription);
}

/**
 * Cleans up a socket class
 *
 * @param Socket the socket object to destroy
 */
void FSocketSubsystemBSD::DestroySocket(FSocket* Socket)
{
	delete Socket;
}

/**
 * Returns a human readable string from an error code
 *
 * @param Code the error code to check
 */
const TCHAR* FSocketSubsystemBSD::GetSocketError(INT Code)
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
// !!! FIXME
//		case SE_EPROCLIM: return TEXT("SE_EPROCLIM");
		case SE_EUSERS: return TEXT("SE_EUSERS");
		case SE_EDQUOT: return TEXT("SE_EDQUOT");
		case SE_ESTALE: return TEXT("SE_ESTALE");
		case SE_EREMOTE: return TEXT("SE_EREMOTE");
// !!! FIXME
//		case SE_EDISCON: return TEXT("SE_EDISCON");
//		case SE_SYSNOTREADY: return TEXT("SE_SYSNOTREADY");
//		case SE_VERNOTSUPPORTED: return TEXT("SE_VERNOTSUPPORTED");
//		case SE_NOTINITIALISED: return TEXT("SE_NOTINITIALISED");
		case SE_HOST_NOT_FOUND: return TEXT("SE_HOST_NOT_FOUND");
		case SE_TRY_AGAIN: return TEXT("SE_TRY_AGAIN");
		case SE_NO_RECOVERY: return TEXT("SE_NO_RECOVERY");

// !!! FIXME: same value as EINTR.
		//case SE_NO_DATA: return TEXT("SE_NO_DATA");
		default: return TEXT("Unknown Error");
	};
#else
	return TEXT("");
#endif
}

/**
 * Does a DNS look up of a host name
 *
 * @param HostName the name of the host to look up
 * @param Addr the address to copy the IP address to
 */
INT FSocketSubsystemBSD::GetHostByName(ANSICHAR* HostName,FInternetIpAddr& Addr)
{
	INT ErrorCode = SE_HOST_NOT_FOUND;
#if PLATFORM_MACOSX
	// detect that it wants local address first, and if it does, use getifaddrs() instead of getaddrinfo().
	// That's because getaddrinfo() likes to return IP addresses of interfaces that are turned off before addresses of interfaces that are active.
	{
		ANSICHAR Buffer[256];
		if ( !gethostname(Buffer,256) && !strcmp(HostName,Buffer) )
		{
			struct ifaddrs *MyAddrs;
			if (!getifaddrs(&MyAddrs))
			{
				for (struct ifaddrs *InterfaceAddress = MyAddrs; InterfaceAddress != 0; InterfaceAddress = InterfaceAddress->ifa_next)
				{
					if ((InterfaceAddress->ifa_addr == NULL) || ((InterfaceAddress->ifa_flags & IFF_UP) == 0) || (InterfaceAddress->ifa_addr->sa_family != AF_INET))
					{
						continue;
					}

					struct sockaddr_in *ValidAddress = (struct sockaddr_in*)InterfaceAddress->ifa_addr;
					const in_addr &IP = ValidAddress->sin_addr;
					if ( (IP.s_addr == 0) || (IP.s_addr == 0x100007f) )	// we don't need localhost either
					{
						continue;
					}

					Addr.SetIp(IP);
					ErrorCode = 0;  // good to go.
					break;
				}
			}
		}

		return ErrorCode;
	}
#endif
	// getaddrinfo() is thread safe, unlike gethostbyname()...
	struct addrinfo *AddrInfo = NULL;
	int rc = getaddrinfo(HostName, NULL, NULL, &AddrInfo);
	if (rc != 0)
	{
		STUBBED("Handle getaddrinfo error codes"); // right now it's just SE_HOST_NOT_FOUND...
	}
	else
	{
		for (struct addrinfo *i = AddrInfo; i != NULL; i = i->ai_next)
		{
			if (i->ai_family == AF_INET)
			{
				const in_addr &IP = ((sockaddr_in *) i->ai_addr)->sin_addr;
				if (IP.s_addr != 0)
				{
					Addr.SetIp(IP);
					ErrorCode = 0;  // good to go.
					break;
				}
			}
		}
		freeaddrinfo(AddrInfo);
	}

	return ErrorCode;


#if 0  // here's a gethostbyname() version for platforms without getaddrinfo()...

	INT ErrorCode = 0;
	// gethostbyname() touches a static object so lock for thread safety
	FScopeLock ScopeLock(&HostByNameSynch);
	HOSTENT* HostEnt = gethostbyname(HostName);
	if (HostEnt != NULL)
	{
		// Make sure it's a valid type
		if (HostEnt->h_addrtype == AF_INET)
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
		ErrorCode = h_errno;
	}

	return ErrorCode;
#endif
}

/**
 * Determines the name of the local machine
 *
 * @param HostName the string that receives the data
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketSubsystemBSD::GetHostName(FString& HostName)
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
UBOOL FSocketSubsystemBSD::GetLocalHostAddr(FOutputDevice& Out,
	FInternetIpAddr& HostAddr)
{
	UBOOL CanBindAll = FALSE;
	HostAddr.SetAnyAddress();
	TCHAR Home[256]=TEXT("");
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
		FString HostName;
		if (GSocketSubsystem->GetHostName(HostName) == FALSE)
		{
			Out.Logf(TEXT("%s: gethostname failed (%s)"),SOCKET_API,
					 GSocketSubsystem->GetSocketError());
		}

#if IPHONE
		// append .local so device can figure it out
		if (GSocketSubsystem->GetHostByName(TCHAR_TO_ANSI(*(HostName + TEXT(".local"))),HostAddr) == 0)
#else
		if (GSocketSubsystem->GetHostByName(TCHAR_TO_ANSI(*HostName),HostAddr) == 0)
#endif
		{
			if( !ParseParam(appCmdLine(),TEXT("PRIMARYNET")) )
			{
				CanBindAll = TRUE;
			}
			static UBOOL First;
			if( !First )
			{
				First = TRUE;
				debugf( /*NAME_Init,*/ TEXT("%s: I am %s (%s)"), SOCKET_API, *HostName, *HostAddr.ToString(TRUE) );
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
