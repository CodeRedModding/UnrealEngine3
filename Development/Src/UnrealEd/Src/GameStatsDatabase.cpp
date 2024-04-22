/*=============================================================================
	GameStatsDatabase.cpp: Implementation of a game stats database 
	for fast access of game stats data
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "GameFrameworkGameStatsClasses.h"
#include "UnrealEdGameStatsClasses.h"
#include "GameplayEventsUtilities.h"
#include "GameStatsDatabaseTypes.h"

//Warn helper
#ifndef warnexprf
#define warnexprf(expr, msg) { if(!(expr)) warnf(msg); }
#endif

IMPLEMENT_CLASS(UGameStatsDatabase);
IMPLEMENT_CLASS(UGameStatsFileReader);
IMPLEMENT_CLASS(UGameStatsDatabaseVisitor);
IMPLEMENT_CLASS(UGameStatsVisitorImpl);

/** 
 *   Mappings of data values to the database column names in the SQL query 
 */
#define DBEventName					TEXT("EventName")
#define DBEventID					TEXT("EventID")
#define DBEventTime					TEXT("TimeOffset")
#define DBIntValue					TEXT("IntData0")
#define DBFloatValue				TEXT("FloatData0")
#define DBStringValue				TEXT("StringData0")

#define DBPositionX					TEXT("Position0_X")
#define DBPositionY					TEXT("Position0_Y")
#define DBPositionZ					TEXT("Position0_Z")
#define DBOrientationYaw			TEXT("Orientation0_Yaw")
#define DBOrientationPitch			TEXT("Orientation0_Pitch")
#define DBOrientationRoll			TEXT("Orientation0_Roll")

#define DBTeamIndex					TEXT("Index0")
#define DBPlayerIndex				TEXT("Index0")
#define DBPlayerName				TEXT("PlayerName")

#define DBTargetIndex				TEXT("Index1")
#define DBTargetName				TEXT("TargetName")
#define DBTargetPositionX			TEXT("Position1_X")
#define DBTargetPositionY			TEXT("Position1_Y")
#define DBTargetPositionZ			TEXT("Position1_Z")
#define DBTargetOrientationYaw		TEXT("Orientation1_Yaw")
#define DBTargetOrientationPitch	TEXT("Orientation1_Pitch")
#define DBTargetOrientationRoll		TEXT("Orientation1_Roll")

#define DBWeaponClassIndex			TEXT("IntData0")
#define DBWeaponClassName			TEXT("StaticString")
#define DBWeaponClassValue			TEXT("IntData1")
#define DBDamageClassIndex			TEXT("IntData0")
#define DBDamageClassName			TEXT("StaticString")
#define DBDamageClassValue			TEXT("IntData1")
#define DBKillTypeIndex				TEXT("IntData1")
#define DBKillTypeName				TEXT("KillTypeName")
#define DBProjectileClassIndex		TEXT("IntData0")
#define DBProjectileClassName		TEXT("StaticString")
#define DBProjectileClassValue		TEXT("IntData1")
#define DBPawnIndex					TEXT("IntData0")
#define DBPawnName					TEXT("StaticString")
#define DBPawnTeamIndex				TEXT("Index1")

/** Temporary global pointer to allow database entries access to metadata */
UGameplayEventsReader* GGameStatsFileReader = NULL;
UGameStatsFileReader* GGameStatsHandler = NULL;

/** 
 *   Parse a sessionID of the form <GUID>:<INSTANCE> into its constituent parts
 * @param SessionID - session to parse
 * @param SessionGUID - Session GUID 
 * @param InstanceID - Session Instance
 * @return TRUE on success, FALSE otherwise
 */
UBOOL ParseSessionID(const FString& SessionID, FString& SessionGUID, INT& InstanceID)
{
	INT SpacerIdx = SessionID.InStr(TEXT(":"));
	if (SpacerIdx != INDEX_NONE)
	{
		SessionGUID = SessionID.Left(SpacerIdx);
		FString DBIDString = SessionID.Right(SessionID.Len() - SpacerIdx - 1);
		InstanceID = appAtoi(*DBIDString);
		return TRUE;
	}
	else
	{
		debugf(TEXT("Error parsing session ID %s"), *SessionID);
		return FALSE;
	}
}

/** Helper for returning the event name from the file reader */
FString GetEventNameFromIndex(INT EventIndex)
{
	if (GGameStatsFileReader != NULL && EventIndex != INDEX_NONE)
	{													
		const FGameplayEventMetaData& EventData = GGameStatsFileReader->GetEventMetaData(EventIndex);
		return EventData.EventName.ToString(); 
	}
	else
	{
		return TEXT("UNKNOWN");
	}
}

/** Helper for returning the player name from the file reader */
FString GetPlayerNameFromIndex(INT PlayerIndex)
{
	if (GGameStatsFileReader != NULL && PlayerIndex != INDEX_NONE)
	{
		const FPlayerInformation& PlayerInfo = GGameStatsFileReader->GetPlayerMetaData(PlayerIndex);
		return PlayerInfo.PlayerName; 
	}
	else
	{
		return TEXT("UNKNOWN");
	}
}

/** Helper for returning the pawn name from the file reader */
FString GetPawnNameFromIndex(INT PawnIndex)
{
	if (GGameStatsFileReader != NULL && PawnIndex != INDEX_NONE)
	{													
		const FPawnClassEventData& PawnData = GGameStatsFileReader->GetPawnMetaData(PawnIndex);
		return PawnData.PawnClassName.ToString(); 
	}
	else
	{
		return TEXT("UNKNOWN");
	}
}

/** Helper for returning the weapon name from the file reader */
FString GetWeaponNameFromIndex(INT WeaponIndex)
{
	if (GGameStatsFileReader != NULL && WeaponIndex != INDEX_NONE)
	{													
		const FWeaponClassEventData& WeaponData = GGameStatsFileReader->GetWeaponMetaData(WeaponIndex);
		return WeaponData.WeaponClassName.ToString(); 
	}
	else
	{
		return TEXT("UNKNOWN");
	}
}

/** Helper for returning the projectile name from the file reader */
FString GetProjectileNameFromIndex(INT ProjectileIndex)
{
	if (GGameStatsFileReader != NULL && ProjectileIndex != INDEX_NONE)
	{													
		const FProjectileClassEventData& ProjectileData = GGameStatsFileReader->GetProjectileMetaData(ProjectileIndex);
		return ProjectileData.ProjectileClassName.ToString(); 
	}
	else
	{
		return TEXT("UNKNOWN");
	}
}

/** Helper for returning the damageclass name from the file reader */
FString GetDamageNameFromIndex(INT DamageIndex)
{
	if (GGameStatsFileReader != NULL && DamageIndex != INDEX_NONE)
	{													
		const FDamageClassEventData& DamageData = GGameStatsFileReader->GetDamageMetaData(DamageIndex);
		return DamageData.DamageClassName.ToString(); 
	}
	else
	{
		return TEXT("UNKNOWN");
	}
}

/**
 * Helper for returning the actor name from the file reader
 *
 * @param ActorIndex the actor index to look up
 *
 * @return The name of the string if found
 */
FString GetActorNameFromIndex(INT ActorIndex)
{
	if (GGameStatsFileReader != NULL && ActorIndex != INDEX_NONE)
	{													
		return GGameStatsFileReader->GetActorMetaData(ActorIndex);
	}
	return FString(TEXT("UNKNOWN"));
}

/** 
 * Clear all data stored within
 */
void UGameStatsFileReader::Cleanup()
{
	//Delete the cached data
	AllEvents.Empty();
	SessionData.Empty();
}


/** 
 * Adds a new event created to the array of all events in the file 
 * @param NewEvent - new event to add
 * @param TeamIndex - Team Index for team events (INDEX_NONE if not a team event)
 * @param PlayerIndex - Player Index for player events (INDEX_NONE if not a player event)
 * @param TargetIndex - Target Index for player events (INDEX_NONE if event has no target)
 */
void UGameStatsFileReader::AddNewEvent(struct FIGameStatEntry* NewEvent, INT TeamIndex, INT PlayerIndex, INT TargetIndex)
{
	if (NewEvent)
	{
		//Normalize the time [0, SessionDuration], clamping shouldn't be necessary
		NewEvent->EventTime = Clamp(NewEvent->EventTime - Reader->GetSessionStart(), 0.0f, Reader->GetSessionDuration());

		//add the event to the "database"
		INT NewEventIndex = AllEvents.AddItem(NewEvent);
		NewEventIndex += EventsOffset;

		//Add to the game session
		SessionData.AllEvents.AddItem(NewEventIndex);

		//Add to the event id mapping
		SessionData.EventsByType.Add(NewEvent->EventID, NewEventIndex);

		if (GameState->bIsRoundStarted)
		{
			//Add to the round mapping
			SessionData.EventsByRound.Add(GameState->RoundNumber, NewEventIndex);
		}

		if (TeamIndex != INDEX_NONE)
		{
			//Add to the team mapping
			SessionData.EventsByTeam.Add(TeamIndex, NewEventIndex);
		}

		if (PlayerIndex != INDEX_NONE)
		{
			//Add to the player mapping
			SessionData.EventsByPlayer.Add(PlayerIndex, NewEventIndex);
		}

		if (TargetIndex != INDEX_NONE)
		{
			//Add to the player mapping
			SessionData.EventsByPlayer.Add(TargetIndex, NewEventIndex);
		}

		if (TeamIndex == INDEX_NONE && PlayerIndex == INDEX_NONE)
		{
			SessionData.GameEvents.AddUniqueItem(NewEventIndex);
		}
	}
}

/*
 *   Set the game state this aggregator will use
 * @param InGameState - game state object to use
 */
void UGameStatsFileReader::SetGameState(UGameStateObject* InGameState)
{
	GameState = InGameState;
}

void UGameStatsFileReader::HandleEvent(FGameEventHeader& GameEvent, IGameEvent* GameEventData)
{
	if (IsEventFiltered(GameEvent.EventID))
	{
		return;
	}

	INT PlayerIndex = INDEX_NONE;
	INT TargetIndex = INDEX_NONE;
	INT TeamIndex = INDEX_NONE;
	FIGameStatEntry* NewGameEntry = NULL;

	//Process the event
	switch(GameEvent.EventType)
	{
	case GET_GameString:
		{												   
			NewGameEntry = new GameStringEntry(GameEvent, (FGameStringEvent*)GameEventData);
			break;
		}
	case GET_GameInt:
		{
			NewGameEntry = new GameIntEntry(GameEvent, (FGameIntEvent*)GameEventData);
			break;
		}
	case GET_GameFloat:
		{
			NewGameEntry = new GameFloatEntry(GameEvent, (FGameFloatEvent*)GameEventData);
			break;
		}
	case GET_GamePosition:
		{
			NewGameEntry = new GamePositionEntry(GameEvent, (FGamePositionEvent*)GameEventData);
			break;
		}
	case GET_TeamInt:
		{
			NewGameEntry = new TeamIntEntry(GameEvent, (FTeamIntEvent*)GameEventData);
			TeamIndex = static_cast<TeamIntEntry*>(NewGameEntry)->TeamIndex;
			break;
		}
	case GET_TeamString:
		{
			NewGameEntry = new TeamStringEntry(GameEvent, (FTeamStringEvent*)GameEventData);
			TeamIndex = static_cast<TeamStringEntry*>(NewGameEntry)->TeamIndex;
			break;
		}
	case GET_TeamFloat:
		{
			NewGameEntry = new TeamFloatEntry(GameEvent, (FTeamFloatEvent*)GameEventData);
			TeamIndex = static_cast<TeamFloatEntry*>(NewGameEntry)->TeamIndex;
			break;
		}
	case GET_PlayerString:
		{
			NewGameEntry = new PlayerStringEntry(GameEvent, (FPlayerStringEvent*)GameEventData);
			PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
			break;
		}
	case GET_PlayerInt:
		{
			NewGameEntry = new PlayerIntEntry(GameEvent, (FPlayerIntEvent*)GameEventData);
			PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
			break;
		}
	case GET_PlayerFloat:
		{
			NewGameEntry = new PlayerFloatEntry(GameEvent, (FPlayerFloatEvent*)GameEventData);
			PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
			break;
		}
	case GET_PlayerSpawn:
		{
			NewGameEntry = new PlayerSpawnEntry(GameEvent, (FPlayerSpawnEvent*)GameEventData);
			PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
			break;
		}
	case GET_PlayerLogin:
		{
			NewGameEntry = new PlayerLoginEntry(GameEvent, (FPlayerLoginEvent*)GameEventData);
			PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
			break;
		}
	case GET_PlayerLocationPoll:
		{
			FPlayerLocationsEvent* LocationEvent = (FPlayerLocationsEvent*)GameEventData;

			FPlayerIntEvent TempEvent;
			for (INT PlayerIdx = 0; PlayerIdx < LocationEvent->PlayerLocations.Num(); PlayerIdx++)
			{
				FPlayerLocation& PlayerLoc = LocationEvent->PlayerLocations(PlayerIdx);

				TempEvent.PlayerIndexAndYaw = PlayerLoc.PlayerIndexAndYaw;
				TempEvent.PlayerPitchAndRoll = PlayerLoc.PlayerPitchAndRoll;
				TempEvent.Location = PlayerLoc.Location;
				TempEvent.Value = 0;

				NewGameEntry = new PlayerIntEntry(GameEvent, &TempEvent);
				PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
				AddNewEvent(NewGameEntry, TeamIndex, PlayerIndex, INDEX_NONE);
			}

			return;
		}
	case GET_PlayerKillDeath:
		{
			NewGameEntry = new PlayerKillDeathEntry(GameEvent, (FPlayerKillDeathEvent*)GameEventData);
			PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
			// TargetIndex is DEATH event, handled separately
			break;				   
		}
	case GET_PlayerPlayer:
		{
			NewGameEntry = new PlayerPlayerEntry(GameEvent, (FPlayerPlayerEvent*)GameEventData);
			PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
			TargetIndex = static_cast<PlayerPlayerEntry*>(NewGameEntry)->Target.PlayerIndex;
			break;
		}
	case GET_WeaponInt:
		{
			NewGameEntry = new WeaponEntry(GameEvent, (FWeaponIntEvent*)GameEventData);
			PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
			break;
		}
	case GET_DamageInt:
		{	
			NewGameEntry = new DamageEntry(GameEvent, (FDamageIntEvent*)GameEventData);
			PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
			TargetIndex = static_cast<DamageEntry*>(NewGameEntry)->Target.PlayerIndex;
			break;
		}
	case GET_ProjectileInt:
		{
			NewGameEntry = new ProjectileIntEntry(GameEvent, (FProjectileIntEvent*)GameEventData);
			PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
			break;
		}
	case GET_GenericParamList:
		{
			NewGameEntry = new GenericParamListEntry(GameEvent, (FGenericParamListEvent*)GameEventData);
			if( !static_cast<GenericParamListEntry*>(NewGameEntry)->GetNamedParamData<INT>(TEXT("PlayerIndex"),PlayerIndex))
			{
				PlayerIndex = 0;
			}
			break;
		}
	default:
		debugf(NAME_GameStats,TEXT("Unknown game stat type %d"),GameEvent.EventType);
		return;
	}

	if (NewGameEntry != NULL)
	{
		AddNewEvent(NewGameEntry, TeamIndex, PlayerIndex, TargetIndex);
	}
}

void UGameStatsDatabase::Init(const FString& MapName, BYTE DateFilter)
{
	if (RemoteDB == NULL)
	{
		//@TODO handle user specified connection
		RemoteDB = new FGameStatsRemoteDB();
		if (!RemoteDB->IsConnected())
		{
			delete RemoteDB;
			RemoteDB = NULL;
		}
	}

	LoadRemoteData(MapName, (GameStatsDateFilters)DateFilter);
	LoadLocalData(MapName, (GameStatsDateFilters)DateFilter);
}

/** 
 *   Creates the archive that we are going to be manipulating 
 * @param Filename - name of the file that will be open for serialization
 * @return TRUE if successful, else FALSE
 */
UBOOL UGameStatsDatabase::OpenStatsFile(const FString& Filename)
{
	check(GGameStatsFileReader);
	check(GGameStatsHandler);

	//Open the file
	return GGameStatsFileReader->OpenStatsFile(Filename);
}

/*
 *   Iterate over all valid files in the stats directory and create a mapping of map name to filename
 */
void UGameStatsDatabase::CacheLocalFilenames()
{
	//Find all relevant files
	TArray<FString> GameStatsFilesInUse;
	TArray<FString> FilesFound;
	FString StatsDir;
	GetStatsDirectory(StatsDir);

	appFindFilesInDirectory(FilesFound, *StatsDir, FALSE, TRUE);
	for (INT FileIdx = 0; FileIdx < FilesFound.Num(); FileIdx++)
	{
		const FString& File = FilesFound(FileIdx);
		if (File.InStr(GAME_STATS_FILE_EXT, TRUE, TRUE) >= 0)
		{
			GameStatsFilesInUse.AddItem(File);
		}
	}

	if (GameStatsFilesInUse.Num() > 0)
	{
		UGameplayEventsReader* FileReader = ConstructObject<UGameplayEventsReader>(UGameplayEventsReader::StaticClass());
		if (FileReader != NULL)
		{
			GGameStatsFileReader = FileReader;

			// Load the files in question
			for (INT GameFileIndex = 0; GameFileIndex < GameStatsFilesInUse.Num(); GameFileIndex++)
			{
				const FString& GameFilename = GameStatsFilesInUse(GameFileIndex);
				if (FileReader->OpenStatsFile(GameFilename) == TRUE)
				{
					//Since we were successful in opening the file, save off the map it contains data for future lookups
					MapNameToFilenameMapping.AddUnique(FileReader->CurrentSessionInfo.MapName, GameFilename);
					FileReader->CloseStatsFile();
				}
			}

			GGameStatsFileReader = NULL;
		}
	}
}

/** Searches the stats directory for relevant data files and populates the database */
void UGameStatsDatabase::LoadLocalData(const FString& MapName, GameStatsDateFilters DateFilter)
{
	//Iterate over all files related to this map
	TArray<FString> GameStatsFilesInUse;
	MapNameToFilenameMapping.MultiFind(MapName, GameStatsFilesInUse);
	if (GameStatsFilesInUse.Num() > 0)
	{
		UClass* GameStatsFileReaderClass = LoadClass<UGameStatsFileReader>(NULL, *GameStatsFileReaderClassname, NULL, LOAD_None, NULL);
		if (GameStatsFileReaderClass != NULL)
		{
			UGameplayEventsReader* FileReader = ConstructObject<UGameplayEventsReader>(UGameplayEventsReader::StaticClass());
			UGameStatsFileReader* StatHandler = ConstructObject<UGameStatsFileReader>(GameStatsFileReaderClass);
			if (StatHandler != NULL && StatHandler->IsA(UGameStatsFileReader::StaticClass()))
			{
				UClass* GameStateClass = LoadClass<UGameStateObject>(NULL, *GameStateClassname, NULL, LOAD_None, NULL);
				if (GameStateClass != NULL)
				{
					UGameStateObject* GameState = ConstructObject<UGameStateObject>(GameStateClass);
					check(GameState);
					StatHandler->SetGameState(GameState);

					// Put state first so other handlers can make use of it
					FileReader->eventRegisterHandler(GameState);
					FileReader->eventRegisterHandler(StatHandler);
					GGameStatsFileReader = FileReader;
					GGameStatsHandler = StatHandler;

					// Load the files in question
					for (INT GameFileIndex = 0; GameFileIndex < GameStatsFilesInUse.Num(); GameFileIndex++)
					{
						const FString& GameFilename = GameStatsFilesInUse(GameFileIndex);
						if (OpenStatsFile(GameFilename) == TRUE)
						{
							const FString& SessionID = FileReader->GetSessionID();

							//Only process if we haven't already
							if (SessionInfoBySessionID.Find(SessionID) == NULL)
							{
								//Make sure it hasn't already been uploaded to the remote db
								if (RemoteDB == NULL || RemoteDB->DoesSessionExistInDB(FileReader->CurrentSessionInfo.GameplaySessionID) == FALSE)
								{
									//Copy metadata from file
									AllGameTypes.AddUniqueItem(FileReader->CurrentSessionInfo.GameClassName);
									SessionInfoBySessionID.Set(SessionID, FileReader->CurrentSessionInfo);
									PlayerListBySessionID.Set(SessionID, FileReader->PlayerList);
									TeamListBySessionID.Set(SessionID, FileReader->TeamList);

									SupportedEventsBySessionID.Set(SessionID, FileReader->SupportedEvents);
									WeaponClassesBySessionID.Set(SessionID, FileReader->WeaponClassArray);
									DamageClassesBySessionID.Set(SessionID, FileReader->DamageClassArray);
									ProjectileClassesBySessionID.Set(SessionID, FileReader->ProjectileClassArray);
									PawnClassesBySessionID.Set(SessionID, FileReader->PawnClassArray);

									SessionFilenamesBySessionID.Set(SessionID, FileReader->StatsFileName);

									//Process the data
									StatHandler->EventsOffset = AllEvents.Num();
									FileReader->ProcessStream();

									//Add the data to the database
									AllSessions.Set(SessionID, StatHandler->SessionData);
									AllEvents.Append(StatHandler->AllEvents);
									StatHandler->Cleanup();
								}
							}

							FileReader->CloseStatsFile();
						}
					}

					GGameStatsHandler = NULL;
					GGameStatsFileReader = NULL;
					FileReader->eventUnregisterHandler(GameState);
					FileReader->eventUnregisterHandler(StatHandler);
				}
				else
				{
					debugf(TEXT("Invalid classname %s when creating game stats database state object."), *GameStateClassname);

				}
			}
		}
	}
}

/** Connects to the remote database and populates the local db with data */
void UGameStatsDatabase::LoadRemoteData(const FString& MapName, GameStatsDateFilters DateFilter)
{
	if (HasRemoteConnection())
	{
		RemoteDB->Init(MapName, DateFilter);
	}
}

/*
* Upload a given session ID to the master database
* @SessionID - session ID to upload
* @return - TRUE for success, FALSE for any error condition
*/
UBOOL UGameStatsDatabase::UploadSession(const FString& SessionID)
{
	UBOOL bSuccess = FALSE;

	const FString* SessionFilename = SessionFilenamesBySessionID.Find(SessionID);

	//Early out on missing files or already existing uploads
	if (!SessionFilename || SessionFilename->Len() <= 0 || (RemoteDB && RemoteDB->DoesSessionExistInDB(SessionID)))
	{
		return FALSE;
	}

	// Upload system deprecated as too slow for now
	return bSuccess;
}

void UGameStatsDatabase::ClearDatabase()
{
	for (INT i=0; i<AllEvents.Num(); i++)
	{
		delete AllEvents(i);
	}
	AllEvents.Empty();

	AllGameTypes.Empty();

	AllSessions.Empty();
	SessionInfoBySessionID.Empty();
	SessionFilenamesBySessionID.Empty();

	PlayerListBySessionID.Empty();
	TeamListBySessionID.Empty();
	SupportedEventsBySessionID.Empty();
	WeaponClassesBySessionID.Empty();
	DamageClassesBySessionID.Empty();
	ProjectileClassesBySessionID.Empty();
	PawnClassesBySessionID.Empty();

	if (HasRemoteConnection())
	{
	   	RemoteDB->Reset();
	}
}

/** 
* Query this database
* @param SearchQuery - the query to run on the database (do not mix the ALL_PLAYERS/ALL_TEAMS flags with player indices from the same session)
* @param Events - out array of indices of relevant events in the database
* @return the number of results found for this query
*/
INT UGameStatsDatabase::QueryDatabase(const FGameStatsSearchQuery& Query, FGameStatsRecordSet& RecordSet)
{
	// Clear results
	RecordSet.LocalRecordSet.Empty();
	RecordSet.RemoteRecordSet.Empty();

	// Remote query
	if (HasRemoteConnection())
	{
		RemoteDB->QueryDatabase(Query, RecordSet.RemoteRecordSet);
	}

	// Local query
	if (Query.SessionIDs.Num() == 0)
	{
		return 0;
	}

	// Sessions that have no filter also need game stats
	TArray<FString> SessionIdsNeedingGameStats = Query.SessionIDs;
	
	// Accumulate player events
	TArray<INT> PlayerEvents;
	for (INT PlayerIndex=0; PlayerIndex < Query.PlayerIndices.Num(); PlayerIndex++)
	{
		const FSessionIndexPair& Player = Query.PlayerIndices(PlayerIndex);
		GetEventsByPlayer(Player.SessionId, Player.Index, PlayerEvents);
		if (Player.Index != FGameStatsSearchQuery::ALL_PLAYERS)
		{
			SessionIdsNeedingGameStats.RemoveItem(Player.SessionId);
		}
	}

	// Accumulate team events
	TArray<INT> TeamEvents;
	for (INT TeamIndex=0; TeamIndex<Query.TeamIndices.Num(); TeamIndex++)
	{
		const FSessionIndexPair& Team = Query.TeamIndices(TeamIndex);
		GetEventsByTeam(Team.SessionId, Team.Index, TeamEvents);
		if (Team.Index != FGameStatsSearchQuery::ALL_TEAMS)
		{
			SessionIdsNeedingGameStats.RemoveItem(Team.SessionId);
		}
	}

	// Accumulate game events
	TArray<INT> GameEvents;
	for (INT GameIndex=0; GameIndex<SessionIdsNeedingGameStats.Num(); GameIndex++)
	{
		GetGameEventsBySession(SessionIdsNeedingGameStats(GameIndex), GameEvents);
	}

	// Create a unique array of entries from all game/team/player possibilities
	TSet<INT> UniqueEntries;
	for (INT PlayerEventIdx=0; PlayerEventIdx<PlayerEvents.Num(); PlayerEventIdx++)
	{
		UniqueEntries.Add(PlayerEvents(PlayerEventIdx));
	}

	for (INT TeamEventIdx=0; TeamEventIdx<TeamEvents.Num(); TeamEventIdx++)
	{
		UniqueEntries.Add(TeamEvents(TeamEventIdx));
	}

	for (INT GameEventIdx=0; GameEventIdx<GameEvents.Num(); GameEventIdx++)
	{
		UniqueEntries.Add(GameEvents(GameEventIdx));
	}
	
	// Remove entries based on time and event id
	UBOOL bAllEvents = Query.EventIDs.FindItemIndex(FGameStatsSearchQuery::ALL_EVENTS) == INDEX_NONE ? FALSE : TRUE;
	for(TSet<INT>::TIterator EntryIt = TSet<INT>::TIterator(UniqueEntries); EntryIt; ++EntryIt)
	{
		const FIGameStatEntry* Entry = AllEvents(*EntryIt);

		// Filter the queries added to results by time and event
		if ((Query.StartTime <= Entry->EventTime && Entry->EventTime <= Query.EndTime) &&
			(bAllEvents || (Query.EventIDs.ContainsItem(Entry->EventID))))
		{
			RecordSet.LocalRecordSet.AddItem(*EntryIt);
		}
	}

	return (RecordSet.LocalRecordSet.Num() + RecordSet.RemoteRecordSet.Num());
}

/**
 *   Fill in database structures for a given sessionID if necessary
 * @param SessionID - session to cache data for
 */
void UGameStatsDatabase::PopulateSessionData(const FString& SessionID)
{
	//Remote
	if (HasRemoteConnection())
	{
		FString SessionGUID;
		INT SessionDBID = 0;
		if (ParseSessionID(SessionID, SessionGUID, SessionDBID))
		{
			RemoteDB->PopulateSessionData(SessionDBID);
		}
	}
}

/*
 * Get the gametypes in the database
 * @param GameTypes - array of all gametypes in the database
 */
void UGameStatsDatabase::GetGameTypes(TArray<FString>& GameTypes)
{
	GameTypes.Empty();

	GameTypes.Append(AllGameTypes);

	if (HasRemoteConnection())
	{
		TArray<FString> TempGameTypes;
		RemoteDB->GetGameTypes(TempGameTypes);
		for (INT i=0; i<TempGameTypes.Num(); i++)
		{
			GameTypes.AddUniqueItem(TempGameTypes(i));
		}
	}
}

/*
 * Get the session ids in the database
 * @param DateFilter - Date enum to filter by
 * @param GameTypeFilter - GameClass name or "ANY"
 * @param SessionIDs - array of all sessions in the database
 */
void UGameStatsDatabase::GetSessionIDs(BYTE DateFilter, const FString& GameTypeFilter, TArray<FString>& SessionIDs)
{
	SessionIDs.Empty();

	//Local
	UBOOL bFilterGameType = appStricmp(*GameTypeFilter, TEXT("ANY")) != 0;
	for(TMap<FString, FGameSessionInformation>::TIterator SessionIter = TMap<FString, FGameSessionInformation>::TIterator(SessionInfoBySessionID); SessionIter; ++SessionIter)
	{
		const FGameSessionInformation& SessionInfo = SessionIter.Value();
		
		// Gametype Filtering
		if (!bFilterGameType || appStricmp(*GameTypeFilter, *SessionInfo.GameClassName) == 0)
		{
			// Date Filtering
			INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
			appSystemTime(Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);
			INT NumSecondsElapsed = ((Hour * 60 + Min) * 60) + Sec;

			time_t CurTime = appStrToSeconds(*appUtcTimeString());
			time_t SessionTime = appStrToSeconds(*SessionInfo.GameplaySessionTimestamp);

			CurTime -= NumSecondsElapsed;
			switch (DateFilter)
			{
			case GSDF_Today:
				break;
			case GSDF_Last3Days:
				// Today + 2 days time
				CurTime -= (2*24*60*60); 
				break;
			case GSDF_LastWeek:
				// Today + 6 days time
				CurTime -= (7*24*60*60); 
				break;
			}

			if (CurTime < SessionTime)
			{
				SessionIDs.AddItem(SessionIter.Key());
			}
		}
	}

	//Remote
	if (HasRemoteConnection())
	{		
		TArray<FString> TempSessionIDs;
		RemoteDB->GetSessionIDs(DateFilter, GameTypeFilter, TempSessionIDs);
		SessionIDs.Append(TempSessionIDs);
	}
}

/*
 *   Is the session ID found in a local file or in the remote database
 * @param SessionID - session of interest
 * @return TRUE if this data is from a file on disk, FALSE if remote database
 */
UBOOL UGameStatsDatabase::IsSessionIDLocal(const FString& SessionID)
{
   return SessionFilenamesBySessionID.HasKey(SessionID);
}

void UGameStatsDatabase::GetSessionInfoBySessionID(const FString& SessionId, FGameSessionInformation& OutSessionInfo)
{
	//Local
	const FGameSessionInformation* GameSessionInfo = SessionInfoBySessionID.Find(SessionId);
	if (GameSessionInfo != NULL)
	{
		OutSessionInfo = *GameSessionInfo;
	}
	else
	{			
		//Remote
		if (HasRemoteConnection()) 
		{
			FString SessionGUID;
			INT SessionDBID = 0;
			if (ParseSessionID(SessionId, SessionGUID, SessionDBID))
			{
				RemoteDB->GetSessionInfoBySessionDBID(SessionDBID, OutSessionInfo);
			}
		}
	}
}

/*
 * Get the total count of events of a given type
 * @param SessionID - session we're interested in
 * @param EventID - the event to return the count for
 */
INT UGameStatsDatabase::GetEventCountByType(const FString& SessionID, INT EventID)
{
	INT Count = 0;
	if (AllSessions.HasKey(SessionID))
	{
		const FGameSessionEntry* GameSession = AllSessions.Find(SessionID);
		Count = GameSession->EventsByType.Num(EventID);
	}
	else
	{
		if (HasRemoteConnection())
		{
			Count = RemoteDB->GetEventCountByType(SessionID, EventID);
		}
	}

	return Count;
}	

/*
* Get all events associated with a given team
* @param SessionID - session we're interested in
* @param TeamIndex - the team to return the events for
* @param Events - array of indices related to relevant team events
*/
INT UGameStatsDatabase::GetEventsBySessionID(const FString& SessionID, TArray<INT>& Events)
{
	INT Count = 0;
	if (AllSessions.HasKey(SessionID))
	{
		const FGameSessionEntry* GameSession = AllSessions.Find(SessionID);
		Events.Append(GameSession->AllEvents);
		Count = GameSession->AllEvents.Num();
	}

	return Count;
}

/*
 * Get all game events associated with a given session (neither player nor team)
 * @param SessionID - session we're interested in
 * @param Events - array of indices related to relevant game events
 */
INT UGameStatsDatabase::GetGameEventsBySession(const FString& SessionID, TArray<INT>& Events)
{
	INT Count = 0;
	if (AllSessions.HasKey(SessionID))
	{
		const FGameSessionEntry* GameSession = AllSessions.Find(SessionID);
		Events.Append(GameSession->GameEvents);
		Count = GameSession->GameEvents.Num();
	}

	return Count;
}

/*
 * Get all events associated with a given team
 * @param SessionID - session we're interested in
 * @param TeamIndex - the team to return the events for	(INDEX_NONE is all teams)
 * @param Events - array of indices related to relevant team events
 */
INT UGameStatsDatabase::GetEventsByTeam(const FString& SessionID, INT TeamIndex, TArray<INT>& Events)
{
	INT Count = 0;
	if (AllSessions.HasKey(SessionID))
	{
		const FGameSessionEntry* GameSession = AllSessions.Find(SessionID);
		if (TeamIndex == INDEX_NONE)
		{
			TArray<INT> TempArray;
			GameSession->EventsByTeam.GenerateValueArray(TempArray);
			Events.Append(TempArray);
		}
		else
		{
			GameSession->EventsByTeam.MultiFind(TeamIndex, Events);
		}

		Count = Events.Num();
	}

	return Count;
}

/*
* Get all events associated with a given player
* @param SessionID - session we're interested in
* @param PlayerIndex - the player to return the events for (INDEX_NONE is all players)
* @param Events - array of indices related to relevant player events
*/
INT UGameStatsDatabase::GetEventsByPlayer(const FString& SessionID, INT PlayerIndex, TArray<INT>& Events)
{
	INT Count = 0;
	if (AllSessions.HasKey(SessionID))
	{
		const FGameSessionEntry* GameSession = AllSessions.Find(SessionID);
		if (PlayerIndex == INDEX_NONE)
		{
			TArray<INT> TempArray;
			GameSession->EventsByPlayer.GenerateValueArray(TempArray);
			Events.Append(TempArray);
		}
		else
		{
			GameSession->EventsByPlayer.MultiFind(PlayerIndex, Events);
		}

		Count = Events.Num();
	}

	return Count;
}

/*
* Get all events associated with a given round
* @param SessionID - session we're interested in
* @param RoundNumber - the round to return events for (INDEX_NONE is all rounds)
* @param Events - array of indices related to relevant round events
*/
INT UGameStatsDatabase::GetEventsByRound(const FString& SessionID, INT RoundNumber, TArray<INT>& Events)
{
	INT Count = 0;
	if (AllSessions.HasKey(SessionID))
	{
		const FGameSessionEntry* GameSession = AllSessions.Find(SessionID);
		if (RoundNumber == INDEX_NONE)
		{
			TArray<INT> TempArray;
			GameSession->EventsByRound.GenerateValueArray(Events);
			Events.Append(TempArray);
		}
		else
		{
			GameSession->EventsByRound.MultiFind(RoundNumber, Events);
		}

		Count = Events.Num();
	}

	return Count;
}

/*
* Get all events associated with a given event ID
* @param SessionID - session we're interested in
* @param EventID - the event of interest (INDEX_NONE is all events)
* @param Events - array of indices related to relevant events
*/
INT UGameStatsDatabase::GetEventsByID(const FString& SessionID, INT EventID, TArray<INT>& Events)
{
	INT Count = 0;
	if (AllSessions.HasKey(SessionID))
	{
		const FGameSessionEntry* GameSession = AllSessions.Find(SessionID);
		if (EventID == INDEX_NONE)
		{
			TArray<INT> TempArray;
			GameSession->EventsByType.GenerateValueArray(Events);
			Events.Append(TempArray);
		}
		else
		{
			GameSession->EventsByType.MultiFind(EventID, Events);
		}

		Count = Events.Num();
	}

	return Count;
}

/** 
 *  Get a list of the players by session ID
 * @param SessionID - session ID to get the list for
 * @param PlayerList - output array of players
 */
void UGameStatsDatabase::GetPlayersListBySessionID(const FString& SessionId, TArray<struct FPlayerInformation>& OutPlayerList)
{
	TArray<FPlayerInformation>* PlayerList = PlayerListBySessionID.Find(SessionId);
	if (PlayerList)
	{
	   OutPlayerList.Append(*PlayerList);
	}
	else
	{
		//Try to find it remotely
		if (HasRemoteConnection())
		{
			FString SessionGUID;
			INT SessionDBID = 0;
			if (ParseSessionID(SessionId, SessionGUID, SessionDBID))
			{
				RemoteDB->GetPlayersListBySessionDBID(SessionDBID, OutPlayerList);
			}
		}
	}
}

/** 
 *  Get a list of the teams by session ID
 * @param SessionID - session ID to get the list for
 * @param PlayerList - output array of teams
 */
void UGameStatsDatabase::GetTeamListBySessionID(const FString& SessionId, TArray<struct FTeamInformation>& OutTeamList)
{
	TArray<FTeamInformation>* TeamList = TeamListBySessionID.Find(SessionId);
	if (TeamList)
	{
		OutTeamList.Append(*TeamList);
	}
	else
	{
		//Try to find it remotely
		if (HasRemoteConnection())
		{
			FString SessionGUID;
			INT SessionDBID = 0;
			if (ParseSessionID(SessionId, SessionGUID, SessionDBID))
			{
			   RemoteDB->GetTeamListBySessionDBID(SessionDBID, OutTeamList);
			}
		}
	}
}

/** 
 *  Get a list of the recorded events by session ID
 * @param SessionID - session ID to get the list for
 * @param PlayerList - output array of events
 */
void UGameStatsDatabase::GetEventsListBySessionID(const FString& SessionId,TArray<struct FGameplayEventMetaData>& OutGameplayEvents)
{
	TArray<FGameplayEventMetaData>* GameplayEventData = SupportedEventsBySessionID.Find(SessionId);
	if (GameplayEventData)
	{
	   OutGameplayEvents.Append(*GameplayEventData);
	}
	else
	{
		//Try to find it remotely
		if (HasRemoteConnection())
		{
			FString SessionGUID;
			INT SessionDBID = 0;
			if (ParseSessionID(SessionId, SessionGUID, SessionDBID))
			{
				RemoteDB->GetEventsListBySessionDBID(SessionDBID, OutGameplayEvents);
			}
		}
	}
}

/** 
 *  Get a list of the recorded weapons by session ID
 * @param SessionID - session ID to get the list for
 * @param PlayerList - output array of weapons
 */
void UGameStatsDatabase::GetWeaponListBySessionID(const FString& SessionId,TArray<struct FWeaponClassEventData>& OutWeaponList)
{
	OutWeaponList.Append(*WeaponClassesBySessionID.Find(SessionId));
}

/** 
 *  Get a list of the recorded damage types by session ID
 * @param SessionID - session ID to get the list for
 * @param PlayerList - output array of damage types
 */
void UGameStatsDatabase::GetDamageListBySessionID(const FString& SessionId,TArray<struct FDamageClassEventData>& OutDamageList)
{
	OutDamageList.Append(*DamageClassesBySessionID.Find(SessionId));
}

/** 
 *  Get a list of the recorded projectiles by session ID
 * @param SessionID - session ID to get the list for
 * @param PlayerList - output array of projectiles
 */
void UGameStatsDatabase::GetProjectileListBySessionID(const FString& SessionId,TArray<struct FProjectileClassEventData>& OutProjectileList)
{
	OutProjectileList.Append(*ProjectileClassesBySessionID.Find(SessionId));
}

/** 
 *  Get a list of the recorded pawn types by session ID
 * @param SessionID - session ID to get the list for
 * @param PlayerList - output array of pawn types
 */
void UGameStatsDatabase::GetPawnListBySessionID(const FString& SessionId,TArray<struct FPawnClassEventData>& OutPawnList)
{
	OutPawnList.Append(*PawnClassesBySessionID.Find(SessionId));
}

/**
 * Allows a visitor interface access to every database entry of interest
 * @param EventIndices - all events the visitor wants access to
 * @param Visitor - the visitor interface that will be accessing the data
 * @return TRUE if the visitor got what it needed from the visit, FALSE otherwise
 */
UBOOL UGameStatsDatabase::VisitEntries(const FGameStatsRecordSet& RecordSet, IGameStatsDatabaseVisitor* Visitor)
{
	UBOOL bSuccess = FALSE;
	if (Visitor != NULL)
	{
		//Signal the start of visiting
		Visitor->BeginVisiting();
		for (INT EntryIdx = 0; EntryIdx < RecordSet.LocalRecordSet.Num(); EntryIdx++)
		{
			//Visit all valid events
			const INT& EventIndex = RecordSet.LocalRecordSet(EntryIdx);
			if (AllEvents.IsValidIndex(EventIndex))
			{
				AllEvents(EventIndex)->Accept(Visitor);
			}
		}

		if (HasRemoteConnection())
		{
			RemoteDB->VisitEntries(RecordSet.RemoteRecordSet, Visitor);
		}

		//Signal the end of visiting
		bSuccess = Visitor->EndVisiting();
	}

	return bSuccess;
}


/***************************************************************************/
/* BELOW HERE BE DRAGONS... this code is merely bootstrapping              */
/* until we/I finalize what the "database" will look like				   */
/* atm its simply a bunch of structs allowing access to the data from disk */
/***************************************************************************/

FIGameStatEntry::FIGameStatEntry(const FGameEventHeader& GameEvent)
: EventID(GameEvent.EventID), EventTime(GameEvent.TimeStamp)
{
	EventName = GetEventNameFromIndex(EventID);
}

FIGameStatEntry::FIGameStatEntry(class FDataBaseRecordSet* Record)
{
	EventName = Record->GetString(DBEventName);
	EventID = Record->GetInt(DBEventID);
	EventTime = Record->GetFloat(DBEventTime);
}

/**
 * Game stats location entry.  SubEntry type for any event that contains a location and orientation 
 */
LocationEntry::LocationEntry(class FDataBaseRecordSet* Record, UBOOL bHasRotation) : FIGameStatEntry(Record)
{
	Location.X = Record->GetFloat(DBPositionX);
	Location.Y = Record->GetFloat(DBPositionY);
	Location.Z = Record->GetFloat(DBPositionZ);

	if (bHasRotation)
	{
		Rotation.Yaw = Record->GetFloat(DBOrientationYaw);
		Rotation.Pitch = Record->GetFloat(DBOrientationPitch);
		Rotation.Roll = Record->GetFloat(DBOrientationRoll);
	}
	else
	{
		Rotation.Yaw = Rotation.Pitch = Rotation.Roll = 0;
	}
}

/**
 * Game stats game string entry.  High level event for anything occurring at the game level
 */
GameStringEntry::GameStringEntry(const FGameEventHeader& GameEvent, const FGameStringEvent* GameStringEvent)
: FIGameStatEntry(GameEvent), String(GameStringEvent->StringEvent)
{}

GameStringEntry::GameStringEntry(FDataBaseRecordSet* Record) : FIGameStatEntry(Record)
{
	String = Record->GetString(DBStringValue);
}

/**
 * Game stats game int entry.  High level event for anything occurring at the game level
 */
GameIntEntry::GameIntEntry(const FGameEventHeader& GameEvent, const FGameIntEvent* GameIntEvent)
: FIGameStatEntry(GameEvent), Value(GameIntEvent->Value)
{}

GameIntEntry::GameIntEntry(FDataBaseRecordSet* Record) : FIGameStatEntry(Record)
{
	Value = Record->GetInt(DBIntValue);
}

/**
 * Game stats game float entry.  High level event for anything occurring at the game level
 */
GameFloatEntry::GameFloatEntry(const FGameEventHeader& GameEvent, const FGameFloatEvent* GameFloatEvent)
: FIGameStatEntry(GameEvent), Value(GameFloatEvent->Value)
{}

GameFloatEntry::GameFloatEntry(FDataBaseRecordSet* Record) : FIGameStatEntry(Record)
{
	Value = Record->GetFloat(DBFloatValue);
}

/**
 * Game stats game position entry.  High level event for anything occurring at the game level
 */
GamePositionEntry::GamePositionEntry(const FGameEventHeader& GameEvent, const FGamePositionEvent* GamePositionEvent)
: LocationEntry(GameEvent, FRotator(EC_EventParm), GamePositionEvent->Location),
	Value(GamePositionEvent->Value)
{}

GamePositionEntry::GamePositionEntry(FDataBaseRecordSet* Record) : LocationEntry(Record, FALSE)
{
	Value = Record->GetFloat(DBFloatValue);
}

/**
 * Game stats team entry.  SubEntry type for any event that contains information about a team
 */
TeamEntry::TeamEntry(FDataBaseRecordSet* Record) : FIGameStatEntry(Record)
{
	TeamIndex = Record->GetInt(DBTeamIndex); 
}

/**
 * Game stats player entry.  SubEntry type for any event containing information about a player
 */
PlayerEntry::PlayerEntry(const FGameEventHeader& GameEvent, INT InPlayerIndexAndYaw, INT InPlayerPitchAndRoll, const FVector& InLocation)
{
	//FIGameStatEntry
	EventID = GameEvent.EventID;
	EventName = GetEventNameFromIndex(GameEvent.EventID);
	EventTime = GameEvent.TimeStamp;

	//LocationEntry
	ConvertToPlayerIndexAndRotation(InPlayerIndexAndYaw, InPlayerPitchAndRoll, PlayerIndex, Rotation);
	Location = InLocation;

	//PlayerEntry
	PlayerName = GetPlayerNameFromIndex(PlayerIndex);
}

PlayerEntry::PlayerEntry(FDataBaseRecordSet* Record) : LocationEntry(Record)
{
	PlayerIndex = Record->GetInt(DBPlayerIndex);
	PlayerName = Record->GetString(DBPlayerName);
}

/**
 * Game stats player string entry.  High level event for anything occurring at the player level
 */
PlayerStringEntry::PlayerStringEntry(const FGameEventHeader& GameEvent, const FPlayerStringEvent* PlayerStringEvent)
: PlayerEntry(GameEvent, PlayerStringEvent->PlayerIndexAndYaw, PlayerStringEvent->PlayerPitchAndRoll, PlayerStringEvent->Location),
	StringEvent(PlayerStringEvent->StringEvent)
{}

PlayerStringEntry::PlayerStringEntry(FDataBaseRecordSet* Record) : PlayerEntry(Record)
{
	StringEvent = Record->GetString(DBStringValue);
}

/**
 * Game stats player int entry.  High level event for anything occurring at the player level
 */
PlayerIntEntry::PlayerIntEntry(const FGameEventHeader& GameEvent, const FPlayerIntEvent* PlayerIntEvent)
: PlayerEntry(GameEvent, PlayerIntEvent->PlayerIndexAndYaw, PlayerIntEvent->PlayerPitchAndRoll, PlayerIntEvent->Location),
	Value(PlayerIntEvent->Value)
{}

PlayerIntEntry::PlayerIntEntry(FDataBaseRecordSet* Record, UBOOL bHasValue) : PlayerEntry(Record)
{
	if (bHasValue)
	{
		Value = Record->GetInt(DBIntValue);
	}
	else
	{
		Value = 0;
	}
}

/**
 * Game stats player float entry.  High level event for anything occurring at the player level
 */
PlayerFloatEntry::PlayerFloatEntry(const FGameEventHeader& GameEvent, const FPlayerFloatEvent* PlayerFloatEvent)
: PlayerEntry(GameEvent, PlayerFloatEvent->PlayerIndexAndYaw, PlayerFloatEvent->PlayerPitchAndRoll, PlayerFloatEvent->Location),
	Value(PlayerFloatEvent->Value)
{}

PlayerFloatEntry::PlayerFloatEntry(FDataBaseRecordSet* Record) : PlayerEntry(Record)
{
	Value = Record->GetFloat(DBFloatValue);
}

/**
 * Game stats player login entry.  High level event for recording a player login
 */
PlayerLoginEntry::PlayerLoginEntry(const FGameEventHeader& GameEvent, const FPlayerLoginEvent* PlayerLoginEvent)
: PlayerEntry(GameEvent, PlayerLoginEvent->PlayerIndexAndYaw, PlayerLoginEvent->PlayerPitchAndRoll, PlayerLoginEvent->Location)
{
	bSplitScreen = PlayerLoginEvent->bSplitScreen ? TRUE : FALSE;
}

PlayerLoginEntry::PlayerLoginEntry(FDataBaseRecordSet* Record) : PlayerEntry(Record)
{
	bSplitScreen = Record->GetInt(DBIntValue) ? TRUE : FALSE;
}

/**
 * Game stats player spawn entry.  High level event for recording a player spawn during a match
 */
PlayerSpawnEntry::PlayerSpawnEntry(const FGameEventHeader& GameEvent, const FPlayerSpawnEvent* PlayerSpawnEvent)
: PlayerEntry(GameEvent, PlayerSpawnEvent->PlayerIndexAndYaw, PlayerSpawnEvent->PlayerPitchAndRoll, PlayerSpawnEvent->Location),
	PawnClassIndex(PlayerSpawnEvent->PawnClassIndex),
	TeamIndex(PlayerSpawnEvent->TeamIndex)
{
	PawnClassName = GetPawnNameFromIndex(PlayerSpawnEvent->PawnClassIndex);
}

PlayerSpawnEntry::PlayerSpawnEntry(FDataBaseRecordSet* Record) : PlayerEntry(Record)
{
	PawnClassIndex = Record->GetInt(DBPawnIndex);
	PawnClassName = Record->GetString(DBPawnName);
	TeamIndex = Record->GetInt(DBPawnTeamIndex);
}

/**
 * Game stats player kill entry.  High level event for recording the killing of one player by another
 */
PlayerKillDeathEntry::PlayerKillDeathEntry(const FGameEventHeader& GameEvent, const FPlayerKillDeathEvent* PlayerKillDeathEvent)
: PlayerEntry(GameEvent, PlayerKillDeathEvent->PlayerIndexAndYaw, PlayerKillDeathEvent->PlayerPitchAndRoll, PlayerKillDeathEvent->PlayerLocation),
	Target(GameEvent, PlayerKillDeathEvent->TargetIndexAndYaw, PlayerKillDeathEvent->TargetPitchAndRoll, PlayerKillDeathEvent->TargetLocation),
	DamageClassIndex(PlayerKillDeathEvent->DamageClassIndex),
	KillType(PlayerKillDeathEvent->KillType)
{
	DamageClassName = GetDamageNameFromIndex(PlayerKillDeathEvent->DamageClassIndex);
	KillTypeString = GetEventNameFromIndex(PlayerKillDeathEvent->KillType);
}

PlayerKillDeathEntry::PlayerKillDeathEntry(FDataBaseRecordSet* Record) : PlayerEntry(Record)
{
	Target.PlayerIndex = Record->GetInt(DBTargetIndex);
	Target.PlayerName = Record->GetString(DBTargetName);
	
	DamageClassIndex = Record->GetInt(DBDamageClassIndex);
	DamageClassName = Record->GetString(DBDamageClassName);

	KillType = Record->GetInt(DBKillTypeIndex);
	KillTypeString = Record->GetString(DBKillTypeName);

	Target.Location.X = Record->GetFloat(DBTargetPositionX);
	Target.Location.Y = Record->GetFloat(DBTargetPositionY);
	Target.Location.Z = Record->GetFloat(DBTargetPositionZ);

	Target.Rotation.Yaw = Record->GetFloat(DBTargetOrientationYaw);
	Target.Rotation.Pitch = Record->GetFloat(DBTargetOrientationPitch);
	Target.Rotation.Roll = Record->GetFloat(DBTargetOrientationRoll);
}

/**
 * Game stats player player entry.  High level event for recording an interaction between two players
 */
PlayerPlayerEntry::PlayerPlayerEntry(const FGameEventHeader& GameEvent, const FPlayerPlayerEvent* PlayerPlayerEvent)
: PlayerEntry(GameEvent, PlayerPlayerEvent->PlayerIndexAndYaw, PlayerPlayerEvent->PlayerPitchAndRoll, PlayerPlayerEvent->PlayerLocation),
	Target(GameEvent, PlayerPlayerEvent->TargetIndexAndYaw, PlayerPlayerEvent->TargetPitchAndRoll, PlayerPlayerEvent->TargetLocation)
{}

PlayerPlayerEntry::PlayerPlayerEntry(FDataBaseRecordSet* Record) : PlayerEntry(Record)
{
	Target.PlayerIndex = Record->GetInt(DBTargetIndex);
	Target.PlayerName = Record->GetString(DBTargetName);
	
	Target.Location.X = Record->GetFloat(DBTargetPositionX);
	Target.Location.Y = Record->GetFloat(DBTargetPositionY);
	Target.Location.Z = Record->GetFloat(DBTargetPositionZ);

	Target.Rotation.Yaw = Record->GetFloat(DBTargetOrientationYaw);
	Target.Rotation.Pitch = Record->GetFloat(DBTargetOrientationPitch);
	Target.Rotation.Roll = Record->GetFloat(DBTargetOrientationRoll);
}

/**
 * Game stats weapon entry.  High level event for recording weapon use
 */
WeaponEntry::WeaponEntry(const FGameEventHeader& GameEvent, const FWeaponIntEvent* WeaponIntEvent)
: PlayerEntry(GameEvent, WeaponIntEvent->PlayerIndexAndYaw, WeaponIntEvent->PlayerPitchAndRoll, WeaponIntEvent->Location)
{
	WeaponClassIndex = WeaponIntEvent->WeaponClassIndex;
	WeaponClassName = GetWeaponNameFromIndex(WeaponIntEvent->WeaponClassIndex);
	Value = WeaponIntEvent->Value;
}

WeaponEntry::WeaponEntry(FDataBaseRecordSet* Record) : PlayerEntry(Record)
{
	WeaponClassIndex = Record->GetInt(DBWeaponClassIndex);
	WeaponClassName = Record->GetString(DBWeaponClassName);
	Value = Record->GetInt(DBWeaponClassValue);
}

/**
 * Game stats damage entry.  High level event for anything involving damage to players
 */
DamageEntry::DamageEntry(const FGameEventHeader& GameEvent, const FDamageIntEvent* DamageIntEvent)
: PlayerEntry(GameEvent, DamageIntEvent->PlayerIndexAndYaw, DamageIntEvent->PlayerPitchAndRoll, DamageIntEvent->PlayerLocation),
	Target(GameEvent, DamageIntEvent->TargetPlayerIndexAndYaw, DamageIntEvent->TargetPlayerPitchAndRoll, DamageIntEvent->TargetLocation)
{
	DamageClassIndex = DamageIntEvent->DamageClassIndex;
	DamageClassName = GetDamageNameFromIndex(DamageIntEvent->DamageClassIndex);
	Value = DamageIntEvent->Value;
}

DamageEntry::DamageEntry(FDataBaseRecordSet* Record) : PlayerEntry(Record)
{
	Target.PlayerIndex = Record->GetInt(DBTargetIndex);
	Target.PlayerName = Record->GetString(DBTargetName);

	Target.Location.X = Record->GetFloat(DBTargetPositionX);
	Target.Location.Y = Record->GetFloat(DBTargetPositionY);
	Target.Location.Z = Record->GetFloat(DBTargetPositionZ);

	Target.Rotation.Yaw = Record->GetFloat(DBTargetOrientationYaw);
	Target.Rotation.Pitch = Record->GetFloat(DBTargetOrientationPitch);
	Target.Rotation.Roll = Record->GetFloat(DBTargetOrientationRoll);
	
	DamageClassIndex = Record->GetInt(DBDamageClassIndex);
	DamageClassName = Record->GetString(DBDamageClassName);
	Value = Record->GetInt(DBDamageClassValue);
}

/**
 * Game stats projectile entry.  High level event for anything involving projectile use
 */
ProjectileIntEntry::ProjectileIntEntry(const FGameEventHeader& GameEvent, const FProjectileIntEvent* ProjectileIntEvent)
: PlayerEntry(GameEvent, ProjectileIntEvent->PlayerIndexAndYaw, ProjectileIntEvent->PlayerPitchAndRoll, ProjectileIntEvent->Location)
{
	ProjectileClassIndex = ProjectileIntEvent->ProjectileClassIndex;
	ProjectileName = GetProjectileNameFromIndex(ProjectileIntEvent->ProjectileClassIndex);
	Value = ProjectileIntEvent->Value;
}

ProjectileIntEntry::ProjectileIntEntry(FDataBaseRecordSet* Record) : PlayerEntry(Record)
{
	ProjectileClassIndex = Record->GetInt(DBProjectileClassIndex);
	ProjectileName= Record->GetString(DBProjectileClassName);
	Value = Record->GetInt(DBProjectileClassValue);
}

/**
 * generic param list entry
 */
GenericParamListEntry::GenericParamListEntry(const FGameEventHeader& GameEvent, const FGenericParamListEvent* ParamEvent) :
 FGenericParamListEvent(*ParamEvent)
{
	//FIGameStatEntry
	EventID = GameEvent.EventID;
	EventName = GetEventNameFromIndex(GameEvent.EventID);
	EventTime = GameEvent.TimeStamp;
}

GenericParamListEntry::GenericParamListEntry(FDataBaseRecordSet* Record)
{
	// derp	derp derp
	TArray<FDatabaseColumnInfo> Columns = Record->GetColumnNames();

	for(INT Idx=0;Idx,Columns.Num();++Idx)
	{
		FDatabaseColumnInfo& Inf = Columns(Idx);
		
		switch(Inf.DataType)
		{
		case DBT_FLOAT:
			SetNamedParamData<FLOAT>(*Inf.ColumnName,Record->GetFloat(*Inf.ColumnName));
			break;
		case DBT_INT:
			SetNamedParamData<INT>(*Inf.ColumnName,Record->GetInt(*Inf.ColumnName));
			break;
		case DBT_STRING:
			SetNamedParamData<FString>(*Inf.ColumnName,Record->GetString(*Inf.ColumnName));
			break;
		default:
			break;
		}
	}
}	

#undef warnexprf

