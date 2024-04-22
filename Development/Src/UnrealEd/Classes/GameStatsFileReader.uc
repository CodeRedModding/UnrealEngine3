/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*
* Streams gameplay events recorded during a session to disk
*/
class GameStatsFileReader extends GameplayEventsHandler
	dependson(GameStatsDatabase)
	native(GameStats);

// Stream processing helpers
var private GameStateObject GameState;

// All the relevant mappings after reading in a file
var GameSessionEntry SessionData;

// Amount all values in the indices are offset by (set by database before call to ProcessStream ) 
var int EventsOffset;

/** All events in the file */
var	const private native transient array<pointer> AllEvents{FIGameStatEntry};

cpptext
{
public:
	/** 
	 * The function that does the actual handling of data (override with particular implementation) 
	 * @param GameEvent - header of the current game event from disk
	 * @param GameEventData - payload immediately following the header
	 */
	virtual void HandleEvent(struct FGameEventHeader& GameEvent, class IGameEvent* GameEventData);

	/*
	 *   Set the game state this handler will use
	 * @param InGameState - game state object to use
	 */
	virtual void SetGameState(class UGameStateObject* InGameState);

	/** 
	 * Adds a new event created to the array of all events in the file 
	 * @param NewEvent - new event to add
	 * @param TeamIndex - Team Index for team events (INDEX_NONE if not a team event)
	 * @param PlayerIndex - Player Index for player events (INDEX_NONE if not a player event)
	 * @param TargetIndex - Target Index for player events (INDEX_NONE if event has no target)
	 */
	void AddNewEvent(struct FIGameStatEntry* NewEvent, INT TeamIndex, INT PlayerIndex, INT TargetIndex);
};

/** Free up the temp data */ 
native event Cleanup();







