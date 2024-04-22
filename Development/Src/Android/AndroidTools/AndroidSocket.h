/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _SOCKET_H_
#define _SOCKET_H_

#include "Common.h"

/**
 * Wraps a iphone socket.
 */
class FAndroidSocket : public FConsoleSocket
{
public:
	FAndroidSocket();
	FAndroidSocket(ESocketFamily SocketFamily, ESocketType SocketType, ESocketProtocol SocketProtocol);
	virtual ~FAndroidSocket();

	/**
	 * Sets the broadcasting state of the socket.
	 *
	 * @param	bEnable	True if the socket is set to broadcast.
	 * @return	True if the operation succeeds.
	 */
	bool SetBroadcasting(bool bEnable);

	/**
	 * Closes the socket.
	 */
	void Close();

	/**
	 * Associates the socket with an IO completion port.
	 *
	 * @param	IOCP	The IO completion port handle.
	 * @return	True if the operation succeeds.
	 */
	bool AssociateWithIOCP(HANDLE IOCP);

	inline bool IsAssociatedWithIOCP() const { return IOCPHandle != NULL; }

private:
	HANDLE IOCPHandle;
};

#endif
