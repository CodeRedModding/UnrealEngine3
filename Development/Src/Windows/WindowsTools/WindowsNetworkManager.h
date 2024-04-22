/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _NETWORKMANAGER_H_
#define _NETWORKMANAGER_H_

#include "WindowsTarget.h"

/**
 * This class contains all of the network code for interacting with windows targets.
 */
class FWindowsNetworkManager : public FConsoleNetworkManager
{
public:
	FWindowsNetworkManager() {}
	virtual ~FWindowsNetworkManager() {}

	/**
	 * Creates a new socket
	 */
	inline FWindowsSocket* CreateSocket( void ) const
	{
		return new FWindowsSocket(FConsoleSocket::SF_IPv4, FConsoleSocket::ST_Datagram, FConsoleSocket::SP_UDP);
	}

	/**
	 * Creates a new target
	 */
	inline CWindowsTarget* CreateTarget( const sockaddr_in* InAddress ) const
	{
		return new CWindowsTarget(InAddress, new FWindowsSocket(), NULL );
	}

	/**
	 * Makes sure the platform is correct
	 */ 
	inline bool ValidatePlatform( const char* InPlatform ) const
	{ 
		return !( strcmp( InPlatform, "PC" ) && strcmp( InPlatform, "PCServer" ) && strcmp( InPlatform, "PCConsole" ) );
	}

	/**
	 * Get the configuration
	 */ 
	inline wstring GetConfiguration( void ) const
	{
		return L"PC";
	}

	/**
	 * Get the platform
	 */ 
	inline FConsoleSupport::EPlatformType GetPlatform( void ) const
	{
		return FConsoleSupport::EPlatformType_Windows;
	}
};

#endif
