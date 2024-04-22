/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
/**
 * This class implements the live party chat interface
 */
class LivePartyChat extends Object within OnlineSubsystemLive
	native
	inherits(FTickableObject)
	implements(OnlinePartyChatInterface);

/** The notification handle to use for polling events */
var const native transient pointer NotificationHandle{void};

/** Holds an array of party chat delegates */
struct native PerUserPartyChatDelegates
{
	/** This is the list of requested delegates to fire */
	var array<delegate<OnSendPartyGameInvitesComplete> > GameInviteDelegates;
	/** This is the list of requested delegates to fire */
	var array<delegate<OnPartyMemberListChanged> > PartyMemberDelegates;
	/** This is the list of requested delegates to fire */
	var array<delegate<OnPartyMembersInfoChanged> > PartyMemberInfoDelegates;
};

/** Used for per player index notification of party game invite delegates */
var PerUserPartyChatDelegates PartyChatDelegates[4];

/** How much data has been used across all sessions */
var transient qword TotalBandwidthUsed;

/** The bandwidth used by party chat over the last second */
var transient int BandwidthUsed;

/** The amount of elapsed time since the last time we updated the bandwidth information */
var transient float ElapsedTime;

/**
 * This is the array of pending async tasks. Each tick these tasks are checked
 * completion. If complete, the delegate associated with them is called
 */
var native const array<pointer> AsyncTasks{FOnlineAsyncTaskLive};

/** Used to track changes to the membership list */
var array<OnlinePartyMember> CachedPartyMembers;

/**
 * Sends an invite to everyone in the existing party session
 *
 * @param LocalUserNum the user to sending the invites
 *
 * @return true if it was able to send them, false otherwise
 */
native function bool SendPartyGameInvites(byte LocalUserNum);

/**
 * Called when the async invite send has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnSendPartyGameInvitesComplete(bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the achievements read request has completed
 *
 * @param LocalUserNum the user to sending the invites
 * @param SendPartyGameInvitesCompleteDelegate the delegate to use for notifications
 */
function AddSendPartyGameInvitesCompleteDelegate(byte LocalUserNum,delegate<OnSendPartyGameInvitesComplete> SendPartyGameInvitesCompleteDelegate)
{
	if (LocalUserNum >= 0 && LocalUserNum < 4)
	{
		// Add this delegate to the array if not already present
		if (PartyChatDelegates[LocalUserNum].GameInviteDelegates.Find(SendPartyGameInvitesCompleteDelegate) == INDEX_NONE)
		{
			PartyChatDelegates[LocalUserNum].GameInviteDelegates.AddItem(SendPartyGameInvitesCompleteDelegate);
		}
	}
}

/**
 * Clears the delegate used to notify the gameplay code that the achievements read request has completed
 *
 * @param LocalUserNum the user to sending the invites
 * @param SendPartyGameInvitesCompleteDelegate the delegate to use for notifications
 */
function ClearSendPartyGameInvitesCompleteDelegate(byte LocalUserNum,delegate<OnSendPartyGameInvitesComplete> SendPartyGameInvitesCompleteDelegate)
{
	local int RemoveIndex;

	if (LocalUserNum >= 0 && LocalUserNum < 4)
	{
		// Remove this delegate from the array if found
		RemoveIndex = PartyChatDelegates[LocalUserNum].GameInviteDelegates.Find(SendPartyGameInvitesCompleteDelegate);
		if (RemoveIndex != INDEX_NONE)
		{
			PartyChatDelegates[LocalUserNum].GameInviteDelegates.Remove(RemoveIndex,1);
		}
	}
}

/**
 * Gets the party member information from the platform, including the application specific data
 *
 * @param PartyMembers the array to be filled out of party member information
 *
 * @return true if the call could populate the array, false otherwise
 */
native function bool GetPartyMembersInformation(out array<OnlinePartyMember> PartyMembers);

/**
 * Gets the individual party member's information from the platform, including the application specific data
 *
 * @param MemberId the id of the party member to lookup
 * @param PartyMember out value where the data is copied to
 *
 * @return true if the call found the player, false otherwise
 */
native function bool GetPartyMemberInformation(UniqueNetId MemberId,out OnlinePartyMember PartyMember);

/**
 * Called when a player has joined or left your party chat
 *
 * @param bJoinedOrLeft true if the player joined, false if they left
 * @param PlayerName the name of the player that was affected
 * @param PlayerId the net id of the player that left
 */
delegate OnPartyMemberListChanged(bool bJoinedOrLeft,string PlayerName,UniqueNetId PlayerId);

/**
 * Sets the delegate used to notify the gameplay code that async task has completed
 *
 * @param LocalUserNum the user to listening for party chat notifications
 * @param PartyMemberListChangedDelegate the delegate to use for notifications
 */
function AddPartyMemberListChangedDelegate(byte LocalUserNum,delegate<OnPartyMemberListChanged> PartyMemberListChangedDelegate)
{
	if (LocalUserNum >= 0 && LocalUserNum < 4)
	{
		// Add this delegate to the array if not already present
		if (PartyChatDelegates[LocalUserNum].PartyMemberDelegates.Find(PartyMemberListChangedDelegate) == INDEX_NONE)
		{
			PartyChatDelegates[LocalUserNum].PartyMemberDelegates.AddItem(PartyMemberListChangedDelegate);
		}
	}
}

/**
 * Clears the delegate used to notify the gameplay code that async task has completed
 *
 * @param LocalUserNum the user to listening for party chat notifications
 * @param PartyMemberListChangedDelegate the delegate to use for notifications
 */
function ClearPartyMemberListChangedDelegate(byte LocalUserNum,delegate<OnPartyMemberListChanged> PartyMemberListChangedDelegate)
{
	local int RemoveIndex;

	if (LocalUserNum >= 0 && LocalUserNum < 4)
	{
		// Remove this delegate from the array if found
		RemoveIndex = PartyChatDelegates[LocalUserNum].PartyMemberDelegates.Find(PartyMemberListChangedDelegate);
		if (RemoveIndex != INDEX_NONE)
		{
			PartyChatDelegates[LocalUserNum].PartyMemberDelegates.Remove(RemoveIndex,1);
		}
	}
}

/**
 * Called when a player has joined or left your party chat
 *
 * @param PlayerName the name of the player that was affected
 * @param PlayerId the net id of the player that left
 * @param CustomData1 the first 4 bytes of the custom data
 * @param CustomData2 the second 4 bytes of the custom data
 * @param CustomData3 the third 4 bytes of the custom data
 * @param CustomData4 the fourth 4 bytes of the custom data
 */
delegate OnPartyMembersInfoChanged(string PlayerName,UniqueNetId PlayerId,int CustomData1,int CustomData2,int CustomData3,int CustomData4);

/**
 * Sets the delegate used to notify the gameplay code that async task has completed
 *
 * @param LocalUserNum the user to listening for party chat notifications
 * @param PartyMembersInfoChangedDelegate the delegate to use for notifications
 */
function AddPartyMembersInfoChangedDelegate(byte LocalUserNum,delegate<OnPartyMembersInfoChanged> PartyMembersInfoChangedDelegate)
{
	if (LocalUserNum >= 0 && LocalUserNum < 4)
	{
		// Add this delegate to the array if not already present
		if (PartyChatDelegates[LocalUserNum].PartyMemberInfoDelegates.Find(PartyMembersInfoChangedDelegate) == INDEX_NONE)
		{
			PartyChatDelegates[LocalUserNum].PartyMemberInfoDelegates.AddItem(PartyMembersInfoChangedDelegate);
		}
	}
}

/**
 * Clears the delegate used to notify the gameplay code that async task has completed
 *
 * @param LocalUserNum the user to listening for party chat notifications
 * @param PartyMembersInfoChangedDelegate the delegate to use for notifications
 */
function ClearPartyMembersInfoChangedDelegate(byte LocalUserNum,delegate<OnPartyMembersInfoChanged> PartyMembersInfoChangedDelegate)
{
	local int RemoveIndex;

	if (LocalUserNum >= 0 && LocalUserNum < 4)
	{
		// Remove this delegate from the array if found
		RemoveIndex = PartyChatDelegates[LocalUserNum].PartyMemberInfoDelegates.Find(PartyMembersInfoChangedDelegate);
		if (RemoveIndex != INDEX_NONE)
		{
			PartyChatDelegates[LocalUserNum].PartyMemberInfoDelegates.Remove(RemoveIndex,1);
		}
	}
}

/**
 * Sets a party member's application specific data
 *
 * @param LocalUserNum the user that you are setting the data for
 * @param Data1 the first 4 bytes of custom data
 * @param Data2 the second 4 bytes of custom data
 * @param Data3 the third 4 bytes of custom data
 * @param Data4 the fourth 4 bytes of custom data
 *
 * @return true if the data could be set, false otherwise
 */
native function bool SetPartyMemberCustomData(byte LocalUserNum,int Data1,int Data2,int Data3,int Data4);

/**
 * Determines the amount of data that has been sent in the last second
 *
 * @return >= 0 if able to get the bandwidth used over the last second, < 0 upon an error
 */
function int GetPartyBandwidth()
{
	return BandwidthUsed;
}

/**
 * Opens the party UI for the user
 *
 * @param LocalUserNum the user requesting the UI
 */
native function bool ShowPartyUI(byte LocalUserNum);

/**
 * Opens the voice channel UI for the user
 *
 * @param LocalUserNum the user requesting the UI
 */
native function bool ShowVoiceChannelUI(byte LocalUserNum);

/**
 * Opens the community sessions UI for the user
 *
 * @param LocalUserNum the user requesting the UI
 */
native function bool ShowCommunitySessionsUI(byte LocalUserNum);


/**
 * Checks for the specified player being in a party chat
 *
 * @param LocalUserNum the user that you are setting the data for
 *
 * @return true if there is a party chat in progress, false otherwise
 */
native function bool IsInPartyChat(byte LocalUserNum);

cpptext
{
// FTickableObject interface

	/**
	 * Returns whether it is okay to tick this object. E.g. objects being loaded in the background shouldn't be ticked
	 * till they are finalized and unreachable objects cannot be ticked either.
	 *
	 * @return	TRUE if tickable, FALSE otherwise
	 */
	virtual UBOOL IsTickable() const
	{
		// We cannot tick objects that are unreachable or are in the process of being loaded in the background.
		return !HasAnyFlags( RF_Unreachable | RF_AsyncLoading );
	}

	/**
	 * Used to determine if an object should be ticked when the game is paused.
	 *
	 * @return always TRUE as networking needs to be ticked even when paused
	 */
	virtual UBOOL IsTickableWhenPaused() const
	{
		return TRUE;
	}

	/**
	 * Allows per frame work to be done
	 *
	 * @param DeltaTime the amount of time that has passed since the last tick
	 */
	virtual void Tick(FLOAT);

	/**
	 * Updates the bandwidth tracking information
	 *
	 * @param DeltaTime the amount of time since the last update
	 */
	void TickBandwidthTracking(FLOAT DeltaTime);

	/**
	 * Processes any party chat notifications that have happened since last tick
	 *
	 * @param DeltaTime the amount of time since the last update
	 */
	void TickPartyChatNotifications(FLOAT DeltaTime);

	/**
	 * Processes a chat notification that was returned during polling
	 *
	 * @param Notification the notification event that was fired
	 * @param Data the notification specific data
	 */
	void ProcessPartyChatNotification(DWORD Notification,ULONG_PTR Data);
}
