/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Keeps track of current state as a game stats stream is parsed
 */
class GameStateObject extends GameplayEventsHandler
	native(GameStats);

/** Types of game sessions the state object can handle */
enum GameSessionType
{
	GT_SessionInvalid,
	GT_SinglePlayer,
	GT_Coop,
	GT_Multiplayer
};

/** State variables related to a game team */
struct native TeamState
{
	/** Team Index related to team metadata array */
	var int TeamIndex;
	/** Array of player indices that were ever on a given team */
	var init array<int> PlayerIndices;
};

/** All teams present in the game over the course of the stream */
var native const Array_Mirror TeamStates{TArray<FTeamState*>};

/** Contains the notion of player state while parsing the stats stream */
struct native PlayerState
{
	/** Player index related to player metadata array */
	var int PlayerIndex;
	/** Current team index (changes with TEAM_CHANGE event) */
	var int CurrentTeamIndex;
	/** Last time player spawned */
	var float TimeSpawned; 
	/** If non-zero represents the time between a spawn and death event */
	var float TimeAliveSinceLastDeath;
};

/** All players present in the game over the course of the stream */
var native const Array_Mirror PlayerStates{TArray<FPlayerState*>};

/** Type of game session we are parsing */
var GameSessionType SessionType;
/** Has the stream passed a match started event */
var bool bIsMatchStarted;
/** True if between round started and ended events */
var bool bIsRoundStarted; 
/** Current round number reported by the last round started event */
var int RoundNumber;
/** Maximum round number reported during the match */
var int MaxRoundNumber;

cpptext
{
	/** Return the round number in a given match, -1 if its not multiplayer */
	INT GetRoundNumber() { if (SessionType == GT_Multiplayer) { return RoundNumber; } else { return -1; } }

	/*
	 *   Get a given team's current state, creating a new one if necessary
	 *   @param TeamIndex - index of team to return state for
	 *   @return State for given team
	 */
	virtual FTeamState* GetTeamState(INT TeamIndex)
	{
		INT TeamStateIdx = 0;
		for (; TeamStateIdx < TeamStates.Num(); TeamStateIdx++)
		{
			if (TeamStates(TeamStateIdx)->TeamIndex == TeamIndex)
			{
				break;
			}
		}

		//Create a new team if necessary
		if (TeamStateIdx == TeamStates.Num())
		{
			FTeamState* NewTeamState = new FTeamState;
			NewTeamState->TeamIndex = TeamIndex;
			TeamStateIdx = TeamStates.AddItem(NewTeamState);
		}

		return TeamStates(TeamStateIdx);
	}

	/*
	 *   Get a given player's current state, creating a new one if necessary
	 *   @param PlayerIndex - index of player to return state for
	 *   @return State for given player
	 */
	virtual FPlayerState* GetPlayerState(INT PlayerIndex)
	{
		INT PlayerStateIdx = 0;
		for (; PlayerStateIdx < PlayerStates.Num(); PlayerStateIdx++)
		{
			if (PlayerStates(PlayerStateIdx)->PlayerIndex == PlayerIndex)
			{
				break;
			}
		}

		//Create a new player if necessary
		if (PlayerStateIdx == PlayerStates.Num())
		{
			FPlayerState* NewPlayerState = new FPlayerState;
			NewPlayerState->PlayerIndex = PlayerIndex;
			NewPlayerState->CurrentTeamIndex = INDEX_NONE;
			NewPlayerState->TimeSpawned = 0;
			NewPlayerState->TimeAliveSinceLastDeath = 0;
			PlayerStateIdx = PlayerStates.AddItem(NewPlayerState);
		}

		return PlayerStates(PlayerStateIdx);
	}

	/*
	 *   Get the team index for a given player
	 * @param PlayerIndex - player to get team index for
	 * @return Game specific team index
	 */
	INT GetTeamIndexForPlayer(INT PlayerIndex)
	{
		 const FPlayerState* PlayerState = GetPlayerState(PlayerIndex);
		 return PlayerState->CurrentTeamIndex;
	}

	/** Handlers for parsing the game stats stream */

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

	/*
	 * Called when end of round event is parsed, allows for any current
	 * state values to be closed out (time alive, etc) 
	 * @param TimeStamp - time of the round end event
	 */
	virtual void CleanupRoundState(FLOAT TimeStamp);
	/*
	 * Called when end of match event is parsed, allows for any current
	 * state values to be closed out (round events, etc) 
	 * @param TimeStamp - time of the match end event
	 */
	virtual void CleanupMatchState(FLOAT TimeStamp);

	/*
	 *   Cleanup a player state (round over/logout)
	 */
	virtual void CleanupPlayerState(INT PlayerIndex, FLOAT TimeStamp);
}

/** A chance to do something before the stream starts */
native event PreProcessStream();

/** Completely reset the game state object */
native function Reset();

defaultproperties
{

}

