/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemPC.h"

#if WITH_UE3_NETWORKING

#include "VoiceInterfaceCommon.h"

/**
 * This interface is an abstract mechanism for getting voice data. Each platform
 * implements a specific version of this interface. The 
 */
class FVoiceInterfacePC :
	public FVoiceInterfaceCommon
{
	/** Singleton instance pointer */
	static FVoiceInterfacePC* GVoiceInterface;

	/** Whether the audio device is recording input from the mic */
	UBOOL bAlreadyRecording;

	/** Simple constructor that zeros members. Hidden due to factory method */
	FVoiceInterfacePC( void )
	{
	}

public:
	/** Destructor that releases the engine if allocated */
	virtual ~FVoiceInterfacePC( void )
	{
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
	static FVoiceInterfacePC* CreateInstance( INT MaxLocalTalkers, INT MaxRemoteTalkers, UBOOL bIsSpeechRecognitionDesired );

	// FVoiceInterface

	/**
	 * Starts local voice processing for the specified user index
	 *
	 * @param UserIndex the user to start processing for
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD StartLocalVoiceProcessing( DWORD UserIndex ) { return( 0 ); }

	/**
	 * Stops local voice processing for the specified user index
	 *
	 * @param UserIndex the user to stop processing of
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD StopLocalVoiceProcessing( DWORD UserIndex ) { return( 0 ); }

	/**
	 * Starts remote voice processing for the specified user
	 *
	 * @param UniqueId the unique id of the user that will be talking
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD StartRemoteVoiceProcessing( FUniqueNetId UniqueId ) { return( 0 ); }

	/**
	 * Stops remote voice processing for the specified user
	 *
	 * @param UniqueId the unique id of the user that will no longer be talking
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD StopRemoteVoiceProcessing( FUniqueNetId UniqueId ) { return( 0 ); }

	/**
	 * Registers the user index as a local talker (interested in voice data)
	 *
	 * @param UserIndex the user index that is going to be a talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD RegisterLocalTalker( DWORD UserIndex ) { return( 0 ); }

	/**
	 * Unregisters the user index as a local talker (not interested in voice data)
	 *
	 * @param UserIndex the user index that is no longer a talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD UnregisterLocalTalker( DWORD UserIndex ) { return( 0 ); }

	/**
	 * Registers the unique player id as a remote talker (submitted voice data only)
	 *
	 * @param UniqueId the id of the remote talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD RegisterRemoteTalker( FUniqueNetId UniqueId ) { return( 0 ); }

	/**
	 * Unregisters the unique player id as a remote talker
	 *
	 * @param UniqueId the id of the remote talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD UnregisterRemoteTalker( FUniqueNetId UniqueId ) { return( 0 ); }

	/**
	 * Checks whether a local user index has a headset present or not
	 *
	 * @param UserIndex the user to check status for
	 *
	 * @return TRUE if there is a headset, FALSE otherwise
	 */
	virtual UBOOL IsHeadsetPresent( DWORD UserIndex ) { return( FALSE ); }

	/**
	 * Determines whether a local user index is currently talking or not
	 *
	 * @param UserIndex the user to check status for
	 *
	 * @return TRUE if the user is talking, FALSE otherwise
	 */
	virtual UBOOL IsLocalPlayerTalking( DWORD UserIndex ) { return( FALSE ); }

	/**
	 * Determines whether a remote talker is currently talking or not
	 *
	 * @param UniqueId the unique id of the talker to check status on
	 *
	 * @return TRUE if the user is talking, FALSE otherwise
	 */
	virtual UBOOL IsRemotePlayerTalking( FUniqueNetId UniqueId ) { return( FALSE ); }

	/**
	 * Returns which local talkers have data ready to be read from the voice system
	 *
	 * @return Bit mask of talkers that have data to be read (1 << UserIndex)
	 */
	virtual DWORD GetVoiceDataReadyFlags( void ) { return( 0 ); }

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
	virtual DWORD SetPlaybackPriority( DWORD UserIndex, FUniqueNetId RemoteTalkerId, DWORD Priority ) { return( 0 ); }

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
	virtual DWORD ReadLocalVoiceData( DWORD UserIndex, BYTE* Data, DWORD* Size ) { return( 0 ); }

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
	virtual DWORD SubmitRemoteVoiceData( FUniqueNetId RemoteTalkerId, BYTE* Data, DWORD* Size ) { return( 0 ); }

	/**
	 * Tells the voice system to start tracking voice data for speech recognition
	 *
	 * @param UserIndex the local user to recognize voice data for
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD StartSpeechRecognition( DWORD UserIndex );

	/**
	 * Allows for platform specific servicing of devices, etc.
	 *
	 * @param DeltaTime the amount of time that has elapsed since the last update
	 */
	virtual void Tick(FLOAT DeltaTime) {}
};

/** Singleton instance pointer initialization */
FVoiceInterfacePC* FVoiceInterfacePC::GVoiceInterface = NULL;

/**
 * Returns an instance of the singleton object
 *
 * @param MaxLocalTalkers the maximum number of local talkers to support
 * @param MaxRemoteTalkers the maximum number of remote talkers to support
 * @param bIsSpeechRecognitionDesired whether speech recognition should be enabled
 *
 * @return A pointer to the singleton object or NULL if failed to init
 */
FVoiceInterfacePC* FVoiceInterfacePC::CreateInstance( INT MaxLocalTalkers, INT MaxRemoteTalkers, UBOOL bIsSpeechRecognitionDesired )
{
	if( GVoiceInterface == NULL )
	{
		GVoiceInterface = new FVoiceInterfacePC();
		// Init OpenAL with the relevant data
		if( GVoiceInterface->Init( MaxLocalTalkers, MaxRemoteTalkers, bIsSpeechRecognitionDesired ) == FALSE )
		{
			delete GVoiceInterface;
			GVoiceInterface = NULL;
		}
	}
	return( GVoiceInterface );
}

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
FVoiceInterface* appCreateVoiceInterfacePC(INT MaxLocalTalkers,INT MaxRemoteTalkers,UBOOL bIsSpeechRecognitionDesired)
{
	return( FVoiceInterfacePC::CreateInstance( MaxLocalTalkers, MaxRemoteTalkers, bIsSpeechRecognitionDesired ) );
}

/**
 * Tells the voice system to start tracking voice data for speech recognition
 *
 * @param UserIndex the local user to recognize voice data for
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfacePC::StartSpeechRecognition( DWORD UserIndex )
{
#if WITH_SPEECH_RECOGNITION
	return( FVoiceInterfaceCommon::StartSpeechRecognition( UserIndex ) );
#else
	return( 0 );
#endif
}

// end
#endif	//#if WITH_UE3_NETWORKING
