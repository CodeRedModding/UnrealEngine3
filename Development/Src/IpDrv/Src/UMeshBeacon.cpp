/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(UMeshBeacon);
IMPLEMENT_CLASS(UMeshBeaconClient);
IMPLEMENT_CLASS(UMeshBeaconHost);

#define DEBUG_PACKET_TRAFFIC 0

/**
 * Convert index from an enum type to it's equivalent string
 *
 * @param EnumIdx Index of enum value to convert
 * @param EnumTypeStr String name of the enum to lookup
 */
static FString EnumTypeToString(BYTE EnumIdx,const TCHAR* EnumTypeStr)
{
	FString Result(TEXT("None"));

	FString EnumObjPath = FString(TEXT("IpDrv.MeshBeacon.")) + FString(EnumTypeStr);
	UEnum* EnumType = FindObject<UEnum>(NULL, *EnumObjPath, TRUE);
	if (EnumType != NULL && EnumIdx < EnumType->NumEnums())
	{
		Result = EnumType->GetEnum(EnumIdx).ToString();
	}
	return Result;
}

/*-----------------------------------------------------------------------------
	UMeshBeacon implementation
-----------------------------------------------------------------------------*/

/**
 * Serialize from NBO buffer to FConnectionBandwidthStats
 */
FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& FromBuffer,FConnectionBandwidthStats& BandwidthStats)
{
	FromBuffer >> BandwidthStats.DownstreamRate
		>> BandwidthStats.RoundtripLatency
		>> BandwidthStats.UpstreamRate;

	return FromBuffer;
}

/**
 * Serialize from FConnectionBandwidthStats to NBO buffer
 */
FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& ToBuffer,const FConnectionBandwidthStats& BandwidthStats)
{
	ToBuffer << BandwidthStats.DownstreamRate
		<< BandwidthStats.RoundtripLatency
		<< BandwidthStats.UpstreamRate;

	return ToBuffer;
}

/**
 * Serialize from NBO buffer to FPlayerMember
 */
FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& FromBuffer,FPlayerMember& PlayerEntry)
{
	FromBuffer >> PlayerEntry.NetId
		>> PlayerEntry.Skill
		>> PlayerEntry.TeamNum;

	return FromBuffer;
}

/**
 * Serialize from FPlayerMember to NBO buffer
 */
FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& ToBuffer,const FPlayerMember& PlayerEntry)
{
	ToBuffer << PlayerEntry.NetId
		<< PlayerEntry.Skill
		<< PlayerEntry.TeamNum;

	return ToBuffer;
}

/**
 * Ticks the network layer to see if there are any requests or responses to requests
 *
 * @param DeltaTime the amount of time that has elapsed since the last tick
 */
void UMeshBeacon::Tick(FLOAT DeltaTime)
{
	// Only tick if we have a socket present
	if (Socket)
	{
		// See if we need to clean up
		if (bWantsDeferredDestroy)
		{
			eventDestroyBeacon();
		}
	}
}

/**
 * Stops listening for requests/responses and releases any allocated memory
 */
void UMeshBeacon::DestroyBeacon()
{
	if (Socket)
	{
		// Don't delete if this is during a tick or we'll crash
		if (bIsInTick == FALSE)
		{
			GSocketSubsystem->DestroySocket(Socket);
			Socket = NULL;
			// Clear the deletion flag so we don't incorrectly delete
			bWantsDeferredDestroy = FALSE;
			// Stop ticking the beacon since beacon no longer has a socket connection
			bShouldTick = FALSE;
			
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) destroy complete."),
				*BeaconName.ToString());
		}
		else
		{
			bWantsDeferredDestroy = TRUE;
			
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) deferring destroy until end of tick."),
				*BeaconName.ToString());
		}
	}
}

/**
 * Sends a heartbeat packet to the specified socket
 *
 * @param Socket the socket to send the data on
 *
 * @return TRUE if it sent ok, FALSE if there was an error
 */
UBOOL UMeshBeacon::SendHeartbeat(FSocket* Socket)
{
	UBOOL bResult = FALSE;
	if (Socket)
	{
		BYTE Heartbeat = MB_Packet_Heartbeat;
		INT BytesSent;
		// Send the message indicating the party that is cancelling
		UBOOL bDidSendOk = Socket->Send(&Heartbeat,1,BytesSent);
		if (bDidSendOk == FALSE)
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) failed to send heartbeat packet with (%s)."),
				*BeaconName.ToString(),
				GSocketSubsystem->GetSocketError());
		}
		bResult = bDidSendOk;
	}
	return bResult;
}

/**
 * Handles dummy packets that are received by reading from the buffer until there is no more data or a non-dummy packet is seen.
 *
 * @param FromBuffer the packet serializer to read from
 */
void UMeshBeacon::ProcessDummyPackets(FNboSerializeFromBuffer& FromBuffer)
{
	if (FromBuffer.AvailableToRead() > 0)
	{
		// Keep reading the buffer as long as we see dummy packets or until the end of the buffer
		BYTE DummyPacket = MB_Packet_DummyData;
		while (!FromBuffer.HasOverflow() && DummyPacket == MB_Packet_DummyData)
		{
			FromBuffer >> DummyPacket;
		}
		// If we didn't reach the end of the buffer then the last packet was a non-dummy
		// then go back one byte to allow the packet type to be processed.
		if (!FromBuffer.HasOverflow())
		{
			FromBuffer.Seek(FromBuffer.Tell()-1);
		}
	}
}

/*-----------------------------------------------------------------------------
	UMeshBeaconClient implementation
-----------------------------------------------------------------------------*/

/**
 * Creates a beacon that will send requests to remote hosts
 *
 * @param Addr the address that we are connecting to (needs to be resolved)
 *
 * @return true if the beacon was created successfully, false otherwise
 */
UBOOL UMeshBeaconClient::InitClientBeacon(const FInternetIpAddr& Addr)
{
	UBOOL bResult = FALSE;

	// Now create and set up our TCP socket
	Socket = GSocketSubsystem->CreateStreamSocket(TEXT("client mesh beacon"));
	if (Socket != NULL)
	{
		// Set basic state
		Socket->SetReuseAddr();
		Socket->SetNonBlocking();

		// Size of socket send buffer. Once this is filled then socket blocks on the next send.
		INT SizeSet=0;
		if (SocketSendBufferSize > 0)
		{
			Socket->SetSendBufferSize(SocketSendBufferSize,SizeSet);
		}		

		// Connect to the remote host so we can send the request
		if (Socket->Connect(Addr))
		{
			ClientBeaconState = MBCS_Connecting;
			bResult = TRUE;
		}
		else
		{
			INT Error = GSocketSubsystem->GetLastErrorCode();
			debugf(NAME_DevBeacon, 
				TEXT("Beacon (%s) failed to connect to %s with %s."),
				*BeaconName.ToString(),
				*Addr.ToString(TRUE),
				GSocketSubsystem->GetSocketError(Error));
		}
	}
	else
	{
		debugf(NAME_DevBeacon, 
				TEXT("Beacon (%s) failed to create socket to %s."),
				*BeaconName.ToString(),
				*Addr.ToString(TRUE));

		// Failed to create the connection
		ClientBeaconState = MBCS_ConnectionFailed;
	}
	return bResult;
}

/**
 * Ticks the network layer to see if there are any requests or responses to requests
 *
 * @param DeltaTime the amount of time that has elapsed since the last tick
 */
void UMeshBeaconClient::Tick(FLOAT DeltaTime)
{
	// Only tick if we have a socket present
	if (Socket && bShouldTick && bWantsDeferredDestroy == FALSE)
	{
		// Use this to guard against deleting the socket while in use
		bIsInTick = TRUE;
		switch (ClientBeaconState)
		{
			case MBCS_Connecting:
			{
				// Determine if the socket connection is still open
				CheckConnectionStatus();
				break;
			}
			case MBCS_Connected:
			{
				// Once connected to host then send history of previous bandwidth results
				SendClientConnectionRequest();
				break;
			}
			case MBCS_AwaitingResponse:
			{
				if (CurrentBandwidthTest.CurrentState == MB_BandwidthTestState_InProgress)
				{
					// Keep track of elapsed time since test was started
					CurrentBandwidthTest.ElapsedTestTime += DeltaTime;
					// Ignore host packets while a bandwidth test is in progress
					ProcessInProgressBandwidthTest();
				}
				else
				{
					// Increment the time since we've last seen a heartbeat
					ElapsedHeartbeatTime += DeltaTime;
					// Read and process any host packets
					ReadHostData();
					// Make sure we haven't processed a packet saying to travel
					if (bShouldTick && bWantsDeferredDestroy == FALSE)
					{
						// Check to see if we've lost the host
						if (ElapsedHeartbeatTime > HeartbeatTimeout ||
							ClientBeaconState == MBCS_ConnectionFailed)
						{
							ProcessHostTimeout();
						}
					}
				}
				break;
			}
		}

		// If < 0 then already received a host response, else waiting for a host response
		if (ConnectionRequestElapsedTime >= 0.f)
		{
			// Keep track of time elapsed since the client initiated the connection request
			ConnectionRequestElapsedTime += DeltaTime;
			// Check for a timeout or an error
			if (ConnectionRequestElapsedTime > ConnectionRequestTimeout ||
				ClientBeaconState == MBCS_ConnectionFailed)
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) timeout waiting for host to accept our connection."),
					*BeaconName.ToString());
				ProcessHostTimeout();
			}
		}

		// No longer a danger, so release the sentinel on the destruction
		bIsInTick = FALSE;
	}
	Super::Tick(DeltaTime);
}

/**
 * Stops listening for requests/responses and releases any allocated memory
 */
void UMeshBeaconClient::DestroyBeacon()
{
	if (bIsInTick == FALSE)
	{
		CleanupAddress();
	}
	// Clean up the socket now too
	Super::DestroyBeacon();
}

/** 
 * Unregisters the address and zeros the members involved to prevent multiple releases 
 */
void UMeshBeaconClient::CleanupAddress(void)
{
	// Release any memory associated with communicating with the host
	if( Resolver != NULL )
	{
		// Don't unregister if didn't have to register secure keys during connection
		if (bUsingRegisteredAddr && 
			!Resolver->UnregisterAddress(HostPendingRequest))
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) CleanupAddress() failed to unregister the party host address."),
				*BeaconName.ToString());
		}
	}

	// Zero since this was a shallow copy
	HostPendingRequest.GameSettings = NULL;
	HostPendingRequest.PlatformData = NULL;
	ClientBeaconState = MBCS_Closed;
}

/**
 * Handles checking for the transition from connecting to connected (socket established)
 */
void UMeshBeaconClient::CheckConnectionStatus(void)
{
	ESocketConnectionState State = Socket->GetConnectionState();
	if (State == SCS_NotConnected)
	{
		debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) waiting for connection to host (%s) ..."),
				*BeaconName.ToString(),
				*Socket->GetAddress().ToString(TRUE));
	}
	else if (State == SCS_Connected)
	{
		ClientBeaconState = MBCS_Connected;
	}
	else if (State == SCS_ConnectionError)
	{
		INT SocketErrorCode = GSocketSubsystem->GetLastErrorCode();
		// Continue to allow the session to be established for EWOULDBLOCK (not an error)
		if (SocketErrorCode != SE_EWOULDBLOCK)
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) error connecting to host (%s) with error (%s)."),
				*BeaconName.ToString(),
				*Socket->GetAddress().ToString(TRUE),
				GSocketSubsystem->GetSocketError());

			ClientBeaconState = MBCS_ConnectionFailed;
		}
	}
}

/**
 * Loads the class specified for the Resolver and constructs it if needed
 */
void UMeshBeaconClient::InitResolver(void)
{
	if (Resolver == NULL)
	{
		// Load the specified beacon address resolver class
		ResolverClass = LoadClass<UClientBeaconAddressResolver>(NULL,*ResolverClassName,NULL,LOAD_None,NULL);
		if (ResolverClass != NULL)
		{
			// create a new instance once
			Resolver = ConstructObject<UClientBeaconAddressResolver>(ResolverClass,this);
			if (Resolver != NULL)
			{
				// keep track of the requesting beacon's name
				Resolver->BeaconName = BeaconName;
				// keep track of the requesting beacon's port
				Resolver->BeaconPort = MeshBeaconPort;
			}
			else
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) failed to create Resolver."),
					*BeaconName.ToString());
			}
		}
		else
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) failed to load ResolverClassName class %s."),
				*BeaconName.ToString(),
				*ResolverClassName);			
		}
	}
}

/**
 * Request a connection to be established to the remote host. As part of the 
 * connection request also send the NAT type and bandwidth history data for the client.
 * Note this request is async and the results will be sent via the delegate
 *
 * @param DesiredHost the server that the connection will be made to
 * @param ClientRequest the client data that is going to be sendt with the request
 * @param bRegisterSecureAddress if TRUE then then key exchange is required to connect with the host
 *
 * @return true if the request async task started ok, false if it failed to send
 */
UBOOL UMeshBeaconClient::RequestConnection(const struct FOnlineGameSearchResult& DesiredHost,const struct FClientConnectionRequest& ClientRequest,UBOOL bRegisterSecureAddress)
{
	UBOOL bWasStarted = FALSE;
	bUsingRegisteredAddr = FALSE;

	// Make sure the resolver has been initialized
	InitResolver();
	if (Resolver != NULL)
	{
		// Register the secure keys so we can decrypt and communicate or 
		// assume the key exchange has already been done and is not needed
		if (!bRegisterSecureAddress ||
			Resolver->RegisterAddress(DesiredHost))
		{
			// Keep track of whether or not secure key registration was done for the host session
			bUsingRegisteredAddr = bRegisterSecureAddress;

			FInternetIpAddr SendTo;
			// Make sure we can resolve where we are sending to
			if (Resolver->ResolveAddress(DesiredHost,SendTo))
			{
				HostPendingRequest = DesiredHost;
				if (InitClientBeacon(SendTo))
				{
					// At this point we have a socket to the server and we'll check in tick when the
					// connection is fully established
					bWasStarted = TRUE;
					// Reset the request timeout
					ConnectionRequestElapsedTime = 0.f;

					// Keep track of all the info from the current connection request for the player
					// Once a socket connection is established this data is sent to the host
					ClientPendingRequest = ClientRequest;
					ClientPendingRequest.BandwidthHistory = ClientRequest.BandwidthHistory;

					// Clear any stale bandwidth test data since this is a new connection
					appMemzero(&CurrentBandwidthTest,sizeof(CurrentBandwidthTest));

					debugf(NAME_DevBeacon,
						TEXT("Beacon (%s) RequestConnection() succeeded. bRegisterSecureAddress=%d"),
						*BeaconName.ToString(),
						bRegisterSecureAddress);
				}
				else
				{
					debugf(NAME_DevBeacon,
						TEXT("Beacon (%s) RequestConnection() failed to init client beacon."),
						*BeaconName.ToString());
				}
			}
			else
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) RequestConnection() failed to resolve host address"),
					*BeaconName.ToString());
			}
		}
		else
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) RequestConnection() failed to register the host address"),
				*BeaconName.ToString());
		}
	}
	// Fire the delegate so the code can process the next items
	if (bWasStarted == FALSE)
	{
		DestroyBeacon();
	}
	return bWasStarted;
}

/**
 * Sends all the data for a new client connection on the host.
 * Client data includes the player net id, cient NAT type, and previous bandwidth history.
 * Assumes that a connection has successfully been established with the host.
 */
void UMeshBeaconClient::SendClientConnectionRequest(void)
{
	// Create a buffer and serialize the request in
	FNboSerializeToBuffer ToBuffer(512);
	// Packet format is <MB_Packet_ClientNewConnectionRequest><PlayerNetId><NATType><GoodHostRatio><bCanHostVs><MinutesSinceLastTest><BanwidthHistory(NumEntries)(Entry[0]..Entry[NumEntries-1])>
	ToBuffer << (BYTE)MB_Packet_ClientNewConnectionRequest
			 << ClientPendingRequest.PlayerNetId
			 << ClientPendingRequest.NatType
			 << ClientPendingRequest.GoodHostRatio
			 << (BYTE)ClientPendingRequest.bCanHostVs
			 << ClientPendingRequest.MinutesSinceLastTest;
	INT NumBandwidthEntries = ClientPendingRequest.BandwidthHistory.Num();
	ToBuffer << NumBandwidthEntries;
	for (INT EntryIdx=0; EntryIdx < ClientPendingRequest.BandwidthHistory.Num(); EntryIdx++)
	{
		const FConnectionBandwidthStats& BandwidthEntry = ClientPendingRequest.BandwidthHistory(EntryIdx);
		ToBuffer	<< BandwidthEntry;
	}

	INT BytesSent;
	// Now send to the destination host
	if (Socket->Send(ToBuffer.GetRawBuffer(0),ToBuffer.GetByteCount(),BytesSent))
	{
		// After the initial connection then just wait for host response packets
		ClientBeaconState = MBCS_AwaitingResponse;
		// reset timer to wait for a response to the connection request
		ConnectionRequestElapsedTime = 0.f;
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) sent connection request with history to (%s)."),
			*BeaconName.ToString(),
			*Socket->GetAddress().ToString(TRUE));
	}
	else
	{
		// Failed to send, so mark the request as failed
		ClientBeaconState = MBCS_ConnectionFailed;
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) failed to send the connection request with history to host (%s) with error code (%s)."),
			*BeaconName.ToString(),
			*Socket->GetAddress().ToString(TRUE),
			GSocketSubsystem->GetSocketError());
	}
}

/**
 * Checks the socket for a response packet from the host and processes if present
 */
void UMeshBeaconClient::ReadHostData(void)
{
	UBOOL bShouldRead = TRUE;
	BYTE PacketData[2048];

	// Read each pending packet and pass it out for processing
	while (bShouldRead && bShouldTick && bWantsDeferredDestroy == FALSE)
	{
		INT BytesRead = 0;
		// Read the response from the socket
		if (Socket->Recv(PacketData,2048,BytesRead))
		{
			if (BytesRead > 0)
			{
				// Route the host packet for processing
				ProcessHostPacket(PacketData,BytesRead);
			}
			else
			{
				bShouldRead = FALSE;
			}
		}
		else
		{
			// Check for an error other than would block, which isn't really an error
			INT ErrorCode = GSocketSubsystem->GetLastErrorCode();
			if (ErrorCode != SE_EWOULDBLOCK)
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) socket error (%s) detected trying to read host packet from (%s)."),
					*BeaconName.ToString(),
					GSocketSubsystem->GetSocketError(ErrorCode),
					*Socket->GetAddress().ToString(TRUE));
				ClientBeaconState = MBCS_ConnectionFailed;
			}
			bShouldRead = FALSE;
		}
	}
}

/**
 * Processes a packet that was received from the host 
 *
 * @param Packet the packet that the host sent
 * @param PacketSize the size of the packet to process
 */
void UMeshBeaconClient::ProcessHostPacket(BYTE* Packet,INT PacketSize)
{
	// Use our packet serializer to read from the raw buffer
	FNboSerializeFromBuffer FromBuffer(Packet,PacketSize);
	// Work through the stream until we've consumed it all
	do
	{
		BYTE PacketType = MB_Packet_UnknownType;
		FromBuffer >> PacketType;
		// Don't process a packet if we are at the end
		if (FromBuffer.HasOverflow() == FALSE)
		{
#if DEBUG_PACKET_TRAFFIC
			debugf(NAME_DevBeacon,
				TEXT("(UMeshBeaconClient.ProcessHostPacket): PacketSize=%d HostPacketType=%s"),
				PacketSize,
				*EnumTypeToString(PacketType,TEXT("EMeshBeaconPacketType")));
#endif

			// Route the host response based on packet type
			HandleHostPacketByType(PacketType,FromBuffer);
		}
	}
	while (FromBuffer.HasOverflow() == FALSE);
}

/**
 * Routes the response packet received from a host to the correct handler based on its type.
 *
 * @param HostPacketType packet ID from EMeshBeaconPacketType that represents a host response to this client
 * @param FromBuffer the packet serializer to read from
 * @return TRUE if the data packet type was processed
 */
UBOOL UMeshBeaconClient::HandleHostPacketByType(BYTE HostPacketType,FNboSerializeFromBuffer& FromBuffer)
{
	// Route the call to the proper handler
	switch (HostPacketType)
	{
		case MB_Packet_HostNewConnectionResponse:
		{
			ProcessHostResponseConnectionRequest(FromBuffer);
			break;
		}
		case MB_Packet_HostBandwidthTestRequest:
		{
			ProcessHostRequestBandwidthTest(FromBuffer);
			break;
		}
		case MB_Packet_HostCompletedBandwidthTest:
		{
			ProcessHostFinishedBandwidthTest(FromBuffer);
			break;
		}
		case MB_Packet_HostTravelRequest:
		{
			ProcessHostTravelRequest(FromBuffer);
			break;
		}
		case MB_Packet_HostCreateNewSessionRequest:
		{
			ProcessHostCreateNewSessionRequest(FromBuffer);
			break;
		}
		case MB_Packet_DummyData:
		{
			ProcessDummyPackets(FromBuffer);
			break;
		}
		case MB_Packet_Heartbeat:
		{
			// Respond to this so they know we are alive
			ProcessHeartbeat();
			break;
		}
		default:
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) unknown packet type received from host (%d)."),
				*BeaconName.ToString(),
				(DWORD)HostPacketType);

			return FALSE;
		}
	}
	return TRUE;
}

/** 
 * Common routine for notifying of a timeout trying to talk to host 
 */
void UMeshBeaconClient::ProcessHostTimeout(void)
{
	// Unregister secure key for address
	CleanupAddress();	
	// Trigger connection delegate on timeout as well
	delegateOnConnectionRequestResult(MB_ConnectionResult_Timeout);
}

/**
 * Processes a heartbeat update, sends a heartbeat back, and clears the timer
 */
void UMeshBeaconClient::ProcessHeartbeat(void)
{
	ElapsedHeartbeatTime = 0.f;
	if (Socket != NULL)
	{
		// Notify the host that we received their heartbeat
		SendHeartbeat(Socket);
	}
}

/**
 * Reads the host response to the client's connection request. 
 * Triggers a delegate.
 */
void UMeshBeaconClient::ProcessHostResponseConnectionRequest(FNboSerializeFromBuffer& FromBuffer)
{
	BYTE ConnectionResult = MB_ConnectionResult_Error;
	FromBuffer >> ConnectionResult;
	// flag timer to < 0 indicated host response was received
	ConnectionRequestElapsedTime = -1;
	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) host (%s) connection request response was (%s)."),
		*BeaconName.ToString(),
		*Socket->GetAddress().ToString(TRUE),
		*EnumTypeToString(ConnectionResult,TEXT("EMeshBeaconConnectionResult")));
	// Trigger delegate to handle the notification
	delegateOnConnectionRequestResult(ConnectionResult);
}

/**
 * Handles a new bandwidth test request initiated by the host for this client. 
 * Triggers a delegate.
 */
void UMeshBeaconClient::ProcessHostRequestBandwidthTest(FNboSerializeFromBuffer& FromBuffer)
{
	// Packet format is <MB_Packet_HostBandwidthTestRequest><TestType><TestBufferSize>
	BYTE TestType = MB_BandwidthTestType_Upstream;
	INT TestBufferSize = 0;	
	FromBuffer	>> TestType
				>> TestBufferSize;

	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) host (%s) requesting (%s) bandwidth test with buffer size (%.3f KB)."),
		*BeaconName.ToString(),
		*Socket->GetAddress().ToString(TRUE),
		*EnumTypeToString(TestType,TEXT("EMeshBeaconBandwidthTestType")),
		TestBufferSize/1024.f);

	// Trigger delegate indicating the request was received
	delegateOnReceivedBandwidthTestRequest(TestType);
	// Start processing the test
	BeginBandwidthTest(TestType,TestBufferSize);	
}

/**
 * Handles a host response that all upstream bandwidth data was received by the host.
 * Triggers a delegate.
 */
void UMeshBeaconClient::ProcessHostFinishedBandwidthTest(FNboSerializeFromBuffer& FromBuffer)
{
	BYTE TestResult = 0;
	BYTE TestType = 0;	
	INT BytesReceived = 0;
	FConnectionBandwidthStats BandwidthStats;
	appMemzero(&BandwidthStats,sizeof(BandwidthStats));
	// Packet format is <MB_Packet_HostCompletedBandwidthTest><TestType><TestResult><BytesReceived><UpstreamRate><DownstreamRate><RoundtripLatency>
	FromBuffer >> TestResult
			>> TestType
			>> BytesReceived
			>> BandwidthStats;

	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) host (%s) finished processing (%s) bandwidth test. Result (%s) total bytes processed (%.3f KB)."),
		*BeaconName.ToString(),
		*Socket->GetAddress().ToString(TRUE),
		*EnumTypeToString(TestType,TEXT("EMeshBeaconBandwidthTestType")),
		*EnumTypeToString(TestResult,TEXT("EMeshBeaconBandwidthTestResult")),
		BytesReceived/1024.f);

	// Add the new bandwidth test entry to the front of the list
	ClientPendingRequest.BandwidthHistory.InsertItem(BandwidthStats,0);
	// Only keep up to MaxBandwidthHistoryEntries in the history
	if (ClientPendingRequest.BandwidthHistory.Num() > MaxBandwidthHistoryEntries)
	{
		ClientPendingRequest.BandwidthHistory.Remove(MaxBandwidthHistoryEntries,ClientPendingRequest.BandwidthHistory.Num()-MaxBandwidthHistoryEntries);
	}

	// Notify game of results from test
	delegateOnReceivedBandwidthTestResults(TestType,TestResult,BandwidthStats);
}

/**
 * Have this client start a bandwidth test on the connected host by sending a start packet 
 * and then streaming as many dummy packets as possible before timeout (MaxBandwidthTestSendTime).
 *
 * @param TestType current test to run based on enum of EMeshBeaconBandwidthTestType supported bandwidth test types
 * @param TestBufferSize size in bytes of total data to be sent for the bandwidth test
 * @return true if the test was successfully started
 */
UBOOL UMeshBeaconClient::BeginBandwidthTest(BYTE TestType,INT TestBufferSize)
{
	UBOOL bResult= FALSE;

	//@todo sz - only upstream bandwidth testing is currently implemented
	if (TestType != MB_BandwidthTestType_Upstream)
	{
		return bResult;
	}

	// Keep track of the current bandwidth test started for this client
	CurrentBandwidthTest.TestType = TestType;
	// Keep track of elapsed time since the test was started
	CurrentBandwidthTest.ElapsedTestTime = 0;

	// Clamp requested test buffer size to the [min,max] allowed sizes
	const INT NumBytesToSend = Clamp<INT>(TestBufferSize, MinBandwidthTestBufferSize, MaxBandwidthTestBufferSize);

	if (NumBytesToSend > 0)
	{
		if (CurrentBandwidthTest.CurrentState != MB_BandwidthTestState_InProgress)
		{
			// Begin test by sending the start packet to the host
			FNboSerializeToBuffer ToBuffer(512);
			// Packet format is <MB_Packet_ClientBeginBandwidthTest><TestType><TestByteSize>
			ToBuffer	<< (BYTE)MB_Packet_ClientBeginBandwidthTest
						<< TestType
						<< NumBytesToSend;
			INT BytesSent = 0;
			UBOOL bDidSendOk = Socket->Send(ToBuffer.GetRawBuffer(0),ToBuffer.GetByteCount(),BytesSent);

			// Clear to start the new test
			appMemzero(&CurrentBandwidthTest,sizeof(CurrentBandwidthTest));
			bResult = bDidSendOk;

			if (bDidSendOk)
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) starting upstream bandwidth test to (%s) BytesSent=%d. Total BufferSize=%.3f KB"),
					*BeaconName.ToString(),
					*Socket->GetAddress().ToString(TRUE),
					BytesSent,
					NumBytesToSend/1024.f);

				// Mark test as started so that it will be continued until all data has been uploaded
				CurrentBandwidthTest.CurrentState = MB_BandwidthTestState_InProgress;
				// Keep track of how much total data will need to be sent
				CurrentBandwidthTest.NumBytesToSendTotal = NumBytesToSend;
			}
			else
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) failed upstream bandwidth test to (%s) BytesSent=%d (%s)"),
					*BeaconName.ToString(),
					*Socket->GetAddress().ToString(TRUE),
					BytesSent,
					GSocketSubsystem->GetSocketError());

				// Mark test as failed due to socket error
				CurrentBandwidthTest.CurrentState = MB_BandwidthTestState_Error;
			}
		}
		else
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) upstream bandwidth test to (%s) failed. Upstream test is already in progress."),
				*BeaconName.ToString(),
				*Socket->GetAddress().ToString(TRUE));
		}
	}
	else
	{
		debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) upstream bandwidth test to (%s) failed. Buffer size is invalid TestBufferSize=%d."),
				*BeaconName.ToString(),
				*Socket->GetAddress().ToString(TRUE),
				TestBufferSize);
	}
		
	return bResult;
}

/**
 * Update a bandwidth test that is currently in progress for this client.
 * All other host packets are ignored until the current test finishes or timeout occurs.
 */
void UMeshBeaconClient::ProcessInProgressBandwidthTest(void)
{
	check(CurrentBandwidthTest.CurrentState == MB_BandwidthTestState_InProgress);

	if (CurrentBandwidthTest.ElapsedTestTime >= MaxBandwidthTestSendTime)
	{
		debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) upstream bandwidth test to (%s) timed out sending data. CurrentBandwidthTest.ElapsedTestTime=%0.3f."),
				*BeaconName.ToString(),
				*Socket->GetAddress().ToString(TRUE),
				CurrentBandwidthTest.ElapsedTestTime);

		// If the total requested buffer size for the test couldn't be sent before exceeding the timeout then mark test as incomplete
		CurrentBandwidthTest.CurrentState = MB_BandwidthTestState_Incomplete;
	}
	else
	{
		// Max buffer size to send
		const INT MAX_SEND_SIZE = 8 * 1024;
		BYTE DummyBuffer[MAX_SEND_SIZE];
		appMemset(DummyBuffer,(BYTE)MB_Packet_DummyData,MAX_SEND_SIZE);

		// Keep track of how much is still left to send. Continue processing until this is 0 or we exceed the timeout.
		INT NumBytesToSend = CurrentBandwidthTest.NumBytesToSendTotal - CurrentBandwidthTest.NumBytesSentTotal;
		if (NumBytesToSend > 0)
		{
			// Keep sending data until the send buffer is full and socket errors with SE_EWOULDBLOCK
			INT BytesSent=0;
			UBOOL bDidSendOk = Socket->Send(DummyBuffer,Min<INT>(NumBytesToSend,MAX_SEND_SIZE),BytesSent);
			if (bDidSendOk)
			{
				// Total sent successfully so far
				CurrentBandwidthTest.NumBytesSentTotal += BytesSent;
				// Size of the last successful socket send result
				CurrentBandwidthTest.NumBytesSentLast = BytesSent;
				// Update what is left to send				
				NumBytesToSend = CurrentBandwidthTest.NumBytesToSendTotal - CurrentBandwidthTest.NumBytesSentTotal;

#if DEBUG_PACKET_TRAFFIC
 				debugf(NAME_DevBeacon,
					TEXT("(UMeshBeaconClient.ProcessInProgressBandwidthTest):  NumBytesToSend=%d NumBytesSentTotal=%d BytesSent=%d"),
 					NumBytesToSend,
					CurrentBandwidthTest.NumBytesSentTotal,
					BytesSent);
#endif

				if (NumBytesToSend <= 0)
				{
					debugf(NAME_DevBeacon,
						TEXT("Beacon (%s) completed sending data for bandwith test to (%s)."),
						*BeaconName.ToString(),
						*Socket->GetAddress().ToString(TRUE));

					CurrentBandwidthTest.CurrentState = MB_BandwidthTestState_Completed;
				}
			}
			else
			{
				// Handle socket error except for SE_EWOULDBLOCK since that just means the send buffer is full
				INT ErrorCode = GSocketSubsystem->GetLastErrorCode();
				if (ErrorCode != SE_EWOULDBLOCK)
				{
					CurrentBandwidthTest.CurrentState = MB_BandwidthTestState_Error;

					debugf(NAME_DevBeacon,
						TEXT("Beacon (%s) socket error (%s) detected trying to send packet to (%s)."),
						*BeaconName.ToString(),
						GSocketSubsystem->GetSocketError(ErrorCode),
						*Socket->GetAddress().ToString(TRUE));
				}
			}
		}
	}
}

/**
 * Processes a travel request packet that was received from the host
 *
 * @param FromBuffer the packet serializer to read from
 */
void UMeshBeaconClient::ProcessHostTravelRequest(FNboSerializeFromBuffer& FromBuffer)
{
	// Packet format is <MB_Packet_HostTravelRequest><SessionNameLen><SessionName><ClassNameLen><ClassName><SecureSessionInfo>
	FString SessionNameStr;
	FString ClassNameStr;
	BYTE DestinationInfo[80];
	// Read the two strings first
	FromBuffer	>> SessionNameStr 
				>> ClassNameStr;
	// Now copy the buffer data in
	FromBuffer.ReadBinary(DestinationInfo,80);
	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) host (%s) sent travel request for (%s),(%s)"),
		*BeaconName.ToString(),
		*Socket->GetAddress().ToString(TRUE),
		*SessionNameStr,
		*ClassNameStr);
	FName SessionName(*SessionNameStr,0);
	UClass* SearchClass = FindObject<UClass>(NULL,*ClassNameStr);
	// Fire the delegate so the client can process
	delegateOnTravelRequestReceived(SessionName,SearchClass,DestinationInfo);

	// Stop ticking the beacon to avoid processing new requests
	bShouldTick = FALSE;
	CleanupAddress();
}

/**
 * Processes a request packet that was received from the host to create a new game session
 *
 * @param FromBuffer the packet serializer to read from
 */
void UMeshBeaconClient::ProcessHostCreateNewSessionRequest(FNboSerializeFromBuffer& FromBuffer)
{
	// Packet format is <MB_Packet_HostCreateNewSessionRequest><SessionNameLen><SessionName><ClassNameLen><ClassName><PlayersLen><Players>
	FString SessionNameStr;
	FString ClassNameStr;
	// Read the two strings first
	FromBuffer	>> SessionNameStr 
				>> ClassNameStr;
	// Read the list of player net ids
	INT NumPlayers = 0;
	FromBuffer	>> NumPlayers;
	TArray<FPlayerMember> Players;
	for (INT PlayerIdx=0; PlayerIdx < NumPlayers; PlayerIdx++)
	{
		FPlayerMember PlayerMember;
		appMemzero(&PlayerMember,sizeof(PlayerMember));
		FromBuffer	>> PlayerMember;
		if (PlayerMember.NetId.HasValue())
		{
			Players.AddItem(PlayerMember);
		}
	}	
	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) host (%s) sent new session create request for (%s),(%s), NumPlayers=%d"),
		*BeaconName.ToString(),
		*Socket->GetAddress().ToString(TRUE),
		*SessionNameStr,
		*ClassNameStr,
		NumPlayers);
	for (INT PlayerIdx=0; PlayerIdx < Players.Num(); PlayerIdx++)
	{
		const FPlayerMember& PlayerMember = Players(PlayerIdx);
		debugf(NAME_DevBeacon,
			TEXT("	Beacon (%s) adding player for new session: NetId=0x%016I64X TeamNum=%d Skill=%d."),
			*BeaconName.ToString(),
			(QWORD&)PlayerMember.NetId,
			PlayerMember.TeamNum,
			PlayerMember.Skill);
	}
	FName SessionName(*SessionNameStr,0);
	UClass* SearchClass = FindObject<UClass>(NULL,*ClassNameStr);
	// Trigger delegate to allow game code to handle session creation
	delegateOnCreateNewSessionRequestReceived(SessionName,SearchClass,Players);
}

/**
 * Notify host of a newly created game session by this client. Host can decide to use/discard the new game session.
 *
 * @param bSuccess TRUE if the session was created successfully
 * @param SessionName the name of the session that was created
 * @param SearchClass the search that should be populated with the session
 * @param PlatformSpecificInfo the binary data to place in the platform specific areas
 */
UBOOL UMeshBeaconClient::SendHostNewGameSessionResponse(UBOOL bSuccess,FName SessionName,class UClass* SearchClass,const BYTE* PlatformSpecificInfo)
{
	const FString& SessionNameStr = SessionName.ToString();
	const FString& ClassNameStr = SearchClass->GetPathName();
	// Build the packet that we are sending
	FNboSerializeToBuffer ToBuffer(512);
	// Packet format is <MB_Packet_ClientCreateNewSessionResponse><bSuccess><SessionNameLen><SessionName><ClassNameLen><ClassName><SecureSessionInfo>
	ToBuffer << (BYTE)MB_Packet_ClientCreateNewSessionResponse
		<< (BYTE)bSuccess
		<< SessionNameStr
		<< ClassNameStr;
	// Copy the buffer over in raw form (it is already in NBO)
	ToBuffer.WriteBinary(PlatformSpecificInfo,80);
	// Send to the host
	INT BytesSent;
	UBOOL bDidSendOk = Socket->Send(ToBuffer.GetRawBuffer(0),ToBuffer.GetByteCount(),BytesSent);
	if (bDidSendOk == FALSE)
	{
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) to host (%s) failed to send new game session creation response with (%s)"),
			*BeaconName.ToString(),
			*Socket->GetAddress().ToString(TRUE),
			GSocketSubsystem->GetSocketError());
	}
	else
	{
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) sent to host (%s) new game session creation result response. (%s),(%s)"),
			*BeaconName.ToString(),
			*Socket->GetAddress().ToString(TRUE),
			*SessionNameStr,
			*ClassNameStr);
	}
	return bDidSendOk;
}

/*-----------------------------------------------------------------------------
	UMeshBeaconHost implementation
-----------------------------------------------------------------------------*/

/**
 * Creates a listening host mesh beacon to accept new client connections.
 *
 * @param OwningPlayerId Net Id of player that is hosting this beacon
 * @return true if the beacon was created successfully, false otherwise
 */
UBOOL UMeshBeaconHost::InitHostBeacon(struct FUniqueNetId InOwningPlayerId)
{
	UBOOL bResult = FALSE;

	FInternetIpAddr ListenAddr;
	// Keep track of the hosting Net Id
	OwningPlayerId = InOwningPlayerId;
	// Now the listen address
	ListenAddr.SetPort(MeshBeaconPort);
	ListenAddr.SetIp(getlocalbindaddr(*GWarn));
	// Now create and set up our TCP socket
	Socket = GSocketSubsystem->CreateStreamSocket(TEXT("host mesh beacon"));
	if (Socket != NULL)
	{
		// Set basic state
		Socket->SetReuseAddr();
		Socket->SetNonBlocking();

		// Size of socket recv buffer. Once this is filled then socket blocks on the next recv.
		INT SizeSet=0;
		if (SocketReceiveBufferSize > 0)
		{
			Socket->SetReceiveBufferSize(SocketReceiveBufferSize,SizeSet);
		}

		// Bind to our listen port so we can respond to client connections
		if (Socket->Bind(ListenAddr))
		{
			// Start listening for client connections
			if (Socket->Listen(ConnectionBacklog))
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) created on port (%d)."),
					*BeaconName.ToString(),
					MeshBeaconPort);
				bResult = TRUE;
			}
			else
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) failed to Listen(%d) on the socket for clients."),
					*BeaconName.ToString(),
					ConnectionBacklog);
			}
		}
		else
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) failed to bind listen socket to addr (%s)."),
				*BeaconName.ToString(),
				*ListenAddr.ToString(TRUE));
		}
	}
	else
	{
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) failed to create listen socket."),
			*BeaconName.ToString());
	}
	return bResult;
}

/**
 * Stops listening for clients and releases any allocated memory
 */
void UMeshBeaconHost::DestroyBeacon()
{
	if (Socket)
	{
		// Don't delete if this is during a tick or we'll crash
		if (bIsInTick == FALSE)
		{
			// Destroy each socket in the list and then empty it
			for (INT Index = 0; Index < ClientConnections.Num(); Index++)
			{
				GSocketSubsystem->DestroySocket(ClientConnections(Index).Socket);
			}
			ClientConnections.Empty();
		}
	}
	Super::DestroyBeacon();
}


/**
 * Ticks the network layer to see if there are any requests or responses to requests
 *
 * @param DeltaTime the amount of time that has elapsed since the last tick
 */
void UMeshBeaconHost::Tick(FLOAT DeltaTime)
{
	// Only tick if we have a socket present
	if (Socket && bShouldTick && bWantsDeferredDestroy == FALSE)
	{
		// Use this to guard against deleting the socket while in use
		bIsInTick = TRUE;
		// Add any new connections since the last tick
		AcceptConnections();
		// Keep track of the index for the client with an active bandwidth test in progress/started
		INT BandwidthTestClientIdx = INDEX_NONE;
		for (INT ClientIdx=0; ClientIdx < ClientConnections.Num(); ClientIdx++)
		{
			const FClientMeshBeaconConnection& ClientConn = ClientConnections(ClientIdx);
			if (ClientConn.BandwidthTest.CurrentState == MB_BandwidthTestState_InProgress ||
				ClientConn.BandwidthTest.CurrentState == MB_BandwidthTestState_StartPending)
			{
				BandwidthTestClientIdx = ClientIdx;
				break;
			}
		}
		// Skip ticking if no clients are connected
		if (ClientConnections.Num())
		{
			// Determine if it's time to send a heartbeat
			ElapsedHeartbeatTime += DeltaTime;
			UBOOL bNeedsHeartbeat = (ElapsedHeartbeatTime > (HeartbeatTimeout * 0.5f));

			// Check each client in the list for a packet
			for (INT Index = 0; Index < ClientConnections.Num(); Index++)
			{
				UBOOL bHadError = FALSE;
				// Grab the connection we are checking for data on
				FClientMeshBeaconConnection& ClientConn = ClientConnections(Index);
				// Mark how long since we've gotten something from the client
				ClientConn.ElapsedHeartbeatTime += DeltaTime;
				// Read the client data processing any packets
				if (ReadClientData(ClientConn))
				{
					// Send this client the heartbeat request
					if (bNeedsHeartbeat)
					{
						SendHeartbeat(ClientConn.Socket);
						ElapsedHeartbeatTime = 0.f;
					}
					// Only one bandwidth test at a time is allowed to be serviced by the host
					// If client has a pending bandwidth request and there are no active bandwidth tests running for any other client
					if (bAllowBandwidthTesting &&
						BandwidthTestClientIdx == INDEX_NONE &&
						ClientConn.BandwidthTest.CurrentState == MB_BandwidthTestState_RequestPending)
					{
						// Then send a request to start a new test for the client
						SendBandwidthTestStartRequest(
							(EMeshBeaconBandwidthTestType)ClientConn.BandwidthTest.TestType,
							ClientConn.BandwidthTest.BytesTotalNeeded,
							ClientConn);
						BandwidthTestClientIdx = Index;
					}
					// Check for a client going beyond our heartbeat timeout
					if (ClientConn.ElapsedHeartbeatTime > HeartbeatTimeout)
					{
						debugf(NAME_DevBeacon,
							TEXT("Beacon (%s) client (PlayerNetId=0x%016I64X) timed out."),
							*BeaconName.ToString(),
							(QWORD&)ClientConn.PlayerNetId);
						bHadError = bShouldTick && bWantsDeferredDestroy == FALSE;
					}
				}
				else
				{
					debugf(NAME_DevBeacon,
						TEXT("Beacon (%s) reading from client (PlayerNetId=0x%016I64X) failed."),
						*BeaconName.ToString(),
						(QWORD&)ClientConn.PlayerNetId);
					bHadError = bShouldTick && bWantsDeferredDestroy == FALSE;
				}
				if (bHadError)
				{
					// Zero the party leader, so his socket timeout doesn't cause another cancel
					(QWORD&)ClientConn.PlayerNetId = 0;
					// Now clean up that socket
					GSocketSubsystem->DestroySocket(ClientConn.Socket);
					ClientConnections.Remove(Index);
					Index--;
				}
			}
		}
		// No longer a danger, so release the sentinel on the destruction
		bIsInTick = FALSE;
	}
	Super::Tick(DeltaTime);
}

/** 
 * Accepts any pending connections and adds them to our queue 
 */
void UMeshBeaconHost::AcceptConnections(void)
{
	FSocket* ClientSocket = NULL;
	do
	{
		// See what clients have connected and add them for processing
		ClientSocket = Socket->Accept(TEXT("mesh beacon host client"));
		if (ClientSocket)
		{
			// Add to the list for reading
			INT AddIndex = ClientConnections.AddZeroed();
			FClientMeshBeaconConnection& ClientConn = ClientConnections(AddIndex);
			ClientConn.Socket = ClientSocket;
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) new client connection from (%s)."),
				*BeaconName.ToString(),
				*ClientSocket->GetAddress().ToString(TRUE));
		}
		else
		{
			INT SocketError = GSocketSubsystem->GetLastErrorCode();
			if (SocketError != SE_EWOULDBLOCK)
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) failed to accept a connection due to: %s."),
					*BeaconName.ToString(),
					GSocketSubsystem->GetSocketError());
			}
		}
	}
	while (ClientSocket);

	// Determine if the list of pending players have all established a connection
	if (PendingPlayerConnections.Num() > 0)
	{
		if (AllPlayersConnected(PendingPlayerConnections))
		{
			// Trigger delegate once all the connections for pending players have been established
			delegateOnAllPendingPlayersConnected();
			// Pending list is always cleared once delegate is triggered
			PendingPlayerConnections.Empty();
		}
	}
}

/**
 * Reads the socket and processes any data from it
 *
 * @param ClientConn the client connection that sent the packet
 *
 * @return TRUE if the socket is ok, FALSE if it is in error
 */
UBOOL UMeshBeaconHost::ReadClientData(FClientMeshBeaconConnection& ClientConn)
{
	const INT MAX_READ_PACKET_SIZE = 8 * 1024;
	BYTE PacketData[MAX_READ_PACKET_SIZE];
	// Read each pending packet and pass it out for processing
	UBOOL bShouldRead = TRUE;
	while (bShouldRead)
	{
		INT BytesRead;
		// Read from the socket and handle errors if present
		if (ClientConn.Socket->Recv(PacketData,MAX_READ_PACKET_SIZE,BytesRead))
		{
			if (BytesRead > 0)
			{
				// keep heartbeat alive for client if host receives any data from it
				ClientConn.ElapsedHeartbeatTime = 0.f;
				// Process the packet
				ProcessClientPacket(PacketData,BytesRead,ClientConn);
			}
			else
			{
				bShouldRead = FALSE;
			}
		}
		else
		{
			// Check for an error other than would block, which isn't really an error
			INT ErrorCode = GSocketSubsystem->GetLastErrorCode();
			if (ErrorCode != SE_EWOULDBLOCK)
			{
				// Zero the party leader, so his socket timeout doesn't cause another cancel
				(QWORD&)ClientConn.PlayerNetId = 0;
				// Log and remove this connection
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) closing socket to client (%s) (PlayerNetId=0x%016I64X) with error (%s)."),
					*BeaconName.ToString(),
					*ClientConn.Socket->GetAddress().ToString(TRUE),
					(QWORD&)ClientConn.PlayerNetId,
					GSocketSubsystem->GetSocketError(ErrorCode));
				return FALSE;
			}
			bShouldRead = FALSE;
		}
	}
	return TRUE;
}

/**
 * Processes a packet that was received from a client
 *
 * @param Packet the packet that the client sent
 * @param PacketSize the size of the packet to process
 * @param ClientConn the client connection that sent the packet
 */
void UMeshBeaconHost::ProcessClientPacket(BYTE* Packet,INT PacketSize,FClientMeshBeaconConnection& ClientConn)
{
	// Use our packet serializer to read from the raw buffer
	FNboSerializeFromBuffer FromBuffer(Packet,PacketSize);

	// Work through the stream until we've consumed it all
	do
	{
		const INT AvailableToRead = FromBuffer.AvailableToRead();
		BYTE PacketType = MB_Packet_UnknownType;
		FromBuffer >> PacketType;

		// Handle processing of packets received while bandwidth testing is in progress
		if (ClientConn.BandwidthTest.CurrentState == MB_BandwidthTestState_InProgress &&
			!FromBuffer.HasOverflow())
		{
			// Process dummy packets for testing
			ProcessClientInProgressBandwidthTest(PacketType,AvailableToRead,FromBuffer,ClientConn);
		}

		// If testing is no longer in progress then check to see if there are other packets left to process
		if (ClientConn.BandwidthTest.CurrentState != MB_BandwidthTestState_InProgress &&
			!FromBuffer.HasOverflow())
		{
			// Route the client packet type
			if (!HandleClientPacketByType(PacketType,FromBuffer,ClientConn))
			{
				break;
			}
		}
	}
	while (!FromBuffer.HasOverflow());
}

/**
 * Routes the packet received from a client to the correct handler based on its type.
 * Overridden by base implementations to handle custom data packet types
 *
 * @param ClientPacketType packet ID from EReservationPacketType (or derived version) that represents a client request
 * @param FromBuffer the packet serializer to read from
 * @param ClientConn the client connection that sent the packet
 * @return TRUE if the requested packet type was processed
 */
UBOOL UMeshBeaconHost::HandleClientPacketByType(BYTE ClientPacketType,FNboSerializeFromBuffer& FromBuffer,FClientMeshBeaconConnection& ClientConn)
{
#if DEBUG_PACKET_TRAFFIC
	debugf(NAME_DevBeacon,
		TEXT("(UMeshBeaconHost.HandleClientPacketByType): ClientPacketType=%s"),
		*EnumTypeToString(ClientPacketType,TEXT("EMeshBeaconPacketType")));
#endif

	// Route the call to the proper handler
	switch (ClientPacketType)
	{
		case MB_Packet_ClientNewConnectionRequest:
		{
			ProcessClientConnectionRequest(FromBuffer,ClientConn);
			break;
		}
		case MB_Packet_ClientBeginBandwidthTest:
		{
			ProcessClientBeginBandwidthTest(FromBuffer,ClientConn);
			break;
		}
		case MB_Packet_ClientCreateNewSessionResponse:
		{
			ProcessClientCreateNewSessionResponse(FromBuffer,ClientConn);
			break;
		}
		case MB_Packet_DummyData:
		{
			ProcessDummyPackets(FromBuffer);
			break;
		}
		case MB_Packet_Heartbeat:
		{
			break;
		}
		default:
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) unknown packet type (%d) received from client (%s) (PlayerNetId=0x%016I64X)."),
				*BeaconName.ToString(),
				(DWORD)ClientPacketType,
				*ClientConn.Socket->GetAddress().ToString(TRUE),
				(QWORD&)ClientConn.PlayerNetId);

			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Read the client data for a new connection request. Includes player ID, NAT type, bandwidth history.
 *
 * @param FromBuffer the packet serializer to read from
 * @param ClientConn the client connection that sent the packet
 */
void UMeshBeaconHost::ProcessClientConnectionRequest(FNboSerializeFromBuffer& FromBuffer,FClientMeshBeaconConnection& ClientConn)
{
	EMeshBeaconConnectionResult Result = MB_ConnectionResult_Succeeded;

	// Packet format is <MB_Packet_ClientNewConnectionRequest><PlayerNetId><NATType><GoodHostRatio><bCanHostVs><MinutesSinceLastTest><BanwidthHistory(NumEntries)(Entry[0]..Entry[NumEntries-1])>
	BYTE bCanHostVs = FALSE;
	FromBuffer	>> ClientConn.PlayerNetId
				>> ClientConn.NatType
				>> ClientConn.GoodHostRatio
				>> bCanHostVs
				>> ClientConn.MinutesSinceLastTest;
	ClientConn.bCanHostVs = bCanHostVs;
	INT NumBandwidthEntries = 0;
	FromBuffer	>> NumBandwidthEntries;
	ClientConn.BandwidthHistory.Empty(NumBandwidthEntries);
	for (INT EntryIdx=0; EntryIdx < NumBandwidthEntries; EntryIdx++)
	{
		FConnectionBandwidthStats& BandwidthEntry = ClientConn.BandwidthHistory(ClientConn.BandwidthHistory.AddZeroed());
		FromBuffer	>> BandwidthEntry;
	}
	// Only keep up to MaxBandwidthHistoryEntries in the history
	if (ClientConn.BandwidthHistory.Num() > MaxBandwidthHistoryEntries)
	{
		ClientConn.BandwidthHistory.Remove(MaxBandwidthHistoryEntries,ClientConn.BandwidthHistory.Num()-MaxBandwidthHistoryEntries);
	}
	// Check for an existing connection for this player that has already been accepted
	const INT ExistingConnectionIdx = GetConnectionIndexForPlayer(ClientConn.PlayerNetId);
	if (ClientConnections.IsValidIndex(ExistingConnectionIdx) && 
		ClientConnections(ExistingConnectionIdx).bConnectionAccepted)
	{
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) received duplicate connection request from client (%s) (PlayerNetId=0x%016I64X)."),
			*BeaconName.ToString(),
			*ClientConn.Socket->GetAddress().ToString(TRUE),
			(QWORD&)ClientConn.PlayerNetId);

		// Assume that client will disconnect if they receive a duplicate response
		Result = MB_ConnectionResult_Duplicate;
	}
	else
	{
		// Mark connection as accepted to later check for duplicates
		ClientConn.bConnectionAccepted = TRUE;

		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) received connection request from client (%s) (PlayerNetId=0x%016I64X)."),
			*BeaconName.ToString(),
			*ClientConn.Socket->GetAddress().ToString(TRUE),
			(QWORD&)ClientConn.PlayerNetId);
		// Trigger delegate only for non-duplicate connection requests
		delegateOnReceivedClientConnectionRequest(ClientConn);
	}

	// Send an Ack to the client for the successful connection request
	SendClientConnectionResponse(Result,ClientConn);	
}

/**
 * Sends the results of a connection request by the client.
 *
 * @param ConnectionResult result of the connection request
 * @param ClientConn the client connection with socket to send the response on
 */
void UMeshBeaconHost::SendClientConnectionResponse(EMeshBeaconConnectionResult ConnectionResult,FClientMeshBeaconConnection& ClientConn)
{
	check(ClientConn.Socket);
	// Packet format is <MB_Packet_HostNewConnectionResponse><Result>
	FNboSerializeToBuffer ToBuffer(512);
	ToBuffer	<< (BYTE)MB_Packet_HostNewConnectionResponse
				<< (BYTE)ConnectionResult;
	INT BytesSent;
	UBOOL bDidSendOk = ClientConn.Socket->Send(ToBuffer.GetRawBuffer(0),ToBuffer.GetByteCount(),BytesSent);
	if (bDidSendOk == FALSE)
	{
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) failed to send connection request response to client with (%s)"),
			*BeaconName.ToString(),
			GSocketSubsystem->GetSocketError());
	}
}

/**
 * The client has started sending data for a new bandwidth test. 
 * Begin measurements for test and process data that is received.
 *
 * @param FromBuffer the packet serializer to read from
 * @param ClientConn the client connection that sent the packet
 */
void UMeshBeaconHost::ProcessClientBeginBandwidthTest(FNboSerializeFromBuffer& FromBuffer,FClientMeshBeaconConnection& ClientConn)
{
	// Packet format is <MB_Packet_ClientBeginBandwidthTest><TestType><TestByteSize>
	BYTE TestType = MB_BandwidthTestType_Upstream;
	INT NumBytesBeingSent = 0;
	FromBuffer	>> TestType
				>> NumBytesBeingSent;

	debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) received bandwidth test start from client (%s) (PlayerNetId=0x%016I64X). Starting test Type=%s."),
			*BeaconName.ToString(),
			*ClientConn.Socket->GetAddress().ToString(TRUE),
			(QWORD&)ClientConn.PlayerNetId,
			*EnumTypeToString(TestType,TEXT("EMeshBeaconBandwidthTestType")));

	if (bAllowBandwidthTesting)
	{
		// Handle starting the requested bandwidth test type
		switch(TestType)
		{
			case MB_BandwidthTestType_Upstream:
			{
				BeginUpstreamTest(ClientConn,NumBytesBeingSent);
				break;
			}
			case MB_BandwidthTestType_Downstream:
			case MB_BandwidthTestType_RoundtripLatency:
			default:
			{
				debugf(NAME_DevBeacon,
					TEXT("(UMeshBeaconHost.ProcessClientBeginBandwidthTest): Bandwidth test type not implemented!"));
			}
		}
	}
	else
	{
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) can't start test for client (%s) (PlayerNetId=0x%016I64X). Bandwidth testing has been disabled."),
			*BeaconName.ToString(),
			*ClientConn.Socket->GetAddress().ToString(TRUE),
			(QWORD&)ClientConn.PlayerNetId);
	}
}

/**
 * The client currently has a bandwidth test that has been started and is now in progress.
 * Process data that is received and handle timeout and finishing the test.
 * Only packets of type MB_Packet_DummyData are expected from the client once the test has started.
 *
 * @param PacketType type of packet read from the buffer
 * @param AvailableToRead data still available to read from the buffer
 * @param FromBuffer the packet serializer to read from
 * @param ClientConn the client connection that sent the packet
 */
void UMeshBeaconHost::ProcessClientInProgressBandwidthTest(BYTE PacketType,INT AvailableToRead,FNboSerializeFromBuffer& FromBuffer,FClientMeshBeaconConnection& ClientConn)
{
	// Delta time since the first test packet was received on the host
	const DOUBLE CurTime = appSeconds();
	const DOUBLE UpstreamTestDeltaTime = CurTime - ClientConn.BandwidthTest.TestStartTime;
	// Handle timeout if the maximum time has been exceed to receive the data for the bandwidth test
	if (UpstreamTestDeltaTime >= MaxBandwidthTestReceiveTime)
	{
		// End test due to timeout
		ClientConn.BandwidthTest.CurrentState = MB_BandwidthTestState_Timeout;
		FinishUpstreamTest(ClientConn);
	}
	else
	{
		// Upstream bytes still needed to complete the current bandwidth test
		const INT BytesNeeded = ClientConn.BandwidthTest.BytesTotalNeeded - ClientConn.BandwidthTest.BytesReceived;			

#if DEBUG_PACKET_TRAFFIC
 		debugf(NAME_DevBeacon,
			TEXT("(UMeshBeaconHost.ProcessClientPacket): reading bandwidth data AvailableToRead=%d BytesReceived=%d BytesNeeded=%d"),
 			AvailableToRead, 
			ClientConn.BandwidthTest.BytesReceived, 
			BytesNeeded);
#endif

		// Check for dummy buffer for bandwidth testing. The data doesn't have to be read.
		if (PacketType == MB_Packet_DummyData)
		{
			// Read all data requested for the upstream test
			if (BytesNeeded <= AvailableToRead)
			{
				// All data has been read for bandwidth test
				ClientConn.BandwidthTest.BytesReceived += BytesNeeded;
				FinishUpstreamTest(ClientConn);
				// Seek to the end of the dummy data and allow processing to continue
				FromBuffer.Seek(FromBuffer.Tell() + BytesNeeded);
			}
			else
			{
				// Keep track of all the data that was read from the last recv buffer
				ClientConn.BandwidthTest.BytesReceived += AvailableToRead;
				// Seek to the end of the test buffer (entire buffer)
				FromBuffer.Seek(FromBuffer.GetBufferSize());
			}

			// Keep updating the current bandwidth stat in bytes per sec
			ClientConn.BandwidthTest.BandwidthStats.UpstreamRate = appTrunc((FLOAT)ClientConn.BandwidthTest.BytesReceived / UpstreamTestDeltaTime);
		
		}
		else
		{
#if DEBUG_PACKET_TRAFFIC
			debugf(NAME_DevBeacon,
				TEXT("(UMeshBeaconHost.ProcessClientPacket): packet was not MB_Packet_DummyData. Ending current test."));
#endif
			// Didn't receive all the data for the bandwidth test, but end it anyway
			FinishUpstreamTest(ClientConn);
		}
	}
}

/**
 * Begin processing for a new upstream bandwidth test on a client.  All packets
 * from the client are expected to be dummy packets from this point until NumBytesBeingSent is 
 * reached or we hit timeout receiving the data (MaxBandwidthTestReceiveTime).
 *
 * @param ClientConn the client connection that is sending packets for the test
 * @param NumBytesBeingSent expected size of test data being sent in bytes for the bandwidth test to complete
 */
void UMeshBeaconHost::BeginUpstreamTest(FClientMeshBeaconConnection& ClientConn, INT NumBytesBeingSent)
{
	const DOUBLE CurTime = appSeconds();

	// Client has initiated an upstream test. Switch state to begin processing the upstream data
	ClientConn.BandwidthTest.TestType = MB_BandwidthTestType_Upstream;
	ClientConn.BandwidthTest.TestStartTime = CurTime;
	ClientConn.BandwidthTest.BytesTotalNeeded = NumBytesBeingSent;
	ClientConn.BandwidthTest.BytesReceived = 0;
	ClientConn.BandwidthTest.CurrentState = MB_BandwidthTestState_InProgress;

	// Latency based on delta from time the test was requested to when a response was received for it to start
	const DOUBLE RoundtripDeltaTime = CurTime - ClientConn.BandwidthTest.RequestTestStartTime;
	// Store roundtrip latency as milliseconds
	ClientConn.BandwidthTest.BandwidthStats.RoundtripLatency = appTrunc(RoundtripDeltaTime * 1000.f);

	debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) bandwidth test roundtrip delta time =%.2f msecs from client (%s) (PlayerNetId=0x%016I64X)."),
			*BeaconName.ToString(),
			RoundtripDeltaTime * 1000.f,
			*ClientConn.Socket->GetAddress().ToString(TRUE),			
			(QWORD&)ClientConn.PlayerNetId);

	// Trigger delegate to indicate the start of a bandwidth test on the host
	delegateOnStartedBandwidthTest(ClientConn.PlayerNetId,ClientConn.BandwidthTest.TestType);
}

/**
 * Finish process for an in-progress upstream bandwidth test on a client.  The test
 * is marked as completed successfully if all the expected data for the test was received
 * or if the test ended prematurely but there was still enough data (MinBandwidthTestBufferSize) 
 * to calculate results.
 *
 * @param ClientConn the client connection that is sending packets for the test
 */
void UMeshBeaconHost::FinishUpstreamTest(FClientMeshBeaconConnection& ClientConn)
{
	EMeshBeaconBandwidthTestResult TestResult;
	const DOUBLE CurTime = appSeconds();
	const DOUBLE UpstreamTestDeltaTime = CurTime - ClientConn.BandwidthTest.TestStartTime;
	if (UpstreamTestDeltaTime > 0)
	{
		if (ClientConn.BandwidthTest.BytesReceived >= ClientConn.BandwidthTest.BytesTotalNeeded)
		{
			// All requested data was received so mark test as fully completed 
			ClientConn.BandwidthTest.CurrentState = MB_BandwidthTestState_Completed;
			TestResult = MB_BandwidthTestResult_Succeeded;
		}
		else if (ClientConn.BandwidthTest.BytesReceived >= MinBandwidthTestBufferSize)
		{
			if (ClientConn.BandwidthTest.CurrentState == MB_BandwidthTestState_Timeout)
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) bandwidth test: Timeout reading results from client. But still enough to calc results."),
					*BeaconName.ToString());
			}
			else
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) bandwidth test: Didn't read all data requested from client. But still enough to calc results."),
					*BeaconName.ToString());
			}

			// Test didn't complete due to timeout either on client or host. 
			// Mark test as incomplete if at least got the minimum required size
			ClientConn.BandwidthTest.CurrentState = MB_BandwidthTestState_Incomplete;
			TestResult = MB_BandwidthTestResult_Succeeded;
		}
		else if (ClientConn.BandwidthTest.CurrentState == MB_BandwidthTestState_Timeout)
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) bandwidth test: Timeout reading results from client. Not enough data was received to calc results."),
				*BeaconName.ToString());

			// Mark as timed out
			TestResult = MB_BandwidthTestResult_Timeout;
		}
		else
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) bandwidth test: Error running test. Not enough data was received to calc results."),
				*BeaconName.ToString());

			// Mark as errored for any other reason
			ClientConn.BandwidthTest.CurrentState = MB_BandwidthTestState_Error;
			TestResult = MB_BandwidthTestResult_Error;
		}
		// Upstream results in bytes per sec
		ClientConn.BandwidthTest.BandwidthStats.UpstreamRate = appTrunc((FLOAT)ClientConn.BandwidthTest.BytesReceived / UpstreamTestDeltaTime);

		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) bandwidth test finished: State=%s BytesReceived=%d BytesTotalNeeded=%d UpstreamTestDeltaTime=%.4f secs Bandwidth=%.4f Kbps"),
			*BeaconName.ToString(),
			*EnumTypeToString(ClientConn.BandwidthTest.CurrentState,TEXT("EMeshBeaconBandwidthTestState")),
			ClientConn.BandwidthTest.BytesReceived,
			ClientConn.BandwidthTest.BytesTotalNeeded,
			UpstreamTestDeltaTime, 
			ClientConn.BandwidthTest.BandwidthStats.UpstreamRate * 8.0f / 1024.f);
	}
	else
	{
		ClientConn.BandwidthTest.CurrentState = MB_BandwidthTestState_Error;
		TestResult = MB_BandwidthTestResult_Error;
	}

	// Add new bandwidth results to history of client only if the test was successful
	if (TestResult != MB_BandwidthTestResult_Succeeded)
	{
		// if the test didn't finish due to error/timeout then just clamp to min value of 56 kbps
		ClientConn.BandwidthTest.BandwidthStats.UpstreamRate = 56 * 1024 / 8;		
	}
	// Reset time since last test
	ClientConn.MinutesSinceLastTest = 0;
	// Add the new bandwidth test entry to the front of the list
	ClientConn.BandwidthHistory.InsertItem(ClientConn.BandwidthTest.BandwidthStats,0);
	// Only keep up to MaxBandwidthHistoryEntries in the history
	if (ClientConn.BandwidthHistory.Num() > MaxBandwidthHistoryEntries)
	{
		ClientConn.BandwidthHistory.Remove(MaxBandwidthHistoryEntries,ClientConn.BandwidthHistory.Num()-MaxBandwidthHistoryEntries);
	}

	// Send results of bandwidth testing back to client
	SendBandwidthTestCompletedResponse(TestResult, ClientConn);
	// Trigger delegate indicating bandwidth testing has finished for the client
	delegateOnFinishedBandwidthTest(
		ClientConn.PlayerNetId, 
		ClientConn.BandwidthTest.TestType, 
		TestResult, 
		ClientConn.BandwidthTest.BandwidthStats);
}

/**
 * Sends a request to client to start a new bandwidth test.
 *
 * @param TestType EMeshBeaconBandwidthTestType type of bandwidth test to request
 * @param TestBufferSize size of buffer to use for the test
 * @param ClientConn the client connection with socket to send the response on
 */
void UMeshBeaconHost::SendBandwidthTestStartRequest(BYTE TestType,INT TestBufferSize,FClientMeshBeaconConnection& ClientConn)
{
	check(TestType < MB_BandwidthTestType_MAX);
	check(TestBufferSize > 0);

	// Clear out previous results since a new test is being requested
	appMemzero(&ClientConn.BandwidthTest,sizeof(ClientConn.BandwidthTest));
	ClientConn.BandwidthTest.TestType = TestType;
	ClientConn.BandwidthTest.BytesTotalNeeded = TestBufferSize;
	ClientConn.BandwidthTest.RequestTestStartTime = appSeconds();
	ClientConn.BandwidthTest.CurrentState = MB_BandwidthTestState_StartPending;

	//@todo sz - need a timeout waiting for a client to start a pending bandwidth test

	// Send the request packet to the client
	// Packet format is <MB_Packet_HostBandwidthTestRequest><TestType><TestBufferSize>
	FNboSerializeToBuffer ToBuffer(512);
	ToBuffer << (BYTE)MB_Packet_HostBandwidthTestRequest
			 << TestType
			 << TestBufferSize;

	FSocket* ClientSocket = ClientConn.Socket;
	check(ClientSocket != NULL);

	INT BytesSent;
	UBOOL bResult = ClientSocket->Send(ToBuffer.GetRawBuffer(0),ToBuffer.GetByteCount(),BytesSent);
	if (bResult)
	{
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) sent bandwidth test request to client (%s) (PlayerNetId=0x%016I64X)."),
			*BeaconName.ToString(),
			*ClientSocket->GetAddress().ToString(TRUE),
			(QWORD&)ClientConn.PlayerNetId);
	}
	else
	{
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) failed to send bandwidth test request to client (%s) (PlayerNetId=0x%016I64X) with (%s)."),
			*BeaconName.ToString(),
			*ClientSocket->GetAddress().ToString(TRUE),
			(QWORD&)ClientConn.PlayerNetId,
			GSocketSubsystem->GetSocketError());

		ClientConn.BandwidthTest.CurrentState = MB_BandwidthTestState_Error;
	}
}

/**
 * Sends the results of a completed bandwidth test to the client.
 *
 * @param TestResult result of the bandwidth test
 * @param ClientConn the client connection with socket to send the response on
 */
void UMeshBeaconHost::SendBandwidthTestCompletedResponse(EMeshBeaconBandwidthTestResult TestResult,FClientMeshBeaconConnection& ClientConn)
{
	// Create a buffer and serialize the request in
	FNboSerializeToBuffer ToBuffer(512);
	// Packet format is <MB_Packet_HostCompletedBandwidthTest><TestType><BytesReceived><UpstreamRate><DownstreamRate><RoundtripLatency>
	ToBuffer << (BYTE)MB_Packet_HostCompletedBandwidthTest
			 << (BYTE)TestResult
			 << ClientConn.BandwidthTest.TestType
			 << ClientConn.BandwidthTest.BytesReceived
			 << ClientConn.BandwidthTest.BandwidthStats;

	INT BytesSent;
	// Now send to the destination host
	if (ClientConn.Socket->Send(ToBuffer.GetRawBuffer(0),ToBuffer.GetByteCount(),BytesSent))
	{
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) sent bandwidth test results to client (%s) (PlayerNetId=0x%016I64X)."),
			*BeaconName.ToString(),
			*ClientConn.Socket->GetAddress().ToString(TRUE),
			(QWORD&)ClientConn.PlayerNetId);
	}
	else
	{
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) SendRequest() failed to send the packet to the client (%s) (PlayerNetId=0x%016I64X) with (%s)."),
			*BeaconName.ToString(),
			*ClientConn.Socket->GetAddress().ToString(TRUE),
			(QWORD&)ClientConn.PlayerNetId,
			GSocketSubsystem->GetSocketError());
	}
}

/**
 * Send a request to a client connection to initiate a new bandwidth test.
 *
 * @param PlayerNetId player with an active connection to receive test request
 * @param TestType EMeshBeaconBandwidthTestType type of bandwidth test to request
 * @param TestBufferSize size of buffer in bytes to use for running the test
 * @return TRUE if the request was successfully sent to the client
 */
UBOOL UMeshBeaconHost::RequestClientBandwidthTest(struct FUniqueNetId PlayerNetId,BYTE TestType,INT TestBufferSize)
{
	check(TestType < MB_BandwidthTestType_MAX);
	check(TestBufferSize > 0);
	
	UBOOL bResult = FALSE;
	if (bAllowBandwidthTesting)
	{
		// Find the client connection entry for the given player net Id
		INT ClientFoundIdx = INDEX_NONE;
		for (INT ClientIdx=0; ClientIdx < ClientConnections.Num(); ClientIdx++)
		{
			FClientMeshBeaconConnection& ClientConn = ClientConnections(ClientIdx);
			if (ClientConn.PlayerNetId == PlayerNetId)
			{
				ClientFoundIdx = ClientIdx;
				break;
			}
		}
		if (ClientConnections.IsValidIndex(ClientFoundIdx))
		{
			FClientMeshBeaconConnection& ClientConn = ClientConnections(ClientFoundIdx);		
			// Make sure a test has not already been initiated for this client connection
			if (ClientConn.BandwidthTest.CurrentState != MB_BandwidthTestState_InProgress &&
				ClientConn.BandwidthTest.CurrentState != MB_BandwidthTestState_StartPending &&
				ClientConn.BandwidthTest.CurrentState != MB_BandwidthTestState_RequestPending)
			{
				// Mark as requested a bandwidth test so that it can be serviced when in order
				// Only a single client can have a bandwidth test started at a time
				ClientConn.BandwidthTest.CurrentState = MB_BandwidthTestState_RequestPending;
				ClientConn.BandwidthTest.TestType = TestType;
				ClientConn.BandwidthTest.BytesTotalNeeded = TestBufferSize;

				bResult = TRUE;
			}
			else
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) couldn't request bandiwdth test (%s). Already performing a test for PlayerNetId=0x%016I64X"),
					*BeaconName.ToString(),
					*EnumTypeToString(ClientConn.BandwidthTest.CurrentState,TEXT("EMeshBeaconBandwidthTestState")),
					(QWORD&)PlayerNetId);
			}
		}
		else
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) couldn't request bandiwdth test. No client connection for PlayerNetId=0x%016I64X"),
				*BeaconName.ToString(),
				(QWORD&)PlayerNetId);

		}
	}
	else
	{
		debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) couldn't request bandiwdth test for PlayerNetId=0x%016I64X. bAllowBandwidthTesting=false."),
				*BeaconName.ToString(),
				(QWORD&)PlayerNetId);
	}
	return bResult;
}

/**
 * Determine if a client is currently running a bandwidth test.
 *
 * @return TRUE if a client connection is currently running a bandwidth test
 */
UBOOL UMeshBeaconHost::HasInProgressBandwidthTest()
{
	UBOOL bResult = FALSE;
	for (INT ClientIdx=0; ClientIdx < ClientConnections.Num(); ClientIdx++)
	{
		const FClientMeshBeaconConnection& ClientConn = ClientConnections(ClientIdx);
		if (ClientConn.BandwidthTest.CurrentState == MB_BandwidthTestState_InProgress ||
			ClientConn.BandwidthTest.CurrentState == MB_BandwidthTestState_StartPending)
		{
			bResult = TRUE;
			break;
		}
	}
	return bResult;
}

/**
 * Cancel any bandwidth tests that are already in progress.
 */
void UMeshBeaconHost::CancelInProgressBandwidthTests()
{
	for (INT ClientIdx=0; ClientIdx < ClientConnections.Num(); ClientIdx++)
	{
		FClientMeshBeaconConnection& ClientConn = ClientConnections(ClientIdx);
		if (ClientConn.BandwidthTest.CurrentState == MB_BandwidthTestState_InProgress ||
			ClientConn.BandwidthTest.CurrentState == MB_BandwidthTestState_StartPending )
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) cancelling in progress bandwidth test for client (%s) PlayerNetId=0x%016I64X"),
				*BeaconName.ToString(),
				*ClientConn.Socket->GetAddress().ToString(TRUE),
				(QWORD&)ClientConn.PlayerNetId);

			switch(ClientConn.BandwidthTest.TestType)
			{
				case MB_BandwidthTestType_Upstream:
				{
					// Finish the test without waiting for all the requested data to be received from the client
					FinishUpstreamTest(ClientConn);
					break;
				}
				default:
				{
					debugf(NAME_DevBeacon,TEXT(" (UMeshBeaconHost.ProcessClientBeginBandwidthTest): not implemented!"));
				}
			}
		}
	}
}

/**
 * Determine if a client is currently waiting/pending for a bandwidth test.
 *
 * @return TRUE if a client connection is currently pending a bandwidth test
 */
UBOOL UMeshBeaconHost::HasPendingBandwidthTest()
{
	UBOOL bResult = FALSE;
	for (INT ClientIdx=0; ClientIdx < ClientConnections.Num(); ClientIdx++)
	{
		const FClientMeshBeaconConnection& ClientConn = ClientConnections(ClientIdx);
		if (ClientConn.BandwidthTest.CurrentState == MB_BandwidthTestState_RequestPending)
		{
			bResult = TRUE;
			break;
		}
	}
	return bResult;
}

/**
 * Cancel any bandwidth tests that are pending.
 */
void UMeshBeaconHost::CancelPendingBandwidthTests()
{
	for (INT ClientIdx=0; ClientIdx < ClientConnections.Num(); ClientIdx++)
	{
		FClientMeshBeaconConnection& ClientConn = ClientConnections(ClientIdx);
		if (ClientConn.BandwidthTest.CurrentState == MB_BandwidthTestState_RequestPending)
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) cancelling pending bandwidth test for client (%s) PlayerNetId=0x%016I64X"),
				*BeaconName.ToString(),
				*ClientConn.Socket->GetAddress().ToString(TRUE),
				(QWORD&)ClientConn.PlayerNetId);

			ClientConn.BandwidthTest.CurrentState = MB_BandwidthTestState_NotStarted;
		}
	}
}

/**
 * Determine if the given player has an active connection on this host beacon.
 *
 * @param PlayerNetId player we are searching for
 * @return index within ClientConnections for the player's connection, -1 if not found
 */
INT UMeshBeaconHost::GetConnectionIndexForPlayer(struct FUniqueNetId PlayerNetId)
{
	INT Result = INDEX_NONE;
	if (PlayerNetId.HasValue())
	{			
		for (INT ClientIdx=0; ClientIdx < ClientConnections.Num(); ClientIdx++)
		{
			const FClientMeshBeaconConnection& ClientConn = ClientConnections(ClientIdx);
			// Match connection for player by his unique Net id
			if (PlayerNetId == ClientConn.PlayerNetId)
			{
				Result = ClientIdx;
				break;
			}
		}
	}
	return Result;
}

/**
 * Determine if the players all have connections on this host beacon
 *
 * @param Players list of player ids we are searching for
 * @return TRUE if all players had connections
 */
UBOOL UMeshBeaconHost::AllPlayersConnected(const TArray<struct FUniqueNetId>& Players)
{
	UBOOL bResult = TRUE;
	for (INT PlayerIdx=0; PlayerIdx < Players.Num(); PlayerIdx++)
	{
		const FUniqueNetId& PendingPlayerNetId = Players(PlayerIdx);
		// Check to see if player is hosting this beacon
		if (PendingPlayerNetId != OwningPlayerId &&
		// Check all client connections to find the one matching the pending player net id
			GetConnectionIndexForPlayer(PendingPlayerNetId) == INDEX_NONE)
		{
			bResult = FALSE;
			break;
		}
	}
	return bResult;
}

/**
 * Tells all of the clients to go to a specific session (contained in platform
 * specific info). Used to route all clients to one destination.
 *
 * @param SessionName the name of the session to register
 * @param SearchClass the search that should be populated with the session
 * @param PlatformSpecificInfo the binary data to place in the platform specific areas
 */
void UMeshBeaconHost::TellClientsToTravel(FName SessionName,class UClass* SearchClass,const BYTE* PlatformSpecificInfo)
{
	check(SearchClass != NULL && SearchClass->IsChildOf(UOnlineGameSearch::StaticClass()));

	debugf(NAME_DevBeacon,TEXT("Beacon (%s) sending travel information to clients."),
		*BeaconName.ToString());
	const FString& SessionNameStr = SessionName.ToString();
	const FString& ClassName = SearchClass->GetPathName();
	// Build the packet that we are sending
	FNboSerializeToBuffer ToBuffer(512);
	// Packet format is <MB_Packet_HostTravelRequest><SessionNameLen><SessionName><ClassNameLen><ClassName><SecureSessionInfo>
	ToBuffer << (BYTE)MB_Packet_HostTravelRequest
		<< SessionNameStr
		<< ClassName;
	// Copy the buffer over in raw form (it is already in NBO)
	ToBuffer.WriteBinary(PlatformSpecificInfo,80);
	INT BytesSent;
	// Iterate through and send to each client
	for (INT SocketIndex = 0; SocketIndex < ClientConnections.Num(); SocketIndex++)
	{
		FClientMeshBeaconConnection& ClientConn = ClientConnections(SocketIndex);
		if ((QWORD&)ClientConn.PlayerNetId != (QWORD)0)
		{
			FSocket* ClientSocket = ClientConn.Socket;
			check(ClientSocket);
			UBOOL bDidSendOk = ClientSocket->Send(ToBuffer.GetRawBuffer(0),ToBuffer.GetByteCount(),BytesSent);
			if (bDidSendOk == FALSE)
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) failed to send travel request to client with (%s)"),
					*BeaconName.ToString(),
					GSocketSubsystem->GetSocketError());
			}
		}
	}

	// Stop ticking the beacon to avoid processing new requests
	bShouldTick = FALSE;
}

/**
 * Sends a request to a specified client to create a new game session.
 *
 * @param PlayerNetId net id of player for client connection to send request to
 * @param SessionName the name of the session to create
 * @param SearchClass the search that should be with corresponding game settings when creating the session
 * @param Players list of players to register on the newly created session
 */
UBOOL UMeshBeaconHost::RequestClientCreateNewSession(struct FUniqueNetId PlayerNetId,FName SessionName,class UClass* SearchClass,const TArray<struct FPlayerMember>& Players)
{
	UBOOL bResult = FALSE;

	check(SearchClass != NULL && SearchClass->IsChildOf(UOnlineGameSearch::StaticClass()));

	const INT PlayerConnectionIdx = GetConnectionIndexForPlayer(PlayerNetId);
	if (ClientConnections.IsValidIndex(PlayerConnectionIdx))
	{
		FClientMeshBeaconConnection& ClientConn = ClientConnections(PlayerConnectionIdx);

		// Create a buffer and serialize the request in
		FNboSerializeToBuffer ToBuffer(512);		
		const FString& SessionNameStr = SessionName.ToString();
		const FString& ClassName = SearchClass->GetPathName();

		// Packet format is <MB_Packet_HostCreateNewSessionRequest><SessionNameLen><SessionName><ClassNameLen><ClassName><PlayersLen><Players>
		ToBuffer << (BYTE)MB_Packet_HostCreateNewSessionRequest
				 << SessionNameStr
				 << ClassName
				 << Players.Num();
		for (INT PlayerIdx=0; PlayerIdx < Players.Num(); PlayerIdx++)
		{
			const FPlayerMember& PlayerMember = Players(PlayerIdx);
			ToBuffer << PlayerMember;
		}

		INT BytesSent;
		check(ClientConn.Socket);

		UBOOL bDidSendOk = ClientConn.Socket->Send(ToBuffer.GetRawBuffer(0),ToBuffer.GetByteCount(),BytesSent);
		bResult = bDidSendOk;
		if (bDidSendOk == FALSE)
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) failed to send new session request to client PlayerNetId=0x%016I64X with (%s)"),
				*BeaconName.ToString(),
				(QWORD&)PlayerNetId,
				GSocketSubsystem->GetSocketError());
		}
		else
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) sending new session create request to client (%s) (PlayerNetId=0x%016I64X)"),
				*BeaconName.ToString(),
				*ClientConn.Socket->GetAddress().ToString(TRUE),
				(QWORD&)ClientConn.PlayerNetId);
		}
	}
	else
	{
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) failed sending new session create request to client. Can't find connection for PlayerNetId=0x%016I64X."),
			*BeaconName.ToString(),
			(QWORD&)PlayerNetId);
	}
	return bResult;
}

/**
 * The client has create a new game session and has sent the session results back.
 *
 * @param FromBuffer the packet serializer to read from
 * @param ClientConn the client connection that sent the packet
 */
void UMeshBeaconHost::ProcessClientCreateNewSessionResponse(FNboSerializeFromBuffer& FromBuffer,FClientMeshBeaconConnection& ClientConn)
{
	// Packet format is <MB_Packet_ClientCreateNewSessionResponse><bSuccess><SessionNameLen><SessionName><ClassNameLen><ClassName><SecureSessionInfo>
	FString SessionNameStr;
	FString ClassNameStr;
	BYTE bSuccess = FALSE;
	// Read the two strings first
	FromBuffer	>> bSuccess
				>> SessionNameStr 
				>> ClassNameStr;
	// Now copy the buffer data in
	BYTE DestinationInfo[80];
	FromBuffer.ReadBinary(DestinationInfo,80);
	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) client (%s) (PlayerNetId=0x%016I64X) sent new game session creation response (%s),(%s)"),
		*BeaconName.ToString(),
		*ClientConn.Socket->GetAddress().ToString(TRUE),
		(QWORD&)ClientConn.PlayerNetId,
		*SessionNameStr,
		*ClassNameStr);
	FName SessionName(*SessionNameStr,0);
	UClass* SearchClass = FindObject<UClass>(NULL,*ClassNameStr);
	// Fire the delegate so the host can process this new session
	delegateOnReceivedClientCreateNewSessionResult(bSuccess,SessionName,SearchClass,DestinationInfo);
}

#endif
