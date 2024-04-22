/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _SOCKET_H_
#define _SOCKET_H_

#include "Common.h"

/**
 * Wraps a mac socket.
 */
class FMacSocket : public FConsoleSocket
{
public:
	FMacSocket() {}
	FMacSocket(ESocketFamily SocketFamily, ESocketType SocketType, ESocketProtocol SocketProtocol)
		: FConsoleSocket( SocketFamily, SocketType, SocketProtocol ) {}
	virtual ~FMacSocket() {}
};

#endif
