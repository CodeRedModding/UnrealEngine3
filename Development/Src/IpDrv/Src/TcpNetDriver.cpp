/*=============================================================================
	TcpNetDriver.cpp: Unreal TCP/IP driver.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
Notes:
	* See \msdev\vc98\include\winsock.h and \msdev\vc98\include\winsock2.h 
	  for Winsock WSAE* errors returned by Windows Sockets.
=============================================================================*/

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// Size of a UDP header.
#define IP_HEADER_SIZE     (20)
#define UDP_HEADER_SIZE    (IP_HEADER_SIZE+8)
#define SLIP_HEADER_SIZE   (UDP_HEADER_SIZE+4)
#define WINSOCK_MAX_PACKET (512)
#define NETWORK_MAX_PACKET (576)

// Variables.
#ifndef XBOX
// Xenon version is in UnXenon.cpp
UBOOL GIpDrvInitialized;
#endif

/*-----------------------------------------------------------------------------
	UTcpipConnection.
-----------------------------------------------------------------------------*/

/**
 * Initializes a connection with the passed in settings
 *
 * @param InDriver the net driver associated with this connection
 * @param InSocket the socket associated with this connection
 * @param InRemoteAddr the remote address for this connection
 * @param InState the connection state to start with for this connection
 * @param InOpenedLocally whether the connection was a client/server
 * @param InURL the URL to init with
 * @param InMaxPacket the max packet size that will be used for sending
 * @param InPacketOverhead the packet overhead for this connection type
 */
void UTcpipConnection::InitConnection(UNetDriver* InDriver,FSocket* InSocket,
	const FInternetIpAddr& InRemoteAddr,EConnectionState InState,
	UBOOL InOpenedLocally,const FURL& InURL,INT InMaxPacket,INT InPacketOverhead)
{
	// Init the connection
	Driver = InDriver;
	StatUpdateTime = Driver->Time;
	LastReceiveTime = Driver->Time;
	LastSendTime = Driver->Time;
	LastTickTime = Driver->Time;
	LastRecvAckTime = Driver->Time;
	ConnectTime = Driver->Time;
	RemoteAddr = InRemoteAddr;
	URL = InURL;
	OpenedLocally = InOpenedLocally;
	if (OpenedLocally == FALSE)
	{
		URL.Host = RemoteAddr.ToString(FALSE);
	}
	Socket = InSocket;
	ResolveInfo = NULL;
	State = InState;
	// Pass the call up the chain
	Super::InitConnection(InDriver,InSocket,InRemoteAddr,InState,InOpenedLocally,InURL,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? WINSOCK_MAX_PACKET : InMaxPacket,
		InPacketOverhead == 0 ? SLIP_HEADER_SIZE : InPacketOverhead);
	// Initialize our send bunch
	InitOut();

	// In connecting, figure out IP address (if it was not already explicitly specified)
	if (InOpenedLocally)
	{
		DWORD RemoteIP;
		RemoteAddr.GetIp(RemoteIP);

		if (RemoteIP == 0)
		{
			UBOOL bIsValid;

			// Get numerical address directly
			RemoteAddr.SetIp(*InURL.Host, bIsValid);

			// Try to resolve it if it failed
			if (bIsValid == FALSE)
			{
				// Create thread to resolve the address
				ResolveInfo = GSocketSubsystem->GetHostByName(TCHAR_TO_ANSI(*InURL.Host));
			}
		}
	}
}

void UTcpipConnection::LowLevelSend( void* Data, INT Count )
{
	if( ResolveInfo )
	{
		// If destination address isn't resolved yet, send nowhere.
		if( !ResolveInfo->IsComplete() )
		{
			// Host name still resolving.
			return;
		}
		else if( ResolveInfo->GetErrorCode() != SE_NO_ERROR )
		{
			// Host name resolution just now failed.
			debugf( NAME_Log, TEXT("Host name resolution failed with %d"), ResolveInfo->GetErrorCode() );
			Driver->ServerConnection->State = USOCK_Closed;
			delete ResolveInfo;
			ResolveInfo = NULL;
			return;
		}
		else
		{
			// Host name resolution just now succeeded.
			RemoteAddr.SetIp(ResolveInfo->GetResolvedAddress());
			debugf(TEXT("Host name resolution completed"));
			delete ResolveInfo;
			ResolveInfo = NULL;
		}
	}
	// Send to remote.
	INT BytesSent = 0;
	CLOCK_CYCLES(Driver->SendCycles);
	Socket->SendTo((BYTE*)Data,Count,BytesSent,RemoteAddr);
	UNCLOCK_CYCLES(Driver->SendCycles);
}

FString UTcpipConnection::LowLevelGetRemoteAddress(UBOOL bAppendPort)
{
	return RemoteAddr.ToString(bAppendPort);
}

FString UTcpipConnection::LowLevelDescribe()
{
	FInternetIpAddr LocalAddr = Socket->GetAddress();
	return FString::Printf
	(
		TEXT("url=%s remote=%s local=%s state: %s"),
		*URL.Host,
		*RemoteAddr.ToString(TRUE),
		*LocalAddr.ToString(TRUE),
			State==USOCK_Pending	?	TEXT("Pending")
		:	State==USOCK_Open		?	TEXT("Open")
		:	State==USOCK_Closed		?	TEXT("Closed")
		:								TEXT("Invalid")
	);
}

IMPLEMENT_CLASS(UTcpipConnection);

/*-----------------------------------------------------------------------------
	UTcpNetDriver.
-----------------------------------------------------------------------------*/

//
// Windows sockets network driver.
//
UBOOL UTcpNetDriver::InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error )
{
	if( !Super::InitConnect( InNotify, ConnectURL, Error ) )
	{
		return FALSE;
	}
	if( !InitBase( 1, InNotify, ConnectURL, Error ) )
	{
		return FALSE;
	}

	// Connect to remote.
	FInternetIpAddr TempAddr;
	TempAddr.SetPort(ConnectURL.Port);
	TempAddr.SetIp(0,0,0,0);

	// Create new connection.
	ServerConnection = ConstructObject<UNetConnection>(NetConnectionClass);
	ServerConnection->InitConnection( this, Socket, TempAddr, USOCK_Pending, TRUE, ConnectURL );
	debugf( NAME_DevNet, TEXT("Game client on port %i, rate %i"), TempAddr.GetPort(), ServerConnection->CurrentNetSpeed );

	// Create channel zero.
	GetServerConnection()->CreateChannel( CHTYPE_Control, 1, 0 );

	return TRUE;
}

UBOOL UTcpNetDriver::InitListen( FNetworkNotify* InNotify, FURL& LocalURL, FString& Error )
{
	if( !Super::InitListen( InNotify, LocalURL, Error ) )
	{
		return FALSE;
	}
	if( !InitBase( 0, InNotify, LocalURL, Error ) )
	{
		return FALSE;
	}

	// Update result URL.
	LocalURL.Host = LocalAddr.ToString(FALSE);
	LocalURL.Port = LocalAddr.GetPort();
	debugf( NAME_DevNet, TEXT("%s TcpNetDriver listening on port %i"), *GetDescription(), LocalURL.Port );

	return TRUE;
}

/**
 * Initialize a new peer connection on the net driver
 *
 * @param InNotify notification object to associate with the net driver
 * @param ConnectURL remote ip:port of client peer to connect
 * @param RemotePlayerId remote net id of client peer player
 * @param LocalPlayerId net id of primary local player
 * @param Error resulting error string from connection attempt
 */
UBOOL UTcpNetDriver::InitPeer( FNetworkNotify* InNotify, const FURL& ConnectURL, FUniqueNetId RemotePlayerId, FUniqueNetId LocalPlayerId, FString& Error )
{
	UBOOL bIsValid = FALSE;
	FInternetIpAddr PeerRemoteAddr;
	PeerRemoteAddr.SetIp(*ConnectURL.Host,bIsValid);
	PeerRemoteAddr.SetPort(ConnectURL.Port);

	// Make sure ip specified on URL was valid
	if (!bIsValid)
	{
		Error = FString(TEXT("UTcpNetDriver.InitPeer: Invalid ip addr"));
		return FALSE;
	}
	// Socket should already be initialized for listening and it will also be used for connection attempts
	if (Socket == NULL)
	{
		Error = FString(TEXT("UTcpNetDriver.InitPeer: No socket was created"));
		return FALSE;
	}
	// Make sure playerid of remote client is valid
	if (!RemotePlayerId.HasValue())
	{
		debugf(NAME_DevNet,TEXT("UTcpNetDriver.InitPeer: Zero RemotePlayerId specified for peer"),
			*PeerRemoteAddr.ToString(TRUE));
		//Error = FString(TEXT("Zero RemotePlayerId specified for peer"));
		//return FALSE;
	}
	// Make sure playerid of local client is valid
	if (!LocalPlayerId.HasValue())
	{
		debugf(NAME_DevNet,TEXT("UTcpNetDriver.InitPeer: Zero LocalPlayerId specified for peer"),
			*PeerRemoteAddr.ToString(TRUE));
		//Error = FString(TEXT("Zero LocalPlayerId specified for peer"));
		//return FALSE;
	}

	debugf(NAME_DevNet,TEXT("UTcpNetDriver.InitPeer: connecting to peer at remote addr=%s RemotePlayerId=0x%016I64X LocalPlayerId=0x%016I64X"),
		*PeerRemoteAddr.ToString(TRUE),
		RemotePlayerId.Uid,
		LocalPlayerId.Uid);

	// Make sure the peer connection does not already exist	
	for (INT ClientIdx=0; ClientIdx < ClientConnections.Num(); ClientIdx++)
	{
		UTcpipConnection* ClientConnection = Cast<UTcpipConnection>(ClientConnections(ClientIdx));
		if (ClientConnection != NULL && 
			ClientConnection->RemoteAddr == PeerRemoteAddr)
		{
			// already connected, so just update net id
			ClientConnection->PlayerId = RemotePlayerId;
			// Make sure there is a valid control channel
			if (ClientConnection->Channels[0] == NULL)
			{
				ClientConnection->CreateChannel(CHTYPE_Control, 1, 0);
			}
			// Send initial message to initiate a peer join
 			FNetControlMessage<NMT_PeerJoin>::Send(ClientConnection, LocalPlayerId);
 			ClientConnection->FlushNet(TRUE);
			return TRUE;
			//Error = FString(TEXT("Already have a connection for peer "));
			//return FALSE;
		}
	}

	if( !Super::InitPeer( InNotify, ConnectURL, RemotePlayerId, LocalPlayerId, Error ) )
	{
		return FALSE;
	}

	// Create new connection.
	UTcpipConnection* Connection = ConstructObject<UTcpipConnection>(NetConnectionClass);
	check(Connection != NULL);

	// Connect to remote peer
	FInternetIpAddr TempAddr;
	TempAddr.SetPort(ConnectURL.Port);
	TempAddr.SetIp(0,0,0,0);
	Connection->InitConnection( this, Socket, TempAddr, USOCK_Pending, TRUE, ConnectURL );
	// Keep track of the remote peer's unique id
	Connection->PlayerId = RemotePlayerId;

	// Notify that a new peer connection was created
	Notify->NotifyAcceptedPeerConnection( Connection );
	ClientConnections.AddItem( Connection );
	// Create control channel to handle peer messages
	Connection->CreateChannel( CHTYPE_Control, 1, 0 );
	
	// Send initial message to initiate a peer join
	FNetControlMessage<NMT_PeerJoin>::Send(Connection, LocalPlayerId);
	Connection->FlushNet(TRUE);

	return TRUE;
}

void UTcpNetDriver::TickDispatch( FLOAT DeltaTime )
{
	Super::TickDispatch( DeltaTime );

	// Process all incoming packets.
	BYTE Data[NETWORK_MAX_PACKET];
	FInternetIpAddr FromAddr;
	while( Socket != NULL )
	{
		INT BytesRead = 0;
		// Get data, if any.
		CLOCK_CYCLES(RecvCycles);
		UBOOL bOk = Socket->RecvFrom(Data,sizeof(Data),BytesRead,FromAddr);
		UNCLOCK_CYCLES(RecvCycles);
		// Handle result.
		if( bOk == FALSE )
		{
			INT Error = GSocketSubsystem->GetLastErrorCode();
			if( Error == SE_EWOULDBLOCK )
			{
				// No data
				break;
			}
			else if (Error == SE_NO_ERROR)
			{
				// no error...?
				break;
			}
			else
			{
                #if 0 // !!! FIXME def __linux__
                    // determine IP address where problem originated. --ryan.
					GSocketSubsystem->GetErrorOriginatingAddress(&FromAddr);
                #endif
                
				if( Error != SE_UDP_ERR_PORT_UNREACH )
				{
					static UBOOL FirstError=1;
#if !CONSOLE//@todo joeg -- Remove/re-add this after verifying the problem on console
					if( FirstError )
#endif
					{
						debugf( TEXT("UDP recvfrom error: %i (%s) from %s"),
							Error,
							GSocketSubsystem->GetSocketError(Error),
							*FromAddr.ToString(TRUE));
					}
					FirstError = 0;
					break;
				}
			}
		}
		// Figure out which socket the received data came from.
		UTcpipConnection* Connection = NULL;
		if (GetServerConnection() && GetServerConnection()->RemoteAddr == FromAddr)
		{
			Connection = GetServerConnection();
		}
		for( INT i=0; i<ClientConnections.Num() && !Connection; i++ )
		{
			if(((UTcpipConnection*)ClientConnections(i))->RemoteAddr == FromAddr)
			{
				Connection = (UTcpipConnection*)ClientConnections(i);
			}
		}

		if( bOk == FALSE )
		{
			if( Connection )
			{
				if( Connection != GetServerConnection() )
				{
					// We received an ICMP port unreachable from the client, meaning the client is no longer running the game
					// (or someone is trying to perform a DoS attack on the client)

					// rcg08182002 Some buggy firewalls get occasional ICMP port
					// unreachable messages from legitimate players. Still, this code
					// will drop them unceremoniously, so there's an option in the .INI
					// file for servers with such flakey connections to let these
					// players slide...which means if the client's game crashes, they
					// might get flooded to some degree with packets until they timeout.
					// Either way, this should close up the usual DoS attacks.
					if ((Connection->State != USOCK_Open) || (!AllowPlayerPortUnreach))
					{
						if (LogPortUnreach)
						{
							debugf( TEXT("Received ICMP port unreachable from client %s.  Disconnecting."),
								*FromAddr.ToString(TRUE));
						}
						Connection->CleanUp();
					}
				}
			}
			else
			{
				if (LogPortUnreach)
				{
					debugf( TEXT("Received ICMP port unreachable from %s.  No matching connection found."),
						*FromAddr.ToString(TRUE));
				}
			}
		}
		else
		{
			// If we didn't find a client connection, maybe create a new one.
			if( !Connection )
			{
				// Determine if allowing for peer/peer connections
				const UBOOL bAcceptingPeerConnection = bIsPeer && Notify->NotifyAcceptingPeerConnection()==ACCEPTC_Accept;
				// Determine if allowing for client/server connections
				const UBOOL bAcceptingConnection = !bIsPeer && Notify->NotifyAcceptingConnection()==ACCEPTC_Accept;

				if (bAcceptingConnection || bAcceptingPeerConnection)
				{
					Connection = ConstructObject<UTcpipConnection>(NetConnectionClass);
					Connection->InitConnection( this, Socket, FromAddr, USOCK_Open, FALSE, FURL() );
					if (bAcceptingPeerConnection)
					{
						Notify->NotifyAcceptedPeerConnection( Connection );
					}
					else
					{
						Notify->NotifyAcceptedConnection( Connection );
					}
					ClientConnections.AddItem( Connection );
				}
			}

			// Send the packet to the connection for processing.
			if( Connection )
			{
				Connection->ReceivedRawPacket( Data, BytesRead );
			}
		}
	}

#if NGP
	// the ngp network interface sleeps after 2 minutes of inactivity
	// listening isn't enough to keep the network interface up, so we need to send something periodically
	static const DOUBLE wakeInterval = 60; // in seconds
	static DOUBLE lastWakeTime = 0;

	// if we are the server (no ServerConnection) then send the wakeup packet
	// the network interface will stay awake if there are active client connections
	if (Socket && !ServerConnection && ClientConnections.Num() == 0)
	{
		// ignore the delta time passed in because it gets fudged when the framerate changes and we need *real* time
		DOUBLE CurrentTime = appSeconds();
		if (CurrentTime >= lastWakeTime + wakeInterval)
		{
			lastWakeTime = CurrentTime;

			// This is the official Sony solution to wake the network according to "4.3 Cautions for Intermittent Connection and Intermittent Disconnection"
			FInternetIpAddr WakeAddr;
			WakeAddr.SetAddress(SCE_NET_INADDR_BROADCAST);
			WakeAddr.SetPort(8000);

			BYTE WakeMsg[] = "Listening";
			INT BytesSent = 0;
			Socket->SendTo(WakeMsg, sizeof(WakeMsg), BytesSent, WakeAddr);
		}
	}
#endif
}

FString UTcpNetDriver::LowLevelGetNetworkNumber(UBOOL bAppendPort)
{
	return LocalAddr.ToString(bAppendPort);
}

void UTcpNetDriver::LowLevelDestroy()
{
	// Close the socket.
	if( Socket && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		if( !Socket->Close() )
		{
			debugf( NAME_Exit, TEXT("closesocket error (%i)"), GSocketSubsystem->GetLastErrorCode() );
		}
		// Free the memory the OS allocated for this socket
		GSocketSubsystem->DestroySocket(Socket);
		Socket=NULL;
		debugf( NAME_Exit, TEXT("%s shut down"),*GetDescription() );
	}

}

UBOOL UTcpNetDriver::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (ParseCommand(&Cmd,TEXT("SOCKETS")))
	{
		Ar.Logf(TEXT(""));
		if (Socket != NULL)
		{
			Ar.Logf(TEXT("%s Socket: %s"), *GetDescription(), *Socket->GetAddress().ToString(TRUE));
		}		
		else
		{
			Ar.Logf(TEXT("%s Socket: null"), *GetDescription());
		}
		return UNetDriver::Exec(TEXT("SOCKETS"),Ar);
	}
	return UNetDriver::Exec(Cmd,Ar);
}

// UTcpNetDriver interface.
UBOOL UTcpNetDriver::InitBase( UBOOL Connect, FNetworkNotify* InNotify, const FURL& URL, FString& Error )
{
	// Create UDP socket and enable broadcasting.
	if (Socket == NULL)
	{
		Socket = GSocketSubsystem->CreateDGramSocket(TEXT("Unreal"));
	}

	if( Socket == NULL )
	{
		Socket = 0;
		Error = FString::Printf( TEXT("WinSock: socket failed (%i)"), GSocketSubsystem->GetLastErrorCode() );
		return 0;
	}
#if !WITH_PANORAMA && !PS3 && !_XBOX
	if (GSocketSubsystem->RequiresChatDataBeSeparate() == FALSE &&
		Socket->SetBroadcast() == FALSE)
	{
		Error = FString::Printf( TEXT("%s: setsockopt SO_BROADCAST failed (%i)"), SOCKET_API, GSocketSubsystem->GetLastErrorCode() );
		return 0;
	}
#endif

	if (Socket->SetReuseAddr() == FALSE)
	{
		debugf(TEXT("setsockopt with SO_REUSEADDR failed"));
	}

	if (Socket->SetRecvErr() == FALSE)
	{
		debugf(TEXT("setsockopt with IP_RECVERR failed"));
	}

    // Increase socket queue size, because we are polling rather than threading
	// and thus we rely on Windows Sockets to buffer a lot of data on the server.
	int RecvSize = Connect ? 0x8000 : 0x20000;
	int SendSize = Connect ? 0x8000 : 0x20000;
	Socket->SetReceiveBufferSize(RecvSize,RecvSize);
	Socket->SetSendBufferSize(SendSize,SendSize);
	debugf( NAME_Init, TEXT("%s: Socket queue %i / %i"), SOCKET_API, RecvSize, SendSize );

	// Bind socket to our port.
	LocalAddr.SetIp(getlocalbindaddr( *GLog ));
	LocalAddr.SetPort(0);
	if( !Connect )
	{
		// Init as a server.
		LocalAddr.SetPort(URL.Port);
	}
	INT AttemptPort = LocalAddr.GetPort();
	INT boundport   = bindnextport( Socket, LocalAddr, 20, 1 );
	if( boundport==0 )
	{
		Error = FString::Printf( TEXT("%s: binding to port %i failed (%i)"), SOCKET_API, AttemptPort,
			GSocketSubsystem->GetLastErrorCode() );
		return FALSE;
	}
	if( Socket->SetNonBlocking() == FALSE )
	{
		Error = FString::Printf( TEXT("%s: SetNonBlocking failed (%i)"), SOCKET_API,
			GSocketSubsystem->GetLastErrorCode());
		return FALSE;
	}

	// Success.
	return TRUE;
}

UTcpipConnection* UTcpNetDriver::GetServerConnection() 
{
	return (UTcpipConnection*)ServerConnection;
}

//
// Return the NetDriver's socket.  For master server NAT socket opening purposes.
//
FSocketData UTcpNetDriver::GetSocketData()
{
	FSocketData Result;
	Result.Socket = Socket;
	Result.UpdateFromSocket();
	return Result;
}

void UTcpNetDriver::StaticConstructor()
{
	new(GetClass(),TEXT("AllowPlayerPortUnreach"),	RF_Public)UBoolProperty (CPP_PROPERTY(AllowPlayerPortUnreach), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("LogPortUnreach"),			RF_Public)UBoolProperty (CPP_PROPERTY(LogPortUnreach        ), TEXT("Client"), CPF_Config );
}


IMPLEMENT_CLASS(UTcpNetDriver);

#endif	//#if WITH_UE3_NETWORKING
