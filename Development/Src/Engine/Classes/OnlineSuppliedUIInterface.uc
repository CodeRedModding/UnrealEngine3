/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This interface provides accessors to platform-supplied UIs
 */
interface OnlineSuppliedUIInterface;

/**
 * Delegate fired when the supplied stats UI is closed
 */
delegate OnShowOnlineStatsUIComplete();

/**
 * Shows the platform supplied leaderboard UI
 *
 * @param Players the array of unique ids to show stats for
 * @param StatsRead holds the definitions of the tables to show the data for
 *		  (note that no results will be filled out)
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
function bool ShowOnlineStatsUI(const out array<UniqueNetId> Players, OnlineStatsRead StatsRead);

/**
 * Adds the delegate to the list to be notified when stats UI is closed
 *
 * @param ShowOnlineStatsUICompleteDelegate the delegate to add
 */
function AddShowOnlineStatsUICompleteDelegate(delegate<OnShowOnlineStatsUIComplete> ShowOnlineStatsUICompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param ShowOnlineStatsUICompleteDelegate the delegate to remove
 */
function ClearShowOnlineStatsUICompleteDelegate(delegate<OnShowOnlineStatsUIComplete> ShowOnlineStatsUICompleteDelegate);

/**
 * Shows the platform supplied matchmaking UI. This will eventually either the JoinOnlineGameComplete 
 * or CreateOnlineGameComplete delegates, depending on if it's server or client
 *
 * @param SearchingPlayerNum the index of the player searching for a match
 * @param SearchSettings settings used to search for
 * @param GameSettings the game settings to use if this player becomes the server
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
function bool ShowMatchmakingUI(byte SearchingPlayerNum, OnlineGameSearch SearchSettings, OnlineGameSettings GameSettings);


