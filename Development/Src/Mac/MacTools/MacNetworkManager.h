/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _NETWORKMANAGER_H_
#define _NETWORKMANAGER_H_

#include "MacTarget.h"

typedef FReferenceCountPtr<CMacTarget> MacTargetPtr;

/**
 * This class contains all of the network code for interacting with Mac targets.
 */
class FMacNetworkManager : public FConsoleNetworkManager
{
public:
	FMacNetworkManager() {}
	virtual ~FMacNetworkManager() {}

	/**
	 * Creates a new socket
	 */
	inline FMacSocket* CreateSocket( void ) const
	{
		return new FMacSocket(FConsoleSocket::SF_IPv4, FConsoleSocket::ST_Datagram, FConsoleSocket::SP_UDP);
	}

	/**
	 * Creates a new target
	 */
	inline CMacTarget* CreateTarget( const sockaddr_in* InAddress ) const
	{
		return new CMacTarget(InAddress, new FMacSocket(), NULL );
	}

	/**
	 * Makes sure the platform is correct
	 */ 
	inline bool ValidatePlatform( const char* InPlatform ) const
	{ 
		return !( strcmp( InPlatform, "Mac" ) );
	}

	/**
	 * Get the configuration
	 */ 
	inline wstring GetConfiguration( void ) const
	{
		return L"Mac";
	}

	/**
	 * Get the platform
	 */ 
	inline FConsoleSupport::EPlatformType GetPlatform( void ) const
	{
		return FConsoleSupport::EPlatformType_MacOSX;
	}

	/**
	 * Gets an CMacTarget from a TARGETHANDLE
	 */
	CMacTarget* ConvertTarget( const TARGETHANDLE Handle );

	/**
	 * Gets an CIPhoneTarget from a sockaddr_in
	 */
	CMacTarget* ConvertTarget( const sockaddr_in &Address );

	/**
	 * Gets an CMacTarget from a TargetPtr
	 */
	CMacTarget* ConvertTarget( TargetPtr InTarget );

	/**
	 * Initalizes sock and the FConsoleNetworkManager instance.
	 */
	void Initialize();

	/**
	 * Sets up the target with the information it needs
	 */
	void SetupTarget( TargetPtr InTarget, const char* CompName, const char* GameName, const char* GameType );

	/**
	 * Forces a stub target to be created to await connection
	 *
	 * @returns Handle of new stub target
	 */
	TARGETHANDLE ForceAddTarget( const wchar_t* TargetIP );
};

#endif
