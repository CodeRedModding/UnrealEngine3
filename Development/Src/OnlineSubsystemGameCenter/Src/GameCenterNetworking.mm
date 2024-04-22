/*=============================================================================
	GameCenterNetworking.mm: GameCenter peer to peer network driver.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "UnIpDrv.h"
#include "GameCenter.h"
#include "GameCenterNetworking.h"

#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(UGameCenterNetConnection)
IMPLEMENT_CLASS(UGameCenterNetDriver)

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
void UGameCenterNetConnection::InitConnection(UNetDriver* InDriver,FSocket* InSocket,const FInternetIpAddr& InRemoteAddr,EConnectionState InState,UBOOL InOpenedLocally,const FURL& InURL,INT InMaxPacket,INT InPacketOverhead)
{
	// initialize similar to TcpNetDriver
	Driver = InDriver;
	StatUpdateTime = Driver->Time;
	LastReceiveTime = Driver->Time;
	LastSendTime = Driver->Time;
	LastTickTime = Driver->Time;
	LastRecvAckTime = Driver->Time;
	State = InState;


// @todo: no idea what values here
#define IP_HEADER_SIZE     (20)
#define UDP_HEADER_SIZE    (IP_HEADER_SIZE+8)
#define WINSOCK_MAX_PACKET (512)
#define SLIP_HEADER_SIZE   (UDP_HEADER_SIZE+4)

	Super::InitConnection(InDriver, InSocket, InRemoteAddr, InState, InOpenedLocally, InURL,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? WINSOCK_MAX_PACKET : InMaxPacket,
		InPacketOverhead == 0 ? SLIP_HEADER_SIZE : InPacketOverhead);

	// initialize the out writer
	InitOut();
}

/**
 * Sends a byte stream to the remote endpoint using the underlying socket
 *
 * @param Data the byte stream to send
 * @param Count the length of the stream to send
 */
void UGameCenterNetConnection::LowLevelSend(void* Data,INT Count)
{
	// send unreliable data to the other side
	// @TODO: THIS IS ONLY VALID FOR 2 PLAYERS. WE NEED TO GET THE PLAYER ID FOR THIS CONNECTION!!
	NSData* Packet = [NSData dataWithBytes:Data length:Count];
	[GGameCenter.CurrentMatch sendDataToAllPlayers:Packet withDataMode:GKMatchSendDataUnreliable error:nil];
}

FString UGameCenterNetConnection::LowLevelGetRemoteAddress(UBOOL bAppendPort)
{
	return TEXT("GKMATCH");
}

FString UGameCenterNetConnection::LowLevelDescribe()
{
	return TEXT("GKMatch Based connection");
}


// UNetDriver interface.
UBOOL UGameCenterNetDriver::InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error )
{
	// base class does most of this
	if (!Super::InitConnect(InNotify, ConnectURL, Error))
	{
		return FALSE;
	}

	// open a GC connection to the server
	ServerConnection = ConstructObject<UGameCenterNetConnection>(NetConnectionClass);

	// start it up!
	ServerConnection->InitConnection(this, NULL, FInternetIpAddr(), USOCK_Open, TRUE, ConnectURL);

	// Create channel zero.
	ServerConnection->CreateChannel( CHTYPE_Control, 1, 0 );

	return TRUE;
}

UBOOL UGameCenterNetDriver::InitListen( FNetworkNotify* InNotify, FURL& LocalURL, FString& Error )
{
	// nothing to do here, the GKMatch is already set up
	if (!Super::InitListen(InNotify, LocalURL, Error))
	{
		return FALSE;
	}

	return TRUE;
}

void UGameCenterNetDriver::TickDispatch( FLOAT DeltaTime )
{
	Super::TickDispatch( DeltaTime );

	@synchronized(GGameCenter.PendingMessages)
	{
		// pull data from the queue received on the iOS thread
		for (NSData* Data in GGameCenter.PendingMessages)
		{
			// process the data block

			// Figure out which socket the received data came from.
			UNetConnection* Connection = NULL;

			// @todo: Take the PlayerID on to the data blob!!
//			if (GetServerConnection() && GetServerConnection()->RemoteAddr == FromAddr)
			if (ServerConnection)
			{
				Connection = ServerConnection;
			}
			for( INT i=0; i<ClientConnections.Num() && !Connection; i++ )
			{
				// @todo 1v1 hack, just use first connection
//				if(((UTcpipConnection*)ClientConnections(i))->RemoteAddr == FromAddr)
				{
					Connection = (UTcpipConnection*)ClientConnections(i);
				}
			}


			if (!Connection)
			{
				// If we didn't find a client connection, maybe create a new one.
				if( !Connection && Notify->NotifyAcceptingConnection()==ACCEPTC_Accept )
				{
					Connection = ConstructObject<UGameCenterNetConnection>(NetConnectionClass);

					Connection->InitConnection( this, NULL, FInternetIpAddr(), USOCK_Open, FALSE, FURL() );
					Notify->NotifyAcceptedConnection( Connection );
					ClientConnections.AddItem( Connection );
				}
			}

			// Send the packet to the connection for processing.
			if( Connection )
			{
				// @todo ReceivedRawPacket should really take a const void* pointer :)
				Connection->ReceivedRawPacket( (void*)[Data bytes], [Data length]);
			}
		}
		
		// empty the array
		[GGameCenter.PendingMessages removeAllObjects];
	}
}

FString UGameCenterNetDriver::LowLevelGetNetworkNumber()
{
	return TEXT("");
}

void UGameCenterNetDriver::LowLevelDestroy()
{
	// @todo: maybe we should release the GKMatch here?
}

/** @return TRUE if the net resource is valid or FALSE if it should not be used */
UBOOL UGameCenterNetDriver::IsNetResourceValid(void)
{
	return GGameCenter.CurrentMatch != nil;
}

#endif
