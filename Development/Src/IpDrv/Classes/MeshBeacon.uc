/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class is the base class for the client/host mesh beacon classes.
 */
class MeshBeacon extends Object
	native
	inherits(FTickableObject)
	config(Engine);

/** Packet ids used to communicate between host & client mesh beacons */
enum EMeshBeaconPacketType
{
	// 0 packet type treated as undefined
	MB_Packet_UnknownType,
	// Sent by a client when a new connection is established and to send history from past bandwidth tests
	MB_Packet_ClientNewConnectionRequest,
	// Sent by a client when starting a bandwidth test to the host. Immediately followed by dummy buffer based on test size
	MB_Packet_ClientBeginBandwidthTest,
	// Sent by a client in response to a host request to create a new session
	MB_Packet_ClientCreateNewSessionResponse,
	// Sent by the host to acknowledge it received the connection request with bandwidth history from a client
	MB_Packet_HostNewConnectionResponse,
	// Sent by the host to a client to request a new bandwidth test
	MB_Packet_HostBandwidthTestRequest,
	// Sent by the host to a client that had initiated the bandwidth test, followed by the results of the test
	MB_Packet_HostCompletedBandwidthTest,
	// Sent by the host to all clients to initiate travel to a new session
	MB_Packet_HostTravelRequest,
	// Sent by the host to a client to request a new session to be created on the client.  A list of players is sent to register with the new session.
	MB_Packet_HostCreateNewSessionRequest,
	// Used to flag dummy buffers sent for bandwidth testing
	MB_Packet_DummyData,
	// Sent periodically to tell the host/client that the other end is there
	MB_Packet_Heartbeat
};

/** Result of a new client connection request */
enum EMeshBeaconConnectionResult
{
	// Client was able to connect successfully
	MB_ConnectionResult_Succeeded,
	// Client already has a connection, duplicate
	MB_ConnectionResult_Duplicate,
	// Client connection request failed due to timeout
	MB_ConnectionResult_Timeout,
	// Client connection request failed due to socket error
	MB_ConnectionResult_Error
};

/** State of current bandwidth testing on a client connection */
enum EMeshBeaconBandwidthTestState
{
	// Bandwidth test for a connection has not been started/completed yet
	MB_BandwidthTestState_NotStarted,
	// Bandwidth test has been requested for the connection but the start request hasn't been to the client yet
	MB_BandwidthTestState_RequestPending,
	// Start request has been sent to client from host but test hasn't been started by client yet
	MB_BandwidthTestState_StartPending,
	// Test has been started for client and is currently in progress
	MB_BandwidthTestState_InProgress,
	// Test has completed for the client successfully
	MB_BandwidthTestState_Completed,
	// Test never completely finished, but didn't error either. Bandwidth results based on incomplete data
	MB_BandwidthTestState_Incomplete,
	// Test never finished due to timeout waiting for client.
	MB_BandwidthTestState_Timeout,
	// Test was started but never completed due to error
	MB_BandwidthTestState_Error
};

/** Result of a bandwidth test between host/clietn connection */
enum EMeshBeaconBandwidthTestResult
{
	// Test has completed for the client successfully
	MB_BandwidthTestResult_Succeeded,
	// Test never finished due to timeout waiting for client.
	MB_BandwidthTestResult_Timeout,
	// Test was started but never completed due to error
	MB_BandwidthTestResult_Error
};

/** Bandwidth tests that are supported */
enum EMeshBeaconBandwidthTestType
{
	// Test for rate at which data can be uploaded
	MB_BandwidthTestType_Upstream,
	// Test for rate at which data can be downloaded
	MB_BandwidthTestType_Downstream,
	// Test for time it takes to send/receive a packet
	MB_BandwidthTestType_RoundtripLatency,
};

/** Bandwidth data for a connection */
struct native ConnectionBandwidthStats
{
	/** Upstream rate in bytes per second */
	var int UpstreamRate;
	/** Downstream rate in bytes per second */
	var int DownstreamRate;
	/** Roundtrip latency in milliseconds */
	var int RoundtripLatency;

	structcpptext
	{

		/** Constructors */
		FConnectionBandwidthStats() {}
		FConnectionBandwidthStats(EEventParm)
		{
			appMemzero(this, sizeof(FConnectionBandwidthStats));
		}
		/**
		 * Serialize from NBO buffer to FConnectionBandwidthStats
		 */
		friend FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FConnectionBandwidthStats& BandwidthStats);
		/**
		 * Serialize from FConnectionBandwidthStats to NBO buffer
		 */
		friend FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FConnectionBandwidthStats& BandwidthStats);
	}
};

/** Player that is to be a member of a new session */
struct native PlayerMember
{
	/** The team the player is on */
	var int TeamNum;
	/** The skill rating of the player */
	var int Skill;
	/** The unique net id for the player */
	var UniqueNetId NetId;

	structcpptext
	{
		/** Constructors */
		FPlayerMember() {}
		FPlayerMember(EEventParm)
		{
			appMemzero(this, sizeof(FPlayerMember));
		}
		/**
		 * Serialize from NBO buffer to FPlayerMember
		 */
		friend FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FPlayerMember& PlayerEntry);
		/**
		 * Serialize from FPlayerMember to NBO buffer
		 */
		friend FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FPlayerMember& PlayerEntry);
	}
};

/** The port that the mesh beacon will listen on */
var config int MeshBeaconPort;

/** The object that is used to send/receive data with the remote host/client */
var native transient pointer Socket{FSocket};

/** Used to determine whether to use deferred destruction or not */
var transient bool bIsInTick;

/** The maximum amount of time to pass between heartbeat packets being sent */
var config float HeartbeatTimeout;

/** The elapsed time that has passed since the last heartbeat */
var float ElapsedHeartbeatTime;

/** True if the beacon should be destroyed at the end of the tick */
var transient bool bWantsDeferredDestroy;

/** Whether to the socket(s) or not (not during travel) */
var bool bShouldTick;

/** The name to use when logging (helps debugging) */
var name BeaconName;

/** Size of socket send buffer. Once this is filled then socket blocks on the next send. */
var config int SocketSendBufferSize;

/** Size of socket recv buffer. Once this is filled then socket blocks on the next recv. */
var config int SocketReceiveBufferSize;

/** Maximum size of data that is allowed to be sent for bandwidth testing */
var config int MaxBandwidthTestBufferSize;

/** Minimum size of data that is required to be sent for acurate bandwidth testing */
var config int MinBandwidthTestBufferSize;

/** Maximum time allowed to send the buffer for bandwidth testing */
var config float MaxBandwidthTestSendTime;

/** Maximum time allowed to receive the buffer for bandwidth testing */
var config float MaxBandwidthTestReceiveTime;

/** Maximum number of entries allowed for the bandwidth history of a client connection */
var config int MaxBandwidthHistoryEntries;

cpptext
{
	// FTickableObject interface

	/**
	 * Returns whether it is okay to tick this object. E.g. objects being loaded in the background shouldn't be ticked
	 * till they are finalized and unreachable objects cannot be ticked either.
	 *
	 * @return	TRUE if tickable, FALSE otherwise
	 */
	virtual UBOOL IsTickable() const
	{
		// We cannot tick objects that are unreachable or are in the process of being loaded in the background.
		return !HasAnyFlags( RF_Unreachable | RF_AsyncLoading );
	}

	/**
	 * Used to determine if an object should be ticked when the game is paused.
	 *
	 * @return always TRUE as networking needs to be ticked even when paused
	 */
	virtual UBOOL IsTickableWhenPaused() const
	{
		return TRUE;
	}

	/**
	 * Ticks the network layer to see if there are any requests or responses to requests
	 *
	 * @param DeltaTime the amount of time that has elapsed since the last tick
	 */
	virtual void Tick(FLOAT DeltaTime);

	/**
	 * Sends a heartbeat packet to the specified socket
	 *
	 * @param Socket the socket to send the data on
	 *
	 * @return TRUE if it sent ok, FALSE if there was an error
	 */
	UBOOL SendHeartbeat(FSocket* Socket);

	/**
	 * Handles dummy packets that are received by reading from the buffer until there is no more data or a non-dummy packet is seen.
	 *
	 * @param FromBuffer the packet serializer to read from
	 */
	void ProcessDummyPackets(FNboSerializeFromBuffer& FromBuffer);
}

/**
 * Stops listening for requests/responses and releases any allocated memory
 */
native event DestroyBeacon();

defaultproperties
{
	bShouldTick=true
}
