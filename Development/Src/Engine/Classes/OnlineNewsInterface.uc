/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This interface provides functions for reading game specific news and announcements
 */
interface OnlineNewsInterface dependson(OnlineSubsystem);

/**
 * Reads the game specific news from the online subsystem
 *
 * @param LocalUserNum the local user the news is being read for
 * @param NewsType the type of news to read
 *
 * @return true if the async task was successfully started, false otherwise
 */
function bool ReadNews(byte LocalUserNum,EOnlineNewsType NewsType);

/**
 * Delegate used in notifying the UI/game that the news read operation completed
 *
 * @param bWasSuccessful true if the read completed ok, false otherwise
 * @param NewsType the type of news this callback is for
 */
delegate OnReadNewsCompleted(bool bWasSuccessful,EOnlineNewsType NewsType);

/**
 * Sets the delegate used to notify the gameplay code that news reading has completed
 *
 * @param ReadNewsDelegate the delegate to use for notifications
 */
function AddReadNewsCompletedDelegate(delegate<OnReadNewsCompleted> ReadNewsDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param ReadNewsDelegate the delegate to use for notifications
 */
function ClearReadNewsCompletedDelegate(delegate<OnReadNewsCompleted> ReadNewsDelegate);

/**
 * Returns the game specific news from the cache
 *
 * @param LocalUserNum the local user the news is being read for
 * @param NewsType the type of news to read
 *
 * @return an empty string if no news was read, otherwise the contents of the read
 */
function string GetNews(byte LocalUserNum,EOnlineNewsType NewsType);
