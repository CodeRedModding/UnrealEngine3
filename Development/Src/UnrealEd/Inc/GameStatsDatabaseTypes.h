/*=============================================================================
GameStatsDatabaseTypes.h: Various types that are stored in the game stats database
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if _MSC_VER
#pragma once
#endif

#ifndef __GAMESTATSDATABASETYPES_H__
#define __GAMESTATSDATABASETYPES_H__

#include "GameplayEventsUtilities.h"
#include "Database.h"

/** 
 *   Parse a sessionID of the form <GUID>:<INSTANCE> into its constituent parts
 * @param SessionID - session to parse
 * @param SessionGUID - Session GUID 
 * @param InstanceID - Session Instance
 * @return TRUE if parse was successful, FALSE otherwise
 */
UBOOL ParseSessionID(const FString& SessionID, FString& SessionGUID, INT& InstanceID);

/** Remote session storage object */
struct FGameSessionEntryRemote
{
	TArray<FIGameStatEntry*> AllEventsData;
	TArrayNoInit<FIGameStatEntry*> GameEvents;
	TMultiMap<INT, FIGameStatEntry*> EventsByPlayer;
	TMultiMap<INT, FIGameStatEntry*> EventsByRound;
	TMultiMap<INT, FIGameStatEntry*> EventsByType;
	TMultiMap<INT, FIGameStatEntry*> EventsByTeam;

	/** Constructors */
	FGameSessionEntryRemote() {}
	FGameSessionEntryRemote(EEventParm)
	{
		appMemzero(this, sizeof(FGameSessionEntryRemote));
	}
	~FGameSessionEntryRemote()
	{
		Empty();
	}

	/* Clear out all contained data */
	void Empty()
	{
		for (INT i=0; i<AllEventsData.Num(); i++)
		{
			delete AllEventsData(i);
		}
		AllEventsData.Empty();

		GameEvents.Empty();
		EventsByPlayer.Empty();
		EventsByRound.Empty();
		EventsByType.Empty();
		EventsByTeam.Empty();
	}
};


/**
 * Game stats remote database. Used to update/query the game stats database 
 */
struct FGameStatsRemoteDB : FTaskDatabase
{
private:

	/** 
	 *  This will send the text to be "Exec" 'd to the DB proxy 
	 *
	 * @param	ExecCommand  Exec command to send to the Proxy DB.
	 */
	UBOOL SendExecCommand( const FString& ExecCommand );

	/** 
	 *  This will send the text to be "Exec" 'd to the DB proxy 
	 *
	 * @param  ExecCommand  Exec command to send to the Proxy DB.
	 * @param RecordSet		Reference to recordset pointer that is going to hold result
	 */
	UBOOL SendExecCommandRecordSet( const FString& ExecCommand, FDataBaseRecordSet*& RecordSet, UBOOL bAsync = FALSE, UBOOL bNoRecords = FALSE );

	/** ID of the session added to the database with last call to AddGameSessionInfo() */
	LONG					SessionDBID;
	/** Mapping of stream player IDs to database record IDs			*/
	TMap<INT, LONG>			PlayerIDToDBIDMapping;
	/** Mapping of streamed team IDs to database record IDs			*/
	TMap<INT, LONG>			TeamIDToDBIDMapping;

	/*
	 *   Add a new event to the cached database structure
	 * @param SessionInfo - session information related to the event to store
	 * @param SessionData - cache structure for storing the event
	 * @param NewEvent - event to add to the database
	 * @param TeamIndex - Team Index if applicable to the event
	 * @param PlayerIndex - Player Index if applicable to the event
	 * @param TargetIndex - Target Index if applicable to the event
	 */
	void AddNewEvent(const FGameSessionInformation& SessionInfo, FGameSessionEntryRemote* SessionData, struct FIGameStatEntry* NewEvent, INT TeamIndex, INT PlayerIndex, INT TargetIndex);

	/* 
	 *   Given a mapname, find all the sessions in the database
	 * @param MapName - name of the map we are looking at
	 * @param DateFilter - enum of days to filter by
	 * @param GameType - GameType classname or "ANY"
	 * @param GameSessionArray - output of all the session information found
	 */
	UBOOL GetGameSessionInfosForMap(const FString& MapName, GameStatsDateFilters DateFilter, const FString& GameType, TArray<struct FGameSessionInformation>& GameSessionArray);

	/** 
	 *   Populate the game session info arrays for a given map/time range
	 * @param InMapName - map to query data for
	 * @param DateFilter - enum of date range to filter by
	 * @param GameTypeFilter - Gametype classname or "ANY"
	 */
	void UpdateGameSessionInfo(const FString& InMapname, GameStatsDateFilters DateFilter, const FString& GameTypeFilter);

	TMap<INT, struct FGameSessionEntryRemote> AllSessions;
	TMap<FString, struct FGameSessionInformation> SessionInfoBySessionID;
	TMap<INT, struct FGameSessionInformation> SessionInfoBySessionDBID;
	TMap<INT, TArray<struct FPlayerInformation> > PlayerListBySessionDBID;
	TMap<INT, TArray<struct FTeamInformation> > TeamListBySessionDBID;
	TMap<INT, TArray<struct FGameplayEventMetaData> > SupportedEventsBySessionDBID;
	TArray<FString> AllGameTypes;

public:
	/**
	 * Constructor, initializing all member variables. It also reads .ini settings and caches them and creates the
	 * database connection object and attempts to connect.
	 */
	FGameStatsRemoteDB();

	/** Destructor, cleaning up connection if it was in use. */
	virtual ~FGameStatsRemoteDB();

	/** Time spent communicating with DB (in seconds).				*/
	DOUBLE					TimeSpentTalkingWithDB;

	FString MapName;

	/** 
	 *   Initialize the remote db data structures from a given map/time range
	 * @param InMapName - map to query data for
	 * @param DateFilter - enum of date range to filter by
	 */
	void Init(const FString& InMapname, BYTE DateFilter);

	/** @return TRUE if connection exists to a remote DB, FALSE otherwise */
	UBOOL IsConnected() { return Connection != NULL; }

	/** 
	 *  Opens a connection to the configured database 
	 */
	void OpenConnection();

	/** 
	 *  Closes the database connection 
	 */
	void CloseConnection()
	{
		if (Connection)
		{
			Connection->Close();
			delete Connection;
			Connection = NULL;
		}
	}

	/** 
     * Query the remote SQL database and pull down the session data 
     * @param SessionDBID - remote session ID in the SQL database
     */
	void PopulateSessionData(INT SessionDBID);

	/** Reset the database interface */
	void Reset()
	{
		MapName.Empty();

		//Write data
		SessionDBID = -1;
		PlayerIDToDBIDMapping.Empty();
		TeamIDToDBIDMapping.Empty();

		//Read data
		AllSessions.Empty();

		SessionInfoBySessionID.Empty();
		SessionInfoBySessionDBID.Empty();
		PlayerListBySessionDBID.Empty();
		TeamListBySessionDBID.Empty();
		SupportedEventsBySessionDBID.Empty();

		AllGameTypes.Empty();
	}

	INT QueryDatabase(const FGameStatsSearchQuery& Query, TArray<FIGameStatEntry*>& Events);

	/**
	 * Allows a visitor interface access to every database entry of interest
	 * @param EventIndices - all events the visitor wants access to
	 * @param Visitor - the visitor interface that will be accessing the data
	 */
	void VisitEntries(const TArray<FIGameStatEntry*>& Events, IGameStatsDatabaseVisitor* Visitor);

	/** 
	 *   Returns whether or not the given session exists in the database
	 *   @param SessionID - the session to check against
	 *   @return TRUE if it exists, FALSE otherwise
	 */
	UBOOL DoesSessionExistInDB(const FString& SessionID);

   /*
    * Get the gametypes in the database
    * @param GameTypes - array of all gametypes in the database
    */
	virtual void GetGameTypes(TArray<FString>& GameTypes);

	/*
	 * Get the session ids in the database
	 * @param DateFilter - Date enum to filter by
	 * @param GameTypeFilter - GameClass name or "ANY"
	 * @param SessionIDs - array of all sessions in the database
	 */
	virtual void GetSessionIDs(BYTE DateFilter,const FString& GameTypeFilter,TArray<FString>& SessionIDs);
	virtual void GetSessionInfoBySessionDBID(INT SessionDBID, struct FGameSessionInformation& OutSessionInfo);
	virtual void GetPlayersListBySessionDBID(INT SessionDBID, TArray<struct FPlayerInformation>& OutPlayerList);
	virtual void GetTeamListBySessionDBID(INT SessionDBID, TArray<struct FTeamInformation>& OutTeamList);
	virtual void GetEventsListBySessionDBID(INT SessionDBID, TArray<struct FGameplayEventMetaData>& OutGameplayEvents);
	//virtual void GetWeaponListBySessionID(const FString& SessionId,TArray<struct FWeaponClassEventData>& OutWeaponList);
	//virtual void GetDamageListBySessionID(const FString& SessionId,TArray<struct FDamageClassEventData>& OutDamageList);
	//virtual void GetProjectileListBySessionID(const FString& SessionId,TArray<struct FProjectileClassEventData>& OutProjectileList);
	//virtual void GetPawnListBySessionID(const FString& SessionId,TArray<struct FPawnClassEventData>& OutPawnList);
	virtual INT GetEventCountByType(const FString& SessionId, INT EventID);

	/*
	 * Get all events associated with a given player
	 * @param SessionID - session we're interested in
	 * @param PlayerIndex - the player to return the events for (INDEX_NONE is all players)
	 * @param Events - array of events related to relevant player events
	 */
	virtual INT GetEventsByPlayer(const FString& SessionID, INT PlayerIndex, TArray<FIGameStatEntry*>& Events);

	/*
	 * Get all events associated with a given team
	 * @param SessionID - session we're interested in
	 * @param TeamIndex - the team to return the events for (INDEX_NONE is all teams)
	 * @param Events - array of events related to relevant team events
	 */
	virtual INT GetEventsByTeam(const FString& SessionID, INT TeamIndex, TArray<FIGameStatEntry*>& Events);

	/*
	 * Get all events associated with a given game (non-team/player)
	 * @param SessionID - session we're interested in
	 * @param Events - array of events from the game
	 */
	virtual INT GetGameEventsBySession(const FString& SessionID, TArray<FIGameStatEntry*>& Events);
};

/**
 * Game stats location entry.  SubEntry type for any event that contains a location and orientation 
 */
class LocationEntry : public FIGameStatEntry
{
public:

	LocationEntry() {}
	LocationEntry(const FGameEventHeader& GameEvent, const FRotator& InRotation, const FVector& InLocation)
		: FIGameStatEntry(GameEvent), Rotation(InRotation), Location(InLocation) {}
	LocationEntry(class FDataBaseRecordSet* RecordSet, UBOOL bHasRotation = TRUE);

	/** Orientation of the event */
	FRotator Rotation;

	/** Location of the event */
	FVector Location;
};

/**
 * Game stats game string entry.  High level event for anything occurring at the game level
 */
class GameStringEntry : public FIGameStatEntry
{
public:

	GameStringEntry(const FGameEventHeader& GameEvent, const struct FGameStringEvent* GameStringEvent);
	GameStringEntry(class FDataBaseRecordSet* RecordSet);

	/** Generic container for anything STRING */
	FString String;

	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats game int entry.  High level event for anything occurring at the game level
 */
class GameIntEntry : public FIGameStatEntry
{
public:
	GameIntEntry(const FGameEventHeader& GameEvent, const struct FGameIntEvent* GameIntEvent);
	GameIntEntry(class FDataBaseRecordSet* RecordSet);
	
	/** Generic container for anything INT */
	INT Value;

	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats game float entry.  High level event for anything occurring at the game level
 */
class GameFloatEntry : public FIGameStatEntry
{
public:
	GameFloatEntry(const FGameEventHeader& GameEvent, const struct FGameFloatEvent* GameIntEvent);
	GameFloatEntry(class FDataBaseRecordSet* RecordSet);
	
	/** Generic container for anything FLOAT */
	FLOAT Value;

	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats game position entry.  High level event for anything occurring at the game level
 */
class GamePositionEntry : public LocationEntry
{
public:
	GamePositionEntry(const FGameEventHeader& GameEvent, const struct FGamePositionEvent* GamePositionEvent);
	GamePositionEntry(class FDataBaseRecordSet* RecordSet);
	
	/** Generic container for anything FLOAT */
	FLOAT Value;

	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats team entry.  SubEntry type for any event that contains information about a team
 */
class TeamEntry : public FIGameStatEntry
{
public:

	TeamEntry() {}
	TeamEntry(const FGameEventHeader& GameEvent, INT InTeamIndex)
		: FIGameStatEntry(GameEvent), TeamIndex(InTeamIndex) {}
	TeamEntry(class FDataBaseRecordSet* RecordSet);

	/** Team index of the event recorded */
	INT TeamIndex;
};

/**
 * Game stats team int entry.  High level event for anything occurring at the team level
 */
class TeamIntEntry : public TeamEntry
{
public:

	TeamIntEntry::TeamIntEntry(const FGameEventHeader& GameEvent, const struct FTeamIntEvent* TeamIntEvent)
		: TeamEntry(GameEvent, TeamIntEvent->TeamIndex), Value(TeamIntEvent->Value)
	{}

	TeamIntEntry::TeamIntEntry(FDataBaseRecordSet* Record) : TeamEntry(Record)
	{}

	/** Generic container for anything INT */
	INT Value;
	
	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats team float entry.  High level event for anything occurring at the team level
 */
class TeamFloatEntry : public TeamEntry
{
public:

	TeamFloatEntry::TeamFloatEntry(const FGameEventHeader& GameEvent, const struct FTeamFloatEvent* TeamFloatEvent)
		: TeamEntry(GameEvent, TeamFloatEvent->TeamIndex), Value(TeamFloatEvent->Value)
	{}

	TeamFloatEntry::TeamFloatEntry(FDataBaseRecordSet* Record) : TeamEntry(Record)
	{}

	/** Generic container for anything FLOAT */
	FLOAT Value;
	
	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats team string entry.  High level event for anything occurring at the team level
 */
class TeamStringEntry : public TeamEntry
{
public:

	TeamStringEntry::TeamStringEntry(const FGameEventHeader& GameEvent, const struct FTeamStringEvent* TeamStringEvent)
		: TeamEntry(GameEvent, TeamStringEvent->TeamIndex), Value(TeamStringEvent->StringEvent)
	{}

	TeamStringEntry::TeamStringEntry(FDataBaseRecordSet* Record) : TeamEntry(Record)
	{}

	/** Generic container for anything STRING */
	FString Value;
	
	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats player entry.  SubEntry type for any event containing information about a player
 */
class PlayerEntry : public LocationEntry
{
public:

	PlayerEntry() {}
	PlayerEntry(const FGameEventHeader& GameEvent, INT InPlayerIndexAndYaw, INT InPlayerPitchAndRoll, const FVector& InLocation);
	PlayerEntry(class FDataBaseRecordSet* RecordSet);

	/** Index into player metadata array */
	INT PlayerIndex;
	/** Player name as recorded at the time of the event */
	FString PlayerName;
};

/**
 * Game stats player string entry.  High level event for anything occurring at the player level
 */
class PlayerStringEntry : public PlayerEntry
{
public:

	PlayerStringEntry(const FGameEventHeader& GameEvent, const struct FPlayerStringEvent* PlayerStringEvent);
	PlayerStringEntry(class FDataBaseRecordSet* RecordSet);

	/** Generic container for anything STRING */
	FString StringEvent;

	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats player int entry.  High level event for anything occurring at the player level
 */
class PlayerIntEntry : public PlayerEntry
{
public:
	PlayerIntEntry(const FGameEventHeader& GameEvent, const struct FPlayerIntEvent* PlayerIntEvent);
	PlayerIntEntry(class FDataBaseRecordSet* RecordSet, UBOOL bHasValue = TRUE);

	/** Generic container for anything INT */
	INT Value;

	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats player float entry.  High level event for anything occurring at the player level
 */
class PlayerFloatEntry : public PlayerEntry
{
public:
	PlayerFloatEntry(const FGameEventHeader& GameEvent, const struct FPlayerFloatEvent* PlayerFloatEvent);
	PlayerFloatEntry(class FDataBaseRecordSet* RecordSet);
	
	/** Generic container for anything FLOAT */
	FLOAT Value;

	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats player login entry.  High level event for recording a player login
 */
class PlayerLoginEntry : public PlayerEntry
{
public:
	PlayerLoginEntry(const FGameEventHeader& GameEvent, const struct FPlayerLoginEvent* PlayerLoginEvent);
	PlayerLoginEntry(class FDataBaseRecordSet* RecordSet);

	/* Splitscreen player */
	UBOOL bSplitScreen;

	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats player spawn entry.  High level event for recording a player spawn during a match
 */
class PlayerSpawnEntry : public PlayerEntry
{
public:
	PlayerSpawnEntry(const FGameEventHeader& GameEvent, const struct FPlayerSpawnEvent* PlayerSpawnEvent);
	PlayerSpawnEntry(class FDataBaseRecordSet* RecordSet);
	
	/** Index into pawn class metadata array */
	INT PawnClassIndex;
	/** Name of pawn spawned for the player */
	FString PawnClassName;
	/** Team requested for player at time of spawn */
	INT TeamIndex;

	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats player kill entry.  High level event for recording the killing of one player by another
 */
class PlayerKillDeathEntry : public PlayerEntry
{
public:

	PlayerKillDeathEntry(const FGameEventHeader& GameEvent, const struct FPlayerKillDeathEvent* PlayerKillDeathEvent);
	PlayerKillDeathEntry(class FDataBaseRecordSet* RecordSet);

	/** Player's killed target */
	PlayerEntry Target;

	/** Index into damage class metadata array */
	INT DamageClassIndex;
	/** Name of the class doing damage to the player */
	FString DamageClassName;
	/** Type of kill (game specific enum)  */
	INT KillType;
	/** Name of the kill type */
	FString KillTypeString;

	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats player player entry.  High level event for recording an interaction between two players
 */
class PlayerPlayerEntry : public PlayerEntry
{
public:

	PlayerPlayerEntry(const FGameEventHeader& GameEvent, const struct FPlayerPlayerEvent* PlayerPlayerEvent);
	PlayerPlayerEntry(class FDataBaseRecordSet* RecordSet);

	/** Other player involved */
	PlayerEntry Target;

	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats weapon entry.  High level event for recording weapon use
 */
class WeaponEntry : public PlayerEntry
{
public:

	WeaponEntry(const FGameEventHeader& GameEvent, const struct FWeaponIntEvent* WeaponIntEvent);
	WeaponEntry(class FDataBaseRecordSet* RecordSet);

	/** Index into the weapon class metadata array */
	INT WeaponClassIndex;
	/** Name of weapon class involved in the event */
	FString WeaponClassName;
	/** Generic container for anything INT */
	INT Value;

	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats damage entry.  High level event for anything involving damage to players
 */
class DamageEntry : public PlayerEntry
{
public:

	DamageEntry(const FGameEventHeader& GameEvent, const struct FDamageIntEvent* DamageIntEvent);
	DamageEntry(class FDataBaseRecordSet* RecordSet);

	/** Other player involved */
	PlayerEntry Target;

	/** Index into damage class metadata array */
	INT DamageClassIndex;
	/** Name of the class doing damage to the player */
	FString DamageClassName;
	/** Generic container for anything INT */
	INT Value;

	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Game stats projectile entry.  High level event for anything involving projectile use
 */
class ProjectileIntEntry : public PlayerEntry
{
public:

	ProjectileIntEntry(const FGameEventHeader& GameEvent, const struct FProjectileIntEvent* ProjectileIntEvent);
	ProjectileIntEntry(class FDataBaseRecordSet* RecordSet);
	
	/** Index into projectile class metadata array */
	INT ProjectileClassIndex;
	/** Name of the projectile class */
	FString ProjectileName;
	/** Generic container for anything INT */
	INT Value;

	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};

/**
 * Entry for generic event based on param list
 */
class GenericParamListEntry : public FIGameStatEntry, public FGenericParamListEvent
{
public:
	GenericParamListEntry(const FGameEventHeader& GameEvent, const struct FGenericParamListEvent* ParamEvent);
	GenericParamListEntry(class FDataBaseRecordSet* RecordSet);
	
	/** 
	 * Every concrete entry type must handle/accept the visitor interface 
	 * @param Visitor - Interface class wanting access to the entry
	 */
	void Accept(IGameStatsDatabaseVisitor* Visitor)
	{
		Visitor->Visit(this);
	}
};


#endif //__GAMESTATSDATABASETYPES_H__
