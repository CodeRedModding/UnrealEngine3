/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "AndroidSocket.h"

FAndroidSocket::FAndroidSocket()
: IOCPHandle(NULL)
{
}

FAndroidSocket::FAndroidSocket(ESocketFamily SocketFamily, ESocketType SocketType, ESocketProtocol SocketProtocol)
: FConsoleSocket( SocketFamily, SocketType, SocketProtocol )
, IOCPHandle(NULL)
{
}

FAndroidSocket::~FAndroidSocket()
{
}

/**
 * Sets the broadcasting state of the socket.
 *
 * @param	bEnable	True if the socket is set to broadcast.
 * @return	True if the operation succeeds.
 */
bool FAndroidSocket::SetBroadcasting(bool bEnable)
{
	static const unsigned int BroadcastIP = inet_addr("255.255.255.255");

	if(bEnable)
	{
		Address.sin_addr.s_addr = BroadcastIP;
	}

	return FConsoleSocket::SetBroadcasting(bEnable);
}

/**
 * Closes the socket.
 */
void FAndroidSocket::Close()
{
	if (IOCPHandle)
	{
//	CloseHandle(IOCPHandle);
		IOCPHandle = NULL;
	}

	FConsoleSocket::Close();
}

/**
 * Associates the socket with an IO completion port.
 *
 * @param	IOCP	The IO completion port handle.
 * @return	True if the operation succeeds.
 */
bool FAndroidSocket::AssociateWithIOCP(HANDLE IOCP)
{
	if(IsValid() && IOCP && IOCP != INVALID_HANDLE_VALUE)
	{
		IOCPHandle = CreateIoCompletionPort((HANDLE)Socket, IOCP, NULL, 0);
		return IOCPHandle != NULL;
	}

	return false;
}
