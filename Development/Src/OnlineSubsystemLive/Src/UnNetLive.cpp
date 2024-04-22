/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemLive.h"

#if WITH_UE3_NETWORKING && (XBOX || WITH_PANORAMA)

#if CONSOLE
	#if _DEBUG
		#pragma comment(lib,"xhvd2.lib")
	#else
		#pragma comment(lib,"xhv2.lib")
	#endif
#endif

#if STATS
/** Stats for tracking voice and other Xe specific data */
enum EXeNetStats
{
	STAT_PercentInOverhead = STAT_PercentOutVoice + 1,
	STAT_PercentOutOverhead,
};

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("In % Overhead"),STAT_PercentInOverhead,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Out % Overhead"),STAT_PercentOutOverhead,STATGROUP_Net);
#endif

#define XE_PACKET_OVERHEAD 51

IMPLEMENT_CLASS(UIpConnectionLive);
IMPLEMENT_CLASS(UIpNetDriverLive);

/**
 * Looks at the last error code to determine if the server is dead
 */
inline static UBOOL WasCriticalSendError(void)
{
	// Decide if we should shut down
	switch (WSAGetLastError())
	{
		case WSAETIMEDOUT:
		case WSAEHOSTUNREACH:
		case WSAENETUNREACH:
		case WSAECONNRESET:
		case WSAECONNABORTED:
			return TRUE;
	};
	return FALSE;
}


/** Registers the net classes */
void RegisterLiveNetClasses(void)
{
	// Don't register these classes when "make" is running because that causes them
	// to be exported which breaks the build when not compiling WITH_PANORAMA=1
	if (!GIsUCCMake)
	{
		UIpNetDriverLive::StaticClass();
		UIpConnectionLive::StaticClass();
	}
}

/**
 * Emits object references for GC
 */
void UIpConnectionLive::StaticConstructor(void)
{
	Super::StaticConstructor();
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference(STRUCT_OFFSET(UIpConnectionLive,XeNetDriver));
}

/**
 * Initializes a connection with the passed in settings
 *
 * @param InDriver the net driver associated with this connection
 * @param InSocket the socket associated with this connection
 * @param InRemoteAddr the remote address for this connection
 * @param InState the connection state to start with for this connection
 * @param InOpenedLocally whether the connection was a client/server
 * @param InURL the URL to init with
 * @param InMaxPacket the max packet size that will be used for sending
 * @param InPacketOverhead the packet overhead for this connection type
 */
void UIpConnectionLive::InitConnection(UNetDriver* InDriver,FSocket* InSocket,
	const FInternetIpAddr& InRemoteAddr,EConnectionState InState,
	UBOOL InOpenedLocally,const FURL& InURL,INT InMaxPacket,
	INT InPacketOverhead)
{
	// check if we need to disable all live functionality from this class
	bUseFallbackConnection = ParseParam(appCmdLine(), TEXT("NOLIVE"));

	if (bUseFallbackConnection)
	{
		// just to make sure VDP is disabled
		bUseVDP = FALSE;
	}
	else
	{
		// The code needs to be updated if this assumption changes
		checkSlow(sizeof(FUniqueNetId) == sizeof(XUID));
		// Cache the VDP setting
		bUseVDP = GSocketSubsystem->RequiresChatDataBeSeparate();
		// Cache the server setting
		bIsServer = InDriver->ServerConnection == NULL && !InDriver->bIsPeer;
		// Clear the error checking flag
		bHasSocketError = FALSE;
		// Cache a pointer to our net driver to be able to call ReplicateVoicePacket()
		XeNetDriver = Cast<UIpNetDriverLive>(InDriver);
		check(XeNetDriver && "Specialized Xe connection class must use UIpNetDriverLive");
	}

	// Now have the base class init
	Super::InitConnection(InDriver,InSocket,InRemoteAddr,InState,InOpenedLocally,InURL,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? XE_MAX_PACKET_SIZE : InMaxPacket,
		InPacketOverhead == 0 ? XE_PACKET_OVERHEAD : InPacketOverhead);
}

/**
 * Sends a byte stream to the remote endpoint using the underlying socket.
 * To minimize bandwidth being consumed by encryption overhead, the voice
 * data is appended to the gamedata before sending
 *
 * @param Data the byte stream to send
 * @param Count the length of the stream to send
 */
void UIpConnectionLive::LowLevelSend(void* Data,INT Count)
{
	if (bHasSocketError)
	{
		return;
	}
	// We only need to do special processing if we are merging voice/game packets
	if (bUseFallbackConnection == FALSE && bUseVDP == TRUE)
	{
		CLOCK_CYCLES(Driver->SendCycles);
		// Make sure the size to send will fit in our buffer
		check(XE_MAX_PACKET_SIZE >= Count);
		// Go ahead and set up the vdp portion of the packet (size + gamedata)
		BYTE* WriteAt = Buffer;
		WORD VdpCount = (WORD)(Count & 0xFFFF);
		// Write in game data size as a word
		*(WORD*)WriteAt = VdpCount;
		WriteAt += 2;
		VdpCount += 2;
		// Now copy the game data before appending voice data
		appMemcpy(WriteAt,Data,Count);
		WriteAt += Count;
		DWORD PacketSpace = XE_MAX_PACKET_SIZE - Count;
		UNCLOCK_CYCLES(Driver->SendCycles);
		// Check to see if the packet has space for voice to append
		if (PacketSpace > sizeof(FVoicePacket))
		{
			CLOCK_CYCLES(Driver->SendCycles);
			WORD BytesMerged = 0;
			// Merge the voice packets from the voice channel into the stream
			MergeVoicePackets(WriteAt,PacketSpace,BytesMerged);
			// Update the size of the packet for correct sending
			VdpCount += BytesMerged;
			INT BytesSent = 0;
			// Send the merged packet to the remote endpoint
			if (Socket->SendTo(Buffer,VdpCount,BytesSent,RemoteAddr) == FALSE)
			{
				// If the error is one we can't recover from
				if (WasCriticalSendError() == TRUE &&
					// Skip this error if the cable was pulled, since the notification will handle it
					XNetGetEthernetLinkStatus() & XNET_ETHERNET_LINK_ACTIVE)
				{
					bHasSocketError = TRUE;
					UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
					// If we are a client and the server is no longer reachable
					// then send an error notification to the first player
					// Don't fire this error if there is a pending level
					if (bIsServer == FALSE &&
						Driver->bIsPeer == FALSE &&
						GameEngine != NULL &&
						GWorld != NULL &&
						GameEngine->GPendingLevel == NULL &&
						!GIsGarbageCollecting)
					{
						debugfLiveSlow(NAME_Error,TEXT("Failed to send to host, notifying player"));
						GameEngine->SetProgress(PMT_SocketFailure, TEXT("Socket Error (change this title)"), TEXT("A socket error was received (todo: change this message to something more user-friendly"));
					}
					debugfLiveSlow(NAME_Error,TEXT("Closing connection due to unrecoverable error"));
					// Close this connection
					//@note: we intentionally do not use Close() here as that will attempt to send a close bunch which will obviously fail in this case
					State = USOCK_Closed;
				}
			}
			UNCLOCK_CYCLES(Driver->SendCycles);
		}
		else
		{
			// Send without voice being appended
			Super::LowLevelSend(Buffer,VdpCount);
		}
#if STATS
		// Update total bytes sent
		Driver->OutBytes += VdpCount - Count;
		XeNetDriver->VoiceBytesSent += VdpCount - Count;
#endif
	}
	else
	{
		// Send it the old way
		Super::LowLevelSend(Data,Count);
	}
}

/**
 * Attempts to pack all of the voice packets in the voice channel into the buffer.
 *
 * @param WriteAt the point in the buffer to write voice data
 * @param SpaceAvail the amount of space left in the merge buffer
 * @param OutBytesMerged the amount of data added to the buffer
 *
 * @return TRUE if the merging included all replicated packets, FALSE otherwise (not enough space)
 */
UBOOL UIpConnectionLive::MergeVoicePackets(BYTE* WriteAt,DWORD SpaceAvail,WORD& OutBytesMerged)
{
	// this should never get called when using the fallback case
	check(!bUseFallbackConnection);

	OutBytesMerged = 0;
	UVoiceChannel* VoiceChannel = GetVoiceChannel();
	if (VoiceChannel != NULL)
	{
		TArray<FVoicePacket*>& SendList = VoiceChannel->VoicePackets;
		// Try to append each packet in turn
		for (INT Index = 0; Index < SendList.Num(); Index++)
		{
			FVoicePacket* VoicePacket = SendList(Index);
			WORD TotalPacketSize = VoicePacket->GetTotalPacketSize();
			// Determine if there is enough space to add the voice packet
			if ((DWORD)OutBytesMerged + (DWORD)TotalPacketSize <= SpaceAvail)
			{
				VoicePacket->WriteToBuffer(WriteAt);
				// Advance through the buffer
				WriteAt += TotalPacketSize;
				// Update the total number of bytes appended
				OutBytesMerged += TotalPacketSize;
				// Remove this packet and decrement its ref count
				SendList.Remove(Index);
				Index--;
				VoicePacket->DecRef();
#if STATS
				// Increment the number of voice packets we've sent
				XeNetDriver->VoicePacketsSent++;
#endif
			}
			else
			{
				// Not enough space to process all packets
				return FALSE;
			}
		}
	}
	// Processed all packets
	return TRUE;
}

/**
 * Processes the byte stream for VDP merged packet support. Parses out all
 * of the voice packets and then forwards the game data to the base class
 * for handling
 *
 * @param Data the data to process
 * @param Count the size of the data buffer to process
 */
void UIpConnectionLive::ReceivedRawPacket(void* Data,INT Count)
{
	// We only need to do special processing if we are parsing merged
	// voice/game packets
	if (bUseFallbackConnection == FALSE && bUseVDP == TRUE)
	{
		BYTE* GameDataStart = (BYTE*)Data + 2;
		// VDP places the game data size in the first 2 bytes followed by voice data
		WORD GamePacketSize = *(WORD*)Data;
		// See if there was any voice data included
		if (GamePacketSize < Count - 2)
		{
			if (Actor && Actor->bHasVoiceHandshakeCompleted)
			{
				// Figure out the amount and locations of the voice data
				INT VoiceCount = Count - 2 - GamePacketSize;
				BYTE* VoiceData = GameDataStart + GamePacketSize;
				// While there is voice data to parse
				while (VoiceCount > 0)
				{
					// Construct a new voice packet with ref counting
					FVoicePacket* VoicePacket = new FVoicePacket(1);
					// Read the data
					WORD BytesRead = VoicePacket->ReadFromBuffer(VoiceData);
					// Move to the next packet and adjust the amount remaining
					VoiceData += BytesRead;
					VoiceCount -= BytesRead;
					// Now add the packet to the list to process
					GVoiceData.RemotePackets.AddItem(VoicePacket);
#if 0
					debugf(TEXT("ReceivedVoicePacket: %s [%s] PacketSize=%d BytesRead=%d to=0x%016I64X from=0x%016I64X"),
						*Driver->GetDescription(),
						*LowLevelDescribe(),
						Count,
						BytesRead,
						PlayerId.Uid,
						VoicePacket->Sender.Uid);
#endif
					// Send this packet to other clients as needed if we are a server
					if (bIsServer)
					{
						XeNetDriver->ReplicateVoicePacket(VoicePacket,this);
					}
#if STATS
					// Increment the number of voice packets we've received
					XeNetDriver->VoicePacketsRecv++;
#endif
				}
			}
			// It's possible to have voice only packets
			if (GamePacketSize > 0)
			{
				// Finally, let the game data get processed
				Super::ReceivedRawPacket(GameDataStart,GamePacketSize);
			}
		}
		// Make sure the amount indicated exactly matches VDP with no voice
		else if (GamePacketSize == Count - 2)
		{
			Super::ReceivedRawPacket(GameDataStart,GamePacketSize);
		}
		else
		{
			// This means that the game packet size is reported as larger than count
			// Badness, so just drop the packet
			debugf(NAME_Error,TEXT("Corrupt VDP packet received GamePacketSize (%u), Count (%d)"),
				(DWORD)GamePacketSize,Count);
		}
#if STATS
		// Update total bytes received
		Driver->InBytes += Count - GamePacketSize;
		XeNetDriver->VoiceBytesRecv += Count - GamePacketSize;
#endif
	}
	else
	{
		// Process it the old way
		Super::ReceivedRawPacket(Data,Count);
	}
}

/**
 * Queues any local voice packets for replication
 */
void UIpNetDriverLive::TickFlush(void)
{
#if STATS
	// Update network stats before the base class zeros stuff
	if (Time - StatUpdateTime > StatPeriod)
	{
		FLOAT RealTime = Time - StatUpdateTime;
		// Determine overhead percentages
		DWORD OutOverheadBytes = OutPackets * XE_PACKET_OVERHEAD;
		DWORD InOverheadBytes = InPackets * XE_PACKET_OVERHEAD;
		OutPercentOverhead = appTrunc(100.f * (FLOAT)OutOverheadBytes / (FLOAT)OutBytes);
		InPercentOverhead = appTrunc(100.f * (FLOAT)InOverheadBytes / (FLOAT)InBytes);
		SET_DWORD_STAT(STAT_PercentOutOverhead,OutPercentOverhead);
		SET_DWORD_STAT(STAT_PercentInOverhead,InPercentOverhead);
		InPercentOverhead = 0;
		OutPercentOverhead = 0;
	}
#endif
	// Let the base class tick connections
	Super::TickFlush();
}

#endif	// #if WITH_UE3_NETWORKING && (XBOX || WITH_PANORAMA)
