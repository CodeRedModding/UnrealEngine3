/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _UN_SOCKET_BSD_H
#define _UN_SOCKET_BSD_H

#include <errno.h>

#if PLATFORM_SOLARIS
#include <sys/filio.h>  // for FIONREAD
#endif

// Handle glibc < 2.1.3
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0x4000
#endif

/** Platform specific errors mapped to our error codes */
enum ESocketErrors
{
	SE_NO_ERROR,
	SE_EINTR = EINTR,
	SE_EBADF = EBADF,
	SE_EACCES = EACCES,
	SE_EFAULT = EFAULT,
	SE_EINVAL = EINVAL,
	SE_EMFILE = EMFILE,
	SE_EWOULDBLOCK = EAGAIN,
	SE_EINPROGRESS = EINPROGRESS,
	SE_EALREADY = EALREADY,
	SE_ENOTSOCK = ENOTSOCK,
	SE_EDESTADDRREQ = EDESTADDRREQ,
	SE_EMSGSIZE = EMSGSIZE,
	SE_EPROTOTYPE = EPROTOTYPE,
	SE_ENOPROTOOPT = ENOPROTOOPT,
	SE_EPROTONOSUPPORT = EPROTONOSUPPORT,
	SE_ESOCKTNOSUPPORT = ESOCKTNOSUPPORT,
	SE_EOPNOTSUPP = EOPNOTSUPP,
	SE_EPFNOSUPPORT = EPFNOSUPPORT,
	SE_EAFNOSUPPORT = EAFNOSUPPORT,
	SE_EADDRINUSE = EADDRINUSE,
	SE_EADDRNOTAVAIL = EADDRNOTAVAIL,
	SE_ENETDOWN = ENETDOWN,
	SE_ENETUNREACH = ENETUNREACH,
	SE_ENETRESET = ENETRESET,
	SE_ECONNABORTED = ECONNABORTED,
	SE_ECONNRESET = ECONNRESET,
	SE_ENOBUFS = ENOBUFS,
	SE_EISCONN = EISCONN,
	SE_ENOTCONN = ENOTCONN,
	SE_ESHUTDOWN = ESHUTDOWN,
	SE_ETOOMANYREFS = ETOOMANYREFS,
	SE_ETIMEDOUT = ETIMEDOUT,
	SE_ECONNREFUSED = ECONNREFUSED,
	SE_ELOOP = ELOOP,
	SE_ENAMETOOLONG = ENAMETOOLONG,
	SE_EHOSTDOWN = EHOSTDOWN,
	SE_EHOSTUNREACH = EHOSTUNREACH,
	SE_ENOTEMPTY = ENOTEMPTY,
// !!! FIXME:
//	SE_EPROCLIM = EPROCLIM,
	SE_EUSERS = EUSERS,
	SE_EDQUOT = EDQUOT,
	SE_ESTALE = ESTALE,
	SE_EREMOTE = EREMOTE,
// !!! FIXME:
//	SE_EDISCON = EDISCON,
//	SE_SYSNOTREADY = WSASYSNOTREADY,
//	SE_VERNOTSUPPORTED = WSAVERNOTSUPPORTED,
//	SE_NOTINITIALISED = WSANOTINITIALISED,
	SE_HOST_NOT_FOUND = HOST_NOT_FOUND,
	SE_TRY_AGAIN = TRY_AGAIN,
	SE_NO_RECOVERY = NO_RECOVERY,
	SE_NO_DATA = NO_DATA,
	SE_UDP_ERR_PORT_UNREACH = ECONNRESET,
};

/**
 * This is the BSD Sockets specific socket class
 */
class FSocketBSD :
	public FSocket
{
protected:
	/** The BSD Sockets specific socket object */
	SOCKET Socket;

public:
	/**
	 * Assigns a BSD socket to this object
	 *
	 * @param InSocket the socket to assign to this object
	 * @param InSocketType the type of socket that was created
	 */
	FSocketBSD(SOCKET InSocket,ESocketType InSocketType,const FString& InSocketDescription) :
		FSocket(InSocketType, InSocketDescription),
		Socket(InSocket)
	{
	}
	/** Closes the socket if it is still open */
	virtual ~FSocketBSD(void)
	{
		Close();
	}
	/**
	 * Closes the socket
	 *
	 * @param TRUE if it closes without errors, FALSE otherwise
	 */
	virtual UBOOL Close(void);
	/**
	 * Binds a socket to a network byte ordered address
	 *
	 * @param Addr the address to bind to
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL Bind(const FInternetIpAddr& Addr);
	/**
	 * Connects a socket to a network byte ordered address
	 *
	 * @param Addr the address to connect to
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL Connect(const FInternetIpAddr& Addr);
	/**
	 * Places the socket into a state to listen for incoming connections
	 *
	 * @param MaxBacklog the number of connections to queue before refusing them
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL Listen(INT MaxBacklog);
	/**
	 * Queries the socket to determine if there is a pending connection
	 *
	 * @param bHasPendingConnection out parameter indicating whether a connection is pending or not
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL HasPendingConnection(UBOOL& bHasPendingConnection);
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
	 * @return		The new (heap-allocated) socket, or NULL if unsuccessful.
	 */
	virtual FSocket* Accept(const FString& SocketDescription);
	/**
	 * Accepts a connection that is pending
	 *
	 * @param OutAddr the address of the connection
	 *
	 * @return		The new (heap-allocated) socket, or NULL if unsuccessful.
	 */
	virtual FSocket* Accept(FInternetIpAddr& OutAddr, const FString& SocketDescription);
	/**
	 * Sends a buffer to a network byte ordered address
	 *
	 * @param Data the buffer to send
	 * @param Count the size of the data to send
	 * @param BytesSent out param indicating how much was sent
	 * @param Destination the network byte ordered address to send to
	 */
	virtual UBOOL SendTo(const BYTE* Data,INT Count,INT& BytesSent,const FInternetIpAddr& Destination);
	/**
	 * Sends a buffer on a connected socket
	 *
	 * @param Data the buffer to send
	 * @param Count the size of the data to send
	 * @param BytesSent out param indicating how much was sent
	 */
	virtual UBOOL Send(const BYTE* Data,INT Count,INT& BytesSent);
	/**
	 * Reads a chunk of data from the socket. Gathers the source address too
	 *
	 * @param Data the buffer to read into
	 * @param BufferSize the max size of the buffer
	 * @param BytesRead out param indicating how many bytes were read from the socket
	 * @param Source out param receiving the address of the sender of the data
	 */
	virtual UBOOL RecvFrom(BYTE* Data,INT BufferSize,INT& BytesRead,FInternetIpAddr& Source);
	/**
	 * Reads a chunk of data from a connected socket
	 *
	 * @param Data the buffer to read into
	 * @param BufferSize the max size of the buffer
	 * @param BytesRead out param indicating how many bytes were read from the socket
	 */
	virtual UBOOL Recv(BYTE* Data,INT BufferSize,INT& BytesRead);
	/**
	 * Determines the connection state of the socket
	 */
	virtual ESocketConnectionState GetConnectionState(void);
	/**
	 * Reads the address the socket is bound to and returns it
	 */
	virtual FInternetIpAddr GetAddress(void);
	/**
	 * Sets this socket into non-blocking mode
	 *
	 * @param bIsNonBlocking whether to enable broadcast or not
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL SetNonBlocking(UBOOL bIsNonBlocking = TRUE);
	/**
	 * Sets a socket into broadcast mode (UDP only)
	 *
	 * @param bAllowBroadcast whether to enable broadcast or not
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL SetBroadcast(UBOOL bAllowBroadcast = TRUE);
	/**
	 * Sets whether a socket can be bound to an address in use
	 *
	 * @param bAllowReuse whether to allow reuse or not
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetReuseAddr(UBOOL bAllowReuse = TRUE);
	/**
	 * Sets whether and how long a socket will linger after closing
	 *
	 * @param bShouldLinger whether to have the socket remain open for a time period after closing or not
	 * @param Timeout the amount of time to linger before closing
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetLinger(UBOOL bShouldLinger = TRUE,INT Timeout = 0);
	/**
	 * Enables error queue support for the socket
	 *
	 * @param bUseErrorQueue whether to enable error queueing or not
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetRecvErr(UBOOL bUseErrorQueue = TRUE);
	/**
	 * Sets the size of the send buffer to use
	 *
	 * @param Size the size to change it to
	 * @param NewSize the out value returning the size that was set (in case OS can't set that)
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetSendBufferSize(INT Size,INT& NewSize);
	/**
	 * Sets the size of the receive buffer to use
	 *
	 * @param Size the size to change it to
	 * @param NewSize the out value returning the size that was set (in case OS can't set that)
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetReceiveBufferSize(INT Size,INT& NewSize);
	/**
	 * Reads the port this socket is bound to.
	 */ 
	virtual INT GetPortNo(void);
	/**
	 * Fetches the IP address that generated the error
	 *
	 * @param FromAddr the out param getting the address
	 *
	 * @return TRUE if succeeded, FALSE otherwise
	 */
	virtual UBOOL GetErrorOriginatingAddress(FInternetIpAddr& FromAddr)
	{
		#ifdef __linux__
		socklen_t FromSize = sizeof (FromAddr);
		recvfrom(Socket, NULL, 0, MSG_ERRQUEUE, (sockaddr*)&FromAddr, &FromSize );
		#endif

		return TRUE;
	}
};

/**
 * BSD Sockets specific socket subsystem implementation
 */
class FSocketSubsystemBSD :
	public FSocketSubsystem
{
protected:
	/** Whether Init() has been called before or not */
	UBOOL bTriedToInit;
	/** Used to prevent multiple threads accessing the shared data */
	FCriticalSection HostByNameSynch;

public:
	/** Zeroes members */
	FSocketSubsystemBSD(void) :
		bTriedToInit(FALSE)
	{
	}

	/**
	 * Does BSD platform initialization of the sockets library
	 *
	 * @param Error a string that is filled with error information
	 *
	 * @return TRUE if initialized ok, FALSE otherwise
	 */
	virtual UBOOL Initialize(FString& Error);
	/**
	 * Performs BSD specific socket clean up
	 */
	virtual void Destroy(void);
	/**
	 * Creates a data gram socket
	 *
	 * @param bForceUDP overrides any platform specific protocol with UDP instead
	 *
	 * @return the new socket or NULL if failed
	 */
	virtual FSocket* CreateDGramSocket(const FString& SocketDescription, UBOOL bForceUDP = FALSE);
	/**
	 * Creates a stream socket
	 *
	 * @return the new socket or NULL if failed
	 */
	virtual FSocket* CreateStreamSocket(const FString& SocketDescription);
	/**
	 * Cleans up a socket class
	 *
	 * @param Socket the socket object to destroy
	 */
	virtual void DestroySocket(FSocket* Socket);
	/**
	 * Returns the last error that has happened
	 */
	virtual INT GetLastErrorCode(void)
	{
		return errno;
	}
	/**
	 * Returns a human readable string from an error code
	 *
	 * @param Code the error code to check
	 */
	virtual const TCHAR* GetSocketError(INT Code = -1);
	/**
	 * Does a DNS look up of a host name
	 *
	 * @param HostName the name of the host to look up
	 * @param Addr the address to copy the IP address to
	 */
	virtual INT GetHostByName(ANSICHAR* HostName,FInternetIpAddr& Addr);
	/**
	 * Determines the name of the local machine
	 *
	 * @param HostName the string that receives the data
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL GetHostName(FString& HostName);
	/**
	 * Uses the platform specific look up to determine the host address
	 *
	 * @param Out the output device to log messages to
	 * @param HostAddr the out param receiving the host address
	 *
	 * @return TRUE if all can be bound (no primarynet), FALSE otherwise
	 */
	virtual UBOOL GetLocalHostAddr(FOutputDevice& Out,FInternetIpAddr& HostAddr);
};

#endif

