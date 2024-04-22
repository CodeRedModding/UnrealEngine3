/*=============================================================================
	GameStats.cpp: 
	Classes and interfaces for interpreting game stats files
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "GameFramework.h"
#include "GameStatsUtilities.h"
#include "GameplayEventsUtilities.h"
#include "UnIpDrv.h"


//Warn helper
#ifndef warnexprf
#define warnexprf(expr, msg) { if(!(expr)) {warnf(msg);} }
#endif

#define ALL_AGGREGATES !CONSOLE && !DEDICATED_SERVER

/************************************************************************/
/* UGameplayEventsHandler                                               */
/************************************************************************/

/** A chance to do something before the stream starts */
void UGameplayEventsHandler::PreProcessStream()
{
	// Setup specified filters 
	eventResolveGroupFilters();
}

/** 
 * The function that does the actual handling of data
 * Makes sure that the game state object gets a chance to handle data before the aggregator does
 * @param GameEvent - header of the current game event from disk
 * @param GameEventData - payload immediately following the header
 */
void UGameplayEventsHandler::HandleEvent(struct FGameEventHeader& GameEvent, class IGameEvent* GameEventData)
{
	if (IsEventFiltered(GameEvent.EventID))
	{
		return;
	}

	// Process the event
	switch(GameEvent.EventType)
	{
	case GET_GameString:
		{		
			HandleGameStringEvent(GameEvent, (FGameStringEvent*)GameEventData);
			break;
		}
	case GET_GameInt:
		{
			HandleGameIntEvent(GameEvent, (FGameIntEvent*)GameEventData);
			break;
		}
	case GET_GameFloat:
		{
			HandleGameFloatEvent(GameEvent, (FGameFloatEvent*)GameEventData);
			break;
		}
	case GET_GamePosition:
		{
			HandleGamePositionEvent(GameEvent, (FGamePositionEvent*)GameEventData);
			break;
		}
	case GET_TeamString:
		{
			HandleTeamStringEvent(GameEvent, (FTeamStringEvent*)GameEventData);
			break;
		}
	case GET_TeamInt:
		{
			HandleTeamIntEvent(GameEvent, (FTeamIntEvent*)GameEventData);
			break;
		}
	case GET_TeamFloat:
		{
			HandleTeamFloatEvent(GameEvent, (FTeamFloatEvent*)GameEventData);
			break;
		}
	case GET_PlayerString:
		{
			HandlePlayerStringEvent(GameEvent, (FPlayerStringEvent*)GameEventData);
			break;
		}
	case GET_PlayerInt:
		{
			HandlePlayerIntEvent(GameEvent, (FPlayerIntEvent*)GameEventData);
			break;
		}
	case GET_PlayerFloat:
		{
			HandlePlayerFloatEvent(GameEvent, (FPlayerFloatEvent*)GameEventData);
			break;
		}
	case GET_PlayerSpawn:
		{
			HandlePlayerSpawnEvent(GameEvent, (FPlayerSpawnEvent*)GameEventData);
			break;
		}
	case GET_PlayerLogin:
		{
			HandlePlayerLoginEvent(GameEvent, (FPlayerLoginEvent*)GameEventData);
			break;
		}
	case GET_PlayerLocationPoll:
		{
			HandlePlayerLocationsEvent(GameEvent, (FPlayerLocationsEvent*)GameEventData);
			return;
		}
	case GET_PlayerKillDeath:
		{
			HandlePlayerKillDeathEvent(GameEvent, (FPlayerKillDeathEvent*)GameEventData);
			break;				   
		}
	case GET_PlayerPlayer:
		{										  
			HandlePlayerPlayerEvent(GameEvent, (FPlayerPlayerEvent*)GameEventData);
			break;
		}
	case GET_WeaponInt:
		{
			HandleWeaponIntEvent(GameEvent, (FWeaponIntEvent*)GameEventData);
			break;
		}
	case GET_DamageInt:
		{	
			HandleDamageIntEvent(GameEvent, (FDamageIntEvent*)GameEventData);
			break;
		}
	case GET_ProjectileInt:
		{
			HandleProjectileIntEvent(GameEvent, (FProjectileIntEvent*)GameEventData);
			break;
		}
	case GET_GenericParamList:
		break;
	default:
		debugf(NAME_GameStats,TEXT("Unknown game stat type %d"), GameEvent.EventType);
		return;
	}
}

/************************************************************************/
/* UGameStateObject - state of the game recorded in the stream          */
/************************************************************************/
IMPLEMENT_CLASS(UGameStateObject);

/** Signal start of stream processing */
void UGameStateObject::PreProcessStream()
{
	Reset();
	Super::PreProcessStream();
}

/** Completely reset the game state object */
void UGameStateObject::Reset()
{
	for (INT i=0; i<TeamStates.Num(); i++)
	{
		FTeamState* TeamState = TeamStates(i);
		delete TeamState;
	}
	TeamStates.Empty();

	for (INT i=0; i<PlayerStates.Num(); i++)
	{
		FPlayerState* PlayerState = PlayerStates(i);
		delete PlayerState;
	}
	PlayerStates.Empty();

	SessionType = GT_SessionInvalid;
	bIsMatchStarted = FALSE;
	bIsRoundStarted = FALSE;
	RoundNumber = 0;
}

/*
 *   Cleanup a player state (round over/logout)
 */
void UGameStateObject::CleanupPlayerState(INT PlayerIndex, FLOAT TimeStamp)
{
	FPlayerState* PlayerState = GetPlayerState(PlayerIndex);
	if (PlayerState)
	{
		warnexprf(PlayerState->TimeSpawned > 0, *FString::Printf(TEXT("Player %d: Last spawn time not found"), PlayerIndex));
		PlayerState->TimeAliveSinceLastDeath = PlayerState->TimeSpawned > 0 ? TimeStamp - PlayerState->TimeSpawned : 0;
		PlayerState->TimeSpawned = 0;
	}
}

/*
 * Called when end of round event is parsed, allows for any current
 * state values to be closed out (time alive, etc) 
 * @param TimeStamp - time of the round end event
 */
void UGameStateObject::CleanupRoundState(FLOAT TimeStamp)
{
// 	for (INT TeamStateIdx=0; TeamStateIdx < TeamStates.Num(); TeamStateIdx++)
// 	{
// 		FTeamState* TeamState = TeamStates(TeamStateIdx);
// 	}

	for (INT PlayerStateIdx=0; PlayerStateIdx < PlayerStates.Num(); PlayerStateIdx++)
	{
		FPlayerState* PlayerState = PlayerStates(PlayerStateIdx);
		CleanupPlayerState(PlayerState->PlayerIndex, TimeStamp);
	}
}

/*
 * Called when end of match event is parsed, allows for any current
 * state values to be closed out (round events, etc) 
 * @param TimeStamp - time of the match end event
 */
void UGameStateObject::CleanupMatchState(FLOAT TimeStamp)
{

}

/*
 *   Handles all game string events in the stream
 * @param GameEvent - the game event header in the stream
 * @param GameEventData - the game event payload 
 */
void UGameStateObject::HandleGameStringEvent(struct FGameEventHeader& GameEvent, struct FGameStringEvent* GameEventData)
{

}

/*
 *   Handles all game int events in the stream
 * @param GameEvent - the game event header in the stream
 * @param GameEventData - the game event payload 
 */
void UGameStateObject::HandleGameIntEvent(struct FGameEventHeader& GameEvent, struct FGameIntEvent* GameEventData)
{
	switch (GameEvent.EventID)
	{
	case UCONST_GAMEEVENT_MATCH_STARTED:
		warnexprf(!bIsMatchStarted, TEXT("Match started twice."));
		bIsMatchStarted = TRUE;
		break;
	case UCONST_GAMEEVENT_MATCH_ENDED:
		warnexprf(bIsMatchStarted, TEXT("Match ended but not started."));
		CleanupMatchState(GameEvent.TimeStamp);
		bIsMatchStarted = FALSE;
		break;
	case UCONST_GAMEEVENT_ROUND_STARTED:
		warnexprf(bIsMatchStarted, TEXT("Round start outside match start."));
		bIsMatchStarted = TRUE;
		warnexprf(!bIsRoundStarted, TEXT("Round started twice."));
		bIsRoundStarted = TRUE;
		RoundNumber = GameEventData->Value;
		MaxRoundNumber = Max(MaxRoundNumber, RoundNumber);
		//debugf(TEXT(" (UGameStateObject): Round Started %d -----------------"), RoundNumber);
		break;
	case UCONST_GAMEEVENT_ROUND_ENDED:
		warnexprf(bIsRoundStarted, TEXT("Round ended but not started."));
		CleanupRoundState(GameEvent.TimeStamp);
		bIsRoundStarted = FALSE;
		RoundNumber = GameEventData->Value;
		MaxRoundNumber = Max(MaxRoundNumber, RoundNumber);
		//debugf(TEXT(" (UGameStateObject): Round Ended %d --------------------"), RoundNumber);
		break;
	default:
		break;
	}
}

void UGameStateObject::HandleGameFloatEvent(struct FGameEventHeader& GameEvent, struct FGameFloatEvent* GameEventData)
{

}

void UGameStateObject::HandleGamePositionEvent(struct FGameEventHeader& GameEvent, struct FGamePositionEvent* GameEventData)
{

}

void UGameStateObject::HandleTeamStringEvent(struct FGameEventHeader& GameEvent, struct FTeamStringEvent* GameEventData)
{

}

/*
 *   Handles all team int events in the stream
 * @param GameEvent - the game event header in the stream
 * @param GameEventData - the game event payload 
 */
void UGameStateObject::HandleTeamIntEvent(struct FGameEventHeader& GameEvent, struct FTeamIntEvent* GameEventData)
{
	switch(GameEvent.EventID)
	{
	case UCONST_GAMEEVENT_TEAM_MATCH_WON:  
		//debugf(TEXT("Match %s by team %d."), GameEventData->Value == 1 ? TEXT("won") : TEXT("lost"), GameEventData->TeamIndex);
		break;
	case UCONST_GAMEEVENT_TEAM_ROUND_WON:
		//debugf(TEXT("Round %s by team %d."), GameEventData->Value == 1 ? TEXT("won") : TEXT("lost"), GameEventData->TeamIndex);
		break;
	case UCONST_GAMEEVENT_TEAM_CREATED:
		//debugf(TEXT("Team %d created."), GameEventData->TeamIndex);
		break;	
	}
}

void UGameStateObject::HandleTeamFloatEvent(struct FGameEventHeader& GameEvent, struct FTeamFloatEvent* GameEventData)
{

}

/*
 *   Handles all player int events in the stream
 * @param GameEvent - the game event header in the stream
 * @param GameEventData - the game event payload 
 */
void UGameStateObject::HandlePlayerIntEvent(struct FGameEventHeader& GameEvent, struct FPlayerIntEvent* GameEventData)
{
	INT PlayerIndex = INDEX_NONE;
	FRotator UnusedRotation;

	switch(GameEvent.EventID)
	{
	case UCONST_GAMEEVENT_PLAYER_MATCH_WON:
		//ConvertToPlayerIndexAndRotation(GameEventData->PlayerIndexAndYaw, GameEventData->PlayerPitchAndRoll, PlayerIndex, UnusedRotation);
		//debugf(TEXT("Match %s by player %d."), GameEventData->Value == 1 ? TEXT("won") : TEXT("lost"), PlayerIndex);
		break;
	case UCONST_GAMEEVENT_PLAYER_ROUND_WON:
		//ConvertToPlayerIndexAndRotation(GameEventData->PlayerIndexAndYaw, GameEventData->PlayerPitchAndRoll, PlayerIndex, UnusedRotation);
		//debugf(TEXT("Round %s by player %d."), GameEventData->Value == 1 ? TEXT("won") : TEXT("lost"), PlayerIndex);
		break;
	case UCONST_GAMEEVENT_PLAYER_TEAMCHANGE:
		ConvertToPlayerIndexAndRotation(GameEventData->PlayerIndexAndYaw, GameEventData->PlayerPitchAndRoll, PlayerIndex, UnusedRotation);
		if (PlayerIndex >= 0)
		{
			FPlayerState* PlayerState = GetPlayerState(PlayerIndex);
			FTeamState* OldTeamState = GetTeamState(PlayerState->CurrentTeamIndex);
			OldTeamState->PlayerIndices.RemoveItem(PlayerIndex);
			FTeamState* NewTeamState = GetTeamState(GameEventData->Value);
			NewTeamState->PlayerIndices.AddUniqueItem(PlayerIndex);
			PlayerState->CurrentTeamIndex = NewTeamState->TeamIndex;
			//debugf(TEXT(" DEBUGJM(UGameStateObject): HandlePlayerIntEvent player %d team change from %d to %d"), PlayerIndex, OldTeamState->TeamIndex, NewTeamState->TeamIndex);
		}
		break;
	}
}

/*
 *   Handles all player float events in the stream
 * @param GameEvent - the game event header in the stream
 * @param GameEventData - the game event payload 
 */
void UGameStateObject::HandlePlayerFloatEvent(struct FGameEventHeader& GameEvent, struct FPlayerFloatEvent* GameEventData)
{
}

/*
 *   Handles all player string events in the stream
 * @param GameEvent - the game event header in the stream
 * @param GameEventData - the game event payload 
 */
void UGameStateObject::HandlePlayerStringEvent(struct FGameEventHeader& GameEvent, struct FPlayerStringEvent* GameEventData)
{
}

/*
 *   Handles all player spawn events in the stream
 * @param GameEvent - the game event header in the stream
 * @param GameEventData - the game event payload 
 */
void UGameStateObject::HandlePlayerSpawnEvent(struct FGameEventHeader& GameEvent, struct FPlayerSpawnEvent* GameEventData)
{
	INT PlayerIndex;
	FRotator UnusedRotation;

	ConvertToPlayerIndexAndRotation(GameEventData->PlayerIndexAndYaw, GameEventData->PlayerPitchAndRoll, PlayerIndex, UnusedRotation);
	FPlayerState* PlayerState = GetPlayerState(PlayerIndex);

	warnexprf(PlayerState->TimeSpawned <= 0, *FString::Printf(TEXT("Player %d: Last time spawned not closed "), PlayerIndex));
	PlayerState->TimeSpawned = GameEvent.TimeStamp;

	if (PlayerState->CurrentTeamIndex != GameEventData->TeamIndex)
	{
		FTeamState* OldTeamState = GetTeamState(PlayerState->CurrentTeamIndex);
		OldTeamState->PlayerIndices.RemoveItem(PlayerIndex);
		FTeamState* NewTeamState = GetTeamState(GameEventData->TeamIndex);
		NewTeamState->PlayerIndices.AddUniqueItem(PlayerIndex);
		PlayerState->CurrentTeamIndex = NewTeamState->TeamIndex;
		debugf(NAME_GameStats, TEXT("(UGameStateObject): HandlePlayerSpawnEvent player %d team change from %d to %d"), PlayerIndex, OldTeamState->TeamIndex, NewTeamState->TeamIndex);
	}
}

/*
 *   Handles all player login events in the stream
 * @param GameEvent - the game event header in the stream
 * @param GameEventData - the game event payload 
 */
void UGameStateObject::HandlePlayerLoginEvent(struct FGameEventHeader& GameEvent, struct FPlayerLoginEvent* GameEventData)
{
	INT PlayerIndex;
	FRotator UnusedRotation;

	ConvertToPlayerIndexAndRotation(GameEventData->PlayerIndexAndYaw, GameEventData->PlayerPitchAndRoll, PlayerIndex, UnusedRotation);

	switch(GameEvent.EventID)
	{
	case UCONST_GAMEEVENT_PLAYER_LOGOUT:
		// Logouts mid round need cleanup
		if (PlayerIndex != INDEX_NONE && (SessionType != GT_Multiplayer || bIsRoundStarted))
		{
			CleanupPlayerState(PlayerIndex, GameEvent.TimeStamp);
		}
		break;
	}
}

/*
 *   Handles all player kill or death events in the stream
 * @param GameEvent - the game event header in the stream
 * @param GameEventData - the game event payload 
 */
void UGameStateObject::HandlePlayerKillDeathEvent(struct FGameEventHeader& GameEvent, struct FPlayerKillDeathEvent* GameEventData)
{
	// These stats aren't valid outside the match/round (cleanup should handle closure with an appropriate timestamp)
	if (SessionType != GT_Multiplayer || bIsRoundStarted)
	{
		INT PlayerIndex;
		FRotator UnusedRotation;

		ConvertToPlayerIndexAndRotation(GameEventData->PlayerIndexAndYaw, GameEventData->PlayerPitchAndRoll, PlayerIndex, UnusedRotation);
		FPlayerState* PlayerState = GetPlayerState(PlayerIndex);

		switch(GameEvent.EventID)
		{
			// Killer - Target, Dead - Player
		case UCONST_GAMEEVENT_PLAYER_DEATH:
			if (PlayerIndex != INDEX_NONE)
			{
				warnexprf(PlayerState->TimeSpawned > 0, *FString::Printf(TEXT("Player %d: Last spawn time not found"), PlayerIndex));
				PlayerState->TimeAliveSinceLastDeath = PlayerState->TimeSpawned > 0 ? GameEvent.TimeStamp - PlayerState->TimeSpawned : 0;
				PlayerState->TimeSpawned = 0;
			}
			break;
		}
	}
	else
	{
		debugf(TEXT("Event %d recorded outside of round, skipping."), GameEvent.EventID);
	}
}

/*
 *   Handles all player interaction events in the stream
 * @param GameEvent - the game event header in the stream
 * @param GameEventData - the game event payload 
 */
void UGameStateObject::HandlePlayerPlayerEvent(struct FGameEventHeader& GameEvent, struct FPlayerPlayerEvent* GameEventData)
{

}

/*
 *   Handles all player location events in the stream
 * @param GameEvent - the game event header in the stream
 * @param GameEventData - the game event payload 
 */
void UGameStateObject::HandlePlayerLocationsEvent(struct FGameEventHeader& GameEvent, struct FPlayerLocationsEvent* GameEventData)
{

}

/*
 *   Handles all weapon int events in the stream
 * @param GameEvent - the game event header in the stream
 * @param GameEventData - the game event payload 
 */
void UGameStateObject::HandleWeaponIntEvent(struct FGameEventHeader& GameEvent, struct FWeaponIntEvent* GameEventData)
{

}

/*
 *   Handles all damage events in the stream
 * @param GameEvent - the game event header in the stream
 * @param GameEventData - the game event payload 
 */
void UGameStateObject::HandleDamageIntEvent(struct FGameEventHeader& GameEvent, struct FDamageIntEvent* GameEventData)
{

}

/*
 *   Handles all projectile events in the stream
 * @param GameEvent - the game event header in the stream
 * @param GameEventData - the game event payload 
 */
void UGameStateObject::HandleProjectileIntEvent(struct FGameEventHeader& GameEvent, struct FProjectileIntEvent* GameEventData)
{

}

/*****************************************************************************/
/* UGameStatsAggregator - parses game stats stream and aggregates the events */
/*****************************************************************************/
IMPLEMENT_CLASS(UGameStatsAggregator);

/* 
 *   Accumulate an event's data
 * @param EventID - the event to record
 * @param Value - the events recorded value
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FGameEvents::AddEvent(INT EventID, FLOAT Value, INT TimePeriod)
{
	if (EventID > 0)
	{
		// See if the event exists
		FGameEvent* EventData = Events.Find(EventID);
		if (EventData == NULL)				   
		{
			// Add the event, data pair
			Events.Set(EventID, FGameEvent(EC_EventParm));
			EventData = Events.Find(EventID);
		}

		check(EventData);
		// Always add to the whole thing
		EventData->AddEventData(0, Value);
		if (TimePeriod > 0)
		{
			EventData->AddEventData(TimePeriod, Value);
		}
	}
}

/* 
 *   Accumulate a weapon event's data
 * @param EventID - the event to record
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FWeaponEvents::AddWeaponIntEvent(INT EventID, struct FWeaponIntEvent* GameEventData, INT TimePeriod)
{
	TotalEvents.AddEvent(EventID, GameEventData->Value, TimePeriod);
	if (EventsByClass.IsValidIndex(GameEventData->WeaponClassIndex))
	{
		EventsByClass(GameEventData->WeaponClassIndex).AddEvent(EventID, GameEventData->Value, TimePeriod);
	}
}

/* 
 *   Accumulate a projectile event's data
 * @param EventID - the event to record
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FProjectileEvents::AddProjectileIntEvent(INT EventID, struct FProjectileIntEvent* GameEventData, INT TimePeriod)
{
	TotalEvents.AddEvent(EventID, GameEventData->Value, TimePeriod);
	if (EventsByClass.IsValidIndex(GameEventData->ProjectileClassIndex))
	{
		EventsByClass(GameEventData->ProjectileClassIndex).AddEvent(EventID, GameEventData->Value, TimePeriod);
	}
}

/* 
 *   Accumulate a kill event for a given damage type
 * @param EventID - the event to record
 * @param KillTypeID - the ID of the kill type recorded
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FDamageEvents::AddKillEvent(INT EventID, INT KillTypeID, struct FPlayerKillDeathEvent* GameEventData, INT TimePeriod)
{
	TotalEvents.AddEvent(UCONST_GAMEEVENT_AGGREGATED_DAMAGE_KILLS, 1, TimePeriod);
	TotalEvents.AddEvent(KillTypeID, 1, TimePeriod);
	if (EventsByClass.IsValidIndex(GameEventData->DamageClassIndex))
	{
		EventsByClass(GameEventData->DamageClassIndex).AddEvent(UCONST_GAMEEVENT_AGGREGATED_DAMAGE_KILLS, 1, TimePeriod);
		EventsByClass(GameEventData->DamageClassIndex).AddEvent(KillTypeID, 1, TimePeriod);
	}
}

/* 
 *   Accumulate a death event for a given damage type
 * @param EventID - the event to record
 * @param KillTypeID - the ID of the kill type recorded
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FDamageEvents::AddDeathEvent(INT EventID, INT KillTypeID, struct FPlayerKillDeathEvent* GameEventData, INT TimePeriod)
{
	TotalEvents.AddEvent(UCONST_GAMEEVENT_AGGREGATED_DAMAGE_DEATHS, 1, TimePeriod);
	TotalEvents.AddEvent(KillTypeID, 1, TimePeriod);
	if (EventsByClass.IsValidIndex(GameEventData->DamageClassIndex))
	{
		EventsByClass(GameEventData->DamageClassIndex).AddEvent(UCONST_GAMEEVENT_AGGREGATED_DAMAGE_DEATHS, 1, TimePeriod);
		EventsByClass(GameEventData->DamageClassIndex).AddEvent(KillTypeID, 1, TimePeriod);
	}
}

/* 
 *   Accumulate a damage event for a given damage type
 * @param EventID - the event to record
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FDamageEvents::AddDamageIntEvent(INT EventID, struct FDamageIntEvent* GameEventData, INT TimePeriod)
{
	if (EventID == UCONST_GAMEEVENT_AGGREGATED_DAMAGE_DEALT_MELEE_DAMAGE)
	{
		TotalEvents.AddEvent(UCONST_GAMEEVENT_AGGREGATED_DAMAGE_DEALT_MELEEHITS, 1, TimePeriod);
		TotalEvents.AddEvent(EventID, GameEventData->Value, TimePeriod);
		if (EventsByClass.IsValidIndex(GameEventData->DamageClassIndex))
		{
			EventsByClass(GameEventData->DamageClassIndex).AddEvent(UCONST_GAMEEVENT_AGGREGATED_DAMAGE_DEALT_MELEEHITS, 1, TimePeriod);
			EventsByClass(GameEventData->DamageClassIndex).AddEvent(EventID, GameEventData->Value, TimePeriod);
		}
	}
	else if(EventID == UCONST_GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_MELEE_DAMAGE)
	{
		TotalEvents.AddEvent(UCONST_GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_WASMELEEHIT, 1, TimePeriod);
		TotalEvents.AddEvent(EventID, GameEventData->Value, TimePeriod);
		if (EventsByClass.IsValidIndex(GameEventData->DamageClassIndex))
		{
			EventsByClass(GameEventData->DamageClassIndex).AddEvent(UCONST_GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_WASMELEEHIT, 1, TimePeriod);
			EventsByClass(GameEventData->DamageClassIndex).AddEvent(EventID, GameEventData->Value, TimePeriod);
		}
	}
	else
	{
		TotalEvents.AddEvent(EventID, GameEventData->Value, TimePeriod);
		if (EventsByClass.IsValidIndex(GameEventData->DamageClassIndex))
		{
			EventsByClass(GameEventData->DamageClassIndex).AddEvent(EventID, GameEventData->Value, TimePeriod);
		}
	}
}

/* 
 *   Accumulate a pawn event for a given pawn type
 * @param EventID - the event to record
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FPawnEvents::AddPlayerSpawnEvent(INT EventID, struct FPlayerSpawnEvent* GameEventData, INT TimePeriod)
{
	TotalEvents.AddEvent(EventID, 1, TimePeriod);
	if (EventsByClass.IsValidIndex(GameEventData->PawnClassIndex))
	{
		EventsByClass(GameEventData->PawnClassIndex).AddEvent(EventID, 1, TimePeriod);
	}
}

/** 
 * Accumulate data for a generic event
 * @param EventID - the event to record
 * @param TimePeriod - time period slot to use (0 - game total, 1+ round total)
 * @param Value - value to accumulate 
 */
void FTeamEvents::AddEvent(INT EventID, FLOAT Value, INT TimePeriod)
{
	TotalEvents.AddEvent(EventID, Value, TimePeriod);
}

/* 
 *   Accumulate a kill event for a given damage type
 * @param EventID - the event to record
 * @param KillTypeID - the ID of the kill type recorded
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FTeamEvents::AddKillEvent(INT EventID, INT KillTypeID, struct FPlayerKillDeathEvent* GameEventData, INT TimePeriod)
{
	TotalEvents.AddEvent(UCONST_GAMEEVENT_AGGREGATED_TEAM_KILLS, 1, TimePeriod);
	DamageAsPlayerEvents.AddKillEvent(EventID, KillTypeID, GameEventData, TimePeriod);
}

/* 
 *   Accumulate a death event for a given damage type
 * @param EventID - the event to record
 * @param KillTypeID - the ID of the kill type recorded
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FTeamEvents::AddDeathEvent(INT EventID, INT KillTypeID, struct FPlayerKillDeathEvent* GameEventData, INT TimePeriod)
{
	TotalEvents.AddEvent(UCONST_GAMEEVENT_AGGREGATED_TEAM_DEATHS, 1, TimePeriod);
	DamageAsTargetEvents.AddDeathEvent(EventID, KillTypeID, GameEventData, TimePeriod);
}

/* 
 *   Accumulate a weapon event's data
 * @param EventID - the event to record
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FTeamEvents::AddWeaponIntEvent(INT EventID, struct FWeaponIntEvent* GameEventData, INT TimePeriod)
{
	WeaponEvents.AddWeaponIntEvent(EventID, GameEventData, TimePeriod);
}

/* 
 *   Accumulate a damage event for a given damage type where the team member was the attacker
 * @param EventID - the event to record
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FTeamEvents::AddDamageDoneIntEvent(INT EventID, struct FDamageIntEvent* GameEventData, INT TimePeriod)
{
	DamageAsPlayerEvents.AddDamageIntEvent(EventID, GameEventData, TimePeriod);
}

/* 
 *   Accumulate a damage event for a given damage type where the team member was the target
 * @param EventID - the event to record
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FTeamEvents::AddDamageTakenIntEvent(INT EventID, struct FDamageIntEvent* GameEventData, INT TimePeriod)
{
	DamageAsTargetEvents.AddDamageIntEvent(EventID, GameEventData, TimePeriod);
}

/* 
 *   Accumulate a pawn event for a given pawn type
 * @param EventID - the event to record
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FTeamEvents::AddPlayerSpawnEvent(INT EventID, struct FPlayerSpawnEvent* GameEventData, INT TimePeriod)
{
	PawnEvents.AddPlayerSpawnEvent(EventID, GameEventData, TimePeriod);
}

/* 
 *   Accumulate a projectile event's data
 * @param EventID - the event to record
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FTeamEvents::AddProjectileIntEvent(INT EventID, struct FProjectileIntEvent* GameEventData, INT TimePeriod)
{
	ProjectileEvents.AddProjectileIntEvent(EventID, GameEventData, TimePeriod);
}

/** 
 * Accumulate data for a generic event
 * @param EventID - the event to record
 * @param TimePeriod - time period slot to use (0 - game total, 1+ round total)
 * @param Value - value to accumulate 
 */
void FPlayerEvents::AddEvent(INT EventID, FLOAT Value, INT TimePeriod)
{
	TotalEvents.AddEvent(EventID, Value, TimePeriod);
}

/* 
 *   Accumulate a kill event for a given damage type
 * @param EventID - the event to record
 * @param KillTypeID - the ID of the kill type recorded
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FPlayerEvents::AddKillEvent(INT EventID, INT KillTypeID, struct FPlayerKillDeathEvent* GameEventData, INT TimePeriod)
{
	TotalEvents.AddEvent(UCONST_GAMEEVENT_AGGREGATED_PLAYER_KILLS, 1, TimePeriod);
	DamageAsPlayerEvents.AddKillEvent(EventID, KillTypeID, GameEventData, TimePeriod);
}

/* 
 *   Accumulate a death event for a given damage type
 * @param EventID - the event to record
 * @param KillTypeID - the ID of the kill type recorded
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FPlayerEvents::AddDeathEvent(INT EventID, INT KillTypeID, struct FPlayerKillDeathEvent* GameEventData, INT TimePeriod)
{
	TotalEvents.AddEvent(UCONST_GAMEEVENT_AGGREGATED_PLAYER_DEATHS, 1, TimePeriod);
	DamageAsTargetEvents.AddDeathEvent(EventID, KillTypeID, GameEventData, TimePeriod);
}

/* 
 *   Accumulate a weapon event's data
 * @param EventID - the event to record
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FPlayerEvents::AddWeaponIntEvent(INT EventID, struct FWeaponIntEvent* GameEventData, INT TimePeriod)
{
	WeaponEvents.AddWeaponIntEvent(EventID, GameEventData, TimePeriod);
}

/* 
 *   Accumulate a damage event for a given damage type where the player was the attacker
 * @param EventID - the event to record
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FPlayerEvents::AddDamageDoneIntEvent(INT EventID, struct FDamageIntEvent* GameEventData, INT TimePeriod)
{
	DamageAsPlayerEvents.AddDamageIntEvent(EventID, GameEventData, TimePeriod);
}

/* 
 *   Accumulate a damage event for a given damage type where the player was the target
 * @param EventID - the event to record
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FPlayerEvents::AddDamageTakenIntEvent(INT EventID, struct FDamageIntEvent* GameEventData, INT TimePeriod)
{
	DamageAsTargetEvents.AddDamageIntEvent(EventID, GameEventData, TimePeriod);
}

/* 
 *   Accumulate a pawn event for a given pawn type
 * @param EventID - the event to record
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FPlayerEvents::AddPlayerSpawnEvent(INT EventID, struct FPlayerSpawnEvent* GameEventData, INT TimePeriod)
{
	PawnEvents.AddPlayerSpawnEvent(EventID, GameEventData, TimePeriod);
}

/* 
 *   Accumulate a projectile event's data
 * @param EventID - the event to record
 * @param GameEventData - the event data
 * @param TimePeriod - a given time period (0 - game total, 1+ round total) 
 */
void FPlayerEvents::AddProjectileIntEvent(INT EventID, struct FProjectileIntEvent* GameEventData, INT TimePeriod)
{
	ProjectileEvents.AddProjectileIntEvent(EventID, GameEventData, TimePeriod);
}

/** 
 * Returns the metadata associated with the given index,
 * overloaded to access aggregate events not found in the stream directly 
 */
const FGameplayEventMetaData& UGameStatsAggregator::GetEventMetaData(INT EventID) const
{
	check(Reader);

	// Look to the aggregator list of supported events first
	for (INT SearchIdx=0; SearchIdx<AggregateEvents.Num(); SearchIdx++)
	{
		const FGameplayEventMetaData& AggEventData = AggregateEvents(SearchIdx);
		if (AggEventData.EventID == EventID)
		{
			return AggEventData;
		}
	}

	// Fallback to the event metadata in the file as a last resort
	const FGameplayEventMetaData& EventMetaData = Reader->GetEventMetaData(EventID);
	return EventMetaData;
}

/*
 *   Set the game state this aggregator will use
 * @param InGameState - game state object to use
 */
void UGameStatsAggregator::SetGameState(UGameStateObject* InGameState)
{
	GameState = InGameState;
}

/** Cleanup all data related to this aggregation */
void UGameStatsAggregator::Reset()
{
	AllGameEvents.ClearEvents();

	for (INT TeamIter=0; TeamIter<AllTeamEvents.Num(); TeamIter++)
	{
		AllTeamEvents(TeamIter).ClearEvents();
	}
	AllTeamEvents.Empty();

	for (INT PlayerIter=0; PlayerIter<AllPlayerEvents.Num(); PlayerIter++)
	{
		AllPlayerEvents(PlayerIter).ClearEvents();
	}
	AllPlayerEvents.Empty();

	AllWeaponEvents.ClearEvents();
	AllProjectileEvents.ClearEvents();
	AllPawnEvents.ClearEvents();
	AllDamageEvents.ClearEvents();
	AggregateEventsMapping.Empty();
	AggregatesFound.Empty();
}

/** Signal start of stream processing */
void UGameStatsAggregator::PreProcessStream()
{
	/** Initialization of the aggregation object (sets up game state, etc) */
	check(Reader);
	check(GameState);

	Super::PreProcessStream();
   
	INT NumPlayers = Reader->PlayerList.Num() + 1;	// +1 for PlayerIndex -1
	INT NumTeams = Reader->TeamList.Num() + 1;		// +1 for TeamIndex -1/255
	INT NumWeaponClasses = Reader->WeaponClassArray.Num();
	INT NumDamageClasses = Reader->DamageClassArray.Num();
	INT NumProjectileClasses = Reader->ProjectileClassArray.Num();
	INT NumPawnClasses = Reader->PawnClassArray.Num();

	// Initialize all the containers
	AllTeamEvents.AddZeroed(NumTeams);
	for (INT TeamIter=0; TeamIter<NumTeams; TeamIter++)
	{
		AllTeamEvents(TeamIter).WeaponEvents.EventsByClass.AddZeroed(NumWeaponClasses);
		AllTeamEvents(TeamIter).DamageAsPlayerEvents.EventsByClass.AddZeroed(NumDamageClasses);
		AllTeamEvents(TeamIter).DamageAsTargetEvents.EventsByClass.AddZeroed(NumDamageClasses);
		AllTeamEvents(TeamIter).ProjectileEvents.EventsByClass.AddZeroed(NumProjectileClasses);
		AllTeamEvents(TeamIter).PawnEvents.EventsByClass.AddZeroed(NumPawnClasses);
	}

	AllPlayerEvents.AddZeroed(NumPlayers);
	for (INT PlayerIter=0; PlayerIter<NumPlayers; PlayerIter++)
	{
		AllPlayerEvents(PlayerIter).WeaponEvents.EventsByClass.AddZeroed(NumWeaponClasses);
		AllPlayerEvents(PlayerIter).DamageAsPlayerEvents.EventsByClass.AddZeroed(NumDamageClasses);
		AllPlayerEvents(PlayerIter).DamageAsTargetEvents.EventsByClass.AddZeroed(NumDamageClasses);
		AllPlayerEvents(PlayerIter).ProjectileEvents.EventsByClass.AddZeroed(NumProjectileClasses);
		AllPlayerEvents(PlayerIter).PawnEvents.EventsByClass.AddZeroed(NumPawnClasses);
	}

	AllWeaponEvents.EventsByClass.AddZeroed(NumWeaponClasses);
	AllProjectileEvents.EventsByClass.AddZeroed(NumProjectileClasses);
	AllPawnEvents.EventsByClass.AddZeroed(NumPawnClasses);
	AllDamageEvents.EventsByClass.AddZeroed(NumDamageClasses);

	// Populate the lookup table
	for (INT i=0; i<AggregatesList.Num(); i++)
	{
		const FAggregateEventMapping& Mapping = AggregatesList(i);
		if (Mapping.EventID > 0)
		{
			AggregateEventsMapping.Set(Mapping.EventID, Mapping);
		}
	}
}

/** Signal end of stream processing */
void UGameStatsAggregator::PostProcessStream()
{
	check(Reader);
	// This should only happen in cases where the file was closed before match end
	if (GameState->SessionType != GT_Multiplayer || GameState->bIsMatchStarted)
	{
		// This should only happen in cases where the file was closed before round end
		if (GameState->SessionType != GT_Multiplayer || GameState->bIsRoundStarted)
		{
			GameState->CleanupRoundState(Reader->CurrentSessionInfo.GameplaySessionEndTime);
			AddEndOfRoundStats();
		}

		GameState->CleanupMatchState(Reader->CurrentSessionInfo.GameplaySessionEndTime);
		AddEndOfMatchStats();
	}
}

UBOOL UGameStatsAggregator::GetAggregateMappingIDs(INT EventID,INT& AggregateID,INT& TargetAggregateID)
{
	FAggregateEventMapping* AggregateMapping = AggregateEventsMapping.Find(EventID);
	if (AggregateMapping)
	{
		AggregateID = AggregateMapping->AggregateID;
		TargetAggregateID = AggregateMapping->TargetAggregateID;
		return TRUE;
	}
	else
	{
		AggregateID = INDEX_NONE;
		TargetAggregateID = INDEX_NONE;
		return FALSE;
	}
}

/** 
 * Cleanup for a given player at the end of a round
 * @param PlayerIndex - player to cleanup/record stats for
 */
void UGameStatsAggregator::AddPlayerEndOfRoundStats(INT PlayerIndex)
{
	const FPlayerState* PlayerState = GameState->GetPlayerState(PlayerIndex);
	if (PlayerState && PlayerState->TimeAliveSinceLastDeath > 0)
	{
		// Per Player
		FPlayerEvents& PlayerEvents = GetPlayerEvents(PlayerIndex);
		PlayerEvents.AddEvent(UCONST_GAMEEVENT_AGGREGATED_PLAYER_TIMEALIVE, PlayerState->TimeAliveSinceLastDeath, GameState->GetRoundNumber());

#if ALL_AGGREGATES
		// Per Team
		if (PlayerState->CurrentTeamIndex >= 0)
		{
			FTeamEvents& TeamEvents = GetTeamEvents(PlayerState->CurrentTeamIndex);

			// Time Alive
			TeamEvents.AddEvent(UCONST_GAMEEVENT_AGGREGATED_PLAYER_TIMEALIVE, PlayerState->TimeAliveSinceLastDeath, GameState->GetRoundNumber());
		}
#endif // ALL_AGGREGATES
	}
}

/** Triggered by the end of round event, adds any additional aggregate stats required */
void UGameStatsAggregator::AddEndOfRoundStats()
{
	check(Reader);
	// Make sure all round data is properly credited
	for (INT PlayerIndex=0; PlayerIndex<Reader->PlayerList.Num(); PlayerIndex++)
	{
		AddPlayerEndOfRoundStats(PlayerIndex);
	}
}

/** Triggered by the end of match event, adds any additional aggregate stats required */
void UGameStatsAggregator::AddEndOfMatchStats()
{
	// Make sure all round/match data is properly credited
}

void UGameStatsAggregator::HandleGameStringEvent(struct FGameEventHeader& GameEvent, FGameStringEvent* GameEventData)
{
   /** Nothing to aggregate */
}

void UGameStatsAggregator::HandleGameIntEvent(struct FGameEventHeader& GameEvent, FGameIntEvent* GameEventData)
{
	switch (GameEvent.EventID)
	{
	case UCONST_GAMEEVENT_MATCH_ENDED:
		// Make sure all round/match data is properly credited
		AddEndOfMatchStats();
		break;
	case UCONST_GAMEEVENT_ROUND_ENDED:
		// Make sure all round data is properly credited
		AddEndOfRoundStats();
		break;
	case UCONST_GAMEEVENT_ROUND_STARTED:
	case UCONST_GAMEEVENT_MATCH_STARTED:
		break;
	default:
		{
			INT AggregateID, TargetAggregateID;
			if (GetAggregateMappingIDs(GameEvent.EventID, AggregateID, TargetAggregateID))
			{
				// Per Event
				AllGameEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());
			}
			break;
		}
	}
}

void UGameStatsAggregator::HandleGameFloatEvent(struct FGameEventHeader& GameEvent, FGameFloatEvent* GameEventData)
{
	INT AggregateID, TargetAggregateID;
	if (GetAggregateMappingIDs(GameEvent.EventID, AggregateID, TargetAggregateID))
	{
		// Per Event
		AllGameEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());
	}
}

void UGameStatsAggregator::HandleGamePositionEvent(struct FGameEventHeader& GameEvent, struct FGamePositionEvent* GameEventData)
{

}

void UGameStatsAggregator::HandleTeamStringEvent(struct FGameEventHeader& GameEvent, FTeamStringEvent* GameEventData)
{
	/** Nothing to aggregate */
}

void UGameStatsAggregator::HandleTeamIntEvent(struct FGameEventHeader& GameEvent, FTeamIntEvent* GameEventData)
{
	if (GameEventData->TeamIndex >= 0)
	{ 
		INT AggregateID, TargetAggregateID;
		if (GetAggregateMappingIDs(GameEvent.EventID, AggregateID, TargetAggregateID))
		{
		    // Per Team
		    FTeamState* TeamState = GameState->GetTeamState(GameEventData->TeamIndex);
		    FTeamEvents& TeamEvents = GetTeamEvents(GameEventData->TeamIndex);
		    TeamEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());
    
		    // Per Player
		    for (INT PlayerIter=0; PlayerIter<TeamState->PlayerIndices.Num(); PlayerIter++)
		    {
			    FPlayerState* PlayerState = GameState->GetPlayerState(TeamState->PlayerIndices(PlayerIter));
			    if (PlayerState->PlayerIndex >= 0)
			    {
				    FPlayerEvents& PlayerEvents = GetPlayerEvents(PlayerState->PlayerIndex);
				    PlayerEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());
			    }
		    }
    
		    // Per Event
		    AllGameEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());
		}
	}
	else
	{
		debugf(TEXT("HandleTeamIntEvent: Invalid team index %d"), GameEventData->TeamIndex);
	}
}

void UGameStatsAggregator::HandleTeamFloatEvent(struct FGameEventHeader& GameEvent, FTeamFloatEvent* GameEventData)
{
	if (GameEventData->TeamIndex >= 0)
	{
	    INT AggregateID, TargetAggregateID;
	    if (GetAggregateMappingIDs(GameEvent.EventID, AggregateID, TargetAggregateID))
	    {
		    // Per Team
		    FTeamState* TeamState = GameState->GetTeamState(GameEventData->TeamIndex);
		    FTeamEvents& TeamEvents = GetTeamEvents(GameEventData->TeamIndex);
		    TeamEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());
    
		    // Per Player
		    for (INT PlayerIter=0; PlayerIter<TeamState->PlayerIndices.Num(); PlayerIter++)
		    {
			    FPlayerState* PlayerState = GameState->GetPlayerState(TeamState->PlayerIndices(PlayerIter));
			    if (PlayerState->PlayerIndex >= 0)
			    {
				    FPlayerEvents& PlayerEvents = GetPlayerEvents(PlayerState->PlayerIndex);
				    PlayerEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());
			    }
		    }
    
		    // Per Event
		    AllGameEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());
	    }
	}
	else
	{
		debugf(TEXT("HandleTeamFloatEvent: Invalid team index %d"), GameEventData->TeamIndex);
	}
}

void UGameStatsAggregator::HandlePlayerIntEvent(struct FGameEventHeader& GameEvent, FPlayerIntEvent* GameEventData)
{
	INT PlayerIndex;
	FRotator UnusedRotation;
	ConvertToPlayerIndexAndRotation(GameEventData->PlayerIndexAndYaw, GameEventData->PlayerPitchAndRoll, PlayerIndex, UnusedRotation);

	if (PlayerIndex >= 0)
	{
		INT AggregateID, TargetAggregateID;
		if (GetAggregateMappingIDs(GameEvent.EventID, AggregateID, TargetAggregateID))
		{
			// Per Player
			FPlayerEvents& PlayerEvents = GetPlayerEvents(PlayerIndex);
			PlayerEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());

#if ALL_AGGREGATES
			// Per Team
			FPlayerState* PlayerState = GameState->GetPlayerState(PlayerIndex);
			if (PlayerState->CurrentTeamIndex >= 0)
			{
				FTeamEvents& TeamEvents = GetTeamEvents(PlayerState->CurrentTeamIndex);
				TeamEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());
			}

			// Per Event
			AllGameEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());
#endif // ALL_AGGREGATES
		}
	}
	else
	{
		debugf(TEXT("HandlePlayerIntEvent %d: Invalid player index %d"), GameEvent.EventID, PlayerIndex);
	}
}

void UGameStatsAggregator::HandlePlayerFloatEvent(struct FGameEventHeader& GameEvent, FPlayerFloatEvent* GameEventData)
{
	INT PlayerIndex;
	FRotator UnusedRotation;
	ConvertToPlayerIndexAndRotation(GameEventData->PlayerIndexAndYaw, GameEventData->PlayerPitchAndRoll, PlayerIndex, UnusedRotation);

	if (PlayerIndex >= 0)
	{
		INT AggregateID, TargetAggregateID;
		if (GetAggregateMappingIDs(GameEvent.EventID, AggregateID, TargetAggregateID))
		{
			// Per Player
			FPlayerEvents& PlayerEvents = GetPlayerEvents(PlayerIndex);
			PlayerEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());

#if ALL_AGGREGATES
			// Per Team
			FPlayerState* PlayerState = GameState->GetPlayerState(PlayerIndex);
			if (PlayerState->CurrentTeamIndex >= 0)
			{
				FTeamEvents& TeamEvents = GetTeamEvents(PlayerState->CurrentTeamIndex);
				TeamEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());
			}

			// Per Event
			AllGameEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());
#endif // ALL_AGGREGATES
		}
	}
	else
	{
		debugf(TEXT("HandlePlayerFloatEvent %d: Invalid player index %d"), GameEvent.EventID, PlayerIndex);
	}
}

void UGameStatsAggregator::HandlePlayerStringEvent(struct FGameEventHeader& GameEvent, FPlayerStringEvent* GameEventData)
{
	/** Nothing to aggregate */
}

void UGameStatsAggregator::HandlePlayerSpawnEvent(struct FGameEventHeader& GameEvent, FPlayerSpawnEvent* GameEventData)
{
	INT PlayerIndex;
	FRotator UnusedRotation;
	ConvertToPlayerIndexAndRotation(GameEventData->PlayerIndexAndYaw, GameEventData->PlayerPitchAndRoll, PlayerIndex, UnusedRotation);

	if (PlayerIndex >= 0)
	{
		INT AggregateID, TargetAggregateID;
		if (GetAggregateMappingIDs(GameEvent.EventID, AggregateID, TargetAggregateID))
		{
			// Per Player
			FPlayerEvents& PlayerEvents = GetPlayerEvents(PlayerIndex);
			PlayerEvents.AddPlayerSpawnEvent(AggregateID, GameEventData, GameState->GetRoundNumber());

#if ALL_AGGREGATES
			// Per Team
			FPlayerState* PlayerState = GameState->GetPlayerState(PlayerIndex);
			if (PlayerState->CurrentTeamIndex >= 0)
			{
				FTeamEvents& TeamEvents = GetTeamEvents(PlayerState->CurrentTeamIndex);
				TeamEvents.AddPlayerSpawnEvent(AggregateID, GameEventData, GameState->GetRoundNumber());
			}

			// Per Pawn Type
			AllPawnEvents.AddPlayerSpawnEvent(AggregateID, GameEventData, GameState->GetRoundNumber());

			// Per Event
			AllGameEvents.AddEvent(AggregateID, 1, GameState->GetRoundNumber());
#endif // ALL_AGGREGATES
		}
	}
	else
	{
		debugf(TEXT("HandlePlayerSpawnEvent %d: Invalid player index %d"), GameEvent.EventID, PlayerIndex);
	}
}

void UGameStatsAggregator::HandlePlayerLoginEvent(struct FGameEventHeader& GameEvent, FPlayerLoginEvent* GameEventData)
{
	INT PlayerIndex;
	FRotator UnusedRotation;
	ConvertToPlayerIndexAndRotation(GameEventData->PlayerIndexAndYaw, GameEventData->PlayerPitchAndRoll, PlayerIndex, UnusedRotation);

	if (PlayerIndex >= 0)
	{
		FPlayerState* PlayerState = GameState->GetPlayerState(PlayerIndex);
		FPlayerEvents& PlayerEvents = GetPlayerEvents(PlayerIndex);

		switch (GameEvent.EventID)
		{
		case UCONST_GAMEEVENT_PLAYER_LOGOUT:
			AddPlayerEndOfRoundStats(PlayerIndex);
			break;
		}

		INT AggregateID, TargetAggregateID;
		if (GetAggregateMappingIDs(GameEvent.EventID, AggregateID, TargetAggregateID))
		{
			// Per Player
			PlayerEvents.AddEvent(AggregateID, 1, GameState->GetRoundNumber());

#if ALL_AGGREGATES
			// Per Team
			if (PlayerState->CurrentTeamIndex >= 0)
			{
				FTeamEvents& TeamEvents = GetTeamEvents(PlayerState->CurrentTeamIndex);
				TeamEvents.AddEvent(AggregateID, 1, GameState->GetRoundNumber());
			}

			// Per Event
			AllGameEvents.AddEvent(AggregateID, 1, GameState->GetRoundNumber());
#endif
		}
	}
	else
	{
		debugf(TEXT("HandlePlayerLoginEvent %d: Invalid player index %d"), GameEvent.EventID, PlayerIndex);
	}
}

void UGameStatsAggregator::HandlePlayerKillDeathEvent(struct FGameEventHeader& GameEvent, FPlayerKillDeathEvent* GameEventData)
{
	INT PlayerIndex, TargetIndex;
	FRotator UnusedRotation;
	ConvertToPlayerIndexAndRotation(GameEventData->PlayerIndexAndYaw, GameEventData->PlayerPitchAndRoll, PlayerIndex, UnusedRotation);
	ConvertToPlayerIndexAndRotation(GameEventData->TargetIndexAndYaw, GameEventData->TargetPitchAndRoll, TargetIndex, UnusedRotation);
	
	if (PlayerIndex >= 0)
	{
		FPlayerState* PlayerState = GameState->GetPlayerState(PlayerIndex);
		FPlayerState* TargetState = GameState->GetPlayerState(TargetIndex);

		INT AggregateID, TargetAggregateID;
		GetAggregateMappingIDs(GameEvent.EventID, AggregateID, TargetAggregateID);

		INT KillTypeAggregateID, KillTypeTargetAggregateID;
		GetAggregateMappingIDs(GameEventData->KillType, KillTypeAggregateID, KillTypeTargetAggregateID);

		switch(GameEvent.EventID)
		{
		case UCONST_GAMEEVENT_PLAYER_KILL:
			{
				// Player killer, target dead
				// Per Player
				if (PlayerIndex != TargetIndex)
				{
					FPlayerEvents& PlayerEvents = GetPlayerEvents(PlayerIndex);
					PlayerEvents.AddKillEvent(AggregateID, KillTypeAggregateID, GameEventData, GameState->GetRoundNumber());

					if (TargetIndex >= 0)
					{
						FPlayerEvents& TargetEvents = GetPlayerEvents(TargetIndex);
						TargetEvents.AddDeathEvent(TargetAggregateID, KillTypeTargetAggregateID, GameEventData, GameState->GetRoundNumber());
					}

#if ALL_AGGREGATES
					// Per Team	Player
					if (PlayerState->CurrentTeamIndex >= 0)
					{
						FTeamEvents& TeamEvents = GetTeamEvents(PlayerState->CurrentTeamIndex);
						TeamEvents.AddKillEvent(AggregateID, KillTypeAggregateID, GameEventData, GameState->GetRoundNumber());
					}

					// Per Team Target
					if (TargetState->CurrentTeamIndex >= 0)
					{
						FTeamEvents& TeamEvents = GetTeamEvents(TargetState->CurrentTeamIndex);
						TeamEvents.AddDeathEvent(TargetAggregateID, KillTypeTargetAggregateID, GameEventData, GameState->GetRoundNumber());
					}

					// Per Damage Class
					AllDamageEvents.AddKillEvent(AggregateID, KillTypeAggregateID, GameEventData, GameState->GetRoundNumber());
					AllDamageEvents.AddDeathEvent(TargetAggregateID, KillTypeTargetAggregateID, GameEventData, GameState->GetRoundNumber());
#endif // ALL_AGGREGATES
				}
				break;
			}
		case UCONST_GAMEEVENT_PLAYER_DEATH:
			{
				// Player dead, target killer
				// Per Player
				FPlayerEvents& PlayerEvents = GetPlayerEvents(PlayerIndex);

				// Time Alive
				PlayerEvents.AddEvent(UCONST_GAMEEVENT_AGGREGATED_PLAYER_TIMEALIVE, PlayerState->TimeAliveSinceLastDeath, GameState->GetRoundNumber());

				// Suicides handled here
				if (PlayerIndex == TargetIndex)
				{
					PlayerEvents.AddDeathEvent(TargetAggregateID, KillTypeTargetAggregateID, GameEventData, GameState->GetRoundNumber());
				}

#if ALL_AGGREGATES
				// Per Team
				if (PlayerState->CurrentTeamIndex >= 0)
				{
					FTeamEvents& TeamEvents = GetTeamEvents(PlayerState->CurrentTeamIndex);
					
					// Time Alive
					TeamEvents.AddEvent(UCONST_GAMEEVENT_AGGREGATED_PLAYER_TIMEALIVE, PlayerState->TimeAliveSinceLastDeath, GameState->GetRoundNumber());

					// Suicides handled here
					if (PlayerIndex == TargetIndex)
					{
						TeamEvents.AddDeathEvent(TargetAggregateID, KillTypeTargetAggregateID, GameEventData, GameState->GetRoundNumber());
					}
				}

				if (PlayerIndex == TargetIndex)
				{
					AllDamageEvents.AddDeathEvent(TargetAggregateID, KillTypeTargetAggregateID, GameEventData, GameState->GetRoundNumber());
				}
#endif // ALL_AGGREGATES
				break;
			}
		}

#if ALL_AGGREGATES
		// Per Game
		AllGameEvents.AddEvent(AggregateID, 1, GameState->GetRoundNumber());
#endif // ALL_AGGREGATES
	}
	else
	{
		debugf(TEXT("HandlePlayerKillDeathEvent %d: Invalid player index %d"), GameEvent.EventID, PlayerIndex);
	}
}

void UGameStatsAggregator::HandlePlayerPlayerEvent(struct FGameEventHeader& GameEvent, FPlayerPlayerEvent* GameEventData)
{
	INT PlayerIndex, TargetIndex;
	FRotator UnusedRotation;

	INT AggregateID, TargetAggregateID;
	if (GetAggregateMappingIDs(GameEvent.EventID, AggregateID, TargetAggregateID))
	{
		ConvertToPlayerIndexAndRotation(GameEventData->PlayerIndexAndYaw, GameEventData->PlayerPitchAndRoll, PlayerIndex, UnusedRotation);
		if (PlayerIndex >= 0)
		{
			// Per Player
			FPlayerEvents& PlayerEvents = GetPlayerEvents(PlayerIndex);
			PlayerEvents.AddEvent(AggregateID, 1, GameState->GetRoundNumber());

#if ALL_AGGREGATES
			// Per Team
			FPlayerState* PlayerState = GameState->GetPlayerState(PlayerIndex);
			if (PlayerState->CurrentTeamIndex >= 0)
			{
				FTeamEvents& TeamEvents = GetTeamEvents(PlayerState->CurrentTeamIndex);
				TeamEvents.AddEvent(AggregateID, 1, GameState->GetRoundNumber());
			}

			// Per Event
			AllGameEvents.AddEvent(AggregateID, 1, GameState->GetRoundNumber());
#endif // ALL_AGGREGATES
		}
		else
		{
			debugf(TEXT("HandlePlayerPlayerEvent %d: Invalid player index %d"), GameEvent.EventID, PlayerIndex);
		}

		ConvertToPlayerIndexAndRotation(GameEventData->TargetIndexAndYaw, GameEventData->TargetPitchAndRoll, TargetIndex, UnusedRotation);
		if (TargetIndex >= 0)
		{
			// Per Player
			FPlayerEvents& TargetEvents = GetPlayerEvents(TargetIndex);
			TargetEvents.AddEvent(TargetAggregateID, 1, GameState->GetRoundNumber());

#if ALL_AGGREGATES
			// Per Team
			FPlayerState* TargetState = GameState->GetPlayerState(TargetIndex);
			if (TargetState->CurrentTeamIndex >= 0)
			{
				FTeamEvents& TeamEvents = GetTeamEvents(TargetState->CurrentTeamIndex);
				TeamEvents.AddEvent(TargetAggregateID, 1, GameState->GetRoundNumber());
			}

			// Per Event
			AllGameEvents.AddEvent(TargetAggregateID, 1, GameState->GetRoundNumber());
#endif // ALL_AGGREGATES
		}
		else
		{
			debugf(TEXT("HandlePlayerPlayerEvent: Invalid target index %d"), TargetIndex);
		}
	}
}

void UGameStatsAggregator::HandlePlayerLocationsEvent(struct FGameEventHeader& GameEvent, FPlayerLocationsEvent* GameEventData)
{
	/** Nothing to aggregate */
}

void UGameStatsAggregator::HandleWeaponIntEvent(struct FGameEventHeader& GameEvent, FWeaponIntEvent* GameEventData)
{
	INT AggregateID, TargetAggregateID;
	if (GetAggregateMappingIDs(GameEvent.EventID, AggregateID, TargetAggregateID))
	{
		INT PlayerIndex;
		FRotator UnusedRotation;

		ConvertToPlayerIndexAndRotation(GameEventData->PlayerIndexAndYaw, GameEventData->PlayerPitchAndRoll, PlayerIndex, UnusedRotation);
		if (PlayerIndex >= 0)
		{
			// Per Player
			FPlayerEvents& PlayerEvents = GetPlayerEvents(PlayerIndex);
			PlayerEvents.AddWeaponIntEvent(AggregateID, GameEventData, GameState->GetRoundNumber());

#if ALL_AGGREGATES
			// Per Team
			FPlayerState* PlayerState = GameState->GetPlayerState(PlayerIndex);
			if (PlayerState->CurrentTeamIndex >= 0)
			{
				FTeamEvents& TeamEvents = GetTeamEvents(PlayerState->CurrentTeamIndex);
				TeamEvents.AddWeaponIntEvent(AggregateID, GameEventData, GameState->GetRoundNumber());
			}

			// Per Weapon
			AllWeaponEvents.AddWeaponIntEvent(AggregateID, GameEventData, GameState->GetRoundNumber());

			// Per Game
			AllGameEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());
#endif // ALL_AGGREGATES
		}
		else
		{
			debugf(TEXT("HandleWeaponIntEvent %d: Invalid player index %d"), GameEvent.EventID, PlayerIndex);
		}
	}
}

void UGameStatsAggregator::HandleDamageIntEvent(struct FGameEventHeader& GameEvent, FDamageIntEvent* GameEventData)
{
	INT AggregateID, TargetAggregateID;
	if (GetAggregateMappingIDs(GameEvent.EventID, AggregateID, TargetAggregateID))
	{
		INT PlayerIndex;
		FRotator UnusedRotation;

		ConvertToPlayerIndexAndRotation(GameEventData->PlayerIndexAndYaw, GameEventData->PlayerPitchAndRoll, PlayerIndex, UnusedRotation);
		if (PlayerIndex >= 0)
		{
			// Per Player
			FPlayerEvents& PlayerEvents = GetPlayerEvents(PlayerIndex);
			PlayerEvents.AddDamageDoneIntEvent(AggregateID, GameEventData, GameState->GetRoundNumber());

#if ALL_AGGREGATES
			// Per Team
			FPlayerState* PlayerState = GameState->GetPlayerState(PlayerIndex);
			if (PlayerState->CurrentTeamIndex >= 0)
			{
				FTeamEvents& TeamEvents = GetTeamEvents(PlayerState->CurrentTeamIndex);
				TeamEvents.AddDamageDoneIntEvent(AggregateID, GameEventData, GameState->GetRoundNumber());
			}
#endif // ALL_AGGREGATES
		}
		else
		{
			debugf(TEXT("HandleDamageIntEvent %d: Invalid player index %d"),  GameEvent.EventID, PlayerIndex);
		}

		INT TargetIndex;
		ConvertToPlayerIndexAndRotation(GameEventData->TargetPlayerIndexAndYaw, GameEventData->TargetPlayerPitchAndRoll, TargetIndex, UnusedRotation);
		if (TargetIndex >= 0)
		{
			// Per Target
			FPlayerEvents& PlayerEvents = GetPlayerEvents(TargetIndex);
			PlayerEvents.AddDamageTakenIntEvent(TargetAggregateID, GameEventData, GameState->GetRoundNumber());

#if ALL_AGGREGATES
			// Per Target Team
			FPlayerState* PlayerState = GameState->GetPlayerState(TargetIndex);
			if (PlayerState->CurrentTeamIndex >= 0)
			{
				FTeamEvents& TeamEvents = GetTeamEvents(PlayerState->CurrentTeamIndex);
				TeamEvents.AddDamageTakenIntEvent(TargetAggregateID, GameEventData, GameState->GetRoundNumber());
			}
#endif // ALL_AGGREGATES
		}
		else
		{
			debugf(TEXT("HandleDamageIntEvent %d: Invalid target index %d"),  GameEvent.EventID, TargetIndex);
		}

#if ALL_AGGREGATES
		if (PlayerIndex >= 0)
		{
			// Per Damage Class
			AllDamageEvents.AddDamageIntEvent(AggregateID, GameEventData, GameState->GetRoundNumber());
			AllDamageEvents.AddDamageIntEvent(TargetAggregateID, GameEventData, GameState->GetRoundNumber());

			// Per Event
			AllGameEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber());
		}
#endif // ALL_AGGREGATES
	}
}

void UGameStatsAggregator::HandleProjectileIntEvent(struct FGameEventHeader& GameEvent, FProjectileIntEvent* GameEventData)
{
	INT AggregateID, TargetAggregateID;
	if (GetAggregateMappingIDs(GameEvent.EventID, AggregateID, TargetAggregateID))
	{
		INT PlayerIndex;
		FRotator UnusedRotation;

		ConvertToPlayerIndexAndRotation(GameEventData->PlayerIndexAndYaw, GameEventData->PlayerPitchAndRoll, PlayerIndex, UnusedRotation);
		if (PlayerIndex >= 0)
		{
			// Per Player
			FPlayerEvents& PlayerEvents = GetPlayerEvents(PlayerIndex);
			PlayerEvents.AddProjectileIntEvent(AggregateID, GameEventData, GameState->GetRoundNumber());

#if ALL_AGGREGATES
			// Per Team
			FPlayerState* PlayerState = GameState->GetPlayerState(PlayerIndex);
			if (PlayerState->CurrentTeamIndex >= 0)
			{
				FTeamEvents& TeamEvents = GetTeamEvents(PlayerState->CurrentTeamIndex);
				TeamEvents.AddProjectileIntEvent(AggregateID, GameEventData, GameState->GetRoundNumber());
			}

			// Per Projectile
			AllProjectileEvents.AddProjectileIntEvent(AggregateID, GameEventData, GameState->GetRoundNumber());

			// Per Event
			AllGameEvents.AddEvent(AggregateID, GameEventData->Value, GameState->GetRoundNumber()); 
#endif // ALL_AGGREGATES
		}
		else
		{
			debugf(TEXT("HandleProjectileIntEvent %d: Invalid player index %d"), GameEvent.EventID, PlayerIndex);
		}
	}
}

#undef warnexprf
