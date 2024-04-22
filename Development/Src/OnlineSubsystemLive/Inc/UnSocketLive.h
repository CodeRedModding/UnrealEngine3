/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __UNSOCKETLIVE_H__
#define __UNSOCKETLIVE_H__

// Include the base class for our xenon subsystem
#include "UnSocketWin.h"

#if WITH_UE3_NETWORKING
#if !WITH_PANORAMA

/**
 * This is the Xbox Live specific socket class
 */
class FSocketLive : public FSocketWin
{
	/** Current active ips referenced during connect/bind that have not been closed */
	TArray<FInternetIpAddr> UsedAddrs;	
	
public:
	/**
	 * Assigns a Xbox Live socket to this object
	 *
	 * @param InSocket the socket to assign to this object
	 * @param InSocketType the type of socket that was created
	 * @param InSocketDescription the debug description of the socket
	 */
	FSocketLive(SOCKET InSocket,ESocketType InSocketType,const FString& InSocketDescription) 
	:	FSocketWin(InSocket,InSocketType,InSocketDescription)
	{
	}
	
	/** Closes the socket if it is still open */
	virtual ~FSocketLive(void)
	{
		Close();
	}

	/**
	 * Closes the socket
	 *
	 * @param TRUE if it closes without errors, FALSE otherwise
	 */
	virtual UBOOL Close(void);
	/**
	 * Binds a socket to a network byte ordered address
	 *
	 * @param Addr the address to bind to
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL Bind(const FInternetIpAddr& Addr);
	/**
	 * Connects a socket to a network byte ordered address
	 *
	 * @param Addr the address to connect to
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL Connect(const FInternetIpAddr& Addr);
};

/**
 * Xenon specific socket subsystem implementation
 */
class FSocketSubsystemLive :
	public FSocketSubsystemWindows
{
	/** Whether to create dgram sockets using VDP or not */
	UBOOL bUseVDP;
	/** Whether to enable the secure libs or not */
	UBOOL bUseSecureConnections;
	/** Whether to use the LSP enumeration or not */
	UBOOL bUseLspEnumerate;
	/** The LSP service id provided by the Live team */
	INT ServiceId;	

public:
	/** Zeroes members */
	FSocketSubsystemLive(void) :
		FSocketSubsystemWindows(),
		bUseVDP(TRUE),
		bUseSecureConnections(FALSE),
		bUseLspEnumerate(FALSE),
		ServiceId(0)
	{
	}

	/**
	 * Does Xenon platform initialization of the sockets library
	 *
	 * @param Error a string that is filled with error information
	 *
	 * @return TRUE if initialized ok, FALSE otherwise
	 */
	virtual UBOOL Initialize(FString& Error);
	/**
	 * Performs Xenon & Live specific socket subsystem clean up
	 */
	virtual void Destroy(void);
	/**
	 * Creates a data gram socket
	 *
 	 * @param SocketDescription debug description
	 * @param bForceUDP overrides any platform specific protocol with UDP instead
	 *
	 * @return the new socket or NULL if failed
	 */
	virtual FSocket* CreateDGramSocket(const FString& SocketDescription, UBOOL bForceUDP = FALSE);
	/**
	 * Creates a stream socket
	 *
	 * @param SocketDescription debug description
	 *
	 * @return the new socket or NULL if failed
	 */
	virtual FSocket* CreateStreamSocket(const FString& SocketDescription);
	/**
	 * Does a DNS look up of a host name
	 *
	 * @param HostName the name of the host to look up
	 * @param Addr the address to copy the IP address to
	 */
	virtual INT GetHostByName(ANSICHAR* HostName,FInternetIpAddr& Addr);
	/**
	 * Creates a platform specific async hostname resolution object
	 *
	 * @param HostName the name of the host to look up
	 *
	 * @return the resolve info to query for the address
	 */
	virtual FResolveInfo* GetHostByName(ANSICHAR* HostName);
	/**
	 * Live requires chat data (voice, text, etc.) to be placed into
	 * packets after game data (unencrypted). Use the VDP setting to
	 * control this
	 */
	virtual UBOOL RequiresChatDataBeSeparate(void)
	{
		return bUseVDP;
	}
	/**
	 * Some platforms require packets be encrypted. This function tells the
	 * net connection whether this is required for this platform
	 */
	virtual UBOOL RequiresEncryptedPackets(void)
	{
		return bUseSecureConnections;
	}
	/**
	 * Determines the name of the Xenon
	 *
	 * @param HostName the string that receives the data
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL GetHostName(FString& HostName);
	/**
	 * Uses the secure libs to look up the host address
	 *
	 * @param Out the output device to log messages to
	 * @param HostAddr the out param receiving the host address
	 *
	 * @return always TRUE
	 */
	virtual UBOOL GetLocalHostAddr(FOutputDevice& Out,FInternetIpAddr& HostAddr);

	/**
	 * Decode ip addr from platform addr read from buffer
	 *
	 * @param ToIpAddr dest ip addr result to decode platform addr to
	 * @param FromPlatformInfo byte array containing platform addr
	 * @return TRUE if platform addr was successfully decoded to ip addr
	 */
	virtual UBOOL DecodeIpAddr(FIpAddr& ToIpAddr,const TArray<BYTE>& FromPlatformInfo);
	/**
	 * Encode ip addr to platform addr and save in buffer
	 *
	 * @param ToPlatformInfo byte array containing platform addr result
	 * @param FromIpAddr source ip addr to encode platform addr from
	 * @return TRUE if platform addr was successfully encoded from ip addr
	 */
	virtual UBOOL EncodeIpAddr(TArray<BYTE>& ToPlatformInfo,const FIpAddr& FromIpAddr);
};

#endif	//#if !WITH_PANORAMA

#endif	//#if WITH_UE3_NETWORKING

#endif
