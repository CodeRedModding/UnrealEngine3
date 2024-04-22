/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _SOCKET_H_
#define _SOCKET_H_

#include "Common.h"

/**
 * Wraps a windows socket.
 */
class FWindowsSocket : public FConsoleSocket
{
public:
	FWindowsSocket() {}
	FWindowsSocket(ESocketFamily SocketFamily, ESocketType SocketType, ESocketProtocol SocketProtocol)
		: FConsoleSocket( SocketFamily, SocketType, SocketProtocol ) {}
	virtual ~FWindowsSocket() {}
};

#endif
