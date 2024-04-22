/*=============================================================================
	GameStatsDatabaseRemote.cpp: Implementation of a remotely accessed 
	SQL Database for game stats collection
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

/** Helper for cleaning strings for DB use */
FString ScrubString(const FString& DirtyText)
{
	return DirtyText.Replace(TEXT("'"), TEXT("''"));
}

/*-----------------------------------------------------------------------------
FGameStatsRemoteDB implementation.
-----------------------------------------------------------------------------*/

/**
* Constructor, initializing all member variables. It also reads .ini settings and caches them and creates the
* database connection object and attempts to connect if bUseTaskPerfTracking is set.
*/
FGameStatsRemoteDB::FGameStatsRemoteDB()
: FTaskDatabase(),
TimeSpentTalkingWithDB(0),
SessionDBID(-1)
{
	OpenConnection();
}

/** Destructor */
FGameStatsRemoteDB::~FGameStatsRemoteDB()
{
	// If the connection failed this will include the time spent waiting for the failure.
	if( TimeSpentTalkingWithDB > 0 )
	{
		debugf(NAME_DevDataBase,TEXT("Spent %f seconds communicating with \"%s\" or \"%s\""),TimeSpentTalkingWithDB, *ConnectionString, *RemoteConnectionIP);
	}

	Reset();
	CloseConnection();
}   

/** 
 *   Initialize the remote db data structures from a given map/time range
 * @param InMapName - map to query data for
 * @param DateFilter - enum of date range to filter by
 */
void FGameStatsRemoteDB::Init(const FString& InMapname, BYTE DateFilter)
{
	Reset();
	MapName = InMapname;

	// Fill in the data
	UpdateGameSessionInfo(MapName, (GameStatsDateFilters)DateFilter, TEXT("ANY"));
}

/** 
 *   Populate the game session info arrays for a given map/time range
 * @param InMapName - map to query data for
 * @param DateFilter - enum of date range to filter by
 * @param GameTypeFilter - Gametype classname or "ANY"
 */
void FGameStatsRemoteDB::UpdateGameSessionInfo(const FString& InMapname, GameStatsDateFilters DateFilter, const FString& GameTypeFilter)
{
	TArray<FGameSessionInformation> GameSessions;
	if (GetGameSessionInfosForMap(MapName, DateFilter, GameTypeFilter, GameSessions))
	{
		for (INT SessionIter=0; SessionIter<GameSessions.Num(); SessionIter++)
		{
			const FGameSessionInformation& SessionInfo = GameSessions(SessionIter);
			SessionInfoBySessionID.Set(SessionInfo.GetSessionID(), SessionInfo);
			SessionInfoBySessionDBID.Set(SessionInfo.SessionInstance, SessionInfo);
		}
	}
}

/** 
*  Opens a connection to the configured database 
*/
void FGameStatsRemoteDB::OpenConnection()
{
#if !UDK
	// only attempt to get the data when we want to use the TaskPerfTracking
	verify(GConfig->GetString( TEXT("UnrealEd.GameStatsBrowser"), TEXT("ConnectionString"), ConnectionString, GEditorIni ));
	verify(GConfig->GetString( TEXT("UnrealEd.GameStatsBrowser"), TEXT("RemoteConnectionIP"), RemoteConnectionIP, GEditorIni ));
	verify(GConfig->GetString( TEXT("UnrealEd.GameStatsBrowser"), TEXT("RemoteConnectionStringOverride"), RemoteConnectionStringOverride, GEditorIni ));

	// Track time spent talking with DB to ensure we don't introduce nasty stalls.
	SCOPE_SECONDS_COUNTER(TimeSpentTalkingWithDB);

	// Create the connection object; needs to be deleted via "delete".
	GWarn->BeginSlowTask( *LocalizeUnrealEd("GameStatsConnectingToDatabase") , TRUE );

	// Create the connection object; needs to be deleted via "delete".
	Connection = FDataBaseConnection::CreateObject();

	// Try to open connection to DB - this is a synchronous operation.
	if( Connection && Connection->Open( *ConnectionString, *RemoteConnectionIP, *RemoteConnectionStringOverride ) == TRUE )
	{
		debugf(NAME_DevDataBase,TEXT("Game Stats Visualizer Connection to \"%s\" or \"%s\" succeeded"), *ConnectionString, *RemoteConnectionIP);
	}
	// Connection failed :(
	else
	{
		warnf(NAME_DevDataBase,TEXT("Game Stats Visualizer Connection to \"%s\" or \"%s\" failed"), *ConnectionString, *RemoteConnectionIP);
		// Only delete object - no need to close as connection failed.
		delete Connection;
		Connection = NULL;
	}

	GWarn->EndSlowTask();
#endif	//#if !UDK
}

/** 
*  This will send the text to be "Exec" 'd to the DB proxy 
*
* @param	ExecCommand  Exec command to send to the Proxy DB.
*/
UBOOL FGameStatsRemoteDB::SendExecCommand( const FString& ExecCommand )
{
	UBOOL Retval = FALSE;
	// Track time spent talking with DB to ensure we don't introduce nasty stalls.
	SCOPE_SECONDS_COUNTER(TimeSpentTalkingWithDB);
	if( Connection != NULL )
	{
		// Execute SQL command generated. The order of arguments needs to match the format string.
		Retval = Connection->Execute( *ExecCommand );
		//warnf( TEXT( "%s"), *ExecCommand );
	}

	return Retval;
}


/** 
*  This will send the text to be "Exec" 'd to the DB proxy 
*
* @param  ExecCommand  Exec command to send to the Proxy DB.
* @param RecordSet		Reference to recordset pointer that is going to hold result
*/
UBOOL FGameStatsRemoteDB::SendExecCommandRecordSet( const FString& ExecCommand, FDataBaseRecordSet*& RecordSet, UBOOL bAsync, UBOOL bNoRecords )
{
	UBOOL Retval = FALSE;
	// Track time spent talking with DB to ensure we don't introduce nasty stalls.
	SCOPE_SECONDS_COUNTER(TimeSpentTalkingWithDB);
	if( Connection != NULL )
	{
		// Execute SQL command generated. The order of arguments needs to match the format string.
		Retval = Connection->Execute( *ExecCommand, RecordSet );
		//warnf( TEXT( "%s"), *ExecCommand );
	}

	return Retval;
}

/** 
*   Returns whether or not the given session exists in the database
*   @param SessionID - the session to check against
*   @return TRUE if it exists, FALSE otherwise
*/
UBOOL FGameStatsRemoteDB::DoesSessionExistInDB(const FString& SessionID)
{
	UBOOL bSuccess = FALSE;

	FString ScrubSessionID = ScrubString(SessionID);
	FString Command = FString::Printf(TEXT("SELECT COUNT(1) AS SessionCount FROM [gamestats].[GameSessions_v1] WHERE IsComplete = 1 AND SessionUID = '%s'"), *ScrubSessionID);

	FDataBaseRecordSet* RecordSet = NULL;
	if( SendExecCommandRecordSet( *Command, RecordSet ) && RecordSet )
	{
		// Return count of sessions that have this ID in the DB
		INT SessionDBID = RecordSet->GetInt(TEXT("SessionCount"));
		//warnf( TEXT("Session Exist Count: %s = %d"), *ScrubSessionID, SessionDBID );
		bSuccess = (SessionDBID > 0);
	}
	else
	{
		debugf(TEXT("DoesSessionExistInDB(): Failed to query for session ID %s"), *SessionID);
	}

	delete RecordSet;
	RecordSet = NULL;

	return bSuccess;
}

INT FGameStatsRemoteDB::QueryDatabase(const FGameStatsSearchQuery& Query, TArray<FIGameStatEntry*>& Events)
{
	if (Query.SessionIDs.Num() == 0)
	{
		return 0;
	}

	// Sessions that have no filter also need game stats
	TArray<FString> SessionIdsNeedingGameStats = Query.SessionIDs;

	// Accumulate player events
	TArray<FIGameStatEntry*> PlayerEvents;
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
	TArray<FIGameStatEntry*> TeamEvents;
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
	TArray<FIGameStatEntry*> GameEvents;
	for (INT GameIndex=0; GameIndex<SessionIdsNeedingGameStats.Num(); GameIndex++)
	{
		GetGameEventsBySession(SessionIdsNeedingGameStats(GameIndex), GameEvents);
	}

	// Create a unique array of entries from all game/team/player possibilities
	TSet<FIGameStatEntry*> UniqueEntries;
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
	for(TSet<FIGameStatEntry*>::TIterator EntryIt = TSet<FIGameStatEntry*>::TIterator(UniqueEntries); EntryIt; ++EntryIt)
	{
		const FIGameStatEntry* Entry = *EntryIt;

		// Filter the queries added to results by time and event
		if ((Query.StartTime <= Entry->EventTime && Entry->EventTime <= Query.EndTime) &&
			(bAllEvents || (Query.EventIDs.ContainsItem(Entry->EventID))))
		{
			Events.AddItem(*EntryIt);
		}
	}

	return Events.Num();
}

/**
* Allows a visitor interface access to every database entry of interest
* @param EventIndices - all events the visitor wants access to
* @param Visitor - the visitor interface that will be accessing the data
* @return TRUE if the visitor got what it needed from the visit, FALSE otherwise
*/
void FGameStatsRemoteDB::VisitEntries(const TArray<FIGameStatEntry*>& Events, IGameStatsDatabaseVisitor* Visitor)
{
	for (INT EntryIdx = 0; EntryIdx < Events.Num(); EntryIdx++)
	{
		//Visit all valid events
		Events(EntryIdx)->Accept(Visitor);
	}
}

/*
 * Get the gametypes in the database
 * @param GameTypes - array of all gametypes in the database
 */
void FGameStatsRemoteDB::GetGameTypes(TArray<FString>& GameTypes)
{
	if (AllGameTypes.Num() == 0)
	{
		FString Command = TEXT("SELECT [GameClass], [FriendlyName] FROM [editor].[GameClasses_v1]");
		FDataBaseRecordSet* RecordSet = NULL;
		if( SendExecCommandRecordSet( *Command, RecordSet ) && RecordSet )
		{
			// Iterate over all rows in record set and log them.
			for( FDataBaseRecordSet::TIterator It( RecordSet ); It; ++It )
			{
				FString GameType = It->GetString(TEXT("GameClass"));
				AllGameTypes.AddItem(GameType);
			}
		}
		delete RecordSet;
		RecordSet = NULL;
	}

	GameTypes.Append(AllGameTypes);
}

/*
 * Get the session ids in the database
 * @param DateFilter - Date enum to filter by
 * @param GameTypeFilter - GameClass name or "ANY"
 * @param SessionIDs - array of all sessions in the database
 */
void FGameStatsRemoteDB::GetSessionIDs(BYTE DateFilter, const FString& GameTypeFilter, TArray<FString>& SessionIDs)
{	
	SessionInfoBySessionID.Empty();
	SessionInfoBySessionDBID.Empty();

	UpdateGameSessionInfo(MapName, (GameStatsDateFilters)DateFilter, GameTypeFilter);

	SessionInfoBySessionID.GenerateKeyArray(SessionIDs);
}

void FGameStatsRemoteDB::GetSessionInfoBySessionDBID(INT SessionDBID,struct FGameSessionInformation& OutSessionInfo)
{
	const FGameSessionInformation* GameSessionInfo = SessionInfoBySessionDBID.Find(SessionDBID);
	if (GameSessionInfo != NULL)
	{
		OutSessionInfo = *GameSessionInfo;
	}
}

UBOOL FGameStatsRemoteDB::GetGameSessionInfosForMap(const FString& MapName, GameStatsDateFilters DateFilter, const FString& GameType, TArray<FGameSessionInformation>& GameSessionArray)
{
	INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
	appSystemTime(Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);

	// Map Filtering
	FString ScrubbedMap = ScrubString(MapName);

	// Date Filtering
	INT NumMinutesElapsed = Hour * 60 + Min;
	switch (DateFilter)
	{
	case GSDF_Today:
		break;
	case GSDF_Last3Days:
		// Today + 2 days time
		NumMinutesElapsed += (2 * 24 * 60);	 
		break;
	case GSDF_LastWeek:
		// Today + 6 days time
		NumMinutesElapsed += (6 * 24 * 60);	 
		break;
	}

	// Game Type Filtering
	FString GameTypeFilter = TEXT("");
	if (appStricmp(*GameType, TEXT("ANY")) != 0)
	{
		GameTypeFilter = FString::Printf(TEXT("AND GameClass = '%s' "), *GameType);
	}

	FString Command = FString::Printf( TEXT("SELECT TOP(100) ")
									   TEXT("CONVERT(VARCHAR, DatePlayed, 20) AS DatePlayedString,")
									   TEXT("* FROM [gamestats].[GameSessions_v1] WHERE IsComplete = 1 ")
									   TEXT("AND DateUploadedUTC > DATEADD(mi, %d, GetUTCDate()) %s AND MapName = '%s' ")
									   TEXT("ORDER BY DateUploadedUTC DESC"), -NumMinutesElapsed, *GameTypeFilter, *ScrubbedMap);
	FDataBaseRecordSet* RecordSet = NULL;
	if( SendExecCommandRecordSet( *Command, RecordSet ) && RecordSet )
	{
		// Iterate over all rows in record set and log them.
		for( FDataBaseRecordSet::TIterator It( RecordSet ); It; ++It )
		{
			INT SessionIndex = GameSessionArray.AddZeroed(1);
			FGameSessionInformation& SessionInfo = GameSessionArray(SessionIndex);

			SessionInfo.SessionInstance = It->GetInt(TEXT("ID"));
			SessionInfo.GameplaySessionID = It->GetString(TEXT("SessionUID")).TrimTrailing();
			SessionInfo.GameplaySessionTimestamp = It->GetString(TEXT("DatePlayedString")).TrimTrailing();
			// Make this timestamp look like appUtcTimeString()
			SessionInfo.GameplaySessionTimestamp.ReplaceInline(TEXT("-"),TEXT("."));
			SessionInfo.GameplaySessionTimestamp.ReplaceInline(TEXT(":"),TEXT("."));
			SessionInfo.GameplaySessionTimestamp.ReplaceInline(TEXT(" "),TEXT("-"));
			SessionInfo.AppTitleID = It->GetInt(TEXT("TitleID"));
			SessionInfo.PlatformType = It->GetInt(TEXT("Platform"));
			SessionInfo.Language = It->GetString(TEXT("Language")).TrimTrailing();
			SessionInfo.GameplaySessionStartTime = It->GetFloat(TEXT("StartTime"));
			SessionInfo.GameplaySessionEndTime = It->GetFloat(TEXT("EndTime"));
			SessionInfo.GameClassName = It->GetString(TEXT("GameClass")).TrimTrailing();
			SessionInfo.MapURL = It->GetString(TEXT("MapURL")).TrimTrailing();
			SessionInfo.MapName	= It->GetString(TEXT("MapName")).TrimTrailing();
		}
	}

	delete RecordSet;
	RecordSet = NULL;
	return (GameSessionArray.Num() > 0);
}

void FGameStatsRemoteDB::GetPlayersListBySessionDBID(INT SessionDBID, TArray<FPlayerInformation>& PlayerMetadata)
{
	PlayerMetadata.Empty();

	TArray<FPlayerInformation>* PlayerList = PlayerListBySessionDBID.Find(SessionDBID);
	if (PlayerList)
	{
		PlayerMetadata.Append(*PlayerList);
	}
	else
	{
		FString Command = FString::Printf(TEXT("SELECT * FROM [gamestats].[SessionPlayerMap_v1] WHERE SessionID = %d"), SessionDBID);

		FDataBaseRecordSet* RecordSet = NULL;
		if( SendExecCommandRecordSet( *Command, RecordSet ) && RecordSet )
		{
			INT PlayerCount = 0;
			for( FDataBaseRecordSet::TIterator It( RecordSet ); It; ++It )
			{
				PlayerCount++;
			}

			if (PlayerCount > 0)
			{
				PlayerMetadata.AddZeroed(PlayerCount);

				// Iterate over all rows in record set and log them.
				for( FDataBaseRecordSet::TIterator It( RecordSet ); It; ++It )
				{
					INT PlayerIndex = It->GetInt(TEXT("PlayerIndex"));
					FPlayerInformation& PlayerInfo = PlayerMetadata(PlayerIndex);

					PlayerInfo.ControllerName = FName(*It->GetString(TEXT("ControllerName")));
					PlayerInfo.PlayerName = It->GetString(TEXT("PlayerName"));
					PlayerInfo.UniqueId.Uid = (QWORD)It->GetBigInt(TEXT("PlayerUID"));
					PlayerInfo.bIsBot = PlayerInfo.UniqueId.Uid != 0 ? FALSE : TRUE;
				}

				PlayerListBySessionDBID.Set(SessionDBID, PlayerMetadata);
			}
		}

		delete RecordSet;
	}
}

void FGameStatsRemoteDB::GetTeamListBySessionDBID(INT SessionDBID, TArray<FTeamInformation>& TeamMetadata)
{
	TeamMetadata.Empty();

	FTeamInformation TempTeam;
	appMemzero(&TempTeam, sizeof(FTeamInformation));

	TempTeam.TeamIndex = 0;
	TempTeam.TeamName = TEXT("Team1");
	TempTeam.TeamColor = FColor(255,255,255); 
	TempTeam.MaxSize = 0;
	TeamMetadata.AddItem(TempTeam);

	TempTeam.TeamIndex = 1;
	TempTeam.TeamName = TEXT("Team2");
	TeamMetadata.AddItem(TempTeam);

	/*
	TArray<FTeamInformation>* TeamList = TeamListBySessionID.Find(SessionID);
	if (TeamList)
	{
		TeamMetadata.Append(*TeamList);
	}
	else
	{
		FString Command = FString::Printf( TEXT("EXEC GetAllTeamMetadataForSession @SessionID='%s'"), *SessionID );

		FDataBaseRecordSet* RecordSet = NULL;
		if( SendExecCommandRecordSet( *Command, RecordSet ) && RecordSet )
		{
			// Iterate over all rows in record set and log them.
			for( FDataBaseRecordSet::TIterator It( RecordSet ); It; ++It )
			{
				INT TeamIndex = TeamMetadata.AddZeroed(1);
				FTeamInformation& TeamInfo = TeamMetadata(TeamIndex);

				TeamInfo.TeamIndex = It->GetInt(TEXT("TeamID"));
				TeamInfo.TeamName = It->GetString(TEXT("TeamName"));
				TeamInfo.TeamColor = FColor(It->GetInt(TEXT("TeamColor")));
			}

			TeamListBySessionID.Set(SessionID, TeamMetadata);
		}

		delete RecordSet;
	}
	*/
}

/*
 * Get all events associated with a given player
 * @param SessionID - session we're interested in
 * @param PlayerIndex - the player to return the events for (INDEX_NONE is all players)
 * @param Events - array of events related to relevant player events
 */
INT FGameStatsRemoteDB::GetEventsByPlayer(const FString& SessionID, INT PlayerIndex, TArray<FIGameStatEntry*>& Events)
{
 	INT Count = 0;

	FString SessionGUID;
	INT SessionDBID = 0;
	if (ParseSessionID(SessionID, SessionGUID, SessionDBID))
	{
		const FGameSessionEntryRemote* GameSession = AllSessions.Find(SessionDBID);
		if (GameSession != NULL)
		{
			if (PlayerIndex == INDEX_NONE)
			{
				TArray<FIGameStatEntry*> TempArray;
				GameSession->EventsByPlayer.GenerateValueArray(TempArray);
				Events.Append(TempArray);
			}
			else
			{
				GameSession->EventsByPlayer.MultiFind(PlayerIndex, Events);
			}

			Count = Events.Num();
		}
	}

	return Count;
}

/*
 * Get all events associated with a given team
 * @param SessionID - session we're interested in
 * @param TeamIndex - the team to return the events for (INDEX_NONE is all teams)
 * @param Events - array of events related to relevant team events
 */
INT FGameStatsRemoteDB::GetEventsByTeam(const FString& SessionID, INT TeamIndex, TArray<FIGameStatEntry*>& Events)
{
	INT Count = 0;								   
	FString SessionGUID;
	INT SessionDBID = 0;
	if (ParseSessionID(SessionID, SessionGUID, SessionDBID))
	{
		const FGameSessionEntryRemote* GameSession = AllSessions.Find(SessionDBID);
		if (GameSession != NULL)
		{
			if (TeamIndex == INDEX_NONE)
			{
				TArray<FIGameStatEntry*> TempArray;
				GameSession->EventsByTeam.GenerateValueArray(TempArray);
				Events.Append(TempArray);
			}
			else
			{
				GameSession->EventsByTeam.MultiFind(TeamIndex, Events);
			}

			Count = Events.Num();
		}
	}

	return Count;
}

/*
 * Get all events associated with a given game (non-team/player)
 * @param SessionID - session we're interested in
 * @param Events - array of events from the game
 */
INT FGameStatsRemoteDB::GetGameEventsBySession(const FString& SessionID, TArray<FIGameStatEntry*>& Events)
{
	INT Count = 0;
	FString SessionGUID;
	INT SessionDBID = 0;
	if (ParseSessionID(SessionID, SessionGUID, SessionDBID))
	{
		const FGameSessionEntryRemote* GameSession = AllSessions.Find(SessionDBID);
		if (GameSession != NULL)
		{
			Events.Append(GameSession->GameEvents);
			Count = GameSession->GameEvents.Num();
		}
	}

	return Count;
}

/*
 * Get the total count of events of a given type
 * @param SessionID - session we're interested in
 * @param EventID - the event to return the count for
 */
INT FGameStatsRemoteDB::GetEventCountByType(const FString& SessionID, INT EventID)
{
	INT Count = 0;
	INT SessionDBID = 0;

	INT SpacerIdx = SessionID.InStr(TEXT(":"));
	if (SpacerIdx != INDEX_NONE)
	{
		FString DBIDString = SessionID.Right(SessionID.Len() - SpacerIdx - 1);
		SessionDBID = appAtoi(*DBIDString);
	}

	const FGameSessionEntryRemote* GameSession = AllSessions.Find(SessionDBID);
	if (GameSession != NULL)
	{
		Count = GameSession->EventsByType.Num(EventID);
	}

	return Count;
}

void FGameStatsRemoteDB::GetEventsListBySessionDBID(INT SessionDBID, TArray<struct FGameplayEventMetaData>& OutGameplayEvents)
{
	OutGameplayEvents.Empty();

	TArray<FGameplayEventMetaData>* EventsList = SupportedEventsBySessionDBID.Find(SessionDBID);
	if (EventsList)
	{
		OutGameplayEvents.Append(*EventsList);
	}
	else
	{
		FString Command = TEXT("SELECT [EventID],[ExpectedType],[Name] FROM [gamestats].[KnownEvents_v1]");
		FDataBaseRecordSet* RecordSet = NULL;
		if( SendExecCommandRecordSet( *Command, RecordSet ) && RecordSet )
		{
			FGameplayEventMetaData EventMetadata;

			for( FDataBaseRecordSet::TIterator It( RecordSet ); It; ++It )
			{
				EventMetadata.EventID = It->GetInt(TEXT("EventID"));
				EventMetadata.EventName = FName(*It->GetString(TEXT("Name")));
				EventMetadata.EventDataType = It->GetInt(TEXT("ExpectedType"));
				EventMetadata.StatGroup.Group = 0;
				EventMetadata.StatGroup.Level = 0;
				OutGameplayEvents.AddItem(EventMetadata);
			}

			SupportedEventsBySessionDBID.Set(SessionDBID, OutGameplayEvents);
		}
	}
}

/** 
 * Query the remote SQL database and pull down the session data 
 * @param SessionDBID - remote session ID in the SQL database
 */
void FGameStatsRemoteDB::PopulateSessionData(INT SessionDBID)
{
	FGameSessionEntryRemote* SessionData = AllSessions.Find(SessionDBID);
	if (SessionData != NULL)
	{
		return;
	}

	FString Command = FString::Printf(TEXT("SELECT ") 
									  TEXT("(SELECT Name FROM gamestats.KnownEvents_v1 WHERE EventID = ge.EventID) AS EventName, ") 
									  TEXT("(SELECT PlayerName FROM gamestats.SessionPlayerMap_v1 WHERE SessionID = ge.SessionID AND PlayerIndex = ge.Index0) AS PlayerName, ") 
									  TEXT("CASE WHEN EventType IN (9,10,12) THEN (SELECT PlayerName FROM gamestats.SessionPlayerMap_v1 WHERE SessionID = ge.SessionID AND PlayerIndex = ge.Index1) END AS TargetName, ") 
									  TEXT("CASE WHEN EventType IN (9) THEN (SELECT Name FROM gamestats.KnownEvents_v1 WHERE EventID = ge.IntData1) END AS KillTypeName, ") 
									  TEXT("(SELECT Value FROM gamestats.StaticStrings_v1 WHERE ID = IntData0) AS StaticString, ") 
									  TEXT("* FROM [gamestats].[GameEvents_v1] AS ge ") 
									  TEXT("WHERE SessionID = %d"), SessionDBID);

	FDataBaseRecordSet* RecordSet = NULL;
	if( SendExecCommandRecordSet( *Command, RecordSet ) && RecordSet )
	{
		INT RecordCount = RecordSet->GetRecordCount();
		if (RecordCount > 0)
		{
			const FString& UpdateString = LocalizeUnrealEd("GameStatsVisualizer_PopulatingDatabase");
			GWarn->BeginSlowTask( *LocalizeUnrealEd("GameStatsVisualizer_PopulatingDatabase"), TRUE );

			// Create a new container for this data
			AllSessions.Set(SessionDBID, FGameSessionEntryRemote(EC_EventParm));
			FGameSessionEntryRemote* SessionData = AllSessions.Find(SessionDBID);
			check(SessionData);

			FGameSessionInformation SessionInfo(EC_EventParm);
			GetSessionInfoBySessionDBID(SessionDBID, SessionInfo);

			INT RecordsProcessed = 0;
			for( FDataBaseRecordSet::TIterator It( RecordSet ); It; ++It )
			{
				if (IsInGameThread() && (RecordsProcessed % 250 == 0))
				{
					GWarn->StatusUpdatef(RecordsProcessed, RecordCount, *UpdateString );
				}

				INT PlayerIndex = INDEX_NONE;
				INT TargetIndex = INDEX_NONE;
				INT TeamIndex = INDEX_NONE;
				FIGameStatEntry* NewGameEntry = NULL;

				//Process the event
				const INT EventType = It->GetInt(TEXT("EventType"));
				const INT EventID = It->GetInt(TEXT("EventID"));
				switch (EventType)
				{
				case GET_GameString:
					{												   
						NewGameEntry = new GameStringEntry(*It);
						break;
					}
				case GET_GameInt:
					{
						NewGameEntry = new GameIntEntry(*It);
						break;
					}
				case GET_GameFloat:
					{
						NewGameEntry = new GameFloatEntry(*It);
						break;
					}
				case GET_GamePosition:
					{
						NewGameEntry = new GamePositionEntry(*It);
						break;
					}
				case GET_TeamInt:
					{
						NewGameEntry = new TeamIntEntry(*It);
						TeamIndex = static_cast<TeamIntEntry*>(NewGameEntry)->TeamIndex;
						break;
					}
				case GET_TeamString:
					{
						NewGameEntry = new TeamStringEntry(*It);
						TeamIndex = static_cast<TeamStringEntry*>(NewGameEntry)->TeamIndex;
						break;
					}
				case GET_TeamFloat:
					{
						NewGameEntry = new TeamFloatEntry(*It);
						TeamIndex = static_cast<TeamFloatEntry*>(NewGameEntry)->TeamIndex;
						break;
					}
				case GET_PlayerString:
					{
						NewGameEntry = new PlayerStringEntry(*It);
						PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
						break;
					}
				case GET_PlayerInt:
					{
						NewGameEntry = new PlayerIntEntry(*It);
						PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
						break;
					}
				case GET_PlayerFloat:
					{
						NewGameEntry = new PlayerFloatEntry(*It);
						PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
						break;
					}
				case GET_PlayerSpawn:
					{
						NewGameEntry = new PlayerSpawnEntry(*It);
						PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
						break;
					}
				case GET_PlayerLogin:
					{
						NewGameEntry = new PlayerLoginEntry(*It);
						PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
						break;
					}
				case GET_PlayerLocationPoll:
					{
						NewGameEntry = new PlayerIntEntry(*It, FALSE);
						PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
						break;
					}
				case GET_PlayerKillDeath:
					{
						NewGameEntry = new PlayerKillDeathEntry(*It);
						PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
						// TargetIndex is DEATH event, handled separately
						break;				   
					}
				case GET_PlayerPlayer:
					{
						NewGameEntry = new PlayerPlayerEntry(*It);
						PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
						TargetIndex = static_cast<PlayerPlayerEntry*>(NewGameEntry)->Target.PlayerIndex;
						break;
					}
				case GET_WeaponInt:
					{
						NewGameEntry = new WeaponEntry(*It);
						PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
						break;
					}
				case GET_DamageInt:
					{	
						NewGameEntry = new DamageEntry(*It);
						PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
						TargetIndex = static_cast<DamageEntry*>(NewGameEntry)->Target.PlayerIndex;
						break;
					}
				case GET_ProjectileInt:
					{
						NewGameEntry = new ProjectileIntEntry(*It);
						PlayerIndex = static_cast<PlayerEntry*>(NewGameEntry)->PlayerIndex;
						break;
					}
				case GET_GenericParamList:
					{
						break;
					}
				default:
					debugf(NAME_GameStats,TEXT("Unknown game stat type %d"), EventType);
					break;
				}

				if (NewGameEntry != NULL)
				{
					AddNewEvent(SessionInfo, SessionData, NewGameEntry, TeamIndex, PlayerIndex, TargetIndex);
				}

				RecordsProcessed++;
			}

			GWarn->EndSlowTask();
		}
		delete RecordSet;
	}
}

/** 
 * Adds a new event created to the array of all events in the file 
 * @param NewEvent - new event to add
 * @param TeamIndex - Team Index for team events (INDEX_NONE if not a team event)
 * @param PlayerIndex - Player Index for player events (INDEX_NONE if not a player event)
 * @param TargetIndex - Target Index for player events (INDEX_NONE if event has no target)
 */
void FGameStatsRemoteDB::AddNewEvent(const FGameSessionInformation& SessionInfo, FGameSessionEntryRemote* SessionData, struct FIGameStatEntry* NewEvent, INT TeamIndex, INT PlayerIndex, INT TargetIndex)
{
	if (NewEvent)
	{
		FLOAT SessionDuration = SessionInfo.GameplaySessionEndTime - SessionInfo.GameplaySessionStartTime;
		// Normalize the time [0, SessionDuration], clamping shouldn't be necessary
		NewEvent->EventTime = Clamp(NewEvent->EventTime - SessionInfo.GameplaySessionStartTime, 0.0f, SessionDuration);

		// Add the event to the "database"
		INT NewEventIndex = SessionData->AllEventsData.AddItem(NewEvent);

		// Add to the event id mapping
		SessionData->EventsByType.Add(NewEvent->EventID, NewEvent);

		if (TeamIndex != INDEX_NONE) 
		{
			// Add to the team mapping
			SessionData->EventsByTeam.Add(TeamIndex, NewEvent);
		}

		if (PlayerIndex != INDEX_NONE)
		{
			// Add to the player mapping
			SessionData->EventsByPlayer.Add(PlayerIndex, NewEvent);
		}

		if (TargetIndex != INDEX_NONE)
		{
			// Add to the player mapping
			SessionData->EventsByPlayer.Add(TargetIndex, NewEvent);
		}

		if (TeamIndex == INDEX_NONE && PlayerIndex == INDEX_NONE)
		{
			SessionData->GameEvents.AddUniqueItem(NewEvent);
		}
	}
}

#undef warnexprf
