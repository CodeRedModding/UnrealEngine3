/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Stats class that accumulates the stats data before submitting it to the
 * online subsytem for storage.
 */
class OnlineStatsWrite extends OnlineStats
	native
	abstract;

/** Maps the stat's column num to the human readable stat name */
var const array<StringIdToStringMapping> StatMappings;

/** The array of properties to publish to the stats view */
var const array<SettingsProperty> Properties;

/** This array contains the list of views to write the properties to */
var array<int> ViewIds;

/** This array contains the list of views to write the properties to for arbitrated matches */
var array<int> ArbitratedViewIds;

/** This is the property id that is used to rate on */
var const int RatingId;

/**
 * Delegate used to notify the caller when stats write has completed
 */
delegate OnStatsWriteComplete();

cpptext
{
	/**
	 * Finds the specified stat in the property list
	 *
	 * @param StatId the stat to search for
	 *
	 * @return pointer to the stat or NULL if not found
	 */
	FORCEINLINE FSettingsData* FindStat(INT StatId)
	{
		// Search for the individual stat
		for (INT PropertyIndex = 0; PropertyIndex < Properties.Num(); ++PropertyIndex)
		{
			FSettingsProperty& Stat = Properties(PropertyIndex);
			if (Stat.PropertyId == StatId)
			{
				return &Stat.Data;
			}
		}
		return NULL;
	}
}

/**
 * Searches the stat mappings to find the stat id that matches the name
 *
 * @param StatName the name of the stat being searched for
 * @param StatId the out value that gets the id
 *
 * @return true if it was found, false otherwise
 */
native function bool GetStatId(name StatName,out int StatId);

/**
 * Searches the stat mappings to find human readable name for the stat id
 *
 * @param StatId the id of the stats to find the name for
 *
 * @return true if it was found, false otherwise
 */
native function name GetStatName(int StatId);

/**
 * Sets a stat of type SDT_Float to the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatId the stat to change the value of
 * @param Value the new value to assign to the stat
 */
native function SetFloatStat(int StatId,float Value);

/**
 * Sets a stat of type SDT_Int to the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatId the stat to change the value of
 * @param Value the new value to assign to the stat
 */
native function SetIntStat(int StatId,int Value);

/**
 * Increments a stat of type SDT_Float by the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatId the stat to increment
 * @param IncBy the value to increment by
 */
native function IncrementFloatStat(int StatId,optional float IncBy = 1.0);

/**
 * Increments a stat of type SDT_Int by the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatId the stat to increment
 * @param IncBy the value to increment by
 */
native function IncrementIntStat(int StatId,optional int IncBy = 1);

/**
 * Decrements a stat of type SDT_Float by the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatId the stat to decrement
 * @param DecBy the value to decrement by
 */
native function DecrementFloatStat(int StatId,optional float DecBy = 1.0);

/**
 * Decrements a stat of type SDT_Int by the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatId the stat to decrement
 * @param DecBy the value to decrement by
 */
native function DecrementIntStat(int StatId,optional int DecBy = 1);
