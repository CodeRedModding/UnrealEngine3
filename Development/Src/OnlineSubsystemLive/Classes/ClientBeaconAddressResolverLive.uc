/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This is the Live specific version of beacon address resolver. It handles registering &
 * unregistering of secure keys so that communication with a potential host is
 * possible
 */
class ClientBeaconAddressResolverLive extends ClientBeaconAddressResolver
	native;

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
	virtual UBOOL RegisterAddress(const FOnlineGameSearchResult& DesiredHost);

	/**
	 * Allows for per platform unregistration of secure keys, which breaks the link between
	 * a client and server. This also releases any memory associated with the keys.
	 *
	 * @param DesiredHost the host that is being registered
	 */
	virtual UBOOL UnregisterAddress(const FOnlineGameSearchResult& DesiredHost);
}
