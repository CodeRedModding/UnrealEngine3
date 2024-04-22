/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _VOICE_INTEFACE_H
#define _VOICE_INTEFACE_H

/**
 * Holds a word/phrase that was recognized by the speech analyzer
 *
 * @note Update OnlineSubsystem.uc if you change the layout of this struct
 */
struct FSpeechRecognizedWord
{
	/** The id of the word in the vocabulary */
	INT WordId;
	/** the actual word */
	FString WordText;
	/** How confident the analyzer was in the recognition */
	FLOAT Confidence;
};

/**
 * This interface is an abstract mechanism for getting voice data. Each platform
 * implements a specific version of this interface. The 
 */
class FVoiceInterface
{
public:
	/** Virtual destructor to force proper child cleanup */
	virtual ~FVoiceInterface(void)
	{
	}

	/**
	 * Starts local voice processing for the specified user index
	 *
	 * @param UserIndex the user to start processing for
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD StartLocalVoiceProcessing(DWORD UserIndex) = 0;

	/**
	 * Stops local voice processing for the specified user index
	 *
	 * @param UserIndex the user to stop processing of
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD StopLocalVoiceProcessing(DWORD UserIndex) = 0;

	/**
	 * Starts remote voice processing for the specified user
	 *
	 * @param UniqueId the unique id of the user that will be talking
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD StartRemoteVoiceProcessing(FUniqueNetId UniqueId) = 0;

	/**
	 * Stops remote voice processing for the specified user
	 *
	 * @param UniqueId the unique id of the user that will no longer be talking
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD StopRemoteVoiceProcessing(FUniqueNetId UniqueId) = 0;

	/**
	 * Registers the user index as a local talker (interested in voice data)
	 *
	 * @param UserIndex the user index that is going to be a talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD RegisterLocalTalker(DWORD UserIndex) = 0;

	/**
	 * Unregisters the user index as a local talker (not interested in voice data)
	 *
	 * @param UserIndex the user index that is no longer a talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD UnregisterLocalTalker(DWORD UserIndex) = 0;

	/**
	 * Registers the unique player id as a remote talker (submitted voice data only)
	 *
	 * @param UniqueId the id of the remote talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD RegisterRemoteTalker(FUniqueNetId UniqueId) = 0;

	/**
	 * Unregisters the unique player id as a remote talker
	 *
	 * @param UniqueId the id of the remote talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD UnregisterRemoteTalker(FUniqueNetId UniqueId) = 0;

	/**
	 * Checks whether a local user index has a headset present or not
	 *
	 * @param UserIndex the user to check status for
	 *
	 * @return TRUE if there is a headset, FALSE otherwise
	 */
    virtual UBOOL IsHeadsetPresent(DWORD UserIndex) = 0;

	/**
	 * Determines whether a local user index is currently talking or not
	 *
	 * @param UserIndex the user to check status for
	 *
	 * @return TRUE if the user is talking, FALSE otherwise
	 */
    virtual UBOOL IsLocalPlayerTalking(DWORD UserIndex) = 0;

	/**
	 * Determines whether a remote talker is currently talking or not
	 *
	 * @param UniqueId the unique id of the talker to check status on
	 *
	 * @return TRUE if the user is talking, FALSE otherwise
	 */
	virtual UBOOL IsRemotePlayerTalking(FUniqueNetId UniqueId) = 0;

	/**
	 * Returns which local talkers have data ready to be read from the voice system
	 *
	 * @return Bit mask of talkers that have data to be read (1 << UserIndex)
	 */
	virtual DWORD GetVoiceDataReadyFlags(void) = 0;

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
	virtual DWORD SetPlaybackPriority(DWORD UserIndex,FUniqueNetId RemoteTalkerId,DWORD Priority) = 0;

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
	virtual DWORD ReadLocalVoiceData(DWORD UserIndex,BYTE* Data,DWORD* Size) = 0;

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
	virtual DWORD SubmitRemoteVoiceData(FUniqueNetId RemoteTalkerId,BYTE* Data,DWORD* Size) = 0;

	/**
	 * Tells the voice system which vocabulary to use for the specified user
	 * when performing voice recognition
	 *
	 * @param UserIndex the local user to associate the vocabulary with
	 * @param VocabularyIndex the index of the vocabulary file to use
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD SelectVocabulary(DWORD UserIndex,DWORD VocabularyIndex) = 0;

	/**
	 * Tells the voice system to start tracking voice data for speech recognition
	 *
	 * @param UserIndex the local user to recognize voice data for
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD StartSpeechRecognition(DWORD UserIndex) = 0;

	/**
	 * Tells the voice system to stop tracking voice data for speech recognition
	 *
	 * @param UserIndex the local user to recognize voice data for
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD StopSpeechRecognition(DWORD UserIndex) = 0;

	/**
	 * Determines if the recognition process for the specified user has
	 * completed or not
	 *
	 * @param UserIndex the user that is being queried
	 *
	 * @return TRUE if completed, FALSE otherwise
	 */
	virtual UBOOL HasRecognitionCompleted(DWORD UserIndex) = 0;

	/**
	 * Gets the results of the voice recognition
	 *
	 * @param UserIndex the local user to read the results of
	 * @param Words the set of words that were recognized by the voice analyzer
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD GetRecognitionResults(DWORD UserIndex,TArray<FSpeechRecognizedWord>& Words) = 0;

	/**
	 * Allows for platform specific servicing of devices, etc.
	 *
	 * @param DeltaTime the amount of time that has elapsed since the last update
	 */
	virtual void Tick(FLOAT DeltaTime) = 0;

	/**
	 * Changes the object that is in use to the one specified
	 *
	 * @param LocalUserNum the local user that is making the change
	 * @param SpeechRecogObj the new object use
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD SetRecognitionObject(DWORD UserIndex,USpeechRecognition* SpeechObject) = 0;
};

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
FVoiceInterface* appCreateVoiceInterface(INT MaxLocalTalkers,INT MaxRemoteTalkers,UBOOL bIsSpeechRecognitionDesired);

#endif
