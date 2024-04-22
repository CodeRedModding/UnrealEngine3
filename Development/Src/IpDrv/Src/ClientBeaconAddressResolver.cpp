/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(UClientBeaconAddressResolver);

/**
 * Performs platform specific resolution of the address
 *
 * @param DesiredHost the host to resolve the IP address for
 * @param Addr out param having it's address set
 *
 * @return true if the address could be resolved, false otherwise
 */
UBOOL UClientBeaconAddressResolver::ResolveAddress(const FOnlineGameSearchResult& DesiredHost,FInternetIpAddr& Addr)
{
	// Use the session information to build the address
	FSessionInfo* SessionInfo = (FSessionInfo*)DesiredHost.PlatformData;
	if (SessionInfo != NULL)
	{
		// Copy the destination IP
		Addr = SessionInfo->HostAddr;
		// Set to the configured port rather than what's in the address
		Addr.SetPort(BeaconPort);
		return TRUE;
	}
	return FALSE;
}

#endif