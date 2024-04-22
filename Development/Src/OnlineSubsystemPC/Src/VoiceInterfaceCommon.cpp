/**
 * Copyright © 1998-2010 Epic Games, Inc. All Rights Reserved.
 */

#if _WINDOWS
#include "OnlineSubsystemPC.h"
#elif _XBOX
#include "OnlineSubsystemLive.h"
#endif

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
	return( E_NOTIMPL );
}

/**
 * Creates the voice engine and performs any other initialization
 *
 * @param MaxLocalTalkers the maximum number of local talkers to support
 * @param MaxRemoteTalkers the maximum number of remote talkers to support
 * @param bIsSpeechRecognitionDesired whether speech recognition should be enabled
 *
 * @return TRUE if everything initialized correctly, FALSE otherwise
 */
UBOOL FVoiceInterfaceCommon::Init( INT MaxLocalTalkers, INT MaxRemoteTalkers, UBOOL bIsSpeechRecognitionDesired )
{
#if WITH_SPEECH_RECOGNITION
	// Only init speech recognition if desired by the game
	if( bIsSpeechRecognitionDesired )
	{
		SpeechRecogniser = LoadObject<USpeechRecognition>( NULL, TEXT( "SpeechRecognition.Alphabet" ), NULL, LOAD_None, NULL );
		if( SpeechRecogniser )
		{
			SpeechRecogniser->InitSpeechRecognition( MaxLocalTalkers );
			SpeechRecogniser->AddToRoot();
		}
	}
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
	return( S_OK );
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
	return( TRUE );
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
		if( SpeechRecogniser->GetResult( UserIndex, Words ) )
		{
			ResultFlags[UserIndex] = 0;
		}
		return( S_OK );
	}
#endif
	return( E_NOTIMPL );
}

// end
