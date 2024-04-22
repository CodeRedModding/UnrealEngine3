/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(UPartyBeacon);
IMPLEMENT_CLASS(UPartyBeaconHost);
IMPLEMENT_CLASS(UPartyBeaconClient);

/*-----------------------------------------------------------------------------
	UPartyBeacon implementation
-----------------------------------------------------------------------------*/

/**
 * Ticks the network layer to see if there are any requests or responses to requests
 *
 * @param DeltaTime the amount of time that has elapsed since the last tick
 */
void UPartyBeacon::Tick(FLOAT DeltaTime)
{
#if WITH_UE3_NETWORKING
	// Only tick if we have a socket present
	if (Socket)
	{
		// See if we need to clean up
		if (bWantsDeferredDestroy)
		{
			eventDestroyBeacon();
		}
	}
#endif
}

/**
 * Stops listening for requests/responses and releases any allocated memory
 */
void UPartyBeacon::DestroyBeacon(void)
{
#if WITH_UE3_NETWORKING
	if (Socket)
	{
		// Don't delete if this is during a tick or we'll crash
		if (bIsInTick == FALSE)
		{
			GSocketSubsystem->DestroySocket(Socket);
			Socket = NULL;
			// Clear the deletion flag so we don't incorrectly delete
			bWantsDeferredDestroy = FALSE;
			bShouldTick = FALSE;
			debugf(NAME_DevBeacon,TEXT("Beacon (%s) destroy complete"),*BeaconName.ToString());
			// Notify that the socket destruction has completed
			delegateOnDestroyComplete();
		}
		else
		{
			bWantsDeferredDestroy = TRUE;
			debugf(NAME_DevBeacon,TEXT("Deferring beacon (%s) destroy until end of tick"),*BeaconName.ToString());
		}
	}
#endif
}


/**
 * Sends a heartbeat packet to the specified socket
 *
 * @param Socket the socket to send the data on
 *
 * @return TRUE if it sent ok, FALSE if there was an error
 */
UBOOL UPartyBeacon::SendHeartbeat(FSocket* Socket)
{
	if (Socket)
	{
		BYTE Heartbeat = RPT_Heartbeat;
		INT BytesSent;
		// Send the message indicating the party that is cancelling
		UBOOL bDidSendOk = Socket->Send(&Heartbeat,1,BytesSent);
		if (bDidSendOk == FALSE)
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) failed to send heartbeat packet with (%s)"),
				*BeaconName.ToString(),
				GSocketSubsystem->GetSocketError());
		}
		return bDidSendOk;
	}
	return FALSE;
}

/**
* Serialize from NBO buffer to FPlayerReservation
*/
FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& FromBuffer,FPlayerReservation& PlayerRes)
{
	FromBuffer >> PlayerRes.NetId
		>> PlayerRes.Skill
		>> PlayerRes.XpLevel
		>> PlayerRes.Mu
		>> PlayerRes.Sigma;

	return FromBuffer;
}

/**
* Serialize from FPlayerReservation to NBO buffer
*/
FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& ToBuffer,const FPlayerReservation& PlayerRes)
{
	ToBuffer << PlayerRes.NetId
		<< PlayerRes.Skill
		<< PlayerRes.XpLevel
		<< PlayerRes.Mu
		<< PlayerRes.Sigma;

	return ToBuffer;
}

/*-----------------------------------------------------------------------------
	UPartyBeaconHost implementation
-----------------------------------------------------------------------------*/

/**
 * Pauses new reservation request on the beacon
 *
 * @param bPause if true then new reservation requests are denied
 */
void UPartyBeaconHost::PauseReservationRequests(UBOOL bPause)
{
	if (bPause)
	{
		BeaconState = PBHS_DenyReservations;
	}
	else
	{
		BeaconState = PBHS_AllowReservations;
	}
}

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
UBOOL UPartyBeaconHost::InitHostBeacon(INT InNumTeams,INT InNumPlayersPerTeam,INT InNumReservations,FName InSessionName, INT InForceTeamNum)
{
	// initial state to allow reservation requests
	BeaconState = PBHS_AllowReservations;

#if WITH_UE3_NETWORKING
	// Make sure we allow at least one client to be queued
	ConnectionBacklog = Max(1,ConnectionBacklog);
	FInternetIpAddr ListenAddr;
	// Now the listen address
	ListenAddr.SetPort(PartyBeaconPort);
	ListenAddr.SetIp(getlocalbindaddr(*GWarn));
	// Now create and set up our TCP socket
	Socket = GSocketSubsystem->CreateStreamSocket(TEXT("host party beacon"));
	if (Socket != NULL)
	{
		// Set basic state
		Socket->SetReuseAddr();
		Socket->SetNonBlocking();
		// Bind to our listen port so we can respond to client connections
		if (Socket->Bind(ListenAddr))
		{
			// Start listening for client connections
			if (Socket->Listen(ConnectionBacklog))
			{
				// It worked so copy the settings and set our party beacon type
				NumTeams = InNumTeams;
				NumPlayersPerTeam = InNumPlayersPerTeam;
				ForceTeamNum = InForceTeamNum;
				NumReservations = InNumReservations;
				NumConsumedReservations = 0;
				OnlineSessionName = InSessionName;
				// Initialize the random teams
				InitTeamArray();
				debugf(NAME_DevBeacon,
					TEXT("Created party beacon (%s) on port (%d) for session (%s)"),
					*BeaconName.ToString(),
					PartyBeaconPort,
					*OnlineSessionName.ToString());
				return TRUE;
			}
			else
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) failed to Listen(%d) on the socket for clients"),
					*BeaconName.ToString(),
					ConnectionBacklog);
			}
		}
		else
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) failed to bind listen socket to addr (%s) for party beacon"),
				*BeaconName.ToString(),
				*ListenAddr.ToString(TRUE));
		}
	}
	else
	{
		debugf(NAME_DevBeacon,
			TEXT("Failed to create listen socket for lan beacon (%s)"),
			*BeaconName.ToString());
	}
#endif
	return FALSE;
}

/**
 * Called whenever a new player is added to a reservation on this beacon
 *
 * @param PlayerRes player reservation entry that was added
 */
void UPartyBeaconHost::NewPlayerAdded(const FPlayerReservation& PlayerRes)
{
	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) Adding Player 0x%016I64X, Skill (%d), XpLevel(%d), Mu (%f), Sigma (%f)"),
		*BeaconName.ToString(),
		(QWORD&)PlayerRes.NetId,
		PlayerRes.Skill,
		PlayerRes.XpLevel,
		PlayerRes.Mu,
		PlayerRes.Sigma);
}

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
BYTE UPartyBeaconHost::AddPartyReservationEntry(struct FUniqueNetId PartyLeader,const TArray<struct FPlayerReservation>& PlayerMembers,INT TeamNum,UBOOL bIsHost)
{
	EPartyReservationResult Result = PRR_GeneralError;
	if (bWantsDeferredDestroy)
	{
		return Result;
	}
	// See if the beacon is currently accepting reservations
	if (BeaconState == PBHS_DenyReservations)
	{
		return PRR_ReservationDenied;
	}

	// make sure an existing reservation does not exist for this party
	const INT ExistingReservationIdx = GetExistingReservation(PartyLeader);
	if (ExistingReservationIdx == INDEX_NONE)
	{
		if (NumConsumedReservations < NumReservations)
		{
			if (NumConsumedReservations + PlayerMembers.Num() <= NumReservations &&
				PlayerMembers.Num() <= NumPlayersPerTeam)
			{
				// create a new reservation entry and copy to it
				INT AddIndex = Reservations.AddZeroed();
				FPartyReservation& Reservation = Reservations(AddIndex);
				Reservation.PartyLeader = PartyLeader;
				Reservation.PartyMembers = PlayerMembers;

				// Make sure we have a valid team assignment
				if (NumTeams == 1)
				{
					TeamNum = ForceTeamNum;
				}
				else if (TeamNum == -1 || TeamNum >= NumTeams)
				{	
					TeamNum = GetTeamAssignment(Reservation);
				}

				Reservation.TeamNum = TeamNum;
				if (bIsHost)
				{
					ReservedHostTeamNum = TeamNum;
				}
				// Keep track of newly added players
				for (INT PlayerIdx=0; PlayerIdx < PlayerMembers.Num(); PlayerIdx++)
				{
					NewPlayerAdded(PlayerMembers(PlayerIdx));
				}
				// update consumed reservations
				NumConsumedReservations += Reservation.PartyMembers.Num();
				// Tell any UI and/or clients that there has been a change in the reservation state
				SendReservationUpdates();
				// Tell the owner that we've changed a reservation so the UI can be updated
				delegateOnReservationChange();
				// If we've hit our limit, fire the delegate so the host can do the
				// next step in getting parties together
				if (NumConsumedReservations == NumReservations)
				{
					delegateOnReservationsFull();
				}
				// successfully added
				Result = PRR_ReservationAccepted;				
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) successfully added reservation for party leader 0x%016I64X."),
					*BeaconName.ToString(),
					(QWORD&)PartyLeader);
			}
			else
			{
				Result = PRR_IncorrectPlayerCount;
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) failed add reservation for party leader 0x%016I64X. Party too large."),
					*BeaconName.ToString(),
					(QWORD&)PartyLeader);
			}
		}
		else
		{
			Result = PRR_PartyLimitReached;
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) failed add reservation for party leader 0x%016I64X. Reservations are already full."),
				*BeaconName.ToString(),
				(QWORD&)PartyLeader);
		}
	}
	else
	{
		Result = PRR_ReservationDuplicate;
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) failed add reservation for party leader 0x%016I64X. Reservation already exists."),
			*BeaconName.ToString(),
			(QWORD&)PartyLeader);
	}

	return Result;
}

/**
 * Update a party with an existing reservation
 * Avoids adding duplicate entries to the player members of a party.
 *
 * @param PartyLeader the party leader for which the existing reservation entry is being updated
 * @param PlayerMembers players (not including party leader) being added to the existing reservation
 * @return EPartyReservationResult similar to a client update request
 */
BYTE UPartyBeaconHost::UpdatePartyReservationEntry(struct FUniqueNetId PartyLeader,const TArray<struct FPlayerReservation>& PlayerMembers)
{
	EPartyReservationResult Result = PRR_GeneralError;
	if (bWantsDeferredDestroy)
	{
		return Result;
	}
	// See if the beacon is currently accepting reservations
	if (BeaconState == PBHS_DenyReservations)
	{
		return PRR_ReservationDenied;
	}

	// make sure an existing reservation does not exist for this party
	const INT ExistingReservationIdx = GetExistingReservation(PartyLeader);
	if (ExistingReservationIdx != INDEX_NONE)
	{
		if (NumConsumedReservations < NumReservations)
		{
			// Count the number of available slots for the existing reservation's team
			FPartyReservation& ExistingReservation = Reservations(ExistingReservationIdx);
			const INT NumTeamMembers = GetNumPlayersOnTeam(ExistingReservation.TeamNum);
			const INT NumAvailableSlotsOnTeam = Max<INT>(0,NumPlayersPerTeam - NumTeamMembers);
			// Read the list of new players and remove the ones that have existing reservation entries
			TArray<FPlayerReservation> NewPlayers;
			for (INT PlayerIdx=0; PlayerIdx < PlayerMembers.Num(); PlayerIdx++)
			{
				const FPlayerReservation& PlayerRes = PlayerMembers(PlayerIdx);
				if (GetReservationPlayerMember(ExistingReservation,PlayerRes.NetId) == INDEX_NONE)
				{
					// player reservation doesn't exist so add it as a new player
					NewPlayers.AddItem(PlayerRes);
				}
				else
				{
					// duplicate entry for this player
					debugf(NAME_DevBeacon,
						TEXT("Skipping Player 0x%016I64X, Skill (%d), Mu (%f), Sigma (%f)"),
						(QWORD&)PlayerRes.NetId,
						PlayerRes.Skill,
						PlayerRes.Mu,
						PlayerRes.Sigma);
				}
			}
			// Validate that adding the new party members to this reservation entry still fits within the team size
			if (NewPlayers.Num() <= NumAvailableSlotsOnTeam)
			{
				if (NewPlayers.Num() > 0)
				{
					// Copy new player entries into existing reservation
					for (INT PlayerIdx=0; PlayerIdx < NewPlayers.Num(); PlayerIdx++)
					{
						const FPlayerReservation& PlayerRes = NewPlayers(PlayerIdx);						
						ExistingReservation.PartyMembers.AddItem(PlayerRes);
						// Keep track of newly added players
						NewPlayerAdded(PlayerRes);
					}
					// successfully updated
					Result = PRR_ReservationAccepted;				
					debugf(NAME_DevBeacon,
						TEXT("Beacon (%s) successfully updated reservation for party leader 0x%016I64X."),
						*BeaconName.ToString(),
						(QWORD&)PartyLeader);
					// Update the reservation count before sending the response
					NumConsumedReservations += NewPlayers.Num();
					// Tell any UI and/or clients that there has been a change in the reservation state
					SendReservationUpdates();
					// Tell the owner that we've received a reservation so the UI can be updated
					delegateOnReservationChange();
					// If we've hit our limit, fire the delegate so the host can do the
					// next step in getting parties together
					if (NumConsumedReservations == NumReservations)
					{
						delegateOnReservationsFull();
					}
				}	
				else
				{
					// Duplicate entries (or zero) so existing reservation not updated
					Result = PRR_ReservationDuplicate;
					debugf(NAME_DevBeacon,
						TEXT("Beacon (%s) failed to update reservation for party leader 0x%016I64X. All player entries were duplicates."),
						*BeaconName.ToString(),
						(QWORD&)PartyLeader);
				}
			}
			else
			{
				Result = PRR_IncorrectPlayerCount;
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) failed to update reservation for party leader 0x%016I64X. Party too large."),
					*BeaconName.ToString(),
					(QWORD&)PartyLeader);
			}
			
		}
		else
		{
			Result = PRR_PartyLimitReached;
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) failed to update reservation for party leader 0x%016I64X. Reservations are already full."),
				*BeaconName.ToString(),
				(QWORD&)PartyLeader);
		}

	}
	else
	{
		Result = PRR_ReservationNotFound;
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) failed to update reservation for party leader 0x%016I64X. Reservation does not exists."),
			*BeaconName.ToString(),
			(QWORD&)PartyLeader);
	}

	return Result;
}

/**
 * Called when a player logs out of the current game.  The player's 
 * party reservation entry is freed up so that a new reservation request
 * can be accepted.
 *
 * @param PlayerId the net Id of the player that just logged out
 * @param bMaintainParty if TRUE then preserve party members of a reservation when the party leader logs out
 */
void UPartyBeaconHost::HandlePlayerLogout(FUniqueNetId PlayerId, UBOOL bMaintainParty)
{
	if (PlayerId.HasValue())
	{
		UBOOL bWasRemoved=FALSE;

		debugf(NAME_DevBeacon,TEXT("Beacon (%s) HandlePlayerLogout: %s"),
				*BeaconName.ToString(),
				*UOnlineSubsystem::UniqueNetIdToString(PlayerId));

		// New reservations that may have been created from splits
		TArray<FPartyReservation> SplitReservations;
		for (INT ResIdx=0; ResIdx < Reservations.Num(); ResIdx++)
		{
			FPartyReservation& Reservation = Reservations(ResIdx);
			// promote a new party leader if it is the party leader that is logging out
			// but still maintain the party members of the reservation
			if (Reservation.PartyLeader == PlayerId)
			{
				debugf(NAME_DevBeacon,TEXT("Beacon (%s) HandlePlayerLogout: %s was party host. Splitting existing party."),
					*BeaconName.ToString(),
					*UOnlineSubsystem::UniqueNetIdToString(PlayerId));

				if (!bMaintainParty)
				{
					// If party leader left then split up the party into individual reservations with their own party leaders
					for (INT PlayerIdx=0; PlayerIdx < Reservation.PartyMembers.Num(); PlayerIdx++)
					{
						// Players left behind by their party leader
						const FPlayerReservation& PlayerEntry = Reservation.PartyMembers(PlayerIdx);
						// leave party leader since he will just get removed below
						if (PlayerEntry.NetId != Reservation.PartyLeader)
						{
							// They each get their own reservation entries
							FPartyReservation& SplitRes = *new(SplitReservations) FPartyReservation(EC_EventParm);
							SplitRes.PartyLeader = PlayerEntry.NetId;
							SplitRes.TeamNum = Reservation.TeamNum;
							SplitRes.PartyMembers.AddItem(PlayerEntry);
							Reservation.PartyMembers.Remove(PlayerIdx--);
						}
					}
				}
				else
				{
					// Maintain existing members of party reservation that lost its leader
					for (INT PlayerIdx=0; PlayerIdx < Reservation.PartyMembers.Num(); PlayerIdx++)
					{
						FPlayerReservation& PlayerEntry = Reservation.PartyMembers(PlayerIdx);
						if (PlayerEntry.NetId != Reservation.PartyLeader && PlayerEntry.NetId.HasValue())
						{
							// Promote to party leader
							Reservation.PartyLeader = PlayerEntry.NetId;
							break;
						}
					}
				}
				// player removed
				bWasRemoved = TRUE;
			}
			// find the player in an existing reservation slot
			for (INT PlayerIdx=0; PlayerIdx < Reservation.PartyMembers.Num(); PlayerIdx++)
			{	
				FPlayerReservation& PlayerEntry = Reservation.PartyMembers(PlayerIdx);
				if (PlayerEntry.NetId == PlayerId)
				{
					// player removed
					Reservation.PartyMembers.Remove(PlayerIdx--);
					bWasRemoved = TRUE;

					// free up a consumed entry
					NumConsumedReservations--;
				}
			}
			// remove the entire party reservation slot if no more party members
			if (Reservation.PartyMembers.Num() == 0)
			{
				Reservations.Remove(ResIdx--);
			}
		}
		// Add any new reservations
		for (INT ResIdx=0; ResIdx < SplitReservations.Num(); ResIdx++)
		{
			Reservations.AddItem(SplitReservations(ResIdx));
		}
		if (bWasRemoved)
		{	
			// Reshuffle existing teams so that beacon can accommodate biggest open slots
			BestFitTeamAssignmentJiggle();
			// Tell any UI and/or clients that there has been a change in the reservation state
			SendReservationUpdates();
			// Tell the owner that we've changed a reservation so the UI can be updated
			delegateOnReservationChange();
		}
	}
}

/**
 * Initializes the team array so that random choices can be made from it
 * Also initializes the host's team number (random from range)
 */
void UPartyBeaconHost::InitTeamArray(void)
{
	if (NumTeams > 1)
	{
		// Grab one for the host team
		ReservedHostTeamNum = appRand() % NumTeams;
	}
	else
	{
		// Only one team, so choose 'forced team' for everything
		NumTeams = 1;
		ReservedHostTeamNum = ForceTeamNum;
	}
	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) team count (%d), team size (%d), host team (%d)"),
		*BeaconName.ToString(),
		NumTeams,
		NumPlayersPerTeam,
		ReservedHostTeamNum);
}

/** 
 * Determine if there are any teams that can fit the current party request.
 * 
 * @param PartySize number of players in the party making a reservation request
 * @return TRUE if there are teams available, FALSE otherwise 
 */
UBOOL UPartyBeaconHost::AreTeamsAvailable(INT PartySize)
{
	for (INT TeamIdx=0; TeamIdx < NumTeams; TeamIdx++)
	{
		const INT CurrentPlayersOnTeam = GetNumPlayersOnTeam(TeamIdx);
		if ((CurrentPlayersOnTeam + PartySize) <= NumPlayersPerTeam)
		{
			return TRUE;
		}
	}
	return FALSE;
}

/** Helps to sort reservations by party size for use by best fit team assignment */
struct FBestFitHelper
{
	FPartyReservation* PartyRes;
	FBestFitHelper(FPartyReservation* InPartyRes=NULL)
		: PartyRes(InPartyRes)
	{}
	FBestFitHelper& operator=(const FBestFitHelper& Other)
	{
		PartyRes = Other.PartyRes;
		return *this;
	}

};
/** Sort in descending (highest to lowest) order based on size of reservation */
IMPLEMENT_COMPARE_CONSTREF(FBestFitHelper,PartySizeDescending, 
{ 
	return B.PartyRes->PartyMembers.Num() - A.PartyRes->PartyMembers.Num(); 
})

/**
 * Readjust existing team assignments for party reservations in order to open the max
 * available slots possible.
 */
void UPartyBeaconHost::BestFitTeamAssignmentJiggle()
{
	if (bBestFitTeamAssignment && 
		NumTeams > 1)
	{
		TArray<FBestFitHelper> ReservationsToJiggle;
		for (INT ResIdx=0; ResIdx < Reservations.Num(); ResIdx++)
		{
			FPartyReservation& Reservation = Reservations(ResIdx);
			// Only want to rejiggle reservations with existing team assignments (new reservations will still stay at -1)
			if (Reservation.TeamNum != -1)
			{
				// Remove existing team assignments so new assignments can be given
				Reservation.TeamNum = -1;
				// Add to list of reservations that need new assignments
				ReservationsToJiggle.AddItem(FBestFitHelper(&Reservation));
			}
		}
		// Sort so that largest party reservations come first
		Sort<USE_COMPARE_CONSTREF(FBestFitHelper,PartySizeDescending)>(&ReservationsToJiggle(0),ReservationsToJiggle.Num());
		// Re-add these reservations with best fit team assignments
		for (INT ResIdx=0; ResIdx < ReservationsToJiggle.Num(); ResIdx++)
		{
			FPartyReservation& Reservation = *(ReservationsToJiggle(ResIdx).PartyRes);
			Reservation.TeamNum = GetTeamAssignment(Reservation);
			if (Reservation.TeamNum == -1)
			{
				debugf(NAME_DevBeacon,TEXT("(UPartyBeaconHost.BestFitTeamAssignmentJiggle): could not reassign to a team!"));
			}
		}
	}
}

/**
 * Determine the team number for the given party reservation request.
 * Uses the list of current reservations to determine what teams have open slots.
 *
 * @param PartyRequest the party reservation request received from the client beacon
 * @return index of the team to assign to all members of this party
 */
INT UPartyBeaconHost::GetTeamAssignment(const FPartyReservation& Party)
{
	if (NumTeams > 1)
	{
		TArray<INT> PotentialTeamChoices;
		for (INT TeamIdx=0; TeamIdx < NumTeams; TeamIdx++)
		{
			const INT CurrentPlayersOnTeam = GetNumPlayersOnTeam(TeamIdx);
			if ((CurrentPlayersOnTeam + Party.PartyMembers.Num()) <= NumPlayersPerTeam)
			{
				PotentialTeamChoices.AddItem(TeamIdx);
			}
		}

		// Get the team with the largest current num of players that can still accommodate our party size
		if (bBestFitTeamAssignment && 
			PotentialTeamChoices.Num() > 0)
		{
			// Iterate over team choices and find largest current team size
			INT LargestTeamSize = 0;
			for (INT ChoiceIdx=0; ChoiceIdx < PotentialTeamChoices.Num(); ChoiceIdx++)
			{
				const INT TeamIdx = PotentialTeamChoices(ChoiceIdx);
				const INT CurrentPlayersOnTeam = GetNumPlayersOnTeam(TeamIdx);
				if (CurrentPlayersOnTeam > LargestTeamSize)
				{
					LargestTeamSize = CurrentPlayersOnTeam;
				}
			}
			// List of teams matching the largest team size
			TArray<INT> PotentialTeamChoicesLargest;
			for (INT ChoiceIdx=0; ChoiceIdx < PotentialTeamChoices.Num(); ChoiceIdx++)
			{
				const INT TeamIdx = PotentialTeamChoices(ChoiceIdx);
				const INT CurrentPlayersOnTeam = GetNumPlayersOnTeam(TeamIdx);
				if (CurrentPlayersOnTeam == LargestTeamSize)
				{
					PotentialTeamChoicesLargest.AddItem(TeamIdx);
				}
			}
			// Limit potential team choices the ones with the same largest available size to choose randomly from these
			PotentialTeamChoices = PotentialTeamChoicesLargest;
		}

		// Grab one from our list of choices
		if (PotentialTeamChoices.Num() > 0)
		{
			// Random choice from set of choices
			INT TeamIndex = appRand() % PotentialTeamChoices.Num();
			return PotentialTeamChoices(TeamIndex);
		}
		else
		{
			debugf(NAME_DevBeacon,TEXT("(UPartyBeaconHost.GetTeamAssignment): couldn't find an open team for party members."));
			return -1;
		}
	}

	return ForceTeamNum;
}

/** Accepts any pending connections and adds them to our queue */
void UPartyBeaconHost::AcceptConnections(void)
{
	FSocket* ClientSocket = NULL;
	do
	{
		// See what clients have connected and add them for processing
		ClientSocket = Socket->Accept(TEXT("party beacon host client"));
		if (ClientSocket)
		{
			// Add to the list for reading
			INT AddIndex = Clients.AddZeroed();
			FClientBeaconConnection& ClientConn = Clients(AddIndex);
			ClientConn.Socket = ClientSocket;
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) new client connection from (%s)"),
				*BeaconName.ToString(),
				*ClientSocket->GetAddress().ToString(TRUE));
		}
		else
		{
			INT SocketError = GSocketSubsystem->GetLastErrorCode();
			if (SocketError != SE_EWOULDBLOCK)
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) failed to accept a connection due to: %s"),
					*BeaconName.ToString(),
					GSocketSubsystem->GetSocketError());
			}
		}
	}
	while (ClientSocket);
}

/**
 * Reads the socket and processes any data from it
 *
 * @param ClientConn the client connection that sent the packet
 *
 * @return TRUE if the socket is ok, FALSE if it is in error
 */
UBOOL UPartyBeaconHost::ReadClientData(FClientBeaconConnection& ClientConn)
{
	UBOOL bShouldRead = TRUE;
	BYTE PacketData[512];
	// Read each pending packet and pass it out for processing
	while (bShouldRead)
	{
		INT BytesRead;
		// Read from the socket and handle errors if present
		if (ClientConn.Socket->Recv(PacketData,512,BytesRead))
		{
			if (BytesRead > 0)
			{
				ClientConn.ElapsedHeartbeatTime = 0.f;
				// Process the packet
				ProcessRequest(PacketData,BytesRead,ClientConn);
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
				if (ShouldCancelReservationOnDisconnect(ClientConn))
				{
					// Cancel this reservation, since they are gone
					CancelPartyReservation(ClientConn.PartyLeader,ClientConn);
				}
				else
				{
					// Zero the party leader, so his socket timeout doesn't cause another cancel
					(QWORD&)ClientConn.PartyLeader = 0;
				}
				// Log and remove this connection
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) closing socket to (%s) with error (%s)"),
					*BeaconName.ToString(),
					*ClientConn.Socket->GetAddress().ToString(TRUE),
					GSocketSubsystem->GetSocketError(ErrorCode));
				return FALSE;
			}
			bShouldRead = FALSE;
		}
	}
	return TRUE;
}

/**
 * Ticks the network layer to see if there are any requests or responses to requests
 *
 * @param DeltaTime the amount of time that has elapsed since the last tick
 */
void UPartyBeaconHost::Tick(FLOAT DeltaTime)
{
#if WITH_UE3_NETWORKING		 
	// Only tick if we have a socket present
	if (Socket && bShouldTick && bWantsDeferredDestroy == FALSE)
	{
		// Use this to guard against deleting the socket while in use
		bIsInTick = TRUE;
		// Add any new connections since the last tick
		AcceptConnections();
		// Skip ticking if no clients are connected
		if (Clients.Num())
		{
			// Determine if it's time to send a heartbeat
			ElapsedHeartbeatTime += DeltaTime;
			UBOOL bNeedsHeartbeat = (ElapsedHeartbeatTime > (HeartbeatTimeout * 0.5f));
			// Check each client in the list for a packet
			for (INT Index = 0; Index < Clients.Num(); Index++)
			{
				UBOOL bHadError = FALSE;
				// Grab the connection we are checking for data on
				FClientBeaconConnection& ClientConn = Clients(Index);
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
					// Check for a client going beyond our heartbeat timeout
					if (ClientConn.ElapsedHeartbeatTime > HeartbeatTimeout)
					{
						debugf(NAME_DevBeacon,
							TEXT("Beacon (%s) client timed out. Leader 0x%016I64X"),
							*BeaconName.ToString(),
							(QWORD&)ClientConn.PartyLeader);
						bHadError = bShouldTick && bWantsDeferredDestroy == FALSE;
					}
				}
				else
				{
					debugf(NAME_DevBeacon,
						TEXT("Beacon (%s) reading from client failed. Leader 0x%016I64X"),
						*BeaconName.ToString(),
						(QWORD&)ClientConn.PartyLeader);
					bHadError = bShouldTick && bWantsDeferredDestroy == FALSE;
				}
				if (bHadError)
				{
					if (ShouldCancelReservationOnDisconnect(ClientConn))
					{
						// Cancel this client
						CancelPartyReservation(ClientConn.PartyLeader,ClientConn);
					}
					else
					{
						// Zero the party leader, so his socket timeout doesn't cause another cancel
						(QWORD&)ClientConn.PartyLeader = 0;
					}
					// Now clean up that socket
					GSocketSubsystem->DestroySocket(ClientConn.Socket);
					Clients.Remove(Index);
					Index--;
				}
			}
		}
		// No longer a danger, so release the sentinel on the destruction
		bIsInTick = FALSE;
	}
	Super::Tick(DeltaTime);
#endif
}

/**
 * Determine if the reservation entry for a disconnected client should be removed.
 *
 * @param ClientConn the client connection that is disconnecting
 * @return TRUE if the reservation for the disconnected client should be removed
 */
UBOOL UPartyBeaconHost::ShouldCancelReservationOnDisconnect(const FClientBeaconConnection& ClientConn)
{
	return TRUE;
}

/**
 * Routes the packet received from a client to the correct handler based on its type.
 * Overridden by base implementations to handle custom data packet types
 *
 * @param RequestPacketType packet ID from EReservationPacketType (or derived version) that represents a client request
 * @param FromBuffer the packet serializer to read from
 * @param ClientConn the client connection that sent the packet
 * @return TRUE if the requested packet type was processed
 */
UBOOL UPartyBeaconHost::HandleClientRequestPacketType(BYTE RequestPacketType,FNboSerializeFromBuffer& FromBuffer,FClientBeaconConnection& ClientConn)
{
	// Route the call to the proper handler
	switch (RequestPacketType)
	{
		case RPT_ClientReservationRequest:
		{
			ProcessReservationRequest(FromBuffer,ClientConn);
			break;
		}
		case RPT_ClientReservationUpdateRequest:
		{
			ProcessReservationUpdateRequest(FromBuffer,ClientConn);
			break;
		}
		case RPT_ClientCancellationRequest:
		{
			ProcessCancellationRequest(FromBuffer,ClientConn);
			break;
		}
		case RPT_Heartbeat:
		{
			break;
		}
		default:
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) unknown packet type received from client (%d)"),
				*BeaconName.ToString(),
				(DWORD)RequestPacketType);

			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Processes a packet that was received from a potential client when in host mode
 *
 * @param Packet the packet that the client sent
 * @param PacketSize the size of the packet to process
 * @param ClientConn the client connection that sent the packet
 */
void UPartyBeaconHost::ProcessRequest(BYTE* Packet,INT PacketSize,FClientBeaconConnection& ClientConn)
{
	// Use our packet serializer to read from the raw buffer
	FNboSerializeFromBuffer FromBuffer(Packet,PacketSize);

	// Work through the stream until we've consumed it all
	do
	{
		BYTE PacketType = RPT_UnknownPacketType;
		FromBuffer >> PacketType;

		// Don't process a packet if we are at the end
		if (FromBuffer.HasOverflow() == FALSE)
		{
			// Route the host response based on packet type
			if (!HandleClientRequestPacketType(PacketType,FromBuffer,ClientConn))
			{
				break;
			}
		}
	}
	while (FromBuffer.HasOverflow() == FALSE);
}

/**
 * Determine the number of players on a given team based on current reservation entries
 *
 * @param TeamIdx index of team to search for
 * @return number of players on a given team
 */
INT UPartyBeaconHost::GetNumPlayersOnTeam(INT TeamIdx) const
{
	INT Result = 0;
	for (INT ResIdx=0; ResIdx < Reservations.Num(); ResIdx++)
	{
		const FPartyReservation& Reservation = Reservations(ResIdx);
		if (Reservation.TeamNum == TeamIdx)
		{
			for (INT PlayerIdx=0; PlayerIdx < Reservation.PartyMembers.Num(); PlayerIdx++)
			{
				const FPlayerReservation& PlayerEntry = Reservation.PartyMembers(PlayerIdx);
				// only count valid player net ids
				if (PlayerEntry.NetId.HasValue())
				{
					// count party members in each team (includes party leader)
					Result++;
				}
			}
		}
	}
	return Result;
}

/**
 * Find an existing reservation for the party leader
 *
 * @param PartyLeader the party leader to find a reservation entry for
 * @return index of party leader in list of reservations, -1 if not found
 */
INT UPartyBeaconHost::GetExistingReservation(const FUniqueNetId& PartyLeader)
{
	INT Result = INDEX_NONE;
	for (INT ResIdx=0; ResIdx < Reservations.Num(); ResIdx++)
	{
		const FPartyReservation& ReservationEntry = Reservations(ResIdx);
		if (ReservationEntry.PartyLeader == PartyLeader)
		{
			Result = ResIdx;
			break;
		}
	}
	return Result; 
}

/**
 * Find an existing player entry in a party reservation 
 *
 * @param ExistingReservation party reservation entry
 * @param PlayerMember member to search for in the existing party reservation
 * @return index of party member in the given reservation, -1 if not found
 */
INT UPartyBeaconHost::GetReservationPlayerMember(const FPartyReservation& ExistingReservation, const FUniqueNetId& PlayerMember) const
{
	INT Result = INDEX_NONE;
	for (INT PlayerIdx=0; PlayerIdx < ExistingReservation.PartyMembers.Num(); PlayerIdx++)
	{
		const FPlayerReservation& PlayerReservationEntry = ExistingReservation.PartyMembers(PlayerIdx);
		if (PlayerReservationEntry.NetId == PlayerMember)
		{
			Result = PlayerIdx;
			break;
		}
	}
	return Result;
}

/**
 * Processes a reservation packet that was received from a client
 *
 * @param FromBuffer the packet serializer to read from
 * @param ClientConn the client connection that sent the packet
 */
void UPartyBeaconHost::ProcessReservationRequest(FNboSerializeFromBuffer& FromBuffer,FClientBeaconConnection& ClientConn)
{
	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) received reservation request from (%s)"),
		*BeaconName.ToString(),
		*ClientConn.Socket->GetAddress().ToString(TRUE));

	FUniqueNetId PartyLeader;
	// Serialize the leader from the buffer
	FromBuffer >> PartyLeader;
	INT PartySize = 0;
	// Now read the number of party members that were sent over
	FromBuffer >> PartySize;
	// The size is fine, so create a reservation entry
	FPartyReservation Reservation;
	appMemzero(&Reservation, sizeof(FPartyReservation));
	// Copy over the information
	Reservation.PartyLeader = PartyLeader;
	// Make sure buffer has enough data to read all members of the party
	UBOOL bBufferOverflow = FromBuffer.AvailableToRead() < (PartySize * (INT)(sizeof(FPlayerReservation)-sizeof(FLOAT)));
	if (bBufferOverflow)
	{		
		// Seek to end of buffer so that processing of packet reads will end 
		FromBuffer.Seek(FromBuffer.GetBufferSize());
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) overflowed buffer trying to add reservation: Leader 0x%016I64X"),
			*BeaconName.ToString(),
			(QWORD&)PartyLeader);
	}
	else
	{
		// Create all of the members
		Reservation.PartyMembers.AddZeroed(PartySize);
		// Now serialize each member
		for (INT Count = 0; Count < PartySize; Count++)
		{
			FPlayerReservation& PlayerRes = Reservation.PartyMembers(Count);
			FromBuffer >> PlayerRes;
		}
	}

	// See if the beacon is currently accepting reservations
	if (BeaconState == PBHS_DenyReservations)
	{
		SendReservationResponse(PRR_ReservationDenied,ClientConn.Socket);
		return;
	}

	// See if we have space or not
	if (NumConsumedReservations < NumReservations && !bBufferOverflow)
	{
		// Make sure we don't create duplicate reservations for the same party leader
		const INT ExistingReservationIdx = GetExistingReservation(PartyLeader);
		if (ExistingReservationIdx == INDEX_NONE)
		{
			// Validate that the party size matches our max team size expectations
			if (PartySize <= NumPlayersPerTeam &&
				// And that there is enough space to hold this group
				(PartySize + NumConsumedReservations) <= NumReservations &&
				// And that there are teams left for them to reserve
				AreTeamsAvailable(PartySize))
			{
				// Keep track of newly added players
				for (INT Count = 0; Count < Reservation.PartyMembers.Num(); Count++)
				{	
					NewPlayerAdded(Reservation.PartyMembers(Count));
				}
				// Put this party on a team chosen from the set available
				Reservation.TeamNum = -1;
				Reservation.TeamNum = GetTeamAssignment(Reservation);
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) added reservation: Leader 0x%016I64X, Team (%d)"),
					*BeaconName.ToString(),
					(QWORD&)PartyLeader,
					Reservation.TeamNum);
				if (Reservation.TeamNum != -1)
				{
					// Add the new reservation entry
					Reservations.AddItem(Reservation);
					// Update the reservation count before sending the response
					NumConsumedReservations += PartySize;
					// Copy the party leader to this connection so that timeouts can remove the party
					ClientConn.PartyLeader = PartyLeader;
					// Reshuffle existing teams so that beacon can accommodate biggest open slots
					BestFitTeamAssignmentJiggle();
					// Send a happy response
					SendReservationResponse(PRR_ReservationAccepted,ClientConn.Socket);
					// Tell any UI and/or clients that there has been a change in the reservation state
					SendReservationUpdates();
					// Tell the owner that we've received a reservation so the UI can be updated
					delegateOnReservationChange();
					// If we've hit our limit, fire the delegate so the host can do the
					// next step in getting parties together
					if (NumConsumedReservations == NumReservations)
					{
						delegateOnReservationsFull();
					}
				}
				else
				{
					// Send an invalid party size response
					SendReservationResponse(PRR_IncorrectPlayerCount,ClientConn.Socket);
				}
			}
			else
			{
				// Send an invalid party size response
				SendReservationResponse(PRR_IncorrectPlayerCount,ClientConn.Socket);
			}
		}
		else		
		{
			// Send a duplicate reservation response
			SendReservationResponse(PRR_ReservationDuplicate,ClientConn.Socket);
		}
	}
	else
	{
		// Send a session full response
		SendReservationResponse(PRR_PartyLimitReached,ClientConn.Socket);
	}
}

/**
 * Processes a reservation update packet that was received from a client
 * The update finds an existing reservation based on the party leader and
 * then tries to add new players to it (can't remove). Then sends a response to
 * the client based on the update results (see EPartyReservationResult).
 *
 * Note: assumes that party leader was not added as a party member
 *
 * @param FromBuffer the packet serializer to read from
 * @param ClientConn the client connection that sent the packet
 */
void UPartyBeaconHost::ProcessReservationUpdateRequest(FNboSerializeFromBuffer& FromBuffer,FClientBeaconConnection& ClientConn)
{
	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) received reservation update request from (%s)"),
		*BeaconName.ToString(),
		*ClientConn.Socket->GetAddress().ToString(TRUE));

	FUniqueNetId PartyLeader;
	// Serialize the leader from the buffer
	FromBuffer >> PartyLeader;
	INT PartySize = 0;
	// Now read the number of party members that were sent over
	FromBuffer >> PartySize;
	// The size is fine, so create a reservation entry
	FPartyReservation Reservation;
	appMemzero(&Reservation, sizeof(FPartyReservation));
	// Copy over the information
	Reservation.PartyLeader = PartyLeader;
	// Make sure buffer has enough data to read all members of the party
	UBOOL bBufferOverflow = FromBuffer.AvailableToRead() < (PartySize * (INT)(sizeof(FPlayerReservation)-sizeof(FLOAT)));
	if (bBufferOverflow)
	{		
		// Seek to end of buffer so that processing of packet reads will end 
		FromBuffer.Seek(FromBuffer.GetBufferSize());
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) overflowed buffer trying to add reservation: Leader 0x%016I64X"),
			*BeaconName.ToString(),
			(QWORD&)PartyLeader);
	}
	else
	{
		// Create all of the members
		Reservation.PartyMembers.AddZeroed(PartySize);
		// Now serialize each member
		for (INT Count = 0; Count < PartySize; Count++)
		{
			FPlayerReservation& PlayerRes = Reservation.PartyMembers(Count);
			FromBuffer >> PlayerRes;
		}
	}

	// See if the beacon is currently accepting reservations
	if (BeaconState == PBHS_DenyReservations)
	{
		SendReservationResponse(PRR_ReservationDenied,ClientConn.Socket);
		return;
	}

	// See if we have space or not
	if (NumConsumedReservations < NumReservations)
	{
		// Find the existing reservation slot for the party leader
		const INT ExistingReservationIdx = GetExistingReservation(PartyLeader);
		if (ExistingReservationIdx != INDEX_NONE)
		{
			// Count the number of available slots for the existing reservation's team
			FPartyReservation& ExistingReservation = Reservations(ExistingReservationIdx);
			const INT NumTeamMembers = GetNumPlayersOnTeam(ExistingReservation.TeamNum);
			const INT NumAvailableSlotsOnTeam = Max<INT>(0,NumPlayersPerTeam - NumTeamMembers);
			// Read the list of new players and remove the ones that have existing reservation entries
			TArray<FPlayerReservation> NewPlayers;
			for (INT PlayerIdx=0; PlayerIdx < Reservation.PartyMembers.Num(); PlayerIdx++)
			{
				FPlayerReservation& PlayerRes = Reservation.PartyMembers(PlayerIdx);
				if (GetReservationPlayerMember(ExistingReservation,PlayerRes.NetId) == INDEX_NONE)
				{
					// player reservation doesn't exist so add it as a new player
					NewPlayers.AddItem(PlayerRes);					
				}
				else
				{
					// duplicate entry for this player
					debugf(NAME_DevBeacon,
						TEXT("Skipping Player 0x%016I64X, Skill (%d), Mu (%f), Sigma (%f)"),
						(QWORD&)PlayerRes.NetId,
						PlayerRes.Skill,
						PlayerRes.Mu,
						PlayerRes.Sigma);
				}
			}
			
			// Validate that adding the new party members to this reservation entry still fits within the team size
			if (NewPlayers.Num() <= NumAvailableSlotsOnTeam)
			{
				if (NewPlayers.Num() > 0)
				{
					// Copy new player entries into existing reservation
					for (INT PlayerIdx=0; PlayerIdx < NewPlayers.Num(); PlayerIdx++)
					{
						const FPlayerReservation& PlayerRes = NewPlayers(PlayerIdx);						
						ExistingReservation.PartyMembers.AddItem(PlayerRes);
						// Keep track of newly added players
						NewPlayerAdded(PlayerRes);
					}
					// Update the reservation count before sending the response
					NumConsumedReservations += NewPlayers.Num();
					// Send a happy response
					SendReservationResponse(PRR_ReservationAccepted,ClientConn.Socket);
					// Tell any UI and/or clients that there has been a change in the reservation state
					SendReservationUpdates();
					// Tell the owner that we've received a reservation so the UI can be updated
					delegateOnReservationChange();
					// If we've hit our limit, fire the delegate so the host can do the
					// next step in getting parties together
					if (NumConsumedReservations == NumReservations)
					{
						delegateOnReservationsFull();
					}
				}	
				else
				{
					// Duplicate entries (or zero) so existing reservation not updated
					SendReservationResponse(PRR_ReservationDuplicate,ClientConn.Socket);
				}
			}
			else
			{
				// Send an invalid party size response
				SendReservationResponse(PRR_IncorrectPlayerCount,ClientConn.Socket);
			}
		}
		else		
		{
			// Send a not found reservation response
			SendReservationResponse(PRR_ReservationNotFound,ClientConn.Socket);
		}
	}
	else
	{
		// Send a session full response
		SendReservationResponse(PRR_PartyLimitReached,ClientConn.Socket);
	}
}

/**
 * Processes a cancellation packet that was received from a client
 *
 * @param FromBuffer the packet serializer to read from
 * @param ClientConn the client connection that sent the packet
 */
void UPartyBeaconHost::ProcessCancellationRequest(FNboSerializeFromBuffer& FromBuffer,FClientBeaconConnection& ClientConn)
{
	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) received cancellation request from (%s)"),
		*BeaconName.ToString(),
		*ClientConn.Socket->GetAddress().ToString(TRUE));
	FUniqueNetId PartyLeader;
	// Serialize the leader from the buffer
	FromBuffer >> PartyLeader;
	// Remove them from the various arrays
	CancelPartyReservation(PartyLeader,ClientConn);
}

/**
 * Removes the specified party leader (and party) from the arrays and notifies
 * any connected clients of the change in status
 *
 * @param PartyLeader the leader of the party to remove
 * @param ClientConn the client connection that sent the packet
 */
void UPartyBeaconHost::CancelPartyReservation(FUniqueNetId& PartyLeader,FClientBeaconConnection& ClientConn)
{
	INT PartyIndex;
	INT PartySize = 0;
	INT TeamNum = -1;
	// Find the party so they can be removed
	for (PartyIndex = 0; PartyIndex < Reservations.Num(); PartyIndex++)
	{
		FPartyReservation& Reservation = Reservations(PartyIndex);
		if (Reservation.PartyLeader == PartyLeader)
		{
			TeamNum = Reservation.TeamNum;
			PartySize = Reservation.PartyMembers.Num();
			break;
		}
	}
	// Whether we found the leader or not
	if (Reservations.IsValidIndex(PartyIndex))
	{
		// Notify the code that it has lost players
		delegateOnClientCancellationReceived(PartyLeader);
		// Remove the party members from the session
		eventUnregisterParty(PartyLeader);
		NumConsumedReservations -= PartySize;
		// Remove the reservation, now that we are no longer accessing it
		Reservations.Remove(PartyIndex);
		// Reshuffle existing teams so that beacon can accommodate biggest open slots
		BestFitTeamAssignmentJiggle();
		// Tell any UI and/or clients that there has been a change in the reservation state
		SendReservationUpdates();
		// Tell the owner that we've received a reservation so the UI can be updated
		delegateOnReservationChange();
		// Zero the party leader, so his socket timeout doesn't cause another cancel
		(QWORD&)ClientConn.PartyLeader = 0;
	}
	else
	{
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) unable to find party reservation to cancel for party leader: 0x%016I64X."),
			*BeaconName.ToString(),
			(QWORD&)PartyLeader);
	}
}

/**
 * Sends a client the specified response code
 *
 * @param Result the result being sent to the client
 * @param ClientSocket the client socket to send the response on
 */
void UPartyBeaconHost::SendReservationResponse(EPartyReservationResult Result,FSocket* ClientSocket)
{
	check(ClientSocket);
	INT NumRemaining = NumReservations - NumConsumedReservations;
	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) sending host response (%s),(%d) to client (%s)"),
		*BeaconName.ToString(),
		PartyReservationResultToString(Result),
		NumRemaining,
		*ClientSocket->GetAddress().ToString(TRUE));
	FNboSerializeToBuffer ToBuffer(64);
	// Packet format is <Type><Result><NumRemaining>
	ToBuffer << (BYTE)RPT_HostReservationResponse
		<< (BYTE)Result
		<< NumRemaining;
	INT BytesSent;
	UBOOL bDidSendOk = ClientSocket->Send(ToBuffer.GetRawBuffer(0),ToBuffer.GetByteCount(),BytesSent);
	if (bDidSendOk == FALSE)
	{
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) failed to send reservation response to client with (%s)"),
			*BeaconName.ToString(),
			GSocketSubsystem->GetSocketError());
	}
}

/**
 * Tells clients that a reservation update has occured and sends them the current
 * number of remaining reservations so they can update their UI
 */
void UPartyBeaconHost::SendReservationUpdates(void)
{
	INT NumRemaining = NumReservations - NumConsumedReservations;
	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) sending reservation count update to clients (%d)"),
		*BeaconName.ToString(),
		NumRemaining);
	// Build the packet that we are sending
	FNboSerializeToBuffer ToBuffer(64);
	// Packet format is <Type><NumRemaining>
	ToBuffer << (BYTE)RPT_HostReservationCountUpdate
		<< NumRemaining;
	INT BytesSent;
	// Iterate through and send to each client
	for (INT SocketIndex = 0; SocketIndex < Clients.Num(); SocketIndex++)
	{
		FClientBeaconConnection& ClientConn = Clients(SocketIndex);
		if ((QWORD&)ClientConn.PartyLeader != (QWORD)0)
		{
			FSocket* ClientSocket = ClientConn.Socket;
			check(ClientSocket);
			UBOOL bDidSendOk = ClientSocket->Send(ToBuffer.GetRawBuffer(0),ToBuffer.GetByteCount(),BytesSent);
			if (bDidSendOk == FALSE)
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) failed to send reservation update to client with (%s)"),
					*BeaconName.ToString(),
					GSocketSubsystem->GetSocketError());
			}
		}
	}
}

/**
 * Tells all of the clients to go to a specific session (contained in platform specific info)
 *
 * @param SessionName the name of the session to register
 * @param SearchClass the search that should be populated with the session
 * @param PlatformSpecificInfo the binary data to place in the platform specific areas
 */
void UPartyBeaconHost::TellClientsToTravel(FName SessionName,UClass* SearchClass,BYTE* PlatformSpecificInfo)
{
	debugf(NAME_DevBeacon,TEXT("Beacon (%s) sending travel information to clients"),*BeaconName.ToString());
	const FString& SessionNameStr = SessionName.ToString();
	const FString& ClassName = SearchClass->GetPathName();
	// Build the packet that we are sending
	FNboSerializeToBuffer ToBuffer(512);
	// Packet format is <Type><SessionNameLen><SessionName><ClassNameLen><ClassName><SecureSessionInfo>
	ToBuffer << (BYTE)RPT_HostTravelRequest
		<< SessionNameStr
		<< ClassName;
	// Copy the buffer over in raw form (it is already in NBO)
	ToBuffer.WriteBinary(PlatformSpecificInfo,80);
	INT BytesSent;
	// Iterate through and send to each client
	for (INT SocketIndex = 0; SocketIndex < Clients.Num(); SocketIndex++)
	{
		FClientBeaconConnection& ClientConn = Clients(SocketIndex);
		// Don't send to a party leader that we didn't accept
		if ((QWORD&)ClientConn.PartyLeader != (QWORD)0)
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
	bShouldTick = FALSE;
}

/**
 * Tells all of the clients that the host is ready for them to travel to
 */
void UPartyBeaconHost::TellClientsHostIsReady(void)
{
	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) sending host is ready message to clients"),
		*BeaconName.ToString());
	BYTE Buffer = RPT_HostIsReady;
	INT BytesSent;
	// Iterate through and send to each client
	for (INT SocketIndex = 0; SocketIndex < Clients.Num(); SocketIndex++)
	{
		FClientBeaconConnection& ClientConn = Clients(SocketIndex);
		if ((QWORD&)ClientConn.PartyLeader != (QWORD)0)
		{
			FSocket* ClientSocket = ClientConn.Socket;
			check(ClientSocket);
			UBOOL bDidSendOk = ClientSocket->Send(&Buffer,1,BytesSent);
			if (bDidSendOk == FALSE)
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) failed to notify client that host is ready with (%s)"),
					*BeaconName.ToString(),
					GSocketSubsystem->GetSocketError());
			}
		}
	}
	bShouldTick = FALSE;
}

/**
 * Tells all of the clients that the host has cancelled the matchmaking beacon and that they
 * need to find a different host
 */
void UPartyBeaconHost::TellClientsHostHasCancelled(void)
{
	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) sending host has cancelled message to clients"),
		*BeaconName.ToString());
	BYTE Buffer = RPT_HostHasCancelled;
	INT BytesSent;
	// Iterate through and send to each client
	for (INT SocketIndex = 0; SocketIndex < Clients.Num(); SocketIndex++)
	{
		FClientBeaconConnection& ClientConn = Clients(SocketIndex);
		if ((QWORD&)ClientConn.PartyLeader != (QWORD)0)
		{
			FSocket* ClientSocket = ClientConn.Socket;
			check(ClientSocket);
			UBOOL bDidSendOk = ClientSocket->Send(&Buffer,1,BytesSent);
			if (bDidSendOk == FALSE)
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) failed to notify client that host is cancelling with (%s)"),
					*BeaconName.ToString(),
					GSocketSubsystem->GetSocketError());
			}
		}
	}
	bShouldTick = FALSE;
}

/**
 * Appends the skills from all reservations to the search object so that they can
 * be included in the search information
 *
 * @param Search the search object to update
 */
void UPartyBeaconHost::AppendReservationSkillsToSearch(UOnlineGameSearch* Search)
{
	if (Search != NULL)
	{
		// Add each reservations players to the skill information
		for (INT PartyIndex = 0; PartyIndex < Reservations.Num(); PartyIndex++)
		{
			const FPartyReservation& Reservation = Reservations(PartyIndex);
			for (INT PlayerIndex = 0; PlayerIndex < Reservation.PartyMembers.Num(); PlayerIndex++)
			{
				const FPlayerReservation& PlayerRes = Reservation.PartyMembers(PlayerIndex);
				// Copy the player & their skill information over
				Search->ManualSkillOverride.Players.AddItem(PlayerRes.NetId);
				Search->ManualSkillOverride.Mus.AddItem(PlayerRes.Mu);
				Search->ManualSkillOverride.Sigmas.AddItem(PlayerRes.Sigma);
			}
		}
	}
}

/**
 * Determine the maximum team size that can be accomodated based 
 * on the current reservation slots occupied.
 *
 * @return maximum team size that is currently available
 */
INT UPartyBeaconHost::GetMaxAvailableTeamSize()
{
	INT MaxFreeSlots=0;
	// find the largest available free slots within all the teams
	for (INT TeamIdx=0; TeamIdx < NumTeams; TeamIdx++)
	{
		MaxFreeSlots = Max<INT>(MaxFreeSlots,NumPlayersPerTeam - GetNumPlayersOnTeam(TeamIdx));
	}	
	return MaxFreeSlots;
}

/**
 * Cleans up any client connections and then routes to the base class
 */
void UPartyBeaconHost::DestroyBeacon(void)
{
#if WITH_UE3_NETWORKING
	if (Socket)
	{
		// Don't delete if this is during a tick or we'll crash
		if (bIsInTick == FALSE)
		{
			// Destroy each socket in the list and then empty it
			for (INT Index = 0; Index < Clients.Num(); Index++)
			{
				GSocketSubsystem->DestroySocket(Clients(Index).Socket);
			}
			Clients.Empty();
		}
	}
	Super::DestroyBeacon();
#endif
}

/*-----------------------------------------------------------------------------
	UPartyBeaconClient implementation
-----------------------------------------------------------------------------*/

/**
 * Creates a beacon that will send requests to remote hosts
 *
 * @param Addr the address that we are connecting to (needs to be resolved)
 *
 * @return true if the beacon was created successfully, false otherwise
 */
UBOOL UPartyBeaconClient::InitClientBeacon(const FInternetIpAddr& Addr)
{
#if WITH_UE3_NETWORKING
	// Now create and set up our TCP socket
	Socket = GSocketSubsystem->CreateStreamSocket(TEXT("client party beacon"));
	if (Socket != NULL)
	{
		// Set basic state
		Socket->SetReuseAddr();
		Socket->SetNonBlocking();
		// Connect to the remote host so we can send the request
		if (Socket->Connect(Addr))
		{
			ClientBeaconState = PBCS_Connecting;
			return TRUE;
		}
		else
		{
			INT Error = GSocketSubsystem->GetLastErrorCode();
			debugf(NAME_DevBeacon, TEXT("Client Beacon failed to connect: %s"), GSocketSubsystem->GetSocketError(Error));
		}
	}
	// Failed to create the connection
	ClientBeaconState = PBCS_ConnectionFailed;
#endif
	return FALSE;
}

/**
 * Loads the class specified for the Resolver and constructs it if needed
 */
void UPartyBeaconClient::InitResolver(void)
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
				Resolver->BeaconPort = PartyBeaconPort;
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
 * Sends a request to the remote host to allow the specified members to reserve space
 * in the host's session. Note this request is async and the results will be sent via
 * the delegate
 *
 * @param DesiredHost the server that the connection will be made to
 * @param RequestingPartyLeader the leader of this party that will be joining
 * @param Players the list of players that want to reserve space
 *
 * @return true if the request async task started ok, false if it failed to send
 */
UBOOL UPartyBeaconClient::RequestReservation(const FOnlineGameSearchResult& DesiredHost,FUniqueNetId RequestingPartyLeader,const TArray<FPlayerReservation>& Players)
{
	UBOOL bWasStarted = FALSE;
#if WITH_UE3_NETWORKING

	// Make sure the resolver has been initialized
	InitResolver();

	// Register the secure keys so we can decrypt and communicate
	if (Resolver != NULL)
	{
		if (Resolver->RegisterAddress(DesiredHost))
		{
			FInternetIpAddr SendTo;
			// Make sure we can resolve where we are sending to
			if (Resolver->ResolveAddress(DesiredHost,SendTo))
			{
				HostPendingRequest = DesiredHost;
				// Copy the party information
				PendingRequest.PartyLeader = RequestingPartyLeader;
				PendingRequest.PartyMembers = Players;
				if (InitClientBeacon(SendTo))
				{
					// At this point we have a socket to the server and we'll check in tick when the
					// connection is fully established
					bWasStarted = TRUE;
					// Reset the request timeout
					ReservationRequestElapsedTime = 0.f;
					// Type of request that is pending 
					ClientBeaconRequestType = PBClientRequest_NewReservation;
				}
				else
				{
					debugf(NAME_DevBeacon,
						TEXT("Beacon (%s) RequestReservation() failed to init client beacon."),
						*BeaconName.ToString());
				}
			}
			else
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) RequestReservation() failed to resolve party host address"),
					*BeaconName.ToString());
			}
		}
		else
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) RequestReservation() failed to register the party host address"),
				*BeaconName.ToString());
		}
	}
#endif
	// Fire the delegate so the code can process the next items
	if (bWasStarted == FALSE)
	{
		DestroyBeacon();
	}
	return bWasStarted;
}

/**
 * Sends a request to the remote host to update an existing reservation for the
 * specified party leader.  Any new players not already in the party leader's reservation
 * will be added to that reservation on the host.
 *
 * @param DesiredHost the server that the connection will be made to
 * @param RequestingPartyLeader party leader that will be updating his existing reservation
 * @param PlayersToAdd the list of players that want to reserve space in an existing reservation
 *
 * @return true if the request able to be sent, false if it failed to send
 */
UBOOL UPartyBeaconClient::RequestReservationUpdate(const FOnlineGameSearchResult& DesiredHost,FUniqueNetId RequestingPartyLeader,const TArray<FPlayerReservation>& PlayersToAdd)
{
	// create a new pending reservation for these players in the same way as a new reservation request
	UBOOL bWasStarted = RequestReservation(DesiredHost,RequestingPartyLeader,PlayersToAdd);
	if (bWasStarted)
	{
		// Treat the new reservation as an update to an existing reservation on the host
		ClientBeaconRequestType = PBClientRequest_UpdateReservation;
	}
	return bWasStarted;
}

/**
 * Sends a cancellation message to the remote host so that it knows there is more
 * space available
 *
 * @param CancellingPartyLeader the leader of this party that wants to cancel
 *
 * @return true if the request able to be sent, false if it failed to send
 */
UBOOL UPartyBeaconClient::CancelReservation(FUniqueNetId CancellingPartyLeader)
{
	//Stop responding to host packets now that we are canceling our reservation
	bShouldTick = FALSE;
	if (Socket != NULL)
	{
		// Create a buffer and serialize the request in
		FNboSerializeToBuffer ToBuffer(64);
		ToBuffer << (BYTE)RPT_ClientCancellationRequest
			<< CancellingPartyLeader;
		INT BytesSent;
		// Send the message indicating the party that is cancelling
		UBOOL bDidSendOk = Socket->Send(ToBuffer.GetRawBuffer(0),ToBuffer.GetByteCount(),BytesSent);
		if (bDidSendOk == FALSE)
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) failed to send cancel reservation to host with (%s)"),
				*BeaconName.ToString(),
				GSocketSubsystem->GetSocketError());
		}
		return bDidSendOk;
	}
	return FALSE;
}

/**
 * Once the socket has been established, it sends the pending request to the host
 */
void UPartyBeaconClient::SendReservationRequest(void)
{
	// Create a buffer and serialize the request in
	FNboSerializeToBuffer ToBuffer(512);
	// Packet format is <Type><PartyLeader><PartySize><PartyMember1><PartyMemberN>...
	if (ClientBeaconRequestType == PBClientRequest_UpdateReservation)
	{
		// Treated as an update to an existing request
		ToBuffer << (BYTE)RPT_ClientReservationUpdateRequest;
	}
	else
	{
		// Treated as a new request
		ToBuffer << (BYTE)RPT_ClientReservationRequest;
	}
	// Serialize the leader to the buffer
	ToBuffer << PendingRequest.PartyLeader;
	DWORD PartySize = PendingRequest.PartyMembers.Num();
	// Write the number players in the party
	ToBuffer << PartySize;
	// Now serialize each member
	for (INT Index = 0; Index < PendingRequest.PartyMembers.Num(); Index++)
	{
		const FPlayerReservation& PlayerRes = PendingRequest.PartyMembers(Index);
		ToBuffer << PlayerRes;
	}
	INT BytesSent;
	// Now send to the destination host
	if (Socket->Send(ToBuffer.GetRawBuffer(0),ToBuffer.GetByteCount(),BytesSent))
	{
		ClientBeaconState = PBCS_AwaitingResponse;
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) sent party reservation request with (%d) players to (%s)"),
			*BeaconName.ToString(),
			PartySize,
			*Socket->GetAddress().ToString(TRUE));
	}
	else
	{
		// Failed to send, so mark the request as failed
		ClientBeaconState = PBCS_ConnectionFailed;
		debugf(NAME_DevBeacon,
			TEXT("Beacon (%s) SendRequest() failed to send the packet to the host (%s) with error code (%s)"),
			*BeaconName.ToString(),
			*Socket->GetAddress().ToString(TRUE),
			GSocketSubsystem->GetSocketError());
	}
}

/**
 * Routes the response packet received from a host to the correct handler based on its type.
 * Overridden by base implementations to handle custom packet types
 *
 * @param HostResponsePacketType packet ID from EReservationPacketType (or derived version) that represents a host response to this client
 * @param FromBuffer the packet serializer to read from
 * @return TRUE if the data packet type was processed
 */
UBOOL UPartyBeaconClient::HandleHostResponsePacketType(BYTE HostResponsePacketType,FNboSerializeFromBuffer& FromBuffer)
{
	// Route the call to the proper handler
	switch (HostResponsePacketType)
	{
		case RPT_HostReservationResponse:
		{
			ProcessReservationResponse(FromBuffer);
			break;
		}
		case RPT_HostReservationCountUpdate:
		{
			ProcessReservationCountUpdate(FromBuffer);
			break;
		}
		case RPT_HostTravelRequest:
		{
			ProcessTravelRequest(FromBuffer);
			break;
		}
		case RPT_HostIsReady:
		{
			ProcessHostIsReady();
			break;
		}
		case RPT_HostHasCancelled:
		{
			ProcessHostCancelled();
			break;
		}
		case RPT_Heartbeat:
		{
			// Respond to this so they know we are alive
			ProcessHeartbeat();
			break;
		}
		default:
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) unknown packet type received from host (%d)"),
				*BeaconName.ToString(),
				(DWORD)HostResponsePacketType);

			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Processes a packet that was received from the host indicating success or
 * failure for our reservation
 *
 * @param Packet the packet that the host sent
 * @param PacketSize the size of the packet to process
 */
void UPartyBeaconClient::ProcessHostResponse(BYTE* Packet,INT PacketSize)
{
	// Use our packet serializer to read from the raw buffer
	FNboSerializeFromBuffer FromBuffer(Packet,PacketSize);
	// Work through the stream until we've consumed it all
	do
	{
		BYTE PacketType = RPT_UnknownPacketType;
		FromBuffer >> PacketType;
		// Don't process a packet if we are at the end
		if (FromBuffer.HasOverflow() == FALSE)
		{
			// Route the host response based on packet type
			HandleHostResponsePacketType(PacketType,FromBuffer);
		}
	}
	while (FromBuffer.HasOverflow() == FALSE);
}

/**
 * Notifies the delegates that the host is ready to play
 */
void UPartyBeaconClient::ProcessHostIsReady(void)
{
	bShouldTick = FALSE;
	CleanupAddress();
	delegateOnHostIsReady();
}

/**
 * Processes a reservation response packet that was received from the host
 *
 * @param FromBuffer the packet serializer to read from
 */
void UPartyBeaconClient::ProcessReservationResponse(FNboSerializeFromBuffer& FromBuffer)
{
	BYTE Result = PRR_GeneralError;
	// flag timer to < 0 indicated host response was received
	ReservationRequestElapsedTime = -1;
	FromBuffer >> Result;
	INT ReservationRemaining = 0;
	FromBuffer >> ReservationRemaining;
	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) host (%s) response was (%s) with %d reservations remaining"),
		*BeaconName.ToString(),
		*Socket->GetAddress().ToString(TRUE),
		PartyReservationResultToString((EPartyReservationResult)Result),
		ReservationRemaining);
	// Tell the game code the result
	delegateOnReservationRequestComplete((EPartyReservationResult)Result);
}

/**
 * Processes a reservation count update packet that was received from the host
 *
 * @param FromBuffer the packet serializer to read from
 */
void UPartyBeaconClient::ProcessReservationCountUpdate(FNboSerializeFromBuffer& FromBuffer)
{
	INT ReservationsRemaining = 0;
	FromBuffer >> ReservationsRemaining;
	debugf(NAME_DevBeacon,
		TEXT("Beacon (%s) host (%s) reservations remaining is (%d)"),
		*BeaconName.ToString(),
		*Socket->GetAddress().ToString(TRUE),
		ReservationsRemaining);
	// Notify the client of the change
	delegateOnReservationCountUpdated(ReservationsRemaining);
}

/**
 * Processes a travel request packet that was received from the host
 *
 * @param FromBuffer the packet serializer to read from
 */
void UPartyBeaconClient::ProcessTravelRequest(FNboSerializeFromBuffer& FromBuffer)
{
	bShouldTick = FALSE;
	FString SessionNameStr;
	FString ClassNameStr;
	BYTE DestinationInfo[80];
	// Read the two strings first
	FromBuffer >> SessionNameStr >> ClassNameStr;
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
	CleanupAddress();
	// Fire the delegate so the client can process
	delegateOnTravelRequestReceived(SessionName,SearchClass,DestinationInfo);
}

/**
 * Ticks the network layer to see if there are any requests or responses to requests
 *
 * @param DeltaTime the amount of time that has elapsed since the last tick
 */
void UPartyBeaconClient::Tick(FLOAT DeltaTime)
{
#if WITH_UE3_NETWORKING
	// Only tick if we have a socket present
	if (Socket && bShouldTick && bWantsDeferredDestroy == FALSE)
	{
		// Use this to guard against deleting the socket while in use
		bIsInTick = TRUE;
		switch (ClientBeaconState)
		{
			case PBCS_Connecting:
			{
				CheckConnectionStatus();
				break;
			}
			case PBCS_Connected:
			{
				// Send the request to the host and transition to waiting for a response
				SendReservationRequest();				
				break;
			}
			case PBCS_AwaitingResponse:
			{
				// Increment the time since we've last seen a heartbeat
				ElapsedHeartbeatTime += DeltaTime;
				// Read and process any host packets
				ReadResponse();
				// Make sure we haven't processed a packet saying to travel
				if (bShouldTick && bWantsDeferredDestroy == FALSE)
				{
					// Check to see if we've lost the host
					if (ElapsedHeartbeatTime > HeartbeatTimeout ||
						ClientBeaconState == PBCS_ConnectionFailed)
					{
						ProcessHostCancelled();
					}
				}
				break;
			}
		}
		// If < 0 then already received a host response, else waiting for a host response
		if (ReservationRequestElapsedTime >= 0.f)
		{
			ReservationRequestElapsedTime += DeltaTime;
			// Check for a timeout or an error
			if (ReservationRequestElapsedTime > ReservationRequestTimeout ||
				ClientBeaconState == PBCS_ConnectionFailed)
			{
				debugf(NAME_DevBeacon,
					TEXT("Beacon (%s) timeout waiting for host to accept our connection"),
					*BeaconName.ToString());
				ProcessHostTimeout();
			}
		}
		// No longer a danger, so release the sentinel on the destruction
		bIsInTick = FALSE;
	}
	Super::Tick(DeltaTime);
#endif
}

/**
 * Handles checking for the transition from connecting to connected (socket established)
 */
void UPartyBeaconClient::CheckConnectionStatus(void)
{
	ESocketConnectionState State = Socket->GetConnectionState();
	if (State == SCS_Connected)
	{
		ClientBeaconState = PBCS_Connected;
	}
	else if (State == SCS_ConnectionError)
	{
		INT SocketErrorCode = GSocketSubsystem->GetLastErrorCode();
		// Continue to allow the session to be established for EWOULDBLOCK (not an error)
		if (SocketErrorCode != SE_EWOULDBLOCK)
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) error connecting to host (%s) with error (%s)"),
				*BeaconName.ToString(),
				*Socket->GetAddress().ToString(TRUE),
				GSocketSubsystem->GetSocketError());
			ClientBeaconState = PBCS_ConnectionFailed;
		}
	}
}

/**
 * Checks the socket for a response from the and processes if present
 */
void UPartyBeaconClient::ReadResponse(void)
{
	UBOOL bShouldRead = TRUE;
	BYTE PacketData[512];
	// Read each pending packet and pass it out for processing
	while (bShouldRead && bShouldTick && bWantsDeferredDestroy == FALSE)
	{
		INT BytesRead = 0;
		// Read the response from the socket
		if (Socket->Recv(PacketData,512,BytesRead))
		{
			if (BytesRead > 0)
			{
				ProcessHostResponse(PacketData,BytesRead);
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
					TEXT("Beacon (%s) socket error (%s) detected trying to read host response from (%s)"),
					*BeaconName.ToString(),
					GSocketSubsystem->GetSocketError(ErrorCode),
					*Socket->GetAddress().ToString(TRUE));
				ClientBeaconState = PBCS_ConnectionFailed;
			}
			bShouldRead = FALSE;
		}
	}
}

/**
 * Stops listening for requests/responses and releases any allocated memory
 */
void UPartyBeaconClient::DestroyBeacon(void)
{
	if (bIsInTick == FALSE)
	{
		CleanupAddress();
	}
	// Clean up the socket now too
	Super::DestroyBeacon();
}

/** Unregisters the address and zeros the members involved to prevent multiple releases */
void UPartyBeaconClient::CleanupAddress(void)
{
	// Release any memory associated with communicating with the host
	if( Resolver != NULL )
	{
		if (!Resolver->UnregisterAddress(HostPendingRequest))
		{
			debugf(NAME_DevBeacon,
				TEXT("Beacon (%s) CleanupAddress() failed to unregister the party host address"),
				*BeaconName.ToString());
		}
	}

	// Zero since this was a shallow copy
	HostPendingRequest.GameSettings = NULL;
	HostPendingRequest.PlatformData = NULL;
	ClientBeaconState = PBCS_Closed;
}

/**
 * Processes a heartbeat update, sends a heartbeat back, and clears the timer
 */
void UPartyBeaconClient::ProcessHeartbeat(void)
{
	ElapsedHeartbeatTime = 0.f;
	if (Socket != NULL)
	{
		// Notify the host that we received their heartbeat
		SendHeartbeat(Socket);
	}
}

#endif //WITH_UE3_NETWORKING
