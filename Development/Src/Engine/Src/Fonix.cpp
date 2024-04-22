/*=============================================================================
	Fonix.cpp: UnrealEngine3 fonix speech recognition support
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "VoiceInterface.h"

#if WITH_SPEECH_RECOGNITION

#define MAX_VOCABULARIES	32

#if SUPPORTS_PRAGMA_PACK
#pragma pack( push, 8 )
#endif

// Common header file for all platforms (except PS3)
#include "../../../External/GamersSDK/4.2.1/include/VoiceCmds.h"
// Fonix does a no-no and #defines DWORD on PS3. sigh...
#if PS3
	#undef DWORD
#endif

#if _WINDOWS
#pragma comment( lib, "VoiceCmds.lib" )
#elif _XBOX
#pragma comment( lib, "VoiceCmds.lib" )
#elif PS3
#elif PLATFORM_UNIX
#else
#error "Undefined platform"
#endif

#if _WINDOWS
#include "../../../External/GamersSDK/4.2.1/tools/Win32/include/VocabLib.h"
#endif

#if SUPPORTS_PRAGMA_PACK
#pragma pack( pop )
#endif

// Fonix does horrible things with DWORD on PS3...
#if PS3
typedef unsigned int FONIX_DWORD;
#else
typedef DWORD FONIX_DWORD;
#endif


#endif // WITH_SPEECH_RECOGNITION

/** 
 * Saves the dictionary in the simplest format possible
 */
void FRecogVocabulary::OutputDictionary( TArrayNoInit<struct FRecognisableWord>& Dictionary, FString& Line )
{
#if !CONSOLE && WITH_SPEECH_RECOGNITION
	for( INT i = 0; i < Dictionary.Num(); i++  )
	{
		FRecognisableWord& Word = Dictionary( i );
		Line += FString::Printf( TEXT( "\"%s\"%%%d" ), *Word.PhoneticWord, Word.Id );

		if( i != Dictionary.Num() - 1 )
		{
			Line += TEXT( " | " );
		}
	}
#endif
}

UBOOL FRecogVocabulary::SaveDictionary( FString& TextFile )
{
#if !CONSOLE && WITH_SPEECH_RECOGNITION
	FString Line = TEXT( "$who = ( " );
	OutputDictionary( WhoDictionary, Line );
	Line += TEXT( " );\r\n\r\n" );

	Line += TEXT( "$what = ( " );
	OutputDictionary( WhatDictionary, Line );
	Line += TEXT( " );\r\n\r\n" );

	if( WhereDictionary.Num() )
	{
		Line += TEXT( "$where = ( " );
		OutputDictionary( WhereDictionary, Line );
		Line += TEXT( " );\r\n\r\n" );

		Line += TEXT( "$grammar = $who $what [$where];\r\n" );
	}
	else
	{
		Line += TEXT( "$grammar = $who $what;\r\n" );
	}

	appSaveStringToFile( Line, *TextFile );
#endif
	return( TRUE );
}

/** 
 * Creates the work data required for speech recognition
 */
UBOOL FRecogVocabulary::CreateSpeechRecognitionData( USpeechRecognition* Owner, FString Folder, INT Index )
{
#if !CONSOLE && WITH_SPEECH_RECOGNITION
	HRESULT Error;

	if( WhoDictionary.Num() < 1 || WhatDictionary.Num() < 1 )
	{
		appMsgf( AMT_OK, TEXT( "Need at least one entry in 'what' dictionary and one entry in 'where' dictionary." ) );
		return( FALSE );
	}

	FString ErrorString = TEXT( "" );
	FString Root = TEXT( "../../Development/External/GamersSDK/4.2.1/Languages/" );
	FString TextFile = TEXT( "Keywords.txt" );
	FString Dictionary = Root + Folder + TEXT( "/" ) + Folder + TEXT( ".pdc" );
	FString NeuralNet = Root + Folder + TEXT( "/" ) + Folder + TEXT( ".psi" );

	VocabName = FString::Printf( TEXT( "Vocab%d.xvocab" ), Index );

	SaveDictionary( TextFile );

	// Build the xvocab file
	Error = FnxVocabBld( ( TCHAR* )*TextFile, ( TCHAR* )*Dictionary, ( TCHAR* )*NeuralNet, ( TCHAR* )*VocabName, NULL, 1, 50.0f );
	ErrorString = Owner->HandleError( Error );
	if( ErrorString.Len() > 0 )
	{
		appMsgf( AMT_OK, TEXT( "Build vocab error: %s" ), *ErrorString );
		return( FALSE );
	}
#endif
	return( TRUE );
}

/** 
 * Loads the created vocabulary after it has been modified by BuildVoice
 */
UBOOL FRecogVocabulary::LoadSpeechRecognitionData( void )
{
	// Load in created files
	appLoadFileToArray( VocabData, *VocabName );

	WorkingVocabData.Empty();
	return( TRUE );
}

/** 
 * Initialise the vocabulary
 */
UBOOL FRecogVocabulary::InitSpeechRecognition( USpeechRecognition* Owner )
{
#if WITH_SPEECH_RECOGNITION
	FString ErrorString = "";

	// Load vocabulary
	WorkingVocabData = VocabData;

	HRESULT Result = FnxVocabInit( ( FnxVocabPtr )GetVocabData() );
	if( Result )
	{
		ErrorString = Owner->HandleError( Result );
		debugf( NAME_Warning, TEXT( "FAILED call to FnxVocabInit; error %s" ), *ErrorString );
		return( FALSE );
	}
#endif
	return( TRUE );
}

/** 
 * Clear out all the created vocab data
 */
void FRecogVocabulary::Clear( void )
{
	VocabData.Empty();
	WorkingVocabData.Empty();
}

/** 
 * Returns name of created vocab file
 */
FString FRecogVocabulary::GetVocabName( void )
{
	return( VocabName );
}

/** 
 * Returns address of converted vocab data
 */
void* FRecogVocabulary::GetVocabData( void )
{
	return( ( TCHAR* )WorkingVocabData.GetTypedData() );
}

/** 
 * Return the number of items in this vocabulary
 */
INT FRecogVocabulary::GetNumItems( void )
{
	return( WhoDictionary.Num() + WhatDictionary.Num() + WhereDictionary.Num() );
}

/** 
 * Return the number of bytes allocated by this resource
 */
INT FRecogVocabulary::GetResourceSize( void )
{
	return( WhoDictionary.GetAllocatedSize() + WhatDictionary.GetAllocatedSize() + WhereDictionary.GetAllocatedSize() + VocabData.GetAllocatedSize() );
}

/** 
 * Looks up the word in the dictionary.  Returns the reference word.
 */
FString FRecogVocabulary::GetStringFromWordId( DWORD WordId )
{
#if WITH_SPEECH_RECOGNITION
	for( INT i = 0; i < WhoDictionary.Num(); i++ )
	{
		if( WhoDictionary( i ).Id == WordId )
		{
			return( WhoDictionary( i ).ReferenceWord );
		}
	}

	for( INT i = 0; i < WhatDictionary.Num(); i++ )
	{
		if( WhatDictionary( i ).Id == WordId )
		{
			return( WhatDictionary( i ).ReferenceWord );
		}
	}

	for( INT i = 0; i < WhereDictionary.Num(); i++ )
	{
		if( WhereDictionary( i ).Id == WordId )
		{
			return( WhereDictionary( i ).ReferenceWord );
		}
	}
#endif
	return( "" );
}

/** 
 * Convert the language code to Fonix folder
 */
FString USpeechRecognition::GetLanguageFolder( void )
{
	FString Folder = TEXT( "" );
#if !CONSOLE && WITH_SPEECH_RECOGNITION
	if( Language == TEXT( "INT" ) )
	{
		Folder = TEXT( "USEnglish" );
	}
	else if( Language == TEXT( "JPN" ) )
	{
		Folder = TEXT( "Japanese" );
	}
	else if( Language == TEXT( "DEU" ) )
	{
		Folder = TEXT( "German" );
	}
	else if( Language == TEXT( "FRA" ) )
	{
		Folder = TEXT( "French" );
	}
	else if( Language == TEXT( "ESN" ) )
	{
		Folder = TEXT( "Spanish" );
	}
	else if( Language == TEXT( "ITA" ) )
	{
		Folder = TEXT( "Italian" );
	}
	else if( Language == TEXT( "KOR" ) )
	{
		Folder = TEXT( "Korean" );
	}
#endif
	return( Folder );
}

/** 
 * Manages any error codes that may have occurred
 */
FString USpeechRecognition::HandleError( INT Error )
{
	FString ErrorString = TEXT( "" );
#if WITH_SPEECH_RECOGNITION
	switch( Error )
	{
	case 0:
		break;

	case FNX_TOO_MANY_VOCABS:
		ErrorString = TEXT( "Too many vocabs" );
		break;

	case FNX_NODE_OVERFLOW:
		ErrorString = TEXT( "Node overflow" );
		break;

	case FNX_NULL_POINTER:
		ErrorString = TEXT( "NULL pointer passed in" );
		break;

	case FNX_NOT_INITIALIZED:
		ErrorString = TEXT( "Uninitialised" );
		break;

	case FNX_WRONG_TYPE:
		ErrorString = TEXT( "Wrong type; voice passed in as vocab?" );
		break;

	case FNX_INCOMPLETE:
		ErrorString = TEXT( "Truncated data" );
		break;

	case FNX_SPEECH_ERROR:
		ErrorString = TEXT( "Could not reset recogniser for unexpected reason" );
		break;

	case FNX_VERSION_MISMATCH:
		ErrorString = TEXT( "Version mismatch" );
		break;

	case FNX_TAG_MISMATCH:
		ErrorString = TEXT( "Mismatched tags" );
		break;

	case FNX_UNALIGNED_BLOCK:
		ErrorString = TEXT( "Data not aligned to DWORD" );
		break;

#if _WINDOWS
	case FNX_PNI_READ_ERROR:
		ErrorString = TEXT( "Failed to read .pni file" );
		break;

	case FNX_VOCAB_READ_ERROR:
		ErrorString = TEXT( "Failed to read .xvocab file" );
		break;

	case FNX_DICTIONARY_READ_ERROR:
		ErrorString = TEXT( "Failed to read .pdc file" );
		break;

	case FNX_WRITE_ERROR:
		ErrorString = TEXT( "Error writing file" );
		break;

	case FNX_WORDLIST_READ_ERROR:
		ErrorString = TEXT( "Failed to read wordlist file" );
		break;

	case FNX_INIT_ERROR:
		ErrorString = TEXT( "Failed to initialise converter" );
		break;
#endif
	default:
		ErrorString = TEXT( "Unknown error" );
		break;
	}
#endif
	return( ErrorString );
}

/**
 * Validates a word from a dictionary
 */
UBOOL USpeechRecognition::ValidateRecognitionItem( BYTE* UniqueIDs, FRecognisableWord& Word )
{
	INT UniqueID = Word.Id;
	if( UniqueID < 1 || UniqueID > 4095 )
	{
		appMsgf( AMT_OK, TEXT( "Unique ID '%d' for '%s' is out of range, it needs to be between 1 and 4095 inclusive" ), UniqueID, *Word.PhoneticWord );
		return( FALSE );
	}

	if( UniqueIDs[UniqueID] != 0 )
	{
		appMsgf( AMT_OK, TEXT( "Unique ID '%d' for '%s' is invalid, it needs to be unique" ), UniqueID, *Word.PhoneticWord );
		return( FALSE );
	}

	UniqueIDs[UniqueID] = 1;
	return( TRUE );
}

/**
 * Validates the source speech recognition data
 */
UBOOL USpeechRecognition::ValidateRecognitionData( void )
{
	BYTE* UniqueIDs = ( BYTE* )appAlloca( 4096 );
	appMemzero( UniqueIDs, 4096 );

	for( INT i = 0; i < Vocabularies.Num(); i++ )
	{
		FRecogVocabulary& Vocab = Vocabularies( i );
		for( INT j = 0; j < Vocab.WhoDictionary.Num(); j++ )
		{
			if( !ValidateRecognitionItem( UniqueIDs, Vocab.WhoDictionary( j ) ) )
			{
				return( FALSE );
			}
		}

		for( INT j = 0; j < Vocab.WhatDictionary.Num(); j++ )
		{
			if( !ValidateRecognitionItem( UniqueIDs, Vocab.WhatDictionary( j ) ) )
			{
				return( FALSE );
			}
		}

		for( INT j = 0; j < Vocab.WhereDictionary.Num(); j++ )
		{
			if( !ValidateRecognitionItem( UniqueIDs, Vocab.WhereDictionary( j ) ) )
			{
				return( FALSE );
			}
		}
	}

	return( TRUE );
}

/** 
 * Creates the work data required for speech recognition
 */
UBOOL USpeechRecognition::CreateSpeechRecognitionData( void )
{
#if !CONSOLE && WITH_SPEECH_RECOGNITION
	HRESULT	Error;

	if( Vocabularies.Num() > MAX_VOCABULARIES )
	{
		appMsgf( AMT_OK, TEXT( "Too many vocabularies, there are %d, the max is %d" ), Vocabularies.Num(), MAX_VOCABULARIES );
		return( FALSE );
	}

	FString ErrorString = TEXT( "" );
	FString Root = TEXT( "../../Development/External/GamersSDK/4.2.1/Languages/" );
	FString Folder = GetLanguageFolder();
	if( Folder.Len() == 0 )
	{
		// CHI, POR, ESM, ESN and UKEnglish is not supported by UE3
		appMsgf( AMT_OK, TEXT( "Unsupported language: %s" ), *Language );
		return( FALSE );
	}

	// Convert all the vocabularies
	for( INT i = 0; i < Vocabularies.Num(); i++ )
	{
		if( !Vocabularies( i ).CreateSpeechRecognitionData( this, Folder, i ) )
		{
			return( FALSE );
		}
	}

	FString Dictionary = Root + Folder + TEXT( "/" ) + Folder + TEXT( ".pdc" );
	FString NeuralNet = Root + Folder + TEXT( "/" ) + Folder + TEXT( ".psi" );

	FString OutputVoice = TEXT( "OutputVoice.vnn" );
	FString OutputVoiceUsr = TEXT( "OutputVoiceUsr.usr" );

	// Build the voice and user files
	TCHAR VocabFilenames[MAX_VOCABULARIES][64];
	TCHAR* VocabFiles[MAX_VOCABULARIES + 1];
	for( INT i = 0; i < Vocabularies.Num(); i++ )
	{
		appStrcpy( VocabFilenames[i], *Vocabularies( i ).GetVocabName() );
		VocabFiles[i] = VocabFilenames[i];
	}
	VocabFiles[Vocabularies.Num()] = NULL;

	Error = FnxBuildVoice( ( TCHAR* )*OutputVoice, ( TCHAR* )*OutputVoiceUsr, ( TCHAR* )*NeuralNet, VocabFiles, Vocabularies.Num(), Vocabularies.Num(), 0, NULL, 0, NULL );	

	ErrorString = HandleError( Error );
	if( ErrorString.Len() > 0 )
	{
		appMsgf( AMT_OK, TEXT( "Build voice error: %s" ), *ErrorString );
		return( FALSE );
	}

	// Load in created files
	appLoadFileToArray( VoiceData, *OutputVoice );
	WorkingVoiceData.Empty();

	appLoadFileToArray( UserData, *OutputVoiceUsr );

	for( INT i = 0; i < Vocabularies.Num(); i++ )
	{
		Vocabularies( i ).LoadSpeechRecognitionData();
	}
#endif

	return( TRUE );
}

/** 
 * Callback after any property has changed
 */
void USpeechRecognition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
#if !CONSOLE && WITH_SPEECH_RECOGNITION
	Super::PostEditChangeProperty(PropertyChangedEvent);

	bDirty = TRUE;
	MarkPackageDirty();

	GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI, this));
#endif
}

/**
 * Called before the package is saved
 */
void USpeechRecognition::PreSave( void )
{
#if !CONSOLE && WITH_SPEECH_RECOGNITION
	if( bDirty )
	{
		// Clear out any old data
		for( INT i = 0; i < Vocabularies.Num(); i++ )
		{
			Vocabularies( i ).Clear();
		}

		VoiceData.Empty();
		WorkingVoiceData.Empty();
		UserData.Empty();

		if( ValidateRecognitionData() )
		{
			// Recreate new user data	
			if( CreateSpeechRecognitionData() )
			{
				bDirty = FALSE;
			}
		}
	}
#endif
}

/** 
 * Returns a one line description of an object for viewing in the generic browser
 */
FString USpeechRecognition::GetDesc( void )
{
	INT Count = 0;

	for( INT i = 0; i < Vocabularies.Num(); i++ )
	{
		Count += Vocabularies( i ).GetNumItems();
	}

	return( FString::Printf( TEXT( "Items: %d" ), Count ) );
}

/** 
 * Returns detailed info to populate listview columns
 */
FString USpeechRecognition::GetDetailedDescription( INT InIndex )
{
	INT Count = 0;
	FString Description = TEXT( "" );
	switch( InIndex )
	{
	case 0:
		for( INT i = 0; i < Vocabularies.Num(); i++ )
		{
			Count += Vocabularies( i ).GetNumItems();
		}
		Description = FString::Printf( TEXT( "%d items" ), Count );
		break;

	default:
		break;
	}

	return( Description );
}

/**
 * Returns the size of the asset.
 */
INT USpeechRecognition::GetResourceSize( void )
{
	INT Size = 0;

	for( INT i = 0; i < Vocabularies.Num(); i++ )
	{
		Size += Vocabularies( i ).GetResourceSize();
	}

	Size += VoiceData.GetAllocatedSize();
	Size += UserData.GetAllocatedSize();

	return( Size );
}

/** 
 * Initialise the recogniser
 */
UBOOL USpeechRecognition::InitSpeechRecognition( INT MaxLocalTalkers )
{
#if WITH_SPEECH_RECOGNITION
	if( bInitialised )
	{
		return( TRUE );
	}

	INT		Result;
	FString	ErrorString;

	if( !VoiceData.Num() || !UserData.Num() )
	{
		debugf( NAME_Warning, TEXT( "No speech recognition data available" ) );
		return( FALSE );
	}

	// Load vocabulary
	for( INT i = 0; i < Vocabularies.Num(); i++ )
	{
		if( !Vocabularies( i ).InitSpeechRecognition( this ) )
		{
			return( FALSE );
		}
	}

	WorkingVoiceData = VoiceData;
	FnxVoiceData = ( FnxVoicePtr )WorkingVoiceData.GetTypedData();

	// Initialize the common voice recognition block (or set m_err to FNX_NULL_POINTER if the read failed)
	Result = FnxVoiceInit( FnxVoiceData );
	if( Result )
	{
		debugf( NAME_Warning, TEXT( "FAILED call to FnxVoiceInit; error %d" ), Result );
		return( FALSE );
	}

	for( INT i = 0; i < MaxLocalTalkers; i++ )
	{
		InstanceData[i].UserData = UserData;
		FnxVoicePtr FnxUserData = ( FnxVoicePtr )InstanceData[i].UserData.GetTypedData();

		Result = FnxVoiceUserInit( FnxVoiceData, FnxUserData );
		if( Result )
		{
			ErrorString = HandleError( Result );
			debugf( NAME_Warning, TEXT( "FAILED call to FnxVoiceUserInit; error %s" ), *ErrorString );
			return( FALSE );
		}

		// Select all vocabularies by default
		if( !SelectVocabularies( i, (DWORD)-1 ) )
		{
			return( FALSE );
		}
	}

	bInitialised = TRUE;
#endif // WITH_SPEECH_RECOGNITION
	return( TRUE );
}

/** 
 * Select vocabularies
 */
UBOOL USpeechRecognition::SelectVocabularies( DWORD LocalTalker, DWORD VocabBitField )
{
#if WITH_SPEECH_RECOGNITION
	HRESULT Result;

	FString ErrorString = "";
	FnxVocabPtr VocabData[MAX_VOCABULARIES];
	INT j = 0;

	for( INT i = 0; i < Vocabularies.Num(); i++ )
	{
		if( VocabBitField & ( 1 << i ) )
		{
			VocabData[j] = Vocabularies( i ).GetVocabData();
			j++;
		}
	}

	FnxVoicePtr FnxUserData = ( FnxVoicePtr )InstanceData[LocalTalker].UserData.GetTypedData();
	Result = FnxSelectXVocabs( FnxUserData, VocabData, j );
	if( Result )
	{
		ErrorString = HandleError( Result );
		debugf( NAME_Warning, TEXT( "FAILED call to FnxSelectXVocabs; error %s" ), *ErrorString );
		return( FALSE );
	}
#endif
	return( TRUE );
}

/** 
 * Process input samples
 */
DWORD USpeechRecognition::RecogniseSpeech( DWORD UserIndex, SWORD* Samples, INT NumSamples )
{
#if !WITH_SPEECH_RECOGNITION
	DWORD ResultFlags = 0;
#endif

#if WITH_SPEECH_RECOGNITION
	FONIX_DWORD ResultFlags = 0;
	LONG Result;

	if( !bInitialised )
	{
		return( 0 );
	}

	// Send in some wave data and do some processing (can safely handle NULL Samples and 0 NumSamples)
	FnxVoicePtr FnxUserData = ( FnxVoicePtr )InstanceData[UserIndex].UserData.GetTypedData();
	Result = FnxVoiceRecognize( FnxVoiceData, FnxUserData, Samples, NumSamples, &ResultFlags );

#endif // WITH_SPEECH_RECOGNITION
	return( ResultFlags );
}

/**
 * Returns the recognised words from the input samples (currently only 1)
 */
UBOOL USpeechRecognition::GetResult( DWORD UserIndex, TArray<FSpeechRecognizedWord>& Words )
{
#if WITH_SPEECH_RECOGNITION
	FSpeechRecognizedWord Word;
	int NumBest;
	FLOAT* TempConfidence;
	FONIX_DWORD** TempWordID;
	FONIX_DWORD* NumWords;
	FString Phrase;

	FnxVoicePtr FnxUserData = ( FnxVoicePtr )InstanceData[UserIndex].UserData.GetTypedData();
	HRESULT Res = FnxVoiceGetResultsGrammar( FnxVoiceData, FnxUserData, &TempWordID, &NumWords, &TempConfidence, &NumBest );
	if (Res != 0)
	{
		debugf( NAME_DevAudio, TEXT("FnxVoiceGetResultsGrammar failed with return value %d"), Res);
		return FALSE;
	}

	Word.Confidence = *TempConfidence;

	if( *TempConfidence > ConfidenceThreshhold )
	{
		for( DWORD i = 0; i < *NumWords; i++ )
		{
			Word.WordId = ( *TempWordID )[i];
			INT Idx = Words.AddItem( Word );
			Words( Idx ).WordText = GetStringFromWordId( Word.WordId );

			Phrase += Words( Idx ).WordText + TEXT( " " );
		}

		debugf( NAME_DevAudio, TEXT( "Recognised: %s (%f)" ), *Phrase, *TempConfidence );
	}
	else
	{
		for( DWORD i = 0; i < *NumWords; i++ )
		{
			Phrase += GetStringFromWordId( ( *TempWordID )[i] ) + TEXT( " " );
		}
		debugf( NAME_DevAudio, TEXT( "Low confidence: %s (%f)" ), *Phrase, *TempConfidence );
	}

	return( *NumWords > 0 );
#else
	return( FALSE );
#endif
}

/** 
 * Looks up the word in the dictionary
 * Linear search - if the dictionaries get large, this should be converted to a map
 */
FString USpeechRecognition::GetStringFromWordId( DWORD WordId )
{
	FString Result = "";

#if WITH_SPEECH_RECOGNITION
	for( INT i = 0; i < Vocabularies.Num(); i++ )
	{
		Result = Vocabularies( i ).GetStringFromWordId( WordId );
		if( Result.Len() > 0 )
		{
			break;
		}
	}
#endif
	return( Result );
}

IMPLEMENT_CLASS( USpeechRecognition );

// end
