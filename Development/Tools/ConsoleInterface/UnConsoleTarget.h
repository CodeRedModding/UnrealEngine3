// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#ifndef _UNCONSOLETARGET_H_
#define _UNCONSOLETARGET_H_

// don't include this on consoles
#if !CONSOLE && !PLATFORM_MACOSX && WITH_UE3_NETWORKING

#pragma pack(push,8)

#include "UnConsoleSocket.h"

#include "..\..\IpDrv\Inc\GameType.h"

#define PACKET_BUFFER_SIZE 1024

/** Number of milliseconds between connection retries */
#define RECONNECT_RETRY_TIME 1000

class CTarget;

/// Representation of a single UE3 instance running on PC
class CTarget : public FRefCountedTarget<CTarget>
{
public:
	/** User friendly name of the target. */
	wstring Name;
	/** Computer name. */
	wstring ComputerName;
	/** Game name. */
	wstring GameName;
	/** Configuration name. */
	wstring Configuration;
	/** Game type id. */
	EGameType GameType;
	/** Game type name. */
	wstring GameTypeName;
	/** Platform id. */
	FConsoleSupport::EPlatformType PlatformType;
	/** Number of ticks since the last connection retry */
	DWORD LastConnectionTicks;
	/** Number of reconnect tries */
	INT ReconnectTries;

	/** The callback for TTY notifications. */
	TTYEventCallbackPtr TxtCallback;

	virtual const sockaddr_in& GetRemoteAddress() const 
	{ 
		return RemoteAddress; 
	}

	/** TCP client used to communicate with the game. */
	FConsoleSocket* TCPClient;

	/** UDP client used to communicate with the game. */
	FConsoleSocket* UDPClient;

	/** True if something is currently connected to this target. */
	bool bConnected;
	/** TRUE if we got a socket error while trying to send. Will disconnect on next tick*/
	bool bHadSocketError;

	/** Constructor. */
	CTarget(const sockaddr_in* InRemoteAddress, FConsoleSocket* InTCPClient, FConsoleSocket* InUDPClient)
		: ReconnectTries(0)
		, bHadSocketError(false)
		, bConnected(false)
		, TxtCallback(NULL)
	{
		ZeroMemory(PacketBuffer, sizeof(PacketBuffer));

		if ( InRemoteAddress )
		{
			memcpy(&RemoteAddress, InRemoteAddress, sizeof(RemoteAddress));
		}
		else
		{
			ZeroMemory(&RemoteAddress, sizeof(RemoteAddress));
		}

		TCPClient = InTCPClient;
		if ( TCPClient )
		{
			TCPClient->SetAttributes(FConsoleSocket::SF_IPv4, FConsoleSocket::ST_Stream, FConsoleSocket::SP_TCP);
		}

		UDPClient = InUDPClient;
		if ( UDPClient )
		{
			UDPClient->SetAttributes(FConsoleSocket::SF_IPv4, FConsoleSocket::ST_Datagram, FConsoleSocket::SP_UDP);
		}
	}

	virtual ~CTarget()
	{
		Disconnect();
		if ( UDPClient )
		{
			delete UDPClient;
			UDPClient = NULL;
		}
		if ( TCPClient )
		{
			delete TCPClient;
			TCPClient = NULL;
		}
	}

	virtual bool ShouldSendTTY()
	{
		return true;
	}

	// ticks the connection, receiving and dispatching messages
	virtual void Tick()
	{
		if (!bConnected)
		{
			Reconnect();
			return;
		}

		// if we had a socket error, disconnect now
		if (bHadSocketError)
		{
			Disconnect();
			SendTTY("*** Socket Error While Sending\n");
			return;
		}

		// fill our buffer
		if (TCPClient && TCPClient->IsReadReady())
		{
			for (;;)
			{
				// read into buffer
				int BytesReceived = TCPClient->Recv(PacketBuffer, PACKET_BUFFER_SIZE);
				if (BytesReceived == 0)
				{
					// normal TCP close
					Disconnect();
					SendTTY("*** Socket Closed\n");
					return;
				}
				else if (BytesReceived < 0)
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK)
					{
						// socket Error
						Disconnect();
						SendTTY("*** Socket Error While Receiving\n");
						return;
					}

					// no more data
					break;
				}

				// terminate the line
				PacketBuffer[BytesReceived] = '\0';
				if ( ShouldSendTTY() )
				{
					SendTTY(PacketBuffer);
				}
			}
		}
	}

	virtual bool Connect()
	{
		Disconnect();

		if ( TCPClient )
		{
			if (!TCPClient->CreateSocket())
			{
				return false;
			}

			TCPClient->SetAddress(RemoteAddress);
			if (!TCPClient->Connect())
			{
				return false;
			}
			TCPClient->SetNonBlocking(true);
		}

		ReconnectTries = 0;
		bConnected = true;
		return true;
	}

	virtual void Reconnect()
	{
		DWORD Ticks = GetTickCount();
		if( Ticks - LastConnectionTicks > RECONNECT_RETRY_TIME && ReconnectTries < 60 )
		{
			LastConnectionTicks = Ticks;
			ReconnectTries++;
			SendTTY("*** Reconnecting\n");
			Connect();
		}
	}

	virtual void Disconnect()
	{
		bHadSocketError = false;
		bConnected = false;
		if ( TCPClient )
		{
			TCPClient->Close();
		}
	}

	// asynchronous send
	virtual bool SendConsoleCommand(const char* Message)
	{
		int MessageLength = ( int )strlen( Message );
		if (MessageLength > 0 && bConnected && !bHadSocketError)
		{
			if (TCPClient && TCPClient->Send((char*)Message, MessageLength) == MessageLength)
			{
				return true;
			}
			bHadSocketError = true;
		}
		return false;
	}

	/** Sends TTY text on the callback if it is valid. */
	virtual void SendTTY(const char* Txt)
	{
		wchar_t Buffer[PACKET_BUFFER_SIZE + 16] = L"";
		int Length = MultiByteToWideChar( CP_UTF8, 0, Txt, ( int )strlen( Txt ), Buffer, PACKET_BUFFER_SIZE );
		if (Length > 0)
		{
			Buffer[Length] = '\0';
			SendTTY(Buffer);
		}
	}

	virtual void SendTTY(const wchar_t* Txt)
	{
		if(TxtCallback)
		{
			TxtCallback(Txt);
		}
	}

protected:
	/** The remote address of the target */
	sockaddr_in RemoteAddress;

	char PacketBuffer[PACKET_BUFFER_SIZE+1];
};

#pragma pack( pop )

#endif // !CONSOLE && !PLATFORM_MACOSX && WITH_UE3_NETWORKING

#endif // _UNCONSOLETARGET_H_
