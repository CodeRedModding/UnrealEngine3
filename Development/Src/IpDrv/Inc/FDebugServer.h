/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This file contains the definitions of the debug server.
 */

#ifndef __DEBUG_SERVER_H__
#define __DEBUG_SERVER_H__

#if WITH_UE3_NETWORKING
#if !SHIPPING_PC_GAME

#define DEFAULT_DEBUG_SERVER_PORT 13650
#define DEBUG_SOCKET_BUFFER_SIZE 1024

/**
 *	General purpose debug server used by remote tools to communicate with UE3 engine.
 */
class FDebugServer
{
protected:
	class FClientConnection
	{
	public:
		FClientConnection(FSocket* Socket);
		~FClientConnection();

		/**
		 * Returns the name of the connection (IP:port) suitable for printing
		 */
		FString Name() const;

		/**
		 * Ticks the client connection. Returns FALSE if the connection is closed and should be cleaned up.
		 */
		UBOOL Tick();

		/**
		 * Send text to the socket (may be called from any thread)
		 */
		void Send(const BYTE* Data, INT Length);

	private:
		FSocket* Socket;
		BYTE Buffer[DEBUG_SOCKET_BUFFER_SIZE+1];
		INT BufferEnd;
		volatile UBOOL bHasSocketError;
	};

	/** Lock for the Clients list **/
	FCriticalSection* ClientsSync;

	/** The list of clients that we are sending console data to **/
	TArray<FClientConnection*> Clients;

	/** The socket to listen for requests on **/
	FSocket* ListenSocket;

	/** The socket to listen for broadcasts and respond with presence info on **/
	FSocket* PingSocket;

	/** This message is constructed at startup and used as a reply to all pings (contains some sys info) **/
	BYTE* PingReply;
	INT PingReplyLen;
public:
	/**
	 * Default constructor.
	 */
	FDebugServer();

	/**
	 * Initializes the threads that handle the network layer
	 */
	UBOOL Init();

	/**
	 * Shuts down the network threads
	 */
	void Destroy();

	/**
	 *	Per-frame debug server tick.
	 */
	UBOOL Tick();

	void SendText(const TCHAR* Text);

	/**
	 *	Get the number of connected clients
	 *
	 *	@return	INT		The number of clients connected
	 */
	INT GetConnectedClientCount() const
	{
		return Clients.Num();
	}

private:
	void Send(const BYTE* Data, const INT Length);
};

#endif // #if !SHIPPING_PC_GAME
#endif	//#if WITH_UE3_NETWORKING

#endif // __DEBUG_SERVER_H__
