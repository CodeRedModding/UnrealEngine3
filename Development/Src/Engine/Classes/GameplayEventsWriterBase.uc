/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Base class for handling recorded gameplay events that need to be written
 */
class GameplayEventsWriterBase extends GameplayEvents
	native;

`define INCLUDE_GAME_STATS(dummy)
`include(Engine\Classes\GameStats.uci);
`undefine(INCLUDE_GAME_STATS)

/** Reference to the game (set by StartLogging/EndLogging) */
var const GameInfo Game;

/** Returns whether or not a logging session has been started */
function bool IsSessionInProgress()
{
	return CurrentSessionInfo.bGameplaySessionInProgress;
}

/**
 * Start timer for heartbeat polling
 * @param HeartbeatDelta - polling frequency in seconds
 */
event StartPolling(float HearbeatDelta)
{
	local WorldInfo WI;
	WI = class'WorldInfo'.static.GetWorldInfo();
	if (WI != None && 
		WI.Game != None)
	{
		WI.Game.SetTimer(HearbeatDelta,true,nameof(Poll),self);
	}
}

/**
 * Stop timer for heartbeat polling
 */
event StopPolling()
{
	local WorldInfo WI;
	WI = class'WorldInfo'.static.GetWorldInfo();
	if (WI != None && 
		WI.Game != None)
	{
		WI.Game.ClearTimer(nameof(Poll),self);
	}
}

/** Heartbeat function to record various stats (player location, etc) */
function Poll()
{
	local WorldInfo WI;
	WI = class'WorldInfo'.static.GetWorldInfo();
	if (WI.Pauser == None)
	{
		//Get a sample of where everyone is at the moment
		if (WI.Game != None && 
			!WI.Game.bWaitingToStartMatch)
		{
			LogAllPlayerPositionsEvent(`PlayerStatId(LOCATION_POLL));
			WI.Game.GameEventsPoll();
		}

		LogSystemPollEvents();
	}
}

/**
 * Get the Id for the gametype the game is using
 * @return Id related to the game specific game type being played
 */
event int GetGameTypeId() 
{ 
	return 0; 
}

/** 
 * Get the playlist id the game is using
 * @return Id related to the game specific playlist id in use
 */
event int GetPlaylistId() 
{ 
	return -1; 
}

/** 
 * Mark a new session, clear existing events, etc 
 *
 * @param HeartbeatDelta - polling frequency (0 turns it off)
 */
function StartLogging(optional float HeartbeatDelta);

/** 
 * Resets the session, clearing all event data, but keeps the session ID/Timestamp intact
 * @param HeartbeatDelta - polling frequency (0 turns it off)
 */
function ResetLogging(optional float HeartbeatDelta);

/** 
 * Mark the end of a logging session
 * closes file, stops polling, etc
 */
function EndLogging();

/**
* Logs an int base game event
*
* @param EventId the event being logged
* @param Value the value associated with the event
*/
function LogGameIntEvent(int EventId, int Value);

/**
* Logs a string based game event
*
* @param EventId the event being logged
* @param Value the value associated with the event
*/
function LogGameStringEvent(int EventId, string Value);

/**
* Logs a float based game event
*
* @param EventId the event being logged
* @param Value the value associated with the event
*/
function LogGameFloatEvent(int EventId, float Value);

/**
* Logs a position based game event
*
* @param EventId the event being logged
* @param Position the position of the event
* @param Value the value associated with the event
*/
function LogGamePositionEvent(int EventId, const out vector Position, float Value);

/**
* Logs a int based team event
*
* @param EventId - the event being logged
* @param Team - the team associated with this event
* @param Value - the value associated with the event
*/
function LogTeamIntEvent(int EventId, TeamInfo Team, int Value);

/**
* Logs a float based team event
*
* @param EventId - the event being logged
* @param Team - the team associated with this event
* @param Value - the value associated with the event
*/
function LogTeamFloatEvent(int EventId, TeamInfo Team, float Value);

/**
* Logs a string based team event
*
* @param EventId - the event being logged
* @param Team - the team associated with this event
* @param Value - the value associated with the event
*/
function LogTeamStringEvent(int EventId, TeamInfo Team, string Value);

/**
* Logs an event with an integer value associated with it
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param Value the value for this event
*/
function LogPlayerIntEvent(int EventId, Controller Player, int Value);

/**
* Logs an event with an float value associated with it
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param Value the value for this event
*/
function LogPlayerFloatEvent(int EventId, Controller Player, float Value);

/**
* Logs an event with an string value associated with it
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param EventString the value for this event
*/
function LogPlayerStringEvent(int EventId, Controller Player, string EventString);

/**
* Logs a spawn event for a player (team, class, etc)
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param PawnClass the pawn this player spawned with
* @param Team the team the player is on
*/
function LogPlayerSpawnEvent(int EventId, Controller Player, class<Pawn> PawnClass, int TeamID);

/**
* Logs when a player leaves/joins a session
*
* @param EventId the login/logout event for the player
* @param Player the player that joined/left
* @param PlayerName the name of the player in question
* @param PlayerId the net id of the player in question
* @param bSplitScreen whether the player is on splitscreen
*/
function LogPlayerLoginChange(int EventId, Controller Player, string PlayerName, UniqueNetId PlayerId, bool bSplitScreen);

/**
* Logs the location of all players when this event occurred 
*
* @param EventId the event being logged
*/
function LogAllPlayerPositionsEvent(int EventId);

/**
* Logs a player killing and a player being killed
*
* @param EventId the event that should be written
* @param KillType the additional information about a kill
* @param Killer the player that did the killing
* @param DmgType the damage type that was done
* @param Dead the player that was killed
*/
function LogPlayerKillDeath(int EventId, int KillType, Controller Killer, class<DamageType> DmgType, Controller Dead);

/**
* Logs a player to player event
*
* @param EventId the event that should be written
* @param Player the player that triggered the event
* @param Target the player that was the recipient
*/
function LogPlayerPlayerEvent(int EventId, Controller Player, Controller Target);

/**
* Logs a weapon event with an integer value associated with it
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param WeaponClass the weapon class associated with the event
* @param Value the value for this event
*/
function LogWeaponIntEvent(int EventId, Controller Player, class<Weapon> WeaponClass, int Value);

/**
* Logs damage with the amount that was done and to whom it was done
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param DmgType the damage type that was done
* @param Target the player being damaged
* @param Amount the amount of damage done
*/
function LogDamageEvent(int EventId, Controller Player, class<DamageType> DmgType, Controller Target, int Amount);

/**
* Logs a projectile event with an integer value associated with it
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param Proj the projectile class associated with the event
* @param Value the value for this event
*/
function LogProjectileIntEvent(int EventId, Controller Player, class<Projectile> Proj, int Value);

/** Log various system properties like memory usage, network usage, etc. */
function LogSystemPollEvents();

/** AI pathfinding failure event */
function RecordAIPathFail(Controller AI, coerce string reason, vector dest);

/** Coverlink fire event */
function int RecordCoverLinkFireLinks(CoverLink Link,Controller Player);