/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Stats class that holds the read request definitions and the resulting data
 */
class OnlineStatsRead extends OnlineStats
	native
	abstract;

/** A single instance of a stat in a row */
struct native OnlineStatsColumn
{
	/** The ordinal value of the column */
	var int ColumnNo;
	/** The value of the stat for this column */
	var SettingsData StatValue;
};

/** Holds a single player's set of data for this stats view */
struct native OnlineStatsRow
{
	/** The unique player id of the player these stats are for */
	var const UniqueNetId PlayerId;
	/** The rank of the player in this stats view */
	var const SettingsData Rank;
	/** Player's online nickname */
	var const string NickName;
	/** The set of columns (stat instances) for this row */
	var array<OnlineStatsColumn> Columns;
};

/** The unique id of the view that these stats are from */
var int ViewId;

/** The column id to use for sorting rank */
var const int SortColumnId;

/** The columns to read in the view we are interested in */
var const array<int> ColumnIds;

/** The total number of rows in the view */
var const int TotalRowsInView;

/** The rows of data returned by the online service */
var array<OnlineStatsRow> Rows;

/** Provides human readable values for column ids */
struct native ColumnMetaData
{
	/** Id for the given string */
	var const int Id;
	/** Human readable form of the Id */
	var const name Name;
	/** The name displayed in column headings in the UI */
	var localized string ColumnName;
};

/** Provides metadata for column ids so that we can present their human readable form */
var const array<ColumnMetaData> ColumnMappings;

/** The name of the view in human readable terms */
var const string ViewName;

/** An optional title id to specify when reading stats (zero uses the default for the exe) */
var const int TitleId;

/**
 * This event is called post read complete so that the stats object has a chance
 * synthesize new stats from returned data, e.g. ratios, averages, etc.
 */
event OnReadComplete();

/**
 * Searches the stat rows for the player and then finds the stat value from the specified column within that row
 *
 * @param PlayerId the player to search for
 * @param StatColumnNo the column number to look up
 * @param StatValue the out value that is assigned the stat
 *
 * @return whether the value was found for the player/column or not
 */
native function bool GetIntStatValueForPlayer(UniqueNetId PlayerId,int StatColumnNo,out int StatValue);

/**
 * Searches the stat rows for the player and then sets the stat value from the specified column within that row
 *
 * @param PlayerId the player to search for
 * @param StatColumnNo the column number to look up
 * @param StatValue the value to set that column to
 *
 * @return whether the value was found for the player/column or not
 */
native function bool SetIntStatValueForPlayer(UniqueNetId PlayerId,int StatColumnNo,int StatValue);

/**
 * Searches the stat rows for the player and then finds the stat value from the specified column within that row
 *
 * @param PlayerId the player to search for
 * @param StatColumnNo the column number to look up
 * @param StatValue the out value that is assigned the stat
 *
 * @return whether the value was found for the player/column or not
 */
native function bool GetFloatStatValueForPlayer(UniqueNetId PlayerId,int StatColumnNo,out float StatValue);

/**
 * Searches the stat rows for the player and then sets the stat value from the specified column within that row
 *
 * @param PlayerId the player to search for
 * @param StatColumnNo the column number to look up
 * @param StatValue the value to set that column to
 *
 * @return whether the value was found for the player/column or not
 */
native function bool SetFloatStatValueForPlayer(UniqueNetId PlayerId,int StatColumnNo,float StatValue);

/**
 * Searches the stat rows for the player and then finds the stat value from the specified column within that row
 *
 * @param PlayerId the player to search for
 * @param StatColumnNo the column number to look up
 * @param StatValue the out value that is assigned the stat in string form
 *
 * @return whether the value was found for the player/column or not
 */
native function bool GetStatValueForPlayerAsString(UniqueNetId PlayerId,int StatColumnNo,out String StatValue);

/**
 * Searches the stat rows for the player and then returns whether or not the stat is 0.  Only works on float, double, int, int64, and empty stat types.  Others return FALSE.
 * 
 * @param PlayerId the player to search for
 * @param StatColumnNo the column number to look up
 * 
 * @return whether the value is 0 (or equivalent)
 */
native function bool IsStatZero(UniqueNetId PlayerId,int StatColumnNo);

/**
 * Adds a player to the results if not present
 *
 * @param PlayerName the name to place in the data
 * @param PlayerId the player to search for
 */
native function AddPlayer(string PlayerName,UniqueNetId PlayerId);

/**
 * Searches the rows for the player and returns their rank on the leaderboard
 *
 * @param PlayerId the player to search for
 *
 * @return the rank for the player
 */
native function int GetRankForPlayer(UniqueNetId PlayerId);
