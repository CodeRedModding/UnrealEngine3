/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "OnlineSubsystemPC.h"

#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(UOnlineSubsystemPC);

/**
 * PC specific implementation. Sets the supported interface pointers and
 * initilizes the voice engine
 *
 * @return always returns TRUE
 */
UBOOL UOnlineSubsystemPC::Init(void)
{
	// Set the player interface to be the same as the object
	eventSetPlayerInterface(this);
	// Construct the object that handles the game interface for us
	GameInterfaceImpl = ConstructObject<UOnlineGameInterfaceImpl>(UOnlineGameInterfaceImpl::StaticClass(),this);
	if (GameInterfaceImpl)
	{
		GameInterfaceImpl->OwningSubsystem = this;
		// Set the game interface to be the same as the object
		eventSetGameInterface(GameInterfaceImpl);
	}
	// Set the stats reading/writing interface
	eventSetStatsInterface(this);
	eventSetSystemInterface(this);
	extern FVoiceInterface* appCreateVoiceInterfacePC(INT MaxLocalTalkers,INT MaxRemoteTalkers,UBOOL bIsSpeechRecognitionDesired);
	// Create the voice engine and if successful register the interface
	VoiceEngine = appCreateVoiceInterfacePC(MaxLocalTalkers,MaxRemoteTalkers,
		bIsUsingSpeechRecognition);
	if (VoiceEngine != NULL)
	{
		// Set the voice interface to this object
		eventSetVoiceInterface(this);
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Failed to create the voice interface"));
	}
	// Use web requests for downloading title files
	UOnlineTitleFileDownloadWeb* TitleFileObject = ConstructObject<UOnlineTitleFileDownloadWeb>(UOnlineTitleFileDownloadWeb::StaticClass(),this);
	TitleFileObject->eventInit();
	eventSetTitleFileInterface(TitleFileObject);
	// Add interface for caching downloaded files to disk
	UTitleFileDownloadCache* TitleFileCacheObject = ConstructObject<UTitleFileDownloadCache>(UTitleFileDownloadCache::StaticClass(),this);
	eventSetTitleFileCacheInterface(TitleFileCacheObject);
	// Use web requests for downloading user files
	UMcpUserCloudFileDownload* UserCloudFileDownload = ConstructObject<UMcpUserCloudFileDownload>(UMcpUserCloudFileDownload::StaticClass(),this);
	UserCloudFileDownload->eventInit();
	eventSetUserCloudInterface(UserCloudFileDownload);
	// Handle a missing profile data directory specification
	if (ProfileDataDirectory.Len() == 0)
	{
		ProfileDataDirectory = TEXT(".\\");
	}
	return GameInterfaceImpl != NULL;
}

/**
 * Handles updating of any async tasks that need to be performed
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UOnlineSubsystemPC::Tick(FLOAT DeltaTime)
{
	// Tick any async tasks that may need to notify their delegates
	TickAsyncTasks();
	// Tick any tasks needed for LAN/networked game support
	TickGameInterfaceTasks(DeltaTime);
//@todo joeg -- Move into TickVoice() once there is network support for this
	if (VoiceEngine)
	{
		VoiceEngine->Tick(DeltaTime);
	}
}

/**
 * Checks any queued async tasks for status, allows the task a change
 * to process the results, and fires off their delegates if the task is
 * complete
 */
void UOnlineSubsystemPC::TickAsyncTasks(void)
{
	// Check each task for completion
	for (INT Index = 0; Index < AsyncTasks.Num(); Index++)
	{
		if (AsyncTasks(Index)->HasTaskCompleted())
		{
			// Perform any task specific finalization of data before having
			// the script delegate fired off
			if (AsyncTasks(Index)->ProcessAsyncResults(this) == TRUE)
			{
				// Have the task fire off its delegates on our object
				AsyncTasks(Index)->TriggerDelegates(this);
				// Free the memory and remove it from our list
				delete AsyncTasks(Index);
				AsyncTasks.Remove(Index);
				Index--;
			}
		}
	}
}

/**
 * Determines whether the user's profile file exists or not
 */
UBOOL UOnlineSubsystemPC::DoesProfileExist(void)
{
	// This will determine if the file exists and verifies that it isn't a directory
	return (GFileManager->FileSize(*CreateProfileName()) != -1);
}

/**
 * Reads the online profile settings for a given user from disk. If the file
 * exists, an async task is used to verify the file wasn't hacked and to
 * decompress the contents into a buffer. Once the task
 *
 * @param LocalUserNum the user that we are reading the data for
 * @param ProfileSettings the object to copy the results to and contains the list of items to read
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineSubsystemPC::ReadProfileSettings(BYTE LocalUserNum,
	UOnlineProfileSettings* ProfileSettings)
{
	DWORD Return = E_FAIL;
	// Only read if we don't have a profile for this player
	if (CachedProfile == NULL)
	{
		if (ProfileSettings != NULL)
		{
			CachedProfile = ProfileSettings;
			// Don't bother reading if they haven't saved it before
			if (DoesProfileExist())
			{
				CachedProfile->AsyncState = OPAS_Read;
				// Clear the previous set of results
				CachedProfile->ProfileSettings.Empty();
				TArray<BYTE> Buffer;
				// Load the profile into a byte array
				if (appLoadFileToArray(Buffer,*CreateProfileName()))
				{
					FProfileSettingsReader Reader(64 * 1024,TRUE,Buffer.GetTypedData(),Buffer.Num());
					// Serialize the profile from that array
					if (Reader.SerializeFromBuffer(CachedProfile->ProfileSettings))
					{
						INT ReadVersion = CachedProfile->GetVersionNumber();
						// Check the version number and reset to defaults if they don't match
						if (CachedProfile->VersionNumber != ReadVersion)
						{
							debugf(NAME_DevOnline,
								TEXT("Detected profile version mismatch (%d != %d), setting to defaults"),
								CachedProfile->VersionNumber,
								ReadVersion);
							CachedProfile->SetToDefaults();
						}
						CachedProfile->AsyncState = OPAS_Finished;
						Return = S_OK;
					}
					else
					{
						debugf(NAME_DevOnline,
							TEXT("Profile data for %s was corrupt, using defaults"),
							*LoggedInPlayerName);
						CachedProfile->SetToDefaults();
						Return = S_OK;
					}
				}
			}
			else
			{
				debugf(NAME_DevOnline,
					TEXT("Profile for %s doesn't exist, using defaults"),
					*LoggedInPlayerName);
				CachedProfile->SetToDefaults();
				Return = S_OK;
			}
			if (Return != S_OK && Return != ERROR_IO_PENDING)
			{
				debugf(NAME_DevOnline,
					TEXT("Unable to read the profile for %s, using defaults"),
					*LoggedInPlayerName);
				CachedProfile->SetToDefaults();
				CachedProfile->AsyncState = OPAS_Finished;
			}
		}
		else
		{
			debugf(NAME_Error,TEXT("Can't specify a null profile settings object"));
		}
	}
	// Make sure the profile isn't already being read, since this is going to
	// complete immediately
	else if (CachedProfile->AsyncState != OPAS_Read)
	{
		debugf(NAME_DevOnline,TEXT("Using cached profile data instead of reading"));
		// If the specified read isn't the same as the cached object, copy the
		// data from the cache
		if (CachedProfile != ProfileSettings)
		{
			ProfileSettings->ProfileSettings = CachedProfile->ProfileSettings;
		}
		Return = S_OK;
	}
	else
	{
		debugf(NAME_Error,TEXT("Profile read for player (%d) is already in progress"),LocalUserNum);
	}
	// Trigger the delegates if there are any registered
	if (Return != ERROR_IO_PENDING)
	{
		FAsyncTaskDelegateResults Params(Return);
		TriggerOnlineDelegates(this,ReadProfileSettingsDelegates,&Params);
	}
	return Return == S_OK || Return == ERROR_IO_PENDING;
}

/**
 * Writes the online profile settings for a given user Live using an async task
 *
 * @param LocalUserNum the user that we are writing the data for
 * @param ProfileSettings the list of settings to write out
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineSubsystemPC::WriteProfileSettings(BYTE LocalUserNum,
	UOnlineProfileSettings* ProfileSettings)
{
	DWORD Return = E_FAIL;
	// Don't allow a write if there is a task already in progress
	if (CachedProfile == NULL ||
		(CachedProfile->AsyncState != OPAS_Read && CachedProfile->AsyncState != OPAS_Write))
	{
		if (ProfileSettings != NULL)
		{
			// Cache to make sure GC doesn't collect this while we are waiting
			// for the task to complete
			CachedProfile = ProfileSettings;
			// Mark this as a write in progress
			CachedProfile->AsyncState = OPAS_Write;
			// Make sure the profile settings have a version number
			CachedProfile->AppendVersionToSettings();
			// Used to write the profile settings into a blob
			FProfileSettingsWriter Writer(64 * 1024,TRUE);
			if (Writer.SerializeToBuffer(CachedProfile->ProfileSettings))
			{
				// Write the file to disk
				FArchive* FileWriter = GFileManager->CreateFileWriter(*CreateProfileName());
				if (FileWriter)
				{
					FileWriter->Serialize((void*)Writer.GetFinalBuffer(),Writer.GetFinalBufferLength());
					delete FileWriter;
				}
				// Remove the write state so that subsequent writes work
				CachedProfile->AsyncState = OPAS_Finished;
				Return = S_OK;
			}
			if (Return != S_OK && Return != ERROR_IO_PENDING)
			{
				debugf(NAME_DevOnline,TEXT("Failed to write profile data for %s"),*LoggedInPlayerName);
				// Remove the write state so that subsequent writes work
				CachedProfile->AsyncState = OPAS_Finished;
			}
		}
		else
		{
			debugf(NAME_Error,TEXT("Can't write a null profile settings object"));
		}
	}
	else
	{
		debugf(NAME_Error,
			TEXT("Can't write profile as an async profile task is already in progress for player (%d)"),
			LocalUserNum);
	}
	if (Return != ERROR_IO_PENDING)
	{
		// Remove the write state so that subsequent writes work
		CachedProfile->AsyncState = OPAS_Finished;
		// Send the notification of completion
		FAsyncTaskDelegateResults Params(Return);
		TriggerOnlineDelegates(this,WriteProfileSettingsDelegates,&Params);
	}
	return Return == S_OK || Return == ERROR_IO_PENDING;
}

#endif	//#if WITH_UE3_NETWORKING
