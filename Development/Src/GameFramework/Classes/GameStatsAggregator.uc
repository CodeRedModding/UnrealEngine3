/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Aggregates data from a game session stored on disk
 */
class GameStatsAggregator extends GameplayEventsHandler
	native(GameStats);

`include(Engine\Classes\GameStats.uci);

/* Aggregate data starts here */
const GAMEEVENT_AGGREGATED_DATA = 10000;

/** Player aggregates */
const GAMEEVENT_AGGREGATED_PLAYER_TIMEALIVE					= 10001;
const GAMEEVENT_AGGREGATED_PLAYER_KILLS						= 10002;
const GAMEEVENT_AGGREGATED_PLAYER_DEATHS					= 10003;
const GAMEEVENT_AGGREGATED_PLAYER_MATCH_WON					= 10004;
const GAMEEVENT_AGGREGATED_PLAYER_ROUND_WON					= 10005;
const GAMEEVENT_AGGREGATED_DAMAGE_DEALT_NORMALKILL			= 10006;
const GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_WASNORMALKILL	= 10007;

/** Team aggregates */
const GAMEEVENT_AGGREGATED_TEAM_KILLS			= 10100;
const GAMEEVENT_AGGREGATED_TEAM_DEATHS			= 10101;
const GAMEEVENT_AGGREGATED_TEAM_GAME_SCORE		= 10102;
const GAMEEVENT_AGGREGATED_TEAM_MATCH_WON		= 10103;
const GAMEEVENT_AGGREGATED_TEAM_ROUND_WON		= 10104;

/** Damage class aggregates */
const GAMEEVENT_AGGREGATED_DAMAGE_KILLS						= 10200;
const GAMEEVENT_AGGREGATED_DAMAGE_DEATHS					= 10201;
const GAMEEVENT_AGGREGATED_DAMAGE_DEALT_WEAPON_DAMAGE		= 10202;
const GAMEEVENT_AGGREGATED_DAMAGE_DEALT_MELEE_DAMAGE		= 10203;
const GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_WEAPON_DAMAGE	= 10204;
const GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_MELEE_DAMAGE		= 10205;
const GAMEEVENT_AGGREGATED_DAMAGE_DEALT_MELEEHITS			= 10206;
const GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_WASMELEEHIT		= 10207;

/** Weapon class aggregates */
const GAMEEVENT_AGGREGATED_WEAPON_FIRED			= 10300;

/** Pawn class aggregates */
const GAMEEVENT_AGGREGATED_PAWN_SPAWN			= 10400;

/** Game specific starts here */
const GAMEEVENT_AGGREGATED_GAME_SPECIFIC		= 11000;


/** Current game state as the game stream is parsed */
var GameStateObject GameState;

/** Base container for a single stat aggregated over multiple time periods */
struct native GameEvent
{
	var init array<float> EventCountByTimePeriod;
	structcpptext
	{
		FGameEvent()
		{}
		FGameEvent(EEventParm)
		{
			appMemzero(this, sizeof(FGameEvent));
		}
		/** 
		 * Accumulate data for a given time period
		 * @param TimePeriod - time period slot to use (0 - game total, 1+ round total)
		 * @param Value - value to accumulate 
		 */
		void AddEventData(INT TimePeriod, FLOAT Value)
		{
			if (TimePeriod >= 0 && TimePeriod < 100) //sanity check
			{
				if (!EventCountByTimePeriod.IsValidIndex(TimePeriod))
				{
					EventCountByTimePeriod.AddZeroed(TimePeriod - EventCountByTimePeriod.Num() + 1);
				}

				check(EventCountByTimePeriod.IsValidIndex(TimePeriod));
				EventCountByTimePeriod(TimePeriod) += Value;
			}
			else
			{
				debugf(TEXT("AddEventData: Timeperiod %d way out of range."), TimePeriod);
			}
		}

		/*
		 *	Get accumulated data for a given time period
		 * @param TimePeriod - time period slot to get (0 - game total, 1+ round total)
		 */
		FLOAT GetEventData(INT TimePeriod) const
		{
			if (EventCountByTimePeriod.IsValidIndex(TimePeriod))
			{
				return EventCountByTimePeriod(TimePeriod);
			}
			else
			{
				return 0.0f;
			}
		}
	}
};

/** Container for game event stats stored by event ID */
struct native GameEvents
{
	var const private native transient  Map_Mirror  Events{TMap<INT, FGameEvent>};
	structcpptext
	{
		FGameEvents()
		{}

		/* 
		 *   Accumulate an event's data
		 * @param EventID - the event to record
		 * @param Value - the events recorded value
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddEvent(INT EventID, FLOAT Value, INT TimePeriod);

		/** @return Number of events in the list */
		INT Num() const
		{
			return Events.Num();
		}

		/** Clear out the contents */
		void ClearEvents()
		{
			Events.Empty();
		}
	}
};

/** Base class for event storage */
struct native EventsBase
{
	var GameEvents TotalEvents;
	var array<GameEvents> EventsByClass;

	structcpptext
	{
		FEventsBase()
		{}

		/** Clear out the contents */
		void ClearEvents()
		{
			TotalEvents.ClearEvents();
			for (INT i=0; i<EventsByClass.Num(); i++)
			{
				EventsByClass(i).ClearEvents();
			}
			EventsByClass.Empty();
		}
	}
};

/** 
 * Container for all weapon events
 * Stores totals across all weapons plus individually by recorded weapon class metadata
 */
struct native WeaponEvents extends EventsBase
{
	structcpptext
	{
		FWeaponEvents()
		{}

		/* 
		 *   Accumulate a weapon event's data
		 * @param EventID - the event to record
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddWeaponIntEvent(INT EventID, struct FWeaponIntEvent* GameEventData, INT TimePeriod);
	}
};

/** 
 * Container for all projectile events
 * Stores totals across all projectiles plus individually by recorded projectile class metadata
 */
struct native ProjectileEvents extends EventsBase
{
	structcpptext
	{
		FProjectileEvents()
		{}

		/* 
		 *   Accumulate a projectile event's data
		 * @param EventID - the event to record
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddProjectileIntEvent(INT EventID, struct FProjectileIntEvent* GameEventData, INT TimePeriod);
	}
};

/** 
 * Container for all damage events
 * Stores totals across all damage plus individually by recorded damage class metadata
 */
struct native DamageEvents extends EventsBase
{
	structcpptext
	{
		FDamageEvents()
		{}

		/* 
		 *   Accumulate a kill event for a given damage type
		 * @param EventID - the event to record
		 * @param KillTypeID - the ID of the kill type recorded
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddKillEvent(INT EventID, INT KillTypeID, struct FPlayerKillDeathEvent* GameEventData, INT TimePeriod);
		/* 
		 *   Accumulate a death event for a given damage type
		 * @param EventID - the event to record
		 * @param KillTypeID - the ID of the kill type recorded
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddDeathEvent(INT EventID, INT KillTypeID, struct FPlayerKillDeathEvent* GameEventData, INT TimePeriod);
		/* 
		 *   Accumulate a damage event for a given damage type
		 * @param EventID - the event to record
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddDamageIntEvent(INT EventID, struct FDamageIntEvent* GameEventData, INT TimePeriod);
	}
};

/** 
 * Container for all pawn events
 * Stores totals across all pawn plus individually by recorded pawn class metadata
 */
struct native PawnEvents extends EventsBase
{		
	structcpptext
	{
		FPawnEvents()
		{}

		/* 
		 *   Accumulate a pawn event for a given pawn type
		 * @param EventID - the event to record
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddPlayerSpawnEvent(INT EventID, struct FPlayerSpawnEvent* GameEventData, INT TimePeriod);
	}
};

/** 
 * Container for all team events
 * Stores totals across a single team plus all sub container types
 */
struct native TeamEvents
{
	var GameEvents TotalEvents;
	var WeaponEvents WeaponEvents;
	var DamageEvents DamageAsPlayerEvents;
	var DamageEvents DamageAsTargetEvents;
	var ProjectileEvents ProjectileEvents;
	var PawnEvents PawnEvents;

	structcpptext
	{
		FTeamEvents()
		{}

		/** 
		 * Accumulate data for a generic event
		 * @param EventID - the event to record
		 * @param TimePeriod - time period slot to use (0 - game total, 1+ round total)
		 * @param Value - value to accumulate 
		 */
		void AddEvent(INT EventID, FLOAT Value, INT TimePeriod);
		/* 
		 *   Accumulate a kill event for a given damage type
		 * @param EventID - the event to record
		 * @param KillTypeID - the ID of the kill type recorded
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddKillEvent(INT EventID, INT KillTypeID, struct FPlayerKillDeathEvent* GameEventData, INT TimePeriod);
		/* 
		 *   Accumulate a death event for a given damage type
		 * @param EventID - the event to record
		 * @param KillTypeID - the ID of the kill type recorded
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddDeathEvent(INT EventID, INT KillTypeID, struct FPlayerKillDeathEvent* GameEventData, INT TimePeriod);
		/* 
		 *   Accumulate a weapon event's data
		 * @param EventID - the event to record
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddWeaponIntEvent(INT EventID, struct FWeaponIntEvent* GameEventData, INT TimePeriod);
		/* 
		 *   Accumulate a damage event for a given damage type where the team member was the attacker
		 * @param EventID - the event to record
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddDamageDoneIntEvent(INT EventID, struct FDamageIntEvent* GameEventData, INT TimePeriod);
		/* 
		 *   Accumulate a damage event for a given damage type where the team member was the target
		 * @param EventID - the event to record
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddDamageTakenIntEvent(INT EventID, struct FDamageIntEvent* GameEventData, INT TimePeriod);
		/* 
		 *   Accumulate a pawn event for a given pawn type
		 * @param EventID - the event to record
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddPlayerSpawnEvent(INT EventID, struct FPlayerSpawnEvent* GameEventData, INT TimePeriod);
		/* 
		 *   Accumulate a projectile event's data
		 * @param EventID - the event to record
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddProjectileIntEvent(INT EventID, struct FProjectileIntEvent* GameEventData, INT TimePeriod);

		/** Clear out the contents */
		void ClearEvents()
		{
			TotalEvents.ClearEvents();
			WeaponEvents.ClearEvents();
			DamageAsPlayerEvents.ClearEvents();
			DamageAsTargetEvents.ClearEvents();
			ProjectileEvents.ClearEvents();
			PawnEvents.ClearEvents();
		}
	}
};

/** 
 * Container for all player events
 * Stores totals across a single player plus all sub container types
 */
struct native PlayerEvents
{
	var GameEvents TotalEvents;
	var WeaponEvents WeaponEvents;
	var DamageEvents DamageAsPlayerEvents;
	var DamageEvents DamageAsTargetEvents;
	var ProjectileEvents ProjectileEvents;
	var PawnEvents PawnEvents;

	structcpptext
	{
		FPlayerEvents()
		{}

		/** 
		 * Accumulate data for a generic event
		 * @param EventID - the event to record
		 * @param TimePeriod - time period slot to use (0 - game total, 1+ round total)
		 * @param Value - value to accumulate 
		 */
		void AddEvent(INT EventID, FLOAT Value, INT TimePeriod);
		/* 
		 *   Accumulate a kill event for a given damage type
		 * @param EventID - the event to record
		 * @param KillTypeID - the ID of the kill type recorded
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddKillEvent(INT EventID, INT KillTypeID, struct FPlayerKillDeathEvent* GameEventData, INT TimePeriod);
		/* 
		 *   Accumulate a death event for a given damage type
		 * @param EventID - the event to record
		 * @param KillTypeID - the ID of the kill type recorded
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddDeathEvent(INT EventID, INT KillTypeID, struct FPlayerKillDeathEvent* GameEventData, INT TimePeriod);
		/* 
		 *   Accumulate a weapon event's data
		 * @param EventID - the event to record
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddWeaponIntEvent(INT EventID, struct FWeaponIntEvent* GameEventData, INT TimePeriod);
		/* 
		 *   Accumulate a damage event for a given damage type where the player was the attacker
		 * @param EventID - the event to record
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddDamageDoneIntEvent(INT EventID, struct FDamageIntEvent* GameEventData, INT TimePeriod);
		/* 
		 *   Accumulate a damage event for a given damage type where the player was the target
		 * @param EventID - the event to record
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddDamageTakenIntEvent(INT EventID, struct FDamageIntEvent* GameEventData, INT TimePeriod);
		/* 
		 *   Accumulate a pawn event for a given pawn type
		 * @param EventID - the event to record
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddPlayerSpawnEvent(INT EventID, struct FPlayerSpawnEvent* GameEventData, INT TimePeriod);
		/* 
		 *   Accumulate a projectile event's data
		 * @param EventID - the event to record
		 * @param GameEventData - the event data
		 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
		 */
		void AddProjectileIntEvent(INT EventID, struct FProjectileIntEvent* GameEventData, INT TimePeriod);

		/** Clear out the contents */
		void ClearEvents()
		{
			TotalEvents.ClearEvents();
			WeaponEvents.ClearEvents();
			DamageAsPlayerEvents.ClearEvents();
			DamageAsTargetEvents.ClearEvents();
			ProjectileEvents.ClearEvents();
			PawnEvents.ClearEvents();
		}
	}
};

struct native AggregateEventMapping
{
	/** Recorded event ID */
	var int EventID;
	/** Mapping to the main aggregate event */
	var int AggregateID;
	/** Mapping to the aggregate event for the target if applicable*/
	var int TargetAggregateID;
};

/** Array of all aggregates that require mappings when making an aggregate run */
var array<AggregateEventMapping> AggregatesList;

/** Mapping of event ID to its aggregate equivalents (created at runtime) */
var	const private native transient	Map_Mirror	AggregateEventsMapping{TMap<INT, struct FAggregateEventMapping>};

/** The set of aggregate events that the aggregation supports */
var array<GameplayEventMetaData> AggregateEvents;

/** All aggregates generated this match */
var const array<int> AggregatesFound;
				
/** Aggregates of all recorded events */
var GameEvents AllGameEvents;
/** Aggregates of all recorded team events */
var const array<TeamEvents> AllTeamEvents;
/** Aggregates of all recorded player events */
var const array<PlayerEvents> AllPlayerEvents;
/** Aggregates of all recorded weapon events */
var const WeaponEvents AllWeaponEvents;
/** Aggregates of all recorded projectile events */
var const ProjectileEvents AllProjectileEvents;
/** Aggregates of all recorded pawn events */
var const PawnEvents AllPawnEvents;
/** Aggregates of all recorded damage events */
var const DamageEvents AllDamageEvents;

cpptext
{
	/*
	 *   Set the game state this aggregator will use
	 * @param InGameState - game state object to use
	 */
	virtual void SetGameState(class UGameStateObject* InGameState);

	/*
	 *   GameStatsFileReader Interface (handles parsing of the data stream)
	 */
	
	// Game Event Handling
	virtual void HandleGameStringEvent(struct FGameEventHeader& GameEvent, struct FGameStringEvent* GameEventData);
	virtual void HandleGameIntEvent(struct FGameEventHeader& GameEvent, struct FGameIntEvent* GameEventData);
	virtual void HandleGameFloatEvent(struct FGameEventHeader& GameEvent, struct FGameFloatEvent* GameEventData);
	virtual void HandleGamePositionEvent(struct FGameEventHeader& GameEvent, struct FGamePositionEvent* GameEventData);

	// Team Event Handling
	virtual void HandleTeamStringEvent(struct FGameEventHeader& GameEvent, struct FTeamStringEvent* GameEventData);
	virtual void HandleTeamIntEvent(struct FGameEventHeader& GameEvent, struct FTeamIntEvent* GameEventData);
	virtual void HandleTeamFloatEvent(struct FGameEventHeader& GameEvent, struct FTeamFloatEvent* GameEventData);

	// Player Event Handling
	virtual void HandlePlayerIntEvent(struct FGameEventHeader& GameEvent, struct FPlayerIntEvent* GameEventData);
	virtual void HandlePlayerFloatEvent(struct FGameEventHeader& GameEvent, struct FPlayerFloatEvent* GameEventData);
	virtual void HandlePlayerStringEvent(struct FGameEventHeader& GameEvent, struct FPlayerStringEvent* GameEventData);
	virtual void HandlePlayerSpawnEvent(struct FGameEventHeader& GameEvent, struct FPlayerSpawnEvent* GameEventData);
	virtual void HandlePlayerLoginEvent(struct FGameEventHeader& GameEvent, struct FPlayerLoginEvent* GameEventData);
	virtual void HandlePlayerKillDeathEvent(struct FGameEventHeader& GameEvent, struct FPlayerKillDeathEvent* GameEventData);
	virtual void HandlePlayerPlayerEvent(struct FGameEventHeader& GameEvent, struct FPlayerPlayerEvent* GameEventData);
	virtual void HandlePlayerLocationsEvent(struct FGameEventHeader& GameEvent, struct FPlayerLocationsEvent* GameEventData);
	
	virtual void HandleWeaponIntEvent(struct FGameEventHeader& GameEvent, struct FWeaponIntEvent* GameEventData);
	virtual void HandleDamageIntEvent(struct FGameEventHeader& GameEvent, struct FDamageIntEvent* GameEventData);
	virtual void HandleProjectileIntEvent(struct FGameEventHeader& GameEvent, struct FProjectileIntEvent* GameEventData);

	/** 
	 * Cleanup for a given player at the end of a round
	 * @param PlayerIndex - player to cleanup/record stats for
	 */
	virtual void AddPlayerEndOfRoundStats(INT PlayerIndex);
	/** Triggered by the end of round event, adds any additional aggregate stats required */
	virtual void AddEndOfRoundStats();
	/** Triggered by the end of match event, adds any additional aggregate stats required */
	virtual void AddEndOfMatchStats();

	/** Returns the metadata associated with the given index, overloaded to access aggregate events not found in the stream directly */
	virtual const FGameplayEventMetaData& GetEventMetaData(INT EventID) const;

	/** 
	 * Get the team event container for the given team
	 * @param TeamIndex - team of interest (-1/255 are considered same team)
	 */
	FTeamEvents& GetTeamEvents(INT TeamIndex) { if (TeamIndex >=0 && TeamIndex < 255) { return AllTeamEvents(TeamIndex); } else { return AllTeamEvents(AllTeamEvents.Num() - 1); } }
	/** 
	 * Get the player event container for the given player
	 * @param PlayerIndex - player of interest (-1 is valid and returns container for "invalid player")
	 */
	FPlayerEvents& GetPlayerEvents(INT PlayerIndex) { if (PlayerIndex >=0) { return AllPlayerEvents(PlayerIndex); } else { return AllPlayerEvents(AllPlayerEvents.Num() - 1); } }

	/*
	 * Call Reset on destroy to make sure all native structs are cleaned up
	 */
	virtual void BeginDestroy()
	{
		Reset();
		Super::BeginDestroy();		
	}
};

/** A chance to do something before the stream starts */
native event PreProcessStream();

/** A chance to do something after the stream ends */
native event PostProcessStream();

/** Cleanup/reset all data related to this aggregation */
native function Reset();

/*
 *   Get the mapping from an event ID to its equivalent aggregate IDs
 * @param EventID - EventID to map
 * @param AggregateID - Aggregate ID (main/player ID)
 * @param TargetAggregateID - AggregateID that applies to the target (if applicable)
 * @return TRUE if mapping found, FALSE otherwise
 */
native function bool GetAggregateMappingIDs(int EventID, out int AggregateID, out int TargetAggregateID);

defaultproperties
{
	// Additional aggregate events added to the output as the game stats stream is parsed
 	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_PLAYER_TIMEALIVE,EventName="Player Time Alive",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_PlayerAggregate))
 	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_PLAYER_KILLS,EventName="Kills",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_PlayerAggregate))
 	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_PLAYER_DEATHS,EventName="Deaths",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_PlayerAggregate))
	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_DAMAGE_DEALT_NORMALKILL,EventName="Normal Kill",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_PlayerAggregate))
	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_WASNORMALKILL,EventName="Was Normal Kill",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_PlayerAggregate))
	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_PLAYER_MATCH_WON,EventName="Match Won",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_PlayerAggregate))	
	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_PLAYER_ROUND_WON,EventName="Round Won",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_PlayerAggregate))	
	
	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_TEAM_KILLS,EventName="Kills",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_TeamAggregate))
 	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_TEAM_DEATHS,EventName="Deaths",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_TeamAggregate))
	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_TEAM_GAME_SCORE,EventName="Team Score",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_TeamAggregate))
	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_TEAM_MATCH_WON,EventName="Matches Won",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_TeamAggregate))
	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_TEAM_ROUND_WON,EventName="Rounds Won",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_TeamAggregate))

 	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_DAMAGE_KILLS,EventName="Kills",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_DamageAggregate))
 	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_DAMAGE_DEATHS,EventName="Deaths",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_DamageAggregate))
	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_DAMAGE_DEALT_WEAPON_DAMAGE,EventName="Weapon Damage Dealt",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_DamageAggregate))
	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_DAMAGE_DEALT_MELEE_DAMAGE,EventName="Melee Damage Dealt",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_DamageAggregate))
	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_WEAPON_DAMAGE,EventName="Weapon Damage Received",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_DamageAggregate))
	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_MELEE_DAMAGE,EventName="Melee Damage Received",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_DamageAggregate))
	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_DAMAGE_DEALT_MELEEHITS,EventName="Melee Hits",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_DamageAggregate))
	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_WASMELEEHIT,EventName="Was Melee Hit",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_DamageAggregate))

	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_WEAPON_FIRED,EventName="Weapon Fired",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_WeaponAggregate))

	AggregateEvents.Add((EventID=GAMEEVENT_AGGREGATED_PAWN_SPAWN,EventName="Spawns",StatGroup=(Group=GSG_Aggregate,Level=1),EventDataType=`GET_PawnAggregate))

	// Mapping from stream stat ID to aggregate stat ID (kill/death handle special)
	AggregatesList.Add((EventID=GAMEEVENT_PLAYER_MATCH_WON,AggregateID=GAMEEVENT_AGGREGATED_PLAYER_MATCH_WON))					
	AggregatesList.Add((EventID=GAMEEVENT_PLAYER_ROUND_WON,AggregateID=GAMEEVENT_AGGREGATED_PLAYER_ROUND_WON))

	AggregatesList.Add((EventID=GAMEEVENT_TEAM_GAME_SCORE,AggregateID=GAMEEVENT_AGGREGATED_TEAM_GAME_SCORE))
	AggregatesList.Add((EventID=GAMEEVENT_TEAM_MATCH_WON,AggregateID=GAMEEVENT_AGGREGATED_TEAM_MATCH_WON))
	AggregatesList.Add((EventID=GAMEEVENT_TEAM_ROUND_WON,AggregateID=GAMEEVENT_AGGREGATED_TEAM_ROUND_WON))

	AggregatesList.Add((EventID=GAMEEVENT_WEAPON_DAMAGE,AggregateID=GAMEEVENT_AGGREGATED_DAMAGE_DEALT_WEAPON_DAMAGE,TargetAggregateID=GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_WEAPON_DAMAGE))
	AggregatesList.Add((EventID=GAMEEVENT_WEAPON_DAMAGE_MELEE,AggregateID=GAMEEVENT_AGGREGATED_DAMAGE_DEALT_MELEE_DAMAGE,TargetAggregateID=GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_MELEE_DAMAGE))	

	AggregatesList.Add((EventID=GAMEEVENT_WEAPON_FIRED,AggregateID=GAMEEVENT_AGGREGATED_WEAPON_FIRED))

	AggregatesList.Add((EventID=GAMEEVENT_PLAYER_SPAWN,AggregateID=GAMEEVENT_AGGREGATED_PAWN_SPAWN))	

	AggregatesList.Add((EventID=GAMEEVENT_PLAYER_KILL_NORMAL,AggregateID=GAMEEVENT_AGGREGATED_DAMAGE_DEALT_NORMALKILL,TargetAggregateID=GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_WASNORMALKILL))
}

