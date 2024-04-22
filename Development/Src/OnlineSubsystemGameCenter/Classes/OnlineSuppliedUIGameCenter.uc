/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Provides an GameCenter supplied UI functionality
 */
class OnlineSuppliedUIGameCenter extends Object
	native
	implements(OnlineSuppliedUIInterface);


/** The array of delegates that notify leaderboard UI closed */
var array<delegate<OnShowOnlineStatsUIComplete> > ShowOnlineStatsUIDelegates;

/** The array of delegates that notify matchmaking UI closed */
//var array<delegate<OnShowMatchmakingUIComplete> > ShowMatchmakingUIDelegates;


/**
 * Delegate fired when the supplied stats UI is closed
 */
delegate OnShowOnlineStatsUIComplete();

/**
 * Shows the platform supplid leaderboard UI
 *
 * @param Players the array of unique ids to show stats for
 * @param StatsRead holds the definitions of the tables to show the data for
 *		  (note that no results will be filled out)
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
native function bool ShowOnlineStatsUI(const out array<UniqueNetId> Players, OnlineStatsRead StatsRead);

/**
 * Adds the delegate to the list to be notified when stats UI is closed
 *
 * @param ShowOnlineStatsUICompleteDelegate the delegate to add
 */
function AddShowOnlineStatsUICompleteDelegate(delegate<OnShowOnlineStatsUIComplete> ShowOnlineStatsUICompleteDelegate)
{
	// Add this delegate to the array if not already present
	if (ShowOnlineStatsUIDelegates.Find(ShowOnlineStatsUICompleteDelegate) == INDEX_NONE)
	{
		ShowOnlineStatsUIDelegates[ShowOnlineStatsUIDelegates.Length] = ShowOnlineStatsUICompleteDelegate;
	}
}


/**
 * Removes the delegate from the notify list
 *
 * @param ShowOnlineStatsUICompleteDelegate the delegate to remove
 */
function ClearShowOnlineStatsUICompleteDelegate(delegate<OnShowOnlineStatsUIComplete> ShowOnlineStatsUICompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = ShowOnlineStatsUIDelegates.Find(ShowOnlineStatsUICompleteDelegate);
	// Remove this delegate from the array if found
	if (RemoveIndex != INDEX_NONE)
	{
		ShowOnlineStatsUIDelegates.Remove(RemoveIndex,1);
	}
}


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
native function bool ShowMatchmakingUI(byte SearchingPlayerNum, OnlineGameSearch SearchSettings, OnlineGameSettings GameSettings);
