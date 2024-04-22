/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*
* Streams gameplay events recorded during a session to disk
*/
class GameStatsDatabase extends Object
	native(GameStats)
	config(Editor);

/** Date Filters */
enum GameStatsDateFilters
{
	GSDF_Today,
	GSDF_Last3Days,
	GSDF_LastWeek
};

/** Cached mapping of all files found on editor load to the maps they represent */
var const private native transient MultiMap_Mirror MapNameToFilenameMapping{TMultiMap<FString, FString>};

/** All events in the database */
var	const private native transient array<pointer> AllEvents{FIGameStatEntry};

/** Mapping of all session query indices by session ID */
var const private native transient Map_Mirror AllSessions{TMap<FString, struct FGameSessionEntry>};

/** Mapping of session id to local filename */
var const private native transient  Map_Mirror  SessionFilenamesBySessionID{TMap<FString, FString>};

/** Mapping of session metadata by session id */
var const private native transient  Map_Mirror  SessionInfoBySessionID{TMap<FString, struct FGameSessionInformation>};
/** Mapping of recorded player metadata by session id */
var	const private native transient	Map_Mirror	PlayerListBySessionID{TMap<FString, TArray<struct FPlayerInformation> >};
/** Mapping of recorded team metadata by session id */
var	const private native transient	Map_Mirror	TeamListBySessionID{TMap<FString, TArray<struct FTeamInformation> >};
/** Mapping of recorded event metadata by session id */
var	const private native transient	Map_Mirror	SupportedEventsBySessionID{TMap<FString, TArray<struct FGameplayEventMetaData> >};
/** Mapping of recorded weapon class metadata by session id */
var	const private native transient	Map_Mirror	WeaponClassesBySessionID{TMap<FString, TArray<struct FWeaponClassEventData> >};
/** Mapping of recorded damage class metadata by session id */
var	const private native transient	Map_Mirror	DamageClassesBySessionID{TMap<FString, TArray<struct FDamageClassEventData> >};
/** Mapping of recorded projectile class metadata by session id */
var	const private native transient	Map_Mirror	ProjectileClassesBySessionID{TMap<FString, TArray<struct FProjectileClassEventData> >};
/** Mapping of recorded pawn class metadata by session id */
var	const private native transient	Map_Mirror	PawnClassesBySessionID{TMap<FString, TArray<struct FPawnClassEventData> >};

/** Unique gametypes found in the database */
var const transient array<string> AllGameTypes;


/** REMOTE IMPLEMENTATION **/

/** Pointer to the remote database interface */
var const private native transient pointer RemoteDB{struct FGameStatsRemoteDB};

/** Name of the class responsible for parsing stats file on disk */
var config string GameStatsFileReaderClassname;
/** Name of the class responsible for game state while parsing the stats file */
var config string GameStateClassname;

/** Representation of some session/value pair for database queries */
struct native SessionIndexPair
{
	/** The session this index is relevant for */
	var init string SessionId;
	/** The index we're searching for */
	var int Index;

	structcpptext
	{
		FSessionIndexPair(EEventParm)
		{
			appMemzero(this, sizeof(FSessionIndexPair));
		}

		FSessionIndexPair(const FString& InSessionId, const INT InIndex) 
			: SessionId(InSessionId), Index(InIndex) {}
	}
};

/** The struct containing the current notion of a game stats database query */
struct native GameStatsSearchQuery
{
	/** Min time in query */
	var int StartTime;

	/** Max time in query */
	var int EndTime;

	/** Array of relevant session IDs */
	var array<string> SessionIDs;

	/** Array of relevant event IDs */
	var array<int> EventIDs;

	/** Array of relevant team indices */ 
	var array<SessionIndexPair> TeamIndices;

	/** Array of relevant player indices */
	var array<SessionIndexPair> PlayerIndices;

	structcpptext
	{
		/** Constructors */
		FGameStatsSearchQuery() {}
		FGameStatsSearchQuery(EEventParm)
		{
			appMemzero(this, sizeof(FGameStatsSearchQuery));
		}

		enum SearchQueryTypes
		{
			ALL_PLAYERS =  INDEX_NONE,
			ALL_TEAMS =    INDEX_NONE,
			ALL_EVENTS =   INDEX_NONE,
		};
	}
};

/** Organizational notion of a stats session */
struct native GameSessionEntry
{
	/** Mapping of session ID to events recorded */
	var	const init transient array<int> AllEvents;

	/** Any event that isn't specific to a player or team */
	var const transient array<int> GameEvents;

	/** Mapping of player index to events recorded */
	var	const native transient	MultiMap_Mirror	EventsByPlayer{TMultiMap<INT, INT>};

	/** Mapping of round index to events recorded */
	var	const native transient	MultiMap_Mirror	EventsByRound{TMultiMap<INT, INT>};

	/** Mapping of event index to events of that type */
	var	const native transient	MultiMap_Mirror	EventsByType{TMultiMap<INT, INT>};

	/** Mapping of team index to events recorded */
	var	const native transient	MultiMap_Mirror	EventsByTeam{TMultiMap<INT, INT>};

	structcpptext
	{
		/** Constructors */
		FGameSessionEntry() {}
		FGameSessionEntry(EEventParm)
		{
			appMemzero(this, sizeof(FGameSessionEntry));
		}

		/* Clear out all contained data */
		void Empty()
		{
			AllEvents.Empty();
			GameEvents.Empty();
			EventsByPlayer.Empty();
			EventsByRound.Empty();
			EventsByType.Empty();
			EventsByTeam.Empty();
		}
	}
};

/** Base implementation of a "database" entry */
struct native IGameStatEntry
{
	structcpptext
	{
		FIGameStatEntry() {}
		FIGameStatEntry(const struct FGameEventHeader& GameEvent);
		FIGameStatEntry(class FDataBaseRecordSet* RecordSet);

		/** 
		 * Every entry type must handle/accept the visitor interface 
		 * @param Visitor - Interface class wanting access to the entry
		 */
		virtual void Accept(class IGameStatsDatabaseVisitor* Visitor)
		{
			ensureMsg(0, TEXT("Game stats database entry type didn't implement Accept function!"));
		}
	}

	/** VTbl placeholder */
	var	native const noexport pointer Vtbl_IGameStatEntry;

	/** Basic components of a game stat entry in the db */
	var init string					EventName;
	var int							EventID;
	var float						EventTime;
};

/** Implementation of a query result set */
struct native GameStatsRecordSet
{
	var init array<int> LocalRecordSet;
	var init native array<pointer> RemoteRecordSet{FIGameStatEntry};

	structcpptext
	{
		INT GetNumResults() { return LocalRecordSet.Num() + RemoteRecordSet.Num(); }
	}
};

cpptext
{
public:

	/** @return TRUE if this database is able to access a remote database */
	UBOOL HasRemoteConnection() { return RemoteDB != NULL; }

	/** 
	* Query this database
	* @param SearchQuery - the query to run on the database
	* @param Events - out array of indices of relevant events in the database
	* @return the number of results found for this query
	*/
	virtual INT QueryDatabase(const struct FGameStatsSearchQuery& Query, struct FGameStatsRecordSet& RecordSet);

	/**
	* Allows a visitor interface access to every database entry of interest
	* @param SessionID - session we're interested in
	* @param EventIndices - all events the visitor wants access to
	* @param Visitor - the visitor interface that will be accessing the data
	* @return TRUE if the visitor got what it needed from the visit, FALSE otherwise
	*/
	virtual UBOOL VisitEntries(const struct FGameStatsRecordSet& RecordSet, class IGameStatsDatabaseVisitor* Visitor);

	/**
	 *   Fill in database structures for a given sessionID if necessary
	 * @param SessionID - session to cache data for
	 */
	virtual void PopulateSessionData(const FString& SessionID);

protected:
	/*
	 * Get all events associated with a given session
	 * @param SessionID - session we're interested in
	 * @param Events - array of indices related to relevant session events
	 */
	virtual INT GetEventsBySessionID(const FString& SessionID, TArray<INT>& Events);

	/*
	 * Get all game events associated with a given session (neither player nor team)
	 * @param SessionID - session we're interested in
	 * @param Events - array of indices related to relevant game events
	 */
	virtual INT GetGameEventsBySession(const FString& SessionID, TArray<INT>& Events);

	/*
	 * Get all events associated with a given team
	 * @param SessionID - session we're interested in
	 * @param TeamIndex - the team to return the events for (INDEX_NONE is all teams)
	 * @param Events - array of indices related to relevant team events
	 */
	virtual INT GetEventsByTeam(const FString& SessionID, INT TeamIndex, TArray<INT>& Events);

	/*
	 * Get all events associated with a given player
	 * @param SessionID - session we're interested in
	 * @param PlayerIndex - the player to return the events for (INDEX_NONE is all players)
	 * @param Events - array of indices related to relevant player events
	 */
	virtual INT GetEventsByPlayer(const FString& SessionID, INT PlayerIndex, TArray<INT>& Events);

	/*
	 * Get all events associated with a given round
	 * @param SessionID - session we're interested in
	 * @param RoundNumber - the round to return events for  (INDEX_NONE is all rounds)
	 * @param Events - array of indices related to relevant round events
	 */
	virtual INT GetEventsByRound(const FString& SessionID, INT RoundNumber, TArray<INT>& Events);
	
	/*
	 * Get all events associated with a given event ID
	 * @param SessionID - session we're interested in
	 * @param EventID - the event of interest (INDEX_NONE is all events)
	 * @param Events - array of indices related to relevant events
	 */
	virtual INT GetEventsByID(const FString& SessionID, INT EventID, TArray<INT>& Events);

	/** Searches the stats directory for relevant data files and populates the database */
	virtual void LoadLocalData(const FString& MapName, GameStatsDateFilters DateFilter);

	/** Connects to the remote database and populates the db with data */
	virtual void LoadRemoteData(const FString& MapName, GameStatsDateFilters DateFilter);

	/** 
	 *   Open a game stats file for reading
	 * @param Filename - name of the file that will be open for serialization
	 * @return TRUE if successful, else FALSE
	 */
	virtual UBOOL OpenStatsFile(const FString& Filename);
};

/*
 * Initialize the database session for a given map
 * @param MapName - database will be populated with data relevant to this map
 */
native function Init(const string MapName, GameStatsDateFilters DateFilter);

/*
 *   Iterate over all valid files in the stats directory and create a mapping of map name to filename
 */
native function CacheLocalFilenames();

/*
 * Get the gametypes in the database
 * @param GameTypes - array of all gametypes in the database
 */
native function GetGameTypes(out array<string> GameTypes);

/*
 * Get the session ids in the database
 * @param DateFilter - Date enum to filter by
 * @param GameTypeFilter - GameClass name or "ANY"
 * @param SessionIDs - array of all sessions in the database
 */
native function GetSessionIDs(GameStatsDateFilters DateFilter, const string GameTypeFilter, out array<string> SessionIDs);

/*
 *   Is the session ID found in a local file or in the remote database
 * @param SessionID - session of interest
 * @return TRUE if this data is from a file on disk, FALSE if remote database
 */
native function bool IsSessionIDLocal(const string SessionID);

/** 
 *  Get a list of the players by session ID
 * @param SessionID - session ID to get the list for
 * @param PlayerList - output array of players
 */
native function GetSessionInfoBySessionID(const string SessionID, out GameSessionInformation OutSessionInfo);

/** 
 *  Get a list of the players by session ID
 * @param SessionID - session ID to get the list for
 * @param PlayerList - output array of players
 */
native function GetPlayersListBySessionID(const string SessionID, out array<PlayerInformation> OutPlayerList);

/** 
 *  Get a list of the teams by session ID
 * @param SessionID - session ID to get the list for
 * @param PlayerList - output array of teams
 */
native function GetTeamListBySessionID(const string SessionID, out array<TeamInformation> OutTeamList);

/** 
 *  Get a list of the recorded events by session ID
 * @param SessionID - session ID to get the list for
 * @param PlayerList - output array of events
 */
native function GetEventsListBySessionID(const string SessionID, out array<GameplayEventMetaData> OutGameplayEvents);

/** 
 *  Get a list of the recorded weapons by session ID
 * @param SessionID - session ID to get the list for
 * @param PlayerList - output array of weapons
 */
native function GetWeaponListBySessionID(const string SessionID, out array<WeaponClassEventData> OutWeaponList);

/** 
 *  Get a list of the recorded damage types by session ID
 * @param SessionID - session ID to get the list for
 * @param PlayerList - output array of damage types
 */
native function GetDamageListBySessionID(const string SessionID, out array<DamageClassEventData> OutDamageList);

/** 
 *  Get a list of the recorded projectiles by session ID
 * @param SessionID - session ID to get the list for
 * @param PlayerList - output array of projectiles
 */
native function GetProjectileListBySessionID(const string SessionID, out array<ProjectileClassEventData> OutProjectileList);

/** 
 *  Get a list of the recorded pawn types by session ID
 * @param SessionID - session ID to get the list for
 * @param PlayerList - output array of pawn types
 */
native function GetPawnListBySessionID(const string SessionID, out array<PawnClassEventData> OutPawnList);

/*
 * Get the total count of events of a given type
 * @param SessionID - session we're interested in
 * @param EventID - the event to return the count for
 */
native function int GetEventCountByType(const string SessionID, int EventID);

/*
 *   Empty all tables in the database
 */
native function ClearDatabase();

/*
 * Upload a given session ID to the master database
 * @SessionID - session ID to upload
 * @return TRUE if successful, FALSE for any error condition
 */
native function bool UploadSession(const string SessionID);
