/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This object is responsible for the enumeration of downloadable content bundles on Xbox Live
 */
class DownloadableContentEnumeratorLive extends DownloadableContentEnumerator
	native;

/** The number of reads that are outstanding */
var transient int ReadsOutstanding;

/**
 * Uses the OnlineContentInterface to populate the DLC data for all signed in users
 */
function FindDLC()
{
	local OnlineSubsystem OnlineSub;
	local OnlineContentInterface ContentInt;
	local OnlinePlayerInterface PlayerInt;
	local int PlayerIndex;

	// Skip doing anything if we have already started the reads
	if (ReadsOutstanding == 0)
	{
		DLCBundles.Length = 0;
		OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
		if (OnlineSub != None)
		{
			PlayerInt = OnlineSub.PlayerInterface;
			if (PlayerInt != None)
			{
				ContentInt = OnlineSub.ContentInterface;
				if (ContentInt != None)
				{
					// Check for each signed in user and kick off the read request
					for (PlayerIndex = 0; PlayerIndex < 4; PlayerIndex++)
					{
						if (PlayerInt.GetLoginStatus(PlayerIndex) > LS_NotLoggedIn &&
							!PlayerInt.IsGuestLogin(PlayerIndex))
						{
							// Add our read delegates
							ContentInt.AddReadContentComplete(PlayerIndex,OCT_Downloaded,OnReadContentComplete);
							// Start the read
							ReadsOutstanding += int(ContentInt.ReadContentList(PlayerIndex,OCT_Downloaded));
						}
						else
						{
							// Not logged in so clear any stale data
							ContentInt.ClearContentList(PlayerIndex,OCT_Downloaded);
						}
					}
				}
			}
		}
	}
	// If we are still at zero, then we are complete so fire the delegate
	if (ReadsOutstanding == 0)
	{
		TriggerFindDLCDelegates();
	}
}

/**
 * Called when an async read of content has completed
 *
 * @param bWasSuccessful whether the read worked or not
 */
function OnReadContentComplete(bool bWasSuccessful)
{
	local OnlineSubsystem OnlineSub;
	local OnlineContentInterface ContentInt;
	local int PlayerIndex;
	local array<OnlineContent> UserBundles;

	ReadsOutstanding--;
	// If there are no more outstanding, then build the DLC list from each player
	if (ReadsOutstanding == 0)
	{
		OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
		if (OnlineSub != None)
		{
			ContentInt = OnlineSub.ContentInterface;
			if (ContentInt != None)
			{
				// For each user, get their DLC, and append to our list
				for (PlayerIndex = 0; PlayerIndex < 4; PlayerIndex++)
				{
					UserBundles.Length = 0;
					// Append their DLC bundles to our array
					ContentInt.GetContentList(PlayerIndex,OCT_Downloaded,UserBundles);
					// Skip the call if the array is empty
					if (UserBundles.Length > 0)
					{
						AppendDLC(UserBundles);
					}
				}
			}
		}
		// Trigger the complete delegates
		TriggerFindDLCDelegates();
	}
}

/**
 * Appends the specified array to the DLCBundles array
 *
 * @param Bundles the array to append
 */
native function AppendDLC(const out array<OnlineContent> Bundles);

/**
 * Can't work, so ignore the call
 *
 * @param DLCName the name of the DLC bundle to delete
 */
function DeleteDLC(string DLCName)
{
	// Purposefully empty and doesn't call super
}
