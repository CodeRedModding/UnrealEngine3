/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Allows a client to register and resolve the address for a host that it wants to connect to.
 * The platform specific implementation for this resolver will handle the specifics of generating 
 * a secure key to allow for a connection.
 */
class ClientBeaconAddressResolver extends Object
	native
	config(Engine);

/** The port that the beacon will listen on */
var int BeaconPort;

/** The name to use when logging (helps debugging) */
var name BeaconName;

cpptext
{
	/**
	 * Performs platform specific resolution of the address
	 *
	 * @param DesiredHost the host to resolve the IP address for
	 * @param Addr out param having it's address set
	 *
	 * @return true if the address could be resolved, false otherwise
	 */
	virtual UBOOL ResolveAddress(const FOnlineGameSearchResult& DesiredHost,FInternetIpAddr& Addr);

	/**
	 * Allows for per platform registration of secure keys, so that a secure connection
	 * can be opened and used for sending/receiving data.
	 *
	 * @param DesiredHost the host that is being registered
	 */
	virtual UBOOL RegisterAddress(const FOnlineGameSearchResult& DesiredHost)
	{
		return TRUE;
	}

	/**
	 * Allows for per platform unregistration of secure keys, which breaks the link between
	 * a client and server. This also releases any memory associated with the keys.
	 *
	 * @param DesiredHost the host that is being registered
	 */
	virtual UBOOL UnregisterAddress(const FOnlineGameSearchResult& DesiredHost)
	{
		return TRUE;
	}
}
