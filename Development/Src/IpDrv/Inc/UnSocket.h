/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _UNSOCKET_H_
#define _UNSOCKET_H_

#if WITH_UE3_NETWORKING

#if PS3 || PLATFORM_UNIX
	typedef int					SOCKET;
	typedef struct hostent		HOSTENT;
	typedef in_addr				IN_ADDR;
	typedef struct sockaddr		SOCKADDR;
	typedef struct sockaddr_in	SOCKADDR_IN;
	typedef struct linger		LINGER;
	typedef socklen_t			SOCKLEN;

	#define INVALID_SOCKET		-1
	#define SOCKET_ERROR		-1
	#define GCC_OPT_INT_CAST
	#ifndef PF_INET
	#define PF_INET				AF_INET
	#endif
	#define IP(sin_addr,n) ((BYTE*)&sin_addr.s_addr)[n-1]
#elif WIIU
	typedef int					SOCKET;
	typedef struct hostent		HOSTENT;
	typedef in_addr				IN_ADDR;
	typedef struct sockaddr		SOCKADDR;
	typedef struct sockaddr_in	SOCKADDR_IN;
	typedef struct linger		LINGER;
	typedef socklen_t			SOCKLEN;

	#define htonl				SOHtoNl
	#define htons				SOHtoNs
	#define ntohl				SONtoHl
	#define ntohs				SONtoHs

	#define INVALID_SOCKET		-1
	#define INADDR_NONE			0xFFFFFFFF
	#define INADDR_BROADCAST	0xFFFFFFFF
	#define GCC_OPT_INT_CAST
	#ifndef PF_INET
	#define PF_INET				AF_INET
	#endif
	#define IP(sin_addr,n) ((BYTE*)&sin_addr.s_addr)[n-1]

	// Replace inet_addr 
	FORCEINLINE DWORD inet_addr(const ANSICHAR* AddressString)
	{
		in_addr Addr;
		SOInetPtoN(AF_INET, AddressString, &Addr);
		return Addr.s_addr;
	};
#elif NGP
	typedef SceNetInAddr		IN_ADDR;
 	typedef SceNetSockaddr		SOCKADDR;
 	typedef SceNetSockaddrIn	SOCKADDR_IN;

	#define htonl				sceNetHtonl
	#define htons				sceNetHtons
	#define ntohl				sceNetNtohl
	#define ntohs				sceNetNtohs
	#define INADDR_ANY			SCE_NET_INADDR_ANY
	#define INADDR_NONE			0xFFFFFFFF
	#define INADDR_BROADCAST	SCE_NET_INADDR_BROADCAST
	#define SOCKET_ERROR		-1
	#define GCC_OPT_INT_CAST
	#define AF_INET				SCE_NET_AF_INET
	
	// Replace inet_addr 
	FORCEINLINE DWORD inet_addr(const ANSICHAR* AddressString)
	{
		SceNetInAddr Addr;
		sceNetInetPton(SCE_NET_AF_INET, AddressString, &Addr);
		return Addr.s_addr;
	};

	#define IP(sin_addr,n) ((BYTE*)&sin_addr.s_addr)[n-1]
#endif

#if _MSC_VER
	#define IP(sin_addr,n) sin_addr.S_un.S_un_b.s_b##n
#endif

class FInternetIpAddr;

/**
 * Represents an IP address (ip + port) in host byte order. Not currently
 * hidden from platform specific code as it isn't needed yet.
 */
struct FIpAddr
{
	/** The ip address in host byte order */
	DWORD Addr;
	/** The port number in host byte order */
	DWORD Port;

	/** Zeroes the values */
	FIpAddr(DWORD InAddr=0,DWORD InPort=0) :
		Addr(InAddr),
		Port(InPort)
	{
	}
	/**
	 * Constructs an ip address from an internet ip address
	 *
	 * @param InternetIpAddr the ip address to get host order info from
	 */
	FIpAddr(const FInternetIpAddr& InternetIpAddr);
	/**
	 * Compares two ip addresses for equality
	 *
	 * @param Other the other object to compare against
	 *
	 * @return TRUE if equal, FALSE otherwise
	 */
	UBOOL operator==(const FIpAddr& Other) const
	{
		return Addr == Other.Addr && Port == Other.Port;
	}
	/**
	 * Converts this address into string form. Optionally including the port info
	 *
	 * @param bShowPort whether to append the port number or not
	 *
	 * @return A new string object with the ip address data in it
	 */
	FString ToString(UBOOL ShowPort) const;
	/**
	 * Builds an internet address from this host ip address object
	 */
	FInternetIpAddr GetSocketAddress(void) const;
	/**
	 * Serialization handler
	 *
	 * @param Ar the archive to serialize data with
	 * @param IP the object being serialized
	 */
    friend FArchive& operator<<( FArchive& Ar, FIpAddr& IP )
    {
        return Ar << IP.Addr << IP.Port;
    }
};

/** 
 * Encodes/decodes from platform addr to ip addr 
 */
class FPlatformIpAddr : public FIpAddr
{
public:

	/** Zeroes the values */
	FPlatformIpAddr(DWORD InAddr=0,DWORD InPort=0) 
		: FIpAddr(InAddr,InPort)
	{}
	/**
	 * Constructs an ip address from an internet ip address
	 *
	 * @param InternetIpAddr the ip address to get host order info from
	 */
	FPlatformIpAddr(const FInternetIpAddr& InternetIpAddr)
		: FIpAddr(InternetIpAddr)
	{}
	/**
	 * Constructor
	 */
	FPlatformIpAddr(const FIpAddr& InIpAddr) 
		: FIpAddr(InIpAddr.Addr,InIpAddr.Port)
	{}
	/**
	 * Convert ip address to platform data and serialize to buffer
	 *
	 * @param Buffer array to serialize to
	 * @return TRUE if encoding was successful
	 */
	UBOOL SerializeToBuffer(TArray<BYTE>& ToBuffer);
	/**
	 * Serialize from buffer and convert platform data to ip address
	 *
	 * @param Buffer array to serialize from
	 * @return TRUE if decoding was successful
	 */
	UBOOL SerializeFromBuffer(const TArray<BYTE>& FromBuffer);
};


/**
 * Represents an internet ip address. All data is in network byte order
 */
class FInternetIpAddr
{
	/** The internet ip address structure */
	SOCKADDR_IN Addr;

public:
	/**
	 * Constructor. Sets address to default state
	 */
	FInternetIpAddr(void)
	{
		appMemzero(&Addr,sizeof(SOCKADDR_IN));
		Addr.sin_family = AF_INET;
	}
	/**
	 * Sets the ip address from a host byte order DWORD
	 *
	 * @param InAddr the new address to use (must convert to network byte order)
	 */
	void SetIp(DWORD InAddr)
	{
		Addr.sin_addr.s_addr = htonl(InAddr);
	}
	/**
	 * Sets the ip address from a series of bytes
	 *
	 * @param A the a byte to use (A.B.C.D)
	 * @param B the b byte to use (A.B.C.D)
	 * @param C the c byte to use (A.B.C.D)
	 * @param D the d byte to use (A.B.C.D)
	 */
	void SetIp(BYTE A,BYTE B,BYTE C,BYTE D)
	{
		IP(Addr.sin_addr,1) = A;
		IP(Addr.sin_addr,2) = B;
		IP(Addr.sin_addr,3) = C;
		IP(Addr.sin_addr,4) = D;
	}
	/**
	 * Sets the ip address from a host byte order ip address
	 *
	 * @param InAddr the new address to use (must convert to network byte order)
	 */
	void SetIp(const FIpAddr& IpAddr)
	{
		Addr.sin_addr.s_addr = htonl(IpAddr.Addr);
	}
	/**
	 * Sets the ip address from a string ("A.B.C.D")
	 *
	 * @param InAddr the string containing the new ip address to use
	 */
	void SetIp(const TCHAR* InAddr,UBOOL& bIsValid)
	{
		DWORD ConvAddr = inet_addr(TCHAR_TO_ANSI(InAddr));
		// Make sure the address was valid
		if (ConvAddr != INADDR_NONE)
		{
			Addr.sin_addr.s_addr = ConvAddr;
		}
		else
		{
			debugfSlow(TEXT("Invalid IP address string (%s) passed to SetIp"),InAddr);
		}
		bIsValid = ConvAddr != INADDR_NONE;
	}
	/**
	 * Sets the ip address using a network byte order ip address
	 *
	 * @param IpAddr the new ip address to use
	 */
	void SetIp(const IN_ADDR& IpAddr)
	{
		Addr.sin_addr = IpAddr;
	}
	/**
	 * Copies the ip address info to a series of bytes
	 *
	 * @param A gets the a byte (A.B.C.D)
	 * @param B gets the b byte (A.B.C.D)
	 * @param C gets the c byte (A.B.C.D)
	 * @param D gets the d byte (A.B.C.D)
	 */
	void GetIp(BYTE& A,BYTE& B,BYTE& C,BYTE& D) const
	{
		A = IP(Addr.sin_addr,1);
		B = IP(Addr.sin_addr,2);
		C = IP(Addr.sin_addr,3);
		D = IP(Addr.sin_addr,4);
	}
	/**
	 * Copies the network byte order ip address to a host byte order dword
	 *
	 * @param OutAddr the out param receiving the ip address
	 */
	void GetIp(DWORD& OutAddr) const
	{
		OutAddr = ntohl(Addr.sin_addr.s_addr);
	}
	/**
	 * Copies the network byte order ip address to a host byte order address
	 *
	 * @param OutAddr the out param receiving the ip address
	 */
	void GetIp(FIpAddr& OutAddr) const
	{
		OutAddr.Addr = ntohl(Addr.sin_addr.s_addr);
	}
	/**
	 * Copies the network byte order ip address 
	 *
	 * @param OutAddr the out param receiving the ip address
	 */
	void GetIp(IN_ADDR& OutAddr) const
	{
		OutAddr = Addr.sin_addr;
	}

	/**
	 * Sets the port number from a host byte order int
	 *
	 * @param InPort the new port to use (must convert to network byte order)
	 */
	void SetPort(INT InPort)
	{
		Addr.sin_port = htons(InPort);
	}
	/**
	 * Sets the port number from a host byte order IP address
	 *
	 * @param IpAddr the IP address to get the port from (must convert to network byte order)
	 */
	void SetPort(const FIpAddr& IpAddr)
	{
		Addr.sin_port = htons(IpAddr.Port);
	}
	/**
	 * Copies the port number from this address and places it into a host byte order int
	 *
	 * @param OutPort the host byte order int that receives the port
	 */
	void GetPort(INT& OutPort) const
	{
		OutPort = ntohs(Addr.sin_port);
	}
	/**
	 * Copies the port number from this address and places it into a host byte
	 * order IP address
	 *
	 * @param OutIpAddr the host byte order IP address that receives the port
	 */
	void GetPort(FIpAddr& OutIpAddr) const
	{
		OutIpAddr.Port = ntohs(Addr.sin_port);
	}
	/**
	 * Returns the port number from this address in host byte order
	 */
	INT GetPort(void) const
	{
		return ntohs(Addr.sin_port);
	}

	/** Returns a host byte order version of this internet ip address */
	FIpAddr GetAddress(void) const
	{
		FIpAddr IpAddr;
		GetIp(IpAddr);
		GetPort(IpAddr);
		return IpAddr;
	}
	/**
	 * Sets the address using a host byte order ip address
	 *
	 * @param IpAddr the new ip address to use
	 */
	void SetAddress(const FIpAddr& IpAddr)
	{
		SetIp(IpAddr);
		SetPort(IpAddr);
	}
	/** Sets the address to be any address */
	void SetAnyAddress(void)
	{
		SetIp(INADDR_ANY);
		SetPort(0);
	}

	/**
	 * Converts this internet ip address to string form
	 *
	 * @param bAppendPort whether to append the port information or not
	 */
	FString ToString(UBOOL bAppendPort) const
	{
		// Get the individual bytes
		const INT A = IP(Addr.sin_addr,1);
		const INT B = IP(Addr.sin_addr,2);
		const INT C = IP(Addr.sin_addr,3);
		const INT D = IP(Addr.sin_addr,4);
		if (bAppendPort)
		{
			return FString::Printf(TEXT("%i.%i.%i.%i:%i"),A,B,C,D,GetPort());
		}
		else
		{
			return FString::Printf(TEXT("%i.%i.%i.%i"),A,B,C,D);
		}
	}

	/**
	 * Compares two internet ip addresses for equality
	 *
	 * @param Other the address to compare against
	 */
	UBOOL operator==(const FInternetIpAddr& Other) const
	{
		return Addr.sin_addr.s_addr == Other.Addr.sin_addr.s_addr &&
			Addr.sin_port == Other.Addr.sin_port &&
			Addr.sin_family == Other.Addr.sin_family;
	}

	/**
	 * Determines if the string is in IP address form or needs host resolution
	 *
	 * @param IpAddr the string to check for being a well formed ip
	 *
	 * @return TRUE if the ip address was parseable, FALSE otherwise
	 */
	static UBOOL IsValidIp(const TCHAR* IpAddr)
	{
		FInternetIpAddr LocalAddr;
		UBOOL bIsValid = FALSE;
		LocalAddr.SetIp(IpAddr,bIsValid);
		return bIsValid;
	}

//jgtemp
	operator SOCKADDR*(void)
	{
		return (SOCKADDR*)&Addr;
	}
	operator const SOCKADDR*(void) const
	{
		return (const SOCKADDR*)&Addr;
	}
};

/** Indicates the type of socket being used (streaming or datagram) */
enum ESocketType
{
	/** Not bound to a protocol yet */
	SOCKTYPE_Unknown,
	/** A UDP type socket */
	SOCKTYPE_Datagram,
	/** A TCP type socket */
	SOCKTYPE_Streaming
};

/** Indicates the connection state of the socket */
enum ESocketConnectionState
{
	SCS_NotConnected,
	SCS_Connected,
	/** Indicates that the end point refused the connection or couldn't be reached */
	SCS_ConnectionError
};

/**
 * This is our abstract base class that hides the platform specific socket implementation
 */
class FSocket
{
protected:
	/** Indicates the type of socket this is */
	const ESocketType SocketType;

	/** Debug description of socket usage. */
	FString SocketDescription;

public:
	/** Default ctor */
	inline FSocket(void) :
		SocketType(SOCKTYPE_Unknown)
	,	SocketDescription(TEXT(""))
	{
	}
	/**
	 * Specifies the type of socket being created
	 *
	 * @param InSocketType the type of socket being created
	 * @param InSocketDescription the debug description of the socket
	 */
	inline FSocket(ESocketType InSocketType,const FString& InSocketDescription) :
		SocketType(InSocketType)
	,	SocketDescription(InSocketDescription)
	{
	}
	/**
	 * Virtual destructor
	 */
	virtual ~FSocket(void)
	{
	}
	/**
	 * Closes the socket
	 *
	 * @param TRUE if it closes without errors, FALSE otherwise
	 */
	virtual UBOOL Close(void) = 0;
	/**
	 * Binds a socket to a network byte ordered address
	 *
	 * @param Addr the address to bind to
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL Bind(const FInternetIpAddr& Addr) = 0;
	/**
	 * Connects a socket to a network byte ordered address
	 *
	 * @param Addr the address to connect to
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL Connect(const FInternetIpAddr& Addr) = 0;
	/**
	 * Places the socket into a state to listen for incoming connections
	 *
	 * @param MaxBacklog the number of connections to queue before refusing them
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL Listen(INT MaxBacklog) = 0;
	/**
	 * Queries the socket to determine if there is a pending connection
	 *
	 * @param bHasPendingConnection out parameter indicating whether a connection is pending or not
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL HasPendingConnection(UBOOL& bHasPendingConnection) = 0;
	/**
	* Queries the socket to determine if there is pending data on the queue
	*
	* @param PendingDataSize out parameter indicating how much data is on the pipe for a single recv call
	*
	* @return TRUE if successful, FALSE otherwise
	*/
	virtual UBOOL HasPendingData(UINT& PendingDataSize) = 0;
	/**
	 * Accepts a connection that is pending
	 *
	 * @param		SocketDescription debug description of socket
	 * @return		The new (heap-allocated) socket, or NULL if unsuccessful.
	 */
	virtual FSocket* Accept(const FString& SocketDescription) = 0;
	/**
	 * Accepts a connection that is pending
	 *
	 * @param OutAddr the address of the connection
	 * @param		SocketDescription debug description of socket
	 *
	 * @return		The new (heap-allocated) socket, or NULL if unsuccessful.
	 */
	virtual FSocket* Accept(FInternetIpAddr& OutAddr, const FString& SocketDescription) = 0;
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
	virtual ESocketConnectionState GetConnectionState(void) = 0;
	/**
	 * Reads the address the socket is bound to and returns it
	 */
	virtual FInternetIpAddr GetAddress(void) = 0;
	/**
	 * Sets this socket into non-blocking mode
	 *
	 * @param bIsNonBlocking whether to enable blocking or not
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL SetNonBlocking(UBOOL bIsNonBlocking = TRUE) = 0;
	/**
	 * Sets a socket into broadcast mode (UDP only)
	 *
	 * @param bAllowBroadcast whether to enable broadcast or not
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL SetBroadcast(UBOOL bAllowBroadcast = TRUE) = 0;
	/**
	 * Sets whether a socket can be bound to an address in use
	 *
	 * @param bAllowReuse whether to allow reuse or not
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetReuseAddr(UBOOL bAllowReuse = TRUE) = 0;
	/**
	 * Sets whether and how long a socket will linger after closing
	 *
	 * @param bShouldLinger whether to have the socket remain open for a time period after closing or not
	 * @param Timeout the amount of time to linger before closing
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetLinger(UBOOL bShouldLinger = TRUE,INT Timeout = 0) = 0;
	/**
	 * Enables error queue support for the socket
	 *
	 * @param bUseErrorQueue whether to enable error queueing or not
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetRecvErr(UBOOL bUseErrorQueue = TRUE) = 0;
	/**
	 * Sets the size of the send buffer to use
	 *
	 * @param Size the size to change it to
	 * @param NewSize the out value returning the size that was set (in case OS can't set that)
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetSendBufferSize(INT Size,INT& NewSize) = 0;
	/**
	 * Sets the size of the receive buffer to use
	 *
	 * @param Size the size to change it to
	 * @param NewSize the out value returning the size that was set (in case OS can't set that)
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetReceiveBufferSize(INT Size,INT& NewSize) = 0;
	/**
	 * Reads the port this socket is bound to.
	 */ 
	virtual INT GetPortNo(void) = 0;
	/**
	 * Fetches the IP address that generated the error
	 *
	 * @param FromAddr the out param getting the address
	 *
	 * @return TRUE if succeeded, FALSE otherwise
	 */
	virtual UBOOL GetErrorOriginatingAddress(FInternetIpAddr& FromAddr) = 0;
	/**
	 * @return The type of protocol the socket is bound to
	 */
	FORCEINLINE ESocketType GetSocketType(void) const
	{
		return SocketType;
	}
	/**
	 * @return The debug description of the socket
	 */
	FORCEINLINE FString GetDescription(void) const
	{
		return SocketDescription;
	}
};

/**
 * Abstract interface used by clients to get async host name resolution to work in a
 * crossplatform way
 */
class FResolveInfo
{
protected:
	/** Hidden on purpose */
	FResolveInfo()
	{
	}

public:
	/** Virtual destructor for child classes to overload */
	virtual ~FResolveInfo()
	{
	}

	/**
	 * Whether the async process has completed or not
	 *
	 * @return true if it completed successfully, false otherwise
	 */
	virtual UBOOL IsComplete(void) const = 0;

	/**
	 * The error that occured when trying to resolve
	 *
	 * @return error code from the operation
	 */
	virtual INT GetErrorCode(void) const = 0;

	/**
	 * Returns a copy of the resolved address
	 *
	 * @return the resolved IP address
	 */
	virtual FInternetIpAddr GetResolvedAddress(void) const = 0;
};


/** A non-async resolve info for returning cached results */
class FResolveInfoCached :
	public FResolveInfo
{
	/** The address that was resolved */
	FInternetIpAddr Addr;

	/** Hidden on purpose */
	FResolveInfoCached();

public:
	/**
	 * Sets the address to return to the caller
	 *
	 * @param InAddr the address that is being cached
	 */
	FResolveInfoCached(const FInternetIpAddr& InAddr) :
		Addr(InAddr)
	{
	}

// FResolveInfo interface

	/**
	 * Whether the async process has completed or not
	 *
	 * @return true if it completed successfully, false otherwise
	 */
	virtual UBOOL IsComplete(void) const
	{
		return TRUE;
	}

	/**
	 * The error that occured when trying to resolve
	 *
	 * @return error code from the operation
	 */
	virtual INT GetErrorCode(void) const
	{
		return 0;
	}

	/**
	 * Returns a copy of the resolved address
	 *
	 * @return the resolved IP address
	 */
	virtual FInternetIpAddr GetResolvedAddress(void) const
	{
		return Addr;
	}
};

/**
 * This is the base interface to abstract platform specific sockets API
 * differences.
 */
class FSocketSubsystem
{
	/** Used to provide threadsafe access to the host name cache */
	FCriticalSection HostNameCacheSync;
	/** Stores a resolved IP address for a given host name */
	TMap<FString,FInternetIpAddr> HostNameCache;

public:
	/**
	 * Does per platform initialization of the sockets library
	 *
	 * @param Error a string that is filled with error information
	 *
	 * @return TRUE if initialized ok, FALSE otherwise
	 */
	virtual UBOOL Initialize(FString& Error) = 0;
	/**
	 * Performs platform specific socket clean up
	 */
	virtual void Destroy(void) = 0;
	/**
	 * Creates a data gram socket
	 *
	 * @param SocketDescription debug description
	 * @param bForceUDP overrides any platform specific protocol with UDP instead
	 *
	 * @return the new socket or NULL if failed
	 */
	virtual FSocket* CreateDGramSocket(const FString& SocketDescription, UBOOL bForceUDP = FALSE) = 0;
	/**
	 * Creates a stream socket
	 *
	 * @param SocketDescription debug description
	 *
	 * @return the new socket or NULL if failed
	 */
	virtual FSocket* CreateStreamSocket(const FString& SocketDescription) = 0;
	/**
	 * Cleans up a socket class
	 *
	 * @param Socket the socket object to destroy
	 */
	virtual void DestroySocket(FSocket* Socket) = 0;
	/**
	 * Does a DNS look up of a host name
	 *
	 * @param HostName the name of the host to look up
	 * @param Addr the address to copy the IP address to
	 */
	virtual INT GetHostByName(ANSICHAR* HostName,FInternetIpAddr& Addr) = 0;
	/**
	 * Creates a platform specific async hostname resolution object
	 *
	 * @param HostName the name of the host to look up
	 *
	 * @return the resolve info to query for the address
	 */
	virtual FResolveInfo* GetHostByName(ANSICHAR* HostName);
	/**
	 * Some platforms require chat data (voice, text, etc.) to be placed into
	 * packets in a special way. This function tells the net connection
	 * whether this is required for this platform
	 */
	virtual UBOOL RequiresChatDataBeSeparate(void)
	{
		return FALSE;
	}
	/**
	 * Some platforms require packets be encrypted. This function tells the
	 * net connection whether this is required for this platform
	 */
	virtual UBOOL RequiresEncryptedPackets(void)
	{
		return FALSE;
	}
	/**
	 * Determines the name of the local machine
	 *
	 * @param HostName the string that receives the data
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL GetHostName(FString& HostName) = 0;
	/**
	 * Uses the platform specific look up to determine the host address
	 *
	 * @param Out the output device to log messages to
	 * @param HostAddr the out param receiving the host address
	 *
	 * @return TRUE if all can be bound (no primarynet), FALSE otherwise
	 */
	virtual UBOOL GetLocalHostAddr(FOutputDevice& Out,FInternetIpAddr& HostAddr) = 0;
	/**
	 * Checks the host name cache for an existing entry (faster than resolving again)
	 *
	 * @param HostName the host name to search for
	 * @param Addr the out param that the IP will be copied to
	 *
	 * @return TRUE if the host was found, FALSE otherwise
	 */
	UBOOL GetHostByNameFromCache(ANSICHAR* HostName,FInternetIpAddr& Addr);
	/**
	 * Stores the ip address with the matching host name
	 *
	 * @param HostName the host name to search for
	 * @param Addr the out param that the IP will be copied to
	 */
	void AddHostNameToCache(ANSICHAR* HostName,const FInternetIpAddr& Addr);
	/**
	 * Removes the host name to ip mapping from the cache
	 *
	 * @param HostName the host name to search for
	 */
	void RemoveHostNameFromCache(ANSICHAR* HostName);
	/**
	 * @return Whether the machine has a properly configured network device or not
	 */
	virtual UBOOL HasNetworkDevice(void)
	{
		return TRUE;
	}
	/**
	 * Determines if the result code is considered an error or not
	 *
	 * @param ResultCode the return code to check
	 *
	 * @return TRUE if the code is an error code, FALSE otherwise
	 */
	virtual UBOOL IsSocketError(INT ResultCode);
	/**
	 * Returns a human readable string from an error code
	 *
	 * @param Code the error code to check
	 */
	virtual const TCHAR* GetSocketError(INT Code = -1) = 0;
	/**
	 * Returns the last error that has happened
	 */
	virtual INT GetLastErrorCode(void) = 0;

	/**
	 * Decode ip addr from platform addr read from buffer.
	 * No processing required as ip addr and platform addr are the same.
	 */
	virtual UBOOL DecodeIpAddr(FIpAddr& ToIpAddr,const TArray<BYTE>& FromPlatformInfo)
	{
		return TRUE;
	}
	/**
	 * Encode ip addr to platform addr and save in buffer
	 * No processing required as ip addr and platform addr are the same.
	 */
	virtual UBOOL EncodeIpAddr(TArray<BYTE>& ToPlatformInfo,const FIpAddr& FromIpAddr)
	{
		return TRUE;
	}
};

/** Global declaration */
extern FSocketSubsystem* GSocketSubsystem;
extern FSocketSubsystem* GSocketSubsystemDebug;

/**
 * Represents a full set of socket data: address in network byte order, port
 * information in host byte order, and the socket connected to it
 */
struct FSocketData
{
	/** Internet ip address in network byte order */
	class FInternetIpAddr Addr;
	/** Port number in host byte order */
	INT Port;
	/** The socket associated with the address */
	class FSocket* Socket;

	/** Default ctor, zeros values */
	FSocketData(void) :
		Port(0),
		Socket(NULL)
	{
	}

	/** Reads the address information from the socket */
	void UpdateFromSocket(void);
	/**
	 * Converts this socket data into string form. Optionally including the port info
	 *
	 * @param bShowPort whether to append the port number or not
	 *
	 * @return A new string object with the ip address data in it
	 */
	FString GetString(UBOOL bShowPort);
};

#endif	//#if WITH_UE3_NETWORKING

#endif
