/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _SOCKET_H_
#define _SOCKET_H_

#include "Common.h"

/**
 * Wraps a iphone socket.
 */
class FIPhoneSocket : public FConsoleSocket
{
public:
	FIPhoneSocket() {}
	FIPhoneSocket(ESocketFamily SocketFamily, ESocketType SocketType, ESocketProtocol SocketProtocol) 
	 : FConsoleSocket( SocketFamily, SocketType, SocketProtocol ) {}
	virtual ~FIPhoneSocket() {}
};

#endif
