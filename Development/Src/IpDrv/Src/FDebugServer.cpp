/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This file contains the implementation of the debug server.
 */

#include "UnIpDrv.h"
#include "FDebugServer.h"
#include "GameType.h"

#if WITH_UE3_NETWORKING
#if !SHIPPING_PC_GAME

/*----------------------- FDebugServer::FClientConnection ----------------------*/

FDebugServer::FClientConnection::FClientConnection(FSocket* InSocket)
: Socket(InSocket)
, BufferEnd(0)
, bHasSocketError(FALSE)
{
	appMemzero(Buffer, sizeof(Buffer));
}

FDebugServer::FClientConnection::~FClientConnection()
{
	Socket->Close();
	GSocketSubsystemDebug->DestroySocket(Socket);
}

UBOOL FDebugServer::FClientConnection::Tick()
{
	if (bHasSocketError)
	{
		// got an error while sending. handle it now.
		return FALSE;
	}

	UINT DataSize = 0;
	while (Socket->HasPendingData(DataSize))
	{
		if (DataSize == 0)
		{
			// standard TCP close
			return FALSE;
		}

		// read as much data as we can
		INT DataRead = 0;
		if (!Socket->Recv(&Buffer[BufferEnd], DEBUG_SOCKET_BUFFER_SIZE - BufferEnd, DataRead))
		{
			// socket error
			return FALSE;
		}
		if (DataRead <= 0)
		{
			// standard TCP close
			return FALSE;
		}
		BufferEnd += DataRead;

		// see if there's a command here (search for '\n')
		for (INT i=0;i<BufferEnd;++i)
		{
			if (Buffer[i] == '\n' || Buffer[i] == '\r' || Buffer[i] == '\0')
			{
				// terminate the command
				Buffer[i] = '\0';

				// execute the command if it's got any text
				if (i > 0)
				{
					FUTF8ToTCHAR Converted((const ANSICHAR*)Buffer);
					// this command is here to use as a ping for testing
					if (appStricmp(Converted, TEXT("cookies")) == 0)
					{
						// send the ping reply
						const static ANSICHAR PING_REPLY[] = "(om nom nom)\r\n";
						Send((const BYTE*)PING_REPLY, sizeof(PING_REPLY)-1);
					}
					else
					{
						// process the command in Buffer
						new(GEngine->DeferredCommands) FString(Converted);
					}
				}

				// account for the memory we just processed
				++i;
				BufferEnd -= i;
				if (BufferEnd <= 0)
				{
					return TRUE;
				}

				// move the memory down and reset our index
				appMemmove(Buffer, &Buffer[i], BufferEnd);
				i = -1; // account for ++i in the for loop
			}
		}

		// keep going as long as we still have room in the buffer
		// NOTE: if we run out of room, the command is more than the buffer size and we don't want to keep the socket open
		if (BufferEnd >= DEBUG_SOCKET_BUFFER_SIZE)
			return FALSE;
	}

	// nothing is ready
	return TRUE;
}

void FDebugServer::FClientConnection::Send(const BYTE* Data, INT Length)
{
	if (!bHasSocketError)
	{
		INT BytesSent = 0;
		if (!Socket->Send(Data, Length, BytesSent) || Length != BytesSent)
		{
			bHasSocketError = TRUE;
		}
	}
}

FString FDebugServer::FClientConnection::Name() const
{
	return Socket->GetAddress().ToString(TRUE);
}

/*------------------------------ FDebugServer ----------------------------------*/

FDebugServer::FDebugServer()
: ClientsSync(NULL)
, ListenSocket(NULL)
, PingSocket(NULL)
, PingReply(NULL)
{
}

/**
 * Initializes the threads that handle the network layer
 */
UBOOL FDebugServer::Init()
{
	if (GSocketSubsystemDebug == NULL || !GIpDrvInitialized)
	{
		return FALSE;
	}
	if (ListenSocket != NULL)
	{
		// already initialized
		return FALSE;
	}

	// get the listen port from the command line
	INT ListenPort = DEFAULT_DEBUG_SERVER_PORT;
	if (!Parse( appCmdLine(), TEXT("debugconsoleport="), ListenPort))
	{
		ListenPort = DEFAULT_DEBUG_SERVER_PORT;
	}
	if (ListenPort < 0)
	{
		// don't bind debug console
		return FALSE;
	}

	// apply default port
	if (ListenPort == 0)
	{
		ListenPort = DEFAULT_DEBUG_SERVER_PORT;
	}

	// create a critical section for our Clients Sync
	ClientsSync = GSynchronizeFactory->CreateCriticalSection();
	check(ClientsSync);

	// create a TCP listening socket and bind to ListenPort
	ListenSocket = GSocketSubsystemDebug->CreateStreamSocket(TEXT("FDebugServer tcp-listen"));
	if(!ListenSocket)
	{
		debugf( NAME_Warning, TEXT( "Failed to create listen socket in FDebugServer::Init" ) );
		Destroy();
		return FALSE;
	}

	// bind the socket to the listen port
	FInternetIpAddr ListenAddr;
	ListenAddr.SetAnyAddress();
	ListenAddr.SetPort(ListenPort);
	if (!ListenSocket->Bind(ListenAddr))
	{
		debugf( NAME_Warning, TEXT( "Failed to bind listen socket in FDebugServer::Init" ) );
		Destroy();
		return FALSE;
	}
	if (!ListenSocket->Listen(16))
	{
		debugf( NAME_Warning, TEXT( "Failed to listen on socket in FDebugServer::Init" ) );
		Destroy();
		return FALSE;
	}

	// create a static ping reply with some system information
	TCHAR Buf[MAX_SPRINTF+1] = { 0 };
	appSprintf(Buf, TEXT("UE3PONG\nDEBUGPORT=%d\nCNAME=%s\nGAME=%s\nGAMETYPE=%s\nPLATFORM=%s\n"), 
		ListenPort,
		appComputerName(), 
		GGameName, 
		ANSI_TO_TCHAR(ToString(appGetGameType())), 
		*appGetPlatformString()
		);
	FTCHARToUTF8 Data(Buf);
	PingReplyLen = appStrlen((ANSICHAR*)Data);
	PingReply = new BYTE[PingReplyLen+1];
	appMemcpy(PingReply, (ANSICHAR*)Data, PingReplyLen);
	PingReply[PingReplyLen] = '\0';

	// create a UDP socket and also bind it to ListenPort (UDP and TCP port space are separate)
	PingSocket = GSocketSubsystemDebug->CreateDGramSocket(TEXT("FDebugServer ping-pong"), TRUE);
	check(PingSocket);

	// bind the socket to the well known port in UDP space
	FInternetIpAddr PingAddr;
	PingAddr.SetAnyAddress();
	PingAddr.SetPort(DEFAULT_DEBUG_SERVER_PORT);
	if (!PingSocket->Bind(PingAddr))
	{
		debugf( NAME_Warning, TEXT( "Failed to bind to ping socket in FDebugServer::Init" ) );

		// ping socket is not strictly necessary for operation
		PingSocket->Close();
		GSocketSubsystemDebug->DestroySocket(PingSocket);
		PingSocket = NULL;
	}

	// optionally wait for a connection before proceeding
	if( ParseParam( appCmdLine(), TEXT( "WaitForDebugServer" ) ) )
	{
		while( !GDebugChannel->Tick() )
		{
			appSleep( 0.1f );
		}
	}
	return TRUE;
}

/**
 * Shuts down the network threads
 */
void FDebugServer::Destroy(void)
{
	// close all client connections
	for (INT i=0;i<Clients.Num();++i)
	{
		FClientConnection* Client = Clients(i);
		delete Client;
	}
	Clients.Empty();

	// close the ping socket
	if (PingSocket != NULL)
	{
		PingSocket->Close();
		GSocketSubsystemDebug->DestroySocket(PingSocket);
		PingSocket = NULL;
	}
	// close the listen socket
	if (ListenSocket != NULL)
	{
		ListenSocket->Close();
		GSocketSubsystemDebug->DestroySocket(ListenSocket);
		ListenSocket = NULL;
	}

	// destroy the critical section
	if (ClientsSync != NULL)
	{
		GSynchronizeFactory->Destroy(ClientsSync);
		ClientsSync = NULL;
	}

	// delete the ping reply
	if (PingReply != NULL)
	{
		delete[] PingReply;
		PingReply = NULL;
	}
}

UBOOL FDebugServer::Tick()
{
	UBOOL bHasConnection = FALSE;

	if (ListenSocket == NULL)
	{
		return( bHasConnection );
	}
	UBOOL bReadReady = FALSE;

	// check for incoming connections
	if (ListenSocket->HasPendingConnection(bReadReady) && bReadReady)
	{
		FSocket* Socket = ListenSocket->Accept(TEXT("Remote Console Connection"));
		if (Socket != NULL)
		{
			// make sure the connection is non-blocking
			Socket->SetNonBlocking();

			// create a new connection to service this socket
			FClientConnection* NewConn = new FClientConnection(Socket);

			// send a welcome message to the newbie
			const static ANSICHAR WELCOME_STR[] = "UE3 DEBUG CONSOLE\r\nFeed me cookies!!!\r\n";
			NewConn->Send((const BYTE*)WELCOME_STR, appStrlen(WELCOME_STR));
			for (INT i=0;i<Clients.Num();++i)
			{
				FTCHARToUTF8 Conversion(*FString::Printf(TEXT("(%s is here too)\r\n"), *Clients(i)->Name()));
				ANSICHAR* Data = (ANSICHAR*)Conversion;
				NewConn->Send((BYTE*)Data, Conversion.Length());
			}

			// announce to the other sockets someone joined
			SendText(*FString::Printf(TEXT("(%s is lurking in the shadows)\r\n"), *NewConn->Name()));

			// add it to the clients list
			{
				FScopeLock mux(ClientsSync);
				Clients.Push(NewConn);
			}

			bHasConnection = TRUE;
		}
	}

	// handle command data on any active socket
	for (INT i=0;i<Clients.Num();++i)
	{
		FClientConnection* Client = Clients(i);
		if (!Client->Tick())
		{
			// remove the client from the array
			{
				FScopeLock mux(ClientsSync);
				Clients.RemoveSwap(i);
			}

			// announce to the other sockets someone left
			SendText(*FString::Printf(TEXT("(%s slowly creeps away)\r\n"), *Client->Name()));

			// destroy it
			delete Client;

			// be sure to run this index again
			--i; 
		}
	}

	// check for pings and respond with replies
	if (PingSocket != NULL)
	{
		UINT DataSize = 0;
		while (PingSocket->HasPendingData(DataSize))
		{
			// read the datagram off the socket (excess data will be discarded but our pings are not that large)
			FInternetIpAddr SourceAddr;
			INT DataRead = 0;
			BYTE PingBuffer[1600]; // comfortably above the usual UDP MTU
			PingSocket->RecvFrom(PingBuffer, sizeof(PingBuffer)-1, DataRead, SourceAddr);

			// if we didn't discard any data AND there was some data (0-byte payloads are possible on some systems)
			if ( DataRead > 0)
			{
				PingBuffer[DataRead] = '\0';

				// make sure this is a ping and not traffic from some other app
				// we will also receive UE3PONG packets from ourselves as well as other apps, ignore them
				if (appStrcmpANSI((ANSICHAR*)PingBuffer, "UE3PING") == 0)
				{
					// send a reply to the source address
					INT DataSent = 0;
					PingSocket->SendTo(PingReply, PingReplyLen, DataSent, SourceAddr);
				}
			}
		}
	}

	return( bHasConnection );
}

void FDebugServer::SendText(const TCHAR* Text)
{
	// opportunistically bail
	if (Clients.Num() == 0)
	{
		return;
	}

	// convert to UTF-8 to preserve high bits but not have to deal with NBO or breaking Telnet compatibility
	FTCHARToUTF8 Conversion(Text);
	ANSICHAR* Data = (ANSICHAR*)Conversion;
	Send((const BYTE*)Data, appStrlen(Data));
}

void FDebugServer::Send(const BYTE* Data, const INT Length)
{
	// NOTE: this could be downgraded to a readlock to greatly reduce contention if we had read/write locks
	FScopeLock mux(ClientsSync);

	// Do not send text if there's no client connected
	if (Clients.Num() == 0)
	{
		return;
	}

	// send the text on all the sockets
	for (INT i=0;i<Clients.Num();++i)
	{
		FClientConnection* Client = Clients(i);
		Client->Send(Data, Length);
	}
}

#endif // !SHIPPING_PC_GAME
#endif // WITH_UE3_NETWORKING
