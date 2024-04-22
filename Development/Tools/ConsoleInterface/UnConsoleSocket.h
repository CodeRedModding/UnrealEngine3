// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#ifndef _UNCONSOLESOCKET_H_
#define _UNCONSOLESOCKET_H_

// don't include this on consoles
#if !CONSOLE && !PLATFORM_MACOSX && WITH_UE3_NETWORKING

#pragma pack(push,8)

#include "UnConsoleNetwork.h"

#pragma warning(push)
#pragma warning(disable : 4127) // Conditional expression is constant

/**
 * Wraps a socket.
 */
class FConsoleSocket
{
public:
	enum ESocketFamily
	{
		SF_Unspecified = AF_UNSPEC,
		SF_IPv4 = AF_INET,
		SF_IPv6 = AF_INET6,
		SF_Infrared = AF_IRDA,
		SF_Bluetooth = 32,	// AF_BTH,
	};

	enum ESocketType
	{
		ST_Invalid = 0,
		ST_Stream = SOCK_STREAM,
		ST_Datagram = SOCK_DGRAM,
		ST_Raw = SOCK_RAW,
		ST_ReliableDatagram = SOCK_RDM,
		ST_PsuedoStreamDatagram = SOCK_SEQPACKET,
	};

	enum ESocketProtocol
	{
		SP_Invalid = 0,
		SP_TCP = IPPROTO_TCP,
		SP_UDP = IPPROTO_UDP,
		SP_ReliableMulticast = 113, // IPPROTO_PGM,
	};

	enum ESocketFlags
	{
		SF_None = 0,
		SF_Peek = MSG_PEEK,
		SF_OutOfBand = MSG_OOB,
		SF_WaitAll = MSG_WAITALL,
		SF_DontRoute = MSG_DONTROUTE,
		SF_Partial = MSG_PARTIAL,
	};

	FConsoleSocket()
		: Socket(INVALID_SOCKET)
	{
		ZeroMemory(&Address, sizeof(Address));
		ZeroMemory(&ProtocolInfo, sizeof(ProtocolInfo));
	}

	FConsoleSocket(ESocketFamily SocketFamily, ESocketType SocketType, ESocketProtocol SocketProtocol)
		: Socket(INVALID_SOCKET)
	{
		ZeroMemory(&Address, sizeof(Address));
		ZeroMemory(&ProtocolInfo, sizeof(ProtocolInfo));

		SetAttributes(SocketFamily, SocketType, SocketProtocol);
	}

	virtual ~FConsoleSocket()
	{
		Close();
	}

	/**
	 * Wrapper for the sock recv().
	 *
	 * @param	Buffer		Buffer to save the recv'd data into.
	 * @param	BufLength	The size of Buffer.
	 * @param	Flags		Flags controlling the operation.
	 * @return	SOCKET_ERROR if the operation failed, otherwise the number of bytes recv'd.
	 */
	virtual int Recv(char* Buffer, int BufLength, ESocketFlags Flags = SF_None)
	{
		return recv(Socket, Buffer, BufLength, (int)Flags);
	}

	/**
	 * Wrapper for the sock recvfrom().
	 *
	 * @param	Buffer		Buffer to save the recv'd data into.
	 * @param	BufLength	The size of Buffer.
	 * @param	FromAddress	The address the data was recv'd from.
	 * @param	Flags		Flags controlling the operation.
	 * @return	SOCKET_ERROR if the operation failed, otherwise the number of bytes recv'd.
	 */
	virtual int RecvFrom(char* Buffer, int BufLength, sockaddr_in &FromAddress, ESocketFlags Flags = SF_None)
	{
		int FromLen = sizeof(sockaddr_in);
		return recvfrom(Socket, Buffer, BufLength, (int)Flags, (sockaddr*)&FromAddress, &FromLen);
	}

	/**
	 * Begins an asynchronous recv() operation.
	 *
	 * @param	Buffers		Array of buffers for the incoming data.
	 * @param	BufferCount	The number of buffers.
	 * @param	BytesRecvd	Receives the number of bytes written to the buffers.
	 * @param	EventArgs	Information about the event.
	 * @param	Flags		Flags controlling the operation.
	 * @return	True if the operation succeeded.
	 */
	virtual bool RecvAsync(LPWSABUF Buffers, DWORD BufferCount, DWORD &BytesRecvd, WSAOVERLAPPED *EventArgs, ESocketFlags Flags = SF_None)
	{
		bool ret = WSARecv(Socket, Buffers, BufferCount, &BytesRecvd, (LPDWORD)&Flags, EventArgs, NULL) != SOCKET_ERROR;

		if(!ret && WSAGetLastError() == WSA_IO_PENDING)
		{
			ret = true;
		}

		return ret;
	}

	/**
	 * Wrapper for the sock send().
	 *
	 * @param	Buffer		Buffer to save the recv'd data into.
	 * @param	BufLength	The size of Buffer.
	 * @param	Flags		Flags controlling the operation.
	 * @return	SOCKET_ERROR if the operation failed, otherwise the number of bytes sent.
	 */
	virtual int Send(const char* Buffer, int BufLength, ESocketFlags Flags = SF_None)
	{
		return send(Socket, Buffer, BufLength, (int)Flags);
	}

	/**
	 * Wrapper for the sock sendto().
	 *
	 * @param	Buffer		Buffer to save the recv'd data into.
	 * @param	BufLength	The size of Buffer.
	 * @param	Flags		Flags controlling the operation.
	 * @return	SOCKET_ERROR if the operation failed, otherwise the number of bytes sent.
	 */
	virtual int SendTo(const char* Buffer, int BufLength, ESocketFlags Flags = SF_None)
	{
		return sendto(Socket, Buffer, BufLength, (int)Flags, (sockaddr*)&Address, sizeof(Address));
	}

	/**
	 * Creates a new internal socket.
	 *
	 * @return	True if the operation succeeds.
	 */
	virtual bool CreateSocket()
	{
		Close();

		return (Socket = WSASocket(ProtocolInfo.iAddressFamily, ProtocolInfo.iSocketType, ProtocolInfo.iProtocol, NULL, NULL, WSA_FLAG_OVERLAPPED)) != INVALID_SOCKET;
	}

	/**
	 * Sets attributes used when creating a new socket via CreateSocket().
	 *
	 * @param	SocketFamily	The family of the socket.
	 * @param	SocketType		The type of socket to be created.
	 * @param	SocketProtocol	The protocol of the socket.
	 */
	virtual void SetAttributes(ESocketFamily SocketFamily, ESocketType SocketType, ESocketProtocol SocketProtocol)
	{
		ProtocolInfo.iAddressFamily = (int)SocketFamily;
		ProtocolInfo.iSocketType = (int)SocketType;
		ProtocolInfo.iProtocol = (int)SocketProtocol;
		Address.sin_family = (short)SocketFamily;
	}

	/**
	 * Closes the socket.
	 */
	virtual void Close()
	{
		if(IsValid())
		{
			closesocket(Socket);
			Socket = INVALID_SOCKET;
		}
	}

	/**
	 * Sets the blocking state of the socket.
	 *
	 * @param	IsNonBlocking	True if the socket is set to non-blocking.
	 * @return	True if the operation succeeds.
	 */
	virtual bool SetNonBlocking(bool IsNonBlocking)
	{
		u_long arg = IsNonBlocking ? 1 : 0;
		return ioctlsocket(Socket, FIONBIO, &arg) == 0;
	}
	
	/**
	 * Sets the broadcasting state of the socket.
	 *
	 * @param	bEnable	True if the socket is set to broadcast.
	 * @return	True if the operation succeeds.
	 */
	virtual bool SetBroadcasting(bool bEnable)
	{
		INT bEnableBroadcast = bEnable ? TRUE : FALSE;
		return setsockopt(Socket, SOL_SOCKET, SO_BROADCAST, (const char*)&bEnableBroadcast, sizeof(bEnableBroadcast)) == 0;
	}

	/**
	 * Sets whether a socket can be bound to an address in use
	 *
	 * @param bEnable whether to allow reuse or not
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual bool SetReuseAddr(bool bEnable)
	{
		INT bAllowReuse = bEnable ? TRUE : FALSE;
		return setsockopt(Socket,SOL_SOCKET,SO_REUSEADDR,(char*)&bAllowReuse,sizeof(bAllowReuse)) == 0;
	}

	/**
	 * Binds the socket to the current address.
	 *
	 * @return	True if the operation succeeds.
	 */
	virtual bool Bind()
	{
		return bind(Socket, (sockaddr*)&Address, sizeof(Address)) != SOCKET_ERROR;
	}

	/**
	 * Wraps the sock connect() by connecting to the current address.
	 *
	 * @return	True if the operation succeeds.
	 */
	virtual bool Connect()
	{
		int Error = connect(Socket, (sockaddr*)&Address, sizeof(Address));
		return Error == 0;
	}
	
	// functions for checking state
	inline virtual bool IsValid() const { return Socket != INVALID_SOCKET; }
	
	/**
	 * @return TRUE if the other end of socket has gone away
	 */
	virtual bool IsBroken()
	{
		// Check and return without waiting
		TIMEVAL Time = {0,0};
		fd_set SocketSet;
		// Set up the socket sets we are interested in (just this one)
		FD_ZERO(&SocketSet);
		FD_SET(Socket,&SocketSet);
		// Check the status of the bits for errors
		INT SelectStatus = select(0,NULL,NULL,&SocketSet,&Time);
		if (SelectStatus == 0)
		{
			FD_ZERO(&SocketSet);
			FD_SET(Socket,&SocketSet);
			// Now check to see if it's connected (writable means connected)
			INT WriteSelectStatus = select(0,NULL,&SocketSet,NULL,&Time);
			INT ReadSelectStatus = 1;//select(0,&SocketSet,NULL,NULL,&Time);
			if (WriteSelectStatus > 0 && ReadSelectStatus > 0)
			{
				return FALSE;
			}
		}
		// any other cases are busted socket
		return TRUE;
	}

	/**
 	 * @return TRUE if there's an error or read ready to go
	 */
	virtual bool IsReadReady()
	{
		// Check and return without waiting
		TIMEVAL Time = {0,0};
		fd_set ReadSet;
		FD_ZERO(&ReadSet);
		FD_SET(Socket,&ReadSet);
		fd_set ErrorSet;
		FD_ZERO(&ErrorSet);
		FD_SET(Socket,&ErrorSet);

		// Check the status of the bits for errors
		INT SelectStatus = select(0,&ReadSet,NULL,&ErrorSet,&Time);
		if (SelectStatus == 0)
		{
			// nothing to see here, move along
			return FALSE;
		}

		// either an error or a read (or select failed)
		// in any case, do a recv and see if we get an error
		return TRUE;
	}

	//mutators
	inline virtual void SetAddress(const sockaddr_in Addr) { Address = Addr; }
	inline virtual void SetAddress(const u_long Addr) { Address.sin_addr.s_addr = Addr; }
	inline virtual void SetAddress(const char* Addr) { Address.sin_addr.s_addr = inet_addr(Addr); }
	inline virtual void SetPort(const u_short Port) { Address.sin_port = htons(Port); }

	//accessors
	inline virtual u_short GetPort() const { return ntohs(Address.sin_port); }
	inline virtual u_long GetIP() const { return Address.sin_addr.s_addr; }
	inline virtual ESocketProtocol GetProtocol() const { return (ESocketProtocol)ProtocolInfo.iProtocol; }
	inline virtual ESocketFamily GetFamily() const { return (ESocketFamily)ProtocolInfo.iAddressFamily; }
	inline virtual ESocketType GetType() const { return (ESocketType)ProtocolInfo.iSocketType; }

	//operator overloads
	inline operator bool() const { return IsValid(); }
	inline operator SOCKET() const { return Socket; }

protected:
	/** Socket used. */
	SOCKET Socket;
	/** IP we're connecting to. */
	sockaddr_in Address;
	WSAPROTOCOL_INFO ProtocolInfo;
};

#pragma warning(pop)

#pragma pack( pop )

#endif // !CONSOLE && !PLATFORM_MACOSX && WITH_UE3_NETWORKING

#endif // _UNCONSOLESOCKET_H_
