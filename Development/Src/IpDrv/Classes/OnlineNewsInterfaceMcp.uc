/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Provides an in game news mechanism via the MCP backend
 */
class OnlineNewsInterfaceMcp extends MCPBase
	native
	implements(OnlineNewsInterface);

/** Holds the IP, hostname, URL, and news results for a particular news type */
struct native NewsCacheEntry
{
	/** The URL to the news page that we're reading */
	var const string NewsUrl;
	/** The current async read state for the operation */
	var EOnlineEnumerationReadState ReadState;
	/** The type of news that we are reading */
	var const EOnlineNewsType NewsType;
	/** The results of the read */
	var string NewsItem;
	/** The amount of time before giving up on the read */
	var const float TimeOut;
	/** Whether the news item is in unicode or ansi */
	var const bool bIsUnicode;
	/** Pointer to the native helper object that performs the download */
	var const native pointer HttpDownloader{class FHttpDownloadString};
};

/** The list of cached news items (ips, results, etc.) */
var config array<NewsCacheEntry> NewsItems;

/** The list of delegates to notify when the news read is complete */
var array<delegate<OnReadNewsCompleted> > ReadNewsDelegates;

/** Whether there are outstanding requests that need ticking or not */
var transient bool bNeedsTicking;

cpptext
{
// FTickableObject interface
	/**
	 * Ticks any outstanding news read requests
	 *
	 * @param DeltaTime the amount of time that has passed since the last tick
	 */
	virtual void Tick(FLOAT DeltaTime);

// News specific methods
	/**
	 * Finds the news cache entry for the specified type
	 *
	 * @param NewsType the type of news being read
	 *
	 * @return pointer to the news item or NULL if not found
	 */
	inline FNewsCacheEntry* FindNewsCacheEntry(BYTE NewsType)
	{
		for (INT NewsIndex = 0; NewsIndex < NewsItems.Num(); NewsIndex++)
		{
			if (NewsItems(NewsIndex).NewsType == NewsType)
			{
				return &NewsItems(NewsIndex);
			}
		}
		return NULL;
	}
}

/**
 * Reads the game specific news from the online subsystem
 *
 * @param LocalUserNum the local user the news is being read for
 * @param NewsType the type of news to read
 *
 * @return true if the async task was successfully started, false otherwise
 */
native function bool ReadNews(byte LocalUserNum,EOnlineNewsType NewsType);

/**
 * Delegate used in notifying the UI/game that the news read operation completed
 *
 * @param bWasSuccessful true if the read completed ok, false otherwise
 * @param NewsType the type of news read that just completed
 */
delegate OnReadNewsCompleted(bool bWasSuccessful,EOnlineNewsType NewsType);

/**
 * Sets the delegate used to notify the gameplay code that news reading has completed
 *
 * @param ReadGameNewsDelegate the delegate to use for notifications
 */
function AddReadNewsCompletedDelegate(delegate<OnReadNewsCompleted> ReadNewsDelegate)
{
	if (ReadNewsDelegates.Find(ReadNewsDelegate) == INDEX_NONE)
	{
		ReadNewsDelegates[ReadNewsDelegates.Length] = ReadNewsDelegate;
	}
}

/**
 * Removes the specified delegate from the notification list
 *
 * @param ReadGameNewsDelegate the delegate to use for notifications
 */
function ClearReadNewsCompletedDelegate(delegate<OnReadNewsCompleted> ReadGameNewsDelegate)
{
	local int RemoveIndex;

	RemoveIndex = ReadNewsDelegates.Find(ReadGameNewsDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		ReadNewsDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Returns the game specific news item from the cache
 *
 * @param LocalUserNum the local user the news is being read for
 * @param NewsType the type of news to read
 *
 * @return an empty string if no data was read, otherwise the contents of the news read
 */
function string GetNews(byte LocalUserNum,EOnlineNewsType NewsType)
{
	local int NewsIndex;

	// Search through the list of news items and return the one that matches
	for (NewsIndex = 0; NewsIndex < NewsItems.Length; NewsIndex++)
	{
		if (NewsItems[NewsIndex].NewsType == NewsType)
		{
			return NewsItems[NewsIndex].NewsItem;
		}
	}
	return "";
}

defaultproperties
{
}