/*=============================================================================
	InternetLink.cpp: Unreal Internet Connection Superclass
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

/*-----------------------------------------------------------------------------
	Defines.
-----------------------------------------------------------------------------*/

#define FTCPLINK_MAX_SEND_BYTES 4096

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
	FUdpLink
-----------------------------------------------------------------------------*/

FUdpLink::FUdpLink()
:	ExternalSocket(0)
,	StatBytesSent(0)
,	StatBytesReceived(0)
{
	SocketData.Socket = GSocketSubsystem ? GSocketSubsystem->CreateDGramSocket(TEXT("UdpLink"),TRUE) : NULL;
	if (SocketData.Socket != NULL)
	{
		SocketData.Socket->SetReuseAddr();
		SocketData.Socket->SetNonBlocking();
		SocketData.Socket->SetRecvErr();
	}
}

FUdpLink::FUdpLink(const FSocketData &InSocketData)
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
		GSocketSubsystem->DestroySocket(SocketData.Socket);
		SocketData.Socket = NULL;
	}
}

UBOOL FUdpLink::BindPort(INT InPort)
{
	if (SocketData.Socket == NULL)
	{
		warnf(TEXT("FUdpLink::BindPort: Socket was not created"));
		return FALSE;
	}
	
	SocketData.Port = InPort;
	SocketData.Addr.SetPort(InPort);
	SocketData.Addr.SetIp(getlocalbindaddr( *GWarn ));

	if (SocketData.Socket == NULL)
	{
		warnf(TEXT("FUdpLink::BindPort: Socket was not created"));
		return FALSE;
	}

	if (SocketData.Socket->SetBroadcast() == FALSE)
	{
		warnf(TEXT("FUdpLink::BindPort: setsockopt failed"));
		return FALSE;
	}

	if (SocketData.Socket->Bind(SocketData.Addr) == FALSE)
	{
		warnf(TEXT("FUdpLink::BindPort: setsockopt failed"));
		return FALSE;
	}

	// if 0, read the address we bound from the socket.
	if( InPort == 0 )
	{
		SocketData.UpdateFromSocket();
	}

//	warnf(TEXT("UDP: bound to local port %d"), SocketData.Port);

	return TRUE;
}

INT FUdpLink::SendTo( FIpAddr Destination, BYTE* Data, INT Count )
{
	UBOOL bOk = FALSE;
	INT BytesSent = 0;

	if (SocketData.Socket == NULL)
	{
		return FALSE;
	}

	bOk = SocketData.Socket->SendTo(Data,Count,BytesSent,Destination.GetSocketAddress());
	if (bOk == FALSE)
	{
		warnf(TEXT("SendTo: %s returned %d: %s"), *Destination.ToString(TRUE),
			GSocketSubsystem->GetLastErrorCode(),
			GSocketSubsystem->GetSocketError());
	}
	StatBytesSent += BytesSent;
	return bOk;
}

void FUdpLink::Poll()
{
	BYTE Buffer[4096];
	FInternetIpAddr SockAddr;

	if (SocketData.Socket == NULL)
	{
		return;
	}
	for(;;)
	{
		INT BytesRead = 0;
		UBOOL bOk = SocketData.Socket->RecvFrom(Buffer,sizeof(Buffer),BytesRead,SockAddr);
		if( bOk == FALSE )
		{
			INT LastSocketError = GSocketSubsystem->GetLastErrorCode();
			if (LastSocketError  == SE_EWOULDBLOCK)
			{
				break;
			}
			else if (LastSocketError == SE_NO_ERROR)
			{
				break;
			}
			else
			// SE_ECONNRESET means we got an ICMP unreachable, and should continue calling recv()
			if( GSocketSubsystem->GetLastErrorCode() != SE_ECONNRESET )
			{
#if PS3
				if (GSocketSubsystem->GetLastErrorCode() == SE_ETIMEDOUT) // ETIMEDOUT, apparently an improper error
				{
					// ignore this error
				}
				else
#endif
				{
#if !WIIU
					warnf(TEXT("RecvFrom returned SOCKET_ERROR %s [socket = %d:%d]"),
						GSocketSubsystem->GetSocketError(), SocketData.Socket, SocketData.Port );
#endif
				}
				break;
			}
		}
		else
		if( BytesRead > 0 )
		{
			StatBytesReceived += BytesRead;
			const FIpAddr& Addr = SockAddr.GetAddress();
			OnReceivedData(Addr,Buffer,BytesRead);
		}
		else
			break;
	}
}

/*-----------------------------------------------------------------------------
	AInternetLink implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(AInternetLink);

//
// Constructor.
//
AInternetLink::AInternetLink()
{
	//Socket Init now handled by Subsystem::Initialize()
	LinkMode     = MODE_Text;
	ReceiveMode  = RMODE_Event;
	DataPending  = 0;
	Port         = 0;
	Socket       = NULL;
	RemoteSocket = NULL;
}

//
// Destroy.
//
void AInternetLink::BeginDestroy()
{
	if( Socket != NULL )
	{
		GSocketSubsystem->DestroySocket(Socket);
		Socket = NULL;
	}
	if( RemoteSocket != NULL )
	{
		GSocketSubsystem->DestroySocket(RemoteSocket);
		RemoteSocket = NULL;
	}
	Super::BeginDestroy();
}

//
// Time passing.
//
UBOOL AInternetLink::Tick( FLOAT DeltaTime, enum ELevelTick TickType )
{
	UBOOL Result = Super::Tick( DeltaTime, TickType );

	if( GetResolveInfo() && GetResolveInfo()->IsComplete() )
	{
		if( GetResolveInfo()->GetErrorCode() != SE_NO_ERROR )
		{
			// AInternetLink name resolution just now failed.
			debugf( NAME_Log, TEXT("AInternetLink Resolve failed with error code %d"), GetResolveInfo()->GetErrorCode() );
			eventResolveFailed();
		}
		else
		{
			// Host name resolution just now succeeded.
			FIpAddr Result(GetResolveInfo()->GetResolvedAddress());
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
UBOOL AInternetLink::IsDataPending()
{
	if ( DataPending ) {

		return TRUE;
	}

	return FALSE;
}

//
// ParseURL: Parses an Unreal URL into its component elements.
// Returns false if the URL was invalid.
//
UBOOL AInternetLink::ParseURL(const FString& URL,FString& Addr,INT& PortNum,FString& LevelName,FString& EntryName)
{
	FURL TheURL( 0, *URL, TRAVEL_Absolute );
	Addr   = TheURL.Host;
	Port   = TheURL.Port;
	LevelName = TheURL.Map;
	EntryName = TheURL.Portal;

	return TRUE;
}

//
// Resolve a domain or dotted IP.
// Nonblocking operation.
// Triggers Resolved event if successful.
// Triggers ResolveFailed event if unsuccessful.
//
void AInternetLink::Resolve(const FString& Domain)
{
	// If not, start asynchronous name resolution.
	UBOOL bIsValidIp = FALSE;
	// See if the IP needs a DNS look up or not
	FInternetIpAddr Result;
	Result.SetIp(*Domain,bIsValidIp);
	// If the IP address was valid we don't need to resolve
	if (bIsValidIp)
	{
		eventResolved( Result );

		if ( GetResolveInfo() != NULL )
		{
			// Resolve info is not needed here
			delete GetResolveInfo();
			GetResolveInfo() = NULL;
		}
	}
	else
	{
		// Otherwise kick off a DNS lookup (async)
		if ( GetResolveInfo() == NULL )
		{
			GetResolveInfo() = GSocketSubsystem->GetHostByName(TCHAR_TO_ANSI(*Domain));
		}
		debugf(NAME_DevNet,TEXT("Performing DNS lookup for %s"),*Domain);
	}
}

//
// Convert IP address to string.
//
FString AInternetLink::IpAddrToString(FIpAddr Arg)
{
	return Arg.ToString(TRUE);
}

//
// Convert string to an IP address.
//
UBOOL AInternetLink::StringToIpAddr(const FString& Str,FIpAddr& Addr)
{
	UBOOL bIsValidIp = FALSE;
	FInternetIpAddr Result;
	Result.SetIp(*Str,bIsValidIp);
	if (bIsValidIp)
	{
		Result.SetPort(0);
		Result.GetIp(Addr);
	}
	return bIsValidIp;
}

//
// Return most recent Winsock error.
//
INT AInternetLink::GetLastError()
{
	return GSocketSubsystem->GetLastErrorCode();
}


//
// Return the local IP address
//
void AInternetLink::GetLocalIP(FIpAddr& Arg)
{
	FInternetIpAddr LocalAddr;

	GSocketSubsystem->GetLocalHostAddr( *GLog, LocalAddr );
	Arg = LocalAddr.GetAddress();
}

#endif	//#if WITH_UE3_NETWORKING
