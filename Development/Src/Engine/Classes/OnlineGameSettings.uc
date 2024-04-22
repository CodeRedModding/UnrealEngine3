/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Holds the base configuration settings for an online game
 */
class OnlineGameSettings extends Settings
	dependson(OnlineSubsystem)
	native;

/** The number of publicly available connections advertised */
var databinding int NumPublicConnections;

/** The number of connections that are private (invite/password) only */
var databinding int NumPrivateConnections;

/** The number of publicly available connections that are available (read only) */
var databinding int NumOpenPublicConnections;

/** The number of private connections that are available (read only) */
var databinding int NumOpenPrivateConnections;

/** The server's nonce for this session */
var const qword ServerNonce;

/** Whether this match is publicly advertised on the online service */
var databinding bool bShouldAdvertise;

/** This game will be lan only and not be visible to external players */
var databinding bool bIsLanMatch;

/** Whether the match should gather stats or not */
var databinding bool bUsesStats;

/** Whether joining in progress is allowed or not */
var databinding bool bAllowJoinInProgress;

/** Whether the match allows invitations for this session or not */
var databinding bool bAllowInvites;

/** Whether to display user presence information or not */
var databinding bool bUsesPresence;

/** Whether joining via player presence is allowed or not */
var databinding bool bAllowJoinViaPresence;

/** Whether joining via player presence is allowed for friends only or not */
var databinding bool bAllowJoinViaPresenceFriendsOnly;

/** Whether the session should use arbitration or not */
var databinding bool bUsesArbitration;

/** Whether the server employs anti-cheat (punkbuster, vac, etc) */
var databinding bool bAntiCheatProtected;

/** Whether the game is an invitation or searched for game */
var const bool bWasFromInvite;

/** The owner of the game */
var databinding string OwningPlayerName;

/** The unique net id of the player that owns this game */
var UniqueNetId OwningPlayerId;

/** The ping of the server in milliseconds (-1 means the server was unreachable) */
var databinding int PingInMs;

/** Whether this server is a dedicated server or not */
var databinding bool bIsDedicated;

/** Represents how good a match this is in a range from 0 to 1 */
var databinding float MatchQuality;

/** The current state of the online game */
var const databinding EOnlineGameState GameState;

/** Whether there is a skill update in progress or not (don't do multiple at once) */
var const bool bHasSkillUpdateInProgress;

/** Used to keep different builds from seeing each other during searches */
var const int BuildUniqueId;

/** Whether to shrink the session slots when a player leaves the match or not */
var const bool bShouldShrinkArbitratedSessions;

defaultproperties
{
	bAllowJoinInProgress=true
	bUsesStats=true
	bShouldAdvertise=true
	bAllowInvites=true
	bUsesPresence=true
	bAllowJoinViaPresence=true
	bShouldShrinkArbitratedSessions=true
}