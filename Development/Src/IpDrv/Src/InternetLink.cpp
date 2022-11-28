/*=============================================================================
	InternetLink.cpp: Unreal Internet Connection Superclass
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart
=============================================================================*/

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#include "UnIpDrv.h"

/*-----------------------------------------------------------------------------
	Defines.
-----------------------------------------------------------------------------*/

#define FTCPLINK_MAX_SEND_BYTES 4096

/*-----------------------------------------------------------------------------
	AInternetLink implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(AInternetLink);

//
// Constructor.
//
AInternetLink::AInternetLink()
{
	FString Error;
	InitSockets( Error );

	LinkMode     = MODE_Text;
	ReceiveMode  = RMODE_Event;
	DataPending  = 0;
	Port         = 0;
	Socket       = INVALID_SOCKET;
	RemoteSocket = INVALID_SOCKET;

}

//
// Destroy.
//
void AInternetLink::Destroy()
{
	if (GetSocket() != INVALID_SOCKET)
    {
        closesocket(GetSocket());
	    GetSocket()=INVALID_SOCKET;
    }
	Super::Destroy();
}

//
// Time passing.
//
UBOOL AInternetLink::Tick( FLOAT DeltaTime, enum ELevelTick TickType )
{
	UBOOL Result = Super::Tick( DeltaTime, TickType );

	if( GetResolveInfo() && GetResolveInfo()->Resolved() )
	{
		if( GetResolveInfo()->GetError() )
		{
			debugf( NAME_Log, TEXT("AInternetLink Resolve failed: %s"), GetResolveInfo()->GetError() );
			eventResolveFailed();
		}
		else
		{
			debugf( TEXT("Resolved %s (%s)"), GetResolveInfo()->GetHostName(), *IpString(GetResolveInfo()->GetAddr()) );
			FIpAddr Result;
			IpGetInt( GetResolveInfo()->GetAddr(), Result.Addr );
			Result.Addr = htonl( Result.Addr );
			Result.Port = 0;
			eventResolved( Result );
		}
		delete GetResolveInfo();
		GetResolveInfo() = NULL;
	}

	return Result;
}

//
// IsDataPending: Returns true if data is pending.
//
void AInternetLink::execIsDataPending( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	if ( DataPending ) {
		*(DWORD*)Result = 1;
		return;
	}

	*(DWORD*)Result = 0;
}

//
// ParseURL: Parses an Unreal URL into its component elements.
// Returns false if the URL was invalid.
//
void AInternetLink::execParseURL( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(URL);
	P_GET_STR_REF(Addr);
	P_GET_INT_REF(Port);
	P_GET_STR_REF(Level);
	P_GET_STR_REF(Portal);
	P_FINISH;

	FURL TheURL( 0, *URL, TRAVEL_Absolute );
	*Addr   = TheURL.Host;
	*Port   = TheURL.Port;
	*Level  = TheURL.Map;
	*Portal = TheURL.Portal;

	*(DWORD*)Result = 1;

}

//
// Resolve a domain or dotted IP.
// Nonblocking operation.
// Triggers Resolved event if successful.
// Triggers ResolveFailed event if unsuccessful.
//
void AInternetLink::execResolve( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Domain);
	P_FINISH;

	// If not, start asynchronous name resolution.
	DWORD addr = inet_addr(TCHAR_TO_ANSI(*Domain));
	if( addr==INADDR_NONE && Domain!=TEXT("255.255.255.255") )
	{
		// Success or failure will be called from Tick().
		GetResolveInfo() = new FResolveInfo( *Domain );
	}
	else if( addr == INADDR_NONE )
	{
		// Immediate failure.
		eventResolveFailed();
	}
	else
	{
		// Immediate success.
		FIpAddr Result;
		Result.Addr = htonl( addr );
		Result.Port = 0;
		eventResolved( Result );
	}
}

//
// Convert IP address to string.
//
void AInternetLink::execIpAddrToString( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT(FIpAddr,Arg);
	P_FINISH;

	//!!byte order dependence?
	*(FString*)Result = FString::Printf( TEXT("%i.%i.%i.%i:%i"), (BYTE)((Arg.Addr)>>24), (BYTE)((Arg.Addr)>>16), (BYTE)((Arg.Addr)>>8), (BYTE)((Arg.Addr)>>0), Arg.Port );

}

//
// Convert string to an IP address.
//
void AInternetLink::execStringToIpAddr( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Str);
	P_GET_STRUCT_REF(FIpAddr,IpAddr);
	P_FINISH;

	DWORD addr = inet_addr(TCHAR_TO_ANSI(*Str));
	if( addr!=INADDR_NONE )
	{
		IpAddr->Addr = htonl( addr );
		IpAddr->Port = 0;
		*(UBOOL*)Result = 1;
		return;
	}
	*(UBOOL*)Result = 0;
}

//
// Return most recent Winsock error.
//
void AInternetLink::execGetLastError( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(DWORD*)Result = WSAGetLastError();
}

//
// Return the local IP address
//
void AInternetLink::execGetLocalIP( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT_REF(FIpAddr,Arg);
	P_FINISH;

	in_addr LocalAddr;

	getlocalhostaddr( Stack, LocalAddr );
	IpGetInt( LocalAddr, Arg->Addr );
	Arg->Addr = htonl( Arg->Addr );
	Arg->Port = 0;
}

/*-----------------------------------------------------------------------------
	FInternetLink
-----------------------------------------------------------------------------*/

UBOOL FInternetLink::ThrottleSend = 0;
UBOOL FInternetLink::ThrottleReceive = 0;
INT FInternetLink::BandwidthSendBudget = 0;
INT FInternetLink::BandwidthReceiveBudget = 0;

void FInternetLink::ThrottleBandwidth( INT SendBudget, INT ReceiveBudget )
{
	ThrottleSend = SendBudget != 0;
	ThrottleReceive = ReceiveBudget != 0;

	// If we didn't spend all our sent bandwidth last timeframe, we don't get it back again.
	BandwidthSendBudget = SendBudget;

	// If we received more than our budget last timeframe, reduce this timeframe's budget accordingly.
    BandwidthReceiveBudget = Min<INT>( BandwidthReceiveBudget + ReceiveBudget, ReceiveBudget );
}

/*-----------------------------------------------------------------------------
	FTcpLink
-----------------------------------------------------------------------------*/

FTcpLink::FTcpLink()
:	LinkState(LINK_Closed)
,	LinkMode(TCPLINK_Raw)
,	ArSend(NULL)
,	ArRecv(NULL)
,	StatBytesSent(0)
,	StatBytesReceived(0)
,	ResolveInfo(NULL)
{
	FString Error;
	InitSockets( Error );
	SocketData.Socket = INVALID_SOCKET;
	LastTrafficTime = appSeconds();
}

FTcpLink::FTcpLink(FSocketData InSocketData)
:	FInternetLink(InSocketData)
,	LinkState(LINK_Connected)
,	LinkMode(TCPLINK_Raw)
,	ArSend(NULL)
,	ArRecv(NULL)
,	StatBytesSent(0)
,	StatBytesReceived(0)
,	ResolveInfo(NULL)
{
	LastTrafficTime = appSeconds();
}

FTcpLink::~FTcpLink()
{
	if( LinkState != LINK_Closed )
	{
		closesocket(SocketData.Socket);
		SocketData.Socket = INVALID_SOCKET;
	}
	if( ArSend )
		delete ArSend;
	if( ArRecv)
		delete ArRecv;
}

void FTcpLink::SetLinkMode( ETcpLinkMode InLinkMode )
{
	LinkMode = InLinkMode;

	switch( LinkMode )
	{
	case TCPLINK_FArchive:
		if( !ArSend )
			ArSend = new FArchiveTcpWriter(this);
		if( !ArRecv )
			ArRecv = new FArchiveTcpReader(this);
		break;
	default:
		if( ArSend )
		{
			delete ArSend;
			ArSend = NULL;
		}
		if( ArRecv )
		{
			delete ArRecv;
			ArRecv = NULL;
		}
		break;
	}
}

void FTcpLink::Listen( INT LocalPort )
{
	SocketData.Addr.sin_family = PF_INET;
	SocketData.Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	check( SocketData.Socket != INVALID_SOCKET );
	SetNonBlocking(SocketData.Socket);
	SetSocketReuseAddr( SocketData.Socket );

	SocketData.Port = LocalPort;
	SocketData.Addr.sin_port = htons(SocketData.Port);
	SocketData.Addr.sin_addr = getlocalbindaddr( *GWarn );

	if( bind(SocketData.Socket, (LPSOCKADDR)&SocketData.Addr, sizeof(struct sockaddr)) == SOCKET_ERROR )
		appErrorf(TEXT("Failed to bind listen socket"));

	if( listen( SocketData.Socket, 200/*SOMAXCONN*/ ) == SOCKET_ERROR )
		appErrorf(TEXT("Failed to listen on TCP socket"));

	warnf( TEXT("TCP socket %d listening on port %d"), SocketData.Socket, SocketData.Port );

	LinkState = LINK_Listening;

}

void FTcpLink::WaitForConnections( INT WaitTime )
{
	INT SelectStatus;
	TIMEVAL SelectTime = {WaitTime, 0};
	fd_set SocketSet;
	FD_ZERO( &SocketSet );
	FD_SET( SocketData.Socket, &SocketSet );

	SelectStatus = select( SocketData.Socket + 1, &SocketSet, 0, 0, &SelectTime );
	if ( SelectStatus == SOCKET_ERROR )
	{
		warnf( TEXT("!! Error checking socket status: %i"), WSAGetLastError());
		return;
	}
	else
	if ( SelectStatus == 0 )
	{
		// Nothing waiting.
		return;
	}

	// Accept the new connection.
	FSocketData ConnectionData;
	__SIZE_T__ i = sizeof(ConnectionData.Addr);
	ConnectionData.Socket = accept( SocketData.Socket, (LPSOCKADDR)&ConnectionData.Addr, &i );
	if ( ConnectionData.Socket == INVALID_SOCKET )
	{
		warnf( TEXT("!! Failed to accept queued connection: %i"), WSAGetLastError() );
		return;
	}

	// Announce the new connection.
	OnIncomingConnection( ConnectionData );

}

UBOOL FTcpLink::Poll( INT WaitTime )
{
	if( ResolveInfo )
	{
		if( ResolveInfo->Resolved() )
		{
			if( ResolveInfo->GetError() )
			{
				delete ResolveInfo;
				ResolveInfo = NULL;
				OnResolveFailed();
			}
			else
			{
				SOCKADDR_IN s;
				s.sin_addr	= ResolveInfo->GetAddr();
				s.sin_port  = htons(0);
				delete ResolveInfo;
				ResolveInfo = NULL;
				OnResolved( FIpAddr(s) );
			}
		}
	}

	TIMEVAL SelectTime = {WaitTime, 0};
	switch( LinkState )
	{
	case LINK_Connecting:
		{
			fd_set WritableSocketSet, ErrorSocketSet;
			FD_ZERO( &WritableSocketSet );
			FD_ZERO( &ErrorSocketSet );
			FD_SET( SocketData.Socket, &WritableSocketSet );
			FD_SET( SocketData.Socket, &ErrorSocketSet );

	     	// Check connection status
			INT SelectStatus = select( SocketData.Socket + 1, 0, &WritableSocketSet, &ErrorSocketSet, &SelectTime );
			if ( SelectStatus == SOCKET_ERROR )
			{
				warnf( TEXT("!! Error checking socket status: %i"), WSAGetLastError());
				return 0;
			}
			else
			if ( SelectStatus != 0 )
			{
				if( FD_ISSET( SocketData.Socket, &WritableSocketSet ) )
				{
					// Connected!
					LinkState = LINK_Connected;
					OnConnectionSucceeded();
					return 1;
				}

				if( FD_ISSET( SocketData.Socket, &ErrorSocketSet ) )
				{
					// Connection failed!
					LinkState = LINK_Closed;
					OnConnectionFailed();
					return 1;
				}
			}
		}
		return 1;
		break;
	case LINK_Connected:
		{
			fd_set ReadableSocketSet, WritableSocketSet;
			FD_ZERO( &ReadableSocketSet );
			FD_ZERO( &WritableSocketSet );

			UBOOL CheckReadable = HasBudgetToRecv();
			UBOOL CheckWritable = HasSendPending();

			if( CheckReadable )
				FD_SET( SocketData.Socket, &ReadableSocketSet );
			if( CheckWritable )
				FD_SET( SocketData.Socket, &WritableSocketSet );

			if( CheckReadable || CheckWritable )
			{
	     		// Check connection status
				INT SelectStatus = select( SocketData.Socket + 1, CheckReadable ? &ReadableSocketSet : 0, CheckWritable ? &WritableSocketSet : 0, 0, &SelectTime );
				if ( SelectStatus == SOCKET_ERROR )
				{
					warnf( TEXT("!! Error checking socket status: %i"), WSAGetLastError());
					return 0;
				}
				else
				if ( SelectStatus != 0 )
				{
					if( CheckReadable && !CheckWritable )
					{
//						warnf(TEXT("Socket is readable"));
						ReceivePendingData();
					}
					else
					if( CheckWritable && !CheckReadable )
					{
//						warnf(TEXT("Socket is writable"));
						SendPendingData();
					}
					else
					{
						if( FD_ISSET( SocketData.Socket, &WritableSocketSet ) )
						{
//							warnf(TEXT("Socket is writable"));
							SendPendingData();
						}
						if( FD_ISSET( SocketData.Socket, &ReadableSocketSet ) )
						{
//							warnf(TEXT("Socket is readable"));
							ReceivePendingData();
						}
					}
				}
			}
			else
			if( WaitTime )
				appSleep( WaitTime );
		}
		return 1;
		break;
	default:
		return 0;
		break;
	}

}

void FTcpLink::ReceivePendingData()
{
	if( !ThrottleReceive || BandwidthReceiveBudget > 0 )
	{
		BYTE Buf[1024];
		INT Count = recv( SocketData.Socket, (char*)Buf, sizeof(Buf), 0 );
		if( Count <= 0)
		{
			closesocket(SocketData.Socket);
			SocketData.Socket = INVALID_SOCKET;
			LinkState = LINK_Closed;
			OnClosed();
			return;
		}
		else
		{
			StatBytesReceived += Count;
			LastTrafficTime = appSeconds();
			if( ThrottleReceive )
				BandwidthReceiveBudget -= Count;

			do
			{
				INT i = ReceivedData.Add(Count);
				appMemcpy( &ReceivedData(i), Buf, Count );
				Count = recv( SocketData.Socket, (char*)Buf, sizeof(Buf), 0 );
				if( Count > 0 )
				{
					StatBytesReceived += Count;
					if( ThrottleReceive )
						BandwidthReceiveBudget -= Count;
				}
			} while( Count > 0 && (!ThrottleReceive || BandwidthReceiveBudget > 0) );

			// Notify.
			if( LinkMode == TCPLINK_FArchive )
			{
				ArRecv->ReceiveDataFromLink();
				if( ArRecv->CompletePacketsAvailable() )
					OnDataReceived();
			}
			else
				OnDataReceived();
		}
	}
}

void FTcpLink::Close( UBOOL Force )
{
	if( /*PendingSend.Num() &&*/ !Force )
	{
		// Can't close immediately as there's outgoing data to send.
		LinkState = LINK_ShutdownPending;
	}
	else
	{
		closesocket(SocketData.Socket);
		SocketData.Socket = INVALID_SOCKET;
		LinkState = LINK_Closed;
	}
}

void FTcpLink::Resolve( const TCHAR* Hostname )
{
	FIpAddr a( Hostname, 0 );
	if( a.Addr==INADDR_NONE )
		ResolveInfo = new FResolveInfo(Hostname);
	else
		OnResolved(a);
}

void FTcpLink::Connect( FIpAddr RemoteAddr )
{
	if( LinkState != LINK_Closed )
	{
		closesocket(SocketData.Socket);
		SocketData.Socket = INVALID_SOCKET;
		LinkState = LINK_Closed;
	}

	// create socket
	SocketData.Addr.sin_family = PF_INET;
	SocketData.Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	check( SocketData.Socket != INVALID_SOCKET );
	SetNonBlocking(SocketData.Socket);

	// Bind local address
	SocketData.Addr.sin_family	= AF_INET;
	SocketData.Addr.sin_addr	= getlocalbindaddr( *GWarn );
	SocketData.Addr.sin_port	= htons(0);
	SocketData.Port				= 0;

	SetSocketReuseAddr( SocketData.Socket );

	if( bind(SocketData.Socket, (LPSOCKADDR)&SocketData.Addr, sizeof(struct sockaddr)) == SOCKET_ERROR )
		appErrorf(TEXT("Failed to bind connect socket"));

	// Update bind address.
	SocketData.UpdateFromSocket();
//	warnf( TEXT("TCP: Bound to local port %d"), SocketData.Port );

	SOCKADDR_IN RemoteHost;
	RemoteHost.sin_family		= AF_INET;
	RemoteHost.sin_port			= htons(RemoteAddr.Port);
	RemoteHost.sin_addr.s_addr	= htonl(RemoteAddr.Addr);

	INT Result = connect( SocketData.Socket, (SOCKADDR*)&RemoteHost, sizeof(RemoteHost) );
	if( Result == SOCKET_ERROR )
	{
        #if __linux__
            int err = WSAGetLastError();
            if ((err != EINPROGRESS) && (err != EWOULDBLOCK))
    		    warnf( TEXT("Connect() returned SOCKET_ERROR: %d"), err );
        #else
			if( WSAGetLastError() != WSAEWOULDBLOCK )
				warnf( TEXT("Connect() returned SOCKET_ERROR: %d"), WSAGetLastError() );
        #endif
	}

	LinkState = LINK_Connecting;

}

void FTcpLink::SendPendingData()
{
	if( LinkState != LINK_Connected &&
        LinkState != LINK_ShutdownPending &&
        LinkState != LINK_ClosePending )
    {
		return;
    }

	while( PendingSend.Num() && (!ThrottleSend || BandwidthSendBudget > 0) )
	{
		INT SendMax = Min<INT>(PendingSend.Num(), FTCPLINK_MAX_SEND_BYTES);
		if( ThrottleSend )
			SendMax = Min<INT>(SendMax, BandwidthSendBudget);
		INT sent = send( SocketData.Socket, (const char*)&PendingSend(0), SendMax, 0 );
		if( sent == PendingSend.Num() )
		{
			StatBytesSent += sent;
			LastTrafficTime = appSeconds();
			if( ThrottleSend )
				BandwidthSendBudget -= sent;
			PendingSend.Empty();
		}
		else
		if( sent == SOCKET_ERROR )
		{
			if( WSAGetLastError() == WSAEWOULDBLOCK )
				break;
			else
			{
				warnf(TEXT("!!SendPendingData() got SOCKET_ERROR: %d"), WSAGetLastError());
				// drop connection
				LinkState = LINK_ShutdownPending;
				PendingSend.Empty();
			}
		}
		else
		{
//			warnf(TEXT("SendPendingData() send %d of %d bytes"), sent, PendingSend.Num() );
			PendingSend.Remove(0,sent);
			StatBytesSent += sent;
			LastTrafficTime = appSeconds();
			if( ThrottleSend )
				BandwidthSendBudget -= sent;
		}
	}

	if( LinkState == LINK_ShutdownPending && !PendingSend.Num() )
	{
		shutdown(SocketData.Socket, 2);
        LinkState = LINK_ClosePending;
	}

    if( LinkState == LINK_ClosePending )
	{
		BYTE Buf[1024];
		if (recv( SocketData.Socket, (char*)Buf, sizeof(Buf), 0 ) <= 0)
        {
            closesocket(SocketData.Socket);
            LinkState = LINK_Closed;
            SocketData.Socket = INVALID_SOCKET;
        }
	}
}

INT FTcpLink::Send( BYTE* Data, INT Count )
{
	if( LinkState != LINK_Connected )
		return 0;

	INT sent = 0;
	if( PendingSend.Num()==0 && (!ThrottleSend || BandwidthSendBudget > 0) )
	{
		INT SendMax = ThrottleSend ? Min<INT>(Count, BandwidthSendBudget) : Count;
		sent = send( SocketData.Socket, (const char*)Data, SendMax, 0 );
		if( sent == SOCKET_ERROR )
		{
			if( WSAGetLastError() != WSAEWOULDBLOCK )
				warnf(TEXT("!!Send() got SOCKET_ERROR %d"), WSAGetLastError());
			sent = 0;
		}
	}

	// If we couldn't send it all immediately, buffer.
	if( sent != Count )
	{
		INT i = PendingSend.Add(Count-sent);
		appMemcpy( &PendingSend(i), &Data[sent], Count-sent );
	}

	StatBytesSent += sent;
	LastTrafficTime = appSeconds();
	if( ThrottleSend )
		BandwidthSendBudget -= sent;

	return Count;
}

INT FTcpLink::Recv( BYTE* Data, INT Count )
{
	Count = Max( Min( Count, ReceivedData.Num() ), 0 );
	if( Count )
		appMemcpy( Data, &ReceivedData(0), Count );
	ReceivedData.Remove(0, Count);
	return Count;
}

void FTcpLink::PeekData( BYTE*& Data, INT& Count )
{
	Count = ReceivedData.Num();
	Data = &ReceivedData(0);
}

/*-----------------------------------------------------------------------------
	FArchiveTcpReader - read from a TCP socket as an FArchive.
-----------------------------------------------------------------------------*/

FArchiveTcpReader::FArchiveTcpReader( FTcpLink* InLink  )
:	FArchiveTcpSocket( InLink )
{
	ArIsLoading = 1;
	AtPacketEnd = 0;
}

void FArchiveTcpReader::Serialize( void* V, INT Length )
{
	if( !Packets.Num() ||
		Packets(0).Length != Packets(0).PacketData.Num() ||
		Packets(0).Length < Length )
	{
		ArIsError = 1;
		return;
	}

    appMemcpy( V, &Packets(0).PacketData(0), Length );
	Packets(0).PacketData.Remove( 0, Length );
	Packets(0).Length -= Length;

	if( Packets(0).Length == 0 )
	{
		Packets.Remove(0);
		AtPacketEnd = 1;
	}
	else
		AtPacketEnd = 0;
}

UBOOL FArchiveTcpReader::AtEnd()
{
	return AtPacketEnd;
}

void FArchiveTcpReader::ReceiveDataFromLink()
{
	INT Avail;
	while( (Avail=Link->InternalDataAvailable()) > 0 )
	{
		INT PacketsNum = Packets.Num();
		if( PacketsNum && Packets(PacketsNum-1).PacketData.Num() < Packets(PacketsNum-1).Length )
		{
			// Append to existing packet
			FArchiveTcpReaderPacket* Packet = &Packets(PacketsNum-1);
			INT l = Min( Avail, Packet->Length - Packet->PacketData.Num() );
			INT i = Packet->PacketData.Add(l);
			Link->Recv( &Packet->PacketData(i), l );
			//warnf(TEXT("Received packet data: %d"), l );
		}
		else
		{
			if( Avail < sizeof(INT) )
				return;

			// Create new packet
			FArchiveTcpReaderPacket* Packet = &Packets( Packets.AddZeroed() );
			// Read the packet length
			Link->Recv( (BYTE*)&Packet->Length, sizeof(INT) );
			//warnf(TEXT("Received packet: length=%d"), Packet->Length );
		}
	}
}

INT FArchiveTcpReader::CompletePacketsAvailable()
{
	INT Count = 0, IncompleteCount = 0;
	for( INT i=0;i<Packets.Num();i++ )
	{
		if( Packets(i).Length == Packets(i).PacketData.Num() )
			Count++;
		else
			IncompleteCount++;
	}
	return Count;
}

/*-----------------------------------------------------------------------------
	FArchiveTcpWriter - write to a TCP socket as an FArchive.
-----------------------------------------------------------------------------*/

FArchiveTcpWriter::FArchiveTcpWriter( FTcpLink* InLink  )
:	FArchiveTcpSocket( InLink )
{
	// Save space for packet length.
	SendData.Add(sizeof(INT));
	ArIsSaving = 1;
}

FArchiveTcpWriter::~FArchiveTcpWriter()
{
    Flush();
}

void FArchiveTcpWriter::Serialize( void* V, INT Length )
{
	INT i = SendData.Add(Length);
	appMemcpy( &SendData(i), V, Length );
}

void FArchiveTcpWriter::Flush()
{
	if( SendData.Num() > sizeof(INT) )
	{
		// Update packet length
		INT PacketSize = SendData.Num() - sizeof(INT);
		appMemcpy( &SendData(0), &PacketSize, sizeof(INT) );

		Link->Send( &SendData(0), SendData.Num() );

		//warnf(TEXT("Sending packet: %d bytes"), SendData.Num()-sizeof(INT) );
		SendData.Empty();
		SendData.Add(sizeof(INT));
	}
}

/*-----------------------------------------------------------------------------
	FUdpLink
-----------------------------------------------------------------------------*/

FUdpLink::FUdpLink()
:	ExternalSocket(0)
,	StatBytesSent(0)
,	StatBytesReceived(0)
{
	FString Error;
	InitSockets( Error );
	SocketData.Addr.sin_family = PF_INET;
	SocketData.Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
//	check( SocketData.Socket != INVALID_SOCKET );	// triggered in the field!

	SetSocketReuseAddr( SocketData.Socket );
	SetNonBlocking( SocketData.Socket );
	SetSocketRecvErr( SocketData.Socket );
}

FUdpLink::FUdpLink(FSocketData InSocketData)
:	FInternetLink(InSocketData)
,	ExternalSocket(1)
,	StatBytesSent(0)
,	StatBytesReceived(0)
{
}

FUdpLink::~FUdpLink()
{
	if( !ExternalSocket )
	{
//		warnf(TEXT("Closing UDP socket %d"), SocketData.Port);
		closesocket( SocketData.Socket );
		SocketData.Socket = INVALID_SOCKET;
	}
}

UBOOL FUdpLink::BindPort( INT InPort )
{
	SocketData.Port = InPort;
	SocketData.Addr.sin_port = htons(SocketData.Port);
	SocketData.Addr.sin_addr = getlocalbindaddr( *GWarn );

	UBOOL TrueBuffer=1;
	if( setsockopt( SocketData.Socket, SOL_SOCKET, SO_BROADCAST, (char*)&TrueBuffer, sizeof(TrueBuffer) )!=0 )
		appErrorf(TEXT("FUdpLink::BindPort: setsockopt failed"));

	if( bind(SocketData.Socket, (LPSOCKADDR)&SocketData.Addr, sizeof(struct sockaddr)) == SOCKET_ERROR )
		appErrorf(TEXT("FUdpLink::BindPort: Failed to bind UDP socket to port"));


	// if 0, read the address we bound from the socket.
	if( InPort == 0 )
		SocketData.UpdateFromSocket();

//	warnf(TEXT("UDP: bound to local port %d"), SocketData.Port);

	return 1;
}

INT FUdpLink::SendTo( FIpAddr Destination, BYTE* Data, INT Count )
{
	SOCKADDR_IN SockAddr = Destination.GetSockAddr();
	INT Result = sendto( SocketData.Socket, (const char *)Data, Count, 0, (LPSOCKADDR)&SockAddr, sizeof(SockAddr) );
	if( Result < 0)
		warnf(TEXT("SendTo: %s returned %d: %d"), *Destination.GetString(1), Result, WSAGetLastError());
	StatBytesSent += Count;
	return Result;
}

void FUdpLink::Poll()
{
	BYTE Buffer[4096];
	SOCKADDR_IN SockAddr;
	SOCKLEN SockAddrLen = sizeof(SockAddr);

	for(;;)
	{
		INT Result = recvfrom( SocketData.Socket, (char*)Buffer, sizeof(Buffer), 0, (struct sockaddr*)&SockAddr, &SockAddrLen );
		if( Result == SOCKET_ERROR )
		{
			if( WSAGetLastError() == WSAEWOULDBLOCK )
				break;
			else
			if( WSAGetLastError() != WSAECONNRESET )		// WSAECONNRESET means we got an ICMP unreachable, and should continue calling recv()
			{
				warnf(TEXT("RecvFrom returned SOCKET_ERROR %d"), WSAGetLastError() );
				break;
			}
		}
		else
		if( Result > 0 )
		{
			StatBytesReceived += Result;
			OnReceivedData( FIpAddr(SockAddr), Buffer, Result );
		}
		else
			break;
	}
}

/*----------------------------------------------------------------------------
	FArchiveUdpReader implementation
----------------------------------------------------------------------------*/

FArchiveUdpReader::FArchiveUdpReader( BYTE* InData, INT InLength )
:	Data(InData)
,	Length(InLength)
{
	ArIsLoading = 1;

	//!! OLDVER
	INT Version;
	if( Length < sizeof(Version) )
	{
		ArVer = 0;
		ArNetVer = 0x80000000;
		Length = 0;
	}
	else
	{
		appMemcpy( &Version, Data, sizeof(Version) );
		Data += sizeof(Version);
		Length -= sizeof(Version);

		ArVer = Version;
		ArNetVer = Version | 0x80000000;
	}
}
void FArchiveUdpReader::Serialize( void* V, INT l )
{
	if( l <= Length )
	{
        appMemcpy( V, Data, l );
		Data += l;
		Length -= l;
	}
	else
	{
		ArIsError = 1;
		Length = 0;
	}
}
UBOOL FArchiveUdpReader::AtEnd()
{
	return Length==0;
}

/*----------------------------------------------------------------------------
	FArchiveUdpWriter implementation
----------------------------------------------------------------------------*/

FArchiveUdpWriter::FArchiveUdpWriter( FUdpLink* InLink, FIpAddr InDest )
:	Link(InLink)
,	Dest(InDest)
{
	ArIsSaving = 1;
	INT V = Ver();
	SendData.Add(sizeof(INT));
	appMemcpy( &SendData(0), &V, sizeof(INT) );
}
void FArchiveUdpWriter::Serialize( void* V, INT Length )
{
	INT i = SendData.Add(Length);
	appMemcpy( &SendData(i), V, Length );
}
void FArchiveUdpWriter::Flush()
{
	if( SendData.Num() )
	{
		Link->SendTo( Dest, &SendData(0), SendData.Num() );
		SendData.Empty();
	}
	INT V = Ver();
	SendData.Add(sizeof(INT));
	appMemcpy( &SendData(0), &V, sizeof(INT) );
}
INT FArchiveUdpWriter::Tell()
{
	return SendData.Num();
}

/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/

