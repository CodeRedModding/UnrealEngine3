/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class is used to handle connections from client mesh beacons in order to 
 * establish a mesh network.
 */
class MeshBeaconHost extends MeshBeacon
	native;

/** Stats stored for the current bandwidth test on a client connection */
struct native ClientConnectionBandwidthTestData
{
	/** Current progress of bandwidth test. Only one client should be MB_BandwidthTestState_InProgress at a time. */
	var EMeshBeaconBandwidthTestState CurrentState;
	/** Type of bandwidth test currently running */
	var EMeshBeaconBandwidthTestType TestType;
	/** Total bytes needed to complete the test */
	var int BytesTotalNeeded;
	/** Total bytes received by the client so far */
	var int BytesReceived;
	/** Time when request was first sent to client to start the test*/
	var double RequestTestStartTime;
	/** Time when first response was received from client to being the test */
	var double TestStartTime;
	/** Resulting stats from the bandwidth test */
	var ConnectionBandwidthStats BandwidthStats;
};

/** Holds the information for a client and whether they've timed out */
struct native ClientMeshBeaconConnection
{
	/** The unique id of the player for this connection */
	var UniqueNetId PlayerNetId;
	/** How long it's been since the last heartbeat */
	var float ElapsedHeartbeatTime;
	/** The socket this client is communicating on */
	var native transient pointer Socket{FSocket};
	/** True if the client connection has already been accepted for this player */
	var bool bConnectionAccepted;
	/** Bandwidth test being run for the client */
	var ClientConnectionBandwidthTestData BandwidthTest;
	/** The NAT of the client as reported by the client */
	var ENatType NatType;
	/** TRUE if the client is able to host a vs match */
	var bool bCanHostVs;
	/** Ratio of successful vs unsuccessful matches hosted by this client in the past */
	var float GoodHostRatio;	
	/** 
	 * Previous bandwidth history reported by the client ordered from newest to oldest.  
	 * New bandwidth tests that occur on this host also get added to this history.
	 */
	var array<ConnectionBandwidthStats> BandwidthHistory;
	/** Elapsed time in minutes since the last bandwidth test */
	var int MinutesSinceLastTest;
};

/** The object that is used to send/receive data with the remote host/client */
var const array<ClientMeshBeaconConnection> ClientConnections;

/** List of players this beacon is waiting to establish connections to. */
var private array<UniqueNetId> PendingPlayerConnections;

/** Net Id of player that is hosting this beacon */
var const UniqueNetId OwningPlayerId;

/** TRUE if new bandwidth test requests should be handled. Set to false to ignore any pending and new requests. */
var private bool bAllowBandwidthTesting;

/** The number of connections to allow before refusing them */
var config int ConnectionBacklog;

cpptext
{
	/**
	 * Ticks the network layer to see if there are any requests or responses to requests
	 *
	 * @param DeltaTime the amount of time that has elapsed since the last tick
	 */
	virtual void Tick(FLOAT DeltaTime);

	/** 
	 * Accepts any pending connections and adds them to our queue 
	 */
	void AcceptConnections(void);

	/**
	 * Reads the socket and processes any data from it
	 *
	 * @param ClientConn the client connection that sent the packet
	 * @return TRUE if the socket is ok, FALSE if it is in error
	 */
	UBOOL ReadClientData(FClientMeshBeaconConnection& ClientConn);

	/**
	 * Processes a packet that was received from a client
	 *
	 * @param Packet the packet that the client sent
	 * @param PacketSize the size of the packet to process
	 * @param ClientConn the client connection that sent the packet
	 */
	void ProcessClientPacket(BYTE* Packet,INT PacketSize,FClientMeshBeaconConnection& ClientConn);

	/**
	 * Routes the packet received from a client to the correct handler based on its type.
	 * Overridden by base implementations to handle custom data packet types
	 *
	 * @param ClientPacketType packet ID from EMeshBeaconPacketType (or derived version) that represents a client request
	 * @param FromBuffer the packet serializer to read from
	 * @param ClientConn the client connection that sent the packet
	 * @return TRUE if the requested packet type was processed
	 */
	UBOOL HandleClientPacketByType(BYTE ClientPacketType,FNboSerializeFromBuffer& FromBuffer,FClientMeshBeaconConnection& ClientConn);

	/**
	 * Read the client data for a new connection request. Includes player ID, NAT type, bandwidth history.
	 *
	 * @param FromBuffer the packet serializer to read from
	 * @param ClientConn the client connection that sent the packet
	 */
	void ProcessClientConnectionRequest(FNboSerializeFromBuffer& FromBuffer,FClientMeshBeaconConnection& ClientConn);

	/**
	 * Sends the results of a connection request by the client.
	 *
	 * @param ConnectionResult result of the connection request
	 * @param ClientConn the client connection with socket to send the response on
	 */
	void SendClientConnectionResponse(EMeshBeaconConnectionResult ConnectionResult,FClientMeshBeaconConnection& ClientConn);

	/**
	 * The client has started sending data for a new bandwidth test. 
	 * Begin measurements for test and process data that is received.
	 *
	 * @param FromBuffer the packet serializer to read from
	 * @param ClientConn the client connection that sent the packet
	 */
	void ProcessClientBeginBandwidthTest(FNboSerializeFromBuffer& FromBuffer,FClientMeshBeaconConnection& ClientConn);

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
	void ProcessClientInProgressBandwidthTest(BYTE PacketType,INT AvailableToRead,FNboSerializeFromBuffer& FromBuffer,FClientMeshBeaconConnection& ClientConn);

	/**
	 * Begin processing for a new upstream bandwidth test on a client.  All packets
	 * from the client are expected to be dummy packets from this point until NumBytesBeingSent is 
	 * reached or we hit timeout receiving the data (MaxBandwidthTestReceiveTime).
	 *
	 * @param ClientConn the client connection that is sending packets for the test
	 * @param NumBytesBeingSent expected size of test data being sent in bytes for the bandwidth test to complete
	 */
	void BeginUpstreamTest(FClientMeshBeaconConnection& ClientConn, INT NumBytesBeingSent);
	
	/**
	 * Finish process for an in-progress upstream bandwidth test on a client.  The test
	 * is marked as completed successfully if all the expected data for the test was received
	 * or if the test ended prematurely but there was still enough data (MinBandwidthTestBufferSize) 
	 * to calculate results.
	 *
	 * @param ClientConn the client connection that is sending packets for the test
	 */
	void FinishUpstreamTest(FClientMeshBeaconConnection& ClientConn);
	
	/**
	 * Sends a request to client to start a new bandwidth test.
	 *
	 * @param TestType EMeshBeaconBandwidthTestType type of bandwidth test to request
	 * @param TestBufferSize size of buffer to use for the test
	 * @param ClientConn the client connection with socket to send the response on
	 */
	void SendBandwidthTestStartRequest(BYTE TestType,INT TestBufferSize,FClientMeshBeaconConnection& ClientConn);

	/**
	 * Sends the results of a completed bandwidth test to the client.
	 *
	 * @param TestResult result of the bandwidth test
	 * @param ClientConn the client connection with socket to send the response on
	 */
	void SendBandwidthTestCompletedResponse(EMeshBeaconBandwidthTestResult TestResult,FClientMeshBeaconConnection& ClientConn);

	/**
	 * The client has create a new game session and has sent the session results back.
	 *
	 * @param FromBuffer the packet serializer to read from
	 * @param ClientConn the client connection that sent the packet
	 */
	void ProcessClientCreateNewSessionResponse(FNboSerializeFromBuffer& FromBuffer,FClientMeshBeaconConnection& ClientConn);
}

/**
 * Creates a listening host mesh beacon to accept new client connections.
 *
 * @param InOwningPlayerId Net Id of player that is hosting this beacon
 * @return true if the beacon was created successfully, false otherwise
 */
native function bool InitHostBeacon(UniqueNetId InOwningPlayerId);

/**
 * Stops listening for clients and releases any allocated memory
 */
native event DestroyBeacon();

/**
 * Send a request to a client connection to initiate a new bandwidth test.
 *
 * @param PlayerNetId player with an active connection to receive test request
 * @param TestType EMeshBeaconBandwidthTestType type of bandwidth test to request
 * @param TestBufferSize size of buffer in bytes to use for running the test
 * @return TRUE if the request was successfully sent to the client
 */
native function bool RequestClientBandwidthTest(UniqueNetId PlayerNetId,EMeshBeaconBandwidthTestType TestType,int TestBufferSize);

/**
 * Determine if a client is currently running a bandwidth test.
 *
 * @return TRUE if a client connection is currently running a bandwidth test
 */
native function bool HasInProgressBandwidthTest();

/**
 * Cancel any bandwidth tests that are already in progress.
 */
native function CancelInProgressBandwidthTests();

/**
 * Determine if a client is currently waiting/pending for a bandwidth test.
 *
 * @return TRUE if a client connection is currently pending a bandwidth test
 */
native function bool HasPendingBandwidthTest();

/**
 * Cancel any bandwidth tests that are pending.
 */
native function CancelPendingBandwidthTests();

/**
 * Enable/disable future bandwidth test requests and current pending tests.
 *
 * @param bEnabled true to allow bandwidth testing to be processed by the beacon
 */
function AllowBandwidthTesting(bool bEnabled)
{
	bAllowBandwidthTesting = bEnabled;
}

/**
 * Delegate called by the host mesh beacon after establishing a new client socket and
 * receiving the data for a new connection request.
 *
 * @param NewClientConnection client that sent the request for a new connection
 */
delegate OnReceivedClientConnectionRequest(const out ClientMeshBeaconConnection NewClientConnection);

/**
 * Delegate called by the host mesh beacon when bandwidth testing has started for a client connection.
 * This occurs only when the client sends the start packet to initiate the test.
 *
 * @param PlayerNetId net id for player of client connection that started the test
 * @param TestType test to run based on enum of EMeshBeaconBandwidthTestType supported bandwidth test types
 */
delegate OnStartedBandwidthTest(UniqueNetId PlayerNetId,EMeshBeaconBandwidthTestType TestType);

/**
 * Delegate called by the host mesh beacon when bandwidth testing has completed for a client connection.
 * This occurs when the test completes successfully or due to error/timeout.
 *
 * @param PlayerNetId net id for player of client connection that finished the test
 * @param TestType test that completed based on enum of EMeshBeaconBandwidthTestType supported bandwidth test types
 * @param TestResult overall result from running the test
 * @param BandwidthStats statistics and timing information from running the test
 */
delegate OnFinishedBandwidthTest(
	 UniqueNetId PlayerNetId,
	 EMeshBeaconBandwidthTestType TestType,
	 EMeshBeaconBandwidthTestResult TestResult,
	 const out ConnectionBandwidthStats BandwidthStats);

/**
 * Set list of pending player ids we are waiting to connect with.
 * Once all connections are established then the OnAllPendingPlayersConnected delegate is called.
 *
 * @param Players list of player ids we are waiting to connect
 */
function SetPendingPlayerConnections(const out array<UniqueNetId> Players)
{
	PendingPlayerConnections = Players;
}

/**
 * Determine if the given player has an active connection on this host beacon.
 *
 * @param PlayerNetId player we are searching for
 * @return index within ClientConnections for the player's connection, -1 if not found
 */
native function int GetConnectionIndexForPlayer(UniqueNetId PlayerNetId);

/**
 * Determine if the players all have connections on this host beacon
 *
 * @param Players list of player ids we are searching for
 * @return TRUE if all players had connections
 */
native function bool AllPlayersConnected(const out array<UniqueNetId> Players);

/**
 * Delegate called by the host mesh beacon when all players in the PendingPlayerConnections list get connections.
 */
delegate OnAllPendingPlayersConnected();

/**
 * Tells all of the clients to go to a specific session (contained in platform
 * specific info). Used to route all clients to one destination.
 *
 * @param SessionName the name of the session to register
 * @param SearchClass the search that should be populated with the session
 * @param PlatformSpecificInfo the binary data to place in the platform specific areas
 */
native function TellClientsToTravel(name SessionName,class<OnlineGameSearch> SearchClass,const out byte PlatformSpecificInfo[80]);

/**
 * Sends a request to a specified client to create a new game session.
 *
 * @param PlayerNetId net id of player for client connection to send request to
 * @param SessionName the name of the session to create
 * @param SearchClass the search that should be with corresponding game settings when creating the session
 * @param Players list of players to register on the newly created session
 */
native function bool RequestClientCreateNewSession(UniqueNetId PlayerNetId,name SessionName,class<OnlineGameSearch> SearchClass,const out array<PlayerMember> Players);

/**
 * Delegate called by the host mesh beacon when it gets the results of a new game session creation on a client.
 *
 * @param bSucceeded TRUE if the the new session was created on the client
 * @param SessionName the name of the session to create
 * @param SearchClass the search that should be with corresponding game settings when creating the session
 * @param PlatformSpecificInfo the platform specific binary data of the new session
 */
delegate OnReceivedClientCreateNewSessionResult(bool bSucceeded,name SessionName,class<OnlineGameSearch> SearchClass,const out byte PlatformSpecificInfo[80]);

`if(`notdefined(FINAL_RELEASE))
/**
 * Logs the all the connected clients of this this beacon
 */
function DumpConnections()
{
	local int ClientIdx, HistoryIdx;
	local UniqueNetId NetId;

	`Log("Debug info for Beacon: "$BeaconName,,'DevBeacon');
	for (ClientIdx=0; ClientIdx < ClientConnections.Length; ClientIdx++)
	{
		NetId = ClientConnections[ClientIdx].PlayerNetId;
		`Log("",,'DevBeacon');
		`Log("Client connection entry: "$ClientIdx,,'DevBeacon');
		`Log("	PlayerNetId: "$class'OnlineSubsystem'.static.UniqueNetIdToString(NetId),,'DevBeacon');
		`Log("	NatType: "$ClientConnections[ClientIdx].NatType,,'DevBeacon');
		`Log("	GoodHostRatio: "$ClientConnections[ClientIdx].GoodHostRatio,,'DevBeacon');
		`Log("	bCanHostVs: "$ClientConnections[ClientIdx].bCanHostVs,,'DevBeacon');		
		`Log("	MinutesSinceLastTest: "$ClientConnections[ClientIdx].MinutesSinceLastTest,,'DevBeacon');		
		`Log("	BandwidthTest.CurrentState: "$ClientConnections[ClientIdx].BandwidthTest.CurrentState,,'DevBeacon');
		`Log("	BandwidthTest.TestType: "$ClientConnections[ClientIdx].BandwidthTest.TestType,,'DevBeacon');
		`Log("	Bandwidth History: "$ClientConnections[ClientIdx].BandwidthHistory.Length,,'DevBeacon');
		for (HistoryIdx=0; HistoryIdx < ClientConnections[ClientIdx].BandwidthHistory.Length; HistoryIdx++)
		{	
			`Log("		"
				$" Upstream bytes/sec: "$ClientConnections[ClientIdx].BandwidthHistory[HistoryIdx].UpstreamRate
				$" Downstream bytes/sec: "$ClientConnections[ClientIdx].BandwidthHistory[HistoryIdx].DownstreamRate
				$" Roundrtrip msec: "$ClientConnections[ClientIdx].BandwidthHistory[HistoryIdx].RoundtripLatency,,'DevBeacon');
		}
	}
	`Log("");
}

/**
 * Render debug info about the client mesh beacon
 *
 * @param Canvas canvas object to use for rendering debug info
 * @param CurOptimalHostId net id of player that should be highlighted as the current optimal host
 */
function DebugRender(Canvas Canvas, UniqueNetId CurOptimalHostId)
{
	local int ClientIdx,HistoryIdx;
	local UniqueNetId NetId;
	local float XL,YL;
	local float Offset;

	Offset = 50;

	Canvas.Font = class'Engine'.Static.GetTinyFont();
	Canvas.StrLen("============================================================",XL,YL);
	YL = Canvas.SizeY-(Offset*2);

	Canvas.SetPos(Offset,Offset);
	Canvas.SetDrawColor(0,0,255,64);
	Canvas.DrawTile(Canvas.DefaultTexture,XL,YL,0,0,1,1);
	
	Canvas.SetPos(Offset,Offset);
	Canvas.SetDrawColor(255,255,255);
	Canvas.DrawText("Debug info for Beacon:"$BeaconName);

	if (CurOptimalHostId == OwningPlayerId)
	{
		Canvas.SetDrawColor(255,255,0);
	}
	Canvas.DrawText("Owning Host: "$class'OnlineSubsystem'.static.UniqueNetIdToString(OwningPlayerId));	
	
	for (ClientIdx=0; ClientIdx < ClientConnections.Length; ClientIdx++) 
	{
		Canvas.SetDrawColor(255,255,255);
		if (Canvas.CurY >= YL)
		{			
			Canvas.SetPos(Canvas.CurX+XL,Offset);
		}		
		NetId = ClientConnections[ClientIdx].PlayerNetId;
		Canvas.DrawText("============================================================");
		Canvas.DrawText("Client connection entry: "$ClientIdx);
		Canvas.SetPos(Canvas.CurX+10,Canvas.CurY);
		if (CurOptimalHostId == NetId)
		{
			Canvas.SetDrawColor(255,255,0);
		}
		Canvas.DrawText("PlayerNetId: "$class'OnlineSubsystem'.static.UniqueNetIdToString(NetId));
		Canvas.SetDrawColor(255,255,255);
		Canvas.DrawText("NatType: "$ClientConnections[ClientIdx].NatType);
		Canvas.DrawText("GoodHostRatio: "$ClientConnections[ClientIdx].GoodHostRatio);
		Canvas.DrawText("bCanHostVs: "$ClientConnections[ClientIdx].bCanHostVs);
		Canvas.DrawText("MinutesSinceLastTest: "$ClientConnections[ClientIdx].MinutesSinceLastTest);		
		Canvas.DrawText("Current BandwidthTest: ");
		Canvas.SetPos(Canvas.CurX+10,Canvas.CurY);
		Canvas.DrawText("CurrentState: "$ClientConnections[ClientIdx].BandwidthTest.CurrentState);
		Canvas.DrawText("TestType: "$ClientConnections[ClientIdx].BandwidthTest.TestType);
		Canvas.DrawText("BytesTotalNeeded: "$ClientConnections[ClientIdx].BandwidthTest.BytesTotalNeeded);
		Canvas.DrawText("BytesReceived: "$ClientConnections[ClientIdx].BandwidthTest.BytesReceived);
		Canvas.DrawText("UpstreamRate bytes/sec: "$ClientConnections[ClientIdx].BandwidthTest.BandwidthStats.UpstreamRate);
		Canvas.SetPos(Canvas.CurX-10,Canvas.CurY);		
		Canvas.DrawText("Bandwidth History: "$ClientConnections[ClientIdx].BandwidthHistory.Length);
		Canvas.SetPos(Canvas.CurX+10,Canvas.CurY);
		for (HistoryIdx=0; HistoryIdx < ClientConnections[ClientIdx].BandwidthHistory.Length; HistoryIdx++)
		{	
			Canvas.DrawText("Upstream bytes/sec: "$ClientConnections[ClientIdx].BandwidthHistory[HistoryIdx].UpstreamRate);
		}
		Canvas.SetPos(Canvas.CurX-20,Canvas.CurY);
	}
}

`endif

defaultproperties
{
	bAllowBandwidthTesting=true
}