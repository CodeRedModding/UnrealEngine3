/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemLive.h"
#include "VoiceInterfaceCommon.h"

#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(UOnlineEventsInterfaceMcpLive);

#if WITH_PANORAMA
	#pragma message("Linking Games for Windows Live")
	//NOTE: If you get an error here, make sure the G4WLive directories are in your additional includes/libs
	#pragma comment(lib, "XLive.lib")
#endif

// Change this value anytime the QoS packet format changes
#define QOS_PACKET_VERSION (BYTE)3

/** Static buffer to use when writing stats */
static XSESSION_VIEW_PROPERTIES Views[MAX_VIEWS];
/** Static buffer to use when writing stats */
static XUSER_PROPERTY Stats[MAX_STATS];

IMPLEMENT_CLASS(UOnlineSubsystemLive);

/**
 * Converts the gamestate into a human readable string
 *
 * @param GameState the game state to convert to a string
 *
 * @return a string representation of the game state
 */
inline FString GetOnlineGameStateString(BYTE GameState)
{
#if !FINAL_RELEASE && !SHIPPING_PC_GAME
	if (GameState < OGS_MAX)
	{
		UEnum* OnlineGameStateEnum = FindObject<UEnum>(ANY_PACKAGE,TEXT("EOnlineGameState"),TRUE);
		if (OnlineGameStateEnum != NULL)
		{
			return OnlineGameStateEnum->GetEnum(GameState).ToString();
		}
	}
#endif
	return TEXT("Unknown");
}

/**
 * Logs session properties used from the game settings
 *
 * @param GameSettings the game to log the information for
 */
static void DumpGameSettings(const UOnlineGameSettings* GameSettings)
{
	if (GameSettings != NULL)
	{
		debugf(NAME_ScriptLog,TEXT("dumping OnlineGameSettings: "));
		debugf(NAME_ScriptLog,TEXT("	OwningPlayerName: %s"),*GameSettings->OwningPlayerName);	
		debugf(NAME_ScriptLog,TEXT("	OwningPlayerId: 0x%016I64X"),(QWORD&)GameSettings->OwningPlayerId.Uid);
		debugf(NAME_ScriptLog,TEXT("	PingInMs: %d"),GameSettings->PingInMs);
		debugf(NAME_ScriptLog,TEXT("	NumPublicConnections: %d"),GameSettings->NumPublicConnections);
		debugf(NAME_ScriptLog,TEXT("	NumOpenPublicConnections: %d"),GameSettings->NumOpenPublicConnections);
		debugf(NAME_ScriptLog,TEXT("	NumPrivateConnections: %d"),GameSettings->NumPrivateConnections);
		debugf(NAME_ScriptLog,TEXT("	NumOpenPrivateConnections: %d"),GameSettings->NumOpenPrivateConnections);
		debugf(NAME_ScriptLog,TEXT("	bIsLanMatch: %s"),GameSettings->bIsLanMatch ? TEXT("true") : TEXT("false"));
		debugf(NAME_ScriptLog,TEXT("	bIsDedicated: %s"),GameSettings->bIsDedicated ? TEXT("true") : TEXT("false"));
		debugf(NAME_ScriptLog,TEXT("	bUsesStats: %s"),GameSettings->bUsesStats ? TEXT("true") : TEXT("false"));
		debugf(NAME_ScriptLog,TEXT("	bUsesArbitration: %s"),GameSettings->bUsesArbitration ? TEXT("true") : TEXT("false"));
		debugf(NAME_ScriptLog,TEXT("	bShouldAdvertise: %s"),GameSettings->bShouldAdvertise ? TEXT("true") : TEXT("false"));
		debugf(NAME_ScriptLog,TEXT("	bAllowJoinInProgress: %s"),GameSettings->bAllowJoinInProgress ? TEXT("true") : TEXT("false"));
		debugf(NAME_ScriptLog,TEXT("	bAllowInvites: %s"),GameSettings->bAllowInvites ? TEXT("true") : TEXT("false"));
		debugf(NAME_ScriptLog,TEXT("	bUsesPresence: %s"),GameSettings->bUsesPresence ? TEXT("true") : TEXT("false"));
		debugf(NAME_ScriptLog,TEXT("	bWasFromInvite: %s"),GameSettings->bWasFromInvite ? TEXT("true") : TEXT("false"));
		debugf(NAME_ScriptLog,TEXT("	bAllowJoinViaPresence: %s"),GameSettings->bAllowJoinViaPresence ? TEXT("true") : TEXT("false"));
		debugf(NAME_ScriptLog,TEXT("	bAllowJoinViaPresenceFriendsOnly: %s"),GameSettings->bAllowJoinViaPresenceFriendsOnly ? TEXT("true") : TEXT("false"));
		debugf(NAME_ScriptLog,TEXT("	GameState: %d"),GameSettings->GameState);
	}
}

/**
 * Logs the set of contexts and properties for debugging purposes
 *
 * @param GameSettings the game to log the information for
 */
static void DumpContextsAndProperties(USettings* GameSettings)
{
	debugf(NAME_DevOnline,TEXT("dumping contexts:"));
	// Iterate through all contexts and log them
	for (INT Index = 0; Index < GameSettings->LocalizedSettings.Num(); Index++)
	{
		const FLocalizedStringSetting& Context = GameSettings->LocalizedSettings(Index);
		// Check for wildcard status
		if (GameSettings->IsWildcardStringSetting(Context.Id) == FALSE)
		{
			debugf(NAME_DevOnline,TEXT("	0x%08X (%s) = %d"),
				Context.Id,*GameSettings->GetStringSettingName(Context.Id).ToString(),Context.ValueIndex);
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("	0x%08X (%s) = wildcard"),
				Context.Id,*GameSettings->GetStringSettingName(Context.Id).ToString());
		}
	}
	debugf(NAME_DevOnline,TEXT("dumping properties:"));
	// Iterate through all properties and log them
	for (INT Index = 0; Index < GameSettings->Properties.Num(); Index++)
	{
		const FSettingsProperty& Property = GameSettings->Properties(Index);
		debugf(NAME_DevOnline,TEXT("	0x%08X (%d) (%s) = %s"),
			Property.PropertyId,
			Property.Data.Type,
			*GameSettings->GetPropertyName(Property.PropertyId).ToString(),
			*Property.Data.ToString());
	}
}

/**
 * Given an SettingsData object determines the size we'll be sending
 *
 * @param SettingsData the object to inspect
 */
static inline DWORD GetSettingsDataSize(const FSettingsData& SettingsData)
{
	DWORD SizeOfData = sizeof(DWORD);
	// Figure out if we have a type that isn't the default size
	switch (SettingsData.Type)
	{
		case SDT_Int64:
		case SDT_Double:
		{
			SizeOfData = sizeof(DOUBLE);
			break;
		}
		case SDT_Blob:
		{
			// Read the setting that is set by Value1
			SizeOfData = (DWORD)SettingsData.Value1;
			break;
		}
		case SDT_String:
		{
			// The null terminator needs to be counted too
			SizeOfData = ((DWORD)SettingsData.Value1 + 1) * sizeof(TCHAR);
			break;
		}
	}
	return SizeOfData;
}

/**
 * Given an SettingsData object determines the data pointer to give to Live
 *
 * @param SettingsData the object to inspect
 */
static inline const void* GetSettingsDataPointer(const FSettingsData& SettingsData)
{
	const void* DataPointer = NULL;
	// Determine where to get the pointer from
	switch (SettingsData.Type)
	{
		case SDT_Float:
		case SDT_Int32:
		case SDT_Int64:
		case SDT_Double:
		{
			DataPointer = &SettingsData.Value1;
			break;
		}
		case SDT_Blob:
		case SDT_String:
		{
			DataPointer = SettingsData.Value2;
			break;
		}
	}
	return DataPointer;
}

/**
 * Copies the data from the Live structure to the Epic structure
 *
 * @param Data the Epic structure that is the destination
 * @param XData the Live strucuture that is the source
 */
static inline void CopyXDataToSettingsData(FSettingsData& Data,const XUSER_DATA& XData)
{
	// Copy based upon data type
	switch (XData.type)
	{
		case XUSER_DATA_TYPE_FLOAT:
		{
			Data.SetData(XData.fData);
			break;
		}
		case XUSER_DATA_TYPE_INT32:
		{
			Data.SetData((INT)XData.nData);
			break;
		}
		case XUSER_DATA_TYPE_INT64:
		{
			Data.SetData((QWORD)XData.i64Data);
			break;
		}
		case XUSER_DATA_TYPE_DOUBLE:
		{
			Data.SetData(XData.dblData);
			break;
		}
		case XUSER_DATA_TYPE_BINARY:
		{
			// Deep copy the data
			Data.SetData(XData.binary.cbData,XData.binary.pbData);
			break;
		}
		case XUSER_DATA_TYPE_UNICODE:
		{
			Data.SetData(XData.string.pwszData);
			break;
		}
		case XUSER_DATA_TYPE_DATETIME:
		{
			Data.SetData(XData.ftData.dwLowDateTime,XData.ftData.dwHighDateTime);
			break;
		}
	}
}

/** Global mapping of Unreal enum values to Live values */
const DWORD LiveProfileSettingIDs[] =
{
	// Live read only settings
	XPROFILE_OPTION_CONTROLLER_VIBRATION,
	XPROFILE_GAMER_YAXIS_INVERSION,
	XPROFILE_GAMERCARD_CRED,
	XPROFILE_GAMERCARD_REP,
	XPROFILE_OPTION_VOICE_MUTED,
	XPROFILE_OPTION_VOICE_THRU_SPEAKERS,
	XPROFILE_OPTION_VOICE_VOLUME,
	XPROFILE_GAMERCARD_PICTURE_KEY,
	XPROFILE_GAMERCARD_MOTTO,
	XPROFILE_GAMERCARD_TITLES_PLAYED,
	XPROFILE_GAMERCARD_ACHIEVEMENTS_EARNED,
	XPROFILE_GAMER_DIFFICULTY,
	XPROFILE_GAMER_CONTROL_SENSITIVITY,
	XPROFILE_GAMER_PREFERRED_COLOR_FIRST,
	XPROFILE_GAMER_PREFERRED_COLOR_SECOND,
	XPROFILE_GAMER_ACTION_AUTO_AIM,
	XPROFILE_GAMER_ACTION_AUTO_CENTER,
	XPROFILE_GAMER_ACTION_MOVEMENT_CONTROL,
	XPROFILE_GAMER_RACE_TRANSMISSION,
	XPROFILE_GAMER_RACE_CAMERA_LOCATION,
	XPROFILE_GAMER_RACE_BRAKE_CONTROL,
	XPROFILE_GAMER_RACE_ACCELERATOR_CONTROL,
	XPROFILE_GAMERCARD_TITLE_CRED_EARNED,
	XPROFILE_GAMERCARD_TITLE_ACHIEVEMENTS_EARNED
};

/**
 * Determines if the specified ID is outside the range of standard Live IDs
 *
 * @param Id the id in question
 *
 * @return TRUE of the ID is game specific, FALSE otherwise
 */
FORCEINLINE UBOOL IsProfileSettingIdGameOnly(DWORD Id)
{
	return Id >= PSI_EndLiveIds;
}

/**
 * Converts an EProfileSettingID enum to the Live equivalent
 *
 * @param EnumValue the value to convert to a Live value
 *
 * @return the Live specific value for the specified enum
 */
inline DWORD ConvertToLiveValue(EProfileSettingID EnumValue)
{
	check(ARRAY_COUNT(LiveProfileSettingIDs) == PSI_EndLiveIds - 1 &&
		"Live profile mapping array isn't in synch");
	check(EnumValue > PSI_Unknown && EnumValue < PSI_EndLiveIds);
	return LiveProfileSettingIDs[EnumValue - 1];
}

/**
 * Converts a Live profile id to an EProfileSettingID enum value
 *
 * @param LiveValue the Live specific value to convert
 *
 * @return the Unreal enum value representing that Live profile id
 */
EProfileSettingID ConvertFromLiveValue(DWORD LiveValue)
{
	BYTE EnumValue = PSI_Unknown;
	// Figure out which enum value to use
	switch (LiveValue)
	{
		case XPROFILE_OPTION_CONTROLLER_VIBRATION:
		{
			EnumValue = 1;
			break;
		}
		case XPROFILE_GAMER_YAXIS_INVERSION:
		{
			EnumValue = 2;
			break;
		}
		case XPROFILE_GAMERCARD_CRED:
		{
			EnumValue = 3;
			break;
		}
		case XPROFILE_GAMERCARD_REP:
		{
			EnumValue = 4;
			break;
		}
		case XPROFILE_OPTION_VOICE_MUTED:
		{
			EnumValue = 5;
			break;
		}
		case XPROFILE_OPTION_VOICE_THRU_SPEAKERS:
		{
			EnumValue = 6;
			break;
		}
		case XPROFILE_OPTION_VOICE_VOLUME:
		{
			EnumValue = 7;
			break;
		}
		case XPROFILE_GAMERCARD_PICTURE_KEY:
		{
			EnumValue = 8;
			break;
		}
		case XPROFILE_GAMERCARD_TITLES_PLAYED:
		{
			EnumValue = 9;
			break;
		}
		case XPROFILE_GAMERCARD_MOTTO:
		{
			EnumValue = 10;
			break;
		}
		case XPROFILE_GAMERCARD_ACHIEVEMENTS_EARNED:
		{
			EnumValue = 11;
			break;
		}
		case XPROFILE_GAMER_DIFFICULTY:
		{
			EnumValue = 12;
			break;
		}
		case XPROFILE_GAMER_CONTROL_SENSITIVITY:
		{
			EnumValue = 13;
			break;
		}
		case XPROFILE_GAMER_PREFERRED_COLOR_FIRST:
		{
			EnumValue = 14;
			break;
		}
		case XPROFILE_GAMER_PREFERRED_COLOR_SECOND:
		{
			EnumValue = 15;
			break;
		}
		case XPROFILE_GAMER_ACTION_AUTO_AIM:
		{
			EnumValue = 16;
			break;
		}
		case XPROFILE_GAMER_ACTION_AUTO_CENTER:
		{
			EnumValue = 17;
			break;
		}
		case XPROFILE_GAMER_ACTION_MOVEMENT_CONTROL:
		{
			EnumValue = 18;
			break;
		}
		case XPROFILE_GAMER_RACE_TRANSMISSION:
		{
			EnumValue = 19;
			break;
		}
		case XPROFILE_GAMER_RACE_CAMERA_LOCATION:
		{
			EnumValue = 20;
			break;
		}
		case XPROFILE_GAMER_RACE_BRAKE_CONTROL:
		{
			EnumValue = 21;
			break;
		}
		case XPROFILE_GAMER_RACE_ACCELERATOR_CONTROL:
		{
			EnumValue = 22;
			break;
		}
		case XPROFILE_GAMERCARD_TITLE_CRED_EARNED:
		{
			EnumValue = 23;
			break;
		}
		case XPROFILE_GAMERCARD_TITLE_ACHIEVEMENTS_EARNED:
		{
			EnumValue = 24;
			break;
		}
	};
	return (EProfileSettingID)EnumValue;
}

/**
 * Converts the Unreal enum values into an array of Live values
 *
 * @param ProfileIds the Unreal values to convert
 * @param DestIds an out array that gets the converted data
 */
static inline void BuildLiveProfileReadIDs(const TArray<DWORD>& ProfileIds,
	DWORD* DestIds)
{
	// Loop through using the helper to convert
	for (INT Index = 0; Index < ProfileIds.Num(); Index++)
	{
		DestIds[Index] = ConvertToLiveValue((EProfileSettingID)ProfileIds(Index));
	}
}

#if !FINAL_RELEASE
/**
 * Validates that the specified write stats object has the proper number
 * of views and stats per view
 *
 * @param WriteStats the object to validate
 *
 * @return TRUE if acceptable, FALSE otherwise
 */
UBOOL IsValidStatsWrite(UOnlineStatsWrite* WriteStats)
{
	// Validate the number of views
	if (WriteStats->ViewIds.Num() > 0 && WriteStats->ViewIds.Num() <= 5)
	{
		return WriteStats->Properties.Num() >= 0 &&
			WriteStats->Properties.Num() <= 64;
	}
	return FALSE;
}
#endif

/**
 * Iterates the list of outstanding tasks checking for their completion
 * status. Upon completion, it fires off the corresponding delegate
 * notification
 *
 * @param DeltaTime the amount of elapsed time since the last tick
 * @param AsyncTasks the list of outstanding async tasks
 * @param LiveSubsystem the subsystem for processing results if needed
 * @param DefDelegateObj the default delegate object
 */
void TickAsyncTasks(FLOAT DeltaTime,TArray<FOnlineAsyncTaskLive*>& AsyncTasks,UOnlineSubsystemLive* LiveSubsystem,UObject* DefDelegateObj)
{
	// Check each task for completion
	for (INT Index = 0; Index < AsyncTasks.Num(); Index++)
	{
		AsyncTasks(Index)->UpdateElapsedTime(DeltaTime);
		if (AsyncTasks(Index)->HasTaskCompleted())
		{
			// Perform any task specific finalization of data before having
			// the script delegate fired off
			if (AsyncTasks(Index)->ProcessAsyncResults(LiveSubsystem) == TRUE)
			{
				// Have the task fire off its delegates on our object
				AsyncTasks(Index)->TriggerDelegates(DefDelegateObj);
#if !FINAL_RELEASE && !SHIPPING_PC_GAME
				AsyncTasks(Index)->LogResults();
#endif
				// Free the memory and remove it from our list
				delete AsyncTasks(Index);
				AsyncTasks.Remove(Index);
				Index--;
			}
		}
#if !CONSOLE
		// If for some reason the task is taking way too long, treat it as failed and orphan
		else if (LiveSubsystem && LiveSubsystem->MaxElapsedAsyncTaskTime > 0 && LiveSubsystem->MaxElapsedAsyncTaskTime < AsyncTasks(Index)->GetElapsedTime())
		{
			debugf(NAME_Error,TEXT("\r\n\r\nOnline async task has taken longer than allowed. This will leak memory\r\n"));
			// Trigger the delegates, which will force them to send a failure
			AsyncTasks(Index)->TriggerDelegates(DefDelegateObj);
#if !SHIPPING_PC_GAME
			AsyncTasks(Index)->LogResults();
			debugf(NAME_DevOnline,TEXT("\r\n\r\n"));
#endif
			// Remove from the array
			AsyncTasks.Remove(Index);
			Index--;
		}
#endif
	}
}

/**
 * Finds the player controller associated with the specified index
 *
 * @param Index the id of the user to find
 *
 * @return the player controller for that id
 */
inline APlayerController* GetPlayerControllerFromUserIndex(INT Index)
{
	// Find the local player that has the same controller id as the index
	for (FLocalPlayerIterator It(GEngine); It; ++It)
	{
		ULocalPlayer* Player = *It;
		if (Player->ControllerId == Index)
		{
			// The actor is the corresponding player controller
			return Player->Actor;
		}
	}
	return NULL;
}

/**
 * Copies the properties we are interested in from the source object to the destination
 *
 * @param Dest the target to copy to
 * @param Src the object to copy from
 */
inline void CopyGameSettings(UOnlineGameSettings* Dest,UOnlineGameSettings* Src)
{
	if (Dest && Src)
	{
		// Copy the session size information
		Dest->NumPublicConnections = Src->NumPublicConnections;
		Dest->NumPrivateConnections = Src->NumPrivateConnections;
		// Copy the flags that will be set on the session
		Dest->bUsesStats = Src->bUsesStats;
		Dest->bAllowJoinInProgress = Src->bAllowJoinInProgress;
		Dest->bAllowInvites = Src->bAllowInvites;
		Dest->bUsesPresence = Src->bUsesPresence;
		Dest->bAllowJoinViaPresence = Src->bAllowJoinViaPresence;
		Dest->bUsesArbitration = Src->bUsesArbitration;
		// Update the properties/contexts
		Dest->UpdateStringSettings(Src->LocalizedSettings);
		Dest->UpdateProperties(Src->Properties);
	}
}

/**
 * @return TRUE if this is the server, FALSE otherwise
 */
inline UBOOL IsServer(void)
{
	return GWorld &&
		GWorld->GetWorldInfo() &&
		GWorld->GetWorldInfo()->NetMode < NM_Client;
}

#if CONSOLE
/**
 * Populates a XCONTENT_DATA struct with the data from FOnlineSaveGame
 *
 * @param Src the source FOnlineSaveGame
 * @param Dest the 
 */
inline void CopyOnlineSaveGameToContentData(const FOnlineSaveGame& Src,XCONTENT_DATA* Dest)
{
	check(Dest);
	// Zero any previous data
	appMemzero(Dest,sizeof(XCONTENT_DATA));
	Dest->dwContentType = XCONTENTTYPE_SAVEDGAME;
	Dest->DeviceID = Src.DeviceID;
	// Copy the filenames involved
	appStrncpy(Dest->szDisplayName,*Src.FriendlyName,XCONTENT_MAX_DISPLAYNAME_LENGTH);
	// NOTE: This API does not expect a NULL terminated string even with the SZ prefix,
	// so truncate to one less for the NULL terminator
	appStrcpyANSI(Dest->szFileName,XCONTENT_MAX_FILENAME_LENGTH,TCHAR_TO_ANSI(*Src.Filename.Left(XCONTENT_MAX_FILENAME_LENGTH - 1)));
}

/**
 * Populates a XCONTENT_DATA struct with the data from FOnlineSaveGame
 *
 * @param Src the source FOnlineSaveGame
 * @param Dest the 
 */
inline void CopyOnlineSaveGameToContentData(const FOnlineCrossTitleSaveGame& Src,XCONTENT_CROSS_TITLE_DATA* Dest)
{
	check(Dest);
	// Zero any previous data
	appMemzero(Dest,sizeof(XCONTENT_CROSS_TITLE_DATA));
	Dest->dwContentType = XCONTENTTYPE_SAVEDGAME;
	Dest->DeviceID = Src.DeviceID;
	Dest->dwTitleId = Src.TitleId;
	// Copy the filenames involved
	appStrncpy(Dest->szDisplayName,*Src.FriendlyName,XCONTENT_MAX_DISPLAYNAME_LENGTH);
	// NOTE: This API does not expect a NULL terminated string even with the SZ prefix,
	// so truncate to one less for the NULL terminator
	appStrcpyANSI(Dest->szFileName,XCONTENT_MAX_FILENAME_LENGTH,TCHAR_TO_ANSI(*Src.Filename.Left(XCONTENT_MAX_FILENAME_LENGTH - 1)));
}
#endif

/**
 * Creates a unique path name for content binding
 *
 * @param ContentType whether this is a save game or DLC content
 *
 * @return the unique path
 */
inline FString GenerateUniqueContentPath(const BYTE ContentType)
{
	static INT ContentNum = 0;
	return FString::Printf(ContentType == OCT_Downloaded ? TEXT("DLC%d") : TEXT("SG%d"),ContentNum++);
}

/**
 * Routes the call to the function on the subsystem for parsing search results
 *
 * @param LiveSubsystem the object to make the final call on
 */
UBOOL FLiveAsyncTaskSearch::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	UBOOL bDelete = FALSE;
	DWORD Result = GetCompletionCode();
	if (Result == ERROR_SUCCESS)
	{
		// See if we are just waiting for live to send back matchmaking
		if (bIsWaitingForLive == TRUE)
		{
			bIsWaitingForLive = FALSE;
			FLiveAsyncTaskDataSearch* AsyncData = (FLiveAsyncTaskDataSearch*)TaskData;
			// Parse the Live results
			LiveSubsystem->ParseSearchResults(LiveSubsystem->GameSearch,
				*AsyncData);
			// Kick off QoS searches for the servers that were returned
			if (LiveSubsystem->CheckServersQoS((FLiveAsyncTaskDataSearch*)TaskData))
			{
				TriggerQosDelegates(LiveSubsystem);
			}
			else
			{
				// QoS failed so don't wait
				bDelete = TRUE;
			}
		}
		// We are waiting on QoS results
		else
		{
			FLiveAsyncTaskDataSearch* AsyncData = (FLiveAsyncTaskDataSearch*)TaskData;
			// Make sure we have data and then check it for completion
			XNQOS* QosData = *AsyncData->GetXNQOS();
			if (QosData != NULL)
			{
				// If there have been more that have completed, fire the delegates so the listeners will know
				if (LastQosPending != QosData->cxnqosPending)
				{
					TriggerQosDelegates(LiveSubsystem);
				}
				// Check if all results are back
				if (QosData->cxnqosPending == 0)
				{
					// Have the subsystem update its search results data
					LiveSubsystem->ParseQoSResults(QosData);
					bDelete = TRUE;
				}
			}
			else
			{
				debugfLiveSlow(NAME_DevOnline,TEXT("NULL XNQOS pointer, aborting QoS code"));
				// Something is messed up
				bDelete = TRUE;
			}
		}
	}
	else
	{
		// Stuff is broked
		debugf(NAME_DevOnline,TEXT("XSessionSearch() completed with error 0x%08X"),Result);
		bDelete = TRUE;
	}
	// Mark the search as complete
	if (bDelete == TRUE && LiveSubsystem->GameSearch)
	{
		LiveSubsystem->GameSearch->bIsSearchInProgress = FALSE;
	}
	return bDelete;
}

/**
 * Fires off the qos delegates with the progress data
 *
 * @param LiveSubsystem the object to make the final call on
 */
void FLiveAsyncTaskSearch::TriggerQosDelegates(UOnlineSubsystemLive* LiveSubsystem)
{
	FLiveAsyncTaskDataSearch* AsyncData = (FLiveAsyncTaskDataSearch*)TaskData;
	// Fire off the first of our QoS updates indicating how many are in flight
	XNQOS* QosData = *AsyncData->GetXNQOS();
	// Track how many are left so we know when to fire the delegates again
	LastQosPending = QosData->cxnqosPending;
	OnlineSubsystemLive_eventOnQosStatusChanged_Parms Parms(EC_EventParm);
	// Update the QoS data for sending to script
	Parms.NumComplete = QosData->cxnqos - LastQosPending;
	Parms.NumTotal = QosData->cxnqos;
	TriggerOnlineDelegates(LiveSubsystem,*QosDelegates,&Parms);
}

/**
 * Coalesces the game settings data into one buffer instead of 3
 */
void FLiveAsyncTaskDataReadProfileSettings::CoalesceGameSettings(void)
{
	PXUSER_READ_PROFILE_SETTING_RESULT ReadResults = GetGameSettingsBuffer();
	// Copy each binary buffer into the working buffer
	for (DWORD Index = 0; Index < ReadResults->dwSettingsLen; Index++)
	{
		XUSER_PROFILE_SETTING& LiveSetting = ReadResults->pSettings[Index];
		// Don't bother copying data for no value settings and the data
		// should only be binary. Ignore otherwise
		if (LiveSetting.source != XSOURCE_NO_VALUE &&
			LiveSetting.data.type == XUSER_DATA_TYPE_BINARY)
		{
			// Figure out how much data to copy
			appMemcpy(&WorkingBuffer[WorkingBufferUsed],
				LiveSetting.data.binary.pbData,
				LiveSetting.data.binary.cbData);
			// Increment our offset for the next copy
			WorkingBufferUsed += LiveSetting.data.binary.cbData;
		}
	}
}

/**
 * Reads the online profile settings from the buffer into the specified array
 *
 * @param Settings the array to populate from the game settings
 */
void FLiveAsyncTaskReadProfileSettings::SerializeGameSettings(TArray<FOnlineProfileSetting>& Settings)
{
	FLiveAsyncTaskDataReadProfileSettings* Data =(FLiveAsyncTaskDataReadProfileSettings*)TaskData;
	// Don't bother if the buffer wasn't there
	if (Data->GetWorkingBufferSize() > 0)
	{
		FProfileSettingsReaderLive Reader(MAX_PROFILE_DATA_SIZE,Data->GetWorkingBuffer(),Data->GetWorkingBufferSize(),OnlineXuid);
		// Serialize the profile from that array
		if (Reader.SerializeFromBuffer(Settings) == FALSE)
		{
			// Empty the array if it failed to read
			Settings.Empty();
		}
	}
}

/**
 * Routes the call to the function on the subsystem for parsing the results
 *
 * @param LiveSubsystem the object to make the final call on
 */
UBOOL FLiveAsyncTaskReadProfileSettings::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	UBOOL bDone = FALSE;
	FLiveAsyncTaskDataReadProfileSettings* Data = (FLiveAsyncTaskDataReadProfileSettings*)TaskData;
	DWORD Result = GetCompletionCode();
	if (Result == ERROR_SUCCESS)
	{
		// Figure out what our next step is
		if (Data->GetCurrentAction() == FLiveAsyncTaskDataReadProfileSettings::ReadingGameSettings)
		{
			// Build one big buffer out of the returned data
			Data->CoalesceGameSettings();
			TArray<FOnlineProfileSetting> Settings;
			// Serialize from the buffer
			SerializeGameSettings(Settings);
			TArray<DWORD> MissingSettingIds;
			// Get the Ids that the game is interested in
			DWORD* Ids = Data->GetIds();
			DWORD NumIds = Data->GetIdsCount();
			// For each ID that we need to read
			for (DWORD IdIndex = 0; IdIndex < NumIds; IdIndex++)
			{
				DWORD SettingId = Ids[IdIndex];
				UBOOL bFound = FALSE;
				// Search the resulting array for the data
				for (INT Index = 0; Index < Settings.Num(); Index++)
				{
					const FOnlineProfileSetting& Setting = Settings(Index);
					// If found, copy the data from the array to the profile data
					if (Setting.ProfileSetting.PropertyId == SettingId)
					{
						// Place the data in the user's profile results array
						LiveSubsystem->AppendProfileSetting(Data->GetUserIndex(),Setting);
						bFound = TRUE;
						// Shrink the set so we can track ones that might have come from a TU
						Settings.Remove(Index);
						Index--;
						break;
					}
				}
				// The requested ID wasn't in the game settings list, so add the
				// ID to the list we need to read from Live
				if (bFound == FALSE)
				{
					MissingSettingIds.AddItem(SettingId);
				}
			}
			// If there are IDs we need to read from Live and/or the game defaults
			if (MissingSettingIds.Num() > 0)
			{
				// Fill any game specific settings that aren't Live aware from the defaults
				LiveSubsystem->ProcessProfileDefaults(Data->GetUserIndex(),MissingSettingIds);
				// The game defaults may have fulfilled the remaining ids
				if (MissingSettingIds.Num() > 0)
				{
					check(MissingSettingIds.Num() <= (INT)Data->GetIdsCount());
					// Map the unreal IDs to Live ids
					BuildLiveProfileReadIDs(MissingSettingIds,Data->GetIds());
					// Allocate a buffer for the ones we need to read from Live
					Data->AllocateBuffer(MissingSettingIds.Num());
					appMemzero(&Overlapped,sizeof(XOVERLAPPED));
					// Need to indicate the buffer size
					DWORD SizeNeeded = Data->GetIdsSize();
					// Kick off a new read with just the missing ids
					DWORD Return = XUserReadProfileSettings(0,
						Data->GetUserIndex(),
						MissingSettingIds.Num(),
						Data->GetIds(),
						&SizeNeeded,
						Data->GetProfileBuffer(),
						&Overlapped);
					if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
					{
						bDone = FALSE;
						// Mark this task as still processing
						Data->SetCurrentAction(FLiveAsyncTaskDataReadProfileSettings::ReadingLiveSettings);
					}
					else
					{
						debugf(NAME_DevOnline,TEXT("Failed to read Live IDs 0x%08X"),Return);
						bDone = TRUE;
					}
				}
				else
				{
					// All requested profile settings were met
					bDone = TRUE;
				}
			}
			else
			{
				// See if there are additional settings left and append those
				// This can happen if a TU added data and the player cleared their cache
				for (INT Index = 0; Index < Settings.Num(); Index++)
				{
					const FOnlineProfileSetting& Setting = Settings(Index);
					// Place the data in the user's profile results array
					LiveSubsystem->AppendProfileSetting(Data->GetUserIndex(),Setting);
				}
				// All requested profile settings were met
				bDone = TRUE;
			}
		}
		else
		{
			// Append anything that comes back
			LiveSubsystem->ParseReadProfileResults(Data->GetUserIndex(),
				Data->GetProfileBuffer());
			bDone = TRUE;
		}
	}
	else
	{
		bDone = TRUE;
		debugf(NAME_DevOnline,TEXT("Profile read failed with 0x%08X"),Result);
		// Set the profile to the defaults in this case
		if (LiveSubsystem->ProfileCache[Data->GetUserIndex()].Profile != NULL)
		{
			LiveSubsystem->ProfileCache[Data->GetUserIndex()].Profile->eventSetToDefaults();
		}
	}
	if (bDone)
	{
		// In case this gets cleared while in progress
		if (LiveSubsystem->ProfileCache[Data->GetUserIndex()].Profile != NULL)
		{
			INT ReadVersion = LiveSubsystem->ProfileCache[Data->GetUserIndex()].Profile->GetVersionNumber();
			// Check the version number and reset to defaults if they don't match
			if (LiveSubsystem->ProfileCache[Data->GetUserIndex()].Profile->VersionNumber != ReadVersion)
			{
				debugfLiveSlow(NAME_DevOnline,
					TEXT("Detected profile version mismatch (%d != %d), setting to defaults"),
					LiveSubsystem->ProfileCache[Data->GetUserIndex()].Profile->VersionNumber,
					ReadVersion);
				LiveSubsystem->ProfileCache[Data->GetUserIndex()].Profile->eventSetToDefaults();
			}
			// Done with the reading, so mark the async state as done
			LiveSubsystem->ProfileCache[Data->GetUserIndex()].Profile->AsyncState = OPAS_Finished;
		}
	}
#if DEBUG_PROFILE_DATA
	if (LiveSubsystem->ProfileCache[Data->GetUserIndex()].Profile != NULL)
	{
		DumpProfile(LiveSubsystem->ProfileCache[Data->GetUserIndex()].Profile);
	}
#endif
	return bDone;
}

/**
 * Iterates the list of delegates and fires those notifications
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FLiveAsyncTaskReadProfileSettings::TriggerDelegates(UObject* Object)
{
	check(Object);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL)
	{
		BYTE UserIndex = (BYTE)((FLiveAsyncTaskDataReadProfileSettings*)TaskData)->GetUserIndex();
		// Pass in the data that indicates whether the call worked or not
		OnlineSubsystemLive_eventOnReadProfileSettingsComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = (GetCompletionCode() == 0) ? FIRST_BITFIELD : 0;
		Parms.LocalUserNum = UserIndex;
		// Use the common method to do the work
		TriggerOnlineDelegates(Object,*ScriptDelegates,&Parms);
	}
}

/**
 * Routes the call to the function on the subsystem for parsing search results
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskReadCrossTitleProfileSettings::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	FLiveAsyncTaskDataReadProfileSettings* Data = (FLiveAsyncTaskDataReadProfileSettings*)TaskData;
	DWORD LocalUserNum = Data->GetUserIndex();
	DWORD Result = GetCompletionCode();
	if (Result == ERROR_SUCCESS)
	{
		// Build one big buffer out of the returned data
		Data->CoalesceGameSettings();
		FCrossTitleProfileEntry* CacheEntry = LiveSubsystem->ProfileCache[LocalUserNum].FindCrossTitleProfileEntry(TitleId);
		if (CacheEntry != NULL)
		{
			// Serialize from the buffer
			SerializeGameSettings(CacheEntry->Profile->ProfileSettings);
			// Done with the reading, so mark the async state as done
			CacheEntry->Profile->AsyncState = OPAS_Finished;
		}
	}
	return TRUE;
}

/**
 * Iterates the list of delegates and fires those notifications
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FLiveAsyncTaskReadCrossTitleProfileSettings::TriggerDelegates(UObject* Object)
{
	check(Object);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL)
	{
		BYTE UserIndex = (BYTE)((FLiveAsyncTaskDataReadProfileSettings*)TaskData)->GetUserIndex();
		// Pass in the data that indicates whether the call worked or not
		OnlineSubsystemLive_eventOnReadCrossTitleProfileSettingsComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = (GetCompletionCode() == 0) ? FIRST_BITFIELD : 0;
		Parms.TitleId = TitleId;
		Parms.LocalUserNum = UserIndex;
		// Use the common method to do the work
		TriggerOnlineDelegates(Object,*ScriptDelegates,&Parms);
	}
}

/**
 * Clears the active profile write reference since it is no longer needed
 *
 * @param LiveSubsystem the object to make the final call on
 */
UBOOL FLiveAsyncTaskWriteProfileSettings::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	FLiveAsyncTaskDataWriteProfileSettings* Data = (FLiveAsyncTaskDataWriteProfileSettings*)TaskData;
	// In case this gets cleared while in progress
	if (LiveSubsystem->ProfileCache[Data->GetUserIndex()].Profile != NULL)
	{
		// Done with the writing, so mark the async state
		LiveSubsystem->ProfileCache[Data->GetUserIndex()].Profile->AsyncState = OPAS_Finished;
	}
	return TRUE;
}

/**
 * Iterates the list of delegates and fires those notifications
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FLiveAsyncTaskWriteProfileSettings::TriggerDelegates(UObject* Object)
{
	check(Object);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL)
	{
		BYTE UserIndex = (BYTE)((FLiveAsyncTaskDataWriteProfileSettings*)TaskData)->GetUserIndex();
		// Pass in the data that indicates whether the call worked or not
		OnlineSubsystemLive_eventOnWriteProfileSettingsComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = (GetCompletionCode() == 0) ? FIRST_BITFIELD : 0;
		Parms.LocalUserNum = UserIndex;
		// Use the common method to do the work
		TriggerOnlineDelegates(Object,*ScriptDelegates,&Parms);
	}
}

/**
 * Changes the state of the game session to the one specified at construction
 *
 * @param LiveSubsystem the object to make the final call on
 */
UBOOL FLiveAsyncTaskSessionStateChange::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	FNamedSession* Session = LiveSubsystem->GetNamedSession(SessionName);
	if (Session && Session->GameSettings)
	{
		Session->GameSettings->GameState = StateToTransitionTo;
	}
	return TRUE;
}

/**
 * Checks the arbitration flag and issues a session size change if arbitrated
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskStartSession::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	if (bUsesArbitration)
	{
		// Kick off another task to shrink the session size
		LiveSubsystem->ShrinkToArbitratedRegistrantSize(LiveSubsystem->GetNamedSession(SessionName));
	}
	return FLiveAsyncTaskSessionStateChange::ProcessAsyncResults(LiveSubsystem);
}

/**
 * Changes the state of the game session to pending and registers all of the
 * local players
 *
 * @param LiveSubsystem the object to make the final call on
 */
UBOOL FLiveAsyncTaskCreateSession::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	if (bIsCreate)
	{
		// Forward to the subsystem for completion
		LiveSubsystem->FinishCreateOnlineGame(HostingPlayerNum,
			SessionName,
			XGetOverlappedExtendedError(&Overlapped),			
			bIsFromInvite);
	}
	else
	{
		// Forward to the subsystem for completion
		LiveSubsystem->FinishJoinOnlineGame(HostingPlayerNum,
			SessionName,
			XGetOverlappedExtendedError(&Overlapped),
			bIsFromInvite);
	}
	return TRUE;
}

/**
 * Routes the call to the function on the subsystem for parsing search results
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskMigrateSession::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	LiveSubsystem->FinishMigrateOnlineGame(
		PlayerNum,
		SessionName,
		XGetOverlappedExtendedError(&Overlapped),
		bIsHost);

	return TRUE;
}

/**
 * Routes the call to the function on the subsystem for parsing friends
 * results. Also, continues searching as needed until there are no more
 * friends to read
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskReadFriends::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	FLiveAsyncTaskDataEnumeration* FriendsData = (FLiveAsyncTaskDataEnumeration*)TaskData;
	// Figure out if we are at the end of the list or not
	DWORD Result = XGetOverlappedExtendedError(&Overlapped);
	// If the task completed ok, then parse the results and start another async task
	if (Result == ERROR_SUCCESS)
	{
		DWORD ItemsEnumerated = 0;
		XGetOverlappedResult(&Overlapped,&ItemsEnumerated,FALSE);
		// Figure out which items we were reading
		if (ReadState == ReadingFriendsXuids)
		{
			PXONLINE_FRIEND Friends = (PXONLINE_FRIEND)FriendsData->GetBuffer();
			// Build a list of XUIDs for requesting the presence information
			for (DWORD Index = 0; Index < ItemsEnumerated; Index++)
			{
				PresenceList.AddItem(Friends[Index].xuid);
			}
			// Add the data to the friends cache
			LiveSubsystem->ParseFriendsResults(FriendsData->GetPlayerIndex(),
				(PXONLINE_FRIEND)FriendsData->GetBuffer(),ItemsEnumerated);
			// Zero between uses or results will be incorrect
			appMemzero(&Overlapped,sizeof(XOVERLAPPED));
			// Do another async friends read
			Result = XEnumerate(FriendsData->GetHandle(),
				FriendsData->GetBuffer(),
				FriendsData->GetBufferSize(),
				0,
				&Overlapped);
		}
		else
		{
			// Add the data to the friends cache
			LiveSubsystem->ParseFriendsResults(FriendsData->GetPlayerIndex(),
				(PXONLINE_PRESENCE)FriendsData->GetBuffer(),ItemsEnumerated);
			// Zero between uses or results will be incorrect
			appMemzero(&Overlapped,sizeof(XOVERLAPPED));
			// Do another presence read
			Result = XEnumerate(PresenceHandle,
				FriendsData->GetBuffer(),
				FriendsData->GetBufferSize(),
				0,
				&Overlapped);
		}
	}
	else
	{
		// Check for "no more files"
		if (Result == 0x80070012)
		{
			if (ReadState == ReadingFriendsPresence)
			{
				// Done reading, need to mark the cache as finished
				LiveSubsystem->FriendsCache[FriendsData->GetPlayerIndex()].ReadState = OERS_Done;
			}
			// Done enumerating friends, now read presence
			else
			{
				if ( PresenceList.Num() > 0 )
				{
					// Tell the presence code who we care about
					Result = XPresenceSubscribe(FriendsData->GetPlayerIndex(),
						PresenceList.Num(),
						PresenceList.GetTypedData());
					debugfSlow(NAME_DevOnline,
						TEXT("XPresenceSubscribe(%d,%d,ptr) return 0x%08X"),
						FriendsData->GetPlayerIndex(),
						PresenceList.Num(),
						Result);
					DWORD BufferSize = 0;
					// Create a new enumeration for presence info
					Result = XPresenceCreateEnumerator(FriendsData->GetPlayerIndex(),
						PresenceList.Num(),
						PresenceList.GetTypedData(),
						0,
						MAX_FRIENDS,
						&BufferSize,
						&PresenceHandle);
					debugfSlow(NAME_DevOnline,
						TEXT("XPresenceCreateEnumerator(%d,%d,ptr,0,%d,%d,handle) return 0x%08X"),
						FriendsData->GetPlayerIndex(),
						PresenceList.Num(),
						MAX_FRIENDS,
						BufferSize,
						Result);
					check(BufferSize < FriendsData->GetBufferSize());
					if (Result == ERROR_SUCCESS)
					{
						// Zero between uses or results will be incorrect
						appMemzero(&Overlapped,sizeof(XOVERLAPPED));
						// Have the enumeration start reading data
						Result = XEnumerate(PresenceHandle,
							FriendsData->GetBuffer(),
							FriendsData->GetBufferSize(),
							0,
							&Overlapped);
					}
					// Switch the read state so that we know when to end
					ReadState = ReadingFriendsPresence;
				}
				else
				{
					// Done reading, need to mark the cache as finished
					LiveSubsystem->FriendsCache[ FriendsData->GetPlayerIndex() ].ReadState = OERS_Done;
				}
			}
		}
		else
		{
			// Mark it as in error
			LiveSubsystem->FriendsCache[FriendsData->GetPlayerIndex()].ReadState = OERS_Failed;
		}
	}
	// When this is true, there is no more data left to read
	return Result != ERROR_SUCCESS && Result != ERROR_IO_PENDING;
}

/**
 * Routes the call to the function on the subsystem for parsing content
 * results. Also, continues searching as needed until there are no more
 * content to read
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskReadContent::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
#if CONSOLE
	FLiveAsyncTaskContent* ContentTaskData = (FLiveAsyncTaskContent*)TaskData;
	// Make sure we are marking the correct state
	BYTE& ReadState = ContentTaskData->GetContentType() == OCT_Downloaded ?
		LiveSubsystem->ContentCache[ContentTaskData->GetPlayerIndex()].ReadState :
		LiveSubsystem->ContentCache[ContentTaskData->GetPlayerIndex()].SaveGameReadState;

	// what is our current state?
	DWORD Result = XGetOverlappedExtendedError(&Overlapped);

	// Zero between uses or results will be incorrect
	appMemzero(&Overlapped,sizeof(XOVERLAPPED));

	// look to see how our enumeration is progressing
	if (TaskMode == CTM_Enumerate)
	{
		// If the task completed ok, then parse the results and start another async task to open the content
		if (Result == ERROR_SUCCESS)
		{
			// process a piece of content found by the enumerator
			XCONTENT_DATA* Content = (XCONTENT_DATA*)ContentTaskData->GetBuffer();

			// create a virtual drive that the content will be mapped to (number is unimportant, just needs to be unique)
			const FString ContentDrive = GenerateUniqueContentPath(ContentTaskData->GetContentType());

			// open up the package (make sure it's openable)
			DWORD Return = XContentCreate(ContentTaskData->GetPlayerIndex(),
				TCHAR_TO_ANSI(*ContentDrive),
				Content,
				ContentTaskData->GetContentType() == OCT_Downloaded ? XCONTENTFLAG_OPENEXISTING : XCONTENTFLAG_OPENEXISTING | XCONTENTFLAG_NOPROFILE_TRANSFER,
				NULL,
				// Read the license mask from the package if it is DLC
				ContentTaskData->GetContentType() == OCT_Downloaded ? ContentTaskData->GetLicenseMaskBuffer() : NULL,
				&Overlapped);

			if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
			{
				// remember the data for the async task
				ContentTaskData->SetContentDrive(ContentDrive);
				ContentTaskData->SetFriendlyName(Content->szDisplayName);

				// switch modes
				TaskMode = CTM_Create;
			}
		}
		else
		{
			// Done reading, need to mark the cache as finished
			ReadState = OERS_Done;
		}
	}
	else
	{
		// get the cache to fill out
		FContentListCache& Cache = LiveSubsystem->ContentCache[ContentTaskData->GetPlayerIndex()];
		// find the newly added content
		FOnlineContent* NewContent = NULL;
		XCONTENT_DATA* Content = (XCONTENT_DATA*)ContentTaskData->GetBuffer();
		// Add them to the proper array based upon type
		if (Content->dwContentType == XCONTENTTYPE_MARKETPLACE)
		{
			// add a new empty content structure
			INT Index = Cache.Content.AddZeroed(1);
			NewContent = &Cache.Content(Index);
			// Mark this as a DLC type
			NewContent->ContentType = OCT_Downloaded;
			// Get the license mask for the DLC
			NewContent->LicenseMask = ContentTaskData->GetLicenseMask();
		}
		else
		{
			// add a new empty content structure
			INT Index = Cache.SaveGameContent.AddZeroed(1);
			NewContent = &Cache.SaveGameContent(Index);
			// Mark this as a save game type
			NewContent->ContentType = OCT_SaveGame;
		}
		// Copy the device this content is on
		NewContent->DeviceID = Content->DeviceID;
		// Make an ANSI string out of the filename field
		ANSICHAR Buffer[XCONTENT_MAX_FILENAME_LENGTH + 1];
		appMemzero(Buffer,XCONTENT_MAX_FILENAME_LENGTH + 1);
		appMemcpy(Buffer,Content->szFileName,XCONTENT_MAX_FILENAME_LENGTH);
		// Copy that file name field to our struct
		NewContent->Filename = ANSI_TO_TCHAR(Buffer);
		// friendly name is the displayable for the content (not a map name)
		NewContent->FriendlyName = ContentTaskData->GetFriendlyName();
		// remember the virtual drive for the content (so we can close it later)
		NewContent->ContentPath = ContentTaskData->GetContentDrive();
		debugfLiveSlow(TEXT("Found Content Package '%s'"),*NewContent->FriendlyName);
		// Assume we failed to open it or validate it
		NewContent->bIsCorrupt = TRUE;
		// If the task completed ok, then parse the results and start another enumeration
		if (Result == ERROR_SUCCESS)
		{
			// This was successfully opened
			NewContent->bIsCorrupt = FALSE;
			// find all the packages in the content
			appFindFilesInDirectory(NewContent->ContentPackages, *(ContentTaskData->GetContentDrive() + TEXT(":\\")), TRUE, FALSE);
			// find all the non-packages in the content
			appFindFilesInDirectory(NewContent->ContentFiles, *(ContentTaskData->GetContentDrive() + TEXT(":\\")), FALSE, TRUE);
			// If we are enumerating save games, close the package before continuing
			if (ContentTaskData->GetContentType() == OCT_SaveGame)
			{
				XContentClose(TCHAR_TO_ANSI(*ContentTaskData->GetContentDrive()),NULL);
			}
			// Do another async content read
			DWORD Return = XEnumerate(ContentTaskData->GetHandle(),
				ContentTaskData->GetBuffer(),
				sizeof(XCONTENT_DATA),
				0,
				&Overlapped);
			if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
			{
				// switch modes
				TaskMode = CTM_Enumerate;
			}
			// if this failed, we need to stop
			else
			{
				// Done reading, need to mark the cache as finished
				ReadState = OERS_Done;
			}
		}
		else
		{
			// Do another async content read to get the next DLC
			DWORD Return = XEnumerate(ContentTaskData->GetHandle(), ContentTaskData->GetBuffer(), sizeof(XCONTENT_DATA), 0, &Overlapped);
			// Switch back to enumerate mode to enumerate the next DLC
			TaskMode = CTM_Enumerate;
		}
	}

	// When this is true, there is no more data left to read
	return ReadState == OERS_Done || ReadState == OERS_Failed;
#else
	return TRUE;
#endif
}

/**
 * Parses the cross title enumeration data
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskReadCrossTitleContent::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
#if CONSOLE
	FLiveAsyncTaskCrossTitleContent* ContentTaskData = (FLiveAsyncTaskCrossTitleContent*)TaskData;
	// Make sure we are marking the correct state
	BYTE& ReadState = ContentTaskData->GetContentType() == OCT_Downloaded ?
		LiveSubsystem->ContentCache[ContentTaskData->GetPlayerIndex()].ReadCrossTitleState :
		LiveSubsystem->ContentCache[ContentTaskData->GetPlayerIndex()].SaveGameCrossTitleReadState;
	// Check the current state enumeration result
	DWORD Result = XGetOverlappedExtendedError(&Overlapped);
	// Zero between uses or results will be incorrect
	appMemzero(&Overlapped,sizeof(XOVERLAPPED));
	// If the task completed ok, then parse the results and start another enumeration
	if (Result == ERROR_SUCCESS)
	{
		XCONTENT_CROSS_TITLE_DATA* Content = (XCONTENT_CROSS_TITLE_DATA*)ContentTaskData->GetBuffer();
		// Filter by title id if requested
		if ((ContentTaskData->GetTitleId() == 0 ||
			ContentTaskData->GetTitleId() == Content->dwTitleId) &&
			// Don't report our own content in the cross platform list
			Content->dwTitleId != appGetTitleId())
		{
			// get the cache to fill out
			FContentListCache& Cache = LiveSubsystem->ContentCache[ContentTaskData->GetPlayerIndex()];
			// find the newly added content
			FOnlineCrossTitleContent* NewContent = NULL;
			// Add them to the proper array based upon type
			if (Content->dwContentType == XCONTENTTYPE_MARKETPLACE)
			{
				// add a new empty content structure
				INT Index = Cache.CrossTitleContent.AddZeroed(1);
				NewContent = &Cache.CrossTitleContent(Index);
				// Mark this as a DLC type
				NewContent->ContentType = OCT_Downloaded;
			}
			else
			{
				// add a new empty content structure
				INT Index = Cache.CrossTitleSaveGameContent.AddZeroed(1);
				NewContent = &Cache.CrossTitleSaveGameContent(Index);
				// Mark this as a save game type
				NewContent->ContentType = OCT_SaveGame;
			}
			// Copy the title id
			NewContent->TitleId = Content->dwTitleId;
			// Copy the device this content is on
			NewContent->DeviceID = Content->DeviceID;
			// Make an ANSI string out of the filename field
			ANSICHAR Buffer[XCONTENT_MAX_FILENAME_LENGTH + 1];
			appMemzero(Buffer,XCONTENT_MAX_FILENAME_LENGTH + 1);
			appMemcpy(Buffer,Content->szFileName,XCONTENT_MAX_FILENAME_LENGTH);
			// Copy that file name field to our struct
			NewContent->Filename = ANSI_TO_TCHAR(Buffer);
			// friendly name is the displayable for the content (not a map name)
			NewContent->FriendlyName = Content->szDisplayName;
			// remember the virtual drive for the content (so we can close it later)
			NewContent->ContentPath = ContentTaskData->GetContentDrive();
			debugf(NAME_DevOnline,TEXT("Found Cross Title Content Package '%s'"),*NewContent->FriendlyName);
			// find all the packages in the content
			appFindFilesInDirectory(NewContent->ContentPackages, *(ContentTaskData->GetContentDrive() + TEXT(":\\")), TRUE, FALSE);
			// find all the non-packages in the content
			appFindFilesInDirectory(NewContent->ContentFiles, *(ContentTaskData->GetContentDrive() + TEXT(":\\")), FALSE, TRUE);
		}
		// Do another async content read
		DWORD Return = XEnumerateCrossTitle(ContentTaskData->GetHandle(),
			ContentTaskData->GetBuffer(),
			sizeof(XCONTENT_CROSS_TITLE_DATA),
			0,
			&Overlapped);
		if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
		{
			// switch modes
			TaskMode = CTM_Enumerate;
		}
		// if this failed, we need to stop
		else
		{
			// Done reading, need to mark the cache as finished
			ReadState = OERS_Done;
		}
	}
	else
	{
		// Done reading, need to mark the cache as finished
		ReadState = OERS_Done;
	}

	// When this is true, there is no more data left to read
	return ReadState == OERS_Done;
#else
	return TRUE;
#endif
}

#if !WITH_PANORAMA
/**
 * Copies the download query results into the per user storage on the subsystem
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskQueryDownloads::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	FLiveAsyncTaskDataQueryDownloads* Data = (FLiveAsyncTaskDataQueryDownloads*)TaskData;
	// Copy to our cached data for this user
	LiveSubsystem->ContentCache[Data->GetUserIndex()].NewDownloadCount = Data->GetQuery()->dwNewOffers;
	LiveSubsystem->ContentCache[Data->GetUserIndex()].TotalDownloadCount = Data->GetQuery()->dwTotalOffers;
	return TRUE;
}
#endif

/**
 * Copies the resulting string into subsytem buffer. Optionally, will start
 * another async task to validate the string if requested
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskKeyboard::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	FLiveAsyncTaskDataKeyboard* KeyboardData = (FLiveAsyncTaskDataKeyboard*)TaskData;
	UBOOL bShouldCleanup = TRUE;
	// Determine if we are validating or just processing the keyboard results
	if (bIsValidating == FALSE)
	{
		// Keyboard results are back
		if (KeyboardData->NeedsStringValidation() &&
			XGetOverlappedExtendedError(&Overlapped) != ERROR_CANCELLED)
		{
			appMemzero(&Overlapped,sizeof(XOVERLAPPED));
			// Set up the string input data
			TCHAR* String = *KeyboardData;
			STRING_DATA* StringData = *KeyboardData;
			StringData->wStringSize = appStrlen(String);
			StringData->pszString = String;
			// Kick off the validation as an async task
			DWORD Return = XStringVerify(0,
//@todo joeg -- figure out what the different strings are based upon language
				"en-us",
				1,
				StringData,
				sizeof(STRING_VERIFY_RESPONSE) + sizeof(HRESULT),
				*KeyboardData,
				&Overlapped);
			if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
			{
				bShouldCleanup = FALSE;
				bIsValidating = TRUE;
			}
			else
			{
				debugf(NAME_DevOnline,
					TEXT("Failed to validate string (%s) with 0x%08X"),
					String,
					Return);
			}
		}
		else
		{
			// Copy the data into our subsystem buffer
			LiveSubsystem->KeyboardInputResults = *KeyboardData;
			// Determine if the user canceled input or not
			LiveSubsystem->bWasKeyboardInputCanceled = XGetOverlappedExtendedError(&Overlapped) == ERROR_CANCELLED;
		}
	}
	else
	{
		// Validation is complete, so copy the string if ok otherwise zero it
		STRING_VERIFY_RESPONSE* Response = *KeyboardData;
		if (Response->pStringResult[0] == S_OK)
		{
			// String is ok, so copy
			LiveSubsystem->KeyboardInputResults = *KeyboardData;
		}
		else
		{
			// String was a bad word so empty
			LiveSubsystem->KeyboardInputResults = TEXT("");
		}
	}
	return bShouldCleanup;
}

/**
 * Tells the Live subsystem to parse the results of the stats read
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskReadStats::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	UBOOL bShouldDelete = TRUE;
	DWORD Result = GetCompletionCode();
	debugfLiveSlow(NAME_DevOnline,TEXT("XUserReadStats() returned 0x%08X"),Result);
	if (Result == ERROR_SUCCESS)
	{
		LiveSubsystem->ParseStatsReadResults(GetReadResults());
		// Update the player buffers and see if there are more to read
		UpdatePlayersToRead();
		if (NumToRead > 0)
		{
			appMemzero(&Overlapped,sizeof(XOVERLAPPED));
			bShouldDelete = FALSE;
			// Kick off another async read
			DWORD Return = XUserReadStats(TitleId,
				NumToRead,
				GetPlayers(),
				1,
				GetSpecs(),
				&BufferSize,
				GetReadResults(),
				&Overlapped);
			debugfLiveSlow(NAME_DevOnline,
				TEXT("Paged XUserReadStats(0,%d,Players,1,Specs,%d,Buffer,Overlapped) returned 0x%08X"),
				NumToRead,BufferSize,Return);
			if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
			{
				bShouldDelete = TRUE;
			}
		}
	}
	// If we are done processing, zero the read state
	if (bShouldDelete)
	{
		LiveSubsystem->CurrentStatsRead->eventOnReadComplete();
		LiveSubsystem->CurrentStatsRead = NULL;
	}
	return bShouldDelete;
}

/**
 * Tells the Live subsystem to parse the results of the stats read
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskReadStatsByRank::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	DWORD Result = GetCompletionCode();
	if (Result == ERROR_SUCCESS)
	{
		LiveSubsystem->ParseStatsReadResults(GetReadResults());
		LiveSubsystem->CurrentStatsRead->eventOnReadComplete();
	}
	LiveSubsystem->CurrentStatsRead = NULL;
	debugfLiveSlow(NAME_DevOnline,TEXT("XEnumerate() returned 0x%08X"),Result);
	return TRUE;
}

/**
 * Tells the Live subsystem to continue the async game invite join
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskJoinGameInvite::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	DWORD Result = GetCompletionCode();
	if (Result == ERROR_SUCCESS)
	{
		if (State == WaitingForSearch)
		{
			// Need to create a search object to append the results to
			UOnlineGameSearch* Search = ConstructObject<UOnlineGameSearch>(UOnlineGameSearch::StaticClass());
			// Store the search so it can be used on accept
			LiveSubsystem->InviteCache[UserNum].InviteSearch = Search;
			// We need to parse the search results
			LiveSubsystem->ParseSearchResults(Search,GetResults());
			// Get the game object so we can mark it as being from an invite
			InviteSettings = Search->Results.Num() > 0 ? Search->Results(0).GameSettings : NULL;
			if (InviteSettings != NULL)
			{
				InviteSettings->bWasFromInvite = TRUE;
				// Request QoS so we can get the list play server's data
				RequestQoS(Search);
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("FLiveAsyncTaskJoinGameInvite: No session search result."));
				// Can't join this server since the search didn't return it
				State = InviteReady;
			}
		}
		else if (State == QueryingQos)
		{
			if (QosData != NULL)
			{
				// Check if all results are back
				if (QosData->cxnqosPending == 0)
				{
					// Move the QoS data to the settings object
					ParseQoS(LiveSubsystem->InviteCache[UserNum].InviteSearch);
				}
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("FLiveAsyncTaskJoinGameInvite: QoS request failed."));
			InviteSettings = NULL;
			State = InviteReady;
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("FLiveAsyncTaskJoinGameInvite: Session search failed."));
		InviteSettings = NULL;
		State = InviteReady;
	}
	if (State == InviteReady)
	{
		// Fire off the delegate with the results (possibly NULL if not found)
		OnlineSubsystemLive_eventOnGameInviteAccepted_Parms Parms(EC_EventParm);
		Parms.InviteResult = FOnlineGameSearchResult(EC_EventParm);
		if (InviteSettings != NULL && 
			LiveSubsystem->InviteCache[UserNum].InviteSearch != NULL &&
			LiveSubsystem->InviteCache[UserNum].InviteSearch->Results.Num() > 0)
		{
			Parms.InviteResult = LiveSubsystem->InviteCache[UserNum].InviteSearch->Results(0);
		}
		// Use the helper method to fire the delegates
		TriggerOnlineDelegates(LiveSubsystem,*ScriptDelegates,&Parms);
		// Don't fire automatically since we manually fired it off
		ScriptDelegates = NULL;
	}
	return State == InviteReady;
}

/**
 * Reads the qos data for the server that was sending the invite
 *
 * @param Search the game search to update
 */
void FLiveAsyncTaskJoinGameInvite::RequestQoS(UOnlineGameSearch* Search)
{
	XSESSION_INFO* SessInfo = (XSESSION_INFO*)Search->Results(0).PlatformData;
	debugf(NAME_DevOnline,TEXT("Requesting QoS for 0x%016I64X"),(QWORD&)SessInfo->sessionID);
	ServerAddrs[0] = &SessInfo->hostAddress;
	ServerKids[0] = &SessInfo->sessionID;
	ServerKeys[0] = &SessInfo->keyExchangeKey;
	// Kick off the QoS set of queries
	DWORD Return = XNetQosLookup(1,
		(const XNADDR**)ServerAddrs,
		(const XNKID**)ServerKids,
		(const XNKEY**)ServerKeys,
		// We skip all gateway services
		0,0,0,
		// 1 probe is fine since accurate ping doesn't matter
		1,
		4 * 1024,
		// Flags are unsupported and we'll poll
		0,NULL,
		// The out parameter that holds the data
		&QosData);
	debugf(NAME_DevOnline,
		TEXT("Invite: XNetQosLookup(1,Addrs,Kids,Keys,0,0,0,1,64K,0,NULL,Data) returned 0x%08X"),
		Return);
	State = QueryingQos;
}

/**
 * Parses the qos data that came back from the server
 *
 * @param Search the game search to update
 */
void FLiveAsyncTaskJoinGameInvite::ParseQoS(UOnlineGameSearch* Search)
{
	// Iterate through the results
	if (QosData->cxnqos == 1)
	{
		// Read the custom data if present
		if (QosData->axnqosinfo[0].cbData > 0 &&
			QosData->axnqosinfo[0].pbData != NULL)
		{
			// Create a packet reader to read the data out
			FNboSerializeFromBufferXe Packet(QosData->axnqosinfo[0].pbData,
				QosData->axnqosinfo[0].cbData);
			BYTE QosPacketVersion = 0;
			Packet >> QosPacketVersion;
			// Verify the packet version
			if (QosPacketVersion == QOS_PACKET_VERSION)
			{
				// Read the XUID and the server nonce
				Packet >> InviteSettings->OwningPlayerId;
				Packet >> InviteSettings->ServerNonce;
				Packet >> InviteSettings->BuildUniqueId;
				INT NumProps = 0;
				// Read how many props are in the buffer
				Packet >> NumProps;
				InviteSettings->Properties.Empty(NumProps);
				for (INT PropIndex = 0; PropIndex < NumProps; PropIndex++)
				{
					INT AddAt = InviteSettings->Properties.AddZeroed();
					Packet >> InviteSettings->Properties(AddAt);
				}
				INT NumContexts = 0;
				// Read how many contexts are in the buffer
				Packet >> NumContexts;
				InviteSettings->LocalizedSettings.Empty(NumContexts);
				for (INT ContextIndex = 0; ContextIndex < NumContexts; ContextIndex++)
				{
					INT AddAt = InviteSettings->LocalizedSettings.AddZeroed();
					Packet >> InviteSettings->LocalizedSettings(AddAt);
				}
				// Set the ping that the QoS estimated
				InviteSettings->PingInMs = QosData->axnqosinfo[0].wRttMedInMsecs;
				debugfLiveSlow(NAME_DevOnline,TEXT("QoS for invite is %d"),InviteSettings->PingInMs);
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Failed to get QoS data for invite"));
				InviteSettings = NULL;
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Failed to get QoS data for invite"));
			InviteSettings = NULL;
		}
	}
	State = InviteReady;
}

/**
 * Parses the arbitration results and stores them in the arbitration list
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskArbitrationRegistration::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	DWORD Result = XGetOverlappedExtendedError(&Overlapped);
	if (Result == ERROR_SUCCESS)
	{
		// Forward the call to the subsystem
		LiveSubsystem->ParseArbitrationResults(SessionName,GetResults());
	}
	return TRUE;
}

/**
 * Checks to see if the match is arbitrated and shrinks it by one if it is
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncUnregisterPlayer::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	DWORD Result = XGetOverlappedExtendedError(&Overlapped);
	FNamedSession* Session = LiveSubsystem->GetNamedSession(SessionName);
	// Shrink the session size by one if using arbitration
	if (Session &&
		Session->GameSettings &&
		Session->GameSettings->bUsesArbitration &&
		Session->GameSettings->bShouldShrinkArbitratedSessions &&
		Session->GameSettings->GameState >= OGS_InProgress)
	{
		Session->GameSettings->NumPublicConnections--;
		Session->GameSettings->NumPrivateConnections = 0;
		LiveSubsystem->ModifySession(Session,NULL);
	}
	return TRUE;
}

/**
 * Checks to see if the join worked. If this was an invite it may need to
 * try private and then public.
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncRegisterPlayer::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	DWORD Result = XGetOverlappedExtendedError(&Overlapped);
	// If the task failed and we tried private first, then try public
	if (Result != ERROR_SUCCESS && bPrivateInvite == TRUE && bIsSecondTry == FALSE)
	{
		debugfLiveSlow(NAME_DevOnline,TEXT("Private invite failed with 0x%08X. Trying public"),Result);
		bIsSecondTry = TRUE;
		bPrivateInvite = FALSE;
		appMemzero(&Overlapped,sizeof(XOVERLAPPED));
		// Grab the session information by name
		FNamedSession* Session = LiveSubsystem->GetNamedSession(SessionName);
		check(Session);
		FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
		check(SessionInfo);
		// Kick off the async join request
		Result = XSessionJoinRemote(SessionInfo->Handle,
			1,
			GetXuids(),
			GetPrivateInvites(),
			&Overlapped);
		debugf(NAME_DevOnline,TEXT("XSessionJoinRemote(0x%016I64X) returned 0x%08X"),
			*GetXuids(),Result);
		return FALSE;
	}
	return TRUE;
}

/**
 * Iterates the list of delegates and fires those notifications
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FLiveAsyncPlayer::TriggerDelegates(UObject* Object)
{
	check(Object);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL &&
		ScriptDelegates->Num() > 0)
	{
		// This code relies on these having the same members
		check(sizeof(OnlineSubsystemLive_eventOnUnregisterPlayerComplete_Parms) == sizeof(OnlineSubsystemLive_eventOnRegisterPlayerComplete_Parms));
		OnlineSubsystemLive_eventOnUnregisterPlayerComplete_Parms Results(EC_EventParm);
		Results.SessionName = SessionName;
		Results.bWasSuccessful = GetCompletionCode() == ERROR_SUCCESS ? FIRST_BITFIELD : 0;
		(XUID&)Results.PlayerID = PlayerXuid;
		TriggerOnlineDelegates(Object,*ScriptDelegates,&Results);
	}
}

/**
 * Iterates the list of delegates and fires those notifications
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FLiveAsyncPlayers::TriggerDelegates(UObject* Object)
{
	check(Object);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL &&
		ScriptDelegates->Num() > 0)
	{
		// This code relies on these having the same members
		check(sizeof(OnlineSubsystemLive_eventOnUnregisterPlayerComplete_Parms) == sizeof(OnlineSubsystemLive_eventOnRegisterPlayerComplete_Parms));
		OnlineSubsystemLive_eventOnUnregisterPlayerComplete_Parms Results(EC_EventParm);
		Results.SessionName = SessionName;
		Results.bWasSuccessful = GetCompletionCode() == ERROR_SUCCESS ? FIRST_BITFIELD : 0;
		// For each player send the unregister
		for (INT Index = 0; Index < Players.Num(); Index++)
		{
			Results.PlayerID = Players(Index);
			TriggerOnlineDelegates(Object,*ScriptDelegates,&Results);
		}
	}
}

/**
 * Marks the skill in progress flag as false
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncUpdateSessionSkill::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	// Grab the session information by name
	FNamedSession* Session = LiveSubsystem->GetNamedSession(SessionName);
	check(Session && Session->GameSettings);
	Session->GameSettings->bHasSkillUpdateInProgress = FALSE;
	return TRUE;
}

/**
 * After getting the list of files that are to be downloaded, it downloads
 * and merges each INI file in the list
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTMSRead::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	UBOOL bShouldDelete = FALSE;
	// Merge the file results if the read was successful
	if (NextFileToRead >= 0)
	{
		// Add a TMS file to the list and copy the data
		INT AddIndex = FilesRead.AddZeroed();
		// Make sure the read completed ok, otherwise mark the entry as failed
		if (XGetOverlappedExtendedError(&Overlapped) == ERROR_SUCCESS)
		{
			FString FileName = FilesReturned->pItems[NextFileToRead].pwszPathName;
			// Strip off the title specific directories to get just the INI name
			INT Index = FileName.InStr(TEXT("/"),TRUE);
			if (Index != -1)
			{
				FileName = FileName.Right(FileName.Len() - 1 - Index);
			}
			INT FileSize = FilesReturned->pItems[NextFileToRead].dwInstalledSize;
			// Copy the data into the cached buffer
			FilesRead(AddIndex).Data.AddZeroed(FileSize);
			appMemcpy(FilesRead(AddIndex).Data.GetTypedData(),FileBuffer,FileSize);
			debugfLiveSlow(NAME_DevOnline,TEXT("Read TMS file '%s' of size (%d)"),*FileName,FileSize);
		}
		else
		{
			FilesRead(AddIndex).AsyncState = OERS_Failed;
			debugf(NAME_DevOnline,
				TEXT("Failed to read TMS file '%s'"),
				FilesReturned->pItems[NextFileToRead].pwszPathName);
		}
		// Pass in the data that indicates whether the call worked or not
		OnlineSubsystemLive_eventOnReadTitleFileComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = FilesRead(AddIndex).AsyncState == OERS_Done ? FIRST_BITFIELD : 0;
		Parms.Filename = FilesRead(AddIndex).Filename;
		// Use the common method to do the work
		TriggerOnlineDelegates(LiveSubsystem,*ScriptDelegates,&Parms);
	}
	// Move to the next file to read in the enumeration buffer
	NextFileToRead++;
	// If there are more files to read, kick off the next read
	if (NextFileToRead < (INT)FilesReturned->dwNumItemsReturned)
	{
		// Don't allocate a new buffer if the old one is large enough
		if (FileBufferSize < FilesReturned->pItems[NextFileToRead].dwInstalledSize)
		{
			// Free the previous buffer
			if (FileBuffer != NULL)
			{
				delete [] FileBuffer;
				FileBuffer = NULL;
				FileBufferSize = 0;
			}
			FileBufferSize = FilesReturned->pItems[NextFileToRead].dwInstalledSize;
			// Allocate the buffer needed to download the file
			FileBuffer = new BYTE[FileBufferSize];
		}
		// Clear our overlapped so it can be reused
		appMemzero(&Overlapped,sizeof(XOVERLAPPED));
		// Kick off a download of the file to the buffer
		DWORD Result = XStorageDownloadToMemory(0,
			FilesReturned->pItems[NextFileToRead].pwszPathName,
			FileBufferSize,
			FileBuffer,
			sizeof(XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS),
			&FileDownloadResults,
			&Overlapped);
		debugfLiveSlow(NAME_DevOnline,
			TEXT("XStorageDownloadToMemory(0,\"%s\",%d,Buffer,%d,DLResults,Overlapped) returned 0x%08X"),
			FilesReturned->pItems[NextFileToRead].pwszPathName,
			FileBufferSize,
			sizeof(XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS),
			Result);
		if (Result != ERROR_SUCCESS && Result != ERROR_IO_PENDING)
		{
			// Add a TMS file to the list and mark as being in error
			INT AddIndex = FilesRead.AddZeroed();
			FilesRead(AddIndex).AsyncState = OERS_Failed;
			bShouldDelete = TRUE;
		}
	}
	else
	{
		if (FilesReturned->dwNumItemsReturned == 0)
		{
			// Failed to read any files, so indicate a failure
			OnlineSubsystemLive_eventOnReadTitleFileComplete_Parms Parms(EC_EventParm);
			Parms.bWasSuccessful = 0;
			Parms.Filename = TEXT("");
			// Use the common method to do the work
			TriggerOnlineDelegates(LiveSubsystem,*ScriptDelegates,&Parms);
		}
		bShouldDelete = TRUE;
	}
	return bShouldDelete;
}

/**
 * Reads the results from the last achievement async read request 
 * and also submits the next one.
 *
 * @param LiveSubsystem the object to add the data to
 *
 * @return Result code from the achievement read
 */
DWORD FLiveAsyncTaskReadAchievements::ReadAchievementData(UOnlineSubsystemLive* LiveSubsystem)
{
	// Figure out if we are at the end of the list or not
	DWORD Result = XGetOverlappedExtendedError(&Overlapped);
	// If the task completed ok, then parse the results and start another enumeration fetch
	if (Result == ERROR_SUCCESS)
	{
		FLiveAsyncTaskDataReadAchievements* Data = (FLiveAsyncTaskDataReadAchievements*)TaskData;
		FCachedAchievements& Cached = LiveSubsystem->GetCachedAchievements(
			Data->GetPlayerIndex(),
			Data->GetTitleId());

		XACHIEVEMENT_DETAILS* Details = Data->GetDetailsBuffer();
		DWORD ItemsEnumerated = 0;
		XGetOverlappedResult(&Overlapped,&ItemsEnumerated,FALSE);
		// Add each achievement that was enumerated
		for (DWORD AchIndex = 0; AchIndex < ItemsEnumerated; AchIndex++)
		{
			INT AddIndex = Cached.Achievements.AddZeroed();
			// Add the new details to the list
			FAchievementDetails& AchDetails = Cached.Achievements(AddIndex);
			AchDetails.Id = Details[AchIndex].dwId;
			AchDetails.AchievementName = Details[AchIndex].pwszLabel;
			AchDetails.Description = Details[AchIndex].pwszDescription;
			AchDetails.HowTo = Details[AchIndex].pwszUnachieved;
			AchDetails.GamerPoints = Details[AchIndex].dwCred;
			// Check the flags to see how it was earned (or not)
			if (!(Details[AchIndex].dwFlags & XACHIEVEMENT_DETAILS_SHOWUNACHIEVED))
			{
				AchDetails.bIsSecret = TRUE;
			}
			if (Details[AchIndex].dwFlags & XACHIEVEMENT_DETAILS_ACHIEVED_ONLINE)
			{
				AchDetails.bWasAchievedOnline = TRUE;
				// Get the date that it happened
				SYSTEMTIME SysTime;
				FileTimeToSystemTime(&Details[AchIndex].ftAchieved,&SysTime);
				// Now copy the date info
				AchDetails.MonthEarned = SysTime.wMonth;
				AchDetails.DayEarned = SysTime.wDay;
				AchDetails.YearEarned = SysTime.wYear - 2000;
				AchDetails.DayOfWeekEarned = SysTime.wDayOfWeek;
			}
			if ((Details[AchIndex].dwFlags & XACHIEVEMENT_DETAILS_ACHIEVED) && !AchDetails.bWasAchievedOnline)
			{
				AchDetails.bWasAchievedOffline = TRUE;
			}
			if (Data->GetShouldReadImages())
			{
				// add request to read the image for the cached achievement
				FAchievementImageRequest ImageId(AddIndex, Details[AchIndex].dwImageId);
				ImagesToRead.Push(ImageId);
			}
		}
		// Zero between uses or results will be incorrect
		appMemzero(&Overlapped,sizeof(XOVERLAPPED));
		// Have it try to read the next item in the list
		Result = XEnumerate(Data->GetHandle(),
			Data->GetBuffer(),
			Data->GetBufferSize(),
			0,
			&Overlapped);
	}
	return Result;
}

/**
 * Reads the results from the last achievement image async read request
 * and also submits the next one.
 *
 * @param LiveSubsystem the object to add the data to
 *
 * @return Result code from the achievement read
 */
DWORD FLiveAsyncTaskReadAchievements::ReadAchievementImage(UOnlineSubsystemLive* LiveSubsystem)
{
	DWORD Result = -1;
	
	FLiveAsyncTaskDataReadAchievements* Data = (FLiveAsyncTaskDataReadAchievements*)TaskData;
	// Get the cached entry for the current player/title
	FCachedAchievements& Cached = LiveSubsystem->GetCachedAchievements(
		Data->GetPlayerIndex(),
		Data->GetTitleId());

	// Only continue if there is anything left on the stack
	if (ImagesToRead.Num() > 0)
	{
		// Top of the stack has the current read request that is in flight
		const FAchievementImageRequest& LastImageRequest = ImagesToRead.Top();
		if (LastImageRequest.bSubmitted)
		{
			// Data was read into the first mip of the cached 2D texture
			UTexture2D* NewTexture = Cast<UTexture2D>(Cached.TempImage);
			if (NewTexture != NULL)
			{
				// Mip data has been read in so unlock it
				NewTexture->Mips(0).Data.Unlock();
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Warning, missing temp image texture for achievement read."));
			}
			// No longer need to keep a ref to the cached object
			Cached.TempImage = NULL;
			// Determine if the read was successful
			DWORD LastResult = XGetOverlappedExtendedError(&Overlapped);
			if (LastResult == ERROR_SUCCESS)
			{
				// Assign texture for this achievement. Image entry should be null until this point.
				FAchievementDetails& LastAchDetails = Cached.Achievements(LastImageRequest.AchievementIdx);
				LastAchDetails.Image = NewTexture;
				// Update the render resource for it
				NewTexture->UpdateResource();				
			}			
			// Done with this request so remove from stack
			ImagesToRead.Pop();
		}
		// If the previous read succeeded (or there was not previous read) 
		// handle the next image read request
		while (ImagesToRead.Num() > 0)
		{
			// Top of the stack has the next read request
			FAchievementImageRequest& NextImageRequest = ImagesToRead.Top();
			check(!NextImageRequest.bSubmitted);
			// Make sure the achievement index is a valid entry
			if (Cached.Achievements.IsValidIndex(NextImageRequest.AchievementIdx))
			{	
				const FAchievementDetails& NextAchDetails = Cached.Achievements(NextImageRequest.AchievementIdx);
				// Create a 2D texture for the achievement bitmap
				FString ImageName = FString("AchievementImage_") + FString(NextAchDetails.AchievementName).Replace(TEXT(" "),TEXT("_"));	
				UTexture2D* NewTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), INVALID_OBJECT, FName(*ImageName));
				// disable compression, tiling, sRGB, and streaming for the texture
				NewTexture->CompressionNone			= TRUE;
				NewTexture->CompressionSettings		= TC_Default;
				NewTexture->MipGenSettings			= TMGS_NoMipmaps;
				NewTexture->CompressionNoAlpha		= TRUE;
				NewTexture->DeferCompression		= FALSE;
				NewTexture->bNoTiling				= TRUE;
				NewTexture->SRGB					= FALSE;
				NewTexture->NeverStream				= TRUE;
				NewTexture->LODGroup				= TEXTUREGROUP_UI;	
				NewTexture->Init(Data->GetImageHeight(),Data->GetImageHeight(),PF_A8R8G8B8);
				// Only the first mip level is used
				check(NewTexture->Mips.Num() > 0);
				// Mip 0 is locked for the duration of the read request
				BYTE* MipData = (BYTE*)NewTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);
				if (MipData)
				{
					// Calculate the row stride for the texture
					const INT TexStride = (Data->GetImageHeight() / GPixelFormats[PF_A8R8G8B8].BlockSizeY) * GPixelFormats[PF_A8R8G8B8].BlockBytes;
					// Zero between uses or results will be incorrect
					appMemzero(&Overlapped,sizeof(XOVERLAPPED));
					// Read the achievment image for the player index and title id into the first mip level
					Result = XUserReadAchievementPicture(
						Data->GetPlayerIndex(),
						Data->GetTitleId(),
						NextImageRequest.ImageId,
						MipData,
						TexStride,
						Data->GetImageHeight(),
						&Overlapped);
					debugfLiveSlow(NAME_DevLive,TEXT("XUserReadAchievementPicture Player=%d Title=%d returned 0x%08X"),
						Data->GetPlayerIndex(),
						Data->GetTitleId(),
						Result);
					if (Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING)
					{
						// Flag if read request was submitted successfully 
						NextImageRequest.bSubmitted = TRUE;
						// keep a ref to the object so that it won't get GC'd during the async read task
						Cached.TempImage = NewTexture;
					}
				}
				else
				{
					// Couldn't lock the mip
					NewTexture->Mips(0).Data.Unlock();
				}
			}
			if (NextImageRequest.bSubmitted)
			{
				// found a valid entry so let it run
				break;
			}
			else
			{
				// remove invalid entries and try to find the next valid one
				ImagesToRead.Pop();
			}
		}
	}
	// Return the results from the submitted read request 
	return Result;
}

/**
 * Parses the read results and continues the read if needed
 *
 * @param LiveSubsystem the object to add the data to
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskReadAchievements::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	FLiveAsyncTaskDataReadAchievements* Data = (FLiveAsyncTaskDataReadAchievements*)TaskData;

	if (CurrentReadType == AchievementReadTask_Default)
	{			
		// handle results from achievement read task
		DWORD Result = ReadAchievementData(LiveSubsystem);
		// move to the next read task
		if (Result != ERROR_SUCCESS && Result != ERROR_IO_PENDING)
		{
			if (Data->GetShouldReadImages())
			{
				CurrentReadType = AchievementReadTask_Images;
			}
			else
			{
				CurrentReadType = AchievementReadTask_Done;
			}
		}
	}
	if (CurrentReadType == AchievementReadTask_Images)
	{
		// handle results from achievement image read task
		DWORD Result = ReadAchievementImage(LiveSubsystem);
		// move to the next read task
		if (Result != ERROR_SUCCESS && Result != ERROR_IO_PENDING)
		{
			CurrentReadType = AchievementReadTask_Done;
		}
	}
	if (CurrentReadType == AchievementReadTask_Done)
	{		
		FCachedAchievements& Cached = LiveSubsystem->GetCachedAchievements(
			Data->GetPlayerIndex(),
			Data->GetTitleId());

		// Done reading, need to mark as finished
		Cached.ReadState = OERS_Done;
		// Clear out reference to any temp image data that may have been created
		Cached.TempImage = NULL;
	}
	
	// When this is true, there is no more data left to read
	return CurrentReadType == AchievementReadTask_Done;
}

/**
 * Iterates the list of delegates and fires those notifications
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FLiveAsyncTaskReadAchievements::TriggerDelegates(UObject* Object)
{
	check(Object);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL)
	{
		OnlineSubsystemLive_eventOnReadAchievementsComplete_Parms Parms(EC_EventParm);
		FLiveAsyncTaskDataReadAchievements* Data = (FLiveAsyncTaskDataReadAchievements*)TaskData;
		Parms.TitleId = Data->GetTitleId();
		// Use the common method to do the work
		TriggerOnlineDelegates(Object,*ScriptDelegates,&Parms);
	}
}

/**
 * If the skill read completes successfully, it then triggers the requested search
 * If it fails, it uses the search delegates to notify the game code
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncReadPlayerSkillForSearch::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	XUSER_STATS_READ_RESULTS* ReadResults = GetReadBuffer();
	INT PlayerCount = Search->ManualSkillOverride.Players.Num();
	DWORD Result = XGetOverlappedExtendedError(&Overlapped);
	// If the skill read completed ok, then parse the results and kick of the
	// session search with them
	if (Result == ERROR_SUCCESS &&
		// Make sure we have a skill entry for each player
		ReadResults->pViews->dwNumRows == PlayerCount)
	{
		// Preallocate our space for exactly the number we need (no slack)
		Search->ManualSkillOverride.Mus.Empty(PlayerCount);
		Search->ManualSkillOverride.Sigmas.Empty(PlayerCount);
		// Each row represents a player
		for (INT Index = 0; Index < PlayerCount; Index++)
		{
			// The Mu is in the first column
			DOUBLE Mu = ReadResults->pViews[0].pRows[Index].pColumns[0].Value.dblData;
			// The Sigma is in the second column
			DOUBLE Sigma = ReadResults->pViews[0].pRows[Index].pColumns[1].Value.dblData;
			// Make sure the player isn't new to the leaderboard
			if (Mu == 0.0 && Sigma == 0.0)
			{
				// Default to middle of the range with 100% uncertainty
				Mu = 3.0;
				Sigma = 1.0;
			}
			Search->ManualSkillOverride.Mus.AddItem(Mu);
			Search->ManualSkillOverride.Sigmas.AddItem(Sigma);
		}
		// Now that we have the skill data, kick of the search
		LiveSubsystem->FindOnlineGames(LocalUserNum,Search);
	}
	else
	{
		// Set the delegates so they'll notify the game code
		ScriptDelegates = &LiveSubsystem->FindOnlineGamesCompleteDelegates;
	}
	return TRUE;
}

/**
 * Starts the async process by binding the content package
 */
UBOOL FSaveGameDataAsyncTask::BindContent(void)
{
	DWORD Return = E_FAIL;
	// Mark this as having an async task running so nothing tries to change it
	SaveGameMapping.ReadWriteState = OERS_InProgress;
	// Make a unique binding for the package
	ContentPackageBinding = FString::Printf(TEXT("%s_%d"),
		*SaveGame.ContentPath,
		(DWORD)UserNum);
#if CONSOLE
	// Copy our save game data into something Live understands
	CopyOnlineSaveGameToContentData(SaveGame,&ContentData);
	// Only bind on the first opening of the content bundle
	if (SaveGame.BindRefCount == 0)
	{
		// Bind to the content package
		Return = XContentCreate(UserNum,
			TCHAR_TO_ANSI(*ContentPackageBinding),
			&ContentData,
			XCONTENTFLAG_OPENALWAYS,
			NULL,
			NULL,
			&Overlapped);
	}
	else
	{
		Return = ERROR_IO_PENDING;
	}
#endif
	// Always increment our ref count so that we bind/unbind things when appropriate
	SaveGame.BindRefCount++;
	CurrentState = BindingPackage;
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Cleans up any handles or memory used by reading the data
 */
void FSaveGameDataAsyncTask::Cleanup(void)
{
	// Close the file handle if open
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(FileHandle);
		FileHandle = INVALID_HANDLE_VALUE;
	}
	SaveGame.BindRefCount--;
#if CONSOLE
	// Only unbind on the last closing of the content bundle
	if (SaveGame.BindRefCount == 0 &&
		ContentPackageBinding.Len())
	{
		// Now close out the content package binding
		XContentClose(TCHAR_TO_ANSI(*ContentPackageBinding),&Overlapped);
		// Clear this since it is no longer bound
		SaveGame.bIsBound = FALSE;
	}
#endif			
	CurrentState = ClosingPackage;
}

/**
 * Pumps the reading process and cleans up if we are done
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FSaveGameDataAsyncTask::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	UBOOL bIsDone = CurrentState == ClosingPackage;
	// Based upon which state we are in figure out our next step
	switch (CurrentState)
	{
		case BindingPackage:
		{
			// Open the file and transition to the reading state
			OpenFile();
			break;
		}
		case WaitingForFileIO:
		{
			// Poll the overlapped for complete IO
			CheckFileIoCompletion();
			break;
		}
		// If we get to this state, we are done no matter what the results of the overlapped indicate
		case ClosingPackage:
		{
			// Don't overwrite an error state
			if (SaveGameMapping.ReadWriteState != OERS_Failed)
			{
				SaveGameMapping.ReadWriteState = OERS_Done;
			}
			break;
		}
	}
	return bIsDone;
}

/** Opens the underlying file for reading */
void FSaveGameDataAsyncTask::OpenFile(void)
{
	check(FileHandle == INVALID_HANDLE_VALUE);
	// Assume that we failed to start the file io
	UBOOL bStartOk = FALSE;
	// See if this player owns the file in question
	SaveGame.bIsValid = SaveGame.bIsValid || IsContentValid();
	UBOOL bIsValid = SaveGame.bIsBound || SaveGame.bIsValid;
	// Make sure the content is valid before opening the file
	if (bIsValid)
	{
		SaveGame.bIsBound = FIRST_BITFIELD;
		// See if the binding succeeded properly or not
		DWORD Result = SaveGame.BindRefCount > 1 ? ERROR_SUCCESS : XGetOverlappedExtendedError(&Overlapped);
		if (Result == ERROR_SUCCESS)
		{
			// Get a file handle to the file with overlapped IO enabled
			FileHandle = CreateFileA(TCHAR_TO_ANSI(*(ContentPackageBinding + TEXT(":\\") + SaveGameMapping.InternalFileName)),
				GENERIC_READ | GENERIC_WRITE,
				0,
				NULL,
				OPEN_ALWAYS,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
				NULL);
			if (FileHandle != INVALID_HANDLE_VALUE)
			{
				CurrentState = WaitingForFileIO;
				// Let child initiate the file io
				bStartOk = StartFileIO();
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Content bind failed with error code 0x%08X"),Result);
		}
	}
	if (bStartOk == FALSE)
	{
		debugf(NAME_DevOnline,TEXT("Failed to start async (%s) of save game file %s with error %d"),AsyncTaskName,*SaveGameMapping.InternalFileName,GetLastError());
		Cleanup();
		SaveGameMapping.SaveGameData.Empty();
	}
}

/** Polls the file io for completion */
void FSaveGameDataAsyncTask::CheckFileIoCompletion(void)
{
	// See if the file io has completed
	if (HasOverlappedIoCompleted(&FileOverlapped))
	{
		DWORD ByteCount = 0;
		GetOverlappedResult(FileHandle,&FileOverlapped,&ByteCount,TRUE);
		// Make sure all of the file data was read/written correctly
		if (ByteCount != DesiredByteCount)
		{
			debugf(NAME_DevOnline,TEXT("Failed to perform async (%s) for save game file %s"),AsyncTaskName,*SaveGameMapping.InternalFileName);
			SaveGameMapping.SaveGameData.Empty();
			SaveGameMapping.ReadWriteState = OERS_Failed;
		}
		// We're done so clean up the file
		Cleanup();
	}
}

/**
 * Sends the file completion status to the list of delegates
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FSaveGameDataAsyncTask::TriggerDelegates(UObject* Object)
{
	check(Object);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL)
	{
		// The code following this relies upon these structures being the same
		check(sizeof(OnlineSubsystemLive_eventOnReadSaveGameDataComplete_Parms) == sizeof(OnlineSubsystemLive_eventOnWriteSaveGameDataComplete_Parms));
		OnlineSubsystemLive_eventOnReadSaveGameDataComplete_Parms Parms(EC_EventParm);
		// Fill out all of the data for the file operation that completed
		Parms.LocalUserNum = UserNum;
		Parms.bWasSuccessful = SaveGameMapping.ReadWriteState == OERS_Done ? FIRST_BITFIELD : 0;
		Parms.DeviceID = SaveGame.DeviceID;
		Parms.FriendlyName = SaveGame.FriendlyName;
		Parms.Filename = SaveGame.Filename;
		Parms.SaveFileName = SaveGameMapping.InternalFileName;
		// Use the common method to fire the delegates
		TriggerOnlineDelegates(Object,*ScriptDelegates,&Parms);
	}
}

/**
 * Kicks off the async read on the file
 *
 * @return true if the file io was started ok, false otherwise
 */
UBOOL FReadSaveGameDataAsyncTask::StartFileIO(void)
{
	// Get the file size so we can pre-allocate our buffer
	DesiredByteCount = GetFileSize(FileHandle,NULL);
	// Allocate the buffer to read into
	SaveGameMapping.SaveGameData.Empty(DesiredByteCount);
	SaveGameMapping.SaveGameData.Add(DesiredByteCount);
	// Zero the overlapped for first use
	appMemzero(&FileOverlapped,sizeof(OVERLAPPED));
	// Perform the async read
	return ReadFile(FileHandle,
		SaveGameMapping.SaveGameData.GetTypedData(),
		DesiredByteCount,
		NULL,
		&FileOverlapped) ||
		GetLastError() == ERROR_IO_PENDING;
}

/**
 * Checks to see if the content package is valid before opening the internal file
 *
 * @return true if the package is valid, false otherwise
 */
UBOOL FReadSaveGameDataAsyncTask::IsContentValid(void)
{
	BOOL bUserIsCreator = FALSE;
#if CONSOLE
	// See if this player created it
	XContentGetCreator(UserNum,&ContentData,&bUserIsCreator,NULL,NULL);
	// If the file doesn't exist, then this user will create it
	DWORD FileAttributes = GetFileAttributesA(TCHAR_TO_ANSI(*(ContentPackageBinding + TEXT(":\\") + SaveGameMapping.InternalFileName)));
	bUserIsCreator = FileAttributes == (DWORD)-1 ? TRUE : bUserIsCreator;
#endif
	return bUserIsCreator;
}

/**
 * Kicks off the async write on the file
 *
 * @return true if the file io was started ok, false otherwise
 */
UBOOL FWriteSaveGameDataAsyncTask::StartFileIO(void)
{
	// We're going to write the full buffer out
	DesiredByteCount = SaveGameMapping.SaveGameData.Num();
	// Zero the overlapped for first use
	appMemzero(&FileOverlapped,sizeof(OVERLAPPED));
	// Perform the async read
	return WriteFile(FileHandle,
		SaveGameMapping.SaveGameData.GetTypedData(),
		DesiredByteCount,
		NULL,
		&FileOverlapped) ||
		GetLastError() == ERROR_IO_PENDING;
}

/**
 * Starts the async process by binding the content package
 */
UBOOL FReadCrossTitleSaveGameDataAsyncTask::BindContent(void)
{
	DWORD Return = E_FAIL;
	// Mark this as having an async task running so nothing tries to change it
	SaveGameMapping.ReadWriteState = OERS_InProgress;
	// Make a unique binding for the package
	ContentPackageBinding = FString::Printf(TEXT("%s_%d"),
		*SaveGame.ContentPath,
		(DWORD)UserNum);
#if CONSOLE
	// Copy our save game data into something Live understands
	CopyOnlineSaveGameToContentData(SaveGame,&ContentData);
	// Only bind on the first opening of the content bundle
	if (SaveGame.BindRefCount == 0)
	{
		ULARGE_INTEGER ContentSize = {0};
		// Bind to the content package
		Return = XContentCrossTitleCreate(UserNum,
			TCHAR_TO_ANSI(*ContentPackageBinding),
			&ContentData,
			XCONTENTFLAG_OPENEXISTING,
			NULL,
			NULL,
			0,
			ContentSize,
			&Overlapped);
	}
	else
	{
		Return = ERROR_IO_PENDING;
	}
#endif
	// Always increment our ref count so that we bind/unbind things when appropriate
	SaveGame.BindRefCount++;
	CurrentState = BindingPackage;
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Cleans up any handles or memory used by reading the data
 */
void FReadCrossTitleSaveGameDataAsyncTask::Cleanup(void)
{
	// Close the file handle if open
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(FileHandle);
		FileHandle = INVALID_HANDLE_VALUE;
	}
	SaveGame.BindRefCount--;
#if CONSOLE
	// Only unbind on the last closing of the content bundle
	if (SaveGame.BindRefCount == 0 &&
		ContentPackageBinding.Len())
	{
		// Now close out the content package binding
		XContentClose(TCHAR_TO_ANSI(*ContentPackageBinding),&Overlapped);
		// Clear this since it is no longer bound
		SaveGame.bIsBound = FALSE;
	}
#endif			
	CurrentState = ClosingPackage;
}

/**
 * Pumps the reading process and cleans up if we are done
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FReadCrossTitleSaveGameDataAsyncTask::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	UBOOL bIsDone = CurrentState == ClosingPackage;
	// Based upon which state we are in figure out our next step
	switch (CurrentState)
	{
		case BindingPackage:
		{
			// Open the file and transition to the reading state
			OpenFile();
			break;
		}
		case WaitingForFileIO:
		{
			// Poll the overlapped for complete IO
			CheckFileIoCompletion();
			break;
		}
		// If we get to this state, we are done no matter what the results of the overlapped indicate
		case ClosingPackage:
		{
			// Don't overwrite an error state
			if (SaveGameMapping.ReadWriteState != OERS_Failed)
			{
				SaveGameMapping.ReadWriteState = OERS_Done;
			}
			break;
		}
	}
	return bIsDone;
}

/** Opens the underlying file for reading */
void FReadCrossTitleSaveGameDataAsyncTask::OpenFile(void)
{
	check(FileHandle == INVALID_HANDLE_VALUE);
	// Assume that we failed to start the file io
	UBOOL bStartOk = FALSE;
	// See if this player owns the file in question
	SaveGame.bIsValid = SaveGame.bIsValid || IsContentValid();
	UBOOL bIsValid = SaveGame.bIsBound || SaveGame.bIsValid;
	// Make sure the content is valid before opening the file
	if (bIsValid)
	{
		SaveGame.bIsBound = FIRST_BITFIELD;
		// See if the binding succeeded properly or not
		DWORD Result = SaveGame.BindRefCount > 1 ? ERROR_SUCCESS : XGetOverlappedExtendedError(&Overlapped);
		if (Result == ERROR_SUCCESS)
		{
			// Get a file handle to the file with overlapped IO enabled
			FileHandle = CreateFileA(TCHAR_TO_ANSI(*(ContentPackageBinding + TEXT(":\\") + SaveGameMapping.InternalFileName)),
				GENERIC_READ | GENERIC_WRITE,
				0,
				NULL,
				OPEN_ALWAYS,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
				NULL);
			if (FileHandle != INVALID_HANDLE_VALUE)
			{
				CurrentState = WaitingForFileIO;
				// Let child initiate the file io
				bStartOk = StartFileIO();
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Content bind failed with error code 0x%08X"),Result);
		}
	}
	if (bStartOk == FALSE)
	{
		debugf(NAME_DevOnline,TEXT("Failed to start async (%s) of save game file %s with error %d"),AsyncTaskName,*SaveGameMapping.InternalFileName,GetLastError());
		Cleanup();
		SaveGameMapping.SaveGameData.Empty();
	}
}

/** Polls the file io for completion */
void FReadCrossTitleSaveGameDataAsyncTask::CheckFileIoCompletion(void)
{
	// See if the file io has completed
	if (HasOverlappedIoCompleted(&FileOverlapped))
	{
		DWORD ByteCount = 0;
		GetOverlappedResult(FileHandle,&FileOverlapped,&ByteCount,TRUE);
		// Make sure all of the file data was read/written correctly
		if (ByteCount != DesiredByteCount)
		{
			debugf(NAME_DevOnline,TEXT("Failed to perform async (%s) for save game file %s"),AsyncTaskName,*SaveGameMapping.InternalFileName);
			SaveGameMapping.SaveGameData.Empty();
			SaveGameMapping.ReadWriteState = OERS_Failed;
		}
		// We're done so clean up the file
		Cleanup();
	}
}

/**
 * Sends the file completion status to the list of delegates
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FReadCrossTitleSaveGameDataAsyncTask::TriggerDelegates(UObject* Object)
{
	check(Object);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL)
	{
		OnlineSubsystemLive_eventOnReadCrossTitleSaveGameDataComplete_Parms Parms(EC_EventParm);
		// Fill out all of the data for the file operation that completed
		Parms.LocalUserNum = UserNum;
		Parms.bWasSuccessful = SaveGameMapping.ReadWriteState == OERS_Done ? FIRST_BITFIELD : 0;
		Parms.DeviceID = SaveGame.DeviceID;
		Parms.TitleId = SaveGame.TitleId;
		Parms.FriendlyName = SaveGame.FriendlyName;
		Parms.Filename = SaveGame.Filename;
		Parms.SaveFileName = SaveGameMapping.InternalFileName;
		// Use the common method to fire the delegates
		TriggerOnlineDelegates(Object,*ScriptDelegates,&Parms);
	}
}

/**
 * Kicks off the async read on the file
 *
 * @return true if the file io was started ok, false otherwise
 */
UBOOL FReadCrossTitleSaveGameDataAsyncTask::StartFileIO(void)
{
	// Get the file size so we can pre-allocate our buffer
	DesiredByteCount = GetFileSize(FileHandle,NULL);
	// Allocate the buffer to read into
	SaveGameMapping.SaveGameData.Empty(DesiredByteCount);
	SaveGameMapping.SaveGameData.Add(DesiredByteCount);
	// Zero the overlapped for first use
	appMemzero(&FileOverlapped,sizeof(OVERLAPPED));
	// Perform the async read
	return ReadFile(FileHandle,
		SaveGameMapping.SaveGameData.GetTypedData(),
		DesiredByteCount,
		NULL,
		&FileOverlapped) ||
		GetLastError() == ERROR_IO_PENDING;
}

/**
 * Checks to see if the content package is valid before opening the internal file
 *
 * @return true if the package is valid, false otherwise
 */
UBOOL FReadCrossTitleSaveGameDataAsyncTask::IsContentValid(void)
{
#if CONSOLE
	// The file must exist
	DWORD FileAttributes = GetFileAttributesA(TCHAR_TO_ANSI(*(ContentPackageBinding + TEXT(":\\") + SaveGameMapping.InternalFileName)));
	return FileAttributes != (DWORD)-1;
#endif
	return FALSE;
}

/**
 * Live specific initialization. Sets all the interfaces to point to this
 * object as it implements them all
 *
 * @return always returns TRUE
 */
UBOOL UOnlineSubsystemLive::Init(void)
{
	Super::Init();
	// Set the player interface to be the same as the object
	eventSetPlayerInterface(this);
	// Set the Live specific player interface to be the same as the object
	eventSetPlayerInterfaceEx(this);
	// Set the system interface to be the same as the object
	eventSetSystemInterface(this);
	// Set the game interface to be the same as the object
	eventSetGameInterface(this);
	// Set the content interface to be the same as the object
	eventSetContentInterface(this);
	// Set the stats reading/writing interface
	eventSetStatsInterface(this);
	// Create the voice engine and if successful register the interface
	VoiceEngine = appCreateVoiceInterface(MaxLocalTalkers,MaxRemoteTalkers,
		bIsUsingSpeechRecognition);
	// Set the voice interface to this object
	eventSetVoiceInterface(this);
	if (bShouldUseMcp)
	{
		UOnlineNewsInterfaceMcp* NewsObject = ConstructObject<UOnlineNewsInterfaceMcp>(UOnlineNewsInterfaceMcp::StaticClass(),this);
		eventSetNewsInterface(NewsObject);
		UOnlineTitleFileDownloadMcpLive* TitleFileObject = ConstructObject<UOnlineTitleFileDownloadMcpLive>(UOnlineTitleFileDownloadMcpLive::StaticClass(),this);
		eventSetTitleFileInterface(TitleFileObject);
	}
	else
	{
		eventSetTitleFileInterface(this);
	}
#if CONSOLE
	// Wire up the party chat interface
	ULivePartyChat* ChatObject = ConstructObject<ULivePartyChat>(ULivePartyChat::StaticClass(),this);
	eventSetPartyChatInterface(ChatObject);
#endif
	// Set the interface for posting to social networking sites
	eventSetSocialInterface(this);

	// Check each controller for a logged in player, DLC, etc.
	InitLoginState();
	// Register the notifications we are interested in
	NotificationHandle = XNotifyCreateListener(XNOTIFY_SYSTEM | XNOTIFY_LIVE | XNOTIFY_FRIENDS);
	if (NotificationHandle == NULL)
	{
		debugf(NAME_DevOnline,TEXT("Failed to create Live notification listener"));
	}
	// Use the unique build id to prevent incompatible builds from colliding
	LanGameUniqueId = GetBuildUniqueId();
	// Tell presence how many we want
	XPresenceInitialize(MAX_FRIENDS);
	// Set the default toast location
	SetNetworkNotificationPosition(CurrentNotificationPosition);
	// Set the default log level
	SetDebugSpewLevel(DebugLogLevel); 
	// Start any LSP resolves that we can
	InitLspResolves();
#if WITH_PANORAMA
	return NotificationHandle != NULL && InitG4WLive();
#else
	return NotificationHandle != NULL;
#endif
}

/**
 * Initializes the various sign in state
 */
void UOnlineSubsystemLive::InitLoginState(void)
{
	XINPUT_CAPABILITIES InputCaps;
	appMemzero(&InputCaps,sizeof(XINPUT_CAPABILITIES));
	// Cache a copy of the current login state
	ReadLoginState(LastLoginState);
	// Iterate controller indices and update sign in state & DLC
	for (INT UserIndex = 0; UserIndex < MAX_LOCAL_PLAYERS; UserIndex++)
	{
		// Wipe their profile cache
		ProfileCache[UserIndex].Profile = NULL;
		PlayerStorageCacheLocal[UserIndex].PlayerStorage = NULL;
		// Init the base controller connection state
		if (XInputGetCapabilities(UserIndex,XINPUT_FLAG_GAMEPAD,&InputCaps) == ERROR_SUCCESS)
		{
			LastInputDeviceConnectedMask |= 1 << UserIndex;
		}
	}
}

/**
 * Reads the signin state for all players
 *
 * @param LoginState login state structs to fill in with the current state
 */
void UOnlineSubsystemLive::ReadLoginState(FCachedLoginState LoginState[4])
{
	appMemzero(LoginState,sizeof(FCachedLoginState) * 4);
	// Iterate controller indices and update sign in state & DLC
	for (INT UserIndex = 0; UserIndex < MAX_LOCAL_PLAYERS; UserIndex++)
	{
		// Cache the last logged in xuid for signin comparison
		if (GetUserXuid(UserIndex,&(XUID&)LoginState[UserIndex].OnlineXuid) == ERROR_SUCCESS)
		{
			XUSER_SIGNIN_INFO SigninInfo = {0};
			// Cache the offline xuid to detect sign in changes
			if (XUserGetSigninInfo(UserIndex,
				XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY,
				&SigninInfo) == ERROR_SUCCESS)
			{
				(XUID&)LoginState[UserIndex].OfflineXuid = SigninInfo.xuid;
			}
		}
		// Cache their last login status for comparisons
		LoginState[UserIndex].LoginStatus = GetLoginStatus(UserIndex);
	}
}

#if WITH_PANORAMA
/**
 * Initializes the G4W Live specific features
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::InitG4WLive(void)
{
	extern UBOOL GIsUsingPanorama;
	// Don't try to set these if it failed to init (deadlocks)
	if (GIsUsingPanorama)
	{
#if !SHIPPING_PC_GAME
		XLiveSetDebugLevel(XLIVE_DEBUG_LEVEL_INFO,NULL);
#else
		XLiveSetDebugLevel(XLIVE_DEBUG_LEVEL_OFF,NULL);
#endif
	}
	return TRUE;
}

/**
 * Checks the results of the signin operation and shuts down the server if
 * it fails
 *
 * @param LiveSubsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskSignin::ProcessAsyncResults(UOnlineSubsystemLive*)
{
	DWORD Result = GetCompletionCode();
	if (Result != ERROR_SUCCESS)
	{
		// If this is a dedicated server, bail out
		if (GIsServer && GIsUCC)
		{
			debugf(NAME_Error,
				TEXT("Unable to sign in using the specified credentials (error 0x%08X), exiting"),
				Result);
			appRequestExit(0);
		}
	}
	return TRUE;
}
#endif

/**
 * Called from the engine shutdown code to allow the Live to cleanup. Also, this
 * version blocks until all async tasks are complete before returning.
 */
void UOnlineSubsystemLive::Exit(void)
{
	// While there are any outstanding tasks, block so that file writes, etc. complete
	while (AsyncTasks.Num())
	{
		TickAsyncTasks(0.1f,AsyncTasks,this,this);
		appSleep(0.1f);
	}
	// Close our notification handle
	if (NotificationHandle)
	{
		XCloseHandle(NotificationHandle);
		NotificationHandle = NULL;
	}
#if WITH_PANORAMA
	// Tell Panorama to shut down
	XLiveUnInitialize();
#endif
}

/**
 * Checks for new Live notifications and passes them out to registered delegates
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UOnlineSubsystemLive::Tick(FLOAT DeltaTime)
{
	// Tick sign in notifications if pending
	if (bIsCountingDownSigninNotification)
	{
		SigninCountDownCounter -= DeltaTime;
		if (SigninCountDownCounter <= 0.f)
		{
			ProcessSignInNotification(NULL);
		}
	}
	DWORD Notification = 0;
	ULONG_PTR Data = NULL;
	// Check Live for notification events
	while (XNotifyGetNext(NotificationHandle,0,&Notification,&Data))
	{
		// Now process the event
		ProcessNotification(Notification,Data);
	}
	// Process any invites that occured at start up and are pending
	TickDelayedInvites();
	// Now tick any outstanding async tasks
	TickAsyncTasks(DeltaTime,AsyncTasks,this,this);
	// Tick any tasks needed for LAN support
	TickLanTasks(DeltaTime);
	// Tick voice processing
	TickVoice(DeltaTime);
	// Clean up any resolves that are done
	TickSecureAddressCache(DeltaTime);
}

/**
 * Processes a notification that was returned during polling
 *
 * @param Notification the notification event that was fired
 * @param Data the notification specifc data
 */
void UOnlineSubsystemLive::ProcessNotification(DWORD Notification,ULONG_PTR Data)
{
	switch (Notification)
	{
		case XN_SYS_SIGNINCHANGED:
		{
			// Second notification should override the first according to the XDK FAQ
			if (bIsCountingDownSigninNotification)
			{
				ProcessSignInNotification(Data);
			}
			else
			{
				// Start the ticking of the count down timer to work around an XDK bug
				bIsCountingDownSigninNotification = TRUE;
				SigninCountDownCounter = SigninCountDownDelay;
				// Do the resolves early
				InitLspResolves();
			}
			break;
		}
		case XN_SYS_MUTELISTCHANGED:
		{
			TriggerOnlineDelegates(this,MutingChangeDelegates,NULL);
			// Have the voice code re-evaluate its mute settings
			ProcessMuteChangeNotification();
			break;
		}
		case XN_FRIENDS_FRIEND_ADDED:
		case XN_FRIENDS_FRIEND_REMOVED:
		case XN_FRIENDS_PRESENCE_CHANGED:
		{
			// Per user notification of friends change
			TriggerOnlineDelegates(this,FriendsCache[(DWORD)Data].FriendsChangeDelegates,NULL);
			break;
		}
		case XN_SYS_UI:
		{
			// Data will be non-zero if opening
			UBOOL bIsOpening = Data != 0;
			ProcessExternalUINotification(bIsOpening);
#if WITH_PANORAMA
			extern UBOOL GIsPanoramaUIOpen;
			// Allow the guide to pause input processing
			GIsPanoramaUIOpen = bIsOpening;
#endif
			break;
		}
		case XN_SYS_INPUTDEVICESCHANGED:
		{
			ProcessControllerNotification();
			break;
		}
		case XN_SYS_STORAGEDEVICESCHANGED:
		{
			// Notify any registered delegates
			TriggerOnlineDelegates(this,StorageDeviceChangeDelegates,NULL);
			break;
		}
		case XN_LIVE_LINK_STATE_CHANGED:
		{
			// Data will be non-zero if connected
			UBOOL bIsConnected = Data != 0;
			// Notify registered delegates
			ProcessLinkStateNotification(bIsConnected);
			break;
		}
		case XN_LIVE_CONTENT_INSTALLED:
		{
			ProcessContentChangeNotification();
			break;
		}
		case XN_LIVE_INVITE_ACCEPTED:
		{
			// Accept the invite for the specified user
			ProcessGameInvite((DWORD)Data);
			break;
		}
		case XN_LIVE_CONNECTIONCHANGED:
		{
			// Fire off any events needed for this notification
			ProcessConnectionStatusNotification((HRESULT)Data);
			break;
		}
		case XN_SYS_PROFILESETTINGCHANGED:
		{
			ProcessProfileDataNotification(Data);
			break;
		}
	}
}

/**
 * Handles any sign in change processing (firing delegates, etc)
 *
 * @param Data the mask of changed sign ins
 */
void UOnlineSubsystemLive::ProcessSignInNotification(ULONG_PTR Data)
{
	// Disable the ticking of delayed sign ins
	bIsCountingDownSigninNotification = FALSE;
	// Get a snapshot of the current state so we can compare against previous for events
	FCachedLoginState CurrentState[4];
	ReadLoginState(CurrentState);
	// Loop through the valid bits and send notifications
	for (DWORD Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
	{
		// Check for login change (profile swap, sign out, or sign in)
		if ((CurrentState[Index].LoginStatus == LS_NotLoggedIn && LastLoginState[Index].LoginStatus != LS_NotLoggedIn) ||
			(CurrentState[Index].LoginStatus != LS_NotLoggedIn && LastLoginState[Index].LoginStatus == LS_NotLoggedIn) ||
			// Check for them swapping profiles without a signin notification
			(CurrentState[Index].OnlineXuid != LastLoginState[Index].OnlineXuid && CurrentState[Index].OfflineXuid != LastLoginState[Index].OfflineXuid))
		{
			// Fire a login change
			debugfLiveSlow(NAME_DevOnline,TEXT("Discarding cached profile for user %d"),Index);
			if (ProfileCache[Index].Profile)
			{
				ProfileCache[Index].Profile->AsyncState = OPAS_NotStarted;
			}
			if (PlayerStorageCacheLocal[Index].PlayerStorage)
			{
				PlayerStorageCacheLocal[Index].PlayerStorage->AsyncState = OPAS_NotStarted;
			}
			// Zero the cached profile so we don't use the wrong profile
			ProfileCache[Index].Profile = NULL;
			// Zero the cached player storage entry as we should reread the data on login change
			PlayerStorageCacheLocal[Index].PlayerStorage = NULL;
			// All remote storage reads will have to be re-read
			PlayerStorageCacheRemote.Empty();
			// Clear any cached achievement data
			ClearCachedAchievements(Index);
			// Notify the game code a login change occured
			OnlineSubsystemLive_eventOnLoginChange_Parms Parms(EC_EventParm);
			Parms.LocalUserNum = Index;
			TriggerOnlineDelegates(this,LoginChangeDelegates,&Parms);
		}
		// Was their a change in their login state?
		if (CurrentState[Index].LoginStatus != LastLoginState[Index].LoginStatus)
		{
			OnlineSubsystemLive_eventOnLoginStatusChange_Parms Parms(EC_EventParm);
			Parms.NewId = CurrentState[Index].OnlineXuid;
			Parms.NewStatus = CurrentState[Index].LoginStatus;
			// Fire the delegate for each registered delegate
			TriggerOnlineDelegates(this,PlayerLoginStatusDelegates[Index].Delegates,&Parms);
		}
	}
	appMemcpy(LastLoginState,CurrentState,sizeof(FCachedLoginState) * 4);
	// Update voice's registered talkers
	UpdateVoiceFromLoginChange();
	bIsInSignInUI = FALSE;
}

/**
 * Handles notifying interested parties when installed content changes
 */
void UOnlineSubsystemLive::ProcessContentChangeNotification(void)
{
	// Notify any registered delegates
	TriggerOnlineDelegates(this,AnyContentChangeDelegates,NULL);
}

/**
 * Handles notifying interested parties when a signin is cancelled
 */
void UOnlineSubsystemLive::ProcessSignInCancelledNotification(void)
{
	// Notify each subscriber of the user canceling
	TriggerOnlineDelegates(this,LoginCancelledDelegates,NULL);
	bIsInSignInUI = FALSE;
}

/**
 * Searches the PRI array for the specified player
 *
 * @param User the user to find
 *
 * @return TRUE if found, FALSE otherwise
 */
inline UBOOL IsUserInSession(XUID User)
{
	UBOOL bIsInSession = FALSE;
	if (GWorld)
	{
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		if (WorldInfo)
		{
			AGameReplicationInfo* GameRep = WorldInfo->GRI;
			if (GameRep)
			{
				// Search through the set of players and see if they are in our game
				for (INT Index = 0; Index < GameRep->PRIArray.Num(); Index++)
				{
					APlayerReplicationInfo* PlayerRep = GameRep->PRIArray(Index);
					if (PlayerRep)
					{
						if ((XUID&)PlayerRep->UniqueId == User)
						{
							bIsInSession = TRUE;
							break;
						}
					}
				}
			}
		}
	}
	return bIsInSession;
}

/**
 * Handles accepting a game invite for the specified user
 *
 * @param UserNum the user that accepted the game invite
 */
void UOnlineSubsystemLive::ProcessGameInvite(DWORD UserNum)
{
	UBOOL bCanAcceptInvites = FALSE;
	// Make sure there are registered delegates before trying to handle it
	for (DWORD UserIndex = 0; UserIndex < MAX_LOCAL_PLAYERS; UserIndex++)
	{
		if (InviteCache[UserNum].InviteDelegates.Num() &&
			// Verify that DLC is not in the middle of being processed
			ContentCache[UserIndex].ReadState != OERS_InProgress)
		{
			bCanAcceptInvites = TRUE;
			break;
		}
	}
	if (bCanAcceptInvites)
	{
		// Clear the delayed bit
		DelayedInviteUserMask &= (~(1 << UserNum)) & 0xF;
		// This code assumes XNKID is 8 bytes
		check(sizeof(QWORD) == sizeof(XNKID));
		XINVITE_INFO* Info;
		// Allocate space on demand
		if (InviteCache[UserNum].InviteData == NULL)
		{
			InviteCache[UserNum].InviteData = new XINVITE_INFO;
		}
		// If for some reason the data didn't get cleaned up, do so now
		if (InviteCache[UserNum].InviteSearch != NULL &&
			InviteCache[UserNum].InviteSearch->Results.Num() > 0)
		{
			// Clean up the invite data
			delete (XSESSION_INFO*)InviteCache[UserNum].InviteSearch->Results(0).PlatformData;
			InviteCache[UserNum].InviteSearch->Results(0).PlatformData = NULL;
			InviteCache[UserNum].InviteSearch = NULL;
		}
		// Get the buffer to use and clear the previous contents
		Info = InviteCache[UserNum].InviteData;
		appMemzero(Info,sizeof(XINVITE_INFO));
		// Ask Live for the game details (session info)
		DWORD Return = XInviteGetAcceptedInfo(UserNum,Info);
		debugf(NAME_DevOnline,TEXT("XInviteGetAcceptedInfo(%d,Data) returned 0x%08X"),
			UserNum,Return);
		if (Return == ERROR_SUCCESS)
		{
			HandleJoinBySessionId(UserNum,(QWORD&)Info->xuidInviter,(QWORD&)Info->hostInfo.sessionID);
		}
	}
	else
	{
		// None are present to handle the invite, so set the delayed bit
		DelayedInviteUserMask |= 1 << UserNum;
	}
}

/**
 * Common method for joining a session by session id
 *
 * @param UserNum the user that is performing the search
 * @param Inviter the user that is sending the invite (or following)
 * @param SessionId the session id to join (from invite or friend presence)
 */
UBOOL UOnlineSubsystemLive::HandleJoinBySessionId(DWORD UserNum,QWORD Inviter,QWORD SessionId)
{
	DWORD Return = ERROR_SUCCESS;
	// Don't trigger an invite notification if we are in this session
	if (Sessions.Num() == 0 ||
		(IsInSession(SessionId) == FALSE && IsUserInSession(Inviter) == FALSE))
	{
		DWORD Size = 0;
		// Kick off an async search of the game by its session id. This is because
		// we need the gamesettings to understand what options to set
		//
		// First read the size needed to hold the results
		Return = XSessionSearchByID((XNKID&)SessionId,UserNum,&Size,NULL,NULL);
		if (Return == ERROR_INSUFFICIENT_BUFFER && Size > 0)
		{
			DWORD InviteDelegateIndex = UserNum;
			APlayerController* PC = GetPlayerControllerFromUserIndex(InviteDelegateIndex);
			if (PC == NULL)
			{
				// The player that accepted the invite hasn't created a PC, so notify the first valid one
				for (DWORD PlayerIndex = 0; PlayerIndex < MAX_LOCAL_PLAYERS; PlayerIndex++)
				{
					PC = GetPlayerControllerFromUserIndex(PlayerIndex);
					if (PC != NULL)
					{
						InviteDelegateIndex = PlayerIndex;
						break;
					}
				}
			}
			check(InviteDelegateIndex >= 0 && InviteDelegateIndex < MAX_LOCAL_PLAYERS); 
			// Create the async task with the proper data size
			FLiveAsyncTaskJoinGameInvite* AsyncTask = new FLiveAsyncTaskJoinGameInvite(UserNum,
				Size,&InviteCache[InviteDelegateIndex].InviteDelegates);
			// Now kick off the task
			Return = XSessionSearchByID((XNKID&)SessionId,
				UserNum,
				&Size,
				AsyncTask->GetResults(),
				*AsyncTask);
			debugf(NAME_DevOnline,TEXT("XSessionSearchByID(0x%016I64X,%d,%d,Data,Overlapped) returned 0x%08X"),
				SessionId,
				UserNum,
				Size,
				Return);
			if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
			{
				AsyncTasks.AddItem(AsyncTask);
			}
			else
			{
				// Don't leak the task/data
				delete AsyncTask;
			}
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Couldn't determine the size needed for searching for the game invite/jip information 0x%08X"),
				Return);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Ignoring a game invite/jip to a session we're already in"));
		Return = E_FAIL;
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Handles external UI change notifications
 *
 * @param bIsOpening whether the UI is opening or closing
 */
void UOnlineSubsystemLive::ProcessExternalUINotification(UBOOL bIsOpening)
{
    OnlineSubsystemLive_eventOnExternalUIChange_Parms Parms(EC_EventParm);
    Parms.bIsOpening = bIsOpening ? FIRST_BITFIELD : 0;
	// Notify of the UI changes
	TriggerOnlineDelegates(this,ExternalUIChangeDelegates,&Parms);
	// Handle user cancelling a signin request
	if (bIsOpening == FALSE && bIsInSignInUI == TRUE && bIsCountingDownSigninNotification == FALSE)
	{
		ProcessSignInCancelledNotification();
	}
}

/**
 * Handles controller connection state changes
 */
void UOnlineSubsystemLive::ProcessControllerNotification(void)
{
#if CONSOLE
	XINPUT_CAPABILITIES InputCaps;
	appMemzero(&InputCaps,sizeof(XINPUT_CAPABILITIES));
	// Default to none connected
	INT CurrentMask = 0;
	// Iterate the controllers checking their state
	for (INT ControllerIndex = 0; ControllerIndex < MAX_LOCAL_PLAYERS; ControllerIndex++)
	{
		INT ControllerMask = 1 << ControllerIndex;
		// See if this controller is connected or not
		if (XInputGetCapabilities(ControllerIndex,XINPUT_FLAG_GAMEPAD,&InputCaps) == ERROR_SUCCESS)
		{
			CurrentMask |= ControllerMask;
		}
	}
	// Store off for determining which delegates to fire
	INT OldMask = LastInputDeviceConnectedMask;
	// Set to the new state mask for delegates that check status
	LastInputDeviceConnectedMask = CurrentMask;
	// Now compare the old mask to the new mask for delegates to fire
	for (INT ControllerIndex = 0; ControllerIndex < MAX_LOCAL_PLAYERS; ControllerIndex++)
	{
		INT ControllerMask = 1 << ControllerIndex;
		// Only fire the event if the connection status has changed
		if ((CurrentMask & ControllerMask) != (OldMask & ControllerMask))
		{
			OnlineSubsystemLive_eventOnControllerChange_Parms Parms(EC_EventParm);
			Parms.ControllerId = ControllerIndex;
			// If the current mask and the controller mask match, the controller was inserted
			// otherwise it was removed
			Parms.bIsConnected = (CurrentMask & ControllerMask) ? FIRST_BITFIELD : 0;
			TriggerOnlineDelegates(this,ControllerChangeDelegates,&Parms);
		}
	}
#endif
}

/**
 * Handles notifying interested parties when the Live connection status
 * has changed
 *
 * @param Status the type of change that has happened
 */
void UOnlineSubsystemLive::ProcessConnectionStatusNotification(HRESULT Status)
{
	EOnlineServerConnectionStatus ConnectionStatus;
	// Map the Live code to ours
	switch (Status)
	{
		case XONLINE_S_LOGON_CONNECTION_ESTABLISHED:
			ConnectionStatus = OSCS_Connected;
			break;
		case XONLINE_S_LOGON_DISCONNECTED:
			ConnectionStatus = OSCS_NotConnected;
			break;
		case XONLINE_E_LOGON_NO_NETWORK_CONNECTION:
			ConnectionStatus = OSCS_NoNetworkConnection;
			break;
		case XONLINE_E_LOGON_CANNOT_ACCESS_SERVICE:
			ConnectionStatus = OSCS_ServiceUnavailable;
			break;
		case XONLINE_E_LOGON_UPDATE_REQUIRED:
		case XONLINE_E_LOGON_FLASH_UPDATE_REQUIRED:
			ConnectionStatus = OSCS_UpdateRequired;
			break;
		case XONLINE_E_LOGON_SERVERS_TOO_BUSY:
			ConnectionStatus = OSCS_ServersTooBusy;
			break;
		case XONLINE_E_LOGON_KICKED_BY_DUPLICATE_LOGON:
			ConnectionStatus = OSCS_DuplicateLoginDetected;
			break;
		case XONLINE_E_LOGON_INVALID_USER:
			ConnectionStatus = OSCS_InvalidUser;
			break;
		default:
			ConnectionStatus = OSCS_ConnectionDropped;
			break;
	}
    OnlineSubsystemLive_eventOnConnectionStatusChange_Parms Parms(EC_EventParm);
    Parms.ConnectionStatus = ConnectionStatus;
	TriggerOnlineDelegates(this,ConnectionStatusChangeDelegates,&Parms);
}

/**
 * Handles notifying interested parties when the link state changes
 *
 * @param bIsConnected whether the link has a connection or not
 */
void UOnlineSubsystemLive::ProcessLinkStateNotification(UBOOL bIsConnected)
{
    OnlineSubsystemLive_eventOnLinkStatusChange_Parms Parms(EC_EventParm);
    Parms.bIsConnected = bIsConnected ? FIRST_BITFIELD : 0;
	TriggerOnlineDelegates(this,LinkStatusChangeDelegates,&Parms);
}

/**
 * Handles notifying interested parties when the player changes profile data
 *
 * @param ChangeStatus bit flags indicating which user just changed status
 */
void UOnlineSubsystemLive::ProcessProfileDataNotification(DWORD ChangeStatus)
{
	// Check each user for a change
	for (DWORD Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
	{
		if ((1 << Index) & ChangeStatus)
		{
			// Notify this delegate of the change
			TriggerOnlineDelegates(this,ProfileCache[Index].ProfileDataChangedDelegates,NULL);
		}
	}
}

/**
 * Ticks voice subsystem for reading/submitting any voice data
 *
 * @param DeltaTime the time since the last tick
 */
void UOnlineSubsystemLive::TickVoice(FLOAT DeltaTime)
{
	if (VoiceEngine != NULL)
	{
		// Allow the voice engine to do some stuff ahead of time
		VoiceEngine->Tick(DeltaTime);
		// If we aren't using VDP or aren't in a networked match, no need to update
		// networked voice
		if (GSocketSubsystem->RequiresChatDataBeSeparate() &&
			Sessions.Num())
		{
			// Queue local packets for sending via the network
			ProcessLocalVoicePackets();
			// Submit queued packets to XHV
			ProcessRemoteVoicePackets();
			// Fire off any talking notifications for hud display
			ProcessTalkingDelegates(DeltaTime);
		}
	}
	// Check the speech recognition engine for pending notifications
	ProcessSpeechRecognitionDelegates();
}

/**
 * Reads any data that is currently queued in XHV
 */
void UOnlineSubsystemLive::ProcessLocalVoicePackets(void)
{
	if (VoiceEngine != NULL)
	{
		// Read the data from any local talkers
		DWORD DataReadyFlags = VoiceEngine->GetVoiceDataReadyFlags();
		// Skip processing if there is no data from a local talker
		if (DataReadyFlags)
		{
			// Process each talker with a bit set
			for (DWORD Index = 0; DataReadyFlags; Index++, DataReadyFlags >>= 1)
			{
				// Talkers needing processing will always be in lsb due to shifts
				if (DataReadyFlags & 1)
				{
					// Mark the person as talking
					LocalTalkers[Index].bIsTalking = TRUE;
					DWORD SpaceAvail = MAX_VOICE_DATA_SIZE - GVoiceData.LocalPackets[Index].Length;
					// Figure out if there is space for this packet
					if (SpaceAvail > 0)
					{
						DWORD NumPacketsCopied = 0;
						// Figure out where to append the data
						BYTE* BufferStart = GVoiceData.LocalPackets[Index].Buffer;
						BufferStart += GVoiceData.LocalPackets[Index].Length;
						// Copy the sender info
						GetUserXuid(Index,(XUID*)&GVoiceData.LocalPackets[Index].Sender);
						// Process this user
						HRESULT hr = VoiceEngine->ReadLocalVoiceData(Index,
							BufferStart,
							&SpaceAvail);
						if (SUCCEEDED(hr))
						{
							if (LocalTalkers[Index].bHasNetworkedVoice)
							{
								// Update the length based on what it copied
								GVoiceData.LocalPackets[Index].Length += SpaceAvail;
							}
							else
							{
								// Zero out the data since it isn't to be sent via the network
								GVoiceData.LocalPackets[Index].Length = 0;
							}
						}
					}
					else
					{
						debugfLiveSlow(NAME_DevOnline,TEXT("Dropping voice data due to network layer not processing fast enough"));
						// Buffer overflow, so drop previous data
						GVoiceData.LocalPackets[Index].Length = 0;
					}
				}
				else
				{
					LocalTalkers[Index].bIsTalking = FALSE;
				}
			}
		}
		else
		{
			// Clear the talking state for local players
			for (DWORD Index = 0; Index < 4; Index++)
			{
				LocalTalkers[Index].bIsTalking = FALSE;
			}
		}
	}
}

/**
 * Submits network packets to XHV for playback
 */
void UOnlineSubsystemLive::ProcessRemoteVoicePackets(void)
{
	// Clear the talking state for remote players
	for (INT Index = 0; Index < RemoteTalkers.Num(); Index++)
	{
		RemoteTalkers(Index).bIsTalking = FALSE;
	}

	// Now process all pending packets from the server
	for (INT Index = 0; Index < GVoiceData.RemotePackets.Num(); Index++)
	{
		FVoicePacket* VoicePacket = GVoiceData.RemotePackets(Index);
		if (VoicePacket != NULL)
		{
			// Skip local submission of voice if dedicated server or no voice
			if (VoiceEngine != NULL)
			{
				// Get the size since it is an in/out param
				DWORD PacketSize = VoicePacket->Length;
				// Submit this packet to the XHV engine
				HRESULT hr = VoiceEngine->SubmitRemoteVoiceData(VoicePacket->Sender,
					VoicePacket->Buffer,
					&PacketSize);
				if (FAILED(hr))
				{
					debugf(NAME_DevOnline,
						TEXT("SubmitRemoteVoiceData(0x%016I64X) failed with 0x%08X"),
						(QWORD&)VoicePacket->Sender,
						hr);
				}
			}
			// Skip all delegate handling if none are registered
			if (TalkingDelegates.Num() > 0)
			{
				// Find the remote talker and mark them as talking
				for (INT Index2 = 0; Index2 < RemoteTalkers.Num(); Index2++)
				{
					FLiveRemoteTalker& Talker = RemoteTalkers(Index2);
					// Compare the xuids
					if (Talker.TalkerId == VoicePacket->Sender)
					{
						// If the player is marked as muted, they can't be talking
						Talker.bIsTalking = !Talker.IsLocallyMuted();
					}
				}
			}
			VoicePacket->DecRef();
		}
	}
	// Zero the list without causing a free/realloc
	GVoiceData.RemotePackets.Reset();
}

/**
 * Processes any talking delegates that need to be fired off
 *
 * @param DeltaTime the amount of time that has elapsed since the last tick
 */
void UOnlineSubsystemLive::ProcessTalkingDelegates(FLOAT DeltaTime)
{
	// Skip all delegate handling if none are registered
	if (TalkingDelegates.Num() > 0)
	{
		// Fire off any talker notification delegates for local talkers
		for (DWORD Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
		{
			// Only check players with voice
			if (LocalTalkers[Index].bHasVoice &&
				LocalTalkers[Index].bWasTalking != LocalTalkers[Index].bIsTalking)
			{
				OnlineSubsystemLive_eventOnPlayerTalkingStateChange_Parms Parms(EC_EventParm);
				// Read the XUID from Live
				DWORD Return = GetUserXuid(Index,(XUID*)&Parms.Player);
				Parms.bIsTalking = LocalTalkers[Index].bIsTalking ? FIRST_BITFIELD : 0;
				if (Return == ERROR_SUCCESS)
				{
					TriggerOnlineDelegates(this,TalkingDelegates,&Parms);
				}
				// Clear the flag so it only activates when needed
				LocalTalkers[Index].bWasTalking = LocalTalkers[Index].bIsTalking;
			}
		}
		// Now check all remote talkers
		for (INT Index = 0; Index < RemoteTalkers.Num(); Index++)
		{
			FLiveRemoteTalker& Talker = RemoteTalkers(Index);
			// If the talker was not previously talking, but now is trigger the event
			UBOOL bShouldNotify = !Talker.bWasTalking && Talker.bIsTalking;
			// If the talker was previously talking, but now isn't time delay the event
			if (!bShouldNotify && Talker.bWasTalking && !Talker.bIsTalking)
			{
				Talker.LastNotificationTime -= DeltaTime;
				if (Talker.LastNotificationTime <= 0.f)
				{
					bShouldNotify = TRUE;
				}
			}
			if (bShouldNotify)
			{
				OnlineSubsystemLive_eventOnPlayerTalkingStateChange_Parms Parms(EC_EventParm);
				Parms.Player = Talker.TalkerId;
				Parms.bIsTalking = Talker.bIsTalking ? FIRST_BITFIELD : 0;
				TriggerOnlineDelegates(this,TalkingDelegates,&Parms);
				// Clear the flag so it only activates when needed
				Talker.bWasTalking = Talker.bIsTalking;
				Talker.LastNotificationTime = VoiceNotificationDelta;
			}
		}
	}
}

/**
 * Processes any speech recognition delegates that need to be fired off
 */
void UOnlineSubsystemLive::ProcessSpeechRecognitionDelegates(void)
{
	// Skip all delegate handling if we aren't using speech recognition
	if (bIsUsingSpeechRecognition && VoiceEngine != NULL)
	{
		// Fire off any talker notification delegates for local talkers
		for (DWORD Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
		{
			if (VoiceEngine->HasRecognitionCompleted(Index))
			{
				TriggerOnlineDelegates(this,PerUserDelegates[Index].SpeechRecognitionDelegates,NULL);
			}
		}
	}
}

/**
 * Processes a system link packet. For a host, responds to discovery
 * requests. For a client, parses the discovery response and places
 * the resultant data in the current search's search results array
 *
 * @param PacketData the packet data to parse
 * @param PacketLength the amount of data that was received
 */
void UOnlineSubsystemLive::ProcessLanPacket(BYTE* PacketData,INT PacketLength)
{
	// Check our mode to determine the type of allowed packets
	if (LanBeaconState == LANB_Hosting)
	{
		QWORD ClientNonce;
		// We can only accept Server Query packets
		if (IsValidLanQueryPacket(PacketData,PacketLength,ClientNonce))
		{
			// Iterate through all registered sessions and respond for each LAN match
			for (INT SessionIndex = 0; SessionIndex < Sessions.Num(); SessionIndex++)
			{
				FNamedSession* Session = &Sessions(SessionIndex);
				// Don't respond to queries when the match is full or it's not a lan match
				if (Session &&
					Session->GameSettings &&
					Session->GameSettings->bIsLanMatch &&
					Session->GameSettings->NumOpenPublicConnections > 0)
				{
					FNboSerializeToBufferXe Packet;
					// Add the supported version
					Packet << LAN_BEACON_PACKET_VERSION
						// Platform information
						<< (BYTE)appGetPlatformType()
						// Game id to prevent cross game lan packets
						<< LanGameUniqueId
						// Add the packet type
						<< LAN_SERVER_RESPONSE1 << LAN_SERVER_RESPONSE2
						// Append the client nonce as a QWORD
						<< ClientNonce;
					FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
					// Write host info (host addr, session id, and key)
					Packet << SessionInfo->XSessionInfo.hostAddress
						<< SessionInfo->XSessionInfo.sessionID
						<< SessionInfo->XSessionInfo.keyExchangeKey;
					// Now append per game settings
					AppendGameSettingsToPacket(Packet,Session->GameSettings);
					// Broadcast this response so the client can see us
					if (LanBeacon->BroadcastPacket(Packet,Packet.GetByteCount()) == FALSE)
					{
						debugfLiveSlow(NAME_DevOnline,TEXT("Failed to send response packet %d"),
							GSocketSubsystem->GetLastErrorCode());
					}
				}
			}
		}
	}
	else if (LanBeaconState == LANB_Searching)
	{
		// We can only accept Server Response packets
		if (IsValidLanResponsePacket(PacketData,PacketLength))
		{
			// Create an object that we'll copy the data to
			UOnlineGameSettings* NewServer = ConstructObject<UOnlineGameSettings>(
				GameSearch->GameSettingsClass);
			if (NewServer != NULL)
			{
				// Add space in the search results array
				INT NewSearch = GameSearch->Results.Add();
				FOnlineGameSearchResult& Result = GameSearch->Results(NewSearch);
				// Link the settings to this result
				Result.GameSettings = NewServer;
				// Strip off the type and nonce since it's been validated
				FNboSerializeFromBufferXe Packet(&PacketData[LAN_BEACON_PACKET_HEADER_SIZE],
					PacketLength - LAN_BEACON_PACKET_HEADER_SIZE);
				// Allocate and read the session data
				XSESSION_INFO* SessInfo = new XSESSION_INFO;
				// Read the connection data
				Packet >> SessInfo->hostAddress
					>> SessInfo->sessionID
					>> SessInfo->keyExchangeKey;
				// Store this in the results
				Result.PlatformData = SessInfo;
				// Read any per object data using the server object
				ReadGameSettingsFromPacket(Packet,NewServer);
				// Allow game code to sort the servers
				GameSearch->eventSortSearchResults();
				// NOTE: we don't notify until the timeout happens
			}
			else
			{
				debugfLiveSlow(NAME_DevOnline,TEXT("Failed to create new online game settings object"));
			}
		}
	}
}

/**
 * Displays the Xbox Guide to perform the login
 *
 * @param bShowOnlineOnly whether to only display online enabled profiles or not
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemLive::ShowLoginUI(UBOOL bShowOnlineOnly)
{
	// We allow 1, 2, or 4 on Xe. Should be 1 for Windows Live
	if (NumLogins != 1 && NumLogins != 2 && NumLogins != 4)
	{
		NumLogins = 1;
	}
	DWORD Result = XShowSigninUI(NumLogins,bShowOnlineOnly ? XSSUI_FLAGS_SHOWONLYONLINEENABLED : 0);
	if (Result == ERROR_SUCCESS)
	{
		bIsInSignInUI = TRUE;
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("XShowSigninUI(%d,0) failed with 0x%08X"),NumLogins,Result);
	}
	return Result == ERROR_SUCCESS;
}

/**
 * Logs the player into the online service. If this fails, it generates a
 * OnLoginFailed notification
 *
 * @param LocalUserNum the controller number of the associated user
 * @param LoginName the unique identifier for the player
 * @param Password the password for this account
 * @param ignored
 *
 * @return true if the async call started ok, false otherwise
 */
UBOOL UOnlineSubsystemLive::Login(BYTE LocalUserNum,const FString& LoginName,const FString& Password,UBOOL)
{
	HRESULT Return = E_NOTIMPL;
#if WITH_PANORAMA
	// Create a simple async task
	FLiveAsyncTaskSignin* AsyncTask = new FLiveAsyncTaskSignin();
	// Flags to use for signing in
	DWORD SigninFlags = XLSIGNIN_FLAG_ALLOWTITLEUPDATES | XLSIGNIN_FLAG_ALLOWSYSTEMUPDATES;
	FString IgnoredParam;
	// Dedicated server logins can save their credentials
	if (Parse(appCmdLine(),TEXT("-SAVECREDS"),IgnoredParam))
	{
		SigninFlags |= XLSIGNIN_FLAG_SAVECREDS;
	}
	// Now try to sign them in
	Return = XLiveSignin((LPWSTR)*LoginName,
		(LPWSTR)*Password,
		SigninFlags,
		*AsyncTask);
	debugfLiveSlow(NAME_DevOnline,TEXT("XLiveSignin(%s,...) returned 0x%08X"),*LoginName,Return);
	if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
	{
		AsyncTasks.AddItem(AsyncTask);
	}
	else
	{
		// Don't leak the task/data
		delete AsyncTask;
	}
#endif
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Logs the player into the online service using parameters passed on the
 * command line. Expects -Login=<UserName> -Password=<password>. If either
 * are missing, the function returns false and doesn't start the login
 * process
 *
 * @return true if the async call started ok, false otherwise
 */
UBOOL UOnlineSubsystemLive::AutoLogin(void)
{
	UBOOL bReturn = FALSE;
	FString LiveId;
	// Check to see if they specified a login
	if (Parse(appCmdLine(),TEXT("-Login="),LiveId))
	{
		FString LivePassword;
		// Make sure there is a password too
		if (Parse(appCmdLine(),TEXT("-Password="),LivePassword))
		{
			bReturn = Login(0,LiveId,LivePassword);
		}
	}
	return bReturn;
}

/**
 * Signs the player out of the online service
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::Logout(BYTE LocalUserNum)
{
	HRESULT Return = E_NOTIMPL;
#if WITH_PANORAMA
	// Create a simple async task
	FLiveAsyncTaskSignout* AsyncTask = new FLiveAsyncTaskSignout();
	// Sign the player out
	Return = XLiveSignout(*AsyncTask);
	debugfLiveSlow(NAME_DevOnline,TEXT("XLiveSignout() returned 0x%08X"),Return);
	if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
	{
		AsyncTasks.AddItem(AsyncTask);
	}
	else
	{
		// Don't leak the task/data
		delete AsyncTask;
	}
#endif
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Fetches the login status for a given player from Live
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the enum value of their status
 */
BYTE UOnlineSubsystemLive::GetLoginStatus(BYTE LocalUserNum)
{
	ELoginStatus Status = LS_NotLoggedIn;
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Ask Live for the login status
		XUSER_SIGNIN_STATE State = XUserGetSigninState(LocalUserNum);
		if (State == eXUserSigninState_SignedInToLive)
		{
			Status = LS_LoggedIn;
		}
		else if (State == eXUserSigninState_SignedInLocally)
		{
			Status = LS_UsingLocalProfile;
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to GetLoginStatus()"),
			(DWORD)LocalUserNum);
	}
	return Status;
}

/**
 * Determines whether the specified user is a guest login or not
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return true if a guest, false otherwise
 */
UBOOL UOnlineSubsystemLive::IsGuestLogin(BYTE LocalUserNum)
{
	UBOOL bIsGuest = FALSE;
	// Validate the user number
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		XUSER_SIGNIN_INFO SigninInfo;
		// Ask live for the signin flags that indicate a guest or not
		if (XUserGetSigninInfo(LocalUserNum,0,&SigninInfo) == ERROR_SUCCESS)
		{
			bIsGuest = (SigninInfo.dwInfoFlags & XUSER_INFO_FLAG_GUEST) ? TRUE : FALSE;
		}
	}
	return bIsGuest;
}

/**
 * Determines whether the specified user is a local (non-online) login or not
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return true if a local profile, false otherwise
 */
UBOOL UOnlineSubsystemLive::IsLocalLogin(BYTE LocalUserNum)
{
	UBOOL bIsLocal = TRUE;
	// Validate the user number
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		XUSER_SIGNIN_INFO SigninInfo;
		// Ask live for the signin flags that indicate a local profile or not
		if (XUserGetSigninInfo(LocalUserNum,0,&SigninInfo) == ERROR_SUCCESS)
		{
			bIsLocal = (SigninInfo.dwInfoFlags & XUSER_INFO_FLAG_LIVE_ENABLED) ? FALSE : TRUE;
		}
	}
	return bIsLocal;
}

/**
 * Gets the platform specific unique id for the specified player
 *
 * @param LocalUserNum the controller number of the associated user
 * @param UniqueId the byte array that will receive the id
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::GetUniquePlayerId(BYTE LocalUserNum,FUniqueNetId& UniqueId)
{
	check(sizeof(XUID) == 8);
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Read the XUID from Live
		Result = GetUserXuid(LocalUserNum,(XUID*)&UniqueId);
		if (Result != ERROR_SUCCESS)
		{
			debugf(NAME_DevOnline,TEXT("XUserGetXUID(%d) failed 0x%08X"),LocalUserNum,Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to GetUniquePlayerId()"),
			(DWORD)LocalUserNum);
	}
	return Result == ERROR_SUCCESS;
}

/**
 * Reads the player's nick name from the online service
 *
 * @param UniqueId the unique id of the player being queried
 *
 * @return a string containing the players nick name
 */
FString UOnlineSubsystemLive::GetPlayerNickname(BYTE LocalUserNum)
{
	ANSICHAR Buffer[32];
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Read the gamertag from Live
		Result = XUserGetName(LocalUserNum,Buffer,sizeof(Buffer));
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to GetPlayerNickname()"),
			(DWORD)LocalUserNum);
	}
	if (Result != ERROR_SUCCESS)
	{
		debugf(NAME_DevOnline,TEXT("XUserGetName(%d) failed 0x%08X"),LocalUserNum,Result);
		Buffer[0] = '\0';
	}
	return FString(ANSI_TO_TCHAR(Buffer));
}

/**
 * Determines whether the player is allowed to play online
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
BYTE UOnlineSubsystemLive::CanPlayOnline(BYTE LocalUserNum)
{
	// Default to enabled for non-Live accounts
	EFeaturePrivilegeLevel Priv = FPL_Enabled;
	BOOL bCan;
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Check for the priveledge
		Result = XUserCheckPrivilege(LocalUserNum,XPRIVILEGE_MULTIPLAYER_SESSIONS,
			&bCan);
		if (Result == ERROR_SUCCESS)
		{
			Priv = bCan == TRUE ? FPL_Enabled : FPL_Disabled;
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("XUserCheckPrivilege(%d,XPRIVILEGE_MULTIPLAYER_SESSIONS) failed with 0x%08X"),
				LocalUserNum,Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to CanPlayOnline()"),
			(DWORD)LocalUserNum);
		// Force it off because this is a bogus player
		Priv = FPL_Disabled;
	}
	return Priv;
}

/**
 * Determines whether the player is allowed to use voice or text chat online
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
BYTE UOnlineSubsystemLive::CanCommunicate(BYTE LocalUserNum)
{
	// Default to enabled for non-Live accounts
	EFeaturePrivilegeLevel Priv = FPL_Enabled;
	BOOL bCan;
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Check for the priveledge
		Result = XUserCheckPrivilege(LocalUserNum,XPRIVILEGE_COMMUNICATIONS,
			&bCan);
		if (Result == ERROR_SUCCESS)
		{
			if (bCan == TRUE)
			{
				// Universally ok
				Priv = FPL_Enabled;
			}
			else
			{
				// Not valid for everyone so check for friends only
				Result = XUserCheckPrivilege(LocalUserNum,
					XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY,&bCan);
				if (Result == ERROR_SUCCESS)
				{
					// Can only do this with friends or not at all
					Priv = bCan == TRUE ? FPL_EnabledFriendsOnly : FPL_Disabled;
				}
				else
				{
					debugf(NAME_DevOnline,TEXT("XUserCheckPrivilege(%d,XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY) failed with 0x%08X"),
						LocalUserNum,Result);
				}
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("XUserCheckPrivilege(%d,XPRIVILEGE_COMMUNICATIONS) failed with 0x%08X"),
				LocalUserNum,Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to CanCommunicate()"),
			(DWORD)LocalUserNum);
		// Force it off because this is a bogus player
		Priv = FPL_Disabled;
	}
	return Priv;
}

/**
 * Determines whether the player is allowed to download user created content
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
BYTE UOnlineSubsystemLive::CanDownloadUserContent(BYTE LocalUserNum)
{
	// Default to enabled for non-Live accounts
	EFeaturePrivilegeLevel Priv = FPL_Enabled;
	BOOL bCan;
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Check for the priveledge
		Result = XUserCheckPrivilege(LocalUserNum,
			XPRIVILEGE_USER_CREATED_CONTENT,&bCan);
		if (Result == ERROR_SUCCESS)
		{
			if (bCan == TRUE)
			{
				// Universally ok
				Priv = FPL_Enabled;
			}
			else
			{
				// Not valid for everyone so check for friends only
				Result = XUserCheckPrivilege(LocalUserNum,
					XPRIVILEGE_USER_CREATED_CONTENT_FRIENDS_ONLY,&bCan);
				if (Result == ERROR_SUCCESS)
				{
					// Can only do this with friends or not at all
					Priv = bCan == TRUE ? FPL_EnabledFriendsOnly : FPL_Disabled;
				}
				else
				{
					debugf(NAME_DevOnline,TEXT("XUserCheckPrivilege(%d,XPRIVILEGE_USER_CREATED_CONTENT_FRIENDS_ONLY) failed with 0x%08X"),
						LocalUserNum,Result);
				}
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("XUserCheckPrivilege(%d,XPRIVILEGE_USER_CREATED_CONTENT) failed with 0x%08X"),
				LocalUserNum,Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to CanDownloadUserContent()"),
			(DWORD)LocalUserNum);
		// Force it off because this is a bogus player
		Priv = FPL_Disabled;
	}
	return Priv;
}

/**
 * Determines whether the player is allowed to view other people's player profile
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
BYTE UOnlineSubsystemLive::CanViewPlayerProfiles(BYTE LocalUserNum)
{
	// Default to enabled for non-Live accounts
	EFeaturePrivilegeLevel Priv = FPL_Enabled;
	BOOL bCan;
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Check for the priveledge
		Result = XUserCheckPrivilege(LocalUserNum,XPRIVILEGE_PROFILE_VIEWING,
			&bCan);
		if (Result == ERROR_SUCCESS)
		{
			if (bCan == TRUE)
			{
				// Universally ok
				Priv = FPL_Enabled;
			}
			else
			{
				// Not valid for everyone so check for friends only
				Result = XUserCheckPrivilege(LocalUserNum,
					XPRIVILEGE_PROFILE_VIEWING_FRIENDS_ONLY,&bCan);
				if (Result == ERROR_SUCCESS)
				{
					// Can only do this with friends or not at all
					Priv = bCan == TRUE ? FPL_EnabledFriendsOnly : FPL_Disabled;
				}
				else
				{
					debugf(NAME_DevOnline,TEXT("XUserCheckPrivilege(%d,XPRIVILEGE_PROFILE_VIEWING_FRIENDS_ONLY) failed with 0x%08X"),
						LocalUserNum,Result);
				}
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("XUserCheckPrivilege(%d,XPRIVILEGE_PROFILE_VIEWING) failed with 0x%08X"),
				LocalUserNum,Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to CanViewPlayerProfiles()"),
			(DWORD)LocalUserNum);
		// Force it off because this is a bogus player
		Priv = FPL_Disabled;
	}
	return Priv;
}

/**
 * Determines whether the player is allowed to have their online presence
 * information shown to remote clients
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
BYTE UOnlineSubsystemLive::CanShowPresenceInformation(BYTE LocalUserNum)
{
	// Default to enabled for non-Live accounts
	EFeaturePrivilegeLevel Priv = FPL_Enabled;
	BOOL bCan;
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Check for the priveledge
		Result = XUserCheckPrivilege(LocalUserNum,XPRIVILEGE_PRESENCE,
			&bCan);
		if (Result == ERROR_SUCCESS)
		{
			if (bCan == TRUE)
			{
				// Universally ok
				Priv = FPL_Enabled;
			}
			else
			{
				// Not valid for everyone so check for friends only
				Result = XUserCheckPrivilege(LocalUserNum,
					XPRIVILEGE_PRESENCE_FRIENDS_ONLY,&bCan);
				if (Result == ERROR_SUCCESS)
				{
					// Can only do this with friends or not at all
					Priv = bCan == TRUE ? FPL_EnabledFriendsOnly : FPL_Disabled;
				}
				else
				{
					debugf(NAME_DevOnline,TEXT("XUserCheckPrivilege(%d,XPRIVILEGE_PRESENCE_FRIENDS_ONLY) failed with 0x%08X"),
						LocalUserNum,Result);
				}
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("XUserCheckPrivilege(%d,XPRIVILEGE_PRESENCE) failed with 0x%08X"),
				LocalUserNum,Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to CanShowPresenceInformation()"),
			(DWORD)LocalUserNum);
		// Force it off because this is a bogus player
		Priv = FPL_Disabled;
	}
	return Priv;
}

/**
 * Determines whether the player is allowed to purchase downloadable content
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
BYTE UOnlineSubsystemLive::CanPurchaseContent(BYTE LocalUserNum)
{
	// Default to enabled for non-Live accounts
	EFeaturePrivilegeLevel Priv = FPL_Enabled;
	BOOL bCan;
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Check for the priveledge
		Result = XUserCheckPrivilege(LocalUserNum,XPRIVILEGE_PURCHASE_CONTENT,
			&bCan);
		if (Result == ERROR_SUCCESS)
		{
			Priv = bCan == TRUE ? FPL_Enabled : FPL_Disabled;
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("XUserCheckPrivilege(%d,XPRIVILEGE_PURCHASE_CONTENT) failed with 0x%08X"),
				LocalUserNum,Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to CanPurchaseContent()"),
			(DWORD)LocalUserNum);
		// Force it off because this is a bogus player
		Priv = FPL_Disabled;
	}
	return Priv;
}

/**
 * Checks that a unique player id is part of the specified user's friends list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param UniqueId the id of the player being checked
 *
 * @return TRUE if a member of their friends list, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::IsFriend(BYTE LocalUserNum,FUniqueNetId UniqueId)
{
	check(sizeof(XUID) == 8);
	BOOL bIsFriend = FALSE;
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Ask Live if the local user is a friend of the specified player
		Result = XUserAreUsersFriends(LocalUserNum,(XUID*)&UniqueId,1,
			&bIsFriend,NULL);
		if (Result != ERROR_SUCCESS)
		{
			debugf(NAME_DevOnline,TEXT("XUserAreUsersFriends(%d,(0x%016I64X)) failed with 0x%08X"),
				LocalUserNum,UniqueId.Uid,Result);
			bIsFriend = FALSE;
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to IsFriend()"),
			(DWORD)LocalUserNum);
	}
	return bIsFriend;
}

/**
 * Checks that whether a group of player ids are among the specified player's
 * friends
 *
 * @param LocalUserNum the controller number of the associated user
 * @param Query an array of players to check for being included on the friends list
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::AreAnyFriends(BYTE LocalUserNum,TArray<FFriendsQuery>& Query)
{
	check(sizeof(XUID) == 8);
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Perform the check for each query
		for (INT Index = 0; Index < Query.Num(); Index++)
		{
			BOOL bIsFriend;
			// Ask Live if the local user is a friend of the specified player
			Result = XUserAreUsersFriends(LocalUserNum,(XUID*)&Query(Index).UniqueId,1,
				&bIsFriend,NULL);
			if (Result != ERROR_SUCCESS)
			{
				Query(Index).bIsFriend = bIsFriend;
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("XUserAreUsersFriends(%d,(0x%016I64X)) failed with 0x%08X"),
					LocalUserNum,Query(Index).UniqueId.Uid,Result);
				// Failure means no friendship
				Query(Index).bIsFriend = FALSE;
				break;
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to AreAnyFriends()"),
			(DWORD)LocalUserNum);
	}
	return Result == ERROR_SUCCESS;
}

/**
 * Checks that a unique player id is on the specified user's mute list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param UniqueId the id of the player being checked
 *
 * @return TRUE if a member of their friends list, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::IsMuted(BYTE LocalUserNum,FUniqueNetId UniqueId)
{
	check(sizeof(XUID) == 8);
	BOOL bIsMuted = FALSE;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Ask Live if the local user has muted the specified player
		XUserMuteListQuery(LocalUserNum,(XUID&)UniqueId,&bIsMuted);
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to IsMuted()"),
			(DWORD)LocalUserNum);
	}
	return bIsMuted;
}

/**
 * Displays the Xbox Guide Friends UI
 *
 * @param LocalUserNum the controller number of the user where are showing the friends for
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemLive::ShowFriendsUI(BYTE LocalUserNum)
{
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Show the friends UI for the specified controller num
		Result = XShowFriendsUI(LocalUserNum);
		if (Result != ERROR_SUCCESS)
		{
			debugf(NAME_DevOnline,TEXT("XShowFriendsUI(%d) failed with 0x%08X"),
				LocalUserNum,Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to ShowFriendsUI()"),
			(DWORD)LocalUserNum);
	}
	return Result == ERROR_SUCCESS;
}

/**
 * Displays the Xbox Guide Friends Request UI
 *
 * @param LocalUserNum the controller number of the user where are showing the friends for
 * @param UniqueId the id of the player being invited (null or 0 to have UI pick)
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemLive::ShowFriendsInviteUI(BYTE LocalUserNum,FUniqueNetId UniqueId)
{
	check(sizeof(XUID) == 8);
	// Figure out whether to use a specific XUID or not
	XUID RequestedId = (XUID&)UniqueId;
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Show the friends UI for the specified controller num and player
		Result = XShowFriendRequestUI(LocalUserNum,RequestedId);
		if (Result != ERROR_SUCCESS)
		{
			BYTE* Xuid = (BYTE*)&RequestedId;
			debugf(NAME_DevOnline,
				TEXT("XShowFriendsRequestUI(%d,) failed with 0x%08X"),
				LocalUserNum,
				Xuid,
				Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to ShowFriendsInviteUI()"),
			(DWORD)LocalUserNum);
	}
	return Result == ERROR_SUCCESS;
}

/**
 * Displays the UI that allows a player to give feedback on another player
 *
 * @param LocalUserNum the controller number of the associated user
 * @param UniqueId the id of the player having feedback given for
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemLive::ShowFeedbackUI(BYTE LocalUserNum,FUniqueNetId UniqueId)
{
	check(sizeof(XUID) == 8);
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Show the live guide ui for player review
		Result = XShowPlayerReviewUI(LocalUserNum,(XUID&)UniqueId);
		if (Result != ERROR_SUCCESS)
		{
			BYTE* Xuid = (BYTE*)&UniqueId;
			debugf(NAME_DevOnline,
				TEXT("XShowPlayerReviewUI(%d,0x%016I64X) failed with 0x%08X"),
				LocalUserNum,
				Xuid,
				Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to ShowFeedbackUI()"),
			(DWORD)LocalUserNum);
	}
	return Result == ERROR_SUCCESS;
}

/**
 * Displays the gamer card UI for the specified player
 *
 * @param LocalUserNum the controller number of the associated user
 * @param UniqueId the id of the player to show the gamer card of
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemLive::ShowGamerCardUI(BYTE LocalUserNum,FUniqueNetId UniqueId)
{
	check(sizeof(XUID) == 8);
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Show the live guide ui for gamer cards
		Result = XShowGamerCardUI(LocalUserNum,(XUID&)UniqueId);
		if (Result != ERROR_SUCCESS)
		{
			BYTE* Xuid = (BYTE*)&UniqueId;
			debugf(NAME_DevOnline,
				TEXT("XShowGamerCardUI(%d,0x%016I64X) failed with 0x%08X"),
				LocalUserNum,
				Xuid,
				Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to ShowGamerCardUI()"),
			(DWORD)LocalUserNum);
	}
	return Result == ERROR_SUCCESS;
}

/**
 * Displays the messages UI for a player
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemLive::ShowMessagesUI(BYTE LocalUserNum)
{
	// Show the live guide ui for player messages
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		Result = XShowMessagesUI(LocalUserNum);
		if (Result != ERROR_SUCCESS)
		{
			debugf(NAME_DevOnline,
				TEXT("XShowMessagesUI(%d) failed with 0x%08X"),
				LocalUserNum,
				Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to ShowMessagesUI()"),
			(DWORD)LocalUserNum);
	}
	return Result == ERROR_SUCCESS;
}

/**
 * Displays the achievements UI for a player
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemLive::ShowAchievementsUI(BYTE LocalUserNum)
{
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Show the live guide ui for player achievements
		Result = XShowAchievementsUI(LocalUserNum);
		if (Result != ERROR_SUCCESS)
		{
			debugf(NAME_DevOnline,TEXT("XShowAchievementsUI(%d) failed with 0x%08X"),
				LocalUserNum,Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to ShowAchievementsUI()"),
			(DWORD)LocalUserNum);
	}
	return Result == ERROR_SUCCESS;
}

/**
 * Displays the Live Guide
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemLive::ShowGuideUI()
{
#if WITH_PANORAMA
	DWORD Result = XShowGuideUI( 0 );
	if (Result != ERROR_SUCCESS)
	{
		debugf(NAME_DevOnline,TEXT("XShowGuideUI(0) failed with 0x%08X"),Result);
	}
	return Result == ERROR_SUCCESS;
#else
	return FALSE;
#endif
}

/**
 * Displays the achievements UI for a player
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemLive::ShowPlayersUI(BYTE LocalUserNum)
{
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Show the live guide ui for the player list
		Result = XShowPlayersUI(LocalUserNum);
		if (Result != ERROR_SUCCESS)
		{
			debugf(NAME_DevOnline,TEXT("XShowPlayersUI(%d) failed with 0x%08X"),
				LocalUserNum,Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to ShowPlayersUI()"),
			(DWORD)LocalUserNum);
	}
	return Result == ERROR_SUCCESS;
}

/**
 * Displays the UI that shows the keyboard for inputing text
 *
 * @param LocalUserNum the controller number of the associated user
 * @param TitleText the title to display to the user
 * @param DescriptionText the text telling the user what to input
 * @param bIsPassword whether the item being entered is a password or not
 * @param bShouldValidate whether to apply the string validation API after input or not
 * @param DefaultText the default string to display
 * @param MaxResultLength the maximum length string expected to be filled in
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemLive::ShowKeyboardUI(BYTE LocalUserNum,
	const FString& TitleText,const FString& DescriptionText,UBOOL bIsPassword,
	UBOOL bShouldValidate,const FString& DefaultText,INT MaxResultLength)
{
	DWORD Return = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		if (MaxResultLength > 0)
		{
			DWORD Flags = VKBD_HIGHLIGHT_TEXT;
#if !WITH_PANORAMA
			// Determine which keyboard features based upon dash language
			switch (XGetLanguage())
			{
				case XC_LANGUAGE_JAPANESE:
				{
					Flags |= VKBD_JAPANESE_FULL;
					break;
				}
				case XC_LANGUAGE_KOREAN:
				{
					Flags |= VKBD_KOREAN_FULL;
					break;
				}
				case XC_LANGUAGE_TCHINESE:
				{
					Flags |= VKBD_TCH_FULL;
					break;
				}
				default:
				{
					Flags |= VKBD_LATIN_FULL;
					break;
				}
			}
#endif
			// Allow for password entry if requested
			if (bIsPassword)
			{
				Flags |= VKBD_LATIN_PASSWORD;
			}
			// Allocate an async task to hold the data while in process
			FLiveAsyncTaskDataKeyboard* AsyncData = new FLiveAsyncTaskDataKeyboard(TitleText,
				DefaultText,DescriptionText,MaxResultLength,bShouldValidate);
			FLiveAsyncTaskKeyboard* AsyncTask = new FLiveAsyncTaskKeyboard(&KeyboardInputDelegates,AsyncData);
			// Show the live guide ui for inputing text
			Return = XShowKeyboardUI(LocalUserNum,
				Flags,
				AsyncData->GetDefaultText(),
				AsyncData->GetTitleText(),
				AsyncData->GetDescriptionText(),
				*AsyncData,
				MaxResultLength,
				*AsyncTask);
			if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
			{
				AsyncTasks.AddItem(AsyncTask);
			}
			else
			{
				// Just trigger the delegate as having failed
				FAsyncTaskDelegateResults Results(Return);
				TriggerOnlineDelegates(this,KeyboardInputDelegates,&Results);
				// Don't leak the task/data
				delete AsyncTask;
				debugf(NAME_DevOnline,TEXT("XShowKeyboardUI(%d,%d,'%s','%s','%s',data,data,data) failed with 0x%08X"),
					LocalUserNum,Flags,*DefaultText,*TitleText,*DescriptionText,Return);
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Invalid MaxResultLength"));
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to ShowKeyboardUI()"),
			(DWORD)LocalUserNum);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Determines if the ethernet link is connected or not
 */
UBOOL UOnlineSubsystemLive::HasLinkConnection(void)
{
	return (XNetGetEthernetLinkStatus() & XNET_ETHERNET_LINK_ACTIVE) != 0;
}

/**
 * Sets a new position for the network notification icons/images
 *
 * @param NewPos the new location to use
 */
void UOnlineSubsystemLive::SetNetworkNotificationPosition(BYTE NewPos)
{
	CurrentNotificationPosition = NewPos;
	// Map our enum to Live's
	switch (CurrentNotificationPosition)
	{
		case NNP_TopLeft:
		{
			XNotifyPositionUI(XNOTIFYUI_POS_TOPLEFT);
			break;
		}
		case NNP_TopCenter:
		{
			XNotifyPositionUI(XNOTIFYUI_POS_TOPCENTER);
			break;
		}
		case NNP_TopRight:
		{
			XNotifyPositionUI(XNOTIFYUI_POS_TOPRIGHT);
			break;
		}
		case NNP_CenterLeft:
		{
			XNotifyPositionUI(XNOTIFYUI_POS_CENTERLEFT);
			break;
		}
		case NNP_Center:
		{
			XNotifyPositionUI(XNOTIFYUI_POS_CENTER);
			break;
		}
		case NNP_CenterRight:
		{
			XNotifyPositionUI(XNOTIFYUI_POS_CENTERRIGHT);
			break;
		}
		case NNP_BottomLeft:
		{
			XNotifyPositionUI(XNOTIFYUI_POS_BOTTOMLEFT);
			break;
		}
		case NNP_BottomCenter:
		{
			XNotifyPositionUI(XNOTIFYUI_POS_BOTTOMCENTER);
			break;
		}
		case NNP_BottomRight:
		{
			XNotifyPositionUI(XNOTIFYUI_POS_BOTTOMRIGHT);
			break;
		}
	}
}

/**
 * Determines if the specified controller is connected or not
 *
 * @param ControllerId the controller to query
 *
 * @return true if connected, false otherwise
 */
UBOOL UOnlineSubsystemLive::IsControllerConnected(INT ControllerId)
{
	return (LastInputDeviceConnectedMask & (1 << ControllerId)) ? TRUE : FALSE;
}

/**
 * Determines the NAT type the player is using
 */
BYTE UOnlineSubsystemLive::GetNATType(void)
{
	ENATType NatType = NAT_Unknown;

 	if (AreAnySignedIntoLive())
	{
		// Ask the system for it
		switch (XOnlineGetNatType())
		{
			case XONLINE_NAT_OPEN:
				NatType = NAT_Open;
				break;

			case XONLINE_NAT_MODERATE:
				NatType = NAT_Moderate;
				break;

			case XONLINE_NAT_STRICT:
				NatType = NAT_Strict;
				break;
		}
	}
	return NatType;
}

/**
 * Determine the locale (country code) for the player
 */
INT UOnlineSubsystemLive::GetLocale(void)
{
#if CONSOLE
	return XGetLocale();
#else
	return 0;
#endif
}

/**
 * Creates the session flags value from the game settings object. First looks
 * for the standard settings and then checks for Live specific settings
 *
 * @param InSettings the game settings of the new session
 *
 * @return the flags needed to set up the session
 */
DWORD UOnlineSubsystemLive::BuildSessionFlags(UOnlineGameSettings* InSettings)
{
	DWORD Flags = 0;
	// Base setting is that we are using the peer network (secure with
	// parental controls)
	Flags |= XSESSION_CREATE_HOST | XSESSION_CREATE_USES_PEER_NETWORK;
	// The flag checks below are only for normal Live (player/ranked) sessions
	if (InSettings->bIsLanMatch == FALSE)
	{
		// Whether to advertise the server or not
		if (InSettings->bShouldAdvertise == TRUE)
		{
			Flags |= XSESSION_CREATE_USES_MATCHMAKING;
		}
		// Whether to require arbitration or not
		if (InSettings->bUsesArbitration == TRUE)
		{
			Flags |= XSESSION_CREATE_USES_ARBITRATION;
		}
		// Whether to use stats or not
		if (InSettings->bUsesStats == TRUE)
		{
			Flags |= XSESSION_CREATE_USES_STATS;
		}
		// Check all of the flags that rely on presence information
		if (InSettings->bUsesPresence)
		{
			Flags |= XSESSION_CREATE_USES_PRESENCE;
#if CONSOLE
			// Check for the friends only flag
			if (InSettings->bAllowJoinViaPresenceFriendsOnly)
			{
				Flags |= XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY;
			}
			else
#endif
			// Whether to allow join via presence information or not. NOTE: Friends only overrides
			if (InSettings->bAllowJoinViaPresence == FALSE)
			{
				Flags |= XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED;
			}
			// Whether to allow invites or not
			if (InSettings->bAllowInvites == FALSE)
			{
				Flags |= XSESSION_CREATE_INVITES_DISABLED;
			}
		}
		// Whether to allow join in progress or not
		if (InSettings->bAllowJoinInProgress == FALSE)
		{
			Flags |= XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED;
		}
	}
	return Flags;
}

/**
 * Sets the list contexts for the player
 *
 * @param PlayerNum the index of the player hosting the match
 * @param Contexts the list of contexts to set
 */
void UOnlineSubsystemLive::SetContexts(BYTE PlayerNum,const TArray<FLocalizedStringSetting>& Contexts)
{
	// Iterate through all contexts and set them
	for (INT Index = 0; Index < Contexts.Num(); Index++)
	{
		const FLocalizedStringSetting& Context = Contexts(Index);
		// Only publish fields that are meant to be advertised
		if (Context.AdvertisementType == ODAT_OnlineService ||
			Context.AdvertisementType == ODAT_OnlineServiceAndQoS)
		{
			// Set the context data
			DWORD Result = XUserSetContextEx(PlayerNum,Context.Id,Context.ValueIndex,NULL);
			// Log it for debug purposes
			debugf(NAME_DevOnline,TEXT("XUserSetContextEx(%d,0x%08X,%d,NULL) returned 0x%08X"),
				PlayerNum,Context.Id,Context.ValueIndex,Result);
		}
	}
}

/**
 * Sets the list properties for the player
 *
 * @param PlayerNum the index of the player hosting the match
 * @param Properties the list of properties to set
 */
void UOnlineSubsystemLive::SetProperties(BYTE PlayerNum,const TArray<FSettingsProperty>& Properties)
{
	// Iterate through all properties and set those too
	for (INT Index = 0; Index < Properties.Num(); Index++)
	{
		const FSettingsProperty& Property = Properties(Index);
		// Only publish fields that are meant to be advertised
		if (Property.AdvertisementType == ODAT_OnlineService ||
			Property.AdvertisementType == ODAT_OnlineServiceAndQoS)
		{
			// Get the size of data that we'll be sending
			DWORD SizeOfData = GetSettingsDataSize(Property.Data);
			// Get the pointer to the data we are sending
			const void* DataPointer = GetSettingsDataPointer(Property.Data);
			// Set the context data
			DWORD Result = XUserSetPropertyEx(PlayerNum,Property.PropertyId,SizeOfData,DataPointer,NULL);
#if !FINAL_RELEASE
			// Log it for debug purposes
			FString StringVal = Property.Data.ToString();
			debugf(NAME_DevOnline,TEXT("XUserSetPropertyEx(%d,0x%08X,%d,%s,NULL) returned 0x%08X"),
				PlayerNum,Property.PropertyId,SizeOfData,*StringVal,Result);
#endif
		}
	}
}

/**
 * Sets the contexts and properties for this game settings object
 *
 * @param PlayerNum the index of the player performing the search/hosting the match
 * @param GameSettings the game settings of the new session
 *
 * @return TRUE if successful, FALSE otherwise
 */
void UOnlineSubsystemLive::SetContextsAndProperties(BYTE PlayerNum,
	UOnlineGameSettings* InSettings)
{
	DWORD GameType = X_CONTEXT_GAME_TYPE_STANDARD;
	// Add arbitration flag if requested
	if (InSettings->bUsesArbitration == TRUE)
	{
		GameType = X_CONTEXT_GAME_TYPE_RANKED;
	}
	// Set the game type (standard or ranked)
	DWORD Result = XUserSetContextEx(PlayerNum,X_CONTEXT_GAME_TYPE,GameType,NULL);
	debugf(NAME_DevOnline,TEXT("XUserSetContextEx(%d,X_CONTEXT_GAME_TYPE,%d,NULL) returned 0x%08X"),
		PlayerNum,GameType,Result);
	// Use the common methods for setting the lists of contexts & properties
	SetContexts(PlayerNum,InSettings->LocalizedSettings);
	SetProperties(PlayerNum,InSettings->Properties);
}

/**
 * Creates an online game based upon the settings object specified.
 *
 * @param HostingPlayerNum the index of the player hosting the match
 * @param SessionName the name to associate with this setting
 * @param GameSettings the settings to use for the new game session
 *
 * @return true if successful creating the session, false otherwsie
 */
UBOOL UOnlineSubsystemLive::CreateOnlineGame(BYTE HostingPlayerNum,FName SessionName,
	UOnlineGameSettings* NewGameSettings)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	FNamedSession* Session = GetNamedSession(SessionName);
	// Don't set if we already have a session going
	if (Session == NULL)
	{
		// Add a named session and set it's game settings
		Session = AddNamedSession(SessionName,NewGameSettings);
		Session->SessionInfo = new FSecureSessionInfo();
		// Init the game settings counts so the host can use them later
		Session->GameSettings->NumOpenPrivateConnections = Session->GameSettings->NumPrivateConnections;
		Session->GameSettings->NumOpenPublicConnections = Session->GameSettings->NumPublicConnections;
		// Read the XUID of the owning player for gamertag and gamercard support
		GetUserXuid(HostingPlayerNum,(XUID*)&Session->GameSettings->OwningPlayerId);
		ANSICHAR Buffer[32];
		// Read the name of the owning player
		XUserGetName(HostingPlayerNum,Buffer,sizeof(Buffer));
		Session->GameSettings->OwningPlayerName = Buffer;
		// Register via Live
		Return = CreateLiveGame(HostingPlayerNum,Session);
		// If we were unable to create the game, clean up
		if (Return != ERROR_IO_PENDING && Return != ERROR_SUCCESS && Session)
		{
			// Clean up the session info so we don't get into a confused state
			RemoveNamedSession(SessionName);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Cannot create session '%s': session already exists."), *SessionName.ToString());
	}
	if (Return != ERROR_IO_PENDING)
	{
		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResultsNamedSession Results(SessionName,Return);
		TriggerOnlineDelegates(this,CreateOnlineGameCompleteDelegates,&Results);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Finishes creating the online game, including creating lan beacons and/or
 * list play sessions
 *
 * @param HostingPlayerNum the player starting the session
 * @param SessionName the name of the session that is being created
 * @param CreateResult the result code from the async create operation
 * @param bIsFromInvite whether this is from an invite or not
 */
void UOnlineSubsystemLive::FinishCreateOnlineGame(DWORD HostingPlayerNum,FName SessionName,DWORD CreateResult,UBOOL bIsFromInvite)
{
	// Get the session from the name (can't be missing since this is a finish process)
	FNamedSession* Session = GetNamedSession(SessionName);
	check(Session);
	// If the task completed ok, then continue the create process
	if (CreateResult == ERROR_SUCCESS)
	{
		FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
		// Register the host with QoS
		RegisterQoS(Session);
		// Determine if we are registering a Live session or system link
		if (Session->GameSettings->bIsLanMatch)
		{
			// Initialize the lan game's lan beacon for queries
			CreateResult = CreateLanGame(HostingPlayerNum,Session);
		}
		// Set the game state as pending (not started)
		Session->GameSettings->GameState = OGS_Pending;
		// Register all local folks as participants/talkers
		if (Session->GameSettings->bIsDedicated == FALSE)
		{
		   RegisterLocalPlayers(Session,bIsFromInvite);
		}
	}
	else
	{
		// Clean up partial create
		RemoveNamedSession(SessionName);
	}
	// As long as there isn't an async task outstanding, fire the events
	if (CreateResult != ERROR_IO_PENDING)
	{
		// Just trigger the delegate with the success code
		FAsyncTaskDelegateResultsNamedSession Results(SessionName,CreateResult);
		TriggerOnlineDelegates(this,CreateOnlineGameCompleteDelegates,&Results);
	}
}

/** max size of QoS packet served by listener */
#define MAX_QOSPACKET_SIZE 512

/**
 * Generate the BYTE array representation of data to be sent via QoS from game settings
 *
 * @param OutQosPacket [out] BYTE array to fill with QoS packet
 * @param OutQoSPacketLen [out] size of QoS packet that was copied
 * @param GameSettings online settings to pack into QoS packet
 */
static void GenerateQoSPacket(BYTE* OutQoSPacket, DWORD& OutQoSPacketLen, UOnlineGameSettings* GameSettings)
{
	appMemzero(OutQoSPacket,MAX_QOSPACKET_SIZE);
	FNboSerializeToBufferXe Packet;
	Packet << QOS_PACKET_VERSION;
	Packet << GameSettings->OwningPlayerId;
	Packet << GameSettings->ServerNonce;
	Packet << GameSettings->BuildUniqueId;
	TArray<FSettingsProperty> Props;
	// Get the properties that are to be advertised via QoS
	GameSettings->GetQoSAdvertisedProperties(Props);
	// Append any custom properties to be exposed via QoS
	INT Num = Props.Num();
	Packet << Num;
	for (INT Index = 0; Index < Props.Num(); Index++)
	{
		Packet << Props(Index);
	}
	TArray<FLocalizedStringSetting> Contexts;
	// Get the contexts that are to be advertised via QoS
	GameSettings->GetQoSAdvertisedStringSettings(Contexts);
	// Append any custom contexts to be exposed via QoS
	Num = Contexts.Num();
	Packet << Num;
	for (INT Index = 0; Index < Contexts.Num(); Index++)
	{
		Packet << Contexts(Index);
	}
	// Determine the size of the packet data
	OutQoSPacketLen = Packet.GetByteCount();
	if (OutQoSPacketLen > MAX_QOSPACKET_SIZE)
	{
		OutQoSPacketLen = 0;
		debugfLiveSlow(NAME_DevOnline,TEXT("QoS packet too large, discarding it"));
	}
	else
	{
		// Copy the data into our persistent buffer since QoS will access it async	
		appMemcpy(OutQoSPacket,(BYTE*)Packet,OutQoSPacketLen);
	}
}

/**
 * Tells the QoS thread to start its listening process. Builds the packet
 * of custom data to send back to clients in the query.
 *
 * @param Session the named session information that is being registered
 *
 * @return The success/error code of the operation
 */
DWORD UOnlineSubsystemLive::RegisterQoS(FNamedSession* Session)
{
	check(Session && Session->GameSettings && Session->SessionInfo);
	DWORD Return = ERROR_SUCCESS;
	// Grab the data from the session for less typing
	FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
	UOnlineGameSettings* GameSettings = Session->GameSettings;
	// Skip if the game isn't being advertised
	if (GameSettings->bShouldAdvertise)
	{
		// Copy over the Nonce for replicating to clients
		GameSettings->ServerNonce = SessionInfo->Nonce;
		// Set the build unique id for replication to clients
		GameSettings->BuildUniqueId = GetBuildUniqueId();
		DWORD QoSPacketLen = 0;
		// Build our custom QoS packet
		GenerateQoSPacket(QoSPacket,QoSPacketLen,GameSettings);
		DWORD QoSFlags = XNET_QOS_LISTEN_ENABLE | XNET_QOS_LISTEN_SET_DATA;
		if (QoSPacketLen == 0)
		{
			QoSFlags = XNET_QOS_LISTEN_ENABLE;			
		}
		// Register with the QoS listener
		Return = XNetQosListen(&SessionInfo->XSessionInfo.sessionID,
			QoSPacket,
			QoSPacketLen,
			// Uses the default (16kbps)
			0,
			QoSFlags);
		debugfLiveSlow(NAME_DevOnline,
			TEXT("XNetQosListen(Key,Data,%d,0,XNET_QOS_LISTEN_ENABLE | XNET_QOS_LISTEN_SET_DATA) returned 0x%08X"),
			QoSPacketLen,Return);
	}
	return Return;
}

/**
 * Tells the QoS to respond with a "go away" packet and includes our custom
 * data. Prevents bandwidth from going to QoS probes
 *
 * @param Session the named session info for the session
 *
 * @return The success/error code of the operation
 */
DWORD UOnlineSubsystemLive::DisableQoS(FNamedSession* Session)
{
	DWORD Return = ERROR_SUCCESS;
	check(Session && Session->SessionInfo);
	// Skip if the game isn't being advertised
	if (Session->GameSettings->bShouldAdvertise)
	{
		FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
		// Stops the QoS from responding
		Return = XNetQosListen(&SessionInfo->XSessionInfo.sessionID,
			NULL,
			0,
			0,
			XNET_QOS_LISTEN_DISABLE);
		debugfLiveSlow(NAME_DevOnline,
			TEXT("XNetQosListen(Key,NULL,0,0,XNET_QOS_LISTEN_DISABLE) returned 0x%08X"),
			Return);
	}
	return Return;
}

/**
 * Tells the QoS thread to stop its listening process
 *
 * @param Session the named session info for the session
 *
 * @return The success/error code of the operation
 */
DWORD UOnlineSubsystemLive::UnregisterQoS(FNamedSession* Session)
{
	DWORD Return = ERROR_SUCCESS;
	check(Session && Session->SessionInfo);
	// Skip if the game isn't being advertised
	if (Session->GameSettings->bShouldAdvertise)
	{
		FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
		// Unregister with the QoS listener and releases any memory underneath
		Return = XNetQosListen(&SessionInfo->XSessionInfo.sessionID,
			NULL,
			0,
			0,
			XNET_QOS_LISTEN_RELEASE | XNET_QOS_LISTEN_SET_DATA);
		debugfLiveSlow(NAME_DevOnline,
			TEXT("XNetQosListen(Key,NULL,0,0,XNET_QOS_LISTEN_RELEASE | XNET_QOS_LISTEN_SET_DATA) returned 0x%08X"),
			Return);
	}
	return Return;
}

/**
 * Kicks off the list of returned servers' QoS queries
 *
 * @param AsyncData the object that holds the async QoS data
 *
 * @return TRUE if the call worked and the results should be polled for,
 *		   FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::CheckServersQoS(FLiveAsyncTaskDataSearch* AsyncData)
{
	UBOOL bOk = FALSE;
	if (GameSearch != NULL)
	{
		check(AsyncData);
		// Figure out how many servers we need to ping
		DWORD NumServers = (DWORD)GameSearch->Results.Num();
		if (NumServers > 0)
		{
			// The QoS arrays are hardcoded to 50
			check(NumServers < 50);
			// Get the pointers to the arrays used in the QoS calls
			XNADDR** ServerAddrs = AsyncData->GetXNADDRs();
			XNKID** ServerKids = AsyncData->GetXNKIDs();
			XNKEY** ServerKeys = AsyncData->GetXNKEYs();
			// Loop through the results and build the arrays needed for our queries
			for (INT Index = 0; Index < GameSearch->Results.Num(); Index++)
			{
				const FOnlineGameSearchResult& Game = GameSearch->Results(Index);
				const XSESSION_INFO* XSessInfo = (XSESSION_INFO*)Game.PlatformData;
				// Copy the addr, id, and key
				ServerAddrs[Index] = (XNADDR*)&XSessInfo->hostAddress;
				ServerKids[Index] = (XNKID*)&XSessInfo->sessionID;
				ServerKeys[Index] = (XNKEY*)&XSessInfo->keyExchangeKey;
			}
			// Kick off the QoS set of queries
			DWORD Return = XNetQosLookup(NumServers,
				(const XNADDR**)ServerAddrs,
				(const XNKID**)ServerKids,
				(const XNKEY**)ServerKeys,
				// We skip all gateway services
				0,0,0,
				// Use 8 if not set
				GameSearch->NumPingProbes > 0 ? GameSearch->NumPingProbes : 8,
				// 64k if not set
				GameSearch->MaxPingBytes > 0 ? GameSearch->MaxPingBytes : 64 * 1024,
				// Flags are unsupported and we'll poll
				0,NULL,
				// The out parameter that holds the data
				AsyncData->GetXNQOS());
			debugfLiveSlow(NAME_DevOnline,
				TEXT("XNetQosLookup(%d,Addrs,Kids,Keys,0,0,0,8,64K,0,NULL,Data) returned 0x%08X"),
				NumServers,
				Return);
			bOk = Return == ERROR_SUCCESS;
		}
	}
	return bOk;
}

/**
 * Parses the results from the QoS queries and places those results in the
 * corresponding search results info
 *
 * @param QosData the data to parse the results of
 */
void UOnlineSubsystemLive::ParseQoSResults(XNQOS* QosData)
{
	check(QosData);
	check(GameSearch);
	// If these don't match, we don't know who the data belongs to
	if (GameSearch->Results.Num() == QosData->cxnqos)
	{
		// Iterate through the results
		for (DWORD Index = 0; Index < QosData->cxnqos; Index++)
		{
			// Get the game settings object to add data to
			UOnlineGameSettings* ServerSettings = GameSearch->Results(Index).GameSettings;
			// Read the custom data if present
			if (QosData->axnqosinfo[Index].cbData > 0 &&
				QosData->axnqosinfo[Index].pbData != NULL)
			{
				// Create a packet reader to read the data out
				FNboSerializeFromBufferXe Packet(QosData->axnqosinfo[Index].pbData,
					QosData->axnqosinfo[Index].cbData);
				BYTE QosPacketVersion = 0;
				Packet >> QosPacketVersion;
				// Verify the packet version
				if (QosPacketVersion == QOS_PACKET_VERSION)
				{
					// Read the XUID and the server nonce
					Packet >> ServerSettings->OwningPlayerId;
					Packet >> ServerSettings->ServerNonce;
					Packet >> ServerSettings->BuildUniqueId;
					INT NumProps = 0;
					// Read how many props are in the buffer
					Packet >> NumProps;
					for (INT PropIndex = 0; PropIndex < NumProps; PropIndex++)
					{
						INT AddAt = ServerSettings->Properties.AddZeroed();
						Packet >> ServerSettings->Properties(AddAt);
					}
					INT NumContexts = 0;
					// Read how many contexts are in the buffer
					Packet >> NumContexts;
					for (INT ContextIndex = 0; ContextIndex < NumContexts; ContextIndex++)
					{
						INT AddAt = ServerSettings->LocalizedSettings.AddZeroed();
						Packet >> ServerSettings->LocalizedSettings(AddAt);
					}
					// Set the ping that the QoS estimated
					ServerSettings->PingInMs = QosData->axnqosinfo[Index].wRttMedInMsecs;
					debugfLiveSlow(NAME_DevOnline,TEXT("QoS for %s is %d"),
						*ServerSettings->OwningPlayerName,ServerSettings->PingInMs);
				}
				else
				{
					debugfLiveSlow(NAME_DevOnline,TEXT("Skipping QoS packet due to version mismatch"));
				}
			}
			else
			{
				// Mark with an unreachable ping for this server
				ServerSettings->PingInMs = -1;
				debugfLiveSlow(NAME_DevOnline,TEXT("QoS for %s is %d"),
					*ServerSettings->OwningPlayerName,ServerSettings->PingInMs);
#if !FINAL_RELEASE
				if (QosData->axnqosinfo[Index].pbData == NULL)
				{
					debugfLiveSlow(NAME_DevOnline,TEXT("NULL data for QoS packet at index (%d)"),Index);
				}
				if (QosData->axnqosinfo[Index].cbData == 0)
				{
					debugfLiveSlow(NAME_DevOnline,TEXT("Zero bytes indicated for QoS packet at index (%d)"),Index);
				}
#endif
			}
		}
		// Make a second pass through the search results and pull out any
		// that had partial QoS data. This can't be done during QoS parsing
		// since the indices need to match then
		for (INT Index = 0; Index < GameSearch->Results.Num(); Index++)
		{
			FOnlineGameSearchResult& SearchResult = GameSearch->Results(Index);
			// If any of the fields are missing on a valid server, remove this item from the list			
			if ((SearchResult.GameSettings->ServerNonce == 0 && GameSearch->bUsesArbitration) ||
				SearchResult.GameSettings->OwningPlayerName.Len() == 0 ||
				(XUID&)SearchResult.GameSettings->OwningPlayerId == 0 ||
				SearchResult.GameSettings->BuildUniqueId != GetBuildUniqueId())
			{
				debugfLiveSlow(NAME_DevOnline,TEXT("Removing server with malformed QoS data at index %d"),Index);
				// Log incompatible builds if present
				if (SearchResult.GameSettings->BuildUniqueId != GetBuildUniqueId())
				{
					debugfLiveSlow(NAME_DevOnline,
						TEXT("Removed incompatible builds: GameSettings->BuildUniqueId = 0x%08x, GetBuildUniqueId() = 0x%08x"),
						SearchResult.GameSettings->BuildUniqueId,
						GetBuildUniqueId());
				}
				// Free the data
				delete (XSESSION_INFO*)SearchResult.PlatformData;
				// And then remove from the list
				GameSearch->Results.Remove(Index);
				Index--;
			}
		}
		// Allow game code to sort the servers
		GameSearch->eventSortSearchResults();
	}
	else
	{
		GameSearch->Results.Empty();
		debugfLiveSlow(NAME_Warning,TEXT("QoS data for servers doesn't match up, skipping"));
	}
}

/**
 * Creates a new Live enabled game for the requesting player using the
 * settings specified in the game settings object
 *
 * @param HostingPlayerNum the player hosting the game
 * @param Session the named session for this live match
 *
 * @return The result from the Live APIs
 */
DWORD UOnlineSubsystemLive::CreateLiveGame(BYTE HostingPlayerNum,FNamedSession* Session)
{
	check(Session && Session->GameSettings && Session->SessionInfo);
	UOnlineGameSettings* GameSettings = Session->GameSettings;
	FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
	debugf(NAME_DevOnline,TEXT("Creating a %s match"),GameSettings->bIsLanMatch ? TEXT("System Link") : TEXT("Live"));
#if DEBUG_CONTEXT_LOGGING
	// Log game settings
	DumpGameSettings(GameSettings);
	// Log properties and contexts
	DumpContextsAndProperties(GameSettings);
#endif
	// For each local player, force them to use the same props/contexts
	for (DWORD Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
	{
		// Ignore non-Live enabled profiles
		if (XUserGetSigninState(Index) != eXUserSigninState_NotSignedIn)
		{
			// Register all of the context/property information for the session
			SetContextsAndProperties(Index,GameSettings);
		}
	}
	// Get the flags for the session
	DWORD Flags = BuildSessionFlags(GameSettings);
	// Save off the session flags
	SessionInfo->Flags = Flags;
	// Create a new async task for handling the creation
	FOnlineAsyncTaskLive* AsyncTask = new FLiveAsyncTaskCreateSession(Session->SessionName,HostingPlayerNum);
	// Now create the session
	DWORD Return = XSessionCreate(Flags,
		HostingPlayerNum,
		GameSettings->NumPublicConnections,
		GameSettings->NumPrivateConnections,
		&SessionInfo->Nonce,
		&SessionInfo->XSessionInfo,
		*AsyncTask,
		&SessionInfo->Handle);
	debugf(NAME_DevOnline,TEXT("XSessionCreate '%s' (%d,%d,%d,%d,Nonce,SessInfo,Data,OutHandle) returned 0x%08X"),
		*Session->SessionName.ToString(),
		Flags,(DWORD)HostingPlayerNum,GameSettings->NumPublicConnections,
		GameSettings->NumPrivateConnections,Return);
	if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
	{
		// Add the task for tracking since the call worked
		AsyncTasks.AddItem(AsyncTask);
	}
	else
	{
		// Don't leak the task in this case
		delete AsyncTask;
	}
	return Return;
}

/**
 * Creates a new system link enabled game. Registers the keys/nonce needed
 * for secure communication
 *
 * @param HostingPlayerNum the player hosting the game
 * @param Session the named session for this lan game
 *
 * @return The result code from the nonce/key APIs
 */
DWORD UOnlineSubsystemLive::CreateLanGame(BYTE HostingPlayerNum,FNamedSession* Session)
{
	check(Session && Session->GameSettings && Session->SessionInfo);
	DWORD Return = ERROR_SUCCESS;
	// Don't create a system link beacon if advertising is off
	if (Session->GameSettings->bShouldAdvertise == TRUE)
	{
		if (LanBeacon != NULL)
		{
			// Cleanup old beacon if not destroyed
			StopLanBeacon();
		}
		// Bind a socket for system link activity
		LanBeacon = new FLanBeacon();
		if (LanBeacon->Init(LanAnnouncePort))
		{
			// We successfully created everything so mark the socket as
			// needing polling
			LanBeaconState = LANB_Hosting;
			debugfLiveSlow(NAME_DevOnline,TEXT("Listening for beacon requestes on %d"),
				LanAnnouncePort);
		}
		else
		{
			debugfLiveSlow(NAME_Error,TEXT("Failed to init to system link beacon %s"),
				GSocketSubsystem->GetSocketError());
			Return = XNET_CONNECT_STATUS_LOST;
		}
	}
	return Return;
}

/**
 * Updates the localized settings/properties for the game in question. Updates
 * the QoS packet if needed (starting & restarting QoS).
 *
 * @param SessionName the session that is being updated
 * @param UpdatedGameSettings the settings to use for the new game session
 * @param bShouldRefreshOnlineData whether to submit the data to the backend or not
 *
 * @return true if successful creating the session, false otherwsie
 */
UBOOL UOnlineSubsystemLive::UpdateOnlineGame(FName SessionName,UOnlineGameSettings* UpdatedGameSettings,UBOOL bShouldRefreshOnlineData)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Find the session in question
	FNamedSession* Session = GetNamedSession(SessionName);
	if (Session &&
		Session->GameSettings &&
		UpdatedGameSettings)
	{
		// If they specified a different object copy over the settings to the
		// existing one
		if (UpdatedGameSettings != Session->GameSettings)
		{
			CopyGameSettings(Session->GameSettings,UpdatedGameSettings);
		}
		// Don't update QoS data when not the host
		if (IsServer())
		{
			// Determine if this is a lan match or Live to see if we need to change QoS
			if (Session->GameSettings->bIsLanMatch == FALSE)
			{
				// only update QoS listener if packet contents changed
				BYTE TempQoSPacket[MAX_QOSPACKET_SIZE];
				DWORD TempQoSPacketLen;
				GenerateQoSPacket(TempQoSPacket,TempQoSPacketLen,Session->GameSettings);
				UBOOL bQosValuesChanged = appMemcmp(TempQoSPacket,QoSPacket,512) != 0;
				if (bQosValuesChanged)
				{
					// Unregister our QoS packet data so that it can be updated
					UnregisterQoS(Session);
					// Now reregister with the new info
					Return = RegisterQoS(Session);
				}
			}
		}
#if DEBUG_CONTEXT_LOGGING
		// Log game settings
		DumpGameSettings(Session->GameSettings);
		// Log properties and contexts
		DumpContextsAndProperties(Session->GameSettings);
#endif
		// Now update all of the props/contexts for all players
		for (DWORD Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
		{
			// Ignore non-Live enabled profiles
			if (XUserGetSigninState(Index) != eXUserSigninState_NotSignedIn)
			{
				// Register all of the context/property information for the session
				SetContextsAndProperties(Index,Session->GameSettings);
			}
		}
		// If requested, update the session information
		if (bShouldRefreshOnlineData &&
			AreAnySignedIntoLive())
		{
			Return = ModifySession(Session,&UpdateOnlineGameCompleteDelegates);
		}
		else
		{
			Return = ERROR_SUCCESS;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Can't update (%s) game settings with a NULL object"),
			*SessionName.ToString());
	}
	if (Return != ERROR_IO_PENDING)
	{
		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResultsNamedSession Results(SessionName,Return);
		TriggerOnlineDelegates(this,UpdateOnlineGameCompleteDelegates,&Results);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Destroys the current online game
 *
 * @param SessionName the session that is being destroyed
 *
 * @return true if successful destroying the session, false otherwise
 */
UBOOL UOnlineSubsystemLive::DestroyOnlineGame(FName SessionName)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Find the session in question
	FNamedSession* Session = GetNamedSession(SessionName);
	// Don't shut down if it isn't valid
	if (Session && Session->GameSettings && Session->SessionInfo)
	{
		UBOOL bWasDedicatedServer = Session->GameSettings->bIsDedicated;

		// Shutdown the lan beacon if needed
		if (Session->GameSettings->bIsLanMatch &&
			Session->GameSettings->bShouldAdvertise)
		{
			// Tear down the system link beacon
			StopLanBeacon();
		}
		// Unregister our QoS packet data and stop handling the queries
		UnregisterQoS(Session);
		Return = DestroyLiveGame(Session);
		// The session info is no longer needed
		RemoveNamedSession(Session->SessionName);
		// Only unregister everything if we no longer have sessions
		if (Sessions.Num() == 0)
		{
			if (bWasDedicatedServer == FALSE)
			{
				// Stop all local talkers (avoids a debug runtime warning)
				UnregisterLocalTalkers();
			}
			// Stop all remote voice before ending the session
			RemoveAllRemoteTalkers();
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Can't destroy a null online session (%s)"),
			*SessionName.ToString());
	}
	if (Return != ERROR_IO_PENDING)
	{
		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResultsNamedSession Results(SessionName,Return);
		TriggerOnlineDelegates(this,DestroyOnlineGameCompleteDelegates,&Results);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Terminates a Live session
 *
 * @param Session the named session that is being destroyed
 *
 * @return The result from the Live APIs
 */
DWORD UOnlineSubsystemLive::DestroyLiveGame(FNamedSession* Session)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
	FOnlineAsyncTaskLive* AsyncTask = NULL;

	if (AreAnySignedIntoLive())
	{	
		// Create a new async task for handling the deletion
		AsyncTask = new FLiveAsyncDestroySession(Session->SessionName,
			SessionInfo->Handle,
			NULL,
			&DestroyOnlineGameCompleteDelegates);
		// Shutdown the session asynchronously
		Return = XSessionDelete(SessionInfo->Handle,*AsyncTask);
		debugf(NAME_DevOnline,
			TEXT("XSessionDelete() '%s' returned 0x%08X"),
			*Session->SessionName.ToString(),
			Return);
	}
	else
	{
		// Holds data for all possible local players during unregistration
		FLiveAsyncTaskDataUnregisterLocalPlayers* AsyncTaskData = new FLiveAsyncTaskDataUnregisterLocalPlayers();
		for (INT Idx=0; Idx < MAX_LOCAL_PLAYERS; Idx++)
		{
			AsyncTaskData->AddPlayer(Idx);
		}
		// Create a new async task for handling the unregistration
		AsyncTask = new FLiveAsyncDestroySession(Session->SessionName,
			SessionInfo->Handle,
			AsyncTaskData,
			&DestroyOnlineGameCompleteDelegates);
		// Unregister all local players so that they can be registered on the new session if recreated
		Return = XSessionLeaveLocal(SessionInfo->Handle,
			AsyncTaskData->GetCount(),
			AsyncTaskData->GetPlayers(),
			*AsyncTask);
		debugf(NAME_DevOnline,
			TEXT("XSessionLeaveLocal() '%s' returned 0x%08X"),
			*Session->SessionName.ToString(),
			Return);

		// Make sure we don't leak secure key for session
		DWORD KeyUnregisterResult = XNetUnregisterKey(&SessionInfo->XSessionInfo.sessionID);
		debugf(NAME_DevOnline,
			TEXT("XSessionDelete() skipped since nobody is logged in. XNetUnregisterKey() returned 0x%08X"),
				KeyUnregisterResult);
	}
	if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
	{
		// Add the task to the list to be ticked later
		AsyncTasks.AddItem(AsyncTask);
	}
	else
	{
		// Don't leak the task
		delete AsyncTask;
	}
	return Return;
}

/**
 * Allocates the space/structure needed for holding the search results plus
 * any resources needed for async support
 *
 * @param SearchingPlayerNum the index of the player searching for the match
 * @param QueryNum the unique id of the query to be run
 * @param MaxSearchResults the maximum number of search results we want
 * @param NumBytes the out param indicating the size that was allocated
 *
 * @return The data allocated for the search (space plus overlapped)
 */
FLiveAsyncTaskDataSearch* UOnlineSubsystemLive::AllocateSearch(BYTE SearchingPlayerNum,
	DWORD QueryNum,DWORD MaxSearchResults,DWORD& NumBytes)
{
	// Use the search code to determine the size buffer we need
    DWORD Return = XSessionSearch(QueryNum,SearchingPlayerNum,MaxSearchResults,
        0,0,NULL,NULL,&NumBytes,NULL,NULL);
	FLiveAsyncTaskDataSearch* Data = NULL;
	// Only allocate the buffer if the call worked ok
	if (Return == ERROR_INSUFFICIENT_BUFFER && NumBytes > 0)
	{
		Data = new FLiveAsyncTaskDataSearch(NumBytes);
	}
	return Data;
}

/**
 * Builds a Live/system link search query and sends it off to be processed. Uses
 * the search settings passed in to populate the query.
 *
 * @param SearchingPlayerNum the index of the player searching for the match
 * @param SearchSettings the game settings that we are interested in
 *
 * @return TRUE if the search was started successfully, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::FindOnlineGames(BYTE SearchingPlayerNum,
	UOnlineGameSearch* SearchSettings)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Verify that we have valid search settings
	if (SearchSettings != NULL)
	{
		// Don't start another while in progress or multiple entries for the
		// same server will show up in the server list
		if (SearchSettings->bIsSearchInProgress == FALSE)
		{
			// Free up previous results
			FreeSearchResults();
			// Check for Live or Systemlink
			if (SearchSettings->bIsLanQuery == FALSE)
			{
				// If they have manually requested a skill search and the data
				// has been read from Live, then do the search with that skill data
				// or if they have not requested a skill override (both arrays are empty)
				if (SearchSettings->ManualSkillOverride.Players.Num() == SearchSettings->ManualSkillOverride.Mus.Num())
				{
					Return = FindLiveGames(SearchingPlayerNum,SearchSettings);
				}
				else
				{
					// Perform the skill leaderboard read async
					Return = ReadSkillForSearch(SearchingPlayerNum,SearchSettings);
				}
			}
			else
			{
				// Copy the search pointer so we can keep it around
				GameSearch = SearchSettings;
				Return = FindLanGames();
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Ignoring game search request while one is pending"));
			Return = ERROR_IO_PENDING;
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Can't search with null criteria"));
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Copies the Epic structures into the Live equivalent
 *
 * @param DestProps the destination properties
 * @param SourceProps the source properties
 */
DWORD UOnlineSubsystemLive::CopyPropertiesForSearch(PXUSER_PROPERTY DestProps,
	const TArray<FSettingsProperty>& SourceProps)
{
	DWORD Count = 0;
	appMemzero(DestProps,sizeof(XUSER_PROPERTY) * SourceProps.Num());
	// Loop through the properties and copy them over
	for (INT Index = 0; Index < SourceProps.Num(); Index++)
	{
		const FSettingsProperty& Prop = SourceProps(Index);

		// Now copy the held data (no strings or blobs as they aren't supported)
		switch (Prop.Data.Type)
		{
			case SDT_Float:
			{
				// Copy property id and type
				DestProps[Count].dwPropertyId = Prop.PropertyId;
				Prop.Data.GetData(DestProps[Count].value.fData);
				DestProps[Count].value.type = XUSER_DATA_TYPE_FLOAT;
				Count++;
				break;
			}
			case SDT_Int32:
			{
				// Copy property id and type
				DestProps[Count].dwPropertyId = Prop.PropertyId;
				Prop.Data.GetData((INT&)DestProps[Count].value.nData);
				DestProps[Count].value.type = XUSER_DATA_TYPE_INT32;
				Count++;
				break;
			}
			case SDT_Int64:
			{
				// Copy property id and type
				DestProps[Count].dwPropertyId = Prop.PropertyId;
				Prop.Data.GetData((QWORD&)DestProps[Count].value.i64Data);
				DestProps[Count].value.type = XUSER_DATA_TYPE_INT64;
				Count++;
				break;
			}
			case SDT_Double:
			{
				// Copy property id and type
				DestProps[Count].dwPropertyId = Prop.PropertyId;
				Prop.Data.GetData(DestProps[Count].value.dblData);
				DestProps[Count].value.type = XUSER_DATA_TYPE_DOUBLE;
				Count++;
				break;
			}
			case SDT_Blob:
			case SDT_String:
			{
				debugfLiveSlow(NAME_DevOnline,
					TEXT("Ignoring property (%d) for search as blobs/strings aren't supported by Live"),
					Prop.PropertyId);
				break;
			}
			case SDT_DateTime:
			{
				// Copy property id and type
				DestProps[Count].dwPropertyId = Prop.PropertyId;
				DestProps[Count].value.ftData.dwLowDateTime = Prop.Data.Value1;
				DestProps[Count].value.ftData.dwHighDateTime = (DWORD)(PTRINT)Prop.Data.Value2;
				DestProps[Count].value.type = XUSER_DATA_TYPE_DATETIME;
				Count++;
				break;
			}
		}
	}

	return Count;
}

/**
 * Copies the Epic structures into the Live equivalent
 *
 * @param Search the object to use when determining
 * @param DestContexts the destination contexts
 * @param SourceContexts the source contexts
 *
 * @return the number of items copied (handles skipping for wildcards)
 */
DWORD UOnlineSubsystemLive::CopyContextsForSearch(UOnlineGameSearch* Search,
	PXUSER_CONTEXT DestContexts,
	const TArray<FLocalizedStringSetting>& SourceContexts)
{
	DWORD Count = 0;
	// Iterate through the source contexts and copy any that aren't wildcards
	for (INT Index = 0; Index < SourceContexts.Num(); Index++)
	{
		const FLocalizedStringSetting& Setting = SourceContexts(Index);
		// Don't copy if the item is meant to use wildcard matching
		if (Search->IsWildcardStringSetting(Setting.Id) == FALSE)
		{
			DestContexts[Count].dwContextId = Setting.Id;
			DestContexts[Count].dwValue = Setting.ValueIndex;
			Count++;
		}
	}
	return Count;
}

/**
 * Builds a Live game query and submits it to Live for processing
 *
 * @param SearchingPlayerNum the player searching for games
 * @param SearchSettings the settings that the player is interested in
 *
 * @return The result from the Live APIs
 */
DWORD UOnlineSubsystemLive::FindLiveGames(BYTE SearchingPlayerNum,UOnlineGameSearch* SearchSettings)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Get to our Live specific settings so we can check arbitration & max results
	DWORD MaxSearchResults = Clamp(SearchSettings->MaxSearchResults,0,XSESSION_SEARCH_MAX_RETURNS);
	DWORD QueryId = SearchSettings->Query.ValueIndex;
	DWORD NumResultBytes = 0;
	// Now allocate the search data bucket
	FLiveAsyncTaskDataSearch* SearchData = AllocateSearch(SearchingPlayerNum,
		QueryId,MaxSearchResults,NumResultBytes);
	if (SearchData != NULL)
	{
		// Figure out the game type we want to search for
		DWORD GameType = X_CONTEXT_GAME_TYPE_STANDARD;
		if (SearchSettings->bUsesArbitration == TRUE)
		{
			GameType = X_CONTEXT_GAME_TYPE_RANKED;
		}
		// Append the required contexts if missing
		SearchSettings->SetStringSettingValue(X_CONTEXT_GAME_TYPE,
			GameType,TRUE);
		// Append the skill properties to override the searching
		AppendSkillProperties(SearchSettings);
		// Allocate space to hold the properties array
		PXUSER_PROPERTY Properties = SearchData->AllocateProperties(SearchSettings->Properties.Num());
		// Copy property data over
		DWORD NumProperties = CopyPropertiesForSearch(Properties,SearchSettings->Properties);
		// Allocate space to hold the contexts array
		PXUSER_CONTEXT Contexts = SearchData->AllocateContexts(SearchSettings->LocalizedSettings.Num());
		// Copy contexts data over
		DWORD NumContexts = CopyContextsForSearch(SearchSettings,Contexts,SearchSettings->LocalizedSettings);
#if DEBUG_CONTEXT_LOGGING
		// Log properties and contexts
		DumpContextsAndProperties(SearchSettings);
#endif

		// Create a new async task for handling the async
		FOnlineAsyncTaskLive* AsyncTask = new FLiveAsyncTaskSearch(&QosStatusChangedDelegates,
			&FindOnlineGamesCompleteDelegates,
			SearchData);
		// Kick off the async search
		Return = XSessionSearch(QueryId,
			SearchingPlayerNum,
			MaxSearchResults,
			NumProperties,
			NumContexts,
#if WITH_PANORAMA // G4WLive doesn't handle non-NULL values when zero elements are passed in
			NumProperties ? Properties : NULL,
			NumContexts ? Contexts : NULL,
#else
			Properties,
			Contexts,
#endif
			&NumResultBytes,
			*SearchData,
			*AsyncTask);
		debugf(NAME_DevOnline,TEXT("XSessionSearch(%d,%d,%d,%d,%d,data,data,%d,data,data) returned 0x%08X"),
			QueryId,(DWORD)SearchingPlayerNum,MaxSearchResults,NumProperties,NumContexts,NumResultBytes,Return);
		if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
		{
			// Mark the search in progress
			SearchSettings->bIsSearchInProgress = TRUE;
			// Add the task to the list to be ticked later
			AsyncTasks.AddItem(AsyncTask);
			// Copy the search pointer so we can keep it around
			GameSearch = SearchSettings;
		}
		else
		{
			// Just trigger the delegate as having failed
			FAsyncTaskDelegateResults Results(Return);
			TriggerOnlineDelegates(this,FindOnlineGamesCompleteDelegates,&Results);
			// Don't leak the task
			delete AsyncTask;
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Failed to allocate space for the online game search"));
	}
	return Return;
}

/**
 * Reads the contexts and properties from the Live search data and populates the
 * game settings object with them
 *
 * @param SearchResult the data that was returned from Live
 * @param GameSettings the game settings that we are setting the data on
 */
void UOnlineSubsystemLive::ParseContextsAndProperties(
	XSESSION_SEARCHRESULT& SearchResult,UOnlineGameSettings* GameSettings)
{
	UOnlineGameSettings* DefaultSettings = GameSettings->GetClass()->GetDefaultObject<UOnlineGameSettings>();
	check(DefaultSettings);
	// Clear any settings that were in the defualts
	GameSettings->LocalizedSettings.Empty();
	GameSettings->Properties.Empty();
	// Check the number of contexts
	if (SearchResult.cContexts > 0)
	{
		// Pre add them so memory isn't copied
		GameSettings->LocalizedSettings.AddZeroed(SearchResult.cContexts);
		// Iterate through the contexts and add them to the GameSettings
		for (INT Index = 0; Index < (INT)SearchResult.cContexts; Index++)
		{
			FLocalizedStringSetting& Context = GameSettings->LocalizedSettings(Index);
			Context.Id = SearchResult.pContexts[Index].dwContextId;
			Context.ValueIndex = SearchResult.pContexts[Index].dwValue;
			// Look at the default object so we can determine how this is to be advertised
			FLocalizedStringSetting* DefaultContextSetting = DefaultSettings->FindStringSetting(Context.Id);
			if (DefaultContextSetting != NULL)
			{
				Context.AdvertisementType = DefaultContextSetting->AdvertisementType;
			}
			else
			{
				debugfLiveSlow(NAME_DevOnline,TEXT("Added non-advertised string setting %d with value %d"),
					Context.Id,Context.ValueIndex);
			}
		}
	}
	// And now the number of properties
	if (SearchResult.cProperties > 0)
	{
		// Pre add them so memory isn't copied
		GameSettings->Properties.AddZeroed(SearchResult.cProperties);
		// Iterate through the properties and add them to the GameSettings
		for (INT Index = 0; Index < (INT)SearchResult.cProperties; Index++)
		{
			FSettingsProperty& Property = GameSettings->Properties(Index);
			Property.PropertyId = SearchResult.pProperties[Index].dwPropertyId;
			// Copy the data over (may require allocs for strings)
			CopyXDataToSettingsData(Property.Data,SearchResult.pProperties[Index].value);
			// Look at the default object so we can determine how this is to be advertised
			FSettingsProperty* DefaultPropertySetting = DefaultSettings->FindProperty(Property.PropertyId);
			if (DefaultPropertySetting != NULL)
			{
				Property.AdvertisementType = DefaultPropertySetting->AdvertisementType;
			}
			else
			{
				debugfLiveSlow(NAME_DevOnline,TEXT("Adding non-advertised property 0x%08X (%d) = %s"),
					Property.PropertyId,Property.Data.Type,*Property.Data.ToString());
			}
			// Copy the hostname into the OwningPlayerName field if this is it
			if (SearchResult.pProperties[Index].dwPropertyId == X_PROPERTY_GAMER_HOSTNAME)
			{
				GameSettings->OwningPlayerName = SearchResult.pProperties[Index].value.string.pwszData;
			}
		}
	}
}

/**
 * Parses the search results into something the game play code can handle
 *
 * @param Search the Unreal search object
 * @param SearchResults the buffer filled by Live
 */
void UOnlineSubsystemLive::ParseSearchResults(UOnlineGameSearch* Search,
	PXSESSION_SEARCHRESULT_HEADER SearchResults)
{
	if (Search != NULL)
	{
		DOUBLE Mu, Sigma, PlayerCount;
		GetLocalSkills(Mu,Sigma,PlayerCount);
		check(SearchResults != NULL);
		// Loop through the results copying the info over
		for (DWORD Index = 0; Index < SearchResults->dwSearchResults; Index++)
		{
			// Matchmaking should never return full servers, but just in case
			if (SearchResults->pResults[Index].dwOpenPrivateSlots > 0 ||
				SearchResults->pResults[Index].dwOpenPublicSlots > 0)
			{
				// Create an object that we'll copy the data to
				UOnlineGameSettings* NewServer = ConstructObject<UOnlineGameSettings>(
					Search->GameSettingsClass);
				if (NewServer != NULL)
				{
					// Add space in the search results array
					INT NewSearch = Search->Results.Add();
					FOnlineGameSearchResult& Result = Search->Results(NewSearch);
					// Whether arbitration is used or not comes from the search
					NewServer->bUsesArbitration = Search->bUsesArbitration;
					// Now copy the data
					Result.GameSettings = NewServer;
					NewServer->NumOpenPrivateConnections = SearchResults->pResults[Index].dwOpenPrivateSlots;
					NewServer->NumOpenPublicConnections = SearchResults->pResults[Index].dwOpenPublicSlots;
					// Determine the total slots for the match (used + open)
					NewServer->NumPrivateConnections = SearchResults->pResults[Index].dwOpenPrivateSlots +
						SearchResults->pResults[Index].dwFilledPrivateSlots;
					NewServer->NumPublicConnections = SearchResults->pResults[Index].dwOpenPublicSlots +
						SearchResults->pResults[Index].dwFilledPublicSlots;
					// Read the various contexts and properties from the search
					ParseContextsAndProperties(SearchResults->pResults[Index],NewServer);
					// Allocate and copy the Live specific data
					XSESSION_INFO* SessInfo = new XSESSION_INFO;
					appMemcpy(SessInfo,&SearchResults->pResults[Index].info,sizeof(XSESSION_INFO));
					// Determine the match quality for this search result
					CalculateMatchQuality(Mu,Sigma,PlayerCount,NewServer);
					// Store this in the results and mark them as needing proper clean ups
					Result.PlatformData = SessInfo;
				}
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("No search object to store results on!"));
	}
}

/**
 * Cancels the current search in progress if possible for that search type
 *
 * @return true if successful searching for sessions, false otherwise
 */
UBOOL UOnlineSubsystemLive::CancelFindOnlineGames(void)
{
	DWORD Return = E_FAIL;
	if (GameSearch != NULL &&
		GameSearch->bIsSearchInProgress)
	{
		// Make sure it's the right type
		if (GameSearch->bIsLanQuery)
		{
			Return = ERROR_SUCCESS;
			StopLanBeacon();
			GameSearch->bIsSearchInProgress = FALSE;
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Can't cancel a Player/Ranked search only LAN/List Play"));
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Can't cancel a search that isn't in progress"));
	}
	if (Return != ERROR_IO_PENDING)
	{
		FAsyncTaskDelegateResults Results(Return);
		TriggerOnlineDelegates(this,CancelFindOnlineGamesCompleteDelegates,&Results);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Pack all relevant game settings data into a DWORD
 *
 * @param GameSettings settings object to read from
 * @return DWORD of packed values
 */
static DWORD PackGameSettingsData(const UOnlineGameSettings* GameSettings)
{
	checkSlow(GameSettings != NULL);
	// GameSettingsData DWORD contains | 8bit NumPublicConnections | 8bit NumPrivateConnections | 16bit bool flags |
	DWORD GameSettingsData=0, BitFlagsShift=0;
	GameSettingsData |= GameSettings->bShouldAdvertise << BitFlagsShift++;
	GameSettingsData |= GameSettings->bIsLanMatch << BitFlagsShift++;
	GameSettingsData |= GameSettings->bUsesStats << BitFlagsShift++;
	GameSettingsData |= GameSettings->bAllowJoinInProgress << BitFlagsShift++;
	GameSettingsData |= GameSettings->bAllowInvites << BitFlagsShift++;
	GameSettingsData |= GameSettings->bUsesPresence << BitFlagsShift++;
	GameSettingsData |= GameSettings->bAllowJoinViaPresence << BitFlagsShift++;
	GameSettingsData |= GameSettings->bAllowJoinViaPresenceFriendsOnly << BitFlagsShift++;
	GameSettingsData |= GameSettings->bUsesArbitration << BitFlagsShift++;
	GameSettingsData |= GameSettings->bAntiCheatProtected << BitFlagsShift++;
	GameSettingsData |= GameSettings->bIsDedicated << BitFlagsShift++;
	GameSettingsData |= (BYTE)GameSettings->NumPrivateConnections << 16;
	GameSettingsData |= (BYTE)GameSettings->NumPublicConnections << 24;
	return GameSettingsData;
}

/**
 * Unpack all relevant game settings data from a DWORD
 *
 * @param GameSettings settings object to write new settings into
 * @param GameSettingsData DWORD of packed values to read from
 */
static void UnpackGameSettingsData(UOnlineGameSettings* GameSettings,DWORD GameSettingsData)
{
	checkSlow(GameSettings != NULL);
	// GameSettingsData DWORD contains | 8bit NumPublicConnections | 8bit NumPrivateConnections | 16bit bool flags |
	DWORD BitFlagsShift=0;
	GameSettings->bShouldAdvertise = (GameSettingsData >> BitFlagsShift++) & 1;
	GameSettings->bIsLanMatch = (GameSettingsData >> BitFlagsShift++) & 1;
	GameSettings->bUsesStats = (GameSettingsData >> BitFlagsShift++) & 1;
	GameSettings->bAllowJoinInProgress = (GameSettingsData >> BitFlagsShift++) & 1;
	GameSettings->bAllowInvites = (GameSettingsData >> BitFlagsShift++) & 1;
	GameSettings->bUsesPresence = (GameSettingsData >> BitFlagsShift++) & 1;
	GameSettings->bAllowJoinViaPresence = (GameSettingsData >> BitFlagsShift++) & 1;
	GameSettings->bAllowJoinViaPresenceFriendsOnly = (GameSettingsData >> BitFlagsShift++) & 1;
	GameSettings->bUsesArbitration = (GameSettingsData >> BitFlagsShift++) & 1;
	GameSettings->bAntiCheatProtected = (GameSettingsData >> BitFlagsShift++) & 1;
	GameSettings->bIsDedicated = (GameSettingsData >> BitFlagsShift++) & 1;				
	GameSettings->NumPrivateConnections = (GameSettingsData >> 16) & 0xFF;
	GameSettings->NumPublicConnections = (GameSettingsData >> 24) & 0xFF;
	// Private matches need to set their open slots for joining to work
	if (GameSettings->NumPublicConnections == 0)
	{
		// The join code expects this to be populated correctly, so that it knows to consume private slots
		GameSettings->NumOpenPrivateConnections = GameSettings->NumPrivateConnections;
	}
}


/**
 * Serializes the platform specific data into the provided buffer for the specified search result
 *
 * @param DesiredGame the game to copy the platform specific data for
 * @param PlatformSpecificInfo the buffer to fill with the platform specific information
 *
 * @return true if successful serializing the data, false otherwise
 */
UBOOL UOnlineSubsystemLive::ReadPlatformSpecificSessionInfo(const FOnlineGameSearchResult& DesiredGame,BYTE* PlatformSpecificInfo)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	if (DesiredGame.GameSettings && DesiredGame.PlatformData)
	{
		FNboSerializeToBufferXe Buffer;
		XSESSION_INFO* SessionInfo = (XSESSION_INFO*)DesiredGame.PlatformData;

		// Pack game settings into DWORD
		DWORD GameSettingsData = PackGameSettingsData(DesiredGame.GameSettings);

		// Write host info (host addr, session id, and key)
		Buffer << SessionInfo->hostAddress
			<< SessionInfo->sessionID
			<< SessionInfo->keyExchangeKey
			// Read the nonce to the game settings
			<< DesiredGame.GameSettings->ServerNonce
			// Write the owner of the game session
			<< DesiredGame.GameSettings->OwningPlayerId
			// Write the open slots and bit flags from the game settings
			<< GameSettingsData;

		if (Buffer.GetByteCount() <= 80)
		{
			// Copy the built up data
			appMemcpy(PlatformSpecificInfo,Buffer.GetRawBuffer(0),Buffer.GetByteCount());
			Return = ERROR_SUCCESS;
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Platform data is larger (%d) than the supplied buffer (80)"),
				Buffer.GetByteCount());
		}
	}
	return Return == ERROR_SUCCESS;
}

/**
 * Serializes the platform specific data into the provided buffer for the specified settings object.
 * NOTE: This can only be done for a session that is bound to the online system
 *
 * @param GameSettings the game to copy the platform specific data for
 * @param PlatformSpecificInfo the buffer to fill with the platform specific information
 *
 * @return true if successful reading the data for the session, false otherwise
 */
UBOOL UOnlineSubsystemLive::ReadPlatformSpecificSessionInfoBySessionName(FName SessionName,BYTE* PlatformSpecificInfo)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Look up the session by name and copy the session data from it
	FNamedSession* Session = GetNamedSession(SessionName);
	if (Session &&
		Session->SessionInfo)
	{
		FNboSerializeToBufferXe Buffer;
		FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);

		// Pack game settings into DWORD
		DWORD GameSettingsData = PackGameSettingsData(Session->GameSettings);

		// Write host info (host addr, session id, and key)
		Buffer << SessionInfo->XSessionInfo.hostAddress
			<< SessionInfo->XSessionInfo.sessionID
			<< SessionInfo->XSessionInfo.keyExchangeKey
			// Write the nonce from the game settings
			<< Session->GameSettings->ServerNonce
			// Write the owner of the game session
			<< Session->GameSettings->OwningPlayerId
			// Write the open slots and bit flags from the game settings
			<< GameSettingsData;

		if (Buffer.GetByteCount() <= 80)
		{
			// Copy the built up data
			appMemcpy(PlatformSpecificInfo,Buffer.GetRawBuffer(0),Buffer.GetByteCount());
			Return = ERROR_SUCCESS;
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Platform data is larger (%d) than the supplied buffer (80)"),
				Buffer.GetByteCount());
		}
	}
	return Return == ERROR_SUCCESS;
}

/**
 * Creates a search result out of the platform specific data and adds that to the specified search object
 *
 * @param SearchingPlayerNum the index of the player searching for a match
 * @param SearchSettings the desired search to bind the session to
 * @param PlatformSpecificInfo the platform specific information to convert to a server object
 *
 * @return true if successful searching for sessions, false otherwise
 */
UBOOL UOnlineSubsystemLive::BindPlatformSpecificSessionToSearch(BYTE SearchingPlayerNum,UOnlineGameSearch* SearchSettings,BYTE* PlatformSpecificInfo)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Verify that we have valid search settings
	if (SearchSettings != NULL)
	{
		// Don't start another while in progress or multiple entries for the
		// same server will show up in the server list
		if (GameSearch == NULL ||
			GameSearch->bIsSearchInProgress == FALSE)
		{
			// Free up previous results
			FreeSearchResults();
			// Copy the search pointer so we can keep it around
			GameSearch = SearchSettings;
			// Create a new server and assign it the session key info
			UOnlineGameSettings* NewServer = ConstructObject<UOnlineGameSettings>(
				SearchSettings->GameSettingsClass);
			if (NewServer != NULL)
			{
				// Add space in the search results array
				INT NewSearch = SearchSettings->Results.Add();
				FOnlineGameSearchResult& Result = SearchSettings->Results(NewSearch);
				// Now copy the data
				Result.GameSettings = NewServer;
				// Allocate and read the Live secure key info
				XSESSION_INFO* SessInfo = new XSESSION_INFO;
				// Use our reader class to read from the buffer
				FNboSerializeFromBufferXe Buffer(PlatformSpecificInfo,80);
				// Read the connection data
				DWORD GameSettingsData=0, BitFlagsShift=0;
				Buffer >> SessInfo->hostAddress
					>> SessInfo->sessionID
					>> SessInfo->keyExchangeKey
					// Add the nonce to the game settings
					>> NewServer->ServerNonce
					// Read the owner of the game session
					>> NewServer->OwningPlayerId
					// Read the open slots and bit flags of the game settings
					>> GameSettingsData;
				// Update game settings object with new values
				UnpackGameSettingsData(NewServer,GameSettingsData);
				// Store this in the results so they can join it
				Result.PlatformData = SessInfo;
				Return = ERROR_SUCCESS;
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Ignoring bind to game search request while a search is pending"));
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Can't bind to a search that is null"));
	}
	return Return == ERROR_SUCCESS;
}

/**
 * Cleans up the Live specific session data contained in the search results
 *
 * @param Search the object to free the previous results from
 *
 * @return TRUE if it could, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::FreeSearchResults(UOnlineGameSearch* Search)
{
	UBOOL bDidFree = FALSE;
	// If they didn't pass on object in, they meant for us to use the current one
	if (Search == NULL)
	{
		Search = GameSearch;
	}
	if (Search != NULL)
	{
		if (Search->bIsSearchInProgress == FALSE)
		{
			// Loop through the results freeing the session info pointers
			for (INT Index = 0; Index < Search->Results.Num(); Index++)
			{
				FOnlineGameSearchResult& Result = Search->Results(Index);
				if (Result.PlatformData != NULL)
				{
					// Free the data and clear the leak detection flag
					delete (XSESSION_INFO*)Result.PlatformData;
				}
			}
			Search->Results.Empty();
			bDidFree = TRUE;
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Can't free search results while the search is in progress"));
		}
	}
	return bDidFree;
}

/**
 * Joins the game specified. This creates the session, decodes the IP address,
 * then kicks off the connection process
 *
 * @param PlayerNum the index of the player searching for a match
 * @param SessionName the name of the session that is being joined
 * @param DesiredGame the desired game to join
 *
 * @return true if successful destroying the session, false otherwsie
 */
UBOOL UOnlineSubsystemLive::JoinOnlineGame(BYTE PlayerNum,FName SessionName,
	const FOnlineGameSearchResult& DesiredGame)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	FNamedSession* Session = GetNamedSession(SessionName);
	// Don't join a session if already in one or hosting one
	if (Session == NULL)
	{
		// Add a named session and set it's game settings
		Session = AddNamedSession(SessionName,DesiredGame.GameSettings);
		FSecureSessionInfo* SessionInfo = new FSecureSessionInfo();
		Session->SessionInfo = SessionInfo;
		// Copy the session info over
		appMemcpy(&SessionInfo->XSessionInfo,DesiredGame.PlatformData,
			sizeof(XSESSION_INFO));
		// The session nonce needs to come from the game settings when joining
		SessionInfo->Nonce = Session->GameSettings->ServerNonce;
		// Fill in Live specific data
		Return = JoinLiveGame(PlayerNum,Session,Session->GameSettings->bWasFromInvite);
		if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
		{
			// Clean up the session info so we don't get into a confused state
			RemoveNamedSession(SessionName);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Session (%s) already exists, can't join twice"),*SessionName.ToString());
	}
	if (Return != ERROR_IO_PENDING)
	{
		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResultsNamedSession Results(SessionName,Return);
		TriggerOnlineDelegates(this,JoinOnlineGameCompleteDelegates,&Results);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Joins a Live game by creating the session without hosting it
 *
 * @param PlayerNum the player joining the game
 * @param Session the session the join is being processed on
 * @param bIsFromInvite whether this join is from invite or search
 *
 * @return The result from the Live APIs
 */
DWORD UOnlineSubsystemLive::JoinLiveGame(BYTE PlayerNum,FNamedSession* Session,
	UBOOL bIsFromInvite)
{
	FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
	debugf(NAME_DevOnline,TEXT("Joining a Live match 0x%016I64X"),(QWORD&)SessionInfo->XSessionInfo.sessionID);
	// Register all of the context/property information for the session
	SetContextsAndProperties(PlayerNum,Session->GameSettings);
	// Get the flags for the session
	DWORD Flags = BuildSessionFlags(Session->GameSettings);
	// Strip off the hosting flag if specified
	Flags &= ~XSESSION_CREATE_HOST;
	// Save off the session flags
	SessionInfo->Flags = Flags;
	// Create a new async task for handling the creation/joining
	FOnlineAsyncTaskLive* AsyncTask = new FLiveAsyncTaskCreateSession(Session->SessionName,
		PlayerNum,
		FALSE,
		bIsFromInvite);
	// Now create the session so we can decode the IP address
	DWORD Return = XSessionCreate(Flags,
		PlayerNum,
		Session->GameSettings->NumPublicConnections,
		Session->GameSettings->NumPrivateConnections,
		&SessionInfo->Nonce,
		&SessionInfo->XSessionInfo,
		*AsyncTask,
		&SessionInfo->Handle);
	debugf(NAME_DevOnline,TEXT("XSessionCreate '%s' (%d,%d,%d,%d,0x%016I64X,SessInfo,Data,OutHandle) (join request) returned 0x%08X"),
		*Session->SessionName.ToString(),
		Flags,
		(DWORD)PlayerNum,
		Session->GameSettings->NumPublicConnections,
		Session->GameSettings->NumPrivateConnections,
		Session->GameSettings->ServerNonce,
		Return);
	if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
	{
		// Add the task for tracking since the call worked
		AsyncTasks.AddItem(AsyncTask);
	}
	else
	{
		// Don't leak the task
		delete AsyncTask;
	}
	return Return;
}

/**
 * Finishes creating the online game, including creating lan beacons and/or
 * list play sessions
 *
 * @param HostingPlayerNum the player starting the session
 * @param SessionName the name of the session that is being joined
 * @param JoinResult the result code from the async create operation
 * @param bIsFromInvite whether this is from an invite or not
 */
void UOnlineSubsystemLive::FinishJoinOnlineGame(DWORD HostingPlayerNum,FName SessionName,DWORD JoinResult,UBOOL bIsFromInvite)
{
	// If the task completed ok, then continue the create process
	if (JoinResult == ERROR_SUCCESS)
	{
		// Find the session they are referring to
		FNamedSession* Session = GetNamedSession(SessionName);
		if (Session && Session->GameSettings && Session->SessionInfo)
		{
			FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
			// Set the game state as pending (not started)
			Session->GameSettings->GameState = OGS_Pending;
			// Register all local folks as participants/talkers
			RegisterLocalPlayers(Session,bIsFromInvite);
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Session (%s) was missing to complete join, failing"),
				*SessionName.ToString());
			JoinResult = E_FAIL;
		}
	}
	else
	{
		// Clean up partial create/join
		RemoveNamedSession(SessionName);
	}
	// As long as there isn't an async task outstanding, fire the events
	if (JoinResult != ERROR_IO_PENDING)
	{
		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResultsNamedSession Results(SessionName,JoinResult);
		TriggerOnlineDelegates(this,JoinOnlineGameCompleteDelegates,&Results);
	}
}

/**
 * Returns the platform specific connection information for joining the match.
 * Call this function from the delegate of join completion
 *
 * @param SessionName the name of the session to resolve
 * @param ConnectInfo the out var containing the platform specific connection information
 *
 * @return true if the call was successful, false otherwise
 */
UBOOL UOnlineSubsystemLive::GetResolvedConnectString(FName SessionName,FString& ConnectInfo)
{
	UBOOL bOk = FALSE;
	// Find the session they are referring to
	FNamedSession* Session = GetNamedSession(SessionName);
	if (Session != NULL)
	{
		FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
		if (SessionInfo != NULL)
		{
			FInternetIpAddr IpAddr;
			// Figure out if we need to do the secure IP handling or not
			if (GSocketSubsystem->RequiresEncryptedPackets())
			{
				in_addr Addr;
				// Try to decode the secure address so we can connect to it
				if (XNetXnAddrToInAddr(&SessionInfo->XSessionInfo.hostAddress,
					&SessionInfo->XSessionInfo.sessionID,
					&Addr) == 0)
				{
					// Always use the secure layer in final release
					IpAddr.SetIp(Addr);
					bOk = TRUE;
				}
				else
				{
					debugf(NAME_DevOnline,
						TEXT("Failed to decrypt target host IP for session (%s)"),
						*SessionName.ToString());
				}
			}
			else
			{
				bOk = TRUE;
				// Don't use the encrypted/decrypted form of the IP when it's not required
				IpAddr.SetIp(SessionInfo->XSessionInfo.hostAddress.inaOnline);
			}
			// Copy the destination IP
			ConnectInfo = IpAddr.ToString(FALSE);
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Can't decrypt a NULL session's IP for session (%s)"),
				*SessionName.ToString());
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Unknown session name (%s) specified to GetResolvedConnectString()"),
			*SessionName.ToString());
	}
	return bOk;
}

/**
 * Registers a player with the online service as being part of the online game
 *
 * @param NewPlayer the player to register with the online service
 * @param bWasInvited whether to use private or public slots first
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineSubsystemLive::RegisterPlayer(FName SessionName,FUniqueNetId UniquePlayerId,UBOOL bWasInvited)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Find the session they are referring to
	FNamedSession* Session = GetNamedSession(SessionName);
	// Don't try to join a non-existant game
	if (Session &&
		Session->GameSettings &&
		Session->SessionInfo)
	{
		INT RegistrantIndex = INDEX_NONE;
		FOnlineRegistrant Registrant(UniquePlayerId);
		// See if this is a new player or not
		if (Session->Registrants.FindItem(Registrant,RegistrantIndex) == FALSE)
		{
			// Add the player as a registrant for this session
			Session->Registrants.AddItem(Registrant);
			// Determine if this player is really remote or not
			if (IsLocalPlayer(UniquePlayerId) == FALSE &&
				// Skip these if nobody is signed into live
				AreAnySignedIntoLive())
			{
				FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
				// Create a new async task for handling the async notification
				FLiveAsyncRegisterPlayer* AsyncTask = new FLiveAsyncRegisterPlayer(SessionName,
					(XUID&)UniquePlayerId,
					// Treat invite or a private match the same
					bWasInvited || Session->GameSettings->NumPublicConnections == 0,
					&RegisterPlayerCompleteDelegates);
				// Kick off the async join request
				Return = XSessionJoinRemote(SessionInfo->Handle,
					1,
					AsyncTask->GetXuids(),
					AsyncTask->GetPrivateInvites(),
					*AsyncTask);
				debugf(NAME_DevOnline,
					TEXT("XSessionJoinRemote(0x%016I64X) for '%s' returned 0x%08X"),
					(XUID&)UniquePlayerId,
					*SessionName.ToString(),
					Return);
				// Only queue the task up if the call worked
				if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
				{
					// Register this player for voice
					RegisterRemoteTalker(UniquePlayerId);
					// Add the async task to be ticked
					AsyncTasks.AddItem(AsyncTask);
				}
				else
				{
					delete AsyncTask;
				}
			}
			else
			{
				// This is a local player. In case their PRI came last during replication, reprocess muting
				ProcessMuteChangeNotification();
				Return = ERROR_SUCCESS;
			}
		}
		else
		{
			// Determine if this player is really remote or not
			if (IsLocalPlayer(UniquePlayerId) == FALSE)
			{
				// Re-register this player for voice in case they were removed from one session,
				// but are still in another
				RegisterRemoteTalker(UniquePlayerId);
			}
			else
			{
				// This is a local player. In case their PRI came last during replication, reprocess muting
				ProcessMuteChangeNotification();
			}
			debugf(NAME_DevOnline,
				TEXT("Skipping register since player is already registered at index (%d) for session (%s)"),
				RegistrantIndex,
				*SessionName.ToString());
			Return = ERROR_SUCCESS;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("No game present to join for session (%s)"),
			*SessionName.ToString());
	}
	if (Return != ERROR_IO_PENDING)
	{
		OnlineSubsystemLive_eventOnRegisterPlayerComplete_Parms Results(EC_EventParm);
		Results.SessionName = SessionName;
		Results.PlayerID = UniquePlayerId;
		Results.bWasSuccessful = Return == ERROR_SUCCESS ? FIRST_BITFIELD : 0;
		TriggerOnlineDelegates(this,RegisterPlayerCompleteDelegates,&Results);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Registers a group of players with the online service as being part of the online game
 *
 * @param SessionName the name of the session the player is joining
 * @param Players the list of players to register with the online service
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineSubsystemLive::RegisterPlayers(FName SessionName,const TArray<FUniqueNetId>& Players)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Find the session they are referring to
	FNamedSession* Session = GetNamedSession(SessionName);
	// Don't try to join a non-existant game
	if (Session &&
		Session->GameSettings &&
		Session->SessionInfo)
	{
		// Skip this if nobody is signed into live
		if (AreAnySignedIntoLive())
		{
			TArray<FUniqueNetId> FoundPlayers;
			// Go through the list of requested players and add them to our session
			for (INT Index = 0; Index < Players.Num(); Index++)
			{
				INT RegistrantIndex = INDEX_NONE;
				FOnlineRegistrant Registrant(Players(Index));
				// See if this is a new player or not
				if (Session->Registrants.FindItem(Registrant,RegistrantIndex) == FALSE)
				{
					// Now remove from Live if they are a remote player
					if (IsLocalPlayer(Players(Index)) == FALSE)
					{
						FoundPlayers.AddItem(Players(Index));
						// Remove this player from the voice list
						RegisterRemoteTalker(Players(Index));
					}
					else
					{
						// This is a local player. In case their PRI came last during replication, reprocess muting
						ProcessMuteChangeNotification();
					}
				}
				else
				{
					// Determine if this player is really remote or not
					if (IsLocalPlayer(Players(Index)) == FALSE)
					{
						// Re-register this player for voice in case they were removed from one session,
						// but are still in another
						RegisterRemoteTalker(Players(Index));
					}
					else
					{
						// This is a local player. In case their PRI came last during replication, reprocess muting
						ProcessMuteChangeNotification();
					}
					debugf(NAME_DevOnline,
						TEXT("Skipping register since player is already registered at index (%d) for session (%s)"),
						RegistrantIndex,
						*SessionName.ToString());
				}
			}
			// Don't do anything if they are all local or the list is empty
			if (FoundPlayers.Num())
			{
				FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
				// Create a new async task for handling the async notification
				FLiveAsyncRegisterPlayers* AsyncTask = new FLiveAsyncRegisterPlayers(SessionName,
					FoundPlayers,
					Session->GameSettings->NumPublicConnections == 0,
					&RegisterPlayerCompleteDelegates);
				// Kick off the async join request
				Return = XSessionJoinRemote(SessionInfo->Handle,
					FoundPlayers.Num(),
					AsyncTask->GetXuids(),
					AsyncTask->GetPrivateInvites(),
					*AsyncTask);
				debugf(NAME_DevOnline,
					TEXT("XSessionJoinRemote(%d) for '%s' returned 0x%08X"),
					FoundPlayers.Num(),
					*SessionName.ToString(),
					Return);
				// Only queue the task up if the call worked
				if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
				{
					// Add the async task to be ticked
					AsyncTasks.AddItem(AsyncTask);
				}
				else
				{
					delete AsyncTask;
				}
			}
			else
			{
				Return = ERROR_SUCCESS;
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("No game present to join for session (%s)"),
			*SessionName.ToString());
	}
	if (Return != ERROR_IO_PENDING)
	{
		OnlineSubsystemLive_eventOnRegisterPlayerComplete_Parms Results(EC_EventParm);
		Results.SessionName = SessionName;
		Results.bWasSuccessful = Return == ERROR_SUCCESS ? FIRST_BITFIELD : 0;
		// For each player send the register
		for (INT Index = 0; Index < Players.Num(); Index++)
		{
			Results.PlayerID = Players(Index);
			TriggerOnlineDelegates(this,RegisterPlayerCompleteDelegates,&Results);
		}
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Unregisters a player with the online service as being part of the online game
 *
 * @param SessionName the name of the session the player is leaving
 * @param UniquePlayerId the player to unregister with the online service
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineSubsystemLive::UnregisterPlayer(FName SessionName,FUniqueNetId UniquePlayerId)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Find the session they are referring to
	FNamedSession* Session = GetNamedSession(SessionName);
	// Don't try to leave a non-existant game
	if (Session &&
		Session->GameSettings &&
		Session->SessionInfo)
	{
		INT RegistrantIndex = INDEX_NONE;
		FOnlineRegistrant Registrant(UniquePlayerId);
		// See if this is a new player or not
		if (Session->Registrants.FindItem(Registrant,RegistrantIndex))
		{
			// Remove the player from the list
			Session->Registrants.Remove(RegistrantIndex);
			// Now remove from Live if they are a remote player
			if (IsLocalPlayer(UniquePlayerId) == FALSE &&
				// Skip these if nobody is signed into live
				AreAnySignedIntoLive())
			{
				FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
				// Create a new async task for handling the async notification
				FLiveAsyncUnregisterPlayer* AsyncTask = new FLiveAsyncUnregisterPlayer(SessionName,
					(XUID&)UniquePlayerId,
					&UnregisterPlayerCompleteDelegates);
				// Kick off the async leave request
				Return = XSessionLeaveRemote(SessionInfo->Handle,
					1,
					AsyncTask->GetXuids(),
					*AsyncTask);
				debugf(NAME_DevOnline,
					TEXT("XSessionLeaveRemote(0x%016I64X) '%s' returned 0x%08X"),
					(QWORD&)UniquePlayerId,
					*SessionName.ToString(),
					Return);
				// Only queue the task up if the call worked
				if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
				{
					// Remove this player from the voice list
					UnregisterRemoteTalker(UniquePlayerId);
					// Add the async task to be ticked
					AsyncTasks.AddItem(AsyncTask);
				}
				else
				{
					delete AsyncTask;
				}
			}
			else
			{
				Return = ERROR_SUCCESS;
			}
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Player 0x%016I64X is not part of session (%s)"),
				(QWORD&)UniquePlayerId,
				*SessionName.ToString());
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("No game present to leave for session (%s)"),
			*SessionName.ToString());
	}
	if (Return != ERROR_IO_PENDING)
	{
		OnlineSubsystemLive_eventOnUnregisterPlayerComplete_Parms Results(EC_EventParm);
		Results.SessionName = SessionName;
		Results.PlayerID = UniquePlayerId;
		Results.bWasSuccessful = Return == ERROR_SUCCESS ? FIRST_BITFIELD : 0;
		TriggerOnlineDelegates(this,UnregisterPlayerCompleteDelegates,&Results);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Unregisters a group of players with the online service as being part of the online game
 *
 * @param SessionName the name of the session the player is joining
 * @param Players the list of players to unregister with the online service
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineSubsystemLive::UnregisterPlayers(FName SessionName,const TArray<FUniqueNetId>& Players)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Find the session they are referring to
	FNamedSession* Session = GetNamedSession(SessionName);
	// Don't try to leave a non-existant game
	if (Session &&
		Session->GameSettings &&
		Session->SessionInfo)
	{
		// Skip this if nobody is signed into live
		if (AreAnySignedIntoLive())
		{
			TArray<FUniqueNetId> FoundPlayers;
			// Go through the list of requested players and remove them from our session
			for (INT Index = 0; Index < Players.Num(); Index++)
			{
				INT RegistrantIndex = INDEX_NONE;
				FOnlineRegistrant Registrant(Players(Index));
				// See if this is a new player or not
				if (Session->Registrants.FindItem(Registrant,RegistrantIndex))
				{
					// Remove the player from the list
					Session->Registrants.Remove(RegistrantIndex);
					// Now remove from Live if they are a remote player
					if (IsLocalPlayer(Players(Index)) == FALSE)
					{
						FoundPlayers.AddItem(Players(Index));
						// Remove this player from the voice list
						UnregisterRemoteTalker(Players(Index));
					}
				}
			}
			// Don't do anything if they are all local or the list is empty
			if (FoundPlayers.Num())
			{
				FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
				// Create a new async task for handling the async notification
				FLiveAsyncUnregisterPlayers* AsyncTask = new FLiveAsyncUnregisterPlayers(SessionName,
					FoundPlayers,
					&UnregisterPlayerCompleteDelegates);
				// Kick off the async leave request
				Return = XSessionLeaveRemote(SessionInfo->Handle,
					FoundPlayers.Num(),
					AsyncTask->GetXuids(),
					*AsyncTask);
				debugf(NAME_DevOnline,
					TEXT("XSessionLeaveRemote(%d) '%s' returned 0x%08X"),
					FoundPlayers.Num(),
					*SessionName.ToString(),
					Return);
				// Only queue the task up if the call worked
				if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
				{
					// Add the async task to be ticked
					AsyncTasks.AddItem(AsyncTask);
				}
				else
				{
					delete AsyncTask;
				}
			}
			else
			{
				Return = ERROR_SUCCESS;
			}
		}
		else
		{
			Return = ERROR_SUCCESS;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("No game present to leave for session (%s)"),
			*SessionName.ToString());
	}
	if (Return != ERROR_IO_PENDING)
	{
		OnlineSubsystemLive_eventOnUnregisterPlayerComplete_Parms Results(EC_EventParm);
		Results.SessionName = SessionName;
		Results.bWasSuccessful = Return == ERROR_SUCCESS ? FIRST_BITFIELD : 0;
		// For each player send the unregister
		for (INT Index = 0; Index < Players.Num(); Index++)
		{
			Results.PlayerID = Players(Index);
			TriggerOnlineDelegates(this,UnregisterPlayerCompleteDelegates,&Results);
		}
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Updates the current session's skill rating using the list of players' skills
 *
 * @param SessionName the name of the session that is being updated
 * @param Players the set of players to use in the skill calculation
 *
 * @return true if the update succeeded, false otherwise
 */
UBOOL UOnlineSubsystemLive::RecalculateSkillRating(FName SessionName,const TArray<FUniqueNetId>& Players)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	if (Players.Num())
	{
		FNamedSession* Session = GetNamedSession(SessionName);
		// Skip for LAN
		if (Session &&
			Session->GameSettings &&
			Session->GameSettings->bIsLanMatch == FALSE &&
			// Don't try to modify if you aren't the server
			IsServer() &&
			// Skip this if they are signed out
			AreAnySignedIntoLive())
		{
			// Skip if the game isn't pending or in progress
			if (Session->GameSettings->GameState < OGS_Ending)
			{
				// Skip if an update is outstanding
				if (Session->GameSettings->bHasSkillUpdateInProgress == FALSE)
				{
					FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
					FLiveAsyncUpdateSessionSkill* AsyncTask = new FLiveAsyncUpdateSessionSkill(SessionName,&RecalculateSkillRatingCompleteDelegates,Players);
					// Tell Live to update the skill for the server with the following people
					Return = XSessionModifySkill(SessionInfo->Handle,
						AsyncTask->GetCount(),
						AsyncTask->GetXuids(),
						*AsyncTask);
					debugfLiveSlow(NAME_DevOnline,TEXT("XSessionModifySkill() '%s' returned 0x%08X"),
						*SessionName.ToString(),
						Return);
					if (Return == ERROR_IO_PENDING)
					{
						// Add the async task to be ticked
						AsyncTasks.AddItem(AsyncTask);
						// Indicate the async task is running
						Session->GameSettings->bHasSkillUpdateInProgress = TRUE;
					}
					else
					{
						delete AsyncTask;
			
						FAsyncTaskDelegateResultsNamedSession Results(Session->SessionName,Return);
						TriggerOnlineDelegates(this,RecalculateSkillRatingCompleteDelegates,&Results);
					}
				}
				else
				{
					debugf(NAME_DevOnline,
						TEXT("A skill update is already being processed for (%s), ignoring request"),
						*SessionName.ToString());
					Return = ERROR_SUCCESS;
				}
			}
			else
			{
				debugf(NAME_DevOnline,
					TEXT("Skipping a skill update for a session (%s) that is ending/ed"),
					*SessionName.ToString());
				Return = ERROR_SUCCESS;
			}
		}
		else
		{
			Return = ERROR_SUCCESS;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Can't update skill for a session (%s) with no players"),
			*SessionName.ToString());
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Migrates an existing online game on the host.
 * NOTE: online game migration is an async process and does not complete
 * until the OnMigrateOnlineGameComplete delegate is called.
 *
 * @param HostingPlayerNum the index of the player now hosting the match
 * @param SessionName the name of the existing session to migrate
 *
 * @return true if successful migrating the session, false otherwise
 */
UBOOL UOnlineSubsystemLive::MigrateOnlineGame(BYTE HostingPlayerNum,FName SessionName)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	FNamedSession* Session = GetNamedSession(SessionName);

	// Make sure session to migrate exists
	if (Session != NULL)
	{
		if (Session->GameSettings->bIsLanMatch)
		{
			// Cleanup an existing lan beacon
			StopLanBeacon();
		}
		else
		{
			// Stop servicing QoS requests on old session so it can be started on the migrated one
			UnregisterQoS(Session);
		}	
		// Migration only supported for clients so new session will be a player
		Session->GameSettings->bIsDedicated = FALSE;
		// Treated as host now
		Session->GameSettings->bWasFromInvite = FALSE;
		// Init the game settings counts so the host can use them later
		Session->GameSettings->NumOpenPrivateConnections = Session->GameSettings->NumPrivateConnections;
		Session->GameSettings->NumOpenPublicConnections = Session->GameSettings->NumPublicConnections;
		// Read the XUID of the owning player for gamertag and gamercard support
		GetUserXuid(HostingPlayerNum,(XUID*)&Session->GameSettings->OwningPlayerId);
		// Read the name of the owning player
		Session->GameSettings->OwningPlayerName = GetPlayerNickname(HostingPlayerNum);
		// Register migration via Live
		Return = MigrateLiveGame(HostingPlayerNum,Session,TRUE);
		// If we were unable to migrate the game, clean up
		if (Return != ERROR_IO_PENDING && Return != ERROR_SUCCESS && Session)
		{
			// Clean up the session info so we don't get into a confused state
			RemoveNamedSession(SessionName);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Cannot migrate session '%s': session doesn't exist."), *SessionName.ToString());
	}
	if (Return != ERROR_IO_PENDING)
	{
		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResultsNamedSession Results(SessionName,Return);
		TriggerOnlineDelegates(this,MigrateOnlineGameCompleteDelegates,&Results);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Joins the migrated game specified
 *
 * @param PlayerNum the index of the player about to join a match
 * @param SessionName the name of the migrated session to join
 * @param DesiredGame the desired migrated game to join
 *
 * @return true if the call completed successfully, false otherwise
 */
UBOOL UOnlineSubsystemLive::JoinMigratedOnlineGame(BYTE PlayerNum,FName SessionName,const struct FOnlineGameSearchResult& DesiredGame)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	FNamedSession* Session = GetNamedSession(SessionName);
	// Make sure session to migrate exists
	if (Session != NULL)
	{
		// Set it's game settings
		Session->GameSettings = DesiredGame.GameSettings;
		// Copy the session info over, but maintain original session handle
		FSecureSessionInfo* SessionInfo = (FSecureSessionInfo*)Session->SessionInfo;		
		appMemcpy(&SessionInfo->XSessionInfo,DesiredGame.PlatformData,sizeof(XSESSION_INFO));
		// The session nonce needs to come from the game settings when joining
		SessionInfo->Nonce = Session->GameSettings->ServerNonce;
		// Register migration join via Live
		Return = MigrateLiveGame(PlayerNum,Session,FALSE);
		// If we were unable to migrate the game, clean up
		if (Return != ERROR_IO_PENDING && Return != ERROR_SUCCESS && Session)
		{
			// Clean up the session info so we don't get into a confused state
			RemoveNamedSession(SessionName);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Cannot migrate session '%s': session doesn't exist."), *SessionName.ToString());
	}
	if (Return != ERROR_IO_PENDING)
	{
		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResultsNamedSession Results(SessionName,Return);
		TriggerOnlineDelegates(this,JoinMigratedOnlineGameCompleteDelegates,&Results);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Migrates an existing Live enabled game for the requesting player using the
 * existing settings specified in the game settings object
 *
 * @param PlayerNum the new player hosting or joining the game
 * @param Session the named session for this online game
 * @param bIsHost whether migration is occurring on the new host or client joining
 *
 * @return The result from the Live APIs
 */
DWORD UOnlineSubsystemLive::MigrateLiveGame(BYTE PlayerNum,FNamedSession* Session,UBOOL bIsHost)
{
	check(Session && Session->GameSettings && Session->SessionInfo);
	UOnlineGameSettings* GameSettings = Session->GameSettings;
	FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
	debugf(NAME_DevOnline,TEXT("Migrating %s session"),*Session->SessionName.ToString());
#if DEBUG_CONTEXT_LOGGING
	// Log game settings
	DumpGameSettings(GameSettings);
	// Log properties and contexts
	DumpContextsAndProperties(GameSettings);
#endif
	// For each local player, force them to use the same props/contexts
	for (DWORD Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
	{
		// Ignore non-Live enabled profiles
		if (XUserGetSigninState(Index) != eXUserSigninState_NotSignedIn)
		{
			// Register all of the context/property information for the session
			SetContextsAndProperties(Index,GameSettings);
		}
	}
	// Create a new async task for handling the migration
	FOnlineAsyncTaskLive* AsyncTask = new FLiveAsyncTaskMigrateSession(Session->SessionName,PlayerNum,bIsHost);
	// Initiate the session migration on Live 
	DWORD Return = XSessionMigrateHost(
		SessionInfo->Handle,
		bIsHost ? PlayerNum : XUSER_INDEX_NONE,
		&SessionInfo->XSessionInfo,
		*AsyncTask
		);	
	debugf(NAME_DevOnline,TEXT("XSessionMigrateHost(%s) '%s' (%d,%d,%d,Nonce,SessInfo,Data,OutHandle) returned 0x%08X"),
		bIsHost ? TEXT("hosting") : TEXT("joining"),
		*Session->SessionName.ToString(),(DWORD)PlayerNum,GameSettings->NumPublicConnections,GameSettings->NumPrivateConnections,Return);
	if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
	{
		// Add the task for tracking since the call worked
		AsyncTasks.AddItem(AsyncTask);
	}
	else
	{
		// Don't leak the task in this case
		delete AsyncTask;
	}
	return Return;
}

/**
 * Finishes migrating the online game
 *
 * @param PlayerNum the local player index migrating the session
 * @param SessionName the name of the session that is being migrated
 * @param MigrateResult the result code from the async create operation
 * @param bIsHost TRUE if migrated session is for new host, FALSE if client
 */
void UOnlineSubsystemLive::FinishMigrateOnlineGame(DWORD PlayerNum,FName SessionName,DWORD MigrateResult,UBOOL bIsHost)
{
	// If the task completed ok, then continue the session migrate process
	if (MigrateResult == ERROR_SUCCESS)
	{
		// Get the session from the name
		FNamedSession* Session = GetNamedSession(SessionName);
		if (Session != NULL && Session->GameSettings != NULL && Session->SessionInfo != NULL)
		{
			FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
			if (bIsHost)
			{
				// Copy nonce from the original session
				SessionInfo->Nonce = Session->GameSettings->ServerNonce;
				// If switching to new host then keep track of host flag
				SessionInfo->Flags |= XSESSION_CREATE_HOST;
				// Register the host with QoS
				RegisterQoS(Session);
				// Determine if we are registering a Live session or system link
				if (Session->GameSettings->bIsLanMatch)
				{
					// Initialize the lan game's lan beacon for queries
					MigrateResult = CreateLanGame(PlayerNum,Session);
				}
			}			
			// Set the game state as pending (not started)
			Session->GameSettings->GameState = OGS_Pending;
			// Register all local folks as participants/talkers
			RegisterLocalPlayers(Session,Session->GameSettings->bWasFromInvite);
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Session (%s) was missing to complete migration, failing"),
				*SessionName.ToString());

			RemoveNamedSession(SessionName);
			MigrateResult = E_FAIL;
		}
	}	
	else
	{		
		// Clean up partial create
		RemoveNamedSession(SessionName);
	}
	// Just trigger the delegate with the error/success code
	FAsyncTaskDelegateResultsNamedSession Params(SessionName,MigrateResult);
	TriggerOnlineDelegates(this,bIsHost ? MigrateOnlineGameCompleteDelegates : JoinMigratedOnlineGameCompleteDelegates,&Params);
}

/**
 * Reads the online profile settings for a given user from Live using an async task.
 * First, the game settings are read. If there is data in those, then the settings
 * are pulled from there. If not, the settings come from Live, followed by the
 * class defaults.
 *
 * @param LocalUserNum the user that we are reading the data for
 * @param ProfileSettings the object to copy the results to and contains the list of items to read
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineSubsystemLive::ReadProfileSettings(BYTE LocalUserNum,
	UOnlineProfileSettings* ProfileSettings)
{
 	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Only read if we don't have a profile for this player
		if (ProfileCache[LocalUserNum].Profile == NULL)
		{
			if (ProfileSettings != NULL)
			{
				ProfileCache[LocalUserNum].Profile = ProfileSettings;
				ProfileSettings->AsyncState = OPAS_Read;
				// Clear the previous set of results
				ProfileSettings->ProfileSettings.Empty();
				// Make sure the version number is requested
				ProfileSettings->AppendVersionToReadIds();
				// If they are not logged in, give them all the defaults
				XUSER_SIGNIN_INFO SigninOnline;
				// Skip the write if the user isn't signed in
				if (XUserGetSigninInfo(LocalUserNum,XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY,&SigninOnline) == ERROR_SUCCESS &&
					// Treat guests as getting the defaults
					IsGuestLogin(LocalUserNum) == FALSE)
				{
					DWORD NumIds = ProfileSettings->ProfileSettingIds.Num();
					DWORD* ProfileIds = (DWORD*)ProfileSettings->ProfileSettingIds.GetData();
					// Create the read buffer
					FLiveAsyncTaskDataReadProfileSettings* ReadData = new FLiveAsyncTaskDataReadProfileSettings(LocalUserNum,NumIds);
					// Copy the IDs for later use when inspecting the game settings blobs
					appMemcpy(ReadData->GetIds(),ProfileIds,sizeof(DWORD) * NumIds);
					// Create a new async task for handling the async notification
					FOnlineAsyncTaskLive* AsyncTask = new FLiveAsyncTaskReadProfileSettings(
						&ProfileCache[LocalUserNum].ReadDelegates,
						ReadData,
						SigninOnline.xuid);
					// Tell Live the size of our buffer
					DWORD SizeNeeded = ReadData->GetGameSettingsSize();
					// Start by reading the game settings fields
					Return = XUserReadProfileSettings(0,
						LocalUserNum,
						ReadData->GetGameSettingsIdsCount(),
						ReadData->GetGameSettingsIds(),
						&SizeNeeded,
						ReadData->GetGameSettingsBuffer(),
						*AsyncTask);
					if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
					{
						// Queue the async task for ticking
						AsyncTasks.AddItem(AsyncTask);
					}
					else
					{
						// Just trigger the delegate as having failed
						OnlineSubsystemLive_eventOnReadProfileSettingsComplete_Parms Results(EC_EventParm);
						Results.LocalUserNum = LocalUserNum;
						Results.bWasSuccessful = FALSE;
						TriggerOnlineDelegates(this,ProfileCache[LocalUserNum].ReadDelegates,&Results);
						delete AsyncTask;
						ProfileCache[LocalUserNum].Profile = NULL;
					}
					debugfLiveSlow(NAME_DevOnline,TEXT("XUserReadProfileSettings(0,%d,3,GameSettingsIds,%d,data,data) returned 0x%08X"),
						LocalUserNum,SizeNeeded,Return);
				}
				else
				{
					debugfLiveSlow(NAME_DevOnline,
						TEXT("User (%d) not logged in or is a guest, using defaults"),
						(DWORD)LocalUserNum);
					// Use the defaults for this player
					ProfileSettings->eventSetToDefaults();
					ProfileSettings->AsyncState = OPAS_Finished;
					// Just trigger the delegate as having succeeded
					OnlineSubsystemLive_eventOnReadProfileSettingsComplete_Parms Results(EC_EventParm);
					Results.LocalUserNum = LocalUserNum;
					Results.bWasSuccessful = FIRST_BITFIELD;
					TriggerOnlineDelegates(this,ProfileCache[LocalUserNum].ReadDelegates,&Results);
				}
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Can't specify a null profile settings object"));
			}
		}
		// Make sure the profile isn't already being read, since this is going to
		// complete immediately
		else if (ProfileCache[LocalUserNum].Profile->AsyncState != OPAS_Read)
		{
			debugfLiveSlow(NAME_DevOnline,TEXT("Using cached profile data instead of reading"));
			// If the specified read isn't the same as the cached object, copy the
			// data from the cache
			if (ProfileCache[LocalUserNum].Profile != ProfileSettings)
			{
				ProfileSettings->ProfileSettings = ProfileCache[LocalUserNum].Profile->ProfileSettings;
				ProfileCache[LocalUserNum].Profile = ProfileSettings;
			}
			// Just trigger the read delegate as being done
			// Send the notification of completion
			OnlineSubsystemLive_eventOnReadProfileSettingsComplete_Parms Results(EC_EventParm);
			Results.LocalUserNum = LocalUserNum;
			Results.bWasSuccessful = FIRST_BITFIELD;
			TriggerOnlineDelegates(this,ProfileCache[LocalUserNum].ReadDelegates,&Results);
			Return = ERROR_SUCCESS;
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Profile read for player (%d) is already in progress"),LocalUserNum);
			// Just trigger the read delegate as failed
			OnlineSubsystemLive_eventOnReadProfileSettingsComplete_Parms Results(EC_EventParm);
			Results.LocalUserNum = LocalUserNum;
			Results.bWasSuccessful = FALSE;
			TriggerOnlineDelegates(this,ProfileCache[LocalUserNum].ReadDelegates,&Results);
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid player specified to ReadProfileSettings(%d)"),
			LocalUserNum);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Parses the read profile results into something the game play code can handle
 *
 * @param PlayerNum the number of the user being processed
 * @param ReadResults the buffer filled by Live
 */
void UOnlineSubsystemLive::ParseReadProfileResults(BYTE PlayerNum,PXUSER_READ_PROFILE_SETTING_RESULT ReadResults)
{
	check(PlayerNum >=0 && PlayerNum < MAX_LOCAL_PLAYERS);
	UOnlineProfileSettings* ProfileRead = ProfileCache[PlayerNum].Profile;
	if (ProfileRead != NULL)
	{
		// Make sure the profile settings have a version number
		ProfileRead->SetDefaultVersionNumber();
		check(ReadResults != NULL);
		// Loop through the results copying the info over
		for (DWORD Index = 0; Index < ReadResults->dwSettingsLen; Index++)
		{
			XUSER_PROFILE_SETTING& LiveSetting = ReadResults->pSettings[Index];
			// Convert to our property id
			INT PropId = ConvertFromLiveValue(LiveSetting.dwSettingId);
			INT UpdateIndex = INDEX_NONE;
			// Search the settings for the property so we can replace if needed
			for (INT FindIndex = 0; FindIndex < ProfileRead->ProfileSettings.Num(); FindIndex++)
			{
				if (ProfileRead->ProfileSettings(FindIndex).ProfileSetting.PropertyId == PropId)
				{
					UpdateIndex = FindIndex;
					break;
				}
			}
			// Add if not already in the settings
			if (UpdateIndex == INDEX_NONE)
			{
				UpdateIndex = ProfileRead->ProfileSettings.AddZeroed();
			}
			// Now update the setting
			FOnlineProfileSetting& Setting = ProfileRead->ProfileSettings(UpdateIndex);
			// Copy the source and id is set to game since you can't write to Live ones
			Setting.Owner = OPPO_Game;
			Setting.ProfileSetting.PropertyId = PropId;
			// Don't bother copying data for no value settings
			if (LiveSetting.source != XSOURCE_NO_VALUE)
			{
				// Use the helper to copy the data over
				CopyXDataToSettingsData(Setting.ProfileSetting.Data,LiveSetting.data);
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Skipping read profile results parsing due to no read object"));
	}
}

/**
 * Copies Unreal data to Live structures for the Live property writes
 *
 * @param Profile the profile object to copy the data from
 * @param LiveData the Live data structures to copy the data to
 */
void UOnlineSubsystemLive::CopyLiveProfileSettings(UOnlineProfileSettings* Profile,
	PXUSER_PROFILE_SETTING LiveData)
{
	check(Profile && LiveData);
	// Make a copy of the data for each setting
	for (INT Index = 0; Index < Profile->ProfileSettings.Num(); Index++)
	{
		FOnlineProfileSetting& Setting = Profile->ProfileSettings(Index);
		// Copy the data
		LiveData[Index].dwSettingId = ConvertToLiveValue((EProfileSettingID)Setting.ProfileSetting.PropertyId);
		LiveData[Index].source = (XUSER_PROFILE_SOURCE)Setting.Owner;
		// Shallow copy requires the data to live throughout the duration of the call
		LiveData[Index].data.type = Setting.ProfileSetting.Data.Type;
		LiveData[Index].data.binary.cbData = Setting.ProfileSetting.Data.Value1;
		LiveData[Index].data.binary.pbData = (BYTE*)Setting.ProfileSetting.Data.Value2;
	}
}

/**
 * Determines whether the specified settings should come from the game
 * default settings. If so, the defaults are copied into the players
 * profile results and removed from the settings list
 *
 * @param PlayerNum the id of the player
 * @param SettingsIds the set of ids to filter against the game defaults
 */
void UOnlineSubsystemLive::ProcessProfileDefaults(BYTE PlayerNum,TArray<DWORD>& SettingsIds)
{
	check(PlayerNum >=0 && PlayerNum < MAX_LOCAL_PLAYERS);
	check(ProfileCache[PlayerNum].Profile);
	// Copy the current settings so that setting the defaults doesn't clobber them
	TArray<FOnlineProfileSetting> Copy = ProfileCache[PlayerNum].Profile->ProfileSettings; 
	// Tell the profile to replace it's defaults
	ProfileCache[PlayerNum].Profile->eventSetToDefaults();
	TArray<FOnlineProfileSetting>& Settings = ProfileCache[PlayerNum].Profile->ProfileSettings;
	// Now reapply the copied settings
	for (INT Index = 0; Index < Copy.Num(); Index++)
	{
		UBOOL bFound = FALSE;
		const FOnlineProfileSetting& CopiedSetting = Copy(Index);
		// Search the profile settings and replace the setting with copied one
		for (INT Index2 = 0; Index2 < Settings.Num(); Index2++)
		{
			if (Settings(Index2).ProfileSetting.PropertyId == CopiedSetting.ProfileSetting.PropertyId)
			{
				Settings(Index2) = CopiedSetting;
				bFound = TRUE;
				break;
			}
		}
		// Add if it wasn't in the defaults
		if (bFound == FALSE)
		{
			Settings.AddItem(CopiedSetting);
		}
	}
	// Now remove the IDs that the defaults set from the missing list
	for (INT Index = 0; Index < Settings.Num(); Index++)
	{
		INT FoundIdIndex = INDEX_NONE;
		// Search and remove if found because it isn't missing then
		if (SettingsIds.FindItem(Settings(Index).ProfileSetting.PropertyId,FoundIdIndex) &&
			FoundIdIndex != INDEX_NONE &&
			Settings(Index).Owner != OPPO_OnlineService)
		{
			SettingsIds.Remove(FoundIdIndex);
		}
	}
}

/**
 * Adds one setting to the users profile results
 *
 * @param Profile the profile object to copy the data from
 * @param LiveData the Live data structures to copy the data to
 */
void UOnlineSubsystemLive::AppendProfileSetting(BYTE PlayerNum,const FOnlineProfileSetting& Setting)
{
	check(PlayerNum >=0 && PlayerNum < MAX_LOCAL_PLAYERS);
	check(ProfileCache[PlayerNum].Profile);
	INT AddIndex = ProfileCache[PlayerNum].Profile->ProfileSettings.AddZeroed();
	// Deep copy the data
	ProfileCache[PlayerNum].Profile->ProfileSettings(AddIndex) = Setting;
}

/**
 * Writes the online profile settings for a given user Live using an async task
 *
 * @param LocalUserNum the user that we are writing the data for
 * @param ProfileSettings the list of settings to write out
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineSubsystemLive::WriteProfileSettings(BYTE LocalUserNum,
	UOnlineProfileSettings* ProfileSettings)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Don't allow a write if there is a task already in progress
		if (ProfileCache[LocalUserNum].Profile == NULL ||
			(ProfileCache[LocalUserNum].Profile->AsyncState != OPAS_Read &&	ProfileCache[LocalUserNum].Profile->AsyncState != OPAS_Write))
		{
			if (ProfileSettings != NULL)
			{
				// Mark this as a write in progress
				ProfileSettings->AsyncState = OPAS_Write;
				// Make sure the profile settings have a version number
				ProfileSettings->AppendVersionToSettings();
				// Cache to make sure GC doesn't collect this while we are waiting
				// for the Live task to complete
				ProfileCache[LocalUserNum].Profile = ProfileSettings;
				XUSER_SIGNIN_INFO SigninOnline;
				// Skip the write if the user isn't signed in
				if (XUserGetSigninInfo(LocalUserNum,XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY,&SigninOnline) == ERROR_SUCCESS)
				{
					// Used to write the profile settings into a blob
					FProfileSettingsWriterLive Writer(MAX_PROFILE_DATA_SIZE,SigninOnline.xuid);
					if (Writer.SerializeToBuffer(ProfileSettings->ProfileSettings))
					{
						// Create the write buffer
						FLiveAsyncTaskDataWriteProfileSettings* WriteData =
							new FLiveAsyncTaskDataWriteProfileSettings(LocalUserNum,Writer.GetFinalBuffer(),Writer.GetFinalBufferLength());
						// Create a new async task to hold the data during the lifetime of
						// the call. It will be freed once the call is complete.
						FLiveAsyncTaskWriteProfileSettings* AsyncTask = new FLiveAsyncTaskWriteProfileSettings(
							&ProfileCache[LocalUserNum].WriteDelegates,WriteData);
						// Call a second time to fill in the data
						Return = XUserWriteProfileSettings(LocalUserNum,
							3,
							*WriteData,
							*AsyncTask);
						debugfLiveSlow(NAME_DevOnline,TEXT("XUserWriteProfileSettings(%d,3,data,data) returned 0x%08X"),
							LocalUserNum,Return);
						if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
						{
							// Queue the async task for ticking
							AsyncTasks.AddItem(AsyncTask);
						}
						else
						{
							// Remove the write state so that subsequent writes work
							ProfileCache[LocalUserNum].Profile->AsyncState = OPAS_Finished;
							// Send the notification of error completion
							OnlineSubsystemLive_eventOnWriteProfileSettingsComplete_Parms Results(EC_EventParm);
							Results.LocalUserNum = LocalUserNum;
							Results.bWasSuccessful = FALSE;
							TriggerOnlineDelegates(this,ProfileCache[LocalUserNum].WriteDelegates,&Results);
							delete AsyncTask;
						}
					}
					else
					{
						// Remove the write state so that subsequent writes work
						ProfileCache[LocalUserNum].Profile->AsyncState = OPAS_Finished;
						debugf(NAME_DevOnline,TEXT("Failed to compress buffer for profile settings. Write aborted"));
					}
				}
				else
				{
					Return = ERROR_SUCCESS;
					// Remove the write state so that subsequent writes work
					ProfileCache[LocalUserNum].Profile->AsyncState = OPAS_Finished;
					// Send the notification of completion
					OnlineSubsystemLive_eventOnWriteProfileSettingsComplete_Parms Results(EC_EventParm);
					Results.LocalUserNum = LocalUserNum;
					Results.bWasSuccessful = FIRST_BITFIELD;
					TriggerOnlineDelegates(this,ProfileCache[LocalUserNum].WriteDelegates,&Results);
					debugfLiveSlow(NAME_DevOnline,TEXT("Skipping profile write for non-signed in user. Caching in profile cache"));
				}
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Can't write a null profile settings object"));
			}
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Can't write profile as an async profile task is already in progress for player (%d)"),
				LocalUserNum);
		}
		if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
		{
			// Send the notification of error completion
			OnlineSubsystemLive_eventOnWriteProfileSettingsComplete_Parms Results(EC_EventParm);
			Results.LocalUserNum = LocalUserNum;
			Results.bWasSuccessful = FALSE;
			TriggerOnlineDelegates(this,ProfileCache[LocalUserNum].WriteDelegates,&Results);
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid player specified to WriteProfileSettings(%d)"),
			LocalUserNum);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Constructs sub-tasks for local/online reads.
 *
 * @param InUserIndex the user the read is being performed for
 * @param InUserLoginStatus signin status of the user's profile
 * @param InOnlineNetId xuid of player signed into Live
 * @param InScriptDelegates the delegate to fire off when complete
 * @param InPlayerStorage storage object to process from the reads
 * @param InDeviceID validated device ID for local read or -1 if invalid
 */
FLiveAsyncTaskParentReadPlayerStorage::FLiveAsyncTaskParentReadPlayerStorage(
		BYTE InUserIndex,
		ELoginStatus InUserLoginStatus,
		const FUniqueNetId& InOnlineNetId,
		TArray<FScriptDelegate>* InScriptDelegates,
		UOnlinePlayerStorage* InPlayerStorage,
		INT InDeviceID)
	:	FOnlineAsyncTaskLive(InScriptDelegates,NULL,TEXT("Read Player Storage"))
	,	UserIndex(InUserIndex)
	,	UserLoginStatus(InUserLoginStatus)
	,	OnlineNetId(InOnlineNetId)
	,	PlayerStorage(InPlayerStorage)
	,	AsyncTaskReadOnline(NULL)
	,	AsyncTaskReadLocal(NULL)
	,	ReadVersionNum(-2)
{
	check(PlayerStorage != NULL);
	
	// If signed in to a Live profile then write to online storage
	if (UserLoginStatus == LS_LoggedIn)
	{
		// Keep track of buffer data and state needed to write to online storage
		FLiveAsyncTaskDataReadOnlinePlayerStorage* ReadDataOnline = 
			new FLiveAsyncTaskDataReadOnlinePlayerStorage(UserIndex);

		// Create sub-task for writing to online storage. Delegates and PlayerStorage write state handled by parent task
		AsyncTaskReadOnline = new FLiveAsyncTaskReadOnlinePlayerStorage(NULL,ReadDataOnline);
		if (!AsyncTaskReadOnline->IsValid())
		{
			delete AsyncTaskReadOnline;
			AsyncTaskReadOnline = NULL;
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Player(%d): Online player storage read started."),ReadDataOnline->GetUserIndex());
		}
	}			
	// If signed in to a profile (Live or not) write to local storage 
	if (UserLoginStatus > LS_NotLoggedIn && 
		InDeviceID != -1)
	{
		// Generate a unique content name for each user that is saving
		const FString& SaveDrive = FString::Printf(TEXT("savedrive%d"),UserIndex);
		// Keep track of buffer data and state needed to write to local storage
		FLiveAsyncTaskDataReadLocalPlayerSave* ReadDataLocal = new FLiveAsyncTaskDataReadLocalPlayerSave(
			UserIndex,
			InDeviceID,
			*SaveDrive,
			TEXT("PlayerStorage.dat"),
			TEXT("Player Data"));

		// Create sub-task for writing to local storage. Delegates and PlayerStorage write state handled by parent task 	
		AsyncTaskReadLocal = new FLiveAsyncTaskReadLocalPlayerSave(NULL,ReadDataLocal);
		if (!AsyncTaskReadLocal->IsValid())
		{
			delete AsyncTaskReadLocal;
			AsyncTaskReadLocal = NULL;
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Player(%d): Local player storage read started."),ReadDataLocal->GetUserIndex());
		}
	}
}

/**
 * Frees the sub-tasks that were allocated
 */
FLiveAsyncTaskParentReadPlayerStorage::~FLiveAsyncTaskParentReadPlayerStorage()
{
	delete TaskData;
	delete AsyncTaskReadOnline;
	delete AsyncTaskReadLocal;
}

/**
 * Updates the amount of elapsed time this task has taken
 *
 * @param DeltaTime the amount of time that has passed since the last update
 */
void FLiveAsyncTaskParentReadPlayerStorage::UpdateElapsedTime(FLOAT DeltaTime)
{
	FOnlineAsyncTask::UpdateElapsedTime(DeltaTime);
	// Tick sub-tasks
	if (AsyncTaskReadOnline != NULL)
	{
		AsyncTaskReadOnline->UpdateElapsedTime(DeltaTime);
	}
	if (AsyncTaskReadLocal != NULL)
	{
		AsyncTaskReadLocal->UpdateElapsedTime(DeltaTime);
	}
}

/**
 * Checks the completion status of the task. Based on completion of sub-tasks as well.
 *
 * @return TRUE if done, FALSE otherwise
 */
UBOOL FLiveAsyncTaskParentReadPlayerStorage::HasTaskCompleted(void) const
{
	// Determine if each task has completed. If a task wasn't created treat as completed
	const UBOOL bCompleteReadOnline = AsyncTaskReadOnline != NULL ? AsyncTaskReadOnline->HasTaskCompleted() : TRUE;
	const UBOOL bCompleteReadLocal = AsyncTaskReadLocal != NULL ? AsyncTaskReadLocal->HasTaskCompleted() : TRUE;
	// Parent task is not finished until both sub-tasks are done
	return FOnlineAsyncTaskLive::HasTaskCompleted()	&& bCompleteReadOnline && bCompleteReadLocal;
}

/**
 * Determine if sub-tasks for reading player storage have finished. 
 * Finalizes the processing for the player storage object that was read.
 *
 * @param LiveSubsystem the object to make the final call on
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskParentReadPlayerStorage::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	// Process next completion async state for each sub-task
	const UBOOL bFinishedReadOnline = AsyncTaskReadOnline != NULL ? AsyncTaskReadOnline->ProcessAsyncResults(LiveSubsystem) : TRUE;
	const UBOOL bFinishedReadLocal = AsyncTaskReadLocal != NULL ? AsyncTaskReadLocal->ProcessAsyncResults(LiveSubsystem) : TRUE;

	// If both tasks are done then parent task is done as well and can be removed
	if (bFinishedReadOnline && bFinishedReadLocal)
	{
		// In case this gets cleared while in progress
		// Make sure cached storage is also valid since it could have been cleared during sign in change and GC'd
		if (PlayerStorage != NULL &&
			LiveSubsystem != NULL &&
			LiveSubsystem->PlayerStorageCacheLocal[UserIndex].PlayerStorage != NULL)
		{
			// Done with the writing, so mark the async state
			PlayerStorage->AsyncState = OPAS_Finished;

			// Copy from online read if successful
			if (AsyncTaskReadOnline	!= NULL && AsyncTaskReadOnline->IsSuccessful())
			{
				FLiveAsyncTaskDataReadOnlinePlayerStorage* TaskReadData = (FLiveAsyncTaskDataReadOnlinePlayerStorage*)AsyncTaskReadOnline->TaskData;
				check(TaskReadData != NULL);
				// Parse the settings data and check hash for validity
				FProfileSettingsReaderLive Reader(MAX_PERSISTENT_DATA_SIZE,TaskReadData->GetBuffer(),TaskReadData->GetBufferRead(),OnlineNetId.Uid);
				if (!Reader.SerializeFromBuffer(PlayerStorage->ProfileSettings))
				{
					debugf(NAME_DevOnline,
						TEXT("Failed parse online player storage. Setting back to defaults."));
					PlayerStorage->eventSetToDefaults();
				}
			}
			// Copy from local read if successful and if newer than online read
			if (AsyncTaskReadLocal!= NULL && AsyncTaskReadLocal->IsSuccessful())
			{
				FLiveAsyncTaskDataReadLocalPlayerSave* TaskReadData = (FLiveAsyncTaskDataReadLocalPlayerSave*)AsyncTaskReadLocal->TaskData;
				check(TaskReadData != NULL);
				// Parse the settings data and check hash for validity
				FProfileSettingsReaderLive Reader(MAX_PERSISTENT_DATA_SIZE,TaskReadData->GetBuffer(),TaskReadData->GetBufferRead(),OnlineNetId.Uid);
				TArray<FOnlineProfileSetting> LocalProfileSettings;
				if (Reader.SerializeFromBuffer(LocalProfileSettings))
				{
					// Num saves for online copy
					INT OnlineSaveCount = UOnlinePlayerStorage::GetProfileSaveCount(PlayerStorage->ProfileSettings, PlayerStorage->SaveCountSettingId);
					// Num saves for local copy
					INT LocalSaveCount = UOnlinePlayerStorage::GetProfileSaveCount(LocalProfileSettings, PlayerStorage->SaveCountSettingId);
					// If the local profile settings has been saved more recently than the online one then replace it
					if (LocalSaveCount > OnlineSaveCount)
					{
						PlayerStorage->ProfileSettings = LocalProfileSettings;
						debugf(NAME_DevOnline,
							TEXT("Local copy of player storage was newer. Replacing with local copy."));
						
						//@todo sz - auto attempt online write if local was newer?
					}
				}
				else
				{
					debugf(NAME_DevOnline,
						TEXT("Failed parse local player storage."));
				}
			}

			// Check the version number and reset to defaults if they don't match
			ReadVersionNum = PlayerStorage->GetVersionNumber();
			if (PlayerStorage->VersionNumber != ReadVersionNum)
			{
				if (ReadVersionNum > 0)
				{
					debugf(NAME_DevOnline,
						TEXT("Detected player storage version mismatch (%d != %d). Setting to defaults."),
						PlayerStorage->VersionNumber,
						ReadVersionNum);
				}
				else
				{
					debugf(NAME_DevOnline,
						TEXT("No player storage data to read (online or local). Setting to defaults."));
				}				
				PlayerStorage->eventSetToDefaults();
			}
		}
	}
	return bFinishedReadOnline && bFinishedReadLocal;
}
	
/**
 * Iterates the list of delegates and fires those notifications
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FLiveAsyncTaskParentReadPlayerStorage::TriggerDelegates(UObject* Object)
{
	check(Object);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL)
	{
		// check to see if both local/online tasks have completed successfully
		const UBOOL bSuccessReadOnline = AsyncTaskReadOnline != NULL ? AsyncTaskReadOnline->IsSuccessful() : FALSE;
		const UBOOL bSuccessReadLocal = AsyncTaskReadLocal != NULL ? AsyncTaskReadLocal->IsSuccessful() : FALSE;
		// Pass in the data that indicates whether the call worked or not
		OnlineSubsystemLive_eventOnReadPlayerStorageComplete_Parms Parms(EC_EventParm);
		// Success if either local/online read succeeded or if nothing available to read
		Parms.bWasSuccessful = (bSuccessReadOnline || bSuccessReadLocal || ReadVersionNum == -1) ? FIRST_BITFIELD : 0;
		Parms.LocalUserNum = UserIndex;
		// Use the common method to do the work
		TriggerOnlineDelegates(Object,*ScriptDelegates,&Parms);
	}
}

/** 
 * Task is only added if valid
 *
 * @return FALSE if failed to initialize properly 
 */
UBOOL FLiveAsyncTaskParentReadPlayerStorage::IsValid() const
{
	return (AsyncTaskReadOnline != NULL && AsyncTaskReadOnline->IsValid()) || 
		(AsyncTaskReadLocal != NULL && AsyncTaskReadLocal->IsValid());
}

/**
 * Initializes the server paths for each file that needs to be read.
 * Forwards the call to the base class for proper initialization.
 *
 * @param InScriptDelegates the delegate to fire off when complete
 * @param InTaskData the data associated with the task. Freed when complete
 * @param InPlayerStorage player storage object to keep track of state
 * @param bBuildServerPaths TRUE if server paths should be created
 */
FLiveAsyncTaskReadOnlinePlayerStorage::FLiveAsyncTaskReadOnlinePlayerStorage(
	TArray<FScriptDelegate>* InScriptDelegates, 
	FLiveAsyncTaskDataReadOnlinePlayerStorage* InTaskData,
	UBOOL bBuildServerPaths)
	:	FOnlineAsyncTaskLive(InScriptDelegates,InTaskData,TEXT("Online Read Player Data"))
	,	NextFileToRead(-1)
	,	bIsValid(TRUE)
	,	bReadSucceeded(FALSE)
{
	check(InTaskData != NULL);
	if (bBuildServerPaths)
	{
		// enumeration is not supported for user title storage (XSTORAGE_FACILITY_PER_USER_TITLE) 
		// so assume that max files are always available for reading from the store
		FilesToRead.Empty(MAX_ONLINE_PLAYER_STORAGE_FILES);	
		for (INT FileIdx=0; FileIdx < MAX_ONLINE_PLAYER_STORAGE_FILES; FileIdx++)
		{
			FPlayerStorageFileItem& FileEntry = *new(FilesToRead) FPlayerStorageFileItem();		
			// files are read in the same order as written [0..MAX_ONLINE_PLAYER_STORAGE_FILES]
			const FString FileToRead = FString::Printf(TEXT("PlayerStore%d.dat"),FileIdx);
			// build the online user title storage path
			DWORD Result = XStorageBuildServerPath(
				InTaskData->GetUserIndex(),
				XSTORAGE_FACILITY_PER_USER_TITLE,
				NULL,
				0,
				*FileToRead,
				FileEntry.ServerPath,
				&FileEntry.ServerPathLen);

			if (Result != ERROR_SUCCESS)
			{
				debugf(NAME_DevOnline,TEXT("Player(%d): XStorageBuildServerPath failed for online read. Error=0x%08X"),
					InTaskData->GetUserIndex(), GetLastError());

				// if not valid then this task is not added to the asynch task list
				bIsValid = FALSE;
				break;
			}
		}
	}
}

/**
 * Reads the next available file from the online store 
 * and handles the final concatenation of all the read buffers.
 * Updates bReadSucceeded based on the task result
 *
 * @param LiveSubsystem the object to make the final call on
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskReadOnlinePlayerStorage::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	UBOOL bFinished = FALSE;
	// could be called even when not done since this is a sub-task
	if (GetCompletionCode() == ERROR_IO_PENDING)
	{
		return FALSE;
	}
	check(TaskData != NULL);
	FLiveAsyncTaskDataReadOnlinePlayerStorage* TaskReadData = (FLiveAsyncTaskDataReadOnlinePlayerStorage*)TaskData;
	const INT UserIndex = TaskReadData->GetUserIndex();
	
	if (FilesToRead.IsValidIndex(NextFileToRead))
	{
		FPlayerStorageFileItem& LastFile = FilesToRead(NextFileToRead);

		// Make sure the last file read completed ok
		if (LastFile.AsyncState == OERS_InProgress &&
			XGetOverlappedExtendedError(&Overlapped) == ERROR_SUCCESS)
		{
			// At this point the contents of the file should have been read from online storage
			LastFile.AsyncState = OERS_Done;
			// update the size of data that was read for the file
			LastFile.FileBufferSize = LastFile.DownloadResults.dwBytesTotal;
			// update the current position of the read buffer pointer
			TaskReadData->SetBufferNext(TaskReadData->GetBufferNext()+LastFile.FileBufferSize);
			// done reading files if the last file read was nut a full file
			if (LastFile.DownloadResults.dwBytesTotal < MAX_ONLINE_PLAYER_STORAGE_FILESIZE)
			{
				bFinished = TRUE;
			}			
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Failed to read file '%s' from online player storage. Can't read file."),
				LastFile.ServerPath);
			// keep track of file entries that failed
			LastFile.AsyncState = OERS_Failed;
			// no need to continue reading files if one failed
			bFinished = TRUE;
		}
	}
	
	// Move to the next file
	NextFileToRead++;	
	if (!bFinished &&
		FilesToRead.IsValidIndex(NextFileToRead))
	{
		FPlayerStorageFileItem& NextFile = FilesToRead(NextFileToRead);
		NextFile.FileBuffer = TaskReadData->GetBufferNext();
		
		DWORD Result = XONLINE_E_SESSION_WRONG_STATE;		
		// make sure we don't exceeded the max read buffer size
		if ((TaskReadData->GetBufferNext()+MAX_ONLINE_PLAYER_STORAGE_FILESIZE) <= (TaskReadData->GetBuffer()+TaskReadData->GetBufferSize()))
		{
			// Kick off the next async task to upload the file to Live
			Result = XStorageDownloadToMemory(
				UserIndex,
				NextFile.ServerPath,
				MAX_ONLINE_PLAYER_STORAGE_FILESIZE,
				NextFile.FileBuffer,
				sizeof(NextFile.DownloadResults),
				&NextFile.DownloadResults,
				&Overlapped
				);

			debugfLiveSlow(NAME_DevLive,TEXT("[%d] XStorageDownloadFromMemory returned 0x%08X with path %s"),
				UserIndex,
				Result,
				NextFile.ServerPath);
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Failed to read file '%s' from online player storage. Not enough read buffer space."),
				NextFile.ServerPath);		
		}
		
		if (Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING)
		{
			// once we iterate and check the completion state we will be able to mark the state as OERS_Done
			NextFile.AsyncState = OERS_InProgress;
		}
		else
		{
			// no need to continue since the download couldn't be initiated
			bFinished = TRUE;
			NextFile.AsyncState = OERS_Failed;
		}
	}
	else
	{
		// no more files to process
		bFinished = TRUE;
	}
	
	if (bFinished)
	{
		// Check async state for all files for completion
		UBOOL bAllReadsSucceeded = TRUE;
		for (INT FileIdx=0; FileIdx < NextFileToRead; FileIdx++)
		{
			FPlayerStorageFileItem& FileRead = FilesToRead(FileIdx);
			if (FileRead.AsyncState != OERS_Done)
			{
				bAllReadsSucceeded = FALSE;
				break;
			}
		}
		if (bAllReadsSucceeded && TaskReadData->GetBufferRead() > 0)
		{
			bReadSucceeded = TRUE;
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Failed to read all files for online player storage."));
		}
	}
	return bFinished;
}

/**
 * Iterates the list of delegates and fires those notifications
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FLiveAsyncTaskReadOnlinePlayerStorage::TriggerDelegates(UObject* Object)
{
	check(Object != NULL);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL)
	{
		// check async state for proper completion to make sure all files were written
		const UBOOL bSucceeded = IsSuccessful();
		// Pass in the data that indicates whether the call worked or not
		OnlineSubsystemLive_eventOnReadPlayerStorageComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = bSucceeded ? FIRST_BITFIELD : 0;
		Parms.LocalUserNum = (BYTE)((FLiveAsyncTaskDataReadOnlinePlayerStorage*)TaskData)->GetUserIndex();
		// Use the common method to do the work
		TriggerOnlineDelegates(Object,*ScriptDelegates,&Parms);
	}
}

/**
 * Initiates the XContent meta file creation for a local storage save.
 *
 * @param InScriptDelegates the delegate to fire off when complete
 * @param InTaskData the data associated with the task. Freed when complete
 */
FLiveAsyncTaskReadLocalPlayerSave::FLiveAsyncTaskReadLocalPlayerSave(
	TArray<FScriptDelegate>* InScriptDelegates, 
	FLiveAsyncTaskDataReadLocalPlayerSave* InTaskData)
	:	FOnlineAsyncTaskLive(InScriptDelegates,InTaskData,TEXT("Local Read Player Data"))
	,	bIsValid(TRUE)
	,	bReadSucceeded(FALSE)
	,	ReadState(RSS_NotStarted)
	,	FileHandle(INVALID_HANDLE_VALUE)
{
	check(InTaskData != NULL);
	appMemzero(&FileOverlapped,sizeof(OVERLAPPED));
#if CONSOLE
	// Fill in the XCONTENT_DATA to access the player save 
	const FContentSaveFileInfo& SaveFileInfo = InTaskData->GetSaveFileInfo();
	appMemzero(&ContentData,sizeof(XCONTENT_DATA));
	appStrcpy(ContentData.szDisplayName, *SaveFileInfo.DisplayName);
	appStrcpyANSI(ContentData.szFileName, TCHAR_TO_ANSI(*SaveFileInfo.Filename));
	ContentData.dwContentType = XCONTENTTYPE_SAVEDGAME;
	ContentData.DeviceID = InTaskData->GetDeviceID();

	// Kick off the content meta data for the player save
	DWORD Return = XContentCreate(
		InTaskData->GetUserIndex(), 
		TCHAR_TO_ANSI(*SaveFileInfo.LogicalPath),
		&ContentData, 
		XCONTENTFLAG_OPENEXISTING, 
		NULL, 
		NULL, 
		&Overlapped);

	if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
	{
		ReadState = RSS_CreatingMeta;
	}
	else
	{
		bIsValid = FALSE;
		debugf(NAME_DevOnline,TEXT("Player(%d): XContentCreate failed for local read. Error=0x%08X"),
			InTaskData->GetUserIndex(), GetLastError());
	}
#endif
}

/**
 * Process the local file read as well as the XContent creation.
 * Updates bReadSucceeded based on the task result
 *
 * @param LiveSubsystem the object to make the final call on
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskReadLocalPlayerSave::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	FLiveAsyncTaskDataReadLocalPlayerSave* LocalReadData = (FLiveAsyncTaskDataReadLocalPlayerSave*)TaskData;

	DWORD Result = GetCompletionCode();
	if (ReadState != RSS_Finished && Result != ERROR_IO_PENDING)
	{	
#if CONSOLE
		FString FileName(
			FString::Printf(TEXT("%s:\\%s"),
			*LocalReadData->GetSaveFileInfo().LogicalPath,
			*LocalReadData->GetSaveFileInfo().Filename));
#else
		FString FileName(
			FString::Printf(
			TEXT("%sSaveData\\%s_%s"),
			*appGameDir(),
			*LiveSubsystem->GetPlayerNickname(LocalReadData->GetUserIndex()),
			*LocalReadData->GetSaveFileInfo().Filename));
#endif
		if (Result == ERROR_SUCCESS)
		{
			// Check for a kicked off read that has completed
			if (ReadState == RSS_ReadingFile && HasOverlappedIoCompleted(&FileOverlapped))
			{
				DWORD BytesRead = 0;
				GetOverlappedResult(FileHandle,&FileOverlapped,&BytesRead,TRUE);
				LocalReadData->SetBufferNext(LocalReadData->GetBufferNext() + BytesRead);
				ReadState = RSS_Finished;
				bReadSucceeded = TRUE;
			}
			else
			{
				// Start async file IO
				FileHandle = CreateFileA(TCHAR_TO_ANSI(*FileName),GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,NULL);
				if (FileHandle != INVALID_HANDLE_VALUE)
				{
					if (!ReadFile(FileHandle,(void*)LocalReadData->GetBuffer(),LocalReadData->GetBufferSize(),NULL,&FileOverlapped))
					{
						DWORD ReadResult = GetLastError();
						if (ReadResult == ERROR_IO_PENDING)
						{
							ReadState = RSS_ReadingFile;
						}
						else
						{
							debugf(NAME_DevOnline,TEXT("Player(%d): ReadFile failed for local read of file '%s'. Error=0x%08X"),
								LocalReadData->GetUserIndex(), *FileName, GetLastError());
						}
					}
				}
				else
				{
					DWORD CreateResult = GetLastError();
					debugf(NAME_DevOnline,TEXT("Player(%d): CreateFile failed for local read of file '%s'. Error=0x%08X"),
						LocalReadData->GetUserIndex(), *FileName, CreateResult);
				}
			}
			// Close file and meta content when IO is done
			if (ReadState != RSS_ReadingFile)
			{
				if (bReadSucceeded)
				{
					debugf(NAME_DevOnline,TEXT("Player(%d): Local read succeeded for file '%s'."),
						LocalReadData->GetUserIndex(), *FileName);
				}
				ReadState = RSS_Finished;
				if (FileHandle != INVALID_HANDLE_VALUE)
				{
					CloseHandle(FileHandle);
				}
#if CONSOLE
				DWORD CloseResult = XContentClose(TCHAR_TO_ANSI(*LocalReadData->GetSaveFileInfo().LogicalPath), &Overlapped);
				if (CloseResult == ERROR_IO_PENDING)
				{
					// allow task to continue for close to finish
					return FALSE;
				}
#endif			
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Player(%d): pending XContentCreate failed for local read. Error=0x%08X"),
				LocalReadData->GetUserIndex(), GetLastError());

			ReadState = RSS_Finished;
		}
	}
	return ReadState == RSS_Finished;
}

/**
 * Iterates the list of delegates and fires those notifications
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FLiveAsyncTaskReadLocalPlayerSave::TriggerDelegates(UObject* Object)
{
	check(Object);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL)
	{
		// Pass in the data that indicates whether the call worked or not
		OnlineSubsystemLive_eventOnReadPlayerStorageComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = IsSuccessful() ? FIRST_BITFIELD : 0;
		Parms.LocalUserNum = (BYTE)((FLiveAsyncTaskDataReadLocalPlayerSave*)TaskData)->GetUserIndex();
		// Use the common method to do the work
		TriggerOnlineDelegates(Object,*ScriptDelegates,&Parms);
	}
}

/**
 * Reads the online player storage data for a given net user
 *
 * @param LocalUserNum the local user that is initiating the read
 * @param NetId the net user that we are reading the data for
 * @param PlayerStorage the object to copy the results to and contains the list of items to read
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineSubsystemLive::ReadPlayerStorage(BYTE LocalUserNum,UOnlinePlayerStorage* PlayerStorage,INT DeviceID)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Validate the player index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Only read if we don't already have cached data for this player
		if (PlayerStorageCacheLocal[LocalUserNum].PlayerStorage == NULL)
		{
			if (PlayerStorage != NULL)
			{
				// cache the current item being read so it won't be GC'ed and for later access
				PlayerStorageCacheLocal[LocalUserNum].PlayerStorage = PlayerStorage;
				// keep track of current state to prevent overlapped reads
				PlayerStorage->AsyncState = OPAS_Read;
				// Clear the previous set of results
				PlayerStorage->ProfileSettings.Empty();
				// Determine the login state of the user
				const ELoginStatus UserLoginStatus = (ELoginStatus)GetLoginStatus(LocalUserNum);

				// For Live login both local/online stores are read, for Local login only the local store is updated
				if (UserLoginStatus > LS_NotLoggedIn && !IsGuestLogin(LocalUserNum))
				{
#if CONSOLE
					// Check to see if the device ID is valid and has enough space
					const INT ValidatedDeviceId = IsDeviceValid(DeviceID,0) ? DeviceID : -1;
#else
					// Just force valid device on PC since we just read/write directly to file
					const INT ValidatedDeviceId = DeviceID;
#endif
					if (ValidatedDeviceId == -1)
					{
						debugf(NAME_DevOnline,TEXT("Player(%d): No valid storage device (%d) specified or not enough space. Read (local) aborted."),
							LocalUserNum,DeviceID);
					}

					// Get the online XUID for the user. This will be used to sign the profile settings
					FUniqueNetId OnlineNetId;
					XUSER_SIGNIN_INFO SigninInfoOnline;
					DWORD ErrorCode = XUserGetSigninInfo(LocalUserNum,XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY,&SigninInfoOnline);
					if (ErrorCode == ERROR_SUCCESS)
					{
						OnlineNetId.Uid = SigninInfoOnline.xuid;
					}

					// Parent task contains sub-tasks for writing to both the local/online storage
					FLiveAsyncTaskParentReadPlayerStorage* AsyncTask = new FLiveAsyncTaskParentReadPlayerStorage(
						LocalUserNum,
						UserLoginStatus,
						OnlineNetId,
						&PlayerStorageCacheLocal[LocalUserNum].ReadDelegates,
						PlayerStorage,
						ValidatedDeviceId);
						
					if (AsyncTask->IsValid())
					{
						// Queue the async task for ticking					
						AsyncTasks.AddItem(AsyncTask);
						Return = ERROR_IO_PENDING;
					}
					else
					{
						debugf(NAME_DevOnline,TEXT("Player(%d): Failed to kick off player storage task. Read aborted."),LocalUserNum);
						delete AsyncTask;
						PlayerStorage->AsyncState = OPAS_Finished;
					}
				}
				else
				{
					debugf(NAME_DevOnline,TEXT("Player(%d): Using defaults for storage read for non-signed in or guest user."),LocalUserNum);
					// Set the profile to the defaults when not signed in
					PlayerStorage->eventSetToDefaults();
					PlayerStorage->AsyncState = OPAS_Finished;
					Return = ERROR_SUCCESS;
				}
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Player(%d): Can't specify a null player storage object."),LocalUserNum);
			}
		}
		// Make sure the player storage entry isn't already being read or written, 
		// since this is going to complete immediately
		else if (PlayerStorageCacheLocal[LocalUserNum].PlayerStorage->AsyncState != OPAS_Read &&
				 PlayerStorageCacheLocal[LocalUserNum].PlayerStorage->AsyncState != OPAS_Write)
		{
			debugfLiveSlow(NAME_DevOnline,TEXT("Player(%d): Using cached player storage instead of reading."),LocalUserNum);
			// If the specified read isn't the same as the cached object, copy the
			// data from the cache
			if (PlayerStorageCacheLocal[LocalUserNum].PlayerStorage != PlayerStorage)
			{
				PlayerStorage->ProfileSettings = PlayerStorageCacheLocal[LocalUserNum].PlayerStorage->ProfileSettings;
				PlayerStorageCacheLocal[LocalUserNum].PlayerStorage = PlayerStorage;
			}
			Return = ERROR_SUCCESS;
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Player(%d): Can't read player storage as an async player storage task is already in progress."),LocalUserNum);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to ReadPlayerStorage()"),
			(DWORD)LocalUserNum);
	}
	// Trigger the delegate if a task was not kicked off
	if (Return != ERROR_IO_PENDING)
	{
		if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
		{
			if (PlayerStorageCacheLocal[LocalUserNum].PlayerStorage != NULL)
			{
				// Remove the read state so that subsequent reads work
				PlayerStorageCacheLocal[LocalUserNum].PlayerStorage->AsyncState = OPAS_Finished;
			}
			// Send the notification of error completion
			OnlineSubsystemLive_eventOnReadPlayerStorageComplete_Parms Results(EC_EventParm);
			Results.LocalUserNum = LocalUserNum;
			Results.bWasSuccessful = Return == ERROR_SUCCESS ? FIRST_BITFIELD : FALSE;
			TriggerOnlineDelegates(this,PlayerStorageCacheLocal[LocalUserNum].ReadDelegates,&Results);
		}
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Initializes the server paths for each file that needs to be read.  Remote user paths in this case.
 * Forwards the call to the base class for proper initialization.
 *
 * @param InScriptDelegates the delegate to fire off when complete
 * @param InTaskData the data associated with the task. Freed when complete
 * @param InPlayerStorage player storage object to keep track of state
 */
FLiveAsyncTaskReadOnlinePlayerStorageRemote::FLiveAsyncTaskReadOnlinePlayerStorageRemote(
	TArray<FScriptDelegate>* InScriptDelegates, 
	FLiveAsyncTaskDataReadOnlinePlayerStorageRemote* InTaskData,
	UOnlinePlayerStorage* InPlayerStorage)
	:	FLiveAsyncTaskReadOnlinePlayerStorage(InScriptDelegates,InTaskData,FALSE)
{
	// enumeration is not supported for user title storage (XSTORAGE_FACILITY_PER_USER_TITLE) 
	// so assume that max files are always available for reading from the store
	FilesToRead.Empty(MAX_ONLINE_PLAYER_STORAGE_FILES);	
	for (INT FileIdx=0; FileIdx < MAX_ONLINE_PLAYER_STORAGE_FILES; FileIdx++)
	{
		FPlayerStorageFileItem& FileEntry = *new(FilesToRead) FPlayerStorageFileItem();		
		// files are read in the same order as written [0..MAX_ONLINE_PLAYER_STORAGE_FILES]
		const FString FileToRead = FString::Printf(TEXT("PlayerStore%d.dat"),FileIdx);
		// build the online user title storage path	using the remote user's net id
		DWORD Result = XStorageBuildServerPathByXuid(
			(const XUID&)InTaskData->GetNetId(),
			XSTORAGE_FACILITY_PER_USER_TITLE,
			NULL,
			0,
			*FileToRead,
			FileEntry.ServerPath,
			&FileEntry.ServerPathLen);

		if (Result != ERROR_SUCCESS)
		{
			debugf(NAME_DevOnline,TEXT("Player(%d): XStorageBuildServerPathByXuid failed for online read. Error=0x%08X"),
				InTaskData->GetUserIndex(), GetLastError());

			// if not valid then this task is not added to the asynch task list
			bIsValid = FALSE;
			break;
		}
	}
}

/**
 * Reads the next available file from the online store 
 * and handles the final concatenation of all the read buffers.
 *
 * @param LiveSubsystem the object to make the final call on
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskReadOnlinePlayerStorageRemote::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	UBOOL bFinished = FLiveAsyncTaskReadOnlinePlayerStorage::ProcessAsyncResults(LiveSubsystem);
	if (bFinished)
	{
		// In case this gets cleared while in progress
		if (PlayerStorage != NULL)
		{
			// Done with the reading, so mark the async state
			PlayerStorage->AsyncState = OPAS_Finished;
			// Set the profile to the defaults when a read failed
			if (!bReadSucceeded)
			{
				PlayerStorage->eventSetToDefaults();
			}
		}
	}
	return bFinished;
}

/**
 * Iterates the list of delegates and fires those notifications
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FLiveAsyncTaskReadOnlinePlayerStorageRemote::TriggerDelegates(UObject* Object)
{
	check(Object != NULL);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL)
	{
		// check async state for proper completion to make sure all files were written
		const UBOOL bSucceeded = bReadSucceeded && (GetCompletionCode() == ERROR_SUCCESS);
		// Pass in the data that indicates whether the call worked or not
		OnlineSubsystemLive_eventOnReadPlayerStorageForNetIdComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = bSucceeded ? FIRST_BITFIELD : 0;
		Parms.NetId = ((FLiveAsyncTaskDataReadOnlinePlayerStorageRemote*)TaskData)->GetNetId();
		// Use the common method to do the work
		TriggerOnlineDelegates(Object,*ScriptDelegates,&Parms);
	}
}

/**
 * Reads the online player storage data for a given net user
 *
 * @param LocalUserNum the local user that is initiating the read
 * @param NetId the net user that we are reading the data for
 * @param PlayerStorage the object to copy the results to and contains the list of items to read
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineSubsystemLive::ReadPlayerStorageForNetId(BYTE LocalUserNum,struct FUniqueNetId NetId,UOnlinePlayerStorage* PlayerStorage)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Get the cached entry based on the unique net id of the remote player
	FPlayerStorageSettingsCacheRemote* CachedEntry = PlayerStorageCacheRemote.Find(NetId.Uid);	
	if (NetId.HasValue())
	{
		if (CachedEntry == NULL)
		{
			CachedEntry = &PlayerStorageCacheRemote.Set(NetId.Uid,FPlayerStorageSettingsCacheRemote(EC_EventParm));
		}
		// Only read if we don't already have cached data for this player
		if (CachedEntry->PlayerStorage == NULL)
		{
			if (PlayerStorage != NULL)
			{
				// cache the current item being read so it won't be GC'ed and for later access
				CachedEntry->PlayerStorage = PlayerStorage;
				// keep track of current state to prevent overlapped reads
				PlayerStorage->AsyncState = OPAS_Read;
				// Clear the previous set of results
				PlayerStorage->ProfileSettings.Empty();
				// make sure user is logged in otherwise can't read from Live			o
				if (XUserGetSigninState(LocalUserNum) == eXUserSigninState_SignedInToLive &&
 					IsGuestLogin(LocalUserNum) == FALSE)
				{
					// Contains the full data during the async read from the player store 
					FLiveAsyncTaskDataReadOnlinePlayerStorageRemote* ReadData = 
						new FLiveAsyncTaskDataReadOnlinePlayerStorageRemote(LocalUserNum,NetId);
					
					// Async task to read from the online player store 
					FLiveAsyncTaskReadOnlinePlayerStorageRemote* AsyncTask = 
						new FLiveAsyncTaskReadOnlinePlayerStorageRemote(&CachedEntry->ReadDelegates,ReadData,PlayerStorage);
						
					if (AsyncTask->IsValid())
					{
						// Queue the async task for ticking
						AsyncTasks.AddItem(AsyncTask);
						Return = ERROR_IO_PENDING;
					}
					else
					{
						debugf(NAME_DevOnline,TEXT("Player(0x%016I64X): Failed to kick off online player storage task. Read aborted."),NetId.Uid);
						// failed to start asyc read so nothing to cache
						CachedEntry->PlayerStorage = NULL;
						// Just trigger the delegate as having failed
						OnlineSubsystemLive_eventOnReadPlayerStorageForNetIdComplete_Parms Results(EC_EventParm);
						Results.NetId = NetId;
						Results.bWasSuccessful = FALSE;
						TriggerOnlineDelegates(this,CachedEntry->ReadDelegates,&Results);
						delete AsyncTask;
					}
				}
				else
				{
					debugf(NAME_DevOnline,TEXT("Player(%d): Skipping storage read for non-signed in user."),LocalUserNum);
					// Set the profile to the defaults when not signed in
					PlayerStorage->eventSetToDefaults();
					PlayerStorage->AsyncState = OPAS_Finished;
					// failed to read so nothing to cache
					CachedEntry->PlayerStorage = NULL;
					// Just trigger the delegate as having succeeded
					OnlineSubsystemLive_eventOnReadPlayerStorageForNetIdComplete_Parms Results(EC_EventParm);
					Results.NetId = NetId;
					Results.bWasSuccessful = FIRST_BITFIELD;
					TriggerOnlineDelegates(this,CachedEntry->ReadDelegates,&Results);
				}
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Player(0x%016I64X): Can't specify a null player storage object."),NetId.Uid);
			}
		}
		// Make sure the player storage entry isn't already being read, 
		// since this is going to complete immediately
		else if (CachedEntry->PlayerStorage->AsyncState != OPAS_Read)
		{
			debugfLiveSlow(NAME_DevOnline,TEXT("Player(0x%016I64X): Using cached player storage instead of reading."),NetId.Uid);
			// If the specified read isn't the same as the cached object, copy the
			// data from the cache
			if (CachedEntry->PlayerStorage != PlayerStorage)
			{
				PlayerStorage->ProfileSettings = CachedEntry->PlayerStorage->ProfileSettings;
				CachedEntry->PlayerStorage = PlayerStorage;
			}
			// Just trigger the read delegate as being done
			// Send the notification of completion
			OnlineSubsystemLive_eventOnReadPlayerStorageForNetIdComplete_Parms Results(EC_EventParm);
			Results.NetId = NetId;
			Results.bWasSuccessful = FIRST_BITFIELD;
			TriggerOnlineDelegates(this,CachedEntry->ReadDelegates,&Results);
			Return = ERROR_SUCCESS;
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Player(0x%016I64X): Can't read player storage as an async player storage task is already in progress."),NetId.Uid);
			// Just trigger the read delegate as failed
			OnlineSubsystemLive_eventOnReadPlayerStorageForNetIdComplete_Parms Results(EC_EventParm);
			Results.NetId = NetId;
			Results.bWasSuccessful = FALSE;		
			TriggerOnlineDelegates(this,CachedEntry->ReadDelegates,&Results);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Player(0x%016I64X): Skipping storage read. Invalid net id specified."),NetId.Uid);
		if (CachedEntry != NULL)
		{
			OnlineSubsystemLive_eventOnReadPlayerStorageForNetIdComplete_Parms Results(EC_EventParm);
			Results.NetId = NetId;
			Results.bWasSuccessful = FALSE;
			TriggerOnlineDelegates(this,CachedEntry->ReadDelegates,&Results);
		}
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Sets the delegate used to notify the gameplay code that the last read request has completed
 *
 * @param NetId the net id for the user to watch for read complete notifications
 * @param ReadPlayerStorageForNetIdCompleteDelegate the delegate to use for notifications
 */
void UOnlineSubsystemLive::AddReadPlayerStorageForNetIdCompleteDelegate(struct FUniqueNetId NetId,FScriptDelegate ReadPlayerStorageForNetIdCompleteDelegate)
{	
	if (!NetId.HasValue())
	{
		debugf(NAME_DevOnline,TEXT("Player(0x%016I64X): AddReadPlayerStorageForNetIdCompleteDelegate invalid net id specified."),NetId.Uid);
	}
	else
	{
		// Get the cached entry based on the unique net id of the remote player
		FPlayerStorageSettingsCacheRemote* CachedEntry = PlayerStorageCacheRemote.Find(NetId.Uid);
		if (CachedEntry == NULL)
		{
			CachedEntry = &PlayerStorageCacheRemote.Set(NetId.Uid,FPlayerStorageSettingsCacheRemote(EC_EventParm));
		}
		CachedEntry->ReadDelegates.AddUniqueItem(ReadPlayerStorageForNetIdCompleteDelegate);
	}
}

/**
 * Searches the existing set of delegates for the one specified and removes it
 * from the list
 *
 * @param NetId the net id for the user to watch for read complete notifications
 * @param ReadPlayerStorageForNetIdCompleteDelegate the delegate to find and clear
 */
void UOnlineSubsystemLive::ClearReadPlayerStorageForNetIdCompleteDelegate(struct FUniqueNetId NetId,FScriptDelegate ReadPlayerStorageForNetIdCompleteDelegate)
{
	if (!NetId.HasValue())
	{
		debugf(NAME_DevOnline,TEXT("Player(0x%016I64X): ClearReadPlayerStorageForNetIdCompleteDelegate invalid net id specified."),NetId.Uid);
	}
	else
	{
		// Get the cached entry based on the unique net id of the remote player
		FPlayerStorageSettingsCacheRemote* CachedEntry = PlayerStorageCacheRemote.Find(NetId.Uid);
		if (CachedEntry != NULL)
		{
			CachedEntry->ReadDelegates.RemoveItem(ReadPlayerStorageForNetIdCompleteDelegate);
			if (CachedEntry->ReadDelegates.Num() == 0)
			{
				PlayerStorageCacheRemote.Remove(NetId.Uid);
			}
		}
	}
}

/**
 * Constructs sub-tasks for local/online writes.
 *
 * @param InUserIndex the user the write is being performed for
 * @param InUserLoginStatus signin status of the user's profile
 * @param InOnlineNetId xuid of player signed into Live
 * @param InScriptDelegates the delegate to fire off when complete
 * @param InPlayerStorage storage object to process with the writes
 * @param InDeviceID validated device ID for local read or -1 if invalid
 */
FLiveAsyncTaskParentWritePlayerStorage::FLiveAsyncTaskParentWritePlayerStorage(
	BYTE InUserIndex,
	ELoginStatus InUserLoginStatus,
	const FUniqueNetId& InOnlineNetId,
	TArray<FScriptDelegate>* InScriptDelegates,
	UOnlinePlayerStorage* InPlayerStorage,
	INT InDeviceID)
	:	FOnlineAsyncTaskLive(InScriptDelegates,NULL,TEXT("Write Player Storage"))
	,	UserIndex(InUserIndex)
	,	UserLoginStatus(InUserLoginStatus)
	,	OnlineNetId(InOnlineNetId)
	,	PlayerStorage(InPlayerStorage)
	,	AsyncTaskWriteOnline(NULL)
	,	AsyncTaskWriteLocal(NULL)
{
	check(PlayerStorage != NULL);

	FProfileSettingsWriterLive SettingsWriter(MAX_PERSISTENT_DATA_SIZE,InOnlineNetId.Uid);
	if (SettingsWriter.SerializeToBuffer(PlayerStorage->ProfileSettings))
	{
		// If signed in to a Live profile then write to online storage
		if (UserLoginStatus == LS_LoggedIn)
		{
			// Keep track of buffer data and state needed to write to online storage
			FLiveAsyncTaskDataWriteOnlinePlayerStorage* WriteDataOnline = 
				new FLiveAsyncTaskDataWriteOnlinePlayerStorage(UserIndex,SettingsWriter.GetFinalBuffer(),SettingsWriter.GetFinalBufferLength());

			// Create sub-task for writing to online storage. Delegates and PlayerStorage write state handled by parent task
			AsyncTaskWriteOnline = new FLiveAsyncTaskWriteOnlinePlayerStorage(NULL,WriteDataOnline);
			if (!AsyncTaskWriteOnline->IsValid())
			{
				delete AsyncTaskWriteOnline;
				AsyncTaskWriteOnline = NULL;
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Player(%d): Online player storage write started."),UserIndex);
			}
		}			
		// If signed in to a profile (Live or not) write to local storage 
		if (UserLoginStatus > LS_NotLoggedIn && InDeviceID != -1)
		{
			// Generate a unique content name for each user that is saving
			const FString& SaveDrive = FString::Printf(TEXT("savedrive%d"),UserIndex);
			// Keep track of buffer data and state needed to write to local storage
			FLiveAsyncTaskDataWriteLocalPlayerSave* WriteDataLocal = new FLiveAsyncTaskDataWriteLocalPlayerSave(
				UserIndex,
				InDeviceID,
				SettingsWriter.
				GetFinalBuffer(),
				SettingsWriter.GetFinalBufferLength(),
				*SaveDrive,
				TEXT("PlayerStorage.dat"),
				TEXT("Player Data")
				);

			// Create sub-task for writing to local storage. Delegates and PlayerStorage write state handled by parent task 	
			AsyncTaskWriteLocal = new FLiveAsyncTaskWriteLocalPlayerSave(NULL,WriteDataLocal);
			if (!AsyncTaskWriteLocal->IsValid())
			{
				delete AsyncTaskWriteLocal;
				AsyncTaskWriteLocal = NULL;
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Player(%d): Local player storage write started."),UserIndex);
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Player(%d): Failed to compress buffer for profile settings. Write aborted."),UserIndex);
	}
}

/**
 * Frees the sub-tasks that were allocated
 */
FLiveAsyncTaskParentWritePlayerStorage::~FLiveAsyncTaskParentWritePlayerStorage()
{
	delete TaskData;
	delete AsyncTaskWriteOnline;
	delete AsyncTaskWriteLocal;
}

/**
 * Updates the amount of elapsed time this task has taken
 *
 * @param DeltaTime the amount of time that has passed since the last update
 */
void FLiveAsyncTaskParentWritePlayerStorage::UpdateElapsedTime(FLOAT DeltaTime)
{
	FOnlineAsyncTask::UpdateElapsedTime(DeltaTime);
	if (AsyncTaskWriteOnline != NULL)
	{
		AsyncTaskWriteOnline->UpdateElapsedTime(DeltaTime);
	}
	if (AsyncTaskWriteLocal != NULL)
	{
		AsyncTaskWriteLocal->UpdateElapsedTime(DeltaTime);
	}
}

/**
 * Checks the completion status of the task. Based on completion of sub-tasks as well.
 *
 * @return TRUE if done, FALSE otherwise
 */
UBOOL FLiveAsyncTaskParentWritePlayerStorage::HasTaskCompleted(void) const
{
	// Determine if each task has completed
	const UBOOL bCompleteWriteOnline = AsyncTaskWriteOnline != NULL ? AsyncTaskWriteOnline->HasTaskCompleted() : TRUE;
	const UBOOL bCompleteWriteLocal = AsyncTaskWriteLocal != NULL ? AsyncTaskWriteLocal->HasTaskCompleted() : TRUE;
	// Parent task is not finished until both sub-tasks are done
	return FOnlineAsyncTaskLive::HasTaskCompleted()	&& bCompleteWriteOnline && bCompleteWriteLocal;
}

/**
 * Determine if sub-tasks for writing player storage have finished. 
 * Finalizes the file IO for the player storage write
 *
 * @param LiveSubsystem the object to make the final call on
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskParentWritePlayerStorage::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	// Process next completion async state for each sub-task
	const UBOOL bFinishedWriteOnline = AsyncTaskWriteOnline != NULL ? AsyncTaskWriteOnline->ProcessAsyncResults(LiveSubsystem) : TRUE;
	const UBOOL bFinishedWriteLocal = AsyncTaskWriteLocal != NULL ? AsyncTaskWriteLocal->ProcessAsyncResults(LiveSubsystem) : TRUE;
	// If both tasks are done then parent task is done as well and can be removed
	if (bFinishedWriteOnline && bFinishedWriteLocal)
	{
		// In case this gets cleared while in progress
		// Make sure cached storage is also valid since it could have been cleared during sign in change and GC'd
		if (PlayerStorage != NULL &&
			LiveSubsystem != NULL &&
			LiveSubsystem->PlayerStorageCacheLocal[UserIndex].PlayerStorage != NULL)
		{
			// Done with the writing, so mark the async state
			PlayerStorage->AsyncState = OPAS_Finished;
		}
	}
	return bFinishedWriteOnline && bFinishedWriteLocal;
}
	
/**
 * Iterates the list of delegates and fires those notifications
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FLiveAsyncTaskParentWritePlayerStorage::TriggerDelegates(UObject* Object)
{
	check(Object);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL)
	{
		// check to see if both local/online tasks have completed successfully
		const UBOOL bSuccessWriteOnline = AsyncTaskWriteOnline != NULL ? AsyncTaskWriteOnline->IsSuccessful() : FALSE;
		const UBOOL bSuccessWriteLocal = AsyncTaskWriteLocal != NULL ? AsyncTaskWriteLocal->IsSuccessful() : FALSE;
		// Pass in the data that indicates whether the call worked or not
		OnlineSubsystemLive_eventOnWritePlayerStorageComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = (bSuccessWriteOnline || bSuccessWriteLocal) ? FIRST_BITFIELD : 0;
		Parms.LocalUserNum = UserIndex;
		// Use the common method to do the work
		TriggerOnlineDelegates(Object,*ScriptDelegates,&Parms);
	}
}

/** 
 * Task is only added if valid
 *
 * @return FALSE if failed to initialize properly 
 */
UBOOL FLiveAsyncTaskParentWritePlayerStorage::IsValid() const
{
	return (AsyncTaskWriteOnline != NULL && AsyncTaskWriteOnline->IsValid()) ||
		(AsyncTaskWriteLocal != NULL && AsyncTaskWriteLocal->IsValid());
}

/**
 * Initializes the server paths for each file that needs to be written.
 * Splits up the data to be written amongst multiple files.
 * Forwards the call to the base class for proper initialization.
 *
 * @param InScriptDelegates the delegate to fire off when complete
 * @param InTaskData the data associated with the task. Freed when complete
 * @param InPlayerStorage player storage object to keep track of state
 */
FLiveAsyncTaskWriteOnlinePlayerStorage::FLiveAsyncTaskWriteOnlinePlayerStorage(
	TArray<FScriptDelegate>* InScriptDelegates, 
	FLiveAsyncTaskDataWriteOnlinePlayerStorage* InTaskData)
	:	FOnlineAsyncTaskLive(InScriptDelegates,InTaskData,TEXT("Online Write Player Data"))
	,	NextFileToWrite(-1)
	,	bIsValid(TRUE)
	,	bWriteSucceeded(FALSE)
{
	check(InTaskData);
	FilesToWrite.Empty(MAX_ONLINE_PLAYER_STORAGE_FILES);	
	
	// initialize file entries for each file that gets written	
	DWORD BufferOffset = 0;
	for (INT FileIdx=0; FileIdx < MAX_ONLINE_PLAYER_STORAGE_FILES && BufferOffset < InTaskData->GetBufferSize(); FileIdx++)
	{	
		FPlayerStorageFileItem& FileEntry = *new(FilesToWrite) FPlayerStorageFileItem();
		// split data into chunks of MAX_ONLINE_PLAYER_STORAGE_FILESIZE	to be written by each file					
		FileEntry.FileBuffer = const_cast<BYTE*>(InTaskData->GetBuffer()) + BufferOffset;
		FileEntry.FileBufferSize = MAX_ONLINE_PLAYER_STORAGE_FILESIZE;
		BufferOffset += MAX_ONLINE_PLAYER_STORAGE_FILESIZE;
		// last file entry may be <= MAX_ONLINE_PLAYER_STORAGE_FILESIZE
		if (BufferOffset > InTaskData->GetBufferSize())
		{
			// all file entries except for the last file are of size MAX_ONLINE_PLAYER_STORAGE_FILESIZE
			FileEntry.FileBufferSize = InTaskData->GetBufferSize() % MAX_ONLINE_PLAYER_STORAGE_FILESIZE;
		}
		// maintain explicit file ordering 
		const FString FileToWrite = FString::Printf(TEXT("PlayerStore%d.dat"),FileIdx);
		// build the online user title storage path
		DWORD Result = XStorageBuildServerPath(
			InTaskData->GetUserIndex(),
			XSTORAGE_FACILITY_PER_USER_TITLE,
			NULL,
			0,
			*FileToWrite,
			FileEntry.ServerPath,
			&FileEntry.ServerPathLen);

		if (Result != ERROR_SUCCESS)
		{
			debugf(NAME_DevOnline,TEXT("Player(%d): XStorageBuildServerPath failed for online write. Error=0x%08X"),
				InTaskData->GetUserIndex(), GetLastError());

			// if not valid then this task is not added to the asynch task list
			bIsValid = FALSE;
			break;
		}
	}
}

/**
 * Writes the next file buffer segment to the online store.
 * Updates bWriteSucceeded based on the task result
 *
 * @param LiveSubsystem the object to make the final call on
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskWriteOnlinePlayerStorage::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	UBOOL bFinished = FALSE;
	if (GetCompletionCode() == ERROR_IO_PENDING)
	{
		return FALSE;
	}

	check(TaskData != NULL);
	const INT UserIndex = ((FLiveAsyncTaskDataWriteOnlinePlayerStorage*)TaskData)->GetUserIndex();
	
	if (FilesToWrite.IsValidIndex(NextFileToWrite))
	{
		FPlayerStorageFileItem& LastFile = FilesToWrite(NextFileToWrite);
		
		// Make sure the last file write completed ok
		if (LastFile.AsyncState == OERS_InProgress &&
			XGetOverlappedExtendedError(&Overlapped) == ERROR_SUCCESS)
		{
			// At this point the contents of the file buffer should be on the store
			LastFile.AsyncState = OERS_Done;
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Failed to read file '%s' from online player storage."),
				LastFile.ServerPath);

			// keep track of the file entries that failed
			LastFile.AsyncState = OERS_Failed;			
			// no need to continue writing files if one failed
			bFinished = TRUE;
		}		
	}
	
	// Move to the next file
	NextFileToWrite++;		
	if (!bFinished &&
		FilesToWrite.IsValidIndex(NextFileToWrite))
	{
		FPlayerStorageFileItem& NextFile = FilesToWrite(NextFileToWrite);		

		// Kick off the next async task to upload the file to Live
		DWORD Result = XStorageUploadFromMemory(
			UserIndex,
			NextFile.ServerPath,
			NextFile.FileBufferSize,
			NextFile.FileBuffer,
			&Overlapped
			);
			
		debugfLiveSlow(NAME_DevLive,TEXT("[%d] XStorageUploadFromMemory returned 0x%08X with path %s."),
			UserIndex,
			Result,
			NextFile.ServerPath);
		
		if (Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING)
		{
			// once we iterate and check the completion state we will be able to mark the state as OERS_Done
			NextFile.AsyncState = OERS_InProgress;
		}
		else
		{
			// no need to continue since the upload couldn't be initiated
			bFinished = TRUE;
			NextFile.AsyncState = OERS_Failed;
		}
	}
	else
	{
		// no more files to process
		bFinished = TRUE;
	}
	
	if (bFinished)
	{
		// check async state for proper completion to make sure all files were written
		bWriteSucceeded = GetCompletionCode() == ERROR_SUCCESS;
		for (INT FileIdx=0; FileIdx < FilesToWrite.Num(); FileIdx++)
		{
			FPlayerStorageFileItem& FileWritten = FilesToWrite(FileIdx);
			if (FileWritten.AsyncState != OERS_Done)
			{
				bWriteSucceeded = FALSE;
				break;
			}
		}
	}
	return bFinished;
}

/**
 * Iterates the list of delegates and fires those notifications
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FLiveAsyncTaskWriteOnlinePlayerStorage::TriggerDelegates(UObject* Object)
{
	check(Object);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL)
	{	
		// Pass in the data that indicates whether the call worked or not
		OnlineSubsystemLive_eventOnWritePlayerStorageComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = bWriteSucceeded ? FIRST_BITFIELD : 0;
		Parms.LocalUserNum = (BYTE)((FLiveAsyncTaskDataWriteOnlinePlayerStorage*)TaskData)->GetUserIndex();
		// Use the common method to do the work
		TriggerOnlineDelegates(Object,*ScriptDelegates,&Parms);
	}
}

/**
 * Initializes the server paths for each file that needs to be written.
 * Splits up the data to be written amongst multiple files.
 * Forwards the call to the base class for proper initialization.
 *
 * @param InScriptDelegates the delegate to fire off when complete
 * @param InTaskData the data associated with the task. Freed when complete
 */
FLiveAsyncTaskWriteLocalPlayerSave::FLiveAsyncTaskWriteLocalPlayerSave(
	TArray<FScriptDelegate>* InScriptDelegates, 
	FLiveAsyncTaskDataWriteLocalPlayerSave* InTaskData)
	:	FOnlineAsyncTaskLive(InScriptDelegates,InTaskData,TEXT("Local Write Player Data"))
	,	bIsValid(TRUE)
	,	bWriteSucceeded(FALSE)
	,	WriteState(WSS_NotStarted)
	,	FileHandle(INVALID_HANDLE_VALUE)
{
	check(TaskData != NULL);	
	appMemzero(&FileOverlapped,sizeof(OVERLAPPED));
#if CONSOLE
	const FContentSaveFileInfo& SaveFileInfo = InTaskData->GetSaveFileInfo();
	appMemzero(&ContentData,sizeof(XCONTENT_DATA));
	appStrcpy(ContentData.szDisplayName, *SaveFileInfo.DisplayName);
	appStrcpyANSI(ContentData.szFileName, TCHAR_TO_ANSI(*SaveFileInfo.Filename));
	ContentData.dwContentType = XCONTENTTYPE_SAVEDGAME;
	ContentData.DeviceID = InTaskData->GetDeviceID();

	DWORD Return = XContentCreate(	
		InTaskData->GetUserIndex(), 
		TCHAR_TO_ANSI(*SaveFileInfo.LogicalPath),
		&ContentData, 
		XCONTENTFLAG_CREATEALWAYS | XCONTENTFLAG_NOPROFILE_TRANSFER | XCONTENTFLAG_MOVEONLY_TRANSFER, 
		NULL, 
		NULL, 
		&Overlapped);

	if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
	{
		WriteState = WSS_CreatingMeta;
	}
	else
	{
		bIsValid = FALSE;
		debugf(NAME_DevOnline,TEXT("Player(%d): XContentCreate failed for local write. Error=0x%08X"),
			InTaskData->GetUserIndex(), GetLastError());
	}
#endif
}

/**
 * Process the local read and XContent creation.
 * Updates bWriteSucceeded based on the task result
 *
 * @param LiveSubsystem the object to make the final call on
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FLiveAsyncTaskWriteLocalPlayerSave::ProcessAsyncResults(UOnlineSubsystemLive* LiveSubsystem)
{
	FLiveAsyncTaskDataWriteLocalPlayerSave* LocalWriteData = (FLiveAsyncTaskDataWriteLocalPlayerSave*)TaskData;

	DWORD Result = GetCompletionCode();
	if (WriteState != WSS_Finished && Result != ERROR_IO_PENDING)
	{	
#if CONSOLE
		FString FileName(
			FString::Printf(TEXT("%s:\\%s"),
			*LocalWriteData->GetSaveFileInfo().LogicalPath,
			*LocalWriteData->GetSaveFileInfo().Filename));
#else
		FString FileName(
			FString::Printf(
			TEXT("%sSaveData\\%s_%s"),
			*appGameDir(),
			*LiveSubsystem->GetPlayerNickname(LocalWriteData->GetUserIndex()),
			*LocalWriteData->GetSaveFileInfo().Filename));
#endif
		if (Result == ERROR_SUCCESS)
		{
			// Check for a kicked off write that has completed
			if (WriteState == WSS_WritingFile && HasOverlappedIoCompleted(&FileOverlapped))
			{
				WriteState = WSS_Finished;
				bWriteSucceeded = TRUE;
			}
			else
			{
				// Start async file IO
				FileHandle = CreateFileA(TCHAR_TO_ANSI(*FileName),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,NULL);
				if (FileHandle != INVALID_HANDLE_VALUE)
				{
					if (!WriteFile(FileHandle,(void*)LocalWriteData->GetBuffer(),LocalWriteData->GetBufferSize(),NULL,&FileOverlapped))
					{
						DWORD WriteResult = GetLastError();
						if (WriteResult == ERROR_IO_PENDING)
						{
							WriteState = WSS_WritingFile;
						}
						else
						{
							debugf(NAME_DevOnline,TEXT("Player(%d): WriteFile failed for local write of file '%s'. Error=0x%08X"),
								LocalWriteData->GetUserIndex(), *FileName, GetLastError());
						}
					}
				}
				else
				{
					debugf(NAME_DevOnline,TEXT("Player(%d): CreateFile failed for local write of file '%s'. Error=0x%08X"),
						LocalWriteData->GetUserIndex(), *FileName, GetLastError());
				}
			}
			// Close file and meta content when IO is done
			if (WriteState != WSS_WritingFile)
			{
				if (bWriteSucceeded)
				{
					debugf(NAME_DevOnline,TEXT("Player(%d): Write succeeded for file '%s'."),
						LocalWriteData->GetUserIndex(), *FileName);
				}

				WriteState = WSS_Finished;
				if (FileHandle != INVALID_HANDLE_VALUE)
				{
					CloseHandle(FileHandle);
				}
#if CONSOLE
				DWORD CloseResult = XContentClose(TCHAR_TO_ANSI(*LocalWriteData->GetSaveFileInfo().LogicalPath), &Overlapped);
				if (CloseResult == ERROR_IO_PENDING)
				{
					// allow task to continue for close to finish
					return FALSE;
				}
#endif			
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Player(%d): pending XContentCreate failed for local write. Error=0x%08X"),
				LocalWriteData->GetUserIndex(), GetLastError());

			WriteState = WSS_Finished;
		}
	}
	return WriteState == WSS_Finished;
}

/**
 * Iterates the list of delegates and fires those notifications
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FLiveAsyncTaskWriteLocalPlayerSave::TriggerDelegates(UObject* Object)
{
	check(Object);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL)
	{
		// Pass in the data that indicates whether the call worked or not
		OnlineSubsystemLive_eventOnWritePlayerStorageComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = bWriteSucceeded ? FIRST_BITFIELD : 0;
		Parms.LocalUserNum = (BYTE)((FLiveAsyncTaskDataWriteLocalPlayerSave*)TaskData)->GetUserIndex();
		// Use the common method to do the work
		TriggerOnlineDelegates(Object,*ScriptDelegates,&Parms);
	}
}

/**
 * Writes the score data for the match
 *
 * @param SessionName the name of the session to write scores for
 * @param LeaderboardId the leaderboard to write the score information to
 * @param PlayerScores the list of players, teams, and scores they earned
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::WritePlayerStorage(BYTE LocalUserNum,UOnlinePlayerStorage* PlayerStorage,INT DeviceID)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Validate the player index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Don't allow a write if there is a task already in progress
		if (PlayerStorageCacheLocal[LocalUserNum].PlayerStorage == NULL ||
			(PlayerStorageCacheLocal[LocalUserNum].PlayerStorage->AsyncState != OPAS_Read && PlayerStorageCacheLocal[LocalUserNum].PlayerStorage->AsyncState != OPAS_Write))
		{
			if (PlayerStorage != NULL)
			{
				// Cache to make sure GC doesn't collect this while we are waiting
				// for the Live task to complete
				PlayerStorageCacheLocal[LocalUserNum].PlayerStorage = PlayerStorage;
				// Mark this as a write in progress to prevent overlapped writes
				PlayerStorage->AsyncState = OPAS_Write;
				// Make sure the profile settings have a version number
				PlayerStorage->AppendVersionToSettings();			
				// Update the save count for roaming profile support
				UOnlinePlayerStorage::SetProfileSaveCount(UOnlinePlayerStorage::GetProfileSaveCount(PlayerStorage->ProfileSettings,PlayerStorage->SaveCountSettingId) + 1,PlayerStorage->ProfileSettings,PlayerStorage->SaveCountSettingId);			
				// Determine the login state of the user
				const ELoginStatus UserLoginStatus = (ELoginStatus)GetLoginStatus(LocalUserNum);
				// For Live login both local/online stores are updated, for Local login only the local store is updated
				if (UserLoginStatus > LS_NotLoggedIn && !IsGuestLogin(LocalUserNum))
				{
#if CONSOLE
					// Check to see if the device ID is valid and has enough space
					const INT ValidatedDeviceId = IsDeviceValid(DeviceID,MAX_PERSISTENT_DATA_SIZE) ? DeviceID : -1;
#else
					// Just force valid device on PC since we just read/write directly to file
					const INT ValidatedDeviceId = DeviceID;
#endif
					if (ValidatedDeviceId == -1)
					{
						debugf(NAME_DevOnline,TEXT("Player(%d): No valid storage device (%d) or not enough space. Write (local) aborted."),
							LocalUserNum,DeviceID);
					}

					// Get the online XUID for the user. This will be used to sign the profile settings
					FUniqueNetId OnlineNetId;
					XUSER_SIGNIN_INFO SigninInfoOnline;
					DWORD ErrorCode = XUserGetSigninInfo(LocalUserNum,XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY,&SigninInfoOnline);
					if (ErrorCode == ERROR_SUCCESS)
					{
						OnlineNetId.Uid = SigninInfoOnline.xuid;
					}
					// Parent task contains sub-tasks for writing to both the local/online storage
					FLiveAsyncTaskParentWritePlayerStorage* AsyncTask = new FLiveAsyncTaskParentWritePlayerStorage(
						LocalUserNum,
						UserLoginStatus,
						OnlineNetId,
						&PlayerStorageCacheLocal[LocalUserNum].WriteDelegates,
						PlayerStorage,
						ValidatedDeviceId);

					// Only process if the task initialized correctly
					if (AsyncTask->IsValid())
					{
						// Queue the async task for ticking
						AsyncTasks.AddItem(AsyncTask);
						Return = ERROR_IO_PENDING;
					}
					else
					{
						debugf(NAME_DevOnline,TEXT("Player(%d): Failed to kick off online player storage task. Write aborted."),LocalUserNum);
						delete AsyncTask;
					}
				}
				else
				{
					debugf(NAME_DevOnline,TEXT("Player(%d): Skipping player storage write for non-signed in user."),LocalUserNum);
					Return = ERROR_SUCCESS;
				}
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Player(%d): Can't write a null player storage object."),LocalUserNum);
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Player(%d): Can't write player storage as an async player storage task is already in progress."),LocalUserNum);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to WritePlayerStorage()"),
			(DWORD)LocalUserNum);
	}
	// Trigger the delegate if a task was not kicked off
	if (Return != ERROR_IO_PENDING)
	{
		if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
		{
			if (PlayerStorageCacheLocal[LocalUserNum].PlayerStorage != NULL)
			{
				// Remove the write state so that subsequent writes work
				PlayerStorageCacheLocal[LocalUserNum].PlayerStorage->AsyncState = OPAS_Finished;
			}
			// Send the notification of error completion
			OnlineSubsystemLive_eventOnWritePlayerStorageComplete_Parms Results(EC_EventParm);
			Results.LocalUserNum = LocalUserNum;
			Results.bWasSuccessful = Return == ERROR_SUCCESS ? FIRST_BITFIELD : FALSE;
			TriggerOnlineDelegates(this,PlayerStorageCacheLocal[LocalUserNum].WriteDelegates,&Results);
		}
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Sets a rich presence information to use for the specified player
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PresenceMode the rich presence mode to use
 * @param Contexts the list of contexts to set
 * @param Properties the list of properties to set for the contexts
 */
void UOnlineSubsystemLive::SetOnlineStatus(BYTE LocalUserNum,INT PresenceMode,
	const TArray<FLocalizedStringSetting>& Contexts,
	const TArray<FSettingsProperty>& Properties)
{
	// Set all of the contexts/properties before setting the presence mode
	SetContexts(LocalUserNum,Contexts);
	SetProperties(LocalUserNum,Properties);
	debugf(NAME_DevOnline,TEXT("XUserSetContext(%d,X_CONTEXT_PRESENCE,%d)"),
		LocalUserNum,PresenceMode);
	// Update the presence mode
	XUserSetContext(LocalUserNum,X_CONTEXT_PRESENCE,PresenceMode);
}

/**
 * Displays the invite ui
 *
 * @param LocalUserNum the local user sending the invite
 * @param InviteText the string to prefill the UI with
 */
UBOOL UOnlineSubsystemLive::ShowInviteUI(BYTE LocalUserNum,const FString& InviteText)
{
	DWORD Result = E_FAIL;
	// Validate the user index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Show the game invite UI for the specified controller num
		Result = XShowGameInviteUI(LocalUserNum,NULL,0,
			InviteText.Len() > 0 ? *InviteText : NULL);
		if (Result != ERROR_SUCCESS)
		{
			debugf(NAME_DevOnline,TEXT("XShowInviteUI(%d) failed with 0x%08X"),
				LocalUserNum,Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to ShowInviteUI()"),
			(DWORD)LocalUserNum);
	}
	return Result == ERROR_SUCCESS;
}

/**
 * Displays the marketplace UI for content
 *
 * @param LocalUserNum the local user viewing available content
 * @param CategoryMask the bitmask to use to filter content by type
 * @param OfferId a specific offer that you want shown
 */
UBOOL UOnlineSubsystemLive::ShowContentMarketplaceUI(BYTE LocalUserNum,INT CategoryMask,INT OfferId)
{
	DWORD Result = E_FAIL;
	// Validate the user index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS &&
		// Added to work around the broken Feb XDK
		GetLoginStatus(LocalUserNum) > LS_NotLoggedIn)
	{
		QWORD FinalOfferId = 0;
		if (OfferId)
		{
			FinalOfferId |= ((QWORD)appGetTitleId() << 32);
			FinalOfferId |= (QWORD)OfferId;
		}
		// Show the marketplace for content
		Result = XShowMarketplaceUI(LocalUserNum,
			OfferId != 0 ? XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM : XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST,
			FinalOfferId,
			(DWORD)CategoryMask);
		if (Result != ERROR_SUCCESS)
		{
			debugf(NAME_DevOnline,TEXT("XShowMarketplaceUI(%d,%s,0x%016I64X,0x%08X) failed with 0x%08X"),
				LocalUserNum,
				OfferId != 0 ? TEXT("XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM") : TEXT("XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST"),
				FinalOfferId,
				CategoryMask,
				Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to ShowContentMarketplaceUI()"),
			(DWORD)LocalUserNum);
	}
	return Result == ERROR_SUCCESS;
}

/**
 * Displays the marketplace UI for memberships
 *
 * @param LocalUserNum the local user viewing available memberships
 */
UBOOL UOnlineSubsystemLive::ShowMembershipMarketplaceUI(BYTE LocalUserNum)
{
	DWORD Result = E_FAIL;
	// Validate the user index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Show the marketplace for memberships
		Result = XShowMarketplaceUI(LocalUserNum,
			XSHOWMARKETPLACEUI_ENTRYPOINT_MEMBERSHIPLIST,0,(DWORD)-1);
		if (Result != ERROR_SUCCESS)
		{
			debugf(NAME_DevOnline,TEXT("XShowMarketplaceUI(%d,XSHOWMARKETPLACEUI_ENTRYPOINT_MEMBERSHIPLIST,0,-1) failed with 0x%08X"),
				LocalUserNum,Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to ShowMembershipMarketplaceUI()"),
			(DWORD)LocalUserNum);
	}
	return Result == ERROR_SUCCESS;
}

/**
 * Displays the UI that allows the user to choose which device to save content to
 *
 * @param LocalUserNum the controller number of the associated user
 * @param SizeNeeded the size of the data to be saved in bytes
 * @param bManageStorage whether to allow the user to manage their storage or not
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemLive::ShowDeviceSelectionUI(BYTE LocalUserNum,INT SizeNeeded,UBOOL bManageStorage)
{
	DWORD Return = E_FAIL;
#if CONSOLE
	// Validate the user index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		ULARGE_INTEGER BytesNeeded;
		BytesNeeded.HighPart = 0;
		BytesNeeded.LowPart = SizeNeeded;
		// Allocate an async task for deferred calling of the delegates
		FOnlineAsyncTaskLive* AsyncTask = new FOnlineAsyncTaskLive(&DeviceCache[LocalUserNum].DeviceSelectionDelegates,NULL,TEXT("XShowDeviceSelectorUI()"));
		ULARGE_INTEGER ContentSize = {0,0};
		ContentSize.QuadPart = XContentCalculateSize(BytesNeeded.QuadPart,1);
		DWORD Flags = bManageStorage ? XCONTENTFLAG_MANAGESTORAGE : XCONTENTFLAG_NONE;
		// Show the live guide for selecting a device
		Return = XShowDeviceSelectorUI(LocalUserNum,
			XCONTENTTYPE_SAVEDGAME,
			Flags,
			ContentSize,
			(PXCONTENTDEVICEID)&DeviceCache[LocalUserNum].DeviceID,
			*AsyncTask);
		if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
		{
			AsyncTasks.AddItem(AsyncTask);
		}
		else
		{
			// Just trigger the delegate as having failed
			FAsyncTaskDelegateResults Results(Return);
			TriggerOnlineDelegates(this,DeviceCache[LocalUserNum].DeviceSelectionDelegates,&Results);
			// Don't leak the task/data
			delete AsyncTask;
			debugf(NAME_DevOnline,
				TEXT("XShowDeviceSelectorUI(%d,XCONTENTTYPE_SAVEDGAME,%d,%d,data,data) failed with 0x%08X"),
				LocalUserNum,
				Flags,
				SizeNeeded,
				Return);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified ShowDeviceSelectionUI()"),
			(DWORD)LocalUserNum);
	}
#else
	// Validate the user index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		Return = ERROR_SUCCESS;
		// Just trigger the delegate as having succeeded
		FAsyncTaskDelegateResults Results(Return);
		TriggerOnlineDelegates(this,DeviceCache[LocalUserNum].DeviceSelectionDelegates,&Results);
	}
#endif
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Fetches the results of the device selection
 *
 * @param LocalUserNum the controller number of the associated user
 * @param DeviceName out param that gets a copy of the string
 *
 * @return the ID of the device that was selected
 */
INT UOnlineSubsystemLive::GetDeviceSelectionResults(BYTE LocalUserNum,FString& DeviceName)
{
	DeviceName.Empty();
	INT DeviceId = -1;
#if CONSOLE
	// Validate the user index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Zero means they haven't selected a device
		if (DeviceCache[LocalUserNum].DeviceID > 0)
		{
			// Live asserts if you call with a non-signed in player
			if (XUserGetSigninState(LocalUserNum) != eXUserSigninState_NotSignedIn)
			{
				XDEVICE_DATA DeviceData;
				appMemzero(&DeviceData,sizeof(XDEVICE_DATA));
				// Fetch the data, so we can get the friendly name
				DWORD Return = XContentGetDeviceData(DeviceCache[LocalUserNum].DeviceID,&DeviceData);
				if (Return == ERROR_SUCCESS)
				{
					DeviceName = DeviceData.wszFriendlyName;
					DeviceId = DeviceCache[LocalUserNum].DeviceID;
				}
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("User (%d) is not signed in, returning zero as an error"),
					(DWORD)LocalUserNum);
				DeviceCache[LocalUserNum].DeviceID = -1;
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("User (%d) has not selected a device yet, returning zero as an error"),
				(DWORD)LocalUserNum);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified GetDeviceSelectionResults()"),
			(DWORD)LocalUserNum);
	}
#endif
	return DeviceId;
}

/**
 * Checks the device id to determine if it is still valid (could be removed)
 *
 * @param DeviceId the device to check
 *
 * @return TRUE if valid, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::IsDeviceValid(INT DeviceId,INT SizeNeeded)
{
#if !CONSOLE
	DWORD Result = ERROR_SUCCESS;
#else
	DWORD Result = E_FAIL;
	// Live asserts for a device id of zero
	if (DeviceId > 0)
	{
		Result = XContentGetDeviceState(DeviceId,NULL);
		if (Result == ERROR_SUCCESS && SizeNeeded > 0)
		{
			XDEVICE_DATA DeviceData;
			// Check the space available for this device
			Result = XContentGetDeviceData(DeviceId,&DeviceData);
			if (Result == ERROR_SUCCESS)
			{
				// Compare the size too
				return DeviceData.ulDeviceFreeBytes >= (QWORD)SizeNeeded;
			}
		}
	}
#endif
	return Result == ERROR_SUCCESS;
}

/**
 * Unlocks the specified achievement for the specified user
 *
 * @param LocalUserNum the controller number of the associated user
 * @param AchievementId the id of the achievement to unlock
 *
 * @return TRUE if the call worked, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::UnlockAchievement(BYTE LocalUserNum,INT AchievementId,FLOAT PercentComplete)
{
	DWORD Return = E_FAIL;
	// Validate the user index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		FLiveAsyncTaskDataWriteAchievement* AsyncTaskData = new FLiveAsyncTaskDataWriteAchievement(LocalUserNum,AchievementId);
		// Create a new async task to hold the data
		FOnlineAsyncTaskLive* AsyncTask = new FOnlineAsyncTaskLive(&PerUserDelegates[LocalUserNum].AchievementDelegates,
			AsyncTaskData,TEXT("XUserWriteAchievements()"));
		// Write the achievement to Live
		Return = XUserWriteAchievements(1,
			AsyncTaskData->GetAchievement(),
			*AsyncTask);
		debugfLiveSlow(NAME_DevOnline,TEXT("XUserWriteAchievements(%d,%d) returned 0x%08X"),
			(DWORD)LocalUserNum,AchievementId,Return);
		// Clean up the task if it didn't succeed
		if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
		{
			// Clear any cached achievement data for this title so the sort order is correct
			ClearCachedAchievements(LocalUserNum,0);
			// Queue the async task for ticking
			AsyncTasks.AddItem(AsyncTask);
		}
		else
		{
			// Just trigger the delegate as having failed
			FAsyncTaskDelegateResults Results(Return);
			TriggerOnlineDelegates(this,PerUserDelegates[LocalUserNum].AchievementDelegates,&Results);
			delete AsyncTask;
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to UnlockAchievement()"),
			(DWORD)LocalUserNum);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Unlocks a gamer picture for the local user
 *
 * @param LocalUserNum the user to unlock the picture for
 * @param PictureId the id of the picture to unlock
 *
 * @return TRUE if the call worked, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::UnlockGamerPicture(BYTE LocalUserNum,INT PictureId)
{
	DWORD Return = E_FAIL;
	// Validate the user index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Create a new async task to hold the data
		FOnlineAsyncTaskLive* AsyncTask = new FOnlineAsyncTaskLive(NULL,NULL,TEXT("XUserAwardGamerPicture()"));
		// Unlock the picture with Live
		Return = XUserAwardGamerPicture(LocalUserNum,
			PictureId,
			0,
			*AsyncTask);
		debugfLiveSlow(NAME_DevOnline,TEXT("XUserAwardGamerPicture(%d,%d,0,Overlapped) returned 0x%08X"),
			(DWORD)LocalUserNum,PictureId,Return);
		// Clean up the task if it didn't succeed
		if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
		{
			// Queue the async task for ticking
			AsyncTasks.AddItem(AsyncTask);
		}
		else
		{
			delete AsyncTask;
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to UnlockGamerPicture()"),
			(DWORD)LocalUserNum);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Tells Live that the session is in progress. Matches with join in progress
 * disabled will no longer show up in the search results.
 *
 * @return TRUE if the call succeeds, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::StartOnlineGame(FName SessionName)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Grab the session information by name
	FNamedSession* Session = GetNamedSession(SessionName);
	if (Session && Session->GameSettings && Session->SessionInfo)
	{
		// Lan matches don't report starting to Live
		if (Session->GameSettings->bIsLanMatch == FALSE)
		{
			// Can't start a match multiple times
			if (Session->GameSettings->GameState == OGS_Pending ||
				Session->GameSettings->GameState == OGS_Ended)
			{
#if DEBUG_CONTEXT_LOGGING
				// Log game settings
				DumpGameSettings(Session->GameSettings);
				// Log properties and contexts
				DumpContextsAndProperties(Session->GameSettings);
#endif
				// For each local player, force them to use the same props/contexts again before starting the game session
				// This is needed as a safeguard due to party/game session needing to use context that are shared
				for (DWORD Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
				{
					// Ignore non-Live enabled profiles
					if (XUserGetSigninState(Index) != eXUserSigninState_NotSignedIn)
					{
						// Register all of the context/property information for the session
						SetContextsAndProperties(Index,Session->GameSettings);
					}
				}

				FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
				// Stop handling QoS queries if we aren't join in progress
				if (Session->GameSettings->bAllowJoinInProgress == FALSE ||
					Session->GameSettings->bUsesArbitration == TRUE)
				{
					DisableQoS(Session);
				}
				// Allocate the object that will hold the data throughout the async lifetime
				FOnlineAsyncTaskLive* AsyncTask = new FLiveAsyncTaskStartSession(SessionName,
					Session->GameSettings->bUsesArbitration,
					&StartOnlineGameCompleteDelegates,
					TEXT("XSessionStart()"));
				// Do an async start request
				Return = XSessionStart(SessionInfo->Handle,
					0,
					*AsyncTask);
				debugf(NAME_DevOnline,
					TEXT("XSessionStart() '%s' returned 0x%08X"),
					*SessionName.ToString(),
					Return);
				// Clean up the task if it didn't succeed
				if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
				{
					Session->GameSettings->GameState = OGS_Starting;
					// Queue the async task for ticking
					AsyncTasks.AddItem(AsyncTask);
				}
				else
				{
					delete AsyncTask;
				}
			}
			else
			{
				debugf(NAME_DevOnline,
					TEXT("Can't start an online game for session (%s) in state %s"),
					*SessionName.ToString(),
					*GetOnlineGameStateString(Session->GameSettings->GameState));
			}
		}
		else
		{
			// If this lan match has join in progress disabled, shut down the beacon
			if (Session->GameSettings->bAllowJoinInProgress == FALSE)
			{
				StopLanBeacon();
			}
			Return = ERROR_SUCCESS;
			Session->GameSettings->GameState = OGS_InProgress;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Can't start an online game for session (%s) that hasn't been created"),
			*SessionName.ToString());
	}
	if (Return != ERROR_IO_PENDING)
	{
		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResultsNamedSession Results(SessionName,Return);
		TriggerOnlineDelegates(this,StartOnlineGameCompleteDelegates,&Results);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Tells Live that the session has ended
 *
 * @param SessionName the name of the session to end
 *
 * @return TRUE if the call succeeds, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::EndOnlineGame(FName SessionName)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Grab the session information by name
	FNamedSession* Session = GetNamedSession(SessionName);
	if (Session &&
		Session->GameSettings &&
		Session->SessionInfo)
	{
		// Lan matches don't report ending to Live
		if (Session->GameSettings->bIsLanMatch == FALSE)
		{
			// Don't try to make any Live calls if noone is signed in
			if (AreAnySignedIntoLive())
			{
				// Can't end a match that isn't in progress
				if (Session->GameSettings->GameState == OGS_InProgress)
				{
					Session->GameSettings->GameState = OGS_Ending;
					FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
					// Task that will change the state
					FOnlineAsyncTaskLive* AsyncTask = new FLiveAsyncTaskSessionStateChange(SessionName,
						OGS_Ended,
						&EndOnlineGameCompleteDelegates,
						TEXT("XSessionEnd()"));
					// Do an async end request
					Return = XSessionEnd(SessionInfo->Handle,*AsyncTask);
					debugf(NAME_DevOnline,
						TEXT("XSessionEnd() '%s' returned 0x%08X"),
						*SessionName.ToString(),
						Return);
					// Clean up the task if it didn't succeed
					if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
					{
						// Queue the async task for ticking
						AsyncTasks.AddItem(AsyncTask);
					}
					else
					{
						delete AsyncTask;
					}
				}
				else
				{
					debugf(NAME_DevOnline,
						TEXT("Can't end an online game for session (%s) in state %s"),
						*SessionName.ToString(),
						*GetOnlineGameStateString(Session->GameSettings->GameState));
				}
			}
			else
			{
				debugf(NAME_DevOnline,
					TEXT("Can't end an online game for session (%s) in state %s due to no signed in players"),
					*SessionName.ToString(),
					*GetOnlineGameStateString(Session->GameSettings->GameState));
			}
		}
		else
		{
			if (AreAnySignedIn())
			{
				// If the session should be advertised and the lan beacon was destroyed, recreate
				if (Session->GameSettings->bShouldAdvertise &&
					LanBeacon == NULL &&
					IsServer())
				{
					// Recreate the beacon
					Return = CreateLanGame(0,Session);
				}
				else
				{
					Return = ERROR_SUCCESS;
				}
			}
			else
			{
				debugf(NAME_DevOnline,
					TEXT("Can't end a LAN game for session (%s) in state %s due to no signed in players. Not restarting LAN beacon"),
					*SessionName.ToString(),
					*GetOnlineGameStateString(Session->GameSettings->GameState));

			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Can't end an online game for session (%s) that hasn't been created"),
			*SessionName.ToString());
	}
	if (Return != ERROR_IO_PENDING)
	{
		if (Session && Session->GameSettings)
		{
			Session->GameSettings->GameState = OGS_Ended;
		}
		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResultsNamedSession Results(SessionName,Return);
		TriggerOnlineDelegates(this,EndOnlineGameCompleteDelegates,&Results);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Writes the specified set of scores to the skill tables
 *
 * @param SessionName the name of the session the player scores are for
 * @param LeaderboardId the leaderboard to write the score information to
 * @param PlayerScores the list of scores to write out
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::WriteOnlinePlayerScores(FName SessionName,INT LeaderboardId,const TArray<FOnlinePlayerScore>& PlayerScores)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Grab the session information by name
	FNamedSession* Session = GetNamedSession(SessionName);
	// Can only write skill data as part of a session
	if (Session && Session->GameSettings && Session->SessionInfo)
	{
		FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
		// Perform an async write for each player involved
		for (INT Index = 0; Index < PlayerScores.Num(); Index++)
		{
			XSESSION_VIEW_PROPERTIES Views[1];
			XUSER_PROPERTY Stats[2];
			// Build the skill view
			Views[0].dwViewId = LeaderboardId;
			Views[0].dwNumProperties = 2;
			Views[0].pProperties = Stats;
			// Now build the score info
			Stats[0].dwPropertyId = X_PROPERTY_RELATIVE_SCORE;
			Stats[0].value.nData = PlayerScores(Index).Score;
			Stats[0].value.type = XUSER_DATA_TYPE_INT32;
			// And finally the team info
			Stats[1].dwPropertyId = X_PROPERTY_SESSION_TEAM;
			Stats[1].value.nData = PlayerScores(Index).TeamID;
			Stats[1].value.type = XUSER_DATA_TYPE_INT32;
			// Kick off the async write
			Return = XSessionWriteStats(SessionInfo->Handle,
				(XUID&)PlayerScores(Index).PlayerID,
				1,
				Views,
				NULL);
			debugf(NAME_DevOnline,TEXT("TrueSkill write for '%s' with ViewId (0x%08X) (Player = 0x%016I64X, Team = %d, Score = %d) returned 0x%08X"),
				*SessionName.ToString(),
				LeaderboardId,
				(QWORD&)PlayerScores(Index).PlayerID,
				PlayerScores(Index).TeamID,
				PlayerScores(Index).Score,
				Return);
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Can't write TrueSkill for session (%s) when not in a game"),
			*SessionName.ToString());
	}
	return Return == ERROR_SUCCESS;
}

/**
 * Parses the arbitration results into something the game play code can handle
 *
 * @param SessionName the session that arbitration happened for
 * @param ArbitrationResults the buffer filled by Live
 */
void UOnlineSubsystemLive::ParseArbitrationResults(FName SessionName,PXSESSION_REGISTRATION_RESULTS ArbitrationResults)
{
	// Grab the session information by name
	FNamedSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		// Dump out arbitration data if configured
		if (bShouldLogArbitrationData)
		{
			debugf(NAME_DevOnline,TEXT("Aribitration registration results %d"),
				(DWORD)ArbitrationResults->wNumRegistrants);
		}
		// Iterate through the list and set each entry
		for (DWORD Index = 0; Index < ArbitrationResults->wNumRegistrants; Index++)
		{
			const XSESSION_REGISTRANT& LiveEntry = ArbitrationResults->rgRegistrants[Index];
			// Add a new item for each player listed
			for (DWORD PlayerIndex = 0; PlayerIndex < LiveEntry.bNumUsers; PlayerIndex++)
			{
				INT AddAtIndex = Session->ArbitrationRegistrants.AddZeroed();
				FOnlineArbitrationRegistrant& Entry = Session->ArbitrationRegistrants(AddAtIndex);
				// Copy the data over
				Entry.MachineId = LiveEntry.qwMachineID;
				(XUID&)Entry.PlayerNetId = LiveEntry.rgUsers[PlayerIndex];
				Entry.Trustworthiness = LiveEntry.bTrustworthiness;
				// Dump out arbitration data if configured
				if (bShouldLogArbitrationData)
				{
					debugf(NAME_DevOnline,TEXT("MachineId = 0x%016I64X, PlayerId = 0x%016I64X, Trustworthiness = %d"),
						Entry.MachineId,(XUID&)Entry.PlayerNetId,Entry.Trustworthiness);
				}
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Couldn't find session (%s) to add arbitration results to"),
			*SessionName.ToString());
	}
}

/**
 * Tells the game to register with the underlying arbitration server if available
 *
 * @param SessionName the name of the session to start arbitration for
 *
 * @return TRUE if the async task start up succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::RegisterForArbitration(FName SessionName)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Grab the session information by name
	FNamedSession* Session = GetNamedSession(SessionName);
	// Can't register for arbitration without the host information
	if (Session && Session->GameSettings && Session->SessionInfo)
	{
		// Lan matches don't use arbitration
		if (Session->GameSettings->bIsLanMatch == FALSE)
		{
			// Verify that the game is meant to use arbitration
			if (Session->GameSettings->bUsesArbitration == TRUE)
			{
				// Make sure the game state is pending as registering after that is silly
				if (Session->GameSettings->GameState == OGS_Pending)
				{
					FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
					if (Session->GameSettings->ServerNonce != (QWORD)0)
					{
						DWORD BufferSize = 0;
						// Determine the amount of space needed for arbitration
						Return = XSessionArbitrationRegister(SessionInfo->Handle,
							0,
							Session->GameSettings->ServerNonce,
							&BufferSize,
							NULL,
							NULL);
						if (Return == ERROR_INSUFFICIENT_BUFFER && BufferSize > 0)
						{
							// Async task to parse the results
							FLiveAsyncTaskArbitrationRegistration* AsyncTask = new FLiveAsyncTaskArbitrationRegistration(SessionName,
								BufferSize,
								&ArbitrationRegistrationCompleteDelegates);
							// Now kick off the async task to do the registration
							Return = XSessionArbitrationRegister(SessionInfo->Handle,
								0,
								Session->GameSettings->ServerNonce,
								&BufferSize,
								AsyncTask->GetResults(),
								*AsyncTask);
							debugf(NAME_DevOnline,
								TEXT("XSessionArbitrationRegister(0x%016I64X) '%s' returned 0x%08X"),
								Session->GameSettings->ServerNonce,
								*SessionName.ToString(),
								Return);
							// Clean up the task if it didn't succeed
							if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
							{
								// Queue the async task for ticking
								AsyncTasks.AddItem(AsyncTask);
							}
							else
							{
								delete AsyncTask;
							}
						}
						else
						{
							debugf(NAME_DevOnline,
								TEXT("Failed to determine buffer size for arbitration for session (%s) with 0x%08X"),
								*SessionName.ToString(),
								Return);
						}
					}
					else
					{
						debugf(NAME_DevOnline,
							TEXT("Can't register for arbitration in session (%s) with an invalid ServerNonce"),
							*SessionName.ToString());
					}
				}
				else
				{
					debugf(NAME_DevOnline,
						TEXT("Can't register for arbitration in session (%s) when the game is not pending"),
						*SessionName.ToString());
				}
			}
			else
			{
				debugf(NAME_DevOnline,
					TEXT("Can't register for arbitration for session (%s) on non-arbitrated games"),
					*SessionName.ToString());
			}
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("LAN matches don't use arbitration for session (%s), ignoring call"),
				*SessionName.ToString());
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Can't register for arbitration for session (%s) with a non-existant game"),
			*SessionName.ToString());
	}
	// If there is an error, fire the delegate indicating the error
	if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
	{
		FAsyncTaskDelegateResultsNamedSession Results(SessionName,Return);
		TriggerOnlineDelegates(this,ArbitrationRegistrationCompleteDelegates,&Results);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Tells the online subsystem to accept the game invite that is currently pending
 *
 * @param LocalUserNum the local user accepting the invite
 * @param SessionName the name of the session the invite will be part of
 *
 * @return TRUE if the invite was accepted ok, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::AcceptGameInvite(BYTE LocalUserNum,FName SessionName)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Fail if we don't have an invite pending for this user
	if (InviteCache[LocalUserNum].InviteData != NULL)
	{
		FInviteData& Invite = InviteCache[LocalUserNum];
		// And now we can join the session
		if (JoinOnlineGame(LocalUserNum,SessionName,Invite.InviteSearch->Results(0)) == TRUE)
		{
			Return = ERROR_SUCCESS;
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Failed to join the invite game in session (%s), aborting"),
				*SessionName.ToString());
		}
		// Clean up the invite data
		delete (XSESSION_INFO*)Invite.InviteSearch->Results(0).PlatformData;
		Invite.InviteSearch->Results(0).PlatformData = NULL;
		delete Invite.InviteData;
		// Zero out so we know this invite has been handled
		Invite.InviteData = NULL;
		// Zero the search so it can be GCed
		Invite.InviteSearch = NULL;
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("No invite pending for user at index(%d) for session (%s)"),
			LocalUserNum,
			*SessionName.ToString());
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Registers all local players with the current session
 *
 * @param Session the session that they are registering in
 * @param bIsFromInvite whether this is from an invite or from searching
 */
void UOnlineSubsystemLive::RegisterLocalPlayers(FNamedSession* Session,UBOOL bIsFromInvite)
{
	check(Session && Session->SessionInfo);
	FLiveAsyncTaskDataRegisterLocalPlayers* AsyncTaskData = new FLiveAsyncTaskDataRegisterLocalPlayers();
	// Loop through the 4 available players and register them if they
	// are valid
	for (DWORD Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
	{
		// Ignore non-Live enabled profiles
		if (XUserGetSigninState(Index) != eXUserSigninState_NotSignedIn)
		{
			AsyncTaskData->AddPlayer(Index);
			// Register the local player as a local talker
			RegisterLocalTalker(Index);
			// Add the local player's XUID to the session
			XUID Xuid;
			GetUserXuid(Index,&Xuid);
			FOnlineRegistrant Registrant;
			Registrant.PlayerNetId.Uid = Xuid;
			Session->Registrants.AddUniqueItem(Registrant);
		}
	}
	DWORD PlayerCount = AsyncTaskData->GetCount();
	// This should never happen outside of testing, but check anyway
	if (PlayerCount > 0)
	{
		// If this match is an invite match or there were never any public slots (private only)
		// then join as private
		if (bIsFromInvite || Session->GameSettings->NumPublicConnections == 0)
		{
			// Adjust the number of private slots based upon invites and space
			DWORD NumPrivateToUse = Min<DWORD>(PlayerCount,Session->GameSettings->NumOpenPrivateConnections);
			debugfLiveSlow(NAME_DevOnline,TEXT("Using %d private slots"),NumPrivateToUse);
			AsyncTaskData->SetPrivateSlotsUsed(NumPrivateToUse);
		}
		FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
		// Create a new async task to hold the data
		FOnlineAsyncTaskLive* AsyncTask = new FOnlineAsyncTaskLive(NULL,AsyncTaskData,TEXT("XSessionJoinLocal()"));
		// Now register them as a group, asynchronously
		DWORD Return = XSessionJoinLocal(SessionInfo->Handle,
			AsyncTaskData->GetCount(),
			AsyncTaskData->GetPlayers(),
			AsyncTaskData->GetPrivateSlots(),
			*AsyncTask);
		debugf(NAME_DevOnline,
			TEXT("XSessionJoinLocal() '%s' returned 0x%08X"),
			*Session->SessionName.ToString(),
			Return);
		// Clean up the task if it didn't succeed
		if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
		{
			// Queue the async task for ticking
			AsyncTasks.AddItem(AsyncTask);
		}
		else
		{
			delete AsyncTask;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("No locally signed in players to join game in session (%s)"),
			*Session->SessionName.ToString());
		delete AsyncTaskData;
	}
}

/**
 * Starts an async task that retrieves the list of friends for the player from the
 * online service. The list can be retrieved in whole or in part.
 *
 * @param LocalUserNum the user to read the friends list of
 * @param Count the number of friends to read or zero for all
 * @param StartingAt the index of the friends list to start at (for pulling partial lists)
 *
 * @return true if the read request was issued successfully, false otherwise
 */
UBOOL UOnlineSubsystemLive::ReadFriendsList(BYTE LocalUserNum,INT Count,INT StartingAt)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	UBOOL bFireDelegate = FALSE;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Don't issue another read if one is in progress
		if (FriendsCache[LocalUserNum].ReadState != OERS_InProgress)
		{
			// Throw out the old friends list
			FriendsCache[LocalUserNum].Friends.Empty();
			FriendsCache[LocalUserNum].ReadState = OERS_NotStarted;
			DWORD NumPerRead = MAX_FRIENDS;
			HANDLE Handle;
			DWORD SizeNeeded;
			// Create a new enumerator for the friends list
			Return = XFriendsCreateEnumerator(LocalUserNum,
				StartingAt,
				NumPerRead,
				&SizeNeeded,
				&Handle);
			debugfLiveSlow(NAME_DevOnline,
				TEXT("XFriendsCreateEnumerator(%d,%d,%d,out,out) returned 0x%08X"),
				LocalUserNum,
				StartingAt,
				Count,
				Return);
			if (Return == ERROR_SUCCESS)
			{
				// Create the async data object that holds the buffers, etc.
				FLiveAsyncTaskDataEnumeration* AsyncTaskData = new FLiveAsyncTaskDataEnumeration(LocalUserNum,Handle,SizeNeeded,Count);
				// Create the async task object
				FLiveAsyncTaskReadFriends* AsyncTask = new FLiveAsyncTaskReadFriends(&FriendsCache[LocalUserNum].ReadFriendsDelegates,AsyncTaskData);
				// Start the async read
				Return = XEnumerate(Handle,
					AsyncTaskData->GetBuffer(),
					SizeNeeded,
					0,
					*AsyncTask);
				if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
				{
					// Mark this as being read
					FriendsCache[LocalUserNum].ReadState = OERS_InProgress;
					AsyncTasks.AddItem(AsyncTask);
				}
				else
				{
					bFireDelegate = TRUE;
					// Delete the async task
					delete AsyncTask;
				}
			}
			// Friends list might be empty
			if (Return == ERROR_NO_MORE_FILES)
			{
				bFireDelegate = TRUE;
				Return = ERROR_SUCCESS;
				// Set it to done, since there is nothing to read
				FriendsCache[LocalUserNum].ReadState = OERS_Done;
			}
			else if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
			{
				bFireDelegate = TRUE;
				FriendsCache[LocalUserNum].ReadState = OERS_Failed;
			}
		}
	}
	else
	{
		bFireDelegate = TRUE;
	}
	// Fire off the delegate if needed
	if (bFireDelegate)
	{
		FAsyncTaskDelegateResults Results(Return);
		TriggerOnlineDelegates(this,FriendsCache[LocalUserNum].ReadFriendsDelegates,&Results);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Copies the list of friends for the player previously retrieved from the online
 * service. The list can be retrieved in whole or in part.
 *
 * @param LocalUserNum the user to read the friends list of
 * @param Friends the out array that receives the copied data
 * @param Count the number of friends to read or zero for all
 * @param StartingAt the index of the friends list to start at (for pulling partial lists)
 *
 * @return OERS_Done if the read has completed, otherwise one of the other states
 */
BYTE UOnlineSubsystemLive::GetFriendsList(BYTE LocalUserNum,
	TArray<FOnlineFriend>& Friends,INT Count,INT StartingAt)
{
	BYTE Return = OERS_Failed;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Check to see if the last read request has completed
		Return = FriendsCache[LocalUserNum].ReadState;
		if (Return == OERS_Done)
		{
			// See if they requested all of the data
			if (Count == 0)
			{
				Count = FriendsCache[LocalUserNum].Friends.Num();
			}
			// Presize the out array
			INT AmountToAdd = Min(Count,FriendsCache[LocalUserNum].Friends.Num() - StartingAt);
			Friends.Empty(AmountToAdd);
			Friends.AddZeroed(AmountToAdd);
			// Copy the data from the starting point to the number desired
			for (INT Index = 0;
				Index < Count && (Index + StartingAt) < FriendsCache[LocalUserNum].Friends.Num();
				Index++)
			{
				Friends(Index) = FriendsCache[LocalUserNum].Friends(Index + StartingAt);
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in GetFriendsList(%d)"),
			(DWORD)LocalUserNum);
	}
	return Return;
}

/**
 * Parses the friends results into something the game play code can handle
 *
 * @param PlayerIndex the index of the player that this read was for
 * @param LiveFriends the buffer filled by Live
 * @param NumReturned the number of friends returned by live
 */
void UOnlineSubsystemLive::ParseFriendsResults(DWORD PlayerIndex,PXONLINE_PRESENCE LiveFriends,DWORD NumReturned)
{
	check(PlayerIndex >= 0 && PlayerIndex < MAX_LOCAL_PLAYERS);
	DWORD TitleId = appGetTitleId();
	// Iterate through them all, adding them
	for (DWORD Count = 0; Count < NumReturned; Count++)
	{
		// Skip the update if there is no title id because that means it wasn't read
		if (LiveFriends[Count].dwTitleID != 0)
		{
			// Find the friend being updated
			for (INT FriendIndex = 0; FriendIndex < FriendsCache[PlayerIndex].Friends.Num(); FriendIndex++)
			{
				// Grab access to the friend
				FOnlineFriend& Friend = FriendsCache[PlayerIndex].Friends(FriendIndex);
				// If this is the one we need to update, then do so
				if (Friend.UniqueId.Uid == LiveFriends[Count].xuid)
				{
					// Copy the session info so we can follow them
					Friend.SessionId = (QWORD&)LiveFriends[Count].sessionID;
					// Copy the presence string
					Friend.PresenceInfo = LiveFriends[Count].wszRichPresence;
					// Set booleans based off of Live state
					Friend.bHasVoiceSupport = LiveFriends[Count].dwState & XONLINE_FRIENDSTATE_FLAG_VOICE ? TRUE : FALSE;
#if !WITH_PANORAMA
					Friend.bIsJoinable = (LiveFriends[Count].dwState & XONLINE_FRIENDSTATE_FLAG_JOINABLE ||
						LiveFriends[Count].dwState & XONLINE_FRIENDSTATE_FLAG_JOINABLE_FRIENDS_ONLY) ? TRUE : FALSE;
#else
					Friend.bIsJoinable = LiveFriends[Count].dwState & XONLINE_FRIENDSTATE_FLAG_JOINABLE ? TRUE : FALSE;
#endif
					Friend.bIsOnline = LiveFriends[Count].dwState & XONLINE_FRIENDSTATE_FLAG_ONLINE ? TRUE : FALSE;
					// Update the friend state with the online info and state flags
					if (Friend.bIsOnline)
					{
						if (XOnlineIsUserAway(LiveFriends[Count].dwState))
						{
							Friend.FriendState = OFS_Away;
						}
						else if (XOnlineIsUserBusy(LiveFriends[Count].dwState))
						{
							Friend.FriendState = OFS_Busy;
						}
						else
						{
							Friend.FriendState = OFS_Online;
						}
					}
					else
					{
						Friend.FriendState = OFS_Offline;
					}
					Friend.bIsPlaying = LiveFriends[Count].dwState & XONLINE_FRIENDSTATE_FLAG_PLAYING ? TRUE : FALSE;
					// Check that the title id is the one we expect
					Friend.bIsPlayingThisGame = LiveFriends[Count].dwTitleID == TitleId;
				}
			}
		}
	}
	/** Helper class that sorts friends by online in this game, online, and then offline */
	class FriendSorter
	{
	public:
		static inline INT Compare(const FOnlineFriend& A,const FOnlineFriend& B)
		{
			// Sort by our game first so they are at the top
			INT InGameSort = (INT)B.bIsPlayingThisGame - (INT)A.bIsPlayingThisGame;
			if (InGameSort == 0)
			{
				// Now sort by whether they are online
				INT OnlineSort = (INT)B.bIsOnline - (INT)A.bIsOnline;
				if (OnlineSort == 0)
				{
					// Finally sort by name
					INT NameSort = appStrcmp(*A.NickName,*B.NickName);
					return NameSort;
				}
				return OnlineSort;
			}
			return InGameSort;
		}
	};
	// Now sort the friends
	Sort<FOnlineFriend,FriendSorter>(FriendsCache[PlayerIndex].Friends.GetTypedData(),FriendsCache[PlayerIndex].Friends.Num());
}

/**
 * Parses the friends results into something the game play code can handle
 *
 * @param PlayerIndex the index of the player that this read was for
 * @param LiveFriends the buffer filled by Live
 * @param NumReturned the number of friends returned by live
 */
void UOnlineSubsystemLive::ParseFriendsResults(DWORD PlayerIndex,PXONLINE_FRIEND LiveFriends,DWORD NumReturned)
{
	check(PlayerIndex >= 0 && PlayerIndex < MAX_LOCAL_PLAYERS);
	DWORD TitleId = appGetTitleId();
	// Iterate through them all, adding them
	for (DWORD Count = 0; Count < NumReturned; Count++)
	{
		// Figure out where we are appending our friends to
		INT Offset = FriendsCache[PlayerIndex].Friends.AddZeroed(1);
		// Get the friend we just added
		FOnlineFriend& Friend = FriendsCache[PlayerIndex].Friends(Offset);
		// Copy the session info so we can follow them
		Friend.SessionId = (QWORD&)LiveFriends[Count].sessionID;
		// Copy the name
		Friend.NickName = LiveFriends[Count].szGamertag;
		// Copy the presence string
		Friend.PresenceInfo = LiveFriends[Count].wszRichPresence;
		// Copy the XUID
		Friend.UniqueId.Uid = LiveFriends[Count].xuid;
		// Set booleans based off of Live state
		Friend.bHasVoiceSupport = LiveFriends[Count].dwFriendState & XONLINE_FRIENDSTATE_FLAG_VOICE ? TRUE : FALSE;
#if !WITH_PANORAMA
		Friend.bIsJoinable = (LiveFriends[Count].dwFriendState & XONLINE_FRIENDSTATE_FLAG_JOINABLE ||
			LiveFriends[Count].dwFriendState & XONLINE_FRIENDSTATE_FLAG_JOINABLE_FRIENDS_ONLY) ? TRUE : FALSE;
#else
		Friend.bIsJoinable = LiveFriends[Count].dwFriendState & XONLINE_FRIENDSTATE_FLAG_JOINABLE ? TRUE : FALSE;
#endif
		Friend.bIsOnline = LiveFriends[Count].dwFriendState & XONLINE_FRIENDSTATE_FLAG_ONLINE ? TRUE : FALSE;
		// Update the friend state with the online info and state flags
		if (Friend.bIsOnline)
		{
			if (XOnlineIsUserAway(LiveFriends[Count].dwFriendState))
			{
				Friend.FriendState = OFS_Away;
			}
			else if (XOnlineIsUserBusy(LiveFriends[Count].dwFriendState))
			{
				Friend.FriendState = OFS_Busy;
			}
			else
			{
				Friend.FriendState = OFS_Online;
			}
		}
		else
		{
			Friend.FriendState = OFS_Offline;
		}
		Friend.bIsPlaying = LiveFriends[Count].dwFriendState & XONLINE_FRIENDSTATE_FLAG_PLAYING ? TRUE : FALSE;
		// Check that the title id is the one we expect
		Friend.bIsPlayingThisGame = LiveFriends[Count].dwTitleID == TitleId;
		// Set invite status
		Friend.bHaveInvited = LiveFriends[Count].dwFriendState & XONLINE_FRIENDSTATE_FLAG_SENTINVITE ? TRUE : FALSE;
		Friend.bHasInvitedYou = LiveFriends[Count].dwFriendState & XONLINE_FRIENDSTATE_FLAG_RECEIVEDINVITE ? TRUE : FALSE;
	}
}

/**
 * Starts an async task that retrieves the list of downloaded content for the player.
 *
 * @param LocalUserNum The user to read the content list of
 * @param ContentType the type of content being read
 * @param DeviceId optional value to restrict the enumeration to a particular device
 *
 * @return true if the read request was issued successfully, false otherwise
 */
UBOOL UOnlineSubsystemLive::ReadContentList(BYTE LocalUserNum,BYTE ContentType,INT DeviceId)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	UBOOL bFireDelegate = FALSE;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Mark the right one based off of type
		BYTE& ReadState = ContentType == OCT_Downloaded ? ContentCache[LocalUserNum].ReadState : ContentCache[LocalUserNum].SaveGameReadState;
		if (ReadState != OERS_InProgress)
		{
			// If this is a save game task, make sure none of them are in progress
			if (ContentType == OCT_Downloaded ||
				AreAnySaveGamesInProgress(LocalUserNum) == FALSE)
			{
				// Throw out the old content
				ClearContentList(LocalUserNum,ContentType);
#if CONSOLE
				// if the user is logging in, search for any DLC
				DWORD SizeNeeded;
				HANDLE Handle;
				// return 1 at a time per XEnumerate call
				DWORD NumToRead = 1;
				// Use the specified device id if it is valid, otherwise default to any device
				XCONTENTDEVICEID XDeviceId = IsDeviceValid(DeviceId) ? DeviceId : XCONTENTDEVICE_ANY;
				// start looking for this user's content
				Return = XContentCreateEnumerator(LocalUserNum,
					XDeviceId,
					ContentType == OCT_Downloaded ? XCONTENTTYPE_MARKETPLACE : XCONTENTTYPE_SAVEDGAME, 
					0,
					NumToRead,
					&SizeNeeded,
					&Handle);
				// make sure we succeeded
				if (Return == ERROR_SUCCESS)
				{
					// Create the async data object that holds the buffers, etc (using 0 for number to retrieve for all)
					FLiveAsyncTaskContent* AsyncTaskData = new FLiveAsyncTaskContent(LocalUserNum,
						Handle,
						SizeNeeded,
						0,
						ContentType);
					// Create the async task object
					FLiveAsyncTaskReadContent* AsyncTask = new FLiveAsyncTaskReadContent(
						ContentType == OCT_Downloaded ? &ContentCache[LocalUserNum].ReadCompleteDelegates : &ContentCache[LocalUserNum].SaveGameReadCompleteDelegates,
						AsyncTaskData);
					// Start the async read
					Return = XEnumerate(Handle, AsyncTaskData->GetBuffer(), SizeNeeded, 0, *AsyncTask);
					if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
					{
						// Mark this as being read
						ReadState = OERS_InProgress;
						AsyncTasks.AddItem(AsyncTask);
					}
					else
					{
						bFireDelegate = TRUE;
						// Delete the async task
						delete AsyncTask;
					}
				}
				else
				{
					debugf(NAME_DevOnline,TEXT("XContentCreateEnumerator(%d, XCONTENTDEVICE_ANY, XCONTENTTYPE_MARKETPLACE, 0, 1, &BufferSize, &EnumerateHandle) failed with 0x%08X"),
						LocalUserNum, Return);
				}
				// Content list might be empty
				if (Return == ERROR_NO_MORE_FILES)
				{
					bFireDelegate = TRUE;
					Return = ERROR_SUCCESS;
					// Set it to done, since there is nothing to read
					ReadState = OERS_Done;
				}
				else if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
				{
					bFireDelegate = TRUE;
					ReadState = OERS_Failed;
				}
#endif
			}
			else
			{
				debugf(NAME_DevOnline,
					TEXT("ReadContentList(%d,%d) failed since a save game read/write is in progress"),
					(DWORD)LocalUserNum,
					(DWORD)ContentType);
				bFireDelegate = TRUE;
				ReadState = OERS_Failed;
			}
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Ignoring call to ReadContentList(%d,%d) since one is already in progress"),
				(DWORD)LocalUserNum,
				(DWORD)ContentType);
			Return = ERROR_IO_PENDING;
		}
		// Fire off the delegate if needed
		if (bFireDelegate)
		{
			FAsyncTaskDelegateResults Results(Return);
			TriggerOnlineDelegates(this,
				ContentType == OCT_Downloaded ? ContentCache[LocalUserNum].ReadCompleteDelegates : ContentCache[LocalUserNum].SaveGameReadCompleteDelegates,
				&Results);
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in ReadContentList(%d)"),
			(DWORD)LocalUserNum);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Starts an async task that frees any downloaded content resources for that player
 *
 * @param LocalUserNum The user to clear the content list for
 * @param ContentType the type of content being read
 */
void UOnlineSubsystemLive::ClearContentList(BYTE LocalUserNum,BYTE ContentType)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		if (ContentType == OCT_Downloaded)
		{
#if CONSOLE
			// Close all opened content bundles
			for (INT DlcIndex = 0; DlcIndex < ContentCache[LocalUserNum].Content.Num(); DlcIndex++)
			{
				FOnlineAsyncTaskLive* Task = new FOnlineAsyncTaskLive(NULL,NULL,TEXT("DLC XContentClose"));
				DWORD Result = XContentClose(TCHAR_TO_ANSI(*ContentCache[LocalUserNum].Content(DlcIndex).ContentPath),*Task);
				debugf(NAME_DevOnline,TEXT("Freeing DLC (%s) returned 0x%08X"),*ContentCache[LocalUserNum].Content(DlcIndex).FriendlyName,Result);
				AsyncTasks.AddItem(Task);
			}
#endif
			// Can't read DLC on PC, so empty out and mark as not started
			ContentCache[LocalUserNum].Content.Empty();
			ContentCache[LocalUserNum].ReadState = OERS_NotStarted;
		}
		else
		{
#if CONSOLE
			// Close all opened content bundles
			for (INT SaveGameIndex = 0; SaveGameIndex < ContentCache[LocalUserNum].SaveGameContent.Num(); SaveGameIndex++)
			{
				FOnlineAsyncTaskLive* Task = new FOnlineAsyncTaskLive(NULL,NULL,TEXT("SaveGame XContentClose"));
				DWORD Result = XContentClose(TCHAR_TO_ANSI(*ContentCache[LocalUserNum].SaveGameContent(SaveGameIndex).ContentPath),*Task);
				debugf(NAME_DevOnline,TEXT("Freeing SaveGame (%s) returned 0x%08X"),*ContentCache[LocalUserNum].SaveGameContent(SaveGameIndex).FriendlyName,Result);
				AsyncTasks.AddItem(Task);
			}
#endif
			ContentCache[LocalUserNum].SaveGameContent.Empty();
			ContentCache[LocalUserNum].SaveGameReadState = OERS_NotStarted;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in ClearContentList(%d)"),
			(DWORD)LocalUserNum);
	}
}

/**
 * Retrieve the list of content the given user has downloaded or otherwise retrieved
 * to the local console.
 *
 * @param LocalUserNum The user to read the content list of
 * @param ContentType the type of content being read
 * @param ContentList The out array that receives the list of all content
 */
BYTE UOnlineSubsystemLive::GetContentList(BYTE LocalUserNum,BYTE ContentType,TArray<FOnlineContent>& ContentList)
{
	BYTE Return = OERS_Failed;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		if (ContentType == OCT_Downloaded)
		{
			// Check to see if the last DLC read request has completed
			Return = ContentCache[LocalUserNum].ReadState;
			if (Return == OERS_Done)
			{
				ContentList = ContentCache[LocalUserNum].Content;
			}
		}
		else
		{
			// Check to see if the last save game read request has completed
			Return = ContentCache[LocalUserNum].SaveGameReadState;
			if (Return == OERS_Done)
			{
				ContentList = ContentCache[LocalUserNum].SaveGameContent;
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in GetContentList(%d)"),
			(DWORD)LocalUserNum);
	}
	return Return;
}

/**
 * Asks the online system for the number of new and total content downloads
 *
 * @param LocalUserNum the user to check the content download availability for
 * @param CategoryMask the bitmask to use to filter content by type
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::QueryAvailableDownloads(BYTE LocalUserNum,INT CategoryMask)
{
//@todo  joeg -- Determine if Panorama is going to support this and then re-hook up
#if CONSOLE
	DWORD Return = E_INVALIDARG;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Create the async task and data objects
		FLiveAsyncTaskDataQueryDownloads* AsyncData = new FLiveAsyncTaskDataQueryDownloads(LocalUserNum);
		FLiveAsyncTaskQueryDownloads* AsyncTask = new FLiveAsyncTaskQueryDownloads(&ContentCache[LocalUserNum].QueryDownloadsDelegates,AsyncData);
		// Do an async query for the content counts
		Return = XContentGetMarketplaceCounts(LocalUserNum,
			(DWORD)CategoryMask,
			sizeof(XOFFERING_CONTENTAVAILABLE_RESULT),
			AsyncData->GetQuery(),
			*AsyncTask);
		debugfLiveSlow(NAME_DevOnline,TEXT("XContentGetMarketplaceCounts(%d,0x%08X) returned 0x%08X"),
			(DWORD)LocalUserNum,
			CategoryMask,
			Return);
		// Clean up the task if it didn't succeed
		if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
		{
			// Queue the async task for ticking
			AsyncTasks.AddItem(AsyncTask);
		}
		else
		{
			// Just trigger the delegate as having failed
			FAsyncTaskDelegateResults Results(Return);
			TriggerOnlineDelegates(this,ContentCache[LocalUserNum].QueryDownloadsDelegates,&Results);
			delete AsyncTask;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in QueryAvailableDownloads(%d)"),
			(DWORD)LocalUserNum);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
#else
	return FALSE;
#endif
}

/**
 * Registers the user as a talker
 *
 * @param LocalUserNum the local player index that is a talker
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::RegisterLocalTalker(BYTE LocalUserNum)
{
	DWORD Return = E_FAIL;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Get at the local talker's cached data
		FLocalTalker& Talker = LocalTalkers[LocalUserNum];
		// Make local user capable of sending voice data
		StartNetworkedVoice(LocalUserNum);
		// Don't register non-Live enabled accounts
		if (XUserGetSigninState(LocalUserNum) != eXUserSigninState_NotSignedIn &&
			// Or register talkers when voice is disabled
			VoiceEngine != NULL)
		{
			if (Talker.bIsRegistered == FALSE)
			{
				// Register the talker locally
				Return = VoiceEngine->RegisterLocalTalker(LocalUserNum);
				debugfLiveSlow(NAME_DevOnline,TEXT("RegisterLocalTalker(%d) returned 0x%08X"),
					LocalUserNum,Return);
				if (Return == S_OK)
				{
					Talker.bHasVoice = TRUE;
					Talker.bIsRegistered = TRUE;
					// Kick off the processing mode
					Return = VoiceEngine->StartLocalVoiceProcessing(LocalUserNum);
					debugfLiveSlow(NAME_DevOnline,TEXT("StartLocalProcessing(%d) returned 0x%08X"),
						(DWORD)LocalUserNum,Return);
				}
			}
			else
			{
				// Just say yes, we registered fine
				Return = S_OK;
			}
			APlayerController* PlayerController = GetPlayerControllerFromUserIndex(LocalUserNum);
			if (PlayerController != NULL)
			{
				// Update the muting information for this local talker
				UpdateMuteListForLocalTalker(LocalUserNum,PlayerController);
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Can't update mute list for %d due to:"),LocalUserNum);
				debugf(NAME_DevOnline,TEXT("Failed to find player controller for %d"),LocalUserNum);
			}
		}
		else
		{
			// Not properly logged in, so skip voice for them
			Talker.bHasVoice = FALSE;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in RegisterLocalTalker(%d)"),
			(DWORD)LocalUserNum);
	}
	return Return == S_OK;
}

/**
 * Unregisters the user as a talker
 *
 * @param LocalUserNum the local player index to be removed
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::UnregisterLocalTalker(BYTE LocalUserNum)
{
	DWORD Return = S_OK;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Get at the local talker's cached data
		FLocalTalker& Talker = LocalTalkers[LocalUserNum];
		// Skip the unregistration if not registered
		if (Talker.bHasVoice == TRUE &&
			// Or when voice is disabled
			VoiceEngine != NULL)
		{
			// Remove them from XHV too
			Return = VoiceEngine->UnregisterLocalTalker(LocalUserNum);
			debugfLiveSlow(NAME_DevOnline,TEXT("UnregisterLocalTalker(%d) returned 0x%08X"),
				LocalUserNum,Return);
			Talker.bHasVoice = FALSE;
			Talker.bIsRegistered = FALSE;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in UnregisterLocalTalker(%d)"),
			(DWORD)LocalUserNum);
	}
	return Return == S_OK;
}

/**
 * Registers a remote player as a talker
 *
 * @param UniqueId the unique id of the remote player that is a talker
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::RegisterRemoteTalker(FUniqueNetId UniqueId)
{
	DWORD Return = E_FAIL;
	// Skip this if the session isn't active
	if (Sessions.Num() &&
		// Or when voice is disabled
		VoiceEngine != NULL)
	{
		// See if this talker has already been registered or not
		FLiveRemoteTalker* Talker = FindRemoteTalker(UniqueId);
		if (Talker == NULL)
		{
			// Add a new talker to our list
			INT AddIndex = RemoteTalkers.AddZeroed();
			Talker = &RemoteTalkers(AddIndex);
			// Copy the XUID
			(XUID&)Talker->TalkerId = (XUID&)UniqueId;
			// Register the remote talker locally
			Return = VoiceEngine->RegisterRemoteTalker(UniqueId);
			debugfLiveSlow(NAME_DevOnline,TEXT("RegisterRemoteTalker(0x%016I64X) returned 0x%08X"),
				(XUID&)UniqueId,Return);
		}
		else
		{
			debugfLiveSlow(NAME_DevOnline,TEXT("Remote talker 0x%016I64X is being re-registered"),(XUID&)UniqueId);
			Return = S_OK;
		}
		// Update muting all of the local talkers with this remote talker
		ProcessMuteChangeNotification();
		// Now start processing the remote voices
		Return = VoiceEngine->StartRemoteVoiceProcessing(UniqueId);
		debugfLiveSlow(NAME_DevOnline,TEXT("StartRemoteVoiceProcessing(0x%016I64X) returned 0x%08X"),
			(XUID&)UniqueId,Return);
	}
	return Return == S_OK;
}

/**
 * Unregisters a remote player as a talker
 *
 * @param UniqueId the unique id of the remote player to be removed
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::UnregisterRemoteTalker(FUniqueNetId UniqueId)
{
	DWORD Return = E_FAIL;
	// Skip this if the session isn't active
	if (Sessions.Num() &&
		// Or when voice is disabled
		VoiceEngine != NULL)
	{
		// Make sure the talker is valid
		if (FindRemoteTalker(UniqueId) != NULL)
		{
			// Find them in the talkers array and remove them
			for (INT Index = 0; Index < RemoteTalkers.Num(); Index++)
			{
				const FLiveRemoteTalker& Talker = RemoteTalkers(Index);
				// Is this the remote talker?
				if ((XUID&)Talker.TalkerId == (XUID&)UniqueId)
				{
					RemoteTalkers.Remove(Index);
					break;
				}
			}
			// Remove them from XHV too
			Return = VoiceEngine->UnregisterRemoteTalker(UniqueId);
			debugfLiveSlow(NAME_DevOnline,TEXT("UnregisterRemoteTalker(0x%016I64X) returned 0x%08X"),
				(XUID&)UniqueId,Return);
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Unknown remote talker (0x%016I64X) specified to UnregisterRemoteTalker()"),
				(QWORD&)UniqueId);
								  
		}
	}
	return Return == S_OK;
}

/**
 * Determines if the specified player is actively talking into the mic
 *
 * @param LocalUserNum the local player index being queried
 *
 * @return TRUE if the player is talking, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::IsLocalPlayerTalking(BYTE LocalUserNum)
{
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		return VoiceEngine != NULL && VoiceEngine->IsLocalPlayerTalking(LocalUserNum);
	}
	return FALSE;
}

/**
 * Determines if the specified remote player is actively talking into the mic
 * NOTE: Network latencies will make this not 100% accurate
 *
 * @param UniqueId the unique id of the remote player being queried
 *
 * @return TRUE if the player is talking, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::IsRemotePlayerTalking(FUniqueNetId UniqueId)
{
	return Sessions.Num() && VoiceEngine != NULL && VoiceEngine->IsRemotePlayerTalking(UniqueId);
}

/**
 * Determines if the specified player has a headset connected
 *
 * @param LocalUserNum the local player index being queried
 *
 * @return TRUE if the player has a headset plugged in, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::IsHeadsetPresent(BYTE LocalUserNum)
{
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		return VoiceEngine != NULL && VoiceEngine->IsHeadsetPresent(LocalUserNum);
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in IsHeadsetPresent(%d)"),
			(DWORD)LocalUserNum);
	}
	return FALSE;
}

/**
 * Sets the relative priority for a remote talker. 0 is highest
 *
 * @param LocalUserNum the user that controls the relative priority
 * @param UniqueId the remote talker that is having their priority changed for
 * @param Priority the relative priority to use (0 highest, < 0 is muted)
 *
 * @return TRUE if the function succeeds, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::SetRemoteTalkerPriority(BYTE LocalUserNum,FUniqueNetId UniqueId,INT Priority)
{
	DWORD Return = E_INVALIDARG;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Skip this if the session isn't active
		if (Sessions.Num() &&
			// Or if voice is disabled
			VoiceEngine != NULL)
		{
			// Find the remote talker to modify
			FLiveRemoteTalker* Talker = FindRemoteTalker(UniqueId);
			if (Talker != NULL)
			{
				// Cache the old and set the new current priority
				Talker->LocalPriorities[LocalUserNum].LastPriority = Talker->LocalPriorities[LocalUserNum].CurrentPriority;
				Talker->LocalPriorities[LocalUserNum].CurrentPriority = Priority;
				Return = VoiceEngine->SetPlaybackPriority(LocalUserNum,UniqueId,
					(XHV_PLAYBACK_PRIORITY)Priority);
				debugfLiveSlow(NAME_DevOnline,TEXT("SetPlaybackPriority(%d,0x%016I64X,%d) return 0x%08X"),
					LocalUserNum,(XUID&)UniqueId,Priority,Return);
			}
			else
			{
				debugf(NAME_DevOnline,
					TEXT("Unknown remote talker (0x%016I64X) specified to SetRemoteTalkerPriority()"),
					(QWORD&)UniqueId);
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in SetRemoteTalkerPriority(%d)"),
			(DWORD)LocalUserNum);
	}
	return Return == S_OK;
}

/**
 * Mutes a remote talker for the specified local player. NOTE: This only mutes them in the
 * game unless the bIsSystemWide flag is true, which attempts to mute them globally
 *
 * @param LocalUserNum the user that is muting the remote talker
 * @param PlayerId the remote talker that is being muted
 * @param bIsSystemWide whether to try to mute them globally or not
 *
 * @return TRUE if the function succeeds, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::MuteRemoteTalker(BYTE LocalUserNum,FUniqueNetId UniqueId,UBOOL bIsSystemWide)
{
	DWORD Return = E_INVALIDARG;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
#if !WITH_PANORAMA
		// Handle a system wide request first
		if (bIsSystemWide)
		{
			// Tell Live to mute them which will trigger a mute change notification so we don't need to manually mute them
			Return = XUserMuteListSetState(LocalUserNum,(XUID&)UniqueId,TRUE);
			debugf(NAME_DevOnline,
				TEXT("XUserMuteListSetState(%d,0x%016I64X,TRUE) returned 0x%08X"),
				LocalUserNum,
				(XUID&)UniqueId,
				Return);
		}
		else
#endif
		{
			// Skip this if the session isn't active
			if (Sessions.Num() &&
				// Or if voice is disabled
				VoiceEngine != NULL)
			{
				// Find the specified talker
				FLiveRemoteTalker* Talker = FindRemoteTalker(UniqueId);
				if (Talker != NULL)
				{
					// This is the talker in question, so cache the last priority
					// and change the current to muted
					Talker->LocalPriorities[LocalUserNum].LastPriority = Talker->LocalPriorities[LocalUserNum].CurrentPriority;
					Talker->LocalPriorities[LocalUserNum].CurrentPriority = XHV_PLAYBACK_PRIORITY_NEVER;
					// Set their priority to never
					Return = VoiceEngine->SetPlaybackPriority(LocalUserNum,UniqueId,XHV_PLAYBACK_PRIORITY_NEVER);
					debugfLiveSlow(NAME_DevOnline,TEXT("SetPlaybackPriority(%d,0x%016I64X,-1) return 0x%08X"),
						LocalUserNum,(XUID&)UniqueId,Return);
				}
				else
				{
					debugf(NAME_DevOnline,TEXT("Unknown remote talker (0x%016I64X) specified to MuteRemoteTalker()"),
						(QWORD&)UniqueId);
				}
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in MuteRemoteTalker(%d)"),
			(DWORD)LocalUserNum);
	}
	return Return == S_OK;
}

/**
 * Allows a remote talker to talk to the specified local player. NOTE: This only unmutes them in the
 * game unless the bIsSystemWide flag is true, which attempts to unmute them globally
 *
 * @param LocalUserNum the user that is allowing the remote talker to talk
 * @param PlayerId the remote talker that is being restored to talking
 * @param bIsSystemWide whether to try to unmute them globally or not
 *
 * @return TRUE if the function succeeds, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::UnmuteRemoteTalker(BYTE LocalUserNum,FUniqueNetId UniqueId,UBOOL bIsSystemWide)
{
	DWORD Return = E_INVALIDARG;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
#if !WITH_PANORAMA
		// Handle a system wide request first
		if (bIsSystemWide)
		{
			// Tell Live to unmute them which will trigger a mute change notification so we don't need to manually unmute them
			Return = XUserMuteListSetState(LocalUserNum,(XUID&)UniqueId,FALSE);
			debugf(NAME_DevOnline,
				TEXT("XUserMuteListSetState(%d,0x%016I64X,FALSE) returned 0x%08X"),
				LocalUserNum,
				(XUID&)UniqueId,
				Return);
		}
		else
#endif
		{
			// Skip this if the session isn't active
			if (Sessions.Num() &&
				// Or if voice is disabled
				VoiceEngine != NULL)
			{
				// Find the specified talker
				FLiveRemoteTalker* Talker = FindRemoteTalker(UniqueId);
				if (Talker != NULL)
				{
					INT bIsMuted = FALSE;
					// Verify that this talker isn't on the mute list
					XUserMuteListQuery(LocalUserNum,(XUID&)UniqueId,&bIsMuted);
					// Only restore their priority if they aren't muted
					if (bIsMuted == FALSE)
					{
						Talker->LocalPriorities[LocalUserNum].LastPriority = Talker->LocalPriorities[LocalUserNum].CurrentPriority;
						Talker->LocalPriorities[LocalUserNum].CurrentPriority = XHV_PLAYBACK_PRIORITY_MAX;
						// Don't unmute if any player on this console has them muted
						if (Talker->IsLocallyMuted() == FALSE)
						{
							// Set their priority to unmuted
							Return = VoiceEngine->SetPlaybackPriority(LocalUserNum,UniqueId,
								Talker->LocalPriorities[LocalUserNum].CurrentPriority);
							debugfLiveSlow(NAME_DevOnline,TEXT("SetPlaybackPriority(%d,0x%016I64X,%d) returned 0x%08X"),
								LocalUserNum,(XUID&)UniqueId,
								Talker->LocalPriorities[LocalUserNum].CurrentPriority,Return);
						}
					}
				}
				else
				{
					debugf(NAME_DevOnline,TEXT("Unknown remote talker (0x%016I64X) specified to UnmuteRemoteTalker()"),
						(QWORD&)UniqueId);
				}											
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in UnmuteRemoteTalker(%d)"),
			(DWORD)LocalUserNum);
	}
	return Return == S_OK;
}

/**
 * Tells the voice layer that networked processing of the voice data is allowed
 * for the specified player. This allows for push-to-talk style voice communication
 *
 * @param LocalUserNum the local user to allow network transimission for
 */
void UOnlineSubsystemLive::StartNetworkedVoice(BYTE LocalUserNum)
{
	// Validate the range of the entry
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		LocalTalkers[LocalUserNum].bHasNetworkedVoice = TRUE;
		debugfLiveSlow(NAME_DevOnline,TEXT("Starting networked voice for %d"),LocalUserNum);
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid user specified in StartNetworkedVoice(%d)"),
			(DWORD)LocalUserNum);
	}
}

/**
 * Tells the voice layer to stop processing networked voice support for the
 * specified player. This allows for push-to-talk style voice communication
 *
 * @param LocalUserNum the local user to disallow network transimission for
 */
void UOnlineSubsystemLive::StopNetworkedVoice(BYTE LocalUserNum)
{
	// Validate the range of the entry
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		LocalTalkers[LocalUserNum].bHasNetworkedVoice = FALSE;
		debugfLiveSlow(NAME_DevOnline,TEXT("Stopping networked voice for %d"),LocalUserNum);
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid user specified in StopNetworkedVoice(%d)"),
			(DWORD)LocalUserNum);
	}
}

/**
 * Tells the voice system to start tracking voice data for speech recognition
 *
 * @param LocalUserNum the local user to recognize voice data for
 *
 * @return true upon success, false otherwise
 */
UBOOL UOnlineSubsystemLive::StartSpeechRecognition(BYTE LocalUserNum)
{
	HRESULT Return = E_FAIL;
	if (bIsUsingSpeechRecognition && VoiceEngine != NULL)
	{
		Return = VoiceEngine->StartSpeechRecognition(LocalUserNum);
		debugfLiveSlow(NAME_DevOnline,TEXT("StartSpeechRecognition(%d) returned 0x%08X"),
			LocalUserNum,Return);
	}
	return SUCCEEDED(Return);
}

/**
 * Tells the voice system to stop tracking voice data for speech recognition
 *
 * @param LocalUserNum the local user to recognize voice data for
 *
 * @return true upon success, false otherwise
 */
UBOOL UOnlineSubsystemLive::StopSpeechRecognition(BYTE LocalUserNum)
{
	HRESULT Return = E_FAIL;
	if (bIsUsingSpeechRecognition && VoiceEngine != NULL)
	{
		Return = VoiceEngine->StopSpeechRecognition(LocalUserNum);
		debugfLiveSlow(NAME_DevOnline,TEXT("StopSpeechRecognition(%d) returned 0x%08X"),
			LocalUserNum,Return);
	}
	return SUCCEEDED(Return);
}

/**
 * Gets the results of the voice recognition
 *
 * @param LocalUserNum the local user to read the results of
 * @param Words the set of words that were recognized by the voice analyzer
 *
 * @return true upon success, false otherwise
 */
UBOOL UOnlineSubsystemLive::GetRecognitionResults(BYTE LocalUserNum,TArray<FSpeechRecognizedWord>& Words)
{
	HRESULT Return = E_FAIL;
	if (bIsUsingSpeechRecognition && VoiceEngine != NULL)
	{
		Return = VoiceEngine->GetRecognitionResults(LocalUserNum,Words);
		debugfLiveSlow(NAME_DevOnline,TEXT("GetRecognitionResults(%d,Array) returned 0x%08X"),
			LocalUserNum,Return);
	}
	return SUCCEEDED(Return);
}

/**
 * Changes the vocabulary id that is currently being used
 *
 * @param LocalUserNum the local user that is making the change
 * @param VocabularyId the new id to use
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemLive::SelectVocabulary(BYTE LocalUserNum,INT VocabularyId)
{
	HRESULT Return = E_FAIL;
	if (bIsUsingSpeechRecognition && VoiceEngine != NULL)
	{
		Return = VoiceEngine->SelectVocabulary(LocalUserNum,VocabularyId);
		debugfLiveSlow(NAME_DevOnline,TEXT("SelectVocabulary(%d,%d) returned 0x%08X"),
			LocalUserNum,VocabularyId,Return);
	}
	return SUCCEEDED(Return);
}

/**
 * Changes the object that is in use to the one specified
 *
 * @param LocalUserNum the local user that is making the change
 * @param SpeechRecogObj the new object use
 *
 * @param true if successful, false otherwise
 */
UBOOL UOnlineSubsystemLive::SetSpeechRecognitionObject(BYTE LocalUserNum,USpeechRecognition* SpeechRecogObj)
{
	HRESULT Return = E_FAIL;
	if (bIsUsingSpeechRecognition && VoiceEngine != NULL)
	{
		Return = VoiceEngine->SetRecognitionObject(LocalUserNum,SpeechRecogObj);
		debugfLiveSlow(NAME_DevOnline,TEXT("SetRecognitionObject(%d,%s) returned 0x%08X"),
			LocalUserNum,SpeechRecogObj ? *SpeechRecogObj->GetName() : TEXT("NULL"),Return);
	}
	return SUCCEEDED(Return);
}

/**
 * Re-evaluates the muting list for all local talkers
 */
void UOnlineSubsystemLive::ProcessMuteChangeNotification(void)
{
	// Nothing to update if there isn't an active session
	if (Sessions.Num() && VoiceEngine != NULL)
	{
		// For each local user with voice
		for (INT Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
		{
			APlayerController* PlayerController = GetPlayerControllerFromUserIndex(Index);
			// If there is a player controller, we can mute/unmute people
			if (LocalTalkers[Index].bHasVoice && PlayerController != NULL)
			{
				// Use the common method of checking muting
				UpdateMuteListForLocalTalker(Index,PlayerController);
			}
		}
	}
}

/**
 * Figures out which remote talkers need to be muted for a given local talker
 *
 * @param TalkerIndex the talker that needs the mute list checked for
 * @param PlayerController the player controller associated with this talker
 */
void UOnlineSubsystemLive::UpdateMuteListForLocalTalker(INT TalkerIndex,
	APlayerController* PlayerController)
{
	// For each registered remote talker
	for (INT RemoteIndex = 0; RemoteIndex < RemoteTalkers.Num(); RemoteIndex++)
	{
		FLiveRemoteTalker& Talker = RemoteTalkers(RemoteIndex);
		INT bIsMuted = FALSE;
		// Is the remote talker on this local player's mute list?
		XUserMuteListQuery(TalkerIndex,(XUID&)Talker.TalkerId,&bIsMuted);
		// Figure out which priority to use now
		if (bIsMuted == FALSE)
		{
			// If they were previously muted, set them to zero priority
			if (Talker.LocalPriorities[TalkerIndex].CurrentPriority == XHV_PLAYBACK_PRIORITY_NEVER)
			{
				Talker.LocalPriorities[TalkerIndex].LastPriority = Talker.LocalPriorities[TalkerIndex].CurrentPriority;
				Talker.LocalPriorities[TalkerIndex].CurrentPriority = XHV_PLAYBACK_PRIORITY_MAX;
				// Unmute on the server
				PlayerController->eventServerUnmutePlayer(Talker.TalkerId);
			}
			else
			{
				// Use their current priority without changes
			}
		}
		else
		{
			// Mute this remote talker
			Talker.LocalPriorities[TalkerIndex].LastPriority = Talker.LocalPriorities[TalkerIndex].CurrentPriority;
			Talker.LocalPriorities[TalkerIndex].CurrentPriority = XHV_PLAYBACK_PRIORITY_NEVER;
			// Mute on the server
			PlayerController->eventServerMutePlayer(Talker.TalkerId);
		}
		// The ServerUn/MutePlayer() functions will perform the muting based
		// upon gameplay settings and other player's mute list
	}
}

/**
 * Registers/unregisters local talkers based upon login changes
 */
void UOnlineSubsystemLive::UpdateVoiceFromLoginChange(void)
{
	// Nothing to update if there isn't an active session
	if (Sessions.Num())
	{
		// Check each user index for a sign in change
		for (INT Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
		{
			XUSER_SIGNIN_STATE SignInState = XUserGetSigninState(Index);
			// Was this player registered last time around but is no longer signed in
			if (LocalTalkers[Index].bHasVoice == TRUE &&
				SignInState == eXUserSigninState_NotSignedIn)
			{
				UnregisterLocalTalker(Index);
			}
			// Was this player not registered, but now is logged in
			else if (LocalTalkers[Index].bHasVoice == FALSE &&
				SignInState != eXUserSigninState_NotSignedIn)
			{
				RegisterLocalTalker(Index);
			}
			else
			{
				// Logged in and registered, so do nothing
			}
		}
	}
}

/**
 * Iterates the current remote talker list unregistering them with XHV
 * and our internal state
 */
void UOnlineSubsystemLive::RemoveAllRemoteTalkers(void)
{
	debugfLiveSlow(NAME_DevOnline,TEXT("Removing all remote talkers"));
	if (VoiceEngine != NULL)
	{
		// Work backwards through array removing the talkers
		for (INT Index = RemoteTalkers.Num() - 1; Index >= 0; Index--)
		{
			const FLiveRemoteTalker& Talker = RemoteTalkers(Index);
			// Remove them from XHV
			DWORD Return = VoiceEngine->UnregisterRemoteTalker(Talker.TalkerId);
			debugfLiveSlow(NAME_DevOnline,TEXT("UnregisterRemoteTalker(0x%016I64X) returned 0x%08X"),
				(XUID&)Talker.TalkerId,Return);
		}
	}
	// Empty the array now that they are all unregistered
	RemoteTalkers.Empty();
}

/**
 * Registers all signed in local talkers
 */
void UOnlineSubsystemLive::RegisterLocalTalkers(void)
{
	debugfLiveSlow(NAME_DevOnline,TEXT("Registering all local talkers"));
	// Loop through the 4 available players and register them
	for (DWORD Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
	{
		// Register the local player as a local talker
		RegisterLocalTalker(Index);
	}
}

/**
 * Unregisters all signed in local talkers
 */
void UOnlineSubsystemLive::UnregisterLocalTalkers(void)
{
	debugfLiveSlow(NAME_DevOnline,TEXT("Unregistering all local talkers"));
	// Loop through the 4 available players and unregister them
	for (DWORD Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
	{
		// Unregister the local player as a local talker
		UnregisterLocalTalker(Index);
	}
}

/**
 * Parses the read results and copies them to the stats read object
 *
 * @param ReadResults the data to add to the stats object
 */
void UOnlineSubsystemLive::ParseStatsReadResults(XUSER_STATS_READ_RESULTS* ReadResults)
{
	check(CurrentStatsRead && ReadResults);
	// Copy over the view's info
	CurrentStatsRead->TotalRowsInView = ReadResults->pViews->dwTotalViewRows;
	CurrentStatsRead->ViewId = ReadResults->pViews->dwViewId;
	// Now copy each row that was returned
	for (DWORD RowIndex = 0; RowIndex < ReadResults->pViews->dwNumRows; RowIndex++)
	{
		INT NewIndex = CurrentStatsRead->Rows.AddZeroed();
		FOnlineStatsRow& Row = CurrentStatsRead->Rows(NewIndex);
		const XUSER_STATS_ROW& XRow = ReadResults->pViews->pRows[RowIndex];
		// Copy the row data over
		Row.NickName = XRow.szGamertag;
		(XUID&)Row.PlayerID = XRow.xuid;
		// See if they are ranked on the leaderboard or not
		if (XRow.dwRank > 0)
		{
			Row.Rank.SetData((INT)XRow.dwRank);
		}
		else
		{
			Row.Rank.SetData(TEXT("--"));
		}
		// Now allocate our columns
		Row.Columns.Empty(XRow.dwNumColumns);
		Row.Columns.AddZeroed(XRow.dwNumColumns);
		// And copy the columns
		for (DWORD ColIndex = 0; ColIndex < XRow.dwNumColumns; ColIndex++)
		{
			FOnlineStatsColumn& Col = Row.Columns(ColIndex);
			const XUSER_STATS_COLUMN& XCol = XRow.pColumns[ColIndex];
			// Copy the column id and the data object
			Col.ColumnNo = XCol.wColumnId;
			CopyXDataToSettingsData(Col.StatValue,XCol.Value);
			// Handle Live sending "empty" values when they should be zeroed
			if (Col.StatValue.Type == SDT_Empty)
			{
				Col.StatValue.SetData(TEXT("--"));
			}
		}
	}
}

/**
 * Builds the data that we want to read into the Live specific format. Live
 * uses WORDs which script doesn't support, so we can't map directly to it
 *
 * @param DestSpecs the destination stat specs to fill in
 * @param ViewId the view id that is to be used
 * @param Columns the columns that are being requested
 */
void UOnlineSubsystemLive::BuildStatsSpecs(XUSER_STATS_SPEC* DestSpecs,INT ViewId,
	const TArrayNoInit<INT>& Columns)
{
	debugfLiveSlow(NAME_DevOnline,TEXT("ViewId = %d, NumCols = %d"),ViewId,Columns.Num());
	// Copy the view data over
	DestSpecs->dwViewId = ViewId;
	DestSpecs->dwNumColumnIds = Columns.Num();
	// Iterate through the columns and copy those over
	// NOTE: These are different types so we can't just memcpy
	for (INT Index = 0; Index < Columns.Num(); Index++)
	{
		DestSpecs->rgwColumnIds[Index] = (WORD)Columns(Index);
		if (bShouldLogStatsData)
		{
			debugfLiveSlow(NAME_DevOnline,TEXT("rgwColumnIds[%d] = %d"),Index,Columns(Index));
		}
	}
}

/**
 * Reads a set of stats for the specified list of players
 *
 * @param Players the array of unique ids to read stats for
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::ReadOnlineStats(const TArray<FUniqueNetId>& Players,
	UOnlineStatsRead* StatsRead)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	if (CurrentStatsRead == NULL)
	{
		if (StatsRead != NULL)
		{
			CurrentStatsRead = StatsRead;
			// Clear previous results
			CurrentStatsRead->Rows.Empty();
			// Validate that players were specified
			if (Players.Num() > 0)
			{
				// Create an async task to read the stats and kick that off
				FLiveAsyncTaskReadStats* AsyncTask = new FLiveAsyncTaskReadStats(StatsRead->TitleId,
					Players,
					&ReadOnlineStatsCompleteDelegates);
				// Get the read specs so they can be populated
				XUSER_STATS_SPEC* Specs = AsyncTask->GetSpecs();
				// Fill in the Live data
				BuildStatsSpecs(Specs,StatsRead->ViewId,StatsRead->ColumnIds);
				// Copy the player info
				XUID* XPlayers = AsyncTask->GetPlayers();
				DWORD NumPlayers = AsyncTask->GetPlayerCount();
				DWORD BufferSize = 0;
				// First time through figure out how much memory to allocate for search results
				Return = XUserReadStats(StatsRead->TitleId,
					NumPlayers,
					XPlayers,
					1,
					Specs,
					&BufferSize,
					NULL,
					NULL);
				if (Return == ERROR_INSUFFICIENT_BUFFER && BufferSize > 0)
				{
					// Allocate the results buffer
					AsyncTask->AllocateResults(BufferSize);
					// Now kick off the async read
					Return = XUserReadStats(StatsRead->TitleId,
						NumPlayers,
						XPlayers,
						1,
						Specs,
						&BufferSize,
						AsyncTask->GetReadResults(),
						*AsyncTask);
					debugf(NAME_DevOnline,
						TEXT("XUserReadStats(0,%d,Players,1,Specs,%d,Buffer,Overlapped) returned 0x%08X"),
						NumPlayers,BufferSize,Return);
					if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
					{
						AsyncTasks.AddItem(AsyncTask);
					}
					else
					{
						// Don't leak the task/data
						delete AsyncTask;
					}
				}
				else
				{
					// Don't leak the task/data
					delete AsyncTask;
					debugf(NAME_DevOnline,
						TEXT("Failed to determine buffer size needed for stats read 0x%08X"),
						Return);
				}
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Can't read stats for zero players"));
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Can't perform a stats read with a null object"));
		}
		// Fire the delegate upon error
		if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
		{
			CurrentStatsRead = NULL;
			// Just trigger the delegate as having failed
			FAsyncTaskDelegateResults Results(Return);
			TriggerOnlineDelegates(this,ReadOnlineStatsCompleteDelegates,&Results);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Can't perform a stats read while one is in progress"));
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Reads a player's stats and all of that player's friends stats for the
 * specified set of stat views. This allows you to easily compare a player's
 * stats to their friends.
 *
 * @param LocalUserNum the local player having their stats and friend's stats read for
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::ReadOnlineStatsForFriends(BYTE LocalUserNum,
	UOnlineStatsRead* StatsRead)
{
	if (CurrentStatsRead == NULL)
	{
		if (StatsRead != NULL)
		{
			// Validate that it won't be outside our friends cache
			if (LocalUserNum >= 0 && LocalUserNum <= 4)
			{
				const FFriendsListCache& Cache = FriendsCache[LocalUserNum];
				TArray<FUniqueNetId> Players;
				// Allocate space for all of the friends plus the player
				Players.AddZeroed(Cache.Friends.Num() + 1);
				// Copy the player into the first
				GetUserXuid(LocalUserNum,(XUID*)&Players(0));
				// Iterate through the friends list and add them to the list
				for (INT Index = 0; Index < Cache.Friends.Num(); Index++)
				{
					Players(Index + 1) = Cache.Friends(Index).UniqueId;
				}
				// Now use the common method to read the stats
				return ReadOnlineStats(Players,StatsRead);
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Invalid player index specified %d"),
					(DWORD)LocalUserNum);
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Can't perform a stats read with a null object"));
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Can't perform a stats read while one is in progress"));
	}
	return FALSE;
}

/**
 * Reads stats by ranking. This grabs the rows starting at StartIndex through
 * NumToRead and places them in the StatsRead object.
 *
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 * @param StartIndex the starting rank to begin reads at (1 for top)
 * @param NumToRead the number of rows to read (clamped at 100 underneath)
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::ReadOnlineStatsByRank(UOnlineStatsRead* StatsRead,
	INT StartIndex,INT NumToRead)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	if (CurrentStatsRead == NULL)
	{
		if (StatsRead != NULL)
		{
			CurrentStatsRead = StatsRead;
			// Clear previous results
			CurrentStatsRead->Rows.Empty();
			FLiveAsyncTaskReadStatsByRank* AsyncTask = new FLiveAsyncTaskReadStatsByRank(&ReadOnlineStatsCompleteDelegates);
			DWORD BufferSize = 0;
			HANDLE hEnumerate = NULL;
			// Get the read specs so they can be populated
			XUSER_STATS_SPEC* Specs = AsyncTask->GetSpecs();
			// Fill in the Live data
			BuildStatsSpecs(Specs,StatsRead->ViewId,StatsRead->ColumnIds);
			// Figure out how much space is needed
			Return = XUserCreateStatsEnumeratorByRank(StatsRead->TitleId,
				StartIndex,
				NumToRead,
				1,
				Specs,
				&BufferSize,
				&hEnumerate);
			debugf(NAME_DevOnline,
				TEXT("XUserCreateStatsEnumeratorByRank(0,%d,%d,1,Specs,OutSize,OutHandle) returned 0x%08X"),
				StartIndex,NumToRead,Return);
			if (Return == ERROR_SUCCESS)
			{
				AsyncTask->Init(hEnumerate,BufferSize);
				// Start the async enumeration
				Return = XEnumerate(hEnumerate,
					AsyncTask->GetReadResults(),
					BufferSize,
					NULL,
					*AsyncTask);
				debugf(NAME_DevOnline,
					TEXT("XEnumerate(hEnumerate,Data,%d,Data,Overlapped) returned 0x%08X"),
					BufferSize,Return);
				if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
				{
					AsyncTasks.AddItem(AsyncTask);
				}
				else
				{
					// Don't leak the task/data
					delete AsyncTask;
				}
			}
			else
			{
				delete AsyncTask;
				debugf(NAME_DevOnline,
					TEXT("Failed to determine buffer size needed for stats enumeration 0x%08X"),
					Return);
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Can't perform a stats read with a null object"));
		}
		// Fire the delegate upon error
		if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
		{
			CurrentStatsRead = NULL;
			// Just trigger the delegate as having failed
			FAsyncTaskDelegateResults Results(Return);
			TriggerOnlineDelegates(this,ReadOnlineStatsCompleteDelegates,&Results);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Can't perform a stats read while one is in progress"));
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Reads stats by ranking centered around a player. This grabs a set of rows
 * above and below the player's current rank
 *
 * @param LocalUserNum the local player having their stats being centered upon
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 * @param NumRows the number of rows to read above and below the player's rank
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::ReadOnlineStatsByRankAroundPlayer(BYTE LocalUserNum,
	UOnlineStatsRead* StatsRead,INT NumRows)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	if (CurrentStatsRead == NULL)
	{
		if (StatsRead != NULL)
		{
			CurrentStatsRead = StatsRead;
			// Validate the index
			if (LocalUserNum >= 0 && LocalUserNum <= 4)
			{
				XUID Player;
				// Get the XUID of the player in question
				GetUserXuid(LocalUserNum,&Player);
				FLiveAsyncTaskReadStatsByRank* AsyncTask = new FLiveAsyncTaskReadStatsByRank(&ReadOnlineStatsCompleteDelegates);
				DWORD BufferSize = 0;
				HANDLE hEnumerate = NULL;
				// Get the read specs so they can be populated
				XUSER_STATS_SPEC* Specs = AsyncTask->GetSpecs();
				// Fill in the Live data
				BuildStatsSpecs(Specs,StatsRead->ViewId,StatsRead->ColumnIds);
				// Figure out how much space is needed
				Return = XUserCreateStatsEnumeratorByXuid(StatsRead->TitleId,
					Player,
					NumRows,
					1,
					Specs,
					&BufferSize,
					&hEnumerate);
				debugf(NAME_DevOnline,
					TEXT("XUserCreateStatsEnumeratorByXuid(0,%d,%d,1,Specs,OutSize,OutHandle) returned 0x%08X"),
					(DWORD)LocalUserNum,NumRows,Return);
				if (Return == ERROR_SUCCESS)
				{
					AsyncTask->Init(hEnumerate,BufferSize);
					// Start the async enumeration
					Return = XEnumerate(hEnumerate,
						AsyncTask->GetReadResults(),
						BufferSize,
						NULL,
						*AsyncTask);
					debugf(NAME_DevOnline,
						TEXT("XEnumerate(hEnumerate,Data,%d,Data,Overlapped) returned 0x%08X"),
						BufferSize,Return);
					if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
					{
						AsyncTasks.AddItem(AsyncTask);
					}
					else
					{
						// Don't leak the task/data
						delete AsyncTask;
					}
				}
				else
				{
					delete AsyncTask;
					debugf(NAME_DevOnline,
						TEXT("Failed to determine buffer size needed for stats enumeration 0x%08X"),
						Return);
				}
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Invalid player index specified %d"),
					(DWORD)LocalUserNum);
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Can't perform a stats read with a null object"));
		}
		// Fire the delegate upon error
		if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
		{
			CurrentStatsRead = NULL;
			// Just trigger the delegate as having failed
			FAsyncTaskDelegateResults Results(Return);
			TriggerOnlineDelegates(this,ReadOnlineStatsCompleteDelegates,&Results);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Can't perform a stats read while one is in progress"));
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Copies the stats data from our Epic form to something Live can handle
 *
 * @param Stats the destination buffer the stats are written to
 * @param Properties the Epic structures that need copying over
 * @param RatingId the id to set as the rating for this leaderboard set
 */
void UOnlineSubsystemLive::CopyStatsToProperties(XUSER_PROPERTY* Stats,
	const TArray<FSettingsProperty>& Properties,const DWORD RatingId)
{
	check(Properties.Num() < 64);
	// Copy each Epic property into the Live form
	for (INT Index = 0; Index < Properties.Num(); Index++)
	{
		const FSettingsProperty& Property = Properties(Index);
		// Assign the values
		Stats[Index].dwPropertyId = Property.PropertyId;
		// Do per type init
		switch (Property.Data.Type)
		{
			case SDT_Int32:
			{
				// Determine if this property needs to be promoted
				if (Property.PropertyId != RatingId)
				{
					Property.Data.GetData((INT&)Stats[Index].value.nData);
					Stats[Index].value.type = XUSER_DATA_TYPE_INT32;
				}
				else
				{
					INT Value;
					// Promote this value from Int32 to 64
					Property.Data.GetData(Value);
					Stats[Index].value.i64Data = (QWORD)Value;
					Stats[Index].value.type = XUSER_DATA_TYPE_INT64;
				}
				break;
			}
			case SDT_Float:
			{
				FLOAT Convert = 0.f;
				// Read it as a float, but report as a double
				Property.Data.GetData(Convert);
				Stats[Index].value.dblData = Convert;
				Stats[Index].value.type = XUSER_DATA_TYPE_DOUBLE;
				break;
			}
			case SDT_Double:
			{
				Property.Data.GetData(Stats[Index].value.dblData);
				Stats[Index].value.type = XUSER_DATA_TYPE_DOUBLE;
				break;
			}
			case SDT_Int64:
			{
				Property.Data.GetData((QWORD&)Stats[Index].value.i64Data);
				Stats[Index].value.type = XUSER_DATA_TYPE_INT64;
				break;
			}
			default:
			{
				Stats[Index].value.type = 0;
				debugf(NAME_DevOnline,
					TEXT("Ignoring stat type %d at index %d as it is unsupported by Live"),
					Property.Data.Type,Index);
				break;
			}
		}
		if (bShouldLogStatsData)
		{
			debugfLiveSlow(NAME_DevOnline,TEXT("Writing stat (%d) of type (%d) with value %s"),
				Property.PropertyId,Property.Data.Type,*Property.Data.ToString());
		}
	}
}

/**
 * Cleans up any platform specific allocated data contained in the stats data
 *
 * @param StatsRead the object to handle per platform clean up on
 */
void UOnlineSubsystemLive::FreeStats(UOnlineStatsRead* StatsRead)
{
	check(StatsRead);
	// We just empty these without any special platform data...yet
	StatsRead->Rows.Empty();
}

/**
 * Writes out the stats contained within the stats write object to the online
 * subsystem's cache of stats data. Note the new data replaces the old. It does
 * not write the data to the permanent storage until a FlushOnlineStats() call
 * or a session ends. Stats cannot be written without a session or the write
 * request is ignored. No more than 5 stats views can be written to at a time
 * or the write request is ignored.
 *
 * @param SessionName the name of the session the stats are for
 * @param Player the player to write stats for
 * @param StatsWrite the object containing the information to write
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::WriteOnlineStats(FName SessionName,FUniqueNetId Player,
	UOnlineStatsWrite* StatsWrite)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Grab the session information by name
	FNamedSession* Session = GetNamedSession(SessionName);
	// Can only write stats data as part of a session
	if (Session && Session->GameSettings && Session->SessionInfo)
	{
		if (StatsWrite != NULL)
		{
#if !FINAL_RELEASE
			// Validate the number of views and the number of stats per view
			if (IsValidStatsWrite(StatsWrite))
#endif
			{
				if (bShouldLogStatsData)
				{
					debugfLiveSlow(NAME_DevOnline,
						TEXT("Writing stats object type %s for session (%s)"),
						*StatsWrite->GetClass()->GetName(),
						*SessionName.ToString());
				}
				// The live specific buffers to use
				appMemzero(Views,sizeof(XSESSION_VIEW_PROPERTIES) * MAX_VIEWS);
				appMemzero(Stats,sizeof(XUSER_PROPERTY) * MAX_STATS);
				// Copy stats properties to the Live data
				CopyStatsToProperties(Stats,StatsWrite->Properties,StatsWrite->RatingId);
				// Get the number of views/stats involved
				DWORD StatsCount = StatsWrite->Properties.Num();
				// Switch upon the arbitration setting which set of view ids to use
				DWORD ViewCount = Session->GameSettings->bUsesArbitration ?
					StatsWrite->ArbitratedViewIds.Num() :
					StatsWrite->ViewIds.Num();
				INT* ViewIds = Session->GameSettings->bUsesArbitration ?
					(INT*)StatsWrite->ArbitratedViewIds.GetData() :
					(INT*)StatsWrite->ViewIds.GetData();
				// Initialize the view data for each view involved
				for (DWORD Index = 0; Index < ViewCount; Index++)
				{
					Views[Index].dwViewId = ViewIds[Index];
					Views[Index].dwNumProperties = StatsCount;
					Views[Index].pProperties = Stats;
					if (bShouldLogStatsData)
					{
						debugfLiveSlow(NAME_DevOnline,TEXT("ViewId = %d, NumProps = %d"),ViewIds[Index],StatsCount);
					}
				}
				FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
				// Write the data to the leaderboard
				Return = XSessionWriteStats(SessionInfo->Handle,
					(XUID&)Player,
					ViewCount,
					Views,
					NULL);
				debugf(NAME_DevOnline,
					TEXT("XSessionWriteStats() '%s' return 0x%08X"),
					*SessionName.ToString(),
					Return);
			}
#if !FINAL_RELEASE
			else
			{
				debugf(NAME_DevOnline,
					TEXT("Invalid stats write object specified for session (%s)"),
					*SessionName.ToString());
			}
#endif
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Can't write stats for session (%s) using a null object"),
				*SessionName.ToString());
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Can't write stats for session (%s) without a Live game in progress"),
			*SessionName.ToString());
	}
	return Return == ERROR_SUCCESS;
}

/**
 * Commits any changes in the online stats cache to the permanent storage
 *
 * @param SessionName the name of the session flushing stats
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemLive::FlushOnlineStats(FName SessionName)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	// Grab the session information by name
	FNamedSession* Session = GetNamedSession(SessionName);
	// Error if there isn't a session going
	if (Session && Session->GameSettings && Session->SessionInfo)
	{
		FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
		// Allocate an async task for deferred calling of the delegates
		FOnlineAsyncTaskLiveNamedSession* AsyncTask = new FOnlineAsyncTaskLiveNamedSession(SessionName,
			&FlushOnlineStatsDelegates,
			NULL,
			TEXT("XSessionFlushStats()"));
		// Show the live guide ui for inputing text
		Return = XSessionFlushStats(SessionInfo->Handle,*AsyncTask);
		debugf(NAME_DevOnline,
			TEXT("XSessionFlushStats() '%s' return 0x%08X"),
			*SessionName.ToString(),
			Return);
		if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
		{
			AsyncTasks.AddItem(AsyncTask);
		}
		else
		{
			// Just trigger the delegate as having failed
			FAsyncTaskDelegateResultsNamedSession Results(SessionName,Return);
			TriggerOnlineDelegates(this,FlushOnlineStatsDelegates,&Results);
			// Don't leak the task/data
			delete AsyncTask;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Can't flush stats for session (%s) without a Live game in progress"),
			*SessionName.ToString());
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Updates the flags and number of public/private slots that are available
 *
 * @param Session the session to modify/update
 * @param ScriptDelegates the set of delegates to fire when the modify complete
 */
DWORD UOnlineSubsystemLive::ModifySession(FNamedSession* Session,TArray<FScriptDelegate>* ScriptDelegates)
{
	check(Session && Session->GameSettings && Session->SessionInfo);
	FSecureSessionInfo* SessionInfo = GetSessionInfo(Session);
	// We must pass the same flags in (zero doesn't mean please don't change stuff)
	DWORD SessionFlags = BuildSessionFlags(Session->GameSettings);
	// Maintain original host flag since it can not be updated
	if (!(SessionInfo->Flags & XSESSION_CREATE_HOST))
	{
		// Strip off the hosting flag if specified
		SessionFlags &= ~XSESSION_CREATE_HOST;
	}
	// Save off the session flags
	SessionInfo->Flags = SessionFlags;	
	// Allocate the object that will hold the data throughout the async lifetime
	FOnlineAsyncTaskLiveNamedSession* AsyncTask = new FOnlineAsyncTaskLiveNamedSession(Session->SessionName,
		ScriptDelegates,
		NULL,
		TEXT("XSessionModify()"));
	// Tell Live to shrink to the number of public/private specified
	DWORD Result = XSessionModify(SessionInfo->Handle,
		SessionFlags,
		Session->GameSettings->NumPublicConnections,
		Session->GameSettings->NumPrivateConnections,
		*AsyncTask);
	debugfLiveSlow(NAME_DevOnline,
		TEXT("Updating session (%s) flags and size to public = %d, private = %d XSessionModify() returned 0x%08X"),
		*Session->SessionName.ToString(),
		Session->GameSettings->NumPublicConnections,
		Session->GameSettings->NumPrivateConnections,
		Result);
	// Clean up the task if it didn't succeed
	if (Result == ERROR_IO_PENDING)
	{
		// Queue the async task for ticking
		AsyncTasks.AddItem(AsyncTask);
	}
	else
	{
		delete AsyncTask;
		if (ScriptDelegates)
		{
			FAsyncTaskDelegateResultsNamedSession Results(Session->SessionName,Result);
			TriggerOnlineDelegates(this,*ScriptDelegates,&Results);
		}
	}
	return Result;
}

/**
 * Shrinks the session to the number of arbitrated registrants
 *
 * @param Session the session to modify/update
 */
void UOnlineSubsystemLive::ShrinkToArbitratedRegistrantSize(FNamedSession* Session)
{
	// Don't try to modify if you aren't the server
	if (IsServer() &&
		Session &&
		Session->GameSettings &&
		Session->GameSettings->bShouldShrinkArbitratedSessions &&
		// Don't try to do this if you aren't signed in
		AreAnySignedIntoLive())
	{
		// Update the number of public slots on the game settings
		Session->GameSettings->NumPublicConnections = Session->ArbitrationRegistrants.Num();
		Session->GameSettings->NumOpenPublicConnections = Session->ArbitrationRegistrants.Num() - 1;
		Session->GameSettings->NumPrivateConnections = 0;
		Session->GameSettings->NumOpenPrivateConnections = 0;
		// Flush the data up to Live
		ModifySession(Session,NULL);
	}
}

/**
 * Determines if the packet header is valid or not
 *
 * @param Packet the packet data to check
 * @param Length the size of the packet buffer
 *
 * @return true if the header is valid, false otherwise
 */
UBOOL UOnlineSubsystemLive::IsValidLanQueryPacket(const BYTE* Packet,
	DWORD Length,QWORD& ClientNonce)
{
	ClientNonce = 0;
	UBOOL bIsValid = FALSE;
	// Serialize out the data if the packet is the right size
	if (Length == LAN_BEACON_PACKET_HEADER_SIZE)
	{
		FNboSerializeFromBuffer PacketReader(Packet,Length);
		BYTE Version = 0;
		PacketReader >> Version;
		// Do the versions match?
		if (Version == LAN_BEACON_PACKET_VERSION)
		{
			BYTE Platform = 255;
			PacketReader >> Platform;
			// Can we communicate with this platform?
			if (Platform & LanPacketPlatformMask)
			{
				INT GameId = -1;
				PacketReader >> GameId;
				// Is this our game?
				if (GameId == LanGameUniqueId)
				{
					BYTE SQ1 = 0;
					PacketReader >> SQ1;
					BYTE SQ2 = 0;
					PacketReader >> SQ2;
					// Is this a server query?
					bIsValid = (SQ1 == LAN_SERVER_QUERY1 && SQ2 == LAN_SERVER_QUERY2);
					// Read the client nonce as the outvalue
					PacketReader >> ClientNonce;
				}
			}
		}
	}
	return bIsValid;
}

/**
 * Determines if the packet header is valid or not
 *
 * @param Packet the packet data to check
 * @param Length the size of the packet buffer
 *
 * @return true if the header is valid, false otherwise
 */
UBOOL UOnlineSubsystemLive::IsValidLanResponsePacket(const BYTE* Packet,DWORD Length)
{
	UBOOL bIsValid = FALSE;
	// Serialize out the data if the packet is the right size
	if (Length > LAN_BEACON_PACKET_HEADER_SIZE)
	{
		FNboSerializeFromBuffer PacketReader(Packet,Length);
		BYTE Version = 0;
		PacketReader >> Version;
		// Do the versions match?
		if (Version == LAN_BEACON_PACKET_VERSION)
		{
			BYTE Platform = 255;
			PacketReader >> Platform;
			// Can we communicate with this platform?
			if (Platform & LanPacketPlatformMask)
			{
				INT GameId = -1;
				PacketReader >> GameId;
				// Is this our game?
				if (GameId == LanGameUniqueId)
				{
					BYTE SQ1 = 0;
					PacketReader >> SQ1;
					BYTE SQ2 = 0;
					PacketReader >> SQ2;
					// Is this a server response?
					if (SQ1 == LAN_SERVER_RESPONSE1 && SQ2 == LAN_SERVER_RESPONSE2)
					{
						QWORD Nonce = 0;
						PacketReader >> Nonce;
						bIsValid = Nonce == (QWORD&)LanNonce;
					}
				}
			}
		}
	}
	return bIsValid;
}

/**
 * Ticks any lan beacon background tasks
 *
 * @param DeltaTime the time since the last tick
 */
void UOnlineSubsystemLive::TickLanTasks(FLOAT DeltaTime)
{
	if (LanBeaconState > LANB_NotUsingLanBeacon && LanBeacon != NULL)
	{
		BYTE PacketData[512];
		UBOOL bShouldRead = TRUE;
		// Read each pending packet and pass it out for processing
		while (bShouldRead)
		{
			INT NumRead = LanBeacon->ReceivePacket(PacketData,512);
			if (NumRead > 0)
			{
				// Hand this packet off to child classes for processing
				ProcessLanPacket(PacketData,NumRead);
				// Reset the timeout since a packet came in
				LanQueryTimeLeft = LanQueryTimeout;
			}
			else
			{
				if (LanBeaconState == LANB_Searching)
				{
					// Decrement the amount of time remaining
					LanQueryTimeLeft -= DeltaTime;
					// Check for a timeout on the search packet
					if (LanQueryTimeLeft <= 0.f)
					{
						// See if there were any sessions that were marked as hosting before the search started
						UBOOL bWasHosting = FALSE;
						for (INT SessionIdx=0; SessionIdx < Sessions.Num(); SessionIdx++)
						{
							FNamedSession& Session = Sessions(SessionIdx);
							if (Session.GameSettings != NULL &&
								Session.GameSettings->bShouldAdvertise &&
								Session.GameSettings->bIsLanMatch &&
								IsServer())
							{
								bWasHosting = TRUE;
								break;
							}
						}
						if (bWasHosting)
						{
							// Maintain lan beacon if there was a session that was marked as hosting
							LanBeaconState = LANB_Hosting;
						}
						else
						{
							// Stop future timeouts since we aren't searching any more
							StopLanBeacon();
						}
						if (GameSearch != NULL)
						{
							GameSearch->bIsSearchInProgress = FALSE;
						}
						// Trigger the delegate so the UI knows we didn't find any
						FAsyncTaskDelegateResults Results(S_OK);
						TriggerOnlineDelegates(this,FindOnlineGamesCompleteDelegates,&Results);
					}
				}
				bShouldRead = FALSE;
			}
		}
	}
}

/**
 * Adds the game settings data to the packet that is sent by the host
 * in reponse to a server query
 *
 * @param Packet the writer object that will encode the data
 * @param GameSettings the game settings to add to the packet
 */
void UOnlineSubsystemLive::AppendGameSettingsToPacket(FNboSerializeToBuffer& Packet,
	UOnlineGameSettings* GameSettings)
{
#if DEBUG_LAN_BEACON
	debugf(NAME_DevOnline,TEXT("Sending game settings to client"));
#endif
	// Members of the game settings class
	Packet << GameSettings->NumOpenPublicConnections
		<< GameSettings->NumOpenPrivateConnections
		<< GameSettings->NumPublicConnections
		<< GameSettings->NumPrivateConnections
		<< (BYTE)GameSettings->bShouldAdvertise
		<< (BYTE)GameSettings->bIsLanMatch
		<< (BYTE)GameSettings->bUsesStats
		<< (BYTE)GameSettings->bAllowJoinInProgress
		<< (BYTE)GameSettings->bAllowInvites
		<< (BYTE)GameSettings->bUsesPresence
		<< (BYTE)GameSettings->bAllowJoinViaPresence
		<< (BYTE)GameSettings->bUsesArbitration;
	// Write the player id so we can show gamercard
	Packet << GameSettings->OwningPlayerId;
	Packet << GameSettings->OwningPlayerName;
#if DEBUG_SYSLINK
	QWORD Uid = (QWORD&)GameSettings->OwningPlayerId.Uid;
	debugf(NAME_DevOnline,TEXT("%s 0x%016I64X"),*GameSettings->OwningPlayerName,Uid);
#endif
	// Now add the contexts and properties from the settings class
	// First, add the number contexts involved
	INT Num = GameSettings->LocalizedSettings.Num();
	Packet << Num;
	// Now add each context individually
	for (INT Index = 0; Index < GameSettings->LocalizedSettings.Num(); Index++)
	{
		Packet << GameSettings->LocalizedSettings(Index);
#if DEBUG_LAN_BEACON
		debugf(NAME_DevOnline,*BuildContextString(GameSettings,GameSettings->LocalizedSettings(Index)));
#endif
	}
	// Next, add the number of properties involved
	Num = GameSettings->Properties.Num();
	Packet << Num;
	// Now add each property
	for (INT Index = 0; Index < GameSettings->Properties.Num(); Index++)
	{
		Packet << GameSettings->Properties(Index);
#if DEBUG_LAN_BEACON
		debugf(NAME_DevOnline,*BuildPropertyString(GameSettings,GameSettings->Properties(Index)));
#endif
	}
}

/**
 * Reads the game settings data from the packet and applies it to the
 * specified object
 *
 * @param Packet the reader object that will read the data
 * @param GameSettings the game settings to copy the data to
 */
void UOnlineSubsystemLive::ReadGameSettingsFromPacket(FNboSerializeFromBuffer& Packet,
	UOnlineGameSettings* GameSettings)
{
#if DEBUG_LAN_BEACON
	debugf(NAME_DevOnline,TEXT("Reading game settings from server"));
#endif
	// Members of the game settings class
	Packet >> GameSettings->NumOpenPublicConnections
		>> GameSettings->NumOpenPrivateConnections
		>> GameSettings->NumPublicConnections
		>> GameSettings->NumPrivateConnections;
	BYTE Read = FALSE;
	// Read all the bools as bytes
	Packet >> Read;
	GameSettings->bShouldAdvertise = Read == TRUE;
	Packet >> Read;
	GameSettings->bIsLanMatch = Read == TRUE;
	Packet >> Read;
	GameSettings->bUsesStats = Read == TRUE;
	Packet >> Read;
	GameSettings->bAllowJoinInProgress = Read == TRUE;
	Packet >> Read;
	GameSettings->bAllowInvites = Read == TRUE;
	Packet >> Read;
	GameSettings->bUsesPresence = Read == TRUE;
	Packet >> Read;
	GameSettings->bAllowJoinViaPresence = Read == TRUE;
	Packet >> Read;
	GameSettings->bUsesArbitration = Read == TRUE;
	// Read the owning player id
	Packet >> GameSettings->OwningPlayerId;
	// Read the owning player name
	Packet >> GameSettings->OwningPlayerName;
#if DEBUG_LAN_BEACON
	QWORD Uid = (QWORD&)GameSettings->OwningPlayerId.Uid;
	debugf(NAME_DevOnline,TEXT("%s 0x%016I64X"),*GameSettings->OwningPlayerName,Uid);
#endif
	// Now read the contexts and properties from the settings class
	INT NumContexts = 0;
	// First, read the number contexts involved, so we can presize the array
	Packet >> NumContexts;
	if (Packet.HasOverflow() == FALSE)
	{
		GameSettings->LocalizedSettings.Empty(NumContexts);
		GameSettings->LocalizedSettings.AddZeroed(NumContexts);
	}
	// Now read each context individually
	for (INT Index = 0;
		Index < GameSettings->LocalizedSettings.Num() && Packet.HasOverflow() == FALSE;
		Index++)
	{
		Packet >> GameSettings->LocalizedSettings(Index);
#if DEBUG_LAN_BEACON
		debugf(NAME_DevOnline,*BuildContextString(GameSettings,GameSettings->LocalizedSettings(Index)));
#endif
	}
	INT NumProps = 0;
	// Next, read the number of properties involved for array presizing
	Packet >> NumProps;
	if (Packet.HasOverflow() == FALSE)
	{
		GameSettings->Properties.Empty(NumProps);
		GameSettings->Properties.AddZeroed(NumProps);
	}
	// Now read each property from the packet
	for (INT Index = 0;
		Index < GameSettings->Properties.Num() && Packet.HasOverflow() == FALSE;
		Index++)
	{
		Packet >> GameSettings->Properties(Index);
#if DEBUG_LAN_BEACON
		debugf(NAME_DevOnline,*BuildPropertyString(GameSettings,GameSettings->Properties(Index)));
#endif
	}
	// If there was an overflow, treat the string settings/properties as broken
	if (Packet.HasOverflow())
	{
		GameSettings->LocalizedSettings.Empty();
		GameSettings->Properties.Empty();
		debugf(NAME_DevOnline,TEXT("Packet overflow detected in ReadGameSettingsFromPacket()"));
	}
}

/**
 * Builds a LAN query and broadcasts it
 *
 * @return an error/success code
 */
DWORD UOnlineSubsystemLive::FindLanGames(void)
{
	DWORD Return = S_OK;
	// Recreate the unique identifier for this client
	XNetRandom(LanNonce,8);
	// Create the lan beacon if we don't already have one
	if (LanBeacon == NULL)
	{
		LanBeacon = new FLanBeacon();
		if (LanBeacon->Init(LanAnnouncePort) == FALSE)
		{
			debugf(NAME_DevOnline,TEXT("Failed to create socket for lan announce port %s"),
				GSocketSubsystem->GetSocketError());
			Return = E_FAIL;
		}
	}
	// If we have a socket and a nonce, broadcast a discovery packet
	if (LanBeacon && Return == S_OK)
	{
		QWORD Nonce = *(QWORD*)LanNonce;
		FNboSerializeToBuffer Packet(LAN_BEACON_MAX_PACKET_SIZE);
		// Build the discovery packet
		Packet << LAN_BEACON_PACKET_VERSION
			// Platform information
			<< (BYTE)appGetPlatformType()
			// Game id to prevent cross game lan packets
			<< LanGameUniqueId
			// Identify the packet type
			<< LAN_SERVER_QUERY1 << LAN_SERVER_QUERY2
			// Append the nonce as a QWORD
			<< Nonce;
		// Now kick off our broadcast which hosts will respond to
		if (LanBeacon->BroadcastPacket(Packet,Packet.GetByteCount()))
		{
			debugfLiveSlow(NAME_DevOnline,TEXT("Sent query packet..."));
			// We need to poll for the return packets
			LanBeaconState = LANB_Searching;
			// Set the timestamp for timing out a search
			LanQueryTimeLeft = LanQueryTimeout;
			GameSearch->bIsSearchInProgress = TRUE;
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Failed to send discovery broadcast %s"),
				GSocketSubsystem->GetSocketError());
			Return = E_FAIL;
		}
	}
	if (Return != S_OK)
	{
		delete LanBeacon;
		LanBeacon = NULL;
		LanBeaconState = LANB_NotUsingLanBeacon;

		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResults Results(Return);
		TriggerOnlineDelegates(this,FindOnlineGamesCompleteDelegates,&Results);
	}
	return Return;
}

/**
 * Sends a message to a friend
 *
 * @param LocalUserNum the user that is sending the message
 * @param Friend the player to send the message to
 * @param Message the message to display to the recipient
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemLive::SendMessageToFriend(BYTE LocalUserNum,FUniqueNetId Friend,const FString& Message)
{
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Show the UI with the message in it
		return XShowMessageComposeUI(LocalUserNum,(XUID*)&Friend,1,*Message);
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid or not logged in player specified (%d)"),(DWORD)LocalUserNum);
	}
	return FALSE;
}

/**
 * Sends an invitation to play in the player's current session
 *
 * @param LocalUserNum the user that is sending the invite
 * @param Friend the player to send the invite to
 * @param Text the message to accompany the invite
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemLive::SendGameInviteToFriend(BYTE LocalUserNum,FUniqueNetId Friend,const FString& Text)
{
	TArray<FUniqueNetId> Friends;
	Friends.AddItem(Friend);
	// Use the group method to do the send
	return SendGameInviteToFriends(LocalUserNum,Friends,Text);
}

/**
 * Sends invitations to play in the player's current session
 *
 * @param LocalUserNum the user that is sending the invite
 * @param Friends the player to send the invite to
 * @param Text the message to accompany the invite
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemLive::SendGameInviteToFriends(BYTE LocalUserNum,const TArray<FUniqueNetId>& Friends,const FString& Text)
{
	DWORD Return = E_FAIL;
	if (LocalUserNum >=0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		UBOOL bHasInvitableSession = FALSE;
		// Make sure that there is an invitable session present
		for (INT Index = 0; Index < Sessions.Num(); Index++)
		{
			const FNamedSession& Session = Sessions(Index);
			// Make sure the session is invitable and not full
			if (Session.GameSettings != NULL &&
				Session.GameSettings->bAllowInvites &&
				(Session.GameSettings->NumPublicConnections + Session.GameSettings->NumPrivateConnections) > Session.Registrants.Num())
			{
				bHasInvitableSession = TRUE;
				break;
			}
		}
		if (bHasInvitableSession)
		{
			// Create an async task for logging the process
			FLiveAsyncTaskInviteToGame* AsyncTask = new FLiveAsyncTaskInviteToGame(Friends,Text);
			// Send the invites async
			Return = XInviteSend(LocalUserNum,
				AsyncTask->GetInviteeCount(),
				AsyncTask->GetInvitees(),
				AsyncTask->GetMessage(),
				*AsyncTask);
			debugf(NAME_DevOnline,TEXT("XInviteSend(%d,%d,,%s,) returned 0x%08X"),
				(DWORD)LocalUserNum,
				AsyncTask->GetInviteeCount(),
				AsyncTask->GetMessage(),
				Return);
			if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
			{
				// Queue the async task for ticking
				AsyncTasks.AddItem(AsyncTask);
			}
			else
			{
				delete AsyncTask;
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("No invitable session present. Not sending an invite"));
		}
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Attempts to join a friend's game session (join in progress)
 *
 * @param LocalUserNum the user that is sending the invite
 * @param FriendId the player to follow into the match
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemLive::JoinFriendGame(BYTE LocalUserNum,FUniqueNetId FriendId)
{
	if (LocalUserNum >=0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		if (IsUserInSession((QWORD&)FriendId) == FALSE)
		{
			QWORD SessionId = 0;
			const TArray<FOnlineFriend>& Friends = FriendsCache[LocalUserNum].Friends;
			// Find the friend in the cached list and search for that session id
			for (INT FriendIndex = 0; FriendIndex < Friends.Num(); FriendIndex++)
			{
				const FOnlineFriend& Friend = Friends(FriendIndex);
				if (Friend.UniqueId == FriendId)
				{
					// This is the session that we are going to try to join
					SessionId = Friend.SessionId;
					break;
				}
			}
#if CONSOLE
			// Check any party sessions in case this is not a friend, but a party chat person
			if (SessionId == 0)
			{
				ULivePartyChat* PartyChat = Cast<ULivePartyChat>(PartyChatInterface.GetObject());
				if (PartyChat != NULL)
				{
					TArray<FOnlinePartyMember> PartyMembers;
					// Get the list of players you are in party chat with
					if (PartyChat->GetPartyMembersInformation(PartyMembers))
					{
						// Search the list for the player's xuid
						for (INT PartyIndex = 0; PartyIndex < PartyMembers.Num(); PartyIndex++)
						{
							const FOnlinePartyMember& Member = PartyMembers(PartyIndex);
							if (Member.UniqueId == FriendId)
							{
								// This is the session that we are going to try to join
								SessionId = Member.SessionId;
								break;
							}
						}
					}
				}
			}
#endif
			if (SessionId != 0)
			{
				// This code assumes XNKID is 8 bytes
				check(sizeof(QWORD) == sizeof(XNKID));
				XINVITE_INFO* Info;
				// Allocate space on demand
				if (InviteCache[LocalUserNum].InviteData == NULL)
				{
					InviteCache[LocalUserNum].InviteData = new XINVITE_INFO;
				}
				// If for some reason the data didn't get cleaned up, do so now
				if (InviteCache[LocalUserNum].InviteSearch != NULL &&
					InviteCache[LocalUserNum].InviteSearch->Results.Num() > 0)
				{
					// Clean up the invite data
					delete (XSESSION_INFO*)InviteCache[LocalUserNum].InviteSearch->Results(0).PlatformData;
					InviteCache[LocalUserNum].InviteSearch->Results(0).PlatformData = NULL;
					InviteCache[LocalUserNum].InviteSearch = NULL;
				}
				// Get the buffer to use and clear the previous contents
				Info = InviteCache[LocalUserNum].InviteData;
				appMemzero(Info,sizeof(XINVITE_INFO));
				// Fill in the invite data manually
				Info->dwTitleID = appGetTitleId();
				Info->fFromGameInvite = TRUE;
				Info->xuidInviter = FriendId.Uid;
				// Now use the join by id code
				return HandleJoinBySessionId(LocalUserNum,FriendId.Uid,SessionId);
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Friend 0x%016I64X was not in a joinable session"),FriendId.Uid);
			}
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Player (%d) is already in the session with friend 0x%016I64X"),
				LocalUserNum,
				FriendId.Uid);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to JoinFriendGame()"),
			(DWORD)LocalUserNum);
	}
	return FALSE;
}

/**
 * Starts an asynchronous read of the specified file from the network platform's
 * title specific file store
 *
 * @param FileToRead the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
UBOOL UOnlineSubsystemLive::ReadTitleFile(const FString& FileToRead)
{
	WCHAR ServerPath[XONLINE_MAX_PATHNAME_LENGTH];
	ServerPath[0] = L'\0';
	DWORD ServerPathLen = XONLINE_MAX_PATHNAME_LENGTH;
	// Get the name to send to Live for finding files
	DWORD Result = XStorageBuildServerPath(0,
		XSTORAGE_FACILITY_PER_TITLE,
		NULL,
		0,
		*FileToRead,
		ServerPath,
		&ServerPathLen);
	debugfLiveSlow(NAME_DevLive,TEXT("XStorageBuildServerPath(%s) returned 0x%08X with path %s"),
		*FileToRead,
		Result,
		ServerPath);
	if (Result == ERROR_SUCCESS)
	{
		// Create the async task that will hold the enumeration data
		FLiveAsyncTMSRead* AsyncTask = new FLiveAsyncTMSRead(TitleManagedFiles,&ReadTitleFileCompleteDelegates);
		// Start the file list enumeration
		Result = XStorageEnumerate(0,
			ServerPath,
			0,
			MAX_TITLE_MANAGED_STORAGE_FILES,
			AsyncTask->GetAllocatedSize(),
			AsyncTask->GetEnumerationResults(),
			*AsyncTask);
		if (Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING)
		{
			// Queue the async task for ticking
			AsyncTasks.AddItem(AsyncTask);
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Failed to enumerate TMS file(s) (%s). XStorageEnumerate() returned 0x%08X"),
				*FileToRead,
				Result);
			delete AsyncTask;
		}
	}
	return Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING;
}

/**
 * Copies the file data into the specified buffer for the specified file
 *
 * @param FileToRead the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
UBOOL UOnlineSubsystemLive::GetTitleFileContents(const FString& FileName,TArray<BYTE>& FileContents)
{
	// Search for the specified file and return the raw data
	for (INT FileIndex = 0; FileIndex < TitleManagedFiles.Num(); FileIndex++)
	{
		FTitleFile& TitleFile = TitleManagedFiles(FileIndex);
		if (TitleFile.Filename == FileName)
		{
			FileContents = TitleFile.Data;
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Empties the set of downloaded files if possible (no async tasks outstanding)
 *
 * @return true if they could be deleted, false if they could not
 */
UBOOL UOnlineSubsystemLive::ClearDownloadedFiles(void)
{
	for (INT Index = 0; Index < TitleManagedFiles.Num(); Index++)
	{
		FTitleFile& TitleFile = TitleManagedFiles(Index);
		// If there is an async task outstanding, fail to empty
		if (TitleFile.AsyncState == OERS_InProgress)
		{
			return FALSE;
		}
	}
	// No async files being handled, so empty them all
	TitleManagedFiles.Empty();
	return TRUE;
}

/**
 * Empties the cached data for this file if it is not being downloaded currently
 *
 * @param FileName the name of the file to remove from the cache
 *
 * @return true if it could be deleted, false if it could not
 */
UBOOL UOnlineSubsystemLive::ClearDownloadedFile(const FString& FileName)
{
	INT FoundIndex = INDEX_NONE;
	// Search for the file
	for (INT Index = 0; Index < TitleManagedFiles.Num(); Index++)
	{
		FTitleFile& TitleFile = TitleManagedFiles(Index);
		// If there is an async task outstanding on this file, fail to empty
		if (TitleFile.Filename == FileName)
		{
			if (TitleFile.AsyncState == OERS_InProgress)
			{
				return FALSE;
			}
			FoundIndex = Index;
			break;
		}
	}
	if (FoundIndex != INDEX_NONE)
	{
		TitleManagedFiles.Remove(FoundIndex);
	}
	return TRUE;
}

/**
 * Starts an async read for the achievement list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param TitleId the title id of the game the achievements are to be read for
 * @param bShouldReadText whether to fetch the text strings or not
 * @param bShouldReadImages whether to fetch the image data or not
 *
 * @return TRUE if the task starts, FALSE if it failed
 */
UBOOL UOnlineSubsystemLive::ReadAchievements(BYTE LocalUserNum,INT TitleId,UBOOL bShouldReadText,UBOOL bShouldReadImages)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		XUID Xuid = INVALID_XUID;
		// Get the struct that we'll fill
		FCachedAchievements& Cached = GetCachedAchievements(LocalUserNum,TitleId);
		// Ignore multiple reads on the same achievement list
		if (Cached.ReadState == OERS_NotStarted)
		{
			HANDLE Handle = NULL;
			DWORD SizeNeeded = 0;
			DWORD Flags = XACHIEVEMENT_DETAILS_TFC;
			// Set the flags on what to read based upon the flags passed in
			if (bShouldReadText)
			{
				Flags |= XACHIEVEMENT_DETAILS_LABEL | XACHIEVEMENT_DETAILS_DESCRIPTION | XACHIEVEMENT_DETAILS_UNACHIEVED;
			}
			// Create a new enumerator for reading the achievements list
			Return = XUserCreateAchievementEnumerator((DWORD)TitleId,
				LocalUserNum,
				Xuid,
				Flags,
				0,
				60,
				&SizeNeeded,
				&Handle);
			debugfLiveSlow(NAME_DevOnline,
				TEXT("XUserCreateAchievementEnumerator(0x%08X,%d,0x%016I64X,0,1,%d,out) returned 0x%08X"),
				TitleId,
				(DWORD)LocalUserNum,
				Xuid,
				SizeNeeded,
				Return);
			if (Return == ERROR_SUCCESS)
			{
				// Create the async data object that holds the buffers, etc.
				FLiveAsyncTaskDataReadAchievements* AsyncTaskData = new FLiveAsyncTaskDataReadAchievements(TitleId,LocalUserNum,Handle,SizeNeeded,0,bShouldReadImages);
				// Create the async task object
				FLiveAsyncTaskReadAchievements* AsyncTask = new FLiveAsyncTaskReadAchievements(
					&PerUserDelegates[LocalUserNum].AchievementReadDelegates,
					AsyncTaskData);
				// Start the async read
				Return = XEnumerate(Handle,
					AsyncTaskData->GetBuffer(),
					SizeNeeded,
					0,
					*AsyncTask);
				if (Return == ERROR_IO_PENDING)
				{
					// Mark this as being read
					Cached.ReadState = OERS_InProgress;
					AsyncTasks.AddItem(AsyncTask);
				}
				else
				{
					// Delete the async task
					delete AsyncTask;
				}
			}
		}
		// If it has already been done, indicate success
		else if (Cached.ReadState == OERS_Done)
		{
			Return = ERROR_SUCCESS;
		}
		// If one is in progress, indicate that it is still outstanding
		else if (Cached.ReadState == OERS_InProgress)
		{
			Return = ERROR_IO_PENDING;
		}
		if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
		{
			Cached.ReadState = OERS_Failed;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in ReadAchievements(%d)"),
			(DWORD)LocalUserNum);
	}
	// Fire off the delegate if needed
	if (Return != ERROR_IO_PENDING)
	{
		OnlineSubsystemLive_eventOnReadAchievementsComplete_Parms Parms(EC_EventParm);
		Parms.TitleId = TitleId;
		// Use the common method to do the work
		TriggerOnlineDelegates(this,PerUserDelegates[LocalUserNum].AchievementReadDelegates,&Parms);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Copies the list of achievements for the specified player and title id
 *
 * @param LocalUserNum the user to read the friends list of
 * @param Achievements the out array that receives the copied data
 * @param TitleId the title id of the game that these were read for
 *
 * @return OERS_Done if the read has completed, otherwise one of the other states
 */
BYTE UOnlineSubsystemLive::GetAchievements(BYTE LocalUserNum,TArray<FAchievementDetails>& Achievements,INT TitleId)
{
	FCachedAchievements& Cached = GetCachedAchievements(LocalUserNum,TitleId);
	Achievements.Reset();
	Achievements = Cached.Achievements;
	return Cached.ReadState;
}

/**
 * Shows a custom players UI for the specified list of players
 *
 * @param LocalUserNum the controller number of the associated user
 * @param Players the list of players to show in the custom UI
 * @param Title the title to use for the UI
 * @param Description the text to show at the top of the UI
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemLive::ShowCustomPlayersUI(BYTE LocalUserNum,const TArray<FUniqueNetId>& Players,const FString& Title,const FString& Description)
{
	DWORD Result = E_FAIL;
	// Validate the user index passed in
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Copy the various data so that it exists throughout the async call
		FLiveAsyncTaskCustomPlayersList* AsyncTask = new FLiveAsyncTaskCustomPlayersList(Players,Title,Description);
		// Show the live guide ui for the player list
		Result = XShowCustomPlayerListUI(LocalUserNum,
			0,
			AsyncTask->GetTitle(),
			AsyncTask->GetDescription(),
			NULL,
			0,
			AsyncTask->GetPlayers(),
			AsyncTask->GetPlayerCount(),
			NULL,
			NULL,
			NULL,
			*AsyncTask);
		if (Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING)
		{
			AsyncTasks.AddItem(AsyncTask);
		}
		else
		{
			delete AsyncTask;
			debugf(NAME_DevOnline,TEXT("XShowCustomPlayersUI(%d,%d,%s,%s) failed with 0x%08X"),
				(DWORD)LocalUserNum,
				Players.Num(),
				*Title,
				*Description,
				Result);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to ShowPlayersUI()"),
			(DWORD)LocalUserNum);
	}
	return Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING;
}

/**
 * Kicks off an async read of the skill information for the list of players in the
 * search object
 *
 * @param SearchingPlayerNum the player executing the search
 * @param SearchSettings the object that has the list of players to use in the read
 *
 * @return the error/success code from the skill search
 */
DWORD UOnlineSubsystemLive::ReadSkillForSearch(BYTE SearchingPlayerNum,UOnlineGameSearch* SearchSettings)
{
	check(SearchSettings);
	// Create an async task to read the stats and kick that off
	FLiveAsyncReadPlayerSkillForSearch* AsyncTask = new FLiveAsyncReadPlayerSkillForSearch(SearchingPlayerNum,SearchSettings);
	// Set the vars passed into the read so we can inspect them when debugging
	DWORD NumPlayers = SearchSettings->ManualSkillOverride.Players.Num();
	XUID* XPlayers = (XUID*)SearchSettings->ManualSkillOverride.Players.GetData();
	DWORD BufferSize = 0;
	// First time through figure out how much memory to allocate for search results
	DWORD Return = XUserReadStats(0,
		NumPlayers,
		XPlayers,
		1,
		AsyncTask->GetSpecs(),
		&BufferSize,
		NULL,
		NULL);
	if (Return == ERROR_INSUFFICIENT_BUFFER && BufferSize > 0)
	{
		// Allocate the results buffer
		AsyncTask->AllocateSpace(BufferSize);
		// Now kick off the async skill leaderboard read
		Return = XUserReadStats(0,
			NumPlayers,
			XPlayers,
			1,
			AsyncTask->GetSpecs(),
			&BufferSize,
			AsyncTask->GetReadBuffer(),
			*AsyncTask);
		debugfLiveSlow(NAME_DevOnline,
			TEXT("XUserReadStats(0,%d,Players,1,Specs,%d,Buffer,Overlapped) for skill read returned 0x%08X"),
			NumPlayers,BufferSize,Return);
		if (Return == ERROR_IO_PENDING)
		{
			AsyncTasks.AddItem(AsyncTask);
		}
	}
	if (Return != ERROR_IO_PENDING)
	{
		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResults Results(Return);
		TriggerOnlineDelegates(this,FindOnlineGamesCompleteDelegates,&Results);
		// Don't leak the task
		delete AsyncTask;
	}
	return Return;
}

/**
 * Returns the skill for the last search if the search manually specified a search value
 * otherwise it uses the default skill rating
 *
 * @param OutMu has the skill rating set
 * @param OutSigma has the skill certainty set
 * @param OutCount has the number of contributing players in it
 */
void UOnlineSubsystemLive::GetLocalSkills(DOUBLE& OutMu,DOUBLE& OutSigma,DOUBLE& OutCount)
{
	// Set to the defaults (middle of the curve, completely uncertain)
	OutMu = 3.0;
	OutSigma = 1.0;
	OutCount = 1.0;
	// Check for a game search overriding the values
	if (GameSearch && GameSearch->ManualSkillOverride.Mus.Num())
	{
		OutCount = GameSearch->ManualSkillOverride.Mus.Num();
		// Use the API to aggregate the skill information
		XSessionCalculateSkill(GameSearch->ManualSkillOverride.Mus.Num(),
			GameSearch->ManualSkillOverride.Mus.GetTypedData(),
			GameSearch->ManualSkillOverride.Sigmas.GetTypedData(),
			&OutMu,
			&OutSigma);
	}
	debugfLiveSlow(NAME_DevOnline,
		TEXT("Local skills: Mu (%f), Sigma (%f), Count (%f), SkillLow (%d), SkillHigh (%d)"),
		OutMu,
		OutSigma,
		OutCount,
		CalculateConservativeSkill(OutMu,OutSigma),
		CalculateOptimisticSkill(OutMu,OutSigma));
}

/**
 * Determines how good of a skill match this session is for the local players
 *
 * @param Mu the skill rating of the local player(s)
 * @param Sigma the certainty of that rating
 * @param PlayerCount the number of players contributing to the skill
 * @param GameSettings the game session to calculate the match quality for
 */
void UOnlineSubsystemLive::CalculateMatchQuality(DOUBLE Mu,DOUBLE Sigma,DOUBLE PlayerCount,UOnlineGameSettings* HostSettings)
{
	DOUBLE HostMu = 3.0;
	DOUBLE HostSigma = 1.0;
	// Count how many players are consuming public & private slots
	DOUBLE HostCount = HostSettings->NumPublicConnections +
		HostSettings->NumPrivateConnections -
		HostSettings->NumOpenPublicConnections -
		HostSettings->NumOpenPrivateConnections;
	// Search through the returned settings for the Mu & Sigma values
	FSettingsProperty* PropMu = HostSettings->FindProperty(X_PROPERTY_GAMER_MU);
	if (PropMu != NULL)
	{
		FSettingsProperty* PropSigma = HostSettings->FindProperty(X_PROPERTY_GAMER_SIGMA);
		if (PropSigma != NULL)
		{
			PropMu->Data.GetData(HostMu);
			PropSigma->Data.GetData(HostSigma);
		}
	}
	// Now use the two sets of skill information to determine the match quality
	HostSettings->MatchQuality = CalculateHostQuality(Mu,Sigma,PlayerCount,HostMu,HostSigma,HostCount);
	debugfLiveSlow(NAME_DevOnline,
		TEXT("Match quality for host %s with skill (Mu (%f), Sigma (%f), Count (%f), SkillLow (%d), SkillHigh (%d)) is %f"),
		*HostSettings->OwningPlayerName,
		HostMu,
		HostSigma,
		HostCount,
		CalculateConservativeSkill(HostMu,HostSigma),
		CalculateOptimisticSkill(HostMu,HostSigma),
		HostSettings->MatchQuality);
}

/**
 * Takes the manual skill override data and places that in the properties array
 *
 * @param SearchSettings the search object to update
 */
void UOnlineSubsystemLive::AppendSkillProperties(UOnlineGameSearch* SearchSettings)
{
	if (SearchSettings->ManualSkillOverride.Players.Num())
	{
		DOUBLE Mu = 3.0;
		DOUBLE Sigma = 1.0;
		// Use the API to aggregate the skill information
		XSessionCalculateSkill(SearchSettings->ManualSkillOverride.Mus.Num(),
			SearchSettings->ManualSkillOverride.Mus.GetTypedData(),
			SearchSettings->ManualSkillOverride.Sigmas.GetTypedData(),
			&Mu,
			&Sigma);
		debugfLiveSlow(NAME_DevOnline,
			TEXT("Searching for match using skill of Mu (%f), Sigma (%f), SkillLow (%d), SkillHigh (%d)"),
			Mu,
			Sigma,
			CalculateConservativeSkill(Mu,Sigma),
			CalculateOptimisticSkill(Mu,Sigma));
		// Set the Mu property and add if not present
		FSettingsProperty* PropMu = SearchSettings->FindProperty(X_PROPERTY_GAMER_MU);
		if (PropMu == NULL)
		{
			INT AddIndex = SearchSettings->Properties.AddZeroed();
			PropMu = &SearchSettings->Properties(AddIndex);
			PropMu->PropertyId = X_PROPERTY_GAMER_MU;
		}
		PropMu->Data.SetData(Mu);
		// Set the Sigma property and add if not present
		FSettingsProperty* PropSigma = SearchSettings->FindProperty(X_PROPERTY_GAMER_SIGMA);
		if (PropSigma == NULL)
		{
			INT AddIndex = SearchSettings->Properties.AddZeroed();
			PropSigma = &SearchSettings->Properties(AddIndex);
			PropSigma->PropertyId = X_PROPERTY_GAMER_SIGMA;
		}
		PropSigma->Data.SetData(Sigma);
	}
	else
	{
		// Remove the special search fields if present
		for (INT PropertyIndex = 0; PropertyIndex < SearchSettings->Properties.Num(); PropertyIndex++)
		{
			FSettingsProperty& Property = SearchSettings->Properties(PropertyIndex);
			if (Property.PropertyId == X_PROPERTY_GAMER_MU ||
				Property.PropertyId == X_PROPERTY_GAMER_SIGMA)
			{
				SearchSettings->Properties.Remove(PropertyIndex);
				PropertyIndex--;
			}
		}
	}
}

/**
 * Calculates the aggregate skill from an array of skills. 
 * 
 * @param Mus - array that holds the mu values 
 * @param Sigmas - array that holds the sigma values 
 * @param OutAggregateMu - aggregate Mu
 * @param OutAggregateSigma - aggregate Sigma
 */
void UOnlineSubsystemLive::CalcAggregateSkill(const TArray<DOUBLE>& Mus,const TArray<DOUBLE>& Sigmas,DOUBLE& OutAggregateMu,DOUBLE& OutAggregateSigma)
{
	// Set to the defaults (middle of the curve, completely uncertain)
	OutAggregateMu = 3.0;
	OutAggregateSigma = 1.0;
	// Check for a game search overriding the values
	if (Mus.Num() == Sigmas.Num() && Mus.Num() > 0)
	{
		// Use the API to aggregate the skill information
		XSessionCalculateSkill(Mus.Num(),
			const_cast<DOUBLE*>(Mus.GetTypedData()),
			const_cast<DOUBLE*>(Sigmas.GetTypedData()),
			&OutAggregateMu,
			&OutAggregateSigma);
	}
}

/**
 * Generates a unique number based off of the current script compilation
 *
 * @return the unique number from the current script compilation
 */
INT UOnlineSubsystemLive::GetBuildUniqueId(void)
{
	INT Crc = 0;
	if (bUseBuildIdOverride == FALSE)
	{
		UPackage* EnginePackage = UEngine::StaticClass()->GetOutermost();
		if (EnginePackage)
		{
			FNboSerializeToBuffer Buffer(64);
			// Serialize to a NBO buffer for consistent CRCs across platforms
			Buffer << EnginePackage->Guid;
			// Now calculate the CRC
			Crc = appMemCrc((BYTE*)Buffer,Buffer.GetByteCount());
		}
	}
	else
	{
		Crc = BuildIdOverride;
	}
	return Crc;
}

/**
 * Enumerates the sessions that are set and call XSessionGetDetails() on them to
 * log Live's view of the session information
 */
void UOnlineSubsystemLive::DumpLiveSessionState(void)
{
	debugf(NAME_ScriptLog,TEXT(""));
	debugf(NAME_ScriptLog,TEXT("Live's online session state"));
	debugf(NAME_ScriptLog,TEXT("-------------------------------------------------------------"));
	debugf(NAME_ScriptLog,TEXT(""));
	debugf(NAME_ScriptLog,TEXT("Number of sessions: %d"),Sessions.Num());
	// Iterate through the sessions listing the session info that Live has
	for (INT Index = 0; Index < Sessions.Num(); Index++)
	{
		debugf(NAME_ScriptLog,TEXT("  Session: %s"),*Sessions(Index).SessionName.ToString());
		FSecureSessionInfo* SessionInfo = GetSessionInfo(&Sessions(Index));
		if (SessionInfo != NULL)
		{
			debugf(NAME_ScriptLog,TEXT("    Handle: 0x%08X"),SessionInfo->Handle);
			// Local details plus space for up to 32 players
			struct FLiveSessionDetails
			{
				XSESSION_LOCAL_DETAILS LocalDetails;
				XSESSION_MEMBER Members[32];

				/** Inits Live structures */
				inline FLiveSessionDetails(void)
				{
					// Zero this out
					appMemzero(this,sizeof(FLiveSessionDetails));
					// Point to the member buffer
					LocalDetails.pSessionMembers = Members;
				}
			};
			DWORD BufferSize = sizeof(FLiveSessionDetails);
			FLiveSessionDetails LSD;
			// Ask Live for the details, which will block and be slow
			DWORD Result = XSessionGetDetails(SessionInfo->Handle,
				&BufferSize,
				&LSD.LocalDetails,
				NULL);
			if (Result == ERROR_SUCCESS)
			{
				debugf(NAME_ScriptLog,TEXT("    GameType: 0x%08X"),LSD.LocalDetails.dwGameType);
				debugf(NAME_ScriptLog,TEXT("    GameMode: 0x%08X"),LSD.LocalDetails.dwGameMode);
				debugf(NAME_ScriptLog,TEXT("    MaxPublic: %d"),LSD.LocalDetails.dwMaxPublicSlots);
				debugf(NAME_ScriptLog,TEXT("    AvailPublic: %d"),LSD.LocalDetails.dwAvailablePublicSlots);
				debugf(NAME_ScriptLog,TEXT("    MaxPrivate: %d"),LSD.LocalDetails.dwMaxPrivateSlots);
				debugf(NAME_ScriptLog,TEXT("    AvailPrivate: %d"),LSD.LocalDetails.dwAvailablePrivateSlots);
				debugf(NAME_ScriptLog,TEXT("    NumLocalRegistrants: %d"),LSD.LocalDetails.dwActualMemberCount);
				debugf(NAME_ScriptLog,TEXT("    Nonce: 0x%016I64X"),LSD.LocalDetails.qwNonce);
				debugf(NAME_ScriptLog,TEXT("    SessionInfo->Nonce: 0x%016I64X"),SessionInfo->Nonce);
				debugf(NAME_ScriptLog,TEXT("    SessionId: 0x%016I64X"),(QWORD&)LSD.LocalDetails.sessionInfo.sessionID);
				debugf(NAME_ScriptLog,TEXT("    ArbitrationId: 0x%016I64X"),(QWORD&)LSD.LocalDetails.xnkidArbitration);
				// Build the flags string
				debugf(NAME_ScriptLog,TEXT("    Flags:"));
				if (LSD.LocalDetails.dwFlags & XSESSION_CREATE_HOST)
				{
					debugf(NAME_ScriptLog,TEXT("           XSESSION_CREATE_HOST |"));
				}
				if (LSD.LocalDetails.dwFlags & XSESSION_CREATE_USES_PEER_NETWORK)
				{
					debugf(NAME_ScriptLog,TEXT("           XSESSION_CREATE_USES_PEER_NETWORK |"));
				}
				if (LSD.LocalDetails.dwFlags & XSESSION_CREATE_USES_MATCHMAKING)
				{
					debugf(NAME_ScriptLog,TEXT("           XSESSION_CREATE_USES_MATCHMAKING |"));
				}
				if (LSD.LocalDetails.dwFlags & XSESSION_CREATE_USES_ARBITRATION)
				{
					debugf(NAME_ScriptLog,TEXT("           XSESSION_CREATE_USES_ARBITRATION |"));
				}
				if (LSD.LocalDetails.dwFlags & XSESSION_CREATE_USES_STATS)
				{
					debugf(NAME_ScriptLog,TEXT("           XSESSION_CREATE_USES_STATS |"));
				}
				if (LSD.LocalDetails.dwFlags & XSESSION_CREATE_USES_PRESENCE)
				{
					debugf(NAME_ScriptLog,TEXT("           XSESSION_CREATE_USES_PRESENCE |"));
				}
#if !WITH_PANORAMA
				if (LSD.LocalDetails.dwFlags & XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY)
				{
					debugf(NAME_ScriptLog,TEXT("           XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY |"));
				}
#endif
				if (LSD.LocalDetails.dwFlags & XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED)
				{
					debugf(NAME_ScriptLog,TEXT("           XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED |"));
				}
				if (LSD.LocalDetails.dwFlags & XSESSION_CREATE_INVITES_DISABLED)
				{
					debugf(NAME_ScriptLog,TEXT("           XSESSION_CREATE_INVITES_DISABLED |"));
				}
				if (LSD.LocalDetails.dwFlags & XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED)
				{
					debugf(NAME_ScriptLog,TEXT("           XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED |"));
				}
				// Log the Live version of the state
				switch (LSD.LocalDetails.eState)
				{
					case XSESSION_STATE_LOBBY:
					{
						debugf(NAME_ScriptLog,TEXT("    State: XSESSION_STATE_LOBBY"));
						break;
					}
					case XSESSION_STATE_REGISTRATION:
					{
						debugf(NAME_ScriptLog,TEXT("    State: XSESSION_STATE_REGISTRATION"));
						break;
					}
					case XSESSION_STATE_INGAME:
					{
						debugf(NAME_ScriptLog,TEXT("    State: XSESSION_STATE_INGAME"));
						break;
					}
					case XSESSION_STATE_REPORTING:
					{
						debugf(NAME_ScriptLog,TEXT("    State: XSESSION_STATE_REPORTING"));
						break;
					}
					case XSESSION_STATE_DELETED:
					{
						debugf(NAME_ScriptLog,TEXT("    State: XSESSION_STATE_DELETED"));
						break;
					}
				}
				debugf(NAME_ScriptLog,TEXT("    Number of players: %d"),LSD.LocalDetails.dwReturnedMemberCount);
				// List each player in the session
				for (DWORD PlayerIndex = 0; PlayerIndex < LSD.LocalDetails.dwReturnedMemberCount; PlayerIndex++)
				{
					debugf(NAME_ScriptLog,TEXT("      Player: 0x%016I64X"),LSD.Members[PlayerIndex].xuidOnline);
					debugf(NAME_ScriptLog,TEXT("        Type: %s"),LSD.Members[PlayerIndex].dwUserIndex == XUSER_INDEX_NONE ? TEXT("Remote") : TEXT("Local"));
					debugf(NAME_ScriptLog,TEXT("        Private?: %s"),LSD.Members[PlayerIndex].dwFlags & XSESSION_MEMBER_FLAGS_PRIVATE_SLOT ? TEXT("True") : TEXT("False"));
					debugf(NAME_ScriptLog,TEXT("        Zombie?: %s"),LSD.Members[PlayerIndex].dwFlags & XSESSION_MEMBER_FLAGS_ZOMBIE ? TEXT("True") : TEXT("False"));
				}
			}
			else
			{
				debugf(NAME_ScriptLog,TEXT("Failed to read session information with result 0x%08X"),Result);
			}

			XNQOSLISTENSTATS xnqls;
			xnqls.dwSizeOfStruct = sizeof(xnqls);
			Result = XNetQosGetListenStats(&LSD.LocalDetails.sessionInfo.sessionID, &xnqls);
			if (Result == ERROR_SUCCESS)
			{
				debugf(NAME_ScriptLog,TEXT("QOS Data for session:"));
				debugf(NAME_ScriptLog,TEXT("    Client data request probes received: %d"), xnqls.dwNumDataRequestsReceived);
				debugf(NAME_ScriptLog,TEXT("    Client probe requests received: %d"), xnqls.dwNumProbesReceived);
				debugf(NAME_ScriptLog,TEXT("    Client requests discarded because all slots are full: %d"), xnqls.dwNumSlotsFullDiscards);
				debugf(NAME_ScriptLog,TEXT("    Number of data replies sent: %d"), xnqls.dwNumDataRepliesSent);
				debugf(NAME_ScriptLog,TEXT("    Number of data reply bytes sent: %d"), xnqls.dwNumDataReplyBytesSent);
				debugf(NAME_ScriptLog,TEXT("    Number of probe replies sent: %d"), xnqls.dwNumProbeRepliesSent);
			}
			else
			{
				debugf(NAME_ScriptLog,TEXT("Failed to read QOS information with result 0x%08X"),Result);
			}

			UOnlineGameSettings* GameSettings = Sessions(Index).GameSettings;
			if (GameSettings != NULL)
			{
				DumpContextsAndProperties(GameSettings);
			}
		}
		debugf(NAME_ScriptLog,TEXT(""));
	}
}

/**
 * Logs the list of players that are registered for voice
 */
void UOnlineSubsystemLive::DumpVoiceRegistration(void)
{
	debugf(NAME_ScriptLog,TEXT("Voice registrants:"));
	// Iterate through local talkers
	for (INT Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
	{
		if (LocalTalkers[Index].bHasVoice)
		{
			XUID Xuid;
			GetUserXuid(Index,&Xuid);
			debugf(NAME_ScriptLog,TEXT("    Player: 0x%016I64X"),Xuid);
			debugf(NAME_ScriptLog,TEXT("        Type: Local"));
			debugf(NAME_ScriptLog,TEXT("        bHasVoice: %s"),LocalTalkers[Index].bHasVoice ? TEXT("True") : TEXT("False"));
			debugf(NAME_ScriptLog,TEXT("        bHasNetworkedVoice: %s"),LocalTalkers[Index].bHasNetworkedVoice ? TEXT("True") : TEXT("False"));
		}
	}
	// Iterate through the remote talkers listing them
	for (INT Index = 0; Index < RemoteTalkers.Num(); Index++)
	{
		FLiveRemoteTalker& Talker = RemoteTalkers(Index);
		debugf(NAME_ScriptLog,TEXT("    Player: 0x%016I64X"),Talker.TalkerId.Uid);
		debugf(NAME_ScriptLog,TEXT("        Type: Remote"));
		debugf(NAME_ScriptLog,TEXT("        bHasVoice: True"));
		debugf(NAME_ScriptLog,TEXT("        IsLocallyMuted: %s"),Talker.IsLocallyMuted() ? TEXT("True") : TEXT("False"));
		// Log out who has muted this player
		if (Talker.IsLocallyMuted())
		{
			for (INT Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
			{
				if (Talker.LocalPriorities[Index].CurrentPriority == XHV_PLAYBACK_PRIORITY_NEVER)
				{
					debugf(NAME_ScriptLog,TEXT("          MutedBy: %d"),Index);
				}
			}
		}
	}
	debugf(NAME_ScriptLog,TEXT(""));
}

/**
 * Sets the debug output level for the platform specific API (if applicable)
 *
 * @param DebugSpewLevel the level to set
 */
void UOnlineSubsystemLive::SetDebugSpewLevel(INT DebugSpewLevel)
{
#if _DEBUG && !WITH_PANORAMA
	debugf(NAME_DevOnline,TEXT("Setting debug spew to %d"),DebugSpewLevel);
	XDebugSetSystemOutputLevel(HXAMAPP_XGI,DebugSpewLevel);
//	XDebugSetSystemOutputLevel(HXAMAPP_XLIVEBASE ,DebugSpewLevel);
//	XDebugSetSystemOutputLevel(HXAMAPP_XAM,DebugSpewLevel);
//	XDebugSetSystemOutputLevel(HXAMAPP_UI,DebugSpewLevel);	
#endif
}

/**
 * Handle native property serialization
 *
 * @param Ar archive to serialize to/from
 */
void UOnlineSubsystemLive::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Only serialize if the archive collects object references 
	// e.g. garbage collection or the object reference collector.
	if (Ar.IsObjectReferenceCollector())
	{
		Ar << PlayerStorageCacheRemote;
	}	
}

/**
 * Unlocks an avatar award for the local user
 *
 * @param LocalUserNum the user to unlock the avatar item for
 * @param AvatarItemId the id of the avatar item to unlock
 */
UBOOL UOnlineSubsystemLive::UnlockAvatarAward(BYTE LocalUserNum,INT AvatarItemId)
{
	DWORD Return = E_FAIL;
#if CONSOLE
	// Validate the user index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Create a new async task to hold the data
		FUnlockAvatarAward* AsyncTask = new FUnlockAvatarAward(LocalUserNum,AvatarItemId);
		// Unlock the avatar award via Live
		Return = XUserAwardAvatarAssets(1,
			AsyncTask->GetAwardData(),
			*AsyncTask);
		debugf(NAME_DevOnline,TEXT("XUserAwardAvatarAssets() returned 0x%08X"),Return);
		// Clean up the task if it didn't succeed
		if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
		{
			// Queue the async task for ticking
			AsyncTasks.AddItem(AsyncTask);
		}
		else
		{
			delete AsyncTask;
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to UnlockAvatarAward()"),
			(DWORD)LocalUserNum);
	}
#endif
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Reads a player's save game data from the specified content bundle
 *
 * @param LocalUserNum the user that is initiating the data read (also used in validating ownership of the data)
 * @param DeviceId the device to read the same game from
 * @param FriendlyName the friendly name of the save game that was returned by enumeration
 * @param FileName the file to read from inside of the content package
 * @param SaveFileName the file name of the save game inside the content package
 *
 * @return true if the async read was started successfully, false otherwise
 */
UBOOL UOnlineSubsystemLive::ReadSaveGameData(BYTE LocalUserNum,INT DeviceId,const FString& FriendlyName,const FString& FileName,const FString& SaveFileName)
{
	DWORD Return = E_FAIL;
	// Validate the user index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
#if CONSOLE
		FOnlineSaveGame* SaveGame = FindSaveGame(LocalUserNum,DeviceId,FriendlyName,FileName);
		// If it wasn't found, then it's a new file that we need to add
		if (SaveGame == NULL)
		{
			SaveGame = AddSaveGame(LocalUserNum,DeviceId,FriendlyName,FileName);
		}
		check(SaveGame != NULL);
		// Find the existing file mapping and if not there
		FOnlineSaveGameDataMapping* SaveGameMapping = SaveGame->FindSaveGameMapping(SaveFileName);
		if (SaveGameMapping == NULL)
		{
			// Add a new mapping
			SaveGameMapping = SaveGame->AddSaveGameMapping(SaveFileName);
		}
		check(SaveGameMapping);
		// Only kick off a read if there's no data or there was an error
		if (SaveGameMapping->ReadWriteState == OERS_NotStarted ||
			SaveGameMapping->ReadWriteState == OERS_Failed)
		{
			// Handle guests or not signed in players
			if (GetLoginStatus(LocalUserNum) > LS_NotLoggedIn &&
				IsGuestLogin(LocalUserNum) == FALSE)
			{
				// Create a new async task to hold the data
				FReadSaveGameDataAsyncTask* AsyncTask = new FReadSaveGameDataAsyncTask(LocalUserNum,
					*SaveGame,
					*SaveGameMapping,
					&ContentCache[LocalUserNum].ReadSaveGameDataCompleteDelegates);
				// Start the async binding of the content package
				if (AsyncTask->BindContent())
				{
					Return = ERROR_IO_PENDING;
					// Queue the async task for ticking
					AsyncTasks.AddItem(AsyncTask);
				}
				else
				{
					delete AsyncTask;
				}
			}
			else
			{
				// Done so trigger the delegates
				Return = ERROR_SUCCESS;
				SaveGameMapping->ReadWriteState = OERS_Failed;
			}
		}
		else if (SaveGameMapping->ReadWriteState == OERS_InProgress)
		{
			// Read is in progress so don't trigger an error here
			Return = ERROR_IO_PENDING;
		}
		else if (SaveGameMapping->ReadWriteState == OERS_Done)
		{
			// Done so trigger the delegates
			Return = ERROR_SUCCESS;
		}
		// If it is done, notify the delegate
		if (Return == ERROR_SUCCESS)
		{
			// The code following this relies upon these structures being the same
			check(sizeof(OnlineSubsystemLive_eventOnReadSaveGameDataComplete_Parms) == sizeof(OnlineSubsystemLive_eventOnWriteSaveGameDataComplete_Parms));
			OnlineSubsystemLive_eventOnReadSaveGameDataComplete_Parms Parms(EC_EventParm);
			// Fill out all of the data for the file operation that completed
			Parms.LocalUserNum = LocalUserNum;
			Parms.bWasSuccessful = SaveGameMapping->ReadWriteState == OERS_Done ? FIRST_BITFIELD : 0;
			Parms.DeviceID = SaveGame->DeviceID;
			Parms.FriendlyName = SaveGame->FriendlyName;
			Parms.Filename = SaveGame->Filename;
			Parms.SaveFileName = SaveGameMapping->InternalFileName;
			// Use the common method to fire the delegates
			TriggerOnlineDelegates(this,ContentCache[LocalUserNum].ReadSaveGameDataCompleteDelegates,&Parms);
		}
#endif
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid player index (%d) specified to ReadSaveGameData()"),
			(DWORD)LocalUserNum);
	}
	if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
	{
		// The code following this relies upon these structures being the same
		check(sizeof(OnlineSubsystemLive_eventOnReadSaveGameDataComplete_Parms) == sizeof(OnlineSubsystemLive_eventOnWriteSaveGameDataComplete_Parms));
		OnlineSubsystemLive_eventOnReadSaveGameDataComplete_Parms Parms(EC_EventParm);
		// Fill out all of the data for the file operation that completed
		Parms.LocalUserNum = LocalUserNum;
		Parms.bWasSuccessful = FALSE;
		Parms.DeviceID = DeviceId;
		Parms.FriendlyName = FriendlyName;
		Parms.Filename = FileName;
		Parms.SaveFileName = SaveFileName;
		// Use the common method to fire the delegates
		TriggerOnlineDelegates(this,ContentCache[LocalUserNum].ReadSaveGameDataCompleteDelegates,&Parms);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Copies a player's save game data from the cached async read data
 *
 * @param LocalUserNum the user that is initiating the data read (also used in validating ownership of the data)
 * @param DeviceId the device to read the same game from
 * @param FriendlyName the friendly name of the save game that was returned by enumeration
 * @param FileName the file to read from inside of the content package
 * @param SaveFileName the file name of the save game inside the content package
 * @param bIsValid out value indicating whether the save is corrupt or not
 * @param SaveGameData the array that is filled with the save game data
 *
 * @return true if the async read was started successfully, false otherwise
 */
UBOOL UOnlineSubsystemLive::GetSaveGameData(BYTE LocalUserNum,INT DeviceId,const FString& FriendlyName,const FString& FileName,const FString& SaveFileName,BYTE& bIsValid,TArray<BYTE>& SaveGameData)
{
	BYTE Return = OERS_Failed;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		FOnlineSaveGame* SaveGame = FindSaveGame(LocalUserNum,DeviceId,FriendlyName,FileName);
		// If we found the file, check to see if it's done being read
		if (SaveGame != NULL)
		{
			FOnlineSaveGameDataMapping* SaveGameDataMapping = SaveGame->FindSaveGameMapping(SaveFileName);
			if (SaveGameDataMapping != NULL)
			{
				Return = SaveGameDataMapping->ReadWriteState;
				bIsValid = SaveGame->bIsValid ? 1 : 0;
				// We can only copy the data if it's done reading/writing
				if (Return == OERS_Done)
				{
					SaveGameData = SaveGameDataMapping->SaveGameData;
				}
			}
		}
		else
		{
			// Not found so log an error
			debugf(NAME_DevOnline,
				TEXT("Invalid save game (%s) with file name (%s) for user (%d) specified to GetSaveGameData()"),
				*FriendlyName,
				*SaveFileName,
				(DWORD)LocalUserNum);
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in GetSaveGameData(%d)"),
			(DWORD)LocalUserNum);
	}
	return Return == OERS_Done;
}

/**
 * Writes a player's save game data to the specified content bundle and file
 *
 * @param LocalUserNum the user that is initiating the data write
 * @param DeviceId the device to write the same game to
 * @param FriendlyName the friendly name of the save game that was returned by enumeration
 * @param FileName the file to write to inside of the content package
 * @param SaveFileName the file name of the save game inside the content package
 * @param SaveGameData the data to write to the save game file
 *
 * @return true if the async write was started successfully, false otherwise
 */
UBOOL UOnlineSubsystemLive::WriteSaveGameData(BYTE LocalUserNum,INT DeviceId,const FString& FriendlyName,const FString& FileName,const FString& SaveFileName,const TArray<BYTE>& SaveGameData)
{
	DWORD Return = E_FAIL;
	// Validate the user index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
#if CONSOLE
		FOnlineSaveGame* SaveGame = FindSaveGame(LocalUserNum,DeviceId,FriendlyName,FileName);
		// If it wasn't found, then it's a new file that we need to add
		if (SaveGame == NULL)
		{
			SaveGame = AddSaveGame(LocalUserNum,DeviceId,FriendlyName,FileName);
		}
		check(SaveGame != NULL);
		// Find the existing file mapping and if not there
		FOnlineSaveGameDataMapping* SaveGameMapping = SaveGame->FindSaveGameMapping(SaveFileName);
		if (SaveGameMapping == NULL)
		{
			// Copy the data that is to be written so the async task can happen in the background
			SaveGameMapping = SaveGame->AddSaveGameMapping(SaveFileName,SaveGameData);
		}
		check(SaveGameMapping);
		// Don't issue a write if one is in progress
		if (SaveGameMapping->ReadWriteState != OERS_InProgress)
		{
			// Handle guests or not signed in players
			if (GetLoginStatus(LocalUserNum) > LS_NotLoggedIn &&
				IsGuestLogin(LocalUserNum) == FALSE)
			{
				// Create a new async task to hold the data
				FWriteSaveGameDataAsyncTask* AsyncTask = new FWriteSaveGameDataAsyncTask(LocalUserNum,
					*SaveGame,
					*SaveGameMapping,
					&ContentCache[LocalUserNum].WriteSaveGameDataCompleteDelegates);
				// Start the async binding of the content package
				if (AsyncTask->BindContent())
				{
					Return = ERROR_IO_PENDING;
					// Queue the async task for ticking
					AsyncTasks.AddItem(AsyncTask);
				}
				else
				{
					delete AsyncTask;
				}
			}
			else
			{
				// Done so trigger the delegates
				Return = ERROR_SUCCESS;
				SaveGameMapping->ReadWriteState = OERS_Failed;
			}
		}
#endif
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid player index (%d) specified to WriteSaveGameData()"),
			(DWORD)LocalUserNum);
	}
	if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
	{
		// The code following this relies upon these structures being the same
		check(sizeof(OnlineSubsystemLive_eventOnReadSaveGameDataComplete_Parms) == sizeof(OnlineSubsystemLive_eventOnWriteSaveGameDataComplete_Parms));
		OnlineSubsystemLive_eventOnWriteSaveGameDataComplete_Parms Parms(EC_EventParm);
		// Fill out all of the data for the file operation that completed
		Parms.LocalUserNum = LocalUserNum;
		Parms.bWasSuccessful = FALSE;
		Parms.DeviceID = DeviceId;
		Parms.FriendlyName = FriendlyName;
		Parms.Filename = FileName;
		Parms.SaveFileName = SaveFileName;
		// Use the common method to fire the delegates
		TriggerOnlineDelegates(this,ContentCache[LocalUserNum].WriteSaveGameDataCompleteDelegates,&Parms);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Deletes a player's save game data
 *
 * @param LocalUserNum the user that is deleting data
 * @param FriendlyName the friendly name of the save game that was returned by enumeration
 * @param FileName the file name of the content package to delete
 *
 * @return true if the delete succeeded, false otherwise
 */
UBOOL UOnlineSubsystemLive::DeleteSaveGame(BYTE LocalUserNum,INT DeviceId,const FString& FriendlyName,const FString& FileName)
{
	DWORD Return = E_FAIL;
#if CONSOLE
	// Validate the user index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Find this file in our list and verify no async tasks are outstanding
		FOnlineSaveGame* SaveGame = FindSaveGame(LocalUserNum,DeviceId,FriendlyName,FileName);
		if (SaveGame != NULL)
		{
			if (SaveGame->AreAnySaveGamesInProgress() == FALSE)
			{
				XCONTENT_DATA ContentData;
				// Populate the content data with the save game data
				CopyOnlineSaveGameToContentData(*SaveGame,&ContentData);
				// Delete the content bundle
				Return = XContentDelete(LocalUserNum,&ContentData,NULL);
				debugf(NAME_DevOnline,
					TEXT("XContentDelete(%d) for friendly name (%s) and file name (%s) returned 0x%08X"),
					(DWORD)LocalUserNum,
					*FriendlyName,
					*FileName,
					Return);
				// Release the save game data
				SaveGame->SaveGames.Empty();
			}
			else
			{
				debugf(NAME_DevOnline,
					TEXT("Save game data for user (%d) with friendly name (%s) and file name (%s) on device (%d) is in progress, skipping delete"),
					(DWORD)LocalUserNum,
					*FriendlyName,
					*FileName,
					DeviceId);
			}
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Save game data for user (%d) with friendly name (%s) and file name (%s) on device (%d) was not found"),
				(DWORD)LocalUserNum,
				*FriendlyName,
				*FileName,
				DeviceId);
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid player index (%d) specified to DeleteSaveGameData()"),
			(DWORD)LocalUserNum);
	}
#endif
	return Return == ERROR_SUCCESS;
}

/**
 * Clears any cached save games
 *
 * @param LocalUserNum the user that is deleting data
 *
 * @return true if the clear succeeded, false otherwise
 */
UBOOL UOnlineSubsystemLive::ClearSaveGames(BYTE LocalUserNum)
{
	DWORD Return = E_FAIL;
	// Validate the user index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		if (AreAnySaveGamesInProgress(LocalUserNum) == FALSE)
		{
			ContentCache[LocalUserNum].SaveGames.Empty();
			Return = ERROR_SUCCESS;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid player index (%d) specified to ClearSaveGames()"),
			(DWORD)LocalUserNum);
	}
	return Return == ERROR_SUCCESS;
}

/**
 * Finds the specified save game
 *
 * @param LocalUser the user that owns the data
 * @param DeviceId the device to search for
 * @param FriendlyName the friendly name of the save game data
 * @param FileName the file name of the save game data
 *
 * @return a pointer to the data or NULL if not found
 */
FOnlineSaveGame* UOnlineSubsystemLive::FindSaveGame(BYTE LocalUser,INT DeviceId,const FString& FriendlyName,const FString& FileName)
{
	check(LocalUser >= 0 && LocalUser < MAX_LOCAL_PLAYERS);
	// Search through the save game cache for the data
	for (INT Index = 0; Index < ContentCache[LocalUser].SaveGames.Num(); Index++)
	{
		FOnlineSaveGame* SaveGame = &ContentCache[LocalUser].SaveGames(Index);
		// See if these match
		if (SaveGame->DeviceID == DeviceId &&
			SaveGame->FriendlyName == FriendlyName &&
			SaveGame->Filename == FileName)
		{
			return SaveGame;
		}
	}
	return NULL;
}

/**
 * Adds the specified save game
 *
 * @param LocalUser the user that owns the data
 * @param DeviceId the device to search for
 * @param FriendlyName the friendly name of the save game data
 * @param FileName the file name of the save game data
 *
 * @return a pointer to the data or NULL if not found
 */
FOnlineSaveGame* UOnlineSubsystemLive::AddSaveGame(BYTE LocalUser,INT DeviceId,const FString& FriendlyName,const FString& FileName)
{
	check(LocalUser >= 0 && LocalUser < MAX_LOCAL_PLAYERS);
	INT AddIndex = ContentCache[LocalUser].SaveGames.AddZeroed(1);
	FOnlineSaveGame* SaveGame = &ContentCache[LocalUser].SaveGames(AddIndex);
	// Set the members that it needs
	SaveGame->DeviceID = DeviceId;
	SaveGame->FriendlyName = FriendlyName;
	SaveGame->Filename = FileName;
	// Make a unique content path
	SaveGame->ContentPath = GenerateUniqueContentPath(OCT_SaveGame);
	return SaveGame;
}

/**
 * Checks the save games for a given player to see if any have async tasks outstanding
 *
 * @param LocalUser the user that owns the data
 *
 * @return true if a save game has an async task in progress, false otherwise
 */
UBOOL UOnlineSubsystemLive::AreAnySaveGamesInProgress(BYTE LocalUser) const
{
	check(LocalUser >= 0 && LocalUser < MAX_LOCAL_PLAYERS);
	// Search through the save game cache for the data
	for (INT Index = 0; Index < ContentCache[LocalUser].SaveGames.Num(); Index++)
	{
		const FOnlineSaveGame& SaveGame = ContentCache[LocalUser].SaveGames(Index);
		// See if any internal file are in progress
		if (SaveGame.AreAnySaveGamesInProgress())
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Finds the specified save game
 *
 * @param LocalUser the user that owns the data
 * @param DeviceId the device to search for
 * @param TitleId the title id the save game is from
 * @param FriendlyName the friendly name of the save game data
 * @param FileName the file name of the save game data
 *
 * @return a pointer to the data or NULL if not found
 */
FOnlineCrossTitleSaveGame* UOnlineSubsystemLive::FindCrossTitleSaveGame(BYTE LocalUser,INT DeviceId,INT TitleId,const FString& FriendlyName,const FString& FileName)
{
	check(LocalUser >= 0 && LocalUser < MAX_LOCAL_PLAYERS);
	// Search through the save game cache for the data
	for (INT Index = 0; Index < ContentCache[LocalUser].CrossTitleSaveGames.Num(); Index++)
	{
		FOnlineCrossTitleSaveGame* SaveGame = &ContentCache[LocalUser].CrossTitleSaveGames(Index);
		// See if these match
		if (SaveGame->DeviceID == DeviceId &&
			SaveGame->FriendlyName == FriendlyName &&
			SaveGame->Filename == FileName)
		{
			return SaveGame;
		}
	}
	return NULL;
}

/**
 * Adds the specified save game
 *
 * @param LocalUser the user that owns the data
 * @param DeviceId the device to search for
 * @param TitleId the title id the save game is from
 * @param FriendlyName the friendly name of the save game data
 * @param FileName the file name of the save game data
 *
 * @return a pointer to the data or NULL if not found
 */
FOnlineCrossTitleSaveGame* UOnlineSubsystemLive::AddCrossTitleSaveGame(BYTE LocalUser,INT DeviceId,INT TitleId,const FString& FriendlyName,const FString& FileName)
{
	check(LocalUser >= 0 && LocalUser < MAX_LOCAL_PLAYERS);
	INT AddIndex = ContentCache[LocalUser].CrossTitleSaveGames.AddZeroed(1);
	FOnlineCrossTitleSaveGame* SaveGame = &ContentCache[LocalUser].CrossTitleSaveGames(AddIndex);
	// Set the members that it needs
	SaveGame->DeviceID = DeviceId;
	SaveGame->TitleId = TitleId;
	SaveGame->FriendlyName = FriendlyName;
	SaveGame->Filename = FileName;
	// Make a unique content path
	SaveGame->ContentPath = GenerateUniqueContentPath(OCT_SaveGame);
	return SaveGame;
}

/**
 * Checks the save games for a given player to see if any have async tasks outstanding
 *
 * @param LocalUser the user that owns the data
 *
 * @return true if a save game has an async task in progress, false otherwise
 */
UBOOL UOnlineSubsystemLive::AreAnyCrossTitleSaveGamesInProgress(BYTE LocalUser) const
{
	check(LocalUser >= 0 && LocalUser < MAX_LOCAL_PLAYERS);
	// Search through the save game cache for the data
	for (INT Index = 0; Index < ContentCache[LocalUser].CrossTitleSaveGames.Num(); Index++)
	{
		const FOnlineCrossTitleSaveGame& SaveGame = ContentCache[LocalUser].CrossTitleSaveGames(Index);
		// See if any internal file are in progress
		if (SaveGame.AreAnySaveGamesInProgress())
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Ticks the upfront LSP caching that we do and deletes the resolve infos once they are done
 *
 * @param DeltaTime the amount of time since the last tick
 */
void UOnlineSubsystemLive::TickSecureAddressCache(FLOAT DeltaTime)
{
	// Make sure the thread cache is created in the right thread
	FSecureAddressCache::GetCache().Tick(DeltaTime);
}

/**
 * Kicks off the async LSP resolves if there are LSPs to be used with this title
 */
void UOnlineSubsystemLive::InitLspResolves(void)
{
	UBOOL bAreAnySignedIn = AreAnySignedIntoLive();
	// For each entry, kick off an async task to resolve to an ip addr
	for (INT Index = 0; Index < LspNames.Num(); Index++)
	{
		FInternetIpAddr Addr;
		UBOOL bIsCached = GSocketSubsystem->GetHostByNameFromCache(TCHAR_TO_ANSI(*LspNames(Index)),Addr);
		// If we have this already cached, clear it so we can re-resolve
		if (bIsCached &&
			bAreAnySignedIn == FALSE)
		{
			// See if there is a secure address cached
			FSecureAddressCache::GetCache().ClearSecureIpAddress(Addr);
			// Remove this name from any existing cache
			GSocketSubsystem->RemoveHostNameFromCache(TCHAR_TO_ANSI(*LspNames(Index)));
		}
		// Only try to resolve when signed in
		if (bIsCached == FALSE &&
			bAreAnySignedIn)
		{
			// Now create the new resolve task
			FResolveInfo* Resolve = GSocketSubsystem->GetHostByName(TCHAR_TO_ANSI(*LspNames(Index)));
			if (Resolve != NULL)
			{
				AsyncTasks.AddItem(new FEnumLspTask(Resolve));
			}
		}
	}
}

/**
 * Starts an async task that retrieves the list of downloaded/savegame content for the player across all titles
 *
 * @param LocalUserNum The user to read the content list of
 * @param ContentType the type of content being read
 * @param TitleId the title id to filter on. Zero means all titles
 * @param DeviceId optional value to restrict the enumeration to a particular device
 *
 * @return true if the read request was issued successfully, false otherwise
 */
UBOOL UOnlineSubsystemLive::ReadCrossTitleContentList(BYTE LocalUserNum,BYTE ContentType,INT TitleId,INT DeviceId)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	UBOOL bFireDelegate = FALSE;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Mark the right one based off of type
		BYTE& ReadState = ContentType == OCT_Downloaded ? ContentCache[LocalUserNum].ReadCrossTitleState : ContentCache[LocalUserNum].SaveGameCrossTitleReadState;
		if (ReadState != OERS_InProgress)
		{
			// If this is a save game task, make sure none of them are in progress
			if (ContentType == OCT_Downloaded ||
				AreAnySaveGamesInProgress(LocalUserNum) == FALSE)
			{
				// Throw out the old content
				ClearCrossTitleContentList(LocalUserNum,ContentType);
#if CONSOLE
				// if the user is logging in, search for any DLC
				DWORD SizeNeeded;
				HANDLE Handle;
				// return 1 at a time per XEnumerate call
				DWORD NumToRead = 1;
				// Use the specified device id if it is valid, otherwise default to any device
				XCONTENTDEVICEID XDeviceId = IsDeviceValid(DeviceId) ? DeviceId : XCONTENTDEVICE_ANY;
				// start looking for this user's content
				Return = XContentCreateCrossTitleEnumerator(ContentType == OCT_Downloaded ? XUSER_INDEX_ANY : LocalUserNum,
					XDeviceId,
					ContentType == OCT_Downloaded ? XCONTENTTYPE_MARKETPLACE : XCONTENTTYPE_SAVEDGAME, 
					0,
					NumToRead,
					&SizeNeeded,
					&Handle);
				// make sure we succeeded
				if (Return == ERROR_SUCCESS)
				{
					// Create the async data object that holds the buffers, etc (using 0 for number to retrieve for all)
					FLiveAsyncTaskCrossTitleContent* AsyncTaskData = new FLiveAsyncTaskCrossTitleContent(LocalUserNum,
						Handle,
						SizeNeeded,
						0,
						ContentType,
						TitleId);
					// Create the async task object
					FLiveAsyncTaskReadCrossTitleContent* AsyncTask = new FLiveAsyncTaskReadCrossTitleContent(
						ContentType == OCT_Downloaded ? &ContentCache[LocalUserNum].ReadCrossTitleCompleteDelegates : &ContentCache[LocalUserNum].SaveGameReadCrossTitleCompleteDelegates,
						AsyncTaskData);
					// Start the async read
					Return = XEnumerateCrossTitle(Handle,
						AsyncTaskData->GetBuffer(),
						SizeNeeded,
						0,
						*AsyncTask);
					if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
					{
						// Mark this as being read
						ReadState = OERS_InProgress;
						AsyncTasks.AddItem(AsyncTask);
					}
					else
					{
						bFireDelegate = TRUE;
						// Delete the async task
						delete AsyncTask;
					}
				}
				else
				{
					debugf(NAME_DevOnline,
						TEXT("XContentCreateEnumerator(%d, XCONTENTDEVICE_ANY, XCONTENTTYPE_MARKETPLACE, 0, 1, &BufferSize, &EnumerateHandle) failed with 0x%08X"),
						LocalUserNum,
						Return);
				}
				// Content list might be empty
				if (Return == ERROR_NO_MORE_FILES)
				{
					bFireDelegate = TRUE;
					Return = ERROR_SUCCESS;
					// Set it to done, since there is nothing to read
					ReadState = OERS_Done;
				}
				else if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
				{
					bFireDelegate = TRUE;
					ReadState = OERS_Failed;
				}
#endif
			}
			else
			{
				debugf(NAME_DevOnline,
					TEXT("ReadCrossTitleContentList(%d,%d,%d,%d) failed since a save game read/write is in progress"),
					(DWORD)LocalUserNum,
					(DWORD)ContentType,
					(DWORD)TitleId,
					(DWORD)DeviceId);
				bFireDelegate = TRUE;
				ReadState = OERS_Failed;
			}
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Ignoring call to ReadCrossTitleContentList(%d,%d,%d,%d) since one is already in progress"),
				(DWORD)LocalUserNum,
				(DWORD)ContentType,
				(DWORD)TitleId,
				(DWORD)DeviceId);
			Return = ERROR_IO_PENDING;
		}
		// Fire off the delegate if needed
		if (bFireDelegate)
		{
			FAsyncTaskDelegateResults Results(Return);
			TriggerOnlineDelegates(this,
				ContentType == OCT_Downloaded ? ContentCache[LocalUserNum].ReadCrossTitleCompleteDelegates : ContentCache[LocalUserNum].SaveGameReadCrossTitleCompleteDelegates,
				&Results);
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in ReadCrossTitleContentList(%d)"),
			(DWORD)LocalUserNum);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Starts an async task that frees any downloaded content resources for that player
 *
 * @param LocalUserNum The user to clear the content list for
 * @param ContentType the type of content being read
 */
void UOnlineSubsystemLive::ClearCrossTitleContentList(BYTE LocalUserNum,BYTE ContentType)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		if (ContentType == OCT_Downloaded)
		{
#if CONSOLE
			// Close all opened content bundles
			for (INT DlcIndex = 0; DlcIndex < ContentCache[LocalUserNum].CrossTitleContent.Num(); DlcIndex++)
			{
				FOnlineAsyncTaskLive* Task = new FOnlineAsyncTaskLive(NULL,NULL,TEXT("Cross Title DLC XContentClose"));
				DWORD Result = XContentClose(TCHAR_TO_ANSI(*ContentCache[LocalUserNum].CrossTitleContent(DlcIndex).ContentPath),*Task);
				debugf(NAME_DevOnline,
					TEXT("Freeing Cross Title DLC (%s) returned 0x%08X"),
					*ContentCache[LocalUserNum].CrossTitleContent(DlcIndex).FriendlyName,
					Result);
				AsyncTasks.AddItem(Task);
			}
#endif
			// Can't read DLC on PC, so empty out and mark as not started
			ContentCache[LocalUserNum].CrossTitleContent.Empty();
			ContentCache[LocalUserNum].ReadCrossTitleState = OERS_NotStarted;
		}
		else
		{
#if CONSOLE
			// Close all opened content bundles
			for (INT SaveGameIndex = 0; SaveGameIndex < ContentCache[LocalUserNum].CrossTitleSaveGameContent.Num(); SaveGameIndex++)
			{
				FOnlineAsyncTaskLive* Task = new FOnlineAsyncTaskLive(NULL,NULL,TEXT("Cross Title SaveGame XContentClose"));
				DWORD Result = XContentClose(TCHAR_TO_ANSI(*ContentCache[LocalUserNum].CrossTitleSaveGameContent(SaveGameIndex).ContentPath),*Task);
				debugf(NAME_DevOnline,
					TEXT("Freeing Cross Title SaveGame (%s) returned 0x%08X"),
					*ContentCache[LocalUserNum].CrossTitleSaveGameContent(SaveGameIndex).FriendlyName,
					Result);
				AsyncTasks.AddItem(Task);
			}
#endif
			ContentCache[LocalUserNum].CrossTitleSaveGameContent.Empty();
			ContentCache[LocalUserNum].SaveGameCrossTitleReadState = OERS_NotStarted;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in ClearCrossTitleContentList(%d,%d)"),
			(DWORD)LocalUserNum,
			(DWORD)ContentType);
	}
}

/**
 * Retrieve the list of content the given user has downloaded or otherwise retrieved
 * to the local console.
 
 * @param LocalUserNum The user to read the content list of
 * @param ContentType the type of content being read
 * @param ContentList The out array that receives the list of all content
 *
 * @return OERS_Done if the read has completed, otherwise one of the other states
 */
BYTE UOnlineSubsystemLive::GetCrossTitleContentList(BYTE LocalUserNum,BYTE ContentType,TArray<FOnlineCrossTitleContent>& ContentList)
{
	BYTE Return = OERS_Failed;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		if (ContentType == OCT_Downloaded)
		{
			// Check to see if the last DLC read request has completed
			Return = ContentCache[LocalUserNum].ReadCrossTitleState;
			if (Return == OERS_Done)
			{
				ContentList = ContentCache[LocalUserNum].CrossTitleContent;
			}
		}
		else
		{
			// Check to see if the last save game read request has completed
			Return = ContentCache[LocalUserNum].SaveGameCrossTitleReadState;
			if (Return == OERS_Done)
			{
				ContentList = ContentCache[LocalUserNum].CrossTitleSaveGameContent;
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in GetCrossTitleContentList(%d,%d)"),
			(DWORD)LocalUserNum,
			(DWORD)ContentType);
	}
	return Return;
}

/**
 * Reads a player's cross title save game data from the specified content bundle
 *
 * @param LocalUserNum the user that is initiating the data read (also used in validating ownership of the data)
 * @param DeviceId the device to read the same game from
 * @param FriendlyName the friendly name of the save game that was returned by enumeration
 * @param FileName the file to read from inside of the content package
 * @param SaveFileName the file name of the save game inside the content package
 *
 * @return true if the async read was started successfully, false otherwise
 */
UBOOL UOnlineSubsystemLive::ReadCrossTitleSaveGameData(BYTE LocalUserNum,INT DeviceId,INT TitleId,const FString& FriendlyName,const FString& FileName,const FString& SaveFileName)
{
	DWORD Return = E_FAIL;
	// Validate the user index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
#if CONSOLE
		FOnlineCrossTitleSaveGame* SaveGame = FindCrossTitleSaveGame(LocalUserNum,DeviceId,TitleId,FriendlyName,FileName);
		// If it wasn't found, then it's a new file that we need to add
		if (SaveGame == NULL)
		{
			SaveGame = AddCrossTitleSaveGame(LocalUserNum,DeviceId,TitleId,FriendlyName,FileName);
		}
		check(SaveGame != NULL);
		// Find the existing file mapping and if not there
		FOnlineSaveGameDataMapping* SaveGameMapping = SaveGame->FindSaveGameMapping(SaveFileName);
		if (SaveGameMapping == NULL)
		{
			// Add a new mapping
			SaveGameMapping = SaveGame->AddSaveGameMapping(SaveFileName);
		}
		check(SaveGameMapping);
		// Only kick off a read if there's no data or there was an error
		if (SaveGameMapping->ReadWriteState == OERS_NotStarted ||
			SaveGameMapping->ReadWriteState == OERS_Failed)
		{
			// Create a new async task to hold the data
			FReadCrossTitleSaveGameDataAsyncTask* AsyncTask = new FReadCrossTitleSaveGameDataAsyncTask(LocalUserNum,
				*SaveGame,
				*SaveGameMapping,
				&ContentCache[LocalUserNum].ReadCrossTitleSaveGameDataCompleteDelegates);
			// Start the async binding of the content package
			if (AsyncTask->BindContent())
			{
				Return = ERROR_IO_PENDING;
				// Queue the async task for ticking
				AsyncTasks.AddItem(AsyncTask);
			}
			else
			{
				delete AsyncTask;
			}
		}
		else if (SaveGameMapping->ReadWriteState == OERS_InProgress)
		{
			// Read is in progress so don't trigger an error here
			Return = ERROR_IO_PENDING;
		}
		else if (SaveGameMapping->ReadWriteState == OERS_Done)
		{
			// Done so trigger the delegates
			Return = ERROR_SUCCESS;
			OnlineSubsystemLive_eventOnReadCrossTitleSaveGameDataComplete_Parms Parms(EC_EventParm);
			// Fill out all of the data for the file operation that completed
			Parms.LocalUserNum = LocalUserNum;
			Parms.bWasSuccessful = FIRST_BITFIELD;
			Parms.DeviceID = SaveGame->DeviceID;
			Parms.FriendlyName = SaveGame->FriendlyName;
			Parms.Filename = SaveGame->Filename;
			Parms.SaveFileName = SaveGameMapping->InternalFileName;
			// Use the common method to fire the delegates
			TriggerOnlineDelegates(this,ContentCache[LocalUserNum].ReadCrossTitleSaveGameDataCompleteDelegates,&Parms);
		}
#endif
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid player index (%d) specified to ReadSaveGameData()"),
			(DWORD)LocalUserNum);
	}
	if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
	{
		OnlineSubsystemLive_eventOnReadCrossTitleSaveGameDataComplete_Parms Parms(EC_EventParm);
		// Fill out all of the data for the file operation that completed
		Parms.LocalUserNum = LocalUserNum;
		Parms.bWasSuccessful = FALSE;
		Parms.DeviceID = DeviceId;
		Parms.FriendlyName = FriendlyName;
		Parms.Filename = FileName;
		Parms.SaveFileName = SaveFileName;
		// Use the common method to fire the delegates
		TriggerOnlineDelegates(this,ContentCache[LocalUserNum].ReadCrossTitleSaveGameDataCompleteDelegates,&Parms);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Copies a player's cross title save game data from the cached async read data
 *
 * @param LocalUserNum the user that is initiating the data read (also used in validating ownership of the data)
 * @param DeviceId the device to read the same game from
 * @param TitleId the title id the save game is from
 * @param FriendlyName the friendly name of the save game that was returned by enumeration
 * @param FileName the file to read from inside of the content package
 * @param SaveFileName the file name of the save game inside the content package
 * @param bIsValid out value indicating whether the save is corrupt or not
 * @param SaveGameData the array that is filled with the save game data
 *
 * @return true if the async read was started successfully, false otherwise
 */
UBOOL UOnlineSubsystemLive::GetCrossTitleSaveGameData(BYTE LocalUserNum,INT DeviceId,int TitleId,const FString& FriendlyName,const FString& FileName,const FString& SaveFileName,BYTE& bIsValid,TArray<BYTE>& SaveGameData)
{
	BYTE Return = OERS_Failed;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		FOnlineCrossTitleSaveGame* SaveGame = FindCrossTitleSaveGame(LocalUserNum,DeviceId,TitleId,FriendlyName,FileName);
		// If we found the file, check to see if it's done being read
		if (SaveGame != NULL)
		{
			FOnlineSaveGameDataMapping* SaveGameDataMapping = SaveGame->FindSaveGameMapping(SaveFileName);
			if (SaveGameDataMapping != NULL)
			{
				Return = SaveGameDataMapping->ReadWriteState;
				bIsValid = SaveGame->bIsValid ? 1 : 0;
				// We can only copy the data if it's done reading/writing
				if (Return == OERS_Done)
				{
					SaveGameData = SaveGameDataMapping->SaveGameData;
				}
			}
		}
		else
		{
			// Not found so log an error
			debugf(NAME_DevOnline,
				TEXT("Invalid save game (%s) with file name (%s) for user (%d) specified to GetSaveGameData()"),
				*FriendlyName,
				*SaveFileName,
				(DWORD)LocalUserNum);
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid user specified in GetSaveGameData(%d)"),
			(DWORD)LocalUserNum);
	}
	return Return == OERS_Done;
}

/**
 * Clears any cached save games
 *
 * @param LocalUserNum the user that is deleting data
 *
 * @return true if the clear succeeded, false otherwise
 */
UBOOL UOnlineSubsystemLive::ClearCrossTitleSaveGames(BYTE LocalUserNum)
{
	DWORD Return = E_FAIL;
	// Validate the user index
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		if (AreAnyCrossTitleSaveGamesInProgress(LocalUserNum) == FALSE)
		{
			ContentCache[LocalUserNum].CrossTitleSaveGames.Empty();
			Return = ERROR_SUCCESS;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid player index (%d) specified to ClearCrossTitleSaveGames()"),
			(DWORD)LocalUserNum);
	}
	return Return == ERROR_SUCCESS;
}

/**
 * Reads the online profile settings for a given user and title id
 *
 * @param LocalUserNum the user that we are reading the data for
 * @param TitleId the title that the profile settings are being read for
 * @param ProfileSettings the object to copy the results to and contains the list of items to read
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineSubsystemLive::ReadCrossTitleProfileSettings(BYTE LocalUserNum,INT TitleId,UOnlineProfileSettings* ProfileSettings)
{
 	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Find the mapping of title id to profile
		FCrossTitleProfileEntry* CacheEntry = ProfileCache[LocalUserNum].FindCrossTitleProfileEntry(TitleId);
		if (CacheEntry == NULL)
		{
			CacheEntry = ProfileCache[LocalUserNum].AddCrossTitleProfileEntry(TitleId);
		}
		check(CacheEntry);
		// Only read if we don't have a profile for this player
		if (CacheEntry->Profile == NULL)
		{
			if (ProfileSettings != NULL)
			{
				CacheEntry->Profile = ProfileSettings;
				ProfileSettings->AsyncState = OPAS_Read;
				// Clear the previous set of results
				ProfileSettings->ProfileSettings.Empty();
				// Make sure the version number is requested
				ProfileSettings->AppendVersionToReadIds();
				// If they are not logged in, give them all the defaults
				XUSER_SIGNIN_INFO SigninOnline;
				// Skip the write if the user isn't signed in
				if (XUserGetSigninInfo(LocalUserNum,XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY,&SigninOnline) == ERROR_SUCCESS &&
					// Treat guests as getting the defaults
					IsGuestLogin(LocalUserNum) == FALSE)
				{
					DWORD NumIds = ProfileSettings->ProfileSettingIds.Num();
					DWORD* ProfileIds = (DWORD*)ProfileSettings->ProfileSettingIds.GetData();
					// Create the read buffer
					FLiveAsyncTaskDataReadProfileSettings* ReadData = new FLiveAsyncTaskDataReadProfileSettings(LocalUserNum,NumIds);
					// Copy the IDs for later use when inspecting the game settings blobs
					appMemcpy(ReadData->GetIds(),ProfileIds,sizeof(DWORD) * NumIds);
					// Create a new async task for handling the async notification
					FOnlineAsyncTaskLive* AsyncTask = new FLiveAsyncTaskReadCrossTitleProfileSettings(
						&ProfileCache[LocalUserNum].CrossTitleReadDelegates,
						ReadData,
						SigninOnline.xuid,
						TitleId);
					// Tell Live the size of our buffer
					DWORD SizeNeeded = ReadData->GetGameSettingsSize();
					// Start by reading the game settings fields
					Return = XUserReadProfileSettings(TitleId,
						LocalUserNum,
						ReadData->GetGameSettingsIdsCount(),
						ReadData->GetGameSettingsIds(),
						&SizeNeeded,
						ReadData->GetGameSettingsBuffer(),
						*AsyncTask);
					if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
					{
						// Queue the async task for ticking
						AsyncTasks.AddItem(AsyncTask);
					}
					else
					{
						// Just trigger the delegate as having failed
						OnlineSubsystemLive_eventOnReadCrossTitleProfileSettingsComplete_Parms Results(EC_EventParm);
						Results.LocalUserNum = LocalUserNum;
						Results.TitleId = TitleId;
						Results.bWasSuccessful = FALSE;
						TriggerOnlineDelegates(this,ProfileCache[LocalUserNum].CrossTitleReadDelegates,&Results);
						delete AsyncTask;
						CacheEntry->Profile = NULL;
					}
					debugf(NAME_DevOnline,
						TEXT("XUserReadProfileSettings(%d,%d,3,GameSettingsIds,%d,data,data) returned 0x%08X"),
						TitleId,
						LocalUserNum,
						SizeNeeded,
						Return);
				}
				else
				{
					debugfLiveSlow(NAME_DevOnline,
						TEXT("User (%d) not logged in or is a guest, skipping read"),
						(DWORD)LocalUserNum);
					ProfileSettings->AsyncState = OPAS_Finished;
					// Just trigger the delegate as having succeeded
					OnlineSubsystemLive_eventOnReadCrossTitleProfileSettingsComplete_Parms Results(EC_EventParm);
					Results.LocalUserNum = LocalUserNum;
					Results.TitleId = TitleId;
					Results.bWasSuccessful = FALSE;
					TriggerOnlineDelegates(this,ProfileCache[LocalUserNum].CrossTitleReadDelegates,&Results);
				}
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Can't specify a null profile settings object"));
			}
		}
		// Make sure the profile isn't already being read, since this is going to
		// complete immediately
		else if (CacheEntry->Profile->AsyncState != OPAS_Read)
		{
			debugfLiveSlow(NAME_DevOnline,TEXT("Using cached profile data instead of reading"));
			// If the specified read isn't the same as the cached object, copy the
			// data from the cache
			if (CacheEntry->Profile != ProfileSettings)
			{
				ProfileSettings->ProfileSettings = CacheEntry->Profile->ProfileSettings;
				CacheEntry->Profile = ProfileSettings;
			}
			// Just trigger the read delegate as being done
			// Send the notification of completion
			OnlineSubsystemLive_eventOnReadCrossTitleProfileSettingsComplete_Parms Results(EC_EventParm);
			Results.LocalUserNum = LocalUserNum;
			Results.TitleId = TitleId;
			Results.bWasSuccessful = FIRST_BITFIELD;
			TriggerOnlineDelegates(this,ProfileCache[LocalUserNum].CrossTitleReadDelegates,&Results);
			Return = ERROR_SUCCESS;
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Profile read for player (%d) is already in progress"),LocalUserNum);
			// Just trigger the read delegate as failed
			OnlineSubsystemLive_eventOnReadCrossTitleProfileSettingsComplete_Parms Results(EC_EventParm);
			Results.LocalUserNum = LocalUserNum;
			Results.TitleId = TitleId;
			Results.bWasSuccessful = FALSE;
			TriggerOnlineDelegates(this,ProfileCache[LocalUserNum].CrossTitleReadDelegates,&Results);
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid player specified to ReadCrossTitleProfileSettings(%d)"),
			LocalUserNum);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

/**
 * Queries the social networking features that the title is allowed to use.
 *
 * @return true if the async task was successfully started, false otherwise
 */
UBOOL UOnlineSubsystemLive::QuerySocialPostPrivileges()
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;

	if (AreAnySignedIntoLive())
	{
#if 0 // requires XDK 20764
		// Create the async task for querying social privilege settings
		FLiveAsyncTaskQuerySocialPostPrivileges* AsyncTask = new FLiveAsyncTaskQuerySocialPostPrivileges(&QuerySocialPostPrivilegesDelegates);
		// Query for the capability flags
		Return = XSocialGetCapabilities(
			AsyncTask->GetCapabilityFlagsPtr(),
			*AsyncTask);
		
		debugf(NAME_DevOnline,
			TEXT("XSocialGetCapabilities() returned 0x%08X"),
			Return);
		if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
		{
			// Queue the async task for ticking
			AsyncTasks.AddItem(AsyncTask);
		}
		else
		{
			delete AsyncTask;
		}
#else
		debugf(NAME_DevOnline,
			TEXT("XSocialGetCapabilities() not supported"));
#endif
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Must have a player signed in to query social privileges."));
	}
	if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
	{
		OnlineSubsystemLive_eventOnQuerySocialPostPrivilegesCompleted_Parms Params(EC_EventParm);
		TriggerOnlineDelegates(this,QuerySocialPostPrivilegesDelegates,&Params);
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

/**
 * Iterates the list of delegates and fires those notifications
 *
 * @param Object the object that the notifications are going to be issued on
 */
void FLiveAsyncTaskQuerySocialPostPrivileges::TriggerDelegates(UObject* Object)
{
	check(Object);
	// Only fire off the events if there are some registered
	if (ScriptDelegates != NULL &&
		ScriptDelegates->Num() > 0)
	{
		OnlineSubsystemLive_eventOnQuerySocialPostPrivilegesCompleted_Parms Params(EC_EventParm);
		Params.bWasSuccessful = GetCompletionCode() == ERROR_SUCCESS ? FIRST_BITFIELD : 0;
#if 0 // requires XDK 20764
		Params.PostPrivileges.bCanPostImage = CapabilityFlags & XSOCIAL_CAPABILITY_POSTIMAGE ? TRUE : FALSE;
		Params.PostPrivileges.bCanPostLink = CapabilityFlags & XSOCIAL_CAPABILITY_POSTLINK ? TRUE : FALSE;
#endif
		TriggerOnlineDelegates(Object,*ScriptDelegates,&Params);
	}	
}

UBOOL UOnlineSubsystemLive::PostImage(BYTE LocalUserNum,const struct FSocialPostImageInfo& PostImageInfo,const TArray<BYTE>& FullImage)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Make sure they are signed in so they can post to Live
		if (GetLoginStatus(LocalUserNum) == LS_LoggedIn &&
			IsGuestLogin(LocalUserNum) == FALSE)
		{
#if 0 // requires XDK 20764
			XSOCIAL_IMAGEPOSTPARAMS ImagePostParams;
			appMemzero(&ImagePostParams,sizeof(XSOCIAL_IMAGEPOSTPARAMS));
			ImagePostParams.Size = sizeof(XSOCIAL_IMAGEPOSTPARAMS);
 			ImagePostParams.MessageText = *PostImageInfo.MessageText;
 			ImagePostParams.TitleText = *PostImageInfo.TitleText;
 			ImagePostParams.PictureCaption = *PostImageInfo.PictureCaption;
 			ImagePostParams.PictureDescription = *PostImageInfo.PictureDescription;
			//ImagePostParams.PreviewImage;
			ImagePostParams.pFullImage = FullImage.GetData();
			ImagePostParams.FullImageByteCount = FullImage.Num();
			ImagePostParams.PreviewImage.pBytes = FullImage.GetData();
			ImagePostParams.PreviewImage.Pitch = 0;
			ImagePostParams.PreviewImage.Height = 0;
			ImagePostParams.PreviewImage.Width = 0;
			ImagePostParams.Flags |= PostImageInfo.Flags.bIsUserGeneratedImage ? XSOCIAL_POST_USERGENERATEDCONTENT : 0;
			ImagePostParams.Flags |= PostImageInfo.Flags.bIsGameGeneratedImage ? XSOCIAL_POST_GAMECONTENT : 0;
			ImagePostParams.Flags |= PostImageInfo.Flags.bIsAchievementImage ? XSOCIAL_POST_ACHIEVEMENTCONTENT : 0;
			ImagePostParams.Flags |= PostImageInfo.Flags.bIsMediaImage ? XSOCIAL_POST_MEDIACONTENT : 0;

			debugf(NAME_DevOnline,TEXT("PostImage: MessageText=%s TitleText=%s PictureCaption=%s PictureDescription=%s ImageBytes=%0.2fKB"),
				*PostImageInfo.MessageText,
				*PostImageInfo.TitleText,
				*PostImageInfo.PictureCaption,
				*PostImageInfo.PictureDescription,
				FullImage.Num()/1024.f);

			// Query for the capability flags
			Return = XShowSocialNetworkImagePostUI(
				LocalUserNum,
				&ImagePostParams,
				NULL);

			debugf(NAME_DevOnline,
				TEXT("XShowSocialNetworkImagePostUI() returned 0x%08X"),
				Return);

			if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
			{
				// Queue the async task for ticking
				//AsyncTasks.AddItem(AsyncTask);

				//@debug sz
				OnlineSubsystemLive_eventOnPostImageCompleted_Parms Params(EC_EventParm);
				Params.LocalUserNum = LocalUserNum;
				Params.bWasSuccessful = FIRST_BITFIELD;
				TriggerOnlineDelegates(this,PerUserDelegates[LocalUserNum].PostImageDelegates,&Params);
			}
			else
			{
				//delete AsyncTask;
			}
#else
		debugf(NAME_DevOnline,
			TEXT("XShowSocialNetworkImagePostUI() not supported"));
#endif
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Non-signed in or guest player specified to PostImage(%d)"),
				LocalUserNum);
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid player specified to PostImage(%d)"),
			LocalUserNum);
	}	
	if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
	{
		OnlineSubsystemLive_eventOnPostImageCompleted_Parms Params(EC_EventParm);
		Params.LocalUserNum = LocalUserNum;
		Params.bWasSuccessful = FALSE;
		TriggerOnlineDelegates(this,PerUserDelegates[LocalUserNum].PostImageDelegates,&Params);
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

UBOOL UOnlineSubsystemLive::PostLink(BYTE LocalUserNum,const struct FSocialPostLinkInfo& PostLinkInfo)
{
	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Make sure they are signed in so they can post to Live
		if (GetLoginStatus(LocalUserNum) == LS_LoggedIn &&
			IsGuestLogin(LocalUserNum) == FALSE)
		{
#if 0 // requires XDK 20764
			XSOCIAL_LINKPOSTPARAMS LinkPostParams;
			appMemzero(&LinkPostParams,sizeof(XSOCIAL_LINKPOSTPARAMS));
			LinkPostParams.Size = sizeof(XSOCIAL_LINKPOSTPARAMS);
 			LinkPostParams.MessageText = *PostLinkInfo.MessageText;
 			LinkPostParams.TitleText = *PostLinkInfo.TitleText;
			LinkPostParams.TitleURL = *PostLinkInfo.TitleURL;
 			LinkPostParams.PictureCaption = *PostLinkInfo.PictureCaption;
 			LinkPostParams.PictureDescription = *PostLinkInfo.PictureDescription;
			LinkPostParams.PictureURL = *PostLinkInfo.PictureURL;
			LinkPostParams.PreviewImage.pBytes = NULL;
			LinkPostParams.PreviewImage.Pitch = 0;
			LinkPostParams.PreviewImage.Height = 0;
			LinkPostParams.PreviewImage.Width = 0;
			LinkPostParams.Flags |= PostLinkInfo.Flags.bIsUserGeneratedImage ? XSOCIAL_POST_USERGENERATEDCONTENT : 0;
			LinkPostParams.Flags |= PostLinkInfo.Flags.bIsGameGeneratedImage ? XSOCIAL_POST_GAMECONTENT : 0;
			LinkPostParams.Flags |= PostLinkInfo.Flags.bIsAchievementImage ? XSOCIAL_POST_ACHIEVEMENTCONTENT : 0;
			LinkPostParams.Flags |= PostLinkInfo.Flags.bIsMediaImage ? XSOCIAL_POST_MEDIACONTENT : 0;

			debugf(NAME_DevOnline,TEXT("PostLink: MessageText=%s TitleText=%s TitleURL=%s PictureCaption=%s PictureDescription=%s PictureURL=%s"),
				*PostLinkInfo.MessageText,
				*PostLinkInfo.TitleText,
				*PostLinkInfo.TitleURL,
				*PostLinkInfo.PictureCaption,
				*PostLinkInfo.PictureDescription,
				*PostLinkInfo.PictureURL);

			// Query for the capability flags
			Return = XShowSocialNetworkLinkPostUI(
				LocalUserNum,
				&LinkPostParams,
				NULL);

			debugf(NAME_DevOnline,
				TEXT("XShowSocialNetworkLinkPostUI() returned 0x%08X"),
				Return);

			if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
			{
				// Queue the async task for ticking
				//AsyncTasks.AddItem(AsyncTask);

				//@debug sz
				OnlineSubsystemLive_eventOnPostLinkCompleted_Parms Params(EC_EventParm);
				Params.LocalUserNum = LocalUserNum;
				Params.bWasSuccessful = FIRST_BITFIELD;
				TriggerOnlineDelegates(this,PerUserDelegates[LocalUserNum].PostLinkDelegates,&Params);
			}
			else
			{
				//delete AsyncTask;
			}
#else
		debugf(NAME_DevOnline,
			TEXT("XShowSocialNetworkLinkPostUI() not supported"));
#endif
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Non-signed in or guest player specified to PostLink(%d)"),
				LocalUserNum);
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid player specified to PostLink(%d)"),
			LocalUserNum);
	}
	if (Return != ERROR_SUCCESS && Return != ERROR_IO_PENDING)
	{
		OnlineSubsystemLive_eventOnPostLinkCompleted_Parms Params(EC_EventParm);
		Params.LocalUserNum = LocalUserNum;
		Params.bWasSuccessful = FALSE;
		TriggerOnlineDelegates(this,PerUserDelegates[LocalUserNum].PostLinkDelegates,&Params);
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

/**
 * Shows a dialog with the message pre-populated in it
 *
 * @param LocalUserNum the user sending the message
 * @param Recipients the list of people to send the message to
 * @param MessageTitle the title of the message being sent
 * @param NonEditableMessage the portion of the message that the user cannot edit
 * @param EditableMessage the portion of the message the user can edit
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemLive::ShowCustomMessageUI(BYTE LocalUserNum,const TArray<FUniqueNetId>& Recipients,const FString& MessageTitle,const FString& NonEditableMessage,const FString& EditableMessage)
{
 	DWORD Return = XONLINE_E_SESSION_WRONG_STATE;
	if (LocalUserNum >= 0 && LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Make sure they are signed in so they can send messages
		if (GetLoginStatus(LocalUserNum) == LS_LoggedIn &&
			IsGuestLogin(LocalUserNum) == FALSE)
		{
#if CONSOLE
			// Create the object that will hold onto the data for the lifetime of the call
			FLiveAsyncTaskCustomMessage* AsyncTask = new FLiveAsyncTaskCustomMessage(Recipients,
				// Max of 30 chars or it asserts
				MessageTitle.Left(30),
				// Max of 96 chars or it asserts
				NonEditableMessage.Left(96),
				// Max of 256 chars or it asserts
				EditableMessage.Left(256),
				CloseGuideString,
				DeleteMessageString);
			// Display the message for the user
			Return = XShowCustomMessageComposeUI(LocalUserNum,
				 AsyncTask->GetPlayers(),
				 AsyncTask->GetPlayerCount(),
				 0,
				 AsyncTask->GetTitle(),
				 AsyncTask->GetNonEditableMessage(),
				 AsyncTask->GetEditableMessage(),
				 NULL,
				 0,
				 AsyncTask->GetActions(),
				 AsyncTask->GetActionCount(),
				 // @todo joeg -- support this for sending more interesting data across
				 NULL,
				 0,
				 0,
				 *AsyncTask);
			if (Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING)
			{
				// Queue the async task for ticking
				AsyncTasks.AddItem(AsyncTask);
			}
			debugf(NAME_DevOnline,
				TEXT("XShowCustomMessageComposeUI(%d,Players,%d,\"%s\") returned 0x%08X"),
				(DWORD)LocalUserNum,
				Recipients.Num(),
				*MessageTitle,
				Return);
#endif
		}
		else
		{
			debugf(NAME_DevOnline,
				TEXT("Non-signed in or guest player specified to ShowCustomMessageUI(%d)"),
				LocalUserNum);
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Invalid player specified to ShowCustomMessageUI(%d)"),
			LocalUserNum);
	}
	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

#endif	//#if WITH_UE3_NETWORKING
