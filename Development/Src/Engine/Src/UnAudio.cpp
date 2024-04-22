/*=============================================================================
	UnAudio.cpp: Unreal base audio.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSoundClasses.h"
#include "UnAudioEffect.h"
#include "UnSubtitleManager.h"
#include "EngineSplineClasses.h"

IMPLEMENT_CLASS( UAudioDevice );
IMPLEMENT_CLASS( USoundClass );
IMPLEMENT_CLASS( USoundMode );
IMPLEMENT_CLASS( AAmbientSound );
IMPLEMENT_CLASS( AAmbientSoundMovable );
IMPLEMENT_CLASS( AAmbientSoundSimple );
IMPLEMENT_CLASS( AAmbientSoundNonLoop );
IMPLEMENT_CLASS( AAmbientSoundSimpleToggleable );
IMPLEMENT_CLASS( AAmbientSoundNonLoopingToggleable );
IMPLEMENT_CLASS( UDrawSoundRadiusComponent );
IMPLEMENT_CLASS( AAmbientSoundSimpleSpline );
IMPLEMENT_CLASS( AAmbientSoundSpline );
IMPLEMENT_CLASS( USplineAudioComponent );
IMPLEMENT_CLASS( USimpleSplineAudioComponent );
IMPLEMENT_CLASS( AAmbientSoundSplineMultiCue );
IMPLEMENT_CLASS( UMultiCueSplineAudioComponent );
IMPLEMENT_CLASS( USimpleSplineNonLoopAudioComponent );

/** Audio stats */
DECLARE_STATS_GROUP( TEXT( "Audio" ), STATGROUP_Audio );
DECLARE_DWORD_COUNTER_STAT( TEXT( "Audio Components" ), STAT_AudioComponents, STATGROUP_Audio );
DECLARE_DWORD_COUNTER_STAT( TEXT( "Audio Sources" ), STAT_AudioSources, STATGROUP_Audio );
DECLARE_DWORD_COUNTER_STAT( TEXT( "Wave Instances" ), STAT_WaveInstances, STATGROUP_Audio );
DECLARE_DWORD_COUNTER_STAT( TEXT( "Wave Instances Dropped" ), STAT_WavesDroppedDueToPriority, STATGROUP_Audio );
DECLARE_DWORD_COUNTER_STAT( TEXT( "Audible Wave Instances Dropped" ), STAT_AudibleWavesDroppedDueToPriority, STATGROUP_Audio );
DECLARE_DWORD_COUNTER_STAT( TEXT( "Finished delegates called" ), STAT_AudioFinishedDelegatesCalled, STATGROUP_Audio );
DECLARE_CYCLE_STAT( TEXT( "Finished delegates time" ), STAT_AudioFinishedDelegates, STATGROUP_Audio );
DECLARE_MEMORY_STAT( TEXT( "Audio Memory Used" ), STAT_AudioMemorySize, STATGROUP_Audio );
DECLARE_FLOAT_ACCUMULATOR_STAT( TEXT( "Audio Buffer Time" ), STAT_AudioBufferTime, STATGROUP_Audio );
DECLARE_FLOAT_ACCUMULATOR_STAT( TEXT( "Audio Buffer Time (w/ Channels)" ), STAT_AudioBufferTimeChannels, STATGROUP_Audio );

#if !CONSOLE
DECLARE_DWORD_COUNTER_STAT( TEXT( "CPU Decompressed Wave Instances" ), STAT_OggWaveInstances, STATGROUP_Audio );
DECLARE_CYCLE_STAT( TEXT( "Decompress Vorbis" ), STAT_VorbisDecompressTime, STATGROUP_Audio );
DECLARE_CYCLE_STAT( TEXT( "Prepare Audio Decompression" ), STAT_VorbisPrepareDecompressionTime, STATGROUP_Audio );
DECLARE_CYCLE_STAT( TEXT( "Decompress Audio" ), STAT_AudioDecompressTime, STATGROUP_Audio );
DECLARE_CYCLE_STAT( TEXT( "Prepare Vorbis Decompression" ), STAT_AudioPrepareDecompressionTime, STATGROUP_Audio );
#endif

DECLARE_CYCLE_STAT( TEXT( "Updating Effects" ), STAT_AudioUpdateEffects, STATGROUP_Audio );
DECLARE_CYCLE_STAT( TEXT( "Updating Sources" ), STAT_AudioUpdateSources, STATGROUP_Audio );
DECLARE_CYCLE_STAT( TEXT( "Buffer Creation" ), STAT_AudioResourceCreationTime, STATGROUP_Audio );
DECLARE_CYCLE_STAT( TEXT( "Source Init" ), STAT_AudioSourceInitTime, STATGROUP_Audio );
DECLARE_CYCLE_STAT( TEXT( "Source Create" ), STAT_AudioSourceCreateTime, STATGROUP_Audio );
DECLARE_CYCLE_STAT( TEXT( "Submit Buffers" ), STAT_AudioSubmitBuffersTime, STATGROUP_Audio );
DECLARE_CYCLE_STAT( TEXT( "Processing Sources" ), STAT_AudioStartSources, STATGROUP_Audio );
DECLARE_CYCLE_STAT( TEXT( "Gathering WaveInstances" ), STAT_AudioGatherWaveInstances, STATGROUP_Audio );
DECLARE_CYCLE_STAT( TEXT( "Audio Update Time" ), STAT_AudioUpdateTime, STATGROUP_Audio );
DECLARE_CYCLE_STAT( TEXT( "Finding Nearest Location" ), STAT_AudioFindNearestLocation, STATGROUP_Audio );
//@HACK!!!
FLOAT GGlobalAudioMultiplier = 1.0f;

/*-----------------------------------------------------------------------------
	UAudioDevice implementation.
-----------------------------------------------------------------------------*/

// Helper function for "Sort" (higher priority sorts last).
IMPLEMENT_COMPARE_POINTER( FWaveInstance, UnAudio, { return ( B->PlayPriority - A->PlayPriority >= 0 ) ? -1 : 1; } )

void UAudioDevice::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	InitSoundClasses();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

UBOOL UAudioDevice::Init( void )
{
	bGameWasTicking = TRUE;
	bSoundSpawningEnabled = TRUE;

	DebugState = DEBUGSTATE_None;

	CurrentTick = 0;
	TextToSpeech = NULL;

	// Make sure the Listeners array has at least one entry, so we don't have to check for Listeners.Num() == 0 all the time
	Listeners.AddZeroed( 1 );

	// Parses sound classes.
	InitSoundClasses();

	// Iterate over all already loaded sounds and precache them. This relies on Super::Init in derived classes to be called last.
	if( !GIsEditor )
	{
		for( TObjectIterator<USoundNodeWave> It; It; ++It )
		{
			USoundNodeWave* SoundNodeWave = *It;
			Precache( SoundNodeWave );
		}
	}

#if WITH_TTS
	// Always init TTS, even if it's only a stub
	TextToSpeech = new FTextToSpeech();
	TextToSpeech->Init();
#endif

	if( ChirpInSoundNodeWaveName.Len() > 0 )
	{
		ChirpInSoundNodeWave = LoadObject<USoundNodeWave>( NULL, *ChirpInSoundNodeWaveName, NULL, LOAD_None, NULL );
	}

	if( ChirpOutSoundNodeWaveName.Len() > 0 )
	{
		ChirpOutSoundNodeWave = LoadObject<USoundNodeWave>( NULL, *ChirpOutSoundNodeWaveName, NULL, LOAD_None, NULL );
	}

	debugf( NAME_Init, TEXT( "%s initialized." ), *GetClass()->GetName() );

	return( TRUE );
}

/**
 * Resets all interpolating values to defaults.
 */
void UAudioDevice::ResetInterpolation( void )
{
	InteriorStartTime = 0.0;
	InteriorEndTime = 0.0;
	ExteriorEndTime = 0.0;
	InteriorLPFEndTime = 0.0;
	ExteriorLPFEndTime = 0.0;

	InteriorVolumeInterp = 0.0f;
	InteriorLPFInterp = 0.0f;
	ExteriorVolumeInterp = 0.0f;
	ExteriorLPFInterp = 0.0f;

	// Reset sound class properties to defaults
	for( TMap<FName, USoundClass*>::TIterator It( SoundClasses ); It; ++It )
	{
		USoundClass* SoundClass = It.Value();
		CurrentSoundClasses.Set( It.Key(), SoundClass->Properties );
		SourceSoundClasses.Set( It.Key(), SoundClass->Properties );
		DestinationSoundClasses.Set( It.Key(), SoundClass->Properties );
	}

	BaseSoundModeName = NAME_Default;
	CurrentMode = NULL;

	// reset audio effects
	Effects->ResetInterpolation();
}

/** Enables or Disables the radio effect. */
void UAudioDevice::EnableRadioEffect( UBOOL bEnable )
{
	if( bEnable )
	{
		Exec( TEXT( "EnableRadio" ) );
	}
	else
	{
		Exec( TEXT( "DisableRadio" ) );
	}
}

/** 
 * Enables or Disables sound spawning.
 */
void UAudioDevice::SetSoundSpawningEnabled( UBOOL bEnable )
{
	bSoundSpawningEnabled = bEnable;
}

/**
 * List the WaveInstances and whether they have a source
 */
void UAudioDevice::ListWaves( FOutputDevice& Ar )
{
	TArray<FWaveInstance*> WaveInstances;
	INT FirstActiveIndex = GetSortedActiveWaveInstances( WaveInstances, false );

	for( INT InstanceIndex = FirstActiveIndex; InstanceIndex < WaveInstances.Num(); InstanceIndex++ )
	{
		FWaveInstance* WaveInstance = WaveInstances( InstanceIndex );
		FSoundSource* Source = WaveInstanceSourceMap.FindRef( WaveInstance );
		AActor* SoundOwner = WaveInstance->AudioComponent ? WaveInstance->AudioComponent->GetOwner() : NULL;
		FLOAT PlayBackTime = WaveInstance->AudioComponent ? WaveInstance->AudioComponent->PlaybackTime : 0.0f;
		Ar.Logf( TEXT( "%4i.    %s %6.2f %6.2f  %s   %s"), InstanceIndex, Source ? TEXT( "Yes" ) : TEXT( " No" ), PlayBackTime, WaveInstance->Volume, *WaveInstance->WaveData->GetPathName(), SoundOwner ? *SoundOwner->GetName() : TEXT("None") );
	}

	Ar.Logf( TEXT("Total: %i"), WaveInstances.Num()-FirstActiveIndex );
}

/**
 * Gets a summary of loaded sound collated by class
 */
void UAudioDevice::GetSoundClassInfo( TMap<FName, FAudioClassInfo>& AudioClassInfos )
{
	// Iterate over all sound cues to get a unique map of sound node waves to class names
	TMap<USoundNodeWave*, FName> SoundNodeWaveClasses;

	for( TObjectIterator<USoundCue> CueIt; CueIt; ++CueIt )
	{
		TArray<USoundNodeWave*> Waves;

		USoundCue* SoundCue = *CueIt;
		SoundCue->RecursiveFindNode<USoundNodeWave>( SoundCue->FirstNode, Waves );

		for( INT WaveIndex = 0; WaveIndex < Waves.Num(); ++WaveIndex )
		{
			// Presume one class per sound node wave
			SoundNodeWaveClasses.Set( Waves( WaveIndex ), SoundCue->SoundClass );
		}
	}

	// Add any sound node waves that are not referenced by sound cues
	for( TObjectIterator<USoundNodeWave> WaveIt; WaveIt; ++WaveIt )
	{
		USoundNodeWave* SoundNodeWave = *WaveIt;
		if( SoundNodeWaveClasses.Find( SoundNodeWave ) == NULL )
		{
			SoundNodeWaveClasses.Set( SoundNodeWave, NAME_UnGrouped );
		}
	}

	// Collate the data into something useful
	for( TMap<USoundNodeWave*, FName>::TIterator MapIter( SoundNodeWaveClasses ); MapIter; ++MapIter )
	{
		USoundNodeWave* SoundNodeWave = MapIter.Key();
		FName ClassName = MapIter.Value();

		FAudioClassInfo* AudioClassInfo = AudioClassInfos.Find( ClassName );
		if( AudioClassInfo == NULL )
		{
			FAudioClassInfo NewAudioClassInfo;

			NewAudioClassInfo.NumResident = 0;
			NewAudioClassInfo.SizeResident = 0;
			NewAudioClassInfo.NumRealTime = 0;
			NewAudioClassInfo.SizeRealTime = 0;

			AudioClassInfos.Set( ClassName, NewAudioClassInfo );

			AudioClassInfo = AudioClassInfos.Find( ClassName );
			check( AudioClassInfo );
		}

#if PS3
		AudioClassInfo->SizeResident += SoundNodeWave->CompressedPS3Data.GetBulkDataSize();;
		AudioClassInfo->NumResident++;
#elif XBOX
		AudioClassInfo->SizeResident += SoundNodeWave->CompressedXbox360Data.GetBulkDataSize();;
		AudioClassInfo->NumResident++;
#elif WIIU
		AudioClassInfo->SizeResident += SoundNodeWave->CompressedWiiUData.GetBulkDataSize();;
		AudioClassInfo->NumResident++;
#elif IPHONE
		AudioClassInfo->SizeResident += SoundNodeWave->CompressedIPhoneData.GetBulkDataSize();;
		AudioClassInfo->NumResident++;
#elif FLASH
		AudioClassInfo->SizeResident += SoundNodeWave->CompressedFlashData.GetBulkDataSize();;
		AudioClassInfo->NumResident++;
#else
		switch( SoundNodeWave->DecompressionType )
		{
		case DTYPE_Native:
		case DTYPE_Preview:
			AudioClassInfo->SizeResident += SoundNodeWave->RawPCMDataSize;
			AudioClassInfo->NumResident++;
			break;

		case DTYPE_RealTime:
			AudioClassInfo->SizeRealTime += SoundNodeWave->CompressedPCData.GetBulkDataSize();
			AudioClassInfo->NumRealTime++;
			break;

		case DTYPE_Setup:
		case DTYPE_Invalid:
		default:
			break;
		}
#endif
	}
}

/**
 * Lists a summary of loaded sound collated by class
 */
void UAudioDevice::ListSoundClasses( FOutputDevice& Ar )
{
	TMap<FName, FAudioClassInfo> AudioClassInfos;

	GetSoundClassInfo( AudioClassInfos );

	Ar.Logf( TEXT( "Listing all sound classes." ) );

	// Display the collated data
	INT TotalSounds = 0;
	for( TMap<FName, FAudioClassInfo>::TIterator ACIIter( AudioClassInfos ); ACIIter; ++ACIIter )
	{
		FName ClassName = ACIIter.Key();
		FAudioClassInfo* ACI = AudioClassInfos.Find( ClassName );

		FString Line = FString::Printf( TEXT( "Class '%s' has %d resident sounds taking %.2f kb" ), *ClassName.GetNameString(), ACI->NumResident, ACI->SizeResident / 1024.0f );
		TotalSounds += ACI->NumResident;
		if( ACI->NumRealTime > 0 )
		{
			Line += FString::Printf( TEXT( ", and %d real time sounds taking %.2f kb " ), ACI->NumRealTime, ACI->SizeRealTime / 1024.0f );
			TotalSounds += ACI->NumRealTime;
		}

		Ar.Logf( *Line );
	}

	Ar.Logf( TEXT( "%d total sounds in %d classes" ), TotalSounds, AudioClassInfos.Num() );
}

/**
 * Prints the subtitle associated with the SoundNodeWave to the console
 */
void USoundNodeWave::LogSubtitle( FOutputDevice& Ar )
{
	FString Subtitle = "";
	for( INT i = 0; i < Subtitles.Num(); i++ )
	{
		Subtitle += Subtitles( i ).Text;
	}

	if( Subtitle.Len() == 0 )
	{
		Subtitle = SpokenText;
	}

	if( Subtitle.Len() == 0 )
	{
		Subtitle = "<NO SUBTITLE>";
	}

	Ar.Logf( TEXT( "Subtitle:  %s" ), *Subtitle );
#if WITH_EDITORONLY_DATA
	Ar.Logf( TEXT( "Comment:   %s" ), *Comment );
#endif // WITH_EDITORONLY_DATA
	Ar.Logf( bMature ? TEXT( "Mature:    Yes" ) : TEXT( "Mature:    No" ) );
}

/**
 * Gets the current audio debug state
 */
EDebugState UAudioDevice::GetMixDebugState( void )
{
	return( ( EDebugState )DebugState );
}

IMPLEMENT_COMPARE_CONSTREF( INT, SimpleIntCompare, { return( B - A ) > 0 ? 1 : -1; } )

void ListSoundTemplateInfo( FOutputDevice& Ar )
{
	INT NumCues = 0;
	TMap<FString, INT> UniqueCues;

	for( TObjectIterator<USoundCue> It; It; ++It )
	{
		TArray<USoundNode*> SoundNodes;

		USoundCue* Cue = *It;
		if( Cue )
		{
			if( Cue->FirstNode )
			{
				Cue->FirstNode->GetAllNodes( SoundNodes );

				FString Unique = TEXT( "" );
				for( INT SoundNodeIndex = 0; SoundNodeIndex < SoundNodes.Num(); SoundNodeIndex++ )
				{
					USoundNode* SoundNode = SoundNodes( SoundNodeIndex );
					Unique += SoundNode->GetUniqueString();
				}

				if( !appStristr( *Unique, TEXT( "Complex" ) ) )
				{
					INT Count = 1;
					if( UniqueCues.Find( Unique ) )
					{
						Count = UniqueCues.FindRef( Unique ) + 1;
					}
					UniqueCues.Set( Unique, Count );
				}

				Ar.Logf( TEXT( "Cue: %s : %s" ), *Cue->GetFullName(), *Unique );
				NumCues++;
			}
			else
			{
				Ar.Logf( TEXT( "No FirstNode : %s" ), *Cue->GetFullName() );
			}
		}
	}

	Ar.Logf( TEXT( "Potential Templates -" ) );

	UniqueCues.ValueSort<COMPARE_CONSTREF_CLASS( INT, SimpleIntCompare )>();

	for( TMap<FString, INT>::TIterator It( UniqueCues ); It; ++It )
	{
		FString Template = It.Key();
		INT TemplateCount = It.Value();
		Ar.Logf( TEXT( "%05d : %s" ), TemplateCount, *Template );
	}

	Ar.Logf( TEXT( "SoundCues processed: %d" ), NumCues );
	Ar.Logf( TEXT( "Unique SoundCues   : %d" ), UniqueCues.Num() );
}

struct FSoundCueData
{
	USoundCue* SoundCue;
	INT NumWaves;
	INT NumHighDetailWaves;
	INT NumRandom;
	INT NumMixers;
	INT MemorySize;
	INT HighDetailMemorySize;
	INT PotentialMemorySavings;
};

/**
 * Exec handler used to parse console commands.
 *
 * @param	Cmd		Command to parse
 * @param	Ar		Output device to use in case the handler prints anything
 * @return	TRUE if command was handled, FALSE otherwise
 */
UBOOL UAudioDevice::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( ParseCommand( &Cmd, TEXT( "ListSounds" ) ) )
	{
		ListSounds( Cmd, Ar );
		return( TRUE );
	}
	else if( ParseCommand( &Cmd, TEXT( "ListWaves" ) ) )
	{
		ListWaves( Ar );
		return( TRUE );
	}
	else if( ParseCommand( &Cmd, TEXT( "ListSoundClasses" ) ) )
	{
		ListSoundClasses( Ar );
		return( TRUE );
	}
	else if( ParseCommand( &Cmd, TEXT( "ListSoundClassVolumes" ) ) )
	{
		Ar.Logf(TEXT("SoundClass Volumes: (Volume, Pitch)"));

		for( TMap<FName, USoundClass*>::TIterator It( SoundClasses ); It; ++It )
		{
			FName ClassName = It.Key();
			FSoundClassProperties* const DestClass = DestinationSoundClasses.Find( ClassName );
			FSoundClassProperties* const SrcClass = SourceSoundClasses.Find( ClassName );
			FSoundClassProperties* const CurClass = CurrentSoundClasses.Find( ClassName );
			if( DestClass && SrcClass && CurClass )
			{
				FString Line = FString::Printf( TEXT("Src (%3.2f, %3.2f), Dest (%3.2f, %3.2f), Cur (%3.2f, %3.2f) for SoundClass %s"), SrcClass->Volume, SrcClass->Pitch, DestClass->Volume, DestClass->Pitch, CurClass->Volume, CurClass->Pitch, *ClassName.GetNameString() );
				Ar.Logf( *Line );
			}
		}
	}
	else if( ParseCommand( &Cmd, TEXT( "ListSoundModes" ) ) )
	{
		// Display a list of the sound modes
		INT TotalSounds = 0;
		for( TMap<FName, USoundMode*>::TIterator ModeIter( SoundModes ); ModeIter; ++ModeIter )
		{
			FName ModeName = ModeIter.Key();
			USoundMode* Mode = ModeIter.Value();

			FString Line = FString::Printf( TEXT( "'%s' (%s)" ), *ModeName.ToString(), Mode ? *Mode->GetFullName() : TEXT("None"));
			Ar.Logf( *Line );
		}

		return( TRUE );
	}
	else if( ParseCommand( &Cmd, TEXT( "ListAudioComponents" ) ) )
	{
		INT Count = 0;
		Ar.Logf( TEXT( "AudioComponent Dump" ) );
		for( TObjectIterator<UAudioComponent> It; It; ++It )
		{
			UAudioComponent* AudioComponent = *It;
			UObject* Outer = It->GetOuter();
			UObject* Owner = It->GetOwner();
			Ar.Logf( TEXT("    0x%p: %s, %s, %s, %s"),
				AudioComponent,
				*( It->GetPathName() ),
				It->SoundCue ? *( It->SoundCue->GetPathName() ) : TEXT( "NO SOUND CUE" ),
				Outer ? *( Outer->GetPathName() ) : TEXT( "NO OUTER" ),
				Owner ? *( Owner->GetPathName() ) : TEXT( "NO OWNER" ) );
			Ar.Logf( TEXT( "        bAutoDestroy....................%s" ), AudioComponent->bAutoDestroy ? TEXT( "TRUE" ) : TEXT( "FALSE" ) );
			Ar.Logf( TEXT( "        bStopWhenOwnerDestroyed.........%s" ), AudioComponent->bStopWhenOwnerDestroyed ? TEXT( "TRUE" ) : TEXT( "FALSE" ) );
			Ar.Logf( TEXT( "        bShouldRemainActiveIfDropped....%s" ), AudioComponent->bShouldRemainActiveIfDropped ? TEXT( "TRUE" ) : TEXT( "FALSE" ) );
			Ar.Logf( TEXT( "        bIgnoreForFlushing..............%s" ), AudioComponent->bIgnoreForFlushing ? TEXT( "TRUE" ) : TEXT( "FALSE" ) );
			Count++;
		}
		Ar.Logf( TEXT( "AudioComponent Total = %d" ), Count );

		for( TObjectIterator<UAudioDevice> AudioDevIt; AudioDevIt; ++AudioDevIt )
		{
			UAudioDevice* AudioDevice = *AudioDevIt;

			Ar.Logf( TEXT( "AudioDevice 0x%p has %d AudioComponents (%s)" ),
				AudioDevice, AudioDevice->AudioComponents.Num(),
				*( AudioDevice->GetPathName() ) );
			for( INT ACIndex = 0; ACIndex < AudioDevice->AudioComponents.Num(); ACIndex++ )
			{
				UAudioComponent* AComp = AudioDevice->AudioComponents( ACIndex );
				if( AComp )
				{
					UObject* Outer = AComp->GetOuter();
					Ar.Logf( TEXT( "    0x%p: %4d - %s, %s, %s, %s" ),
						AComp,
						ACIndex,
						*( AComp->GetPathName() ),
						AComp->SoundCue ? *( AComp->SoundCue->GetPathName() ) : TEXT( "NO SOUND CUE" ),
						Outer ? *( Outer->GetPathName() ) : TEXT( "NO OUTER" ),
						AComp->GetOwner() ? *( AComp->GetOwner()->GetPathName() ) : TEXT( "NO OWNER" ) );
				}
				else
				{
					Ar.Logf( TEXT( "    %4d - %s" ), ACIndex, TEXT( "NULL COMPONENT IN THE ARRAY" ) );
				}
			}
		}
		return TRUE;
	}
	else if( ParseCommand( &Cmd, TEXT( "ListSoundDurations" ) ) )
	{
		debugf( TEXT( ",Sound,Duration,Channels" ) );
		for( TObjectIterator<USoundNodeWave> It; It; ++It )
		{
			USoundNodeWave* SoundNodeWave = *It;
			debugf( TEXT( ",%s,%f,%i" ), *SoundNodeWave->GetPathName(), SoundNodeWave->Duration, SoundNodeWave->NumChannels );
		}
		return( TRUE );
	}
	else if( ParseCommand( &Cmd, TEXT( "ListSoundVariety" ) ) )
	{
		UBOOL bAutoCutVariety = FALSE;
		if( ParseCommand( &Cmd, TEXT("AutoCut") ) )
		{
			bAutoCutVariety = TRUE;
		}

		UBOOL bShowDetails = FALSE;
		if( ParseCommand( &Cmd, TEXT("Details") ) )
		{
			bShowDetails = TRUE;
		}

		INT Count = 0;
		INT KeepCount = 0;
		INT CutCount = 0;
		INT HighDetailMemorySize = 0;
		INT AlwaysPlayMemorySize = 0;
		INT PotentialCutMemorySize = 0;
		TArray<FString> HighDetailList, AlwaysPlayList, CompletelyCutList;
		TArray<FSoundCueData> Cues;

		for( TObjectIterator<USoundCue> It; It; ++It )
		{
			USoundCue* SoundCue = *It;
			Count++;

			Cues.AddZeroed();
			FSoundCueData* CueData = &Cues(Cues.Num()-1);
			CueData->SoundCue = SoundCue;

			debugf( TEXT("  %3i: %s" ), Count, *SoundCue->GetPathName() );

			INT LocalKeepCount = 0, LocalCount = 0;

			TArray<USoundNode*> Nodes;
			SoundCue->RecursiveFindNode<USoundNode>( SoundCue->FirstNode, Nodes );
			for( INT i=0; i<Nodes.Num(); i++ )
			{
				if (Nodes(i) != NULL && Nodes(i)->IsA(USoundNodeMixer::StaticClass()) || Nodes(i)->IsA(USoundNodeConcatenator::StaticClass()))
				{
					CueData->NumMixers++;
				}

				if (Nodes(i) != NULL && Nodes(i)->IsA(USoundNodeRandom::StaticClass()))
				{
					CueData->NumRandom++;
				}

				USoundNodeWave* Wave = Cast<USoundNodeWave>(Nodes(i));
				if (Wave != NULL)
				{
					LocalCount++;

					INT WaveSize = Wave->CompressedIPhoneData.GetBulkDataSize();
					CueData->NumWaves++;
					CueData->MemorySize += WaveSize;

					debugf( TEXT("      %2i: %s %iKB %s" ), LocalCount, *Wave->GetName(), WaveSize / 1024, Wave->MobileDetailMode == DM_High ? TEXT("Cut") : TEXT("") );
					if( Wave->MobileDetailMode == DM_High )
					{
						CueData->NumHighDetailWaves++;
						CutCount++;
						if( !HighDetailList.ContainsItem(Wave->GetPathName()) )
						{
							HighDetailList.AddUniqueItem(Wave->GetPathName());
							HighDetailMemorySize += WaveSize;
							CueData->HighDetailMemorySize += WaveSize;
						}
					}
					else
					{
						LocalKeepCount++;
						KeepCount++;
						if( !AlwaysPlayList.ContainsItem(Wave->GetPathName()) )
						{
							AlwaysPlayList.AddUniqueItem(Wave->GetPathName());
							AlwaysPlayMemorySize += WaveSize;
							// In theory, we can get down to one wav per cue, so count up potential savings
							if( LocalKeepCount > 1 )
							{
								PotentialCutMemorySize += WaveSize;
								CueData->PotentialMemorySavings += WaveSize;
							}
						}
					}
				}
			}

			// Has a whole sound cue been cut out?
			if( CueData->NumWaves == 0 )
			{
				debugf( TEXT("     This cue has no wavs." ));
			}
			else if( CueData->NumHighDetailWaves == CueData->NumWaves )
			{
				debugf( TEXT("     All %i wavs have been cut from this sound. THIS SOUND WILL NOT PLAY!" ), CueData->NumWaves);
				CompletelyCutList.AddUniqueItem(SoundCue->GetPathName());
			}
		}

		if( bShowDetails )
		{
			debugf( TEXT("\nSoundCue Data:" ));
			debugf( TEXT("      WavesLeft, Waves, HighDetailWaves, RandomNodes, MixerNodes, MemoryUsed, MemorySaved, PotentialSavings, AlwaysLoaded, SoundCue" ));
			for( INT i=0; i<Cues.Num(); i++ )
			{
				UBOOL bAlwaysLoaded = Cues(i).SoundCue->HasAnyFlags( RF_RootSet );
				debugf(TEXT("      %3i: %i %i %i %i %i %i %i %i %s %s"), i, Cues(i).NumWaves - Cues(i).NumHighDetailWaves, Cues(i).NumWaves, Cues(i).NumHighDetailWaves, Cues(i).NumRandom, Cues(i).NumMixers, Cues(i).MemorySize - Cues(i).HighDetailMemorySize, Cues(i).HighDetailMemorySize, Cues(i).PotentialMemorySavings, bAlwaysLoaded ? TEXT("Y") : TEXT("N"), *Cues(i).SoundCue->GetPathName());
			}

			debugf( TEXT("\nSoundCues that have been completely cut (no wavs to play):" ));
			for( INT i=0; i<CompletelyCutList.Num(); i++ )
			{
				debugf(TEXT("      %3i: %s"), i, *CompletelyCutList(i));
			}

			//debugf( TEXT("\nSoundCues that have random nodes with only one wav:" ));
			//for( INT i=0; i<HasRandomOnlyOne.Num(); i++ )
			//{
			//	debugf(TEXT("      %3i: %s"), i, HasRandom(i)->GetPathName());
			//}

			//debugf( TEXT("\nSoundCues that have random nodes with more than one wav:" ));
			//for( INT i=0; i<HasRandomVariety.Num(); i++ )
			//{
			//	debugf(TEXT("      %3i: %s"), i, HasRandom(i)->GetPathName());
			//}

			//debugf( TEXT("\nSoundCues that have mixer nodes:" ));
			//for( INT i=0; i<HasMixer.Num(); i++ )
			//{
			//	debugf(TEXT("      %3i: %s"), i, HasMixer(i)->GetPathName());
			//}
		}

		debugf( TEXT("\n\nList Sound Variety Results:" ));
		// Subtract the SoundCue count, as we usually want one sound wav per cue. The CompletelyCut count shows how many have no sound wavs at all.
		debugf( TEXT("  %i/%i (%.1f%%) sound node variety has been cut" ), CutCount, CutCount+KeepCount-Count, (FLOAT)CutCount * 100.f / (CutCount+KeepCount-Count) );
		debugf( TEXT("  %i sound nodes have been completely cut" ), CompletelyCutList.Num() );
		debugf( TEXT("  %i/%i (%.1f%%) sound waves have been cut, for a savings of:" ), HighDetailList.Num(), HighDetailList.Num()+AlwaysPlayList.Num(), (FLOAT)HighDetailList.Num() * 100.f / (HighDetailList.Num()+AlwaysPlayList.Num()) );
		debugf( TEXT("      %i/%i KB (%.1f%%)" ), HighDetailMemorySize/1024, (HighDetailMemorySize+AlwaysPlayMemorySize)/1024, ((FLOAT)HighDetailMemorySize * 100.f / (HighDetailMemorySize+AlwaysPlayMemorySize)) );
		debugf( TEXT("  Cutting all variety left could save %i KB" ), PotentialCutMemorySize / 1024 );

		return( TRUE );
	}
	else if( ParseCommand( &Cmd, TEXT( "SoundTemplateInfo" ) ) )
	{
		ListSoundTemplateInfo( Ar );
	}
	else if( ParseCommand( &Cmd, TEXT( "PlaySoundCue" ) ) )
	{
		// Stop any existing sound playing
		if( TestAudioComponent == NULL )
		{
			TestAudioComponent = ConstructObject<UAudioComponent>( UAudioComponent::StaticClass() );
		}

		if( TestAudioComponent != NULL )
		{
			TestAudioComponent->Stop();

			// Load up an arbitrary cue
			USoundCue* Cue = LoadObject<USoundCue>( NULL, Cmd, NULL, LOAD_None, NULL );
			if( Cue != NULL )
			{
				TestAudioComponent->SoundCue = Cue;
				TestAudioComponent->bAllowSpatialization = FALSE;
				TestAudioComponent->bAutoDestroy = TRUE;
				TestAudioComponent->Play();

				TArray<USoundNodeWave*> Waves;
				Cue->RecursiveFindNode<USoundNodeWave>( Cue->FirstNode, Waves );
				for( INT i = 0; i < Waves.Num(); i++ )
				{
					Waves( i )->LogSubtitle( Ar );
				}
			}
		}
	}
	else if( ParseCommand( &Cmd, TEXT( "PlaySoundWave" ) ) )
	{
		// Stop any existing sound playing
		if( TestAudioComponent == NULL )
		{
			TestAudioComponent = ConstructObject<UAudioComponent>( UAudioComponent::StaticClass() );
		}

		if( TestAudioComponent != NULL )
		{
			TestAudioComponent->Stop();

			// Load up an arbitrary wave
			USoundNodeWave* Wave = LoadObject<USoundNodeWave>( NULL, Cmd, NULL, LOAD_None, NULL );
			USoundCue* Cue = ConstructObject<USoundCue>( USoundCue::StaticClass() );
			if( Cue != NULL && Wave != NULL )
			{
				Cue->FirstNode = Wave;
				TestAudioComponent->SoundCue = Cue;
				TestAudioComponent->bAllowSpatialization = FALSE;
				TestAudioComponent->bAutoDestroy = TRUE;
				TestAudioComponent->Play();

				Wave->LogSubtitle( Ar );
			}
		}
	}
	else if( ParseCommand( &Cmd, TEXT( "SetSoundMode" ) ) )
	{
		// Ar.Logf( TEXT( "Setting sound mode '%s'" ), Cmd );
		FName NewMode = FName( Cmd );
		SetSoundMode( NewMode );
		return( TRUE );
	}
	else if( ParseCommand( &Cmd, TEXT( "IsolateDryAudio" ) ) )
	{
		Ar.Logf( TEXT( "Dry audio isolated" ) );
		DebugState = DEBUGSTATE_IsolateDryAudio;
		return( TRUE );
	}
	else if( ParseCommand( &Cmd, TEXT( "IsolateReverb" ) ) )
	{
		Ar.Logf( TEXT( "Reverb audio isolated" ) );
		DebugState = DEBUGSTATE_IsolateReverb;
		return( TRUE );
	}
	else if( ParseCommand( &Cmd, TEXT( "TestLPF" ) ) )
	{
		Ar.Logf( TEXT( "LPF set to max for all sources" ) );
		DebugState =  DEBUGSTATE_TestLPF;
		return( TRUE );
	}
	else if( ParseCommand( &Cmd, TEXT( "TestStereoBleed" ) ) )
	{
		Ar.Logf( TEXT( "StereoBleed set to max for all sources" ) );
		DebugState =  DEBUGSTATE_TestStereoBleed;
		return( TRUE );
	}
	else if( ParseCommand( &Cmd, TEXT( "TestLFEBleed" ) ) )
	{
		Ar.Logf( TEXT( "LFEBleed set to max for all sources" ) );
		DebugState = DEBUGSTATE_TestLFEBleed;
		return( TRUE );
	}
	else if( ParseCommand( &Cmd, TEXT( "DisableLPF" ) ) )
	{
		Ar.Logf( TEXT( "LPF disabled for all sources" ) );
		DebugState = DEBUGSTATE_DisableLPF;
		return( TRUE );
	}
	else if( ParseCommand( &Cmd, TEXT( "DisableRadio" ) ) )
	{
		Ar.Logf( TEXT( "Radio disabled for all sources" ) );
		DebugState = DEBUGSTATE_DisableRadio;
		return( TRUE );
	}
	else if( ParseCommand( &Cmd, TEXT( "EnableRadio" ) ) )
	{
		DebugState = DEBUGSTATE_None;
		return ( TRUE );
	}

	else if( ParseCommand( &Cmd, TEXT( "ResetSoundState" ) ) )
	{
		Ar.Logf( TEXT( "All volumes reset to their defaults; all test filters removed" ) );
		DebugState = DEBUGSTATE_None;
		return( TRUE );
	}
	// usage ModifySoundClass <soundclassname> vol=<new volume>
	else if( ParseCommand( &Cmd, TEXT( "ModifySoundClass" ) ) )
	{
		const FString SoundClassName = ParseToken( Cmd, 0 );
		FLOAT NewVolume = -1.0f;
		Parse( Cmd, TEXT( "Vol=" ), NewVolume );

		warnf( TEXT( "ModifySoundClass Class: %s NewVolume: %f" ), *SoundClassName, NewVolume );
		SetClassVolume( *SoundClassName, NewVolume );
	}

	return( FALSE );
}

/**
 * Add any referenced objects that shouldn't be garbage collected
 */
void UAudioDevice::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects( ObjectArray );
}

/**
 * Add a newly created sound class
 */
void UAudioDevice::AddClass( USoundClass* SoundClass )
{
	// Rebuild the entire sound classes map from scratch
	InitSoundClasses();
}

/**
 * Remove references to the sound class
 */
void UAudioDevice::RemoveClass( USoundClass* SoundClass )
{
	if( SoundClass )
	{
#if WITH_EDITORONLY_DATA
		USoundClass* MasterClass = SoundClasses.FindRef( NAME_Master );
		MasterClass->EditorData.Remove( SoundClass );
#endif // WITH_EDITORONLY_DATA

		SoundClasses.Remove( SoundClass->GetFName() );
		SoundClass->RemoveFromRoot();
		SoundClass->ClearFlags( RF_Standalone );
		SoundClass->MarkPendingKill();
		// InitSoundClasses called from NotifyPostDeleteObject
	}
}

/**
 * Set up the sound class hierarchy
 */
void UAudioDevice::InitSoundClasses( void )
{
	TArray<FName> SoundClassFNames;
	UEnum* SoundClassNamesEnum = FindObject<UEnum>( NULL, TEXT( "Engine.AudioDevice.ESoundClassName" ) );
	check( SoundClassNamesEnum );

	SoundClasses.Empty();

	// Reset the map of available sound classes and the associated enum
	for( TObjectIterator<USoundClass> It; It; ++It )
	{
		USoundClass* SoundClass = *It;
		if( SoundClass && !SoundClass->IsPendingKill() )
		{
			SoundClasses.Set( SoundClass->GetFName(), SoundClass );
			SoundClassFNames.AddUniqueItem( SoundClass->GetFName() );
		}
	}

	SoundClassNamesEnum->SetEnums( SoundClassFNames );

	// Reset the maps of sound class properties
	for( TMap<FName, USoundClass*>::TIterator It( SoundClasses ); It; ++It )
	{
		USoundClass* SoundClass = It.Value();
		CurrentSoundClasses.Set( It.Key(), SoundClass->Properties );
		SourceSoundClasses.Set( It.Key(), SoundClass->Properties );
		DestinationSoundClasses.Set( It.Key(), SoundClass->Properties );
	}

	// Propagate the properties down the hierarchy
	ParseSoundClasses();

	// Refresh the sound modes
	InitSoundModes();

}

/**
 * Add a newly created sound mode
 */
void UAudioDevice::AddMode( USoundMode* SoundMode )
{
	// Sound modes map rebuilt from scratch
	InitSoundModes();
}

/**
 * Remove references to the sound mode
 */
void UAudioDevice::RemoveMode( USoundMode* SoundMode )
{
	if( SoundMode )
	{
		SoundModes.Remove( SoundMode->GetFName() );
		SoundMode->RemoveFromRoot();
		// InitSoundModes called from NotifyPostDeleteObject
	}
}

/**
 * Called after duplication & serialization and before PostLoad.
 */
void USoundMode::PostDuplicate( void )
{
	Super::PostDuplicate();
	Fixup();
}

/**
 * Init anything required by the sound modes
 */
void UAudioDevice::InitSoundModes( void )
{
	SoundModes.Empty();

	// Cache all the sound modes locally for quick access
	for( TObjectIterator<USoundMode> It; It; ++It )
	{
		USoundMode* Mode = *It;
		if( Mode )
		{
			SoundModes.Set( Mode->GetFName(), Mode );
			Mode->Fixup();
		}
	}

	BaseSoundModeName = NAME_Default;
}

/**
 * Internal helper function used by ParseSoundClasses to traverse the tree.
 *
 * @param CurrentClass			Subtree to deal with
 * @param ParentProperties		Propagated properties of parent node
 */
void UAudioDevice::RecurseIntoSoundClasses( USoundClass* CurrentClass, FSoundClassProperties* ParentProperties )
{
	// Iterate over all child nodes and recurse.
	for( INT ChildIndex = 0; ChildIndex < CurrentClass->ChildClassNames.Num(); ChildIndex++ )
	{
		// Look up class and propagated properties.
		FName ChildClassName = CurrentClass->ChildClassNames( ChildIndex );
		USoundClass* ChildClass = SoundClasses.FindRef( ChildClassName );
		FSoundClassProperties* ChildClassProperties = DestinationSoundClasses.Find( ChildClassName );

		// Should never be NULL for a properly set up tree.
		if( ChildClass && ChildClassProperties )
		{
			// Propagate parent values...
			ChildClass->bIsChild = TRUE;

			ChildClassProperties->Volume *= ParentProperties->Volume;
			ChildClassProperties->Pitch *= ParentProperties->Pitch;
			ChildClassProperties->bIsUISound |= ParentProperties->bIsUISound;
			ChildClassProperties->bIsMusic |= ParentProperties->bIsMusic;

			// Not all values propagate equally...
			// VoiceCenterChannelVolume, RadioFilterVolume, RadioFilterVolumeThreshold, bApplyEffects, BleedStereo, bReverb, and bCenterChannelOnly do not propagate (sub-classes can be non-zero even if parent class is zero)

			// ... and recurse into child nodes.
			RecurseIntoSoundClasses( ChildClass, ChildClassProperties );
		}
		else
		{
			warnf( TEXT( "Couldn't find child class %s - sound class functionality will not work correctly!" ), *ChildClassName.ToString() );
		}
	}
}

/**
 * Parses the sound classes and propagates multiplicative properties down the tree.
 */
void UAudioDevice::ParseSoundClasses( void )
{
	// Reset to known state - preadjusted by set class volume calls
	for( TMap<FName, USoundClass*>::TIterator It( SoundClasses ); It; ++It )
	{
		USoundClass* SoundClass = It.Value();
		DestinationSoundClasses.Set( It.Key(), SoundClass->Properties );
	}

	// We are bound to have a Master volume.
	USoundClass* MasterClass = SoundClasses.FindRef( NAME_Master );
	FSoundClassProperties* MasterClassProperties = DestinationSoundClasses.Find( NAME_Master );
	if( MasterClass && MasterClassProperties )
	{
		// Follow the tree.
		RecurseIntoSoundClasses( MasterClass, MasterClassProperties );
	}
	else
	{
		warnf( TEXT( "Couldn't find Master sound class! This class is required for the audio sound mode system to function!" ) );
	}
}

/**
 * Recursively apply an adjuster to the passed in sound class and all children of the sound class
 *
 * @param InAdjuster		The adjuster to apply
 * @param InSoundClassName	The name of the sound class to apply the adjuster to.  Also applies to all children of this class
 */
void UAudioDevice::RecursiveApplyAdjuster( const FSoundClassAdjuster& InAdjuster, const FName& InSoundClassName )
{
	// Find the sound class properties so we can apply the adjuster
	// and find the sound class so we can recurse through the children
	FSoundClassProperties* ClassToAdjust = DestinationSoundClasses.Find( InSoundClassName );
	USoundClass* SoundClass = SoundClasses.FindRef( InSoundClassName );
	if( SoundClass && ClassToAdjust )
	{
		// Adjust this class
		ClassToAdjust->Volume *= InAdjuster.VolumeAdjuster;
		ClassToAdjust->Pitch *= InAdjuster.PitchAdjuster;
		ClassToAdjust->VoiceCenterChannelVolume *= InAdjuster.VoiceCenterChannelVolumeAdjuster;

		// Recurse through this classes children
		for( INT ChildIdx = 0; ChildIdx < SoundClass->ChildClassNames.Num(); ++ChildIdx )
		{
			RecursiveApplyAdjuster( InAdjuster, SoundClass->ChildClassNames( ChildIdx ) );
		}
	}
	else
	{
		warnf( TEXT( "Sound class '%s' does not exist" ), *InAdjuster.SoundClass.ToString() );
	}
}

/**
 * Handle mode setting with respect to sound class volumes
 */
UBOOL UAudioDevice::ApplySoundMode( USoundMode* NewMode )
{
	// Set a new mode if it changes
	if( NewMode != CurrentMode )
	{
		debugf( NAME_DevAudio, TEXT( "UAudioDevice::ApplySoundMode(): %s" ), *NewMode->GetName() );

		// Copy the current sound class state
		SourceSoundClasses = CurrentSoundClasses;

		SoundModeStartTime = GCurrentTime;
		if( NewMode->GetFName() == BaseSoundModeName )
		{
			// If resetting to base mode, use the original fadeout time (without delay)
			SoundModeFadeInStartTime = SoundModeStartTime;
			SoundModeFadeInEndTime = SoundModeFadeInStartTime;
			SoundModeEndTime = SoundModeFadeInEndTime;
			if( CurrentMode )
			{
				SoundModeFadeInEndTime += CurrentMode->FadeOutTime;
				SoundModeEndTime += CurrentMode->FadeOutTime;
			}
		}
		else
		{
			// If going to non base mode, set up the volume envelope
			SoundModeFadeInStartTime = SoundModeStartTime + NewMode->InitialDelay;
			SoundModeFadeInEndTime = SoundModeFadeInStartTime + NewMode->FadeInTime;
			SoundModeEndTime = -1.0;
			if( NewMode->Duration >= 0.0f )
			{
				SoundModeEndTime = SoundModeFadeInEndTime + NewMode->Duration;
			}
		}

		CurrentMode = NewMode;
		if( CurrentMode->Duration < 0.0f )
		{
			// Current also becomes the new base sound mode
			BaseSoundModeName = NewMode->GetFName();
		}

		ParseSoundClasses();
		ApplyClassAdjusters();

		return( TRUE );
	}

	return( FALSE );
}

/**
 * Sets the sound class adjusters from the current sound mode.
 */
void UAudioDevice::ApplyClassAdjusters( void )
{
	if( CurrentMode )
	{
		// Adjust the sound class properties non recursively
		TArray<FSoundClassAdjuster>& Adjusters = CurrentMode->SoundClassEffects;

		for( INT i = 0; i < Adjusters.Num(); i++ )
		{
			if( Adjusters( i ).bApplyToChildren )
			{
				// Apply the adjuster the sound class specified by the adjuster and all its children
				RecursiveApplyAdjuster( Adjusters( i ), Adjusters( i ).SoundClass );
			}
			else
			{
				// Apply the adjuster to only the sound class specified by the adjuster
				FSoundClassProperties* ClassToAdjust = DestinationSoundClasses.Find( Adjusters( i ).SoundClass );
				if( ClassToAdjust )
				{
					ClassToAdjust->Volume *= Adjusters( i ).VolumeAdjuster;
					ClassToAdjust->Pitch *= Adjusters( i ).PitchAdjuster;
					ClassToAdjust->VoiceCenterChannelVolume *= Adjusters( i ).VoiceCenterChannelVolumeAdjuster;
				}
				else
				{
					warnf( TEXT( "Sound class '%s' does not exist" ), *Adjusters( i ).SoundClass.ToString() );
				}
			}
		}
	}
}

/**
 * Gets the parameters for the sound mode
 */
void USoundClass::Interpolate( FLOAT InterpValue, FSoundClassProperties* Current, FSoundClassProperties* Start, FSoundClassProperties* End )
{
	if( InterpValue >= 1.0f )
	{
		*Current = *End;
	}
	else if( InterpValue <= 0.0f )
	{
		*Current = *Start;
	}
	else
	{
		FLOAT InvInterpValue = 1.0f - InterpValue;

		Current->Volume = ( Start->Volume * InvInterpValue ) + ( End->Volume * InterpValue );
		Current->Pitch = ( Start->Pitch * InvInterpValue ) + ( End->Pitch * InterpValue );
		Current->VoiceCenterChannelVolume = ( Start->VoiceCenterChannelVolume * InvInterpValue ) + ( End->VoiceCenterChannelVolume * InterpValue );
		Current->RadioFilterVolume = ( Start->RadioFilterVolume * InvInterpValue ) + ( End->RadioFilterVolume * InterpValue );
		Current->RadioFilterVolumeThreshold = ( Start->RadioFilterVolumeThreshold * InvInterpValue ) + ( End->RadioFilterVolumeThreshold * InterpValue );
	}
}

/**
 * Construct the CurrentSoundClassProperties map
 *
 * This contains the original sound class properties propagated properly, and all adjustments due to the sound mode
 */
void UAudioDevice::GetCurrentSoundClassState( void )
{
	FLOAT InterpValue = 1.0f;

	// Initial delay before mode is applied
	if( GCurrentTime >= SoundModeStartTime && GCurrentTime < SoundModeFadeInStartTime )
	{
		InterpValue = 0.0f;
	}
	else if( GCurrentTime >= SoundModeFadeInStartTime && GCurrentTime < SoundModeFadeInEndTime && ( SoundModeFadeInEndTime - SoundModeFadeInStartTime ) > 0.0 )
	{
		// Work out the fade in portion
		InterpValue = ( FLOAT )( ( GCurrentTime - SoundModeFadeInStartTime ) / ( SoundModeFadeInEndTime - SoundModeFadeInStartTime ) );
	}
	else if( GCurrentTime >= SoundModeFadeInEndTime && GCurrentTime < SoundModeEndTime )
	{
		// .. ensure the full mode is applied between the end of the fade in time and the start of the fade out time
		InterpValue = 1.0f;
	}
	else if( SoundModeEndTime >= 0.0 && GCurrentTime >= SoundModeEndTime )
	{
		// .. trigger the fade out
		if( SetSoundMode( BaseSoundModeName ) )
		{
			return;
		}
	}

	// *Always* need to call this to apply any changes to the original sound classes (such as from the UI)
	for( TMap<FName, USoundClass*>::TIterator It( SoundClasses ); It; ++It )
	{
		FName SoundClassName = It.Value()->GetFName();

		USoundClass* CurrentClass = SoundClasses.FindRef( SoundClassName );
		FSoundClassProperties* Current = CurrentSoundClasses.Find( SoundClassName );
		FSoundClassProperties* Start = SourceSoundClasses.Find( SoundClassName );
		FSoundClassProperties* End = DestinationSoundClasses.Find( SoundClassName );

		if( Current && Start && End )
		{
			CurrentClass->Interpolate( InterpValue, Current, Start, End );
		}
	}
}

/**
 * Works out the interp value between source and end
 */
FLOAT UAudioDevice::Interpolate( DOUBLE EndTime )
{
	if( GCurrentTime < InteriorStartTime )
	{
		return( 0.0f );
	}

	if( GCurrentTime >= EndTime )
	{
		return( 1.0f );
	}

	FLOAT InterpValue = ( FLOAT )( ( GCurrentTime - InteriorStartTime ) / ( EndTime - InteriorStartTime ) );
	return( InterpValue );
}

/**
 * Gets the current state of the interior settings for the listener
 */
void UAudioDevice::GetCurrentInteriorSettings( void )
{
	// Store the interpolation value, not the actual value
	InteriorVolumeInterp = Interpolate( InteriorEndTime );
	ExteriorVolumeInterp = Interpolate( ExteriorEndTime );
	InteriorLPFInterp = Interpolate( InteriorLPFEndTime );
	ExteriorLPFInterp = Interpolate( ExteriorLPFEndTime );
}

/**
 * Apply the interior settings to ambient sounds
 */
void UAudioDevice::ApplyInteriorSettings( INT VolumeIndex, const FInteriorSettings& Settings )
{
	if( VolumeIndex != ListenerVolumeIndex )
	{
		debugf( NAME_DevAudio, TEXT( "New interior setting!" ) );

		// Use previous/ current interpolation time if we're transitioning to the default worldinfo zone.
		InteriorStartTime = GCurrentTime;
		InteriorEndTime = InteriorStartTime + (Settings.bIsWorldInfo ? ListenerInteriorSettings.InteriorTime : Settings.InteriorTime);
		ExteriorEndTime = InteriorStartTime + (Settings.bIsWorldInfo ? ListenerInteriorSettings.ExteriorTime : Settings.ExteriorTime);
		InteriorLPFEndTime = InteriorStartTime + (Settings.bIsWorldInfo ? ListenerInteriorSettings.InteriorLPFTime : Settings.InteriorLPFTime);
		ExteriorLPFEndTime = InteriorStartTime + (Settings.bIsWorldInfo ? ListenerInteriorSettings.ExteriorLPFTime : Settings.ExteriorLPFTime);

		ListenerVolumeIndex = VolumeIndex;
		ListenerInteriorSettings = Settings;
	}
}

/**
 * UObject functions
 */
void UAudioDevice::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	if( Ar.IsObjectReferenceCollector() )
	{
		Ar << SoundClasses;
		Ar << SoundModes;
	}

	if( Ar.IsCountingMemory() )
	{
		Sources.CountBytes( Ar );
		FreeSources.CountBytes( Ar );
		WaveInstanceSourceMap.CountBytes( Ar );
		Ar.CountBytes( sizeof( FWaveInstance ) * WaveInstanceSourceMap.Num(), sizeof( FWaveInstance ) * WaveInstanceSourceMap.Num() );
		SourceSoundClasses.CountBytes( Ar );
		CurrentSoundClasses.CountBytes( Ar );
		DestinationSoundClasses.CountBytes( Ar );
		SoundClasses.CountBytes( Ar );
		SoundModes.CountBytes( Ar );
	}
}

/**
 * Complete the destruction of this class
 */
void UAudioDevice::FinishDestroy( void )
{
#if WITH_TTS
	if( TextToSpeech )
	{
		delete TextToSpeech;
		TextToSpeech = NULL;
	}
#endif

	Super::FinishDestroy();
}

void UAudioDevice::SetListener( INT ViewportIndex, INT MaxViewportIndex, const FVector& Location, const FVector& Up, const FVector& Right, const FVector& Front, UBOOL bUpdateVelocity )
{
	if( Listeners.Num() != MaxViewportIndex )
	{
		debugf( NAME_DevAudio, TEXT( "Resizing Listeners array: %d -> %d" ), Listeners.Num(), MaxViewportIndex );
		Listeners.Empty( MaxViewportIndex );
		Listeners.AddZeroed( MaxViewportIndex );
	}

	Listeners( ViewportIndex ).Velocity = bUpdateVelocity ? 
											(Location - Listeners( ViewportIndex ).Location) / GWorld->GetDeltaSeconds()
											: FVector::ZeroVector;
	
	Listeners( ViewportIndex ).Location = Location;
	Listeners( ViewportIndex ).Up = Up;
	Listeners( ViewportIndex ).Right = Right;
	Listeners( ViewportIndex ).Front = Front;
}

/**
 * Set up the sound EQ effect
 */
UBOOL UAudioDevice::SetSoundMode( FName NewSoundMode )
{
	USoundMode* NewMode = SoundModes.FindRef( NewSoundMode );
	if( NewMode )
	{
		Effects->SetModeSettings( NewMode );

		// Modify the sound class volumes for this mode (handled separately to EQ as it doesn't require audio effects to work)
		return( ApplySoundMode( NewMode ) );
	}

	debugfSuppressed( NAME_DevAudio, TEXT( "SetSoundMode(): Could not find SoundMode: %s" ), *NewSoundMode.ToString() );
	return( FALSE );
}

/**
 * Starts a transition to new reverb and interior settings
 *
 * @param   VolumeIndex			The object index of the volume
 * @param	ReverbSettings		The reverb settings to use.
 * @param	InteriorSettings	The interior settings to use.
 */
void UAudioDevice::SetAudioSettings( INT VolumeIndex, const FReverbSettings& ReverbSettings, const FInteriorSettings& InteriorSettings )
{
	Effects->SetReverbSettings( ReverbSettings );

	ApplyInteriorSettings( VolumeIndex, InteriorSettings );
}

/**
 * Platform dependent call to init effect data on a sound source
 */
void* UAudioDevice::InitEffect( FSoundSource* Source )
{
	return( Effects->InitEffect( Source ) );
}

/**
 * Platform dependent call to update the sound output with new parameters
 */
void* UAudioDevice::UpdateEffect( FSoundSource* Source )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioUpdateEffects );

	return( Effects->UpdateEffect( Source ) );
}

/**
 * Platform dependent call to destroy any effect related data
 */
void UAudioDevice::DestroyEffect( FSoundSource* Source )
{
	return( Effects->DestroyEffect( Source ) );
}

/**
 * Handle pausing/unpausing of sources when entering or leaving pause mode
 */
void UAudioDevice::HandlePause( UBOOL bGameTicking )
{
	// Pause all sounds if transitioning to pause mode.
	if( !bGameTicking && bGameWasTicking )
	{
		for( INT i = 0; i < Sources.Num(); i++ )
		{
			FSoundSource* Source = Sources( i );
			if( Source->IsGameOnly() )
			{
				Source->Pause();
			}
		}
	}
	// Unpause all sounds if transitioning back to game.
	else if( bGameTicking && !bGameWasTicking )
	{
		for( INT i = 0; i < Sources.Num(); i++ )
		{
			FSoundSource* Source = Sources( i );
			if( Source->IsGameOnly() )
			{
				Source->Play();
			}
		}
	}

	bGameWasTicking = bGameTicking;
}

/**
 * Iterate over the active AudioComponents for wave instances that could be playing.
 *
 * @return Index of first wave instance that can have a source attached
 */
INT UAudioDevice::GetSortedActiveWaveInstances( TArray<FWaveInstance*>& WaveInstances, UBOOL bGameTicking )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioGatherWaveInstances );

	// Update the portal volumes
	for( INT i = 0; i < Listeners.Num(); i++ )
	{
		Listeners( i ).PortalVolume = GWorld->GetWorldInfo()->GetPortalVolume( Listeners( i ).Location );
	}

	// Tick all the active audio components
	for( INT i = AudioComponents.Num() - 1; i >= 0; i-- )
	{
		UAudioComponent* AudioComponent = AudioComponents( i );

		if( !AudioComponent )
		{
			// Remove empty slot.
			AudioComponents.Remove( i );
		}
		else if( !AudioComponent->SoundCue )
		{
			// No sound cue - cleanup and remove
			AudioComponent->Stop();
		}
		// If the world scene allows audio - tick wave instances.
		else if( GWorld == NULL || GWorld->Scene == NULL || GWorld->Scene->AllowAudioPlayback() )
		{
			const FLOAT Duration = AudioComponent->GetDuration();
			// Divide by minimum pitch for longest possible duration
			if( Duration < INDEFINITELY_LOOPING_DURATION && AudioComponent->PlaybackTime > Duration / MIN_PITCH )
			{
				debugf( NAME_DevAudio, TEXT( "Sound stopped due to duration: %g > %g : %s" ), AudioComponent->PlaybackTime, Duration, *AudioComponent->GetName() );
				AudioComponent->Stop();
			}
			else
			{
				// If not in game, do not advance sounds unless they are UI sounds.
				FLOAT UsedDeltaTime = GDeltaTime;
				if( !bGameTicking && !AudioComponent->bIsUISound )
				{
					UsedDeltaTime = 0.0f;
				}

				// UpdateWaveInstances might cause AudioComponent to remove itself from AudioComponents TArray which is why why iterate in reverse order!
				AudioComponent->UpdateWaveInstances( this, WaveInstances, Listeners, UsedDeltaTime );
			}
		}
	}

	// Sort by priority (lowest priority first).
	Sort<USE_COMPARE_POINTER( FWaveInstance, UnAudio )>( &WaveInstances( 0 ), WaveInstances.Num() );

	// Return the first audible waveinstance
	INT FirstActiveIndex = Max( WaveInstances.Num() - MaxChannels, 0 );
	for( ; FirstActiveIndex < WaveInstances.Num(); FirstActiveIndex++ )
	{
		if( WaveInstances( FirstActiveIndex )->PlayPriority > KINDA_SMALL_NUMBER )
		{
			break;
		}
	}
	return( FirstActiveIndex );
}

/**
 * Stop sources that need to be stopped, and touch the ones that need to be kept alive
 * Stop sounds that are too low in priority to be played
 */
void UAudioDevice::StopSources( TArray<FWaveInstance*>& WaveInstances, INT FirstActiveIndex )
{
	// Touch sources that are high enough priority to play
	for( INT InstanceIndex = FirstActiveIndex; InstanceIndex < WaveInstances.Num(); InstanceIndex++ )
	{
		FWaveInstance* WaveInstance = WaveInstances( InstanceIndex );
		FSoundSource* Source = WaveInstanceSourceMap.FindRef( WaveInstance );
		if( Source )
		{
			Source->LastUpdate = CurrentTick;

			// If they are still audible, mark them as such
			if( WaveInstance->PlayPriority > KINDA_SMALL_NUMBER )
			{
				Source->LastHeardUpdate = CurrentTick;
			}
		}
	}

	// Stop inactive sources, sources that no longer have a WaveInstance associated
	// or sources that need to be reset because Stop & Play were called in the same frame.
	for( INT SourceIndex = 0; SourceIndex < Sources.Num(); SourceIndex++ )
	{
		FSoundSource* Source = Sources( SourceIndex );

		if( Source->WaveInstance )
		{
#if STATS && !CONSOLE
			if( Source->UsesCPUDecompression() )
			{
				INC_DWORD_STAT( STAT_OggWaveInstances );
			}
#endif
			// Source was not high enough priority to play this tick
			if( Source->LastUpdate != CurrentTick )
			{
				Source->Stop();
			}
			// Source has been inaudible for several ticks
			else if( Source->LastHeardUpdate + AUDIOSOURCE_TICK_LONGEVITY < CurrentTick )
			{
				Source->Stop();
			}
			else if( Source->WaveInstance && Source->WaveInstance->bIsRequestingRestart )
			{
				Source->Stop();
			}
		}
	}

	// Stop wave instances that are no longer playing due to priority reasons. This needs to happen AFTER
	// stopping sources as calling Stop on a sound source in turn notifies the wave instance of a buffer
	// being finished which might reset it being finished.
	for( INT InstanceIndex = 0; InstanceIndex < FirstActiveIndex; InstanceIndex++ )
	{
		FWaveInstance* WaveInstance = WaveInstances( InstanceIndex );
		WaveInstance->StopWithoutNotification();
		// debugf( NAME_DevAudio, TEXT( "SoundStoppedWithoutNotification due to priority reasons: %s" ), *WaveInstance->WaveData->GetName() );
	}

#if STATS
	DWORD AudibleInactiveSounds = 0;
	// Count how many sounds are not being played but were audible
	for( INT InstanceIndex = 0; InstanceIndex < FirstActiveIndex; InstanceIndex++ )
	{
		FWaveInstance* WaveInstance = WaveInstances( InstanceIndex );
		if( WaveInstance->Volume > 0.1f )
		{
			AudibleInactiveSounds++;
		}
	}
	SET_DWORD_STAT( STAT_AudibleWavesDroppedDueToPriority, AudibleInactiveSounds );
#endif
}

/**
 * Start and/or update any sources that have a high enough priority to play
 */
void UAudioDevice::StartSources( TArray<FWaveInstance*>& WaveInstances, INT FirstActiveIndex, UBOOL bGameTicking )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioStartSources );

	// Start sources as needed.
	for( INT InstanceIndex = FirstActiveIndex; InstanceIndex < WaveInstances.Num(); InstanceIndex++ )
	{
		FWaveInstance* WaveInstance = WaveInstances( InstanceIndex );

		// Editor uses bIsUISound for sounds played in the browser.
		if(	bGameTicking || WaveInstance->AudioComponent->bIsUISound )
		{
			FSoundSource* Source = WaveInstanceSourceMap.FindRef( WaveInstance );
			if( !Source )
			{
				check( FreeSources.Num() );
				Source = FreeSources.Pop();
				check( Source);

				// Try to initialize source.
				if( Source->Init( WaveInstance ) )
				{
					// Associate wave instance with it which is used earlier in this function.
					WaveInstanceSourceMap.Set( WaveInstance, Source );
					// Playback might be deferred till the end of the update function on certain implementations.
					Source->Play();

					//debugf( NAME_DevAudio, TEXT( "Playing: %s" ), *WaveInstance->WaveData->GetName() );
				}
				else
				{
					// This can happen if e.g. the USoundNodeWave pointed to by the WaveInstance is not a valid sound file.
					// If we don't stop the wave file, it will continue to try initializing the file every frame, which is a perf hit
					WaveInstance->StopWithoutNotification();
					FreeSources.AddItem( Source );
				}
			}
			else
			{
				Source->Update();
			}
		}
	}
}

/**
 * The audio system's main "Tick" function
 */
void UAudioDevice::Update( UBOOL bGameTicking )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioUpdateTime );

	// Start a new frame
	CurrentTick++;

	// Handle pause/unpause for the game and editor.
	HandlePause( bGameTicking );

	// Update the audio effects - reverb, EQ etc
	Effects->Update();

	// Gets the current state of the sound classes accounting for sound mode
	GetCurrentSoundClassState();

	// Gets the current state of the interior settings
	GetCurrentInteriorSettings();

	// Kill any sources that have finished
	for( INT SourceIndex = 0; SourceIndex < Sources.Num(); SourceIndex++ )
	{
		FSoundSource* SoundSource = Sources(SourceIndex);
		// Source has finished playing (it's one shot)
		if(SoundSource->IsPlaying() && SoundSource->IsFinished() )
		{
			SoundSource->Stop();
		}
	}

	// Poll audio components for active wave instances (== paths in node tree that end in a USoundNodeWave)
	TArray<FWaveInstance*> WaveInstances;
	INT FirstActiveIndex = GetSortedActiveWaveInstances( WaveInstances, bGameTicking );

	// Stop sources that need to be stopped, and touch the ones that need to be kept alive
	StopSources( WaveInstances, FirstActiveIndex );

	// Start and/or update any sources that have a high enough priority to play
	StartSources( WaveInstances, FirstActiveIndex, bGameTicking );

	INC_DWORD_STAT_BY( STAT_WaveInstances, WaveInstances.Num() );
	INC_DWORD_STAT_BY( STAT_AudioSources, MaxChannels - FreeSources.Num() );
	INC_DWORD_STAT_BY( STAT_WavesDroppedDueToPriority, Max( WaveInstances.Num() - MaxChannels, 0 ) );
	INC_DWORD_STAT_BY( STAT_AudioComponents, AudioComponents.Num() );
}

/**
 * Stops all game (and possibly UI) sounds
 *
 * @param bShouldStopUISounds If TRUE, this function will stop UI sounds as well
 */
void UAudioDevice::StopAllSounds( UBOOL bShouldStopUISounds )
{
	// go over all sound sources
	for( INT i = 0; i < Sources.Num(); i++ )
	{
		FSoundSource* Source = Sources( i );

		// stop game sounds, and UI also if desired
		if( Source->IsGameOnly() || bShouldStopUISounds )
		{
			// Stop audio component first.
			UAudioComponent* AudioComponent = Source->WaveInstance ? Source->WaveInstance->AudioComponent : NULL;
			if( AudioComponent )
			{
				AudioComponent->Stop();
			}

			// Stop source.
			Source->Stop();
		}
	}
}

void UAudioDevice::AddComponent( UAudioComponent* AudioComponent )
{
	check( AudioComponent );
	AudioComponents.AddUniqueItem( AudioComponent );
}

void UAudioDevice::RemoveComponent( UAudioComponent* AudioComponent )
{
	check( AudioComponent );

	for( INT i = 0; i < AudioComponent->WaveInstances.Num(); i++ )
	{
		FWaveInstance* WaveInstance = AudioComponent->WaveInstances( i );

		// Stop the owning sound source
		FSoundSource* Source = WaveInstanceSourceMap.FindRef( WaveInstance );
		if( Source )
		{
			Source->Stop();
		}
	}

	AudioComponents.RemoveItem( AudioComponent );
}

void UAudioDevice::SetClassVolume( FName ClassName, FLOAT Volume )
{
	// Set the volume in the original sound class
	USoundClass* Class = SoundClasses.FindRef( ClassName );
	if( Class )
	{
		// Find the propagated master class properties as the root for tree traversal.
		Class->Properties.Volume = Volume;
		ParseSoundClasses();
		ApplyClassAdjusters();
	}
	else
	{
		debugfSuppressed( NAME_DevAudio, TEXT( "Couldn't find specified sound class (%s) in UAudioDevice::SetClassVolume!" ), *ClassName.ToString() );
	}
}

UBOOL UAudioDevice::LocationIsAudible( FVector Location, FLOAT MaxDistance )
{
	if( MaxDistance >= WORLD_MAX )
	{
		return( TRUE );
	}

	MaxDistance *= MaxDistance;
	for( INT i = 0; i < Listeners.Num(); i++ )
	{
		if( ( Listeners( i ).Location - Location ).SizeSquared() < MaxDistance )
		{
			return( TRUE );
		}
	}

	return( FALSE );
}

UAudioComponent* UAudioDevice::CreateComponent( USoundCue* SoundCue, FSceneInterface* Scene, AActor* Actor, UBOOL bPlay, UBOOL bStopWhenOwnerDestroyed, FVector* Location )
{
	UAudioComponent* AudioComponent = NULL;
	
	if( SoundCue && GEngine && GEngine->bUseSound)
	{
		// Spawning can be disabled during map change. 
		UAudioDevice* AudioDevice = GEngine->Client ? GEngine->Client->GetAudioDevice() : NULL;
		if (!AudioDevice || !AudioDevice->bSoundSpawningEnabled)
		{
			return NULL;
		}

		// Don't create component if we have gone over the MaxConcurrentPlay, as it won't be able to Play, and might leak
		if( (SoundCue->MaxConcurrentPlayCount != 0) && (SoundCue->CurrentPlayCount >= SoundCue->MaxConcurrentPlayCount) )
		{
			debugfSuppressed( NAME_DevAudio, TEXT( "CreateComponent: MaxConcurrentPlayCount with Sound Cue: '%s' Max: %d   Curr: %d " ), SoundCue ? *SoundCue->GetName() : TEXT( "NULL" ), SoundCue->MaxConcurrentPlayCount, SoundCue->CurrentPlayCount );
		}
		// Avoid creating component if we're trying to play a sound on an already destroyed actor.
		else if( Actor && Actor->ActorIsPendingKill() )
		{
			// Don't create component on destroyed actor.
		}
		// Either no actor or actor is still alive.
		else if( !SoundCue->IsAudibleSimple( Location ) )
		{
			// Don't create a sound component for short sounds that start out of range of any listener
			debugfSuppressed( NAME_DevAudio, TEXT( "AudioComponent not created for out of range SoundCue %s" ), *SoundCue->GetName() );
		}
		else
		{
			// Use actor as outer if we have one.
			if( Actor )
			{
				AudioComponent = ConstructObject<UAudioComponent>( UAudioComponent::StaticClass(), Actor );
			}
			// Let engine pick the outer (transient package).
			else
			{
				AudioComponent = ConstructObject<UAudioComponent>( UAudioComponent::StaticClass() );
			}

			check( AudioComponent );

			AudioComponent->SoundCue = SoundCue;
			AudioComponent->bUseOwnerLocation = Actor ? TRUE : FALSE;
			AudioComponent->bAutoPlay = FALSE;
			AudioComponent->bIsUISound = FALSE;
			AudioComponent->bAutoDestroy = bPlay;
			AudioComponent->bStopWhenOwnerDestroyed = bStopWhenOwnerDestroyed;

			if( Actor )
			{
				// AActor::UpdateComponents calls this as well though we need an initial location as we manually create the component.
				AudioComponent->ConditionalAttach( Scene, Actor, Actor->LocalToWorld() );
				// Add this audio component to the actor's components array.
				Actor->Components.AddItem( AudioComponent );
			}
			else
			{
				AudioComponent->ConditionalAttach( Scene, NULL, FMatrix::Identity );
			}

			if( bPlay )
			{
				AudioComponent->Play();
			}
		}
	}

	return( AudioComponent );
}

/**
 * Stop all the audio components and sources attached to a scene. A NULL scene refers to all components.
 */
void UAudioDevice::Flush( FSceneInterface* SceneToFlush )
{
	// Stop all audio components attached to the scene
	UBOOL bFoundIgnoredComponent = FALSE;
	for( INT ComponentIndex = AudioComponents.Num() - 1; ComponentIndex >= 0; ComponentIndex-- )
	{
		UAudioComponent* AudioComponent = AudioComponents( ComponentIndex );
		if( AudioComponent )
		{
			// if we are in the editor we want to always flush the AudioComponents
			if( AudioComponent->bIgnoreForFlushing && !GIsEditor )
			{
				bFoundIgnoredComponent = TRUE;
			}
			else
			{
				FSceneInterface* ComponentScene = AudioComponent->GetScene();
				if( SceneToFlush == NULL || ComponentScene == NULL || ComponentScene == SceneToFlush )
				{
					AudioComponent->Stop();
				}
			}
		}
	}

	if( SceneToFlush == NULL )
	{
		// Make sure sounds are fully stopped.
		if( bFoundIgnoredComponent )
		{
			// We encountered an ignored component, so address the sounds individually.
			// There's no need to individually clear WaveInstanceSourceMap elements,
			// because FSoundSource::Stop(...) takes care of this.
			for( INT SourceIndex = 0; SourceIndex < Sources.Num(); SourceIndex++ )
			{
				const FWaveInstance* WaveInstance = Sources(SourceIndex)->GetWaveInstance();
				if( WaveInstance == NULL || !WaveInstance->AudioComponent->bIgnoreForFlushing )
				{
					Sources( SourceIndex )->Stop();
				}
			}
		}
		else
		{
			// No components were ignored, so stop all sounds.
			for( INT SourceIndex = 0; SourceIndex < Sources.Num(); SourceIndex++ )
			{
				Sources( SourceIndex )->Stop();
			}

			WaveInstanceSourceMap.Empty();
		}
	}
}

/**
 * Script access to GetSoundClass()
 */
 USoundClass* UAudioDevice::FindSoundClass( FName SoundClassName )
 {
	return( GetSoundClass( SoundClassName ) );
 }

/**
 * Returns the sound class properties associates with a sound class taking into account
 * the class tree.
 *
 * @param	SoundClassName	name of sound class to retrieve properties from
 * @return	sound class
 */
USoundClass* UAudioDevice::GetSoundClass( FName SoundClassName )
{
	USoundClass* SoundClass = SoundClasses.FindRef( SoundClassName );
	return( SoundClass );
}

FName UAudioDevice::GetSoundClass( INT ID )
{
#if WITH_EDITORONLY_DATA
	for( TMap<FName, USoundClass*>::TIterator It( SoundClasses ); It; ++It )
	{
		USoundClass* SoundClass = It.Value();
		if( SoundClass->MenuID == ID )
		{
			return( It.Key() );
		}
	}
#endif // WITH_EDITORONLY_DATA

	return( NAME_None );
}

/**
 * Returns the soundclass that contains the current state of the mode system
 *
 * @param	SoundClassName	name of sound class properties to retrieve
 * @return	sound class properties if it exists
 */
FSoundClassProperties* UAudioDevice::GetCurrentSoundClass( FName SoundClassName )
{
	FSoundClassProperties* SoundClassProperties = CurrentSoundClasses.Find( SoundClassName );
	return( SoundClassProperties );
}

/*-----------------------------------------------------------------------------
	USoundClass implementation.
-----------------------------------------------------------------------------*/

void USoundClass::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	if( Ar.Ver() >= VER_SOUND_CLASS_SERIALISATION_UPDATE )
	{
		Ar << EditorData;

		// Empty when loading and we don't care about saving it again, like e.g. a client.
		if( Ar.IsLoading() && ( !GIsEditor && !GIsUCC ) )
		{
			EditorData.Empty();
		}
	}
}

/**
 * Used by various commandlets to purge editor only and platform-specific data from various objects
 *
 * @param PlatformsToKeep Platforms for which to keep platform-specific data
 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
 */
void USoundClass::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData);
#if WITH_EDITORONLY_DATA
	// if we aren't keeping any non-stripped platforms, we can toss the data
	if (!(PlatformsToKeep & ~UE3::PLATFORM_Stripped) || GIsCookingForDemo )
	{
		EditorData.Empty();
	}
#endif // WITH_EDITORONLY_DATA
}



FString USoundClass::GetDesc( void )
{
	return( FString::Printf( TEXT( "Children: %d" ), ChildClassNames.Num() ) );
}

FString USoundClass::GetDetailedDescription( INT InIndex )
{
	INT Count = 0;
	FString Description = TEXT( "" );
	switch( InIndex )
	{
	case 0:
		Description = FString::Printf( TEXT( "Children: %d" ), ChildClassNames.Num() );
		break;

	case 1:
		if( !bIsChild )
		{
			Description = FString::Printf( TEXT( "No Parent" ) );
		}
		break;

	default:
		break;
	}

	return( Description );
}

#if CONSOLE || !WITH_EDITOR
/**
 * Called when a property value from a member struct or array has been changed in the editor.
 */
void USoundClass::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

/*-----------------------------------------------------------------------------
	USoundMode implementation.
-----------------------------------------------------------------------------*/

void USoundMode::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
}

FString USoundMode::GetDesc( void )
{
	return( FString::Printf( TEXT( "Adjusters: %d" ), SoundClassEffects.Num() ) );
}

FString USoundMode::GetDetailedDescription( INT InIndex )
{
	INT Count = 0;
	FString Description = TEXT( "" );
	switch( InIndex )
	{
	case 0:
		Description = FString::Printf( TEXT( "Adjusters: %d" ), SoundClassEffects.Num() );
		break;

	case 1:
		Description = bApplyEQ ? TEXT( "EQ" ) : TEXT( "No EQ" );
		break;

	default:
		break;
	}

	return( Description );
}

/**
 * Populate the enum using the serialised fname
 */
void USoundMode::Fixup( void )
{
	UEnum* SoundClassNamesEnum = FindObject<UEnum>( NULL, TEXT( "Engine.AudioDevice.ESoundClassName" ) );
	if( SoundClassNamesEnum )
	{
		for( INT i = 0; i < SoundClassEffects.Num(); i++ )
		{
			FSoundClassAdjuster& Adjuster = SoundClassEffects( i );
			INT SoundClassName = SoundClassNamesEnum->FindEnumIndex( Adjuster.SoundClass );
			if( SoundClassName != INDEX_NONE )
			{
				Adjuster.SoundClassName = ( BYTE )SoundClassName;
			}
			else
			{
				Adjuster.SoundClassName = 0;
				Adjuster.SoundClass = NAME_Master;
			}
		}
	}
}

/**
 * Called when a property value from a member struct or array has been changed in the editor.
 */
void USoundMode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UEnum* SoundClassNamesEnum = FindObject<UEnum>( NULL, TEXT( "Engine.AudioDevice.ESoundClassName" ) );
	if( SoundClassNamesEnum )
	{
		for( INT i = 0; i < SoundClassEffects.Num(); i++ )
		{
			FSoundClassAdjuster& Adjuster = SoundClassEffects( i );
			if( Adjuster.SoundClassName < SoundClassNamesEnum->NumEnums() )
			{
				Adjuster.SoundClass = SoundClassNamesEnum->GetEnum( ( INT )Adjuster.SoundClassName );
			}
			else
			{
				Adjuster.SoundClassName = 0;
				Adjuster.SoundClass = NAME_Master;
			}
		}
	}

	// Sanity check the EQ values
	EQSettings.LFFrequency = Clamp<FLOAT>( EQSettings.LFFrequency, MIN_FILTER_FREQUENCY, MAX_FILTER_FREQUENCY );
	EQSettings.LFGain = Clamp<FLOAT>( EQSettings.LFGain, MIN_FILTER_GAIN, MAX_FILTER_GAIN );

	EQSettings.MFCutoffFrequency = Clamp<FLOAT>( EQSettings.MFCutoffFrequency, MIN_FILTER_FREQUENCY, MAX_FILTER_FREQUENCY );
	EQSettings.MFBandwidth = Clamp<FLOAT>( EQSettings.MFBandwidth, MIN_FILTER_BANDWIDTH, MAX_FILTER_BANDWIDTH );
	EQSettings.MFGain = Clamp<FLOAT>( EQSettings.MFGain, MIN_FILTER_GAIN, MAX_FILTER_GAIN );

	EQSettings.HFFrequency = Clamp<FLOAT>( EQSettings.HFFrequency, MIN_FILTER_FREQUENCY, MAX_FILTER_FREQUENCY );
	EQSettings.HFGain = Clamp<FLOAT>( EQSettings.HFGain, MIN_FILTER_GAIN, MAX_FILTER_GAIN );

	// Refresh the browser
	GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI, this ) );
}

/*-----------------------------------------------------------------------------
	FSoundSource implementation.
-----------------------------------------------------------------------------*/

void FSoundSource::Stop( void )
{
	if( WaveInstance )
	{
		check( AudioDevice );
		AudioDevice->FreeSources.AddUniqueItem( this );
		AudioDevice->WaveInstanceSourceMap.Remove( WaveInstance );
		WaveInstance->NotifyFinished();
		WaveInstance->bIsRequestingRestart = FALSE;
		WaveInstance = NULL;
	}
	else
	{
		check( AudioDevice->FreeSources.FindItemIndex( this ) != INDEX_NONE );
	}
}

/**
 * Returns whether associated audio component is an ingame only component, aka one that will
 * not play unless we're in game mode (not paused in the UI)
 *
 * @return FALSE if associated component has bIsUISound set, TRUE otherwise
 */
UBOOL FSoundSource::IsGameOnly( void )
{
	if( WaveInstance
		&& WaveInstance->AudioComponent
		&& WaveInstance->AudioComponent->bIsUISound )
	{
		return( FALSE );
	}

	return( TRUE );
}

/**
 * Set the bReverbApplied variable
 */
UBOOL FSoundSource::SetReverbApplied( UBOOL bHardwareAvailable )
{
	// Do not apply reverb if it is explicitly disallowed
	bReverbApplied = WaveInstance->bReverb && bHardwareAvailable;

	// Do not apply reverb to music
	if( WaveInstance->bIsMusic )
	{
		bReverbApplied = FALSE;
	}

	// Do not apply reverb to multichannel sounds
	if( WaveInstance->WaveData->NumChannels > 2 )
	{
		bReverbApplied = FALSE;
	}

	return( bReverbApplied );
}

/**
 * Set the SetStereoBleed variable
 */
FLOAT FSoundSource::SetStereoBleed( void )
{
	StereoBleed = 0.0f;

	// All stereo sounds bleed by default
	if( WaveInstance->WaveData->NumChannels == 2 )
	{
		StereoBleed = WaveInstance->StereoBleed;

		if( AudioDevice->GetMixDebugState() == DEBUGSTATE_TestStereoBleed )
		{
			StereoBleed = 1.0f;
		}
	}

	return( StereoBleed );
}

/**
 * Set the SetLFEBleed variable
 */
FLOAT FSoundSource::SetLFEBleed( void )
{
	LFEBleed = WaveInstance->LFEBleed;

	if( AudioDevice->GetMixDebugState() == DEBUGSTATE_TestLFEBleed )
	{
		LFEBleed = 10.0f;
	}

	return( LFEBleed );
}

/**
 * Set the HighFrequencyGain value
 */
void FSoundSource::SetHighFrequencyGain( void )
{
	HighFrequencyGain = Clamp<FLOAT>( WaveInstance->HighFrequencyGain, MIN_FILTER_GAIN, 1.0f );

	if( AudioDevice->GetMixDebugState() == DEBUGSTATE_DisableLPF )
	{
		HighFrequencyGain = 1.0f;
	}
	else if( AudioDevice->GetMixDebugState() == DEBUGSTATE_TestLPF )
	{
		HighFrequencyGain = MIN_FILTER_GAIN;
	}
}

/*-----------------------------------------------------------------------------
	FWaveInstance implementation.
-----------------------------------------------------------------------------*/

/** Helper to create good unique type hashs for FWaveInstance instances */
DWORD FWaveInstance::TypeHashCounter = 0;

/**
 * Constructor, initializing all member variables.
 *
 * @param InAudioComponent	Audio component this wave instance belongs to.
 */
FWaveInstance::FWaveInstance( UAudioComponent* InAudioComponent )
:	WaveData( NULL )
,	NotifyBufferFinishedHook( NULL )
,	AudioComponent( InAudioComponent )
,	Volume( 0.0f )
,	VolumeMultiplier( 1.0f )
,	PlayPriority( 0.0f )
,	VoiceCenterChannelVolume( 0.0f )
,	RadioFilterVolume( 0.0f )
,	RadioFilterVolumeThreshold( 0.0f )
,	bApplyRadioFilter( FALSE )
,	LoopingMode( LOOP_Never )
,	bIsStarted( FALSE )
,	bIsFinished( FALSE )
,	bAlreadyNotifiedHook( FALSE )
,	bUseSpatialization( FALSE )
,	bIsRequestingRestart( FALSE )
,	StereoBleed( 0.0f )
,	LFEBleed( 0.0f )
,	bEQFilterApplied( FALSE )
,	bAlwaysPlay( FALSE )
,	bIsUISound( FALSE )
,	bIsMusic( FALSE )
,	bReverb( TRUE )
,	bCenterChannelOnly( FALSE )
,	HighFrequencyGain( 1.0f )
,	Pitch( 0.0f )
,	Velocity( FVector( 0.0f, 0.0f, 0.0f ) )
,	Location( FVector( 0.0f, 0.0f, 0.0f ) )
,	OmniRadius( 0.0f )
{
	TypeHash = ++TypeHashCounter;
}

/**
 * Notifies the wave instance that it has finished.
 */
UBOOL FWaveInstance::NotifyFinished( void )
{
	if( !bAlreadyNotifiedHook )
	{
		// Can't have a source finishing that hasn't started
		if( !bIsStarted )
		{
			debugf( NAME_Warning, TEXT( "Received finished notification from waveinstance that hasn't started!" ) );
		}

		// We are finished.
		bIsFinished = TRUE;

		// Avoid double notifications.
		bAlreadyNotifiedHook = TRUE;

		if( NotifyBufferFinishedHook && AudioComponent )
		{
			// Notify NotifyBufferFinishedHook that the current playback buffer has finished...
			UBOOL bFinishedLooping = NotifyBufferFinishedHook->NotifyWaveInstanceFinished( this );
			return( bFinishedLooping );
		}
	}

	return( FALSE );
}

/**
 * Stops the wave instance without notifying NotifyWaveInstanceFinishedHook. This will NOT stop wave instance
 * if it is set up to loop indefinitely or set to remain active.
 */
void FWaveInstance::StopWithoutNotification( void )
{
	if( LoopingMode == LOOP_Forever || ( AudioComponent && AudioComponent->bShouldRemainActiveIfDropped ) )
	{
		// We don't finish if we're either indefinitely looping or the audio component explicitly mandates that we should
		// remain active which is e.g. used for engine sounds and such.
		bIsFinished = FALSE;
	}
	else
	{
		// We're finished.
		bIsFinished = TRUE;
	}
}

FArchive& operator<<( FArchive& Ar, FWaveInstance* WaveInstance )
{
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << WaveInstance->WaveData << WaveInstance->NotifyBufferFinishedHook << WaveInstance->AudioComponent;
	}
	return( Ar );
}

/*-----------------------------------------------------------------------------
	FAudioComponentSavedState implementation.
-----------------------------------------------------------------------------*/

void FAudioComponentSavedState::Set( UAudioComponent* AudioComponent )
{
	CurrentNotifyBufferFinishedHook	= AudioComponent->CurrentNotifyBufferFinishedHook;
	CurrentLocation = AudioComponent->CurrentLocation;
	CurrentVolume = AudioComponent->CurrentVolume;
	CurrentPitch = AudioComponent->CurrentPitch;
	CurrentHighFrequencyGain = AudioComponent->CurrentHighFrequencyGain;
	CurrentUseSpatialization = AudioComponent->CurrentUseSpatialization;
	CurrentNotifyOnLoop = AudioComponent->CurrentNotifyOnLoop;
}

void FAudioComponentSavedState::Restore( UAudioComponent* AudioComponent )
{
	AudioComponent->CurrentNotifyBufferFinishedHook	= CurrentNotifyBufferFinishedHook;
	AudioComponent->CurrentLocation = CurrentLocation;
	AudioComponent->CurrentVolume = CurrentVolume;
	AudioComponent->CurrentPitch = CurrentPitch;
	AudioComponent->CurrentHighFrequencyGain = CurrentHighFrequencyGain;
	AudioComponent->CurrentUseSpatialization = CurrentUseSpatialization;
	AudioComponent->CurrentNotifyOnLoop = CurrentNotifyOnLoop;
}

void FAudioComponentSavedState::Reset( UAudioComponent* AudioComponent )
{
	AudioComponent->CurrentNotifyBufferFinishedHook	= NULL;
	AudioComponent->CurrentVolume = 1.0f,
	AudioComponent->CurrentPitch = 1.0f;
	AudioComponent->CurrentHighFrequencyGain = 1.0f;
	AudioComponent->CurrentUseSpatialization = 0;
	AudioComponent->CurrentLocation = AudioComponent->bUseOwnerLocation ? AudioComponent->ComponentLocation : AudioComponent->Location;
	AudioComponent->CurrentNotifyOnLoop = FALSE;
}

/*-----------------------------------------------------------------------------
	UAudioComponent implementation.
-----------------------------------------------------------------------------*/

void UAudioComponent::Attach( void )
{
	Super::Attach();
	if( bAutoPlay )
	{
		Play();
	}
}

void UAudioComponent::FinishDestroy( void )
{
	// Delete wave instances and remove from audio device.
	Cleanup();

	// Route Destroy event.
	Super::FinishDestroy();
}

#if USE_GAMEPLAY_PROFILER
/**
 * This function actually does the work for the GetProfilerAssetObject and is virtual.
 * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
 **/
UObject* UAudioComponent::GetProfilerAssetObjectInternal() const
{
	return SoundCue;
}
#endif

/**
 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
 * you have a component of interest but what you really want is some characteristic that you can use to track
 * down where it came from.
 */
FString UAudioComponent::GetDetailedInfoInternal( void ) const
{
	FString Result;

	if( SoundCue != NULL )
	{
		Result = SoundCue->GetPathName( NULL );
	}
	else
	{
		Result = TEXT( "No_SoundCue" );
	}

	return Result;
}

void UAudioComponent::Detach( UBOOL bWillReattach )
{
	// Route Detach event.
	Super::Detach( bWillReattach );

	// Don't stop audio and clean up component if owner has been destroyed (default behaviour). This function gets
	// called from AActor::ClearComponents when an actor gets destroyed which is not usually what we want for one-
	// shot sounds.
	if( !Owner || (!bWillReattach && bStopWhenOwnerDestroyed) )
	{
		Cleanup();
	}
	else if( Owner->IsPendingKill() && !bStopWhenOwnerDestroyed && GIsGame )
	{
		// Detach from pending kill owner so we don't get marked as pending kill ourselves.
		Owner = NULL;
	}
}

/**
 * Returns whether component should be marked as pending kill inside AActor::MarkComponentsAsPendingKill if it has
 * a say in it. Components that should continue playback after the deletion of their owner return FALSE in this case.
 *
 * @return TRUE if we allow being marked as pending kill, FALSE otherwise
 */
UBOOL UAudioComponent::AllowBeingMarkedPendingKill( void )
{
	return( !Owner || bStopWhenOwnerDestroyed );
}

/**
 * Dissociates component from audio device and deletes wave instances. Also causes sound to be stopped.
 */
void UAudioComponent::Cleanup( void )
{
	if( bWasPlaying && !GExitPurge )
	{
		// @see UAudioComponent::Stop()  we set this to null there.  so if we bypass stop and just call
		// ::Cleanup (for things like emitters which are destroyed) we need to decrement CurrentPlayCount
		if( CueFirstNode && SoundCue )
		{
			SoundCue->CurrentPlayCount = Max( SoundCue->CurrentPlayCount - 1, 0 );
		}

		// Removes component from the audio device's component list, implicitly also stopping the sound.
		UAudioDevice* AudioDevice = GEngine && GEngine->Client ? GEngine->Client->GetAudioDevice() : NULL;
		if( AudioDevice )
		{
			AudioDevice->RemoveComponent( this );
		}

		// Delete wave instances.
		for( INT InstanceIndex = 0; InstanceIndex < WaveInstances.Num(); InstanceIndex++ )
		{
			FWaveInstance* WaveInstance = WaveInstances( InstanceIndex );

			// Dequeue subtitles for this sounds
			FSubtitleManager::GetSubtitleManager()->KillSubtitles( ( PTRINT )WaveInstance );

			delete WaveInstance;
		}

		// Reset hook so it can be GCed if other code changes soundcue.
		CurrentNotifyBufferFinishedHook = NULL;

		// Don't clear instance parameters in the editor.
		// Necessary to support defining parameters via the property window, where PostEditChanges
		// routes component reattach, which would immediately clear the just-edited parameters.
		if( !GIsEditor || GIsPlayInEditorWorld )
		{
			InstanceParameters.Empty();
		}

		// Reset flags
		bWasOccluded = FALSE;
		bIsUISound = FALSE;

		// Force re-initialization on next play.
		SoundNodeData.Empty();
		SoundNodeOffsetMap.Empty();
		SoundNodeResetWaveMap.Empty();
		WaveInstances.Empty();

		// We cleaned up everything.
		bWasPlaying = FALSE;
	}

	PlaybackTime = 0.0f;
	LastOcclusionCheckTime = 0.0f;
	OcclusionCheckInterval = 0.0f;

	// Reset fade in variables
	FadeInStartTime = 0.0f;
	FadeInStopTime = -1.0f;
	FadeInTargetVolume = 1.0f;

	// Reset fade out variables
	FadeOutStartTime = 0.0f;
	FadeOutStopTime = -1.0f;
	FadeOutTargetVolume = 1.0f;

	// Reset adjust variables
	AdjustVolumeStartTime = 0.0f;
	AdjustVolumeStopTime = -1.0f;
	AdjustVolumeTargetVolume = 1.0f;
	CurrAdjustVolumeTargetVolume = 1.0f;

	LastUpdateTime = 0.0;
	SourceInteriorVolume = 1.0f;
	SourceInteriorLPF = 1.0f;
	CurrentInteriorVolume = 1.0f;
	CurrentInteriorLPF = 1.0f;

	bApplyRadioFilter = FALSE;
	bRadioFilterSelected = FALSE;
}

void UAudioComponent::SetParentToWorld( const FMatrix& ParentToWorld )
{
	Super::SetParentToWorld( ParentToWorld );
	ComponentLocation = ParentToWorld.GetOrigin();
}

void UAudioComponent::SetSoundCue( USoundCue* NewSoundCue )
{
	// We shouldn't really have any references to auto-destroy audiocomponents, and also its kind of undefined what changing the sound cue on
	// one with this flag set should do.
	check( !bAutoDestroy );

	Stop();
	SoundCue = NewSoundCue;
}

/**
 * Start a sound cue playing on an audio component
 */
void UAudioComponent::Play( void )
{
	debugfSuppressed( NAME_DevAudioVerbose, TEXT( "%g: Playing AudioComponent : '%s' with Sound Cue: '%s'" ), GWorld ? GWorld->GetAudioTimeSeconds() : 0.0f, *GetFullName(), SoundCue ? *SoundCue->GetName() : TEXT( "NULL" ) );
	
	// Playing is not allowed during map change.
	UAudioDevice* AudioDevice = GEngine && GEngine->Client ? GEngine->Client->GetAudioDevice() : NULL;
	if (!AudioDevice || !AudioDevice->bSoundSpawningEnabled)
	{
		return;
	}

	// if we have gone over the MaxConcurrentPlay
	if( ( SoundCue != NULL ) && ( SoundCue->MaxConcurrentPlayCount != 0 ) && ( SoundCue->CurrentPlayCount >= SoundCue->MaxConcurrentPlayCount ) )
	{
		//debugf( NAME_DevAudioVerbose, TEXT( "   %g: MaxConcurrentPlayCount AudioComponent : '%s' with Sound Cue: '%s' Max: %d   Curr: %d " ), GWorld ? GWorld->GetAudioTimeSeconds() : 0.0f, *GetFullName(), SoundCue ? *SoundCue->GetName() : TEXT( "NULL" ), SoundCue->MaxConcurrentPlayCount, SoundCue->CurrentPlayCount );
		return;
	}

#if !FINAL_RELEASE
	if( GShouldLogAllPlaySoundCalls == TRUE )
	{
		warnf( TEXT("%s::Play %s  Loc: %s"), *GetFullName(), *SoundCue->GetName(), *Location.ToString() );
	}
#endif


	// Cache root node of sound container to avoid subsequent dereferencing.
	if( SoundCue )
	{
		CueFirstNode = SoundCue->FirstNode;
	}

	// Reset variables if we were already playing.
	if( bWasPlaying )
	{
		for( INT InstanceIndex = 0; InstanceIndex < WaveInstances.Num(); InstanceIndex++ )
		{
			FWaveInstance* WaveInstance = WaveInstances( InstanceIndex );
			if( WaveInstance )
			{
				WaveInstance->bIsStarted = TRUE;
				WaveInstance->bIsFinished = FALSE;
				WaveInstance->bIsRequestingRestart = TRUE;
			}
		}

		// stop any fadeins or fadeouts
		FadeInStartTime = 0.0f;
		FadeInStopTime = -1.0f;
		FadeInTargetVolume = 1.0f;

		FadeOutStartTime = 0.0f;
		FadeOutStopTime = -1.0f;
		FadeOutTargetVolume = 1.0f;
	}
	// Increase the cue's current play count if we're starting up.
	else if( SoundCue )
	{
		SoundCue->CurrentPlayCount++;
	}

	PlaybackTime = 0.0f;
	bFinished = FALSE;
	bWasPlaying = TRUE;
	bApplyRadioFilter = FALSE;
	bRadioFilterSelected = FALSE;
	LastOwner = Owner;

	// Adds component from the audio device's component list.
	if( AudioDevice )
	{
		AudioDevice->AddComponent( this );
	}
}

/**
 * Stop an audio component playing its sound cue, issue any delegates if needed
 */
void UAudioComponent::Stop( void )
{
	debugfSuppressed( NAME_DevAudioVerbose, TEXT( "%g: Stopping AudioComponent : '%s' with Sound Cue: '%s'" ), GWorld ? GWorld->GetAudioTimeSeconds() : 0.0f, *GetFullName(), SoundCue ? *SoundCue->GetName() : TEXT( "NULL" ) );

	// Decrease the cue's current play count on the first call to Stop.
	if( CueFirstNode && SoundCue )
	{
		SoundCue->CurrentPlayCount = Max( SoundCue->CurrentPlayCount - 1, 0 );
	}

	// For safety, clear out the cached root sound node pointer.
	CueFirstNode = NULL;

	// We're done.
	bFinished = TRUE;

	UBOOL bOldWasPlaying = bWasPlaying;

	// Clean up intermediate arrays which also dissociates from the audio device, hence stopping the sound.
	// This leaves the component in a state that is suited for having Play called on it again to restart
	// playback from the beginning.
	Cleanup();

	if( bOldWasPlaying && GWorld != NULL && DELEGATE_IS_SET( OnAudioFinished ) )
	{
		INC_DWORD_STAT( STAT_AudioFinishedDelegatesCalled );
		SCOPE_CYCLE_COUNTER( STAT_AudioFinishedDelegates );
		delegateOnAudioFinished( this );
	}

	// Auto destruction is handled via marking object for deletion.
	if( bAutoDestroy )
	{
		// If no owner, or not detached from owner, remove from the last owners component array
		if( LastOwner )
		{
			LastOwner->DetachComponent( this );
			LastOwner = NULL;
		}

		// Mark object for deletion and reference removal.
		MarkPendingKill();
	}
}

/** Returns TRUE if this component is currently playing a SoundCue. */
UBOOL UAudioComponent::IsPlaying( void )
{
	return ( bWasPlaying && !bFinished );
}

void UAudioComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Reset variables.
	if( bWasPlaying )
	{
		for( INT InstanceIndex = 0; InstanceIndex < WaveInstances.Num(); InstanceIndex++ )
		{
			FWaveInstance* WaveInstance = WaveInstances( InstanceIndex );
			if( WaveInstance )
			{
				WaveInstance->bIsStarted = TRUE;
				WaveInstance->bIsFinished = FALSE;
				WaveInstance->bIsRequestingRestart = TRUE;
			}
		}
	}

	PlaybackTime = 0.0f;
	bFinished = FALSE;

	// Clear node offset associations and data so dynamic data gets re-initialized.
	SoundNodeData.Empty();
	SoundNodeOffsetMap.Empty();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UAudioComponent::CheckOcclusion( const FVector& ListenerLocation )
{
	// @optimization: dont do this if listener and current locations haven't changed much?
	// also, should this use getaudiotimeseconds?
	if( OcclusionCheckInterval > 0.0f && GWorld->GetTimeSeconds() - LastOcclusionCheckTime > OcclusionCheckInterval && SoundCue->MaxAudibleDistance != WORLD_MAX )
	{
		LastOcclusionCheckTime = GWorld->GetTimeSeconds();
		FCheckResult Hit( 1.0f );
		UBOOL bNowOccluded = !GWorld->SingleLineCheck( Hit, GetOwner(), ListenerLocation, GetPointForDistanceEval(), TRACE_World | TRACE_StopAtAnyHit );
		if( bNowOccluded != bWasOccluded )
		{
			bWasOccluded = bNowOccluded;
			eventOcclusionChanged( bNowOccluded );
		}
	}
}

/**
 * Apply the interior settings to the ambient sound as appropriate
 */
void UAudioComponent::HandleInteriorVolumes( UAudioDevice* AudioDevice, AWorldInfo* WorldInfo, UBOOL AlwaysRecalculate, const FVector& TrueSpeakerPosition )
{
	// Get the settings of the ambient sound
	FInteriorSettings Ambient;
	INT ReverbVolumeIndex;
	if (AlwaysRecalculate || (GIsEditor && !GIsGame) || (TrueSpeakerPosition - LastLocation).SizeSquared() > KINDA_SMALL_NUMBER)
	{
		ReverbVolumeIndex = WorldInfo->GetAudioSettings(TrueSpeakerPosition, NULL, &Ambient);
		LastInteriorSettings = Ambient;
		LastReverbVolumeIndex = ReverbVolumeIndex;
	}
	else
	{
		// use previous settings as we haven't moved
		Ambient = LastInteriorSettings;
		ReverbVolumeIndex = LastReverbVolumeIndex;
	}

	// Check to see if we've moved to a new reverb volume
	if( LastUpdateTime < AudioDevice->InteriorStartTime )
	{
		SourceInteriorVolume = CurrentInteriorVolume;
		SourceInteriorLPF = CurrentInteriorLPF;
		LastUpdateTime = GCurrentTime;
	}

	if( AudioDevice->ListenerVolumeIndex == ReverbVolumeIndex )
	{
		// Ambient and listener in same ambient zone
		CurrentInteriorVolume = ( SourceInteriorVolume * ( 1.0f - AudioDevice->InteriorVolumeInterp ) ) + AudioDevice->InteriorVolumeInterp;
		CurrentVolumeMultiplier *= CurrentInteriorVolume;

		CurrentInteriorLPF = ( SourceInteriorLPF * ( 1.0f - AudioDevice->InteriorLPFInterp ) ) + AudioDevice->InteriorLPFInterp;
		CurrentHighFrequencyGainMultiplier *= CurrentInteriorLPF;

		debugfSuppressed( NAME_DevAudioVerbose, TEXT( "Ambient in same volume. Volume *= %g LPF *= %g (%s)" ),
			CurrentInteriorVolume, CurrentInteriorLPF, ( WaveInstances.Num() > 0 ) ? *WaveInstances( 0 )->WaveData->GetName() : TEXT( "NULL" ) );
	}
	else
	{
		// Ambient and listener in different ambient zone
		if( Ambient.bIsWorldInfo )
		{
			// The ambient sound is 'outside' - use the listener's exterior volume
			CurrentInteriorVolume = ( SourceInteriorVolume * ( 1.0f - AudioDevice->ExteriorVolumeInterp ) ) + ( AudioDevice->ListenerInteriorSettings.ExteriorVolume * AudioDevice->ExteriorVolumeInterp );
			CurrentVolumeMultiplier *= CurrentInteriorVolume;

			CurrentInteriorLPF = ( SourceInteriorLPF * ( 1.0f - AudioDevice->ExteriorLPFInterp ) ) + ( AudioDevice->ListenerInteriorSettings.ExteriorLPF * AudioDevice->ExteriorLPFInterp );
			CurrentHighFrequencyGainMultiplier *= CurrentInteriorLPF;

			debugfSuppressed( NAME_DevAudioVerbose, TEXT( "Ambient in diff volume, ambient outside. Volume *= %g LPF *= %g (%s)" ),
				CurrentInteriorVolume, CurrentInteriorLPF, ( WaveInstances.Num() > 0 ) ? *WaveInstances( 0 )->WaveData->GetName() : TEXT( "NULL" ) );
		}
		else
		{
			// The ambient sound is 'inside' - use the ambient sound's interior volume
			CurrentInteriorVolume = ( SourceInteriorVolume * ( 1.0f - AudioDevice->InteriorVolumeInterp ) ) + ( Ambient.InteriorVolume * AudioDevice->InteriorVolumeInterp );
			FLOAT CurrentExteriorVolume = ( SourceInteriorVolume * ( 1.0f - AudioDevice->ExteriorVolumeInterp ) ) + ( AudioDevice->ListenerInteriorSettings.ExteriorVolume * AudioDevice->ExteriorVolumeInterp );
			CurrentVolumeMultiplier *= CurrentInteriorVolume*CurrentExteriorVolume;

			CurrentInteriorLPF = ( SourceInteriorLPF * ( 1.0f - AudioDevice->InteriorLPFInterp ) ) + ( Ambient.InteriorLPF * AudioDevice->InteriorLPFInterp );
			FLOAT CurrentExteriorLPF = ( SourceInteriorLPF * ( 1.0f - AudioDevice->ExteriorLPFInterp ) ) + ( AudioDevice->ListenerInteriorSettings.ExteriorLPF * AudioDevice->ExteriorLPFInterp );
			CurrentHighFrequencyGainMultiplier *= CurrentInteriorLPF*CurrentExteriorLPF;

			debugfSuppressed( NAME_DevAudioVerbose, TEXT( "Ambient in diff volume, ambient inside. Volume *= %g LPF *= %g (%s)" ),
				CurrentInteriorVolume, CurrentInteriorLPF, ( WaveInstances.Num() > 0 ) ? *WaveInstances( 0 )->WaveData->GetName() : TEXT( "NULL" ) );
		}
	}
}

FLOAT UAudioComponent::GetDuration( )
{
	return (SoundCue != NULL) ? SoundCue->GetCueDuration() : 0.0f; 
}

FVector USplineAudioComponent::FindVirtualSpeakerPosition(const TArray< FInterpCurveVector::FPointOnSpline >& Points, FVector Listener, FLOAT Radius, INT * ClosestPointIndex)
{
	if(NULL != ClosestPointIndex)
	{
		*ClosestPointIndex = -1;
	}

	FVector Result(0.0f, 0.0f, 0.0f);
	FLOAT WeightSum = 0.0f;
	const FLOAT RadiusSq = Radius * Radius;
	FLOAT BestDistanceSq = BIG_NUMBER;

	for(INT i = 0; i < Points.Num(); i++)
	{
		const FVector Point = Points(i).Position;
		const FLOAT DistanceSq = Point.DistanceSquared(Listener);
		if(DistanceSq < RadiusSq)
		{
			const FLOAT Distance = appSqrt(DistanceSq);
			const FLOAT Weight = 
				// weight function - linear:
				1.0f - (Distance/Radius);

			Result += Points(i).Position * Weight;
			WeightSum += Weight;
			if(DistanceSq < BestDistanceSq && NULL != ClosestPointIndex)
			{
				BestDistanceSq = DistanceSq;
				*ClosestPointIndex = i;
			}
		}
	}
	if(WeightSum <= 0.0f)
	{
		return FVector(BIG_NUMBER, BIG_NUMBER, BIG_NUMBER);
	}

	return Result / WeightSum;
}

/** 
 * @param InListeners all listeners list
 * @param ClosestListenerIndexOut through this variable index of the closest listener is returned 
 * @return Closest RELATIVE location of sound (relative to position of the closest listener). 
 */
FVector USplineAudioComponent::FindClosestLocation( const TArray<struct FListener>& InListeners, INT& ClosestListenerIndexOut )
{
	//@note: USplineAudioComponent doesn't handle portals

	if( Points.Num() <= 1 || ListenerScopeRadius <= 0.0f || InListeners.Num() < 1 )
	{
		return Super::FindClosestLocation(InListeners, ClosestListenerIndexOut);
	}

	INT ClosestListenerIndex = 0;
	INT BestPointIndex = -1;
	FVector BestPosition = FindVirtualSpeakerPosition( Points, InListeners( 0 ).Location, ListenerScopeRadius, &BestPointIndex );
	FLOAT BestDistanceSq = BestPosition.DistanceSquared(InListeners( 0 ).Location);

	for( INT i = 1; i < InListeners.Num(); i++ )
	{
		INT TempClosestPointIndex = -1;
		const FVector VirtualSpeakerPosition = FindVirtualSpeakerPosition( Points, InListeners( i ).Location, ListenerScopeRadius, &TempClosestPointIndex );
		const FLOAT DistanceSq = VirtualSpeakerPosition.DistanceSquared(InListeners( i ).Location);
		if(DistanceSq < BestDistanceSq)
		{
			ClosestListenerIndex = i;
			BestDistanceSq = DistanceSq;
			BestPosition = VirtualSpeakerPosition;
			BestPointIndex = TempClosestPointIndex;
		}
	}

	ClosestPointOnSplineIndex = BestPointIndex;

	ClosestListenerIndexOut = ClosestListenerIndex;
	return BestPosition;
}

FVector USplineAudioComponent::GetPointForDistanceEval()
{
	if(ClosestPointOnSplineIndex >= 0 && ClosestPointOnSplineIndex < Points.Num())
	{
		return Points(ClosestPointOnSplineIndex).Position;
	}
	return Super::GetPointForDistanceEval();
}

UBOOL USplineAudioComponent::SetSplineData(const FInterpCurveVector& SplineData, FLOAT DistanceBetweenPoints)
{
	ClosestPointOnSplineIndex = 0;

	if(SplineData.Points.Num() <= 1)
		return FALSE;

	if(DistanceBetweenPoints <= 0.0f)
		return FALSE;
	
	SplineData.UniformDistributionInRespectToLength(DistanceBetweenPoints, Points, 30);
	return TRUE;
}

FVector UAudioComponent::GetPointForDistanceEval()
{
	return CurrentLocation;
}

/** 
 * @param InListeners all listeners list
 * @param ClosestListenerIndexOut through this variable index of the closest listener is returned 
 * @return Closest RELATIVE location of sound (relative to position of the closest listener). 
 */
FVector UAudioComponent::FindClosestLocation( const TArray<struct FListener>& InListeners, INT& ClosestListenerIndexOut )
{
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

	INT ClosestListenerIndex = 0;
	FVector ClosestLocation = WorldInfo->RemapLocationThroughPortals( CurrentLocation, InListeners( 0 ).Location );
	FLOAT ClosestDistSq = ( ClosestLocation - InListeners( 0 ).Location ).SizeSquared();

	for( INT i = 1; i < InListeners.Num(); i++ )
	{
		const FVector TestLocation = InListeners( i ).Location;
		const FVector ModifiedLocation = WorldInfo->RemapLocationThroughPortals( CurrentLocation, TestLocation );
		const FLOAT DistSq = ( ModifiedLocation - TestLocation ).SizeSquared();
		if( DistSq < ClosestDistSq )
		{
			ClosestListenerIndex = i;
			ClosestDistSq = DistSq;
			ClosestLocation = ModifiedLocation;
		}
	}
	ClosestListenerIndexOut = ClosestListenerIndex;
	return ClosestLocation;
}

void UAudioComponent::UpdateWaveInstances( UAudioDevice* AudioDevice, TArray<FWaveInstance*> &InWaveInstances, const TArray<FListener>& InListeners, FLOAT DeltaTime )
{
	check( AudioDevice );

	// Early outs.
	if( CueFirstNode == NULL || SoundCue == NULL )
	{
		return;
	}

	//@todo audio: Need to handle pausing and not getting out of sync by using the mixer's time.
	//@todo audio: Fading in and out is also dependent on the DeltaTime
	PlaybackTime += DeltaTime;

	// Reset temporary variables used for node traversal.
	FAudioComponentSavedState::Reset( this );

	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

	// splitscreen support:
	// we always pass the 'primary' listener (viewport 0) to the sound nodes and the underlying audio system
	// then move the AudioComponent's CurrentLocation so that its position relative to that Listener is the same as its real position is relative to the closest Listener
	Listener = &InListeners( 0 );

	INT ClosestListenerIndex = 0;
	FVector ClosestLocation;
	{
		SCOPE_CYCLE_COUNTER( STAT_AudioFindNearestLocation );
		ClosestLocation = FindClosestLocation(InListeners, ClosestListenerIndex);
	}

	const FListener* ClosestListener = &InListeners( ClosestListenerIndex );

	//@note: we don't currently handle checking occlusion for sounds remapped through portals
	if( CurrentLocation == ClosestLocation )
	{
		CheckOcclusion( ClosestListener->Location );
	}

	CurrentLocation = ClosestLocation;

	// if the closest listener is not the primary one, transform CurrentLocation
	if( ClosestListener != Listener )
	{
		// get the world space delta between the sound source and the viewer
		FVector Delta = CurrentLocation - ClosestListener->Location;
		// transform Delta to the viewer's local space
		FVector ViewActorLocal = FInverseRotationMatrix( ClosestListener->Front.Rotation() ).TransformFVector( Delta );
		// transform again to a world space delta to the 'real' listener
		FVector ListenerWorld = FRotationMatrix( Listener->Front.Rotation() ).TransformFVector( ViewActorLocal );
		// add 'real' listener's location to get the final position to use
		CurrentLocation = ListenerWorld + Listener->Location;
	}

	// for vel-based effects like doppler
	CurrentVelocity = (CurrentLocation - LastLocation) / DeltaTime;

	// Default values.
	// It's all Multiplicative!  So now people are all modifying the multiplier values via various means
	// (even after the Sound has started playing, and this line takes them all into account and gives us
	// final value that is correct
	CurrentVolumeMultiplier = VolumeMultiplier * SoundCue->VolumeMultiplier * GetFadeInMultiplier() * GetFadeOutMultiplier() * GetAdjustVolumeOnFlyMultiplier() * AudioDevice->TransientMasterVolume;
	CurrentPitchMultiplier = PitchMultiplier * SoundCue->PitchMultiplier;
	CurrentHighFrequencyGainMultiplier = HighFrequencyGainMultiplier;

	// Set multipliers to allow propagation of sound class properties to wave instances.
	FSoundClassProperties* SoundClassProperties = AudioDevice->GetCurrentSoundClass( SoundCue->SoundClass );
	if( SoundClassProperties )
	{
		// Use values from "parsed/ propagated" sound class properties
		CurrentVolumeMultiplier *= SoundClassProperties->Volume * GGlobalAudioMultiplier;
		CurrentPitchMultiplier *= SoundClassProperties->Pitch;
		//TODO: Add in HighFrequencyGainMultiplier property to sound classes

		// Not all values propagate multiplicatively
		CurrentVoiceCenterChannelVolume = SoundClassProperties->VoiceCenterChannelVolume;
		CurrentRadioFilterVolume = SoundClassProperties->RadioFilterVolume * CurrentVolumeMultiplier * GGlobalAudioMultiplier;
		CurrentRadioFilterVolumeThreshold = SoundClassProperties->RadioFilterVolumeThreshold * CurrentVolumeMultiplier * GGlobalAudioMultiplier;
		StereoBleed = SoundClassProperties->StereoBleed;				// Amount to bleed stereo to the rear speakers (does not propagate to children)
		LFEBleed = SoundClassProperties->LFEBleed;						// Amount to bleed to the LFE speaker (does not propagate to children)

		bEQFilterApplied = SoundClassProperties->bApplyEffects;
		bAlwaysPlay = SoundClassProperties->bAlwaysPlay;
		bIsUISound |= SoundClassProperties->bIsUISound;					// Yes, that's |= because all children of a UI class should be UI sounds
		bIsMusic |= SoundClassProperties->bIsMusic;						// Yes, that's |= because all children of a music class should be music
		bReverb = SoundClassProperties->bReverb;						// A class with reverb applied may have children with no reverb
		bCenterChannelOnly = SoundClassProperties->bCenterChannelOnly;	// A class with bCenterChannelOnly applied may have children without this property
	}

	// Additional inside/outside processing for ambient sounds
	USoundClass* SoundClass = AudioDevice->GetSoundClass(SoundCue->SoundClass);
	if( SoundClass != NULL && SoundClass->Properties.bApplyAmbientVolumes )
	{
		HandleInteriorVolumes( AudioDevice, WorldInfo, FALSE, GetPointForDistanceEval() );
	}

	// Recurse nodes, have SoundNodeWave's create new wave instances and update bFinished unless we finished fading out.
	bFinished = TRUE;
	if( FadeOutStopTime == -1 || ( PlaybackTime <= FadeOutStopTime ) )
	{
		CueFirstNode->ParseNodes( AudioDevice, NULL, 0, this, InWaveInstances );
	}

	// Stop playback, handles bAutoDestroy in Stop.
	if( bFinished )
	{
		Stop();
	}

	LastLocation = CurrentLocation;
}

/** stops the audio (if playing), detaches the component, and resets the component's properties to the values of its template */
void UAudioComponent::ResetToDefaults( void )
{
	if( !IsTemplate() )
	{
		// make sure we're fully stopped and detached
		Stop();
		DetachFromAny();

		UAudioComponent* Default = GetArchetype<UAudioComponent>();

		// copy all non-native, non-duplicatetransient, non-Component properties we have from all classes up to and including UActorComponent
		for( UProperty* Property = GetClass()->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext )
		{
			if( !( Property->PropertyFlags & CPF_Native ) && !( Property->PropertyFlags & CPF_DuplicateTransient ) && !( Property->PropertyFlags & CPF_Component ) &&
				Property->GetOwnerClass()->IsChildOf( UActorComponent::StaticClass() ) )
			{
				Property->CopyCompleteValue( ( BYTE* )this + Property->Offset, ( BYTE* )Default + Property->Offset, NULL, this );
			}
		}
	}
}

void UAudioComponent::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << CueFirstNode;
		Ar << WaveInstances;
		Ar << SoundNodeOffsetMap;
		Ar << SoundNodeResetWaveMap;
		Ar << CurrentNotifyBufferFinishedHook;
	}
}

void UAudioComponent::SetFloatParameter( FName InName, FLOAT InFloat )
{
	if( InName != NAME_None )
	{
		// First see if an entry for this name already exists
		for( INT i = 0; i < InstanceParameters.Num(); i++ )
		{
			FAudioComponentParam& P = InstanceParameters( i );
			if( P.ParamName == InName )
			{
				P.FloatParam = InFloat;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const INT NewParamIndex = InstanceParameters.AddZeroed();
		InstanceParameters( NewParamIndex ).ParamName = InName;
		InstanceParameters( NewParamIndex ).FloatParam = InFloat;
	}
}

void UAudioComponent::SetWaveParameter( FName InName, USoundNodeWave* InWave )
{
	if( InName != NAME_None )
	{
		// First see if an entry for this name already exists
		for( INT i = 0; i < InstanceParameters.Num(); i++ )
		{
			FAudioComponentParam& P = InstanceParameters( i );
			if( P.ParamName == InName )
			{
				P.WaveParam = InWave;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const INT NewParamIndex = InstanceParameters.AddZeroed();
		InstanceParameters( NewParamIndex ).ParamName = InName;
		InstanceParameters( NewParamIndex ).WaveParam = InWave;
	}
}

/**
 *	Try and find an Instance Parameter with the given name in this AudioComponent, and if we find it return the float value.
 *	If we fail to find the parameter, return false.
 */
UBOOL UAudioComponent::GetFloatParameter( FName InName, FLOAT& OutFloat )
{
	// Always fail if we pass in no name.
	if( InName != NAME_None )
	{
		for( INT i = 0; i < InstanceParameters.Num(); i++ )
		{
			const FAudioComponentParam& P = InstanceParameters( i );
			if( P.ParamName == InName )
			{
				OutFloat = P.FloatParam;
				return( TRUE );
			}
		}
	}

	return( FALSE );
}

/**
 *	Try and find an Instance Parameter with the given name in this AudioComponent, and if we find it return the USoundNodeWave value.
 *	If we fail to find the parameter, return false.
 */
UBOOL UAudioComponent::GetWaveParameter( FName InName, USoundNodeWave*& OutWave )
{
	// Always fail if we pass in no name.
	if( InName != NAME_None )
	{
		for( INT i = 0; i < InstanceParameters.Num(); i++ )
		{
			const FAudioComponentParam& P = InstanceParameters( i );
			if( P.ParamName == InName )
			{
				OutWave = P.WaveParam;
				return( TRUE );
			}
		}
	}

	return( FALSE );
}

/**
 * This is called in place of "play".  So you will say AudioComponent->FadeIn().
 * This is useful for fading in music or some constant playing sound.
 *
 * If FadeTime is 0.0, this is the same as calling Play() but just modifying the volume by
 * FadeVolumeLevel. (e.g. you will play instantly but the FadeVolumeLevel will affect the AudioComponent
 *
 * If FadeTime is > 0.0, this will call Play(), and then increase the volume level of this
 * AudioCompoenent to the passed in FadeVolumeLevel over FadeInTime seconds.
 *
 * The VolumeLevel is MODIFYING the AudioComponent's "base" volume.  (e.g.  if you have an
 * AudioComponent that is volume 1000 and you pass in .5 as your VolumeLevel then you will fade to 500 )
 *
 * @param FadeInDuration how long it should take to reach the FadeVolumeLevel
 * @param FadeVolumeLevel the percentage of the AudioComponents's calculated volume in which to fade to
 **/
void UAudioComponent::FadeIn( FLOAT FadeInDuration, FLOAT FadeVolumeLevel )
{
	if(PlaybackTime >= FadeOutStopTime)
	{
		if( FadeInDuration >= 0.0f )
		{
			FadeInStartTime = PlaybackTime;
			FadeInStopTime = FadeInStartTime + FadeInDuration;
			FadeInTargetVolume = FadeVolumeLevel;
		}

		// @todo msew:  should this restarting playing?  No probably not.  It should AdjustVolume to the FadeVolumeLevel
		Play();
	}
	else
	{
		//warnf( TEXT( "no need to Play() here, since it's already playing %s %f %f"), *SoundCue->GetName(), PlaybackTime, FadeOutStopTime );
		// there's a fadeout active, cancel it and set up the fadeout to start where the fadeout left off
		if( FadeInDuration >= 0.0f )
		{
			// set up the fade in to be seamless by backing up the FadeInStartTime such that the result volume is the same
			FadeInStartTime = PlaybackTime - FadeInDuration * GetFadeOutMultiplier();
			FadeInStopTime = FadeInStartTime + FadeInDuration;
			FadeInTargetVolume = FadeVolumeLevel;
		}

		// stop the fadeout
		FadeOutStartTime = 0.0f;
		FadeOutStopTime = -1.0f;
		FadeOutTargetVolume = 1.0f;

		// no need to Play() here, since it's already playing
	}

}

/**
 * This is called in place of "stop".  So you will say AudioComponent->FadeOut().
 * This is useful for fading out music or some constant playing sound.
 *
 * If FadeTime is 0.0, this is the same as calling Stop().
 *
 * If FadeTime is > 0.0, this will decrease the volume level of this
 * AudioCompoenent to the passed in FadeVolumeLevel over FadeInTime seconds.
 *
 * The VolumeLevel is MODIFYING the AudioComponent's "base" volume.  (e.g.  if you have an
 * AudioComponent that is volume 1000 and you pass in .5 as your VolumeLevel then you will fade to 500 )
 *
 * @param FadeOutDuration how long it should take to reach the FadeVolumeLevel
 * @param FadeVolumeLevel the percentage of the AudioComponents's calculated volume in which to fade to
 **/
void UAudioComponent::FadeOut( FLOAT FadeOutDuration, FLOAT FadeVolumeLevel )
{
	if(PlaybackTime >= FadeInStopTime)
	{
		if( FadeOutDuration >= 0.0f )
		{
			FadeOutStartTime = PlaybackTime;
			FadeOutStopTime = FadeOutStartTime + FadeOutDuration;
			FadeOutTargetVolume = FadeVolumeLevel;
		}
		else
		{
			Stop();
		}
	}
	else
	{
		// there's a fadein active, cancel it and set up the fadeout to start where the fadein left off
		if( FadeOutDuration >= 0.0f )
		{
			// set up the fade in to be seamless by backing up the FadeInStartTime such that the result volume is the same
			FadeOutStartTime = PlaybackTime - FadeOutDuration * (1.f - GetFadeInMultiplier());
			FadeOutStopTime = FadeOutStartTime + FadeOutDuration;
			FadeOutTargetVolume = FadeVolumeLevel;
		}
		else
		{
			Stop();
		}

		// stop the fadein
		FadeInStartTime = 0.0f;
		FadeInStopTime = -1.0f;
		FadeInTargetVolume = 1.0f;
	}
}

/** @return TRUE if this component is currently fading in. */
UBOOL UAudioComponent::IsFadingIn()
{
	if (FadeInStartTime > 0.0f && PlaybackTime < FadeInStopTime)
	{
		return TRUE;
	}

	return FALSE;
}

/** @return TRUE if this component is currently fading out. */
UBOOL UAudioComponent::IsFadingOut()
{
	if (FadeOutStartTime > 0.0f && PlaybackTime < FadeOutStopTime)
	{
		return TRUE;
	}

	return FALSE;
}

/**
 * This will allow one to adjust the volume of an AudioComponent on the fly
 **/
void UAudioComponent::AdjustVolume( FLOAT AdjustVolumeDuration, FLOAT AdjustVolumeLevel )
{
	if( AdjustVolumeDuration >= 0.0f )
	{
		AdjustVolumeStartTime = PlaybackTime;
		AdjustVolumeStopTime = AdjustVolumeStartTime + AdjustVolumeDuration;
		AdjustVolumeTargetVolume = AdjustVolumeLevel;
	}
}

/** Helper function to do determine the fade volume value based on start, stop, target volume levels **/
FLOAT UAudioComponent::FadeMultiplierHelper( FLOAT FadeStartTime, FLOAT FadeStopTime, FLOAT FadeTargetValue ) const
{
	// we calculate the total amount of the duration used up and then use that percentage of the TargetVol
	const FLOAT PercentOfVolumeToUse = ( PlaybackTime - FadeStartTime ) / ( FadeStopTime - FadeStartTime );

	// If the clamp fires, the source data is incorrect.
	FLOAT Retval = Clamp<FLOAT>( PercentOfVolumeToUse * FadeTargetValue, 0.0f, 1.0f );

	return Retval;
}

FLOAT UAudioComponent::GetFadeInMultiplier( void ) const
{
	FLOAT Retval = 1.0f;

	// keep stepping towards our target until we hit our stop time
	if( PlaybackTime <= FadeInStopTime )
	{
		Retval = FadeMultiplierHelper( FadeInStartTime, FadeInStopTime, FadeInTargetVolume );
	}
	else if( PlaybackTime > FadeInStopTime )
	{
		Retval = FadeInTargetVolume;
	}

	return( Retval );
}


FLOAT UAudioComponent::GetFadeOutMultiplier( void ) const
{
	FLOAT Retval = 1.0f;

	// keep stepping towards our target until we hit our stop time
	if( PlaybackTime <= FadeOutStopTime )
	{
		// deal with people wanting to crescendo fade out!
		FLOAT VolAmt = 1.0f;
		if( FadeOutTargetVolume < 1.0f )
		{
			VolAmt = FadeMultiplierHelper( FadeOutStartTime, FadeOutStopTime, ( 1.0f - FadeOutTargetVolume ) );
			Retval = 1.0f - VolAmt; // in FadeOut() we check for negative values
		}
		else if( FadeOutTargetVolume > 1.0f )
		{
			VolAmt = FadeMultiplierHelper( FadeOutStartTime, FadeOutStopTime, ( FadeOutTargetVolume  - 1.0f ) );
			Retval = 1.0f + VolAmt;
		}
	}
	else if( PlaybackTime > FadeOutStopTime )
	{
		Retval =  FadeOutTargetVolume;
	}

	return( Retval );
}

FLOAT UAudioComponent::GetAdjustVolumeOnFlyMultiplier( void )
{
	FLOAT Retval = 1.0f;

	// keep stepping towards our target until we hit our stop time
	if( PlaybackTime <= AdjustVolumeStopTime )
	{
		// deal with people wanting to crescendo fade out!
		FLOAT VolAmt = 1.0f;
		if( AdjustVolumeTargetVolume < CurrAdjustVolumeTargetVolume )
		{
			VolAmt = FadeMultiplierHelper( AdjustVolumeStartTime, AdjustVolumeStopTime, ( CurrAdjustVolumeTargetVolume - AdjustVolumeTargetVolume ) );
			Retval = CurrAdjustVolumeTargetVolume - VolAmt;
		}
		else if( AdjustVolumeTargetVolume > CurrAdjustVolumeTargetVolume )
		{
			VolAmt = FadeMultiplierHelper( AdjustVolumeStartTime, AdjustVolumeStopTime, ( AdjustVolumeTargetVolume - CurrAdjustVolumeTargetVolume ) );
			Retval = CurrAdjustVolumeTargetVolume + VolAmt;
		}
		else
		{
			Retval = CurrAdjustVolumeTargetVolume;
		}

		//debugf( TEXT( "VolAmt: %f CurrentVolume: %f Retval: %f" ), VolAmt, CurrentVolume, Retval );
	}
	else if( PlaybackTime > AdjustVolumeStopTime )
	{
		CurrAdjustVolumeTargetVolume = AdjustVolumeTargetVolume;
		Retval = AdjustVolumeTargetVolume;
	}

	return( Retval );
}

//
// Script interface
//

void UAudioComponent::execPlay( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	Play();
}
IMPLEMENT_FUNCTION( UAudioComponent, INDEX_NONE, execPlay );

void UAudioComponent::execStop( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	Stop();
}
IMPLEMENT_FUNCTION( UAudioComponent, INDEX_NONE, execStop );

void UAudioComponent::execIsPlaying( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*( UBOOL* )Result = IsPlaying();
}
IMPLEMENT_FUNCTION( UAudioComponent, INDEX_NONE, execIsPlaying );

void UAudioComponent::execIsFadingIn( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*( UBOOL* )Result = IsFadingIn();
}
IMPLEMENT_FUNCTION( UAudioComponent, INDEX_NONE, execIsFadingIn );

void UAudioComponent::execIsFadingOut( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*( UBOOL* )Result = IsFadingOut();
}
IMPLEMENT_FUNCTION( UAudioComponent, INDEX_NONE, execIsFadingOut );

void UAudioComponent::execSetFloatParameter( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME( InName );
	P_GET_FLOAT( InFloat );
	P_FINISH;

	SetFloatParameter( InName, InFloat );
}
IMPLEMENT_FUNCTION( UAudioComponent, INDEX_NONE, execSetFloatParameter );

void UAudioComponent::execSetWaveParameter( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME( InName );
	P_GET_OBJECT( USoundNodeWave, InWave );
	P_FINISH;

	SetWaveParameter( InName, InWave );
}
IMPLEMENT_FUNCTION( UAudioComponent, INDEX_NONE, execSetWaveParameter );

void UAudioComponent::execFadeIn( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT( FadeInDuration );
	P_GET_FLOAT( FadeVolumeLevel );
	P_FINISH;

	FadeIn( FadeInDuration, FadeVolumeLevel );
}
IMPLEMENT_FUNCTION( UAudioComponent, INDEX_NONE, execFadeIn );

void UAudioComponent::execFadeOut( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT( FadeOutDuration );
	P_GET_FLOAT( FadeVolumeLevel );
	P_FINISH;

	FadeOut( FadeOutDuration, FadeVolumeLevel );
}
IMPLEMENT_FUNCTION( UAudioComponent, INDEX_NONE, execFadeOut );

void UAudioComponent::execAdjustVolume( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT( AdjustVolumeDuration );
	P_GET_FLOAT( AdjustVolumeLevel );
	P_FINISH;

	AdjustVolume( AdjustVolumeDuration, AdjustVolumeLevel );
}
IMPLEMENT_FUNCTION( UAudioComponent, INDEX_NONE, execAdjustVolume );

/*-----------------------------------------------------------------------------
	AAmbientSound implementation.
-----------------------------------------------------------------------------*/

#if WITH_EDITOR
void AAmbientSound::CheckForErrors( void )
{
	Super::CheckForErrors();

	if( AudioComponent == NULL )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_AudioComponentNull" ), *GetName() ) ), TEXT( "AudioComponentNull" ) );
	}
	else if( AudioComponent->SoundCue == NULL )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_SoundCueNull" ), *GetName() ) ), TEXT( "SoundCueNull" ) );
	}
}
#endif

/**
 * Starts audio playback if wanted.
 */
void AAmbientSound::UpdateComponentsInternal( UBOOL bCollisionUpdate )
{
	Super::UpdateComponentsInternal( bCollisionUpdate );

	if( bAutoPlay && !ShouldIgnoreAutoPlay() && AudioComponent && !AudioComponent->bWasPlaying )
	{
		AudioComponent->Play();
		bIsPlaying = TRUE;
	}
}

/**
 * Check whether the auto-play setting should be ignored, even if enabled. By default,
 * this returns TRUE if the Ambient Sound's level is hidden in the editor, though subclasses may implement custom behavior.
 *
 * @return TRUE if the auto-play setting should be ignored, FALSE otherwise
 */
UBOOL AAmbientSound::ShouldIgnoreAutoPlay() const
{
	return bHiddenEdLevel;
}

/*-----------------------------------------------------------------------------
	AAmbientSoundNonLoop implementation.
-----------------------------------------------------------------------------*/
#if WITH_EDITOR
void AAmbientSoundNonLoop::CheckForErrors( void )
{
	Super::CheckForErrors();
	if( SoundNodeInstance ) // GWarn'd by AAmbientSoundSimple::CheckForErrors().
	{
		FString OwnerName = Owner ? Owner->GetName() : TEXT( "" );
		USoundNodeAmbientNonLoop* SoundNodeNonLoop = Cast<USoundNodeAmbientNonLoop>( SoundNodeInstance );

		if( !SoundNodeNonLoop )
		{
			// Warn that the SoundNodeInstance is not of the expected type (USoundNodeAmbientNonLoop).
			GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NonAmbientSoundNonLoopInstance" ), *GetName(), *OwnerName ) ), TEXT( "NonAmbientSoundNonLoopInstance" ) );
		}
		else
		{
			// Search sound slots for at least one wave.
			UBOOL bFoundNonNULLWave = FALSE;

			for( INT SlotIndex = 0; SlotIndex < SoundNodeNonLoop->SoundSlots.Num(); ++SlotIndex )
			{
				if( SoundNodeNonLoop->SoundSlots( SlotIndex ).Wave != NULL )
				{
					bFoundNonNULLWave = TRUE;
					break;
				}
			}

			// Warn if no wave was found.
			if( !bFoundNonNULLWave )
			{
				GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NoSlotsWithWave" ), *GetName(), *OwnerName ) ), TEXT( "NoSlotsWithWave" ) );
			}
		}
	}
}
#endif


/*-----------------------------------------------------------------------------
	AAmbientSoundSimple implementation.
-----------------------------------------------------------------------------*/

/**
 * Helper function used to sync up instantiated objects.
 */
void AAmbientSoundSimple::SyncUpInstantiatedObjects( void )
{
	if( AudioComponent )
	{
		// Propagate instanced objects.
		SoundCueInstance->FirstNode	= SoundNodeInstance;
		AudioComponent->SoundCue = SoundCueInstance;
		AmbientProperties = SoundNodeInstance;

		// Make sure neither sound cue nor node are shareable or show up in generic browser.
		check( SoundNodeInstance );
		check( SoundCueInstance );
		SoundNodeInstance->ClearFlags( RF_Public );
		SoundCueInstance->ClearFlags( RF_Public );
	}
}

/**
 * Called from within SpawnActor, calling SyncUpInstantiatedObjects.
 */
void AAmbientSoundSimple::Spawned( void )
{
	Super::Spawned();
	SyncUpInstantiatedObjects();
}

/**
 * UObject serialization override
 *
 * @param Ar The archive to serialize with
 */
void AAmbientSoundSimple::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	if(NULL != SoundCueInstance)
	{
		// Force sound class to fix up serialisation errors
		SoundCueInstance->SoundClass = FName( TEXT( "Ambient" ) );
	}
}

/**
 * Called when after .t3d import of this actor (paste, duplicate or .t3d import),
 * calling SyncUpInstantiatedObjects.
 */
void AAmbientSoundSimple::PostEditImport( void )
{
	Super::PostEditImport();
	SyncUpInstantiatedObjects();
}

/**
 * Used to temporarily clear references for duplication.
 *
 * @param PropertyThatWillChange	property that will change
 */
void AAmbientSoundSimple::PreEditChange( UProperty* PropertyThatWillChange )
{
	Super::PreEditChange( PropertyThatWillChange );
	if( AudioComponent )
	{
		// Clear references for duplication/ import.
		AudioComponent->SoundCue = NULL;
		AmbientProperties = NULL;
	}
}

/**
 * Used to reset audio component when AmbientProperties change
 *
 * @param PropertyThatChanged	property that changed
 */
void AAmbientSoundSimple::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if( AudioComponent )
	{
		// Sync up properties.
		SyncUpInstantiatedObjects();

		// Reset audio component.
		if( AudioComponent->bWasPlaying )
		{
			AudioComponent->Play();
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

static void ApplyScaleToFloat( FLOAT& Dst, const FVector& DeltaScale )
{
	const FLOAT Multiplier = ( DeltaScale.X > 0.0f || DeltaScale.Y > 0.0f || DeltaScale.Z > 0.0f ) ? 1.0f : -1.0f;
	Dst += Multiplier * DeltaScale.Size();
	Dst = Max( 0.0f, Dst );
}

#if WITH_EDITOR
void AAmbientSoundSimple::EditorApplyScale( const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown )
{
	const FVector ModifiedScale = DeltaScale * 500.0f;

	if( AmbientProperties )
	{
		if( bCtrlDown )
		{
			ApplyScaleToFloat( AmbientProperties->RadiusMin, ModifiedScale );
		}
		else
		{
			ApplyScaleToFloat( AmbientProperties->RadiusMax, ModifiedScale );
		}

		PostEditChange();
	}
}

void AAmbientSoundSimple::CheckForErrors( void )
{
	Super::CheckForErrors();

	FString OwnerName = Owner ? Owner->GetName() : TEXT("");
	if( AmbientProperties == NULL )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NullAmbientProperties" ), *GetName(), *OwnerName ) ), TEXT( "NullAmbientProperties" ) );
	}
	else if( !AmbientProperties->SoundSlots.Num() )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NoSounds" ), *GetName(), *OwnerName ) ), TEXT( "NoSounds" ) );
	}

	if( SoundNodeInstance == NULL )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NullSoundNodeInstance" ), *GetName(), *OwnerName ) ), TEXT( "NullSoundNodeInstance" ) );
	}
}
#endif

/*-----------------------------------------------------------------------------
	AmbientSoundSimpleToggleable implementation.
-----------------------------------------------------------------------------*/

/**
 * Check whether the auto-play setting should be ignored, even if enabled. If a sound has been
 * explicitly toggled off via Kismet action, autoplay should be ignored from that point on.
 *
 * @return TRUE if the auto-play setting should be ignored, FALSE otherwise
 */
UBOOL AAmbientSoundSimpleToggleable::ShouldIgnoreAutoPlay() const
{
	return bIgnoreAutoPlay ? TRUE : Super::ShouldIgnoreAutoPlay();
}

/*-----------------------------------------------------------------------------
	WaveModInfo implementation - downsampling of wave files.
-----------------------------------------------------------------------------*/

//  Macros to convert 4 bytes to a Riff-style ID DWORD.
//  Todo: make these endian independent !!!

#undef MAKEFOURCC

#define MAKEFOURCC(ch0, ch1, ch2, ch3)\
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |\
    ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))

#define mmioFOURCC(ch0, ch1, ch2, ch3)\
    MAKEFOURCC(ch0, ch1, ch2, ch3)

// Main Riff-Wave header.
struct FRiffWaveHeader
{
	DWORD	rID;			// Contains 'RIFF'
	DWORD	ChunkLen;		// Remaining length of the entire riff chunk (= file).
	DWORD	wID;			// Form type. Contains 'WAVE' for .wav files.
};

// General chunk header format.
struct FRiffChunkOld
{
	DWORD	ChunkID;		  // General data chunk ID like 'data', or 'fmt '
	DWORD	ChunkLen;		  // Length of the rest of this chunk in bytes.
};

// ChunkID: 'fmt ' ("WaveFormatEx" structure )
struct FFormatChunk
{
    WORD   wFormatTag;        // Format type: 1 = PCM
    WORD   nChannels;         // Number of channels (i.e. mono, stereo...).
    DWORD   nSamplesPerSec;    // Sample rate. 44100 or 22050 or 11025  Hz.
    DWORD   nAvgBytesPerSec;   // For buffer estimation  = sample rate * BlockAlign.
    WORD   nBlockAlign;       // Block size of data = Channels times BYTES per sample.
    WORD   wBitsPerSample;    // Number of bits per sample of mono data.
    WORD   cbSize;            // The count in bytes of the size of extra information (after cbSize).
};

// ChunkID: 'smpl'
struct FSampleChunk
{
	DWORD   dwManufacturer;
	DWORD   dwProduct;
	DWORD   dwSamplePeriod;
	DWORD   dwMIDIUnityNote;
	DWORD   dwMIDIPitchFraction;
	DWORD	dwSMPTEFormat;
	DWORD   dwSMPTEOffset;		//
	DWORD   cSampleLoops;		// Number of tSampleLoop structures following this chunk
	DWORD   cbSamplerData;		//
};

//
//	Figure out the WAVE file layout.
//
UBOOL FWaveModInfo::ReadWaveInfo( BYTE* WaveData, INT WaveDataSize )
{
	FFormatChunk* FmtChunk;
	FRiffWaveHeader* RiffHdr = ( FRiffWaveHeader* )WaveData;
	WaveDataEnd = WaveData + WaveDataSize;

	if( WaveDataSize == 0 )
	{
		return( FALSE );
	}

	// Verify we've got a real 'WAVE' header.
#if __INTEL_BYTE_ORDER__
	if( RiffHdr->wID != mmioFOURCC( 'W','A','V','E' ) )
	{
		return( FALSE );
	}
#else
	if( ( RiffHdr->wID != ( mmioFOURCC( 'W','A','V','E' ) ) ) &&
	     ( RiffHdr->wID != ( mmioFOURCC( 'E','V','A','W' ) ) ) )
	{
		return( FALSE );
	}

	UBOOL AlreadySwapped = ( RiffHdr->wID == ( mmioFOURCC('W','A','V','E' ) ) );
	if( !AlreadySwapped )
	{
		RiffHdr->rID = INTEL_ORDER32( RiffHdr->rID );
		RiffHdr->ChunkLen = INTEL_ORDER32( RiffHdr->ChunkLen );
		RiffHdr->wID = INTEL_ORDER32( RiffHdr->wID );
	}
#endif

	FRiffChunkOld* RiffChunk = ( FRiffChunkOld* )&WaveData[3 * 4];
	pMasterSize = &RiffHdr->ChunkLen;

	// Look for the 'fmt ' chunk.
	while( ( ( ( BYTE* )RiffChunk + 8 ) < WaveDataEnd ) && ( INTEL_ORDER32( RiffChunk->ChunkID ) != mmioFOURCC( 'f','m','t',' ' ) ) )
	{
		RiffChunk = ( FRiffChunkOld* )( ( BYTE* )RiffChunk + Pad16Bit( INTEL_ORDER32( RiffChunk->ChunkLen ) ) + 8 );
	}

	if( INTEL_ORDER32( RiffChunk->ChunkID ) != mmioFOURCC( 'f','m','t',' ' ) )
	{
		#if !__INTEL_BYTE_ORDER__  // swap them back just in case.
			if( !AlreadySwapped )
			{
				RiffHdr->rID = INTEL_ORDER32( RiffHdr->rID );
				RiffHdr->ChunkLen = INTEL_ORDER32( RiffHdr->ChunkLen );
				RiffHdr->wID = INTEL_ORDER32( RiffHdr->wID );
            }
		#endif
		return( FALSE );
	}

	FmtChunk = ( FFormatChunk* )( ( BYTE* )RiffChunk + 8 );
#if !__INTEL_BYTE_ORDER__
	if( !AlreadySwapped )
	{
		FmtChunk->wFormatTag = INTEL_ORDER16( FmtChunk->wFormatTag );
		FmtChunk->nChannels = INTEL_ORDER16( FmtChunk->nChannels );
		FmtChunk->nSamplesPerSec = INTEL_ORDER32( FmtChunk->nSamplesPerSec );
		FmtChunk->nAvgBytesPerSec = INTEL_ORDER32( FmtChunk->nAvgBytesPerSec );
		FmtChunk->nBlockAlign = INTEL_ORDER16( FmtChunk->nBlockAlign );
		FmtChunk->wBitsPerSample = INTEL_ORDER16( FmtChunk->wBitsPerSample );
	}
#endif
	pBitsPerSample = &FmtChunk->wBitsPerSample;
	pSamplesPerSec = &FmtChunk->nSamplesPerSec;
	pAvgBytesPerSec = &FmtChunk->nAvgBytesPerSec;
	pBlockAlign = &FmtChunk->nBlockAlign;
	pChannels = &FmtChunk->nChannels;
	pFormatTag = &FmtChunk->wFormatTag;

	// re-initalize the RiffChunk pointer
	RiffChunk = ( FRiffChunkOld* )&WaveData[3 * 4];

	// Look for the 'data' chunk.
	while( ( ( ( BYTE* )RiffChunk + 8 ) < WaveDataEnd ) && ( INTEL_ORDER32( RiffChunk->ChunkID ) != mmioFOURCC( 'd','a','t','a' ) ) )
	{
		RiffChunk = ( FRiffChunkOld* )( ( BYTE* )RiffChunk + Pad16Bit( INTEL_ORDER32( RiffChunk->ChunkLen ) ) + 8 );
	}

	if( INTEL_ORDER32( RiffChunk->ChunkID ) != mmioFOURCC( 'd','a','t','a' ) )
	{
		#if !__INTEL_BYTE_ORDER__  // swap them back just in case.
			if( !AlreadySwapped )
			{
				RiffHdr->rID = INTEL_ORDER32( RiffHdr->rID );
				RiffHdr->ChunkLen = INTEL_ORDER32( RiffHdr->ChunkLen );
				RiffHdr->wID = INTEL_ORDER32( RiffHdr->wID );
				FmtChunk->wFormatTag = INTEL_ORDER16( FmtChunk->wFormatTag );
				FmtChunk->nChannels = INTEL_ORDER16( FmtChunk->nChannels );
				FmtChunk->nSamplesPerSec = INTEL_ORDER32( FmtChunk->nSamplesPerSec );
				FmtChunk->nAvgBytesPerSec = INTEL_ORDER32( FmtChunk->nAvgBytesPerSec );
				FmtChunk->nBlockAlign = INTEL_ORDER16( FmtChunk->nBlockAlign );
				FmtChunk->wBitsPerSample = INTEL_ORDER16( FmtChunk->wBitsPerSample );
			}
		#endif
		return( FALSE );
	}

#if !__INTEL_BYTE_ORDER__  // swap them back just in case.
	if( AlreadySwapped ) // swap back into Intel order for chunk search...
	{
		RiffChunk->ChunkLen = INTEL_ORDER32( RiffChunk->ChunkLen );
	}
#endif

	SampleDataStart = ( BYTE* )RiffChunk + 8;
	pWaveDataSize = &RiffChunk->ChunkLen;
	SampleDataSize = INTEL_ORDER32( RiffChunk->ChunkLen );
	OldBitsPerSample = FmtChunk->wBitsPerSample;
	SampleDataEnd = SampleDataStart + SampleDataSize;

	if( ( BYTE* )SampleDataEnd > ( BYTE* )WaveDataEnd )
	{
		debugf( NAME_Warning, TEXT( "Wave data chunk is too big!" ) );

		// Fix it up by clamping data chunk.
		SampleDataEnd = ( BYTE* )WaveDataEnd;
		SampleDataSize = SampleDataEnd - SampleDataStart;
		RiffChunk->ChunkLen = INTEL_ORDER32( SampleDataSize );
	}

	NewDataSize = SampleDataSize;

	if( FmtChunk->wFormatTag != FORMAT_LPCM && FmtChunk->wFormatTag != FORMAT_ADPCM && FmtChunk->wFormatTag != 17 )
	{
		return( FALSE );
	}

	// PS3 expects the data to be in Intel order, but not the header values
#if !__INTEL_BYTE_ORDER__ && !PS3
	if( !AlreadySwapped )
	{
		if( FmtChunk->wBitsPerSample == 16 )
		{
			for( WORD* i = ( WORD* )SampleDataStart; i < ( WORD* )SampleDataEnd; i++ )
			{
				*i = INTEL_ORDER16( *i );
			}
		}
		else if( FmtChunk->wBitsPerSample == 32 )
		{
			for( DWORD* i = ( DWORD* )SampleDataStart; i < ( DWORD* )SampleDataEnd; i++ )
			{
				*i = INTEL_ORDER32( *i );
			}
		}
	}
#endif

	// Couldn't byte swap this before, since it'd throw off the chunk search.
#if !__INTEL_BYTE_ORDER__
	*pWaveDataSize = INTEL_ORDER32( *pWaveDataSize );
#endif

	return( TRUE );
}

//
//	Validates wave format with detailed feedback
//
UBOOL FWaveModInfo::ValidateWaveInfo( BYTE* WaveData, INT WaveDataSize, const TCHAR* FileName, FFeedbackContext* Warn )
{
	FRiffWaveHeader* RiffHdr = ( FRiffWaveHeader* )WaveData;
	BYTE* ValidatedWaveDataEnd = WaveData + WaveDataSize;

	if( WaveDataSize == 0 )
	{
		Warn->Logf( NAME_Error, TEXT( "Bad wave file '%s': data size is 0." ), FileName );
		return( FALSE );
	}

	// Verify we've got a real 'WAVE' header.
#if __INTEL_BYTE_ORDER__
	if( RiffHdr->wID != mmioFOURCC( 'W','A','V','E' ) )
	{
		Warn->Logf( NAME_Error, TEXT( "Bad wave file '%s': unrecognized file format." ), FileName );
		return( FALSE );
	}
#else
	if( ( RiffHdr->wID != ( mmioFOURCC( 'W','A','V','E' ) ) ) &&
	     ( RiffHdr->wID != ( mmioFOURCC( 'E','V','A','W' ) ) ) )
	{
		Warn->Logf( NAME_Error, TEXT( "Bad wave file '%s': unrecognized file format." ), FileName );
		return( FALSE );
	}

	UBOOL AlreadySwapped = ( RiffHdr->wID == ( mmioFOURCC('W','A','V','E' ) ) );
#endif

	FRiffChunkOld* RiffChunk = ( FRiffChunkOld* )&WaveData[3 * 4];
	FRiffChunkOld* DataChunkToValidate = NULL;
	FRiffChunkOld* FmtChunkToValidate = NULL;

	// Check chunk alignment and collect relevant chunks
	UBOOL OffsetError = FALSE;
	while( ( ( BYTE* )RiffChunk + 8 ) < ValidatedWaveDataEnd )
	{
		DWORD ChunkId = INTEL_ORDER32( RiffChunk->ChunkID );
		if( ChunkId == mmioFOURCC( 'd','a','t','a' ) )
		{
			DataChunkToValidate = RiffChunk;
		}
		else if( ChunkId == mmioFOURCC( 'f','m','t',' ' ) )
		{
			FmtChunkToValidate = RiffChunk;
		}	

		// Continue scanning...
		RiffChunk = ( FRiffChunkOld* )( ( BYTE* )RiffChunk + Pad16Bit( INTEL_ORDER32( RiffChunk->ChunkLen ) ) + 8 );
	}

	if( DataChunkToValidate == NULL )
	{
		// Bad format / alignment
		Warn->Logf( NAME_Error, TEXT( "Bad wave file '%s': unable to find data chunk (probably due to bad chunk alignment)." ), FileName );
		return( FALSE );
	}
	else if( FmtChunkToValidate == NULL )
	{
		// Bad format / alignment
		Warn->Logf( NAME_Error, TEXT( "Bad wave file '%s': unable to find format chunk (probably due to bad chunk alignment)." ), FileName );
	}
	else if( ( BYTE* )RiffChunk != ValidatedWaveDataEnd )
	{
		// Warn if unknown chunks have been found
		Warn->Logf( NAME_Warning, TEXT( "Wave file '%s' appears to have misaligned chunks. Please check if your encoding tool is working properly." ), FileName );
	}

	// Verify format
	FFormatChunk FmtChunk = *( FFormatChunk* )( ( BYTE* )FmtChunkToValidate + 8 );
#if !__INTEL_BYTE_ORDER__
	if( !AlreadySwapped )
	{
		FmtChunk.wFormatTag = INTEL_ORDER16( FmtChunk.wFormatTag );
		FmtChunk.nChannels = INTEL_ORDER16( FmtChunk.nChannels );
		FmtChunk.nSamplesPerSec = INTEL_ORDER32( FmtChunk.nSamplesPerSec );
		FmtChunk.nAvgBytesPerSec = INTEL_ORDER32( FmtChunk.nAvgBytesPerSec );
		FmtChunk.nBlockAlign = INTEL_ORDER16( FmtChunk.nBlockAlign );
		FmtChunk.wBitsPerSample = INTEL_ORDER16( FmtChunk.wBitsPerSample );
	}
#endif

	if( FmtChunk.wFormatTag != FORMAT_LPCM && FmtChunk.wFormatTag != FORMAT_ADPCM && FmtChunk.wFormatTag != 17 )
	{
		Warn->Logf( NAME_Error, TEXT( "Bad wave file '%s': unsupported compression format." ), FileName );
		return( FALSE );
	}

	if( FmtChunk.wBitsPerSample != 16 )
	{
		Warn->Logf( NAME_Error, TEXT( "Currently, only 16 bit WAV files are supported (%s)." ), FileName );
		return( FALSE );
	}

	if( FmtChunk.nChannels != 1 && FmtChunk.nChannels != 2 )
	{
		Warn->Logf( NAME_Error, TEXT( "Currently, only mono or stereo WAV files are supported (%s)." ), FileName );
		return( FALSE );
	}

	return( TRUE );
}

/*-----------------------------------------------------------------------------
	UDrawSoundRadiusComponent implementation.
-----------------------------------------------------------------------------*/

/**
 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
 * @return The proxy object.
 */
FPrimitiveSceneProxy* UDrawSoundRadiusComponent::CreateSceneProxy( void )
{
	/** Represents a UDrawSoundRadiusComponent to the scene manager. */
	class FDrawSoundRadiusSceneProxy : public FPrimitiveSceneProxy
	{
	public:
		FDrawSoundRadiusSceneProxy( const UDrawSoundRadiusComponent* InComponent )
			:	FPrimitiveSceneProxy( InComponent )
			,	SphereRadius( 0.0f )
			,	SphereInnerRadius( 0.0f )
			,	bDrawWireSphere( InComponent->bDrawWireSphere )
			,	bDrawLitSphere( InComponent->bDrawLitSphere )
			,	bShouldDraw( FALSE )
			,	SphereColor( InComponent->SphereColor )
			,	SphereMaterial( InComponent->SphereMaterial )
			,	SphereSides( InComponent->SphereSides )
		{
			// in AmbientSoundSimpleSpline radius data are in component
			AAmbientSoundSimpleSpline* ASSS = Cast<AAmbientSoundSimpleSpline>( InComponent->GetOwner() );
			// .. also gets AmbientSoundNonLoop that derives from AmbientSoundSimple
			AAmbientSoundSimple* ASS = Cast<AAmbientSoundSimple>( InComponent->GetOwner() );
			// .. gets all ambient sound types
			AAmbientSound* AS = Cast<AAmbientSound>( InComponent->GetOwner() );

			if( ASSS )
			{
				USimpleSplineAudioComponent* SimpleSplineAudioComponent =  Cast<USimpleSplineAudioComponent>( ASSS->AudioComponent );
				if(SimpleSplineAudioComponent)
				{
					SphereRadius = SimpleSplineAudioComponent->RadiusMax;
					SphereInnerRadius = SimpleSplineAudioComponent->RadiusMin;
					bShouldDraw = TRUE;
				}
			}
			else if( ASS )
			{
				// Handle AmbientSoundSimple and AmbientSoundNonLoop
				if (ASS->AmbientProperties)
				{
					SphereRadius = ASS->AmbientProperties->RadiusMax;
					SphereInnerRadius = ASS->AmbientProperties->RadiusMin;
				}
				bShouldDraw = TRUE;
			}
			else
			{
				// Handle AmbientSound and AmbientSoundMovable
				TArray<USoundNodeAttenuation*> Attenuations;

				USoundCue* Cue = AS->AudioComponent->SoundCue;
				if( Cue )
				{
					Cue->RecursiveFindNode<USoundNodeAttenuation>( Cue->FirstNode, Attenuations );
					if( Attenuations.Num() > 0 )
					{
						USoundNodeAttenuation* Atten = Attenuations( 0 );
						SphereRadius = Atten->RadiusMax;
						SphereInnerRadius = Atten->RadiusMin;
						bShouldDraw = TRUE;
					}
				}
			}
		}

		/**
		 * Draw the scene proxy as a dynamic element
		 *
		 * @param	PDI - draw interface to render to
		 * @param	View - current view
		 * @param	DPGIndex - current depth priority
		 * @param	Flags - optional set of flags from EDrawDynamicElementFlags
		 */
		virtual void DrawDynamicElements( FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, DWORD Flags )
		{
			if( bDrawWireSphere )
			{
				DrawCircle( PDI, LocalToWorld.GetOrigin(), LocalToWorld.GetAxis( 0 ), LocalToWorld.GetAxis( 1 ), SphereColor, SphereRadius, SphereSides, SDPG_World );
				DrawCircle( PDI, LocalToWorld.GetOrigin(), LocalToWorld.GetAxis( 0 ), LocalToWorld.GetAxis( 2 ), SphereColor, SphereRadius, SphereSides, SDPG_World );
				DrawCircle( PDI, LocalToWorld.GetOrigin(), LocalToWorld.GetAxis( 1 ), LocalToWorld.GetAxis( 2 ), SphereColor, SphereRadius, SphereSides, SDPG_World );

				if( SphereInnerRadius > 0.0f && SphereInnerRadius < SphereRadius )
				{
					DrawCircle( PDI, LocalToWorld.GetOrigin(), LocalToWorld.GetAxis( 0 ), LocalToWorld.GetAxis( 1 ), SphereColor, SphereInnerRadius, SphereSides, SDPG_World );
					DrawCircle( PDI, LocalToWorld.GetOrigin(), LocalToWorld.GetAxis( 0 ), LocalToWorld.GetAxis( 2 ), SphereColor, SphereInnerRadius, SphereSides, SDPG_World );
					DrawCircle( PDI, LocalToWorld.GetOrigin(), LocalToWorld.GetAxis( 1 ), LocalToWorld.GetAxis( 2 ), SphereColor, SphereInnerRadius, SphereSides, SDPG_World );
				}
			}

			if( bDrawLitSphere && SphereMaterial )
			{
				DrawSphere( PDI, LocalToWorld.GetOrigin(), FVector( SphereRadius ), SphereSides, SphereSides / 2, SphereMaterial->GetRenderProxy( TRUE ), SDPG_World );

				if( SphereInnerRadius > 0.0f && SphereInnerRadius < SphereRadius )
				{
					DrawSphere( PDI, LocalToWorld.GetOrigin(), FVector( SphereInnerRadius ), SphereSides, SphereSides / 2, SphereMaterial->GetRenderProxy( TRUE ), SDPG_World );
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance( const FSceneView* View )
		{
			const UBOOL bVisible = bShouldDraw && ( View->Family->ShowFlags & SHOW_AudioRadius ) && ( bDrawWireSphere || bDrawLitSphere );
			FPrimitiveViewRelevance Result;
			Result.bDynamicRelevance = IsShown( View ) && bVisible && bSelected;
			Result.SetDPG( SDPG_World, TRUE );
			if( IsShadowCast( View ) )
			{
				Result.bShadowRelevance = TRUE;
			}
			return Result;
		}

		virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
		DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

	public:
		FLOAT				SphereRadius;
		FLOAT				SphereInnerRadius;
	private:
		BITFIELD			bDrawWireSphere;
		BITFIELD			bDrawLitSphere;
		BITFIELD			bShouldDraw;
		FColor				SphereColor;
		UMaterialInterface*	SphereMaterial;
		INT					SphereSides;
	};

	FDrawSoundRadiusSceneProxy* Proxy = NULL;
	if( Owner->IsA( AAmbientSoundSimple::StaticClass() )
		|| Owner->IsA( AAmbientSound::StaticClass() )
		|| Owner->IsA( AAmbientSoundNonLoop::StaticClass() )
		|| Owner->IsA( AAmbientSoundMovable::StaticClass() ) )
	{
		Proxy = new FDrawSoundRadiusSceneProxy( this );

		SphereRadius = Proxy->SphereRadius;
		UpdateBounds();
	}

	return Proxy;
}

void UDrawSoundRadiusComponent::UpdateBounds( void )
{
	Bounds = FBoxSphereBounds( FVector( 0, 0, 0 ), FVector( SphereRadius ), SphereRadius ).TransformBy( LocalToWorld );
}

#if WITH_TTS

/*------------------------------------------------------------------------------------
	FTTSAudioInfo.
------------------------------------------------------------------------------------*/

#if SUPPORTS_PRAGMA_PACK
#pragma pack( push, 8 )
#endif

#undef WAVE_FORMAT_1M16

#include "../../../External/DECtalk464/include/FonixTtsDtSimple.h"

#if _WINDOWS
#pragma comment( lib, "FonixTtsDtSimple.lib" )
#elif _XBOX
#pragma comment( lib, "FonixTtsDtSimple.lib" )
#elif PS3 || MOBILE || PLATFORM_MACOSX
#else
#error "Undefined platform"
#endif

#if SUPPORTS_PRAGMA_PACK
#pragma pack( pop )
#endif

/**
 * Callback from TTS processing
 */
static FTextToSpeech* CallbackState = NULL;

SWORD* FTextToSpeech::StaticCallback( SWORD* AudioData, long Flags )
{
	if( CallbackState )
	{
		CallbackState->WriteChunk( AudioData );
	}

	check(CallbackState);
	return( CallbackState->TTSBuffer );
}

/**
 * Convert UE3 language to TTS language index
 */
const TCHAR* FTextToSpeech::LanguageConvert[] =
{
	TEXT( "INT" ),		// US_English,
	TEXT( "FRA" ),		// French,
	TEXT( "DEU" ),		// German,
	TEXT( "ESN" ),		// Castilian_Spanish,
	TEXT( "XXX" ),		// Reserved,
	TEXT( "XXX" ),		// UK_English,
	TEXT( "ESM" ),		// Latin_American_Spanish,
	TEXT( "ITA" ),		// Italian
	NULL
};

/**
 * Look up the language to use for TTS
 */
INT FTextToSpeech::GetLanguageIndex( FString& Language )
{
	INT LanguageIndex = 0;
	while( LanguageConvert[LanguageIndex] )
	{
		if( !appStricmp( LanguageConvert[LanguageIndex], *Language ) )
		{
			return( LanguageIndex );
		}

		LanguageIndex++;
	}

	// None found; default to US English
	return( 0 );
}

/**
 * Initialise the TTS system
 */
void FTextToSpeech::Init( void )
{
	INT	Error;

	Error = FnxTTSDtSimpleOpen( StaticCallback, NULL );
	if( Error )
	{
		debugf( NAME_Init, TEXT( "Failed to init TTS" ) );
		return;
	}

	FString Language = appGetLanguageExt();
	INT LanguageIndex = GetLanguageIndex( Language );

	Error = FnxTTSDtSimpleSetLanguage( LanguageIndex, NULL );
	if( Error )
	{
		debugf( NAME_Init, TEXT( "Failed to set language for TTS" ) );
		return;
	}

	CurrentSpeaker = 0;
	FnxTTSDtSimpleChangeVoice( ( FnxDECtalkVoiceId )CurrentSpeaker, WAVE_FORMAT_1M16 );

	bInit = TRUE;
}

/**
 * Free up any resources used by TTS
 */
FTextToSpeech::~FTextToSpeech( void )
{
	if( bInit )
	{
		FnxTTSDtSimpleClose();
	}
}

/**
 * Write a chunk of data to a buffer
 */
void FTextToSpeech::WriteChunk( SWORD* AudioData )
{
	INT Index = PCMData.Num();

	PCMData.Add( TTS_CHUNK_SIZE );

	SWORD* Destination = &PCMData( Index );
	appMemcpy( Destination, AudioData, TTS_CHUNK_SIZE * sizeof( SWORD ) );
}

/**
 * Speak a line of text using the speaker
 */
void FTextToSpeech::CreatePCMData( USoundNodeWave* Wave )
{
	if( bInit )
	{
		if( Wave->TTSSpeaker != CurrentSpeaker )
		{
			FnxTTSDtSimpleChangeVoice( ( FnxDECtalkVoiceId )Wave->TTSSpeaker, WAVE_FORMAT_1M16 );
			CurrentSpeaker = Wave->TTSSpeaker;
		}

		// Best guess at size of generated PCM data
		PCMData.Empty( Wave->SpokenText.Len() * 1024 );

		CallbackState = this;
		FnxTTSDtSimpleStart( TCHAR_TO_ANSI( *Wave->SpokenText ), TTSBuffer, TTS_CHUNK_SIZE, WAVE_FORMAT_1M16 );
		CallbackState = NULL;

		// Copy out the generated PCM data
		Wave->RawPCMDataSize = PCMData.Num() * sizeof( SWORD );
		Wave->RawPCMData = ( BYTE* )appMalloc( Wave->RawPCMDataSize );
		appMemcpy( Wave->RawPCMData, PCMData.GetTypedData(), Wave->RawPCMDataSize );

		Wave->NumChannels = 1;
		Wave->SampleRate = 11025;
		Wave->Duration = ( FLOAT )Wave->RawPCMDataSize / ( Wave->SampleRate * sizeof( SWORD ) );
		Wave->bDynamicResource = TRUE;

		PCMData.Empty();
	}
}

/**
 * Create a new sound cue with a single programmatically generated wave
 */
USoundCue* UAudioDevice::CreateTTSSoundCue( const FString& SpokenText, ETTSSpeaker Speaker )
{
	USoundCue* SoundCueTTS = ConstructObject<USoundCue>( USoundCue::StaticClass(), GetTransientPackage() );
	if( SoundCueTTS )
	{
		USoundNodeWave* SoundNodeWaveTTS = ConstructObject<USoundNodeWave>( USoundNodeWave::StaticClass(), SoundCueTTS );
		if( SoundNodeWaveTTS )
		{
			SoundNodeWaveTTS->bUseTTS = TRUE;
			SoundNodeWaveTTS->TTSSpeaker = Speaker;
			SoundNodeWaveTTS->SpokenText = SpokenText;

			SoundCueTTS->SoundClass = NAME_VoiceChat;
			// Link the attenuation node to root.
			SoundCueTTS->FirstNode = SoundNodeWaveTTS;

			// Expand the TTS data
			debugf( NAME_DevAudio, TEXT( "Building TTS PCM data for '%s'" ), *SoundCueTTS->GetName() );
			TextToSpeech->CreatePCMData( SoundNodeWaveTTS );
		}
	}

	return( SoundCueTTS );
}

#undef WAVE_FORMAT_1M16

#endif // WITH_TTS

/*-----------------------------------------------------------------------------
	AAmbientSoundSpline implementation.
-----------------------------------------------------------------------------*/

void AAmbientSoundSpline::PostEditMove(UBOOL bFinished)
{
	Super::PostEditMove(bFinished);
#if WITH_EDITOR
	UpdateSpline();
#endif
}

/**
 *  In all AAmbientSoundSpline (and inherited classes) actors MaxRadius (in Attenuation group) must be smaller than ListenerScopeRadius. 
 *  Otherwise the ambient sound will occur rapidly with relatively loud volume. 
 *  A user should be responsible for the proper setup, but since it may be hard to track all used sound nodes, it is done automatically. 
 */
static void FixListenerScopeRadius(AAmbientSoundSpline* AmbientSoundSpline)
{
	if(NULL == AmbientSoundSpline) return;
	UAudioComponent* AudioComponent = AmbientSoundSpline->AudioComponent;
	if(NULL == AudioComponent) return;
	
	if(NULL != Cast<AAmbientSoundSimpleSpline>(AmbientSoundSpline))
	{
		USimpleSplineAudioComponent* SimpleSplineAudioComponent = Cast<USimpleSplineAudioComponent>(AudioComponent);
		if(NULL == SimpleSplineAudioComponent) return;
		SimpleSplineAudioComponent->ListenerScopeRadius = Max( SimpleSplineAudioComponent->RadiusMax, SimpleSplineAudioComponent->ListenerScopeRadius );
		return;
	}

	if(NULL != Cast<AAmbientSoundSplineMultiCue>(AmbientSoundSpline))
	{
		UMultiCueSplineAudioComponent * MultiCueSplineAudioComponent = Cast<UMultiCueSplineAudioComponent>(AudioComponent);
		if(NULL == MultiCueSplineAudioComponent) return;

		FLOAT MaxListenerScopeRadius = MultiCueSplineAudioComponent->ListenerScopeRadius;

		TArray<FMultiCueSplineSoundSlot>& SoundSlots = MultiCueSplineAudioComponent->SoundSlots;
		for(INT SlotIndex = 0; SlotIndex < SoundSlots.Num(); SlotIndex++ )
		{
			USoundCue* SoundCue = SoundSlots(SlotIndex).SoundCue;	if(NULL == SoundCue) continue;
			USoundNode* Node = SoundCue->FirstNode;					if(NULL == Node) continue;

			TArray<class USoundNodeAttenuation*> AttenuationNodes;
			SoundCue->RecursiveFindAttenuation( Node, AttenuationNodes );
			for(INT i = 0; i < AttenuationNodes.Num(); i++)
			{
				if(NULL == AttenuationNodes(i)) continue;
				MaxListenerScopeRadius = Max(MaxListenerScopeRadius, AttenuationNodes(i)->RadiusMax);
			}
		}
		MultiCueSplineAudioComponent->ListenerScopeRadius = MaxListenerScopeRadius;
		return;
	}

	{
		USplineAudioComponent* SplineAudioComponent = Cast<USplineAudioComponent>(AudioComponent);
		if(NULL == SplineAudioComponent) return;

		USoundCue* SoundCue = AudioComponent->SoundCue;		if(NULL == SoundCue) return;
		USoundNode* Node = SoundCue->FirstNode;				if(NULL == Node) return;
		
		TArray<class USoundNodeAttenuation*> AttenuationNodes;
		SoundCue->RecursiveFindAttenuation( Node, AttenuationNodes );
		FLOAT MaxListenerScopeRadius = SplineAudioComponent->ListenerScopeRadius;
		for(INT i = 0; i < AttenuationNodes.Num(); i++)
		{
			if(NULL == AttenuationNodes(i)) continue;
			MaxListenerScopeRadius = Max(MaxListenerScopeRadius, AttenuationNodes(i)->RadiusMax);
		}
		SplineAudioComponent->ListenerScopeRadius = MaxListenerScopeRadius;
	}
}

void AAmbientSoundSpline::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
#if WITH_EDITOR
	UpdateSpline();
#endif
	FixListenerScopeRadius(this);
}
void AAmbientSoundSpline::PostLoad()
{
	Super::PostLoad();
	FixListenerScopeRadius(this);
#if WITH_EDITOR
	UpdateSplineGeometry();
#endif
}

#if WITH_EDITOR
/**
 * Called by ApplyDeltaToActor to perform an actor class-specific operation based on widget manipulation.
 * The default implementation is simply to translate the actor's location.
 */
void AAmbientSoundSpline::EditorApplyTranslation(const FVector& DeltaTranslation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	Super::EditorApplyTranslation( DeltaTranslation, bAltDown, bShiftDown, bCtrlDown);

	if(NULL != SplineComponent)
	{
		for(INT i = 0; i < SplineComponent->SplineInfo.Points.Num(); ++i)
		{
			SplineComponent->SplineInfo.Points(i).OutVal += DeltaTranslation;
		}
		UpdateSplineGeometry();
	}
}

/**
 *  Recalculate spline after any control point was moved. 
 */
void AAmbientSoundSpline::UpdateSplineGeometry()
{
	if(NULL == SplineComponent)
	{
		return;
	}

		if( 0 >= SplineComponent->SplineInfo.Points.Num())
		{
		const INT NewPointIndex = SplineComponent->SplineInfo.AddPoint(0.0f, Location);
		SplineComponent->SplineInfo.Points(NewPointIndex).InterpMode = CIM_CurveAuto;
		}
		else
	{
			SplineComponent->SplineInfo.Points(0).OutVal = Location;
		}
		SplineComponent->SplineInfo.AutoSetTangents();

	USplineAudioComponent* PointSplineAudioComponent = Cast<USplineAudioComponent>(AudioComponent);
	if(NULL != PointSplineAudioComponent)
	{
		if(!PointSplineAudioComponent->SetSplineData(SplineComponent->SplineInfo, DistanceBetweenPoints))
		{
			// some message
			// warnf(TEXT("AAmbientSoundSpline::UpdateSplineGeometry: !SetSplineData"));
		}
	}
	else
	{
		warnf(TEXT("AAmbientSoundSpline::UpdateSplineGeometry: NULL == PointSplineAudioComponent || NULL == SplineComponent"));
	}
}

/**
 *  Force all spline data to be consistent.
 */
void AAmbientSoundSpline::UpdateSpline()
{
	if(NULL != SplineComponent)
	{
		FComponentReattachContext ComponentReattachContext(SplineComponent);
		UpdateSplineGeometry();
		
		SplineComponent->UpdateSplineReparamTable();			
		SplineComponent->UpdateSplineCurviness();
		SplineComponent->SetHiddenGame( bHidden );
	}
}

#endif

/*-----------------------------------------------------------------------------
	AAmbientSoundSimpleSpline implementation.
-----------------------------------------------------------------------------*/

template<class TSlot> 
void MakeSlotsValid(TArray<TSlot>& Slots, INT LastIndex)
{
	for(INT SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		TSlot& Slot = Slots(SlotIndex);
		if(Slot.EndPoint >= 0)
			Slot.EndPoint = Max(1, Min(Slot.EndPoint, LastIndex));

		if(Slot.StartPoint >= 0)
			Slot.StartPoint = Min(Slot.StartPoint, LastIndex-1);

		if(Slot.EndPoint >= 0 && Slot.StartPoint >= 0 && Slot.EndPoint < Slot.StartPoint)
		{
			const INT TempStart = Slot.StartPoint;
			Slot.StartPoint = Slot.EndPoint;
			Slot.EndPoint = TempStart;
		}

		if(Slot.EndPoint >= 0 && Slot.StartPoint >= 0 && Slot.EndPoint == Slot.StartPoint)
	{
			Slot.StartPoint = Max(Slot.StartPoint-1, 0);
			Slot.EndPoint = Min(Slot.EndPoint+1, LastIndex);
		}
	}
}

void AAmbientSoundSimpleSpline::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// validate all slots
	USimpleSplineAudioComponent* SimpleSplineAudioComponent = Cast<USimpleSplineAudioComponent>(AudioComponent);
	if(NULL == SimpleSplineAudioComponent) return;

#if WITH_EDITORONLY_DATA
	EditedSlot = Max(0, Min(SimpleSplineAudioComponent->SoundSlots.Num()-1, EditedSlot));
#endif // WITH_EDITORONLY_DATA

	const INT PointsNum = SimpleSplineAudioComponent->Points.Num();
	if(PointsNum < 3) return;

	const INT LastIndex = SimpleSplineAudioComponent->Points.Num()-1;
	MakeSlotsValid(SimpleSplineAudioComponent->SoundSlots, LastIndex);
}

template<class TSlot>
static FVector FindVirtualSpeakerScaledPositionForSlot(
	const TArray< FInterpCurveVector::FPointOnSpline >& Points, FVector Listener, FLOAT Radius, const TSlot& Slot, FLOAT& OutScaledDistance, INT& OutClosestPointOnSpline )
{
	if( 1 > Points.Num() )
	{
		OutScaledDistance = BIG_NUMBER;
		return FVector(BIG_NUMBER, BIG_NUMBER, BIG_NUMBER);
	}

	const FLOAT RadiusSq = Radius * Radius;

	FLOAT BestScaledDistance = BIG_NUMBER;
	FVector Result(0.0f, 0.0f, 0.0f);
	FLOAT WeightSum = 0.0f;
	OutClosestPointOnSpline = -1;

	for(INT i = 0; i < Points.Num(); i++)
	{
		const FVector Point = Points(i).Position;
		const FLOAT DistanceSq = Point.DistanceSquared(Listener);
		if(DistanceSq > RadiusSq) continue;
		if(i < Slot.StartPoint || (Slot.EndPoint >= 0 && i > Slot.EndPoint)) continue;

		const FLOAT Distance = appSqrt(DistanceSq);
		const FLOAT Weight = (1.0f - (Distance/Radius));

		WeightSum += Weight;
		Result += Point * Weight;
		if(BestScaledDistance > Distance)
		{
			BestScaledDistance = Distance;
			OutClosestPointOnSpline = i;
		}
	}

	if(WeightSum <= 0.0f)
	{
		OutScaledDistance = BIG_NUMBER;
		return FVector(BIG_NUMBER, BIG_NUMBER, BIG_NUMBER);
	}

	OutScaledDistance = BestScaledDistance;
	return Result / WeightSum;
}

FVector USimpleSplineAudioComponent::FindVirtualSpeakerScaledPosition(
	const TArray< FInterpCurveVector::FPointOnSpline >& Points, FVector Listener, FLOAT Radius, const FSplineSoundSlot& Slot, FLOAT& OutScaledDistance, INT& OutClosestPointOnSpline )
{
	return FindVirtualSpeakerScaledPositionForSlot(Points, Listener, Radius, Slot, OutScaledDistance, OutClosestPointOnSpline );
}

template<class TSlot>
static FVector FindLocationForAmbientSoundSimple(
	const TArray< FInterpCurveVector::FPointOnSpline >& Points, 
	FLOAT ListenerScopeRadius, 
	const TArray<struct FListener>& InListeners,
	const TSlot& Slot,
	INT& OutClosestListenerIndex,
	FLOAT& OutDistanceToAttenuationEval,
	INT& OutClosestPointOnSplineIndex)
	{
	SCOPE_CYCLE_COUNTER( STAT_AudioFindNearestLocation );
	if( Points.Num() <= 1 || ListenerScopeRadius <= 0.0f || InListeners.Num() < 1 )
	{
		return FVector::ZeroVector;
	}

	INT ClosestListenerIndex = 0;
	FLOAT BestScaledDistance;
	OutClosestPointOnSplineIndex=-1;

	FVector BestPosition = FindVirtualSpeakerScaledPositionForSlot( Points, InListeners( 0 ).Location, ListenerScopeRadius, Slot, BestScaledDistance, OutClosestPointOnSplineIndex );

	for( INT i = 1; i < InListeners.Num(); i++ )
	{
		FLOAT ScaledDistance;
		INT ClosestPointOnSpline=-1;
		const FVector VirtualSpeakerPosition = FindVirtualSpeakerScaledPositionForSlot( Points, InListeners( i ).Location, ListenerScopeRadius, Slot, ScaledDistance, ClosestPointOnSpline );
		if(ScaledDistance < BestScaledDistance)
		{
			OutClosestPointOnSplineIndex = ClosestPointOnSpline;
			BestScaledDistance = ScaledDistance;
			BestPosition = VirtualSpeakerPosition;
		}
	}
	
	OutClosestListenerIndex = ClosestListenerIndex;
	OutDistanceToAttenuationEval = BestScaledDistance;
	return BestPosition;
}

void USimpleSplineAudioComponent::HandleSoundSlot( UAudioDevice* AudioDevice, TArray<FWaveInstance*> &InWaveInstances, const TArray<struct FListener>& InListeners, FSplineSoundSlot& Slot, INT ChildIndex)
{
	if(NULL == Slot.Wave) return;

	SourceInteriorVolume = Slot.SourceInteriorVolume;
	SourceInteriorLPF = Slot.SourceInteriorLPF;
	LastUpdateTime = Slot.LastUpdateTime;
	CurrentInteriorLPF = Slot.CurrentInteriorLPF;
	CurrentInteriorVolume = Slot.CurrentInteriorVolume;

	FLOAT SavedCurrentVolumeMultiplier = CurrentVolumeMultiplier;
	FLOAT SavedCurrentHighFrequencyGainMultiplier = CurrentHighFrequencyGainMultiplier;
	FLOAT SavedCurrentVolume = CurrentVolume;
	FLOAT SavedCurrentPitch = CurrentPitch;

	//FIND LOCATION
	Listener = &InListeners( 0 );
	INT ClosestListenerIndex = 0;
	FLOAT DistanceToAttenuationEval = 0.0f;
	ClosestPointOnSplineIndex=-1;

	CurrentLocation = FindLocationForAmbientSoundSimple(Points, ListenerScopeRadius, InListeners, Slot, ClosestListenerIndex, DistanceToAttenuationEval, ClosestPointOnSplineIndex);
	const FListener* ClosestListener = &InListeners( ClosestListenerIndex );
	// CheckOcclusion( ClosestListener->Location ); - not needed for spline actors?

	// if the closest listener is not the primary one, transform CurrentLocation
	if( ClosestListener != Listener )
	{
		// get the world space delta between the sound source and the viewer
		FVector Delta = CurrentLocation - ClosestListener->Location;
		// transform Delta to the viewer's local space
		FVector ViewActorLocal = FInverseRotationMatrix( ClosestListener->Front.Rotation() ).TransformFVector( Delta );
		// transform again to a world space delta to the 'real' listener
		FVector ListenerWorld = FRotationMatrix( Listener->Front.Rotation() ).TransformFVector( ViewActorLocal );
		// add 'real' listener's location to get the final position to use
		CurrentLocation = ListenerWorld + Listener->Location;
	}

	// LOCATION DEPENDENT
	// Additional inside/outside processing for ambient sounds

	if( SoundCue->SoundClass == NAME_Ambient )
		{
		HandleInteriorVolumes( AudioDevice, GWorld->GetWorldInfo(), TRUE, GetPointForDistanceEval() );
		}

	// Recurse nodes, have SoundNodeWave's create new wave instances and update bFinished unless we finished fading out. 
	USoundNode::CalculateAttenuatedVolume( this, DistanceAlgorithm, DistanceToAttenuationEval, RadiusMin, RadiusMax, dBAttenuationAtMax );
	if(bAttenuateWithLPF)
		{
		USoundNode::CalculateLPFComponent( this, DistanceToAttenuationEval, LPFRadiusMin, LPFRadiusMax );
		}
	CurrentUseSpatialization = TRUE;
	OmniRadius = FlattenAttenuationRadius;

	//PLAY WAVE
	CurrentVolume *= Slot.VolumeScale;
	CurrentPitch *= Slot.PitchScale;
	const INT PreviousNum = InWaveInstances.Num();
	Slot.Wave->ParseNodes( AudioDevice, NULL, ChildIndex, this, InWaveInstances );

	// Mark wave instance associated with this "Wave" to loop indefinitely.
	for( INT i = PreviousNum; i < InWaveInstances.Num(); i++ )
	{
		FWaveInstance* WaveInstance = InWaveInstances( i );
		WaveInstance->LoopingMode = LOOP_Forever;
	}

	Slot.SourceInteriorVolume = SourceInteriorVolume;
	Slot.SourceInteriorLPF = SourceInteriorLPF;
	Slot.LastUpdateTime = LastUpdateTime;
	Slot.CurrentInteriorLPF = CurrentInteriorLPF;
	Slot.CurrentInteriorVolume = CurrentInteriorVolume;

	CurrentVolumeMultiplier = SavedCurrentVolumeMultiplier;
	CurrentHighFrequencyGainMultiplier = SavedCurrentHighFrequencyGainMultiplier;
	CurrentPitch = SavedCurrentPitch;
	CurrentVolume = SavedCurrentVolume;

	LastLocation = CurrentLocation;
}

void USimpleSplineAudioComponent::UpdateWaveInstances( UAudioDevice* AudioDevice, TArray<FWaveInstance*> &InWaveInstances, const TArray<struct FListener>& InListeners, FLOAT DeltaTime )
{
	//INIT
	check( AudioDevice );
	check(NULL != SoundCue );

	//@todo audio: Need to handle pausing and not getting out of sync by using the mixer's time.
	//@todo audio: Fading in and out is also dependent on the DeltaTime
	PlaybackTime += DeltaTime;

	// Reset temporary variables used for node traversal.
	FAudioComponentSavedState::Reset( this );
	CurrentNotifyBufferFinishedHook = NotifyBufferFinishedHook;

	// LOCATION INDEPENDENT
	// Default values.
	// It's all Multiplicative!  So now people are all modifying the multiplier values via various means 
	// (even after the Sound has started playing, and this line takes them all into account and gives us
	// final value that is correct
	CurrentVolumeMultiplier = VolumeMultiplier * SoundCue->VolumeMultiplier * GetFadeInMultiplier() * GetFadeOutMultiplier() * GetAdjustVolumeOnFlyMultiplier() * AudioDevice->TransientMasterVolume;
	CurrentPitchMultiplier = PitchMultiplier * SoundCue->PitchMultiplier;
	CurrentHighFrequencyGainMultiplier = HighFrequencyGainMultiplier;

	// Set multipliers to allow propagation of sound class properties to wave instances.
	FSoundClassProperties* SoundClassProperties = AudioDevice->GetCurrentSoundClass( SoundCue->SoundClass );
	if( SoundClassProperties )
{
		// Use values from "parsed/ propagated" sound class properties
		CurrentVolumeMultiplier *= SoundClassProperties->Volume * GGlobalAudioMultiplier;
		CurrentPitchMultiplier *= SoundClassProperties->Pitch;
		//TODO: Add in HighFrequencyGainMultiplier property to sound classes

		// Not all values propagate multiplicatively
		CurrentVoiceCenterChannelVolume = SoundClassProperties->VoiceCenterChannelVolume;
		CurrentRadioFilterVolume = SoundClassProperties->RadioFilterVolume * CurrentVolumeMultiplier * GGlobalAudioMultiplier;
		CurrentRadioFilterVolumeThreshold = SoundClassProperties->RadioFilterVolumeThreshold * CurrentVolumeMultiplier * GGlobalAudioMultiplier;
		StereoBleed = SoundClassProperties->StereoBleed;				// Amount to bleed stereo to the rear speakers (does not propagate to children)
		LFEBleed = SoundClassProperties->LFEBleed;						// Amount to bleed to the LFE speaker (does not propagate to children)

		bEQFilterApplied = SoundClassProperties->bApplyEffects;
		bAlwaysPlay = SoundClassProperties->bAlwaysPlay;
		bIsUISound |= SoundClassProperties->bIsUISound;					// Yes, that's |= because all children of a UI class should be UI sounds
		bIsMusic |= SoundClassProperties->bIsMusic;						// Yes, that's |= because all children of a music class should be music
		bReverb = SoundClassProperties->bReverb;						// A class with reverb applied may have children with no reverb
		bCenterChannelOnly = SoundClassProperties->bCenterChannelOnly;	// A class with bCenterChannelOnly applied may have children without this property
	}

	bFinished = TRUE;
	for( INT SlotIdx = 0; ( FadeOutStopTime == -1 || ( PlaybackTime <= FadeOutStopTime ) ) && ( SlotIdx < SoundSlots.Num() ) ; ++SlotIdx )
	{
		HandleSoundSlot( AudioDevice, InWaveInstances, InListeners, SoundSlots(SlotIdx), SlotIdx);
	}
	//OUT
	// Stop playback, handles bAutoDestroy in Stop.
	if( bFinished )
	{
		Stop();
	}
	LastLocation = CurrentLocation;
}



FVector USimpleSplineAudioComponent::FindClosestLocation( const TArray<struct FListener>& InListeners, INT& ClosestListenerIndexOut )
{
	//USimpleSplineAudioComponent does not use SoundNodes, so there is no object that can call this function
	check(false); 
	return FVector::ZeroVector;
}
	
FVector USimpleSplineAudioComponent::GetPointForDistanceEval()
{
	if(ClosestPointOnSplineIndex >= 0 && ClosestPointOnSplineIndex < Points.Num())
	{
		return Points(ClosestPointOnSplineIndex).Position;
}
	return Super::GetPointForDistanceEval();
}

void USimpleSplineAudioComponent::Cleanup( )
{
	for( INT SlotIdx = 0; ( FadeOutStopTime == -1 || ( PlaybackTime <= FadeOutStopTime ) ) && ( SlotIdx < SoundSlots.Num() ) ; ++SlotIdx )
	{
		SoundSlots(SlotIdx).LastUpdateTime = 0.0f;
	}
	Super::Cleanup();
}

/*-----------------------------------------------------------------------------
	AAmbientSoundSplineMultiCue implementation.
-----------------------------------------------------------------------------*/

void AAmbientSoundSplineMultiCue::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// validate all slots
	UMultiCueSplineAudioComponent* MultiCueSplineAudioComponent = Cast<UMultiCueSplineAudioComponent>(AudioComponent);
	if(NULL == MultiCueSplineAudioComponent) return;

#if WITH_EDITORONLY_DATA
	EditedSlot = Max(0, Min(MultiCueSplineAudioComponent->SoundSlots.Num()-1, EditedSlot));
#endif // WITH_EDITORONLY_DATA

	const INT PointsNum = MultiCueSplineAudioComponent->Points.Num();
	if(PointsNum < 3) return;

	const INT LastIndex = MultiCueSplineAudioComponent->Points.Num()-1;
	MakeSlotsValid(MultiCueSplineAudioComponent->SoundSlots, LastIndex);

	if(NULL != MultiCueSplineAudioComponent->SoundCue)
	{
		UBOOL InvalidSoundCue = TRUE;
		for( INT i = 0; i < MultiCueSplineAudioComponent->SoundSlots.Num(); ++i )
		{
			if(MultiCueSplineAudioComponent->SoundCue == MultiCueSplineAudioComponent->SoundSlots(i).SoundCue)
			{
				InvalidSoundCue = FALSE;
				break;
			}
		}
		if(InvalidSoundCue)
		{
			MultiCueSplineAudioComponent->SoundCue = NULL;
			MultiCueSplineAudioComponent->CueFirstNode = NULL;
		}
	}
}	

/*-----------------------------------------------------------------------------
	UMultiCueSplineAudioComponent implementation.
-----------------------------------------------------------------------------*/

/**
 * Dissociates component from audio device and deletes wave instances.
 */
void UMultiCueSplineAudioComponent::Cleanup( void )
{
	for( INT SlotIdx = 0; ( FadeOutStopTime == -1 || ( PlaybackTime <= FadeOutStopTime ) ) && ( SlotIdx < SoundSlots.Num() ) ; ++SlotIdx )
	{
		SoundSlots(SlotIdx).LastUpdateTime = 0.0f;
	}
	if( bWasPlaying && !GExitPurge )
	{
		// @see UAudioComponent::Stop()  we set this to null there.  so if we bypass stop and just call 
		// ::Cleanup (for things like emitters which are destroyed) we need to decrement CurrentPlayCount
		for(INT SlotIndex = 0; SlotIndex < SoundSlots.Num(); ++SlotIndex)
		{	
			FMultiCueSplineSoundSlot& Slot = SoundSlots(SlotIndex);
			if(!Slot.bPlaying || NULL == Slot.SoundCue || NULL == Slot.SoundCue->FirstNode) continue;

			// Slot.SoundCue->DecreaseConcurrentPlayCounts(Owner);
			Slot.SoundCue->CurrentPlayCount = Max( Slot.SoundCue->CurrentPlayCount - 1, 0 );
			Slot.bPlaying = FALSE;
		}

		// Removes component from the audio device's component list, implicitly also stopping the sound.
		UAudioDevice* AudioDevice = GEngine && GEngine->Client ? GEngine->Client->GetAudioDevice() : NULL;
		if( AudioDevice )
		{
			AudioDevice->RemoveComponent( this );
		}

		// Delete wave instances.
		for( INT InstanceIndex = 0; InstanceIndex < WaveInstances.Num(); InstanceIndex++ )
		{
			FWaveInstance* WaveInstance = WaveInstances( InstanceIndex );

			// Dequeue subtitles for this sounds
			FSubtitleManager::GetSubtitleManager()->KillSubtitles( ( PTRINT )WaveInstance );

			delete WaveInstance;
		}

		// Reset hook so it can be GCed if other code changes soundcue.
		CurrentNotifyBufferFinishedHook = NULL;

		// Don't clear instance parameters in the editor.
		// Necessary to support defining parameters via the property window, where PostEditChanges
		// routes component reattach, which would immediately clear the just-edited parameters.
		if( !GIsEditor || GIsPlayInEditorWorld )
		{
			InstanceParameters.Empty();
		}

		// Reset flags
		bWasOccluded = FALSE;
		bIsUISound = FALSE;

		// Force re-initialization on next play.
		SoundNodeData.Empty();
		SoundNodeOffsetMap.Empty();
		SoundNodeResetWaveMap.Empty();
		WaveInstances.Empty();

		// We cleaned up everything.
		bWasPlaying = FALSE;
	}

	PlaybackTime = 0.0f;
	LastOcclusionCheckTime = 0.0f;
	OcclusionCheckInterval = 0.0f;

	// Reset fade in variables
	FadeInStartTime = 0.0f;
	FadeInStopTime = -1.0f;
	FadeInTargetVolume = 1.0f;

	// Reset fade out variables
	FadeOutStartTime = 0.0f;	
	FadeOutStopTime = -1.0f;
	FadeOutTargetVolume = 1.0f;

	// Reset adjust variables
	AdjustVolumeStartTime = 0.0f;	
	AdjustVolumeStopTime = -1.0f;
	AdjustVolumeTargetVolume = 1.0f;
	CurrAdjustVolumeTargetVolume = 1.0f;

	LastUpdateTime = 0.0;
	SourceInteriorVolume = 1.0f;
	SourceInteriorLPF = 1.0f;
	CurrentInteriorVolume = 1.0f;
	CurrentInteriorLPF = 1.0f;

	bApplyRadioFilter = FALSE;
	bRadioFilterSelected = FALSE;
}

/** 
 * @param InListeners all listeners list
 * @param ClosestListenerIndexOut through this variable index of the closest listener is returned 
 * @return Closest RELATIVE location of sound (relative to position of the closest listener). 
 */
FVector UMultiCueSplineAudioComponent::FindClosestLocation( const TArray<struct FListener>& InListeners, INT& ClosestListenerIndexOut )
{
	//@note: UMultiCueSplineAudioComponent doesn't handle portals

	if( Points.Num() <= 1 || ListenerScopeRadius <= 0.0f || InListeners.Num() < 1 || CurrentSlotIndex < 0 || CurrentSlotIndex > SoundSlots.Num() )
	{
		return Super::FindClosestLocation(InListeners, ClosestListenerIndexOut);
	}

	FLOAT DummyDistanceToAttenuationEval;
	return FindLocationForAmbientSoundSimple(Points, ListenerScopeRadius, InListeners, SoundSlots(CurrentSlotIndex), ClosestListenerIndexOut, DummyDistanceToAttenuationEval, ClosestPointOnSplineIndex );
}

void UMultiCueSplineAudioComponent::Play( )
{
	debugfSuppressed( NAME_DevAudioVerbose, TEXT( "%g: Playing AudioComponent : '%s' with Sound Cue: '%s'" ), GWorld ? GWorld->GetAudioTimeSeconds() : 0.0f, *GetFullName(), SoundCue ? *SoundCue->GetName() : TEXT( "NULL" ) );

	INT PlayingCuesCounter = 0;
	for(INT SlotIndex = 0; SlotIndex < SoundSlots.Num(); ++SlotIndex)
	{	
		FMultiCueSplineSoundSlot& Slot = SoundSlots(SlotIndex);
		if( ( Slot.SoundCue == NULL ) || (( Slot.SoundCue->MaxConcurrentPlayCount != 0 ) && ( Slot.SoundCue->CurrentPlayCount >= Slot.SoundCue->MaxConcurrentPlayCount )) )
		{
			Slot.bPlaying = FALSE;
			continue;
		}
		Slot.bPlaying = TRUE;
		PlayingCuesCounter++;
#if !FINAL_RELEASE
		if( GShouldLogAllPlaySoundCalls == TRUE )
		{
			warnf( TEXT("%s::Play %s  Loc: %s"), *GetFullName(), *Slot.SoundCue->GetName(), *Location.ToString() );
		}
#endif
		if(NULL == CueFirstNode )
		{
			CueFirstNode = Slot.SoundCue->FirstNode;
			SoundCue = Slot.SoundCue;
		}
	}

	if(0 == PlayingCuesCounter) return;

	// Reset variables if we were already playing.
	if( bWasPlaying )
	{
		for( INT InstanceIndex = 0; InstanceIndex < WaveInstances.Num(); InstanceIndex++ )
		{
			FWaveInstance* WaveInstance = WaveInstances( InstanceIndex );
			if( WaveInstance )
			{
				WaveInstance->bIsStarted = TRUE;
				WaveInstance->bIsFinished = FALSE;
				WaveInstance->bIsRequestingRestart = TRUE;
			}
		}

		// stop any fadeins or fadeouts
		FadeInStartTime = 0.0f;	
		FadeInStopTime = -1.0f;
		FadeInTargetVolume = 1.0f;

		FadeOutStartTime = 0.0f;	
		FadeOutStopTime = -1.0f;
		FadeOutTargetVolume = 1.0f;
	}
	// Increase the cue's current play count if we're starting up.
	else
	{
		for(INT SlotIndex = 0; SlotIndex < SoundSlots.Num(); ++SlotIndex)
		{	
			FMultiCueSplineSoundSlot& Slot = SoundSlots(SlotIndex);
			if(Slot.bPlaying && NULL != Slot.SoundCue && NULL != Slot.SoundCue->FirstNode)
			{
				Slot.SoundCue->CurrentPlayCount++;
			}
		}
	}

	PlaybackTime = 0.0f;
	bFinished = FALSE;
	bWasPlaying = TRUE;
	bApplyRadioFilter = FALSE;
	bRadioFilterSelected = FALSE;
	LastOwner = Owner;

	// Adds component from the audio device's component list.
	UAudioDevice* AudioDevice = GEngine && GEngine->Client ? GEngine->Client->GetAudioDevice() : NULL;
	if( AudioDevice )
	{
		AudioDevice->AddComponent( this );
	}	
}

void UMultiCueSplineAudioComponent::Stop( )
{
	debugfSuppressed( NAME_DevAudioVerbose, TEXT( "%g: Stopping AudioComponent : '%s' with Sound Cue: '%s'" ), GWorld ? GWorld->GetAudioTimeSeconds() : 0.0f, *GetFullName(), SoundCue ? *SoundCue->GetName() : TEXT( "NULL" ) );

	// Decrease the cue's current play count on the first call to Stop.
	for(INT SlotIndex = 0; SlotIndex < SoundSlots.Num(); ++SlotIndex)
	{	
		FMultiCueSplineSoundSlot& Slot = SoundSlots(SlotIndex);
		if(!Slot.bPlaying || NULL == Slot.SoundCue || NULL == Slot.SoundCue->FirstNode) continue;

		Slot.SoundCue->CurrentPlayCount = Max( Slot.SoundCue->CurrentPlayCount - 1, 0 );
		Slot.bPlaying = FALSE;
	}

	// For safety, clear out the cached root sound node pointer.
	CueFirstNode = NULL;

	// We're done.
	bFinished = TRUE;

	UBOOL bOldWasPlaying = bWasPlaying;

	// Clean up intermediate arrays which also dissociates from the audio device, hence stopping the sound.
	// This leaves the component in a state that is suited for having Play called on it again to restart
	// playback from the beginning.
	Cleanup();

	if( bOldWasPlaying && GWorld != NULL && DELEGATE_IS_SET( OnAudioFinished ) )
	{
		INC_DWORD_STAT( STAT_AudioFinishedDelegatesCalled );
		SCOPE_CYCLE_COUNTER( STAT_AudioFinishedDelegates );
		delegateOnAudioFinished( this );
	}

	// Auto destruction is handled via marking object for deletion.
	if( bAutoDestroy )
	{
		// If no owner, or not detached from owner, remove from the last owners component array
		if( LastOwner )
		{
			LastOwner->DetachComponent( this );
			LastOwner = NULL;
		}

		// Mark object for deletion and reference removal.
		MarkPendingKill();
	}
}

void UMultiCueSplineAudioComponent::UpdateWaveInstances( UAudioDevice* AudioDevice, TArray<FWaveInstance*> &InWaveInstances, const TArray<struct FListener>& InListeners, FLOAT DeltaTime )
{
	check( AudioDevice );

	//@todo audio: Need to handle pausing and not getting out of sync by using the mixer's time.
	//@todo audio: Fading in and out is also dependent on the DeltaTime
	PlaybackTime += DeltaTime;
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

	bFinished = TRUE;

	for(CurrentSlotIndex = 0; CurrentSlotIndex < SoundSlots.Num(); ++CurrentSlotIndex)
	{	
		FMultiCueSplineSoundSlot& Slot = SoundSlots(CurrentSlotIndex);
		if(!Slot.bPlaying || NULL == Slot.SoundCue || NULL == Slot.SoundCue->FirstNode) continue;

		SoundCue = Slot.SoundCue;
		CueFirstNode = SoundCue->FirstNode;

		// Reset temporary variables used for node traversal.
		FAudioComponentSavedState::Reset( this );

		OmniRadius=0.0f;
		//RESTORE SLOT DATA
		SourceInteriorVolume = Slot.SourceInteriorVolume;
		SourceInteriorLPF = Slot.SourceInteriorLPF;
		LastUpdateTime = Slot.LastUpdateTime;
		CurrentInteriorLPF = Slot.CurrentInteriorLPF;
		CurrentInteriorVolume = Slot.CurrentInteriorVolume;

		FLOAT SavedCurrentVolumeMultiplier = CurrentVolumeMultiplier;
		FLOAT SavedCurrentHighFrequencyGainMultiplier = CurrentHighFrequencyGainMultiplier;
		FLOAT SavedCurrentVolume = CurrentVolume;
		FLOAT SavedCurrentPitch = CurrentPitch;

		// splitscreen support:
		// we always pass the 'primary' listener (viewport 0) to the sound nodes and the underlying audio system
		// then move the AudioComponent's CurrentLocation so that its position relative to that Listener is the same as its real position is relative to the closest Listener
		Listener = &InListeners( 0 );

		INT ClosestListenerIndex = 0;
		{
			SCOPE_CYCLE_COUNTER( STAT_AudioFindNearestLocation );
			CurrentLocation = FindClosestLocation(InListeners, ClosestListenerIndex);
		}

		const FListener* ClosestListener = &InListeners( ClosestListenerIndex );

		// if the closest listener is not the primary one, transform CurrentLocation
		if( ClosestListener != Listener )
		{
			// get the world space delta between the sound source and the viewer
			FVector Delta = CurrentLocation - ClosestListener->Location;
			// transform Delta to the viewer's local space
			FVector ViewActorLocal = FInverseRotationMatrix( ClosestListener->Front.Rotation() ).TransformFVector( Delta );
			// transform again to a world space delta to the 'real' listener
			FVector ListenerWorld = FRotationMatrix( Listener->Front.Rotation() ).TransformFVector( ViewActorLocal );
			// add 'real' listener's location to get the final position to use
			CurrentLocation = ListenerWorld + Listener->Location;
		}

		// Default values.
		// It's all Multiplicative!  So now people are all modifying the multiplier values via various means 
		// (even after the Sound has started playing, and this line takes them all into account and gives us
		// final value that is correct
		CurrentVolumeMultiplier = VolumeMultiplier * SoundCue->VolumeMultiplier * GetFadeInMultiplier() * GetFadeOutMultiplier() * GetAdjustVolumeOnFlyMultiplier() * AudioDevice->TransientMasterVolume;
		CurrentPitchMultiplier = PitchMultiplier * SoundCue->PitchMultiplier;
		CurrentHighFrequencyGainMultiplier = HighFrequencyGainMultiplier;

		// Set multipliers to allow propagation of sound class properties to wave instances.
		FSoundClassProperties* SoundClassProperties = AudioDevice->GetCurrentSoundClass( SoundCue->SoundClass );
		if( SoundClassProperties )
		{
			// Use values from "parsed/ propagated" sound class properties
			CurrentVolumeMultiplier *= SoundClassProperties->Volume * GGlobalAudioMultiplier;
			CurrentPitchMultiplier *= SoundClassProperties->Pitch;
			//TODO: Add in HighFrequencyGainMultiplier property to sound classes

			// Not all values propagate multiplicatively
			CurrentVoiceCenterChannelVolume = SoundClassProperties->VoiceCenterChannelVolume;
			CurrentRadioFilterVolume = SoundClassProperties->RadioFilterVolume * CurrentVolumeMultiplier * GGlobalAudioMultiplier;
			CurrentRadioFilterVolumeThreshold = SoundClassProperties->RadioFilterVolumeThreshold * CurrentVolumeMultiplier * GGlobalAudioMultiplier;
			StereoBleed = SoundClassProperties->StereoBleed;				// Amount to bleed stereo to the rear speakers (does not propagate to children)
			LFEBleed = SoundClassProperties->LFEBleed;						// Amount to bleed to the LFE speaker (does not propagate to children)

			bEQFilterApplied = SoundClassProperties->bApplyEffects;
			bAlwaysPlay = SoundClassProperties->bAlwaysPlay;
			bIsUISound |= SoundClassProperties->bIsUISound;					// Yes, that's |= because all children of a UI class should be UI sounds
			bIsMusic |= SoundClassProperties->bIsMusic;						// Yes, that's |= because all children of a music class should be music
			bReverb = SoundClassProperties->bReverb;						// A class with reverb applied may have children with no reverb
			bCenterChannelOnly = SoundClassProperties->bCenterChannelOnly;	// A class with bCenterChannelOnly applied may have children without this property
		}

		// Additional inside/outside processing for ambient sounds
		if( SoundCue->SoundClass == NAME_Ambient )
		{
			HandleInteriorVolumes( AudioDevice, WorldInfo, TRUE, GetPointForDistanceEval() );
		}

		// Recurse nodes, have SoundNodeWave's create new wave instances and update bFinished unless we finished fading out. 
		if( FadeOutStopTime == -1 || ( PlaybackTime <= FadeOutStopTime ) )
		{
			CueFirstNode->ParseNodes( AudioDevice, NULL, 0, this, InWaveInstances );
		}

		Slot.SourceInteriorVolume = SourceInteriorVolume;
		Slot.SourceInteriorLPF = SourceInteriorLPF;
		Slot.LastUpdateTime = LastUpdateTime;
		Slot.CurrentInteriorLPF = CurrentInteriorLPF;
		Slot.CurrentInteriorVolume = CurrentInteriorVolume;

		CurrentVolumeMultiplier = SavedCurrentVolumeMultiplier;
		CurrentHighFrequencyGainMultiplier = SavedCurrentHighFrequencyGainMultiplier;
		CurrentPitch = SavedCurrentPitch;
		CurrentVolume = SavedCurrentVolume;
	}

	// Stop playback, handles bAutoDestroy in Stop.
	if( bFinished )
	{
		Stop();
	}
}

FLOAT UMultiCueSplineAudioComponent::GetDuration( )
{
	FLOAT MaxDuration = 0.0f;
	for(INT SlotIndex = 0; SlotIndex < SoundSlots.Num(); ++SlotIndex)
	{	
		FMultiCueSplineSoundSlot& Slot = SoundSlots(SlotIndex);
		if(!Slot.bPlaying || NULL == Slot.SoundCue) continue;
		
		const FLOAT Duration = Slot.SoundCue->GetCueDuration();
		MaxDuration = Max(MaxDuration, Duration);
	}
	return MaxDuration;
}

FVector UMultiCueSplineAudioComponent::FindVirtualSpeakerScaledPosition( const TArray< FInterpCurveVector::FPointOnSpline >& Points, FVector Listener, FLOAT Radius, const FMultiCueSplineSoundSlot& Slot, FLOAT& OutScaledDistance, INT& OutClosestPointOnSplineIndex )
{
	return FindVirtualSpeakerScaledPositionForSlot(Points, Listener, Radius, Slot, OutScaledDistance, OutClosestPointOnSplineIndex );
}

/*-----------------------------------------------------------------------------
	USimpleSplineNonLoopAudioComponent implementation.
-----------------------------------------------------------------------------*/

void USimpleSplineNonLoopAudioComponent::HandleSoundSlot( UAudioDevice* AudioDevice, TArray<FWaveInstance*> &WaveInstances, const TArray<struct FListener>& InListeners, FSplineSoundSlot& Slot, INT ChildIndex)
{
	if(0 > CurrentSlotIndex) Reshuffle();
	if(CurrentSlotIndex != ChildIndex) return;
	
	//FIND LOCATION
	Listener = &InListeners( 0 );
	INT ClosestListenerIndex = 0;
	FLOAT DistanceToAttenuationEval = 0.0f;
	ClosestPointOnSplineIndex=-1;

	CurrentLocation = FindLocationForAmbientSoundSimple(Points, ListenerScopeRadius, InListeners, Slot, ClosestListenerIndex, DistanceToAttenuationEval, ClosestPointOnSplineIndex);
	const FListener* ClosestListener = &InListeners( ClosestListenerIndex );
	// CheckOcclusion( ClosestListener->Location ); - not needed for spline actors?

	// if the closest listener is not the primary one, transform CurrentLocation
	if( ClosestListener != Listener )
	{
		// get the world space delta between the sound source and the viewer
		FVector Delta = CurrentLocation - ClosestListener->Location;
		// transform Delta to the viewer's local space
		FVector ViewActorLocal = FInverseRotationMatrix( ClosestListener->Front.Rotation() ).TransformFVector( Delta );
		// transform again to a world space delta to the 'real' listener
		FVector ListenerWorld = FRotationMatrix( Listener->Front.Rotation() ).TransformFVector( ViewActorLocal );
		// add 'real' listener's location to get the final position to use
		CurrentLocation = ListenerWorld + Listener->Location;
	}

	// LOCATION DEPENDENT
	// Additional inside/outside processing for ambient sounds

	if( SoundCue->SoundClass == NAME_Ambient )
	{
		HandleInteriorVolumes( AudioDevice, GWorld->GetWorldInfo(), TRUE, GetPointForDistanceEval() );
	}

	// Recurse nodes, have SoundNodeWave's create new wave instances and update bFinished unless we finished fading out. 
	USoundNode::CalculateAttenuatedVolume( this, DistanceAlgorithm, DistanceToAttenuationEval, RadiusMin, RadiusMax, dBAttenuationAtMax );
	if(bAttenuateWithLPF)
	{
		USoundNode::CalculateLPFComponent( this, DistanceToAttenuationEval, LPFRadiusMin, LPFRadiusMax );
	}
	CurrentUseSpatialization = TRUE;
	OmniRadius = FlattenAttenuationRadius;

	//PLAY WAVE
	CurrentVolume *= Slot.VolumeScale * UsedVolumeModulation;
	CurrentPitch *= Slot.PitchScale * UsedPitchModulation;

	bFinished = FALSE;

	// If we should currently be playing a sound, parse the current slot
	if( PlaybackTime >= NextSoundTime )
	{
		// If slot has a wave, start playing it
		if( Slot.Wave != NULL )
		{
			Slot.Wave->ParseNodes( AudioDevice, NULL, ChildIndex, this, WaveInstances );
		}
		// If it doesn't - move the NextSoundTime on again and pick a different slot right now.
		else
		{
			Reshuffle();
		}
	}
}	


/**
 * Function randomly picks slot index for USimpleSplineNonLoopAudioComponent
 */
static INT RandomPickNextSlot(TArray<FSplineSoundSlot>& SoundSlots)
{
	// USoundNodeAmbientNonLoop::PickNextSlot
	// Handle case of empty array
	if( SoundSlots.Num() == 0 )
	{
		return 0;
	}

	// First determine to sum total of all weights.
	FLOAT TotalWeight = 0.0f;
	for( INT i = 0; i < SoundSlots.Num(); i++ )
	{
		TotalWeight += SoundSlots( i ).Weight;
	}

	// The pick the weight we want.
	const FLOAT ChosenWeight = appFrand() * TotalWeight;

	// Then count back through accumulating weights until we pass it.
	FLOAT TestWeight = 0.0f;
	for( INT i = 0; i < SoundSlots.Num(); i++ )
	{
		TestWeight += SoundSlots( i ).Weight;
		if( TestWeight >= ChosenWeight )
		{
			return i;
		}
	}

	// Handle edge case - use last slot.
	return SoundSlots.Num() - 1;
}

/**
 * Reassign all randomized fields
 */
void USimpleSplineNonLoopAudioComponent::Reshuffle()
{
	UsedVolumeModulation = VolumeMax + ( ( VolumeMin - VolumeMax ) * appSRand() );
	UsedPitchModulation = PitchMax + ( ( PitchMin - PitchMax ) * appSRand() );
	NextSoundTime = PlaybackTime + DelayMax + ( ( DelayMin - DelayMax ) * appSRand() );
	CurrentSlotIndex = RandomPickNextSlot(SoundSlots);
}

void USimpleSplineNonLoopAudioComponent::Play( )
{
	Super::Play( );
	Reshuffle();
}

void USimpleSplineNonLoopAudioComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty( PropertyChangedEvent );
	Reshuffle();
}

void USplineAudioComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Reset variables.
	if( bWasPlaying )
	{
		for( INT InstanceIndex = 0; InstanceIndex < WaveInstances.Num(); InstanceIndex++ )
		{
			FWaveInstance* WaveInstance = WaveInstances( InstanceIndex );
			if( WaveInstance )
			{
				WaveInstance->bIsStarted = TRUE;
				WaveInstance->bIsFinished = FALSE;
				WaveInstance->bIsRequestingRestart = TRUE;
			}
		}
	}

	PlaybackTime = 0.0f;
	bFinished = FALSE;

	// WORKAROUND FOR BUG INTRODUCED IN CL# 939332	
	/*
		When a property of UAudioComponent is changed SoundNodeData is cleaned in UAudioComponent::PostEditChangeProperty.
		Then AActor::PostEditChangeProperty calls:
		UActorComponent::ConditionalDetach
		UAudioComponent::Detach
		UAudioComponent::Cleanup (FIRST call)
		UAudioDevice::RemoveComponent
		FXAudio2SoundSource::Stop
		FSoundSource::Stop
		FWaveInstance::NotifyFinished
		.. then USoundNode needs SoundNodeData from AudioComponent.

		So it is better to clean SoundNodeData only in UAudioComponent::Cleanup, after UAudioDevice::RemoveComponent is called.

		Before CL# 939332:  UAudioComponent::Cleanup was called FIRST by:
		UActorComponent::PreEditChange
		FComponentReattachContext::FComponentReattachContext
		UAudioComponent::Detach
		.. when SoundNodeData was still intact.
		And the SECOND call from PostEditChangeProperty was harmless.
	*/
	// Clear node offset associations and data so dynamic data gets re-initialized.
	// SoundNodeData.Empty();
	// SoundNodeOffsetMap.Empty();

	UActorComponent::PostEditChangeProperty(PropertyChangedEvent);
}