/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "WindowsTarget.h"

///////////////////////CWindowsTarget/////////////////////////////////

CWindowsTarget::CWindowsTarget(const sockaddr_in* InRemoteAddress, FWindowsSocket* InTCPClient, FWindowsSocket* InUDPClient)
 : CTarget( InRemoteAddress, InTCPClient, InUDPClient )
{
}

CWindowsTarget::~CWindowsTarget()
{
}