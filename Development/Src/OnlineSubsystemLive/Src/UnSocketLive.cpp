/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemLive.h"

#if WITH_UE3_NETWORKING && !WITH_PANORAMA

FSocketSubsystemLive SocketSubsystem;

/**
 * Starts up the socket subsystem 
 *
 * @param bIsEarlyInit If TRUE, this function is being called before GCmdLine, GFileManager, GConfig, etc are setup. If FALSE, they have been initialized properly
 */
void appSocketInit(UBOOL bIsEarlyInit)
{
	// FSocketSubsystemLive::Initialize needs GConfig, so we can't initialize early
	if (bIsEarlyInit)
	{
		return;
	}

	GSocketSubsystem = &SocketSubsystem;
	GSocketSubsystemDebug = &SocketSubsystem;
	FString Error;
	if (GSocketSubsystem->Initialize(Error) == FALSE)
	{
		debugf(NAME_Init,TEXT("Failed to initialize socket subsystem: (%s)"),*Error);
	}
}

/**
 * Closes the socket
 *
 * @param TRUE if it closes without errors, FALSE otherwise
 */
UBOOL FSocketLive::Close(void)
{
	UBOOL Result = FSocketWin::Close();
	
	// release cached secure addrs
	for (INT AddrIdx=0; AddrIdx < UsedAddrs.Num(); AddrIdx++)
	{
		const FInternetIpAddr& Addr = UsedAddrs(AddrIdx);
		FSecureAddressCache::GetCache().ReleaseSecureIpAddress(Addr);
	}
	UsedAddrs.Empty();
	
	return Result;
}

/**
 * Binds a socket to a network byte ordered address
 *
 * @param Addr the address to bind to
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketLive::Bind(const FInternetIpAddr& Addr)
{
	// convert addr to secure addr for bind
	FInternetIpAddr SecureAddr(Addr);
	FSecureAddressCache::GetCache().GetSecureIpAddress(SecureAddr,Addr);
	UsedAddrs.AddItem(Addr);
	
	return FSocketWin::Bind(SecureAddr);
}

/**
 * Connects a socket to a network byte ordered address
 *
 * @param Addr the address to connect to
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketLive::Connect(const FInternetIpAddr& Addr)
{
	// convert addr to secure ip for connection
	FInternetIpAddr SecureAddr(Addr);
	FSecureAddressCache::GetCache().GetSecureIpAddress(SecureAddr,Addr);
	UsedAddrs.AddItem(Addr);
	
	return FSocketWin::Connect(SecureAddr);
}

/**
 * Does Xenon platform initialization of the sockets library
 *
 * @param Error a string that is filled with error information
 *
 * @return TRUE if initialized ok, FALSE otherwise
 */
UBOOL FSocketSubsystemLive::Initialize(FString& Error)
{
	if (bTriedToInit == FALSE)
	{
		bTriedToInit = TRUE;
		// Determine if we should use VDP or not
#if	!FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		if (GConfig != NULL)
		{
			// Figure out if VDP is enabled or not
			GConfig->GetBool(SOCKET_API,TEXT("bUseVDP"),bUseVDP,GEngineIni);
			// Figure out if secure connection is enabled or not
			GConfig->GetBool(SOCKET_API,TEXT("bUseSecureConnections"),bUseSecureConnections,GEngineIni);
			// Figure out if LSP enumeration is enabled or not
			GConfig->GetBool(SOCKET_API,TEXT("bUseLspEnumerate"),bUseLspEnumerate,GEngineIni);
			// Allow disabling secure connections via commandline override.
			if( ParseParam(appCmdLine(), TEXT("DEVCON")) )
			{
				bUseSecureConnections = FALSE;
			}
		}
#else
		#pragma message("Forcing VDP packets to on")
		// Force encryption and voice packets being appended without it
		bUseVDP = TRUE;
		bUseSecureConnections = TRUE;
#endif
		INT SystemLinkPort = 14000;
		GConfig->GetInt(SOCKET_API,TEXT("SystemLinkPort"),SystemLinkPort,GEngineIni);
		// Set the system link port so Live doesn't block it
		INT ErrorCode = XNetSetSystemLinkPort(XSocketHTONS((WORD)SystemLinkPort));
		if (ErrorCode != 0)
		{
			debugf(NAME_DevOnline,
				TEXT("XNetSetSystemLinkPort(%d) returned 0x%08X"),
				SystemLinkPort,
				ErrorCode);
		}
		XNetStartupParams XNParams;
		appMemzero(&XNParams,sizeof(XNetStartupParams));
		XNParams.cfgSizeOfStruct = sizeof(XNetStartupParams);
#if	(!FINAL_RELEASE && !WITH_PANORAMA) || FINAL_RELEASE_DEBUGCONSOLE 
		// The final library should enforce this, but to be sure...
		if (bUseSecureConnections == FALSE)
		{
			XNParams.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
		}
#else
		#pragma message("Using secure socket layer")
		#pragma message("This means a Live client cannot talk to a non-Live server/client that isn't using LSP")
#endif
		// Don't cause runtime allocations
		XNParams.cfgFlags |= XNET_STARTUP_ALLOCATE_MAX_DGRAM_SOCKETS |
			XNET_STARTUP_ALLOCATE_MAX_STREAM_SOCKETS;
		if (GConfig != NULL)
		{
			// Read the values from the INI file
			INT Value;
			if (GConfig->GetInt(SOCKET_API,TEXT("MaxDgramSockets"),Value,GEngineIni))
			{
				XNParams.cfgSockMaxDgramSockets = (BYTE)Clamp(Value,0,255);
			}
			if (GConfig->GetInt(SOCKET_API,TEXT("MaxStreamSockets"),Value,GEngineIni))
			{
				XNParams.cfgSockMaxStreamSockets = (BYTE)Clamp(Value,0,255);
			}
			if (GConfig->GetInt(SOCKET_API,TEXT("DefaultRecvBufsizeInK"),Value,GEngineIni))
			{
				XNParams.cfgSockDefaultRecvBufsizeInK = (BYTE)Clamp(Value,0,255);
			}
			if (GConfig->GetInt(SOCKET_API,TEXT("DefaultSendBufsizeInK"),Value,GEngineIni))
			{
				XNParams.cfgSockDefaultSendBufsizeInK = (BYTE)Clamp(Value,0,255);
			}
			if (GConfig->GetInt(SOCKET_API,TEXT("DefaultQosProbeRetries"),Value,GEngineIni))
			{
				XNParams.cfgQosProbeRetries = (BYTE)Clamp(Value,1,255);
			}
			if (GConfig->GetInt(SOCKET_API,TEXT("DefaultQosMaxSimultaneousResponses"),Value,GEngineIni))
			{
				XNParams.cfgQosSrvMaxSimultaneousResponses = (BYTE)Clamp(Value,1,255);
			}
			// Read the service id to use for LSP access
			GConfig->GetInt(SOCKET_API,TEXT("ServiceId"),ServiceId,GEngineIni);
		}
		// Do the Xenon preinitialization
		ErrorCode = XNetStartup(&XNParams);
		if (ErrorCode == 0)
		{
			WSADATA WSAData;
			// Init WinSock
			if (WSAStartup(0x0101,&WSAData) == 0)
			{
				// Now init Live
				DWORD LiveInit = XOnlineStartup();
				if (LiveInit == ERROR_SUCCESS)
				{
					GIpDrvInitialized = TRUE;
					debugf(NAME_Init,
						TEXT("%s: version %i.%i (%i.%i), MaxSocks=%i, MaxUdp=%i"),
						SOCKET_API,
						WSAData.wVersion >> 8,WSAData.wVersion & 0xFF,
						WSAData.wHighVersion >> 8,WSAData.wHighVersion & 0xFF,
						WSAData.iMaxSockets,WSAData.iMaxUdpDg);
				}
				else
				{
					Error = FString::Printf(TEXT("XOnlineStartup() failed with 0x%X"),LiveInit);
					debugf(NAME_Init,*Error);
				}
			}
		}
		else
		{
			Error = FString::Printf(TEXT("XNetStartup() failed with 0x%X"),GetLastError());
			debugf(NAME_Init,*Error);
		}
	}
	return GIpDrvInitialized;
}

/**
 * Performs Xenon & Live specific socket subsystem clean up
 */
void FSocketSubsystemLive::Destroy(void)
{
	FSecureAddressCache::GetCache().Cleanup();
	// Shut Live down first
	XOnlineCleanup();
	// Then the sockets layer
	WSACleanup();
	// Finally the secure layer
	XNetCleanup();
	GIpDrvInitialized = FALSE;
}

/**
 * Creates a data gram socket. Creates either a UDP or VDP socket based
 * upon the INI configuration
 *
 * @param SocketDescription debug description
 * @param bForceUDP overrides any platform specific protocol with UDP instead
 *
 * @return the new socket or NULL if failed
 */
FSocket* FSocketSubsystemLive::CreateDGramSocket(const FString& SocketDescription, UBOOL bForceUDP)
{
	INT Protocol = bUseVDP == TRUE && bForceUDP == FALSE ? IPPROTO_VDP : IPPROTO_UDP;
	// Create a socket with the configured protocol support
	SOCKET Socket = socket(AF_INET,SOCK_DGRAM,Protocol);
	return Socket != INVALID_SOCKET ? new FSocketLive(Socket,SOCKTYPE_Datagram,SocketDescription) : NULL;
}

/**
 * Creates a stream (TCP) socket
 *
 * @param SocketDescription debug description
 *
 * @return the new socket or NULL if failed
 */
FSocket* FSocketSubsystemLive::CreateStreamSocket(const FString& SocketDescription)
{
	SOCKET Socket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	return Socket != INVALID_SOCKET ? new FSocketLive(Socket,SOCKTYPE_Streaming,SocketDescription) : NULL;
}

/**
 * Does a DNS look up of a host name
 *
 * @param HostName the name of the host to look up
 * @param Addr the address to copy the IP address to
 */
INT FSocketSubsystemLive::GetHostByName(ANSICHAR* HostName,FInternetIpAddr& Addr)
{
	Addr.SetIp(0);
	XNDNS* Xnds = NULL;
	// Kick off a DNS lookup. This is non-blocking
	INT ErrorCode = XNetDnsLookup(HostName,NULL,&Xnds);
	if (ErrorCode == 0 && Xnds != NULL)
	{
		// While we are waiting for the results to come back, sleep to let
		// other threads run
		while (Xnds->iStatus == WSAEINPROGRESS)
		{
			appSleep(0.1f);
		}
		// It's done with the look up, so validate the results
		if (Xnds->iStatus == 0)
		{
			// Make sure it found some entries
			if (Xnds->cina > 0)
			{
				// Copy the address
				Addr.SetIp(Xnds->aina[0]);
			}
			else
			{
				ErrorCode = WSAHOST_NOT_FOUND;
			}
		}
		else
		{
			ErrorCode = Xnds->iStatus;
		}
		// Free the dns structure
		XNetDnsRelease(Xnds);
	}
	return ErrorCode;
}

/**
 * Creates a platform specific async hostname resolution object
 *
 * @param HostName the name of the host to look up
 *
 * @return the resolve info to query for the address
 */
FResolveInfo* FSocketSubsystemLive::GetHostByName(ANSICHAR* HostName)
{
#if !FINAL_RELEASE
	if (bUseLspEnumerate)
#endif
	{
		FInternetIpAddr Addr;
		// See if we have it cached or not
		if (GetHostByNameFromCache(HostName,Addr))
		{
			return new FResolveInfoCached(Addr);
		}
		else
		{
			FResolveInfoLsp* AsyncResolve = new FResolveInfoLsp(HostName,ServiceId);
			AsyncResolve->StartAsyncTask();
			return AsyncResolve;
		}
	}
	return FSocketSubsystem::GetHostByName(HostName);
}

/**
 * Determines the name of the local machine
 *
 * @param HostName the string that receives the data
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketSubsystemLive::GetHostName(FString& HostName)
{
	HostName = appComputerName();
	return TRUE;
}

/**
 * Uses the secure libs to look up the host address
 *
 * @param Out the output device to log messages to
 * @param HostAddr the out param receiving the host address
 *
 * @return always TRUE
 */
UBOOL FSocketSubsystemLive::GetLocalHostAddr(FOutputDevice& Out,
	FInternetIpAddr& HostAddr)
{
	XNADDR XnAddr;
	appMemzero(&XnAddr,sizeof(XNADDR));
	while (XNetGetTitleXnAddr(&XnAddr) == XNET_GET_XNADDR_PENDING)
	{
		appSleep(0.1f);
	}
	HostAddr.SetIp(XnAddr.ina);
	static UBOOL First = TRUE;
	if (First)
	{
		First = FALSE;
		debugf(NAME_Init, TEXT("%s: I am %s (%s)"), SOCKET_API, appComputerName(), *HostAddr.ToString(TRUE) );
	}
	return TRUE;
}

/**
 * Decode ip addr from platform addr read from buffer
 *
 * @param ToIpAddr dest ip addr result to decode platform addr to
 * @param FromPlatformInfo byte array containing platform addr
 * @return TRUE if platform addr was successfully decoded to ip addr
 */
UBOOL FSocketSubsystemLive::DecodeIpAddr(FIpAddr& ToIpAddr,const TArray<BYTE>& FromPlatformInfo)
{
	UBOOL bValid = FALSE;

	// Live platform addr
	XNADDR XnAddr;
	// key exchange id used to decode XnAddr
	XNKID XnKeyId;
	if (FromPlatformInfo.Num() == (sizeof(XNADDR)+sizeof(XNKID)))
	{
		// copy XNADDR,XNKID from buffer
		const BYTE* Buffer = FromPlatformInfo.GetData();
		appMemcpy(&XnAddr,Buffer,sizeof(XNADDR));
		Buffer += sizeof(XNADDR);
		appMemcpy(&XnKeyId,Buffer,sizeof(XNKID));
		// decode XNADDR to IN_ADDR 
		IN_ADDR InAddr;
		INT Result = XNetXnAddrToInAddr(&XnAddr,&XnKeyId,&InAddr);
		if (Result == 0)
		{
			FInternetIpAddr InternetIpAddr;
			InternetIpAddr.SetIp(InAddr);
			InternetIpAddr.GetIp(ToIpAddr.Addr);
			bValid = TRUE;
		}
		else
		{
			debugf(NAME_DevNet,TEXT("DecodeIpAddr XNetXnAddrToInAddr() returned 0x%08X"),Result);
		}
	}
	else
	{
		debugf(NAME_DevNet,TEXT("DecodeIpAddr platform info buffer size mismatch!"));
	}
	return bValid;
}

/**
 * Encode ip addr to platform addr and save in buffer
 *
 * @param ToPlatformInfo byte array containing platform addr result
 * @param FromIpAddr source ip addr to encode platform addr from
 * @return TRUE if platform addr was successfully encoded from ip addr
 */
UBOOL FSocketSubsystemLive::EncodeIpAddr(TArray<BYTE>& ToPlatformInfo,const FIpAddr& FromIpAddr)
{
	UBOOL bValid = FALSE;

	FInternetIpAddr InternetIpAddr;
	InternetIpAddr.SetIp(FromIpAddr);
	IN_ADDR InAddr;
	InternetIpAddr.GetIp(InAddr);

	// Live platform addr
	XNADDR XnAddr;
	// key exchange id used to decode XnAddr
	XNKID XnKeyId;
	// decode IN_ADDR to XNADDR
	INT Result = XNetInAddrToXnAddr(InAddr,&XnAddr,&XnKeyId);
	if (Result == 0)
	{
		// copy XNADDR,XNKID to buffer
		ToPlatformInfo.Empty(sizeof(XNADDR)+sizeof(XNKID));
		ToPlatformInfo.AddZeroed(sizeof(XNADDR)+sizeof(XNKID));
		BYTE* Buffer = ToPlatformInfo.GetData();
		appMemcpy(Buffer,&XnAddr,sizeof(XNADDR));
		Buffer += sizeof(XNADDR);
		appMemcpy(Buffer,&XnKeyId,sizeof(XNKID));
		bValid = TRUE;
	}
	else
	{
		debugf(NAME_DevNet,TEXT("EncodeIpAddr XNetInAddrToXnAddr() returned 0x%08X"),Result);
	}
	return bValid;
}

#endif	//#if WITH_UE3_NETWORKING
