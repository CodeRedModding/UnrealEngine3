/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _NETWORKMANAGER_H_
#define _NETWORKMANAGER_H_

#include "IPhoneTarget.h"

typedef FReferenceCountPtr<CIPhoneTarget> IPhoneTargetPtr;

/**
 * This class contains all of the network code for interacting with iphone targets.
 */
class FIPhoneNetworkManager : public FConsoleNetworkManager
{
public:
	FIPhoneNetworkManager() {}
	virtual ~FIPhoneNetworkManager() {}

	/**
	 * Creates a new socket
	 */
	inline FIPhoneSocket* CreateSocket( void ) const
	{
		return new FIPhoneSocket(FConsoleSocket::SF_IPv4, FConsoleSocket::ST_Datagram, FConsoleSocket::SP_UDP);
	}

	/**
	 * Creates a new target
	 */
	inline CIPhoneTarget* CreateTarget( const sockaddr_in* InAddress ) const
	{
		return new CIPhoneTarget(InAddress, new FIPhoneSocket(), NULL );
	}

	/**
	 * Makes sure the platform is correct
	 */ 
	inline bool ValidatePlatform( const char* InPlatform ) const
	{ 
		return !( strcmp( InPlatform, "IPhone" ) );
	}

	/**
	 * Get the configuration
	 */ 
	inline wstring GetConfiguration( void ) const
	{
		return L"iThingConnectedDevice";
	}

	/**
	 * Get the platform
	 */ 
	inline FConsoleSupport::EPlatformType GetPlatform( void ) const
	{
		return FConsoleSupport::EPlatformType_IPhone;
	}

	/**
	 * Gets an CIPhoneTarget from a TARGETHANDLE
	 */
	CIPhoneTarget* ConvertTarget( const TARGETHANDLE Handle );

	/**
	 * Gets an CIPhoneTarget from a sockaddr_in
	 */
	CIPhoneTarget* ConvertTarget( const sockaddr_in &Address );

	/**
	 * Gets an CIPhoneTarget from a TargetPtr
	 */
	CIPhoneTarget* ConvertTarget( TargetPtr InTarget );

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
