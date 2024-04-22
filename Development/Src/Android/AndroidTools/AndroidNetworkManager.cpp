/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "AndroidNetworkManager.h"

#include "..\..\IpDrv\Inc\DebugServerDefs.h"

#define WORKER_THREAD_HEURISTIC 2

FAndroidNetworkManager::FAndroidNetworkManager()
: IOCompletionPort(INVALID_HANDLE_VALUE)
, TTYCallback(NULL)
{
	// 	GetSystemInfo(&SysInfo);
	// 	WorkerThreads.resize(SysInfo.dwNumberOfProcessors * WORKER_THREAD_HEURISTIC);
	// the above creates a zillion threads, we don't need many, and especially with the fact that we
	// only support the local simulator, one thread is plenty
	WorkerThreads.resize(1);
	ZeroMemory(&WorkerThreads[0], sizeof(HANDLE) * WorkerThreads.size());
}

FAndroidNetworkManager::~FAndroidNetworkManager()
{
}

/**
 * Gets an CAndroidTarget from a TARGETHANDLE
 */
CAndroidTarget* FAndroidNetworkManager::ConvertTarget( const TARGETHANDLE Handle )
{
	TargetPtr Target = GetTarget(Handle);

	if( Target)
	{
		return (CAndroidTarget*)(Target.GetHandle());
	}

	return NULL;
}

/**
 * Gets an CAndroidTarget from a sockaddr_in
 */
CAndroidTarget* FAndroidNetworkManager::ConvertTarget( const sockaddr_in &Address )
{
	TargetPtr Target = GetTarget(Address);

	if( Target)
	{
		return (CAndroidTarget*)(Target.GetHandle());
	}

	return NULL;
}

/**
 * Gets an CAndroidTarget from a TargetPtr
 */
CAndroidTarget* FAndroidNetworkManager::ConvertTarget( TargetPtr InTarget )
{
	return (CAndroidTarget*)(InTarget.GetHandle());
}

/**
 * Initalizes winsock and the FAndroidNetworkManager instance.
 */
void FAndroidNetworkManager::Initialize()
{
	if(bInitialized)
	{
		return;
	}

	DebugOutput(L"Creating shutdown event...\n");
	ThreadCleanupEvent = CreateEvent(NULL, TRUE, FALSE, L"FAndroidNetworkManager::ThreadCleanupEvent");
	if(!ThreadCleanupEvent)
	{
		DebugOutput(L"Creating shutdown event failed.\n");
		return;
	}

	DebugOutput(L"Shutdown event created.\n");

	DebugOutput(L"Initializing Network Manager...\n");

	// Init Windows Sockets
	WSADATA WSAData;
	WORD WSAVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(WSAVersionRequested, &WSAData) != 0)
	{
		return;
	}

	DebugOutput(L"WSA Sockets Initialized.\n");

	DebugOutput(L"Creating IO Completion Port...\n");

	// By providing 0 to the last argument a IOCP thread is created for every processor on the machine
	IOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
	if(!IOCompletionPort)
	{
		DebugOutput(L"Creating IO Completion Port Failed.\n");
		WSACleanup();
		return;
	}

	DebugOutput(L"IO Completion Port Created.\n");

// 	DebugOutput(L"Creating IO Worker Threads...\n");
// 
// 	size_t NumThreadsCreated = 0;
// 	for(size_t ThreadIndex = 0; ThreadIndex < WorkerThreads.size(); ++ThreadIndex, ++NumThreadsCreated)
// 	{
// 		WorkerThreads[ThreadIndex] = (HANDLE)_beginthreadex(NULL, 0, WorkerThreadProc, this, 0, NULL);
// 		if(!WorkerThreads[ThreadIndex])
// 		{
// 			break;
// 		}
// 	}
// 
// 	if(NumThreadsCreated != WorkerThreads.size())
// 	{
// 		DebugOutput(L"Creating IO Worker Threads Failed.\n");
// 		
// 		Cleanup();
// 
// 		return;
// 	}
// 
// 	DebugOutput(L"IO Worker Threads Created.\n");

	// Init broadcaster
	Broadcaster.SetPort(DefaultDebugChannelReceivePort + AndroidOffset);
	Broadcaster.SetAttributes(FConsoleSocket::SF_IPv4, FConsoleSocket::ST_Datagram, FConsoleSocket::SP_UDP);
	if(!Broadcaster.CreateSocket())
	{
		DebugOutput(L"Failed to create broadcast socket.\n");
		return;
	}
	Broadcaster.SetBroadcasting(true);
}

/**
 * Cleans up winsock and all of the resources allocated by the FAndroidNetworkManager instance.
 */
void FAndroidNetworkManager::Cleanup()
{
	DebugOutput(L"Cleaning up Network Manager...\n");

	// Shut down all of the sockets and let the IOCP worker threads remove the active ones themselves
	TargetsLock.Lock();
	{
		for(TargetMap::iterator Iter = Targets.begin(); Iter != Targets.end(); ++Iter)
		{
			TargetPtr Target = (*Iter).second;
			if ( Target->TCPClient )
			{
				Target->TCPClient->Close();
			}
			if ( Target->UDPClient )
			{
				Target->UDPClient->Close();
			}
		}
	}
	TargetsLock.Unlock();

	// Close broadcaster
	Broadcaster.Close();

	// Close 'server announce' listener
	ServerResponseListener.Close();

	SetEvent(ThreadCleanupEvent);

	WaitForMultipleObjects((DWORD)WorkerThreads.size(), &WorkerThreads[0], TRUE, 5000);

	for(size_t ThreadIndex = 0; ThreadIndex < WorkerThreads.size(); ++ThreadIndex)
	{
		CloseHandle(WorkerThreads[ThreadIndex]);
		WorkerThreads[ThreadIndex] = 0;
	}

	CloseHandle(ThreadCleanupEvent);
	CloseHandle(IOCompletionPort);

	WSACleanup();

	// Clean up any remaining targets that were not on an IOCP worker thread
	TargetsLock.Lock();
	{
		Targets.clear();
	}
	TargetsLock.Unlock();

	DebugOutput(L"Finished cleaning up Network Manager.\n");
	bInitialized = false;
}

/**
 * Adds a target to the list of targets.
 *
 * @param	Address		The address of the target being added.
 * @return	A reference pointer to the new target.
 */
TargetPtr FAndroidNetworkManager::AddTarget(const sockaddr_in& Address)
{
	TargetPtr Ret;

	TargetsLock.Lock();
	{
		Ret = GetTarget(Address);

		if(!Ret)
		{
			Ret = CreateTarget(&Address);
			Targets[Ret.GetHandle()] = Ret;
		}
		else
		{
			if(Ret->UDPClient && !Ret->UDPClient->IsValid())
			{
				Ret->UDPClient->CreateSocket();
			}

			if(Ret->TCPClient && !Ret->TCPClient->IsValid())
			{
				Ret->TCPClient->CreateSocket();
			}
		}
	}
	TargetsLock.Unlock();

	return Ret;
}

/**
 * Removes the target with the specified address.
 *
 * @param	Handle		The handle of the target to be removed.
 */
TargetPtr FAndroidNetworkManager::RemoveTarget(TARGETHANDLE Handle)
{
	TargetPtr Target = FConsoleNetworkManager::RemoveTarget(Handle);
	if ( Target )
	{
		if ( Target->UDPClient )
		{
			Target->UDPClient->Close();
		}
		if ( Target->TCPClient )
		{
			Target->TCPClient->Close();
		}
		Target->bConnected = false;
	}
	return Target;
}

/**
 * Triggers the TTY callback for outputting a message.
 *
 * @param	Data	The message to be output.
 * @param	Length	The size in bytes of the buffer pointed to by Data.
 * @param	Target	The target to send the TTY text to.
 */
void FAndroidNetworkManager::NotifyTTYCallback(TargetPtr &InTarget, const char *Txt)
{
	if(InTarget)
	{
		wchar_t Buffer[1024];
		swprintf_s(Buffer, 1024, L"%S", Txt);

		InTarget->SendTTY(Buffer);
	}
}

/**
 * Triggers the TTY callback for outputting a message.
 *
 * @param	Channel	The channel the message is occuring on.
 * @param	Text	The message to be displayed.
 * @param	Target	The target to send the TTY text to.
 */
void FAndroidNetworkManager::NotifyTTYCallback(TargetPtr &InTarget, const string &Channel, const string &Text)
{
	wchar_t Buffer[1024];
	swprintf_s(Buffer, 1024, L"%S: %S", Channel.c_str(), Text.c_str());

	if(InTarget)
	{
		InTarget->SendTTY(Buffer);
	}
}

/**
 *	Sends message using given UDP client.
 *	@param Handle			client to send message to
 *	@param MessageType		sent message type
 *	@param Message			actual text of the message
 *	@return true on success, false otherwise
 */
bool FAndroidNetworkManager::SendToConsole(const TARGETHANDLE Handle, const EDebugServerMessageType MessageType, const string& Message)
{
	TargetPtr Target = GetTarget(Handle);

	if(Target && Target->UDPClient)
	{
		return SendToConsole(Target->UDPClient, MessageType, Message);
	}

	return false;
}

/**
 *	Attempts to determine available targets.
 */
void FAndroidNetworkManager::DetermineTargets()
{
	sockaddr_in LocalAddress;
	LocalAddress.sin_addr.S_un.S_addr = htonl(0x7f000001);
	TargetPtr LocalTarget = AddTarget(LocalAddress);
	CAndroidTarget* Target = ConvertTarget(LocalTarget);
	_time64(&Target->TimeRegistered);
	Target->Name = L"Local Simulator";
	Target->bIsLocal = true;
	if ( Target->UDPClient )
	{
		Target->UDPClient->SetAddress(LocalAddress.sin_addr.s_addr);
	}

	// we don't have Android targets that do networking yet, so no need for the overhead
	// @todo: We may have a way to query for connected devices over USB, which would be a much
	// better way of doing this!
	return;
#if 0
	// Init 'server announce' listener
	ServerResponseListener.SetPort(DefaultDebugChannelSendPort + AndroidOffset);
	ServerResponseListener.SetAttributes(FConsoleSocket::SF_IPv4, FConsoleSocket::ST_Datagram, FConsoleSocket::SP_UDP);
	if(!ServerResponseListener.CreateSocket() ||
		!ServerResponseListener.SetNonBlocking(true) ||
		!ServerResponseListener.Bind())
	{
		DebugOutput(L"Failed to create server response receiver.\n");
		return;
	}

	DebugOutput(L"Determining targets...\n");

	// open a file in the Binaries\IPhone directory that lists non-broadcastable targets
	FILE* TargetFile = fopen("Android\\Targets.txt", "r");
	if (!TargetFile)
	{
		TargetFile = fopen("..\\Android\\Targets.txt", "r");
	}
	
	if (TargetFile)
	{
		char Line[1024];
		bool bFoundEOF = false;
		while (!bFoundEOF)
		{
			bFoundEOF = fgets(Line, 1023, TargetFile) == NULL;
			if (!bFoundEOF)
			{
				FAndroidSocket TempUDPSocket;
				DebugOutput(L"Sending Server Announce to:\n   ");
				DebugOutput(ToWString(string(Line)).c_str());
				DebugOutput(L"\n");
				TempUDPSocket.SetAddress(Line);
				TempUDPSocket.SetPort(DefaultDebugChannelReceivePort + AndroidOffset);
				TempUDPSocket.SetAttributes(FConsoleSocket::SF_IPv4, FConsoleSocket::ST_Datagram, FConsoleSocket::SP_UDP);
				if (TempUDPSocket.CreateSocket())
				{
					// communuication back from a device is flaky, so send a lot of packets
					bool bServerAnnounceResult = SendToConsole(TempUDPSocket, EDebugServerMessageType_ServerAnnounce, "");
					bServerAnnounceResult = SendToConsole(TempUDPSocket, EDebugServerMessageType_ServerAnnounce, "") || bServerAnnounceResult;
					bServerAnnounceResult = SendToConsole(TempUDPSocket, EDebugServerMessageType_ServerAnnounce, "") || bServerAnnounceResult;
				}
			}
		}
		fclose(TargetFile);
	}

	// Broadcast 'server announce' request.
	// Send three requests in case packets are dropped.
	bool bServerAnnounceResult = SendToConsole(Broadcaster, EDebugServerMessageType_ServerAnnounce, "");
	bServerAnnounceResult = SendToConsole(Broadcaster, EDebugServerMessageType_ServerAnnounce, "") || bServerAnnounceResult;
	bServerAnnounceResult = SendToConsole(Broadcaster, EDebugServerMessageType_ServerAnnounce, "") || bServerAnnounceResult;

	if(!bServerAnnounceResult)
	{
		return;
	}

	DebugOutput(L"Sent broadcast message...\n");

	// Give the servers some time to respond (could be shorter, but 100 is too short)
	Sleep(500);

	// Attempt to receive 'server response'
	ReceiveMessages(ServerResponseListener, 50);

	RemoveTimeExpiredTargets();

	ServerResponseListener.Close();
#endif
}

/**
 * Removes the targets that have not been active for a period of time.
 */
void FAndroidNetworkManager::RemoveTimeExpiredTargets()
{
	// Invalidate targets older than fixed no. seconds
	__time64_t CurrentTime;
	_time64(&CurrentTime);

	TargetsLock.Lock();
	{
		vector<TARGETHANDLE> TargetsToRemove;

		for(TargetMap::iterator Iter = Targets.begin(); Iter != Targets.end(); ++Iter)
		{
			CAndroidTarget* Target = ConvertTarget( (*Iter).second );
			if(!Target->bConnected && CurrentTime - Target->TimeRegistered > 10)
			{
				TargetsToRemove.push_back((*Iter).first);
			}
		}

		for(size_t Index = 0; Index < TargetsToRemove.size(); ++Index)
		{
			TargetMap::iterator Iter = Targets.find(TargetsToRemove[Index]);
			TargetPtr Target = (*Iter).second;

			Targets.erase(Iter);

			if ( Target->TCPClient )
			{
				Target->TCPClient->Close();
			}
			if ( Target->UDPClient )
			{
				Target->UDPClient->Close();
			}
		}
	}
	TargetsLock.Unlock();
}

/**
 *	Attempts to receive (and dispatch) messages from given client.
 *	@param Client client to attempt to receive message from
 *	@param AttemptsCount no. attempts (between each fixed miliseconds waiting happens)
 */
void FAndroidNetworkManager::ReceiveMessages(FAndroidSocket& UDPClient, int AttemptsCount)
{
	while(AttemptsCount > 0)
	{
		// Attempt to receive message
		char* Data = NULL;
		int DataSize = 0;
		EDebugServerMessageType MessageType;
		sockaddr_in ServerAddress;

		if(!ReceiveFromConsole(UDPClient, MessageType, Data, DataSize, ServerAddress))
		{
			// See if we should stop attempting to receive messages
			AttemptsCount--;
			if(AttemptsCount == 0)
			{
				return;
			}

			// Try again
			Sleep(10);
			continue;
		}

		int CurDataSize = DataSize;

		// Handle each supported message type
		switch(MessageType)
		{
		case EDebugServerMessageType_ServerResponse:
			{
				// See if target already exists
				TargetPtr Target = GetTarget(ServerAddress);

				// Add target
				if(!Target)
				{
					Target = AddTarget(ServerAddress);
				}

				CAndroidTarget* AndroidTarget = ConvertTarget(Target);

				// Set up target info
				AndroidTarget->Configuration = L"Unknown configuration";
				AndroidTarget->ComputerName = ToWString(CByteStreamConverter::LoadStringBE(Data, CurDataSize));

				if(AndroidTarget->ComputerName.length() == 0)
				{
					DebugOutput(L"Corrupt packet received\n");
					RemoveTarget(Target.GetHandle());
					continue;
				}

				AndroidTarget->GameName = ToWString(CByteStreamConverter::LoadStringBE(Data, CurDataSize));
				AndroidTarget->GameType = (EGameType)*(Data++);
				AndroidTarget->GameTypeName = ToWString(ToString(AndroidTarget->GameType));
				AndroidTarget->PlatformType = (FConsoleSupport::EPlatformType)(unsigned char)*(Data++);
				AndroidTarget->ListenPortNo = CByteStreamConverter::LoadIntBE(Data, CurDataSize);
				_time64(&AndroidTarget->TimeRegistered);

				// Set up target connection
				if ( AndroidTarget->UDPClient )
				{
					AndroidTarget->UDPClient->SetAddress(ServerAddress.sin_addr.s_addr);
					AndroidTarget->UDPClient->SetPort(DefaultDebugChannelReceivePort + AndroidOffset);
				}
				if ( AndroidTarget->TCPClient )
				{
					AndroidTarget->TCPClient->SetAddress(ServerAddress.sin_addr.s_addr);
					AndroidTarget->TCPClient->SetPort(DefaultDebugChannelListenPort + AndroidOffset);
				}
				AndroidTarget->UpdateName();

				break;
			}

		case EDebugServerMessageType_ServerDisconnect:
			{
				// Figure out target
				TargetPtr Target = GetTarget(ServerAddress);
				if(!Target)
				{
					DebugOutput(L"Received 'server disconnect' from unknown server.\n");
					break;
				}

				// Disconnect from server
				if ( Target->UDPClient )
				{
					Target->UDPClient->Close();
				}
				if ( Target->TCPClient )
				{	
					Target->TCPClient->Close();
				}

				break;
			}

		case EDebugServerMessageType_ServerTransmission:
			{
				DebugOutput(L"Server TEXT transmission.\n");

				// Figure out target
				TargetPtr Target = GetTarget(ServerAddress);
				if(!Target)
				{
					DebugOutput(L"Received TEXT transmission from unknown server.\n");
					break;
				}

				// Retrieve channel and text
				const int ChannelEnum = CByteStreamConverter::LoadIntBE(Data, CurDataSize);
				string Channel;

				// Enums mirrored from FDebugServer!
				if(ChannelEnum == 0)
				{
					Channel="DEBUG";
				}
				else if(ChannelEnum == 1)
				{
					Channel="REMOTE";
				}
				else if(ChannelEnum == 2)
				{
					Channel="MEM";
				}
				else
				{
					Channel="UNKNOWN";
				}

				const string Text = CByteStreamConverter::LoadStringBE(Data, CurDataSize);

				DebugOutput(L"Server TEXT transmission decoded.\n");
				DebugOutput(L"Channel: ");
				DebugOutput(ToWString(Channel).c_str());
				DebugOutput(L"\n");
				DebugOutput(L"Text: ");
				DebugOutput(ToWString(Text).c_str());
				DebugOutput(L"\n");

				NotifyTTYCallback(Target, Channel, Text);
				break;
			}

		default:
			{
				DebugOutput(L"Some server sent message of unknown type.\n");
			}
		}
	}
}

/**
 *	Attempts to receive message from given UDP client.
 *	@param Client client to receive message from
 *	@param MessageType received message type
 *	@param Data received message data
 *	@param DataSize received message size
 *	@return true on success, false otherwise
 */
bool FAndroidNetworkManager::ReceiveFromConsole(FAndroidSocket& Client, EDebugServerMessageType& MessageType, char*& Data, int& DataSize, sockaddr_in& SenderAddress)
{
	// Attempt to receive bytes
#define MAX_BUFFER_SIZE (1 << 12)
	char Buffer[MAX_BUFFER_SIZE];
	const int NumReceivedBytes = Client.RecvFrom(Buffer, MAX_BUFFER_SIZE, SenderAddress);

	// The message should contain at least 2 characters identyfing message type
	if (NumReceivedBytes < 2)
	{
		return false;
	}

	// Determine message type
	char MessageTypeString[3];
	sprintf_s(MessageTypeString, 3, "%c%c", Buffer[0], Buffer[1]);
	MessageType = ToDebugServerMessageType(MessageTypeString);

	DebugOutput(L"Received message of type: ");
	DebugOutput(ToWString(string(MessageTypeString)).c_str());
	DebugOutput(L"\n");

	// Determine message content
	// Skip the two bytes that pad out the message header.
	DataSize = NumReceivedBytes - 2;
	Data = new char[DataSize];
	memcpy(Data, Buffer + 2, DataSize);

	return true;
}

/**
 *	Sends message using given UDP client.
 *	@param Client client to send message to
 *	@param MessageType sent message type
 *	@param Message actual text of the message
 *	@return true on success, false otherwise
 */
bool FAndroidNetworkManager::SendToConsole(FAndroidSocket& Client, const EDebugServerMessageType MessageType, const string& Message)
{
	// Compose message to be sent (header, followed by actual message)
	const int BufferLength = DebugServerMessageTypeNameLength + (Message.length() > 0 ? (sizeof(int) + (int) Message.length()) : 0);
	char* Buffer = new char[BufferLength];

	memcpy(Buffer, ToString(MessageType), DebugServerMessageTypeNameLength);
	if (Message.length() > 0)
	{
		CByteStreamConverter::StoreIntBE(Buffer + DebugServerMessageTypeNameLength, (int) Message.length());
		memcpy(Buffer + DebugServerMessageTypeNameLength + sizeof(int), Message.c_str(), Message.length());
	}

	// Send the message
	const int NumSentBytes = Client.SendTo(Buffer, BufferLength);

	// Clean up
	delete[] Buffer;

	return NumSentBytes == BufferLength;
}

/**
 * Connects to the target with the specified name.
 *
 * @param	TargetName		The name of the target to connect to.
 * @return	Handle to the target that was connected or else INVALID_TARGETHANDLE.
 */
TARGETHANDLE FAndroidNetworkManager::ConnectToTarget(const wchar_t* TargetName)
{
	// Find the target
	TargetPtr CurrentTarget;

	for(TargetMap::iterator Iter = Targets.begin(); Iter != Targets.end(); ++Iter)
	{
		if((*Iter).second->Name == TargetName)
		{
			CurrentTarget = (*Iter).second;
			break;
		}
	}

	if(ConnectToTarget(CurrentTarget))
	{
		CurrentTarget.GetHandle();
	}

	return INVALID_TARGETHANDLE;
}

/**
 * Connects to the target with the specified handle.
 *
 * @param	Handle		The handle of the target to connect to.
 */
bool FAndroidNetworkManager::ConnectToTarget(TARGETHANDLE Handle)
{
	TargetPtr Target = GetTarget(Handle);

	return ConnectToTarget(Target);
}

/**
 * Connects to the target.
 *
 * @param	Handle		The handle of the target to connect to.
 */
bool FAndroidNetworkManager::ConnectToTarget(TargetPtr &InTarget)
{
	// Target not found
	if(!InTarget)
	{
		return false;
	}

	CAndroidTarget* Target = ConvertTarget(InTarget);

	if (Target->bIsLocal)
	{
		return true;
	}

	// if the target is already connected, then close it down before starting it back up
	if (Target->UDPClient && Target->TCPClient && Target->TCPClient->IsValid())
	{
		SendToConsole(&Target->UDPClient, EDebugServerMessageType_ClientDisconnect);
		SendToConsole(&Target->UDPClient, EDebugServerMessageType_ClientDisconnect);
		SendToConsole(&Target->UDPClient, EDebugServerMessageType_ClientDisconnect);
		Sleep(2000);
		Target->UDPClient->Close();
		Target->TCPClient->Close();
	}

	if(Target->UDPClient && !Target->UDPClient->IsValid())
	{
		// Create the socket used to send messages to server
		if(!Target->UDPClient->CreateSocket())
		{
			return false;
		}

		// Inform server about connection
		const bool connectionRequestResult = SendToConsole(&Target->UDPClient, EDebugServerMessageType_ClientConnect);
		if(!connectionRequestResult)
		{
			Target->UDPClient->Close();
			return false;
		}
	}


	if(Target->TCPClient)
	{
		FAndroidSocket* TCPClient = (FAndroidSocket*)(Target->TCPClient);
		if(!TCPClient->IsValid() || !TCPClient->IsAssociatedWithIOCP())
		{
			if(!TCPClient->CreateSocket())
			{
				return false;
			}

			if(!TCPClient->AssociateWithIOCP(IOCompletionPort))
			{
				return false;
			}

			//TCPClient.SetNonBlocking(true);
			if(!TCPClient->Connect())
			{
				return false;
			}

			OverlappedEventArgs *Args = new OverlappedEventArgs(OVT_Recv);
			Args->Owner = Target;

			DWORD BytesRecvd = 0;
			if(!TCPClient->RecvAsync(&Args->WSABuffer, 1, BytesRecvd, Args))
			{
				return false;
			}
		}
	}

	// Add initial info for the user
	if ( !Target->UDPClient )
	{
		return false;
	}

	const unsigned int IP = Target->UDPClient->GetIP();
	NotifyTTYCallback(InTarget, "CONNECTION",
		string("Connected to server:") +
		"\n\tIP: " + inet_ntoa(*(in_addr*) &IP) +
		"\n\tComputer name: " + ToString(Target->ComputerName) +
		"\n\tGame name: " + ToString(Target->GameName) +
		"\n\tGame type: " + ToString((EGameType)Target->GameType) + "\n");

	Target->bConnected = true;

	// initialize our connection time for timeout
	_time64(&Target->LastPingReplyTime);

	return true;
}

/**
 * Exists for compatability with UnrealConsole. Index is disregarded and CurrentTarget is disconnected if it contains a valid pointer.
 *
 * @param	Handle		Handle to the target to disconnect.
 */
void FAndroidNetworkManager::DisconnectTarget(const TARGETHANDLE Handle)
{
	TargetPtr Target = GetTarget(Handle);

	if(Target && Target->UDPClient)
	{
		Target->bConnected = false;

		// Inform server about disconnection
		// Send three messages in case packets are dropped.
		SendToConsole(&Target->UDPClient, EDebugServerMessageType_ClientDisconnect);
		SendToConsole(&Target->UDPClient, EDebugServerMessageType_ClientDisconnect);
		SendToConsole(&Target->UDPClient, EDebugServerMessageType_ClientDisconnect);

		RemoveTarget(Handle);
	}
}

/**
 * Handles a packet.
 *
 * @param	Data				The packet data.
 * @param	BytesRecv			The size of the packet data.
 * @param	Target				The target that received the packet.
 */
void FAndroidNetworkManager::HandlePacket(char* Data, const int BytesRecv, TargetPtr& InTarget)
{
	if ( !InTarget )
	{
		return;
	}

	bool bAllocated = false;
	char *Buf = Data;
	int ActualBufSize = BytesRecv;

	CAndroidTarget* Target = ConvertTarget(InTarget);

	if(Target->PartialPacketBuffer && Target->PartialPacketBufferSize > 0)
	{
		bAllocated = true;
		ActualBufSize = Target->PartialPacketBufferSize + BytesRecv;
		Buf = new char[ActualBufSize];
		memcpy_s(Buf, ActualBufSize, Target->PartialPacketBuffer, Target->PartialPacketBufferSize);
		memcpy_s(&Buf[Target->PartialPacketBufferSize], ActualBufSize - Target->PartialPacketBufferSize, Data, BytesRecv);

		delete [] Target->PartialPacketBuffer;
		Target->PartialPacketBuffer = NULL;
		Target->PartialPacketBufferSize = 0;
	}

	int ChannelEnum = 0;
	string Text;
	string ChannelText;
	int BytesConsumed = 0;
	char *BufPtr = Buf;
	bool bIsServerReply;

	while(CheckForCompleteMessage(BufPtr, ActualBufSize, bIsServerReply, ChannelEnum, Text, BytesConsumed))
	{
		BufPtr += BytesConsumed;
		ActualBufSize -= BytesConsumed;

		if(Target->bConnected)
		{
			if (bIsServerReply)
			{
				// set the last heartbeat time of the target
				_time64(&Target->LastPingReplyTime);
			}
			else
			{
				switch(ChannelEnum)
				{
				case 0:
					{
						ChannelText = "DEBUG";
						break;
					}
				case 1:
					{
						ChannelText = "REMOTE";
						break;
					}
				case 2:
					{
						ChannelText = "MEM";
						break;
					}
				default:
					{
						ChannelText = "UNKNOWN";
						break;
					}
				}

				NotifyTTYCallback(InTarget, ChannelText, Text);
			}
		}
	}

	if(ActualBufSize > 0)
	{
		Target->PartialPacketBuffer = new char[ActualBufSize];
		Target->PartialPacketBufferSize = ActualBufSize;

		memcpy_s(Target->PartialPacketBuffer, Target->PartialPacketBufferSize, BufPtr, ActualBufSize);
	}

	if(bAllocated)
	{
		delete [] Buf;
	}
}

/**
 * Parses packet data for a message.
 *
 * @param	TCPData				The packet data.
 * @param	TCPDataSize			The size of the packet data.
 * @param	bIsServerReply		If TRUE, this message was just a server ping reply, not a TTY message
 * @param	ChannelEnum			Receives the channel the packet is operating on.
 * @param	Text				Receives the text of the message.
 * @param	NumBytesConsumed	The number of bytes read.
 */
bool FAndroidNetworkManager::CheckForCompleteMessage(char* TCPData, const int TCPDataSize, bool& bIsServerReply, int& ChannelEnum, string& Text, int& NumBytesConsumed) const
{
	Text.clear();
	NumBytesConsumed = 0;

	// We've stopped receiving data for now.
	// Have a look at the buffered data and see if there's enough to start parsing.
	const int PacketHeaderSize =
		//2							// Message header padding
		+ sizeof(char) * 2		// Message header
		+ 1//4							// Channel enum
		+ sizeof(int);				// TextLength

	// first look at the message type (2 chars)
	if (TCPDataSize < 2)
	{
		return false;
	}

	// Check that the message type is a 'server transmission' or 'server ping reply'.
	char MessageTypeString[3];
	sprintf_s(MessageTypeString, 3, "%c%c", TCPData[0], TCPData[1]);
	const EDebugServerMessageType MessageType = ToDebugServerMessageType(MessageTypeString);
	if(MessageType != EDebugServerMessageType_ServerTransmission && MessageType != EDebugServerMessageType_ServerPingReply)
	{
		DebugOutput(L"UNKNOWN MESSAGE TYPE\n");
		return false;
	}

	bIsServerReply = MessageType == EDebugServerMessageType_ServerPingReply;

	// ping replies just need the header, so we are done
	if (bIsServerReply)
	{
		NumBytesConsumed = 2;
		return true;
	}
	

	if(TCPDataSize < PacketHeaderSize)
	{
		return false;
	}

	// Retrieve the channel enum.
	ChannelEnum = (int)TCPData[2];

	// Copy the data over to a temp buffer, skipping the first HEADER_OFFSET bytes.
	const int HEADER_OFFSET = 3;//4;
	char* Data = &TCPData[HEADER_OFFSET];

	int CurDataSize = TCPDataSize - HEADER_OFFSET;

	// Retrieve the channel enum.
	//ChannelEnum = CByteStreamConverter::LoadIntBE(Data);
	// Retrieve the text buffer.  This will advance the Data pointer by sizeof(int).
	const int StringLen = CByteStreamConverter::LoadIntBE(Data, CurDataSize);

	// At this point, we're at the end of the header.  See if the entire text buffer has been received.
	if(TCPDataSize - PacketHeaderSize < StringLen)
	{
		return false;
	}

	Text.assign(Data, StringLen);
	NumBytesConsumed = PacketHeaderSize + StringLen;

	assert((int)Text.length() == StringLen);
	assert(NumBytesConsumed <= TCPDataSize);

	return true;
}

/**
 * The callback for IOCP worker threads. Handles IO events for the targets.
 *
 * @param	Data	A pointer to the owning FAndroidNetworkManager instance.
 */
unsigned int __stdcall FAndroidNetworkManager::WorkerThreadProc(void *Data)
{
	FAndroidNetworkManager *Mgr = (FAndroidNetworkManager*)Data;

	ULONG_PTR CompletionKey = NULL;
	OverlappedEventArgs *Overlapped = NULL;
	DWORD BytesTransferred  = 0;

	while(WaitForSingleObject(Mgr->ThreadCleanupEvent, 0) != WAIT_OBJECT_0)
	{
		BOOL bReturn = GetQueuedCompletionStatus(Mgr->IOCompletionPort, &BytesTransferred, &CompletionKey, (LPOVERLAPPED*)&Overlapped, 100);

		if(bReturn)
		{
			if(BytesTransferred > 0)
			{
				Mgr->HandlePacket(Overlapped->Buffer, BytesTransferred, Overlapped->Owner);
			}

			ZeroMemory(Overlapped, sizeof(OVERLAPPED));
			if(Overlapped->Owner->TCPClient && !Overlapped->Owner->TCPClient->RecvAsync(&Overlapped->WSABuffer, 1, BytesTransferred, Overlapped))
			{
				Mgr->NotifyTTYCallback(Overlapped->Owner, "Error", "Could not being an asynchronous recv on the current target!");
			}
		}
		else if(Overlapped)
		{
			Mgr->RemoveTarget(Overlapped->Owner.GetHandle());
			delete Overlapped;
		}

		Overlapped = NULL;
	}

	return 0;
}
