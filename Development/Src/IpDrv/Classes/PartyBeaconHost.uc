/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class is used to create a network accessible beacon for responding
 * to reservation requests for party matches. It handles all tracking of
 * who has reserved space, how much space is available, and how many parties
 * can reserve space.
 */
class PartyBeaconHost extends PartyBeacon
	native;

/** Holds the information for a client and whether they've timed out */
struct native ClientBeaconConnection
{
	/** The unique id of the party leader for this connection */
	var UniqueNetId PartyLeader;
	/** How long it's been since the last heartbeat */
	var float ElapsedHeartbeatTime;
	/** The socket this client is communicating on */
	var native transient pointer Socket{FSocket};
};

/** The object that is used to send/receive data with the remote host/client */
var const array<ClientBeaconConnection> Clients;

/** The number of teams that these players will be divided amongst */
var const int NumTeams;

/** The number of players required for a full team */
var const int NumPlayersPerTeam;

/** The number of players that are allowed to reserve space */
var const int NumReservations;

/** The number of slots that have been consumed by parties in total (saves having to iterate and sum) */
var const int NumConsumedReservations;

/** The list of accepted reservations */
var const array<PartyReservation> Reservations;

/** The online session name that players will register with */
var name OnlineSessionName;

/** The number of connections to allow before refusing them */
var config int ConnectionBacklog;

/** Team to force all reservations to in single team situations */
var const int ForceTeamNum;

/** The team the host (owner of the beacon) is assigned to when random teams are chosen */
var const int ReservedHostTeamNum;

/** Force new reservations to teams with the smallest accommodating need size, otherwise team assignment is random */
var bool bBestFitTeamAssignment;

enum EPartyBeaconHostState
{
	/** New reservations are accepted if possible */
	PBHS_AllowReservations,
	/** New reservations are denied */
	PBHS_DenyReservations
};
/** Current state of the beacon wrt allowing reservation requests */ 
var const EPartyBeaconHostState BeaconState;

cpptext
{
	/**
	 * Ticks the network layer to see if there are any requests or responses to requests
	 *
	 * @param DeltaTime the amount of time that has elapsed since the last tick
	 */
	virtual void Tick(FLOAT DeltaTime);

	/** Accepts any pending connections and adds them to our queue */
	void AcceptConnections(void);

	/**
	 * Reads the socket and processes any data from it
	 *
	 * @param ClientConn the client connection that sent the packet
	 *
	 * @return TRUE if the socket is ok, FALSE if it is in error
	 */
	UBOOL ReadClientData(FClientBeaconConnection& ClientConn);

	/**
	 * Processes a packet that was received from a client
	 *
	 * @param Packet the packet that the client sent
	 * @param PacketSize the size of the packet to process
	 * @param ClientConn the client connection that sent the packet
	 */
	void ProcessRequest(BYTE* Packet,INT PacketSize,FClientBeaconConnection& ClientConn);

	/**
	 * Routes the packet received from a client to the correct handler based on its type.
	 * Overridden by base implementations to handle custom data packet types
	 *
	 * @param RequestPacketType packet ID from EReservationPacketType (or derived version) that represents a client request
	 * @param FromBuffer the packet serializer to read from
	 * @param ClientConn the client connection that sent the packet
	 * @return TRUE if the requested packet type was processed
	 */
	virtual UBOOL HandleClientRequestPacketType(BYTE RequestPacketType,FNboSerializeFromBuffer& FromBuffer,FClientBeaconConnection& ClientConn);

	/**
	 * Processes a reservation packet that was received from a client
	 *
	 * @param FromBuffer the packet serializer to read from
	 * @param ClientConn the client connection that sent the packet
	 */
	virtual void ProcessReservationRequest(FNboSerializeFromBuffer& FromBuffer,FClientBeaconConnection& ClientConn);

	/**
	 * Processes a reservation update packet that was received from a client
	 * The update finds an existing reservation based on the party leader and
	 * then tries to add new players to it (can't remove). Then sends a response to
	 * the client based on the update results (see EPartyReservationResult).
	 *
	 * @param FromBuffer the packet serializer to read from
	 * @param ClientConn the client connection that sent the packet
	 */
	virtual void ProcessReservationUpdateRequest(FNboSerializeFromBuffer& FromBuffer,FClientBeaconConnection& ClientConn);

	/**
	 * Processes a cancellation packet that was received from a client
	 *
	 * @param FromBuffer the packet serializer to read from
	 * @param ClientConn the client connection that sent the packet
	 */
	virtual void ProcessCancellationRequest(FNboSerializeFromBuffer& FromBuffer,FClientBeaconConnection& ClientConn);

	/**
	 * Sends a client the specified response code
	 *
	 * @param Result the result being sent to the client
	 * @param ClientSocket the client socket to send the response on
	 */
	void SendReservationResponse(EPartyReservationResult Result,FSocket* ClientSocket);

	/**
	 * Tells clients that a reservation update has occured and sends them the current
	 * number of remaining reservations so they can update their UI
	 */
	void SendReservationUpdates(void);

	/**
	 * Initializes the team array so that random choices can be made from it
	 * Also initializes the host's team number (random from range)
	 */
	void InitTeamArray(void);

	/** 
	 * Determine if there are any teams that can fit the current party request.
	 * 
	 * @param PartySize number of players in the party making a reservation request
	 * @return TRUE if there are teams available, FALSE otherwise 
	 */
	virtual UBOOL AreTeamsAvailable(INT PartySize);

	/**
	 * Determine the team number for the given party reservation request.
	 * Uses the list of current reservations to determine what teams have open slots.
	 *
	 * @param PartyRequest the party reservation request received from the client beacon
	 * @return index of the team to assign to all members of this party
	 */
	virtual INT GetTeamAssignment(const FPartyReservation& PartyRequest);

	/**
	 * Readjust existing team assignments for party reservations in order to open the max
	 * available slots possible.
	 */
	void BestFitTeamAssignmentJiggle();

	/**
	 * Determine the number of players on a given team based on current reservation entries
	 *
	 * @param TeamIdx index of team to search for
	 * @return number of players on a given team
	 */
	INT GetNumPlayersOnTeam(INT TeamIdx) const;

	/**
	 * Find an existing player entry in a party reservation 
	 *
	 * @param ExistingReservation party reservation entry
	 * @param PlayerMember member to search for in the existing party reservation
	 * @return index of party member in the given reservation, -1 if not found
	 */
	INT GetReservationPlayerMember(const FPartyReservation& ExistingReservation, const FUniqueNetId& PlayerMember) const;

	/**
	 * Removes the specified party leader (and party) from the arrays and notifies
	 * any connected clients of the change in status
	 *
	 * @param PartyLeader the leader of the party to remove
	 * @param ClientConn the client connection that sent the packet
	 */
	void CancelPartyReservation(FUniqueNetId& PartyLeader,FClientBeaconConnection& ClientConn);
	
	/**
	 * Determine if the reservation entry for a disconnected client should be removed.
	 *
	 * @param ClientConn the client connection that is disconnecting
	 * @return TRUE if the reservation for the disconnected client should be removed
	 */
	virtual UBOOL ShouldCancelReservationOnDisconnect(const FClientBeaconConnection& ClientConn);

	/**
	 * Called whenever a new player is added to a reservation on this beacon
	 *
	 * @param PlayerRes player reservation entry that was added
	 */
	virtual void NewPlayerAdded(const FPlayerReservation& PlayerRes);
}

/**
 * Pauses new reservation request on the beacon
 *
 * @param bPause if true then new reservation requests are denied
 */
native function PauseReservationRequests(bool bPause);

/**
 * Creates a listening host beacon with the specified number of parties, players, and
 * the session name that remote parties will be registered under
 *
 * @param InNumTeams the number of teams that are expected to join
 * @param InNumPlayersPerTeam the number of players that are allowed to be on each team
 * @param InNumReservations the total number of players to allow to join (if different than team * players)
 * @param InSessionName the name of the session to add the players to when a reservation occurs
 * @param InForceTeamNum the team to force to (only single team situations)
 *
 * @return true if the beacon was created successfully, false otherwise
 */
native function bool InitHostBeacon(int InNumTeams,int InNumPlayersPerTeam,int InNumReservations,name InSessionName, optional int InForceTeamNum=0);

/**
 * Add a new party to the reservation list.
 * Avoids adding duplicate entries based on party leader.
 *
 * @param PartyLeader the party leader that is adding the reservation
 * @param PlayerMembers players (including party leader) being added to the reservation
 * @param TeamNum team assignment of the new party
 * @param bIsHost treat the party as the game host
 * @return EPartyReservationResult similar to a client update request
 */
native function EPartyReservationResult AddPartyReservationEntry(UniqueNetId PartyLeader, const out array<PlayerReservation> PlayerMembers, int TeamNum, bool bIsHost);

/**
 * Update a party with an existing reservation
 * Avoids adding duplicate entries to the player members of a party.
 *
 * @param PartyLeader the party leader for which the existing reservation entry is being updated
 * @param PlayerMembers players (not including party leader) being added to the existing reservation
 * @return EPartyReservationResult similar to a client update request
 */
native function EPartyReservationResult UpdatePartyReservationEntry(UniqueNetId PartyLeader, const out array<PlayerReservation> PlayerMembers);

/**
 * Find an existing reservation for the party leader
 *
 * @param PartyLeader the party leader to find a reservation entry for
 * @return index of party leader in list of reservations, -1 if not found
 */
native function int GetExistingReservation(const out UniqueNetId PartyLeader); 

/**
 * Called when a player logs out of the current game.  The player's 
 * party reservation entry is freed up so that a new reservation request
 * can be accepted.
 *
 * @param PlayerId the net Id of the player that just logged out
 * @param bMaintainParty if TRUE then preserve party members of a reservation when the party leader logs out
 */
native function HandlePlayerLogout(UniqueNetId PlayerId, bool bMaintainParty);

/**
 * Called by the beacon when a reservation occurs or is cancelled so that UI can be updated, etc.
 */
delegate OnReservationChange();

/**
 * Called by the beacon when all of the available reservations have been filled
 */
delegate OnReservationsFull();

/**
 * Called by the beacon when a client cancels a reservation
 *
 * @param PartyLeader the party leader that is cancelling the reservation
 */
delegate OnClientCancellationReceived(UniqueNetId PartyLeader);

/**
 * Stops listening for clients and releases any allocated memory
 */
native event DestroyBeacon();

/**
 * Tells all of the clients to go to a specific session (contained in platform
 * specific info). Used to route clients that aren't in the same party to one destination.
 *
 * @param SessionName the name of the session to register
 * @param SearchClass the search that should be populated with the session
 * @param PlatformSpecificInfo the binary data to place in the platform specific areas
 */
native function TellClientsToTravel(name SessionName,class<OnlineGameSearch> SearchClass,byte PlatformSpecificInfo[80]);

/**
 * Tells all of the clients that the host is ready for them to travel to the host connection
 */
native function TellClientsHostIsReady();

/**
 * Tells all of the clients that the host has cancelled the matchmaking beacon and that they
 * need to find a different host
 */
native function TellClientsHostHasCancelled();

/**
 * Determine if the beacon has filled all open reservation slots
 *
 * @return TRUE if all reservations have been consumed by party members
 */
function bool AreReservationsFull()
{
	return NumConsumedReservations == NumReservations;
}

/**
 * Registers all of the parties as part of the session that this beacon is associated with
 */
event RegisterPartyMembers()
{
	local int Index;
	local int PartyIndex;
	local OnlineSubsystem OnlineSub;
	local OnlineRecentPlayersList PlayersList;
	local array<UniqueNetId> Members;
	local PlayerReservation PlayerRes;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None &&
		OnlineSub.GameInterface != None)
	{
		// Iterate through the parties adding the players from each to the session
		for (PartyIndex = 0; PartyIndex < Reservations.Length; PartyIndex++)
		{
			for (Index = 0; Index < Reservations[PartyIndex].PartyMembers.Length; Index++)
			{
				PlayerRes = Reservations[PartyIndex].PartyMembers[Index];
				OnlineSub.GameInterface.RegisterPlayer(OnlineSessionName,PlayerRes.NetId,false);
				Members.AddItem(PlayerRes.NetId);
			}
			// Add the remote party members to the recent players list if available
			PlayersList = OnlineRecentPlayersList(OnlineSub.GetNamedInterface('RecentPlayersList'));
			if (PlayersList != None)
			{
				PlayersList.AddPartyToRecentParties(Reservations[PartyIndex].PartyLeader,Members);
			}
		}
	}
}

/**
 * Unregisters each of the party members at the specified reservation with the session
 */
event UnregisterPartyMembers()
{
	local int Index;
	local int PartyIndex;
	local OnlineSubsystem OnlineSub;
	local PlayerReservation PlayerRes;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None &&
		OnlineSub.GameInterface != None)
	{
		// Iterate through the parties removing the players from each from the session
		for (PartyIndex = 0; PartyIndex < Reservations.Length; PartyIndex++)
		{
			for (Index = 0; Index < Reservations[PartyIndex].PartyMembers.Length; Index++)
			{
				PlayerRes = Reservations[PartyIndex].PartyMembers[Index];
				OnlineSub.GameInterface.UnregisterPlayer(OnlineSessionName,PlayerRes.NetId);
			}
		}
	}
}

/**
 * Unregisters each of the party members that have the specified party leader
 *
 * @param PartyLeader the leader to search for in the reservation array
 */
event UnregisterParty(UniqueNetId PartyLeader)
{
	local int PlayerIndex;
	local int PartyIndex;
	local OnlineSubsystem OnlineSub;
	local PlayerReservation PlayerRes;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None &&
		OnlineSub.GameInterface != None)
	{
		// Iterate through the parties removing the players from each from the session
		for (PartyIndex = 0; PartyIndex < Reservations.Length; PartyIndex++)
		{
			// If this is the reservation in question, remove the members from the session
			if (Reservations[PartyIndex].PartyLeader == PartyLeader)
			{
				for (PlayerIndex = 0; PlayerIndex < Reservations[PartyIndex].PartyMembers.Length; PlayerIndex++)
				{
					PlayerRes = Reservations[PartyIndex].PartyMembers[PlayerIndex];
					OnlineSub.GameInterface.UnregisterPlayer(OnlineSessionName,PlayerRes.NetId);
				}
			}
		}
	}
}

/**
 * Appends the skills from all reservations to the search object so that they can
 * be included in the search information
 *
 * @param Search the search object to update
 */
native function AppendReservationSkillsToSearch(OnlineGameSearch Search);

/**
 * Gathers all the unique ids for players that have reservations
 */
function GetPlayers(out array<UniqueNetId> Players)
{
	local int PlayerIndex;
	local int PartyIndex;
	local PlayerReservation PlayerRes;

	// Iterate through the parties adding the players from each to the out array
	for (PartyIndex = 0; PartyIndex < Reservations.Length; PartyIndex++)
	{
		for (PlayerIndex = 0; PlayerIndex < Reservations[PartyIndex].PartyMembers.Length; PlayerIndex++)
		{
			PlayerRes = Reservations[PartyIndex].PartyMembers[PlayerIndex];
			Players.AddItem(PlayerRes.NetId);
		}
	}
}

/**
 * Gathers all the unique ids for party leaders that have reservations
 */
function GetPartyLeaders(out array<UniqueNetId> PartyLeaders)
{
	local int PartyIndex;

	// Iterate through the parties adding the players from each to the out array
	for (PartyIndex = 0; PartyIndex < Reservations.Length; PartyIndex++)
	{
		PartyLeaders.AddItem(Reservations[PartyIndex].PartyLeader);
	}
}

/**
 * Determine the maximum team size that can be accommodated based 
 * on the current reservation slots occupied.
 *
 * @return maximum team size that is currently available
 */
native function int GetMaxAvailableTeamSize();

`if(`notdefined(FINAL_RELEASE))
/**
 * Logs the reservation information for this beacon
 */
function DumpReservations()
{
	local int PartyIndex;
	local int MemberIndex;
	local UniqueNetId NetId;
	local PlayerReservation PlayerRes;

	`Log("Debug info for Beacon: "$BeaconName,,'DevBeacon');
	`Log("Session that reservations are for: "$OnlineSessionName,,'DevBeacon');
	`Log("Number of teams: "$NumTeams,,'DevBeacon');
	`Log("Number players per team: "$NumPlayersPerTeam,,'DevBeacon');
	`Log("Number total reservations: "$NumReservations,,'DevBeacon');
	`Log("Number consumed reservations: "$NumConsumedReservations,,'DevBeacon');
	`Log("Number of party reservations: "$Reservations.Length,,'DevBeacon');
	`Log("Reserved host team: "$ReservedHostTeamNum,,'DevBeacon');
	// Log each party that has a reservation
	for (PartyIndex = 0; PartyIndex < Reservations.Length; PartyIndex++)
	{
		NetId = Reservations[PartyIndex].PartyLeader;
		`Log("  Party leader: "$class'OnlineSubsystem'.static.UniqueNetIdToString(NetId),,'DevBeacon');
		`Log("  Party team: "$Reservations[PartyIndex].TeamNum,,'DevBeacon');
		`Log("  Party size: "$Reservations[PartyIndex].PartyMembers.Length,,'DevBeacon');
		// Log each member of the party
		for (MemberIndex = 0; MemberIndex < Reservations[PartyIndex].PartyMembers.Length; MemberIndex++)
		{
			PlayerRes = Reservations[PartyIndex].PartyMembers[MemberIndex];
			`Log("  Party member: "$class'OnlineSubsystem'.static.UniqueNetIdToString(PlayerRes.NetId)
				$" skill: "$PlayerRes.Skill
				$" xp: "$PlayerRes.XpLevel,,'DevBeacon');
		}
	}
	`Log("",,'DevBeacon');
}
`endif