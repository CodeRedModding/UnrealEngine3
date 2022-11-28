/*=============================================================================
	IpDrv.cpp: Unreal TCP/IP driver.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
	* Additions by Brandon Reinhart.
=============================================================================*/

#include "UnIpDrv.h"

/*-----------------------------------------------------------------------------
	AUdpLink implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(AUdpLink);

#define MAXRECVDATASIZE 4096

//
// Constructor.
//
AUdpLink::AUdpLink()
{
}

//
// PostScriptDestroyed.
//
void AUdpLink::PostScriptDestroyed()
{
	if( GetSocket() != INVALID_SOCKET )
		closesocket(GetSocket());
	Super::PostScriptDestroyed();
}

//
// BindPort: Binds a free port or optional port specified in argument one.
//
void AUdpLink::execBindPort( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT_OPTX(InPort,0);
	P_GET_UBOOL_OPTX(bUseNextAvailable,0);
	P_FINISH;
	if( GIpDrvInitialized )
	{
		if( GetSocket()==INVALID_SOCKET )
		{
			Socket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
			if( GetSocket() != INVALID_SOCKET )
			{
				UBOOL TrueBuffer=1;
				if( setsockopt( GetSocket(), SOL_SOCKET, SO_BROADCAST, (char*)&TrueBuffer, sizeof(TrueBuffer) )==0 )
				{
					sockaddr_in Addr;
					Addr.sin_family      = AF_INET;
					Addr.sin_addr        = getlocalbindaddr( Stack );
					Addr.sin_port        = htons(InPort);
					INT boundport = bindnextport( Socket, &Addr, bUseNextAvailable ? 20 : 1, 1 );
					if( boundport )
					{
						#if __UNIX__
						INT pd_flags;
						pd_flags = fcntl( Socket, F_GETFL, 0 );
						pd_flags |= O_NONBLOCK;
						if( fcntl( Socket, F_SETFL, pd_flags ) == 0 )
						#else
						DWORD NoBlock = 1;
						if( ioctlsocket( Socket, FIONBIO, &NoBlock )==0 )
						#endif
						{
							// Success.
							*(INT*)Result = boundport;
							Port = ntohs( Addr.sin_port );
							return;
						}
						else Stack.Logf( TEXT("BindPort: ioctlsocket failed") );
					}
					else Stack.Logf( TEXT("BindPort: bind failed") );
				}
				else Stack.Logf( TEXT("BindPort: setsockopt failed") );
			}
			else Stack.Logf( TEXT("BindPort: socket failed") );
			closesocket(GetSocket());
			GetSocket()=0;
		}
		else Stack.Logf( TEXT("BindPort: already bound") );
	}
	else Stack.Logf( TEXT("BindPort: winsock failed") );
	*(INT*)Result = 0;
}

//
// Send text in a UDP packet.
//
void AUdpLink::execSendText( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT(FIpAddr,IpAddr);
	P_GET_STR(Str);
	P_FINISH;
	if( GetSocket() != INVALID_SOCKET )
	{
		sockaddr_in Addr;
		Addr.sin_family      = AF_INET;
		Addr.sin_port        = htons(IpAddr.Port);
		Addr.sin_addr.s_addr = htonl(IpAddr.Addr);
		INT SentBytes = sendto( Socket, TCHAR_TO_ANSI(*Str), sizeof(ANSICHAR)*Str.Len(), MSG_NOSIGNAL, (sockaddr*)&Addr, sizeof(Addr) );
		if ( SentBytes == SOCKET_ERROR )
		{
            TCHAR* error = SocketError(WSAGetLastError());
            Stack.Logf( TEXT("SentText: sendto failed: %s"), error );
			*(DWORD*)Result = 0;
			return;
		}
		else
		{
			//debugf("Sent %i bytes.", SentBytes);
		}
	}
	*(DWORD*)Result = 1;
}

//
// Send binary data.
//
void AUdpLink::execSendBinary( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT(FIpAddr,IpAddr);
	P_GET_INT(Count);
	P_GET_ARRAY_REF(BYTE,B);
	P_FINISH;
	if( GetSocket() != INVALID_SOCKET )
	{
		sockaddr_in Addr;
		Addr.sin_family      = AF_INET;
		Addr.sin_port        = htons(IpAddr.Port);
		Addr.sin_addr.s_addr = htonl(IpAddr.Addr);
		if( sendto( Socket, (char*)B, Count, MSG_NOSIGNAL, (sockaddr*)&Addr, sizeof(Addr) ) == SOCKET_ERROR )
		{
			Stack.Logf( TEXT("SendBinary: sendto failed") );
			*(DWORD*)Result = 1;
			return;
		}
	}
	*(DWORD*)Result = 0;
}

//
// Time passes...
//
UBOOL AUdpLink::Tick( FLOAT DeltaTime, enum ELevelTick TickType )
{
	UBOOL Result = Super::Tick( DeltaTime, TickType );
	if( GetSocket() != INVALID_SOCKET )
	{
		if( ReceiveMode == RMODE_Event )
		{
			BYTE Buffer[MAXRECVDATASIZE];
			sockaddr_in FromAddr;
			__SIZE_T__ FromSize = sizeof(FromAddr);
			INT Count = recvfrom( GetSocket(), (char*)Buffer, ARRAY_COUNT(Buffer)-1, MSG_NOSIGNAL, (sockaddr*)&FromAddr, &FromSize );
			if( Count!=SOCKET_ERROR )
			{
				FIpAddr Addr;
				Addr.Addr = ntohl( FromAddr.sin_addr.s_addr );
				Addr.Port = ntohs( FromAddr.sin_port );
				if( LinkMode == MODE_Text )
				{
					Buffer[Count]=0;
					eventReceivedText( Addr, FString((ANSICHAR*)Buffer) );
				}
				else if ( LinkMode == MODE_Line )
				{
					Buffer[Count]=0;
					eventReceivedLine( Addr, FString((ANSICHAR*)Buffer) );
				}
				else if( LinkMode == MODE_Binary )
				{
					eventReceivedBinary( Addr, Count, (BYTE*)Buffer );
				}
			}
		}
		else if( ReceiveMode == RMODE_Manual )
		{
			fd_set SocketSet;
			TIMEVAL SelectTime = {0, 0};
			INT Error;

			FD_ZERO( &SocketSet );
			FD_SET( Socket, &SocketSet );
			Error = select( Socket + 1, &SocketSet, 0, 0, &SelectTime);
			if( Error==0 || Error==SOCKET_ERROR )
			{
				DataPending = 0;
			}
			else
			{
				DataPending = 1;
			}
		}
	}

	return Result;
}

//
// Read text.
//
void AUdpLink::execReadText( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT_REF( FIpAddr, Addr );
	P_GET_STR_REF( Str );
	P_FINISH;
	*Str = TEXT("");
	if( GetSocket() != INVALID_SOCKET )
	{
		BYTE Buffer[MAXRECVDATASIZE];
		sockaddr_in FromAddr;
		__SIZE_T__ FromSize = sizeof(FromAddr);
		INT BytesReceived = recvfrom( (SOCKET)Socket, (char*)Buffer, sizeof(Buffer), MSG_NOSIGNAL, (sockaddr*)&FromAddr, &FromSize );
		if( BytesReceived != SOCKET_ERROR )
		{
			Addr->Addr = ntohl( FromAddr.sin_addr.s_addr );
			Addr->Port = ntohs( FromAddr.sin_port );
			*Str = FString((ANSICHAR*)Buffer);
			*(DWORD*)Result = BytesReceived;
		}
		else
		{
			*(DWORD*) Result = 0;
			if ( WSAGetLastError() != WSAEWOULDBLOCK )
				debugf( NAME_Log, TEXT("ReadText: Error reading text.") );
			return;
		}
		return;
	}
	*(DWORD*)Result = 0;

}

//
// Read Binary.
//
void AUdpLink::execReadBinary( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT_REF(FIpAddr, Addr);
	P_GET_INT(Count);
	P_GET_ARRAY_REF(BYTE,B);
	P_FINISH;
	if( GetSocket() != INVALID_SOCKET )
	{
		sockaddr_in FromAddr;
		__SIZE_T__ FromSize = sizeof(FromAddr);
		INT BytesReceived = recvfrom( (SOCKET) Socket, (char*)B, Min(Count,255), MSG_NOSIGNAL, (sockaddr*)&FromAddr, &FromSize );
		if( BytesReceived != SOCKET_ERROR )
		{
			Addr->Addr = ntohl( FromAddr.sin_addr.s_addr );
			Addr->Port = ntohs( FromAddr.sin_port );
			*(DWORD*) Result = BytesReceived;
		}
		else
		{
			*(DWORD*)Result = 0;
			if( WSAGetLastError() != WSAEWOULDBLOCK )
				debugf( NAME_Log, TEXT("ReadBinary: Error reading text.") );
			return;
		}
		return;
	}
	*(DWORD*)Result = 0;

}

//
// Return the UdpLink's socket.  For master server NAT socket opening purposes.
//
FSocketData AUdpLink::GetSocketData()
{
	FSocketData Result;
	Result.Socket = GetSocket();
	Result.UpdateFromSocket();
	return Result;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

