/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemSteamworks.h"

#if WITH_STEAMWORKS_SOCKETS

/**
 * Extern implementations
 */

/**
 * Determines if the Steam sockets net driver is enabled
 *
 * @return	Returns TRUE if the steam sockets net driver is enabled, FALSE otherwise
 */
UBOOL appSteamNetDriverEnabled()
{
	return IsSteamNetDriverEnabled();
}

/**
 * Determines whether or not steam sockets is forcibly enabled, so that all layers of the online subsystem assume steam sockets is enabled.
 *
 * This primarily affects server hosting (steam sockets is always on regardless of ?steamsockets parameter) the server browser (assumes all
 * servers use steam sockets, setting their SteamServerId without checking it is enabled), and invites (invites >ALWAYS< connect to the server
 * steam sockets address, and this has 100% NAT traversal)
 *
 * @return	Whether or not steam sockets is force-enabled
 */
UBOOL appSteamSocketsOnly()
{
	return IsSteamSocketsOnly();
}

/**
 * Handles NMT_Redirect messages; either immediately redirecting clients, or waiting to redirect them if GSteamGameServer is not fully setup
 *
 * @param Connection	The net connection to send a redirect message to
 */
void appSteamHandleRedirect(UNetConnection* Connection)
{
	if (GSteamGameServer != NULL)
	{
		// If GSteamGameServer is fully setup, and has a valid SteamID for steam sockets, send the message immediately
		if (GSteamworksGameServerConnected)
		{
			FString RedirectAddress = FString::Printf(I64_FORMAT_TAG, SteamGameServer_GetSteamID());
			FNetControlMessage<NMT_Redirect>::Send(Connection, RedirectAddress);

			Connection->FlushNet();
			Connection->Close();
		}
		// If GSteamGameServer is NOT fully setup, queue the redirect until it is
		else
		{
			UIpNetDriverSteamworks* NetDriver = Cast<UIpNetDriverSteamworks>(GetActiveNetDriver());

			if (NetDriver != NULL)
			{
				FIpAddr CurAddr(Connection->GetAddrAsInt(), Connection->GetAddrPort());
				NetDriver->PendingRedirects.AddItem(CurAddr);
			}
		}
	}
	else
	{
		FString Error = TEXT("Server failed to redirect to Steam sockets URL");
		FNetControlMessage<NMT_Failure>::Send(Connection, Error);

		Connection->FlushNet();
		Connection->Close();
	}
}

/**
 * Registers the Steamworks native-only net classes
 */
void RegisterSteamworksNetClasses()
{
	// Don't want to register/export the net classes during 'make'
	if (!GIsUCCMake)
	{
		UIpNetDriverSteamworks::StaticClass();
		UIpNetConnectionSteamworks::StaticClass();
	}
}


/**
 * Utility functions
 */

/**
 * Determines if the Steam sockets net driver is enabled
 *
 * @return	Returns TRUE if the steam sockets net driver is enabled, FALSE otherwise
 */
UBOOL IsSteamNetDriverEnabled()
{
	UBOOL bReturnVal = FALSE;
	UNetDriver* NetDriver = GetActiveNetDriver();

	if (NetDriver != NULL)
	{
		bReturnVal = Cast<UIpNetDriverSteamworks>(NetDriver) != NULL;
	}
	else
	{
		// If there is no net driver currently active, determine based upon the .ini file
		UClass* NetDriverClass = UObject::StaticLoadClass(UNetDriver::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.NetworkDevice"),
									NULL, LOAD_Quiet, NULL);
		if (NetDriverClass == NULL)
		{
			NetDriverClass = UObject::StaticLoadClass(UNetDriver::StaticClass(), NULL,
									TEXT("engine-ini:Engine.Engine.FallbackNetworkDevice"), NULL, LOAD_None, NULL);
		}

		if (NetDriverClass == UIpNetDriverSteamworks::StaticClass())
		{
			bReturnVal = TRUE;
		}
	}

	return bReturnVal;
}

/**
 * Determines whether or not steam sockets is forcibly enabled, so that all layers of the online subsystem assume steam sockets is enabled.
 *
 * This primarily affects server hosting (steam sockets is always on regardless of ?steamsockets parameter) the server browser (assumes all
 * servers use steam sockets, setting their SteamServerId without checking it is enabled), and invites (invites >ALWAYS< connect to the server
 * steam sockets address, and this has 100% NAT traversal)
 *
 * @return	Whether or not steam sockets is force-enabled
 */
UBOOL IsSteamSocketsOnly()
{
	UBOOL bReturnVal = FALSE;

	if (IsSteamNetDriverEnabled())
	{
		bReturnVal = GetDefault<UIpNetDriverSteamworks>()->bSteamSocketsOnly;
	}

	return bReturnVal;
}

/**
 * Parses a "steam.#" URL address, into an FInternetIpAddr containing the UID
 *
 * @param InHost	The URL host string to parse
 * @param OutAddr	Outputs the Steam address
 * @return		Whether or not the URL was successfully converted into a SteamId
 */
FORCEINLINE UBOOL ParseSteamAddress(const TCHAR* InHost, FInternetIpAddr* OutAddr)
{
	UBOOL bSuccess = FALSE;

	FString HostStr(InHost);
	const FString Prefix(TEXT("steam."));

	if (HostStr.StartsWith(Prefix))
	{
		TCHAR* EndPtr = NULL;
		FString UIDStr(HostStr.Mid(Prefix.Len()));
		QWORD UID = appStrtoi64(*UIDStr, &EndPtr, 10);

		// Check that after parsing the UID, we are at the end of the string (otherwise the steam URL is invalid)
		if ((*EndPtr == '\0') && (EndPtr != *UIDStr))
		{
			SteamIdToIpAddr(CSteamID((uint64)UID), *OutAddr);
			bSuccess = TRUE;
		}
	}

	return bSuccess;
}


/**
 * UIpNetDriverSteamworks implementation
 */

/**
 * Static constructor (for config values)
 */
void UIpNetDriverSteamworks::StaticConstructor()
{
	new(GetClass(), TEXT("bSteamSocketsOnly"), RF_Public) UBoolProperty(CPP_PROPERTY(bSteamSocketsOnly), TEXT("Steam"), CPF_Config);
}

/**
 * Initiates a client connection to a game server, through ServerConnection
 *
 * @param InNotify	The FNetworkNotify object which is to receive networking notification events
 * @param ConnectURL	The URL the client is connecting to
 * @param Error		If there was an error during connection, this returns the error
 * @return		whether or not the connect kicked off successfully
 */
UBOOL UIpNetDriverSteamworks::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	// If we are opening a Steam URL, create a Steam client socket
	if (GSteamworksClientInitialized && ConnectURL.Host.StartsWith(TEXT("steam.")))
	{
		FSocketSteamworks* NewSocket = new FSocketSteamworksClient(TEXT("Unreal client (Steam)"));
		NewSocket->InitializeSocket();

		Socket = NewSocket;
	}

	return Super::InitConnect(InNotify, ConnectURL, Error);
}

/**
 * Initiates a server into listen mode, for accepting client connections
 *
 * @param InNotify	The FNetworkNotify object which is to receive networking notification events
 * @param ListenURL	The startup URL for the server
 * @param Error		If there is an error during setup, this returns the error
 * @return		whether or not listen mode kicked off successfully
 */
UBOOL UIpNetDriverSteamworks::InitListen(FNetworkNotify* InNotify, FURL& ListenURL, FString& Error)
{
	// If the server is starting up with ?steamsockets in the URL, create a Steam server socket
	if (!bRedirectDriver && (IsSteamSocketsOnly() || ListenURL.HasOption(TEXT("steamsockets"))))
	{
		FSocketSteamworks* NewSocket = new FSocketSteamworksServer(TEXT("Unreal server (Steam)"));
		NewSocket->InitializeSocket();

		Socket = NewSocket;
	}

	return Super::InitListen(InNotify, ListenURL, Error);
}

/**
 * Called by the game engine, when the game is exiting, to allow for special game-exit cleanup
 */
void UIpNetDriverSteamworks::PreExit()
{
	Super::PreExit();

	if (GSteamSocketsManager != NULL)
	{
		GSteamSocketsManager->PreExit(this);
	}
}

/**
 * Called from the Steam subsystem, when the Steamworks game server interface is initialized
 */
void UIpNetDriverSteamworks::InitGameServer()
{
	// If there are any IP connections waiting to be redirected to Steam sockets connections, carry out the redirect
	UNetDriver* RedirectDriver = (GWorld != NULL ? GWorld->RedirectNetDriver : NULL);

	if (RedirectDriver != NULL && PendingRedirects.Num() > 0)
	{
		FString RedirectAddress = FString::Printf(I64_FORMAT_TAG, SteamGameServer_GetSteamID());

		// Iterate backwards, because elements get removed from ClientConnections as we go
		for (INT ConnIdx=RedirectDriver->ClientConnections.Num()-1; ConnIdx>=0; ConnIdx--)
		{
			UNetConnection* CurConn = RedirectDriver->ClientConnections(ConnIdx);
			INT CurIP = CurConn->GetAddrAsInt();
			INT CurPort = CurConn->GetAddrPort();
			UBOOL bAwaitingRedirect = FALSE;

			// NOTE: I stored the IP/Port instead of UNetConnection, in case of unexpected travel/GC
			for (INT i=0; i<PendingRedirects.Num(); i++)
			{
				if (PendingRedirects(i).Addr == CurIP && PendingRedirects(i).Port == CurPort)
				{
					PendingRedirects.Remove(i, 1);
					bAwaitingRedirect = TRUE;

					break;
				}
			}


			if (CurConn->State != USOCK_Closed && bAwaitingRedirect)
			{
				FNetControlMessage<NMT_Redirect>::Send(CurConn, RedirectAddress);

				CurConn->FlushNet();
				CurConn->Close();
			}
		}
	}

	// No 'PendingRedirects' elements should be left now; spit out debug messages if there are
	if (PendingRedirects.Num() > 0)
	{
		debugf(NAME_DevOnline, TEXT("UIpNetDriverSteamworks: Warning: Not all 'PendingRedirects' were processed, %i remaining"),
			PendingRedirects.Num());

		// NOTE: Disabled but left in for debugging, as this will trigger a crash if any elements have been GC'd
#if 0
		for (INT i=0; i<PendingRedirects.Num(); i++)
		{
			debugf(NAME_DevOnline, TEXT("UIpNetDriverSteamworks: PendingRedirects(%i): %s"), i, *PendingRedirects(i).ToString(TRUE));
		}
#endif

		PendingRedirects.Empty();
	}
}


IMPLEMENT_CLASS(UIpNetDriverSteamworks);


/**
 * UIpNetConnectionSteamworks implementation
 */

/**
 * Base constructor
 */
UIpNetConnectionSteamworks::UIpNetConnectionSteamworks()
{
	// Enable signing of packets with UID's
	bUseSessionUID = TRUE;
}

/**
 * Initializes a connection with the passed in settings (modified for Steam, to store the SteamId in the IP address)
 *
 * @param InDriver		The net driver associated with this connection
 * @param InSocket		The socket associated with this connection
 * @param InRemoteAddr		The remote address for this connection
 * @param InState		The connection state to start with for this connection
 * @param InOpenedLocally	Whether the connection was a client/server
 * @param InURL			The URL to init with
 * @param InMaxPacket		The max packet size that will be used for sending
 * @param InPacketOverhead	The packet overhead for this connection type
 */
void UIpNetConnectionSteamworks::InitConnection(UNetDriver* InDriver, FSocket* InSocket, const FInternetIpAddr& InRemoteAddr,
						EConnectionState InState, UBOOL InOpenedLocally, const FURL& InURL, INT InMaxPacket/*=0*/,
						INT InPacketOverhead/*=0*/)
{
	UBOOL bSteamConnect = FALSE;

	// If we are opening a Steam URL, store the SteamId in RemoteAddr
	if (GSteamworksClientInitialized && InURL.Host.StartsWith(TEXT("steam.")))
	{
		FInternetIpAddr SteamAddr;

		// Check that the Steam address is valid, and if so, pass on the call with that is the remote address
		if (ParseSteamAddress(*InURL.Host, &SteamAddr))
		{
			Super::InitConnection(InDriver, InSocket, SteamAddr, InState, InOpenedLocally, InURL, InMaxPacket, InPacketOverhead);
			bSteamConnect = TRUE;
		}
	}

	// Normal connection
	if (!bSteamConnect)
	{
		Super::InitConnection(InDriver, InSocket, InRemoteAddr, InState, InOpenedLocally, InURL, InMaxPacket, InPacketOverhead);
	}
}

/**
 * Closes the control channel, cleans up structures, and prepares for deletion; used by Steam to close the Steam sockets connection
 */
void UIpNetConnectionSteamworks::CleanUp()
{
	UBOOL bIsClientConnection = Driver != NULL && Driver->ClientConnections.ContainsItem(this) && GSteamGameServerNetworking != NULL;
	UBOOL bIsServerConnection = Driver != NULL && Driver->ServerConnection == this && GSteamNetworking != NULL;

	Super::CleanUp();


	// Trigger Steam sockets disconnect if this was a Steam sockets connection
	if (GSteamSocketsManager && (bIsClientConnection || bIsServerConnection))
	{
		CSteamID CurSteamID;

		if (IpAddrToSteamId(RemoteAddr, CurSteamID))
		{
			QWORD RemoteUID = CurSteamID.ConvertToUint64();

			if (RemoteUID != 0)
			{
				if (bIsClientConnection)
				{
					GSteamSocketsManager->CloseP2PSession(RemoteUID, GSteamGameServerNetworking);
				}
				else // if (bIsServerConnection)
				{
					GSteamSocketsManager->CloseP2PSession(RemoteUID, GSteamNetworking);
				}
			}
		}
	}
}

IMPLEMENT_CLASS(UIpNetConnectionSteamworks);

#endif	// WITH_STEAMWORKS_SOCKETS



