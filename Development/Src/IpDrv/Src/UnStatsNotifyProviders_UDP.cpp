/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This file contains the implementation of the networked stat notify provider
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

#if STATS

/**
 * Initializes the threads that handle the network layer
 */
UBOOL FStatNotifyProvider_UDP::Init(void)
{
	INT ListenPort = 13000;
	// Read the port that is to be used for listening for client broadcasts
	GConfig->GetInt(TEXT("StatNotifyProviders.StatNotifyProvider_UDP"),
		TEXT("ListenPort"),ListenPort,GEngineIni);
	INT StatsTrafficPort = 13002;
	// Read the port that is to be used for listening for client broadcasts
	GConfig->GetInt(TEXT("StatNotifyProviders.StatNotifyProvider_UDP"),
		TEXT("StatsTrafficPort"),StatsTrafficPort,GEngineIni);
	// Validate that the listen/traffic ports don't collide
	if (ListenPort + 1 == StatsTrafficPort)
	{
		StatsTrafficPort++;
		debugf(TEXT("Port Collision: Changed stats traffic port to %d"),StatsTrafficPort);
	}
	// Create the object that will manage our client connections
	ClientList = ::new FStatClientList(StatsTrafficPort);
	// Create the listener object
	ListenerRunnable = new FStatListenerRunnable(ListenPort,StatsTrafficPort,
		ClientList);
	// Now create the thread that will do the work
	ListenerThread = GThreadFactory->CreateThread(ListenerRunnable,TEXT("StatListener"),FALSE,FALSE,
		8 * 1024);
	if (ListenerThread != NULL)
	{
#if XBOX
		// See UnXenon.h
		ListenerThread->SetProcessorAffinity(STATS_LISTENER_HWTHREAD);
#endif
		// Created externally so that it can be locked before the sender thread has
		// gone through its initialization
		FCriticalSection* AccessSync = GSynchronizeFactory->CreateCriticalSection();
		if (AccessSync != NULL)
		{
			// Now create the sender thread
			SenderRunnable = new FStatSenderRunnable(AccessSync,ClientList);
			// Now create the thread that will do the work
			SenderThread = GThreadFactory->CreateThread(SenderRunnable,TEXT("StatSender"),FALSE,FALSE,
				12 * 1024);
			if (SenderThread != NULL)
			{
#if XBOX
				// See UnXenon.h
				SenderThread->SetProcessorAffinity(STATS_SENDER_HWTHREAD);
#endif
			}
			else
			{
				debugf(NAME_Error,TEXT("Failed to create FStatNotifyProvider_UDP send thread"));
			}
		}
		else
		{
			debugf(NAME_Error,TEXT("Failed to create FStatNotifyProvider_UDP send thread"));
		}
	}
	else
	{
		debugf(NAME_Error,TEXT("Failed to create FStatNotifyProvider_UDP listener thread"));
	}
	return ListenerThread != NULL && SenderThread != NULL;
}

/**
 * Copies the specified values to use for the listener thread
 *
 * @param InPort the port to listen for client requests on
 * @param InStatsTrafficPort the port that stats updates will be sent on
 * @param InClients the list that clients will be added to/removed from
 */
FStatNotifyProvider_UDP::FStatListenerRunnable::FStatListenerRunnable(INT InPort,
	INT InStatsTrafficPort,FStatClientList* InClients) :
	ListenPort(InPort),
	StatsTrafficPort(InStatsTrafficPort),
	TimeToDie(NULL),
	ClientList(InClients),
	ListenSocket(NULL)
{
}

/**
 * Binds to the specified port
 *
 * @return True if initialization was successful, false otherwise
 */
UBOOL FStatNotifyProvider_UDP::FStatListenerRunnable::Init(void)
{
	// Create the event used to tell our threads to bail
	TimeToDie = GSynchronizeFactory->CreateSynchEvent(TRUE);
	UBOOL bOk = TimeToDie != NULL;
	if (bOk)
	{
		ListenAddr.SetPort(ListenPort);
		ListenAddr.SetIp(getlocalbindaddr(*GWarn));
		// Now create and set up our sockets (force UDP even when speciliazed
		// protocols exist)
		ListenSocket = GSocketSubsystem->CreateDGramSocket(TEXT("FStatNotifyProfer_UDP"),TRUE);
		if (ListenSocket != NULL)
		{
			ListenSocket->SetReuseAddr();
			ListenSocket->SetNonBlocking();
			ListenSocket->SetRecvErr();
			// Bind to our listen port
			if (ListenSocket->Bind(ListenAddr))
			{
				INT SizeSet = 0;
				// Make the send buffer large so we don't overflow it
				bOk = ListenSocket->SetSendBufferSize(0x20000,SizeSet);
			}
		}
	}
	return bOk;
}

/**
 * Sends an announcement message on start up and periodically thereafter
 *
 * @return The exit code of the runnable object
 */
DWORD FStatNotifyProvider_UDP::FStatListenerRunnable::Run(void)
{
	BYTE PacketData[512];
	// Check every 1/2 second for a client request, while the death event
	// isn't signaled
	while (TimeToDie->Wait(500) == FALSE)
	{
		RETURN_VAL_IF_EXIT_REQUESTED(0);

		// Default to no data being read
		INT BytesRead = 0;
		if (ListenSocket != NULL)
		{
			FInternetIpAddr SockAddr;
			// Read from the socket and process if some was read
			ListenSocket->RecvFrom(PacketData,512,BytesRead,SockAddr);
			if (BytesRead > 0)
			{
				// Process this data
				ProcessPacket(SockAddr,PacketData,BytesRead);
			}
		}
	}
	return 0;
}

/**
 * Receives data from a given call to Poll(). For the beacon, we
 * just send an announcement directly to the source address
 *
 * @param SrcAddr the source address of the request
 * @param Data the packet data
 * @param Count the number of bytes in the packet
 */
void FStatNotifyProvider_UDP::FStatListenerRunnable::ProcessPacket(FIpAddr SrcAddr,
	BYTE* Data,INT Count)
{
	// Check for a "server announce" request
	if (Count == 2 && Data[0] == 'S' && Data[1] == 'A')
	{
		debugfSuppressed(NAME_DevStats,TEXT("Sending server announce to %s"),
			*SrcAddr.ToString(TRUE));
		// Build the packet containing the information the client needs
		FNboSerializeBuffer QueryResponsePacket;
		// Format is SR, computer name, game name, game type, platform type
		QueryResponsePacket << 'S' << 'R' << appComputerName() << GGameName <<
			(BYTE)appGetStatGameType() << (BYTE)appGetPlatformType() <<
			StatsTrafficPort;
// @todo joeg - add a "requires password" option
		// Respond on the next port from what we are listening to
		SrcAddr.Port = ListenPort + 1;
		INT BytesSent;
		// Send our info to the client
		ListenSocket->SendTo(QueryResponsePacket,QueryResponsePacket.GetByteCount(),
			BytesSent,SrcAddr.GetSocketAddress());
	}
	// Check for a client connect request
	else if (Count == 2 && Data[0] == 'C' && Data[1] == 'C')
	{
		debugfSuppressed(NAME_DevStats,TEXT("Received client connect from %s"),
			*SrcAddr.ToString(TRUE));
		// Add this address to our update list
		ClientList->AddClient(SrcAddr);
		// Force scoped cycle stats to be enabled.
		GForceEnableScopedCycleStats++;
	}
	// Check for a client disconnect
	else if (Count == 2 && Data[0] == 'C' && Data[1] == 'D')
	{
		debugfSuppressed(NAME_DevStats,TEXT("Received client disconnect from %s"),
			*SrcAddr.ToString(TRUE));
		// Remove this address from our update list
		ClientList->RemoveClient(SrcAddr);
		// Decrease counter forcing scoped cycle stats to be enabled.
		GForceEnableScopedCycleStats--;
	}
	else
	{
		// Log the unknown request type
		debugfSuppressed(NAME_DevStats,TEXT("Unknown request of size %d from %s"),Count,
			*SrcAddr.ToString(TRUE));
	}
}

/**
 * Shuts down the network threads
 */
void FStatNotifyProvider_UDP::Destroy(void)
{
	// Tell the threads to stop running
	if (ListenerRunnable != NULL)
	{
		ListenerRunnable->Stop();
		ListenerThread->WaitForCompletion();
		GThreadFactory->Destroy(ListenerThread);
		delete ListenerRunnable;
		ListenerRunnable = NULL;
		ListenerThread = NULL;
	}
	if (SenderRunnable != NULL)
	{
		SenderRunnable->Stop();
		SenderThread->WaitForCompletion();
		GThreadFactory->Destroy(SenderThread);
		delete SenderRunnable;
		SenderRunnable = NULL;
		SenderThread = NULL;
	}
	// Delete the clients class
	delete ClientList;
}

/**
 * Copies the specified values to use for the send thread
 *
 * @param InSync the critical section to use
 * @param InClients the list that clients will be added to/removed from
 */
FStatNotifyProvider_UDP::FStatSenderRunnable::FStatSenderRunnable(
	FCriticalSection* InSync,FStatClientList* InClients) :
	bIsTimeToDie(FALSE),
	WorkEvent(NULL),
	QueueEmptyEvent(NULL),
	SendFromIndex(0),
	QueueIntoIndex(1),
	AccessSync(InSync),
	ClientList(InClients),
	SenderSocket(NULL)
{
}

/**
 * Binds to the specified port
 *
 * @return True if initialization was successful, false otherwise
 */
UBOOL FStatNotifyProvider_UDP::FStatSenderRunnable::Init(void)
{
	UBOOL bSuccess = FALSE;
	// Create the event used to tell our threads to bail
	WorkEvent = GSynchronizeFactory->CreateSynchEvent();
	QueueEmptyEvent = GSynchronizeFactory->CreateSynchEvent();
	if (QueueEmptyEvent != NULL)
	{
		QueueEmptyEvent->Trigger();
		// Create the socket used for sending (force UDP)
		SenderSocket = GSocketSubsystem->CreateDGramSocket(TEXT("FStatNotifyProvider_UDP"),TRUE);
		if (SenderSocket != NULL)
		{
			// Set it to broadcast mode
			bSuccess = SenderSocket->SetBroadcast();
		}
	}
	return WorkEvent != NULL && QueueEmptyEvent != NULL && bSuccess;
}

/**
 * Sends an announcement message on start up and periodically thereafter
 *
 * @return The exit code of the runnable object
 */
DWORD FStatNotifyProvider_UDP::FStatSenderRunnable::Run(void)
{
	do
	{
		// Wait for there to be work to do
		WorkEvent->Wait();
		// Holds the copy of clients we are sending to
		FClientConnection Clients[10];
		DWORD ClientCount = 0;
		// Copy the clients list
		for (FClientConnection* Client = ClientList->BeginIterator();
			Client != NULL && ClientCount < 10;
			Client = ClientList->GetNext())
		{
			Clients[ClientCount] = *Client;
			ClientCount++;
			Client->bNewConnection = FALSE;
		}
		ClientList->EndIterator();
		// Send the descriptions to any new clients
		SendDescriptions(Clients,ClientCount);
		// Tell all of the clients a new frame's data is coming
		SendNewFramePacket(Clients,ClientCount);
		// Now process the sets of stats that need to be sent
		SendCycleCounters(Clients,ClientCount);
		SendDwordCounters(Clients,ClientCount);
		SendFloatCounters(Clients,ClientCount);
		// Empty the packet list and trigger our "done" event
		SendQueues[SendFromIndex].Reset();
		// Trigger the done event
		QueueEmptyEvent->Trigger();
	}
	while (bIsTimeToDie == FALSE);
	return 0;
}

/**
 * Adds a packet to the descriptions queue. The entire description
 * queue is sent to a newly connected client
 *
 * @param Packet the data that is to be sent to all clients
 */
void FStatNotifyProvider_UDP::FStatSenderRunnable::AddDescriptionPacket(
	FNboSerializeBuffer* Packet)
{
	FScopeLock sl(AccessSync);
	DescriptionsQueue.AddItem(Packet);
}

/**
 * Handles sending all of the descriptions to a new client
 *
 * @param Clients the client list that needs to be checked
 * @param ClientCount the number of clients to check
 */
void FStatNotifyProvider_UDP::FStatSenderRunnable::SendDescriptions(
	FClientConnection* Clients,DWORD ClientCount)
{
	// Check each client for data to send to
	for (DWORD Index = 0; Index < ClientCount; Index++)
	{
		if (Clients[Index].bNewConnection == TRUE)
		{
			// Make sure nothing is changing the tarray while we iterate it
			FScopeLock sl(AccessSync);
			// Send all of the description packets each in FIFO order
			for (INT DescIndex = 0; DescIndex < DescriptionsQueue.Num(); DescIndex++)
			{
				INT BytesSent;
				SenderSocket->SendTo(*DescriptionsQueue(DescIndex),
					DescriptionsQueue(DescIndex)->GetByteCount(),
					BytesSent,
					Clients[Index].Address.GetSocketAddress());
			}
			Clients[Index].bNewConnection = FALSE;
		}
	}
}

/**
 * Sends the new frame packet to all clients
 *
 * @param Clients the clients that will receive the packets
 * @param ClientCount the number of clients to send to
 */
void FStatNotifyProvider_UDP::FStatSenderRunnable::SendNewFramePacket(
	FClientConnection* Clients,DWORD ClientCount)
{
	FNboSerializeBuffer Packet;
	Packet << 'N' << 'F' << SendQueues[SendFromIndex].FrameNumber;
	// Send the packet to each client
	for (DWORD Index = 0; Index < ClientCount; Index++)
	{
		INT BytesSent;
		SenderSocket->SendTo(Packet,Packet.GetByteCount(),BytesSent,
			Clients[Index].Address.GetSocketAddress());
	}
}

/**
 * Handles sending all of the cycle counters to each client
 *
 * @param Clients the clients that will receive the packets
 * @param ClientCount the number of clients to send to
 */
void FStatNotifyProvider_UDP::FStatSenderRunnable::SendCycleCounters(
	FClientConnection* Clients,DWORD ClientCount)
{
	// Build a packet for each counter in our queue
	for (DWORD Index = 0; Index < SendQueues[SendFromIndex].NumCycleCounters; Index++)
	{
		FNboSerializeBuffer Packet;
		// Add the data to our packet (UC == Update Cycle Counter)
		Packet << 'U' << 'C'
			<< SendQueues[SendFromIndex].CycleCounters[Index].StatId
			<< SendQueues[SendFromIndex].CycleCounters[Index].InstanceId
			<< SendQueues[SendFromIndex].CycleCounters[Index].ParentInstanceId
			<< SendQueues[SendFromIndex].CycleCounters[Index].ThreadId
			<< SendQueues[SendFromIndex].CycleCounters[Index].Value
			<< SendQueues[SendFromIndex].CycleCounters[Index].CallsPerFrame;
		// Send the packet to each client
		for (DWORD ClientIndex = 0; ClientIndex < ClientCount; ClientIndex++)
		{
			INT BytesSent;
			SenderSocket->SendTo(Packet,Packet.GetByteCount(),BytesSent,
				Clients[ClientIndex].Address.GetSocketAddress());
		}
	}
}

/**
 * Handles sending all of the dword counters to each client
 *
 * @param Clients the clients that will receive the packets
 * @param ClientCount the number of clients to send to
 */
void FStatNotifyProvider_UDP::FStatSenderRunnable::SendDwordCounters(
	FClientConnection* Clients,DWORD ClientCount)
{
	// Build a packet for each counter in our queue
	for (DWORD Index = 0; Index < SendQueues[SendFromIndex].NumDwordCounters; Index++)
	{
		FNboSerializeBuffer Packet;
		// Add the data to our packet (UD == Update Dword Counter)
		Packet << 'U' << 'D'
			<< SendQueues[SendFromIndex].DwordCounters[Index].StatId
			<< SendQueues[SendFromIndex].DwordCounters[Index].Value;
		// Send the packet to each client
		for (DWORD ClientIndex = 0; ClientIndex < ClientCount; ClientIndex++)
		{
			INT BytesSent;
			SenderSocket->SendTo(Packet,Packet.GetByteCount(),BytesSent,
				Clients[ClientIndex].Address.GetSocketAddress());
		}
	}
}

/**
 * Handles sending all of the float counters to each client
 *
 * @param Clients the clients that will receive the packets
 * @param ClientCount the number of clients to send to
 */
void FStatNotifyProvider_UDP::FStatSenderRunnable::SendFloatCounters(
	FClientConnection* Clients,DWORD ClientCount)
{
	// Build a packet for each counter in our queue
	for (DWORD Index = 0; Index < SendQueues[SendFromIndex].NumFloatCounters; Index++)
	{
		FNboSerializeBuffer Packet;
		// Add the data to our packet (UF == Update Float Counter)
		Packet << 'U' << 'F'
			<< SendQueues[SendFromIndex].FloatCounters[Index].StatId
			<< SendQueues[SendFromIndex].FloatCounters[Index].Value;
		// Send the packet to each client
		for (DWORD ClientIndex = 0; ClientIndex < ClientCount; ClientIndex++)
		{
			INT BytesSent;
			SenderSocket->SendTo(Packet,Packet.GetByteCount(),BytesSent,
				Clients[ClientIndex].Address.GetSocketAddress());
		}
	}
}

/**
 * Swaps the queues that are being written to/read from
 */
void FStatNotifyProvider_UDP::FStatSenderRunnable::SwapQueues(void)
{
	// Wait for the thread to be done processing its queue
	QueueEmptyEvent->Wait();
	// Now swap the indices (changes 1 to 0 and vice versa)
	SendFromIndex ^= TRUE;
	QueueIntoIndex ^= TRUE;
	// Tell the thread it has work to do
	WorkEvent->Trigger();
}

/**
 * Cleans up any allocated resources
 */
void FStatNotifyProvider_UDP::FStatSenderRunnable::Exit(void)
{
	GSynchronizeFactory->Destroy(WorkEvent);
	WorkEvent = NULL;
	GSynchronizeFactory->Destroy(AccessSync);
	AccessSync = NULL;
	GSynchronizeFactory->Destroy(QueueEmptyEvent);
	QueueEmptyEvent = NULL;
	// Clean up the prebuilt set of description packets
	for (INT Index = 0; Index < DescriptionsQueue.Num(); Index++)
	{
		delete DescriptionsQueue(Index);
	}
	DescriptionsQueue.Empty();
	// Clean up socket
	if (SenderSocket != NULL)
	{
		GSocketSubsystem->DestroySocket(SenderSocket);
		SenderSocket = NULL;
	}
}

/**
 * Builds a packet from the passed in data. Queues that packet to be sent
 * on the sender thread. The packet will be sent to every client in the
 * clients list
 *
 * @param StatId the id of the stat that is being written out
 * @param GroupId the id of the group the stat belongs to
 * @param ParentId the id of the stat that is being written out
 * @param InstanceId the instance id of the stat being written
 * @param ParentInstanceId the instance id of parent stat
 * @param ThreadId the thread this stat is for
 * @param Value the value of the stat to write out
 * @param CallsPerFrame the number of calls for this frame
 */
void FStatNotifyProvider_UDP::WriteStat(DWORD StatId,DWORD GroupId,DWORD ParentId,
	DWORD InstanceId,DWORD ParentInstanceId,DWORD ThreadId,DWORD Value,
	DWORD CallsPerFrame)
{
	// Don't bother building a packet if there are no clients connected
	if (ClientList->GetConnectionCount() > 0)
	{
		FPerFrameStatData& Queue = SenderRunnable->GetPerFrameQueue();
		Queue.CopyStat(StatId,ParentId,InstanceId,ParentInstanceId,ThreadId,
			Value,CallsPerFrame);
	}
}

/**
 * Builds a packet from the passed in data. Queues that packet to be sent
 * on the sender thread. The packet will be sent to every client in the
 * clients list
 *
 * @param StatId the id of the stat that is being written out
 * @param GroupId the id of the group the stat belongs to
 * @param Value the value of the stat to write out
 */
void FStatNotifyProvider_UDP::WriteStat(DWORD StatId,DWORD GroupId,FLOAT Value)
{
	// Don't bother building a packet if there are no clients connected
	if (ClientList->GetConnectionCount() > 0)
	{
		FPerFrameStatData& Queue = SenderRunnable->GetPerFrameQueue();
		Queue.CopyStat(StatId,Value);
	}
}

/**
 * Builds a packet from the passed in data. Queues that packet to be sent
 * on the sender thread. The packet will be sent to every client in the
 * clients list
 *
 * @param StatId the id of the stat that is being written out
 * @param GroupId the id of the group the stat belongs to
 * @param Value the value of the stat to write out
 */
void FStatNotifyProvider_UDP::WriteStat(DWORD StatId,DWORD GroupId,DWORD Value)
{
	// Don't bother building a packet if there are no clients connected
	if (ClientList->GetConnectionCount() > 0)
	{
		FPerFrameStatData& Queue = SenderRunnable->GetPerFrameQueue();
		Queue.CopyStat(StatId,Value);
	}
}

/**
 * Tells the provider that we are starting to supply it with descriptions
 * for all of the stats/groups.
 */
void FStatNotifyProvider_UDP::StartDescriptions(void)
{
	FNboSerializeBuffer* Packet = new FNboSerializeBuffer();
	FLOAT SecondsPerCycle = GSecondsPerCycle;
	// Queue up the seconds per cycle value
	*Packet << 'P' << 'C' << SecondsPerCycle;
	// Append this to the queue
	SenderRunnable->AddDescriptionPacket(Packet);
}

/**
 * Adds a stat to the list of descriptions. Used to allow custom stats to
 * report who they are, parentage, etc. Prevents applications that consume
 * the stats data from having to change when stats information changes
 *
 * @param StatId the id of the stat
 * @param StatName the name of the stat
 * @param StatType the type of stat this is
 * @param GroupId the id of the group this stat belongs to
 */
void FStatNotifyProvider_UDP::AddStatDescription(DWORD StatId,
	const TCHAR* StatName,DWORD StatType,DWORD GroupId)
{
	FNboSerializeBuffer* Packet = new FNboSerializeBuffer();
	// Build the stat description packet
	*Packet << 'S' << 'D' << StatId << StatName << StatType << GroupId;
	// Append this to the queue
	SenderRunnable->AddDescriptionPacket(Packet);
}

/**
 * Adds a group to the list of descriptions
 *
 * @param GroupId the id of the group being added
 * @param GroupName the name of the group
 */
void FStatNotifyProvider_UDP::AddGroupDescription(DWORD GroupId,
	const TCHAR* GroupName)
{
	FNboSerializeBuffer* Packet = new FNboSerializeBuffer();
	// Build the group description packet
	*Packet << 'G' << 'D' << GroupId << GroupName;
	// Append this to the queue
	SenderRunnable->AddDescriptionPacket(Packet);
}

/**
 * Queues a packet for the new frame information
 *
 * @param FrameNumber the new frame number being processed
 */
void FStatNotifyProvider_UDP::SetFrameNumber(DWORD FrameNumber)
{
	// Don't bother building a packet if there are no clients connected
	if (ClientList->GetConnectionCount() > 0)
	{
		// Switch queues that are being read from/written into
		SenderRunnable->SwapQueues();
		FPerFrameStatData& Queue = SenderRunnable->GetPerFrameQueue();
		// Copy the frame number being captured
		Queue.FrameNumber = FrameNumber;
	}
}

#endif

#endif	//#if WITH_UE3_NETWORKING
