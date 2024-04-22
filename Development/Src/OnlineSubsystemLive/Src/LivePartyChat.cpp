/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemLive.h"

#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(ULivePartyChat)

#if CONSOLE
	#pragma comment(lib,"xparty.lib")
#endif

#if STATS
/** Stats for tracking voice and other Xe specific data */
enum EXePartyChatNetStats
{
//	STAT_PercentInOverhead = STAT_PercentOutVoice + 1,
//	STAT_PercentOutOverhead,
	// Third stat after voice out
	STAT_PartyChatBytesLastSecond = STAT_PercentOutVoice + 3,
	STAT_PartyChatBytesTotal,
};

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Party Chat Bytes Last Second"),STAT_PartyChatBytesLastSecond,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Party Chat Bytes Total"),STAT_PartyChatBytesTotal,STATGROUP_Net);
#endif


/**
 * Allows per frame work to be done
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void ULivePartyChat::Tick(FLOAT DeltaTime)
{
	// Process any pending events
	TickPartyChatNotifications(DeltaTime);
	// Update stats
	TickBandwidthTracking(DeltaTime);
	// Now tick any outstanding async tasks
	TickAsyncTasks(DeltaTime,AsyncTasks,NULL,this);
}

/**
 * Updates the bandwidth tracking information
 *
 * @param DeltaTime the amount of time since we last updated
 */
void ULivePartyChat::TickBandwidthTracking(FLOAT DeltaTime)
{
#if CONSOLE
	ElapsedTime += DeltaTime;
	if (ElapsedTime > 1.f)
	{
		ElapsedTime = 0.f;
		QWORD QwordBytes = 0;
		// Read the current total and use that to determine the delta
		HRESULT hr = XPartyGetBandwidth(XPARTY_BANDWIDTH_UPLOAD_TOTAL_BYTES,&QwordBytes);
		if (SUCCEEDED(hr))
		{
			BandwidthUsed = (INT)(QwordBytes - TotalBandwidthUsed);
			// Update the total so we have a base line for the next time we update
			TotalBandwidthUsed = QwordBytes;
#if STATS
			SET_DWORD_STAT(STAT_PartyChatBytesLastSecond,BandwidthUsed);
			SET_DWORD_STAT(STAT_PartyChatBytesTotal,TotalBandwidthUsed);
#endif
		}
		else
		{
			BandwidthUsed = -1;
		}
	}
#endif
}

/**
 * Processes any party chat notifications that have happened since last tick
 *
 * @param DeltaTime the amount of time since the last update
 */
void ULivePartyChat::TickPartyChatNotifications(FLOAT DeltaTime)
{
#if CONSOLE
	if (NotificationHandle != NULL)
	{
		DWORD Notification = 0;
		ULONG_PTR Data = NULL;
		// Check Live for notification events
		while (XNotifyGetNext(NotificationHandle,0,&Notification,&Data))
		{
			// Now process the event
			ProcessPartyChatNotification(Notification,Data);
		}
	}
	else
	{
		// Listen only for party events
		NotificationHandle = XNotifyCreateListener(XNOTIFY_PARTY);
	}
#endif
}

/**
 * Processes a chat notification that was returned during polling
 *
 * @param Notification the notification event that was fired
 * @param Data the notification specific data
 */
void ULivePartyChat::ProcessPartyChatNotification(DWORD Notification,ULONG_PTR Data)
{
#if CONSOLE
	// Make sure we only are getting party chat events
	if (Notification == XN_PARTY_MEMBERS_CHANGED)
	{
		XPARTY_USER_LIST PartyList;
		appMemzero(&PartyList,sizeof(XPARTY_USER_LIST));
		TArray<FOnlinePartyMember> PartyMembers;
		// Read the list of members of the party
		HRESULT hr = XPartyGetUserList(&PartyList);
		debugf(NAME_DevOnline,TEXT("XPartyGetUserList() returned 0x%08X"),hr);
		if (SUCCEEDED(hr) && PartyList.dwUserCount > 0)
		{
			// Create a party member for each returned person
			PartyMembers.Empty(PartyList.dwUserCount);
			PartyMembers.AddZeroed(PartyList.dwUserCount);
			// Now iterate the returned players converting to our version of the struct
			for (DWORD Index = 0; Index < PartyList.dwUserCount; Index++)
			{
				FOnlinePartyMember& PartyMember = PartyMembers(Index);
				// Copy over the user data
				PartyMember.UniqueId.Uid = PartyList.Users[Index].Xuid;
				PartyMember.NickName = PartyList.Users[Index].GamerTag;
				PartyMember.LocalUserNum = PartyList.Users[Index].dwUserIndex;
				PartyMember.NatType = PartyList.Users[Index].NatType;
				PartyMember.TitleId = PartyList.Users[Index].dwTitleId;
				PartyMember.bIsPlayingThisGame = PartyList.Users[Index].dwTitleId == appGetTitleId();
				// Map their party flags to our bools
				PartyMember.bIsLocal = PartyList.Users[Index].dwFlags & XPARTY_USER_ISLOCAL ? TRUE : FALSE;
				PartyMember.bIsInPartyVoice = PartyList.Users[Index].dwFlags & XPARTY_USER_ISINPARTYVOICE ? TRUE : FALSE;
				PartyMember.bIsTalking = PartyList.Users[Index].dwFlags & XPARTY_USER_ISTALKING ? TRUE : FALSE;
				PartyMember.bIsInGameSession = PartyList.Users[Index].dwFlags & XPARTY_USER_ISINGAMESESSION ? TRUE : FALSE;
				QWORD First = PartyList.Users[Index].CustomData.qwFirst;
				QWORD Second = PartyList.Users[Index].CustomData.qwSecond;
				// Split the custom data into network byte order values
				PartyMember.Data1 = ((First >> 56) & 0xFF) | ((First >> 48) & 0xFF) | ((First >> 40) & 0xFF) | ((First >> 32) & 0xFF);
				PartyMember.Data2 = ((First >> 24) & 0xFF) | ((First >> 16) & 0xFF) | ((First >> 8) & 0xFF) | (First & 0xFF);
				PartyMember.Data3 = ((Second >> 56) & 0xFF) | ((Second >> 48) & 0xFF) | ((Second >> 40) & 0xFF) | ((Second >> 32) & 0xFF);
				PartyMember.Data4 = ((Second >> 24) & 0xFF) | ((Second >> 16) & 0xFF) | ((Second >> 8) & 0xFF) | (Second & 0xFF);
				check(sizeof(XNKID) == sizeof(QWORD));
				PartyMember.SessionId = (QWORD&)PartyList.Users[Index].SessionInfo.sessionID;
			}
		}
		/** Used to track how things changed */
		struct FOnlinePartyMemberDelta
		{
			FString PlayerName;
			FUniqueNetId PlayerId;
			INT Data1;
			INT Data2;
			INT Data3;
			INT Data4;
			UBOOL bJoinedOrLeft;
			UBOOL bDataChanged;
		};
		TArray<FOnlinePartyMemberDelta> DeltaPartyMembers;
		// First search the new list for additions or changes
		for (INT Index = 0; Index < PartyMembers.Num(); Index++)
		{
			const FOnlinePartyMember& PartyMember = PartyMembers(Index);
			UBOOL bWasFound = FALSE;
			UBOOL bDataChanged = FALSE;
			// Try to find this person in the cached list
			for (INT CachedIndex = 0; CachedIndex < CachedPartyMembers.Num(); CachedIndex++)
			{
				const FOnlinePartyMember& CachedMember = CachedPartyMembers(CachedIndex);
				// Compare xuids and if they match check to see if the data was updated
				if (CachedMember.UniqueId == PartyMember.UniqueId)
				{
					bWasFound = TRUE;
					bDataChanged = CachedMember.Data1 != PartyMember.Data1 || CachedMember.Data2 != PartyMember.Data2;
					break;
				}
			}
			// If they weren't found, they are new
			if (bWasFound == FALSE || bDataChanged == TRUE)
			{
				INT AddIndex = DeltaPartyMembers.AddZeroed();
				DeltaPartyMembers(AddIndex).PlayerName = PartyMember.NickName;
				DeltaPartyMembers(AddIndex).PlayerId = PartyMember.UniqueId;
				DeltaPartyMembers(AddIndex).bJoinedOrLeft = bWasFound == FALSE;
				DeltaPartyMembers(AddIndex).bDataChanged = bDataChanged;
				DeltaPartyMembers(AddIndex).Data1 = PartyMember.Data1;
				DeltaPartyMembers(AddIndex).Data2 = PartyMember.Data2;
				DeltaPartyMembers(AddIndex).Data3 = PartyMember.Data3;
				DeltaPartyMembers(AddIndex).Data4 = PartyMember.Data4;
			}
		}
		// Now search the other way for people that left
		for (INT CachedIndex = 0; CachedIndex < CachedPartyMembers.Num(); CachedIndex++)
		{
			const FOnlinePartyMember& CachedMember = CachedPartyMembers(CachedIndex);
			UBOOL bWasFound = FALSE;
			// Try to find this person in the current list
			for (INT Index = 0; Index < PartyMembers.Num(); Index++)
			{
				const FOnlinePartyMember& PartyMember = PartyMembers(Index);
				// Compare xuids and if they match check to see if the data was updated
				if (CachedMember.UniqueId == PartyMember.UniqueId)
				{
					bWasFound = TRUE;
					break;
				}
			}
			// If they weren't found, they left recently
			if (bWasFound == FALSE)
			{
				INT AddIndex = DeltaPartyMembers.AddZeroed();
				DeltaPartyMembers(AddIndex).PlayerName = CachedMember.NickName;
				DeltaPartyMembers(AddIndex).PlayerId = CachedMember.UniqueId;
				DeltaPartyMembers(AddIndex).bJoinedOrLeft = FALSE;
			}
		}
		// Store the current data as our cached set
		CachedPartyMembers = PartyMembers;
		// Now send out any notifications
		for (INT Index = 0; Index < DeltaPartyMembers.Num(); Index++)
		{
			const FOnlinePartyMemberDelta& DeltaMember = DeltaPartyMembers(Index);
			// If data didn't change the player either joined or left
			if (DeltaMember.bDataChanged == FALSE)
			{
				LivePartyChat_eventOnPartyMemberListChanged_Parms Parms(EC_EventParm);
				// Copy over the data to pass to script
				Parms.bJoinedOrLeft = DeltaMember.bJoinedOrLeft ? FIRST_BITFIELD : 0;
				Parms.PlayerID = DeltaMember.PlayerId;
				Parms.PlayerName = DeltaMember.PlayerName;
				// Now trigger the delegates for all players
				for (INT PlayerIndex = 0; PlayerIndex < MAX_LOCAL_PLAYERS; PlayerIndex++)
				{
					// Skip if there are no delegates registered
					if (PartyChatDelegates[PlayerIndex].PartyMemberDelegates.Num())
					{
						TriggerOnlineDelegates(this,PartyChatDelegates[PlayerIndex].PartyMemberDelegates,&Parms);
					}
				}
			}
			else
			{
				LivePartyChat_eventOnPartyMembersInfoChanged_Parms Parms(EC_EventParm);
				// Copy over the data to pass to script
				Parms.PlayerName = DeltaMember.PlayerName;
				Parms.PlayerID = DeltaMember.PlayerId;
				Parms.CustomData1 = DeltaMember.Data1;
				Parms.CustomData2 = DeltaMember.Data2;
				Parms.CustomData3 = DeltaMember.Data3;
				Parms.CustomData4 = DeltaMember.Data4;
				// Now trigger the delegates for all players
				for (INT PlayerIndex = 0; PlayerIndex < MAX_LOCAL_PLAYERS; PlayerIndex++)
				{
					// Skip if there are no delegates registered
					if (PartyChatDelegates[PlayerIndex].PartyMemberInfoDelegates.Num())
					{
						TriggerOnlineDelegates(this,PartyChatDelegates[PlayerIndex].PartyMemberInfoDelegates,&Parms);
					}
				}
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("LivePartyChat received a non-party chat event notification (%d)"),
			Notification);
	}
#endif
}

/**
 * Sends an invite to everyone in the existing party session
 *
 * @param LocalUserNum the user to sending the invites
 *
 * @return true if it was able to send them, false otherwise
 */
UBOOL ULivePartyChat::SendPartyGameInvites(BYTE LocalUserNum)
{
#if CONSOLE
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		FOnlineAsyncTaskLive* AsyncTask = new FOnlineAsyncTaskLive(&PartyChatDelegates[LocalUserNum].GameInviteDelegates,
			NULL,
			TEXT("XPartySendGameInvites"));
		// Send the invites asynchronously
		DWORD Result = XPartySendGameInvites((DWORD)LocalUserNum,*AsyncTask);
		debugf(NAME_DevOnline,TEXT("XPartySendGameInvites(%d) return 0x%08X"),LocalUserNum,Result);
		// Queue the async task for ticking
		if (Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING)
		{
			AsyncTasks.AddItem(AsyncTask);
		}
		else
		{
			delete AsyncTask;
		}
		return Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING;
	}
#endif
	return FALSE;
}

/**
 * Gets the party member information from the platform, including the application specific data
 *
 * @param PartyMembers the array to be filled out of party member information
 *
 * @return true if the call could populate the array, false otherwise
 */
UBOOL ULivePartyChat::GetPartyMembersInformation(TArray<FOnlinePartyMember>& PartyMembers)
{
#if CONSOLE
	// Copy our cached array
	PartyMembers = CachedPartyMembers;
	return TRUE;
#endif
	return FALSE;
}

/**
 * Gets the individual party member's information from the platform, including the application specific data
 *
 * @param MemberId the id of the party member to lookup
 * @param PartyMember out value where the data is copied to
 *
 * @return true if the call found the player, false otherwise
 */
UBOOL ULivePartyChat::GetPartyMemberInformation(FUniqueNetId MemberId,FOnlinePartyMember& PartyMember)
{
	HRESULT hr = E_FAIL;
	// Clear any previous results
	FOnlinePartyMember Empty(EC_EventParm);
	PartyMember = Empty;
#if CONSOLE
	// Loop through the list searching for this member
	for (INT Index = 0; Index < CachedPartyMembers.Num(); Index++)
	{
		if (MemberId == CachedPartyMembers(Index).UniqueId)
		{
			// Copy the party member's data
			PartyMember = CachedPartyMembers(Index);
			hr = S_OK;
			break;
		}
	}
#endif
	return SUCCEEDED(hr);
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
UBOOL ULivePartyChat::SetPartyMemberCustomData(BYTE LocalUserNum,INT Data1,INT Data2,INT Data3,INT Data4)
{
#if CONSOLE
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Copy the two QWORDs into the special struct
		XPARTY_CUSTOM_DATA Data;
		Data.qwFirst = ((QWORD)(Data1 & 0xFF000000) << 56) |
			((QWORD)(Data1 & 0x00FF0000) << 48) |
			((QWORD)(Data1 & 0x0000FF00) << 40) |
			((QWORD)(Data1 & 0x000000FF) << 32) |
			((QWORD)(Data2 & 0xFF000000) << 24) |
			((QWORD)(Data2 & 0x00FF0000) << 16) |
			((QWORD)(Data2 & 0x0000FF00) << 8) |
			(QWORD)(Data2 & 0x000000FF);
		Data.qwSecond = ((QWORD)(Data3 & 0xFF000000) << 56) |
			((QWORD)(Data3 & 0x00FF0000) << 48) |
			((QWORD)(Data3 & 0x0000FF00) << 40) |
			((QWORD)(Data3 & 0x000000FF) << 32) |
			((QWORD)(Data4 & 0xFF000000) << 24) |
			((QWORD)(Data4 & 0x00FF0000) << 16) |
			((QWORD)(Data4 & 0x0000FF00) << 8) |
			(QWORD)(Data4 & 0x000000FF);
		// Set the data for the user
		HRESULT hr = XPartySetCustomData((DWORD)LocalUserNum,&Data);
		debugf(NAME_DevOnline,TEXT("XPartySetCustomData(%d) return 0x%08X"),LocalUserNum,hr);
		return SUCCEEDED(hr);
	}
#endif
	return FALSE;
}

/**
 * Opens the party UI for the user
 *
 * @param LocalUserNum the user requesting the UI
 */
UBOOL ULivePartyChat::ShowPartyUI(BYTE LocalUserNum)
{
#if CONSOLE
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Open the requested UI
		HRESULT hr = XShowPartyUI((DWORD)LocalUserNum);
		debugf(NAME_DevOnline,TEXT("XShowPartyUI(%d) return 0x%08X"),LocalUserNum,hr);
		return SUCCEEDED(hr);
	}
#endif
	return FALSE;
}

/**
 * Opens the voice channel UI for the user
 *
 * @param LocalUserNum the user requesting the UI
 */
UBOOL ULivePartyChat::ShowVoiceChannelUI(BYTE LocalUserNum)
{
#if CONSOLE
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Open the requested UI
		HRESULT hr = XShowVoiceChannelUI((DWORD)LocalUserNum);
		debugf(NAME_DevOnline,TEXT("XShowVoiceChannelUI(%d) return 0x%08X"),LocalUserNum,hr);
		return SUCCEEDED(hr);
	}
#endif
	return FALSE;
}

/**
 * Opens the community sessions UI for the user
 *
 * @param LocalUserNum the user requesting the UI
 */
UBOOL ULivePartyChat::ShowCommunitySessionsUI(BYTE LocalUserNum)
{
#if CONSOLE
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Open the requested UI
		HRESULT hr = XShowCommunitySessionsUI((DWORD)LocalUserNum,XSHOWCOMMUNITYSESSION_SHOWPARTY);
		debugf(NAME_DevOnline,TEXT("XShowCommunitySessionsUI(%d) return 0x%08X"),LocalUserNum,hr);
		return SUCCEEDED(hr);
	}
#endif
	return FALSE;
}

/**
 * Checks for the specified player being in a party chat
 *
 * @param LocalUserNum the user that you are setting the data for
 *
 * @return true if there is a party chat in progress, false otherwise
 */
UBOOL ULivePartyChat::IsInPartyChat(BYTE LocalUserNum)
{
#if CONSOLE
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		XUID Xuid = 0;
		// Get this player's xuid so we can search for them
		if (GetUserXuid(LocalUserNum,&Xuid) == ERROR_SUCCESS)
		{
			// Search the array for the player
			for (INT Index = 0; Index < CachedPartyMembers.Num(); Index++)
			{
				if (Xuid == CachedPartyMembers(Index).UniqueId.Uid)
				{
					return TRUE;
				}
			}
		}
	}
#endif
	return FALSE;
}

#endif
