/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class is the base class for the client/host beacon classes.
 */
class PartyBeacon extends Object
	native
	inherits(FTickableObject)
	config(Engine);

/** The port that the party beacon will listen on */
var config int PartyBeaconPort;

/** The object that is used to send/receive data with the remote host/client */
var native transient pointer Socket{FSocket};

/** The types of packets being sent */
enum EReservationPacketType
{
	RPT_UnknownPacketType,
	// Sent by a client that wants to reserve space
	RPT_ClientReservationRequest,
	// Sent by a client that wants to update an existing party reservation
	RPT_ClientReservationUpdateRequest,
	// Sent by a client that is backing out of its request
	RPT_ClientCancellationRequest,
	// Sent by the host telling the client whether the reservation was accepted or not
	RPT_HostReservationResponse,
	// Sent by the host telling all clients the current reservation count
	RPT_HostReservationCountUpdate,
	// Sent by the host telling all clients to travel to a specified destination
	RPT_HostTravelRequest,
	// When doing full party matching, tells the clients the host is ready
	RPT_HostIsReady,
	// Tells the clients that the host has canceled this matching session
	RPT_HostHasCancelled,
	// Sent periodically to tell the host/client that the other end is there
	RPT_Heartbeat
};

/** The result code that will be returned during party reservation */
enum EPartyReservationResult
{
	// An unknown error happened
	PRR_GeneralError,
	// All available reservations are booked
	PRR_PartyLimitReached,
	// Wrong number of players to join the session
	PRR_IncorrectPlayerCount,
	// No response from the host
	PRR_RequestTimedOut,
	// Already have a reservation entry for the requesting party leader
	PRR_ReservationDuplicate,
	// Couldn't find the party leader specified for a reservation update request 
	PRR_ReservationNotFound,
	// Space was available and it's time to join
	PRR_ReservationAccepted,
	// The beacon is paused and not accepting new connections
	PRR_ReservationDenied
};

/** A player that has/is requesting a reservation */
struct native PlayerReservation
{
	/** The unique identifier for this player */
	var UniqueNetId NetId;
	/** The skill of the player */
	var int Skill;
	/** The player experience level */
	var int XpLevel;
	/** The raw skill value */
	var double Mu;
	/** The uncertainty of that raw skill value */
	var double Sigma;
	/** Seconds since we checked to see if the player reservation exists in the session */
	var float ElapsedSessionTime;

	structcpptext
	{
		/** Constructors */
		FPlayerReservation() {}
		FPlayerReservation(EEventParm)
		{
			appMemzero(this, sizeof(FPlayerReservation));
		}
		/**
		 * Serialize from NBO buffer to FPlayerReservation
		 */
		friend FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FPlayerReservation& PlayerRes);
		/**
		 * Serialize from FPlayerReservation to NBO buffer
		 */
		friend FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FPlayerReservation& PlayerRes);
	}
};

/** Holds information about a party that has reserved space in the session */
struct native PartyReservation
{
	/** The team this party was assigned to */
	var int TeamNum;
	/** The party leader that has requested a reservation */
	var UniqueNetId PartyLeader;
	/** The list of members of the party (includes the party leader) */
	var array<PlayerReservation> PartyMembers;
};

/** Used to determine whether to use deferred destruction or not */
var bool bIsInTick;

/** True if the beacon should be destroyed at the end of the tick */
var bool bWantsDeferredDestroy;

/** The maximum amount of time to pass between heartbeat packets being sent */
var config float HeartbeatTimeout;

/** The elapsed time that has passed since the last heartbeat */
var float ElapsedHeartbeatTime;

/** Whether to the socket(s) or not (not during travel) */
var bool bShouldTick;

/** The name to use when logging (helps debugging) */
var name BeaconName;

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
	 * Converts a host response code to a readable string
	 *
	 * @param Result the code to translate
	 *
	 * @return the string that maps to it
	 */
	inline const TCHAR* PartyReservationResultToString(EPartyReservationResult Result)
	{
		switch (Result)
		{
			case PRR_PartyLimitReached: return TEXT("PRR_PartyLimitReached");
			case PRR_IncorrectPlayerCount: return TEXT("PRR_IncorrectPlayerCount");
			case PRR_RequestTimedOut: return TEXT("PRR_RequestTimedOut");
			case PRR_ReservationDuplicate: return TEXT("PRR_ReservationDuplicate");
			case PRR_ReservationNotFound: return TEXT("PRR_ReservationNotFound");
			case PRR_ReservationAccepted: return TEXT("PRR_ReservationAccepted");
			case PRR_ReservationDenied: return TEXT("PRR_ReservationDenied");
		}
		return TEXT("PRR_GeneralError");
	}

	/**
	 * Sends a heartbeat packet to the specified socket
	 *
	 * @param Socket the socket to send the data on
	 *
	 * @return TRUE if it sent ok, FALSE if there was an error
	 */
	UBOOL SendHeartbeat(FSocket* Socket);

	/**
	 * @return the max value for the packet types handled by this beacon 
	 */
	virtual BYTE GetMaxPacketValue()
	{
		return RPT_MAX;
	}
}

/**
 * Stops listening for requests/responses and releases any allocated memory
 */
native event DestroyBeacon();

/**
 * Called when the beacon has completed destroying its socket
 */
delegate OnDestroyComplete();

defaultproperties
{
	bShouldTick=true
}
