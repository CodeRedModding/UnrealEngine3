/*=============================================================================
	IpDrv.cpp: Unreal TCP/IP driver.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Implemented by M.Michon
	* Created by Tim Sweeney.
	* Rewritten by Brandon Reinhart.
	* Multiple inbound connections added by Jack Porter.
=============================================================================*/

#include "UnIpDrv.h"

/*-----------------------------------------------------------------------------
	ATcpLink functions.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(ATcpLink);

//
// Constructor.
//
ATcpLink::ATcpLink()
{
	LinkState = STATE_Initialized;
}

//
// PostScriptDestroyed.
//
void ATcpLink::PostScriptDestroyed()
{
	if( GetSocket() != INVALID_SOCKET )
		closesocket(GetSocket());
	if( RemoteSocket != INVALID_SOCKET )
		closesocket(RemoteSocket);
	Super::PostScriptDestroyed();
}

//
// BindPort: Binds a free port or optional port specified in argument one.
//
void ATcpLink::execBindPort( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT_OPTX(InPort,0);
	P_GET_UBOOL_OPTX(bUseNextAvailable,0);
	P_FINISH;

	if( GIpDrvInitialized )
	{
		if( GetSocket() == INVALID_SOCKET )
		{
			Socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
			SetSocketReuseAddr( GetSocket() );
			if( GetSocket() != INVALID_SOCKET )
			{
				//if( SetSocketLinger( GetSocket()) )
				{
					sockaddr_in Addr;
					Addr.sin_family      = AF_INET;
					Addr.sin_addr        = getlocalbindaddr( Stack );
					Addr.sin_port        = htons(InPort);
					INT boundport = bindnextport( Socket, &Addr, bUseNextAvailable ? 20 : 1, 1 );
					if( boundport )
					{
						if( SetNonBlocking( Socket ) )
						{
							// Successfully bound the port.
							Port = ntohs( Addr.sin_port );
							LinkState = STATE_Ready;
							*(INT*)Result = boundport;
							return;
						}
						else Stack.Logf( TEXT("BindPort: ioctlsocket or fcntl failed") );
					}
					else Stack.Logf( TEXT("BindPort: bind failed") );
				}
				//else Stack.Logf( TEXT("BindPort: setsockopt failed") );
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
// IsConnected: Returns true if connected.
//
void ATcpLink::execIsConnected( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	fd_set SocketSet;
	TIMEVAL SelectTime = {0, 0};
	INT Error;

	if ( LinkState == STATE_Initialized )
	{
		*(DWORD*)Result = 0;
		return;
	}

	if ( (LinkState == STATE_Listening) && (GetSocket() != INVALID_SOCKET) )
	{
		FD_ZERO( &SocketSet );
		FD_SET( RemoteSocket, &SocketSet );
		Error = select( RemoteSocket + 1, 0, &SocketSet, 0, &SelectTime );
		if (( Error != SOCKET_ERROR ) && ( Error != 0 )) {
			*(DWORD*)Result = 1;
			return;
		}
	}

	if ( GetSocket() != INVALID_SOCKET )
	{
		FD_ZERO( &SocketSet );
		FD_SET( Socket, &SocketSet );
		Error = select( Socket + 1, 0, &SocketSet, 0, &SelectTime );
		if (( Error != SOCKET_ERROR ) && ( Error != 0 )) {
			*(DWORD*)Result = 1;
			return;
		}
	}

	*(DWORD*)Result = 0;
}

//
// Listen: Puts this object in a listening state.
//
void ATcpLink::execListen( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	INT Error;
	if ( GIpDrvInitialized && (GetSocket() != INVALID_SOCKET) )
	{
		if( LinkState != STATE_ListenClosing )
		{
			if ( LinkState != STATE_Ready ) {
				*(DWORD*)Result = 0;
				return;
			}
			Error = listen(Socket, AcceptClass ? 10 : 1);
			if (Error == SOCKET_ERROR)
			{
				Stack.Logf( TEXT("Listen: listen failed") );
				*(DWORD*)Result = 0;
				return;
			}
		}
		LinkState = STATE_Listening;
		SendFIFO.Empty();
		*(DWORD*)Result = 1;
		return;
	}

	*(DWORD*)Result = 1;

}

//
// Time passes...
//
UBOOL ATcpLink::Tick( FLOAT DeltaTime, enum ELevelTick TickType )
{
	UBOOL Result = Super::Tick( DeltaTime, TickType );

	if( GetSocket() != INVALID_SOCKET )
	{
		switch( LinkState )
		{
			case STATE_Initialized:
			case STATE_Ready:
				break;
			case STATE_Listening:
				// We are listening, so first we check for connections
				// that want to be accepted.
				CheckConnectionQueue();
				// Then we poll each connection for queued input.
				PollConnections();
				FlushSendBuffer();
				break;
			case STATE_Connecting:
				// We are trying to connect, so find out if the connection
				// attempt completed.
				CheckConnectionAttempt();
				PollConnections();
				break;
			case STATE_Connected:
				PollConnections();
				FlushSendBuffer();
				break;
			case STATE_ConnectClosePending:
			case STATE_ListenClosePending:
				PollConnections();
				if(!FlushSendBuffer())
					ShutdownConnection();
				break;
		}
	}

	INT* CheckSocket;
	switch( LinkState )
	{
		case STATE_Connecting:
			return Result;
		case STATE_ConnectClosing:
		case STATE_ConnectClosePending:
		case STATE_Connected:
			CheckSocket = &Socket;
			break;
		case STATE_ListenClosing:
		case STATE_ListenClosePending:
		case STATE_Listening:
			CheckSocket = &RemoteSocket;
			break;
		default:
			return Result;
	}
	if (*CheckSocket != INVALID_SOCKET)
	{
		// See if the socket needs to be closed.
		TIMEVAL SelectTime = {0, 0};
		SOCKET s;
		fd_set SocketSet;
		FD_ZERO(&SocketSet);
		FD_SET( *CheckSocket, &SocketSet );
		s = select( *CheckSocket + 1, &SocketSet, 0, 0, &SelectTime );
		if (( s != SOCKET_ERROR ) && ( s != 0 ))
		{
			u_long numbytes;
			ioctlsocket( *CheckSocket, FIONREAD, &numbytes );
			if (numbytes == 0)
			{
				// Disconnect
				if (LinkState != STATE_Listening)
					LinkState = STATE_Initialized;
				closesocket(*CheckSocket);
				*CheckSocket = INVALID_SOCKET;
				eventClosed();
			}
			if (numbytes == SOCKET_ERROR)
				if (WSAGetLastError() != WSAEWOULDBLOCK)
				{
					// Socket error, disconnect.
					if (LinkState != STATE_Listening)
						LinkState = STATE_Initialized;
					closesocket(*CheckSocket);
					*CheckSocket = INVALID_SOCKET;
					eventClosed();
				}
		}
	}

	return Result;
}

//
// CheckConnectionQueue: Called during Tick() if LinkState
// is STATE_Listening.  Checks the listen socket's pending
// connection queue for connection requests.  Creates
// a new connection if one is available.
//
void ATcpLink::CheckConnectionQueue()
{
	fd_set SocketSet;
	TIMEVAL SelectTime = {0, 0};
	INT Error;
	INT NewSocket;
	SOCKADDR_IN ForeignHost;

	FD_ZERO( &SocketSet );
	FD_SET( GetSocket(), &SocketSet );
	// If listening, check for a queued connection to accept.
	Error = select( GetSocket() + 1, &SocketSet, 0, 0, &SelectTime);
	if ( Error == SOCKET_ERROR ) {
		debugf( NAME_Log, TEXT("CheckConnectionQueue: Error while checking socket status. %i"), WSAGetLastError());
		return;
	}
	if ( Error == 0 ) {
		// debugf( NAME_Log, "CheckConnectionQueue: No connections waiting." );
		return;
	}
	__SIZE_T__ i = sizeof(SOCKADDR);
	NewSocket = accept( Socket, (SOCKADDR*) &ForeignHost, &i );
	if ( NewSocket == INVALID_SOCKET ) {
		debugf( NAME_Log, TEXT("CheckConnectionQueue: Failed to accept queued connection: %i"), WSAGetLastError() );
		return;
	}

	if ( !AcceptClass && RemoteSocket != INVALID_SOCKET ) {
		debugf( NAME_Log, TEXT("Discarding redundant connection attempt.") );
		debugf( NAME_Log, TEXT("Current socket handle is %i"), RemoteSocket);
		return;
	}

	SetNonBlocking(NewSocket);

	if( AcceptClass )
	{
		if( AcceptClass->IsChildOf(ATcpLink::StaticClass()) )
		{
			ATcpLink *Child = Cast<ATcpLink>( GetLevel()->SpawnActor( AcceptClass, NAME_None, Location, Rotation, NULL, 0,0,this,Instigator ) );
			if (!Child)
			  debugf( NAME_Log, TEXT("Could not spawn AcceptClass!") );
			else
			{
			  Child->LinkState = STATE_Connected;
			  Child->LinkMode = LinkMode;
			  Child->Socket = NewSocket;
			  IpGetInt( ForeignHost.sin_addr, Child->RemoteAddr.Addr );
			  Child->RemoteAddr.Addr = htonl( Child->RemoteAddr.Addr );
			  Child->RemoteAddr.Port = ForeignHost.sin_port;
			  Child->eventAccepted();
			}
		}
		else
			debugf( NAME_Log, TEXT("AcceptClass is not a TcpLink!") );

		return;
	}

	RemoteSocket = NewSocket;
	IpGetInt( ForeignHost.sin_addr, RemoteAddr.Addr );
	RemoteAddr.Addr = htonl( RemoteAddr.Addr );
	RemoteAddr.Port = ForeignHost.sin_port;
	eventAccepted();

}

//
// PollConnections: Poll a connection for pending data.
//
void ATcpLink::PollConnections()
{
	if ( ReceiveMode == RMODE_Manual )
	{
		INT S;
		fd_set SocketSet;
		TIMEVAL SelectTime = {0, 0};
		INT Error;

		FD_ZERO( &SocketSet );
		if ( RemoteSocket == INVALID_SOCKET ) {
			S = Socket;
			FD_SET( Socket, &SocketSet );
		} else {
			S = RemoteSocket;
			FD_SET( RemoteSocket, &SocketSet );
		}

		Error = select( S + 1, &SocketSet, 0, 0, &SelectTime);

		if (( Error == 0 ) || ( Error == SOCKET_ERROR )) {
			DataPending = 0;
		} else {
			DataPending = 1;
		}
	} else if ( ReceiveMode == RMODE_Event ) {
		if ( LinkMode == MODE_Text ) {
			ANSICHAR Str[1000];
			INT BytesReceived;

			for ( INT i=0; i<1000; i++ )
				Str[i] = 0;

			if ( RemoteSocket != INVALID_SOCKET )
				BytesReceived = recv( (SOCKET) RemoteSocket, Str, sizeof(Str) - 1, 0 );
			else
				BytesReceived = recv( (SOCKET) Socket, Str, sizeof(Str) - 1, 0 );

			if( BytesReceived != SOCKET_ERROR )
			{
				Str[BytesReceived]=0;
				eventReceivedText( ANSI_TO_TCHAR(Str) );
			}
		} else if ( LinkMode == MODE_Line ) {
			ANSICHAR Str[1000];
			INT BytesReceived;

			for ( INT i=0; i<1000; i++ )
				Str[i] = 0;

			if ( RemoteSocket != INVALID_SOCKET )
				BytesReceived = recv( (SOCKET) RemoteSocket, Str, sizeof(Str) - 1, 0 );
			else
				BytesReceived = recv( (SOCKET) Socket, Str, sizeof(Str) - 1, 0 );

			if( BytesReceived != SOCKET_ERROR )
			{
				Str[BytesReceived]=0;
				eventReceivedLine( ANSI_TO_TCHAR(Str) );
			}
		} else if ( LinkMode == MODE_Binary ) {
			BYTE Str[1000];
			INT BytesReceived;

			for ( INT i=0; i<1000; i++ )
				Str[i] = 0;

			if ( RemoteSocket != INVALID_SOCKET )
				BytesReceived = recv( (SOCKET) RemoteSocket, (char*) Str, sizeof(Str) - 1, 0 );
			else
				BytesReceived = recv( (SOCKET) Socket, (char*) Str, sizeof(Str) - 1, 0 );

			if( BytesReceived != SOCKET_ERROR )
				eventReceivedBinary( BytesReceived, Str );
		}
	}

}

//
// Open: Open a connection to a foreign host.
//
void ATcpLink::execOpen( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT( FIpAddr, Addr );
	P_FINISH;

	SOCKADDR_IN RemoteHost;
	INT Error;

	if ( GIpDrvInitialized && (GetSocket() != INVALID_SOCKET) )
	{
		RemoteHost.sin_family      = AF_INET;
		RemoteHost.sin_port		   = htons(Addr.Port);
		RemoteHost.sin_addr.s_addr = htonl(Addr.Addr);
		Error = connect( GetSocket(), (SOCKADDR*) &RemoteHost, sizeof(RemoteHost) );
		if ( Error == SOCKET_ERROR ) {
            int err = WSAGetLastError();
            bool ignoreError;
            #if __linux__
                ignoreError = ((err == EINPROGRESS) || (err == EWOULDBLOCK));
            #else
                ignoreError = (err == WSAEWOULDBLOCK);
            #endif

			if ( !ignoreError ) {
				debugf(NAME_Log, TEXT("Open: An error occured while attempting to connect: %i"), WSAGetLastError());
				*(DWORD*) Result = 0;
				return;
			}
		}

		LinkState = STATE_Connecting;
		SendFIFO.Empty();
	}

	*(DWORD*) Result = 1;

}

//
// CheckConnectionAttempt: Check and see if Socket is writable during a
// connection attempt.  If so, the connect succeeded.
//
void ATcpLink::CheckConnectionAttempt()
{
	fd_set SocketSet;
	TIMEVAL SelectTime = {0, 0};
	INT Error;

	if (GetSocket() == INVALID_SOCKET)
		return;

	FD_ZERO( &SocketSet );
	FD_SET( GetSocket(), &SocketSet );
	// Check for writability.  If the socket is writable, the
	// connection attempt succeeded.
	Error = select( GetSocket() + 1, 0, &SocketSet, 0, &SelectTime);

	if ( Error == SOCKET_ERROR ) {
		debugf( NAME_Log, TEXT("CheckConnectionAttempt: Error while checking socket status.") );
		return;
	} else if ( Error == 0 ) {
		//debugf( NAME_Log, "CheckConnectionAttempt: Connection attempt has not yet completed." );
		return;
	}

	// Socket is writable, so we are connected.
	LinkState = STATE_Connected;
	eventOpened();

}

//
// Close: Closes the specified connection.  If no connection
// is specified, closes the "main" connection..
//
void ATcpLink::execClose( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	if ( GIpDrvInitialized && (GetSocket() != INVALID_SOCKET) )
	{
		if ( LinkState == STATE_Listening )
		{
			// if we're listening and not connected, just go into the ListenClosing state
			// to stop performing accept()'s.  If we're listening and connected, we stay in
			// the Listening state to accept further connections.
			if( RemoteSocket != INVALID_SOCKET )
				LinkState = STATE_ListenClosePending;
			else
				LinkState = STATE_ListenClosing;
		}
		else
			LinkState = STATE_ConnectClosePending;
	}
	*(DWORD*) Result = 1;
}

//
// Gracefully shutdown a connection
//
void ATcpLink::ShutdownConnection()
{
	if ( GIpDrvInitialized && (GetSocket() != INVALID_SOCKET) )
	{
		INT Error = 0;
		if( LinkState == STATE_ListenClosePending )
		{
			Error = shutdown( (SOCKET) RemoteSocket, 2 );
			if( Error != SOCKET_ERROR )
				LinkState = STATE_ListenClosing;
		}
		else
		if( LinkState == STATE_ConnectClosePending )
		{
			Error = shutdown( (SOCKET) Socket, 2 );
			if( Error != SOCKET_ERROR )
				LinkState = STATE_ConnectClosing;
		}
		if( Error == SOCKET_ERROR )
		{
			debugf( NAME_Log, TEXT("Close: Error while attempting to close socket.") );
			if ( WSAGetLastError() == WSAENOTSOCK )
				debugf( NAME_Log, TEXT("Close: Tried to close an invalid socket.") );
		}
	}
}

//
// Read text
//
void ATcpLink::execReadText( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR_REF( Str );
	P_FINISH;

	INT BytesReceived;
	if( GIpDrvInitialized && (GetSocket() != INVALID_SOCKET) )
	{
		if( LinkState==STATE_Listening || LinkState==STATE_Connected )
		{
			BYTE Buffer[MAX_STRING_CONST_SIZE];
			appMemset( Buffer, 0, sizeof(Buffer) );
			if( RemoteSocket != INVALID_SOCKET )
				BytesReceived = recv( (SOCKET)RemoteSocket, (char*)Buffer, sizeof(Buffer) - 1, 0 );
			else
				BytesReceived = recv( (SOCKET)Socket, (char*)Buffer, sizeof(Buffer) - 1, 0 );
			if( BytesReceived == SOCKET_ERROR )
			{
				*(DWORD*)Result = 0;
				if( WSAGetLastError() != WSAEWOULDBLOCK )
					debugf( NAME_Log, TEXT("ReadText: Error reading text.") );
				return;
			}
			*Str = ANSI_TO_TCHAR((ANSICHAR*)Buffer);
			*(DWORD*)Result = BytesReceived;
			return;
		}
	}
	*(DWORD*)Result = 0;
}

//
// Send buffered data
//
UBOOL ATcpLink::FlushSendBuffer()
{
	if ( (LinkState == STATE_Listening) ||
		 (LinkState == STATE_Connected) ||
		 (LinkState == STATE_ConnectClosePending) ||
		 (LinkState == STATE_ListenClosePending))
	{
		INT Count = Min<INT>(SendFIFO.Num(), 512);
		INT BytesSent;
		while(Count > 0)
		{
			if ( RemoteSocket != INVALID_SOCKET )
				BytesSent = send( (SOCKET) RemoteSocket, (char*)&SendFIFO(0), Count, 0 );
			else
				BytesSent = send( (SOCKET) Socket, (char*)&SendFIFO(0), Count, 0 );
			if ( BytesSent == SOCKET_ERROR )
				return 1;
			SendFIFO.Remove(0, BytesSent);
			Count = Min<INT>(SendFIFO.Num(), 512);
		}
	}
	return 0;
}

//
// Send raw binary data.
//
void ATcpLink::execSendBinary( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(Count);
	P_GET_ARRAY_REF(BYTE,B);
	P_FINISH;

	if ( GIpDrvInitialized && (GetSocket() != INVALID_SOCKET) )
	{
		INT Index = SendFIFO.Add( Count );
		for(INT i=0; i < Count; i++)
			SendFIFO(i+Index) = B[i];

		*(DWORD*)Result = Count;
		FlushSendBuffer();
		return;
	}
	*(DWORD*)Result = 0;
}

//
// SendText: Sends text string.
// Appends a cr/lf if LinkMode=MODE_Line.
//
void ATcpLink::execSendText( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR( Str );
	P_FINISH;

	*(DWORD*)Result = 0;
	if( GIpDrvInitialized && (GetSocket() != INVALID_SOCKET) )
	{
		if( LinkMode == MODE_Line )
			Str += TEXT("\r\n");
		ANSICHAR* p = TCHAR_TO_ANSI(*Str);
		INT Count = ::strlen(p);
		INT Index = SendFIFO.Add( Count );
		appMemcpy( &SendFIFO(Index), p, Count );
		*(DWORD*)Result = Count;
		FlushSendBuffer();
		return;
	}
	*(DWORD*)Result = 0;
}

//
// Read Binary.
//
void ATcpLink::execReadBinary( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(Count);
	P_GET_ARRAY_REF(BYTE,B);
	P_FINISH;

	int BytesReceived;

	if ( GIpDrvInitialized && (GetSocket() != INVALID_SOCKET) )
	{
		if ( (LinkState == STATE_Listening) || (LinkState == STATE_Connected) )
		{
			if ( RemoteSocket != INVALID_SOCKET )
				BytesReceived = recv( (SOCKET) RemoteSocket, (char *) B, Count, 0 );
			else
				BytesReceived = recv( (SOCKET) Socket, (char *) B, Count, 0 );
			if ( BytesReceived == SOCKET_ERROR ) {
				*(DWORD*) Result = 0;
				if ( WSAGetLastError() != WSAEWOULDBLOCK )
					debugf( NAME_Log, TEXT("ReadBinary: Error reading bytes.") );
				return;
			}
			*(DWORD*) Result = BytesReceived;
			return;
		}
	}

	*(DWORD*)Result = 0;

}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

