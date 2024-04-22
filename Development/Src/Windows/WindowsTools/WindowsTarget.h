/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _WINDOWSTARGET_H_
#define _WINDOWSTARGET_H_

#include "WindowsSocket.h"

/// Representation of a single UE3 instance running on PC
class CWindowsTarget : public CTarget
{
public:
	CWindowsTarget(const sockaddr_in* InRemoteAddress, FWindowsSocket* InTCPClient, FWindowsSocket* InUDPClient);
	virtual ~CWindowsTarget();
};

#endif
