/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Gameplay event interface
 */
class GameplayEvents extends Object
	abstract
	native;

`include(Engine\Classes\GameStats.uci);	

// Bitmasks for flags stored on the MCP header
const HeaderFlags_NoEventStrings = 1;

/** Stat verbosity level */
enum EGameStatGroups
{
	GSG_EngineStats,
	GSG_Game,
	GSG_Team,
	GSG_Player,
	GSG_Weapon,
	GSG_Damage,
	GSG_Projectile,
	GSG_Pawn,
	GSG_GameSpecific,
	GSG_Aggregate
};

struct native GameStatGroup
{
	/** Group stat belongs to*/
	var EGameStatGroups Group;
	/** Level of the stat */
	var int Level;

	structcpptext
	{
		friend FArchive& operator<<( FArchive& Ar, FGameStatGroup& T );
	}
};

/** Basic file header when writing to disk */
struct native GameplayEventsHeader
{   
	/** Version of engine at the time of writing the file */
	var const int EngineVersion;

	/** Version of the stats format at the time of writing the file */
	var const int StatsWriterVersion;

	/** Offset into the file for the stream data */
	var const int StreamOffset;

	/** Offset into the file for aggregate data */
	var const int AggregateOffset;

	/** Offset into the file where the metadata is written */
	var const int FooterOffset;

	/** Amount of data in the stream (not including header/footer data) */
	var const int TotalStreamSize;

	/** File size on disk */
	var const int FileSize;

	/** What filter class is being used on the data, if any */
	var string FilterClass;

	/** Various settings */
	var int Flags;
};

/** Game stats session information recorded at log start */
struct native GameSessionInformation
{
	/** Unique title identifier */
	var int AppTitleID;

	/** Platform the session was run on */
	var int PlatformType;

	/** Language the session was run in */
	var string Language;

	/** Time this session was begun (real time) */
	var const string GameplaySessionTimestamp;

	/** Time this session was started (game time) */
	var const float GameplaySessionStartTime;

	/** Time this session was ended (game time) */
	var const float GameplaySessionEndTime;

	/** Is a session currently in progress */
	var const bool bGameplaySessionInProgress;

	/** Unique session ID */
	var const string GameplaySessionID;

	/** Name of the game class used */
	var const string GameClassName;

	/** Name of map at time of session  */
	var const string MapName;

	/** Game URL at time of session */
	var const string MapURL;

	/** Value used to distinguish between contiguous sections */
	var const int SessionInstance;

	/** Gametype ID */
	var const int GameTypeId;

	/** UniqueID of player logging stats */
	var const UniqueNetId OwningNetId;

	/** ID of the playlist in use */
	var int PlaylistId;

	structcpptext
	{
		/** Constructors */
		FGameSessionInformation() {}
		FGameSessionInformation(EEventParm)
		{
			appMemzero(this, sizeof(FGameSessionInformation));
		}

		/** Return the unique key for this session */
		const FString GetSessionID() const { return FString::Printf(TEXT("%s:%d"), *GameplaySessionID, SessionInstance); }
	}
};

/** List of team information cached during the play session */
struct native TeamInformation
{
	/** Index of the team in game */
	var int TeamIndex;

	/** Name of the team */
	var string TeamName;

	/** Color of the team */
	var color TeamColor;

	/** Max size during the game */
	var int MaxSize;
	
	structcpptext
	{
		friend FArchive& operator<<( FArchive& Ar, FTeamInformation& T );
	}
};

/** List of player information cached in case the player logs out and GC collects the objects */
struct native PlayerInformation
{
	/** Name of Controller object */
	var name ControllerName;
	/** Controller.PlayerReplicationInfo.PlayerName */
	var string PlayerName;
	/** UniqueID of the player */
	var UniqueNetId UniqueId;
	/** Whether the player is a bot or not */
	var bool bIsBot;
	
	structcpptext
	{
		friend FArchive& operator<<( FArchive& Ar, FPlayerInformation& T );
	}
};

/** Holds the information describing a gameplay event */
struct native GameplayEventMetaData
{
	/** The unique id of the event (16 bits clamped) */
	var const int EventID;

	/** Human readable name of the event */
	var const name EventName;

	/** Group that this stat belongs to, for filtering */
	var const GameStatGroup StatGroup;

	/** Type of data associated with this event */
	var const int EventDataType;
	
	structcpptext
	{
		friend FArchive& operator<<( FArchive& Ar, FGameplayEventMetaData& T );
	}
};

/** Metadata describing the weapon classes recorded during gameplay */
struct native WeaponClassEventData
{
	/** Name of the weapon class used **/
	var name WeaponClassName;
	
	structcpptext
	{
		friend FArchive& operator<<( FArchive& Ar, FWeaponClassEventData& T );
	}
};

/** Metadata describing the damage classes recorded during gameplay */
struct native DamageClassEventData
{
	/** Name of the damage class used **/
	var name DamageClassName;
	
	structcpptext
	{
		friend FArchive& operator<<( FArchive& Ar, FDamageClassEventData& T );
	}
};

/** Metadata describing the projectile classes recorded during gameplay */
struct native ProjectileClassEventData
{
	/** name of the projectile class used **/
	var name ProjectileClassName;
	
	structcpptext
	{
		friend FArchive& operator<<( FArchive& Ar, FProjectileClassEventData& T );
	}
};

/** Metadata describing the pawn classes recorded during gameplay */
struct native PawnClassEventData
{
	/** Name of the pawn class used **/
	var name PawnClassName;
	
	structcpptext
	{
		friend FArchive& operator<<( FArchive& Ar, FPawnClassEventData& T );
	}
};

cpptext
{
	/** Access the current session info */
	const FGameSessionInformation& GetSessionInfo() const
	{
		return CurrentSessionInfo;
	}

	/** Returns the metadata associated with the given index */
	virtual const FGameplayEventMetaData& GetEventMetaData(INT EventID) const;

	/** Returns the metadata associated with the given index */
	const FTeamInformation& GetTeamMetaData(INT TeamIndex) const;

	/** Returns the metadata associated with the given index */
	const FPlayerInformation& GetPlayerMetaData(INT PlayerIndex) const;

	/** Returns the metadata associated with the given index */
	const FPawnClassEventData& GetPawnMetaData(INT PawnClassIndex) const;

	/** Returns the metadata associated with the given index */
	const FWeaponClassEventData& GetWeaponMetaData(INT WeaponClassIndex) const;

	/** Returns the metadata associated with the given index */
	const FDamageClassEventData& GetDamageMetaData(INT DamageClassIndex) const;

	/** Returns the metadata associated with the given index */
	const FProjectileClassEventData& GetProjectileMetaData(INT ProjectileClassIndex) const;

	/**
	 * Returns the metadata associated with the given index
	 * @param ActorIndex the index of the actor being looked up
	 * @return the name of the actor at that index
	 */
	const FString& GetActorMetaData(INT ActorIndex) const
	{
		return ActorArray(ActorIndex);
	}

	/**
	 * Returns the metadata associated with the given index
	 * @param SoundIndex the index of the soundcue being looked up
	 * @return the name of the actor at that index
	 */
	const FString& GetSoundMetaData(INT SoundIndex) const
	{
		return SoundCueArray(SoundIndex);
	}
};

/** FArchive pointer to serialize the data to/from disk */
var const native pointer Archive{FArchive};

/** The name of the file we are writing the data to (const so set only upon create natively) */
var const private string StatsFileName;

/** Header of the gameplay events file */
var GameplayEventsHeader Header;

/** Information specific to the session when it was run */
var GameSessionInformation CurrentSessionInfo;

/** Array of all players 'seen' by the game in this session **/
var const array<PlayerInformation> PlayerList;

/** Array of all teams 'seen' by the game in this session **/
var const array<TeamInformation> TeamList;

/** The set of events that the game supports writing to disk */
var array<GameplayEventMetaData> SupportedEvents;

/** The set of weapons recorded during gameplay */
var array<WeaponClassEventData> WeaponClassArray;

/** The set of damage types recorded during gameplay */
var array<DamageClassEventData> DamageClassArray;

/** The set of projectiles recorded during gameplay */
var array<ProjectileClassEventData> ProjectileClassArray;

/** The set of pawns recorded during gameplay */
var array<PawnClassEventData> PawnClassArray;

/** The set of actors recorded during gameplay */
var array<String> ActorArray;

/** The set of sound cues encountered during gameplay */
var array<String> SoundCueArray;

/** 
 *   Creates the archive that we are going to be manipulating 
 * @param Filename - name of the file that will be open for serialization
 * @return TRUE if successful, else FALSE
 */
function bool OpenStatsFile(string Filename);

/** 
 * Closes and deletes the archive created
 * clearing all data stored within
 */
function CloseStatsFile();

/** Retrieve the name of the file last in use by the gameplay event serializer, possibly empty */
event string GetFilename()
{
	return StatsFileName;
}

defaultproperties
{	
	SupportedEvents.Empty()

	//Must leave this first as a fallback for failed event retrieval
	SupportedEvents(0)={(EventID=-1,EventName="UNKNOWN",StatGroup=(Group=GSG_EngineStats,Level=999),EventDataType=1)}
	//Real events start here
	SupportedEvents.Add((EventID=GAMEEVENT_MATCH_STARTED,EventName="Match Started",StatGroup=(Group=GSG_Game,Level=1),EventDataType=`GET_GameInt))
	SupportedEvents.Add((EventID=GAMEEVENT_MATCH_ENDED,EventName="Match Ended",StatGroup=(Group=GSG_Game,Level=1),EventDataType=`GET_GameInt))
	SupportedEvents.Add((EventID=GAMEEVENT_ROUND_STARTED,EventName="Round Started",StatGroup=(Group=GSG_Game,Level=1),EventDataType=`GET_GameInt))
	SupportedEvents.Add((EventID=GAMEEVENT_ROUND_ENDED,EventName="Round Ended",StatGroup=(Group=GSG_Game,Level=1),EventDataType=`GET_GameInt))
	SupportedEvents.Add((EventID=GAMEEVENT_GAME_CLASS,EventName="Game Class",StatGroup=(Group=GSG_Game,Level=1),EventDataType=`GET_GameString))
	SupportedEvents.Add((EventID=GAMEEVENT_GAME_OPTION_URL,EventName="Game Options",StatGroup=(Group=GSG_Game,Level=1),EventDataType=`GET_GameString))
	SupportedEvents.Add((EventID=GAMEEVENT_GAME_MAPNAME,EventName="Map Name",StatGroup=(Group=GSG_Game,Level=1),EventDataType=`GET_GameString))
	
	SupportedEvents.Add((EventID=GAMEEVENT_TEAM_CREATED,EventName="Team Created",StatGroup=(Group=GSG_Team,Level=1),EventDataType=`GET_TeamInt))
	SupportedEvents.Add((EventID=GAMEEVENT_TEAM_GAME_SCORE,EventName="Team Score",StatGroup=(Group=GSG_Team,Level=1),EventDataType=`GET_TeamInt))
	SupportedEvents.Add((EventID=GAMEEVENT_TEAM_MATCH_WON,EventName="Match Won",StatGroup=(Group=GSG_Team,Level=1),EventDataType=`GET_TeamInt))
	SupportedEvents.Add((EventID=GAMEEVENT_TEAM_ROUND_WON,EventName="Round Won",StatGroup=(Group=GSG_Team,Level=1),EventDataType=`GET_TeamInt))
	SupportedEvents.Add((EventID=GAMEEVENT_TEAM_ROUND_STALEMATE,EventName="Round Stalemate",StatGroup=(Group=GSG_Team,Level=1),EventDataType=`GET_TeamInt))

	SupportedEvents.Add((EventID=GAMEEVENT_PLAYER_LOGIN,EventName="Player Login",StatGroup=(Group=GSG_Player,Level=1),EventDataType=`GET_PlayerLogin))
	SupportedEvents.Add((EventID=GAMEEVENT_PLAYER_LOGOUT,EventName="Player Logout",StatGroup=(Group=GSG_Player,Level=1),EventDataType=`GET_PlayerLogin))
	SupportedEvents.Add((EventID=GAMEEVENT_PLAYER_KILL,EventName="Player Killed",StatGroup=(Group=GSG_Player,Level=1),EventDataType=`GET_PlayerKillDeath))
	SupportedEvents.Add((EventID=GAMEEVENT_PLAYER_DEATH,EventName="Player Death",StatGroup=(Group=GSG_Player,Level=1),EventDataType=`GET_PlayerKillDeath))
	
	SupportedEvents.Add((EventID=GAMEEVENT_PLAYER_TEAMCHANGE,EventName="Player Team Change",StatGroup=(Group=GSG_Player,Level=1),EventDataType=`GET_PlayerInt))
	SupportedEvents.Add((EventID=GAMEEVENT_PLAYER_SPAWN,EventName="Player Spawn",StatGroup=(Group=GSG_Player,Level=1),EventDataType=`GET_PlayerSpawn))
	SupportedEvents.Add((EventID=GAMEEVENT_PLAYER_LOCATION_POLL,EventName="Player Locations",StatGroup=(Group=GSG_Player,Level=10),EventDataType=`GET_PlayerLocationPoll))
	SupportedEvents.Add((EventID=GAMEEVENT_PLAYER_KILL_STREAK,EventName="Kill Streak",StatGroup=(Group=GSG_Player,Level=1),EventDataType=`GET_PlayerInt))
	SupportedEvents.Add((EventID=GAMEEVENT_PLAYER_MATCH_WON,EventName="Player Match Won",StatGroup=(Group=GSG_Player,Level=1),EventDataType=`GET_PlayerInt))
	SupportedEvents.Add((EventID=GAMEEVENT_PLAYER_ROUND_WON,EventName="Player Round Won",StatGroup=(Group=GSG_Player,Level=1),EventDataType=`GET_PlayerInt))
	SupportedEvents.Add((EventID=GAMEEVENT_PLAYER_ROUND_STALEMATE,EventName="Player Round Stalemate",StatGroup=(Group=GSG_Player,Level=1),EventDataType=`GET_PlayerInt))

	SupportedEvents.Add((EventID=GAMEEVENT_WEAPON_DAMAGE,EventName="Weapon Damage",StatGroup=(Group=GSG_Weapon,Level=10),EventDataType=`GET_DamageInt))
	SupportedEvents.Add((EventID=GAMEEVENT_WEAPON_DAMAGE_MELEE,EventName="Melee Damage",StatGroup=(Group=GSG_Weapon,Level=10),EventDataType=`GET_DamageInt))
	SupportedEvents.Add((EventID=GAMEEVENT_WEAPON_FIRED,EventName="Weapon Fired",StatGroup=(Group=GSG_Weapon,Level=10),EventDataType=`GET_WeaponInt))

	SupportedEvents.Add((EventID=GAMEEVENT_PLAYER_KILL_NORMAL,EventName="Normal Kill",StatGroup=(Group=GSG_Weapon,Level=1),EventDataType=`GET_PlayerKillDeath))

	SupportedEvents.Add((EventID=GAMEEVENT_MEMORYUSAGE_POLL,EventName="Memory Usage",StatGroup=(Group=GSG_EngineStats,Level=10),EventDataType=`GET_GameInt))
	SupportedEvents.Add((EventID=GAMEEVENT_NETWORKUSAGEIN_POLL,EventName="Network Usage IN",StatGroup=(Group=GSG_EngineStats,Level=10),EventDataType=`GET_GameInt))
	SupportedEvents.Add((EventID=GAMEEVENT_NETWORKUSAGEOUT_POLL,EventName="Network Usage OUT",StatGroup=(Group=GSG_EngineStats,Level=10),EventDataType=`GET_GameInt))
	SupportedEvents.Add((EventID=GAMEEVENT_PING_POLL,EventName="Ping",StatGroup=(Group=GSG_EngineStats,Level=10),EventDataType=`GET_GameInt))
	SupportedEvents.Add((EventID=GAMEEVENT_FRAMERATE_POLL,EventName="Frame Rate",StatGroup=(Group=GSG_EngineStats,Level=10),EventDataType=`GET_GameInt))
	SupportedEvents.Add((EventID=GAMEEVENT_GAMETHREAD_POLL,EventName="Game thread time",StatGroup=(Group=GSG_EngineStats,Level=10),EventDataType=`GET_GamePosition))
	SupportedEvents.Add((EventID=GAMEEVENT_RENDERTHREAD_POLL,EventName="Render thread time",StatGroup=(Group=GSG_EngineStats,Level=10),EventDataType=`GET_GamePosition))
	SupportedEvents.Add((EventID=GAMEEVENT_GPUFRAMETIME_POLL,EventName="GPU render time",StatGroup=(Group=GSG_EngineStats,Level=10),EventDataType=`GET_GamePosition))
	SupportedEvents.Add((EventID=GAMEEVENT_FRAMETIME_POLL,EventName="Total frame time",StatGroup=(Group=GSG_EngineStats,Level=10),EventDataType=`GET_GamePosition))
	SupportedEvents.Add((EventID=`GAMEEVENT_AI_PATH_FAILURE,EventName="AI Path Failure",StatGroup=(Group=GSG_Game,Level=10),EventDataType=`GET_GenericParamList))
	SupportedEvents.Add((EventID=`GAMEEVENT_AI_FIRELINK,EventName="AI Firelink",StatGroup=(Group=GSG_Game,Level=10),EventDataType=`GET_GenericParamList))
}