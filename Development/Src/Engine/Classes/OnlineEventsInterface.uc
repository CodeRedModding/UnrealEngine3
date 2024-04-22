/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This interface deals with capturing gameplay events for logging with an online service
 */
interface OnlineEventsInterface;

/**
 * Sends the profile data to the server for statistics aggregation
 *
 * @param UniqueId the unique id for the player
 * @param PlayerNick the player's nick name
 * @param ProfileSettings the profile object that is being sent
 * @param PlayerStorage the player storage object that is being sent
 *
 * @return true if the async task was started successfully, false otherwise
 */
function bool UploadPlayerData(UniqueNetId UniqueId,string PlayerNick,OnlineProfileSettings ProfileSettings,OnlinePlayerStorage PlayerStorage);

/**
 * Sends gameplay event data to MCP
 *
 * @param UniqueId the player that is sending the stats
 * @param Payload the stats data to upload
 *
 * @return true if the async send started ok, false otherwise
 */
function bool UploadGameplayEventsData(UniqueNetId UniqueId,const out array<byte> Payload);

/**
 * Sends the network backend the playlist population for this host
 *
 * @param PlaylistId the playlist we are updating the population for
 * @param NumPlayers the number of players on this host in this playlist
 *
 * @return true if the async send started ok, false otherwise
 */
function bool UpdatePlaylistPopulation(int PlaylistId,int NumPlayers);
