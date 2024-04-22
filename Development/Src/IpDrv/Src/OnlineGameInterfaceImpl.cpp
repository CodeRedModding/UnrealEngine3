/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(UOnlineGameInterfaceImpl);

/**
 * Ticks this object to update any async tasks
 *
 * @param DeltaTime the time since the last tick
 */
void UOnlineGameInterfaceImpl::Tick(FLOAT DeltaTime)
{
	// Tick lan tasks
	TickLanTasks(DeltaTime);
	// Tick any internet tasks
	TickInternetTasks(DeltaTime);
}

/**
 * Determines if the packet header is valid or not
 *
 * @param Packet the packet data to check
 * @param Length the size of the packet buffer
 *
 * @return true if the header is valid, false otherwise
 */
UBOOL UOnlineGameInterfaceImpl::IsValidLanQueryPacket(const BYTE* Packet,
	DWORD Length,QWORD& ClientNonce)
{
	ClientNonce = 0;
	UBOOL bIsValid = FALSE;
	// Serialize out the data if the packet is the right size
	if (Length == LAN_BEACON_PACKET_HEADER_SIZE)
	{
		FNboSerializeFromBuffer PacketReader(Packet,Length);
		BYTE Version = 0;
		PacketReader >> Version;
		// Do the versions match?
		if (Version == LAN_BEACON_PACKET_VERSION)
		{
			BYTE Platform = 255;
			PacketReader >> Platform;
			// Can we communicate with this platform?
			if (Platform & LanPacketPlatformMask)
			{
				INT GameId = -1;
				PacketReader >> GameId;
				// Is this our game?
				if (GameId == LanGameUniqueId)
				{
					BYTE SQ1 = 0;
					PacketReader >> SQ1;
					BYTE SQ2 = 0;
					PacketReader >> SQ2;
					// Is this a server query?
					bIsValid = (SQ1 == LAN_SERVER_QUERY1 && SQ2 == LAN_SERVER_QUERY2);
					// Read the client nonce as the outvalue
					PacketReader >> ClientNonce;
				}
			}
		}
	}
	return bIsValid;
}

/**
 * Determines if the packet header is valid or not
 *
 * @param Packet the packet data to check
 * @param Length the size of the packet buffer
 *
 * @return true if the header is valid, false otherwise
 */
UBOOL UOnlineGameInterfaceImpl::IsValidLanResponsePacket(const BYTE* Packet,DWORD Length)
{
	UBOOL bIsValid = FALSE;
	// Serialize out the data if the packet is the right size
	if (Length > LAN_BEACON_PACKET_HEADER_SIZE)
	{
		FNboSerializeFromBuffer PacketReader(Packet,Length);
		BYTE Version = 0;
		PacketReader >> Version;
		// Do the versions match?
		if (Version == LAN_BEACON_PACKET_VERSION)
		{
			BYTE Platform = 255;
			PacketReader >> Platform;
			// Can we communicate with this platform?
			if (Platform & LanPacketPlatformMask)
			{
				INT GameId = -1;
				PacketReader >> GameId;
				// Is this our game?
				if (GameId == LanGameUniqueId)
				{
					BYTE SQ1 = 0;
					PacketReader >> SQ1;
					BYTE SQ2 = 0;
					PacketReader >> SQ2;
					// Is this a server response?
					if (SQ1 == LAN_SERVER_RESPONSE1 && SQ2 == LAN_SERVER_RESPONSE2)
					{
						QWORD Nonce = 0;
						PacketReader >> Nonce;
						bIsValid = Nonce == (QWORD&)LanNonce;
					}
				}
			}
		}
	}
	return bIsValid;
}

/**
 * Ticks any lan beacon background tasks
 *
 * @param DeltaTime the time since the last tick
 */
void UOnlineGameInterfaceImpl::TickLanTasks(FLOAT DeltaTime)
{
	if (LanBeaconState > LANB_NotUsingLanBeacon && LanBeacon != NULL)
	{
		BYTE PacketData[512];
		UBOOL bShouldRead = TRUE;
		// Read each pending packet and pass it out for processing
		while (bShouldRead)
		{
			INT NumRead = LanBeacon->ReceivePacket(PacketData,512);
			if (NumRead > 0)
			{
				// Hand this packet off to child classes for processing
				ProcessLanPacket(PacketData,NumRead);
				// Reset the timeout since a packet came in
				LanQueryTimeLeft = LanQueryTimeout;
			}
			else
			{
				if (LanBeaconState == LANB_Searching)
				{
					// Decrement the amount of time remaining
					LanQueryTimeLeft -= DeltaTime;
					// Check for a timeout on the search packet
					if (LanQueryTimeLeft <= 0.f)
					{
						// Stop future timeouts since we aren't searching any more
						StopLanBeacon();
						if (GameSearch != NULL)
						{
							GameSearch->bIsSearchInProgress = FALSE;
						}
						// Trigger the delegate so the UI knows we didn't find any
						FAsyncTaskDelegateResults Params(S_OK);
						TriggerOnlineDelegates(this,FindOnlineGamesCompleteDelegates,&Params);
					}
				}
				bShouldRead = FALSE;
			}
		}
	}
}

/**
 * Adds the game settings data to the packet that is sent by the host
 * in reponse to a server query
 *
 * @param Packet the writer object that will encode the data
 * @param InGameSettings the game settings to add to the packet
 */
void UOnlineGameInterfaceImpl::AppendGameSettingsToPacket(FNboSerializeToBuffer& Packet,
	UOnlineGameSettings* InGameSettings)
{
#if DEBUG_LAN_BEACON
	debugf(NAME_DevOnline,TEXT("Sending game settings to client"));
#endif
	// Members of the game settings class
	Packet << InGameSettings->NumOpenPublicConnections
		<< InGameSettings->NumOpenPrivateConnections
		<< InGameSettings->NumPublicConnections
		<< InGameSettings->NumPrivateConnections
		<< (BYTE)InGameSettings->bShouldAdvertise
		<< (BYTE)InGameSettings->bIsLanMatch
		<< (BYTE)InGameSettings->bUsesStats
		<< (BYTE)InGameSettings->bAllowJoinInProgress
		<< (BYTE)InGameSettings->bAllowInvites
		<< (BYTE)InGameSettings->bUsesPresence
		<< (BYTE)InGameSettings->bAllowJoinViaPresence
		<< (BYTE)InGameSettings->bUsesArbitration
#if WITH_STEAMWORKS
		<< (BYTE)InGameSettings->bAntiCheatProtected;
#else
		;
#endif
	// Write the player id so we can show gamercard
	Packet << InGameSettings->OwningPlayerId;
	Packet << InGameSettings->OwningPlayerName;
	// Now add the contexts and properties from the settings class
	// First, add the number contexts involved
	INT Num = InGameSettings->LocalizedSettings.Num();
	Packet << Num;
	// Now add each context individually
	for (INT Index = 0; Index < InGameSettings->LocalizedSettings.Num(); Index++)
	{
		Packet << InGameSettings->LocalizedSettings(Index);
#if DEBUG_LAN_BEACON
		debugf(NAME_DevOnline,*BuildContextString(InGameSettings,InGameSettings->LocalizedSettings(Index)));
#endif
	}
	// Next, add the number of properties involved
	Num = InGameSettings->Properties.Num();
	Packet << Num;
	// Now add each property
	for (INT Index = 0; Index < InGameSettings->Properties.Num(); Index++)
	{
		Packet << InGameSettings->Properties(Index);
#if DEBUG_LAN_BEACON
		debugf(NAME_DevOnline,*BuildPropertyString(InGameSettings,InGameSettings->Properties(Index)));
#endif
	}
}

/**
 * Reads the game settings data from the packet and applies it to the
 * specified object
 *
 * @param Packet the reader object that will read the data
 * @param InGameSettings the game settings to copy the data to
 */
void UOnlineGameInterfaceImpl::ReadGameSettingsFromPacket(FNboSerializeFromBuffer& Packet,
	UOnlineGameSettings* InGameSettings)
{
#if DEBUG_LAN_BEACON
	debugf(NAME_DevOnline,TEXT("Reading game settings from server"));
#endif
	// Members of the game settings class
	Packet >> InGameSettings->NumOpenPublicConnections
		>> InGameSettings->NumOpenPrivateConnections
		>> InGameSettings->NumPublicConnections
		>> InGameSettings->NumPrivateConnections;
	BYTE Read = FALSE;
	// Read all the bools as bytes
	Packet >> Read;
	InGameSettings->bShouldAdvertise = Read == TRUE;
	Packet >> Read;
	InGameSettings->bIsLanMatch = Read == TRUE;
	Packet >> Read;
	InGameSettings->bUsesStats = Read == TRUE;
	Packet >> Read;
	InGameSettings->bAllowJoinInProgress = Read == TRUE;
	Packet >> Read;
	InGameSettings->bAllowInvites = Read == TRUE;
	Packet >> Read;
	InGameSettings->bUsesPresence = Read == TRUE;
	Packet >> Read;
	InGameSettings->bAllowJoinViaPresence = Read == TRUE;
	Packet >> Read;
	InGameSettings->bUsesArbitration = Read == TRUE;
#if WITH_STEAMWORKS
	Packet >> Read;
	InGameSettings->bAntiCheatProtected = Read == TRUE;
#endif
	// Read the owning player id
	Packet >> InGameSettings->OwningPlayerId;
	// Read the owning player name
	Packet >> InGameSettings->OwningPlayerName;
#if DEBUG_LAN_BEACON
	QWORD Uid = (QWORD&)InGameSettings->OwningPlayerId.Uid;
	debugf(NAME_DevOnline,TEXT("%s 0x%016I64X"),*InGameSettings->OwningPlayerName,Uid);
#endif
	// Now read the contexts and properties from the settings class
	INT NumContexts = 0;
	// First, read the number contexts involved, so we can presize the array
	Packet >> NumContexts;
	if (Packet.HasOverflow() == FALSE)
	{
		InGameSettings->LocalizedSettings.Empty(NumContexts);
		InGameSettings->LocalizedSettings.AddZeroed(NumContexts);
	}
	// Now read each context individually
	for (INT Index = 0;
		Index < InGameSettings->LocalizedSettings.Num() && Packet.HasOverflow() == FALSE;
		Index++)
	{
		Packet >> InGameSettings->LocalizedSettings(Index);
#if DEBUG_LAN_BEACON
		debugf(NAME_DevOnline,*BuildContextString(InGameSettings,InGameSettings->LocalizedSettings(Index)));
#endif
	}
	INT NumProps = 0;
	// Next, read the number of properties involved for array presizing
	Packet >> NumProps;
	if (Packet.HasOverflow() == FALSE)
	{
		InGameSettings->Properties.Empty(NumProps);
		InGameSettings->Properties.AddZeroed(NumProps);
	}
	// Now read each property from the packet
	for (INT Index = 0;
		Index < InGameSettings->Properties.Num() && Packet.HasOverflow() == FALSE;
		Index++)
	{
		Packet >> InGameSettings->Properties(Index);
#if DEBUG_LAN_BEACON
		debugf(NAME_DevOnline,*BuildPropertyString(InGameSettings,InGameSettings->Properties(Index)));
#endif
	}
	// If there was an overflow, treat the string settings/properties as broken
	if (Packet.HasOverflow())
	{
		InGameSettings->LocalizedSettings.Empty();
		InGameSettings->Properties.Empty();
		debugf(NAME_DevOnline,TEXT("Packet overflow detected in ReadGameSettingsFromPacket()"));
	}
}

/**
 * Builds a LAN query and broadcasts it
 *
 * @return an error/success code
 */
DWORD UOnlineGameInterfaceImpl::FindLanGames(void)
{
	// Recreate the unique identifier for this client
	GenerateNonce(LanNonce,8);
	// Create the lan beacon if we don't already have one
	DWORD Return = StartLanBeacon();
	// If we have a socket and a nonce, broadcast a discovery packet
	if (LanBeacon && Return == S_OK)
	{
		QWORD Nonce = *(QWORD*)LanNonce;
		FNboSerializeToBuffer Packet(LAN_BEACON_MAX_PACKET_SIZE);
		// Build the discovery packet
		Packet << LAN_BEACON_PACKET_VERSION
			// Platform information
			<< (BYTE)appGetPlatformType()
			// Game id to prevent cross game lan packets
			<< LanGameUniqueId
			// Identify the packet type
			<< LAN_SERVER_QUERY1 << LAN_SERVER_QUERY2
			// Append the nonce as a QWORD
			<< Nonce;
		// Now kick off our broadcast which hosts will respond to
		if (LanBeacon->BroadcastPacket(Packet,Packet.GetByteCount()))
		{
#if DEBUG_LAN_BEACON
			debugf(NAME_DevOnline,TEXT("Sent query packet..."));
#endif
			// We need to poll for the return packets
			LanBeaconState = LANB_Searching;
			// Set the timestamp for timing out a search
			LanQueryTimeLeft = LanQueryTimeout;
			GameSearch->bIsSearchInProgress = TRUE;

			// Don't fire the search results delegate until a match has come back or timed out
			Return = ERROR_IO_PENDING;
		}
		else
		{
			debugf(NAME_Error,TEXT("Failed to send discovery broadcast %s"),
				GSocketSubsystem->GetSocketError());
			Return = E_FAIL;
		}
	}
	if (Return != ERROR_IO_PENDING)
	{
		delete LanBeacon;
		LanBeacon = NULL;
		LanBeaconState = LANB_NotUsingLanBeacon;
	}
	return Return;
}

/**
 * Cancels the current search in progress if possible for that search type
 *
 * @return true if successful searching for sessions, false otherwise
 */
UBOOL UOnlineGameInterfaceImpl::CancelFindOnlineGames(void)
{
	DWORD Return = E_FAIL;
	if (GameSearch != NULL &&
		GameSearch->bIsSearchInProgress)
	{
		// Lan and internet are handled differently
		if (GameSearch->bIsLanQuery)
		{
			Return = S_OK;
			StopLanBeacon();
			GameSearch->bIsSearchInProgress = FALSE;
		}
		else
		{
			// Trying to cancel an internet game search, so let the child class handle it
			Return = CancelFindInternetGames();
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Can't cancel a search that isn't in progress"));
	}
	if (Return != ERROR_IO_PENDING)
	{
		FAsyncTaskDelegateResults Results(Return);
		TriggerOnlineDelegates(this,CancelFindOnlineGamesCompleteDelegates,&Results);
	}
	return Return == S_OK || Return == ERROR_IO_PENDING;
}

/**
 * Creates an online game based upon the settings object specified.
 * NOTE: online game registration is an async process and does not complete
 * until the OnCreateOnlineGameComplete delegate is called.
 *
 * @param HostingPlayerNum the index of the player hosting the match
 * @param SessionName the name of the session being created
 * @param NewGameSettings the settings to use for the new game session
 *
 * @return true if successful creating the session, false otherwsie
 */
UBOOL UOnlineGameInterfaceImpl::CreateOnlineGame(BYTE HostingPlayerNum,FName SessionName,UOnlineGameSettings* NewGameSettings)
{
	check(OwningSubsystem && "Was this object created and initialized properly?");
	DWORD Return = E_FAIL;
	// Don't set if we already have a session going
	if (GameSettings == NULL)
	{
		GameSettings = NewGameSettings;
		if (GameSettings != NULL)
		{
			check(SessionInfo == NULL);
			// Allow for per platform override of the session info
			SessionInfo = CreateSessionInfo();
			// Init the game settings counts so the host can use them later
			GameSettings->NumOpenPrivateConnections = GameSettings->NumPrivateConnections;
			GameSettings->NumOpenPublicConnections = GameSettings->NumPublicConnections;
			// Copy the unique id of the owning player
			GameSettings->OwningPlayerId = OwningSubsystem->eventGetPlayerUniqueNetIdFromIndex(HostingPlayerNum);
			// Copy the name of the owning player
			GameSettings->OwningPlayerName = AGameReplicationInfo::StaticClass()->GetDefaultObject<AGameReplicationInfo>()->ServerName;
			if ( GameSettings->OwningPlayerName.Len() == 0 )
			{
				GameSettings->OwningPlayerName = OwningSubsystem->eventGetPlayerNicknameFromIndex(HostingPlayerNum);
			}
			// Determine if we are registering a session on our master server or
			// via lan
			if (GameSettings->bIsLanMatch == FALSE)
			{
				Return = CreateInternetGame(HostingPlayerNum);
			}
			else
			{
				// Lan match so do any beacon creation
				Return = CreateLanGame(HostingPlayerNum);
			}

			// Update the game state if successful
			if (Return == S_OK || Return == ERROR_IO_PENDING)
			{
				GameSettings->GameState = OGS_Pending;
			}
		}
		else
		{
			debugf(NAME_Error,TEXT("Can't create an online session with null game settings"));
		}
	}
	else
	{
		debugf(NAME_Error,TEXT("Can't create a new online session when one is in progress: %s"), *(GameSettings->GetPathName()));
	}

	if (Return != ERROR_IO_PENDING)
	{
		FAsyncTaskDelegateResultsNamedSession Params(SessionName,Return);
		TriggerOnlineDelegates(this,CreateOnlineGameCompleteDelegates,&Params);
	}
	return Return == S_OK || Return == ERROR_IO_PENDING;
}

/**
 * Creates a new lan enabled game
 *
 * @param HostingPlayerNum the player hosting the game
 *
 * @return S_OK if it succeeded, otherwise an error code
 */
DWORD UOnlineGameInterfaceImpl::CreateLanGame(BYTE HostingPlayerNum)
{
	check(SessionInfo);
	DWORD Return = E_FAIL;

	if (GameSettings != NULL)
	{
		// Don't create a lan beacon if advertising is off
		if (GameSettings->bShouldAdvertise == TRUE)
		{
			Return = StartLanBeacon();
		}

		if (Return == S_OK)
		{
			GameSettings->GameState = OGS_Pending;
		}
	}

	if (Return == S_OK)
	{
		// Register all local talkers
		RegisterLocalTalkers();
	}
	else
	{
		// Clean up the session info so we don't get into a confused state
		delete SessionInfo;
		SessionInfo = NULL;
		GameSettings = NULL;
	}
	return Return;
}

/**
 * Destroys the current online game
 * NOTE: online game de-registration is an async process and does not complete
 * until the OnDestroyOnlineGameComplete delegate is called.
 *
 * @param SessionName the name of the session being destroyed
 *
 * @return true if successful destroying the session, false otherwsie
 */
UBOOL UOnlineGameInterfaceImpl::DestroyOnlineGame(FName SessionName)
{
	DWORD Return = E_FAIL;
	// Don't shut down if it isn't valid
	if (GameSettings != NULL && SessionInfo != NULL)
	{
		// Stop all local talkers (avoids a debug runtime warning)
		UnregisterLocalTalkers();
		// Stop all remote voice before ending the session
		RemoveAllRemoteTalkers();
		// Determine if this is a lan match or our master server
		if (GameSettings->bIsLanMatch == FALSE)
		{
			Return = DestroyInternetGame();
		}
		else
		{
			Return = DestroyLanGame();
		}

		// Update the game state if successful
		if (GameSettings != NULL && (Return == S_OK || Return == ERROR_IO_PENDING))
		{
			GameSettings->GameState = OGS_NoSession;
		}
	}
	else
	{
		debugf(NAME_Error,TEXT("Can't destroy a null online session"));
	}

	if (Return != ERROR_IO_PENDING)
	{
		// Fire the delegate off immediately
		FAsyncTaskDelegateResultsNamedSession Params(SessionName,Return);
		TriggerOnlineDelegates(this,DestroyOnlineGameCompleteDelegates,&Params);
	}
	return Return == S_OK || Return == ERROR_IO_PENDING;
}

/**
 * Terminates a LAN session
 *
 * @return an error/success code
 */
DWORD UOnlineGameInterfaceImpl::DestroyLanGame(void)
{
	check(SessionInfo);
	// Only tear down the beacon if it was advertising
	if (GameSettings->bShouldAdvertise)
	{
		// Tear down the lan beacon
		StopLanBeacon();
	}
	// Clean up before firing the delegate
	delete SessionInfo;
	SessionInfo = NULL;
	// Null out the no longer valid game settings
	GameSettings = NULL;
	return S_OK;
}

/**
 * Searches for games matching the settings specified
 *
 * @param SearchingPlayerNum the index of the player searching for a match
 * @param SearchSettings the desired settings that the returned sessions will have
 *
 * @return true if successful searching for sessions, false otherwise
 */
UBOOL UOnlineGameInterfaceImpl::FindOnlineGames(BYTE SearchingPlayerNum,UOnlineGameSearch* SearchSettings)
{
	DWORD Return = E_FAIL;
	// Verify that we have valid search settings
	if (SearchSettings != NULL)
	{
		// Don't start another while in progress or multiple entries for the
		// same server will show up in the server list
		if ((GameSearch && GameSearch->bIsSearchInProgress == FALSE) ||
			GameSearch == NULL)
		{
			// Free up previous results, if present
			if (SearchSettings->Results.Num())
			{
				FreeSearchResults(SearchSettings);
			}
			GameSearch = SearchSettings;
			// Check for master server or lan query
			if (SearchSettings->bIsLanQuery == FALSE)
			{
				Return = FindInternetGames();
			}
			else
			{
				Return = FindLanGames();
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Ignoring game search request while one is pending"));
			Return = ERROR_IO_PENDING;
		}
	}
	else
	{
		debugf(NAME_Error,TEXT("Can't search with null criteria"));
	}
	if (Return != ERROR_IO_PENDING)
	{
		// Fire the delegate off immediately
		FAsyncTaskDelegateResults Params(Return);
		TriggerOnlineDelegates(this,FindOnlineGamesCompleteDelegates,&Params);
	}
	return Return == S_OK || Return == ERROR_IO_PENDING;
}

/**
 * Parses a LAN packet and handles responses/search population
 * as needed
 *
 * @param PacketData the packet data to parse
 * @param PacketLength the amount of data that was received
 */
void UOnlineGameInterfaceImpl::ProcessLanPacket(BYTE* PacketData,INT PacketLength)
{
	// Check our mode to determine the type of allowed packets
	if (LanBeaconState == LANB_Hosting)
	{
		// Don't respond to queries when the match is full
		if (GameSettings->NumOpenPublicConnections > 0)
		{
			QWORD ClientNonce;
			// We can only accept Server Query packets
			if (IsValidLanQueryPacket(PacketData,PacketLength,ClientNonce))
			{
				FNboSerializeToBuffer Packet(LAN_BEACON_MAX_PACKET_SIZE);
				// Add the supported version
				Packet << LAN_BEACON_PACKET_VERSION
					// Platform information
					<< (BYTE)appGetPlatformType()
					// Game id to prevent cross game lan packets
					<< LanGameUniqueId
					// Add the packet type
					<< LAN_SERVER_RESPONSE1 << LAN_SERVER_RESPONSE2
					// Append the client nonce as a QWORD
					<< ClientNonce;
				// Write host info (ip and port)
				Packet << SessionInfo->HostAddr;
				// Now append per game settings
				AppendGameSettingsToPacket(Packet,GameSettings);
				// Broadcast this response so the client can see us
				if (LanBeacon->BroadcastPacket(Packet,Packet.GetByteCount()) == FALSE)
				{
					debugf(NAME_Error,TEXT("Failed to send response packet %d"),
						GSocketSubsystem->GetLastErrorCode());
				}
			}
		}
	}
	else if (LanBeaconState == LANB_Searching)
	{
		// We can only accept Server Response packets
		if (IsValidLanResponsePacket(PacketData,PacketLength))
		{
			// Create an object that we'll copy the data to
			UOnlineGameSettings* NewServer = ConstructObject<UOnlineGameSettings>(
				GameSearch->GameSettingsClass);
			if (NewServer != NULL)
			{
				// Add space in the search results array
				INT NewSearch = GameSearch->Results.Add();
				FOnlineGameSearchResult& Result = GameSearch->Results(NewSearch);
				// Link the settings to this result
				Result.GameSettings = NewServer;
				// Strip off the header since it's been validated
				FNboSerializeFromBuffer Packet(&PacketData[LAN_BEACON_PACKET_HEADER_SIZE],
					PacketLength - LAN_BEACON_PACKET_HEADER_SIZE);
				// Allocate and read the session data
				FSessionInfo* SessInfo = new FSessionInfo(E_NoInit);
				// Read the connection data
				Packet >> SessInfo->HostAddr;
				// Store this in the results
				Result.PlatformData = SessInfo;
				// Read any per object data using the server object
				ReadGameSettingsFromPacket(Packet,NewServer);
				// Let any registered consumers know the data has changed
				FAsyncTaskDelegateResults Params(S_OK);
				TriggerOnlineDelegates(this,FindOnlineGamesCompleteDelegates,&Params);
			}
			else
			{
				debugf(NAME_Error,TEXT("Failed to create new online game settings object"));
			}
		}
	}
}

/**
 * Cleans up the Live specific session data contained in the search results
 *
 * @param Search the object to free the previous results from
 *
 * @return TRUE if it could, FALSE otherwise
 */
UBOOL UOnlineGameInterfaceImpl::FreeSearchResults(UOnlineGameSearch* Search)
{
	UBOOL bDidFree = FALSE;
	// If they didn't pass on object in, they meant for us to use the current one
	if (Search == NULL)
	{
		Search = GameSearch;
	}
	if (Search != NULL)
	{
		if (Search->bIsSearchInProgress == FALSE)
		{
			// Loop through the results freeing the session info pointers
			for (INT Index = 0; Index < Search->Results.Num(); Index++)
			{
				FOnlineGameSearchResult& Result = Search->Results(Index);
				if (Result.PlatformData != NULL)
				{
					// Free the data and clear the leak detection flag
					delete (FSessionInfo*)Result.PlatformData;
				}
			}
			Search->Results.Empty();
			bDidFree = TRUE;
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Can't free search results while the search is in progress"));
		}
	}
	return bDidFree;
}

/**
 * Joins the game specified
 *
 * @param PlayerNum the index of the player searching for a match
 * @param SessionName the name of the session being joined
 * @param DesiredGame the desired game to join
 *
 * @return true if the call completed successfully, false otherwise
 */
UBOOL UOnlineGameInterfaceImpl::JoinOnlineGame(BYTE PlayerNum,FName SessionName,const FOnlineGameSearchResult& DesiredGame)
{
	DWORD Return = E_FAIL;
	// Don't join a session if already in one or hosting one
	if (SessionInfo == NULL)
	{
		// Make the selected game our current game settings
		GameSettings = DesiredGame.GameSettings;

		if (GameSettings != NULL)
		{
			Return = S_OK;

			// Create an empty session and fill it based upon game type
			SessionInfo = CreateSessionInfo();
			// Copy the session info over
			// @note: clang warns about overwriting the vtable here, the (void*) cast allows it
			appMemcpy((void*)SessionInfo,DesiredGame.PlatformData,GetSessionInfoSize());
			// The session info is created/filled differently depending on type
			if (GameSettings->bIsLanMatch == FALSE)
			{
				Return = JoinInternetGame(PlayerNum);
			}
			else
			{
				// Register all local talkers for voice
				RegisterLocalTalkers();
				FAsyncTaskDelegateResultsNamedSession Params(SessionName,S_OK);
				TriggerOnlineDelegates(this,JoinOnlineGameCompleteDelegates,&Params);
			}

			// Update the game state if successful
			if (Return == S_OK || Return == ERROR_IO_PENDING)
			{
				// Set the state so that StartOnlineGame() can work
				GameSettings->GameState = OGS_Pending;
			}
		}

		// Handle clean up in one place
		if (Return != S_OK && Return != ERROR_IO_PENDING)
		{
			// Clean up the session info so we don't get into a confused state
			delete SessionInfo;
			SessionInfo = NULL;
			GameSettings = NULL;
		}
	}

	if (Return != ERROR_IO_PENDING)
	{
		FAsyncTaskDelegateResultsNamedSession Params(SessionName,Return);
		TriggerOnlineDelegates(this,JoinOnlineGameCompleteDelegates,&Params);
	}
	debugf(NAME_DevOnline, TEXT("JoinOnlineGame  Return:0x%08X"), Return);
	return Return == S_OK || Return == ERROR_IO_PENDING;
}

/**
 * Marks an online game as in progress (as opposed to being in lobby)
 *
 * @param SessionName the name of the session that is being started
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineGameInterfaceImpl::StartOnlineGame(FName SessionName)
{
	DWORD Return = E_FAIL;
	if (GameSettings != NULL && SessionInfo != NULL)
	{
		// Lan matches don't report starting to external services
		if (GameSettings->bIsLanMatch == FALSE)
		{
			// Can't start a match multiple times
			if (GameSettings->GameState == OGS_Pending || GameSettings->GameState == OGS_Ended)
			{
				Return = StartInternetGame();
			}
			else
			{
				debugf(NAME_Error, TEXT("Can't start an online game in state %i"), GameSettings->GameState);
			}
		}
		else
		{
			// If this lan match has join in progress disabled, shut down the beacon
			if (GameSettings->bAllowJoinInProgress == FALSE)
			{
				StopLanBeacon();
			}
			Return = S_OK;
		}

		// Update the game state if successful
		if (Return == S_OK || Return == ERROR_IO_PENDING)
		{
			GameSettings->GameState = OGS_InProgress;
		}
	}
	else
	{
		debugf(NAME_Error,TEXT("Can't start an online game that hasn't been created"));
	}

	if (Return != ERROR_IO_PENDING)
	{
		// Indicate that the start completed
		FAsyncTaskDelegateResultsNamedSession Params(SessionName,Return);
		TriggerOnlineDelegates(this,StartOnlineGameCompleteDelegates,&Params);
	}
	return Return == S_OK || Return == ERROR_IO_PENDING;
}

/**
 * Marks an online game as having been ended
 *
 * @param SessionName the name of the session being ended
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineGameInterfaceImpl::EndOnlineGame(FName SessionName)
{
	DWORD Return = E_FAIL;
	if (GameSettings != NULL && SessionInfo != NULL)
	{
		// Lan matches don't report ending to master server
		if (GameSettings->bIsLanMatch == FALSE)
		{
			// Can't end a match that isn't in progress
			if (GameSettings->GameState == OGS_InProgress)
			{
				Return = EndInternetGame();
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("Can't end an online game in state %i"), GameSettings->GameState);
			}
		}
		else
		{
			Return = S_OK;
			// If the session should be advertised and the lan beacon was destroyed, recreate
			if (GameSettings->bShouldAdvertise &&
				LanBeacon == NULL)
			{
				// Recreate the beacon
				Return = StartLanBeacon();
			}
		}

		GameSettings->GameState = (Return == ERROR_IO_PENDING ? OGS_Ending : OGS_Ended);
	}
	else
	{
		debugf(NAME_Error,TEXT("Can't end an online game that hasn't been created"));
	}

	// Fire delegates off if not async or in error
	if (Return != ERROR_IO_PENDING)
	{
		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResultsNamedSession Params(SessionName,Return);
		TriggerOnlineDelegates(this,EndOnlineGameCompleteDelegates,&Params);
	}
	return Return == S_OK || Return == ERROR_IO_PENDING;
}

/**
 * Returns the platform specific connection information for joining the match.
 * Call this function from the delegate of join completion
 *
 * @param SessionName the name of the session that is being resolved
 * @param ConnectInfo the out var containing the platform specific connection information
 *
 * @return true if the call was successful, false otherwise
 */
UBOOL UOnlineGameInterfaceImpl::GetResolvedConnectString(FName SessionName,FString& ConnectInfo)
{
	UBOOL bOk = FALSE;
	if (SessionInfo != NULL)
	{
		// Copy the destination IP and port information
		ConnectInfo = SessionInfo->HostAddr.ToString(TRUE);
		bOk = TRUE;
	}
	else
	{
		debugf(NAME_Error,TEXT("Can't decrypt a NULL session's IP"));
	}
	return bOk;
}

/**
 * Serializes the platform specific data into the provided buffer for the specified search result
 *
 * @param DesiredGame the game to copy the platform specific data for
 * @param PlatformSpecificInfo the buffer to fill with the platform specific information
 *
 * @return true if successful serializing the data, false otherwise
 */
UBOOL UOnlineGameInterfaceImpl::ReadPlatformSpecificSessionInfo(const FOnlineGameSearchResult& DesiredGame,BYTE* PlatformSpecificInfo)
{
	DWORD Return = E_FAIL;
	if (DesiredGame.GameSettings && DesiredGame.PlatformData)
	{
		if (DesiredGame.GameSettings->bIsLanMatch == FALSE)
		{
			Return = ReadPlatformSpecificInternetSessionInfo(DesiredGame,PlatformSpecificInfo);
		}
		else
		{
			FNboSerializeToBuffer Buffer(80);
			FSessionInfo* SessionInfo = (FSessionInfo*)DesiredGame.PlatformData;
			// Write the connection data
			Buffer << SessionInfo->HostAddr;
			if (Buffer.GetByteCount() <= 80)
			{
				// Copy the built up data
				appMemcpy(PlatformSpecificInfo,Buffer.GetRawBuffer(0),Buffer.GetByteCount());
				Return = S_OK;
			}
			else
			{
				debugf(NAME_DevOnline,
					TEXT("Platform data is larger (%d) than the supplied buffer (80)"),
					Buffer.GetByteCount());
			}
		}
	}
	return Return == S_OK;
}

/**
 * Creates a search result out of the platform specific data and adds that to the specified search object
 *
 * @param SearchingPlayerNum the index of the player searching for a match
 * @param SearchSettings the desired search to bind the session to
 * @param PlatformSpecificInfo the platform specific information to convert to a server object
 *
 * @return true if successful searching for sessions, false otherwise
 */
UBOOL UOnlineGameInterfaceImpl::BindPlatformSpecificSessionToSearch(BYTE SearchingPlayerNum,UOnlineGameSearch* SearchSettings,BYTE* PlatformSpecificInfo)
{
	DWORD Return = E_FAIL;
	// Verify that we have valid search settings
	if (SearchSettings != NULL)
	{
		// Don't start another while in progress or multiple entries for the
		// same server will show up in the server list
		if ((GameSearch && GameSearch->bIsSearchInProgress == FALSE) ||
			GameSearch == NULL)
		{
			// Free up previous results, if present
			if (SearchSettings->Results.Num())
			{
				FreeSearchResults(SearchSettings);
			}
			GameSearch = SearchSettings;
			// Check for master server or lan query
			if (SearchSettings->bIsLanQuery == FALSE)
			{
				Return = BindPlatformSpecificSessionToInternetSearch(SearchingPlayerNum,SearchSettings,PlatformSpecificInfo);
			}
			else
			{
				// Create an object that we'll copy the data to
				UOnlineGameSettings* NewServer = ConstructObject<UOnlineGameSettings>(
					GameSearch->GameSettingsClass);
				if (NewServer != NULL)
				{
					// Add space in the search results array
					INT NewSearch = GameSearch->Results.Add();
					FOnlineGameSearchResult& Result = GameSearch->Results(NewSearch);
					// Link the settings to this result
					Result.GameSettings = NewServer;
					// Allocate and read the session data
					FSessionInfo* SessInfo = new FSessionInfo(E_NoInit);
					// Read the serialized data from the buffer
					FNboSerializeFromBuffer Buffer(PlatformSpecificInfo,64);
					// Read the connection data
					Buffer >> SessInfo->HostAddr;
					// Store this in the results
					Result.PlatformData = SessInfo;
					Return = S_OK;
				}
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Ignoring game bind to search request while a search is pending"));
		}
	}
	else
	{
		debugf(NAME_Error,TEXT("Can't bind to a search that is null"));
	}
	return Return == S_OK;
}

#endif	//#if WITH_UE3_NETWORKING
