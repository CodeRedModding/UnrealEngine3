/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"

#include "VoiceInterfaceCommon.h"

/**
 * Tells the voice system which vocabulary to use for the specified user
 * when performing voice recognition
 *
 * @param UserIndex the local user to associate the vocabulary with
 * @param VocabularyIndex the index of the vocabulary file to use
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceCommon::SelectVocabulary( DWORD UserIndex, DWORD VocabularyIndex )
{
#if WITH_SPEECH_RECOGNITION
	if( SpeechRecogniser )
	{
		SpeechRecogniser->SelectVocabularies( UserIndex, VocabularyIndex );
	}
#endif
	return( TRUE );
}

/**
 * Creates the voice engine and performs any other initialization
 *
 * @param InMaxLocalTalkers the maximum number of local talkers to support
 * @param MaxRemoteTalkers the maximum number of remote talkers to support
 * @param bIsSpeechRecognitionDesired whether speech recognition should be enabled
 *
 * @return TRUE if everything initialized correctly, FALSE otherwise
 */
UBOOL FVoiceInterfaceCommon::Init( INT InMaxLocalTalkers, INT MaxRemoteTalkers, UBOOL bIsSpeechRecognitionDesired )
{
#if WITH_SPEECH_RECOGNITION
	MaxLocalTalkers = InMaxLocalTalkers;
#endif
	return( TRUE );
}

/**
 * Tells the voice system to start tracking voice data for speech recognition
 *
 * @param UserIndex the local user to recognize voice data for
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceCommon::StartSpeechRecognition( DWORD UserIndex )
{
#if WITH_SPEECH_RECOGNITION
	bIsSpeechRecogOn[UserIndex] = TRUE;

	// Reset the recognition state
	if( SpeechRecogniser )
	{
		SpeechRecogniser->RecogniseSpeech( UserIndex, NULL, -1 );
	}
#endif

#if VOICE_SAVE_DATA
	FString RecordedSoundName = FString::Printf( TEXT( "../../%sGame/RecordedSounds/Mic_%s.raw" ), appGetGameName(), *appSystemTimeString() );
	RecordedSound = GFileManager->CreateFileWriter( *RecordedSoundName );
#endif

	return( S_OK );
}

/**
 * Tells the voice system to stop tracking voice data for speech recognition
 *
 * @param UserIndex the local user to recognize voice data for
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceCommon::StopSpeechRecognition( DWORD UserIndex )
{
#if WITH_SPEECH_RECOGNITION
	bIsSpeechRecogOn[UserIndex] = FALSE;
#endif

#if VOICE_SAVE_DATA
	if( RecordedSound )
	{
		RecordedSound->Close();
		RecordedSound = NULL;
	}
#endif

	return( S_OK );
}

/**
 * Callback that occurs when raw data available for the speech recognition to process
 *
 * @param UserIndex the user the data is from
 * @param Data the buffer holding the data
 * @param Size the amount of data in the buffer
 */
void FVoiceInterfaceCommon::RawVoiceDataCallback( DWORD UserIndex, SWORD* Data, DWORD NumSamples )
{
#if VOICE_SAVE_DATA
	if( RecordedSound )
	{
		RecordedSound->Serialize( Data, NumSamples * sizeof( SWORD ) );
	}
#endif

#if WITH_SPEECH_RECOGNITION
	if( SpeechRecogniser )
	{
		if( bIsSpeechRecogOn[UserIndex] || ( ResultFlags[UserIndex] & UNSPEECH_DETECTED ) )
		{
			ResultFlags[UserIndex] |= SpeechRecogniser->RecogniseSpeech( UserIndex, Data, NumSamples );
		}
	}
#endif
}

/**
 * Determines if the recognition process for the specified user has
 * completed or not
 *
 * @param UserIndex the user that is being queried
 *
 * @return TRUE if completed, FALSE otherwise
 */
UBOOL FVoiceInterfaceCommon::HasRecognitionCompleted( DWORD UserIndex )
{
#if WITH_SPEECH_RECOGNITION
	if( SpeechRecogniser )
	{
		return( !!( ResultFlags[UserIndex] & UNRESULTS_AVAILABLE ) );
	}
#endif
	return( FALSE );
}

/**
 * Gets the results of the voice recognition
 *
 * @param UserIndex the local user to read the results of
 * @param Words the set of words that were recognized by the voice analyzer
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceCommon::GetRecognitionResults( DWORD UserIndex, TArray<FSpeechRecognizedWord>& Words )
{
#if WITH_SPEECH_RECOGNITION
	if( SpeechRecogniser )
	{
		SpeechRecogniser->GetResult( UserIndex, Words );
		ResultFlags[UserIndex] = 0;
		return( S_OK );
	}
#endif
	return( E_NOTIMPL );
}

/**
 * Changes the object that is in use to the one specified
 *
 * @param LocalUserNum the local user that is making the change
 * @param SpeechRecogObj the new object use
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceCommon::SetRecognitionObject( DWORD UserIndex, USpeechRecognition* SpeechObject )
{
#if WITH_SPEECH_RECOGNITION
	SpeechRecogniser = SpeechObject;
	if( SpeechRecogniser )
	{
		// Init the new one
		SpeechRecogniser->InitSpeechRecognition( MaxLocalTalkers );
	}
	return( S_OK );
#endif
	return( E_NOTIMPL );
}

