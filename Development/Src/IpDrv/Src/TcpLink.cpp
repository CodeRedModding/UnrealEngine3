/*=============================================================================
	TcpLink.cpp: Simple TCP socket implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

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
	Super::PostScriptDestroyed();
}

//
// BindPort: Binds a free port or optional port specified in argument one.
//
INT ATcpLink::BindPort(INT PortNum,UBOOL bUseNextAvailable)
{
	if( GIpDrvInitialized )
	{
		if( GetSocket() == NULL )
		{
			FSocket* NewSocket = GSocketSubsystem->CreateStreamSocket(TEXT("TCPLink Connection"));
			NewSocket->SetReuseAddr(TRUE);
			
			if( NewSocket != NULL )
			{
				//if( SetSocketLinger( GetSocket()) )
				{
					FInternetIpAddr IpAddr;
					IpAddr.SetIp(getlocalbindaddr(*GWarn));
					IpAddr.SetPort(PortNum);

					INT boundport = bindnextport( NewSocket, IpAddr, bUseNextAvailable ? 20 : 1, 1 );
					if( boundport )
					{
						if( NewSocket->SetNonBlocking(TRUE) )
						{
							// Successfully bound the port.
							IpAddr.GetPort(Port);
							LinkState = STATE_Ready;
							RecvBuf.Empty();
							Socket = NewSocket;
							return boundport;
						}
						else 
						{
							debugf( TEXT("BindPort: ioctlsocket or fcntl failed") );
						}
					}
					else
					{
						debugf( TEXT("BindPort: bind failed") );
					}
				}
				//else debugf( TEXT("BindPort: setsockopt failed") );
				NewSocket->Close();
			}
			else 
			{
				debugf( TEXT("BindPort: socket failed") );
			}
		}
		else 
		{
			debugf( TEXT("BindPort: already bound") );
		}
	}
	else 
	{
		debugf( TEXT("BindPort: IpDrv is not initialized") );
	}
	return 0;
}

//
// IsConnected: Returns true if connected.
//
UBOOL ATcpLink::IsConnected()
{
	if ( LinkState == STATE_Initialized )
	{
		return FALSE;
	}

	if ( (LinkState == STATE_Listening) && (GetRemoteSocket() != NULL) )
	{
		if (GetRemoteSocket()->GetConnectionState() == SCS_Connected)
			return TRUE;
	}

	if ( GetSocket() != NULL )
	{
		if (GetSocket()->GetConnectionState() == SCS_Connected)
			return TRUE;
	}

	return FALSE;
}

//
// Listen: Puts this object in a listening state.
//
UBOOL ATcpLink::Listen()
{
	if ( GIpDrvInitialized && (GetSocket() != NULL) )
	{
		if( LinkState != STATE_ListenClosing )
		{
			if ( LinkState != STATE_Ready ) {
				return FALSE;
			}

			if (GetSocket()->Listen(AcceptClass ? 10 : 1) == FALSE)
			{
				debugf( TEXT("Listen: listen failed") );
				return FALSE;
			}
		}
		LinkState = STATE_Listening;
		SendFIFO.Empty();
		return TRUE;
	}

	return TRUE;
}

//
// Time passes...
//
UBOOL ATcpLink::Tick( FLOAT DeltaTime, enum ELevelTick TickType )
{
	UBOOL Result = Super::Tick( DeltaTime, TickType );

	if( GetSocket() != NULL )
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
				/*while (FlushSendBuffer())
				{
					if (GSocketSubsystem->GetLastErrorCode() != SE_EWOULDBLOCK)
					{
						break;
					}
				};*/
				break;
			case STATE_ConnectClosePending:
			case STATE_ListenClosePending:
				PollConnections();
				if(!FlushSendBuffer())
					ShutdownConnection();
				break;
		}
	}

	FSocket** CheckSocket;
	switch( LinkState )
	{
		case STATE_Connecting:
			return Result;
		case STATE_ConnectClosing:
		case STATE_ConnectClosePending:
		case STATE_Connected:
			CheckSocket = (FSocket**)&Socket;
			break;
		case STATE_ListenClosing:
		case STATE_ListenClosePending:
		case STATE_Listening:
			CheckSocket = (FSocket**)&RemoteSocket;
			break;
		default:
			return Result;
	}

	if (*CheckSocket != NULL)
	{
		// See if the socket needs to be closed.
		UINT PendingDataSize = 0;
		if ((*CheckSocket)->HasPendingData(PendingDataSize))
		{
			if (PendingDataSize == 0)
			{
				// Disconnect
				if (LinkState != STATE_Listening)
				{
					LinkState = STATE_Initialized;
				}
				(*CheckSocket)->Close();
				(*CheckSocket) = NULL;
				eventClosed();
			}
			else if ((PendingDataSize == (UINT)SOCKET_ERROR) && (GSocketSubsystem->GetLastErrorCode() != SE_EWOULDBLOCK)) 
			{
				// Socket error, disconnect.
				if (LinkState != STATE_Listening)
				{
					LinkState = STATE_Initialized;
				}
				(*CheckSocket)->Close();
				(*CheckSocket) = NULL;
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
    UBOOL bHasPendingConnection = FALSE;
	if (GetSocket()->HasPendingConnection(bHasPendingConnection) == FALSE)
	{
		debugf( NAME_Log, TEXT("CheckConnectionQueue: Error while checking socket status. %s"), *GSocketSubsystem->GetSocketError());
		return;
	}

	if (!bHasPendingConnection)
	{
		// debugf( NAME_Log, "CheckConnectionQueue: No connections waiting." );
		return;
	}

	FSocket* NewSocket = GetSocket()->Accept(TEXT("TCPLink accept connection"));

	if ( NewSocket == NULL ) {
		debugf( NAME_Log, TEXT("CheckConnectionQueue: Failed to accept queued connection: %s"), *GSocketSubsystem->GetSocketError());
		return;
	}

	if ( !AcceptClass && RemoteSocket != NULL ) {
		debugf( NAME_Log, TEXT("Discarding redundant connection attempt.") );
		debugf( NAME_Log, TEXT("Current socket handle is %i"), RemoteSocket);
		NewSocket->Close(); //??
		return;
	}

	NewSocket->SetNonBlocking(TRUE);

	if( AcceptClass )
	{
		if( AcceptClass->IsChildOf(ATcpLink::StaticClass()) )
		{
			ATcpLink *Child = Cast<ATcpLink>( GWorld->SpawnActor( AcceptClass, NAME_None, Location, Rotation, NULL, 0,0,this,Instigator ) );
			if (!Child)
			{
				debugf( NAME_Log, TEXT("Could not spawn AcceptClass!") );
			}
			else
			{
				Child->LinkState = STATE_Connected;
				Child->LinkMode = LinkMode;
				Child->Socket = NewSocket;

				Child->RemoteAddr = NewSocket->GetAddress();

				Child->eventAccepted();
			}
		}
		else
			debugf( NAME_Log, TEXT("AcceptClass is not a TcpLink!") );

		return;
	}

	RemoteSocket = NewSocket;
	RemoteAddr = NewSocket->GetAddress();

	eventAccepted();
}

//
// PollConnections: Poll a connection for pending data.
//
void ATcpLink::PollConnections()
{				
	//Grab the right socket
	FSocket* TestSocket = GetRemoteSocket();
	if ( TestSocket == NULL )
	{
		TestSocket = GetSocket();	
	}

	if ( ReceiveMode == RMODE_Manual )
	{
		if (TestSocket && TestSocket->GetConnectionState() == SCS_Connected)
		{
			DataPending = 1;
		}
		else
		{
			DataPending = 0;
		}
	}
	else if ( ReceiveMode == RMODE_Event )
	{
		switch( LinkMode )
		{
		case MODE_Text:
			{
				ANSICHAR Str[1000];
				INT BytesReceived;
				appMemzero(Str, sizeof(Str));

				UBOOL Success = TestSocket->Recv((BYTE*)Str, sizeof(Str) - 1, BytesReceived);
				if( Success && BytesReceived >= 0 )
				{
					Str[BytesReceived]=0;
					eventReceivedText( ANSI_TO_TCHAR(Str) );
				}
			}
			break;
		case MODE_Line:
			{
				ANSICHAR Str[1000];
				INT BytesReceived;
				appMemzero(Str, sizeof(Str));

				UBOOL Success = TestSocket->Recv((BYTE*)Str, sizeof(Str) - 1, BytesReceived);

				if( Success && BytesReceived >= 0 )
				{
					Str[BytesReceived]=0;
					FString fstr, SplitStr;
					switch (InLineMode)
					{
						case LMODE_DOS:		SplitStr = TEXT("\r\n"); break;
						case LMODE_auto:
						case LMODE_UNIX:	SplitStr = TEXT("\n"); break;
						case LMODE_MAC:		SplitStr = TEXT("\n\r"); break;
					}
					RecvBuf = FString::Printf(TEXT("%s%s"), *RecvBuf, ANSI_TO_TCHAR(Str));
					while (RecvBuf.Split(SplitStr, &fstr, &RecvBuf))
					{
						if (InLineMode == LMODE_auto)
						{
							if (fstr.Len() > 0 && fstr[fstr.Len()-1] == '\r') // DOS fix
								fstr = fstr.Left(fstr.Len()-1);
							if (RecvBuf.Len() > 0 && RecvBuf[0] == '\r') // MAC fix
								RecvBuf = RecvBuf.Mid(1);
						}
						eventReceivedLine( fstr );
					}
				}
			}
			break;
		case MODE_Binary:
			{
				BYTE Str[255];
				INT BytesReceived;
				appMemzero(Str, sizeof(Str));

				UBOOL Success = TestSocket->Recv(Str, sizeof(Str) - 1, BytesReceived);

				if( Success && BytesReceived >= 0 )
				{
					eventReceivedBinary( BytesReceived, Str );
				}
			}
			break;
		}
	}
}

//
// Open: Open a connection to a foreign host.
//
UBOOL ATcpLink::Open(FIpAddr Addr)
{
	if ( GIpDrvInitialized && (GetSocket() != NULL) )
	{
		FInternetIpAddr IpAddr;
		IpAddr.SetAddress(Addr);
		GetSocket()->Connect(IpAddr);
		LinkState = STATE_Connecting;
		SendFIFO.Empty();
	}

	return TRUE;
}

//
// CheckConnectionAttempt: Check and see if Socket is writable during a
// connection attempt.  If so, the connect succeeded.
//
void ATcpLink::CheckConnectionAttempt()
{
	if (GetSocket() == NULL)
		return;

	ESocketConnectionState ConnectState = GetSocket()->GetConnectionState();
	if (ConnectState == SCS_Connected)
	{
		// Socket is writable, so we are connected.
		LinkState = STATE_Connected;
		eventOpened();
	}
	else if (ConnectState == SCS_ConnectionError)
	{
		debugf( NAME_Log, TEXT("CheckConnectionAttempt: Error while checking socket status.") );
		return;
	}
	else
	{
		debugf( NAME_Log, TEXT("CheckConnectionAttempt: Connection attempt has not yet completed.") );
		return;
	}
}

//
// Close: Closes the specified connection.  If no connection
// is specified, closes the "main" connection..
//
UBOOL ATcpLink::Close()
{
	if ( GIpDrvInitialized && (GetSocket() != NULL) )
	{
		if ( LinkState == STATE_Listening )
		{
			// if we're listening and not connected, just go into the ListenClosing state
			// to stop performing accept()'s.  If we're listening and connected, we stay in
			// the Listening state to accept further connections.
			if( RemoteSocket != NULL )
			{
				LinkState = STATE_ListenClosePending;
			}
			else
			{
				LinkState = STATE_ListenClosing;
			}
		}
		else if (( LinkState != STATE_ListenClosing ) && ( LinkState != STATE_ConnectClosing ))
		{
			LinkState = STATE_ConnectClosePending;
		}
	}

	return TRUE;
}

//
// Gracefully shutdown a connection
//
void ATcpLink::ShutdownConnection()
{
	if ( GIpDrvInitialized && (GetSocket() != NULL) )
	{
		UBOOL Success = FALSE;
		if( LinkState == STATE_ListenClosePending )
		{
			//Error = shutdown( (SOCKET) RemoteSocket, SD_BOTH );
			if (RemoteSocket != NULL)
			{
				Success = RemoteSocket->Close(); //not the same as shutdown

				LinkState = STATE_Initialized;
				RemoteSocket = NULL;
				eventClosed();
			}
		}
		else if( LinkState == STATE_ConnectClosePending )
		{
			//Error = shutdown( (SOCKET) Socket, SD_BOTH );
			if (Socket != NULL)
			{
				Success = Socket->Close(); //not the same as shutdown

				LinkState = STATE_Initialized;
				Socket = NULL;
				eventClosed();
			}
		}
		if (!Success)
		{
			INT ErrorCode = GSocketSubsystem->GetLastErrorCode();
			debugf( NAME_Log, TEXT("Close: Error while attempting to close socket. (%d)"), ErrorCode );
			if (ErrorCode == SE_ENOTSOCK)
			{
				debugf( NAME_Log, TEXT("Close: Tried to close an invalid socket.") );
			}
		}
	}
}

//
// Read text
//
INT ATcpLink::ReadText(FString& Str)
{
	INT BytesReceived;
	if( GIpDrvInitialized && (GetSocket() != NULL) )
	{
		//Grab the right socket
		FSocket* TestSocket = GetRemoteSocket();
		if ( TestSocket == NULL )
		{
			TestSocket = GetSocket();	
		}

		if( LinkState==STATE_Listening || LinkState==STATE_Connected )
		{
			BYTE Buffer[MAX_STRING_CONST_SIZE];
			appMemset( Buffer, 0, sizeof(Buffer) );

			UBOOL Success = TestSocket->Recv(Buffer, sizeof(Buffer) - 1, BytesReceived);
			
			if (!Success)
			{
				if (GSocketSubsystem->GetLastErrorCode() != SE_EWOULDBLOCK)
				{
					debugf( NAME_Log, TEXT("ReadText: Error reading text.") );
				}
				return 0;
			}

			Str = ANSI_TO_TCHAR((ANSICHAR*)Buffer);
			return BytesReceived;
		}
	}
	
	return 0;
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
		//Grab the right socket
		FSocket* TestSocket = GetRemoteSocket();
		if ( TestSocket == NULL )
		{
			TestSocket = GetSocket();	
		}

		INT Count = Min<INT>(SendFIFO.Num(), 512);
		INT BytesSent;

		while(Count > 0)
		{					  
			UBOOL Success = TestSocket->Send(&SendFIFO(0), Count, BytesSent);
			if (!Success)
			{
				  return 1;
			}

			SendFIFO.Remove(0, BytesSent);
			Count = Min<INT>(SendFIFO.Num(), 512);
		}
	}
	return 0;
}

//
// Send raw binary data.
//
INT ATcpLink::SendBinary(INT Count, BYTE* B)
{
	if ( GIpDrvInitialized && (GetSocket() != NULL) )
	{
		INT Index = SendFIFO.Add( Count );
		for(INT i=0; i < Count; i++)
			SendFIFO(i+Index) = B[i];

		FlushSendBuffer();
		return Count;
	}
	
	return 0;
}

//
// SendText: Sends text string.
// Appends a cr/lf if LinkMode=MODE_Line.
//
INT ATcpLink::SendText(const FString& Str)
{
	if( GIpDrvInitialized && (GetSocket() != NULL) )
	{
		FString NewStr(Str);
		if( LinkMode == MODE_Line )
		{
			switch (OutLineMode)
			{
				case LMODE_auto:
				case LMODE_DOS:		NewStr = Str + TEXT("\r\n"); break;
				case LMODE_UNIX:	NewStr = Str + TEXT("\n"); break;
				case LMODE_MAC:		NewStr = Str + TEXT("\n\r"); break;
			}
		}

		INT Count = NewStr.Len();
		INT Index = SendFIFO.Add( Count );
		//debugf(TEXT("--------------------------SendText size %d chars"), Count);
		//debugf(TEXT("%s"), *FString(TCHAR_TO_ANSI(*NewStr)));
		//debugf(TEXT("------------------------------------------------"), Count);
		appMemcpy( &SendFIFO(Index), TCHAR_TO_ANSI(*NewStr), Count );
		FlushSendBuffer();
		return Count;
	}
	
	return 0;
}

/** Script interface to ReadBinary
*/
INT ATcpLink::ReadBinary(INT Count, BYTE* B)
{
	return NativeReadBinary(Count, B);
}

//
// Read Binary.
//

INT ATcpLink::NativeReadBinary(INT Count, BYTE*& B)
{
	int BytesReceived;

	if ( GIpDrvInitialized && (GetSocket() != NULL) )
	{
		if ( (LinkState == STATE_Listening) || (LinkState == STATE_Connected) )
		{
			//Grab the right socket
			FSocket* TestSocket = GetRemoteSocket();
			if ( TestSocket == NULL )
			{
				TestSocket = GetSocket();	
			}

			UBOOL Success = TestSocket->Recv(B, Count, BytesReceived);

			if (!Success)
			{
				if (GSocketSubsystem->GetLastErrorCode() != SE_EWOULDBLOCK)
				{
					debugf( NAME_Log, TEXT("ReadBinary: Error reading bytes.") );
				}
				return 0;
			}

			return BytesReceived;
		}
	}

	return 0;
}

#endif
