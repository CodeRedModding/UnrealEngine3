/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"

/** Helper class that should be declared in the function using it for minimal scoping, but gcc doesn't handle this properly */
class MatchSorter
{
public:
	static inline INT Compare(const FOnlineGameSearchResult& A,const FOnlineGameSearchResult& B)
	{
		// Only sort by match quality if ranked
		if (A.GameSettings->bUsesArbitration && B.GameSettings->bUsesArbitration)
		{
			// If the pings are in the same bucket, then 
			if (A.GameSettings->PingInMs == B.GameSettings->PingInMs)
			{
				// Sort descending based off of the match quality determined previously
				const FLOAT Difference = B.GameSettings->MatchQuality - A.GameSettings->MatchQuality;
				INT SortValue = 0;
				if (Difference < -KINDA_SMALL_NUMBER)
				{
					SortValue = -1;
				}
				else if (Difference > KINDA_SMALL_NUMBER)
				{
					SortValue = 1;
				}
				return SortValue;
			}
		}
		// Sort ascending based off of ping
		return A.GameSettings->PingInMs - B.GameSettings->PingInMs;
	}
};

/**
 * Allows a search object to provide a customized sort routine to order the results in
 * a way that best fits the game type
 */
void UOnlineGameSearch::SortSearchResults(void)
{
	if (PingBucketSize > 0)
	{
		// Place games into ping buckets so that skill is more valuable than similar pings
		for (INT Index = 0; Index < Results.Num(); Index++)
		{
			UOnlineGameSettings* GameSettings = Results(Index).GameSettings;
			if (GameSettings)
			{
				// Figure out what bucket to put this in
				INT NumDivided = GameSettings->PingInMs / PingBucketSize;
				if (GameSettings->PingInMs % PingBucketSize)
				{
					// Round to the next bucket up
					NumDivided++;
				}
				GameSettings->PingInMs = NumDivided * PingBucketSize;
			}
		}
	}
	// Now sort the results
	Sort<FOnlineGameSearchResult,MatchSorter>(Results.GetTypedData(),Results.Num());
#if _DEBUG
	// Place games into ping buckets so that skill is more valuable than similar pings
	for (INT Index = 0; Index < Results.Num(); Index++)
	{
		UOnlineGameSettings* GameSettings = Results(Index).GameSettings;
		if (GameSettings)
		{
			debugf(NAME_DevOnline,
				TEXT("Session: %s ping: %d quality: %f"),
				*GameSettings->OwningPlayerName,
				GameSettings->PingInMs,
				GameSettings->MatchQuality);
		}
	}
#endif
}
