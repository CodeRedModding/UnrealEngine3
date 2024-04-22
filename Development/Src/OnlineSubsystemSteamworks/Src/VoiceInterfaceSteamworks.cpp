/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemSteamworks.h"
#include "VoiceInterfaceSteamworks.h"

#if WITH_UE3_NETWORKING && WITH_STEAMWORKS

#include "EngineSoundClasses.h"
#include "EngineAudioDeviceClasses.h"

// Desired samplerate for data sent to Fonix
#define RAW_SAMPLERATE 16000

// Desired samplerate for data played back by audio subsystem
#define PLAYBACK_SAMPLERATE 11025

// Minimum length of auth data needed, in seconds, for playback to be kicked off (needed to prevent stuttering)
// @todo Steam: The delay was originally 0.25, but I've had to increase it to avoid hitching on initial playback;
//		try to find a good way of reducing VOIP latency
#define MIN_PLAYBACK_LENGTH 0.75


/** Singleton instance pointer initialization */
FVoiceInterfaceSteamworks* FVoiceInterfaceSteamworks::GVoiceInterface = NULL;


/**
 * Platform specific method for creating the voice interface to use for all
 * voice data/communication
 *
 * @param MaxLocalTalkers the number of local talkers to handle voice for
 * @param MaxRemoteTalkers the number of remote talkers to handle voice for
 * @param bIsSpeechRecognitionDesired whether speech recognition should be enabled or not
 *
 * @return The voice interface to use
 */
FVoiceInterface* appCreateVoiceInterface(INT MaxLocalTalkers, INT MaxRemoteTalkers, UBOOL bIsSpeechRecognitionDesired)
{
	return FVoiceInterfaceSteamworks::CreateInstance(MaxLocalTalkers, MaxRemoteTalkers, bIsSpeechRecognitionDesired);
}

/**
 * Creates the Steamworks voice engine and performs any other initialization
 *
 * @param MaxLocalTalkers the maximum number of local talkers to support
 * @param MaxRemoteTalkers the maximum number of remote talkers to support
 * @param bIsSpeechRecognitionDesired whether speech recognition should be enabled
 *
 * @return TRUE if everything initialized correctly, FALSE otherwise
 */
UBOOL FVoiceInterfaceSteamworks::Init(INT MaxLocalTalkers, INT MaxRemoteTalkers, UBOOL bIsSpeechRecognitionDesired)
{
	OwningSubsystem = Cast<UOnlineSubsystemSteamworks>(UGameEngine::GetOnlineSubsystem());

	if (!IsSteamClientAvailable())
	{
		return FALSE;
	}

	GConfig->GetFloat(TEXT("OnlineSubsystemSteamworks.OnlineSubsystemSteamworks"), TEXT("VOIPVolumeMultiplier"), VOIPVolumeMultiplier,
				GEngineIni);


	// Ask the INI file if voice is wanted
	UBOOL bHasVoiceEnabled = FALSE;

	if (GConfig->GetBool(TEXT("VoIP"), TEXT("bHasVoiceEnabled"), bHasVoiceEnabled, GEngineIni))
	{
		// Check to see if voice is enabled/disabled
		if (bHasVoiceEnabled == FALSE)
		{
			return FALSE;
		}
	}

	// Skip if a dedicated server
	if (!GIsClient)
	{
		debugf(NAME_DevOnline, TEXT("Skipping voice initialization for dedicated server"));
		return FALSE;
	}

	return FVoiceInterfaceCommon::Init(MaxLocalTalkers, MaxRemoteTalkers, bIsSpeechRecognitionDesired);
}

/**
 * Starts local voice processing for the specified user index
 *
 * @param UserIndex the user to start processing for
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceSteamworks::StartLocalVoiceProcessing(DWORD UserIndex)
{
	if (IsOwningIndex(UserIndex))
	{
		if (!bIsCapturing)
		{
			// Update the current recording state, if VOIP data was still being read
			InternalUpdateRecordingState();

			if (!InternalIsRecording())
				InternalStartRecording();

			// NOTE: It's important that this is set >after< the above Internal* functions are called
			bIsCapturing = TRUE;
		}

		return S_OK;
	}
	else
	{
		debugf(NAME_Error, TEXT("StartLocalVoiceProcessing(): Device is currently owned by another user"));
	}

	return E_FAIL;
}

/**
 * Stops local voice processing for the specified user index
 *
 * @param UserIndex the user to stop processing of
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceSteamworks::StopLocalVoiceProcessing(DWORD UserIndex)
{
	if (IsOwningIndex(UserIndex))
	{
		if (bIsCapturing)
		{
			bIsCapturing = FALSE;
			bPendingFinalCapture = TRUE;

			// Make a call to begin stopping the current VOIP recording session
			InternalStopRecording();

			// Now check/update the status of the recording session
			InternalUpdateRecordingState();
		}

		return S_OK;
	}
	else
	{
		debugf(NAME_Error, TEXT("StopLocalVoiceProcessing: Ignoring stop request for non-owning user"));
	}

	return E_FAIL;
}

/**
 * Starts remote voice processing for the specified user
 *
 * @param UniqueId the unique id of the user that will be talking
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceSteamworks::StartRemoteVoiceProcessing(FUniqueNetId UniqueId)
{
	return S_OK;  // No-op in Steamworks (GameSpy, too).
}

/**
 * Stops remote voice processing for the specified user
 *
 * @param UniqueId the unique id of the user that will no longer be talking
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceSteamworks::StopRemoteVoiceProcessing(FUniqueNetId UniqueId)
{
	return S_OK;  // No-op in Steamworks (GameSpy, too).
}

/**
 * Tells the voice system to start tracking voice data for speech recognition
 *
 * @param UserIndex the local user to recognize voice data for
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceSteamworks::StartSpeechRecognition(DWORD UserIndex)
{
	if (IsOwningIndex(UserIndex))
	{
		StartLocalVoiceProcessing(UserIndex);
		return FVoiceInterfaceCommon::StartSpeechRecognition(UserIndex);
	}

	return E_FAIL;
}

/**
 * Tells the voice system to stop tracking voice data for speech recognition
 *
 * @param UserIndex the local user to recognize voice data for
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceSteamworks::StopSpeechRecognition(DWORD UserIndex)
{
	if (IsOwningIndex(UserIndex))
	{
		StopLocalVoiceProcessing(UserIndex);
		return FVoiceInterfaceCommon::StopSpeechRecognition(UserIndex);
	}

	return E_FAIL;
}

/**
 * Registers the user index as a local talker (interested in voice data)
 *
 * @param UserIndex the user index that is going to be a talker
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceSteamworks::RegisterLocalTalker(DWORD UserIndex)
{
	if (OwningIndex == INVALID_OWNING_INDEX)
	{
		OwningIndex = UserIndex;
		return S_OK;
	}

	return E_FAIL;
}

/**
 * Unregisters the user index as a local talker (not interested in voice data)
 *
 * @param UserIndex the user index that is no longer a talker
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceSteamworks::UnregisterLocalTalker(DWORD UserIndex)
{
	if (IsOwningIndex(UserIndex))
	{
		OwningIndex = INVALID_OWNING_INDEX;
		return S_OK;
	}

	return E_FAIL;
}

/**
 * Registers the unique player id as a remote talker (submitted voice data only)
 *
 * @param UniqueId the id of the remote talker
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceSteamworks::RegisterRemoteTalker(FUniqueNetId UniqueId)
{
	return S_OK;  // No-op in Steamworks (GameSpy, too).
}

/**
 * Unregisters the unique player id as a remote talker
 *
 * @param UniqueId the id of the remote talker
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceSteamworks::UnregisterRemoteTalker(FUniqueNetId UniqueId)
{
	return S_OK;  // No-op in Steamworks (GameSpy, too).
}

/**
 * Checks whether a local user index has a headset present or not
 *
 * @param UserIndex the user to check status for
 *
 * @return TRUE if there is a headset, FALSE otherwise
 */
UBOOL FVoiceInterfaceSteamworks::IsHeadsetPresent(DWORD UserIndex)
{
	if (IsOwningIndex(UserIndex))
	{
		return S_OK;
	}

	return E_FAIL;
}

/**
 * Determines whether a local user index is currently talking or not
 *
 * @param UserIndex the user to check status for
 *
 * @return TRUE if the user is talking, FALSE otherwise
 */
UBOOL FVoiceInterfaceSteamworks::IsLocalPlayerTalking(DWORD UserIndex)
{
	return (GetVoiceDataReadyFlags() & (UserIndex << 1)) != 0;
}

/**
 * Determines whether a remote talker is currently talking or not
 *
 * @param UniqueId the unique id of the talker to check status on
 *
 * @return TRUE if the user is talking, FALSE otherwise
 */
UBOOL FVoiceInterfaceSteamworks::IsRemotePlayerTalking(FUniqueNetId UniqueId)
{
	// We keep track of how many milliseconds of audio we submitted per-user.
	return TalkingUsers.Find(UniqueId.Uid) != NULL;
}

/**
 * Returns which local talkers have data ready to be read from the voice system
 *
 * @return Bit mask of talkers that have data to be read (1 << UserIndex)
 */
DWORD FVoiceInterfaceSteamworks::GetVoiceDataReadyFlags(void)
{
	// First check and update the internal state of VOIP recording
	InternalUpdateRecordingState();

	// Now check if the client is currently recording VOIP data
	if (OwningIndex != INVALID_OWNING_INDEX && InternalIsRecording())
	{
		uint32 CompressedBytes, RawBytes;
		const EVoiceResult Result = GSteamUser->GetAvailableVoice(&CompressedBytes, &RawBytes, RAW_SAMPLERATE);

		if (Result == k_EVoiceResultOK && CompressedBytes > 0)
		{
			return 1 << OwningIndex;
		}
	}

	return 0;
}

/**
 * Sets the playback priority of a remote talker for the given user. A
 * priority of 0xFFFFFFFF indicates that the player is muted. All other
 * priorities sorted from zero being most important to higher numbers
 * being less important.
 *
 * @param UserIndex the local talker that is setting the priority
 * @param UniqueId the id of the remote talker that is having priority changed
 * @param Priority the new priority to apply to the talker
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceSteamworks::SetPlaybackPriority(DWORD UserIndex, FUniqueNetId RemoteTalkerId, DWORD Priority)
{
	// Unsupported in Steamworks (and GameSpy)
	return S_OK;
}

/**
 * Reads local voice data for the specified local talker. The size field
 * contains the buffer size on the way in and contains the amount read on
 * the way out
 *
 * @param UserIndex the local talker that is having their data read
 * @param Data the buffer to copy the voice data into
 * @param Size in: the size of the buffer, out: the amount of data copied
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceSteamworks::ReadLocalVoiceData(DWORD UserIndex, BYTE* Data, DWORD* Size)
{
	// Before doing anything, check/update the current recording state
	InternalUpdateRecordingState();

	INT Available = QueuedSendData.Data.Num() - QueuedSendData.Bytes;

	// Return data even if not capturing, if the final half-second of data from Steam is still pending
	if (OwningIndex == UserIndex && (InternalIsRecording() || Available > 0))
	{
		// Steamworks gives us data way bigger than *Size (2k vs 100 bytes), so we cache it, and split it into chunks here
		// The whole chunk has to arrive for playback, or we drop it, which isn't ideal, but it works well enough
		if (Available == 0)
		{
			QueuedSendData.Data.Empty();
			QueuedSendData.Generation++;
			QueuedSendData.Bytes = 0;

			// zero is never valid.
			if (QueuedSendData.Generation == 0)
			{
				QueuedSendData.Generation = 1;
			}

			uint32 CompressedBytes = 0;
			uint32 RawBytes = 0;
			EVoiceResult VoiceResult = GSteamUser->GetAvailableVoice(&CompressedBytes, &RawBytes, RAW_SAMPLERATE);

			if (VoiceResult != k_EVoiceResultOK)
			{
				return E_FAIL;
			}


			// NOTE: Sometimes (rarely) 'RawBytes' is 0, so for that case, make certain there will be enough buffer space
			if (RawBytes <= 0)
			{
				QueuedSendData.DecompressedTotal = 4096;
			}
			else
			{
				QueuedSendData.DecompressedTotal = RawBytes;
			}
			
			// This shouldn't happen, but just in case.
			if (CompressedBytes == 0 && RawBytes == 0)
			{
				*Size = 0;
				return S_OK;
			}

#if VOIP_RECORDLOGSPEW
			debugf(NAME_DevOnline, TEXT("ReadLocalVoiceData: GetAvailableVoice: RawBytes: %i, CompressedBytes: %i"),
				RawBytes, CompressedBytes);
#endif


			Available = CompressedBytes;

			if (RawBytes > 0)
			{
				RawSamples.Empty(RawBytes);
			}

			uint32 AvailableWritten = 0;
			uint32 RawBytesWritten = 0;


			// Loop grabbing of audio data (a maximum of 4 times), increasing the buffer size each loop until it succeeds
			// NOTE: This is >required< as GetAvailableVoice returns wrong values sometimes, apparently
			for (INT i=0; i<4; i++)
			{
				if (Available > 0)
				{
					QueuedSendData.Data.AddZeroed(Available);
				}

				if (RawBytes > 0)
				{
					RawSamples.Add(RawBytes);
				}


				if (Available > 0 && RawBytes > 0)
				{
					VoiceResult = GSteamUser->GetVoice(true, &QueuedSendData.Data(0), QueuedSendData.Data.Num(),
										&AvailableWritten, true, RawSamples.GetData(),
										RawSamples.Num(), &RawBytesWritten, RAW_SAMPLERATE);
				}
				else if (Available > 0)
				{
					VoiceResult = GSteamUser->GetVoice(true, &QueuedSendData.Data(0), QueuedSendData.Data.Num(),
										&AvailableWritten, false, NULL, 0, NULL, RAW_SAMPLERATE);
				}
				else
				{
					VoiceResult = GSteamUser->GetVoice(false, NULL, 0, NULL, true, RawSamples.GetData(), RawBytes,
										&RawBytesWritten, RAW_SAMPLERATE);
				}


				// If the buffer was not too small, break from the loop
				if (VoiceResult != k_EVoiceResultBufferTooSmall)
				{
					break;
				}
			}


			// Cap the buffers, if they are bigger than we need
			if (AvailableWritten > 0 && QueuedSendData.Data.Num() > (INT)AvailableWritten)
			{
				QueuedSendData.Data.Remove(AvailableWritten, QueuedSendData.Data.Num() - (INT)AvailableWritten);
			}

			if (RawBytesWritten > 0 && RawSamples.Num() > (INT)RawBytesWritten)
			{
				RawSamples.Remove(RawBytesWritten, RawSamples.Num() - (INT)RawBytesWritten);
			}

			Available = AvailableWritten;
			RawBytes = RawBytesWritten;

#if VOIP_RECORDLOGSPEW
			debugf(NAME_DevOnline, TEXT("ReadLocalVoiceData: GetVoice: RawBytes: %i, Available: %i"), RawBytes, Available);

			if (RawBytes > 0)
			{
				FString PreCRC;
				FString CRCString;

				appBufferToString(PreCRC, RawSamples.GetData(), RawBytes);
				CRCString = FString::Printf(TEXT("%08X"), *PreCRC);

				debugf(NAME_DevOnline, TEXT("RawBytes CRC: %s"), *CRCString);
			}

			if (CompressedBytes > 0)
			{
				FString PreCRC;
				FString CRCString;

				appBufferToString(PreCRC,  &QueuedSendData.Data(0), CompressedBytes);
				CRCString = FString::Printf(TEXT("%08X"), *PreCRC);

				debugf(NAME_DevOnline, TEXT("CompressedBytes CRC: %s"), *CRCString);
			}

			if (VoiceResult == k_EVoiceResultNotRecording)
				debugf(NAME_DevOnline, TEXT("Got 'k_EVoiceResultNotRecording'"));
#endif

			if (VoiceResult != k_EVoiceResultOK)
			{
				Available = 0;
				QueuedSendData.Data.Empty();
				RawSamples.Empty();

				debugf(NAME_DevOnline, TEXT("ReadLocalVoiceData: Error reading voice data; VoiceResult: %i"), (INT)VoiceResult);
				return E_FAIL;
			}


			// @todo Steam: Updated the code to retrieve raw data at 16Khz (which is what Fonix needs apparently), but need to test it
			if (RawBytes > 0)
			{
				RawVoiceDataCallback(OwningIndex, (SWORD*)RawSamples.GetData(), RawBytes / sizeof (SWORD));
			}
		}


		if (Available <= 0)
		{
			*Size = 0;
		}
		else
		{
			// @todo Steam: Use a byte array archive for serialization, eventually
			// This should probably use serialization.
			const INT MetadataLen = sizeof (WORD) + sizeof (WORD) + sizeof (WORD) + sizeof(WORD) + sizeof(DOUBLE);
			check(*Size > MetadataLen);

			WORD* WordData = (WORD*)Data;
			*(WordData++) = (WORD)QueuedSendData.Generation;
			*(WordData++) = (WORD)QueuedSendData.Bytes;
			*(WordData++) = (WORD)QueuedSendData.Data.Num();
			*(WordData++) = (WORD)QueuedSendData.DecompressedTotal;

			DOUBLE* DoubleData = (DOUBLE*)WordData;
			*(DoubleData++) = appSeconds();

			const INT CopyLen = Min<INT>(*Size - MetadataLen, Available);
			appMemcpy(DoubleData, &QueuedSendData.Data(QueuedSendData.Bytes), CopyLen);

			QueuedSendData.Bytes += CopyLen;
			*Size = CopyLen + MetadataLen;
		}

#if VOIP_RECORDLOGSPEW
		debugf(NAME_DevOnline, TEXT("ReadLocalVoiceData: QueuedSendData.Bytes: %i (IMPORTANT: May not have read data from Steam)"),
			QueuedSendData.Bytes);
#endif

		return S_OK;
	}

	return E_FAIL;
}

/**
 * Submits remote voice data for playback by the voice system. No playback
 * occurs if the priority for this remote talker is 0xFFFFFFFF. Size
 * indicates how much data to submit for processing. It's also an out
 * value in case the system could only process a smaller portion of the data
 *
 * @param RemoteTalkerId the remote talker that sent this data
 * @param Data the buffer to copy the voice data into
 * @param Size in: the size of the buffer, out: the amount of data copied
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceSteamworks::SubmitRemoteVoiceData(FUniqueNetId RemoteTalkerId, BYTE* Data, DWORD* Size)
{
	FQueuedSteamworksVoice* QueuedData = QueuedReceiveData.Find(RemoteTalkerId.Uid);

	if (QueuedData == NULL)
	{
		QueuedReceiveData.Set(RemoteTalkerId.Uid, FQueuedSteamworksVoice());
		QueuedData = QueuedReceiveData.Find(RemoteTalkerId.Uid);
	}

	// This should probably use serialization.
	const INT MetadataLen = sizeof (WORD) + sizeof (WORD) + sizeof (WORD) + sizeof(WORD) + sizeof(DOUBLE);

	if (*Size < MetadataLen)
	{
		return E_FAIL;
	}


	WORD* WordData = (WORD*)Data;
	const WORD Generation = *(WordData++);
	const WORD Offset = *(WordData++);
	const WORD Total = *(WordData++);
	const WORD DecompressedTotal = *(WordData++);

	DOUBLE* DoubleData = (DOUBLE*)WordData;

	const DOUBLE Timestamp = *(DoubleData++);

	Data = (BYTE*)DoubleData;

	if (Generation == 0 || (((Offset + *Size) - MetadataLen) > Total))
	{
		// reset next packet
		QueuedData->Generation = 0;
		return E_FAIL;
	}


	if (Generation != QueuedData->Generation || QueuedData->Data.Num() != Total)
	{
		// new voice packet.
		QueuedData->Data.Empty();
		QueuedData->Data.AddZeroed(Total);
		QueuedData->Generation = Generation;
		QueuedData->Bytes = 0;
		QueuedData->LastSeen = appSeconds();
	}

	appMemcpy(&QueuedData->Data(Offset), Data, *Size - MetadataLen);
	QueuedData->Bytes += (*Size - MetadataLen);

	// Pretend we succeeded if we haven't gotten a complete voice packet reassembled yet...

	//debugf(NAME_DevOnline, TEXT("Have %d bytes of VoIP packet (waiting for %d total)."), (INT)QueuedData->Bytes, (INT)QueuedData->Data.Num());

	if (QueuedData->Bytes != QueuedData->Data.Num())
	{
		return S_OK;
	}


	// Reset this since we're definitely done getting this packet
	QueuedData->Generation = 0;

	INT DecompressBytes = Max((INT)(*Size), (INT)DecompressedTotal) * 4;
	uint32 BytesWritten = 0;

	while (TRUE)
	{
		// Keep/grow allocation, but mark portion we need as used.
		DecompressBuffer.Empty(DecompressBuffer.Num());
		DecompressBuffer.Add(DecompressBytes);

		const EVoiceResult VoiceResult = GSteamUser->DecompressVoice(&QueuedData->Data(0), QueuedData->Bytes, DecompressBuffer.GetData(),
										DecompressBytes, &BytesWritten, PLAYBACK_SAMPLERATE);

		if (VoiceResult == k_EVoiceResultBufferTooSmall)
		{
			// Increase buffer, try again
			DecompressBytes *= 2;
			continue;
		}
		else if (VoiceResult != k_EVoiceResultOK)
		{
			*Size = 0;
			return E_FAIL;
		}

		break;
	}


	// Flag part we didn't need as unused
	DecompressBuffer.Remove(BytesWritten, DecompressBuffer.Num() - BytesWritten);


	// If there is no data, return
	if (DecompressBuffer.GetData() == NULL || DecompressBuffer.Num() <= 0)
	{
		return S_OK;
	}


	const QWORD IdValue = RemoteTalkerId.Uid;
	UAudioComponent** ppAudioComponent = TalkingUsers.Find(IdValue);
	UAudioComponent* AudioComponent = (ppAudioComponent ? *ppAudioComponent : NULL);

#if VOIP_PLAYBACKLOGSPEW
	UBOOL bFirstRun=FALSE;
#endif

	if (AudioComponent == NULL || AudioComponent->IsPendingKill() || AudioComponent->HasAnyFlags(RF_Unreachable))
	{
		// Build a new UAudioComponent, which we'll use to play incoming voice.
		if (GEngine && GEngine->Client && GEngine->Client->GetAudioDevice())
		{
			AWorldInfo* WI = (GWorld != NULL ? GWorld->GetWorldInfo() : NULL);
			UAudioDevice* AudioDevice = GEngine->Client->GetAudioDevice();
			USoundCue* SoundCue = NULL;

			// If in the midst of travelling between maps, disallow creation of new audio components
			//	(otherwise there may be extremely rare crashes from invalid audio components)
			if (WI != NULL && !WI->IsPreparingMapChange() && !WI->IsInSeamlessTravel())
			{
				SoundCue = ConstructObject<USoundCue>(USoundCue::StaticClass(), UObject::GetTransientPackage());
			}
#if VOIP_COMPONENTLOGSPEW
			else
			{
				debugf(TEXT("Blocking creation of new audiocomponent during seamless travel"));
			}
#endif

			if (SoundCue != NULL)
			{
				USoundNodeWaveStreaming* SoundNodeWave = ConstructObject<USoundNodeWaveStreaming>(
										USoundNodeWaveStreaming::StaticClass(), SoundCue);

				if (SoundNodeWave != NULL)
				{
#if VOIP_PLAYBACKLOGSPEW
					bFirstRun = TRUE;
#endif

					// @todo Steam: Replace with 'GetDesiredSampleRate' from SDK v1.13 eventually
					SoundNodeWave->SampleRate = PLAYBACK_SAMPLERATE;  // this is hardcoded in Steamworks.
					SoundNodeWave->NumChannels = 1;

					// Sound data keeps getting pushed into the node, so it has no set duration
					SoundNodeWave->Duration = INDEFINITELY_LOOPING_DURATION;

					SoundCue->SoundClass = FName(TEXT("VoiceChat"));
					SoundCue->FirstNode = SoundNodeWave;
					AudioComponent = AudioDevice->CreateComponent(SoundCue, GWorld->Scene, NULL, FALSE);

					if (AudioComponent != NULL)
					{
#if VOIP_COMPONENTLOGSPEW
						debugf(TEXT("Creating VOIP AudioComponent; UID: ") I64_FORMAT_TAG
							TEXT(", Component Address: %p, Component: %s"),
							IdValue, AudioComponent, *AudioComponent->GetName());
#endif

						TalkingUsers.Set(IdValue, AudioComponent);

						// IMPORTANT: If there are issues with VOIP volume limit being too low, increase 'XA2_SOURCE_VOL_MAX'
						//		in XAudio2Device.h
						AudioComponent->VolumeMultiplier = VOIPVolumeMultiplier;

						// Make sure to hook the 'Stop' event, so we can clean up the 'TalkingUsers' entry later;
						//	without this, the game will crash on map change, if 'TalkingUsers' has any AudioComponent's
						if (OwningSubsystem != NULL)
						{
							OBJ_SET_DELEGATE(AudioComponent, OnAudioFinished, OwningSubsystem,
										FName(TEXT("OnVOIPPlaybackFinished")));
						}
					}
				}
			}
		}
	}

	if (AudioComponent == NULL)
	{
		return E_FAIL;
	}


	USoundNodeWaveStreaming* Wave = Cast<USoundNodeWaveStreaming>(AudioComponent->SoundCue->FirstNode);

#if VOIP_VOLUMELOGSPEW
	SWORD* DecompressSampleData = (SWORD*)DecompressBuffer.GetData();
	INT DecompressLen = DecompressBuffer.Num() / 2;

	SWORD PCMMin = 32767;
	SWORD PCMMax = -32768;
	SQWORD PCMTotal = 0;

	SWORD CurData = 0;

	for (INT i=0; i<DecompressLen; i++)
	{
		CurData = DecompressSampleData[i];
		PCMTotal += CurData;

		if (CurData < PCMMin)
		{
			PCMMin = CurData;
		}

		if (CurData > PCMMax)
		{
			PCMMax = CurData;
		}
	}


	// Maintain a running 2-second average, so you get some meaningful values in the log
	// @todo Steam: The averaging is probably extremely broken since you changed it from BYTE to SQWORD
	static TArray<FLOAT>	StoredTotalTimestamps;
	static TArray<SQWORD>	StoredTotals;
	static TArray<DWORD>	StoredCounts;

	FLOAT CurrentTimestamp = (GWorld != NULL && GWorld->GetWorldInfo() != NULL) ? GWorld->GetWorldInfo()->RealTimeSeconds : 0.0f;
	FLOAT MinimumTimestamp = CurrentTimestamp - 2.0;

	SQWORD CombinedTotals = 0;
	QWORD CombinedCounts = 0;

	UBOOL bStoredCurrent = FALSE;

	for (INT i=0; i<StoredTotalTimestamps.Num(); i++)
	{
		if (StoredTotalTimestamps(i) >= MinimumTimestamp)
		{
			CombinedTotals += StoredTotals(i);
			CombinedCounts += StoredCounts(i);
		}
		else if (!bStoredCurrent)
		{
			StoredTotalTimestamps(i) = CurrentTimestamp;
			StoredTotals(i) = PCMTotal;
			StoredCounts(i) = DecompressLen;

			bStoredCurrent = TRUE;
		}
	}

	if (!bStoredCurrent)
	{
		StoredTotalTimestamps.AddItem(CurrentTimestamp);
		StoredTotals.AddItem(PCMTotal);
		StoredCounts.AddItem(DecompressLen);
	}

	DOUBLE MovingPCMAvg = (CombinedCounts > 0 ? ((DOUBLE)CombinedTotals / (DOUBLE)CombinedCounts) : 0.0);

	debugf(TEXT("VOIP PCM Volume info: Min: %i, Max: %i, Avg: %f"), PCMMin, PCMMax, MovingPCMAvg);
#endif

#if VOIP_PLAYBACKLOGSPEW
	INT PreLen = Wave->AvailableAudioBytes();
	FLOAT PreLenSeconds = (FLOAT)PreLen / (FLOAT)(sizeof (WORD) * PLAYBACK_SAMPLERATE);
#endif

	// @todo Steam: This code is not very clear/clean/maintainable; revisit at some stage and find a good way to simplify it

	// Calculate the amount of silence needed between received audio packets
	static DOUBLE LastReceiveTimestamp = 0;
	static FLOAT LastReceiveLength = 0;

	// If the VOIP timestamps indicate silence in the stream (of less than three seconds), add that silence
	FLOAT CurrentReceiveLength = (FLOAT)DecompressBuffer.Num() / (FLOAT)(sizeof(WORD) * PLAYBACK_SAMPLERATE);
	FLOAT SilenceLen = (Timestamp - (LastReceiveTimestamp + LastReceiveLength)) - CurrentReceiveLength;


	// Track silence that should be added when the audio component starts playback
	static FLOAT PrePlaySilence = 0;

	// Track whether audio-buffering should be disabled, if we were forced to add silence mid-stream (buffering would add more silence)
	static UBOOL bQueuedSilencePending = FALSE;

	if (!AudioComponent->IsPlaying())
	{
		bQueuedSilencePending = FALSE;
	}


	if (LastReceiveTimestamp != 0 && SilenceLen > 0.1 && SilenceLen < 3.0)
	{
#if VOIP_PLAYBACKLOGSPEW || VOIP_SILENCELOGSPEW
		debugf(TEXT("Adding %f seconds of silence"), SilenceLen);
#endif

		// If the audio component is not currently playing, don't add the silence directly, or MIN_PLAYBACK_LENGTH interferes
		// NOTE: If more than one packet adds silence before MIN_PLAYBACK_LENGTH, PrePlaySilence can only consider the first
		if (!AudioComponent->IsPlaying() && PrePlaySilence == 0)
		{
#if VOIP_SILENCELOGSPEW
			debugf(TEXT("(preplay silence)"));
#endif

			PrePlaySilence += SilenceLen;
		}
		else
		{
#if VOIP_SILENCELOGSPEW
			debugf(TEXT("(queued silence)"));
#endif

			// @todo Steam: For some reason, this is adding silences that are too long (it is extremely difficult to fix though)
			Wave->QueueSilence(SilenceLen);

			bQueuedSilencePending = TRUE;
		}
	}


	// Start it playing if we haven't yet, and there's at least 'MIN_PLAYBACK_LENGTH' seconds of audio queued

	// NOTE: This commented code is the 'correct' code, but somehow (absolutely no idea how) it is adding extra silences,
	//	so the version below is used
	/*
	INT UnbufferedBytes = DecompressBuffer.Num();
	FLOAT PotentialPrePlaySilence = (PrePlaySilence > 0 ? Max(PrePlaySilence - Wave->InactiveDuration, 0.0f) : 0);

	UBOOL bStartPlaying = !AudioComponent->IsPlaying() && (bQueuedSilencePending ||
				((UnbufferedBytes + Wave->AvailableAudioBytes()) >
				((sizeof(WORD) * PLAYBACK_SAMPLERATE) * (MIN_PLAYBACK_LENGTH - PotentialPrePlaySilence))));
	*/

	UBOOL bStartPlaying = !AudioComponent->IsPlaying() && (bQueuedSilencePending ||
				(Wave->AvailableAudioBytes() > ((sizeof(WORD) * PLAYBACK_SAMPLERATE) * MIN_PLAYBACK_LENGTH)));

	if (bStartPlaying && PrePlaySilence > 0)
	{
		PrePlaySilence -= Wave->InactiveDuration;

#if VOIP_SILENCELOGSPEW
		debugf(TEXT("Discounting '%f' seconds of silence, due to InactiveDuration"), Wave->InactiveDuration);
#endif

		if (PrePlaySilence > 0)
		{
#if VOIP_SILENCELOGSPEW
			debugf(TEXT("Adding %f seconds of preplay silence"), PrePlaySilence);
#endif

			Wave->QueueSilence(PrePlaySilence);
		}

		PrePlaySilence = 0;
	}

	Wave->QueueAudio(DecompressBuffer);


#if VOIP_PLAYBACKLOGSPEW
	INT CurLen = Wave->AvailableAudioBytes();
	FLOAT CurLenSeconds = (FLOAT)CurLen / (FLOAT)(sizeof(WORD) * PLAYBACK_SAMPLERATE);
#endif


	LastReceiveTimestamp = Timestamp;
	LastReceiveLength = CurrentReceiveLength;

	if (bStartPlaying)
	{
#if VOIP_PLAYBACKLOGSPEW
		if (bFirstRun)
		{
			debugf(TEXT("Post-construct first run; data length: %f seconds (%i bytes)"), CurLenSeconds, CurLen);
		}
		else if (PreLen > 0)
		{
			debugf(TEXT("Pre-existing first run; already has data; length: %f seconds (%i bytes)"), PreLenSeconds, PreLen);
		}
		else
		{
			debugf(TEXT("Pre-existing first run; fresh data start"));
		}
#endif

		AudioComponent->Play();
	}
#if VOIP_PLAYBACKLOGSPEW
	else
	{
		if (bFirstRun)
		{
			if (AudioComponent->IsPlaying())
			{
				debugf(TEXT("POST-CONSTRUCT FIRST RUN; ALREADY PLAYING!!! THIS IS A BUG"));
			}
			else
			{
				debugf(TEXT("Post-construct first run; queueing data before run; length: %f seconds (%i bytes)"), CurLenSeconds,
					CurLen);
			}
		}
		else
		{
			if (AudioComponent->IsPlaying())
			{
				// NOTE: IMPORTANT: You deliberately >DO NOT< start/stop playing all the time, to avoid hitches >ALL THROUGH<
				//		playback; there still seems to be an initial hitch at the start of playback though
				// NOTE: This may be a good indicator of hitches, i.e. showing that the queue has run out of data; not sure
				//		though, as I think the data still exists in the XAudio buffer; a more accurate way of
				//		detecting hitches, may be to compare against the previous sucessful buffer flush, and count
				//		the number of seconds contained in that buffer
				if (PreLen == 0)
				{
					debugf(TEXT("Pre-existing playback; was still playing in absense of audio data"));
				}

				debugf(TEXT("Pre-existing playback; current: %f seconds (%i bytes), previous: %f seconds (%i bytes)"),
					CurLenSeconds, CurLen, PreLenSeconds, PreLen);

			}
			else
			{
				debugf(TEXT("Pre-existing pre-run; queueing data before run; length: %f seconds, (%i bytes)"), CurLenSeconds,
					CurLen);
			}
		}
	}
#endif

	return S_OK;  // okay, we submitted it!
}

/**
 * Voice processing tick function
 */
void FVoiceInterfaceSteamworks::Tick(FLOAT DeltaTime)
{
	// Remove users that are done talking.
	for (TalkingUserMap::TIterator It(TalkingUsers); It; ++It)
	{
		UAudioComponent* AudioComponent = It.Value();

#if VOIP_COMPONENTLOGSPEW
		debugf(TEXT("Iterating VOIP AudioComponent; UID: ") I64_FORMAT_TAG TEXT(", Component Address: %p"), It.Key(), AudioComponent);
#endif

		if (AudioComponent == NULL || AudioComponent->IsPendingKill() || AudioComponent->HasAnyFlags(RF_Unreachable))
		{
			It.RemoveCurrent();
			continue;
		}


		USoundNodeWaveStreaming* Wave = (AudioComponent->SoundCue != NULL && AudioComponent->SoundCue->FirstNode != NULL ?
							Cast<USoundNodeWaveStreaming>(AudioComponent->SoundCue->FirstNode) :
							NULL);

		if (Wave == NULL || Wave->InactiveDuration > 5.0)
		{
#if VOIP_PLAYBACKLOGSPEW
			debugf(TEXT("Removing VOIP AudioComponent"));
#endif

			// No need for a call to 'NotifyVOIPPlaybackFinished', as we are cleaning up here (allowing that call will crash too)
			OBJ_SET_DELEGATE(AudioComponent, OnAudioFinished, NULL, NAME_None);

			AudioComponent->Stop();

			//AudioComponentPool.Push(AudioComponent);
			It.RemoveCurrent();
		}
		else if (!AudioComponent->IsPlaying())
		{
			Wave->InactiveDuration += DeltaTime;
		}
		else
		{
			Wave->InactiveDuration = 0;
		}
	}

	// Remove voice packet fragments for players that haven't sent more data in X seconds.
	const DOUBLE Now = appSeconds();

	for (VoiceFragmentsMap::TIterator It(QueuedReceiveData); It; ++It)
	{
		// We are aggressive and dump the audio if it's been more than 1 second;
		// a given packet is way less than a second, so you'd be having serious skips in the audio by this point anyhow
		if ((Now - It.Value().LastSeen) >= 1.0)
		{
			It.RemoveCurrent();
		}
	}
}

/**
 * Called by the OnlineSubsytem, through the VOIP AudioComponent's OnAudioFinished delegate,
 * for cleaning up 'TalkingUsers' AudioComponent references
 * NOTE: This does not get called for audio components which have not yet called 'Play'
 *
 * @param VOIPAudioComponent	The AudioComponent which needs to be cleaned up
 */
void FVoiceInterfaceSteamworks::NotifyVOIPPlaybackFinished(class UAudioComponent* VOIPAudioComponent)
{
	if (!bBlockFinishNotify)
	{
		for (TalkingUserMap::TIterator It(TalkingUsers); It; ++It)
		{
			UAudioComponent* CurAudioComponent = It.Value();

			if (VOIPAudioComponent == CurAudioComponent)
			{
#if VOIP_COMPONENTLOGSPEW
				debugf(TEXT("Removing VOIP AudioComponent; UID: ") I64_FORMAT_TAG TEXT(", Component Address: %p"), It.Key(),
					CurAudioComponent);
#endif

				It.RemoveCurrent();
				break;
			}
		}
	}
}

/**
 * Notification sent to the VOIP Subsystem, that pre-travel cleanup is occuring (so we can cleanup audio components)
 *
 * @param bSessionEnded		whether or not the game session has ended
 */
void FVoiceInterfaceSteamworks::NotifyCleanupWorld(UBOOL bSessionEnded)
{
	for (TalkingUserMap::TIterator It(TalkingUsers); It; ++It)
	{
		UAudioComponent* CurAudioComponent = It.Value();

#if VOIP_COMPONENTLOGSPEW
		debugf(TEXT("Doing cleanup on VOIP AudioComponent; UID: ") I64_FORMAT_TAG TEXT(", Component Address: %p"), It.Key(),
			CurAudioComponent);
#endif

		if (CurAudioComponent != NULL)
		{
			// No need for a call to 'NotifyVOIPPlaybackFinished', as we are cleaning up here
			//	(allowing that call will crash too)
			OBJ_SET_DELEGATE(CurAudioComponent, OnAudioFinished, NULL, NAME_None);

			// Don't call this during exit, as it can cause rare crashes
			if (!GIsRequestingExit)
			{
				CurAudioComponent->Stop();
			}

			It.RemoveCurrent();
		}
	}

	TalkingUsers.Empty();
}

/**
 * Kicks off VOIP recording in Steam (with additional checks for valid VOIP initialization)
 */
void FVoiceInterfaceSteamworks::InternalStartRecording()
{
#if VOIP_RECORDLOGSPEW
	debugf(NAME_DevOnline, TEXT("InternalStartRecording"));
#endif

	// If this is the first call to 'InternalStartVoiceRecording', throw away any Steam VOIP data from the previous level (if any)
	// The destructor calls 'StopVoiceRecording', but Steam may continue recording for half a second (this may not be needed though)
	if (!bWasStartedOk)
	{
		uint32 CompressedAvailable, RawAvailable;
		EVoiceResult VoiceResult = GSteamUser->GetAvailableVoice(&CompressedAvailable, &RawAvailable, RAW_SAMPLERATE);

		if (VoiceResult == k_EVoiceResultOK && (CompressedAvailable > 0 || RawAvailable > 0))
		{
			VoiceResult = GSteamUser->GetVoice(false, NULL, 0, NULL, false, NULL, 0, NULL, RAW_SAMPLERATE);
		}
	}


	GSteamUser->StartVoiceRecording();
	GSteamFriends->SetInGameVoiceSpeaking(GSteamUser->GetSteamID(), true);

	// If the voice system has not been tested for correct initialization yet, do so now
	if (!bWasStartedOk)
	{
		uint32 DudInt;
		const EVoiceResult RecordingState = GSteamUser->GetAvailableVoice(&DudInt, &DudInt, RAW_SAMPLERATE);

		if (RecordingState != k_EVoiceResultNotInitialized)
		{
			bWasStartedOk = TRUE;
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("Steam VOIP not initialized"));

			// Reset VOIP state
			InternalStopRecording();
			InternalStoppedRecording();
			bIsCapturing = FALSE;
			bPendingFinalCapture = FALSE;
		}
	}
}

/**
 * Requests that Steam end the current VOIP recording session; Steam will continue recording for half a second after this
 */
void FVoiceInterfaceSteamworks::InternalStopRecording()
{
#if VOIP_RECORDLOGSPEW
	debugf(NAME_DevOnline, TEXT("InternalStopRecording"));
#endif

	GSteamUser->StopVoiceRecording();
}

/**
 * Called when Steam finally ends the VOIP recording session
 */
void FVoiceInterfaceSteamworks::InternalStoppedRecording()
{
#if VOIP_RECORDLOGSPEW
	debugf(NAME_DevOnline, TEXT("InternalStoppedRecording"));
#endif

	GSteamFriends->SetInGameVoiceSpeaking(GSteamUser->GetSteamID(), false);
}

/**
 * whether or not Steam is still recording VOIP data that we should handle
 *
 * @return	whether or not audio data is currently being recorded
 */
UBOOL FVoiceInterfaceSteamworks::InternalIsRecording()
{
	return bIsCapturing || bPendingFinalCapture;
}

/**
 * Checks the internal status of the VOIP recording state
 * When the player stop recording VOIP, Steam continues recording for half a second, so this monitors for when that stops
 */
void FVoiceInterfaceSteamworks::InternalUpdateRecordingState()
{
	if (bPendingFinalCapture)
	{
		uint32 DudInt;
		const EVoiceResult RecordingState = GSteamUser->GetAvailableVoice(&DudInt, &DudInt, RAW_SAMPLERATE);

		// If no data is available, we have finished capture the last (post-StopRecording) half-second of voice data
		if (RecordingState == k_EVoiceResultNotRecording)
		{
#if VOIP_RECORDLOGSPEW
			debugf(NAME_DevOnline, TEXT("InternalUpdateRecordingState: Setting bPendingFinalCapture to FALSE"));
#endif

			bPendingFinalCapture = FALSE;

			// If a new recording session has begun since the call to 'StopRecording', kick that off
			if (bIsCapturing)
			{
				InternalStartRecording();
			}
			else
			{
				// Marks that recording has successfully >stopped<
				InternalStoppedRecording();
			}
		}
	}
}

#endif	//#if WITH_UE3_NETWORKING && WITH_STEAMWORKS
