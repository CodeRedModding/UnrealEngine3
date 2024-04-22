/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Uploads stream of gameplay events recorded during a session to analytics service
 */
class GameplayEventsUploadAnalytics extends GameplayEventsWriterBase
	native;

/** 
 * Mark a new session, clear existing events, etc 
 *
 * @param HeartbeatDelta - polling frequency (0 turns it off)
 */
native function StartLogging(optional float HeartbeatDelta);

/** 
 * Resets the session, clearing all event data, but keeps the session ID/Timestamp intact
 * @param HeartbeatDelta - polling frequency (0 turns it off)
 */
native function ResetLogging(optional float HeartbeatDelta);

/** 
 * Mark the end of a logging session
 * closes file, stops polling, etc
 */
native function EndLogging();

/**
* Logs an int base game event
*
* @param EventId the event being logged
* @param Value the value associated with the event
*/
native function LogGameIntEvent(int EventId, int Value);

/**
* Logs a string based game event
*
* @param EventId the event being logged
* @param Value the value associated with the event
*/
native function LogGameStringEvent(int EventId, string Value);

/**
* Logs a float based game event
*
* @param EventId the event being logged
* @param Value the value associated with the event
*/
native function LogGameFloatEvent(int EventId, float Value);

/**
* Logs a position based game event
*
* @param EventId the event being logged
* @param Position the position of the event
* @param Value the value associated with the event
*/
native function LogGamePositionEvent(int EventId, const out vector Position, float Value);

/**
* Logs a int based team event
*
* @param EventId - the event being logged
* @param Team - the team associated with this event
* @param Value - the value associated with the event
*/
native function LogTeamIntEvent(int EventId, TeamInfo Team, int Value);

/**
* Logs a float based team event
*
* @param EventId - the event being logged
* @param Team - the team associated with this event
* @param Value - the value associated with the event
*/
native function LogTeamFloatEvent(int EventId, TeamInfo Team, float Value);

/**
* Logs a string based team event
*
* @param EventId - the event being logged
* @param Team - the team associated with this event
* @param Value - the value associated with the event
*/
native function LogTeamStringEvent(int EventId, TeamInfo Team, string Value);

/**
* Logs an event with an integer value associated with it
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param Value the value for this event
*/
native function LogPlayerIntEvent(int EventId, Controller Player, int Value);

/**
* Logs an event with an float value associated with it
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param Value the value for this event
*/
native function LogPlayerFloatEvent(int EventId, Controller Player, float Value);

/**
* Logs an event with an string value associated with it
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param EventString the value for this event
*/
native function LogPlayerStringEvent(int EventId, Controller Player, string EventString);

/**
* Logs a spawn event for a player (team, class, etc)
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param PawnClass the pawn this player spawned with
* @param Team the team the player is on
*/
native function LogPlayerSpawnEvent(int EventId, Controller Player, class<Pawn> PawnClass, int TeamID);

/**
* Logs when a player leaves/joins a session
*
* @param EventId the login/logout event for the player
* @param Player the player that joined/left
* @param PlayerName the name of the player in question
* @param PlayerId the net id of the player in question
* @param bSplitScreen whether the player is on splitscreen
*/
native function LogPlayerLoginChange(int EventId, Controller Player, string PlayerName, UniqueNetId PlayerId, bool bSplitScreen);

/**
* Logs the location of all players when this event occurred 
*
* @param EventId the event being logged
*/
native function LogAllPlayerPositionsEvent(int EventId);

/**
* Logs a player killing and a player being killed
*
* @param EventId the event that should be written
* @param KillType the additional information about a kill
* @param Killer the player that did the killing
* @param DmgType the damage type that was done
* @param Dead the player that was killed
*/
native function LogPlayerKillDeath(int EventId, int KillType, Controller Killer, class<DamageType> DmgType, Controller Dead);

/**
* Logs a player to player event
*
* @param EventId the event that should be written
* @param Player the player that triggered the event
* @param Target the player that was the recipient
*/
native function LogPlayerPlayerEvent(int EventId, Controller Player, Controller Target);

/**
* Logs a weapon event with an integer value associated with it
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param WeaponClass the weapon class associated with the event
* @param Value the value for this event
*/
native function LogWeaponIntEvent(int EventId, Controller Player, class<Weapon> WeaponClass, int Value);

/**
* Logs damage with the amount that was done and to whom it was done
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param DmgType the damage type that was done
* @param Target the player being damaged
* @param Amount the amount of damage done
*/
native function LogDamageEvent(int EventId, Controller Player, class<DamageType> DmgType, Controller Target, int Amount);

/**
* Logs a projectile event with an integer value associated with it
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param Proj the projectile class associated with the event
* @param Value the value for this event
*/
native function LogProjectileIntEvent(int EventId, Controller Player, class<Projectile> Proj, int Value);

function GenericParamListStatEntry GetGenericParamListEntry();
function RecordAIPathFail(Controller AI, coerce string reason, vector dest);
function int RecordCoverLinkFireLinks(CoverLink Link,Controller Player);
