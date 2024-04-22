/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef INCLUDED_VOICEINTERFACESTEAMWORKS_H
#define INCLUDED_VOICEINTERFACESTEAMWORKS_H 1

#include "VoiceInterfaceCommon.h"

#if WITH_UE3_NETWORKING && WITH_STEAMWORKS

#define INVALID_OWNING_INDEX ((DWORD)-1)

// Debug
// @todo Steam: Eventually remove these
#define VOIP_PLAYBACKLOGSPEW 0
#define VOIP_VOLUMELOGSPEW 0
#define VOIP_RECORDLOGSPEW 0
#define VOIP_SILENCELOGSPEW 0
#define VOIP_COMPONENTLOGSPEW 0


struct FQueuedSteamworksVoice
{
	FQueuedSteamworksVoice()
		: Generation(0)
		, Bytes(0)
		, LastSeen(0.0)
		, DecompressedTotal(0)
		, Timestamp(0.0f)
	{
	}

	TArray<BYTE> Data;
	WORD Generation;
	WORD Bytes;
	DOUBLE LastSeen;
	WORD DecompressedTotal;
	DOUBLE Timestamp;
};

/**
 * This interface is an abstract mechanism for getting voice data. Each platform
 * implements a specific version of this interface.
 */
class FVoiceInterfaceSteamworks :
	public FVoiceInterfaceCommon
{
	/** Singleton instance pointer */
	static FVoiceInterfaceSteamworks* GVoiceInterface;


	typedef TMap<QWORD,UAudioComponent*> TalkingUserMap;

	/** The user that owns the voice devices (Unreal user index for splitscreen, not Steam user) */
	DWORD OwningIndex;

	/** Whether Steamworks let us use the microphone or not. */
	UBOOL bWasStartedOk;

	/** Whether the app is asking Steamworks to capture audio */
	UBOOL bIsCapturing;

	/** Decompressed audio packets go here. This just helps prevent constant allocation. */
	TArray<BYTE> DecompressBuffer;

	/** Map users to time when their audio playback will end. */
	TalkingUserMap TalkingUsers;

	/** Consistent, growable storage for reading raw audio samples. */
	TArray<BYTE> RawSamples;


	typedef TMap<QWORD,FQueuedSteamworksVoice> VoiceFragmentsMap;

	/** Data from Steamworks, waiting to send to network. */
	FQueuedSteamworksVoice QueuedSendData;

	/** Data from network, waiting to send to Steamworks. */
	VoiceFragmentsMap QueuedReceiveData;


	/** Volume for the VOIP AudioComponent */
	FLOAT VOIPVolumeMultiplier;


	/** whether or not to block 'NotifyVOIPPlaybackFinished' (used when calling 'Stop' on the audio component) */
	UBOOL bBlockFinishNotify;


	/** A pool of audio components; assigned to incoming voice data as playback is needed. */
	// @todo Steam: Remove this
	//TArray<UAudioComponent*> AudioComponentPool;

public:
	/** Reference to the Steamworks OnlineSubsystem which owns this VoiceEngine */
	UOnlineSubsystemSteamworks* OwningSubsystem;

	/** Steam momentarily keeps recording voice after being told to stop, for smooth playback; this keeps UE processing voice during that time */
	UBOOL bPendingFinalCapture;


protected:
	/** Simple constructor that zeros members. Hidden due to factory method */
	FVoiceInterfaceSteamworks()
		: OwningIndex(INVALID_OWNING_INDEX)
		, bWasStartedOk(FALSE)
		, bIsCapturing(FALSE)
		, VOIPVolumeMultiplier(1.0)
		, bBlockFinishNotify(FALSE)
		, OwningSubsystem(NULL)
		, bPendingFinalCapture(FALSE)
	{
	}

	/**
	 * Creates the Steamworks voice engine
	 *
	 * @param MaxLocalTalkers the maximum number of local talkers to support
	 * @param MaxRemoteTalkers the maximum number of remote talkers to support
	 * @param bIsSpeechRecognitionDesired whether speech recognition should be enabled
	 *
	 * @return TRUE if everything initialized correctly, FALSE otherwise
	 */
	virtual UBOOL Init( INT MaxLocalTalkers, INT MaxRemoteTalkers, UBOOL bIsSpeechRecognitionDesired );

	/**
	 * Determines if the specified index is the owner or not
	 *
	 * @param InIndex the index being tested
	 *
	 * @return TRUE if this is the owner, FALSE otherwise
	 */
	FORCEINLINE UBOOL IsOwningIndex(DWORD InIndex)
	{
		return InIndex >= 0 && InIndex < MAX_SPLITSCREEN_TALKERS && OwningIndex == InIndex;
	}

public:

	/** Destructor that releases the engine if allocated */
	virtual ~FVoiceInterfaceSteamworks(void)
	{
		if (bIsCapturing)
		{
			// Steam doesn't actually close the device, as it still uses it in the overlay for Steam Community chat, etc.
			GSteamUser->StopVoiceRecording();
			GSteamFriends->SetInGameVoiceSpeaking(GSteamUser->GetSteamID(), false);
		}
	}

	/**
	 * Returns an instance of the singleton object
	 *
	 * @param MaxLocalTalkers the maximum number of local talkers to support
	 * @param MaxRemoteTalkers the maximum number of remote talkers to support
	 * @param bIsSpeechRecognitionDesired whether speech recognition should be enabled
	 *
	 * @return A pointer to the singleton object or NULL if failed to init
	 */
	static FVoiceInterfaceSteamworks* CreateInstance( INT MaxLocalTalkers, INT MaxRemoteTalkers, UBOOL bIsSpeechRecognitionDesired )
	{
		if (GVoiceInterface == NULL)
		{
			GVoiceInterface = new FVoiceInterfaceSteamworks();
			// Init the Steamworks engine with those defaults

			if(GVoiceInterface->Init(MaxLocalTalkers, MaxRemoteTalkers, bIsSpeechRecognitionDesired) == FALSE)
			{
				delete GVoiceInterface;
				GVoiceInterface = NULL;
			}
		}
		return GVoiceInterface;
	}


	// FVoiceInterface

	/**
	 * Starts local voice processing for the specified user index
	 *
	 * @param UserIndex the user to start processing for
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD StartLocalVoiceProcessing(DWORD UserIndex);

	/**
	 * Stops local voice processing for the specified user index
	 *
	 * @param UserIndex the user to stop processing of
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD StopLocalVoiceProcessing(DWORD UserIndex);

	/**
	 * Starts remote voice processing for the specified user
	 *
	 * @param UniqueId the unique id of the user that will be talking
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD StartRemoteVoiceProcessing(FUniqueNetId UniqueId);

	/**
	 * Stops remote voice processing for the specified user
	 *
	 * @param UniqueId the unique id of the user that will no longer be talking
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD StopRemoteVoiceProcessing(FUniqueNetId UniqueId);

	/**
	 * Tells the voice system to start tracking voice data for speech recognition
	 *
	 * @param UserIndex the local user to recognize voice data for
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD StartSpeechRecognition(DWORD UserIndex);

	/**
	 * Tells the voice system to stop tracking voice data for speech recognition
	 *
	 * @param UserIndex the local user to recognize voice data for
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD StopSpeechRecognition(DWORD UserIndex);

	/**
	 * Registers the user index as a local talker (interested in voice data)
	 *
	 * @param UserIndex the user index that is going to be a talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD RegisterLocalTalker(DWORD UserIndex);

	/**
	 * Unregisters the user index as a local talker (not interested in voice data)
	 *
	 * @param UserIndex the user index that is no longer a talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD UnregisterLocalTalker(DWORD UserIndex);

	/**
	 * Registers the unique player id as a remote talker (submitted voice data only)
	 *
	 * @param UniqueId the id of the remote talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD RegisterRemoteTalker(FUniqueNetId UniqueId);

	/**
	 * Unregisters the unique player id as a remote talker
	 *
	 * @param UniqueId the id of the remote talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD UnregisterRemoteTalker(FUniqueNetId UniqueId);

	/**
	 * Checks whether a local user index has a headset present or not
	 *
	 * @param UserIndex the user to check status for
	 *
	 * @return TRUE if there is a headset, FALSE otherwise
	 */
    virtual UBOOL IsHeadsetPresent(DWORD UserIndex);

	/**
	 * Determines whether a local user index is currently talking or not
	 *
	 * @param UserIndex the user to check status for
	 *
	 * @return TRUE if the user is talking, FALSE otherwise
	 */
    virtual UBOOL IsLocalPlayerTalking(DWORD UserIndex);

	/**
	 * Determines whether a remote talker is currently talking or not
	 *
	 * @param UniqueId the unique id of the talker to check status on
	 *
	 * @return TRUE if the user is talking, FALSE otherwise
	 */
	virtual UBOOL IsRemotePlayerTalking(FUniqueNetId UniqueId);

	/**
	 * Returns which local talkers have data ready to be read from the voice system
	 *
	 * @return Bit mask of talkers that have data to be read (1 << UserIndex)
	 */
	virtual DWORD GetVoiceDataReadyFlags(void);

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
	virtual DWORD SetPlaybackPriority(DWORD UserIndex,FUniqueNetId RemoteTalkerId,DWORD Priority);

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
	virtual DWORD ReadLocalVoiceData(DWORD UserIndex,BYTE* Data,DWORD* Size);

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
	virtual DWORD SubmitRemoteVoiceData(FUniqueNetId RemoteTalkerId,BYTE* Data,DWORD* Size);

	/**
	 * Allows for platform specific servicing of devices, etc.
	 *
	 * @param DeltaTime the amount of time that has elapsed since the last update
	 */
	virtual void Tick(FLOAT DeltaTime);

	/**
	 * Called by the OnlineSubsytem, through the VOIP AudioComponent's OnAudioFinished delegate,
	 * for cleaning up 'TalkingUsers' AudioComponent references
	 * NOTE: This does not get called for audio components which have not yet called 'Play'
	 *
	 * @param VOIPAudioComponent	The AudioComponent which needs to be cleaned up
	 */
	void NotifyVOIPPlaybackFinished(class UAudioComponent* VOIPAudioComponent);

	/**
	 * Notification sent to the VOIP Subsystem, that pre-travel cleanup is occuring (so we can cleanup audio components)
	 *
	 * @param bSessionEnded		whether or not the game session has ended
	 */
	void NotifyCleanupWorld(UBOOL bSessionEnded);


	/**
	 * Kicks off VOIP recording in Steam (with additional checks for valid VOIP initialization)
	 */
	void InternalStartRecording();

	/**
	 * Requests that Steam end the current VOIP recording session; Steam will continue recording for half a second after this
	 */
	void InternalStopRecording();

	/**
	 * Called when Steam finally ends the VOIP recording session
	 */
	void InternalStoppedRecording();

	/**
	 * whether or not Steam is still recording VOIP data that we should handle
	 *
	 * @return	whether or not audio data is currently being recorded
	 */
	UBOOL InternalIsRecording();

	/**
	 * Checks the internal status of the VOIP recording state
	 * When the player stop recording VOIP, Steam continues recording for half a second, so this monitors for when that stops
	 */
	void InternalUpdateRecordingState();
};


#endif	//#if WITH_UE3_NETWORKING && WITH_STEAMWORKS
#endif	// !INCLUDED_ONLINESUBSYSTEMSTEAMWORKS_H


