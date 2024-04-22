/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __VOICEINTERFACECOMMON_H__
#define __VOICEINTERFACECOMMON_H__

#include "VoiceInterface.h"

#ifndef E_FAIL
#define E_FAIL (DWORD)-1
#endif

#ifndef E_NOTIMPL
#define E_NOTIMPL (DWORD)-2
#endif

#ifndef ERROR_IO_PENDING
#define ERROR_IO_PENDING 997
#endif

#ifndef S_OK
#define S_OK 0
#endif

#define UNRESULTS_AVAILABLE 1
#define UNSPEECH_DETECTED   2

#define VOICE_SAVE_DATA		0

/**
 * This interface is an abstract mechanism for getting voice data. Each platform
 * implements a specific version of this interface.
 */
class FVoiceInterfaceCommon :
	public FVoiceInterface
{
public:

#if WITH_SPEECH_RECOGNITION
	/** Whether the speech recognition should happen or not for upto 4 users */
	UBOOL bIsSpeechRecogOn[4];
	/** Flags set by the recognition process */
	DWORD ResultFlags[4];
	/** Speech recognition interface */
	USpeechRecognition* SpeechRecogniser;
	/** Cached max local talkers count */
	DWORD MaxLocalTalkers;

	/** Simple constructor that zeros members. Hidden due to factory method */
	FVoiceInterfaceCommon( void ) :
		SpeechRecogniser(NULL)
	{
		appMemzero(bIsSpeechRecogOn,sizeof(UBOOL) * 4);
		appMemzero(ResultFlags,sizeof(DWORD) * 4);
#if VOICE_SAVE_DATA
		RecordedSound = NULL;
#endif
	}
#endif

	/** Destructor that releases the engine if allocated */
	virtual ~FVoiceInterfaceCommon( void )
	{
	}

#if VOICE_SAVE_DATA
	FArchive* RecordedSound;
#endif

	/**
	 * Creates the XHV engine and performs any other initialization
	 *
	 * @param MaxLocalTalkers the maximum number of local talkers to support
	 * @param MaxRemoteTalkers the maximum number of remote talkers to support
	 * @param bIsSpeechRecognitionDesired whether speech recognition should be enabled
	 *
	 * @return TRUE if everything initialized correctly, FALSE otherwise
	 */
	virtual UBOOL Init( INT MaxLocalTalkers, INT MaxRemoteTalkers, UBOOL bIsSpeechRecognitionDesired );

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
	 * Tells the voice system which vocabulary to use for the specified user
	 * when performing voice recognition
	 *
	 * @param UserIndex the local user to associate the vocabulary with
	 * @param VocabularyIndex the index of the vocabulary file to use
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD SelectVocabulary( DWORD UserIndex, DWORD VocabularyIndex );

	/**
	 * Tells the voice system to start tracking voice data for speech recognition
	 *
	 * @param UserIndex the local user to recognize voice data for
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD StartSpeechRecognition( DWORD UserIndex );

	/**
	 * Tells the voice system to stop tracking voice data for speech recognition
	 *
	 * @param UserIndex the local user to recognize voice data for
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD StopSpeechRecognition( DWORD UserIndex );

	/**
	 * Callback that occurs when raw data available for the speech recognition to process
	 *
	 * @param UserIndex the user the data is from
	 * @param Data the buffer holding the data
	 * @param Size the amount of data in the buffer
	 */
	virtual void RawVoiceDataCallback( DWORD UserIndex, SWORD* Data, DWORD NumSamples );

	/**
	 * Determines if the recognition process for the specified user has
	 * completed or not
	 *
	 * @param UserIndex the user that is being queried
	 *
	 * @return TRUE if completed, FALSE otherwise
	 */
	virtual UBOOL HasRecognitionCompleted( DWORD UserIndex );

	/**
	 * Gets the results of the voice recognition
	 *
	 * @param UserIndex the local user to read the results of
	 * @param Words the set of words that were recognized by the voice analyzer
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD GetRecognitionResults( DWORD UserIndex, TArray<FSpeechRecognizedWord>& Words );

	/**
	 * Changes the object that is in use to the one specified
	 *
	 * @param LocalUserNum the local user that is making the change
	 * @param SpeechRecogObj the new object use
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD SetRecognitionObject( DWORD UserIndex,USpeechRecognition* SpeechObject );
};

// end

#endif
