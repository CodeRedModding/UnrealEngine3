/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UOnlineStats);
IMPLEMENT_CLASS(UOnlineStatsRead);
IMPLEMENT_CLASS(UOnlineStatsWrite);

/**
 * Searches the view id mappings to find the view id that matches the name
 *
 * @param ViewName the name of the view being searched for
 * @param ViewId the id of the view that matches the name
 *
 * @return true if it was found, false otherwise
 */
UBOOL UOnlineStats::GetViewId(FName ViewName,INT& ViewId)
{
	// Search through the array for the name and set the id that matches
	for (INT Index = 0; Index < ViewIdMappings.Num(); Index++)
	{
		const FStringIdToStringMapping& Mapping = ViewIdMappings(Index);
		if (Mapping.Name == ViewName)
		{
			ViewId = Mapping.Id;
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Finds the human readable name for the view
 *
 * @param ViewId the id to look up in the mappings table
 *
 * @return the name of the view that matches the id or NAME_None if not found
 */
FName UOnlineStats::GetViewName(INT ViewId)
{
	// Search through the array for an id that matches and return the name
	for (INT Index = 0; Index < ViewIdMappings.Num(); Index++)
	{
		const FStringIdToStringMapping& Mapping = ViewIdMappings(Index);
		if (Mapping.Id == ViewId)
		{
			return Mapping.Name;
		}
	}
	return NAME_None;
}

/**
 * Searches the stat mappings to find the stat id that matches the name
 *
 * @param StatName the name of the stat being searched for
 * @param StatId the out value that gets the id
 *
 * @return true if it was found, false otherwise
 */
UBOOL UOnlineStatsWrite::GetStatId(FName StatName,INT& StatId)
{
	// Now search through the potential stats for a matching name
	for (INT Index = 0; Index < StatMappings.Num(); Index++)
	{
		const FStringIdToStringMapping& Mapping = StatMappings(Index);
		if (Mapping.Name == StatName)
		{
			StatId = Mapping.Id;
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Searches the stat mappings to find human readable name for the stat id
 *
 * @param StatId the id of the stats to find the name for
 *
 * @return true if it was found, false otherwise
 */
FName UOnlineStatsWrite::GetStatName(INT StatId)
{
	// Now search through the potential stats for a matching id
	for (INT Index = 0; Index < StatMappings.Num(); Index++)
	{
		const FStringIdToStringMapping& Mapping = StatMappings(Index);
		if (Mapping.Id == StatId)
		{
			return Mapping.Name;
		}
	}
	return NAME_None;
}

/**
 * Sets a stat of type SDT_Float to the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatId the stat to change the value of
 * @param Value the new value to assign to the stat
 */
void UOnlineStatsWrite::SetFloatStat(INT StatId,FLOAT Value)
{
	FSettingsData* Stat = FindStat(StatId);
	if (Stat != NULL)
	{
		// Set the value
		Stat->SetData(Value);
	}
}

/**
 * Sets a stat of type SDT_Int to the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatId the stat to change the value of
 * @param Value the new value to assign to the stat
 */
void UOnlineStatsWrite::SetIntStat(INT StatId,INT Value)
{
	FSettingsData* Stat = FindStat(StatId);
	if (Stat != NULL)
	{
		// Set the value
		Stat->SetData(Value);
	}
}

/**
 * Increments a stat of type SDT_Float by the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatId the stat to increment
 * @param IncBy the value to increment by
 */
void UOnlineStatsWrite::IncrementFloatStat(INT StatId,FLOAT IncBy)
{
	FSettingsData* Stat = FindStat(StatId);
	if (Stat != NULL)
	{
		// Increment the value
		Stat->Increment<FLOAT,SDT_Float>(IncBy);
	}
}

/**
 * Increments a stat of type SDT_Int by the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatId the stat to increment
 * @param IncBy the value to increment by
 */
void UOnlineStatsWrite::IncrementIntStat(INT StatId,INT IncBy)
{
	FSettingsData* Stat = FindStat(StatId);
	if (Stat != NULL)
	{
		// Increment the value
		Stat->Increment<INT,SDT_Int32>(IncBy);
	}
}

/**
 * Decrements a stat of type SDT_Float by the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatId the stat to decrement
 * @param DecBy the value to decrement by
 */
void UOnlineStatsWrite::DecrementFloatStat(INT StatId,FLOAT DecBy)
{
	FSettingsData* Stat = FindStat(StatId);
	if (Stat != NULL)
	{
		// Decrement the value
		Stat->Decrement<FLOAT,SDT_Float>(DecBy);
	}
}

/**
 * Decrements a stat of type SDT_Int by the value specified. Does nothing
 * if the stat is not of the right type.
 *
 * @param StatId the stat to decrement
 * @param DecBy the value to decrement by
 */
void UOnlineStatsWrite::DecrementIntStat(INT StatId,INT DecBy)
{
	FSettingsData* Stat = FindStat(StatId);
	if (Stat != NULL)
	{
		// Decrement the value
		Stat->Decrement<INT,SDT_Int32>(DecBy);
	}
}

/**
 * Searches the stat rows for the player and then finds the stat value from the specified column within that row
 *
 * @param PlayerId the player to search for
 * @param StatColumnNo the column number to look up
 * @param StatValue the out value that is assigned the stat
 *
 * @return the value of the stat specified by that column no for the specified player
 */
UBOOL UOnlineStatsRead::GetIntStatValueForPlayer(FUniqueNetId PlayerId,INT StatColumnNo,INT& StatValue)
{
	// Search the rows for the player and return the stat value for them
	for (INT RowIndex = 0; RowIndex < Rows.Num(); RowIndex++)
	{
		if (Rows(RowIndex).PlayerID == PlayerId)
		{
			// Search the columns for the stat in question
			for (INT ColumnIndex = 0; ColumnIndex < Rows(RowIndex).Columns.Num(); ColumnIndex++)
			{
				if (Rows(RowIndex).Columns(ColumnIndex).ColumnNo == StatColumnNo)
				{
					StatValue = 0;
					Rows(RowIndex).Columns(ColumnIndex).StatValue.GetData(StatValue);
					return TRUE;
				}
			}
			return FALSE;
		}
	}
	return FALSE;
}

/**
 * Searches the stat rows for the player and then finds the stat value from the specified column within that row
 *
 * @param PlayerId the player to search for
 * @param StatColumnNo the column number to look up
 * @param StatValue the out value that is assigned the stat
 *
 * @return the value of the stat specified by that column no for the specified player
 */
UBOOL UOnlineStatsRead::SetIntStatValueForPlayer(FUniqueNetId PlayerId,INT StatColumnNo,INT StatValue)
{
	// Search the rows for the player and set the stat value for them
	for (INT RowIndex = 0; RowIndex < Rows.Num(); RowIndex++)
	{
		if (Rows(RowIndex).PlayerID == PlayerId)
		{
			// Search the columns for the stat in question
			for (INT ColumnIndex = 0; ColumnIndex < Rows(RowIndex).Columns.Num(); ColumnIndex++)
			{
				if (Rows(RowIndex).Columns(ColumnIndex).ColumnNo == StatColumnNo)
				{
					Rows(RowIndex).Columns(ColumnIndex).StatValue.SetData(StatValue);
					return TRUE;
				}
			}
			INT AddIndex = Rows(RowIndex).Columns.AddZeroed();
			Rows(RowIndex).Columns(AddIndex).ColumnNo = StatColumnNo;
			Rows(RowIndex).Columns(AddIndex).StatValue.SetData(StatValue);
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Searches the stat rows for the player and then finds the stat value from the specified column within that row
 *
 * @param PlayerId the player to search for
 * @param StatColumnNo the column number to look up
 * @param StatValue the out value that is assigned the stat
 *
 * @return the value of the stat specified by that column no for the specified player
 */
UBOOL UOnlineStatsRead::GetFloatStatValueForPlayer(FUniqueNetId PlayerId,INT StatColumnNo,FLOAT& StatValue)
{
	// Search the rows for the player and return the stat value for them
	for (INT RowIndex = 0; RowIndex < Rows.Num(); RowIndex++)
	{
		if (Rows(RowIndex).PlayerID == PlayerId)
		{
			// Search the columns for the stat in question
			for (INT ColumnIndex = 0; ColumnIndex < Rows(RowIndex).Columns.Num(); ColumnIndex++)
			{
				if (Rows(RowIndex).Columns(ColumnIndex).ColumnNo == StatColumnNo)
				{
					StatValue = 0;
					Rows(RowIndex).Columns(ColumnIndex).StatValue.GetData(StatValue);
					return TRUE;
				}
			}
			return FALSE;
		}
	}
	return FALSE;
}

/**
 * Searches the stat rows for the player and then finds the stat value from the specified column within that row
 *
 * @param PlayerId the player to search for
 * @param StatColumnNo the column number to look up
 * @param StatValue the out value that is assigned the stat
 *
 * @return the value of the stat specified by that column no for the specified player
 */
UBOOL UOnlineStatsRead::SetFloatStatValueForPlayer(FUniqueNetId PlayerId,INT StatColumnNo,FLOAT StatValue)
{
	// Search the rows for the player and set the stat value for them
	for (INT RowIndex = 0; RowIndex < Rows.Num(); RowIndex++)
	{
		if (Rows(RowIndex).PlayerID == PlayerId)
		{
			// Search the columns for the stat in question
			for (INT ColumnIndex = 0; ColumnIndex < Rows(RowIndex).Columns.Num(); ColumnIndex++)
			{
				if (Rows(RowIndex).Columns(ColumnIndex).ColumnNo == StatColumnNo)
				{
					Rows(RowIndex).Columns(ColumnIndex).StatValue.SetData(StatValue);
					return TRUE;
				}
			}
			INT AddIndex = Rows(RowIndex).Columns.AddZeroed();
			Rows(RowIndex).Columns(AddIndex).ColumnNo = StatColumnNo;
			Rows(RowIndex).Columns(AddIndex).StatValue.SetData(StatValue);
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Searches the stat rows for the player and then finds the stat value from the specified column within that row
 *
 * @param PlayerId the player to search for
 * @param StatColumnNo the column number to look up
 * @param StatValue the out value that is assigned the stat in string form
 *
 * @return whether the value was found for the player/column or not
 */
UBOOL UOnlineStatsRead::GetStatValueForPlayerAsString(struct FUniqueNetId PlayerId,INT StatColumnNo,FString& StatValue)
{
	// Search the rows for the player and return the stat value for them
	StatValue = FString(TEXT("--"));
	for (INT RowIndex = 0; RowIndex < Rows.Num(); RowIndex++)
	{
		if (Rows(RowIndex).PlayerID == PlayerId)
		{
			// Search the columns for the stat in question
			for (INT ColumnIndex = 0; ColumnIndex < Rows(RowIndex).Columns.Num(); ColumnIndex++)
			{
				if (Rows(RowIndex).Columns(ColumnIndex).ColumnNo == StatColumnNo)
				{
					StatValue = Rows(RowIndex).Columns(ColumnIndex).StatValue.ToString();
					return TRUE;
				}
			}
			return FALSE;
		}
	}
	return FALSE;
}

/**
* Searches the stat rows for the player and then returns whether or not the stat is 0.  Only works on float, double, int, int64, and empty stat types.  Others return FALSE.
* 
* @param PlayerId the player to search for
* @param StatColumnNo the column number to look up
* 
* @return whether the value is 0 (or equivalent)
*/
UBOOL UOnlineStatsRead::IsStatZero(FUniqueNetId PlayerId, INT StatColumnNo)
{
	// Search the rows for the player and return the stat value for them
	for (INT RowIndex = 0; RowIndex < Rows.Num(); RowIndex++)
	{
		if (Rows(RowIndex).PlayerID == PlayerId)
		{
			// Search the columns for the stat in question
			for (INT ColumnIndex = 0; ColumnIndex < Rows(RowIndex).Columns.Num(); ColumnIndex++)
			{
				// Found a match
				if (Rows(RowIndex).Columns(ColumnIndex).ColumnNo == StatColumnNo)
				{
					// Switch based on data types we care about
					const BYTE DataType = Rows(RowIndex).Columns(ColumnIndex).StatValue.Type;
					switch(DataType)
					{
						case SDT_Float:
						{
							FLOAT StatVal;
							Rows(RowIndex).Columns(ColumnIndex).StatValue.GetData(StatVal);
							return (StatVal == 0.f);
						}							
						case SDT_Int32:
						{
							INT StatVal;
							Rows(RowIndex).Columns(ColumnIndex).StatValue.GetData(StatVal);
							return (StatVal == 0);
						}
						case SDT_Int64:
						{
							QWORD StatVal;
							Rows(RowIndex).Columns(ColumnIndex).StatValue.GetData(StatVal);
							return (StatVal == 0);
						}
						case SDT_Double:
						{
							DOUBLE StatVal;
							Rows(RowIndex).Columns(ColumnIndex).StatValue.GetData(StatVal);
							return (StatVal == 0.0);
						}
						case SDT_Empty:
							return TRUE;
					}
				}
			}
		}
	}
	return FALSE;
}

/**
 * Adds a player to the results if not present
 *
 * @param PlayerName the name to place in the data
 * @param PlayerId the player to search for
 */
void UOnlineStatsRead::AddPlayer(const FString& PlayerName,FUniqueNetId PlayerId)
{
	// Search the rows for the player and adds them if missing
	for (INT RowIndex = 0; RowIndex < Rows.Num(); RowIndex++)
	{
		if (Rows(RowIndex).PlayerID == PlayerId)
		{
			return;
		}
	}
	INT AddIndex = Rows.AddZeroed();
	Rows(AddIndex).PlayerID = PlayerId;
	Rows(AddIndex).NickName = PlayerName;
}

/**
 * Searches the rows for the player and returns their rank on the leaderboard
 *
 * @param PlayerId the player to search for
 *
 * @return the rank for the player
 */
INT UOnlineStatsRead::GetRankForPlayer(FUniqueNetId PlayerId)
{
	// Search the rows for the player and set the stat value for them
	for (INT RowIndex = 0; RowIndex < Rows.Num(); RowIndex++)
	{
		if (Rows(RowIndex).PlayerID == PlayerId)
		{
			INT Rank;
			Rows(RowIndex).Rank.GetData(Rank);
			return Rank;
		}
	}
	return 0;
}
